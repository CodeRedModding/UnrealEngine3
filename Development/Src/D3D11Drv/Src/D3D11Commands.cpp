/*=============================================================================
	D3D11Commands.cpp: D3D RHI commands implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D11DrvPrivate.h"
#include "EngineParticleClasses.h"
#include "StaticBoundShaderState.h"
#include "ChartCreation.h"
#include <xnamath.h>

/** Vertex declaration for just one FVector4 position. */
class FVector4VertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.AddItem(FVertexElement(0,0,VET_Float4,VEU_Position,0));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};
FGlobalBoundShaderState GD3D11ClearBoundShaderState;
TGlobalResource<FVector4VertexDeclaration> GD3D11Vector4VertexDeclaration;

// Vertex state.
void FD3D11DynamicRHI::SetStreamSource(UINT StreamIndex,FVertexBufferRHIParamRef VertexBufferRHI,UINT Stride,UINT Offset,UBOOL bUseInstanceIndex,UINT NumVerticesPerInstance,UINT NumInstances)
{
	DYNAMIC_CAST_D3D11RESOURCE(VertexBuffer,VertexBuffer);

	ID3D11Buffer* D3DBuffer = VertexBuffer->Resource;
	Direct3DDeviceIMContext->IASetVertexBuffers(StreamIndex,1,&D3DBuffer,&Stride,&Offset);

	PendingNumInstances = NumInstances;
}

// Rasterizer state.
void FD3D11DynamicRHI::SetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(RasterizerState,NewState);

	CurrentRasterizerState = NewState->RasterizerDesc;
	ID3D11RasterizerState* CachedState = GetCachedRasterizerState(CurrentRasterizerState,CurrentScissorEnable,bCurrentRenderTargetIsMultisample);
	Direct3DDeviceIMContext->RSSetState( CachedState );
}

void FD3D11DynamicRHI::DispatchComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI, UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) 
{ 
	DYNAMIC_CAST_D3D11RESOURCE(ComputeShader,ComputeShader);

	Direct3DDeviceIMContext->CSSetShader(ComputeShader, 0, 0);
	CommitComputeShaderConstants();
	Direct3DDeviceIMContext->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	Direct3DDeviceIMContext->CSSetShader(0, 0, 0);
}

void FD3D11DynamicRHI::SetViewport(UINT MinX,UINT MinY,FLOAT MinZ,UINT MaxX,UINT MaxY,FLOAT MaxZ)
{
	D3D11_VIEWPORT Viewport = { MinX, MinY, MaxX - MinX, MaxY - MinY, MinZ, MaxZ };
	//avoid setting a 0 extent viewport, which the debug runtime doesn't like
	if (Viewport.Width > 0 && Viewport.Height > 0)
	{
		Direct3DDeviceIMContext->RSSetViewports(1,&Viewport);
	}
}

void FD3D11DynamicRHI::SetScissorRect(UBOOL bEnable,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY)
{
	// Defined in UnPlayer.cpp. Used here to disable scissors when doing highres screenshots.
	extern UBOOL GIsTiledScreenshot;
	bEnable = GIsTiledScreenshot ? FALSE : bEnable;

	if(bEnable)
	{
		D3D11_RECT ScissorRect;
		ScissorRect.left = MinX;
		ScissorRect.right = MaxX;
		ScissorRect.top = MinY;
		ScissorRect.bottom = MaxY;
		Direct3DDeviceIMContext->RSSetScissorRects(1,&ScissorRect);
	}

	CurrentScissorEnable = bEnable;
	ID3D11RasterizerState* CachedState = GetCachedRasterizerState(CurrentRasterizerState,CurrentScissorEnable,bCurrentRenderTargetIsMultisample);
	Direct3DDeviceIMContext->RSSetState(CachedState);
}

/**
	* Set depth bounds test state.
	* When enabled, incoming fragments are killed if the value already in the depth buffer is outside of [MinZ, MaxZ]
	*/
void FD3D11DynamicRHI::SetDepthBoundsTest( UBOOL bEnable, const FVector4 &ClipSpaceNearPos, const FVector4 &ClipSpaceFarPos)
{
	// not supported
}

/**
* Set bound shader state. This will set the vertex decl/shader, and pixel shader
* @param BoundShaderState - state resource
*/
void FD3D11DynamicRHI::SetBoundShaderState( FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(BoundShaderState,BoundShaderState);

	Direct3DDeviceIMContext->IASetInputLayout(BoundShaderState->InputLayout);
	Direct3DDeviceIMContext->VSSetShader(BoundShaderState->VertexShader,NULL,0);
	Direct3DDeviceIMContext->PSSetShader(BoundShaderState->PixelShader,NULL,0);

#if WITH_D3D11_TESSELLATION
	Direct3DDeviceIMContext->HSSetShader(BoundShaderState->HullShader,NULL,0);
	Direct3DDeviceIMContext->DSSetShader(BoundShaderState->DomainShader,NULL,0);
	Direct3DDeviceIMContext->GSSetShader(BoundShaderState->GeometryShader,NULL,0);

	if(BoundShaderState->HullShader != NULL && BoundShaderState->DomainShader != NULL)
	{
		bUsingTessellation = TRUE;
	}
	else
	{
		bUsingTessellation = FALSE;
	}
#else
	bUsingTessellation = FALSE;	
#endif

	// @TODO : really should only discard the constants if the shader state has actually changed.
	bDiscardSharedConstants = TRUE;

	// Prevent transient bound shader states from being recreated for each use by keeping a history of the most recently used bound shader states.
	// The history keeps them alive, and the bound shader state cache allows them to be reused if needed.
	BoundShaderStateHistory.Add(BoundShaderState);
}

/** 
	* Set sampler state without modifying texture assignments.  
	* This is only valid for RHI's which support separate sampler state and texture state, like D3D 11.
	*/
void FD3D11DynamicRHI::SetSamplerStateOnly(FPixelShaderRHIParamRef PixelShaderRHI,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(PixelShader,PixelShader);
	DYNAMIC_CAST_D3D11RESOURCE(SamplerState,NewState);

	ID3D11SamplerState* CachedState = GetCachedSamplerState(NewState);
	Direct3DDeviceIMContext->PSSetSamplers(SamplerIndex,1,&CachedState);
}

/** 
	* Set texture state without modifying sampler state.  
	* This is only valid for RHI's which support separate sampler state and texture state, like D3D 11.
	*/
void FD3D11DynamicRHI::SetTextureParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(TextureBase,NewTexture);
	ID3D11ShaderResourceView* SRV = NewTexture->GetShaderResourceView();

	// Trying to set a valid texture with a NULL shader resource view is probably a code mistake
	checkSlow(!NewTexture || SRV);
	Direct3DDeviceIMContext->PSSetShaderResources(TextureIndex,1,&SRV);
	if (NewTexture->RenderTargetView != NULL)
	{
		NewTexture->BoundShaderResourceSlots[SF_Pixel].AddUniqueItem(TextureIndex);
	}
}

/** 
	* Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view.
	*/
void FD3D11DynamicRHI::SetSurfaceParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,FSurfaceRHIParamRef NewSurfaceRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(Surface,NewSurface);
	ID3D11ShaderResourceView* SRV = NewSurface->ShaderResourceView;
	// Trying to set a valid surface with a NULL shader resource view is probably a code mistake
	checkSlow(!NewSurface || SRV);
	Direct3DDeviceIMContext->PSSetShaderResources(TextureIndex,1,&SRV);

	NewSurface->BoundShaderResourceSlots[SF_Pixel].AddUniqueItem(TextureIndex);
}

/** 
	* Set the shader resource view of a surface. This is used for binding TextureMS parameter types that need a multi sampled view. 
	*/
void FD3D11DynamicRHI::SetSurfaceParameter(FComputeShaderRHIParamRef ComputeShaderRHI,UINT TextureIndex,FSurfaceRHIParamRef NewSurfaceRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(Surface,NewSurface);
	ID3D11ShaderResourceView* SRV = NewSurface->ShaderResourceView;
	// Trying to set a valid surface with a NULL shader resource view is probably a code mistake
	checkSlow(!NewSurface || SRV);
	Direct3DDeviceIMContext->CSSetShaderResources(TextureIndex,1,&SRV);

	NewSurface->BoundShaderResourceSlots[SF_Compute].AddUniqueItem(TextureIndex);
}

/** 
	* Set the shader resource view of a surface. This is used for binding UAV to the compute shader.
	*/
void FD3D11DynamicRHI::SetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,UINT TextureIndex,FSurfaceRHIParamRef NewSurfaceRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(Surface,NewSurface);
	ID3D11UnorderedAccessView* UAV = NewSurface->UnorderedAccessView;
	// Trying to set a valid surface with a NULL shader resource view is probably a code mistake (make sure the surface was created with TargetSurfCreate_UAV)
	checkSlow(!NewSurface || UAV);
	Direct3DDeviceIMContext->CSSetUnorderedAccessViews(TextureIndex,1,&UAV,0);

	NewSurface->BoundShaderResourceSlots[SF_Compute].AddUniqueItem(TextureIndex);
}

/**
	* Sets sampler state.
	*
	* @param PixelShader	The pixelshader using the sampler for the next drawcalls.
	* @param TextureIndex	Used as sampler index on all platforms except D3D11, where it's the texture resource index.
	* @param SamplerIndex	Ignored on all platforms except D3D11, where it's the sampler index.
	* @param MipBias		Mip bias to use for the texture
	* @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
	* @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
	*/
void FD3D11DynamicRHI::SetSamplerState(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{
	//@TODO: Support MipBias, LargestMip, SmallestMip

	DYNAMIC_CAST_D3D11RESOURCE(PixelShader,PixelShader);
	DYNAMIC_CAST_D3D11RESOURCE(SamplerState,NewState);
	DYNAMIC_CAST_D3D11RESOURCE(TextureBase,NewTexture);

	ID3D11ShaderResourceView* SRV = NewTexture->GetShaderResourceView();

	// Trying to set a valid texture with a NULL shader resource view is probably a code mistake
	checkSlow(!NewTexture || SRV);
	Direct3DDeviceIMContext->PSSetShaderResources(TextureIndex,1,&SRV);

	ID3D11SamplerState* CachedState = GetCachedSamplerState(NewState);
	Direct3DDeviceIMContext->PSSetSamplers(SamplerIndex,1,&CachedState);

	if (NewTexture->RenderTargetView != NULL || NewTexture->DepthStencilView != NULL)
	{
		NewTexture->BoundShaderResourceSlots[SF_Pixel].AddUniqueItem(TextureIndex);
	}
}

/**
	* Returns the slot index and the size of a mobile uniform parameter.
	*
	* @param ParamName		Name of the uniform parameter to check for.
	* @param OutNumBytes	[out] Set to the size of the parameter value, in bytes, if the parameter was found.
	* @return				Parameter slot index, or -1 if the parameter was not found.
	*/
INT FD3D11DynamicRHI::GetMobileUniformSlotIndexByName(FName ParamName, WORD& OutNumBytes)
{
	// Only used on mobile platforms
	return -1;
}

void FD3D11DynamicRHI::SetMobileTextureSamplerState( FPixelShaderRHIParamRef PixelShader, const INT MobileTextureUnit, FSamplerStateRHIParamRef NewState, FTextureRHIParamRef NewTextureRHI, FLOAT MipBias, FLOAT LargestMip, FLOAT SmallestMip ) 
{
	// Only used on mobile platforms
} 

void FD3D11DynamicRHI::SetMobileSimpleParams(EBlendMode InBlendMode)
{
	// Only used on mobile platforms
}

void FD3D11DynamicRHI::SetMobileMaterialVertexParams(const FMobileMaterialVertexParams& InVertexParams)
{
	// Only used on mobile platforms
}

void FD3D11DynamicRHI::SetMobileMaterialPixelParams(const FMobileMaterialPixelParams& InPixelParams)
{
	// Only used on mobile platforms
}


void FD3D11DynamicRHI::SetMobileMeshVertexParams(const FMobileMeshVertexParams& InMeshParams)
{
	// Only used on mobile platforms
}

void FD3D11DynamicRHI::SetMobileMeshPixelParams(const FMobileMeshPixelParams& InMeshParams)
{
	// Only used on mobile platforms
}

FLOAT FD3D11DynamicRHI::GetMobilePercentColorFade(void)
{
	// Only used on mobile platforms
	return 0.0f;
}


void FD3D11DynamicRHI::SetMobileFogParams (const UBOOL bInEnabled, const FLOAT InFogStart, const FLOAT InFogEnd, const FColor& InFogColor)
{
	// Only used on mobile platforms
}

void FD3D11DynamicRHI::SetMobileHeightFogParams(const FHeightFogParams& Params)
{
	// Only used on mobile platforms
}

void FD3D11DynamicRHI::SetMobileBumpOffsetParams(const UBOOL bInEnabled, const FLOAT InBumpEnd)
{
	// Only used on mobile platforms
}

void FD3D11DynamicRHI::SetMobileGammaCorrection(const UBOOL bInEnabled)
{
	// Only used on mobile platforms
}

void FD3D11DynamicRHI::SetMobileTextureTransformOverride(TMatrix<3,3>& InOverrideTransform)
{
	// Only used on mobile platforms
}

void FD3D11DynamicRHI::SetMobileDistanceFieldParams(const struct FMobileDistanceFieldParams& Params)
{
	// Only used on mobile platforms
}

void FD3D11DynamicRHI::SetMobileColorGradingParams (const FMobileColorGradingParams& Params)
{
	// Only used on mobile platforms
}

void* FD3D11DynamicRHI::GetMobileProgramInstance()
{
	// Only used on mobile platforms
	return NULL;
}

void FD3D11DynamicRHI::SetMobileProgramInstance(void* ProgramInstance)
{
	// Only used on mobile platforms
}

void FD3D11DynamicRHI::ResetTrackedPrimitive()
{
	// Not implemented yet
}

void FD3D11DynamicRHI::CycleTrackedPrimitiveMode()
{
	// Not implemented yet
}

void FD3D11DynamicRHI::IncrementTrackedPrimitive(const INT InDelta)
{
	// Not implemented yet
}

/**
	* Sets sampler state.
	*
	* @param DomainShader	The VertexShader using the sampler for the next drawcalls.
	* @param TextureIndex	Used as sampler index on all platforms except D3D11, where it's the texture resource index.
	* @param SamplerIndex	Ignored on all platforms except D3D11, where it's the sampler index.
	* @param MipBias		Mip bias to use for the texture
	* @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
	* @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
	*/
void FD3D11DynamicRHI::SetVertexTexture(UINT SamplerIndex,FTextureRHIParamRef NewTextureRHI)
{
	// Should not be used for d3d11, should call official API SetSamplerState()
 	check(0);
}

/**
	* Sets sampler state.
	*
	* @param GeometryShaderRHI	The GeometryShader using the sampler for the next drawcalls.
	* @param TextureIndex	Used as sampler index on all platforms except D3D11, where it's the texture resource index.
	* @param SamplerIndex	Ignored on all platforms except D3D11, where it's the sampler index.
	* @param MipBias		Mip bias to use for the texture
	* @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
	* @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
	*/
void FD3D11DynamicRHI::SetSamplerState(FGeometryShaderRHIParamRef GeometryShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ 
	//@TODO: Support MipBias, LargestMip, SmallestMip

	DYNAMIC_CAST_D3D11RESOURCE(GeometryShader,GeometryShader);
	DYNAMIC_CAST_D3D11RESOURCE(SamplerState,NewState);
	DYNAMIC_CAST_D3D11RESOURCE(Texture,NewTexture);

	ID3D11ShaderResourceView* SRV = NewTexture->GetShaderResourceView();

	// Trying to set a valid texture with a NULL shader resource view is probably a code mistake
	checkSlow(!NewTexture || SRV);
	Direct3DDeviceIMContext->GSSetShaderResources(TextureIndex, 1, &SRV);

	ID3D11SamplerState* CachedState = GetCachedSamplerState(NewState);
	Direct3DDeviceIMContext->GSSetSamplers(SamplerIndex, 1, &CachedState);

	if (NewTexture->RenderTargetView != NULL)
	{
		NewTexture->BoundShaderResourceSlots[SF_Geometry].AddUniqueItem(TextureIndex);
	}
}

/**
	* Sets sampler state.
	*
	* @param ComputeShaderRHI	The ComputeShader that wants to use using the sampler
	* @param TextureIndex	Used as sampler index on all platforms except D3D11, where it's the texture resource index.
	* @param SamplerIndex	Ignored on all platforms except D3D11, where it's the sampler index.
	* @param MipBias		Mip bias to use for the texture
	* @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
	* @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
	*/
void FD3D11DynamicRHI::SetSamplerState(FComputeShaderRHIParamRef ComputeShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ 
	//@TODO: Support MipBias, LargestMip, SmallestMip

	DYNAMIC_CAST_D3D11RESOURCE(ComputeShader,ComputeShader);
	DYNAMIC_CAST_D3D11RESOURCE(SamplerState,NewState);
	DYNAMIC_CAST_D3D11RESOURCE(Texture,NewTexture);

	ID3D11ShaderResourceView* SRV = NewTexture->GetShaderResourceView();

	// Trying to set a valid texture with a NULL shader resource view is probably a code mistake
	checkSlow(!NewTexture || SRV);
	Direct3DDeviceIMContext->CSSetShaderResources(TextureIndex, 1, &SRV);

	ID3D11SamplerState* CachedState = GetCachedSamplerState(NewState);
	Direct3DDeviceIMContext->CSSetSamplers(SamplerIndex, 1, &CachedState);

	if (NewTexture->RenderTargetView != NULL)
	{
		NewTexture->BoundShaderResourceSlots[SF_Compute].AddUniqueItem(TextureIndex);
	}
}

void FD3D11DynamicRHI::SetSamplerState(FVertexShaderRHIParamRef VertexShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{
	//@TODO: Support MipBias, LargestMip, SmallestMip

	DYNAMIC_CAST_D3D11RESOURCE(VertexShader,VertexShader);
	DYNAMIC_CAST_D3D11RESOURCE(SamplerState,NewState);
	DYNAMIC_CAST_D3D11RESOURCE(Texture,NewTexture);

	ID3D11ShaderResourceView* SRV = NewTexture->GetShaderResourceView();

	Direct3DDeviceIMContext->VSSetShaderResources(TextureIndex,1,&SRV);

	ID3D11SamplerState* CachedState = GetCachedSamplerState(NewState);
	Direct3DDeviceIMContext->VSSetSamplers(SamplerIndex,1,&CachedState);
}

/**
	* Sets sampler state.
	*
	* @param DomainShader	The DomainShader using the sampler for the next drawcalls.
	* @param TextureIndex	Used as sampler index on all platforms except D3D11, where it's the texture resource index.
	* @param SamplerIndex	Ignored on all platforms except D3D11, where it's the sampler index.
	* @param MipBias		Mip bias to use for the texture
	* @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
	* @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
	*/
void FD3D11DynamicRHI::SetSamplerState(FDomainShaderRHIParamRef DomainShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{
	//@TODO: Support MipBias, LargestMip, SmallestMip

	DYNAMIC_CAST_D3D11RESOURCE(DomainShader,DomainShader);
	DYNAMIC_CAST_D3D11RESOURCE(SamplerState,NewState);
	DYNAMIC_CAST_D3D11RESOURCE(TextureBase,NewTexture);

	ID3D11ShaderResourceView* SRV = NewTexture->GetShaderResourceView();

	// Trying to set a valid texture with a NULL shader resource view is probably a code mistake
	checkSlow(!NewTexture || SRV);
	Direct3DDeviceIMContext->DSSetShaderResources(TextureIndex,1,&SRV);

	ID3D11SamplerState* CachedState = GetCachedSamplerState(NewState);
	Direct3DDeviceIMContext->DSSetSamplers(SamplerIndex,1,&CachedState);

	if (NewTexture->RenderTargetView != NULL)
	{
		NewTexture->BoundShaderResourceSlots[SF_Domain].AddUniqueItem(TextureIndex);
	}
}

/**
	* Sets sampler state.
	*
	* @param HullShader	The HullShader using the sampler for the next drawcalls.
	* @param TextureIndex	Used as sampler index on all platforms except D3D11, where it's the texture resource index.
	* @param SamplerIndex	Ignored on all platforms except D3D11, where it's the sampler index.
	* @param MipBias		Mip bias to use for the texture
	* @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
	* @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
	*/
void FD3D11DynamicRHI::SetSamplerState(FHullShaderRHIParamRef HullShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{
	//@TODO: Support MipBias, LargestMip, SmallestMip

	DYNAMIC_CAST_D3D11RESOURCE(HullShader,HullShader);
	DYNAMIC_CAST_D3D11RESOURCE(SamplerState,NewState);
	DYNAMIC_CAST_D3D11RESOURCE(TextureBase,NewTexture);

	ID3D11ShaderResourceView* SRV = NewTexture->GetShaderResourceView();

	// Trying to set a valid texture with a NULL shader resource view is probably a code mistake
	checkSlow(!NewTexture || SRV);
	Direct3DDeviceIMContext->HSSetShaderResources(TextureIndex,1,&SRV);

	ID3D11SamplerState* CachedState = GetCachedSamplerState(NewState);
	Direct3DDeviceIMContext->HSSetSamplers(SamplerIndex,1,&CachedState);

	if (NewTexture->RenderTargetView != NULL)
	{
		NewTexture->BoundShaderResourceSlots[SF_Hull].AddUniqueItem(TextureIndex);
	}
}

void FD3D11DynamicRHI::ClearSamplerBias()
{
	// Not used
}

void FD3D11DynamicRHI::SetVertexShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	VSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)NewValue,BaseIndex,NumBytes);
}

void FD3D11DynamicRHI::SetDomainShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	DSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)NewValue,BaseIndex,NumBytes);
}

void FD3D11DynamicRHI::SetHullShaderParameter(FHullShaderRHIParamRef HullShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	HSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)NewValue,BaseIndex,NumBytes);
}

/*
	* *** PERFORMANCE WARNING *****
	* This code is from Mikey W @ Xbox 
	* This function is to support single float array parameter in shader
	* This pads 3 more floats for each element - at the expensive of CPU/stack memory
	* Do not overuse this. If you can, use float4 in shader. 
	*/
void FD3D11DynamicRHI::SetVertexShaderFloatArray(FVertexShaderRHIParamRef VertexShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumValues,const FLOAT* FloatValues, INT ParamIndex)
{
	// On D3D, this function takes an array of floats and pads them out to an array of vector4's before setting them as
	// vertex shader constants. This is the proper thing to do for Xenon, although please note the pros and cons
	// when using float arrays. The first con is that there's alot of wasted vertex shader constants that could
	// otherwise be saved if the float array was compressed. However, that would require a fair number of shader
	// instructions to decompress, so the overwhleming pro is that the accessing a float array in the shader is
	// trivial. Another con to be aware of is that the use of more shader constants contributes to "constant
	// waterfalling"...a potential performance problem that can not be predicted, but rather must be measured.

	// Temp storage space for the vector4 padded data. (Note: We might want to make this dynamic, but using
	// the stack is cheap and convenient.)
	const UINT GroupSize = 64;
	XMFLOAT4A pPaddedVectorValues[GroupSize];

	// Process a large set of shader constants in groups of a fixed size
	while( NumValues )
	{
		// Number of values to process this time through the loop
		UINT NumValuesThisGroup = Min( NumValues, GroupSize );
		UINT NumValuesThisGroupDiv4 = (NumValuesThisGroup+4-1)/4;

		// For performance, we use the XnaMath intrinsics. Furthermore, we prefer to use XMLoadFloat4A,
		// if we can guarantee pFloatValues is 16-byte aligned
#if  _WIN64
		BOOL bAligned = (((QWORD)FloatValues)%16)==0 ? TRUE : FALSE;
#else
		BOOL bAligned = (((UINT)FloatValues)%16)==0 ? TRUE : FALSE;
#endif
		if( bAligned )
		{
			XMFLOAT4A* pSrc  = (XMFLOAT4A*)FloatValues;  // pFloatValues is 16-byte aligned
			XMFLOAT4A* pDest = pPaddedVectorValues;

			// Load FLOAT values 4 at a time and store them out as padded FLOAT4 values
			for( UINT i=0; i<NumValuesThisGroupDiv4; i++ )
			{
				XMVECTOR V = XMLoadFloat4A( pSrc++ ); // pFloatValues is 16-byte aligned
				XMVectorGetXPtr( (FLOAT*)pDest++, V ); // Write V.x to first component of pDest
				XMVectorGetYPtr( (FLOAT*)pDest++, V ); // Write V.y to first component of pDest
				XMVectorGetZPtr( (FLOAT*)pDest++, V ); // Write V.w to first component of pDest
				XMVectorGetWPtr( (FLOAT*)pDest++, V ); // Write V.z to first component of pDest
			}
		}
		else
		{
			XMFLOAT4* pSrc  = (XMFLOAT4A*)FloatValues;  // pFloatValues is not aligned
			XMFLOAT4* pDest = pPaddedVectorValues;

			// Load FLOAT values 4 at a time and store them out as padded FLOAT4 values
			for( UINT i=0; i<NumValuesThisGroupDiv4; i++ )
			{
				XMVECTOR V = XMLoadFloat4( pSrc++ ); // pFloatValues is not aligned
				XMVectorGetXPtr( (FLOAT*)pDest++, V ); // Write V.x to first component of pDest
				XMVectorGetYPtr( (FLOAT*)pDest++, V ); // Write V.y to first component of pDest
				XMVectorGetZPtr( (FLOAT*)pDest++, V ); // Write V.w to first component of pDest
				XMVectorGetWPtr( (FLOAT*)pDest++, V ); // Write V.z to first component of pDest
			}
		}

		// Set the newly padded vertex shader constants to the D3DDevice
		//GDirect3DDevice->SetVertexShaderConstantF( BaseIndex, (FLOAT*)pPaddedVectorValues, NumValuesThisGroup );
		VSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)pPaddedVectorValues,BaseIndex,NumValuesThisGroup*sizeof(FLOAT)*4);
		// In case we need to continue processing more values, advance to the next group of values
		NumValues	-= NumValuesThisGroup;
		FloatValues	+= NumValuesThisGroup;
		BaseIndex   += NumValuesThisGroup*16;
	}
}

void FD3D11DynamicRHI::SetVertexShaderBoolParameter(FVertexShaderRHIParamRef VertexShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue)
{
	VSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)&NewValue,BaseIndex,sizeof(BOOL));
}

void FD3D11DynamicRHI::SetPixelShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	checkSlow(PSConstantBuffers(BufferIndex));
	PSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)NewValue,BaseIndex,NumBytes);
}

void FD3D11DynamicRHI::SetPixelShaderBoolParameter(FPixelShaderRHIParamRef PixelShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue)
{
	checkSlow(PSConstantBuffers(BufferIndex));
	PSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)&NewValue,BaseIndex,sizeof(BOOL));
}

void FD3D11DynamicRHI::SetShaderBoolParameter(FHullShaderRHIParamRef HullShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue)
{
	checkSlow(HSConstantBuffers(BufferIndex));
	HSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)&NewValue,BaseIndex,sizeof(BOOL));
}

void FD3D11DynamicRHI::SetShaderBoolParameter(FDomainShaderRHIParamRef DomainShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue)
{
	checkSlow(DSConstantBuffers(BufferIndex));
	DSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)&NewValue,BaseIndex,sizeof(BOOL));
}

void FD3D11DynamicRHI::SetShaderParameter(FHullShaderRHIParamRef HullShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	checkSlow(HSConstantBuffers(BufferIndex));
	HSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)NewValue,BaseIndex,NumBytes);
}

void FD3D11DynamicRHI::SetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	checkSlow(DSConstantBuffers(BufferIndex));
	DSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)NewValue,BaseIndex,NumBytes);
}

void FD3D11DynamicRHI::SetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	checkSlow(VSConstantBuffers(BufferIndex));
	VSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)NewValue,BaseIndex,NumBytes);
}

void FD3D11DynamicRHI::SetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	checkSlow(PSConstantBuffers(BufferIndex));
	PSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)NewValue,BaseIndex,NumBytes);
}

void FD3D11DynamicRHI::SetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	checkSlow(GSConstantBuffers(BufferIndex));
	GSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)NewValue,BaseIndex,NumBytes);
}

void FD3D11DynamicRHI::SetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	checkSlow(CSConstantBuffers(BufferIndex));
	CSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)NewValue,BaseIndex,NumBytes);
}

/**
	* Set engine shader parameters for the view.
	* @param View					The current view
	*/
void FD3D11DynamicRHI::SetViewParameters( const FSceneView& View )
{
	FD3D11DynamicRHI::SetViewParametersWithOverrides(View, View.TranslatedViewProjectionMatrix, View.DiffuseOverrideParameter, View.SpecularOverrideParameter);
}

/**
	* Set engine shader parameters for the view.
	* @param View					The current view
	* @param ViewProjectionMatrix	Matrix that transforms from world space to projection space for the view
	* @param DiffuseOverride		Material diffuse input override
	* @param SpecularOverride		Material specular input override
	*/
void FD3D11DynamicRHI::SetViewParametersWithOverrides( const FSceneView& View, const FMatrix& ViewProjectionMatrix, const FVector4& DiffuseOverride, const FVector4& SpecularOverride )
{
	const FVector4 TranslatedViewOrigin = View.ViewOrigin + FVector4(View.PreViewTranslation,0);

	FVertexShaderOffsetConstantBufferContents VSCBContents;
	VSCBContents.ViewProjectionMatrix = ViewProjectionMatrix;
	VSCBContents.ViewOrigin = TranslatedViewOrigin;
	VSCBContents.PreViewTranslation = View.PreViewTranslation;

	FPixelShaderOffsetConstantBufferContents PSCBContents;
	PSCBContents.ScreenPositionScaleBias = View.ScreenPositionScaleBias;
	PSCBContents.MinZ_MaxZRatio = View.InvDeviceZToWorldZTransform;
	PSCBContents.NvStereoEnabled = nv::stereo::IsStereoEnabled() ? 1.0f : 0.0f;
	PSCBContents.DiffuseOverrideParameter = DiffuseOverride;
	PSCBContents.SpecularOverrideParameter = SpecularOverride;
	PSCBContents.ViewOrigin = View.ViewOrigin;
	PSCBContents.ScreenAndTexelSize = FVector4(View.SizeX, View.SizeY, 1.0f / (FLOAT)View.RenderTargetSizeX, 1.0f / (FLOAT)View.RenderTargetSizeY);

	VSConstantBuffers(VS_VIEW_CONSTANT_BUFFER_INDEX)->UpdateConstant((BYTE*)&VSCBContents,0,sizeof(VSCBContents));

	PSConstantBuffers(PS_VIEW_CONSTANT_BUFFER_INDEX)->UpdateConstant((BYTE*)&PSCBContents,0,sizeof(PSCBContents));

	FHullShaderOffsetConstantBufferContents HSCBContents;
	HSCBContents.ViewProjectionMatrixHS = ViewProjectionMatrix;

	// Engine.ini setting is pixels/tri which is nice and intuitive.  But we want pixels/tessellated edge.  So use a heuristic.
	FLOAT TessellationAdaptivePixelsPerEdge = appSqrt(2.f * GSystemSettings.TessellationAdaptivePixelsPerTriangle);
	HSCBContents.AdaptiveTessellationFactor = 0.5f * FLOAT(View.SizeY) / TessellationAdaptivePixelsPerEdge;
	HSCBContents.ProjectionScaleY = View.ProjectionMatrix.M[1][1];

	FDomainShaderOffsetConstantBufferContents DSCBContents;
	DSCBContents.ViewProjectionMatrixDS = ViewProjectionMatrix;
	DSCBContents.CameraPositionDS = TranslatedViewOrigin;

	HSConstantBuffers(HS_VIEW_CONSTANT_BUFFER_INDEX)->UpdateConstant((BYTE*)&HSCBContents,0,sizeof(HSCBContents));
	DSConstantBuffers(DS_VIEW_CONSTANT_BUFFER_INDEX)->UpdateConstant((BYTE*)&DSCBContents,0,sizeof(DSCBContents));
}

/**
	* Not used on PC
	*/
void FD3D11DynamicRHI::SetViewPixelParameters(const FSceneView* View,FPixelShaderRHIParamRef PixelShader,const class FShaderParameter* SceneDepthCalcParameter,const class FShaderParameter* ScreenPositionScaleBiasParameter,const class FShaderParameter* ScreenAndTexelSizeParameter)
{
}
void FD3D11DynamicRHI::SetRenderTargetBias(  FLOAT ColorBias )
{
}
void FD3D11DynamicRHI::SetShaderRegisterAllocation(UINT NumVertexShaderRegisters, UINT NumPixelShaderRegisters)
{
}
void FD3D11DynamicRHI::ReduceTextureCachePenalty( FPixelShaderRHIParamRef PixelShader )
{
}

void FD3D11DynamicRHI::SetDepthState(FDepthStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(DepthState,NewState);

	CurrentDepthState = NewState->DepthStencilDesc;
	ID3D11DepthStencilState* CachedState = GetCachedDepthStencilState(CurrentDepthState,CurrentStencilState);
	
	if (CurrentDepthSurface
		&& (CurrentDepthState.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ZERO && !bCurrentDSTIsReadonly
		|| CurrentDepthState.DepthWriteMask != D3D11_DEPTH_WRITE_MASK_ZERO && bCurrentDSTIsReadonly))
	{
		bCurrentDSTIsReadonly = CurrentDepthState.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ZERO;
		// Set the appropriate depth stencil view depending on whether depth writes are enabled or not
		if (bCurrentDSTIsReadonly)
		{
			CurrentDepthStencilTarget = CurrentDepthSurface->ReadOnlyDepthStencilView;
		}	
		else
		{
			CurrentDepthStencilTarget = CurrentDepthSurface->DepthStencilView;
		}

		ID3D11RenderTargetView* RTArray[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		for(UINT RenderTargetIndex = 0;RenderTargetIndex < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++RenderTargetIndex)
		{
			RTArray[RenderTargetIndex] = CurrentRenderTargets[RenderTargetIndex];
		}
		Direct3DDeviceIMContext->OMSetRenderTargets(
			D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
			RTArray,
			CurrentDepthStencilTarget
			);
	}

	Direct3DDeviceIMContext->OMSetDepthStencilState(CachedState,CurrentStencilRef);
}

void FD3D11DynamicRHI::SetStencilState(FStencilStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(StencilState,NewState);

	CurrentStencilState = NewState->DepthStencilDesc;
	CurrentStencilRef = NewState->StencilRef;
	ID3D11DepthStencilState* CachedState = GetCachedDepthStencilState(CurrentDepthState,CurrentStencilState);
	Direct3DDeviceIMContext->OMSetDepthStencilState(CachedState,CurrentStencilRef);
}

void FD3D11DynamicRHI::SetBlendState(FBlendStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(BlendState,NewState);

	CurrentBlendState = NewState->BlendDesc;
	CurrentBlendFactor = NewState->BlendFactor;
	ID3D11BlendState* CachedState = GetCachedBlendState(CurrentBlendState,CurrentColorWriteEnable);
	Direct3DDeviceIMContext->OMSetBlendState(CachedState,(FLOAT*)&CurrentBlendFactor,0xFFFFFFFF);
}

void FD3D11DynamicRHI::SetMRTBlendState(FBlendStateRHIParamRef NewStateRHI, UINT TargetIndex)
{
	//@todo: MRT support for D3D11
	check(0);
}

void FD3D11DynamicRHI::SetRenderTarget( FSurfaceRHIParamRef NewRenderTargetRHI, FSurfaceRHIParamRef NewDepthStencilTargetRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(Surface,NewRenderTarget);
	DYNAMIC_CAST_D3D11RESOURCE(Surface,NewDepthStencilTarget);

	ID3D11RenderTargetView* RTV = NULL;
	ID3D11DepthStencilView* DSV = NULL;
	if(NewRenderTarget)
	{
		RTV = NewRenderTarget->RenderTargetView;
	}
	if(NewDepthStencilTarget)
	{
		bCurrentDSTIsReadonly = CurrentDepthState.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ZERO;
		// Set the appropriate depth stencil view depending on whether depth writes are enabled or not
		if (bCurrentDSTIsReadonly)
		{
			DSV = NewDepthStencilTarget->ReadOnlyDepthStencilView;
		}	
		else
		{
			DSV = NewDepthStencilTarget->DepthStencilView;
		}
	}

	UBOOL bMRTSet = FALSE;
	for(UINT RenderTargetIndex = 1;RenderTargetIndex < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++RenderTargetIndex)
	{
		bMRTSet = bMRTSet || CurrentRenderTargets[RenderTargetIndex];
	}

	// Only set the render target if different than the currently bound RT
	if (RTV != CurrentRenderTargets[0] || DSV != CurrentDepthStencilTarget || bMRTSet)
	{
		// Reset all texture references to this render target
		if (NewRenderTarget)
		{
			NewRenderTarget->UnsetTextureReferences(this);
		}
		if (NewDepthStencilTarget)
		{
			NewDepthStencilTarget->UnsetTextureReferences(this);
		}

		CurrentDepthSurface = NewDepthStencilTarget;
		CurrentDepthStencilTarget = DSV;
		CurrentRenderTargets[0] = RTV;
		for(UINT RenderTargetIndex = 1;RenderTargetIndex < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++RenderTargetIndex)
		{
			CurrentRenderTargets[RenderTargetIndex] = NULL;
		}

		ID3D11RenderTargetView* RTArray[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		for(UINT RenderTargetIndex = 0;RenderTargetIndex < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++RenderTargetIndex)
		{
			RTArray[RenderTargetIndex] = CurrentRenderTargets[RenderTargetIndex];
		}
		Direct3DDeviceIMContext->OMSetRenderTargets(
			D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
			RTArray,
			CurrentDepthStencilTarget
			);

#if _DEBUG	
		// A check to allow you to pinpoint what is using mismatching targets
		// We filter our d3ddebug spew that checks for this as the d3d runtime's check is wrong.
		// For filter code, see D3D11Device.cpp look for "OMSETRENDERTARGETS_INVALIDVIEW"
		if(RTV && DSV && CurrentDepthState.DepthEnable)
		{
			// Set the viewport to the full size of the surface
			D3D11_TEXTURE2D_DESC RTTDesc;
			ID3D11Texture2D* RenderTargetTexture = NULL;
			RTV->GetResource((ID3D11Resource**)&RenderTargetTexture);
			RenderTargetTexture->GetDesc(&RTTDesc);
			RenderTargetTexture->Release();

			D3D11_TEXTURE2D_DESC DTTDesc;
			ID3D11Texture2D* DepthTargetTexture = NULL;
			DSV->GetResource((ID3D11Resource**)&DepthTargetTexture);
			DepthTargetTexture->GetDesc(&DTTDesc);
			DepthTargetTexture->Release();

			// enforce color target is <= depth and MSAA settings match
			if(RTTDesc.Width > DTTDesc.Width || RTTDesc.Height > DTTDesc.Height || 
				RTTDesc.SampleDesc.Count != DTTDesc.SampleDesc.Count || 
				RTTDesc.SampleDesc.Quality != DTTDesc.SampleDesc.Quality)
			{
				appErrorf(TEXT("RTV(%i,%i c=%i,q=%i) and DSV(%i,%i c=%i,q=%i) have mismatching dimensions and/or MSAA levels!"),
					RTTDesc.Width,RTTDesc.Height,RTTDesc.SampleDesc.Count,RTTDesc.SampleDesc.Quality,
					DTTDesc.Width,DTTDesc.Height,DTTDesc.SampleDesc.Count,DTTDesc.SampleDesc.Quality);
			}
		}
#endif
	}

	// Detect when the back buffer is being set, and set the correct viewport.
	if( DrawingViewport && (!NewRenderTarget || NewRenderTarget == DrawingViewport->GetBackBuffer()) )
	{
		RHISetViewport(0,0,0.0f,DrawingViewport->GetSizeX(),DrawingViewport->GetSizeY(),1.0f);
	}
	else if(RTV)
	{
		// Set the viewport to the full size of the surface
		D3D11_TEXTURE2D_DESC Desc;
		ID3D11Texture2D* BaseResource = NULL;
		RTV->GetResource((ID3D11Resource**)&BaseResource);
		BaseResource->GetDesc(&Desc);

		RHISetViewport(0,0,0.0f,Desc.Width,Desc.Height,1.0f);

		BaseResource->Release();
	}

	// Determine whether the new render target is multisample.
	if(RTV)
	{
		D3D11_RENDER_TARGET_VIEW_DESC RenderTargetViewDesc;
		RTV->GetDesc(&RenderTargetViewDesc);
		bCurrentRenderTargetIsMultisample =
			(RenderTargetViewDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS) ||
			(RenderTargetViewDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY);

		// Update the rasterizer state to take into account whether new render target is multi-sample.
		ID3D11RasterizerState* CachedState = GetCachedRasterizerState(CurrentRasterizerState,CurrentScissorEnable,bCurrentRenderTargetIsMultisample);
		Direct3DDeviceIMContext->RSSetState(CachedState);
	}
}
void FD3D11DynamicRHI::SetMRTRenderTarget( FSurfaceRHIParamRef NewRenderTargetRHI, UINT TargetIndex)
{
	DYNAMIC_CAST_D3D11RESOURCE(Surface,NewRenderTarget);

	check(TargetIndex < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);

	ID3D11RenderTargetView* RTV = NULL;
	if(NewRenderTarget)
	{
		RTV = NewRenderTarget->RenderTargetView;
	}

	// Only set the render target if different than the currently bound RT
	if (RTV != CurrentRenderTargets[TargetIndex])
	{
		// Reset all texture references to the new render target
		if (NewRenderTarget)
		{
			NewRenderTarget->UnsetTextureReferences(this);
		}

		CurrentRenderTargets[TargetIndex] = RTV;

		ID3D11RenderTargetView* RTArray[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		for(UINT RenderTargetIndex = 0;RenderTargetIndex < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++RenderTargetIndex)
		{
			RTArray[RenderTargetIndex] = CurrentRenderTargets[RenderTargetIndex];
		}
		Direct3DDeviceIMContext->OMSetRenderTargets(
			D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
			RTArray,
			CurrentDepthStencilTarget
			);
	}
}
void FD3D11DynamicRHI::SetColorWriteEnable(UBOOL bEnable)
{
	UINT8 EnabledStateValue = bEnable ? D3D11_COLOR_WRITE_ENABLE_ALL : 0;
	CurrentColorWriteEnable[0] = EnabledStateValue;
	ID3D11BlendState* CachedState = GetCachedBlendState(CurrentBlendState,CurrentColorWriteEnable);
	Direct3DDeviceIMContext->OMSetBlendState(CachedState,(FLOAT*)&CurrentBlendFactor,0xFFFFFFFF);
}
void FD3D11DynamicRHI::SetMRTColorWriteEnable(UBOOL bEnable, UINT TargetIndex)
{
	check(TargetIndex < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);

	UINT8 EnabledStateValue = bEnable ? D3D11_COLOR_WRITE_ENABLE_ALL : 0;
	CurrentColorWriteEnable[TargetIndex] = EnabledStateValue;
	ID3D11BlendState* CachedState = GetCachedBlendState(CurrentBlendState,CurrentColorWriteEnable);
	Direct3DDeviceIMContext->OMSetBlendState(CachedState,(FLOAT*)&CurrentBlendFactor,0xFFFFFFFF);
}
void FD3D11DynamicRHI::SetColorWriteMask(UINT ColorWriteMask)
{
	UINT8 EnabledStateValue;
	EnabledStateValue  = (ColorWriteMask & CW_RED) ? D3D11_COLOR_WRITE_ENABLE_RED : 0;
	EnabledStateValue |= (ColorWriteMask & CW_GREEN) ? D3D11_COLOR_WRITE_ENABLE_GREEN : 0;
	EnabledStateValue |= (ColorWriteMask & CW_BLUE) ? D3D11_COLOR_WRITE_ENABLE_BLUE : 0;
	EnabledStateValue |= (ColorWriteMask & CW_ALPHA) ? D3D11_COLOR_WRITE_ENABLE_ALPHA : 0;

	CurrentColorWriteEnable[0] = EnabledStateValue;
	ID3D11BlendState* CachedState = GetCachedBlendState(CurrentBlendState,CurrentColorWriteEnable);
	Direct3DDeviceIMContext->OMSetBlendState(CachedState,(FLOAT*)&CurrentBlendFactor,0xFFFFFFFF);
}
void FD3D11DynamicRHI::SetMRTColorWriteMask(UINT ColorWriteMask, UINT TargetIndex)
{
	check(TargetIndex < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);

	UINT8 EnabledStateValue;
	EnabledStateValue  = (ColorWriteMask & CW_RED) ? D3D11_COLOR_WRITE_ENABLE_RED : 0;
	EnabledStateValue |= (ColorWriteMask & CW_GREEN) ? D3D11_COLOR_WRITE_ENABLE_GREEN : 0;
	EnabledStateValue |= (ColorWriteMask & CW_BLUE) ? D3D11_COLOR_WRITE_ENABLE_BLUE : 0;
	EnabledStateValue |= (ColorWriteMask & CW_ALPHA) ? D3D11_COLOR_WRITE_ENABLE_ALPHA : 0;

	CurrentColorWriteEnable[TargetIndex] = EnabledStateValue;
	ID3D11BlendState* CachedState = GetCachedBlendState(CurrentBlendState,CurrentColorWriteEnable);
	Direct3DDeviceIMContext->OMSetBlendState(CachedState,(FLOAT*)&CurrentBlendFactor,0xFFFFFFFF);
}
// Not supported
void FD3D11DynamicRHI::BeginHiStencilRecord(UBOOL bCompareFunctionEqual, UINT RefValue) { }
void FD3D11DynamicRHI::BeginHiStencilPlayback(UBOOL bFlush) { }
void FD3D11DynamicRHI::EndHiStencil() { }

// Occlusion queries.
void FD3D11DynamicRHI::BeginOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQueryRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(OcclusionQuery,OcclusionQuery);

	Direct3DDeviceIMContext->Begin(OcclusionQuery->Resource);
}
void FD3D11DynamicRHI::EndOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQueryRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(OcclusionQuery,OcclusionQuery);
	Direct3DDeviceIMContext->End(OcclusionQuery->Resource);

	//@todo - d3d debug spews warnings about OQ's that are being issued but not polled, need to investigate
}

// Primitive drawing.

static D3D11_PRIMITIVE_TOPOLOGY GetD3D11PrimitiveType(UINT PrimitiveType, UBOOL bUsingTessellation)
{
	if(bUsingTessellation)
	{
		switch(PrimitiveType)
		{
		case PT_1_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
		case PT_2_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST;

		// This is the case for tessellation without AEN or other buffers, so just flip to 3 CPs
		case PT_TriangleList: return D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

		case PT_LineList:
		case PT_TriangleStrip:
		case PT_QuadList:
		case PT_PointSprite:
			appErrorf(TEXT("Invalid type specified for tessellated render, probably missing a case in FSkeletalMeshSceneProxy::DrawDynamicElementsByMaterial or FStaticMeshSceneProxy::GetMeshElement"));
			break;
		default:
			// Other cases are valid.
			break;
		};
	}

	switch(PrimitiveType)
	{
	case PT_TriangleList: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case PT_TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case PT_LineList: return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;

	// ControlPointPatchList types will pretend to be TRIANGLELISTS with a stride of N 
	// (where N is the number of control points specified), so we can return them for
	// tessellation and non-tessellation. This functionality is only used when rendering a 
	// default material with something that claims to be tessellated, generally because the 
	// tessellation material failed to compile for some reason.
	case PT_3_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
	case PT_4_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
	case PT_5_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST;
	case PT_6_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST;
	case PT_7_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST;
	case PT_8_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST; 
	case PT_9_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST; 
	case PT_10_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST; 
	case PT_11_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST; 
	case PT_12_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST; 
	case PT_13_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST; 
	case PT_14_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST; 
	case PT_15_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST; 
	case PT_16_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST; 
	case PT_17_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST; 
	case PT_18_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST; 
	case PT_19_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST; 
	case PT_20_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST; 
	case PT_21_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST; 
	case PT_22_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST; 
	case PT_23_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST; 
	case PT_24_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST; 
	case PT_25_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST; 
	case PT_26_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST; 
	case PT_27_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST; 
	case PT_28_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST; 
	case PT_29_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST; 
	case PT_30_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST; 
	case PT_31_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST; 
	case PT_32_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST; 
	default: appErrorf(TEXT("Unknown primitive type: %u"),PrimitiveType);
	};

	return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

static UINT GetVertexCountForPrimitiveCount(UINT NumPrimitives, UINT PrimitiveType)
{
	UINT VertexCount = 0;
	switch(PrimitiveType)
	{
	case PT_TriangleList: VertexCount = NumPrimitives*3; break;
	case PT_TriangleStrip: VertexCount = NumPrimitives+2; break;
	case PT_LineList: VertexCount = NumPrimitives*2; break;

	case PT_1_ControlPointPatchList:
	case PT_2_ControlPointPatchList:
	case PT_3_ControlPointPatchList: 
	case PT_4_ControlPointPatchList: 
	case PT_5_ControlPointPatchList:
	case PT_6_ControlPointPatchList:
	case PT_7_ControlPointPatchList: 
	case PT_8_ControlPointPatchList: 
	case PT_9_ControlPointPatchList: 
	case PT_10_ControlPointPatchList: 
	case PT_11_ControlPointPatchList: 
	case PT_12_ControlPointPatchList: 
	case PT_13_ControlPointPatchList: 
	case PT_14_ControlPointPatchList: 
	case PT_15_ControlPointPatchList: 
	case PT_16_ControlPointPatchList: 
	case PT_17_ControlPointPatchList: 
	case PT_18_ControlPointPatchList: 
	case PT_19_ControlPointPatchList: 
	case PT_20_ControlPointPatchList: 
	case PT_21_ControlPointPatchList: 
	case PT_22_ControlPointPatchList: 
	case PT_23_ControlPointPatchList: 
	case PT_24_ControlPointPatchList: 
	case PT_25_ControlPointPatchList: 
	case PT_26_ControlPointPatchList: 
	case PT_27_ControlPointPatchList: 
	case PT_28_ControlPointPatchList: 
	case PT_29_ControlPointPatchList: 
	case PT_30_ControlPointPatchList: 
	case PT_31_ControlPointPatchList: 
	case PT_32_ControlPointPatchList: 
		VertexCount = (PrimitiveType - PT_1_ControlPointPatchList + 1) * NumPrimitives;
		break;
	default: appErrorf(TEXT("Unknown primitive type: %u"),PrimitiveType);
	};

	return VertexCount;
}

void FD3D11DynamicRHI::CommitNonComputeShaderConstants()
{
	// Commit and bind vertex shader constants
	for(UINT i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
	{
		FD3D11ConstantBuffer* ConstantBuffer = VSConstantBuffers(i);
		// Array may contain NULL entries to pad out to proper 
		if(ConstantBuffer && ConstantBuffer->CommitConstantsToDevice(bDiscardSharedConstants))
		{
			ID3D11Buffer* DeviceBuffer = ConstantBuffer->GetConstantBuffer();
			Direct3DDeviceIMContext->VSSetConstantBuffers(i,1,&DeviceBuffer);
		}
	}

	// Skip HS/DS CB updates in cases where tessellation isn't being used
	// Note that this is *potentially* unsafe because bDiscardSharedConstants is cleared at the
	// end of the function, however we're OK for now because bDiscardSharedConstants
	// is always reset whenever bUsingTessellation changes in SetBoundShaderState()
	if(bUsingTessellation)
	{
		// Commit and bind hull shader constants
		for(UINT i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
		{
			FD3D11ConstantBuffer* ConstantBuffer = HSConstantBuffers(i);
			if(ConstantBuffer && ConstantBuffer->CommitConstantsToDevice(bDiscardSharedConstants))
			{
				ID3D11Buffer* DeviceBuffer = ConstantBuffer->GetConstantBuffer();
				Direct3DDeviceIMContext->HSSetConstantBuffers(i,1,&DeviceBuffer);
			}
		}

		// Commit and bind domain shader constants
		for(UINT i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
		{
			FD3D11ConstantBuffer* ConstantBuffer = DSConstantBuffers(i);
			if(ConstantBuffer && ConstantBuffer->CommitConstantsToDevice(bDiscardSharedConstants))
			{
				ID3D11Buffer* DeviceBuffer = ConstantBuffer->GetConstantBuffer();
				Direct3DDeviceIMContext->DSSetConstantBuffers(i,1,&DeviceBuffer);
			}
		}
	}

	// Commit and bind geometry shader constants
	for(UINT i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
	{
		FD3D11ConstantBuffer* ConstantBuffer = GSConstantBuffers(i);
		if(ConstantBuffer && ConstantBuffer->CommitConstantsToDevice(bDiscardSharedConstants))
		{
			ID3D11Buffer* DeviceBuffer = ConstantBuffer->GetConstantBuffer();
			Direct3DDeviceIMContext->GSSetConstantBuffers(i,1,&DeviceBuffer);
		}
	}

	// Commit and bind pixel shader constants
	for(UINT i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
	{
		FD3D11ConstantBuffer* ConstantBuffer = PSConstantBuffers(i);
		if(ConstantBuffer && ConstantBuffer->CommitConstantsToDevice(bDiscardSharedConstants))
		{
			ID3D11Buffer* DeviceBuffer = ConstantBuffer->GetConstantBuffer();
			Direct3DDeviceIMContext->PSSetConstantBuffers(i,1,&DeviceBuffer);
		}
	}

	bDiscardSharedConstants = FALSE;
}

void FD3D11DynamicRHI::CommitComputeShaderConstants()
{
	UBOOL bLocalDiscardSharedConstants = TRUE;

	// Commit and bind compute shader constants
	for(UINT i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
	{
		FD3D11ConstantBuffer* ConstantBuffer = CSConstantBuffers(i);
		if(ConstantBuffer && ConstantBuffer->CommitConstantsToDevice(bLocalDiscardSharedConstants))
		{
			ID3D11Buffer* DeviceBuffer = ConstantBuffer->GetConstantBuffer();
			Direct3DDeviceIMContext->CSSetConstantBuffers(i,1,&DeviceBuffer);
		}
	}
}

void FD3D11DynamicRHI::DrawPrimitive(UINT PrimitiveType,UINT BaseVertexIndex,UINT NumPrimitives)
{
	INC_DWORD_STAT(STAT_D3D11DrawPrimitiveCalls);
	INC_DWORD_STAT_BY(STAT_D3D11Triangles,(DWORD)(PrimitiveType != PT_LineList ? NumPrimitives : 0));
	INC_DWORD_STAT_BY(STAT_D3D11Lines,(DWORD)(PrimitiveType == PT_LineList ? NumPrimitives : 0));

	if (bTrackingEvents && CurrentEventNode)
	{
		CurrentEventNode->NumDraws++;
		CurrentEventNode->NumPrimitives += NumPrimitives;
	}

	CommitNonComputeShaderConstants();
	UINT VertexCount = GetVertexCountForPrimitiveCount(NumPrimitives,PrimitiveType);
	Direct3DDeviceIMContext->IASetPrimitiveTopology(GetD3D11PrimitiveType(PrimitiveType,bUsingTessellation));

	if(PendingNumInstances > 1)
	{
		Direct3DDeviceIMContext->DrawInstanced(VertexCount,PendingNumInstances,BaseVertexIndex,0);
	}
	else
	{
		Direct3DDeviceIMContext->Draw(VertexCount,BaseVertexIndex);
	}
}

void FD3D11DynamicRHI::DrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI,UINT PrimitiveType,INT BaseVertexIndex,UINT MinIndex,UINT NumVertices,UINT StartIndex,UINT NumPrimitives)
{
	DYNAMIC_CAST_D3D11RESOURCE(IndexBuffer,IndexBuffer);

	INC_DWORD_STAT(STAT_D3D11DrawPrimitiveCalls);
	INC_DWORD_STAT_BY(STAT_D3D11Triangles,(DWORD)(PrimitiveType != PT_LineList ? NumPrimitives : 0));
	INC_DWORD_STAT_BY(STAT_D3D11Lines,(DWORD)(PrimitiveType == PT_LineList ? NumPrimitives : 0));

	if (bTrackingEvents && CurrentEventNode)
	{
		CurrentEventNode->NumDraws++;
		CurrentEventNode->NumPrimitives += NumPrimitives;
	}

	CommitNonComputeShaderConstants();
	// determine 16bit vs 32bit indices
	UINT SizeFormat = sizeof(DXGI_FORMAT);
	const DXGI_FORMAT Format = (IndexBuffer->Stride == sizeof(WORD) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);

	UINT IndexCount = GetVertexCountForPrimitiveCount(NumPrimitives,PrimitiveType);
	Direct3DDeviceIMContext->IASetIndexBuffer(IndexBuffer->Resource,Format,0);
	Direct3DDeviceIMContext->IASetPrimitiveTopology(GetD3D11PrimitiveType(PrimitiveType,bUsingTessellation));

	if(PendingNumInstances > 1)
	{
		Direct3DDeviceIMContext->DrawIndexedInstanced(IndexCount,PendingNumInstances,StartIndex,BaseVertexIndex,0);
	}
	else
	{
		Direct3DDeviceIMContext->DrawIndexed(IndexCount,StartIndex,BaseVertexIndex);
	}
}

void FD3D11DynamicRHI::DrawIndexedPrimitive_PreVertexShaderCulling(FIndexBufferRHIParamRef IndexBuffer,UINT PrimitiveType,INT BaseVertexIndex,UINT MinIndex,UINT NumVertices,UINT StartIndex,UINT NumPrimitives,const FMatrix& LocalToWorld,const void* PlatformMeshData)
{
	// On PC, don't use pre-vertex-shader culling.
	DrawIndexedPrimitive(IndexBuffer,PrimitiveType,BaseVertexIndex,MinIndex,NumVertices,StartIndex,NumPrimitives);
}

void* FD3D11DynamicRHI::CreateVertexDataBuffer(UINT Size)
{
	if( Size > StaticDataSize )
	{
		if( StaticData )
		{
			appFree(StaticData);
		}

		StaticDataSize = Size;
		StaticData = appMalloc( Max<UINT>(StaticDataSize,UserDataBufferSize) );
	}
	return StaticData;
}

void FD3D11DynamicRHI::ReleaseDynamicVBandIBBuffers()
{
	for(UINT i = 0;i < NumUserDataBuffers;i++)
	{
		if(DynamicVertexBufferArray[i])
		{
			DynamicVertexBufferArray[i] = NULL;
		}

		if(DynamicIndexBufferArray[i])
		{
			DynamicIndexBufferArray[i] = NULL;
		}
	}

	if(StaticData)
	{
		appFree(StaticData);
	}
}

/**
	* Returns an appropriate D3D11 buffer from an array of buffers.  It also makes sure the buffer is of the proper size.
	* @param Count The number of objects in the buffer
	* @param Stride The stride of each object
	* @param Binding Which type of binding (VB or IB) this buffer will need
	*/
ID3D11Buffer* FD3D11DynamicRHI::EnsureBufferSize(UINT Count, UINT Stride, D3D11_BIND_FLAG Binding)
{
	TRefCountPtr<ID3D11Buffer>* BufferArray = (Binding & D3D11_BIND_VERTEX_BUFFER) ? DynamicVertexBufferArray : DynamicIndexBufferArray;
	UINT* Current = (Binding & D3D11_BIND_VERTEX_BUFFER) ? &CurrentDynamicVB : &CurrentDynamicIB;
	UBOOL CreateBuffer = FALSE;
	UINT SizeNeeded = Count*Stride;

	if(BufferArray[*Current])
	{
		D3D11_BUFFER_DESC Desc;
		BufferArray[*Current]->GetDesc(&Desc);
		if(Desc.ByteWidth < SizeNeeded)
		{
			CreateBuffer = TRUE;
			// This will internally release the buffer
			BufferArray[*Current] = NULL;
		}
	}
	else
	{
		CreateBuffer = TRUE;
	}

	if(CreateBuffer)
	{
		D3D11_BUFFER_DESC Desc;
		Desc.ByteWidth = Max<UINT>(SizeNeeded,UserDataBufferSize);
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = Binding;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Desc.MiscFlags = 0;
		VERIFYD3D11RESULT(Direct3DDevice->CreateBuffer(&Desc,NULL,BufferArray[*Current].GetInitReference()));
	}

	ID3D11Buffer* retBuffer = BufferArray[*Current];
	(*Current) ++;
	if(*Current >= NumUserDataBuffers)
	{
		*Current = 0;
	}
	return retBuffer;
}

/**
	* Fills a D3D11 buffer with the input data.  Returns the D3D11 buffer with the data.
	* @param Count The number of objects in the buffer
	* @param Stride The stride of each object
	* @param Data The actual buffer data
	* @param Binding Which type of binding (VB or IB) this buffer will need
	*/
ID3D11Buffer* FD3D11DynamicRHI::FillD3D11Buffer(UINT Count, UINT Stride, const void* Data, D3D11_BIND_FLAG Binding, UINT& OutOffset)
{
	UINT* CurrentOffset = (Binding & D3D11_BIND_VERTEX_BUFFER) ? &CurrentVBOffset : &CurrentIBOffset;

	UINT NewOffset = *CurrentOffset + Count*Stride;
	D3D11_MAP MapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	if( NewOffset >= UserDataBufferSize )
	{
		*CurrentOffset = 0;
		NewOffset = *CurrentOffset + Count*Stride;

		// When we start over, we could overwrite regions that are still in use by the GPU, therefore
		// we use discard instead of no-overwrite to tell D3D to treat the two sets of data as separate
		MapType = D3D11_MAP_WRITE_DISCARD;
	}
	OutOffset = *CurrentOffset;
	*CurrentOffset = NewOffset;

	ID3D11Buffer* Buffer = EnsureBufferSize(Count,Stride,Binding);
	D3D11_MAPPED_SUBRESOURCE mappedSubresource;
	Direct3DDeviceIMContext->Map(Buffer,0,MapType,0,&mappedSubresource);
	BYTE* MappedData = static_cast<BYTE*>(mappedSubresource.pData);
	appMemcpy(MappedData+OutOffset,Data,Count*Stride);
	Direct3DDeviceIMContext->Unmap(Buffer,0);

	return Buffer;
}

/**
	* Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawPrimitiveUP
	* @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
	* @param NumPrimitives The number of primitives in the VertexData buffer
	* @param NumVertices The number of vertices to be written
	* @param VertexDataStride Size of each vertex 
	* @param OutVertexData Reference to the allocated vertex memory
	*/
void FD3D11DynamicRHI::BeginDrawPrimitiveUP( UINT PrimitiveType, UINT NumPrimitives, UINT NumVertices, UINT VertexDataStride, void*& OutVertexData)
{
	checkSlow(NULL == PendingDrawPrimitiveUPVertexData);
	PendingDrawPrimitiveUPVertexData = CreateVertexDataBuffer(NumVertices * VertexDataStride);
	OutVertexData = PendingDrawPrimitiveUPVertexData;

	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingNumVertices = NumVertices;
	PendingVertexDataStride = VertexDataStride;
}

/**
	* Draw a primitive using the vertex data populated since RHIBeginDrawPrimitiveUP and clean up any memory as needed
	*/
void FD3D11DynamicRHI::EndDrawPrimitiveUP()
{
	checkSlow(NULL != PendingDrawPrimitiveUPVertexData);

	CommitNonComputeShaderConstants();

	if (bTrackingEvents && CurrentEventNode)
	{
		CurrentEventNode->NumDraws++;
		CurrentEventNode->NumPrimitives += PendingNumPrimitives;
	}

	// for now (while RHIDrawPrimitiveUP still exists), just call it because it does the same work we need here
	RHIDrawPrimitiveUP(PendingPrimitiveType, PendingNumPrimitives, PendingDrawPrimitiveUPVertexData, PendingVertexDataStride);

	// free used mem
	PendingDrawPrimitiveUPVertexData = NULL;
}
/**
	* Draw a primitive using the vertices passed in
	* VertexData is NOT created by BeginDrawPrimitiveUP
	* @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
	* @param NumPrimitives The number of primitives in the VertexData buffer
	* @param VertexData A reference to memory preallocate in RHIBeginDrawPrimitiveUP
	* @param VertexDataStride The size of one vertex
	*/
void FD3D11DynamicRHI::DrawPrimitiveUP( UINT PrimitiveType, UINT NumPrimitives, const void* VertexData,UINT VertexDataStride)
{
	INC_DWORD_STAT(STAT_D3D11DrawPrimitiveCalls);
	INC_DWORD_STAT_BY(STAT_D3D11Triangles,(DWORD)(PrimitiveType != PT_LineList ? NumPrimitives : 0));
	INC_DWORD_STAT_BY(STAT_D3D11Lines,(DWORD)(PrimitiveType == PT_LineList ? NumPrimitives : 0));

	// tessellation only supports trilists
	checkSlow(!bUsingTessellation || PrimitiveType == PT_TriangleList);

	UINT VertexCount = GetVertexCountForPrimitiveCount(NumPrimitives,PrimitiveType);
	UINT DrawOffset = 0;
	ID3D11Buffer* Buffer = FillD3D11Buffer(VertexCount,VertexDataStride,VertexData,D3D11_BIND_VERTEX_BUFFER,DrawOffset);
	UINT Offset = DrawOffset;

	if (bTrackingEvents && CurrentEventNode)
	{
		CurrentEventNode->NumDraws++;
		CurrentEventNode->NumPrimitives += NumPrimitives;
	}

	CommitNonComputeShaderConstants();
	Direct3DDeviceIMContext->IASetVertexBuffers(0,1,&Buffer,&VertexDataStride,&Offset);
	Direct3DDeviceIMContext->IASetPrimitiveTopology(GetD3D11PrimitiveType(PrimitiveType,bUsingTessellation));
	Direct3DDeviceIMContext->Draw(VertexCount,0);
}

/**
	* Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawIndexedPrimitiveUP
	* @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
	* @param NumPrimitives The number of primitives in the VertexData buffer
	* @param NumVertices The number of vertices to be written
	* @param VertexDataStride Size of each vertex
	* @param OutVertexData Reference to the allocated vertex memory
	* @param MinVertexIndex The lowest vertex index used by the index buffer
	* @param NumIndices Number of indices to be written
	* @param IndexDataStride Size of each index (either 2 or 4 bytes)
	* @param OutIndexData Reference to the allocated index memory
	*/
void FD3D11DynamicRHI::BeginDrawIndexedPrimitiveUP( UINT PrimitiveType, UINT NumPrimitives, UINT NumVertices, UINT VertexDataStride, void*& OutVertexData, UINT MinVertexIndex, UINT NumIndices, UINT IndexDataStride, void*& OutIndexData)
{
	checkSlow(NULL == PendingDrawPrimitiveUPVertexData);
	checkSlow(NULL == PendingDrawPrimitiveUPIndexData);

	PendingDrawPrimitiveUPVertexData = CreateVertexDataBuffer(NumVertices * VertexDataStride + NumIndices * IndexDataStride);
	OutVertexData = PendingDrawPrimitiveUPVertexData;

	PendingDrawPrimitiveUPIndexData = (BYTE*)PendingDrawPrimitiveUPVertexData + (NumVertices * VertexDataStride);
	OutIndexData = PendingDrawPrimitiveUPIndexData;

	checkSlow((sizeof(WORD) == IndexDataStride) || (sizeof(DWORD) == IndexDataStride));

	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingMinVertexIndex = MinVertexIndex;
	PendingIndexDataStride = IndexDataStride;

	PendingNumVertices = NumVertices;
	PendingVertexDataStride = VertexDataStride;
}

/**
	* Draw a primitive using the vertex and index data populated since RHIBeginDrawIndexedPrimitiveUP and clean up any memory as needed
	*/
void FD3D11DynamicRHI::EndDrawIndexedPrimitiveUP()
{
	checkSlow(NULL != PendingDrawPrimitiveUPVertexData);
	checkSlow(NULL != PendingDrawPrimitiveUPIndexData);

	CommitNonComputeShaderConstants();

	if (bTrackingEvents && CurrentEventNode)
	{
		CurrentEventNode->NumDraws++;
		CurrentEventNode->NumPrimitives += PendingNumPrimitives;
	}

	// for now (while RHIDrawPrimitiveUP still exists), just call it because it does the same work we need here
	RHIDrawIndexedPrimitiveUP( PendingPrimitiveType, PendingMinVertexIndex, PendingNumVertices, PendingNumPrimitives, PendingDrawPrimitiveUPIndexData, PendingIndexDataStride, PendingDrawPrimitiveUPVertexData, PendingVertexDataStride);
	
	// free used mem
	PendingDrawPrimitiveUPIndexData = NULL;
	PendingDrawPrimitiveUPVertexData = NULL;
}

/**
	* Draw a primitive using the vertices passed in as described the passed in indices. 
	* IndexData and VertexData are NOT created by BeginDrawIndexedPrimitveUP
	* @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
	* @param MinVertexIndex The lowest vertex index used by the index buffer
	* @param NumVertices The number of vertices in the vertex buffer
	* @param NumPrimitives THe number of primitives described by the index buffer
	* @param IndexData The memory preallocated in RHIBeginDrawIndexedPrimitiveUP
	* @param IndexDataStride The size of one index
	* @param VertexData The memory preallocate in RHIBeginDrawIndexedPrimitiveUP
	* @param VertexDataStride The size of one vertex
	*/
void FD3D11DynamicRHI::DrawIndexedPrimitiveUP( UINT PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT NumPrimitives, const void* IndexData, UINT IndexDataStride, const void* VertexData, UINT VertexDataStride)
{
	INC_DWORD_STAT(STAT_D3D11DrawPrimitiveCalls);
	INC_DWORD_STAT_BY(STAT_D3D11Triangles,(DWORD)(PrimitiveType != PT_LineList ? NumPrimitives : 0));
	INC_DWORD_STAT_BY(STAT_D3D11Lines,(DWORD)(PrimitiveType == PT_LineList ? NumPrimitives : 0));

	// tessellation only supports trilists
	checkSlow(!bUsingTessellation || PrimitiveType == PT_TriangleList);

	// create a buffer on the fly
	UINT VertexCount = NumVertices;
	UINT IndexCount = GetVertexCountForPrimitiveCount(NumPrimitives,PrimitiveType);
	UINT DrawOffsetVB = 0;
	UINT DrawOffsetIB = 0;
	ID3D11Buffer* VertexBuffer = FillD3D11Buffer(VertexCount,VertexDataStride,VertexData,D3D11_BIND_VERTEX_BUFFER,DrawOffsetVB);
	ID3D11Buffer* IndexBuffer = FillD3D11Buffer(IndexCount,IndexDataStride,IndexData,D3D11_BIND_INDEX_BUFFER,DrawOffsetIB);
	UINT Offset = DrawOffsetVB;

	if (bTrackingEvents && CurrentEventNode)
	{
		CurrentEventNode->NumDraws++;
		CurrentEventNode->NumPrimitives += NumPrimitives;
	}

	CommitNonComputeShaderConstants();
	Direct3DDeviceIMContext->IASetVertexBuffers(0,1,&VertexBuffer,&VertexDataStride,&Offset);
	Direct3DDeviceIMContext->IASetIndexBuffer(IndexBuffer,
			IndexDataStride == sizeof(WORD) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
			DrawOffsetIB);
	Direct3DDeviceIMContext->IASetPrimitiveTopology(GetD3D11PrimitiveType(PrimitiveType,bUsingTessellation));
	Direct3DDeviceIMContext->DrawIndexed(IndexCount,0,MinVertexIndex);
}

//
/**
	* Draw a sprite particle emitter.
	*
	* @param Mesh The mesh element containing the data for rendering the sprite particles
	*/
void FD3D11DynamicRHI::DrawSpriteParticles( const FMeshBatch& Mesh)
{
	checkSlow(Mesh.DynamicVertexData);
	FDynamicSpriteEmitterData* SpriteData = (FDynamicSpriteEmitterData*)(Mesh.DynamicVertexData);

	// Sort the particles if required
	INT ParticleCount = SpriteData->GetSource().ActiveParticleCount;

	// 'clamp' the number of particles actually drawn
	//@todo.SAS. If sorted, we really want to render the front 'N' particles...
	// right now it renders the back ones. (Same for SubUV draws)
	INT StartIndex = 0;
	INT EndIndex = ParticleCount;
	if ((SpriteData->Source.MaxDrawCount >= 0) && (ParticleCount > SpriteData->Source.MaxDrawCount))
	{
		ParticleCount = SpriteData->Source.MaxDrawCount;
	}

	// Render the particles are indexed tri-lists
	void* OutVertexData = NULL;
	void* OutIndexData = NULL;

	// Get the memory from the device for copying the particle vertex/index data to
	RHIBeginDrawIndexedPrimitiveUP( PT_TriangleList, 
		ParticleCount * 2, ParticleCount * 4, Mesh.DynamicVertexStride, OutVertexData, 
		0, ParticleCount * 6, sizeof(WORD), OutIndexData);

	if (OutVertexData && OutIndexData)
	{
		// Pack the data
		FParticleSpriteVertex* Vertices = (FParticleSpriteVertex*)OutVertexData;
		// todo : support batching
		SpriteData->GetVertexAndIndexData(Vertices, OutIndexData, (FParticleOrder*)(Mesh.Elements(0).DynamicIndexData));
		// End the draw, which will submit the data for rendering
		RHIEndDrawIndexedPrimitiveUP();
	}
}

/**
	* Draw a sprite subuv particle emitter.
	*
	* @param Mesh The mesh element containing the data for rendering the sprite subuv particles
	*/
void FD3D11DynamicRHI::DrawSubUVParticles( const FMeshBatch& Mesh)
{
	checkSlow(Mesh.DynamicVertexData);
	FDynamicSubUVEmitterData* SubUVData = (FDynamicSubUVEmitterData*)(Mesh.DynamicVertexData);

	// Sort the particles if required
	INT ParticleCount = SubUVData->Source.ActiveParticleCount;

	// 'clamp' the number of particles actually drawn
	//@todo.SAS. If sorted, we really want to render the front 'N' particles...
	// right now it renders the back ones. (Same for SubUV draws)
	INT StartIndex = 0;
	INT EndIndex = ParticleCount;
	if ((SubUVData->Source.MaxDrawCount >= 0) && (ParticleCount > SubUVData->Source.MaxDrawCount))
	{
		ParticleCount = SubUVData->Source.MaxDrawCount;
	}

	// Render the particles are indexed tri-lists
	void* OutVertexData = NULL;
	void* OutIndexData = NULL;

	// Get the memory from the device for copying the particle vertex/index data to
	RHIBeginDrawIndexedPrimitiveUP( PT_TriangleList, 
		ParticleCount * 2, ParticleCount * 4, Mesh.DynamicVertexStride, OutVertexData, 
		0, ParticleCount * 6, sizeof(WORD), OutIndexData);

	if (OutVertexData && OutIndexData)
	{
		// Pack the data
		FParticleSpriteSubUVVertex* Vertices = (FParticleSpriteSubUVVertex*)OutVertexData;
		// todo : support batching
		SubUVData->GetVertexAndIndexData(Vertices, OutIndexData, (FParticleOrder*)(Mesh.Elements(0).DynamicIndexData));
		// End the draw, which will submit the data for rendering
		RHIEndDrawIndexedPrimitiveUP();
	}
}

/**
	* Draw a point sprite particle emitter.
	*
	* @param Mesh The mesh element containing the data for rendering the sprite subuv particles
	*/
void FD3D11DynamicRHI::DrawPointSpriteParticles(const FMeshBatch& Mesh) 
{
	// Not implemented yet!
}

// Raster operations.
void FD3D11DynamicRHI::Clear(UBOOL bClearColor,const FLinearColor& Color,UBOOL bClearDepth,FLOAT Depth,UBOOL bClearStencil,DWORD Stencil)
{
	ID3D11RenderTargetView* pRTV = NULL;
	ID3D11DepthStencilView* pDSV = NULL;
	Direct3DDeviceIMContext->OMGetRenderTargets(1,&pRTV,&pDSV);

	ID3D11DepthStencilView* OriginalDSV = pDSV;

	// If we're clearing depth or stencil and we have a readonly depth stencil view bound, we need to use a writable depth view instead
	if ((bClearDepth || bClearStencil) && CurrentDepthSurface && bCurrentDSTIsReadonly)
	{
		pDSV = CurrentDepthSurface->DepthStencilView;
	}

	// Determine if we're trying to clear a subrect of the screen
	UBOOL UseDrawClear = FALSE;
	UINT NumViews = 1;
	D3D11_VIEWPORT Viewport;
	Direct3DDeviceIMContext->RSGetViewports(&NumViews,&Viewport);
	if(Viewport.TopLeftX > 0 || Viewport.TopLeftY > 0)
	{
		UseDrawClear = TRUE;
	}
	if(CurrentScissorEnable)
	{
		UINT NumRects = 0;
		Direct3DDeviceIMContext->RSGetScissorRects(&NumRects,NULL);
		if(NumRects > 0)
		{
			UseDrawClear = TRUE;
		}
	}

	if(!UseDrawClear)
	{
		UINT Width = 0;
		UINT Height = 0;
		if(pRTV)
		{
			ID3D11Texture2D* BaseTexture = NULL;
			pRTV->GetResource((ID3D11Resource**)&BaseTexture);
			D3D11_TEXTURE2D_DESC Desc;
			BaseTexture->GetDesc(&Desc);
			Width = Desc.Width;
			Height = Desc.Height;
			BaseTexture->Release();
		}
		else if(pDSV)
		{
			ID3D11Texture2D* BaseTexture = NULL;
			pDSV->GetResource((ID3D11Resource**)&BaseTexture);
			D3D11_TEXTURE2D_DESC Desc;
			BaseTexture->GetDesc(&Desc);
			Width = Desc.Width;
			Height = Desc.Height;
			BaseTexture->Release();
		}

		if((Viewport.Width < Width || Viewport.Height < Height) 
			&& (Viewport.Width > 1 && Viewport.Height > 1))
		{
			UseDrawClear = TRUE;
		}
	}

	if(UseDrawClear)
	{
		if (CurrentDepthSurface)
		{
			// Clear all texture references to this depth buffer
			CurrentDepthSurface->UnsetTextureReferences(this);
		}

		// Store the old states here
		ID3D11DepthStencilState* OldDepthStencilState = NULL;
		ID3D11RasterizerState* OldRasterizerState = NULL;
		ID3D11BlendState* OldBlendState = NULL;
		UINT StencilRef = 0;
		FLOAT BlendFactor[4];
		UINT SampleMask = 0;
		Direct3DDeviceIMContext->OMGetDepthStencilState(&OldDepthStencilState,&StencilRef);
		Direct3DDeviceIMContext->OMGetBlendState(&OldBlendState,BlendFactor,&SampleMask);
		Direct3DDeviceIMContext->RSGetState(&OldRasterizerState);

		// Set new states
		FBlendStateRHIParamRef BlendStateRHI = TStaticBlendState<>::GetRHI();
		FRasterizerStateRHIParamRef RasterizerStateRHI = TStaticRasterizerState<FM_Solid,CM_None>::GetRHI();
		FLOAT BF[4] = {0,0,0,0};
		FDepthStateRHIParamRef DepthStateRHI;
		FStencilStateRHIParamRef StencilStateRHI;

		TStaticArray<UINT8,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> ColorWriteMask =
			bClearColor && pRTV
			? CurrentColorWriteEnable
			: MakeUniformStaticArray<UINT8,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>(0);

		if(bClearDepth)
		{
			DepthStateRHI = TStaticDepthState<TRUE, CF_Always>::GetRHI();
		}
		else
		{
			DepthStateRHI = TStaticDepthState<FALSE, CF_Always>::GetRHI();
		}

		UBOOL bChangedDepthStencilTarget = FALSE;
		if (CurrentDepthSurface && (bClearDepth || bClearStencil) && bCurrentDSTIsReadonly)
		{
			bChangedDepthStencilTarget = TRUE;
			// Bind the writable DST if necessary
			CurrentDepthStencilTarget = CurrentDepthSurface->DepthStencilView;

			ID3D11RenderTargetView* RTArray[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
			for(UINT RenderTargetIndex = 0;RenderTargetIndex < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++RenderTargetIndex)
			{
				RTArray[RenderTargetIndex] = CurrentRenderTargets[RenderTargetIndex];
			}
			Direct3DDeviceIMContext->OMSetRenderTargets(
				D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
				RTArray,
				CurrentDepthStencilTarget
				);
		}

		if(bClearStencil)
		{
			StencilStateRHI = TStaticStencilState<
				TRUE,CF_Always,SO_Replace,SO_Replace,SO_Replace,
				FALSE,CF_Always,SO_Replace,SO_Replace,SO_Replace,
				0xff,0xff,1
				>::GetRHI();
		}
		else
		{
			StencilStateRHI = TStaticStencilState<>::GetRHI();
		}

		DYNAMIC_CAST_D3D11RESOURCE(BlendState,BlendState);
		DYNAMIC_CAST_D3D11RESOURCE(RasterizerState,RasterizerState);
		DYNAMIC_CAST_D3D11RESOURCE(DepthState,DepthState);
		DYNAMIC_CAST_D3D11RESOURCE(StencilState,StencilState);

		// Set the cached state objects
		ID3D11BlendState* CachedBlendState = GetCachedBlendState(BlendState->BlendDesc,ColorWriteMask);
		ID3D11DepthStencilState* CachedDepthStencilState = GetCachedDepthStencilState(DepthState->DepthStencilDesc,StencilState->DepthStencilDesc);
		ID3D11RasterizerState* CachedRasterizerState = GetCachedRasterizerState(RasterizerState->RasterizerDesc,CurrentScissorEnable,bCurrentRenderTargetIsMultisample);
		Direct3DDeviceIMContext->OMSetBlendState(CachedBlendState,BF,0xFFFFFFFF);
		Direct3DDeviceIMContext->OMSetDepthStencilState(CachedDepthStencilState,Stencil);
		Direct3DDeviceIMContext->RSSetState(CachedRasterizerState);

		// Store the old shaders
		ID3D11VertexShader* VSOld = NULL;
		ID3D11PixelShader* PSOld = NULL;
		Direct3DDeviceIMContext->VSGetShader(&VSOld,NULL,NULL);
		Direct3DDeviceIMContext->PSGetShader(&PSOld,NULL,NULL);

		// Set the new shaders
		TShaderMapRef<FOneColorVertexShader> VertexShader(GetGlobalShaderMap());
		TShaderMapRef<FOneColorPixelShader> PixelShader(GetGlobalShaderMap());
		SetGlobalBoundShaderState(GD3D11ClearBoundShaderState, GD3D11Vector4VertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FVector4));
		SetPixelShaderValue(PixelShader->GetPixelShader(),PixelShader->ColorParameter,Color);

		// Draw a fullscreen quad
		FVector4 Vertices[4];
		Vertices[0].Set( -1.0f,  1.0f, Depth, 1.0f );
		Vertices[1].Set(  1.0f,  1.0f, Depth, 1.0f );
		Vertices[2].Set( -1.0f, -1.0f, Depth, 1.0f );
		Vertices[3].Set(  1.0f, -1.0f, Depth, 1.0f );
		RHIDrawPrimitiveUP(PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]) );

		if (bChangedDepthStencilTarget)
		{
			// Restore the DST
			if (bCurrentDSTIsReadonly)
			{
				CurrentDepthStencilTarget = CurrentDepthSurface->ReadOnlyDepthStencilView;
			}	
			else
			{
				CurrentDepthStencilTarget = CurrentDepthSurface->DepthStencilView;
			}

			ID3D11RenderTargetView* RTArray[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
			for(UINT RenderTargetIndex = 0;RenderTargetIndex < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++RenderTargetIndex)
			{
				RTArray[RenderTargetIndex] = CurrentRenderTargets[RenderTargetIndex];
			}
			Direct3DDeviceIMContext->OMSetRenderTargets(
				D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
				RTArray,
				CurrentDepthStencilTarget
				);
		}

		// Restore the old shaders
		Direct3DDeviceIMContext->VSSetShader(VSOld,NULL,0);
		Direct3DDeviceIMContext->PSSetShader(PSOld,NULL,0);
		if(VSOld)
		{
			VSOld->Release();
		}
		if(PSOld)
		{
			PSOld->Release();
		}

		// Restore the old states
		Direct3DDeviceIMContext->OMSetDepthStencilState(OldDepthStencilState,StencilRef);
		Direct3DDeviceIMContext->OMSetBlendState(OldBlendState,BlendFactor,SampleMask);
		Direct3DDeviceIMContext->RSSetState(OldRasterizerState);
		if(OldDepthStencilState)
		{
			OldDepthStencilState->Release();
		}
		if(OldBlendState)
		{
			OldBlendState->Release();
		}
		if(OldRasterizerState)
		{
			OldRasterizerState->Release();
		}

	}
	else
	{
		if (bTrackingEvents && CurrentEventNode)
		{
			CurrentEventNode->NumDraws++;
		}

		if(bClearColor && pRTV)
		{
			Direct3DDeviceIMContext->ClearRenderTargetView(pRTV,(FLOAT*)&Color);
		}
		if((bClearDepth || bClearStencil) && pDSV)
		{
			UINT ClearFlags = 0;
			if(bClearDepth)
			{
				ClearFlags |= D3D11_CLEAR_DEPTH;
			}
			if(bClearStencil)
			{
				ClearFlags |= D3D11_CLEAR_STENCIL;
			}
			Direct3DDeviceIMContext->ClearDepthStencilView(pDSV,ClearFlags,Depth,Stencil);
		}
	}

	if(pRTV)
	{
		pRTV->Release();
	}
	if(OriginalDSV)
	{
		OriginalDSV->Release();
	}
}

// Functions to yield and regain rendering control from D3D

void FD3D11DynamicRHI::SuspendRendering()
{
	// Not supported
}

void FD3D11DynamicRHI::ResumeRendering()
{
	// Not supported
}

UBOOL FD3D11DynamicRHI::IsRenderingSuspended()
{
	// Not supported
	return FALSE;
}

// Kick the rendering commands that are currently queued up in the GPU command buffer.
void FD3D11DynamicRHI::KickCommandBuffer()
{
	// Not really supported
}

// Blocks the CPU until the GPU catches up and goes idle.
void FD3D11DynamicRHI::BlockUntilGPUIdle()
{
	// Not really supported
}

/*
	* Returns the total GPU time taken to render the last frame. Same metric as appCycles().
	*/
DWORD FD3D11DynamicRHI::GetGPUFrameCycles()
{
	return GGPUFrameTime;
}

/*
	* Returns an approximation of the available memory that textures can use, which is video + AGP where applicable, rounded to the nearest MB, in MB.
	*/
DWORD FD3D11DynamicRHI::GetAvailableTextureMemory()
{
	//TODO: Get this through DXGI or WMI channels
	return 512;
}

// not used on PC
void FD3D11DynamicRHI::RestoreColorDepth(FTexture2DRHIParamRef ColorTexture, FTexture2DRHIParamRef DepthTexture)
{
}

// This is NOT related to the D3D11 tessellation pipeline
void FD3D11DynamicRHI::SetTessellationMode( ETessellationMode TessellationMode, FLOAT MinTessellation, FLOAT MaxTessellation )
{
}

void FD3D11DynamicRHI::UpdateStereoFixTexture(FTexture2DRHIParamRef TextureRHI)
{
	if (!nv::stereo::IsStereoEnabled()) 
	{
		return;
	}

	if (!StereoUpdater)
	{
		// $$$$ NEED TO VERIFY STEREO
		StereoUpdater = new nv::stereo::UE3StereoD3D11(false);
	}

	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,Texture);

	// BED - This UpdateStereoTexture uses the IMMEDIATE CONTEXT.  So if DCs are used we need to re-eval this interface.
	// Device Removal is (correctly) unsupported, and device resets no longer cause
	// the loss of textures, so pass false.
	StereoUpdater->UpdateStereoTexture(Direct3DDevice, Texture->Resource, false);
	
}
