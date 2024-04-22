/*=============================================================================
	D3D11RenderTarget.h: D3D render target RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * A D3D surface, and an optional texture handle which can be used to read from the surface.
 */
class FD3D11Surface :
	public FRefCountedObject,
	public TDynamicRHIResource<RRT_Surface>
{
public:

	/** A view of the surface as a render target. */
	TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
	/** A view of the surface as a depth-stencil target. */
	TRefCountPtr<ID3D11DepthStencilView> DepthStencilView;
	/** A view of the surface as a readonly depth-stencil target. */
	TRefCountPtr<ID3D11DepthStencilView> ReadOnlyDepthStencilView;
	/** A view of the surface as a shader resource. */
	TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;
	/** A view of the surface as a shader resource. */
	TRefCountPtr<ID3D11UnorderedAccessView> UnorderedAccessView;
	/** 2d texture to resolve surface to */
	TRefCountPtr<FD3D11Texture2D> ResolveTarget2D;
	/** 2d texture to resolve surface to */
	TRefCountPtr<FD3D11TextureCube> ResolveTargetCube;
	/** Dedicated texture when not rendering directly to resolve target texture surface */
	TRefCountPtr<ID3D11Texture2D> Resource;

	/** Tracks the texture slots that a shader resource view for this surface has been bound to. */
	TArray<INT> BoundShaderResourceSlots[SF_NumFrequencies];

	/** Unsets all texture slots that a shader resource view for this surface has been bound to. */
	void UnsetTextureReferences(FD3D11DynamicRHI* D3DRHI);

	/** Initialization constructor. Using 2d resolve texture */
	FD3D11Surface(
		ID3D11RenderTargetView* InRenderTargetView,
		ID3D11DepthStencilView* InDepthStencilView = NULL,
		ID3D11DepthStencilView* InReadOnlyDepthStencilView = NULL,
		ID3D11ShaderResourceView* InShaderResourceView = NULL,
		ID3D11UnorderedAccessView* InUnorderedAccessView = NULL,
		FD3D11Texture2D* InResolveTarget2D = NULL,
		FD3D11TextureCube* InResolveTargetCube = NULL,
		ID3D11Texture2D* InResource = NULL
		)
		:	RenderTargetView(InRenderTargetView)
		,	DepthStencilView(InDepthStencilView)
		,	ReadOnlyDepthStencilView(InReadOnlyDepthStencilView)
		,	ShaderResourceView(InShaderResourceView)
		,	UnorderedAccessView(InUnorderedAccessView)
		,	ResolveTarget2D(InResolveTarget2D)
		,	ResolveTargetCube(InResolveTargetCube)
		,	Resource(InResource)
	{}
};

