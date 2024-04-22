/*=============================================================================
	D3D9RenderTarget.h: D3D render target RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * A D3D surface, and an optional texture handle which can be used to read from the surface.
 */
class FD3D9Surface :
	public FRefCountedObject,
	public TRefCountPtr<IDirect3DSurface9>,
	public TDynamicRHIResource<RRT_Surface>
{
public:
	/** 2d texture to resolve surface to */
	TRefCountPtr<FD3D9Texture2D> ResolveTargetTexture2D;
	/** Cube texture to resolve surface to */
	TRefCountPtr<FD3D9TextureCube> ResolveTargetTextureCube;
	/** Dedicated texture when not rendering directly to resolve target texture surface */
	TRefCountPtr<FD3D9Texture2D> Texture2D;
	/** Dedicated texture when not rendering directly to resolve target texture surface */
	TRefCountPtr<FD3D9TextureCube> TextureCube;

	/** Initialization constructor. Using 2d resolve texture */
	FD3D9Surface(
		FD3D9Texture2D* InResolveTargetTexture,
		FD3D9Texture2D* InTexture = NULL,
		IDirect3DSurface9* InSurface = NULL
		)
		:	ResolveTargetTexture2D(InResolveTargetTexture)		
		,	Texture2D(InTexture)
		,	TRefCountPtr<IDirect3DSurface9>(InSurface)
	{}

	/** Initialization constructor. Using cube resolve texture */
	FD3D9Surface(
		FD3D9TextureCube* InResolveTargetTexture,
		FD3D9TextureCube* InTexture = NULL,
		IDirect3DSurface9* InSurface = NULL
		)
		:	ResolveTargetTextureCube(InResolveTargetTexture)
		,	TextureCube(InTexture)
		,	TRefCountPtr<IDirect3DSurface9>(InSurface)
	{}

	/** 
	* Initialization constructor. Using cube resolve texture 
	* Needed when using a dedicated 2D texture for rendering 
	* with a cube resolve texture
	*/
	FD3D9Surface(
		FD3D9TextureCube* InResolveTargetTexture,
		FD3D9Texture2D* InTexture = NULL,
		IDirect3DSurface9* InSurface = NULL
		)
		:	ResolveTargetTextureCube(InResolveTargetTexture)
		,	Texture2D(InTexture)
		,	TRefCountPtr<IDirect3DSurface9>(InSurface)
	{}
};
