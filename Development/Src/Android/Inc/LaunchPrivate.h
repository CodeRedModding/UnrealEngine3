/**
 * LaunchPrivate.h: Android version
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "../../Launch/Inc/LaunchGames.h"
#include "Engine.h"
#include "UnIpDrv.h"
#include "DemoRecording.h" // TODO: Why do we need this? LaunchEngineLoop.cpp should include this, not us.

// Includes for CIS.
#if CHECK_NATIVE_CLASS_SIZES
#include "EngineMaterialClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineParticleClasses.h"
#include "EngineAIClasses.h"
#include "EngineAnimClasses.h"
#include "EngineDecalClasses.h"
#include "EngineFogVolumeClasses.h"
#include "EngineMeshClasses.h"
#include "EnginePrefabClasses.h"
#include "EngineSplineClasses.h"
#include "EngineProcBuildingClasses.h"
#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "EngineFoliageClasses.h"
#include "EngineSpeedTreeClasses.h"
#include "UnTerrain.h"
#include "UnCodecs.h"
#include "UnNet.h"
#include "UnrealScriptTestClasses.h"
#include "GameFrameworkClasses.h"
#include "GameFrameworkAnimClasses.h"
#include "GameFrameworkCameraClasses.h"


#if GAMENAME == WARGAME
#include "WarfareGameClasses.h"
#include "WarfareGameCameraClasses.h"
#include "WarfareGameSequenceClasses.h"
#include "WarfareGameSpecialMovesClasses.h"
#include "WarfareGameVehicleClasses.h"
#include "WarfareGameSoundClasses.h"
#include "WarfareGameAIClasses.h"
#elif GAMENAME == GEARGAME
#include "GearGameClasses.h"
#include "GearGameAIClasses.h"
#include "GearGamePCClasses.h"
#include "GearGamePawnClasses.h"
#include "GearGameCameraClasses.h"
#include "GearGameSequenceClasses.h"
#include "GearGameSpecialMovesClasses.h"
#include "GearGameVehicleClasses.h"
#include "GearGameSoundClasses.h"
#include "GearGameWeaponClasses.h"
#include "GearGameUIClasses.h"
#include "GearGameUIPrivateClasses.h"
#include "GearGameSpawnerClasses.h"
#elif GAMENAME == UTGAME
#include "UDKBaseClasses.h"
#include "UDKBaseAnimationClasses.h"
#elif GAMENAME == DUKEGAME
#include "DukeGameClasses.h"
#elif GAMENAME == EXOGAME
#include "ExoGameClasses.h"
#else
#error Hook up your game name here
#endif

#endif // CHECK_NATIVE_CLASS_SIZES

#include "AndroidThreading.h"
#include "FMallocAnsi.h"
#include "FMallocProfiler.h"
#include "ScriptCallstackDecoder.h"
#include "MallocProfilerEx.h"
#include "FMallocProxySimpleTrack.h"
#include "FMallocProxySimpleTag.h"
#include "FMallocThreadSafeProxy.h"
#include "FFeedbackContextAnsi.h"
#include "FFileManagerAndroid.h"
#include "FCallbackDevice.h"
#include "FConfigCacheIni.h"
#include "AsyncLoadingAndroid.h"
#include "../../Launch/Inc/LaunchEngineLoop.h"
#include "AndroidFullScreenMovie.h"

