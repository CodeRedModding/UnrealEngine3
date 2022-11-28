/*=============================================================================
	D3DDrv.cpp: Unreal Direct3D driver precompiled header generator.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

/** Whether to keep shader source around for debugging purposes.	@warning: code relies on D3DX summer update 2004 */
#define ALLOW_DUMP_SHADERS	0

// Unreal includes.
#include <basetsd.h>
#include "Engine.h"
#ifndef XBOX
#include "WinDrv.h"
#endif

// Direct3D includes.
#ifndef XBOX
#pragma pack(push,8)
#define D3D_OVERLOADS 1
#include "d3d9.h"
#include "d3dx9.h"
#pragma pack(pop)
#endif

//
//	FD3DDefinition
//

struct FD3DDefinition
{
	FString	Name,
			Value;
};

// Can't pass TArrays by value on Feb XDK and this seems like a good enough general solution replacing TArray<FD3DDefinition>()
// This has to be a pointer as we can't have global/ static variables that require use of GMalloc at destructor time. Actual
// construction/ destruction happens in UD3DRenderDevice's constructor/ Destroy function.
extern TArray<FD3DDefinition>* D3DTempDefinitions;
#define NO_DEFINITIONS (D3DTempDefinitions->Empty(), *D3DTempDefinitions)


#ifndef XBOX
#define USE_OWN_ALLOCATOR
#define USE_DEFAULT_ALLOCATOR
#endif

//
//	D3DError - Returns a string describing the Direct3D error code given.
//

static inline FString D3DError( HRESULT h )
{
#ifdef XBOX
	return FString::Printf(TEXT("%08X"),(INT)h);
#else
	#define D3DERR(x) case x: return TEXT(#x);
	switch( h )
	{
		D3DERR(D3D_OK)
		D3DERR(D3DERR_WRONGTEXTUREFORMAT)
		D3DERR(D3DERR_UNSUPPORTEDCOLOROPERATION)
		D3DERR(D3DERR_UNSUPPORTEDCOLORARG)
		D3DERR(D3DERR_UNSUPPORTEDALPHAOPERATION)
		D3DERR(D3DERR_UNSUPPORTEDALPHAARG)
		D3DERR(D3DERR_TOOMANYOPERATIONS)
		D3DERR(D3DERR_CONFLICTINGTEXTUREFILTER)
		D3DERR(D3DERR_UNSUPPORTEDFACTORVALUE)
		D3DERR(D3DERR_CONFLICTINGRENDERSTATE)
		D3DERR(D3DERR_UNSUPPORTEDTEXTUREFILTER)
		D3DERR(D3DERR_CONFLICTINGTEXTUREPALETTE)
		D3DERR(D3DERR_DRIVERINTERNALERROR)
		D3DERR(D3DERR_NOTFOUND)
		D3DERR(D3DERR_MOREDATA)
		D3DERR(D3DERR_DEVICELOST)
		D3DERR(D3DERR_DEVICENOTRESET)
		D3DERR(D3DERR_NOTAVAILABLE)
		D3DERR(D3DERR_OUTOFVIDEOMEMORY)
		D3DERR(D3DERR_INVALIDDEVICE)
		D3DERR(D3DERR_INVALIDCALL)
		D3DERR(D3DERR_DRIVERINVALIDCALL)
		D3DERR(D3DXERR_INVALIDDATA)
		D3DERR(E_OUTOFMEMORY)
		default: return FString::Printf(TEXT("%08X"),(INT)h);
	}
	#undef D3DERR
#endif
}

//
//	Definitions and enumerations.
//

#define MAX_TEXTURES			16

#if 0
#define SAFERELEASE(x)	if(x) { DWORD	Refs = (x)->Release(); if(Refs) debugf(TEXT("SAFERELEASE(%s): %u refs remain"),L#x,Refs); (x) = NULL; }
#else
#define SAFERELEASE(x)	if(x) { (x)->Release(); (x) = NULL; }
#endif


/** Enumeration of D3D specific platform flags used for GPixelFormats.PlatformFlags */
enum D3DFormatPlatformFlags
{
	/** Whether this format supports SRGB read, aka gamma correction on texture sampling or whether we need to do it manually (if this flag is set) */
	D3DPF_REQUIRES_GAMMA_CORRECTION = 1
};


//
// NVIDIA FOURCC codes for MET support.
//

#define NV_SURFACE_FORMAT_IMAGE_MET_4_4_4_4 ((D3DFORMAT)MAKEFOURCC('N','V','E','0'))
#define NV_SURFACE_FORMAT_IMAGE_MET_4_4_4_2 ((D3DFORMAT)MAKEFOURCC('N','V','E','1'))
#define NV_SURFACE_FORMAT_IMAGE_MET_4_4_4_1 ((D3DFORMAT)MAKEFOURCC('N','V','E','2'))
#define NV_SURFACE_FORMAT_IMAGE_MET_4_4_2_2 ((D3DFORMAT)MAKEFOURCC('N','V','E','3'))
#define NV_SURFACE_FORMAT_IMAGE_MET_4_4_2_1 ((D3DFORMAT)MAKEFOURCC('N','V','E','4'))
#define NV_SURFACE_FORMAT_IMAGE_MET_4_4_1_1 ((D3DFORMAT)MAKEFOURCC('N','V','E','5'))
#define NV_SURFACE_FORMAT_IMAGE_MET_4_2_2_2 ((D3DFORMAT)MAKEFOURCC('N','V','E','6'))
#define NV_SURFACE_FORMAT_IMAGE_MET_4_2_2_1 ((D3DFORMAT)MAKEFOURCC('N','V','E','7'))
#define NV_SURFACE_FORMAT_IMAGE_MET_4_2_1_1 ((D3DFORMAT)MAKEFOURCC('N','V','E','8'))
#define NV_SURFACE_FORMAT_IMAGE_MET_4_1_1_1 ((D3DFORMAT)MAKEFOURCC('N','V','E','9'))
#define NV_SURFACE_FORMAT_IMAGE_MET_2_2_2_2 ((D3DFORMAT)MAKEFOURCC('N','V','E','A'))
#define NV_SURFACE_FORMAT_IMAGE_MET_2_2_2_1 ((D3DFORMAT)MAKEFOURCC('N','V','E','B'))
#define NV_SURFACE_FORMAT_IMAGE_MET_2_2_1_1 ((D3DFORMAT)MAKEFOURCC('N','V','E','C'))
#define NV_SURFACE_FORMAT_IMAGE_MET_2_1_1_1 ((D3DFORMAT)MAKEFOURCC('N','V','E','D'))

//
//	Forward declarations.
//

class UD3DRenderDevice;

//
//	Includes.
//

#include "D3DStats.h"
#include "D3DResourceHelper.h"
#include "D3DResource.h"
#include "D3DShader.h"
#include "D3DMeshShaders.h"
#include "D3DScene.h"
#include "D3DRenderInterface.h"
#include "D3DRenderDevice.h"

//
//	Globals.
//

extern UD3DRenderDevice*			GD3DRenderDevice;
extern FD3DRenderInterface			GD3DRenderInterface;
extern FD3DSceneRenderer*			GD3DSceneRenderer;
#ifndef XBOX
extern TD3DRef<IDirect3DDevice9>	GDirect3DDevice;
#else
extern IDirect3DDevice9*			GDirect3DDevice;
#endif
extern FD3DStatGroup				GD3DStats;

//
//	GetTextureSurface
//

extern TD3DRef<IDirect3DSurface9> GetTextureSurface(const TD3DRef<IDirect3DTexture9>& Texture,UINT MipIndex = 0);

//
//	AddVertexElement - Appends a vertex element to a list.
//

extern void AddVertexElement(TArray<D3DVERTEXELEMENT9>& Elements,WORD Stream,WORD Offset,_D3DDECLTYPE Type,BYTE Usage,BYTE UsageIndex);


/*-----------------------------------------------------------------------------
	End.
-----------------------------------------------------------------------------*/
