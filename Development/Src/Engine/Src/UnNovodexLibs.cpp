/*=============================================================================
	UnNovodexLibs.cpp: Novodex library imports
	Copyright 2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#ifdef WITH_NOVODEX

// Novodex library imports

#if defined(_XBOX)
/***
	#if defined(_DEBUG)
		#pragma message("Including Novodex Debug libraries.")
//		#pragma comment(lib, "External/Novodex/Foundation/lib/Xenon/Debug/NxFoundationDebug.lib")
		#pragma comment(lib, "..\\..\\..\\..\\Novodex\\sdks/Foundation/lib/Xenon/Debug/NxFoundationDebug.lib")
		#if defined(_NOVODEX_INTEGRATION_USE_FLUIDS_)
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/Debug/NxPhysicsDebug.lib")
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/Debug/NxCoreDebug.lib")
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/Debug/Nxopcode_libDebug.lib")
		#else	//#if defined(_NOVODEX_INTEGRATION_USE_FLUIDS_)
			#pragma comment(lib, "..\\..\\..\\..\\Novodex\\sdks\\Physics/lib/Xenon/DebugNoFluid/NxPhysicsDebugNoFluid.lib")
			#pragma comment(lib, "..\\..\\..\\..\\Novodex\\sdks\\Physics/lib/Xenon/DebugNoFluid/NxCoreDebugNoFluid.lib")
			#pragma comment(lib, "..\\..\\..\\..\\Novodex\\sdks\\Physics/lib/Xenon/DebugNoFluid/Nxopcode_libDebugNoFluid.lib")
		#endif	//#if defined(_NOVODEX_INTEGRATION_USE_FLUIDS_)
	#elif defined(FINAL_RELEASE)
***/
	#if defined(FINAL_RELEASE)
		#pragma comment(lib, "External/Novodex/Foundation/lib/Xenon/ReleaseLTCG/NxFoundationReleaseLTCG.lib")
		#if defined(_NOVODEX_INTEGRATION_USE_FLUIDS_)
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/ReleaseLTCG/NxPhysicsReleaseLTCG.lib")
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/ReleaseLTCG/NxCoreReleaseLTCG.lib")
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/ReleaseLTCG/Nxopcode_libReleaseLTCG.lib")
		#else	//#if defined(_NOVODEX_INTEGRATION_USE_FLUIDS_)
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/ReleaseLTCGNoFluid/NxPhysicsReleaseLTCGNoFluid.lib")
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/ReleaseLTCGNoFluid/NxCoreReleaseLTCGNoFluid.lib")
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/ReleaseLTCGNoFluid/Nxopcode_libReleaseLTCGNoFluid.lib")
		#endif	//#if defined(_NOVODEX_INTEGRATION_USE_FLUIDS_)
	#else	//_DEBUG
		#pragma comment(lib, "External/Novodex/Foundation/lib/Xenon/Release/NxFoundationRelease.lib")
		#if defined(_NOVODEX_INTEGRATION_USE_FLUIDS_)
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/Release/NxPhysicsRelease.lib")
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/Release/NxCoreRelease.lib")
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/Release/Nxopcode_libRelease.lib")
		#else	//#if defined(_NOVODEX_INTEGRATION_USE_FLUIDS_)
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/ReleaseNoFluid/NxPhysicsReleaseNoFluid.lib")
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/ReleaseNoFluid/NxCoreReleaseNoFluid.lib")
			#pragma comment(lib, "External/Novodex/Physics/lib/Xenon/ReleaseNoFluid/Nxopcode_libReleaseNoFluid.lib")
		#endif	//#if defined(_NOVODEX_INTEGRATION_USE_FLUIDS_)
	#endif	//_DEBUG
#else	//#if defined(_XBOX)
//#ifdef _DEBUG
//	#pragma comment(lib, "../../External/Novodex/Foundation/lib/win32/debug/NxFoundationDEBUG.lib")
//	#pragma comment(lib, "../../External/Novodex/Physics/lib/win32/debug/NxPhysicsDEBUG.lib")
//#else
	#pragma comment(lib, "../../External/Novodex/Foundation/lib/win32/Release/NxFoundation.lib")
	#pragma comment(lib, "../../External/Novodex/Physics/lib/win32/Release/NxPhysics.lib")
//#endif

#endif	//#if defined(_XBOX)

#include "UnNovodexSupport.h"

NxPhysicsSDK*	GNovodexSDK = NULL;

#endif // WITH_NOVODEX
