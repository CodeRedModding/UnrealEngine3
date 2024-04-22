/*=============================================================================
	UnFaceFXSupport.cpp: FaceFX support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#if WITH_FACEFX

#include "UnFaceFXSupport.h"
#include "UnFaceFXRegMap.h"
#include "UnFaceFXMaterialParameterProxy.h"
#include "UnFaceFXMorphTargetProxy.h"

#include "../FaceFX/UnFaceFXNode.h"
#include "../FaceFX/UnFaceFXMaterialNode.h"
#include "../FaceFX/UnFaceFXMorphNode.h"

// Define this to link in the debug libraries in debug builds.
#define USE_FACEFX_DEBUG_LIBS

#if defined(XBOX)

#if defined(_DEBUG) && defined(USE_FACEFX_DEBUG_LIBS)
	#pragma comment(lib, "FxSDKd_Unreal.lib")
#else
	#pragma comment(lib, "FxSDK_Unreal.lib")
#endif

#elif defined(PS3)

#else // WIN32

#if defined(_DEBUG) && defined(USE_FACEFX_DEBUG_LIBS)
#if _WIN64
	#pragma comment(lib, "FxSDKd_Unreal.lib")
#else
	#pragma comment(lib, "FxSDKd_Unreal.lib")
#endif
#else
#if _WIN64
	#pragma comment(lib, "FxSDK_Unreal.lib")
#else
	#pragma comment(lib, "FxSDK_Unreal.lib")
#endif
#endif

#endif

using namespace OC3Ent;
using namespace Face;

void* FxAllocateUE3( FxSize NumBytes )
{
	return appMalloc(NumBytes);
}

void* FxAllocateDebugUE3( FxSize NumBytes, const FxAChar* /*system*/ )
{
	return appMalloc(NumBytes);
}

void FxFreeUE3( void* Ptr, FxSize /*n*/ )
{
	appFree(Ptr);
}

void FxRemoveNodeUserDataUE3( FxCompiledFaceGraph& cg )
{
	FxSize numNodes = cg.nodes.Length();
	for( FxSize i = 0; i < numNodes; ++i )
	{
		if( cg.nodes[i].pUserData )
		{
			switch( cg.nodes[i].nodeType )
			{
			case NT_MorphTargetUE3:
				{
					if( cg.nodes[i].pUserData )
					{
						FFaceFXMorphTargetProxy* pUserData = reinterpret_cast<FFaceFXMorphTargetProxy*>(cg.nodes[i].pUserData);
						delete pUserData;
						cg.nodes[i].pUserData = NULL;
					}
				}
				break;
			case NT_MaterialParameterUE3:
				{
					if( cg.nodes[i].pUserData )
					{
						FFaceFXMaterialParameterProxy* pUserData = reinterpret_cast<FFaceFXMaterialParameterProxy*>(cg.nodes[i].pUserData);
						delete pUserData;
						cg.nodes[i].pUserData = NULL;
					}
				}
				break;
			default:
				break;
			}
		}
	}
}

void FxPreDestroyCallbackFuncUE3( FxCompiledFaceGraph& cg )
{
	FxRemoveNodeUserDataUE3(cg);
}

void FxPreCompileCallbackFuncUE3( FxCompiledFaceGraph& cg, FxFaceGraph& FxUnused(faceGraph) )
{
	FxRemoveNodeUserDataUE3(cg);
}

// This code assumes that only one thread will initialize/shutdown FaceFX...
// This is done to prevent multiple calls to UnShutdownFaceFX from actually
// crashing the application.
UBOOL GFaceFXIsInitialized = FALSE;
void UnInitFaceFX( void )
{
	if (GFaceFXIsInitialized == FALSE)
	{
		GFaceFXIsInitialized = TRUE;

		debugf(NAME_Init,TEXT("Initializing FaceFX..."));
		FxMemoryAllocationPolicy allocPolicyUE3(MAT_Custom, FxFalse, FxAllocateUE3, FxAllocateDebugUE3, FxFreeUE3);
		FxSDKStartup(allocPolicyUE3);
		FxCompiledFaceGraph::SetPreDestroyCallback(FxPreDestroyCallbackFuncUE3);
		FxCompiledFaceGraph::SetCompilationCallbacks(FxPreCompileCallbackFuncUE3, NULL, NULL, NULL);
		FFaceFXRegMap::Startup();
		debugf(NAME_Init,TEXT("FaceFX %s initialized."),*FString(FxSDKGetVersionString().GetCstr()));
	}
}

void UnShutdownFaceFX( void )
{
	if (GFaceFXIsInitialized == TRUE)
	{
		debugf(TEXT("Shutting down FaceFX..."));
		FFaceFXRegMap::Shutdown();
		FxSDKShutdown();
		debugf(TEXT("FaceFX shutdown."));
		GFaceFXIsInitialized = FALSE;
	}
}

TArray<FFaceFXRegMapEntry> FFaceFXRegMap::RegMap;

void FFaceFXRegMap::Startup( void )
{
}

void FFaceFXRegMap::Shutdown( void )
{
	RegMap.Empty();
}

FFaceFXRegMapEntry* FFaceFXRegMap::GetRegisterMapping( const FName& RegName )
{
	INT NumRegMapEntries = RegMap.Num();
	for( INT i = 0; i < NumRegMapEntries; ++i )
	{
		if( RegMap(i).UnrealRegName == RegName )
		{
			return &RegMap(i);
		}
	}
	return NULL;
}

void FFaceFXRegMap::AddRegisterMapping( const FName& RegName )
{
	if( !GetRegisterMapping(RegName) )
	{
		RegMap.AddItem(FFaceFXRegMapEntry(RegName, FxName(TCHAR_TO_ANSI(*RegName.ToString()))));
	}
}

#endif // WITH_FACEFX

