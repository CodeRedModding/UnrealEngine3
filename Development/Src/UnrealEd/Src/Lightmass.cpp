/*=============================================================================
	Lightmass.h: lightmass import/export implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "PrecomputedLightVolume.h"
#include "EngineMeshClasses.h"
#include "LandscapeRender.h"

extern FSwarmDebugOptions GSwarmDebugOptions;

/**
 * If FALSE (default behavior), Lightmass is launched automatically when a lighting build starts.
 * If TRUE, it must be launched manually (e.g. through a debugger).
 */
UBOOL GLightmassDebugMode = FALSE; 

/** If TRUE, all participating Lightmass agents will report back detailed stats to the log. */
UBOOL GLightmassStatsMode = FALSE;

/*-----------------------------------------------------------------------------
	FLightmassDebugOptions
-----------------------------------------------------------------------------*/
void FLightmassDebugOptions::Touch()
{
	//@lmtodo. For some reason, the global instance is not initializing to the default settings...
	if (bInitialized == FALSE)
	{
		bDebugMode = FALSE;
		bStatsEnabled = FALSE;
		bGatherBSPSurfacesAcrossComponents = TRUE;
		CoplanarTolerance = 0.001f;
		bUseDeterministicLighting = TRUE;
		bUseImmediateImport = TRUE;
		bImmediateProcessMappings = TRUE;
		bSortMappings = TRUE;
		bDumpBinaryFiles = FALSE;
		bDebugMaterials = FALSE;
		bPadMappings = TRUE;
		bDebugPaddings = FALSE;
		bOnlyCalcDebugTexelMappings = FALSE;
		bUseRandomColors = FALSE;
		bColorBordersGreen = FALSE;
		bColorByExecutionTime = FALSE;
		ExecutionTimeDivisor = 15.0f;

		bInitialized = TRUE;
	}
}

/*-----------------------------------------------------------------------------
	FSwarmDebugOptions
-----------------------------------------------------------------------------*/
void FSwarmDebugOptions::Touch()
{
	//@lmtodo. For some reason, the global instance is not initializing to the default settings...
	if (bInitialized == FALSE)
	{
		bDistributionEnabled = TRUE;
		bForceContentExport = FALSE;

		bInitialized = TRUE;
	}
}

#if WITH_MANAGED_CODE

#include "LightmassPublic.h"
#include "Lightmass.h"
#include "UnStaticMeshLight.h"
#include "UnModelLight.h"
#include "TerrainLight.h"
#include "EngineFluidClasses.h"
#include "FluidSurfaceLight.h"
#include "LandscapeLight.h"
#include "LandscapeDataAccess.h"
#if WITH_SPEEDTREE
#include "SpeedTreeStaticLighting.h"
#include "SpeedTree.h"
#endif	//#if WITH_SPEEDTREE
#include "UnFracturedStaticMesh.h"

/** The number of available mappings to process before yielding back to the importing. */
INT FLightmassProcessor::MaxProcessAvailableCount = 8;

volatile INT FLightmassProcessor::VolumeSampleTaskCompleted = 0;
volatile INT FLightmassProcessor::MeshAreaLightDataTaskCompleted = 0;
volatile INT FLightmassProcessor::VolumeDistanceFieldTaskCompleted = 0;

/** Flags to use when opening the different kinds of input channels */
/** MUST PAIR APPROPRIATELY WITH THE SAME FLAGS IN LIGHTMASS */
static NSwarm::TChannelFlags LM_TEXTUREMAPPING_CHANNEL_FLAGS	= NSwarm::SWARM_JOB_CHANNEL_READ;
static NSwarm::TChannelFlags LM_VERTEXMAPPING_CHANNEL_FLAGS		= NSwarm::SWARM_JOB_CHANNEL_READ;
static NSwarm::TChannelFlags LM_VOLUMESAMPLES_CHANNEL_FLAGS		= NSwarm::SWARM_JOB_CHANNEL_READ;
static NSwarm::TChannelFlags LM_PRECOMPUTEDVISIBILITY_CHANNEL_FLAGS	= NSwarm::SWARM_JOB_CHANNEL_READ;
static NSwarm::TChannelFlags LM_VOLUMEDEBUGOUTPUT_CHANNEL_FLAGS	= NSwarm::SWARM_JOB_CHANNEL_READ;
static NSwarm::TChannelFlags LM_DOMINANTSHADOW_CHANNEL_FLAGS	= NSwarm::SWARM_JOB_CHANNEL_READ;
static NSwarm::TChannelFlags LM_MESHAREALIGHT_CHANNEL_FLAGS		= NSwarm::SWARM_JOB_CHANNEL_READ;
static NSwarm::TChannelFlags LM_DEBUGOUTPUT_CHANNEL_FLAGS		= NSwarm::SWARM_JOB_CHANNEL_READ;

/** Flags to use when opening the different kinds of output channels */
/** MUST PAIR APPROPRIATELY WITH THE SAME FLAGS IN LIGHTMASS */
#if LM_COMPRESS_INPUT_DATA
	static NSwarm::TChannelFlags LM_SCENE_CHANNEL_FLAGS			= (NSwarm::TChannelFlags)(NSwarm::SWARM_JOB_CHANNEL_WRITE | NSwarm::SWARM_CHANNEL_MISC_ENABLE_COMPRESSION);
	static NSwarm::TChannelFlags LM_STATICMESH_CHANNEL_FLAGS	= (NSwarm::TChannelFlags)(NSwarm::SWARM_CHANNEL_WRITE | NSwarm::SWARM_CHANNEL_MISC_ENABLE_COMPRESSION);
	static NSwarm::TChannelFlags LM_TERRAIN_CHANNEL_FLAGS		= (NSwarm::TChannelFlags)(NSwarm::SWARM_CHANNEL_WRITE | NSwarm::SWARM_CHANNEL_MISC_ENABLE_COMPRESSION);
	static NSwarm::TChannelFlags LM_SPEEDTREE_CHANNEL_FLAGS		= (NSwarm::TChannelFlags)(NSwarm::SWARM_CHANNEL_WRITE | NSwarm::SWARM_CHANNEL_MISC_ENABLE_COMPRESSION);
	static NSwarm::TChannelFlags LM_MATERIAL_CHANNEL_FLAGS		= (NSwarm::TChannelFlags)(NSwarm::SWARM_CHANNEL_WRITE | NSwarm::SWARM_CHANNEL_MISC_ENABLE_COMPRESSION);
#else
	static NSwarm::TChannelFlags LM_SCENE_CHANNEL_FLAGS			= NSwarm::SWARM_JOB_CHANNEL_WRITE;
	static NSwarm::TChannelFlags LM_STATICMESH_CHANNEL_FLAGS	= NSwarm::SWARM_CHANNEL_WRITE;
	static NSwarm::TChannelFlags LM_TERRAIN_CHANNEL_FLAGS		= NSwarm::SWARM_CHANNEL_WRITE;
	static NSwarm::TChannelFlags LM_SPEEDTREE_CHANNEL_FLAGS		= NSwarm::SWARM_CHANNEL_WRITE;
	static NSwarm::TChannelFlags LM_MATERIAL_CHANNEL_FLAGS		= NSwarm::SWARM_CHANNEL_WRITE;
#endif

/*-----------------------------------------------------------------------------
	FLightmassExporter
-----------------------------------------------------------------------------*/

FORCEINLINE void Copy( const FVector2D& In, Lightmass::FVector2D& Out )
{
	Out.X = In.X;
	Out.Y = In.Y;
}
FORCEINLINE void Copy( const FVector2D& In, Lightmass::FVector4& Out )
{
	Out.X = In.X;
	Out.Y = In.Y;
	Out.Z = 0.0f;
	Out.W = 0.0f;
}
FORCEINLINE void Copy( const FVector& In, Lightmass::FVector4& Out )
{
	Out.X = In.X;
	Out.Y = In.Y;
	Out.Z = In.Z;
	Out.W = 1.0f;
}
FORCEINLINE void Copy( const Lightmass::FVector4& In, FVector& Out )
{
	Out.X = In.X;
	Out.Y = In.Y;
	Out.Z = In.Z;
}
FORCEINLINE void Copy( const FVector4& In, Lightmass::FVector4& Out )
{
	Out.X = In.X;
	Out.Y = In.Y;
	Out.Z = In.Z;
	Out.W = In.W;
}
FORCEINLINE void Copy( const Lightmass::FVector4& In,  FVector4& Out )
{
	Out.X = In.X;
	Out.Y = In.Y;
	Out.Z = In.Z;
	Out.W = In.W;
}
FORCEINLINE void Copy( const FColor& In, Lightmass::FColor& Out )
{
	Out.R = In.R;
	Out.B = In.B;
	Out.G = In.G;
	Out.A = In.A;
}
FORCEINLINE void Copy( const Lightmass::FColor& In, FColor& Out )
{
	Out.R = In.R;
	Out.B = In.B;
	Out.G = In.G;
	Out.A = In.A;
}
FORCEINLINE void Copy( const FLOAT& In, Lightmass::FLOAT& Out )
{
	Out = In;
}
FORCEINLINE void Copy( const FMatrix& In, Lightmass::FMatrix& Out )
{
	for ( INT Row=0; Row < 4; ++Row )
	{
		for ( INT Column=0; Column < 4; ++Column )
		{
			Copy( In.M[Row][Column], Out.M[Row][Column] );
		}
	}
}
FORCEINLINE void Copy( const Lightmass::FMatrix& In, FMatrix& Out )
{
	for ( INT Row=0; Row < 4; ++Row )
	{
		for ( INT Column=0; Column < 4; ++Column )
		{
			Copy( In.M[Row][Column], Out.M[Row][Column] );
		}
	}
}
FORCEINLINE void Copy( const Lightmass::FQuantizedSHVector& In, FQuantizedSHVector& Out )
{
	Out.MinCoefficient.Encoded = In.MinCoefficient.Encoded;
	Out.MaxCoefficient.Encoded = In.MaxCoefficient.Encoded;
	for (INT BandIndex = 0; BandIndex < MAX_SH_BASIS; BandIndex++)
	{
		Out.V[BandIndex] = In.V[BandIndex];
	}
}
FORCEINLINE void Copy( const Lightmass::FQuantizedSHVectorRGB& In, FQuantizedSHVectorRGB& Out )
{
	Copy(In.R, Out.R);
	Copy(In.G, Out.G);
	Copy(In.B, Out.B);
}
FORCEINLINE void Copy( const FGuid& In, Lightmass::FGuid& Out )
{
	Out.A = In.A;
	Out.B = In.B;
	Out.C = In.C;
	Out.D = In.D;
}
FORCEINLINE void Copy( const Lightmass::FGuid& In, FGuid& Out )
{
	Out.A = In.A;
	Out.B = In.B;
	Out.C = In.C;
	Out.D = In.D;
}
FORCEINLINE void Copy( const FGuid& In, NSwarm::FGuid& Out )
{
	Out.A = In.A;
	Out.B = In.B;
	Out.C = In.C;
	Out.D = In.D;
}
FORCEINLINE void Copy( const Lightmass::FGuid& In, NSwarm::FGuid& Out )
{
	Out.A = In.A;
	Out.B = In.B;
	Out.C = In.C;
	Out.D = In.D;
}
FORCEINLINE void Copy( const NSwarm::FGuid& In, FGuid& Out )
{
	Out.A = In.A;
	Out.B = In.B;
	Out.C = In.C;
	Out.D = In.D;
}
FORCEINLINE void Copy( const ULightComponent* In, Lightmass::FLightData& Out )
{	
	Out.LightFlags = 0;
	if (In->CastShadows)
	{
		Out.LightFlags |= Lightmass::GI_LIGHT_CASTSHADOWS;
	}

	if (In->HasStaticLighting())
	{
		Out.LightFlags |= Lightmass::GI_LIGHT_HASSTATICLIGHTING;
	}

	if (In->UseDirectLightMap)
	{
		Out.LightFlags |= Lightmass::GI_LIGHT_USEDIRECTLIGHTMAP;
	}

	if (In->HasStaticShadowing())
	{
		Out.LightFlags |= Lightmass::GI_LIGHT_HASSTATICSHADOWING;
	}

	if (In->CastStaticShadows)
	{
		Out.LightFlags |= Lightmass::GI_LIGHT_CASTSTATICSHADOWS;
	}

	// Currently always using signed distance field shadows for dominant lights
	if (In->IsA(UDominantDirectionalLightComponent::StaticClass())
		|| In->IsA(UDominantPointLightComponent::StaticClass()) 
		|| In->IsA(UDominantSpotLightComponent::StaticClass()))
	{
		Out.LightFlags |= Lightmass::GI_LIGHT_USESIGNEDDISTANCEFIELDSHADOWS;
		Out.LightFlags |= Lightmass::GI_LIGHT_DOMINANT;
	}

	Copy( In->GetPosition(), Out.Position );
	Copy( In->GetDirection(), Out.Direction );
	Copy( In->LightColor, Out.Color );
	Copy( In->Brightness, Out.Brightness );
	Copy( In->LightmapGuid, Out.Guid );
}
FORCEINLINE void Copy( const FPackedNormal& In, Lightmass::FPackedNormal& Out )
{
	Out.Vector.X = In.Vector.X;
	Out.Vector.Y = In.Vector.Y;
	Out.Vector.Z = In.Vector.Z;
	Out.Vector.W = In.Vector.W;
}

FORCEINLINE void Copy( const FPackedNormal& In, Lightmass::FVector4& Out )
{
	Copy(FVector(In), Out);
}

FORCEINLINE void Copy( const FBox& In, Lightmass::FBox& Out )
{
	Copy(In.Max, Out.Max);
	Copy(In.Min, Out.Min);
}

FORCEINLINE void Copy( const Lightmass::FBox& In, FBox& Out )
{
	Out.IsValid = 1;
	Copy(In.Max, Out.Max);
	Copy(In.Min, Out.Min);
}

FORCEINLINE void Copy( const Lightmass::FDominantLightShadowInfoData& In, FDominantShadowInfo& Out )
{
	Copy(In.WorldToLight, Out.WorldToLight);
	Out.LightToWorld = Out.WorldToLight.Inverse();

	FVector4 Min;
	Copy(In.LightSpaceImportanceBoundMin, Min);
	FVector4 Max;
	Copy(In.LightSpaceImportanceBoundMax, Max);
	Out.LightSpaceImportanceBounds = FBox(Min, Max);

	Out.ShadowMapSizeX = In.ShadowMapSizeX;
	Out.ShadowMapSizeY = In.ShadowMapSizeY;
}

FORCEINLINE void Copy( const FSplineMeshParams& In, Lightmass::FSplineMeshParams& Out )
{
	Copy(In.StartPos, Out.StartPos);
	Copy(In.StartTangent, Out.StartTangent);
	Copy(In.StartScale, Out.StartScale);
	Out.StartRoll = In.StartRoll;
	Copy(In.StartOffset, Out.StartOffset);
	Copy(In.EndPos, Out.EndPos);
	Copy(In.EndTangent, Out.EndTangent);
	Copy(In.EndScale, Out.EndScale);
	Copy(In.EndOffset, Out.EndOffset);
	Out.EndRoll = In.EndRoll;
}

/*-----------------------------------------------------------------------------
	SwarmCallback function
-----------------------------------------------------------------------------*/
void FLightmassProcessor::SwarmCallback( NSwarm::FMessage* CallbackMessage, void* CallbackData )
{
	FLightmassProcessor* Processor = (FLightmassProcessor*) CallbackData;
	DOUBLE SwarmCallbackStartTime = appSeconds();

	switch ( CallbackMessage->Type )
	{
		case NSwarm::MESSAGE_JOB_STATE:
		{
			NSwarm::FJobState* JobStateMessage = (NSwarm::FJobState*)CallbackMessage;
			switch (JobStateMessage->JobState)
			{
				case NSwarm::JOB_STATE_INVALID:
					Processor->bProcessingFailed = TRUE;
					break;
				case NSwarm::JOB_STATE_RUNNING:
					break;
				case NSwarm::JOB_STATE_COMPLETE_SUCCESS:
					Processor->bProcessingSuccessful = TRUE;
					break;
				case NSwarm::JOB_STATE_COMPLETE_FAILURE:
					Processor->bProcessingFailed = TRUE;
					break;
				case NSwarm::JOB_STATE_KILLED:
					Processor->bProcessingFailed = TRUE;
					break;
				default:
					break;
			}
		}
		break;

		case NSwarm::MESSAGE_TASK_STATE:
		{
			NSwarm::FTaskState* TaskStateMessage = (NSwarm::FTaskState*)CallbackMessage;
			switch (TaskStateMessage->TaskState)
			{
				case NSwarm::JOB_TASK_STATE_INVALID:
					// Consider this cause for failing the entire Job
					Processor->bProcessingFailed = TRUE;
					break;
				case NSwarm::JOB_TASK_STATE_ACCEPTED:
					break;
				case NSwarm::JOB_TASK_STATE_REJECTED:
					// Consider this cause for failing the entire Job
					Processor->bProcessingFailed = TRUE;
					break;
				case NSwarm::JOB_TASK_STATE_RUNNING:
					break;
				case NSwarm::JOB_TASK_STATE_COMPLETE_SUCCESS:
				{
					FGuid TaskGuid;
					Copy( TaskStateMessage->TaskGuid, TaskGuid );
					FGuid PrecomputedVolumeLightingGuid;
					Copy( Lightmass::PrecomputedVolumeLightingGuid, PrecomputedVolumeLightingGuid );
					FGuid MeshAreaLightDataGuid;
					Copy( Lightmass::MeshAreaLightDataGuid, MeshAreaLightDataGuid );
					FGuid VolumeDistanceFieldGuid;
					Copy( Lightmass::VolumeDistanceFieldGuid, VolumeDistanceFieldGuid );
					if (TaskGuid == PrecomputedVolumeLightingGuid)
					{
						appInterlockedIncrement( &VolumeSampleTaskCompleted );
						appInterlockedIncrement( &Processor->NumCompletedTasks );
					}
					else if (Processor->Exporter->VisibilityBucketGuids.ContainsItem(TaskGuid))
					{
						TList<FGuid>* NewElement = new TList<FGuid>(TaskGuid, NULL);
						Processor->CompletedVisibilityTasks.AddElement( NewElement );
						appInterlockedIncrement( &Processor->NumCompletedTasks );
					}
					else if (TaskGuid == MeshAreaLightDataGuid)
					{
						appInterlockedIncrement( &MeshAreaLightDataTaskCompleted );
						appInterlockedIncrement( &Processor->NumCompletedTasks );
					}
					else if (TaskGuid == VolumeDistanceFieldGuid)
					{
						appInterlockedIncrement( &VolumeDistanceFieldTaskCompleted );
						appInterlockedIncrement( &Processor->NumCompletedTasks );
					}
					else if (Processor->DominantShadowTaskLights.FindRef(TaskGuid))
					{
						TList<FGuid>* NewElement = new TList<FGuid>(TaskGuid, NULL);
						Processor->CompletedDominantShadowTasks.AddElement( NewElement );
						appInterlockedIncrement( &Processor->NumCompletedTasks );
					}
					else
					{
						// Add a mapping to the list of mapping GUIDs that have been completed
						TList<FGuid>* NewElement = new TList<FGuid>(TaskGuid, NULL);
						Processor->CompletedMappingTasks.AddElement( NewElement );
						appInterlockedIncrement( &Processor->NumCompletedTasks );
					}
					break;
				}
				case NSwarm::JOB_TASK_STATE_COMPLETE_FAILURE:
				{
					// Add a mapping to the list of mapping GUIDs that have been completed
					FGuid TaskGuid;
					Copy( TaskStateMessage->TaskGuid, TaskGuid );
					TList<FGuid>* NewElement = new TList<FGuid>(TaskGuid, NULL);
					Processor->CompletedMappingTasks.AddElement( NewElement );
					appInterlockedIncrement( &Processor->NumCompletedTasks );

					// Consider this cause for failing the entire Job
					Processor->bProcessingFailed = TRUE;

					break;
				}
				case NSwarm::JOB_TASK_STATE_KILLED:
					break;
				default:
					break;
			}
		}
		break;

		case NSwarm::MESSAGE_INFO:
		{
#if !NO_LOGGING && !FINAL_RELEASE
			NSwarm::FInfoMessage* InfoMessage = (NSwarm::FInfoMessage*)CallbackMessage;
			GLog->Log( InfoMessage->TextMessage );
#endif
		}
		break;

		case NSwarm::MESSAGE_UPDATE:
		{
			GWarn->UpdateProgress( -1, -1 );
		}
		break;

		case NSwarm::MESSAGE_ALERT:
		{
			NSwarm::FAlertMessage* AlertMessage = (NSwarm::FAlertMessage*)CallbackMessage;
			MapCheckType CheckType = MCTYPE_INFO;

			switch (AlertMessage->AlertLevel)
			{
			case NSwarm::ALERT_LEVEL_WARNING:			CheckType = MCTYPE_WARNING;			break;
			case NSwarm::ALERT_LEVEL_ERROR:				CheckType = MCTYPE_ERROR;			break;
			case NSwarm::ALERT_LEVEL_CRITICAL_ERROR:	CheckType = MCTYPE_CRITICALERROR;	break;
			}

			UObject* Object = NULL;
			FGuid ObjectGuid = FGuid(	AlertMessage->ObjectGuid.A, 
										AlertMessage->ObjectGuid.B,
										AlertMessage->ObjectGuid.C,
										AlertMessage->ObjectGuid.D);
			switch (AlertMessage->TypeId)
			{
			case Lightmass::SOURCEOBJECTTYPE_StaticMesh:
				Object = Processor->FindStaticMesh(ObjectGuid);
				break;
			case Lightmass::SOURCEOBJECTTYPE_Mapping:
				const FStaticLightingMapping* FoundMapping = Processor->GetLightmassExporter()->FindMappingByGuid(ObjectGuid);
				if (FoundMapping)
				{
					Object = FoundMapping->GetMappedObject();
				}
				break;
			}

			FString LocalizedMessage = Localize(TEXT("Lightmass"), AlertMessage->TextMessage, TEXT("UnrealEd"));
			if (LocalizedMessage.InStr(TEXT("?UnrealEd.Lightmass")) != INDEX_NONE)
			{
				// Didn't find the string - display it directly?
				LocalizedMessage = AlertMessage->TextMessage;
			}
			GWarn->LightingBuild_Add(CheckType, Object, *LocalizedMessage);
		}
		break;

		case NSwarm::MESSAGE_QUIT:
		{
			Processor->bQuitReceived = TRUE;
		}
		break;
	}

	Processor->Statistics.SwarmCallbackTime += appSeconds() - SwarmCallbackStartTime;
}

/*-----------------------------------------------------------------------------
	FLightmassExporter
-----------------------------------------------------------------------------*/
FLightmassExporter::FLightmassExporter()
:	Swarm( NSwarm::FSwarmInterface::Get() ) 
{
	if (GLightmassDebugOptions.bDebugMode)
	{
		SceneGuid = FGuid(0x0123, 0x4567, 0x89AB, 0xCDEF);
	}
	else
	{
		SceneGuid = appCreateGuid();
	}
	Lightmass::FGuid LightmassGuid;
	Copy( SceneGuid, LightmassGuid );
	ChannelName = Lightmass::CreateChannelName(LightmassGuid, Lightmass::LM_SCENE_VERSION, Lightmass::LM_SCENE_EXTENSION);

	appMemzero(&CustomImportanceBoundingBox, sizeof(FBox));
}

FLightmassExporter::~FLightmassExporter()
{
}

void FLightmassExporter::AddMaterial(UMaterialInterface* InMaterialInterface)
{
	if (InMaterialInterface)
	{
		// Check for material texture changes...
		//@TODO: Add package to warning list if it needs to be resaved (perf warning)
		InMaterialInterface->UpdateLightmassTextureTracking();

		// Check material instance parent...
		UMaterialInstance* MaterialInst = Cast<UMaterialInstance>(InMaterialInterface);
		if (MaterialInst)
		{
			if (MaterialInst->Parent)
			{
				if (MaterialInst->ParentLightingGuid != MaterialInst->Parent->LightingGuid)
				{
					//@TODO: Add package to warning list if it needs to be resaved (perf warning)
					MaterialInst->ParentLightingGuid = MaterialInst->Parent->LightingGuid;
					MaterialInst->LightingGuid = appCreateGuid();
				}
			}
		}

		Materials.AddItem(InMaterialInterface);
	}
}

void FLightmassExporter::AddTerrainMaterialResource(FTerrainMaterialResource* InTerrainMaterialResource)
{
	if (InTerrainMaterialResource)
	{
		//@lmtodo. Check material textures, etc.
		TerrainMaterialResources.AddItem(InTerrainMaterialResource);
	}
}

const FStaticLightingMapping* FLightmassExporter::FindMappingByGuid(FGuid FindGuid) const
{
	for( INT MappingIdx=0; MappingIdx < BSPSurfaceMappings.Num(); MappingIdx++ )
	{
		const FStaticLightingMapping* CurrentMapping = BSPSurfaceMappings(MappingIdx);
		if (CurrentMapping->GetLightingGuid() == FindGuid)
		{
			return CurrentMapping;
		}
	}

	for( INT MappingIdx=0; MappingIdx < StaticMeshTextureMappings.Num(); MappingIdx++ )
	{
		const FStaticLightingMapping* CurrentMapping = StaticMeshTextureMappings(MappingIdx);
		if (CurrentMapping->GetLightingGuid() == FindGuid)
		{
			return CurrentMapping;
		}
	}

	for( INT MappingIdx=0; MappingIdx < StaticMeshVertexMappings.Num(); MappingIdx++ )
	{
		const FStaticLightingMapping* CurrentMapping = StaticMeshVertexMappings(MappingIdx);
		if (CurrentMapping->GetLightingGuid() == FindGuid)
		{
			return CurrentMapping;
		}
	}
	for( INT MappingIdx=0; MappingIdx < TerrainMappings.Num(); MappingIdx++ )
	{
		const FStaticLightingMapping* CurrentMapping = TerrainMappings(MappingIdx);
		if (CurrentMapping->GetLightingGuid() == FindGuid)
		{
			return CurrentMapping;
		}
	}
	for( INT MappingIdx=0; MappingIdx < FluidSurfaceTextureMappings.Num(); MappingIdx++ )
	{
		const FStaticLightingMapping* CurrentMapping = FluidSurfaceTextureMappings(MappingIdx);
		if (CurrentMapping->GetLightingGuid() == FindGuid)
		{
			return CurrentMapping;
		}
	}
	for( INT MappingIdx=0; MappingIdx < LandscapeTextureMappings.Num(); MappingIdx++ )
	{
		const FStaticLightingMapping* CurrentMapping = LandscapeTextureMappings(MappingIdx);
		if (CurrentMapping->GetLightingGuid() == FindGuid)
		{
			return CurrentMapping;
		}
	}

#if WITH_SPEEDTREE
	for( INT MappingIdx=0; MappingIdx < SpeedTreeMappings.Num(); MappingIdx++ )
	{
		const FStaticLightingMapping* CurrentMapping = SpeedTreeMappings(MappingIdx);
		if (CurrentMapping->GetLightingGuid() == FindGuid)
		{
			return CurrentMapping;
		}
	}
#endif

	return NULL;
}

void FLightmassExporter::WriteToChannel( FGuid& DebugMappingGuid )
{
	// Initialize the debug mapping Guid to something not in the scene.
	DebugMappingGuid = FGuid(0x96dc6516, 0xa616421d, 0x82f0ef5b, 0x299152b5);
	if( bSwarmConnectionIsValid )
	{
		INT Channel = Swarm.OpenChannel( *ChannelName, LM_SCENE_CHANNEL_FLAGS );
		if( Channel >= 0 )
		{
			// Ensure the default material is present...
			AddMaterial(GEngine->DefaultMaterial);

			TotalProgress = 
				DirectionalLights.Num() + PointLights.Num() + SpotLights.Num() + SkyLights.Num() + 
				StaticMeshes.Num() + StaticMeshLightingMeshes.Num() + StaticMeshTextureMappings.Num() + StaticMeshVertexMappings.Num() + 
				BSPSurfaceMappings.Num() + Materials.Num() + 
				Terrains.Num() + TerrainMappings.Num() + TerrainMaterialResources.Num() + 
				FluidSurfaceLightingMeshes.Num() + FluidSurfaceTextureMappings.Num()
				+ LandscapeLightingMeshes.Num() + LandscapeTextureMappings.Num()
#if WITH_SPEEDTREE
				+ SpeedTrees.Num() + SpeedTreeLightingMeshes.Num() + SpeedTreeMappings.Num()
#endif	//#if WITH_SPEEDTREE
				;

			CurrentProgress = 0;

			// Export scene header.
			Lightmass::FSceneFileHeader Scene;
			Scene.Cookie = 'SCEN';
			Scene.FormatVersion = Lightmass::FGuid( 0, 0, 0, 1 );
			Scene.Guid = Lightmass::FGuid( 0, 0, 0, 1 );
			Lightmass::FBox LMCustomImportanceBoundingBox;
			Copy(CustomImportanceBoundingBox, LMCustomImportanceBoundingBox);
			Scene.SetCustomImportanceBoundingBox(LMCustomImportanceBoundingBox);

			WriteSceneSettings(Scene);
			WriteDebugInput(Scene.DebugInput, DebugMappingGuid);

			/** If TRUE, pad the mappings (shrink the requested size and then pad) */
			Scene.bPadMappings = GLightmassDebugOptions.bPadMappings;
			Scene.bDebugPadding = GLightmassDebugOptions.bDebugPaddings;
			Scene.ExecutionTimeDivisor = GLightmassDebugOptions.ExecutionTimeDivisor;
			Scene.bColorByExecutionTime = GLightmassDebugOptions.bColorByExecutionTime;
			Scene.bUseRandomColors = GLightmassDebugOptions.bUseRandomColors;
			Scene.bColorBordersGreen = GLightmassDebugOptions.bColorBordersGreen;
			Scene.bOnlyCalcDebugTexelMappings = GLightmassDebugOptions.bOnlyCalcDebugTexelMappings;

#if WITH_SPEEDTREE
			Scene.bWithSpeedTree = TRUE;
#else
			Scene.bWithSpeedTree = FALSE;
#endif
			Scene.bBuildOnlyVisibleLevels = bVisibleLevelsOnly;
			Scene.NumImportanceVolumes = ImportanceVolumes.Num();
			Scene.NumCharacterIndirectDetailVolumes = CharacterIndirectDetailVolumes.Num();
			Scene.NumDirectionalLights = DirectionalLights.Num();
			Scene.NumPointLights = PointLights.Num();
			Scene.NumSpotLights = SpotLights.Num();
			Scene.NumSkyLights = SkyLights.Num();
			Scene.NumStaticMeshes = StaticMeshes.Num();
			Scene.NumTerrains = Terrains.Num();
			Scene.NumStaticMeshInstances = StaticMeshLightingMeshes.Num();
			Scene.NumFluidSurfaceInstances = FluidSurfaceLightingMeshes.Num();
			Scene.NumLandscapeInstances = LandscapeLightingMeshes.Num();
#if WITH_SPEEDTREE
			Scene.NumSpeedTreeLightingMeshes = SpeedTreeLightingMeshes.Num();
#else
			Scene.NumSpeedTreeLightingMeshes = 0;
#endif
			Scene.NumBSPMappings = BSPSurfaceMappings.Num();
			Scene.NumStaticMeshTextureMappings = StaticMeshTextureMappings.Num();
			Scene.NumStaticMeshVertexMappings = StaticMeshVertexMappings.Num();	
			Scene.NumTerrainMappings = TerrainMappings.Num();
			Scene.NumFluidSurfaceTextureMappings = FluidSurfaceTextureMappings.Num();
			Scene.NumLandscapeTextureMappings = LandscapeTextureMappings.Num();
#if WITH_SPEEDTREE
			Scene.NumSpeedTreeMappings = SpeedTreeMappings.Num();
#else
			Scene.NumSpeedTreeMappings = 0;
#endif
			Scene.NumPrecomputedVisibilityBuckets = VisibilityBucketGuids.Num();
			Swarm.WriteChannel( Channel, &Scene, sizeof(Scene) );

			const INT UserNameLength = appStrlen(appUserName());
			Swarm.WriteChannel( Channel, &UserNameLength, sizeof(UserNameLength) );
			Swarm.WriteChannel( Channel, appUserName(), UserNameLength * sizeof(TCHAR) );

			const INT LevelNameLength = LevelName.Len();
			Swarm.WriteChannel( Channel, &LevelNameLength, sizeof(LevelNameLength) );
			Swarm.WriteChannel( Channel, *LevelName, LevelName.Len() * sizeof(TCHAR) );

			for (INT VolumeIndex = 0; VolumeIndex < ImportanceVolumes.Num(); VolumeIndex++)
			{
				Lightmass::FBox LMBox;
				Copy(ImportanceVolumes(VolumeIndex), LMBox);
				Swarm.WriteChannel(Channel, &LMBox, sizeof(LMBox));
			}

			for (INT VolumeIndex = 0; VolumeIndex < CharacterIndirectDetailVolumes.Num(); VolumeIndex++)
			{
				Lightmass::FBox LMBox;
				Copy(CharacterIndirectDetailVolumes(VolumeIndex), LMBox);
				Swarm.WriteChannel(Channel, &LMBox, sizeof(LMBox));
			}

			WriteVisibilityData(Channel);

			WriteLights( Channel );
			WriteModels();
			WriteStaticMeshes();
			WriteTerrains();
#if WITH_SPEEDTREE
			WriteSpeedTrees();
#endif	//#if WITH_SPEEDTREE
			WriteMaterials();
			WriteMeshInstances( Channel );
			WriteFluidSurfaceInstances( Channel );
			WriteLandscapeInstances( Channel );
#if WITH_SPEEDTREE
			WriteSpeedTreeMeshInstances( Channel );
#endif	//#if WITH_SPEEDTREE
			WriteMappings( Channel );

			Swarm.CloseChannel( Channel );
		}
		else
		{
			debugf( TEXT("Error, OpenChannel failed to open %s with error code %d"), *ChannelName, Channel );
		}
	}
}

void FLightmassExporter::WriteVisibilityData( INT Channel )
{
	Swarm.WriteChannel( Channel, VisibilityBucketGuids.GetData(), VisibilityBucketGuids.Num() * VisibilityBucketGuids.GetTypeSize() );

	INT NumVisVolumes = 0;
	for( TObjectIterator<APrecomputedVisibilityVolume> It; It; ++It )
	{
		if (GWorld->ContainsActor(*It))
		{
			NumVisVolumes++;
		}
	}

	if (GWorld->GetWorldInfo()->bPrecomputeVisibility && NumVisVolumes == 0)
	{
		GWarn->LightingBuild_Add(MCTYPE_CRITICALERROR, NULL, *Localize(TEXT("Lightmass"), TEXT("LightmassError_MissingPrecomputedVisibilityVolume"), TEXT("UnrealEd")));
	}

	// Export the visibility volumes that indicate to lightmass where to place visibility cells
	Swarm.WriteChannel( Channel, &NumVisVolumes, sizeof(NumVisVolumes) );
	for( TObjectIterator<APrecomputedVisibilityVolume> It; It; ++It )
	{
		APrecomputedVisibilityVolume* Volume = *It;
		if (GWorld->ContainsActor(Volume))
		{
			Lightmass::FBox LMBox;
			Copy(Volume->GetComponentsBoundingBox(TRUE), LMBox);
			Swarm.WriteChannel(Channel, &LMBox, sizeof(LMBox));

			TArray<FPlane> Planes;
			Volume->Brush->GetSurfacePlanes(Volume, Planes);
			const INT NumPlanes = Planes.Num();
			Swarm.WriteChannel( Channel, &NumPlanes, sizeof(NumPlanes) );
			Swarm.WriteChannel( Channel, Planes.GetData(), Planes.Num() * Planes.GetTypeSize() );
		}
	}

	INT NumOverrideVisVolumes = 0;
	for( TObjectIterator<APrecomputedVisibilityOverrideVolume> It; It; ++It )
	{
		if (GWorld->ContainsActor(*It))
		{
			NumOverrideVisVolumes++;
		}
	}

	Swarm.WriteChannel( Channel, &NumOverrideVisVolumes, sizeof(NumOverrideVisVolumes) );
	for( TObjectIterator<APrecomputedVisibilityOverrideVolume> It; It; ++It )
	{
		APrecomputedVisibilityOverrideVolume* Volume = *It;
		if (GWorld->ContainsActor(Volume))
		{
			Lightmass::FBox LMBox;
			Copy(Volume->GetComponentsBoundingBox(TRUE), LMBox);
			Swarm.WriteChannel(Channel, &LMBox, sizeof(LMBox));

			TArray<INT> VisibilityIds;
			for (INT ActorIndex = 0; ActorIndex < Volume->OverrideVisibleActors.Num(); ActorIndex++)
			{
				AActor* CurrentActor = Volume->OverrideVisibleActors(ActorIndex);
				if (CurrentActor && !CurrentActor->bMovable)
				{
					for (INT ComponentIndex = 0; ComponentIndex < CurrentActor->Components.Num(); ComponentIndex++)
					{
						UPrimitiveComponent* CurrentComponent = Cast<UPrimitiveComponent>(CurrentActor->Components(ComponentIndex));
						if (CurrentComponent && CurrentComponent->VisibilityId != INDEX_NONE)
						{
							VisibilityIds.AddUniqueItem(CurrentComponent->VisibilityId);
						}
					}
				}
			}
			TArray<INT> InvisibilityIds;
			for (INT RemoveActorIndex = 0; RemoveActorIndex < Volume->OverrideInvisibleActors.Num(); RemoveActorIndex++)
			{
				AActor* RemoveActor = Volume->OverrideInvisibleActors(RemoveActorIndex);
				if (RemoveActor && !RemoveActor->bMovable)
				{
					for (INT ComponentIndex = 0; ComponentIndex < RemoveActor->Components.Num(); ComponentIndex++)
					{
						UPrimitiveComponent* RemoveComponent = Cast<UPrimitiveComponent>(RemoveActor->Components(ComponentIndex));
						if (RemoveComponent && RemoveComponent->VisibilityId != INDEX_NONE)
						{
							InvisibilityIds.AddUniqueItem(RemoveComponent->VisibilityId);
						}
					}
				}
			}
			
			const INT NumVisibilityIds = VisibilityIds.Num();
			Swarm.WriteChannel( Channel, &NumVisibilityIds, sizeof(NumVisibilityIds) );
			Swarm.WriteChannel( Channel, VisibilityIds.GetData(), VisibilityIds.Num() * VisibilityIds.GetTypeSize() );
			const INT NumInvisibilityIds = InvisibilityIds.Num();
			Swarm.WriteChannel( Channel, &NumInvisibilityIds, sizeof(NumInvisibilityIds) );
			Swarm.WriteChannel( Channel, InvisibilityIds.GetData(), InvisibilityIds.Num() * InvisibilityIds.GetTypeSize() );
		}
	}

	const FLOAT CellSize = GWorld->GetWorldInfo()->VisibilityCellSize;

	TArray<FVector4> CameraTrackPositions;
	if (GWorld->GetWorldInfo()->bPrecomputeVisibility)
	{
		// Export positions along matinee camera tracks
		// Lightmass needs to know these positions in order to place visibility cells containing them, since they may be outside any visibility volumes
		for( TObjectIterator<ACameraActor> CameraIt; CameraIt; ++CameraIt )
		{
			ACameraActor* Camera = *CameraIt;
			if (GWorld->ContainsActor(Camera))
			{
				for( TObjectIterator<USeqAct_Interp> It; It; ++It )
				{
					USeqAct_Interp* Interp = *It;
					UBOOL bNeedsTermInterp = FALSE;
					if (Interp->GroupInst.Num() == 0)
					{
						// If matinee is closed, GroupInst will be empty, so we need to populate it
						bNeedsTermInterp = TRUE;
						Interp->InitInterp();
					}
					UInterpGroupInst* GroupInstance = It->FindGroupInst(Camera);
					if (GroupInstance && GroupInstance->Group)
					{
						for (INT TrackIndex = 0; TrackIndex < GroupInstance->Group->InterpTracks.Num(); TrackIndex++)
						{
							UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(GroupInstance->Group->InterpTracks(TrackIndex));
							if (MoveTrack)
							{
								FLOAT StartTime;
								FLOAT EndTime;
								//@todo - look at the camera cuts to only process time ranges where the camera is actually active
								MoveTrack->GetTimeRange(StartTime, EndTime);
								for (INT TrackInstanceIndex = 0; TrackInstanceIndex < GroupInstance->TrackInst.Num(); TrackInstanceIndex++)
								{
									UInterpTrackInst* TrackInstance = GroupInstance->TrackInst(TrackInstanceIndex);
									UInterpTrackInstMove* MoveTrackInstance = Cast<UInterpTrackInstMove>(TrackInstance);
									if (MoveTrackInstance)
									{
										//@todo - handle long camera paths
										for (FLOAT Time = StartTime; Time < EndTime; Time += Max((EndTime - StartTime) * .001f, .001f))
										{
											const FVector RelativePosition = MoveTrack->EvalPositionAtTime(TrackInstance, Time);
											FVector CurrentPosition;
											FRotator CurrentRotation;
											MoveTrack->ComputeWorldSpaceKeyTransform(MoveTrackInstance, RelativePosition, FRotator(EC_EventParm), CurrentPosition, CurrentRotation);
											if (CameraTrackPositions.Num() == 0 || !CurrentPosition.Equals(CameraTrackPositions.Last(), CellSize * .1f))
											{
												CameraTrackPositions.AddItem(CurrentPosition);
											}
										}
									}
								}
							}
						}
					}
					if (bNeedsTermInterp)
					{
						Interp->TermInterp();
					}
				}
			}
		}
	}
	
	INT NumCameraPositions = CameraTrackPositions.Num();
	Swarm.WriteChannel( Channel, &NumCameraPositions, sizeof(NumCameraPositions) );
	Swarm.WriteChannel( Channel, CameraTrackPositions.GetData(), CameraTrackPositions.Num() * CameraTrackPositions.GetTypeSize() );
}

void FLightmassExporter::WriteLights( INT Channel )
{
	// Export directional lights.
	for ( INT LightIndex = 0; LightIndex < DirectionalLights.Num(); ++LightIndex )
	{
		const UDirectionalLightComponent* Light = DirectionalLights(LightIndex);
		Lightmass::FLightData LightData;
		Lightmass::FDirectionalLightData DirectionalData;
		Copy( Light, LightData );
		Copy( Light->LightmassSettings.IndirectLightingScale, LightData.IndirectLightingScale );
		Copy( Light->LightmassSettings.IndirectLightingSaturation, LightData.IndirectLightingSaturation );
		Copy( Light->LightmassSettings.ShadowExponent, LightData.ShadowExponent );
		Copy( Light->LightmassSettings.LightSourceAngle * (FLOAT)PI / 180.0f, DirectionalData.LightSourceAngle );
		Swarm.WriteChannel( Channel, &LightData, sizeof(LightData) );
		Swarm.WriteChannel( Channel, &DirectionalData, sizeof(DirectionalData) );
		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}

	// Export point lights.
	for ( INT LightIndex = 0; LightIndex < PointLights.Num(); ++LightIndex )
	{
		const UPointLightComponent* Light = PointLights(LightIndex);
		Lightmass::FLightData LightData;
		Lightmass::FPointLightData PointData;
		Copy( Light, LightData );
		Copy( Light->LightmassSettings.IndirectLightingScale, LightData.IndirectLightingScale );
		Copy( Light->LightmassSettings.IndirectLightingSaturation, LightData.IndirectLightingSaturation );
		Copy( Light->LightmassSettings.ShadowExponent, LightData.ShadowExponent );
		Copy( Light->LightmassSettings.LightSourceRadius, LightData.LightSourceRadius );
		Copy( Light->Radius, PointData.Radius );
		Copy( Light->FalloffExponent, PointData.FalloffExponent );
		Swarm.WriteChannel( Channel, &LightData, sizeof(LightData) );
		Swarm.WriteChannel( Channel, &PointData, sizeof(PointData) );
		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}

	// Export spot lights.
	for ( INT LightIndex = 0; LightIndex < SpotLights.Num(); ++LightIndex )
	{
		const USpotLightComponent* Light = SpotLights(LightIndex);
		Lightmass::FLightData LightData;
		Lightmass::FPointLightData PointData;
		Lightmass::FSpotLightData SpotData;
		Copy( Light, LightData ); 
		Copy( Light->LightmassSettings.IndirectLightingScale, LightData.IndirectLightingScale );
		Copy( Light->LightmassSettings.IndirectLightingSaturation, LightData.IndirectLightingSaturation );
		Copy( Light->LightmassSettings.ShadowExponent, LightData.ShadowExponent );
		Copy( Light->LightmassSettings.LightSourceRadius, LightData.LightSourceRadius );
		Copy( Light->Radius, PointData.Radius );
		Copy( Light->FalloffExponent, PointData.FalloffExponent );
		Copy( Light->InnerConeAngle, SpotData.InnerConeAngle ); 
		Copy( Light->OuterConeAngle, SpotData.OuterConeAngle );
		Swarm.WriteChannel( Channel, &LightData, sizeof(LightData) );
		Swarm.WriteChannel( Channel, &PointData, sizeof(PointData) );
		Swarm.WriteChannel( Channel, &SpotData, sizeof(SpotData) );
		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}

	// Export sky lights.
	for ( INT LightIndex = 0; LightIndex < SkyLights.Num(); ++LightIndex )
	{
		const USkyLightComponent* Light = SkyLights(LightIndex);
		Lightmass::FLightData LightData;
		Lightmass::FSkyLightData SkyData;
		Copy( Light, LightData );
		Copy( Light->LowerBrightness, SkyData.LowerBrightness );
		Copy( Light->LowerColor, SkyData.LowerColor );
		Swarm.WriteChannel( Channel, &LightData, sizeof(LightData) );
		Swarm.WriteChannel( Channel, &SkyData, sizeof(SkyData) );
		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}
}

/**
 * Exports all UModels to secondary, persistent channels
 */
void FLightmassExporter::WriteModels()
{
	for (INT ModelIndex = 0; ModelIndex < Models.Num(); ModelIndex++)
	{
		
	}
}

/**
 * Exports all UStaticMeshes to secondary, persistent channels
 */
void FLightmassExporter::WriteStaticMeshes()
{
	// Export geometry resources.
	for (INT StaticMeshIndex = 0; StaticMeshIndex < StaticMeshes.Num() && !GEditor->GetMapBuildCancelled(); StaticMeshIndex++)
	{
		const UStaticMesh* StaticMesh = StaticMeshes(StaticMeshIndex);

		Lightmass::FBaseMeshData BaseMeshData;
		Copy(StaticMesh->LightingGuid, BaseMeshData.Guid);

		// create a channel name to write the mesh out to
		FString ChannelName = Lightmass::CreateChannelName(BaseMeshData.Guid, Lightmass::LM_STATICMESH_VERSION, Lightmass::LM_STATICMESH_EXTENSION);

		// Warn the user if there is an invalid lightmap UV channel specified.
		if( StaticMesh->LightMapCoordinateIndex > 0 && StaticMesh->LODModels.Num() > 0 )
		{
			const FStaticMeshRenderData& RenderData = StaticMesh->LODModels(0);
			if( StaticMesh->LightMapCoordinateIndex >= (INT)RenderData.VertexBuffer.GetNumTexCoords() )
			{
				GWarn->LightingBuild_Add(MCTYPE_WARNING, const_cast<UStaticMesh*>(StaticMesh), *Localize(TEXT("Lightmass"), TEXT("LightmassError_BadLightMapCoordinateIndex"), TEXT("UnrealEd")));
			}
		}

		// only export the static mesh if it's not currently in the cache
		if ((GSwarmDebugOptions.bForceContentExport == TRUE) ||
			(Swarm.TestChannel(*ChannelName) < 0))
		{
			// open the channel
			INT Channel = Swarm.OpenChannel( *ChannelName, LM_STATICMESH_CHANNEL_FLAGS );
			if( Channel >= 0 )
			{
				// write out base data
				Swarm.WriteChannel(Channel, &BaseMeshData, sizeof(BaseMeshData));

				Lightmass::FStaticMeshData StaticMeshData;
				StaticMeshData.LightmapCoordinateIndex = StaticMesh->LightMapCoordinateIndex;
				StaticMeshData.NumLODs = StaticMesh->LODModels.Num();
				Swarm.WriteChannel( Channel, &StaticMeshData, sizeof(StaticMeshData) );
				for (INT LODIndex = 0; LODIndex < StaticMesh->LODModels.Num(); LODIndex++)
				{
					const FStaticMeshRenderData& RenderData = StaticMesh->LODModels(LODIndex);
					Lightmass::FStaticMeshLODData SMLODData;
					SMLODData.NumElements = RenderData.Elements.Num();
					SMLODData.NumTriangles = RenderData.GetTriangleCount();
					SMLODData.NumIndices = RenderData.IndexBuffer.Indices.Num();
					// the vertex buffer could have double vertices for shadow buffer data, so we use what the render data thinks it has, not what is actually there
					SMLODData.NumVertices = RenderData.NumVertices;
					Swarm.WriteChannel( Channel, &SMLODData, sizeof(SMLODData) );

					INT ElementCount = RenderData.Elements.Num();
					if( ElementCount > 0 )
					{
						TArray<Lightmass::FStaticMeshElementData> LMElements;
						LMElements.Empty(ElementCount);
						LMElements.AddZeroed(ElementCount);
						for (INT ElementIndex = 0; ElementIndex < ElementCount; ElementIndex++)
						{
							const FStaticMeshElement& Element = RenderData.Elements(ElementIndex);
							Lightmass::FStaticMeshElementData& SMElementData = LMElements(ElementIndex);
							SMElementData.FirstIndex = Element.FirstIndex;
							SMElementData.NumTriangles = Element.NumTriangles;
							SMElementData.bEnableShadowCasting = Element.bEnableShadowCasting;
						}
						Swarm.WriteChannel( Channel, (void*)(LMElements.GetData()), ElementCount * sizeof(Lightmass::FStaticMeshElementData) );
					}

					Swarm.WriteChannel( Channel, (void*)(RenderData.IndexBuffer.Indices.GetData()), RenderData.IndexBuffer.Indices.Num() * sizeof(WORD));
					
					INT VertexCount = SMLODData.NumVertices;
					if( VertexCount > 0 )
					{
						TArray<Lightmass::FStaticMeshVertex> LMVertices;
						LMVertices.Empty(VertexCount);
						LMVertices.AddZeroed(VertexCount);
						for (INT VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
						{
							Lightmass::FStaticMeshVertex& Vertex = LMVertices(VertexIndex);
							Copy(RenderData.PositionVertexBuffer.VertexPosition(VertexIndex), Vertex.Position);
							Copy(RenderData.VertexBuffer.VertexTangentX(VertexIndex), Vertex.TangentX);
							Copy(RenderData.VertexBuffer.VertexTangentY(VertexIndex), Vertex.TangentY);
							Copy(RenderData.VertexBuffer.VertexTangentZ(VertexIndex), Vertex.TangentZ);
							INT UVCount = Clamp<INT>(RenderData.VertexBuffer.GetNumTexCoords(), 0, MAX_TEXCOORDS);
							INT UVIndex;
							for (UVIndex = 0; UVIndex < UVCount; UVIndex++)
							{
								Copy(RenderData.VertexBuffer.GetVertexUV(VertexIndex, UVIndex), Vertex.UVs[UVIndex]);
							}
							FVector2D ZeroUV(0.0f, 0.0f);
							for (; UVIndex < MAX_TEXCOORDS; UVIndex++)
							{
								Copy(ZeroUV, Vertex.UVs[UVIndex]);
							}
						}
						Swarm.WriteChannel( Channel, (void*)(LMVertices.GetData()), LMVertices.Num() * sizeof(Lightmass::FStaticMeshVertex));
					}
				}

				// close the channel, the whole mesh is now exported
				Swarm.CloseChannel(Channel);
			}
			else
			{
				debugf( TEXT("Error, OpenChannel failed to open %s with error code %d"), *ChannelName, Channel );
			}
		}
		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}
}

/**
 * Exports all ATerrains to secondary, persistent channels
 */
void FLightmassExporter::WriteTerrains()
{
	// Export geometry resources.
	for (INT TerrainIndex = 0; TerrainIndex < Terrains.Num() && !GEditor->GetMapBuildCancelled(); TerrainIndex++)
	{
		const ATerrain* Terrain = Terrains(TerrainIndex);
		check(Terrain);

		if (0)
		{
			debugf(TEXT("TERRAIN DUMP"));
			debugf(TEXT("\tGuid                             : %s"), *(Terrain->LightingGuid.String()));
			debugf(TEXT("\tLocalToWorld                     : "));
			FMatrix LocalToWorld = Terrain->LocalToWorld();
			debugf(TEXT("\t\t%-8.5f,%-8.5f,%-8.5f,%-8.5f"), LocalToWorld.M[0][0], LocalToWorld.M[0][1], LocalToWorld.M[0][2], LocalToWorld.M[0][3]);
			debugf(TEXT("\t\t%-8.5f,%-8.5f,%-8.5f,%-8.5f"), LocalToWorld.M[1][0], LocalToWorld.M[1][1], LocalToWorld.M[1][2], LocalToWorld.M[1][3]);
			debugf(TEXT("\t\t%-8.5f,%-8.5f,%-8.5f,%-8.5f"), LocalToWorld.M[2][0], LocalToWorld.M[2][1], LocalToWorld.M[2][2], LocalToWorld.M[2][3]);
			debugf(TEXT("\t\t%-8.5f,%-8.5f,%-8.5f,%-8.5f"), LocalToWorld.M[3][0], LocalToWorld.M[3][1], LocalToWorld.M[3][2], LocalToWorld.M[3][3]);
			debugf(TEXT("\tWorldToLocal                     : "));
			FMatrix WorldToLocal = Terrain->WorldToLocal();
			debugf(TEXT("\t\t%-8.5f,%-8.5f,%-8.5f,%-8.5f"), WorldToLocal.M[0][0], WorldToLocal.M[0][1], WorldToLocal.M[0][2], WorldToLocal.M[0][3]);
			debugf(TEXT("\t\t%-8.5f,%-8.5f,%-8.5f,%-8.5f"), WorldToLocal.M[1][0], WorldToLocal.M[1][1], WorldToLocal.M[1][2], WorldToLocal.M[1][3]);
			debugf(TEXT("\t\t%-8.5f,%-8.5f,%-8.5f,%-8.5f"), WorldToLocal.M[2][0], WorldToLocal.M[2][1], WorldToLocal.M[2][2], WorldToLocal.M[2][3]);
			debugf(TEXT("\t\t%-8.5f,%-8.5f,%-8.5f,%-8.5f"), WorldToLocal.M[3][0], WorldToLocal.M[3][1], WorldToLocal.M[3][2], WorldToLocal.M[3][3]);
			debugf(TEXT("\tLocation                         : %-8.5f,%-8.5f,%-8.5f,%-8.5f"), Terrain->Location.X, Terrain->Location.Y, Terrain->Location.Z, 1.0f);
			FVector _EulerRotation = Terrain->Rotation.Euler();
			debugf(TEXT("\tRotation                         : %-8.5f,%-8.5f,%-8.5f,%-8.5f"), _EulerRotation.X, _EulerRotation.Y, _EulerRotation.Z, 1.0f);
			debugf(TEXT("\tDrawScale3D                      : %-8.5f,%-8.5f,%-8.5f,%-8.5f"), Terrain->DrawScale3D.X, Terrain->DrawScale3D.Y, Terrain->DrawScale3D.Z, 1.0f);
			debugf(TEXT("\tDrawScale                        : %-8.5f"), Terrain->DrawScale);
			debugf(TEXT("\tNumSectionsX                     : %d"), Terrain->NumSectionsX);
			debugf(TEXT("\tNumSectionsY                     : %d"), Terrain->NumSectionsY);
			debugf(TEXT("\tSectionSize                      : %d"), 0);//removed Terrain->SectionSize);
			debugf(TEXT("\tMaxCollisionDisplacement         : %-8.5f"), 0.0f);
			debugf(TEXT("\tMaxTesselationLevel              : %d"), Terrain->MaxTesselationLevel);
			debugf(TEXT("\tMinTessellationLevel             : %d"), Terrain->MinTessellationLevel);
			debugf(TEXT("\tTesselationDistanceScale         : %-8.5f"), Terrain->TesselationDistanceScale);
			debugf(TEXT("\tTessellationCheckDistance        : %-8.5f"), Terrain->TessellationCheckDistance);
			debugf(TEXT("\tNumVerticesX                     : %d"), Terrain->NumVerticesX);
			debugf(TEXT("\tNumVerticesY                     : %d"), Terrain->NumVerticesY);
			debugf(TEXT("\tNumPatchesX                      : %d"), Terrain->NumPatchesX);
			debugf(TEXT("\tNumPatchesY                      : %d"), Terrain->NumPatchesY);
			debugf(TEXT("\tMaxComponentSize                 : %d"), Terrain->MaxComponentSize);
			debugf(TEXT("\tStaticLightingResolution         : %d"), Terrain->StaticLightingResolution);
			debugf(TEXT("\tbIsOverridingLightResolution     : %s"), Terrain->bIsOverridingLightResolution ? TEXT("TRUE") : TEXT("FALSE"));
			debugf(TEXT("\tbBilinearFilterLightmapGeneration: %s"), Terrain->bBilinearFilterLightmapGeneration ? TEXT("TRUE") : TEXT("FALSE"));
			debugf(TEXT("\tbCastShadow                      : %s"), Terrain->bCastShadow ? TEXT("TRUE") : TEXT("FALSE"));
			debugf(TEXT("\tbForceDirectLightMap             : %s"), Terrain->bForceDirectLightMap ? TEXT("TRUE") : TEXT("FALSE"));
			debugf(TEXT("\tbCastDynamicShadow               : %s"), Terrain->bCastDynamicShadow ? TEXT("TRUE") : TEXT("FALSE"));
			debugf(TEXT("\tbAcceptsDynamicLights            : %s"), Terrain->bAcceptsDynamicLights ? TEXT("TRUE") : TEXT("FALSE"));
			debugf(TEXT("\tNumMaterials                     : %d"), 0);
			INT TempNumCollisionVerts = (Terrain->NumVerticesX + 1) * (Terrain->NumVerticesY + 1);
			debugf(TEXT("\tNumCollisionVertices             : %d"), TempNumCollisionVerts);

			FString DebugOut;
			debugf(TEXT("\tHeights                          : %d"), Terrain->Heights.Num());
			DebugOut = TEXT("\t\t");
			for (INT Idx = 0; Idx < Terrain->Heights.Num(); Idx++)
			{
				DebugOut += FString::Printf(TEXT("%4d,"), Terrain->Heights(Idx).Value);
				if ((Idx + 1) % 8 == 0)
				{
					debugf(*DebugOut);
					DebugOut = TEXT("\t\t");
				}
			}

			debugf(TEXT("\tInfoData                        : %d"), Terrain->InfoData.Num());
			DebugOut = TEXT("\t\t");
			for (INT Idx = 0; Idx < Terrain->InfoData.Num(); Idx++)
			{
				DebugOut += FString::Printf(TEXT("0x%02x,"), Terrain->InfoData(Idx).Data);
				if ((Idx + 1) % 16 == 0)
				{
					debugf(*DebugOut);
					DebugOut = TEXT("\t\t");
				}
			}

			debugf(TEXT("\tCachedDisplacements             : %d"), 0);
			DebugOut = TEXT("\t\t");

		//	TArray<FGuid>		TerrainMaterials;

			debugf(TEXT("\tCollisionVertices                : %d"), TempNumCollisionVerts);
			INT SubX = 0;
			INT	SubY = 0;
			for (INT IntY = 0; IntY <= Terrain->NumVerticesY; IntY++)
			{
				for (INT IntX = 0; IntX <= Terrain->NumVerticesX; IntX++)
				{
					FTerrainPatch	Patch = Terrain->GetPatch(IntX, IntY);
					FVector			Vertex = Terrain->GetCollisionVertex(Patch, IntX, IntY, SubX, SubY, 1);
					debugf(TEXT("\t\t%5d: %-8.5f,%-8.5f,%-8.5f"), 
						(IntY * (Terrain->NumVerticesX + 1)) + IntX,
						Vertex.X, Vertex.Y, Vertex.Z);
				}
			}

			debugf(TEXT("END TERRAIN DUMP"));
		}

		Lightmass::FBaseTerrainData BaseTerrainData;
		Copy(Terrain->LightingGuid, BaseTerrainData.Guid);

		// create a channel name to write the mesh out to
		FString ChannelName = Lightmass::CreateChannelName(BaseTerrainData.Guid, Lightmass::LM_TERRAIN_VERSION, Lightmass::LM_TERRAIN_EXTENSION);

		// only export the terrain if it's not currently in the cache
		if ((GSwarmDebugOptions.bForceContentExport == TRUE) ||
			(Swarm.TestChannel(*ChannelName) < 0))
		{
			// open the channel
			INT Channel = Swarm.OpenChannel( *ChannelName, LM_TERRAIN_CHANNEL_FLAGS );
			if (Channel >= 0)
			{
				// write out base data
				Swarm.WriteChannel(Channel, &BaseTerrainData, sizeof(BaseTerrainData));

				// write out the terrain data
				Lightmass::FTerrainData TerrainData;

				Copy(Terrain->LocalToWorld(), TerrainData.LocalToWorld);
				Copy(Terrain->WorldToLocal(), TerrainData.WorldToLocal);
				Copy(Terrain->Location, TerrainData.Location);
				FVector EulerRotation = Terrain->Rotation.Euler();
				Copy(EulerRotation, TerrainData.Rotation);
				Copy(Terrain->DrawScale3D, TerrainData.DrawScale3D);
				TerrainData.DrawScale = Terrain->DrawScale;
				TerrainData.NumSectionsX = Terrain->NumSectionsX;
				TerrainData.NumSectionsY = Terrain->NumSectionsY;
				TerrainData.SectionSize = 0;// removed Terrain->SectionSize;
				TerrainData.MaxCollisionDisplacement = 0.0f;
				TerrainData.MaxTesselationLevel = Terrain->MaxTesselationLevel;
				TerrainData.MinTessellationLevel = Terrain->MinTessellationLevel;
				TerrainData.TesselationDistanceScale = Terrain->TesselationDistanceScale;
				TerrainData.TessellationCheckDistance = Terrain->TessellationCheckDistance;
				TerrainData.NumVerticesX = Terrain->NumVerticesX;
				TerrainData.NumVerticesY = Terrain->NumVerticesY;
				TerrainData.NumPatchesX = Terrain->NumPatchesX;
				TerrainData.NumPatchesY = Terrain->NumPatchesY;
				TerrainData.MaxComponentSize = Terrain->MaxComponentSize;
				TerrainData.StaticLightingResolution = Terrain->StaticLightingResolution;
				TerrainData.bIsOverridingLightResolution = Terrain->bIsOverridingLightResolution;
				TerrainData.bBilinearFilterLightmapGeneration = Terrain->bBilinearFilterLightmapGeneration;
				TerrainData.bCastShadow = Terrain->bCastShadow;
				TerrainData.bForceDirectLightMap = Terrain->bForceDirectLightMap;
				TerrainData.bCastDynamicShadow = Terrain->bCastDynamicShadow;
				TerrainData.bAcceptsDynamicLights = Terrain->bAcceptsDynamicLights;
				//@lmtodo. Materials!
				//TerrainData.NumMaterials = Terrain->CachedTerrainMaterials[0].CachedMaterials.Num();
				TerrainData.NumMaterials = 0;

				INT NumCollisionVertices = (Terrain->NumVerticesX + 1) * (Terrain->NumVerticesY + 1);
				TerrainData.NumCollisionVertices = NumCollisionVertices;

				INT WriteSize = sizeof(Lightmass::FTerrainData);
				INT CheckSize = Swarm.WriteChannel(Channel, &TerrainData, WriteSize);
				if (CheckSize != sizeof(TerrainData))
				{
					warnf(NAME_Warning, TEXT("Lightmass::FTerrainData size mismatch!"));
				}

				// Now write out the arrays of data...
				INT NumHeights = (Terrain->NumVerticesX * Terrain->NumVerticesY);
				check(NumHeights == Terrain->Heights.Num());
				// Heights...
				WORD* HeightData = (WORD*)(Terrain->Heights.GetData());
				Swarm.WriteChannel(Channel, (void*)(HeightData), NumHeights * sizeof(WORD));
				BYTE* InfoData = (BYTE*)(Terrain->InfoData.GetData());
				Swarm.WriteChannel(Channel, (void*)(InfoData), NumHeights * sizeof(BYTE));
				TArray<BYTE> CachedDisplacements;
				CachedDisplacements.AddZeroed(NumHeights);
				BYTE* CachedDisplacementData = (BYTE*)(CachedDisplacements.GetData());
				Swarm.WriteChannel(Channel, (void*)(CachedDisplacementData), NumHeights * sizeof(BYTE));
				//@lmtodo. Materials!
				//TArray<FGuid> TerrainMaterials;				- GUID

				FVector4* CollisionVertices = new FVector4[NumCollisionVertices];
				check(CollisionVertices);

				INT SubX = 0;
				INT	SubY = 0;
				for (INT IntY = 0; IntY <= TerrainData.NumVerticesY; IntY++)
				{
					for (INT IntX = 0; IntX <= TerrainData.NumVerticesX; IntX++)
					{
						FTerrainPatch	Patch = Terrain->GetPatch(IntX, IntY);
						FVector			Vertex = Terrain->GetCollisionVertex(Patch, IntX, IntY, SubX, SubY, 1);
						CollisionVertices[IntY * (TerrainData.NumVerticesX + 1) + IntX] = FVector4(Vertex,1.0f);
					}
				}
				Swarm.WriteChannel(Channel, (void*)(CollisionVertices), NumCollisionVertices * sizeof(FVector4));
				delete [] CollisionVertices;

				// close the channel, the whole terrain is now exported
				Swarm.CloseChannel(Channel);
			}
			else
			{
				debugf( TEXT("Error, OpenChannel failed to open %s with error code %d"), *ChannelName, Channel );
			}
		}
		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}
}

#if WITH_SPEEDTREE
/**
 * Exports all USpeedTrees to secondary, persistent channels
 */
void FLightmassExporter::WriteSpeedTrees()
{
	for (INT SpeedTreeIndex = 0; SpeedTreeIndex < SpeedTrees.Num(); SpeedTreeIndex++)
	{
		const USpeedTree* SpeedTree = SpeedTrees(SpeedTreeIndex);

		Lightmass::FBaseMeshData BaseMeshData;
		Copy(SpeedTree->LightingGuid, BaseMeshData.Guid);

		// create a channel name to write the mesh out to
		FString ChannelName = Lightmass::CreateChannelName(BaseMeshData.Guid, Lightmass::LM_SPEEDTREE_VERSION, Lightmass::LM_SPEEDTREE_EXTENSION);

		// only export the static mesh if it's not currently in the cache
		if ((GSwarmDebugOptions.bForceContentExport == TRUE) ||
			(Swarm.TestChannel(*ChannelName) < 0))
		{
			// open the channel
			INT Channel = Swarm.OpenChannel( *ChannelName, LM_SPEEDTREE_CHANNEL_FLAGS );
			if( Channel >= 0 )
			{
				// write out base data
				Swarm.WriteChannel(Channel, &BaseMeshData, sizeof(BaseMeshData));

				// fill out shared structure
				Lightmass::FSpeedTreeData SpeedTreeData;
				SpeedTreeData.NumIndices = SpeedTree->SRH->IndexBuffer.Indices.Num();
				SpeedTreeData.NumBranchVertices = SpeedTree->SRH->BranchPositionBuffer.Vertices.Num();
				SpeedTreeData.NumFrondVertices = SpeedTree->SRH->FrondPositionBuffer.Vertices.Num();
				SpeedTreeData.NumLeafMeshVertices = SpeedTree->SRH->LeafMeshPositionBuffer.Vertices.Num();
				SpeedTreeData.NumLeafCardVertices = SpeedTree->SRH->LeafCardPositionBuffer.Vertices.Num();
				SpeedTreeData.NumBillboardVertices = SpeedTree->SRH->BillboardPositionBuffer.Vertices.Num();

				Swarm.WriteChannel(Channel, &SpeedTreeData, sizeof(SpeedTreeData));

				// @lmtodo: Export the Elements
				
				// write the indices
				Swarm.WriteChannel(Channel, SpeedTree->SRH->IndexBuffer.Indices.GetData(), SpeedTree->SRH->IndexBuffer.Indices.Num() * sizeof(WORD));

				// write all the separate vertices
				if (SpeedTreeData.NumBranchVertices > 0)
				{
					const INT VertexCount = SpeedTreeData.NumBranchVertices;

					// export the verts just like a static mesh vert
					TArray<Lightmass::FSpeedTreeVertex> LMVertices;
					LMVertices.Empty(VertexCount);
					LMVertices.AddZeroed(VertexCount);
					for (INT VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
					{
						Lightmass::FSpeedTreeVertex& Vertex = LMVertices(VertexIndex);

						// fill out the branch and frond verts
						Copy(SpeedTree->SRH->BranchPositionBuffer.Vertices(VertexIndex).Position, Vertex.Position);
						Copy(SpeedTree->SRH->BranchDataBuffer.Vertices(VertexIndex).TangentX, Vertex.TangentX);
						Copy(SpeedTree->SRH->BranchDataBuffer.Vertices(VertexIndex).TangentY, Vertex.TangentY);
						Copy(SpeedTree->SRH->BranchDataBuffer.Vertices(VertexIndex).TangentZ, Vertex.TangentZ);
						Copy(SpeedTree->SRH->BranchDataBuffer.Vertices(VertexIndex).TexCoord, Vertex.TexCoord);
					}

					// write the branch and frond verts
					Swarm.WriteChannel(Channel, (void*)(LMVertices.GetData()), LMVertices.Num() * sizeof(Lightmass::FSpeedTreeVertex));
				}
				// write all the separate vertices
				if (SpeedTreeData.NumFrondVertices > 0)
				{
					const INT VertexCount = SpeedTreeData.NumFrondVertices;

					// export the verts just like a static mesh vert
					TArray<Lightmass::FSpeedTreeVertex> LMVertices;
					LMVertices.Empty(VertexCount);
					LMVertices.AddZeroed(VertexCount);
					for (INT VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
					{
						Lightmass::FSpeedTreeVertex& Vertex = LMVertices(VertexIndex);

						// fill out the branch and frond verts
						Copy(SpeedTree->SRH->FrondPositionBuffer.Vertices(VertexIndex).Position, Vertex.Position);
						Copy(SpeedTree->SRH->FrondDataBuffer.Vertices(VertexIndex).TangentX, Vertex.TangentX);
						Copy(SpeedTree->SRH->FrondDataBuffer.Vertices(VertexIndex).TangentY, Vertex.TangentY);
						Copy(SpeedTree->SRH->FrondDataBuffer.Vertices(VertexIndex).TangentZ, Vertex.TangentZ);
						Copy(SpeedTree->SRH->FrondDataBuffer.Vertices(VertexIndex).TexCoord, Vertex.TexCoord);
					}

					// write the branch and frond verts
					Swarm.WriteChannel(Channel, (void*)(LMVertices.GetData()), LMVertices.Num() * sizeof(Lightmass::FSpeedTreeVertex));
				}
				if (SpeedTreeData.NumLeafMeshVertices > 0)
				{
					INT VertexCount = SpeedTreeData.NumLeafMeshVertices;

					// export the verts just like a static mesh vert
					TArray<Lightmass::FSpeedTreeVertex> LMVertices;
					LMVertices.Empty(VertexCount);
					LMVertices.AddZeroed(VertexCount);
					for (INT VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
					{
						Lightmass::FSpeedTreeVertex& Vertex = LMVertices(VertexIndex);

						// fill out the branch and frond verts
						Copy(SpeedTree->SRH->LeafMeshPositionBuffer.Vertices(VertexIndex).Position, Vertex.Position);
						Copy(SpeedTree->SRH->LeafMeshDataBuffer.Vertices(VertexIndex).TangentX, Vertex.TangentX);
						Copy(SpeedTree->SRH->LeafMeshDataBuffer.Vertices(VertexIndex).TangentY, Vertex.TangentY);
						Copy(SpeedTree->SRH->LeafMeshDataBuffer.Vertices(VertexIndex).TangentZ, Vertex.TangentZ);
						Copy(SpeedTree->SRH->LeafMeshDataBuffer.Vertices(VertexIndex).TexCoord, Vertex.TexCoord);
					}

					// write the branch and frond verts
					Swarm.WriteChannel(Channel, (void*)(LMVertices.GetData()), LMVertices.Num() * sizeof(Lightmass::FSpeedTreeVertex));
				}
				if (SpeedTreeData.NumLeafCardVertices > 0)
				{
					INT VertexCount = SpeedTreeData.NumLeafCardVertices;

					// export the verts just like a static mesh vert
					TArray<Lightmass::FSpeedTreeVertex> LMVertices;
					LMVertices.Empty(VertexCount);
					LMVertices.AddZeroed(VertexCount);
					for (INT VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
					{
						Lightmass::FSpeedTreeVertex& Vertex = LMVertices(VertexIndex);

						// fill out the branch and frond verts
						Copy(SpeedTree->SRH->LeafCardPositionBuffer.Vertices(VertexIndex).Position, Vertex.Position);
						Copy(SpeedTree->SRH->LeafCardDataBuffer.Vertices(VertexIndex).TangentX, Vertex.TangentX);
						Copy(SpeedTree->SRH->LeafCardDataBuffer.Vertices(VertexIndex).TangentY, Vertex.TangentY);
						Copy(SpeedTree->SRH->LeafCardDataBuffer.Vertices(VertexIndex).TangentZ, Vertex.TangentZ);
						Copy(SpeedTree->SRH->LeafCardDataBuffer.Vertices(VertexIndex).TexCoord, Vertex.TexCoord);
						Copy(SpeedTree->SRH->LeafCardDataBuffer.Vertices(VertexIndex).CornerOffset, Vertex.CornerOffset);
					}

					// write the branch and frond verts
					Swarm.WriteChannel(Channel, (void*)(LMVertices.GetData()), LMVertices.Num() * sizeof(Lightmass::FSpeedTreeVertex));
				}
				if (SpeedTreeData.NumBillboardVertices > 0)
				{
					INT VertexCount = SpeedTreeData.NumBillboardVertices;

					// export the verts just like a static mesh vert
					TArray<Lightmass::FSpeedTreeVertex> LMVertices;
					LMVertices.Empty(VertexCount);
					LMVertices.AddZeroed(VertexCount);
					for (INT VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
					{
						Lightmass::FSpeedTreeVertex& Vertex = LMVertices(VertexIndex);

						// fill out the branch and frond verts
						Copy(SpeedTree->SRH->BillboardPositionBuffer.Vertices(VertexIndex).Position, Vertex.Position);
						Copy(SpeedTree->SRH->BillboardDataBuffer.Vertices(VertexIndex).TangentX, Vertex.TangentX);
						Copy(SpeedTree->SRH->BillboardDataBuffer.Vertices(VertexIndex).TangentY, Vertex.TangentY);
						Copy(SpeedTree->SRH->BillboardDataBuffer.Vertices(VertexIndex).TangentZ, Vertex.TangentZ);
						Copy(SpeedTree->SRH->BillboardDataBuffer.Vertices(VertexIndex).TexCoord, Vertex.TexCoord);
					}

					// write the branch and frond verts
					Swarm.WriteChannel(Channel, (void*)(LMVertices.GetData()), LMVertices.Num() * sizeof(Lightmass::FSpeedTreeVertex));
				}

				// close the channel, the whole mesh is now exported
				Swarm.CloseChannel(Channel);
			}
			else
			{
				debugf( TEXT("Error, OpenChannel failed to open %s with error code %d"), *ChannelName, Channel );
			}
		}
		
		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}
}
#endif	//#if WITH_SPEEDTREE

/**
 *	Exports all of the materials to secondary, persistent channels
 */
void FLightmassExporter::WriteMaterials()
{
	// Standard materials
	for (INT MaterialIndex = 0; MaterialIndex < Materials.Num() && !GEditor->GetMapBuildCancelled(); MaterialIndex++)
	{
		UMaterialInterface* Material = Materials(MaterialIndex);
		if (Material)
		{
			Lightmass::FBaseMaterialData BaseMaterialData;
			Copy(Material->LightingGuid, BaseMaterialData.Guid);

			// create a channel name to write the material out to
			FString ChannelName = Lightmass::CreateChannelName(BaseMaterialData.Guid, Lightmass::LM_MATERIAL_VERSION, Lightmass::LM_MATERIAL_EXTENSION);

			// only export the material if it's not currently in the cache
			INT ErrorCode;
			if ( GSwarmDebugOptions.bForceContentExport == TRUE )
			{
				// If we're forcing export of content, pretend we didn't find it
				ErrorCode = NSwarm::SWARM_ERROR_FILE_FOUND_NOT;
			}
			else
			{
				// Otherwise, test the channel
				ErrorCode = Swarm.TestChannel(*ChannelName);
			}

			if ( ErrorCode != NSwarm::SWARM_SUCCESS )
			{
				if( ErrorCode == NSwarm::SWARM_ERROR_FILE_FOUND_NOT )
				{
					// Generate the required information
					Lightmass::FMaterialData MaterialData;
					appMemzero(&MaterialData,sizeof(MaterialData));
					UMaterial* BaseMaterial = Material->GetMaterial();
					MaterialData.bTwoSided = BaseMaterial->TwoSided;
					Copy( Material->GetEmissiveBoost(), MaterialData.EmissiveBoost);
					Copy( Material->GetDiffuseBoost(), MaterialData.DiffuseBoost);
					Copy( Material->GetSpecularBoost(), MaterialData.SpecularBoost);

					TArray<FFloat16Color> MaterialEmissive;
					TArray<FFloat16Color> MaterialDiffuse;
					TArray<FFloat16Color> MaterialSpecular;
					TArray<FFloat16Color> MaterialTransmission;
					TArray<FFloat16Color> MaterialNormal;

					// Only generate normal maps if we'll actually need them for lighting
					const UBOOL bWantNormals = GEngine->bUseNormalMapsForSimpleLightMaps;

					if (MaterialRenderer.GenerateMaterialData(*Material, bWantNormals, MaterialData, 
						MaterialEmissive, MaterialDiffuse, MaterialSpecular, MaterialTransmission, MaterialNormal) == TRUE)
					{
						// open the channel
						INT Channel = Swarm.OpenChannel( *ChannelName, LM_MATERIAL_CHANNEL_FLAGS );
						if( Channel >= 0 )
						{
							// write out base data
							Swarm.WriteChannel(Channel, &BaseMaterialData, sizeof(BaseMaterialData));

							// the material data
							Swarm.WriteChannel(Channel, &MaterialData, sizeof(MaterialData));

							// Write each array of data
							BYTE* OutData;
							INT OutSize;

							OutSize = Square(MaterialData.EmissiveSize) * sizeof(FFloat16Color);  
							if (OutSize > 0)
							{
								OutData = (BYTE*)(MaterialEmissive.GetData());
								Swarm.WriteChannel(Channel, OutData, OutSize);
							}

							OutSize = Square(MaterialData.DiffuseSize) * sizeof(FFloat16Color);
							if (OutSize > 0)
							{
								OutData = (BYTE*)(MaterialDiffuse.GetData());
								Swarm.WriteChannel(Channel, OutData, OutSize);
							}

							OutSize = Square(MaterialData.SpecularSize) * sizeof(FFloat16Color);
							if (OutSize > 0)
							{
								OutData = (BYTE*)(MaterialSpecular.GetData());
								Swarm.WriteChannel(Channel, OutData, OutSize);
							}
		
							OutSize = Square(MaterialData.TransmissionSize) * sizeof(FFloat16Color);
							if (OutSize > 0)
							{
								OutData = (BYTE*)(MaterialTransmission.GetData());
								Swarm.WriteChannel(Channel, OutData, OutSize);
							}

							OutSize = Square(MaterialData.NormalSize) * sizeof(FFloat16Color);
							if (OutSize > 0)
							{
								OutData = (BYTE*)(MaterialNormal.GetData());
								Swarm.WriteChannel(Channel, OutData, OutSize);
							}

							// close the channel, the whole mesh is now exported
							Swarm.CloseChannel(Channel);
						}
						else
						{
							warnf(NAME_Warning, TEXT("Failed to open channel for material data for %s: %s"), *(Material->LightingGuid.String()), *(Material->GetPathName()));
						}
					}
					else
					{
						warnf(NAME_Warning, TEXT("Failed to generate material data for %s: %s"), *(Material->LightingGuid.String()), *(Material->GetPathName()));
					}
				}
				else
				{
					warnf(NAME_Warning, TEXT("Error in TestChannel() for %s: %s"), *(Material->LightingGuid.String()), *(Material->GetPathName()));
				}
			}
		}
		
		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}

	// Terrain materials
	for (INT TerrainIdx = 0; TerrainIdx < TerrainMaterialResources.Num(); TerrainIdx++)
	{
		FTerrainMaterialResource* TerrainMatRes = TerrainMaterialResources(TerrainIdx);
		if (TerrainMatRes)
		{
			Lightmass::FBaseMaterialData BaseMaterialData;
			Copy(TerrainMatRes->GetLightingGuid(), BaseMaterialData.Guid);

			// create a channel name to write the material out to
			FString ChannelName = Lightmass::CreateChannelName(BaseMaterialData.Guid, Lightmass::LM_MATERIAL_VERSION, Lightmass::LM_MATERIAL_EXTENSION);

			// only export the material if it's not currently in the cache
			INT ErrorCode;
			if ( GSwarmDebugOptions.bForceContentExport == TRUE )
			{
				// If we're forcing export of content, pretend we didn't find it
				ErrorCode = NSwarm::SWARM_ERROR_FILE_FOUND_NOT;
			}
			else
			{
				// Otherwise, test the channel
				ErrorCode = Swarm.TestChannel(*ChannelName);
			}

			if ( ErrorCode != NSwarm::SWARM_SUCCESS )
			{
				if( ErrorCode == NSwarm::SWARM_ERROR_FILE_FOUND_NOT )
				{
					// Generate the required information
					Lightmass::FMaterialData MaterialData;
					appMemzero(&MaterialData,sizeof(MaterialData));

// 					Copy( Material->LightmassSettings.EmissiveBoost, MaterialData.EmissiveBoost);
// 					Copy( Material->LightmassSettings.DiffuseBoost, MaterialData.DiffuseBoost);
// 					Copy( Material->LightmassSettings.SpecularBoost, MaterialData.SpecularBoost);
					MaterialData.EmissiveBoost = 1.0f;
					MaterialData.DiffuseBoost = 1.0f;
					MaterialData.SpecularBoost = 1.0f;

					TArray<FFloat16Color> MaterialEmissive;
					TArray<FFloat16Color> MaterialDiffuse;
					TArray<FFloat16Color> MaterialSpecular;
					TArray<FFloat16Color> MaterialTransmission;
					TArray<FFloat16Color> MaterialNormal;

					// Only generate normal maps if we'll actually need them for lighting
					const UBOOL bWantNormals = GEngine->bUseNormalMapsForSimpleLightMaps;

					if (TerrainMaterialRenderer.GenerateTerrainMaterialData(*TerrainMatRes, bWantNormals, MaterialData, 
						MaterialEmissive, MaterialDiffuse, MaterialSpecular, MaterialTransmission, MaterialNormal) == TRUE)
					{
						// open the channel
						INT Channel = Swarm.OpenChannel( *ChannelName, LM_MATERIAL_CHANNEL_FLAGS );
						if( Channel >= 0 )
						{
							// write out base data
							Swarm.WriteChannel(Channel, &BaseMaterialData, sizeof(BaseMaterialData));

							// the material data
							Swarm.WriteChannel(Channel, &MaterialData, sizeof(MaterialData));

							// Write each array of data
							BYTE* OutData;
							INT OutSize;

							OutSize = Square(MaterialData.EmissiveSize) * sizeof(FFloat16Color);  
							if (OutSize > 0)
							{
								OutData = (BYTE*)(MaterialEmissive.GetData());
								Swarm.WriteChannel(Channel, OutData, OutSize);
							}

							OutSize = Square(MaterialData.DiffuseSize) * sizeof(FFloat16Color);
							if (OutSize > 0)
							{
								OutData = (BYTE*)(MaterialDiffuse.GetData());
								Swarm.WriteChannel(Channel, OutData, OutSize);
							}

							OutSize = Square(MaterialData.SpecularSize) * sizeof(FFloat16Color);
							if (OutSize > 0)
							{
								OutData = (BYTE*)(MaterialSpecular.GetData());
								Swarm.WriteChannel(Channel, OutData, OutSize);
							}
		
							OutSize = Square(MaterialData.TransmissionSize) * sizeof(FFloat16Color);
							if (OutSize > 0)
							{
								OutData = (BYTE*)(MaterialTransmission.GetData());
								Swarm.WriteChannel(Channel, OutData, OutSize);
							}

							OutSize = Square(MaterialData.NormalSize) * sizeof(FFloat16Color);
							if (OutSize > 0)
							{
								OutData = (BYTE*)(MaterialNormal.GetData());
								Swarm.WriteChannel(Channel, OutData, OutSize);
							}
							// close the channel, the whole mesh is now exported
							Swarm.CloseChannel(Channel);
						}
						else
						{
							warnf(NAME_Warning, TEXT("Failed to open channel for terrain material data for %s: %s"), 
								*(TerrainMatRes->GetLightingGuid().String()), *(TerrainMatRes->GetFriendlyName()));
						}
					}
					else
					{
						warnf(NAME_Warning, TEXT("Failed to generate terrain material data for %s: %s"), 
							*(TerrainMatRes->GetLightingGuid().String()), *(TerrainMatRes->GetFriendlyName()));
					}
				}
				else
				{
					warnf(NAME_Warning, TEXT("Error in TestChannel() for %s: %s"), 
						*(TerrainMatRes->GetLightingGuid().String()), *(TerrainMatRes->GetFriendlyName()));
				}
			}
		}
		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}
}

void FLightmassExporter::WriteBaseMeshInstanceData( INT Channel, INT MeshIndex, const FStaticLightingMesh* Mesh, TArray<Lightmass::FMaterialElementData>& MaterialElementData )
{
	Lightmass::FStaticLightingMeshInstanceData MeshInstanceData;
	appMemzero(&MeshInstanceData,sizeof(MeshInstanceData));
	Copy(Mesh->Guid, MeshInstanceData.Guid);
	MeshInstanceData.NumTriangles = Mesh->NumTriangles;
	MeshInstanceData.NumShadingTriangles = Mesh->NumShadingTriangles;
	MeshInstanceData.NumVertices = Mesh->NumVertices;
	MeshInstanceData.NumShadingVertices = Mesh->NumShadingVertices;
	MeshInstanceData.MeshIndex = MeshIndex; 
	MeshInstanceData.LevelId = INDEX_NONE;
	check(Mesh->Component);
	UBOOL bFoundLevel = FALSE;
	AActor* ComponentOwner = Mesh->Component->GetOwner();
	if (ComponentOwner && ComponentOwner->GetLevel())
	{
		const ULevel* MeshLevel = Mesh->Component->GetOwner()->GetLevel();
		for (INT LevelIndex = 0; LevelIndex < GWorld->Levels.Num(); LevelIndex++)
		{
			if (MeshLevel == GWorld->Levels(LevelIndex))
			{
				MeshInstanceData.LevelId = LevelIndex;
				bFoundLevel = TRUE;
				break;
			}
		}
	}
	else if (Mesh->Component->IsA(UModelComponent::StaticClass()))
	{
		UModelComponent* ModelComponent = CastChecked<UModelComponent>(Mesh->Component);
		for (INT LevelIndex = 0; LevelIndex < GWorld->Levels.Num(); LevelIndex++)
		{
			if (ModelComponent->GetModel() == GWorld->Levels(LevelIndex)->Model)
			{
				MeshInstanceData.LevelId = LevelIndex;
				bFoundLevel = TRUE;
				break;
			}
		}
	}
	
	if (!bFoundLevel)
	{
		warnf(TEXT("Couldn't determine level for component %s during Lightmass export, it will be considered in the persistent level!"), *Mesh->Component->GetPathName());
	}

	MeshInstanceData.LightingFlags = 0;
	MeshInstanceData.LightingFlags |= Mesh->bCastShadow ? Lightmass::GI_INSTANCE_CASTSHADOW : 0;
	MeshInstanceData.LightingFlags |= Mesh->bTwoSidedMaterial ? Lightmass::GI_INSTANCE_TWOSIDED : 0;
	MeshInstanceData.LightingFlags |= Mesh->bSelfShadowOnly ? Lightmass::GI_INSTANCE_SELFSHADOWONLY : 0;
	// For the special case of fracture static meshes, don't allow self shadowing since that
	// will cause the fractured static mesh core to end up unlit
	if(Mesh->Component && Mesh->Component->IsA(UFracturedStaticMeshComponent::StaticClass()))
	{
		MeshInstanceData.LightingFlags |= Lightmass::GI_INSTANCE_SELFSHADOWDISABLE;
	}
	MeshInstanceData.bCastShadowAsTwoSided = Mesh->Component->bCastShadowAsTwoSided;
	MeshInstanceData.bMovable = ComponentOwner ? ComponentOwner->bMovable : FALSE;
	MeshInstanceData.bInstancedStaticMesh = Mesh->Component->IsA(UInstancedStaticMeshComponent::StaticClass());
	MeshInstanceData.NumRelevantLights = Mesh->RelevantLights.Num();
	Copy( Mesh->BoundingBox, MeshInstanceData.BoundingBox );
	Swarm.WriteChannel( Channel, &MeshInstanceData, sizeof(MeshInstanceData) );
	const DWORD LightGuidsSize = Mesh->RelevantLights.Num() * sizeof(Lightmass::FGuid);
	if( LightGuidsSize > 0 )
	{
		Lightmass::FGuid* LightGuids = (Lightmass::FGuid*)appMalloc(LightGuidsSize);
		for( INT LightIdx=0; LightIdx < Mesh->RelevantLights.Num(); LightIdx++ )
		{
			const ULightComponent* Light = Mesh->RelevantLights(LightIdx);
			Copy( Light->LightmapGuid, LightGuids[LightIdx] );
		}
		Swarm.WriteChannel( Channel, LightGuids, LightGuidsSize );
		appFree( LightGuids );
	}

	const INT NumVisibilitiyIds = Mesh->VisibilityIds.Num();
	Swarm.WriteChannel(Channel, &NumVisibilitiyIds, sizeof(NumVisibilitiyIds));
	Swarm.WriteChannel(Channel, Mesh->VisibilityIds.GetData(), Mesh->VisibilityIds.Num() * Mesh->VisibilityIds.GetTypeSize());

	// Always need to have at least one material
	if (MaterialElementData.Num() == 0)
	{
		Lightmass::FMaterialElementData DefaultData;
		Copy(GEngine->DefaultMaterial->LightingGuid, DefaultData.MaterialId);
		MaterialElementData.AddItem(DefaultData);
	}

	// Write out the materials used by this mesh...
	const INT NumMaterialElements = MaterialElementData.Num();
	Swarm.WriteChannel(Channel, &NumMaterialElements, sizeof(NumMaterialElements));
	for (INT MtrlIdx = 0; MtrlIdx < NumMaterialElements; MtrlIdx++)
	{
		Swarm.WriteChannel(Channel, &(MaterialElementData(MtrlIdx)), sizeof(Lightmass::FMaterialElementData));
	}
}

void FLightmassExporter::WriteBaseMappingData( INT Channel, const FStaticLightingMapping* Mapping )
{
	Lightmass::FStaticLightingMappingData MappingData;
	appMemzero(&MappingData,sizeof(MappingData));
	Copy(Mapping->Mesh->Guid, MappingData.Guid);
	Copy(Mapping->Mesh->SourceMeshGuid, MappingData.StaticLightingMeshInstance);
	MappingData.bForceDirectLightMap = Mapping->bForceDirectLightMap ? TRUE : FALSE;
	Swarm.WriteChannel( Channel, &MappingData, sizeof(MappingData) );
}

void FLightmassExporter::WriteBaseVertexMappingData( INT Channel, const FStaticLightingVertexMapping* VertexMapping )
{
	WriteBaseMappingData( Channel, VertexMapping );
	
	Lightmass::FStaticLightingVertexMappingData VertexMappingData;
	appMemzero(&VertexMappingData,sizeof(VertexMappingData));
	VertexMappingData.SampleToAreaRatio = VertexMapping->SampleToAreaRatio;
	VertexMappingData.bSampleVertices = VertexMapping->bSampleVertices ? TRUE : FALSE;
	Swarm.WriteChannel( Channel, &VertexMappingData, sizeof(VertexMappingData) );
}

void FLightmassExporter::WriteBaseTextureMappingData( INT Channel, const FStaticLightingTextureMapping* TextureMapping )
{
	WriteBaseMappingData( Channel, TextureMapping );
	
	Lightmass::FStaticLightingTextureMappingData TextureMappingData;
	appMemzero(&TextureMappingData,sizeof(TextureMappingData));
	TextureMappingData.SizeX = TextureMapping->SizeX;
	TextureMappingData.SizeY = TextureMapping->SizeY;
	TextureMappingData.LightmapTextureCoordinateIndex = TextureMapping->LightmapTextureCoordinateIndex;
	TextureMappingData.bBilinearFilter = TextureMapping->bBilinearFilter;

	Swarm.WriteChannel( Channel, &TextureMappingData, sizeof(TextureMappingData) );
}

void FLightmassExporter::WriteTerrainMapping(INT Channel, const class FTerrainComponentStaticLighting* TerrainMapping)
{
	check(TerrainMapping);
	const UTerrainComponent* TerrainComp = (const UTerrainComponent*)(TerrainMapping->Component);
	check(TerrainComp);
	ATerrain* Terrain = TerrainComp->GetTerrain();

	TArray<Lightmass::FMaterialElementData> MaterialElementData;

	// Fill in the material element data for the terrain
	FTerrainMaterialMask Mask = TerrainComp->BatchMaterials(TerrainComp->FullBatch);
	// Fetch the material instance
	UBOOL bIsTerrainMaterialResourceInstance;
	FMaterialRenderProxy* MaterialRenderProxy = Terrain->GetCachedMaterial(Mask, bIsTerrainMaterialResourceInstance);

	Lightmass::FMaterialElementData TempData;
	if (MaterialRenderProxy && bIsTerrainMaterialResourceInstance)
	{
		FTerrainMaterialResource* TerrainMatRes = (FTerrainMaterialResource*)MaterialRenderProxy;
		Copy(TerrainMatRes->GetLightingGuid(), TempData.MaterialId);
	}
	else
	{
		Copy(GEngine->DefaultMaterial->LightingGuid, TempData.MaterialId);
	}
	TempData.bUseTwoSidedLighting = Terrain->LightmassSettings.bUseTwoSidedLighting;
	TempData.bShadowIndirectOnly = Terrain->LightmassSettings.bShadowIndirectOnly;
	TempData.bUseEmissiveForStaticLighting = Terrain->LightmassSettings.bUseEmissiveForStaticLighting;
	Copy(Terrain->LightmassSettings.EmissiveLightFalloffExponent, TempData.EmissiveLightFalloffExponent);
	Copy(Terrain->LightmassSettings.EmissiveLightExplicitInfluenceRadius, TempData.EmissiveLightExplicitInfluenceRadius);
	Copy(Terrain->LightmassSettings.EmissiveBoost * LevelSettings.EmissiveBoost, TempData.EmissiveBoost);
	Copy(Terrain->LightmassSettings.DiffuseBoost * LevelSettings.DiffuseBoost, TempData.DiffuseBoost);
	Copy(Terrain->LightmassSettings.SpecularBoost * LevelSettings.SpecularBoost, TempData.SpecularBoost);
	Copy(Terrain->LightmassSettings.FullyOccludedSamplesFraction, TempData.FullyOccludedSamplesFraction);
	MaterialElementData.AddItem(TempData);

	WriteBaseMeshInstanceData(Channel, INDEX_NONE, (const FStaticLightingMesh*)TerrainMapping, MaterialElementData);
	WriteBaseTextureMappingData(Channel, (const FStaticLightingTextureMapping*)TerrainMapping);

	Lightmass::FStaticLightingTerrainMappingData TerrainMappingData;
	Copy(Terrain->LightingGuid, TerrainMappingData.TerrainGuid);
	TerrainMappingData.SectionBaseX = TerrainComp->SectionBaseX;
	TerrainMappingData.SectionBaseY = TerrainComp->SectionBaseY;
	TerrainMappingData.SectionSizeX = TerrainComp->SectionSizeX;
	TerrainMappingData.SectionSizeY = TerrainComp->SectionSizeY;
	TerrainMappingData.TrueSectionSizeX = TerrainComp->TrueSectionSizeX;
	TerrainMappingData.TrueSectionSizeY = TerrainComp->TrueSectionSizeY;
	TerrainMappingData.NumQuadsX = TerrainComp->TrueSectionSizeX;
	TerrainMappingData.NumQuadsY = TerrainComp->TrueSectionSizeY;

	// Assuming DXT_1 compression at the moment...
	INT PixelPaddingX = GPixelFormats[PF_DXT1].BlockSizeX;
	INT PixelPaddingY = GPixelFormats[PF_DXT1].BlockSizeY;
	if (GAllowLightmapCompression == FALSE)
	{
		PixelPaddingX = GPixelFormats[PF_A8R8G8B8].BlockSizeX;
		PixelPaddingY = GPixelFormats[PF_A8R8G8B8].BlockSizeY;
	}

	INT PatchExpandCountX = (TERRAIN_PATCH_EXPAND_SCALAR * PixelPaddingX) / Terrain->StaticLightingResolution;
	INT PatchExpandCountY = (TERRAIN_PATCH_EXPAND_SCALAR * PixelPaddingY) / Terrain->StaticLightingResolution;

	PatchExpandCountX = Max<INT>(1, PatchExpandCountX);
	PatchExpandCountY = Max<INT>(1, PatchExpandCountY);

	TerrainMappingData.ExpandQuadsX = PatchExpandCountX;
	TerrainMappingData.ExpandQuadsY = PatchExpandCountY;

	Copy(TerrainComp->LocalToWorld.Inverse().Transpose(), TerrainMappingData.LocalToWorldInverseTranspose);

	Swarm.WriteChannel(Channel, &TerrainMappingData, sizeof(TerrainMappingData));

	// Now write the data...
	Lightmass::FMatrix OutLocalToWorld;
	Copy(TerrainComp->LocalToWorld, OutLocalToWorld);
	Swarm.WriteChannel(Channel, &OutLocalToWorld, sizeof(OutLocalToWorld));
	Lightmass::FVector4 TempBoundsVector;
	Copy(TerrainComp->Bounds.Origin, TempBoundsVector);
	Swarm.WriteChannel(Channel, &TempBoundsVector, sizeof(TempBoundsVector));
	Copy(TerrainComp->Bounds.BoxExtent, TempBoundsVector);
	Swarm.WriteChannel(Channel, &TempBoundsVector, sizeof(TempBoundsVector));
	Swarm.WriteChannel(Channel, (void*)(&(TerrainComp->Bounds.SphereRadius)), sizeof(TerrainComp->Bounds.SphereRadius));

	Terrain->UpdatePatchBounds(0,0,Terrain->NumPatchesX + 1,Terrain->NumPatchesY + 1);
	const FTerrainPatchBounds* OutPatchBounds = TerrainComp->PatchBounds.GetData();
	Swarm.WriteChannel(Channel, (void*)OutPatchBounds, sizeof(FTerrainPatchBounds) * TerrainComp->PatchBounds.Num());

	// Index the quads in the component.
	TArray<FIntPoint> QuadIndexToCoordinateMap;
	for (INT QuadY = -PatchExpandCountY; QuadY < TerrainComp->TrueSectionSizeY + PatchExpandCountY; QuadY++)
	{
		for (INT QuadX = -PatchExpandCountX; QuadX < TerrainComp->TrueSectionSizeX + PatchExpandCountX; QuadX++)
		{
			if(Terrain->IsTerrainQuadVisible(TerrainComp->SectionBaseX + QuadX,TerrainComp->SectionBaseY + QuadY))
			{
				QuadIndexToCoordinateMap.AddItem(FIntPoint(QuadX,QuadY));
			}
		}
	}
	INT NumQuadMappings = QuadIndexToCoordinateMap.Num();
	Swarm.WriteChannel(Channel, &NumQuadMappings, sizeof(INT));
	Swarm.WriteChannel(Channel, QuadIndexToCoordinateMap.GetData(), sizeof(FIntPoint) * NumQuadMappings);

	//TerrainMapping->Dump();
}

void FLightmassExporter::WriteFluidMapping(INT Channel, const class FFluidSurfaceStaticLightingTextureMapping* FluidMapping)
{
	WriteBaseTextureMappingData(Channel, (const FStaticLightingTextureMapping*)FluidMapping);
}

void FLightmassExporter::WriteLandscapeMapping(INT Channel, const class FLandscapeStaticLightingTextureMapping* LandscapeMapping)
{
	WriteBaseTextureMappingData(Channel, (const FStaticLightingTextureMapping*)LandscapeMapping);
}

#if WITH_SPEEDTREE
void FLightmassExporter::WriteSpeedTreeMapping(INT Channel, const class FSpeedTreeStaticLightingMapping* SpeedTreeMapping)
{
	WriteBaseVertexMappingData(Channel, (const FStaticLightingVertexMapping*)SpeedTreeMapping);
}
#endif	//#if WITH_SPEEDTREE

struct FMeshAndLODId
{
	INT MeshIndex;
	INT LODIndex;
};

void FLightmassExporter::WriteMeshInstances( INT Channel )
{
	// initially come up with a unique ID for each component
	TMap<const UPrimitiveComponent*, FMeshAndLODId> ComponentToIDMap;

	INT NextId = 0;
	for( INT MeshIdx=0; MeshIdx < StaticMeshLightingMeshes.Num(); MeshIdx++ )
	{
		const FStaticMeshStaticLightingMesh* SMLightingMesh = StaticMeshLightingMeshes(MeshIdx);
		const UStaticMesh* StaticMesh = SMLightingMesh->StaticMesh;
		if (StaticMesh)
		{
			const UStaticMeshComponent* Primitive = SMLightingMesh->Primitive;
			if (Primitive)
			{
				// All FStaticMeshStaticLightingMesh's in the OtherMeshLODs array need to get the same MeshIndex but different LODIndex
				// So that they won't shadow each other in Lightmass
				if (SMLightingMesh->OtherMeshLODs.Num() > 0)
				{
					FMeshAndLODId* ExistingLODId = NULL;
					INT LargestLODIndex = INDEX_NONE;
					for (INT OtherLODIndex = 0; OtherLODIndex < SMLightingMesh->OtherMeshLODs.Num(); OtherLODIndex++)
					{
						FStaticLightingMesh* OtherLOD = SMLightingMesh->OtherMeshLODs(OtherLODIndex);
						if (OtherLOD->Component)
						{
							FMeshAndLODId* CurrentLODId = ComponentToIDMap.Find(OtherLOD->Component);
							// Find the mesh with the largest index
							if (CurrentLODId && CurrentLODId->LODIndex > LargestLODIndex)
							{
								ExistingLODId = CurrentLODId;
								LargestLODIndex = CurrentLODId->LODIndex;
							}
						}
					}
					if (ExistingLODId)
					{
						FMeshAndLODId NewId;
						// Reuse the mesh index from another LOD
						NewId.MeshIndex = ExistingLODId->MeshIndex;
						// Assign a new unique LOD index
						NewId.LODIndex = ExistingLODId->LODIndex + 1;
						ComponentToIDMap.Set(Primitive, NewId);
					}
					else
					{
						FMeshAndLODId NewId;
						NewId.MeshIndex = NextId++;
						NewId.LODIndex = 0;
						ComponentToIDMap.Set(Primitive, NewId);
					}
				}
				else
				{
					FMeshAndLODId NewId;
					NewId.MeshIndex = NextId++;
					NewId.LODIndex = 0;
					ComponentToIDMap.Set(Primitive, NewId);
				}
			}
		}
	}

#if USE_MASSIVE_LOD
	// now go over the list again and copy the IDs from the parents into their children
	// currently only goes up staticmesh parents (stops at any non-staticmesh component)
	for( INT MeshIdx=0; MeshIdx < StaticMeshLightingMeshes.Num(); MeshIdx++ )
	{
		const FStaticMeshStaticLightingMesh* SMLightingMesh = StaticMeshLightingMeshes(MeshIdx);
		const UStaticMesh* StaticMesh = SMLightingMesh->StaticMesh;
		if (StaticMesh)
		{
			const UStaticMeshComponent* Primitive = SMLightingMesh->Primitive;
			if (Primitive)
			{
				const UPrimitiveComponent* Parent = Primitive;

				// The lowest LOD wants to be 0, with an increasing number going up the chain
				INT CurrentLOD = 0;

				FMeshAndLODId* ParentId = NULL;

				// do we have a staticmesh component parent?
				while (Parent)
				{
					ParentId = ComponentToIDMap.Find(Parent);

					// the parent LOD index can increase safely. for instance, if we have this:
					//      A 
					//   B     C
					//        D  E
					// then B, D, and E want to be LOD 0, C is 1, and A is 2
					// Just because B is 0, it's okay for A to be 2, just just has to be bigger than all children
					if (ParentId)
					{
						ParentId->LODIndex = Max(CurrentLOD, ParentId->LODIndex);
					}

					if (Parent->ReplacementPrimitive && Parent->ReplacementPrimitive->IsA(UStaticMeshComponent::StaticClass()))
					{
						Parent = Parent->ReplacementPrimitive;
						// increase as we go up
						CurrentLOD++;
					}
					else
					{
						Parent = NULL;
					}
				}

				if (ParentId)
				{
					// use that one for me
					FMeshAndLODId* MyId = ComponentToIDMap.Find(Primitive);
					check(MyId);
					MyId->MeshIndex = ParentId->MeshIndex;
				}
			}
		}
	}
#endif


	// static mesh instance meshes
	for( INT MeshIdx=0; MeshIdx < StaticMeshLightingMeshes.Num(); MeshIdx++ )
	{
		const FStaticMeshStaticLightingMesh* SMLightingMesh = StaticMeshLightingMeshes(MeshIdx);

		FMeshAndLODId* MeshId = NULL;

		// Collect the material guids for each element
		TArray<Lightmass::FMaterialElementData> MaterialElementData;
		const UStaticMesh* StaticMesh = SMLightingMesh->StaticMesh;

		if (StaticMesh)
		{
			const UStaticMeshComponent* Primitive = SMLightingMesh->Primitive;
			if (Primitive)
			{	
				// get the meshindex from the component
				MeshId = ComponentToIDMap.Find(Primitive);

				if (SMLightingMesh->LODIndex < StaticMesh->LODModels.Num())
				{
					const FStaticMeshRenderData& LODRenderData = StaticMesh->LODModels(SMLightingMesh->LODIndex);
					for (INT ElementIndex = 0; ElementIndex < LODRenderData.Elements.Num(); ElementIndex++)
					{
						const FStaticMeshElement& Element = LODRenderData.Elements( ElementIndex );
						UMaterialInterface* Material = Primitive->GetMaterial(Element.MaterialIndex, SMLightingMesh->LODIndex);
						if (Material == NULL)
						{
							Material = GEngine->DefaultMaterial;
						}
						Lightmass::FMaterialElementData NewElementData;
						Copy(Material->LightingGuid, NewElementData.MaterialId);
						NewElementData.bUseTwoSidedLighting = Primitive->LightmassSettings.bUseTwoSidedLighting;
						NewElementData.bShadowIndirectOnly = Primitive->LightmassSettings.bShadowIndirectOnly;
						if (Primitive->LightmassSettings.bUseEmissiveForStaticLighting)
						{
							if (StaticMesh->IsA(UFracturedStaticMesh::StaticClass()))
							{
								GWarn->LightingBuild_Add(MCTYPE_WARNING, NULL, *Localize(TEXT("Lightmass"), TEXT("LightmassError_EmissiveFracturedMesh"), TEXT("UnrealEd")));
								NewElementData.bUseEmissiveForStaticLighting = FALSE;
							}
							else
							{
								NewElementData.bUseEmissiveForStaticLighting = TRUE;
							}
						}
						else
						{
							NewElementData.bUseEmissiveForStaticLighting = FALSE;
						}
						// Combine primitive and level boost settings so we don't have to send the level settings over to Lightmass  
						Copy(Primitive->LightmassSettings.EmissiveLightFalloffExponent, NewElementData.EmissiveLightFalloffExponent);
						Copy(Primitive->LightmassSettings.EmissiveLightExplicitInfluenceRadius, NewElementData.EmissiveLightExplicitInfluenceRadius);
						Copy(Primitive->GetEmissiveBoost(ElementIndex) * LevelSettings.EmissiveBoost, NewElementData.EmissiveBoost);
						Copy(Primitive->GetDiffuseBoost(ElementIndex) * LevelSettings.DiffuseBoost, NewElementData.DiffuseBoost);
						Copy(Primitive->GetSpecularBoost(ElementIndex) * LevelSettings.SpecularBoost, NewElementData.SpecularBoost);
						Copy(Primitive->LightmassSettings.FullyOccludedSamplesFraction, NewElementData.FullyOccludedSamplesFraction);
						MaterialElementData.AddItem(NewElementData);
					}
				}
			}
		}

		WriteBaseMeshInstanceData( Channel, MeshId ? MeshId->MeshIndex : INDEX_NONE, SMLightingMesh, MaterialElementData );

		Lightmass::FStaticMeshStaticLightingMeshData SMInstanceMeshData;
		appMemzero(&SMInstanceMeshData,sizeof(SMInstanceMeshData));
		// store the mesh LOD in with the MassiveLOD, by shifting the MassiveLOD by 16
		SMInstanceMeshData.EncodedLODIndex = SMLightingMesh->LODIndex + (MeshId ? (MeshId->LODIndex << 16) : 0);
		Copy( SMLightingMesh->LocalToWorld, SMInstanceMeshData.LocalToWorld );
		SMInstanceMeshData.bReverseWinding = SMLightingMesh->bReverseWinding;
		SMInstanceMeshData.bShouldSelfShadow = TRUE;
		if (SMLightingMesh->StaticMesh && SMLightingMesh->StaticMesh->IsA(UFracturedStaticMesh::StaticClass()))
		{
			SMInstanceMeshData.bShouldSelfShadow = FALSE;
		}
		Copy(SMLightingMesh->StaticMesh->LightingGuid, SMInstanceMeshData.StaticMeshGuid);
		const FSplineMeshParams* SplineParams = SMLightingMesh->GetSplineParameters();
		if (SplineParams && StaticMesh)
		{
			USplineMeshComponent* SplineComponent = CastChecked<USplineMeshComponent>(SMLightingMesh->Component);
			SMInstanceMeshData.bIsSplineMesh = TRUE;
			Copy(*SplineParams, SMInstanceMeshData.SplineParameters);
			Copy(SplineComponent->SplineXDir, SMInstanceMeshData.SplineParameters.SplineXDir);
			SMInstanceMeshData.SplineParameters.bSmoothInterpRollScale = SplineComponent->bSmoothInterpRollScale;
			Copy(StaticMesh->Bounds.Origin.Z - StaticMesh->Bounds.BoxExtent.Z, SMInstanceMeshData.SplineParameters.MeshMinZ);
			Copy(2.f * StaticMesh->Bounds.BoxExtent.Z, SMInstanceMeshData.SplineParameters.MeshRangeZ);
		}
		else
		{
			SMInstanceMeshData.bIsSplineMesh = FALSE;
			appMemzero(&SMInstanceMeshData.SplineParameters, sizeof(SMInstanceMeshData.SplineParameters));
		}

		Swarm.WriteChannel( Channel, &SMInstanceMeshData, sizeof(SMInstanceMeshData) );

		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}
}

void FLightmassExporter::WriteFluidSurfaceInstances( INT Channel )
{
	// fluid surface instance meshes
	for (INT FluidIdx = 0; FluidIdx < FluidSurfaceLightingMeshes.Num(); FluidIdx++)
	{
		const FFluidSurfaceStaticLightingMesh* FluidLightingMesh = FluidSurfaceLightingMeshes(FluidIdx);

		// Collect the material guids for each element
		TArray<Lightmass::FMaterialElementData> MaterialElementData;
		const UFluidSurfaceComponent* FluidComp = FluidLightingMesh->GetFluidSurfaceComponent();
		if (FluidComp)
		{
		    UMaterialInterface* Material = FluidComp->FluidMaterial;
			if (Material == NULL)
			{
				Material = GEngine->DefaultMaterial;
			}
			Lightmass::FMaterialElementData NewElementData;
			Copy(Material->LightingGuid, NewElementData.MaterialId);
			NewElementData.bUseTwoSidedLighting = FluidComp->LightmassSettings.bUseTwoSidedLighting;
			NewElementData.bShadowIndirectOnly = FluidComp->LightmassSettings.bShadowIndirectOnly;
			NewElementData.bUseEmissiveForStaticLighting = FluidComp->LightmassSettings.bUseEmissiveForStaticLighting;
			// Combine primitive and level boost settings so we don't have to send the level settings over to Lightmass  
			Copy(FluidComp->LightmassSettings.EmissiveLightFalloffExponent, NewElementData.EmissiveLightFalloffExponent);
			Copy(FluidComp->LightmassSettings.EmissiveLightExplicitInfluenceRadius, NewElementData.EmissiveLightExplicitInfluenceRadius);
			Copy(FluidComp->GetEmissiveBoost(0) * LevelSettings.EmissiveBoost, NewElementData.EmissiveBoost);
			Copy(FluidComp->GetDiffuseBoost(0) * LevelSettings.DiffuseBoost, NewElementData.DiffuseBoost);
			Copy(FluidComp->GetSpecularBoost(0) * LevelSettings.SpecularBoost, NewElementData.SpecularBoost);
			Copy(FluidComp->LightmassSettings.FullyOccludedSamplesFraction, NewElementData.FullyOccludedSamplesFraction);
			MaterialElementData.AddItem(NewElementData);
		}

		WriteBaseMeshInstanceData(Channel, INDEX_NONE, FluidLightingMesh, MaterialElementData);

		Lightmass::FFluidSurfaceStaticLightingMeshData FluidInstanceMeshData;
		appMemzero(&FluidInstanceMeshData,sizeof(FluidInstanceMeshData));
		Copy(FluidComp->LocalToWorld, FluidInstanceMeshData.LocalToWorld);
		Copy(FluidComp->LocalToWorld.Inverse().Transpose(), FluidInstanceMeshData.LocalToWorldInverseTranspose);
		for (INT Idx = 0; Idx < 4; Idx++)
		{
			Copy(FluidLightingMesh->QuadCorners[Idx], FluidInstanceMeshData.QuadCorners[Idx]);
			Copy(FluidLightingMesh->QuadUVCorners[Idx], FluidInstanceMeshData.QuadUVCorners[Idx]);
		}
		for (INT Idx = 0; Idx < 6; Idx++)
		{
			FluidInstanceMeshData.QuadIndices[Idx] = FluidLightingMesh->QuadIndices[Idx];
		}
		Swarm.WriteChannel( Channel, &FluidInstanceMeshData, sizeof(FluidInstanceMeshData) );
		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}
}

void FLightmassExporter::WriteLandscapeInstances( INT Channel )
{
	// landscape instance meshes
	for (INT LandscapeIdx = 0; LandscapeIdx < LandscapeLightingMeshes.Num(); LandscapeIdx++)
	{
		const FLandscapeStaticLightingMesh* LandscapeLightingMesh = LandscapeLightingMeshes(LandscapeIdx);

		// Collect the material guids for each element
		TArray<Lightmass::FMaterialElementData> MaterialElementData;
		const ULandscapeComponent* LandscapeComp = LandscapeLightingMesh->LandscapeComponent;
		if (LandscapeComp && LandscapeComp->GetLandscapeProxy())
		{
			UMaterialInterface* Material = LandscapeComp->MaterialInstance;
			if (Material == NULL)
			{
				Material = GEngine->DefaultMaterial;
			}
			Lightmass::FMaterialElementData NewElementData;
			Copy(Material->LightingGuid, NewElementData.MaterialId);
			FLightmassPrimitiveSettings& LMSetting = LandscapeComp->GetLandscapeProxy()->LightmassSettings;
			NewElementData.bUseTwoSidedLighting = LMSetting.bUseTwoSidedLighting;
			NewElementData.bShadowIndirectOnly = LMSetting.bShadowIndirectOnly;
			NewElementData.bUseEmissiveForStaticLighting = LMSetting.bUseEmissiveForStaticLighting;
			// Combine primitive and level boost settings so we don't have to send the level settings over to Lightmass  
			Copy(LMSetting.EmissiveLightFalloffExponent, NewElementData.EmissiveLightFalloffExponent);
			Copy(LMSetting.EmissiveLightExplicitInfluenceRadius, NewElementData.EmissiveLightExplicitInfluenceRadius);
			Copy(LandscapeComp->GetEmissiveBoost(0) * LevelSettings.EmissiveBoost, NewElementData.EmissiveBoost);
			Copy(LandscapeComp->GetDiffuseBoost(0) * LevelSettings.DiffuseBoost, NewElementData.DiffuseBoost);
			Copy(LandscapeComp->GetSpecularBoost(0) * LevelSettings.SpecularBoost, NewElementData.SpecularBoost);
			Copy(LMSetting.FullyOccludedSamplesFraction, NewElementData.FullyOccludedSamplesFraction);
			MaterialElementData.AddItem(NewElementData);
		}

		WriteBaseMeshInstanceData(Channel, INDEX_NONE, LandscapeLightingMesh, MaterialElementData);

		Lightmass::FLandscapeStaticLightingMeshData LandscapeInstanceMeshData;
		appMemzero(&LandscapeInstanceMeshData,sizeof(LandscapeInstanceMeshData));
		Copy(LandscapeComp->LocalToWorld, LandscapeInstanceMeshData.LocalToWorld);
		Copy(LandscapeComp->LocalToWorld.Inverse().Transpose(), LandscapeInstanceMeshData.LocalToWorldInverseTranspose);
		LandscapeInstanceMeshData.SectionBaseX = LandscapeComp->SectionBaseX;
		LandscapeInstanceMeshData.SectionBaseY = LandscapeComp->SectionBaseY;
		LandscapeInstanceMeshData.HeightmapStride = LandscapeComp->HeightmapTexture->SizeX;
		LandscapeInstanceMeshData.HeightmapComponentOffsetX = appRound( (FLOAT)LandscapeComp->HeightmapTexture->SizeX * LandscapeComp->HeightmapScaleBias.Z );
		LandscapeInstanceMeshData.HeightmapComponentOffsetY = appRound( (FLOAT)LandscapeComp->HeightmapTexture->SizeY * LandscapeComp->HeightmapScaleBias.W );
		LandscapeInstanceMeshData.HeightmapSubsectionOffset = LandscapeComp->SubsectionSizeQuads + 1;
		LandscapeInstanceMeshData.ComponentSizeQuads = LandscapeComp->ComponentSizeQuads;
		LandscapeInstanceMeshData.SubsectionSizeQuads = LandscapeComp->SubsectionSizeQuads;
		LandscapeInstanceMeshData.SubsectionSizeVerts = LandscapeComp->SubsectionSizeQuads + 1;
		LandscapeInstanceMeshData.StaticLightingResolution = LandscapeComp->GetLandscapeProxy()->StaticLightingResolution;
		INT DesiredSize = 1;
		LandscapeInstanceMeshData.LightMapRatio = 
			::GetTerrainExpandPatchCount(LandscapeInstanceMeshData.StaticLightingResolution, LandscapeInstanceMeshData.ExpandQuadsX, LandscapeInstanceMeshData.ExpandQuadsY, 
			LandscapeComp->ComponentSizeQuads, (LandscapeComp->NumSubsections * (LandscapeComp->SubsectionSizeQuads+1)), DesiredSize);
		LandscapeInstanceMeshData.SizeX = DesiredSize; //LandscapeLightingMesh->DataInterface->SizeX;
		LandscapeInstanceMeshData.SizeY = DesiredSize; //LandscapeLightingMesh->DataInterface->SizeY;
		//LandscapeInstanceMeshData.ExpandQuadsX = LandscapeLightingMesh->DataInterface->ExpandQuadsX;
		//LandscapeInstanceMeshData.ExpandQuadsY = LandscapeLightingMesh->DataInterface->ExpandQuadsY;

		Swarm.WriteChannel( Channel, &LandscapeInstanceMeshData, sizeof(LandscapeInstanceMeshData) );

		//TArray<FColor> HeightData;
		//LandscapeLightingMesh->DataInterface->GetRawHeightmapData(HeightData);
		//LandscapeLightingMesh->DataInterface->UnlockRawHeightData();
		// write height map data
		BYTE* OutData;
		INT OutSize;

		OutSize = LandscapeLightingMesh->HeightData.Num() * sizeof(FColor);  
		if (OutSize > 0)
		{
			OutData = (BYTE*)(LandscapeLightingMesh->HeightData.GetData());
			Swarm.WriteChannel(Channel, OutData, OutSize);
		}

		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}
}

#if WITH_SPEEDTREE
void FLightmassExporter::WriteSpeedTreeMeshInstances( INT Channel )
{
	// speedtree instance meshes
	for (INT SpeedTreeIdx = 0; SpeedTreeIdx < SpeedTreeLightingMeshes.Num(); SpeedTreeIdx++)
	{
		const FSpeedTreeStaticLightingMesh* SpeedTreeLightingMesh = SpeedTreeLightingMeshes(SpeedTreeIdx);
		TArray<Lightmass::FMaterialElementData> MaterialElementData;

		USpeedTreeComponent* SpeedTreeComponent = SpeedTreeLightingMesh->GetComponentStaticLighting()->GetComponent();
		if (SpeedTreeComponent)
		{
			// Collect each material...
			for (BYTE MeshTypeIdx = STMT_MinMinusOne + 1; MeshTypeIdx < STMT_Max; MeshTypeIdx++)
			{
				UMaterialInterface* Material = SpeedTreeComponent->GetMaterial(MeshTypeIdx);

				if (Material == NULL)
				{
					Material = GEngine->DefaultMaterial;
				}

				Lightmass::FMaterialElementData NewElementData;
				Copy(Material->LightingGuid, NewElementData.MaterialId);
				NewElementData.bUseTwoSidedLighting = SpeedTreeComponent->LightmassSettings.bUseTwoSidedLighting;
				NewElementData.bShadowIndirectOnly = SpeedTreeComponent->LightmassSettings.bShadowIndirectOnly;
				NewElementData.bUseEmissiveForStaticLighting = SpeedTreeComponent->LightmassSettings.bUseEmissiveForStaticLighting;
				// Combine primitive and level boost settings so we don't have to send the level settings over to Lightmass  
				Copy(SpeedTreeComponent->LightmassSettings.EmissiveBoost * LevelSettings.EmissiveBoost, NewElementData.EmissiveBoost);
				Copy(SpeedTreeComponent->LightmassSettings.DiffuseBoost * LevelSettings.DiffuseBoost, NewElementData.DiffuseBoost);
				Copy(SpeedTreeComponent->LightmassSettings.SpecularBoost * LevelSettings.SpecularBoost, NewElementData.SpecularBoost);
				Copy(SpeedTreeComponent->LightmassSettings.FullyOccludedSamplesFraction, NewElementData.FullyOccludedSamplesFraction);
				MaterialElementData.AddItem(NewElementData);
			}
		}

		WriteBaseMeshInstanceData(Channel, INDEX_NONE, SpeedTreeLightingMesh, MaterialElementData);
		
		// get out the component mirror object
		TRefCountPtr<FSpeedTreeComponentStaticLighting> LightingComponent = SpeedTreeLightingMesh->GetComponentStaticLighting();
		USpeedTreeComponent* Component = LightingComponent->GetComponent();

		Lightmass::FSpeedTreeStaticLightingMeshData SpeedTreeSLMData;
		appMemzero(&SpeedTreeSLMData, sizeof(SpeedTreeSLMData));
		Copy(Component->SpeedTree->LightingGuid, SpeedTreeSLMData.InstanceSpeedTreeGuid);
		SpeedTreeSLMData.LODIndex = SpeedTreeLightingMesh->GetLODIndex();
		SpeedTreeSLMData.MeshType = (Lightmass::ESpeedTreeMeshType)SpeedTreeLightingMesh->GetMeshType();
		SpeedTreeSLMData.ElementFirstIndex = SpeedTreeLightingMesh->GetMeshElement().FirstIndex;
		SpeedTreeSLMData.ElementMinVertexIndex = SpeedTreeLightingMesh->GetMeshElement().MinVertexIndex;
		Copy(LightingComponent->GetComponentGuid(), SpeedTreeSLMData.ComponentGuid);

		// Compute the combined local to world transform, including rotation.
		const FMatrix LocalToWorld = Component->RotationOnlyMatrix.Inverse() * Component->LocalToWorld;
		Copy(LocalToWorld, SpeedTreeSLMData.LocalToWorld);

		// write the instance out
		Swarm.WriteChannel(Channel, &SpeedTreeSLMData, sizeof(SpeedTreeSLMData));

		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}
}
#endif	//#if WITH_SPEEDTREE

struct FLightmassMaterialPair
{
	FLightmassMaterialPair(INT InLightmassSettingsIndex, UMaterialInterface* InMaterial)
		: LightmassSettingsIndex(InLightmassSettingsIndex)
		, Material(InMaterial)
	{
	}

	/** Index into the Model's LightmassSettings array for this triangle */
	INT LightmassSettingsIndex;

	/** Materials used by this triangle */
	UMaterialInterface* Material;

	UBOOL operator==( const FLightmassMaterialPair& Other ) const
	{
		return LightmassSettingsIndex == Other.LightmassSettingsIndex && Material == Other.Material;
	}
};


void FLightmassExporter::WriteMappings( INT Channel )
{
	// bsp mappings
	for( INT MappingIdx=0; MappingIdx < BSPSurfaceMappings.Num() && !GEditor->GetMapBuildCancelled(); MappingIdx++ )
	{
		const FBSPSurfaceStaticLighting* BSPMapping = BSPSurfaceMappings(MappingIdx);

		TArray<Lightmass::FMaterialElementData> MaterialElementData;
		const UModel* Model = BSPMapping->GetModel();
		check(Model);

		// make a list of the used lightmass settings by this NodeGroup and a mapping from
		// each triangle into this array
		TArray<FLightmassMaterialPair> LocalLightmassSettings;
		TArray<INT> LocalPerTriangleLightmassSettings;

		// go through each triangle, looking for unique settings, and remapping each triangle 
		INT NumTriangles = BSPMapping->NodeGroup->TriangleSurfaceMap.Num();
		LocalPerTriangleLightmassSettings.Empty(NumTriangles);
		LocalPerTriangleLightmassSettings.Add(NumTriangles);
		for (INT TriangleIndex = 0; TriangleIndex < NumTriangles; TriangleIndex++)
		{
			const FBspSurf& Surf = Model->Surfs(BSPMapping->NodeGroup->TriangleSurfaceMap(TriangleIndex));
			LocalPerTriangleLightmassSettings(TriangleIndex) = 
				LocalLightmassSettings.AddUniqueItem(FLightmassMaterialPair(Surf.iLightmassIndex, Surf.Material));
		}

		// now for each used setting, export it
		for (INT SettingIndex = 0; SettingIndex < LocalLightmassSettings.Num(); SettingIndex++)
		{
			const FLightmassMaterialPair& Pair = LocalLightmassSettings(SettingIndex);
			UMaterialInterface* Material = Pair.Material;
			if (Material == NULL)
			{
				Material = GEngine->DefaultMaterial;
			}
			check(Material);

			// get the settings from the model
			const FLightmassPrimitiveSettings& PrimitiveSettings = Model->LightmassSettings(Pair.LightmassSettingsIndex);

			Lightmass::FMaterialElementData TempData;
			Copy(Material->LightingGuid, TempData.MaterialId);
			TempData.bUseTwoSidedLighting = PrimitiveSettings.bUseTwoSidedLighting;
			TempData.bShadowIndirectOnly = PrimitiveSettings.bShadowIndirectOnly;
			TempData.bUseEmissiveForStaticLighting = PrimitiveSettings.bUseEmissiveForStaticLighting;
			Copy(PrimitiveSettings.EmissiveLightFalloffExponent, TempData.EmissiveLightFalloffExponent);
			Copy(PrimitiveSettings.EmissiveLightExplicitInfluenceRadius, TempData.EmissiveLightExplicitInfluenceRadius);
			Copy(PrimitiveSettings.EmissiveBoost * LevelSettings.EmissiveBoost, TempData.EmissiveBoost);
			Copy(PrimitiveSettings.DiffuseBoost * LevelSettings.DiffuseBoost, TempData.DiffuseBoost);
			Copy(PrimitiveSettings.SpecularBoost * LevelSettings.SpecularBoost, TempData.SpecularBoost);
			Copy(PrimitiveSettings.FullyOccludedSamplesFraction, TempData.FullyOccludedSamplesFraction);
			MaterialElementData.AddItem(TempData);
		}
		
		WriteBaseMeshInstanceData(Channel, INDEX_NONE, BSPMapping, MaterialElementData);
		WriteBaseTextureMappingData( Channel, BSPMapping );
		
		Lightmass::FBSPSurfaceStaticLightingData BSPSurfaceMappingData;
		Copy( BSPMapping->NodeGroup->TangentX, BSPSurfaceMappingData.TangentX );
		Copy( BSPMapping->NodeGroup->TangentY, BSPSurfaceMappingData.TangentY );
		Copy( BSPMapping->NodeGroup->TangentZ, BSPSurfaceMappingData.TangentZ );
		Copy( BSPMapping->NodeGroup->MapToWorld, BSPSurfaceMappingData.MapToWorld );
		Copy( BSPMapping->NodeGroup->WorldToMap, BSPSurfaceMappingData.WorldToMap );
		
		Swarm.WriteChannel( Channel, &BSPSurfaceMappingData, sizeof(BSPSurfaceMappingData) );
	
		const DWORD VertexDataSize = BSPMapping->NodeGroup->Vertices.Num() * sizeof(Lightmass::FStaticLightingVertexData);
		if( VertexDataSize > 0 )
		{
			Lightmass::FStaticLightingVertexData* VertexData = (Lightmass::FStaticLightingVertexData*)appMalloc(VertexDataSize);
			for( INT VertIdx=0; VertIdx < BSPMapping->NodeGroup->Vertices.Num(); VertIdx++ )
			{
				const FStaticLightingVertex& SrcVertex = BSPMapping->NodeGroup->Vertices(VertIdx);
				Lightmass::FStaticLightingVertexData& DstVertex = VertexData[VertIdx];

				Copy( SrcVertex.WorldPosition, DstVertex.WorldPosition );
				Copy( SrcVertex.WorldTangentX, DstVertex.WorldTangentX );
				Copy( SrcVertex.WorldTangentY, DstVertex.WorldTangentY );
				Copy( SrcVertex.WorldTangentZ, DstVertex.WorldTangentZ );			
				for( INT CoordIdx=0; CoordIdx < Lightmass::MAX_TEXCOORDS; CoordIdx++ )
				{	
					Copy( SrcVertex.TextureCoordinates[CoordIdx], DstVertex.TextureCoordinates[CoordIdx] );
				}
			}
			Swarm.WriteChannel( Channel, VertexData, VertexDataSize );
			appFree( VertexData );
		}
		if( BSPMapping->NodeGroup->TriangleVertexIndices.Num() > 0 )
		{
			Swarm.WriteChannel( Channel, (void*)BSPMapping->NodeGroup->TriangleVertexIndices.GetData(), BSPMapping->NodeGroup->TriangleVertexIndices.Num() * sizeof(Lightmass::INT) );
		}

		Swarm.WriteChannel(Channel, LocalPerTriangleLightmassSettings.GetData(), LocalPerTriangleLightmassSettings.Num() * sizeof(Lightmass::INT));
		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}	
	
	// static mesh texture mappings
	for( INT MappingIdx=0; MappingIdx < StaticMeshTextureMappings.Num(); MappingIdx++ )
	{
		const FStaticMeshStaticLightingTextureMapping* SMTextureMapping = StaticMeshTextureMappings(MappingIdx);
		WriteBaseTextureMappingData( Channel, SMTextureMapping );
		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}	
	
	// static mesh vertex mappings
	for( INT MappingIdx=0; MappingIdx < StaticMeshVertexMappings.Num(); MappingIdx++ )
	{
		const FStaticMeshStaticLightingVertexMapping* SMVertexMapping = StaticMeshVertexMappings(MappingIdx);
		WriteBaseVertexMappingData( Channel, SMVertexMapping );
		GWarn->UpdateProgress( CurrentProgress++, TotalProgress );
	}

	// terrain mappings
	for (INT MappingIdx = 0; MappingIdx < TerrainMappings.Num(); MappingIdx++)
	{
		const FTerrainComponentStaticLighting* TerrainMapping = TerrainMappings(MappingIdx);
		WriteTerrainMapping(Channel, TerrainMapping);
		GWarn->UpdateProgress(CurrentProgress++, TotalProgress);
	}

	// fluid surface mappings
	for (INT MappingIdx = 0; MappingIdx < FluidSurfaceTextureMappings.Num(); MappingIdx++)
	{
		const FFluidSurfaceStaticLightingTextureMapping* FluidMapping = FluidSurfaceTextureMappings(MappingIdx);
		WriteFluidMapping(Channel, FluidMapping);
		GWarn->UpdateProgress(CurrentProgress++, TotalProgress);
	}

	// landscape surface mappings
	for (INT MappingIdx = 0; MappingIdx < LandscapeTextureMappings.Num(); MappingIdx++)
	{
		const FLandscapeStaticLightingTextureMapping* LandscapeMapping = LandscapeTextureMappings(MappingIdx);
		WriteLandscapeMapping(Channel, LandscapeMapping);
		GWarn->UpdateProgress(CurrentProgress++, TotalProgress);
	}

#if WITH_SPEEDTREE
	// speedtree volume mappings
	for (INT MappingIdx = 0; MappingIdx < SpeedTreeMappings.Num(); MappingIdx++)
	{
		const FSpeedTreeStaticLightingMapping* SpeedTreeMapping = SpeedTreeMappings(MappingIdx);
		WriteSpeedTreeMapping(Channel, SpeedTreeMapping);
		GWarn->UpdateProgress(CurrentProgress++, TotalProgress);
	}
#endif	//#if WITH_SPEEDTREE
}

/** Finds the GUID of the mapping that is being debugged. */
UBOOL FLightmassExporter::FindDebugMapping(FGuid& DebugMappingGuid)
{
	const FStaticLightingMapping* FoundDebugMapping = NULL;
	// Only BSP texture, static mesh vertex and texture and fluid texture lightmaps supported for now.
	for( INT MappingIdx=0; MappingIdx < BSPSurfaceMappings.Num(); MappingIdx++ )
	{
		const FBSPSurfaceStaticLighting* BSPMapping = BSPSurfaceMappings(MappingIdx);
		if (BSPMapping->DebugThisMapping())
		{
			// Only one mapping should be setup for debugging
			check(!FoundDebugMapping);
			FoundDebugMapping = BSPMapping;
		}
	}

	for( INT MappingIdx=0; MappingIdx < StaticMeshTextureMappings.Num(); MappingIdx++ )
	{
		const FStaticMeshStaticLightingTextureMapping* SMTextureMapping = StaticMeshTextureMappings(MappingIdx);
		if (SMTextureMapping->DebugThisMapping())
		{
			// Only one mapping should be setup for debugging
			check(!FoundDebugMapping);
			FoundDebugMapping = SMTextureMapping;
		}
	}	

	for( INT MappingIdx=0; MappingIdx < StaticMeshVertexMappings.Num(); MappingIdx++ )
	{
		const FStaticMeshStaticLightingVertexMapping* SMVertexMapping = StaticMeshVertexMappings(MappingIdx);
		if (SMVertexMapping->DebugThisMapping())
		{
			// Only one mapping should be setup for debugging
			check(!FoundDebugMapping);
			FoundDebugMapping = SMVertexMapping;
		}
	}	

	for( INT MappingIdx=0; MappingIdx < FluidSurfaceTextureMappings.Num(); MappingIdx++ )
	{
		const FFluidSurfaceStaticLightingTextureMapping* FluidMapping = FluidSurfaceTextureMappings(MappingIdx);
		if (FluidMapping->DebugThisMapping())
		{
			// Only one mapping should be setup for debugging
			check(!FoundDebugMapping);
			FoundDebugMapping = FluidMapping;
		}
	}

	if (FoundDebugMapping)
	{
		DebugMappingGuid = FoundDebugMapping->GetLightingGuid();
		return TRUE;
	}
	else 
	{
		return FALSE;
	}
}

static FVector FindDominantLightDirection(const FVector& UserInput)
{
	FVector NormalizedLightDirection;
	if(FVector::ZeroVector != UserInput)
	{
		NormalizedLightDirection = UserInput;
	}
	else if (GWorld->DominantDirectionalLight && !GWorld->DominantDirectionalLight->GetOwner()->bMovable)
	{
		NormalizedLightDirection = GWorld->DominantDirectionalLight->GetDirection();
	}
	else
	{
		warnf(TEXT("FLightmassExporter::WriteSceneSettings: No DominantDirectionalLight was found, EnvironmentLightTerminatorAngleFactor is set to a default one."));
		return FVector(0, 0, -1);
	}
	if(!NormalizedLightDirection.Normalize())
	{
		warnf(TEXT("FLightmassExporter::WriteSceneSettings: Wrong DominantDirectionalLight or EnvironmentLightTerminatorAngleFactor"));
		return FVector(0, 0, -1);
	}
	return NormalizedLightDirection;
}

/** Fills out the Scene's settings, read from the engine ini. */
void FLightmassExporter::WriteSceneSettings( Lightmass::FSceneFileHeader& Scene )
{
	//@todo - need a mechanism to automatically catch when a new setting has been added but doesn't get initialized
	{
		verify(GConfig->GetBool(TEXT("DevOptions.StaticLighting"), TEXT("bAllowMultiThreadedStaticLighting"), Scene.GeneralSettings.bAllowMultiThreadedStaticLighting, GLightmassIni));
		Scene.GeneralSettings.NumUnusedLocalCores = NumUnusedLocalCores;
		Scene.GeneralSettings.NumIndirectLightingBounces = LevelSettings.NumIndirectLightingBounces;
		verify(GConfig->GetInt(TEXT("DevOptions.StaticLighting"), TEXT("ViewSingleBounceNumber"), Scene.GeneralSettings.ViewSingleBounceNumber, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.StaticLighting"), TEXT("bUseConservativeTexelRasterization"), Scene.GeneralSettings.bUseConservativeTexelRasterization, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.StaticLighting"), TEXT("bAccountForTexelSize"), Scene.GeneralSettings.bAccountForTexelSize, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.StaticLighting"), TEXT("bUseMaxWeight"), Scene.GeneralSettings.bUseMaxWeight, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.StaticLighting"), TEXT("MaxTriangleLightingSamples"), Scene.GeneralSettings.MaxTriangleLightingSamples, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.StaticLighting"), TEXT("MaxTriangleIrradiancePhotonCacheSamples"), Scene.GeneralSettings.MaxTriangleIrradiancePhotonCacheSamples, GLightmassIni));

		INT CheckQualityLevel;
		GConfig->GetInt( TEXT("LightingBuildOptions"), TEXT("QualityLevel"), CheckQualityLevel, GEditorUserSettingsIni);
		CheckQualityLevel = Clamp<INT>(CheckQualityLevel, Quality_Preview, Quality_Production);
		debugf(TEXT("LIGHTMASS: Writing scene settings: Quality level %d (%d in INI)"), (INT)(QualityLevel), CheckQualityLevel);
		if (CheckQualityLevel != QualityLevel)
		{
			warnf(NAME_Warning, TEXT("LIGHTMASS: Writing scene settings w/ QualityLevel mismatch! %d vs %d (ini setting)"), (INT)QualityLevel, CheckQualityLevel);
		}

		switch (QualityLevel)
		{
		case Quality_High:
		case Quality_Production:
			Scene.GeneralSettings.bUseErrorColoring = FALSE;
			Scene.GeneralSettings.UnmappedTexelColor = FLinearColor(0.0f, 0.0f, 0.0f);
			break;
		default:
			{
				UBOOL bUseErrorColoring = FALSE;
				GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("UseErrorColoring"),		bUseErrorColoring,					GEditorUserSettingsIni );
				Scene.GeneralSettings.bUseErrorColoring = bUseErrorColoring;
				if (bUseErrorColoring == FALSE)
				{
					Scene.GeneralSettings.UnmappedTexelColor = FLinearColor(0.0f, 0.0f, 0.0f);
				}
				else
				{
					Scene.GeneralSettings.UnmappedTexelColor = FLinearColor(0.7f, 0.7f, 0.0f);
				}
			}
			break;
		}
	}
	{
		FLOAT GlobalLevelScale = 1.0f;
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticLightingSceneConstants"), TEXT("StaticLightingLevelScale"), GlobalLevelScale, GLightmassIni));
		Scene.SceneConstants.StaticLightingLevelScale = GlobalLevelScale * LevelSettings.StaticLightingLevelScale;
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticLightingSceneConstants"), TEXT("VisibilityRayOffsetDistance"), Scene.SceneConstants.VisibilityRayOffsetDistance, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticLightingSceneConstants"), TEXT("VisibilityNormalOffsetDistance"), Scene.SceneConstants.VisibilityNormalOffsetDistance, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticLightingSceneConstants"), TEXT("VisibilityNormalOffsetSampleRadiusScale"), Scene.SceneConstants.VisibilityNormalOffsetSampleRadiusScale, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticLightingSceneConstants"), TEXT("VisibilityTangentOffsetSampleRadiusScale"), Scene.SceneConstants.VisibilityTangentOffsetSampleRadiusScale, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticLightingSceneConstants"), TEXT("SmallestTexelRadius"), Scene.SceneConstants.SmallestTexelRadius, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingSceneConstants"), TEXT("LightGridSize"), Scene.SceneConstants.LightGridSize, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticLightingSceneConstants"), TEXT("DirectionalCoefficientFalloffPower"), Scene.SceneConstants.DirectionalCoefficientFalloffPower, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticLightingSceneConstants"), TEXT("DirectionalCoefficientScale"), Scene.SceneConstants.DirectionalCoefficientScale, GLightmassIni));
		Scene.SceneConstants.IndirectNormalInfluenceBoost = LevelSettings.IndirectNormalInfluenceBoost;

		// Only use a fixed lightmap scale if gamma correction is turned off.  Without gamma correction
		// we can't properly scale lightmaps without severely tinting the hue.
		Scene.SceneConstants.bUseFixedScaleForSimpleLightmaps = !GWorld->GetWorldInfo()->bUseGammaCorrection;
		Scene.SceneConstants.UseFixedScaleValue = 2.0f;
	}
	{
		verify(GConfig->GetBool(TEXT("DevOptions.StaticLightingMaterial"), TEXT("bUseDebugMaterial"), Scene.MaterialSettings.bUseDebugMaterial, GLightmassIni));
		FString ShowMaterialAttributeName;
		verify(GConfig->GetString(TEXT("DevOptions.StaticLightingMaterial"), TEXT("ShowMaterialAttribute"), ShowMaterialAttributeName, GLightmassIni));

		Scene.MaterialSettings.ViewMaterialAttribute = Lightmass::VMA_None;
		if (ShowMaterialAttributeName.InStr(TEXT("Emissive"), FALSE, TRUE) != INDEX_NONE)
		{
			Scene.MaterialSettings.ViewMaterialAttribute = Lightmass::VMA_Emissive;
		}
		else if (ShowMaterialAttributeName.InStr(TEXT("Diffuse"), FALSE, TRUE) != INDEX_NONE
			|| LevelSettings.bVisualizeMaterialDiffuse)
		{
			Scene.MaterialSettings.ViewMaterialAttribute = Lightmass::VMA_Diffuse;
		}
		else if (ShowMaterialAttributeName.InStr(TEXT("SpecularPower"), FALSE, TRUE) != INDEX_NONE)
		{
			Scene.MaterialSettings.ViewMaterialAttribute = Lightmass::VMA_SpecularPower;
		}
		else if (ShowMaterialAttributeName.InStr(TEXT("Specular"), FALSE, TRUE) != INDEX_NONE)
		{
			Scene.MaterialSettings.ViewMaterialAttribute = Lightmass::VMA_Specular;
		}
		else if (ShowMaterialAttributeName.InStr(TEXT("Transmission"), FALSE, TRUE) != INDEX_NONE)
		{
			Scene.MaterialSettings.ViewMaterialAttribute = Lightmass::VMA_Transmission;
		}
		else if (ShowMaterialAttributeName.InStr(TEXT("Normal"), FALSE, TRUE) != INDEX_NONE)
		{
			Scene.MaterialSettings.ViewMaterialAttribute = Lightmass::VMA_Normal;
		}

		verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("EmissiveSampleSize"), Scene.MaterialSettings.EmissiveSize, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("DiffuseSampleSize"), Scene.MaterialSettings.DiffuseSize, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("SpecularSampleSize"), Scene.MaterialSettings.SpecularSize, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("TransmissionSampleSize"), Scene.MaterialSettings.TransmissionSize, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("NormalSampleSize"), Scene.MaterialSettings.NormalSize, GLightmassIni));

		const FString DiffuseStr = GConfig->GetStr(TEXT("DevOptions.StaticLightingMaterial"), TEXT("DebugDiffuse"), GLightmassIni);
		verify(Parse(*DiffuseStr, TEXT("R="), Scene.MaterialSettings.DebugDiffuse.R));
		verify(Parse(*DiffuseStr, TEXT("G="), Scene.MaterialSettings.DebugDiffuse.G));
		verify(Parse(*DiffuseStr, TEXT("B="), Scene.MaterialSettings.DebugDiffuse.B));

		const FString SpecularStr = GConfig->GetStr(TEXT("DevOptions.StaticLightingMaterial"), TEXT("DebugSpecular"), GLightmassIni);
		verify(Parse(*SpecularStr, TEXT("R="), Scene.MaterialSettings.DebugSpecular.R));
		verify(Parse(*SpecularStr, TEXT("G="), Scene.MaterialSettings.DebugSpecular.G));
		verify(Parse(*SpecularStr, TEXT("B="), Scene.MaterialSettings.DebugSpecular.B));

		verify(GConfig->GetFloat(TEXT("DevOptions.StaticLightingMaterial"), TEXT("DebugSpecularPower"), Scene.MaterialSettings.DebugSpecularPower, GLightmassIni));
		{
			Scene.MaterialSettings.EnvironmentColor = FLinearColor(LevelSettings.EnvironmentColor) * LevelSettings.EnvironmentIntensity;

			Scene.MaterialSettings.EnvironmentSunColor = FLinearColor(LevelSettings.EnvironmentSunColor) * LevelSettings.EnvironmentSunIntensity;

			Scene.MaterialSettings.bEnableAdvancedEnvironmentColor = LevelSettings.bEnableAdvancedEnvironmentColor;

			{
				//MATH MODEL COPIED FROM ExponentialFog (UnrealLightMass FStaticLightingSystem::EvaluateEnvironmentLighting)
				const FLOAT CosTerminatorAngle = Clamp(appCos(LevelSettings.EnvironmentLightTerminatorAngle * PI / 180.0f), -1.0f + DELTA, 1.0f - DELTA);
				// bring -1..1 into range 1..0
				const FLOAT NormalizedAngle = 0.5f - 0.5f * CosTerminatorAngle;
				// LogHalf = log(0.5f)
				const FLOAT LogHalf = -0.30103f;
				// precompute a constant
				const FLOAT PowFactor = LogHalf / appLoge(NormalizedAngle);
				Scene.MaterialSettings.EnvironmentLightTerminatorAngleFactor = PowFactor;
			}

			FVector SunDirection = FindDominantLightDirection(LevelSettings.EnvironmentLightDirection);
			Scene.MaterialSettings.EnvironmentLightDirection.X = SunDirection.X;
			Scene.MaterialSettings.EnvironmentLightDirection.Y = SunDirection.Y;
			Scene.MaterialSettings.EnvironmentLightDirection.Z = SunDirection.Z;
			Scene.MaterialSettings.EnvironmentLightDirection.W = 1.0f;
		}
		Scene.MaterialSettings.bUseNormalMapsForSimpleLightMaps = GEngine->bUseNormalMapsForSimpleLightMaps;
	}
	{
		verify(GConfig->GetBool(TEXT("DevOptions.MeshAreaLights"), TEXT("bVisualizeMeshAreaLightPrimitives"), Scene.MeshAreaLightSettings.bVisualizeMeshAreaLightPrimitives, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.MeshAreaLights"), TEXT("EmissiveIntensityThreshold"), Scene.MeshAreaLightSettings.EmissiveIntensityThreshold, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.MeshAreaLights"), TEXT("MeshAreaLightGridSize"), Scene.MeshAreaLightSettings.MeshAreaLightGridSize, GLightmassIni));
		FLOAT MeshAreaLightSimplifyNormalAngleThreshold;
		verify(GConfig->GetFloat(TEXT("DevOptions.MeshAreaLights"), TEXT("MeshAreaLightSimplifyNormalAngleThreshold"), MeshAreaLightSimplifyNormalAngleThreshold, GLightmassIni));
		Scene.MeshAreaLightSettings.MeshAreaLightSimplifyNormalCosAngleThreshold = appCos(Clamp(MeshAreaLightSimplifyNormalAngleThreshold, 0.0f, 90.0f) * (FLOAT)PI / 180.0f);
		verify(GConfig->GetFloat(TEXT("DevOptions.MeshAreaLights"), TEXT("MeshAreaLightSimplifyCornerDistanceThreshold"), Scene.MeshAreaLightSettings.MeshAreaLightSimplifyCornerDistanceThreshold, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.MeshAreaLights"), TEXT("MeshAreaLightSimplifyMeshBoundingRadiusFractionThreshold"), Scene.MeshAreaLightSettings.MeshAreaLightSimplifyMeshBoundingRadiusFractionThreshold, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.MeshAreaLights"), TEXT("MeshAreaLightGeneratedDynamicLightSurfaceOffset"), Scene.MeshAreaLightSettings.MeshAreaLightGeneratedDynamicLightSurfaceOffset, GLightmassIni));
	}
	{
		Scene.AmbientOcclusionSettings.bUseAmbientOcclusion = LevelSettings.bUseAmbientOcclusion;
		Scene.AmbientOcclusionSettings.bVisualizeAmbientOcclusion = LevelSettings.bVisualizeAmbientOcclusion;
		Scene.AmbientOcclusionSettings.DirectIlluminationOcclusionFraction = LevelSettings.DirectIlluminationOcclusionFraction;
		Scene.AmbientOcclusionSettings.IndirectIlluminationOcclusionFraction = LevelSettings.IndirectIlluminationOcclusionFraction;
		Scene.AmbientOcclusionSettings.OcclusionExponent = LevelSettings.OcclusionExponent;
		Scene.AmbientOcclusionSettings.FullyOccludedSamplesFraction = LevelSettings.FullyOccludedSamplesFraction;
		Scene.AmbientOcclusionSettings.MaxOcclusionDistance = LevelSettings.MaxOcclusionDistance;
	}
	{
		verify(GConfig->GetBool(TEXT("DevOptions.PrecomputedDynamicObjectLighting"), TEXT("bVisualizeVolumeLightSamples"), Scene.DynamicObjectSettings.bVisualizeVolumeLightSamples, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.PrecomputedDynamicObjectLighting"), TEXT("bVisualizeVolumeLightInterpolation"), Scene.DynamicObjectSettings.bVisualizeVolumeLightInterpolation, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PrecomputedDynamicObjectLighting"), TEXT("SurfaceLightSampleSpacing"), Scene.DynamicObjectSettings.SurfaceLightSampleSpacing, GLightmassIni));
		
		verify(GConfig->GetFloat(TEXT("DevOptions.PrecomputedDynamicObjectLighting"), TEXT("FirstSurfaceSampleLayerHeight"), Scene.DynamicObjectSettings.FirstSurfaceSampleLayerHeight, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PrecomputedDynamicObjectLighting"), TEXT("SurfaceSampleLayerHeightSpacing"), Scene.DynamicObjectSettings.SurfaceSampleLayerHeightSpacing, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.PrecomputedDynamicObjectLighting"), TEXT("NumSurfaceSampleLayers"), Scene.DynamicObjectSettings.NumSurfaceSampleLayers, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PrecomputedDynamicObjectLighting"), TEXT("DetailVolumeSampleSpacing"), Scene.DynamicObjectSettings.DetailVolumeSampleSpacing, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PrecomputedDynamicObjectLighting"), TEXT("VolumeLightSampleSpacing"), Scene.DynamicObjectSettings.VolumeLightSampleSpacing, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.PrecomputedDynamicObjectLighting"), TEXT("MaxVolumeSamples"), Scene.DynamicObjectSettings.MaxVolumeSamples, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.PrecomputedDynamicObjectLighting"), TEXT("bUseMaxSurfaceSampleNum"), Scene.DynamicObjectSettings.bUseMaxSurfaceSampleNum, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.PrecomputedDynamicObjectLighting"), TEXT("MaxSurfaceLightSamples"), Scene.DynamicObjectSettings.MaxSurfaceLightSamples, GLightmassIni));
	}
	{
		verify(GConfig->GetBool(TEXT("DevOptions.PrecomputedVisibility"), TEXT("bVisualizePrecomputedVisibility"), Scene.PrecomputedVisibilitySettings.bVisualizePrecomputedVisibility, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.PrecomputedVisibility"), TEXT("bPlaceCellsOnOpaqueOnly"), Scene.PrecomputedVisibilitySettings.bPlaceCellsOnOpaqueOnly, GLightmassIni));
		Scene.PrecomputedVisibilitySettings.bPlaceCellsOnSurfaces = GWorld->GetWorldInfo()->bPlaceCellsOnSurfaces;
		Scene.PrecomputedVisibilitySettings.CellSize = GWorld->GetWorldInfo()->VisibilityCellSize;
		verify(GConfig->GetInt(TEXT("DevOptions.PrecomputedVisibility"), TEXT("NumCellDistributionBuckets"), Scene.PrecomputedVisibilitySettings.NumCellDistributionBuckets, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PrecomputedVisibility"), TEXT("PlayAreaHeight"), Scene.PrecomputedVisibilitySettings.PlayAreaHeight, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PrecomputedVisibility"), TEXT("MeshBoundsScale"), Scene.PrecomputedVisibilitySettings.MeshBoundsScale, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.PrecomputedVisibility"), TEXT("MinMeshSamples"), Scene.PrecomputedVisibilitySettings.MinMeshSamples, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.PrecomputedVisibility"), TEXT("MaxMeshSamples"), Scene.PrecomputedVisibilitySettings.MaxMeshSamples, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.PrecomputedVisibility"), TEXT("NumCellSamples"), Scene.PrecomputedVisibilitySettings.NumCellSamples, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.PrecomputedVisibility"), TEXT("NumImportanceSamples"), Scene.PrecomputedVisibilitySettings.NumImportanceSamples, GLightmassIni));
	}
	if (GWorld->GetWorldInfo()->VisibilityAggressiveness != VIS_LeastAggressive)
	{
		const TCHAR* AggressivenessSectionNames[VIS_Max] = {
			TEXT(""), 
			TEXT("DevOptions.PrecomputedVisibilityModeratelyAggressive"), 
			TEXT("DevOptions.PrecomputedVisibilityMostAggressive")};
		const TCHAR* ActiveSection = AggressivenessSectionNames[GWorld->GetWorldInfo()->VisibilityAggressiveness];
		verify(GConfig->GetFloat(ActiveSection, TEXT("MeshBoundsScale"), Scene.PrecomputedVisibilitySettings.MeshBoundsScale, GLightmassIni));
	}
	{
		verify(GConfig->GetFloat(TEXT("DevOptions.VolumeDistanceField"), TEXT("VoxelSize"), Scene.VolumeDistanceFieldSettings.VoxelSize, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.VolumeDistanceField"), TEXT("VolumeMaxDistance"), Scene.VolumeDistanceFieldSettings.VolumeMaxDistance, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.VolumeDistanceField"), TEXT("NumVoxelDistanceSamples"), Scene.VolumeDistanceFieldSettings.NumVoxelDistanceSamples, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.VolumeDistanceField"), TEXT("MaxVoxels"), Scene.VolumeDistanceFieldSettings.MaxVoxels, GLightmassIni));
	}
	{
		verify(GConfig->GetBool(TEXT("DevOptions.StaticShadows"), TEXT("bUseZeroAreaLightmapSpaceFilteredLights"), Scene.ShadowSettings.bUseZeroAreaLightmapSpaceFilteredLights, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.StaticShadows"), TEXT("NumShadowRays"), Scene.ShadowSettings.NumShadowRays, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.StaticShadows"), TEXT("NumPenumbraShadowRays"), Scene.ShadowSettings.NumPenumbraShadowRays, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.StaticShadows"), TEXT("NumBounceShadowRays"), Scene.ShadowSettings.NumBounceShadowRays, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.StaticShadows"), TEXT("bFilterShadowFactor"), Scene.ShadowSettings.bFilterShadowFactor, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticShadows"), TEXT("ShadowFactorGradientTolerance"), Scene.ShadowSettings.ShadowFactorGradientTolerance, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.StaticShadows"), TEXT("bAllowSignedDistanceFieldShadows"), Scene.ShadowSettings.bAllowSignedDistanceFieldShadows, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticShadows"), TEXT("MaxTransitionDistanceWorldSpace"), Scene.ShadowSettings.MaxTransitionDistanceWorldSpace, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.StaticShadows"), TEXT("ApproximateHighResTexelsPerMaxTransitionDistance"), Scene.ShadowSettings.ApproximateHighResTexelsPerMaxTransitionDistance, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.StaticShadows"), TEXT("MinDistanceFieldUpsampleFactor"), Scene.ShadowSettings.MinDistanceFieldUpsampleFactor, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticShadows"), TEXT("DominantShadowTransitionSampleDistanceX"), Scene.ShadowSettings.DominantShadowTransitionSampleDistanceX, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticShadows"), TEXT("DominantShadowTransitionSampleDistanceY"), Scene.ShadowSettings.DominantShadowTransitionSampleDistanceY, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.StaticShadows"), TEXT("DominantShadowSuperSampleFactor"), Scene.ShadowSettings.DominantShadowSuperSampleFactor, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.StaticShadows"), TEXT("DominantShadowMaxSamples"), Scene.ShadowSettings.DominantShadowMaxSamples, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticShadows"), TEXT("MinUnoccludedFraction"), Scene.ShadowSettings.MinUnoccludedFraction, GLightmassIni));
	}
	{
		verify(GConfig->GetBool(TEXT("DevOptions.ImportanceTracing"), TEXT("bUsePathTracer"), Scene.ImportanceTracingSettings.bUsePathTracer, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.ImportanceTracing"), TEXT("bUseCosinePDF"), Scene.ImportanceTracingSettings.bUseCosinePDF, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.ImportanceTracing"), TEXT("bUseStratifiedSampling"), Scene.ImportanceTracingSettings.bUseStratifiedSampling, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.ImportanceTracing"), TEXT("NumPaths"), Scene.ImportanceTracingSettings.NumPaths, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.ImportanceTracing"), TEXT("NumHemisphereSamples"), Scene.ImportanceTracingSettings.NumHemisphereSamples, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.ImportanceTracing"), TEXT("NumBounceHemisphereSamples"), Scene.ImportanceTracingSettings.NumBounceHemisphereSamples, GLightmassIni));
		FLOAT MaxHemisphereAngleDegrees;
		verify(GConfig->GetFloat(TEXT("DevOptions.ImportanceTracing"), TEXT("MaxHemisphereRayAngle"), MaxHemisphereAngleDegrees, GLightmassIni));
		Scene.ImportanceTracingSettings.MaxHemisphereRayAngle = MaxHemisphereAngleDegrees * (FLOAT)PI / 180.0f;
	}
	{
		verify(GConfig->GetBool(TEXT("DevOptions.PhotonMapping"), TEXT("bUsePhotonMapping"), Scene.PhotonMappingSettings.bUsePhotonMapping, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.PhotonMapping"), TEXT("bUseFinalGathering"), Scene.PhotonMappingSettings.bUseFinalGathering, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.PhotonMapping"), TEXT("bUsePhotonsForDirectLighting"), Scene.PhotonMappingSettings.bUsePhotonsForDirectLighting, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.PhotonMapping"), TEXT("bUseIrradiancePhotons"), Scene.PhotonMappingSettings.bUseIrradiancePhotons, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.PhotonMapping"), TEXT("bCacheIrradiancePhotonsOnSurfaces"), Scene.PhotonMappingSettings.bCacheIrradiancePhotonsOnSurfaces, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.PhotonMapping"), TEXT("bOptimizeDirectLightingWithPhotons"), Scene.PhotonMappingSettings.bOptimizeDirectLightingWithPhotons, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.PhotonMapping"), TEXT("bVisualizePhotonPaths"), Scene.PhotonMappingSettings.bVisualizePhotonPaths, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.PhotonMapping"), TEXT("bVisualizePhotonGathers"), Scene.PhotonMappingSettings.bVisualizePhotonGathers, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.PhotonMapping"), TEXT("bVisualizePhotonImportanceSamples"), Scene.PhotonMappingSettings.bVisualizePhotonImportanceSamples, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.PhotonMapping"), TEXT("bVisualizeIrradiancePhotonCalculation"), Scene.PhotonMappingSettings.bVisualizeIrradiancePhotonCalculation, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.PhotonMapping"), TEXT("bEmitPhotonsOutsideImportanceVolume"), Scene.PhotonMappingSettings.bEmitPhotonsOutsideImportanceVolume, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("ConeFilterConstant"), Scene.PhotonMappingSettings.ConeFilterConstant, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.PhotonMapping"), TEXT("NumIrradianceCalculationPhotons"), Scene.PhotonMappingSettings.NumIrradianceCalculationPhotons, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("FinalGatherImportanceSampleFraction"), Scene.PhotonMappingSettings.FinalGatherImportanceSampleFraction, GLightmassIni));
		FLOAT FinalGatherImportanceSampleConeAngle;
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("FinalGatherImportanceSampleConeAngle"), FinalGatherImportanceSampleConeAngle, GLightmassIni));
		Scene.PhotonMappingSettings.FinalGatherImportanceSampleCosConeAngle = appCos(Clamp(FinalGatherImportanceSampleConeAngle, 0.0f, 90.0f) * (FLOAT)PI / 180.0f);
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("IndirectPhotonEmitDiskRadius"), Scene.PhotonMappingSettings.IndirectPhotonEmitDiskRadius, GLightmassIni));
		FLOAT IndirectPhotonEmitConeAngleDegrees;
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("IndirectPhotonEmitConeAngle"), IndirectPhotonEmitConeAngleDegrees, GLightmassIni));
		Scene.PhotonMappingSettings.IndirectPhotonEmitConeAngle = Clamp(IndirectPhotonEmitConeAngleDegrees, 0.0f, 90.0f) * (FLOAT)PI / 180.0f;
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("MaxImportancePhotonSearchDistance"), Scene.PhotonMappingSettings.MaxImportancePhotonSearchDistance, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("MinImportancePhotonSearchDistance"), Scene.PhotonMappingSettings.MinImportancePhotonSearchDistance, GLightmassIni));
		verify(GConfig->GetInt(TEXT("DevOptions.PhotonMapping"), TEXT("NumImportanceSearchPhotons"), Scene.PhotonMappingSettings.NumImportanceSearchPhotons, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("OutsideImportanceVolumeDensityScale"), Scene.PhotonMappingSettings.OutsideImportanceVolumeDensityScale, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("DirectPhotonDensity"), Scene.PhotonMappingSettings.DirectPhotonDensity, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("DirectIrradiancePhotonDensity"), Scene.PhotonMappingSettings.DirectIrradiancePhotonDensity, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("DirectPhotonSearchDistance"), Scene.PhotonMappingSettings.DirectPhotonSearchDistance, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("IndirectPhotonPathDensity"), Scene.PhotonMappingSettings.IndirectPhotonPathDensity, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("IndirectPhotonDensity"), Scene.PhotonMappingSettings.IndirectPhotonDensity, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("IndirectIrradiancePhotonDensity"), Scene.PhotonMappingSettings.IndirectIrradiancePhotonDensity, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("IndirectPhotonSearchDistance"), Scene.PhotonMappingSettings.IndirectPhotonSearchDistance, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("CausticPhotonDensity"), Scene.PhotonMappingSettings.CausticPhotonDensity, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("CausticPhotonSearchDistance"), Scene.PhotonMappingSettings.CausticPhotonSearchDistance, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("PhotonSearchAngleThreshold"), Scene.PhotonMappingSettings.PhotonSearchAngleThreshold, GLightmassIni));
		FLOAT IrradiancePhotonSearchConeAngle;
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("IrradiancePhotonSearchConeAngle"), IrradiancePhotonSearchConeAngle, GLightmassIni));
		Scene.PhotonMappingSettings.MinCosIrradiancePhotonSearchCone = appCos((90.0f - Clamp(IrradiancePhotonSearchConeAngle, 1.0f, 90.0f)) * (FLOAT)PI / 180.0f);
		FLOAT MaxDirectLightingPhotonOptimizationAngle;
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("MaxDirectLightingPhotonOptimizationAngle"), MaxDirectLightingPhotonOptimizationAngle, GLightmassIni));
		Scene.PhotonMappingSettings.MaxCosDirectLightingPhotonOptimizationAngle = appCos(Clamp(MaxDirectLightingPhotonOptimizationAngle, 0.0f, 90.0f) * (FLOAT)PI / 180.0f);  
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("CachedIrradiancePhotonDownsampleFactor"), Scene.PhotonMappingSettings.CachedIrradiancePhotonDownsampleFactor, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.PhotonMapping"), TEXT("OutsideVolumeIrradiancePhotonDistanceThreshold"), Scene.PhotonMappingSettings.OutsideVolumeIrradiancePhotonDistanceThreshold, GLightmassIni));
	}
	{
		verify(GConfig->GetBool(TEXT("DevOptions.IrradianceCache"), TEXT("bAllowIrradianceCaching"), Scene.IrradianceCachingSettings.bAllowIrradianceCaching, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.IrradianceCache"), TEXT("bUseIrradianceGradients"), Scene.IrradianceCachingSettings.bUseIrradianceGradients, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.IrradianceCache"), TEXT("bShowGradientsOnly"), Scene.IrradianceCachingSettings.bShowGradientsOnly, GLightmassIni));
		verify(GConfig->GetBool(TEXT("DevOptions.IrradianceCache"), TEXT("bVisualizeIrradianceSamples"), Scene.IrradianceCachingSettings.bVisualizeIrradianceSamples, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.IrradianceCache"), TEXT("RecordRadiusScale"), Scene.IrradianceCachingSettings.RecordRadiusScale, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.IrradianceCache"), TEXT("InterpolationMaxAngle"), Scene.IrradianceCachingSettings.InterpolationMaxAngle, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.IrradianceCache"), TEXT("PointBehindRecordMaxAngle"), Scene.IrradianceCachingSettings.PointBehindRecordMaxAngle, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.IrradianceCache"), TEXT("DistanceSmoothFactor"), Scene.IrradianceCachingSettings.DistanceSmoothFactor, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.IrradianceCache"), TEXT("AngleSmoothFactor"), Scene.IrradianceCachingSettings.AngleSmoothFactor, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.IrradianceCache"), TEXT("RecordBounceRadiusScale"), Scene.IrradianceCachingSettings.RecordBounceRadiusScale, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.IrradianceCache"), TEXT("MinRecordRadius"), Scene.IrradianceCachingSettings.MinRecordRadius, GLightmassIni));
		verify(GConfig->GetFloat(TEXT("DevOptions.IrradianceCache"), TEXT("MaxRecordRadius"), Scene.IrradianceCachingSettings.MaxRecordRadius, GLightmassIni));
	}

	// Modify settings based on the quality level required
	// Preview is assumed to have a scale of 1 for all settings and therefore is not in the ini
	if (QualityLevel != Quality_Preview)
	{
		const TCHAR* QualitySectionNames[Quality_MAX] = {
			TEXT(""), 
			TEXT("DevOptions.StaticLightingMediumQuality"), 
			TEXT("DevOptions.StaticLightingHighQuality"), 
			TEXT("DevOptions.StaticLightingProductionQuality")};

		FLOAT NumShadowRaysScale;
		verify(GConfig->GetFloat(QualitySectionNames[QualityLevel], TEXT("NumShadowRaysScale"), NumShadowRaysScale, GLightmassIni));
		Scene.ShadowSettings.NumShadowRays = appTrunc(Scene.ShadowSettings.NumShadowRays * NumShadowRaysScale);

		FLOAT NumPenumbraShadowRaysScale;
		verify(GConfig->GetFloat(QualitySectionNames[QualityLevel], TEXT("NumPenumbraShadowRaysScale"), NumPenumbraShadowRaysScale, GLightmassIni));
		Scene.ShadowSettings.NumPenumbraShadowRays = appTrunc(Scene.ShadowSettings.NumPenumbraShadowRays * NumPenumbraShadowRaysScale);

		FLOAT ApproximateHighResTexelsPerMaxTransitionDistanceScale;
		verify(GConfig->GetFloat(QualitySectionNames[QualityLevel], TEXT("ApproximateHighResTexelsPerMaxTransitionDistanceScale"), ApproximateHighResTexelsPerMaxTransitionDistanceScale, GLightmassIni));
		Scene.ShadowSettings.ApproximateHighResTexelsPerMaxTransitionDistance = appTrunc(Scene.ShadowSettings.ApproximateHighResTexelsPerMaxTransitionDistance * ApproximateHighResTexelsPerMaxTransitionDistanceScale);

		verify(GConfig->GetInt(QualitySectionNames[QualityLevel], TEXT("MinDistanceFieldUpsampleFactor"), Scene.ShadowSettings.MinDistanceFieldUpsampleFactor, GLightmassIni));

		FLOAT NumHemisphereSamplesScale;
		verify(GConfig->GetFloat(QualitySectionNames[QualityLevel], TEXT("NumHemisphereSamplesScale"), NumHemisphereSamplesScale, GLightmassIni));
		Scene.ImportanceTracingSettings.NumHemisphereSamples = appTrunc(Scene.ImportanceTracingSettings.NumHemisphereSamples * NumHemisphereSamplesScale);

		FLOAT NumImportanceSearchPhotonsScale;
		verify(GConfig->GetFloat(QualitySectionNames[QualityLevel], TEXT("NumImportanceSearchPhotonsScale"), NumImportanceSearchPhotonsScale, GLightmassIni));
		Scene.PhotonMappingSettings.NumImportanceSearchPhotons = appTrunc(Scene.PhotonMappingSettings.NumImportanceSearchPhotons * NumImportanceSearchPhotonsScale);

		FLOAT NumDirectPhotonsScale;
		verify(GConfig->GetFloat(QualitySectionNames[QualityLevel], TEXT("NumDirectPhotonsScale"), NumDirectPhotonsScale, GLightmassIni));
		Scene.PhotonMappingSettings.DirectPhotonDensity = Scene.PhotonMappingSettings.DirectPhotonDensity * NumDirectPhotonsScale;
		Scene.PhotonMappingSettings.DirectIrradiancePhotonDensity = Scene.PhotonMappingSettings.DirectIrradiancePhotonDensity * NumDirectPhotonsScale; 

		FLOAT DirectPhotonSearchDistanceScale;
		verify(GConfig->GetFloat(QualitySectionNames[QualityLevel], TEXT("DirectPhotonSearchDistanceScale"), DirectPhotonSearchDistanceScale, GLightmassIni));
		Scene.PhotonMappingSettings.DirectPhotonSearchDistance = Scene.PhotonMappingSettings.DirectPhotonSearchDistance * DirectPhotonSearchDistanceScale;

		FLOAT NumIndirectPhotonPathsScale;
		verify(GConfig->GetFloat(QualitySectionNames[QualityLevel], TEXT("NumIndirectPhotonPathsScale"), NumIndirectPhotonPathsScale, GLightmassIni));
		Scene.PhotonMappingSettings.IndirectPhotonPathDensity = Scene.PhotonMappingSettings.IndirectPhotonPathDensity * NumIndirectPhotonPathsScale;

		FLOAT NumIndirectPhotonsScale;
		verify(GConfig->GetFloat(QualitySectionNames[QualityLevel], TEXT("NumIndirectPhotonsScale"), NumIndirectPhotonsScale, GLightmassIni));
		Scene.PhotonMappingSettings.IndirectPhotonDensity = Scene.PhotonMappingSettings.IndirectPhotonDensity * NumIndirectPhotonsScale;

		FLOAT NumIndirectIrradiancePhotonsScale;
		verify(GConfig->GetFloat(QualitySectionNames[QualityLevel], TEXT("NumIndirectIrradiancePhotonsScale"), NumIndirectIrradiancePhotonsScale, GLightmassIni));
		Scene.PhotonMappingSettings.IndirectIrradiancePhotonDensity = Scene.PhotonMappingSettings.IndirectIrradiancePhotonDensity * NumIndirectIrradiancePhotonsScale;

		FLOAT NumCausticPhotonsScale;
		verify(GConfig->GetFloat(QualitySectionNames[QualityLevel], TEXT("NumCausticPhotonsScale"), NumCausticPhotonsScale, GLightmassIni));
		Scene.PhotonMappingSettings.CausticPhotonDensity = Scene.PhotonMappingSettings.CausticPhotonDensity * NumCausticPhotonsScale; 

		FLOAT RecordRadiusScaleScale;
		verify(GConfig->GetFloat(QualitySectionNames[QualityLevel], TEXT("RecordRadiusScaleScale"), RecordRadiusScaleScale, GLightmassIni));
		Scene.IrradianceCachingSettings.RecordRadiusScale = Scene.IrradianceCachingSettings.RecordRadiusScale * RecordRadiusScaleScale;

		FLOAT InterpolationMaxAngleScale;
		verify(GConfig->GetFloat(QualitySectionNames[QualityLevel], TEXT("RecordRadiusScaleScale"), InterpolationMaxAngleScale, GLightmassIni));
		Scene.IrradianceCachingSettings.InterpolationMaxAngle = Scene.IrradianceCachingSettings.InterpolationMaxAngle * InterpolationMaxAngleScale;

		FLOAT MinRecordRadiusScale;
		verify(GConfig->GetFloat(QualitySectionNames[QualityLevel], TEXT("MinRecordRadiusScale"), MinRecordRadiusScale, GLightmassIni));
		Scene.IrradianceCachingSettings.MinRecordRadius = Scene.IrradianceCachingSettings.MinRecordRadius * MinRecordRadiusScale;
	}
}

/** Fills InputData with debug information */
void FLightmassExporter::WriteDebugInput( Lightmass::FDebugLightingInputData& InputData, FGuid& DebugMappingGuid )
{
	InputData.bRelaySolverStats = !FName::SafeSuppressed(NAME_DevLightmassSolver);
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING && !CONSOLE
	FindDebugMapping(DebugMappingGuid);
#endif
	Copy(DebugMappingGuid, InputData.MappingGuid);
	InputData.NodeIndex = GCurrentSelectedLightmapSample.NodeIndex;
	Copy(FVector4(GCurrentSelectedLightmapSample.Position, 0), InputData.Position);
	InputData.LocalX = GCurrentSelectedLightmapSample.LocalX;
	InputData.LocalY = GCurrentSelectedLightmapSample.LocalY;
	InputData.MappingSizeX = GCurrentSelectedLightmapSample.MappingSizeX;
	InputData.MappingSizeY = GCurrentSelectedLightmapSample.MappingSizeY;
	InputData.LightmapX = GCurrentSelectedLightmapSample.LightmapX;
	InputData.LightmapY = GCurrentSelectedLightmapSample.LightmapY;
	InputData.VertexIndex = GCurrentSelectedLightmapSample.VertexIndex;
	Copy(GCurrentSelectedLightmapSample.OriginalColor, InputData.OriginalColor);
	FVector4 ViewPosition(0, 0, 0, 0);
	for (INT ViewIndex = 0; ViewIndex < GEditor->ViewportClients.Num(); ViewIndex++)
	{
		if (GEditor->ViewportClients(ViewIndex)->ViewportType == LVT_Perspective)
		{
			ViewPosition = GEditor->ViewportClients(ViewIndex)->ViewLocation;
		}
	}
	Copy(ViewPosition, InputData.CameraPosition);
	INT DebugVisibilityId = INDEX_NONE;
	UBOOL bVisualizePrecomputedVisibility = FALSE;
	verify(GConfig->GetBool(TEXT("DevOptions.PrecomputedVisibility"), TEXT("bVisualizePrecomputedVisibility"), bVisualizePrecomputedVisibility, GLightmassIni));
	if (bVisualizePrecomputedVisibility)
	{
		for (FSelectedActorIterator It; It; ++It)
		{
			for (INT ComponentIndex = 0; ComponentIndex < It->Components.Num(); ComponentIndex++)
			{
				UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(It->Components(ComponentIndex));
				if (Component)
				{
					if (DebugVisibilityId == INDEX_NONE)
					{
						DebugVisibilityId = Component->VisibilityId;
					}
					else if (DebugVisibilityId != Component->VisibilityId)
					{
						warnf(NAME_DevLightmassSolver, TEXT("Not debugging visibility for component %s with vis id %u, as it was not the first component on the selected actor."),
							*Component->GetPathName(), Component->VisibilityId);
					}
				}
			}
		}

		for (INT LevelIndex= 0; LevelIndex < GWorld->Levels.Num(); LevelIndex++)
		{
			ULevel* Level = GWorld->Levels(LevelIndex);
			for (INT SurfIdx = 0; SurfIdx < Level->Model->Surfs.Num(); SurfIdx++)
			{
				const FBspSurf& Surf = Level->Model->Surfs(SurfIdx);
				if ((Surf.PolyFlags & PF_Selected) != 0)
				{
					for (INT NodeIdx = 0; NodeIdx < Level->Model->Nodes.Num(); NodeIdx++)
					{
						const FBspNode& Node = Level->Model->Nodes(NodeIdx);
						if (Node.iSurf == SurfIdx)
						{
							UModelComponent* SomeModelComponent = Level->ModelComponents(Node.ComponentIndex);
							if (DebugVisibilityId == INDEX_NONE)
							{
								DebugVisibilityId = SomeModelComponent->VisibilityId;
							}
							else if (DebugVisibilityId != SomeModelComponent->VisibilityId)
							{
								warnf(NAME_DevLightmassSolver, TEXT("Not debugging visibility for model component %s with vis id %u!"),
									*SomeModelComponent->GetPathName(), SomeModelComponent->VisibilityId);
							}
						}
					}
				}
			}
		}
	}
	InputData.DebugVisibilityId = DebugVisibilityId;
}

void FLightmassExporter::AddLight(ULightComponent* Light)
{
	UDirectionalLightComponent* DirectionalLight = Cast<UDirectionalLightComponent>(Light);
	UPointLightComponent* PointLight = Cast<UPointLightComponent>(Light);
	USpotLightComponent* SpotLight = Cast<USpotLightComponent>(Light);
	USkyLightComponent* SkyLight = Cast<USkyLightComponent>(Light);

	if( DirectionalLight )
	{
		DirectionalLights.AddItem(DirectionalLight);
	}
	else if( SpotLight )
	{
		SpotLights.AddItem(SpotLight);
	}
	else if( SkyLight )
	{
		SkyLights.AddItem(SkyLight);
	}
	else if( PointLight )
	{
		PointLights.AddItem(PointLight);
	}
}

/*-----------------------------------------------------------------------------
	FLightmassProcessor
-----------------------------------------------------------------------------*/

/** 
 * Constructor
 * 
 * @param bInDumpBinaryResults TRUE if it should dump out raw binary lighting data to disk
 */
FLightmassProcessor::FLightmassProcessor(const FStaticLightingSystem& InSystem, UBOOL bInDumpBinaryResults, UBOOL bInOnlyBuildVisibility, UBOOL bInOnlyBuildVisibleLevels)
:	Exporter(NULL)
,	Importer(NULL)
,	System(InSystem)
,	Swarm( NSwarm::FSwarmInterface::Get() )
,	bProcessingSuccessful(FALSE)
,	bProcessingFailed(FALSE)
,	bQuitReceived(FALSE)
,	NumCompletedTasks(0)
,	bRunningLightmass(FALSE)
,	bDumpBinaryResults( bInDumpBinaryResults )
,	bOnlyBuildVisibility( bInOnlyBuildVisibility )
,	bOnlyBuildVisibleLevels( bInOnlyBuildVisibleLevels )
,	bUseDeterministicLighting(FALSE)
,	bImportCompletedMappingsImmediately(FALSE)
,	bProcessCompletedMappingsImmediately(FALSE)
,	MappingToProcessIndex(0)
{
	//@lmtodo. For some reason, the global instance is not initializing to the default settings...
	GLightmassDebugOptions.Touch();
	// Since these can be set by the commandline, we need to update them here...
	GLightmassDebugOptions.bDebugMode = GLightmassDebugMode;
	GLightmassDebugOptions.bStatsEnabled = GLightmassStatsMode;

	NSwarm::TLogFlags LogFlags = NSwarm::SWARM_LOG_NONE;
	if (GLightmassDebugOptions.bStatsEnabled)
	{
		LogFlags = NSwarm::TLogFlags(LogFlags | NSwarm::SWARM_LOG_TIMINGS);
	}
	INT ConnectionHandle = Swarm.OpenConnection( SwarmCallback, this, LogFlags );
	bSwarmConnectionIsValid = (ConnectionHandle >= 0);

	Exporter = new FLightmassExporter();
	check(Exporter);
	Exporter->bSwarmConnectionIsValid = bSwarmConnectionIsValid;
}

FLightmassProcessor::~FLightmassProcessor()
{
	delete Exporter;
	delete Importer;

	Swarm.CloseConnection();

	for ( TMap<FGuid, FMappingImportHelper*>::TIterator It(ImportedMappings); It; ++It )
	{
		FMappingImportHelper* ImportData = It.Value();
		delete ImportData;
	}
	ImportedMappings.Empty();
}

/** Retrieve an exporter for the given channel name */
FLightmassExporter* FLightmassProcessor::GetLightmassExporter()
{
	return Exporter;
}

FString FLightmassProcessor::GetMappingFileExtension(const FStaticLightingMapping* InMapping)
{
	// Determine the input file name
	FString FileExtension = TEXT("");
	if (InMapping)
	{
		if (InMapping->IsTextureMapping() == TRUE)
		{
			FileExtension = Lightmass::LM_TEXTUREMAPPING_EXTENSION;
		}
		else
		if (InMapping->IsVertexMapping() == TRUE)
		{
			FileExtension = Lightmass::LM_VERTEXMAPPING_EXTENSION;
		}
	}

	return FileExtension;
}

Lightmass::FGuid FLightmassProcessor_GetMappingFileVersion(const FStaticLightingMapping* InMapping)
{
	// Determine the input file name
	Lightmass::FGuid ReturnGuid = Lightmass::FGuid(0,0,0,0);
	if (InMapping)
	{
		if (InMapping->IsTextureMapping() == TRUE)
		{
			ReturnGuid = Lightmass::LM_TEXTUREMAPPING_VERSION;
		}
		else
		if (InMapping->IsVertexMapping() == TRUE)
		{
			ReturnGuid = Lightmass::LM_VERTEXMAPPING_VERSION;
		}
	}
	return ReturnGuid;
}

UBOOL FLightmassProcessor::OpenJob()
{
	// Start the Job
	NSwarm::FGuid JobGuid;
	Copy( Exporter->SceneGuid, JobGuid );

	INT ErrorCode = Swarm.OpenJob( JobGuid );
	if( ErrorCode < 0 )
	{
		debugf( TEXT("Error, OpenJob failed with error code %d"), ErrorCode );
		return FALSE;
	}
	return TRUE;
}

UBOOL FLightmassProcessor::CloseJob()
{
	// All done, end the Job
	INT ErrorCode = Swarm.CloseJob();
	if( ErrorCode < 0 )
	{
		debugf( TEXT("Error, CloseJob failed with error code %d"), ErrorCode );
		return FALSE;
	}
	return TRUE;
}

UBOOL FLightmassProcessor::Run()
{
	DOUBLE SwarmJobStartTime = appSeconds();
	VolumeSampleTaskCompleted = 0;
	MeshAreaLightDataTaskCompleted = 0;
	VolumeDistanceFieldTaskCompleted = 0;

	// Check if we can use 64-bit Lightmass.
	UBOOL bUse64bitProcess = FALSE;
	UBOOL bAllow64bitProcess = TRUE;
	verify(GConfig->GetBool(TEXT("DevOptions.StaticLighting"), TEXT("bAllow64bitProcess"), bAllow64bitProcess, GLightmassIni));
	if ( bAllow64bitProcess && appIs64bitOperatingSystem() )
	{
		bUse64bitProcess = TRUE;
	}

	// Setup dependencies for 32bit.
	const TCHAR* LightmassExecutable32 = TEXT("..\\Win32\\UnrealLightmass.exe");
	const TCHAR* RequiredDependencyPaths32[] =
	{
		TEXT("..\\AgentInterface.dll"),
	};
	const INT RequiredDependencyPaths32Count = ARRAY_COUNT(RequiredDependencyPaths32);

	// Setup dependencies for 64bit.
	const TCHAR* LightmassExecutable64 = TEXT("..\\Win64\\UnrealLightmass.exe");
	const TCHAR* RequiredDependencyPaths64[] =
	{
		TEXT("..\\AgentInterface.dll"),
	};
	const INT RequiredDependencyPaths64Count = ARRAY_COUNT(RequiredDependencyPaths64);

#if !UDK
	// Set up optional dependencies.
	const TCHAR* OptionalDependencyPaths32[] =
	{
		TEXT("..\\Win32\\UnrealLightmass.pdb"),
		TEXT("..\\AutoReporter.exe"),
		TEXT("..\\AutoReporter.exe.config"),
		TEXT("..\\AutoReporter.XmlSerializers.dll"),
	};
	const INT OptionalDependencyPaths32Count = ARRAY_COUNT(OptionalDependencyPaths32);

	const TCHAR* OptionalDependencyPaths64[] =
	{
		TEXT("..\\Win64\\UnrealLightmass.pdb"),
		TEXT("..\\AutoReporter.exe"),
		TEXT("..\\AutoReporter.exe.config"),
		TEXT("..\\AutoReporter.XmlSerializers.dll"),
	};
	const INT OptionalDependencyPaths64Count = ARRAY_COUNT(OptionalDependencyPaths64);

#else // UDK

	// For UDK builds, all optional dependencies are omitted from the distribution
	const TCHAR** OptionalDependencyPaths32 = NULL;
	const TCHAR** OptionalDependencyPaths64 = NULL;
	const INT OptionalDependencyPaths32Count = 0;
	const INT OptionalDependencyPaths64Count = 0;

#endif // !UDK

	// Set up the description for the Job
	const TCHAR* DescriptionKeys[] =
	{
		TEXT("MapName"),
		TEXT("GameName"),
		TEXT("QualityLevel")
	};

	// Get the map name
	FString MapNameStr = GWorld ? GWorld->GetMapName() : FString( TEXT("None") );
	const TCHAR* MapName = MapNameStr.GetCharArray().GetData();
	// Get the game name
	const TCHAR* GameName = GGameName;
	// Get the quality level
	TCHAR QualityLevel[MAX_SPRINTF] = TEXT("");
	appSprintf( QualityLevel, TEXT("%d"), ( INT )Exporter->QualityLevel );

	const TCHAR* DescriptionValues[] =
	{
		MapName,
		GameName,
		QualityLevel
	};

	// Create the job - one task per mapping.
	bProcessingSuccessful = FALSE;
	bProcessingFailed = FALSE;
	bQuitReceived = FALSE;
	NumCompletedTasks = 0;
	bRunningLightmass = FALSE;

	Statistics.SwarmJobTime += appSeconds() - SwarmJobStartTime;
	DOUBLE ExportStartTime = appSeconds();

	debugf( TEXT("Swarm launching: %s %s"), bUse64bitProcess ? LightmassExecutable64 : LightmassExecutable32, *Exporter->SceneGuid.String() );

	// If the Job started successfully, export the scene
	GWarn->StatusUpdatef( 0, 100, TEXT("Exporting the scene...") );
	DOUBLE StartTime = appSeconds();

	INT NumCellDistributionBuckets;
	verify(GConfig->GetInt(TEXT("DevOptions.PrecomputedVisibility"), TEXT("NumCellDistributionBuckets"), NumCellDistributionBuckets, GLightmassIni));

	for (INT DistributionBucketIndex = 0; DistributionBucketIndex < NumCellDistributionBuckets; DistributionBucketIndex++)
	{
		Exporter->VisibilityBucketGuids.AddItem(appCreateGuid());
	}

	Exporter->WriteToChannel(DebugMappingGuid);

	UBOOL bGarbageCollectAfterExport = FALSE;
	verify(GConfig->GetBool(TEXT("DevOptions.StaticLighting"), TEXT("bGarbageCollectAfterExport"), bGarbageCollectAfterExport, GLightmassIni));
	if (bGarbageCollectAfterExport == TRUE)
	{
		UObject::CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, TRUE);
	}

	Statistics.ExportTime += appSeconds() - ExportStartTime;
	SwarmJobStartTime = appSeconds();

	// If using Debug Mode (off by default), we use a hard-coded job GUID and Lightmass must be executed manually
	// (e.g. through a debugger), using the -debug command line parameter.
	// Lightmass will read all the cached input files and process the whole job locally without notifying
	// Swarm or UE3 that the job is completed. This also means that Lightmass can be executed as many times
	// as required (the input files will still be there in the Swarm cache) and UE3 doesn't need to be
	// running concurrently.
	INT JobFlags = NSwarm::JOB_FLAG_USE_DEFAULTS;
	if (GLightmassDebugOptions.bDebugMode)
	{
		debugf( TEXT("Waiting for UnrealLightmass.exe to be launched manually...") );
		debugf( TEXT("Note: This Job will not be distributed") );
		JobFlags |= NSwarm::JOB_FLAG_MANUAL_START;
	}
	else
	{
		// Enable Swarm Job distribution, if requested
		if (GSwarmDebugOptions.bDistributionEnabled)
		{
			debugf( TEXT("Swarm will be allowed to distribute this job") );
			JobFlags |= NSwarm::JOB_FLAG_ALLOW_REMOTE;
		}
		else
		{
			debugf( TEXT("Swarm will be not be allowed to distribute this job; it will run locally only") );
		}
	}

	NSwarm::FGuid TaskGuid;
	FString CommandLineParameters = *Exporter->SceneGuid.String();
	if (GLightmassDebugOptions.bStatsEnabled)
	{
		CommandLineParameters += TEXT(" -stats");
	}

	//append on the maximum number of triangles per leaf for the kdop tree
	UINT MaxTrisPerLeaf = 4;
	if (GWorld && GWorld->GetWorldInfo())
	{
		MaxTrisPerLeaf = GWorld->GetWorldInfo()->MaxTrianglesPerLeaf;
	}
	CommandLineParameters += FString::Printf(TEXT(" -trisperleaf %i"), MaxTrisPerLeaf);

	NSwarm::FJobSpecification JobSpecification32, JobSpecification64;
	JobSpecification32 = NSwarm::FJobSpecification( LightmassExecutable32, *CommandLineParameters, ( NSwarm::TJobTaskFlags )JobFlags );
	JobSpecification32.AddDependencies( RequiredDependencyPaths32, RequiredDependencyPaths32Count, OptionalDependencyPaths32, OptionalDependencyPaths32Count );
	JobSpecification32.AddDescription( DescriptionKeys, DescriptionValues, ARRAY_COUNT(DescriptionKeys) );
	if ( bUse64bitProcess )
	{
		JobSpecification64 = NSwarm::FJobSpecification( LightmassExecutable64, *CommandLineParameters, ( NSwarm::TJobTaskFlags )JobFlags );
		JobSpecification64.AddDependencies( RequiredDependencyPaths64, RequiredDependencyPaths64Count, OptionalDependencyPaths64, OptionalDependencyPaths64Count );
		JobSpecification64.AddDescription( DescriptionKeys, DescriptionValues, ARRAY_COUNT(DescriptionKeys) );
	}
	INT ErrorCode = Swarm.BeginJobSpecification( JobSpecification32, JobSpecification64 );
	if( ErrorCode < 0 )
	{
		debugf( TEXT("Error, BeginJobSpecification failed with error code %d"), ErrorCode );
	}

	// Count the number of tasks given to Swarm
	INT NumTotalSwarmTasks = 0;

	if (GWorld->GetWorldInfo()->bPrecomputeVisibility)
	{
		for (INT TaskIndex = 0; TaskIndex < Exporter->VisibilityBucketGuids.Num(); TaskIndex++)
		{
			Copy(Exporter->VisibilityBucketGuids(TaskIndex), TaskGuid);
			NSwarm::FTaskSpecification NewTaskSpecification( TaskGuid, TEXT("PrecomputedVisibility"), NSwarm::JOB_TASK_FLAG_USE_DEFAULTS );
			//@todo - accurately estimate cost
			NewTaskSpecification.Cost = 10000;
			ErrorCode = Swarm.AddTask( NewTaskSpecification );
			if( ErrorCode >= 0 )
			{
				NumTotalSwarmTasks++;
			}
			else
			{
				debugf( TEXT("Error, AddTask failed with error code %d"), ErrorCode );
			}
		}
	}

	if (!bOnlyBuildVisibility)
	{
		{
			Copy(Lightmass::PrecomputedVolumeLightingGuid, TaskGuid);
			NSwarm::FTaskSpecification NewTaskSpecification( TaskGuid, TEXT("VolumeSamples"), NSwarm::JOB_TASK_FLAG_USE_DEFAULTS );
			//@todo - accurately estimate cost
			// Changed estimated cost: this should be the maximum cost, because it became really big if there are WORLD_MAX size light-mapping
			NewTaskSpecification.Cost = INT_MAX;
			ErrorCode = Swarm.AddTask( NewTaskSpecification );
			if( ErrorCode >= 0 )
			{
				NumTotalSwarmTasks++;
			}
			else
			{
				debugf( TEXT("Error, AddTask failed with error code %d"), ErrorCode );
			}
		}

		if (GWorld->DominantDirectionalLight
			 && !GWorld->DominantDirectionalLight->GetOwner()->bMovable
			 && AddDominantShadowTask(GWorld->DominantDirectionalLight))
		{
			NumTotalSwarmTasks++;
		}

		for (TSparseArray<UDominantSpotLightComponent*>::TConstIterator LightIt(GWorld->DominantSpotLights); LightIt; ++LightIt)
		{
			UDominantSpotLightComponent* CurrentLight = *LightIt;
			if (AddDominantShadowTask(CurrentLight))
			{
				NumTotalSwarmTasks++;
			}
		}

		{
			Copy(Lightmass::MeshAreaLightDataGuid, TaskGuid);
			NSwarm::FTaskSpecification NewTaskSpecification( TaskGuid, TEXT("MeshAreaLightData"), NSwarm::JOB_TASK_FLAG_USE_DEFAULTS );
			NewTaskSpecification.Cost = 1000;
			ErrorCode = Swarm.AddTask( NewTaskSpecification );
			if( ErrorCode >= 0 )
			{
				NumTotalSwarmTasks++;
			}
			else
			{
				debugf( TEXT("Error, AddTask failed with error code %d"), ErrorCode );
			}
		}

		if (GWorld->GetWorldInfo()->LightmassSettings.bEnableImageReflectionShadowing)
		{
			Copy(Lightmass::VolumeDistanceFieldGuid, TaskGuid);
			NSwarm::FTaskSpecification NewTaskSpecification( TaskGuid, TEXT("VolumeDistanceFieldData"), NSwarm::JOB_TASK_FLAG_USE_DEFAULTS );
			NewTaskSpecification.Cost = 100000;
			ErrorCode = Swarm.AddTask( NewTaskSpecification );
			if( ErrorCode >= 0 )
			{
				NumTotalSwarmTasks++;
			}
			else
			{
				debugf( TEXT("Error, AddTask failed with error code %d"), ErrorCode );
			}
		}

		// Add BSP mapping tasks.
		for( INT MappingIdx=0; (ErrorCode >= 0) && MappingIdx < Exporter->BSPSurfaceMappings.Num() && !GEditor->GetMapBuildCancelled(); MappingIdx++ )
		{
			FBSPSurfaceStaticLighting* BSPMapping = Exporter->BSPSurfaceMappings(MappingIdx);
			if (BSPMapping->bProcessMapping == TRUE)
			{
				Copy(BSPMapping->GetLightingGuid(), TaskGuid);
				PendingBSPMappings.Set(BSPMapping->GetLightingGuid(), BSPMapping);

				NSwarm::FTaskSpecification NewTaskSpecification( TaskGuid, TEXT("BSPMapping"), NSwarm::JOB_TASK_FLAG_USE_DEFAULTS );
				NewTaskSpecification.Cost = BSPMapping->GetTexelCount();
				ErrorCode = Swarm.AddTask( NewTaskSpecification );
				if( ErrorCode >= 0 )
				{
					NumTotalSwarmTasks++;
				}
				else
				{
					debugf( TEXT("Error, AddTask failed with error code %d"), ErrorCode );
				}
			}
		}

		// Add static mesh texture mappings tasks.
		for( INT MappingIdx=0; (ErrorCode >= 0) && MappingIdx < Exporter->StaticMeshTextureMappings.Num() && !GEditor->GetMapBuildCancelled(); MappingIdx++ )
		{
			FStaticMeshStaticLightingTextureMapping* SMTextureMapping = Exporter->StaticMeshTextureMappings(MappingIdx);
			if (SMTextureMapping->bProcessMapping == TRUE)
			{
				Copy(SMTextureMapping->GetLightingGuid(), TaskGuid);
				PendingTextureMappings.Set(SMTextureMapping->GetLightingGuid(), SMTextureMapping);

				NSwarm::FTaskSpecification NewTaskSpecification( TaskGuid, TEXT("SMTextureMapping"), NSwarm::JOB_TASK_FLAG_USE_DEFAULTS );
				NewTaskSpecification.Cost = SMTextureMapping->GetTexelCount();
				ErrorCode = Swarm.AddTask( NewTaskSpecification );
				if( ErrorCode >= 0 )
				{
					NumTotalSwarmTasks++;
				}
				else
				{
					debugf( TEXT("Error, AddTask failed with error code %d"), ErrorCode );
				}
			}
		}

		// Add terrain mapping tasks.
		for( INT MappingIdx=0; (ErrorCode >= 0) && MappingIdx < Exporter->TerrainMappings.Num() && !GEditor->GetMapBuildCancelled(); MappingIdx++ )
		{
			FTerrainComponentStaticLighting* TerrainMapping = Exporter->TerrainMappings(MappingIdx);
			if (TerrainMapping->bProcessMapping == TRUE)
			{
				Copy(TerrainMapping->GetLightingGuid(), TaskGuid);
				PendingTerrainMappings.Set(TerrainMapping->GetLightingGuid(), TerrainMapping);

				NSwarm::FTaskSpecification NewTaskSpecification( TaskGuid, TEXT("TerrainMapping"), NSwarm::JOB_TASK_FLAG_USE_DEFAULTS );
				NewTaskSpecification.Cost = TerrainMapping->GetTexelCount();
				ErrorCode = Swarm.AddTask( NewTaskSpecification );
				if( ErrorCode >= 0 )
				{
					NumTotalSwarmTasks++;
				}
				else
				{
					debugf( TEXT("Error, AddTask failed with error code %d"), ErrorCode );
				}
			}
		}

		// Add Fluid mapping tasks.
		for( INT MappingIdx=0; (ErrorCode >= 0) && MappingIdx < Exporter->FluidSurfaceTextureMappings.Num() && !GEditor->GetMapBuildCancelled(); MappingIdx++ )
		{
			FFluidSurfaceStaticLightingTextureMapping* FluidMapping = Exporter->FluidSurfaceTextureMappings(MappingIdx);
			if (FluidMapping->bProcessMapping == TRUE)
			{
				Copy(FluidMapping->GetLightingGuid(), TaskGuid);
				PendingFluidMappings.Set(FluidMapping->GetLightingGuid(), FluidMapping);

				NSwarm::FTaskSpecification NewTaskSpecification( TaskGuid, TEXT("FluidMapping"), NSwarm::JOB_TASK_FLAG_USE_DEFAULTS );
				NewTaskSpecification.Cost = FluidMapping->GetTexelCount();
				ErrorCode = Swarm.AddTask( NewTaskSpecification );
				if( ErrorCode >= 0 )
				{
					NumTotalSwarmTasks++;
				}
				else
				{
					debugf( TEXT("Error, AddTask failed with error code %d"), ErrorCode );
				}
			}
		}

		// Add Landscape mapping tasks.
		for( INT MappingIdx=0; (ErrorCode >= 0) && MappingIdx < Exporter->LandscapeTextureMappings.Num() && !GEditor->GetMapBuildCancelled(); MappingIdx++ )
		{
			FLandscapeStaticLightingTextureMapping* LandscapeMapping = Exporter->LandscapeTextureMappings(MappingIdx);
			if (LandscapeMapping->bProcessMapping == TRUE)
			{
				Copy(LandscapeMapping->GetLightingGuid(), TaskGuid);
				PendingLandscapeMappings.Set(LandscapeMapping->GetLightingGuid(), LandscapeMapping);

				NSwarm::FTaskSpecification NewTaskSpecification( TaskGuid, TEXT("LandscapeMapping"), NSwarm::JOB_TASK_FLAG_USE_DEFAULTS );
				NewTaskSpecification.Cost = LandscapeMapping->GetTexelCount();
				ErrorCode = Swarm.AddTask( NewTaskSpecification );
				if( ErrorCode >= 0 )
				{
					NumTotalSwarmTasks++;
				}
				else
				{
					debugf( TEXT("Error, AddTask failed with error code %d"), ErrorCode );
				}
			}
		}

		// Add static mesh vertex mappings tasks.
		for( INT MappingIdx=0; (ErrorCode >= 0) && MappingIdx < Exporter->StaticMeshVertexMappings.Num() && !GEditor->GetMapBuildCancelled(); MappingIdx++ )
		{
			FStaticMeshStaticLightingVertexMapping* SMVertexMapping = Exporter->StaticMeshVertexMappings(MappingIdx);
			if (SMVertexMapping->bProcessMapping == TRUE)
			{
				Copy(SMVertexMapping->GetLightingGuid(), TaskGuid);
				PendingVertexMappings.Set(SMVertexMapping->GetLightingGuid(), SMVertexMapping);

				NSwarm::FTaskSpecification NewTaskSpecification( TaskGuid, TEXT("SMVertexMapping"), NSwarm::JOB_TASK_FLAG_USE_DEFAULTS );
				NewTaskSpecification.Cost = SMVertexMapping->GetTexelCount();
				ErrorCode = Swarm.AddTask( NewTaskSpecification );
				if( ErrorCode >= 0 )
				{
					NumTotalSwarmTasks++;
				}
				else
				{
					debugf( TEXT("Error, AddTask failed with error code %d"), ErrorCode );
				}
			}
		}

	#if WITH_SPEEDTREE
		// Add SpeedTree mapping tasks.
		for( INT MappingIdx=0; (ErrorCode >= 0) && MappingIdx < Exporter->SpeedTreeMappings.Num() && !GEditor->GetMapBuildCancelled(); MappingIdx++ )
		{
			FSpeedTreeStaticLightingMapping* SpeedTreeMapping = Exporter->SpeedTreeMappings(MappingIdx);
			if (SpeedTreeMapping->bProcessMapping == TRUE)
			{
				Copy(SpeedTreeMapping->GetLightingGuid(), TaskGuid);
				PendingSpeedTreeMappings.Set(SpeedTreeMapping->GetLightingGuid(), SpeedTreeMapping);

				NSwarm::FTaskSpecification NewTaskSpecification( TaskGuid, TEXT("SpeedTreeMapping"), NSwarm::JOB_TASK_FLAG_USE_DEFAULTS );
				NewTaskSpecification.Cost = SpeedTreeMapping->GetTexelCount();
				ErrorCode = Swarm.AddTask( NewTaskSpecification );
				if( ErrorCode >= 0 )
				{
					NumTotalSwarmTasks++;
				}
				else
				{
					debugf( TEXT("Error, AddTask failed with error code %d"), ErrorCode );
				}
			}
		}
	#endif	//#if WITH_SPEEDTREE
	}

	INT EndJobErrorCode = Swarm.EndJobSpecification();
	if( EndJobErrorCode < 0 )
	{
		debugf( TEXT("Error, EndJobSpecification failed with error code %d"), ErrorCode );
	}

	if ( ErrorCode < 0 || EndJobErrorCode < 0 )
	{
		bProcessingFailed = TRUE;
	}

	INT NumTotalTasks = NumTotalSwarmTasks;
	if ( bUseDeterministicLighting )
	{
		// In deterministic mode, we import and process the mappings after Lightmass is done, so we have twice the steps.
		NumTotalTasks *= 2;
	}

	GWarn->StatusUpdatef( NumCompletedTasks, NumTotalTasks, TEXT("Running Lightmass...") );

	Statistics.SwarmJobTime += appSeconds() - SwarmJobStartTime;

	// Wait for results...
	DOUBLE LightmassStartTime = appSeconds();
	bRunningLightmass = TRUE;
	while ( !bQuitReceived && !bProcessingFailed && !GEditor->GetMapBuildCancelled() )
	{
		appSleep( 0.050f );
		UBOOL bAllTaskAreComplete = (NumCompletedTasks == NumTotalSwarmTasks ? TRUE : FALSE);

		GWarn->UpdateProgress( NumCompletedTasks, NumTotalTasks );
		GLog->Flush();

		ImportVolumeSamples();
		ImportPrecomputedVisibility();
		ImportDominantShadowInfo();
		ImportMeshAreaLightData();
		ImportVolumeDistanceFieldData();

		if ( !bUseDeterministicLighting )
		{
			// Import and Process any mappings that are completed so far.
			ImportMappings(TRUE);
		}
		else if (bImportCompletedMappingsImmediately)
		{
			ImportMappings(FALSE);
			if (bProcessCompletedMappingsImmediately)
			{
				ProcessAvailableMappings(FALSE);
			}
		}

		// If this loop was executed with all tasks completed, then we're done
		if (bAllTaskAreComplete)
		{
			break;
		}
	}
	bRunningLightmass = FALSE;
	
	Statistics.LightmassTime += appSeconds() - LightmassStartTime;
	DOUBLE ImportStartTime = appSeconds();
	DOUBLE OriginalApplyTime = Statistics.ApplyTimeInProcessing;

	if ( !bProcessingFailed && !GEditor->GetMapBuildCancelled() )
	{
		ImportVolumeSamples();
		ImportPrecomputedVisibility();
		ImportDominantShadowInfo();
		ImportMeshAreaLightData();
		ImportVolumeDistanceFieldData();

		if (!bUseDeterministicLighting)
		{
			// Import and Process any outstanding completed mappings.
			ImportMappings(TRUE);
		}
		else 
		{
			if (bImportCompletedMappingsImmediately)
			{
				// Import any outstanding completed mappings.
				ImportMappings(FALSE);
				if (bProcessCompletedMappingsImmediately)
				{
					ProcessAvailableMappings(TRUE);
				}
			}
		}
		ApplyPrecomputedVisibility();
	}
	CompletedMappingTasks.Clear();
	CompletedVisibilityTasks.Clear();
	CompletedDominantShadowTasks.Clear();

	DOUBLE ApplyTimeDelta = Statistics.ApplyTimeInProcessing - OriginalApplyTime;
	Statistics.ImportTimeInProcessing += appSeconds() - ImportStartTime - ApplyTimeDelta;

	// Wait for the final success or failure message after everything is complete.
	// If one if these conditions evaluated true above, we've either been canceled
	// or the job has failed.
	while ( !bQuitReceived && !bProcessingSuccessful && !bProcessingFailed && !GEditor->GetMapBuildCancelled() )
	{
		appSleep( 0.050f );
	}

	// A final update on the lighting build warnings and errors dialog, now that everything is finished
	GWarn->LightingBuild_Refresh();

	UBOOL bShowLightingBuildInfo = FALSE;
	GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("ShowLightingBuildInfo"), bShowLightingBuildInfo, GEditorUserSettingsIni );
	if( bShowLightingBuildInfo )
	{
		GWarn->LightingBuildInfo_Show();
	}

	return bProcessingSuccessful;
}

/**
 * Import all mappings that have been completed so far.
 *	@param	bProcessImmediately		If TRUE, immediately process the mapping
 *									If FALSE, store it off for later processing
 */
void FLightmassProcessor::ImportMappings(UBOOL bProcessImmediately)
{
	// This will return a list of all the completed Guids
	TList<FGuid>* Element = CompletedMappingTasks.ExtractAll();

	// Reverse the list, so we have the mappings in the same order that they came in
	TList<FGuid>* PrevElement = NULL;
	TList<FGuid>* NextElement = NULL;
	while (Element)
	{
		NextElement = Element->Next;
		Element->Next = PrevElement;
		PrevElement = Element;
		Element = NextElement;
	}
	// Assign the new head of the list
	Element = PrevElement;

	while ( Element )
	{
		TList<FGuid>* NextElement = Element->Next;
		ImportMapping( Element->Element, bProcessImmediately );
		delete Element;
		Element = NextElement;
	}
}

/** Imports volume lighting samples from Lightmass and adds them to the appropriate levels. */
void FLightmassProcessor::ImportVolumeSamples()
{
	if (VolumeSampleTaskCompleted > 0)
	{
		{
			checkAtCompileTime(sizeof(FDebugVolumeLightingSample) == sizeof(Lightmass::FDebugVolumeLightingSample), DebugTypeSizesMustMatch);
			const FString ChannelName = Lightmass::CreateChannelName(Lightmass::VolumeLightingDebugOutputGuid, Lightmass::LM_VOLUMEDEBUGOUTPUT_VERSION, Lightmass::LM_VOLUMEDEBUGOUTPUT_EXTENSION);
			const INT Channel = Swarm.OpenChannel( *ChannelName, LM_VOLUMEDEBUGOUTPUT_CHANNEL_FLAGS );
			if (Channel >= 0)
			{
				ReadArray(Channel, GDebugStaticLightingInfo.VolumeLightingSamples);
				Swarm.CloseChannel(Channel);
			}
		}

		const FString ChannelName = Lightmass::CreateChannelName(Lightmass::PrecomputedVolumeLightingGuid, Lightmass::LM_VOLUMESAMPLES_VERSION, Lightmass::LM_VOLUMESAMPLES_EXTENSION);
		const INT Channel = Swarm.OpenChannel( *ChannelName, LM_VOLUMESAMPLES_CHANNEL_FLAGS );
		if (Channel >= 0)
		{
			FVector4 VolumeCenter;
			Swarm.ReadChannel(Channel, &VolumeCenter, sizeof(VolumeCenter));
			FVector4 VolumeExtent;
			Swarm.ReadChannel(Channel, &VolumeExtent, sizeof(VolumeExtent));

			if (!GWorld->PersistentLevel->PrecomputedLightVolume)
			{
				GWorld->PersistentLevel->PrecomputedLightVolume = new FPrecomputedLightVolume();
			}
			GWorld->PersistentLevel->PrecomputedLightVolume->Initialize(FBox(VolumeCenter - VolumeExtent, VolumeCenter + VolumeExtent));

			INT NumVolumeSampleArrays;
			Swarm.ReadChannel(Channel, &NumVolumeSampleArrays, sizeof(NumVolumeSampleArrays));
			for (INT ArrayIndex = 0; ArrayIndex < NumVolumeSampleArrays; ArrayIndex++)
			{
				INT LevelId;
				Swarm.ReadChannel(Channel, &LevelId, sizeof(LevelId));
				TArray<Lightmass::FVolumeLightingSampleData> VolumeSamples;
				ReadArray(Channel, VolumeSamples);
				check(LevelId < GWorld->Levels.Num());
				ULevel* CurrentLevel = (LevelId == INDEX_NONE || LevelId == 0) ? GWorld->PersistentLevel : GWorld->Levels(LevelId);
				if (CurrentLevel != GWorld->PersistentLevel)
				{
					if (!CurrentLevel->PrecomputedLightVolume)
					{
						CurrentLevel->PrecomputedLightVolume = new FPrecomputedLightVolume();
					}
					CurrentLevel->PrecomputedLightVolume->Initialize(FBox(VolumeCenter - VolumeExtent, VolumeCenter + VolumeExtent));
				}
				for (INT SampleIndex = 0; SampleIndex < VolumeSamples.Num(); SampleIndex++)
				{
					const Lightmass::FVolumeLightingSampleData& CurrentSample = VolumeSamples(SampleIndex);
					FVector4 SamplePositionAndRadius;
					Copy(CurrentSample.PositionAndRadius, SamplePositionAndRadius);

					FVector4 IndirectDirection;
					Copy(CurrentSample.IndirectDirection, IndirectDirection);
					FVector4 EnvironmentDirection;
					Copy(CurrentSample.EnvironmentDirection, EnvironmentDirection);
					FColor IndirectRadiance;
					Copy(CurrentSample.IndirectRadiance, IndirectRadiance);
					FColor EnvironmentRadiance;
					Copy(CurrentSample.EnvironmentRadiance, EnvironmentRadiance);
					FColor AmbientRadiance;
					Copy(CurrentSample.AmbientRadiance, AmbientRadiance);

					CurrentLevel->PrecomputedLightVolume->AddLightingSample(
						FVolumeLightingSample(
							SamplePositionAndRadius, 
							IndirectDirection, 
							EnvironmentDirection,
							IndirectRadiance,
							EnvironmentRadiance,
							AmbientRadiance,
							CurrentSample.bShadowedFromDominantLights));
				}
				if (CurrentLevel != GWorld->PersistentLevel)
				{
					CurrentLevel->PrecomputedLightVolume->FinalizeSamples();
				}
			}

			GWorld->PersistentLevel->PrecomputedLightVolume->FinalizeSamples();

			Swarm.CloseChannel(Channel);
		}
		else
		{
			debugf( TEXT("Error, OpenChannel failed to open %s with error code %d"), *ChannelName, Channel );
		}
		appInterlockedExchange(&VolumeSampleTaskCompleted, 0);
	}
}

/** Imports precomputed visibility */
void FLightmassProcessor::ImportPrecomputedVisibility()
{
	TList<FGuid>* Element = CompletedVisibilityTasks.ExtractAll();

	// Reverse the list, so we have the tasks in the same order that they came in
	TList<FGuid>* PrevElement = NULL;
	TList<FGuid>* NextElement = NULL;
	while (Element)
	{
		NextElement = Element->Next;
		Element->Next = PrevElement;
		PrevElement = Element;
		Element = NextElement;
	}
	// Assign the new head of the list
	Element = PrevElement;

	while ( Element )
	{
		// If this task has not already been imported, import it now
		TList<FGuid>* NextElement = Element->Next;

		Lightmass::FGuid LMGuid;
		Copy(Element->Element, LMGuid);
		const FString ChannelName = Lightmass::CreateChannelName(LMGuid, Lightmass::LM_PRECOMPUTEDVISIBILITY_VERSION, Lightmass::LM_PRECOMPUTEDVISIBILITY_EXTENSION);
		const INT Channel = Swarm.OpenChannel( *ChannelName, LM_PRECOMPUTEDVISIBILITY_CHANNEL_FLAGS );
		if (Channel >= 0)
		{
			// Find the index of this visibility task in VisibilityBucketGuids
			const INT ArrayIndex = Exporter->VisibilityBucketGuids.FindItemIndex(Element->Element);
			check(ArrayIndex >= 0);

			if (CompletedPrecomputedVisibilityCells.Num() == 0)
			{
				CompletedPrecomputedVisibilityCells.AddZeroed(Exporter->VisibilityBucketGuids.Num());
			}
			
			INT NumCells = 0;
			Swarm.ReadChannel(Channel, &NumCells, sizeof(NumCells));

			for (INT CellIndex = 0; CellIndex < NumCells; CellIndex++)
			{
				FBox Bounds;
				Lightmass::FBox LMBounds;
				Swarm.ReadChannel(Channel, &LMBounds, sizeof(LMBounds));
				Copy(LMBounds, Bounds);

				// Use the same index for this task guid as it has in VisibilityBucketGuids, so that visibility cells are processed in a deterministic order
				CompletedPrecomputedVisibilityCells(ArrayIndex).AddZeroed();
				FUncompressedPrecomputedVisibilityCell& CurrentCell = CompletedPrecomputedVisibilityCells(ArrayIndex).Last();
				CurrentCell.Bounds = Bounds;
				ReadArray(Channel, CurrentCell.VisibilityData);
			}

			TArray<FDebugStaticLightingRay> DebugRays;
			ReadArray(Channel, DebugRays);
			GDebugStaticLightingInfo.PrecomputedVisibilityRays.Append(DebugRays);

			Swarm.CloseChannel(Channel);
		}
		else
		{
			debugf( TEXT("Error, OpenChannel failed to open %s with error code %d"), *ChannelName, Channel );
		}

		delete Element;
		Element = NextElement;
	}
}

static UBOOL IsMeshVisible(const TArray<BYTE>& VisibilityData, INT MeshId)
{
	return (VisibilityData(MeshId / 8) & 1 << (MeshId % 8)) != 0;
}

static INT AccumulateVisibility(const TArray<BYTE>& OtherCellData, TArray<BYTE>& CellData)
{
	INT NumAdded = 0;

	checkSlow(OtherCellData.Num() == CellData.Num());
	for (INT i = 0; i < OtherCellData.Num(); i++)
	{
		if (OtherCellData(i) != 0)
		{
			for (INT BitIndex = 0; BitIndex < 8; BitIndex++)
			{
				NumAdded += IsMeshVisible(OtherCellData, i * 8 + BitIndex) && !IsMeshVisible(CellData, i * 8 + BitIndex) ? 1 : 0;
			}
		}

		CellData(i) |= OtherCellData(i);
	}
	return NumAdded;
}

void FLightmassProcessor::ApplyPrecomputedVisibility()
{
	TArray<FUncompressedPrecomputedVisibilityCell> CombinedPrecomputedVisibilityCells;
	for (INT ArrayIndex = 0; ArrayIndex < CompletedPrecomputedVisibilityCells.Num(); ArrayIndex++)
	{
		CombinedPrecomputedVisibilityCells.Append(CompletedPrecomputedVisibilityCells(ArrayIndex));
	}
	CompletedPrecomputedVisibilityCells.Empty();

	if (CombinedPrecomputedVisibilityCells.Num() > 0)
	{
		const DOUBLE StartTime = appSeconds();
		INT VisibilitySpreadingIterations;

		const TCHAR* AggressivenessSectionNames[VIS_Max] = {
			TEXT("DevOptions.PrecomputedVisibility"), 
			TEXT("DevOptions.PrecomputedVisibilityModeratelyAggressive"), 
			TEXT("DevOptions.PrecomputedVisibilityMostAggressive")};
		const TCHAR* ActiveSection = AggressivenessSectionNames[GWorld->GetWorldInfo()->VisibilityAggressiveness];
		verify(GConfig->GetInt(ActiveSection, TEXT("VisibilitySpreadingIterations"), VisibilitySpreadingIterations, GLightmassIni));
		UBOOL bCompressVisibilityData;
		verify(GConfig->GetBool(TEXT("DevOptions.PrecomputedVisibility"), TEXT("bCompressVisibilityData"), bCompressVisibilityData, GLightmassIni));
		const FLOAT CellSize = GWorld->GetWorldInfo()->VisibilityCellSize;
		FLOAT PlayAreaHeight = 0;
		verify(GConfig->GetFloat(TEXT("DevOptions.PrecomputedVisibility"), TEXT("PlayAreaHeight"), PlayAreaHeight, GLightmassIni));
		INT CellBucketSize = 0;
		verify(GConfig->GetInt(TEXT("DevOptions.PrecomputedVisibility"), TEXT("CellRenderingBucketSize"), CellBucketSize, GLightmassIni));
		INT NumCellBuckets = 0;
		verify(GConfig->GetInt(TEXT("DevOptions.PrecomputedVisibility"), TEXT("NumCellRenderingBuckets"), NumCellBuckets, GLightmassIni));

		INT TotalNumQueries = 0;
		INT QueriesVisibleFromSpreadingNeighbors = 0;
		for (INT IterationIndex = 0; IterationIndex < VisibilitySpreadingIterations; IterationIndex++)
		{
			// Copy the original data since we read from outside the current cell
			TArray<FUncompressedPrecomputedVisibilityCell> OriginalPrecomputedVisibilityCells(CombinedPrecomputedVisibilityCells);
			for (INT CellIndex = 0; CellIndex < CombinedPrecomputedVisibilityCells.Num(); CellIndex++)
			{
				FUncompressedPrecomputedVisibilityCell& CurrentCell = CombinedPrecomputedVisibilityCells(CellIndex);
				TotalNumQueries += CurrentCell.VisibilityData.Num() * 8;
				for (INT OtherCellIndex = 0; OtherCellIndex < OriginalPrecomputedVisibilityCells.Num(); OtherCellIndex++)
				{
					const FUncompressedPrecomputedVisibilityCell& OtherCell = OriginalPrecomputedVisibilityCells(OtherCellIndex);
					// Determine whether the cell is a world space neighbor
					if (CellIndex != OtherCellIndex
						&& Abs(CurrentCell.Bounds.Min.X - OtherCell.Bounds.Min.X) < CellSize + KINDA_SMALL_NUMBER
						&& Abs(CurrentCell.Bounds.Min.Y - OtherCell.Bounds.Min.Y) < CellSize + KINDA_SMALL_NUMBER
						// Don't spread from cells below, they're probably below the ground and see too much
						&& OtherCell.Bounds.Min.Z - CurrentCell.Bounds.Min.Z > -PlayAreaHeight * 0.5f
						// Only spread from one cell above
						&& OtherCell.Bounds.Min.Z - CurrentCell.Bounds.Min.Z < PlayAreaHeight * 1.5f)
					{
						// Combine the neighbor's visibility with the current cell's visibility
						// This reduces visibility errors at the cost of less effective culling
						QueriesVisibleFromSpreadingNeighbors += AccumulateVisibility(OtherCell.VisibilityData, CurrentCell.VisibilityData);
					}
				}
			}
		}	

		const FVector2D CellBucketOriginXY(CombinedPrecomputedVisibilityCells(0).Bounds.Min.X, CombinedPrecomputedVisibilityCells(0).Bounds.Min.Y);

		TArray<TArray<const FUncompressedPrecomputedVisibilityCell*> > CellRenderingBuckets;
		CellRenderingBuckets.Empty(NumCellBuckets * NumCellBuckets);
		CellRenderingBuckets.AddZeroed(NumCellBuckets * NumCellBuckets);
		SIZE_T UncompressedSize = 0;
		// Sort the cells into buckets based on their position
		//@todo - sort cells inside buckets based on locality, to reduce visibility cache misses
		for (INT CellIndex = 0; CellIndex < CombinedPrecomputedVisibilityCells.Num(); CellIndex++)
		{
			const FUncompressedPrecomputedVisibilityCell& CurrentCell = CombinedPrecomputedVisibilityCells(CellIndex);

			const FLOAT FloatOffsetX = (CurrentCell.Bounds.Min.X - CellBucketOriginXY.X + .5f * CellSize) / CellSize;
			// appTrunc rounds toward 0, we want to always round down
			const INT BucketIndexX = Abs((appTrunc(FloatOffsetX) - (FloatOffsetX < 0.0f ? 1 : 0)) / CellBucketSize % NumCellBuckets);
			const FLOAT FloatOffsetY = (CurrentCell.Bounds.Min.Y - CellBucketOriginXY.Y + .5f * CellSize) / CellSize;
			const INT BucketIndexY = Abs((appTrunc(FloatOffsetY) - (FloatOffsetY < 0.0f ? 1 : 0)) / CellBucketSize % NumCellBuckets);

			const INT BucketIndex = BucketIndexY * CellBucketSize + BucketIndexX;
			CellRenderingBuckets(BucketIndex).AddItem(&CurrentCell);
			UncompressedSize += CurrentCell.VisibilityData.Num();
		}

		GWorld->PersistentLevel->MarkPackageDirty();

		// Set all the level parameters needed to access visibility
		GWorld->PersistentLevel->PrecomputedVisibilityHandler.PrecomputedVisibilityCellBucketOriginXY = CellBucketOriginXY;
		GWorld->PersistentLevel->PrecomputedVisibilityHandler.PrecomputedVisibilityCellSizeXY = CellSize;
		GWorld->PersistentLevel->PrecomputedVisibilityHandler.PrecomputedVisibilityCellSizeZ = PlayAreaHeight;
		GWorld->PersistentLevel->PrecomputedVisibilityHandler.PrecomputedVisibilityCellBucketSizeXY = CellBucketSize;
		GWorld->PersistentLevel->PrecomputedVisibilityHandler.PrecomputedVisibilityNumCellBuckets = NumCellBuckets;
		GWorld->PersistentLevel->PrecomputedVisibilityHandler.PrecomputedVisibilityCellBuckets.Empty(NumCellBuckets * NumCellBuckets);
		GWorld->PersistentLevel->PrecomputedVisibilityHandler.PrecomputedVisibilityCellBuckets.AddZeroed(NumCellBuckets * NumCellBuckets);

		// Split visibility data into ~32Kb chunks, to limit decompression time
		const INT ChunkSizeTarget = 32 * 1024;
		TArray<BYTE> UncompressedVisibilityData;
		SIZE_T TotalCompressedSize = 0;
		for (INT BucketIndex = 0; BucketIndex < CellRenderingBuckets.Num(); BucketIndex++)
		{
			FPrecomputedVisibilityBucket& OutputBucket = GWorld->PersistentLevel->PrecomputedVisibilityHandler.PrecomputedVisibilityCellBuckets(BucketIndex);
			OutputBucket.CellDataSize = CombinedPrecomputedVisibilityCells(0).VisibilityData.Num();
			INT ChunkIndex = 0;
			UncompressedVisibilityData.Reset();
			for (INT CellIndex = 0; CellIndex < CellRenderingBuckets(BucketIndex).Num(); CellIndex++)
			{	
				const FUncompressedPrecomputedVisibilityCell& CurrentCell = *CellRenderingBuckets(BucketIndex)(CellIndex);
				FPrecomputedVisibilityCell NewCell;
				NewCell.Min = CurrentCell.Bounds.Min;
				// We're only storing Min per cell with a shared SizeXY and SizeZ for reduced memory storage
				//checkSlow(CurrentCell.Bounds.Max.Equals(CurrentCell.Bounds.Min + FVector(CellSize, CellSize, PlayAreaHeight), KINDA_SMALL_NUMBER * 10.0f));
				NewCell.ChunkIndex = ChunkIndex;
				NewCell.DataOffset = UncompressedVisibilityData.Num();
				OutputBucket.Cells.AddItem(NewCell);
				UncompressedVisibilityData.Append(CurrentCell.VisibilityData);
				// Create a new chunk if we've reached the size limit or this is the last cell in a bucket
				if (UncompressedVisibilityData.Num() > ChunkSizeTarget || CellIndex == CellRenderingBuckets(BucketIndex).Num() - 1)
				{
					// Don't compress small amounts of data because appCompressMemory will fail
					if (bCompressVisibilityData && UncompressedVisibilityData.Num() > 32)
					{
						TArray<BYTE> TempCompressionOutput;
						// Compressed output can be larger than the input, so we use temporary storage to hold the compressed output for now
						TempCompressionOutput.Empty(UncompressedVisibilityData.Num() * 4 / 3);
						TempCompressionOutput.Add(UncompressedVisibilityData.Num() * 4 / 3);
						INT CompressedSize = TempCompressionOutput.Num();
						verify(appCompressMemory(
							// Using zlib since it is supported on all platforms, otherwise we would need to compress on cook
							(ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory), 
							TempCompressionOutput.GetData(), 
							CompressedSize, 
							UncompressedVisibilityData.GetData(), 
							UncompressedVisibilityData.Num()));

						OutputBucket.CellDataChunks.AddZeroed();
						FCompressedVisibilityChunk& NewChunk = OutputBucket.CellDataChunks.Last();
						NewChunk.UncompressedSize = UncompressedVisibilityData.Num();
						NewChunk.bCompressed = TRUE;
						NewChunk.Data.Empty(CompressedSize);
						NewChunk.Data.Add(CompressedSize);
						appMemcpy(NewChunk.Data.GetData(), TempCompressionOutput.GetData(), CompressedSize);
						ChunkIndex++;
						TotalCompressedSize += CompressedSize;
						UncompressedVisibilityData.Reset();
					}
					else
					{
						OutputBucket.CellDataChunks.AddZeroed();
						FCompressedVisibilityChunk& NewChunk = OutputBucket.CellDataChunks.Last();
						NewChunk.UncompressedSize = UncompressedVisibilityData.Num();
						NewChunk.bCompressed = FALSE;
						NewChunk.Data = UncompressedVisibilityData;
						ChunkIndex++;
						TotalCompressedSize += UncompressedVisibilityData.Num();
						UncompressedVisibilityData.Reset();
					}
				}
			}
		}

		GWorld->PersistentLevel->PrecomputedVisibilityHandler.UpdateVisibilityStats(TRUE);

		warnf(NAME_DevLightmassSolver, TEXT("ApplyPrecomputedVisibility %.1fs, %.1f%% of all queries changed to visible from spreading neighbors, compressed %.3fMb to %.3fMb (%.1f ratio) with %u rendering buckets"),
			appSeconds() - StartTime,
			100.0f * QueriesVisibleFromSpreadingNeighbors / TotalNumQueries,
			UncompressedSize / 1024.0f / 1024.0f,
			TotalCompressedSize / 1024.0f / 1024.0f,
			UncompressedSize / (FLOAT)TotalCompressedSize,
			CellRenderingBuckets.Num());
	}
	else
	{
		GWorld->PersistentLevel->PrecomputedVisibilityHandler.Invalidate(GWorld->Scene);
	}
}

/** Imports dominant shadow information from Lightmass. */
void FLightmassProcessor::ImportDominantShadowInfo()
{
	TList<FGuid>* Element = CompletedDominantShadowTasks.ExtractAll();

	// Reverse the list, so we have the tasks in the same order that they came in
	TList<FGuid>* PrevElement = NULL;
	TList<FGuid>* NextElement = NULL;
	while (Element)
	{
		NextElement = Element->Next;
		Element->Next = PrevElement;
		PrevElement = Element;
		Element = NextElement;
	}
	// Assign the new head of the list
	Element = PrevElement;

	while ( Element )
	{
		// If this task has not already been imported, import it now
		TList<FGuid>* NextElement = Element->Next;
		
		ULightComponent* DominantLightComponent = DominantShadowTaskLights.FindRef(Element->Element);
		if (DominantLightComponent)
		{
			Lightmass::FGuid TaskGuid;
			Copy(DominantLightComponent->LightmapGuid, TaskGuid);
			const FString ChannelName = Lightmass::CreateChannelName(TaskGuid, Lightmass::LM_DOMINANTSHADOW_VERSION, Lightmass::LM_DOMINANTSHADOW_EXTENSION);
			const INT Channel = Swarm.OpenChannel( *ChannelName, LM_DOMINANTSHADOW_CHANNEL_FLAGS );
			if (Channel >= 0)
			{
				Lightmass::FDominantLightShadowInfoData LMDominantLightShadowInfo;
				Swarm.ReadChannel(Channel, &LMDominantLightShadowInfo, sizeof(LMDominantLightShadowInfo));
				FDominantShadowInfo DominantLightShadowInfo;
				Copy(LMDominantLightShadowInfo, DominantLightShadowInfo);
				checkAtCompileTime(sizeof(WORD) == sizeof(Lightmass::FDominantLightShadowSampleData), DominantLightShadowSampleSizesMustMatch);
				TArray<WORD> DominantLightShadowMap;
				ReadArray(Channel, DominantLightShadowMap);
				Swarm.CloseChannel(Channel);

				UDominantDirectionalLightComponent* DirectionalComponent = Cast<UDominantDirectionalLightComponent>(DominantLightComponent);
				if (DirectionalComponent)
				{
					check(DirectionalComponent == GWorld->DominantDirectionalLight);
					GWorld->DominantDirectionalLight->Initialize(DominantLightShadowInfo, DominantLightShadowMap, bOnlyBuildVisibleLevels);
				}
				else
				{
					UDominantSpotLightComponent* SpotComponent = Cast<UDominantSpotLightComponent>(DominantLightComponent);
					check(SpotComponent);
					SpotComponent->Initialize(DominantLightShadowInfo, DominantLightShadowMap, bOnlyBuildVisibleLevels);
				}
			}
			else
			{
				debugf( TEXT("Error, OpenChannel failed to open %s with error code %d"), *ChannelName, Channel );
			}
		}
		else
		{
			warnf(TEXT("Received dominant shadow task completion but couldn't find the light!"));
		}

		delete Element;
		Element = NextElement;
	}
}

/** Imports data from Lightmass about the mesh area lights generated for the scene, and creates AGeneratedMeshAreaLight's for them. */
void FLightmassProcessor::ImportMeshAreaLightData()
{
	if (MeshAreaLightDataTaskCompleted > 0)
	{
		const FString ChannelName = Lightmass::CreateChannelName(Lightmass::MeshAreaLightDataGuid, Lightmass::LM_MESHAREALIGHTDATA_VERSION, Lightmass::LM_MESHAREALIGHTDATA_EXTENSION);
		const INT Channel = Swarm.OpenChannel( *ChannelName, LM_MESHAREALIGHT_CHANNEL_FLAGS );
		if (Channel >= 0)
		{
			INT NumMeshAreaLights = 0;
			Swarm.ReadChannel(Channel, &NumMeshAreaLights, sizeof(NumMeshAreaLights));
			for (INT LightIndex = 0; LightIndex < NumMeshAreaLights; LightIndex++)
			{
				Lightmass::FMeshAreaLightData LMCurrentLightData;
				Swarm.ReadChannel(Channel, &LMCurrentLightData, sizeof(LMCurrentLightData));
				check(LMCurrentLightData.LevelId < GWorld->Levels.Num());
				// Find the level that the mesh area light was in
				ULevel* CurrentLevel = (LMCurrentLightData.LevelId == INDEX_NONE || LMCurrentLightData.LevelId == 0) ? GWorld->PersistentLevel : GWorld->Levels(LMCurrentLightData.LevelId);
				FVector4 Position;
				Copy(LMCurrentLightData.Position, Position);
				FVector4 Direction;
				Copy(LMCurrentLightData.Direction, Direction);
				if (CurrentLevel && CurrentLevel->Actors.Num() > 0)
				{
					// Spawn a AGeneratedMeshAreaLight to handle the light's influence on dynamic objects
					AGeneratedMeshAreaLight* NewGeneratedLight = 
						CastChecked<AGeneratedMeshAreaLight>(GWorld->SpawnActor(AGeneratedMeshAreaLight::StaticClass(), NAME_None, Position, Direction.Rotation(), NULL, FALSE, FALSE, CurrentLevel->Actors(0)));
					USpotLightComponent* SpotComponent = CastChecked<USpotLightComponent>(NewGeneratedLight->LightComponent);
					// Detach the component before we change its attributes
					FComponentReattachContext Reattach(SpotComponent);
					// Setup spotlight properties to approximate a mesh area light
					SpotComponent->Radius = LMCurrentLightData.Radius;
					SpotComponent->OuterConeAngle = LMCurrentLightData.ConeAngle * 180.0f / PI;
					Copy(LMCurrentLightData.Color, SpotComponent->LightColor);
					SpotComponent->Brightness = LMCurrentLightData.Brightness;
					SpotComponent->FalloffExponent = LMCurrentLightData.FalloffExponent;
				}
			}
			Swarm.CloseChannel(Channel);
		}
		else
		{
			debugf( TEXT("Error, OpenChannel failed to open %s with error code %d"), *ChannelName, Channel );
		}
		appInterlockedExchange(&MeshAreaLightDataTaskCompleted, 0);
	}
}


/** Imports the volume distance field from Lightmass. */
void FLightmassProcessor::ImportVolumeDistanceFieldData()
{
	if (VolumeDistanceFieldTaskCompleted > 0)
	{
		const FString ChannelName = Lightmass::CreateChannelName(Lightmass::VolumeDistanceFieldGuid, Lightmass::LM_MESHAREALIGHTDATA_VERSION, Lightmass::LM_MESHAREALIGHTDATA_EXTENSION);
		const INT Channel = Swarm.OpenChannel( *ChannelName, LM_MESHAREALIGHT_CHANNEL_FLAGS );
		if (Channel >= 0)
		{
			FPrecomputedVolumeDistanceField& DistanceField = GWorld->PersistentLevel->PrecomputedVolumeDistanceField;
			Swarm.ReadChannel(Channel, &DistanceField.VolumeSizeX, sizeof(DistanceField.VolumeSizeX));
			Swarm.ReadChannel(Channel, &DistanceField.VolumeSizeY, sizeof(DistanceField.VolumeSizeY));
			Swarm.ReadChannel(Channel, &DistanceField.VolumeSizeZ, sizeof(DistanceField.VolumeSizeZ));
			Swarm.ReadChannel(Channel, &DistanceField.VolumeMaxDistance, sizeof(DistanceField.VolumeMaxDistance));
			
			FVector4 BoxMin;
			Swarm.ReadChannel(Channel, &BoxMin, sizeof(BoxMin));
			FVector4 BoxMax;
			Swarm.ReadChannel(Channel, &BoxMax, sizeof(BoxMax));
			DistanceField.VolumeBox = FBox(BoxMin, BoxMax);

			ReadArray(Channel, DistanceField.Data);

			Swarm.CloseChannel(Channel);
		}
		else
		{
			debugf( TEXT("Error, OpenChannel failed to open %s with error code %d"), *ChannelName, Channel );
		}
		appInterlockedExchange(&VolumeDistanceFieldTaskCompleted, 0);
	}
}

/**
 *	Import the texture mapping 
 *	@param	TextureMapping			The mapping being imported.
 *	@param	bProcessImmediately		If TRUE, immediately process the mapping
 *									If FALSE, store it off for later processing
 */
void FLightmassProcessor::ImportStaticLightingTextureMapping( const FGuid& MappingGuid, UBOOL bProcessImmediately )
{
	Lightmass::FGuid LightmassGuid;
	Copy( MappingGuid, LightmassGuid );
	FString ChannelName = Lightmass::CreateChannelName(LightmassGuid, Lightmass::LM_TEXTUREMAPPING_VERSION, Lightmass::LM_TEXTUREMAPPING_EXTENSION);

	// We need to check if there's a channel with this name for each completed mapping,
	// even if the mapping has been imported as part of a previous channel.
	// Example:
	// 1. If the remote agent gets reassigned, it might have written out a merged channel
	//    (mappings A, B, C and D in one channel) but only sent out a "completed" message for
	//    some of the mappings (e.g. A and B).
	// 2. UE3 imports A, B, C and D when it receives the "completed" message for A.
	// 3. A new remote agent will process C, D and some new mappings E and F, and write out
	//    a merged channel named "C", containing C, D, E, F.
	// 4. UE3 must now read the "C" channel - even if C has been imported already - in order
	//    to import E and F.
	INT Channel = Swarm.OpenChannel( *ChannelName, LM_TEXTUREMAPPING_CHANNEL_FLAGS );
	if (Channel >= 0)
	{
		// Read in how many mappings this channel contains
		UINT MappingsImported = 0;
		UINT NumMappings = 0;
		Swarm.ReadChannel(Channel, &NumMappings, sizeof(NumMappings));

		// Read in each of the mappings
		while (MappingsImported != NumMappings)
		{
			// Read in the next GUID and look up its mapping
			FGuid NextMappingGuid;
			Swarm.ReadChannel(Channel, &NextMappingGuid, sizeof(FGuid));
			FStaticLightingTextureMapping* TextureMapping = GetStaticLightingTextureMapping(NextMappingGuid);

			// If we don't have a mapping pending, check to see if we've already imported
			// it which can *possibly* happen if a disconnection race condition occured
			// where we got the results for a task, but didn't get the message that it
			// had finished before we re-queued/re-assigned the task to another agent,
			// which could result in duplicate results. If we get a duplicate, just
			// re-import the redundant results.
			UBOOL bReimporting = FALSE;
			if ( TextureMapping == NULL )
			{
				// Remove the mapping from ImportedMappings and re-import it.
				FMappingImportHelper** pImportData = ImportedMappings.Find( NextMappingGuid );
				check( pImportData && *pImportData && (*pImportData)->Type == SLT_Texture );
				FTextureMappingImportHelper* pTextureImportData = (*pImportData)->GetTextureMappingHelper();
				TextureMapping = pTextureImportData->TextureMapping;
				bReimporting = TRUE;
				if ( GLightmassStatsMode )
				{
					debugf(TEXT("Re-importing texture mapping: %s"), *NextMappingGuid.String());
				}
			}

			if( ensureMsgf( (TextureMapping != NULL), TEXT("Opened mapping channel %s to Swarm, then tried to find texture mapping %s (number %d of %d) and failed."), *MappingGuid.String(), *NextMappingGuid.String(), MappingsImported, NumMappings) )
			{
				debugfSlow(TEXT("Importing %32s %s"), *(TextureMapping->GetDescription()), *(TextureMapping->GetLightingGuid().String()));

				// If we are importing the debug mapping, first read in the debug output channel
				if (NextMappingGuid == DebugMappingGuid)
				{
					ImportDebugOutput();
				}

				FTextureMappingImportHelper* ImportData = new FTextureMappingImportHelper();
				ImportData->TextureMapping = TextureMapping;
				ImportData->MappingGuid = NextMappingGuid;
				if (ImportTextureMapping(Channel, *ImportData) == TRUE)
				{
					if ( bReimporting == FALSE )
					{
						ImportedMappings.Set(ImportData->MappingGuid, ImportData);
					}
					if (bProcessImmediately)
					{
						ProcessMapping(ImportData->MappingGuid);
					}
				}
				else
				{
					warnf(NAME_Warning, TEXT("Failed to import texture mapping results!"));
				}

				// Completed this mapping, increment
				MappingsImported++;
			}
			else
			{
				// Report an error for this mapping
				UObject* Object = NULL;
				const FStaticLightingMapping* FoundMapping = Exporter->FindMappingByGuid(NextMappingGuid);
				if (FoundMapping)
				{
					Object = FoundMapping->GetMappedObject();
				}
				GWarn->LightingBuild_Add(MCTYPE_CRITICALERROR, Object, *Localize(TEXT("Lightmass"), TEXT("LightmassError_LightingBuildError"), TEXT("UnrealEd")));

				// We can't trust the rest of this file, so we'll need to bail now
				break;
			}
		}
		Swarm.CloseChannel( Channel );
	}
	// File not found?
	else if ( Channel == NSwarm::SWARM_ERROR_CHANNEL_NOT_FOUND )
	{
		// If the channel doesn't exist, then this mapping must've been part of another channel that has already been imported.
		check( ImportedMappings.HasKey( MappingGuid ) );
		FStaticLightingTextureMapping* TextureMapping = GetStaticLightingTextureMapping(MappingGuid);
	}
	// Other error
	else
	{
		debugf( TEXT("Error, OpenChannel failed to open %s with error code %d"), *ChannelName, Channel );
	}
}

/**
 *	Import the vertex mapping 
 *	@param	VertexMapping			The mapping being imported.
 *	@param	bProcessImmediately		If TRUE, immediately process the mapping
 *									If FALSE, store it off for later processing
 */
void FLightmassProcessor::ImportStaticLightingVertexMapping( const FGuid& MappingGuid, UBOOL bProcessImmediately )
{
	Lightmass::FGuid LightmassGuid;
	Copy( MappingGuid, LightmassGuid );
	FString ChannelName = Lightmass::CreateChannelName(LightmassGuid, Lightmass::LM_VERTEXMAPPING_VERSION, Lightmass::LM_VERTEXMAPPING_EXTENSION);

	// Note: See the corresponding comment in FLightmassProcessor::ImportStaticLightingTextureMapping()
	INT Channel = Swarm.OpenChannel( *ChannelName, LM_VERTEXMAPPING_CHANNEL_FLAGS );
	if (Channel >= 0)
	{
		// Read in how many mappings this channel contains
		UINT MappingsImported = 0;
		UINT NumMappings = 0;
		Swarm.ReadChannel(Channel, &NumMappings, sizeof(NumMappings));

		// Read in each of the mappings
		while (MappingsImported != NumMappings)
		{
			// Read in the next GUID and look up its mapping
			FGuid NextMappingGuid;
			Swarm.ReadChannel(Channel, &NextMappingGuid, sizeof(FGuid));
			FStaticLightingVertexMapping* VertexMapping = GetStaticLightingVertexMapping(NextMappingGuid);

			// If we don't have a mapping pending, check to see if we've already imported
			// it which can *possibly* happen if a disconnection race condition occured
			// where we got the results for a task, but didn't get the message that it
			// had finished before we re-queued/re-assigned the task to another agent,
			// which could result in duplicate results. If we get a duplicate, just
			// re-import the redundant results.
			UBOOL bReimporting = FALSE;
			if ( VertexMapping == NULL )
			{
				// Remove the mapping from ImportedMappings and re-import it.
				FMappingImportHelper** pImportData = ImportedMappings.Find( NextMappingGuid );
				check( pImportData && *pImportData && (*pImportData)->Type == SLT_Vertex );
				FVertexMappingImportHelper* pVertexImportData = (*pImportData)->GetVertexMappingHelper();
				VertexMapping = pVertexImportData->VertexMapping;
				bReimporting = TRUE;
				if ( GLightmassStatsMode )
				{
					debugf(TEXT("Re-importing vertex mapping: %s"), *NextMappingGuid.String());
				}
			}

			if( ensureMsgf( (VertexMapping != NULL), TEXT("Opened mapping channel %s to Swarm, then tried to find vertex mapping %s (number %d of %d) and failed."), *MappingGuid.String(), *NextMappingGuid.String(), MappingsImported, NumMappings) )
			{
				debugfSlow(TEXT("Importing %32s %s"), *(VertexMapping->GetDescription()), *(VertexMapping->GetLightingGuid().String()));

				// If we are importing the debug mapping, first read in the debug output channel
				if (NextMappingGuid == DebugMappingGuid)
				{
					ImportDebugOutput();
				}

				FVertexMappingImportHelper* ImportData = new FVertexMappingImportHelper();
				ImportData->VertexMapping = VertexMapping;
				ImportData->MappingGuid = NextMappingGuid;
				if (ImportVertexMapping(Channel, *ImportData) == TRUE)
				{
					if ( bReimporting == FALSE )
					{
						ImportedMappings.Set(ImportData->MappingGuid, ImportData);
					}
					if (bProcessImmediately)
					{
						ProcessMapping(ImportData->MappingGuid);
					}
				}
				else
				{
					warnf(NAME_Warning, TEXT("Failed to import vertex mapping results!"));
				}

				// Completed this mapping, increment
				MappingsImported++;
			}
			else
			{
				// Report an error for this mapping
				UObject* Object = NULL;
				const FStaticLightingMapping* FoundMapping = Exporter->FindMappingByGuid(NextMappingGuid);
				if (FoundMapping)
				{
					Object = FoundMapping->GetMappedObject();
				}
				GWarn->LightingBuild_Add(MCTYPE_CRITICALERROR, Object, *Localize(TEXT("Lightmass"), TEXT("LightmassError_LightingBuildError"), TEXT("UnrealEd")));

				// We can't trust the rest of this file, so we'll need to bail now
				break;
			}
		}
		Swarm.CloseChannel( Channel );
	}
	// File not found?
	else if ( Channel == NSwarm::SWARM_ERROR_CHANNEL_NOT_FOUND )
	{
		// If the channel doesn't exist, then this mapping must've been part of another channel that has already been imported.
		check( ImportedMappings.HasKey( MappingGuid ) );
	}
	// Other error
	else
	{
		debugf( TEXT("Error, OpenChannel failed to open %s with error code %d"), *ChannelName, Channel );
	}
}

/** Determines whether the specified mapping is a texture mapping */
UBOOL FLightmassProcessor::IsStaticLightingTextureMapping( const FGuid& MappingGuid )
{
	if (PendingBSPMappings.HasKey(MappingGuid))
	{
		return TRUE;
	}
	else if (PendingTextureMappings.HasKey(MappingGuid))
	{
		return TRUE;
	}
	else if (PendingTerrainMappings.HasKey(MappingGuid))
	{
		return TRUE;
	}
	else if (PendingFluidMappings.HasKey(MappingGuid))
	{
		return TRUE;
	}
	else if (PendingLandscapeMappings.HasKey(MappingGuid))
	{
		return TRUE;
	}
	else
	{
		FMappingImportHelper** pImportData = ImportedMappings.Find(MappingGuid);
		if ( pImportData && (*pImportData)->Type == SLT_Texture )
		{
			return TRUE;
		}
	}
	return FALSE;
}

/** Gets the texture mapping for the specified GUID */
FStaticLightingTextureMapping* FLightmassProcessor::GetStaticLightingTextureMapping( const FGuid& MappingGuid )
{
	FBSPSurfaceStaticLighting* BSPMapping = NULL;
	FStaticMeshStaticLightingTextureMapping* SMTextureMapping = NULL;
	FTerrainComponentStaticLighting* TerrainMapping = NULL;
	FFluidSurfaceStaticLightingTextureMapping* FluidMapping = NULL;
	FLandscapeStaticLightingTextureMapping* LandscapeMapping = NULL;

	if (PendingBSPMappings.RemoveAndCopyValue(MappingGuid, BSPMapping))
	{
		return BSPMapping->GetTextureMapping();
	}
	else if (PendingTextureMappings.RemoveAndCopyValue(MappingGuid, SMTextureMapping))
	{
		return SMTextureMapping->GetTextureMapping();
	}
	else if (PendingTerrainMappings.RemoveAndCopyValue(MappingGuid, TerrainMapping))
	{
		return TerrainMapping->GetTextureMapping();
	}
	else if (PendingFluidMappings.RemoveAndCopyValue(MappingGuid, FluidMapping))
	{
		return FluidMapping->GetTextureMapping();
	}
	else if (PendingLandscapeMappings.RemoveAndCopyValue(MappingGuid, LandscapeMapping))
	{
		return LandscapeMapping->GetTextureMapping();
	}
	return NULL;
}

/** Determines whether the specified mapping is a vertex mapping */
UBOOL FLightmassProcessor::IsStaticLightingVertexMapping( const FGuid& MappingGuid )
{
	if (PendingVertexMappings.HasKey(MappingGuid))
	{
		return TRUE;
	}
#if WITH_SPEEDTREE
	else if (PendingSpeedTreeMappings.HasKey(MappingGuid))
	{
		return TRUE;
	}
#endif	//#if WITH_SPEEDTREE
	else
	{
		FMappingImportHelper** pImportData = ImportedMappings.Find(MappingGuid);
		if ( pImportData && (*pImportData)->Type == SLT_Vertex )
		{
			return TRUE;
		}
	}
	return FALSE;
}

/** Gets the vertex mapping for the specified GUID */
FStaticLightingVertexMapping* FLightmassProcessor::GetStaticLightingVertexMapping( const FGuid& MappingGuid )
{
	FStaticMeshStaticLightingVertexMapping* SMVertexMapping = NULL;
#if WITH_SPEEDTREE
	FSpeedTreeStaticLightingMapping* SpeedTreeMapping = NULL;
#endif	//#if WITH_SPEEDTREE

	if ( PendingVertexMappings.RemoveAndCopyValue(MappingGuid, SMVertexMapping) )
	{
		return SMVertexMapping->GetVertexMapping();
	}
#if WITH_SPEEDTREE
	else if (PendingSpeedTreeMappings.RemoveAndCopyValue(MappingGuid, SpeedTreeMapping))
	{
		return SpeedTreeMapping->GetVertexMapping();
	}
#endif	//#if WITH_SPEEDTREE

	return NULL;
}

/**
 * Import the mapping specified by a Guid.
 *	@param MappingGuid				Guid that identifies a mapping
 *	@param	bProcessImmediately		If TRUE, immediately process the mapping
 *									If FALSE, store it off for later processing
 */
void FLightmassProcessor::ImportMapping( const FGuid& MappingGuid, UBOOL bProcessImmediately )
{
	DOUBLE ImportAndApplyStartTime = appSeconds();
	DOUBLE OriginalApplyTime = Statistics.ApplyTimeInProcessing;

	if (IsStaticLightingTextureMapping(MappingGuid))
	{
		ImportStaticLightingTextureMapping(MappingGuid, bProcessImmediately);
	}
	else if (IsStaticLightingVertexMapping(MappingGuid))
	{
		ImportStaticLightingVertexMapping(MappingGuid, bProcessImmediately);
	}
	else
	{
		FMappingImportHelper** pImportData = ImportedMappings.Find(MappingGuid);
		if ((pImportData == NULL) || (*pImportData == NULL))
		{
			warnf(TEXT("Mapping not found for %s"), *(MappingGuid.String()));
		}
	}

	if ( !bRunningLightmass )
	{
		DOUBLE ApplyTime = Statistics.ApplyTimeInProcessing - OriginalApplyTime;
		DOUBLE ImportTime = appSeconds() - ImportAndApplyStartTime - ApplyTime;
		Statistics.ImportTimeInProcessing += ImportTime;
	}
}

/**
 * Process the mapping specified by a Guid.
 * @param MappingGuid	Guid that identifies a mapping
 **/
void FLightmassProcessor::ProcessMapping( const FGuid& MappingGuid )
{
	DOUBLE ApplyStartTime = appSeconds();

	FMappingImportHelper** pImportData = ImportedMappings.Find(MappingGuid);
	if ( pImportData && *pImportData )
	{
		if ( (*pImportData)->bProcessed == FALSE )
		{
			FMappingImportHelper* ImportData = *pImportData;
			switch (ImportData->Type)
			{
			case SLT_Texture:
				{
					FTextureMappingImportHelper* TImportData = (FTextureMappingImportHelper*)ImportData;
					if(TImportData->TextureMapping)
					{
 						debugfSlow(TEXT("Processing %32s: %s"), *(TImportData->TextureMapping->GetDescription()), *(TImportData->TextureMapping->GetLightingGuid().String()));
						System.ApplyMapping(TImportData->TextureMapping, NULL, TImportData->ShadowMapData, TImportData->QuantizedData);
					}
					else
					{
						debugfSlow(TEXT("Processing texture mapping %s failed due to missing mapping!"), *MappingGuid.String());
					}
				}
				break;
			case SLT_Vertex:
				{
					FVertexMappingImportHelper* VImportData = (FVertexMappingImportHelper*)ImportData;
					if(VImportData->VertexMapping)
					{
						debugfSlow(TEXT("Processing %32s: %s"), *(VImportData->VertexMapping->GetDescription()), *(VImportData->VertexMapping->GetLightingGuid().String()));
						System.ApplyMapping(VImportData->VertexMapping, NULL, VImportData->ShadowMapData, VImportData->QuantizedData);
					}
					else
					{
						debugfSlow(TEXT("Processing vertex mapping %s failed due to missing mapping!"), *MappingGuid.String());
					}
				}
				break;
			default:
				{
					warnf(NAME_Warning, TEXT("Unknown mapping type in the ImportedMappings: 0x%08x"), (DWORD)(ImportData->Type));
				}
			}

			(*pImportData)->bProcessed = TRUE;
		}
		else
		{
			// Just to be able to set a breakpoint here.
			INT DebugDummy = 0;
		}
	}
	else
	{
		warnf(NAME_Warning, TEXT("Failed to find imported mapping %s"), *(MappingGuid.String()));
	}

	if ( !bRunningLightmass )
	{
		Statistics.ApplyTimeInProcessing += appSeconds() - ApplyStartTime;
	}
}

/**
 * Process any available mappings.
 * Used when running w/ bProcessCompletedMappingsImmediately == TRUE
 **/
void FLightmassProcessor::ProcessAvailableMappings(UBOOL bProcessAllAvailable)
{
	UBOOL bDoneProcessing = FALSE;
	INT ProcessedCount = 0;
	while (!bDoneProcessing)
	{
		FGuid NextGuid = FGuid(0,0,0,MappingToProcessIndex);
		FMappingImportHelper** pImportData = ImportedMappings.Find(NextGuid);
		if ( pImportData && *pImportData && (*pImportData)->bProcessed == FALSE )
		{
			ProcessMapping(NextGuid);
			MappingToProcessIndex++;
			ProcessedCount++;
		}
		else
		{
			bDoneProcessing = TRUE;
		}

		if (!bProcessAllAvailable && (ProcessedCount == MaxProcessAvailableCount))
		{
			bDoneProcessing = TRUE;
		}
	}
}

/** Reads in a TArray from the given channel. */
template<class T>
void FLightmassProcessor::ReadArray(INT Channel, TArray<T>& Array)
{
	INT ArrayNum = 0;
	Swarm.ReadChannel(Channel, &ArrayNum, sizeof(ArrayNum));
	if (ArrayNum > 0)
	{
		Array.Empty(ArrayNum);
		Array.AddZeroed(ArrayNum);
		Swarm.ReadChannel(Channel, Array.GetData(), Array.GetTypeSize() * ArrayNum);
	}
}

/** Fills out GDebugStaticLightingInfo with the output from Lightmass */
void FLightmassProcessor::ImportDebugOutput()
{
	checkAtCompileTime(sizeof(FDebugStaticLightingRay) == sizeof(Lightmass::FDebugStaticLightingRay), DebugTypeSizesMustMatch);
	checkAtCompileTime(sizeof(FDebugStaticLightingVertex) == sizeof(Lightmass::FDebugStaticLightingVertex), DebugTypeSizesMustMatch);
	checkAtCompileTime(sizeof(FDebugLightingCacheRecord) == sizeof(Lightmass::FDebugLightingCacheRecord), DebugTypeSizesMustMatch);
	checkAtCompileTime(sizeof(FDebugPhoton) == sizeof(Lightmass::FDebugPhoton), DebugTypeSizesMustMatch);
	checkAtCompileTime(sizeof(FDebugOctreeNode) == sizeof(Lightmass::FDebugOctreeNode), DebugTypeSizesMustMatch);
	checkAtCompileTime(NumTexelCorners == Lightmass::NumTexelCorners, DebugTypeSizesMustMatch);

	const FString ChannelName = Lightmass::CreateChannelName(Lightmass::DebugOutputGuid, Lightmass::LM_DEBUGOUTPUT_VERSION, Lightmass::LM_DEBUGOUTPUT_EXTENSION);
	const INT Channel = Swarm.OpenChannel( *ChannelName, LM_DEBUGOUTPUT_CHANNEL_FLAGS );
	if (Channel >= 0)
	{
		Swarm.ReadChannel(Channel, &GDebugStaticLightingInfo.bValid, sizeof(GDebugStaticLightingInfo.bValid));
		ReadArray(Channel, GDebugStaticLightingInfo.PathRays);
		ReadArray(Channel, GDebugStaticLightingInfo.ShadowRays);
		ReadArray(Channel, GDebugStaticLightingInfo.IndirectPhotonPaths);
		ReadArray(Channel, GDebugStaticLightingInfo.SelectedVertexIndices);
		ReadArray(Channel, GDebugStaticLightingInfo.Vertices);
		ReadArray(Channel, GDebugStaticLightingInfo.CacheRecords);
		ReadArray(Channel, GDebugStaticLightingInfo.DirectPhotons);
		ReadArray(Channel, GDebugStaticLightingInfo.IndirectPhotons);
		ReadArray(Channel, GDebugStaticLightingInfo.IrradiancePhotons);
		ReadArray(Channel, GDebugStaticLightingInfo.GatheredCausticPhotons);
		ReadArray(Channel, GDebugStaticLightingInfo.GatheredPhotons);
		ReadArray(Channel, GDebugStaticLightingInfo.GatheredImportancePhotons);
		ReadArray(Channel, GDebugStaticLightingInfo.GatheredPhotonNodes);
		Swarm.ReadChannel(Channel, &GDebugStaticLightingInfo.bDirectPhotonValid, sizeof(GDebugStaticLightingInfo.bDirectPhotonValid));
		Swarm.ReadChannel(Channel, &GDebugStaticLightingInfo.GatheredDirectPhoton, sizeof(GDebugStaticLightingInfo.GatheredDirectPhoton));
		Swarm.ReadChannel(Channel, &GDebugStaticLightingInfo.TexelCorners, sizeof(GDebugStaticLightingInfo.TexelCorners));
		Swarm.ReadChannel(Channel, &GDebugStaticLightingInfo.bCornerValid, sizeof(GDebugStaticLightingInfo.bCornerValid));
		Swarm.ReadChannel(Channel, &GDebugStaticLightingInfo.SampleRadius, sizeof(GDebugStaticLightingInfo.SampleRadius));

		Swarm.CloseChannel(Channel);
	}
	else
	{
		debugf( TEXT("Error, OpenChannel failed to open %s with error code %d"), *ChannelName, Channel );
	}
}

UBOOL FLightmassProcessor::AddDominantShadowTask(ULightComponent* Light)
{
	NSwarm::FGuid TaskGuid;
	Copy(Light->LightmapGuid, TaskGuid);
	NSwarm::FTaskSpecification NewTaskSpecification( TaskGuid, TEXT("DominantShadow"), NSwarm::JOB_TASK_FLAG_USE_DEFAULTS );
	//@todo - accurately estimate cost
	NewTaskSpecification.Cost = 50000;
	const INT ErrorCode = Swarm.AddTask( NewTaskSpecification );
	if( ErrorCode >= 0 )
	{
		DominantShadowTaskLights.Set(Light->LightmapGuid, Light);
		return TRUE;
	}
	else
	{
		debugf( TEXT("Error, AddDominantShadowTask->AddTask failed with error code %d"), ErrorCode );
		return FALSE;
	}
}

/**
 *	Retrieve the light for the given Guid
 *
 *	@param	LightGuid			The guid of the light we are looking for
 *
 *	@return	ULightComponent*	The corresponding light component.
 *								NULL if not found.
 */
ULightComponent* FLightmassProcessor::FindLight(FGuid& LightGuid)
{
	if (Exporter)
	{
		INT LightIndex;
		for (LightIndex = 0; LightIndex < Exporter->DirectionalLights.Num(); LightIndex++)
		{
			const UDirectionalLightComponent* Light = Exporter->DirectionalLights(LightIndex);
			if (Light)
			{
				if (Light->LightmapGuid == LightGuid)
				{
					return (ULightComponent*)Light;
				}
			}
		}
		for (LightIndex = 0; LightIndex < Exporter->PointLights.Num(); LightIndex++)
		{
			const UPointLightComponent* Light = Exporter->PointLights(LightIndex);
			if (Light)
			{
				if (Light->LightmapGuid == LightGuid)
				{
					return (ULightComponent*)Light;
				}
			}
		}
		for (LightIndex = 0; LightIndex < Exporter->SpotLights.Num(); LightIndex++)
		{
			const USpotLightComponent* Light = Exporter->SpotLights(LightIndex);
			if (Light)
			{
				if (Light->LightmapGuid == LightGuid)
				{
					return (ULightComponent*)Light;
				}
			}
		}
		for (LightIndex = 0; LightIndex < Exporter->SkyLights.Num(); LightIndex++)
		{
			const USkyLightComponent* Light = Exporter->SkyLights(LightIndex);
			if (Light)
			{
				if (Light->LightmapGuid == LightGuid)
				{
					return (ULightComponent*)Light;
				}
			}
		}
	}

	return NULL;
}

/**
 *	Retrieve the static mehs for the given Guid
 *
 *	@param	Guid				The guid of the static mesh we are looking for
 *
 *	@return	UStaticMesh*		The corresponding static mesh.
 *								NULL if not found.
 */
UStaticMesh* FLightmassProcessor::FindStaticMesh(FGuid& Guid)
{
	if (Exporter)
	{
		for (INT SMIdx = 0; SMIdx < Exporter->StaticMeshes.Num(); SMIdx++)
		{
			const UStaticMesh* StaticMesh = Exporter->StaticMeshes(SMIdx);
			if (StaticMesh && (StaticMesh->LightingGuid == Guid))
			{
				return (UStaticMesh*)StaticMesh;
			}
		}
	}
	return NULL;
}

/**
 *	Import light map data from the given channel.
 *
 *	@param	Channel				The channel to import from.
 *	@param	QuantizedData		The quantized lightmap data to fill in
 *	@param	UncompressedSize	Size the data will be after uncompressing it (if compressed)
 *	@param	CompressedSize		Size of the source data if compressed
 *
 *	@return	UBOOL				TRUE if successful, FALSE otherwise.
 */
UBOOL FLightmassProcessor::ImportLightMapData1DData(INT Channel, FQuantizedLightmapData* QuantizedData, INT UncompressedSize, INT CompressedSize)
{
	check(QuantizedData);
	checkAtCompileTime(NUM_STORED_LIGHTMAP_COEF == Lightmass::LM_NUM_STORED_LIGHTMAP_COEF, LightmassDefineMismatch);
	checkAtCompileTime(NUM_DIRECTIONAL_LIGHTMAP_COEF == Lightmass::LM_NUM_DIRECTIONAL_LIGHTMAP_COEF, LightmassDefineMismatch);

	INT Size = QuantizedData->SizeX;

	QuantizedData->Data.Empty(Size);
	QuantizedData->Data.Add(Size);
	FLightMapCoefficients* DataBuffer = QuantizedData->Data.GetTypedData();

	INT DataBufferSize = Size * sizeof(FLightMapCoefficients);

#if LM_COMPRESS_OUTPUT_DATA

	check(DataBufferSize == UncompressedSize);

	// read in the compressed data
	void* CompressedBuffer = appMalloc(CompressedSize);
	Swarm.ReadChannel(Channel, CompressedBuffer, CompressedSize);

	// decompress the temp buffer into the final location
	// we can read directly into the lightmap data because it's a TArray, not a TChunkedArray like in the 2D case
	if (!appUncompressMemory(COMPRESS_ZLIB, DataBuffer, UncompressedSize, CompressedBuffer, CompressedSize))
	{
		checkf(TEXT("Uncompress failed, which is unexpected"));
	}
	appFree(CompressedBuffer);

#else

	// read directly into the UE3 lightmap data
	Swarm.ReadChannel(Channel, DataBuffer, DataBufferSize);

#endif

	return TRUE;
}

/**
 *	Import light map data from the given channel.
 *
 *	@param	Channel				The channel to import from.
 *	@param	QuantizedData		The quantized lightmap data to fill in
 *	@param	UncompressedSize	Size the data will be after uncompressing it (if compressed)
 *	@param	CompressedSize		Size of the source data if compressed
 *
 *	@return	UBOOL				TRUE if successful, FALSE otherwise.
 */
UBOOL FLightmassProcessor::ImportLightMapData2DData(INT Channel, FQuantizedLightmapData* QuantizedData, INT UncompressedSize, INT CompressedSize)
{
	check(QuantizedData);
	
	INT SizeX = QuantizedData->SizeX;
	INT SizeY = QuantizedData->SizeY;

	// make space for the samples
	QuantizedData->Data.Empty(SizeX * SizeY);
	QuantizedData->Data.Add(SizeX * SizeY);
	FLightMapCoefficients* DataBuffer = QuantizedData->Data.GetTypedData();

	INT DataBufferSize = SizeX * SizeY * sizeof(FLightMapCoefficients);

#if LM_COMPRESS_OUTPUT_DATA
	check(DataBufferSize == UncompressedSize);

	// read in the compressed data
	void* CompressedBuffer = appMalloc(CompressedSize);
	Swarm.ReadChannel(Channel, CompressedBuffer, CompressedSize);

	// decompress the temp buffer into another temp buffer 
	if (!appUncompressMemory(COMPRESS_ZLIB, DataBuffer, UncompressedSize, CompressedBuffer, CompressedSize))
	{
		checkf(TEXT("Uncompress failed, which is unexpected"));
	}

	// can free one buffer now
	appFree(CompressedBuffer);
#else
	// read directly into the UE3 lightmap data
	Swarm.ReadChannel(Channel, DataBuffer, DataBufferSize);
#endif

	return TRUE;
}

/**
 *	Import shadow map data from the given channel.
 *
 *	@param	Channel				The channel to import from.
 *	@param	OutShadowMapData	The light component --> shadow map data mapping that should be filled in.
 *	@param	ShadowMapCount		Number of shadow maps to import
 *
 *	@return	UBOOL				TRUE if successful, FALSE otherwise.
 */
UBOOL FLightmassProcessor::ImportShadowMapData1D(INT Channel, TMap<ULightComponent*,FShadowMapData1D*>& OutShadowMapData, INT ShadowMapCount)
{
	for (INT SMIndex = 0; SMIndex < ShadowMapCount; SMIndex++)
	{
		FGuid LightGuid;
		Swarm.ReadChannel(Channel, &LightGuid, sizeof(FGuid));

		ULightComponent* LightComp = FindLight(LightGuid);
		if (LightComp == NULL)
		{
			warnf(NAME_Warning, TEXT("Failed to find light for vertex  mapping: %s"), *(LightGuid.String()));
		}

		Lightmass::FShadowMapData1DData SMData(0);
		Swarm.ReadChannel(Channel, &SMData, sizeof(Lightmass::FShadowMapData1DData));

		// Taking advantage of the shadow data being floats here...
		FShadowMapData1D* ShadowMapData = new FShadowMapData1D(SMData.NumSamples);
		check(ShadowMapData);

		FLOAT* DataBuffer = &((*ShadowMapData)(0));
		UINT DataBufferSize = SMData.NumSamples * sizeof(FLOAT);

		// Read the unquantized, uncompressed raw data
		Swarm.ReadChannel(Channel, DataBuffer, DataBufferSize);

		if (LightComp)
		{
			OutShadowMapData.Set(LightComp, ShadowMapData);
		}
		else
		{
			delete ShadowMapData;
		}
	}

	return TRUE;
}

/**
 *	Import shadow map data from the given channel.
 *
 *	@param	Channel				The channel to import from.
 *	@param	OutShadowMapData	The light component --> shadow map data mapping that should be filled in.
 *	@param	ShadowMapCount		Number of shadow maps to import
 *
 *	@return	UBOOL				TRUE if successful, FALSE otherwise.
 */
UBOOL FLightmassProcessor::ImportShadowMapData2D(INT Channel, TMap<ULightComponent*,FShadowMapData2D*>& OutShadowMapData, INT ShadowMapCount)
{
	for (INT SMIndex = 0; SMIndex < ShadowMapCount; SMIndex++)
	{
		FGuid LightGuid;
		Swarm.ReadChannel(Channel, &LightGuid, sizeof(FGuid));

		ULightComponent* LightComp = FindLight(LightGuid);
		if (LightComp == NULL)
		{
			warnf(NAME_Warning, TEXT("Failed to find light for texture mapping: %s"), *(LightGuid.String()));
		}

		Lightmass::FShadowMapData2DData SMData(0,0);
		Swarm.ReadChannel(Channel, &SMData, sizeof(Lightmass::FShadowMapData2DData));

		checkAtCompileTime(sizeof(FQuantizedShadowSample) == sizeof(Lightmass::FQuantizedShadowSampleData), SampleDataSizesMustMatch);

		// Taking advantage of the Lightmass::FShadowSampleData being identical to FShadowSample here... both in size and serialization order
		FQuantizedShadowFactorData2D* ShadowMapData = new FQuantizedShadowFactorData2D(SMData.SizeX, SMData.SizeY);
		check(ShadowMapData);

		FQuantizedShadowSample* DataBuffer = &((*ShadowMapData)(0, 0));
		UINT DataBufferSize = SMData.SizeX * SMData.SizeY * sizeof(Lightmass::FQuantizedShadowSampleData);

#if LM_COMPRESS_OUTPUT_DATA
		UINT CompressedSize = SMData.CompressedDataSize;
		UINT UncompressedSize = SMData.UncompressedDataSize;
		check(DataBufferSize == UncompressedSize);

		// Read in the compressed data
		void* CompressedBuffer = appMalloc(CompressedSize);
		Swarm.ReadChannel(Channel, CompressedBuffer, CompressedSize);

		// Decompress the temp buffer into another temp buffer 
		if (!appUncompressMemory(COMPRESS_ZLIB, DataBuffer, UncompressedSize, CompressedBuffer, CompressedSize))
		{
			checkf(TEXT("Uncompress failed, which is unexpected"));
		}
		appFree(CompressedBuffer);
#else
		// Read the data directly
		Swarm.ReadChannel(Channel, DataBuffer, DataBufferSize);
#endif

		if (LightComp)
		{
			OutShadowMapData.Set(LightComp, ShadowMapData);
		}
		else
		{
			delete ShadowMapData;
		}
	}

	return TRUE;
}

/**
 *	Import signed distance field shadow map data from the given channel.
 *
 *	@param	Channel				The channel to import from.
 *	@param	OutShadowMapData	The light component --> shadow map data mapping that should be filled in.
 *	@param	ShadowMapCount		Number of shadow maps to import
 *
 *	@return	UBOOL				TRUE if successful, FALSE otherwise.
 */
UBOOL FLightmassProcessor::ImportSignedDistanceFieldShadowMapData2D(INT Channel, TMap<ULightComponent*,FShadowMapData2D*>& OutShadowMapData, INT ShadowMapCount)
{
	for (INT SMIndex = 0; SMIndex < ShadowMapCount; SMIndex++)
	{
		FGuid LightGuid;
		Swarm.ReadChannel(Channel, &LightGuid, sizeof(FGuid));

		ULightComponent* LightComp = FindLight(LightGuid);
		if (LightComp == NULL)
		{
			warnf(NAME_Warning, TEXT("Failed to find light for texture mapping: %s"), *(LightGuid.String()));
		}

		Lightmass::FShadowMapData2DData SMData(0,0);
		Swarm.ReadChannel(Channel, &SMData, sizeof(Lightmass::FShadowMapData2DData));

		checkAtCompileTime(sizeof(FQuantizedSignedDistanceFieldShadowSample) == sizeof(Lightmass::FQuantizedSignedDistanceFieldShadowSampleData), SampleDataSizesMustMatch);

		FQuantizedShadowSignedDistanceFieldData2D* ShadowMapData = new FQuantizedShadowSignedDistanceFieldData2D(SMData.SizeX, SMData.SizeY);
		check(ShadowMapData);

		FQuantizedSignedDistanceFieldShadowSample* DataBuffer = &((*ShadowMapData)(0, 0));
		UINT DataBufferSize = SMData.SizeX * SMData.SizeY * sizeof(Lightmass::FQuantizedSignedDistanceFieldShadowSampleData);

#if LM_COMPRESS_OUTPUT_DATA
		UINT CompressedSize = SMData.CompressedDataSize;
		UINT UncompressedSize = SMData.UncompressedDataSize;
		check(DataBufferSize == UncompressedSize);

		// Read in the compressed data
		void* CompressedBuffer = appMalloc(CompressedSize);
		Swarm.ReadChannel(Channel, CompressedBuffer, CompressedSize);

		// Decompress the temp buffer into another temp buffer 
		if (!appUncompressMemory(COMPRESS_ZLIB, DataBuffer, UncompressedSize, CompressedBuffer, CompressedSize))
		{
			checkf(TEXT("Uncompress failed, which is unexpected"));
		}
		appFree(CompressedBuffer);
#else
		// Read the data directly
		Swarm.ReadChannel(Channel, DataBuffer, DataBufferSize);
#endif

		if (LightComp)
		{
			OutShadowMapData.Set(LightComp, ShadowMapData);
		}
		else
		{
			delete ShadowMapData;
		}
	}

	return TRUE;
}


/**
 *	Import a complete vertex mapping....
 *
 *	@param	Channel			The channel to import from.
 *	@param	VMImport		The vertex mapping information that will be imported.
 *
 *	@return	UBOOL			TRUE if successful, FALSE otherwise.
 */
UBOOL FLightmassProcessor::ImportVertexMapping(INT Channel, FVertexMappingImportHelper& VMImport)
{
	UBOOL bResult = TRUE;

	// Additional information for this mapping
	Swarm.ReadChannel(Channel, &(VMImport.ExecutionTime), sizeof(DOUBLE));

	// The resulting light map data for this mapping (shared header and TArray) 
	Lightmass::FLightMapData1DData LMLightmapData1DData(0);
	Swarm.ReadChannel(Channel, &LMLightmapData1DData, sizeof(Lightmass::FLightMapData1DData));
	Swarm.ReadChannel(Channel, &VMImport.NumShadowMaps, sizeof(VMImport.NumShadowMaps));

	INT NumLights = 0;
	TArray<FGuid> LightGuids;
	Swarm.ReadChannel(Channel, &NumLights, sizeof(NumLights));
	LightGuids.Empty(NumLights);
	for (INT i = 0; i < NumLights; i++)
	{
		const INT NewLightIndex = LightGuids.Add();
		Swarm.ReadChannel(Channel, &LightGuids(NewLightIndex), sizeof(LightGuids(NewLightIndex)));
	}

	// allocate space to store the quantized data
	VMImport.QuantizedData = new FQuantizedLightmapData;
	appMemcpy(VMImport.QuantizedData->Scale, LMLightmapData1DData.Scale, sizeof(VMImport.QuantizedData->Scale));
	VMImport.QuantizedData->SizeX = LMLightmapData1DData.NumSamples;
	VMImport.QuantizedData->SizeY = 1;
	VMImport.QuantizedData->LightGuids = LightGuids;

#if LM_COMPRESS_OUTPUT_DATA
	if (ImportLightMapData1DData(Channel, VMImport.QuantizedData, LMLightmapData1DData.UncompressedDataSize, LMLightmapData1DData.CompressedDataSize) == FALSE)
#else
	if (ImportLightMapData1DData(Channel, VMImport.QuantizedData, 0, 0) == FALSE)
#endif
	{
		bResult = FALSE;
	}

	Swarm.ReadChannel(Channel, &VMImport.QuantizedData->PreviewEnvironmentShadowing, sizeof(VMImport.QuantizedData->PreviewEnvironmentShadowing));

	// The resulting light GUID --> shadow map data
	if (ImportShadowMapData1D(Channel, VMImport.ShadowMapData, VMImport.NumShadowMaps) == FALSE)
	{
		bResult = FALSE;
	}

	// Update the LightingBuildInfo list
	UObject* MappedObject = VMImport.VertexMapping->GetMappedObject();
	GWarn->LightingBuildInfo_Add(MappedObject, VMImport.ExecutionTime, -1.0f, -1, -1);

	return bResult;
}

/**
 *	Import a complete texture mapping....
 *
 *	@param	Channel			The channel to import from.
 *	@param	TMImport		The texture mapping information that will be imported.
 *
 *	@return	UBOOL			TRUE if successful, FALSE otherwise.
 */
UBOOL FLightmassProcessor::ImportTextureMapping(INT Channel, FTextureMappingImportHelper& TMImport)
{
	UBOOL bResult = TRUE;

	// Additional information for this mapping
	Swarm.ReadChannel(Channel, &(TMImport.ExecutionTime), sizeof(DOUBLE));

	// The resulting light map data for this mapping (shared header and TArray) 
	Lightmass::FLightMapData2DData LMLightmapData2DData(0,0);
	Swarm.ReadChannel(Channel, &LMLightmapData2DData, sizeof(Lightmass::FLightMapData2DData));
	check(TMImport.TextureMapping->SizeX == LMLightmapData2DData.SizeX);
	check(TMImport.TextureMapping->SizeY == LMLightmapData2DData.SizeY);
	Swarm.ReadChannel(Channel, &TMImport.NumShadowMaps, sizeof(TMImport.NumShadowMaps));
	Swarm.ReadChannel(Channel, &TMImport.NumSignedDistanceFieldShadowMaps, sizeof(TMImport.NumSignedDistanceFieldShadowMaps));

	INT NumLights = 0;
	TArray<FGuid> LightGuids;
	LightGuids.Empty(NumLights);
	Swarm.ReadChannel(Channel, &NumLights, sizeof(NumLights));
	for (INT i = 0; i < NumLights; i++)
	{
		const INT NewLightIndex = LightGuids.Add();
		Swarm.ReadChannel(Channel, &LightGuids(NewLightIndex), sizeof(LightGuids(NewLightIndex)));
	}

	// allocate space to store the quantized data
	TMImport.QuantizedData = new FQuantizedLightmapData;
	appMemcpy(TMImport.QuantizedData->Scale, LMLightmapData2DData.Scale, sizeof(TMImport.QuantizedData->Scale));
	TMImport.QuantizedData->SizeX = LMLightmapData2DData.SizeX;
	TMImport.QuantizedData->SizeY = LMLightmapData2DData.SizeY;
	TMImport.QuantizedData->LightGuids = LightGuids;

#if LM_COMPRESS_OUTPUT_DATA
	if (ImportLightMapData2DData(Channel, TMImport.QuantizedData, LMLightmapData2DData.UncompressedDataSize, LMLightmapData2DData.CompressedDataSize) == FALSE)
#else
	if (ImportLightMapData2DData(Channel, TMImport.QuantizedData, 0, 0) == FALSE)
#endif
	{
		bResult = FALSE;
	}

	Swarm.ReadChannel(Channel, &TMImport.QuantizedData->PreviewEnvironmentShadowing, sizeof(TMImport.QuantizedData->PreviewEnvironmentShadowing));

	INT NumUnmappedTexels = 0;
	for (INT DebugIdx = 0; DebugIdx < TMImport.QuantizedData->Data.Num(); DebugIdx++)
	{
		if (TMImport.QuantizedData->Data(DebugIdx).Coverage == 0.0f)
		{
			NumUnmappedTexels++;
		}
	}

	if (TMImport.QuantizedData && TMImport.QuantizedData->Data.Num() > 0)
	{
		TMImport.UnmappedTexelsPercentage = (FLOAT)NumUnmappedTexels / (FLOAT)TMImport.QuantizedData->Data.Num();
	}
	else
	{
		TMImport.UnmappedTexelsPercentage = 0.0f;
	}

	// The resulting light GUID --> shadow map data
	if (ImportShadowMapData2D(Channel, TMImport.ShadowMapData, TMImport.NumShadowMaps) == FALSE)
	{
		bResult = FALSE;
	}

	if (ImportSignedDistanceFieldShadowMapData2D(Channel, TMImport.ShadowMapData, TMImport.NumSignedDistanceFieldShadowMaps) == FALSE)
	{
		bResult = FALSE;
	}

	// Update the LightingBuildInfo list
	UObject* MappedObject = TMImport.TextureMapping->GetMappedObject();
	FLOAT MemoryAmount = FLOAT(NumUnmappedTexels);
	FLOAT TotalMemoryAmount = FLOAT(TMImport.QuantizedData->Data.Num());
	//@todo. Move this into some common place... it's defined in several places now!
	const FLOAT MIP_FACTOR = 4.0f / 3.0f;
	// Compressed == 4 bits/pixel
	FLOAT BytesPerPixel = 0.5f;
	if (GAllowLightmapCompression == FALSE)
	{
		// Uncompressed == 4 BYTES/pixels
		BytesPerPixel = 4.0f;
	}

	FLOAT LightMapTypeModifier = NUM_DIRECTIONAL_LIGHTMAP_COEF;
	if (GSystemSettings.bAllowDirectionalLightMaps == FALSE)
	{
		LightMapTypeModifier = NUM_SIMPLE_LIGHTMAP_COEF;
	}

	INT WastedMemory = appTrunc(MemoryAmount * BytesPerPixel * MIP_FACTOR * LightMapTypeModifier);
	INT TotalMemory = appTrunc(TotalMemoryAmount * BytesPerPixel * MIP_FACTOR * LightMapTypeModifier);
	GWarn->LightingBuildInfo_Add(MappedObject, TMImport.ExecutionTime, TMImport.UnmappedTexelsPercentage, WastedMemory, TotalMemory);

	return bResult;
}

#endif // WITH_MANAGED_CODE
