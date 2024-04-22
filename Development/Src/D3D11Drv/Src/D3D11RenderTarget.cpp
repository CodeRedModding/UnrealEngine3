/*=============================================================================
	D3D11RenderTarget.cpp: D3D render target implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D11DrvPrivate.h"
#include "BatchedElements.h"
#include "ScreenRendering.h"

namespace
{
	struct FDummyResolveParameter {};

	class FResolveDepthPixelShader : public FShader
	{
		DECLARE_SHADER_TYPE(FResolveDepthPixelShader,Global);
	public:

		typedef FDummyResolveParameter FParameter;

		static UBOOL ShouldCache(EShaderPlatform Platform) { return Platform == SP_PCD3D_SM5; }

		FResolveDepthPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
			FShader(Initializer)
		{
			UnresolvedSurfaceParameter.Bind(Initializer.ParameterMap,TEXT("UnresolvedSurface"));
		}
		FResolveDepthPixelShader() {}

		void SetParameters(FD3D11Surface* SourceSurface,ID3D11DeviceContext* Direct3DDeviceContext,FParameter)
		{
			const UINT TextureIndex = UnresolvedSurfaceParameter.GetBaseIndex();

			ID3D11ShaderResourceView* SRV = SourceSurface->ShaderResourceView;
			Direct3DDeviceContext->PSSetShaderResources(TextureIndex,1,&SRV);

			SourceSurface->BoundShaderResourceSlots[SF_Pixel].AddUniqueItem(TextureIndex);
		}

		virtual UBOOL Serialize(FArchive& Ar)
		{
			UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
			Ar << UnresolvedSurfaceParameter;
			return bShaderHasOutdatedParameters;
		}

	private:
		FShaderResourceParameter UnresolvedSurfaceParameter;
	};
	IMPLEMENT_SHADER_TYPE(,FResolveDepthPixelShader,TEXT("ResolvePixelShader"),TEXT("MainDepth"),SF_Pixel,0,0);

	class FResolveSingleSamplePixelShader : public FShader
	{
		DECLARE_SHADER_TYPE(FResolveSingleSamplePixelShader,Global);
	public:

		typedef UINT FParameter;

		static UBOOL ShouldCache(EShaderPlatform Platform) { return Platform == SP_PCD3D_SM5; }

		FResolveSingleSamplePixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
			FShader(Initializer)
		{
			UnresolvedSurfaceParameter.Bind(Initializer.ParameterMap,TEXT("UnresolvedSurface"));
			SingleSampleIndexParameter.Bind(Initializer.ParameterMap,TEXT("SingleSampleIndex"));
		}
		FResolveSingleSamplePixelShader() {}

		void SetParameters(FD3D11Surface* SourceSurface,ID3D11DeviceContext* Direct3DDeviceContext,UINT SingleSampleIndex)
		{
			const UINT TextureIndex = UnresolvedSurfaceParameter.GetBaseIndex();

			ID3D11ShaderResourceView* SRV = SourceSurface->ShaderResourceView;
			Direct3DDeviceContext->PSSetShaderResources(TextureIndex,1,&SRV);

			SetPixelShaderValue(GetPixelShader(),SingleSampleIndexParameter,SingleSampleIndex);

			SourceSurface->BoundShaderResourceSlots[SF_Pixel].AddUniqueItem(TextureIndex);
		}

		virtual UBOOL Serialize(FArchive& Ar)
		{
			UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
			Ar << UnresolvedSurfaceParameter;
			Ar << SingleSampleIndexParameter;
			return bShaderHasOutdatedParameters;
		}

	private:
		FShaderResourceParameter UnresolvedSurfaceParameter;
		FShaderParameter SingleSampleIndexParameter;
	};
	IMPLEMENT_SHADER_TYPE(,FResolveSingleSamplePixelShader,TEXT("ResolvePixelShader"),TEXT("MainSingleSample"),SF_Pixel,0,0);

	/**
	 * A vertex shader for rendering a textured screen element.
	 */
	class FResolveVertexShader : public FShader
	{
		DECLARE_SHADER_TYPE(FResolveVertexShader,Global);
	public:

		static UBOOL ShouldCache(EShaderPlatform Platform) { return Platform == SP_PCD3D_SM5; }

		FResolveVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
			FShader(Initializer)
		{}
		FResolveVertexShader() {}
	};
	IMPLEMENT_SHADER_TYPE(,FResolveVertexShader,TEXT("ResolveVertexShader"),TEXT("Main"),SF_Vertex,0,0);
}

static FResolveRect GetDefaultRect(const FResolveRect& Rect,UINT DefaultWidth,UINT DefaultHeight)
{
	if (Rect.X1 >= 0 && Rect.X2 >= 0 && Rect.Y1 >= 0 && Rect.Y2 >= 0)
	{
		return Rect;
	}
	else
	{
		return FResolveRect(0,0,DefaultWidth,DefaultHeight);
	}
}

template<typename TPixelShader>
void FD3D11DynamicRHI::ResolveSurfaceUsingShader(
	FD3D11Surface* SourceSurface,
	FD3D11Texture2D* DestTexture2D,
	ID3D11RenderTargetView* DestSurfaceRTV,
	ID3D11DepthStencilView* DestSurfaceDSV,
	const D3D11_TEXTURE2D_DESC& ResolveTargetDesc,
	const FResolveRect& SourceRect,
	const FResolveRect& DestRect,
	ID3D11DeviceContext* Direct3DDeviceContext, 
	typename TPixelShader::FParameter PixelShaderParameter
	)
{
	// Save the current viewport so that it can be restored
	D3D11_VIEWPORT SavedViewport;
	UINT NumSavedViewports = 1;
	Direct3DDeviceContext->RSGetViewports(&NumSavedViewports,&SavedViewport);

	// No alpha blending, no depth tests or writes, no stencil tests or writes, no backface culling.
	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetStencilState(TStaticStencilState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());

	// Make sure this render target is not bound as a texture
	if (DestTexture2D)
	{
		DestTexture2D->UnsetTextureReferences();
	}

	// Determine if the entire destination surface is being resolved to.
	// If the entire surface is being resolved to, then it means we can clear it and signal the driver that it can discard
	// the surface's previous contents, which breaks dependencies between frames when using alternate-frame SLI.
	const UBOOL bClearDestSurface =
			DestRect.X1 == 0
		&&	DestRect.Y1 == 0
		&&	DestRect.X2 == ResolveTargetDesc.Width
		&&	DestRect.Y2 == ResolveTargetDesc.Height;

	if(ResolveTargetDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		// Clear the dest surface.
		if(bClearDestSurface)
		{
			if (bTrackingEvents && CurrentEventNode)
			{
				CurrentEventNode->NumDraws++;
			}
			Direct3DDeviceContext->ClearDepthStencilView(DestSurfaceDSV,D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,0,0);
		}

		RHISetDepthState(TStaticDepthState<TRUE,CF_Always>::GetRHI());

		// Write to the dest surface as a depth-stencil target.
		ID3D11RenderTargetView* NullRTV = NULL;
		Direct3DDeviceContext->OMSetRenderTargets(1,&NullRTV,DestSurfaceDSV);
	}
	else
	{
		// Clear the dest surface.
		if(bClearDestSurface)
		{
			if (bTrackingEvents && CurrentEventNode)
			{
				CurrentEventNode->NumDraws++;
			}

			FLinearColor ClearColor(0,0,0,0);
			Direct3DDeviceContext->ClearRenderTargetView(DestSurfaceRTV,(FLOAT*)&ClearColor);
		}

		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());

		// Write to the dest surface as a render target.
		Direct3DDeviceContext->OMSetRenderTargets(1,&DestSurfaceRTV,NULL);
	}

	D3D11_VIEWPORT TempViewport;
	TempViewport.MinDepth = 0.0f;
	TempViewport.MaxDepth = 1.0f;
	TempViewport.TopLeftX = 0;
	TempViewport.TopLeftY = 0;
	TempViewport.Width = ResolveTargetDesc.Width;
	TempViewport.Height = ResolveTargetDesc.Height;
	Direct3DDeviceContext->RSSetViewports(1,&TempViewport);

	// Generate the vertices used to copy from the source surface to the destination surface.
	const FLOAT MinU = SourceRect.X1;
	const FLOAT MinV = SourceRect.Y1;
	const FLOAT MaxU = SourceRect.X2;
	const FLOAT MaxV = SourceRect.Y2;
	const FLOAT MinX = -1.f + DestRect.X1 / ((FLOAT)ResolveTargetDesc.Width * 0.5f);		
	const FLOAT MinY = +1.f - DestRect.Y1 / ((FLOAT)ResolveTargetDesc.Height * 0.5f);
	const FLOAT MaxX = -1.f + DestRect.X2 / ((FLOAT)ResolveTargetDesc.Width * 0.5f);		
	const FLOAT MaxY = +1.f - DestRect.Y2 / ((FLOAT)ResolveTargetDesc.Height * 0.5f);

	static FGlobalBoundShaderState ResolveBoundShaderState;

	// Set the screen vertex shader.
	TShaderMapRef<FResolveVertexShader> ResolveVertexShader(GetGlobalShaderMap());

	// Set the screen pixel shader.
	TShaderMapRef<TPixelShader> ResolvePixelShader(GetGlobalShaderMap());
	ResolvePixelShader->SetParameters(SourceSurface,Direct3DDeviceContext,PixelShaderParameter);

	SetGlobalBoundShaderState(ResolveBoundShaderState, GScreenVertexDeclaration.VertexDeclarationRHI, *ResolveVertexShader, *ResolvePixelShader, sizeof(FScreenVertex));

	// Generate the vertices used
	FScreenVertex Vertices[4];

	Vertices[0].Position.X = MaxX;
	Vertices[0].Position.Y = MinY;
	Vertices[0].UV.X       = MaxU;
	Vertices[0].UV.Y       = MinV;

	Vertices[1].Position.X = MaxX;
	Vertices[1].Position.Y = MaxY;
	Vertices[1].UV.X       = MaxU;
	Vertices[1].UV.Y       = MaxV;

	Vertices[2].Position.X = MinX;
	Vertices[2].Position.Y = MinY;
	Vertices[2].UV.X       = MinU;
	Vertices[2].UV.Y       = MinV;

	Vertices[3].Position.X = MinX;
	Vertices[3].Position.Y = MaxY;
	Vertices[3].UV.X       = MinU;
	Vertices[3].UV.Y       = MaxV;

	RHIDrawPrimitiveUP(PT_TriangleStrip,2,Vertices,sizeof(Vertices[0]));

	if (SourceSurface)
	{
		SourceSurface->UnsetTextureReferences(this);
	}

	// Reset saved render targets
	ID3D11RenderTargetView* RTArray[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	for(UINT RenderTargetIndex = 0;RenderTargetIndex < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++RenderTargetIndex)
	{
		RTArray[RenderTargetIndex] = CurrentRenderTargets[RenderTargetIndex];
	}
	Direct3DDeviceContext->OMSetRenderTargets(
		D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
		RTArray,
		CurrentDepthStencilTarget
		);

	// Reset saved viewport
	Direct3DDeviceContext->RSSetViewports(NumSavedViewports,&SavedViewport);
}

/** Unsets all texture slots that a shader resource view for this surface has been bound to. */
void FD3D11Surface::UnsetTextureReferences(FD3D11DynamicRHI* D3DRHI)
{
	// Clear references to the resolve target as well if they share the same resource
	if (ResolveTarget2D && ResolveTarget2D->Resource == Resource)
	{
		ResolveTarget2D->UnsetTextureReferences();
	}
	
	ID3D11ShaderResourceView* NullView = NULL;
	for (INT FrequencyIndex = 0; FrequencyIndex < SF_NumFrequencies; FrequencyIndex++)
	{
		for (INT SlotIndex = 0; SlotIndex < BoundShaderResourceSlots[FrequencyIndex].Num(); SlotIndex++)
		{
			INT TextureIndex = BoundShaderResourceSlots[FrequencyIndex](SlotIndex);
			if (FrequencyIndex == SF_Pixel)
			{
				D3DRHI->GetDeviceContext()->PSSetShaderResources(TextureIndex, 1, &NullView);
			}
			else if (FrequencyIndex == SF_Compute)
			{
				D3DRHI->GetDeviceContext()->CSSetShaderResources(TextureIndex, 1, &NullView);
			}
			else if (FrequencyIndex == SF_Geometry)
			{
				D3DRHI->GetDeviceContext()->GSSetShaderResources(TextureIndex, 1, &NullView);
			}
			else if (FrequencyIndex == SF_Domain)
			{
				D3DRHI->GetDeviceContext()->DSSetShaderResources(TextureIndex, 1, &NullView);
			}
			else if (FrequencyIndex == SF_Hull)
			{
				D3DRHI->GetDeviceContext()->HSSetShaderResources(TextureIndex, 1, &NullView);
			}
			else if (FrequencyIndex == SF_Vertex)
			{
				D3DRHI->GetDeviceContext()->VSSetShaderResources(TextureIndex, 1, &NullView);
			}
			else
			{
				check(0);
			}
		}
		BoundShaderResourceSlots[FrequencyIndex].Reset();
	}
}

/**
* Copies the contents of the given surface to its resolve target texture.
* @param SourceSurface - surface with a resolve texture to copy to
* @param bKeepOriginalSurface - TRUE if the original surface will still be used after this function so must remain valid
* @param ResolveParams - optional resolve params
*/
void FD3D11DynamicRHI::CopyToResolveTarget(FSurfaceRHIParamRef SourceSurfaceRHI, UBOOL bKeepOriginalSurface, const FResolveParams& ResolveParams)
{
	DYNAMIC_CAST_D3D11RESOURCE(Surface,SourceSurface);

	FD3D11Texture2D* ResolveTarget2D = ResolveParams.ResolveTarget ? (FD3D11Texture2D*)ResolveParams.ResolveTarget : SourceSurface->ResolveTarget2D;
	FD3D11TextureCube* ResolveTargetCube = SourceSurface->ResolveTargetCube;

	check(ResolveTarget2D || ResolveTargetCube);

	ID3D11Texture2D* ResolveTargetResource = ResolveTarget2D ? ResolveTarget2D->Resource : ResolveTargetCube->Resource;
	if(	SourceSurface->Resource &&
		SourceSurface->Resource != ResolveTargetResource)
	{
		if (bTrackingEvents && CurrentEventNode)
		{
			CurrentEventNode->NumDraws++;
		}

		if(ResolveTargetResource)
		{
			D3D11_TEXTURE2D_DESC ResolveTargetDesc;
			ResolveTargetResource->GetDesc(&ResolveTargetDesc);

			if(	FeatureLevel == D3D_FEATURE_LEVEL_11_0 
			&& (ResolveTargetDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
			&& GSystemSettings.UsesMSAA())
			{
				ResolveSurfaceUsingShader<FResolveDepthPixelShader>(
					SourceSurface,
					ResolveTarget2D,
					ResolveTarget2D->RenderTargetView,
					ResolveTarget2D->DepthStencilView,
					ResolveTargetDesc,
					GetDefaultRect(ResolveParams.Rect,ResolveTargetDesc.Width,ResolveTargetDesc.Height),
					GetDefaultRect(ResolveParams.Rect,ResolveTargetDesc.Width,ResolveTargetDesc.Height),
					Direct3DDeviceIMContext,
					FDummyResolveParameter()
					);
			}
			else
			{
				// Determine whether the resolve target is a cube-map face.
				UINT Subresource = 0;
				if(ResolveTargetDesc.MiscFlags == D3D11_RESOURCE_MISC_TEXTURECUBE)
				{
					// Resolving a cube texture
					UINT D3DFace = GetD3D11CubeFace(ResolveParams.CubeFace);
					Subresource = D3D11CalcSubresource(0,D3DFace,1);
				}

				// Determine whether a MSAA resolve is needed, or just a copy.
				D3D11_TEXTURE2D_DESC SourceSurfaceDesc;
				SourceSurface->Resource->GetDesc(&SourceSurfaceDesc);
				if(SourceSurfaceDesc.SampleDesc.Count != 1)
				{
					Direct3DDeviceIMContext->ResolveSubresource(ResolveTargetResource,Subresource,SourceSurface->Resource,Subresource,ResolveTargetDesc.Format);
				}
				else
				{
					Direct3DDeviceIMContext->CopySubresourceRegion(ResolveTargetResource,Subresource,0,0,0,SourceSurface->Resource,Subresource,NULL);
				}
			}
		}
	}
}

void FD3D11DynamicRHI::CopyFromResolveTargetFast(FSurfaceRHIParamRef DestSurface)
{
	// these need to be referenced in order for the FScreenVertexShader/FScreenPixelShader types to not be compiled out on PC
	TShaderMapRef<FScreenVertexShader> ScreenVertexShader(GetGlobalShaderMap());
	TShaderMapRef<FScreenPixelShader> ScreenPixelShader(GetGlobalShaderMap());
}

void FD3D11DynamicRHI::CopyFromResolveTargetRectFast(FSurfaceRHIParamRef DestSurface,FLOAT X1,FLOAT Y1,FLOAT X2,FLOAT Y2)
{
	// these need to be referenced in order for the FScreenVertexShader/FScreenPixelShader types to not be compiled out on PC
	TShaderMapRef<FScreenVertexShader> ScreenVertexShader(GetGlobalShaderMap());
	TShaderMapRef<FScreenPixelShader> ScreenPixelShader(GetGlobalShaderMap());
}

void FD3D11DynamicRHI::CopyFromResolveTarget(FSurfaceRHIParamRef DestSurface)
{
	// these need to be referenced in order for the FScreenVertexShader/FScreenPixelShader types to not be compiled out on PC
	TShaderMapRef<FScreenVertexShader> ScreenVertexShader(GetGlobalShaderMap());
	TShaderMapRef<FScreenPixelShader> ScreenPixelShader(GetGlobalShaderMap());
}

/**
 *	Returns the resolve target of a surface.
 *	@param SurfaceRHI	- Surface from which to get the resolve target
 *	@return				- Resolve target texture associated with the surface
 */
FTexture2DRHIRef FD3D11DynamicRHI::GetResolveTarget( FSurfaceRHIParamRef SurfaceRHI )
{
	DYNAMIC_CAST_D3D11RESOURCE(Surface,Surface);
	return (FTexture2DRHIParamRef)Surface->ResolveTarget2D;
}

void FD3D11DynamicRHI::DiscardRenderBuffer( DWORD RenderBufferTypes )
{
	// Not supported on this platform at this time.
}

/**
* Creates a RHI surface that can be bound as a render target.
* Note that a surface cannot be created which is both resolvable AND readable.
* @param SizeX - The width of the surface to create.
* @param SizeY - The height of the surface to create.
* @param Format - The surface format to create.
* @param ResolveTargetTexture - The 2d texture which the surface will be resolved to.  It must have been allocated with bResolveTargetable=TRUE
* @param Flags - Surface creation flags
* @param UsageStr - Text describing usage for this surface
* @return The surface that was created.
*/
FSurfaceRHIRef FD3D11DynamicRHI::CreateTargetableSurface(
	UINT SizeX,
	UINT SizeY,
	BYTE Format,
	FTexture2DRHIParamRef ResolveTargetTextureRHI,
	DWORD Flags,
	const TCHAR* UsageStr
	)
{
	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,ResolveTargetTexture);

	const UBOOL bDepthFormat = (Format == PF_DepthStencil || Format == PF_ShadowDepth|| Format == PF_FilteredShadowDepth || Format == PF_D24);
	const DXGI_FORMAT PlatformFormat = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;

	D3D11_DSV_DIMENSION DepthStencilViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	D3D11_RTV_DIMENSION RenderTargetViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	D3D11_SRV_DIMENSION ShaderResourceViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;

	UINT ActualMSAACount = 1;
	UINT ActualMSAAQuality = 0;
	// Determine whether to use MSAA for this surface.
	UpdateMSAASettings();
	if(MSAACount > 1 && (Flags&TargetSurfCreate_Multisample))
	{
		DepthStencilViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		RenderTargetViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
		ShaderResourceViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;

		ActualMSAACount = MSAACount;
		ActualMSAAQuality = MSAAQuality;

		// MSAA surfaces can't be shared with a texture.
		Flags |= TargetSurfCreate_Dedicated;
	}

	// Determine the surface's resource.
	TRefCountPtr<ID3D11Texture2D> SurfaceResource;
	const UBOOL bSurfaceResourceIsSRGB = FALSE;
	if(ResolveTargetTexture && !(Flags & TargetSurfCreate_Dedicated))
	{
		// For non-dedicated views, use the resolve target resource directly.
		SurfaceResource = ResolveTargetTexture->Resource;
	}
	else
	{
		// For dedicated views, or views without a resolve target, create a new resource for the surface.
		D3D11_TEXTURE2D_DESC Desc;
		Desc.Width = SizeX;
		Desc.Height = SizeY;
		Desc.MipLevels = 1;
		Desc.ArraySize = 1;
		Desc.Format = PlatformFormat;
		Desc.SampleDesc.Count = ActualMSAACount;
		Desc.SampleDesc.Quality = ActualMSAAQuality;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = bDepthFormat ? D3D11_BIND_DEPTH_STENCIL : (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

		if(Flags & TargetSurfCreate_UAV)
		{
			Desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}

		if (PlatformFormat == DXGI_FORMAT_R24G8_TYPELESS)
		{
			// A typeless resource format implies that the resource will be bound as a shader resource
			Desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		}
		Desc.CPUAccessFlags = 0;
		Desc.MiscFlags = 0;
		VERIFYD3D11RESULT(Direct3DDevice->CreateTexture2D(&Desc,NULL,SurfaceResource.GetInitReference()));
	}

	// Create either a render target view or depth stencil view for the surface.
	TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
	TRefCountPtr<ID3D11DepthStencilView> DepthStencilView;
	TRefCountPtr<ID3D11DepthStencilView> ReadOnlyDepthStencilView;
	TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;
	TRefCountPtr<ID3D11UnorderedAccessView> UnorderedAccessView;

	if(bDepthFormat)
	{
		DXGI_FORMAT DepthViewFormat = PlatformFormat;
		if (PlatformFormat == DXGI_FORMAT_R24G8_TYPELESS)
		{
			// Use the typed depth stencil view format corresponding to DXGI_FORMAT_R24G8_TYPELESS
			DepthViewFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		}
			
		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc;
		appMemset(&DSVDesc,0,sizeof(DSVDesc));
		DSVDesc.Format = DepthViewFormat;
		DSVDesc.ViewDimension = DepthStencilViewDimension;
		DSVDesc.Texture2D.MipSlice = 0;
		VERIFYD3D11RESULT(Direct3DDevice->CreateDepthStencilView(SurfaceResource,&DSVDesc,DepthStencilView.GetInitReference()));

		// Create a read only depth stencil view for the depth buffer
		// This will be used in passes that don't need to write to depth, but want to bind a shader resource view of the depth buffer
		D3D11_DEPTH_STENCIL_VIEW_DESC ReadOnlyDSVDesc;
		appMemset(&ReadOnlyDSVDesc,0,sizeof(ReadOnlyDSVDesc));
		ReadOnlyDSVDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH;
		ReadOnlyDSVDesc.Format = DepthViewFormat;
		ReadOnlyDSVDesc.ViewDimension = DepthStencilViewDimension;
		ReadOnlyDSVDesc.Texture2D.MipSlice = 0;
		VERIFYD3D11RESULT(Direct3DDevice->CreateDepthStencilView(SurfaceResource,&ReadOnlyDSVDesc,ReadOnlyDepthStencilView.GetInitReference()));

		// Create a shader resource view for the depth buffer.
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		appMemzero(&SRVDesc,sizeof(SRVDesc));
		SRVDesc.Format = FindDXGIFormat(PlatformFormat,bSurfaceResourceIsSRGB);
		if (PlatformFormat == DXGI_FORMAT_R24G8_TYPELESS)
		{
			// Use the typed shader resource view format corresponding to DXGI_FORMAT_R24G8_TYPELESS
			SRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		}
		SRVDesc.ViewDimension = ShaderResourceViewDimension;
		SRVDesc.Texture2D.MipLevels = 1;
		VERIFYD3D11RESULT(Direct3DDevice->CreateShaderResourceView(SurfaceResource,&SRVDesc,ShaderResourceView.GetInitReference()));
	}
	else
	{
		D3D11_TEXTURE2D_DESC ResolveTargetDesc;
		SurfaceResource->GetDesc(&ResolveTargetDesc);

		D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
		appMemset(&RTVDesc,0,sizeof(RTVDesc));
		RTVDesc.Format = FindDXGIFormat(ResolveTargetDesc.Format,bSurfaceResourceIsSRGB);
		RTVDesc.ViewDimension = RenderTargetViewDimension;
		RTVDesc.Texture2D.MipSlice = 0;
		VERIFYD3D11RESULT(Direct3DDevice->CreateRenderTargetView(SurfaceResource,&RTVDesc,RenderTargetView.GetInitReference()));

		// Create a shader resource view for the render target.
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		appMemzero(&SRVDesc,sizeof(SRVDesc));
		SRVDesc.Format = FindDXGIFormat(ResolveTargetDesc.Format,bSurfaceResourceIsSRGB);
		SRVDesc.ViewDimension = ShaderResourceViewDimension;
		// GenerateMips will only generate for mip levels in the view, so we have to specify all of them
		SRVDesc.Texture2D.MipLevels = (Flags & TargetSurfCreate_GenerateMipCapable) ? -1 : 1;
		VERIFYD3D11RESULT(Direct3DDevice->CreateShaderResourceView(SurfaceResource,&SRVDesc,ShaderResourceView.GetInitReference()));
	}

	if(Flags & TargetSurfCreate_UAV)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
		appMemzero(&UAVDesc,sizeof(UAVDesc));
		UAVDesc.Format = DXGI_FORMAT_UNKNOWN;		// ResolveTargetDesc.Format;
		UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		// we currently only expose mip slice 0
		UAVDesc.Texture2D.MipSlice = 0;
		VERIFYD3D11RESULT(Direct3DDevice->CreateUnorderedAccessView(SurfaceResource,&UAVDesc,UnorderedAccessView.GetInitReference()));
	}

	return new FD3D11Surface(RenderTargetView,DepthStencilView,ReadOnlyDepthStencilView,ShaderResourceView,UnorderedAccessView,ResolveTargetTexture,NULL,SurfaceResource);
}

/**
* Creates a RHI surface that can be bound as a render target and can resolve w/ a cube texture
* Note that a surface cannot be created which is both resolvable AND readable.
* @param SizeX - The width of the surface to create.
* @param Format - The surface format to create.
* @param ResolveTargetTexture - The cube texture which the surface will be resolved to.  It must have been allocated with bResolveTargetable=TRUE
* @param CubeFace - face from resolve texture to use as surface
* @param Flags - Surface creation flags
* @param UsageStr - Text describing usage for this surface
* @return The surface that was created.
*/
FSurfaceRHIRef FD3D11DynamicRHI::CreateTargetableCubeSurface(
	UINT SizeX,
	BYTE Format,
	FTextureCubeRHIParamRef ResolveTargetTextureRHI,
	ECubeFace CubeFace,
	DWORD Flags,
	const TCHAR* UsageStr
	)
{
	DYNAMIC_CAST_D3D11RESOURCE(TextureCube,ResolveTargetTexture);

	const UBOOL bDepthFormat = (Format == PF_DepthStencil || Format == PF_ShadowDepth|| Format == PF_FilteredShadowDepth || Format == PF_D24);
	// Determine if we are creating a surface for a single face, or for the whole cube map
	const UBOOL bCreateSingleFaceSurface = CubeFace < CubeFace_MAX;

	if (!ResolveTargetTexture)
	{
		checkMsg(FALSE,TEXT("No resolve target cube texture specified.  Just use RHICreateTargetableSurface instead."));
		return NULL;
	}
	else
	{
		const DXGI_FORMAT PlatformFormat = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;
		const UINT D3DFace = GetD3D11CubeFace(CubeFace);

		// Determine the surface's resource.
		TRefCountPtr<ID3D11Texture2D> SurfaceResource;
		D3D11_RTV_DIMENSION DimensionRTV;
		D3D11_DSV_DIMENSION DimensionDSV;
		if(ResolveTargetTexture && !(Flags & TargetSurfCreate_Dedicated))
		{
			// For non-dedicated views, use the resolve target resource directly.
			SurfaceResource = ResolveTargetTexture->Resource;
			DimensionRTV = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			DimensionDSV = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		}
		else
		{
			// For dedicated views, or views without a resolve target, create a new resource for the surface.
			D3D11_TEXTURE2D_DESC Desc;
			appMemzero(&Desc, sizeof(Desc));
			Desc.Width = SizeX;
			Desc.Height = SizeX;
			Desc.MipLevels = 1;
			Desc.ArraySize = bCreateSingleFaceSurface ? 1 : 6;
			Desc.Format = PlatformFormat;
			Desc.SampleDesc.Count = 1;
			Desc.SampleDesc.Quality = 0;
			Desc.Usage = D3D11_USAGE_DEFAULT;
			Desc.BindFlags = bDepthFormat ? D3D11_BIND_DEPTH_STENCIL : (D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);

			if (PlatformFormat == DXGI_FORMAT_R24G8_TYPELESS)
			{
				// A typeless resource format implies that the resource will be bound as a shader resource
				Desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
			}

			Desc.CPUAccessFlags = 0;
			Desc.MiscFlags = bCreateSingleFaceSurface ? 0 : D3D11_RESOURCE_MISC_TEXTURECUBE;
			VERIFYD3D11RESULT(Direct3DDevice->CreateTexture2D(&Desc,NULL,SurfaceResource.GetInitReference()));
			if (bCreateSingleFaceSurface)
			{
				DimensionRTV = D3D11_RTV_DIMENSION_TEXTURE2D;
				DimensionDSV = D3D11_DSV_DIMENSION_TEXTURE2D;
			}
			else
			{
				DimensionRTV = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
				DimensionDSV = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
			}
		}

		// Create either a render target view or depth stencil view for the surface.
		TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
		TRefCountPtr<ID3D11DepthStencilView> DepthStencilView;
		TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;

		if (bDepthFormat)
		{
			DXGI_FORMAT DepthViewFormat = PlatformFormat;
			if (PlatformFormat == DXGI_FORMAT_R24G8_TYPELESS)
			{
				// Use the typed depth stencil view format corresponding to DXGI_FORMAT_R24G8_TYPELESS
				DepthViewFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
			}

			D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc;
			appMemzero(&DSVDesc, sizeof(DSVDesc));
			DSVDesc.Format = DepthViewFormat;
			DSVDesc.ViewDimension = DimensionDSV;

			if (DSVDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D)
			{
				DSVDesc.Texture2D.MipSlice = 0;
			}
			else if (bCreateSingleFaceSurface && DSVDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY)
			{
				DSVDesc.Texture2DArray.ArraySize = 1;
				DSVDesc.Texture2DArray.FirstArraySlice = D3DFace;
				DSVDesc.Texture2DArray.MipSlice = 0;
			}
			else if (!bCreateSingleFaceSurface && DSVDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY)
			{
				DSVDesc.Texture2DArray.ArraySize = 6;
				DSVDesc.Texture2DArray.FirstArraySlice = 0;
				DSVDesc.Texture2DArray.MipSlice = 0;
			}
			VERIFYD3D11RESULT(Direct3DDevice->CreateDepthStencilView(SurfaceResource,&DSVDesc,DepthStencilView.GetInitReference()));

			// Create a shader resource view for the render target.
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			appMemzero(&SRVDesc,sizeof(SRVDesc));
			SRVDesc.Format = PlatformFormat;

			if (PlatformFormat == DXGI_FORMAT_R24G8_TYPELESS)
			{
				// Use the typed shader resource view format corresponding to DXGI_FORMAT_R24G8_TYPELESS
				SRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			}

			if (bCreateSingleFaceSurface)
			{
				SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				SRVDesc.Texture2D.MostDetailedMip = 0;
				SRVDesc.Texture2D.MipLevels = 1;
			}
			else
			{
				SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
				SRVDesc.TextureCube.MostDetailedMip = 0;
				SRVDesc.TextureCube.MipLevels = 1;
			}
			
			VERIFYD3D11RESULT(Direct3DDevice->CreateShaderResourceView(SurfaceResource,&SRVDesc,ShaderResourceView.GetInitReference()));
		}
		else
		{
			D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
			appMemzero(&RTVDesc, sizeof(RTVDesc));
			RTVDesc.Format = PlatformFormat;
			RTVDesc.ViewDimension = DimensionRTV;
			RTVDesc.Texture2D.MipSlice = 0;
			RTVDesc.Texture2DArray.ArraySize = 1;
			RTVDesc.Texture2DArray.FirstArraySlice = D3DFace;
			RTVDesc.Texture2DArray.MipSlice = 0;
			VERIFYD3D11RESULT(Direct3DDevice->CreateRenderTargetView(SurfaceResource,&RTVDesc,RenderTargetView.GetInitReference()));

			// Create a shader resource view for the render target.
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			appMemzero(&SRVDesc,sizeof(SRVDesc));
			SRVDesc.Format = PlatformFormat;
			SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			SRVDesc.TextureCube.MipLevels = 1;
			VERIFYD3D11RESULT(Direct3DDevice->CreateShaderResourceView(SurfaceResource,&SRVDesc,ShaderResourceView.GetInitReference()));
		}

		return new FD3D11Surface(RenderTargetView,DepthStencilView,DepthStencilView,ShaderResourceView,NULL,NULL,ResolveTargetTexture,SurfaceResource);
	}
}

/**
* Helper for storing IEEE 32 bit float components
*/
struct FFloatIEEE
{
	union
	{
		struct
		{
			DWORD	Mantissa : 23, Exponent : 8, Sign : 1;
		} Components;

		FLOAT	Float;
	};
};

/**
* Helper for storing 16 bit float components
*/
struct FD3DFloat16
{
	union
	{
		struct
		{
			WORD	Mantissa : 10, Exponent : 5, Sign : 1;
		} Components;

		WORD	Encoded;
	};

	/**
	* @return full 32 bit float from the 16 bit value
	*/
	operator FLOAT()
	{
		FFloatIEEE	Result;

		Result.Components.Sign = Components.Sign;
		Result.Components.Exponent = Components.Exponent - 15 + 127; // Stored exponents are biased by half their range.
		Result.Components.Mantissa = Min<DWORD>(appFloor((FLOAT)Components.Mantissa / 1024.0f * 8388608.0f),(1 << 23) - 1);

		return Result.Float;
	}
};

void FD3D11DynamicRHI::ReadSurfaceData(FSurfaceRHIParamRef SurfaceRHI,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<BYTE>& OutData, FReadSurfaceDataFlags InFlags)
{
	DYNAMIC_CAST_D3D11RESOURCE(Surface,Surface);

	UINT SizeX = MaxX - MinX + 1;
	UINT SizeY = MaxY - MinY + 1;
	
	// Select which resource to copy from depending on whether there is a resolve target
	ID3D11Texture2D* SourceSurface = Surface->Resource;
	if( Surface->ResolveTarget2D )
	{
		SourceSurface = Surface->ResolveTarget2D->Resource;
	}
	check(SourceSurface);

	// Check the format of the surface
	D3D11_TEXTURE2D_DESC SurfaceDesc;
	SourceSurface->GetDesc(&SurfaceDesc);

	// Handle both SRGB and non-SRGB surface formats
	check( SurfaceDesc.Format == GPixelFormats[PF_A8R8G8B8].PlatformFormat
		|| SurfaceDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT
		|| SurfaceDesc.Format == FindDXGIFormat((DXGI_FORMAT)GPixelFormats[PF_A8R8G8B8].PlatformFormat, TRUE )
		|| SurfaceDesc.Format == FindTypelessDXGIFormat((DXGI_FORMAT)GPixelFormats[PF_A8R8G8B8].PlatformFormat )
		|| SurfaceDesc.Format == DXGI_FORMAT_R24G8_TYPELESS);

	// Allocate the output buffer.
	OutData.Empty((MaxX - MinX + 1) * (MaxY - MinY + 1) * sizeof(FColor));

	// Read back the surface data from (MinX,MinY) to (MaxX,MaxY)
	D3D11_BOX	Rect;
	Rect.left	= MinX;
	Rect.top	= MinY;
	Rect.right	= MaxX + 1;
	Rect.bottom	= MaxY + 1;
	Rect.back = 1;
	Rect.front = 0;

	// create a temp 2d texture to copy render target to
	TRefCountPtr<ID3D11Texture2D> Texture2D;
	D3D11_TEXTURE2D_DESC Desc;
	Desc.Width = SizeX;
	Desc.Height = SizeY;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = SurfaceDesc.Format;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.Usage = D3D11_USAGE_STAGING;
	Desc.BindFlags = 0;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	Desc.MiscFlags = 0;
	VERIFYD3D11RESULT(Direct3DDevice->CreateTexture2D(&Desc,NULL,Texture2D.GetInitReference()));

	// Copy the data to a staging resource.
	UINT Subresource = 0;
	if( SurfaceDesc.MiscFlags == D3D11_RESOURCE_MISC_TEXTURECUBE )
	{
		UINT D3DFace = GetD3D11CubeFace(InFlags.GetCubeFace());
		Subresource = D3D11CalcSubresource(0,D3DFace,1);
	}

	D3D11_BOX* RectPtr = &Rect;

	// full texture is copied, do not provide pSrcBox (API likes it this way)
	if(Rect.left == 0 && Rect.top == 0 && Rect.right == SurfaceDesc.Width && Rect.bottom == SurfaceDesc.Height)
	{
		RectPtr = 0;
	}

	Direct3DDeviceIMContext->CopySubresourceRegion(Texture2D,0,0,0,0,SourceSurface,Subresource,RectPtr);

	// Lock the staging resource.
	D3D11_MAPPED_SUBRESOURCE LockedRect;
	VERIFYD3D11RESULT(Direct3DDeviceIMContext->Map(Texture2D,0,D3D11_MAP_READ,0,&LockedRect));

	if( SurfaceDesc.Format == GPixelFormats[PF_A8R8G8B8].PlatformFormat ||
		SurfaceDesc.Format == FindDXGIFormat((DXGI_FORMAT)GPixelFormats[PF_A8R8G8B8].PlatformFormat, TRUE ) ||
		SurfaceDesc.Format == FindTypelessDXGIFormat((DXGI_FORMAT)GPixelFormats[PF_A8R8G8B8].PlatformFormat ) )
	{
		// Read the data out of the buffer, converting it from ABGR to ARGB.
		for(UINT Y = MinY;Y <= MaxY;Y++)
		{
			FColor* SrcPtr = (FColor*)((BYTE*)LockedRect.pData + (Y - MinY) * LockedRect.RowPitch);
			FColor* DestPtr = (FColor*)((BYTE*)&OutData(OutData.Add(SizeX * sizeof(FColor))));
			for(UINT X = MinX;X <= MaxX;X++)
			{
				*DestPtr = FColor(SrcPtr->B,SrcPtr->G,SrcPtr->R,SrcPtr->A);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if ( SurfaceDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
	{
		FPlane	MinValue(0.0f,0.0f,0.0f,0.0f),
			MaxValue(1.0f,1.0f,1.0f,1.0f);

		check(sizeof(FD3DFloat16)==sizeof(WORD));

		for( UINT Y = MinY; Y <= MaxY; Y++ )
		{
			FD3DFloat16* SrcPtr = (FD3DFloat16*)((BYTE*)LockedRect.pData + (Y - MinY) * LockedRect.RowPitch);

			for( UINT X = MinX; X <= MaxX; X++ )
			{
				MinValue.X = Min<FLOAT>(SrcPtr[0],MinValue.X);
				MinValue.Y = Min<FLOAT>(SrcPtr[1],MinValue.Y);
				MinValue.Z = Min<FLOAT>(SrcPtr[2],MinValue.Z);
				MinValue.W = Min<FLOAT>(SrcPtr[3],MinValue.W);
				MaxValue.X = Max<FLOAT>(SrcPtr[0],MaxValue.X);
				MaxValue.Y = Max<FLOAT>(SrcPtr[1],MaxValue.Y);
				MaxValue.Z = Max<FLOAT>(SrcPtr[2],MaxValue.Z);
				MaxValue.W = Max<FLOAT>(SrcPtr[3],MaxValue.W);
				SrcPtr += 4;
			}
		}

		for( UINT Y = MinY; Y <= MaxY; Y++ )
		{
			FD3DFloat16* SrcPtr = (FD3DFloat16*)((BYTE*)LockedRect.pData + (Y - MinY) * LockedRect.RowPitch);
			FColor* DestPtr = (FColor*)(BYTE*)&OutData(OutData.Add(SizeX * sizeof(FColor)));

			for( UINT X = MinX; X <= MaxX; X++ )
			{
				FColor NormalizedColor =
					FLinearColor(
					(SrcPtr[0] - MinValue.X) / (MaxValue.X - MinValue.X),
					(SrcPtr[1] - MinValue.Y) / (MaxValue.Y - MinValue.Y),
					(SrcPtr[2] - MinValue.Z) / (MaxValue.Z - MinValue.Z),
					(SrcPtr[3] - MinValue.W) / (MaxValue.W - MinValue.W)
					).ToFColor(TRUE);
				appMemcpy(DestPtr++,&NormalizedColor,sizeof(FColor));
				SrcPtr += 4;
			}
		}
	}
	else if ( SurfaceDesc.Format == DXGI_FORMAT_R24G8_TYPELESS)
	{
		// Depth stencil
		for( UINT Y = MinY; Y <= MaxY; Y++ )
		{
			UINT* SrcPtr = (UINT *)((BYTE*)LockedRect.pData + (Y - MinY) * LockedRect.RowPitch);
			FColor* DestPtr = (FColor*)(BYTE*)&OutData(OutData.Add(SizeX * sizeof(FColor)));

			for( UINT X = MinX; X <= MaxX; X++ )
			{
				FLOAT DeviceZ = (*SrcPtr & 0xffffff) / (FLOAT)(1<<24);

				FLOAT LinearValue = Min(InFlags.ComputeNormalizedDepth(DeviceZ), 1.0f);

				FColor NormalizedColor = FLinearColor(LinearValue, LinearValue, LinearValue, 0).ToFColor(TRUE);
				appMemcpy(DestPtr++, &NormalizedColor, sizeof(FColor));
				++SrcPtr;
			}
		}
	}
	else
	{
		// not supported yet
	}

	Direct3DDeviceIMContext->Unmap(Texture2D,0);
}

void FD3D11DynamicRHI::ReadSurfaceDataMSAA(FSurfaceRHIParamRef SurfaceRHI,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	DYNAMIC_CAST_D3D11RESOURCE(Surface,Surface);

	UINT SizeX = MaxX - MinX + 1;
	UINT SizeY = MaxY - MinY + 1;
	
	// Select which resource to copy from depending on whether there is a resolve target
	ID3D11Texture2D* SourceSurface = Surface->Resource;
	check(SourceSurface);

	// Check the format of the surface
	D3D11_TEXTURE2D_DESC SurfaceDesc;
	SourceSurface->GetDesc(&SurfaceDesc);

	// Handle both SRGB and non-SRGB surface formats
	check( SurfaceDesc.Format == GPixelFormats[PF_A8R8G8B8].PlatformFormat
		|| SurfaceDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT
		|| SurfaceDesc.Format == FindDXGIFormat((DXGI_FORMAT)GPixelFormats[PF_A8R8G8B8].PlatformFormat, TRUE )
		|| SurfaceDesc.Format == FindTypelessDXGIFormat((DXGI_FORMAT)GPixelFormats[PF_A8R8G8B8].PlatformFormat ) );
	
	const UINT NumSamples = GSystemSettings.MaxMultiSamples;
	check(SurfaceDesc.SampleDesc.Count == NumSamples);

	// Allocate the output buffer.
	OutData.Init((MaxX - MinX + 1) * (MaxY - MinY + 1) * NumSamples);

	// Read back the surface data from (MinX,MinY) to (MaxX,MaxY)
	D3D11_BOX	Rect;
	Rect.left	= MinX;
	Rect.top	= MinY;
	Rect.right	= MaxX + 1;
	Rect.bottom	= MaxY + 1;
	Rect.back = 1;
	Rect.front = 0;

	// Create a non-MSAA render target to resolve individual samples of the source surface to.
	TRefCountPtr<ID3D11Texture2D> NonMSAATexture2D;
	D3D11_TEXTURE2D_DESC NonMSAADesc;
	NonMSAADesc.Width = SizeX;
	NonMSAADesc.Height = SizeY;
	NonMSAADesc.MipLevels = 1;
	NonMSAADesc.ArraySize = 1;
	NonMSAADesc.Format = SurfaceDesc.Format;
	NonMSAADesc.SampleDesc.Count = 1;
	NonMSAADesc.SampleDesc.Quality = 0;
	NonMSAADesc.Usage = D3D11_USAGE_DEFAULT;
	NonMSAADesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	NonMSAADesc.CPUAccessFlags = 0;
	NonMSAADesc.MiscFlags = 0;
	VERIFYD3D11RESULT(Direct3DDevice->CreateTexture2D(&NonMSAADesc,NULL,NonMSAATexture2D.GetInitReference()));

	TRefCountPtr<ID3D11RenderTargetView> NonMSAARTV;
	D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
	appMemset(&RTVDesc,0,sizeof(RTVDesc));
	RTVDesc.Format = NonMSAADesc.Format;
	RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Texture2D.MipSlice = 0;
	VERIFYD3D11RESULT(Direct3DDevice->CreateRenderTargetView(NonMSAATexture2D,&RTVDesc,NonMSAARTV.GetInitReference()));

	// Create a CPU-accessible staging texture to copy the resolved sample data to.
	TRefCountPtr<ID3D11Texture2D> StagingTexture2D;
	D3D11_TEXTURE2D_DESC StagingDesc;
	StagingDesc.Width = SizeX;
	StagingDesc.Height = SizeY;
	StagingDesc.MipLevels = 1;
	StagingDesc.ArraySize = 1;
	StagingDesc.Format = SurfaceDesc.Format;
	StagingDesc.SampleDesc.Count = 1;
	StagingDesc.SampleDesc.Quality = 0;
	StagingDesc.Usage = D3D11_USAGE_STAGING;
	StagingDesc.BindFlags = 0;
	StagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	StagingDesc.MiscFlags = 0;
	VERIFYD3D11RESULT(Direct3DDevice->CreateTexture2D(&StagingDesc,NULL,StagingTexture2D.GetInitReference()));

	// Determine the subresource index for cubemaps.
	UINT Subresource = 0;
	if( SurfaceDesc.MiscFlags == D3D11_RESOURCE_MISC_TEXTURECUBE )
	{
		UINT D3DFace = GetD3D11CubeFace(InFlags.GetCubeFace());
		Subresource = D3D11CalcSubresource(0,D3DFace,1);
	}

	if( SurfaceDesc.Format == GPixelFormats[PF_A8R8G8B8].PlatformFormat ||
		SurfaceDesc.Format == FindDXGIFormat((DXGI_FORMAT)GPixelFormats[PF_A8R8G8B8].PlatformFormat, TRUE ) ||
		SurfaceDesc.Format == FindTypelessDXGIFormat((DXGI_FORMAT)GPixelFormats[PF_A8R8G8B8].PlatformFormat ) )
	{
		for(UINT SampleIndex = 0;SampleIndex < NumSamples;++SampleIndex)
		{
			// Resolve the sample to the non-MSAA render target.
			ResolveSurfaceUsingShader<FResolveSingleSamplePixelShader>(
				Surface,
				NULL,
				NonMSAARTV,
				NULL,
				NonMSAADesc,
				FResolveRect(MinX,MinY,MaxX + 1,MaxY + 1),
				FResolveRect(0,0,SizeX,SizeY),
				Direct3DDeviceIMContext,
				SampleIndex
				);

			// Copy the resolved sample data to the staging texture.
			Direct3DDeviceIMContext->CopySubresourceRegion(StagingTexture2D,0,0,0,0,NonMSAATexture2D,Subresource,&Rect);

			// Lock the staging texture.
			D3D11_MAPPED_SUBRESOURCE LockedRect;
			VERIFYD3D11RESULT(Direct3DDeviceIMContext->Map(StagingTexture2D,0,D3D11_MAP_READ,0,&LockedRect));

			// Read the data out of the buffer, converting it from ABGR to ARGB.
			for(UINT Y = MinY;Y <= MaxY;Y++)
			{
				FColor* SrcPtr = (FColor*)((BYTE*)LockedRect.pData + (Y - MinY) * LockedRect.RowPitch);
				FColor* DestPtr = &OutData((Y - MinY) * SizeX * NumSamples + SampleIndex);
				for(UINT X = MinX;X <= MaxX;X++)
				{
					*DestPtr = FColor(SrcPtr->B,SrcPtr->G,SrcPtr->R,SrcPtr->A);
					++SrcPtr;
					DestPtr += NumSamples;
				}
			}

			Direct3DDeviceIMContext->Unmap(StagingTexture2D,0);
		}
	}
	else if ( SurfaceDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
	{
		for(UINT SampleIndex = 0;SampleIndex < NumSamples;++SampleIndex)
		{
			// Resolve the sample to the non-MSAA render target.
			ResolveSurfaceUsingShader<FResolveSingleSamplePixelShader>(
				Surface,
				NULL,
				NonMSAARTV,
				NULL,
				NonMSAADesc,
				FResolveRect(MinX,MinY,MaxX + 1,MaxY + 1),
				FResolveRect(0,0,SizeX,SizeY),
				Direct3DDeviceIMContext,
				SampleIndex
				);

			// Copy the resolved sample data to the staging texture.
			Direct3DDeviceIMContext->CopySubresourceRegion(StagingTexture2D,0,0,0,0,NonMSAATexture2D,Subresource,&Rect);

			// Lock the staging texture.
			D3D11_MAPPED_SUBRESOURCE LockedRect;
			VERIFYD3D11RESULT(Direct3DDeviceIMContext->Map(StagingTexture2D,0,D3D11_MAP_READ,0,&LockedRect));

			FPlane	MinValue(0.0f,0.0f,0.0f,0.0f),
				MaxValue(1.0f,1.0f,1.0f,1.0f);

			check(sizeof(FD3DFloat16)==sizeof(WORD));

			for( UINT Y = MinY; Y <= MaxY; Y++ )
			{
				FD3DFloat16*	SrcPtr = (FD3DFloat16*)((BYTE*)LockedRect.pData + (Y - MinY) * LockedRect.RowPitch);

				for( UINT X = MinX; X <= MaxX; X++ )
				{
					MinValue.X = Min<FLOAT>(SrcPtr[0],MinValue.X);
					MinValue.Y = Min<FLOAT>(SrcPtr[1],MinValue.Y);
					MinValue.Z = Min<FLOAT>(SrcPtr[2],MinValue.Z);
					MinValue.W = Min<FLOAT>(SrcPtr[3],MinValue.W);
					MaxValue.X = Max<FLOAT>(SrcPtr[0],MaxValue.X);
					MaxValue.Y = Max<FLOAT>(SrcPtr[1],MaxValue.Y);
					MaxValue.Z = Max<FLOAT>(SrcPtr[2],MaxValue.Z);
					MaxValue.W = Max<FLOAT>(SrcPtr[3],MaxValue.W);
					SrcPtr += 4;
				}
			}
			
			Direct3DDeviceIMContext->Unmap(StagingTexture2D,0);
		}

		for(UINT SampleIndex = 0;SampleIndex < NumSamples;++SampleIndex)
		{
			// Resolve the sample to the non-MSAA render target.
			ResolveSurfaceUsingShader<FResolveSingleSamplePixelShader>(
				Surface,
				NULL,
				NonMSAARTV,
				NULL,
				NonMSAADesc,
				FResolveRect(MinX,MinY,MaxX + 1,MaxY + 1),
				FResolveRect(0,0,SizeX,SizeY),
				Direct3DDeviceIMContext,
				SampleIndex
				);

			// Copy the resolved sample data to the staging texture.
			Direct3DDeviceIMContext->CopySubresourceRegion(StagingTexture2D,0,0,0,0,NonMSAATexture2D,Subresource,&Rect);

			// Lock the staging texture.
			D3D11_MAPPED_SUBRESOURCE LockedRect;
			VERIFYD3D11RESULT(Direct3DDeviceIMContext->Map(StagingTexture2D,0,D3D11_MAP_READ,0,&LockedRect));

			FPlane	MinValue(0.0f,0.0f,0.0f,0.0f),
				MaxValue(1.0f,1.0f,1.0f,1.0f);

			check(sizeof(FD3DFloat16)==sizeof(WORD));

			for( UINT Y = MinY; Y <= MaxY; Y++ )
			{
				FD3DFloat16* SrcPtr = (FD3DFloat16*)((BYTE*)LockedRect.pData + (Y - MinY) * LockedRect.RowPitch);
				FColor* DestPtr = &OutData((Y - MinY) * SizeX * NumSamples + SampleIndex);

				for( UINT X = MinX; X <= MaxX; X++ )
				{
					FColor NormalizedColor =
						FLinearColor(
						(SrcPtr[0] - MinValue.X) / (MaxValue.X - MinValue.X),
						(SrcPtr[1] - MinValue.Y) / (MaxValue.Y - MinValue.Y),
						(SrcPtr[2] - MinValue.Z) / (MaxValue.Z - MinValue.Z),
						(SrcPtr[3] - MinValue.W) / (MaxValue.W - MinValue.W)
						).ToFColor(TRUE);
					appMemcpy(DestPtr,&NormalizedColor,sizeof(FColor));
					DestPtr += NumSamples;
					SrcPtr += 4;
				}
			}
			
			Direct3DDeviceIMContext->Unmap(StagingTexture2D,0);
		}
	}
	else
	{
		// not supported yet
	}
}

void FD3D11DynamicRHI::ReadSurfaceFloatData(FSurfaceRHIParamRef SurfaceRHI,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<FFloat16Color>& OutData,ECubeFace CubeFace)
{
	DYNAMIC_CAST_D3D11RESOURCE(Surface,Surface);

	UINT SizeX = MaxX - MinX + 1;
	UINT SizeY = MaxY - MinY + 1;
	
	// Select which resource to copy from depending on whether there is a resolve target
	ID3D11Texture2D* SourceSurface = Surface->Resource;
	if( Surface->ResolveTarget2D )
	{
		SourceSurface = Surface->ResolveTarget2D->Resource;
	}
	check(SourceSurface);

	// Check the format of the surface
	D3D11_TEXTURE2D_DESC SurfaceDesc;
	SourceSurface->GetDesc(&SurfaceDesc);

	check(SurfaceDesc.Format == GPixelFormats[PF_FloatRGB].PlatformFormat);

	// Allocate the output buffer.
	OutData.Empty((MaxX - MinX + 1) * (MaxY - MinY + 1) * sizeof(FFloat16Color));

	// Read back the surface data from (MinX,MinY) to (MaxX,MaxY)
	D3D11_BOX	Rect;
	Rect.left	= MinX;
	Rect.top	= MinY;
	Rect.right	= MaxX + 1;
	Rect.bottom	= MaxY + 1;
	Rect.back = 1;
	Rect.front = 0;

	// create a temp 2d texture to copy render target to
	TRefCountPtr<ID3D11Texture2D> Texture2D;
	D3D11_TEXTURE2D_DESC Desc;
	Desc.Width = SizeX;
	Desc.Height = SizeY;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = SurfaceDesc.Format;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.Usage = D3D11_USAGE_STAGING;
	Desc.BindFlags = 0;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	Desc.MiscFlags = 0;
	VERIFYD3D11RESULT(Direct3DDevice->CreateTexture2D(&Desc,NULL,Texture2D.GetInitReference()));

	// Copy the data to a staging resource.
	UINT Subresource = 0;
	if( SurfaceDesc.MiscFlags == D3D11_RESOURCE_MISC_TEXTURECUBE )
	{
		UINT D3DFace = GetD3D11CubeFace(CubeFace);
		Subresource = D3D11CalcSubresource(0,D3DFace,1);
	}
	Direct3DDeviceIMContext->CopySubresourceRegion(Texture2D,0,0,0,0,SourceSurface,Subresource,&Rect);

	// Lock the staging resource.
	D3D11_MAPPED_SUBRESOURCE LockedRect;
	VERIFYD3D11RESULT(Direct3DDeviceIMContext->Map(Texture2D,0,D3D11_MAP_READ,0,&LockedRect));

	// Presize the array
	INT TotalCount = SizeX * SizeY;
	if (TotalCount >= OutData.Num())
	{
		OutData.AddZeroed(TotalCount);
	}

	// Read the data out of the buffer, converting it from ABGR to ARGB.
	for(UINT Y = MinY;Y <= MaxY;Y++)
	{
		FFloat16Color* SrcPtr = (FFloat16Color*)((BYTE*)LockedRect.pData + (Y - MinY) * LockedRect.RowPitch);
		INT Index = (Y - MinY) * SizeX;
		check(Index < OutData.Num());
		FFloat16Color* DestColor = &OutData(Index);
		FFloat16* DestPtr = (FFloat16*)(DestColor);
		appMemcpy(DestPtr,SrcPtr,SizeX * sizeof(FFloat16) * 4);
	}

	Direct3DDeviceIMContext->Unmap(Texture2D,0);
}

/**
 *	Copies the contents of the back buffer to specified texture.
 *	@param ResolveParams Required resolve params
 */
void FD3D11DynamicRHI::CopyFrontBufferToTexture( const FResolveParams& ResolveParams )
{
	// Not supported
}


#if !RHI_UNIFIED_MEMORY && !USE_NULL_RHI
void FD3D11DynamicRHI::GetTargetSurfaceSize(FSurfaceRHIParamRef SurfaceRHI, UINT& OutSizeX, UINT& OutSizeY)
{
    if ( SurfaceRHI )
    {
        DYNAMIC_CAST_D3D11RESOURCE(Surface,Surface);

        ID3D11Texture2D* SourceSurface = Surface->Resource;
        check(SourceSurface);

        D3D11_TEXTURE2D_DESC SurfaceDesc;
        SourceSurface->GetDesc(&SurfaceDesc);
        OutSizeX = SurfaceDesc.Width;
        OutSizeY = SurfaceDesc.Height;
    }
    else
    {
        OutSizeX = 0;
        OutSizeY = 0;
    }
}
#endif
