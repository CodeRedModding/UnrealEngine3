/*=============================================================================
	D3D11State.h: D3D state definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

class FD3D11SamplerState : public FRefCountedObject, public TDynamicRHIResource<RRT_SamplerState>
{
public:
	D3D11_SAMPLER_DESC SamplerDesc;
};

class FD3D11RasterizerState : public FRefCountedObject, public TDynamicRHIResource<RRT_RasterizerState>
{
public:

	D3D11_RASTERIZER_DESC RasterizerDesc;
};

class FD3D11DepthState : public FRefCountedObject, public TDynamicRHIResource<RRT_DepthState>
{
public:

	D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
};

class FD3D11StencilState : public FRefCountedObject, public TDynamicRHIResource<RRT_StencilState>
{
public:

	D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
	UINT StencilRef;
};

class FD3D11BlendState : public FRefCountedObject, public TDynamicRHIResource<RRT_BlendState>
{
public:

	D3D11_BLEND_DESC BlendDesc;
	FLinearColor BlendFactor;

	FD3D11BlendState():
		FRefCountedObject(),
		BlendFactor(0,0,0,0)
	{
	}
	~FD3D11BlendState()
	{
	}
};

void ReleaseCachedD3D11States();
