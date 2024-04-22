/*=============================================================================
	UnBuild.h: Unreal build settings.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _HEADER_UNBUILD_H_
#define _HEADER_UNBUILD_H_

// Include defining EPIC_INTERNAL. We use P4 permissions to mask out our version which has it set to 1.
#include "BaseInclude.h"

/**
 * Whether we want the slimmest possible build of UE3 or not. Don't modify directly but rather change UE3BuildConfiguration.cs in UBT.
 */
#ifndef UE3_LEAN_AND_MEAN
#define UE3_LEAN_AND_MEAN 0
#endif

/**
 * Whether we are in "ship" mode on the PC and therefore want to disable certain debug functionality. Don't modify directly.
 */
#ifndef SHIPPING_PC_GAME
#define SHIPPING_PC_GAME 0
#endif

/**
 * Whether we are compiling the UDK. Don't change the option here but either pass into UBT or change it there.
 */
#ifndef UDK 
#define UDK	0
#endif

#if FINAL_RELEASE_DEBUGCONSOLE
#define ALLOW_TRACEDUMP		XBOX
#define DO_CHECK			0
#define STATS				0
#define ALLOW_NON_APPROVED_FOR_SHIPPING_LIB 1  // this is to allow us to link against libs that we can no ship with (e.g. TCR unapproved libs)
#elif FINAL_RELEASE
#define ALLOW_TRACEDUMP		0
#define DO_CHECK			0
#define STATS				0
#define ALLOW_NON_APPROVED_FOR_SHIPPING_LIB 0  // this is to allow us to link against libs that we can no ship with (e.g. TCR unapproved libs)
#else
#define ALLOW_TRACEDUMP		XBOX
#define DO_CHECK			(!(SHIPPING_PC_GAME || UE3_LEAN_AND_MEAN))
// NGP: stats thread is currently causing threading bottlenecks
#define STATS				(UDK || !(SHIPPING_PC_GAME || UE3_LEAN_AND_MEAN || NGP || FLASH))
#define ALLOW_NON_APPROVED_FOR_SHIPPING_LIB 1  // this is to allow us to link against libs that we can no ship with (e.g. TCR unapproved libs)
#endif

/**
 * Whether the game is a demo version and therefore e.g. want to disable auto download and such.
 */
#ifndef DEMOVERSION
#define DEMOVERSION	0
#endif

/**
 * Set if you're debugging memory issues. Enables things like check slow, removes array slack, etc.
 */
#ifndef DEBUG_MEMORY_ISSUES
#define DEBUG_MEMORY_ISSUES 0
#endif

/**
 * DO_GUARD_SLOW is enabled for debug builds which enables checkSlow, appErrorfDebug, ...
 **/
#ifdef _DEBUG
#define DO_GUARD_SLOW	1
#else
#define DO_GUARD_SLOW	(0 || DEBUG_MEMORY_ISSUES)
#endif


/**
 * DO_GUARD_SLOWISH is enabled when you want to do minimal checks on certain platforms
 * - currently disabled in release/final on consoles, and enabled on PC except for FINAL_RELEASE
 **/
#if (!CONSOLE && !FINAL_RELEASE) || defined(_DEBUG)
#define DO_GUARD_SLOWISH	1
#else
#define DO_GUARD_SLOWISH	(0 || DEBUG_MEMORY_ISSUES)
#endif


/**
 * Whether to enable debugfSlow
 */
#ifndef DO_LOG_SLOW
#define DO_LOG_SLOW		0
#endif


/**
 * Checks to see if pure virtual has actually been implemented
 *
 * @see Core.h
 * @see UnObjBas.h
 **/
#ifndef CHECK_PUREVIRTUALS
#define CHECK_PUREVIRTUALS 0
#endif


/**
 * Checks for native class script/ C++ mismatch of class size and member variable
 * offset.
 * 
 * @see CheckNativeClassSizes()
 **/
#ifndef CHECK_NATIVE_CLASS_SIZES
#define CHECK_NATIVE_CLASS_SIZES 0
#endif


/**
 * Warn if native function doesn't actually exist.
 *
 * @see CompileFunctionDeclaration
 **/
#ifndef CHECK_NATIVE_MATCH
#define CHECK_NATIVE_MATCH 0
#endif

/** 
 *   Whether compiling for dedicated server or not
 */
#ifndef DEDICATED_SERVER
#define DEDICATED_SERVER 0
#endif

/**
 * Whether to use Substance Air or not
 */
#ifndef WITH_SUBSTANCE_AIR
	#define WITH_SUBSTANCE_AIR 0
#endif

/**
 * Whether to use the PhysX physics engine.
 * NB: Unreal simplified collision relies on PhysX to pre-process convex hulls to generate face/edge data.
 **/
#ifndef WITH_NOVODEX
#define WITH_NOVODEX 1
#endif

#if IPHONE || ANDROID || NGP || FLASH
#define WITH_PHYSX_COOKING 1 // Temporarily re-enabled.  disable physx cooking when building for the iphone
#else
#define WITH_PHYSX_COOKING 1
#endif

#ifndef WITH_APEX
#define WITH_APEX (WITH_NOVODEX && !CONSOLE && !DEDICATED_SERVER && WITH_PHYSX_COOKING && !PLATFORM_MACOSX)
#endif

#ifndef WITH_APEX_DESTRUCTIBLE
#define WITH_APEX_DESTRUCTIBLE WITH_APEX
#endif

#ifndef WITH_APEX_GRB
#define WITH_APEX_GRB WITH_APEX_DESTRUCTIBLE && !CONSOLE
#endif

#ifndef WITH_APEX_CLOTHING
#define WITH_APEX_CLOTHING WITH_APEX
#endif

#ifndef WITH_APEX_PARTICLES
#define WITH_APEX_PARTICLES 0//WITH_APEX
#endif

#ifndef WITH_APEX_EMITTER
#define WITH_APEX_EMITTER WITH_APEX_PARTICLES
#endif

#ifndef WITH_APEX_IOFX
#define WITH_APEX_IOFX WITH_APEX_PARTICLES
#endif

#ifndef WITH_APEX_BASIC_IOS
#define WITH_APEX_BASIC_IOS WITH_APEX_PARTICLES
#endif

#ifndef WITH_APEX_SHIPPING

// The shipping build of APEX removes editor and debug rendering support to reduce library size on consoles
#if FINAL_RELEASE

#if _XBOX || PS3
#define WITH_APEX_SHIPPING 1
#else
#define WITH_APEX_SHIPPING 0
#endif

#else

#define WITH_APEX_SHIPPING 0

#endif

#endif

/**
 * Whether to use OggVorbis audio format.
 **/
#ifndef WITH_OGGVORBIS
#if _XBOX || PS3
#define WITH_OGGVORBIS 0
#else
#define WITH_OGGVORBIS 1
#endif
#endif


/**
 * Whether to tie Ageia's Performance Monitor into Unreal's timers.
 * AGPerfmon is only supported on Win32 platforms.
 **/

#if !defined(_WINDOWS) && !defined(_WIN64)
#ifdef USE_AG_PERFMON
#undef USE_AG_PERFMON
#endif
#endif

#ifndef USE_AG_PERFMON
#define USE_AG_PERFMON 0
#endif


// to turn off novodex we just set WITH_NOVODEX to 0
#if !WITH_NOVODEX && !_XBOX  // this is turned off by novodex elsewhere on xbox
#define NX_DISABLE_FLUIDS 1
#endif


// Disable certain PhysX features on mobile platforms
// Note: Disabling Cloth and Softbody in PhysX is not supported when using APEX.
#if IPHONE || ANDROID || NGP || FLASH
	#define NX_DISABLE_FLUIDS 1
	#define NX_DISABLE_CLOTH 1
	#define NX_DISABLE_SOFTBODY 1
#endif


/**
 * Whether to use D3D11+ Tessellation.
 *
 */
// Turn off unless PC Windows
#if !CONSOLE //&& !UE3_LEAN_AND_MEAN && defined(_WINDOWS)
#define WITH_D3D11_TESSELLATION 1
#endif


/**
 * Whether to use the FaceFX Facial animation engine.
 *
 * to compile without FaceFX by doing the following:
 *    #define WITH_FACEFX 0
 *    remove/rename the FaceFX directory ( Development\External\FaceFX )
 *
 **/
#ifndef WITH_FACEFX
#define WITH_FACEFX (!UE3_LEAN_AND_MEAN)
#endif


/**
 * Whether to use the FaceFX Editor integration via FaceFX Studio.
 **/
#ifndef WITH_FACEFX_STUDIO
#define WITH_FACEFX_STUDIO WITH_FACEFX
#endif


/** 
 * Whether to include the Fonix speech recognition library
 */
#ifndef WITH_SPEECH_RECOGNITION
#if _WIN64 || UDK
//@todo win64: enable once there is a 64 bit version
#define WITH_SPEECH_RECOGNITION 0
#else
#define WITH_SPEECH_RECOGNITION ((EPIC_INTERNAL) && (!UE3_LEAN_AND_MEAN) && (!FINAL_RELEASE))
#endif
#endif


/** 
 * Whether to include support for Fonix's TTS
 */
#ifndef WITH_TTS
#if PS3 || _WIN64
//@todo win64: enable once there is a 64 bit version
#define WITH_TTS 0
#else
#define WITH_TTS ((!UE3_LEAN_AND_MEAN) && (!FINAL_RELEASE))
#endif
#endif


/** 
 * Whether to include support for IME
 */
#ifndef WITH_IME
#define WITH_IME 1
#endif


/**
 * Whether to support Wintab pressure sensitivity in the editor
 */
#ifndef WITH_WINTAB
#define WITH_WINTAB 1
#endif

/**
 * Whether to use Autodesk FBX importer
 **/
#ifndef WITH_FBX
#define WITH_FBX (!UE3_LEAN_AND_MEAN)
#endif

/**
 * Whether to enable support for ActorX
 **/
#ifndef WITH_ACTORX
#define WITH_ACTORX UDK
#endif

/** Whether to use the null RHI. */
#ifndef USE_NULL_RHI
#define USE_NULL_RHI 0
#endif

/**
 * Whether to use the Bink codec
 **/
#ifndef USE_BINK_CODEC
#define USE_BINK_CODEC (EPIC_INTERNAL && (!UE3_LEAN_AND_MEAN))
#endif
#if PS3 && USE_NULL_RHI
	#undef USE_BINK_CODEC
	#define USE_BINK_CODEC 0
#endif

/**
 * Whether to compile in support for Simplygon.
 */
#ifndef WITH_SIMPLYGON
#define WITH_SIMPLYGON 0
#endif

/**
 * Whether to compile in support for Simplygon via a DLL.
 */
#ifndef WITH_SIMPLYGON_DLL
#define WITH_SIMPLYGON_DLL 0
#endif

/**
 * Whether to use mangled SpeedTree importing
 */
#define WITH_SPEEDTREE_MANGLE (EPIC_INTERNAL && UDK)

/**
* Whether to use Dual Quaternion Skinning or not 
* This only works if QST_TRANSFORM is 1
* NOTE: If you change this, please delete your local shader cache 
*/
#define DQ_SKINNING 0

/**
 * Whether to compile in support for database connectivity and SQL execution.
 */
#ifndef WITH_DATABASE_SUPPORT
	#define WITH_DATABASE_SUPPORT (EPIC_INTERNAL && (!UE3_LEAN_AND_MEAN) && (!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE))
#endif

/** Whether to use LIBFFI for the UnrealScript DLLBind DLL calling feature. */
#ifndef WITH_LIBFFI
#define WITH_LIBFFI 0
#endif

/** Whether to use KissFFT for FFT. */
#ifndef WITH_KISSFFT
#define WITH_KISSFFT 0
#endif

/**
 * This is a global setting which will turn on logging / checks for things which are
 * considered especially bad for consoles.  Some of the checks are probably useful for PCs also.
 *
 * Throughout the code base there are specific things which dramatically affect performance and/or
 * are good indicators that something is wrong with the content.  These have PERF_ISSUE_FINDER in the
 * comment near the define to turn the individual checks on. 
 *
 * e.g. #if defined(PERF_LOG_DYNAMIC_LOAD_OBJECT) || LOOKING_FOR_PERF_ISSUES
 *
 * If one only cares about DLO, then one can enable the PERF_LOG_DYNAMIC_LOAD_OBJECT define.  Or one can
 * globally turn on all PERF_ISSUE_FINDERS :-)
 *
 **/
#ifndef LOOKING_FOR_PERF_ISSUES
#define LOOKING_FOR_PERF_ISSUES (0 && !FINAL_RELEASE)
#endif


/** here is an easy way to disable AI logging :D */
#ifndef DO_AI_LOGGING
#define DO_AI_LOGGING (1 && !(NO_LOGGING || FINAL_RELEASE || LOOKING_FOR_PERF_ISSUES || PS3 || DEDICATED_SERVER))
#if DO_AI_LOGGING 
// defines a condition evaluated at runtime (for fast toggling of AI logging)
#define RUNTIME_DO_AI_LOGGING (GEngine != NULL && !GEngine->bDisableAILogging)
#else
#define RUNTIME_DO_AI_LOGGING 0
#endif
#endif


/** Whether debugfSuppressed should be compiled out or not */
#ifndef SUPPORT_SUPPRESSED_LOGGING
#define SUPPORT_SUPPRESSED_LOGGING (LOOKING_FOR_PERF_ISSUES || _DEBUG)
#endif


/**
 * Enables/disables per-struct serialization performance tracking.
 */
#ifndef PERF_TRACK_SERIALIZATION_PERFORMANCE
#define PERF_TRACK_SERIALIZATION_PERFORMANCE 0
#endif


/**
 * Enables/disables detailed tracking of FAsyncPackage::Tick time.
 */
#ifndef PERF_TRACK_DETAILED_ASYNC_STATS
#define PERF_TRACK_DETAILED_ASYNC_STATS (0 || LOOKING_FOR_PERF_ISSUES)
#endif


/**
 * Enables general file IO stats tracking
 */
#ifndef PERF_TRACK_FILEIO_STATS
#define PERF_TRACK_FILEIO_STATS (0 || LOOKING_FOR_PERF_ISSUES)
#endif


/**
 * Enables/disables UE3 networking support
 */
#ifndef WITH_UE3_NETWORKING
    #define WITH_UE3_NETWORKING	(!UE3_LEAN_AND_MEAN)
#endif	// #ifndef WITH_UE3_NETWORKING

/**
 * If networking is disabled, make sure that no OnlineSubsystem is set
 */
#if !WITH_UE3_NETWORKING
#undef WITH_PANORAMA
#undef WITH_GAMESPY
#undef WITH_STEAMWORKS
#undef WITH_GAMECENTER
#endif

#ifndef WITH_GAMESPY
	// Use UBT to set this variable (see GetDesiredOnlineSubsystem)
	#define WITH_GAMESPY	0
#endif

#ifndef WITH_STEAMWORKS
	// Use UBT to set this variable (see GetDesiredOnlineSubsystem)
	#define WITH_STEAMWORKS	0
#endif

#if WITH_STEAMWORKS
	#ifndef WITH_STEAMWORKS_SOCKETS
		#define WITH_STEAMWORKS_SOCKETS	1
	#endif
#else
	#undef WITH_STEAMWORKS_SOCKETS
	#define WITH_STEAMWORKS_SOCKETS 0
#endif

#ifndef WITH_PANORAMA
	// Use UBT to set this variable (see GetDesiredOnlineSubsystem)
	#define WITH_PANORAMA	0
#endif

#ifndef WITH_GAMECENTER
	// Use UBT to set this variable (see GetDesiredOnlineSubsystem)
	#define WITH_GAMECENTER	0
#endif
/** Guarantee mutual exclusivity */
#if ((WITH_GAMESPY + WITH_STEAMWORKS + WITH_PANORAMA) > 1)
#error You can not have more than one of GameSpy, Steamworks, and Live in a single build.
#endif

/**
 * Enables/disables integration with TestTrack Pro
 */
#ifndef WITH_TESTTRACK
	#define WITH_TESTTRACK ( EPIC_INTERNAL && !UDK && _WINDOWS && !CONSOLE && !FINAL_RELEASE && (!UE3_LEAN_AND_MEAN))
#endif


/**
 * Enables/disables support for VTune integration
 */
#ifndef WITH_VTUNE
#if UDK
	#define WITH_VTUNE 0
#else
	// VTune is only supported on Windows platform.
	#define WITH_VTUNE ( _WINDOWS && !FINAL_RELEASE && !__INTEL_COMPILER)
#endif
#endif


/**
 * Enables/disables support for AQtime integration
 */
#ifndef WITH_AQTIME
#if UDK
	#define WITH_AQTIME 0
#else
	// AQtime is only supported on Windows platform.
	#define WITH_AQTIME ( _WINDOWS && !CONSOLE && !FINAL_RELEASE && !__INTEL_COMPILER) 
#endif
#endif


/**
 * Whether to use EasyHook to patch broken Windows routines
 *
 */
#ifndef WITH_EASYHOOK
#define WITH_EASYHOOK ( _WINDOWS )
#endif

/**
 * Whether the Xbox 360 XDK is installed and its functionality can be used in PC builds. An
 * example is using XCompress for cooking.
 */
#ifndef WITH_XDK
	#if PS3
		#define WITH_XDK 0
	#elif XBOX
		#define WITH_XDK 1
	#else
		#define WITH_XDK (XDKINSTALLED && !SHIPPING_PC_GAME && _WINDOWS && (!UE3_LEAN_AND_MEAN))
	#endif
#endif



/** Whether to use the secure CRT variants. Enabled by default */
#ifndef USE_SECURE_CRT
	#define USE_SECURE_CRT 1
#endif


/**
 *	Enables/disables LZO compression support
 */
#ifndef WITH_LZO
	#if PS3 // ps3 uses zlib task
		#define WITH_LZO 	0
	#else
		#define WITH_LZO	1
	#endif
#endif	// #ifndef WITH_LZO


/** 
 * Whether support for integrating into the firewall is there
 */
#define WITH_FIREWALL_SUPPORT (_WINDOWS && UDK)


/**
 * Whether this build supports generating a script patch
 */
//@script patcher
#ifndef SUPPORTS_SCRIPTPATCH_CREATION
	#if CONSOLE
		#define SUPPORTS_SCRIPTPATCH_CREATION 0
	#else
		// this should only be enabled when actually creating script patches, as it has a slight performance impact
		#define SUPPORTS_SCRIPTPATCH_CREATION 0
	#endif
#endif

#ifndef SUPPORTS_SCRIPTPATCH_LOADING
	#if IPHONE
		#define SUPPORTS_SCRIPTPATCH_LOADING 0 // disable script patching on iphone (where patches re-download whole app)
	#else
		#define SUPPORTS_SCRIPTPATCH_LOADING 1
	#endif
#endif

/**
 * Enables/disables Scaleform GFx support. Don't change the value here but rather adjust UBT.
 * Note: Scaleform SDK must be compiled with GFC_NO_IME_SUPPORT=0 in order for IME support to compile
 */
#ifndef WITH_GFx
	#define WITH_GFx 1
	#ifndef WITH_GFx_IME
		#define WITH_GFx_IME ((WITH_GFx) && (WITH_IME) && (!CONSOLE))
	#endif
	#if defined(SF_BUILD_RELEASE) && defined(SF_BUILD_DEBUG)
 		#undef SF_BUILD_DEBUG
 	#endif
#endif

/**
 * Enables/disables Scaleform GFx IME support depending on whether overall IME is enabled
 */
#if WITH_GFx_IME
	#if !WITH_IME // If IME is disabled, disable GFx IME support as well
		#undef WITH_GFx_IME
		#define WITH_GFx_IME 0
 	#endif
#endif

/**
 * Enables/disables support for FFullscreenMovieGFx.
 */
#if WITH_GFx
	#define WITH_GFx_FULLSCREEN_MOVIE 1
#endif

/**
 * Compile out FNameEntry::Flags on consoles in final release. This saves 64 bytes per name. The
 * name flags are only used for log suppresion and package saving so this doesn't remove any
 * used functionality.
 */
#ifndef SUPPORT_NAME_FLAGS
#define SUPPORT_NAME_FLAGS !(CONSOLE && FINAL_RELEASE)
#endif


/**
 * Allowing debug files means debugging files can be created, and CreateFileWriter does not need
 * a max file size it can grow to (needed for PS3 HD caching for now, may use for Xbox caching)
 */
#define ALLOW_DEBUG_FILES (!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE)


/**
 * Set profiling define for non final release configurations. This is e.g. used by PIX.
 */
#if !FINAL_RELEASE && !FINAL_RELEASE_DEBUGCONSOLE
#define PROFILE 1
#endif


/**
 * Whether code should verify that no unreachable actors and components are referenced.
 */
#if FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE || SHIPPING_PC_GAME
#define VERIFY_NO_UNREACHABLE_OBJECTS_ARE_REFERENCED 0
#else
#define	VERIFY_NO_UNREACHABLE_OBJECTS_ARE_REFERENCED 1
#endif


/**
 * Whether to use the allocator fixed size free list (dlmalloc)
 */
#ifndef USE_ALLOCATORFIXEDSIZEFREELIST
#if PS3
#define USE_ALLOCATORFIXEDSIZEFREELIST 1
#else
#define USE_ALLOCATORFIXEDSIZEFREELIST 0
#endif
#endif


/**
 * This is a new feature, potentially still under development (it will replace multiple meshes with a single
 * mesh at a certain distance, an N:1 LOD replacement system). It's now enabled by default, but the old code
 * will be left around for a little while at least until licensees, etc, have had a chance to use it.
 * See https://udn.epicgames.com/Three/MassiveLOD for information on the feature
 */
#ifndef USE_MASSIVE_LOD
#define USE_MASSIVE_LOD 1
#endif

/** Should be set to 0 in cases where hardware instancing is not needed, as it slows down non-instanced rendering slightly on Xenon. */
#ifndef SUPPORT_HARDWARE_INSTANCING
#define SUPPORT_HARDWARE_INSTANCING 1
#endif

/**
 * Whether or not to use Edge for PreVertexShaderCulling
 */
#define USE_PS3_PREVERTEXCULLING 0

/** 
 *   Whether profiling tools should be enabled or disabled as a whole
 */
#ifndef USE_PROFILING_TOOLS
#define USE_PROFILING_TOOLS  (UDK || !(SHIPPING_PC_GAME && DEDICATED_SERVER))
#endif 

/**
 * Whether to support the network profiler. We want to allow capturing in FR-DC to allow playtests
 * to run with the same configuration on client and server.
 */
#ifndef USE_NETWORK_PROFILER
#define USE_NETWORK_PROFILER  (USE_PROFILING_TOOLS && WITH_UE3_NETWORKING && (!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE))
#endif

/**
 * Whether to compile in support for the gameplay profiler.
 *
 * So the Gameplay Profiler has about 1.0% - 1.5% overhead in LTCG builds.  It also some really spikey performance characteristics.
 * The profile times are basically the same but the non-LTCG are just higher.  (e.g. FunctionA will be more costly than FunctionB in both
 * configs just that the timing for the functions will be higher in non-LTCG builds)
 *
 * So we recommend:  Release use the GamePlay Profiler (with FinalReleaseScript) and in LTCG use trace game
 */
#define USE_GAMEPLAY_PROFILER (USE_PROFILING_TOOLS && (!FINAL_RELEASE)) 

/**
 * Whether to compile in support for the AI profiler.
 */
#ifndef USE_AI_PROFILER
#define USE_AI_PROFILER (USE_PROFILING_TOOLS && !FINAL_RELEASE && !SHIPPING_PC_GAME)
#endif

#ifndef DO_CHARTING
#define DO_CHARTING ((!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE) || !(DEDICATED_SERVER && SHIPPING_PC_GAME))
#endif

/*
 * Whether to compile in support for the Apsalar Analytics API 
 */
#ifndef WITH_APSALAR
#define WITH_APSALAR 0
#endif

#ifndef DEFINE_ANALYTICS_CONFIG
	#define DEFINE_ANALYTICS_CONFIG TMap<FString, FAnalyticsConfig> AnalyticsConfig
#endif

/*
 * Whether to compile in support for the Swrve Analytics API 
 */
#ifndef WITH_SWRVE
#define WITH_SWRVE 0
#endif

/*
 * Whether to compile in support for JPEG decompression
 */
#ifndef WITH_JPEG
#define WITH_JPEG 0
#endif

/**
 * Whether to compile in support for unit tests.
 */
#ifndef USE_UNIT_TESTS
#define USE_UNIT_TESTS (!FINAL_RELEASE && !SHIPPING_PC_GAME)
#endif

/**
 * Whether to compile with RealD stereo rendering
 */
#ifndef WITH_REALD
    // NOTE: if you add a new platform you will also need to add it to the RealD change in the
    //       following files:
    //          - ShaderCompiler.cpp: BeginCompileShader() function
    //          - UE3BuildExternal.cs: SetUpRealDEnvironment() function

    // Default to off, UBT should decide whether to compile this or not.
    #define WITH_REALD 0
#endif

/**
 *Specifies if detailed iphone mem tracking should occur
 */
#ifndef USE_DETAILED_IPHONE_MEM_TRACKING
#define USE_DETAILED_IPHONE_MEM_TRACKING 0
#endif

/**
 * Specifies if scoped mem stats should be active
 */
#ifndef USE_SCOPED_MEM_STATS
#define USE_SCOPED_MEM_STATS 0
#endif

/** 
 * Whether to compile in OpenAutomate support
 */
#ifndef WITH_OPEN_AUTOMATE
#define WITH_OPEN_AUTOMATE 0
#endif

#endif	// #ifndef _HEADER_UNBUILD_H_

