/*=============================================================================
	LaunchPrivate.h: Unreal launcher.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#define DEMOGAME	1
#define WARGAME		2
#define UTGAME		3

//@warning: this needs to be the very first include
#include "UnrealEd.h"

#include "Engine.h"
#include "EngineMaterialClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineParticleClasses.h"
#include "EngineAIClasses.h"
#include "EngineAnimClasses.h"
#include "UnTerrain.h"
#include "UnCodecs.h"
#include "UnIpDrv.h"
#if   GAMENAME == DEMOGAME
#include "DemoGameClasses.h"
#elif GAMENAME == WARGAME
#include "WarfareGameClasses.h"
#elif GAMENAME == UTGAME
#include "UTGameClasses.h"
#else
	#error Hook up your game name here
#endif
#include "EditorPrivate.h"
#include "ALAudio.h"
#include "D3DDrv.h"
#include "WinDrv.h"
#include "Window.h"
#include "FMallocAnsi.h"
#include "FMallocDebug.h"
#include "FMallocWindows.h"
#include "FMallocDebugProxyWindows.h"
#include "FMallocThreadSafeProxy.h"
#include "FOutputDeviceDebug.h"
#include "FOutputDeviceAnsiError.h"
#include "FFeedbackContextAnsi.h"
#include "FOutputDeviceFile.h"
#include "FOutputDeviceWindowsError.h"
#include "FOutputDeviceConsoleWindows.h"
#include "FFeedbackContextWindows.h"
#include "FFileManagerWindows.h"
#include "FCallbackDevice.h"
#include "FConfigCacheIni.h"
#include "LaunchEngineLoop.h"
#include "UnThreadingWindows.h"

#define KEEP_ALLOCATION_BACKTRACE	0

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
