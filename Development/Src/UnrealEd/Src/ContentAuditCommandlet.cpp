/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "EngineMaterialClasses.h"
#include "EngineSequenceClasses.h"
#include "UnPropertyTag.h"
#include "EngineUIPrivateClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineParticleClasses.h"
#include "LensFlare.h"
#include "EngineAnimClasses.h"
#include "UnTerrain.h"
#include "EngineFoliageClasses.h"
#include "SpeedTree.h"
#include "EnginePrefabClasses.h"
#include "Database.h"
#include "EngineSoundClasses.h"

#include "SourceControl.h"

#include "PackageHelperFunctions.h"
#include "PackageUtilityWorkers.h"

#include "PerfMem.h"
#include "AnimationEncodingFormat.h"
#include "AnimationUtils.h"

#include "UnFile.h"
#include "DiagnosticTable.h"

#if WITH_MANAGED_CODE
#include "GameAssetDatabaseShared.h"

FGADHelper CommandletGADHelper;

#endif // WITH_MANAGED_CODE



IMPLEMENT_COMPARE_CONSTREF( FString, ContentAuditCommandlet, { return appStricmp(*A,*B); } )

/**
 *	Helper class for Content Audit commandlets 
 */
class FAuditHelper
{
public:
	/** Helper for tracking and tagging assets */
	struct FAuditHelperTagInfo
	{
		FAuditHelperTagInfo(const FString& InTagName, const FString& InWhitelistTagName) :
			  TagName(InTagName)
			, WhitelistTagName(InWhitelistTagName)
		{
		}

		/** The name of the tag */
		FString TagName;
		/** The name of the whitelist tag associated with this tag */
		FString WhitelistTagName;
		/** Map of the full names of objects to tag */
		TMap<FString,UBOOL> AssetsToTag;
	};

	/** The tags to apply to the given assets */
	TArray<FAuditHelperTagInfo> Tags;

	/**
	 *	Get the tag info for the given name
	 *
	 *	@param	InTagName				Name of the tag to get the info for
	 *
	 *	@return	FAuditHelperTagInfo*	The info for the given tag, NULL if not found
	 */
	virtual FAuditHelperTagInfo* FindTagInfo(const FString& InTagName)
	{
		for (INT TagIdx = 0; TagIdx < Tags.Num(); TagIdx++)
		{
			FAuditHelperTagInfo& TagInfo = Tags(TagIdx);
			if (TagInfo.TagName == InTagName)
			{
				return &TagInfo;
			}
		}
		return NULL;
	}

	/** Cleaup the GAD tags in the list */
	virtual void CleanUpGADTags()
	{
		// Used to clear the tags we are about to set
		for (INT TagIdx = 0; TagIdx < Tags.Num(); TagIdx++)
		{
			FAuditHelperTagInfo& TagInfo = Tags(TagIdx);
			UpdateGADClearTags(TagInfo.TagName);
		}
	}

	/**
	 *	Process the given object.
	 *
	 *	@param	Commandlet		The commandlet that is calling into this object
	 *	@param	Package			The package that is being processed
	 *	@param	InObject		The object that is being processed
	 *	@param	Tokens			The Tokens from the commandline
	 *	@param	Switches		The Switches from the commandline
	 */
	virtual void ProcessObject(UCommandlet* Commandlet, UPackage* Package, UObject* InObject, UBOOL bVerbose, TArray<FString>& Tokens, TArray<FString>& Switches)
	{
		// Fill this in with the processing of the object given
	}

	/**
	 *	Update the GAD tags
	 *
	 *	@param	Commandlet		The commandlet that is calling into this object
	 *	@param	Tokens			The Tokens from the commandline
	 *	@param	Switches		The Switches from the commandline
	 */
	virtual void UpdateGADTags(UCommandlet* Commandlet, TArray<FString>& Tokens, TArray<FString>& Switches)
	{
		/** This will set the passed in tag name on the objects if they are not in the whitelist **/
		for (INT TagIdx = 0; TagIdx < Tags.Num(); TagIdx++)
		{
			FAuditHelperTagInfo& TagInfo = Tags(TagIdx);
			UpdateGADSetTagsForObjects(TagInfo.AssetsToTag, TagInfo.TagName, TagInfo.WhitelistTagName);
		}
	}
};

/** Helper function for adding AuditHelpers to the given map */
void ContentAuditCommandlet_AddHelperToMap(UClass* InClass, FAuditHelper* InHelper, TMap<UClass*,TArray<FAuditHelper*> >& InOutHelperMap)
{
	TArray<FAuditHelper*>* HelperList = InOutHelperMap.Find(InClass);
	if (HelperList == NULL)
	{
		TArray<FAuditHelper*> TempHelperList;
		InOutHelperMap.Set(InClass, TempHelperList);
		HelperList = InOutHelperMap.Find(InClass);
	}
	check(HelperList);
	HelperList->AddItem(InHelper);
}

/**
 */
void DoActionsToAllObjectsInPackages(UCommandlet* Commandlet, const FString& Params, TMap<UClass*,TArray<FAuditHelper*> >& ObjToHelperMap)
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	warnf(TEXT("%s"), *Params);

	const TCHAR* Parms = *Params;
	UCommandlet::ParseCommandLine(Parms, Tokens, Switches);

	const UBOOL bVerbose = Switches.ContainsItem(TEXT("VERBOSE"));
	const UBOOL bLoadMaps = Switches.ContainsItem(TEXT("LOADMAPS"));
	const UBOOL bOverrideLoadMaps = Switches.ContainsItem(TEXT("OVERRIDELOADMAPS"));
	const UBOOL bOnlyLoadMaps = Switches.ContainsItem(TEXT("ONLYLOADMAPS"));
	const UBOOL bSkipReadOnly = Switches.ContainsItem(TEXT("SKIPREADONLY"));
	const UBOOL bOverrideSkipOnly = Switches.ContainsItem(TEXT("OVERRIDEREADONLY"));
	const UBOOL bGCEveryPackage = Switches.ContainsItem(TEXT("GCEVERYPACKAGE"));
	const UBOOL bSaveShaderCaches = Switches.ContainsItem(TEXT("SAVESHADERS"));
	const UBOOL bSkipScriptPackages = Switches.ContainsItem(TEXT("SKIPSCRIPTPACKAGES"));
	const UBOOL bSkipTagUpdates = Switches.ContainsItem(TEXT("SKIPTAGS"));
	const UBOOL bLogTagUpdates = Switches.ContainsItem(TEXT("LOGTAGS"));
	const UBOOL bSilent = Switches.ContainsItem(TEXT("SILENT"));

	TArray<FString> FilesInPath;
	FilesInPath = GPackageFileCache->GetPackageFileList();

#if WITH_MANAGED_CODE
	CommandletGADHelper.Initialize();
#endif // WITH_MANAGED_CODE

	if ((bSkipTagUpdates == FALSE) || (bLogTagUpdates == TRUE))
	{
		// Clear all the tags
		if (bLogTagUpdates == TRUE)
		{
			warnf(NAME_Log, TEXT("ContentAudit Commandlet Class List"));
		}
		for (TMap<UClass*,TArray<FAuditHelper*> >::TIterator ClearIt(ObjToHelperMap); ClearIt; ++ClearIt)
		{
			UClass* Class = ClearIt.Key();
			if (bLogTagUpdates == TRUE)
			{
				warnf(NAME_Log, TEXT("\tClass %s"), *(Class->GetName()));
			}
			TArray<FAuditHelper*>& HelperList = ClearIt.Value();
			for (INT ClearIdx = 0; ClearIdx < HelperList.Num(); ClearIdx++)
			{
				FAuditHelper* Helper = HelperList(ClearIdx);
				if (Helper != NULL)
				{
					if (bLogTagUpdates == TRUE)
					{
						for (INT TagIdx = 0; TagIdx < Helper->Tags.Num(); TagIdx++)
						{
							FAuditHelper::FAuditHelperTagInfo& TagInfo = Helper->Tags(TagIdx);
							warnf(NAME_Log, TEXT("\t\tTAG = %s"), *(TagInfo.TagName));
							warnf(NAME_Log, TEXT("\t\t\tWHITELIST = %s"), *(TagInfo.WhitelistTagName));
						}
					}
					if (bSkipTagUpdates == FALSE)
					{
						Helper->CleanUpGADTags();
					}
				}
			}
		}
	}

	INT GCIndex = 0;
	for (INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++)
	{
		const FFilename& Filename = FilesInPath(FileIndex);

		const UBOOL	bIsShaderCacheFile		= FString(*Filename).ToUpper().InStr( TEXT("SHADERCACHE") ) != INDEX_NONE;
		const UBOOL	bIsAutoSave				= FString(*Filename).ToUpper().InStr( TEXT("AUTOSAVES") ) != INDEX_NONE;
		const UBOOL bIsReadOnly             = GFileManager->IsReadOnly( *Filename );

		// we don't care about trying to wrangle the various shader caches so just skipz0r them
		if (	(Filename.GetBaseFilename().InStr(TEXT("LocalShaderCache")) != INDEX_NONE)
			||	(Filename.GetBaseFilename().InStr(TEXT("RefShaderCache")) != INDEX_NONE)
			||	(bIsAutoSave == TRUE)
			)
		{
			continue;
		}

		// Skip over script packages
		if (bSkipScriptPackages && (Filename.GetExtension() == TEXT("u")))
		{
			continue;
		}

		// See if we should skip read only packages
		if (bSkipReadOnly && !bOverrideSkipOnly)
		{
			if (GFileManager->IsReadOnly(*Filename))
			{
				warnf(TEXT("Skipping %s (read-only)"), *Filename);			
				continue;
			}
		}

		// if we don't want to load maps for this
		if (((!bLoadMaps && !bOnlyLoadMaps) || bOverrideLoadMaps) && (Filename.GetExtension() == FURL::DefaultMapExt))
		{
			continue;
		}

		// if we only want to load maps for this
		if ((bOnlyLoadMaps == TRUE) && (Filename.GetExtension() != FURL::DefaultMapExt))
		{
			continue;
		}

		if (bSilent == FALSE)
		{
			warnf(NAME_Log, TEXT("Loading %4d of %4d: %s"), FileIndex, FilesInPath.Num(), *Filename);
		}

		// don't die out when we have a few bad packages, just keep on going so we get most of the data
		try
		{
			UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_None );
			if (Package != NULL)
			{
				for (FObjectIterator ObjIt; ObjIt; ++ObjIt)
				{
					UObject* Object = *ObjIt;
					if (Object != NULL)
					{
						UClass* ObjClass = Object->GetClass();
						for (TMap<UClass*,TArray<FAuditHelper*> >::TIterator CheckIt(ObjToHelperMap); CheckIt; ++CheckIt)
						{
							UClass* CheckClass = CheckIt.Key();
							if (Object->IsA(CheckClass) == TRUE)
							{
								TArray<FAuditHelper*>& HelperList = CheckIt.Value();
								for (INT HelperIdx = 0; HelperIdx < HelperList.Num(); HelperIdx++)
								{
									FAuditHelper* Helper = HelperList(HelperIdx);
									if (Helper != NULL)
									{
										Helper->ProcessObject(Commandlet, Package, Object, bVerbose, Switches, Tokens);
									}
								}
							}
						}
					}
				}
			}
			else
			{
				warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
			}
		}
		catch ( ... )
		{
			warnf( NAME_Log, TEXT("Exception %s"), *Filename.GetBaseFilename() );
		}

		if( ( (++GCIndex % 10) == 0 ) || ( bGCEveryPackage == TRUE ) )
		{
			UObject::CollectGarbage(RF_Native);
		}

		if (bSaveShaderCaches == TRUE)
		{
			SaveLocalShaderCaches();
		}
	}

	// Update all the tags
	for (TMap<UClass*,TArray<FAuditHelper*> >::TIterator UpdateIt(ObjToHelperMap); UpdateIt; ++UpdateIt)
	{
		UClass* Class = UpdateIt.Key();
		TArray<FAuditHelper*>& HelperList = UpdateIt.Value();
		for (INT ClearIdx = 0; ClearIdx < HelperList.Num(); ClearIdx++)
		{
			FAuditHelper* Helper = HelperList(ClearIdx);
			if (Helper != NULL)
			{
				if (bLogTagUpdates == TRUE)
				{
					for (INT TagIdx = 0; TagIdx < Helper->Tags.Num(); TagIdx++)
					{
						FAuditHelper::FAuditHelperTagInfo& TagInfo = Helper->Tags(TagIdx);
						warnf(NAME_Log, TEXT("TAG = %s"), *(TagInfo.TagName));
						for (TMap<FString,UBOOL>::TIterator ObjIt(TagInfo.AssetsToTag); ObjIt; ++ObjIt)
						{
							warnf(NAME_Log, TEXT("\t%s"), *(ObjIt.Key()));
						}
					}
				}
				if (bSkipTagUpdates == FALSE)
				{
					Helper->UpdateGADTags(Commandlet, Tokens, Switches);
				}
			}
		}
	}

#if WITH_MANAGED_CODE
	CommandletGADHelper.Shutdown();
#endif // WITH_MANAGED_CODE
}

/**
 * This will look for any tags with Audit. in their name and then output them in a format
 * that the build system will be able to send out in email.
 **/
static void OutputAllAuditSummary()
{
#if WITH_MANAGED_CODE
	CommandletGADHelper.Initialize();

	// get all of the tags back
	TArray<FString> AllTags;
	FGameAssetDatabase::Get().QueryAllTags( AllTags );

	TArray<FString> AuditTags;

	for( INT i = 0; i < AllTags.Num(); ++i )
	{
		const FString& ATag = AllTags(i);		

		// then look for ones with Audit. in their name
		if( ATag.InStr( TEXT("Audit.") ) != INDEX_NONE )
		{
			AuditTags.AddItem( ATag );
		}
	}

	// sort them here
	Sort<USE_COMPARE_CONSTREF( FString, ContentAuditCommandlet )>( AuditTags.GetTypedData(), AuditTags.Num() );

	warnf( TEXT( "[REPORT] All Audit Tags and Their Counts" ) );

	// then query for membership
	for( INT i = 0; i < AuditTags.Num(); ++i )
	{
		const FString& ATag = AuditTags(i);		

		TArray<FString> AllAssetsTagged;

		CommandletGADHelper.QueryTaggedAssets( ATag, AllAssetsTagged );


// Experiment with getting just the OnDVD tagged asssets to be in the log
//
//		TArray<FString> OnDVDPlusTag;
//		OnDVDPlusTag.AddItem( ATag );
//		OnDVDPlusTag.AddItem( FString( TEXT( "Audit.OnDVD (Xbox360)" ) ) );
//
//		TArray<FString> AllAssetsTagged;
//
//		CommandletGADHelper.QueryAssetsWithAllTags( OnDVDPlusTag, AllAssetsTagged );
//

		warnf( TEXT( "[REPORT] %s: %d" ), *ATag, AllAssetsTagged.Num() );
	}

#endif // WITH_MANAGED_CODE
}

/**
 * This will look for Textures which:
 *
 *   0) are probably specular (based off their name) and see if they have an LODBias of two
 *   1) have a negative LODBias
 *   2) have neverstream set
 *   3) a texture which looks to be a normalmap but doesn't have the correct texture compression set 
 *
 * All of the above are things which can be considered suspicious and probably should be changed
 * for best memory usage.  
 *
 * Specifically:  Specular with an LODBias of 2 was not noticeably different at the texture resolutions we used for gears; 512+
 *
 */
class FTextureAuditHelper : public FAuditHelper
{
public:
	FTextureAuditHelper()
	{
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.Texture_NormalmapNonDXT1CompressionNoAlphaIsTrue"), TEXT("Audit.Texture_NormalmapNonDXT1CompressionNoAlphaIsTrue_Whitelist"));
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.Texture_LODBiasIsNegative"), TEXT("Audit.Texture_LODBiasIsNegative_Whitelist"));
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.Texture_NeverStream"), TEXT("Audit.Texture_NeverStream_Whitelist"));
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.Texture_G8"), TEXT("Audit.Texture_G8_Whitelist"));
	}

	virtual void ProcessObject(UCommandlet* Commandlet, UPackage* Package, UObject* InObject, UBOOL bVerbose, TArray<FString>& Tokens, TArray<FString>& Switches)
	{
		UTexture2D* Texture2D = Cast<UTexture2D>(InObject);
		if ((Texture2D == NULL)|| (Texture2D->IsIn(Package) == FALSE))
		{
			return;
		}

		// if we are a TEXTUREGROUP_Cinematic then we don't care as that will be some crazy size / settings to look awesome :-)
		// TEXTUREGROUP_Cinematic:  implies being baked out
		if (Texture2D->LODGroup == TEXTUREGROUP_Cinematic)
		{
			return;
		}

		const FString&  TextureFullName = Texture2D->GetFullName();
		const FString&  TextureName = Texture2D->GetPathName();
		const INT		LODBias     = Texture2D->LODBias;

		FString OrigDescription = TEXT( "" );
		TArray<FString> TextureGroupNames = FTextureLODSettings::GetTextureGroupNames();
		if( Texture2D->LODGroup < TextureGroupNames.Num() )
		{
			OrigDescription = TextureGroupNames(Texture2D->LODGroup);
		}

		//warnf( TEXT( " checking %s" ), *TextureName );

		FAuditHelperTagInfo* TagInfo_NormalmapNonDXT1CompressionNoAlphaIsTrue = FindTagInfo(TEXT("Audit.Texture_NormalmapNonDXT1CompressionNoAlphaIsTrue"));
		FAuditHelperTagInfo* TagInfo_LODBiasIsNegative = FindTagInfo(TEXT("Audit.Texture_LODBiasIsNegative"));
		FAuditHelperTagInfo* TagInfo_NeverStream = FindTagInfo(TEXT("Audit.Texture_NeverStream"));
		FAuditHelperTagInfo* TagInfo_G8 = FindTagInfo(TEXT("Audit.Texture_G8"));
		check(	TagInfo_NormalmapNonDXT1CompressionNoAlphaIsTrue && 
				TagInfo_LODBiasIsNegative && 
				TagInfo_NeverStream && 
				TagInfo_G8
				);

		// if this has been named as a specular texture
		if ((	(TextureName.ToUpper().InStr(TEXT("SPEC")) != INDEX_NONE)  // gears
			||	(TextureName.ToUpper().InStr(TEXT("_S0")) != INDEX_NONE)  // ut
			||	(TextureName.ToUpper().InStr(TEXT("_S_")) != INDEX_NONE)  // ut
			||	((TextureName.ToUpper().Right(2)).InStr(TEXT("_S")) != INDEX_NONE)  // ut
			)
			&& (!((LODBias == 0) // groups are in charge of the spec LODBias
			//	|| (Texture2D->LODGroup == TEXTUREGROUP_WorldSpecular)
				|| (Texture2D->LODGroup == TEXTUREGROUP_CharacterSpecular)  // we hand set these
				|| (Texture2D->LODGroup == TEXTUREGROUP_WeaponSpecular) // we hand set these
			//	|| (Texture2D->LODGroup == TEXTUREGROUP_VehicleSpecular)
			)
			)
			)		 
		{
			if (bVerbose == TRUE)
			{
				warnf( TEXT("%s:  Desired LODBias of 2 not correct.  ( Currently has %d )  OR not set to a SpecularTextureGroup (Currently has: %d (%s))"), *TextureName, LODBias, Texture2D->LODGroup, *OrigDescription );
			}
		}

		if( (
			( TextureName.ToUpper().InStr( TEXT("_N0" )) != INDEX_NONE )  // ut
			|| ( TextureName.ToUpper().InStr( TEXT("_N_" )) != INDEX_NONE )  // ut
			|| ( (TextureName.ToUpper().Right(2)).InStr( TEXT("_N" )) != INDEX_NONE )  // ut
			|| ( Texture2D->LODGroup == TEXTUREGROUP_WorldNormalMap )
			|| ( Texture2D->LODGroup == TEXTUREGROUP_CharacterNormalMap )
			|| ( Texture2D->LODGroup == TEXTUREGROUP_WeaponNormalMap )
			|| ( Texture2D->LODGroup == TEXTUREGROUP_VehicleNormalMap )
			) )
		{

			// DXT5 have an alpha channel which usually is not used.
			// so we don't want to waste memory by having it be used / in memory
			if( ( Texture2D->CompressionSettings == TC_Normalmap)
				&& (( Texture2D->Format != PF_DXT1 )  // prob DXT5
				&& ( Texture2D->CompressionNoAlpha == FALSE ) )
				)
			{
				TagInfo_NormalmapNonDXT1CompressionNoAlphaIsTrue->AssetsToTag.Set(TextureFullName, TRUE);
				if (bVerbose == TRUE)
				{
					warnf( TEXT( "%s:  Normalmap CompressionNoAlpha should probably be TRUE or should be reimported with Format as DXT1.  Current Compression is: %d.  Current Format is: %d.  You need to look at the usage of this texture and see if there are any Material nodes are using the Alpha channel." ), *TextureName, Texture2D->CompressionSettings, Texture2D->Format );
				}
			}

			// this should be automatically repaired by the SetTextureLODGroup Commandlet
			// 				// checks for TC_NormalMapAlpha
			// 				if( ( Texture2D->CompressionSettings == TC_NormalmapAlpha)
			// 					&& (( Texture2D->Format == PF_DXT1 )
			// 					|| ( Texture2D->CompressionNoAlpha != FALSE ) )
			// 					)
			// 				{
			// 					warnf( TEXT( "%s: NormalMapAlpha CompressionNoAlpha should be FALSE and Format should not be DXT1" ), *TextureName );
			// 				}
			// 
			// 				// SRGB should be false
			// 				if( Texture2D->SRGB == FALSE )
			// 				{
			// 					warnf( TEXT( "%s: Normalmap should have SRGB = FALSE"), *TextureName);
			// 				}
		}



		// if this has a negative LOD Bias that might be suspicious :-) (artists will often bump up the LODBias on their pet textures to make certain they are full full res)
		if( ( LODBias < 0 )
			)
		{
			TagInfo_LODBiasIsNegative->AssetsToTag.Set(TextureFullName, TRUE);
			if (bVerbose == TRUE)
			{
				warnf( TEXT("%s:  LODBias is negative ( Currently has %d )"), *TextureName, LODBias );
			}
		}


		// check for neverstream
		if( (  Texture2D->LODGroup != TEXTUREGROUP_UI ) // UI textures are NeverStream
			&& ( Texture2D->NeverStream == TRUE )
			&& ( TextureName.InStr( TEXT("TerrainWeightMapTexture" )) == INDEX_NONE )  // TerrainWeightMapTextures are neverstream so we don't care about them

			)
		{
			TagInfo_NeverStream->AssetsToTag.Set(TextureFullName, TRUE);
			if (bVerbose == TRUE)
			{
				warnf( TEXT("%s:  NeverStream is set to true" ), *TextureName );
			}
		}

		// if this texture is a G8 it usually can be converted to a dxt without loss of quality.  Remember a 1024x1024 G* is always 1024!
		if( Texture2D->CompressionSettings == TC_Grayscale )
		{
			TagInfo_G8->AssetsToTag.Set(TextureFullName, TRUE);
			if (bVerbose == TRUE)
			{
				warnf( TEXT("%s:  G8 texture"), *TextureName );
			}
		}
	}
};

/**
 * This will find materials which are missing Physical Materials
 */
class FMaterialMissingPhysMaterialAuditHelper : public FAuditHelper
{
public:
	FMaterialMissingPhysMaterialAuditHelper()
	{
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.Material_MissingPhysicalMaterial"), TEXT("Audit.Material_MissingPhysicalMaterial_Whitelist"));
	}

	virtual void ProcessObject(UCommandlet* Commandlet, UPackage* Package, UObject* InObject, UBOOL bVerbose, TArray<FString>& Tokens, TArray<FString>& Switches)
	{
		UMaterial* Material = Cast<UMaterial>(InObject);
		if ((Material == NULL) || (Material->IsIn(Package) == FALSE))
		{
			return;
		}

		FAuditHelperTagInfo* TagInfo_MissingPhysicalMaterial = FindTagInfo(TEXT("Audit.Material_MissingPhysicalMaterial"));
		check(TagInfo_MissingPhysicalMaterial);

		UBOOL bHasPhysicalMaterial = FALSE;

		if (Material->PhysMaterial == NULL)
		{
			TagInfo_MissingPhysicalMaterial->AssetsToTag.Set(Material->GetFullName(), TRUE);
			if (bVerbose == TRUE)
			{
				warnf(TEXT("%s:  Lacking PhysicalMaterial"), *(Material->GetPathName()));
			}
		}
	}
};

/**
 * This will find materials that have unique specular textures
 */
class FMaterialWithUniqueSpecularTextureAuditHelper : public FAuditHelper
{
public:
	FMaterialWithUniqueSpecularTextureAuditHelper()
	{
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.MaterialInterface_WithUniqueSpecularTexture"), TEXT("Audit.MaterialInterface_WithUniqueSpecularTexture_Whitelist"));
	}

	virtual void ProcessObject(UCommandlet* Commandlet, UPackage* Package, UObject* InObject, UBOOL bVerbose, TArray<FString>& Tokens, TArray<FString>& Switches)
	{
		UMaterialInterface* MtrlIntf = Cast<UMaterialInterface>(InObject);
		if ((MtrlIntf == NULL) || (MtrlIntf->IsIn(Package) == FALSE))
		{
			return;
		}

		UBOOL bAllMaterials = Switches.ContainsItem(TEXT("ALLMATERIALS"));

		FAuditHelperTagInfo* TagInfo_WithUniqueSpecularTexture = FindTagInfo(TEXT("Audit.MaterialInterface_WithUniqueSpecularTexture"));
		check(TagInfo_WithUniqueSpecularTexture);

		TArray<TArray<UTexture*>> TexturesInChains;
		TexturesInChains.Empty(MP_MAX);
		TexturesInChains.AddZeroed(MP_MAX);
		// Get the textures for ALL property chains...
		for (INT MPIdx = 0; MPIdx < MP_MAX; MPIdx++)
		{
			TArray<UTexture*>& FetchTexturesInChain = TexturesInChains(MPIdx);
			MtrlIntf->GetTexturesInPropertyChain((EMaterialProperty)MPIdx, FetchTexturesInChain, NULL, NULL);
		}

		if (bAllMaterials == FALSE)
		{
			UBOOL bIsEnvironmental = FALSE;
			// We only want to check environmental materials in this case...
			TArray<UTexture*>& DiffuseChainTextures = TexturesInChains(MP_DiffuseColor);
			for (INT DiffuseTexIdx = 0; DiffuseTexIdx < DiffuseChainTextures.Num(); DiffuseTexIdx++)
			{
				UTexture* DiffuseTexture = DiffuseChainTextures(DiffuseTexIdx);
				if (DiffuseTexture != NULL)
				{
					if (DiffuseTexture->LODGroup == TEXTUREGROUP_World)
					{
						bIsEnvironmental = TRUE;
						break;
					}
				}
			}

			if (bIsEnvironmental == FALSE)
			{
				return;
			}
		}

		// For each texture found in the specular chain, see if it is in any other chain.
		for (INT SpecTexIdx = 0; SpecTexIdx < TexturesInChains(MP_SpecularColor).Num(); SpecTexIdx++)
		{
			UTexture* CheckSpecTexture = TexturesInChains(MP_SpecularColor)(SpecTexIdx);
			UBOOL bFoundInAnotherChain = FALSE;
			for (INT MPIdx = 0; MPIdx < MP_MAX; MPIdx++)
			{
				// Skip the specular chain...
				if (MPIdx == MP_SpecularColor)
				{
					continue;
				}

				// Check all the others
				INT DummyIdx;
				if (TexturesInChains(MPIdx).FindItem(CheckSpecTexture, DummyIdx) == TRUE)
				{
					bFoundInAnotherChain = TRUE;
				}
			}

			if (bFoundInAnotherChain == FALSE)
			{
				UBOOL bFoundInParent = TRUE;

				UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(MtrlIntf);
				if (MaterialInstance && MaterialInstance->Parent)
				{
					UMaterialInterface* ParentInterface = MaterialInstance->Parent;
					// We need to make sure that the MI* is overriding the 
					// unique specular and not falsely reporting the parent
					TArray<UTexture*> IgnoredTextures;
					TArray<FName> CheckTextureParamNames;
					if (MaterialInstance->GetTexturesInPropertyChain(MP_SpecularColor, IgnoredTextures, &CheckTextureParamNames, NULL) == TRUE)
					{
						// Are there texture parameter names in the chain?
						if (CheckTextureParamNames.Num() > 0)
						{
							TArray<UTexture*> ParentTextures;
							// Are they actually being overriden on the MI*?
							if (ParentInterface->GetTexturesInPropertyChain(MP_SpecularColor, ParentTextures, NULL, NULL) == TRUE)
							{
								for (INT NameIdx = 0; NameIdx < CheckTextureParamNames.Num(); NameIdx++)
								{
									UTexture* CheckTexture;
									if (MaterialInstance->GetTextureParameterValue(CheckTextureParamNames(NameIdx), CheckTexture) == TRUE)
									{
										// Is the texture NOT in parent textures? 
										INT DummyIdx;
										if (ParentTextures.FindItem(CheckTexture, DummyIdx) == FALSE)
										{
											bFoundInParent = FALSE;
										}
									}
								}
							}
						}
					}
				}
				else
				{
					bFoundInParent = FALSE;
				}

				if (bFoundInParent == FALSE)
				{
					TagInfo_WithUniqueSpecularTexture->AssetsToTag.Set(MtrlIntf->GetFullName(), TRUE);
					break;
				}
			}
		}
	}
};

/** Will find all of the PhysicsAssets bodies which do not have a phys material assigned to them **/
class FSkeletalMeshesMissingPhysMaterialAuditHelper : public FAuditHelper
{
public:
	FSkeletalMeshesMissingPhysMaterialAuditHelper()
	{
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.PhysAsset_MissingPhysicalMaterial"), TEXT("Audit.PhysAsset_MissingPhysicalMaterial_Whitelist"));
	}

	virtual void ProcessObject(UCommandlet* Commandlet, UPackage* Package, UObject* InObject, UBOOL bVerbose, TArray<FString>& Tokens, TArray<FString>& Switches)
	{
		UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(InObject);
		if ((PhysicsAsset == NULL) || (PhysicsAsset->IsIn(Package) == FALSE))
		{
			return;
		}

		FAuditHelperTagInfo* TagInfo_MissingPhysicalMaterial = FindTagInfo(TEXT("Audit.PhysAsset_MissingPhysicalMaterial"));
		check(TagInfo_MissingPhysicalMaterial);

		for (INT i = 0; i < PhysicsAsset->BodySetup.Num(); ++i)
		{
			const UPhysicalMaterial* PhysMat = PhysicsAsset->BodySetup(i)->PhysMaterial;
			if (PhysMat == NULL)
			{
				TagInfo_MissingPhysicalMaterial->AssetsToTag.Set(PhysicsAsset->GetFullName(), TRUE);
				if (bVerbose == TRUE)
				{
					warnf( TEXT( "Missing PhysMaterial on PhysAsset: %s Bone: %s" ), *PhysicsAsset->GetPathName(), *PhysicsAsset->BodySetup(i)->BoneName.ToString() );
				}
			}
		}
	}
};

/** 
 * Useful for finding PS with bad FixedRelativeBoundingBox.  
 * Bad is either not set OR set but with defaults for the BoundingBox (e.g. turned on but no one actually set the values. 
 */
class FParticleSystemsWithBadFixedRelativeBoundingBoxAuditHelper : public FAuditHelper
{
public:
	FParticleSystemsWithBadFixedRelativeBoundingBoxAuditHelper()
	{
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.Particle_BadFixedRelativeBoundingBox"), TEXT("Audit.Particle_BadFixedRelativeBoundingBox_Whitelist"));
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.Particle_MissingFixedRelativeBoundingBox"), TEXT("Audit.Particle_MissingFixedRelativeBoundingBox_Whitelist"));
	}

	virtual void ProcessObject(UCommandlet* Commandlet, UPackage* Package, UObject* InObject, UBOOL bVerbose, TArray<FString>& Tokens, TArray<FString>& Switches)
	{
		UParticleSystem* PS = Cast<UParticleSystem>(InObject);
		if ((PS == NULL) || (PS->IsIn(Package) == FALSE))
		{
			return;
		}

		FAuditHelperTagInfo* TagInfo_BadFixedRelativeBoundingBox = FindTagInfo(TEXT("Audit.Particle_BadFixedRelativeBoundingBox"));
		FAuditHelperTagInfo* TagInfo_MissingFixedRelativeBoundingBox = FindTagInfo(TEXT("Audit.Particle_MissingFixedRelativeBoundingBox"));
		check(TagInfo_BadFixedRelativeBoundingBox && TagInfo_MissingFixedRelativeBoundingBox);

		if ( 
			(PS->bUseFixedRelativeBoundingBox == TRUE )
			&& (PS->FixedRelativeBoundingBox.Min.X == -1)
			&& (PS->FixedRelativeBoundingBox.Min.Y == -1)
			&& (PS->FixedRelativeBoundingBox.Min.Z == -1)
			&& (PS->FixedRelativeBoundingBox.Max.X ==  1)
			&& (PS->FixedRelativeBoundingBox.Max.Y ==  1)
			&& (PS->FixedRelativeBoundingBox.Max.Z ==  1)
			)
		{
			if (bVerbose == TRUE)
			{
				warnf( TEXT("PS is has bad FixedRelativeBoundingBox: %s"), *PS->GetFullName() );
			}
			TagInfo_BadFixedRelativeBoundingBox->AssetsToTag.Set(PS->GetFullName(), TRUE);
		}

		if (PS->bUseFixedRelativeBoundingBox == FALSE)
		{
			if (bVerbose == TRUE)
			{
				warnf( TEXT("PS is MissingFixedRelativeBoundingBox: %s"), *PS->GetFullName() );
			}
			TagInfo_MissingFixedRelativeBoundingBox->AssetsToTag.Set(PS->GetFullName(), TRUE);
		}
	}
};

/** 
 * Useful for finding particle modules with zombie components 
 *	The audit tag will actually go on the ParticleSystem that owns the module,
 *	since particle modules do not appear in the content browser.
 */
class FParticleModulesWithZombieComponentsAuditHelper : public FAuditHelper
{
public:
	FParticleModulesWithZombieComponentsAuditHelper()
	{
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.ParticleModule_ZombieComponents"), TEXT("Audit.ParticleModule_ZombieComponents_Whitelist"));
	}

	virtual void ProcessObject(UCommandlet* Commandlet, UPackage* Package, UObject* InObject, UBOOL bVerbose, TArray<FString>& Tokens, TArray<FString>& Switches)
	{
		UParticleModule* Module = Cast<UParticleModule>(InObject);
		if ((Module == NULL) || (Module->IsIn(Package) == FALSE))
		{
			return;
		}

		FAuditHelperTagInfo* TagInfo_ZombieComponents = FindTagInfo(TEXT("Audit.ParticleModule_ZombieComponents"));
		check(TagInfo_ZombieComponents);

		TArray<FParticleCurvePair> ModuleCurves;
		Module->GetCurveObjects(ModuleCurves);
		UBOOL bHasZombies = FALSE;
		for (INT CurveIdx = 0; CurveIdx < ModuleCurves.Num(); CurveIdx++)
		{
			FParticleCurvePair& Pair = ModuleCurves(CurveIdx);
			if (Pair.CurveObject != NULL)
			{
				if (Pair.CurveObject->HasAnyFlags(RF_ZombieComponent) == TRUE)
				{
					warnf( TEXT("Module has zombie component(s): %s"), *Module->GetFullName() );
					bHasZombies = TRUE;
				}
			}
		}

		if (bHasZombies == TRUE)
		{
			UParticleSystem* OwnerParticleSystem = Cast<UParticleSystem>(Module->GetOuter());
			if (OwnerParticleSystem != NULL)
			{
				TagInfo_ZombieComponents->AssetsToTag.Set(OwnerParticleSystem->GetFullName(), TRUE);
			}
		}
	}
};

/** 
 * Useful for testing just the DoActionToAllPackages without any side effects.
 **/
struct TestFunctor
{
	void CleanUpGADTags()
	{
	}

	template< typename OBJECTYPE >
	void DoIt( UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches )
	{
	}
};

/** 
 *	This will look at all SoundCues and check for the set of "questionable" content cases that have been defined.
 */
class FSoundCueAuditHelper : public FAuditHelper
{
public:
	FSoundCueAuditHelper()
	{
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.SoundCue_MoreThan3Waves"), TEXT("Audit.SoundCue_MoreThan3Waves_Whitelist"));
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.SoundWave_QualityHigherThan40"), TEXT("Audit.SoundWave_QualityHigherThan40_Whitelist"));
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.SoundCue_HasMixerWithMoreThan2Children"), TEXT("Audit.SoundCue_HasMixerWithMoreThan2Children_Whitelist") );
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.SoundCue_NoSoundClass"), TEXT("Audit.SoundCue_NoSoundClass_Whitelist") );
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.SoundCue_IncorrectAmbientSoundClass"), TEXT("Audit.SoundCue_IncorrectAmbientSoundClass_Whitelist") );
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.SoundCue_DiconnectedSoundNodes"), TEXT("Audit.SoundCue_DiconnectedSoundNodes_Whitelist"));
	}

	virtual void ProcessObject(UCommandlet* Commandlet, UPackage* Package, UObject* InObject, UBOOL bVerbose, TArray<FString>& Tokens, TArray<FString>& Switches)
	{
		USoundCue* SoundCue = Cast<USoundCue>(InObject);
		if (SoundCue->IsIn(Package) == TRUE)
		{
			const FName& PackageName = Package->GetFName(); 
			const FString& CueName = SoundCue->GetFullName();

			FAuditHelperTagInfo* TagInfo_MoreThan3Waves = FindTagInfo(TEXT("Audit.SoundCue_MoreThan3Waves"));
			FAuditHelperTagInfo* TagInfo_QualityHigherThan40 = FindTagInfo(TEXT("Audit.SoundWave_QualityHigherThan40"));
			FAuditHelperTagInfo* TagInfo_HasMixerWithMoreThan2Children = FindTagInfo(TEXT("Audit.SoundCue_HasMixerWithMoreThan2Children"));
			FAuditHelperTagInfo* TagInfo_NoSoundClass = FindTagInfo(TEXT("Audit.SoundCue_NoSoundClass"));
			FAuditHelperTagInfo* TagInfo_IncorrectAmbientSoundClass = FindTagInfo(TEXT("Audit.SoundCue_IncorrectAmbientSoundClass"));
			FAuditHelperTagInfo* TagInfo_DiconnectedSoundNodes = FindTagInfo(TEXT("Audit.SoundCue_DiconnectedSoundNodes"));
			check(	TagInfo_MoreThan3Waves && 
					TagInfo_QualityHigherThan40 && 
					TagInfo_HasMixerWithMoreThan2Children && 
					TagInfo_NoSoundClass && 
					TagInfo_IncorrectAmbientSoundClass && 
					TagInfo_DiconnectedSoundNodes
					);

			// so now that we have a SoundCue we need to look inside it!
			// check to see that the number of SoundNodes that are in the SoundCue are actually the same number that
			// are actually referenced
			TArray<class USoundNode*> OutNodes;
			SoundCue->RecursiveFindAllNodes(SoundCue->FirstNode, OutNodes);
			OutNodes.Shrink();

			INT NumActualNodes = 0;
			for (TMap<USoundNode*,FSoundNodeEditorData>::TIterator It(SoundCue->EditorData); It; ++It)
			{
				if (It.Key() != NULL)
				{
					NumActualNodes++;
				}
			}

			if (NumActualNodes != OutNodes.Num())
			{
				TagInfo_DiconnectedSoundNodes->AssetsToTag.Set(CueName, TRUE);
				if (bVerbose == TRUE)
				{
					warnf(TEXT("%s has %d SoundNodeWaves referenced but only %d used"), *CueName, NumActualNodes, OutNodes.Num());
				}
			}

			//// Check to see how many SoundNodeWaves this guy has
			TArray<USoundNodeWave*> Waves;
			SoundCue->RecursiveFindNode<USoundNodeWave>(SoundCue->FirstNode, Waves);
			if (Waves.Num() > 3)
			{
				TagInfo_MoreThan3Waves->AssetsToTag.Set(CueName, TRUE);
				if (bVerbose == TRUE)
				{
					warnf(TEXT("%s has %d SoundNodeWaves referenced"), *CueName, Waves.Num());
				}
			}

			//// Check for Compression Quality for soundnodewaves (default is 40; flag if higher than 40)
			for (INT i = 0; i < Waves.Num(); ++i)
			{
				USoundNodeWave* Wave = Waves(i);
				if (Wave->CompressionQuality > 40)
				{
					TagInfo_QualityHigherThan40->AssetsToTag.Set(Wave->GetFullName(), TRUE);
					if (bVerbose == TRUE)
					{
						warnf(TEXT("%s has Compression Quality greater than 40.  Curr: %d"), *Wave->GetFullName(), Wave->CompressionQuality);
					}
				}
			}

			//// Check to see if any mixers have more than 2 outputs
			TArray<USoundNodeMixer*> Mixers;
			SoundCue->RecursiveFindNode<USoundNodeMixer>(SoundCue->FirstNode, Mixers);
			for (INT i = 0; i < Mixers.Num(); ++i)
			{
				USoundNodeMixer* Mixer = Mixers(i);
				if (Mixer->ChildNodes.Num() > 2)
				{
					TagInfo_HasMixerWithMoreThan2Children->AssetsToTag.Set(CueName, TRUE);
					if (bVerbose == TRUE)
					{
						warnf(TEXT("%s has Mixer with more than 2 children.  Curr: %d"), *CueName, Mixer->ChildNodes.Num());
					}
				}
			}

			//// Check to see if a soundclass is set
			if (SoundCue->SoundClass == FName(TEXT("None")))
			{
				TagInfo_NoSoundClass->AssetsToTag.Set(CueName, TRUE);
				if (bVerbose == TRUE)
				{
					warnf(TEXT("%s has no SoundClass"), *CueName);
				}
			}
			else if (SoundCue->SoundClass == FName(TEXT("Ambient")))
			{
				// if not in a correct Ambient package
				if ((PackageName.ToString().InStr(TEXT("Ambient_Loop")) == INDEX_NONE) && (PackageName.ToString().InStr(TEXT("Ambient_NonLoop")) == INDEX_NONE))
				{
					TagInfo_IncorrectAmbientSoundClass->AssetsToTag.Set(CueName, TRUE);
					if (bVerbose == TRUE)
					{
						warnf( TEXT( "%s is classified as Ambient SoundClass but is not in the correct Package:  %s" ), *CueName, *PackageName.ToString()  );
					}
				}
			}
		}
	}
};

/**
 * This will find static meshes with lightmap uv issues
 */
class FCheckLightmapUVsAuditHelper : public FAuditHelper
{
public:
	FCheckLightmapUVsAuditHelper()
	{
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.StaticMesh_WithMissingUVSets"), TEXT("Audit.StaticMesh_WithMissingUVSets_Whitelist"));
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.StaticMesh_WithBadUVSets"), TEXT("Audit.StaticMesh_WithBadUVSets_Whitelist"));
	}

	virtual void ProcessObject(UCommandlet* Commandlet, UPackage* Package, UObject* InObject, UBOOL bVerbose, TArray<FString>& Tokens, TArray<FString>& Switches)
	{
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(InObject);
		if ((StaticMesh == NULL) || (StaticMesh->IsIn(Package) == FALSE))
		{
			return;
		}

		TArray<FString> InOutAssetsWithMissingUVSets;
		TArray<FString> InOutAssetsWithBadUVSets;
		TArray<FString> InOutAssetsWithValidUVSets;

		UStaticMesh::CheckLightMapUVs( StaticMesh, InOutAssetsWithMissingUVSets, InOutAssetsWithBadUVSets, InOutAssetsWithValidUVSets, FALSE );

		FAuditHelperTagInfo* TagInfo_WithMissingUVSets = FindTagInfo(TEXT("Audit.StaticMesh_WithMissingUVSets"));
		FAuditHelperTagInfo* TagInfo_WithBadUVSets = FindTagInfo(TEXT("Audit.StaticMesh_WithBadUVSets"));
		check(TagInfo_WithMissingUVSets && TagInfo_WithBadUVSets);

		for( INT i = 0; i < InOutAssetsWithMissingUVSets.Num(); ++i )
		{
			TagInfo_WithMissingUVSets->AssetsToTag.Set(InOutAssetsWithMissingUVSets(i), TRUE);
			if (bVerbose == TRUE)
			{
				warnf( TEXT("StaticMesh_WithMissingUVSets: %s"), *InOutAssetsWithMissingUVSets(i) );
			}
		}

		for( INT i = 0; i < InOutAssetsWithBadUVSets.Num(); ++i )
		{
			TagInfo_WithBadUVSets->AssetsToTag.Set(InOutAssetsWithBadUVSets(i), TRUE);
			if (bVerbose == TRUE)
			{
				warnf( TEXT("StaticMeshWithBadUVSets: %s"), *InOutAssetsWithBadUVSets(i) );
			}
		}
	}
};

/** 
 *
 */
class FStaticMeshAuditHelper : public FAuditHelper
{
public:
	FStaticMeshAuditHelper()
	{
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.StaticMesh_EmptySections"), TEXT("Audit.StaticMesh_EmptySections_Whitelist"));
		new(Tags)FAuditHelperTagInfo(TEXT("Audit.StaticMesh_CanBecomeDynamic"), TEXT("Audit.StaticMesh_CanBecomeDynamic_Whitelist"));
	}

	virtual void ProcessObject(UCommandlet* Commandlet, UPackage* Package, UObject* InObject, UBOOL bVerbose, TArray<FString>& Tokens, TArray<FString>& Switches)
	{
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(InObject);
		if ((StaticMesh == NULL) || (StaticMesh->IsIn(Package) == FALSE))
		{
			return;
		}

		FAuditHelperTagInfo* TagInfo_EmptySections = FindTagInfo(TEXT("Audit.StaticMesh_EmptySections"));
		FAuditHelperTagInfo* TagInfo_CanBecomeDynamic = FindTagInfo(TEXT("Audit.StaticMesh_CanBecomeDynamic"));
		check(TagInfo_EmptySections && TagInfo_CanBecomeDynamic);

		UBOOL bAdded = FALSE;
		for (INT LODIdx = 0; LODIdx < StaticMesh->LODModels.Num() && !bAdded; LODIdx++)
		{
			const FStaticMeshRenderData& LODModel = StaticMesh->LODModels(LODIdx);
			for (INT ElementIdx = 0; ElementIdx < LODModel.Elements.Num() && !bAdded; ElementIdx++)
			{
				const FStaticMeshElement& Element = LODModel.Elements(ElementIdx);
				if ((Element.NumTriangles == 0) || (Element.Material == NULL))
				{
					if (bVerbose == TRUE)
					{
						warnf( TEXT("StaticMesh has Empty Section: %s"), *StaticMesh->GetFullName() );
					}
					TagInfo_EmptySections->AssetsToTag.Set(StaticMesh->GetFullName(), TRUE);
					bAdded = TRUE;
				}
			}
		}

		if (StaticMesh->bCanBecomeDynamic == TRUE)
		{
			TagInfo_CanBecomeDynamic->AssetsToTag.Set(StaticMesh->GetFullName(), TRUE);
		}
	}
};

/*-----------------------------------------------------------------------------
SoundCueAudit Commandlet
-----------------------------------------------------------------------------*/
INT USoundCueAuditCommandlet::Main( const FString& Params )
{
	TMap<UClass*,TArray<FAuditHelper*> > ObjToHelperMap;
	FSoundCueAuditHelper SoundCueHelper;
	ContentAuditCommandlet_AddHelperToMap(USoundCue::StaticClass(), &SoundCueHelper, ObjToHelperMap);
	DoActionsToAllObjectsInPackages(this, Params, ObjToHelperMap);

	return 0;
}
IMPLEMENT_CLASS(USoundCueAuditCommandlet)



/*-----------------------------------------------------------------------------
FindMissingPhysicalMaterials commandlet.
-----------------------------------------------------------------------------*/

INT UFindTexturesWithMissingPhysicalMaterialsCommandlet::Main( const FString& Params )
{
	TMap<UClass*,TArray<FAuditHelper*> > ObjToHelperMap;
	FMaterialMissingPhysMaterialAuditHelper MaterialMissingPhysMaterialHelper;
	FSkeletalMeshesMissingPhysMaterialAuditHelper SkeletalMeshesMissingPhysMaterialHelper;

	ContentAuditCommandlet_AddHelperToMap(UMaterial::StaticClass(), &MaterialMissingPhysMaterialHelper, ObjToHelperMap);
	ContentAuditCommandlet_AddHelperToMap(UPhysicsAsset::StaticClass(), &SkeletalMeshesMissingPhysMaterialHelper, ObjToHelperMap);

	DoActionsToAllObjectsInPackages(this, Params, ObjToHelperMap);

	return 0;
}
IMPLEMENT_CLASS(UFindTexturesWithMissingPhysicalMaterialsCommandlet)


/*-----------------------------------------------------------------------------
FindQuestionableTextures Commandlet
-----------------------------------------------------------------------------*/

INT UFindQuestionableTexturesCommandlet::Main( const FString& Params )
{
	TMap<UClass*,TArray<FAuditHelper*> > ObjToHelperMap;
	FTextureAuditHelper TextureHelper;
	ContentAuditCommandlet_AddHelperToMap(UTexture2D::StaticClass(), &TextureHelper, ObjToHelperMap);
	DoActionsToAllObjectsInPackages(this, Params, ObjToHelperMap);

	return 0;
}
IMPLEMENT_CLASS(UFindQuestionableTexturesCommandlet)




/*-----------------------------------------------------------------------------
OutputAuditSummary Commandlet
-----------------------------------------------------------------------------*/
INT UOutputAuditSummaryCommandlet::Main( const FString& Params )
{
	OutputAllAuditSummary();
	return 0;
}

IMPLEMENT_CLASS(UOutputAuditSummaryCommandlet)



/*-----------------------------------------------------------------------------
ContentAudit Commandlet
-----------------------------------------------------------------------------*/
INT UContentAuditCommandlet::Main( const FString& Params )
{
	const FString NewParams = Params + TEXT(" -SKIPSCRIPTPACKAGES");

	TMap<UClass*,TArray<FAuditHelper*> > ObjToHelperMap;
	FSoundCueAuditHelper SoundCueHelper;
	FTextureAuditHelper TextureHelper;
	FMaterialMissingPhysMaterialAuditHelper MaterialMissingPhysMaterialHelper;
	FMaterialWithUniqueSpecularTextureAuditHelper MaterialWithUniqueSpecularTextureHelper;
	FSkeletalMeshesMissingPhysMaterialAuditHelper SkeletalMeshesMissingPhysMaterialHelper;
	FParticleSystemsWithBadFixedRelativeBoundingBoxAuditHelper ParticleSystemsWithBadFixedRelativeBoundingBoxHelper;
	FParticleModulesWithZombieComponentsAuditHelper ParticleModulesWithZombieComponentsHelper;
	FCheckLightmapUVsAuditHelper CheckLightmapUVsHelper;
	FStaticMeshAuditHelper StaticMeshHelper;

	ContentAuditCommandlet_AddHelperToMap(USoundCue::StaticClass(), &SoundCueHelper, ObjToHelperMap);
	ContentAuditCommandlet_AddHelperToMap(UTexture2D::StaticClass(), &TextureHelper, ObjToHelperMap);
	ContentAuditCommandlet_AddHelperToMap(UMaterial::StaticClass(), &MaterialMissingPhysMaterialHelper, ObjToHelperMap);
	ContentAuditCommandlet_AddHelperToMap(UMaterialInterface::StaticClass(), &MaterialWithUniqueSpecularTextureHelper, ObjToHelperMap);
	ContentAuditCommandlet_AddHelperToMap(UPhysicsAsset::StaticClass(), &SkeletalMeshesMissingPhysMaterialHelper, ObjToHelperMap);
	ContentAuditCommandlet_AddHelperToMap(UParticleSystem::StaticClass(), &ParticleSystemsWithBadFixedRelativeBoundingBoxHelper, ObjToHelperMap);
	ContentAuditCommandlet_AddHelperToMap(UParticleModule::StaticClass(), &ParticleModulesWithZombieComponentsHelper, ObjToHelperMap);
	ContentAuditCommandlet_AddHelperToMap(UStaticMesh::StaticClass(), &CheckLightmapUVsHelper, ObjToHelperMap);
	ContentAuditCommandlet_AddHelperToMap(UStaticMesh::StaticClass(), &StaticMeshHelper, ObjToHelperMap);

	DoActionsToAllObjectsInPackages(this, NewParams, ObjToHelperMap);

	// so at the end here we need to ask the GAD for all of the Audit categories and print them out
	OutputAllAuditSummary();

	return 0;
}

IMPLEMENT_CLASS(UContentAuditCommandlet)



/**
 * We use the CreateCustomEngine call to set some flags which will allow SerializeTaggedProperties to have the correct settings
 * such that editoronly and notforconsole data can correctly NOT be loaded from startup packages (e.g. engine.u)
 *
 **/
void UContentComparisonCommandlet::CreateCustomEngine()
{
	// get the platform from the Params (MUST happen first thing)
// 	if ( !SetPlatform( appCmdLine() ) )
// 	{
// 		SET_WARN_COLOR(COLOR_RED);
// 		warnf(NAME_Error, TEXT("Platform not specified. You must use:\n   platform=[xenon|pc|pcserver|pcconsole|ps3|linux|macosx|iphone|android]"));
// 		CLEAR_WARN_COLOR();
// 	}
    
	GIsCooking				= TRUE;
	GCookingTarget 			= UE3::PLATFORM_Stripped;

	// force cooking to use highest compat level
	//appSetCompatibilityLevel(FCompatibilityLevelInfo(5,5,5), FALSE);
}





/*-----------------------------------------------------------------------------
ContentAudit Commandlet
-----------------------------------------------------------------------------*/
INT UContentComparisonCommandlet::Main( const FString& Params )
{
	// TODO:  we need to make a baseline for everything really.  Otherwise all of the 
	//        always loaded meshes and such "pollute" the costs  (e.g. we have a ton of gore staticmeshes always loaded
	//        that get dumped into every pawn's costs.
	//  need to turn off guds loadig also prob

	TArray<FString> Tokens;
	TArray<FString> Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	bGenerateAssetGrid = (Switches.FindItemIndex(TEXT("ASSETGRID")) != INDEX_NONE);
	bGenerateFullAssetGrid = (Switches.FindItemIndex(TEXT("FULLASSETGRID")) != INDEX_NONE);
	bSkipTagging = (Switches.FindItemIndex(TEXT("CREATETAGS")) == INDEX_NONE);
	bSkipAssetCSV = (Switches.FindItemIndex(TEXT("SKIPASSETCSV")) != INDEX_NONE);

	if (bSkipTagging == FALSE)
	{
		GADHelper = new FGADHelper();
		check(GADHelper);
		GADHelper->Initialize();
	}

	// so here we need to look at the ClassesToCompare
	// 0) load correct package
	// 1) search for the class
	// 2) analyze the class and save off the data we care about


	CurrentTime = appSystemTimeString();

	// load all of the non native script packages that exist
	const UEditorEngine* EditorEngine = CastChecked<UEditorEngine>(GEngine);
	for( INT i=0; i < EditorEngine->EditPackages.Num(); i++ )
	{
		UObject::BeginLoad();
		warnf( TEXT("Loading EditPackage: %s"), *EditorEngine->EditPackages(i) );
		UPackage* Package = LoadPackage( NULL, *EditorEngine->EditPackages(i), LOAD_NoWarn );
		//ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *EditorEngine->EditPackages(i), LOAD_NoVerify, NULL, NULL );
		UObject::EndLoad();
	}

	for (TObjectIterator<UObject> It; It; ++It)
	{
		It->SetFlags(RF_Marked);
	}


	FConfigSection* CompareClasses = GConfig->GetSectionPrivate( TEXT("ContentComparisonClassesToCompare"), FALSE, TRUE, GEditorIni );
	if( CompareClasses != NULL )
	{
		for( FConfigSectionMap::TIterator It(*CompareClasses); It; ++It )
		{
			const FString& ClassToCompare = It.Value();

			ClassesToCompare.AddUniqueItem( ClassToCompare );
		}
	}

	// Make a list of classes to compare that have derived classes in the list as well.
	// In these cases, we want to skip objects that are of the derived type when filling
	// in the information on the base.
	TArray<UClass*> Classes;
	for (INT ClassIdx = 0; ClassIdx < ClassesToCompare.Num(); ClassIdx++)
	{
		const FString& ClassToCompare = ClassesToCompare(ClassIdx);
		UClass* TheClass = (UClass*)UObject::StaticLoadObject( UClass::StaticClass(), ANY_PACKAGE, *ClassToCompare, NULL, LOAD_NoWarn|LOAD_Quiet, NULL );
		if (TheClass != NULL)
		{
			Classes.AddUniqueItem(TheClass);
		}
	}
	for (INT OuterClassIdx = 0; OuterClassIdx < Classes.Num(); OuterClassIdx++)
	{
		UClass* OuterClass = Classes(OuterClassIdx);
		for (INT InnerClassIdx = 0; InnerClassIdx < Classes.Num(); InnerClassIdx++)
		{
			if (InnerClassIdx != OuterClassIdx)
			{
				UClass* InnerClass = Classes(InnerClassIdx);
				if (InnerClass->IsChildOf(OuterClass) == TRUE)
				{
					TArray<FString>* DerivedList = ClassToDerivedClassesBeingCompared.Find(OuterClass->GetName());
					if (DerivedList == NULL)
					{
						TArray<FString> Temp;
						ClassToDerivedClassesBeingCompared.Set(OuterClass->GetName(), Temp);
						DerivedList = ClassToDerivedClassesBeingCompared.Find(OuterClass->GetName());
					}
					check(DerivedList);
					DerivedList->AddUniqueItem(InnerClass->GetName());
				}
			}
		}
	}
	Classes.Empty();
	UObject::CollectGarbage(RF_Marked);

	for( INT i = 0; i < ClassesToCompare.Num(); ++i )
	{
		const FString& ClassToCompare = ClassesToCompare(i);

		UClass* TheClass = (UClass*)UObject::StaticLoadObject( UClass::StaticClass(), ANY_PACKAGE, *ClassToCompare, NULL, LOAD_NoWarn|LOAD_Quiet, NULL );

		warnf( TEXT("Loaded Class: %s"), *TheClass->GetFullName() );

		if (TheClass != NULL)
		{
			for (TObjectIterator<UFaceFXAnimSet> FFXIt; FFXIt; ++FFXIt)
			{
				UFaceFXAnimSet* FaceFXAnimSet = *FFXIt;
				FaceFXAnimSet->ReferencedSoundCues.Empty();
			}

			TArray<UClass*> DerivedClasses;
			TArray<FString>* DerivedClassNames = ClassToDerivedClassesBeingCompared.Find(TheClass->GetName());
			if (DerivedClassNames)
			{
				for (INT DerivedIdx = 0; DerivedIdx < DerivedClassNames->Num(); DerivedIdx++)
				{
					const FString& DerivedClassName = (*DerivedClassNames)(DerivedIdx);
					UClass* DerivedClass = (UClass*)UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *DerivedClassName, TRUE);
					DerivedClasses.AddItem(DerivedClass);
				}
			}

			InitClassesToGatherDataOn( TheClass->GetFName().ToString() );

			for( TObjectIterator<UClass> It; It; ++It )
			{
				const UClass* const TheAssetClass = *It;

				if( ( TheAssetClass->IsChildOf( TheClass ) == TRUE )
					&& ( TheAssetClass->HasAnyClassFlags(CLASS_Abstract) == FALSE )
					)
				{
					UBOOL bSkipIt = FALSE;
					for (INT CheckIdx = 0; CheckIdx < DerivedClasses.Num(); CheckIdx++)
					{
						UClass* CheckClass = DerivedClasses(CheckIdx);
						if (TheAssetClass->IsChildOf(CheckClass) == TRUE)
						{
							warnf(TEXT("Skipping class derived from other content comparison class..."));
							bSkipIt = TRUE;
						}
					}
					if (bSkipIt == FALSE)
					{
						warnf( TEXT("TheAssetClass: %s"), *TheAssetClass->GetFullName() );

						GatherClassBasedData( TheAssetClass->GetLinker(), TheAssetClass );

						ReportComparisonData( TheAssetClass->GetFullName() );

						UObject::CollectGarbage(RF_Marked);
					}
				}
			}

			// close the current sheet
			if( Table != NULL )
			{
				Table->Close();
				delete Table;
				Table = NULL;
			}

			ReportClassAssetDependencies(TheClass->GetFName().ToString());
		}
	}


	if (bSkipTagging == FALSE)
	{
		GADHelper->Shutdown();
		delete GADHelper;
	}

	return 0;


// old way

// 	for( INT i = 0; i < ClassesToCompare.Num(); ++i )
// 	{
// 		const FString& ClassToCompare = ClassesToCompare(i);
// 
// 		const INT DotIndex = ClassToCompare.InStr( TEXT(".") );
// 
// 		FString PackageName;
// 		FString ClassName;
// 
// 		ClassToCompare.Split( TEXT("."), &PackageName, &ClassName, TRUE );
// 
// 		warnf( TEXT("About to compare: PackageName %s with Class: %s  DotIndex: %d"), *PackageName, *ClassName, DotIndex );
// 
// 		UObject::BeginLoad();
// 		ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *PackageName, LOAD_NoVerify, NULL, NULL );
// 		UObject::EndLoad();
// 
// 		if( Linker == NULL )
// 		{
// 			warnf( TEXT( "NULL LINKER" ) );
// 		}
// 
// 		GatherClassBasedData( Linker, ClassName );
// 
// 		ReportComparisonData( ClassToCompare );
// 
// 		UObject::CollectGarbage(RF_Native);
// 	}

	if( Table != NULL )
	{
		Table->Close();
	}

	return 0;
}



void UContentComparisonCommandlet::InitClassesToGatherDataOn( const FString& InTableName )
{
	Table = NULL;
	TypesToLookFor.Empty();


	FConfigSection* StatTypes = GConfig->GetSectionPrivate( TEXT("ContentComparisonReferenceTypes"), FALSE, TRUE, GEngineIni );
	if( StatTypes != NULL )
	{
		// make our cvs file
		Table = new FDiagnosticTableViewer( *FDiagnosticTableViewer::GetUniqueTemporaryFilePath( *  ( FString(TEXT("ContentComparison\\")) + FString::Printf(TEXT("ContentComparison-%d-%s\\"), GetChangeListNumberForPerfTesting(), *CurrentTime ) + FString::Printf( TEXT("ContentComparison-%s"), *InTableName ) ) ) );

		Table->AddColumn( TEXT("AssetName") );

		for( FConfigSectionMap::TIterator It(*StatTypes); It; ++It )
		{
			const FString StatType = It.Value();
			UClass* TheClass = (UClass*)UObject::StaticFindObject( UClass::StaticClass(), ANY_PACKAGE, *StatType );
			//UClass* TheClass = (UClass*)UObject::StaticLoadObject( UClass::StaticClass(), NULL, *StatType, NULL, LOAD_None, NULL );

			//warnf( TEXT("StatType: '%s'  TheClass: %s"), *StatType, *TheClass->GetFullName() );

			if( TheClass != NULL )
			{
				TypesToLookForDatum ClassToCompare( StatType, TheClass );
				TypesToLookFor.AddItem( ClassToCompare );

				Table->AddColumn( *StatType );
			}
		}

		Table->CycleRow();
	}


}

void UContentComparisonCommandlet::GatherClassBasedData( ULinkerLoad* Linker, const UClass* const InClassToGatherDataOn )
{
	ClassesTaggedInDependentAssetsList.AddUniqueItem(InClassToGatherDataOn->GetPathName());
	ClassesTaggedInDependentAssetsList_Tags.AddUniqueItem(InClassToGatherDataOn->GetName());

	INT ExportIndex = INDEX_NONE;

	for( INT i = 0; i < Linker->ExportMap.Num(); ++i )
	{
		const FObjectExport& Export = Linker->ExportMap(i);

		// find the name of this object's class
		const INT ClassIndex = Export.ClassIndex;

		const FName ClassName = ClassIndex > 0 
			? Linker->ExportMap(ClassIndex-1).ObjectName
			: IS_IMPORT_INDEX(ClassIndex)
			? Linker->ImportMap(-ClassIndex-1).ObjectName
			: FName(NAME_Class);


		//warnf( TEXT("ClassName: %s %d  %s"), *ClassName.ToString(), Export.SerialSize, *InClassToGatherDataOn->GetFName().ToString() );

		// here we get the export index and we we can use that in the code below
		const FString ClassNameToAnalyze = InClassToGatherDataOn->GetFName().ToString(); // TEXT("GearWeap_Boomshot");
		if( ClassName.ToString().Right(ClassNameToAnalyze.Len()).InStr( ClassNameToAnalyze, TRUE, TRUE ) != INDEX_NONE )
		{
			warnf( TEXT("ClassName: %s %d"), *ClassName.ToString(), Export.SerialSize );
			ExportIndex = i;
			break;
		}
	}


	if( ExportIndex == INDEX_NONE )
	{
		warnf( TEXT( "ARRRGGHHH" ) );
	}



	FString PathName = Linker->GetExportPathName( ExportIndex );

	FString ClassName = Linker->GetExportPathName( ExportIndex, NULL, FALSE );

	//UClass* TheClass = (UClass*)UObject::StaticFindObject( UClass::StaticClass(), ANY_PACKAGE, *ClassName.ToString() );
	//warnf( TEXT("PathName: '%s'  TheClass: %s"), *PathName, *ClassName );

	UClass* TheClass = (UClass*)UObject::StaticLoadObject( UClass::StaticClass(), ANY_PACKAGE, *ClassName, NULL, LOAD_NoWarn|LOAD_Quiet, NULL );

	warnf( TEXT("Loaded Class: %s"), *TheClass->GetFullName() );



	TSet<FDependencyRef> AllDepends;
	Linker->GatherExportDependencies(ExportIndex, AllDepends, FALSE);
	//warnf(TEXT("\t\t\tAll Depends:  Num Depends: %d") , AllDepends.Num());
	INT DependsIndex = 0;
	for(TSet<FDependencyRef>::TConstIterator It(AllDepends);It;++It)
	{

		const FDependencyRef& Ref = *It;

		UBOOL bIsImport = IS_IMPORT_INDEX(Ref.ExportIndex);
//		warnf(TEXT("\t\t\t%i)%s (%i)  IsImport: %d"),
//			DependsIndex++,
//			IS_IMPORT_INDEX(Ref.ExportIndex)
//			? *Ref.Linker->GetImportFullName(Ref.ExportIndex)
//			: *Ref.Linker->GetExportFullName(Ref.ExportIndex),
//			Ref.ExportIndex,
//			bIsImport
//			);

		UObject* Object	= NULL;

		// 			if( IS_IMPORT_INDEX(Ref.ExportIndex) )
		// 			{
		// 				Object = Ref.Linker->CreateImport( Ref.ExportIndex );
		// 			}
		// 			else
		// 			{
		// 				Object = Ref.Linker->CreateExport( Ref.ExportIndex );
		// 			}
		// 

		FString PathName = Ref.Linker->GetExportPathName( Ref.ExportIndex );

		FName ClassName = Ref.Linker->GetExportClassName( Ref.ExportIndex );

		UClass* TheClass = (UClass*)UObject::StaticFindObject( UClass::StaticClass(), ANY_PACKAGE, *ClassName.ToString(), TRUE );

		//UClass* TheClass = (UClass*)UObject::StaticLoadObject( UClass::StaticClass(), NULL, *ClassName.ToString(), NULL, LOAD_None, NULL );

		Object = UObject::StaticFindObject( TheClass, ANY_PACKAGE, *PathName, TRUE );

		//Object = UObject::StaticLoadObject( TheClass, NULL, *PathName, NULL, LOAD_None, NULL );



		if( Object != NULL )
		{
			// This will cause the object to be serialized. We do this here for all objects and
			// not just UClass and template objects, for which this is required in order to ensure
			// seek free loading, to be able introduce async file I/O.
			Ref.Linker->Preload( Object );
		}

		if( Object != NULL )
		{
			Object->StripData(UE3::PLATFORM_Console);

			// now see how big it is
			FArchiveCountMem ObjectToSizeUp( Object ); 

			//warnf( TEXT("\t\t\t\t GetNum: %8d  GetMax: %8d  GetResourceSize: %8d"), ObjectToSizeUp.GetNum(), ObjectToSizeUp.GetMax(), Object->GetResourceSize() );

			//warnf( TEXT("\t\t\t\t STRIPPED GetNum: %8d  GetMax: %8d  GetResourceSize: %8d"), ObjectToSizeUp.GetNum(), ObjectToSizeUp.GetMax(), Object->GetResourceSize() );

			// look in our list and see if this object is one we care about
			for( INT i = 0; i< TypesToLookFor.Num(); ++i )
			{
				TypesToLookForDatum& TheType = TypesToLookFor(i);

				UBOOL bAdded = TheType.PossiblyAddObject( Object );
				if ((bGenerateAssetGrid && bAdded) || bGenerateFullAssetGrid)
				{
					// Don't bother adding packages...
					FString ObjectFullName = Object->GetFullName();
					if ((Object->IsA(UPackage::StaticClass()) == FALSE) &&
						(ObjectFullName.InStr(TEXT(":")) == INDEX_NONE))
					{
						FDependentAssetInfo* Info = DependentAssets.Find(ObjectFullName);
						if (Info == NULL)
						{
							FDependentAssetInfo TempInfo;
							DependentAssets.Set(ObjectFullName, TempInfo);
							Info = DependentAssets.Find(ObjectFullName);

							Info->AssetName = ObjectFullName;
							Info->ResourceSize = Object->GetResourceSize();
						}
						check(Info);
						Info->ClassesThatDependOnAsset.Set(InClassToGatherDataOn->GetPathName(), TRUE);
					}
				}
			}
		}
	}
}

void UContentComparisonCommandlet::ReportComparisonData( const FString& ClassToCompare )
{
	warnf( TEXT("Stats for:  %s"), *ClassToCompare );
	if( Table != NULL )
	{
		Table->AddColumn( TEXT("%s"), *ClassToCompare );
	}


	// report all of the results
	for( INT i = 0; i< TypesToLookFor.Num(); ++i )
	{
		TypesToLookForDatum& TheType = TypesToLookFor(i);

		warnf( TEXT("%s: %f KB"), *TheType.Class->GetName(), TheType.TotalSize/1024.0f );

		if( Table != NULL )
		{
			Table->AddColumn( TEXT("%f"), TheType.TotalSize/1024.0f );
		}


		// reset
		TheType.TotalSize = 0;
	}

	if( Table != NULL )
	{
		Table->CycleRow();
	}

}

void UContentComparisonCommandlet::ReportClassAssetDependencies(const FString& InTableName)
{
	TMap<FString,TArray<FString>> ClassToAssetsTagList;
	TArray<FString> UsedByMultipleClassesAssetList;

	if (bGenerateAssetGrid || bGenerateFullAssetGrid)
	{
		FDiagnosticTableViewer* AssetTable = NULL;
		if (bSkipAssetCSV == FALSE)
		{
			AssetTable = new FDiagnosticTableViewer(
			*FDiagnosticTableViewer::GetUniqueTemporaryFilePath(
				*(
					FString(TEXT("ContentComparison\\")) + 
					FString::Printf(TEXT("ContentComparison-%d-%s\\"), GetChangeListNumberForPerfTesting(), *CurrentTime) +
					FString::Printf(TEXT("ContentComparison-%s-%s"), bGenerateFullAssetGrid ? TEXT("FullAssets") : TEXT("Assets"), *InTableName)
					)
				)
			);
		}

		if (AssetTable != NULL)
		{
			// Fill in the header row
			AssetTable->AddColumn(TEXT("Asset"));
			AssetTable->AddColumn(TEXT("ResourceSize(kB)"));
			for (INT ClassIdx = 0; ClassIdx < ClassesTaggedInDependentAssetsList.Num(); ClassIdx++)
			{
				AssetTable->AddColumn(*(ClassesTaggedInDependentAssetsList(ClassIdx)));
			}
			AssetTable->CycleRow();
		}

		// Fill it in
		for (TMap<FString,FDependentAssetInfo>::TIterator DumpIt(DependentAssets); DumpIt; ++DumpIt)
		{
			FDependentAssetInfo& Info = DumpIt.Value();

			if (AssetTable != NULL)
			{
				AssetTable->AddColumn(*(Info.AssetName));
				AssetTable->AddColumn(TEXT("%f"), Info.ResourceSize/1024.0f);
			}
			for (INT ClassIdx = 0; ClassIdx < ClassesTaggedInDependentAssetsList.Num(); ClassIdx++)
			{
				FString ClassName = ClassesTaggedInDependentAssetsList(ClassIdx);
				FString TagName = ClassesTaggedInDependentAssetsList_Tags(ClassIdx);
				if (Info.ClassesThatDependOnAsset.Find(ClassName) != NULL)
				{
					if (AssetTable != NULL)
					{
						AssetTable->AddColumn(TEXT("XXX"));
					}
					if (bSkipTagging == FALSE)
					{
						// Add to the list of assets for tagging
						TArray<FString>* ClassAssetList = ClassToAssetsTagList.Find(TagName);
						if (ClassAssetList == NULL)
						{
							TArray<FString> TempList;
							ClassToAssetsTagList.Set(TagName, TempList);
							ClassAssetList = ClassToAssetsTagList.Find(TagName);
						}
						check(ClassAssetList);
						ClassAssetList->AddUniqueItem(Info.AssetName);
					}
				}
				else
				{
					if (AssetTable != NULL)
					{
						AssetTable->AddColumn(TEXT(""));
					}
				}
			}
			if (AssetTable != NULL)
			{
				AssetTable->CycleRow();
			}

			if (bSkipTagging == FALSE)
			{
				// If it is used by more than one class, then add it to the list
				// of items to be tagged w/ the "MultipleClasses" tag
				if (Info.ClassesThatDependOnAsset.Num() > 1)
				{
					UsedByMultipleClassesAssetList.AddUniqueItem(Info.AssetName);
				}
			}
		}

		if (AssetTable != NULL)
		{
			// Close it and kill it
			AssetTable->Close();
			delete AssetTable;
		}

		if (bSkipTagging == FALSE)
		{
			// Clear all the tags... this is a drag, but needs to be done.
			FString TagPrefix = TEXT("Audit.");
			TagPrefix += InTableName;

			FString MultipleTag = TagPrefix;
			MultipleTag += TEXT("_MULTIPLECLASSES");
			debugf(TEXT("Clearing tag %s"), *MultipleTag);
			for (INT TagIdx = 0; TagIdx < ClassesTaggedInDependentAssetsList_Tags.Num(); TagIdx++)
			{
				debugf(TEXT("Clearing tag %s_%s"), *TagPrefix, *(ClassesTaggedInDependentAssetsList_Tags(TagIdx)));
			}

			// Tag the multiples
			GADHelper->SetTaggedAssets(MultipleTag, UsedByMultipleClassesAssetList);
			// Tag each class
			for (TMap<FString,TArray<FString>>::TIterator TagIt(ClassToAssetsTagList); TagIt; ++TagIt)
			{
				FString ClassName = TagIt.Key();
				FString TagName = TagPrefix;
				TagName += TEXT("_");
				TagName += ClassName;
				TArray<FString>& AssetList = TagIt.Value();

				GADHelper->SetTaggedAssets(TagName, AssetList);
			}
		}

		// Clear the lists
		ClassesTaggedInDependentAssetsList.Empty();
		DependentAssets.Empty();
	}
}

IMPLEMENT_CLASS(UContentComparisonCommandlet)
