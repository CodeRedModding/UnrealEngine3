/*=============================================================================
	D3D9State.h: D3D state definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

class FD3D9SamplerState : public FRefCountedObject, public TDynamicRHIResource<RRT_SamplerState>
{
public:

	D3DTEXTUREFILTERTYPE MagFilter;
	D3DTEXTUREFILTERTYPE MinFilter;
	D3DTEXTUREFILTERTYPE MipFilter;
	D3DTEXTUREADDRESS AddressU;
	D3DTEXTUREADDRESS AddressV;
	D3DTEXTUREADDRESS AddressW;
	INT MipMapLODBias;
	DWORD BorderColor;
};

class FD3D9RasterizerState : public FRefCountedObject, public TDynamicRHIResource<RRT_RasterizerState>
{
public:

	D3DFILLMODE FillMode;
	D3DCULL CullMode;
	FLOAT DepthBias;
	FLOAT SlopeScaleDepthBias;
	UBOOL bAllowMSAA;
};

class FD3D9DepthState : public FRefCountedObject, public TDynamicRHIResource<RRT_DepthState>
{
public:

	UBOOL bZEnable;
	UBOOL bZWriteEnable;
	D3DCMPFUNC ZFunc;
};

class FD3D9StencilState : public FRefCountedObject, public TDynamicRHIResource<RRT_StencilState>
{
public:

	UBOOL bStencilEnable;
	UBOOL bTwoSidedStencilMode;
	D3DCMPFUNC StencilFunc;
	D3DSTENCILOP StencilFail;
	D3DSTENCILOP StencilZFail;
	D3DSTENCILOP StencilPass;
	D3DCMPFUNC CCWStencilFunc;
	D3DSTENCILOP CCWStencilFail;
	D3DSTENCILOP CCWStencilZFail;
	D3DSTENCILOP CCWStencilPass;
	DWORD StencilReadMask;
	DWORD StencilWriteMask;
	DWORD StencilRef;
};

class FD3D9BlendState : public FRefCountedObject, public TDynamicRHIResource<RRT_BlendState>
{
public:

	UBOOL bAlphaBlendEnable;
	D3DBLENDOP ColorBlendOperation;
	D3DBLEND ColorSourceBlendFactor;
	D3DBLEND ColorDestBlendFactor;
	UBOOL bSeparateAlphaBlendEnable;
	D3DBLENDOP AlphaBlendOperation;
	D3DBLEND AlphaSourceBlendFactor;
	D3DBLEND AlphaDestBlendFactor;
	UBOOL bAlphaTestEnable;
	D3DCMPFUNC AlphaFunc;
	DWORD AlphaRef;
	DWORD BlendFactor;
};
