/*=============================================================================
	D3DDrv.cpp: Unreal Direct3D driver precompiled header generator.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

// Includes.
#include "D3DDrv.h"

// Globals.

UD3DRenderDevice*			GD3DRenderDevice = NULL;
FD3DRenderInterface			GD3DRenderInterface;
FD3DSceneRenderer*			GD3DSceneRenderer = NULL;
FD3DStatGroup				GD3DStats;

#ifndef XBOX	// GDirect3DDevice is defined in XeLaunch.cpp on Xenon.
TD3DRef<IDirect3DDevice9>	GDirect3DDevice;
#endif

TArray<FD3DDefinition>*		D3DTempDefinitions;

//
//	GetTextureSurface
//

TD3DRef<IDirect3DSurface9> GetTextureSurface(const TD3DRef<IDirect3DTexture9>& Texture,UINT MipIndex)
{
	TD3DRef<IDirect3DSurface9>	Surface;
	HRESULT						Result = Texture->GetSurfaceLevel(MipIndex,&Surface.Handle);

	if(FAILED(Result))
		appErrorf(TEXT("Failed to get surface from texture: %s"),*D3DError(Result));

	return Surface;

}

void AddVertexElement(TArray<D3DVERTEXELEMENT9>& Elements,WORD Stream,WORD Offset,_D3DDECLTYPE Type,BYTE Usage,BYTE UsageIndex)
{
	D3DVERTEXELEMENT9	Element;
	Element.Stream		= Stream;
	Element.Offset		= Offset;
	Element.Type		= Type;
	Element.Method		= D3DDECLMETHOD_DEFAULT;
	Element.Usage		= Usage;
	Element.UsageIndex	= UsageIndex;
	Elements.AddItem(Element);
}
/*-----------------------------------------------------------------------------
	End.
-----------------------------------------------------------------------------*/
