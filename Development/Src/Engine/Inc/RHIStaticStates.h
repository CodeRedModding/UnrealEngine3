/*=============================================================================
	RHIStaticStates.h: RHI static state template definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * The base class of the static RHI state classes.
 */
template<typename InitializerType,typename RHIRefType,typename RHIParamRefType>
class TStaticStateRHI
{
public:

	static RHIParamRefType GetRHI()
	{
		static FStaticStateResource Resource;
		return Resource.StateRHI;
	};

private:

	/** A resource which manages the RHI resource. */
	class FStaticStateResource : public FRenderResource
	{
	public:

		RHIRefType StateRHI;

		FStaticStateResource()
		{
			InitResource();
		}

		// FRenderResource interface.
		virtual void InitRHI()
		{
			StateRHI = InitializerType::CreateRHI();
		}
		virtual void ReleaseRHI()
		{
			StateRHI.SafeRelease();
		}

		~FStaticStateResource()
		{
			ReleaseResource();
		}
	};
};

/**
 * A static RHI sampler state resource.
 * TStaticSamplerStateRHI<...>::GetStaticState() will return a FSamplerStateRHIRef to a sampler state with the desired settings.
 * Should only be used from the rendering thread.
 */
template<ESamplerFilter Filter=SF_Point,
	ESamplerAddressMode AddressU=AM_Clamp,
	ESamplerAddressMode AddressV=AM_Clamp,
	ESamplerAddressMode AddressW=AM_Clamp, 
	ESamplerMipMapLODBias MipBias=MIPBIAS_None,
	// Note: setting to a different value than GSystemSettings.MaxAnisotropy is only supported in D3D11
	// A value of 0 will use GSystemSettings.MaxAnisotropy
	INT MaxAnisotropy=0,
	DWORD BorderColor=0,
	/** Only supported in D3D11 */
	ESamplerCompareFunction SamplerComparisonFunction=SCF_Never>
class TStaticSamplerState : public TStaticStateRHI<TStaticSamplerState<Filter,AddressU,AddressV,AddressW,MipBias,MaxAnisotropy,BorderColor,SamplerComparisonFunction>,FSamplerStateRHIRef,FSamplerStateRHIParamRef>
{
public:
	static FSamplerStateRHIRef CreateRHI()
	{
		FSamplerStateInitializerRHI Initializer( Filter, AddressU, AddressV, AddressW, MipBias, MaxAnisotropy, BorderColor, SamplerComparisonFunction );
		return RHICreateSamplerState(Initializer);
	}
};

/**
 * A static RHI rasterizer state resource.
 * TStaticRasterizerStateRHI<...>::GetStaticState() will return a FRasterizerStateRHIRef to a rasterizer state with the desired
 * settings.
 * Should only be used from the rendering thread.
 */
template<ERasterizerFillMode FillMode=FM_Solid,ERasterizerCullMode CullMode=CM_None>
class TStaticRasterizerState : public TStaticStateRHI<TStaticRasterizerState<FillMode,CullMode>,FRasterizerStateRHIRef,FRasterizerStateRHIParamRef>
{
public:
	static FRasterizerStateRHIRef CreateRHI()
	{
		FRasterizerStateInitializerRHI Initializer = { FillMode, CullMode, 0, 0, TRUE };
		return RHICreateRasterizerState(Initializer);
	}
};

/**
 * A static RHI depth state resource.
 * TStaticDepthStateRHI<...>::GetStaticState() will return a FDepthStateRHIRef to a depth state with the desired
 * settings.
 * Should only be used from the rendering thread.
 */
template<
	UBOOL bEnableDepthWrite=TRUE,
	ECompareFunction DepthTest=CF_LessEqual
	>
class TStaticDepthState : public TStaticStateRHI<
	TStaticDepthState<
		bEnableDepthWrite,
		DepthTest
		>,
	FDepthStateRHIRef,
	FDepthStateRHIParamRef
	>
{
public:
	static FDepthStateRHIRef CreateRHI()
	{
		FDepthStateInitializerRHI Initializer =
		{
			bEnableDepthWrite,
			DepthTest
		};
		return RHICreateDepthState(Initializer);
	}
};

/**
 * A static RHI stencil state resource.
 * TStaticStencilStateRHI<...>::GetStaticState() will return a FStencilStateRHIRef to a stencil state with the desired
 * settings.
 * Should only be used from the rendering thread.
 */
template<
	UBOOL bEnableFrontFaceStencil = FALSE,
	ECompareFunction FrontFaceStencilTest = CF_Always,
	EStencilOp FrontFaceStencilFailStencilOp = SO_Keep,
	EStencilOp FrontFaceDepthFailStencilOp = SO_Keep,
	EStencilOp FrontFacePassStencilOp = SO_Keep,
	UBOOL bEnableBackFaceStencil = FALSE,
	ECompareFunction BackFaceStencilTest = CF_Always,
	EStencilOp BackFaceStencilFailStencilOp = SO_Keep,
	EStencilOp BackFaceDepthFailStencilOp = SO_Keep,
	EStencilOp BackFacePassStencilOp = SO_Keep,
	DWORD StencilReadMask = 0xFFFFFFFF,
	DWORD StencilWriteMask = 0xFFFFFFFF,
	DWORD StencilRef = 0
	>
class TStaticStencilState : public TStaticStateRHI<
	TStaticStencilState<
		bEnableFrontFaceStencil,
		FrontFaceStencilTest,
		FrontFaceStencilFailStencilOp,
		FrontFaceDepthFailStencilOp,
		FrontFacePassStencilOp,
		bEnableBackFaceStencil,
		BackFaceStencilTest,
		BackFaceStencilFailStencilOp,
		BackFaceDepthFailStencilOp,
		BackFacePassStencilOp,
		StencilReadMask,
		StencilWriteMask,
		StencilRef
		>,
	FStencilStateRHIRef,
	FStencilStateRHIParamRef
	>
{
public:
	static FStencilStateRHIRef CreateRHI()
	{
		FStencilStateInitializerRHI Initializer(
			bEnableFrontFaceStencil,
			FrontFaceStencilTest,
			FrontFaceStencilFailStencilOp,
			FrontFaceDepthFailStencilOp,
			FrontFacePassStencilOp,
			bEnableBackFaceStencil,
			BackFaceStencilTest,
			BackFaceStencilFailStencilOp,
			BackFaceDepthFailStencilOp,
			BackFacePassStencilOp,
			StencilReadMask,
			StencilWriteMask,
			StencilRef);

		return RHICreateStencilState(Initializer);
	}
};

/**
 * A static RHI blend state resource.
 * TStaticBlendStateRHI<...>::GetStaticState() will return a FBlendStateRHIRef to a blend state with the desired settings.
 * Should only be used from the rendering thread.
 * 
 * Alpha blending happens on GPU's as:
 * FinalColor.rgb = SourceColor * ColorSrcBlend (ColorBlendOp) DestColor * ColorDestBlend;
 * if (BlendState->bSeparateAlphaBlendEnable)
 *		FinalColor.a = SourceAlpha * AlphaSrcBlend (AlphaBlendOp) DestAlpha * AlphaDestBlend;
 * else
 *		Alpha blended the same way as rgb
 * 
 * So for example, TStaticBlendState<BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_Zero,BF_One> produces:
 * FinalColor.rgb = SourceColor * SourceAlpha + DestColor * (1 - SourceAlpha);
 * FinalColor.a = SourceAlpha * 0 + DestAlpha * 1;
 */
template<
	EBlendOperation ColorBlendOp = BO_Add,
	EBlendFactor ColorSrcBlend = BF_One,
	EBlendFactor ColorDestBlend = BF_Zero,
	EBlendOperation AlphaBlendOp = BO_Add,
	EBlendFactor AlphaSrcBlend = BF_One,
	EBlendFactor AlphaDestBlend = BF_Zero,
	ECompareFunction AlphaTest = CF_Always,
	BYTE AlphaRef = 255
	>
class TStaticBlendState : public TStaticStateRHI<
	TStaticBlendState<ColorBlendOp,ColorSrcBlend,ColorDestBlend,AlphaBlendOp,AlphaSrcBlend,AlphaDestBlend,AlphaTest,AlphaRef>,
	FBlendStateRHIRef,
	FBlendStateRHIParamRef
	>
{
public:
	static FBlendStateRHIRef CreateRHI()
	{
		return RHICreateBlendState(FBlendStateInitializerRHI(ColorBlendOp, ColorSrcBlend, ColorDestBlend, AlphaBlendOp, AlphaSrcBlend, AlphaDestBlend, AlphaTest, AlphaRef));
	}
};
