/*=============================================================================
	Engine.h: Unreal engine public header file.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_ENGINE
#define _INC_ENGINE

/*-----------------------------------------------------------------------------
	Dependencies.
-----------------------------------------------------------------------------*/

#include "Core.h"

/** Helper function to flush resource streaming. */
extern void FlushResourceStreaming();

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

extern class FStatChart*		GStatChart;
extern class UEngine*			GEngine;

/** Enables texture streaming support.  Must not be changed after engine is initialized. */
extern UBOOL GUseTextureStreaming;

/** Whether screen door fade effects are used in game */
extern UBOOL GAllowScreenDoorFade;

/** Cache for property window queries*/
extern class FPropertyWindowDataCache* GPropertyWindowDataCache;

/** 
 * Whether Nvidia's Stereo 3d can be used.  
 * Changing this requires a shader recompile, and enabling it uses an extra texture sampler (which may cause some materials to fail compiling).
 */
extern UBOOL GAllowNvidiaStereo3d;

/** when set, disallows all network travel (pending level rejects all client travel attempts) */
extern UBOOL GDisallowNetworkTravel;

/** Stores a reference to the current connecting UNetConnection, only valid from within PreLogin */
extern class UNetConnection* GPreLoginConnection;

#if WITH_EDITOR
	// Note: This is a global because GEngine is often not loaded when we need to access this set
extern TSet<UPackage*> MaterialPackagesWithDependentChanges;
#endif

/*-----------------------------------------------------------------------------
	Size of the world.
-----------------------------------------------------------------------------*/

#define WORLD_MAX				524288.0	/* Maximum size of the world */
#define THREE_FOURTHS_WORLD_MAX	393216.0	/* 3/4ths of the maximum size of the world */
#define HALF_WORLD_MAX			262144.0	/* Half the maximum size of the world */
#define HALF_WORLD_MAX1			262143.0	/* Half the maximum size of the world - 1*/
#define MIN_ORTHOZOOM			250.0		/* Limit of 2D viewport zoom in */
#define MAX_ORTHOZOOM			16000000.0	/* Limit of 2D viewport zoom out */
#define DEFAULT_ORTHOZOOM		10000		/* Default 2D viewport zoom */

/*-----------------------------------------------------------------------------
	Localization defines.
-----------------------------------------------------------------------------*/

#define LOCALIZED_SEEKFREE_SUFFIX	TEXT("_LOC")

/*-----------------------------------------------------------------------------
	In Editor Game viewport name (hardcoded)
-----------------------------------------------------------------------------*/

#define PLAYWORLD_PACKAGE_PREFIX TEXT("UEDPIE")
#define PLAYWORLD_CONSOLE_BASE_PACKAGE_PREFIX TEXT("UED")

/*-----------------------------------------------------------------------------
	Level updating. Moved out here from UnLevel.h so that AActor.h can use it (needed for gcc)
-----------------------------------------------------------------------------*/

enum ELevelTick
{
	LEVELTICK_TimeOnly		= 0,	// Update the level time only.
	LEVELTICK_ViewportsOnly	= 1,	// Update time and viewports.
	LEVELTICK_All			= 2,	// Update all.
};

enum EAIFunctions
{
	AI_PollMoveTo = 501,
	AI_PollMoveToward = 503,
	AI_PollStrafeTo = 505,
	AI_PollStrafeFacing = 507,
	AI_PollFinishRotation = 509,
	AI_PollWaitForLanding = 528,
};

/*-----------------------------------------------------------------------------
	FaceFX support.
-----------------------------------------------------------------------------*/
#if WITH_FACEFX
#	include "UnFaceFXSupport.h"
#endif // WITH_FACEFX

/**
 * Engine stats
 */
enum EEngineStats
{
	/** Terrain stats */
	STAT_TerrainRenderTime = STAT_EngineFirstStat,
	STAT_TerrainSmoothTime,
	STAT_TerrainTriangles,
	/** Input stat */
	STAT_InputTime,
	STAT_InputLatencyTime,
	/** HUD stat */
	STAT_HudTime,
	/** Static mesh tris rendered */
	STAT_StaticMeshTriangles,
	STAT_SkinningTime,
	STAT_UpdateClothVertsTime,
	STAT_UpdateSoftBodyVertsTime,
	STAT_SkelMeshTriangles,
	STAT_SkelMeshDrawCalls,
	STAT_CPUSkinVertices,
	STAT_GPUSkinVertices,
	STAT_FracturePartPoolUsed,
	STAT_GameEngineTick,
	STAT_GameViewportTick,
	STAT_RedrawViewports,
	STAT_UpdateLevelStreaming,
	STAT_RHITickTime
};

/**
 * Game stats
 */
enum EGameStats
{
	STAT_DecalTime = STAT_UnrealScriptTime + 1,
	STAT_PhysicsTime,
	STAT_SpawnActorTime,
	STAT_MoveActorTime,
	STAT_FarMoveActorTime,
	STAT_GCMarkTime,
	STAT_GCSweepTime,
	STAT_UpdateComponentsTime,
	STAT_KismetTime,
	STAT_AIVisManagerTime,
	/** Ticking stats */
	STAT_PostUpdateTickTime,
	STAT_PostAsyncTickTime,
	STAT_DuringAsyncTickTime,
	STAT_PreAsyncTickTime,
	STAT_PostUpdateComponentTickTime,
	STAT_PostAsyncComponentTickTime,
	STAT_DuringAsyncComponentTickTime,
	STAT_PreAsyncComponentTickTime,
	STAT_TickTime,
	STAT_WorldTickTime,
	STAT_PostTickComponentUpdate,
	STAT_ParticleManagerUpdateData,
	STAT_PostUpdateWorkActorsTicked,
	STAT_PostAsyncActorsTicked,
	STAT_DuringAsyncActorsTicked,
	STAT_PreAsyncActorsTicked,
	STAT_PostAsyncComponentsTicked,
	STAT_DuringAsyncComponentsTicked,
	STAT_PreAsyncComponentsTicked,
	STAT_AsyncWorkWaitTime,
	STAT_PawnPhysics,
};

/**
 * FPS chart stats
 */
enum FPSChartStats
{
	STAT_FPSChart_0_5	= STAT_FPSChartFirstStat,
	STAT_FPSChart_5_10,
	STAT_FPSChart_10_15,
	STAT_FPSChart_15_20,
	STAT_FPSChart_20_25,
	STAT_FPSChart_25_30,
	STAT_FPSChart_30_35,
	STAT_FPSChart_35_40,
	STAT_FPSChart_40_45,
	STAT_FPSChart_45_50,
	STAT_FPSChart_50_55,
	STAT_FPSChart_55_60,
	STAT_FPSChart_60_INF,
	STAT_FPSChartLastBucketStat,
	STAT_FPSChart_30Plus,
	STAT_FPSChart_UnaccountedTime,
	STAT_FPSChart_FrameCount,

	/** Hitch stats */
	STAT_FPSChart_FirstHitchStat,
	STAT_FPSChart_Hitch_5000_Plus = STAT_FPSChart_FirstHitchStat,
	STAT_FPSChart_Hitch_2500_5000,
	STAT_FPSChart_Hitch_2000_2500,
	STAT_FPSChart_Hitch_1500_2000,
	STAT_FPSChart_Hitch_1000_1500,
	STAT_FPSChart_Hitch_750_1000,
	STAT_FPSChart_Hitch_500_750,
	STAT_FPSChart_Hitch_300_500,
	STAT_FPSChart_Hitch_200_300,
	STAT_FPSChart_Hitch_150_200,
	STAT_FPSChart_Hitch_100_150,
	STAT_FPSChart_LastHitchBucketStat,
	STAT_FPSChart_TotalHitchCount,

	/** Unit time stats */
	STAT_FPSChart_UnitFrame,
	STAT_FPSChart_UnitRender,
	STAT_FPSChart_UnitGame,
	STAT_FPSChart_UnitGPU,

};

/**
 * Path finding stats
 */
enum EPathFindingStats
{
	STAT_PathFinding_Reachable = STAT_PathFindingFirstStat,
	STAT_PathFinding_FindPathToward,
	STAT_PathFinding_BestPathTo,
};

/**
 * UI Stats
 */
enum EUIStats
{
	STAT_UIKismetTime = STAT_UIFirstStat,
	STAT_UITickTime,
	STAT_UIDrawingTime,
};

/**
* Canvas Stats
*/
enum ECanvasStats
{
	STAT_Canvas_FlushTime = STAT_CanvasFirstStat,	
	STAT_Canvas_DrawTextureTileTime,
	STAT_Canvas_DrawMaterialTileTime,
	STAT_Canvas_DrawStringTime,
	STAT_Canvas_GetBatchElementsTime,
	STAT_Canvas_AddTileRenderTime,
	STAT_Canvas_NumBatchesCreated	
};

/**
* Decal stats
*/
enum EDecalStats
{
	STAT_DecalAttachTime = STAT_DecalFirstStat,
	STAT_DecalBSPAttachTime,
	STAT_DecalStaticMeshAttachTime,
	STAT_DecalSkeletalMeshAttachTime,
	STAT_DecalTerrainAttachTime,
	STAT_DecalHitComponentAttachTime,
	STAT_DecalHitNodeAttachTime,
	STAT_DecalMultiComponentAttachTime,
	STAT_DecalReceiverImagesAttachTime,

	/** Decal stats */
	STAT_DecalTriangles,
	STAT_DecalDrawCalls,
	STAT_DecalRenderUnlitTime,
	STAT_DecalRenderLitTime,
	STAT_DecalRenderDynamicBSPTime,
	STAT_DecalRenderDynamicSMTime,
	STAT_DecalRenderDynamicTerrainTime,
	STAT_DecalRenderDynamicSkelTime
};

/** Timing stats for checkpoint loading and saving. */
enum ECheckPointSaveStats
{
	STAT_CheckPointSave = STAT_CheckPointFirstStat,
	STAT_CheckPointPreSave,
	STAT_CheckPointPostSave,
	STAT_CheckPointFlushLevels,
	STAT_CheckPointSaveKismet,
};

/**
 * Landscape stats
 */
enum ELandscapeStats
{
	/** Terrain stats */
	STAT_LandscapeDynamicDrawTime = STAT_LandscapeFirstStat,
	STAT_LandscapeStaticDrawLODTime,
	STAT_LandscapeVFDrawTime,
	STAT_LandscapeTriangles,
	STAT_LandscapeComponents,
	STAT_LandscapeDrawCalls,
	STAT_LandscapeHeightmapTextureMem,
	STAT_LandscapeWeightmapTextureMem,
	STAT_LandscapeComponentMem,
};

/*-----------------------------------------------------------------------------
	Forward declarations.
-----------------------------------------------------------------------------*/
class UTexture;
class UTexture2D;
class FLightMap2D;
class UShadowMap2D;
class FSceneInterface;
class FPrimitiveSceneInfo;
class FPrimitiveSceneProxy;
class UMaterialExpression;
class FMaterialRenderProxy;
class UMaterial;
class FSceneView;
class FSceneViewFamily;
class FViewportClient;
class FCanvas;
class FLinkedObjectDrawHelper;

namespace physx
{
	namespace apex
	{
		class NxApexRenderable;
		class NxDestructibleActor;
		class NxDestructiblePreview;
		class NxDestructiblePreviewDesc;
	}
}


#define ENUMS_ONLY 1
#include "EngineBaseClasses.h"
#include "EngineClasses.h"
#include "EngineTextureClasses.h"
#include "EngineSceneClasses.h"
#undef ENUMS_ONLY

/*-----------------------------------------------------------------------------
	Engine public includes.
-----------------------------------------------------------------------------*/
struct FURL;

#define NO_ENUMS 1
#include "EngineBaseClasses.h"				// Types needed by the renderer and script
#undef NO_ENUMS

#include "ShowFlags.h"						// Flags for enable scene rendering features
#include "UnObserver.h"						// FObserverInterface interface
#include "UnTickable.h"						// FTickableObject interface.
#include "UnSelection.h"					// Tools used by UnrealEd for managing resource selections.
#include "UnContentStreaming.h"				// Content streaming class definitions.
#include "UnRenderUtils.h"					// Render utility classes.
#include "HitProxies.h"						// Hit proxy definitions.
#include "ConvexVolume.h"					// Convex volume definition.
#include "ShaderCompiler.h"					// Platform independent shader compilation definitions.
#include "Mobile.h"							// Mobile defines and structures needed by both the cooker and mobile platforms
#include "RHI.h"							// Common RHI definitions.
#include "RenderingThread.h"				// Rendering thread definitions.
#include "RenderResource.h"					// Render resource definitions.
#include "RHIStaticStates.h"				// RHI static state template definition.
#include "RawIndexBuffer.h"					// Raw index buffer definitions.
#include "VertexFactory.h"					// Vertex factory definitions.
#include "MaterialShared.h"					// Shared material definitions.
#include "ShaderManager.h"					// Shader manager definitions.
#include "ShaderCache.h"					// Shader cache definitions.
#include "GlobalShader.h"					// Global shader definitions.
#include "MaterialShader.h"					// Material shader definitions.
#include "MeshMaterialShader.h"				// Mesh material shader defintions.
#include "LensFlareVertexFactory.h"			// Lens flare vertex factory definition.
#include "LocalVertexFactory.h"				// Local vertex factory definition.
#include "ParticleVertexFactory.h"			// Particle sprite vertex factory definition.
#include "ParticleSubUVVertexFactory.h"		// Particle sprite subUV vertex factory definition.
#include "ParticleBeamTrailVertexFactory.h"	// Particle beam/trail vertex factory definition.
#include "TerrainVertexFactory.h"			// Terrain vertex factory definition
#include "UnClient.h"						// Platform specific client interface definition.
#include "UnTex.h"							// Textures.
#include "SystemSettings.h"					// Scalability options.
#include "UnObj.h"							// Standard object definitions.
#include "UnPrim.h"							// Primitive class.
#include "UnAnimTree.h"						// Animation.
#include "UnComponents.h"					// Forward declarations of object components of actors
#include "Scene.h"							// Scene management.
#include "DynamicMeshBuilder.h"				// Dynamic mesh builder.
#include "PreviewScene.h"					// Preview scene definitions.
#include "UnPhysPublic.h"					// Public physics integration types.
#include "LightingBuildOptions.h"			// Definition of lighting build option struct.
#include "StaticLighting.h"					// Static lighting definitions.
#include "UnActorComponent.h"				// Actor component definitions.
#include "PrimitiveComponent.h"				// Primitive component definitions.
#include "UnParticleHelper.h"				// Particle helper definitions.
#include "ParticleEmitterInstances.h"
#include "UnSceneCapture.h"					// Scene render to texture probes
#include "UnPhysAsset.h"					// Physics Asset.
#include "UnInterpolation.h"				// Matinee.
#include "GenericOctreePublic.h"			// Generic octree public defines
#define NO_ENUMS 1
#include "EngineClasses.h"					// All actor classes.
#include "EngineTextureClasses.h"
#include "EngineSceneClasses.h"
#undef NO_ENUMS
#include "EngineCameraClasses.h"
#include "EngineControllerClasses.h"
#include "EnginePawnClasses.h"
#include "EngineLightClasses.h"
#include "EngineSkeletalMeshClasses.h"
#include "EngineReplicationInfoClasses.h"
#include "UnLightMap.h"						// Light-maps.
#include "UnShadowMap.h"					// Shadow-maps.
#include "UnModel.h"						// Model class.
#include "UnPrefab.h"						// Prefabs.
#include "UnPhysic.h"						// Physics constants
#include "EngineGameEngineClasses.h"		// Main Unreal engine declarations
#include "UnLevel.h"						// Level object.
#include "UnWorld.h"						// World object.
#include "UnKeys.h"							// Key name definitions.
#include "UnUIKeys.h"						// UI key name definitions.
#include "UnEngine.h"						// Unreal engine helpers.
#include "UnSkeletalMesh.h"					// Skeletal animated mesh.
#include "UnMorphMesh.h"					// Morph target mesh
#include "UnActor.h"						// Actor inlines.
#include "UnAudio.h"						// Audio code.
#include "UnStaticMesh.h"					// Static T&L meshes.
#include "UnCDKey.h"						// CD key validation.
#include "UnCanvas.h"						// Canvas.
#include "UnPNG.h"							// PNG helper code for storing compressed source art.
#include "UnStandardObjectPropagator.h"		// A class containing common propagator code.
#include "UnMiscDeclarations.h"				// Header containing misc class declarations.
#include "UnPostProcess.h"					// Post process defs/decls
#include "FullScreenMovie.h"				// Full screen movie playback support
#include "FullScreenMovieFallback.h"		// Full screen movie fallback 
#include "LensFlare.h"						
#include "AIProfiler.h"
#include "EngineUtils.h"
#include "IConsoleManager.h"
#include "InstancedFoliage.h"				// Instanced foliage.

/**
 * Texture group stats
 */
// This needs to be declared after EngineTextureClasses.h is included
enum ETexGroupStats
{
	_STAT_TextureGroup_Prev = STAT_TextureGroupFirst-1,
#define DECLARETEXGROUPSTAT(g) STAT_##g,
	FOREACH_ENUM_TEXTUREGROUP(DECLARETEXGROUPSTAT)
	DECLARETEXGROUPSTAT(TEXTUREGROUP_Unknown)
#undef DECLARETEXGROUPSTAT
};

#endif // _INC_ENGINE

