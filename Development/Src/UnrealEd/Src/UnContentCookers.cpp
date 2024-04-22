
/*=============================================================================
	UnContentCookers.cpp: Various platform specific content cookers.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineAIClasses.h"
#include "EngineMaterialClasses.h"
#include "EngineSoundClasses.h"
#include "UnTerrain.h"
#include "EngineParticleClasses.h"
#include "EngineAnimClasses.h"
#include "EngineInterpolationClasses.h"
#include "AnimationUtils.h"
#include "UnCodecs.h"
#include "UnConsoleSupportContainer.h"
#include "UnNet.h"
#include "UnFracturedStaticMesh.h"
#include "UnAudioDecompress.h"

#include "DebuggingDefines.h"
#include "PerfMem.h"

#if WITH_SUBSTANCE_AIR == 1
#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirTextureClasses.h"
#endif // WITH_SUBSTANCE_AIR == 1

//@TODO: Put this code in a platform-independent place
#include "GlobalShaderNGP.h"

#include "../../GFxUI/Inc/GFxUIClasses.h"

static INT BrushPhysDataBytes = 0;

/** delay in seconds for the spinning involved in synchronizing child jobs */
static const FLOAT MTSpinDelay = 0.1f; 
/** time in seconds before we decide someone has crashed or a file is permanently locked */
static FLOAT MTTimeout = 60.0f; 

//@script patcher
static UBOOL bAbortCooking=FALSE;

UPersistentCookerData* GShippingCookerData = NULL;

static FCookStats CookStats;

#if STATS
/** Macro for updating a seconds counter without creating new scope. */
	#define SCOPE_SECONDS_COUNTER_COOK(Seconds) FScopeSecondsCounter SecondsCount_##Seconds(CookStats.Seconds);
#else
	#define SCOPE_SECONDS_COUNTER_COOK(Seconds)
#endif


extern INT Localization_GetLanguageExtensionIndex(const TCHAR* Ext);

const FName BaseTFCName(TEXT("Textures"));
const FName CharacterTFCName(TEXT("CharTextures"));
const FName LightingTFCName(TEXT("Lighting"));
const FName TFCReportFileName(TEXT("TFC_Generation_Report.txt"));

using namespace UE3;

IMPLEMENT_COMPARE_CONSTREF( FString, UnContentCookersStringSort, { return appStricmp(*A,*B); } )
IMPLEMENT_COMPARE_CONSTREF( QWORD, UnContentCookers, { if (A < B) return -1; if (A > B) return 1; return 0; })
IMPLEMENT_COMPARE_CONSTREF( FProgramKey, UnContentCookers, { return A.Compare(B); })

extern void GatherMaterialKeyData(FProgramKeyData& MaterialKeyData, const UMaterialInterface* Material, const UWorld* InWorld);
extern void GetNonNativeStartupPackageNames(TArray<FString>& PackageNames, const TCHAR* EngineConfigFilename=NULL, UBOOL bIsCreatingHashes=FALSE);

/**
 * A utility function to log out warning messages uniquely to avoid spam
 */
static TLookupMap<FString> UniqueWarningMessages;
void LogWarningOnce(const FString& WarningMessage)
{
	INT CurrentCount = UniqueWarningMessages.Num();
	UniqueWarningMessages.AddItem( WarningMessage );
	if( UniqueWarningMessages.Num() != CurrentCount )
	{
		warnf(NAME_Warning, *WarningMessage );
	}
}

/**
 * Abstract mobile shader cooker interface.
 */
class FConsoleMobileShaderCooker
{
public:
	/**
	 * Constructor
 */
	FConsoleMobileShaderCooker()
	:	CurrentShaderGroup(NULL)
	{
		bSupported = FALSE;

		Platform = PLATFORM_IPhone;
	}

	/**
	 * Virtual destructor
	 */
	virtual ~FConsoleMobileShaderCooker()
	{
	}

	/** Perform any closing duties of the shader cooker */
	void Close (void)
	{
		if (bDebugFullMaterialReport)
		{
			DecrementXMLIndentLevel();
			WriteDebugText(GetXMLIndentLevelString() + TEXT("</ROOT>"));
		}
	}

	/**
	 * Enables based on platform
	 */
	void Init( UE3::EPlatformType InPlatform, const TCHAR *InCookedDirectory, UBOOL bInShouldByteSwap )
	{
		//Instrumentation of mobile variables via commandline
		bDebugMaterialToKey = ParseParam(appCmdLine(),TEXT("DebugMaterialToKey"));
		bDebugKeyToMaterial = ParseParam(appCmdLine(),TEXT("DebugKeyToMaterial"));
		bDebugFullMaterialReport = ParseParam(appCmdLine(), TEXT("DebugFullMaterialReport"));
		DebugXMLIndentLevel =0;

		bDebugUseMaterialLog = FALSE;
		FString LogName;
		if (Parse(appCmdLine(),TEXT("DebugMaterialLog="),LogName))
		{
			DebugMaterialLogName = FFilename(InCookedDirectory) + TEXT("DebugLogs\\") + LogName;
			bDebugUseMaterialLog = TRUE;
			warnf(NAME_Log, TEXT("Writing material debug info to %s\\%s"), *(DebugMaterialLogName.GetPath()), *(DebugMaterialLogName.GetCleanFilename()));
		}
		else if (bDebugFullMaterialReport)
		{
			// if full report, write to a log even if not requested
			if (!bDebugUseMaterialLog)
			{
				bDebugUseMaterialLog = TRUE;
				DebugMaterialLogName = FFilename(InCookedDirectory) + TEXT("DebugLogs\\FullMaterialData.xml");
				warnf(NAME_Log, TEXT("Writing material debug info to %s\\%s"), *(DebugMaterialLogName.GetPath()), *(DebugMaterialLogName.GetCleanFilename()));
			}
		}

		// write xml header to FullReport if needed, set default settings for a full report
		if (bDebugFullMaterialReport)
		{
			bDebugKeyToMaterial = TRUE;
			bDebugMaterialToKey = TRUE;

			FString XMLHeader = TEXT("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<ROOT>\n");
			IncrementXMLIndentLevel();
			appSaveStringToFile(XMLHeader, *DebugMaterialLogName);
		}

		// get threshold for displaying materials-per-key (only show keys with this number of materials or fewer)
		DebugMaterialKeyMaxCount = -1;
		Parse(appCmdLine(), TEXT("DebugMaterialKeyMaxCount="), DebugMaterialKeyMaxCount);
		
		bListSkinningIteratedMaterials = ParseParam(appCmdLine(),TEXT("ListSkinningIteratedMaterials"));
		bListLightmapIteratedMaterials = ParseParam(appCmdLine(),TEXT("ListLightmapIteratedMaterials"));

		//pull material lists from ini file
		GConfig->GetArray(TEXT("MobileMaterialCookSettings"), TEXT("SkinningOnlyMaterials"), SkinningOnlyMaterials, GEngineIni);
		GConfig->GetArray(TEXT("MobileMaterialCookSettings"), TEXT("NoLightmapOnlyMaterials"), NoLightmapOnlyMaterials, GEngineIni);

		// Read shader group settings from the ini file
		GConfig->Parse1ToNSectionOfNames(TEXT("Engine.MobileShaderGroups"), TEXT("ShaderGroup"), TEXT("Package"), ShaderGroupPackages, GEngineIni);


		for ( TMap<FName, TArray<FName> >::TConstIterator It(ShaderGroupPackages); It; ++It )
		{
			FName ShaderGroupName = It.Key();
			warnf(NAME_Log, TEXT("ShaderGroup - %s"), *ShaderGroupName.GetNameString());
		}


		Platform = InPlatform;
		bShouldByteSwap = bInShouldByteSwap;
		if ((Platform == PLATFORM_IPhone) || (Platform == PLATFORM_Android) || (Platform == PLATFORM_NGP) || (Platform == PLATFORM_Flash))
		{
			CookedDirectory = FFilename(InCookedDirectory);
			ShadersDirectory = CookedDirectory + TEXT("Shaders\\");
			bSupported = TRUE;
			Load();
			// Create folder for cooked data.
			if( !GFileManager->MakeDirectory( *ShadersDirectory, TRUE ) )
			{
				appErrorf(TEXT("Couldn't create %s"), *ShadersDirectory);
			}
		}
	}

	//During particle cooking, they will send their material to the mobile shader cooker for future processing
	void SetParticleMaterialUsage(const UMaterialInterface* InParticleMaterial, const BYTE InInterpolationMethod, 
		const BYTE InSpriteScreenAlignment, const UBOOL bInBeamTrailParticle, const UBOOL bInMeshParticle )
	{
		if (bSupported && InParticleMaterial)
		{
			FParticleMaterialDesc NewDesc;
			InParticleMaterial->GetName(NewDesc.MaterialName);
			NewDesc.SpriteScreenAlignment = InSpriteScreenAlignment;
			NewDesc.InterpolationMethod = InInterpolationMethod;
			NewDesc.bMeshParticle = bInMeshParticle;
			NewDesc.bBeamTrailParticle = bInBeamTrailParticle;

			ParticleMaterialDescs.AddUniqueItem(NewDesc);
		}
	}


	/** Helper function to find out if this particle material is used with */
	void FindParticleUsage(const UMaterialInterface* InMaterial, UBOOL& bUsedAsParticle, UBOOL& bUsedAsParticleSubUV, UBOOL& bUsedAsBeamTrailParticle, UBOOL& bMeshParticle, 
		DWORD& OutSpriteScreenAlignment, DWORD& OutScreenAlignmentMask)
	{
		check(InMaterial);

		bUsedAsParticle = FALSE;
		bUsedAsParticleSubUV = FALSE;
		bUsedAsBeamTrailParticle = FALSE;
		bMeshParticle = FALSE;
		OutSpriteScreenAlignment = 0;
		OutScreenAlignmentMask = 0;
		UBOOL bFound = FALSE;

		for (INT i = 0; i < ParticleMaterialDescs.Num(); ++i)
		{
			if (ParticleMaterialDescs(i).MaterialName == InMaterial->GetName())
			{
				if (ParticleMaterialDescs(i).bMeshParticle)
				{
					bMeshParticle = TRUE;
				}
				else
				{
					bUsedAsBeamTrailParticle = ParticleMaterialDescs(i).bBeamTrailParticle;

					bUsedAsParticle      = bUsedAsParticle      || (ParticleMaterialDescs(i).InterpolationMethod == 0);
					bUsedAsParticleSubUV = bUsedAsParticleSubUV || (ParticleMaterialDescs(i).InterpolationMethod != 0);

					if (bFound && (OutSpriteScreenAlignment != ParticleMaterialDescs(i).SpriteScreenAlignment))
					{
						//set to max so we can iterate over all types
						OutSpriteScreenAlignment = SSA_Max;
					}
					else
					{
						OutSpriteScreenAlignment = ParticleMaterialDescs(i).SpriteScreenAlignment;
					}
					//set a bit incase we have to iterate over all particles, just do the ones we need (not generic at all)
					OutScreenAlignmentMask |= (1<<ParticleMaterialDescs(i).SpriteScreenAlignment);
				}
				
				bFound = TRUE;
			}
		}
	}
	/** Helper function to find out if this material was associated with lightmaps */
	void FindComponentLightmapUsage(const UMaterial* InMaterial, TArray<UActorComponent*>& InComponents, UBOOL& bOutUsedWithoutLightMap, UBOOL& bOutUsedWithLightMap)
	{
		//Get all primitive components
		for(INT ComponentIndex = 0;ComponentIndex < InComponents.Num();ComponentIndex++)
		{
			const UPrimitiveComponent* PrimComp = ConstCast<UPrimitiveComponent>(InComponents(ComponentIndex));
			if( PrimComp )
			{
				TArray <UMaterialInterface*> UsedMaterials;
				PrimComp->GetUsedMaterials(UsedMaterials);
				for (INT MaterialIndex = 0; MaterialIndex < UsedMaterials.Num(); ++MaterialIndex)
				{
					if (UsedMaterials(MaterialIndex)->GetName() == InMaterial->GetName())
					{
						UBOOL bUsesTextureLightmap = PrimComp->bUsePrecomputedShadows;

						const UStaticMeshComponent* StaticMeshComp = ConstCast<UStaticMeshComponent>(PrimComp);
						if (StaticMeshComp && StaticMeshComp->HasStaticShadowing())
						{
							INT LightMapWidth = 0;
							INT	LightMapHeight = 0;
							StaticMeshComp->GetLightMapResolution(LightMapWidth, LightMapHeight);

							// Determine whether we are using a texture or vertex buffer to store precomputed data.
							bUsesTextureLightmap = bUsesTextureLightmap && StaticMeshComp->UsesTextureLightmaps(LightMapWidth, LightMapHeight);
						}

						//FOUND IT!
						bOutUsedWithLightMap    = bOutUsedWithLightMap    || bUsesTextureLightmap;
						bOutUsedWithoutLightMap = bOutUsedWithoutLightMap || (!bUsesTextureLightmap);
					}
				}
			}	//end if PrimComp
		}	//end for each component
	}

	/** Helper function to find out if this material was associated with lightmaps */
	void FindLightmapUsage(const UMaterial* InMaterial, const UWorld* InWorld, UBOOL& bOutUsedWithoutLightMap, UBOOL& bOutUsedWithLightMap)
	{
		check(InMaterial);

		bOutUsedWithoutLightMap = FALSE;
		bOutUsedWithLightMap = FALSE;
		if (InWorld)
		{
			for( INT ActorIndex=0; ActorIndex<InWorld->PersistentLevel->Actors.Num(); ActorIndex++ )
			{
				AActor* Actor = InWorld->PersistentLevel->Actors(ActorIndex);
				if( Actor )
				{
					AStaticMeshCollectionActor * StaticMeshCollectionActor = Cast<AStaticMeshCollectionActor>(Actor);
					if (StaticMeshCollectionActor)
					{
						TArray<UActorComponent*> TempPrimitiveComponents;
						for (INT StaticMeshCompIndex = 0; StaticMeshCompIndex < StaticMeshCollectionActor->StaticMeshComponents.Num(); ++StaticMeshCompIndex)
						{
							TempPrimitiveComponents.AddItem(StaticMeshCollectionActor->StaticMeshComponents(StaticMeshCompIndex));
						}
						FindComponentLightmapUsage(InMaterial, TempPrimitiveComponents, bOutUsedWithoutLightMap, bOutUsedWithLightMap);
					}
					FindComponentLightmapUsage(InMaterial, Actor->Components, bOutUsedWithoutLightMap, bOutUsedWithLightMap);
				}
			}	//end for each actor
		}
	}

	/**
	 * Cooks the source data for the platform and stores the cooked data internally.
	 *
	 * @param	InMaterialInterface		Material to cook
	 * @param	InWorld					World requesting the cook
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	void AddMobileShaderKey( const UMaterial* InBaseMaterial, const UMaterialInterface* InMaterial, const UWorld* InWorld, const EPreferredLightmapType InMobileLightmapType)
	{
		check( InMaterial != NULL );
		check( InBaseMaterial != NULL );

		UBOOL bNewMaterial = TRUE;

		// clear out per-materials debug key list (added to in AddUniqueKey)
		CurrentMaterialRequestedShaderKeys.Empty();

		//NEEDED - better way to reject useless materials.  Temp for demo
		if (InMaterial->GetOutermost()->GetName() == TEXT("EngineMaterials"))
		{
			return;
		}

		//if we're processing mobile materials and this isn't a special engine material
		if (bSupported) // && !InMaterial->bUsedAsSpecialEngineMaterial)
		{
			//for future stat gathering
			if (bDebugKeyToMaterial)
			{
				MobileMaterialName = InMaterial->GetName();
				// add to our unique list of materials
				if (AllMaterialsFound.FindItemIndex(MobileMaterialName) >= 0)
				{
					bNewMaterial = FALSE;
				}
				else 
				{
					AllMaterialsFound.AddItem(MobileMaterialName);			
				}
			}

			TArray <INT> FullyIteratedFields;
			TArray <EMobilePrimitiveType> PrimitiveTypes;
			FProgramKeyData KeyData;

			//extract some flags early
			UBOOL bIsDecal = InBaseMaterial->GetUsageByFlag(MATUSAGE_Decals);
			UBOOL bParticle = !bIsDecal && InBaseMaterial->GetUsageByFlag(MATUSAGE_ParticleSprites);
			UBOOL bParticleSubUV = !bIsDecal && InBaseMaterial->GetUsageByFlag(MATUSAGE_ParticleSubUV);
			UBOOL bParticleBeamTrail = !bIsDecal && InBaseMaterial->GetUsageByFlag(MATUSAGE_BeamTrails);
			UBOOL bSkeletalMesh = !bIsDecal && InBaseMaterial->GetUsageByFlag(MATUSAGE_SkeletalMesh);
			UBOOL bLensFlare = !bIsDecal && InBaseMaterial->GetUsageByFlag(MATUSAGE_LensFlare);
			UBOOL bLandscape = !bIsDecal && (InBaseMaterial->GetUsageByFlag(MATUSAGE_Landscape) || InBaseMaterial->GetUsageByFlag(MATUSAGE_MobileLandscape));

			/** If particles, cache off some data*/
			UBOOL bUsedAsParticle = FALSE;
			UBOOL bUsedAsParticleSubUV = FALSE;
			UBOOL bUsedAsBeamTrailParticle = FALSE;
			UBOOL bUsedAsMeshParticle = FALSE;
			DWORD ScreenAlignment = 0;
			DWORD ScreenAlignmentMask = 0;

			FindParticleUsage(InMaterial, bUsedAsParticle, bUsedAsParticleSubUV, bUsedAsBeamTrailParticle, bUsedAsMeshParticle, ScreenAlignment, ScreenAlignmentMask);

			//Seed this key data with the appropriate KNOWN keys
			KeyData.Start();
			GatherMaterialKeyData(KeyData, InMaterial, InWorld);

			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_GlobalShaderType, EGST_None);
            KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_GFxBlendMode, 0);

			FullyIteratedFields.AddItem(FProgramKeyData::PKDT_IsDepthOnlyRendering);
			FullyIteratedFields.AddItem(FProgramKeyData::PKDT_DepthShaderType);
			FullyIteratedFields.AddItem(FProgramKeyData::PKDT_ForwardShadowProjectionShaderType);

			UBOOL bAllowMobileHeightFog = FALSE;
			GConfig->GetBool(TEXT("SystemSettings"), TEXT("MobileHeightFog"), bAllowMobileHeightFog, GSystemSettingsIni);
			UBOOL bMobileMinimizeFogShaders = FALSE;
			GConfig->GetBool(TEXT("SystemSettings"), TEXT("MobileMinimizeFogShaders"), bMobileMinimizeFogShaders, GSystemSettingsIni);
			if (bMobileMinimizeFogShaders && !bAllowMobileHeightFog)
			{
				UBOOL bUseGradientFog = FALSE;
				GConfig->GetBool(TEXT("SystemSettings"), TEXT("MobileFog"), bUseGradientFog, GSystemSettingsIni);
				bUseGradientFog = bUseGradientFog && InMaterial->bMobileAllowFog;
				KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsGradientFogEnabled, bUseGradientFog);
				KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsHeightFogEnabled, FALSE);
			}
			else
			{
				//FOG - if the world doesn't exist (a script referenced asset) or fog is both enabled and the material uses it
				UBOOL bFogRequested = !InWorld || (InWorld->GetWorldInfo()->bFogEnabled && (InMaterial->bMobileAllowFog));
				if (bFogRequested)
				{
					FProgramKeyData::EProgramKeyDataTypes IteratedFogType = FProgramKeyData::PKDT_IsHeightFogEnabled;
					//find out which is supported by the target platform
					if (Platform == PLATFORM_Flash)
					{
						KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsGradientFogEnabled, FALSE);
					}
					else
					{

						if (bAllowMobileHeightFog)
						{
							KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsGradientFogEnabled, FALSE);
						}
						else
						{
							IteratedFogType = FProgramKeyData::PKDT_IsGradientFogEnabled;
							KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsHeightFogEnabled, FALSE);
						}
					}

					FullyIteratedFields.AddItem(IteratedFogType);
				}
				else
				{
					//always assume fog is off
					KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsGradientFogEnabled, FALSE);
					KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsHeightFogEnabled, FALSE);
				}
			}
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_TwoSided, FALSE);

			// Mobile color grading
			UBOOL bAllowMobileColorGrading;

			GConfig->GetBool(TEXT("SystemSettings"), TEXT("MobileColorGrading"), bAllowMobileColorGrading, GSystemSettingsIni);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsMobileColorGradingEnabled, bAllowMobileColorGrading);

			//PARTICLES
			//if we use multiple alignments, just generate them all
			if (ScreenAlignment == SSA_Max)
			{
				FullyIteratedFields.AddItem(FProgramKeyData::PKDT_ParticleScreenAlignment);
			}
			else
			{
				KeyData.AssignProgramKeyValue( FProgramKeyData::PKDT_ParticleScreenAlignment, ScreenAlignment );
			}

			//Bump offset is dynamically turned on and off if disabled
			#if !MINIMIZE_ES2_SHADERS
				if (InMaterial->bUseMobileBumpOffset)
				{
					KeyData.ResetProgramKeyValue(FProgramKeyData::PKDT_IsBumpOffsetEnabled);
					FullyIteratedFields.AddItem(FProgramKeyData::PKDT_IsBumpOffsetEnabled);
				}
			#endif

			//VERTEX FACTORY FLAGS
			if (bUsedAsParticle && bUsedAsParticleSubUV)
			{
				FullyIteratedFields.AddItem(FProgramKeyData::PKDT_IsSubUV);
			}
			else
			{
				KeyData.AssignProgramKeyValue( FProgramKeyData::PKDT_IsSubUV, bUsedAsParticleSubUV );
			}

			//mesh particles
			if (bUsedAsMeshParticle)
			{
				KeyData.ResetProgramKeyValue(FProgramKeyData::PKDT_UseUniformColorMultiply);
				FullyIteratedFields.AddItem(FProgramKeyData::PKDT_UseUniformColorMultiply);
			}

			//Skeletal mesh
			if( bSkeletalMesh )
			{
				//if we requested the fully iterated skinned materials, add the material to the list
				if (bListSkinningIteratedMaterials)
				{
					SkinningIteratedMaterials.AddUniqueItem(InMaterial->GetName());
				}

				//if this is white-listed as to allow ONLY skinning
				if (SkinningOnlyMaterials.ContainsItem(InMaterial->GetName()))
				{
					KeyData.AssignProgramKeyValue( FProgramKeyData::PKDT_IsSkinned, TRUE );
				}
				else
				{
					FullyIteratedFields.AddItem( FProgramKeyData::PKDT_IsSkinned );
				}
			}
			else
			{
				KeyData.AssignProgramKeyValue( FProgramKeyData::PKDT_IsSkinned, FALSE );
			}

			// Decals
			KeyData.AssignProgramKeyValue( FProgramKeyData::PKDT_IsDecal, bIsDecal );

			// Landscape
			if( bLandscape )
			{
				FullyIteratedFields.AddItem( FProgramKeyData::PKDT_IsLandscape );
			}
			else
			{
				KeyData.AssignProgramKeyValue( FProgramKeyData::PKDT_IsLandscape, FALSE );
			}
			
			//Used lightmaps
			UBOOL bUsedWithLightMap = FALSE;
			UBOOL bUsedWithoutLightMap = FALSE;
			if (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsLightingEnabled) != 0)
			{
				//If this material should NEVER be used with lightmaps, then just ignore the generation of the lightmap shader
				if (!InWorld || NoLightmapOnlyMaterials.ContainsItem(InMaterial->GetName()) || SkinningOnlyMaterials.ContainsItem(InMaterial->GetName()))
				{
					KeyData.AssignProgramKeyValue( FProgramKeyData::PKDT_IsLightmap, FALSE );
					KeyData.AssignProgramKeyValue( FProgramKeyData::PKDT_IsDirectionalLightmap, FALSE );
				}
				else
				{
					//if list lightmap iterated materials is requested, make a note of the material name
					if (bListLightmapIteratedMaterials)
					{
						LightmapIteratedMaterials.AddUniqueItem(InMaterial->GetName());
					}
					FullyIteratedFields.AddItem(FProgramKeyData::PKDT_IsLightmap);
					KeyData.AssignProgramKeyValue( FProgramKeyData::PKDT_IsDirectionalLightmap, (InMobileLightmapType == EPLT_Directional) );
				}
			}
			else
			{
				KeyData.AssignProgramKeyValue( FProgramKeyData::PKDT_IsLightmap, FALSE);
				KeyData.AssignProgramKeyValue( FProgramKeyData::PKDT_IsDirectionalLightmap, FALSE);
			}

			//rim lighting is ONLY controlled through strength.  Make sure allow it to toggle
			if (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsRimLightingEnabled) != 0)
			{
				KeyData.ResetProgramKeyValue(FProgramKeyData::PKDT_IsRimLightingEnabled);
				FullyIteratedFields.AddItem(FProgramKeyData::PKDT_IsRimLightingEnabled);
			}

			//PICK THE PRIMITIVE TYPES
			if ( bUsedAsParticle || bUsedAsParticleSubUV )
			{
				PrimitiveTypes.AddItem(EPT_Particle);
			}
			if (bUsedAsBeamTrailParticle)
			{
				PrimitiveTypes.AddItem(EPT_BeamTrailParticle);
			}
			if (bLensFlare)
			{
				PrimitiveTypes.AddItem(EPT_LensFlare);
			}
			if (bIsDecal || bSkeletalMesh || bUsedWithLightMap || bUsedAsMeshParticle)
			{
				PrimitiveTypes.AddItem(EPT_Default);
			}
			//GAMMA IS CURRENTLY NOT USED

			//if we're not a particle material or lens flare (default particle, etc)
			if (!(bUsedAsParticle || bUsedAsParticleSubUV || bLensFlare))
			{
				//if we don't know any more information, and it's not a special type
				if ((PrimitiveTypes.Num() == 0))
				{
					PrimitiveTypes.AddItem(EPT_Default);
					PrimitiveTypes.AddItem(EPT_Simple);
				}
			}
			
			//if the logic changes, we'll need to just append the single shader since this key has no "iterating fields"
			if (PrimitiveTypes.Num() > 0)
			{
				UBOOL bUseShaderGroupForStartup = TRUE;
				GConfig->GetBool( TEXT("SystemSettings"), TEXT("MobileUseShaderGroupForStartupObjects"), bUseShaderGroupForStartup, GSystemSettingsIni );

				// Is this material used by the startup packages?
				bUsedByStartupPackages = bUseShaderGroupForStartup && InMaterial->HasAnyFlags(RF_CookedStartupObject);

				//Sends down BY COPY the current key data, the fields we need to iterate on, and the starting index
				GenerateAllKeys(PrimitiveTypes, KeyData, FullyIteratedFields, 0, ScreenAlignmentMask);

				// Restore to default
				bUsedByStartupPackages = TRUE;
			}
			if (bDebugMaterialToKey && bNewMaterial)
			{
				FString OutputString = GetMaterialXMLOpenText(*(InMaterial->GetName()));
				WriteDebugText(OutputString);
				WriteDebugText(GetXMLIndentLevelString() + TEXT("<KEYS>\n"));
				IncrementXMLIndentLevel();
				for (INT CurKeyIndex = 0; CurKeyIndex < CurrentMaterialRequestedShaderKeys.Num(); ++CurKeyIndex)
				{
					const FProgramKey &CurKey = CurrentMaterialRequestedShaderKeys(CurKeyIndex);
					// Output the key to screen/log
					FString NewKeyString = GetProgramKeyXMLOpenText(CurKey);
					NewKeyString += GetProgramKeyText(CurKey);
					NewKeyString += GetProgramKeyXMLCloseText();
					WriteDebugText(NewKeyString);
				}
				DecrementXMLIndentLevel();
				WriteDebugText(GetXMLIndentLevelString() + TEXT("</KEYS>\n"));
				WriteDebugText(GetMaterialXMLCloseText());
			}
		}
	}

	/**
	 * Helper function to add unique keys to the requested list
	 * @param Key - The new key to try and add
	 * @return	UBOOL		TRUE if the item was actually added to the array
	 */
	UBOOL AddUniqueKey(FProgramKey Key)
	{
		UBOOL ActuallyAdded = FALSE;
		INT OldSize = AllRequestedShaderKeys.Num();
		INT NewIndex = AllRequestedShaderKeys.AddUniqueItem( Key );
		if (bDebugMaterialToKey)
		{
			// add to our per-material list as well, used to report uniquely added keys for each material
			CurrentMaterialRequestedShaderKeys.AddUniqueItem(Key);
		}
		if (AllRequestedShaderKeys.Num() != OldSize)
		{
			ActuallyAdded = TRUE;
		}

		if (bDebugKeyToMaterial)
		{
			//add this material to the "contributors" of this key
			if (MaterialsPerKey.Num() != AllRequestedShaderKeys.Num())
			{
				TArray<FString> MaterialNames;
				MaterialsPerKey.AddItem(MaterialNames);
			}
			MaterialsPerKey(NewIndex).AddUniqueItem(MobileMaterialName);
		}

		if ( bUsedByStartupPackages )
		{
			StartupShaderGroup.AddUniqueItem( Key );
		}
		// If there's a current shader group set, add it to that group
		//@TODO: Perhaps also check IsIn(CurrentPackage) || HasAnyFlag(RF_ForceTagExp)
		else if ( CurrentShaderGroup )
		{
			CurrentShaderGroup->AddUniqueItem( Key );
		}

		return ActuallyAdded;
	}

	/**
	 * Generates the shader key for all EMobilePlatformFeatures and adds them to AllRequestedShaderKeys.
	 *
	 * @param InPrimitiveTypes - Program prefixes that should be included in this compile
	 * @param InKeyData - The data we've accumulated so far
	 */
	void GenerateKey( EMobilePrimitiveType PrimitiveType, const FProgramKeyData& InKeyData )
	{
		for (INT PlatformFeatures = 0; PlatformFeatures < EPF_MAX; ++PlatformFeatures)
		{
			FProgramKeyData FinalKeyData = InKeyData;
			FinalKeyData.AssignProgramKeyValue( FProgramKeyData::PKDT_PrimitiveType, PrimitiveType );
			FinalKeyData.AssignProgramKeyValue( FProgramKeyData::PKDT_PlatformFeatures, PlatformFeatures );
			FinalKeyData.Stop();

			FProgramKey NewKey;
			FinalKeyData.GetPackedProgramKey( NewKey );

			//add this to the global list
			AddUniqueKey(NewKey);
		}
	}

	/**
	 * Recursively adds all appropriate keys 
	 * @param InPrimitiveTypes - Program prefixes that should be included in this compile
	 * @param InKeyData- PASSED BY COPY, the data we've accumulated so far
	 * @param FullyIteratedFields - The fields to iterate over all values
	 * @param InFieldIndex - The current index into FullyIteratedFields that we're going to start at
	 */
	void GenerateAllKeys(TArray <EMobilePrimitiveType>& InPrimitiveTypes, FProgramKeyData InKeyData, TArray <INT>& FullyIteratedFields, const INT FieldIndex, const DWORD ScreenAlignmentMask)
	{
		//ensure we're still in bounds
		check(FieldIndex <= FullyIteratedFields.Num());

		//recurse or done?
		if (FieldIndex == FullyIteratedFields.Num())
		{
			for (INT PrimitiveIndex = 0; PrimitiveIndex < InPrimitiveTypes.Num(); ++PrimitiveIndex)
			{
				EMobilePrimitiveType PrimitiveType = InPrimitiveTypes(PrimitiveIndex);

// 				if ( InKeyData.GetFieldValue(FProgramKeyData::PKDT_IsDepthOnlyRendering) != 0 )
// 				{
// 					// Generate keys for each depth shader type.
// 					for ( INT DepthShaderType=0; DepthShaderType < MobileDepthShader_MAX; ++DepthShaderType )
// 					{
// 						if (DepthShaderType != MobileDepthShader_None)
// 						{
// 							InKeyData.OverrideProgramKeyValue( FProgramKeyData::PKDT_DepthShaderType, DepthShaderType );
// 							GenerateKey( PrimitiveType, InKeyData );
// 						}
// 					}
// 				}
// 				else
// 				{
// 					InKeyData.OverrideProgramKeyValue( FProgramKeyData::PKDT_DepthShaderType, MobileDepthShader_None );
// 					GenerateKey( PrimitiveType, InKeyData );
// 				}

				GenerateKey( PrimitiveType, InKeyData );
			}
		}
		else
		{
			INT FieldEnum = FullyIteratedFields(FieldIndex);
			INT MaxValue = FProgramKeyData::GetFieldMaxValue(FieldEnum);
			for (INT IterValue = 0; IterValue < MaxValue; IterValue++)
			{
				//unused alignment, but more than one specified
				if ((FieldEnum == FProgramKeyData::PKDT_ParticleScreenAlignment) && ((ScreenAlignmentMask & (1<<IterValue)) == 0))
				{
					continue;
				}
				//make a copy of the current state of things
				FProgramKeyData CopyKeyData = InKeyData;
				CopyKeyData.AssignProgramKeyValue(FieldEnum, IterValue);
				GenerateAllKeys(InPrimitiveTypes, CopyKeyData, FullyIteratedFields, FieldIndex + 1, ScreenAlignmentMask);
			}
		}
	}

	/**
	 * Calculates a program key for a global shader type.
	 */
	FProgramKey CalcGlobalShaderKey( EMobileGlobalShaderType GlobalShaderType, EBlendMode BlendMode )
	{
		FProgramKeyData KeyData;
		KeyData.Start();
		KeyData.ClearProgramKeyData();
		KeyData.OverrideProgramKeyValue( FProgramKeyData::PKDT_PrimitiveType, EPT_GlobalShader );
		KeyData.OverrideProgramKeyValue( FProgramKeyData::PKDT_GlobalShaderType, GlobalShaderType );
		KeyData.OverrideProgramKeyValue( FProgramKeyData::PKDT_BlendMode, BlendMode );
		KeyData.Stop();
		FProgramKey Key;
		KeyData.GetPackedProgramKey( Key );
		return Key;
	}

	/** Add fallback shaders in case one is missing, except for global shaders. */
	void AddFallbackShaders ()
	{
		for (INT PrimitiveType = 0; PrimitiveType < EPT_MAX; ++PrimitiveType)
		{
			if ( PrimitiveType != EPT_GlobalShader )
			{
				//for instrumentation
				MobileMaterialName = FString::Printf(TEXT("FallbackShader%d"), PrimitiveType);
				if (bDebugMaterialToKey)
				{
					FString OutputString = GetMaterialXMLOpenText(*MobileMaterialName);
					WriteDebugText(OutputString);
					WriteDebugText(GetXMLIndentLevelString() + TEXT("<KEYS>\n"));
					IncrementXMLIndentLevel();
				}

				//opaque (and translucent for simples
				for (INT BlendIndex = 0; BlendIndex < BLEND_Modulate; ++BlendIndex)
				{
					FProgramKeyData KeyData;	//reset the key data to be filled in again
					KeyData.Start();
					KeyData.ClearProgramKeyData();
					KeyData.OverrideProgramKeyValue(FProgramKeyData::PKDT_PrimitiveType, PrimitiveType);
					if ((PrimitiveType == EPT_Simple) || (PrimitiveType == EPT_DistanceFieldFont))
					{
						KeyData.OverrideProgramKeyValue(FProgramKeyData::PKDT_BlendMode, BlendIndex);
					}
					KeyData.Stop();

					FProgramKey NewKey;
					KeyData.GetPackedProgramKey( NewKey );
					//make sure to always have a fallback of 0
					AddUniqueKey(NewKey);

					if (bDebugMaterialToKey)
					{
						FString NewKeyString = GetProgramKeyXMLOpenText(NewKey);
						NewKeyString += GetProgramKeyText(NewKey);
						NewKeyString += GetProgramKeyXMLCloseText();
						WriteDebugText(NewKeyString);
					}

				}
				if (bDebugMaterialToKey)
				{
					DecrementXMLIndentLevel();
					WriteDebugText(GetXMLIndentLevelString() + TEXT("</KEYS>\n"));
					WriteDebugText(GetMaterialXMLCloseText());
				}

			}
		}
	}

	/** Adds all global shaders */
	void AddGlobalShaders()
	{
		// these are only needed for mobile
		if (!(Platform & PLATFORM_Mobile))
		{
			return;
		}

		//Do NOT preload GFX shaders!  There are lots of them!
		INT MaxGlobalShader = EGST_MAX;
#if WITH_GFx
		if (!(Platform & PLATFORM_NGP))
		{
			MaxGlobalShader = EGST_GFxBegin;
		}
#endif

		// Generate keys for the global shaders and add them.
		for ( INT GlobalShaderIndex=1; GlobalShaderIndex < MaxGlobalShader; ++GlobalShaderIndex )
		{
            if ( MobileGlobalShaderExists( (EMobileGlobalShaderType)GlobalShaderIndex ))
            {
				MobileMaterialName = FString::Printf(TEXT("GlobalShader%d"), GlobalShaderIndex);
				if (bDebugMaterialToKey)
				{
					WriteDebugText(GetMaterialXMLOpenText(*MobileMaterialName));
					WriteDebugText(GetXMLIndentLevelString() + TEXT("<KEYS>\n"));
				}
    			FProgramKey Key = CalcGlobalShaderKey((EMobileGlobalShaderType)GlobalShaderIndex, BLEND_Opaque);
	    		AddUniqueKey( Key );

	    		if (bDebugMaterialToKey)
				{
					FString NewKeyString = GetProgramKeyXMLOpenText(Key);
					NewKeyString += GetProgramKeyText(Key);
					NewKeyString += GetProgramKeyXMLCloseText();
					WriteDebugText(NewKeyString);
					WriteDebugText(GetXMLIndentLevelString() + TEXT("</KEYS>\n"));
					WriteDebugText(GetMaterialXMLCloseText());
				}
            }
		}

		{
			MobileMaterialName = FString::Printf(TEXT("GlobalShader%d"), (INT)EGST_LightShaftBlur);
			if (bDebugMaterialToKey)
			{
				WriteDebugText(GetMaterialXMLOpenText(*MobileMaterialName));
				WriteDebugText(GetXMLIndentLevelString() + TEXT("<KEYS>\n"));
			}
			// Half of the radial blur drawcalls are additive, but this only required on NGP.
			FProgramKey Key = CalcGlobalShaderKey(EGST_LightShaftBlur, BLEND_Additive);
			AddUniqueKey( Key );

			if (bDebugMaterialToKey)
			{
				FString NewKeyString = GetProgramKeyXMLOpenText(Key);
				NewKeyString += GetProgramKeyText(Key);
				NewKeyString += GetProgramKeyXMLCloseText();
				WriteDebugText(NewKeyString);
				WriteDebugText(GetXMLIndentLevelString() + TEXT("</KEYS>\n"));
				WriteDebugText(GetMaterialXMLCloseText());
			}
		}
	}

	/** Save the new additions to the shader cache */
	void Save( const TCHAR* PlatformEngineConfigFilename, const TCHAR* PlatformSystemSettingsConfigFilename )
	{
		if (bSupported)
		{
			//if they requested a key to material report, print that out!
			if (bDebugKeyToMaterial)
			{
				FString OutputText = FString::Printf(TEXT("<!-- KEY TO MATERIAL REPORT - Key Count=%d -->\n"), AllRequestedShaderKeys.Num());
				WriteDebugText(OutputText);
				WriteDebugText(GetXMLIndentLevelString() + TEXT("<KEYS>\n"));
				IncrementXMLIndentLevel();
				for (INT KeyIndex = 0; KeyIndex < AllRequestedShaderKeys.Num(); ++KeyIndex)
				{
					const TArray<FString>& MaterialNames = MaterialsPerKey(KeyIndex);
					UBOOL bUsingMaterialMaxCount = (DebugMaterialKeyMaxCount > 0) ? TRUE: FALSE;
					UBOOL bNumMaterialsWithinThreshold = (MaterialNames.Num() <= DebugMaterialKeyMaxCount) ? TRUE : FALSE;
					if (!bUsingMaterialMaxCount || (bUsingMaterialMaxCount && bNumMaterialsWithinThreshold))
					{
						OutputText = GetProgramKeyXMLOpenText(AllRequestedShaderKeys(KeyIndex));
						WriteDebugText(OutputText);
						OutputText = GetProgramKeyText(AllRequestedShaderKeys(KeyIndex));
						WriteDebugText(OutputText);
						WriteDebugText(GetXMLIndentLevelString() + TEXT("<MATERIALS>\n"));
						IncrementXMLIndentLevel();
						for (INT MaterialIndex= 0; MaterialIndex < MaterialNames.Num(); ++MaterialIndex)
						{
							OutputText = GetMaterialXMLOpenText(MaterialNames(MaterialIndex));
							WriteDebugText(OutputText);
							WriteDebugText(GetMaterialXMLCloseText());
						}
						DecrementXMLIndentLevel();
						WriteDebugText(GetXMLIndentLevelString() + TEXT("</MATERIALS>\n"));
						WriteDebugText(GetProgramKeyXMLCloseText());
					}
				}
				DecrementXMLIndentLevel();
				WriteDebugText(GetXMLIndentLevelString() + TEXT("</KEYS>\n"));
			}
			//if they requested a list of skinning iterated materials, print that out!
			if (bListSkinningIteratedMaterials)
			{
				warnf(NAME_Log, TEXT("SKINNING ITERATED REPORT - Count=%d"), SkinningIteratedMaterials.Num());
				for (INT MaterialIndex = 0; MaterialIndex < SkinningIteratedMaterials.Num(); ++MaterialIndex)
				{
					warnf(NAME_Log, TEXT("     %s"), *SkinningIteratedMaterials(MaterialIndex));
				}
			}
			//if they requested a list of skinning iterated materials, print that out!
			if (bListLightmapIteratedMaterials)
			{
				warnf(NAME_Log, TEXT("LIGHTMAP ITERATED REPORT - Count=%d"), LightmapIteratedMaterials.Num());
				for (INT MaterialIndex = 0; MaterialIndex < LightmapIteratedMaterials.Num(); ++MaterialIndex)
				{
					warnf(NAME_Log, TEXT("     %s"), *LightmapIteratedMaterials(MaterialIndex));
				}
			}

			TMap<FString,TArray<FProgramKey> > PixelShaderPreprocessedText;
			TMap<FString,TArray<FProgramKey> > VertexShaderPreprocessedText;
			TArray<FNGPShaderCompileInfo> ShaderCompileInfos;

			FString	CommonShaderPrefixFile;
			FString	VertexShaderPrefixFile;
			FString	PixelShaderPrefixFile;
			UBOOL bSuccess = FALSE;
			if (Platform == PLATFORM_Flash)
			{
				bSuccess = appLoadFileToString( CommonShaderPrefixFile, *( appEngineDir() + TEXT("Shaders\\Flash\\Prefix_Common.msf") ) );
				bSuccess = bSuccess && appLoadFileToString( VertexShaderPrefixFile, *( appEngineDir() + TEXT("Shaders\\Flash\\Prefix_VertexShader.msf") ) );
				bSuccess = bSuccess && appLoadFileToString( PixelShaderPrefixFile, *( appEngineDir() + TEXT("Shaders\\Flash\\Prefix_PixelShader.msf") ) );
			}
			else
			{
				bSuccess = appLoadFileToString( CommonShaderPrefixFile, *( appEngineDir() + TEXT("Shaders\\Mobile\\Prefix_Common.msf") ) );
				bSuccess = bSuccess && appLoadFileToString( VertexShaderPrefixFile, *( appEngineDir() + TEXT("Shaders\\Mobile\\Prefix_VertexShader.msf") ) );
				bSuccess = bSuccess && appLoadFileToString( PixelShaderPrefixFile, *( appEngineDir() + TEXT("Shaders\\Mobile\\Prefix_PixelShader.msf") ) );
			}

			if (!bSuccess)
			{
				appErrorf(TEXT("Failed to load shader prefix files."));
			}

			FString CachedProgramKeyFileContents;

			// skip preprocessing shaders in flash
			// @todo flash: Preprocess flash shaders offline
			if (Platform != PLATFORM_Flash)
			{
				//sort shaders by key
				Sort<USE_COMPARE_CONSTREF(FProgramKey,UnContentCookers)>( AllRequestedShaderKeys.GetTypedData(), AllRequestedShaderKeys.Num() );

				// load or preprocess shaders and build equivalence maps
				for( INT KeyIndex = 0; KeyIndex < AllRequestedShaderKeys.Num(); KeyIndex++ )
				{
					FProgramKey ProgramKey = AllRequestedShaderKeys(KeyIndex);
					// Unpack the program key
					FProgramKeyData KeyData;
					KeyData.UnpackProgramKeyData(ProgramKey);

					EMobilePrimitiveType PrimitiveType =(EMobilePrimitiveType)KeyData.GetFieldValue(FProgramKeyData::PKDT_PrimitiveType);
					const EMobileGlobalShaderType GlobalShaderType = (EMobileGlobalShaderType) KeyData.GetFieldValue(FProgramKeyData::PKDT_GlobalShaderType);
					const FString PreprocessedDirectoryName = FString::Printf( TEXT( "%s%s\\" ), *ShadersDirectory, *ProgramKey.ToString() );

					const FFilename VertexFileName = 
						appConvertRelativePathToFull(FString::Printf( TEXT( "%s%s.msf.i" ), 
						*PreprocessedDirectoryName, *GetES2ShaderFilename(PrimitiveType,GlobalShaderType,SF_Vertex ) ));
					const FFilename PixelFileName = 
						appConvertRelativePathToFull(FString::Printf( TEXT( "%s%s.msf.i" ), 
						*PreprocessedDirectoryName, *GetES2ShaderFilename(PrimitiveType,GlobalShaderType,SF_Pixel ) ));
					FString PreprocessedVertexShader;
					FString PreprocessedPixelShader;
					{
						// We're generating shaders to be executed on console platforms
						const UBOOL bIsCompilingForPC = FALSE;

						// NOTE: The following settings will currently not allow different shaders to be saved per platform.
						//		 We chose to do this because it results in dramatically fewer preprocessed shaders and these
						//		 settings should not usually need to be different for different device types in the same SKU
						FLOAT MobileLODBias = 0.0f;
						INT MobileBoneCount = 75;
						INT MobileBoneWeightCount = 4;
						GConfig->GetFloat( TEXT("SystemSettings"), TEXT("MobileLODBias"), MobileLODBias, PlatformSystemSettingsConfigFilename );
						GConfig->GetInt( TEXT("SystemSettings"), TEXT("MobileBoneCount"), MobileBoneCount, PlatformSystemSettingsConfigFilename );
						GConfig->GetInt( TEXT("SystemSettings"), TEXT("MobileBoneWeightCount"), MobileBoneWeightCount, PlatformSystemSettingsConfigFilename );

						FString VertexShaderCode;
						FString PixelShaderCode;
						UBOOL bSuccess = ES2ShaderSourceFromKeys( 
							Platform,
							ProgramKey, 
							KeyData, 
							PrimitiveType,
							bIsCompilingForPC,
							CommonShaderPrefixFile,
							VertexShaderPrefixFile,
							PixelShaderPrefixFile,
							MobileLODBias,
							MobileBoneCount,
							MobileBoneWeightCount,
							NULL,
							NULL,
							VertexShaderCode,
							PixelShaderCode
							);
						if (!bSuccess || !VertexShaderCode.Len() || !PixelShaderCode.Len())
						{
							appErrorf(TEXT("Failed to build shader code."));
						}

						PreprocessedVertexShader = RunCPreprocessor(VertexShaderCode,*VertexFileName);
						if (!PreprocessedVertexShader.Len())
						{
							warnf(NAME_Warning, TEXT("Failed to preprocess vertex shader source '%s'; Check Binaries\\...\\mcpp.err for details or check the installation of Redist\\MCPP\\bin\\mcpp.exe. Not fatal."),*VertexFileName);
							PreprocessedVertexShader = VertexShaderCode;
						}
						PreprocessedPixelShader = RunCPreprocessor(PixelShaderCode,*PixelFileName);
						if (!PreprocessedPixelShader.Len())
						{
							warnf(NAME_Warning, TEXT("Failed to preprocess pixel shader source '%s'; Check Binaries\\...\\mcpp.err for details or check the installation of Redist\\MCPP\\bin\\mcpp.exe. Not fatal."),*PixelFileName);
							PreprocessedPixelShader = PixelShaderCode;
						}
					}

					// vertex
					{
						TArray<FProgramKey>* Existing = VertexShaderPreprocessedText.Find(PreprocessedVertexShader);
						if (!Existing)
						{
							TArray<FProgramKey> Blank;
							VertexShaderPreprocessedText.Set(PreprocessedVertexShader,Blank);
							Existing = VertexShaderPreprocessedText.Find(PreprocessedVertexShader);
							check(Existing);
						}
						Existing->AddUniqueItem(ProgramKey);
					}
					// pixel
					{
						TArray<FProgramKey>* Existing = PixelShaderPreprocessedText.Find(PreprocessedPixelShader);
						if (!Existing)
						{
							TArray<FProgramKey> Blank;
							PixelShaderPreprocessedText.Set(PreprocessedPixelShader,Blank);
							Existing = PixelShaderPreprocessedText.Find(PreprocessedPixelShader);
							check(Existing);
						}
						Existing->AddUniqueItem(ProgramKey);
					}

					if ( Platform & PLATFORM_NGP )
					{
						NGPBeginCompileShader( ShaderCompileInfos, *VertexFileName, *PixelFileName, ProgramKey );
					}
				}

				CachedProgramKeyFileContents = FString::Printf( TEXT( "version:%d\r\n"),SHADER_MANIFEST_VERSION);
				// vertex shader equivalence map
				for (TMap<FString,TArray<FProgramKey> >::TIterator It(VertexShaderPreprocessedText);It;++It)
				{
					if (It.Value().Num() > 1)
					{
						Sort<USE_COMPARE_CONSTREF(FProgramKey,UnContentCookers)>( It.Value().GetTypedData(), It.Value().Num() );
						UBOOL First = TRUE;
						for (TArray<FProgramKey>::TIterator ItQ(It.Value());ItQ;++ItQ,First = FALSE)
						{
							FProgramKey MobileKey = *ItQ;
							CachedProgramKeyFileContents += FString::Printf( TEXT( "%s%s"),
								First ? TEXT("vse:") : TEXT(","), 
								*MobileKey.ToString() );
						}
						CachedProgramKeyFileContents += TEXT("\r\n");
					}
				}

				// pixel shader equivalence map
				for (TMap<FString,TArray<FProgramKey> >::TIterator It(PixelShaderPreprocessedText);It;++It)
				{
					if (It.Value().Num() > 1)
					{
						Sort<USE_COMPARE_CONSTREF(FProgramKey,UnContentCookers)>( It.Value().GetTypedData(), It.Value().Num() );
						UBOOL First = TRUE;
						for (TArray<FProgramKey>::TIterator ItQ(It.Value());ItQ;++ItQ,First = FALSE)
						{
							FProgramKey MobileKey = *ItQ;
							CachedProgramKeyFileContents += FString::Printf( TEXT( "%s%s"),
								First ? TEXT("pse:") : TEXT(","), 
								*MobileKey.ToString() );
						}
						CachedProgramKeyFileContents += TEXT("\r\n");
					}
				}

				for( INT KeyIndex = 0; KeyIndex < AllRequestedShaderKeys.Num(); KeyIndex++ )
				{
					// Construct the contents of the cache file
					FProgramKey ShaderKey = AllRequestedShaderKeys(KeyIndex);
					CachedProgramKeyFileContents += FString::Printf( TEXT( "%s\r\n"), *ShaderKey.ToString() );
				}

				// 			if (Platform & PLATFORM_Flash)
				// 			{
				// 				GFileManager->DeleteDirectory(*CookedDirectory, FALSE, TRUE);
				// 			}
				GFileManager->MakeDirectory(*CookedDirectory);
				FString ProgramKeysFileName =  FString::Printf(TEXT("%sCachedProgramKeys.txt"), *CookedDirectory);
				appSaveStringToFile(CachedProgramKeyFileContents, *ProgramKeysFileName, TRUE);
			}

			if ( Platform & PLATFORM_NGP )
			{
				NGPFinishCompileShaders(ShaderCompileInfos);
			}
			else
			{
				// put the shaders into a single file
				FArchive* AllShadersFile = GFileManager->CreateFileWriter(*(CookedDirectory + TEXT("AllShaders.bin")));
				if (AllShadersFile == NULL)
				{
					warnf(NAME_Error, TEXT("Failed to open %s..\\AllShaders.bin for writing! Shader loading will fail at runtime!!!"), *CookedDirectory);
					return;
				}

				// make the metadata files
				FArchive* EngineShaderInfoFile = GFileManager->CreateFileWriter(*(CookedDirectory + TEXT("EngineShadersInfo.bin")));
				if (EngineShaderInfoFile == NULL)
				{
					warnf(NAME_Error, TEXT("Failed to open %s..\\EngineShadersInfo.bin for writing! Shader loading will fail at runtime!!!"), *CookedDirectory);
					return;
				}

				FArchive* PreprocessedShaderInfoFile = GFileManager->CreateFileWriter(*(CookedDirectory + TEXT("PreprocessedShadersInfo.bin")));
				if (PreprocessedShaderInfoFile == NULL)
				{
					warnf(NAME_Error, TEXT("Failed to open %s..\\PreprocessedShadersInfo.bin for writing! Shader loading will fail at runtime!!!"), *CookedDirectory);
					return;
				}

				// keep a list of engine shader offsets and sizes (QWORD contains offset/size pair)
				TMap<FString, QWORD> EngineShaders;
				TMap<FString, QWORD> PreprocessedShaders;

				// load all the shader names
				TArray<FString> EngineShaderNames;
				FString EngineShaderPrefix = appEngineDir() + ((Platform == PLATFORM_Flash) ? TEXT("Shaders\\Flash") : TEXT("Shaders\\Mobile"));
				appFindFilesInDirectory(EngineShaderNames, *EngineShaderPrefix, FALSE, TRUE);

				TArray<FString> PreprocessedShaderNames;
				FString PreprocessedShaderPrefix = ShadersDirectory;
				// we currently don't preprocess flash shaders
				// @todo flash: Preprocess flash shaders offline
				if (Platform != PLATFORM_Flash)
				{
					appFindFilesInDirectory(PreprocessedShaderNames, *PreprocessedShaderPrefix, FALSE, TRUE);
				}
				else
				{
					FString OutputFile;
					TArray<FString> GLSLShaderNames;
					FString GLSLShaderDir = appGameDir() + TEXT("Shaders\\Vertex");
					appFindFilesInDirectory(GLSLShaderNames, *GLSLShaderDir, FALSE, TRUE);
					for (INT ShaderIndex = 0; ShaderIndex < GLSLShaderNames.Num(); ShaderIndex++)
					{
						OutputFile = GLSLShaderNames(ShaderIndex).RightChop(GLSLShaderNames(ShaderIndex).InStr(TEXT("\\"), TRUE)+1);
						OutputFile = OutputFile.LeftChop(4);
						OutputFile = ShadersDirectory + OutputFile;
						RunShaderConverter(*GLSLShaderNames(ShaderIndex), true, *OutputFile );
					}
					GLSLShaderNames.Empty();
					GLSLShaderDir = appGameDir() + TEXT("Shaders\\Fragment");
					appFindFilesInDirectory(GLSLShaderNames, *GLSLShaderDir, FALSE, TRUE);
					for (INT ShaderIndex = 0; ShaderIndex < GLSLShaderNames.Num(); ShaderIndex++)
					{
						OutputFile = GLSLShaderNames(ShaderIndex).RightChop(GLSLShaderNames(ShaderIndex).InStr(TEXT("\\"), TRUE)+1);
						OutputFile = OutputFile.LeftChop(4);
						OutputFile = ShadersDirectory + OutputFile;
						RunShaderConverter(*GLSLShaderNames(ShaderIndex), false, *OutputFile );
					}
				}

				// now put them together into a single file
				for (INT ShaderIndex = 0; ShaderIndex < EngineShaderNames.Num(); ShaderIndex++)
				{
					// only do .msf files
					if (!EngineShaderNames(ShaderIndex).EndsWith(TEXT(".msf")))
					{
						continue;
					}

					TArray<BYTE> Shader;
					appLoadFileToArray(Shader, *EngineShaderNames(ShaderIndex));
					// null terminate the string
					Shader.AddZeroed(1);

					// skip over the Engine\\Shaders\\Mobile to save memory
					FString ShaderTag = EngineShaderNames(ShaderIndex).Mid(EngineShaderPrefix.Len() + 1);
					// make QWORD
					QWORD ShaderOffset = AllShadersFile->Tell();
					QWORD ShaderSize = Shader.Num();
					QWORD ShaderInfo = (ShaderOffset << 32) | ShaderSize;

					// update metadata and the AllShaders file
					EngineShaders.Set(ShaderTag, ShaderInfo);
					AllShadersFile->Serialize(Shader.GetData(), Shader.Num());
				}

				for (INT ShaderIndex = 0; ShaderIndex < PreprocessedShaderNames.Num(); ShaderIndex++)
				{
					// only do preprocessed files
					if (!PreprocessedShaderNames(ShaderIndex).EndsWith(TEXT(".i")))
					{
						continue;
					}

					TArray<BYTE> Shader;
					appLoadFileToArray(Shader, *PreprocessedShaderNames(ShaderIndex));
					// null terminate the string
					Shader.AddZeroed(1);

					// skip over the Cooked\\Shaders\\ to save memory
					FString ShaderTag = PreprocessedShaderNames(ShaderIndex).Mid(PreprocessedShaderPrefix.Len());
					// make QWORD
					QWORD ShaderOffset = AllShadersFile->Tell();
					QWORD ShaderSize = Shader.Num();
					QWORD ShaderInfo = (ShaderOffset << 32) | ShaderSize;

					// update metadata and the AllShaders file
					PreprocessedShaders.Set(ShaderTag, ShaderInfo);
					AllShadersFile->Serialize(Shader.GetData(), Shader.Num());
				}
				delete AllShadersFile;

				// save the meta data
				EngineShaderInfoFile->SetByteSwapping(bShouldByteSwap);
				*EngineShaderInfoFile << EngineShaders;
				delete EngineShaderInfoFile;

				PreprocessedShaderInfoFile->SetByteSwapping(bShouldByteSwap);
				*PreprocessedShaderInfoFile << PreprocessedShaders;
				delete PreprocessedShaderInfoFile;

				SaveShaderGroups();
				SaveShaderGroupsBinary();
			}
		}
	}


	/** Save a single ShaderGroup out in the ShaderGroups.ini file. */
	UBOOL SaveShaderGroup( FConfigFile & ConfigFile, const FString & GroupName, const TArray<FProgramKey>& ShaderGroup )
	{
		TArray<FString> Keys;

		FConfigSection* Sec  = ConfigFile.Find(*GroupName);
		if (!Sec)
		{
			Sec = &ConfigFile.Set(*GroupName, FConfigSection());
		}

		for ( INT KeyIndex=0; KeyIndex < ShaderGroup.Num(); ++KeyIndex )
		{
			FProgramKey ShaderKey = ShaderGroup(KeyIndex);
			Sec->Add(TEXT("Key"), *ShaderKey.ToString());
		}

		return TRUE;
	}


	/** Save all ShaderGroups out in an ShaderGroups.ini file. */
	void SaveShaderGroups()
	{
		FConfigFile ShaderGroupIni;
		FString ShaderIniFilename =  CookedDirectory + TEXT("ShaderGroups.ini");

		// Store the shader group for the startup packages
		SaveShaderGroup( ShaderGroupIni, TEXT("StartupPackages"), StartupShaderGroup );

		for ( TMap<FName, TArray<FProgramKey> >::TConstIterator It(ShaderGroups); It; ++It )
		{
			FName ShaderGroupName = It.Key();
			const TArray<FProgramKey>& ShaderKeys = It.Value();
			SaveShaderGroup( ShaderGroupIni, ShaderGroupName.ToString(), ShaderKeys );
		}

		ShaderGroupIni.Dirty = TRUE;
		ShaderGroupIni.Write(*ShaderIniFilename);
	}

	/** Save a single ShaderGroup out in a binary form. */
	UBOOL SaveShaderGroupBinary( FArchive & Ar, FString & GroupName, const TArray<FProgramKey>& ShaderGroup )
	{

		Ar << GroupName;
		INT ShaderGroupNum = ShaderGroup.Num();
		Ar << ShaderGroupNum;

		for ( INT KeyIndex=0; KeyIndex < ShaderGroup.Num(); ++KeyIndex )
		{
			FProgramKey Key = ShaderGroup(KeyIndex);
			Ar << Key;
		}

		return TRUE;
	}

	/** Save all ShaderGroups out in a binary form. */
	void SaveShaderGroupsBinary()
	{
		FString ShaderBinFilename =  CookedDirectory + TEXT("ShaderGroups.bin");
		FArchive * ShaderBinFile = GFileManager->CreateFileWriter(*ShaderBinFilename);

		// Store the shader group for the startup packages
		FString StartupPackages = TEXT("StartupPackages");
		SaveShaderGroupBinary( *ShaderBinFile, StartupPackages, StartupShaderGroup );

		for ( TMap<FName, TArray<FProgramKey> >::TConstIterator It(ShaderGroups); It; ++It )
		{
			FName ShaderGroupName = It.Key();
			FString GroupName = ShaderGroupName.ToString();
			const TArray<FProgramKey>& ShaderKeys = It.Value();
			SaveShaderGroupBinary( *ShaderBinFile, GroupName, ShaderKeys );
		}

		delete ShaderBinFile;
	}


	void LoadAllShaderGroups()
	{
		FConfigFile ShaderGroupIni;
		FString ShaderGroupIniFile = CookedDirectory + TEXT("ShaderGroups.ini");
		ShaderGroupIni.Read(*ShaderGroupIniFile);

		for ( FConfigFile::TIterator It(ShaderGroupIni); It; ++It )
		{
			FConfigSection* Sec = ShaderGroupIni.Find(*It.Key());
			if ( Sec != NULL )
			{
				TArray<FString> ShaderStringKeysArray;
				TArray<FProgramKey> ShaderProgramKeysArray;
				Sec->MultiFind(TEXT("Key"), ShaderStringKeysArray);

				for (INT ArrayIndex = 0; ArrayIndex < ShaderStringKeysArray.Num(); ++ArrayIndex)
				{
					ShaderProgramKeysArray.AddItem(FProgramKey(ShaderStringKeysArray(ArrayIndex)));
				}

				FName ShaderGroupName = *It.Key();
				ShaderGroups.Set(ShaderGroupName, ShaderProgramKeysArray);
			}
		}

		if (ShaderGroups.Find(TEXT("StartupPackages")))
		{
			StartupShaderGroup = *ShaderGroups.Find(TEXT("StartupPackages"));
			ShaderGroups.Remove(TEXT("StartupPackages"));
			ShaderGroups.Shrink();
		}
	}

	/** Load current state of the shader cache */
	void Load( void )
	{
		if (bSupported)
		{
			FString CachedProgramKeyFileContents;
			FString ProgramKeysFileName = FString::Printf(TEXT("%sCachedProgramKeys.txt"), *CookedDirectory);
			appLoadFileToString(CachedProgramKeyFileContents, *ProgramKeysFileName);

			//parse into temp array
			TArray <FString> KeyStrings;
			CachedProgramKeyFileContents.ParseIntoArray(&KeyStrings, TEXT("\r\n"), TRUE);
			if( KeyStrings.Num() > 0 )
			{
				FString& VersionString = KeyStrings(0);
				const FString VersionToken(TEXT("version:"));
				if( VersionString.StartsWith(VersionToken) )
				{
					INT ManifestVersion = appAtoi(*VersionString.Mid(VersionToken.Len()));
					if( ManifestVersion != SHADER_MANIFEST_VERSION )
					{
						warnf(NAME_Warning, TEXT("Shader manifest is an old version, ignoring."));
					}
					else
					{
						// Shader manifest is valid, proceed to load and cache the shader programs

						// Parse the program keys, ignoring equivalence classes; we will build those later
						const FString vse(TEXT("vse:"));
						const FString pse(TEXT("pse:"));
						for( INT KeyIndex = 1; KeyIndex < KeyStrings.Num(); KeyIndex++ )  // start with 1 because the version is on line 0
						{
							FString& ProgramKeyName = KeyStrings(KeyIndex);

							UBOOL bIsVertexShaderEquivalence = ProgramKeyName.StartsWith(vse);
							UBOOL bIsPixelShaderEquivalence = ProgramKeyName.StartsWith(pse);

							// Check for an equivalence group
							if( !(bIsVertexShaderEquivalence || bIsPixelShaderEquivalence) )
							{
								FProgramKey ProgramKey( ProgramKeyName );
								// Unpack the program key
								FProgramKeyData KeyData;
								KeyData.UnpackProgramKeyData(ProgramKey);
								AddUniqueKey(ProgramKey);
							}
						}
					}
				}
			}

			LoadAllShaderGroups();
		}
	}

	void SetupStartupShaderGroup()
	{
		SetCurrentPackage( NULL );

		AddGlobalShaders();
		AddFallbackShaders();

		TArray<FString>	StartupShaderKeys;
		GConfig->GetArray(TEXT("Engine.StartupShaderKeys"), TEXT("Key"), StartupShaderKeys, GEngineIni);
		for (TArray<FString>::TConstIterator It(StartupShaderKeys); It; ++It)
		{
			FProgramKey NewKey(*It);
			AddUniqueKey(NewKey);
		}
	}

	/**
	 * Sets the current package.
	 * All mobile shaders added after this will be added to this shader group.
	 */
	void SetCurrentPackage( UPackage* Package )
	{
		CurrentShaderGroupName = FName(NAME_None);
		if ( Package == NULL )
		{
			CurrentShaderGroup = &StartupShaderGroup;
		}
		else
		{
 			for ( TMap<FName, TArray<FName> >::TConstIterator It(ShaderGroupPackages); It && CurrentShaderGroupName == NAME_None; ++It )
			{
				const TArray<FName>& PackageNames = It.Value();
				for ( INT PackageIndex=0; PackageIndex < PackageNames.Num(); ++PackageIndex )
				{
					if ( PackageNames(PackageIndex) == Package->GetFName() )
					{
						CurrentShaderGroupName = It.Key();
						break;
					}
				}
			}
			CurrentShaderGroup = (CurrentShaderGroupName != NAME_None) ? &ShaderGroups.FindOrAdd( CurrentShaderGroupName ) : &RemainderShaderGroup;
		}
	}

	/** Call to alert us that we are about to iterate materials, so reflect that in the XML */
	void StartMaterialList (void)
	{
		if (bDebugKeyToMaterial || bDebugMaterialToKey || bDebugFullMaterialReport)
		{
			WriteDebugText(GetXMLIndentLevelString() + TEXT("<MATERIALS>\n"));
			IncrementXMLIndentLevel();
		}
	}

	/** Call to alert us that we are about to stop iterating materials, so reflect that in the XML */
	void StopMaterialList (void)
	{
		if (bDebugKeyToMaterial || bDebugMaterialToKey || bDebugFullMaterialReport)
		{
			DecrementXMLIndentLevel();
			WriteDebugText(GetXMLIndentLevelString() + TEXT("</MATERIALS>\n"));
		}
	}

	/** 
	 * Add shader groups programatically
	 */
	void AddDynamicShaderGroup(const FName& GroupName, const FName& PackageName)
	{
		warnf(NAME_Log, TEXT("@@@ Adding Shader Group %s, package %s"), *GroupName.ToString(), *PackageName.ToString());
		TArray<FName>& PackageList = ShaderGroupPackages.FindOrAdd(GroupName);

		PackageList.AddUniqueItem(PackageName);
	}


private:

	/** Write debug text either to standard warn output, or a log file if requested */
	void WriteDebugText (const FString &OutputText)
	{
		if (bDebugUseMaterialLog)
		{
			appAppendStringToFile(OutputText, *DebugMaterialLogName);
		}
		else
		{
			warnf(NAME_Log, *OutputText);
		}
	}

	/** Return a string with tabs representing the current XML indent level */
	FString GetXMLIndentLevelString (void) const
	{
		FString IndentString;
		for (INT scan = 0; scan < DebugXMLIndentLevel; ++scan)
		{
			IndentString += TEXT("\t");
		}

		return IndentString;
	}

	/** Note that we are indenting a level in the debug XML output */
	void IncrementXMLIndentLevel (void) const
	{
		DebugXMLIndentLevel++;
	}

	/** Note that we are unindenting a level in the debug XML output */
	void DecrementXMLIndentLevel (void) const
	{
		DebugXMLIndentLevel--;
	}

	/** Helper functions for building XML material reports - returns open XML element text for a program key*/
	FString GetProgramKeyXMLOpenText (const FProgramKey &Key) const
	{
		FString ReturnString = GetXMLIndentLevelString() + FString::Printf(TEXT("<KEY code=\"%s\">\n"), *(Key.ToString()));
		IncrementXMLIndentLevel();
		return ReturnString;
	}

	/** Helper functions for building XML material reports - returns closing XML element text for a program key*/
	FString GetProgramKeyXMLCloseText (void) const
	{
		DecrementXMLIndentLevel();
		return GetXMLIndentLevelString() + TEXT("</KEY>\n");
	}

	/** Helper functions for building XML material reports - returns opening XML element text for a material*/
	FString GetMaterialXMLOpenText (const FString &Name) const
	{
		FString ReturnString = GetXMLIndentLevelString() + FString::Printf(TEXT("<MATERIAL name=\"%s\">\n"),*Name);
		IncrementXMLIndentLevel();
		return ReturnString;
	}

	/** Helper functions for building XML material reports - returns closing XML element text for a material*/
	FString GetMaterialXMLCloseText (void) const
	{
		DecrementXMLIndentLevel();
		return GetXMLIndentLevelString() + TEXT("</MATERIAL>\n");
	}

	/** Rough approximation of the Shader Key Tool.  Convert a ShaderKey into text displaying what bits/bitvalues are set */
	FString GetProgramKeyText (const FProgramKey &Key)
	{
#if !FINAL_RELEASE
		FString BaseText = Key.ToString();

		INT SplitIndex = BaseText.InStr(FString(TEXT("_")));
		if (SplitIndex < 0)
		{
			return BaseText;
		}

		FString LeftString, RightString;
		BaseText.Split(FString(TEXT("_")), &LeftString, &RightString, SplitIndex);

		INT StrLen = LeftString.Len();
		TCHAR* StrEnd = const_cast<TCHAR*>(*LeftString + StrLen - 1);
		QWORD keyHigh = appStrtoi64(*LeftString, &StrEnd, 16);
		
		StrLen = RightString.Len();
		StrEnd = const_cast<TCHAR*>(*RightString + StrLen - 1);
		QWORD keyLow = appStrtoi64(*RightString, &StrEnd, 16);

		FString FinalString;

		static const FString ArrayPKDT_AlphaValueSource[] = { TEXT("MAVS_DiffuseTextureAlpha"), TEXT("MAVS_MaskTextureRed"), TEXT("MAVS_MaskTextureGreen"), TEXT("MAVS_MaskTextureBlue") };
		static const FString ArrayPKDT_EmissiveMaskSource[] = { TEXT("MVS_Constant"), TEXT("MVS_VertexColorRed"), TEXT("MVS_VertexColorGreen"), TEXT("MVS_VertexColorBlue"), TEXT("MVS_VertexColorAlpha"), TEXT("MVS_BaseTextureRed"), TEXT("MVS_BaseTextureGreen"), TEXT("MVS_BaseTextureBlue"), TEXT("MVS_BaseTextureAlpha"), TEXT("MVS_MaskTextureRed"), TEXT("MVS_MaskTextureGreen"), TEXT("MVS_MaskTextureBlue"), TEXT("MVS_MaskTextureAlpha"), TEXT("MVS_NormalTextureAlpha"), TEXT("MVS_EmissiveTextureRed"), TEXT("MVS_EmissiveTextureGreen"), TEXT("MVS_EmissiveTextureBlue"), TEXT("MVS_EmissiveTextureAlpha")};
		static const FString ArrayPKDT_EmissiveColorSource[] = { TEXT("MECS_EmissiveTexture"), TEXT("MECS_BaseTexture"), TEXT("MECS_Constant") };
		static const FString ArrayPKDT_EnvironmentMaskSource[] = { TEXT("MVS_Constant"), TEXT("MVS_VertexColorRed"), TEXT("MVS_VertexColorGreen"), TEXT("MVS_VertexColorBlue"), TEXT("MVS_VertexColorAlpha"), TEXT("MVS_BaseTextureRed"), TEXT("MVS_BaseTextureGreen"), TEXT("MVS_BaseTextureBlue"), TEXT("MVS_BaseTextureAlpha"), TEXT("MVS_MaskTextureRed"), TEXT("MVS_MaskTextureGreen"), TEXT("MVS_MaskTextureBlue"), TEXT("MVS_MaskTextureAlpha"), TEXT("MVS_NormalTextureAlpha"), TEXT("MVS_EmissiveTextureRed"), TEXT("MVS_EmissiveTextureGreen"), TEXT("MVS_EmissiveTextureBlue"), TEXT("MVS_EmissiveTextureAlpha") };
		static const FString ArrayPKDT_RimLightingMaskSource[] = { TEXT("MVS_Constant"), TEXT("MVS_VertexColorRed"), TEXT("MVS_VertexColorGreen"), TEXT("MVS_VertexColorBlue"), TEXT("MVS_VertexColorAlpha"), TEXT("MVS_BaseTextureRed"), TEXT("MVS_BaseTextureGreen"), TEXT("MVS_BaseTextureBlue"), TEXT("MVS_BaseTextureAlpha"), TEXT("MVS_MaskTextureRed"), TEXT("MVS_MaskTextureGreen"), TEXT("MVS_MaskTextureBlue"), TEXT("MVS_MaskTextureAlpha"), TEXT("MVS_NormalTextureAlpha"), TEXT("MVS_EmissiveTextureRed"), TEXT("MVS_EmissiveTextureGreen"), TEXT("MVS_EmissiveTextureBlue"), TEXT("MVS_EmissiveTextureAlpha") };
		static const FString ArrayPKDT_AmbientOcclusionSource[] = { TEXT("MAOS_Disabled"), TEXT("MAOS_VertexColorRed"), TEXT("MAOS_VertexColorGreen"), TEXT("MAOS_VertexColorBlue"), TEXT("MAOS_VertexColorAlpha") };
		static const FString ArrayPKDT_SpecularMask[] = { TEXT("MSM_Constant"), TEXT("MSM_Luminance"), TEXT("MSM_DiffuseRed"), TEXT("MSM_DiffuseGreen"), TEXT("MSM_DiffuseBlue"), TEXT("MSM_DiffuseAlpha"), TEXT("MSM_MaskTextureRGB"), TEXT("MSM_MaskTextureRed"), TEXT("MSM_MaskTextureGreen"), TEXT("MSM_MaskTextureBlue"), TEXT("MSM_MaskTextureAlpha") };
		static const FString ArrayPKDT_MaskTextureTexCoordsSource[] = { TEXT("MTCS_TexCoords0"), TEXT("MTCS_TexCoords1"), TEXT("MTCS_TexCoords2"), TEXT("MTCS_TexCoords3") };
		static const FString ArrayPKDT_DetailTextureTexCoordsSource[] = { TEXT("MTCS_TexCoords0"), TEXT("MTCS_TexCoords1"), TEXT("MTCS_TexCoords2"), TEXT("MTCS_TexCoords3") };
		static const FString ArrayPKDT_BaseTextureTexCoordsSource[] = { TEXT("MTCS_TexCoords0"), TEXT("MTCS_TexCoords1"), TEXT("MTCS_TexCoords2"), TEXT("MTCS_TexCoords3") };
		static const FString ArrayPKDT_BlendMode[] = { TEXT("BLEND_Opaque"), TEXT("BLEND_Masked"), TEXT("BLEND_Translucent"), TEXT("BLEND_Additive"), TEXT("BLEND_Modulate"), TEXT("BLEND_ModulateAndAdd"), TEXT("BLEND_SoftMasked"), TEXT("BLEND_AlphaComposite"), TEXT("BLEND_DitheredTranslucent") };
		static const FString ArrayPKDT_PrimitiveType[] = { TEXT("EPT_Default"), TEXT("EPT_Particle"), TEXT("EPT_BeamTrailParticle"), TEXT("EPT_LensFlare"), TEXT("EPT_Simple"), TEXT("EPT_DistanceFieldFont"), TEXT("EPT_GlobalShader") };
		static const FString ArrayPKDT_PlatformFeatures[] = { TEXT("EPF_HighEndFeatures"), TEXT("EPF_LowEndFeatures") };
		static const FString ArrayPKDT_ColorMultiplySource[] = { TEXT("MCMS_None"), TEXT("MCMS_BaseTextureRed"), TEXT("MCMS_BaseTextureGreen"), TEXT("MCMS_BaseTextureBlue"), TEXT("MCMS_BaseTextureAlpha"), TEXT("MCMS_MaskTextureRed"), TEXT("MCMS_MaskTextureGreen"), TEXT("MCMS_MaskTextureBlue"), TEXT("MCMS_MaskTextureAlpha") };
		static const FString ArrayPKDT_GfxBlendMode[] = { TEXT("EGFXBM_Disabled"), TEXT("EGFXBM_Normal"), TEXT("EGFXBM_Add"), TEXT("EGFXBM_Subtract"), TEXT("EGFXBM_Multiply"), TEXT("EGFXBM_Darken"), TEXT("EGFXBM_Lighten"), TEXT("EGFXBM_None"), TEXT("EGFXBM_SourceAc") };
		static const FString ArrayPKDT_GlobalShaderType[] = { TEXT("EGST_None"), TEXT("EGST_GammaCorrection"), TEXT("EGST_Filter1"), TEXT("EGST_Filter4"), TEXT("EGST_Filter16"), TEXT("EGST_LUTBlender"), TEXT("EGST_UberPostProcess"), TEXT("EGST_LightShaftDownSample"), TEXT("EGST_LightShaftDownSample_NoDepth"), TEXT("EGST_LightShaftBlur"), TEXT("EGST_LightShaftApply"), TEXT("EGST_SimpleF32"), TEXT("EGST_PositionOnly"), TEXT("EGST_ShadowProjection"), TEXT("EGST_BloomGather"), TEXT("EGST_DOFAndBloomGather"), TEXT("EGST_MobileUberPostProcess"), TEXT("EGST_MobileUberPostProcessNoColorGrading"), TEXT("EGST_VisualizeTexture") };
		static const FString ArrayPKDT_DepthShaderType[] = { TEXT("MobileDepthShader_None"), TEXT("MobileDepthShader_Normal"), TEXT("MobileDepthShader_Shadow")};
		static const FString ArrayPKDT_ParticleAlignType[] = { TEXT("ParticleAlignSSA_CameraFacing"), TEXT("ParticleAlignSSA_Velocity"), TEXT("ParticleAlignSSA_LockedAxis")};

		// key 0
		ParseBits(keyLow, 2, FString(TEXT("PKDT_AlphaValueSource")), FinalString, ARRAY_COUNT(ArrayPKDT_AlphaValueSource), (FString*)ArrayPKDT_AlphaValueSource);
		ParseBits(keyLow, 5, FString(TEXT("PKDT_EmissiveMaskSource")), FinalString, ARRAY_COUNT(ArrayPKDT_EmissiveMaskSource), (FString*)ArrayPKDT_EmissiveMaskSource);
		ParseBits(keyLow, 2, FString(TEXT("PKDT_EmissiveColorSource")), FinalString, ARRAY_COUNT(ArrayPKDT_EmissiveColorSource), (FString*)ArrayPKDT_EmissiveColorSource);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsEmissiveEnabled")), FinalString, 0, NULL);
		ParseBits(keyLow, 5, FString(TEXT("PKDT_EnvironmentMaskSource")), FinalString, ARRAY_COUNT(ArrayPKDT_EnvironmentMaskSource), (FString*)ArrayPKDT_EnvironmentMaskSource);
		ParseBits(keyLow, 5, FString(TEXT("PKDT_RimLightingMaskSource")), FinalString, ARRAY_COUNT(ArrayPKDT_RimLightingMaskSource), (FString*)ArrayPKDT_RimLightingMaskSource);

		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsRimLightingEnabled")), FinalString, 0, NULL);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_UseVertexColorMultiply")), FinalString, 0, NULL);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_UseUniformColorMultiply")), FinalString, 0, NULL);
		ParseBits(keyLow, 3, FString(TEXT("PKDT_AmbientOcclusionSource")), FinalString, ARRAY_COUNT(ArrayPKDT_RimLightingMaskSource), (FString*)ArrayPKDT_RimLightingMaskSource);
		ParseBits(keyLow, 4, FString(TEXT("PKDT_SpecularMask")), FinalString, ARRAY_COUNT(ArrayPKDT_SpecularMask), (FString*)ArrayPKDT_SpecularMask);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_TextureBlendFactorSource")), FinalString, 0, NULL);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsUsingOneDetailTexture")), FinalString, 0, NULL);

		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsUsingTwoDetailTexture")), FinalString, 0, NULL);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsUsingThreeDetailTexture")), FinalString, 0, NULL);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_MobileEnvironmentBlendMode")), FinalString, 0, NULL);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsEnvironmentMappingEnabled")), FinalString, 0, NULL);

		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsDetailTextureTransformed")), FinalString, 0, NULL);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsMaskTextureTransformed")), FinalString, 0, NULL);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsNormalTextureTransformed")), FinalString, 0, NULL);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsEmissiveTextureTransformed")), FinalString, 0, NULL);

		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsBaseTextureTransformed")), FinalString, 0, NULL);

		ParseBits(keyLow, 2, FString(TEXT("PKDT_MaskTextureTexCoordsSource")), FinalString, ARRAY_COUNT(ArrayPKDT_MaskTextureTexCoordsSource), (FString*)ArrayPKDT_MaskTextureTexCoordsSource);
		ParseBits(keyLow, 2, FString(TEXT("PKDT_DetailTextureTexCoordsSource")), FinalString, ARRAY_COUNT(ArrayPKDT_DetailTextureTexCoordsSource), (FString*)ArrayPKDT_DetailTextureTexCoordsSource);
		ParseBits(keyLow, 2, FString(TEXT("PKDT_BaseTextureTexCoordsSource")), FinalString, ARRAY_COUNT(ArrayPKDT_BaseTextureTexCoordsSource), (FString*)ArrayPKDT_BaseTextureTexCoordsSource);
		ParseBits(keyLow, 3, FString(TEXT("PKDT_BlendMode")), FinalString, ARRAY_COUNT(ArrayPKDT_BlendMode), (FString*)ArrayPKDT_BlendMode);

		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsSubUV")), FinalString, 0, NULL);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsDecal")), FinalString, 0, NULL);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsSkinned")), FinalString, 0, NULL);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsLightmap")), FinalString, 0, NULL);

		ParseBits(keyLow, 1, FString(TEXT("PKDT_UseGammaCorrection")), FinalString, 0, NULL);
		ParseBits(keyLow, 2, FString(TEXT("PKDT_ParticleScreenAlignment")), FinalString, ARRAY_COUNT(ArrayPKDT_ParticleAlignType), (FString*)(ArrayPKDT_ParticleAlignType));
		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsGradientFogEnabled")), FinalString, 0, NULL);
		ParseBits(keyLow, 1, FString(TEXT("PKDT_IsDepthOnlyRendering")), FinalString, 0, NULL);

		ParseBits(keyLow, 3, FString(TEXT("PKDT_PrimitiveType")), FinalString, ARRAY_COUNT(ArrayPKDT_PrimitiveType), (FString*)ArrayPKDT_PrimitiveType);
		ParseBits(keyLow, 2, FString(TEXT("PKDT_PlatformFeatures")), FinalString, ARRAY_COUNT(ArrayPKDT_PlatformFeatures), (FString*)ArrayPKDT_PlatformFeatures);

		// key 1
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_IsGfxGammaCorrectionEnabled")), FinalString, 0, NULL);
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_UseLandscapeMonochromeLayerBlending")), FinalString, 0, NULL);
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_IsLandscape")), FinalString, 0, NULL);
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_UseFallbackStreamColor")), FinalString, 0, NULL);
		ParseBits(keyHigh, 4, FString(TEXT("PKDT_ColorMultiplySource")), FinalString, ARRAY_COUNT(ArrayPKDT_ColorMultiplySource), (FString*)ArrayPKDT_ColorMultiplySource);
		ParseBits(keyHigh, 5, FString(TEXT("PKDT_GfxBlendMode")), FinalString, ARRAY_COUNT(ArrayPKDT_GfxBlendMode), (FString*)ArrayPKDT_GfxBlendMode);
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_IsMobileColorGradingEnabled")), FinalString, 0, NULL);
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_IsBumpOffsetEnabled")), FinalString, 0, NULL);
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_IsMobileEnvironmentFresnelEnabled")), FinalString, 0, NULL);
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_IsDetailNormalEnabled")), FinalString, 0, NULL);
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_IsWaveVertexMovementEnabled")), FinalString, 0, NULL);

		ParseBits(keyHigh, 1, FString(TEXT("PKDT_TwoSided")), FinalString, 0, NULL);
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_IsHeightFogEnabled")), FinalString, 0, NULL);
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_IsNormalMappingEnabled")), FinalString, 0, NULL);
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_IsPixelSpecularEnabled")), FinalString, 0, NULL);

		ParseBits(keyHigh, 1, FString(TEXT("PKDT_IsSpecularEnabled")), FinalString, 0, NULL);
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_IsLightingEnabled")), FinalString, 0, NULL);
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_IsDirectionalLightmap")), FinalString, 0, NULL);
		ParseBits(keyHigh, 1, FString(TEXT("PKDT_ForwardShadowProjectionShaderType")), FinalString, 0, NULL);

		ParseBits(keyHigh, 2, FString(TEXT("PKDT_DepthShaderType")), FinalString, ARRAY_COUNT(ArrayPKDT_DepthShaderType), (FString*)ArrayPKDT_DepthShaderType);
		ParseBits(keyHigh, 0, FString(TEXT("PKDT_GlobalShaderType2")), FinalString, 0, NULL);
		ParseBits(keyHigh, 10, FString(TEXT("PKDT_GlobalShaderType")), FinalString, ARRAY_COUNT(ArrayPKDT_GlobalShaderType), (FString*)ArrayPKDT_GlobalShaderType);
		
		return FinalString;
#else
		return TEXT("");
#endif
	}

	/** Ripped from Shader key Tool, translate shader bits into their textual representation */
	void ParseBits (QWORD &Bits, INT NumBits, const FString &Name, FString &FinalString, INT NumEnums, FString *Enums)
	{
		if (NumBits > 0)
		{
			QWORD Mask = (QWORD)(appPow(2.0f, NumBits) - 1);
			INT Result = (int)(Bits & Mask);

			// only append if this is different than default
			if (Result && (Enums != NULL) && (Result <= (NumEnums-1)))
			{
				FinalString = FString::Printf(TEXT("%s%s<SETTING name=\"%s\" value=\"%d\"/>\n"), *FinalString, *GetXMLIndentLevelString(), *(Enums[Result]), Result);
			}
			else if (Result)
			{
				FinalString = FString::Printf(TEXT("%s%s<SETTING name=\"%s\"/>\n"), *FinalString, *GetXMLIndentLevelString(), *Name);
			}

			Bits >>= NumBits;
		}
	}

	/** Internal storage for particle to mesh linkages */
	struct FParticleMaterialDesc
	{
		UBOOL operator==( const FParticleMaterialDesc& Other) const
		{
			if ((MaterialName == Other.MaterialName) &&
				(SpriteScreenAlignment == Other.SpriteScreenAlignment) &&
				(InterpolationMethod == Other.InterpolationMethod) &&
				(bMeshParticle == Other.bMeshParticle) &&
				(bBeamTrailParticle == Other.bBeamTrailParticle))
			{
				return TRUE;
			}
			return FALSE;
		}

		FString MaterialName;
		BYTE SpriteScreenAlignment;
		BYTE InterpolationMethod;
		BYTE bMeshParticle;
		BYTE bBeamTrailParticle;
	};

	TArray<FProgramKey> AllRequestedShaderKeys;
	//for each key, the unique names of all requesting materials
	TArray<TArray<FString> > MaterialsPerKey;
	// used to track already added keys per material
	TArray<FProgramKey> CurrentMaterialRequestedShaderKeys;
	// used to track all unique materials
	TArray<FString> AllMaterialsFound;

	//the list of all materials requesting skinning to be fully iterated in mobile shader key generation
	TArray<FString> SkinningIteratedMaterials;
	//the list of all materials requesting lightmap usage to be fully iterated in mobile shader key generation
	TArray<FString> LightmapIteratedMaterials;

	//List of materials provided by the Engine.ini saying if we can skip non-skinning shader key generation
	TArray<FString> SkinningOnlyMaterials;
	//List of materials provided by the Engine.ini saying if we can skip lightmap shader key generation
	TArray<FString> NoLightmapOnlyMaterials;


	//instrumentation variable for tracking "current" cooked material
	FString MobileMaterialName;

	UBOOL bSupported;
	UBOOL bShouldByteSwap;

	//Debug instrumentation flags
	UBOOL bDebugMaterialToKey;
	UBOOL bDebugKeyToMaterial;
	UBOOL bDebugFullMaterialReport;
	UBOOL bListSkinningIteratedMaterials;
	UBOOL bListLightmapIteratedMaterials;
	UBOOL bDebugUseMaterialLog;

	mutable INT	DebugXMLIndentLevel;
	INT	DebugMaterialKeyMaxCount;
	FFilename DebugMaterialLogName;			// Output to log file only
	FFilename ShadersDirectory;
	FFilename CookedDirectory;
	UE3::EPlatformType Platform;

	TArray <FParticleMaterialDesc> ParticleMaterialDescs;

	// All of the Packages that contribute to each ShaderGroup
	TMap<FName, TArray<FName> >			ShaderGroupPackages;
	// All of the FProgramKeys in each ShaderGroup
	TMap<FName, TArray<FProgramKey> >	ShaderGroups;
	// Array of all of the Global and Fallback Shaders
	TArray<FProgramKey>					StartupShaderGroup;
	// All ungrouped shaders
	TArray<FProgramKey>					RemainderShaderGroup;
	FName								CurrentShaderGroupName;
	TArray<FProgramKey>*				CurrentShaderGroup;
	UBOOL								bUsedByStartupPackages;
};

FConsoleMobileShaderCooker GMobileShaderCooker;

/** 
 * Sanitise and display shader compiler errors
 * 
 * Don't suppress these error messages as they will never be seen in the material editor
 * And are needed to track down platform specific compilation issues
 */
void DumpShaderCompileErrors( FMaterialResource* MaterialResource, FString* OutputString )
{
	const TArray<FString>& CompileErrors = MaterialResource->GetCompileErrors();
	for( INT ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++ )
	{
		FString CompileError = CompileErrors( ErrorIndex );
		CompileError = CompileError.Replace( TEXT( ": error " ), TEXT( ": shader compiler error " ) );

		TArray<FString> ErrorArray;
		FString OutWarningString = TEXT("");
		CompileError.ParseIntoArray(&ErrorArray, TEXT("\n"), TRUE);

		for (INT ErrorIndex = 0; ErrorIndex < ErrorArray.Num(); ErrorIndex++)
		{
			if (ErrorArray(ErrorIndex).InStr(TEXT("warning ")) == INDEX_NONE)
			{
				OutWarningString += FString((OutWarningString.Len() > 0 ? TEXT("\n") : TEXT(""))) + ErrorArray(ErrorIndex);
			}
		}

		warnf( NAME_Warning, TEXT( "    %s" ), *OutWarningString );

		if (OutputString != NULL)
		{
			*OutputString += OutWarningString;
		}
	}
}

/**
 * Warn if a texture that is marked as a compositing source is used in a material
 */
void CheckMobileMaterialForCompositingTextures(UMaterialInterface* MaterialInterface)
{
	// check for any compositing source textures in materials, and warn if so
	for (INT i = Base_MobileTexture; i < MAX_MobileTexture; ++i)
	{
		UTexture* Texture = MaterialInterface->GetMobileTexture(static_cast<EMobileTextureUnit>(i));
		if (Texture)
		{
			UTexture2D* Texture2D = Cast<UTexture2D>(Texture);	
			if (Texture2D && Texture2D->bIsCompositingSource)
			{
				LogWarningOnce(FString::Printf(TEXT("%s uses texture %s which is marked as a compositing source, and will not render in game."), *MaterialInterface->GetFullName(), *Texture->GetFullName()));
			}
		}
	}
}

/**
 * @return The name of the output cooked data directory - regardless of user mode, etc.
 */
FString UCookPackagesCommandlet::GetTrueCookedDirectory()
{
	FString CookedDir;
	appGetCookedContentPath(Platform, CookedDir);
	return CookedDir;
}

/**
 * @return The name of the output cooked data directory
 */
FString UCookPackagesCommandlet::GetCookedDirectory()
{
	FString CookedDir;
	FString PlatformName;

	// user mode cooking requires special handling
	if (DLCName.Len())
	{
		CookedDir = appGameDir();
		PlatformName = appPlatformTypeToString( Platform );
		switch (Platform)
		{
			// cooking unpublished data goes to published
			case PLATFORM_Windows:
			case PLATFORM_WindowsConsole:
			case PLATFORM_WindowsServer:
				CookedDir += FString::Printf(TEXT("Published\\Cooked%s\\"),*PlatformName);
				break;

			// Console go into DLC directory
			case PLATFORM_PS3:
			case PLATFORM_Xbox360:
			default:
				CookedDir += FString::Printf(TEXT("DLC\\%s\\%s\\Content\\%sGame\\Cooked%s\\"),*PlatformName,*DLCName,GGameName,*PlatformName);
				break;
		}
	}
	else
	{
		CookedDir = GetTrueCookedDirectory();
	}
	
	return CookedDir;
}

/**
 * @return The name of the android texture format
*/
const TCHAR* UCookPackagesCommandlet::GetAndroidTextureFormatName(ETextureFormatSupport TextureFormat)
{
	if (TextureFormat & TEXSUPPORT_DXT)
	{
		return TEXT("DXT");
	}
	if (TextureFormat & TEXSUPPORT_PVRTC)
	{
		return TEXT("PVRTC");
	}
	if (TextureFormat & TEXSUPPORT_ATITC)
	{
		return TEXT("ATITC");
	}
	if (TextureFormat & TEXSUPPORT_ETC)
	{
		return TEXT("ETC");
	}

	return TEXT("");
}

/**
 * @param The texture format to create the TFC file for
 */
void UCookPackagesCommandlet::CreateAndroidTFCFile(ETextureFormatSupport TextureFormat)
{
	FString BaseTFCStringAndroid = BaseTFCName.ToString() + TEXT("_") + GetAndroidTextureFormatName(TextureFormat);
	FName BaseTFCNameAndroid(*BaseTFCStringAndroid);

	FArchive* BaseTFC = GetTextureCacheArchive(BaseTFCNameAndroid);
	check(BaseTFC);
	if (bSeparateLightingTFC == TRUE)
	{
		FString LightingTFCStringAndroid = LightingTFCName.ToString() + TEXT("_") + GetAndroidTextureFormatName(TextureFormat);
		FName LightingTFCNameAndroid(*LightingTFCStringAndroid);

		FArchive* LightingTFC = GetTextureCacheArchive(LightingTFCNameAndroid);
		check(LightingTFC);
	}
	if (bSeparateCharacterTFC == TRUE)
	{
		FString CharacterTFCStringAndroid = CharacterTFCName.ToString() + TEXT("_") + GetAndroidTextureFormatName(TextureFormat);
		FName CharacterTFCNameAndroid(*CharacterTFCStringAndroid);

		FArchive* CharacterTFC = GetTextureCacheArchive(CharacterTFCNameAndroid);
		check(CharacterTFC);
	}
}

/**
 * @return TRUE if the destination platform expects pre-byteswapped data (packages, coalesced ini files, etc)
 */
UBOOL UCookPackagesCommandlet::ShouldByteSwapData()
{
	return Platform == PLATFORM_Xbox360 || Platform == PLATFORM_PS3 || Platform == PLATFORM_WiiU;
}

/**
 * @return The name of the bulk data container file to use
 */
FString UCookPackagesCommandlet::GetBulkDataContainerFilename()
{
	return TEXT("GlobalPersistentCookerData.upk");
}

/** Gets the name of the persistent shader data file. */
FString UCookPackagesCommandlet::GetShaderDataContainerFilename() const
{
	return TEXT("PersistentCookerShaderData.bin");
}

/**
 * @return The name of the guid cache file to use
 */
FString UCookPackagesCommandlet::GetGuidCacheFilename()
{
	FString Filename = TEXT("GuidCache");

	// append mod name if it exists so that it is differentiated fro the shipping guid cache
	if (DLCName.Len())
	{
		Filename += TEXT("_");
		Filename += DLCName;
	}

	if (Platform == PLATFORM_Windows || 
		Platform == PLATFORM_WindowsServer || 
		Platform == PLATFORM_WindowsConsole ||
		Platform == PLATFORM_MacOSX)
	{
		// tack on a PC usable extension
		Filename += TEXT(".");
		Filename += GSys->Extensions(0);
	}
	else
	{
		// use cooked extension
		Filename += TEXT(".xxx");
	}

	return Filename;
}

FString UCookPackagesCommandlet::GetTextureCacheName(UTexture* Texture)
{
	FString Filename;

	if ((bSeparateLightingTFC == TRUE) && 
		(Texture->IsA(ULightMapTexture2D::StaticClass()) || Texture->IsA(UShadowMapTexture2D::StaticClass())))
	{
		Filename = LightingTFCName.ToString();
	}
	else if ((bSeparateCharacterTFC == TRUE) &&
			 ((Texture->LODGroup == TEXTUREGROUP_Character) ||
			  (Texture->LODGroup == TEXTUREGROUP_CharacterNormalMap) ||
			  (Texture->LODGroup == TEXTUREGROUP_CharacterSpecular)))
	{
		// Character textures go in CharTextures.tfc
		Filename = CharacterTFCName.ToString();
	}
	else
	{
		// All other textures go in Textures.tfc
		Filename = BaseTFCName.ToString();
	}
	// Multithreaded cooks use this to allow child processes to use a variant of the TFC name to distinguish the master and final TFCs from temporary ones
	static UBOOL AppendageLoaded = FALSE;
	static FString Appendage;
	if (!AppendageLoaded)
	{
		AppendageLoaded = TRUE;
		Parse(appCmdLine(),TEXT("TFCSUFFIX="),Appendage);
	}
	Filename += Appendage;

	// append mod name if cooking a new texture for a mod
	// @todo pc: How to handle this for PC cooking, which has no mod names??! Also GuidCache!

	UBOOL bIsShippedTexture = FALSE;
	if (GShippingCookerData)
	{
		// look for any mips for the texture in the shipped texture info
		for (INT Mip = 0; Mip < 12 && !bIsShippedTexture; Mip++)
		{
			FString					BulkDataName = *FString::Printf(TEXT("MipLevel_%i"), Mip);
			FCookedBulkDataInfo*	BulkDataInfo = GShippingCookerData->GetBulkDataInfo(Texture, *BulkDataName);

			// if the texture was found in the shipped info, then we mark it as shipped
			if (BulkDataInfo)
			{
				bIsShippedTexture = TRUE;
			}
		}
	}

	// shipped textures point to the original, not the mod version
	if (!bIsShippedTexture && DLCName.Len())
	{
		Filename += TEXT("_");
		Filename += DLCName;
	}

	return Filename;
}

FArchive* UCookPackagesCommandlet::GetTextureCacheArchive(const FName& TextureCacheName)
{
	check( bUseTextureFileCache );

	// Check for an existing open archive for the specified texture file cache.
	FArchive* TextureCacheArchive = TextureCacheNameToArMap.FindRef(TextureCacheName);
	if(!TextureCacheArchive)
	{
		FString TextureCacheFilename = CookedDirForPerProcessData + TextureCacheName.ToString() + TEXT(".") + GSys->TextureFileCacheExtension;

		// Create texture cache file writer.
		TextureCacheArchive = GFileManager->CreateFileWriter( *TextureCacheFilename, FILEWRITE_Append );
		if (TextureCacheArchive == NULL)
		{
			appMsgf(AMT_OK, TEXT("Failed to open %s - it may be in use by another program or console.\nTry rebooting consoles or closing applications, then click OK"),*TextureCacheFilename);

			TextureCacheArchive = GFileManager->CreateFileWriter( *TextureCacheFilename, FILEWRITE_Append );
			if (TextureCacheArchive == NULL)
			{
				appMsgf(AMT_OK, TEXT("Failed to open %s during second attempt."),*TextureCacheFilename);
				return NULL;
			}
		}
		// Propagate byte swapping based on cooking platform.	
		TextureCacheArchive->SetByteSwapping( ShouldByteSwapData() );
		// Seek to the end of the file.
		INT CurrentFileSize = TextureCacheArchive->TotalSize();
		check( CurrentFileSize >= 0 );
		TextureCacheArchive->Seek( CurrentFileSize );

		// Add the newly opened texture cache file to our opened texture cache map.
		TextureCacheNameToArMap.Set(TextureCacheName,TextureCacheArchive);
		TextureCacheNameToFilenameMap.Set(TextureCacheName,TextureCacheFilename);
	}

	return TextureCacheArchive;
}

/**
 *	@return	UBOOL		TRUE if the texture cache entry was valid, FALSE if not.
 */
UBOOL UCookPackagesCommandlet::VerifyTextureFileCacheEntry()
{
	if (bUseTextureFileCache == FALSE)
	{
		// Don't fail, as technically the entry is valid (ie it's not used so who cares)
		return TRUE;
	}

	UBOOL bValid = TRUE;

	// Verify the character texture file cache...
	for (INT CharIndex = 0; CharIndex < CharTFCCheckData.Num(); CharIndex++)
	{
		FTextureFileCacheEntry& TFCEntry = CharTFCCheckData(CharIndex);

		// Open the file and verify the expected header...

		// Compare against all other entries in the cache...
		for (INT InnerIndex = CharIndex + 1; InnerIndex < CharTFCCheckData.Num(); InnerIndex++)
		{
			FTextureFileCacheEntry& InnerTFCEntry = CharTFCCheckData(InnerIndex);
			if (FTextureFileCacheEntry::EntriesOverlap(TFCEntry, InnerTFCEntry) == TRUE)
			{
				warnf(NAME_Warning, TEXT("**** Texture overlap found in Character TFC!"));
				warnf(NAME_Warning, TEXT("\tOffset = %10d, Size = %10d, Mip %d of %s"), 
					TFCEntry.OffsetInCache, TFCEntry.SizeInCache, TFCEntry.MipIndex, *(TFCEntry.TextureName));
				warnf(NAME_Warning, TEXT("\tOffset = %10d, Size = %10d, Mip %d of %s"), 
					InnerTFCEntry.OffsetInCache, InnerTFCEntry.SizeInCache, InnerTFCEntry.MipIndex, *(InnerTFCEntry.TextureName));

				bValid = FALSE;
			}
		}
	}

	// Verify the lighting texture file cache...
	for (INT LightingIndex = 0; LightingIndex < LightingTFCCheckData.Num(); LightingIndex++)
	{
		FTextureFileCacheEntry& TFCEntry = LightingTFCCheckData(LightingIndex);

		// Open the file and verify the expected header...

		// Compare against all other entries in the cache...
		for (INT InnerIndex = LightingIndex + 1; InnerIndex < LightingTFCCheckData.Num(); InnerIndex++)
		{
			FTextureFileCacheEntry& InnerTFCEntry = LightingTFCCheckData(InnerIndex);
			if (FTextureFileCacheEntry::EntriesOverlap(TFCEntry, InnerTFCEntry) == TRUE)
			{
				warnf(NAME_Warning, TEXT("**** Texture overlap found in Lighting TFC!"));
				warnf(NAME_Warning, TEXT("\tOffset = %10d, Size = %10d, Mip %d of %s"), 
					TFCEntry.OffsetInCache, TFCEntry.SizeInCache, TFCEntry.MipIndex, *(TFCEntry.TextureName));
				warnf(NAME_Warning, TEXT("\tOffset = %10d, Size = %10d, Mip %d of %s"), 
					InnerTFCEntry.OffsetInCache, InnerTFCEntry.SizeInCache, InnerTFCEntry.MipIndex, *(InnerTFCEntry.TextureName));

				bValid = FALSE;
			}
		}
	}

	// Verify the base TFC
	for (INT Index = 0; Index < TFCCheckData.Num(); Index++)
	{
		FTextureFileCacheEntry& TFCEntry = TFCCheckData(Index);

		// Open the file and verify the expected header...

		// Compare against all other entries in the cache...
		for (INT InnerIndex = Index + 1; InnerIndex < TFCCheckData.Num(); InnerIndex++)
		{
			FTextureFileCacheEntry& InnerTFCEntry = TFCCheckData(InnerIndex);
			if (FTextureFileCacheEntry::EntriesOverlap(TFCEntry, InnerTFCEntry) == TRUE)
			{
				warnf(NAME_Warning, TEXT("**** Texture overlap found in TFC!"));
				warnf(NAME_Warning, TEXT("\tOffset = %10d, Size = %10d, Mip %d of %s"), 
					TFCEntry.OffsetInCache, TFCEntry.SizeInCache, TFCEntry.MipIndex, *(TFCEntry.TextureName));
				warnf(NAME_Warning, TEXT("\tOffset = %10d, Size = %10d, Mip %d of %s"), 
					InnerTFCEntry.OffsetInCache, InnerTFCEntry.SizeInCache, InnerTFCEntry.MipIndex, *(InnerTFCEntry.TextureName));

				bValid = FALSE;
			}
		}
	}

	return bValid;
}

/**
 *	Verifies that the data in the texture file cache for the given texture
 *	is a valid 'header' packet...
 *
 *	@param	Package		The package the texture was cooked into.
 *	@param	Texture2D	The texture that was cooked.
 *	@param	bIsSaved...	If TRUE, the texture was saved in a seekfree pacakge
 *
 *	@return	UBOOL		TRUE if the texture cache entry was valid, FALSE if not.
 */
UBOOL UCookPackagesCommandlet::AddVerificationTextureFileCacheEntry(UPackage* Package, UTexture2D* Texture2D, UBOOL bIsSavedInSeekFreePackage)
{
	if (bUseTextureFileCache == FALSE)
	{
		// Don't fail, as technically the entry is valid (ie it's not used so who cares)
		return TRUE;
	}
	if (Texture2D->TextureFileCacheName == NAME_None)
	{
		return TRUE;
	}

	// look for any mips for the texture in the shipped texture info
	//@SAS. Why 12? Shouldn't this be a define somewhere??
	for (INT Mip = 0; Mip < 12; Mip++)
	{
		FString					BulkDataName = *FString::Printf(TEXT("MipLevel_%i"), Mip);
		FCookedBulkDataInfo*	BulkDataInfo = PersistentCookerData->GetBulkDataInfo(Texture2D, *BulkDataName);

		// if the texture was found in the shipped info, then we mark it as shipped
		if (BulkDataInfo)
		{
			if ((BulkDataInfo->SavedBulkDataFlags & BULKDATA_StoreInSeparateFile) != 0)
			{
				FTextureFileCacheEntry* TFCEntry = TFCVerificationData.Find(BulkDataName);
				if (TFCEntry == NULL)
				{
					FTextureFileCacheEntry TempEntry;
					TFCVerificationData.Set(Texture2D->GetPathName(), TempEntry);
					TFCEntry = TFCVerificationData.Find(Texture2D->GetPathName());

					TFCEntry->TextureName = Texture2D->GetPathName();
					TFCEntry->MipIndex = Mip;
					TFCEntry->OffsetInCache = BulkDataInfo->SavedBulkDataOffsetInFile;
					TFCEntry->SizeInCache = BulkDataInfo->SavedBulkDataSizeOnDisk;
					TFCEntry->FlagsInCache = BulkDataInfo->SavedBulkDataFlags;
					TFCEntry->ElementCountInCache = BulkDataInfo->SavedElementCount;

					if (Texture2D->TextureFileCacheName.ToString().StartsWith(CharacterTFCName.ToString()))
					{
						TFCEntry->TFCType = TFCT_Character;
					}
					else if (Texture2D->TextureFileCacheName.ToString().StartsWith(LightingTFCName.ToString()))
					{
						TFCEntry->TFCType = TFCT_Lighting;
					}
					else
					{
						TFCEntry->TFCType = TFCT_Base;
					}

#if 0 // Define this to 1 to log out each texture added to the TFC when running in VerifyTFC...
					warnf(NAME_Warning, TEXT("Added to %8s TFC: Offset = %10d, Size = %10d, Mip = %2d, %s"),
						(TFCEntry->TFCType == TFCT_Character) ? TEXT("CHAR") : 
						    (TFCEntry->TFCType == TFCT_Lighting) ? TEXT("LIGHT") : TEXT("BASE"),
						TFCEntry->OffsetInCache, TFCEntry->SizeInCache,
						TFCEntry->MipIndex, *(TFCEntry->TextureName));
#endif

					FTextureFileCacheEntry* OverlapEntry = FindTFCEntryOverlap(*TFCEntry);
					if (OverlapEntry)
					{
						warnf(NAME_Warning, TEXT("AddVerificationTextureFileCacheEntry> Texture overlap found in TFC!"));
						warnf(NAME_Warning, TEXT("\tMip %d of %s"), TFCEntry->MipIndex, *(TFCEntry->TextureName));
						warnf(NAME_Warning, TEXT("\t\tOffset = %10d, Size = %d"), TFCEntry->OffsetInCache, TFCEntry->SizeInCache);
						warnf(NAME_Warning, TEXT("\tMip %d of %s"), OverlapEntry->MipIndex, *(OverlapEntry->TextureName));
						warnf(NAME_Warning, TEXT("\t\tOffset = %10d, Size = %d"), OverlapEntry->OffsetInCache, OverlapEntry->SizeInCache);
					}

					if (TFCEntry->TFCType == TFCT_Character)
					{
						FTextureFileCacheEntry* CheckData = new(CharTFCCheckData)FTextureFileCacheEntry(*TFCEntry);
					}
					else if (TFCEntry->TFCType == TFCT_Lighting)
					{
						FTextureFileCacheEntry* CheckData = new(LightingTFCCheckData)FTextureFileCacheEntry(*TFCEntry);
					}
					else
					{
						FTextureFileCacheEntry* CheckData = new(TFCCheckData)FTextureFileCacheEntry(*TFCEntry);
					}
				}
				else
				{
					if ((TFCEntry->TextureName != Texture2D->GetPathName()) ||
						(TFCEntry->MipIndex != Mip) ||
						(TFCEntry->OffsetInCache != BulkDataInfo->SavedBulkDataOffsetInFile) ||
						(TFCEntry->SizeInCache != BulkDataInfo->SavedBulkDataSizeOnDisk) ||
						(TFCEntry->FlagsInCache != BulkDataInfo->SavedBulkDataFlags) ||
						(TFCEntry->ElementCountInCache != BulkDataInfo->SavedElementCount))
					{
						warnf(NAME_Warning, TEXT("DATA MISMATCH on texture already in file cache!"));
						warnf(NAME_Warning, TEXT("\tTextureName:	      EXPECTED %s"), *(Texture2D->GetPathName()));
						warnf(NAME_Warning, TEXT("\t                      FOUND    %s"), *(TFCEntry->TextureName));
						warnf(NAME_Warning, TEXT("\tMipIndex:             EXPECTED %10d, FOUND %10d"), Mip, TFCEntry->MipIndex);
						warnf(NAME_Warning, TEXT("\tOffsetInCache:        EXPECTED %10d, FOUND %10d"), TFCEntry->OffsetInCache, BulkDataInfo->SavedBulkDataOffsetInFile);
						warnf(NAME_Warning, TEXT("\tSizeInCache:          EXPECTED %10d, FOUND %10d"), BulkDataInfo->SavedBulkDataSizeOnDisk, TFCEntry->SizeInCache);
						warnf(NAME_Warning, TEXT("\tFlagsInCache:         EXPECTED 0x%08x, FOUND 0x%08x"), BulkDataInfo->SavedBulkDataFlags, TFCEntry->FlagsInCache);
						warnf(NAME_Warning, TEXT("\tElementCountInCache:  EXPECTED %10d, FOUND %10d"), BulkDataInfo->SavedElementCount, TFCEntry->ElementCountInCache);
					}
				}
			}
		}
	}

	return TRUE;
}

FTextureFileCacheEntry* UCookPackagesCommandlet::FindTFCEntryOverlap(FTextureFileCacheEntry& InEntry)
{
	if (InEntry.TFCType == TFCT_Character)
	{
		for (INT CharCheckIndex = 0; CharCheckIndex < CharTFCCheckData.Num(); CharCheckIndex++)
		{
			FTextureFileCacheEntry& CheckEntry = CharTFCCheckData(CharCheckIndex);
			if (FTextureFileCacheEntry::EntriesOverlap(InEntry, CheckEntry))
			{
				return &CheckEntry;
			}
		}
	}
	else if (InEntry.TFCType == TFCT_Lighting)
	{
		for (INT LightingCheckIndex = 0; LightingCheckIndex < LightingTFCCheckData.Num(); LightingCheckIndex++)
		{
			FTextureFileCacheEntry& CheckEntry = LightingTFCCheckData(LightingCheckIndex);
			if (FTextureFileCacheEntry::EntriesOverlap(InEntry, CheckEntry))
			{
				return &CheckEntry;
			}
		}
	}
	else
	{
		for (INT CheckIndex = 0; CheckIndex < TFCCheckData.Num(); CheckIndex++)
		{
			FTextureFileCacheEntry& CheckEntry = TFCCheckData(CheckIndex);
			if (FTextureFileCacheEntry::EntriesOverlap(InEntry, CheckEntry))
			{
				return &CheckEntry;
			}
		}
	}

	return NULL;
}


/** @return UBOOL  Whether or not this texture is a streaming texture and should be set as NeverStream. **/
UBOOL UCookPackagesCommandlet::ShouldBeANeverStreamTexture( UTexture2D* Texture2D, UBOOL bIsSavedInSeekFreePackage ) const
{
	UBOOL Retval;

	// Whether the texture is applied to the face of a cubemap, in which case we can't stream it.
	const UBOOL bIsCubemapFace = Texture2D->GetOuter()->IsA(UTextureCube::StaticClass());

	if ((Platform & PLATFORM_Mobile) && !bIsCubemapFace)
	{
		// on mobile, all textures are "streamed" in that they live in a .tfc - this is to minimize download sizes by not
		// sharing any textures, and seeking isn't important on a flash memory device
		return FALSE;
	}

	// Check if it's a lightmap/shadowmap that should be streamed from the TFC.
	ULightMapTexture2D* LightmapTexture = Cast<ULightMapTexture2D>( Texture2D );
	UShadowMapTexture2D* ShadowmapTexture = Cast<UShadowMapTexture2D>( Texture2D );

	const UBOOL bIsNonStreamingLightmap = LightmapTexture && !(LightmapTexture->LightmapFlags & LMF_Streamed);
	const UBOOL bIsNonStreamingShadowmap = ShadowmapTexture && !(ShadowmapTexture->ShadowmapFlags & SMF_Streamed);
	const UBOOL bIsMobileFlattenedTexture = Texture2D->LODGroup == TEXTUREGROUP_MobileFlattened;
	const UBOOL bIsInSeekFreeWithoutTFC = bIsSavedInSeekFreePackage && !bUseTextureFileCache;

#if WITH_SUBSTANCE_AIR
	USubstanceAirTexture2D* SubstanceTexture = Cast<USubstanceAirTexture2D>( Texture2D );

	const UBOOL bIsSubstanceInstallTime = SubstanceTexture && 
		(GUseSubstanceInstallTimeCache || SubstanceTexture->OutputCopy && 
			SubstanceTexture->OutputCopy->GetParentGraphInstance() && 
			SubstanceTexture->OutputCopy->GetParentGraphInstance()->bIsBaked);
#endif

	// Textures residing in seekfree packages cannot be streamed as the file offset is not known till after
	// they are saved. Exception: If we're using a TFC, allow lightmaps to be streamed.
	Retval = bIsNonStreamingLightmap || bIsNonStreamingShadowmap || bIsInSeekFreeWithoutTFC || bIsCubemapFace || bIsMobileFlattenedTexture 
#if WITH_SUBSTANCE_AIR		
		|| bIsSubstanceInstallTime;
#else
		;
#endif

	return Retval;
}

/**
 *  Make sure data is (and remains) loaded for (and after) saving.
 *
 * @param	Texture2D					The texture to operate on
 * @param	Mips						The Mips array to operate on (should differ for each compressed texture format)
 * @param	FirstMipIndex				The first mip to operate on
 * @param	bIsSavedInSeekFreePackage	Whether or not the package is seek free
 * @param	bIsStreamingTexture			Whether or not the texture is streaming
 */
void UCookPackagesCommandlet::EnsureMipsLoaded (UTexture2D* Texture2D, TIndirectArray<FTexture2DMipMap>& Mips, INT FirstMipIndex, UBOOL bIsSavedInSeekFreePackage, UBOOL bIsStreamingTexture)
{
	for( INT MipIndex=FirstMipIndex; MipIndex<Mips.Num(); MipIndex++ )
	{
		FTexture2DMipMap& Mip = Mips(MipIndex);

		UBOOL bIsStoredInSeparateFile	= FALSE;

		// We still require MinStreamedInMips to be present in the seek free package as the streaming code 
		// assumes those to be always loaded.
		if( bIsSavedInSeekFreePackage
			&&	bIsStreamingTexture
			// @todo streaming, @todo cooking: This assumes the same value of GMinTextureResidentMipCount across platforms.
			&&	(MipIndex < Mips.Num() - GMinTextureResidentMipCount)
			// Miptail must always be loaded.
			&&	MipIndex < Texture2D->MipTailBaseIdx )
		{
			bIsStoredInSeparateFile = TRUE;
		}

		if (!bIsStoredInSeparateFile)
		{
			Mip.Data.Lock(LOCK_READ_WRITE);
			Mip.Data.Unlock();
		}
	}
}

/**
 * Helper function used by CookTexture - strips mips from UI Textures 
 */
void UCookPackagesCommandlet::StripUIMips(UTexture2D* Texture2D, TIndirectArray<FTexture2DMipMap> &Mips, INT FirstMipIndex)
{
	// remove any mips that are bigger than the desired biggest size
	if (FirstMipIndex > 0)
	{
		Mips.Remove(0, FirstMipIndex);
		Texture2D->SizeX = Max(Texture2D->SizeX >> FirstMipIndex,1);
		Texture2D->SizeY = Max(Texture2D->SizeY >> FirstMipIndex,1);
		Texture2D->LODBias = 0;
		FirstMipIndex = 0;
	}

	// remove any remaining lower mips
	if( Mips.Num() > 1 )
	{
		Mips.Remove( 1, Mips.Num() - 1 );
	}
}

/**
 *	Get the list of packages that will always be loaded
 *
 *	@return	UBOOL		TRUE if successful, FALSE if failed
 */
UBOOL UCookPackagesCommandlet::GetAlwaysLoadedPackageList(TArray<FString>& OutAlwaysLoaded, TMap<FString,TArray<FString> >& OutAlwaysLoadedLoc)
{
	OutAlwaysLoaded.Empty();
	OutAlwaysLoadedLoc.Empty();

	// Assemble array of native script packages.
	appGetScriptPackageNames(OutAlwaysLoaded, SPT_Native);

	if (GCookingTarget & PLATFORM_Stripped)
	{
		OutAlwaysLoaded.AddItem(TEXT("Startup"));
	}
	else
	{
 		// startup packages from .ini
	 	GetNonNativeStartupPackageNames(OutAlwaysLoaded, PlatformEngineConfigFilename, TRUE);
	}

	// Get a list of known language extensions
	const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();
	// go through and add localized versions of each package for all known languages
	// since they could be used at runtime depending on the language at runtime
	INT NumPackages = OutAlwaysLoaded.Num();
	// add the packagename with _XXX language extension
	for (INT LangIndex = 0; LangIndex < KnownLanguageExtensions.Num(); LangIndex++)
	{
		TArray<FString>* LocdList = OutAlwaysLoadedLoc.Find(KnownLanguageExtensions(LangIndex));
		if (LocdList == NULL)
		{
			TArray<FString> TempLocdList;
			OutAlwaysLoadedLoc.Set(KnownLanguageExtensions(LangIndex), TempLocdList);
			LocdList = OutAlwaysLoadedLoc.Find(KnownLanguageExtensions(LangIndex));
		}
		check(LocdList);
		for (INT PackageIndex = 0; PackageIndex < NumPackages; PackageIndex++)
		{
			FString PkgName = OutAlwaysLoaded(PackageIndex);
			LocdList->AddItem(*(PkgName + LOCALIZED_SEEKFREE_SUFFIX + TEXT("_") + KnownLanguageExtensions(LangIndex)));
		}
	}

	return TRUE;
}

/**
 *	Mark known cooked startup objects
 */
void UCookPackagesCommandlet::MarkCookedStartupObjects(UBOOL bClear)
{
	DOUBLE MarkStartTime = appSeconds();
	if (bSkipStartupObjectDetermination == FALSE)
	{
		EObjectFlags ObjClearFlags = 0;
		if (bClear == TRUE)
		{
			// Clear *all* marked objects...
			debugfSlow(TEXT("Clearing *all* marked objects..."));
			ObjClearFlags = RF_MarkedByCooker|RF_ForceTagExp;
		}

		// Mark all AlwaysLoad objects as cooked
		debugfSlow(TEXT("--- Marking startup cooked objects"));
		TArray<UObject*> StartupPackageObjects;
		for (FObjectIterator It; It; ++It)
		{
			EObjectFlags LocalObjClearFlags = ObjClearFlags;
			UObject* Object = *It;
			if (Object->HasAnyFlags(RF_MarkedByCookerTemp))
			{
				LocalObjClearFlags |= RF_Transient|RF_MarkedByCookerTemp;
			}
			Object->ClearFlags(LocalObjClearFlags);
			// mark any objects that are in startup packages as cooked...
			if (Object->HasAnyFlags(RF_CookedStartupObject))
			{
				Object->SetFlags(RF_MarkedByCooker);
			}
			else if (TrackedStartupPackageObjects.Find(Object->GetFullName()))
			{
				// This is a STARTUP package object that has been loaded...
				StartupPackageObjects.AddItem(Object);
			}
		}

		if (StartupPackageObjects.Num() > 0)
		{
			warnf(NAME_Log, TEXT("Marked %d STARTUP package objects..."), StartupPackageObjects.Num());
			for (INT StartupPkgObjIdx = 0; StartupPkgObjIdx< StartupPackageObjects.Num(); StartupPkgObjIdx++)
			{
				UObject* StartupPkgObj = StartupPackageObjects(StartupPkgObjIdx);
				debugfSlow(TEXT("CookObject       being called on startup object %s"), *(StartupPkgObj->GetFullName()));
				CookObject(NULL, StartupPkgObj, TRUE);
				debugfSlow(TEXT("PrepareForSaving being called on startup object %s"), *(StartupPkgObj->GetFullName()));
				PrepareForSaving(NULL, StartupPkgObj, TRUE, FALSE);
				StartupPkgObj->SetFlags(RF_CookedStartupObject);
				StartupPkgObj->SetFlags(RF_MarkedByCooker);
				// 'Cook-out' material expressions on consoles
				// In this case, simply don't load them on the client
				// Keep 'parameter' types around so that we can get their defaults.
				if ((GCookingTarget & UE3::PLATFORM_Stripped) != 0)
				{
					UMaterialExpression* MatExp = Cast<UMaterialExpression>(StartupPkgObj);
					if (MatExp)
					{
						if ((MatExp->IsTemplate() == FALSE) && (MatExp->bIsParameterExpression == FALSE))
						{
							if (bCleanupMaterials == TRUE)
							{
								MatExp->ClearFlags(RF_ForceTagExp);
								MatExp->ClearFlags(RF_TagExp);
								MatExp->SetFlags(RF_MarkedByCooker|RF_Transient|RF_MarkedByCookerTemp);
							}
							else
							{
								// The old behavior... which didn't really work correctly.
								// It also didn't prevent unreferenced textures (ones in unhooked samplers) 
								// from being placed into the texture file caches...
								debugfSuppressed(NAME_DevCooking, TEXT("Cooking out material expression %s (%s)"), 
									*(MatExp->GetName()), MatExp->GetOuter() ? *(MatExp->GetOuter()->GetName()) : TEXT("????"));
								MatExp->SetFlags(RF_NotForClient|RF_NotForServer|RF_NotForEdit);
								MatExp->ClearFlags(RF_ForceTagExp);
								MatExp->ClearFlags(RF_TagExp);
								MatExp->SetFlags(RF_MarkedByCooker|RF_Transient|RF_MarkedByCookerTemp);
							}
						}
					}
				}
			}
		}
	}

	CookStats.MarkCookedStartupTime += appSeconds() - MarkStartTime;
}

/**
 *	Tag known cooked startup objects
 *	Called once at the start of the second processing pass
 */
void UCookPackagesCommandlet::TagCookedStartupObjects()
{
	DOUBLE TagStartTime = appSeconds();

	if (bSkipStartupObjectDetermination == FALSE)
	{
		// Use the 'shipped' cooked persistent cooker data to gather the list 
		// of objects that were cooked into startup (always loaded) packages.
		UPersistentCookerData* PCD = GShippingCookerData;
		if (PCD == NULL)
		{
			// We must not be cooking DLC...
			// Use the current PCD
			warnf(NAME_Log, TEXT("TagCookedStartupObjects> Using current PersistentCookerData..."));
			PCD = PersistentCookerData;
		}
		else
		{
			warnf(NAME_Log, TEXT("TagCookedStartupObjects> Using Shipped PersistentCookerData - this must be DLC..."));
		}
		checkf(PCD, TEXT("TagCookedStartupObjects> PersistentCookerData not found??"));

		// Insert all cooked objects into the TrackedCookedStartupObjects list
		for (TMap<FString,TMap<FString,FString> >::TIterator StartupPkgIt(PCD->CookedStartupObjects); StartupPkgIt; ++StartupPkgIt)
		{
			FString& StartupPkgName = StartupPkgIt.Key();
			TMap<FString,FString>& StartupObjMap = StartupPkgIt.Value();
			UBOOL bStartup = (StartupPkgName.ToUpper() == TEXT("STARTUP"));
			warnf(NAME_Log, TEXT("Tagging %d always loaded objects for %s"), StartupObjMap.Num(), *StartupPkgName);
			for (TMap<FString,FString>::TIterator StartupObjIt(StartupObjMap); StartupObjIt; ++StartupObjIt)
			{
				FString& StartupObjFullName = StartupObjIt.Key();
				FString& StartupObjClassName = StartupObjIt.Value();
				TrackedCookedStartupObjects.Set(StartupObjFullName, TRUE);
				if (bStartup == TRUE)
				{
					// We need to track the objects in the STARTUP package as they are not 
					// likely to be loaded in child processes...
					debugfSlow(TEXT("Adding object to STARTUP cooked objects: %s"), *StartupObjFullName);
					TrackedStartupPackageObjects.Set(StartupObjFullName, TRUE);
				}
			}
		}

		// Insert all cooked objects into the TrackedCookedStartupObjects list
		for (TMap<FString,TMap<FString,TMap<FString,FString> > >::TIterator StartupLocPkgIt(PCD->CookedStartupObjectsLoc); StartupLocPkgIt; ++StartupLocPkgIt)
		{
			FString& LanguageExtName = StartupLocPkgIt.Key();
			TMap<FString,TMap<FString,FString> >& StartupLocPkgMap = StartupLocPkgIt.Value();
			warnf(NAME_Log, TEXT("Tagging always loaded objects for language %s"), *LanguageExtName);
			//@todo. Need to test multilanguage cooking...
			for (TMap<FString,TMap<FString,FString> >::TIterator StartupPkgIt(StartupLocPkgMap); StartupPkgIt; ++StartupPkgIt)
			{
				FString& StartupPkgName = StartupPkgIt.Key();
				TMap<FString,FString>& StartupObjMap = StartupPkgIt.Value();
				UBOOL bStartup = (StartupPkgName.ToUpper() == TEXT("STARTUP"));
				warnf(NAME_Log, TEXT("\tTagging %d localized always loaded objects for %s"), StartupObjMap.Num(), *StartupPkgName);
				for (TMap<FString,FString>::TIterator StartupObjIt(StartupObjMap); StartupObjIt; ++StartupObjIt)
				{
					FString& StartupObjFullName = StartupObjIt.Key();
					FString& StartupObjClassName = StartupObjIt.Value();
					TrackedCookedStartupObjects.Set(StartupObjFullName, TRUE);
					if (bStartup == TRUE)
					{
						// We need to track the objects in the STARTUP package as they are not 
						// likely to be loaded in child processes...
						debugfSlow(TEXT("Adding object to STARTUP cooked objects: %s"), *StartupObjFullName);
						TrackedStartupPackageObjects.Set(StartupObjFullName, TRUE);
					}
				}
			}
		}

		// Fill in the AlreadyHandled material lists...
		AlreadyHandledMaterials.Empty();
		warnf(NAME_Log, TEXT("Tagging %d already handled materials"), PCD->AlreadyHandledStartupMaterials.Num());
		AlreadyHandledMaterials = PCD->AlreadyHandledStartupMaterials;

		AlreadyHandledMaterialInstances.Empty();
		warnf(NAME_Log, TEXT("Tagging %d already handled material instances"), PCD->AlreadyHandledStartupMaterialInstances.Num());
		AlreadyHandledMaterialInstances = PCD->AlreadyHandledStartupMaterialInstances;

		if (GGameCookerHelper)
		{
			GGameCookerHelper->TagCookedStartupObjects(this);
		}

		debugfSlow(TEXT("--- Tagging startup cooked objects"));
		TArray<UObject*> UntaggedStartupObjects;
		for (FObjectIterator It; It; ++It)
		{
			UObject* Object = *It;
			FString ObjFullName = Object->GetFullName();
			if ((TrackedCookedStartupObjects.Find(ObjFullName) != NULL) ||
				(TrackedInitialMarkedObjects.Find(ObjFullName) != NULL))
			{
				if (Object->HasAnyFlags(RF_CookedStartupObject) == FALSE)
				{
					// This is a alwaysloaded package object that has not been tagged already.
					// This means this is an iterative cook or we are a child, and so we must cook/prep the object.
					UntaggedStartupObjects.AddItem(Object);
				}
				Object->SetFlags(RF_CookedStartupObject);
				if (Object->HasAnyFlags(RF_MarkedByCookerTemp))
				{
					Object->ClearFlags(RF_Transient|RF_MarkedByCookerTemp);
				}
			}
		}

		if (UntaggedStartupObjects.Num() > 0)
		{
			warnf(NAME_Log, TEXT("Marked %d untagged startup objects..."), UntaggedStartupObjects.Num());
			for (INT PkgObjIdx = 0; PkgObjIdx< UntaggedStartupObjects.Num(); PkgObjIdx++)
			{
				UObject* PkgObj = UntaggedStartupObjects(PkgObjIdx);
				debugfSlow(TEXT("CookObject       being called on untagged startup object %s"), *(PkgObj->GetFullName()));
				CookObject(NULL, PkgObj, TRUE);
				debugfSlow(TEXT("PrepareForSaving being called on untagged startup object %s"), *(PkgObj->GetFullName()));
				PrepareForSaving(NULL, PkgObj, TRUE, FALSE);
				PkgObj->SetFlags(RF_CookedStartupObject);
				PkgObj->SetFlags(RF_MarkedByCooker);
				// 'Cook-out' material expressions on consoles
				// In this case, simply don't load them on the client
				// Keep 'parameter' types around so that we can get their defaults.
				if ((GCookingTarget & UE3::PLATFORM_Stripped) != 0)
				{
					UMaterialExpression* MatExp = Cast<UMaterialExpression>(PkgObj);
					if (MatExp)
					{
						if ((MatExp->IsTemplate() == FALSE) && (MatExp->bIsParameterExpression == FALSE))
						{
							if (bCleanupMaterials == TRUE)
							{
								MatExp->ClearFlags(RF_ForceTagExp);
								MatExp->ClearFlags(RF_TagExp);
								MatExp->SetFlags(RF_MarkedByCooker|RF_Transient|RF_MarkedByCookerTemp);
							}
							else
							{
								// The old behavior... which didn't really work correctly.
								// It also didn't prevent unreferenced textures (ones in unhooked samplers) 
								// from being placed into the texture file caches...
								debugfSuppressed(NAME_DevCooking, TEXT("Cooking out material expression %s (%s)"), 
									*(MatExp->GetName()), MatExp->GetOuter() ? *(MatExp->GetOuter()->GetName()) : TEXT("????"));
								MatExp->SetFlags(RF_NotForClient|RF_NotForServer|RF_NotForEdit);
								MatExp->ClearFlags(RF_ForceTagExp);
								MatExp->ClearFlags(RF_TagExp);
								MatExp->SetFlags(RF_MarkedByCooker|RF_Transient|RF_MarkedByCookerTemp);
							}
						}
					}
				}
			}
		}
	}

	CookStats.TagCookedStartupTime += appSeconds() - TagStartTime;
}

/** 
 *	Set Current Persistent Map Cache
 *
 *	@param	PersistentMapName	: Set Current Persistent Map Name to process 
 *	@param	FaceFXArray			: FaceFXArray for current persistent map
 *	@param	GroupAnimLookupMap	: GroupAnimLookupMap for current persistent map
 *	return TRUE if succeed 
 */
void FPersistentFaceFXAnimSetGenerator::SetCurrentPersistentMapCache(const FString& PersistentMapName, const FString& AliasedMapName)
{
	CurrentPersistentMapName = PersistentMapName;
	AliasedPersistentMapName = AliasedMapName;
	CurrentPersistentMapFaceFXArray = PersistentMapFaceFXArray.Find(CurrentPersistentMapName);
	CurrentPMapGroupAnimLookupMap = PersistentMapToGroupAnimLookupMap.Find(CurrentPersistentMapName);
}

/** Setup scripted referenced animset */
void FPersistentFaceFXAnimSetGenerator::SetupScriptReferencedFaceFXAnimSets()
{
	const TCHAR* SafeFaceFXIniSection = TEXT("Cooker.SafeFaceFXAnimSets");
	FConfigSection* IniSafeFaceFXList = GConfig->GetSectionPrivate(SafeFaceFXIniSection, FALSE, TRUE, GEditorIni);
	if (IniSafeFaceFXList)
	{
		for (FConfigSectionMap::TConstIterator It(*IniSafeFaceFXList) ; It ; ++It)
		{
			const FString& SafeName = It.Value();
			ScriptReferencedFaceFXAnimSets.Set(SafeName, 1);
		}
	}
}

/** Set Caller class information */
void FPersistentFaceFXAnimSetGenerator::SetCallerInfo( UCommandlet * Commandlet, _GarbageCollectFn GCFn)
{
	CallerCommandlet = Commandlet;

	if (GCFn!=NULL)
	{
		GarbageCollectFn = GCFn;
	}
	else
	{
		GarbageCollectFn = DefaultGarbageCollectFn;
	}
}

/** Helper function to mangle the given FaceFX names for the persistent level anim set */
void FPersistentFaceFXAnimSetGenerator::GenerateMangledFaceFXInformation( FString& PersistentLevelName, FMangledFaceFXInfo& PFFXData )
{
#if WITH_FACEFX
	// This code assume the PFFXData.FaceFXAnimSet, PFFXData.OriginalFaceFXGroupName 
	// and PFFXData.OriginalFaceFXAnimName are set correctly!
	FString ConcatanatedGroupName = FString::Printf(TEXT("%s.%s"), *(PFFXData.FaceFXAnimSet), *(PFFXData.OriginalFaceFXGroupName));
	FString* MangledNamePrefix = GroupNameToMangledMap.Find(ConcatanatedGroupName);
	if (MangledNamePrefix == NULL)
	{
		FString NewMangledName = FString::Printf(TEXT("%s$"), *(PFFXData.FaceFXAnimSet));
		GroupNameToMangledMap.Set(ConcatanatedGroupName, NewMangledName);
		MangledNamePrefix = GroupNameToMangledMap.Find(ConcatanatedGroupName);
		check(MangledNamePrefix);
		//debugf(TEXT("\tAdded mangled group name %5s (%s)"), *NewMangledName, *ConcatanatedGroupName);
	}
	
	PFFXData.MangledFaceFXGroupName = PersistentLevelName;
	PFFXData.MangledFaceFXAnimName = *MangledNamePrefix + PFFXData.OriginalFaceFXAnimName;
#endif //#if WITH_FACEFX
}

/** Find the GroupName/AnimName from FaceFXAnimSets. If found, return the Index */
INT FPersistentFaceFXAnimSetGenerator::FindSourceAnimSet( TArrayNoInit<class UFaceFXAnimSet*>& FaceFXAnimSets, FString& GroupName, FString& AnimName )
{
#if WITH_FACEFX
	for (INT SetIndex = 0; SetIndex < FaceFXAnimSets.Num(); SetIndex++)
	{
		UFaceFXAnimSet* FFXAnimSet = FaceFXAnimSets(SetIndex);
		if (FFXAnimSet)
		{
			FString SeqNameToCheck = GroupName + TEXT(".") + AnimName;
			TArray<FString> SeqNames;
			FFXAnimSet->GetSequenceNames(SeqNames);
			for (INT SeqIndex = 0; SeqIndex < SeqNames.Num(); SeqIndex++)
			{
				if (SeqNameToCheck == SeqNames(SeqIndex))
				{
					return SetIndex;
				}
			}
		}
	}
#endif //#if WITH_FACEFX
	return -1;
}

/** Find MangledFaceFXInfo of input of AnimSetRefName, GroupName, AnimName from GroupAnimToFFXMap */
FPersistentFaceFXAnimSetGenerator::FMangledFaceFXInfo* FPersistentFaceFXAnimSetGenerator::FindMangledFaceFXInfo( TGroupAnimToMangledFaceFXMap* GroupAnimToFFXMap, FString& AnimSetRefName, FString& GroupName, FString& AnimName )
{
#if WITH_FACEFX
	FMangledFaceFXInfo* MangledFFXInfo = NULL;

	FString GroupAnimName = GroupName + TEXT(".") + AnimName;
	GroupAnimName = GroupAnimName.ToUpper();
	TMangledFaceFXMap* MangledFaceFXMap = GroupAnimToFFXMap->Find(GroupAnimName);
	if (MangledFaceFXMap)
	{
		MangledFFXInfo = MangledFaceFXMap->Find(AnimSetRefName);
	}

	return MangledFFXInfo;
#else
	return NULL;
#endif //#if WITH_FACEFX
}

FPersistentFaceFXAnimSetGenerator::TMangledFaceFXMap* FPersistentFaceFXAnimSetGenerator::GetMangledFaceFXMap( TGroupAnimToMangledFaceFXMap* GroupAnimToFFXMap, FString& GroupName, FString& AnimName, UBOOL bCreateIfNotFound )
{
#if WITH_FACEFX
	FString GroupAnimName = GroupName + TEXT(".") + AnimName;
	GroupAnimName = GroupAnimName.ToUpper();
	FPersistentFaceFXAnimSetGenerator::TMangledFaceFXMap* MangledFFXMap = GroupAnimToFFXMap->Find(GroupAnimName);
	if ((MangledFFXMap == NULL) && bCreateIfNotFound)
	{
		// Doesn't exist yet...
		FPersistentFaceFXAnimSetGenerator::TMangledFaceFXMap TempMangled;
		GroupAnimToFFXMap->Set(GroupAnimName, TempMangled);
		MangledFFXMap = GroupAnimToFFXMap->Find(GroupAnimName);
	}
	check(bCreateIfNotFound ? (MangledFFXMap != NULL) : 1);
	return MangledFFXMap;
#else
	return NULL;
#endif //#if WITH_FACEFX
}

FPersistentFaceFXAnimSetGenerator::FMangledFaceFXInfo* FPersistentFaceFXAnimSetGenerator::GetMangledFaceFXMap(
	TMangledFaceFXMap* MangledFFXMap,
	FString& PersistentLevelName, FString& AnimSetRefName, FString& GroupName, FString& AnimName, UBOOL bCreateIfNotFound)
{
#if WITH_FACEFX
	FMangledFaceFXInfo* MangledFFXInfo = MangledFFXMap->Find(AnimSetRefName);
	if (bCreateIfNotFound)
	{
		if (MangledFFXInfo == NULL)
		{
			FMangledFaceFXInfo TempMangledFFX;
			MangledFFXMap->Set(AnimSetRefName, TempMangledFFX);
			MangledFFXInfo = MangledFFXMap->Find(AnimSetRefName);
			check(MangledFFXInfo);

			MangledFFXInfo->FaceFXAnimSet = AnimSetRefName;
			MangledFFXInfo->OriginalFaceFXGroupName = GroupName;
			MangledFFXInfo->OriginalFaceFXAnimName = AnimName;

			GenerateMangledFaceFXInformation(PersistentLevelName, *MangledFFXInfo);
		}
		else
		{
			if ((MangledFFXInfo->FaceFXAnimSet != AnimSetRefName) ||
				(MangledFFXInfo->OriginalFaceFXGroupName != GroupName) ||
				(MangledFFXInfo->OriginalFaceFXAnimName != AnimName))
			{
				warnf(NAME_Log, TEXT("ERROR: Persistent FaceFX Data mismatch on %s: Stored %s %s, On Cue %s %s)"), 
					*MangledFFXInfo->OriginalFaceFXGroupName, *MangledFFXInfo->OriginalFaceFXAnimName,
					*GroupName, *AnimName);
			}
		}
	}
	check(bCreateIfNotFound ? (MangledFFXInfo != NULL) : 1);
	return MangledFFXInfo;
#else
	return NULL;
#endif //#if WITH_FACEFX
}

/** 
 *	Check the given map package for persistent level.
 *	If it is, then generate the corresponding FaceFX animation information.
 *
 *	@param	InPackageFile	The name of the package being loaded
 */
UBOOL FPersistentFaceFXAnimSetGenerator::SetupPersistentMapFaceFXAnimation(const FFilename& InPackageFile)
{
#if WITH_FACEFX
	SCOPE_SECONDS_COUNTER_COOK(PersistentFaceFXTime);
	SCOPE_SECONDS_COUNTER_COOK(PersistentFaceFXDeterminationTime);

	check(PersistentMapInfo);
	FString UpperMapName = InPackageFile.GetBaseFilename().ToUpper();
	FString CheckPMapAlias;
	if (PersistentMapInfo->GetPersistentMapAlias(UpperMapName, CheckPMapAlias) == TRUE)
	{
		// It's an alias, so sub in the real pmap name
//		debugf(TEXT("PersistentFaceFX> PMap alias being processed: %s"), *UpperMapName);
		UpperMapName = CheckPMapAlias;
	}
	FString PersistentLevelName;
	if (PersistentMapInfo->GetPersistentMapForLevel(UpperMapName, PersistentLevelName) == FALSE)
	{
		// Map not found... nothing to do!
		return FALSE;
	}

	PersistentLevelName = PersistentLevelName.ToUpper();

	UBOOL bItIsThePersistentMap = (UpperMapName == PersistentLevelName);

	//UBOOL bFastMode = CallerCommandlet->IsA(UCookPackagesCommandlet::StaticClass()) && CastChecked<UCookPackagesCommandlet>(CallerCommandlet)->bIsMTChild;
	UBOOL bFastMode = FALSE; // FastMode temporarily disabled
	if (bFastMode)
	{
		debugf(TEXT("FastMode FaceFX determination."));
	}


	// Find the mapping of info...
	TGroupAnimToMangledFaceFXMap* GroupAnimToFFXMap = PersistentMapToGroupAnimLookupMap.Find(PersistentLevelName);
	if (GroupAnimToFFXMap == NULL || bFastMode)
	{
		// Reset the mangled name status to make sure it's consistent across cookers
		FaceFXMangledNameCount = 0;
		GroupNameToMangledMap.Empty();

		// It has not been generated yet - do it!
		TGroupAnimToMangledFaceFXMap TempPMGA;
		PersistentMapToGroupAnimLookupMap.Set(PersistentLevelName, TempPMGA);
		GroupAnimToFFXMap = PersistentMapToGroupAnimLookupMap.Find(PersistentLevelName);
		check(GroupAnimToFFXMap);

		TArray<FMangledFaceFXInfo>* PMapToFaceFXData = PersistentMapFaceFXArray.Find(PersistentLevelName);
		if (PMapToFaceFXData == NULL)
		{
			TArray<FMangledFaceFXInfo> TempData;
			PersistentMapFaceFXArray.Set(PersistentLevelName, TempData);
			PMapToFaceFXData = PersistentMapFaceFXArray.Find(PersistentLevelName);
		}
		check(PMapToFaceFXData);

		// Figure out all the sub-levels (and the pmap) that are pointed to by this map
		TArray<FFilename> MapsToProcess;
		const TArray<FString>* ContainedLevels = PersistentMapInfo->GetPersistentMapContainedLevelsList(PersistentLevelName);
		if (ContainedLevels != NULL)
		{
			for (INT CopyIdx = 0; CopyIdx < ContainedLevels->Num(); CopyIdx++)
			{
				if (bItIsThePersistentMap || !bFastMode || UpperMapName == FFilename((*ContainedLevels)(CopyIdx)).GetBaseFilename().ToUpper() )
				{
					MapsToProcess.AddUniqueItem((*ContainedLevels)(CopyIdx));
				}
			}
		}
		else
		{
			checkf(0, TEXT("This should no longer be required!"));
		}
		// Process each map...
		UPackage* MapPackage;
		UWorld* World;
		AWorldInfo* WorldInfo;

		FFilename* CurrentMapPackageFile = MapsToProcess.GetTypedData();
		for (INT MapIndex = 0; MapIndex < MapsToProcess.Num(); MapIndex++)
		{
			MapPackage = UObject::LoadPackage( NULL, *(*CurrentMapPackageFile), LOAD_None );
			if (MapPackage == NULL)
			{
				warnf(NAME_Log, TEXT("SetupPersistentMapFaceFXAnimation> Failed to load package %s"), *(*CurrentMapPackageFile));
				CurrentMapPackageFile++;
				continue;
			}
			CurrentMapPackageFile++;
			World = FindObject<UWorld>(MapPackage, TEXT("TheWorld"));
			if (World == NULL)
			{
				//debugf(TEXT("SetupPersistentMapFaceFXAnimation> Not a map package? %s"), *MapPackageFile);
				continue;
			}
			WorldInfo = World->GetWorldInfo();
			if (WorldInfo == NULL)
			{
				//debugf(TEXT("SetupPersistentMapFaceFXAnimation> No WorldInfo? %s"), *MapPackageFile);
				continue;
			}

			if (bLogPersistentFaceFXGeneration)
			{
				warnf(NAME_Log, TEXT("SetupPersistentMapFaceFXAnimation> Processing map package %s"), *(MapPackage->GetName()));
			}

			FString UpperMapName = MapPackage->GetName().ToUpper();

			INT SoundCueCount = 0;
			INT MangledSoundCueCount = 0;
			INT SeqAct_PlayFaceFXAnmCount = 0;
			INT MangledSeqAct_PlayFaceFXAnmCount = 0;
			INT InterpFaceFXCount = 0;
			INT MangledInterpFaceFXCount = 0;

			if (bLogPersistentFaceFXGeneration)
			{
				debugf(TEXT("\t..... Checking objects ....."));
			}
			for (FObjectIterator It; It; ++It)
			{
				USoundCue* SoundCue = Cast<USoundCue>(*It);
				UInterpTrackFaceFX* ITFaceFX = Cast<UInterpTrackFaceFX>(*It);
				USeqAct_PlayFaceFXAnim* SAPlayFFX = Cast<USeqAct_PlayFaceFXAnim>(*It);

				// Note: It is assumed that ANY sound cue that is encountered is going to be cooked into the map...
				if (SoundCue && (SoundCue->HasAnyFlags(RF_MarkedByCooker|RF_Transient) == FALSE))
				{
					UBOOL bIgnoreSoundCue = FALSE;
					if (GGameCookerHelper && CallerCommandlet->IsA(UCookPackagesCommandlet::StaticClass()))
					{
						bIgnoreSoundCue = GGameCookerHelper->ShouldSoundCueBeIgnoredForPersistentFaceFX(CastChecked<UCookPackagesCommandlet>(CallerCommandlet), SoundCue);
					}

					if (bIgnoreSoundCue == FALSE)
					{
						SoundCueCount++;
						if (SoundCue->FaceFXAnimSetRef)
						{
							FString AnimSetRefName = SoundCue->FaceFXAnimSetRef->GetPathName();
							if (ShouldIncludeToPersistentMapFaceFXAnimation(AnimSetRefName))
							{
								if (SoundCue->FaceFXGroupName.Len())
								{
									if (SoundCue->FaceFXAnimName.Len())
									{
										// Find it in the mangled array
										FString GroupName = SoundCue->FaceFXGroupName;
										FString AnimName = SoundCue->FaceFXAnimName;

										TMangledFaceFXMap* MangledFFXMap = GetMangledFaceFXMap(GroupAnimToFFXMap, GroupName, AnimName, TRUE);
										check(MangledFFXMap);
										// Find the info for this sound
										FMangledFaceFXInfo* MangledFFXInfo = GetMangledFaceFXMap(MangledFFXMap, PersistentLevelName, AnimSetRefName, GroupName, AnimName, FALSE);
										if (MangledFFXInfo == NULL)
										{
											MangledFFXInfo = GetMangledFaceFXMap(MangledFFXMap, PersistentLevelName, AnimSetRefName, 
												GroupName, AnimName, TRUE);
											PMapToFaceFXData->AddUniqueItem(*MangledFFXInfo);
											if (bLogPersistentFaceFXGeneration)
											{
												warnf(NAME_Log, TEXT("SoundCue %s: %s-%s --> %s-%s"), 
													*(SoundCue->GetPathName()),
													*GroupName, *AnimName,
													*(MangledFFXInfo->MangledFaceFXGroupName), 
													*(MangledFFXInfo->MangledFaceFXAnimName));
											}
										}
										MangledSoundCueCount++;
									}
									else
									{
										if (bLogPersistentFaceFXGeneration)
										{
											warnf(TEXT("\tInvalid FaceFXAnimName  on %s"), *(SoundCue->GetName()));
										}
									}
								}
								else
								{
									if (bLogPersistentFaceFXGeneration)
									{
										warnf(TEXT("\tInvalid FaceFXAnimGroup on %s"), *(SoundCue->GetName()));
									}
								}
							}
							else
							{
								// Skipping as it is an untouched FaceFX animset.
								if (bLogPersistentFaceFXGeneration)
								{
									debugf(TEXT("Skipping Safe FaceFXAnimSet %s - SoundCue %s"), 
										*(SoundCue->FaceFXAnimSetRef->GetPathName()), *(SoundCue->GetName()));
								}
							}
						}
						else
						{
							if (SoundCue->FaceFXGroupName.Len())
							{
								if (SoundCue->FaceFXAnimName.Len())
								{
									if (bLogPersistentFaceFXGeneration)
									{
										warnf(TEXT("\tNo AnimSetRef on %s"), *(SoundCue->GetName()));
									}
								}
							}
						}
					}
					else
					{
						//debugf(TEXT("\tIGNORING SoundCue %s"), *(SoundCue->GetPathName()));
					}
				}

				// Note: It is assumed that ANY InterpTrackFaceFX that is encountered is going to be cooked into the map...
				if (ITFaceFX && (ITFaceFX->HasAnyFlags(RF_MarkedByCooker|RF_Transient) == FALSE))
				{
					// 
					for (INT FFXIndex = 0; FFXIndex < ITFaceFX->FaceFXSeqs.Num(); FFXIndex++)
					{
						InterpFaceFXCount++;

						FFaceFXTrackKey& FFXKey = ITFaceFX->FaceFXSeqs(FFXIndex);

						INT SetIndex = FindSourceAnimSet(ITFaceFX->FaceFXAnimSets, FFXKey.FaceFXGroupName, FFXKey.FaceFXSeqName);
						if (SetIndex != -1)
						{
							// If it doesn't find the AnimSetRef, it is assumed to be bogus!
							UFaceFXAnimSet* TheFaceFXAnimSet = ITFaceFX->FaceFXAnimSets(SetIndex);
							if (TheFaceFXAnimSet && ShouldIncludeToPersistentMapFaceFXAnimation(TheFaceFXAnimSet->GetPathName()))
							{
								// It's a valid one...
								// Find it in the mangled array
								FString AnimSetRefName = TheFaceFXAnimSet->GetPathName();
								FString GroupName = FFXKey.FaceFXGroupName;
								FString AnimName = FFXKey.FaceFXSeqName;

								TMangledFaceFXMap* MangledFFXMap = GetMangledFaceFXMap(GroupAnimToFFXMap, GroupName, AnimName, TRUE);
								check(MangledFFXMap);
								// Find the info for this sound
								FMangledFaceFXInfo* MangledFFXInfo = GetMangledFaceFXMap(MangledFFXMap, PersistentLevelName, 
									AnimSetRefName, GroupName, AnimName, FALSE);
								if (MangledFFXInfo == NULL)
								{
									MangledFFXInfo = GetMangledFaceFXMap(MangledFFXMap, PersistentLevelName, AnimSetRefName, 
										GroupName, AnimName, TRUE);
									PMapToFaceFXData->AddUniqueItem(*MangledFFXInfo);
									if (bLogPersistentFaceFXGeneration)
									{
										warnf(NAME_Log, TEXT("InterpTrack %s: %s-%s --> %s-%s"), 
											*(ITFaceFX->GetPathName()),
											*GroupName, *AnimName,
											*(MangledFFXInfo->MangledFaceFXGroupName), 
											*(MangledFFXInfo->MangledFaceFXAnimName));
									}
								}
								MangledInterpFaceFXCount++;
							}
						}
						else
						{
							if (bLogPersistentFaceFXGeneration)
							{
								warnf(NAME_Log, TEXT("Failed to find AnimSetRef for %s, %s-%s"), 
									*(ITFaceFX->GetPathName()), *FFXKey.FaceFXGroupName, *FFXKey.FaceFXSeqName);
							}
						}
					}
				}

				// Note: It is assumed that ANY InterpTrackFaceFX that is encountered is going to be cooked into the map...
				if (SAPlayFFX && (SAPlayFFX->HasAnyFlags(RF_MarkedByCooker|RF_Transient) == FALSE))
				{
					SeqAct_PlayFaceFXAnmCount++;
					if (SAPlayFFX->FaceFXAnimSetRef != NULL)
					{
						if (ShouldIncludeToPersistentMapFaceFXAnimation(SAPlayFFX->FaceFXAnimSetRef->GetPathName()))
						{
							// If valid to mangle...
							// See if it is already in the list.
							FMangledFaceFXInfo TempPFFXData;

							FString AnimSetRefName = SAPlayFFX->FaceFXAnimSetRef->GetPathName();
							FString GroupName = SAPlayFFX->FaceFXGroupName;
							FString AnimName = SAPlayFFX->FaceFXAnimName;

							TMangledFaceFXMap* MangledFFXMap = GetMangledFaceFXMap(GroupAnimToFFXMap, GroupName, AnimName, TRUE);
							check(MangledFFXMap);
							// Find the info for this sound
							FMangledFaceFXInfo* MangledFFXInfo = GetMangledFaceFXMap(MangledFFXMap, PersistentLevelName, 
								AnimSetRefName, GroupName, AnimName, FALSE);
							if (MangledFFXInfo == NULL)
							{
								MangledFFXInfo = GetMangledFaceFXMap(MangledFFXMap, PersistentLevelName, AnimSetRefName, 
									GroupName, AnimName, TRUE);
								PMapToFaceFXData->AddUniqueItem(*MangledFFXInfo);
								if (bLogPersistentFaceFXGeneration)
								{
									warnf(NAME_Log, TEXT("SeqActPlayFaceFX %s: %s-%s --> %s-%s"), 
										*(SAPlayFFX->GetPathName()),
										*GroupName, *AnimName,
										*(MangledFFXInfo->MangledFaceFXGroupName), 
										*(MangledFFXInfo->MangledFaceFXAnimName));
								}
							}
							MangledSeqAct_PlayFaceFXAnmCount++;
						}
					}
					else
					{
						if (bLogPersistentFaceFXGeneration)
						{
							warnf(NAME_Log, TEXT("\t\tNo FaceFXAnimSetRef on %s!"), *(SAPlayFFX->GetPathName()));
						}
					}
				}
			}
			if (bLogPersistentFaceFXGeneration)
			{
				warnf(NAME_Log, TEXT("\t..... Completed SoundCues - Mangled %5d out of %5d....."), MangledSoundCueCount, SoundCueCount);
				warnf(NAME_Log, TEXT("\t..... Completed InterpTrackFaceFX - Mangled %5d out of %5d....."), MangledInterpFaceFXCount, InterpFaceFXCount);
				warnf(NAME_Log, TEXT("\t..... Completed SeqAct_PlayFaceFXAnim - Mangled %5d out of %5d....."), MangledSeqAct_PlayFaceFXAnmCount, SeqAct_PlayFaceFXAnmCount);
			}

			GarbageCollectFn();
		}
	}

	return TRUE;
#else
	return FALSE;
#endif	//#if WITH_FACEFX
}

/** 
 *	Generate the persistent level FaceFX AnimSet.
 *
 *	@param	InPackageFile	The name of the package being loaded
 *	@return	UFaceFXAnimSet*	The generated anim set, or NULL if not the persistent map
 */
UFaceFXAnimSet* FPersistentFaceFXAnimSetGenerator::GeneratePersistentMapFaceFXAnimSet(const FFilename& InPackageFile)
{
#if WITH_FACEFX
	// Setup the persistent FaceFX animation information, if required...
	if ( bGeneratePersistentMapAnimSet )
	{
		ClearCurrentPersistentMapCache();

		FFilename CheckPackageFilename = InPackageFile;
		if ( SetupPersistentMapFaceFXAnimation(CheckPackageFilename) )
		{
			// If this is the persistent map, need to create it BEFORE actually loading the package
			// This is to properly clean up the package(s) that get(s) loaded generating it...
			FString LevelCheckName = CheckPackageFilename.GetBaseFilename().ToUpper();
			FString OriginalName = LevelCheckName;
			FString PMapAlias;
			UBOOL bAliased = PersistentMapInfo->GetPersistentMapAlias(LevelCheckName, PMapAlias);
			if (bAliased == TRUE)
			{
//				warnf(TEXT("Found alias for map: %s --> %s"), *LevelCheckName, *PMapAlias);
				LevelCheckName = PMapAlias;
			}
			FString PMapString;
			if (PersistentMapInfo->GetPersistentMapForLevel(LevelCheckName, PMapString) == TRUE)
			{
				if (bLogPersistentFaceFXGeneration)
				{
					debugf(TEXT("Found PMap %32s for level %s"), *PMapString, *LevelCheckName);
				}

				SetCurrentPersistentMapCache(PMapString, bAliased ? OriginalName : PMapString);

				// Generate the FaceFX anim set for the level...
				if (PMapString == LevelCheckName)
				{
					return GeneratePersistentMapFaceFXAnimSetFromCurrentPMapCache();
				}
			}
		}
	}
#endif //#if WITH_FACEFX
	return NULL;
}

/** 
*	Generate the persistent level FaceFX AnimSet from Cached value.
*
*	@return	UFaceFXAnimSet*	The generated anim set, or NULL if not the persistent map
*/
UFaceFXAnimSet* FPersistentFaceFXAnimSetGenerator::GeneratePersistentMapFaceFXAnimSetFromCurrentPMapCache()
{
#if WITH_FACEFX 
	SCOPE_SECONDS_COUNTER_COOK(PersistentFaceFXTime);
	SCOPE_SECONDS_COUNTER_COOK(PersistentFaceFXGenerationTime);

	UFaceFXAnimSet* NewFaceFXAnimSet = NULL;
	FString PersistentMapName = CurrentPersistentMapName;
	if (CurrentPersistentMapFaceFXArray)
	{
		// Make sure there are some...
		if (CurrentPersistentMapFaceFXArray->Num() == 0)
		{
			// No animations to grab!
			return NewFaceFXAnimSet;
		}

		// Need to pull in all the associated FaceFX stuff
		warnf(NAME_Log, TEXT("Generating FaceFXAnimSet for PersistentMap %s"), *AliasedPersistentMapName);
		warnf(NAME_Log, TEXT("\tThere are %5d FaceFX Animations to add..."), CurrentPersistentMapFaceFXArray->Num());

		//@todo.SAS. Sort them by package to minimize overhead? Probably not necessary...

		// Create the new FaceFXAnimSet
		FString AnimSetName = AliasedPersistentMapName + TEXT("_FaceFXAnimSet");
		NewFaceFXAnimSet = Cast<UFaceFXAnimSet>(UObject::StaticConstructObject(UFaceFXAnimSet::StaticClass(), UObject::GetTransientPackage(), FName(*AnimSetName)));
		check(NewFaceFXAnimSet);
		NewFaceFXAnimSet->SetFlags( RF_Standalone );
		// Add to root to prevent GC!
		NewFaceFXAnimSet->AddToRoot();

		// Create the FxAnimSet...
		OC3Ent::Face::FxName FFXAnimSetName(TCHAR_TO_ANSI(*AnimSetName));
		FString TempAGNewName = PersistentMapName.ToUpper();
		OC3Ent::Face::FxName AnimGroupNewName(TCHAR_TO_ANSI(*TempAGNewName));

		OC3Ent::Face::FxAnimSet* DstFFXAnimSet = new OC3Ent::Face::FxAnimSet();
		check(DstFFXAnimSet);
		DstFFXAnimSet->SetName(FFXAnimSetName);

		// Create the FXAnimGroup...
		OC3Ent::Face::FxAnimGroup ContainedAnimGroup;
		ContainedAnimGroup.SetName(AnimGroupNewName);
		DstFFXAnimSet->SetAnimGroup(ContainedAnimGroup);
		NewFaceFXAnimSet->InternalFaceFXAnimSet = DstFFXAnimSet;

		OC3Ent::Face::FxAnimGroup* DestFFXAnimGroup = (OC3Ent::Face::FxAnimGroup*)&(DstFFXAnimSet->GetAnimGroup());

		// Copy all of the reference animations into the new anim set
		for (INT AnimIndex = 0; AnimIndex < CurrentPersistentMapFaceFXArray->Num(); AnimIndex++)
		{
			FMangledFaceFXInfo& FFXInfo = (*CurrentPersistentMapFaceFXArray)(AnimIndex);

			// loading a FaceFXAsset
			//UFaceFXAsset* SrcFaceFXAsset = LoadObject<UFaceFXAsset>(NULL, *(FFXInfo.FaceFXAnimSet), NULL, LOAD_None, NULL);
			// loading a FaceFXAnimSet
			UFaceFXAnimSet* SrcFaceFXAnimSet = LoadObject<UFaceFXAnimSet>(NULL, *(FFXInfo.FaceFXAnimSet), NULL, LOAD_None, NULL);
			if (SrcFaceFXAnimSet)
			{
				OC3Ent::Face::FxName SrcAnimGroupName(TCHAR_TO_ANSI(*(FFXInfo.OriginalFaceFXGroupName)));

				OC3Ent::Face::FxName SrcAnimName(TCHAR_TO_ANSI(*(FFXInfo.OriginalFaceFXAnimName)));
				OC3Ent::Face::FxSize AnimIndex = OC3Ent::Face::FxInvalidIndex;

				// If it wasn't found in the asset, check in any available animsets...
				const OC3Ent::Face::FxAnimSet* SrcFFXAnimSet = SrcFaceFXAnimSet->GetFxAnimSet();
				const OC3Ent::Face::FxAnimGroup& TempFFXAnimGroup = SrcFFXAnimSet->GetAnimGroup();
				OC3Ent::Face::FxAnimGroup* SrcFFXAnimGroup = (OC3Ent::Face::FxAnimGroup*)&TempFFXAnimGroup;

				AnimIndex = SrcFFXAnimGroup->FindAnim(SrcAnimName);
				if (AnimIndex != OC3Ent::Face::FxInvalidIndex)
				{
					const OC3Ent::Face::FxAnim& AnimRef = SrcFFXAnimGroup->GetAnim(AnimIndex);
					if (DestFFXAnimGroup->AddAnim(AnimRef) == FALSE)
					{
						UBOOL bAlreadyExists = (DestFFXAnimGroup->FindAnim(SrcAnimName) != OC3Ent::Face::FxInvalidIndex) ? TRUE : FALSE;
						warnf(NAME_Log, TEXT("\t%s: Failed to add animation %s from %s"),  
							bAlreadyExists ? TEXT("Already exists") : TEXT("ERROR"), 
							*(FFXInfo.OriginalFaceFXAnimName), *(FFXInfo.FaceFXAnimSet));
					}
					else
					{
						// Rename it within the new AnimSet
						OC3Ent::Face::FxSize NewAnimIndex = DestFFXAnimGroup->FindAnim(SrcAnimName);
						if (AnimIndex != OC3Ent::Face::FxInvalidIndex)
						{
							const OC3Ent::Face::FxAnim& NewAnimRef = DestFFXAnimGroup->GetAnim(NewAnimIndex);
							OC3Ent::Face::FxAnim* NewAnim = (OC3Ent::Face::FxAnim*)&NewAnimRef;
							OC3Ent::Face::FxName NewAnimName(TCHAR_TO_ANSI(*(FFXInfo.MangledFaceFXAnimName)));
							NewAnim->SetName(NewAnimName);
							if (bLogPersistentFaceFXGeneration)
							{
								warnf(NAME_Log, TEXT("\t%s,%s,%s,as,%s,%s"),  
									*(FFXInfo.FaceFXAnimSet), *(FFXInfo.OriginalFaceFXGroupName), *(FFXInfo.OriginalFaceFXAnimName),
									*(FFXInfo.MangledFaceFXGroupName), *(FFXInfo.MangledFaceFXAnimName)
									);
							}
						}
						else
						{
							warnf(NAME_Log, TEXT("\tFailed to find anim %s in dest group!"), ANSI_TO_TCHAR(SrcAnimName.GetAsCstr()));
						}
					}
				}
				else
				{
					warnf(NAME_Log, TEXT("\tFailed to load source animation %s in %s"),  *(FFXInfo.OriginalFaceFXAnimName), *(FFXInfo.FaceFXAnimSet));
				}
			}
			else
			{
				warnf(NAME_Warning, TEXT("\tFailed to load source FaceFXAnimSet %s"), *(FFXInfo.FaceFXAnimSet));
			}
		}

		// Collect garbage and verify it worked as expected.	
		GarbageCollectFn();
	}
	else
	{
		if (bLogPersistentFaceFXGeneration)
		{
			debugf(TEXT("\tNo CurrentPersistentMapFaceFXArray set!"));
		}
	}
	return NewFaceFXAnimSet;
#else	//#if WITH_FACEFX
	return NULL;
#endif	//#if WITH_FACEFX
}

/**
 * Cooks passed in object if it hasn't been already.
 *
 * @param	Package						Package going to be saved
 * @param	Object		Object to cook
 * @param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
 */
void UCookPackagesCommandlet::CookObject( UPackage* Package, UObject* Object, UBOOL bIsSavedInSeekFreePackage )
{ 
	if( Object && !Object->HasAnyFlags( RF_Cooked ) )
	{
		if( !Object->HasAnyFlags( RF_ClassDefaultObject ) )
		{
			SCOPE_SECONDS_COUNTER_COOK(CookTime);

			UBOOL bProcessCook = TRUE;
			if (GGameCookerHelper)
			{
				bProcessCook = GGameCookerHelper->CookObject(this, Package, Object, bIsSavedInSeekFreePackage);
			}

			if (bProcessCook)
			{
				ULevel*				Level				= Cast<ULevel>(Object);
				UTexture2D*			Texture2D			= Cast<UTexture2D>(Object);
				UTextureMovie*		TextureMovie		= Cast<UTextureMovie>(Object);
				USoundNodeWave*		SoundNodeWave		= Cast<USoundNodeWave>(Object);
				UTerrainComponent*	TerrainComponent	= Cast<UTerrainComponent>(Object);
				UBrushComponent*	BrushComponent		= Cast<UBrushComponent>(Object);
				UParticleSystem*	ParticleSystem		= Cast<UParticleSystem>(Object);

				USkeletalMesh*		SkeletalMesh		= Cast<USkeletalMesh>(Object);
				UStaticMesh*		StaticMesh			= Cast<UStaticMesh>(Object);
				UClass*				ClassObj			= Cast<UClass>(Object);
				
				AWorldInfo*			WorldInfoActor		= Cast<AWorldInfo>(Object);

				USoundCue*			SoundCue			= Cast<USoundCue>(Object);
				UInterpTrackFaceFX* FaceFXInterpTrack	= Cast<UInterpTrackFaceFX>(Object);
				USeqAct_PlayFaceFXAnim* SeqActPlayFaceFX= Cast<USeqAct_PlayFaceFXAnim>(Object);

				UShadowMap2D*		ShadowMap2D			= Cast<UShadowMap2D>(Object);

				// Cook level, making sure it has built texture streaming data
				if (Level)
				{
					CookLevel( Level );
				}
				// Cook texture, storing in platform format.
				else if( Texture2D )
				{
					CookTexture( Package, Texture2D, bIsSavedInSeekFreePackage );
				}
				// Cook movie texture data
				else if( TextureMovie )
				{
					CookMovieTexture( TextureMovie );
				}
				// Cook brush component, caching physics data and discarding original BSP if it's a volume
				else if ( BrushComponent )
				{
					CookBrushComponent( Package, BrushComponent, bIsSavedInSeekFreePackage );
				}
				// Cook sound, compressing to platform format
				else if( SoundNodeWave )
				{
					CookSoundNodeWave( SoundNodeWave );

					LocSoundNodeWave( SoundNodeWave );

					// Free up the data cached for the platforms we are not cooking for
					if( Platform == PLATFORM_Xbox360 )
					{
						SoundNodeWave->CompressedPCData.RemoveBulkData();
						SoundNodeWave->CompressedPS3Data.RemoveBulkData();
						SoundNodeWave->CompressedWiiUData.RemoveBulkData();
						SoundNodeWave->CompressedIPhoneData.RemoveBulkData();
						SoundNodeWave->CompressedFlashData.RemoveBulkData();
					}
					else if( Platform == PLATFORM_PS3 )
					{
						SoundNodeWave->CompressedPCData.RemoveBulkData();
						SoundNodeWave->CompressedXbox360Data.RemoveBulkData();
						SoundNodeWave->CompressedWiiUData.RemoveBulkData();
						SoundNodeWave->CompressedIPhoneData.RemoveBulkData();
						SoundNodeWave->CompressedFlashData.RemoveBulkData();
					}
					else if( Platform == PLATFORM_WiiU )
					{
						SoundNodeWave->CompressedPCData.RemoveBulkData();
						SoundNodeWave->CompressedXbox360Data.RemoveBulkData();
						SoundNodeWave->CompressedPS3Data.RemoveBulkData();
						SoundNodeWave->CompressedIPhoneData.RemoveBulkData();
						SoundNodeWave->CompressedFlashData.RemoveBulkData();
					}
					else if( Platform == PLATFORM_IPhone )
					{
						SoundNodeWave->CompressedPCData.RemoveBulkData();
						SoundNodeWave->CompressedXbox360Data.RemoveBulkData();
						SoundNodeWave->CompressedPS3Data.RemoveBulkData();
						SoundNodeWave->CompressedWiiUData.RemoveBulkData();
						SoundNodeWave->CompressedFlashData.RemoveBulkData();
					}
					else if( Platform == PLATFORM_Flash )
					{
						SoundNodeWave->CompressedPCData.RemoveBulkData();
						SoundNodeWave->CompressedXbox360Data.RemoveBulkData();
						SoundNodeWave->CompressedWiiUData.RemoveBulkData();
						SoundNodeWave->CompressedPS3Data.RemoveBulkData();
						SoundNodeWave->CompressedIPhoneData.RemoveBulkData();
					}
					else if( Platform == PLATFORM_Windows ||
							 Platform == PLATFORM_WindowsConsole ||
							 Platform == PLATFORM_WindowsServer ||
							 Platform == PLATFORM_MacOSX )
					{
						//@TODO - remove sounds for dedicated server
						SoundNodeWave->CompressedPS3Data.RemoveBulkData();
						SoundNodeWave->CompressedXbox360Data.RemoveBulkData();
						SoundNodeWave->CompressedWiiUData.RemoveBulkData();
						SoundNodeWave->CompressedIPhoneData.RemoveBulkData();
						SoundNodeWave->CompressedFlashData.RemoveBulkData();
					}
					else if( Platform == PLATFORM_IPhone ||
							 Platform == PLATFORM_NGP ||
							 Platform == PLATFORM_Android )
					{
						SoundNodeWave->CompressedPCData.RemoveBulkData();
						SoundNodeWave->CompressedPS3Data.RemoveBulkData();
						SoundNodeWave->CompressedXbox360Data.RemoveBulkData();
						SoundNodeWave->CompressedWiiUData.RemoveBulkData();
						SoundNodeWave->CompressedIPhoneData.RemoveBulkData();
						SoundNodeWave->CompressedFlashData.RemoveBulkData();
					}
				}
				else if (SoundCue)
				{
					CookSoundCue(SoundCue);
				}
				else if (FaceFXInterpTrack)
				{
					CookFaceFXInterpTrack(FaceFXInterpTrack);
				}
				else if (SeqActPlayFaceFX)
				{
					CookSeqActPlayFaceFXAnim(SeqActPlayFaceFX);
				}
				// Cook particle systems
				else if (ParticleSystem)
				{
					CookParticleSystem(ParticleSystem);
				}
				else if (SkeletalMesh)
				{
					CookSkeletalMesh(SkeletalMesh);
				}
				else if (StaticMesh)
				{
					CookStaticMesh(StaticMesh);
				}
				else if (ShadowMap2D)
				{
					if ((Platform & PLATFORM_Mobile) != 0)
					{
						if (ShadowMap2D->IsTemplate(RF_ClassDefaultObject) == FALSE)
						{
							ShadowMap2D->ClearFlags(RF_ForceTagExp);
							ShadowMap2D->ClearFlags(RF_TagExp);
							ShadowMap2D->SetFlags(RF_Transient|RF_MarkedByCooker);
						}
					}
				}
				// If WorldInfo, clear the ClipPadEntry array
				else if (WorldInfoActor)
				{
					WorldInfoActor->ClipPadEntries.Empty();
				}
				else if ( ClassObj && (Platform & UE3::PLATFORM_Stripped) != 0 )
				{
					// if cooking for console and this is the StaticMeshCollectionActor class, mark it placeable
					// so that it can be spawned at runtime
					if ( ClassObj == AStaticMeshCollectionActor::StaticClass() || ClassObj == AStaticLightCollectionActor::StaticClass() )
					{
						ClassObj->ClassFlags |= CLASS_Placeable;
					}
				}
			}
		}

		{
			SCOPE_SECONDS_COUNTER_COOK(CookStripTime);
			// Remove extra data by platform
			Object->StripData(Platform);

			if( !Object->HasAnyFlags( RF_ClassDefaultObject ) )
			{
				UFaceFXAnimSet * FaceFXAnimSet = Cast<UFaceFXAnimSet>(Object);
				if ( FaceFXAnimSet )
				{	
					// Do not include map package as they should be included to the persistentFaceFXAnimSet
					// This could be false information if any other reference is introduced 
					if (!PersistentFaceFXAnimSetGenerator.GetGeneratePersistentMapAnimSet() || (Package && !Package->ContainsMap()))
					{
						LogFaceFXAnimSet( FaceFXAnimSet, Package );
					}
				}
			}
		}

		// Avoid re-cooking.
		Object->SetFlags( RF_Cooked );
	}
}


/** mirrored in XeTools.cpp */
enum TextureCookerCreateFlags
{
	// skip miptail packing
	TextureCreate_NoPackedMip = 1<<0,
	// convert to piecewise-linear approximated gamma curve
	TextureCreate_PWLGamma = 1<<1
};

/**
 * Helper function used by CookObject - performs level specific cooking.
 *
 * @param	Level						Level object to process
 */
void UCookPackagesCommandlet::CookLevel( ULevel* Level )
{
	ULevel::BuildStreamingData( NULL, Level );
}

/**
 * Helper function used by CookObject - performs texture specific cooking.
 *
 * @param	Package						Package going to be saved
 * @param	Texture		Texture to cook
 * @param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
 */
void UCookPackagesCommandlet::CookTexture( UPackage* Package, UTexture2D* Texture2D, UBOOL bIsSavedInSeekFreePackage )
{
	// Cook 2D textures.
	check(Texture2D);

	if (Texture2D->bIsEditorOnly)
	{
		Texture2D->ClearFlags(RF_ForceTagExp); 
		Texture2D->ClearFlags(RF_TagExp); 
		Texture2D->SetFlags(RF_MarkedByCooker|RF_Transient);	
		return;
	}

	if( !Texture2D->IsTemplate( RF_ClassDefaultObject ) )
	{
		if ((Platform & PLATFORM_Mobile) != 0)
		{
			UShadowMapTexture2D* ShadowMapTexture = Cast<UShadowMapTexture2D>(Texture2D);
			if (ShadowMapTexture != NULL)
			{
				ShadowMapTexture->ClearFlags(RF_ForceTagExp);
				ShadowMapTexture->ClearFlags(RF_TagExp);
				ShadowMapTexture->SetFlags(RF_MarkedByCooker|RF_Transient);
				return;
			}

			if( Texture2D->LODGroup == TEXTUREGROUP_Terrain_Heightmap || Texture2D->LODGroup == TEXTUREGROUP_Terrain_Weightmap )
			{
				Texture2D->ClearFlags(RF_ForceTagExp);
				Texture2D->ClearFlags(RF_TagExp);
				Texture2D->SetFlags(RF_MarkedByCooker|RF_Transient);
				return;
			}
		}

		if (Texture2D->Format == PF_A1)
		{
			// mark it so it won't go in to the .tfc later
			Texture2D->NeverStream = TRUE;
			return;
		}

		// This could be generalized to other platforms by looking in their Engine.ini
		ULightMapTexture2D* LightMapTexture = Cast<ULightMapTexture2D>(Texture2D);
		DWORD MobilePlatformSansFlash = UE3::PLATFORM_Mobile & (~UE3::PLATFORM_Flash);
		const UBOOL bIsLightmap = (LightMapTexture != NULL);
		const UBOOL bIsSimpleLightmap = bIsLightmap && Texture2D->GetName().StartsWith("Simple");
		const UBOOL bIsDirectionalLightmap = bIsLightmap && !bIsSimpleLightmap;
		const UBOOL bShouldSkipLightmap = (
			((PreferredLightmapType == EPLT_Directional) && bIsSimpleLightmap) ||
			((PreferredLightmapType == EPLT_Simple) && bIsDirectionalLightmap)
		);

		if (bShouldSkipLightmap)
		{
			// mark it so it won't go in to the .tfc later
			Texture2D->NeverStream = TRUE;
			Texture2D->ClearFlags(RF_ForceTagExp);
			Texture2D->ClearFlags(RF_TagExp);
			Texture2D->SetFlags(RF_MarkedByCooker|RF_Transient);
			return;
		}

		if (Texture2D->Mips.Num() > 0)
		{
			SCOPE_SECONDS_COUNTER_COOK(CookTextureTime);

			UBOOL bSkipCookerOperations = FALSE;
			// if the texture had iphone mips cached, then use those instead of the original mips

			// android needs to swap red and blue since it's missing the runtime texture format for swapping, like iOS has
			UBOOL bNeedToSwapRedAndBlueChannels = (Platform == PLATFORM_Android && Texture2D->Format == PF_A8R8G8B8);
			if (bNeedToSwapRedAndBlueChannels)
			{
				for (INT Mip = 0; Mip < Texture2D->Mips.Num(); Mip++)
				{
					INT MipSizeU = Texture2D->SizeX >> Mip;
					INT MipSizeV = Texture2D->SizeY >> Mip;

					FColor* MipMap = (FColor*)Texture2D->Mips(Mip).Data.Lock(LOCK_READ_WRITE);

					// swap reds and blues
					for (INT Idx = 0; Idx < MipSizeU * MipSizeV; ++Idx)
					{
						BYTE TempRed = MipMap[Idx].R;
						MipMap[Idx].R = MipMap[Idx].B;
						MipMap[Idx].B = TempRed;
					}

					Texture2D->Mips(Mip).Data.Unlock();
				}
			}

			UBOOL bIsDXT = Texture2D->Format >= PF_DXT1 && Texture2D->Format <= PF_DXT5;
			UBOOL bWillBeDXT = (Texture2D->Format == PF_A8R8G8B8 && Texture2D->DeferCompression);
			UBOOL bDoesPlatformNeedRecompression = 
				(Platform == PLATFORM_IPhone || Platform == PLATFORM_NGP || Platform == PLATFORM_Flash ||
				(Platform == PLATFORM_Android && (AndroidTextureFormat & (TEXSUPPORT_PVRTC | TEXSUPPORT_ATITC | TEXSUPPORT_ETC))));
			if ((bIsDXT || bWillBeDXT) && bDoesPlatformNeedRecompression)
			{
				// iphone expects precached mips, make them if they don't exist
				if (Platform == PLATFORM_Flash)
				{
					extern UBOOL ConditionalCacheFlashTextures(UTexture2D* Texture, UBOOL bUseFastCompression, UBOOL bForceCompression, TArray<FColor>* SourceArtOverride);
					ConditionalCacheFlashTextures(Texture2D, FALSE, FALSE, NULL);

					// make sure the data is there
					Texture2D->CachedFlashMips.MakeSureBulkDataIsLoaded();

					// Take texture LOD settings into account, avoiding cooking and keeping memory around for unused miplevels.
					INT	FirstMipIndex = Clamp<INT>( PlatformLODSettings.CalculateLODBias( Texture2D ) - Texture2D->NumCinematicMipLevels, 0, Texture2D->Mips.Num()-1 );

					// copy the flash data to mip 0, and toss everything else
					for (INT MipLevel = 0; MipLevel < Texture2D->Mips.Num(); MipLevel++)
					{
						if (MipLevel == FirstMipIndex)
						{
							checkf(Texture2D->CachedFlashMips.GetBulkDataSize() > 0, TEXT("%s failed to compress to ATF"), *Texture2D->GetFullName());
							Texture2D->Mips(MipLevel).Data = Texture2D->CachedFlashMips;
						}
						else
						{
							// toss other mips
							Texture2D->Mips(MipLevel).Data.RemoveBulkData();
						}
					}
					
					// we don't need to recook the texture, but we still want to do the mip stripping, etc
					bSkipCookerOperations = TRUE;
				}
				else if (Platform != PLATFORM_Android) // PVRTC for non-Android Platforms
				{
					ConditionalCachePVRTCTextures(Texture2D, !ParseParam(appCmdLine(), TEXT("slowpvrtc")));

					// if any textures were cached, then we just use those instead (otherwise, could be other format, etc)
					if (Texture2D->CachedPVRTCMips.Num())
					{
						check(Texture2D->CachedPVRTCMips.Num() == Texture2D->Mips.Num());

						// make sure the cached data is loaded
						for (INT MipLevel = 0; MipLevel < Texture2D->CachedPVRTCMips.Num(); MipLevel++)
						{
							Texture2D->CachedPVRTCMips(MipLevel).Data.MakeSureBulkDataIsLoaded();
							Texture2D->Mips(MipLevel).SizeX = Texture2D->CachedPVRTCMips(MipLevel).SizeX;
							Texture2D->Mips(MipLevel).SizeY = Texture2D->CachedPVRTCMips(MipLevel).SizeY;
							Texture2D->Mips(MipLevel).Data = Texture2D->CachedPVRTCMips(MipLevel).Data;
						}

						// we don't need to recook the texture, but we still want to do the mip stripping, etc
						bSkipCookerOperations = TRUE;

						// make the texture square (the mips were already squarified in ConditionalCachePVRTCTextures(
						Texture2D->SizeX = Texture2D->SizeY = Max(Texture2D->SizeX, Texture2D->SizeY);

						// force an 8x8 texture to be PVRTC4 (their mip data will already be PVRTC4)
						if (Texture2D->SizeX == 8 && Texture2D->SizeY == 8)
						{
							Texture2D->bForcePVRTC4 = TRUE;
						}
					}
				}

				// Android may hold onto multiple formats at once
				if (Platform == PLATFORM_Android && AndroidTextureFormat & TEXSUPPORT_ATITC)
				{
					extern UBOOL ConditionalCacheATITCTextures(UTexture2D* Texture, UBOOL bUseFastCompression, UBOOL bForceCompression, TArray<FColor>* SourceArtOverride);
					ConditionalCacheATITCTextures(Texture2D, FALSE, FALSE, NULL);

					// if any textures were cached, then we just use those instead (otherwise, could be other format, etc)
					if (Texture2D->CachedATITCMips.Num())
					{
						check(Texture2D->CachedATITCMips.Num() == Texture2D->Mips.Num());

						// make sure the cached data is loaded
						for (INT MipLevel = 0; MipLevel < Texture2D->CachedATITCMips.Num(); MipLevel++)
						{
							Texture2D->CachedATITCMips(MipLevel).Data.MakeSureBulkDataIsLoaded();
						}

						// we don't need to recook the texture, but we still want to do the mip stripping, etc
						bSkipCookerOperations = TRUE;

						// force an 8x8 texture to be PVRTC4 (their mip data will already be PVRTC4)
						if (Max(Texture2D->SizeX, Texture2D->SizeY) == 8)
						{
							Texture2D->bForcePVRTC4 = TRUE;
						}
					}
				}

				if (Platform == PLATFORM_Android && AndroidTextureFormat & TEXSUPPORT_ETC)
				{
					extern UBOOL ConditionalCacheETCTextures(UTexture2D* Texture, UBOOL bForceCompression, TArray<FColor>* SourceArtOverride);
					ConditionalCacheETCTextures(Texture2D, FALSE, NULL);

					// if any textures were cached, then we just use those instead (otherwise, could be other format, etc)
				    if (Texture2D->CachedETCMips.Num())
				    {
					    check(Texture2D->CachedETCMips.Num() == Texture2D->Mips.Num());
    
					    // make sure the cached data is loaded
					    for (INT MipLevel = 0; MipLevel < Texture2D->CachedETCMips.Num(); MipLevel++)
					    {
						    Texture2D->CachedETCMips(MipLevel).Data.MakeSureBulkDataIsLoaded();
					    }
    
					    // we don't need to recook the texture, but we still want to do the mip stripping, etc
					    bSkipCookerOperations = TRUE;
    
					    // force an 8x8 texture to be PVRTC4 (their mip data will already be PVRTC4)
					    if (Max(Texture2D->SizeX, Texture2D->SizeY) == 8)
					    {
						    Texture2D->bForcePVRTC4 = TRUE;
					    }
				    }
				}
				
				if (Platform == PLATFORM_Android && AndroidTextureFormat & TEXSUPPORT_PVRTC)
				{
					ConditionalCachePVRTCTextures(Texture2D, !ParseParam(appCmdLine(), TEXT("slowpvrtc")));

					// if any textures were cached, then we just use those instead (otherwise, could be other format, etc)
					if (Texture2D->CachedPVRTCMips.Num())
					{
						check(Texture2D->CachedPVRTCMips.Num() == Texture2D->Mips.Num());

						// make sure the cached data is loaded
						for (INT MipLevel = 0; MipLevel < Texture2D->CachedPVRTCMips.Num(); MipLevel++)
						{
							Texture2D->CachedPVRTCMips(MipLevel).Data.MakeSureBulkDataIsLoaded();
						}

						// we don't need to recook the texture, but we still want to do the mip stripping, etc
						bSkipCookerOperations = TRUE;

						// force an 8x8 texture to be PVRTC4 (their mip data will already be PVRTC4)
						if (Max(Texture2D->SizeX, Texture2D->SizeY) == 8)
						{
							Texture2D->bForcePVRTC4 = TRUE;
						}
					}
				}
			}
			else if (Platform == PLATFORM_Android)
			{
				// make sure the cached data is loaded
				for (INT MipLevel = 0; MipLevel < Texture2D->Mips.Num(); MipLevel++)
				{
					Texture2D->Mips(MipLevel).Data.MakeSureBulkDataIsLoaded();
				}

				// On Android we need to copy the uncompressed data to the other mip caches being cooked
				if (AndroidTextureFormat & TEXSUPPORT_PVRTC)
				{
					Texture2D->CachedPVRTCMips = Texture2D->Mips;
				}
				if (AndroidTextureFormat & TEXSUPPORT_ATITC)
				{
					Texture2D->CachedATITCMips = Texture2D->Mips;
				}
				if (AndroidTextureFormat & TEXSUPPORT_ETC)
				{
					Texture2D->CachedETCMips = Texture2D->Mips;
				}
			}

			// no single format platforms need the cached PVRTC data at this point (IPhone has already copied them over as needed)
			if (Platform != PLATFORM_Android)
			{
				Texture2D->CachedPVRTCMips.Empty();
				Texture2D->CachedATITCMips.Empty();
				Texture2D->CachedETCMips.Empty();
			}
			else
			{
				// discard caches that won't be processed, but keep those that will
				if (!(AndroidTextureFormat & TEXSUPPORT_DXT))
				{
					Texture2D->Mips.Empty();
				}
				if (!(AndroidTextureFormat & TEXSUPPORT_PVRTC))
				{
					Texture2D->CachedPVRTCMips.Empty();
				}
				if (!(AndroidTextureFormat & TEXSUPPORT_ATITC))
				{
					Texture2D->CachedATITCMips.Empty();
				}
				if (!(AndroidTextureFormat & TEXSUPPORT_ETC))
				{
					Texture2D->CachedETCMips.Empty();
				}
			}
			
			Texture2D->CachedFlashMips.RemoveBulkData();
			// if we don't do this, then consoles try to load too much of the tossed bulk data (it will even crash 
			// if the Texture2D's FirstResourceMemMip is non-0)
			Texture2D->CachedFlashMips.StoreInSeparateFile( 
				TRUE,
				BULKDATA_Unused,
				0,
				INDEX_NONE,
				INDEX_NONE );


			// We don't want to stream texture that require on load conversion, the cooker needs to be aware of this.
			if( UTexture2D::GetEffectivePixelFormat((const EPixelFormat)Texture2D->Format, Texture2D->SRGB, GCookingTarget) != (const EPixelFormat)Texture2D->Format )
			{
				Texture2D->NeverStream = TRUE;
			}

			if ( Platform == PLATFORM_PS3 && Texture2D->LODGroup == TEXTUREGROUP_Terrain_Heightmap )
			{
				// Remove height data (R, G) from texture and make texture as U8V8 format
				for (INT Mip = 0; Mip < Texture2D->Mips.Num(); Mip++ )
				{
					INT MipSizeU = Texture2D->SizeX >> Mip;
					INT MipSizeV = Texture2D->SizeY >> Mip;

					FColor* MipMap = (FColor*)Texture2D->Mips(Mip).Data.Lock(LOCK_READ_WRITE);
					TArray<FColor> OriginalData;
					OriginalData.Init(MipSizeU*MipSizeV);
					appMemcpy( &OriginalData(0), MipMap, MipSizeU*MipSizeV*sizeof(FColor) );

					WORD* NormalMip = (WORD*)Texture2D->Mips(Mip).Data.Realloc(MipSizeU*MipSizeV*sizeof(WORD));

					for (INT Idx = 0; Idx < OriginalData.Num(); ++Idx)
					{
						// We have to put this into 2's compilment form as that's what PF_V8U8 is on PC.
						// CookMip will reverse that as the PS3 does not expect it.
						SBYTE SignedX = (INT)OriginalData(Idx).A - 128;
						SBYTE SignedY = (INT)OriginalData(Idx).B - 128;
						NormalMip[Idx] = ((WORD)*((BYTE*)&SignedX)) << 8 | (*((BYTE*)&SignedY));
					}

					Texture2D->Mips(Mip).Data.Unlock();
				}

				Texture2D->Format = PF_V8U8;
				// Indicate we need normal unbiasing.
				Texture2D->CompressionSettings = TC_NormalmapUncompressed;
			}

			// Take texture LOD settings into account, avoiding cooking and keeping memory around for unused miplevels.
			INT	FirstMipIndex;
			if (Platform != PLATFORM_Android)
			{
				FirstMipIndex = Clamp<INT>( PlatformLODSettings.CalculateLODBias( Texture2D ) - Texture2D->NumCinematicMipLevels, 0, Texture2D->Mips.Num()-1 );
			}
			else
			{
				// Need to check all the caches when supporting multiple formats
				INT NumMips = Max(0, Texture2D->Mips.Num());
				NumMips = Max(NumMips,  Texture2D->CachedPVRTCMips.Num());
				NumMips = Max(NumMips,  Texture2D->CachedATITCMips.Num());
				NumMips = Max(NumMips,  Texture2D->CachedETCMips.Num());
				FirstMipIndex = Clamp<INT>( PlatformLODSettings.CalculateLODBias( Texture2D ) - Texture2D->NumCinematicMipLevels, 0, NumMips-1 );
			}

			// make sure we load at least the first packed mip level
			FirstMipIndex = Min(FirstMipIndex, Texture2D->MipTailBaseIdx);

			// Strip out miplevels for UI textures.
			if( Texture2D->LODGroup == TEXTUREGROUP_UI )
			{
				if (Platform != PLATFORM_Android)
				{
					StripUIMips(Texture2D, Texture2D->Mips, FirstMipIndex);
				}
				else
				{
					// Perform this for each format if on multi-format platform
					if (AndroidTextureFormat & TEXSUPPORT_DXT)
					{
						StripUIMips(Texture2D, Texture2D->Mips, FirstMipIndex);
					}
					if (AndroidTextureFormat & TEXSUPPORT_PVRTC)
					{
						StripUIMips(Texture2D, Texture2D->CachedPVRTCMips, FirstMipIndex);
					}
					if (AndroidTextureFormat & TEXSUPPORT_ATITC)
					{
						StripUIMips(Texture2D, Texture2D->CachedATITCMips, FirstMipIndex);
					}
					if (AndroidTextureFormat & TEXSUPPORT_ETC)
					{
						StripUIMips(Texture2D, Texture2D->CachedETCMips, FirstMipIndex);
					}
				}
			}

			if( Platform == PLATFORM_PS3 || Platform == PLATFORM_Xbox360 || Platform == PLATFORM_WiiU)
			{
				// pack mips on the xenon
				UBOOL bPackMipTails = Platform == PLATFORM_Xbox360;

				// get the texture gamma conversion setting
				UBOOL bShouldConvertPWLGamma;
				GConfig->GetBool(TEXT("Engine.PWLGamma"), TEXT("bShouldConvertPWLGamma"), bShouldConvertPWLGamma, PlatformEngineConfigFilename);

				// generate creation flags
				DWORD CreationFlags = 
					((bPackMipTails ? 0 : TextureCreate_NoPackedMip)) | 
					((bShouldConvertPWLGamma && Texture2D->SRGB) ? TextureCreate_PWLGamma : 0);

				// Initialize texture cooker for given format and size.
				TextureCooker->Init(Texture2D->Format, Texture2D->SizeX, Texture2D->SizeY, Texture2D->Mips.Num(), CreationFlags);

				// first level of the packed miptail
				Texture2D->MipTailBaseIdx = TextureCooker->GetMipTailBase();

				// Only cook mips up to the first packed mip level
				INT MaxMipLevel = Texture2D->Mips.Num();
				if (bPackMipTails)
				{
					MaxMipLevel = Min(Texture2D->MipTailBaseIdx,MaxMipLevel);
				}		

				if (!bSkipCookerOperations)
				{
					for( INT MipLevel=FirstMipIndex; MipLevel<MaxMipLevel; MipLevel++ )
					{
						FTexture2DMipMap& Mip = Texture2D->Mips(MipLevel);

						// Allocate enough memory for cooked miplevel.
						UINT	MipSize				= TextureCooker->GetMipSize( MipLevel );
						// a size of 0 means to use original data size as dest size
						if (MipSize == 0)
						{
							MipSize = Mip.Data.GetBulkDataSize();
						}

						void*	IntermediateData	= appMalloc( MipSize );
						appMemzero(IntermediateData, MipSize);

						UINT	SrcRowPitch			= Max<UINT>( 1,	(Texture2D->SizeX >> MipLevel) / GPixelFormats[Texture2D->Format].BlockSizeX ) 
							* GPixelFormats[Texture2D->Format].BlockBytes;
						// Resize upfront to new size to work around issue in Xbox 360 texture cooker reading memory out of bounds.
						// zero-out the newly allocated block of memory as we may not end up using it all
						Mip.Data.Lock(LOCK_READ_WRITE);

						// remember the size of the buffer before realloc
						const INT SizeBeforeRealloc = Mip.Data.GetBulkDataSize();
						void*	MipData				= Mip.Data.Realloc( MipSize );

						// get the size of the newly allocated region
						const INT SizeOfReallocRegion = Mip.Data.GetBulkDataSize() - SizeBeforeRealloc;
						if ( SizeOfReallocRegion > 0 )
						{
							appMemzero((BYTE*)MipData + SizeBeforeRealloc, SizeOfReallocRegion);
						}

						// Cook the miplevel into the intermediate memory.
						TextureCooker->CookMip( MipLevel, MipData, IntermediateData, SrcRowPitch );

						// And replace existing data.
						appMemcpy( MipData, IntermediateData, MipSize );
						appFree( IntermediateData );
						Mip.Data.Unlock();
					}

					// Cook the miptail. This will be the last mip level of the texture
					if( bPackMipTails && Texture2D->MipTailBaseIdx < Texture2D->Mips.Num() )
					{
						// Should always be a multiple of the tile size for this texture's format			
						UINT MipTailSize = TextureCooker->GetMipSize(Texture2D->MipTailBaseIdx);

						// Source mip data for base level
						FTexture2DMipMap& BaseLevelMip = Texture2D->Mips(Texture2D->MipTailBaseIdx);

						// Allocate space for the mip tail
						void* DstMipTail = new BYTE[MipTailSize];
						appMemzero(DstMipTail, MipTailSize);

						// Arrays to hold the data for the tail mip levels
						const INT TailMipLevelCount = Texture2D->Mips.Num() - Texture2D->MipTailBaseIdx;
						void** SrcMipTailData = new void*[TailMipLevelCount];
						UINT* SrcMipPitch = new UINT[TailMipLevelCount];
						appMemzero(SrcMipPitch, TailMipLevelCount * sizeof(UINT));

						// Build up arrays of data to send to the MipTail cooker
						for( INT MipLevel = Texture2D->MipTailBaseIdx; MipLevel < Texture2D->Mips.Num(); MipLevel++ )
						{
							// source mip data for this MipLevel
							FTexture2DMipMap& Mip = Texture2D->Mips(MipLevel);
							// surface pitch for this mip level
							SrcMipPitch[MipLevel - Texture2D->MipTailBaseIdx] = Max<UINT>( 1, (Texture2D->SizeX >> MipLevel) / GPixelFormats[Texture2D->Format].BlockSizeX ) 
								* GPixelFormats[Texture2D->Format].BlockBytes;

							// lock source data for use. realloc to MipTailSize to work around issue with Xbox 360 texture cooker.
							// zero-out the newly allocated block of memory as we may not end up using it all
							Mip.Data.Lock(LOCK_READ_WRITE);

							// remember the size of the buffer before realloc
							const INT SizeBeforeRealloc = Mip.Data.GetBulkDataSize();
							void* MipData = Mip.Data.Realloc(MipTailSize);

							// get the size of the newly allocated region
							const INT SizeOfReallocRegion = Mip.Data.GetBulkDataSize() - SizeBeforeRealloc;
							if ( SizeOfReallocRegion > 0 )
							{
								appMemzero((BYTE*)MipData + SizeBeforeRealloc, SizeOfReallocRegion);
							}

							SrcMipTailData[MipLevel - Texture2D->MipTailBaseIdx] = MipData;
						}

						// Cook the tail mips together
						TextureCooker->CookMipTail( SrcMipTailData, SrcMipPitch, DstMipTail );

						// We can potentially save some memory at the end of a packed mip tail.
						UINT AdjustedMipTailSize = MipTailSize;
						if ( Platform == PLATFORM_Xbox360 )
						{
							UINT UnusedMipTailSize = XeCalcUnusedMipTailSize(Texture2D->SizeX, Texture2D->SizeY, EPixelFormat(Texture2D->Format), Texture2D->Mips.Num(), TRUE);
							AdjustedMipTailSize = MipTailSize - UnusedMipTailSize;
#if DO_CHECK
							// Lets make sure the extra memory is all zero (and unused)
							if ( UnusedMipTailSize > 0 )
							{
								check( MipTailSize > UnusedMipTailSize );
								BYTE *Test = (BYTE *)DstMipTail;
								for (DWORD Offset = AdjustedMipTailSize; Offset < MipTailSize; Offset++)
								{
									check( Test[Offset] == 0 );
								}
							}
#endif
						}

						// And replace existing data. Base level is already locked for writing
						appMemcpy( BaseLevelMip.Data.Realloc( AdjustedMipTailSize ), DstMipTail, AdjustedMipTailSize );
						delete [] DstMipTail;

						// Unlock the src mip data
						for( INT MipLevel = Texture2D->MipTailBaseIdx+1; MipLevel < Texture2D->Mips.Num(); MipLevel++ )
						{
							FTexture2DMipMap& Mip = Texture2D->Mips(MipLevel);
							// Clear out unused tail mips.
							Mip.Data.Realloc(0);
							Mip.Data.Unlock();
						}

						BaseLevelMip.Data.Unlock();

						delete [] SrcMipTailData;
						delete [] SrcMipPitch;
					}				
				}
			}
			else
			{
				// Whether the texture can be streamed.
				UBOOL	bIsStreamingTexture	= !Texture2D->NeverStream;
				// Whether the texture is applied to the face of a cubemap, in which case we can't stream it.
				UBOOL	bIsCubemapFace		= Texture2D->GetOuter()->IsA(UTextureCube::StaticClass());

				// make sure we load at least the first packed mip level
				FirstMipIndex = Min(FirstMipIndex, Texture2D->MipTailBaseIdx);

				// Textures residing in seekfree packages cannot be streamed as the file offset is not known till after
				// they are saved.
				if( ShouldBeANeverStreamTexture( Texture2D, bIsSavedInSeekFreePackage ) == TRUE )
				{
					bIsStreamingTexture		= FALSE;
					Texture2D->NeverStream	= TRUE;
				}

				// Make sure data is (and remains) loaded for (and after) saving.
				if (Platform != PLATFORM_Android)
				{
					EnsureMipsLoaded(Texture2D, Texture2D->Mips, FirstMipIndex, bIsSavedInSeekFreePackage, bIsStreamingTexture);
				}
				else
				{
					// Perform this for each format if on multi-format platform
					if (AndroidTextureFormat & TEXSUPPORT_DXT)
					{
						EnsureMipsLoaded(Texture2D, Texture2D->Mips, FirstMipIndex, bIsSavedInSeekFreePackage, bIsStreamingTexture);
					}
					if (AndroidTextureFormat & TEXSUPPORT_PVRTC)
					{
						EnsureMipsLoaded(Texture2D, Texture2D->CachedPVRTCMips, FirstMipIndex, bIsSavedInSeekFreePackage, bIsStreamingTexture);
					}
					if (AndroidTextureFormat & TEXSUPPORT_ATITC)
					{
						EnsureMipsLoaded(Texture2D, Texture2D->CachedATITCMips, FirstMipIndex, bIsSavedInSeekFreePackage, bIsStreamingTexture);
					}
					if (AndroidTextureFormat & TEXSUPPORT_ETC)
					{
						EnsureMipsLoaded(Texture2D, Texture2D->CachedETCMips, FirstMipIndex, bIsSavedInSeekFreePackage, bIsStreamingTexture);
					}
				}
			}
		}
		else
		{
			warnf(NAME_Warning, TEXT("%s has no mips. Skipping."), *Texture2D->GetFullName());
		}
	}
}

/**
* Check the first bytes of the movie stream for a valid signature
*
* @param Buffer - movie stream buffer including header
* @param BufferSize - total size of movie stream buffer
* @param Signature - signature to compare against
* @param SignatureSize - size of the signature buffer
* @return TRUE if success
*/
UBOOL IsMovieSignatureValid(void* Buffer,INT BufferSize,BYTE* Signature,INT SignatureSize)
{
	UBOOL Result = TRUE;
	// need at least enough room for the signature
	if ( BufferSize >= SignatureSize )
	{
		BYTE* BufferSignature = static_cast<BYTE*>(Buffer);
		// make sure there is a matching signature in the buffer
		for( INT i=0; i < SignatureSize; ++i )
		{
			if( Signature[i] != BufferSignature[i] )
			{
				Result = FALSE;
				break;
			}
		}
	}
	return Result;
}

/**
* Byte swap raw movie data byte stream 
*
* @param Buffer - movie stream buffer including header
* @param BufferSize - total size of movie stream buffer
*/
void EnsureMovieEndianess(void* Buffer, INT BufferSize)
{
	// endian swap the data
	DWORD* Data = static_cast<DWORD*>(Buffer);
	UINT   DataSize = BufferSize / 4;
	for (UINT DataIndex = 0; DataIndex < DataSize; ++DataIndex)
	{
		DWORD SourceData = Data[DataIndex];
		Data[DataIndex] = ((SourceData & 0x000000FF) << 24) |
			((SourceData & 0x0000FF00) <<  8) |
			((SourceData & 0x00FF0000) >>  8) |
			((SourceData & 0xFF000000) >> 24) ;
	}
}

/**
* Helper function used by CookObject - performs movie specific cooking.
*
* @param	TextureMovie	Movie texture to cook
*/
void UCookPackagesCommandlet::CookMovieTexture( UTextureMovie* TextureMovie )
{
	check(TextureMovie);
	if( !TextureMovie->IsTemplate(RF_ClassDefaultObject) )
	{
		if( ShouldByteSwapData() )
		{
			SCOPE_SECONDS_COUNTER_COOK(CookMovieTime);

			// load the movie stream data
			void* Buffer = TextureMovie->Data.Lock(LOCK_READ_WRITE);
			INT BufferSize = TextureMovie->Data.GetBulkDataSize();
			UBOOL Result = FALSE;
			if( TextureMovie->DecoderClass == UCodecMovieBink::StaticClass() )
			{
				// check for a correct signature in the movie stream
				BYTE Signature[]={'B','I','K'};
				if( IsMovieSignatureValid(Buffer,BufferSize,Signature,ARRAY_COUNT(Signature)) )
				{
					// byte swap the data
					EnsureMovieEndianess(Buffer,BufferSize);
					Result = TRUE;
				}
			}
			else
			{
				warnf(NAME_Error, TEXT("Codec type [%s] not implemented! Removing movie data."),
					TextureMovie->DecoderClass ? *TextureMovie->DecoderClass->GetName() : TEXT("None"));
			}
			TextureMovie->Data.Unlock();
			if( !Result )
			{
				// invalid movie type so remove its data
				TextureMovie->Data.RemoveBulkData();
			}			
		}
	}
}

/**
 * Helper function used by CookObject - performs brush component specific cooking.
 *
 * @param	Package						Package going to be saved
 * @param	BrushComponent				Brush component to cook
 * @param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
 */
void UCookPackagesCommandlet::CookBrushComponent( UPackage* Package, UBrushComponent* BrushComponent, UBOOL bIsSavedInSeekFreePackage )
{
	SCOPE_SECONDS_COUNTER_COOK(CookPhysicsTime);

	// Rebuild physics brush data for BrushComponents that are part of Volumes (e.g., blocking volumes)
	AActor* Owner = BrushComponent->GetOwner();
	if (!BrushComponent->IsTemplate() && (Owner != NULL) && Owner->IsVolumeBrush())
	{
		BrushComponent->BuildSimpleBrushCollision();
		BrushComponent->BuildPhysBrushData();

		// Add to memory used total.
		const INT NumConvexElements = BrushComponent->CachedPhysBrushData.CachedConvexElements.Num();
		for(INT HullIdx = 0; HullIdx < NumConvexElements; HullIdx++)
		{
			FKCachedConvexDataElement& Hull = BrushComponent->CachedPhysBrushData.CachedConvexElements(HullIdx);
			BrushPhysDataBytes += Hull.ConvexElementData.Num();
		}

		if (bStripBrushesWithCachedPhysicsData && !BrushComponent->bDisableAllRigidBody)
		{
			AVolume* Volume = Owner->GetAVolume();
			check(Volume);
			UModel* Brush = Volume->Brush;

			if (Brush != NULL)
			{
				if ((NumConvexElements > 0) && (BrushComponent->BrushAggGeom.GetElementCount() > 0))
				{
					// Remove the brush from being saved
					Brush->ClearFlags(RF_ForceTagExp);
					Brush->SetFlags(RF_MarkedByCooker);

					Volume->Brush = NULL;
					BrushComponent->Brush = NULL;
				}
				else
				{
					warnf(NAME_Warning, TEXT("\tFailed to strip volume brush %s belonging to %s because it creates no cached physics geometry (brush has %d polys)"),
						*Brush->GetFullName(),
						*BrushComponent->GetFullName(),
						(Brush->Polys != NULL) ? Brush->Polys->Element.Num() : 0);
				}
			}
		}
	}
}

/**
 * Helper function used by CookObject - performs sound cue specific cooking.
 */
void UCookPackagesCommandlet::CookSoundCue( USoundCue* SoundCue )
{
	SCOPE_SECONDS_COUNTER_COOK( CookSoundCueTime );

	if (SoundCue && SoundCue->FaceFXAnimSetRef && 
		PersistentFaceFXAnimSetGenerator.GetGeneratePersistentMapAnimSet())
	{
		if (PersistentFaceFXAnimSetGenerator.ShouldIncludeToPersistentMapFaceFXAnimation(SoundCue->FaceFXAnimSetRef->GetPathName()))
		{
			// Do not allow the FaceFXAnimSetRef to cook into anything!
			SoundCue->FaceFXAnimSetRef->ClearFlags(RF_ForceTagExp);
			SoundCue->FaceFXAnimSetRef->SetFlags(RF_MarkedByCooker|RF_Transient|RF_MarkedByCookerTemp);
			if (PersistentFaceFXAnimSetGenerator.GetLogPersistentFaceFXGeneration())
			{
				warnf(NAME_Log, TEXT("CookSoundCue> Cleared FaceFXAnimSet %s for sound cue %s"),
					*(SoundCue->FaceFXAnimSetRef->GetPathName()),
					*(SoundCue->GetPathName()));
			}
		}
		else
		{
			if (PersistentFaceFXAnimSetGenerator.GetStripAnimSetReferences())
			{
				if (PersistentFaceFXAnimSetGenerator.GetLogPersistentFaceFXGeneration())
				{
					warnf(NAME_Log, TEXT("CookSoundCue> Stripped FaceFXAnimSet %s for sound cue %s"),
						*(SoundCue->FaceFXAnimSetRef->GetPathName()),
						*(SoundCue->GetPathName()));
				}
				// Clear this specific ref, but allow the package to be cooked normally
				SoundCue->FaceFXAnimSetRef = NULL;
			}
			else if (PersistentFaceFXAnimSetGenerator.GetLogPersistentFaceFXGeneration())
			{
				warnf(NAME_Log, TEXT("CookSoundCue> Allowing FaceFXAnimSet %s for sound cue %s"),
					*(SoundCue->FaceFXAnimSetRef->GetPathName()),
					*(SoundCue->GetPathName()));
			}
		}

		if (PersistentFaceFXAnimSetGenerator.GetGeneratePersistentMapAnimSet() && PersistentFaceFXAnimSetGenerator.IsCurrentPMapCacheValid())
		{
			FString AnimSetRefName = SoundCue->FaceFXAnimSetRef->GetPathName();
			FString GroupName = SoundCue->FaceFXGroupName;
			FString AnimName = SoundCue->FaceFXAnimName;
			FPersistentFaceFXAnimSetGenerator::FMangledFaceFXInfo* MangledFFXInfo = FPersistentFaceFXAnimSetGenerator::FindMangledFaceFXInfo( 
					PersistentFaceFXAnimSetGenerator.CurrentPMapGroupAnimLookupMap, AnimSetRefName, GroupName, AnimName);
			if (MangledFFXInfo)
			{
				// Fix up the names!
				if ((MangledFFXInfo->OriginalFaceFXAnimName == AnimName) &&
					(MangledFFXInfo->OriginalFaceFXGroupName == GroupName))
				{
					if (PersistentFaceFXAnimSetGenerator.GetLogPersistentFaceFXGeneration())
					{
						warnf(NAME_Log, TEXT("FaceFX mangling on SoundCue %s"), *(SoundCue->GetPathName()));
						warnf(NAME_Log, TEXT("\t%s-%s --> %s-%s"), 
							*GroupName, *AnimName,
							*(MangledFFXInfo->MangledFaceFXGroupName), *(MangledFFXInfo->MangledFaceFXAnimName)
							);
					}
					SoundCue->FaceFXAnimName = MangledFFXInfo->MangledFaceFXAnimName;
					SoundCue->FaceFXGroupName = MangledFFXInfo->MangledFaceFXGroupName;
				}
				else
				{
					warnf(NAME_Warning, TEXT("Mismatched FaceFX mangling on SoundCue %s"), *(SoundCue->GetPathName()));
					warnf(NAME_Warning, TEXT("\tExpected %s-%s, found %s-%s"), 
						*GroupName, *AnimName,
						*(MangledFFXInfo->OriginalFaceFXGroupName), *(MangledFFXInfo->OriginalFaceFXAnimName)
						);
				}
			}
			else
			{
				if (PersistentFaceFXAnimSetGenerator.GetLogPersistentFaceFXGeneration())
				{
					warnf(NAME_Log, TEXT("CookSoundCue> Did not find %s.%s in PMap list"), *GroupName, *AnimName);
				}
			}
		}
	}
}

/**
 * Helper function used by CookObject - performs InterpTrackFaceFX specific cooking.
 */
void UCookPackagesCommandlet::CookFaceFXInterpTrack(UInterpTrackFaceFX* FaceFXInterpTrack)
{
	// Are we cooking persistent FaceFX?
	if (PersistentFaceFXAnimSetGenerator.GetGeneratePersistentMapAnimSet() && PersistentFaceFXAnimSetGenerator.IsCurrentPMapCacheValid())
	{
		//FaceFXInterpTrack->UpdateFaceFXSoundCueReferences()
		for (INT SeqIndex = 0; SeqIndex < FaceFXInterpTrack->FaceFXSeqs.Num(); SeqIndex++)
		{
			FFaceFXTrackKey& TrackKey = FaceFXInterpTrack->FaceFXSeqs(SeqIndex);

			FString GroupName = TrackKey.FaceFXGroupName;
			FString AnimName = TrackKey.FaceFXSeqName;

			UBOOL bFound = FALSE;

			INT SetIndex = FPersistentFaceFXAnimSetGenerator::FindSourceAnimSet(FaceFXInterpTrack->FaceFXAnimSets, GroupName, AnimName);
			if (SetIndex != -1)
			{
				UFaceFXAnimSet* TheAnimSet = FaceFXInterpTrack->FaceFXAnimSets(SetIndex);
				if (TheAnimSet && PersistentFaceFXAnimSetGenerator.ShouldIncludeToPersistentMapFaceFXAnimation(TheAnimSet->GetPathName()))
				{
					TheAnimSet->ClearFlags(RF_ForceTagExp);
					TheAnimSet->SetFlags(RF_MarkedByCooker|RF_Transient|RF_MarkedByCookerTemp);
					if (PersistentFaceFXAnimSetGenerator.GetLogPersistentFaceFXGeneration())
					{
						warnf(NAME_Log, TEXT("CookFaceFXInterpTrack> Cleared FaceFXAnimSet %s for %s"),
							*(TheAnimSet->GetPathName()),
							*(FaceFXInterpTrack->GetPathName()));
					}

					FString AnimSetRefName = TheAnimSet->GetPathName();
					FPersistentFaceFXAnimSetGenerator::FMangledFaceFXInfo* MangledFFXInfo = FPersistentFaceFXAnimSetGenerator::FindMangledFaceFXInfo( 
						PersistentFaceFXAnimSetGenerator.CurrentPMapGroupAnimLookupMap, AnimSetRefName, GroupName, AnimName);
					if (MangledFFXInfo)
					{
						// Fix up the names!
						if ((MangledFFXInfo->OriginalFaceFXAnimName == AnimName) &&
							(MangledFFXInfo->OriginalFaceFXGroupName == GroupName))
						{
							if (PersistentFaceFXAnimSetGenerator.GetLogPersistentFaceFXGeneration())
							{
								warnf(NAME_Log, TEXT("FaceFX mangling on InterTrack %d-%s"), SeqIndex, *(FaceFXInterpTrack->GetPathName()));
								warnf(NAME_Log, TEXT("\t%s-%s --> %s-%s"), 
									*(TrackKey.FaceFXGroupName), *(TrackKey.FaceFXSeqName),
									*(MangledFFXInfo->MangledFaceFXGroupName), *(MangledFFXInfo->MangledFaceFXAnimName)
									);
							}
							TrackKey.FaceFXGroupName = MangledFFXInfo->MangledFaceFXGroupName;
							TrackKey.FaceFXSeqName = MangledFFXInfo->MangledFaceFXAnimName;
						}
						else
						{
							warnf(NAME_Warning, TEXT("Mismatched FaceFX mangling on InterTrack %d-%s"), SeqIndex, *(FaceFXInterpTrack->GetPathName()));
							warnf(NAME_Warning, TEXT("\tExpected %s-%s, found %s-%s"), 
								*(TrackKey.FaceFXGroupName), *(TrackKey.FaceFXSeqName),
								*(MangledFFXInfo->OriginalFaceFXGroupName), *(MangledFFXInfo->OriginalFaceFXAnimName)
								);
						}
					}
					else
					{
						if (PersistentFaceFXAnimSetGenerator.GetLogPersistentFaceFXGeneration())
						{
							warnf(NAME_Log, TEXT("CookFaceFXInterpTrack> Did not find %s.%s in PMap list"), *GroupName, *AnimName);
						}
					}
				}
				else
				{
					if (PersistentFaceFXAnimSetGenerator.GetLogPersistentFaceFXGeneration())
					{
						warnf(NAME_Log, TEXT("CookInterpTrackFaceFX> Allowing FaceFXAnimSet %s for %s"),
							*(TheAnimSet->GetPathName()),
							*(FaceFXInterpTrack->GetPathName()));
					}
				}
			}
		}

		// Catch unused FaceFXAnimSets...
		for (INT AnimSetIndex = 0; AnimSetIndex < FaceFXInterpTrack->FaceFXAnimSets.Num(); AnimSetIndex++)
		{
			UFaceFXAnimSet* TheAnimSet = FaceFXInterpTrack->FaceFXAnimSets(AnimSetIndex);
			if (TheAnimSet && PersistentFaceFXAnimSetGenerator.ShouldIncludeToPersistentMapFaceFXAnimation(TheAnimSet->GetPathName()))
			{
				TheAnimSet->ClearFlags(RF_ForceTagExp);
				TheAnimSet->SetFlags(RF_MarkedByCooker|RF_Transient|RF_MarkedByCookerTemp);
				if (PersistentFaceFXAnimSetGenerator.GetLogPersistentFaceFXGeneration())
				{
					warnf(NAME_Log, TEXT("CookFaceFXInterpTrack> Cleared FaceFXAnimSet %s for %s"),
						*(TheAnimSet->GetPathName()),
						*(FaceFXInterpTrack->GetPathName()));
				}
			}
		}
	}
}

/**
 * Helper function used by CookObject - performs USeqAct_PlayFaceFXAnim specific cooking.
 */
void UCookPackagesCommandlet::CookSeqActPlayFaceFXAnim(USeqAct_PlayFaceFXAnim* SeqAct_PlayFaceFX)
{
	if (PersistentFaceFXAnimSetGenerator.GetGeneratePersistentMapAnimSet() && PersistentFaceFXAnimSetGenerator.IsCurrentPMapCacheValid())
	{
		if (SeqAct_PlayFaceFX && SeqAct_PlayFaceFX->FaceFXAnimSetRef)
		{
			if (PersistentFaceFXAnimSetGenerator.ShouldIncludeToPersistentMapFaceFXAnimation(SeqAct_PlayFaceFX->FaceFXAnimSetRef->GetPathName()))
			{
				// Do not allow the FaceFXAnimSetRef to cook into anything!
				SeqAct_PlayFaceFX->FaceFXAnimSetRef->ClearFlags(RF_ForceTagExp);
				SeqAct_PlayFaceFX->FaceFXAnimSetRef->SetFlags(RF_MarkedByCooker|RF_Transient|RF_MarkedByCookerTemp);
				if (PersistentFaceFXAnimSetGenerator.GetLogPersistentFaceFXGeneration())
				{
					warnf(NAME_Log, TEXT("CookSeqActPlayFaceFXAnim> Cleared FaceFXAnimSet %s for %s"),
						*(SeqAct_PlayFaceFX->FaceFXAnimSetRef->GetPathName()),
						*(SeqAct_PlayFaceFX->GetPathName()));
				}
			}
			else
			{
				if (PersistentFaceFXAnimSetGenerator.GetLogPersistentFaceFXGeneration())
				{
					warnf(NAME_Log, TEXT("CookSeqActPlayFaceFXAnim> Allowing FaceFXAnimSet %s for %s"),
						*(SeqAct_PlayFaceFX->FaceFXAnimSetRef->GetPathName()),
						*(SeqAct_PlayFaceFX->GetPathName()));
				}
			}

			FString AnimSetRefName = SeqAct_PlayFaceFX->FaceFXAnimSetRef->GetPathName();
			FString GroupName = SeqAct_PlayFaceFX->FaceFXGroupName;
			FString AnimName = SeqAct_PlayFaceFX->FaceFXAnimName;
			FPersistentFaceFXAnimSetGenerator::FMangledFaceFXInfo* MangledFFXInfo = FPersistentFaceFXAnimSetGenerator::FindMangledFaceFXInfo( 
				PersistentFaceFXAnimSetGenerator.CurrentPMapGroupAnimLookupMap, AnimSetRefName, GroupName, AnimName );
			if (MangledFFXInfo)
			{
				// Fix up the names!
				if ((MangledFFXInfo->OriginalFaceFXAnimName == AnimName) &&
					(MangledFFXInfo->OriginalFaceFXGroupName == GroupName))
				{
					if (PersistentFaceFXAnimSetGenerator.GetLogPersistentFaceFXGeneration())
					{
						warnf(NAME_Log, TEXT("FaceFX mangling on SeqActPlayFaceFXAnim %s"), *(SeqAct_PlayFaceFX->GetPathName()));
						warnf(NAME_Log, TEXT("\t%s-%s --> %s-%s"), 
							*GroupName, *AnimName,
							*(MangledFFXInfo->MangledFaceFXGroupName), *(MangledFFXInfo->MangledFaceFXAnimName)
							);
					}
					SeqAct_PlayFaceFX->FaceFXAnimName = MangledFFXInfo->MangledFaceFXAnimName;
					SeqAct_PlayFaceFX->FaceFXGroupName = MangledFFXInfo->MangledFaceFXGroupName;
				}
				else
				{
					warnf(NAME_Warning, TEXT("Mismatched FaceFX mangling on SeqActPlayFaceFXAnim %s"), *(SeqAct_PlayFaceFX->GetPathName()));
					warnf(NAME_Warning, TEXT("\tExpected %s-%s, found %s-%s"), 
						*GroupName, *AnimName,
						*(MangledFFXInfo->OriginalFaceFXGroupName), *(MangledFFXInfo->OriginalFaceFXAnimName)
						);
				}
			}
			else
			{
				if (PersistentFaceFXAnimSetGenerator.GetLogPersistentFaceFXGeneration())
				{
					warnf(NAME_Log, TEXT("CookSeqActPlayFaceFXAnim> Did not find %s.%s in PMap list"), *GroupName, *AnimName);
				}
			}
		}
	}
}

/**
* Helper function used by CookObject - performs FaceFX cooking.
*/
void UCookPackagesCommandlet::LogFaceFXAnimSet(UFaceFXAnimSet* FaceFXAnimSet, UPackage* Package)
{
#if WITH_FACEFX
	if ( bAnalyzeReferencedContent )
	{
		FAnalyzeReferencedContentStat::FFaceFXAnimSetStats* FaceFXAnimSetStat= ReferencedContentStat.GetFaceFXAnimSetStats( FaceFXAnimSet );

		if (Package->ContainsMap())
		{
			// this is map package
			FaceFXAnimSetStat->AddLevelInfo( Package, TRUE );
		}
		else
		{
			FaceFXAnimSetStat->bIsReferencedByScript = TRUE;
		}
	}
#endif
}
/**
 * Helper function used by CookSoundNodeWave - performs sound specific cooking.
 */
void UCookPackagesCommandlet::CookSoundNodeWave( USoundNodeWave* SoundNodeWave )
{
	SCOPE_SECONDS_COUNTER_COOK( CookSoundTime );

	if (Platform == PLATFORM_Android)
	{
		// export the raw uncompressed data as a wav
		if (SoundNodeWave->RawData.GetBulkDataSize() > 0)
		{
			// figure where to write the wav to
			FString WaveFilename = (appGameDir() + TEXT("Audio")) * SoundNodeWave->GetPathName() + TEXT(".wav");

			// save out the raw wave data to be played back from SD card
			FArchive* WaveFile = GFileManager->CreateFileWriter(*WaveFilename);
			if (WaveFile)
			{
				void* RawWaveData = SoundNodeWave->RawData.Lock(LOCK_READ_ONLY);
				WaveFile->Serialize( RawWaveData, SoundNodeWave->RawData.GetBulkDataSize() );
				SoundNodeWave->RawData.Unlock();
				delete WaveFile;
			}
		}
	}
	else
	{
		// Ensure the current platforms are cooked
		::CookSoundNodeWave( SoundNodeWave, Platform );
	}
}

/**
 * Helper function used by CookSoundNodeWave - localises sound
 */
void UCookPackagesCommandlet::LocSoundNodeWave( USoundNodeWave* SoundNodeWave )
{
	SCOPE_SECONDS_COUNTER_COOK( LocSoundTime );

	if (SoundNodeWave->LocalizedSubtitles.Num())
	{
		return;
	}

	// Setup the localization if needed
	if (SoundNodeWave->IsLocalizedResource())
	{
		// Get a list of known language extensions
		const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();

		FString SavedLangExt = UObject::GetLanguage();
		INT SavedIndex = Localization_GetLanguageExtensionIndex(*SavedLangExt);

		SoundNodeWave->LocalizedSubtitles.Empty();
		SoundNodeWave->LocalizedSubtitles.AddZeroed(KnownLanguageExtensions.Num());

		// Fill in the items...
		if( SavedIndex != -1 )
		{
			FLocalizedSubtitle& LocSubtitle = SoundNodeWave->LocalizedSubtitles(SavedIndex);
			// Copy it into the appropriate slot
			LocSubtitle.bManualWordWrap = SoundNodeWave->bManualWordWrap;
			LocSubtitle.bMature = SoundNodeWave->bMature;
			LocSubtitle.bSingleLine = SoundNodeWave->bSingleLine;
			LocSubtitle.Subtitles = SoundNodeWave->Subtitles;
		}

		if (bCookCurrentLanguageOnly == FALSE)
		{
			for (INT KnownLangIndex = 0; KnownLangIndex < KnownLanguageExtensions.Num(); KnownLangIndex++)
			{
				if (MultilanguageCookingMask && !((1 << KnownLangIndex) & MultilanguageCookingMask))
				{
					continue;
				}
				const FString& Ext = KnownLanguageExtensions(KnownLangIndex);

				// mark each coalesced loc file for sha
				INT LangIndex = Localization_GetLanguageExtensionIndex(*Ext);
				if (LangIndex != -1)
				{
					checkf((LangIndex < KnownLanguageExtensions.Num()), TEXT("LangIndex is too large"));

					// Fill in the items...
					FLocalizedSubtitle& LocSubtitle = SoundNodeWave->LocalizedSubtitles(LangIndex);
					UObject::SetLanguage(*Ext, FALSE);
					SoundNodeWave->Subtitles.Empty();
					SoundNodeWave->LoadLocalized(NULL, FALSE);
					// Copy it into the appropriate slot
					LocSubtitle.LanguageExt = Ext;
					LocSubtitle.bManualWordWrap = SoundNodeWave->bManualWordWrap;
					LocSubtitle.bMature = SoundNodeWave->bMature;
					LocSubtitle.bSingleLine = SoundNodeWave->bSingleLine;
					LocSubtitle.Subtitles = SoundNodeWave->Subtitles;
				}
			}
			// Restore the expected language, and localize the SoundNodeWave for that as well.
			// This will result in the default being the language being cooked for.
			UObject::SetLanguage(*SavedLangExt, FALSE);
			if( SavedIndex != -1 )
			{
				FLocalizedSubtitle& LocSubtitle = SoundNodeWave->LocalizedSubtitles(SavedIndex);
				// Copy it back into the default
				SoundNodeWave->bManualWordWrap = LocSubtitle.bManualWordWrap;
				SoundNodeWave->bMature = LocSubtitle.bMature;
				SoundNodeWave->bSingleLine = LocSubtitle.bSingleLine;
				SoundNodeWave->Subtitles = LocSubtitle.Subtitles;
			}
		}
	}
}

/**
 * Helper function used by CookObject - performs ParticleSystem specific cooking.
 *
 * @param	ParticleSystem	ParticleSystem to cook
 */
void UCookPackagesCommandlet::CookParticleSystem(UParticleSystem* ParticleSystem)
{
	check(ParticleSystem);
	SCOPE_SECONDS_COUNTER_COOK(CookParticleSystemTime);
	if (GCookingTarget & UE3::PLATFORM_Stripped)
	{
		TMap<UObject*,UBOOL> RemovedModules;

		// Remove duplicate modules first...
		if (bSkipPSysDuplicateModules == TRUE)
		{
			SCOPE_SECONDS_COUNTER_COOK(CookParticleSystemDuplicateRemovalTime);
			ParticleSystem->RemoveAllDuplicateModules(TRUE, &RemovedModules);
		}

		// Cook out the thumbnail image - no reason to store it for gameplay.
		ParticleSystem->ThumbnailImageOutOfDate = TRUE;
		if ( ParticleSystem->ThumbnailImage != NULL )
		{
			ParticleSystem->ThumbnailImage->SetFlags(RF_NotForClient|RF_NotForServer);
			// clear ForceTagExp so that it won't get PrepareForSaving called on it
			ParticleSystem->ThumbnailImage->ClearFlags(RF_Standalone|RF_ForceTagExp);	
		}
		ParticleSystem->ThumbnailImage = NULL;

		TArray<INT> EmittersToRemove;
		TMap<UParticleModule*, TArray<INT>> ModuleToEmitterIndexMap;

		INT LODLevelCount = 0;

		// Examine each emitter, iterating in reverse as we are removing entries.
		for (INT EmitterIndex = ParticleSystem->Emitters.Num() - 1; EmitterIndex >= 0; EmitterIndex--)
		{
			UParticleEmitter* Emitter = ParticleSystem->Emitters(EmitterIndex);
			// Default to remove the entry, disabled if the emitter is enabled in an LOD level.
			UBOOL bShouldRemoveEntry = TRUE;
			UBOOL bIsTrailEmitter = FALSE;

			// Iterate over LOD levels to see whether it is enabled in any.
			if (Emitter != NULL)
			{
				if (EmitterIndex == 0)
				{
					LODLevelCount = Emitter->LODLevels.Num();
				}
				for (INT LODLevelIndex = 0; LODLevelIndex < Emitter->LODLevels.Num(); LODLevelIndex++)
				{
					// Check required module to see whether emitter is enabled in this LOD level.
					UParticleLODLevel* LODLevel = Emitter->LODLevels(LODLevelIndex);
					if (LODLevel != NULL)
					{
						check(LODLevel->RequiredModule);

						//for mobile default to no lock flags
						BYTE LockAxisFlags = 0;

						for (INT ModuleIdx = -3; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
						{
							UParticleModule* Module = NULL;
							switch (ModuleIdx)
							{
							case -3:	Module = LODLevel->RequiredModule;		break;
							case -2:	Module = LODLevel->SpawnModule;			break;
							case -1:	Module = LODLevel->TypeDataModule;		break;
							default:	Module = LODLevel->Modules(ModuleIdx);	break;
							}

							if (Module)
							{
								if (Module->IsA(UParticleModuleTypeDataTrail2::StaticClass()))
								{
									bIsTrailEmitter = TRUE;
								}
								TArray<INT>* EmitterIndexArray = ModuleToEmitterIndexMap.Find(Module);
								if (EmitterIndexArray == NULL)
								{
									TArray<INT> TempArray;
									ModuleToEmitterIndexMap.Set(Module, TempArray);
									EmitterIndexArray = ModuleToEmitterIndexMap.Find(Module);
								}
								check(EmitterIndexArray);
								EmitterIndexArray->AddUniqueItem(EmitterIndex);

								//have to get the axis lock flags from the module itself
								UParticleModuleOrientationAxisLock* AxisLockModule = Cast<UParticleModuleOrientationAxisLock>(Module);

								if (AxisLockModule && Module->bEnabled)
								{
									LockAxisFlags = AxisLockModule->LockAxisFlags;
								}
							}
						}

						// We don't remove the entry if it's enabled in one, set flag and break out of loop.
						if ((LODLevel->bEnabled == TRUE) || (bIsTrailEmitter == TRUE))
						{
							bShouldRemoveEntry = FALSE;
//							break;
						}

						if (LODLevel->bEnabled)
						{
							BYTE SpriteScreenAlignment = FParticleVertexFactory::StaticGetSpriteScreenAlignment(LockAxisFlags, LODLevel->RequiredModule->ScreenAlignment);
							UBOOL bMeshParticle = (Cast<UParticleModuleTypeDataMesh>(LODLevel->TypeDataModule)) ? TRUE : FALSE;
							//@todo. All beam/trail types should derive from a common base class (derived from TypeDataBase)
							// to allow for easier customization... Ie, this code won't have to be updated for every possible
							// beam/trail class! Same for the mesh emitter above!
							UBOOL bBeamTrailParticle = 
								(
									(Cast<UParticleModuleTypeDataBeam>(LODLevel->TypeDataModule) != NULL) ||
									(Cast<UParticleModuleTypeDataBeam2>(LODLevel->TypeDataModule) != NULL) ||
									(Cast<UParticleModuleTypeDataTrail>(LODLevel->TypeDataModule) != NULL) ||
									(Cast<UParticleModuleTypeDataTrail2>(LODLevel->TypeDataModule) != NULL) ||
									(Cast<UParticleModuleTypeDataRibbon>(LODLevel->TypeDataModule) != NULL) ||
									(Cast<UParticleModuleTypeDataAnimTrail>(LODLevel->TypeDataModule) != NULL)
								) ? TRUE : FALSE;

							//send material to the mobile material cooker for future reference
							if (bMeshParticle == TRUE)
							{
								UParticleModuleTypeDataMesh* MeshTD = Cast<UParticleModuleTypeDataMesh>(LODLevel->TypeDataModule);
								if (MeshTD->Mesh != NULL)
								{
									// Get the material off the mesh
									if (MeshTD->Mesh->LODModels.Num() > 0)
									{
										if (MeshTD->Mesh->LODModels(0).Elements.Num() > 0)
										{
											if (MeshTD->Mesh->LODModels(0).Elements(0).Material != NULL)
											{
												GMobileShaderCooker.SetParticleMaterialUsage(MeshTD->Mesh->LODModels(0).Elements(0).Material, LODLevel->RequiredModule->InterpolationMethod, SpriteScreenAlignment, bBeamTrailParticle, bMeshParticle);
											}
											else
											{
												GMobileShaderCooker.SetParticleMaterialUsage(GEngine->DefaultMaterial, LODLevel->RequiredModule->InterpolationMethod, SpriteScreenAlignment, bBeamTrailParticle, bMeshParticle);
											}
										}
									}
								}
								GMobileShaderCooker.SetParticleMaterialUsage(LODLevel->RequiredModule->Material, LODLevel->RequiredModule->InterpolationMethod, SpriteScreenAlignment, bBeamTrailParticle, bMeshParticle);

								// Is there a MeshMaterial module?
								for (INT ModuleIdx = 0; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
								{
									UParticleModuleMeshMaterial* MeshMaterialModule = Cast<UParticleModuleMeshMaterial>(LODLevel->Modules(ModuleIdx));
									if (MeshMaterialModule != NULL)
									{
										for (INT MMIdx = 0; MMIdx < MeshMaterialModule->MeshMaterials.Num(); MMIdx++)
										{
											if (MeshMaterialModule->MeshMaterials(MMIdx) != NULL)
											{
												GMobileShaderCooker.SetParticleMaterialUsage(MeshMaterialModule->MeshMaterials(MMIdx), LODLevel->RequiredModule->InterpolationMethod, SpriteScreenAlignment, bBeamTrailParticle, bMeshParticle);
											}
										}
									}
								}
							}
							else
							{
								GMobileShaderCooker.SetParticleMaterialUsage(LODLevel->RequiredModule->Material, LODLevel->RequiredModule->InterpolationMethod, SpriteScreenAlignment, bBeamTrailParticle, bMeshParticle);
							}
						}
					}
				}
			}

			if (bShouldRemoveEntry == TRUE)
			{
				EmittersToRemove.AddUniqueItem(EmitterIndex);
			}
		}

		if (LODLevelCount == 1)
		{
			// If there is only one LODLevel, set to DirectSet to avoid performing LOD checks
			if (ParticleSystem->LODMethod != PARTICLESYSTEMLODMETHOD_DirectSet)
			{
				ParticleSystem->LODMethod = PARTICLESYSTEMLODMETHOD_DirectSet;
			}
		}

		if (EmittersToRemove.Num() > 0)
		{
			for (INT RemoveIdx = 0; RemoveIdx < EmittersToRemove.Num(); RemoveIdx++)
			{
				INT EmitterIdx = EmittersToRemove(RemoveIdx);
				UParticleEmitter* RemoveEmitter = ParticleSystem->Emitters(EmitterIdx);

//				debugf(TEXT("Clearing out emitter %2d from %s"), EmitterIdx, *(ParticleSystem->GetPathName()));
				for (INT LODIdx = 0; LODIdx < RemoveEmitter->LODLevels.Num(); LODIdx++)
				{
					UParticleLODLevel* LODLevel = RemoveEmitter->LODLevels(LODIdx);
					if (LODLevel)
					{
//						debugf(TEXT("\tClearing out LODLevel %2d"), LODIdx);

						for (INT ModuleIdx = LODLevel->Modules.Num() - 1; ModuleIdx >= -3; ModuleIdx--)
						{
							UParticleModule* Module = NULL;
							switch (ModuleIdx)
							{
							case -3:	Module = LODLevel->RequiredModule;		break;
							case -2:	Module = LODLevel->SpawnModule;			break;
							case -1:	Module = LODLevel->TypeDataModule;		break;
							default:	Module = LODLevel->Modules(ModuleIdx);	break;
							}
							if (Module != NULL)
							{
								TArray<INT>* EmitterIndexArray = ModuleToEmitterIndexMap.Find(Module);
								if (EmitterIndexArray != NULL)
								{
									UBOOL bRemoveModule = TRUE;
									for (INT CheckIdx = 0; CheckIdx < EmitterIndexArray->Num(); CheckIdx++)
									{
										INT CheckEmitterIdx = (*EmitterIndexArray)(CheckIdx);
										INT DummyIdx;
										if (EmittersToRemove.FindItem(CheckEmitterIdx, DummyIdx) == FALSE)
										{
											bRemoveModule = FALSE;
										}
									}

									if (bRemoveModule == TRUE)
									{
										RemovedModules.Set(Module, TRUE);
										Module->ClearFlags(RF_ForceTagExp);
										Module->SetFlags(RF_MarkedByCooker);
										if (ModuleIdx >= 0)
										{
											LODLevel->Modules.Remove(ModuleIdx);
										}
									}
								}
							}
						}
						LODLevel->ClearFlags(RF_ForceTagExp);
						LODLevel->SetFlags(RF_MarkedByCooker);
					}
				}
				RemoveEmitter->LODLevels.Empty();
				RemoveEmitter->bCookedOut = TRUE;
			}

		}

		if (RemovedModules.Num() > 0)
		{
			for (FObjectIterator CheckObjIt; CheckObjIt; ++CheckObjIt)
			{
				UObject* CheckObj = *CheckObjIt;
				UObject* CheckOuter = CheckObj->GetOuter();
				if (CheckOuter != NULL)
				{
					if (RemovedModules.Find(CheckOuter) != NULL)
					{
						CheckObj->ClearFlags(RF_ForceTagExp);
						CheckObj->SetFlags(RF_MarkedByCooker);
					}
				}
			}
		}
 	}
}

/**
 * Helper function used by CookSkeletalMesh - performs SkeletalMesh specific cooking.
 */
void UCookPackagesCommandlet::CookSkeletalMesh( USkeletalMesh* SkeletalMesh )
{
	SCOPE_SECONDS_COUNTER_COOK(CookSkeletalMeshTime);

	// Strip out unwanted LOD levels based on current platform.
	if( SkeletalMesh->LODModels.Num() )
	{
		check( SkeletalMesh->LODModels.Num() == SkeletalMesh->LODInfo.Num() );

		// Determine LOD bias to use.
		INT PlatformLODBias = 0;
		switch (Platform)
		{
			case UE3::PLATFORM_Xbox360:
				PlatformLODBias = SkeletalMesh->LODBiasXbox360;
				break;
			case UE3::PLATFORM_PS3:
				PlatformLODBias = SkeletalMesh->LODBiasPS3;
				break;
			case UE3::PLATFORM_PC:
			default:
				PlatformLODBias = SkeletalMesh->LODBiasPC;
				break;
		}

		// Ensure valid range.
		PlatformLODBias = Clamp(PlatformLODBias, 0, SkeletalMesh->LODModels.Num() - 1);

		// Release resources for LOD before destroying it implicitly by removing from array.
		for( INT LODIndex=0; LODIndex<PlatformLODBias; LODIndex++ )
	{
			SkeletalMesh->LODModels(LODIndex).ReleaseResources();
		}

		// Remove LOD models and info structs.
		if( PlatformLODBias )
		{
			SkeletalMesh->LODModels.Remove( 0, PlatformLODBias );	
			SkeletalMesh->LODInfo.Remove( 0, PlatformLODBias );
		}
		}

	// Make sure the texel factors are cached, so they can be saved.
	FLOAT TexelFactor = SkeletalMesh->GetStreamingTextureFactor(0);

	if ((Platform == PLATFORM_PS3) || (Platform & PLATFORM_Mobile))
	{
		SkeletalMeshCooker->Init();

		// loop through each LOD model
		const INT NumLODs = SkeletalMesh->LODModels.Num();
		FStaticLODModel** LODModels = SkeletalMesh->LODModels.GetTypedData();
		for (INT LodIndex=0; LodIndex<NumLODs; ++LodIndex)
		{
			// Loop through each mesh chunk/section
			INT NumSections = LODModels[LodIndex]->Sections.Num();
			FSkelMeshChunk* MeshChunks = LODModels[LodIndex]->Chunks.GetTypedData();
			FSkelMeshSection* MeshSections = LODModels[LodIndex]->Sections.GetTypedData();
			FSkeletalMeshVertexBuffer& VertexBuffer = LODModels[LodIndex]->VertexBufferGPUSkin;

			TArray<DWORD> NewIndexArray(0);
			// These new arrays will always have at least as many elements as the old arrays they're replacing.
			NewIndexArray.Reserve(LODModels[LodIndex]->MultiSizeIndexContainer.GetIndexBuffer()->Num());

			for (INT SectionIndex=0; SectionIndex<NumSections; ++SectionIndex)
			{
				FSkelMeshSection& Section = MeshSections[SectionIndex];
				FSkelMeshChunk& Chunk = MeshChunks[Section.ChunkIndex];

				if (Platform == PLATFORM_PS3)
				{
					// Only re-order the section if we do not want a specific draw order.
					if (Section.TriangleSorting == TRISORT_None)
					{
						FSkeletalMeshFragmentCookInfo CookInfo;
						appMemset(&CookInfo, 0, sizeof(FSkeletalMeshFragmentCookInfo));
						TArray<DWORD> Indices;
						LODModels[LodIndex]->MultiSizeIndexContainer.GetIndexBuffer(Indices);
						CookInfo.Indices = Indices.GetData() + Section.BaseIndex;
						CookInfo.NumTriangles = Section.NumTriangles;
						// Edge Geometry is not yet supported in skeletal meshes.  The following fields
						// will not be referenced, but we try to give them meaningful values anyway.
						CookInfo.bEnableEdgeGeometrySupport = FALSE;
						CookInfo.bUseFullPrecisionUVs = (VertexBuffer.GetUseFullPrecisionUVs() == TRUE);
						CookInfo.PositionVertices = NULL;
						CookInfo.MinVertexIndex = 0;
						CookInfo.MaxVertexIndex = LODModels[LodIndex]->NumVertices;
						CookInfo.NewMinVertexIndex = 0;

						FSkeletalMeshFragmentCookOutputInfo CookOutputInfo;
						appMemset(&CookOutputInfo, 0, sizeof(FSkeletalMeshFragmentCookOutputInfo));
						CookOutputInfo.NumPartitionsReserved = (UINT)((CookInfo.NumTriangles+99) / 100);
						CookOutputInfo.NumVerticesReserved   = (UINT)((float)(CookInfo.MaxVertexIndex-CookInfo.MinVertexIndex+1) + CookOutputInfo.NumPartitionsReserved*15);
						CookOutputInfo.NumTrianglesReserved  = (UINT)((float)CookInfo.NumTriangles + CookOutputInfo.NumPartitionsReserved*15);

						// Allocate the output arrays.
						CookOutputInfo.NewIndices                      =  (DWORD*)appMalloc(CookOutputInfo.NumTrianglesReserved*3*sizeof(DWORD));
						CookOutputInfo.VertexRemapTable                =   (INT*)appMalloc(CookOutputInfo.NumVerticesReserved*sizeof(INT));
						CookOutputInfo.PartitionIoBufferSize           =  (UINT*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(UINT));
						CookOutputInfo.PartitionScratchBufferSize      =  (UINT*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(UINT));
						CookOutputInfo.PartitionCommandBufferHoleSize  =  (WORD*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(WORD));
						CookOutputInfo.PartitionIndexBias              = (short*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(short));
						CookOutputInfo.PartitionNumTriangles           =  (WORD*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(WORD));
						CookOutputInfo.PartitionNumVertices            =  (DWORD*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(DWORD));
						CookOutputInfo.PartitionFirstVertex            =  (DWORD*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(DWORD));
						CookOutputInfo.PartitionFirstTriangle          =  (UINT*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(UINT));

						// Repeatedly cook the mesh until we find the right buffer sizes.  In practice, this usually takes one iteration,
						// and should *never* take more than two iterations.
						bool bSuccess = false;
						do
						{
							SkeletalMeshCooker->CookMeshElement(CookInfo, CookOutputInfo);
							bSuccess = true; // innocent until proven guilty

							// Make sure the provided buffers were large enough to contain the output data.  If not, the buffers
							// should be reallocated to match the correct sizes.
							if (CookOutputInfo.NewNumTriangles  > CookOutputInfo.NumTrianglesReserved)
							{
								appFree(CookOutputInfo.NewIndices);
								CookOutputInfo.NumTrianglesReserved = CookOutputInfo.NewNumTriangles;
								CookOutputInfo.NewIndices           = (DWORD*)appMalloc(CookOutputInfo.NumTrianglesReserved*3*sizeof(DWORD));
								bSuccess = false;
							}
							if (CookOutputInfo.NewNumVertices   > CookOutputInfo.NumVerticesReserved)
							{
								appFree(CookOutputInfo.VertexRemapTable);
								CookOutputInfo.NumVerticesReserved = CookOutputInfo.NewNumVertices;
								CookOutputInfo.VertexRemapTable    =   (INT*)appMalloc(CookOutputInfo.NumVerticesReserved*sizeof(INT));
								bSuccess = false;
							}
							if (CookOutputInfo.NumPartitions > CookOutputInfo.NumPartitionsReserved)
							{
								appFree(CookOutputInfo.PartitionIoBufferSize);
								appFree(CookOutputInfo.PartitionScratchBufferSize);
								appFree(CookOutputInfo.PartitionCommandBufferHoleSize);
								appFree(CookOutputInfo.PartitionIndexBias);
								appFree(CookOutputInfo.PartitionNumTriangles);
								appFree(CookOutputInfo.PartitionNumVertices);
								appFree(CookOutputInfo.PartitionFirstVertex);
								appFree(CookOutputInfo.PartitionFirstTriangle);
								CookOutputInfo.NumPartitionsReserved          = CookOutputInfo.NumPartitions;
								CookOutputInfo.PartitionIoBufferSize          =  (UINT*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(UINT));
								CookOutputInfo.PartitionScratchBufferSize     =  (UINT*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(UINT));
								CookOutputInfo.PartitionCommandBufferHoleSize =  (WORD*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(WORD));
								CookOutputInfo.PartitionIndexBias             = (short*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(short));
								CookOutputInfo.PartitionNumTriangles          =  (WORD*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(WORD));
								CookOutputInfo.PartitionNumVertices           =  (DWORD*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(DWORD));
								CookOutputInfo.PartitionFirstVertex           =  (DWORD*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(DWORD));
								CookOutputInfo.PartitionFirstTriangle         =  (UINT*)appMalloc(CookOutputInfo.NumPartitionsReserved*sizeof(UINT));
								bSuccess = false;
							}
						}
						while (!bSuccess);

						// Build and append the fragment's new index buffer
						// Extend the index buffer to contain the (most likely larger) new data.
						UINT FirstNewIndex = NewIndexArray.Num();
						Section.BaseIndex = FirstNewIndex;
						Section.NumTriangles = CookOutputInfo.NewNumTriangles;
						NewIndexArray.Add(Section.NumTriangles*3);
						DWORD* NewIndices = NewIndexArray.GetTypedData() + FirstNewIndex;
						appMemcpy(NewIndices, CookOutputInfo.NewIndices, Section.NumTriangles*3*sizeof(DWORD));

						// Cleanup
						appFree(CookOutputInfo.NewIndices);
						appFree(CookOutputInfo.VertexRemapTable);
						appFree(CookOutputInfo.PartitionIoBufferSize);
						appFree(CookOutputInfo.PartitionScratchBufferSize);
						appFree(CookOutputInfo.PartitionCommandBufferHoleSize);
						appFree(CookOutputInfo.PartitionIndexBias);
						appFree(CookOutputInfo.PartitionNumTriangles);
						appFree(CookOutputInfo.PartitionNumVertices);
						appFree(CookOutputInfo.PartitionFirstVertex);
						appFree(CookOutputInfo.PartitionFirstTriangle);
					}
					else
					{
						// We want to maintain the triangle drawing order for this section, so we will just copy the indices.
						UINT FirstNewIndex = NewIndexArray.Num();
						UINT NumIndices = (Section.TriangleSorting == TRISORT_CustomLeftRight) ? 2*Section.NumTriangles*3 : Section.NumTriangles*3;
						NewIndexArray.Add(NumIndices);
						TArray<DWORD> Indices;
						LODModels[LodIndex]->MultiSizeIndexContainer.GetIndexBuffer(Indices);
						DWORD* OldIndices = Indices.GetData() + Section.BaseIndex;
						DWORD* NewIndices = NewIndexArray.GetTypedData() + FirstNewIndex;
						appMemcpy(NewIndices, OldIndices, NumIndices*sizeof(DWORD));
						Section.BaseIndex = FirstNewIndex;
					}
				}	//End PS3
				else if (Platform & PLATFORM_Mobile)
				{
					//verify that this skeletal mesh isn't trying to use too many bones when cooked
					//default to standard 4 bones
					INT MobileBoneWeightCountLimit = 4;
					//Get platform specific weight count limit
					GConfig->GetInt(TEXT("Cooker.Mobile"), TEXT("BoneWeightCountLimit"), MobileBoneWeightCountLimit, PlatformEngineConfigFilename);
					//loop through all vertices
					for (UINT VertexIndex = 0; VertexIndex < VertexBuffer.GetNumVertices(); ++VertexIndex)
					{
						FGPUSkinVertexBase* BaseVertex = VertexBuffer.GetVertexPtr(VertexIndex);
						check(BaseVertex);
						//keep track of how much weight we might need to redistribute if there's too many influences and map the indicies (key) to their weights (value)
						BYTE WeightRemoved = 0;
						TMap<INT,BYTE> WeightIndicies;
						for (UINT WeightIndex = 0; WeightIndex < MAX_INFLUENCES; ++WeightIndex)
						{
							BYTE Weight = BaseVertex->InfluenceWeights[WeightIndex];
							if (Weight > SMALL_NUMBER)
							{
								//if we've reached the limit, check to see if there's a weaker one already cached
								UBOOL bAddToMap = TRUE;
								if ( WeightIndicies.Num() >= MobileBoneWeightCountLimit )
								{
									INT SmallestWeightIndex = -1;
									BYTE SmallestWeight = 255;
									for ( TMap<INT,BYTE>::TIterator It(WeightIndicies); It; ++It )
									{
										if ( SmallestWeightIndex == -1 || SmallestWeight >= It.Value() )
										{
											SmallestWeightIndex = It.Key();
											SmallestWeight = It.Value();
										}
									}
									//if there is a weaker one, replace it, clearing the weight attributed to it, otherwise clear this one and don't bother adding it
									if ( SmallestWeightIndex != -1 && SmallestWeight < Weight )
									{
										WeightIndicies.Remove( SmallestWeightIndex );
										WeightRemoved += SmallestWeight;
										BaseVertex->InfluenceWeights[SmallestWeightIndex] = 0;
									}
									else
									{
										bAddToMap = FALSE;
										WeightRemoved += Weight;
										BaseVertex->InfluenceWeights[WeightIndex] = 0;
									}
								}
								if ( bAddToMap )
								{
									WeightIndicies.Set(WeightIndex, Weight);
								}
							}
						}
						//loop through the best weights redistributing the weaker weights to them
						if ( WeightRemoved > 0 )
						{
							INT WeightCount = WeightIndicies.Num();
							for ( TMap<INT,BYTE>::TIterator It(WeightIndicies); It; ++It )
							{
								//do like this in case WeightRemoved is odd, that way it favors the first bone getting the extra +1
								BYTE WeightIncrement = appCeil(((FLOAT)WeightRemoved / WeightCount));
								BaseVertex->InfluenceWeights[It.Key()] += WeightIncrement;
								WeightRemoved -= WeightIncrement;
								WeightCount--;
							}
							LogWarningOnce(FString::Printf(TEXT("%s skeletal mesh uses more bone influences than are allowed by this platform (BoneWeightCountLimit:%d), redistributing to the strongest"), *SkeletalMesh->GetName(), MobileBoneWeightCountLimit));
						}
					}
				}
			}	// End Chunk/Section

			if (Platform == PLATFORM_PS3)
			{
				// Update LOD Model's index buffer with the new data
				if (NewIndexArray.Num() < LODModels[LodIndex]->MultiSizeIndexContainer.GetIndexBuffer()->Num())
				{
					warnf(NAME_Warning, TEXT("Possible error converting skeletal mesh %s"), *(SkeletalMesh->GetPathName()));
					warnf(NAME_Warning, TEXT("\tResults had fewer indices than expected..."));
					warnf(NAME_Warning, TEXT("\tPossibly needs to be reimported..."));
				}
				LODModels[LodIndex]->MultiSizeIndexContainer.CopyIndexBuffer(NewIndexArray);
			}
		} // End LOD Model
	}
}

/**
 * Helper function used by CookStaticMesh - performs StaticMesh specific cooking.
 */
void UCookPackagesCommandlet::CookStaticMesh( UStaticMesh* StaticMesh )
{
	SCOPE_SECONDS_COUNTER_COOK(CookStaticMeshTime);

	// Make sure the texel factors are cached, so they can be saved.
	FLOAT TexelFactor = StaticMesh->GetStreamingTextureFactor(0);

	// this is only needed on PS3, but it could be called for Xbox
	if (Platform == PLATFORM_PS3)
	{
		StaticMeshCooker->Init();

		// Each mesh can indicate whether it should use Edge partitioning if possible.
		UBOOL bEnableEdgePartitioning = (StaticMesh->bPartitionForEdgeGeometry != 0);

#if !USE_PS3_PREVERTEXCULLING
		bEnableEdgePartitioning = FALSE;
#endif

		// loop through each LOD model
		const INT NumLODs = StaticMesh->LODModels.Num();
		FStaticMeshRenderData** LODModels = StaticMesh->LODModels.GetTypedData();
		for (INT LodIndex=0; LodIndex<NumLODs; LodIndex++)
		{
			// loop through each section of the particular LOD
			INT NumElements = LODModels[LodIndex]->Elements.Num();
			FStaticMeshElement* MeshElements = LODModels[LodIndex]->Elements.GetTypedData();
			FRawStaticIndexBuffer&	IndexBuffer = LODModels[LodIndex]->IndexBuffer;
			FPositionVertexBuffer&  PositionBuffer = LODModels[LodIndex]->PositionVertexBuffer;
			FStaticMeshVertexBuffer& VertexBuffer = LODModels[LodIndex]->VertexBuffer;
			FColorVertexBuffer &ColorVertexBuffer = LODModels[LodIndex]->ColorVertexBuffer;

			TArray<WORD> NewIndexArray(0);
			bool bSortVertices = true; // Set to false if any fragments within this LOD Model are partitioned for Edge
			const INT OldNumVertices = PositionBuffer.GetNumVertices();
			check(OldNumVertices == VertexBuffer.GetNumVertices());

			// We store the vertex order mapping in a map, keyed by the pointer to this LOD Model, so that 
			// later we can reorder the vertex and index buffers in any other objects that reference this mesh.
			// This includes light maps, kDOP collision trees...
			TArray<INT> VertexRemapTable(0); // Used to remap vertex buffers -- OldIndex = VertexRemapTable[NewIndex]
			TArray<INT> IndexRemapTable(OldNumVertices); // Used to remap index buffers -- NewIndex = IndexRemapTable[OldIndex]

			// These new arrays will always have at least as many elements as the old arrays they're replacing.
			NewIndexArray.Reserve(IndexBuffer.Indices.Num());
			VertexRemapTable.Reserve(OldNumVertices);
			IndexRemapTable.Reserve(OldNumVertices);

			for (INT ElementIndex=0; ElementIndex<NumElements; ElementIndex++)
			{
				FStaticMeshElement& Element = MeshElements[ElementIndex];
				const INT NumFragments = Element.Fragments.Num();
				FFragmentRange* Fragments = Element.Fragments.GetTypedData();

				for(INT FragmentIndex=0; FragmentIndex<NumFragments; FragmentIndex++)
				{
					FFragmentRange& Fragment = Fragments[FragmentIndex];

					FMeshFragmentCookInfo FragmentCookInfo;
					appMemset(&FragmentCookInfo, 0, sizeof(FMeshFragmentCookInfo));
					FragmentCookInfo.Indices = &IndexBuffer.Indices(Fragment.BaseIndex);
					FragmentCookInfo.NumTriangles = Fragment.NumPrimitives;
					// If the Mesh Element has more than one Fragment, we can't use runtime pre-vertex-shader culling (since it requires
					// the final position of vertices to be known before the vertex shader runs, and fragmented meshes are animated
					// in the vertex shader).  So, in that case, we force the cooker to skip full Edge partitioning.
					FragmentCookInfo.bEnableEdgeGeometrySupport = bEnableEdgePartitioning && (NumFragments == 1);
					FragmentCookInfo.bUseFullPrecisionUVs = (VertexBuffer.GetUseFullPrecisionUVs() == TRUE);
					FragmentCookInfo.PositionVertices = (FLOAT*)&PositionBuffer.VertexPosition(0);
					FragmentCookInfo.MinVertexIndex = Element.MinVertexIndex;
					FragmentCookInfo.MaxVertexIndex = Element.MaxVertexIndex;
					FragmentCookInfo.NewMinVertexIndex = VertexRemapTable.Num();

					FMeshFragmentCookOutputInfo FragmentCookOutputInfo;
					appMemset(&FragmentCookOutputInfo, 0, sizeof(FMeshFragmentCookOutputInfo));
					FragmentCookOutputInfo.NumPartitionsReserved = (UINT)((FragmentCookInfo.NumTriangles+99) / 100);
					FragmentCookOutputInfo.NumVerticesReserved   = (UINT)((FLOAT)(Element.MaxVertexIndex-Element.MinVertexIndex+1) + FragmentCookOutputInfo.NumPartitionsReserved*15);
					FragmentCookOutputInfo.NumTrianglesReserved  = (UINT)((FLOAT)FragmentCookInfo.NumTriangles + FragmentCookOutputInfo.NumPartitionsReserved*15);

					// Allocate the output arrays.
					FragmentCookOutputInfo.NewIndices                      =  (WORD*)appMalloc(FragmentCookOutputInfo.NumTrianglesReserved*3*sizeof(WORD));
					FragmentCookOutputInfo.VertexRemapTable                =   (INT*)appMalloc(FragmentCookOutputInfo.NumVerticesReserved*sizeof(INT));
					FragmentCookOutputInfo.PartitionIoBufferSize           =  (UINT*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(UINT));
					FragmentCookOutputInfo.PartitionScratchBufferSize      =  (UINT*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(UINT));
					FragmentCookOutputInfo.PartitionCommandBufferHoleSize  =  (WORD*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(WORD));
					FragmentCookOutputInfo.PartitionIndexBias              = (SWORD*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(SWORD));
					FragmentCookOutputInfo.PartitionNumTriangles           =  (WORD*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(WORD));
					FragmentCookOutputInfo.PartitionNumVertices            =  (WORD*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(WORD));
					FragmentCookOutputInfo.PartitionFirstVertex            =  (WORD*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(WORD));
					FragmentCookOutputInfo.PartitionFirstTriangle          =  (UINT*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(UINT));

					// Repeatedly cook the mesh until we find the right buffer sizes.  In practice, this usually takes one iteration,
					// and should *never* take more than two iterations.
					UBOOL bSuccess = false;
					do
					{
						StaticMeshCooker->CookMeshElement(FragmentCookInfo, FragmentCookOutputInfo);
						bSuccess = TRUE; // innocent until proven guilty

						// Make sure the provided buffers were large enough to contain the output data.  If not, the buffers
						// should be reallocated to match the correct sizes.
						if (FragmentCookOutputInfo.NewNumTriangles  > FragmentCookOutputInfo.NumTrianglesReserved)
						{
							appFree(FragmentCookOutputInfo.NewIndices);
							FragmentCookOutputInfo.NumTrianglesReserved = FragmentCookOutputInfo.NewNumTriangles;
							FragmentCookOutputInfo.NewIndices           = (WORD*)appMalloc(FragmentCookOutputInfo.NumTrianglesReserved*3*sizeof(WORD));
							bSuccess = FALSE;
						}
						if (FragmentCookOutputInfo.NewNumVertices   > FragmentCookOutputInfo.NumVerticesReserved)
						{
							appFree(FragmentCookOutputInfo.VertexRemapTable);
							FragmentCookOutputInfo.NumVerticesReserved = FragmentCookOutputInfo.NewNumVertices;
							FragmentCookOutputInfo.VertexRemapTable    =   (INT*)appMalloc(FragmentCookOutputInfo.NumVerticesReserved*sizeof(INT));
							bSuccess = FALSE;
						}
						if (FragmentCookOutputInfo.NumPartitions > FragmentCookOutputInfo.NumPartitionsReserved)
						{
							appFree(FragmentCookOutputInfo.PartitionIoBufferSize);
							appFree(FragmentCookOutputInfo.PartitionScratchBufferSize);
							appFree(FragmentCookOutputInfo.PartitionCommandBufferHoleSize);
							appFree(FragmentCookOutputInfo.PartitionIndexBias);
							appFree(FragmentCookOutputInfo.PartitionNumTriangles);
							appFree(FragmentCookOutputInfo.PartitionNumVertices);
							appFree(FragmentCookOutputInfo.PartitionFirstVertex);
							appFree(FragmentCookOutputInfo.PartitionFirstTriangle);
							FragmentCookOutputInfo.NumPartitionsReserved          = FragmentCookOutputInfo.NumPartitions;
							FragmentCookOutputInfo.PartitionIoBufferSize          =  (UINT*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(UINT));
							FragmentCookOutputInfo.PartitionScratchBufferSize     =  (UINT*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(UINT));
							FragmentCookOutputInfo.PartitionCommandBufferHoleSize =  (WORD*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(WORD));
							FragmentCookOutputInfo.PartitionIndexBias             = (SWORD*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(SWORD));
							FragmentCookOutputInfo.PartitionNumTriangles          =  (WORD*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(WORD));
							FragmentCookOutputInfo.PartitionNumVertices           =  (WORD*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(WORD));
							FragmentCookOutputInfo.PartitionFirstVertex           =  (WORD*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(WORD));
							FragmentCookOutputInfo.PartitionFirstTriangle         =  (UINT*)appMalloc(FragmentCookOutputInfo.NumPartitionsReserved*sizeof(UINT));
							bSuccess = FALSE;
						}
					}
					while (!bSuccess);

					// Build and append the fragment's new index buffer
					// Extend the index buffer to contain the (most likely larger) new data.
					UINT FirstNewIndex = NewIndexArray.Num();
					Fragment.BaseIndex = FirstNewIndex;
					Fragment.NumPrimitives = FragmentCookOutputInfo.NewNumTriangles;
					NewIndexArray.Add(Fragment.NumPrimitives*3);
					WORD* NewIndices = NewIndexArray.GetTypedData() + FirstNewIndex;
					appMemcpy(NewIndices, FragmentCookOutputInfo.NewIndices, Fragment.NumPrimitives*3*sizeof(WORD));

					if (FragmentCookInfo.bEnableEdgeGeometrySupport)
					{
						// If partitioning was enabled, we skip the manual vertex sorting below; rearranging the vertices again
						// now would break Edge culling (besides, Edge partitioning has much the same effect on cache performance
						// as the manual sorting performed below)
						bSortVertices = FALSE;
						// If Edge Geometry partitioning was enabled, append the fragment's vertex remap table to the
						// LOD-Model-wide remapping.
						UINT FirstNewVertexIndex = VertexRemapTable.Num();
						VertexRemapTable.Add(FragmentCookOutputInfo.NewNumVertices);
						memcpy(VertexRemapTable.GetTypedData() + FirstNewVertexIndex, FragmentCookOutputInfo.VertexRemapTable,
							FragmentCookOutputInfo.NewNumVertices*sizeof(INT));

						// Copy the per-partition data into the PlatformData member.
						check(Element.PlatformData == NULL);
						FPS3StaticMeshData* PlatformData = new FPS3StaticMeshData;
						Element.PlatformData = PlatformData;
						UINT NumPartitions = FragmentCookOutputInfo.NumPartitions;
						PlatformData->IoBufferSize.Reserve(NumPartitions);
						PlatformData->ScratchBufferSize.Reserve(NumPartitions);
						PlatformData->CommandBufferHoleSize.Reserve(NumPartitions);
						PlatformData->IndexBias.Reserve(NumPartitions);
						PlatformData->VertexCount.Reserve(NumPartitions);
						PlatformData->TriangleCount.Reserve(NumPartitions);
						PlatformData->FirstVertex.Reserve(NumPartitions);
						PlatformData->FirstTriangle.Reserve(NumPartitions);
						UINT TotalNumTriangles = 0; // make sure that the triangles in all partitions sum to the fragments' total
						for(UINT PartitionIndex=0; PartitionIndex<NumPartitions; ++PartitionIndex)
						{
							PlatformData->IoBufferSize.AddItem(FragmentCookOutputInfo.PartitionIoBufferSize[PartitionIndex]);
							PlatformData->ScratchBufferSize.AddItem(FragmentCookOutputInfo.PartitionScratchBufferSize[PartitionIndex]);
							PlatformData->CommandBufferHoleSize.AddItem(FragmentCookOutputInfo.PartitionCommandBufferHoleSize[PartitionIndex]);
							PlatformData->IndexBias.AddItem(FragmentCookOutputInfo.PartitionIndexBias[PartitionIndex]);
							PlatformData->VertexCount.AddItem(FragmentCookOutputInfo.PartitionNumVertices[PartitionIndex]);
							PlatformData->TriangleCount.AddItem(FragmentCookOutputInfo.PartitionNumTriangles[PartitionIndex]);
							PlatformData->FirstVertex.AddItem(FragmentCookOutputInfo.PartitionFirstVertex[PartitionIndex]);
							PlatformData->FirstTriangle.AddItem(FragmentCookOutputInfo.PartitionFirstTriangle[PartitionIndex]);
							TotalNumTriangles += FragmentCookOutputInfo.PartitionNumTriangles[PartitionIndex];
						}
						check(TotalNumTriangles == Fragment.NumPrimitives);
					}

					// Cleanup
					appFree(FragmentCookOutputInfo.NewIndices);
					appFree(FragmentCookOutputInfo.VertexRemapTable);
					appFree(FragmentCookOutputInfo.PartitionIoBufferSize);
					appFree(FragmentCookOutputInfo.PartitionScratchBufferSize);
					appFree(FragmentCookOutputInfo.PartitionCommandBufferHoleSize);
					appFree(FragmentCookOutputInfo.PartitionIndexBias);
					appFree(FragmentCookOutputInfo.PartitionNumTriangles);
					appFree(FragmentCookOutputInfo.PartitionNumVertices);
					appFree(FragmentCookOutputInfo.PartitionFirstVertex);
					appFree(FragmentCookOutputInfo.PartitionFirstTriangle);
				} // End Fragment

				// Update some final per-Element data, now that we know the Fragments' final index/vertex ranges
				Element.FirstIndex = Element.Fragments(0).BaseIndex;
				Element.NumTriangles = 0;
				WORD MinVertex = 0xFFFF;
				WORD MaxVertex = 0x0000;
				for(INT FragmentIndex=0; FragmentIndex<NumFragments; FragmentIndex++)
				{
					FFragmentRange& Fragment = Element.Fragments(FragmentIndex);
					Element.NumTriangles += Fragment.NumPrimitives;
					WORD* FragmentIndices = NewIndexArray.GetTypedData() + Fragment.BaseIndex;
					for(INT IndexIndex=0; IndexIndex<Fragment.NumPrimitives*3; ++IndexIndex)
					{
						const WORD Index = FragmentIndices[IndexIndex];
						MinVertex = Min(Index, MinVertex);
						MaxVertex = Max(Index, MaxVertex);
					}
				}
				Element.MinVertexIndex = MinVertex;
				Element.MaxVertexIndex = MaxVertex;
			} // End Element


			// dump out some stats if desired
#if 0
			if (FALSE)
			{
				UINT OldPositionBufferSize = PositionBuffer.GetNumVertices() * 12;
				UINT NewPositionBufferSize = NewVertexArray.Num() * 12;
				warnf(NAME_Log, TEXT("Cooked position buffer grew by %4.1f%% (%d bytes)"),
					(float)NewPositionBufferSize * 100.0f / (float)OldPositionBufferSize - 100.0f,
					NewPositionBufferSize - OldPositionBufferSize);

				UINT OldVertexBufferSize = VertexBuffer.GetNumVertices() * 16;
				UINT NewVertexBufferSize = NewVertexArray.Num() * 16;
				warnf(NAME_Log, TEXT("Cooked vertex buffer grew by %4.1f%% (%d bytes)"),
					(float)NewVertexBufferSize * 100.0f / (float)OldVertexBufferSize - 100.0f,
					NewVertexBufferSize - OldVertexBufferSize);

				UINT OldIndexBufferSize = IndexBuffer.Indices.Num() * 2;
				UINT NewIndexBufferSize = NewIndexArray.Num() * 2;
				warnf(NAME_Log, TEXT("Cooked index buffer grew by %4.1f%% (%d bytes)"),
					(float)NewIndexBufferSize * 100.0f / (float)OldIndexBufferSize - 100.0f,
					NewIndexBufferSize - OldIndexBufferSize);
			}
#endif

			// If we didn't run Edge partitioning, pre-transform vertex cache performance can be significantly
			// improved by sorting the vertex buffer in the order in which the vertices appear in the index
			// buffer.
			if (bSortVertices)
			{
				check(VertexRemapTable.Num() == 0);
				VertexRemapTable.Add(OldNumVertices);

				for(INT VertexIndex=0; VertexIndex<OldNumVertices; ++VertexIndex)
				{
					VertexRemapTable(VertexIndex) = -1;
					IndexRemapTable(VertexIndex) = -1;
				}
				// Iterate over index buffer.  Every time we hit a vertex we haven't seen, assign it the next
				// available index.
				UINT NextNewVertexIndex = 0;
				for(INT IndexIndex=0; IndexIndex<NewIndexArray.Num(); ++IndexIndex)
				{
					INT OldVertexIndex = NewIndexArray(IndexIndex);
					if (IndexRemapTable(OldVertexIndex) != -1)
					{
						// This vertex has already been remapped
						NewIndexArray(IndexIndex) = IndexRemapTable(OldVertexIndex);
						continue; // this vertex has already been remapped
					}
					else
					{
						INT NewVertexIndex = NextNewVertexIndex++;
						NewIndexArray(IndexIndex) = IndexRemapTable(OldVertexIndex) = NewVertexIndex;
						VertexRemapTable(NewVertexIndex) = OldVertexIndex;
					}
				}

				// Need one more pass over the Mesh Elements to update the Max/Min Vertex indexes
				for (INT ElementIndex=0; ElementIndex<NumElements; ElementIndex++)
				{
					FStaticMeshElement& Element = MeshElements[ElementIndex];
					const INT NumFragments = Element.Fragments.Num();
					FFragmentRange* Fragments = Element.Fragments.GetTypedData();

					WORD MinVertex = 0xFFFF;
					WORD MaxVertex = 0x0000;
					for(INT FragmentIndex=0; FragmentIndex<NumFragments; FragmentIndex++)
					{
						FFragmentRange& Fragment = Element.Fragments(FragmentIndex);
						WORD* FragmentIndices = NewIndexArray.GetTypedData() + Fragment.BaseIndex;
						for(INT IndexIndex=0; IndexIndex<Fragment.NumPrimitives*3; ++IndexIndex)
						{
							const WORD Index = FragmentIndices[IndexIndex];
							MinVertex = Min(Index, MinVertex);
							MaxVertex = Max(Index, MaxVertex);
						}
					} // End Fragments
					Element.MinVertexIndex = MinVertex;
					Element.MaxVertexIndex = MaxVertex;
				} // End Elements
			}
			else
			{
				// We need to build the IndexRemapTable from the final VertexRemapTable.
				appMemset(IndexRemapTable.GetTypedData(), 0xFF, OldNumVertices*sizeof(INT)); // initialize to -1
				for(INT NewIndex=0; NewIndex<VertexRemapTable.Num(); ++NewIndex)
				{
					IndexRemapTable(VertexRemapTable(NewIndex)) = NewIndex;
				}
			}

			// Build the LOD Model's final vertex buffer, now that we're guaranteed to have a valid VertexRemapTable.
			TArray<FStaticMeshBuildVertex> NewVertexArray(0);
			NewVertexArray.Add(VertexRemapTable.Num());
			FStaticMeshBuildVertex* NextNewVertex = NewVertexArray.GetTypedData();
			for(INT VertexIndex=0; VertexIndex<VertexRemapTable.Num(); ++VertexIndex)
			{
				INT SourceVertexIndex = VertexRemapTable(VertexIndex);
				check(SourceVertexIndex >= 0 && SourceVertexIndex < OldNumVertices);
				NextNewVertex->Position = PositionBuffer.VertexPosition(SourceVertexIndex);
				NextNewVertex->TangentX = VertexBuffer.VertexTangentX(SourceVertexIndex);
				NextNewVertex->TangentY = VertexBuffer.VertexTangentY(SourceVertexIndex);
				NextNewVertex->TangentZ = VertexBuffer.VertexTangentZ(SourceVertexIndex);
				for(UINT UvIndex=0; UvIndex<VertexBuffer.GetNumTexCoords(); ++UvIndex)
				{
					NextNewVertex->UVs[UvIndex] = VertexBuffer.GetVertexUV(SourceVertexIndex, UvIndex);
				}
				NextNewVertex->Color    = ColorVertexBuffer.GetNumVertices() ? ColorVertexBuffer.VertexColor(SourceVertexIndex) : FColor(255, 255, 255, 255);
				NextNewVertex->FragmentIndex = 0;
				NextNewVertex++;
			}


			// Finally update the LOD Model's index buffer
			check(NewIndexArray.Num() >= IndexBuffer.Indices.Num());
			IndexBuffer.Indices = NewIndexArray;
			// ...and vertex buffers
			check(NewVertexArray.Num() >= (INT)VertexBuffer.GetNumVertices());
			VertexBuffer.Init(NewVertexArray, VertexBuffer.GetNumTexCoords());
			check(NewVertexArray.Num() >= (INT)PositionBuffer.GetNumVertices());
			PositionBuffer.Init(NewVertexArray);

			// remap the color buffer if it had any vertices in it before
			if (ColorVertexBuffer.GetNumVertices())
			{
				ColorVertexBuffer.Init(NewVertexArray);
			}
			LODModels[LodIndex]->NumVertices = NewVertexArray.Num();
			// ...and store the vertex remap table for future use (e.g. any lightmaps that reference
			// this mesh will need to reorder their samples to match the new vertex order!)
			FString RemapKey = FString::Printf(TEXT("%s::%d"), *StaticMesh->GetPathName(), LodIndex);
			check(VertexRemapTable.Num() == NewVertexArray.Num());
			StaticMeshVertexRemappings.Set(RemapKey, VertexRemapTable);
			// If this is LOD Model 0, we need to remap the triangle indices stored in the StaticMesh's
			// collision tree to use the new vertex ordering.  Since we're remapping an index buffer,
			// we use the IndexRemapTable.
			if (LodIndex == 0)
			{
				StaticMesh->kDOPTree.RemapIndices(IndexRemapTable);
			}
		} // End LOD Model
	}
#if 0 // @TODO Re-enable if shifting UVs proves both effective and necessary
	// For iPhone, we need to scale the texture coordinates to fit within a reduced precision range for performance
	else if (Platform == PLATFORM_IPhone)
	{
		// Loop through each LOD model
		const INT NumLODs = StaticMesh->LODModels.Num();
		FStaticMeshRenderData** LODModels = StaticMesh->LODModels.GetTypedData();
		for (INT LodIndex = 0; LodIndex < NumLODs; LodIndex++)
		{
			// Loop through each section of the particular LOD
			FStaticMeshVertexBuffer& VertexBuffer = LODModels[LodIndex]->VertexBuffer;
			// One pass to find the range of the UVs
			const FLOAT AllowedValueMinLimit = -32.0f;
			const FLOAT AllowedValueMaxLimit =  32.0f;
			const FLOAT MaxAllowedRange = AllowedValueMaxLimit - AllowedValueMinLimit;
			FVector2D UVShifts[MAX_TEXCOORDS];
			FVector2D MinUVs[MAX_TEXCOORDS];
			FVector2D MaxUVs[MAX_TEXCOORDS];
			UINT VertexIndex, UVIndex;

			// Initialize the tracking vector arrays
			for (UVIndex = 0; UVIndex < MAX_TEXCOORDS; UVIndex++)
			{
				UVShifts[UVIndex].Set( 0.0f, 0.0f );
				MinUVs[UVIndex].Set( 0.0f, 0.0f );
				MaxUVs[UVIndex].Set( 0.0f, 0.0f );
			}
			for (UVIndex = 0; UVIndex < VertexBuffer.GetNumTexCoords(); UVIndex++)
			{
				MinUVs[UVIndex].Set(  MAX_FLT,  MAX_FLT );
				MaxUVs[UVIndex].Set( -MAX_FLT, -MAX_FLT );
			}

			for (VertexIndex = 0; VertexIndex < VertexBuffer.GetNumVertices(); VertexIndex++)
			{
				for (UVIndex = 0; UVIndex < VertexBuffer.GetNumTexCoords(); UVIndex++)
				{
					FVector2D NextUV = VertexBuffer.GetVertexUV(VertexIndex, UVIndex);
					MinUVs[UVIndex][0] = NextUV[0] < MinUVs[UVIndex][0] ? NextUV[0] : MinUVs[UVIndex][0];
					MinUVs[UVIndex][1] = NextUV[1] < MinUVs[UVIndex][1] ? NextUV[1] : MinUVs[UVIndex][1];
					MaxUVs[UVIndex][0] = NextUV[0] > MaxUVs[UVIndex][0] ? NextUV[0] : MaxUVs[UVIndex][0];
					MaxUVs[UVIndex][1] = NextUV[1] > MaxUVs[UVIndex][1] ? NextUV[1] : MaxUVs[UVIndex][1];
				}
			}
			// Compute the "real" range since we'll need the UV boundaries to line up
			UBOOL bAnyOutOfBounds = FALSE;
			UBOOL bAnyOutOfRange = FALSE;
			for (UVIndex = 0; UVIndex < MAX_TEXCOORDS; UVIndex++)
			{
				for (UINT i = 0; i < 2; i++)
				{
					MinUVs[UVIndex][i] = appFloor(MinUVs[UVIndex][i]);
					MaxUVs[UVIndex][i] = appCeil(MaxUVs[UVIndex][i]);

					if (MinUVs[UVIndex][i] <= AllowedValueMinLimit ||
						MaxUVs[UVIndex][i] >= AllowedValueMaxLimit)
					{
						bAnyOutOfBounds = TRUE;
						UVShifts[UVIndex][i] = appRound( ( MaxUVs[UVIndex][0] + MinUVs[UVIndex][0] ) / 2.0f );

						if (abs(MaxUVs[UVIndex][0] - MinUVs[UVIndex][0]) > MaxAllowedRange)
						{
							bAnyOutOfRange = TRUE;
						}
					}					
				}
			}
			// One (optional) pass to shift down to the allowed range
			if (bAnyOutOfBounds)
			{
				if (bAnyOutOfRange)
				{
					// oops - Add warning here
				}
				else
				{
					// Apply the shift
					for (VertexIndex = 0; VertexIndex < VertexBuffer.GetNumVertices(); VertexIndex++)
					{
						for (UVIndex = 0; UVIndex < VertexBuffer.GetNumTexCoords(); UVIndex++)
						{
							FVector2D OriginalUV = VertexBuffer.GetVertexUV(VertexIndex, UVIndex);
							FVector2D UpdatedUV = OriginalUV - UVShifts[UVIndex];
							VertexBuffer.SetVertexUV(VertexIndex, UVIndex, UpdatedUV);
						}
					}
				}
			}
		}
	}
#endif
}

/**
 * Performs Landscape component specific cooking. This happens before any textures are cooked as it needs the heightmap data.
 *
 * @param	LandscapeComponent	LandscapeComponent to cook
 */
void UCookPackagesCommandlet::CookLandscapeComponent(ULandscapeComponent* LandscapeComponent)
{
	SCOPE_SECONDS_COUNTER_COOK(CookLandscapeTime);

		if( LandscapeComponent->PlatformData )
		{
			appFree(LandscapeComponent->PlatformData);
			LandscapeComponent->PlatformDataSize = 0;
		}

	UTexture2D* WeightTexture;
	LandscapeComponent->GeneratePlatformData( Platform, LandscapeComponent->PlatformData, LandscapeComponent->PlatformDataSize, WeightTexture );

	// Save the weight and color textures if generated
	if( WeightTexture )
		{
		LandscapeComponent->WeightmapTextures.Empty();

		WeightTexture->SetFlags(RF_ForceTagExp);
		WeightTexture->SetFlags(RF_TagExp);
		LandscapeComponent->WeightmapTextures.AddItem(WeightTexture);
	}
}

/**
 * Finds an appropriate Dominant Texture and NormalMap for the MaterialInterface if
 * not already set
 *
 * @param MaterialInterface the material interface object to fill out
 */
static void ConditionalUpdateFlattenedTexture(UMaterialInterface* MaterialInterface)
{
	// if we don't have a mobile base texture (and Flattening is enabled) we want to set it
	// (use the dominant texture directly so instances don't go up the chain)
	//@todo. Do we want to auto-flatten the MIC override textures? Likely not...
	if ((MaterialInterface->MobileBaseTexture == NULL) && MaterialInterface->bAutoFlattenMobile )
	{
		if ( MaterialInterface->HasAnyFlags(RF_ClassDefaultObject) )
		{
			return;
		}

		// get the textures used in the material
		TArray<UTexture*> Textures;
	
		MaterialInterface->GetUsedTextures(Textures);
		// then hunt down a suitable texture
		for (INT TextureIndex = 0; TextureIndex < Textures.Num(); TextureIndex++)
		{
			UTexture* Texture = Textures(TextureIndex);
			if (Texture)
			{
				// find the first non-normal/spec map and use it
				if (
					Texture->LODGroup == TEXTUREGROUP_World || Texture->LODGroup == TEXTUREGROUP_Character ||
					Texture->LODGroup == TEXTUREGROUP_Weapon || Texture->LODGroup == TEXTUREGROUP_Vehicle ||
					Texture->LODGroup == TEXTUREGROUP_Cinematic || Texture->LODGroup == TEXTUREGROUP_Effects ||
					Texture->LODGroup == TEXTUREGROUP_UI || Texture->LODGroup == TEXTUREGROUP_Skybox ||
					Texture->LODGroup == TEXTUREGROUP_EffectsNotFiltered
					)
				{
					MaterialInterface->MobileBaseTexture = Texture;
					break;
				}
			}
		}

		// if still nothing found, then use the first texture
		if (MaterialInterface->MobileBaseTexture == NULL && Textures.Num() > 0)
		{
			MaterialInterface->MobileBaseTexture = Textures(0);
		}

//		if (MaterialInterface->GetName().InStr(TEXT("Fallback")) == -1)
//		{
//			warnf(TEXT("Choosing a texture for material %s. Consider resaving the package!"), *MaterialInterface->GetFullName());
//		}
	}
}

/**
 * Converts a DXT5 texture to a DXT1 texture by stripping out alpha blocks.
 */
static void ConvertDXT5ToDXT1( UTexture2D* Texture )
{
	check( Texture );
	check( Texture->Format == PF_DXT5 );

	TArray<BYTE> OldMipData;
	const INT MipCount = Texture->Mips.Num();
	for ( INT MipIndex = 0; MipIndex < MipCount; ++MipIndex )
	{
		FTexture2DMipMap& Mip = Texture->Mips(MipIndex);
		const INT ByteCount = Mip.Data.GetBulkDataSize();
		const INT BlockSize = 8; // 8 bytes per color and alpha block.
		check( (ByteCount & (BlockSize*2-1)) == 0 ); // ByteCount must be a multiple of block size.
		const INT BlockCount = ByteCount / BlockSize / 2; // 1 color and 1 alpha block
		OldMipData.Empty( OldMipData.Num() );
		OldMipData.Add( ByteCount );
		BYTE* RESTRICT DXT5Blocks = OldMipData.GetTypedData();
		appMemcpy( DXT5Blocks, Mip.Data.Lock( LOCK_READ_WRITE ), ByteCount );
		BYTE* RESTRICT DXT1Blocks = (BYTE*)Mip.Data.Realloc( BlockCount * BlockSize );
		const BYTE* LastDXT1Block = DXT1Blocks + BlockCount * BlockSize;
		while ( DXT1Blocks < LastDXT1Block )
		{
			// Skip the alpha block.
			DXT5Blocks += BlockSize;

			// Copy the color block and advance.
			appMemcpy( DXT1Blocks, DXT5Blocks, BlockSize );
			DXT1Blocks += BlockSize;
			DXT5Blocks += BlockSize;
		}
		Mip.Data.Unlock();
	}
	Texture->Format = PF_DXT1;

	// Dump cached flash mips.
	Texture->CachedFlashMips.RemoveBulkData();
}

/**
 * Creates an instance of a StaticMeshCollectorActor.  If a valid World is specified, uses SpawnActor;
 * otherwise, uses ConstructObject.
 *
 * @param	Package		the package to create the new actor in
 * @param	World		if Package corresponds to a map package, the reference to the UWorld object for the map.
 */
namespace
{
	template< class T >
	T* CreateComponentCollector( UPackage* Package, UWorld* World )
	{
		T* Result = NULL;
		
		if ( Package != NULL )
		{
			if ( World != NULL && World->PersistentLevel != NULL )
			{
				Result = Cast<T>(World->SpawnActor(T::StaticClass()));
			}
			else
			{
				Result = ConstructObject<T>(T::StaticClass(), Package);
			}

			if ( Result != NULL )
			{
				Result->SetFlags(RF_Cooked);
			}
		}

		return Result;
	}
};

/**
 * Cooks out all static mesh actors in the specified package by re-attaching their StaticMeshComponents to
 * a StaticMeshCollectionActor referenced by the world.
 *
 * @param	Package		the package being cooked
 */
void UCookPackagesCommandlet::CookStaticMeshActors( UPackage* Package )
{
	// 'Cook-out' material expressions on consoles
	// In this case, simply don't load them on the client
	// Keep 'parameter' types around so that we can get their defaults.
	if ( GCookingTarget & UE3::PLATFORM_Stripped )
	{
		// only cook-out StaticMeshActors when cooking for console
		check(Package);

		// find all StaticMeshActors and static Light actors which are referenced by something in the map
		UWorld* World = FindObject<UWorld>( Package, TEXT("TheWorld") );
		if ( World != NULL )
		{
			TArray<ULevel*> LevelsToSearch = World->Levels;

			// make sure that the world's PersistentLevel is part of the levels array for the purpose of this test.
			if ( World->PersistentLevel != NULL )
			{
				LevelsToSearch.AddUniqueItem(World->PersistentLevel);
			}
			for ( INT LevelIndex = 0; LevelIndex < LevelsToSearch.Num(); LevelIndex++ )
			{
				ULevel* Level = LevelsToSearch(LevelIndex);

				// we need to remove all StaticMeshActors and static Light actors from the level's Actors array so that we don't
				// get false positives during our reference checking.
				// however, we'll need to restore any actors which don't get cooked out, so keep track of their indices
				TMap<AStaticMeshActor*,INT> StaticActorIndices;
				TLookupMap<AStaticMeshActor*> StaticMeshActors;

				// remove all StaticMeshActors from the level's Actor array so that we don't get false positives.
				for ( INT ActorIndex = 0; ActorIndex < Level->Actors.Num(); ActorIndex++ )
				{
					AActor* Actor = Level->Actors(ActorIndex);
					// only combine actual SMAs, not subclasses, because we'd lose the added functionality of the subclass
					// when we throw away the actor
					if ( Actor != NULL && Actor->GetClass() == AStaticMeshActor::StaticClass() )
					{
						AStaticMeshActor* StaticMeshActor = static_cast<AStaticMeshActor*>(Actor);

						StaticMeshActors.AddItem(StaticMeshActor);
						StaticActorIndices.Set(StaticMeshActor, ActorIndex);
						Level->Actors(ActorIndex) = NULL;
					}
				}

				// now use the object reference collector to find the static mesh actors that are still being referenced
				TArray<AStaticMeshActor*> ReferencedStaticMeshActors;
				TArchiveObjectReferenceCollector<AStaticMeshActor> SMACollector(&ReferencedStaticMeshActors, Package, FALSE, TRUE, TRUE, TRUE);
				Level->Serialize( SMACollector );

				// remove any StaticMeshActors which aren't valid for cooking out
				TFindObjectReferencers<AStaticMeshActor> StaticMeshReferencers(ReferencedStaticMeshActors, Package);
				for ( INT ActorIndex = ReferencedStaticMeshActors.Num() - 1; ActorIndex >= 0; ActorIndex-- )
				{
					AStaticMeshActor* StaticMeshActor = ReferencedStaticMeshActors(ActorIndex);
					UStaticMeshComponent* Component = StaticMeshActor->StaticMeshComponent;

					// for now, we'll ignore StaticMeshActors that are archetypes or instances of archetypes.
					if ( Component == NULL || Component->IsTemplate(RF_ArchetypeObject) || Component->GetArchetype()->IsTemplate(RF_ArchetypeObject) )
					{
						ReferencedStaticMeshActors.Remove(ActorIndex);
					}
					else
					{
						TArray<UObject*> Referencers;
						StaticMeshReferencers.MultiFind(StaticMeshActor, Referencers);
						for ( INT ReferencerIndex = Referencers.Num() - 1; ReferencerIndex >= 0; ReferencerIndex-- )
						{
							UObject* Referencer = Referencers(ReferencerIndex);
							if ( Referencer == StaticMeshActor->GetLevel() )
							{
								// if this is the actor's level, ignore this reference
								Referencers.Remove(ReferencerIndex);
							}
							else if ( Referencer == StaticMeshActor->StaticMeshComponent )
							{
								// if the referencer is the StaticMeshActor's StaticMeshComponent, we can ignore this reference as this means that
								// something else in the level is referencing the StaticMeshComponent (which will still be around even if we cook
								// out the StaticMeshActor)
								Referencers.Remove(ReferencerIndex);
							}
							else if ( Referencer->IsA(ANavigationPoint::StaticClass())
							&&	(static_cast<ANavigationPoint*>(Referencer)->Base == StaticMeshActor) )
							{
								// If the actor that's based on me is an interp actor, then we need to preserve this
								// reference, since Matinee movement track data is stored relative to the object's
								// coordinate system.  If we change the coordinate system of the object by debasing it,
								// the Matinee data will be positioned incorrectly.  This is super important for
								// objects like Turrets!
								if( static_cast<AActor*>(Referencer)->Physics != PHYS_Interpolating )
								{
									// if this actor references the StaticMeshActor because it's based on the static mesh actor, ignore this reference
									Referencers.Remove(ReferencerIndex);
								}
							}
						}

						// if this StaticMeshActor is still referenced by something, do not cook it out
						if ( Referencers.Num() == 0 )
						{
							ReferencedStaticMeshActors.Remove(ActorIndex);
						}
					}
				}

				// remove the referenced static mesh actors from the list of actors to be cooked-out.
				for ( INT ActorIndex = 0; ActorIndex < ReferencedStaticMeshActors.Num(); ActorIndex++ )
				{
					StaticMeshActors.RemoveItem(ReferencedStaticMeshActors(ActorIndex));
				}

				AStaticMeshCollectionActor* MeshCollector = NULL;
				for ( INT ActorIndex = 0; ActorIndex < StaticMeshActors.Num(); ActorIndex++ )
				{
					AStaticMeshActor* StaticMeshActor = StaticMeshActors(ActorIndex);
					UStaticMeshComponent* Component = StaticMeshActor->StaticMeshComponent;

					// SMAs without a SMC should be removed from maps. There already is a map warning for this so we simply silently handle this case.
					if( !Component )
					{
						continue;
					}

					// Detect static mesh components that have a transform that can't be represented by a single scale/rotation/translation.
					const UBOOL bActorHasScale =
						StaticMeshActor->DrawScale != 1.0f ||
						StaticMeshActor->DrawScale3D != FVector(1.0f,1.0f,1.0f);
					const UBOOL bComponentHasRotation = !Component->Rotation.IsZero();
					const UBOOL bComponentHasTranslation = !Component->Translation.IsZero();
					if(bActorHasScale && (bComponentHasRotation || bComponentHasTranslation))
					{
						continue;
					}

					// if there is a limit to the number of components that can be added to a StaticMeshCollectorActor, create a new
					// one if we have reached the limit
					if (MeshCollector == NULL
					|| (MeshCollector->MaxStaticMeshComponents > 0
					&&  MeshCollector->StaticMeshComponents.Num() >= MeshCollector->MaxStaticMeshComponents) )
					{
						MeshCollector = CreateComponentCollector<AStaticMeshCollectionActor>(Package, World);
					}

					// UPrimitiveComponent::Detach() will clear the ShadowParent but it will never be restored, so save the reference and restore it later
					UPrimitiveComponent* ComponentShadowParent = Component->ShadowParent;

					// remove it from the StaticMeshActor.
					StaticMeshActor->DetachComponent(Component);

					// rather than duplicating the StaticMeshComponent into the mesh collector, rename the component into the collector
					// so that we don't lose any outside references to this component (@todo ronp - are external references to components even allowed?)
					const UBOOL bWasPublic = Component->HasAnyFlags(RF_Public);

					// clear the RF_Public flag so that we don't create a redirector
					Component->ClearFlags(RF_Public);

					// since we're renaming multiple components into the same Outer, it's likely that we'll end up with a name
					// conflict.  unfortunately, for the script patcher these components need to have deterministic names, so
					// create a mangled name using the actor and some tag
					const FString OriginalComponentName = Component->GetName();
					const FString NewComponentName = FString::Printf(TEXT("%s_SMC_%d"),
						*StaticMeshActor->GetFName().GetNameString(),
						NAME_INTERNAL_TO_EXTERNAL(StaticMeshActor->GetFName().GetNumber()));
					if (Component->Rename(*NewComponentName, MeshCollector, REN_ForceNoResetLoaders | REN_KeepNetIndex | REN_Test))
					{
						Component->Rename(*NewComponentName, MeshCollector, REN_ForceNoResetLoaders | REN_KeepNetIndex);
					}
					else
					{
						Component->Rename(NULL, MeshCollector, REN_ForceNoResetLoaders | REN_KeepNetIndex);
					}

					if ( bWasPublic )
					{
						Component->SetFlags(RF_Public);
					}

					// now add it to the mesh collector's StaticMeshComponents array
					MeshCollector->StaticMeshComponents.AddItem(Component);

					// it must also exist in the AllComponents array so that the component's physics data can be cooked
					MeshCollector->AllComponents.AddItem(Component);

					// copy any properties which are usually pulled from the Owner at runtime
					Component->ShadowParent = ComponentShadowParent;
					Component->CollideActors = StaticMeshActor->bCollideActors && Component->CollideActors;
					Component->HiddenGame = Component->HiddenGame || (!Component->bCastHiddenShadow && StaticMeshActor->bHidden);
					Component->HiddenEditor = Component->HiddenEditor || StaticMeshActor->bHiddenEd;

					// Since UPrimitiveComponent::SetTransformedToWorld() generates a matrix which includes the
					// component's Scale/Scale3D then multiplies that by the actor's LocalToWorld to get the final
					// transform, the component's Scale/Scale3D must include the actor's scale and the LocalToWorld
					// matrix we serialize for the component must NOT include scaling.  This is necessary so that if
					// the component has a value for Scale/Scale3D it is preserved without applying the actor's scaling
					// twice for all cases.
					FMatrix ActorLocalToWorld = StaticMeshActor->LocalToWorld();

					FVector ActorScale3D = StaticMeshActor->DrawScale * StaticMeshActor->DrawScale3D;
					Component->Scale3D *= ActorScale3D;
					
					FVector RecipScale( 1.f/ActorScale3D.X, 1.f/ActorScale3D.Y, 1.f/ActorScale3D.Z );

					ActorLocalToWorld.M[0][0] *= RecipScale.X;
					ActorLocalToWorld.M[0][1] *= RecipScale.X;
					ActorLocalToWorld.M[0][2] *= RecipScale.X;

					ActorLocalToWorld.M[1][0] *= RecipScale.Y;
					ActorLocalToWorld.M[1][1] *= RecipScale.Y;
					ActorLocalToWorld.M[1][2] *= RecipScale.Y;

					ActorLocalToWorld.M[2][0] *= RecipScale.Z;
					ActorLocalToWorld.M[2][1] *= RecipScale.Z;
					ActorLocalToWorld.M[2][2] *= RecipScale.Z;

					Component->Translation = ActorLocalToWorld.TransformFVector(Component->Translation);
					Component->Rotation = (FRotationMatrix(Component->Rotation) * ActorLocalToWorld).Rotator();

					// now mark the StaticMeshActor with no-load flags so that it will disappear on save
					StaticMeshActor->SetFlags(RF_NotForClient|RF_NotForServer|RF_NotForEdit);
					for ( INT CompIndex = 0; CompIndex < StaticMeshActor->Components.Num(); CompIndex++ )
					{
						if ( StaticMeshActor->Components(CompIndex) != NULL )
						{
							StaticMeshActor->Components(CompIndex)->SetFlags(RF_NotForClient|RF_NotForServer|RF_NotForEdit);
						}
					}

					debugf(NAME_DevCooking, TEXT("Cooking out StaticMeshActor %s; re-attaching %s to %s as %s"),
						*StaticMeshActor->GetName(), *OriginalComponentName, *MeshCollector->GetName(), *Component->GetName());

					StaticActorIndices.Remove(StaticMeshActor);
				}

				// finally, restore the entries in the level's Actors array for the StaticMeshActors not being cooked out
				for ( TMap<AStaticMeshActor*,INT>::TIterator It(StaticActorIndices); It; ++It )
				{
					INT ActorIndex = It.Value();

					// make sure nothing filled in this entry in the array
					checkSlow(Level->Actors(ActorIndex)==NULL);
					Level->Actors(ActorIndex) = It.Key();
				}
#if _REMOVE_EMPTY_STATIC_ACTORS_
				INT TotalCount = Level->Actors.Num();
				INT RemoveCount = 0;
				for (INT RemoveIndex = Level->Actors.Num() - 1; RemoveIndex >= 0; RemoveIndex--)
				{
					if (Level->Actors(RemoveIndex) == NULL)
					{
						Level->Actors.Remove(RemoveIndex);
						RemoveCount++;
					}
				}
				warnf(TEXT("Purged %5d NULL actors (out of %6d) from Level->Actors in %s"), RemoveCount, TotalCount, *(Level->GetName()));
#endif	//#if _REMOVE_EMPTY_STATIC_ACTORS_
			}
		}
	}
}

/**
 * Cooks out all static Light actors in the specified package by re-attaching their LightComponents to a 
 * StaticLightCollectionActor referenced by the world.
 */
void UCookPackagesCommandlet::CookStaticLightActors( UPackage* Package )
{
	if ( GCookingTarget & UE3::PLATFORM_Stripped )
	{
		// only cook-out static Lights when cooking for console
		check(Package);

		// find all StaticMeshActors and static Light actors which are referenced by something in the map
		UWorld* World = FindObject<UWorld>( Package, TEXT("TheWorld") );
		if ( World != NULL )
		{
			TArray<ULevel*> LevelsToSearch = World->Levels;

			// make sure that the world's PersistentLevel is part of the levels array for the purpose of this test.
			if ( World->PersistentLevel != NULL )
			{
				LevelsToSearch.AddUniqueItem(World->PersistentLevel);
			}
			for ( INT LevelIndex = 0; LevelIndex < LevelsToSearch.Num(); LevelIndex++ )
			{
				ULevel* Level = LevelsToSearch(LevelIndex);

				// we need to remove all static Light actors from the level's Actors array so that we don't
				// get false positives during our reference checking.
				// however, we'll need to restore any actors which don't get cooked out, so keep track of their indices
				TMap<ALight*,INT> StaticActorIndices;
				TLookupMap<ALight*> StaticLightActors;

				// remove all StaticMeshActors from the level's Actor array so that we don't get false positives.
				for ( INT ActorIndex = 0; ActorIndex < Level->Actors.Num(); ActorIndex++ )
				{
					AActor* Actor = Level->Actors(ActorIndex);
					if ( Actor != NULL && Actor->IsA(ALight::StaticClass()) && Actor->IsStatic() )
					{
						ALight* Light = static_cast<ALight*>(Actor);

						StaticLightActors.AddItem(Light);
						StaticActorIndices.Set(Light, ActorIndex);
						Level->Actors(ActorIndex) = NULL;
					}
				}

				// now use the object reference collector to find the static mesh actors that are still being referenced
				TArray<ALight*> ReferencedStaticLightActors;
				{
					TArchiveObjectReferenceCollector<ALight> LightCollector(&ReferencedStaticLightActors, Package, FALSE, TRUE, TRUE, TRUE);
					Level->Serialize( LightCollector );
				}

				// remove any static light actors which aren't valid for cooking out
				for ( INT ActorIndex = ReferencedStaticLightActors.Num() - 1; ActorIndex >= 0; ActorIndex-- )
				{
					ALight* Light = ReferencedStaticLightActors(ActorIndex);
					if ( Light->IsStatic() )
					{
						ULightComponent* Component = ReferencedStaticLightActors(ActorIndex)->LightComponent;

						// for now, we'll ignore static Lights that are archetypes or instances of archetypes.
						if ( Component != NULL
							&&	(Component->IsTemplate(RF_ArchetypeObject) 
							|| Component->GetArchetype()->IsTemplate(RF_ArchetypeObject)
							// Don't cook out static light actors with dynamic lighting
							// We need the actor of dynamic lights for performance analysis (PIX will show the name)
							|| !Component->HasStaticLighting()) )
						{
							ReferencedStaticLightActors.Remove(ActorIndex);
						}
					}
					else
					{
						ReferencedStaticLightActors.Remove(ActorIndex);
					}
				}

				TFindObjectReferencers<ALight> StaticLightReferencers(ReferencedStaticLightActors, Package);
				for ( INT ActorIndex = ReferencedStaticLightActors.Num() - 1; ActorIndex >= 0; ActorIndex-- )
				{
					ALight* LightActor = ReferencedStaticLightActors(ActorIndex);

					TArray<UObject*> Referencers;
					StaticLightReferencers.MultiFind(LightActor, Referencers);
					for ( INT ReferencerIndex = Referencers.Num() - 1; ReferencerIndex >= 0; ReferencerIndex-- )
					{
						UObject* Referencer = Referencers(ReferencerIndex);
						if ( Referencer == LightActor->GetLevel() )
						{
							// if this is the actor's level, ignore this reference
							Referencers.Remove(ReferencerIndex);
						}
						else if ( Referencer->IsIn(LightActor) && Referencer->IsA(UComponent::StaticClass()) )
						{
							// if the referencer is one of the LightActor's components, we can ignore this reference as this means that
							// something else in the level is referencing the component directly (which will still be around even if we cook
							// out the Light actor)
							Referencers.Remove(ReferencerIndex);
						}
					}

					// if this actor is still referenced by something, do not cook it out
					if ( Referencers.Num() == 0 )
					{
						ReferencedStaticLightActors.Remove(ActorIndex);
					}
				}

				for ( INT ActorIndex = 0; ActorIndex < ReferencedStaticLightActors.Num(); ActorIndex++ )
				{
					StaticLightActors.RemoveItem(ReferencedStaticLightActors(ActorIndex));
				}

				AStaticLightCollectionActor* LightCollector = NULL;
				for ( INT ActorIndex = 0; ActorIndex < StaticLightActors.Num(); ActorIndex++ )
				{
					ALight* LightActor = StaticLightActors(ActorIndex);
					ULightComponent* Component = LightActor->LightComponent;

					// Light actors without a light component should be removed from maps. There already is a map warning for this so we simply silently handle this case.
					if( !Component )
					{
						continue;
					}

					// if there is a limit to the number of components that can be added to a StaticLightCollectorActor, create a new
					// one if we have reached the limit
					if (LightCollector == NULL
					|| (LightCollector->MaxLightComponents > 0
					&&  LightCollector->LightComponents.Num() >= LightCollector->MaxLightComponents) )
					{
						LightCollector = CreateComponentCollector<AStaticLightCollectionActor>(Package, World);
					}

					// remove it from the Light actor.
					LightActor->DetachComponent(Component);

					// rather than duplicating the LightComponent into the light collector, rename the component into the collector
					// so that we don't lose any outside references to this component (@todo ronp - are external references to components even allowed?)
					const UBOOL bWasPublic = Component->HasAnyFlags(RF_Public);

					// clear the RF_Public flag so that we don't create a redirector
					Component->ClearFlags(RF_Public);

					// since we're renaming multiple components into the same Outer, it's likely that we'll end up with a name
					// conflict.  unfortunately, for the script patcher these components need to have deterministic names, so
					// create a mangled name using the actor and some tag
					const FString OriginalComponentName = Component->GetName();
					const FString NewComponentName = *(LightActor->GetName() + TEXT("_LC"));
					if (Component->Rename(*NewComponentName, LightCollector, REN_ForceNoResetLoaders | REN_KeepNetIndex | REN_Test))
					{
						Component->Rename(*NewComponentName, LightCollector, REN_ForceNoResetLoaders | REN_KeepNetIndex);
					}
					else
					{
						Component->Rename(NULL, LightCollector, REN_ForceNoResetLoaders | REN_KeepNetIndex);
					}

					if ( bWasPublic )
					{
						Component->SetFlags(RF_Public);
					}

					// now add it to the light collector's LightComponents array
					LightCollector->LightComponents.AddItem(Component);

					// it must also exist in the AllComponents array so that the component's physics data can be cooked
					LightCollector->AllComponents.AddItem(Component);

					// copy any properties which are usually pulled from the Owner at runtime
					// @todo


					// set the component's LightToWorld while we still have a reference to the original owning Light actor.  This matrix
					// will be serialized to disk by the LightCollector

					Component->LightToWorld = LightActor->LocalToWorld();

					// now mark the Light actor with no-load flags so that it will disappear on save
					LightActor->SetFlags(RF_NotForClient|RF_NotForServer|RF_NotForEdit);
					for ( INT CompIndex = 0; CompIndex < LightActor->Components.Num(); CompIndex++ )
					{
						if ( LightActor->Components(CompIndex) != NULL )
						{
							LightActor->Components(CompIndex)->SetFlags(RF_NotForClient|RF_NotForServer|RF_NotForEdit);
						}
					}

					debugf(NAME_DevCooking, TEXT("Cooking out %s %s; re-attaching %s to %s as %s"),
						*LightActor->GetClass()->GetName(), *LightActor->GetName(), *OriginalComponentName, *LightCollector->GetName(), *Component->GetName());

					StaticActorIndices.Remove(LightActor);
				}


				// finally, restore the entries in the level's Actors array for the static Lights not being cooked out
				for ( TMap<ALight*,INT>::TIterator It(StaticActorIndices); It; ++It )
				{
					INT ActorIndex = It.Value();

					// make sure nothing filled in this entry in the array
					checkSlow(Level->Actors(ActorIndex)==NULL);
					Level->Actors(ActorIndex) = It.Key();
				}
			}
		}
	}
}

/**
 *	Clean up the kismet for the given level...
 *	Remove 'danglers' - sequences that don't actually hook up to anything, etc.
 *
 *	@param	Package		The map being cooked
 */
void UCookPackagesCommandlet::CleanupKismet(UPackage* Package)
{
	check(Package);

	// Find the UWorld object (only valid if we're looking at a map).
	UWorld* World = FindObject<UWorld>( Package, TEXT("TheWorld") );
	if (World == NULL)
	{
		debugfSlow(TEXT("CleanupKismet called on non-map package %s"), *(Package->GetName()));
		return;
	}

	UnreferencedMatineeData.Empty();

	// Find dangling InterpData
	TArray<USequence*> FoundSequences;
	TArray<USequenceAction*> FoundActions;
	for (FObjectIterator It; It; ++It)
	{
		UObject* Object = *It;

		if (Object->IsIn(Package) || Object->HasAnyFlags(RF_ForceTagExp))
		{
			UInterpData* InterpData = Cast<UInterpData>(Object);
			USequenceAction* Action = Cast<USequenceAction>(Object);
		    USequence* Sequence = Cast<USequence>(Object);

			if (InterpData)
			{
				UnreferencedMatineeData.AddUniqueItem(InterpData);
			}
			if (Action)
			{
				FoundActions.AddUniqueItem(Action);
			}
			if (Sequence)
			{
				FoundSequences.AddUniqueItem(Sequence);
			}
		}
	}

	// Check all actions... this will catch any variables that are interp data!
	for (INT ActionIndex = 0; ActionIndex < FoundActions.Num(); ActionIndex++)
	{
		USequenceAction* Action = FoundActions(ActionIndex);
		if (Action)
		{
			USeqAct_Interp* Matinee = Cast<USeqAct_Interp>(Action);
			if (Matinee)
			{
				UInterpData* InterpData = Matinee->FindInterpDataFromVariable();
				if (InterpData)
				{
					debugfSlow(TEXT("InterpData %s, ref'd by %s"), 
						*(InterpData->GetPathName()),
						*(Action->GetFullName()));
					UnreferencedMatineeData.RemoveItem(InterpData);
				}
			}

			TArray<UInterpData*> IDataArray;
			Action->GetInterpDataVars(IDataArray);

			for (INT InterpIndex = 0; InterpIndex < IDataArray.Num(); InterpIndex++)
			{
				UInterpData* InterpData = IDataArray(InterpIndex);
				if (InterpData)
				{
					debugfSlow(TEXT("InterpData %s, ref'd by %s"), 
						*(InterpData->GetPathName()),
						*(Action->GetFullName()));
					UnreferencedMatineeData.RemoveItem(InterpData);
				}
			}
		}
	}

	// Allow the game-specific stuff a chance as well...
	if (GGameCookerHelper)
	{
		GGameCookerHelper->CleanupKismet(this, Package);
	}

	// Now, any InterpData remaining are assumed to be unreferenced...
	for (INT UnrefIndex = 0; UnrefIndex < UnreferencedMatineeData.Num(); UnrefIndex++)
	{
		UInterpData* InterpData = UnreferencedMatineeData(UnrefIndex);
		if (InterpData)
		{
			debugf(TEXT("Cooking out InterpData %s"), *(InterpData->GetPathName()));
			InterpData->ClearFlags(RF_ForceTagExp);
			InterpData->SetFlags(RF_MarkedByCooker|RF_Transient);

			if (0) // Make this 1 to list referencers to it...
			{
				// Dump out the references to the soundcue...
				// NOTE: This is the exact code from "obj refs"
				FStringOutputDevice TempAr;
				InterpData->OutputReferencers(TempAr,TRUE);
				TArray<FString> Lines;
				TempAr.ParseIntoArray(&Lines, LINE_TERMINATOR, 0);
				for ( INT i = 0; i < Lines.Num(); i++ )
				{
					warnf(NAME_Warning, TEXT("\t%s"), *Lines(i));
				}
			}
			else
			{
				debugf(TEXT("Cooking out orphaned InterpData: %s"), *(InterpData->GetPathName()));
			}

			for (INT SeqIndex = 0; SeqIndex < FoundSequences.Num(); SeqIndex++)
			{
				USequence* Sequence = FoundSequences(SeqIndex);
				if (Sequence)
				{
					for (INT SeqObjIndex = 0; SeqObjIndex < Sequence->SequenceObjects.Num(); SeqObjIndex++)
					{
						USequenceObject* SeqObj = Sequence->SequenceObjects(SeqObjIndex);
						if (SeqObj && SeqObj->IsA(UInterpData::StaticClass()))
						{
							if (SeqObj == InterpData)
							{
								debugf(TEXT("InterpData %s, remove from %s"), 
									*(InterpData->GetPathName()),
									*(Sequence->GetFullName()));
								Sequence->SequenceObjects(SeqObjIndex) = NULL;
							}
						}
					}
				}
			}
		}
	}
}

/** 
 *	Get the referenced texture param list for the given material instance.
 *
 *	@param	MatInst					The material instance of interest
 *	@param	RefdTextureParamsMap	Map to fill in w/ texture name-texture pairs
 */
void UCookPackagesCommandlet::GetMaterialInstanceReferencedTextureParams(UMaterialInstance* MatInst, TMap<FName,UTexture*>& RefdTextureParamsMap)
{
	if (MatInst->Parent != NULL)
	{
		for (INT MPIdx = 0; MPIdx < MP_MAX; MPIdx++)
		{
			EMaterialProperty MaterialProp = EMaterialProperty(MPIdx);
			TArray<UTexture*> IgnoreTextures;
			TArray<FName> RefdParamNames;
			if (MatInst->Parent->GetTexturesInPropertyChain(MaterialProp, IgnoreTextures, &RefdParamNames, 
				// the set of parameters in an MIC for all quality levels are the same, so we can just use HIGH here
				(bSkipStaticSwitchClean ? MatInst->StaticParameters[MSQ_HIGH] : NULL)) == TRUE)
			{
				for (INT AddIdx = 0; AddIdx < RefdParamNames.Num(); AddIdx++)
				{
					UTexture* DefaultTexture = NULL;
					MatInst->Parent->GetTextureParameterValue(RefdParamNames(AddIdx), DefaultTexture);
					RefdTextureParamsMap.Set(RefdParamNames(AddIdx), DefaultTexture);
				}
			}
		}
	}
}

/**
 *	Clean up the materials
 *
 *	@param	Package		The map being cooked
 */
void UCookPackagesCommandlet::CleanupMaterials(UPackage* Package)
{
	SCOPE_SECONDS_COUNTER_COOK(CleanupMaterialsTime);

	/** List of map referenced StaticMeshes that can/will be spawned - these will not have material cleanup performed on them */
	TMap<FString,UBOOL> PotentiallySpawnedStaticMeshes;
	/** The dependency chain used to find the potentially spawned static meshes */
	TSet<FDependencyRef> FoundDependencies;

	PotentiallySpawnedStaticMeshes.Empty();
	if (bCleanStaticMeshMaterials == TRUE)
	{
		if ((Package->PackageFlags & PKG_ContainsMap) != 0)
		{
			ULinkerLoad* LinkerLoad = Package->GetLinker();
			if (LinkerLoad == NULL)
			{
				// Create a new linker object which goes off and tries load the file.
				LinkerLoad = GetPackageLinker(NULL, *(Package->GetName()), LOAD_Throw, NULL, NULL );
			}
			if (LinkerLoad != NULL)
			{
				// To avoid pulling in classes from non-native packages, we have to generate this list each time.
				TArray<UClass*> ClassesToCheck;
				for (TMap<FString,UBOOL>::TIterator ClassIt(CheckStaticMeshCleanClasses); ClassIt; ++ClassIt)
				{
					// Try to find the class of interest...
					UClass* CheckClass = (UClass*)(UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *(ClassIt.Key()), TRUE));
					if (CheckClass != NULL)
					{
						debugfSlow(TEXT("Found class to check for static meshes: %s"), *(CheckClass->GetPathName()));
						ClassesToCheck.AddItem(CheckClass);
					}
				}

				if (ClassesToCheck.Num() > 0)
				{
					for (INT ExportIdx = 0; ExportIdx < LinkerLoad->ExportMap.Num(); ExportIdx++)
					{
						FObjectExport& Export = LinkerLoad->ExportMap(ExportIdx);
						if (Export.ClassIndex != 0)
						{
							FName ClassName = LinkerLoad->GetExportClassName(ExportIdx);
							UClass* LoadClass = (UClass*)(UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *(ClassName.ToString()), TRUE));
							UBOOL bGatherExportDependencies = FALSE;
							if (LoadClass != NULL)
							{
								for (INT CheckClassIdx = 0; CheckClassIdx < ClassesToCheck.Num(); CheckClassIdx++)
								{
									UClass* CheckClass = ClassesToCheck(CheckClassIdx);
									if (CheckClass != NULL)
									{
										if (LoadClass->IsChildOf(CheckClass))
										{
											bGatherExportDependencies = TRUE;
											break;
										}
									}
								}
							}

							if (bGatherExportDependencies == TRUE)
							{
								// Gather the dependecies for this object
								LinkerLoad->GatherExportDependencies(ExportIdx, FoundDependencies);
							}
						}
					}
				}

				// Check the found dependencies
				for (TSet<FDependencyRef>::TConstIterator It(FoundDependencies); It; ++It)
				{
					const FDependencyRef& Ref = *It;
					FObjectExport& DepExport = Ref.Linker->ExportMap(Ref.ExportIndex);
					if (DepExport.ClassIndex != 0)
					{
						FName ClassName = Ref.Linker->GetExportClassName(Ref.ExportIndex);
						UClass* DepClass = (UClass*)(UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *(ClassName.ToString()), TRUE));
						if (DepClass == UStaticMesh::StaticClass())
						{
							FString StaticMeshName = Ref.Linker->GetExportPathName(Ref.ExportIndex);
							PotentiallySpawnedStaticMeshes.Set(StaticMeshName, TRUE);
						}
					}
				}
			}
		}
	}

	TArray<UMaterial*> FoundMaterials;
	TArray<UMaterialInstanceConstant*> FoundConstant;
	TArray<UMaterialInstanceTimeVarying*> FoundTimeVarying;
	TMap<UStaticMesh*,TArray<UStaticMeshComponent*>> FoundStaticMeshComponents;
	TMap<UStaticMesh*,UBOOL> MeshEmitterStaticMeshes;
	TMap<UTexture*,UBOOL> ClearedExpressionTexturesMap;
	TMap<UTexture*,UBOOL> UsedTexturesMap;

	for (FObjectIterator It; It; ++It)
	{
		UObject* Object = *It;
		UMaterial* Material =  Cast<UMaterial>(Object);;
		UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Object);
		UMaterialInstanceTimeVarying* MITV = Cast<UMaterialInstanceTimeVarying>(Object);
		UBOOL bProcessMaterial = (bSkipStartupObjectDetermination == FALSE) ? (Material || MIC || MITV) : FALSE;
		if (Object->IsIn(Package) || Object->HasAnyFlags(RF_ForceTagExp) || bProcessMaterial)
		{
			UStaticMeshComponent* SMComp = Cast<UStaticMeshComponent>(Object);
			UParticleSystem* PSys = Cast<UParticleSystem>(Object);
			if (Material != NULL)
			{
				FoundMaterials.AddUniqueItem(Material);
			}
			else if (MIC != NULL)
			{
				FoundConstant.AddUniqueItem(MIC);
			}
			else if (MITV != NULL)
			{
				FoundTimeVarying.AddUniqueItem(MITV);
			}
			else if (SMComp != NULL)
			{
				if ((SMComp->StaticMesh != NULL) && (SMComp->StaticMesh->LODModels.Num() == 1))
				{
					TArray<UStaticMeshComponent*>* SMComponents = FoundStaticMeshComponents.Find(SMComp->StaticMesh);
					if (SMComponents == NULL)
					{
						TArray<UStaticMeshComponent*> TempList;
						FoundStaticMeshComponents.Set(SMComp->StaticMesh, TempList);
						SMComponents = FoundStaticMeshComponents.Find(SMComp->StaticMesh);
					}
					check(SMComponents);
					SMComponents->AddUniqueItem(SMComp);
				}
				else
				{
					if (SMComp->StaticMesh != NULL)
					{
						debugfSlow(NAME_Log, TEXT("Skipping static mesh due to %d LOD levels: %s"),
							SMComp->StaticMesh->LODModels.Num(), *(SMComp->StaticMesh->GetPathName()));
					}
					else
					{
						debugfSlow(NAME_Log, TEXT("Skipping static mesh due to NULL mesh"));
					}
				}
			}
			else if ((PSys != NULL) && (bCleanStaticMeshMaterials == TRUE))
			{
				// See if there are any mesh emitters and if so, add them to the MeshEmitter ref'd list
				for (INT EmitterIdx = 0; EmitterIdx < PSys->Emitters.Num(); EmitterIdx++)
				{
					UParticleEmitter* Emitter = PSys->Emitters(EmitterIdx);
					if ((Emitter != NULL) && (Emitter->LODLevels.Num() > 0))
					{
						UBOOL bIsAMeshEmitter = TRUE;
						for (INT LODIdx = 0; (LODIdx < Emitter->LODLevels.Num()) && (bIsAMeshEmitter == TRUE); LODIdx++)
						{
							UParticleLODLevel* LODLevel = Emitter->LODLevels(0);
							if (LODLevel != NULL)
							{
								UParticleModuleTypeDataMesh* MeshTD = Cast<UParticleModuleTypeDataMesh>(LODLevel->TypeDataModule);
								if (MeshTD != NULL)
								{
									if (MeshTD->Mesh != NULL)
									{
										MeshEmitterStaticMeshes.Set(MeshTD->Mesh, TRUE);
									}
								}
								else
								{
									bIsAMeshEmitter = FALSE;
								}
							}
						}
					}
				}
			}
		}
	}

	// 
	if (bCleanStaticMeshMaterials == TRUE)
	{
		for (TMap<UStaticMesh*,TArray<UStaticMeshComponent*>>::TIterator SMCompIt(FoundStaticMeshComponents); SMCompIt; ++SMCompIt)
		{
			UStaticMesh* StaticMesh = SMCompIt.Key();
			TArray<UStaticMeshComponent*>& SMComponents = SMCompIt.Value();

			if (StaticMesh != NULL)
			{
				UBOOL bUsedByMeshEmitter = (MeshEmitterStaticMeshes.Find(StaticMesh) != NULL);
				UBOOL bUsedByScript = (ScriptReferencedStaticMeshes.Find(StaticMesh->GetPathName()) != NULL);
				UBOOL bPotentiallySpawned = (PotentiallySpawnedStaticMeshes.Find(StaticMesh->GetPathName()) != NULL);
				UBOOL bInSkipPackage = (SkipStaticMeshCleanPackages.Find(StaticMesh->GetOutermost()->GetName()) != NULL);
				UBOOL bSkipStaticMesh = (SkipStaticMeshCleanMeshes.Find(StaticMesh->GetPathName()) != NULL);
				if (!bUsedByMeshEmitter && !bUsedByScript && !bPotentiallySpawned && !bInSkipPackage && !bSkipStaticMesh)
				{
					if (StaticMesh->LODModels.Num() == 1)
					{
						// Make a list of the default materials from the source static mesh
						FStaticMeshRenderData& LODModel = StaticMesh->LODModels(0);
						TArray<UMaterialInterface*> DefaultMaterials;
						for (INT ElementIdx = 0; ElementIdx < LODModel.Elements.Num(); ElementIdx++)
						{
							FStaticMeshElement& Element = LODModel.Elements(ElementIdx);
							DefaultMaterials.AddItem(Element.Material);
						}
						INT DefaultMaterialCount = DefaultMaterials.Num();

						// Fill in the arrays for each SMComp
						for (INT SMCIdx = 0; SMCIdx < SMComponents.Num(); SMCIdx++)
						{
							UStaticMeshComponent* SMComp = SMComponents(SMCIdx);
							if (SMComp)
							{
								// Make sure there is enough space in the array
								if (SMComp->Materials.Num() < DefaultMaterialCount)
								{
									SMComp->Materials.AddZeroed(DefaultMaterialCount - SMComp->Materials.Num());
								}

								// Set them (if they aren't already
								for (INT SetIdx = 0; SetIdx < DefaultMaterialCount; SetIdx++)
								{
									if (SMComp->Materials(SetIdx) == NULL)
									{
										SMComp->Materials(SetIdx) = DefaultMaterials(SetIdx);
									}
								}
							}
						}

						// Clear out the material(s) on the source static mesh
						for (INT ElementIdx = 0; ElementIdx < LODModel.Elements.Num(); ElementIdx++)
						{
							FStaticMeshElement& Element = LODModel.Elements(ElementIdx);
							Element.Material = NULL;
						}
					}
					else
					{
						debugfSlow(NAME_Log, TEXT("Skipping static mesh due to %d LOD levels: %s"),
							StaticMesh->LODModels.Num(), *(StaticMesh->GetPathName()));
					}
				}
				else
				{
					if (bUsedByScript == TRUE)
					{
						debugfSlow(NAME_Log, TEXT("Skipping static mesh due to being referenced by script: %s"),
							*(StaticMesh->GetPathName()));
					}
					else if (bUsedByMeshEmitter == TRUE)
					{
						debugfSlow(NAME_Log, TEXT("Skipping static mesh due to being used in a mesh emitter: %s"),
							*(StaticMesh->GetPathName()));
					}
					else if (bInSkipPackage == TRUE)
					{
						debugfSlow(NAME_Log, TEXT("Skipping static mesh due to being in a package in the SkipPackage list: %s"),
							*(StaticMesh->GetPathName()));
					}
					else if (bSkipStaticMesh == TRUE)
					{
						debugfSlow(NAME_Log, TEXT("Skipping static mesh due to being in the SkipMesh list: %s"),
							*(StaticMesh->GetPathName()));
					}
					else	//bPotentiallySpawned
					{
						debugfSlow(NAME_Log, TEXT("Skipping static mesh due to potentially dynamically being spawned: %s"),
							*(StaticMesh->GetPathName()));
					}
				}
			}
		}
	}

	// If we found materials... clean them up first
	TArray<UTexture*> PreliminaryReferencedTextures;
	TArray<UTexture*> ParameterReferencedTextures;
	for (INT MaterialIdx = 0; MaterialIdx < FoundMaterials.Num(); MaterialIdx++)
	{
		UMaterial* Material = FoundMaterials(MaterialIdx);
		// Find all of the referenced expressions
		TMap<UMaterialExpression*,UBOOL> ReferencedExpressionsMap;
		for (INT MPIdx = 0; MPIdx < MP_MAX; MPIdx++)
		{
			EMaterialProperty MaterialProp = EMaterialProperty(MPIdx);
			TArray<UMaterialExpression*> MPRefdExpressions;
			if (Material->GetExpressionsInPropertyChain(MaterialProp, MPRefdExpressions, NULL) == TRUE)
			{
				for (INT AddIdx = 0; AddIdx < MPRefdExpressions.Num(); AddIdx++)
				{
					ReferencedExpressionsMap.Set(MPRefdExpressions(AddIdx), TRUE);
				}
			}
		}

		// Remove any unreferenced expressions
		for (INT ExpIdx = Material->Expressions.Num() - 1; ExpIdx >= 0; ExpIdx--)
		{
			UMaterialExpression* Expression = Material->Expressions(ExpIdx);
			UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(Expression);
			UMaterialExpressionTextureSampleParameter* TextureSampleParameter = Cast<UMaterialExpressionTextureSampleParameter>(Expression);
			if (ReferencedExpressionsMap.Find(Expression) == NULL) //&& (TextureSampleParameter == NULL))
			{
				if ((TextureSample != NULL) && (TextureSample->Texture != NULL))
				{
					ClearedExpressionTexturesMap.Set(TextureSample->Texture, TRUE);
					PreliminaryReferencedTextures.AddUniqueItem(TextureSample->Texture);
					if (TextureSampleParameter != NULL)
					{
						TextureSample->Texture = NULL;
					}
				}

				if (Expression->bIsParameterExpression == TRUE)
				{
					Expression->ClearInputExpressions();
				}
				else
				{
					Expression->ClearFlags(RF_ForceTagExp);
					Expression->SetFlags(RF_MarkedByCooker|RF_Transient|RF_MarkedByCookerTemp);
					Material->Expressions.Remove(ExpIdx);
				}
			}
			else
			{
				// It was referenced by a valid expression - although that expression may
				// not actually be used (static switches, etc.)
				if ((TextureSample != NULL) && (TextureSample->Texture != NULL))
				{
					PreliminaryReferencedTextures.AddUniqueItem(TextureSample->Texture);
					if (TextureSampleParameter != NULL)
					{
						ParameterReferencedTextures.AddUniqueItem(TextureSample->Texture);
					}
				}
			}
		}

		// Since we have to traverse ALL potentially valid branches of the material, we
		// can still end up with textures that aren't actually used. Check them versus
		// the actual UsedTextures from the material.
		TArray<UTexture*> UsedTextures;
		Material->GetUsedTextures(UsedTextures);
		for (INT AddIdx = 0; AddIdx < UsedTextures.Num(); AddIdx++)
		{
			UTexture* Texture = UsedTextures(AddIdx);
			UsedTexturesMap.Set(Texture, TRUE);
		}
		for (INT ParamIdx = 0; ParamIdx < ParameterReferencedTextures.Num(); ParamIdx++)
		{
			UTexture* Texture = ParameterReferencedTextures(ParamIdx);
			if (UsedTexturesMap.Find(Texture) == NULL)
			{
				UsedTexturesMap.Set(Texture, TRUE);
			}
		}
	}

	// Now cleanup the MICs
	for (INT MICIdx = 0; MICIdx < FoundConstant.Num(); MICIdx++)
	{
		UMaterialInstanceConstant* MIC = FoundConstant(MICIdx);
		if (MIC->Parent)
		{
			// Find all of the referenced texture parameters from the parent
			// This assumes the parent has been loaded!
			TMap<FName,UTexture*> RefdTextureParamsMap;
			GetMaterialInstanceReferencedTextureParams(MIC, RefdTextureParamsMap);

			// Remove any texture parameter values that were not found
			for (INT CheckIdx = MIC->TextureParameterValues.Num() - 1; CheckIdx >= 0; CheckIdx--)
			{
				FName ParameterName = MIC->TextureParameterValues(CheckIdx).ParameterName;
				UTexture*const* ParentTexture = RefdTextureParamsMap.Find(ParameterName);
				// Note we have to exclude discarding Weightmap texture parameters by name, because we can only know the parameter 
				// name by actually compiling the static permutation to know which weightmap the UMaterialExpressionTerrainLayerWeight
				// node will need to access.
				if (ParentTexture == NULL && ParameterName.ToString().Left(9) != TEXT("Weightmap") )
				{
					// Parameter wasn't found... remove it
					//@todo. Remove the entire entry?
					//TextureParameterValues.Remove(CheckIdx);
					ClearedExpressionTexturesMap.Set(MIC->TextureParameterValues(CheckIdx).ParameterValue, TRUE);
					MIC->TextureParameterValues(CheckIdx).ParameterValue = NULL;
				}
			}

			TArray<UTexture*> UsedTextures;
			MIC->GetUsedTextures(UsedTextures);
			for (INT SafeIdx = 0; SafeIdx < UsedTextures.Num(); SafeIdx++)
			{
				UTexture* Texture = UsedTextures(SafeIdx);
				UsedTexturesMap.Set(Texture, TRUE);
			}
		}
	}

	// Now cleanup the MITVs
	for (INT MITVIdx = 0; MITVIdx < FoundTimeVarying.Num(); MITVIdx++)
	{
		UMaterialInstanceTimeVarying* MITV = FoundTimeVarying(MITVIdx);
		if (MITV->Parent)
		{
			// Find all of the referenced texture parameters from the parent
			// This assumes the parent has been loaded!
			TMap<FName,UTexture*> RefdTextureParamsMap;
			GetMaterialInstanceReferencedTextureParams(MITV, RefdTextureParamsMap);

			// Remove any texture parameter values that were not found
			for (INT CheckIdx = MITV->TextureParameterValues.Num() - 1; CheckIdx >= 0; CheckIdx--)
			{
				UTexture*const* ParentTexture = RefdTextureParamsMap.Find(MITV->TextureParameterValues(CheckIdx).ParameterName);
				if (ParentTexture == NULL)
				{
					// Parameter wasn't found... remove it
					//@todo. Remove the entire entry?
					//TextureParameterValues.Remove(CheckIdx);
					ClearedExpressionTexturesMap.Set(MITV->TextureParameterValues(CheckIdx).ParameterValue, TRUE);
					MITV->TextureParameterValues(CheckIdx).ParameterValue = NULL;
				}
			}

			TArray<UTexture*> UsedTextures;
			MITV->GetUsedTextures(UsedTextures);
			for (INT SafeIdx = 0; SafeIdx < UsedTextures.Num(); SafeIdx++)
			{
				UTexture* Texture = UsedTextures(SafeIdx);
				UsedTexturesMap.Set(Texture, TRUE);
			}
		}
	}

	for (INT CheckIdx = 0; CheckIdx < PreliminaryReferencedTextures.Num(); CheckIdx++)
	{
		UTexture* PotentiallyRemovedTexture = PreliminaryReferencedTextures(CheckIdx);
		if (UsedTexturesMap.Find(PotentiallyRemovedTexture) == NULL)
		{
			ClearedExpressionTexturesMap.Set(PotentiallyRemovedTexture, TRUE);
		}
	}

	// Now, see if anything else references the 'cleared' textures
	TArray<UTexture*> ClearedTextures;
	for (TMap<UTexture*,UBOOL>::TIterator It(ClearedExpressionTexturesMap); It; ++It)
	{
		UTexture* Texture = It.Key();
		ClearedTextures.AddItem(Texture);
	}

	if (ClearedTextures.Num() > 0)
	{
		for (INT TextureIdx = ClearedTextures.Num() - 1; TextureIdx >= 0; TextureIdx--)
		{
			UTexture* Texture = ClearedTextures(TextureIdx);
			if (Texture != NULL)
			{
				UBOOL bReferenced = FALSE;
				UObject* Referencer = NULL;
				
				if (UsedTexturesMap.Find(Texture) == NULL)
				{
					// We didn't find it in the tagged used textures...
					// See what else is referencing it.
					TArray<FReferencerInformation> OutInternalReferencers;
					TArray<FReferencerInformation> OutExternalReferencers;
					Texture->RetrieveReferencers(&OutInternalReferencers, &OutExternalReferencers, FALSE);
					for (INT OIRefIdx = 0; OIRefIdx < OutInternalReferencers.Num(); OIRefIdx++)
					{
						FReferencerInformation& InternalReferencer = OutInternalReferencers(OIRefIdx);
						if (InternalReferencer.Referencer != NULL)
						{
							if ((InternalReferencer.Referencer->HasAnyFlags(RF_Transient|RF_MarkedByCooker) == FALSE) &&
								(InternalReferencer.Referencer->IsA(UMaterialExpression::StaticClass()) == FALSE))
							{
								// It's referenced!
								Referencer = InternalReferencer.Referencer;
								bReferenced = TRUE;
								break;
							}
						}
					}

					for (INT OERefIdx = 0; OERefIdx < OutExternalReferencers.Num(); OERefIdx++)
					{
						FReferencerInformation& ExternalReferencer = OutExternalReferencers(OERefIdx);
						if (ExternalReferencer.Referencer != NULL)
						{
							if ((ExternalReferencer.Referencer->HasAnyFlags(RF_Transient|RF_MarkedByCooker) == FALSE) &&
								(ExternalReferencer.Referencer->IsA(UMaterialExpression::StaticClass()) == FALSE))
							{
								// It's referenced!
								Referencer = ExternalReferencer.Referencer;
								bReferenced = TRUE;
								break;
							}
						}
					}
				}
				else
				{
					bReferenced = TRUE;
				}

				if (bReferenced == FALSE)
				{
					Texture->SetFlags(RF_MarkedByCooker|RF_Transient|RF_MarkedByCookerTemp);
					Texture->ClearFlags(RF_ForceTagExp);
				}
			}
		}
	}
}

/**
 *	Bake and prune all matinee sequences that are tagged as such.
 */
void UCookPackagesCommandlet::BakeAndPruneMatinee( UPackage* Package )
{
	// Bake and prune matinees when cooking for console.
	if (GCookingTarget & UE3::PLATFORM_Stripped)
	{
		check(Package);
		check(GEditor);
		// find all SeqAct_Interp which are referenced by something in the map
		UWorld* World = FindObject<UWorld>( Package, TEXT("TheWorld") );
		if ( World != NULL )
		{
			TArray<ULevel*> LevelsToSearch = World->Levels;

			// make sure that the world's PersistentLevel is part of the levels array for the purpose of this test.
			if ( World->PersistentLevel != NULL )
			{
				LevelsToSearch.AddUniqueItem(World->PersistentLevel);
			}

			if (bAllowBakeAndPruneOverride == FALSE)
			{
				// Bake animsets...
				for ( INT LevelIndex = 0; LevelIndex < LevelsToSearch.Num(); LevelIndex++ )
				{
					ULevel* Level = LevelsToSearch(LevelIndex);
					GEditor->BakeAnimSetsInLevel(Level, NULL);
				}

				// Prune animsets...
				for ( INT LevelIndex = 0; LevelIndex < LevelsToSearch.Num(); LevelIndex++ )
				{
					ULevel* Level = LevelsToSearch(LevelIndex);
					GEditor->PruneAnimSetsInLevel(Level, NULL);
					GEditor->ClearBakeAndPruneStatus(Level);
				}
			}
			else
			{
				// Bake animsets...
				// Prune animsets...
				for ( INT LevelIndex = 0; LevelIndex < LevelsToSearch.Num(); LevelIndex++ )
				{
					TMap<FString,UBOOL> AnimSetSkipBakeAndPruneMap;
					ULevel* Level = LevelsToSearch(LevelIndex);
					// Clear out unreferenced animsets from groups...
					GEditor->ClearUnreferenceAnimSetsFromGroups(Level);
//					for (TObjectIterator<UInterpData> IterpDataIt; IterpDataIt; ++IterpDataIt)
//					{
//						UInterpData* InterpData = *IterpDataIt;
//						if (InterpData->IsIn(Level))
//						{
//							InterpData->bShouldBakeAndPrune = TRUE;
//						}
//					}
					GEditor->GatherBakeAndPruneStatus(Level, &AnimSetSkipBakeAndPruneMap);
					GEditor->BakeAnimSetsInLevel(Level, &AnimSetSkipBakeAndPruneMap);
					GEditor->PruneAnimSetsInLevel(Level, &AnimSetSkipBakeAndPruneMap);
					GEditor->ClearBakeAndPruneStatus(Level);
				}
			}
		}
	}
}

/**
 * Make sure materials are compiled for the target platform and add them to the shader cache embedded 
 * into seekfree packages.
 * @param Material - Material to process
 */
void UCookPackagesCommandlet::CompileMaterialShaders(UMaterial* Material)
{	
	check(Material);
	if( !Material->HasAnyFlags( RF_ClassDefaultObject ) 
		&& ShaderCache
		&& !AlreadyHandledMaterials.Find( Material->GetFullName() )
		)
	{
		// compile the material
		TRefCountPtr<FMaterialShaderMap> MaterialShaderMapRef;
		FMaterialResource* MaterialResource = Material->GetMaterialResource();
		check(MaterialResource);

		// create an empty static parameter set with just the Id since this is the base material
		FStaticParameterSet EmptySet(MaterialResource->GetId());
		if(MaterialResource->Compile( &EmptySet, ShaderPlatform, GCookingMaterialQuality, MaterialShaderMapRef, FALSE ))
		{
			check(MaterialShaderMapRef);
			// add the material's shader map to the shader cache being saved into the seekfree package
			ShaderCache->AddMaterialShaderMap( MaterialShaderMapRef );
			MaterialShaderMapRef->BeginRelease();
		}
		else if (Material->bUsedAsSpecialEngineMaterial)
		{
			appErrorf(TEXT("Failed to compile default material %s for platform %s!"), *Material->GetPathName(), ShaderPlatformToText(ShaderPlatform));
		}
		else
		{
			warnf(NAME_Warning, TEXT("Failed to compile Material %s for platform %s, Default Material will be used in game."), 
				*Material->GetPathName(), 
				ShaderPlatformToText(ShaderPlatform));

			DumpShaderCompileErrors( MaterialResource, NULL );
		}
	}
}

/**
* Make sure material instances are compiled and add them to the shader cache embedded into seekfree packages.
* @param MaterialInstance - MaterialInstance to process
*/
void UCookPackagesCommandlet::CompileMaterialInstanceShaders(UMaterialInstance* MaterialInstance)
{	
	check(MaterialInstance);
	//only process if the material instance has a static permutation 
	if( !MaterialInstance->HasAnyFlags( RF_ClassDefaultObject ) 
		&& MaterialInstance->bHasStaticPermutationResource
		&& ShaderCache
		&& !AlreadyHandledMaterialInstances.Find( MaterialInstance->GetFullName() )
		)
	{
		// compile the material instance's shaders for the target platform
		MaterialInstance->CacheResourceShaders(ShaderPlatform, FALSE);

		FMaterialResource* MaterialResource = MaterialInstance->GetMaterialResource();
		check(MaterialResource);
		TRefCountPtr<FMaterialShaderMap> MaterialShaderMapRef = MaterialResource->GetShaderMap();
		if (MaterialShaderMapRef)
		{
			// add the material's shader map to the shader cache being saved into the seekfree package
			ShaderCache->AddMaterialShaderMap( MaterialShaderMapRef );
			// Don't set the shader map to NULL as we need it later when cleaning up the shader cache!
			if (bSkipStartupObjectDetermination == TRUE)
			{
				MaterialResource->SetShaderMap(NULL);
			}
		}
		else
		{
			const UMaterial* BaseMaterial = MaterialInstance->GetMaterial();
			warnf(NAME_Warning, TEXT("Failed to compile Material Instance %s with Base %s for platform %s, Default Material will be used in game."), 
				*MaterialInstance->GetPathName(), 
				BaseMaterial ? *BaseMaterial->GetName() : TEXT("Null"), 
				ShaderPlatformToText(ShaderPlatform));

			DumpShaderCompileErrors( MaterialResource, NULL );
		}
	}
}

/**
 * Determine if source package of given object is newer than timestamp
 *
 * @param Object - object to check source package file timestamp
 * @param TimeToCheck - time stamp to check against
 * @return TRUE if object source package is newere than the timestamp
 **/
static UBOOL IsSourcePackageFileNewer(UObject* Object, DOUBLE TimeToCheck)
{
	UBOOL bResult = TRUE;
	// compare source package file timestamp
	ULinkerLoad* Linker = Object->GetLinker();
	if( Linker )
	{
		DOUBLE SrcFileTime = GFileManager->GetFileTimestamp(*Linker->Filename);
		if( SrcFileTime > 0 )
		{
			// src packge is newer
			bResult = SrcFileTime > TimeToCheck;
		}
	}
	return bResult;
}

/**
 * Prepares object for saving into package. Called once for each object being saved 
 * into a new package.
 *
 * @param	Package						Package going to be saved
 * @param	Object						Object to prepare
 * @param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
 * @param	bIsTextureOnlyFile			Whether file is only going to contain texture mips
 */
void UCookPackagesCommandlet::PrepareForSaving(UPackage* Package, UObject* Object, UBOOL bIsSavedInSeekFreePackage, UBOOL bIsTextureOnlyFile)
{
	SCOPE_SECONDS_COUNTER_COOK(PrepareForSavingTime);

	// Don't serialize editor-only objects!
	if (Platform != PLATFORM_Windows)
	{
		if ((Object->IsIn(Package) || Object->HasAllFlags(RF_ForceTagExp)) &&
		(
			// See if the object is
			// LoadForEdit & !LoadForClient & !LoadForServer (editor only)
			(Object->HasAnyFlags(RF_LoadForEdit) && !Object->HasAnyFlags(RF_LoadForClient) && !Object->HasAnyFlags(RF_LoadForServer)) 
			))
		{
			Object->ClearFlags(RF_ForceTagExp);
			Object->ClearFlags(RF_TagExp);
			Object->SetFlags(RF_Transient);
			return;
		}
	}

	// See if it's in the 'never cook' list...
	if (NeverCookObjects.Find(Object->GetPathName()) != NULL || Object->IsA(AGroupActor::StaticClass()))
	{
		Object->ClearFlags(RF_ForceTagExp);
		Object->ClearFlags(RF_TagExp);
		Object->SetFlags(RF_Transient);
		return;
	}

	// Prepare texture for saving unless it's the class default object.
	UBOOL bIsDLC = (DLCName.Len() > 0);
	UTexture2D* Texture2D = Cast<UTexture2D>(Object);
	if( Texture2D && !Texture2D->HasAnyFlags( RF_ClassDefaultObject ) )
	{
		SCOPE_SECONDS_COUNTER_COOK(PrepareForSavingTextureTime);

		// We mark lightmaps we don't want as transient, so check for it here...
		if (Texture2D->HasAnyFlags(RF_Transient) == TRUE)
		{
			return;
		}

		// Whether the texture can be streamed.
		UBOOL	bIsStreamingTexture	= !Texture2D->NeverStream;
		// Whether the texture is applied to the face of a cubemap, in which case we can't stream it.
		UBOOL	bIsCubemapFace		= Texture2D->GetOuter()->IsA(UTextureCube::StaticClass());

		// Textures residing in seekfree packages cannot be streamed as the file offset is not known till after
		// they are saved. Exception: If we're using a TFC, allow lightmaps to be streamed.
		if( ShouldBeANeverStreamTexture( Texture2D, bIsSavedInSeekFreePackage ) == TRUE )
		{
			bIsStreamingTexture		= FALSE;
			Texture2D->NeverStream	= TRUE;
		}

		if (!bIsCubemapFace && ((Platform & PLATFORM_Mobile) != 0))
		{
			// on mobile, all textures are "streamed" in that they live in a .tfc - this is to minimize download sizes by not
			// sharing any textures, and seeking isn't important on a flash memory device
			Texture2D->NeverStream = FALSE;
			bIsStreamingTexture	= TRUE;
			// on mobile, we use a cooking path that looks at mip tail index, so max this out
			Texture2D->MipTailBaseIdx = 99;
		}

		// Streaming textures will use the texture file cache if it is enabled.
		if( bUseTextureFileCache && bIsStreamingTexture )
		{
			if (bIsMTChild)
			{
				// strip off numeric appendage...that only goes in the per-mip info
				Texture2D->TextureFileCacheName = FName(*FName(*GetTextureCacheName(Texture2D)).GetNameString());
			}
			else
			{
				Texture2D->TextureFileCacheName = FName(*GetTextureCacheName(Texture2D));
			}
		}
		// Disable use of texture file cache.
		else
		{
			Texture2D->TextureFileCacheName = NAME_None;
		}
		// TRUE if the texture mips can be stored in a separate file
		const UBOOL bAllowStoreInSeparateFile = bIsStreamingTexture && bIsSavedInSeekFreePackage;

		// see if we have an existing cooked entry for the texture in the TFC
		FCookedTextureFileCacheInfo* TextureFileCacheInfo = NULL;
		// if texture has a matching GUID and has been already been saved in the TFC
		UBOOL bHasMatchingGuidTFC = FALSE;
		
		if( bUseTextureFileCache && bAllowStoreInSeparateFile )
		{
			if( DLCName.Len() )
			{
				// Check for the texture residing in the shipping TFCs (if we're cooking for DLC)
				TextureFileCacheInfo = GShippingCookerData->GetTextureFileCacheEntryInfo(Texture2D);
				if( TextureFileCacheInfo )
				{
					// compare Guids
					if (TextureFileCacheInfo->TextureFileCacheGuid != Texture2D->TextureFileCacheGuid)
					{
						appErrorf( TEXT("Shipping texture updated for DLC; this is not allowed (%s)"), *Texture2D->GetFullName() );
					}
					else
					{
						// Ensure the texture is in the same cache it was...
						bHasMatchingGuidTFC = TRUE;
					}
				}
			}
		}	
		
		if (Platform != PLATFORM_Android)
		{
			PrepareTextureForSaving(Package, Texture2D,  TextureFileCacheInfo, bIsSavedInSeekFreePackage, bIsTextureOnlyFile, bIsDLC, bIsStreamingTexture, bAllowStoreInSeparateFile, bHasMatchingGuidTFC);
		}
		else
		{
			if (AndroidTextureFormat & TEXSUPPORT_DXT)
			{
				PrepareTextureForSaving(Package, Texture2D, TextureFileCacheInfo, bIsSavedInSeekFreePackage, bIsTextureOnlyFile, bIsDLC, bIsStreamingTexture, bAllowStoreInSeparateFile, bHasMatchingGuidTFC, TEXSUPPORT_DXT);
			}
			if (AndroidTextureFormat & TEXSUPPORT_PVRTC)
			{
				PrepareTextureForSaving(Package, Texture2D, TextureFileCacheInfo, bIsSavedInSeekFreePackage, bIsTextureOnlyFile, bIsDLC, bIsStreamingTexture, bAllowStoreInSeparateFile, bHasMatchingGuidTFC, TEXSUPPORT_PVRTC);
			}
			if (AndroidTextureFormat & TEXSUPPORT_ATITC)
			{
				PrepareTextureForSaving(Package, Texture2D, TextureFileCacheInfo, bIsSavedInSeekFreePackage, bIsTextureOnlyFile, bIsDLC, bIsStreamingTexture, bAllowStoreInSeparateFile, bHasMatchingGuidTFC, TEXSUPPORT_ATITC);
			}
			if (AndroidTextureFormat & TEXSUPPORT_ETC)
			{
				PrepareTextureForSaving(Package, Texture2D, TextureFileCacheInfo, bIsSavedInSeekFreePackage, bIsTextureOnlyFile, bIsDLC, bIsStreamingTexture, bAllowStoreInSeparateFile, bHasMatchingGuidTFC, TEXSUPPORT_ETC);
			}
		}

		if( bUseTextureFileCache && bAllowStoreInSeparateFile )
		{
			// update GUIDs for this texture since it was saved in the TFC
			if( TextureFileCacheInfo )
			{
				TextureFileCacheInfo->TextureFileCacheGuid = Texture2D->TextureFileCacheGuid;
			}
			else
			{
				// add a new entry if dont have an existing one
				FCookedTextureFileCacheInfo NewTextureFileCacheInfo;
				NewTextureFileCacheInfo.TextureFileCacheGuid = Texture2D->TextureFileCacheGuid;				
				NewTextureFileCacheInfo.TextureFileCacheName = Texture2D->TextureFileCacheName;
				PersistentCookerData->SetTextureFileCacheEntryInfo(Texture2D,NewTextureFileCacheInfo);
			}			
		}

		if (bUseTextureFileCache && bVerifyTextureFileCache && (Texture2D->TextureFileCacheName != NAME_None))
		{
			// Add it to the tracking data...
			AddVerificationTextureFileCacheEntry(Package, Texture2D, bIsSavedInSeekFreePackage);
		}
	}

	ATerrain* Terrain = Cast<ATerrain>(Object);
	if (Terrain)
	{
		SCOPE_SECONDS_COUNTER_COOK(PrepareForSavingTerrainTime);
		TArrayNoInit<FTerrainMaterialResource*>& CachedMaterials = Terrain->GetCachedTerrainMaterials();
		// Make sure materials are compiled for the platform and add them to the shader cache embedded into seekfree packages.
		for (INT CachedMatIndex = 0; CachedMatIndex < CachedMaterials.Num(); CachedMatIndex++)
		{
			FTerrainMaterialResource* TMatRes = CachedMaterials(CachedMatIndex);
			if( TMatRes && ShaderCache )
			{
				// Compile the material...
				TRefCountPtr<FMaterialShaderMap> MaterialShaderMapRef;
				FStaticParameterSet EmptySet(TMatRes->GetId());
				if (TMatRes->Compile(&EmptySet, ShaderPlatform, MSQ_TERRAIN, MaterialShaderMapRef, FALSE))
				{
					check(MaterialShaderMapRef);
					// ... and add it to the shader cache being saved into the seekfree package.
					ShaderCache->AddMaterialShaderMap( MaterialShaderMapRef );
					MaterialShaderMapRef->BeginRelease();
				}
			}
		}
	}

	// make sure the level doesn't reference any textures for streaming, since they could get pulled in unnecessarily
	if (Platform & PLATFORM_Mobile)
	{
		ULevel* Level = Cast<ULevel>(Object);
		if (Level)
		{
			Level->TextureToInstancesMap.Empty();
			Level->DynamicTextureInstances.Empty();
		}
	}

	// Compile shaders for materials
	UMaterial* Material = Cast<UMaterial>(Object);
	if( Material && !Material->HasAnyFlags(RF_ClassDefaultObject) ) 
	{
		SCOPE_SECONDS_COUNTER_COOK(PrepareForSavingMaterialTime);
		CompileMaterialShaders(Material);
	}

	// Compile shaders for material instances with static parameters
	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Object);
	if( MaterialInstance && !MaterialInstance->HasAnyFlags(RF_ClassDefaultObject) ) 
	{
		SCOPE_SECONDS_COUNTER_COOK(PrepareForSavingMaterialInstanceTime);
		CompileMaterialInstanceShaders(MaterialInstance);
	}

	// 'Cook-out' material expressions on consoles
	// In this case, simply don't load them on the client
	// Keep 'parameter' types around so that we can get their defaults.
	if ((GCookingTarget & UE3::PLATFORM_Stripped) != 0)
	{
		UMaterialExpression* MatExp = Cast<UMaterialExpression>(Object);
		if (MatExp)
		{
			if ((MatExp->IsTemplate() == FALSE) && (MatExp->bIsParameterExpression == FALSE))
			{
				if (bCleanupMaterials == TRUE)
				{
					MatExp->ClearFlags(RF_ForceTagExp);
					MatExp->ClearFlags(RF_TagExp);
					MatExp->SetFlags(RF_MarkedByCooker|RF_Transient|RF_MarkedByCookerTemp);
				}
				else
				{
					// The old behavior... which didn't really work correctly.
					// It also didn't prevent unreferenced textures (ones in unhooked samplers) 
					// from being placed into the texture file caches...
					debugfSuppressed(NAME_DevCooking, TEXT("Cooking out material expression %s (%s)"), 
						*(MatExp->GetName()), MatExp->GetOuter() ? *(MatExp->GetOuter()->GetName()) : TEXT("????"));
					MatExp->SetFlags(RF_NotForClient|RF_NotForServer|RF_NotForEdit);
					MatExp->ClearFlags(RF_ForceTagExp);
					MatExp->ClearFlags(RF_TagExp);
					MatExp->SetFlags(RF_MarkedByCooker|RF_Transient|RF_MarkedByCookerTemp);
				}
				return;
			}
		}
	}
	
	// Cook out extra detail static meshes on consoles.
	// Also, on the PS3, reorder the lightmap's samples to correspond to the rearranged vertex ordering.
	if( GCookingTarget & PLATFORM_Console)
	{
		SCOPE_SECONDS_COUNTER_COOK(PrepareForSavingStaticMeshTime);
		// NULL out static mesh reference for components below platform detail mode threshold.
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Object);
		if( StaticMeshComponent && StaticMeshComponent->DetailMode > PlatformDetailMode )
		{
			StaticMeshComponent->StaticMesh = NULL;
		}


		if (Platform == PLATFORM_PS3 && StaticMeshComponent && StaticMeshComponent->StaticMesh)
		{
			UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh;

			// A static mesh's vertex buffer may have been reordered during cooking (as part of the
			// preparation for EDGE runtime triangle culling).  If so, the remapping will be stored in
			// the StaticMeshVertexRemappings table.  We need to update any associated LightMap1D objects
			// and per-instance vertex colors (mesh painting) to use the new vertex ordering.
			for(INT LODIndex=0; LODIndex<StaticMeshComponent->LODData.Num(); ++LODIndex)
			{
				FStaticMeshComponentLODInfo& LODInfo = StaticMeshComponent->LODData(LODIndex);
				FString RemapKey = FString::Printf(TEXT("%s::%d"), *StaticMesh->GetPathName(), LODIndex);
				TArray<INT>* VertexRemapTable = StaticMeshVertexRemappings.Find(RemapKey);

				if (!VertexRemapTable)
				{
					// the static mesh wasnt cooked for some reason, try cooking it now to get the remap table
					CookStaticMesh(StaticMesh);
					VertexRemapTable = StaticMeshVertexRemappings.Find(RemapKey);
					if (!VertexRemapTable)
					{
						warnf(TEXT("Cooking static mesh component '%s', but the static mesh '%s' is failing to be cooked"), *StaticMeshComponent->GetPathName(), *StaticMesh->GetPathName());
						continue;
					}
				}

				FLightMap1D* LightMap1D = LODInfo.LightMap ? const_cast<FLightMap1D*>(LODInfo.LightMap->GetLightMap1D()) : NULL;
				// reorder the lightmap
				if (LightMap1D && LODIndex < StaticMesh->LODModels.Num())
				{
					LODInfo.LightMap = LightMap1D->DuplicateWithRemappedVerts(*VertexRemapTable);
				}

				// reorder any 1D shadowmaps
				for (INT ShadowMapIndex = 0; ShadowMapIndex < LODInfo.ShadowVertexBuffers.Num(); ShadowMapIndex++)
				{
					LODInfo.ShadowVertexBuffers(ShadowMapIndex)->ReorderSamples(*VertexRemapTable);
				}


				// now remap the per-instance colors
				if (LODInfo.OverrideVertexColors)
				{
					UINT OverrideNum = LODInfo.OverrideVertexColors->GetNumVertices(); 
					if (OverrideNum != VertexRemapTable->Num())
					{
						warnf(NAME_Warning, TEXT("Override vertex colors has the wrong number of vertices [%d vs %d], not reordering for Edge [%s] StaticMesh %s"), 
							OverrideNum,  VertexRemapTable->Num(), *StaticMeshComponent->GetPathName(), *StaticMesh->GetPathName());
						continue;
					}

					// we need a temporary array to do the reordering (not in place reordering)
					TArray<FColor> NewColors;
					NewColors.Add(VertexRemapTable->Num());
					for (INT ColorIndex = 0; ColorIndex < NewColors.Num(); ColorIndex++)
					{
						INT RemappedIndex = (*VertexRemapTable)(ColorIndex);
						NewColors(ColorIndex) = LODInfo.OverrideVertexColors->VertexColor(RemappedIndex);
					}

					// apply the new ordering
					LODInfo.OverrideVertexColors->InitFromColorArray(NewColors);
				}
			}
		}
	}
}

/**
 * Called by PrepareForSaving to process a texture specifically
 *
 * @param	Package						Package going to be saved
 * @param	Texture2D					Texture to prepare
 * @param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
 * @param	bIsTextureOnlyFile			Whether file is only going to contain texture mips
 * @param	bIsStreamingTexture			Whether the texture is streaming
 * @param	bAllowStoreInSeparateFile	Whether or not the texture can be stored in a separate file
 * @param	bHasMatchingGuidTFC			Whether or not the texture's GUID matches the one in the TFC
 * @param	TextureFormat				Which texture format to prepare the texture for
 */
void UCookPackagesCommandlet::PrepareTextureForSaving(UPackage* Package, UTexture2D* Texture2D, FCookedTextureFileCacheInfo* TextureFileCacheInfo, UBOOL bIsSavedInSeekFreePackage, UBOOL bIsTextureOnlyFile, UBOOL bIsDLC, UBOOL bIsStreamingTexture, const UBOOL bAllowStoreInSeparateFile, UBOOL bHasMatchingGuidTFC, ETextureFormatSupport TextureFormat)
{
	// Used to access mips that are actually being processed for this TFC
	TIndirectArray<FTexture2DMipMap>* ActiveMips;

	if (TextureFormat & TEXSUPPORT_DXT)
	{
		ActiveMips = &Texture2D->Mips;
	}
	else if (TextureFormat & TEXSUPPORT_PVRTC)
	{
		ActiveMips = &Texture2D->CachedPVRTCMips;
	}
	else if (TextureFormat & TEXSUPPORT_ATITC)
	{
		ActiveMips = &Texture2D->CachedATITCMips;
	}
	else if (TextureFormat & TEXSUPPORT_ETC)
	{
		ActiveMips = &Texture2D->CachedETCMips;
	}
	else // default to main mips array
	{
		ActiveMips = &Texture2D->Mips;
	}

	// Number of mips in texture.
	INT		NumMips				= ActiveMips->Num();								
	// Index of first mip, taking into account LOD bias and settings.
	INT		FirstMipIndex		= Clamp<INT>( PlatformLODSettings.CalculateLODBias( Texture2D ) - Texture2D->NumCinematicMipLevels, 0, NumMips-1 );

	// make sure we load at least the first packed mip level
	FirstMipIndex = Min(FirstMipIndex, Texture2D->MipTailBaseIdx);	

	// if texture's source package has a newer timestamp than the entry already saved in the TFC
	UBOOL bSrcIsNewerTimeStampTFC = TRUE;
	// if texture is of a different texture format than that which is alreday saved in the TFC
	UBOOL bHasMatchingTextureFormatTFC = FALSE;

	if (!bHasMatchingGuidTFC)
	{
		// Check for the texture already residing in the TFC
		TextureFileCacheInfo = PersistentCookerData->GetTextureFileCacheEntryInfo(Texture2D);
		if (TextureFileCacheInfo)
		{
			// compare Guids
			bHasMatchingGuidTFC = (TextureFileCacheInfo->TextureFileCacheGuid == Texture2D->TextureFileCacheGuid);

			// texture Guid trumps file stamp comparison so no need to check it
			if (!bHasMatchingGuidTFC)
			{
				bSrcIsNewerTimeStampTFC = IsSourcePackageFileNewer(Texture2D, TextureFileCacheInfo->LastSaved);
			}

			// Ensure the texture is in the same cache it was...
			if (TextureFileCacheInfo->TextureFileCacheName != Texture2D->TextureFileCacheName)
			{
				bHasMatchingGuidTFC = FALSE;
			}
		}
	}

	// Texture usage stats tracked by persistent cooker data.
	INT StoredOnceMipSize	= 0;
	INT DuplicatedMipSize	= 0;
	INT SizeX				= 0;
	INT SizeY				= 0;

	// Keep track of the largest mip-level stored within the package.
	Texture2D->FirstResourceMemMip = 0;

	for( INT MipIndex=NumMips-1; MipIndex>=0; MipIndex-- )
	{	
		FTexture2DMipMap& Mip = (*ActiveMips)(MipIndex);

		UBOOL bIsStoredInSeparateFile	= FALSE;
		UBOOL bUnusedMipLevel			= FALSE;

		// We still require MinStreamedInMips to be present in the seek free package as the streaming code 
		// assumes those to be always loaded.
		if( bAllowStoreInSeparateFile
			// @todo streaming, @todo cooking: This assumes the same value of GMinTextureResidentMipCount across platforms.
			&&	(MipIndex < NumMips - GMinTextureResidentMipCount)
			// Miptail must always be loaded.
			&&	MipIndex < Texture2D->MipTailBaseIdx )
		{
			bIsStoredInSeparateFile = TRUE;
		}

		// Cut off miplevels for streaming textures that are always loaded and therefore don't need to be streamed if
		// we are using the texture file cache and saving a texture only file. We don't cut them off otherwise as the 
		// packages are openable in the Editor for e.g. the PC in their cooked state and therefore require the lower
		// miplevels.
		//
		//	Texture file cache is enabled.
		if( bUseTextureFileCache 
			//	Texture-only file, all other objects  will be stripped.
			&&	bIsTextureOnlyFile
			//	Non- streaming textures don't need to be stored in file cache, neither do miplevels that are duplicated
			//	into the seekfree packages or cubemap faces.
			&&	(!bIsStreamingTexture || (MipIndex >= NumMips - GMinTextureResidentMipCount)) )
		{
			bUnusedMipLevel = TRUE;
		}

		// Cut off miplevels that are never going to be used. This requires a FULL recook every time the texture
		// resolution is increased.
		if( MipIndex < FirstMipIndex )
		{
			bUnusedMipLevel	= TRUE;
		}

		// Cut off miplevels that are already packed in the textures miptail level 
		// since they're all stored in the base miptail level
		if( MipIndex > Texture2D->MipTailBaseIdx )
		{
			bUnusedMipLevel	= TRUE;
		}

		// Flash textures are combined into the first mip, so all other levels are not used
		if (Platform == PLATFORM_Flash && MipIndex != FirstMipIndex)
		{
			bUnusedMipLevel = TRUE;
		}

		// skip mip levels that are not being used	
		if( bUnusedMipLevel )
		{
			// bulk data is not saved 
			Mip.Data.StoreInSeparateFile( 
				TRUE,
				BULKDATA_Unused,
				0,
				INDEX_NONE,
				INDEX_NONE );
		}
		// Handle miplevels that are being streamed and therefore stored in separate file.
		else if( bIsStoredInSeparateFile )
		{
			// Retrieve offset and size of payload in separate file.
			FString					BulkDataName = *FString::Printf(TEXT("MipLevel_%i"),MipIndex);
			FCookedBulkDataInfo*	BulkDataInfo;

			// Must handle multiple texture formats here to prevent data collision
			if (Platform != PLATFORM_Android)
			{
				BulkDataInfo = PersistentCookerData->GetBulkDataInfo( Texture2D, *BulkDataName );
			}
			else
			{
				BulkDataInfo = PersistentCookerData->GetBulkDataInfo( Texture2D, *(BulkDataName + GetAndroidTextureFormatName(TextureFormat)) );
			}

			if ((BulkDataInfo == NULL) && (bIsDLC == TRUE))
			{
				if (Texture2D->TextureFileCacheName == BaseTFCName ||
					Texture2D->TextureFileCacheName == CharacterTFCName ||
					Texture2D->TextureFileCacheName == LightingTFCName)
				{
					warnf(NAME_Warning, TEXT("Texture issue: Should be in shipped cache: %s %s tfc=%s"), 
						*BulkDataName, *Texture2D->GetPathName(), *Texture2D->TextureFileCacheName.ToString());
				}
			}

			if( bUseTextureFileCache )
			{
				// If we have an existing entry and the texture GUIDs match then no TFC update is needed for the mip
				if( BulkDataInfo && 
					(bHasMatchingGuidTFC || !bSrcIsNewerTimeStampTFC))
				{
					// use previous bulk data info for setting info
					Mip.Data.StoreInSeparateFile( 
						TRUE,
						BulkDataInfo->SavedBulkDataFlags,
						BulkDataInfo->SavedElementCount,
						BulkDataInfo->SavedBulkDataOffsetInFile,
						BulkDataInfo->SavedBulkDataSizeOnDisk );

					CookStats.TFCTextureAlreadySaved += (QWORD)BulkDataInfo->SavedBulkDataSizeOnDisk;
				}
				else
				{
					// Update the mip entry in the TFC file
					// This will also update the mip bulk data info
					SaveMipToTextureFileCache(Package, Texture2D, MipIndex, TextureFormat);
				}
			}
			// not using TFC so assumption is that texture has already been saved in
			// another SF package and just need to update the bulk data info for the mip in this package
			else 
			{
				if( BulkDataInfo )
				{
					// use previous bulk data info for setting info
					Mip.Data.StoreInSeparateFile( 
						TRUE,
						BulkDataInfo->SavedBulkDataFlags,
						BulkDataInfo->SavedElementCount,
						BulkDataInfo->SavedBulkDataOffsetInFile,
						BulkDataInfo->SavedBulkDataSizeOnDisk );
				}
				else
				{
					appErrorf( TEXT("Couldn't find seek free bulk data info: %s for %s\nRecook with -full"), *BulkDataName, *Texture2D->GetFullName() );
				}
			}

			StoredOnceMipSize += Mip.Data.GetBulkDataSize();
		}
		// We're not cutting off miplevels as we're either a regular package or the texture is not streamable.
		else
		{
			Mip.Data.StoreInSeparateFile( FALSE );
			DuplicatedMipSize += Mip.Data.GetBulkDataSize();

			// Keep track of the largest mip-level stored within the package.
			Texture2D->FirstResourceMemMip = MipIndex;
		}

		// Track max size saved out.
		if( !bUnusedMipLevel )
		{
			SizeX = Max( SizeX, Mip.SizeX );
			SizeY = Max( SizeY, Mip.SizeY );
		}

		// Miplevels that are saved into the map/ seekfree package shouldn't be individually compressed as we're not going
		// to stream out of them and it just makes serialization slower. In the long run whole package compression is going
		// to take care of getting the file size down.
		if (bIsSavedInSeekFreePackage && !bIsStoredInSeparateFile)
		{
			Mip.Data.StoreCompressedOnDisk( COMPRESS_None );
		}
		// Store mips compressed in regular packages so we can stream compressed from disk.
		else
		{
			Mip.Data.StoreCompressedOnDisk( GBaseCompressionMethod );
		}
	}

	// Keep track of texture usage in persistent cooker info.
	PersistentCookerData->AddTextureUsageInfo( Package, Texture2D, StoredOnceMipSize, DuplicatedMipSize, SizeX, SizeY );
}

/**
 * Setup the commandlet's platform setting based on commandlet params
 * @param Params The commandline parameters to the commandlet - should include "platform=xxx"
 */
UBOOL UCookPackagesCommandlet::SetPlatform(const FString& Params)
{
	// first, get the platform enum
	Platform = ParsePlatformType(*Params);

	// return false on failure
	if (Platform == PLATFORM_Unknown)
	{
		return FALSE;
	}

	// now get the shader platform from the platform 
	switch (Platform)
	{
		case PLATFORM_PS3:
			ShaderPlatform = SP_PS3;
			break;
		case PLATFORM_Xbox360:
			ShaderPlatform = SP_XBOXD3D;
			break;
		case PLATFORM_MacOSX:
			ShaderPlatform = SP_PCOGL;
			break;
		case PLATFORM_WiiU:
			ShaderPlatform = SP_WIIU;
			break;
		default:
			ShaderPlatform = SP_PCD3D_SM3;
			break;
	}

	// Android _must_ specify a texture format when cooking
	if (Platform == PLATFORM_Android)
	{
		FString TextureFormat;
		if (!Parse(*Params, TEXT("-TexFormat="), TextureFormat))
		{
			warnf(TEXT("When cooking for Android, you MUST specify a texture format with -TexFormat=[dxt | pvrtc | atitc | etc]"));
			return FALSE;
		}

		AndroidTextureFormat = 0x0;

		TArray<FString> ParsedFormatNames;
		TextureFormat.ParseIntoArray(&ParsedFormatNames, TEXT("|"), TRUE);

		for (INT FormatIndex = 0; FormatIndex < ParsedFormatNames.Num(); ++ FormatIndex)
		{
			// now set the bitmask
			if (ParsedFormatNames(FormatIndex) == TEXT("dxt"))
			{
				AndroidTextureFormat |= TEXSUPPORT_DXT;
			}
			else if (ParsedFormatNames(FormatIndex) == TEXT("pvrtc"))
			{
				AndroidTextureFormat |= TEXSUPPORT_PVRTC;
			}
			else if (ParsedFormatNames(FormatIndex) == TEXT("atitc"))
			{
				AndroidTextureFormat |= TEXSUPPORT_ATITC;
			}
			else if (ParsedFormatNames(FormatIndex) == TEXT("etc"))
			{
				AndroidTextureFormat |= TEXSUPPORT_ETC;
			}
			else
			{
				warnf(TEXT("Unrecognized texture format specified with -TexFormat. Must be one of [dxt | pvrtc | atitc | etc]"));
				return FALSE;
			}
		}
	}

	// return success
	return TRUE;
}

/**
 * Tried to load the DLLs and bind entry points.
 *
 * @return	TRUE if successful, FALSE otherwise
 */
UBOOL UCookPackagesCommandlet::BindDLLs()
{
	if( Platform & PLATFORM_Console )
	{
		// get the platform name (this works for consoles, but not PC!)
		FString PlatformNameAndBinariesSubdir = appPlatformTypeToString(Platform);

		// Load in the console support containers
		FConsoleSupportContainer::GetConsoleSupportContainer()->LoadAllConsoleSupportModules(*PlatformNameAndBinariesSubdir);

		// Find the target platform PC-side support implementation.
		ConsoleSupport = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(*PlatformNameAndBinariesSubdir);
		if(!ConsoleSupport)
		{
			appDebugMessagef(TEXT("Couldn't bind to support DLL for %s."), *PlatformNameAndBinariesSubdir);
			return FALSE;
		}

		// Create the platform specific texture and sound cookers and shader compiler.
		TextureCooker		= ConsoleSupport->GetGlobalTextureCooker();
		SoundCooker			= ConsoleSupport->GetGlobalSoundCooker();

		SkeletalMeshCooker	= ConsoleSupport->GetGlobalSkeletalMeshCooker();
		StaticMeshCooker	= ConsoleSupport->GetGlobalStaticMeshCooker();

		check(!GConsoleShaderPrecompilers[ShaderPlatform]);
		GConsoleShaderPrecompilers[ShaderPlatform] = ConsoleSupport->GetGlobalShaderPrecompiler();
		
		if ( Platform == PLATFORM_NGP )
		{
			GConsoleShaderPrecompilers[SP_NGP] = GConsoleShaderPrecompilers[ShaderPlatform];
		}

		// Clean up and abort on failure.
		if( !TextureCooker 
		||	!SoundCooker 
		||	!SkeletalMeshCooker
		||	!StaticMeshCooker
		||	!GConsoleShaderPrecompilers[ShaderPlatform] )
		{
			appDebugMessagef(TEXT("Couldn't create platform cooker singletons."));
			return FALSE;
		}
	}
	GetLocalShaderCache( ShaderPlatform );
	return TRUE;
}

/**
* Update all game .ini files from defaults
*
* @param IniPrefix	prefix for ini filename in case we want to write to cooked path
*/
void UCookPackagesCommandlet::UpdateGameIniFilesFromDefaults(const TCHAR* IniPrefix)
{
	// get all files in the config dir
	TArray<FString> IniFiles;
	appFindFilesInDirectory(IniFiles, *appGameConfigDir(), FALSE, TRUE);

	UINT YesNoToAll = ART_No;
	// go over ini files and generate non-default ini files from default inis
	for (INT FileIndex = 0; FileIndex < IniFiles.Num(); FileIndex++)
	{
		// the ini path/file name
		FFilename IniPath = IniFiles(FileIndex);
		FString IniFilename = IniPath.GetCleanFilename();

		// we want to process all the Default*.ini files
		if (IniFilename.Left(7) == PC_DEFAULT_INI_PREFIX )
		{
			// generate ini files
			TCHAR GeneratedIniName[MAX_SPRINTF] = TEXT("");
			TCHAR GeneratedDefaultIniName[MAX_SPRINTF] = TEXT("");

			appCreateIniNames(
				GeneratedIniName, 
				GeneratedDefaultIniName,
				NULL, NULL,
				*IniFilename.Right(IniFilename.Len() - 7),
				PC_DEFAULT_INI_PREFIX,
				IniPrefix);

			const UBOOL bTryToPreserveContents = FALSE;
			appCheckIniForOutdatedness(
				GeneratedIniName, 
				GeneratedDefaultIniName,
				bTryToPreserveContents,
				YesNoToAll);
		}
	}
}

/**
 * Precreate all the .ini files that the platform will use at runtime
 * @param bAddForHashing - TRUE if running with -sha and ini files should be added to list of hashed files
 */
void UCookPackagesCommandlet::CreateIniFiles(UBOOL bAddForHashing)
{
	SCOPE_SECONDS_COUNTER_COOK(CreateIniFilesTime);

	// Get a list of known language extensions
	const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();

	if (Platform & (PLATFORM_Console | PLATFORM_WindowsServer | PLATFORM_MacOSX))
	{
		// Update the cooked inis first, getting the proper platform engine ini in the process
		UpdateCookedPlatformIniFilesFromDefaults(Platform, PlatformEngineConfigFilename, PlatformSystemSettingsConfigFilename);

		// update all game inis
		UpdateGameIniFilesFromDefaults( *GetConfigOutputPrefix(Platform) );

		if (!bIsMTChild)  // child processes don't need to do this and the files would clash
		{
			if (Platform & PLATFORM_Console)
			{
				// figure out where to put coalsced ini/loc files
				// coalesce the ini and int files
				GConfig->CoalesceFilesFromDisk(*(appGameConfigDir() + GetConfigOutputDirectory(Platform)), *GetTrueCookedDirectory(), 
					ShouldByteSwapData(), PlatformEngineConfigFilename, *appPlatformTypeToString(Platform), 
					bCookCurrentLanguageOnly, MultilanguageCookingMask);
	
				if( bAddForHashing )
				{
					if (bCookCurrentLanguageOnly)
					{
						const TCHAR* Ext = UObject::GetLanguage();
						// mark each coalesced loc file for sha
						FilesForSHA.AddItem(*FString::Printf(TEXT("%sCoalesced_%s.bin"), *GetCookedDirectory(), Ext));
					}
					else
					{
						for (INT LangIndex = 0; LangIndex < KnownLanguageExtensions.Num(); LangIndex++)
						{
							const FString& Ext = KnownLanguageExtensions(LangIndex);
							// mark each coalesced loc file for sha
							FilesForSHA.AddItem(*FString::Printf(TEXT("%sCoalesced_%s.bin"), *GetCookedDirectory(), *Ext));
						}
					}
				}
			}
		}
	}
	else
	{
		// Use the current engine ini file on the PC.
		appStrcpy( PlatformEngineConfigFilename, GEngineIni );
		appStrcpy( PlatformSystemSettingsConfigFilename, GSystemSettingsIni );
		
		if( bAddForHashing && !bIsMTChild )  // child processes don't need to do this and the files would clash
		{
			// update all game inis in default PC directory
			UpdateGameIniFilesFromDefaults(PC_INI_PREFIX);

			// get list of files we don't want to sign
			FConfigSection* NonSignedConfigFiles = GConfig->GetSectionPrivate(TEXT("NonSignedConfigFilter"), FALSE, TRUE, GEngineIni);

			// get all generated ini files in the game config dir
			TArray<FString> GameIniFiles;
			appFindFilesInDirectory(GameIniFiles, *appGameConfigDir(), FALSE, TRUE);
			for( INT FileIdx=0; FileIdx < GameIniFiles.Num(); FileIdx++ )
			{
				INT GameNameLen = appStrlen(GGameName);
				check(GameNameLen>0);

				// the ini path/file name
				FFilename IniPath = GameIniFiles(FileIdx);
				FString IniFilename = IniPath.GetCleanFilename();

				UBOOL bShouldHash =TRUE;
				// we want to process all the <GameName>*.ini files
				if( IniFilename.Left(GameNameLen) != GGameName )
				{
					bShouldHash = FALSE;
				}
				// skip over *.xml files
				else if( FFilename(IniFilename).GetExtension().ToLower() == FString(TEXT("xml")) )
				{
					bShouldHash = FALSE;
				}
				// skip any that are marked to not be signed
				else if( NonSignedConfigFiles )
				{
					for( FConfigSectionMap::TIterator It(*NonSignedConfigFiles); It; ++It )
					{
						if( FFilename(It.Value()).GetCleanFilename() == IniFilename )
						{
							bShouldHash = FALSE;
							break;
						}
					}
				}

				// add to list of files for hashing
				if( bShouldHash )
				{
					FilesForSHA.AddItem(IniPath);
				}
			}
		}
	}
}

/**
 * Copies all files in a folder to another folder, based on wildcard matching.
 *
 * @param SourceFolder			Source folder
 * @param DestinationFolder		Destination folder
 * @param Wildcard				Wildcard file pattern to match
 * @return TRUE if successful
 */
UBOOL CopyFiles( const FFilename& SourceFolder, const FFilename& DestinationFolder, const FString& Wildcard )
{
	TArray<FString> Results;
	FFilename WildcardPattern = SourceFolder * Wildcard;
	GFileManager->FindFiles(Results, *WildcardPattern, TRUE, FALSE);
	if (SourceFolder != DestinationFolder)
	{
		// Make sure the target directory exists.
		if( !GFileManager->MakeDirectory( *DestinationFolder, TRUE ) )
		{
			return FALSE;
		}

		// copy each shader over
		for (INT ShaderIndex = 0; ShaderIndex < Results.Num(); ShaderIndex++)
		{
			FString DestFilename = DestinationFolder * FFilename(Results(ShaderIndex)).GetCleanFilename();
			FString SrcFilename = SourceFolder * FFilename(Results(ShaderIndex)).GetCleanFilename();
			if ( GFileManager->Copy(*DestFilename, *SrcFilename, TRUE, TRUE) != COPY_OK )
			{
				warnf(NAME_Log,TEXT("Could not copy '%s' -> '%s'"),*SrcFilename,*DestFilename);
				return FALSE;
			}
		}
	}
	return TRUE;
}

/** 
 * Prepares shader files for the given platform to make sure they can be used for compiling
 */
UBOOL UCookPackagesCommandlet::PrepareShaderFiles()
{
	if (bIsMTChild)
	{
		// copy all shaders to our own directory since we might needed them during cooking
		FFilename ShaderDir = FString(appBaseDir()) * appShaderDir();
		check(!DLCName.Len());
		extern void appSetShaderDir(const TCHAR* Where);
		appSetShaderDir(*(CookedDirForPerProcessData * TEXT("Shaders")));
		FFilename UserShaderDir = FString(appBaseDir()) * appShaderDir();

		// Copy Engine\Shaders
		CopyFiles( ShaderDir, UserShaderDir, FString(TEXT("*.*")) );
		// Copy Engine\Shaders\Binaries
		CopyFiles( ShaderDir * TEXT("Binaries"), UserShaderDir * TEXT("Binaries"), FString(TEXT("*.*")) );
		// Copy Engine\Shaders\Xbox360 if cooking for Xbox360.
		if( Platform == PLATFORM_Xbox360 )
		{
			CopyFiles( ShaderDir * TEXT("Xbox360"), UserShaderDir * TEXT("Xbox360"), FString(TEXT("*.*")) );
		}
	}
	// for cooking as an enduser for PS3, the shader files must exist in the My Games directory
	// because we need to be able to write the Material.usf/VertexShader.usf to the same directory
	// that contains the shader files
	else if ((DLCName.Len() && Platform == PLATFORM_PS3))
	{
		// get all the shaders in the shaders directory
		TArray<FString> Results;
		FString ShaderDir = FString(appBaseDir()) * appShaderDir();
		FString UserShaderDir = GFileManager->ConvertAbsolutePathToUserPath(*GFileManager->ConvertToAbsolutePath(*ShaderDir));
		appFindFilesInDirectory(Results, *ShaderDir, FALSE, TRUE);

		if (GFileManager->ConvertToAbsolutePath(*ShaderDir) != UserShaderDir)
		{
			// Make sure the target directory exists.
			if( !GFileManager->MakeDirectory( *UserShaderDir, TRUE ) )
			{
				return FALSE;
			}

			// copy each shader over
			for (INT ShaderIndex = 0; ShaderIndex < Results.Num(); ShaderIndex++)
			{
				FString DestFilename = GFileManager->ConvertAbsolutePathToUserPath(*GFileManager->ConvertToAbsolutePath(*Results(ShaderIndex)));
				if ( GFileManager->Copy(*DestFilename, *Results(ShaderIndex), TRUE, TRUE) != COPY_OK )
				{
					return FALSE;
				}
			}
		}
	}

	return TRUE;
}

/** 
 * Cleans up shader files for the given platform 
 */
void UCookPackagesCommandlet::CleanupShaderFiles()
{
	// delete any files we copied over in PrepareShaderFiles
	if (DLCName.Len() && Platform == PLATFORM_PS3)
	{
		// this will clean up the shader files
		appCleanFileCache();
	}
}

/**
 * Warns the user if the map they are cooking has references to editor content (EditorMeshes, etc)
 *
 * @param Package Package that has been loaded by the cooker
 */
void UCookPackagesCommandlet::WarnAboutEditorContentReferences(UPackage* Package)
{
	if ( (GCookingTarget & UE3::PLATFORM_Stripped) != 0 && Package->ContainsMap() )
	{
		if( EditorOnlyContentPackageNames.Num() )
		{
			// get linker load for the package
			UObject::BeginLoad();
			ULinkerLoad* Linker = UObject::GetPackageLinker(Package, NULL, LOAD_None, NULL, NULL);
			UObject::EndLoad();
			
			// look for editor references
			for (INT ImportIndex = 0; ImportIndex < Linker->ImportMap.Num(); ImportIndex++)
			{
				// don't bother outputting package references, just the objects
				if (Linker->ImportMap(ImportIndex).ClassName != NAME_Package)
				{
					// get package name of the import
					FString ImportPackage = FFilename(Linker->GetImportPathName(ImportIndex)).GetBaseFilename();
					// Warn if part of content package list.
					if( EditorOnlyContentPackageNames.FindItemIndex(ImportPackage) != INDEX_NONE )
					{
						warnf(NAME_Warning, TEXT("Map (%s) references editor only content: '%s'! This will not load in-game!!"), *Package->GetName(), *Linker->GetImportFullName(ImportIndex));
					}
				}
			}
		}
	}
}

/** Clear Current Persistent Map Cache */
void FPersistentFaceFXAnimSetGenerator::ClearCurrentPersistentMapCache()
{
	CurrentPersistentMapName.Empty();
	AliasedPersistentMapName.Empty();
	CurrentPersistentMapFaceFXArray = NULL;
	CurrentPMapGroupAnimLookupMap = NULL;
}
/**
 * Loads a package that will be used for cooking. This will cache the source file time
 * and add the package to the Guid Cache
 *
 * @param Filename Name of package to load
 *
 * @return Package that was loaded and cached
 */
UPackage* UCookPackagesCommandlet::LoadPackageForCooking(const TCHAR* Filename)
{
	UPackage* Package = NULL;

	UFaceFXAnimSet* PersistentFaceFXAnimSet = PersistentFaceFXAnimSetGenerator.GeneratePersistentMapFaceFXAnimSet(Filename);

	if (GGameCookerHelper)
	{
		// Allow the cooker helper to preload any required items for the package about to be loaded
		if (GGameCookerHelper->PreLoadPackageForCookingCallback(this, Filename) == TRUE)
		{
			Package = GGameCookerHelper->LoadPackageForCookingCallback(this, Filename);
		}
		else
		{
			warnf(NAME_Warning, TEXT("Aborting cook: PreLoadPackageForCookingCallback failed for %s"), Filename);
			return NULL;
		}
	}

	if (Package == NULL)
	{
		Package = UObject::LoadPackage(NULL, Filename, LOAD_None);
	}

	// if the package loaded, then add it to the package cache, and cache the filetime (unless it's the guid cache or persistent data)
	if (Package)
	{
		if (GGameCookerHelper)
		{
			if (GGameCookerHelper->PostLoadPackageForCookingCallback(this, Package) == FALSE)
			{
				Package = NULL;
				// Collect garbage and verify it worked as expected.	
				CollectGarbageAndVerify();
				return NULL;
			}
		}

		if ( (Package->PackageFlags&(PKG_ContainsDebugInfo|PKG_ContainsScript)) == (PKG_ContainsDebugInfo|PKG_ContainsScript) )
		{
			warnf(NAME_Error, TEXT("The script contained in package '%s' was compiled in debug mode.  Please recompile all script package in release before cooking."), Filename);
			return NULL;
		}

		if( bUseTextureFileCache )
		{
			// cache the guid for all currently loaded (outermost) packages since we don't iterate over all packages when using the TFC
			for( TObjectIterator<UPackage> It; It; ++It )
			{
				UPackage* PackageObj = It->GetOutermost();
				if (!bIsMTChild || !PackageObj->GetFName().ToString().StartsWith(TEXT("Startup"))) // special case, we need to not add this unless we are actually cooking it
				{
					GuidCache->SetPackageGuid(PackageObj->GetFName(), PackageObj->GetGuid());
				}
			}
		}
		else
		{
			// cache guid for current cooked package
			GuidCache->SetPackageGuid(Package->GetFName(), Package->GetGuid());
		}
		
		if (!bIsMTChild)
		{
			// save it out
			GuidCache->SaveToDisk(ShouldByteSwapData());
		}

		// warn about any references to editor-only content
		if( ParseParam(appCmdLine(),TEXT("WarnAboutEditorContentReferences")) )
		{
			WarnAboutEditorContentReferences(Package);
		}

		if ( PersistentFaceFXAnimSet )
		{
			PersistentFaceFXAnimSet->Rename(*(PersistentFaceFXAnimSet->GetName()), Package);
			UWorld* World = FindObject<UWorld>( Package, TEXT("TheWorld") );
			check(World);
			PersistentFaceFXAnimSet->RemoveFromRoot();
			World->PersistentFaceFXAnimSet = PersistentFaceFXAnimSet;

			// this is map package
			LogFaceFXAnimSet( PersistentFaceFXAnimSet, Package );
		}

		CleanupKismet(Package);
	}
#if 0
	if (PersistentFaceFXAnimSetGenerator.GetGeneratePersistentMapAnimSet() == FALSE)
	{
		for (TObjectIterator<UFaceFXAnimSet> FFXASIt; FFXASIt; ++FFXASIt)
		{
			UFaceFXAnimSet* AnimSet = *FFXASIt;
			AnimSet->ReferencedSoundCues.Empty();
		}
	}
#endif
	return Package;
}

/**
 * Force load a package and emit some useful info
 * 
 * @param PackageName Name of package, could be different from filename due to localization
 * @param bRequireServerSideOnly If TRUE, the loaded packages are required to have the PKG_ServerSideOnly flag set for this function to succeed
 *
 * @return TRUE if successful
 */
UBOOL UCookPackagesCommandlet::ForceLoadPackage(const FString& PackageName, UBOOL bRequireServerSideOnly)
{
	warnf( NAME_Log, TEXT("Force loading:  %s"), *PackageName );
	UPackage* Package = LoadPackageForCooking(*PackageName);
	if (Package == NULL)
	{
		warnf(NAME_Error, TEXT("Failed to load package '%s'"), *PackageName);
		return FALSE;
	}
	else if (bRequireServerSideOnly && !(Package->PackageFlags & PKG_ServerSideOnly))
	{
		warnf(NAME_Error, TEXT("Standalone seekfree packages must have ServerSideOnly set. Use the 'SetPackageFlags' commandlet to do this."));
		return FALSE;
	}

	return TRUE;
}

/**
 * Load all packages in a specified ini section with the Package= key
 * @param SectionName Name of the .ini section ([Engine.PackagesToAlwaysCook])
 * @param PackageNames Paths of the loaded packages
 * @param KeyName Optional name for the key of the list to load (defaults to "Package")
 * @param bShouldSkipLoading If TRUE, this function will only fill out PackageNames, and not load the package
 * @param bRequireServerSideOnly If TRUE, the loaded packages are required to have the PKG_ServerSideOnly flag set for this function to succeed
 * @return if loading was required, whether we successfully loaded all the packages; otherwise, always TRUE
 */
UBOOL UCookPackagesCommandlet::LoadSectionPackages(const TCHAR* SectionName, TArray<FString>& PackageNames, const TCHAR* KeyName, UBOOL bShouldSkipLoading, UBOOL bRequireServerSideOnly)
{
	// here we need to look in the .ini to see which packages to load
	FConfigSection* PackagesToLoad = GConfig->GetSectionPrivate( SectionName, 0, 1, PlatformEngineConfigFilename );
	FString KeyName_Optional = *FString::Printf(TEXT("%s_Optional"), KeyName);
	SCOPE_SECONDS_COUNTER_COOK(LoadSectionPackagesTime);
	for( FConfigSectionMap::TIterator It(*PackagesToLoad); It; ++It )
	{
		if (It.Key() == KeyName || It.Key() == *KeyName_Optional)
		{
			FString& PackageName = It.Value();
			FString PackageFileName;
			if( GPackageFileCache->FindPackageFile( *PackageName, NULL, PackageFileName ) == TRUE )
			{
				PackageNames.AddItem( FString(*PackageName).ToUpper() );
				if (!bShouldSkipLoading)
				{
					if (!ForceLoadPackage(PackageName, bRequireServerSideOnly))
					{
						return FALSE;
					}
				}
			}
			else if (It.Key() == KeyName)
			{
				warnf(NAME_Error, TEXT("Failed to find package '%s'"), *PackageName);
				return FALSE;
			}
			else
			{
				warnf(TEXT("Optional package '%s' not found... skipping"), *PackageName);
			}
		}
	}

	return TRUE;
}


/**
 * Helper struct to sort a list of packages by size
 * Not fast!
 * @todo: Cache file sizes for faster sorting
 */
struct FPackageSizeSorter
{
	static INT Compare(const FString& A, const FString& B)
	{
		// get the paths to the files
		FString PathA;
		if (GPackageFileCache->FindPackageFile(*A, NULL, PathA) == FALSE)
		{
			return 0;
		}

		FString PathB;
		if (GPackageFileCache->FindPackageFile(*B, NULL, PathB) == FALSE)
		{
			return 0;
		}

		// get the sizes of the files
		QWORD SizeA = GFileManager->FileSize(*PathA);
		QWORD SizeB = GFileManager->FileSize(*PathB);

		// we want biggest to smallest, so return < 0 if A is bigger
		return (INT)(SizeB - SizeA);
	}
};

#if WITH_GFx
/* from GFxUILocalization.cpp */
UBOOL LocalizeArray(TArray<FString>& Result, const TCHAR* Section, const TCHAR* Key, const TCHAR* Package=GPackage, const TCHAR* LangExt = NULL);
#endif // WITH_GFx

TArray<FString> TokenMaps;
TArray<FString> TokenScriptPackages;
TArray<FString> TokenContentPackages;
/** The maps passed in on the command line */
TArray<FString> CommandLineMaps;

/** 
 * Indirects the map list through a text file - 1 map per line of file
 * 
 * @param Params		The command line passed into the commandlet
 *
 * @return UBOOL		FALSE if processing failed
 */
UBOOL UCookPackagesCommandlet::ParseMapList( const TCHAR* Params )
{
	FString ParameterString;
	if (Parse(Params, TEXT("PKGLIST="), ParameterString))
	{
		// Remove the option so it is not treated as a map
		FString Text = FString::Printf(TEXT("PKGLIST=%s"), *ParameterString);
		Tokens.RemoveItem(Text);

		// Load the package list into a string
		if (appLoadFileToString(Text, *ParameterString))
		{
			const TCHAR* Data = *Text;
			FString Line;
			while (ParseLine(&Data, Line))
			{
				Tokens.AddItem(Line);
			}
		}
		else
		{
			warnf(NAME_Error, TEXT("Failed to load package list %s"), *ParameterString);
			return FALSE;
		}
	}
	
	GEditor->ParseMapSectionIni(Params, Tokens);

	return TRUE;
}

#if WITH_GFx
static void GFx_AddExtraPackages(TArray<FString>& RequiredPackages)
{
    TArray<FString> FontLibFiles;
    LocalizeArray(FontLibFiles, TEXT("FontLib"), TEXT("FontLib"), TEXT("GFxUI"));

	TArray<FString> MoviePaths;
    LocalizeArray(MoviePaths, TEXT("IME"), TEXT("MoviePath"), TEXT("GFxUI"));
	FontLibFiles += MoviePaths;

    for (int FontIndex=0; FontIndex<FontLibFiles.Num(); FontIndex++)
    {
        FString FontLibFile(FontLibFiles(FontIndex));
        FString FontLib;
        if (FontLibFile.Split(TEXT("."),&FontLib,NULL))
        {
            FString PackageFileName;
            if( GPackageFileCache->FindPackageFile( *FontLib, NULL, PackageFileName ) == TRUE )
			{
                RequiredPackages.AddItem(FontLib.ToUpper());
			}
            else
			{
                warnf(TEXT("Fontlib/IME package %s not found"), *FontLib);
			}
        }
    }

    FConfigSection* MovieIni = GConfig->GetSectionPrivate(TEXT("FullScreenMovie"), FALSE, TRUE, GEngineIni);
    if (MovieIni)
    {
        TArray<FString> Movies;
        for (FConfigSectionMap::TIterator It(*MovieIni); It; ++It)
        {
            FString MoviePkg;
            if ((It.Key() == TEXT("LoadMapMovies") ||
                 It.Key() == TEXT("StartupMovies")) &&
                It.Value().InStr(TEXT(".bik")) < 0 &&
                It.Value().Split(TEXT("."),&MoviePkg,NULL))
            {
                FString PackageFileName;
                if( GPackageFileCache->FindPackageFile( *MoviePkg, NULL, PackageFileName ) == TRUE )
				{
                    RequiredPackages.AddItem(MoviePkg.ToUpper());
				}
                else
				{
                    warnf(TEXT("GFx movie package %s not found"), *MoviePkg);
				}
            }
        }
    }
}
#endif

/**
 * Performs command line and engine specific initialization.
 *
 * @param	Params	command line
 * @param	bQuitAfterInit [out] If TRUE, the caller will quit the commandlet, even if the Init function returns TRUE
 * @return	TRUE if successful, FALSE otherwise
 */
UBOOL UCookPackagesCommandlet::Init( const TCHAR* Params, UBOOL& bQuitAfterInit )
{
	bIsMTMaster = FALSE;
	bIsMTChild = FALSE;	
	bQuickMode = FALSE;

	if (GIsBuildMachine)
	{
		MTTimeout *= 6.0f; // for build machines err on the side of completing the build
	}
	
	// Parse command line args.
	ParseCommandLine( Params, Tokens, Switches );

	// Optionally load in the maps from a text file
	if( !ParseMapList( Params ) )
	{
		return( FALSE );
	}

	// Set the "we're cooking" flags.
	GIsCooking					= TRUE;
	GIsCookingForDemo			= Switches.FindItemIndex(TEXT("COOKFORDEMO")) != INDEX_NONE;
	GCookingTarget 				= Platform;

	// Look for -SKIPMAPS command line switch.
	bSkipCookingMaps			= Switches.FindItemIndex(TEXT("SKIPMAPS")) != INDEX_NONE;
	// Look for -INISONLY command line switch
	bIniFilesOnly				= Switches.FindItemIndex(TEXT("INISONLY")) != INDEX_NONE;
#if SHIPPING_PC_GAME
	bGenerateSHAHashes			= FALSE;
#else
	// Check if we are only doing ini files
	bGenerateSHAHashes			= Switches.FindItemIndex(TEXT("SHA")) != INDEX_NONE;
#endif //SHIPPING_PC_GAME
	// Skip saving maps if SKIPSAVINGMAPS is specified, useful for LOC cooking.
	bSkipSavingMaps				= Switches.FindItemIndex(TEXT("SKIPSAVINGMAPS")) != INDEX_NONE;
	// Skip loading & saving packages not required for cooking process to speed up LOC cooking.
	bSkipNotRequiredPackages	= Switches.FindItemIndex(TEXT("SKIPNOTREQUIREDPACKAGES")) != INDEX_NONE;
	// Check for flag to recook seekfree
	bForceRecookSeekfree		= Switches.FindItemIndex(TEXT("RECOOKSEEKFREE")) != INDEX_NONE;
	// should non map packages be cooked?
	bCookMapsOnly				= Switches.FindItemIndex(TEXT("MAPSONLY")) != INDEX_NONE;
	// should the shader cache be saved at end instead of after each package?
	bSaveShaderCacheAtEnd		= Switches.FindItemIndex(TEXT("SAVESHADERSATEND")) != INDEX_NONE;
	// Disallow map and package compression if option is set.
	bDisallowPackageCompression	= Switches.FindItemIndex(TEXT("NOPACKAGECOMPRESSION")) != INDEX_NONE;
	bCookCurrentLanguageOnly	= (Switches.FindItemIndex(TEXT("NOLOCCOOKING")) != INDEX_NONE) || (Switches.FindItemIndex(TEXT("FASTCOOK")) != INDEX_NONE);
	if (bCookCurrentLanguageOnly && (Switches.FindItemIndex(TEXT("LOCCOOKING")) != INDEX_NONE))
	{
		warnf(NAME_Log, TEXT("Override is enabling LOCCOOKING!"));
		bCookCurrentLanguageOnly = FALSE;
	}
	bVerifyTextureFileCache		= Switches.FindItemIndex(TEXT("VERIFYTFC")) != INDEX_NONE;
	bWriteTFCReport = Switches.FindItemIndex(TEXT("WRITETFCREPORT")) != INDEX_NONE;
	bAnalyzeReferencedContent	= Switches.FindItemIndex(TEXT("ANALYZEREFERENCEDCONTENT")) != INDEX_NONE;
	bSkipPSysDuplicateModules	= (Switches.FindItemIndex(TEXT("SKIPPSYSMODULES")) != INDEX_NONE) || (Switches.FindItemIndex(TEXT("FASTCOOK")) != INDEX_NONE);
	bSkipStartupObjectDetermination = (Switches.FindItemIndex(TEXT("SKIPSTARTUPOBJS")) != INDEX_NONE) || (Switches.FindItemIndex(TEXT("FASTCOOK")) != INDEX_NONE);
	if (bSkipStartupObjectDetermination == TRUE)
	{
		warnf(NAME_Log, TEXT("Cooking w/ StartupObjectDetermination DISABLED!"));
	}

	bForceFilterEditorOnly = (Switches.FindItemIndex(TEXT("FORCEFILTEREDITORONLY")) != INDEX_NONE);
	if (bForceFilterEditorOnly == TRUE)
	{
		warnf(NAME_Log, TEXT("Cooking w/ forced FilterEditorOnly!"));
	}

	// Controls whether volumes that have cached physics data should be stripped of their brush data during cooking (saves a decent amount of memory)
	bStripBrushesWithCachedPhysicsData = TRUE;
	if (Switches.FindItemIndex(TEXT("STRIPVOLUMEBRUSHES")) != INDEX_NONE)
	{
		bStripBrushesWithCachedPhysicsData = TRUE;
	}
	else if (Switches.FindItemIndex(TEXT("KEEPVOLUMEBRUSHES")) != INDEX_NONE)
	{
		bStripBrushesWithCachedPhysicsData = FALSE;
	}

	// if writing a TFC report, start a new file now and include a CSV header for the columns
	if (bWriteTFCReport)
	{
		FString DestFileName = FFilename(GetCookedDirectory()) + TEXT("DebugLogs\\") + TFCReportFileName.GetNameString();
		warnf(NAME_Log, TEXT("Writing TFC generation log file to %s"), *DestFileName);
		appSaveStringToFile(TEXT("SrcPackageName,TFC Name,Texture Name,Texture Slice,Size Written,PixelFormat, SizeX, SizeY, Current TFC Size\n"), *DestFileName);
	}

	// disable package compression - the decompression can be slower than reading on cpu-limited devices
	if (Platform == PLATFORM_IPhone || Platform == PLATFORM_Flash)
	{
		// @todo ib2merge - Chair had this commented out, and lower down would do compression on some packages (removed)
		bDisallowPackageCompression = TRUE;
	}

	if (Platform & (PLATFORM_Mobile | PLATFORM_MacOSX | PLATFORM_WiiU))
	{
		// Due to the USE_CONVEX_QUICKLOAD mismatch between cook-time and run-time, all physx data is
		// currently recooked at load time on mobile platforms, requiring the source data to not be stripped.
		bStripBrushesWithCachedPhysicsData = FALSE;
	}

	bSeparateLightingTFC = TRUE;
	GConfig->GetBool(TEXT("Cooker.GeneralOptions"), TEXT("bSeparateLightingTFC"), bSeparateLightingTFC, GEditorIni);
	warnf(NAME_Log, TEXT("Cooking with %s Lighting TextureFileCache..."), bSeparateLightingTFC ? TEXT("SEPARATE") : TEXT("COMBINED"));

	bSeparateCharacterTFC = FALSE;
	GConfig->GetBool(TEXT("Cooker.GeneralOptions"), TEXT("bSeparateCharacterTFC"), bSeparateCharacterTFC, GEditorIni);
	warnf(NAME_Log, TEXT("Cooking with %s Character TextureFileCache..."), bSeparateCharacterTFC ? TEXT("SEPARATE") : TEXT("COMBINED"));

	PersistentFaceFXAnimSetGenerator.SetLogPersistentFaceFXGeneration(Switches.FindItemIndex(TEXT("LOGPERSISTENTFACEFX")) != INDEX_NONE);
	
	// Look for a DLC name to infer user mode for the appropriate platforms
	if ((Platform & PLATFORM_DLCSupported) != 0)
	{
		Parse(Params, TEXT("DLCName="), DLCName);
	}

#if SHIPPING_PC_GAME && !UDK && !DEDICATED_SERVER
	if(!DLCName.Len())
	{
		DLCName = TEXT("UserMod");
	}
#else
	// Check for user mode
	if(!DLCName.Len() && Switches.FindItemIndex(TEXT("INSTALLED")) != INDEX_NONE)
	{
		DLCName = TEXT("UserMod");
	}
#endif

	// Check for user mode
	UBOOL bDumpBulkData = Switches.FindItemIndex(TEXT("DUMPBULK")) != INDEX_NONE;
	
	//For the editor, make sure we cook all content on PC
	if (Platform != PLATFORM_Windows && Platform != PLATFORM_MacOSX)
	{
		// Load the 'never cook' list.
		const TCHAR* NeverCookIniSection = TEXT("Cooker.ObjectsToNeverCook");
		FConfigSection* IniNeverCookList = GConfig->GetSectionPrivate(NeverCookIniSection, FALSE, TRUE, GEditorIni);
		if (IniNeverCookList)
		{
			// Add each value to the NeverCookObjects map.
			for (FConfigSectionMap::TIterator It(*IniNeverCookList); It; ++It)
			{
				const FName TypeString = It.Key();
				FString ValueString = It.Value();

				INT* pCheckVal = NeverCookObjects.Find(ValueString);
				if (!pCheckVal)
				{
					NeverCookObjects.Set(ValueString, 1);
					debugf(TEXT("Adding object to NeverCook list: %s"), *ValueString);
				}
			}
		}
	}

	// Create folder for cooked data.
	CookedDir = GetCookedDirectory();
	CookedDirForPerProcessData = CookedDir;
	if( !GFileManager->MakeDirectory( *CookedDir, TRUE ) )
	{
		appDebugMessagef(TEXT("Couldn't create %s"), *CookedDir);
		return FALSE;
	}
	// Check for -FULL command line switch. If we have passed in -FULL we want to cook all packages
	UBOOL bForceFullRecook = Switches.FindItemIndex(TEXT("FULL")) != INDEX_NONE;

	if (!GIsBuildMachine)
	{
		if ((Switches.FindItemIndex(TEXT("QUICK")) != INDEX_NONE) || (Switches.FindItemIndex(TEXT("FASTCOOK")) != INDEX_NONE))
		{
			bQuickMode = TRUE;
			// quick mode skips cooking of the startup maps for most children. 
			// This saves a lot of time, but something about this cooking process correctly omits certain obscure assets from future maps.
			// So for formal builds, we should do the exhaustive accurate process.
			// For almost any other build, quick mode will be nearly identical, except for a few small assets which will be skipped at load time anyway.
			// Quick mode can save about a minute
		}
	}

	UBOOL bMaterialShadersOutdated = FALSE;
	{
		// If we're doing a full recook, pretend the file has been deleted by not reading it.
		// This makes sure bMaterialShadersOutdated will be TRUE.
		// It's going to be deleted properly further down in this function.
		if ( !bForceFullRecook )
		{
			// Load the shader data that the last cook was done with
			const FString ShaderDataFileName = CookedDir + GetShaderDataContainerFilename();
			PersistentShaderData.LoadFromDisk(ShaderDataFileName);
		}

		// Build maps of shader types that have been cooked into packages and their file hashes
		TMap<FString, FSHAHash> NewShaderTypeToSHA;
		for (TLinkedList<FShaderType*>::TIterator It(FShaderType::GetTypeList()); It; It.Next())
		{
			const FGlobalShaderType* GlobalType = It->GetGlobalShaderType();

			// Skip global shaders since they will only be cooked into the global shader cache
			if (!GlobalType)
			{
				NewShaderTypeToSHA.Set(It->GetName(), It->GetSourceHash());
			}
		}

		TMap<FString, FSHAHash> NewVertexFactoryTypeToSHA;
		for (TLinkedList<FVertexFactoryType*>::TIterator FactoryIt(FVertexFactoryType::GetTypeList()); FactoryIt; FactoryIt.Next())
		{
			const FVertexFactoryType* VertexFactoryType = *FactoryIt;
			if (VertexFactoryType)
			{
				NewVertexFactoryTypeToSHA.Set(VertexFactoryType->GetName(), VertexFactoryType->GetSourceHash());
			}
		}

		// Update the persistent shader data with the hashes used for this cook, and detect mismatches from the previous cook
		if (PersistentShaderData.UpdateShaderHashes(NewShaderTypeToSHA, NewVertexFactoryTypeToSHA))
		{
			bMaterialShadersOutdated = TRUE;
		}
	}

	if (Switches.FindItemIndex(TEXT("SINGLETHREAD")) == INDEX_NONE )
	{
		if ( bMaterialShadersOutdated && !GIsBuildMachine) // Outdated material shaders probably indicates a lot of shader compiling is going to happen, so disable MT
		{
			warnf(NAME_Log, TEXT("Ignoring multithreaded flag because shaders are out of date..."));
		}
		else if (bVerifyTextureFileCache || bIniFilesOnly || bAnalyzeReferencedContent || DLCName.Len() || bDumpBulkData
			|| (Platform & PLATFORM_Mobile) // not hard to support, need to merge the preprocessed shaders
			)
		{
			warnf(NAME_Log, TEXT("Ignoring multithreaded flag because of incompatible options..."));
		}
		else
		{
			check(MTChildString.Len() == 0); // we assume this is nothing for the master in several places
			bIsMTMaster = TRUE;
		}
	}
	if (Switches.FindItemIndex(TEXT("MTCHILD")) != INDEX_NONE )
	{
		if (bVerifyTextureFileCache || bIniFilesOnly || bAnalyzeReferencedContent || DLCName.Len() || bDumpBulkData)
		{
			check(0); // this should have been prohibited
		}
		else
		{
			DWORD ProcessID = appGetCurrentProcessId();
			MTChildString = FString::Printf(TEXT("Process_%x"),ProcessID);
			CookedDirForPerProcessData = FFilename(CookedDir) * MTChildString + PATH_SEPARATOR;
			bIsMTChild = TRUE;
			bIsMTMaster = FALSE;
			bForceFullRecook = TRUE; // children are always considered full cooks
			warnf(NAME_Log, TEXT("********* MT Child name is %s"),*MTChildString);
		}
	}
	MultilanguageCookingMask = 0;
	MultilanguageCookingMaskTextOnly = 0;
	if (!bCookCurrentLanguageOnly)
	{
		const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();
		TCHAR LanguageSpec[MAX_SPRINTF]=TEXT("");
		if (Parse(appCmdLine(), TEXT("MultilanguageCook="), LanguageSpec, ARRAY_COUNT(LanguageSpec)))
		{
			check(Localization_GetLanguageExtensionIndex(TEXT("INT"))==0);  // we make this assumption in a few places here
			FString LanguageSpecString = LanguageSpec;
			for (INT LangIndex = 1; LangIndex < KnownLanguageExtensions.Num(); LangIndex++)  // start at 1 to avoid looking for INT
			{
				if (LanguageSpecString.InStr(KnownLanguageExtensions(LangIndex),FALSE,TRUE)!=INDEX_NONE)
				{
					check(LangIndex < 32); // The mask is a DWORD
					MultilanguageCookingMask |= 1 << LangIndex;
				}
				FString TextOnlySearch = TEXT("-");
				TextOnlySearch += KnownLanguageExtensions(LangIndex);
				if (LanguageSpecString.InStr(TextOnlySearch,FALSE,TRUE)!=INDEX_NONE)
				{
					MultilanguageCookingMaskTextOnly |= 1 << LangIndex;
				}
			}
		}
		check((MultilanguageCookingMask & MultilanguageCookingMaskTextOnly) ==  MultilanguageCookingMaskTextOnly);
		if (MultilanguageCookingMask)
		{
			MultilanguageCookingMask |= 1; // we always add INT, which we just verified above was index 0
			MultilanguageCookingMaskTextOnly &= ~1; // INT can't be text only

			check((MultilanguageCookingMask & MultilanguageCookingMaskTextOnly) ==  MultilanguageCookingMaskTextOnly);

			warnf(NAME_Log, TEXT("**** Multilanguage Cook"));
			for (INT LangIndex = 0; LangIndex < KnownLanguageExtensions.Num(); LangIndex++)
			{
				if (MultilanguageCookingMask & (1 << LangIndex))
				{
					warnf(NAME_Log, TEXT("     Adding Language: %s  %s"),*KnownLanguageExtensions(LangIndex), 
						(MultilanguageCookingMaskTextOnly & (1 << LangIndex)) ? TEXT("Text and starup packages only.") : TEXT("Fully localized.")
						);
				}
			}
			check( appStricmp( TEXT("INT"), UObject::GetLanguage() ) == 0 ); // we better be running in INT for this to work
		}
	}

	bSaveLocalizedCookedPackagesInSubdirectories = FALSE;

	// Force a full recook if PersistentCookerData doesn't exist or is outdated.
	FString PersistentCookerDataFilename = CookedDir + GetBulkDataContainerFilename();
	UBOOL bPersistentCookerDataExists = FALSE;
	if (!bIniFilesOnly)
	{
		if ( !bForceFullRecook )
		{
			UBOOL bFileOutOfDate = TRUE;
			UBOOL bFileExists = GFileManager->FileSize( *PersistentCookerDataFilename ) > 0;
			if ( bFileExists )
			{
				// Create a dummy package for the already existing package.
				UPackage* Package = UObject::CreatePackage( NULL, TEXT( "PersistentCookerDataPackage") );
				// GetPackageLinker will find already existing packages first and we cannot reset loaders at this point
				// so we revert to manually creating the linker load ourselves. Don't do this at home, kids!
				UObject::BeginLoad();
				ULinkerLoad* Linker	= ULinkerLoad::CreateLinker( Package, *PersistentCookerDataFilename, LOAD_NoWarn | LOAD_NoVerify );
				UObject::EndLoad();

				bFileOutOfDate = !Linker || (Linker->Summary.CookedContentVersion != GPackageFileCookedContentVersion);

				// Collect garbage to make sure the file gets closed.
				UObject::CollectGarbage(RF_Native);
			}
			if ( !bFileExists || bFileOutOfDate )
			{
				warnf(NAME_Log,TEXT("PCD didn't exist or was out of date '%s'."),*PersistentCookerDataFilename);
				if ( DLCName.Len() && (Platform & PLATFORM_DLCSupported))
				{
					// Try to load the Epic CookerData!
					const FString ShippingCookedDir = GetTrueCookedDirectory();
					const FString ShippingCookerDataFilename = ShippingCookedDir + GetBulkDataContainerFilename();

					if (GGameCookerHelper)
					{
						PersistentCookerData = GGameCookerHelper->CreateInstance(*ShippingCookerDataFilename, FALSE);
					}
					else
					{
						PersistentCookerData = UPersistentCookerData::CreateInstance( *ShippingCookerDataFilename, FALSE );
					}
					if ( !PersistentCookerData )
					{
						appDebugMessagef(TEXT("Can't load cooker data. (%s) (%s)"), *PersistentCookerDataFilename, *ShippingCookerDataFilename );
						return FALSE;
					}
					// We've loaded the shipping cooked data, so now update the Filename member of
					// PersistentCookerData so that it saves to the mod directory 
					PersistentCookerData->SetFilename( *PersistentCookerDataFilename );
				}
	// we never want to delete files in a shipping build unless the user specifically said so
	#if !SHIPPING_PC_GAME
				else
				{
					// Make sure we delete all cooked files so that they will be properly recooked
					// even if we abort cooking for whatever reason but PersistentCookerData has been updated on disk.
					warnf(NAME_Log, TEXT("%s is out of date. Forcing a full recook."), *PersistentCookerDataFilename );
				}
				bForceFullRecook = TRUE;
	#endif
			}
			else
			{
				bPersistentCookerDataExists = TRUE;
			}

			if (bMaterialShadersOutdated && !bForceFullRecook)
			{
				// The persistent data's hashes were different than the new hashes, force all packages to be recooked.
				//@todo - could improve this by only forcing packages containing the outdated shader types to be recooked.
				bForceFullRecook = TRUE;
				warnf(TEXT("Material shader hashes have changed, forcing a recook of all packages."));
			}

			// On Android force a full recook if the texture formats being cooked do not match 
			if (!bForceFullRecook && bPersistentCookerDataExists && Platform == PLATFORM_Android)
			{
				// Grab the PCD early to check the formats
				UPersistentCookerData* TempPersistentCookerData;
				if (GGameCookerHelper)
				{
					TempPersistentCookerData = GGameCookerHelper->CreateInstance(*PersistentCookerDataFilename, TRUE );
				}
				else
				{
					TempPersistentCookerData = UPersistentCookerData::CreateInstance( *PersistentCookerDataFilename, TRUE );
				}

				if (TempPersistentCookerData)
				{
					bForceFullRecook = !TempPersistentCookerData->AreTextureFormatsCooked(AndroidTextureFormat);

					if (bForceFullRecook)
					{
						warnf(NAME_Warning, TEXT("Texture Formats have changed, forcing a recook of all packages."));
					}

					TempPersistentCookerData->RemoveFromRoot();
					CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
				}
			}
		}

		// Are we doing a full recook?
		if( bForceFullRecook 
			&& !bIsMTChild // parent always wipes my directory
			)
		{
			// We want to make certain there are no outdated packages in existence
			// so we delete all of the files in the CookedDir!
			TArray<FString> AllFiles; 
			appFindFilesInDirectory(AllFiles, *CookedDir, TRUE, TRUE);

			// also delete the Audio directory if this is Android
			if (Platform == PLATFORM_Android)
			{
				appFindFilesInDirectory(AllFiles, *(appGameDir() + TEXT("Audio")), TRUE, TRUE);
			}

			// delete em all
			for (INT FileIndex = 0; FileIndex < AllFiles.Num(); FileIndex++)
			{
				warnf(NAME_Log, TEXT("Deleting: %s"), *AllFiles(FileIndex));
				if (!GFileManager->Delete(*AllFiles(FileIndex),FALSE,TRUE))
				{
					warnf(NAME_Error,TEXT("Could not delete %s"),*AllFiles(FileIndex));
					appDebugMessagef(TEXT("Couldn't delete %s"), *AllFiles(FileIndex));
					return FALSE;
				}
			}
			TArray<FString> FoundDirectoryNames;
			GFileManager->FindFiles(FoundDirectoryNames, *(FFilename(CookedDir) * TEXT("*")), FALSE, TRUE );
			for( INT DirIndex = 0; DirIndex < FoundDirectoryNames.Num(); DirIndex++ )
			{
				if (!GFileManager->DeleteDirectory(*(FFilename(CookedDir) * FoundDirectoryNames(DirIndex)), TRUE, TRUE))
				{
					DOUBLE StartTime = appSeconds();
					while (1)
					{
						warnf(NAME_Warning,TEXT("Could not delete directory %s, retrying..."),*(FFilename(CookedDir) * FoundDirectoryNames(DirIndex)));
						appSleep(10.0f); 
						if (GFileManager->DeleteDirectory(*(FFilename(CookedDir) * FoundDirectoryNames(DirIndex)), TRUE, TRUE))
						{
							warnf(NAME_Warning,TEXT("    %s sucessfully deleted."),*(FFilename(CookedDir) * FoundDirectoryNames(DirIndex)));
							break;
						}
						if (appSeconds() - StartTime > MTTimeout + 5.0)
					{
							appErrorf(TEXT("Could not delete directory %s"),*(FFilename(CookedDir) * FoundDirectoryNames(DirIndex)));
						}
					}
				}
			}
		}
		else if (bIsMTMaster)
		{
			// make sure all process directories are gone
			TArray<FString> FoundDirectoryNames;
			GFileManager->FindFiles(FoundDirectoryNames, *(FFilename(CookedDir) * TEXT("Process_*")), FALSE, TRUE );
			for( INT DirIndex = 0; DirIndex < FoundDirectoryNames.Num(); DirIndex++ )
			{
				if (!GFileManager->DeleteDirectory(*(FFilename(CookedDir) * FoundDirectoryNames(DirIndex)), TRUE, TRUE))
				{
					DOUBLE StartTime = appSeconds();
					while (1)
					{
						warnf(NAME_Warning,TEXT("Could not delete directory %s, retrying..."),*(FFilename(CookedDir) * FoundDirectoryNames(DirIndex)));
						appSleep(10.0f); 
						if (GFileManager->DeleteDirectory(*(FFilename(CookedDir) * FoundDirectoryNames(DirIndex)), TRUE, TRUE))
						{
							warnf(NAME_Warning,TEXT("    %s sucessfully deleted."),*(FFilename(CookedDir) * FoundDirectoryNames(DirIndex)));
							break;
						}
						if (appSeconds() - StartTime > MTTimeout + 5.0)
						{
							appErrorf(TEXT("Could not delete child directory %s"),*(FFilename(CookedDir) * FoundDirectoryNames(DirIndex)));
						}
					}
				}
			}
		}
	}

	//enable based on the platform we're cooking for
	GMobileShaderCooker.Init(Platform, *CookedDir, ShouldByteSwapData());

	// create the ini files for the target platform (we aren't signing inis on PC)
	CreateIniFiles(Platform & PLATFORM_PC ? FALSE : bGenerateSHAHashes);

	// prepare shaders for compiling
	if ( !PrepareShaderFiles() )
	{
		appDebugMessagef( TEXT("Couldn't prepare shader files") );
		return FALSE;
	}

	if (Platform == PLATFORM_Windows ||
		Platform == PLATFORM_WindowsConsole ||
		Platform == PLATFORM_WindowsServer)
	{
		// on windows, we use the current settings since the values in the INI are set for what this machine will play
		// with, not necessarily what it should cook with. We force the current settings to the highest when running
		// the cooker, so just use those.
		PlatformLODSettings = GSystemSettings.TextureLODSettings;
		PlatformDetailMode = GSystemSettings.DetailMode;
	}
	else
	{
		// Initialize LOD settings from the patform's engine ini.
		PlatformLODSettings.Initialize(PlatformSystemSettingsConfigFilename, TEXT("SystemSettings"));

		// Initialize platform detail mode.
		GConfig->GetInt( TEXT("SystemSettings"), TEXT("DetailMode"), PlatformDetailMode, PlatformSystemSettingsConfigFilename );

		UBOOL bAllowHighQualityMaterials;
		if (GConfig->GetBool( TEXT("SystemSettings"), TEXT("bAllowHighQualityMaterials"), bAllowHighQualityMaterials, PlatformSystemSettingsConfigFilename ))
		{
			// if the target platform doesn't allow high quality materials, then cook to low quality
			if (!bAllowHighQualityMaterials)
			{
				GCookingMaterialQuality = MSQ_LOW;
			}
		}
	}

	// Add a blank line to break up initial logging from cook logging.
	warnf(TEXT(" "));

	if ( (GCookingTarget & UE3::PLATFORM_Stripped) != 0 )
	{
		// determine whether we should cook-out StaticMeshActors
		GConfig->GetBool(TEXT("Engine.StaticMeshCollectionActor"), TEXT("bCookOutStaticMeshActors"), bCookOutStaticMeshActors, PlatformEngineConfigFilename);
		bCookOutStaticMeshActors = (bCookOutStaticMeshActors || Switches.ContainsItem(TEXT("RemoveStaticMeshActors"))) && !Switches.ContainsItem(TEXT("KeepStaticMeshActors"));
		if ( bCookOutStaticMeshActors )
		{
			debugf(NAME_Log, TEXT("StaticMeshActors will be removed from cooked files."));
		}
		else
		{
			debugf(NAME_Log, TEXT("StaticMeshActors will NOT be removed from cooked files."));
		}

		// determine whether we should cook-out static Lights
		GConfig->GetBool(TEXT("Engine.StaticLightCollectionActor"), TEXT("bCookOutStaticLightActors"), bCookOutStaticLightActors, PlatformEngineConfigFilename);
		bCookOutStaticLightActors = (bCookOutStaticLightActors || Switches.ContainsItem(TEXT("RemoveStaticLights"))) && !Switches.ContainsItem(TEXT("KeepStaticLights"));
		if ( bCookOutStaticLightActors )
		{
			debugf(NAME_Log, TEXT("Static Light actors will be removed from cooked files."));
		}
		else
		{
			debugf(NAME_Log, TEXT("Static Light actors will NOT be removed from cooked files."));
		}

		GConfig->GetBool(TEXT("Cooker.MatineeOptions"), TEXT("bBakeAndPruneDuringCook"), bBakeAndPruneDuringCook, GEditorIni);
		bBakeAndPruneDuringCook = bBakeAndPruneDuringCook || Switches.ContainsItem(TEXT("BakeAndPrune"));
		if (bBakeAndPruneDuringCook)
		{
			debugf(NAME_Log, TEXT("Matinees will be baked and pruned when tagged as such."));
		}
		else
		{
			debugf(NAME_Log, TEXT("Matinees will be NOT baked and pruned when tagged as such."));
		}

		GConfig->GetBool(TEXT("Cooker.MatineeOptions"), TEXT("bAllowBakeAndPruneOverride"), bAllowBakeAndPruneOverride, GEditorIni);
		bAllowBakeAndPruneOverride = bAllowBakeAndPruneOverride || Switches.ContainsItem(TEXT("OVERRIDEBANDP"));
		if (bAllowBakeAndPruneOverride)
		{
			debugf(NAME_Log, TEXT("Matinees will allow overriding of bake and prune when tagged as such."));
		}
		else
		{
			debugf(NAME_Log, TEXT("Matinees will NOT allow overriding of bake and prune when tagged as such."));
		}

		PersistentFaceFXAnimSetGenerator.SetGeneratePersistentMapAnimSet( PersistentFaceFXAnimSetGenerator.GetGeneratePersistentMapAnimSet() || Switches.ContainsItem(TEXT("PersistentAnimSet")));

		if (PersistentFaceFXAnimSetGenerator.GetGeneratePersistentMapAnimSet())
		{
			debugf(NAME_Log, TEXT("FaceFX animations will be pulled into the persistent map."));
		}
		else
		{
			debugf(NAME_Log, TEXT("FaceFX animation will be NOT be pulled into the persistent map."));
		}
		PersistentFaceFXAnimSetGenerator.ClearCurrentPersistentMapCache();
	}


	if (bIniFilesOnly)
	{
		warnf(NAME_Log, TEXT("Cooked .ini and localization files."));

		// if generating sha's, we may need the GPCD
		if( !PersistentCookerData )
		{
			if (GGameCookerHelper)
			{
				PersistentCookerData = GGameCookerHelper->CreateInstance(*PersistentCookerDataFilename, TRUE );
			}
			else
			{
				PersistentCookerData = UPersistentCookerData::CreateInstance( *PersistentCookerDataFilename, TRUE );
			}
			check( PersistentCookerData );
		}

		return TRUE;
	}

	if (Platform == PLATFORM_Xbox360)
	{
		// Change physics to cook content for Xenon target.
		SetPhysCookingXenon();
	}
	else if (Platform == PLATFORM_PS3)
	{
		// Change physics to cook content for PS3 target.
		SetPhysCookingPS3();

		// set the compression chunk size to 64k to fit in SPU local ram
		extern INT GSavingCompressionChunkSize;
		GSavingCompressionChunkSize = 64 * 1024;
	}

	// when cooking a mod for PS3, we need to get the shipped cooker data to know where a cooked texture came from
	// (we need a separate instance that cannot contain any mod texture info)
	if ( DLCName.Len() && (Platform & PLATFORM_DLCSupported))
	{
		const FString ShippingCookedDir = GetTrueCookedDirectory();

		// hardcode the filename because we know what we shipped with :)
		const FString ShippingCookerDataFilename = ShippingCookedDir + GetBulkDataContainerFilename();

		// create the instance for the shipped info
		if (GGameCookerHelper)
		{
			GShippingCookerData = GGameCookerHelper->CreateInstance(*ShippingCookerDataFilename, FALSE);
		}
		else
		{
			GShippingCookerData = UPersistentCookerData::CreateInstance( *ShippingCookerDataFilename, FALSE );
		}

		checkf(GShippingCookerData, TEXT("Failed to find shipping global cooker info file [%s]"), *ShippingCookerDataFilename);
	}

	// recompile the global shaders if we need to
	if( Switches.FindItemIndex(TEXT("RECOMPILEGLOBALSHADERS")) != INDEX_NONE && !bIsMTChild)
	{
		extern TShaderMap<FGlobalShaderType>* GGlobalShaderMap[SP_NumPlatforms];	
		delete GGlobalShaderMap[ShaderPlatform];
		GGlobalShaderMap[ShaderPlatform] = NULL;
	}

	// Keep track of time spent loading packages.
	DOUBLE StartTime = appSeconds();

	{
		SCOPE_SECONDS_COUNTER_COOK(LoadNativePackagesTime);
		// Load up all native script files, not excluding game specific ones.
		LoadAllNativeScriptPackages();
	}
		
	// call the shared function to get all native script package names
	TArray<FString> PackageNames;
	appGetScriptPackageNames(PackageNames, SPT_Native, PlatformEngineConfigFilename);

	// Make sure that none of the loaded script packages get garbage collected.
	for( FObjectIterator It; It; ++It )
	{
		UObject* Object = *It;
		// Check for code packages and add them and their contents to root set.
		if( Object->GetOutermost()->PackageFlags & PKG_ContainsScript )
		{
			// Only add class objects to root if they are in native script packages (this will prevent redirectors from 
			// causing problems with adding objects to root, but then not cooking into packages, see
			// https://udn.epicgames.com/lists/showpost.php?id=45492&list=unprog3 )
			if( PackageNames.ContainsItem(*Object->GetOutermost()->GetName()) )
			{
				Object->AddToRoot();
			}
		}
	}

	// get bool indicating whether standalone seekfree packages must be server side only
	UBOOL bStandaloneSFServerSideOnly = FALSE;
	GConfig->GetBool(TEXT("Engine.PackagesToAlwaysCook"), TEXT("bStandaloneSFServerSideOnly"), bStandaloneSFServerSideOnly, PlatformEngineConfigFilename);

	// read in the per-map packages to cook
	TMap<FName, TArray<FName> > PerMapCookPackages;
	GConfig->Parse1ToNSectionOfNames(TEXT("Engine.PackagesToForceCookPerMap"), TEXT("Map"), TEXT("Package"), PerMapCookPackages, PlatformEngineConfigFilename);

	// Retrieve list of editor-only package names.
	GConfig->GetArray(TEXT("UnrealEd.EditorEngine"), TEXT("EditorOnlyContentPackages"), EditorOnlyContentPackageNames, GEngineIni);
	
	if( GCookingTarget & UE3::PLATFORM_Stripped )
	{
		// List of packages we removed.
		TArray<FString> RemovedPackageNames;

		// For each package in editor-only list, find all objects inside and remove from root and mark as pending kill.
		// This will allow the garbage collector to purge references to them.
		for( INT NameIndex=0; NameIndex<EditorOnlyContentPackageNames.Num(); NameIndex++ )
		{
			// Check whether package is loaded. This will also find subgroups of the same name so we verify that it is a toplevel package.
			UPackage* Package = FindObject<UPackage>(ANY_PACKAGE,*EditorOnlyContentPackageNames(NameIndex));
			if( Package && Package->GetOutermost() == Package )
			{
				// Keep track of what we're removing so we can verify.
				new(RemovedPackageNames) FString(*EditorOnlyContentPackageNames(NameIndex));

				// Iterate over all objects inside package and mark them as pending kill and remove from root.
				for( FObjectIterator It; It; ++It )
				{
					UObject* Object = *It;
					if( Object->IsIn( Package ) )
					{
						Object->RemoveFromRoot();
						Object->MarkPendingKill();
					}
				}
				// Last but not least, do the same for the package.
				Package->RemoveFromRoot();
				Package->MarkPendingKill();
			}
		}

		// This should purge all editor-only packages and content.
		UObject::CollectGarbage(RF_Native);

		// Verify removal of packages we tried to remove.
		for( INT PackageIndex=0; PackageIndex<RemovedPackageNames.Num(); PackageIndex++ )
		{
			UPackage* Package = FindObject<UPackage>(ANY_PACKAGE,*RemovedPackageNames(PackageIndex));
			if( Package && Package->GetOutermost() == Package )
			{
				warnf(TEXT("Unable to remove editor-only package %s"),*Package->GetName());
			}
		}
	}

	// Iterate over all objects and mark them to be excluded from being put into the seek free package.
	EObjectFlags CheckFlags = RF_Native|RF_ClassDefaultObject;
	if (bSkipStartupObjectDetermination == TRUE)
	{
		CheckFlags |= RF_Transient;
	}
	for( FObjectIterator It; It; ++It )
	{
		UObject*		Object				= *It;
		UPackage*		Package				= Cast<UPackage>(Object);
		ULinkerLoad*	LinkerLoad			= Object->GetLinker();
		FString			OutFileName;
		
		// Toplevel packages don't have a linker so we try to find it unless we know it's not a package or it is
		// the transient package, or it doesn't exist on disk.
		if( !LinkerLoad && Package && Package != UObject::GetTransientPackage() && !Package->HasAnyFlags(RF_ClassDefaultObject) && GPackageFileCache->FindPackageFile( *Package->GetName(), NULL, OutFileName ) )
		{
			SCOPE_SECONDS_COUNTER_COOK(LoadPackagesTime);
			UObject::BeginLoad();

			LinkerLoad = UObject::GetPackageLinker( Package, NULL, LOAD_NoWarn, NULL, NULL );

			UObject::EndLoad();
		}

		// Mark objects that reside in a code package, are native, a default object or transient to not be put 
		// into the seek free package.
		if( (LinkerLoad && LinkerLoad->ContainsCode()) || Object->HasAnyFlags(CheckFlags))
		{
			if (bSkipStartupObjectDetermination == FALSE)
			{
				TrackedInitialMarkedObjects.Set(Object->GetFullName(), TRUE);
				Object->SetFlags(RF_CookedStartupObject);
			}
			Object->SetFlags(RF_MarkedByCooker);
			if (bSkipStartupObjectDetermination == FALSE)
			{
				if (Object->HasAnyFlags(RF_MarkedByCookerTemp))
				{
					Object->ClearFlags(RF_MarkedByCookerTemp|RF_Transient);
				}
			}
		}
	}

	// packages needed just to start the cooker, which we don't want to cook when in user mode
	TMap<FString,INT> InitialDependencies;
	if (DLCName.Len())
	{
		// mark that all packages currently loaded (ie before loading usermode commandline specified packages) shouldn't be cooked later
		for( FObjectIterator It; It; ++It )
		{
			UObject* Object = *It;
			if( Object->GetLinker() )
			{
				// We need to (potentially) cook this package.
				InitialDependencies.Set( *Object->GetLinker()->Filename, 0 );
			}
		}
	}


	// create or open Guid cache file (except when cooking mods for PC, they just look in the packages for GUIDs)
	if (DLCName.Len() && Platform == PLATFORM_Windows)
	{
		GuidCache = NULL;
	}
	else
	{
		const FString GuidCacheFilename( CookedDirForPerProcessData + GetGuidCacheFilename() );
		GuidCache = UGuidCache::CreateInstance( *GuidCacheFilename );
		check( GuidCache );
	}

	if (!PersistentCookerData)
	{
		if (!bPersistentCookerDataExists && DLCName.Len() && (Platform & PLATFORM_DLCSupported))
		{
			warnf(NAME_Log,TEXT("PCD didn't exist or was out of date '%s'."),*PersistentCookerDataFilename);
			// Try to load the Epic CookerData!
			const FString ShippingCookedDir = GetTrueCookedDirectory();
			const FString ShippingCookerDataFilename = ShippingCookedDir + GetBulkDataContainerFilename();

			if (GGameCookerHelper)
			{
				PersistentCookerData = GGameCookerHelper->CreateInstance(*ShippingCookerDataFilename, FALSE);
			}
			else
			{
				PersistentCookerData = UPersistentCookerData::CreateInstance( *ShippingCookerDataFilename, FALSE );
			}
			if ( !PersistentCookerData )
			{
				appDebugMessagef(TEXT("Can't load cooker data. (%s) (%s)"), *PersistentCookerDataFilename, *ShippingCookerDataFilename );
				return FALSE;
			}
			// We've loaded the shipping cooked data, so now update the Filename member of
			// PersistentCookerData so that it saves to the mod directory 
			PersistentCookerData->SetFilename( *PersistentCookerDataFilename );
		}
		else
		{
			// Create container helper object for keeping track of where bulk data ends up inside cooked packages. This is needed so we can fix
			// them up in the case of forced exports that don't need their bulk data inside the package they were forced into.
			//		PersistentCookerData = UPersistentCookerData::CreateInstance( *PersistentCookerDataFilename, TRUE );
			if (GGameCookerHelper)
			{
				PersistentCookerData = GGameCookerHelper->CreateInstance(*PersistentCookerDataFilename, TRUE );
			}
			else
			{
				PersistentCookerData = UPersistentCookerData::CreateInstance( *PersistentCookerDataFilename, TRUE );
			}
		}
		check( PersistentCookerData );
	}

	if (bIsMTChild)
	{
		// need to save this elsewhere
		PersistentCookerDataFilename = CookedDirForPerProcessData + GetBulkDataContainerFilename();
		PersistentCookerData->SetFilename( *PersistentCookerDataFilename );
	}

	if ( bAnalyzeReferencedContent )
	{
		// Ignore for now everything else except FaceFX
		// Please check UAnalyzeReferencedContent for more info if you'd like to add more types
		ReferencedContentStat.SetIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_StaticMesh|
			FAnalyzeReferencedContentStat::IGNORE_StaticMeshComponent|FAnalyzeReferencedContentStat::IGNORE_StaticMeshActor|
			FAnalyzeReferencedContentStat::IGNORE_Texture|FAnalyzeReferencedContentStat::IGNORE_Material|FAnalyzeReferencedContentStat::IGNORE_Particle|
			FAnalyzeReferencedContentStat::IGNORE_Anim|FAnalyzeReferencedContentStat::IGNORE_LightingOptimization|FAnalyzeReferencedContentStat::IGNORE_SoundCue|FAnalyzeReferencedContentStat::IGNORE_SoundNodeWave|
			FAnalyzeReferencedContentStat::IGNORE_Brush|FAnalyzeReferencedContentStat::IGNORE_Level|FAnalyzeReferencedContentStat::IGNORE_ShadowMap|
			FAnalyzeReferencedContentStat::IGNORE_SkeletalMesh|FAnalyzeReferencedContentStat::IGNORE_SkeletalMeshComponent);
	}

	// Make a list of script ref'd anim sets... (for efforts)
	PersistentFaceFXAnimSetGenerator.SetupScriptReferencedFaceFXAnimSets();

	if (1)//@todo. Make this an option in the ini file??
	{
		PersistentMapInfoHelper.SetCallerInfo(this);
		PersistentMapInfoHelper.SetPersistentMapInfoGenerationVerboseLevel(FPersistentMapInfo::VL_Simple);
		// Copy out all the map names and send them into the persistent map info helper
		TArray<FString> AllMapsList;
		for (INT TestIdx = 0; TestIdx < Tokens.Num(); TestIdx++)
		{
			AllMapsList.AddItem(Tokens(TestIdx));
		}
		PersistentMapInfoHelper.GeneratePersistentMapList(AllMapsList, TRUE, FALSE);
		PersistentFaceFXAnimSetGenerator.SetPersistentMapInfo(&PersistentMapInfoHelper);
		if (GGameCookerHelper)
		{
			GGameCookerHelper->PersistentMapInfoGeneratedCallback(this);
		}
	}

	// Disabled for this check-in
#if WITH_FACEFX
	if (PersistentFaceFXAnimSetGenerator.GetGeneratePersistentMapAnimSet())
	{
		PersistentFaceFXAnimSetGenerator.SetCallerInfo( this, &UCookPackagesCommandlet::CollectGarbageAndVerify );
	}
#endif	// WITH_FACEFX

	bPreloadingPackagesForDLC = FALSE;
	// Check whether we only want to cook passed in packages and their dependencies, unless
	// it was specified to cook all packages
	bOnlyCookDependencies = DLCName.Len() != 0;
	if( bOnlyCookDependencies )
	{
		SCOPE_SECONDS_COUNTER_COOK(LoadDependenciesTime);

		bPreloadingPackagesForDLC = TRUE;

		// Iterate over all passed in packages, loading them.
		for( INT TokenIndex=0; TokenIndex<Tokens.Num(); TokenIndex++ )
		{
			const FString& Token = Tokens(TokenIndex);

			// Load package if found.
			FString PackageFilename;
			UPackage* Result = NULL;
			if( GPackageFileCache->FindPackageFile( *Token, NULL, PackageFilename ) )
			{
				warnf(NAME_Log, TEXT("Loading base level %s"), *PackageFilename);
				Result = LoadPackageForCooking(*PackageFilename);
				if (Result == NULL)
				{
					warnf(NAME_Error, TEXT("Failed to load base level %s"), *Token);
					return FALSE;
				}

				// Classify packages specified on the command line.
				if ( Result->PackageFlags & PKG_ContainsMap )
				{
					TokenMaps.AddItem( PackageFilename );
					CommandLineMaps.AddItem(PackageFilename);
				}
				else if ( Result->PackageFlags & PKG_ContainsScript )
				{
					TokenScriptPackages.AddItem( PackageFilename );
				}
				else
				{
					TokenContentPackages.AddItem( PackageFilename );
				}

				// add dependencies for the per-map packages for this map (if any)
				TArray<FName>* Packages = PerMapCookPackages.Find(Result->GetFName());
				if (Packages != NULL)
				{
					for (INT PackageIndex = 0; PackageIndex < Packages->Num(); PackageIndex++)
					{
						FName PackageName = (*Packages)(PackageIndex);
						FString PackageFilename;
						if( GPackageFileCache->FindPackageFile( *PackageName.ToString(), NULL, PackageFilename ) )
						{
							if (!ForceLoadPackage(PackageFilename))
							{
								return FALSE;
							}
						}
					}
				}
			}
			if (Result == NULL)
			{
				warnf(NAME_Error, TEXT("Failed to load base level %s"), *Token);
				return FALSE;
			}

			TArray<FString> SubLevelFilenames;

			// Iterate over all UWorld objects and load the referenced levels.
			for( TObjectIterator<UWorld> It; It; ++It )
			{
				UWorld*		World		= *It;
				AWorldInfo* WorldInfo	= World->GetWorldInfo();
				// Iterate over streaming level objects loading the levels.
				for( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
				{
					ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
					if( StreamingLevel )
					{
						// Load package if found.
						FString PackageFilename;
						if( GPackageFileCache->FindPackageFile( *StreamingLevel->PackageName.ToString(), NULL, PackageFilename ) )
						{
							SubLevelFilenames.AddItem(*PackageFilename);
							TokenMaps.AddUniqueItem(*PackageFilename);
						}
					}
				}
			}

			for (INT SubLevelIndex = 0; SubLevelIndex < SubLevelFilenames.Num(); SubLevelIndex++)
			{
				warnf(NAME_Log, TEXT("Loading sub-level %s"), *SubLevelFilenames(SubLevelIndex));
				LoadPackageForCooking(*SubLevelFilenames(SubLevelIndex));
			}

			// Iterate over all objects and add the filename of their linker (if existing) to the dependencies map.
			for( FObjectIterator It; It; ++It )
			{
				UObject* Object = *It;
				if( Object->GetLinker() )
				{
					// by default, all loaded object's packages are valid dependencies
					UBOOL bIsValidDependency = TRUE;

					// handle special cases for user mode
					if (DLCName.Len())
					{
						// don't cook packages during startup, or already cooked packages
						if (InitialDependencies.Find(Object->GetLinker()->Filename) != NULL ||
							(Object->GetOutermost()->PackageFlags & PKG_Cooked) || 
							(Object->GetLinker()->Summary.PackageFlags & PKG_Cooked))
						{
							bIsValidDependency = FALSE;
						}
					}

					// if allowed, put this objects package into the list of dependent packages
					if (bIsValidDependency)
					{
						// We need to (potentially) cook this package.
						PackageDependencies.Set( *Object->GetLinker()->Filename, 0 );
					}
				}
			}

			UObject::CollectGarbage(RF_Native);
		}

		bPreloadingPackagesForDLC = FALSE;
	}
	else
	{
		// add the always cook list to the tokens to cook
		TArray<FString>				AlwaysCookPackages;
		LoadSectionPackages(TEXT("Engine.PackagesToAlwaysCook"), AlwaysCookPackages, TEXT("Package"), TRUE);
        // add FontLib packages
#if WITH_GFx
		if( (Platform & PLATFORM_Mobile) == 0)
		{
			GFx_AddExtraPackages(AlwaysCookPackages);
		}
#endif // WITH_GFx

		for ( INT PackageIdx = 0; PackageIdx < AlwaysCookPackages.Num(); PackageIdx++ )
		{
			Tokens.AddUniqueItem(AlwaysCookPackages(PackageIdx));
		}

		// Figure out the list of maps to check for need of cooking
		for( INT TokenIndex=0; TokenIndex<Tokens.Num(); TokenIndex++ )
		{
			FString PackageFilename;
			if( GPackageFileCache->FindPackageFile(*Tokens(TokenIndex), NULL, PackageFilename))
			{
				TArray<FString>* ListOfPackages;

				// add the token to the appropriate list along wiht any additional packages stored in the 
				// package file summary

				// read the package file summary of the package to get a list of sublevels
				FArchive* PackageFile = GFileManager->CreateFileReader(*PackageFilename);
				if (PackageFile)
				{
					// read the package summary, which has list of sub levels
					FPackageFileSummary Summary;
					(*PackageFile) << Summary;

					// close the map
					delete PackageFile;

					if ( (Summary.PackageFlags&PKG_ContainsMap) != 0 )
					{
						warnf(NAME_Log, TEXT("Adding level %s for cooking..."), *PackageFilename);
						ListOfPackages = &TokenMaps;
						CommandLineMaps.AddItem(PackageFilename);
					}
					else if ( (Summary.PackageFlags&PKG_ContainsScript) != 0 )
					{
						warnf(NAME_Log, TEXT("Adding script %s for cooking..."), *PackageFilename);
						ListOfPackages = &TokenScriptPackages;
					}
					else
					{
						warnf(NAME_Log, TEXT("Adding package %s for cooking..."), *PackageFilename);
						ListOfPackages = &TokenContentPackages;
					}

					ListOfPackages->AddUniqueItem(PackageFilename);

					// if it's an old map, then we have to load it to get the list of sublevels
					if (Summary.GetFileVersion() < VER_ADDITIONAL_COOK_PACKAGE_SUMMARY)
					{
						SCOPE_SECONDS_COUNTER_COOK(LoadPackagesTime);

						if (ListOfPackages == &TokenMaps)
						{
							warnf(NAME_Log, TEXT("  Old package, so must open fully to look for sublevels"), *PackageFilename);
							UPackage* Package = UObject::LoadPackage(NULL, *PackageFilename, LOAD_None);

							// Iterate over all UWorld objects and load the referenced levels.
							for( TObjectIterator<UWorld> It; It; ++It )
							{
								UWorld*		World		= *It;
								if (World->IsIn(Package))
								{
									AWorldInfo* WorldInfo	= World->GetWorldInfo();
									// Iterate over streaming level objects loading the levels.
									for (INT LevelIndex = 0; LevelIndex < WorldInfo->StreamingLevels.Num(); LevelIndex++)
									{
										ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
										if( StreamingLevel )
										{
											// Load package if found.
											FString SubPackageFilename;
											if (GPackageFileCache->FindPackageFile(*StreamingLevel->PackageName.ToString(), NULL, SubPackageFilename))
											{
												warnf(NAME_Log, TEXT("Adding sublevel %s for cooking..."), *SubPackageFilename);
												ListOfPackages->AddUniqueItem(SubPackageFilename);
											}
										}
									}
								}
							}

							// close the package
							CollectGarbageAndVerify();
						}
					}
					else
					{
						// tack the streaming levels onto the list of maps to cook
						for (INT AdditionalPackageIndex = 0; AdditionalPackageIndex < Summary.AdditionalPackagesToCook.Num(); AdditionalPackageIndex++)
						{
							if( GPackageFileCache->FindPackageFile(*Summary.AdditionalPackagesToCook(AdditionalPackageIndex), NULL, PackageFilename))
							{
								warnf(NAME_Log, TEXT("Adding additional package %s for cooking..."), *PackageFilename);
								ListOfPackages->AddUniqueItem(PackageFilename);
							}
						}
					}
				}
				else
				{
					warnf(NAME_Error, TEXT("Failed to create file reader for package %s while cooking..."), *PackageFilename);
				}
			}
			else
			{
				warnf(NAME_Error, TEXT("Failed to find package %s for cooking..."), *Tokens(TokenIndex));
			}
		}
	}

	if (bDumpBulkData)
	{
		PersistentCookerData->DumpBulkDataInfos();
		return FALSE;
	}

	// retrieve the save localized
	GConfig->GetBool(TEXT("Core.System"), TEXT("SaveLocalizedCookedPackagesInSubdirectories"), bSaveLocalizedCookedPackagesInSubdirectories, PlatformEngineConfigFilename);

	// Check whether we are using the texture file cache or not.
	GConfig->GetBool(TEXT("TextureStreaming"), TEXT("UseTextureFileCache"), bUseTextureFileCache, PlatformEngineConfigFilename);

	// load the number of mips to always be resident, for the target platform
	verify(GConfig->GetInt(TEXT("TextureStreaming"), TEXT("MinTextureResidentMipCount"), GMinTextureResidentMipCount, PlatformEngineConfigFilename));

	// Alignment for bulk data stored in the texture file cache. This will result in alignment overhead and will affect the size of the TFC files generated
	INT TempTextureFileCacheBulkDataAlignment = DVD_ECC_BLOCK_SIZE;
	GConfig->GetInt(TEXT("TextureStreaming"), TEXT("TextureFileCacheBulkDataAlignment"), TempTextureFileCacheBulkDataAlignment, PlatformEngineConfigFilename);
	TextureFileCacheBulkDataAlignment = Max<INT>(TempTextureFileCacheBulkDataAlignment,0);

	// We always need to generate TFCs, even if they are empty. Otherwise the blast step of 
	// publishing will fail due to missing files. Not needed for user mode tho, since it 
	// will have it's own blast setup
	// children have slightly different TFC names and it is ok if they are missing
	if (bUseTextureFileCache == TRUE && !DLCName.Len() && !bIsMTChild )
	{
		// Use corrected names if building for Android
		if (Platform == PLATFORM_Android)
		{
			if (AndroidTextureFormat & TEXSUPPORT_DXT)
			{
				CreateAndroidTFCFile(TEXSUPPORT_DXT);
			}
			if (AndroidTextureFormat & TEXSUPPORT_PVRTC)
			{
				CreateAndroidTFCFile(TEXSUPPORT_PVRTC);
			}
			if (AndroidTextureFormat & TEXSUPPORT_ATITC)
			{
				CreateAndroidTFCFile(TEXSUPPORT_ATITC);
			}
			if (AndroidTextureFormat & TEXSUPPORT_ETC)
			{
				CreateAndroidTFCFile(TEXSUPPORT_ETC);
			}
		}
		else
		{
			FArchive* BaseTFC = GetTextureCacheArchive(BaseTFCName);
			check(BaseTFC);
			if (bSeparateLightingTFC == TRUE)
			{
				FArchive* LightingTFC = GetTextureCacheArchive(LightingTFCName);
				check(LightingTFC);
			}
			if (bSeparateCharacterTFC == TRUE)
			{
				FArchive* CharacterTFC = GetTextureCacheArchive(CharacterTFCName);
				check(CharacterTFC);
			}
		}		
	}

	// 'Game' cooker helper call to Init
	if (GGameCookerHelper)
	{
		GGameCookerHelper->Init(this, Tokens, Switches);
	}

	if (!bIsMTChild)
	{
		// Compile all global shaders.
		VerifyGlobalShaders( ShaderPlatform );
	}

	// Make sure shader caches are loaded for the target platform before we start processing any materials
	// Without this, the first material (DefaultMaterial) will be recompiled every cook
	GetLocalShaderCache( ShaderPlatform );

	return TRUE;
}

/**
 *	Initializes the cooker for cleaning up materials
 */
void UCookPackagesCommandlet::InitializeMaterialCleanupSettings()
{
	bCleanupMaterials = FALSE;
	GConfig->GetBool(TEXT("Cooker.GeneralOptions"), TEXT("bCleanupMaterials"), bCleanupMaterials, GEditorIni);
	if ((Switches.FindItemIndex(TEXT("SKIPMATERIALCLEANUP")) != INDEX_NONE) ||
		(Switches.FindItemIndex(TEXT("FASTCOOK")) != INDEX_NONE))
	{
		bCleanupMaterials = FALSE;
		warnf(NAME_Log, TEXT("Command-line disabling of CleanupMaterials (-SKIPMATERIALCLEANUP)"));
	}
	else
	{
		warnf(NAME_Log, TEXT("Materials will%s be cleaned up..."), bCleanupMaterials ? TEXT("") : TEXT(" NOT"));
	}

	bCleanStaticMeshMaterials = FALSE;
	if (bCleanupMaterials == TRUE)
	{
		GConfig->GetBool(TEXT("Cooker.GeneralOptions"), TEXT("bCleanStaticMeshMaterials"), bCleanStaticMeshMaterials, GEditorIni);
		if (Switches.FindItemIndex(TEXT("SKIPSTATICMESHCLEAN")) != INDEX_NONE)
		{
			bCleanStaticMeshMaterials = FALSE;
			warnf(NAME_Log, TEXT("Command-line disabling of bCleanStaticMeshMaterials (-SKIPSTATICMESHCLEAN)"));
		}
		else
		{
			warnf(NAME_Log, TEXT("StaticMesh materials will%s be cleaned up..."), bCleanStaticMeshMaterials ? TEXT("") : TEXT(" NOT"));
		}

		bSkipStaticSwitchClean = (Switches.FindItemIndex(TEXT("SKIPSTATICSWITCHCLEAN")) != INDEX_NONE);
		if (bSkipStaticSwitchClean == TRUE)
		{
			warnf(NAME_Log, TEXT("Command-line disabling of Material StaticSwitch cleaning (-SKIPSTATICSWITCHCLEAN)"));
		}

		ScriptReferencedStaticMeshes.Empty();
		if (bCleanStaticMeshMaterials == TRUE)
		{
			// List all static meshes currently loaded... this will cover engine-level refs and native game script.
			for (TObjectIterator<UStaticMesh> It; It; ++It)
			{
				UStaticMesh* StaticMesh = *It;
				ScriptReferencedStaticMeshes.Set(StaticMesh->GetPathName(), TRUE);
			}

			TArray<FString> GameScriptPackages;
			appGetScriptPackageNames(GameScriptPackages, SPT_NonNative);

			// Load the non-native game script packages and add their static meshes to the list...
			FName StaticMeshName = FName(TEXT("StaticMesh"));
			TSet<FDependencyRef> AllDepends;
			for (INT GameScriptIdx = 0; GameScriptIdx < GameScriptPackages.Num(); GameScriptIdx++)
			{
				// Just use the linker import/export maps...
				FString GameScript = GameScriptPackages(GameScriptIdx);

				UObject::BeginLoad();
				ULinkerLoad* Linker = UObject::GetPackageLinker(NULL, *GameScript, LOAD_NoVerify, NULL, NULL);
				UObject::EndLoad();

				checkf(Linker,TEXT("Failed to load linker for %s"),*GameScript);

				// gather all the unique dependencies for all the objects in the package
				for (INT ImportIndex = 0; ImportIndex < Linker->ImportMap.Num(); ImportIndex++)
				{
					if (Linker->ImportMap(ImportIndex).ClassName == StaticMeshName)
					{
						FString ImportName = Linker->GetImportPathName(ImportIndex);
						ScriptReferencedStaticMeshes.Set(ImportName, TRUE);
					}
					else
					{
						Linker->GatherImportDependencies(ImportIndex, AllDepends);
					}
				}
			}

			for(TSet<FDependencyRef>::TConstIterator It(AllDepends);It;++It)
			{
				const FDependencyRef& Ref = *It;
				if (Ref.Linker->GetExportClassName(Ref.ExportIndex) == StaticMeshName)
				{
					ScriptReferencedStaticMeshes.Set(Ref.Linker->GetExportPathName(Ref.ExportIndex), TRUE);
				}
			}

			SkipStaticMeshCleanMeshes.Empty();
			SkipStaticMeshCleanPackages.Empty();
			CheckStaticMeshCleanClasses.Empty();

			const TCHAR* SMCleanSkipIniSection = TEXT("Cooker.CleanStaticMeshMtrlSkip");
			FConfigSection* SMCleanSkipIniList = GConfig->GetSectionPrivate(SMCleanSkipIniSection, FALSE, TRUE, GEditorIni);
			if (SMCleanSkipIniList)
			{
				// Fill in the classes, packages, and static mesh lists to skip cleaning
				for (FConfigSectionMap::TIterator It(*SMCleanSkipIniList); It; ++It)
				{
					const FName TypeString = It.Key();
					FString ValueString = It.Value();

					if (TypeString == TEXT("Class"))
					{
						// Add the class name to the list
						CheckStaticMeshCleanClasses.Set(ValueString, TRUE);
						warnf(NAME_Log, TEXT("StaticMesh CleanUp: Adding Class      : %s"), *ValueString);
					}
					else if (TypeString == TEXT("StaticMesh"))
					{
						// Add the mesh name to the list
						SkipStaticMeshCleanMeshes.Set(ValueString, TRUE);
						warnf(NAME_Log, TEXT("StaticMesh CleanUp: Adding StaticMesh : %s"), *ValueString);
					}
					else if (TypeString == TEXT("Package"))
					{
						// Add the package name to the list
						SkipStaticMeshCleanPackages.Set(ValueString, TRUE);
						warnf(NAME_Log, TEXT("StaticMesh CleanUp: Adding Package    : %s"), *ValueString);
					}
				}
			}

			// Collect garbage to make sure the file gets closed.
			UObject::CollectGarbage(RF_Native);
		}
	}
}

/**
 *	Get the extension of a cooked package on the given platform
 *
 *	@param	Platform		The platform of interest
 *
 *	@return	FString			The extension
 */
FString UCookPackagesCommandlet::GetCookedPackageExtension(UE3::EPlatformType Platform)
{
	if (Platform&PLATFORM_Console)
	{
		return TEXT(".xxx");
	}
	else
	{
		return TEXT(".upk");
	}
}

/**
 * Get the destination filename for the given source package file based on platform
 *
 * @param SrcFilename - source package file path
 * @return cooked filename destination path
 */
FFilename UCookPackagesCommandlet::GetCookedPackageFilename( const FFilename& SrcFilename )
{
	// Dest pathname depends on platform
	FFilename			DstFilename;
	if( GIsCookingForDemo )
	{
		DstFilename = CookedDir + SrcFilename.GetCleanFilename();
	}
	else if( Platform == PLATFORM_Windows || Platform == PLATFORM_MacOSX )
	{
		// get top level directory under appGameDir of the source (..\ExampleGame\) or ..\Engine
		FFilename AfterRootDir;
		// look for packages in engine
		FString RootDir = appEngineDir();
		if (SrcFilename.StartsWith(RootDir))
		{
			AfterRootDir = SrcFilename.Right(SrcFilename.Len() - RootDir.Len());
		}
		// if it's not in Engine, it needs to be in the appGameDir
		else
		{
			RootDir = appGameDir();
			check(SrcFilename.StartsWith(RootDir));
			AfterRootDir = SrcFilename.Right(SrcFilename.Len() - RootDir.Len());

			// we need to chop off two more directories if we are cooking out of the CutdownPackages
			// if we are cooking the direct output of a WrangleContent commandlet run, then we need to chop off a couple more directories
			if (AfterRootDir.StartsWith(TEXT("CutdownPackages")))
			{
				// chop 1
				AfterRootDir = AfterRootDir.Right(AfterRootDir.Len() - (AfterRootDir.InStr(PATH_SEPARATOR) + 1));
				// chop 2
				AfterRootDir = AfterRootDir.Right(AfterRootDir.Len() - (AfterRootDir.InStr(PATH_SEPARATOR) + 1));
			}
		}
		// now skip over the first directory after which ever root directory it is (ie Content)
		FFilename AfterTopLevelDir = AfterRootDir.Right(AfterRootDir.Len() - AfterRootDir.InStr(PATH_SEPARATOR));
		DstFilename = CookedDir + AfterTopLevelDir;
	}
	else
	{
		DstFilename = CookedDir + SrcFilename.GetBaseFilename() + GetCookedPackageExtension(Platform);
	}
	return DstFilename;
}



/**
 * Check if any dependencies of a seekfree package are newer than the cooked seekfree package.
 * If they are, the package needs to be recooked.
 *
 * @param SrcLinker Optional source package linker to check dependencies for when no src file is available
 * @param SrcFilename Name of the source of the seekfree package
 * @param DstTimestamp Timestamp of the cooked version of this package
 *
 * @return TRUE if any dependencies are newer, meaning to recook the package
 */
UBOOL UCookPackagesCommandlet::AreSeekfreeDependenciesNewer(ULinkerLoad* SrcLinker, const FString& SrcFilename, DOUBLE DstTimestamp)
{
	SCOPE_SECONDS_COUNTER_COOK(CheckDependentPackagesTime);

	// use source package linker if available
	ULinkerLoad* Linker = SrcLinker;
	// open the linker of the source package (this is pretty fast)
	if( !Linker )
	{	
		UObject::BeginLoad();
		Linker = UObject::GetPackageLinker(NULL, *SrcFilename, LOAD_NoVerify, NULL, NULL);
		UObject::EndLoad();
	}

	// gather all the unique dependencies for all the objects in the package
	TSet<FDependencyRef> ObjectDependencies;
	for (INT ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ExportIndex++)
	{
		Linker->GatherExportDependencies(ExportIndex, ObjectDependencies);
	}

	// now figure out just the packages from the list
	TSet<FName> DependentPackages;
	for( TSet<FDependencyRef>::TConstIterator It(ObjectDependencies); It; ++It )
	{
		const FDependencyRef& Ref = *It;
		DependentPackages.Add(Ref.Linker->LinkerRoot->GetFName());
	}

	UBOOL bHasNewerDependentPackage = FALSE;
	// look for any newer dependent packages
	for( TSet<FName>::TConstIterator It(DependentPackages); It; ++It )
	{
		const FName& PackageName = *It;
		FString Path;
		if (GPackageFileCache->FindPackageFile(*PackageName.ToString(), NULL, Path))
		{
			DOUBLE Timestamp = GFileManager->GetFileTimestamp(*Path);

			if (Timestamp > DstTimestamp)
			{
				bHasNewerDependentPackage = TRUE;
				break;
			}
		}
	}

	// return if there were any newer dependent packages
	return bHasNewerDependentPackage;
}

/**
 * Generates list of src/ dst filename mappings of packages that need to be cooked after taking the command
 * line options into account.
 *
 * @param [out] FirstStartupIndex		index of first startup package in returned array, untouched if there are none
 * @param [out]	FirstScriptIndex		index of first script package in returned array, untouched if there are none
 * @param [out] FirstGameScriptIndex	index of first game script package in returned array, untouched if there are none
 * @param [out] FirstMapIndex			index of first map package in returned array, untouched if there are none
 *
 * @return	array of src/ dst filename mappings for packages that need to be cooked
 */
TArray<FPackageCookerInfo> UCookPackagesCommandlet::GeneratePackageList( INT& FirstStartupIndex, INT& FirstScriptIndex, INT& FirstGameScriptIndex, INT& FirstMapIndex )
{
	// Split into two to allow easy sorting via array concatenation at the end.
	TArray<FPackageCookerInfo>	NotRequiredFilenamePairs;
	TArray<FPackageCookerInfo>	MapFilenamePairs;
	TArray<FPackageCookerInfo>	ScriptFilenamePairs;
	TArray<FPackageCookerInfo>	StartupFilenamePairs;
	TArray<FPackageCookerInfo>	StandaloneSeekfreeFilenamePairs;
	// unused (here for helper function)
	TArray<FPackageCookerInfo>	RegularFilenamePairs;


	// Get a list of all script package names split by type, excluding editor- only ones.
	TArray<FString>				EngineOnlyNativeScriptPackageNames;
	TArray<FString>				NativeScriptPackageNames;
	TArray<FString>				NonNativeScriptPackageNames;
	// get just the engine native so we can calculate the first game script
	appGetScriptPackageNames(EngineOnlyNativeScriptPackageNames, SPT_EngineNative, PlatformEngineConfigFilename);
	// we want to cook native editor packages when cooking for Windows
	appGetScriptPackageNames(NativeScriptPackageNames, SPT_Native | (Platform == PLATFORM_Windows ? SPT_Editor : 0), PlatformEngineConfigFilename);
	appGetScriptPackageNames(NonNativeScriptPackageNames, SPT_NonNative, PlatformEngineConfigFilename);

	// get all the startup packages that the runtime will use (so we need to pass it the platform-specific engine config name)
	TArray<FString>				AllStartupPackageNames;
	GConfig->GetArray(TEXT("Engine.StartupPackages"), TEXT("Package"), AllStartupPackageNames, PlatformEngineConfigFilename);

	if (AllowDebugViewmodes(ShaderPlatformFromUE3Platform(Platform)) && !AllStartupPackageNames.ContainsItem(TEXT("EngineDebugMaterials")))
	{
		appMsgf(AMT_OK, TEXT("Debug viewmodes are being allowed (bAllowDebugViewmodesOnConsoles=true in the engine ini) but EngineDebugMaterials was not in the startup packages list!  ")
			TEXT("Materials required for debug viewmodes will not load."));
	}
	else if (!AllowDebugViewmodes(ShaderPlatformFromUE3Platform(Platform)) && AllStartupPackageNames.ContainsItem(TEXT("EngineDebugMaterials")))
	{
		appMsgf(AMT_OK, TEXT("Debug viewmodes are not being allowed (bAllowDebugViewmodesOnConsoles=false in the engine ini) but EngineDebugMaterials was in the startup packages list!  ")
			TEXT("Materials required for debug viewmodes will be cooked and take up memory even though they can't be used."));
	}

	// get a list of the seekfree, always cook maps, but don't reload them
	TArray<FString>				SeekFreeAlwaysCookMaps;
	LoadSectionPackages(TEXT("Engine.PackagesToAlwaysCook"), SeekFreeAlwaysCookMaps, TEXT("SeekFreePackage"), TRUE);
	
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();

	// Get a list of known language extensions
	const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();

#if WITH_GFx
	if( (Platform & PLATFORM_Mobile ) == 0 )
	{
		// Add the GFx packages to the SeekFreeAlwaysCookMaps list
		//@todo. Make sure this is okay w/ the scaleform folks
		GFx_AddExtraPackages(RequiredPackages);
	}
#endif // WITH_GFx

	// if we are cooking a language other than default, always cook the script and startup packages, because 
	// dependency checking may not catch that it needs to regenerate the _LOC_LANG files since the base package
	// was already cooked
	UBOOL bForceCookNativeScript = bForceRecookSeekfree || (appStricmp(UObject::GetLanguage(), TEXT("INT")) != 0);
	UBOOL bForceCookMaps = bForceRecookSeekfree || (appStricmp(UObject::GetLanguage(), TEXT("INT")) != 0);
	UBOOL bForceCookStartupPackage = bForceRecookSeekfree || (appStricmp(UObject::GetLanguage(), TEXT("INT")) != 0);
	UBOOL bShouldLogSkippedPackages = ParseParam( appCmdLine(), TEXT("LOGSKIPPEDPACKAGES") );


	// split the package iteration in two passes, one for normal packages, one for native script
	// this will allow bForceCookNativeScript to be set if needed before deciding what to do with 
	// the native script packages
	for (INT Pass = 0; Pass < 2; Pass++)
	{
		for( INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
		{
			FFilename SrcFilename = PackageList(PackageIndex);
			FFilename DstFilename = GetCookedPackageFilename(SrcFilename);
			// We store cooked packages in a folder that is outside the Package search path and use an "unregistered" extension.
			FFilename BaseSrcFilename = SrcFilename.GetBaseFilename();	

			// Check whether we are autosave, PIE, a manual dependency or a map file.
			UBOOL	bIsShaderCacheFile		= FString(*SrcFilename).ToUpper().InStr( TEXT("SHADERCACHE") ) != INDEX_NONE;
			UBOOL	bIsAutoSave				= FString(*SrcFilename).ToUpper().InStr( TEXT("AUTOSAVES") ) != INDEX_NONE;
			UBOOL	bIsPIE					= bIsAutoSave && BaseSrcFilename.StartsWith( PLAYWORLD_CONSOLE_BASE_PACKAGE_PREFIX );
			UBOOL	bIsADependency			= PackageDependencies.Find( SrcFilename ) != NULL;
			UBOOL	bIsNativeScriptFile		= NativeScriptPackageNames.FindItemIndex( BaseSrcFilename ) != INDEX_NONE;
			UBOOL	bIsCombinedStartupPackage = AllStartupPackageNames.FindItemIndex( BaseSrcFilename ) != INDEX_NONE;
			// if we put a non-native script package into the startup packages list, pretend it's not a script package at all
			UBOOL	bIsNonNativeScriptFile	= !bIsCombinedStartupPackage && NonNativeScriptPackageNames.FindItemIndex( BaseSrcFilename ) != INDEX_NONE;
			UBOOL	bIsTokenMap				= TokenMaps.ContainsItem(SrcFilename);
			UBOOL	bIsMapFile				= bIsTokenMap || (SrcFilename.GetExtension() == FURL::DefaultMapExt) || (BaseSrcFilename == TEXT("entry"));
			UBOOL   bIsScriptFile           = bIsNativeScriptFile || bIsNonNativeScriptFile;
			UBOOL	bIsStandaloneSeekfree	= SeekFreeAlwaysCookMaps.FindItemIndex(BaseSrcFilename) != INDEX_NONE;
			UBOOL	bIsSeekfree				= bIsMapFile || bIsNativeScriptFile || bIsCombinedStartupPackage || bIsStandaloneSeekfree;

			// only do maps on second pass (and process startup packages/native script in both passes)
			if (Pass == (bIsMapFile ? 0 : 1) && !bIsCombinedStartupPackage && !bIsNativeScriptFile)
			{
				continue;
			}

			UBOOL	bIsTokenScriptPackage	= TokenScriptPackages.ContainsItem(SrcFilename);
			UBOOL	bIsTokenContentPackage	= TokenContentPackages.ContainsItem(SrcFilename);
			UBOOL	bIsCommandLinePackage	= bIsTokenMap || bIsTokenScriptPackage || bIsTokenContentPackage;

			// Compare file times.
			DOUBLE	DstFileTime;
			if (bIsStandaloneSeekfree)
			{
				// form the seekfree package name...
				FString Ext = FString(".") + DstFilename.GetExtension();
				DstFileTime = GFileManager->GetFileTimestamp(*DstFilename.Replace(*Ext, *(FString(STANDALONE_SEEKFREE_SUFFIX) + Ext)));
			}
			else
			{
				DstFileTime = bIsSeekfree ? GFileManager->GetFileTimestamp(*DstFilename) : PersistentCookerData->GetFileTime(*DstFilename);
			}
			DOUBLE	SrcFileTime				= GFileManager->GetFileTimestamp( *SrcFilename );
			UBOOL	DstFileExists			= GFileManager->FileSize( *DstFilename ) > 0;
			UBOOL	DstFileNewer			= DstFileExists && (DstFileTime >= 0) && (DstFileTime >= SrcFileTime);// && !bIsScriptFile;

			// It would be nice if we could just rely on the map extension to be set but in theory we would have to load up the package
			// and see whether it contained a UWorld object to be sure. Checking the map extension with a FindObject should give us
			// very good coverage and should always work in the case of dependency cooking. In theory we could also use LoadObject albeit
			// at the cost of having to create linkers for all existing packages.
			if( FindObject<UWorld>( NULL, *FString::Printf(TEXT("%s.TheWorld"),*BaseSrcFilename) ) )
			{
				bIsMapFile = TRUE;
			}
			// Special case handling for Entry.upk.
			if( BaseSrcFilename == TEXT("Entry") )
			{
				bIsMapFile = TRUE;
				bIsTokenMap = bIsTokenMap || bIsTokenContentPackage;
			}

			// Skip over shader caches.
			if( bIsShaderCacheFile )
			{
				if( bShouldLogSkippedPackages )
				{
					warnf(NAME_Log, TEXT("Skipping shader cache %s"), *SrcFilename);
				}
				continue;
			}
			// Skip over packages we don't depend on if we're only cooking dependencies .
			else if( bOnlyCookDependencies && !bIsADependency )
			{
				if( bShouldLogSkippedPackages )
				{
					warnf(NAME_Log, TEXT("Skipping %s"), *SrcFilename);
				}
				continue;
			}
			// Skip cooking maps if wanted.
			else if( bSkipCookingMaps && bIsMapFile )
			{
				if( bShouldLogSkippedPackages )
				{
					warnf(NAME_Log, TEXT("Skipping %s"), *SrcFilename);
				}
				continue;
			}
			else if ( !bIsMapFile && bCookMapsOnly)
			{
				continue;
			}
			// Only cook maps on the commandline
			else if( bIsMapFile && !bIsTokenMap )
			{
				if( bShouldLogSkippedPackages )
				{
					warnf(NAME_Log, TEXT("Skipping %s"), *SrcFilename);
				}
				continue;
			}
			// Skip over autosaves, unless it is a PlayInEditor file.
			else if( bIsAutoSave && !bIsPIE )
			{
				if( bShouldLogSkippedPackages )
				{
					warnf(NAME_Log, TEXT("Disregarding %s"), *SrcFilename);
				}
				continue;
			}
			// Skip over packages not needed for users
			else if ( DLCName.Len() )
			{
				// skip over any startup (which prolly haven't been loaded, but just in case), and script files
				// @todo: need to handle a command-line specified script package for code-only mods!
				if ( bIsCombinedStartupPackage || bIsScriptFile)
				{
					continue;
				}
			}


			// look for a language extension, _XXX
			INT Underscore = BaseSrcFilename.InStr(TEXT("_"), TRUE);
			UBOOL bIsWrongLanguage = FALSE;
			if (Underscore != INDEX_NONE)
			{
				FString PostUnderscore = BaseSrcFilename.Right((BaseSrcFilename.Len() - Underscore) - 1);
				// if there is an underscore, is what follows a language extension?
				for (INT LangIndex = 0; LangIndex < KnownLanguageExtensions.Num(); LangIndex++)
				{
					// is the extension a language extension?
					if (PostUnderscore == KnownLanguageExtensions(LangIndex))
					{
						// if it's not the language 
						if (MultilanguageCookingMask)
						{
							if (!(MultilanguageCookingMask & (1 << LangIndex)))
							{
								bIsWrongLanguage = TRUE;
							}
						}
						else if (PostUnderscore != UObject::GetLanguage())
						{
							bIsWrongLanguage = TRUE;
						}

						// found a language extension, we can stop looking
						break;
					}
				}
			}

			// don't cook if it's the wrong language
			if (bIsWrongLanguage)
			{
				if( bShouldLogSkippedPackages )
				{
					warnf(NAME_Log, TEXT("Skipping wrong language file %s"), *SrcFilename);
				}
				continue;
			}


			// check to see if the existing cooked file is an older version, which will need recooking
			UBOOL bCookedVersionIsOutDated = FALSE;
			if( DstFileExists == TRUE )
			{
				INT CookedVersion = PersistentCookerData->GetFileCookedVersion(*DstFilename);
				
				// if it's cooked with a different version, recook!
				if (CookedVersion != GPackageFileCookedContentVersion)
				{
					bCookedVersionIsOutDated = TRUE;
				}
			}



			// Skip over unchanged files (unless we specify ignoring it for e.g. maps or script).
			if(		( DstFileNewer == TRUE ) 
				&&	( bCookedVersionIsOutDated == FALSE ) 
				&&	( !bIsMapFile || !bForceCookMaps )
				&&	( !bIsNativeScriptFile || !bForceCookNativeScript ) 
				&&	( !bIsStandaloneSeekfree || !bForceRecookSeekfree )
				&&	( !bIsCombinedStartupPackage ) )
			{
				// check if any of the dependent packages of a seekfree packages are out to date (otherwise we need to recook the seekfree package)
				UBOOL bIsSeekfreeFileDependencyNewer = FALSE;
				if (bIsSeekfree && DstFileExists)
				{
					FFilename ActualDstName = DstFilename;
					// we need to check the _SF version for standalone seekfree packages
					if (bIsStandaloneSeekfree)
					{
						ActualDstName = DstFilename.GetBaseFilename(FALSE) + STANDALONE_SEEKFREE_SUFFIX + FString(TEXT(".")) + DstFilename.GetExtension();
					}

					// check dependencies against the cooked package
					DOUBLE Time = GFileManager->GetFileTimestamp(*ActualDstName);
					bIsSeekfreeFileDependencyNewer = AreSeekfreeDependenciesNewer(NULL, SrcFilename, Time);
				}

				// if there weren't any seekfree dependencies newer, then we are up to date
				if (!bIsSeekfreeFileDependencyNewer)
				{
					if( bShouldLogSkippedPackages )
					{
						warnf(NAME_Log, TEXT("UpToDate %s"), *SrcFilename);
					}
					continue;
				}
			}
						
			// Skip over any cooked files residing in cooked folder.
			if( SrcFilename.InStr( GetCookedDirectory() ) != INDEX_NONE )
			{
				if( bShouldLogSkippedPackages )
				{
					warnf(NAME_Log, TEXT("Skipping %s"), *SrcFilename);
				}
				continue;
			}		

			// Determine which container to add the item to so it can be cooked correctly

			// Package is definitely a map file.
			if( bIsMapFile || bIsTokenMap )
			{
				MapFilenamePairs.AddItem( FPackageCookerInfo( *SrcFilename, *DstFilename, TRUE, FALSE, FALSE, FALSE, FALSE ) );
			}
			// Package is a script file.
			else if( bIsNativeScriptFile )
			{
				// in the first pass, if a native script was marked as needing to be cooked, cook all
				// native script packages, otherwise some subtle import/forcedexport issues can arise
				if (Pass == 0)
				{
					bForceCookNativeScript = TRUE;
				}
				else
				{
					ScriptFilenamePairs.AddItem( FPackageCookerInfo( *SrcFilename, *DstFilename, TRUE, TRUE, FALSE, FALSE, FALSE ) );
				}
			}
			// Package is a non-native script file
			else if (bIsNonNativeScriptFile && (GCookingTarget == PLATFORM_Windows))
			{
				ScriptFilenamePairs.AddItem( FPackageCookerInfo( *SrcFilename, *DstFilename, FALSE, FALSE, FALSE, FALSE, TRUE ) );
			}
			// Package is a combined startup package
			else if ( bIsCombinedStartupPackage )
			{
				// first pass, just check if any packages are out of date
				if (Pass == 0)
				{
					FString StartupPackageName = FString::Printf(TEXT("%sStartup.%s"), *GetCookedDirectory(), (Platform == PLATFORM_Windows ? TEXT("upk") : TEXT("xxx")));
					DOUBLE	StartupFileTime				= GFileManager->GetFileTimestamp(*StartupPackageName);
					UBOOL	StartupFileExists			= GFileManager->FileSize(*StartupPackageName) > 0;
					UBOOL	StartupFileNewer			= (StartupFileTime >= 0) && (StartupFileTime >= SrcFileTime);

					// is the startup file out of date?
					if (!StartupFileExists || !StartupFileNewer)
					{
						bForceCookStartupPackage = TRUE;
					}

					// also add it to the 'not required' list (if it needs cooking) so that the texture mips are saved out (with no other cooking going on)
					if (!DstFileNewer || bCookedVersionIsOutDated)
					{
						NotRequiredFilenamePairs.AddItem( FPackageCookerInfo( *SrcFilename, *DstFilename, FALSE, FALSE, FALSE, FALSE, FALSE ) );
					}
				}
				// on second pass, cook all startup files into the combined startup package if needed (as determined on first pass)
				else
				{
					// always add to the startup list, we'll decide below if we need it or not
					StartupFilenamePairs.AddItem( FPackageCookerInfo( *SrcFilename, *DstFilename, TRUE, TRUE, TRUE, FALSE, FALSE ) );
				}
			}
			else if ( (!DLCName.Len() && bIsStandaloneSeekfree )
				// In user mode, also considered to be standalone-seekfree are
				//   1) mod script (non-native by definition); and
				//   2) non-map content packages.
				|| (DLCName.Len() && (bIsTokenScriptPackage || bIsTokenContentPackage)) )
			{
				StandaloneSeekfreeFilenamePairs.AddItem( FPackageCookerInfo( *SrcFilename, *DstFilename, TRUE, FALSE, FALSE, TRUE, FALSE ) );

				// also add it to the 'not required' list (if it needs cooking) so that the texture mips are saved out (with no other cooking going on)
				if (!DstFileNewer || bCookedVersionIsOutDated)
				{
					NotRequiredFilenamePairs.AddItem( FPackageCookerInfo( *SrcFilename, *DstFilename, FALSE, FALSE, FALSE, FALSE, FALSE ) );
				}
			}
#if WITH_GFx
            // Don't skip fontlib packages
            else if( RequiredPackages.FindItemIndex( SrcFilename.GetBaseFilename().ToUpper() ) != INDEX_NONE )
            {
                NotRequiredFilenamePairs.AddItem( FPackageCookerInfo( *SrcFilename, *DstFilename, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE ) );
            }
#endif
			// Package is not required and can be stripped of everything but texture miplevels.
			else
			{
				NotRequiredFilenamePairs.AddItem( FPackageCookerInfo( *SrcFilename, *DstFilename, FALSE, FALSE, FALSE, FALSE, FALSE ) );
			}
		}

		// if any of the non-seekfree packages needed cooking, then force cook the startup
		if (NotRequiredFilenamePairs.Num() && !bUseTextureFileCache)
		{
			bForceCookNativeScript = TRUE;
			bForceCookStartupPackage = TRUE;
		}
	}

	// 'Game' cooker helper call to GeneratePackageList
	if (GGameCookerHelper)
	{
		if (GGameCookerHelper->GeneratePackageList(
				this, 
				Platform, 
				ShaderPlatform, 
				NotRequiredFilenamePairs, 
				RegularFilenamePairs, 
				MapFilenamePairs, 
				ScriptFilenamePairs, 
				StartupFilenamePairs, 
				StandaloneSeekfreeFilenamePairs) == FALSE)
		{
			// Failed for some reason... handle it.
		}
	}

	// Sort regular files, script and finally maps to be last.
	TArray<FPackageCookerInfo> SortedFilenamePairs;
	
	// Always cooked regular (non script, non map) packages first.
	SortedFilenamePairs += NotRequiredFilenamePairs;

	// Followed by script packages
	FirstScriptIndex		= SortedFilenamePairs.Num();
	if( ScriptFilenamePairs.Num() )
	{
		FirstGameScriptIndex	= FirstScriptIndex + EngineOnlyNativeScriptPackageNames.Num();
		// Add in order of appearance in EditPackages and not layout on file determined by filename.
		TArray<FString> ScriptPackageNames;
		ScriptPackageNames += NativeScriptPackageNames;

		if (GCookingTarget == PLATFORM_Windows)
		{
			for (INT GameScriptIndex = 0; GameScriptIndex < NonNativeScriptPackageNames.Num(); GameScriptIndex++)
			{
				ScriptPackageNames.AddUniqueItem(NonNativeScriptPackageNames(GameScriptIndex));
			}
		}
		for( INT NameIndex=0; NameIndex<ScriptPackageNames.Num(); NameIndex++ )
		{
			for( INT PairIndex=0; PairIndex<ScriptFilenamePairs.Num(); PairIndex++ )
			{
				if( ScriptPackageNames(NameIndex) == ScriptFilenamePairs(PairIndex).SrcFilename.GetBaseFilename() )
				{
					SortedFilenamePairs.AddItem( ScriptFilenamePairs(PairIndex) );
					break;
				}
			}
		}
	}

	// Then cook startup packages
	FirstStartupIndex = SortedFilenamePairs.Num();

	// check if anything will be getting cooked after the startup packages
	UBOOL bArePostStartupPackagesCooked = StandaloneSeekfreeFilenamePairs.Num() != 0 || MapFilenamePairs.Num() != 0;

	// we only need to process the startup packages if there are packages that will be cooked after this or we 
	// the package needs cooking. if it doens't need cooking, we still need to load the packages if stuff
	// after it will be cooked (for proper MarkedByCooker flags)
	if (bForceCookStartupPackage || bArePostStartupPackagesCooked)
	{
		// if we didn't need to actually cook the startup package, mark the packages to be loaded and not cooked
		if (!bForceCookStartupPackage)
		{
			for (INT PackageIndex = 0; PackageIndex < StartupFilenamePairs.Num(); PackageIndex++)
			{
				StartupFilenamePairs(PackageIndex).bShouldOnlyLoad = TRUE;
			}
		}

		// add the startup packages to the list of packages
		SortedFilenamePairs += StartupFilenamePairs;

	}
	SortedFilenamePairs += StandaloneSeekfreeFilenamePairs;

	// Now append maps.
	if( MapFilenamePairs.Num() )
	{
		FirstMapIndex = SortedFilenamePairs.Num();
		SortedFilenamePairs += MapFilenamePairs;
	}

	return SortedFilenamePairs;
}

/**
 * Cleans up DLL handles and destroys cookers
 */
void UCookPackagesCommandlet::Cleanup()
{
	// Reset the global shader precompiler pointer to NULL.
	if( GConsoleShaderPrecompilers[ShaderPlatform] )
	{
		check(GConsoleShaderPrecompilers[ShaderPlatform] == ConsoleSupport->GetGlobalShaderPrecompiler());
		GConsoleShaderPrecompilers[ShaderPlatform] = NULL;
	}

#if WITH_SUBSTANCE_AIR == 1
	GGameCookerHelper->Cleanup();
#endif
}

/**
 * Handles duplicating cubemap faces that are about to be saved with the passed in package.
 *
 * @param	Package	 Package for which cubemaps that are going to be saved with it need to be handled.
 */
void UCookPackagesCommandlet::HandleCubemaps( UPackage* Package )
{
#if 0 //@todo cooking: this breaks regular textures being used for cubemap faces
	// Create dummy, non serialized package old cubemap faces get renamed into.
	UPackage* OldCubemapFacesRenamedByCooker = CreatePackage( NULL, TEXT("OldCubemapFacesRenamedByCooker") );
#endif
	// Duplicate textures used by cubemaps.
	for( TObjectIterator<UTextureCube> It; It; ++It )
	{
		UTextureCube* Cubemap = *It;
		// Valid cubemap (square textures of same size and format assigned to all faces)
		if (Cubemap->bIsCubemapValid)
		{
			// Only duplicate cubemap faces saved into this package.
			if( !Cubemap->HasAnyFlags(RF_MarkedByCooker) && (Cubemap->IsIn( Package ) || Cubemap->HasAnyFlags( RF_ForceTagExp )) )
			{
				for( INT FaceIndex=0; FaceIndex<6; FaceIndex++ )
				{
					UTexture2D* OldFaceTexture	= Cubemap->GetFace(FaceIndex);
					// Only duplicate once.
					if( OldFaceTexture && OldFaceTexture->GetOuter() != Cubemap )
					{
						// Backup the source face textures bulkdata flags and clear them so that the duplicated face // textures will have clean bulkdata flags.
						TArray<DWORD> BackUpBulkDataFlags;
						BackUpBulkDataFlags.AddZeroed(OldFaceTexture->Mips.Num());

						for (INT i=0; i<OldFaceTexture->Mips.Num(); i++)
						{
							BackUpBulkDataFlags(i) = OldFaceTexture->Mips(i).Data.GetBulkDataFlags();
							OldFaceTexture->Mips(i).Data.ClearBulkDataFlags(BackUpBulkDataFlags(i));
						}

						// Duplicate cubemap faces so every single texture only gets loaded once.
						Cubemap->SetFace(
							FaceIndex,
							CastChecked<UTexture2D>( UObject::StaticDuplicateObject( 
							OldFaceTexture, 
							OldFaceTexture, 
							Cubemap, 
							*FString::Printf(TEXT("CubemapFace%i"),FaceIndex)
									) )
								);

						// Restore the bulkdata flags for the source face textures.
						for (INT i=0; i<OldFaceTexture->Mips.Num(); i++)
						{
							OldFaceTexture->Mips(i).Data.SetBulkDataFlags(BackUpBulkDataFlags(i));
						}

#if 0 //@todo cooking: this breaks regular textures being used for cubemap faces
						// Rename old cubemap faces into non- serialized package and make sure they are never put into
						// a seekfree package by clearing flags and marking them.
						OldFaceTexture->Rename( NULL, OldCubemapFacesRenamedByCooker, REN_ForceNoResetLoaders );
						OldFaceTexture->ClearFlags( RF_ForceTagExp );
						OldFaceTexture->SetFlags( RF_MarkedByCooker );
#endif
					}
				}
			}
		}
		// Cubemap is invalid!
		else
		{
			// No need to reference texture from invalid cubemaps.
			for( INT FaceIndex=0; FaceIndex<6; FaceIndex++ )
			{
				Cubemap->SetFace(FaceIndex,NULL);
			}
		}
	}
}

/**
 * Collects garbage and verifies all maps have been garbage collected.
 */
void UCookPackagesCommandlet::CollectGarbageAndVerify()
{
	SCOPE_SECONDS_COUNTER_COOK(CollectGarbageAndVerifyTime);

	// Clear all components as quick sanity check to ensure GC can remove everything.
	GWorld->ClearComponents();

	// Collect garbage up-front to ensure that only required objects will be put into the seekfree package.
	UObject::CollectGarbage(RF_Native);

	// At this point the only world still around should be GWorld, which is the one created in CreateNew.
	for( TObjectIterator<UWorld> It; It; ++It )
	{
		UWorld* World = *It;
		if( World != GWorld )
		{
			UObject::StaticExec(*FString::Printf(TEXT("OBJ REFS CLASS=WORLD NAME=%s.TheWorld"), *World->GetOutermost()->GetName()));

			TMap<UObject*,UProperty*>	Route		= FArchiveTraceRoute::FindShortestRootPath( World, TRUE, GARBAGE_COLLECTION_KEEPFLAGS );
			FString						ErrorString	= FArchiveTraceRoute::PrintRootPath( Route, World );
			debugf(TEXT("%s"),*ErrorString);		
		
			// We cannot safely recover from this.
			appErrorf( TEXT("%s not cleaned up by garbage collection!") LINE_TERMINATOR TEXT("%s"), *World->GetFullName(), *ErrorString );
		}
	}
}

/**
* Adds the mip data payload for the given texture and mip index to the texture file cache.
* If an entry exists it will try to replace it if the mip is <= the existing entry or
* the mip data will be appended to the end of the TFC file.
* Also updates the bulk data entry for the texture mip with the saved size/offset.
*
* @param Package - Package for texture that is going to be saved
* @param Texture - 2D texture with mips to be saved
* @param MipIndex - index of mip entry in the texture that needs to be saved
* @param TextureFormat - which format of compressed texture to use
*/
void UCookPackagesCommandlet::SaveMipToTextureFileCache( UPackage* Package, UTexture2D* Texture, INT MipIndex, ETextureFormatSupport TextureFormat )
{
	check(bUseTextureFileCache);	

	// Allocate temporary scratch buffer for alignment and zero it out.
	static void* Zeroes = NULL;
	if( !Zeroes )
	{	
		Zeroes = appMalloc( TextureFileCacheBulkDataAlignment );
		appMemzero( Zeroes, TextureFileCacheBulkDataAlignment );
	}

	// Access the texture cache archive for this texture.
	FName TextureFileCache = Texture->TextureFileCacheName;
	
	// Fixup for format extension
	if (Platform == PLATFORM_Android && TextureFileCache != NAME_None)
	{
		FString TextureFileCacheName = TextureFileCache.GetNameString();
		TextureFileCacheName += TEXT("_");
		TextureFileCacheName += GetAndroidTextureFormatName(TextureFormat);
		TextureFileCache = FName(*TextureFileCacheName);
	}

	if (bIsMTChild && TextureFileCache != NAME_None)
	{
		FString TextureFileCacheName = GetTextureCacheName(Texture);

		if (Platform == PLATFORM_Android)
		{
			TextureFileCacheName += TEXT("_");
			TextureFileCacheName += GetAndroidTextureFormatName(TextureFormat);
		}

		TextureFileCache = FName(*TextureFileCacheName); // we want the appendage version here
		check(TextureFileCache.GetNumber() > 0);
	}
	FArchive* TextureCacheArchive = GetTextureCacheArchive(TextureFileCache);

	check(Texture);

	// miplevel to save into texture cache file.
	FTexture2DMipMap* Mip;
	
	if (TextureFormat & TEXSUPPORT_PVRTC)
	{
		check(Texture->CachedPVRTCMips.IsValidIndex(MipIndex));
		Mip = &Texture->CachedPVRTCMips(MipIndex);
	}
	else if (TextureFormat & TEXSUPPORT_ATITC)
	{
		check(Texture->CachedATITCMips.IsValidIndex(MipIndex));
		Mip = &Texture->CachedATITCMips(MipIndex);
	}
	else if (TextureFormat & TEXSUPPORT_ETC)
	{
		check(Texture->CachedETCMips.IsValidIndex(MipIndex));
		Mip = &Texture->CachedETCMips(MipIndex);
	}
	else // Default to Mips for DXT
	{
		check(Texture->Mips.IsValidIndex(MipIndex));
		Mip = &Texture->Mips(MipIndex);
	}
	
	// load bulk data before we modify its bulk data flags for saving 
	Mip->Data.MakeSureBulkDataIsLoaded();

	// Set flag to ensure that we're only saving the raw data to the archive. Required for size comparison.
	Mip->Data.SetBulkDataFlags( BULKDATA_StoreOnlyPayload );	
	// Allow actual payload to get serialized instead of just status info
	Mip->Data.ClearBulkDataFlags( BULKDATA_StoreInSeparateFile );
	// Enable compression on streaming mips
	Mip->Data.StoreCompressedOnDisk( GBaseCompressionMethod );

	// Create persistent memory writer that has byte swapping propagated and is using scratch memory.
	TArray<BYTE> ScratchMemory;
	FMemoryWriter MemoryWriter( ScratchMemory, TRUE );
	MemoryWriter.SetByteSwapping( ShouldByteSwapData() );

	// Serialize to memory writer, taking compression settings into account.
	Mip->Data.Serialize( MemoryWriter, Texture );
	check( ScratchMemory.Num() );

	// Retrieve bulk data info if its already cached.
	FString					BulkDataName = *FString::Printf(TEXT("MipLevel_%i"),MipIndex);
	FCookedBulkDataInfo*	BulkDataInfo;
	
	if (Platform != PLATFORM_Android)
	{
		BulkDataInfo = PersistentCookerData->GetBulkDataInfo( Texture, *BulkDataName );
	}
	else
	{
		BulkDataInfo = PersistentCookerData->GetBulkDataInfo( Texture, *(BulkDataName + GetAndroidTextureFormatName(TextureFormat)) );
	}

	// Texture mip is already part of file, see whether we can replace existing data.
	if ( BulkDataInfo )
	{
		INT OldAlignedBulkDataSize = Align(BulkDataInfo->SavedBulkDataSizeOnDisk, TextureFileCacheBulkDataAlignment);

		if (bIsMTChild || BulkDataInfo->TextureFileCacheName != Texture->TextureFileCacheName)
		// children can't possible save in some old block; old isn't possible
		{
			BulkDataInfo = NULL;
			// Make sure we are at the end of the file...
			INT CacheFileSize = TextureCacheArchive->TotalSize();
			TextureCacheArchive->Seek( CacheFileSize );
			PersistentCookerData->SetTextureFileCacheWaste( PersistentCookerData->GetTextureFileCacheWaste() + OldAlignedBulkDataSize );
		}
		else
		{
			// Only look at aligned sizes as that's what we write out to file.
			INT NewAlignedBulkDataSize = Align(ScratchMemory.Num(),TextureFileCacheBulkDataAlignment);

			// We can replace existing data if the aligned data is less than or equal to existing data.
			if( OldAlignedBulkDataSize >= NewAlignedBulkDataSize )
			{
				// Seek to position in file.
				TextureCacheArchive->Seek( BulkDataInfo->SavedBulkDataOffsetInFile );
				// If smaller, track waste...
				if (NewAlignedBulkDataSize < OldAlignedBulkDataSize)
				{
					PersistentCookerData->SetTextureFileCacheWaste( PersistentCookerData->GetTextureFileCacheWaste() + (OldAlignedBulkDataSize - NewAlignedBulkDataSize) );
				}
			}
			// Can't replace so we need to append.
			else
			{
				// Make sure we are at the end of the file...
				INT CacheFileSize = TextureCacheArchive->TotalSize();
				TextureCacheArchive->Seek( CacheFileSize );
				// Keep track of wasted memory in file.
				PersistentCookerData->SetTextureFileCacheWaste( PersistentCookerData->GetTextureFileCacheWaste() + OldAlignedBulkDataSize );
			}
		}
	}

	check( (TextureCacheArchive->Tell() % TextureFileCacheBulkDataAlignment == 0) );

	// Tell mip data where we are going to serialize data to, simulating serializing it again.
	Mip->Data.StoreInSeparateFile( 
		TRUE, 
		Mip->Data.GetSavedBulkDataFlags() & ~BULKDATA_StoreOnlyPayload,
		Mip->Data.GetElementCount(),
		TextureCacheArchive->Tell(),
		ScratchMemory.Num() );		
	// Serialize scratch data to disk at the proper offset.
	TextureCacheArchive->Serialize( ScratchMemory.GetData(), ScratchMemory.Num() );

	// Figure out position in file and ECC align it by zero padding.
	INT EndPos				= TextureCacheArchive->Tell();
	INT NumRequiredZeroes	= Align( EndPos, TextureFileCacheBulkDataAlignment ) - EndPos;

	// Serialize padding to file.
	if( NumRequiredZeroes )
	{
		TextureCacheArchive->Serialize( Zeroes, NumRequiredZeroes );
	}	
	check( TextureCacheArchive->Tell() % TextureFileCacheBulkDataAlignment == 0 );

	// Seek back to the end of the file.
	INT CurrentFileSize = TextureCacheArchive->TotalSize();
	checkf(CurrentFileSize >= 0, TEXT("TextureCacheArchive has an invalid size: %d"), CurrentFileSize);
	TextureCacheArchive->Seek( CurrentFileSize );

	// Remove temporary serialization flag again.
	Mip->Data.ClearBulkDataFlags( BULKDATA_StoreOnlyPayload );

	// Update the bulk data info for the saved mip entry
	if( !BulkDataInfo )
	{
		FCookedBulkDataInfo Info;
		
		if (Platform != PLATFORM_Android)
		{
			PersistentCookerData->SetBulkDataInfo( Texture, *FString::Printf(TEXT("MipLevel_%i"),MipIndex), Info );
			BulkDataInfo = PersistentCookerData->GetBulkDataInfo(Texture, *FString::Printf(TEXT("MipLevel_%i"),MipIndex));
		}
		else
		{
			PersistentCookerData->SetBulkDataInfo( Texture, *(FString::Printf(TEXT("MipLevel_%i"),MipIndex) + GetAndroidTextureFormatName(TextureFormat)), Info );
			BulkDataInfo = PersistentCookerData->GetBulkDataInfo(Texture, *(FString::Printf(TEXT("MipLevel_%i"),MipIndex) + GetAndroidTextureFormatName(TextureFormat)));
		}
	}

	BulkDataInfo->SavedBulkDataFlags		= Mip->Data.GetSavedBulkDataFlags();
	BulkDataInfo->SavedElementCount			= Mip->Data.GetSavedElementCount();
	BulkDataInfo->SavedBulkDataOffsetInFile	= Mip->Data.GetSavedBulkDataOffsetInFile();
	BulkDataInfo->SavedBulkDataSizeOnDisk	= Mip->Data.GetSavedBulkDataSizeOnDisk();
	BulkDataInfo->TextureFileCacheName		= TextureFileCache;

	CookStats.TFCTextureSaved += (QWORD)BulkDataInfo->SavedBulkDataSizeOnDisk;

	if (bWriteTFCReport)
	{
		FString OutputString;
		FString TexName;
		FString PackageName;
		Package->GetName(PackageName);
		Texture->GetName(TexName);
		const TCHAR* FormatString = UTexture2D::GetPixelFormatString((EPixelFormat)(Texture->Format));
		OutputString = FString::Printf(TEXT("%s,%s,%s,%s,%d,%s,%d,%d,%d\n"),*PackageName, *(Texture->TextureFileCacheName.GetNameString()),*TexName,*BulkDataName,BulkDataInfo->SavedBulkDataSizeOnDisk, FormatString, Mip->SizeX, Mip->SizeY, CurrentFileSize);
		FString DestFileName = FFilename(GetCookedDirectory()) + TEXT("DebugLogs\\") + TFCReportFileName.GetNameString();
		appAppendStringToFile(OutputString, *(DestFileName));
	}
		
	if (bVerifyTextureFileCache)
	{
		FTextureFileCacheEntry TempEntry;

		TempEntry.TextureName = Texture->GetPathName();
		TempEntry.MipIndex = MipIndex;
		TempEntry.OffsetInCache = BulkDataInfo->SavedBulkDataOffsetInFile;
		TempEntry.SizeInCache = BulkDataInfo->SavedBulkDataSizeOnDisk;
		TempEntry.FlagsInCache = BulkDataInfo->SavedBulkDataFlags;
		TempEntry.ElementCountInCache = BulkDataInfo->SavedElementCount;
		if (TextureFileCache == CharacterTFCName)
		{
			TempEntry.TFCType = TFCT_Character;
		}
		else if (Texture->TextureFileCacheName == LightingTFCName)
		{
			TempEntry.TFCType = TFCT_Lighting;
		}
		else
		{
			TempEntry.TFCType = TFCT_Base;
		}

		if (TextureFileCache != NAME_None)
		{
			FTextureFileCacheEntry* OverlapEntry = FindTFCEntryOverlap(TempEntry);
			if (OverlapEntry)
			{
				warnf(NAME_Warning, TEXT("SaveMipToTextureFileCache> Texture overlap found in TFC!"));
				warnf(NAME_Warning, TEXT("\tMip %d of %s"), TempEntry.MipIndex, *(TempEntry.TextureName));
				warnf(NAME_Warning, TEXT("\t\tOffset = %10d, Size = %d"), TempEntry.OffsetInCache, TempEntry.SizeInCache);
				warnf(NAME_Warning, TEXT("\tMip %d of %s"), OverlapEntry->MipIndex, *(OverlapEntry->TextureName));
				warnf(NAME_Warning, TEXT("\t\tOffset = %10d, Size = %d"), OverlapEntry->OffsetInCache, OverlapEntry->SizeInCache);
			}
		}
	}
}

/**
 * Destructor. Will unregister the callback
 */
FCookerCleanupShaderCacheHandler::~FCookerCleanupShaderCacheHandler()
{
	GCallbackEvent->Unregister(CALLBACK_PackageSaveCleanupShaderCache, this);
}

/** Shader cache cleanup time */
DOUBLE Cooker_CleanupSavedShaderCacheTime;
/**
 *	The interface for receiving notifications about cleaning up shader caches while saving a package
 */
void FCookerCleanupShaderCacheHandler::Send( ECallbackEventType InType, const FString& InString, UPackage* InPackage, UObject* InObject)
{
	DOUBLE Cooker_CleanupSavedShaderCacheTimeStart = appSeconds();
	switch (InType)
	{
		case CALLBACK_PackageSaveCleanupShaderCache:
		{
			ULinker* ObjAsLinker = Cast<ULinker>(InObject);
			if (ObjAsLinker != NULL)
			{
				UShaderCache* SavedShaderCache = NULL;
				TArray<FStaticParameterSet> SavedMaterialIDs;

				// Find all material interface and shader caches in the export map
				for (INT ExpIdx = 0; ExpIdx < ObjAsLinker->ExportMap.Num(); ExpIdx++)
				{
					FObjectExport& ObjExp = ObjAsLinker->ExportMap(ExpIdx);
					UShaderCache* CheckCache = Cast<UShaderCache>(ObjExp._Object);
					UMaterialInterface* CheckMtrlIntf = Cast<UMaterialInterface>(ObjExp._Object);
					ATerrain* CheckTerrain = Cast<ATerrain>(ObjExp._Object);
					if (CheckCache != NULL)
					{
						if (CheckCache->GetOuter() == InPackage)
						{
							if (SavedShaderCache != NULL)
							{
								warnf(NAME_Log, TEXT("\t\tPrevious ShaderCache? %s"), *(SavedShaderCache->GetPathName()));
							}
							SavedShaderCache = CheckCache;
						}
						else
						{
							warnf(NAME_Log, TEXT("\t\tShaderCache %s outer (%s) not package %s?"), 
								*(CheckCache->GetPathName()), *(CheckCache->GetOuter()->GetName()), *(InPackage->GetName()));
						}
					}
					else if ((CheckMtrlIntf != NULL) && (!CheckMtrlIntf->HasAnyFlags(RF_ClassDefaultObject)))
					{
						FMaterialResource* MtrlRes = CheckMtrlIntf->GetMaterialResource();
						if (MtrlRes != NULL)
						{
							if (MtrlRes->GetShaderMap() != NULL)
							{
								SavedMaterialIDs.AddUniqueItem(MtrlRes->GetShaderMap()->GetMaterialId());
							}
							else
							{
								debugfSlow(TEXT("Failed to find shadermap for saved %s"), *CheckMtrlIntf->GetFullName());
							}
						}
						else
						{
							debugfSlow(TEXT("Failed to find material resource for saved %s"), *CheckMtrlIntf->GetFullName());
						}
					}
					else if (CheckTerrain != NULL)
					{
						TArrayNoInit<FTerrainMaterialResource*>& CachedMaterials = CheckTerrain->GetCachedTerrainMaterials();
						// Make sure materials are compiled for the platform and add them to the shader cache embedded into seekfree packages.
						for (INT CachedMatIndex = 0; CachedMatIndex < CachedMaterials.Num(); CachedMatIndex++)
						{
							FTerrainMaterialResource* TMatRes = CachedMaterials(CachedMatIndex);
							if (TMatRes != NULL)
							{
								if (TMatRes->GetShaderMap() != NULL)
								{
									SavedMaterialIDs.AddUniqueItem(TMatRes->GetShaderMap()->GetMaterialId());
								}
								else
								{
									FStaticParameterSet EmptySet(TMatRes->GetId());
									SavedMaterialIDs.AddUniqueItem(EmptySet);
								}
							}
						}
					}
				}

				// If we found a value shader cache, clear out any material interfaces that were not saved
				if (SavedShaderCache != NULL)
				{
					// Cleanup the shader cache...
					INT RemovedCount = SavedShaderCache->CleanupCacheEntries(SavedMaterialIDs);
				}
			}
			else
			{
				warnf(NAME_Warning, TEXT("FCookerCleanupShaderCacheHandler> Object passed is not a linker... %s"),
					InObject ? *(InObject->GetFullName()) : TEXT("NULL"));
			}
			break;
		}
	}
	Cooker_CleanupSavedShaderCacheTime = appSeconds() - Cooker_CleanupSavedShaderCacheTimeStart;
}

/**
 * Saves the passed in package, gathers and stores bulk data info and keeps track of time spent.
 *
 * @param	Package						Package to save
 * @param	Base						Base/ root object passed to SavePackage, can be NULL
 * @param	TopLevelFlags				Top level "keep"/ save flags, passed onto SavePackage
 * @param	DstFilename					Filename to save under
 * @param	bStripEverythingButTextures	Whether to strip everything but textures
 * @param	bRememberSavedObjects		TRUE if objects should be marked to not be saved again, as well as materials (if bRemeberSavedObjects is set)
 * @param	OutSavedObjects				Optional map to fill in with the objects that were saved
 */
void UCookPackagesCommandlet::SaveCookedPackage(UPackage* Package, UObject* Base, EObjectFlags TopLevelFlags, const TCHAR* DstFilename, 
	UBOOL bStripEverythingButTextures, UBOOL bCleanupAfterSave, UBOOL bRememberSavedObjects, TMap<FString,FString>* OutSavedObjects)
{
	SCOPE_SECONDS_COUNTER_COOK(PackageSaveTime);

	// Mark the chain of any object set for ForceTagExp as well...
	for (FObjectIterator TagObjIt; TagObjIt; ++TagObjIt)
	{
		UObject* TagObject = *TagObjIt;
		if (TagObject->HasAnyFlags(RF_ForceTagExp))
		{
			UObject* TagOuter = TagObject->GetOuter();
			while (TagOuter)
			{
				TagOuter->SetFlags(RF_ForceTagExp);
				TagOuter = TagOuter->GetOuter();
			}
		}
	}

	// for PC cooking, we leave all objects in their original packages, and in cooked packages
	if (Platform == PLATFORM_Windows)
	{
		bStripEverythingButTextures = FALSE;

		// make sure destination exists
		GFileManager->MakeDirectory(*FFilename(DstFilename).GetPath(), TRUE);

		for( TObjectIterator<UObject> It; It; ++It )
		{
			// never allow any shader objects to get cooked into another package
			if (It->IsA(UShaderCache::StaticClass()))
			{
				It->ClearFlags( RF_ForceTagExp );
			}
		}
	}

	// Remove RF_Standalone from everything but UTexture2D objects if we're stripping out data.
	TMap<UObject*,UBOOL> ObjectsToRestoreStandaloneFlag;
	TMap<UObject*,UBOOL> ObjectsToRemoveStandaloneFlag;
	UBOOL bPackageContainsTextures = FALSE;
	if( bStripEverythingButTextures )
	{
		check(Base==NULL);
		// Iterate over objects, remove RF_Standalone from non- UTexture2D objects and keep track of them for later.
		for( TObjectIterator<UObject> It; It; ++It )
		{
			UObject* Object = *It;
			// Non UTexture2D object that is marked RF_Standalone.
			if( Object->IsA(UTexture2D::StaticClass())
				// if cooking for a console, only keep textures that can actually be loaded on the console
				// if cooking for PC, keep them since we support loading cooked packages in the editor
				&&( ((GCookingTarget & UE3::PLATFORM_Stripped) == 0) || !Object->HasAllFlags(RF_NotForServer|RF_NotForClient)) )
			{
				if( Object->IsIn( Package ) )
				{
					bPackageContainsTextures = TRUE;

					// fonts aren't standalone (and in the future maybe other's won't be as well), so
					// temporarily mark them as standalone so they are saved out in the streaming texture package
					// yes, fonts shouldn't be streaming, but just in case, we don't want to cause crashes!
					if (Object->HasAllFlags(RF_Standalone) == FALSE)
					{
						// Keep track of object and set flag.
						ObjectsToRemoveStandaloneFlag.Set( Object, TRUE );
						Object->SetFlags( RF_Standalone );
					}
				}
			}
			else if( Object->HasAllFlags( RF_Standalone ) )
			{
				// Keep track of object and remove flag.
				ObjectsToRestoreStandaloneFlag.Set( Object, TRUE );
				Object->ClearFlags( RF_Standalone );
			}
		}
	}

	if( !bStripEverythingButTextures || bPackageContainsTextures)
	{
		// Save package. World can be NULL for regular packages. @warning: DO NOT conform here, as that will cause 
		// the number of generations in the summary of package files to mismatch with the number of generations in 
		// forced export copies of that package! SavePackage has code for the cooking case to match up GUIDs with 
		// the original package

		//@script patcher
		ULinkerLoad* ConformBase = NULL;
		if( !ParseParam(appCmdLine(),TEXT("NOCONFORM")) )
		{
			FFilename BasePackageName = DstFilename;
			FString ConformLocationPath;
			UBOOL bUserSpecifiedConformDirectory = Parse(appCmdLine(),TEXT("CONFORMDIR="),ConformLocationPath);
			if ( !bUserSpecifiedConformDirectory )
			{
				//@script patcher fixme - make the default conform directory an .ini setting or something?
				ConformLocationPath = appGameDir() * TEXT("Build\\ConformSourceBuild\\CookedXenon");
			}

			if ( ConformLocationPath.Len() > 0 )
			{
				// verify that the directory exists
				TArray<FString> DummyArray;
				GFileManager->FindFiles(DummyArray, *ConformLocationPath, FALSE, TRUE);
				if ( DummyArray.Num() == 0 )
				{
					// attempt to normalize the specified directory name
					ConformLocationPath = appConvertRelativePathToFull(*ConformLocationPath);
					GFileManager->FindFiles(DummyArray, *ConformLocationPath, FALSE, TRUE);
					if ( DummyArray.Num() == 0 && bUserSpecifiedConformDirectory )
					{
						if ( appMsgf(AMT_YesNo, TEXT("Conform path specified doesn't exist (%s).  Choose 'Yes' to stop cooking or 'No' to continue without cooking without conforming"), *ConformLocationPath) )
						{
							bAbortCooking = TRUE;
						}
					}
				}
			}
			
			if ( !bAbortCooking )
			{
				FFilename ConformBasePathName = ConformLocationPath * BasePackageName.GetCleanFilename();
				if ( GFileManager->FileSize(*ConformBasePathName) > 0 )
				{
					// check the default location for script packages to conform against, if a like-named package exists in the
					// auto-conform directory, use that as the conform package
					UObject::BeginLoad();

					UPackage* ConformBasePackage = UObject::CreatePackage(NULL, *(BasePackageName.GetBaseFilename() + TEXT("_OLD")));

					// loading this cooked package will cause the package file lookup to thereafter refer to the cooked file, so before loading the file, grab the current path
					// associated with this package name so we can restore it later
					FString PreviousPackagePath;
					GPackageFileCache->FindPackageFile(*BasePackageName.GetBaseFilename(), NULL, PreviousPackagePath);

					// now load the cooked version of the original package
					ConformBase = UObject::GetPackageLinker( ConformBasePackage, *ConformBasePathName, LOAD_Quiet|LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );

					// hmmmm, the question is - do we restore the previous path now or after we call EndLoad()?
					if ( PreviousPackagePath.Len() )
					{
						GPackageFileCache->CachePackage(*PreviousPackagePath, TRUE, FALSE);
					}

					UObject::EndLoad();
					warnf( TEXT("Conforming: %s"), DstFilename );

					// If the passed in Base is an ObjectReferencer and it was created using a transient name, we'll need to rename it so that
					// the package can be conformed correctly
					if ( Base != NULL && Base->IsA(UObjectReferencer::StaticClass()) && Base->GetName().StartsWith(TEXT("ObjectReferencer_")) )
					{
						// find the one in the package we're conforming against
						for ( INT ExportIndex = 0; ExportIndex < ConformBase->ExportMap.Num(); ExportIndex++ )
						{
							FObjectExport& OriginalExport = ConformBase->ExportMap(ExportIndex);
							if ( OriginalExport._Object && OriginalExport._Object->GetClass() == UObjectReferencer::StaticClass() )
							{
								if ( OriginalExport._Object->GetName().StartsWith(TEXT("ObjectReferencer_")) &&
									Base->GetName() != OriginalExport._Object->GetName() )
								{
									// found it - now we rename our own ObjectReferencer to have the same name as the original one
									// no need to worry about a redirect being created since ObjectReferencers aren't marked RF_Public

									// first, sanity check that there isn't already another ObjectReferencer using this name
									UObjectReferencer* ExistingReferencer = FindObject<UObjectReferencer>(Base->GetOuter(),
										*OriginalExport._Object->GetName(), TRUE);
									check(ExistingReferencer==NULL);

									Base->Rename(*OriginalExport._Object->GetName(), NULL, REN_ForceNoResetLoaders);
								}
							}
						}
					}
				}
			}
		}

		if ( !bAbortCooking )
		{
			Package->PackageFlags |= (GCookingTarget & PLATFORM_FilterEditorOnly) != 0 ? PKG_FilterEditorOnly : 0;
			if (bForceFilterEditorOnly == TRUE)
			{
				Package->PackageFlags |= PKG_FilterEditorOnly;
			}

			// cache the cooked version for this file
			PersistentCookerData->SetFileCookedVersion(DstFilename, GPackageFileCookedContentVersion);

			// On Android, Save out the global flags for which formats were cooked
			if (Platform == PLATFORM_Android)
			{
				PersistentCookerData->SetCookedTextureFormats(AndroidTextureFormat);
			}

			// Remove package compression flags if package compression is not wanted.
			if( bDisallowPackageCompression )
			{
				Package->PackageFlags &= ~(PKG_StoreCompressed | PKG_StoreFullyCompressed);
			}
			if (bIsMTChild)
			{
				// Gather bulk data information from just saved packages and save persistent cooker data to disk.
				PersistentCookerData->GatherCookedBulkDataInfos( Package, AndroidTextureFormat );
				SynchronizeTFCs();
			}
			if (bIsMTChild || bIsMTMaster)
			{
				WaitToDeleteFile(DstFilename);
			}
			else
			{
				GFileManager->Delete(DstFilename);
			}
			if (FFilename(DstFilename).FileExists())
			{
				appErrorf(TEXT("Destination file exists after deletion %s"),DstFilename);
			}

			if (bSkipStartupObjectDetermination == FALSE)
			{
				// Save off the cleanupshadercache function pointer
				GCallbackEvent->Register(CALLBACK_PackageSaveCleanupShaderCache,&CleanupShaderCacheCallbackHandler);
			}
			UObject::SavePackage( Package, Base, TopLevelFlags, DstFilename, GWarn, ConformBase, ShouldByteSwapData() );
			if (bSkipStartupObjectDetermination == FALSE)
			{
				// Restore 
				GCallbackEvent->Unregister(CALLBACK_PackageSaveCleanupShaderCache,&CleanupShaderCacheCallbackHandler);
				CookStats.CleanupShaderCacheTime += Cooker_CleanupSavedShaderCacheTime;
			}

			if (!FFilename(DstFilename).NonLockingFileExists())
			{
				warnf(NAME_Error,TEXT("Destination file exists does not exist after cooking %s"),DstFilename);
			}

			if (ParseParam(appCmdLine(), TEXT("WriteContents")))
			{
				static UBOOL Once;
				if (!Once)
				{
					Once = TRUE;
					GFileManager->MakeDirectory(*(FFilename(DstFilename).GetPath() * TEXT("Contents")));
				}
				TArray<FString> Items;
				for( FObjectIterator It; It; ++It )
				{
					UObject*			Object				= *It;
					if (Object->HasAnyFlags(RF_Saved))				
					{
						FString Item = Object->GetFullName().ToUpper();
						Item.ReplaceInline(TEXT("0"),TEXT(""));
						Item.ReplaceInline(TEXT("1"),TEXT(""));
						Item.ReplaceInline(TEXT("2"),TEXT(""));
						Item.ReplaceInline(TEXT("3"),TEXT(""));
						Item.ReplaceInline(TEXT("4"),TEXT(""));
						Item.ReplaceInline(TEXT("5"),TEXT(""));
						Item.ReplaceInline(TEXT("6"),TEXT(""));
						Item.ReplaceInline(TEXT("7"),TEXT(""));
						Item.ReplaceInline(TEXT("8"),TEXT(""));
						Item.ReplaceInline(TEXT("9"),TEXT(""));
						Items.AddItem(Item);
					}
				}
				Sort<USE_COMPARE_CONSTREF(FString,UnContentCookersStringSort)>(&Items(0),Items.Num());
				FString All;
				for (INT Index=0;Index<Items.Num();Index++)
				{
					All += Items(Index) + TEXT("\r\n");
				}
				appSaveStringToFile(All,*((FFilename(DstFilename).GetPath() * TEXT("Contents")) * FFilename(DstFilename).GetBaseFilename() + TEXT(".txt")));
			}
		}
	}

	// Restore RF_Standalone flag on objects.
	if( bStripEverythingButTextures )
	{
		for( TMap<UObject*,UBOOL>::TIterator It(ObjectsToRestoreStandaloneFlag); It; ++It )
		{
			UObject* Object = It.Key();
			Object->SetFlags( RF_Standalone );
		}
		for( TMap<UObject*,UBOOL>::TIterator It(ObjectsToRemoveStandaloneFlag); It; ++It )
		{
			UObject* Object = It.Key();
			Object->ClearFlags( RF_Standalone );
		}
	}

	// Clean up objects for subsequent runs.
	if (bCleanupAfterSave || bRememberSavedObjects)
	{
		// Remove RF_ForceTagExp again for subsequent calls to SavePackage and set RF_MarkedByCooker for objects
		// saved into script. Also ensures that always loaded materials are only put into a single seekfree shader 
		// cache.
		for( FObjectIterator It; It; ++It )
		{
			UObject* Object = *It;
			
			if (bRememberSavedObjects)
			{
				UMaterial* Material = Cast<UMaterial>(Object);
				UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Object);
				// Avoid object from being put into subsequent script or map packages.
				if (Object->HasAnyFlags(RF_Saved))
				{
					Object->SetFlags(RF_MarkedByCooker);
					if (bSkipStartupObjectDetermination == FALSE)
					{
						if (OutSavedObjects != NULL)
						{
							if (Object->IsA(UPersistentCookerData::StaticClass()) == FALSE)
							{
								FString FullName = Object->GetFullName();
								FString ClassName = Object->GetClass()->GetName();
								FString PathName = Object->GetPathName();
								FullName = FString::Printf(TEXT("%s %s"), *ClassName, *PathName);

								OutSavedObjects->Set(FullName, ClassName);
								check(FullName == Object->GetFullName());
								// Mark it as a cooked startup object so subsequent startup packages 
								// don't pull it in.
								Object->SetFlags(RF_CookedStartupObject);
								TrackedCookedStartupObjects.Set(FullName, TRUE);
								if (Object->HasAnyFlags(RF_MarkedByCookerTemp))
								{
									Object->ClearFlags(RF_MarkedByCookerTemp|RF_Transient);
								}
							}

							// Don't put material into subsequent seekfree shader caches.
							if (ShaderCache && Material && !Material->HasAnyFlags(RF_ClassDefaultObject))
							{
								PersistentCookerData->AlreadyHandledStartupMaterials.Add(Material->GetFullName());
								AlreadyHandledMaterials.Add(Material->GetFullName());
							}
							if (ShaderCache && MaterialInstance && MaterialInstance->bHasStaticPermutationResource && !MaterialInstance->HasAnyFlags(RF_ClassDefaultObject))
							{
								PersistentCookerData->AlreadyHandledStartupMaterialInstances.Add(MaterialInstance->GetFullName());
								AlreadyHandledMaterialInstances.Add(MaterialInstance->GetFullName());
							}
						}
					}
				}

				if (bSkipStartupObjectDetermination == TRUE)
				{
					// Don't put material into subsequent seekfree shader caches.
					if( ShaderCache && Material )
					{
						AlreadyHandledMaterials.Add(Material->GetFullName());
					}
					if( ShaderCache && MaterialInstance )
					{
						AlreadyHandledMaterialInstances.Add(MaterialInstance->GetFullName());
					}
				}
			}

			if (bCleanupAfterSave)
			{
				Object->ClearFlags(RF_ForceTagExp | RF_Saved);
			}
		}
	}

	//@script patcher
	if (!bAbortCooking)
	{
		if (!bIsMTChild)
		{
			// Warn if waste exceeds 100 MByte.
			INT TextureFileCacheWasteMByte = PersistentCookerData->GetTextureFileCacheWaste() / 1024 / 1024;
			if( TextureFileCacheWasteMByte > 100 )
			{
				warnf( NAME_Warning, TEXT("Texture file cache waste exceeds %i MByte. Please consider a full recook!"), TextureFileCacheWasteMByte );
			}

			// Flush texture file cache archives to disk.
			for(TMap<FName,FArchive*>::TIterator TextureCacheIt(TextureCacheNameToArMap);TextureCacheIt;++TextureCacheIt)
			{
				TextureCacheIt.Value()->Flush();
			}
			// update TFC entries with new file timestamps
			PersistentCookerData->UpdateCookedTextureFileCacheEntryInfos( TextureCacheNameToFilenameMap );

			// Gather bulk data information from just saved packages and save persistent cooker data to disk.
			PersistentCookerData->GatherCookedBulkDataInfos( Package, AndroidTextureFormat );
		}
		// Cache any script SHA info for the package
		if (DLCName.Len())
		{
			if (Platform == PLATFORM_PS3)
			{
				// is there any script SHA for this package?
				FString PackageName = Package->GetName();
				TArray<BYTE>* PackageScriptSHA = ULinkerSave::PackagesToScriptSHAMap.Find(PackageName);
				if (PackageScriptSHA)
				{
					// even if we found it, it may be empty (because there was no script code in it)
					if (PackageScriptSHA->Num() == 20)
					{
						// open the mod hashes file if we haven't opened it yet
						if (UserModeSHAWriter == NULL)
						{
							UserModeSHAWriter = GFileManager->CreateFileWriter(*(CookedDir + TEXT("ScriptHashes.sha")));
							check(UserModeSHAWriter);

							// write a divider first thing, because all hashes in this file are script-only hashes
							UserModeSHAWriter->Serialize(HASHES_SHA_DIVIDER, strlen(HASHES_SHA_DIVIDER) + 1);
						}

						warnf(TEXT("  Hashing mod/dlc package for package %s"), *Package->GetName());

						// write out the filename (including trailing 0)
						// don't write the file path
						UserModeSHAWriter->Serialize(TCHAR_TO_ANSI(*PackageName), PackageName.Len() + 1);

						// write out hash
						UserModeSHAWriter->Serialize(PackageScriptSHA->GetData(), PackageScriptSHA->Num());
					}
				}
			}
		}
		else
		{
			PersistentCookerData->CacheScriptSHAData( *Package->GetName() );
		}

		if (!bIsMTChild && !bIsMTMaster)
		{
			PersistentCookerData->SaveToDisk();
		}
	}
}

/**
 * Returns whether there are any localized resources that need to be handled.
 *
 * @param Package			Current package that is going to be saved
 * @param TopLevelFlags		TopLevelFlags that are going to be passed to SavePackage
 * 
 * @return TRUE if there are any localized resources pending save, FALSE otherwise
 */
UBOOL UCookPackagesCommandlet::AreThereLocalizedResourcesPendingSave( UPackage* Package, EObjectFlags TopLevelFlags )
{
	UBOOL bAreLocalizedResourcesPendingSave = FALSE;
	for( FObjectIterator It; It; ++It )
	{
		UObject* Object = *It;
		if( Object->IsLocalizedResource() )
		{
			if (Object->IsIn(Package) || Object->HasAnyFlags(TopLevelFlags | RF_ForceTagExp))
			{
				if (!Object->HasAnyFlags(RF_MarkedByCooker))
				{
					bAreLocalizedResourcesPendingSave = TRUE;
					break;
				}
				else
				{
					debugf(TEXT("AreThereLocalizedResourcesPendingSave> LocRes MarkedByCooker: %s for package %s"), 
						*(Object->GetPathName()), *(Package->GetName()));
				}
			}
		}
	}
	return bAreLocalizedResourcesPendingSave;
}

/**
 * @return		TRUE if there are localized resources using the current language that need to be handled.
 */
static UBOOL AreThereNonEnglishLocalizedResourcesPendingSave(UPackage* Package)
{
	// Shouldn't be called in English, as _INT doesn't appear as a package file prefix.
	check( appStricmp( TEXT("INT"), UObject::GetLanguage() ) != 0 );

	UBOOL bAreNonEnglishLocalizedResourcesPendingSave = FALSE;
	for( FObjectIterator It; It; ++It )
	{
		UObject* Object = *It;
		if( Object->IsLocalizedResource() )
		{
			if (Object->IsIn(Package) || Object->HasAnyFlags(RF_ForceTagExp))
			{
				if (!Object->HasAnyFlags(RF_MarkedByCooker))
				{
					UPackage* ObjPackage = Object->GetOutermost();
					const FString ObjPackageName( ObjPackage->GetName() );

					FString FoundPackageFileName;
					// Check if the found filename contains for a cached package of the same name.
					FFilename LocalizedPackageName = FPackageFileCache::PackageFromPath( *ObjPackageName );
					LocalizedPackageName = LocalizedPackageName.GetLocalizedFilename( UObject::GetLanguage() );

					if( GPackageFileCache->FindPackageFile( *LocalizedPackageName, NULL, FoundPackageFileName ) )
					{
						bAreNonEnglishLocalizedResourcesPendingSave = TRUE;
						break;
					}
					else
					{
						debugf(TEXT("AreThereNonEnglishLocalizedResourcesPendingSave> LocRes PackageNotFound: %s for package %s (%s), localizedname %s"), 
							*(Object->GetPathName()), *ObjPackageName, *FoundPackageFileName, *LocalizedPackageName);
					}
				}
				else
				{
					debugf(TEXT("AreThereNonEnglishLocalizedResourcesPendingSave> LocRes MarkedByCooker: %s for package %s"), 
						*(Object->GetPathName()), *(Package->GetName()));
				}
			}
		}
	}
	return bAreNonEnglishLocalizedResourcesPendingSave;
}

template < class T >
class TMPSoundObjectReferenceCollector : public FArchive
{
public:

	/**
	* Constructor
	*
	* @param	InObjectArray			Array to add object references to
	* @param	InOuters				value for LimitOuter
	* @param	bInRequireDirectOuter	value for bRequireDirectOuter
	* @param	bShouldIgnoreArchetype	whether to disable serialization of ObjectArchetype references
	* @param	bInSerializeRecursively	only applicable when LimitOuter != NULL && bRequireDirectOuter==TRUE;
	*									serializes each object encountered looking for subobjects of referenced
	*									objects that have LimitOuter for their Outer (i.e. nested subobjects/components)
	* @param	bShouldIgnoreTransient	TRUE to skip serialization of transient properties
	*/
	TMPSoundObjectReferenceCollector( TArray<T*>* InObjectArray, const TArray<FString>& InOuters, UBOOL bInRequireDirectOuter=TRUE, UBOOL bShouldIgnoreArchetype=FALSE, UBOOL bInSerializeRecursively=FALSE, UBOOL bShouldIgnoreTransient=FALSE )
		:	ObjectArray( InObjectArray )
		,	LimitOuters(InOuters)
		,	bRequireDirectOuter(bInRequireDirectOuter)
	{
		ArIsObjectReferenceCollector = TRUE;
		ArIsPersistent = bShouldIgnoreTransient;
		ArIgnoreArchetypeRef = bShouldIgnoreArchetype;
		bSerializeRecursively = bInSerializeRecursively && LimitOuters.Num() > 0;
	}
protected:

	/** Stored pointer to array of objects we add object references to */
	TArray<T*>*		ObjectArray;

	/** List of objects that have been recursively serialized */
	TArray<UObject*> SerializedObjects;

	/** only objects within these outers will be considered. */
	TArray<FString>	LimitOuters;

	/** determines whether nested objects contained within LimitOuter are considered */
	UBOOL			bRequireDirectOuter;

	/** determines whether we serialize objects that are encounterd by this archive */
	UBOOL			bSerializeRecursively;
};

/**
* Helper implementation of FArchive used to collect object references, avoiding duplicate entries.
*/
class FArchiveMPSoundObjectReferenceCollector : public TMPSoundObjectReferenceCollector<UObject>
{
public:
	/**
	* Constructor
	*
	* @param	InObjectArray			Array to add object references to
	* @param	InOuters				value for LimitOuter
	* @param	bInRequireDirectOuter	value for bRequireDirectOuter
	* @param	bShouldIgnoreArchetype	whether to disable serialization of ObjectArchetype references
	* @param	bInSerializeRecursively	only applicable when LimitOuter != NULL && bRequireDirectOuter==TRUE;
	*									serializes each object encountered looking for subobjects of referenced
	*									objects that have LimitOuter for their Outer (i.e. nested subobjects/components)
	* @param	bShouldIgnoreTransient	TRUE to skip serialization of transient properties
	*/
	FArchiveMPSoundObjectReferenceCollector( TArray<UObject*>* InObjectArray, const TArray<FString>& InOuters, UBOOL bInRequireDirectOuter=TRUE, UBOOL bShouldIgnoreArchetypes=FALSE, UBOOL bInSerializeRecursively=FALSE, UBOOL bShouldIgnoreTransient=FALSE )
		:	TMPSoundObjectReferenceCollector<UObject>( InObjectArray, InOuters, bInRequireDirectOuter, bShouldIgnoreArchetypes, bInSerializeRecursively, bShouldIgnoreTransient )
	{}

private:
	/** 
	* UObject serialize operator implementation
	*
	* @param Object	reference to Object reference
	* @return reference to instance of this class
	*/
	FArchive& operator<<( UObject*& Object )
	{
		// Avoid duplicate entries.
		if ( Object != NULL )
		{
			if ( LimitOuters.Num() == 0 || LimitOuters.ContainsItem(Object->GetOutermost()->GetName()) )
			{
				if ( !ObjectArray->ContainsItem(Object) )
				{
					ObjectArray->AddItem( Object );

					// check this object for any potential object references
					if ( bSerializeRecursively )
					{
						Object->Serialize(*this);
					}
				}
			}
		}
		return *this;
	}
};

/**
 * Generates a list of all objects in the object tree rooted at the specified sound cues.
 *
 * @param	MPCueNames				The names of the MP sound 'root set'.
 * @param	OutMPObjectNames		[out] The names of all objects referenced by the input set of objects, including those objects.
 */
static void GenerateMPObjectList(const TArray<FString>& MPCueNames, TArray<FString>& OutMPObjectNames)
{
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Load all MP-referenced cues.
	TArray<USoundCue*> MPCues;
	for ( INT CueIndex = 0 ; CueIndex < MPCueNames.Num() ; ++CueIndex )
	{
		const FString& CueName = MPCueNames(CueIndex);
		USoundCue* Cue = FindObject<USoundCue>( NULL, *CueName );
		if ( !Cue )
		{
			Cue = LoadObject<USoundCue>( NULL, *CueName, NULL, LOAD_None, NULL );
		}
		if ( Cue )
		{
			MPCues.AddItem( Cue );
		}
		else
		{
			warnf( NAME_Log, TEXT("MP Sound Cues: couldn't load %s"), *CueName );
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Load all MP-referenced sound cue packages.
	const TCHAR* MPSoundcuePackagesIniSection = TEXT("Cooker.MPSoundCuePackages");
	FConfigSection* IniMPPackagesList = GConfig->GetSectionPrivate( MPSoundcuePackagesIniSection, FALSE, TRUE, GEditorIni );
	//TArray<UObject*> SoundPackages;
	TArray<FString> SoundPackages;
	if ( MPSoundcuePackagesIniSection && IniMPPackagesList )
	{
		for( FConfigSectionMap::TConstIterator It(*IniMPPackagesList) ; It ; ++It )
		{
			const FString& PackageName = It.Value();
			FString PackageFilename;
			UPackage* Result = NULL;
			if( GPackageFileCache->FindPackageFile( *PackageName, NULL, PackageFilename ) )
			{
				SoundPackages.AddUniqueItem( PackageName );
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Serialize all the cues into a reference collector.
	TArray<UObject*> References;
	FArchiveMPSoundObjectReferenceCollector ObjectReferenceCollector( &References, SoundPackages, FALSE, FALSE, TRUE, TRUE );
	for ( INT CueIndex = 0 ; CueIndex < MPCues.Num() ; ++ CueIndex )
	{
		USoundCue* Cue = MPCues(CueIndex);
		Cue->Serialize( ObjectReferenceCollector );
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Create a list of the names of all objects referenced by the MP cues.
	OutMPObjectNames.Empty();

	// Seed the list with the cues themselves.
	OutMPObjectNames = MPCueNames;

	// Add referenced objects.
	for ( INT Index = 0 ; Index < References.Num() ; ++Index )
	{
		const UObject* Object = References(Index);
		// Exclude transient objects.
		if( !Object->HasAnyFlags( RF_Transient ) &&	!Object->IsIn( UObject::GetTransientPackage() ) )
		{
			const FString ObjectPath = Object->GetPathName();
			debugfSuppressed( NAME_DevCooking, TEXT("MPObjects: %s (%s)"), *ObjectPath, *Object->GetClass()->GetName() );
			OutMPObjectNames.AddItem( ObjectPath );
		}
	}
}

/**
 * We use the CreateCustomEngine call to set some flags which will allow SerializeTaggedProperties to have the correct settings
 * such that editoronly and notforconsole data can correctly NOT be loaded from startup packages (e.g. engine.u)
 *
 **/
void UCookPackagesCommandlet::CreateCustomEngine()
{
	// get the platform from the Params (MUST happen first thing)
	if ( !SetPlatform( appCmdLine() ) )
	{
		SET_WARN_COLOR(COLOR_RED);
		warnf(NAME_Error, TEXT("Platform not specified. You must use:\n   platform=[%s]"), *appValidPlatformsString());
		CLEAR_WARN_COLOR();
	}
    
	GIsCooking				= TRUE;
	GCookingTarget 			= Platform;
	GCookingShaderPlatform	= ShaderPlatform;

	// force cooking to use highest compat level
	appSetCompatibilityLevel(FCompatibilityLevelInfo(5,5,5), FALSE);

	// make sure shader compiler DLL is loaded
	if( !BindDLLs() )
	{
		warnf( TEXT( "Error: Failed to bind to console support dll; aborting." ) );
		Platform = PLATFORM_Unknown;
	}
}

/**
 * On a heavily loaded machine, sometimes, files don't want to delete right away and we need to 
 * make sure they are deleted for integrity, so this routine calls GFilemanger->Delete until it 
 * succeeds or MTTimeout time is spent...if the timeout expires, we call appError
 * During this time, it calls CheckForCrashedChildren repeatedly if we are the master process
 *
 * @param	Filename file to delete
 */
void UCookPackagesCommandlet::WaitToDeleteFile(const TCHAR *Filename)
{
	if(!GFileManager->Delete(Filename))
	{
		DOUBLE StartTime = appSeconds();
		do 
		{
			if (bIsMTMaster)
			{
				CheckForCrashedChildren();
			}
			appSleep(MTSpinDelay);
			if (appSeconds() - StartTime > MTTimeout)
			{
				// do this one more time because a detected child crash is a little more graceful
				if (bIsMTMaster)
				{
					CheckForCrashedChildren();
				}
				appErrorf(TEXT("Could not delete %s"),Filename);
			}
		} while (!GFileManager->Delete(Filename));
	}
}

/**
 * A robust version of appSaveStringToFile
 * we save to a temp file, then rename. This is important for synchronization, which is what this is used for
 *
 * @param	string to save
 * @param	Filename file to save
 */
void UCookPackagesCommandlet::RobustSaveStringToFile(const FString& String,const TCHAR *Filename)
{
	FString TempName = Filename;
	TempName += TEXT(".temp");
	UBOOL Ok = appSaveStringToFile(String,*TempName);
	if (!Ok)
	{
		appErrorf(TEXT("Could not save %s"),*TempName);
	}
	if (!GFileManager->Move( Filename, *TempName ))
	{
		appErrorf(TEXT("Could not move to %s"),*Filename);
	}
}

/**
 * Save a persistent cooker data structure to a specific filename in a bullet proof manner
 * calls reset loaders
 * attempts to delete the destination
 * leave the internal filename to Where
 *
 * @param	Who		UPersistentCookerData to save
 * @param	Where	Filename to save to
 */
void UCookPackagesCommandlet::BulletProofPCDSave(UPersistentCookerData* Who,const TCHAR* Where)
{
	UObject::ResetLoaders(Who->GetOutermost());
	WaitToDeleteFile(Where);
	Who->SetFilename(Where);
	Who->SaveToDisk(); 
	if (!FFilename(Where).NonLockingFileExists())
	{
		appErrorf(TEXT("Could not save %s"),Where);
	}
}

/**
 * Load a persistent cooker data structure from a specific filename in a bullet proof manner
 * verifies the source file exists
 *
 * @param	Where	Filename to save to
 * @return	Newly loaded PCD check()'d to be non-null
 */
UPersistentCookerData* UCookPackagesCommandlet::BulletProofPCDLoad(const TCHAR *Where)
{
	check(FFilename(Where).FileExists());
	UPersistentCookerData* Result;
	if (GGameCookerHelper)
	{
		Result = GGameCookerHelper->CreateInstance(Where, FALSE);
	}
	else
	{
		Result = UPersistentCookerData::CreateInstance(Where, FALSE);
	}
	check(Result);
	return Result;
}

/**
 * Start all child processes for MT cooks
 *  Only used if bIsMTMaster is TRUE. 
 *
 * @param	NumFiles		estimate of the number of files we have to process
 * @return FALSE if there are not enough cores or jobs to bother with MT cooking
 */
UBOOL UCookPackagesCommandlet::StartChildren(INT NumFiles)
{
	INT NumChildProcesses = 5;
	UBOOL Overridden = FALSE;
	if (Parse(appCmdLine(), TEXT("Processes="),NumChildProcesses))
	{
		Overridden = TRUE;
	}
	if (NumChildProcesses < 2)
	{
		warnf(NAME_Log, TEXT("*** Ignoring mutithreaded, less than 2 processes requested."));
		return FALSE; 
	}
	if (NumFiles < 2)
	{
		warnf(NAME_Log, TEXT("*** Ignoring mutithreaded, less than 2 jobs."));
		return FALSE; 
	}
	if (GNumHardwareThreads < 3 && !Overridden)
	{
		warnf(NAME_Log, TEXT("*** Ignoring mutithreaded, less than quadcore."));
		return FALSE; 
	}
	if (GPhysicalGBRam < 7 && !Overridden)
	{
		warnf(NAME_Log, TEXT("*** Ignoring mutithreaded, less than 7GB of RAM (%dGB)."),GPhysicalGBRam);
		return FALSE; 
	}
	BulletProofPCDSave(PersistentCookerData,*(CookedDir * GetBulkDataContainerFilename()));
	SaveLocalShaderCaches(); // children will load this, and maybe we did some work here.

	INT RamLimit = Max<INT>(2,INT(FLOAT(GPhysicalGBRam - 2)/2.2f));
	NumChildProcesses = Min<INT>(NumChildProcesses,RamLimit);
	NumChildProcesses = Min<INT>(NumChildProcesses,GNumHardwareThreads - 1);
	NumChildProcesses = Min<INT>(NumChildProcesses,NumFiles);
	NumChildProcesses = Clamp<INT>(NumChildProcesses,1,48);
	warnf(NAME_Log, TEXT("*** Multithreaded cook, starting %d processes for %d files     (%d cores, %dGB)."),NumChildProcesses,NumFiles,GNumHardwareThreads,GPhysicalGBRam);

	FString Args = appCmdLine();
	Args += FString(" -MTCHILD");
	TCHAR ExeName[MAX_FILEPATH_LENGTH];
	GetModuleFileName(NULL, ExeName, MAX_FILEPATH_LENGTH - 1);
	for (INT ProcessIndex = 0; ProcessIndex < NumChildProcesses; ProcessIndex++)
	{
		FChildProcess ProcessInfo;
		ProcessInfo.LogFilename = FFilename(appBaseDir()) * appGameLogDir() * FString::Printf(TEXT("Child%dLaunch.log"),ProcessIndex);
		FString Command = FString::Printf(TEXT("cookpackages %s -TFCSUFFIX=_%d -nopause -unattended -ABSLOG=\"%s\""),*Args,ProcessIndex + 1,*ProcessInfo.LogFilename);
		debugf(TEXT("Job[%d] Launching with Cmd %s"),ProcessIndex, *Command);

		ProcessInfo.ProcessId = 0;
		ProcessInfo.ProcessHandle = appCreateProc(ExeName,*Command,FALSE,TRUE,TRUE,&ProcessInfo.ProcessId,-1);
		if (!ProcessInfo.ProcessHandle || !ProcessInfo.ProcessId)
		{
			appErrorf(TEXT("Failed to start process %s %s"),ExeName,*Command);
		}
		else
		{
			warnf(NAME_Log,TEXT("Job[%d] Launched"),ProcessIndex);
		}
		FString ChildString = FString::Printf(TEXT("Process_%x"),ProcessInfo.ProcessId);
		ProcessInfo.Directory = FFilename(CookedDir) * ChildString;
		debugf(TEXT("  Directory %s"),*ProcessInfo.Directory);
		ProcessInfo.CommandFile = ProcessInfo.Directory * TEXT("command.txt");
		ProcessInfo.StartTime = 0.0;
		ChildProcesses.AddItem(ProcessInfo);

	}
	return TRUE;
}

/**
 * Determine if a given child process is idle or not
 *  Only used if bIsMTMaster is TRUE. 
 *
 * @param Job String representing the filename of the file to cook on the child process
 * @return TRUE if the given child process is idle
 */
UBOOL UCookPackagesCommandlet::ChildIsIdle(INT ProcessIndex)
{
	UBOOL bIdle = !ChildProcesses(ProcessIndex).CommandFile.NonLockingFileExists();
	if (bIdle && ChildProcesses(ProcessIndex).StartTime > 0.0)
	{
		warnf(NAME_Log,TEXT("Job[%d] %s done in %5.1fs"),
			ProcessIndex,
			*ChildProcesses(ProcessIndex).LastCommand,
			FLOAT(appSeconds() - ChildProcesses(ProcessIndex).StartTime));
		ChildProcesses(ProcessIndex).StartTime = 0.0;
		ChildProcesses(ProcessIndex).JobsCompleted++;
	}
	return bIdle;
}

/**
 * Saves our PCD data for a child process
 *  Only used if bIsMTMaster is TRUE. 
 *
 */
void UCookPackagesCommandlet::SavePersistentCookerDataForChild(INT ProcessIndex)
{
	FFilename OutputPCDFilename = ChildProcesses(ProcessIndex).Directory * TEXT("P_") + GetBulkDataContainerFilename();
	check(!PersistentCookerData->NeedsTFCSync()); // these should have all been remapped
	check(!OutputPCDFilename.FileExists());
	PersistentCookerData->SetMinimalSave(TRUE);
	BulletProofPCDSave(PersistentCookerData,*OutputPCDFilename);
	PersistentCookerData->SetMinimalSave(FALSE);

	ChildProcesses(ProcessIndex).LastTFCTextureSaved = CookStats.TFCTextureSaved;
}

/**
 * Checks to see if a child process is waiting for TFC synchronization, and if so, synchronizes the TFC with the child
 *  Only used if bIsMTMaster is TRUE. 
 * @return TRUE if the TFC was synchronized
 *
 */
UBOOL UCookPackagesCommandlet::CheckForTFCSync(INT ProcessIndex)
{
	FFilename CommandFile(ChildProcesses(ProcessIndex).Directory * TEXT("synctfc.txt"));
	UBOOL bNeedsSync = CommandFile.NonLockingFileExists();
	if (bNeedsSync)
	{
		FFilename ChildPCDFile = ChildProcesses(ProcessIndex).Directory * GetBulkDataContainerFilename();
		UPersistentCookerData *ChildCookerData = BulletProofPCDLoad(*ChildPCDFile);
		FString Prefix = FString::Printf(TEXT("Job[%d] %s"),ProcessIndex,*ChildProcesses(ProcessIndex).LastCommand);
		ChildCookerData->TelegraphWarningsAndErrorsFromChild(*Prefix);
		PersistentCookerData->Merge(ChildCookerData, this, *ChildProcesses(ProcessIndex).Directory, ProcessIndex);

		// Flush texture file cache archives to disk.
		for(TMap<FName,FArchive*>::TIterator TextureCacheIt(TextureCacheNameToArMap);TextureCacheIt;++TextureCacheIt)
		{
			TextureCacheIt.Value()->Flush();
		}
		// update TFC entries with new file timestamps
		PersistentCookerData->UpdateCookedTextureFileCacheEntryInfos( TextureCacheNameToFilenameMap );
		ChildCookerData->RemoveFromRoot(); // done with this
		UObject::ResetLoaders( ChildCookerData->GetOutermost() );
		WaitToDeleteFile(*ChildPCDFile);
		SavePersistentCookerDataForChild(ProcessIndex);
		WaitToDeleteFile(*CommandFile);
		CollectGarbageAndVerify();
	}
	return bNeedsSync;
}


/**
 * Low level routine to send a given command string to a child
 * Caller must verify that child is idle
 *  Only used if bIsMTMaster is TRUE. 
 *
 * @param ProcessIndex	Index of child to send command to
 * @param Command		String to send to child
 */
void UCookPackagesCommandlet::SendChildCommand(INT ProcessIndex, const TCHAR *Command)
{
	if (CookStats.TFCTextureSaved - ChildProcesses(ProcessIndex).LastTFCTextureSaved > 10 * 1024 * 1024
		&& ChildProcesses(ProcessIndex).JobsCompleted > 0)  // must have completed at least one job, otherwise it may already need a TFC sync from startup
	{
		// we saved more than 10MB of TFC so the child should load the new PCD to avoid recooking texture
		SavePersistentCookerDataForChild(ProcessIndex);
	}
	ChildProcesses(ProcessIndex).StartTime = appSeconds();
	ChildProcesses(ProcessIndex).LastCommand = *FFilename(Command).GetBaseFilename();
	RobustSaveStringToFile(FString(Command),*ChildProcesses(ProcessIndex).CommandFile);
}

/**
 * Sends stop commands to all child processes that are idle but have not yet been stopped
 *  Only used if bIsMTMaster is TRUE. 
 *
 * @return	TRUE if all children have been sent stop commands
 */
UBOOL UCookPackagesCommandlet::SendStopCommands()
{
	UBOOL AllDone = TRUE;
	CheckForCrashedChildren();
	for (INT ProcessIndex = 0; ProcessIndex < ChildProcesses.Num(); ProcessIndex++)
	{
		if (!ChildProcesses(ProcessIndex).bStopped)
		{
			if (ChildIsIdle(ProcessIndex))
			{
				SendChildCommand(ProcessIndex,TEXT("*stop"));
				ChildProcesses(ProcessIndex).bStopped = TRUE;
			}
			else
			{
				CheckForTFCSync(ProcessIndex);
				AllDone = FALSE;
			}
		}
	}
	return AllDone;
}

/**
 * Merges texture file caches, shader caches, guid caches and global persistent cooker data for a given child process
 *  Only used if bIsMTMaster is TRUE. 
 *
 * @param	ProcessIndex index of the child process to merge
 */
void UCookPackagesCommandlet::MergeChildProducts(INT ProcessIndex)
{
	DOUBLE StartTime = appSeconds();

	FFilename ChildGPCDFile(*(ChildProcesses(ProcessIndex).Directory * GetBulkDataContainerFilename()));

	if (ChildGPCDFile.FileExists())
	{
		// merge script hashes and any final TFC data
		UPersistentCookerData *ChildCookerData = BulletProofPCDLoad(*ChildGPCDFile);
		check(ChildCookerData);
		ChildCookerData->GetCookStats().TotalTime = 0.0;  // we will just use the parent data for this
		ChildCookerData->GetCookStats().AlwaysLoadedProcessingTime = 0.0;  // we will just use the parent data for this
		ChildCookerData->GetCookStats().ContentProcessingTime = 0.0;  // we will just use the parent data for this
		ChildCookerData->GetCookStats().PostPackageIterationTime = 0.0; // child doesn't really do anything here and has not completed the stat
		CookStats += ChildCookerData->GetCookStats();
		PersistentCookerData->Merge(ChildCookerData, this, *ChildProcesses(ProcessIndex).Directory, ProcessIndex);
		// Flush texture file cache archives to disk.
		for(TMap<FName,FArchive*>::TIterator TextureCacheIt(TextureCacheNameToArMap);TextureCacheIt;++TextureCacheIt)
		{
			TextureCacheIt.Value()->Flush();
		}
		// update TFC entries with new file timestamps
		PersistentCookerData->UpdateCookedTextureFileCacheEntryInfos( TextureCacheNameToFilenameMap );
		ChildCookerData->RemoveFromRoot(); // done with this
		UObject::ResetLoaders(ChildCookerData->GetOutermost());
		WaitToDeleteFile(*ChildGPCDFile); 
	}

	// merge guid caches
	FFilename ChildGuidFile(*(ChildProcesses(ProcessIndex).Directory * GetGuidCacheFilename()));
	if (ChildGuidFile.FileExists())
	{
		// create a unique package to not stomp on another one
		UPackage* UniquePackage = UObject::CreatePackage(NULL, *FString::Printf(TEXT("ChildGuidCache_%d"), ProcessIndex));

		// load the persistent data
		UPackage* Package = UObject::LoadPackage(UniquePackage, *ChildGuidFile, LOAD_NoWarn );
		check(Package);

		// merge this into the global one
		UGuidCache* JobGuidCache = FindObject<UGuidCache>(Package, TEXT("GuidCache"));
		check(JobGuidCache);
		GuidCache->Merge(JobGuidCache);
		UObject::ResetLoaders(JobGuidCache->GetOutermost());
		WaitToDeleteFile(*ChildGuidFile); 
	}

	TArray<EShaderPlatform> ShaderPlatforms;
	if (Platform & (UE3::PLATFORM_Windows|UE3::PLATFORM_WindowsConsole))
	{
		ShaderPlatforms.AddItem(SP_PCD3D_SM3);
		if( !ShaderUtils::ShouldForceSM3ShadersOnPC() )
		{
			ShaderPlatforms.AddItem(SP_PCD3D_SM5);
			ShaderPlatforms.AddItem(SP_PCOGL);
		}
	}
	else if (Platform & UE3::PLATFORM_WindowsServer)
	{
		// Cook no shaders
	}
	else
	{
		ShaderPlatforms.AddItem(ShaderPlatform);
	}
	for (INT ShaderPlatformIndex = 0; ShaderPlatformIndex < ShaderPlatforms.Num(); ShaderPlatformIndex++)
	{
		FFilename ShaderFile(*(ChildProcesses(ProcessIndex).Directory * TEXT("ProcessShaderCache-") + ShaderPlatformToText(ShaderPlatforms(ShaderPlatformIndex)) + TEXT(".upk")));
		if (ShaderFile.FileExists())
		{
			// create a unique package to not stomp on another one
			UPackage* UniquePackage = UObject::CreatePackage(NULL, *FString::Printf(TEXT("ChildShaderCache_%d_%d"), ProcessIndex,ShaderPlatformIndex));

			// get the local shader cache to merge into
			UShaderCache* LocalShaderCache = GetLocalShaderCache(ShaderPlatforms(ShaderPlatformIndex));
			check(LocalShaderCache);

			// load the persistent data
			UPackage* Package = UObject::LoadPackage(UniquePackage, *ShaderFile, LOAD_NoWarn);

			// merge this into the global one
			check(Package);
			//warnf(NAME_Log, TEXT("Merging %s into shader cache %s"), *ShaderFile, *LocalShaderCache->GetFullName());
			UShaderCache* JobShaderCache = FindObject<UShaderCache>(Package, TEXT("CacheObject"));
			check(JobShaderCache);

			LocalShaderCache->Merge(JobShaderCache);
			JobShaderCache->RemoveFromRoot();
			UObject::ResetLoaders(JobShaderCache);
			UObject::ResetLoaders(Package);
			WaitToDeleteFile(*ShaderFile); 
		}
	}

	warnf(NAME_Log,TEXT("Job[%d] Finalized %5.1fs"),
		ProcessIndex,
		FLOAT(appSeconds() - StartTime));
	CleanChild(ProcessIndex);
}

/**
 * Deletes all temp files for a given child process...essentially removes the process_XXX directory
 *  Only used if bIsMTMaster is TRUE. 
 *
 * @param	ProcessIndex index of the child process to clean
 */
void UCookPackagesCommandlet::CleanChild(INT ProcessIndex)
{
	// failure is not considered fatal
	GFileManager->DeleteDirectory(*ChildProcesses(ProcessIndex).Directory,FALSE,TRUE);
}

/**
 * Verifies that all child processes that have not been stoppped are still running, appErrors otherwise
 *  Only used if bIsMTMaster is TRUE. 
 *
 */
void UCookPackagesCommandlet::CheckForCrashedChildren()
{
	UBOOL bAnyCrashed = FALSE;
	for (INT ProcessIndex = 0; ProcessIndex < ChildProcesses.Num(); ProcessIndex++)
	{
		if (!ChildProcesses(ProcessIndex).bStopped)
		{
			if (!appIsProcRunning(ChildProcesses(ProcessIndex).ProcessHandle))
			{
				bAnyCrashed = TRUE;
				warnf(NAME_Log, TEXT("Job[%d] Crashed."),ProcessIndex);
			}
		}
	}
	if (bAnyCrashed)
	{
		// terminate everyone
		appSleep(10.0f); // give the autoreporter a chance to work on the crashed child

		for (INT ProcessIndex = 0; ProcessIndex < ChildProcesses.Num(); ProcessIndex++)
		{
			if (!ChildProcesses(ProcessIndex).bStopped)
			{
				ChildProcesses(ProcessIndex).bStopped = TRUE;
				warnf(NAME_Log, TEXT("Job[%d] Killed."),ProcessIndex);
				appTerminateProc(ChildProcesses(ProcessIndex).ProcessHandle);
			}
		}
		FString Reason;
		MergeLogs(&Reason);
		appErrorf(TEXT("Child process crashed: %s"),*Reason);
	}
}

/**
 * Merges the logs from all of the children into my log
 *  Only used if bIsMTMaster is TRUE. 
 *
 */
void UCookPackagesCommandlet::MergeLogs(FString *CriticalReason)
{
	if (GIsBuildMachine)
	{
		if (CriticalReason)
		{
			for (INT ProcessIndex = 0; ProcessIndex < ChildProcesses.Num(); ProcessIndex++)
			{
				if (ChildProcesses(ProcessIndex).LogFilename.FileExists())
				{
					FString Log;
					if (appLoadFileToString(Log,*ChildProcesses(ProcessIndex).LogFilename))
					{
						if (Log.Len())
						{
							const TCHAR *At = appStristr(&Log[0],TEXT("Critical:"));
							if (At)
							{
								const TCHAR *End = appStristr(At,TEXT("\n"));
								TCHAR Message[2048] = {0};
								INT Size = 2048;
								if (End)
								{
									Size = Min<INT>(Size,End-At);
								}
								appStrncpy(Message,At,Size);
								*CriticalReason += FString::Printf(TEXT("%d>%s\r\n"),ProcessIndex,Message);
							}
						}
					}
				}
			}
		}
		return;
	}
	for (INT ProcessIndex = 0; ProcessIndex < ChildProcesses.Num(); ProcessIndex++)
	{
		if (ChildProcesses(ProcessIndex).LogFilename.FileExists())
		{
			FString Log;
			if (appLoadFileToString(Log,*ChildProcesses(ProcessIndex).LogFilename))
			{
				TArray<FString> LineArray;
				Log.ParseIntoArray(&LineArray, TEXT("\n"), TRUE);
				for (INT LineIndex = 0; LineIndex < LineArray.Num(); LineIndex++)
				{
					FString Line = LineArray(LineIndex);
					Line.Replace(TEXT("\r"),TEXT(""));
					debugf(TEXT("%d>%s"),ProcessIndex,*Line);
					if (CriticalReason && Line.InStr(TEXT("Critical:"),FALSE,TRUE) != INDEX_NONE)
					{
						*CriticalReason += FString::Printf(TEXT("%d>%s\r\n"),ProcessIndex,*Line);
					}
				}
			}
		}
	}
}


/**
 * Waits until all children are stopped, merges results and cleans the directories
 *  Only used if bIsMTMaster is TRUE. 
 *
 */
void UCookPackagesCommandlet::StopChildren()
{
	while (!SendStopCommands())
	{
		appSleep(MTSpinDelay);
	}
	for (INT ProcessIndex = 0; ProcessIndex < ChildProcesses.Num(); ProcessIndex++)
	{
		check(ChildProcesses(ProcessIndex).bStopped);
		check(!ChildProcesses(ProcessIndex).bMergedResults);

		appWaitForProc(ChildProcesses(ProcessIndex).ProcessHandle);
		MergeChildProducts(ProcessIndex);
		ChildProcesses(ProcessIndex).bMergedResults = TRUE;
	}
	MergeLogs();
	ChildProcesses.Empty();
}

/**
 * Waits until a child process is available, then gives it the given job
 *  Only used if bIsMTMaster is TRUE. 
 *
 */
void UCookPackagesCommandlet::StartChildJob(const FFilename &Job)
{
	while (1)
	{
		UBOOL Sent = FALSE;
		static INT LastSent = -1;
		for (INT ProcessIndex = LastSent + 1; ProcessIndex < ChildProcesses.Num(); ProcessIndex++)
		{
			if (ChildIsIdle(ProcessIndex))
			{
				SendChildCommand(ProcessIndex,*Job);
				LastSent = ProcessIndex;
				Sent = TRUE;
				break;
			}
		}
		if (!Sent)
		{
			for (INT ProcessIndex = 0; ProcessIndex <= LastSent && ProcessIndex < ChildProcesses.Num(); ProcessIndex++)
			{
				if (ChildIsIdle(ProcessIndex))
		{
					SendChildCommand(ProcessIndex,*Job);
					LastSent = ProcessIndex;
					Sent = TRUE;
					break;
				}
			}
		}
		if (Sent)
		{
			break;
		}
		UBOOL Sync = FALSE;
		if (ChildProcesses.Num() > 6)
		{
			static INT LastSync = -1;

			INT NumSync = 0;
			INT PrevLastSync = LastSync;
			const INT ToDoPerTurn = 2;

			for (INT ProcessIndex = PrevLastSync + 1; ProcessIndex < ChildProcesses.Num() && NumSync < ToDoPerTurn; ProcessIndex++)
			{
				LastSync = ProcessIndex;
				if (CheckForTFCSync(ProcessIndex))
				{
					Sync = TRUE;
					NumSync++;
				}
			}
			if (NumSync < ToDoPerTurn)
			{
				for (INT ProcessIndex = 0; ProcessIndex <= PrevLastSync && ProcessIndex < ChildProcesses.Num() && NumSync < ToDoPerTurn; ProcessIndex++)
				{
					LastSync = ProcessIndex;
					if (CheckForTFCSync(ProcessIndex))
					{
						Sync = TRUE;
						NumSync++;
					}
				}
			}
		}
		else
		{
			for (INT ProcessIndex = 0; ProcessIndex < ChildProcesses.Num(); ProcessIndex++)
			{
				if (CheckForTFCSync(ProcessIndex))
				{
					Sync = TRUE;
				}
			}
		}
		if (!Sync)
		{
			appSleep(MTSpinDelay);
		}
		CheckForCrashedChildren();
	}
}

/**
 * Called by a child process to signal is has completed a job (or the stop command)
 *  Only used if bIsMTChild is TRUE. 
 *
 */
void UCookPackagesCommandlet::JobCompleted()
{
	FFilename CommandFile(CookedDirForPerProcessData * TEXT("command.txt"));
	WaitToDeleteFile(*CommandFile);
}

/**
 * Called by a child process to wait for the next command and return it
 *  Only used if bIsMTChild is TRUE. 
 *
 * @return	The next job. Either a package name to cook or the empty filename if this is the stop command
 */
FFilename UCookPackagesCommandlet::GetMyNextJob()
{
	FFilename CommandFile(CookedDirForPerProcessData * TEXT("command.txt"));
	FString Command;
	DOUBLE StartTime = appSeconds();
	while (!appLoadFileToString(Command,*CommandFile))
	{
		appSleep(MTSpinDelay);
		if (appSeconds() - StartTime > MTTimeout)
		{
			appErrorf(TEXT("Waited more that 60 seconds for another job, assume parent is dead, killing myself."));
		}
	}
	if (Command.StartsWith(FString(TEXT("*stop"))))
	{
		Command.Empty();
	}
	else
	{
		LoadParentPersistentCookerData();
	}
	return FFilename(Command);
}

/**
 * Called by a child process to load the PCD the parent saved for us
 * Done both when synchronizing TFCs and before each job
 */
void UCookPackagesCommandlet::LoadParentPersistentCookerData()
{
	FFilename ParentPersistentCookerDataFilename = CookedDirForPerProcessData + TEXT("P_") + GetBulkDataContainerFilename();
	if (ParentPersistentCookerDataFilename.FileExists()) // sometimes the parent doesn't give us one because it doesn't have enough new to bother
	{
		UPersistentCookerData* NewPersistentCookerData = BulletProofPCDLoad(*ParentPersistentCookerDataFilename);
		PersistentCookerData->CopyInfo(NewPersistentCookerData);
		NewPersistentCookerData->RemoveFromRoot();
		UObject::ResetLoaders(NewPersistentCookerData->GetOutermost());
		NewPersistentCookerData = 0;
		WaitToDeleteFile(*ParentPersistentCookerDataFilename);
	}
}

/**
 * Called by a child process prior to saving a cooked package
 *  Writes out the global persistent cooker data
 *  Waits for the master process to digest the TFC infor and product a new PCD
 *  Loads new PCD and fixes up texture offsets
 *  Only used if bIsMTChild is TRUE. 
 *
 */
void UCookPackagesCommandlet::SynchronizeTFCs()
{
	if (!PersistentCookerData->NeedsTFCSync())
	{
		return;
	}
	// Flush texture file cache archives to disk.
	for(TMap<FName,FArchive*>::TIterator TextureCacheIt(TextureCacheNameToArMap);TextureCacheIt;++TextureCacheIt)
	{
		delete TextureCacheIt.Value();
	}
	TextureCacheNameToArMap.Empty();

	PersistentCookerData->PackWarningsAndErrorsForParent();


	FFilename PersistentCookerDataFilename = CookedDirForPerProcessData + GetBulkDataContainerFilename();
	PersistentCookerData->SetMinimalSave(TRUE);
	BulletProofPCDSave(PersistentCookerData,*PersistentCookerDataFilename);
	PersistentCookerData->SetMinimalSave(FALSE);

	FFilename CommandFile(CookedDirForPerProcessData * TEXT("synctfc.txt"));
	check(!CommandFile.NonLockingFileExists());
	FFilename ParentPersistentCookerDataFilename = CookedDirForPerProcessData + TEXT("P_") + GetBulkDataContainerFilename();
	check(!ParentPersistentCookerDataFilename.FileExists());
	FString Command(TEXT("synctfc"));
	RobustSaveStringToFile(Command,*CommandFile);
	DOUBLE StartTime = appSeconds();
	appSleep(MTSpinDelay * 5.0f);  // spin for parent to merge tfcs
	while (CommandFile.NonLockingFileExists() || !ParentPersistentCookerDataFilename.NonLockingFileExists())
	{
		appSleep(MTSpinDelay);  // spin for parent to merge tfcs
		if (appSeconds() - StartTime > MTTimeout)
		{
			appErrorf(TEXT("Waited more that 60 seconds for another job, assume parent is dead, killing myself."));
		}
	}
	LoadParentPersistentCookerData();
	// Iterate over all objects in passed in outer and store the bulk data info for types supported.
	for( TObjectIterator<UObject> It; It; ++It )
	{
		UObject* Object = *It;
		// Texture handling.
		UTexture2D* Texture2D = Cast<UTexture2D>( Object );		
		if( Texture2D )
		{
			if (Platform != PLATFORM_Android)
			{
				SynchronizeTFCMips(Texture2D, Texture2D->Mips);
			}
			else
			{			
				if (AndroidTextureFormat & TEXSUPPORT_DXT)
				{
					SynchronizeTFCMips(Texture2D, Texture2D->Mips, TEXSUPPORT_DXT);
				}
				if (AndroidTextureFormat & TEXSUPPORT_PVRTC)
				{
					SynchronizeTFCMips(Texture2D, Texture2D->CachedPVRTCMips, TEXSUPPORT_PVRTC);
				}
				if (AndroidTextureFormat & TEXSUPPORT_ATITC)
				{
					SynchronizeTFCMips(Texture2D, Texture2D->CachedATITCMips, TEXSUPPORT_ATITC);
				}
				if (AndroidTextureFormat & TEXSUPPORT_ETC)
				{
					SynchronizeTFCMips(Texture2D, Texture2D->CachedETCMips, TEXSUPPORT_ETC);
				}
			}
		}
	}
}

/**
 * Helper function for SynchronizeTFCs to synchronize the bulk data for a set of mips
 *
 * @param	Texture2D		the Texture to operate on
 * @param	Mips			Reference to the actual collection of generated mips
 * @param	TextureFormat	Which format is the mip data is in
 */
void UCookPackagesCommandlet::SynchronizeTFCMips( UTexture2D* Texture2D, TIndirectArray<FTexture2DMipMap> &Mips, ETextureFormatSupport TextureFormat )
{
	for( INT MipLevel=0; MipLevel<Mips.Num(); MipLevel++ )
	{
		FTexture2DMipMap& Mip = Mips(MipLevel);

		FString MipPart;
		if (PLATFORM_Android != PLATFORM_Android)
		{
			MipPart = FString::Printf(TEXT("MipLevel_%i"),MipLevel);
		}
		else
		{
			MipPart = FString::Printf(TEXT("MipLevel_%i"),MipLevel) + GetAndroidTextureFormatName(TextureFormat);
		}

		FCookedBulkDataInfo* Info = PersistentCookerData->GetBulkDataInfo( Texture2D, *MipPart );

		if (Info)
		{
			UBOOL bOffsetChanged = Mip.Data.GetSavedBulkDataOffsetInFile() != Info->SavedBulkDataOffsetInFile;
			UBOOL bCompressedSizeChanged = Mip.Data.GetSavedBulkDataSizeOnDisk() >= 0 && Mip.Data.GetSavedBulkDataSizeOnDisk() != Info->SavedBulkDataSizeOnDisk;
			if (bOffsetChanged || bCompressedSizeChanged)
			{
				Mip.Data.SetSavedBulkDataOffsetInFile(Info->SavedBulkDataOffsetInFile);
				if (bCompressedSizeChanged)
				{
					warnf(NAME_Warning,TEXT("Compressor appears to be non-deterministic (%d vs %d compressed bytes) for mip %d texture %s"),
						Mip.Data.GetSavedBulkDataSizeOnDisk(),
						Info->SavedBulkDataSizeOnDisk,
						MipLevel,
						*Texture2D->GetFullName()
						);
					Mip.Data.SetSavedBulkDataSizeOnDisk(Info->SavedBulkDataSizeOnDisk);
				}
			}
		}
	}
}

/**
 * Main entry point for commandlets.
 */
INT UCookPackagesCommandlet::Main( const FString& Params )
{
	CookStats.TotalTime = appSeconds();
	STAT(CookStats.PrePackageIterationTime -= appSeconds());

	// make sure the platform was set in CreateCustomEngine
	if ( Platform == PLATFORM_Unknown )
	{
		return 1;
	}

	// Set compression method based on platform.
	if ((Platform & UE3::PLATFORM_PS3) || (Platform == PLATFORM_NGP) || (Platform == PLATFORM_WiiU) || Platform == PLATFORM_Android)
	{
		// Zlib uses SPU tasks on PS3.
		GBaseCompressionMethod = COMPRESS_ZLIB;
	}
	else if (Platform & UE3::PLATFORM_Xbox360)
	{
		// LZX is best trade-off of perf/ compression size on Xbox 360
		GBaseCompressionMethod = COMPRESS_LZX;
	}
	else if (Platform == PLATFORM_IPhone || Platform == PLATFORM_Flash)
	{
		// @todo ib2merge: Chair set this to Zlib for their small package compression, is it safe to always put to 
		// ZLIB, since package compression is off? This could compress mip data, which has a load time impact 
		GBaseCompressionMethod = COMPRESS_None;
	}

	// Other platforms default to what the PC uses by default.

	// Bind to cooking DLLs
//  	if( !BindDLLs() )
// 	{
// 		warnf( TEXT( "Error: Failed to bind to console support dll; aborting." ) );
// 		return 1;
// 	}

	// Parse command line and perform any initialization that isn't per package.
	UBOOL bQuitEvenOnSuccess = FALSE;
	UBOOL bInitWasSuccessful = Init( *Params, bQuitEvenOnSuccess );
	if (!bInitWasSuccessful || bQuitEvenOnSuccess)
	{
		// make sure we delete any copied shader files
		CleanupShaderFiles();
		return bInitWasSuccessful ? 0 : 1;
	}

	// We don't cook combined startup packages in user build, so manually mark objects before we do any cooking.
	if ( DLCName.Len() )
	{
		for( FObjectIterator It; It; ++It )
		{
			It->SetFlags( RF_MarkedByCooker );
		}
	}

	// if we only wanted to cook inis, then skip over package processing
	if (!bIniFilesOnly)
	{
		// Generate list of packages to cook.
		INT FirstStartupIndex		= INDEX_NONE;
		INT FirstScriptIndex		= INDEX_NONE;
		INT FirstMapIndex			= INDEX_NONE;
		INT FirstGameScriptIndex	= INDEX_NONE;
		TArray<FPackageCookerInfo> PackageList = GeneratePackageList( FirstStartupIndex, FirstScriptIndex, FirstGameScriptIndex, FirstMapIndex );

		TArray<FString> AlwaysLoaded;
		TArray<UBOOL> AlwaysLoadedWasSaved;
		TMap<FString,TArray<FString> > AlwaysLoadedLoc;
		GetAlwaysLoadedPackageList(AlwaysLoaded, AlwaysLoadedLoc);
		AlwaysLoadedWasSaved.Empty();
		AlwaysLoadedWasSaved.AddZeroed(AlwaysLoaded.Num());

		// find out if the target platform wants to fully load packages from memory at startup (fully compressed)
		UBOOL bFullyCompressStartupPackages = FALSE;
		GConfig->GetBool(TEXT("Engine.StartupPackages"), TEXT("bFullyCompressStartupPackages"), bFullyCompressStartupPackages, PlatformEngineConfigFilename);

		// See if INT resources should be subbed in for missing localized resources...
		UBOOL bSubstituteINTPackagesForMissingLoc = FALSE;
		GConfig->GetBool(TEXT("Cooker.LocalizationOptions"), TEXT("bSubstituteINTPackagesForMissingLoc"), bSubstituteINTPackagesForMissingLoc, GEditorIni);
		if (bSubstituteINTPackagesForMissingLoc == TRUE)
		{
			warnf(NAME_Log, TEXT("INT resources will be substituted for missing LOC packages"));
		}

		// Initialize the material cleanup settings
		InitializeMaterialCleanupSettings();

		// create the combined startup package
		FString StartupPackageName = FString(TEXT("Startup"));

		UPackage* CombinedStartupPackage = UObject::CreatePackage(NULL, *StartupPackageName);
		CombinedStartupPackage->PackageFlags |= PKG_ServerSideOnly;
		// make sure this package isn't GC'd
		CombinedStartupPackage->AddToRoot();
		// give the package a guid
		CombinedStartupPackage->MakeNewGuid();


		// create an object to reference all the objects in the startup packages to be combined together
		UObjectReferencer* CombinedStartupReferencer = ConstructObject<UObjectReferencer>( UObjectReferencer::StaticClass(), CombinedStartupPackage, NAME_None, RF_Cooked );
		// make sure this referencer isn't GC'd
		CombinedStartupReferencer->AddToRoot();

		// read in the per-map packages to cook
		TMap<FName, TArray<FName> > PerMapCookPackages;
		GConfig->Parse1ToNSectionOfNames(TEXT("Engine.PackagesToForceCookPerMap"), TEXT("Map"), TEXT("Package"), PerMapCookPackages, PlatformEngineConfigFilename);

		const UBOOL bCookingPS3Mod = ( DLCName.Len() && Platform == PLATFORM_PS3 );

		if (!bIsMTChild)
		{
			// save out the startup shaders
			SaveLocalShaderCaches();
		}
		UBOOL QuickAndDirty = FALSE;
		if (ParseParam(appCmdLine(), TEXT("QuickAndDirty")))
		{
			QuickAndDirty = TRUE;
		}

		FFilename NextChildName;
		INT NextChildIndex = -1;
		INT LastStartupIndex = -1;
		for(INT PackageIndexInner = 0; PackageIndexInner<PackageList.Num(); PackageIndexInner++ )
		{
			if (PackageList(PackageIndexInner).bIsCombinedStartupPackage)
			{
				LastStartupIndex = PackageIndexInner;
			}
		}

		if (bSkipStartupObjectDetermination == FALSE)
		{
			// Never let the PCD get saved into a package that isn't its own...
			PersistentCookerData->SetFlags(RF_MarkedByCooker);
		}

		// Two processing passes are made over the packages.
		//
		// If bSkipStartupObjectDetermination is TRUE, the first one does nothing...
		//
		// Otherwise, the first pass will process the 'always loaded' packages. 
		// These will include Core, Engine, GameFramework, IpDrv, OnlineSubsystem*,
		// <yourgame>, and the 'STARTUP' package. 
		// Lists of the objects that get saved in these packages will be stored
		// in the PersistentCookerData.
		//
		// The second pass will process the remaining packages, properly marking
		// objects that were saved into the 'always loaded' set rather than
		// blindly assuming that everything loaded at that point was saved.
		//
		for (INT ProcessingPass = 0; ProcessingPass < 2; ProcessingPass++)
		{
			DOUBLE PassStartTime = appSeconds();

			UBOOL bWasMTMaster = bIsMTMaster;
			if (ProcessingPass == 0)
			{
				if (bSkipStartupObjectDetermination == TRUE)
				{
					// If this is the first pass, but we are skipping the startup
					// object determination than skip it.
					continue;
				}

				if (bIsMTChild == TRUE)
				{
					// Children never do the first pass
					continue;
				}

				// Clear the bIsMTMaster on the first pass to allow for the
				// cooking/saving of 'always loaded' packages w/out having
				// to avoid starting up Children during it...
				bIsMTMaster = FALSE;
			}
			else
			{
				if (bSkipStartupObjectDetermination == FALSE)
				{
					// Tag all the known cooked startup objects.
					// This is to cover both iterative and child cooks.
					TagCookedStartupObjects();
				}
			}

			if (bIsMTMaster)
			{
				bSaveShaderCacheAtEnd = TRUE; // children will load our shader cache, so we cannot write during iteration
				INT NumJobs = 0;
				for( INT PackageIndex=Max<INT>(0,LastStartupIndex); PackageIndex < PackageList.Num() ; PackageIndex++ )
				{
					if (QuickAndDirty && PackageIndex > LastStartupIndex)
					{
						break;
					}
					// Skip maps not required for cooking process. 
					if( !(bSkipNotRequiredPackages && (PackageIndex < FirstScriptIndex)) 
						&& !( bUseTextureFileCache && (PackageIndex < FirstScriptIndex)  && !PackageList(PackageIndex).bIsOtherRequired)
						&& !PackageList(PackageIndex).bShouldOnlyLoad)
					{
//						warnf(NAME_Log,TEXT("pkg %s"),*PackageList(PackageIndex).DstFilename);
						NumJobs++;
					}
				}
				if (!StartChildren(NumJobs))
				{
					bIsMTMaster = FALSE;
				}
			}
			if (bIsMTChild)
			{
				FPackageCookerInfo Temp;
				PackageList.AddItem(Temp); // add a dummy item so we can't terminate the loop without a stop command
			}

			STAT(CookStats.PrePackageIterationTime += appSeconds());

			TArray<FString> PersistentMapList;
			PersistentMapInfoHelper.GetPersistentMapList( PersistentMapList );

			if (ProcessingPass == 1)
			{
				GMobileShaderCooker.StartMaterialList();
			}

			// Collect the mobile shaders for the startup packages (last pass only, once everything has been marked up with RF_CookedStartupObject).
			if ( ProcessingPass == 1 )
			{
				GMobileShaderCooker.SetupStartupShaderGroup();
			}

			// Iterate over all packages.
			//@script patcher (bAbortCooking)
			for( INT PackageIndex=0; !bAbortCooking && PackageIndex<PackageList.Num(); PackageIndex++ )
			{
				SCOPE_SECONDS_COUNTER_COOK(PackageIterationTime);

				if (bSkipStartupObjectDetermination == FALSE)
				{
					if (ProcessingPass == 0)
					{
						// See if we are past the last startup index...
						if (PackageIndex > LastStartupIndex)
						{
							break;
						}
					}
				}

				// Skip maps not required for cooking process. This is skipping packages that are only going to contain
				// higher miplevels of textures.				
				if( bSkipNotRequiredPackages && (PackageIndex < FirstScriptIndex) )
				{
					continue;
				}
				// Skip non-required packages if cooking with TFC 
				// since only referenced textures need to be updated into the TFC file
				if( bUseTextureFileCache && (PackageIndex < FirstScriptIndex)  && !PackageList(PackageIndex).bIsOtherRequired)
				{
					continue;
				}
				if (!bIsMTChild && QuickAndDirty && PackageIndex > LastStartupIndex)
				{
					break;
				}
				if (bIsMTMaster)
				{
					if (!PackageList(PackageIndex).bShouldOnlyLoad)
					{
						if (PackageIndex > LastStartupIndex)
						{
							StartChildJob(PackageList(PackageIndex).DstFilename);
						}
						else
						{
							if (bSkipStartupObjectDetermination == TRUE)
							{
								// one job will do all of the startup packages
								StartChildJob(TEXT("*startup"));
							}
							PackageIndex = LastStartupIndex;
						}

					}
					continue;
				}
				if (bIsMTChild)
				{
					if (NextChildIndex == -1)
					{
						NextChildName = GetMyNextJob();
						if (!NextChildName.Len())
						{
							break; // stop request
						}
						if (NextChildName.StartsWith(TEXT("*startup")))
						{
							// do all of the startup packages
							check(LastStartupIndex >= 0);
							NextChildIndex = LastStartupIndex;
						}
						else
						{
							// find a specific package to do
							for(INT PackageIndexInner = LastStartupIndex + 1; PackageIndexInner<PackageList.Num(); PackageIndexInner++ )
							{
								if (NextChildName == PackageList(PackageIndexInner).DstFilename)
								{
									NextChildIndex = PackageIndexInner;
									break;
								}
							}

						}
					}
					if (NextChildIndex < 0)
					{
						appErrorf(TEXT("Child job asked to process %s, but it was not in the cook list."),*NextChildName);
						break;  // quit
					}
					if (PackageIndex > LastStartupIndex )
					{
						PackageIndex = NextChildIndex; // we are done with startup, so just skip to wherever we need to go
					}
				}

				const FFilename& SrcFilename			= PackageList(PackageIndex).SrcFilename;
				FFilename DstFilename					= PackageList(PackageIndex).DstFilename;
				UBOOL bShouldBeSeekFree					= PackageList(PackageIndex).bShouldBeSeekFree;
				UBOOL bIsNativeScriptFile				= PackageList(PackageIndex).bIsNativeScriptFile;
				UBOOL bIsCombinedStartupPackage			= PackageList(PackageIndex).bIsCombinedStartupPackage;
				UBOOL bIsStandaloneSeekFreePackage		= PackageList(PackageIndex).bIsStandaloneSeekFreePackage;
				UBOOL bShouldOnlyLoad					= PackageList(PackageIndex).bShouldOnlyLoad;
				UBOOL bIsNonNativeScriptFile			= PackageList(PackageIndex).bIsNonNativeScriptFile;
				UBOOL bShouldBeFullyCompressed			= bFullyCompressStartupPackages && (bIsNativeScriptFile || bIsCombinedStartupPackage);
				FString PackageName						= SrcFilename.GetBaseFilename( TRUE );

				if (bIsMTChild)
				{
					if ( PackageIndex < NextChildIndex && NextChildIndex != LastStartupIndex)
					{
						// this is a startup package and we aren't going to cook it, so just load and mark it
						bShouldOnlyLoad = TRUE; 
					}
				}
				else
				{
					if (bSkipStartupObjectDetermination == FALSE)
					{
						if ((bIsMTMaster == FALSE) && (ProcessingPass == 1))
						{
							if (PackageIndex <= LastStartupIndex)
							{
								// this is a startup package on the second pass and we aren't going to cook it, so just load and mark it
								continue;
							}
						}
					}
				}

				// determine if we need to generate SHA information for the script code in the package:
				//		- PS3 only for now (TRC requirement)
				//		- don't do native script packages, they have SHA on the whole file, and class write order is different than class read order
				//		- don't do the combined startup (that gets SHA on whole file as well)
				//		- seekfree packages only
				// if you change this logic, a full recook and hashes.sha regeneration is suggested
				if (Platform == PLATFORM_PS3 && !bIsNativeScriptFile && bShouldBeSeekFree && !bIsCombinedStartupPackage)
				{
					ULinkerSave::PackagesToScriptSHAMap.Set(bIsCombinedStartupPackage ? CombinedStartupPackage->GetName() : PackageName, TArray<BYTE>());
				}

				STAT(CookStats.PrepPackageTime -= appSeconds());

				// Collect garbage and verify it worked as expected.	
				CollectGarbageAndVerify();

				if (bShouldOnlyLoad == FALSE)
				{
					// Mark startup objects 
					MarkCookedStartupObjects(TRUE);
				}

				// Save the shader caches after each package has been verified.
				if (!bSaveShaderCacheAtEnd)
				{
					SCOPE_SECONDS_COUNTER_COOK(SaveShaderCacheTime);
					SaveLocalShaderCaches();
				}

				STAT(CookStats.PrepPackageTime += appSeconds());
				STAT(CookStats.InitializePackageTime -= appSeconds());

				// When cooking mods for console, we want to enforce constraints on what mods can and cannot include.
				// Classify the specified package to determine if it's part of the mod 'set' by having been
				// specified on the cooker command line.
				UBOOL bIsTokenMap = FALSE;
				UBOOL bIsTokenScriptPackage = FALSE;
				UBOOL bIsTokenContentPackage = FALSE;
				UBOOL bIsTokenPackage = FALSE;
				if ( bCookingPS3Mod )
				{
					bIsTokenMap = TokenMaps.ContainsItem(SrcFilename);
					bIsTokenScriptPackage = TokenScriptPackages.ContainsItem(SrcFilename);
					bIsTokenContentPackage = TokenContentPackages.ContainsItem(SrcFilename);
					bIsTokenPackage = bIsTokenMap | bIsTokenScriptPackage | bIsTokenContentPackage;
				}

				// for PC and Mac we don't strip out non-texture objects
				UBOOL bStripEverythingButTextures	= (PackageIndex < FirstScriptIndex) && (Platform != PLATFORM_Windows) && (Platform != PLATFORM_MacOSX);
		
				UPackage* Package = NULL;
				UObjectReferencer* StandaloneSeekFreeReferencer = NULL;

				// TRUE if after processing this package, we should mark all objects with RF_MarkedByCooker, and mark
				// materials as already handled
				UBOOL bMarkObjectsAfterProcessing = FALSE;

				// handle combining several packages into one
				if (bIsCombinedStartupPackage)
				{
					SCOPE_SECONDS_COUNTER_COOK(LoadCombinedStartupPackagesTime);

					Package = CombinedStartupPackage;
					PackageName = StartupPackageName;
					DstFilename = CookedDir + StartupPackageName + GetCookedPackageExtension(Platform);

					if (bShouldOnlyLoad)
					{
						warnf( NAME_Log, TEXT("Loading startup packages:"), *PackageName); 
					}
					else
					{
						warnf( NAME_Log, TEXT("Cooking [Combined] %s with:"), *PackageName); 
						if (bIsMTChild) // special case, we need to not add this unless we are actually cooking it
						{
							GuidCache->SetPackageGuid(CombinedStartupPackage->GetFName(), CombinedStartupPackage->GetGuid());
						}
					}

					// load all the combined startup packages (they'll be in a row)
					for (; PackageIndex < PackageList.Num() && PackageList(PackageIndex).bIsCombinedStartupPackage; PackageIndex++)
					{
						const FFilename& Filename = PackageList(PackageIndex).SrcFilename;
						warnf( NAME_Log, TEXT("   %s"), *Filename.GetBaseFilename()); 

						// load the package and all objects inside
						// get base package name, including stripping off the language
						FString BasePackageName = PackageList(PackageIndex).SrcFilename.GetBaseFilename();
						FString LangExt = FString(TEXT("_")) + UObject::GetLanguage();
						if (BasePackageName.Right(LangExt.Len()) == LangExt)
						{
							BasePackageName = BasePackageName.Left(BasePackageName.Len() - LangExt.Len());
						}
						UPackage* StartupPackage = LoadPackageForCooking(*BasePackageName);
						if ( StartupPackage == NULL )
						{
							return 1;
						}

						// mark anything inside as unmarked, for any objects already loaded (like EngineMaterials.WireframeMaterial, etc)
						for (FObjectIterator It; It; ++It)
						{
							if (It->IsIn(StartupPackage))
							{
								It->ClearFlags(RF_MarkedByCooker);

								// only store objects whose outer is a package, to reduce the size of the array, and to
								// help with editoronly array properties, as they are stripped, but the Inner is not (this
								// only applies to putting a script package into the startup package)
								if (It->IsA(UField::StaticClass()) || It->IsA(UDistributionFloat::StaticClass()) || It->IsA(UDistributionVector::StaticClass()))
								{
									continue;
								}

								UTexture2D* Texture = Cast<UTexture2D>(*It);
								if( Texture && Texture->LODGroup == TEXTUREGROUP_MobileFlattened && (Platform & PLATFORM_Mobile) == 0 )
								{
									// Skip any flattened textures in startup packages when not cooking for mobile platforms
									continue;
								}


								// reference it so SavePackage will save it
								CombinedStartupReferencer->ReferencedObjects.AddItem(*It);
							}
						}
					}

					// decrement the PackageIndex as the outer for loop will jump to the next package as well
					PackageIndex--;

					// after saving, mark the objects
					bMarkObjectsAfterProcessing = TRUE;
				}
				else if (bIsStandaloneSeekFreePackage)
				{
					SCOPE_SECONDS_COUNTER_COOK(LoadPackagesTime);

					// open the source package
					UPackage* SourcePackage = LoadPackageForCooking(*SrcFilename);
					if ( SourcePackage == NULL )
					{
						return 1;
					}

					// when in user mode, just skip already cooked packages
				if (DLCName.Len() && (SourcePackage->PackageFlags & PKG_Cooked))
					{
						warnf(NAME_Log, TEXT("  Skipping, already cooked")); 
						continue;
					}

					Package = SourcePackage;

					if ( !bCookingPS3Mod )
					{
						// Don't append the seekfree suffix when cooking non-map, content-only mods.
						const FString CookedPackageName = (DLCName.Len() && !bIsTokenMap && !bIsTokenScriptPackage && bIsTokenContentPackage) ? Package->GetName() : Package->GetName() + STANDALONE_SEEKFREE_SUFFIX;
						warnf( NAME_Log, TEXT("Cooking [Seekfree Standalone] %s from %s"), *CookedPackageName, *PackageName);

						// create the new package to cook into
						Package = UObject::CreatePackage(NULL, *CookedPackageName);
						
						// this package needs a new guid, because it's cooked and so SavePackage won't create a new guid for it, expecting it to have a 
						// come from some source linker to get it from, that's not the case
						Package->MakeNewGuid();
						// copy networking package flags from original
						Package->PackageFlags |= SourcePackage->PackageFlags & (PKG_AllowDownload | PKG_ClientOptional | PKG_ServerSideOnly);
						if (!(Package->PackageFlags & PKG_ServerSideOnly))
						{
							Package->CreateEmptyNetInfo();
						}

						// fix up the destination filename
						DstFilename = DstFilename.Replace(*(PackageName + TEXT(".")), *(CookedPackageName + TEXT(".")));
						
						//create an object to reference all the objects in the original package
						StandaloneSeekFreeReferencer = ConstructObject<UObjectReferencer>( UObjectReferencer::StaticClass(), Package, NAME_None, RF_Cooked );

						for (FObjectIterator It; It; ++It)
						{
							if (It->IsIn(SourcePackage))
							{
								// reference it so SavePackage will save it
								StandaloneSeekFreeReferencer->ReferencedObjects.AddItem(*It);
							}
						}
					}
					else
					{
						warnf( NAME_Log, TEXT("Cooking [Seekfree Standalone] %s"), *PackageName);
						for (FObjectIterator It; It; ++It)
						{
							if (It->IsIn(SourcePackage))
							{
								// Mark the object as RF_Standalone so SavePackge will save it.
								It->SetFlags( RF_Standalone );
							}
						}
					}
				}
				else
				{
					SCOPE_SECONDS_COUNTER_COOK(LoadPackagesTime);

					warnf( NAME_Log, TEXT("Cooking%s%s %s"), 
						bShouldBeSeekFree ? TEXT(" [Seekfree]") : TEXT(""),
						bShouldBeFullyCompressed ? TEXT(" [Fully compressed]") : TEXT(""),
						*SrcFilename.GetBaseFilename() );

					Package = LoadPackageForCooking(*SrcFilename);
				}

				STAT(CookStats.InitializePackageTime += appSeconds());
				STAT(CookStats.FinalizePackageTime -= appSeconds());

				if (Package && (!bShouldOnlyLoad || !bQuickMode) )
				{
					// when in user mode, just skip already cooked packages
					if (DLCName.Len() && (Package->PackageFlags & PKG_Cooked))
					{
						warnf(NAME_Log, TEXT("  Skipping, already cooked")); 
						continue;
					}

					if (ProcessingPass == 1)
					{
						// If this is a package that will be saved, mark cooked startup objects...
						if (bShouldOnlyLoad == FALSE)
						{
							MarkCookedStartupObjects(TRUE);
						}
					}

#if WITH_GFx
					// GFx packages need to contain textures and swf data, until the editor
					// places swf textures in a separate package.
					UBOOL   bIsGFxPackage = FALSE;

					if (bStripEverythingButTextures)
					{
						for( FObjectIterator It; It; ++It )
						{
							UObject* Object = *It;
							if( Object->IsIn( Package ) && Object->IsA(USwfMovie::StaticClass()) )
							{
								bStripEverythingButTextures = FALSE;
								bShouldBeSeekFree = TRUE;
								bIsGFxPackage = TRUE;
								break;
							}
						}
					}
#endif
					DWORD OriginalPackageFlags = Package->PackageFlags;

					// Don't try cooking already cooked data!
					checkf( !(Package->PackageFlags & PKG_Cooked), TEXT("%s is already cooked!"), *SrcFilename ); 
					Package->PackageFlags |= PKG_Cooked;

					// Is this an always loaded package?
					INT AlwaysLoadedIdx = INDEX_NONE;
					if (bSkipStartupObjectDetermination == FALSE)
					{
						if (AlwaysLoaded.FindItem(DstFilename.GetBaseFilename(), AlwaysLoadedIdx) == TRUE)
						{
							check(AlwaysLoadedIdx < AlwaysLoaded.Num());
						}
					}

					// Find the UWorld object (only valid if we're looking at a map).
					UWorld* World = FindObject<UWorld>( Package, TEXT("TheWorld") );

					// Reset lightmap type to default
					PreferredLightmapType = EPLT_Directional;

					// Initialize components, so they have correct bounds, Owners and such
					if (World != NULL)
					{
						AWorldInfo* WorldInfo = World->GetWorldInfo();
						if (WorldInfo != NULL)
						{
							if ((Platform & PLATFORM_Mobile) != 0)
							{
								//mobile-not-flash is always simple
								PreferredLightmapType = EPLT_Simple;
								if ( ((Platform & PLATFORM_Flash) != 0) && GSystemSettings.bAllowDirectionalLightMaps )
								{
									//set directional if specified in the ini
									PreferredLightmapType = EPLT_Directional;
								}
							}
						}
						if (World->PersistentLevel != NULL)
						{
							// Reset cooked physics data counters.
							BrushPhysDataBytes = 0;

							World->PersistentLevel->UpdateComponents();

							// Only warn about lighting quality issues on the build machine.
							if( GIsBuildMachine )
							{
								ELightingBuildQuality LightingQuality = (ELightingBuildQuality)World->GetWorldInfo()->LevelLightingQuality;
								// Warn about maps not being built with production quality lighting.  To prevent old maps that were built before this feature, we check to make sure their lighting quality is valid before warning.
								if( LightingQuality != Quality_Production && LightingQuality != Quality_MAX)
								{
									FString PackageName = World->GetWorldInfo()->GetOutermost()->GetName();
									// Only warn about persistent levels.
									if( PersistentMapList.ContainsItem( PackageName ) )
									{
										// Note: Using "report" so builder emails display this.
										warnf( TEXT("[WARNING] %s was built without production quality lighting!"), *PackageName );
									}
								}
							}
						}
					}

					//Extern from MobileSupport.cpp
					extern void SetPreferredLightmapType (EPreferredLightmapType InLightmapType);
					SetPreferredLightmapType (PreferredLightmapType);

					if (bShouldBeSeekFree && World != NULL)
					{
						// look up per-map packages
						TArray<FName>* PerMapPackages = PerMapCookPackages.Find(Package->GetFName());

						if (PerMapPackages != NULL)
						{
							SCOPE_SECONDS_COUNTER_COOK(LoadPerMapPackagesTime);

							for (INT PerMapPackageIndex = 0; PerMapPackageIndex < PerMapPackages->Num(); PerMapPackageIndex++)
							{
								FName PackageName = (*PerMapPackages)(PerMapPackageIndex);

								UPackage* Pkg = LoadPackageForCooking(*PackageName.ToString());
								if (Pkg != NULL)
								{
									warnf( NAME_Log, TEXT("   Forcing %s into %s"), *PackageName.ToString(), *SrcFilename.GetBaseFilename());

									if (bShouldOnlyLoad == FALSE)
									{
										MarkCookedStartupObjects(FALSE);
									}

									// reference all objects in this package with the referencer
									for (FObjectIterator It; It; ++It)
									{
										if (It->IsIn(Pkg))
										{
											// only add objects that are going to be cooked into this package (public, not already marked)
											if (!It->HasAnyFlags(RF_MarkedByCooker|RF_Transient) && It->HasAllFlags(RF_Public))
											{
												World->ExtraReferencedObjects.AddItem(*It);
											}
										}
									}
								}
								else
								{
									warnf( NAME_Log, TEXT("   Failed to force %s into %s"), *PackageName.ToString(), *SrcFilename.GetBaseFilename());
								}
							}
						}
					}

					// Disallow lazy loading for all packages
					Package->PackageFlags |= PKG_DisallowLazyLoading;

					// packages that should be fully compressed need to be marked as such
					if( bShouldBeFullyCompressed )
					{
						Package->PackageFlags |= PKG_StoreFullyCompressed;
					}

					ShaderCache = NULL;
					PrepPackageForObjectCooking(Package, bShouldBeSeekFree, bShouldBeFullyCompressed, 
						bIsNativeScriptFile, bIsCombinedStartupPackage, bStripEverythingButTextures, TRUE);

					// We need to process Landscape objects first, as we need their texture data before it's cooked.
					for( TObjectIterator<ULandscapeComponent> It; It; ++It )
					{
						ULandscapeComponent* LandscapeComponent = *It;
						if( LandscapeComponent->IsIn( Package ) || LandscapeComponent->HasAnyFlags( RF_ForceTagExp ) )
						{
							CookLandscapeComponent(LandscapeComponent);
						}
					}

					// If we're cooking for flash, make sure we fix up any auto flattened textures that are not supposed to be DXT5s.
					if ( Platform & PLATFORM_Flash )
					{
						for ( TObjectIterator<UMaterialInterface> It; It; ++It )
						{
							UMaterialInterface* MaterialInterface = *It;
							if ( !MaterialInterface->IsTemplate( RF_ClassDefaultObject ) && !MaterialInterface->HasAnyFlags( RF_Transient ) )
							{
								//@todo. Why are we only checking the base texture here?
								UTexture2D* MobileBaseTexture = Cast<UTexture2D>( MaterialInterface->MobileBaseTexture );
								if ((MobileBaseTexture != NULL) && MobileBaseTexture->Format == PF_DXT5 )
								{
									UMaterial* Material = MaterialInterface->GetMaterial();
									if ( Material && Material->BlendMode == BLEND_Opaque )
									{
										warnf( TEXT("%s is opaque with a DXT5 mobile base texture (%s). Resave the package to fix this."), *MaterialInterface->GetFullName(), *MobileBaseTexture->GetFullName() );
										ConvertDXT5ToDXT1( MobileBaseTexture );
									}
								}
							}
						}
					}

					GMobileShaderCooker.SetCurrentPackage( Package );

					// Iterate over all objects and cook them if they are going to be saved with the package.
					for( FObjectIterator It; It; ++It )
					{
						UObject* Object = *It;

#if WITH_GFx
						if( Object->IsIn( Package ) || (!bIsGFxPackage && Object->HasAnyFlags( RF_ForceTagExp ) ))
#else
						if( Object->IsIn( Package ) || Object->HasAnyFlags( RF_ForceTagExp ) )
#endif
						{
							// Don't cook non texture objects for packages that will only contain texture mips.
							if (!bStripEverythingButTextures || Object->IsA(UTexture2D::StaticClass()))
							{
								// Cook object.
								CookObject( Package, Object, bShouldBeSeekFree );
							}
						}
					}

					// Iterate over all objects and prepare for saving them if they are going to be saved with the package.
					// This is done in two passes now in case the CookObject above removes objects from being saved (see 
					// particle thumbnails in CookParticleSystem)
					for( FObjectIterator It; It; ++It )
					{
						UObject* Object = *It;
#if WITH_GFx
						if (Object->IsIn(Package) || (!bIsGFxPackage && Object->HasAnyFlags(RF_ForceTagExp)))
#else
						if (Object->IsIn(Package) || Object->HasAnyFlags(RF_ForceTagExp))
#endif
						{
							// Don't cook non texture objects for packages that will only contain texture mips.
							if (!bStripEverythingButTextures || Object->IsA(UTexture2D::StaticClass()))
							{
								// Prepare it for serialization.
								PrepareForSaving(Package, Object, bShouldBeSeekFree, bStripEverythingButTextures);
							}
						}
					}

					// strip out texture references, after compiling, if there's a flattened texture (when there's a shader cache that 
					// will be caching it). We do this after preparing all objects for saving because if we strip a parent material 
					// before compiling a MIC, it would fail (compiling happens in PrepareForSaving)
					if ((Platform & PLATFORM_Mobile) && ShaderCache)
					{
						for( TObjectIterator<UMaterialInstance> It; It; ++It )
						{
							UMaterialInstance* MaterialInstance = *It;
							if (!MaterialInstance->HasAnyFlags(RF_ClassDefaultObject|RF_Transient))
							{
								ConditionalUpdateFlattenedTexture(MaterialInstance);
								// make sure the flattening was successful
								if (MaterialInstance->MobileBaseTexture == NULL)
								{
									LogWarningOnce(FString::Printf(TEXT("Failed to find a MobileBaseTexture for %s"), *MaterialInstance->GetFullName()));
								}

								// for flattened MICs, make sure that all the textures referenced by expressions are cleared out
								// because we do not need them to be loaded on the device
								MaterialInstance->ClearParameterValues();

								for (INT Quality = 0; Quality < MSQ_MAX; Quality++)
								{
									if (MaterialInstance->StaticPermutationResources[Quality])
									{
										MaterialInstance->StaticPermutationResources[Quality]->RemoveUniformExpressionTextures();
									}
								}

								MaterialInstance->ReferencedTextureGuids.Empty();
							}
						}

						for( TObjectIterator<UMaterial> It; It; ++It )
						{
							UMaterial* Material = *It;
							if (Material && !Material->HasAnyFlags(RF_Transient)) // && !Material->HasAnyFlags(RF_ClassDefaultObject) ) 
							{
								ConditionalUpdateFlattenedTexture(Material);
								// make sure the flattening was successful
								if (Material->MobileBaseTexture == NULL)
								{
									LogWarningOnce(FString::Printf(TEXT("Failed to find a MobileBaseTexture for %s"), *Material->GetFullName()));
								}

								// for flattened materials, make sure that all the textures referenced by expressions are cleared out
								// because we do not need them to be loaded on the device
								Material->RemoveExpressions(TRUE);
								Material->ReferencedTextureGuids.Empty();
							}
						}

						// Only collect materials and shaders in the final pass.
						if ( ProcessingPass == 1 )
						{
							// Iterate over materials and material instances
							for( TObjectIterator<UMaterialInterface> It; It; ++It )
							{
								UMaterialInterface* MaterialInterface = *It;
								if (MaterialInterface && !MaterialInterface->HasAnyFlags(RF_Transient)) // && !Material->HasAnyFlags(RF_ClassDefaultObject) ) 
								{
									UMaterial* Material = MaterialInterface->GetMaterial();
									GMobileShaderCooker.AddMobileShaderKey(Material, MaterialInterface, World, PreferredLightmapType);
								}
								// Textures marked with bIsCompositingSource cannot be used for rendering on mobile, so we test for this here and warn if we find a material that references one.
								CheckMobileMaterialForCompositingTextures(MaterialInterface);
							}
						}
					}

					// Avoid saving an empty cache into the package.
					if( ShaderCache && ShaderCache->IsEmpty() )
					{
						debugf(TEXT("Skipping empty shader cache for package '%s'"), *Package->GetName());
						ShaderCache->ClearFlags( RF_Standalone );
						ShaderCache = NULL;
					}

					// Clear components before saving and recook static mesh physics data cache.
					if( World && World->PersistentLevel )
					{
						// Re-build the cooked staticmesh physics data cache for the map, in target format.
						World->PersistentLevel->BuildPhysStaticMeshCache();

						// Clear components before we save.
						World->PersistentLevel->ClearComponents();

						World->PersistentLevel->SortActorList();

						// Print out physics cooked data sizes.
						debugfSuppressed( NAME_DevCooking, TEXT("COOKEDPHYSICS: Brush: %f KB"), ((FLOAT)BrushPhysDataBytes)/1024.f );
					}

					// Figure out whether there are any localized resource that would be saved into seek free package and create a special case package
					// that is loaded upfront instead.
					//
					// @warning: Engine does not support localized materials. Mostly because the localized packages don't have their own shader cache.
					UBOOL bIsLocalizedScriptPackage = ((bIsNativeScriptFile && (PackageIndex >= FirstGameScriptIndex)) || bIsCombinedStartupPackage) && bShouldBeSeekFree;
					UBOOL bSupportsLocalization = bIsLocalizedScriptPackage || (!bIsNativeScriptFile && bShouldBeSeekFree);
					if( bSupportsLocalization && AreThereLocalizedResourcesPendingSave( Package, 0 ) )
					{
						const UBOOL bCookingEnglish = appStricmp( TEXT("INT"), UObject::GetLanguage() ) == 0;
						check( bCookingEnglish || !MultilanguageCookingMask); // we better be running in INT for multilanguage cooks to work
						const UBOOL bNonEnglishLocalizedResources = bCookingEnglish ? FALSE : AreThereNonEnglishLocalizedResourcesPendingSave(Package);

						UBOOL bLocalizeIt = bCookingEnglish || bNonEnglishLocalizedResources || MultilanguageCookingMask;
						if (!bCookingEnglish && !bNonEnglishLocalizedResources)
						{
							if (bSubstituteINTPackagesForMissingLoc == TRUE)
							{
								// We know there are localized resource, but there is not a localized version of the package...
								// Is there an INT version of the package? We assume so give that we are in here.
								warnf(NAME_Warning, TEXT("LOC: %3s version of package not found - substituting INT for %s"), UObject::GetLanguage(), *(Package->GetName()));
								bLocalizeIt = TRUE;
							}
						}


						// The cooker should not save out localization packages when cooking for non-English if no non-English resources were found.
						if (bLocalizeIt)
						{
							UObjectReferencer* IntObjectReferencer = 0;
							TArray<UObject *> CleanupItems;
							{
								// Figure out localized package name and file name.
								FFilename LocalizedPackageName		= FFilename(DstFilename.GetBaseFilename() + LOCALIZED_SEEKFREE_SUFFIX).GetLocalizedFilename();
								FFilename Subdir;

								if (bSaveLocalizedCookedPackagesInSubdirectories && !bCookingEnglish && !bIsNativeScriptFile && !bIsCombinedStartupPackage && !bIsNonNativeScriptFile)
								{
									Subdir = PATH_SEPARATOR;
									Subdir += GetLanguage();
								}
								FFilename LocalizedPackageFilename	= DstFilename.GetPath() + Subdir + PATH_SEPARATOR + LocalizedPackageName + GetCookedPackageExtension(Platform);

								// Create a localized package and disallow lazy loading as we're seek free.
								UPackage* LocalizedPackage = CreatePackage( NULL, *LocalizedPackageName );
								LocalizedPackage->PackageFlags |= PKG_Cooked;
								LocalizedPackage->PackageFlags |= PKG_DisallowLazyLoading;
								LocalizedPackage->PackageFlags |= PKG_RequireImportsAlreadyLoaded;
								LocalizedPackage->PackageFlags |= PKG_ServerSideOnly;
								// fully compressed packages should not be marked with the PKG_StoreCompressed flag, as that will compress
								// on the export level, not the package level
								if (bShouldBeFullyCompressed)
								{
									LocalizedPackage->PackageFlags |= PKG_StoreFullyCompressed;
								}
								else
								{
									LocalizedPackage->PackageFlags |= PKG_StoreCompressed;
								}
								// Create object references within package.
								UObjectReferencer* ObjectReferencer = ConstructObject<UObjectReferencer>( UObjectReferencer::StaticClass(), LocalizedPackage, NAME_None, RF_Cooked );

									IntObjectReferencer = ObjectReferencer;

								// Reference all localized audio with the appropriate flags by the helper object.
								for( FObjectIterator It; It; ++It )
								{
									UObject* Object = *It;
									if( Object->IsLocalizedResource() )
									{
										if( Object->HasAnyFlags(RF_ForceTagExp) && !Object->HasAnyFlags(RF_MarkedByCooker))
										{
											ObjectReferencer->ReferencedObjects.AddItem( Object );
											CleanupItems.AddItem(Object);
										}
										// We don't localize objects that are part of the map package as we can't easily avoid duplication.
										else if( Object->IsIn( Package ) )
										{
											warnf( NAME_Warning, TEXT("Localized resources cannot reside in map packages! Please move %s."), *Object->GetFullName() );
										}
									}
								}


								if (!bShouldOnlyLoad)
								{
									// Save localized assets referenced by object referencer.
									check( bStripEverythingButTextures == FALSE );
									TMap<FString,FString>* CookedObjects = NULL;
									if (AlwaysLoadedIdx != INDEX_NONE)
									{
										FString LanguageString = GetLanguage();
										CookedObjects = PersistentCookerData->GetCookedAlwaysLoadedLocMapping(AlwaysLoaded(AlwaysLoadedIdx), LanguageString, TRUE);
										// Clear them out so we create a fresh list (ie don't leave objects from previous cooks)
										CookedObjects->Empty();
									}
									SaveCookedPackage(LocalizedPackage, ObjectReferencer, RF_Standalone, *LocalizedPackageFilename, FALSE, 
										FALSE, (AlwaysLoadedIdx != INDEX_NONE), CookedObjects);
								}
							}
							if (MultilanguageCookingMask)
							{
								// so we saved the INT package, now use the existing referencer as a guide to creating all of the other languages
								check(IntObjectReferencer);
								// Get a list of known language extensions
								const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();

								for (INT LangIndex = 1; LangIndex < KnownLanguageExtensions.Num(); LangIndex++) // start at 1 here, we don't want to do INT again
								{
									if (MultilanguageCookingMask & (1 << LangIndex))
									{
										if ((MultilanguageCookingMaskTextOnly & (1 << LangIndex)))
										{
											if (!bIsNativeScriptFile && !bIsCombinedStartupPackage && !bIsNonNativeScriptFile)
											{
												continue;
											}
										}
										warnf(NAME_Log, TEXT("     Cooking package for Language: %s"),*KnownLanguageExtensions(LangIndex));

										// Figure out localized package name and file name.
										FFilename LocalizedPackageName		= FFilename(DstFilename.GetBaseFilename() + LOCALIZED_SEEKFREE_SUFFIX).GetLocalizedFilename(*KnownLanguageExtensions(LangIndex));
										FFilename Subdir;
										if (bSaveLocalizedCookedPackagesInSubdirectories && !bIsNativeScriptFile && !bIsCombinedStartupPackage && !bIsNonNativeScriptFile)
										{
											Subdir = PATH_SEPARATOR;
											Subdir += *KnownLanguageExtensions(LangIndex);
										}
										FFilename LocalizedPackageFilename	= DstFilename.GetPath() + Subdir + PATH_SEPARATOR + LocalizedPackageName + GetCookedPackageExtension(Platform);

										// Create a localized package and disallow lazy loading as we're seek free.
										UPackage* LocalizedPackage = CreatePackage( NULL, *LocalizedPackageName );
										LocalizedPackage->PackageFlags |= PKG_Cooked;
										LocalizedPackage->PackageFlags |= PKG_DisallowLazyLoading;
										LocalizedPackage->PackageFlags |= PKG_RequireImportsAlreadyLoaded;
										LocalizedPackage->PackageFlags |= PKG_ServerSideOnly;
										// fully compressed packages should not be marked with the PKG_StoreCompressed flag, as that will compress
										// on the export level, not the package level
										if (bShouldBeFullyCompressed)
										{
											LocalizedPackage->PackageFlags |= PKG_StoreFullyCompressed;
										}
										else
										{
											LocalizedPackage->PackageFlags |= PKG_StoreCompressed;
										}
										// Create object references within package.
										UObjectReferencer* ObjectReferencer = ConstructObject<UObjectReferencer>( UObjectReferencer::StaticClass(), LocalizedPackage, NAME_None, RF_Cooked );

										UBOOL bFoundAny = FALSE;
										// Load and reference all localized audio with the appropriate flags by the helper object.
										for( INT ResourceIndex=0; ResourceIndex<IntObjectReferencer->ReferencedObjects.Num(); ResourceIndex++ )
										{
											UObject* Object = IntObjectReferencer->ReferencedObjects(ResourceIndex);

											FString ObjectName = Object->GetPathName();

											INT DotAt = ObjectName.InStr(TEXT("."));
											check(DotAt != INDEX_NONE);  // pathname without a dot???
											FString LocObjectName = ObjectName.Left(DotAt);
											FString LangSuffix = FString("_") + KnownLanguageExtensions(LangIndex);
// 											if (!LocObjectName.EndsWith(LangSuffix))
 											{
 												LocObjectName += LangSuffix;
 											}
											LocObjectName += ObjectName.Mid(DotAt);

											UObject* LocObject = StaticLoadObject(UObject::StaticClass(), NULL, *LocObjectName, NULL, LOAD_NoWarn | LOAD_Quiet | LOAD_NoWarn, NULL);
											if (LocObject)
											{
												UPackage* OriginalPackage = Cast<UPackage>(Object->GetOutermost());
												UPackage* LocPackage = Cast<UPackage>(LocObject->GetOutermost());
												if (!OriginalPackage || !LocPackage)
												{
													warnf(NAME_Warning,TEXT("Could not determine that loc package is conformed because there is no outermost package %s %s?"),*Object->GetFullName(),*LocObject->GetFullName());
												}
												else if (OriginalPackage->GetGuid() != LocPackage->GetGuid())
												{
													warnf(NAME_Warning,TEXT("Package %s is not conformed, skipping localization for %s"),*LocPackage->GetName(),*LocObject->GetFullName());
													LocObject = NULL;
												}
											}
											if (LocObject)
											{
												USoundNodeWave* SoundNodeWaveLoc = Cast<USoundNodeWave>(LocObject);
												USoundNodeWave* SoundNodeWave = Cast<USoundNodeWave>(Object);
												// it isn't clear that localized audio loading this way would pick up the subtitles
												// and even if it did, it is much faster to just copy what we already figured out
												if (SoundNodeWave && SoundNodeWaveLoc && SoundNodeWave != SoundNodeWaveLoc)
												{
													SoundNodeWaveLoc->LocalizedSubtitles = SoundNodeWave->LocalizedSubtitles;
												}
												// Cook object.
												CookObject( Package, LocObject, bShouldBeSeekFree );

												// this technically would apply to sounds as well, but we save time by not considering them as possible outers
												if (!SoundNodeWave)
												{
													// make sure any inners of this localized object are cooked and saved as well
	//												debugf(TEXT("Checking for inners on %s"),*LocObject->GetFullName());
													for( FObjectIterator It; It; ++It )
													{
														UObject *ItObject = *It;
														if (ItObject != LocObject && ItObject->IsIn(LocObject))
														{
	//														debugf(TEXT("Found inners %s"),*ItObject->GetFullName());
															CookObject( Package, ItObject, bShouldBeSeekFree );
															ObjectReferencer->ReferencedObjects.AddItem( ItObject );
															ItObject->SetFlags( RF_ForceTagExp );
															CleanupItems.AddItem(ItObject);
														}
													}
												}
											}
											if (!LocObject && bSubstituteINTPackagesForMissingLoc == TRUE)
											{
												LocObject = Object;
											}
											if ( LocObject )
											{
												LocObject->SetFlags(RF_LocalizedResource);
												ObjectReferencer->ReferencedObjects.AddItem( LocObject );
												CleanupItems.AddItem(LocObject);
												bFoundAny = TRUE;
												while( LocObject )
												{
													LocObject->SetFlags( RF_ForceTagExp );
													LocObject = LocObject->GetOuter();
												}
											}
											else
											{
												warnf(NAME_Log, TEXT("           Couldn't find localized resource %s"),*LocObjectName);
											}
										}

										if (bFoundAny)
										{
											if (!bShouldOnlyLoad)
											{
												// Save localized assets referenced by object referencer.
												check( bStripEverythingButTextures == FALSE );
												TMap<FString,FString>* CookedObjects = NULL;
												if (AlwaysLoadedIdx != INDEX_NONE)
												{
													FString LanguageString = KnownLanguageExtensions(LangIndex);
													CookedObjects = PersistentCookerData->GetCookedAlwaysLoadedLocMapping(AlwaysLoaded(AlwaysLoadedIdx), LanguageString, TRUE);
													// Clear them out so we create a fresh list (ie don't leave objects from previous cooks)
													CookedObjects->Empty();
												}
												SaveCookedPackage(LocalizedPackage, ObjectReferencer, RF_Standalone, *LocalizedPackageFilename, FALSE, 
													FALSE, (AlwaysLoadedIdx != INDEX_NONE), CookedObjects);
											}
										}
										else
										{
											warnf(NAME_Log, TEXT("           No resources found, not saving %s"),*LocalizedPackageFilename);
										}
									}
								}
							}
							// Prevent objects from being saved into seekfree package.
							for( INT ResourceIndex=0; ResourceIndex<CleanupItems.Num(); ResourceIndex++ )
							{
								UObject* Object = CleanupItems(ResourceIndex);
								Object->ClearFlags( RF_ForceTagExp );
								// Avoid object from being put into subsequent script or map packages.
								if(bIsNativeScriptFile || bIsCombinedStartupPackage)
								{
									Object->SetFlags( RF_MarkedByCooker );
									Object->AddToRoot(); // because of ML cooks, we want to make sure this isn't GC'd, reloaded and resaved
								}
							}

							// If an asset B has a loc asset A as its outer then A gets the RF_ForceTagExp flag
							// cleared while B does not.  This goes through and performs the same process on all B's
							// that has an outer in the ReferencedObjects list.
							for( FObjectIterator It; It; ++It )
							{
								UObject *Object = *It;
								for( INT ResourceIndex=0; ResourceIndex<CleanupItems.Num(); ResourceIndex++ )
								{
									UObject* PossibleOuter = CleanupItems(ResourceIndex);
									if( Object->IsIn(PossibleOuter) )
									{
										Object->ClearFlags( RF_ForceTagExp );
										// Avoid object from being put into subsequent script or map packages.
										if(bIsNativeScriptFile || bIsCombinedStartupPackage)
										{
											Object->SetFlags( RF_MarkedByCooker );
											Object->AddToRoot(); // because of ML cooks, we want to make sure this isn't GC'd, reloaded and resaved
										}
									}
								}
							}
						}
					}

					// Save the cooked package if wanted.
					if( !World || !bSkipSavingMaps )
					{
						UObject* Base = World;
						if (bIsCombinedStartupPackage)
						{
							check(CombinedStartupReferencer);
							Base = CombinedStartupReferencer;
						}
						else if (bIsStandaloneSeekFreePackage)
						{
							if ( bCookingPS3Mod )
							{
								Base = NULL;
							}
							else
							{
								check(StandaloneSeekFreeReferencer);
								Base = StandaloneSeekFreeReferencer;
							}
						}

						// Only script mod packages support auto downloading.
						if ( bIsTokenContentPackage || bIsTokenMap )
						{
							Package->PackageFlags &= ~PKG_AllowDownload;
						}

						if (!bShouldOnlyLoad)
						{
							// cache the file age of the source file now that we've cooked it
							PersistentCookerData->SetFileTime(*DstFilename, GFileManager->GetFileTimestamp(*SrcFilename));

							TMap<FString,FString>* CookedObjects = NULL;
							if (AlwaysLoadedIdx != INDEX_NONE)
							{
								check(AlwaysLoadedIdx < AlwaysLoaded.Num());
								FString AlwaysLoadName = AlwaysLoaded(AlwaysLoadedIdx);
								CookedObjects = PersistentCookerData->GetCookedAlwaysLoadedMapping(AlwaysLoadName, TRUE);
								check(CookedObjects);
								// Clear them out so we create a fresh list (ie don't leave objects from previous cooks)
								CookedObjects->Empty();
							}

							SaveCookedPackage( Package, 
								Base, 
								RF_Standalone, 
								*DstFilename, 
								bStripEverythingButTextures,
								bShouldBeSeekFree, // should we clean up objects for next packages
								bIsNativeScriptFile || bIsCombinedStartupPackage,  // should we mark saved objects to not be saved again
								CookedObjects);

							if (AlwaysLoadedIdx != INDEX_NONE)
							{
								check(AlwaysLoadedIdx < AlwaysLoaded.Num());
								AlwaysLoadedWasSaved(AlwaysLoadedIdx) = TRUE;
#if 0 // Enable this to list out the 'always cooked' objects
								check(CookedObjects);
								warnf(NAME_Log, TEXT("AlwaysLoadPackage was saved: %s"), *AlwaysLoaded(AlwaysLoadedIdx));
								for (TMap<FString,FString>::TIterator DumpIt(*CookedObjects); DumpIt; ++DumpIt)
								{
									warnf(NAME_Log, TEXT("\t%s"), *(DumpIt.Key()));
								}
#endif
							}
						}
					}

					// restore original package flags so that if future cooking loads this package again the code doesn't get confused and think it's loading a cooked package
					Package->PackageFlags = OriginalPackageFlags;

					// NULL out shader cache reference before garbage collection to avoid dangling pointer.
					ShaderCache = NULL;
				}
				else if (!Package)
				{
					warnf( NAME_Error, TEXT("Failed loading %s"), *SrcFilename );
				}

				if (bSkipStartupObjectDetermination && bMarkObjectsAfterProcessing)
				{
					// Collect garbage and verify it worked as expected.	
					CollectGarbageAndVerify();

					// Make sure that none of the currently existing objects get put into a map file. This is required for
					// the code to work correctly in combination with not always recooking script packages.
					// Iterate over all objects, setting RF_MarkedByCooker and marking materials as already handled.
					for( FObjectIterator It; It; ++It )
					{
						UObject*	Object		= *It;
						UMaterial*	Material	= Cast<UMaterial>(Object);
						UMaterialInstance*	MaterialInstance	= Cast<UMaterialInstance>(Object);
						// Don't put into any seekfree packages.
						Object->SetFlags( RF_MarkedByCooker );
						if (bShouldOnlyLoad)
						{ // probably not needed
							Object->ClearFlags( RF_ForceTagExp | RF_Saved );
						}
						// Don't put material into seekfree shader caches. Also remove all expressions.
						if( Material )
						{
							AlreadyHandledMaterials.Add(Material->GetFullName());
						}
						if( MaterialInstance )
						{
							AlreadyHandledMaterialInstances.Add(MaterialInstance->GetFullName());
						}
					}
				}

				STAT(CookStats.FinalizePackageTime += appSeconds());
				if (bIsMTChild && NextChildIndex == PackageIndex )
				{
					check(NextChildIndex != -1);
					JobCompleted();
					NextChildIndex = -1;
				}
			}

			if (ProcessingPass == 1)
			{
				GMobileShaderCooker.StopMaterialList();
			}

			// Restore the master setting...
			bIsMTMaster = bWasMTMaster;

			DOUBLE PassTime = appSeconds() - PassStartTime;
			if (ProcessingPass == 0)
			{
				check(AlwaysLoadedWasSaved.Num() == AlwaysLoaded.Num());
				CookStats.AlwaysLoadedProcessingTime = PassTime;
			}
			else
			{
				CookStats.ContentProcessingTime = PassTime;
			}
		}

		if (bIsMTMaster)
		{
			CookStats.PackageIterationTime = 0.0f; // we won't count my package iteration time
			StopChildren();
		}

		STAT(CookStats.PostPackageIterationTime -= appSeconds());

		// always save final version of the shader cache
		if (bIsMTChild)
		{
			GuidCache->SaveToDisk(ShouldByteSwapData());
			FFilename PersistentCookerDataFilename = CookedDirForPerProcessData + GetBulkDataContainerFilename();
			CookStats.LoadExternalStats();
			PersistentCookerData->GetCookStats() = CookStats;
			BulletProofPCDSave(PersistentCookerData,*PersistentCookerDataFilename);
			UBOOL bSaveShadersInPackages = !(Platform & (UE3::PLATFORM_Windows|UE3::PLATFORM_WindowsConsole));
			if (!GIsBuildMachine || !bSaveShadersInPackages) // build machines don't reuse the local shader cache for anything anyway (if the shaders are saved in packages)
			{
				SCOPE_SECONDS_COUNTER_COOK(SaveShaderCacheTime);
				if (Platform & (UE3::PLATFORM_Windows|UE3::PLATFORM_WindowsConsole))
				{
					SaveLocalShaderCache(SP_PCD3D_SM3,*(CookedDirForPerProcessData * TEXT("ProcessShaderCache-") + ShaderPlatformToText(SP_PCD3D_SM3) + TEXT(".upk")));
					if( !ShaderUtils::ShouldForceSM3ShadersOnPC() )
					{
						SaveLocalShaderCache(SP_PCD3D_SM5,*(CookedDirForPerProcessData * TEXT("ProcessShaderCache-") + ShaderPlatformToText(SP_PCD3D_SM5) + TEXT(".upk")));
						SaveLocalShaderCache(SP_PCOGL,*(CookedDirForPerProcessData * TEXT("ProcessShaderCache-") + ShaderPlatformToText(SP_PCOGL) + TEXT(".upk")));
					}
				}
				else if (Platform & UE3::PLATFORM_WindowsServer)
				{
					// Cook no shaders
				}
				else
				{
					SaveLocalShaderCache(ShaderPlatform,*(CookedDirForPerProcessData * TEXT("ProcessShaderCache-") + ShaderPlatformToText(ShaderPlatform) + TEXT(".upk")));
				}
			}
			// not considered fatal if this fails for some reason
			GFileManager->DeleteDirectory(*(CookedDirForPerProcessData * TEXT("Shaders")),FALSE,TRUE);
			JobCompleted();
		}
		else if (bIsMTMaster)
		{
			// Save the shader hashes used for this cook
			const FString ShaderDataFileName = CookedDir + GetShaderDataContainerFilename();
			PersistentShaderData.SaveToDisk(ShaderDataFileName);

			BulletProofPCDSave(PersistentCookerData,*(CookedDir * GetBulkDataContainerFilename()));
			GuidCache->SaveToDisk(ShouldByteSwapData());
			SCOPE_SECONDS_COUNTER_COOK(SaveShaderCacheTime);
			SaveLocalShaderCaches();
			//save the additions to the mobile shader cache
			GMobileShaderCooker.Save( PlatformEngineConfigFilename, PlatformSystemSettingsConfigFilename );
		}
		else
		{
			// Save the shader hashes used for this cook
			const FString ShaderDataFileName = CookedDir + GetShaderDataContainerFilename();
			PersistentShaderData.SaveToDisk(ShaderDataFileName);

			SCOPE_SECONDS_COUNTER_COOK(SaveShaderCacheTime);
			SaveLocalShaderCaches();
			GuidCache->SaveToDisk(ShouldByteSwapData());
			//save the additions to the mobile shader cache
			GMobileShaderCooker.Save( PlatformEngineConfigFilename, PlatformSystemSettingsConfigFilename );
		}

		if (!bIsMTChild)
		{
			if (Platform & (UE3::PLATFORM_Windows|UE3::PLATFORM_WindowsConsole))
			{
				SCOPE_SECONDS_COUNTER_COOK(CopyShaderCacheTime);

				// Cooked PC builds need the cooked reference shader caches for all PC shader platforms.
				CookReferenceShaderCache(SP_PCD3D_SM3);
				if( !ShaderUtils::ShouldForceSM3ShadersOnPC() )
				{
					CookReferenceShaderCache(SP_PCD3D_SM5);
					CookReferenceShaderCache(SP_PCOGL);
				}

				// Cooked PC builds also need the cooked global shader caches for all PC shader platforms.
				CookGlobalShaderCache(SP_PCD3D_SM3);
				if( !ShaderUtils::ShouldForceSM3ShadersOnPC() )
				{
					CookGlobalShaderCache(SP_PCD3D_SM5);
					CookGlobalShaderCache(SP_PCOGL);
				}
			} 
			else if (Platform & UE3::PLATFORM_WindowsServer)
			{
				// Cook no shaders
			}
			else if (Platform == UE3::PLATFORM_NGP)
			{
				CookGlobalShaderCache(ShaderPlatform);
				CookGlobalShaderCache(SP_NGP);
			}
			else if (Platform == UE3::PLATFORM_MacOSX)
			{
				CookReferenceShaderCache(SP_PCOGL);
				CookGlobalShaderCache(SP_PCOGL);
			}
			else
			{
				// Non-PC cooked builds only need the cooked global shader cache a single shader platform.
				CookGlobalShaderCache(ShaderPlatform);
			}
		}

		// Print out detailed stats

		CookStats.TotalTime = appSeconds() - CookStats.TotalTime;
		STAT(CookStats.PostPackageIterationTime += appSeconds());

		if( CookStats.TotalTime > 30 )
		{
			warnf( NAME_Log, TEXT("") );
			warnf( NAME_Log, TEXT("TotalTime                                %7.2f seconds"), CookStats.TotalTime );
			warnf( NAME_Log, TEXT("  AlwaysLoadedProcessingTime             %7.2f seconds"), CookStats.AlwaysLoadedProcessingTime );
			warnf( NAME_Log, TEXT("  ContentProcessingTime                  %7.2f seconds"), CookStats.ContentProcessingTime );
			warnf( NAME_Log, TEXT("  CreateIniFiles                         %7.2f seconds"), CookStats.CreateIniFilesTime );
			warnf( NAME_Log, TEXT("  LoadSectionPackages                    %7.2f seconds"), CookStats.LoadSectionPackagesTime );
			warnf( NAME_Log, TEXT("  LoadNativePackages                     %7.2f seconds"), CookStats.LoadNativePackagesTime );
			warnf( NAME_Log, TEXT("  LoadDependencies                       %7.2f seconds"), CookStats.LoadDependenciesTime );
			warnf( NAME_Log, TEXT("  LoadPackages                           %7.2f seconds"), CookStats.LoadPackagesTime );
			warnf( NAME_Log, TEXT("  LoadPerMapPackages                     %7.2f seconds"), CookStats.LoadPerMapPackagesTime );
			warnf( NAME_Log, TEXT("  LoadCombinedStartupPackages            %7.2f seconds"), CookStats.LoadCombinedStartupPackagesTime );
			warnf( NAME_Log, TEXT("  CheckDependentPackages                 %7.2f seconds"), CookStats.CheckDependentPackagesTime );
			warnf( NAME_Log, TEXT("  CleanupMaterialsTime                   %7.2f seconds"), CookStats.CleanupMaterialsTime );
			warnf( NAME_Log, TEXT("  CleanupShaderCacheTime                 %7.2f seconds"), CookStats.CleanupShaderCacheTime );
			warnf( NAME_Log, TEXT("  Load ShaderCache                       %7.2f seconds"), CookStats.ExternGShaderCacheLoadTime );
			warnf( NAME_Log, TEXT("  Save ShaderCache                       %7.2f seconds"), CookStats.SaveShaderCacheTime );
			warnf( NAME_Log, TEXT("  Copy ShaderCache                       %7.2f seconds"), CookStats.CopyShaderCacheTime );
			warnf( NAME_Log, TEXT("  RHI shader compile time                %7.2f seconds"), CookStats.ExternGRHIShaderCompileTime_Total );
			warnf( NAME_Log, TEXT("    PS3                                  %7.2f seconds"), CookStats.ExternGRHIShaderCompileTime_PS3 );
			warnf( NAME_Log, TEXT("    XBOXD3D                              %7.2f seconds"), CookStats.ExternGRHIShaderCompileTime_XBOXD3D );
			warnf( NAME_Log, TEXT("    NGP                                  %7.2f seconds"), CookStats.ExternGRHIShaderCompileTime_NGP );
			warnf( NAME_Log, TEXT("    WIIU                                 %7.2f seconds"), CookStats.ExternGRHIShaderCompileTime_WIIU );
			warnf( NAME_Log, TEXT("    PCD3D_SM3                            %7.2f seconds"), CookStats.ExternGRHIShaderCompileTime_PCD3D_SM3 );
			warnf( NAME_Log, TEXT("    PCD3D_SM5                            %7.2f seconds"), CookStats.ExternGRHIShaderCompileTime_PCD3D_SM5 );
			warnf( NAME_Log, TEXT("    PCOGL                                %7.2f seconds"), CookStats.ExternGRHIShaderCompileTime_PCOGL );
			warnf( NAME_Log, TEXT("  CookTime                               %7.2f seconds"), CookStats.CookTime );
			warnf( NAME_Log, TEXT("    CookPhysics                          %7.2f seconds"), CookStats.CookPhysicsTime );
			warnf( NAME_Log, TEXT("    CookTexture                          %7.2f seconds"), CookStats.CookTextureTime );
			warnf( NAME_Log, TEXT("    CookSound                            %7.2f seconds"), CookStats.CookSoundTime );
			warnf( NAME_Log, TEXT("    CookSoundCue                         %7.2f seconds"), CookStats.CookSoundCueTime );		
			warnf( NAME_Log, TEXT("    LocSound                             %7.2f seconds"), CookStats.LocSoundTime );
			warnf( NAME_Log, TEXT("    CookMovie                            %7.2f seconds"), CookStats.CookMovieTime );
			warnf( NAME_Log, TEXT("    CookStrip                            %7.2f seconds"), CookStats.CookStripTime );
			warnf( NAME_Log, TEXT("    CookSkeletalMesh                     %7.2f seconds"), CookStats.CookSkeletalMeshTime );
			warnf( NAME_Log, TEXT("    CookStaticMesh                       %7.2f seconds"), CookStats.CookStaticMeshTime );
			warnf( NAME_Log, TEXT("    CookLandscape                        %7.2f seconds"), CookStats.CookLandscapeTime );
			warnf( NAME_Log, TEXT("    CookParticleSystemTime               %7.2f seconds"), CookStats.CookParticleSystemTime );
			warnf( NAME_Log, TEXT("        DuplicateRemovalTime             %7.2f seconds"), CookStats.CookParticleSystemDuplicateRemovalTime );
			warnf( NAME_Log, TEXT("  PackageSave                            %7.2f seconds"), CookStats.PackageSaveTime );
			warnf( NAME_Log, TEXT("  PrepareForSaving                       %7.2f seconds"), CookStats.PrepareForSavingTime );
			warnf( NAME_Log, TEXT("    PrepareForSavingTexture              %7.2f seconds"), CookStats.PrepareForSavingTextureTime );
			warnf( NAME_Log, TEXT("    PrepareForSavingTerrain              %7.2f seconds"), CookStats.PrepareForSavingTerrainTime );
			warnf( NAME_Log, TEXT("    PrepareForSavingMaterial             %7.2f seconds"), CookStats.PrepareForSavingMaterialTime );
			warnf( NAME_Log, TEXT("    PrepareForSavingMaterialInstance     %7.2f seconds"), CookStats.PrepareForSavingMaterialInstanceTime );
			warnf( NAME_Log, TEXT("    PrepareForSavingStaticMeshTime       %7.2f seconds"), CookStats.PrepareForSavingStaticMeshTime );
			warnf( NAME_Log, TEXT("  PackageLocTime                         %7.2f seconds"), CookStats.PackageLocTime );
			warnf( NAME_Log, TEXT("  CollectGarbageAndVerify                %7.2f seconds"), CookStats.CollectGarbageAndVerifyTime );
			warnf( NAME_Log, TEXT("") );
			warnf( NAME_Log, TEXT("Regional Stats:") );
			warnf( NAME_Log, TEXT("  Before Package Iteration               %7.2f seconds"), CookStats.PrePackageIterationTime );
			warnf( NAME_Log, TEXT("  Package Iteration                      %7.2f seconds"), CookStats.PackageIterationTime );
			warnf( NAME_Log, TEXT("    Prep Package                         %7.2f seconds"), CookStats.PrepPackageTime );
			warnf( NAME_Log, TEXT("    Tag cooked startup time              %7.2f seconds"), CookStats.TagCookedStartupTime );
			warnf( NAME_Log, TEXT("    Mark cooked startup time             %7.2f seconds"), CookStats.MarkCookedStartupTime );
			warnf( NAME_Log, TEXT("    Initialize Package                   %7.2f seconds"), CookStats.InitializePackageTime );
			warnf( NAME_Log, TEXT("    Finalize Package                     %7.2f seconds"), CookStats.FinalizePackageTime );
			warnf( NAME_Log, TEXT("  After Package Iteration                %7.2f seconds"), CookStats.PostPackageIterationTime );
			warnf( NAME_Log, TEXT("") );
			warnf( NAME_Log, TEXT("Compression Stats:") );
			warnf( NAME_Log, TEXT("  FArchive::SerializeCompressed time     %7.2f seconds"), CookStats.ExternGArchiveSerializedCompressedSavingTime );
			warnf( NAME_Log, TEXT("  Compressor thread time                 %7.2f seconds"), CookStats.ExternGCompressorTime );
			warnf( NAME_Log, TEXT("  Compressed src bytes                   %7i MByte"), CookStats.ExternGCompressorSrcBytes / 1024 / 1024 );
			warnf( NAME_Log, TEXT("  Compressor dst bytes                   %7i MByte"), CookStats.ExternGCompressorDstBytes / 1024 / 1024 );
			if( CookStats.ExternGCompressorSrcBytes )
			{
				warnf( NAME_Log, TEXT("  Compression ratio                      %7.2f %%"), 100.f * CookStats.ExternGCompressorDstBytes / CookStats.ExternGCompressorSrcBytes );
			}
			warnf( NAME_Log, TEXT("") );

			warnf( NAME_Log, TEXT("TFC Data Saved                           %7d MByte"), INT(CookStats.TFCTextureSaved / 1024 / 1024) );
			warnf( NAME_Log, TEXT("TFC Data Reused                          %7d MByte"), INT(CookStats.TFCTextureAlreadySaved / 1024 / 1024) );
			warnf( NAME_Log, TEXT("") );

			warnf( NAME_Log, TEXT("PersistentFaceFX Stats:") );
			warnf( NAME_Log, TEXT("  Total time	                            %7.2f seconds"), CookStats.PersistentFaceFXTime );
			warnf( NAME_Log, TEXT("  Determination time                     %7.2f seconds"), CookStats.PersistentFaceFXDeterminationTime );
			warnf( NAME_Log, TEXT("  Generation time                        %7.2f seconds"), CookStats.PersistentFaceFXGenerationTime );
			if (GGameCookerHelper && !bIsMTMaster)
			{
				GGameCookerHelper->DumpStats();
			}
		}

		// Frees DLL handles and deletes cooker objects.
		Cleanup();
	}

	if (!bIsMTChild)
	{
		if( bGenerateSHAHashes )
		{
			// generate SHA hashes if desired
			GenerateSHAHashes();
		}
	}

	delete UserModeSHAWriter;

	// Close texture file caches.
	for(TMap<FName,FArchive*>::TIterator TextureCacheIt(TextureCacheNameToArMap);TextureCacheIt;++TextureCacheIt)
	{
		delete TextureCacheIt.Value();
	}
	TextureCacheNameToArMap.Empty();

	// make sure we delete any copied shader files
	CleanupShaderFiles();

	if (!bIsMTChild)
	{
		if (bUseTextureFileCache && bVerifyTextureFileCache)
		{
			// Verify the supposed offsets w/ the contents of the TextureFileCache.
			VerifyTextureFileCacheEntry();
		}

		if ( bAnalyzeReferencedContent )
		{
			// Re-used helper variables for writing to CSV file.
			const FString CurrentTime = appSystemTimeString();
			const FString CSVDirectory = appGameLogDir() + TEXT("AnalyzeReferencedContentCSVs") + PATH_SEPARATOR + FString::Printf( TEXT("%s-%d-%s_CookData"), GGameName, GetChangeListNumberForPerfTesting(), *CurrentTime ) + PATH_SEPARATOR;
			// Create CSV folder in case it doesn't exist yet.
			GFileManager->MakeDirectory( *CSVDirectory );

			ReferencedContentStat.WriteOutAllAvailableStatData( CSVDirectory );
		}
	}

	GMobileShaderCooker.Close();

	return 0;
}

/**
 *	Prep the given package for Object cooking.
 *	@param	InPackage						The package to prep
 *	@param	bInShouldBeSeekFree				Whether the package will be seekfree
 *	@param	bInShouldBeFullyCompressed		Whether the package will be fully compressed
 *	@param	bInIsNativeScriptFile			Whether the package is a native script file
 *	@param	bInIsCombinedStartupPackage		Whether the package is a combined startup package
 *	@param	bInStripEverythingButTextures	Whether the package should have everything but textures stripped
 *	@param	bInProcessShaderCaches			Whether the shader caches should be processed
 */
void UCookPackagesCommandlet::PrepPackageForObjectCooking(
	UPackage* InPackage, 
	UBOOL bInShouldBeSeekFree,
	UBOOL bInShouldBeFullyCompressed,
	UBOOL bInIsNativeScriptFile,
	UBOOL bInIsCombinedStartupPackage,
	UBOOL bInStripEverythingButTextures,
	UBOOL bInProcessShaderCaches)
{
	// Handle creation of seek free packages/ levels.
	if (bInShouldBeSeekFree)
	{
		InPackage->PackageFlags |= PKG_RequireImportsAlreadyLoaded;
		// fully compressed packages should not be marked with the PKG_StoreCompressed flag, as that will compress
		// on the export level, not the package level
		if (bInShouldBeFullyCompressed == FALSE)
		{
			InPackage->PackageFlags |= PKG_StoreCompressed;
		}

		// Iterate over all objects and mark them as RF_ForceTagExp where appropriate.
		for (FObjectIterator It; It; ++It)
		{
			UObject* Object = *It;
			// Exclude objects that are either marked "marked" or transient.
			if (!Object->HasAnyFlags(RF_MarkedByCooker|RF_Transient))
			{
				// Exclude objects that reside in the transient package.
				if (!Object->IsIn(UObject::GetTransientPackage()))
				{
					// Exclude objects that are in the package as they will be serialized anyways.
					if (!Object->IsIn(InPackage))
					{
						// Mark object so it gets forced into the export table.
						Object->SetFlags(RF_ForceTagExp);

						// Make sure that super classes are also treated as exports for this cooked package
						// This only applies to super classes that are in the same class as an existing force export class
						UClass* TagObjectClass = Cast<UClass>(Object);
						if (TagObjectClass != NULL)
						{
							UObject* TagObjectClassPackage = TagObjectClass->GetOutermost();
							UClass* SuperClass = TagObjectClass->GetSuperClass();
							while (SuperClass)
							{
								if (SuperClass->GetOutermost() == TagObjectClassPackage &&
									!SuperClass->HasAnyFlags(RF_ForceTagExp|RF_Cooked))
								{
									SuperClass->SetFlags(RF_ForceTagExp);
									debugf(TEXT("Force export: %s"),*SuperClass->GetPathName());
									SuperClass = SuperClass->GetSuperClass();
								}
								else
								{
									SuperClass = NULL;
								}
							}
						}
					}
				}
			}
		}
	}

	// make a local shader cache in each of the seekfree packages for consoles
	if (bInProcessShaderCaches && bInShouldBeSeekFree && (Platform & UE3::PLATFORM_Console || (Platform & UE3::PLATFORM_WindowsConsole && ParseParam(appCmdLine(), TEXT("CookShadersIntoMapsForPCConsole")))))
	{
		// We can't save the shader cache into Core as the UShaderCache class is loaded afterwards.
		if (InPackage->GetFName() != NAME_Core)
		{
			// Construct a shader cache in the package with RF_Standalone so it is being saved into the package even though it is not
			// referenced by any objects in the case of passing in a root, like done e.g. when saving worlds.
			ShaderCache = new(InPackage,TEXT("SeekFreeShaderCache"),RF_Standalone) UShaderCache(ShaderPlatform);

			if (GGameCookerHelper)
			{
				GGameCookerHelper->InitializeShaderCache(this, InPackage, ShaderCache, bInIsNativeScriptFile || bInIsCombinedStartupPackage);
			}
		}
	}

	// Handle duplicating cubemap faces. This needs to be done before the below object iterator as it creates new objects.
	HandleCubemaps(InPackage);

	// Replace static mesh actors with a StaticMeshCollectionActor
	if (!bInStripEverythingButTextures)
	{
		// Clean up materials???
		// Need to generate a list of all textures that are used by materials now
		// And mark all others as already cooked.
		if ((bCleanupMaterials == TRUE) && ((GCookingTarget & PLATFORM_Stripped) != 0))
		{
			CleanupMaterials(InPackage);
		}

		if (bCookOutStaticMeshActors == TRUE)
		{
			CookStaticMeshActors(InPackage);
		}

		if (bCookOutStaticLightActors == TRUE)
		{
			CookStaticLightActors(InPackage);
		}

		if (bBakeAndPruneDuringCook == TRUE)
		{
			BakeAndPruneMatinee(InPackage);
		}
	}
}

/**
* If -sha is specified then iterate over all FilesForSHA and generate their hashes
* The results are written to Hashes.sha
*/
void UCookPackagesCommandlet::GenerateSHAHashes()
{
	// add the fully loaded packages to the list to verify (since we fully load the memory for
	// them, it is not crazy to verify the data at runtime) if we are going to preload anything
	UBOOL bSerializeStartupPackagesFromMemory = FALSE;
	GConfig->GetBool(TEXT("Engine.StartupPackages"), TEXT("bSerializeStartupPackagesFromMemory"), bSerializeStartupPackagesFromMemory, PlatformEngineConfigFilename);

	// Get a list of known language extensions
	const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();

	if (bSerializeStartupPackagesFromMemory)
	{
		if (Platform & PLATFORM_PC)
		{
			// Get combined list of all script package names, including editor- only ones.
			TArray<FString>	 AllScriptPackageNames;
			EScriptPackageTypes ScriptTypes = SPT_AllScript;
			if (Platform == PLATFORM_MacOSX)
			{
				ScriptTypes = EScriptPackageTypes(ScriptTypes & ~SPT_Editor); // no editor support in cooked mac
			}
			appGetScriptPackageNames(AllScriptPackageNames, ScriptTypes, PlatformEngineConfigFilename);

			// turn them into file names that can be opened
			for (INT PackageIndex = 0; PackageIndex < AllScriptPackageNames.Num(); PackageIndex++)
			{
				// see if the file exists and get the filename + extension
				FString PackageFilename;
				if( GPackageFileCache->FindPackageFile(*AllScriptPackageNames(PackageIndex), NULL, PackageFilename) )
				{
					const FFilename SrcFilename(PackageFilename);
					UBOOL bIsScriptFile = AllScriptPackageNames.FindItemIndex(SrcFilename.GetBaseFilename()) != INDEX_NONE;
					if( bIsScriptFile )
					{
						// convert the filename to the cooked path
						FFilename DestFilename = GetCookedPackageFilename(SrcFilename);
						FilesForSHA.AddItem( DestFilename );
					}
				}
				else
				{
					warnf(TEXT("SHA: package not found [%s]"),*AllScriptPackageNames(PackageIndex));
				}
			}

			// get the startup packages
			TArray<FString> StartupPackages;
			appGetAllPotentialStartupPackageNames(StartupPackages, PlatformEngineConfigFilename);

			// get the combined startup packages
			TArray<FString> CombinedStartupPackages;
			GConfig->GetArray(TEXT("Engine.StartupPackages"), TEXT("Package"), CombinedStartupPackages, PlatformEngineConfigFilename);

			// add all startup packages except for script 
			for (INT PackageIndex = 0; PackageIndex < StartupPackages.Num(); PackageIndex++)
			{
				// see if the file exists and get the filename + extension
				FString PackageFilename;
				if( GPackageFileCache->FindPackageFile(*StartupPackages(PackageIndex), NULL, PackageFilename) )
				{
					const UBOOL bShouldSignStartupContentPackages = FALSE;
					const FFilename SrcFilename(PackageFilename);
					UBOOL bIsScriptFile = AllScriptPackageNames.FindItemIndex(SrcFilename.GetBaseFilename()) != INDEX_NONE;
					UBOOL bIsCombinedStartupPackage = CombinedStartupPackages.FindItemIndex(SrcFilename.GetBaseFilename()) != INDEX_NONE;

					if( !bIsScriptFile && (!bIsCombinedStartupPackage || !bShouldSignStartupContentPackages) )
					{
						// convert the filename to the cooked path
						FFilename DestFilename = GetCookedPackageFilename(SrcFilename);
						FilesForSHA.AddItem( DestFilename );
					}
				}
				else
				{
					warnf(TEXT("SHA: package not found [%s]"),*StartupPackages(PackageIndex));
				}

			}

			// also add the "Startup_int" package and all of its language versions
			FilesForSHA.AddItem(*FString(CookedDir + TEXT("Startup") + TEXT(".upk")));
			for (INT LangIndex = 0; LangIndex < KnownLanguageExtensions.Num(); LangIndex++)
			{
				const FString& Ext = KnownLanguageExtensions(LangIndex);
				FilesForSHA.AddItem(*FString(CookedDir + TEXT("Startup_LOC_") + Ext + TEXT(".upk")));
			}
		}
		else // Console
		{
			// get the startup packages
			TArray<FString> StartupPackages;
			appGetAllPotentialStartupPackageNames(StartupPackages, PlatformEngineConfigFilename, TRUE);

			// turn them into file names that can be opened
			for (INT PackageIndex = 0; PackageIndex < StartupPackages.Num(); PackageIndex++)
			{
				FilesForSHA.AddItem(*(CookedDir + (FFilename(StartupPackages(PackageIndex)).GetBaseFilename()) + GetCookedPackageExtension(Platform)));
			}
		}
	}
	else
	{
		warnf(TEXT("SHA: Skipping startup packages. bSerializeStartupPackagesFromMemory=True is required."));
	}

	if ((Platform & PLATFORM_PC) && (Platform != PLATFORM_MacOSX))
	{
		// add all of the shader source files for hashing
		TArray<FString> ShaderSourceFiles;
		appGetAllShaderSourceFiles(ShaderSourceFiles);
		for( INT ShaderFileIdx=0; ShaderFileIdx < ShaderSourceFiles.Num(); ShaderFileIdx++ )
		{
			FString ShaderFilename = FString(appShaderDir()) * ShaderSourceFiles(ShaderFileIdx);
			if (ShaderFilename.InStr(TEXT(".usf"), FALSE, TRUE) == INDEX_NONE)
			{
				ShaderFilename += TEXT(".usf");
			}
			FilesForSHA.AddItem( ShaderFilename );
		}
	}

	warnf(TEXT("Generating Hashes.sha:"));
	// open the output file to store all the hashes
	TArray<BYTE> SHAFileContents;
	FMemoryWriter SHAMemoryWriter(SHAFileContents);

	for (INT SHAIndex = 0; SHAIndex < FilesForSHA.Num(); SHAIndex++)
	{
		FString& Filename = FilesForSHA(SHAIndex);
		TArray<BYTE> Contents;

		// look to see if the file was fully compressed
		FString UncompressedSizeStr;
		if (appLoadFileToString(UncompressedSizeStr, *(Filename + TEXT(".uncompressed_size"))))
		{
			// if it was, we need to generate the hash on the uncomprseed
			INT UncompressedSize = appAtoi(*UncompressedSizeStr);

			// read and uncompress the data
			FArchive* Reader = GFileManager->CreateFileReader(*Filename);
			if (Reader)
			{
				warnf(TEXT("  Decompressing %s"), *Filename);
				Contents.Add(UncompressedSize);
				Reader->SerializeCompressed(Contents.GetData(), 0, GBaseCompressionMethod);
				delete Reader;
			}
		}
		else
		{
			// otherwise try to load the file, checking first so the Error log inside
			// doesn't look like a failure, as it's okay to not exist
			if (GFileManager->FileSize(*Filename) >= 0)
			{
				appLoadFileToArray(Contents, *Filename);
			}
		}

		// is there any data to hash?
		if (Contents.Num() != 0)
		{
			warnf(TEXT("  Hashing %s"), *Filename);
			// if we loaded it, generate the hash
			BYTE Hash[20];
			FSHA1::HashBuffer(Contents.GetData(), Contents.Num(), Hash);

			// write out the filename (including trailing 0)
			// don't write the file path
			FString CleanFilename( FFilename(Filename).GetCleanFilename().ToLower() );
			SHAMemoryWriter.Serialize(TCHAR_TO_ANSI(*CleanFilename), CleanFilename.Len() + 1);

			// write out hash
			SHAMemoryWriter.Serialize(Hash, sizeof(Hash));
		}
		else
		{
			warnf(TEXT(" File not found %s"), *Filename);
		}
	}

	// if we have any per-class script SHA entries, then mark a divider between full package SHA and script SHA
	// (this will allow platforms to potentially handle missing entries for script SHA differently than
	// missing entries for full package SHA)
	const TMap<FString, TArray<BYTE> >& FilenameToScriptSHAMap = PersistentCookerData->GetFilenameToScriptSHA();
	if (FilenameToScriptSHAMap.Num())
	{
		// divider is some magic, with a null terminator
		SHAMemoryWriter.Serialize(HASHES_SHA_DIVIDER, strlen(HASHES_SHA_DIVIDER) + 1);

		for (TMap<FString, TArray<BYTE> >::TConstIterator It(FilenameToScriptSHAMap); It; ++It)
		{
			// write out filename
			SHAMemoryWriter.Serialize(TCHAR_TO_ANSI(*It.Key()), It.Key().Len() + 1);

			// write out hash
			SHAMemoryWriter.Serialize((void*)It.Value().GetData(), It.Value().Num());
		}
	}

	// Save the contents to disk
	FString SHAFilename(appGameDir() + TEXT("Build") PATH_SEPARATOR TEXT("Hashes.sha"));
	
	// For some platforms, rewrite the hashes file in a way that lets it be
	// easily included in C++ source text and pass into the SHA routines
	if (Platform == PLATFORM_IPhone ||
		Platform == PLATFORM_Android ||
		Platform == PLATFORM_NGP ||
		Platform == PLATFORM_WiiU)
	{
		// Create a file writer so that we can write out a modified byte stream
		FArchive* SHAWriter = GFileManager->CreateFileWriter(*SHAFilename);
		if( SHAWriter == NULL )
		{
			warnf( TEXT( "SHA: Unable to create hashes.sha; is it checked out?" ) );
			return;
		}
		// Write out the file, byte by byte, formatted to include in C++ array initializer
		for( INT Index = 0; Index < SHAFileContents.Num(); Index++ )
		{
			FString NextByteFormatted = FString::Printf(TEXT("%d,"), SHAFileContents(Index));
			SHAWriter->Serialize(TCHAR_TO_ANSI(*NextByteFormatted), NextByteFormatted.Len());
		}

		// Close the file
		delete SHAWriter;
	}
	else
	{
		// Simply save out the raw file contents to the file
		appSaveArrayToFile(SHAFileContents, *SHAFilename);
	}
}

/**
 * Merges shader caches of the matching CookShaderPlatform and saves the reference and global caches into the cooked directory.
 */
void UCookPackagesCommandlet::CookReferenceShaderCache(EShaderPlatform CookShaderPlatform)
{
	UShaderCache* LocalShaderCache = GetLocalShaderCache(CookShaderPlatform);
	UShaderCache* RefShaderCache = GetReferenceShaderCache(CookShaderPlatform);

	// get the destination reference shader cache package name
	FFilename ReferenceShaderCachePath = GetReferenceShaderCacheFilename(CookShaderPlatform);
	FFilename AfterGameDir = ReferenceShaderCachePath.Right(ReferenceShaderCachePath.Len() - appGameDir().Len());
	FFilename AfterTopLevelDir = AfterGameDir.Right(AfterGameDir.Len() - (AfterGameDir.InStr(PATH_SEPARATOR) + 1));

	// check file times looking for updated files
	DOUBLE LocalSrcTime = GFileManager->GetFileTimestamp(*GetLocalShaderCacheFilename(CookShaderPlatform));
	DOUBLE RefSrcTime = GFileManager->GetFileTimestamp(*GetReferenceShaderCacheFilename(CookShaderPlatform));
	DOUBLE RefDstTime = GFileManager->GetFileTimestamp(*(CookedDir + AfterTopLevelDir));

	// check if either of the "uncooked" shaders are newer
	UBOOL bIsLocalNewer = LocalSrcTime != -1.0 && LocalSrcTime > RefDstTime;
	UBOOL bIsRefNewer = RefSrcTime != -1.0 && RefSrcTime > RefDstTime;

	// make sure there's something to do
	if ((LocalShaderCache || RefShaderCache) &&
		(bIsLocalNewer || bIsRefNewer || (LocalShaderCache && LocalShaderCache->IsDirty())))
	{
		// if only the local shader cache exists, save that as the reference
		if (LocalShaderCache && !RefShaderCache)
		{
			RefShaderCache = LocalShaderCache;
		}
		// if they both exist, merge the local into the reference
		else if (LocalShaderCache && RefShaderCache)
		{
			RefShaderCache->Merge(LocalShaderCache);
		}

		warnf(NAME_Log, TEXT("Copying shader cache %s"), *AfterTopLevelDir);

		// save the reference shader cache into the cooked directory
		UPackage* ReferenceShaderCachePackage = RefShaderCache->GetOutermost();
		ReferenceShaderCachePackage->PackageFlags |= PKG_ServerSideOnly;
		UObject::SavePackage(ReferenceShaderCachePackage, RefShaderCache, 0, *(CookedDir + AfterTopLevelDir));
	}
}

void UCookPackagesCommandlet::CookGlobalShaderCache(EShaderPlatform CookShaderPlatform)
{
	// On all platforms, write the GlobalShaders file into the cooked folder with the target platform's native byte order.
	const FFilename GlobalShaderCacheFilename = GetGlobalShaderCacheFilename(CookShaderPlatform);
	const FFilename CookedGlobalShaderCacheFilename = GetCookedDirectory() + GlobalShaderCacheFilename.GetCleanFilename();
	FArchive* GlobalShaderCacheFile = GFileManager->CreateFileWriter(*CookedGlobalShaderCacheFilename);
	if(!GlobalShaderCacheFile)
	{
		appErrorf(TEXT("Failed to save cooked global shader cache for %s."),ShaderPlatformToText(CookShaderPlatform));
	}
	GlobalShaderCacheFile->SetByteSwapping(ShouldByteSwapData());
	SerializeGlobalShaders(CookShaderPlatform,*GlobalShaderCacheFile);
	delete GlobalShaderCacheFile;
}

void UCookPackagesCommandlet::AddDynamicMobileShaderGroup(const FName& ShaderGroupName, const FName& PackageName)
{
	GMobileShaderCooker.AddDynamicShaderGroup(ShaderGroupName, PackageName);
}

IMPLEMENT_CLASS(UCookPackagesCommandlet)

/*-----------------------------------------------------------------------------
	UPersistentCookerData implementation.
-----------------------------------------------------------------------------*/

/**
 * Create an instance of this class given a filename. First try to load from disk and if not found
 * will construct object and store the filename for later use during saving.
 *
 * @param	Filename	Filename to use for serialization
 * @param	bCreateIfNotFoundOnDisk		If FALSE, don't create if couldn't be found; return NULL.
 * @return	instance of the container associated with the filename
 */
UPersistentCookerData* UPersistentCookerData::CreateInstance( const TCHAR* Filename, UBOOL bCreateIfNotFoundOnDisk )
{
	UPersistentCookerData* Instance = NULL;

	// Find it on disk first.
	// Try to load the package.
	static INT BulkDataIndex = 0;
	FString Temp = FString::Printf(TEXT("BulkData_%d"), BulkDataIndex++);
	UPackage* BulkDataPackage = CreatePackage(NULL, *Temp);
	UPackage* Package = LoadPackage( BulkDataPackage, Filename, LOAD_NoWarn | LOAD_Quiet);

	// Find in memory if package loaded successfully.
	if( Package )
	{
		Instance = FindObject<UPersistentCookerData>( Package, TEXT("PersistentCookerData") );
	}

	// If not found, create an instance.
	if ( !Instance && bCreateIfNotFoundOnDisk )
	{
		UPackage* Package = UObject::CreatePackage( NULL, NULL );
		Instance = ConstructObject<UPersistentCookerData>( 
							UPersistentCookerData::StaticClass(),
							Package,
							TEXT("PersistentCookerData")
							);
		// Mark package as cooked as it is going to be loaded at run-time and has xxx extension.
		Package->PackageFlags |= PKG_Cooked | PKG_ServerSideOnly;
		check( Instance );
	}


	// Keep the filename around for serialization and add to root to prevent garbage collection.
	if ( Instance )
	{
		Instance->Filename = Filename;
		Instance->AddToRoot();
	}

	return Instance;
}

/**
 * Saves the data to disk.
 */
void UPersistentCookerData::SaveToDisk()
{
	// Save package to disk using filename that was passed to CreateInstance.
	UObject::SavePackage( GetOutermost(), this, 0, *Filename );
}

/**
 * Serialize function.
 *
 * @param	Ar	Archive to serialize with.
 */
void UPersistentCookerData::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << CookedStartupObjects;
	Ar << CookedStartupObjectsLoc;
	Ar << AlreadyHandledStartupMaterials;
	Ar << AlreadyHandledStartupMaterialInstances;
	Ar << CookedBulkDataInfoMap;
	if (Ar.IsSaving() && bMinimalSave)
	{
		TMap<FString,DOUBLE> Nothing;
		Ar << Nothing;
		Ar << TextureFileCacheWaste;

		TMap<FString,INT> Nothing2;
		Ar << Nothing2;
	}
	else
	{
		Ar << FilenameToTimeMap;
		Ar << TextureFileCacheWaste;

		Ar << FilenameToCookedVersion;	
	}

	if (Ar.Ver() >= VER_ANDROID_ETC_SEPARATED)
	{
		Ar << CookedTextureFormats;
	}	

	Ar << CookedTextureFileCacheInfoMap;
	if (Ar.IsSaving() && bMinimalSave)
	{
		TMap<FString,FCookedTextureUsageInfo> Nothing1;
		Ar << Nothing1;
		TMap<FString,FForceCookedInfo> Nothing;
		Ar << Nothing;
		Ar << Nothing;
		TMap<FString,TArray<BYTE> > Nothing2;
		Ar << Nothing2;
	}
	else
	{
		Ar << TextureUsageInfos;
		Ar << CookedPrefixCommonInfoMap;
		Ar << PMapForcedObjectsMap;
		Ar << FilenameToScriptSHA;
	}

	Ar << ChildCookWarnings;
	Ar << ChildCookErrors;

	GetCookStats().Serialize(Ar);
#if 0
	// this is useful for debugging communication in MT cooks
	if (Ar.IsSaving())
	{
		debugf(TEXT("Saving %d"),CookedBulkDataInfoMap.Num());
		for (TMap<FString, FCookedBulkDataInfo>::TIterator It(CookedBulkDataInfoMap); It; ++It)
		{
			debugf(TEXT("Saved %s %s"),*It.Key(), *It.Value().TextureFileCacheName.ToString());
		}
	}
	if (Ar.IsLoading())
	{
		debugf(TEXT("loaded %d"),CookedBulkDataInfoMap.Num());
		for (TMap<FString, FCookedBulkDataInfo>::TIterator It(CookedBulkDataInfoMap); It; ++It)
		{
			debugf(TEXT("Loaded %s %s"),*It.Key(), *It.Value().TextureFileCacheName.ToString());
		}
	}
#endif
}

/**
 * Stores the bulk data info after creating a unique name out of the object and name of bulk data
 *
 * @param	Object					Object used to create unique name
 * @param	BulkDataName			Unique name of bulk data within object, e.g. MipLevel_3
 * @param	CookedBulkDataInfo		Data to store
 */
void UPersistentCookerData::SetBulkDataInfo( UObject* Object, const TCHAR* BulkDataName, const FCookedBulkDataInfo& CookedBulkDataInfo )
{
	FString UniqueName = Object->GetPathName() + TEXT(".") + BulkDataName;
	CookedBulkDataInfoMap.Set( *UniqueName, CookedBulkDataInfo );
}

/**
 * Retrieves previously stored bulk data info data for object/ bulk data name combination.
 *
 * @param	Object					Object used to create unique name
 * @param	BulkDataName			Unique name of bulk data within object, e.g. MipLevel_3
 * @return	cooked bulk data info if found, NULL otherwise
 */
FCookedBulkDataInfo* UPersistentCookerData::GetBulkDataInfo( UObject* Object, const TCHAR* BulkDataName )
{
	FString UniqueName = Object->GetPathName() + TEXT(".") + BulkDataName;
	FCookedBulkDataInfo* CookedBulkDataInfo = CookedBulkDataInfoMap.Find( *UniqueName );
	return CookedBulkDataInfo;
}

/**
 * Dumps all bulk data info data to the log.
 */

void UPersistentCookerData::DumpBulkDataInfos()
{
	warnf(TEXT("BulkDataInfo:"));
	TMap<FString,FCookedBulkDataInfo> SortedCookedBulkDataInfoMap = CookedBulkDataInfoMap;
	SortedCookedBulkDataInfoMap.KeySort<COMPARE_CONSTREF_CLASS(FString,UnContentCookersStringSort)>();
	for( TMap<FString,FCookedBulkDataInfo>::TIterator It(SortedCookedBulkDataInfoMap); It; ++It )
	{
		const FString&				UniqueName	= It.Key();
		const FCookedBulkDataInfo&	Info		= It.Value();

		warnf( TEXT("Offset in File: % 10i  Size on Disk: % 10i ElementCount: % 4i  Flags: 0x%08x  TFC: %16s - %s "), 
			Info.SavedBulkDataOffsetInFile,
			Info.SavedBulkDataSizeOnDisk,
			Info.SavedBulkDataFlags, 
			Info.SavedElementCount,
			*Info.TextureFileCacheName.GetNameString(),
			*UniqueName
			);
	}

	warnf(TEXT("\n\nFiletime Info:"));
	for( TMap<FString,DOUBLE>::TIterator It(FilenameToTimeMap); It; ++It )
	{
		warnf(TEXT("%s = %f"), *It.Key(), It.Value());
	}
}

/**
 * Gathers bulk data infos of objects in passed in Outer.
 *
 * @param	Outer					Outer to use for object traversal when looking for bulk data
 * @param	AndroidTextureFormat 	On Android we will need to know which texture formats to use, if empty, it assumes platform is not Android
 */
void UPersistentCookerData::GatherCookedBulkDataInfos( UObject* Outer, DWORD AndroidTextureFormat )
{
	// Iterate over all objects in passed in outer and store the bulk data info for types supported.
	for( TObjectIterator<UObject> It; It; ++It )
	{
		UObject* Object = *It;
		// Make sure we're in the same package.
		if( Object->IsIn( Outer ) )
		{
			// Texture handling.
			UTexture2D* Texture2D = Cast<UTexture2D>( Object );		
			if( Texture2D )
			{
				check( Texture2D->HasAllFlags( RF_Cooked ) );

				if (AndroidTextureFormat == 0x0)
				{
					GatherCookedMipDataInfos(Texture2D, Texture2D->Mips);
				}
				else
				{
					if (AndroidTextureFormat & TEXSUPPORT_DXT)
					{
						GatherCookedMipDataInfos(Texture2D, Texture2D->Mips, TEXSUPPORT_DXT);
					}
					if (AndroidTextureFormat & TEXSUPPORT_PVRTC)
					{
						GatherCookedMipDataInfos(Texture2D, Texture2D->CachedPVRTCMips, TEXSUPPORT_PVRTC);
					}
					if (AndroidTextureFormat & TEXSUPPORT_ATITC)
					{
						GatherCookedMipDataInfos(Texture2D, Texture2D->CachedATITCMips, TEXSUPPORT_ATITC);
					}
					if (AndroidTextureFormat & TEXSUPPORT_ETC)
					{
						GatherCookedMipDataInfos(Texture2D, Texture2D->CachedETCMips, TEXSUPPORT_ETC);
					}
				}
			}
		}
	}
}

/**
 * Helper function for GatherCookedBulkDataInfo to operate on mip data
 * 
 * @param	Texture2D		the Texture to operate on
 * @param	Mips			Reference to the actual collection of generated mips
 * @param	TextureFormat	Which format is the mip data in
 */
void UPersistentCookerData::GatherCookedMipDataInfos( UTexture2D* Texture2D, TIndirectArray<FTexture2DMipMap> &Mips, ETextureFormatSupport TextureFormat )
{
	for( INT MipLevel=0; MipLevel<Mips.Num(); MipLevel++ )
	{
		FTexture2DMipMap& Mip = Mips(MipLevel);
		// Only store if we've actually saved the bulk data in the archive.			
		if( !Mip.Data.IsStoredInSeparateFile() )
		{
			FCookedBulkDataInfo Info;
			Info.SavedBulkDataFlags			= Mip.Data.GetSavedBulkDataFlags();
			Info.SavedElementCount			= Mip.Data.GetSavedElementCount();
			Info.SavedBulkDataOffsetInFile	= Mip.Data.GetSavedBulkDataOffsetInFile();
			Info.SavedBulkDataSizeOnDisk	= Mip.Data.GetSavedBulkDataSizeOnDisk();
			Info.TextureFileCacheName		= NAME_None;

			// TextureFormat is empty if not on Android
			if (TextureFormat == 0x0)
			{
				SetBulkDataInfo( Texture2D, *FString::Printf(TEXT("MipLevel_%i"),MipLevel), Info );
			}
			else
			{
				SetBulkDataInfo( Texture2D, *(FString::Printf(TEXT("MipLevel_%i"),MipLevel) + UCookPackagesCommandlet::GetAndroidTextureFormatName(TextureFormat)), Info );
			}			
		}
	}
}

/**
 * Stores any script SHA data that was saved during this session
 *
 * @param	PackageName Name of the package that has been saved
 */
void UPersistentCookerData::CacheScriptSHAData( const TCHAR* PackageName )
{
	// look to see if this package has any script info
	TArray<BYTE>* PackageScriptSHA = ULinkerSave::PackagesToScriptSHAMap.Find(PackageName);
	if (PackageScriptSHA)
	{
		// even if we found it, it may be empty (because there was no script code in it)
		if (PackageScriptSHA->Num() == 20)
		{
			FilenameToScriptSHA.Set(PackageName, *PackageScriptSHA);
		}
	}
}

/**
* Stores texture file cache entry info for the given object
*
* @param	Object					Object used to create unique name
* @param	CookedBulkDataInfo		Data to store
*/
void UPersistentCookerData::SetTextureFileCacheEntryInfo( 
	UObject* Object, 
	const FCookedTextureFileCacheInfo& CookedTextureFileCacheInfo )
{
	CookedTextureFileCacheInfoMap.Set( Object->GetPathName(), CookedTextureFileCacheInfo );
}

/**
* Retrieves texture file cache entry info data for given object
*
* @param	Object					Object used to create unique name
* @return	texture file cache info entry if found, NULL otherwise
*/
FCookedTextureFileCacheInfo* UPersistentCookerData::GetTextureFileCacheEntryInfo( UObject* Object )
{
	return CookedTextureFileCacheInfoMap.Find( Object->GetPathName() );
}

/**
* Updates texture file cache entry infos by saving TFC file timestamps for each entry
*
* @param	TextureCacheNameToFilenameMap	Map from texture file cache name to its archive filename
*/
void UPersistentCookerData::UpdateCookedTextureFileCacheEntryInfos( const TMap<FName,FString> TextureCacheNameToFilenameMap )
{

	for( TMap<FString,FCookedTextureFileCacheInfo>::TIterator TextureCacheInfoIt(CookedTextureFileCacheInfoMap); TextureCacheInfoIt; ++TextureCacheInfoIt )
	{
		FCookedTextureFileCacheInfo& TextureCacheInfoEntry = TextureCacheInfoIt.Value();
		const FString* TextureCacheArchiveFilename = TextureCacheNameToFilenameMap.Find(TextureCacheInfoEntry.TextureFileCacheName);
		if( TextureCacheArchiveFilename )
		{
			TextureCacheInfoEntry.LastSaved = GFileManager->GetFileTimestamp( **TextureCacheArchiveFilename );
		}
	}
}

/**
 * Adds usage entry for texture 
 *
 * @param	Package				Package the texture wants to be serialized to.
 * @param	Texture				Texture that is being used
 * @param	StoredOnceMipSize	Size in bytes for mips stored just once in special file
 * @param	DuplicatedMipSize	Size in bytes of mips that are duplicated for every usage
 * @param	SizeX				Width of texture (after taking LOD into account)
 * @param	SizeY				Height of texture (after taking LOD into account)
 */
void UPersistentCookerData::AddTextureUsageInfo( UPackage* Package, UTexture2D* Texture, INT StoredOnceMipSize, INT DuplicatedMipSize, INT SizeX, INT SizeY )
{	
	FCookedTextureUsageInfo* ExistingTextureUsageInfo = TextureUsageInfos.Find(Texture->GetPathName());
	if( ExistingTextureUsageInfo )
	{
		ExistingTextureUsageInfo->PackageNames.Add( Package->GetName() );
		// Could log if other stats are different.
	}
	else
	{
		FCookedTextureUsageInfo NewTextureUsageInfo;
		NewTextureUsageInfo.PackageNames.Add( Package->GetName() );
		NewTextureUsageInfo.Format		= Texture->Format;
		NewTextureUsageInfo.LODGroup	= Texture->LODGroup;
		NewTextureUsageInfo.SizeX		= SizeX;
		NewTextureUsageInfo.SizeY		= SizeY;
		NewTextureUsageInfo.StoredOnceMipSize = StoredOnceMipSize;
		NewTextureUsageInfo.DuplicatedMipSize = DuplicatedMipSize;
		TextureUsageInfos.Set( Texture->GetPathName(), NewTextureUsageInfo );
	}
}

/**
 * Retrieves the file time if the file exists, otherwise looks at the cached data.
 * This is used to retrieve file time of files we don't save out like empty destination files.
 * 
 * @param	Filename	Filename to look file age up
 * @return	Timestamp of file in seconds
 */
DOUBLE UPersistentCookerData::GetFileTime( const TCHAR* InFilename )
{
	FFilename Filename = FFilename(InFilename).GetBaseFilename();
	// Look up filename in cache.
	DOUBLE* FileTimePtr = FilenameToTimeMap.Find( *Filename );
	// If found, return cached file age.
	if( FileTimePtr )
	{
		return *FileTimePtr;
	}
	// If it wasn't found, fall back to using filemanager.
	else
	{
		return GFileManager->GetFileTimestamp( *Filename );
	}
}

/**
 * Sets the time of the passed in file.
 *
 * @param	Filename		Filename to set file age
 * @param	FileTime		Time to set
 */
void UPersistentCookerData::SetFileTime( const TCHAR* InFilename, DOUBLE FileTime )
{
	FFilename Filename = FFilename(InFilename).GetBaseFilename();
	FilenameToTimeMap.Set( *Filename, FileTime );
}


/**
 * Retrieves the cooked version of a previously cooked file.
 * This is used to retrieve the version so we don't have to open the package to get it
 * 
 * @param	Filename	Filename to look version up
 * @return	Cooked version of the file, or 0 if it doesn't exist
 */
INT UPersistentCookerData::GetFileCookedVersion( const TCHAR* Filename )
{
	// look up the version
	INT* Version = FilenameToCookedVersion.Find(Filename);

	// return the version, or 0 if the file didn't exist
	if (Version == NULL)
	{
		return 0;
	}

	return *Version;
}

/**
 * Sets the version of the passed in file.
 *
 * @param	Filename		Filename to set version
 * @param	CookedVersion	Version to set
 */
void UPersistentCookerData::SetFileCookedVersion( const TCHAR* Filename, INT CookedVersion )
{
	FilenameToCookedVersion.Set( Filename, CookedVersion );
}

/**
 * Retrieves the previously cooked texture formats
 * 
 * @param	TextureFormats	The formats to check against
 * @return	Whether or not TextureFormats matches the saved flags
 */
UBOOL UPersistentCookerData::AreTextureFormatsCooked( DWORD TextureFormats )
{
	return (CookedTextureFormats == TextureFormats);
}

/**
 * Sets the cooked texture formats
 *
 * @param	TextureFormats	The formats that have been cooked
 */
void UPersistentCookerData::SetCookedTextureFormats( DWORD TextureFormats)
{
	CookedTextureFormats = TextureFormats;
}

/**
 * Copy the required information from another PCD over the top of our information
 * Assumed to be used by children cook process 
 * @param Other PCD containing data to copy
 */
void UPersistentCookerData::CopyInfo(UPersistentCookerData* Other)
{
	CopyCookedStartupObjects(Other);
	CopyTFCInfo(Other);
}

/**
 * Copy the cooked always loaded information from another PCD over the top of our information
 *
 * @param Other PCD containing always loaded data to copy
 */
void UPersistentCookerData::CopyCookedStartupObjects(UPersistentCookerData* Other)
{
	CookedStartupObjects = Other->CookedStartupObjects;
	CookedStartupObjectsLoc = Other->CookedStartupObjectsLoc;
	AlreadyHandledStartupMaterials = Other->AlreadyHandledStartupMaterials;
	AlreadyHandledStartupMaterialInstances = Other->AlreadyHandledStartupMaterialInstances;
}

/**
 * Copy the TFC information from another PCD over the top of our information
 *
 * @param Other PCD containing TFC data to copy
 */
void UPersistentCookerData::CopyTFCInfo(UPersistentCookerData* Other)
{
	for (TMap<FString, FCookedBulkDataInfo>::TIterator It(CookedBulkDataInfoMap); It; ++It)
	{
		// make sure every local TFC entry was remapped to a global one
		if (It.Value().TextureFileCacheName.GetNumber() > 0)
		{
			FCookedBulkDataInfo* FromParent = Other->CookedBulkDataInfoMap.Find(It.Key());
			if (!(FromParent && FromParent->TextureFileCacheName.GetNumber() < 1))
			{
				debugf(TEXT("Parent did not give me new TFC info for %s %s %d"),
					*It.Key(),
					*It.Value().TextureFileCacheName.ToString(),
					INT(!FromParent)
					);
				check(FromParent && FromParent->TextureFileCacheName.GetNumber() < 1);
			}
		}
	}

	CookedBulkDataInfoMap = Other->CookedBulkDataInfoMap;
	CookedTextureFileCacheInfoMap = Other->CookedTextureFileCacheInfoMap;
	if (Other->TextureUsageInfos.Num())
	{
		TextureUsageInfos = Other->TextureUsageInfos;
	}
}

/**
 * Check the TFC information to see if we need to sync with the parent
 *
 * @return TRUE if we need to sync with the parent process
 */
UBOOL UPersistentCookerData::NeedsTFCSync()
{
	for (TMap<FString, FCookedBulkDataInfo>::TIterator It(CookedBulkDataInfoMap); It; ++It)
	{
		if (It.Value().TextureFileCacheName.GetNumber() > 0)
		{
			// this is a local TFC entry
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Merge another persistent cooker data object into this one
 *
 * @param Other Other persistent cooker data object to merge in
 * @param Commandlet Commandlet object that is used to get archive for the base texture file cache
 * @param OtherDirectory Directory used to find any TextureFileCache's used by the Other cooker data
 */
void UPersistentCookerData::Merge(UPersistentCookerData* Other, UCookPackagesCommandlet* Commandlet, const TCHAR* OtherDirectory, INT ChildIndex)
{
	// This now has behavior customized for parallel cooks and shouldn't be used for anything else

	// Note this merge is not complete and is not intended to be complete.
	// Child jobs contribute nothing to the final state of the cooked build except the cooked packages, TFC entires and hash entires.
	// If they did, this would violate the fundamental assumption that child jobs are independent anyway.
	// In some case this information is generated prior to the child jobs and all children start with this GPCD info.
	// In other cases, the information needs to be generated on the fly (perhaps redundantly on many processes).

	// go over other bulk data and merge into this
	TMap<FString,FArchive *> OpenTFCs;
	for (TMap<FString, FCookedBulkDataInfo>::TIterator It(Other->CookedBulkDataInfoMap); It; ++It)
	{
		FCookedBulkDataInfo& New = It.Value();
		INT MipOffset= It.Key().InStr(TEXT(".MipLevel_"),FALSE,TRUE);
		check(MipOffset != INDEX_NONE);
		FString TextureBaseName = It.Key().Left(MipOffset);
		FCookedTextureFileCacheInfo* NewTFCInfo = Other->CookedTextureFileCacheInfoMap.Find(TextureBaseName);
		FCookedBulkDataInfo* Existing = CookedBulkDataInfoMap.Find(It.Key());
		FCookedTextureFileCacheInfo* ExistingTFCInfo = CookedTextureFileCacheInfoMap.Find(TextureBaseName);

		if (Existing && NewTFCInfo)
		{
			// lets see if the existing entry is stale
			if (!ExistingTFCInfo || ExistingTFCInfo->TextureFileCacheGuid != NewTFCInfo->TextureFileCacheGuid )
			{
				// child jobs cannot have stale TFC entries, so if there is a mismatch, child wins
				TextureFileCacheWaste += Existing->SavedBulkDataSizeOnDisk;
				Existing = 0; // stale entry replace it anyway
				TextureUsageInfos.RemoveKey(TextureBaseName); // this information is also stale
				CookedTextureFileCacheInfoMap.RemoveKey(TextureBaseName); // this information is also stale
			}
		}
		if (!Existing)
		{
			if (New.TextureFileCacheName != NAME_None && New.TextureFileCacheName.GetNumber() > 0) // this is a local entry that we need to copy
			{
				FName BaseName = FName(*New.TextureFileCacheName.GetNameString()); // strip off the TFC number 
				FArchive* BaseTextureFileCache = Commandlet->GetTextureCacheArchive(BaseName);
				FString JobTFCName = FString(OtherDirectory) * New.TextureFileCacheName.ToString() + TEXT(".") + GSys->TextureFileCacheExtension;
				if (!OpenTFCs.Find(JobTFCName))
				{
					OpenTFCs.Set(JobTFCName,GFileManager->CreateFileReader(*JobTFCName));
				}
				FArchive** OtherTextureFileCache = OpenTFCs.Find(JobTFCName);
				check(OtherTextureFileCache && *OtherTextureFileCache);

				// we can get the offset into the other TFC from the info
				check(New.SavedBulkDataOffsetInFile >= 0);

				(*OtherTextureFileCache)->Seek(New.SavedBulkDataOffsetInFile);
				check(New.SavedBulkDataSizeOnDisk > 0);

				// read it in, aligned to ECC_BLOCK_SIZE
				INT SizeToRead = Align(New.SavedBulkDataSizeOnDisk, Commandlet->TextureFileCacheBulkDataAlignment);
				TArray<BYTE> MipData;
				MipData.Add(SizeToRead);
				(*OtherTextureFileCache)->Serialize(MipData.GetData(), SizeToRead);

				// update the info
				New.SavedBulkDataOffsetInFile = BaseTextureFileCache->Tell();
				New.TextureFileCacheName = BaseName;
				check((New.SavedBulkDataOffsetInFile % Commandlet->TextureFileCacheBulkDataAlignment) == 0);

				// the base TFC is ready to be written to
				BaseTextureFileCache->Serialize(MipData.GetData(), SizeToRead);
			}
			// not possible for this info to be stale, so we take it regardless if we don't have an entry
			CookedBulkDataInfoMap.Set(It.Key(), New);
		}
	}
	for (TMap<FString, FCookedBulkDataInfo>::TIterator It(Other->CookedBulkDataInfoMap); It; ++It)
	{
		FCookedBulkDataInfo* Existing = CookedBulkDataInfoMap.Find(It.Key());
		check(Existing); // this really needs to be here
		check(Existing->TextureFileCacheName.GetNumber() < 1); // and it better have been remapped
	}
	for (TMap<FString, FCookedBulkDataInfo>::TIterator It(CookedBulkDataInfoMap); It; ++It)
	{
		check(It.Value().TextureFileCacheName.GetNumber() < 1); // There should be no numbered TFCs
	}
	for (TMap<FString,FCookedTextureFileCacheInfo>::TIterator It(Other->CookedTextureFileCacheInfoMap); It; ++It)
	{
		CookedTextureFileCacheInfoMap.Set(It.Key(),It.Value());  // child process can't be stale and always wins, so just set it
	}
	for (TMap<FString,FCookedTextureUsageInfo>::TIterator It(Other->TextureUsageInfos); It; ++It)
	{
		FCookedTextureUsageInfo* OldUsage = TextureUsageInfos.Find(It.Key());
		if (!OldUsage)
		{
			TextureUsageInfos.Set(It.Key(),It.Value());  
		}
		else
		{   
			// merge in package usage list
			for (TSet<FString>::TIterator ItInner(It.Value().PackageNames); ItInner; ++ItInner)
			{
				OldUsage->PackageNames.Add(*ItInner);
			}
		}
	}
	for (TMap<FString,FArchive *>::TIterator It(OpenTFCs); It; ++It)
	{
		delete It.Value();
		Commandlet->WaitToDeleteFile(*It.Key());
	}
	OpenTFCs.Empty();

	// go over other filetimes and merge into this
	for (TMap<FString, DOUBLE>::TIterator It(Other->FilenameToTimeMap); It; ++It)
	{
		// child processes always have up-to-date information, if they have information
		FilenameToTimeMap.Set(It.Key(), It.Value());
	}

	// go over other filetimes and merge into this
	for (TMap<FString, INT>::TIterator It(Other->FilenameToCookedVersion); It; ++It)
	{
		// child processes always have up-to-date information, if they have information
		FilenameToCookedVersion.Set(It.Key(), It.Value());
	}
	for (TMap<FString,TArray<BYTE> >::TIterator It(Other->FilenameToScriptSHA); It; ++It)
	{
		FilenameToScriptSHA.Set(It.Key(), It.Value());
	}

	// Copy over cooked formats
	CookedTextureFormats = Other->CookedTextureFormats;
}

/**
 *	Copy errors and warnings from GWarn into the PCD for sending to parent
 */
void UPersistentCookerData::PackWarningsAndErrorsForParent()
{
	ChildCookErrors.Empty();
	for(; LastSentError < GWarn->Errors.Num(); LastSentError++)
	{
		ChildCookErrors.AddItem(GWarn->Errors(LastSentError));
	}
	ChildCookWarnings.Empty();
	for(; LastSentWarning < GWarn->Warnings.Num(); LastSentWarning++)
	{
		ChildCookWarnings.AddItem(GWarn->Warnings(LastSentWarning));
	}
}


/**
 *	Re-emit warnings and errors from child
 *
 * @param Prefix  purely for display, prefix for errors and warnings
 */
void UPersistentCookerData::TelegraphWarningsAndErrorsFromChild(const TCHAR *Prefix)
{
	for (INT Index = 0; Index < ChildCookWarnings.Num(); Index++)
	{
		warnf(NAME_Warning,TEXT("%s %s"),Prefix,*ChildCookWarnings(Index));
	}
	ChildCookWarnings.Empty();
	for (INT Index = 0; Index < ChildCookErrors.Num(); Index++)
	{
		warnf(NAME_Error,TEXT("%s %s"),Prefix,*ChildCookErrors(Index));
	}
	ChildCookErrors.Empty();
}



/**
 *	Retrieve the CookedPrefixCommonInfo for the given common package name
 *
 *	@param	InCommonName			The name of the common package of interest
 *	@param	bInCreateIfNotFound		If TRUE, a new info set will be created for the common package if not found
 *
 *	@return	FForceCookedInfo*		Pointer to the info if found, NULL if not
 */
FForceCookedInfo* UPersistentCookerData::GetCookedPrefixCommonInfo(const FString& InCommonName, UBOOL bInCreateIfNotFound)
{
	FString FindString = InCommonName.ToUpper();
	FForceCookedInfo* ReturnInfo = CookedPrefixCommonInfoMap.Find(FindString);
	if ((ReturnInfo == NULL) && (bInCreateIfNotFound == TRUE))
	{
		FForceCookedInfo TempInfo;
		CookedPrefixCommonInfoMap.Set(FindString, TempInfo);
		ReturnInfo = CookedPrefixCommonInfoMap.Find(FindString);
		check(ReturnInfo != NULL);
	}
	return ReturnInfo;
}

/**
 *	Retrieve the forced object list for the given PMap name
 *
 *	@param	InPMapName				The name of the PMap of interest
 *	@param	bInCreateIfNotFound		If TRUE, a new info set will be created for the PMap if not found
 *
 *	@return	FForceCookedInfo*		Pointer to the info if found, NULL if not
 */
FForceCookedInfo* UPersistentCookerData::GetPMapForcedObjectInfo(const FString& InPMapName, UBOOL bInCreateIfNotFound)
{
	FString FindString = InPMapName.ToUpper();
	FForceCookedInfo* ReturnInfo = PMapForcedObjectsMap.Find(FindString);
	if ((ReturnInfo == NULL) && (bInCreateIfNotFound == TRUE))
	{
		FForceCookedInfo TempInfo;
		PMapForcedObjectsMap.Set(FindString, TempInfo);
		ReturnInfo = PMapForcedObjectsMap.Find(FindString);
		check(ReturnInfo != NULL);
	}
	return ReturnInfo;
}

/** 
 *	Get the map of objects cooked into the given startup (always loaded) package.
 *
 *	@param	InPackageName			The name of the always loaded package
 *	@param	bInCreateIfMissing		If TRUE and the mapping isn't found, create one for it
 *
 *	@return	TMap<FString,FString>*	The mapping
 */
TMap<FString,FString>* UPersistentCookerData::GetCookedAlwaysLoadedMapping(const FString& InPackageName, UBOOL bInCreateIfMissing)
{
	TMap<FString,FString>* FoundMapping = CookedStartupObjects.Find(InPackageName);
	if ((FoundMapping == NULL) && (bInCreateIfMissing == TRUE))
	{
		TMap<FString,FString> TempFoundMapping;
		CookedStartupObjects.Set(InPackageName, TempFoundMapping);
		FoundMapping = CookedStartupObjects.Find(InPackageName);
	}
	return FoundMapping;
}

/** 
 *	Get the map of objects cooked into the given startup (always loaded) package.
 *
 *	@param	InPackageName			The name of the always loaded package
 *	@param	bInCreateIfMissing		If TRUE and the mapping isn't found, create one for it
 *
 *	@return	TMap<FString,FString>*	The mapping
 */
TMap<FString,FString>* UPersistentCookerData::GetCookedAlwaysLoadedLocMapping(const FString& InPackageName, const FString& InLanguage, UBOOL bInCreateIfMissing)
{
	TMap<FString,FString>* FoundMapping = NULL;

	TMap<FString,TMap<FString,FString> >* LocMapping = CookedStartupObjectsLoc.Find(InLanguage);
	if ((LocMapping == NULL) && (bInCreateIfMissing == TRUE))
	{
		TMap<FString,TMap<FString,FString> > TempLocMapping;
		CookedStartupObjectsLoc.Set(InLanguage, TempLocMapping);
		LocMapping = CookedStartupObjectsLoc.Find(InLanguage);
	}

	if (LocMapping != NULL)
	{
		FoundMapping = LocMapping->Find(InPackageName);
		if ((FoundMapping == NULL) && (bInCreateIfMissing == TRUE))
		{
			TMap<FString,FString> TempFoundMapping;
			LocMapping->Set(InPackageName, TempFoundMapping);
			FoundMapping = LocMapping->Find(InPackageName);
		}
	}
	return FoundMapping;
}

IMPLEMENT_CLASS(UPersistentCookerData);

/**
 *	Create an instance of the persistent cooker data given a filename. 
 *	First try to load from disk and if not found will construct object and store the 
 *	filename for later use during saving.
 *
 *	The cooker will call this first, and if it returns NULL, it will use the standard
 *		UPersistentCookerData::CreateInstance function. 
 *	(They are static hence the need for this)
 *
 * @param	Filename					Filename to use for serialization
 * @param	bCreateIfNotFoundOnDisk		If FALSE, don't create if couldn't be found; return NULL.
 * @return								instance of the container associated with the filename
 */
UPersistentCookerData* FGameCookerHelperBase::CreateInstance( const TCHAR* Filename, UBOOL bCreateIfNotFoundOnDisk )
{
	return UPersistentCookerData::CreateInstance( Filename, bCreateIfNotFoundOnDisk );
}
