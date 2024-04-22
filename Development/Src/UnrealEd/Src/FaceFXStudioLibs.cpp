/*=============================================================================
	FaceFXStudioLibs.cpp: Code for linking in the FaceFX Studio libraries.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"

#if WITH_FACEFX_STUDIO

#include "../../../External/FaceFX/Studio/Main/Inc/FxStudioMainWin.h"

// This is only here to force the compiler to link in the libraries!
OC3Ent::Face::FxStudioMainWin* GFaceFXStudio = NULL;

// Define this to link in the debug libraries in debug builds.
#define USE_FACEFX_DEBUG_LIBS

#if defined(_DEBUG) && defined(USE_FACEFX_DEBUG_LIBS)
#if _WIN64
	#pragma comment(lib, "FxCGd_Unreal.lib")
	#pragma comment(lib, "FxAnalysisd_Unreal.lib")
#else
	#pragma comment(lib, "FxCGd_Unreal.lib")
	#pragma comment(lib, "FxAnalysisd_Unreal.lib")
#endif
#else
#if _WIN64
	#pragma comment(lib, "FxCG_Unreal.lib")
	#pragma comment(lib, "FxAnalysis_Unreal.lib")
#else
	#pragma comment(lib, "FxCG_Unreal.lib")
	#pragma comment(lib, "FxAnalysis_Unreal.lib")
#endif
#endif

#if _WIN64
#pragma comment(lib, "libresample_x64.lib")
#else
#pragma comment(lib, "libresample.lib")
#endif


#endif // WITH_FACEFX_STUDIO

