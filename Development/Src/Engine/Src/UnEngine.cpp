/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineMaterialClasses.h"
#include "UnTerrain.h"
#include "EngineAIClasses.h"
#include "EnginePhysicsClasses.h" 
#include "EngineForceFieldClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineParticleClasses.h"
#include "EngineAnimClasses.h"
#include "EngineDecalClasses.h"
#include "EngineFogVolumeClasses.h"
#include "UnFracturedStaticMesh.h"
#include "EngineMeshClasses.h"
#include "EnginePrefabClasses.h"
#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "EngineFoliageClasses.h"
#include "EngineSpeedTreeClasses.h"
#include "EngineFluidClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSplineClasses.h"
#include "EngineProcBuildingClasses.h"
#include "EnginePlatformInterfaceClasses.h"
#include "LensFlare.h"
#include "UnStatChart.h"
#include "UnNet.h"
#include "UnCodecs.h"
#include "RemoteControl.h"
#include "FTableOfContents.h"
#include "FFileManagerGeneric.h"
#include "DemoRecording.h"
#include "PerfMem.h"
#include "Database.h"
#include "SceneRenderTargets.h"
#include "CrossLevelReferences.h"
#include "UnSkeletalMeshMerge.h"
#include "ProfilingHelpers.h"
#include "UnLinkedObjDrawUtils.h"
#include "PrimitiveSceneInfo.h"
#include "IConsoleManager.h"
#include "AVIWriter.h"
#include "ChartCreation.h"
#include "AES.h"
#include "MicroTransactionProxy.h"

#if WITH_REALD
	#include "RealD/RealD.h"
#endif

#if WITH_OPEN_AUTOMATE
#include "OpenAutomate.h"
#endif // WITH_OPEN_AUTOMATE
#if WITH_SUBSTANCE_AIR
#include "SubstanceAirTypedefs.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirInstanceFactoryClasses.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirOutput.h"
#include "SubstanceAirHelpers.h"
#endif // WITH_SUBSTANCE_AIR

#if !CONSOLE && defined(_MSC_VER)
#include "..\..\UnrealEd\Inc\DebugToolExec.h"
#include "..\Debugger\UnDebuggerCore.h"
#endif

#if PS3 && STATS
#include "SpursProfiler.h"
#endif // PS3 && STATS

#if IPHONE
#include "IPhoneObjCWrapper.h"
#endif

#include "../../IpDrv/Inc/UnIpDrv.h"		// For GDebugChannel

#if WITH_GFx
#include "GFxUIClasses.h"
#endif

#if WITH_ES2_RHI
#include "ES2RHIPrivate.h"
#endif

/*-----------------------------------------------------------------------------
	Static linking.
-----------------------------------------------------------------------------*/

// disable optimizations for all this static linking initialization code - cuts PS3 compile time in half on this file
PRAGMA_DISABLE_OPTIMIZATION

#define STATIC_LINKING_MOJO 1

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName ENGINE_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "EngineGameEngineClasses.h"
#undef AUTOGENERATE_NAME

#include "EngineBaseClasses.h"
#include "EngineClasses.h"
#include "EngineTextureClasses.h"
#include "EngineLightClasses.h"
#include "EngineSkeletalMeshClasses.h"
#include "EngineControllerClasses.h"
#include "EngineReplicationInfoClasses.h"
#include "EnginePawnClasses.h"
#include "EngineAIClasses.h"
#include "EngineMaterialClasses.h"
#include "EngineTerrainClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineForceFieldClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineParticleClasses.h"
#include "EngineAnimClasses.h"
#include "EngineDecalClasses.h"
#include "EngineFogVolumeClasses.h"
#include "UnFracturedStaticMesh.h"
#include "EngineMeshClasses.h"
#include "EnginePrefabClasses.h"
#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "EngineSceneClasses.h"
#include "EngineFoliageClasses.h"
#include "EngineSpeedTreeClasses.h"
#include "EngineLensFlareClasses.h"
#include "EngineFluidClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSplineClasses.h"
#include "EngineProcBuildingClasses.h"
#include "EngineCameraClasses.h"
#include "EnginePlatformInterfaceClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "EngineGameEngineClasses.h"
#undef AUTOGENERATE_NAME

#include "EngineBaseClasses.h"
#include "EngineClasses.h"
#include "EngineTextureClasses.h"
#include "EngineLightClasses.h"
#include "EngineSkeletalMeshClasses.h"
#include "EngineControllerClasses.h"
#include "EngineReplicationInfoClasses.h"
#include "EnginePawnClasses.h"
#include "EngineAIClasses.h"
#include "EngineMaterialClasses.h"
#include "EngineTerrainClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineForceFieldClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineParticleClasses.h"
#include "EngineAnimClasses.h"
#include "EngineDecalClasses.h"
#include "EngineFogVolumeClasses.h"
#include "UnFracturedStaticMesh.h"
#include "EngineMeshClasses.h"
#include "EnginePrefabClasses.h"
#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "EngineSceneClasses.h"
#include "EngineFoliageClasses.h"
#include "EngineSpeedTreeClasses.h"
#include "EngineLensFlareClasses.h"
#include "EngineFluidClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSplineClasses.h"
#include "EngineProcBuildingClasses.h"
#include "EngineCameraClasses.h"
#include "EnginePlatformInterfaceClasses.h"

// needed for the intrinsic UObject, that is outside of #if DWTRIOVIZSDK
#include "DwTrioviz/DwTriovizImpl.h"

#undef AUTOGENERATE_FUNCTION
#undef NATIVES_ONLY
#undef NAMES_ONLY

#if CHECK_NATIVE_CLASS_SIZES

void AutoCheckNativeClassSizesEngine( UBOOL& Mismatch )
{
#define NAMES_ONLY
#define AUTOGENERATE_NAME( name )
#define AUTOGENERATE_FUNCTION( cls, idx, name )
#define VERIFY_CLASS_SIZES
	VERIFY_CLASS_SIZE_NODIE( AWeapon );
	VERIFY_CLASS_SIZE_NODIE( AKActor );
#include "EngineGameEngineClasses.h"
#include "EngineBaseClasses.h"
#include "EngineClasses.h"
#include "EngineTextureClasses.h"
#include "EngineLightClasses.h"
#include "EngineSkeletalMeshClasses.h"
#include "EngineControllerClasses.h"
#include "EngineReplicationInfoClasses.h"
#include "EnginePawnClasses.h"
#include "EngineAIClasses.h"
#include "EngineMaterialClasses.h"
#include "EngineTerrainClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineParticleClasses.h"
#include "EngineAnimClasses.h"
#include "EngineDecalClasses.h"
#include "EngineFogVolumeClasses.h"
#include "EngineMeshClasses.h"
#include "EnginePrefabClasses.h"
#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "EngineFoliageClasses.h"
#include "EngineSpeedTreeClasses.h"
#include "EngineLensFlareClasses.h"
#include "EngineSplineClasses.h"	
#include "EngineProcBuildingClasses.h"
#include "EngineFluidClasses.h"
#include "EngineCameraClasses.h"
#include "EnginePlatformInterfaceClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef VERIFY_CLASS_SIZES
#undef NAMES_ONLY
}

#endif

/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsEngine( INT& Lookup )
{
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_GAMEENGINE
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_BASE
	AUTO_INITIALIZE_REGISTRANTS_ENGINE
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_AI
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_ANIM
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_DECAL
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_FOGVOLUME
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_MESH
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_INTERPOLATION
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_MATERIAL
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_PARTICLE
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_PHYSICS
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_FORCEFIELD
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_PREFAB
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_SEQUENCE
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_SOUND
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_TERRAIN
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_USERINTERFACE
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_UIPRIVATE
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_SCENE
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_FOLIAGE
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_FLUID
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_SPEEDTREE
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_LENSFLARE
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_TEXTURE
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_AUDIODEVICE
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_CONTROLLER
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_PAWN
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_LIGHT
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_SKELETALMESH
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_SPLINE
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_PROCBUILDING
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_REPLICATIONINFO
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_CAMERA
	AUTO_INITIALIZE_REGISTRANTS_ENGINE_PLATFORMINTERFACE
}

/**
 * Auto generates names.
 */
void AutoGenerateNamesEngine()
{
	#define NAMES_ONLY
	#define AUTOGENERATE_NAME(name) ENGINE_##name = FName(TEXT(#name));
		#include "EngineNames.h"
	#undef AUTOGENERATE_NAME

	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "EngineGameEngineClasses.h"
	#include "EngineBaseClasses.h"
	#include "EngineClasses.h"
 	#include "EngineTextureClasses.h"
	#include "EngineLightClasses.h"
	#include "EngineSkeletalMeshClasses.h"
	#include "EngineControllerClasses.h"
	#include "EngineReplicationInfoClasses.h"
	#include "EnginePawnClasses.h"
	#include "EngineAIClasses.h"
	#include "EngineMaterialClasses.h"
	#include "EngineTerrainClasses.h"
	#include "EnginePhysicsClasses.h"
	#include "EngineForceFieldClasses.h"
	#include "EngineSequenceClasses.h"
	#include "EngineSoundClasses.h"
	#include "EngineInterpolationClasses.h"
	#include "EngineParticleClasses.h"
	#include "EngineAnimClasses.h"
	#include "EngineDecalClasses.h"
	#include "EngineFogVolumeClasses.h"
	#include "UnFracturedStaticMesh.h"
	#include "EngineMeshClasses.h"
	#include "EnginePrefabClasses.h"
	#include "EngineUserInterfaceClasses.h"
	#include "EngineUIPrivateClasses.h"
	#include "EngineSceneClasses.h"
	#include "EngineFoliageClasses.h"
	#include "EngineSpeedTreeClasses.h"
	#include "EngineLensFlareClasses.h"
	#include "EngineFluidClasses.h"
	#include "EngineSplineClasses.h"	
	#include "EngineProcBuildingClasses.h"
	#include "EngineCameraClasses.h"
	#include "EnginePlatformInterfaceClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef NAMES_ONLY
}

//GFx Includes
#if WITH_GFx
#include "ScaleformEngine.h"
#endif //WITH_GFx

// Register input keys.
#define DEFINE_KEY(Name, Unused) FName KEY_##Name;
#include "UnKeys.h"
#undef DEFINE_KEY

// reenable optimizations after all the static linking init
PRAGMA_ENABLE_OPTIMIZATION

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

/** Global engine pointer. */
UEngine*	GEngine				= NULL;

/** Enables texture streaming support.  Must not be changed after engine is initialized. */
UBOOL GUseTextureStreaming = TRUE;

/** Whether screen door fade effects are used in game */
UBOOL		GAllowScreenDoorFade = FALSE;

/** Cache for property window queries*/
FPropertyWindowDataCache* GPropertyWindowDataCache = NULL;
/** 
 * Whether Nvidia's Stereo 3d can be used.  
 * Changing this requires a shader recompile, and enabling it uses an extra texture sampler (which may cause some materials to fail compiling).
 */
UBOOL		GAllowNvidiaStereo3d = FALSE;
/** Largest-resolution mip-level to use for lightmaps, zero-based. (Note: 0 is the highest-resolution mip-level.) */
FLOAT		GLargestLightmapMipLevel = 0;
/** Whether to visualize the lightmap selected by the Debug Camera. */
UBOOL		GShowDebugSelectedLightmap = FALSE;

#if !FINAL_RELEASE
/** TRUE if we debug material names with SCOPED_DRAW_EVENT. Toggle with "ShowMaterialDrawEvents" console command. */
UBOOL			GShowMaterialDrawEvents = FALSE;
#endif

/** Global GamePatchHelper object that a game can override */
FGamePatchHelper* GGamePatchHelper = NULL;

/** Whether to show the engine version */
UBOOL GShowVersion = FALSE;
/** Whether to show the FPS counter */
UBOOL GShowFpsCounter = FALSE;
/** Whether to show the FPS counter */
UBOOL GShowHitches = FALSE;
/** Whether to show the memory summary stats. */
UBOOL GShowMemorySummaryStats = FALSE;
/** Whether to show the level stats */
static UBOOL GShowLevelStats = FALSE;
/** Whether to show overhead view of streaming volumes */
UBOOL GShowLevelStatusMap = FALSE;
/** Whether to show the CPU thread and GPU frame times */
UBOOL GShowUnitTimes = FALSE;
/** Whether to show the CPU thread and GPU max frame times */
UBOOL GShowUnitMaxTimes = FALSE;
/** Whether to draw a simple unit time graph on the HUD */
UBOOL GShowUnitTimeGraph = FALSE;
/** Whether to filter unit time data for graphs/peaks; Default to filtered times as raw times are much too noisy in charts */
UBOOL GShowRawUnitTimes = FALSE;
/** Whether to show active sound waves */
UBOOL GShowSoundWaves = FALSE;
/** Whether to show active sound cues */
UBOOL GShowSoundCues = FALSE;

enum EShowSoundsFlags
{
	SSF_Disabled		= 0x00,
	SSF_Debug			= 0x01,
	SSF_Sort_Distance	= 0x02,
	SSF_Sort_Class		= 0x04,
	SSF_Sort_Name		= 0x08,
	SSF_Sort_WavesNum	= 0x10,
	SSF_Sort_Disabled	= 0x20,
};

/** Whether to show active sounds includes sound waves and sound cues */
DWORD GShowSounds;

/** Whether to show AI stats */
UBOOL GShowAIStats = FALSE;
#if !FINAL_RELEASE
/** 
 *	A global to allow turning off the 'NOT RUNNING IN HD' warning.
 *	Is enabled by default - and is *not* stored in an ini file 
 *	so it will always show up when you launch in non-HD mode.
 *
 *	Disable via the console command "TOGGLEHDWARNING"
 */
UBOOL GbWarn_NotRunningInHD = TRUE;
#endif

#if PS3
/** Show GCM memory stat graphs (host,local) */
UBOOL GShowPS3GcmMemoryStats=FALSE;
#endif

/** Whether texture memory has been corrupted because we ran out of memory in the pool. */
UBOOL GIsTextureMemoryCorrupted = FALSE;

#if !FINAL_RELEASE
/** Whether PrepareMapChange is attempting to load a map that doesn't exist */
UBOOL GIsPrepareMapChangeBroken = FALSE;
#endif


// We expose these variables to everyone as we need to access them in other files via an extern
FLOAT GAverageFPS = 0.0f;
FLOAT GAverageMS = 0.0f;


// We expose these variables to everyone as we need to access them in other files via an extern
FLOAT GUnit_RawRenderThreadTime = 0.0f;
FLOAT GUnit_RawGameThreadTime = 0.0f;
FLOAT GUnit_RawGPUFrameTime = 0.0f;
FLOAT GUnit_RawFrameTime = 0.0f;
FLOAT GUnit_RawSpuTime	= 0.0f;

// Unit frame times filtered with a simple running average
FLOAT GUnit_RenderThreadTime = 0.0f;
FLOAT GUnit_GameThreadTime = 0.0f;
FLOAT GUnit_GPUFrameTime = 0.0f;
FLOAT GUnit_FrameTime = 0.0f;
FLOAT GUnit_SpuTime	= 0.0f;

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	const INT GUnit_NumberOfSamples = 500;
	INT GUnit_CurrentIndex = 0;
	TArray<FLOAT> GUnit_RenderThreadTimes(GUnit_NumberOfSamples);
	TArray<FLOAT> GUnit_GameThreadTimes(GUnit_NumberOfSamples);
	TArray<FLOAT> GUnit_GPUFrameTimes(GUnit_NumberOfSamples);
	TArray<FLOAT> GUnit_FrameTimes(GUnit_NumberOfSamples);
	TArray<FLOAT> GUnit_SpuTimes(GUnit_NumberOfSamples);
	FStableSample GUnit_StableFrameTime(1000.0f);	// stable for 1 sec
#endif	//#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE


#if WITH_EDITOR
	// Note: This is a global because GEngine is often not loaded when we need to access this set
	TSet<UPackage*> MaterialPackagesWithDependentChanges;
#endif

IMPLEMENT_COMPARE_CONSTREF( FString, UnEngine, { return appStricmp(*A,*B); } )

/*-----------------------------------------------------------------------------
	Object class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UEngineTypes);
IMPLEMENT_CLASS(UEngineBaseTypes);
IMPLEMENT_CLASS(UEngine);
IMPLEMENT_CLASS(UGameEngine);
IMPLEMENT_CLASS(UDEPRECATED_SaveGameSummary);
IMPLEMENT_CLASS(UObjectReferencer);


/**
 * Compresses and decompresses thumbnails using the PNG format.  This is used by the package loading and
 * saving process.
 */
class FPNGThumbnailCompressor
	: public FThumbnailCompressionInterface
{

public:

	/**
	 * Compresses an image
	 *
	 * @param	InUncompressedData	The uncompressed image data
	 * @param	InWidth				Width of the image
	 * @param	InHeight			Height of the image
	 * @param	OutCompressedData	[Out] Compressed image data
	 *
	 * @return	TRUE if the image was compressed successfully, otherwise FALSE if an error occurred
	 */
	virtual UBOOL CompressImage( const TArray< BYTE >& InUncompressedData, const INT InWidth, const INT InHeight, TArray< BYTE >& OutCompressedData )
	{
#if !CONSOLE && defined(_MSC_VER)
		OutCompressedData.Reset();
		if( InUncompressedData.Num() > 0 )
		{
			FPNGHelper PNG;
			PNG.InitRaw( &InUncompressedData( 0 ), InUncompressedData.Num(), InWidth, InHeight );
			OutCompressedData = PNG.GetCompressedData();
		}
#endif

		return TRUE;
	}


	/**
	 * Decompresses an image
	 *
	 * @param	InCompressedData	The compressed image data
	 * @param	InWidth				Width of the image
	 * @param	InHeight			Height of the image
	 * @param	OutUncompressedData	[Out] Uncompressed image data
	 *
	 * @return	TRUE if the image was decompressed successfully, otherwise FALSE if an error occurred
	 */
	virtual UBOOL DecompressImage( const TArray< BYTE >& InCompressedData, const INT InWidth, const INT InHeight, TArray< BYTE >& OutUncompressedData )
	{
#if !CONSOLE && defined(_MSC_VER)
		OutUncompressedData.Reset();
		if( InCompressedData.Num() > 0 )
		{
			FPNGHelper PNG;
			PNG.InitCompressed( &InCompressedData( 0 ), InCompressedData.Num(), InWidth, InHeight );
			OutUncompressedData = PNG.GetRawData();		// @todo CB: Eliminate image copy here? (decompress straight to buffer)
		}
#endif

		return TRUE;
	}


};


/**
 * Helper class inhibiting screen saver by e.g. moving the mouse by 0 pixels every 50 seconds on Windows.
 */
class FScreenSaverInhibitor : public FRunnable
{
	// FRunnable interface. Not required to be implemented.
	UBOOL Init() { return TRUE; }
	void Stop() {}
	void Exit() {}

	/**
	 * Prevents screensaver from kicking in by calling appPreventScreenSaver every 50 seconds.
	 * 
	 * @return	never returns
	 */
	DWORD Run()
	{
		while( TRUE )
		{
			appSleep( 50 );
			extern void appPreventScreenSaver();
			appPreventScreenSaver();
		}
		return 0;
	}
};

/*-----------------------------------------------------------------------------
	Engine init and exit.
-----------------------------------------------------------------------------*/

//
// Construct the engine
//
UEngine::UEngine()
{
}

//
// Init class.
//
void UEngine::StaticConstructor()
{
}

//
// Initialize the engine.
//
void UEngine::Init()
{
	// Add input key names.
	#define DEFINE_KEY(Name, Unused) KEY_##Name = FName(TEXT(#Name));
	#include "UnKeys.h"
	#undef DEFINE_KEY

	// Subsystems.
	FURL::StaticInit();

	// Check for overrides to the default map on the command line
	TCHAR MapName[512];
	if ( Parse(appCmdLine(), TEXT("DEFAULTMAP="), MapName, ARRAY_COUNT(MapName)) )
	{
		debugf(TEXT("Overriding default map to %s"), MapName);

		FString MapString = FString(MapName);

		FURL::DefaultMap = MapString;
		FURL::DefaultLocalMap = MapString;
		FURL::DefaultTransitionMap = MapString;
	}

#if WITH_SUBSTANCE_AIR
	SubstanceAir::Helpers::SetupSubstanceAir();
#endif // WITH_SUBSTANCE_AIR
	
	// Initialize random number generator.
	if( GIsBenchmarking || ParseParam(appCmdLine(),TEXT("FIXEDSEED")) )
	{
		appRandInit( 0 );
		appSRandInit( 0 );
	}
	else
	{
		appRandInit( appCycles() );
		appSRandInit( appCycles() );
	}

	// Add to root.
	AddToRoot();

	// Assign thumbnail compressor/decompressor
	FObjectThumbnail::SetThumbnailCompressor( new FPNGThumbnailCompressor() );

	// Set the fonts used by linked object editor
	FLinkedObjDrawUtils::InitFonts(this->SmallFont);

	if( !GIsUCCMake )
	{	
		// Make sure the engine is loaded, because some classes need GEngine->DefaultMaterial to be valid
		LoadObject<UClass>(UEngine::StaticClass()->GetOuter(), *UEngine::StaticClass()->GetName(), NULL, LOAD_Quiet|LOAD_NoWarn, NULL );
		// This reads the Engine.ini file to get the proper DefaultMaterial, etc.
		LoadConfig();
		// Initialize engine's object references.
		InitializeObjectReferences();

		// Ensure all native classes are loaded.
		UObject::BeginLoad();
		for( TObjectIterator<UClass> It; It; ++It )
		{
			UClass* Class = *It;
			if( !Class->GetLinker() )
			{
				LoadObject<UClass>( Class->GetOuter(), *Class->GetName(), NULL, LOAD_Quiet|LOAD_NoWarn, NULL );
			}
		}
		UObject::EndLoad();

		for ( TObjectIterator<UClass> It; It; ++It )
		{
			It->ConditionalLink();
		}

#if WITH_EDITOR && defined(_MSC_VER)
		// Create debug exec.	
		GDebugToolExec = new FDebugToolExec;
#endif		

#if USING_REMOTECONTROL
		// Create RemoteControl.
		if ( !GIsUCC )
		{
			RegisterCoreRemoteControlPages();
			RemoteControlExec = new FRemoteControlExec;
		}
#endif

		// Set colors for selection materials
		SelectedMaterialColor = DefaultSelectedMaterialColor;
		UnselectedMaterialColor = FLinearColor::Black;

		// Create the global chart-drawing object.
		GStatChart = new FStatChart();

		if (GConfig)
		{
			UBOOL bTemp = TRUE;
			GConfig->GetBool(TEXT("Engine.Engine"), TEXT("bEnableOnScreenDebugMessages"), bTemp, GEngineIni);
			bEnableOnScreenDebugMessages = bTemp ? TRUE : FALSE;
			bEnableOnScreenDebugMessagesDisplay = bEnableOnScreenDebugMessages;

			GConfig->GetBool(TEXT("DevOptions.Debug"), TEXT("ShowSelectedLightmap"), GShowDebugSelectedLightmap, GEngineIni);
		}
	}

	if ( GConfig )
	{
		GConfig->GetFloat(TEXT("Engine.Engine"), TEXT("NearClipPlane"), GNearClippingPlane, GEngineIni);
	}

	if( GIsEditor && !GIsUCCMake )
	{
		UWorld::CreateNew();
	}

	if ( GlobalTranslationContext == NULL )
	{
		GlobalTranslationContext = ConstructObject<UTranslationContext>(UTranslationContext::StaticClass(), this);
		
		UStringsTag* StringsTag = ConstructObject<UStringsTag>(UStringsTag::StaticClass(), GlobalTranslationContext);
		GlobalTranslationContext->RegisterTranslatorTag(StringsTag);
	}  

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	// Zero out unit time history
	{
		appMemzero( GUnit_RenderThreadTimes.GetData(), sizeof( FLOAT ) * GUnit_NumberOfSamples );
		appMemzero( GUnit_GameThreadTimes.GetData(), sizeof( FLOAT ) * GUnit_NumberOfSamples );
		appMemzero( GUnit_GPUFrameTimes.GetData(), sizeof( FLOAT ) * GUnit_NumberOfSamples );
		appMemzero( GUnit_FrameTimes.GetData(), sizeof( FLOAT ) * GUnit_NumberOfSamples );
	}
#endif

	debugf( NAME_Init, TEXT("UEngine initialized") );
}

/**
 * Loads a special material and verifies that it is marked as a special material (some shaders
 * will only be compiled for materials marked as "special engine material")
 *
 * @param MaterialName Fully qualified name of a material to load/find
 * @param Material Reference to a material object pointer that will be filled out
 * @param bCheckUsage Check if the material has been marked to be used as a special engine material
 */
void LoadSpecialMaterial(const FString& MaterialName, UMaterial*& Material, UBOOL bCheckUsage)
{
	// only bother with materials that aren't already loaded
	if (Material == NULL)
	{
		// find or load the object
		Material = LoadObject<UMaterial>(NULL, *MaterialName, NULL, LOAD_None, NULL);	

		if (!Material)
		{
#if CONSOLE
			debugf(TEXT("ERROR: Failed to load special material '%s'. This will probably have bad consequences (depending on its use)"), *MaterialName);
#else
			appErrorf(TEXT("Failed to load special material '%s'"), *MaterialName);
#endif
		}
		// if the material wasn't marked as being a special engine material, then not all of the shaders 
		// will have been compiled on it by this point, so we need to compile them and alert the use
		// to set the bit
		else if (!Material->bUsedAsSpecialEngineMaterial && bCheckUsage) 
		{
#if CONSOLE
			// consoles must have the flag set properly in the editor
			appErrorf(TEXT("The special material (%s) was not marked with bUsedAsSpecialEngineMaterial. Make sure this flag is set in the editor, save the package, and compile shaders for this platform"), *MaterialName);
#else
			Material->bUsedAsSpecialEngineMaterial = TRUE;
			Material->MarkPackageDirty();

			// make sure all necessary shaders for the default are compiled, now that the flag is set
			Material->CacheResourceShaders(GRHIShaderPlatform);

			appMsgf(AMT_OK, TEXT("The special material (%s) has not been marked with bUsedAsSpecialEngineMaterial.\nThis will prevent shader precompiling properly, so the flag has been set automatically.\nMake sure to save the package and distribute to everyone using this material."), *MaterialName);
#endif
		}
	}
}
/**
 * Loads all Engine object references from their corresponding config entries.
 */
void UEngine::InitializeObjectReferences()
{
	if (DefaultMaterialName.Len() == 0)
	{
		appErrorf(TEXT("Invalid DefaultMaterialName! Make sure that script was compiled successfully, especially if running with -unattended."));
	}
	if (DefaultDecalMaterialName.Len() == 0)
	{
		appErrorf(TEXT("Invalid DefaultDecalMaterialName! Make sure that script was compiled successfully, especially if running with -unattended."));
	}

	// initialize the special engine/editor materials
	// Materials that are always needed
	LoadSpecialMaterial(DefaultMaterialName, DefaultMaterial, TRUE);
	LoadSpecialMaterial(DefaultDecalMaterialName, DefaultDecalMaterial, TRUE);
	if (AllowDebugViewmodes() 
		// During cooking, don't load materials used for debug viewmodes when debug viewmodes are not allowed for the target platform.
		// Otherwise the materials will get cooked even though their packages (EngineDebugMaterials) is not in the StartupPackages for the target platform.
		&& !(GIsCooking && !AllowDebugViewmodes(ShaderPlatformFromUE3Platform(GCookingTarget))))
	{
		// Materials that are needed in-game if debug viewmodes are allowed
		LoadSpecialMaterial(WireframeMaterialName, WireframeMaterial, TRUE);
		LoadSpecialMaterial(LevelColorationLitMaterialName, LevelColorationLitMaterial, TRUE);
		LoadSpecialMaterial(LevelColorationUnlitMaterialName, LevelColorationUnlitMaterial, TRUE);
		LoadSpecialMaterial(LightingTexelDensityName, LightingTexelDensityMaterial, FALSE);
		LoadSpecialMaterial(ShadedLevelColorationLitMaterialName, ShadedLevelColorationLitMaterial, TRUE);
		LoadSpecialMaterial(ShadedLevelColorationUnlitMaterialName, ShadedLevelColorationUnlitMaterial, TRUE);
		LoadSpecialMaterial(VertexColorMaterialName, VertexColorMaterial, FALSE);
		LoadSpecialMaterial(TerrainErrorMaterialName, TerrainErrorMaterial, TRUE);
	}

	// Materials that may or may not be needed when debug viewmodes are disabled but haven't been fixed up yet
	LoadSpecialMaterial(EmissiveTexturedMaterialName, EmissiveTexturedMaterial, FALSE);
	LoadSpecialMaterial(SceneCaptureReflectActorMaterialName, SceneCaptureReflectActorMaterial, FALSE);
	LoadSpecialMaterial(SceneCaptureCubeActorMaterialName, SceneCaptureCubeActorMaterial, FALSE);
	LoadSpecialMaterial(DefaultFogVolumeMaterialName, DefaultFogVolumeMaterial, FALSE);
	LoadSpecialMaterial(DefaultUICaretMaterialName, DefaultUICaretMaterial, FALSE);
	LoadSpecialMaterial(RemoveSurfaceMaterialName, RemoveSurfaceMaterial, FALSE);	

	if (GIsEditor && 
		!(GIsCooking && ((GCookingTarget & UE3::PLATFORM_Stripped) != 0))
		)
	{
		// Materials that are only needed in the editor
		LoadSpecialMaterial(GeomMaterialName, GeomMaterial, FALSE);
		LoadSpecialMaterial(TickMaterialName, TickMaterial, FALSE);
		LoadSpecialMaterial(CrossMaterialName, CrossMaterial, FALSE);
		LoadSpecialMaterial(EditorBrushMaterialName, EditorBrushMaterial, FALSE);
		LoadSpecialMaterial(HeatmapMaterialName, HeatmapMaterial, FALSE);
		LoadSpecialMaterial(BoneWeightMaterialName, BoneWeightMaterial, FALSE);
		LoadSpecialMaterial(TangentColorMaterialName, TangentColorMaterial, FALSE);
#if WITH_EDITOR
		LoadSpecialMaterial(MobileEmulationMasterMaterialName, MobileEmulationMasterMaterial, TRUE);
#endif
		LoadSpecialMaterial(VertexColorViewModeMaterialName_ColorOnly, VertexColorViewModeMaterial_ColorOnly, FALSE);
		LoadSpecialMaterial(VertexColorViewModeMaterialName_AlphaAsColor, VertexColorViewModeMaterial_AlphaAsColor, FALSE);
		LoadSpecialMaterial(VertexColorViewModeMaterialName_RedOnly, VertexColorViewModeMaterial_RedOnly, FALSE);
		LoadSpecialMaterial(VertexColorViewModeMaterialName_GreenOnly, VertexColorViewModeMaterial_GreenOnly, FALSE);
		LoadSpecialMaterial(VertexColorViewModeMaterialName_BlueOnly, VertexColorViewModeMaterial_BlueOnly, FALSE);
		LoadSpecialMaterial(ProcBuildingSimpleMaterialName, ProcBuildingSimpleMaterial, FALSE);
	}
	else
	{
		LoadSpecialMaterial(DefaultMaterialName, GeomMaterial, FALSE);
		LoadSpecialMaterial(DefaultMaterialName, TickMaterial, FALSE);
		LoadSpecialMaterial(DefaultMaterialName, CrossMaterial, FALSE);
		LoadSpecialMaterial(DefaultMaterialName, EditorBrushMaterial, FALSE);
	}

	if( DefaultTexture == NULL )
	{
		DefaultTexture = LoadObject<UTexture2D>(NULL, *DefaultTextureName, NULL, LOAD_None, NULL);	
	}

	if( ScreenDoorNoiseTexture == NULL )
	{
		ScreenDoorNoiseTexture = LoadObject<UTexture2D>(NULL, *ScreenDoorNoiseTextureName, NULL, LOAD_None, NULL);	
	}
	
	if( ImageGrainNoiseTexture == NULL )
	{
		ImageGrainNoiseTexture = LoadObject<UTexture2D>(NULL, *ImageGrainNoiseTextureName, NULL, LOAD_None, NULL);	
	}

	if( RandomAngleTexture == NULL )
	{
		RandomAngleTexture = LoadObject<UTexture2D>(NULL, *RandomAngleTextureName, NULL, LOAD_None, NULL);	
	}

	if( RandomNormalTexture == NULL )
	{
		RandomNormalTexture = LoadObject<UTexture2D>(NULL, *RandomNormalTextureName, NULL, LOAD_None, NULL);	
	}

	if( RandomMirrorDiscTexture == NULL )
	{
		RandomMirrorDiscTexture = LoadObject<UTexture2D>(NULL, *RandomMirrorDiscTextureName, NULL, LOAD_None, NULL);	
	}

	if( WeightMapPlaceholderTexture == NULL )
	{
		WeightMapPlaceholderTexture = LoadObject<UTexture2D>(NULL, *WeightMapPlaceholderTextureName, NULL, LOAD_None, NULL);	
	}

    if (LightMapDensityTexture == NULL)
	{
		LightMapDensityTexture = LoadObject<UTexture2D>(NULL, *LightMapDensityTextureName, NULL, LOAD_None, NULL);
	}

    if (LightMapDensityNormal == NULL)
	{
		LightMapDensityNormal = LoadObject<UTexture2D>(NULL, *LightMapDensityNormalName, NULL, LOAD_None, NULL);
	}

	if ( DefaultPhysMaterial == NULL )
	{
		DefaultPhysMaterial = LoadObject<UPhysicalMaterial>(NULL, *DefaultPhysMaterialName, NULL, LOAD_None, NULL);	
	}

	if ( LandscapeHolePhysMaterial == NULL )
	{
		LandscapeHolePhysMaterial = LoadObject<UPhysicalMaterial>(NULL, *LandscapeHolePhysMaterialName, NULL, LOAD_None, NULL);	
	}

	if(ApexDamageParams == NULL)
	{
		if(ApexDamageParamsName.Len() > 0)
		{
			ApexDamageParams = LoadObject<UApexDestructibleDamageParameters>(NULL, *ApexDamageParamsName, NULL, LOAD_None, NULL);	
		}
	}

	// loading objects here will cause them to be cooked into some seekfree package, but if the target we're cooking for has different
	// settings in their .ini's, then they won't want these objects.
	if (GIsCooking)
	{
		return;
	}

	if ( ConsoleClass == NULL )
	{
		ConsoleClass = LoadClass<UConsole>(NULL, *ConsoleClassName, NULL, LOAD_None, NULL);
	}

	if ( GameViewportClientClass == NULL )
	{
		GameViewportClientClass = LoadClass<UGameViewportClient>(NULL, *GameViewportClientClassName, NULL, LOAD_None, NULL);
	}

	if ( LocalPlayerClass == NULL )
	{
		LocalPlayerClass = LoadClass<ULocalPlayer>(NULL, *LocalPlayerClassName, NULL, LOAD_None, NULL);
	}

	if ( DataStoreClientClass == NULL )
	{
		DataStoreClientClass = LoadClass<UDataStoreClient>(NULL, *DataStoreClientClassName, NULL, LOAD_None, NULL);
	}

#if WITH_UE3_NETWORKING
	const TCHAR* OSSName = appGetOSSPackageName();
	if (OnlineSubsystemClass == NULL && OSSName != NULL)
	{
		OnlineSubsystemClass = LoadClass<UOnlineSubsystem>(NULL, *FString::Printf(TEXT("OnlineSubsystem%s.OnlineSubsystem%s"), OSSName, OSSName), NULL, LOAD_None, NULL);
	}
#endif	//#if WITH_UE3_NETWORKING

	if (GIsEditor == TRUE)
	{
		// Load the post process chain used for skeletal mesh thumbnails
		if( ThumbnailSkeletalMeshPostProcess == NULL && ThumbnailSkeletalMeshPostProcessName.Len() )
		{
			ThumbnailSkeletalMeshPostProcess = LoadObject<UPostProcessChain>(NULL,*ThumbnailSkeletalMeshPostProcessName,NULL,LOAD_None,NULL);
		}
		// Load the post process chain used for particle system thumbnails
		if( ThumbnailParticleSystemPostProcess == NULL && ThumbnailParticleSystemPostProcessName.Len() )
		{
			ThumbnailParticleSystemPostProcess = LoadObject<UPostProcessChain>(NULL,*ThumbnailParticleSystemPostProcessName,NULL,LOAD_None,NULL);
		}
		// Load the post process chain used for material thumbnails
		if( ThumbnailMaterialPostProcess == NULL && ThumbnailMaterialPostProcessName.Len() )
		{
			ThumbnailMaterialPostProcess = LoadObject<UPostProcessChain>(NULL,*ThumbnailMaterialPostProcessName,NULL,LOAD_None,NULL);
		}
	}
	else
	{
		// Load the post process chain used for skeletal mesh thumbnails
		if( ThumbnailSkeletalMeshPostProcess == NULL && DefaultPostProcessName.Len() )
		{
			ThumbnailSkeletalMeshPostProcess = LoadObject<UPostProcessChain>(NULL,*DefaultPostProcessName,NULL,LOAD_None,NULL);
		}
		// Load the post process chain used for particle system thumbnails
		if( ThumbnailParticleSystemPostProcess == NULL && DefaultPostProcessName.Len() )
		{
			ThumbnailParticleSystemPostProcess = LoadObject<UPostProcessChain>(NULL,*DefaultPostProcessName,NULL,LOAD_None,NULL);
		}
		// Load the post process chain used for material thumbnails
		if( ThumbnailMaterialPostProcess == NULL && DefaultPostProcessName.Len() )
		{
			ThumbnailMaterialPostProcess = LoadObject<UPostProcessChain>(NULL,*DefaultPostProcessName,NULL,LOAD_None,NULL);
		}
	}

	// Load the default UI post process chain
	if( DefaultUIScenePostProcess == NULL && DefaultUIScenePostProcessName.Len() )
	{
		DefaultUIScenePostProcess = LoadObject<UPostProcessChain>(NULL,*DefaultUIScenePostProcessName,NULL,LOAD_None,NULL);
	}

	if( DefaultSound == NULL && DefaultSoundName.Len() )
	{
		DefaultSound = LoadObject<USoundNodeWave>( NULL, *DefaultSoundName, NULL, LOAD_None, NULL );
	}

	// set the font object pointers
	if( TinyFont == NULL && TinyFontName.Len() )
	{
		TinyFont = LoadObject<UFont>(NULL,*TinyFontName,NULL,LOAD_None,NULL);
	}
	if( SmallFont == NULL && SmallFontName.Len() )
	{
		SmallFont = LoadObject<UFont>(NULL,*SmallFontName,NULL,LOAD_None,NULL);
	}
	if( MediumFont == NULL && MediumFontName.Len() )
	{
		MediumFont = LoadObject<UFont>(NULL,*MediumFontName,NULL,LOAD_None,NULL);
	}
	if( LargeFont == NULL && LargeFontName.Len() )
	{
		LargeFont = LoadObject<UFont>(NULL,*LargeFontName,NULL,LOAD_None,NULL);
	}
	if( SubtitleFont == NULL && SubtitleFontName.Len() )
	{
		SubtitleFont = LoadObject<UFont>(NULL,*SubtitleFontName,NULL,LOAD_None,NULL);
	}

	// Additional fonts.
	AdditionalFonts.Empty( AdditionalFontNames.Num() );
	for ( INT FontIndex = 0 ; FontIndex < AdditionalFontNames.Num() ; ++FontIndex )
	{
		const FString& FontName = AdditionalFontNames(FontIndex);
		UFont* NewFont = NULL;
		if( FontName.Len() )
		{
			NewFont = LoadObject<UFont>(NULL,*FontName,NULL,LOAD_None,NULL);
		}
		AdditionalFonts.AddItem( NewFont );
	}

}

//
// Exit the engine.
//
void UEngine::FinishDestroy()
{
	// Remove from root.
	RemoveFromRoot();

	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Clean up debug tool.
		delete GDebugToolExec;
		GDebugToolExec		= NULL;

#if USING_REMOTECONTROL
		// Clean up RemoteControl.
		delete RemoteControlExec;
		RemoteControlExec		= NULL;
#endif

		// Shut down all subsystems.
		Client				= NULL;
		GEngine				= NULL;
		FURL::StaticExit();

		delete GStatChart;
		GStatChart			= NULL;
	}
	Super::FinishDestroy();
}

//
// Progress indicator.
//
void UEngine::SetProgress( EProgressMessageType MessageType, const FString& Title, const FString& Message )
{
}

void UEngine::CleanupGameViewport()
{
	// Clean up the viewports that have been closed.
	for(FLocalPlayerIterator It(this);It;++It)
	{
		if(It->ViewportClient && !It->ViewportClient->Viewport)
		{
			if (!It->IsPendingKill() && !It->HasAnyFlags(RF_Unreachable))
			{
				It->eventViewportClosed();
			}

			It->ViewportClient = NULL;
#if WITH_GFx
			FGFxEngine::GetEngine()->RemovePlayerState ( It.GetIndex() );
#endif // WITH_GFx
			It.RemoveCurrent();
		}
	}

	if ( GameViewport != NULL && GameViewport->Viewport == NULL )
	{
		GameViewport->DetachViewportClient();
		GameViewport = NULL;
	}
}

FViewport* UEngine::GetAViewport()
{
	if(GameViewport)
	{
		return GameViewport->Viewport;
	}

	return NULL;
}

/** @return the GIsEditor flag setting */
UBOOL UEngine::IsEditor()
{
	return GIsEditor;
}

/** @return the GIsGame flag setting */
UBOOL UEngine::IsGame()
{
	return GIsGame;
}

/**
 * Returns a pointer to the current world.
 */
AWorldInfo* UEngine::GetCurrentWorldInfo()
{
	return GWorld->GetWorldInfo();
}

/**
 * Returns version info from the engine
 */
FString UEngine::GetBuildDate( void )
{
	FString BuildDate = ANSI_TO_TCHAR( __DATE__ );
	return BuildDate;
}

/**
 * Returns the UUID of the device (or empty if cannot be determined)
 */
FString UEngine::GetDeviceUUID()
{
#if _WINDOWS || IPHONE
	extern FString appGetMacAddress();
	extern FString MD5HashAnsiString(const TCHAR*);
	return MD5HashAnsiString(*appGetMacAddress());
#else
	appErrorf(TEXT("GetDeviceUUID is not supported on this platform!"));
	return TEXT("");
#endif
}

/**
 * Return OS version number
 */
FLOAT UEngine::GetOSVersion()
{
#if IPHONE
	return IPhoneGetOSVersion();
#else
	// Simply return 0.0 so we can test things on PC
	// TODO: implement this for other platforms
	return 0.0;
#endif
}

/**
 * Returns the push notification token for the device + application
 */
FString UEngine::GetDevicePushNotificationToken()
{
#if IPHONE
	ANSICHAR ValString[256];
	IPhoneGetDevicePushNotificationToken( ValString, ARRAY_COUNT(ValString) - 1 );
	return FString(ANSI_TO_TCHAR(ValString));
#else
	// Simply return the default, empty string
	// TODO: implement this for other platforms
	return FString();
#endif
}

/**
 * Builds and adds correct seperators for a bug string to be sent to debug console 
 */
FString UEngine::BuildBugSubmissionString(const FString& bugfield, const FString& bugfielddata)
{
	return bugfield + ":" + bugfielddata + "|";
}

/**
 * Return true if there is a network connection
 */
UBOOL UEngine::HasNetworkConnection()
{
#if IPHONE
	return IPhoneIsNetConnected();
#else
	// Simply return true so we can test on PC
	// TODO: implement this for other platforms
	return TRUE;
#endif
}


/**
 * Returns the engine's default tiny font
 */
UFont* UEngine::GetTinyFont()
{
	return GEngine->TinyFont;
}

/**
 * Returns the engine's default small font
 */
UFont* UEngine::GetSmallFont()
{
	return GEngine->SmallFont;
}

/**
 * Returns the engine's default medium font
 */
UFont* UEngine::GetMediumFont()
{
	return GEngine->MediumFont;
}

/**
 * Returns the engine's default large font
 */
UFont* UEngine::GetLargeFont()
{
	return GEngine->LargeFont;
}

/**
 * Returns the engine's default subtitle font
 */
UFont* UEngine::GetSubtitleFont()
{
	return GEngine->SubtitleFont;
}

/**
 * Returns the specified additional font.
 *
 * @param	AdditionalFontIndex		Index into the AddtionalFonts array.
 */
UFont* UEngine::GetAdditionalFont(INT AdditionalFontIndex)
{
	return GEngine->AdditionalFonts(AdditionalFontIndex);
}

/** @return whether we're currently running in splitscreen (more than one local player) */
UBOOL UEngine::IsSplitScreen()
{
	return (GEngine->GamePlayers.Num() > 1);
}

/** @return whether we're currently running with stereoscopic 3D enabled */
UBOOL UEngine::IsStereoscopic3D()
{
	const UBOOL bS3DActive = 
#if DWTRIOVIZSDK
		DwTriovizImpl_IsTriovizActive();
#else
		FALSE;
#endif
	return bS3DActive;
}

UBOOL UEngine::IsUsingES2Renderer()
{
	return GUsingES2RHI;
}

/** @return the audio device (will be None if sound is disabled) */
UAudioDevice* UEngine::GetAudioDevice()
{
	UAudioDevice* AudioDevice = NULL;

	if( GEngine->Client )
	{
		AudioDevice = GEngine->Client->GetAudioDevice();
	}

	return( AudioDevice );
}

// WITH_REALD BEGIN
/**
 * Returns whether RealD stereo rendering is enabled or not
 */
UBOOL UEngine::IsRealDStereoEnabled()
{
#if WITH_REALD
    return RealD::IsStereoEnabled();
#else
    return FALSE;
#endif
}
// WITH REALD END
 
/** @return Returns the name of the last movie that was played. */
FString UEngine::GetLastMovieName()
{
	if(GFullScreenMovie)
	{
		return GFullScreenMovie->GameThreadGetLastMovieName();
	}

	return "";
}

/**
 * Play one of the LoadMap loading movies as configured by ini file
 */
UBOOL UEngine::PlayLoadMapMovie()
{
	// don't try to load a movie if one is already going
	UBOOL bStartedLoadMapMovie=FALSE;
	if (GFullScreenMovie && !GFullScreenMovie->GameThreadIsMoviePlaying(TEXT("")))
	{
		// potentially load a movie here
		FConfigSection* MovieIni = GConfig->GetSectionPrivate(TEXT("FullScreenMovie"), FALSE, TRUE, GEngineIni);
		if (MovieIni)
		{
			TArray<FString> LoadMapMovies;
			// find all the loadmap movie possibilities
			for (FConfigSectionMap::TIterator It(*MovieIni); It; ++It)
			{
				if (It.Key() == TEXT("LoadMapMovies"))
				{
					LoadMapMovies.AddItem(It.Value());
				}
			}

			// load a random mobvie from the list, if there were any
			if (LoadMapMovies.Num() != 0)
			{
				PlayLoadingMovie( *LoadMapMovies(appRand() % LoadMapMovies.Num()) );

				// keep track of starting playback for loadmap so it can be stopped
				bStartedLoadMapMovie = TRUE;
			}
		}
	}

	// return if we started or not
	return bStartedLoadMapMovie;
}

/**
 * Stops the current movie
 *
 * @param bDelayStopUntilGameHasRendered If TRUE, the engine will delay stopping the movie until after the game has rendered at least one frame
 */
void UEngine::StopMovie(UBOOL bDelayStopUntilGameHasRendered)
{
	// delay if desired
	if (bDelayStopUntilGameHasRendered)
	{
		GFullScreenMovie->GameThreadRequestDelayedStopMovie();
	}
	else
	{
		GFullScreenMovie->GameThreadStopMovie(0, FALSE, TRUE);
	}
}

/**
 * Removes all overlays from displaying
 */
void UEngine::RemoveAllOverlays()
{
	GFullScreenMovie->GameThreadRemoveAllOverlays();
}

/**
 * Adds a text overlay to the movie
 *
 * @param Font Font to use to display (must be in the root set so this will work during loads)
 * @param Text Text to display
 * @param X X location in resolution-independent coordinates (ignored if centered)
 * @param Y Y location in resolution-independent coordinates
 * @param ScaleX Text horizontal scale
 * @param ScaleY Text vertical scale
 * @param bIsCentered TRUE if the text should be centered
 */
void UEngine::AddOverlay( UFont* Font, const FString& Text, FLOAT X, FLOAT Y, FLOAT ScaleX, FLOAT ScaleY, UBOOL bIsCentered )
{
	GFullScreenMovie->GameThreadAddOverlay( Font, Text, X, Y, ScaleX, ScaleY, bIsCentered, FALSE, 0 );
}


/**
 * Adds a wrapped text overlay to the movie
 *
 * @param Font Font to use to display (must be in the root set so this will work during loads)
 * @param Text Text to display
 * @param X X location in resolution-independent coordinates (ignored if centered)
 * @param Y Y location in resolution-independent coordinates
 * @param ScaleX Text horizontal scale
 * @param ScaleY Text vertical scale
 * @param WrapWidth Width before text is wrapped to the next line
 */
void UEngine::AddOverlayWrapped( UFont* Font, const FString& Text, FLOAT X, FLOAT Y, FLOAT ScaleX, FLOAT ScaleY, FLOAT WrapWidth )
{
	GFullScreenMovie->GameThreadAddOverlay( Font, Text, X, Y, ScaleX, ScaleY, FALSE, TRUE, WrapWidth );
}

/**
 * returns GEngine
 */
UEngine* UEngine::GetEngine()
{
	return GEngine;
}

/*-----------------------------------------------------------------------------
	Input.
-----------------------------------------------------------------------------*/

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE

/**
 * Helper structure for sorting textures by relative cost.
 */
struct FSortedTexture 
{
 	INT		OrigSizeX;
 	INT		OrigSizeY;
	INT		CookedSizeX;
	INT		CookedSizeY;
	INT		CurSizeX;
	INT		CurSizeY;
 	INT		LODBias;
	INT		MaxSize;
 	INT		CurrentSize;
	FString Name;
 	INT		LODGroup;
 	UBOOL	bIsStreaming;
	INT		UsageCount;
	INT		bForcePVRTC4;
	INT		Format;

	/** Constructor, initializing every member variable with passed in values. */
	FSortedTexture(	INT InOrigSizeX, INT InOrigSizeY, INT InCookedSizeX, INT InCookedSizeY, INT InCurSizeX, INT InCurSizeY, INT InLODBias, INT InMaxSize, INT InCurrentSize, const FString& InName, INT InLODGroup, UBOOL bInIsStreaming, INT InUsageCount, UBOOL InbForcePVRTC4, INT InFormat )
	:	OrigSizeX( InOrigSizeX )
	,	OrigSizeY( InOrigSizeY )
	,	CookedSizeX( InCookedSizeX )
	,	CookedSizeY( InCookedSizeY )
	,	CurSizeX( InCurSizeX )
	,	CurSizeY( InCurSizeY )
 	,	LODBias( InLODBias )
	,	MaxSize( InMaxSize )
	,	CurrentSize( InCurrentSize )
	,	Name( InName )
 	,	LODGroup( InLODGroup )
 	,	bIsStreaming( bInIsStreaming )
	,	UsageCount( InUsageCount )
	,	bForcePVRTC4( InbForcePVRTC4 )
	,	Format( InFormat )
	{}
};
static UBOOL bAlphaSort = FALSE;
IMPLEMENT_COMPARE_CONSTREF( FSortedTexture, UnPlayer, { return bAlphaSort ? appStricmp(*A.Name,*B.Name) : B.MaxSize - A.MaxSize; } )

/** Helper struct for sorting anim sets by size */
struct FSortedSet
{
	FString Name;
	INT		Size;

	FSortedSet( const FString& InName, INT InSize )
	:	Name(InName)
	,	Size(InSize)
	{}
};
IMPLEMENT_COMPARE_CONSTREF( FSortedSet, UnPlayer, { return bAlphaSort ? appStricmp(*A.Name,*B.Name) : ( A.Size > B.Size ) ? -1 : 1; } );

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
struct FSortedParticleSet
{
	FString Name;
	INT		Size;
	INT		PSysSize;
	INT		ModuleSize;
	INT		ComponentSize;
	INT		ComponentCount;
	INT		ComponentResourceSize;
	INT		ComponentTrueResourceSize;

	FSortedParticleSet( const FString& InName, INT InSize, INT InPSysSize, INT InModuleSize, 
		INT InComponentSize, INT InComponentCount, INT InComponentResourceSize, INT InComponentTrueResourceSize) :
		  Name(InName)
		, Size(InSize)
		, PSysSize(InPSysSize)
		, ModuleSize(InModuleSize)
		, ComponentSize(InComponentSize)
		, ComponentCount(InComponentCount)
		, ComponentResourceSize(InComponentResourceSize)
		, ComponentTrueResourceSize(InComponentTrueResourceSize)
	{}
};

IMPLEMENT_COMPARE_CONSTREF( FSortedParticleSet, UnPlayer, { return bAlphaSort ? appStricmp(*A.Name,*B.Name) : ( A.Size > B.Size ) ? -1 : 1; } );
#endif

/** Helper compare function for the SHOWHOTKISMET exec */
IMPLEMENT_COMPARE_POINTER(USequenceOp, UnPlayer, { return(B->ActivateCount - A->ActivateCount); } );

/** Sort actors by name. */
IMPLEMENT_COMPARE_POINTER( AActor, UnPlayer, { return( appStricmp( *A->GetName(), *B->GetName() ) ); } );

static void ShowSubobjectGraph( FOutputDevice& Ar, UObject* CurrentObject, const FString& IndentString )
{
	if ( CurrentObject == NULL )
	{
		Ar.Logf(TEXT("%sX NULL"), *IndentString);
	}
	else
	{
		TArray<UObject*> ReferencedObjs;
		FArchiveObjectReferenceCollector RefCollector(&ReferencedObjs, CurrentObject, TRUE, FALSE, FALSE, FALSE);
		CurrentObject->Serialize(RefCollector);

		if ( ReferencedObjs.Num() == 0 )
		{
			Ar.Logf(TEXT("%s. %s"), *IndentString, IndentString.Len() == 0 ? *CurrentObject->GetPathName() : *CurrentObject->GetName());
		}
		else
		{
			Ar.Logf(TEXT("%s+ %s"), *IndentString, IndentString.Len() == 0 ? *CurrentObject->GetPathName() : *CurrentObject->GetName());
			for ( INT ObjIndex = 0; ObjIndex < ReferencedObjs.Num(); ObjIndex++ )
			{
				ShowSubobjectGraph(Ar, ReferencedObjs(ObjIndex), IndentString + TEXT("|\t"));
			}
		}
	}
}

/** Holds information about memory usage. */
struct FMemItem
{
	INT		Count;
	SIZE_T	Num, Max, Res;
	UObject* Object;

	FMemItem()
		: Count(0)
		, Num(0)
		, Max(0)
		, Res(0)
		, Object( NULL )
	{}

	FMemItem( UObject* InObject, SIZE_T	InRes )
		: Count(0)
		, Num(0)
		, Max(0)
		, Res(InRes)
		, Object( InObject )
	{}

	void Add( FArchiveCountMem& Ar, SIZE_T InRes )
	{
		Count ++; 
		Num += Ar.GetNum(); 
		Max += Ar.GetMax(); 
		Res += InRes;
	}

	void AddRes( SIZE_T InRes )
	{
		Count ++; 
		Res += InRes;
	}
};

IMPLEMENT_COMPARE_CONSTREF( FMemItem, BaseEngine, { return B.Res - A.Res; } );

#endif // !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE

//
// This always going to be the last exec handler in the chain. It
// handles passing the command to all other global handlers.
//
UBOOL UEngine::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{	
	// See if any other subsystems claim the command.
	if( GSys				&&	GSys->Exec			(Cmd,Ar) ) return TRUE;
	if(	UObject::StaticExec							(Cmd,Ar) ) return TRUE;
	if( Client				&&	Client->Exec		(Cmd,Ar) ) return TRUE;
	if( GDebugToolExec		&&	GDebugToolExec->Exec(Cmd,Ar) ) return TRUE;
	if( GStatChart			&&	GStatChart->Exec	(Cmd,Ar) ) return TRUE;
	if( GMalloc				&&	GMalloc->Exec		(Cmd,Ar) ) return TRUE;
	if(	GObjectPropagator->Exec						(Cmd,Ar) ) return TRUE;
	if( GSystemSettings.Exec						(Cmd,Ar) ) return TRUE;
#if USING_REMOTECONTROL
	if( RemoteControlExec	&&	RemoteControlExec->Exec	(Cmd,Ar) ) return TRUE;
#endif

#if WITH_OPEN_AUTOMATE
	if( GOpenAutomate && GOpenAutomate->Exec( Cmd, Ar ) ) { return TRUE; }
#endif

#if DWTRIOVIZSDK
	// process Trioviz exec commands
	if (DwTriovizImpl_Exec							(Cmd, Ar)) return TRUE;
#endif

	// Handle engine command line.
	if( ParseCommand(&Cmd,TEXT("SHOWLOG")) )
	{
		// Toggle display of console log window.
		if( GLogConsole )
		{
			GLogConsole->Show( !GLogConsole->IsShown() );
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("STARTMOVIECAPTURE")) )
	{
		FAVIWriter* AVIWriter = FAVIWriter::GetInstance();
		if (AVIWriter)
		{
			AVIWriter->StartCapture();
			return TRUE;
		}
		return FALSE;

	}
	else if( ParseCommand(&Cmd,TEXT("STOPMOVIECAPTURE")) )
	{
		FAVIWriter* AVIWriter = FAVIWriter::GetInstance();
		if (AVIWriter)
		{
			AVIWriter->StopCapture();
			return TRUE;
		}
		return FALSE;
	}
#if WITH_ES2_RHI
	else if( ParseCommand(&Cmd,TEXT("PRINTMISSINGSHADERKEYS")) )
	{
		GShaderManager.PrintMissingShaderKeys();
	}
#endif
	else if ( ParseCommand(&Cmd,TEXT("PerfMem_Memory")) )
	{
		//PerfMem MemoryDatum( FVector(), FRotator() );
		//MemoryDatum.AddMemoryStatsForLocationRotation();
		//warnf( TEXT("%s"), *MemoryDatum.GetAlterTableColumnsSQL_StatSceneRendering() );
		//warnf( TEXT("%s"), *MemoryDatum.GetStoredProcedureText() );

		//PerfMem PerfDatum( FVector(), FRotator() );
		//PerfDatum.WriteInsertSQLToBatFile();
		//PerfDatum.AddPerfStatsForLocationRotation();


		return TRUE;
	}
	else if( ParseCommand(&Cmd, TEXT("GAMEVER")) ||  ParseCommand(&Cmd, TEXT("GAMEVERSION")))
	{
		Ar.Logf( TEXT("GEngineVersion:  %d  GBuiltFromChangeList:  %d"), GEngineVersion, GBuiltFromChangeList );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("CRACKURL")) )
	{
		FURL URL(NULL,Cmd,TRAVEL_Absolute);
		if( URL.Valid )
		{
			Ar.Logf( TEXT("     Protocol: %s"), *URL.Protocol );
			Ar.Logf( TEXT("         Host: %s"), *URL.Host );
			Ar.Logf( TEXT("         Port: %i"), URL.Port );
			Ar.Logf( TEXT("          Map: %s"), *URL.Map );
			Ar.Logf( TEXT("   NumOptions: %i"), URL.Op.Num() );
			for( INT i=0; i<URL.Op.Num(); i++ )
				Ar.Logf( TEXT("     Option %i: %s"), i, *URL.Op(i) );
			Ar.Logf( TEXT("       Portal: %s"), *URL.Portal );
			Ar.Logf( TEXT("       String: '%s'"), *URL.String() );
		}
		else Ar.Logf( TEXT("BAD URL") );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("DEFER")) )
	{
		new(DeferredCommands)FString(Cmd);
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("ToggleRenderingThread")) )
	{
		if(GIsThreadedRendering)
		{
			StopRenderingThread();
			GUseThreadedRendering = FALSE;
		}
		else
		{
			GUseThreadedRendering = TRUE;
			StartRenderingThread();
		}
		Ar.Logf( TEXT("RenderThread is now in %s threaded mode."), GUseThreadedRendering ? TEXT("multi") : TEXT("single"));
		return TRUE;
	}
#if WITH_SUBSTANCE_AIR
	else if( ParseCommand(&Cmd,TEXT("setInput")) )
	{
		TArray<FString> Tokens, Switches;
		UCommandlet::ParseCommandLine(Cmd, Tokens, Switches);

		if (Tokens.Num() == 3)
		{
			// default package to look for
			FString InstanceName = Tokens(0);

			// get the package 
			USubstanceAirGraphInstance* GraphInstanceObject =
				FindObject<USubstanceAirGraphInstance>(ANY_PACKAGE,*InstanceName);

			if (NULL != GraphInstanceObject && NULL != GraphInstanceObject->Instance)
			{
				SubstanceAir::FGraphInstance* GraphInstance = GraphInstanceObject->Instance;

				TArray<FString> ValueArray;
				Tokens(2).ParseIntoArray(&ValueArray,TEXT(","),TRUE);

				TArray<FLOAT> Value;
				for (int_t I=0 ; I<ValueArray.Num() ; ++I)
				{
					Value.Push(appAtof(*ValueArray(I)));
				}

				GraphInstance->UpdateInput(*Tokens(1), Value);
				SubstanceAir::Helpers::RenderAsync(GraphInstance);
			}
		}
	}
#endif // WITH_SUBSTANCE_AIR
	else if( ParseCommand(&Cmd,TEXT("TOGGLEOCCLUSION")) )
	{
		extern UBOOL GIgnoreAllOcclusionQueries;
		FlushRenderingCommands();
		GIgnoreAllOcclusionQueries = !GIgnoreAllOcclusionQueries;
		Ar.Logf(TEXT("Occlusion queries are now %s"),GIgnoreAllOcclusionQueries ? TEXT("disabled") : TEXT("enabled")); 
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("LANDSCAPE")) )
	{
		extern UBOOL GUseLandscapeBatching;
		extern UBOOL GUseLandscapeStatic;
		extern INT GForceLandscapeLOD;
		FlushRenderingCommands();
		if ( ParseCommand(&Cmd, TEXT("BATCHING")) )
		{
			GUseLandscapeBatching = !GUseLandscapeBatching;
			Ar.Logf(TEXT("Landscape Batchings are now %s"),GUseLandscapeBatching ? TEXT("disabled") : TEXT("enabled")); 
		}
		else if ( ParseCommand(&Cmd, TEXT("STATIC")) )
		{
			GUseLandscapeStatic = !GUseLandscapeStatic;
			Ar.Logf(TEXT("Landscape Static Drawlist rendering are now %s"),GUseLandscapeStatic ? TEXT("disabled") : TEXT("enabled")); 
		}
		else if ( ParseCommand(&Cmd, TEXT("LOD")) )
		{
			FString LodStr(ParseToken(Cmd, 0));
			if (LodStr.Len() > 0)
			{
				GForceLandscapeLOD = appAtoi(*LodStr);
				Ar.Logf(TEXT("Landscape LOD is forced to %d"),GForceLandscapeLOD); 
			}
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("ToggleHiPriThreadPool")) )
	{
		GHiPriThreadPoolForceOff = !GHiPriThreadPoolForceOff;
		Ar.Logf(TEXT("HiPri Pool %s"),GHiPriThreadPoolForceOff ? TEXT("disabled") : TEXT("enabled")); 
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("TOGGLEPRECOMPUTEDVISIBILITY")) )
	{
		extern UBOOL GAllowPrecomputedVisibility;
		FlushRenderingCommands();
		GAllowPrecomputedVisibility = !GAllowPrecomputedVisibility;
		Ar.Logf(TEXT("Precomputed visibility is now %s"), GAllowPrecomputedVisibility ? TEXT("enabled") : TEXT("disabled")); 
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWPRECOMPUTEDVISIBILITY")) )
	{
		extern UBOOL GShowPrecomputedVisibilityCells;
		extern UBOOL GShowRelevantPrecomputedVisibilityCells;
		FlushRenderingCommands();
		FString FlagStr(ParseToken(Cmd, 0));
		if (FlagStr.InStr(TEXT("Relevant"), FALSE, TRUE) != INDEX_NONE)
		{
			GShowRelevantPrecomputedVisibilityCells = !GShowRelevantPrecomputedVisibilityCells;
			Ar.Logf(TEXT("Relevant precomputed visibility cells are now %s"), GShowRelevantPrecomputedVisibilityCells ? TEXT("visible") : TEXT("hidden")); 
		}
		else
		{
			GShowPrecomputedVisibilityCells = !GShowPrecomputedVisibilityCells;
			Ar.Logf(TEXT("Precomputed visibility cells are now %s"), GShowPrecomputedVisibilityCells ? TEXT("visible") : TEXT("hidden")); 
		}
		
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("TRACKPARTICLERENDERINGSTATS")) )
	{
		extern FLOAT GTimeBetweenParticleRenderStatCaptures;
		extern FLOAT GMinParticleDrawTimeToTrack;
		FString FlagStr(ParseToken(Cmd, 0));
		if (FlagStr.Len() > 0)
		{
			GTimeBetweenParticleRenderStatCaptures = appAtof(*FlagStr);
		}

		FString FlagStr2(ParseToken(Cmd, 0));
		if (FlagStr2.Len() > 0)
		{
			GMinParticleDrawTimeToTrack = appAtof(*FlagStr2);
		}

		extern UBOOL GTrackParticleRenderingStats;
		GTrackParticleRenderingStats = !GTrackParticleRenderingStats;
		if (GTrackParticleRenderingStats)
		{
			if (GSystemSettings.DetailMode == DM_High)
			{
				Ar.Logf(TEXT("Currently in high detail mode, note that particle stats will only be captured in medium or low detail modes (eg splitscreen)."));
			}
			Ar.Logf(TEXT("Enabled particle render stat tracking with %.1fs between captures, min tracked time of %.4fs, use DUMPPARTICLERENDERINGSTATS to save results."),
				GTimeBetweenParticleRenderStatCaptures, GMinParticleDrawTimeToTrack);
		}
		else
		{
			Ar.Logf(TEXT("Disabled particle render stat tracking."));
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("DUMPPARTICLERENDERINGSTATS")) )
	{
		extern void DumpParticleRenderingStats(FOutputDevice& Ar);
		DumpParticleRenderingStats(Ar);
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("DUMPPARTICLEFRAMERENDERINGSTATS")) )
	{
		extern UBOOL GWantsParticleStatsNextFrame;
		GWantsParticleStatsNextFrame = TRUE;
		warnf(TEXT("DUMPPARTICLEFRAMERENDERINGSTATS triggered"));
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("RecompileShaders")) )
	{
		extern UBOOL RecompileShaders(const TCHAR* Cmd, FOutputDevice& Ar);
		return RecompileShaders(Cmd, Ar);
	}
	else if( ParseCommand(&Cmd,TEXT("RecompileGlobalShaders")) )
	{
		RecompileGlobalShaders();
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SAVESHADERS")) )
	{
		SaveLocalShaderCaches();
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("DUMPSHADERSTATS")) )
	{
		FString FlagStr(ParseToken(Cmd, 0));
		EShaderPlatform Platform = GRHIShaderPlatform;
		if (FlagStr.Len() > 0)
		{
			Platform = ShaderPlatformFromText(*FlagStr);
		}
		Ar.Logf(TEXT("Dumping shader stats for platform %s"), ShaderPlatformToText(Platform));
		// Dump info on all loaded shaders regardless of platform and frequency.
		DumpShaderStats( Platform, SF_NumFrequencies );
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("DUMPMATERIALSTATS")) )
	{
		FString FlagStr(ParseToken(Cmd, 0));
		EShaderPlatform Platform = GRHIShaderPlatform;
		if (FlagStr.Len() > 0)
		{
			Platform = ShaderPlatformFromText(*FlagStr);
		}
		Ar.Logf(TEXT("Dumping shader stats for platform %s"), ShaderPlatformToText(Platform));
		// Dump info on all loaded shaders regardless of platform and frequency.
		DumpMaterialStats( Platform );
		return TRUE;
	}
#if DO_CHARTING
	else if( ParseCommand(&Cmd,TEXT("DISTFACTORSTATS")) )
	{
		DumpDistanceFactorChart();
		return TRUE;
	}
#endif // DO_CHARTING
	else if( ParseCommand(&Cmd,TEXT("LOWRESTRANSLUCENCY")) )
	{
		extern FLOAT GDownsampledTransluencyDistanceThreshold;
		FlushRenderingCommands();
		FString FlagStr(ParseToken(Cmd, 0));
		if (FlagStr.Len() > 0)
		{
			GSystemSettings.bAllowDownsampledTranslucency = TRUE;
			GDownsampledTransluencyDistanceThreshold = appAtof(*FlagStr);
		}
		else
		{
			GSystemSettings.bAllowDownsampledTranslucency = !GSystemSettings.bAllowDownsampledTranslucency;
		}
		Ar.Logf(TEXT("Downsampling %u, Distance threshold %.1f"), GSystemSettings.bAllowDownsampledTranslucency, GDownsampledTransluencyDistanceThreshold);
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("TOGGLESHADOWDEPTH")) )
	{
		extern UBOOL GSkipShadowDepthRendering;
		FlushRenderingCommands();
		GSkipShadowDepthRendering = !GSkipShadowDepthRendering;
		Ar.Logf(TEXT("GSkipShadowDepthRendering %u"), GSkipShadowDepthRendering);
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("TOGGLEDEFERRED")) )
	{
		// Have to repopulate static draw lists to propagate the change
		FGlobalComponentReattachContext Context;
		extern UBOOL GAllowDeferredShading;
		FlushRenderingCommands();
		GAllowDeferredShading = !GAllowDeferredShading;
		Ar.Logf(TEXT("GAllowDeferredShading %u"), GAllowDeferredShading);
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("PROFILEGPU")) )
	{
		if (!GProfilingGPUHitches)
		{
			GProfilingGPU = TRUE;
			Ar.Logf(TEXT("Profiling the next GPU frame"));
		}
		else
		{
			Ar.Logf(TEXT("Can't do a gpu profile during a hitch profile!"));
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("PROFILEGPUHITCHES")) )
	{
		GProfilingGPUHitches = !GProfilingGPUHitches;
		if (GProfilingGPUHitches)
		{
			Ar.Logf(TEXT("Profiling GPU hitches."));
		}
		else
		{
			Ar.Logf(TEXT("Stopped profiling GPU hitches."));
		}
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("REBUILDMESH")))
	{
		TArray<FString> Tokens, Switches;
		appParseCommandLine(Cmd, Tokens, Switches);
		if (Tokens.Num() > 0)
		{
			UBOOL bFullRebuild = FALSE;
			UBOOL bRebuildAll = FALSE;
			INT StartToken = 0;
			if (Tokens(0) == TEXT("FULL"))
			{
				bFullRebuild = TRUE;
				StartToken = 1;
			}
			else if (Tokens(0) == TEXT("ALLLOADED"))
			{
				warnf(TEXT("Rebuilding all loaded static meshes"));
				bRebuildAll = TRUE;
			}

			if (bRebuildAll)
			{
				for (TObjectIterator<UStaticMesh> It; It; ++It)
				{
					UStaticMesh* CurrentMesh = *It;
					warnf(TEXT("Starting %u tris %u verts %s"), CurrentMesh->LODModels(0).IndexBuffer.Indices.Num() / 3, CurrentMesh->LODModels(0).NumVertices, *CurrentMesh->GetPathName());
					CurrentMesh->Build(FALSE, TRUE);
				}
			}
			else
			{
				// These are the meshes to rebuild...
				for (INT MeshIdx = StartToken; MeshIdx < Tokens.Num(); MeshIdx++)
				{
					debugf(TEXT("%sRebuild mesh on %s"), bFullRebuild ? TEXT("FULL ") : TEXT(""), *(Tokens(MeshIdx)));
					UStaticMesh* TheMesh = FindObject<UStaticMesh>(NULL, *(Tokens(MeshIdx)));
					if (TheMesh != NULL)
					{
						TArray<UMaterialInterface*> PreviousMaterials;
						if (bFullRebuild == TRUE)
						{
							for (INT LODIdx = TheMesh->LODModels.Num() - 1; LODIdx >= 0; LODIdx--)
							{
								TArray<INT> ElementIdxToRemove;
								UBOOL bHasRemovals = FALSE;
								// See if there are empty elements
								FStaticMeshRenderData& LODModel = TheMesh->LODModels(LODIdx);
								FStaticMeshLODInfo& LODInfo = TheMesh->LODInfo(LODIdx);

								if (LODModel.Elements.Num() < LODInfo.Elements.Num())
								{
									LODInfo.Elements.Remove(LODModel.Elements.Num() - 1, LODInfo.Elements.Num() - LODModel.Elements.Num());
								}
								if (LODModel.Elements.Num() > LODInfo.Elements.Num())
								{
									LODInfo.Elements.AddZeroed(LODModel.Elements.Num() - LODInfo.Elements.Num());
								}
								check(LODModel.Elements.Num() == LODInfo.Elements.Num());

								for (INT ElementIdx = 0; ElementIdx < LODModel.Elements.Num(); ElementIdx++)
								{
									PreviousMaterials.AddItem(LODModel.Elements(ElementIdx).Material);
								}

								LODModel.Elements.Empty();
								LODInfo.Elements.Empty();
							}
						}

						// Rebuild!
						TheMesh->Build();

						if (bFullRebuild == TRUE)
						{
							// Remove 0 triangle elements and try to restore the materials...
							if (UStaticMesh::RemoveZeroTriangleElements(TheMesh, TRUE) == TRUE)
							{
								for (INT LODIdx = TheMesh->LODModels.Num() - 1; LODIdx >= 0; LODIdx--)
								{
									// See if there are empty elements
									FStaticMeshRenderData& LODModel = TheMesh->LODModels(LODIdx);
									FStaticMeshLODInfo& LODInfo = TheMesh->LODInfo(LODIdx);

									for (INT ElementIdx = 0; ElementIdx < LODModel.Elements.Num(); ElementIdx++)
									{
										if (ElementIdx < PreviousMaterials.Num())
										{
											LODModel.Elements(ElementIdx).Material = PreviousMaterials(ElementIdx);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	else if( ParseCommand(&Cmd,TEXT("SHADERCOMPILINGSTATS")) )
	{
		GShaderCompilingThreadManager->DumpStats();
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("DUMPCOMPRESSEDSHADERSTATS")) )
	{
		// Make sure no updates to GCompressedShaderCaches are in flight
		FlushRenderingCommands();
		Ar.Logf(TEXT("Loaded compressed shader caches"));
		for (INT CacheIndex = 0; CacheIndex < GCompressedShaderCaches[GRHIShaderPlatform].Num(); CacheIndex++)
		{
			const FCompressedShaderCodeCache* CompressedCache = GCompressedShaderCaches[GRHIShaderPlatform](CacheIndex);
			
			Ar.Logf(TEXT("	'%s' (%i refs, %i bytes)"), *CompressedCache->CacheName, CompressedCache->GetRefCount(), CompressedCache->GetCompressedCodeSize());
#if !FINAL_RELEASE
			for (TObjectIterator<UMaterial> It; It; ++It)
			{
				UMaterial* CurrentMaterial = *It;
				FMaterialShaderMap* ShaderMap = CurrentMaterial->GetMaterialResource()->GetShaderMap();
				if (ShaderMap && ShaderMap->IdenticalToCompressedCache(CompressedCache))
				{
					Ar.Logf(TEXT("			MATERIAL '%s'"), *CurrentMaterial->GetFullName());
					UObject* OuterWalk = CurrentMaterial->GetOuter();
					while (OuterWalk)
					{
						Ar.Logf(TEXT("				PARENT'%s'"), *OuterWalk->GetFullName());
						OuterWalk = OuterWalk->GetOuter();
					}
				}
			}

			for (TObjectIterator<UMaterialInstance> It; It; ++It)
			{
				UMaterialInstance* CurrentMaterialInstance = *It;
				FMaterialResource* MaterialResource = CurrentMaterialInstance->GetMaterialResource();
				if (MaterialResource)
				{
					FMaterialShaderMap* ShaderMap = MaterialResource->GetShaderMap();
					if (ShaderMap && ShaderMap->IdenticalToCompressedCache(CompressedCache))
					{
						Ar.Logf(TEXT("			MATERIAL INSTANCE - '%s'"), *CurrentMaterialInstance->GetFullName());
					}
				}
			}
#endif // !FINAL_RELEASE
		}

		// A few extra stats for shader caches
#if STATS_FAST
		STAT_MAKE_AVAILABLE_FAST(STAT_ShaderCompression_CompressedShaderMemory);
		STAT_MAKE_AVAILABLE_FAST(STAT_ShaderCompression_UncompressedShaderMemory);

		DWORD CompressedUsage = GET_DWORD_STAT_FAST(STAT_ShaderCompression_CompressedShaderMemory);
		DWORD UncompressedUsage = GET_DWORD_STAT_FAST(STAT_ShaderCompression_UncompressedShaderMemory);
		Ar.Logf(TEXT("CompressedShaderMemUsage = %0.2f KB"), CompressedUsage / 1024.0f);
		Ar.Logf(TEXT("UncompressedShaderMemUsage = %0.2f KB"), UncompressedUsage / 1024.0f);
#endif

		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("DUMPPERSLEVELACTORS")) )
	{
		for (INT i = 0; i < GWorld->PersistentLevel->Actors.Num(); ++i)
		{
			AActor* TempActor = GWorld->PersistentLevel->Actors(i);
			if(TempActor)
			{
				Ar.Logf(TEXT("			PERS LEVEL ACTOR- '%s' %s %d"), *TempActor->GetFullName(), *TempActor->GetClass()->GetName(), TempActor->GetNetIndex());
			}
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("LIGHTENV")) )
	{
		extern UBOOL ExecLightEnvironment(const TCHAR* Cmd, FOutputDevice& Ar);
		return ExecLightEnvironment(Cmd, Ar);
	}
	else if( ParseCommand(&Cmd,TEXT("rebuilddistro")) )
	{
		for (TObjectIterator<UDistributionFloat> It; It; ++It)
		{
			It->bIsDirty = TRUE;
		}
		for (TObjectIterator<UDistributionVector> It; It; ++It)
		{
			It->bIsDirty = TRUE;
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("DUMPDYNAMICLIGHTSHADOWINTERACTIONS")) )
	{
		GWorld->Scene->DumpDynamicLightShadowInteractions( TRUE );
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("DUMPDEVICETOKENS")) )
	{
		FString DeviceUUID = GetDeviceUUID();
		if( DeviceUUID.Len() == 0 )
		{
			DeviceUUID = TEXT("N/A or unable to determine");
		}
		FString DevicePushNotificationToken = GetDevicePushNotificationToken();
		if( DevicePushNotificationToken.Len() == 0 )
		{
			DevicePushNotificationToken = TEXT("N/A or unable to determine");
		}
		
		Ar.Logf(TEXT("Device UUID:"));
		Ar.Logf(TEXT("       %s"), *DeviceUUID);
		Ar.Logf(TEXT("Device Push Notification Token:"));
		Ar.Logf(TEXT("       %s"), *DevicePushNotificationToken);
	}
#if !FINAL_RELEASE
	else if( ParseCommand(&Cmd,TEXT("QUERYPERFDB")) )
	{
		// Check whether we have an active DB connection for perf tracking and try to use it if we do.
		FDataBaseConnection* DataBaseConnection = GTaskPerfTracker->GetConnection();
		if( DataBaseConnection )
		{
			// Query string, calling stored procedure on DB that lists aggregrated duration of tasks on this machine
			// grouped by task, sorted by duration.
			FString QueryString = FString(TEXT("EXEC MachineSummary @MachineName=")) + appComputerName();
		
			// Execute the Query. If successful it will fill in the RecordSet and we're responsible for 
			// deleting it.
			FDataBaseRecordSet* RecordSet = NULL;
			if( DataBaseConnection->Execute( *QueryString, RecordSet ) )
			{
				// Success! Iterate over all rows in record set and log them.
				debugf(TEXT("Aggregate duration of tasks tracked on this machine."));
				for( FDataBaseRecordSet::TIterator It( RecordSet ); It; ++It )
				{
					FLOAT	DurationSum = It->GetFloat( TEXT("DurationSum") );
					FString Task		= It->GetString( TEXT("Task") );
					debugf(TEXT("%10.2f min for '%s'"), DurationSum / 60, *Task);
			}

				// Clean up record set now that we are done.
				delete RecordSet;
				RecordSet = NULL;
			}
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("TestRemoteDBProxy")) )
	{
		// Check whether we have an active DB connection for perf tracking and try to use it if we do.
		FDataBaseConnection* DataBaseConnection = GTaskPerfMemDatabase->GetConnection();
		if( DataBaseConnection != NULL )
		{
			DataBaseConnection->Open( TEXT("172.23.2.8"), NULL, NULL );
			// Query string, calling stored procedure on DB that lists aggregrated duration of tasks on this machine
			// grouped by task, sorted by duration.
			const FString QueryString = FString (TEXT("EXEC dbo.[AddMapLocationData_Perf] @Platform='Xenon', @LevelName='msewTest2', @BuiltFromChangelist=218869, @RunResult='Passed', @LocX=-26953.707031, @LocY=142715.671875, @LocZ=-10642.504883, @RotYaw=49152, @RotPitch=0, @RotRoll=0, @AverageFPS=75.12, @AverageMS=13.31, @FrameTime=13.29, @Game_thread_time=12.03, @Render_thread_time=1.98, @GPU_time=12.92, @Culled_primitives=0.00, @Occluded_primitives=0.00, @Occlusion_queries=0.00, @Projected_shadows=0.00, @Visible_static_mesh_elements=0.00, @Visible_dynamic_primitives=2.00, @Draw_events=0.00, @Streaming_Textures_Size=19564.00, @Textures_Max_Size=63892.00, @Intermediate_Textures_Size=0.00, @Request_Size_Current_Frame=0.00, @Request_Size_Total=211420.00, @Streaming_fudge_factor=667.00" ) );
			
			DataBaseConnection->Execute( *QueryString );

			warnf( TEXT( "TestRemoteDBProxy EXECUTED!!" ) );
		}

		return TRUE;
	}
#endif // !FINAL_RELEASE
#if DO_CHARTING
	else if( ParseCommand(&Cmd,TEXT("RESETFPSCHART")) )
	{
		GSentinelRunID = -1;  // we can do this here as the dumpfpschart / resetfpschart is the process QA uses to get new sets of data
		ResetFPSChart();
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("DUMPFPSCHART")) )
	{
		DumpFPSChart(TRUE);
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("RESETMEMCHART")) )
	{
		ResetMemoryChart();
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("DUMPMEMCHART")) )
	{
		DumpMemoryChart(TRUE);
		return TRUE;
	}
#endif // DO_CHARTING
#if !FINAL_RELEASE
	else if( ParseCommand(&Cmd,TEXT("LISTDYNAMICLEVELS")) )
	{
		debugf(TEXT("Listing dynamic streaming levels for %s"),*GWorld->GetOutermost()->GetName());
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		for( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
		{
			ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
			if( StreamingLevel && !StreamingLevel->bIsFullyStatic ) 
			{
				debugf(TEXT("   %s"),*StreamingLevel->PackageName.ToString());
			}
		}
		return TRUE;
	}
#endif // !FINAL_RELEASE
	else if( ParseCommand(&Cmd,TEXT("TEXTUREDENSITY")) )
	{
		FString MinDensity(ParseToken(Cmd, 0));
		FString IdealDensity(ParseToken(Cmd, 0));
		FString MaxDensity(ParseToken(Cmd, 0));
		GEngine->MinTextureDensity   = ( MinDensity.Len()   > 0 ) ? appAtof(*MinDensity)   : GEngine->MinTextureDensity;
		GEngine->IdealTextureDensity = ( IdealDensity.Len() > 0 ) ? appAtof(*IdealDensity) : GEngine->IdealTextureDensity;
		GEngine->MaxTextureDensity   = ( MaxDensity.Len()   > 0 ) ? appAtof(*MaxDensity)   : GEngine->MaxTextureDensity;
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("SHADERCOMPLEXITY")) )
	{
		FString FlagStr(ParseToken(Cmd, 0));
		if( FlagStr.Len() > 0 )
		{
			if( appStricmp(*FlagStr,TEXT("MAX"))==0)
			{
				FLOAT NewMax = appAtof(Cmd);
				if (NewMax > 0.0f)
				{
					GEngine->MaxPixelShaderAdditiveComplexityCount = NewMax;
				}
			}
			else
			{
				Ar.Logf( TEXT("Format is 'shadercomplexity [toggleadditive] [togglepixel] [max $int]"));
				return TRUE;
			}

			FLOAT CurrentMax = GEngine->MaxPixelShaderAdditiveComplexityCount;

			Ar.Logf( TEXT("New ShaderComplexity Settings: Max = %f"), CurrentMax);
		} 
		else
		{
			Ar.Logf( TEXT("Format is 'shadercomplexity [max $int]"));
		}
		return TRUE; 
	}
#if !FINAL_RELEASE
	else if( ParseCommand(&Cmd, TEXT("LISTMISSINGPHYSICALMATERIALS")) )
	{
		// Gather all material (instances) without a physical material association.
		TArray<FString> MaterialNames;
		for(  TObjectIterator<UMaterialInterface> It; It; ++It )
		{
			UMaterialInterface* MaterialInterface = *It;
			if( MaterialInterface->GetPhysicalMaterial() == NULL )
			{
				new(MaterialNames)FString(MaterialInterface->GetFullName());
			}
		}
		if( MaterialNames.Num() )
		{
			// Sort the list lexigraphically.
			Sort<USE_COMPARE_CONSTREF(FString,UnEngine)>( MaterialNames.GetTypedData(), MaterialNames.Num() );
			// Log the names.
			debugf(TEXT("Materials with no associated physical material:"));
			for( INT i=0; i<MaterialNames.Num(); i++ )
			{
				debugf(TEXT("%s"),*MaterialNames(i));
			}
		}
		else
		{
			debugf(TEXT("All materials have physical materials associated."));
		}
		return TRUE;
	}
#endif // !FINAL_RELEASE
	else if (ParseCommand(&Cmd, TEXT("FULLMOTIONBLUR")))
	{
		extern INT GMotionBlurFullMotionBlur;
		FString Parameter(ParseToken(Cmd, 0));
		GMotionBlurFullMotionBlur = (Parameter.Len() > 0) ? Clamp(appAtoi(*Parameter),-1,1) : -1;
		warnf( TEXT("Motion Blur FullMotionBlur is now set to: %s"),
			(GMotionBlurFullMotionBlur < 0 ? TEXT("DEFAULT") : (GMotionBlurFullMotionBlur > 0 ? TEXT("TRUE") : TEXT("FALSE"))) );
		return TRUE;
	}
#if !FINAL_RELEASE
	else if (ParseCommand(&Cmd, TEXT("MotionBlurSkinning")))
	{
		FString Parameter(ParseToken(Cmd, 0));
		if(Parameter.Len())
		{
			INT NewValue = appAtoi(*Parameter);

			if(NewValue != GSystemSettings.MotionBlurSkinning)
			{
				FSystemSettings Settings = GSystemSettings;

				Settings.MotionBlurSkinning = NewValue;
				GSystemSettings.ApplyNewSettings(Settings, FALSE);

				// wait until resources are released
				FlushRenderingCommands();

				checkSlow(GSystemSettings.MotionBlurSkinning == NewValue);

				{
					TComponentReattachContext<USkeletalMeshComponent> SkeletalMeshReattach;
				}

				GEngine->IssueDecalUpdateRequest();
			}
		}
		else
		{
			Ar.Logf(TEXT("0: force off for all SkeletalMesh"));
			Ar.Logf(TEXT("1: use bool on the SkeletalMesh"));
			Ar.Logf(TEXT("2: force on for all SkeletalMeshes"));
		}

		Ar.Logf( TEXT("MotionBlurSkinning = %d"), GSystemSettings.MotionBlurSkinning );
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("DumpConsoleCommands")))
	{
		Ar.Logf(TEXT("DumpConsoleCommands: %s*"), Cmd);
		Ar.Logf(TEXT(""));
		ConsoleCommandLibrary_DumpLibrary(*GEngine, FString(Cmd) + TEXT("*"), Ar);
		return TRUE;
	}
#endif // !FINAL_RELEASE
	else if( ParseCommand(&Cmd,TEXT("FREEZERENDERING")) )
	{
		ProcessToggleFreezeCommand();
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("FREEZESTREAMING")) )
	{
		ProcessToggleFreezeStreamingCommand();
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("FREEZEALL")) )
	{
		ProcessToggleFreezeCommand();
		ProcessToggleFreezeStreamingCommand();
		return TRUE;
	}
#if !FINAL_RELEASE
	else if( ParseCommand(&Cmd,TEXT("SHOWMATERIALDRAWEVENTS")) )
	{
		GShowMaterialDrawEvents = !GShowMaterialDrawEvents;
		warnf( TEXT("Show material names in SCOPED_DRAW_EVENT: %s"), GShowMaterialDrawEvents ? TEXT("TRUE") : TEXT("FALSE") );
		return TRUE;
	}
#endif
#if PERF_TRACK_FILEIO_STATS
	else if( ParseCommand(&Cmd,TEXT("DUMPFILEIOSTATS")) )
	{
		GetFileIOStats()->DumpStats();
		return TRUE;
	}
#endif
	else if( ParseCommand(&Cmd, TEXT("FLUSHIOMANAGER")) )
	{
		GIOManager->Flush();
		return TRUE;
	}
	else if( ParseCommand(&Cmd, TEXT("MOVIETEST")) )
	{
		if( GFullScreenMovie )
		{
			FString TestMovieName( ParseToken( Cmd, 0 ) );
			if( TestMovieName.Len() == 0 )
			{
				TestMovieName = TEXT("AttractMode");
			}
			// Play movie and block on playback.
			GFullScreenMovie->GameThreadPlayMovie(MM_PlayOnceFromStream, *TestMovieName);
			GFullScreenMovie->GameThreadWaitForMovie();
			GFullScreenMovie->GameThreadStopMovie();
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd, TEXT("MOVIEHIDE")) )
	{
		GFullScreenMovie->GameThreadSetMovieHidden( TRUE );

		return TRUE;
	}
	else if( ParseCommand(&Cmd, TEXT("MOVIESHOW")) )
	{
		GFullScreenMovie->GameThreadSetMovieHidden( FALSE );

		return TRUE;
	}
#if !FINAL_RELEASE
	else if( ParseCommand(&Cmd, TEXT("AVAILABLETEXMEM")) )
	{
		FlushRenderingCommands();
		warnf(TEXT("Available texture memory as reported by the RHI: %dMB"), RHIGetAvailableTextureMemory());
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("DUMPAVAILABLERESOLUTIONS")))
	{
		debugf(TEXT("DumpAvailableResolutions"));
		
		FScreenResolutionArray ResArray;
		if (RHIGetAvailableResolutions(ResArray, FALSE))
		{
			for (INT ModeIndex = 0; ModeIndex < ResArray.Num(); ModeIndex++)
			{
				FScreenResolutionRHI& ScreenRes = ResArray(ModeIndex);
				Ar.Logf(TEXT("%dx%d"), ScreenRes.Width, ScreenRes.Height);
			}
		}
		else
		{
			Ar.Logf(TEXT("Failed to get available resolutions!"));
		}
		return TRUE;
	}
#endif // !FINAL_RELEASE
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	else if(ParseCommand(&Cmd,TEXT("LISTTEXTUREGROUPS")))
	{
		TMap<INT, INT> MemoryByGroupMap;
		for( TObjectIterator<UTexture2D> It; It; ++It )
		{
			// get texture current size
			INT CurrentSize = It->CalcTextureMemorySize(TMC_ResidentMips);

			// track the size
			INT TotalSize = MemoryByGroupMap.FindRef(It->LODGroup);
			MemoryByGroupMap.Set(It->LODGroup, TotalSize + CurrentSize);
		}

		// now print them out
		TArray<FString> GroupNames = FTextureLODSettings::GetTextureGroupNames();
		for (TMap<INT, INT>::TIterator It(MemoryByGroupMap); It; ++It)
		{
			const FString& GroupName = GroupNames(It.Key());
			INT TotalSize = It.Value();
			INT LODBias = GSystemSettings.TextureLODSettings.GetTextureLODGroup(It.Key()).LODBias;
			INT MaxLOD = 1 << GSystemSettings.TextureLODSettings.GetTextureLODGroup(It.Key()).MaxLODMipCount;

			Ar.Logf(TEXT("Group %s: LODBias=%d, Max Texture Size=%d, Total Memory Usage=%.3f MB"), *GroupName, LODBias, MaxLOD, (FLOAT)TotalSize / (1024.0f * 1024.0f));
		}

	}
	// START: These commands were previously located in LocalPlayer::Exec...
	else if(ParseCommand(&Cmd,TEXT("LISTTEXTURES")))
	{
		const UBOOL bShouldOnlyListStreaming = ParseCommand(&Cmd, TEXT("STREAMING"));
		const UBOOL bShouldOnlyListNonStreaming = ParseCommand(&Cmd, TEXT("NONSTREAMING"));
		bAlphaSort = ParseParam( Cmd, TEXT("ALPHASORT") );

		Ar.Logf( TEXT("Listing %s textures."), bShouldOnlyListNonStreaming ? TEXT("non streaming") : bShouldOnlyListStreaming ? TEXT("streaming") : TEXT("all")  );

		// Find out how many times a texture is referenced by primitive components.
		TMap<UTexture2D*,INT> TextureToUsageMap;
		for( TObjectIterator<UPrimitiveComponent> It; It; ++It )
		{
			UPrimitiveComponent* PrimitiveComponent = *It;

			// Use the existing texture streaming functionality to gather referenced textures. Worth noting
			// that GetStreamingTextureInfo doesn't check whether a texture is actually streamable or not
			// and is also implemented for skeletal meshes and such.
			TArray<FStreamingTexturePrimitiveInfo> StreamingTextures;
			PrimitiveComponent->GetStreamingTextureInfo( StreamingTextures );

			// Increase usage count for all referenced textures
			for( INT TextureIndex=0; TextureIndex<StreamingTextures.Num(); TextureIndex++ )
			{
				UTexture2D* Texture = Cast<UTexture2D>(StreamingTextures(TextureIndex).Texture);
				if( Texture )
				{
					// Initializes UsageCount to 0 if texture is not found.
					INT UsageCount = TextureToUsageMap.FindRef( Texture );
					TextureToUsageMap.Set(Texture, UsageCount+1);
				}
			}
		}

		// Collect textures.
		TArray<FSortedTexture> SortedTextures;
		for( TObjectIterator<UTexture2D> It; It; ++It )
		{
			UTexture2D*		Texture				= *It;
			INT				LODGroup			= Texture->LODGroup;
			INT				LODBias				= Texture->GetCachedLODBias();
			INT				NumMips				= Texture->Mips.Num();	
			INT				MaxMips				= Max( 1, Min( NumMips - Texture->GetCachedLODBias(), GMaxTextureMipCount ) );
			INT				OrigSizeX			= Texture->SizeX;
			INT				OrigSizeY			= Texture->SizeY;
			INT				CookedSizeX			= Texture->SizeX >> LODBias;
			INT				CookedSizeY			= Texture->SizeY >> LODBias;
			INT				DroppedMips			= Texture->Mips.Num() - Texture->ResidentMips;
			INT				CurSizeX			= Texture->SizeX >> DroppedMips;
			INT				CurSizeY			= Texture->SizeY >> DroppedMips;
			UBOOL			bIsStreamingTexture = GStreamingManager->IsManagedStreamingResource( Texture );
			INT				MaxSize				= Texture->CalcTextureMemorySize( TMC_AllMips );
			INT				CurrentSize			= Texture->CalcTextureMemorySize( TMC_ResidentMips );
			INT				UsageCount			= TextureToUsageMap.FindRef( Texture );

			if( (bShouldOnlyListStreaming && bIsStreamingTexture) 
			||	(bShouldOnlyListNonStreaming && !bIsStreamingTexture) 
			||	(!bShouldOnlyListStreaming && !bShouldOnlyListNonStreaming) )
			{
				new(SortedTextures) FSortedTexture( 
										OrigSizeX, 
										OrigSizeY, 
										CookedSizeX,
										CookedSizeY,
										CurSizeX,
										CurSizeY,
										LODBias, 
										MaxSize / 1024, 
										CurrentSize / 1024, 
										Texture->GetPathName(), 
										LODGroup, 
										bIsStreamingTexture,
										UsageCount,
										Texture->bForcePVRTC4,
										Texture->Format);
			}
		}

		// Sort textures by cost.
		Sort<USE_COMPARE_CONSTREF(FSortedTexture,UnPlayer)>(SortedTextures.GetTypedData(),SortedTextures.Num());

		// Retrieve mapping from LOD group enum value to text representation.
		TArray<FString> TextureGroupNames = FTextureLODSettings::GetTextureGroupNames();

		// Display.
		INT TotalMaxSize		= 0;
		INT TotalCurrentSize	= 0;
		Ar.Logf( TEXT(",Authored Width,Authored Height,Cooked Width,Cooked Height,Current Width,Current Height,Max Size,Current Size,LODBias,LODGroup,Name,Streaming,Usage Count,ForcePVRTC4,Format") );
		for( INT TextureIndex=0; TextureIndex<SortedTextures.Num(); TextureIndex++ )
 		{
 			const FSortedTexture& SortedTexture = SortedTextures(TextureIndex);
			Ar.Logf( TEXT(",%i,%i,%i,%i,%i,%i,%i,%i,%i,%s,%s,%s,%i,%i,%s"),
 				SortedTexture.OrigSizeX,
 				SortedTexture.OrigSizeY,
				SortedTexture.CookedSizeX,
				SortedTexture.CookedSizeY,
				SortedTexture.CurSizeX,
				SortedTexture.CurSizeY,
				SortedTexture.MaxSize,
 				SortedTexture.CurrentSize,
				SortedTexture.LODBias,
 				*TextureGroupNames(SortedTexture.LODGroup),
				*SortedTexture.Name,
				SortedTexture.bIsStreaming ? TEXT("YES") : TEXT("NO"),
				SortedTexture.UsageCount,
				SortedTexture.bForcePVRTC4,
				GPixelFormats[SortedTexture.Format].Name);
			
			TotalMaxSize		+= SortedTexture.MaxSize;
			TotalCurrentSize	+= SortedTexture.CurrentSize;
 		}

		Ar.Logf(TEXT("Total size: Current= %d  Max= %d  Count=%d"), TotalCurrentSize, TotalMaxSize, SortedTextures.Num() );
		return TRUE;
	}
#if WITH_UE3_NETWORKING && !FINAL_RELEASE
	else if(ParseCommand(&Cmd,TEXT("REMOTETEXTURESTATS")))
	{
		// Address which sent the command.  We will send stats back to this address
		FString Addr(ParseToken(Cmd, 0));
		// Port to send to
		FString Port(ParseToken(Cmd, 0));

		// Make an IP address.
		FIpAddr SrcAddr;
		SrcAddr.Port = appAtoi( *Port );
		SrcAddr.Addr = appAtoi( *Addr );

		FUdpLink ToServerConnection;

		// Gather stats.
		DOUBLE LastTime = GLastTime;

		debugf( TEXT("Remote AssetsStats request received.") );

		TMap<UTexture2D*,INT> TextureToUsageMap;

		TArray<UMaterialInterface*> UsedMaterials;
		TArray<UTexture*> UsedTextures;

		// Find out how many times a texture is referenced by primitive components.
		for( TObjectIterator<UPrimitiveComponent> It; It; ++It )
		{
			UPrimitiveComponent* PrimitiveComponent = *It;

			UsedMaterials.Reset();
			// Get the used materials off the primitive component so we can find the textures
			PrimitiveComponent->GetUsedMaterials( UsedMaterials );
			for( INT MatIndex = 0; MatIndex < UsedMaterials.Num(); ++MatIndex )
			{
				// Ensure we dont have any NULL elements.
				if( UsedMaterials( MatIndex ) )
				{
					UsedTextures.Reset();
					UsedMaterials( MatIndex )->GetUsedTextures( UsedTextures );

					// Increase usage count for all referenced textures
					for( INT TextureIndex=0; TextureIndex<UsedTextures.Num(); TextureIndex++ )
					{
						UTexture2D* Texture = Cast<UTexture2D>( UsedTextures(TextureIndex) );
						if( Texture )
						{
							// Initializes UsageCount to 0 if texture is not found.
							INT UsageCount = TextureToUsageMap.FindRef( Texture );
							TextureToUsageMap.Set(Texture, UsageCount+1);
						}
					}
				}
			}
		}

		for(TObjectIterator<UTexture> It; It; ++It)
		{
			UTexture* Texture = *It;
			FString FullyQualifiedPath = Texture->GetPathName();
			FString MaxDim = Texture->GetDetailedDescription(0); // Passing 0 to GetDetailedDescription returns the dimension
			DWORD GroupId = Texture->LODGroup;
			UINT FullyLoadedInBytes = Texture->CalcTextureMemorySize( TMC_AllMips );
			UINT CurrentInBytes = Texture->CalcTextureMemorySize( TMC_ResidentMips );
			FString TexType;	// e.g. "2D", "Cube", ""
			DWORD FormatId = 0;
			FLOAT LastTimeRendered = FLT_MAX;
			DWORD NumUses = 0;
			INT LODBias = Texture->GetCachedLODBias();
			FTexture* Resource = Texture->Resource; 

			if(Resource)
			{
				LastTimeRendered = LastTime - Resource->LastRenderTime;
			}

			FString CurrentDim = TEXT("?");
			UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
			if(Texture2D)
			{
				FormatId = Texture2D->Format;
				TexType = TEXT("2D");
				NumUses = TextureToUsageMap.FindRef( Texture2D );

				// Calculate in game current dimensions 
				const INT DroppedMips = Texture2D->Mips.Num() - Texture2D->ResidentMips;
				CurrentDim = FString::Printf(TEXT("%dx%d"), Texture2D->SizeX >> DroppedMips, Texture2D->SizeY >> DroppedMips);
			}
			else
			{
				UTextureCube* TextureCube = Cast<UTextureCube>(Texture);
				if(TextureCube)
				{
					FormatId = TextureCube->Format;
					TexType = TEXT("Cube");
					// Calculate in game current dimensions 
					// Use one face of the texture cube to calculate in game size
					UTexture2D* Face = TextureCube->GetFace(0);
					if( Face )
					{
						const INT DroppedMips = Face->Mips.Num() - Face->ResidentMips;
						CurrentDim = FString::Printf(TEXT("%dx%d"), Face->SizeX >> DroppedMips, Face->SizeY >> DroppedMips);
					}
				}
			}

			FLOAT CurrentKB = CurrentInBytes / 1024.0f;
			FLOAT FullyLoadedKB = FullyLoadedInBytes / 1024.0f;

			// 10KB should be enough
			FNboSerializeToBuffer PayloadWriter(10 * 1024);

			PayloadWriter << FullyQualifiedPath << MaxDim << CurrentDim << TexType << FormatId << GroupId << FullyLoadedKB << CurrentKB << LastTimeRendered << NumUses << LODBias;

			// send it over the wire
			ToServerConnection.SendTo(SrcAddr, PayloadWriter, PayloadWriter.GetByteCount());
		}

		// send end terminator \0
		BYTE Terminator = 0;
		ToServerConnection.SendTo(SrcAddr, &Terminator, 1);
		return TRUE;
	}
#endif // WITH_UE3_NETWORKING && !FINAL_RELEASE
	else if(ParseCommand(&Cmd,TEXT("LISTANIMSETS")))
	{
		bAlphaSort = ParseParam( Cmd, TEXT("ALPHASORT") );

		TArray<FSortedSet> SortedSets;
		for( TObjectIterator<UAnimSet> It; It; ++It )
		{
			UAnimSet* Set = *It;
			new(SortedSets) FSortedSet(Set->GetPathName(), Set->GetResourceSize());
		}

		// Sort anim sets by cost
		Sort<USE_COMPARE_CONSTREF(FSortedSet,UnPlayer)>(SortedSets.GetTypedData(),SortedSets.Num());

		// Now print them out.
		Ar.Logf(TEXT("Loaded AnimSets:"));
		INT TotalSize = 0;
		for(INT i=0; i<SortedSets.Num(); i++)
		{
			FSortedSet& SetInfo = SortedSets(i);
			TotalSize += SetInfo.Size;
			Ar.Logf(TEXT("Size: %10d\tName: %s"), SetInfo.Size, *SetInfo.Name);
		}
		Ar.Logf(TEXT("Total Size:%10d(%0.2f KB, %0.2f MB)"), TotalSize, TotalSize/1024.f, TotalSize/1024.f/1024.f);
		return TRUE;
	}
	else if(ParseCommand(&Cmd,TEXT("LISTMATINEEANIMSETS")))
	{
		bAlphaSort = ParseParam( Cmd, TEXT("ALPHASORT") );

		TArray<UObject*> ObjectList;
		TArray<FSortedSet> SortedSets;

		// Iterate over all interp groups in the current level and gather anim set/ sequence usage.
		for( TObjectIterator<UInterpGroup> It; It; ++It )
		{
			UInterpGroup* InterpGroup = *It;
			UInterpData* OuterInterpData = Cast<UInterpData>(InterpGroup->GetOuter());
						// Iterate over all tracks to find anim control tracks and their anim sequences.
			for( INT TrackIndex=0; TrackIndex<InterpGroup->InterpTracks.Num(); TrackIndex++ )
			{
				UInterpTrack*				InterpTrack = InterpGroup->InterpTracks(TrackIndex);
				UInterpTrackAnimControl*	AnimControl	= Cast<UInterpTrackAnimControl>(InterpTrack);				
				if( AnimControl )
				{
					// Iterate over all track key/ sequences and find the associated sequence.
					for( INT TrackKeyIndex=0; TrackKeyIndex<AnimControl->AnimSeqs.Num(); TrackKeyIndex++ )
					{
						const FAnimControlTrackKey& TrackKey = AnimControl->AnimSeqs(TrackKeyIndex);
						UAnimSequence* AnimSequence			 = AnimControl->FindAnimSequenceFromName( TrackKey.AnimSeqName );
						if( AnimSequence )
						{
							if ( AnimSequence->GetAnimSet() )
							{
								ObjectList.AddUniqueItem(AnimSequence->GetAnimSet());
							}
							else
							{
								ObjectList.AddUniqueItem(AnimSequence);
							}
						}
					}
				}	 
			}
		}

		for ( INT I=0; I<ObjectList.Num(); ++I )
		{
			// Also kee track of sets used.
			new(SortedSets) FSortedSet(ObjectList(I)->GetPathName(), ObjectList(I)->GetResourceSize());
		}

		// Sort anim sets by cost
		Sort<USE_COMPARE_CONSTREF(FSortedSet,UnPlayer)>(SortedSets.GetTypedData(),SortedSets.Num());

		// Now print them out.
		Ar.Logf(TEXT("Loaded Matinee AnimSets:"));
		INT TotalSize = 0;
		for(INT i=0; i<SortedSets.Num(); i++)
		{
			FSortedSet& SetInfo = SortedSets(i);
			TotalSize += SetInfo.Size;
			Ar.Logf(TEXT("Size: %10d\tName: %s"), SetInfo.Size, *SetInfo.Name);
		}
		Ar.Logf(TEXT("Total Size:%d(%0.2f KB, %0.2f MB)"), TotalSize, TotalSize/1024.f, TotalSize/1024.f/1024.f);
		return TRUE;
	}
	else if(ParseCommand(&Cmd,TEXT("LISTANIMTREES")))
	{
		bAlphaSort = ParseParam( Cmd, TEXT("ALPHASORT") );

		TArray<FSortedSet> SortedSets;

		FString Description;
		for( TObjectIterator<UAnimTree> It; It; ++It )
		{
			UAnimTree* Tree = *It;
			Description = FString::Printf(TEXT("[%s] (%s)"), (Tree->SkelComponent && Tree->SkelComponent->GetOwner())? *Tree->SkelComponent->GetOwner()->GetName(): TEXT("No Owner"), *Tree->GetPathName());
			new(SortedSets) FSortedSet(Description, Tree->GetTotalNodeBytes());
		}

		// Sort anim sets by cost
		Sort<USE_COMPARE_CONSTREF(FSortedSet,UnPlayer)>(SortedSets.GetTypedData(),SortedSets.Num());

		// Now print them out.
		Ar.Logf(TEXT("AnimTrees:"));
		INT TotalSize = 0;
		for(INT i=0; i<SortedSets.Num(); i++)
		{
			FSortedSet& SetInfo = SortedSets(i);
			TotalSize += SetInfo.Size;
			Ar.Logf(TEXT("Size: %10d\tName: %s"), SetInfo.Size, *SetInfo.Name);
		}
		Ar.Logf(TEXT("Total Size:%d(%0.2f KB)"), TotalSize, TotalSize/1024.f);
		return TRUE;
	}
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	else if(ParseCommand(&Cmd,TEXT("LISTPARTICLESYSTEMS")))
	{
		TArray<FString> Switches;
		TArray<FString> Tokens;
		appParseCommandLine(Cmd, Tokens, Switches);

		bAlphaSort = (Tokens.FindItemIndex(TEXT("ALPHASORT")) != INDEX_NONE) || (Switches.FindItemIndex(TEXT("ALPHASORT")) != INDEX_NONE);
		UBOOL bDumpMesh = (Tokens.FindItemIndex(TEXT("DUMPMESH")) != INDEX_NONE) || (Switches.FindItemIndex(TEXT("DUMPMESH")) != INDEX_NONE);

		TArray<FSortedParticleSet> SortedSets;
		TMap<UObject *,INT> SortMap;

		FString Description;
		for( TObjectIterator<UParticleSystem> SystemIt; SystemIt; ++SystemIt )
		{			
			UParticleSystem* Tree = *SystemIt;
			Description = FString::Printf(TEXT("%s"), *Tree->GetPathName());
			FArchiveCountMem Count( Tree );
			INT RootSize = Count.GetMax();

			FSortedParticleSet *pSet = new(SortedSets) FSortedParticleSet(Description, RootSize, RootSize, 0, 0, 0, 0, 0);

			SortMap.Set(Tree,SortedSets.Num() - 1);
		}
		
		for( TObjectIterator<UParticleModule> It; It; ++It )
		{
			UParticleModule* Module = *It;
			INT *pIndex = SortMap.Find(Module->GetOuter());
			
			if (pIndex && SortedSets.IsValidIndex(*pIndex))
			{
				FSortedParticleSet &Set = SortedSets(*pIndex);
				FArchiveCountMem ModuleCount( Module );
				Set.ModuleSize += ModuleCount.GetMax();
				Set.Size += ModuleCount.GetMax();
			}
		}

		for( TObjectIterator<UParticleSystemComponent> It; It; ++It )
		{
			UParticleSystemComponent* Comp = *It;
			INT *pIndex = SortMap.Find(Comp->Template);
			
			if (pIndex && SortedSets.IsValidIndex(*pIndex))
			{				
				FSortedParticleSet &Set = SortedSets(*pIndex);				
				FArchiveCountMem ComponentCount( Comp );
				Set.ComponentSize += ComponentCount.GetMax();

				UBOOL bSavedMode = GExclusiveResourceSizeMode;
				GExclusiveResourceSizeMode = FALSE;
				// Save this for adding to the total
				INT CompResSize = Comp->GetResourceSize();
				Set.ComponentResourceSize += CompResSize;
				GExclusiveResourceSizeMode = TRUE;
				Set.ComponentTrueResourceSize += Comp->GetResourceSize();
				GExclusiveResourceSizeMode = bSavedMode;

				Set.Size += ComponentCount.GetMax();
				Set.Size += CompResSize;
				Set.ComponentCount++;

				UParticleSystem* Tree = Comp->Template;
				if (bDumpMesh && Tree != NULL)
				{
					for (INT EmitterIdx = 0; (EmitterIdx < Tree->Emitters.Num()); EmitterIdx++)
					{
						UParticleEmitter* Emitter = Tree->Emitters(EmitterIdx);
						if (Emitter != NULL)
						{
							// Have to check each LOD level...
							if (Emitter->LODLevels.Num() > 0)
							{
								UParticleLODLevel* LODLevel = Emitter->LODLevels(0);
								if (LODLevel != NULL)
								{
									if (LODLevel->RequiredModule->bUseLocalSpace == TRUE)
									{
										UParticleModuleTypeDataMesh* MeshTD = Cast<UParticleModuleTypeDataMesh>(LODLevel->TypeDataModule);
										if (MeshTD != NULL)
										{
											INT InstCount = 0;
											// MESH EMITTER
											if (EmitterIdx < Comp->EmitterInstances.Num())
											{
												FParticleEmitterInstance* Inst = Comp->EmitterInstances(EmitterIdx);
												if (Inst != NULL)
												{
													InstCount = Inst->ActiveParticles;
												}

												warnf(NAME_Warning, TEXT("---> PSys w/ mesh emitters: %2d %4d %s %s "), EmitterIdx, InstCount, 
													(Comp->SceneInfo && Comp->SceneInfo->Proxy) ? TEXT("Y") : TEXT("N"),
													*(Tree->GetPathName()));
											}
										}
									}
								}
							}
						}
					}
				}

			}
		}

		// Sort anim sets by cost
		Sort<USE_COMPARE_CONSTREF(FSortedParticleSet,UnPlayer)>(SortedSets.GetTypedData(),SortedSets.Num());

		// Now print them out.
		Ar.Logf(TEXT("ParticleSystems:"));
		Ar.Logf(TEXT("Size,Name,PSysSize,ModuleSize,ComponentSize,ComponentCount,CompResSize,CompTrueResSize"));
		INT TotalSize = 0;
		for(INT i=0; i<SortedSets.Num(); i++)
		{
			FSortedParticleSet& SetInfo = SortedSets(i);
			TotalSize += SetInfo.Size;
			Ar.Logf(TEXT("%10d,%s,%d,%d,%d,%d,%d,%d"), 
				SetInfo.Size, *SetInfo.Name, SetInfo.PSysSize, SetInfo.ModuleSize, SetInfo.ComponentSize, 
				SetInfo.ComponentCount, SetInfo.ComponentResourceSize, SetInfo.ComponentTrueResourceSize);
		}
		Ar.Logf(TEXT("Total Size:%d(%0.2f KB)"), TotalSize, TotalSize/1024.f);
		return TRUE;
	}
#endif
	else if(ParseCommand(&Cmd,TEXT("LISTFACEFX")))
	{
		bAlphaSort = ParseParam( Cmd, TEXT("ALPHASORT") );

		TArray<FSortedSet> SortedSets;

		FString Description;
		for( TObjectIterator<UFaceFXAsset> It; It; ++It )
		{
			UFaceFXAsset* FaceFXAsset = *It;
			Description = FString::Printf(TEXT("(%s)"), *FaceFXAsset->GetPathName());
			new(SortedSets) FSortedSet(Description, FaceFXAsset->GetResourceSize());
		}

		// Sort anim sets by cost
		Sort<USE_COMPARE_CONSTREF(FSortedSet,UnPlayer)>(SortedSets.GetTypedData(),SortedSets.Num());

		// Now print them out.
		Ar.Logf(TEXT("FaceFXAssets:"));
		INT TotalSize = 0;
		for(INT i=0; i<SortedSets.Num(); i++)
		{
			FSortedSet& SetInfo = SortedSets(i);
			TotalSize += SetInfo.Size;
			Ar.Logf(TEXT("Size: %10d\tName: %s"), SetInfo.Size, *SetInfo.Name);
		}
		Ar.Logf(TEXT("Total Size:%d(%0.2f KB)"), TotalSize, TotalSize/1024.f);

		SortedSets.Empty();

		for( TObjectIterator<UFaceFXAnimSet> It; It; ++It )
		{
			UFaceFXAnimSet* FaceFXAnimSet = *It;
			Description = FString::Printf(TEXT("(%s)"), *FaceFXAnimSet->GetPathName());
			new(SortedSets) FSortedSet(Description, FaceFXAnimSet->GetResourceSize());
		}

		// Sort anim sets by cost
		Sort<USE_COMPARE_CONSTREF(FSortedSet,UnPlayer)>(SortedSets.GetTypedData(),SortedSets.Num());

		// Now print them out.
		Ar.Logf(TEXT("FaceFXAnimSets:"));
		TotalSize = 0;
		for(INT i=0; i<SortedSets.Num(); i++)
		{
			FSortedSet& SetInfo = SortedSets(i);
			TotalSize += SetInfo.Size;
			Ar.Logf(TEXT("Size: %10d\tName: %s"), SetInfo.Size, *SetInfo.Name);
		}
		Ar.Logf(TEXT("Total Size:%d(%0.2f KB)"), TotalSize, TotalSize/1024.f);

		return TRUE;
	}
#if !FINAL_RELEASE
	else if(ParseCommand(&Cmd,TEXT("LISTUNCACHEDSTATICLIGHTINGINTERACTIONS")))
	{
		extern void ListUncachedStaticLightingInteractions( FOutputDevice& Ar );
		ListUncachedStaticLightingInteractions( Ar );
		return TRUE;
	}
#endif
	else if(ParseCommand(&Cmd,TEXT("LISTSEQPOOL")))
	{
		UAnimNodeSlot::PrintSlotNodeSequencePool();
	}
#if !FINAL_RELEASE
	else if(ParseCommand(&Cmd,TEXT("ANIMSEQSTATS")))
	{
		extern void GatherAnimSequenceStats(FOutputDevice& Ar);
		GatherAnimSequenceStats( Ar );
		return TRUE;
	}
#endif
	else if( ParseCommand(&Cmd,TEXT("SHOWHOTKISMET")) )
	{
		// First make array of all USequenceOps
		TArray<USequenceOp*> AllOps;
		for( TObjectIterator<USequenceOp> It; It; ++It )
		{
			USequenceOp* Op = *It;
			AllOps.AddItem(Op);
		}

		// Then sort them
		Sort<USE_COMPARE_POINTER(USequenceOp, UnPlayer)>(&AllOps(0), AllOps.Num());

		// Then print out the top 10
		INT TopNum = ::Min(10, AllOps.Num());
		Ar.Logf( TEXT("----- TOP 10 KISMET SEQUENCEOPS ------") );
		for(INT i=0; i<TopNum; i++)
		{
			Ar.Logf( TEXT("%6d: %s (%d)"), i, *(AllOps(i)->GetPathName()), AllOps(i)->ActivateCount );
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("DLE")) )
	{
		Ar.Logf( TEXT( "Looking for disabled LightEnvironmentComponents" ) );
		INT Num = 0;
		for( TObjectIterator<ULightEnvironmentComponent	> It; It; ++It )
		{
			ULightEnvironmentComponent* LE = *It;
			if(!LE->IsTemplate() && !LE->IsEnabled())
			{
				Ar.Logf(TEXT("  Disabled DLE: %d %s"), Num, *LE->GetPathName());
				Num++;
			}
		}
		Ar.Logf( TEXT( "Total Disabled found: %i" ), Num );

		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("DepthPrepassCullingThreshold")))
	{
		extern FLOAT GMinScreenRadiusForDepthPrepassSquared;

		FString Parameter(ParseToken(Cmd, 0));
		if(Parameter.Len())
		{
			FLOAT Value = Max(appAtof(*Parameter), 0.0f);
			GMinScreenRadiusForDepthPrepassSquared = Value * Value;
		}

		Ar.Logf( TEXT("GMinScreenRadiusForDepthPrepass = %g  (used with no full screen shadows)"), appSqrt(GMinScreenRadiusForDepthPrepassSquared) );
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("DepthPrepassCullingThresholdFullShadows")))
	{
		extern FLOAT GMinScreenRadiusForDepthPrepassWithShadowsSquared;

		FString Parameter(ParseToken(Cmd, 0));
		if(Parameter.Len())
		{
			FLOAT Value = Max(appAtof(*Parameter), 0.0f);
			GMinScreenRadiusForDepthPrepassWithShadowsSquared = Value * Value;
		}

		Ar.Logf( TEXT("GMinScreenRadiusForDepthPrepassWithShadows = %g  (used with full screen shadows)"), appSqrt(GMinScreenRadiusForDepthPrepassWithShadowsSquared) );
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("MinRadiusToRenderShadowDepth")))
	{
		extern FLOAT GMinScreenRadiusForShadowDepthSquared;

		FString Parameter(ParseToken(Cmd, 0));
		if(Parameter.Len())
		{
			FLOAT Value = Max(appAtof(*Parameter), 0.0f);
			GMinScreenRadiusForShadowDepthSquared = Value * Value;
		}

		Ar.Logf( TEXT("GMinScreenRadiusForShadowDepth = %g"), appSqrt(GMinScreenRadiusForShadowDepthSquared) );
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PreshadowNearPlaneExtension")))
	{
		extern FLOAT GDirectionalLightPreshadowNearPlaneExtensionFactor;

		FString Parameter(ParseToken(Cmd, 0));
		if(Parameter.Len())
		{
			FLOAT Value = appAtof(*Parameter);
			GDirectionalLightPreshadowNearPlaneExtensionFactor = Value;
		}

		Ar.Logf( TEXT("GDirectionalLightPreshadowNearPlaneExtensionFactor = %g"), GDirectionalLightPreshadowNearPlaneExtensionFactor );
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PreshadowCachedExpandFraction")))
	{
		extern FLOAT GPreshadowExpandFraction;

		FString Parameter(ParseToken(Cmd, 0));
		if(Parameter.Len())
		{
			FLOAT Value = appAtof(*Parameter);
			GPreshadowExpandFraction = Value;
		}

		Ar.Logf( TEXT("GPreshadowExpandFraction = %g"), GPreshadowExpandFraction );
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("ToggleCachePreshadows")))
	{
		extern UBOOL GCachePreshadows;

		GCachePreshadows = !GCachePreshadows;
		Ar.Logf( TEXT("GCachePreshadows = %s"), GCachePreshadows ? TEXT("TRUE") : TEXT("FALSE") );
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("LogShadowGeneration")))
	{
		extern UBOOL GLogNextFrameShadowGenerationData;

		GLogNextFrameShadowGenerationData = TRUE;
		Ar.Logf( TEXT("GLogNextFrameShadowGenerationData = %s"), GLogNextFrameShadowGenerationData ? TEXT("TRUE") : TEXT("FALSE") );
		return TRUE;
	}
#endif // !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE

#if !CONSOLE && defined(_MSC_VER)
	else if( ParseCommand( &Cmd, TEXT("TOGGLEDEBUGGER") ) )
	{
		if ( GDebugger == NULL )
		{
			UDebuggerCore::InitializeDebugger();
		}
		UDebuggerCore* ScriptDebugger = static_cast<UDebuggerCore*>(GDebugger);
		ScriptDebugger->AttachScriptDebugger(!ScriptDebugger->IsDebuggerAttached());
		return TRUE;
	}
#endif

#if !FINAL_RELEASE
	else if ( ParseCommand(&Cmd, TEXT("DEBUGPREFAB")) )
	{
		FString ObjectName;
		if ( ParseToken(Cmd,ObjectName,TRUE) )
		{
			UObject* TargetObject = FindObject<UObject>(ANY_PACKAGE,*ObjectName);
			if ( TargetObject != NULL )
			{
				UBOOL bParsedCommand = FALSE;
				if ( ParseCommand(&Cmd, TEXT("SHOWMAP")) )
				{
					bParsedCommand = TRUE;

					TMap<UObject*,UObject*>* ArcInstMap=NULL;
					APrefabInstance* ActorPrefab = Cast<APrefabInstance>(TargetObject);
					if ( ActorPrefab != NULL )
					{
						ArcInstMap = &ActorPrefab->ArchetypeToInstanceMap;
					}

					if ( ArcInstMap != NULL )
					{
						INT ElementIndex = 0;
						INT IndexPadding = appItoa(ArcInstMap->Num()).Len();
						Ar.Logf(TEXT("Archetype mappings for %s  (%i elements)"), *TargetObject->GetFullName(), ArcInstMap->Num());
						for ( TMap<UObject*,UObject*>::TConstIterator It(*ArcInstMap); It; ++It )
						{
							UObject* Arc = It.Key();
							UObject* Inst = It.Value();
							UObject* RealArc = Inst != NULL ? Inst->GetArchetype() : NULL;

							if ( Arc != NULL && Inst != NULL )
							{
								if ( Arc == RealArc )
								{
									Ar.Logf(TEXT(" +  %*i) %s ==> %s"), IndexPadding, ElementIndex++, *Inst->GetPathName(), *Arc->GetPathName());
								}
								else
								{
									Ar.Logf(TEXT(" .  %*i) %s ==> %s (%s)"), IndexPadding, ElementIndex++, *Inst->GetPathName(), *Arc->GetPathName(), *RealArc->GetPathName());
								}
							}
							else if ( Inst == NULL )
							{
								Ar.Logf(TEXT(" ?  %*i) %s ==> %s"), IndexPadding, ElementIndex++, *Inst->GetPathName(), *Arc->GetPathName());
							}
							else if ( Arc == NULL )
							{
								Ar.Logf(TEXT(" !  %*i) %s ==> %s"), IndexPadding, ElementIndex++, *Inst->GetPathName(), *RealArc->GetPathName());
							}
						}
					}
					else
					{
						Ar.Logf(TEXT("'%s' is not a prefab instance"), *TargetObject->GetFullName());
					}
				}

				if ( ParseCommand(&Cmd, TEXT("GRAPH")) )
				{
					bParsedCommand = TRUE;
					ShowSubobjectGraph( Ar, TargetObject, TEXT("") );
				}

				if ( !bParsedCommand )
				{
					Ar.Logf(TEXT("SYNTAX:  DEBUGPREFAB <PathNameForPrefabInstance> <COMMAND>"));
				}
			}
			else
			{
				Ar.Logf(TEXT("No object found using '%s'"), *ObjectName);
			}
		}
		else
		{
			Ar.Logf(TEXT("No object name specified - syntax: DEBUGPREFAB PathNameForPrefabInstance COMMAND"));
		}

		return TRUE;
	}
#endif // !FINAL_RELEASE
	else if( ParseCommand(&Cmd,TEXT("TOGGLEMINDISTORT")) )
	{
		extern UBOOL GRenderMinimalDistortion;
		FlushRenderingCommands();
		GRenderMinimalDistortion = !GRenderMinimalDistortion;
		debugf(TEXT("Resolve minimal screen distortion rectangle = %s"),
			GRenderMinimalDistortion ? TEXT("TRUE") : TEXT("FALSE"));
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("TOGGLEMINTRANSLUCENT")) )
	{
		extern UBOOL GRenderMinimalTranslucency;
		FlushRenderingCommands();
		GRenderMinimalTranslucency = !GRenderMinimalTranslucency;
		debugf(TEXT("Resolve minimal translucency rectangle = %s"),
			GRenderMinimalTranslucency ? TEXT("TRUE") : TEXT("FALSE"));
		return TRUE;
	}
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	else if( ParseCommand(&Cmd,TEXT("LISTSPAWNEDACTORS")) )
	{
		if( GWorld && GamePlayers.Num() )
		{
			const FLOAT	TimeSeconds		    = GWorld->GetTimeSeconds();

			// Create alphanumerically sorted list of actors in persistent level.
			TArray<AActor*> SortedActorList = GWorld->PersistentLevel->Actors;
			Sort<USE_COMPARE_POINTER( AActor, UnPlayer )>( SortedActorList.GetTypedData(), SortedActorList.Num() );

			Ar.Logf(TEXT("Listing spawned actors in persistent level:"));
			Ar.Logf(TEXT("Total: %d" ), SortedActorList.Num());

			if ( GamePlayers.Num() )
			{
				// If have local player, give info on distance to player
				const FVector PlayerLocation = GamePlayers(0)->LastViewLocation;

				// Iterate over all non-static actors and log detailed information.
				Ar.Logf(TEXT("TimeUnseen,TimeAlive,Distance,Class,Name,Owner"));
				for( INT ActorIndex=0; ActorIndex<SortedActorList.Num(); ActorIndex++ )
				{
					AActor* Actor = SortedActorList(ActorIndex);
					if( Actor && !Actor->IsStatic() )
					{
						// Calculate time actor has been alive for. Certain actors can be spawned before TimeSeconds is valid
						// so we manually reset them to the same time as TimeSeconds.
						FLOAT TimeAlive	= TimeSeconds - Actor->CreationTime;
						if( TimeAlive < 0 )
						{
							TimeAlive = TimeSeconds;
						}
						const FLOAT TimeUnseen = TimeSeconds - Actor->LastRenderTime;
						const FLOAT DistanceToPlayer = FDist( Actor->Location, PlayerLocation );
						Ar.Logf(TEXT("%6.2f,%6.2f,%8.0f,%s,%s,%s"),TimeUnseen,TimeAlive,DistanceToPlayer,*Actor->GetClass()->GetName(),*Actor->GetName(),*Actor->Owner->GetName());
					}
				}
			}
			else
			{
				// Iterate over all non-static actors and log detailed information.
				Ar.Logf(TEXT("TimeAlive,Class,Name,Owner"));
				for( INT ActorIndex=0; ActorIndex<SortedActorList.Num(); ActorIndex++ )
				{
					AActor* Actor = SortedActorList(ActorIndex);
					if( Actor && !Actor->IsStatic() )
					{
						// Calculate time actor has been alive for. Certain actors can be spawned before TimeSeconds is valid
						// so we manually reset them to the same time as TimeSeconds.
						FLOAT TimeAlive	= TimeSeconds - Actor->CreationTime;
						if( TimeAlive < 0 )
						{
							TimeAlive = TimeSeconds;
						}
						Ar.Logf(TEXT("%6.2f,%s,%s,%s"),TimeAlive,*Actor->GetClass()->GetName(),*Actor->GetName(),*Actor->Owner->GetName());
					}
				}
			}
		}
		else
		{
			Ar.Logf(TEXT("LISTSPAWNEDACTORS failed."));
		}
		return TRUE;
	}
#endif // !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
#if !FINAL_RELEASE
	else if ( ParseCommand(&Cmd,TEXT("CrossLevelMem")) )
	{
		FCrossLevelReferenceManager::DumpMemoryUsage(Ar);
		return TRUE;
	}
#endif // !FINAL_RELEASE
#if !FINAL_RELEASE
	else if (ParseCommand(&Cmd,TEXT("MERGEMESH")))
	{
		FString CmdCopy = Cmd;
		TArray<FString> Tokens;
		while (CmdCopy.Len() > 0)
		{
			const TCHAR* LocalCmd = *CmdCopy;
			FString Token = ParseToken(LocalCmd, ' ');
			Tokens.AddItem(Token);
			CmdCopy = CmdCopy.Right(CmdCopy.Len() - Token.Len() - 1);
		}

		// array of source meshes that will be merged
		TArray<USkeletalMesh*> SourceMeshList;

		if ( Tokens.Num() >= 2 )
		{
			for (INT I=0; I<Tokens.Num(); ++I)
			{
				USkeletalMesh * SrcMesh = LoadObject<USkeletalMesh>(NULL, *Tokens(I), NULL, LOAD_None, NULL);
				if (SrcMesh)
				{
					SourceMeshList.AddItem(SrcMesh);
				}
			}
		}

		AWorldInfo* Info = GWorld->GetWorldInfo();

		// find player controller skeletalmesh
		APawn * PlayerPawn = NULL;
		USkeletalMesh * PlayerMesh = NULL;
		for (AController* C = Info->ControllerList; C != NULL; C = C->NextController)
		{
			APlayerController* PC = C->GetAPlayerController();
			if (PC != NULL && PC->Pawn != NULL && PC->Pawn->Mesh != NULL)
			{
				PlayerPawn = PC->Pawn;
				PlayerMesh = PC->Pawn->Mesh->SkeletalMesh;
				break;
			}
		}

		if (PlayerMesh)
		{
			if (SourceMeshList.Num() ==  0)
			{
				SourceMeshList.AddItem(PlayerMesh);
				SourceMeshList.AddItem(PlayerMesh);
			}
		}
		else
		{
			// we don't have a pawn (because we couldn't find a mesh), use any pawn as a spawn point
			for (AController* C = Info->ControllerList; C != NULL; C = C->NextController)
			{
				APlayerController* PC = C->GetAPlayerController();
				if (PC != NULL && PC->Pawn != NULL)
				{
					PlayerPawn = PC->Pawn;
					break;
				}
			}		
		}

		if (PlayerPawn && SourceMeshList.Num() >= 2)
		{
			// create the composite mesh
			USkeletalMesh* CompositeMesh = CastChecked<USkeletalMesh>(StaticConstructObject(USkeletalMesh::StaticClass(), GetTransientPackage(), NAME_None, RF_Transient));

			TArray<FSkelMeshMergeSectionMapping> InForceSectionMapping;
			// create an instance of the FSkeletalMeshMerge utility
			FSkeletalMeshMerge MeshMergeUtil( CompositeMesh, SourceMeshList, InForceSectionMapping,  0);

			// merge the source meshes into the composite mesh
			if( !MeshMergeUtil.DoMerge() )
			{
				// handle errors
				// ...
				debugf(TEXT("DoMerge Error: Merge Mesh Test Failed"));
				return TRUE;
			}

			FVector SpawnLocation = PlayerPawn->Location + PlayerPawn->Rotation.Vector()*50;

			// set the new composite mesh in the existing skeletal mesh component
			ASkeletalMeshActorSpawnable* SMA = Cast<ASkeletalMeshActorSpawnable>(GWorld->SpawnActor(ASkeletalMeshActorSpawnable::StaticClass(), NAME_None, SpawnLocation, PlayerPawn->Rotation*-1)); 
			if (SMA)
			{
				SMA->SkeletalMeshComponent->SetSkeletalMesh(CompositeMesh);
			}
		}

		return TRUE;
	}
#endif // !FINAL_RELEASE
	else if (ParseCommand(&Cmd,TEXT("TOGGLEONSCREENDEBUGMESSAGEDISPLAY")))
	{
		GEngine->bEnableOnScreenDebugMessagesDisplay = !GEngine->bEnableOnScreenDebugMessagesDisplay;
		debugf(TEXT("OnScreenDebug Message Display is now %s"), 
			GEngine->bEnableOnScreenDebugMessagesDisplay ? TEXT("ENABLED") : TEXT("DISABLED"));
		if ((GEngine->bEnableOnScreenDebugMessagesDisplay == TRUE) && (GEngine->bEnableOnScreenDebugMessages == FALSE))
		{
			debugf(TEXT("OnScreenDebug Message system is DISABLED!"));
		}
	}
	else if (ParseCommand(&Cmd,TEXT("TOGGLEONSCREENDEBUGMESSAGESYSTEM")))
	{
		GEngine->bEnableOnScreenDebugMessages = !GEngine->bEnableOnScreenDebugMessages;
		debugf(TEXT("OnScreenDebug Message System is now %s"), 
			GEngine->bEnableOnScreenDebugMessages ? TEXT("ENABLED") : TEXT("DISABLED"));
	}
	else if (ParseCommand(&Cmd, TEXT("SetMaxMipLevel")))
	{
		GLargestLightmapMipLevel = appAtof(Cmd);
		Ar.Logf( TEXT( "Max lightmap mip level: %f" ), GLargestLightmapMipLevel );
		return TRUE;
	}
#if !FINAL_RELEASE
	else if (ParseCommand(&Cmd, TEXT("ShowMipLevels")))
	{
		extern UBOOL GVisualizeMipLevels;
		GVisualizeMipLevels = !GVisualizeMipLevels;
		Ar.Logf( TEXT( "Showing mip levels: %s" ), GVisualizeMipLevels ? TEXT("ENABLED") : TEXT("DISABLED") );
		return TRUE;
	}
#endif
	else if (ParseCommand(&Cmd, TEXT("ShowSelectedLightmap")))
	{
		GShowDebugSelectedLightmap = !GShowDebugSelectedLightmap;
		GConfig->SetBool(TEXT("DevOptions.Debug"), TEXT("ShowSelectedLightmap"), GShowDebugSelectedLightmap, GEngineIni);
		Ar.Logf( TEXT( "Showing the selected lightmap: %s" ), GShowDebugSelectedLightmap ? TEXT("TRUE") : TEXT("FALSE") );
		return TRUE;
	}
#if !SHIPPING_PC_GAME || UDK
	else if( ParseCommand( &Cmd, TEXT("DEBUG") ) )
	{
		if( ParseCommand(&Cmd,TEXT("RENDERCRASH")) )
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND( CauseRenderThreadCrash, { appErrorf( TEXT("%s"), TEXT("Crashing the renderthread at your request") ); } );
			return TRUE;
		}
		return FALSE;
	}
#endif
#if STATS
	else if( ParseCommand( &Cmd, TEXT("DUMPPARTICLEMEM") ) )
	{
		FParticleDataManager::DumpParticleMemoryStats(Ar);
	}
#endif
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	else if( ParseCommand( &Cmd, TEXT("CLEAREMITTERPOOL") ) )
	{
		if (GWorld != NULL)
		{
			AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
			if (WorldInfo != NULL)
			{
				if (WorldInfo->MyEmitterPool != NULL)
				{
					WorldInfo->MyEmitterPool->ClearPoolComponents(FALSE);
				}
			}
		}
	}
#endif
#if !FINAL_RELEASE
	else if (ParseCommand(&Cmd,TEXT("CountDisabledParticleItems")))
	{
		INT ParticleSystemCount = 0;
		INT EmitterCount = 0;
		INT DisabledEmitterCount = 0;
		INT CookedOutEmitterCount = 0;
		INT LODLevelCount = 0;
		INT DisabledLODLevelCount = 0;
		INT ModuleCount = 0;
		INT DisabledModuleCount = 0;
		TMap<FString, INT> ModuleMap;
		for (TObjectIterator<UParticleSystem> It; It; ++It)
		{
			ParticleSystemCount++;

			TArray<UParticleModule*> ProcessedModules;
			TArray<UParticleModule*> DisabledModules;

			UParticleSystem* PSys = *It;
			for (INT EmitterIdx = 0; EmitterIdx < PSys->Emitters.Num(); EmitterIdx++)
			{
				UParticleEmitter* Emitter = PSys->Emitters(EmitterIdx);
				if (Emitter != NULL)
				{
					UBOOL bDisabledEmitter = TRUE;
					EmitterCount++;
					if (Emitter->bCookedOut == TRUE)
					{
						CookedOutEmitterCount++;
					}
					for (INT LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
					{
						UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIdx);
						if (LODLevel != NULL)
						{
							LODLevelCount++;
							if (LODLevel->bEnabled == FALSE)
							{
								DisabledLODLevelCount++;
							}
							else
							{
								bDisabledEmitter = FALSE;
							}
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

								INT DummyIdx;
								if ((Module != NULL) && (ProcessedModules.FindItem(Module, DummyIdx) == FALSE))
								{
									ModuleCount++;
									ProcessedModules.AddUniqueItem(Module);
									if (Module->bEnabled == FALSE)
									{
										check(DisabledModules.FindItem(Module, DummyIdx) == FALSE);
										DisabledModules.AddUniqueItem(Module);
										DisabledModuleCount++;
									}

									FString ModuleName = Module->GetClass()->GetName();
									INT* ModuleCounter = ModuleMap.Find(ModuleName);
									if (ModuleCounter == NULL)
									{
										INT TempInt = 0;
										ModuleMap.Set(ModuleName, TempInt);
										ModuleCounter = ModuleMap.Find(ModuleName);
									}
									check(ModuleCounter != NULL);
									*ModuleCounter = (*ModuleCounter + 1);
								}
							}
						}
					}

					if (bDisabledEmitter)
					{
						DisabledEmitterCount++;
					}
				}
			}
		}

		debugf(TEXT("%5d particle systems w/ %7d emitters (%5d disabled or %5.3f%% - %4d cookedout)"),
			ParticleSystemCount, EmitterCount, DisabledEmitterCount, (FLOAT)DisabledEmitterCount / (FLOAT)EmitterCount, CookedOutEmitterCount);
		debugf(TEXT("\t%8d lodlevels (%5d disabled or %5.3f%%)"),
			LODLevelCount, DisabledLODLevelCount, (FLOAT)DisabledLODLevelCount / (FLOAT)LODLevelCount);
		debugf(TEXT("\t\t%10d modules (%5d disabled or %5.3f%%)"),
			ModuleCount, DisabledModuleCount, (FLOAT)DisabledModuleCount / (FLOAT)ModuleCount);
		for (TMap<FString,INT>::TIterator It(ModuleMap); It; ++It)
		{
			FString ModuleName = It.Key();
			INT ModuleCounter = It.Value();

			debugf(TEXT("\t\t\t%4d....%s"), ModuleCounter, *ModuleName);
		}

		return TRUE;
	}
#endif
	else if( ParseCommand(&Cmd,TEXT("STAT")) )
	{
		const TCHAR* Temp = Cmd;
		if (ParseCommand(&Temp,TEXT("VERSION")))
		{
			GShowVersion ^= TRUE;
			return TRUE;
		}
		// Check for FPS counter
		else if (ParseCommand(&Temp,TEXT("FPS")))
		{
			GShowFpsCounter ^= TRUE;
			return TRUE;
		}
		else if( ParseCommand(&Temp,TEXT("HITCHES")) )
		{
			GShowHitches ^= TRUE;
			appSleep(0.11f); // cause a hitch so it is evidently working
		}
		else if( ParseCommand(&Temp,TEXT("SUMMARY")) )
		{
			// Toggle FPS and memory stats.			
			GShowMemorySummaryStats ^= TRUE;
			GShowFpsCounter ^= TRUE;
		}
		// Named events emission for e.g. PIX
		else if( ParseCommand(&Temp,TEXT("NAMEDEVENTS")) )
		{
			// Enable emission of named events and force enable cycle stats.
			if( !GCycleStatsShouldEmitNamedEvents )
			{
				GCycleStatsShouldEmitNamedEvents = TRUE;
				GForceEnableScopedCycleStats++;
			}
			// Disable emission of named events and force-enabling cycle stats.
			else
			{
				GCycleStatsShouldEmitNamedEvents = FALSE;
				GForceEnableScopedCycleStats--;
			}
		}
		// Check for Level stats.
		else if (ParseCommand(&Temp,TEXT("LEVELS")))
		{
			GShowLevelStats ^= TRUE;
			return TRUE;
		}
		// Check for Level status map
		else if (ParseCommand(&Temp,TEXT("LEVELMAP")))
		{
			GShowLevelStatusMap ^= TRUE;
			return TRUE;
		}
		// Check for idle times.
		else if (ParseCommand(&Temp,TEXT("UNIT")))
		{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
			if (GShowUnitMaxTimes == TRUE)
			{
				GShowUnitTimes = TRUE;
			}
			else
#endif//#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
			{
				GShowUnitTimes ^= TRUE;
			}
			GShowUnitMaxTimes = FALSE;
			return TRUE;
		}
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		else if (ParseCommand(&Temp,TEXT("UNITMAX")))
		{
			if (GShowUnitMaxTimes == FALSE)
			{
				GShowUnitTimes = TRUE;
			}
			else
			{
				GShowUnitTimes ^= TRUE;
			}
			GShowUnitMaxTimes = GShowUnitTimes;
			return TRUE;
		}
		else if (ParseCommand(&Temp,TEXT("UNITGRAPH")))
		{
			GShowUnitTimeGraph ^= TRUE;
			if( GShowUnitTimeGraph )
			{
				GShowUnitTimes = TRUE;
			}
			return TRUE;
		}
		else if (ParseCommand(&Temp,TEXT("RAW")))
		{
			// Toggle raw vs. filtered stats (running average)
			GShowRawUnitTimes ^= TRUE;
			return TRUE;
		}
#endif//#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		else if(ParseCommand(&Temp,TEXT("SOUNDWAVES")))
		{
			GShowSoundWaves ^= TRUE;
			return TRUE;
		}
		else if(ParseCommand(&Temp, TEXT("SOUNDCUES")))
		{
			GShowSoundCues ^= TRUE;
			return TRUE;
		}
		else if(ParseCommand(&Temp, TEXT("SOUNDS")))
		{
			const UBOOL bHelp = appStristr( Temp, TEXT( "?" ) ) != NULL;

			if( bHelp )
			{
				Ar.Logf(TEXT("stat sounds description"));
				Ar.Logf(TEXT("  stat sounds off - Disables drawing stat sounds"));
				Ar.Logf(TEXT("  stat sounds sort=distance|class|name|waves|default"));
				Ar.Logf(TEXT("      distance - sort list by distance to player"));
				Ar.Logf(TEXT("      class - sort by sound class name"));
				Ar.Logf(TEXT("      name - sort by cue pathname"));
				Ar.Logf(TEXT("      waves - sort by waves' num"));
				Ar.Logf(TEXT("      default - sorting is no enabled"));
				Ar.Logf(TEXT("  stat sounds -debug - enables debugging mode like showing sound radius sphere and names, but only for cues with enabled property bDebug"));
				Ar.Logf(TEXT(""));
				Ar.Logf(TEXT("Ex. stat sounds sort=class -debug"));
				Ar.Logf(TEXT(" This will show only debug sounds sorted by sound class"));
				return TRUE;
			}
			GShowSounds = SSF_Disabled;
			const UBOOL bDebug = ParseParam( Temp, TEXT("debug") );
			GShowSounds |= bDebug ? SSF_Debug : 0;
			
			FString SortStr;
			Parse( Temp, TEXT("sort="), SortStr );

			if( SortStr == TEXT( "distance" ) )
			{
				GShowSounds |= SSF_Sort_Distance;
			}
			else if( SortStr == TEXT( "class" ) )
			{
				GShowSounds |= SSF_Sort_Class;
			}
			else if( SortStr == TEXT( "name" ) )
			{
				GShowSounds |= SSF_Sort_Name;
			}
			else if( SortStr == TEXT( "waves" ) )
			{
				GShowSounds |= SSF_Sort_WavesNum;
			}
			else
			{
				GShowSounds |= SSF_Sort_Disabled;
			}

			const UBOOL bHide = ParseCommand( &Temp, TEXT("off") );
			if( bHide )
			{
				GShowSounds = SSF_Disabled;
			}
			
			return TRUE;
		}
		else if(ParseCommand(&Temp,TEXT("AI")))
		{
			GShowAIStats = !GShowAIStats;
			return TRUE;
		}
#if PS3
		else if( ParseCommand(&Temp,TEXT("GPUMEMALLOCS")) )
		{
			GShowPS3GcmMemoryStats = !GShowPS3GcmMemoryStats;
			return TRUE;
		}		
#endif
#if STATS
		// Forward the call to the stat manager
		else
		{
			return GStatManager.Exec(Cmd,Ar);
		}
#else
		return FALSE;
#endif
	}
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	else if (ParseCommand(&Cmd,TEXT("PRIM")))
	{
		if (ParseCommand(&Cmd,TEXT("NEXT")))
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND( IncTrackPrim, { RHIIncrementTrackedPrimitive(1); } );
		}
		else if (ParseCommand(&Cmd,TEXT("PREV")))
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND( DecTrackPrim, { RHIIncrementTrackedPrimitive(-1); } );
		}
		else if (ParseCommand(&Cmd,TEXT("RESET")))
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND( ResetTrackPrim, { RHIResetTrackedPrimitive(); } );
		}
		else if (ParseCommand(&Cmd,TEXT("MODE")))
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND( CycleTrackPrimMode, { RHICycleTrackedPrimitiveMode(); } );
		}
	}
#endif
	else if (ParseCommand(&Cmd,TEXT("DISABLEALLSCREENMESSAGES")))
	{
		GAreScreenMessagesEnabled = FALSE;
		debugf(TEXT("Onscreen warngins/messages are now DISABLED"));
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("ENABLEALLSCREENMESSAGES")))
	{
		GAreScreenMessagesEnabled = TRUE;
		debugf(TEXT("Onscreen warngins/messages are now ENABLED"));
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("TOGGLEALLSCREENMESSAGES")) || ParseCommand(&Cmd,TEXT("CAPTUREMODE")))
	{
		GAreScreenMessagesEnabled = !GAreScreenMessagesEnabled;
		debugf(TEXT("Onscreen warngins/messages are now %s"),
			GAreScreenMessagesEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
		return TRUE;
	}
#if !FINAL_RELEASE
	else if (ParseCommand(&Cmd,TEXT("ColorGrading")))
	{
		FString Parameter(ParseToken(Cmd, 0));
	
		if( Parameter.Len() > 0 )
		{
			GColorGrading = appAtoi(*Parameter);
		}
		else
		{
			Ar.Logf(TEXT("ColorGrading post processing effect:"));
			Ar.Logf(TEXT("  0=LUT off, 1=LUT on(default), -1=debug input of the LUT blender, -2=debug output of the LUT blender"));
		}
		Ar.Logf(TEXT("ColorGrading = %d"), GColorGrading);
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("BloomWeightSmall")))
	{
		FString Parameter(ParseToken(Cmd, 0));

		if( Parameter.Len() > 0 )
		{
			GBloomWeightSmall = appAtof(*Parameter);
		}
		else
		{
			Ar.Logf(TEXT("Allows to override the bloom internal value BloomWeightSmall"));
			Ar.Logf(TEXT("  <0 = use what is specified elsewhere"));
			Ar.Logf(TEXT("   0 = off"));
			Ar.Logf(TEXT("  >0 = specify weight (up to 1)"));
		}
		Ar.Logf(TEXT("GBloomWeightSmall = %g"), GBloomWeightSmall);
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("BloomWeightLarge")))
	{
		FString Parameter(ParseToken(Cmd, 0));

		if( Parameter.Len() > 0 )
		{
			GBloomWeightLarge = appAtof(*Parameter);
		}
		else
		{
			Ar.Logf(TEXT("Allows to override the bloom internal value BloomWeightLarge"));
			Ar.Logf(TEXT("  <0 = use what is specified elsewhere"));
			Ar.Logf(TEXT("   0 = off"));
			Ar.Logf(TEXT("  >0 = specify weight (up to 1)"));
		}
		Ar.Logf(TEXT("GBloomWeightLarge = %g"), GBloomWeightLarge);
		return TRUE;
	}
#endif // !FINAL_RELEASE
	else if (ParseCommand(&Cmd,TEXT("DrawGFx")))
	{
		FString Parameter(ParseToken(Cmd, 0));

		if ( Parameter.Len() > 0 )
		{
			GDrawGFx = (appAtoi(*Parameter) > 0) ? TRUE : FALSE;
		}
		else
		{
			Ar.Logf(TEXT("DrawGFx enabled/disabled GFx rendering:"));
			Ar.Logf(TEXT("  0=off, 1=Enable GFx rendering"));
		}
		Ar.Logf(TEXT("DrawGFx = %d"), GDrawGFx);
		return TRUE;
	}
#if !FINAL_RELEASE
	else if (ParseCommand(&Cmd, TEXT("VisualizeTexture")) || ParseCommand(&Cmd, TEXT("Vis")))
	{
		extern INT GVisualizeTexture;
		extern FLOAT GVisualizeTextureRGBMul;
		extern FLOAT GVisualizeTextureAMul;
		extern INT GVisualizeTextureInputMapping;
		extern INT GVisualizeTextureFlags;

		FlushRenderingCommands();

		UINT ParameterCount = 0;
		UBOOL bSaveBitmap = FALSE;
		UBOOL bDumpInvalidOnes = FALSE;

		// parse parameters
		for(;;)
		{
			FString Parameter = ParseToken(Cmd, 0);

			if(Parameter.IsEmpty())
			{
				break;
			}

			Parameter.ToLower();

			if(Parameter == TEXT("all"))
			{
				bDumpInvalidOnes = TRUE;
			}
			else if(ParameterCount == 0)
			{
				// Init
				GVisualizeTextureRGBMul = 1;
				GVisualizeTextureAMul = 0;
				GVisualizeTextureInputMapping = 0;
				GVisualizeTextureFlags = 0;
				GVisualizeTexture = appAtoi(*Parameter);
			}
			// GVisualizeTextureInputMapping mode
			else if(Parameter == TEXT("uv0"))
			{
				// default already covers this
			}
			else if(Parameter == TEXT("uv1"))
			{
				GVisualizeTextureInputMapping = 1;
			}
			else if(Parameter == TEXT("uv2"))
			{
				GVisualizeTextureInputMapping = 2;
			}
			// BMP flag
			else if(Parameter == TEXT("bmp"))
			{
				bSaveBitmap = TRUE;
			}
			// saturate flag
			else if(Parameter == TEXT("frac"))
			{
				// default already covers this
			}
			// saturate flag
			else if(Parameter == TEXT("sat"))
			{
				GVisualizeTextureFlags |= 0x1;
			}
			// e.g. RGB*6, A, *22, /2.7, A*7
			else if(Parameter.Left(3) == TEXT("rgb")
				|| Parameter.Left(1) == TEXT("a")
				|| Parameter.Left(1) == TEXT("*")
				|| Parameter.Left(1) == TEXT("/"))
			{
				if(Parameter.Left(3) == TEXT("rgb"))
				{
					Parameter = Parameter.Right(Parameter.Len() - 3);
				}
				else if(Parameter.Left(1) == TEXT("a"))
				{
					Parameter = Parameter.Right(Parameter.Len() - 1);
					GVisualizeTextureRGBMul = 0;
					GVisualizeTextureAMul = 1;
				}

				float Mul = 1.0f;

				// * or /
				if(Parameter.Left(1) == TEXT("*"))
				{
					Parameter = Parameter.Right(Parameter.Len() - 1);
					Mul = appAtof(*Parameter);
				}
				else if(Parameter.Left(1) == TEXT("/"))
				{
					Parameter = Parameter.Right(Parameter.Len() - 1);
					Mul = 1.0f / appAtof(*Parameter);
				}
				GVisualizeTextureRGBMul *= Mul;
				GVisualizeTextureAMul *= Mul;
			}
			else
			{
				Ar.Logf(TEXT("Error: parameter \"%s\" not recognized"), *Parameter);
			}

			++ParameterCount;
		}

#if !CONSOLE
		if(bSaveBitmap)
		{
			SCOPED_SUSPEND_RENDERING_THREAD(FSuspendRenderingThread::ST_RecreateThread);

			// save content to disk into the screenshots folder
			FIntPoint Extent;
			GSceneRenderTargets.GetRenderTargetInfo((ESceneRenderTargetTypes)(GVisualizeTexture - 1), Extent);
			const FSurfaceRHIRef& VisTextureSurface = GSceneRenderTargets.GetRenderTargetSurface((ESceneRenderTargetTypes)(GVisualizeTexture - 1));

			if(IsValidRef(VisTextureSurface) && Extent.X && Extent.Y)
			{
				FReadSurfaceDataFlags ReadDataFlags;

				TArray<FColor> Bitmap;
				if(GSystemSettings.UsesMSAA())
				{
					RHIReadSurfaceDataMSAA(VisTextureSurface,0,0,Extent.X - 1,Extent.Y - 1,Bitmap,ReadDataFlags);
				}
				else
				{
					TArray<BYTE> BitmapBytes;
					BitmapBytes.Init(Extent.X * Extent.Y * sizeof(FColor));
					RHIReadSurfaceData(
						VisTextureSurface,
						0,
						0,
						Extent.X - 1,
						Extent.Y - 1,
						BitmapBytes,
						ReadDataFlags
						);

					Bitmap.Init(Extent.X * Extent.Y);
					appMemcpy(&Bitmap(0),&BitmapBytes(0),BitmapBytes.Num());
				}

				// Create screenshot folder if not already present.
				GFileManager->MakeDirectory( *appScreenShotDir(), TRUE );

				const FString ScreenFileName( appScreenShotDir() * TEXT("VisualizeTexture"));

				// Save the contents of the array to a bitmap file.
				appCreateBitmap(*ScreenFileName, Extent.X * GSystemSettings.MaxMultiSamples, Extent.Y, &Bitmap(0), GFileManager);	

				Ar.Logf(TEXT("   content was saved to the folder \"%s\""), *appScreenShotDir());
			}
		}
#endif
		
		if(!ParameterCount || bDumpInvalidOnes)
		{
			// show help
			Ar.Logf(TEXT("VisualizeTexture [all] <TextureId> [<Mode>] [UV0/UV1/UV2] [BMP] [FRAC/SAT]:"));

			Ar.Logf(TEXT("Mode (examples):"));
			Ar.Logf(TEXT("  RGB   = RGB in range 0..1 (default)"));
			Ar.Logf(TEXT("  *8    = RGB * 8"));
			Ar.Logf(TEXT("  A     = alpha channel in range 0..1"));
			Ar.Logf(TEXT("  A*16  = Alpha * 16"));
			Ar.Logf(TEXT("  RGB/2 = RGB / 2"));

			Ar.Logf(TEXT("InputMapping:"));
			Ar.Logf(TEXT("  UV0   = view in left top (default)"));
			Ar.Logf(TEXT("  UV1   = full texture"));
			Ar.Logf(TEXT("  UV2   = view in left top with 1 texel border"));

			Ar.Logf(TEXT("Flags:"));
			Ar.Logf(TEXT("  BMP   = save out bitmap to the screenshots folder (not on console, normalized)"));
			Ar.Logf(TEXT("  FRAC  = use frac() in shader (default)"));
			Ar.Logf(TEXT("  SAT   = use saturate() in shader"));
			Ar.Logf(TEXT("  ALL   = don't hide the invalid ones"));

			Ar.Logf(TEXT("TextureId:"));
			Ar.Logf(TEXT("  0 = <off>"));

			for(UINT i = 0;; ++i)
			{
				FIntPoint Extent;
				FString Name = GSceneRenderTargets.GetRenderTargetInfo((ESceneRenderTargetTypes)i, Extent);

				if(!Name.Len())
				{
					break;
				}

				UBOOL bValid = IsValidRef(GSceneRenderTargets.GetRenderTargetTexture((ESceneRenderTargetTypes)i));

				if(bDumpInvalidOnes || bValid)
				{
					const TCHAR* Valid = bValid ? TEXT("") : TEXT("  <INVALID>");

					if(Extent.X)
					{
						Ar.Logf(TEXT("  %d = %dx%d %s%s"), i + 1, Extent.X, Extent.Y, *Name, Valid);
					}
					else
					{
						// size is unknown
						Ar.Logf(TEXT("  %d = %s%s"), i + 1, *Name, Valid);
					}
				}
			}
		}
		Ar.Logf(TEXT("VisualizeTexture %d"), GVisualizeTexture);
		return TRUE;
	}
#endif // !FINAL_RELEASE
	else if (GStreamingManager && GStreamingManager->Exec(Cmd,Ar))
	{
		// The streaming manager has handled the exec command.
	}
#if _WINDOWS && USE_DYNAMIC_RHI && !USE_NULL_RHI
	else if (ParseCommand(&Cmd,TEXT("TexturePoolSize")))
	{
		extern INT GTexturePoolSize;
		FString Parameter(ParseToken(Cmd, 0));
		INT PoolSize = (Parameter.Len() > 0) ? Max(appAtoi(*Parameter), 0) : -1;
		if ( PoolSize >= 0 )
		{
			GTexturePoolSize = PoolSize * 1024 * 1024;
		}
		if ( GTexturePoolSize > 0 )
		{
			Ar.Logf( TEXT("Texture pool size: %d MB"), GTexturePoolSize/1024/1024 );
		}
		else
		{
			Ar.Logf( TEXT("Texture pool size: UNLIMITED") );
		}
	}
#endif
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	else if (ParseCommand(&Cmd, TEXT("CONTENTCOMPARISON")))
	{
		TArray<FString> Tokens, Switches;
		appParseCommandLine(Cmd, Tokens, Switches);
		if (Tokens.Num() > 0)
		{
			// The first token MUST be the base class name of interest
			FString BaseClassName = Tokens(0);
			TArray<FString> BaseClassesToIgnore;
			INT Depth = 1;
			for (INT TokenIdx = 1; TokenIdx < Tokens.Num(); TokenIdx++)
			{
				FString Token = Tokens(TokenIdx);
				FString TempString;
				if (Parse(*Token, TEXT("DEPTH="), TempString))
				{
					Depth = appAtoi(*TempString);
				}
				else
				{
					BaseClassesToIgnore.AddItem(Token);
					debugf(TEXT("Added ignored base class: %s"), *Token);
				}
			}

			debugf(TEXT("Calling CompareClasses w/ Depth of %d on %s"), Depth, *BaseClassName);
			debugf(TEXT("Ignoring base classes:"));
			for (INT DumpIdx = 0; DumpIdx < BaseClassesToIgnore.Num(); DumpIdx++)
			{
				debugf(TEXT("\t%s"), *(BaseClassesToIgnore(DumpIdx)));
			}
			FContentComparisonHelper ContentComparisonHelper;
			ContentComparisonHelper.CompareClasses(BaseClassName, BaseClassesToIgnore, Depth);
		}
	}
	else if (ParseCommand(&Cmd, TEXT("TOGGLEGTPSYSLOD")))
	{
		extern UBOOL GbEnableGameThreadLODCalculation;
		GbEnableGameThreadLODCalculation = !GbEnableGameThreadLODCalculation;
		warnf(NAME_Log, TEXT("Particle LOD determination is now on the %s thread!"),
			GbEnableGameThreadLODCalculation ? TEXT("GAME") : TEXT("RENDER"));
	}
#endif
#if !FINAL_RELEASE
	else if (ParseCommand(&Cmd, TEXT("TOGGLEHDWARNING")))
	{
		GbWarn_NotRunningInHD = !GbWarn_NotRunningInHD;
		warnf(NAME_Log, TEXT("'NOT RUNNING IN HD!' warning is now %s!"),
			GbWarn_NotRunningInHD ? TEXT("ENABLED") : TEXT("DISABLED"));
	}
#endif
#if WITH_EDITOR
	else if (ParseCommand(&Cmd, TEXT("LOGMOBILEEMUMATERIALS")))
	{
		FMobileEmulationMaterialManager::GetManager()->LogMobileEmulationMaterials();
	}
	else if (ParseCommand(&Cmd, TEXT("KILLMOBILEEMUMATERIALS")))
	{
		FMobileEmulationMaterialManager::GetManager()->UpdateCachedMaterials(TRUE, FALSE);
	}
#endif
	else if( ParseCommand(&Cmd,TEXT("CollapseMICs")) )
	{
		UMaterialInstanceConstant::CollapseMICs(appAtoi(Cmd));
		return TRUE;
	}
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	// Shows kDOPTrees memory usage.
	else if( ParseCommand( &Cmd,TEXT("KDOPMem") ) )
	{
		Ar.Log( TEXT("Static mesh kDOP memory usage") );

		// Lists all static meshes.
		const UBOOL bShowDetailedInfo				= ParseParam( Cmd, TEXT("List") );

		FMemItem TotalkDOPTreeMem;
		TArray<FMemItem> StaticMeshes;

		// Iterate through all Static mesh objects.
		for( FObjectIterator It; It; ++It )
		{
			if( It->IsA( UStaticMesh::StaticClass() ) == FALSE )
			{
				continue;
			}

			UStaticMesh* StaticMesh = Cast<UStaticMesh>( *It );

			FArchiveCountMem MemoryCountAr( NULL );
			StaticMesh->kDOPTree.Nodes.CountBytes( MemoryCountAr );
			StaticMesh->kDOPTree.Triangles.CountBytes( MemoryCountAr );

			// Add to total memory usage.
			TotalkDOPTreeMem.AddRes( MemoryCountAr.GetMax() );

			new(StaticMeshes)FMemItem( StaticMesh, MemoryCountAr.GetMax() );
		}

		const FLOAT OneByMB = 1.0f / 1024.0f / 1024.0f;

#if PS3
#define KDOP_MEM_INFO TEXT("%100ls %6.3f")
#define KDOP_MEM_HEAD TEXT("%100ls %10ls")
#else
#define KDOP_MEM_INFO TEXT("%100s %6.3f")
#define KDOP_MEM_HEAD TEXT("%100s %10s")
#endif

		// Print results.
		if( bShowDetailedInfo == TRUE )
		{
			Ar.Log( TEXT(" Detailed static mesh kDOP memory usage") );
			Ar.Logf( KDOP_MEM_HEAD, TEXT("Static mesh full name"), TEXT("Memory usage MB") );

			// Sort by memory usage.
			Sort<USE_COMPARE_CONSTREF(FMemItem,BaseEngine)>( &StaticMeshes(0), StaticMeshes.Num() );

			for( INT I = 0; I < StaticMeshes.Num(); ++ I )
			{
				FMemItem& MemItem = StaticMeshes(I);
				Ar.Logf( KDOP_MEM_INFO, *MemItem.Object->GetFullName(), OneByMB*MemItem.Res );
			}
		}

		Ar.Log( TEXT("") );
		Ar.Logf( TEXT(" kDOPTree - Total memory usage.") );
		Ar.Logf( TEXT("  Static mesh objects: %3i - %6.3f MB"), TotalkDOPTreeMem.Count, OneByMB*TotalkDOPTreeMem.Res );

#undef KDOP_MEM_INFO
#undef KDOP_MEM_HEAD

		return TRUE;
	}
#endif // !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
#if USE_AI_PROFILER
	else if ( FAIProfiler::Exec( Cmd, Ar ) )
	{
		return TRUE;
	}
#endif
	else if(GConsoleManager->ProcessUserConsoleInput(Cmd, Ar))
	{
		// console variable interaction (get value, set value or get help)
		return TRUE;
	}
#if USE_UNIT_TESTS
	// Handle unit test functionality
	else if ( ParseCommand( &Cmd, TEXT("UNITTEST") ) )
	{
		TMap<FString, FUnitTestExecutionInfo> OutputMap;
		FString TestToRun;

		// Execute test specified by the parameter
		UBOOL bSuccess = Parse( Cmd, TEXT("TEST="), TestToRun );

		// Execute all valid tests
		if ( ParseCommand( &Cmd, TEXT("ALL") ) )
		{
			FUnitTestFramework::GetInstance().RunAllValidTests( OutputMap );
		}
		else if( bSuccess && FUnitTestFramework::GetInstance().ContainsTest( TestToRun ) )
		{
			FUnitTestExecutionInfo& CurOutput = OutputMap.Set( TestToRun, FUnitTestExecutionInfo() );
			FUnitTestFramework::GetInstance().RunTestByName( TestToRun, CurOutput );
		}

		// Dump any output to the log, if any
		FUnitTestFramework::DumpUnitTestExecutionInfoToContext( GWarn, OutputMap );
	}
#endif
	else if ( ExecMemFunctions( Cmd, Ar ) )
	{
		return TRUE;
	}
	else if ( ExecThreadFunctions( Cmd, Ar ) )
	{
		return TRUE;
	}
	else 
	{
		return FALSE;
	}

	return TRUE;
}

//
// Memory related execs grouped together. 
// These functions had to be moved to a separate function as the VS2010 compiler
// couldn't compile them in UEngine::Exec due to 'fatal error C1061: compiler limit : blocks nested too deeply'
//
UBOOL UEngine::ExecMemFunctions( const TCHAR* Cmd, FOutputDevice& Ar )
{	
	const TCHAR* Str=Cmd;

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE	
	if( ParseCommand( &Cmd, TEXT("MemLeakCheck") ) )
	{
		const UBOOL bSkipSlowCommands = ParseParam( Str, TEXT("FAST") );

		//@TODO: Crashes when a texture gets garbage collected during the report generation window
		/*
#if STATS
		// Texture streaming stats are only available when stats are enabled
		const UBOOL bWaitForTextureStreamingStats = ParseParam( Str, TEXT("WAITFORTEXSTREAMING") ) || !bSkipSlowCommands;
		if( bWaitForTextureStreamingStats )
		{
			// This needs to start up now so we can wait for it to finish and the report will be ready
			GStreamingManager->Exec( TEXT("ListStreamingTexturesCollect"), Ar );
			new(GEngine->DeferredCommands) FString( bSkipSlowCommands ? TEXT("MemLeakCheckWaiting -FAST") : TEXT("MemLeakCheckWaiting") );
		}
		else
#endif
		*/
		{
			// This will defer the MemFragCheck to the end of the frame so we can force a GC and get a real frag with no gc'able objects
			new(GEngine->DeferredCommands) FString( bSkipSlowCommands ? TEXT("MemLeakCheckPostGC -FAST") : TEXT("MemLeakCheckPostGC") );
		}

		return TRUE;
	}
	else if( ParseCommand( &Cmd, TEXT("MemLeakCheckPostGC") ) )
	{
		// The MEMLEAK_TIME_THIS_SCOPE macro controls whether each section of a MemLeakCheck should be timed or not
		// It's important to keep at least 'MemLeakCheck -FAST' running in a reasonable amount of time so periodically running it is feasible
#if 0
		// This version is used to time every section.  Any sections that take more than 40 ms are reported, as is the total time
		#define MEMLEAK_TIME_THIS_SCOPE INSERT_SIMPLE_SCOPE_TIMER(0.040)
#else
		// This version disables the timing code
		#define MEMLEAK_TIME_THIS_SCOPE
#endif

		// This macro executes a command with output to the file archiver, then inserts a line terminator
		#define MEMLEAK_EXEC(Command) \
			{ \
				MEMLEAK_TIME_THIS_SCOPE; \
				Exec( TEXT(Command), FileAr ); \
			} \
			FileAr.Logf( LINE_TERMINATOR )

		// Do a MemLeakCheck report
		MEMLEAK_TIME_THIS_SCOPE;

		const UBOOL bPerformSlowCommands = !ParseParam( Str, TEXT("FAST") );

		// turn off as it makes diffing hard
		TGuardValue<UBOOL> DisableLogTimes(GPrintLogTimes, FALSE);

		{
			MEMLEAK_TIME_THIS_SCOPE;
			FlushAsyncLoading();
		}

		{
			MEMLEAK_TIME_THIS_SCOPE;
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS,TRUE);
		}

		{
			MEMLEAK_TIME_THIS_SCOPE;
			FlushRenderingCommands();
		}

		const FString PathName = *(appProfilingDir() + TEXT("MemLeaks") PATH_SEPARATOR );
		GFileManager->MakeDirectory( *PathName );

		const FString Filename = CreateProfileFilename( TEXT(".memlk"), TRUE );
		const FString FilenameFull = PathName + Filename;
		FOutputDeviceFile FileAr(*FilenameFull);

		debugf(TEXT("MemLeakCheckPostGC: saving to %s"), *FilenameFull);

#ifdef ENABLE_MEM_TAGGING
		// for now don't do this and only do SNAPSHOTMEMORY Manually as the snapshots eat up precious mprof size and you will oom when analyzing them
		//Exec(TEXT("MEMTAG_UPDATE"));
#endif	//#ifdef ENABLE_MEM_TAGGING
		
		FileAr.Logf( *FString::Printf( TEXT( "CommandLine Options: %s" ), appCmdLine() ) );

		FileAr.Logf( LINE_TERMINATOR );

		// get our location in the level via the BugIt method
		{
			MEMLEAK_TIME_THIS_SCOPE;

			for( FLocalPlayerIterator It(this); It; ++It )
			{
				FVector ViewLocation(0.0f);
				FRotator ViewRotation(0, 0, 0);
				APlayerController* PC = It->Actor;

				if( PC != NULL )
				{
					PC->eventGetPlayerViewPoint( ViewLocation, ViewRotation );

					FString GoString;
					FString LocString;
					PC->eventBugItStringCreator( ViewLocation, ViewRotation, GoString, LocString );

					FileAr.Logf( *GoString );
					FileAr.Logf( LINE_TERMINATOR );
				}
			}
		}

		if (bPerformSlowCommands)
		{
			MEMLEAK_EXEC( "MEMORYSPLIT ACTUAL" );
		}

		{
#if DO_CHARTING
			MEMLEAK_TIME_THIS_SCOPE;
			FMemoryChartEntry NewMemoryEntry = FMemoryChartEntry();
			NewMemoryEntry.UpdateMemoryChartStats();
			FileAr.Logf( *NewMemoryEntry.GetHeaders() );
			FileAr.Logf( *NewMemoryEntry.ToString() );

			FileAr.Logf( LINE_TERMINATOR );
#endif // DO_CHARTING
		}

		// This command is one of the most expensive ones still done during -FAST (over 1 s)
		MEMLEAK_EXEC( "OBJ LIST -ALPHASORT" );

		if (bPerformSlowCommands)
		{
			MEMLEAK_EXEC( "OBJ LIST class=SkeletalMesh -ALPHASORT -DetailedInfo" );

			MEMLEAK_EXEC( "LISTANIMSETS -ALPHASORT" );

			MEMLEAK_EXEC( "LISTMATINEEANIMSETS -ALPHASORT" );

			MEMLEAK_EXEC( "LISTANIMTREES -ALPHASORT" );

			MEMLEAK_EXEC( "LISTFACEFX -ALPHASORT" );
		}

		MEMLEAK_EXEC( "MEM DETAILED" );

		MEMLEAK_EXEC( "GAMESPECIFIC_MEMORY" );

		MEMLEAK_EXEC( "RENDERTARGET_MEM_USAGE");

		MEMLEAK_EXEC( "GPURESOURCEDUMP" );

		MEMLEAK_EXEC( "DUMPCOMPRESSEDSHADERSTATS" );

		MEMLEAK_EXEC( "DumpTextureStreamingStats" );

		//@TODO: Crashes when a texture gets garbage collected during the report generation window
		/*
#if STATS
		// Texture streaming stat reports are only available when stats are enabled
		MEMLEAK_EXEC( "LISTSTREAMINGTEXTURESREPORT" );
#endif
		*/

		// info about shaders
		{
			MEMLEAK_TIME_THIS_SCOPE;

			INT NumShadersInMemory = 0;
			for( TLinkedList<FShaderType*>::TIterator It(FShaderType::GetTypeList()); It; It.Next() )
			{
				const FShaderType* Type = *It;
				NumShadersInMemory += Type->GetNumShaders();
			}	

			FileAr.Logf( TEXT("NumShadersInMemory: %d"), NumShadersInMemory );

			FileAr.Logf( LINE_TERMINATOR );
		}

		// info about always growing pools
		//  * Name Table
		//	* RB_BodyInstance Pool (c.f. DISABLE_POOLED_RB_INSTANCES)
		//	* RB_ConstraintInstance Pool (c.f. DISABLE_POOLED_RB_INSTANCES)
		//	* Static Mesh Collision Scales
		//	* Static Draw lists on render thread 

		// info about the Octree
		MEMLEAK_EXEC( "ANALYZEOCTREE" );

#if PS3
		MEMLEAK_EXEC( "PS3DUMPGPU2AR" );
#endif	//PS3

		{
			MEMLEAK_TIME_THIS_SCOPE;

			GConfig->ShowMemoryUsage(FileAr);

			FileAr.Logf( LINE_TERMINATOR );
		}


		MEMLEAK_EXEC( "ListPrecacheMapPackages" );

		MEMLEAK_EXEC( "ListLoadedPackages" );

		if ( bPerformSlowCommands )
		{		
			MEMLEAK_EXEC( "LISTTEXTURES -ALPHASORT" );

			// This command is extremely slow (over 30 seconds), due to a linear search for each element
			MEMLEAK_EXEC( "LISTSOUNDS -ALPHASORT" );

			MEMLEAK_EXEC( "LISTAUDIOCOMPONENTS" );

			// need to turn this into a list of exec commands that we can put into an .ini to do ad hoc checks
			MEMLEAK_EXEC( "LISTPARTICLESYSTEMS -ALPHASORT");
		}

		MEMLEAK_EXEC( "LISTSPAWNEDACTORS" );

		MEMLEAK_EXEC( "ListSoundClasses" );

		if (GEngine->GameViewport != NULL)
		{
			MEMLEAK_TIME_THIS_SCOPE;

			GEngine->GameViewport->Exec( TEXT("LOGOUTSTATLEVELS"), FileAr );

			FileAr.Logf( LINE_TERMINATOR );
		}

		if ( bPerformSlowCommands )
		{
			MEMLEAK_EXEC( "LISTGFXRESOURCES" );
		}

		MEMLEAK_EXEC("DUMPPARTICLEMEM");
//		GParticleDataManager.ResetParticleMemoryMaxValues();

		MEMLEAK_EXEC("DUMPGFXMEM");

		FConfigSection* ExecsToRun = GConfig->GetSectionPrivate( TEXT("MemLeakCheckExtraExecsToRun"), FALSE, TRUE, GEngineIni );
		if( (ExecsToRun != NULL) && bPerformSlowCommands )
		{
			MEMLEAK_TIME_THIS_SCOPE;

			for( FConfigSectionMap::TIterator It(*ExecsToRun); It; ++It )
			{
				const FString& ToRun = It.Value();
				FileAr.Logf( *ToRun );
				
				Exec( *ToRun, FileAr );
				FileAr.Logf( LINE_TERMINATOR );
			}
		}

		FileAr.TearDown();

#if !IPHONE
		SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!MEMLEAK:"), *(FilenameFull) );
#endif
		return TRUE;

		#undef MEMLEAK_TIME_THIS_SCOPE
		#undef MEMLEAK_EXEC
	}
	else if( ParseCommand(&Str,TEXT("MEM")) )
	{
		const FString Token = ParseToken( Str, 0 );
		const UBOOL bSummaryOnly = ( Token != TEXT("STAT") );
		const UBOOL bDetailed	 = ( Token == TEXT("DETAILED") );	

#if PS3 && (!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE) && !USE_NULL_RHI
		ENQUEUE_UNIQUE_RENDER_COMMAND(Bah2, 
		{ 
			extern void DEBUG_DumpFixedSizePoolAllocatedSizes();
			DEBUG_DumpFixedSizePoolAllocatedSizes();
		});
#endif	

		FlushAsyncLoading();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS,TRUE);
		FlushRenderingCommands();

		if( !bSummaryOnly || bDetailed )
		{
			GMalloc->DumpAllocations( Ar );
			
			Ar.Logf( TEXT("") );
			Ar.Logf( TEXT("Memory Stats:") );
			Ar.Logf( TEXT("GMainThreadMemStack allocation size [used/ unused] = [%.2f / %.2f] MB"), GMainThreadMemStack.GetByteCount() / (1024.0f * 1024.0f), GMainThreadMemStack.GetUnusedByteCount() / (1024.0f * 1024.0f)  );
			Ar.Logf( TEXT("Nametable memory usage = %.2f MB"), FName::GetNameTableMemorySize() / (1024.0f * 1024.0f) );
#if STATS				
			GStatManager.DumpMemoryStatsForGroup(STATGROUP_Memory, Ar);

			Ar.Logf( TEXT("") );
			Ar.Logf( TEXT("Scene Memory Stats:") );
			GStatManager.DumpMemoryStatsForGroup(STATGROUP_SceneMemory, Ar);
#endif
		}

#if PS3
		appPS3DumpDetailedMemoryInformation( Ar );
#endif

		FMemoryAllocationStats MemStats;

		GMalloc->GetAllocationInfo( MemStats );

		Ar.Logf( TEXT("") );

		if( MemStats.TotalUsed )
		{
			FLOAT AllocatedMB = MemStats.TotalUsed / ( 1024.0f * 1024.0f );
			FLOAT OverheadMB = ( MemStats.TotalAllocated - MemStats.TotalUsed ) / ( 1024.0f * 1024.0f );
			Ar.Logf( NAME_DevMemory, TEXT("Memory allocations reported by the OS: %.2f MB (with %.2f MB waste)"), AllocatedMB, OverheadMB );
		}

		if( MemStats.CPUUsed )
		{
			FLOAT AllocatedMB = MemStats.CPUUsed / ( 1024.0f * 1024.0f );
			FLOAT WasteMB = MemStats.CPUWaste / ( 1024.0f * 1024.0f );
			FLOAT SlackMB = MemStats.CPUSlack / ( 1024.0f * 1024.0f );
			Ar.Logf( NAME_DevMemory, TEXT("Virtual memory tracked in the allocators: %.2f MB (with %.2f MB used, %.2f MB slack and %.2f MB waste)"), AllocatedMB + SlackMB + WasteMB, AllocatedMB, SlackMB, WasteMB );
		}

		if( MemStats.GPUUsed )
		{
			FLOAT AllocatedMB = MemStats.GPUUsed / ( 1024.0f * 1024.0f );
			FLOAT WasteMB = MemStats.GPUWaste / ( 1024.0f * 1024.0f );
			FLOAT SlackMB = MemStats.GPUSlack / ( 1024.0f * 1024.0f );
			Ar.Logf( NAME_DevMemory, TEXT("Physical memory tracked in the allocators: %.2f MB (with %.2f MB used, %.2f MB slack and %.2f MB waste)"), AllocatedMB + SlackMB + WasteMB, AllocatedMB, SlackMB, WasteMB );			
		}

		return TRUE;
	}
#if WITH_GFx
	else if( ParseCommand( &Cmd,TEXT("DUMPGFXMEM") ) )
	{
		UGFxEngine::DumpGFXMemoryStats(Ar);

		return TRUE;
	}
#endif // WITH_GFx
	else if( ParseCommand( &Cmd, TEXT("MEMORYSPLIT") ) )
	{
		// if this is set, this will be closer to what's actually allocated
		UBOOL bShowActualAllocated = (ParseToken(Cmd, 0) == TEXT("ACTUAL"));

		//  Gather various object resource sizes.
		INT		AudioSize 		 		= 0;
		INT		BSPDataSize				= 0;
		INT		LevelDataSize			= 0;
		INT		ParticleActiveSize		= 0;
		INT		ParticlePeakSize		= 0;
		INT		PathingSize				= 0;
		INT		ScriptSize				= 0;
		INT		SkeletalMeshSize		= 0;
		INT		StaticMeshSize			= 0;
		INT     UISize 					= 0;

		// get a list of generic classes to track 
		FConfigSection* ClassesToTrack = GConfig->GetSectionPrivate(TEXT("MemorySplitClassesToTrack"), 0, 1, GEngineIni);
		// get true resource size, in particular, for material, texture
		UBOOL SavedExclusiveMode = GExclusiveResourceSizeMode;
		GExclusiveResourceSizeMode = TRUE;
		// set up a map to store usage, with 0 initial mem used
		TMap<UClass*, INT> MemUsageByClass;
		if (ClassesToTrack)
		{
			for (FConfigSectionMap::TIterator It(*ClassesToTrack); It; ++It)
			{
				// find class by name
				UClass* Class = FindObject<UClass>(ANY_PACKAGE, *It.Value());

				if (Class)
				{
					MemUsageByClass.Set(Class, 0);
				}
			}
		}

		for( TObjectIterator<UObject> It; It; ++It )
		{
			UObject* Object = *It;

			INT ObjectSize = Object->GetResourceSize();
			if( ObjectSize == 0 )
			{
				FArchiveCountMem CountBytesSize( Object );
				// if we want accurate results, count the max (actual amount allocated)
				ObjectSize = bShowActualAllocated ? CountBytesSize.GetMax() : CountBytesSize.GetNum();
			}

			UBOOL bMemoryWasAccountedFor = FALSE;

			// look in the generic class tracking map for the exact class
			INT* MemUsage = MemUsageByClass.Find(Object->GetClass());

			// if not found, look for subclasses with fake IsA
			if (MemUsage == NULL)
			{
				for (TMap<UClass*, INT>::TIterator It(MemUsageByClass); It; ++It)
				{
					if (Object->IsA(It.Key()))
					{
						MemUsage = &It.Value();

						break;
					}
				}
			}

			// if we found the class in the map, update it's memory
			if (MemUsage)
			{
				*MemUsage += ObjectSize;

				bMemoryWasAccountedFor = TRUE;
			}

			// don't count the memory twice if we already counted it and we want
			// to show as accurate results as possible
			if (bMemoryWasAccountedFor && bShowActualAllocated)
			{
				//continue;
			}


			if( Object->IsA(UModel::StaticClass()) 
			|| Object->IsA(UPolys::StaticClass())
			)
			{
				BSPDataSize += ObjectSize;
			}

			// handle objects that may be counted in two sections
			if(	Object->IsA(UModelComponent::StaticClass())
			||	Object->IsA(UBrushComponent::StaticClass())
			||	Object->IsA(ULevel::StaticClass())
			)
			{
				LevelDataSize += ObjectSize;
			}


			// Audio Size
			if( Object->IsA(UAudioComponent::StaticClass())
				||	Object->IsA(UAudioDevice::StaticClass())
				||	Object->IsA(USoundNodeWave::StaticClass()) 
				)
			{
				AudioSize += ObjectSize;
			}


			// Pathing Size
			if( Object->IsA(UReachSpec::StaticClass())
				||	Object->IsA(UNavigationMeshBase::StaticClass())
				||	Object->IsA(APathNode::StaticClass()) 
				)
			{
				PathingSize += ObjectSize;
			}


			// Script Size 
			if( Object->IsA(UArrayProperty::StaticClass())
				||	Object->IsA(UBoolProperty::StaticClass())
				||	Object->IsA(UClass::StaticClass())
				||	Object->IsA(UClassProperty::StaticClass())
				||	Object->IsA(UComponentProperty::StaticClass())
				||	Object->IsA(UConst::StaticClass())
				||	Object->IsA(UDelegateProperty::StaticClass())
				||	Object->IsA(UEnum::StaticClass())
				||	Object->IsA(UField::StaticClass())
				||	Object->IsA(UFloatProperty::StaticClass())
				||	Object->IsA(UFunction::StaticClass())
				||	Object->IsA(UIntProperty::StaticClass())
				||	Object->IsA(UInterfaceProperty::StaticClass())
				||	Object->IsA(UMapProperty::StaticClass())
				||	Object->IsA(UNameProperty::StaticClass())
				||	Object->IsA(UObjectProperty::StaticClass())
				||	Object->IsA(UScriptStruct::StaticClass())
				||	Object->IsA(UStrProperty::StaticClass())
				||	Object->IsA(UStructProperty::StaticClass())
				)
			{
				ScriptSize += ObjectSize;
			}


			// Skeletal Size
			if( Object->IsA(UMorphTarget::StaticClass())
				||	Object->IsA(URB_BodyInstance::StaticClass())
				||	Object->IsA(URB_BodySetup::StaticClass()) 
				||	Object->IsA(USkeletalMesh::StaticClass())
				||	Object->IsA(USkeletalMeshComponent::StaticClass()) 
				)
			{
				SkeletalMeshSize += ObjectSize;
			}


			// Static Mesh Size
			if( Object->IsA(UStaticMesh::StaticClass())
				||	Object->IsA(AStaticMeshActor::StaticClass())
				||	Object->IsA(AStaticMeshCollectionActor::StaticClass())
				||	Object->IsA(UStaticMeshComponent::StaticClass()) 
				)
			{
				StaticMeshSize += ObjectSize;
			}

			
			// UISize Size
			if( Object->IsA(UFont::StaticClass())
				||	Object->IsA(UUIRoot::StaticClass())
				)
			{
				UISize += ObjectSize;
			}

			if( Object->IsA(UParticleSystemComponent::StaticClass()) )
			{
				FArchiveCountMem CountBytesSize( Object);
				ParticlePeakSize += CountBytesSize.GetMax();
				ParticleActiveSize += CountBytesSize.GetNum();

				UParticleSystemComponent* PSysComp = Cast<UParticleSystemComponent>(Object);
				ParticlePeakSize += PSysComp->GetResourceSize();
				ParticleActiveSize += PSysComp->GetResourceSize();
			}
		}

		GExclusiveResourceSizeMode = SavedExclusiveMode;

		// Gather audio stats.
		INT		SoundDataSize			= 0;

		// Common audio pool
		INT CommonAudioPoolSize = 0;
		INT CommonAudioPoolBytesFree = 0;
		if (GEngine->Client && GEngine->Client->GetAudioDevice())
		{
			CommonAudioPoolSize = GEngine->Client->GetAudioDevice()->CommonAudioPoolSize;
			CommonAudioPoolBytesFree = GEngine->Client->GetAudioDevice()->CommonAudioPoolFreeBytes;
		}

		// Gather texture memory stats.
		INT		TexturePoolAllocSize	= 0;
		INT		TexturePoolAvailSize	= 0;
		INT		TexturePendingMemoryAdjustment		= 0;
		RHIGetTextureMemoryStats( TexturePoolAllocSize, TexturePoolAvailSize, TexturePendingMemoryAdjustment );

#if USE_LS_SPEC_FOR_UNICODE
	#define DUMP_MEMORY_SIZE(Name, Amt)		Ar.Logf(TEXT("  %32ls: %6i"), Name, Amt / 1024)
#else
	#define DUMP_MEMORY_SIZE(Name, Amt)		Ar.Logf(TEXT("  %32s: %6i"), Name, Amt / 1024)
#endif
		Ar.Logf( TEXT("Memory split (in KByte)") );
		DUMP_MEMORY_SIZE(TEXT("Audio"),					AudioSize);
		DUMP_MEMORY_SIZE(TEXT("BSP"),					BSPDataSize);
		DUMP_MEMORY_SIZE(TEXT("Level"),					LevelDataSize);
		DUMP_MEMORY_SIZE(TEXT("Particles (Active)"),	ParticleActiveSize);
		DUMP_MEMORY_SIZE(TEXT("Particles (Peak)"),		ParticlePeakSize);
		DUMP_MEMORY_SIZE(TEXT("Pathing"),				PathingSize);
		DUMP_MEMORY_SIZE(TEXT("Script"),				ScriptSize);
		DUMP_MEMORY_SIZE(TEXT("SkeletalMesh"),			SkeletalMeshSize);
		DUMP_MEMORY_SIZE(TEXT("StaticMesh"),			StaticMeshSize);
		DUMP_MEMORY_SIZE(TEXT("UI"),					UISize);

		Ar.Logf( TEXT("  ") );

		for (TMap<UClass*, INT>::TIterator It(MemUsageByClass); It; ++It)
		{
			DUMP_MEMORY_SIZE(*It.Key()->GetName(), It.Value());
		}
		// okie let's get the other big memory uses here
		FOutputDeviceNull NullAr;

		//// RenderTargets
		FlushRenderingCommands();
		const INT RenderTargetMemUsage = GSceneRenderTargets.DumpMemoryUsage(NullAr);
		DUMP_MEMORY_SIZE(TEXT("RenderTargets"),			RenderTargetMemUsage);

		//// FConfig Caches
		const SIZE_T FConfigMaxMem = GConfig->GetMaxMemoryUsage();
		DUMP_MEMORY_SIZE(TEXT("FConfigCache"),			FConfigMaxMem);

		//// Texture pool
		DUMP_MEMORY_SIZE(TEXT("TexturePool"), (TexturePoolAllocSize + TexturePoolAvailSize));

		/// Common audio pool
		DUMP_MEMORY_SIZE(TEXT("CommonAudioPool"), CommonAudioPoolSize);

		return TRUE;
	}
	else if( ParseCommand( &Cmd, TEXT("ListPrecacheMapPackages") ) )
	{
		TArray<FString> Packages;
		ULinkerLoad::GetListOfPackagesInPackagePrecacheMap( Packages );

		Sort<USE_COMPARE_CONSTREF(FString,UnEngine)>( &Packages(0), Packages.Num() );

		Ar.Logf( TEXT( "Total Number Of Packages In PrecacheMap: %i " ), Packages.Num() );

		for( INT i = 0; i < Packages.Num(); ++i )
		{
			Ar.Logf( TEXT( "%i %s" ), i, *Packages(i) );
		}
		Ar.Logf( TEXT( "Total Number Of Packages In PrecacheMap: %i " ), Packages.Num() );

		return TRUE;

	}
	// we can't always do an obj linkers, as cooked games have their linkers tossed out.  So we need to look at the actual packages which are loaded
	else if( ParseCommand( &Cmd, TEXT("ListLoadedPackages") ) )
	{
		TArray<FString> Packages;

		for( TObjectIterator<UPackage> It; It; ++It )
		{
			UPackage* Package = *It;

			const UBOOL bIsARootPackage = Package->GetOuter() == NULL;

			if( bIsARootPackage == TRUE )
			{
				Packages.AddItem( Package->GetFullName() );
				//warnf( TEXT("Package %s"), *Package->GetFullName() );
			}
		}

		Sort<USE_COMPARE_CONSTREF(FString,UnEngine)>( &Packages(0), Packages.Num() );

		Ar.Logf( TEXT( "Total Number Of Packages Loaded: %i " ), Packages.Num() );

		for( INT i = 0; i < Packages.Num(); ++i )
		{
			Ar.Logf( TEXT( "%4i %s" ), i, *Packages(i) );
		}
		Ar.Logf( TEXT( "Total Number Of Packages Loaded: %i " ), Packages.Num() );

		return TRUE;
	}
#endif // !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
#if !FINAL_RELEASE
	else if( ParseCommand(&Cmd,TEXT("RENDERTARGET_MEM_USAGE")) )
	{
		FlushRenderingCommands();
		GSceneRenderTargets.DumpMemoryUsage( Ar );
		return TRUE;
	}
#if WITH_GFx
	else if( ParseCommand( &Cmd,TEXT("LISTGFXRESOURCES") ) )
	{
		Ar.Logf( TEXT("Listing referenced GFx resources") );
		for( TObjectIterator<USwfMovie> It; It; ++It )
		{
			UINT TotalResourceSize = 0;
			USwfMovie* SwfMovie = *It;
			Ar.Logf( TEXT("%s(%s)"), *SwfMovie->GetClass()->GetName(), *SwfMovie->GetName() );
			Ar.Logf( TEXT("%d References"), SwfMovie->References.Num() );
			Ar.Logf( TEXT("\tClass\tName\tSize") );
			for( INT RefIndex = 0; RefIndex < SwfMovie->References.Num(); ++RefIndex )
			{
				UObject* Reference = SwfMovie->References( RefIndex );
				if( Reference )
				{
					const UINT ResourceSize = Reference->GetResourceSize();
					TotalResourceSize += ResourceSize;
					Ar.Logf( TEXT("\t%s\t%s\t%0.2f KB"), *Reference->GetClass()->GetName(), *Reference->GetName(), ResourceSize / 1024.0f);
				}
			}

			Ar.Logf( TEXT("%d User References"), SwfMovie->UserReferences.Num() );
			Ar.Logf( TEXT("\tClass\tName\tSize") );
			for( INT RefIndex = 0; RefIndex < SwfMovie->UserReferences.Num(); ++RefIndex )
			{
				UObject* Reference = SwfMovie->UserReferences( RefIndex );
				if( Reference )
				{
					const UINT ResourceSize = Reference->GetResourceSize();
					TotalResourceSize += ResourceSize;
					Ar.Logf( TEXT("\t%s\t%s\t%0.2f KB"), *Reference->GetClass()->GetName(), *Reference->GetName(), ResourceSize / 1024.0f);
				}
			}
			Ar.Logf( TEXT("Total Resource Size: %0.2f KB"), TotalResourceSize / 1024.0f );
		}

		return TRUE;
	}
#endif // WITH_GFx
	else if( ParseCommand(&Cmd,TEXT("TVisualizeSkeletalMeshTickOptimization")) )
	{
		extern UBOOL GVisualizeSkeletalMeshTickOptimization;
		GVisualizeSkeletalMeshTickOptimization = !GVisualizeSkeletalMeshTickOptimization;

		// Restore default Dynamic Light Environment.
		if( GVisualizeSkeletalMeshTickOptimization == FALSE )
		{
			for( TObjectIterator<USkeletalMeshComponent> It; It; ++It )
			{
				USkeletalMeshComponent* SkelMeshComp = *It;

				if( SkelMeshComp->bUseTickOptimization == TRUE )
				{
					UDynamicLightEnvironmentComponent* DynLightEnv = Cast<UDynamicLightEnvironmentComponent>( SkelMeshComp->LightEnvironment );
					if( DynLightEnv != NULL )
					{
						UDynamicLightEnvironmentComponent* DefaultLightEnv = Cast<UDynamicLightEnvironmentComponent>( DynLightEnv->GetClass()->GetDefaultObject() );
						DynLightEnv->AmbientGlow = DefaultLightEnv->AmbientGlow;

						DynLightEnv->ResetEnvironment();
					}
				}
			}
		}

		Ar.Logf( TEXT( "Visualization for skeletal mesh tick optimization is %s" ), GVisualizeSkeletalMeshTickOptimization == TRUE ? TEXT( "enabled" ) : TEXT( "disabled" ) );
		return TRUE;
	}
#endif // !FINAL_RELEASE

	return FALSE;
}

//
// Threading related execs grouped together. 
//
// These functions had to be moved to a separate function as the VS2010 compiler couldn't compile
// them in UEngine::Exec due to 'fatal error C1061: compiler limit : blocks nested too deeply'
//
UBOOL UEngine::ExecThreadFunctions( const TCHAR* Cmd, FOutputDevice& Ar )
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	if (ParseCommand(&Cmd, TEXT("THREAD")))
	{
		if (ParseCommand(&Cmd,TEXT("SLEEP")))
		{
			GSystemSettings.bAllowPerFrameSleep = !GSystemSettings.bAllowPerFrameSleep;
			Ar.Logf(TEXT("Thread: sleep now %s"), GSystemSettings.bAllowPerFrameSleep ? TEXT("ENABLED") : TEXT("DISABLED"));
		}
		else if (ParseCommand(&Cmd,TEXT("YIELD")))
		{
			GSystemSettings.bAllowPerFrameYield = !GSystemSettings.bAllowPerFrameYield;
			Ar.Logf(TEXT("Thread: yield now %s"), GSystemSettings.bAllowPerFrameYield ? TEXT("ENABLED") : TEXT("DISABLED"));
		}
	}
#endif

	return FALSE;
}

/**
 * Computes a color to use for property coloration for the given object.
 *
 * @param	Object		The object for which to compute a property color.
 * @param	OutColor	[out] The returned color.
 * @return				TRUE if a color was successfully set on OutColor, FALSE otherwise.
 */
UBOOL UEngine::GetPropertyColorationColor(UObject* Object, FColor& OutColor)
{
	return FALSE;
}

/** Uses StatColorMappings to find a color for this stat's value. */
UBOOL UEngine::GetStatValueColoration(const FString& StatName, FLOAT Value, FColor& OutColor)
{
	for(INT i=0; i<StatColorMappings.Num(); i++)
	{
		const FStatColorMapping& Mapping = StatColorMappings(i);
		if(StatName == Mapping.StatName)
		{
			const INT NumPoints = Mapping.ColorMap.Num();

			// If no point in curve, return the Default value we passed in.
			if( NumPoints == 0 )
			{
				return FALSE;
			}

			// If only one point, or before the first point in the curve, return the first points value.
			if( NumPoints < 2 || (Value <= Mapping.ColorMap(0).In) )
			{
				OutColor = Mapping.ColorMap(0).Out;
				return TRUE;
			}

			// If beyond the last point in the curve, return its value.
			if( Value >= Mapping.ColorMap(NumPoints-1).In )
			{
				OutColor = Mapping.ColorMap(NumPoints-1).Out;
				return TRUE;
			}

			// Somewhere with curve range - linear search to find value.
			for( INT i=1; i<NumPoints; i++ )
			{	
				if( Value < Mapping.ColorMap(i).In )
				{
					if (Mapping.DisableBlend)
					{
						OutColor = Mapping.ColorMap(i).Out;
					}
					else
					{
						const FLOAT Diff = Mapping.ColorMap(i).In - Mapping.ColorMap(i-1).In;
						const FLOAT Alpha = (Value - Mapping.ColorMap(i-1).In) / Diff;

						FLinearColor A(Mapping.ColorMap(i-1).Out);
						FVector AV(A.R, A.G, A.B);

						FLinearColor B(Mapping.ColorMap(i).Out);
						FVector BV(B.R, B.G, B.B);

						FVector OutColorV = Lerp( AV, BV, Alpha );
						OutColor = FLinearColor(OutColorV.X, OutColorV.Y, OutColorV.Z);
					}

					return TRUE;
				}
			}

			OutColor = Mapping.ColorMap(NumPoints-1).Out;
			return TRUE;
		}
	}

	// No entry for this stat name
	return FALSE;
}

void UEngine::OnLostFocusPause(UBOOL EnablePause)
{
	if( bPauseOnLossOfFocus )
	{
		// Iterate over all players and pause / unpause them
		// Note: pausing / unpausing the player is done via their HUD pausing / unpausing
		for(INT PlayerIndex = 0;PlayerIndex < GamePlayers.Num();PlayerIndex++)
		{
			ULocalPlayer* Player = GamePlayers(PlayerIndex);
			if(Player && Player->Actor && Player->Actor->myHUD)
			{
				Player->Actor->myHUD->eventOnLostFocusPause(EnablePause);
			}
		}
	}
}

/** Get tick rate limitor. */
FLOAT UEngine::GetMaxTickRate( FLOAT DeltaTime, UBOOL bAllowFrameRateSmoothing )
{
	FLOAT MaxTickRate = 0;

#if !CONSOLE || MOBILE
	// Smooth the framerate if wanted. The code uses a simplistic running average. Other approaches, like reserving
	// a percentage of time, ended up creating negative feedback loops in conjunction with GPU load and were abandonend.
	if( bSmoothFrameRate && bAllowFrameRateSmoothing && GIsClient )
	{
		if( DeltaTime < 0.0f )
		{
#if SHIPPING_PC_GAME
			// End users don't have access to the secure parts of UDN. The localized string points to the release notes,
			// which should include a link to the AMD CPU drivers download site.
			appErrorf( NAME_FriendlyError, *LocalizeError("NegativeDeltaTime",TEXT("Engine")) );
#else
			// Send developers to the support list thread.
			appErrorf(TEXT("Negative delta time! Please see https://udn.epicgames.com/lists/showpost.php?list=ue3bugs&id=4364"));
#endif
		}

		// Running average delta time, initial value at 100 FPS so fast machines don't have to creep up
		// to a good frame rate due to code limiting upward "mobility".
		static FLOAT RunningAverageDeltaTime = 1 / 100.f;

		// Keep track of running average over 300 frames, clamping at min of 5 FPS for individual delta times.
		RunningAverageDeltaTime = Lerp<FLOAT>( RunningAverageDeltaTime, Min<FLOAT>( DeltaTime, 0.2f ), 1 / 300.f );

		// Work in FPS domain as that is what the function will return.
		FLOAT AverageFPS = 1.f / RunningAverageDeltaTime;

		// Clamp FPS into ini defined min/ max range.
		MaxTickRate = Clamp<FLOAT>( AverageFPS, MinSmoothedFrameRate, MaxSmoothedFrameRate );
	}
#endif

	return MaxTickRate;
}

/**
 * Updates all physics constraint actor joint locations.
 */
void UEngine::UpdateConstraintActors()
{
	if( bAreConstraintsDirty )
	{
		for( FActorIterator It; It; ++It )
		{
			ARB_ConstraintActor* ConstraintActor = Cast<ARB_ConstraintActor>( *It );
			if( ConstraintActor )
			{
				ConstraintActor->UpdateConstraintFramesFromActor();
			}
		}
		bAreConstraintsDirty = FALSE;
	}
}

/**
 * Enables or disables the ScreenSaver (PC only)
 *
 * @param bEnable	If TRUE the enable the screen saver, if FALSE disable it.
 */
void UEngine::EnableScreenSaver( UBOOL bEnable )
{
#if !CONSOLE && !DEDICATED_SERVER
	if( !ScreenSaverInhibitor )
	{
		// Create thread inhibiting screen saver while it is running.
		FScreenSaverInhibitor* ScreenSaverInhibitorRunnable = new FScreenSaverInhibitor();
		ScreenSaverInhibitor = GThreadFactory->CreateThread( ScreenSaverInhibitorRunnable, TEXT("ScreenSaverInhibitor"), TRUE, TRUE );
		// Only actually run when needed to not bypass group policies for screensaver, etc.
		ScreenSaverInhibitor->Suspend( TRUE );
		ScreenSaverInhibitorSemaphore = 0;
	}

	if( bEnable && ScreenSaverInhibitorSemaphore > 0)
	{
		if( --ScreenSaverInhibitorSemaphore == 0 )
		{	
			// If the semaphore is zero and we are enabling the screensaver
			// the thread preventing the screen saver should be suspended
			ScreenSaverInhibitor->Suspend( TRUE );
		}
	}
	else if( !bEnable )
	{
		if( ++ScreenSaverInhibitorSemaphore == 1 )
		{
			// If the semaphore is just becoming one, the thread 
			// is was not running so enable it.
			ScreenSaverInhibitor->Suspend( FALSE );
		}
	}
#endif

}

/**
 * Trigger a screensot at the end of next frame
 */ 
void UEngine::SetRequestScreenshot()
{
	GEngine->bScreenshotRequested = TRUE;	
}

/**
 * Starts playing a loading movie from memory.
 * Keeps track of the total time fullscreen movies are playing to cover loading.
 *
 * @param MovieName		Name of the movie to play in its entirety
 */
void UEngine::PlayLoadingMovie( const TCHAR* MovieName )
{
	if ( GFullScreenMovie )
	{
		// If we aren't already tracking the loading movie time
		if ( appIsNearlyZero(LoadingMovieStartTime) )
		{
			// Start measuring the amount of time a loading movie is up on screen.
			LoadingMovieStartTime = appSeconds();

			// Log that we started a loading movie, so it's easy to see what's being done while it's playing.
			debugf( TEXT("--- LOADING MOVIE START ---") );
		}

		GFullScreenMovie->GameThreadPlayMovie( MM_LoopFromMemory, MovieName );
	}
}

/**
 * Gets the value of the specified boolean System Setting
 * 
 * @param SettingName - Name of the boolean System Setting to retrieve
 * 
 * @return The value of the boolean System Setting
 */
UBOOL UEngine::GetSystemSettingBool(const FString& SettingName)
{
	FString BoolSetting = SettingName;
	FSystemSetting* Setting = GSystemSettings.FindSystemSetting(BoolSetting, SST_BOOL);

	if(Setting != NULL && Setting->SettingAddress != NULL)
	{
		return *(UBOOL*)Setting->SettingAddress ? TRUE : FALSE;
	}

	return FALSE;
}

/**
 * Gets the value of the specified integer System Setting
 * 
 * @param SettingName - Name of the integer System Setting to retrieve
 * 
 * @return The value of the integer System Setting
 */
INT UEngine::GetSystemSettingInt(const FString& SettingName)
{
	FString IntSetting = SettingName;
	FSystemSetting* Setting = GSystemSettings.FindSystemSetting(IntSetting, SST_INT);

	if(Setting != NULL && Setting->SettingAddress != NULL)
	{
		return *(INT*)Setting->SettingAddress;
	}

	return -1;
}

/**
 * Gets the value of the specified float System Setting
 * 
 * @param SettingName - Name of the float System Setting to retrieve
 * 
 * @return The value of the float System Setting
 */
FLOAT UEngine::GetSystemSettingFloat(const FString& SettingName)
{
	FString FloatSetting = SettingName;
	FSystemSetting* Setting = GSystemSettings.FindSystemSetting(FloatSetting, SST_FLOAT);

	if(Setting != NULL && Setting->SettingAddress != NULL)
	{
		return *(FLOAT*)Setting->SettingAddress;
	}

	return -1.0;
}

/**
 * Returns the default post process chain (loading it if it has not yet been loaded)
 */
UPostProcessChain* UEngine::GetDefaultPostProcessChain()
{
    if( DefaultPostProcess == NULL && DefaultPostProcessName.Len() )
    {
		DefaultPostProcess = LoadObject<UPostProcessChain>(NULL,*DefaultPostProcessName,NULL,LOAD_None,NULL);
	}
	return DefaultPostProcess; 
}

/**
 * Returns the post process chain to be used with the world. 
 */
UPostProcessChain* UEngine::GetWorldPostProcessChain()
{
	UPostProcessChain* PostProcessChain = NULL;
	if( GWorld )
	{
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		check(WorldInfo);
		PostProcessChain = WorldInfo->WorldPostProcessChain;
	}

	if( PostProcessChain == NULL )
	{
		// If a post process chain has not been found, GWorld is null or WorldInfo doesnt have one set up.
		// Load the default engine post process chain used for the game and main editor view if its not already loaded
		if( DefaultPostProcess == NULL && DefaultPostProcessName.Len() )
		{
			DefaultPostProcess = LoadObject<UPostProcessChain>(NULL,*DefaultPostProcessName,NULL,LOAD_None,NULL);
		}
		PostProcessChain = GetDefaultPostProcessChain();
	}

	return PostProcessChain;
}

/**
 * Queue up view "slave" locations to the streaming system. These locations will be added properly at the next call to AddViewInformation,
 * re-using the screensize and FOV settings.
 *
 * @param SlaveLocation			World-space view origin
 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other locations
 * @param OverrideDuration		How long the streaming system should keep checking this location if bOverrideLocation is TRUE, in seconds. 0 means just for the next Tick.
 */
void UEngine::AddTextureStreamingSlaveLoc(FVector InLoc, FLOAT BoostFactor, UBOOL bOverrideLocation, FLOAT OverrideDuration)
{
	GStreamingManager->AddViewSlaveLocation(InLoc, BoostFactor, bOverrideLocation, OverrideDuration);
}

/** Header to determine if an archive is encrypted or not. **/
static UINT EncryptedMagic = 0xC0DEDBAD;

/**
 * Serializes an object to a file (object pointers to non-always loaded objects are not supported)
 *
 * @param Obj The object to serialize
 * @param Pathname The path to the file to save
 * @param bIsSaveGame If TRUE, FILEREAD_SaveGame will be used to create the file reader
 * 
 * @return TRUE if successful
 */
UBOOL UEngine::BasicSaveObject(UObject* Obj, const FString& PathName, UBOOL bIsSaveGame, INT Version, UBOOL bEncrypt)
{
	checkf(EncryptedMagic != (UINT&)Version, TEXT("We can't save a version that matches the EncryptedMagic"));

	// make sure we can write to the file
	FArchive* PersistentFile = GFileManager->CreateFileWriter(*PathName, bIsSaveGame ? FILEWRITE_SaveGame : 0);
	if (PersistentFile)
	{
		// the memory to write to
		TArray<BYTE> DataBuffer;

		// memory archive to write to
		FMemoryWriter DataArchive(DataBuffer, TRUE);
		DataArchive.SetIsSaveGame(bIsSaveGame);

		// if we need to encrypt the data write out some magic to know it's encrypted on load
		if (bEncrypt)
		{
			// save this directly to file, not in the data buffer (it can't be encrypted)
			*PersistentFile << EncryptedMagic;
		}

		// save out a version
		DataArchive << Version;

		// use a wrapper archive that converts FNames and UObject*'s to strings that can be read back in
		FObjectAndNameAsStringProxyArchive Ar(DataArchive);

		// serialize the object
		Obj->Serialize(Ar);

		if (bEncrypt)
		{	
			// must pad to multiple of 16 bytes
			DataBuffer.AddZeroed(Align(DataBuffer.Num(), 16) - DataBuffer.Num());

			// encrypt the data in place
			appEncryptData(DataBuffer.GetTypedData(), DataBuffer.Num());
		}

		// now write out the memory to disk
		PersistentFile->Serialize(DataBuffer.GetData(), DataBuffer.Num());

		// close the file
		delete PersistentFile;
		return TRUE;
	}

	return FALSE;
}

/**
 * Loads an object from a file (saved with the SaveObject function). It should already be
 * allocated just like the original object was allocated
 *
 * @param Obj The object to serialize
 * @param Pathname The path to the file to read and create the object from
 * @param bIsSaveGame If TRUE, FILEREAD_SaveGame will be used to create the file reader
 * 
 * @return TRUE if successful
 */
UBOOL UEngine::BasicLoadObject(UObject* Obj, const FString& PathName, UBOOL bIsSaveGame, INT Version)
{
	// read the file into a memory buffer
	TArray<BYTE> DataBuffer;
	// @todo ib2merge: Chair used this logic for PathName, but ONLY for BasicLoadObject, not Save: bIsSaveGame ? *PathName : *(appGameDir() + PathName)
	if (appLoadFileToArray(DataBuffer, *PathName, GFileManager, bIsSaveGame ? FILEREAD_SaveGame : 0))
	{
		// read the file into a memory buffer
		FMemoryReader DataArchive(DataBuffer, TRUE);
		DataArchive.SetIsSaveGame(bIsSaveGame);

		// read the beginning - either magic or the version number
		UINT MagicOrVersion;
		DataArchive << MagicOrVersion;

		INT SavedVersion = 0;
		if (MagicOrVersion == EncryptedMagic)
		{
			// pad the part following the header to 16 bytes
			UINT NonHeaderSize = DataBuffer.Num() - DataArchive.Tell();
			DataBuffer.AddZeroed(Align(NonHeaderSize, 16) - NonHeaderSize);

			// decrpt the data after the header
			appDecryptData(&DataBuffer(DataArchive.Tell()), DataBuffer.Num() - DataArchive.Tell());

			// now we can read out of the buffer
			DataArchive << SavedVersion;
		}
		else
		{
			// if it wasn't encrypted, then this was actually the version number
			SavedVersion = (INT&)MagicOrVersion;
		}

		if (SavedVersion != Version)
		{
			// note that it failed to read
			return FALSE;
		}

		// use a wrapper archive that converts FNames and UObject*'s to strings that can be read back in
		FObjectAndNameAsStringProxyArchive Ar(DataArchive);

		// serialize the object out of the part of the buffer following the version number
		Obj->Serialize(Ar);

		return TRUE;
	}

	return FALSE;
}

/** Launches the specified platform via appLaunchURL (platform dependent support) */
void UEngine::LaunchURL(const FString& URL)
{
	appLaunchURL(*URL);
}


/*-----------------------------------------------------------------------------
	UServerCommandlet.
-----------------------------------------------------------------------------*/

INT UServerCommandlet::Main( const FString& Params )
{
	GIsRunning = 1;
	GIsRequestingExit = FALSE;

#if !CONSOLE && !__GNUC__// Windows only
	/**
	 * Used by the .com wrapper to notify that the Ctrl-C handler was triggered.
	 * This shared event is checked each tick so that the log file can be
	 * cleanly flushed
	 */
	FEvent* ComWrapperShutdownEvent = GSynchronizeFactory->CreateSynchEvent(TRUE,TEXT("ComWrapperShutdown"));
#endif

#if DEDICATED_SERVER
	GEngine->bUseSound = FALSE;

	appPerfCountersInit();
	GForceEnableScopedCycleStats++;
#endif

#if !SHIPPING_PC_GAME && WITH_UE3_NETWORKING 
	if (GDebugChannel)
	{
		GDebugChannel->Init();
	}
#endif 

	// Main loop.
	while( GIsRunning && !GIsRequestingExit )
	{
#if STATS
		GFPSCounter.Update(appSeconds());
#endif
		// Calculates average FPS/MS (outside STATS on purpose)
		CalculateFPSTimings();

		// Update GCurrentTime/ GDeltaTime while taking into account max tick rate.
 		extern void appUpdateTimeAndHandleMaxTickRate();
		appUpdateTimeAndHandleMaxTickRate();

		// Tick the engine.
		GEngine->Tick( GDeltaTime );
	
#if DEDICATED_SERVER
		appPerfCountersUpdate();
#endif
#if STATS
		// Write all stats for this frame out
		GStatManager.AdvanceFrame();
#endif

#if WITH_UE3_NETWORKING && !SHIPPING_PC_GAME
		if ( GDebugChannel )
		{
			GDebugChannel->Tick();
		}
#endif	//#if WITH_UE3_NETWORKING

		// Execute deferred commands.
		for( INT DeferredCommandsIndex=0; DeferredCommandsIndex<GEngine->DeferredCommands.Num(); DeferredCommandsIndex++ )
		{
			// Use LocalPlayer if available...
			if( GEngine->GamePlayers.Num() && GEngine->GamePlayers(0) )
			{
				ULocalPlayer* Player = GEngine->GamePlayers(0);
				{
					Player->Exec( *GEngine->DeferredCommands(DeferredCommandsIndex), *GLog );
				}
			}
			// and fall back to UEngine otherwise.
			else
			{
				GEngine->Exec( *GEngine->DeferredCommands(DeferredCommandsIndex), *GLog );
			}

		}
		GEngine->DeferredCommands.Empty();


#if !CONSOLE && !__GNUC__
		// See if the Ctrl-C handler of the .com wrapper asked us to exit
		if (ComWrapperShutdownEvent->Wait(0) == TRUE)
		{
			// inform GameInfo that we're exiting
			if (GWorld != NULL && GWorld->GetWorldInfo() != NULL && GWorld->GetWorldInfo()->Game != NULL)
			{
				GWorld->GetWorldInfo()->Game->eventGameEnding();
			}
			// It wants us to exit
			GIsRequestingExit = TRUE;
		}
#endif
	}

#if DEDICATED_SERVER
	//Cleanup windows performance counters
	GForceEnableScopedCycleStats--;
	appPerfCountersCleanup();
#endif

#if !CONSOLE && !__GNUC__
	// Create an event that is shared across apps and is manual reset
	GSynchronizeFactory->Destroy(ComWrapperShutdownEvent);
#endif

	GIsRunning = 0;
	return 0;
}

IMPLEMENT_CLASS(UServerCommandlet)


INT SmokeTest_CheckNativeClassSizes( const TCHAR* Parms );
INT SmokeTest_RunServer( const TCHAR* Parms );

/**
 * smoke test to check CheckNativeClassSizes  this doesn't work just yet
 *
 *
 * Note:  probably need to move the init() call to below and then set our IsFoo=1 for the type of 
 * test we want
 *
 */
INT USmokeTestCommandlet::Main( const FString& Params )
{
	const TCHAR* Parms = *Params;

	INT Retval = 0;

	// put various smoke test testing code in here before moving off to their
	// own commandlet

	if( ParseParam(appCmdLine(),TEXT("SERVER")) == TRUE )
	{
		Retval = SmokeTest_RunServer( Parms );
	}
	// other tests we want are to launch the editor and then quit
	// launch the game and then quit
	else
	{
		// exit with no error
		Retval = 0;
	}

	// Check native class sizes
	Retval = SmokeTest_CheckNativeClassSizes( Parms );

	// test vector intrinsics
	RunVectorRegisterAbstractionTest();

	GIsRequestingExit = TRUE; // so CTRL-C will exit immediately


	return Retval;
}
IMPLEMENT_CLASS(USmokeTestCommandlet)

/**
 * Run the server for one tick and then quit
 */
INT SmokeTest_RunServer( const TCHAR* Parms )
{
	INT Retval = 0;

	GIsRunning = 1;

	// Main loop.
	while( GIsRunning && !GIsRequestingExit )
	{
		// Update GCurrentTime/ GDeltaTime while taking into account max tick rate.
 		extern void appUpdateTimeAndHandleMaxTickRate();
		appUpdateTimeAndHandleMaxTickRate();
		
		// Tick the engine.
		GEngine->Tick( GDeltaTime );

		GIsRequestingExit = TRUE; // so CTRL-C will exit immediately
	}

	GIsRunning = 0;

	return Retval;
}


/**
 * test to check CheckNativeClassSizes 
 */
INT SmokeTest_CheckNativeClassSizes( const TCHAR* Parms )
{
	GIsRequestingExit = TRUE; // so CTRL-C will exit immediately

	LoadAllNativeScriptPackages(); // after this we can verify them

	// Verify native class sizes and member offsets.
	CheckNativeClassSizes();

	return 0;
}




/*-----------------------------------------------------------------------------
	Actor iterator implementations.
-----------------------------------------------------------------------------*/

/**
 * Returns the actor count.
 *
 * @param total actor count
 */
INT FActorIteratorBase::GetProgressDenominator()
{
	return GetActorCount();
}
/**
 * Returns the actor count.
 *
 * @param total actor count
 */
INT FActorIteratorBase::GetActorCount()
{
	INT TotalActorCount = 0;
	for( INT LevelIndex=0; LevelIndex<GWorld->Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = GWorld->Levels(LevelIndex);
		TotalActorCount += Level->Actors.Num();
	}
	return TotalActorCount;
}
/**
 * Returns the dynamic actor count.
 *
 * @param total dynamic actor count
 */
INT FActorIteratorBase::GetDynamicActorCount()
{
	INT TotalActorCount = 0;
	for( INT LevelIndex=0; LevelIndex<GWorld->Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = GWorld->Levels(LevelIndex);
		TotalActorCount += Level->Actors.Num() - Level->iFirstDynamicActor;
	}
	return TotalActorCount;
}
/**
 * Returns the net relevant actor count.
 *
 * @param total net relevant actor count
 */
INT FActorIteratorBase::GetNetRelevantActorCount()
{
	INT TotalActorCount = 0;
	for( INT LevelIndex=0; LevelIndex<GWorld->Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = GWorld->Levels(LevelIndex);
		TotalActorCount += Level->Actors.Num() - Level->iFirstNetRelevantActor;
	}
	return TotalActorCount;
}

 /**
  *	Renders the color list table
  *
  *	@param Viewport	The viewport to render to
  * @param Canvas	Canvas object to use for rendering
  *	@param X		Suggested X coordinate for where to start drawing
  *	@param Y		Suggested Y coordinate for where to start drawing
  *	@return			Y coordinate of the next line after this output
  */
INT DrawColorListTable( FViewport* Viewport, FCanvas* Canvas, INT X, INT Y )
{
	extern UBOOL GShowColorList;

	if( GShowColorList == FALSE )
	{
		return Y;
	}

	UFont* Font = GEngine->TinyFont;

	const INT LineHeight = appTrunc( Font->GetMaxCharHeight() );
	const INT ColorsNum = GColorList.GetColorsNum();
	const INT MaxLinesInColumn = 35;
	const INT ColumnsNum = appCeil( (FLOAT)ColorsNum / (FLOAT)MaxLinesInColumn );

	Y += 16;
	const INT SavedY = Y;
	const INT LowestY = Y + MaxLinesInColumn * LineHeight;

	INT ColorIndex = 0;

	// Draw columns with color list.
	for( INT ColumnIndex = 0; ColumnIndex < ColumnsNum; ColumnIndex++ )
	{
		INT LineWidthMax = 0;

		for( INT ColColorIndex = 0; ColColorIndex < MaxLinesInColumn; ColColorIndex++ )
		{
			const INT ColorIndex = ColumnIndex * MaxLinesInColumn + ColColorIndex;
			if( ColorIndex >= ColorsNum )
			{
				break;
			}

			const FColor& Color = GColorList.GetFColorByIndex( ColorIndex );
			const FString Line = *FString::Printf( TEXT( "%3i %s %s" ), ColorIndex, *GColorList.GetColorNameByIndex( ColorIndex ), *Color.ToString() );

			LineWidthMax = Max( LineWidthMax, Font->GetStringSize( *Line ) );

			DrawShadowedString( Canvas, X, Y, *Line, Font, FLinearColor(Color) );
			Y += LineHeight;
		}

		X += LineWidthMax;
		LineWidthMax = 0;
		Y = SavedY;
	}

	return LowestY;
}

 /**
  *	Renders the FPS counter
  *
  *	@param Viewport	The viewport to render to
  * @param Canvas	Canvas object to use for rendering
  *	@param X		Suggested X coordinate for where to start drawing
  *	@param Y		Suggested Y coordinate for where to start drawing
  *	@return			Y coordinate of the next line after this output
  */
INT DrawFPSCounter( FViewport* Viewport, FCanvas* Canvas, INT X, INT Y )
{
	// Pick a larger font on console.
#if CONSOLE && !IPHONE
	UFont* Font = GEngine->MediumFont;
#else
	UFont* Font = GEngine->SmallFont;
#endif

	// Choose the counter color based on the average framerate.
	FColor FPSColor = GAverageFPS < 20.0f ? FColor(255,0,0) : (GAverageFPS < 29.5f ? FColor(255,255,0) : FColor(0,255,0));

	// Start drawing the various counters.
	const INT RowHeight = appTrunc( Font->GetMaxCharHeight() * 1.1f );
	// Draw the FPS counter.
	DrawShadowedString(Canvas,
		X,
		Y,
		*FString::Printf(TEXT("%5.2f FPS"), GAverageFPS),
		Font,
		FPSColor
		);
	Y += RowHeight;

	// Draw the frame time.
	DrawShadowedString(Canvas,
		X,
		Y,
		*FString::Printf(TEXT("%5.2f ms"), GAverageMS),
		Font,
		FPSColor
		);
	Y += RowHeight;
	
	return Y;
}

 /**
  *	Renders AI stats
  *
  *	@param Viewport	The viewport to render to
  * @param Canvas	Canvas object to use for rendering
  *	@param X		Suggested X coordinate for where to start drawing
  *	@param Y		Suggested Y coordinate for where to start drawing
  *	@return			Y coordinate of the next line after this output
  */
INT DrawAIStats( FViewport* Viewport, FCanvas* Canvas, INT X, INT Y )
{
	// Pick a larger font on console.
#if CONSOLE
	UFont* Font = GEngine->MediumFont;
#else
	UFont* Font = GEngine->SmallFont;
#endif

	// gather numbers
	INT NumAI=0;
	INT NumAIRendered=0;
	for (AController *Controller = GWorld->GetWorldInfo()->ControllerList; Controller != NULL; Controller = Controller->NextController)
	{
		if( !Controller->GetAPlayerController() )
		{
			++NumAI;
			if( Controller->Pawn != NULL && GWorld->GetTimeSeconds() - Controller->Pawn->LastRenderTime < 0.08f )
			{
				++NumAIRendered;
			}
		}
	}

	
#define MAXDUDES 20
#define BADAMTOFDUDES 12
	FColor TotalColor = FColor(0,255,0);
	if( NumAI > BADAMTOFDUDES )
	{
		FLOAT Scalar = 1.0f - Clamp<FLOAT>((FLOAT)NumAI / (FLOAT)MAXDUDES,0.f,1.f);

		TotalColor = FColor::MakeRedToGreenColorFromScalar(Scalar);
	}

	FColor RenderedColor = FColor(0,255,0);
	if ( NumAIRendered > BADAMTOFDUDES )
	{
		FLOAT Scalar = 1.0f - Clamp<FLOAT>((FLOAT)NumAIRendered / (FLOAT)MAXDUDES,0.f,1.f);

		RenderedColor = FColor::MakeRedToGreenColorFromScalar(Scalar);

	}

	const INT RowHeight = appTrunc( Font->GetMaxCharHeight() * 1.1f );
	DrawShadowedString(Canvas,
		X,
		Y,
		*FString::Printf(TEXT("%i AI"), NumAI),
		Font,
		TotalColor
		);
	Y += RowHeight;

	DrawShadowedString(Canvas,
		X,
		Y,
		*FString::Printf(TEXT("%i AI Rendered"), NumAIRendered),
		Font,
		RenderedColor
		);
	Y += RowHeight;
	
	return Y;
}


 /**
  *	Renders the memory summary stats
  *
  *	@param Viewport	The viewport to render to
  * @param Canvas	Canvas object to use for rendering
  *	@param X		Suggested X coordinate for where to start drawing
  *	@param Y		Suggested Y coordinate for where to start drawing
  *	@return			Y coordinate of the next line after this output
  */
INT DrawMemorySummaryStats( FViewport* Viewport, FCanvas* Canvas, INT X, INT Y )
{
	// Pick a larger font on console.
#if CONSOLE
	UFont* Font = GEngine->MediumFont;
#else
	UFont* Font = GEngine->SmallFont;
#endif

	// Retrieve allocation info.
	FMemoryAllocationStats MemStats;
	GMalloc->GetAllocationInfo( MemStats );
	FLOAT MemoryInMByte = MemStats.TotalUsed / 1024.f / 1024.f;

	// Draw the memory summary stats.
	DrawShadowedString(
		Canvas,
		X,
		Y,
		*FString::Printf(TEXT("%5.2f MByte"), MemoryInMByte),
		Font,
		FColor(30,144,255)
		);

	const INT RowHeight = appTrunc( Font->GetMaxCharHeight() * 1.1f );
	Y += RowHeight;
	return Y;
}

/** Utility that gets a color for a particular level status */
FColor GetColorForLevelStatus(INT Status)
{
	FColor Color = FColor(255,255,255);
	switch( Status )
	{
	case LEVEL_Visible:
		Color = FColor(255,0,0);	// red  loaded and visible
		break;
	case LEVEL_MakingVisible:
		Color = FColor(255,128,0);	// orange, in process of being made visible
		break;
	case LEVEL_Loading:
		Color = FColor(255,0,255);	// purple, in process of being loaded
		break;
	case LEVEL_Loaded:
		Color = FColor(255,255,0);	// yellow loaded but not visible
		break;
	case LEVEL_UnloadedButStillAround:
		Color = FColor(0,0,255);	// blue  (GC needs to occur to remove this)
		break;
	case LEVEL_Unloaded:
		Color = FColor(0,255,0);	// green
		break;
	case LEVEL_Preloading:
		Color = FColor(255,0,255);	// purple (preloading)
		break;
	default:
		break;
	};

	return Color;
}

/** Transforms a location in 3D space into 'map space', in 2D */
static FVector2D TransformLocationToMap(FVector2D TopLeftPos, FVector2D BottomRightPos, FVector2D MapOrigin, const FVector2D& MapSize, FVector Loc)
{
	FVector2D MapPos;

	MapPos = MapOrigin;

	MapPos.X += MapSize.X * ((Loc.Y - TopLeftPos.Y)/(BottomRightPos.Y - TopLeftPos.Y));
	MapPos.Y += MapSize.Y * (1.0 - ((Loc.X - BottomRightPos.X)/(TopLeftPos.X - BottomRightPos.X)));	

	return MapPos;
}

/** Utility for drawing a volume geometry (as seen from above) onto the canvas */
static void DrawVolumeOnCanvas(const AVolume* Volume, FCanvas* Canvas, const FVector2D& TopLeftPos, const FVector2D& BottomRightPos, const FVector2D& MapOrigin, const FVector2D& MapSize, const FColor& VolColor)
{
	if(Volume && Volume->BrushComponent)
	{
		FMatrix BrushTM = Volume->BrushComponent->LocalToWorld;

		// Iterate over each piece
		for(INT ConIdx=0; ConIdx<Volume->BrushComponent->BrushAggGeom.ConvexElems.Num(); ConIdx++)
		{
			FKConvexElem& ConvElem = Volume->BrushComponent->BrushAggGeom.ConvexElems(ConIdx);

			// Draw each triangle that makes up the convex hull
			const INT NumTris = ConvElem.FaceTriData.Num()/3;
			for(INT i=0; i<NumTris; i++)
			{
				// Get the verts that make up this triangle.
				const INT I0 = ConvElem.FaceTriData((i*3)+0);
				const INT I1 = ConvElem.FaceTriData((i*3)+1);
				const INT I2 = ConvElem.FaceTriData((i*3)+2);

				const FVector V0 = BrushTM.TransformFVector(ConvElem.VertexData(I0));
				const FVector V1 = BrushTM.TransformFVector(ConvElem.VertexData(I1));
				const FVector V2 = BrushTM.TransformFVector(ConvElem.VertexData(I2));

				// We only want to draw faces pointing up
				const FVector Edge0 = V1 - V0;
				const FVector Edge1 = V2 - V1;
				const FVector Normal = (Edge1 ^ Edge0).SafeNormal();
				if(Normal.Z > 0.01)
				{
					// Transform as 2d points in 'map space'
					const FVector2D M0 = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, V0 );
					const FVector2D M1 = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, V1 );
					const FVector2D M2 = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, V2 );

					// dummy UVs
					FVector2D UVCoords(0,0);
					DrawTriangle2D(Canvas, M0, UVCoords, M1, UVCoords, M2, UVCoords, FLinearColor(VolColor));					

					// Draw edges of face
					if( ConvElem.DirIsFaceEdge(ConvElem.VertexData(I0) - ConvElem.VertexData(I1)) )
					{
						DrawLine( Canvas, FVector(M0.X,M0.Y,0) , FVector(M1.X,M1.Y,0), VolColor );
					}

					if( ConvElem.DirIsFaceEdge(ConvElem.VertexData(I1) - ConvElem.VertexData(I2)) )
					{
						DrawLine( Canvas, FVector(M1.X,M1.Y,0) , FVector(M2.X,M2.Y,0), VolColor );
					}

					if( ConvElem.DirIsFaceEdge(ConvElem.VertexData(I2) - ConvElem.VertexData(I0)) )
					{
						DrawLine( Canvas, FVector(M2.X,M2.Y,0) , FVector(M0.X,M0.Y,0), VolColor );
					}
				}
			}
		}
	}
}


/** Utility for drawing a level grid volume (as seen from above) onto the canvas */
static void DrawLevelGridVolumeOnCanvas(const ALevelGridVolume* LevelGridVolume, FCanvas* Canvas, const FVector2D& TopLeftPos, const FVector2D& BottomRightPos, const FVector2D& MapOrigin, const FVector2D& MapSize)
{
	if( ensure( LevelGridVolume != NULL ) )
	{
		// Draw the volume's actual brush geometry to the canvas
		const FLinearColor VolumeColor( 0.5f, 0.5f, 0.5f, 0.25f );
		DrawVolumeOnCanvas( LevelGridVolume, Canvas, TopLeftPos, BottomRightPos, MapOrigin, MapSize, VolumeColor );

		// Grab the volume's bounding box.  Grid volumes are always treated as axis aligned boxes.
		const FBox GridBounds = LevelGridVolume->GetGridBounds();

		// Convert the grid bounds to map coordinates
		const FVector2D BoxMinOnMap = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, GridBounds.Min );
		const FVector2D BoxMaxOnMap = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, GridBounds.Max );

		// Draw the bounds of the entire grid in opaque white
		DrawBox2D( Canvas, BoxMinOnMap, BoxMaxOnMap, FLinearColor::White );

		// Draw the name of this grid
		const FLOAT TextOffset = 16.0f;
		const FString& GridNameString = LevelGridVolume->GetLevelGridVolumeName();
		DrawString( Canvas, BoxMinOnMap.X, BoxMaxOnMap.Y - TextOffset, *GridNameString, GEngine->GetTinyFont(), FLinearColor::White );
	}
}



/** Utility for drawing a level grid volume cell (as seen from above) onto the canvas */
static void DrawLevelGridVolumeCellOnCanvas(const ALevelGridVolume* LevelGridVolume, const FLevelGridCellCoordinate& GridCell, FCanvas* Canvas, const FVector2D& TopLeftPos, const FVector2D& BottomRightPos, const FVector2D& MapOrigin, const FVector2D& MapSize, const FColor& VolColor)
{
	if( ensure( LevelGridVolume != NULL ) )
	{
		// Grab the world space bounds of the grid cell
		FBox GridCellBounds = LevelGridVolume->GetGridCellBounds( GridCell );

		// Convert the grid cell bounds to map coordinates
		const FVector2D BoxMinOnMap = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, GridCellBounds.Min );
		const FVector2D BoxMaxOnMap = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, GridCellBounds.Max );
		const FVector2D BoxCenterOnMap = BoxMinOnMap + ( BoxMaxOnMap - BoxMinOnMap ) * 0.5f;

		const FLOAT InsetScale = 0.025f;
		const FLinearColor LoadedCellColor( 40, 200, 40 );	// Bright green
		const FLinearColor TrueBoundsColor( 0.2f, 0.2f, 0.2f );
		if( LevelGridVolume->CellShape == LGCS_Box )
		{
			// Draw the true bounds of the cell
			DrawBox2D( Canvas, BoxMinOnMap, BoxMaxOnMap, TrueBoundsColor );

			// Inset the cell bounds a bit so that adjacent cells don't visually overlap
			FBox InsetGridCellBounds = GridCellBounds;
			const FVector GridCellSize = GridCellBounds.GetExtent() * 2.0f;
			InsetGridCellBounds.Min += GridCellSize * InsetScale;
			InsetGridCellBounds.Max -= GridCellSize * InsetScale;

			// Convert the inset grid cell bounds to map coordinates
			const FVector2D InsetBoxMinOnMap = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, InsetGridCellBounds.Min );
			const FVector2D InsetBoxMaxOnMap = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, InsetGridCellBounds.Max );

			// Draw the box to represent this cell
			const FVector2D InsetBoxSizeOnMap = InsetBoxMaxOnMap - InsetBoxMinOnMap;
			DrawTile( Canvas, InsetBoxMinOnMap.X, InsetBoxMinOnMap.Y, InsetBoxSizeOnMap.X, InsetBoxSizeOnMap.Y, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor( VolColor ) );

			// Draw the cell's bounds using it's loaded color
			DrawBox2D( Canvas, InsetBoxMinOnMap, InsetBoxMaxOnMap, LoadedCellColor );
		}
		else if( ensure( LevelGridVolume->CellShape == LGCS_Hex ) )
		{
			// Construct the hexagon vertices
			FVector2D HexPoints[ 6 ];
			LevelGridVolume->ComputeHexCellShape( HexPoints );	// Out

			const FVector GridCellCenter = LevelGridVolume->GetGridCellCenterPoint( GridCell );
			const FVector2D GridCellCenterOnMap = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, GridCellCenter );
			
			for( INT CurPointIndex = 0; CurPointIndex < ARRAY_COUNT( HexPoints ); ++CurPointIndex )
			{
				const FVector2D& CurHexPoint = HexPoints[ CurPointIndex ];
				const FVector2D& NextHexPoint = HexPoints[ ( CurPointIndex + 1 ) % ARRAY_COUNT( HexPoints ) ];

				const FVector VertA = GridCellCenter + FVector( CurHexPoint.X, CurHexPoint.Y, 0.0f );
				const FVector VertB = GridCellCenter + FVector( NextHexPoint.X, NextHexPoint.Y, 0.0f );

				const FVector2D HexVertAOnMap = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, VertA );
				const FVector2D HexVertBOnMap = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, VertB );

				// Draw the true bounds of the hex cell
				DrawLine2D( Canvas, HexVertAOnMap, HexVertBOnMap, TrueBoundsColor );

				// Push the points toward the cell center a bit just to avoid z-fighting
				// between adjacent cells
				const FVector InsetVertA = GridCellCenter + FVector( CurHexPoint.X, CurHexPoint.Y, 0.0f ) * ( 1.0f - InsetScale );
				const FVector InsetVertB = GridCellCenter + FVector( NextHexPoint.X, NextHexPoint.Y, 0.0f ) * ( 1.0f - InsetScale );
				const FVector2D InsetHexVertAOnMap = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, InsetVertA );
				const FVector2D InsetHexVertBOnMap = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, InsetVertB );

				// Draw a solid polygon using the volume color
				FVector2D UVCoords(0,0); // Dummy UV
				DrawTriangle2D(
					Canvas,
					GridCellCenterOnMap, UVCoords,
					InsetHexVertAOnMap, UVCoords,
					InsetHexVertBOnMap, UVCoords,
					FLinearColor( VolColor ) );

				// Draw the bounds of the hex using the loaded color
				DrawLine2D( Canvas, InsetHexVertAOnMap, InsetHexVertBOnMap, FLinearColor( LoadedCellColor ) );
			}
		}


		// Draw the grid cell coordinates
		const FString GridCellString = FString::Printf( TEXT( "(%i, %i)" ), GridCell.X, GridCell.Y );
		DrawString(
			Canvas,
			BoxCenterOnMap.X - 10.0f,
			BoxCenterOnMap.Y - 10.0f,
			*GridCellString, GEngine->GetTinyFont(), FLinearColor( 0.55f, 0.55f, 0.55f ) );
	}
}

/** Util that takes a 2D vector and rotates it by RotAngle (given in radians) */
static FVector2D RotateVec2D(const FVector2D InVec, FLOAT RotAngle)
{
	FVector2D OutVec;
	OutVec.X = (InVec.X * appCos(RotAngle)) - (InVec.Y * appSin(RotAngle));
	OutVec.Y = (InVec.X * appSin(RotAngle)) + (InVec.Y * appCos(RotAngle));
	return OutVec;
}

/** 
 *	Draw a map showing streaming volumes in the level 
 */
void DrawLevelStatusMap(FCanvas* Canvas, const FVector2D& MapOrigin, const FVector2D& MapSize, const FVector& ViewLocation, const FRotator& ViewRotation )
{
	if(GShowLevelStatusMap)
	{
		// Get status of each sublevel (by name)
		TMap<FName,INT> StreamingLevels;	
		FString LevelPlayerIsInName;
		GetLevelStremingStatus( StreamingLevels, LevelPlayerIsInName );

		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

		// First iterate to find bounds of all streaming volumes
		FBox AllVolBounds(0);
		for( TMap<FName,INT>::TIterator It(StreamingLevels); It; ++It )
		{
			FName	LevelName	= It.Key();

			ULevelStreaming* LevelStreaming = WorldInfo->GetLevelStreamingForPackageName(LevelName);
			if(LevelStreaming && LevelStreaming->bDrawOnLevelStatusMap)
			{
				AllVolBounds += LevelStreaming->GetStreamingVolumeBounds();
			}
		}

		// We need to ensure the XY aspect ratio of AllVolBounds is the same as the map

		// Work out scale factor between map and world
		FVector VolBoundsSize = (AllVolBounds.Max - AllVolBounds.Min);
		FLOAT ScaleX = MapSize.X/VolBoundsSize.X;
		FLOAT ScaleY = MapSize.Y/VolBoundsSize.Y;
		FLOAT UseScale = Min(ScaleX, ScaleY); // Pick the smallest scaling factor

		// Resize AllVolBounds
		FVector NewVolBoundsSize = VolBoundsSize;
		NewVolBoundsSize.X = MapSize.X / UseScale;
		NewVolBoundsSize.Y = MapSize.Y / UseScale;
		FVector DeltaBounds = (NewVolBoundsSize - VolBoundsSize);
		AllVolBounds.Min -= 0.5f * DeltaBounds;
		AllVolBounds.Max += 0.5f * DeltaBounds;
		
		// Find world-space location for top-left and bottom-right corners of map
		FVector2D TopLeftPos(AllVolBounds.Max.X, AllVolBounds.Min.Y); // max X, min Y
		FVector2D BottomRightPos(AllVolBounds.Min.X, AllVolBounds.Max.Y); // min X, max Y


		// Now we iterate and actually draw volumes
		TArray< ALevelGridVolume* > RenderedLevelGridVolumes;
		for( TMap<FName,INT>::TIterator It(StreamingLevels); It; ++It )
		{
			FName	LevelName	= It.Key();
			INT	Status			= It.Value();

			// Find the color to draw this level in
			FColor StatusColor	= GetColorForLevelStatus(Status);
			StatusColor.A = 64; // make it translucent

			ULevelStreaming* LevelStreaming = WorldInfo->GetLevelStreamingForPackageName(LevelName);
			if(LevelStreaming && LevelStreaming->bDrawOnLevelStatusMap)
			{
				for(INT VolIdx=0; VolIdx<LevelStreaming->EditorStreamingVolumes.Num(); VolIdx++)
				{
					ALevelStreamingVolume* StreamingVol = LevelStreaming->EditorStreamingVolumes(VolIdx);
					if(StreamingVol)
					{
						DrawVolumeOnCanvas(StreamingVol, Canvas, TopLeftPos, BottomRightPos, MapOrigin, MapSize, StatusColor);
					}
				}

				if( LevelStreaming->EditorGridVolume != NULL )
				{
					if( !RenderedLevelGridVolumes.ContainsItem( LevelStreaming->EditorGridVolume ) )
					{
						DrawLevelGridVolumeOnCanvas( LevelStreaming->EditorGridVolume, Canvas, TopLeftPos, BottomRightPos, MapOrigin, MapSize);
						RenderedLevelGridVolumes.AddItem( LevelStreaming->EditorGridVolume );
					}

					FLevelGridCellCoordinate GridCell;
					GridCell.X = LevelStreaming->GridPosition[ 0 ];
					GridCell.Y = LevelStreaming->GridPosition[ 1 ];
					GridCell.Z = LevelStreaming->GridPosition[ 2 ];
					DrawLevelGridVolumeCellOnCanvas(LevelStreaming->EditorGridVolume, GridCell, Canvas, TopLeftPos, BottomRightPos, MapOrigin, MapSize, StatusColor);
				}
			}
		}



		// Now we want to draw the player(s) location on the map
		{
			// Find map location for arrow
			const FVector2D PlayerMapPos = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, ViewLocation );

			// Make verts for little rotated arrow
			FLOAT PlayerYaw = (U2Rad * ViewRotation.Yaw) - (0.5f * PI); // We have to add 90 degrees because +X in world space means -Y in map space
			const FVector2D M0 = PlayerMapPos + RotateVec2D(FVector2D(7,0), PlayerYaw);
			const FVector2D M1 = PlayerMapPos + RotateVec2D(FVector2D(-7,5), PlayerYaw);
			const FVector2D M2 = PlayerMapPos + RotateVec2D(FVector2D(-7,-5), PlayerYaw);

			FVector2D UVCoords(0,0); // Dummy UV
			DrawTriangle2D(Canvas, M0, UVCoords, M1, UVCoords, M2, UVCoords, FLinearColor(1,1,1));	
		}
	}
}

/**
 *	Render the level stats
 *
 *	@param Viewport	The viewport to render to
 *	@param Canvas	Canvas object to use for rendering
 *	@param X		Suggested X coordinate for where to start drawing
 *	@param Y		Suggested Y coordinate for where to start drawing
 *	@return			Y coordinate of the next line after this output
 */
INT DrawLevelStats( FViewport* Viewport, FCanvas* Canvas, INT X, INT Y )
{
	// Render level stats.
	INT MaxY = Y;
	if( GShowLevelStats )
	{
		TMap<FName,INT> StreamingLevels;	
		FString LevelPlayerIsInName;
		GetLevelStremingStatus( StreamingLevels, LevelPlayerIsInName );

		// now do drawing to the screen

		// Render unloaded levels in red, loaded ones in yellow and visible ones in green. Blue signifies that a level is unloaded but
		// hasn't been garbage collected yet.
		FLOAT SavedFontScale = GEngine->SmallFont->GetFontScalingFactor();
		FLOAT FontScale = Viewport->GetSizeX() / 1280.0f;
		INT FontSize = (INT)(12.0f * FontScale);
 		GEngine->SmallFont->SetFontScalingFactor(FontScale);

		DrawShadowedString(Canvas, X, Y, TEXT("Level streaming"), GEngine->SmallFont, FLinearColor::White );
		Y+=FontSize;

		// now draw the "map" name
		FString MapName	= GWorld->CurrentLevel->GetOutermost()->GetName();

		if( LevelPlayerIsInName == MapName )
		{
			MapName = *FString::Printf( TEXT("->  %s"), *MapName );
		}
		else
		{
			MapName = *FString::Printf( TEXT("    %s"), *MapName );
		}

		DrawShadowedString(Canvas, X, Y, *MapName, GEngine->SmallFont, FColor(127,127,127) );

		Y+=FontSize;

		INT BaseY = Y;

		// now draw the levels
		for( TMap<FName,INT>::TIterator It(StreamingLevels); It; ++It )
		{	
			// Wrap around at the bottom.
#if MOBILE
			if( Y > GScreenHeight - 30 )
#else
			if( Y > GSystemSettings.ResY - 30 )
#endif
			{
				MaxY = Max( MaxY, Y );
				Y = BaseY;
				X += 250;
			}

			FString	LevelName	= It.Key().ToString();
			INT		Status		= It.Value();
			FColor	Color		= GetColorForLevelStatus(Status);

			UPackage* LevelPackage = FindObject<UPackage>( NULL, *LevelName );

			if( LevelPackage 
			&& (LevelPackage->GetLoadTime() > 0) 
			&& (Status != LEVEL_Unloaded) )
			{
				LevelName += FString::Printf(TEXT(" - %4.1f sec"), LevelPackage->GetLoadTime());
			}
			else if( UObject::GetAsyncLoadPercentage( *LevelName ) >= 0 )
			{
				const INT Percentage = appTrunc( UObject::GetAsyncLoadPercentage( *LevelName ) );
				LevelName += FString::Printf(TEXT(" - %3i %%"), Percentage ); 
			}

			if( LevelPlayerIsInName == LevelName )
			{
				LevelName = *FString::Printf( TEXT("->  %s"), *LevelName );
			}
			else
			{
				LevelName = *FString::Printf( TEXT("    %s"), *LevelName );
			}

			DrawShadowedString(Canvas, X + 4, Y, *LevelName, GEngine->SmallFont, Color );
			Y+=FontSize;
		}

		GEngine->SmallFont->SetFontScalingFactor(SavedFontScale);
	}

	return Max( MaxY, Y);
}

/**
*	Render Active sound waves
*
*	@param Viewport	The viewport to render to
*	@param Canvas	Canvas object to use for rendering
*	@param X		Suggested X coordinate for where to start drawing
*	@param Y		Suggested Y coordinate for where to start drawing
*	@return			Y coordinate of the next line after this output
*/
INT DrawSoundWaves( FViewport* Viewport, FCanvas* Canvas, INT X, INT Y )
{
	// Render level stats.
	if( GShowSoundWaves )
	{
		DrawShadowedString(Canvas, X, Y, TEXT("Active Sound Waves:"), GEngine->SmallFont, FLinearColor::White );
		Y+=12;

		UAudioDevice* AudioDevice = GEngine->Client->GetAudioDevice();
		if( AudioDevice )
		{
			TArray<FWaveInstance*> WaveInstances;
			INT FirstActiveIndex = AudioDevice->GetSortedActiveWaveInstances( WaveInstances, false );

			for( INT InstanceIndex = FirstActiveIndex; InstanceIndex < WaveInstances.Num(); InstanceIndex++ )
			{
				FWaveInstance* WaveInstance = WaveInstances( InstanceIndex );
				FSoundSource* Source = AudioDevice->WaveInstanceSourceMap.FindRef( WaveInstance );
				AActor* SoundOwner = WaveInstance->AudioComponent ? WaveInstance->AudioComponent->GetOwner() : NULL;

				FString TheString = *FString::Printf(TEXT( "%4i.    %s %6.2f  %s   %s"),
															InstanceIndex,
															Source ? TEXT( "Yes" ) : TEXT( " No" ),
															WaveInstance->Volume,
															*WaveInstance->WaveData->GetPathName(),
															SoundOwner ? *SoundOwner->GetName() : TEXT("None") );

				DrawShadowedString(Canvas, X, Y, *TheString, GEngine->SmallFont, FColor(255,255,255) );
				Y+=12;
			}
			INT ActiveInstances = WaveInstances.Num() - FirstActiveIndex;
			INT R,G,B;
			R=G=B=0;
			INT Max = AudioDevice->MaxChannels/2;
			FLOAT f = Clamp<FLOAT>((FLOAT)(ActiveInstances-Max) / (FLOAT)Max,0.f,1.f);			
			R = appTrunc(f * 255);
			if(ActiveInstances > Max)
			{
				f = Clamp<FLOAT>((FLOAT)(Max-ActiveInstances) / (FLOAT)Max,0.5f,1.f);
			}
			else
			{
				f = 1.0f;
			}
			G = appTrunc(f * 255);
			
			DrawShadowedString(Canvas,X,Y,*FString::Printf(TEXT(" Total: %i"),ActiveInstances),GEngine->SmallFont,FColor(R,G,B));
			Y+=12;
		}
	}
	return Y;
}

/**
*	Render Active sound cues
*
*	@param Viewport	The viewport to render to
*	@param Canvas	Canvas object to use for rendering
*	@param X		Suggested X coordinate for where to start drawing
*	@param Y		Suggested Y coordinate for where to start drawing
*	@return			Y coordinate of the next line after this output
*/
INT DrawSoundCues(FViewport* Viewport, FCanvas* Canvas, INT X, INT& Y)
{
	if (GShowSoundCues)
	{
		if (GEngine->GetCurrentWorldInfo()->Pauser == NULL)
		{
			INT ActiveSoundCues = 0;
			UAudioDevice* AudioDevice = GEngine->Client->GetAudioDevice();
			if (AudioDevice)
			{
				DrawShadowedString(Canvas, X, Y, TEXT("Active Sound Cues:"), GEngine->SmallFont, FColor(0, 255, 0));
				Y += 12;

				for (INT i = 0; i < AudioDevice->AudioComponents.Num(); ++i)
				{
					UAudioComponent* AudioComponent = AudioDevice->AudioComponents(i);

					if (AudioComponent && AudioComponent->SoundCue &&
						(AudioComponent->CurrentVolume*AudioComponent->CurrentVolumeMultiplier) > 0) 
					{
						const FString TheString = FString::Printf(TEXT("%4i. %s %s"), ActiveSoundCues, *AudioComponent->SoundCue->GetPathName(), *AudioComponent->SoundCue->SoundClass.GetNameString() );
						DrawShadowedString(Canvas, X, Y, *TheString, GEngine->SmallFont, FColor(255, 255, 255));
						Y += 12;

						ActiveSoundCues ++;
					}
				}
		
				DrawShadowedString( Canvas, X, Y, *FString::Printf(TEXT("Total: %i"), ActiveSoundCues), GEngine->SmallFont, FColor(0, 255, 0) );
				Y += 12;
			}
		}	
	}

	return Y;
}

/** Helper structure for sorting sound cues by predefined criteria. */
struct FSoundCueInfo
{
	/** Path name to this sound cue. */
	FString PathName;

	/** Distance betweend a listener and this sound cue. */
	FLOAT Distance;

	/** Sound group this sound cue belongs to. */
	FString ClassName;

	/** An audio component of this sound cue. */
	UAudioComponent* AudioComponent;

	/** Wave instances currently used by this sound cue. */
	TArray<FWaveInstance*> WaveInstances;

	FSoundCueInfo( FString InPathName, FLOAT InDistance, FString InClassName, UAudioComponent* InAudioComponent )
		: PathName( InPathName )
		, Distance( InDistance )
		, ClassName( InClassName )
		, AudioComponent( InAudioComponent )
	{}

	INT ComparePathNames( const FSoundCueInfo& Other ) const
	{
		return appStricmp( *PathName, *Other.PathName );
	}

	INT CompareDistance( const FSoundCueInfo& Other ) const
	{
		return ( Distance - Other.Distance ) > 0 ? +1 : -1;
	}

	INT CompareClass( const FSoundCueInfo& Other ) const
	{
		return appStricmp( *ClassName, *Other.ClassName );
	}

	INT CompareWaveInstancesNum( const FSoundCueInfo& Other ) const
	{
		return Other.WaveInstances.Num() - WaveInstances.Num();
	}
};

IMPLEMENT_COMPARE_CONSTPOINTER( FSoundCueInfo, _CompareNames,		{ return A->ComparePathNames( *B ); } );
IMPLEMENT_COMPARE_CONSTPOINTER( FSoundCueInfo, _CompareDistances,	{ return A->CompareDistance( *B ); } );
IMPLEMENT_COMPARE_CONSTPOINTER( FSoundCueInfo, _CompareClasses,		{ return A->CompareClass( *B ); } );
IMPLEMENT_COMPARE_CONSTPOINTER( FSoundCueInfo, _CompareWaveInstsNum,{ return A->CompareWaveInstancesNum( *B ); } );

/**
*	Renders active sound waves and sound cues
*
*	@param Viewport	The viewport to render to
*	@param Canvas	Canvas object to use for rendering
*	@param X		Suggested X coordinate for where to start drawing
*	@param Y		Suggested Y coordinate for where to start drawing
*	@return			Y coordinate of the next line after this output
*/
INT DrawSounds(FViewport* Viewport, FCanvas* Canvas, INT X, INT& Y)
{
#if _DEBUG

	typedef TMap< UAudioComponent*, FSoundCueInfo* > TMapSounds;

#else

	typedef TMemStackAllocator<GMainThreadMemStack> TMemStackGameAllocator;
	typedef TSetAllocator<TSparseArrayAllocator<TMemStackGameAllocator,TMemStackGameAllocator>,TMemStackGameAllocator> TMapStackAllocator;
	typedef TMap< UAudioComponent*, FSoundCueInfo*, TMapStackAllocator > TMapSounds;

#endif // _DEBUG

	if( GShowSounds != SSF_Disabled )
	{
		TMapSounds SoundCueInfos;
		UAudioDevice* AudioDevice = GEngine->Client->GetAudioDevice();
		const UBOOL bDebug = GShowSounds & SSF_Debug;

		if( AudioDevice )
		{
			// Refresh the wave instances inside audio components.
			static TArray<FWaveInstance*> WaveInstances;
			WaveInstances.Reset();

			INT FirstActiveIndex = AudioDevice->GetSortedActiveWaveInstances( WaveInstances, false );

			// Grab the list of all active sound cues.
			const FVector ListenerPosition = AudioDevice->Listeners(0).Location;

			for( INT Nx = 0; Nx < AudioDevice->AudioComponents.Num(); ++Nx )
			{
				UAudioComponent* AudioComponent = AudioDevice->AudioComponents(Nx);

				if( AudioComponent && AudioComponent->SoundCue ) 
				{
					const FString PathName = AudioComponent->SoundCue->GetPathName();
					const FLOAT Distance = (ListenerPosition-AudioComponent->ComponentLocation).Size();
					const FString ClassName = *AudioComponent->SoundCue->SoundClass.GetNameString();

					if( bDebug )
					{
						if( AudioComponent->SoundCue->bDebug )
						{
							SoundCueInfos.Set( AudioComponent, new FSoundCueInfo( PathName, Distance, ClassName, AudioComponent ) );
						}
					}
					else
					{
						SoundCueInfos.Set( AudioComponent, new FSoundCueInfo( PathName, Distance, ClassName, AudioComponent ) );
					}
				}
			}

			// Iterate trough all wave instances.
			for( INT InstanceIndex = FirstActiveIndex; InstanceIndex < WaveInstances.Num(); InstanceIndex++ )
			{
				FWaveInstance* WaveInstance = WaveInstances( InstanceIndex );
				FSoundCueInfo* SoundCueInfo = SoundCueInfos.FindRef( WaveInstance->AudioComponent );
				if( SoundCueInfo )
				{
					SoundCueInfo->WaveInstances.AddItem( WaveInstance );
				}
			}

			FString SortingName = TEXT( "disabled" );

			// Sort the list.
			if( GShowSounds & SSF_Sort_Name )
			{
				SoundCueInfos.ValueSort<COMPARE_CONSTPOINTER_CLASS(FSoundCueInfo,_CompareNames)>();
				SortingName = TEXT( "pathname" );
			}
			else if( GShowSounds & SSF_Sort_Distance )
			{
				SoundCueInfos.ValueSort<COMPARE_CONSTPOINTER_CLASS(FSoundCueInfo,_CompareDistances)>();
				SortingName = TEXT( "distance" );
			}
			else if( GShowSounds & SSF_Sort_Class )
			{
				SoundCueInfos.ValueSort<COMPARE_CONSTPOINTER_CLASS(FSoundCueInfo,_CompareClasses)>();
				SortingName = TEXT( "class" );
			}
			else if( GShowSounds & SSF_Sort_WavesNum )
			{
				SoundCueInfos.ValueSort<COMPARE_CONSTPOINTER_CLASS(FSoundCueInfo,_CompareWaveInstsNum)>();
				SortingName = TEXT( "waves' num" );
			}
			
	
			DrawShadowedString( Canvas, X, Y, TEXT("Active Sound Cues:"), GEngine->SmallFont, FColor(0, 255, 0) );
			Y += 12;

			const FString InfoText = FString::Printf( TEXT( " Sorting: %s Debug: %s" ), *SortingName, bDebug ? TEXT( "enabled" ) : TEXT( "disabled" ) );
			DrawShadowedString( Canvas, X, Y, *InfoText, GEngine->SmallFont, FColor(128, 255, 128) );
			Y += 12;

			DrawShadowedString(Canvas, X, Y, TEXT("Index Path (Class) Distance"), GEngine->SmallFont, FColor(0, 255, 0) );
			Y += 12;

			INT TotalSoundWavesNum = 0;
			INT CueIndex = 0;
			for( TMapSounds::TConstIterator It(SoundCueInfos); It; ++It )
			{
				const FSoundCueInfo& SoundCueInfo = *It.Value();
				const INT WaveInstancesNum = SoundCueInfo.WaveInstances.Num();

				if( WaveInstancesNum > 0 )
				{
					const FString TheString = FString::Printf(TEXT("%4i. %s (%s) %6.2f"), CueIndex, *SoundCueInfo.PathName, *SoundCueInfo.ClassName, SoundCueInfo.Distance );
					DrawShadowedString(Canvas, X, Y, *TheString, GEngine->SmallFont, FColor(255, 255, 255) );
					Y += 12;

					// Get the active sound waves.
					for( INT WaveIndex = 0; WaveIndex < WaveInstancesNum; WaveIndex++ )
					{
						FWaveInstance* WaveInstance = SoundCueInfo.WaveInstances(WaveIndex);
						FSoundSource* Source = AudioDevice->WaveInstanceSourceMap.FindRef( WaveInstance );
						AActor* SoundOwner = WaveInstance->AudioComponent ? WaveInstance->AudioComponent->GetOwner() : NULL;

						FString TheString = *FString::Printf
						(
							TEXT( "    %4i. %s %6.2f %s %s"),
							WaveIndex,
							Source ? TEXT( "Yes" ) : TEXT( " No" ),
							WaveInstance->Volume,
							*WaveInstance->WaveData->GetPathName(),
							SoundOwner ? *SoundOwner->GetName() : TEXT("None")
						);

						DrawShadowedString( Canvas, X, Y, *TheString, GEngine->SmallFont, FColor(205,205,205) );
						Y+=12;

						TotalSoundWavesNum ++;
					}
					CueIndex ++;
				}
			}

			DrawShadowedString( Canvas, X, Y, *FString::Printf( TEXT("Total sound cues: %i, sound waves: %i"), CueIndex, TotalSoundWavesNum ), GEngine->SmallFont, FColor(0, 255, 0) );
			Y += 12;

			// Draw sound cue's sphere.
			if( bDebug && GWorld && GWorld->GetWorldInfo() )
			{
				AWorldInfo* WI = GWorld->GetWorldInfo();
				
				for( TMapSounds::TConstIterator It(SoundCueInfos); It; ++It )
				{
					const FSoundCueInfo& SoundCueInfo = *It.Value();
					const INT WaveInstancesNum = SoundCueInfo.WaveInstances.Num();
 
 					if( SoundCueInfo.AudioComponent->SoundCue->bDebug && SoundCueInfo.Distance > 100.0f && WaveInstancesNum > 0 )
 					{
						TArray<USoundNodeAttenuation*> Attenuations;
						SoundCueInfo.AudioComponent->SoundCue->RecursiveFindAttenuation( SoundCueInfo.AudioComponent->SoundCue->FirstNode, Attenuations );
						if( Attenuations.Num() > 0 )
						{
							USoundNodeAttenuation* Atten = Attenuations( 0 );
							WI->DrawDebugSphere( SoundCueInfo.AudioComponent->ComponentLocation, Atten->RadiusMin, 10, 55, 55, 255 );
							WI->DrawDebugSphere( SoundCueInfo.AudioComponent->ComponentLocation, Atten->RadiusMax, 10, 155, 155, 255 );
							WI->DrawDebugString( SoundCueInfo.AudioComponent->ComponentLocation, SoundCueInfo.PathName, NULL, FColor( 255, 255, 255 ), 0.01f );
						}
 					}
				}
			}

			for( TMapSounds::TConstIterator It(SoundCueInfos); It; ++It )
			{
				delete It.Value();
			}
		}	
	}

	return Y;
}

#if !FINAL_RELEASE
/** draws a property of the given object on the screen similarly to stats */
static void DrawProperty(UCanvas* CanvasObject, UObject* Obj, const FDebugDisplayProperty& PropData, UProperty* Prop, INT X, INT& Y)
{
	checkSlow(PropData.bSpecialProperty || Prop != NULL);
	checkSlow(Prop == NULL || Obj->GetClass()->IsChildOf(Prop->GetOwnerClass()));

	FCanvas* Canvas = CanvasObject->Canvas;
	FString PropText, ValueText;
	if (!PropData.bSpecialProperty)
	{
		PropText = FString::Printf(TEXT("%s.%s.%s = "), *Obj->GetOutermost()->GetName(), *Obj->GetName(), *Prop->GetName());
		if (Prop->ArrayDim == 1)
		{
			UStructProperty* StructProp = Cast<UStructProperty>(Prop);
			if (StructProp != NULL && 
				StructProp->Struct != NULL && StructProp->Struct->GetFName() == NAME_UniqueNetId)
			{
				FUniqueNetId StructNetIdVal = *(FUniqueNetId*)((BYTE*)Obj + StructProp->Offset);
				ValueText = UOnlineSubsystem::UniqueNetIdToString(StructNetIdVal);
			}
			else
			{
				Prop->ExportText(0, ValueText, (BYTE*)Obj, (BYTE*)Obj, Obj, PPF_Localized);
			}
		}
		else
		{
			ValueText += TEXT("(");
			for (INT i = 0; i < Prop->ArrayDim; i++)
			{
				Prop->ExportText(i, ValueText, (BYTE*)Obj, (BYTE*)Obj, Obj, PPF_Localized);
				if (i + 1 < Prop->ArrayDim)
				{
					ValueText += TEXT(",");
				}
			}
			ValueText += TEXT(")");
		}
	}
	else
	{
		PropText = FString::Printf(TEXT("%s.%s.(%s) = "), *Obj->GetOutermost()->GetName(), *Obj->GetName(), *PropData.PropertyName.ToString());
		if (PropData.PropertyName == NAME_State)
		{
			ValueText = (Obj->GetStateFrame() != NULL) ? *Obj->GetStateFrame()->StateNode->GetName() : TEXT("None");
		}
	}

	
	INT CommaIdx = -1;
	UBOOL bDrawPropName = TRUE;
	do
	{
		FString Str = ValueText;
		CommaIdx = ValueText.InStr( TEXT(",") );
		if( CommaIdx >= 0 )
		{
			Str = ValueText.Left(CommaIdx);
			ValueText = ValueText.Mid( CommaIdx+1 );
		}

		INT XL, YL;
		CanvasObject->ClippedStrLen(GEngine->SmallFont, 1.0f, 1.0f, XL, YL, *PropText);
		FTextSizingParameters DrawParams(X, Y, CanvasObject->SizeX - X, 0, GEngine->SmallFont);
		TArray<FWrappedStringElement> TextLines;
		UCanvas::WrapString(DrawParams, X + XL, *Str, TextLines);
		INT XL2 = XL;
		if (TextLines.Num() > 0)
		{
			XL2 += appTrunc(TextLines(0).LineExtent.X);
			for (INT i = 1; i < TextLines.Num(); i++)
			{
				XL2 = Max<INT>(XL2, appTrunc(TextLines(i).LineExtent.X));
			}
		}
		DrawTile( Canvas, X, Y, XL2 + 1, YL * Max<INT>(TextLines.Num(), 1), 0, 0, CanvasObject->DefaultTexture->SizeX, CanvasObject->DefaultTexture->SizeY,
			FLinearColor(0.5f, 0.5f, 0.5f, 0.5f), CanvasObject->DefaultTexture->Resource );
		if( bDrawPropName )
		{
			bDrawPropName = FALSE;
			DrawShadowedString(Canvas, X, Y, *PropText, GEngine->SmallFont, FLinearColor(0.0f, 1.0f, 0.0f));
			if( TextLines.Num() > 1 )
			{
				Y += YL;
			}
		}
		if (TextLines.Num() > 0)
		{
			DrawShadowedString(Canvas, X + XL, Y, *TextLines(0).Value, GEngine->SmallFont, FLinearColor(1.0f, 0.0f, 0.0f));
			for (INT i = 1; i < TextLines.Num(); i++)
			{
				DrawShadowedString(Canvas, X, Y + YL * i, *TextLines(i).Value, GEngine->SmallFont, FLinearColor(1.0f, 0.0f, 0.0f));
			}
			Y += YL * TextLines.Num();
		}
		else
		{
			Y += YL;
		}
	} while( CommaIdx >= 0 );
}
#endif


/**
 *	Draws a simple graph that shows unit times overlaid onto the HUD
 *
 *	@param Viewport	The viewport to render to
 *	@param Canvas	Canvas object to use for rendering
 *	@param bHaveGPUData	True if we have GPU data to display
 */
void DrawUnitTimeGraph( FViewport* Viewport, FCanvas* Canvas, const UBOOL bHaveGPUData )
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE

	// Draw simple unit time graph
	if( GShowUnitTimeGraph )
	{
		// The vertical axis is time in milliseconds
		// The horizontal axis is the frame number (NOT time!!!)

		// Threshold where graph lines will pulsate for slow frames
		const FLOAT AlertTimeMS = 33.33f;

		// Graph layout
		const FLOAT GraphLeftXPos = 80.0f;
		const FLOAT GraphBottomYPos = Viewport->GetSizeY() - 50.0f;
		const FLOAT GraphHorizPixelsPerFrame = 2.0f;
		const FLOAT GraphVerticalPixelsPerMS = 10.0f;
		const FLOAT GraphHeightInMS = 40.0f;

		const FLinearColor GraphBorderColor( 0.1f, 0.1f, 0.1f );
		const FLinearColor AlertLineColor( 0.1f, 0.03f, 0.03f );

		// Compute pulse effect for lines above alert threshold
		const FLOAT AlertPulseFreq = 8.0f;
		const FLOAT AlertPulse = 0.5f + 0.5f * (FLOAT)sin( ( 0.25f * PI * 2.0 ) + ( GCurrentTime * PI * 2.0 ) * AlertPulseFreq );


		// For each type of statistic that we want to graph (0=Render, 1=Game, 2=GPU, 3=Frame)
		enum EGraphStats
		{
			EGS_Render = 0,
			EGS_Game,
			EGS_GPU,
			EGS_Frame,
#if PS3 && STATS
			EGS_SPU,
#endif // PS3 && STATS

			EGS_Count
		};


		FBatchedElements* BatchedElements = Canvas->GetBatchedElements(FCanvas::ET_Line);
		FHitProxyId HitProxyId = Canvas->GetHitProxyId();

		// Reserve line vertices (4 border lines, then up to the maximum number of graph lines)
		BatchedElements->AddReserveLines( 4 + EGS_Count * GUnit_NumberOfSamples );


		// Left
		BatchedElements->AddLine(
			FVector( GraphLeftXPos - 1.0f, GraphBottomYPos - GraphVerticalPixelsPerMS * GraphHeightInMS, 0.0f ),
			FVector( GraphLeftXPos - 1.0f, GraphBottomYPos - 1.0f, 0.0f ),
			GraphBorderColor,
			HitProxyId );

		// Right
		BatchedElements->AddLine(
			FVector( GraphLeftXPos + GraphHorizPixelsPerFrame * GUnit_NumberOfSamples + 1.0f, GraphBottomYPos - GraphVerticalPixelsPerMS * GraphHeightInMS, 0.0f ),
			FVector( GraphLeftXPos + GraphHorizPixelsPerFrame * GUnit_NumberOfSamples + 1.0f, GraphBottomYPos - 1.0f, 0.0f ),
			GraphBorderColor,
			HitProxyId );

		// Bottom
		BatchedElements->AddLine(
			FVector( GraphLeftXPos - 1.0f, GraphBottomYPos - 1.0f, 0.0f ),
			FVector( GraphLeftXPos + GraphHorizPixelsPerFrame * GUnit_NumberOfSamples + 1.0f, GraphBottomYPos - 1.0f, 0.0f ),
			GraphBorderColor,
			HitProxyId );

		// Alert line
		BatchedElements->AddLine(
			FVector( GraphLeftXPos - 8.0f, GraphBottomYPos - AlertTimeMS * GraphVerticalPixelsPerMS, 0.0f ),
			FVector( GraphLeftXPos + GraphHorizPixelsPerFrame * GUnit_NumberOfSamples + 8.0f, GraphBottomYPos - AlertTimeMS * GraphVerticalPixelsPerMS, 0.0f ),
			AlertLineColor,
			HitProxyId );


		for( INT StatIndex = 0; StatIndex < EGS_Count; ++StatIndex )
		{
			// If we don't have GPU data to display, then skip this line
			if( StatIndex == EGS_GPU && !bHaveGPUData )
			{
				continue;
			}
			
			FLinearColor StatColor;
			FLOAT* TimeValues = NULL;
			switch( StatIndex )
			{
				case EGS_Render:
					TimeValues = GUnit_RenderThreadTimes.GetData();
					StatColor = FLinearColor( 0.1f, 0.1f, 1.0f );		// Blue
					break;

				case EGS_Game:
					TimeValues = GUnit_GameThreadTimes.GetData();
					StatColor = FLinearColor( 1.0f, 0.1f, 0.1f );		// Red
					break;

				case EGS_GPU:
					TimeValues = GUnit_GPUFrameTimes.GetData();
					StatColor = FLinearColor( 1.0f, 1.0f, 0.1f );		// Yellow
					break;

				case EGS_Frame:
					TimeValues = GUnit_FrameTimes.GetData();
					StatColor = FLinearColor( 0.1f, 1.0f, 0.1f );		// Green
					break;

#if PS3 && STATS
				case EGS_SPU:
					TimeValues = GUnit_SpuTimes.GetData();
					StatColor = FLinearColor( 1.0f, 0.1f, 1.0f );		// Violet
					break;
#endif // PS3 && STATS
			}


			// For each sample in our data set
			for( INT CurFrameIndex = 0; CurFrameIndex < GUnit_NumberOfSamples; ++CurFrameIndex )
			{
				const INT PrevFrameIndex = Max( 0, CurFrameIndex - 1 );

				INT PrevUnitIndex = ( GUnit_CurrentIndex - GUnit_NumberOfSamples ) + PrevFrameIndex;
				if( PrevUnitIndex < 0 )
				{
					PrevUnitIndex += GUnit_NumberOfSamples;
				}
				const FVector LineStart(
					GraphLeftXPos + (FLOAT)PrevFrameIndex * GraphHorizPixelsPerFrame,
					GraphBottomYPos - TimeValues[ PrevUnitIndex ] * GraphVerticalPixelsPerMS,
					0.0f );

				INT CurUnitIndex = ( GUnit_CurrentIndex - GUnit_NumberOfSamples ) + CurFrameIndex;
				if( CurUnitIndex < 0 )
				{
					CurUnitIndex += GUnit_NumberOfSamples;
				}
				const FVector LineEnd(
					GraphLeftXPos + (FLOAT)CurFrameIndex * GraphHorizPixelsPerFrame,
					GraphBottomYPos - TimeValues[ CurUnitIndex ] * GraphVerticalPixelsPerMS,
					0.0f );

				FLinearColor FinalLineColor = StatColor;
				if( TimeValues[ CurUnitIndex ] > AlertTimeMS )
				{
					// Alert!
					FinalLineColor.R *= AlertPulse;
					FinalLineColor.G *= AlertPulse;
					FinalLineColor.B *= AlertPulse;
				}

				BatchedElements->AddLine( LineStart, LineEnd, FinalLineColor, HitProxyId );
			}
		}
	}
#endif	// !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
}

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
static UBOOL ShowStableFrameTime()
{
	static IConsoleVariable* StableFrameTime = GConsoleManager->FindConsoleVariable(TEXT("StableFrameTime"));
	static IConsoleVariable* ProfileEx = GConsoleManager->FindConsoleVariable(TEXT("ProfileEx"));

	return StableFrameTime->GetInt() || ProfileEx->GetInt();
}
#endif

/**
 *	Draws frame times for the overall frame, gamethread, renderthread and GPU.
 *	The gamethread time excludes idle time while it's waiting for the render thread.
 *	The renderthread time excludes idle time while it's waiting for more commands from the gamethread or waiting for the GPU to swap backbuffers.
 *	The GPU time is a measurement on the GPU from the beginning of the first drawcall to the end of the last drawcall. It excludes
 *	idle time while waiting for VSYNC. However, it will include any starvation time between drawcalls.
 *
 *	@param Viewport	The viewport to render to
 *	@param Canvas	Canvas object to use for rendering
 *	@param X		Suggested X coordinate for where to start drawing
 *	@param Y		Suggested Y coordinate for where to start drawing
 *	@return			Y coordinate of the next line after this output
 */
INT DrawUnitTimes( FViewport* Viewport, FCanvas* Canvas, INT X, INT Y )
{
	/** How many cycles the renderthread used (excluding idle time). It's set once per frame in FViewport::Draw. */
	extern DWORD GRenderThreadTime;
	/** How many cycles the gamethread used (excluding idle time). It's set once per frame in FViewport::Draw. */
	extern DWORD GGameThreadTime;

	FLOAT FrameTime;
	if (GIsBenchmarking || GUseFixedTimeStep)
	{
		/** If we're in fixed time step mode, GCurrentTime will be incorrect for benchmarking */
		static DOUBLE LastTime = 0.0;
		const DOUBLE CurrentTime = appSeconds();
		if ( LastTime == 0 )
		{
			LastTime = CurrentTime;
		}
		FrameTime = CurrentTime - LastTime;
		LastTime = CurrentTime;
	}
	else
	{
		/** Use the FrameTime we computed last frame, because it correctly handles the end of frame idling and corresponds better to the other unit times. */
		FrameTime = GCurrentTime - GLastTime;
	}

	GUnit_RawFrameTime		= FrameTime * 1000.0f;

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	GUnit_StableFrameTime.PushValue(GUnit_RawFrameTime);
#endif

	GUnit_FrameTime			= 0.9 * GUnit_FrameTime + 0.1 * GUnit_RawFrameTime;

	/** Number of milliseconds the gamethread was used last frame. */
	GUnit_RawGameThreadTime = GGameThreadTime * GSecondsPerCycle * 1000.0;
	GUnit_GameThreadTime = 0.9 * GUnit_GameThreadTime + 0.1 * GUnit_RawGameThreadTime;
	appSetCounterValue( TEXT("Game thread time"), GGameThreadTime * GSecondsPerCycle * 1000.0 );

	/** Number of milliseconds the renderthread was used last frame. */
	GUnit_RawRenderThreadTime = GRenderThreadTime * GSecondsPerCycle * 1000.0;
	GUnit_RenderThreadTime = 0.9 * GUnit_RenderThreadTime + 0.1 * GUnit_RawRenderThreadTime;
	appSetCounterValue( TEXT("Render thread time"), GRenderThreadTime * GSecondsPerCycle * 1000.0 );

	/** Number of milliseconds the GPU was busy last frame. */
	const DWORD GPUCycles = RHIGetGPUFrameCycles();
	GUnit_RawGPUFrameTime = GPUCycles * GSecondsPerCycle * 1000.0;
	GUnit_GPUFrameTime = 0.9 * GUnit_GPUFrameTime + 0.1 * GUnit_RawGPUFrameTime;
	appSetCounterValue( TEXT("GPU time"), GPUCycles * GSecondsPerCycle * 1000.0 );

#if PS3 && STATS
	/** Number of milliseconds the SPU was busy last frame. */
	GUnit_SpuTime = 0.9f * GUnit_SpuTime + 0.1f * GUnit_RawSpuTime;
#endif // PS3 && STATS

	/** Push the unit timing stats through the STATS path, so they can be correlated with other things */
	STAT_MAKE_AVAILABLE_FAST_FLOAT(STAT_FPSChart_UnitFrame);
	STAT_MAKE_AVAILABLE_FAST_FLOAT(STAT_FPSChart_UnitRender);
	STAT_MAKE_AVAILABLE_FAST_FLOAT(STAT_FPSChart_UnitGame);
	STAT_MAKE_AVAILABLE_FAST_FLOAT(STAT_FPSChart_UnitGPU);

	SET_FLOAT_STAT_FAST(STAT_FPSChart_UnitFrame, GUnit_FrameTime);
	SET_FLOAT_STAT_FAST(STAT_FPSChart_UnitRender, GUnit_RenderThreadTime);
	SET_FLOAT_STAT_FAST(STAT_FPSChart_UnitGame, GUnit_GameThreadTime);
	SET_FLOAT_STAT_FAST(STAT_FPSChart_UnitGPU, GUnit_GPUFrameTime);

	FLOAT Max_RenderThreadTime = 0.0f;
	FLOAT Max_GameThreadTime = 0.0f;
	FLOAT Max_GPUFrameTime = 0.0f;
	FLOAT Max_FrameTime = 0.0f;
	FLOAT Max_SpuTime = 0.0f;

	UBOOL bShowUnitTimes = GShowUnitTimes;

#if !FINAL_RELEASE
	{
		static IConsoleVariable* ICVar = GConsoleManager->FindConsoleVariable(TEXT("ProfileEx"));

		if(ICVar && ICVar->GetInt())
		{
			// if ProfileEx is on, we want to see the frame time
			bShowUnitTimes = TRUE;
		}
	}
#endif

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	if(bShowUnitTimes)
	{
		GUnit_RenderThreadTimes(GUnit_CurrentIndex) = GShowRawUnitTimes ? GUnit_RawRenderThreadTime : GUnit_RenderThreadTime;
		GUnit_GameThreadTimes(GUnit_CurrentIndex) = GShowRawUnitTimes ? GUnit_RawGameThreadTime : GUnit_GameThreadTime;
		GUnit_GPUFrameTimes(GUnit_CurrentIndex) = GShowRawUnitTimes ? GUnit_RawGPUFrameTime : GUnit_GPUFrameTime;
		GUnit_FrameTimes(GUnit_CurrentIndex) = GShowRawUnitTimes ? GUnit_RawFrameTime : GUnit_FrameTime;
#if PS3 && STATS
		GUnit_SpuTimes(GUnit_CurrentIndex) = GShowRawUnitTimes ? GUnit_RawSpuTime : GUnit_SpuTime;
#endif // #if PS3 && STATS
		GUnit_CurrentIndex++;
		if (GUnit_CurrentIndex == GUnit_NumberOfSamples)
		{
			GUnit_CurrentIndex = 0;
		}

#if !FINAL_RELEASE
		if (GShowUnitMaxTimes)
		{
			for (INT MaxIndex = 0; MaxIndex < GUnit_NumberOfSamples; MaxIndex++)
			{
				if (Max_RenderThreadTime < GUnit_RenderThreadTimes(MaxIndex))
				{
					Max_RenderThreadTime = GUnit_RenderThreadTimes(MaxIndex);
				}
				if (Max_GameThreadTime < GUnit_GameThreadTimes(MaxIndex))
				{
					Max_GameThreadTime = GUnit_GameThreadTimes(MaxIndex);
				}
				if (Max_GPUFrameTime < GUnit_GPUFrameTimes(MaxIndex))
				{
					Max_GPUFrameTime = GUnit_GPUFrameTimes(MaxIndex);
				}
				if (Max_FrameTime < GUnit_FrameTimes(MaxIndex))
				{
					Max_FrameTime = GUnit_FrameTimes(MaxIndex);
				}
#if PS3 && STATS
				if( Max_SpuTime < GUnit_SpuTimes(MaxIndex) )
				{
					Max_SpuTime = GUnit_SpuTimes(MaxIndex);
				}
#endif // PS3 && STATS
			}
		}
#endif
	}
#endif	//#if !FINAL_RELEASE

	// Render CPU thread and GPU frame times.
	if(bShowUnitTimes)
	{
#if CONSOLE
		UFont* Font = GEngine->MediumFont ? GEngine->MediumFont : GEngine->SmallFont;
		const INT SafeZone	= appTrunc(Viewport->GetSizeX() * 0.05f);
#else
		UFont* Font = GEngine->SmallFont;
		const INT SafeZone	= 0;
#endif
		
		// Save original font scale
		FLOAT SavedFontScale = Font->GetFontScalingFactor();
#if STATS
		FLOAT FontScale = GStatManager.GetFontScale();
#else
		FLOAT FontScale = 1.0;
#endif


		GEngine->SmallFont->SetFontScalingFactor(FontScale);

		FColor Color;

		INT X3 = Viewport->GetSizeX() - SafeZone;

		if (GShowUnitMaxTimes)
		{
			X3 -= Font->GetStringSize(TEXT(" 0000.00 ms "));
		}

		const TCHAR* FrameTimeText = TEXT("Frame: ");
		const TCHAR* GameTimeText = TEXT("Game: ");
		const TCHAR* DrawTimeText = TEXT("Draw: ");
		const TCHAR* GpuTimeText = TEXT("GPU: ");

		FLOAT FrameTime = GUnit_FrameTime;

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		if(ShowStableFrameTime())
		{
			FrameTime = GUnit_StableFrameTime.GetCurrentStableValue();
			FrameTimeText = TEXT("stable FrameTime: ");
		}
#endif

		const INT MaxMsWidth = appTrunc(FontScale * Font->GetStringSize(TEXT(" 000.00 ms ")));
		const INT X2 = X3 - appTrunc(FontScale * Font->GetStringSize(TEXT(" 000.00 ms ")));
		const INT RowHeight = appTrunc(FontScale * Font->GetMaxCharHeight() * 1.1f);

		// 0-34 ms: Green, 34-50 ms: Yellow, 50+ ms: Red
		Color = FrameTime < 34.0f ? FColor(0,255,0) : (FrameTime < 50.0f ? FColor(255,255,0) : FColor(255,0,0));

		DrawShadowedString(Canvas,X2 - appTrunc(FontScale * Font->GetStringSize(FrameTimeText)), Y, FrameTimeText,Font,GShowUnitTimeGraph ? FColor(100,255,100) : FColor(255,255,255));
		DrawShadowedString(Canvas,X2,Y, *FString::Printf(TEXT("%3.2f ms"), FrameTime),Font,Color);

		if (GShowUnitMaxTimes)
		{
			DrawShadowedString(Canvas,X3,Y, *FString::Printf(TEXT("%4.2f ms"), Max_FrameTime),Font,Color);
		}

		Y += RowHeight;

		Color = GUnit_GameThreadTime < 34.0f ? FColor(0,255,0) : (GUnit_GameThreadTime < 50.0f ? FColor(255,255,0) : FColor(255,0,0));

		DrawShadowedString(Canvas,X2 - appTrunc(FontScale * Font->GetStringSize(GameTimeText)),Y, GameTimeText,Font,GShowUnitTimeGraph ? FColor(255,100,100) : FColor(255,255,255));
		DrawShadowedString(Canvas,X2,Y, *FString::Printf(TEXT("%3.2f ms"), GUnit_GameThreadTime),Font,Color);

		if (GShowUnitMaxTimes)
		{
			DrawShadowedString(Canvas,X3,Y, *FString::Printf(TEXT("%4.2f ms"), Max_GameThreadTime),Font,Color);
		}

		Y += RowHeight;

		Color = GUnit_RenderThreadTime < 34.0f ? FColor(0,255,0) : (GUnit_RenderThreadTime < 50.0f ? FColor(255,255,0) : FColor(255,0,0));

		DrawShadowedString(Canvas,X2 - appTrunc(FontScale * Font->GetStringSize(DrawTimeText)),Y, DrawTimeText,Font,GShowUnitTimeGraph ? FColor(100,100,255) : FColor(255,255,255));
		DrawShadowedString(Canvas,X2,Y, *FString::Printf(TEXT("%3.2f ms"), GUnit_RenderThreadTime),Font,Color);

		if (GShowUnitMaxTimes)
		{
			DrawShadowedString(Canvas,X3,Y, *FString::Printf(TEXT("%4.2f ms"), Max_RenderThreadTime),Font,Color);
		}

		Y += RowHeight;

		const UBOOL bHaveGPUData = GPUCycles > 0;

		if ( bHaveGPUData )
		{
			Color = GUnit_GPUFrameTime < 34.0f ? FColor(0,255,0) : (GUnit_GPUFrameTime < 50.0f ? FColor(255,255,0) : FColor(255,0,0));
			
			DrawShadowedString(Canvas,X2 - appTrunc(FontScale * Font->GetStringSize(GpuTimeText)),Y, GpuTimeText, Font, GShowUnitTimeGraph ? FColor(255,255,100) : FColor(255,255,255));
			DrawShadowedString(Canvas,X2,Y, *FString::Printf(TEXT("%3.2f ms"), GUnit_GPUFrameTime),Font,Color);
			
			if (GShowUnitMaxTimes)
			{
				DrawShadowedString(Canvas,X3,Y, *FString::Printf(TEXT("%4.2f ms"), Max_GPUFrameTime),Font,Color);
			}

			Y += RowHeight;

#if WIIU
			extern void appWiiUDrawGPUStats(FCanvas* Canvas, INT X1, INT X2, INT& Y, INT RowHeight, UFont* Font);
			appWiiUDrawGPUStats(Canvas, X2 - appTrunc(FontScale * Font->GetStringSize(FrameTimeText)), X2, Y, RowHeight, Font);
#endif
		}
		
#if PS3 && STATS
		const TCHAR* SpuTimeText = TEXT("SPU: ");

		// Show SPURS profiler stats. SPU usage / SPU count.
		Color = GUnit_SpuTime < 8.0f ? FColor(0,255,0) : (GUnit_SpuTime < 16.0f ? FColor(255,255,0) : FColor(255,0,0));

		DrawShadowedString(Canvas,X2 - appTrunc(FontScale * Font->GetStringSize(SpuTimeText)), Y, SpuTimeText, Font, GShowUnitTimeGraph ? FColor(255,100,255) : FColor(255,255,255));
		DrawShadowedString(Canvas,X2,Y, *FString::Printf( TEXT("%3.2f ms"), GUnit_SpuTime ), Font, Color);

		if( GShowUnitMaxTimes )
		{
			DrawShadowedString(Canvas,X3,Y, *FString::Printf(TEXT("%4.2f ms"), Max_SpuTime ), Font, Color);
		}

		Y += RowHeight;
#endif // PS3 && STATS

		// Restore original font scale
		Font->SetFontScalingFactor(SavedFontScale);

		// Draw the unit time graph, if that's enabled
		DrawUnitTimeGraph(Viewport, Canvas, bHaveGPUData);
	}

	return Y;
}

/**
 *	Renders stats
 *
 *	@param Viewport	The viewport to render to
 *	@param Canvas	Canvas object to use for rendering
 *	@param CanvasObject		Optional canvas object for visualizing properties
 *	@param DebugProperties	List of properties to visualize (in/out)
 *	@param ViewLocation	Location of camera
 *	@param ViewRotation	Rotation of camera
 */
void DrawStatsHUD( FViewport* Viewport, FCanvas* Canvas, UCanvas* CanvasObject, TArray<FDebugDisplayProperty>& DebugProperties, const FVector& ViewLocation, const FRotator& ViewRotation )
{
#if STATS
	DWORD DrawStatsBeginTime = appCycles();
#endif

	ENQUEUE_UNIQUE_RENDER_COMMAND( BeginDrawingStats, {extern UBOOL GIsDrawingStats; GIsDrawingStats = TRUE;} );

	//@todo joeg: Move this stuff to a function, make safe to use on consoles by
	// respecting the various safe zones, and make it compile out.
#if MOBILE
	const INT FPSXOffset	= 110;
	const INT StatsXOffset	= 4;
#elif CONSOLE
	const INT FPSXOffset	= 250;
	const INT StatsXOffset	= 100;
#else
	const INT FPSXOffset	= 110;
	const INT StatsXOffset	= 4;
#endif

#if !FINAL_RELEASE
	if( !GIsHighResScreenshot && !GIsTiledScreenshot && !GIsDumpingMovie && GAreScreenMessagesEnabled )
	{
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

		const INT MessageX = 40;
		INT MessageY = 100;
		if (!GEngine->bSuppressMapWarnings)
		{
			if (GShowVersion)
			{
				INT StringSize = DrawStringZ( Canvas, 0.0f, 0.0f, 1.0f, *Viewport->AppVersionString, GEngine->SmallFont, FLinearColor::Black, 1.0f, 1.0f, 0.0f, NULL, SE_BLEND_Translucent, FALSE );
				DrawShadowedString(Canvas,
					( INT )Viewport->GetSizeX() - StringSize - 16,
					16,
					*Viewport->AppVersionString,
					GEngine->SmallFont,
					FColor(255,255,0)
					);
			}

			if( GIsTextureMemoryCorrupted )
			{
				DrawShadowedString(Canvas,
					100,
					200,
					TEXT("RAN OUT OF TEXTURE MEMORY, EXPECT CORRUPTION AND GPU HANGS!"),
					GEngine->MediumFont,
					FColor(255,0,0)
					);
			}

			// Put the messages over fairly far to stay in the safe zone on consoles
			if( WorldInfo->bMapNeedsLightingFullyRebuilt )
			{
				FColor Color = FColor(128,128,128);
				// Color unbuilt lighting red if encountered within the last second
				if( GCurrentTime - WorldInfo->LastTimeUnbuiltLightingWasEncountered < 1 )
				{
					Color = FColor(255,0,0);
				}
				DrawShadowedString(Canvas,
					MessageX,
					MessageY,
					TEXT("LIGHTING NEEDS TO BE REBUILT"),
					GEngine->SmallFont,
					Color
					);
			}

			if( !WorldInfo->bPathsRebuilt )
			{
				MessageY += 20;
				DrawShadowedString(Canvas,
					MessageX,
					MessageY,
					TEXT("PATHS NEED TO BE REBUILT"),
					GEngine->SmallFont,
					FColor(128,128,128)
					);
			}

			if( WorldInfo->bMapHasPathingErrors )
			{
				MessageY += 20;
				DrawShadowedString(Canvas,
					MessageX,
					MessageY,
					TEXT("MAP HAS PATHING ERRORS"),
					GEngine->SmallFont,
					FColor(128,128,128)
					);
			}

			if (GWorld->bIsLevelStreamingFrozen)
			{
				MessageY += 20;
				DrawShadowedString(Canvas,
					MessageX,
					MessageY,
					TEXT("Level streaming frozen..."),
					GEngine->SmallFont,
					FColor(128,128,128)
					);
			}

			if (GIsPrepareMapChangeBroken)
			{
				MessageY += 20;
				DrawShadowedString(Canvas,
					MessageX,
					MessageY,
					TEXT("PrepareMapChange had a bad level name! Check the log (tagged with PREPAREMAPCHANGE) for info!"),
					GEngine->SmallFont,
					FColor(128,128,128)
					);
			}

#if XBOX || PS3
			// Warn if HD consoles are running in SD to make it obvious that performance won't be representative
			if (GbWarn_NotRunningInHD && ((GScreenWidth < 1280) || (GScreenHeight < 720)))
			{
				MessageY += 20;
				DrawShadowedString(Canvas,
					MessageX,
					MessageY,
					TEXT("NOT RUNNING IN HD!"),
					GEngine->SmallFont,
					FColor(255,0,0)
					);
			}
#endif
			if (
#if USE_GAMEPLAY_PROFILER
				FGameplayProfiler::IsActive() ||
#endif
#if STATS
				GStatManager.IsRecording() ||
#endif
				0 )
			{
				extern UBOOL GShouldVerifyGCAssumptions;
				if (!GEngine->bDisableAILogging)
				{				
					MessageY += 20;
					DrawShadowedString(Canvas,
						MessageX,
						MessageY,
						TEXT("PROFILING WITH AI LOGGING ON!"),
						GEngine->SmallFont,
						FColor(255,0,0)
						);
				}
				if (GShouldVerifyGCAssumptions)
				{
					MessageY += 20;
					DrawShadowedString(Canvas,
						MessageX,
						MessageY,
						TEXT("PROFILING WITH GC VERIFY ON!"),
						GEngine->SmallFont,
						FColor(255,0,0)
						);
				}
			}
			{
				FString Info;

				if(FLUTBlender::GetDebugInfo(Info))
				{
					MessageY += 20;
					DrawShadowedString(Canvas,
						MessageX,
						MessageY,
						*Info,
						GEngine->SmallFont,
						FColor(128,128,128)
						);
				}
			}
		}

		INT YPos = MessageY + 20;

		if (GEngine->bEnableOnScreenDebugMessagesDisplay && GEngine->bEnableOnScreenDebugMessages)
		{
			if (WorldInfo->PriorityScreenMessages.Num() > 0)
			{
				for (INT PrioIndex = WorldInfo->PriorityScreenMessages.Num() - 1; PrioIndex >= 0; PrioIndex--)
				{
					FScreenMessageString& Message = WorldInfo->PriorityScreenMessages(PrioIndex);
					if (YPos < 700)
					{
						DrawShadowedString(Canvas,
							MessageX,
							YPos,
							*(Message.ScreenMessage),
							GEngine->SmallFont,
							Message.DisplayColor
							);
						YPos += 20;
					}
					Message.CurrentTimeDisplayed += GWorld->GetDeltaSeconds();
					if (Message.CurrentTimeDisplayed >= Message.TimeToDisplay)
					{
						WorldInfo->PriorityScreenMessages.Remove(PrioIndex);
					}
				}
			}

			if (WorldInfo->ScreenMessages.Num() > 0)
			{
				for (TMap<INT, FScreenMessageString>::TIterator MsgIt(WorldInfo->ScreenMessages); MsgIt; ++MsgIt)
				{
					FScreenMessageString& Message = MsgIt.Value();
					if (YPos < 700)
					{
						DrawShadowedString(Canvas,
							MessageX,
							YPos,
							*(Message.ScreenMessage),
							GEngine->SmallFont,
							Message.DisplayColor
							);
						YPos += 20;
					}
					Message.CurrentTimeDisplayed += GWorld->GetDeltaSeconds();
					if (Message.CurrentTimeDisplayed >= Message.TimeToDisplay)
					{
						MsgIt.RemoveCurrent();
					}
				}
			}
		}
	}
#endif

	{
		INT X = Viewport->GetSizeX() - FPSXOffset;
#if MOBILE
		INT	Y = appTrunc(Viewport->GetSizeY() * 0.05f);
#else
		INT	Y = appTrunc(Viewport->GetSizeY() * 0.20f);
#endif

		//give the viewport first shot at drawing stats
		Y = Viewport->DrawStatsHUD(Canvas, X, Y);

#if DEBUGGING_VIEWPORT_SIZES
		// Useful for debugging viewport sizing/resizing, especially on external displays
		FString CurrentRes = FString::Printf(TEXT("W = %d, H = %d"), Viewport->GetSizeX(), Viewport->GetSizeY());
		DrawShadowedString(Canvas, 5, Y, *CurrentRes, GEngine->SmallFont, FColor(0,0,255));

		DrawShadowedString(Canvas, 5,                         5,                         TEXT("00"), GEngine->SmallFont, FColor(0,0,255));
		DrawShadowedString(Canvas, 5,                         Viewport->GetSizeY() - 25, TEXT("0M"), GEngine->SmallFont, FColor(0,0,255));
		DrawShadowedString(Canvas, Viewport->GetSizeX() - 25, 5,                         TEXT("M0"), GEngine->SmallFont, FColor(0,0,255));
		DrawShadowedString(Canvas, Viewport->GetSizeX() - 25, Viewport->GetSizeY() - 25, TEXT("MM"), GEngine->SmallFont, FColor(0,0,255));
#endif

		if( GCycleStatsShouldEmitNamedEvents )
		{
			Y = DrawShadowedString(Canvas, X, Y, TEXT("NAMED EVENTS ENABLED"), GEngine->SmallFont, FColor(0,0,255));
		}

		// Render the FPS counter.
		if( GShowFpsCounter )
		{
			Y = DrawFPSCounter( Viewport, Canvas, X, Y );
		}

		// Render the memory summary stats.
		if( GShowMemorySummaryStats )
		{
			Y = DrawMemorySummaryStats( Viewport, Canvas, X, Y );
		}

		// Render CPU thread and GPU frame times.
		Y = DrawUnitTimes( Viewport, Canvas, X, Y );

		// Render the Hitches.
		if( GShowHitches )
		{
			static DOUBLE LastTime = 0;
			const DOUBLE CurrentTime = appSeconds();
			if( LastTime > 0 )
			{
				static const INT NumHitches = 20;
				static FLOAT Hitches[NumHitches]={0.0f};
				static DOUBLE When[NumHitches]={0.0};
				static INT OverwriteIndex = 0;
				FLOAT DeltaSeconds = CurrentTime - LastTime;
				if (DeltaSeconds > 0.15f)
				{
					Hitches[OverwriteIndex] = DeltaSeconds;
					When[OverwriteIndex] = CurrentTime;
					OverwriteIndex = (OverwriteIndex + 1) % NumHitches;
					static INT Count = 0;
					warnf(TEXT("HITCH %d              running cnt = %5d"), INT(DeltaSeconds * 1000),Count++);
				}
				INT	MaxY = Viewport->GetSizeY();
				for (INT i = 0; i < NumHitches; i++)
				{
					static const DOUBLE TravelTime = 1.2;
					if (When[i] > 0 && When[i] <= CurrentTime && When[i] >= CurrentTime - TravelTime)
					{
						FColor MyColor = FColor(0,255,0);
						if (Hitches[i] > 0.2f) MyColor = FColor(255,255,0);
						if (Hitches[i] > 0.3f) MyColor = FColor(255,0,0);
						INT MyY = Y + INT(FLOAT(MaxY-Y) * FLOAT((CurrentTime - When[i])/TravelTime));
						FString Hitch = FString::Printf(TEXT("%5d"),INT(Hitches[i] * 1000.0f));
						DrawShadowedString(Canvas, X, MyY, *Hitch, GEngine->SmallFont, MyColor);
					}
				}
			}
			LastTime = CurrentTime;
		}

		if ( GShowAIStats )
		{
			Y = DrawAIStats(Viewport, Canvas, X, Y);
		}

		// Reset Y as above stats are rendered on right hand side.
		Y = 20;

		// Draw the color list table.
		Y = DrawColorListTable( Viewport, Canvas, StatsXOffset, Y );

		// Level stats.
		Y = DrawLevelStats( Viewport, Canvas, StatsXOffset, Y );

#if !FINAL_RELEASE
		// Active sound waves
		Y = DrawSoundWaves( Viewport, Canvas, StatsXOffset, Y );

		// Active sound cues
		Y = DrawSoundCues( Viewport, Canvas, StatsXOffset, Y );

		// Active sound waves and sound cues
		Y = DrawSounds( Viewport, Canvas, StatsXOffset, Y );
#endif

#if PS3 && STATS
		extern UBOOL GShowPS3_SPURSProfilerGraphs;
		if( GShowPS3_SPURSProfilerGraphs )
		{
			Y = FSpursProfilerManager::DebugDraw( Canvas, X, Y, Viewport->GetSizeX()-32, Viewport->GetSizeY()-32 );
		}
#endif // PS3 && STATS

#if STATS
		// Render the Stats System UI
		GStatManager.Render( Canvas, StatsXOffset, Y );
#endif
	}

	// Draw top-down streaming level map
	DrawLevelStatusMap( Canvas, FVector2D(512,128), FVector2D(512,512), ViewLocation, ViewRotation );

	// Render the stat chart.
	if(GStatChart)
	{
		GStatChart->Render(Viewport, Canvas);
	}


	// draw debug properties
#if !FINAL_RELEASE
#if SHIPPING_PC_GAME
	if (GWorld != NULL && GWorld->GetNetMode() == NM_Standalone && CanvasObject != NULL)
#endif
	{
		// construct a list of objects relevant to "getall" type elements, so that we only have to do the object iterator once
		// we do the iterator each frame so that new objects will show up immediately
		TArray<UClass*> DebugClasses;
		DebugClasses.Reserve(DebugProperties.Num());
		for (INT i = 0; i < DebugProperties.Num(); i++)
		{
			if (DebugProperties(i).Obj != NULL && !DebugProperties(i).Obj->IsPendingKill())
			{
				UClass* Cls = Cast<UClass>(DebugProperties(i).Obj);
				if (Cls != NULL)
				{
					DebugClasses.AddItem(Cls);
				}
			}
			else
			{
				// invalid, object was destroyed, etc. so remove the entry
				DebugProperties.Remove(i--, 1);
			}
		}
		TArray<UObject*> RelevantObjects;
		if (DebugClasses.Num() > 0)
		{
			for (TObjectIterator<UObject> It(TRUE); It; ++It)
			{
				for (INT i = 0; i < DebugClasses.Num(); i++)
				{
					if (It->IsA(DebugClasses(i)) && !It->IsTemplate())
					{
						RelevantObjects.AddItem(*It);
						break;
					}
				}
			}
		}
		// draw starting in the top left
		INT X = StatsXOffset;
#if CONSOLE
		INT Y = 40;
#else
		INT Y = 20;
#endif
		INT MaxY = INT(Canvas->GetRenderTarget()->GetSizeY());
		for (INT i = 0; i < DebugProperties.Num() && Y < MaxY; i++)
		{
			// we removed entries with invalid Obj above so no need to check for that here
			UClass* Cls = Cast<UClass>(DebugProperties(i).Obj);
			if (Cls != NULL)
			{
				UProperty* Prop = FindField<UProperty>(Cls, DebugProperties(i).PropertyName);
				if (Prop != NULL || DebugProperties(i).bSpecialProperty)
				{
					// getall
					for (INT j = 0; j < RelevantObjects.Num(); j++)
					{
						if (RelevantObjects(j)->IsA(Cls) && !RelevantObjects(j)->IsPendingKill())
						{
							DrawProperty(CanvasObject, RelevantObjects(j), DebugProperties(i), Prop, X, Y);
						}
					}
				}
				else
				{
					// invalid entry
					DebugProperties.Remove(i--, 1);
				}
			}
			else
			{
				UProperty* Prop = FindField<UProperty>(DebugProperties(i).Obj->GetClass(), DebugProperties(i).PropertyName);
				if (Prop != NULL || DebugProperties(i).bSpecialProperty)
				{
					DrawProperty(CanvasObject, DebugProperties(i).Obj, DebugProperties(i), Prop, X, Y);
				}
				else
				{
					DebugProperties.Remove(i--, 1);
				}
			}
		}
	}
#endif

#if PS3 && !USE_NULL_RHI
	if( GShowPS3GcmMemoryStats )
	{
		const FLOAT BorderSize = 0.05f; //pct
		FLOAT X = Viewport->GetSizeX()*BorderSize;
		FLOAT Y = Viewport->GetSizeY()*BorderSize;
		FLOAT SizeX = Viewport->GetSizeX() - Viewport->GetSizeX()*BorderSize*2;
		FLOAT SizeY = GEngine ? GEngine->TinyFont->GetMaxCharHeight()*2 : 80.f;
		// draw graph of all current local allocs and free regions
		GPS3Gcm->GetLocalAllocator()->DebugDraw(Canvas,X,Y,SizeX,SizeY, TEXT("Local"));
		// draw graph of all current host allocs and free regions
		Y += SizeY*2;
		for( INT HostMemIdx=0; HostMemIdx < HostMem_MAX; HostMemIdx++ )
		{
			const EHostMemoryHeapType HostMemType = (EHostMemoryHeapType)HostMemIdx; 
			if( GPS3Gcm->HasHostAllocator(HostMemType) )
			{
				GPS3Gcm->GetHostAllocator(HostMemType)->DebugDraw(Canvas,X,Y,SizeX,SizeY, GPS3Gcm->GetHostAllocatorDesc(HostMemType), HostMemIdx==(HostMem_MAX-1));
			}
		}
		Canvas->Flush();
	}
#endif

	// Flush all the stats rendering commands so we can ensure GIsDrawingStats is set while they are rendered
	Canvas->Flush();
	ENQUEUE_UNIQUE_RENDER_COMMAND( FinishDrawingStats, {extern UBOOL GIsDrawingStats; GIsDrawingStats = FALSE;} );

#if STATS
	DWORD DrawStatsEndTime = appCycles();
	SET_CYCLE_COUNTER(STAT_DrawStats, DrawStatsEndTime - DrawStatsBeginTime, 1);
#endif
}



/**
 * Stats objects for Engine
 */
DECLARE_STATS_GROUP(TEXT("Engine"),STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("FrameTime"),STAT_FrameTime,STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("GameEngine Tick"),STAT_GameEngineTick,STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("GameViewport Tick"),STAT_GameViewportTick,STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("RedrawViewports"),STAT_RedrawViewports,STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("Update Level Streaming"),STAT_UpdateLevelStreaming,STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("RHI Game Tick"),STAT_RHITickTime,STATGROUP_Engine);

/** Terrain stats */
DECLARE_CYCLE_STAT(TEXT("Terrain Smooth Time"),STAT_TerrainSmoothTime,STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("Terrain Render Time"),STAT_TerrainRenderTime,STATGROUP_Engine);
DECLARE_DWORD_COUNTER_STAT(TEXT("Terrain Triangles"),STAT_TerrainTriangles,STATGROUP_Engine);

/** Landscape stats */
DECLARE_STATS_GROUP(TEXT("Landscape"),STATGROUP_Landscape);
DECLARE_CYCLE_STAT(TEXT("Landscape Dynamic Draw Time"),STAT_LandscapeDynamicDrawTime,STATGROUP_Landscape);
DECLARE_CYCLE_STAT(TEXT("Landscape Static Draw LOD Time"),STAT_LandscapeStaticDrawLODTime,STATGROUP_Landscape);
DECLARE_CYCLE_STAT(TEXT("LandscapeVF Draw Time"),STAT_LandscapeVFDrawTime,STATGROUP_Landscape);
DECLARE_DWORD_COUNTER_STAT(TEXT("Landscape Render Passes"),STAT_LandscapeComponents,STATGROUP_Landscape);
DECLARE_DWORD_COUNTER_STAT(TEXT("Landscape DrawCalls"),STAT_LandscapeDrawCalls,STATGROUP_Landscape);
DECLARE_DWORD_COUNTER_STAT(TEXT("Landscape Triangles"),STAT_LandscapeTriangles,STATGROUP_Landscape);
DECLARE_MEMORY_STAT(TEXT("Heightmap Texture Mem"),STAT_LandscapeHeightmapTextureMem,STATGROUP_Landscape);
DECLARE_MEMORY_STAT(TEXT("Weightmap Texture Mem"),STAT_LandscapeWeightmapTextureMem,STATGROUP_Landscape);
DECLARE_DWORD_COUNTER_STAT(TEXT("Component Mem"),STAT_LandscapeComponentMem,STATGROUP_Landscape);

/** Input stat */
DECLARE_CYCLE_STAT(TEXT("Input Time"),STAT_InputTime,STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("Input Latency"),STAT_InputLatencyTime,STATGROUP_Engine);

/** HUD stat */
DECLARE_CYCLE_STAT(TEXT("HUD Time"),STAT_HudTime,STATGROUP_Engine);

/** Static mesh tris rendered */
DECLARE_DWORD_COUNTER_STAT(TEXT("Static Mesh Tris"),STAT_StaticMeshTriangles,STATGROUP_Engine);

/** Skeletal stats */
DECLARE_CYCLE_STAT(TEXT("Skel Skin Time"),STAT_SkinningTime,STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("Update Cloth Verts Time"),STAT_UpdateClothVertsTime,STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("Update SoftBody Verts Time"),STAT_UpdateSoftBodyVertsTime,STATGROUP_Engine);
DECLARE_DWORD_COUNTER_STAT(TEXT("Skel Mesh Tris"),STAT_SkelMeshTriangles,STATGROUP_Engine);
DECLARE_DWORD_COUNTER_STAT(TEXT("Skel Mesh Draw Calls"),STAT_SkelMeshDrawCalls,STATGROUP_Engine);
DECLARE_DWORD_COUNTER_STAT(TEXT("Skel Verts CPU Skin"),STAT_CPUSkinVertices,STATGROUP_Engine);
DECLARE_DWORD_COUNTER_STAT(TEXT("Skel Verts GPU Skin"),STAT_GPUSkinVertices,STATGROUP_Engine);

/** Fracture */
DECLARE_DWORD_COUNTER_STAT(TEXT("Fracture Part Pool Used"),STAT_FracturePartPoolUsed,STATGROUP_Engine);

/** Frame chart stats */
DECLARE_STATS_GROUP(TEXT("FPSChart"),STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("00 - 05 FPS"), STAT_FPSChart_0_5,		STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("05 - 10 FPS"), STAT_FPSChart_5_10,		STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("10 - 15 FPS"), STAT_FPSChart_10_15,	STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("15 - 20 FPS"), STAT_FPSChart_15_20,	STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("20 - 25 FPS"), STAT_FPSChart_20_25,	STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("25 - 30 FPS"), STAT_FPSChart_25_30,	STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("30 - 35 FPS"), STAT_FPSChart_30_35,	STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("35 - 40 FPS"), STAT_FPSChart_35_40,	STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("40 - 45 FPS"), STAT_FPSChart_40_45,	STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("45 - 50 FPS"), STAT_FPSChart_45_50,	STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("50 - 55 FPS"), STAT_FPSChart_50_55,	STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("55 - 60 FPS"), STAT_FPSChart_55_60,	STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("60 - .. FPS"), STAT_FPSChart_60_INF,	STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("30+ FPS"), STAT_FPSChart_30Plus,	STATGROUP_FPSChart);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Unaccounted time"), STAT_FPSChart_UnaccountedTime,	STATGROUP_FPSChart);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Frame count"),	STAT_FPSChart_FrameCount, STATGROUP_FPSChart);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("5.0s  - ....  hitches"), STAT_FPSChart_Hitch_5000_Plus, STATGROUP_FPSChart);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("2.5s  - 5.0s  hitches"), STAT_FPSChart_Hitch_2500_5000, STATGROUP_FPSChart);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("2.0s  - 2.5s  hitches"), STAT_FPSChart_Hitch_2000_2500, STATGROUP_FPSChart);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("1.5s  - 2.0s  hitches"), STAT_FPSChart_Hitch_1500_2000, STATGROUP_FPSChart);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("1.0s  - 1.5s  hitches"), STAT_FPSChart_Hitch_1000_1500, STATGROUP_FPSChart);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("0.75s - 1.0s  hitches"), STAT_FPSChart_Hitch_750_1000, STATGROUP_FPSChart);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("0.5s  - 0.75s hitches"), STAT_FPSChart_Hitch_500_750, STATGROUP_FPSChart);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("0.3s  - 0.5s  hitches"), STAT_FPSChart_Hitch_300_500, STATGROUP_FPSChart);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("0.2s  - 0.3s  hitches"), STAT_FPSChart_Hitch_200_300, STATGROUP_FPSChart);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("0.15s - 0.2s  hitches"), STAT_FPSChart_Hitch_150_200, STATGROUP_FPSChart);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("0.1s  - 0.15s hitches"), STAT_FPSChart_Hitch_100_150, STATGROUP_FPSChart);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total hitches"), STAT_FPSChart_TotalHitchCount, STATGROUP_FPSChart);

DECLARE_FLOAT_COUNTER_STAT_FAST(TEXT("StatUnit FrameTime"), STAT_FPSChart_UnitFrame, STATGROUP_FPSChart);
DECLARE_FLOAT_COUNTER_STAT_FAST(TEXT("StatUnit GameThreadTime"), STAT_FPSChart_UnitGame, STATGROUP_FPSChart);
DECLARE_FLOAT_COUNTER_STAT_FAST(TEXT("StatUnit RenderThreadTime"), STAT_FPSChart_UnitRender, STATGROUP_FPSChart);
DECLARE_FLOAT_COUNTER_STAT_FAST(TEXT("StatUnit GPUTime"), STAT_FPSChart_UnitGPU, STATGROUP_FPSChart);

/*-----------------------------------------------------------------------------
	Lightmass object/actor implementations.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UDEPRECATED_LightmassLevelSettings);
IMPLEMENT_CLASS(ULightmassPrimitiveSettingsObject);
IMPLEMENT_CLASS(ALightmassImportanceVolume);
IMPLEMENT_CLASS(APrecomputedVisibilityVolume);
IMPLEMENT_CLASS(APrecomputedVisibilityOverrideVolume);
IMPLEMENT_CLASS(ALightmassCharacterIndirectDetailVolume);

/*-----------------------------------------------------------------------------
	ULightmappedSurfaceCollection
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(ULightmappedSurfaceCollection);


/*-----------------------------------------------------------------------------
	AMassiveLODOverrideVolume
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(AMassiveLODOverrideVolume);

/**
 * Removes the volume from world info's list of volumes.
 */
void AMassiveLODOverrideVolume::ClearComponents( void )
{
	// Route clear to super first.
	Super::ClearComponents();

	// GWorld will be NULL during exit purge.
	if( GWorld )
	{
		TArrayNoInit<AMassiveLODOverrideVolume*>& WorldVolumes = GWorld->GetWorldInfo()->MassiveLODOverrideVolumes;
		WorldVolumes.RemoveItem(this);
	}
}

/**
 * Adds the volume to world info's list of volumes.
 */
void AMassiveLODOverrideVolume::UpdateComponentsInternal( UBOOL bCollisionUpdate )
{
	// Route update to super first.
	Super::UpdateComponentsInternal( bCollisionUpdate );

	if( GWorld )
	{
		TArrayNoInit<AMassiveLODOverrideVolume*>& WorldVolumes = GWorld->GetWorldInfo()->MassiveLODOverrideVolumes;
		WorldVolumes.AddUniqueItem(this);
	}
}

IMPLEMENT_CLASS(UHttpBaseInterface);
IMPLEMENT_CLASS(UHttpRequestInterface);
IMPLEMENT_CLASS(UHttpResponseInterface);

static void HttpPureVirtual(const TCHAR* FunctionName)
{
	appErrorf(TEXT("%s: Attempt to call base function on HttpBase interface. Must completely define all inherited functions when inheriting from an HttpBase class"), FunctionName);
}

FString UHttpBaseInterface::GetHeader(const FString& HeaderName)
{
	HttpPureVirtual(TEXT("GetHeader"));
	return FString();
}
TArray<FString> UHttpBaseInterface::GetHeaders()
{
	HttpPureVirtual(TEXT("GetHeaders"));
	return TArray<FString>();
}

FString UHttpBaseInterface::GetURLParameter(const FString& ParameterName)
{
	HttpPureVirtual(TEXT("GetURLParameter"));
	return FString();
}

FString UHttpBaseInterface::GetContentType()
{
	HttpPureVirtual(TEXT("GetContentType"));
	return FString();
}
INT UHttpBaseInterface::GetContentLength()
{
	HttpPureVirtual(TEXT("GetContentLength"));
	return 0;
}
FString UHttpBaseInterface::GetURL()
{
	HttpPureVirtual(TEXT("GetURL"));
	return FString();
}
void UHttpBaseInterface::GetContent(TArray<BYTE>& Content)
{
	HttpPureVirtual(TEXT("GetContent"));
}
FString UHttpRequestInterface::GetVerb()
{
	HttpPureVirtual(TEXT("GetVerb"));
	return FString();
}
UHttpRequestInterface* UHttpRequestInterface::SetVerb(const FString& Verb)
{
	HttpPureVirtual(TEXT("SetVerb"));
	return this;
}
UHttpRequestInterface* UHttpRequestInterface::SetURL(const FString& URL)
{
	HttpPureVirtual(TEXT("SetURL"));
	return this;
}
UHttpRequestInterface* UHttpRequestInterface::SetContent(const TArray<BYTE>& ContentPayload)
{
	HttpPureVirtual(TEXT("SetContent"));
	return this;
}
UHttpRequestInterface* UHttpRequestInterface::SetContentAsString(const FString& ContentString)
{
	HttpPureVirtual(TEXT("SetContentAsString"));
	return this;
}
UHttpRequestInterface* UHttpRequestInterface::SetHeader(const FString& HeaderName,const FString& HeaderValue)
{
	HttpPureVirtual(TEXT("SetHeader"));
	return this;
}
UBOOL UHttpRequestInterface::ProcessRequest()
{
	HttpPureVirtual(TEXT("ProcessRequest"));
	return FALSE;
}
INT UHttpResponseInterface::GetResponseCode()
{
	HttpPureVirtual(TEXT("GetResponseCode"));
	return 0;
}
FString UHttpResponseInterface::GetContentAsString()
{
	HttpPureVirtual(TEXT("GetContentAsString"));
	return FString();
}

