/*=============================================================================
	D3D9RenderTarget.cpp: D3D render target implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D9DrvPrivate.h"
#include "BatchedElements.h"
#include "ScreenRendering.h"

#if DWTRIOVIZSDK
// support for capturing depth to the alpha channel
#include "DwTrioviz/DwTriovizImpl.h"
#endif

/** An option to emulate platforms which require resolving surfaces to a texture before sampling. */
#define REQUIRE_D3D_RESOLVE 0

/**
* Copies the contents of the given surface to its resolve target texture.
* @param SourceSurface - surface with a resolve texture to copy to
* @param bKeepOriginalSurface - TRUE if the original surface will still be used after this function so must remain valid
* @param ResolveParams - optional resolve params
*/
void FD3D9DynamicRHI::CopyToResolveTarget(FSurfaceRHIParamRef SourceSurfaceRHI, UBOOL bKeepOriginalSurface, const FResolveParams& ResolveParams)
{
	DYNAMIC_CAST_D3D9RESOURCE(Surface,SourceSurface);
	FD3D9Texture2D* ResolveTargetParameter = (FD3D9Texture2D*)ResolveParams.ResolveTarget;
	TRefCountPtr<FD3D9Texture2D> ResolveTarget2D = ResolveParams.ResolveTarget ? ResolveTargetParameter : SourceSurface->ResolveTargetTexture2D;

	if( REQUIRE_D3D_RESOLVE || 
		 SourceSurface->Texture2D != ResolveTarget2D ||
		 SourceSurface->ResolveTargetTextureCube )
	{
		// surface can't be a part of both 2d/cube textures
		check(!(SourceSurface->Texture2D && SourceSurface->TextureCube));
		// surface can't have both 2d/cube resolve target textures
		check(!(SourceSurface->ResolveTargetTexture2D && SourceSurface->ResolveTargetTextureCube));

		if (bTrackingEvents && CurrentEventNode)
		{
			CurrentEventNode->NumDraws++;
		}

		// Get a handle for the destination surface.
		TRefCountPtr<IDirect3DSurface9> DestinationSurface;
		// resolving to 2d texture
		if( ResolveTarget2D )
		{
			// get level 0 surface from 2d texture
			VERIFYD3D9RESULT((*ResolveTarget2D)->GetSurfaceLevel(0,DestinationSurface.GetInitReference()));
			if( DestinationSurface == *SourceSurface )
			{
				// Nothing to do if the dest and source surfaces match
				return;
			}
		}
		// resolving to cube texture
		else
		{
			// get cube face from cube texture
			VERIFYD3D9RESULT((*SourceSurface->ResolveTargetTextureCube)->GetCubeMapSurface(
				GetD3D9CubeFace(ResolveParams.CubeFace),
				0,
				DestinationSurface.GetInitReference()));
		}		
      
		D3DSURFACE_DESC SourceDesc;
		(*SourceSurface)->GetDesc(&SourceDesc);
		if (!SourceSurface->Texture2D || !SourceSurface->ResolveTargetTexture2D || SourceDesc.MultiSampleType > 1)
		{
			RECT ResolveRect;
			RECT* ResolveRectPtr = NULL;
			if (ResolveParams.Rect.X1 >= 0 && ResolveParams.Rect.X2 >= 0 && ResolveParams.Rect.Y1 >= 0 && ResolveParams.Rect.Y2 >= 0)
			{
				ResolveRect.left	= ResolveParams.Rect.X1;
				ResolveRect.top		= ResolveParams.Rect.Y1;
				ResolveRect.right	= ResolveParams.Rect.X2;
				ResolveRect.bottom	= ResolveParams.Rect.Y2;

				ResolveRectPtr = &ResolveRect;
			}

			// Use StretchRect to resolve the MSAA surface to the non-MSAA texture
			// StretchRect is the only way to access an MSAA surface through a texture in d3d9
			Direct3DDevice->StretchRect(*SourceSurface, ResolveRectPtr, DestinationSurface, ResolveRectPtr, D3DTEXF_NONE);
		}
		else
		{
			// Construct a temporary FTexture to represent the source surface.
			FTexture TempTexture;
			TempTexture.SamplerStateRHI = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
			TempTexture.TextureRHI = SourceSurface->Texture2D;

			// Determine the destination surface size.
			D3DSURFACE_DESC DestinationDesc;
			VERIFYD3D9RESULT(DestinationSurface->GetDesc(&DestinationDesc));

			// Generate the vertices used to copy from the source surface to the destination surface.
			FLOAT MinX; FLOAT MinY;
			FLOAT MaxX; FLOAT MaxY;
			FLOAT MinU; FLOAT MinV;
			FLOAT MaxU; FLOAT MaxV;

			// If ResolveParams has valid dimensions, resolve those instead of the full buffer
			if (ResolveParams.Rect.X1 >= 0 && ResolveParams.Rect.X2 >= 0 && ResolveParams.Rect.Y1 >= 0 && ResolveParams.Rect.Y2 >= 0)
			{
				MinX = 2.0f * (ResolveParams.Rect.X1 - GPixelCenterOffset) / ((FLOAT)DestinationDesc.Width) - 1.0f;
				MinY = -2.0f * (ResolveParams.Rect.Y1 - GPixelCenterOffset) / ((FLOAT)DestinationDesc.Height) + 1.0f;
				MaxX = 2.0f * (ResolveParams.Rect.X2 - GPixelCenterOffset) / ((FLOAT)DestinationDesc.Width) - 1.0f;
				MaxY = -2.0f * (ResolveParams.Rect.Y2 - GPixelCenterOffset) / ((FLOAT)DestinationDesc.Height) + 1.0f;
				MinU = ResolveParams.Rect.X1 / (FLOAT)DestinationDesc.Width;
				MinV = ResolveParams.Rect.Y1 / (FLOAT)DestinationDesc.Height;
				MaxU = ResolveParams.Rect.X2 / (FLOAT)DestinationDesc.Width;
				MaxV = ResolveParams.Rect.Y2 / (FLOAT)DestinationDesc.Height;
			}
			else
			{
				MinX = -1.0f - GPixelCenterOffset / ((FLOAT)DestinationDesc.Width * 0.5f);
				MaxX = +1.0f - GPixelCenterOffset / ((FLOAT)DestinationDesc.Width * 0.5f);
				MinY = +1.0f + GPixelCenterOffset / ((FLOAT)DestinationDesc.Height * 0.5f);
				MaxY = -1.0f + GPixelCenterOffset / ((FLOAT)DestinationDesc.Height * 0.5f);
				MinU = 0.0f;
				MinV = 0.0f;
				MaxU = 1.0f;
				MaxV = 1.0f;
			}

			FBatchedElements BatchedElements;
			INT V00 = BatchedElements.AddVertex(FVector4(MinX,MinY,0,1),FVector2D(MinU,MinV),FColor(255,255,255),FHitProxyId());
			INT V10 = BatchedElements.AddVertex(FVector4(MaxX,MinY,0,1),FVector2D(MaxU,MinV),FColor(255,255,255),FHitProxyId());
			INT V01 = BatchedElements.AddVertex(FVector4(MinX,MaxY,0,1),FVector2D(MinU,MaxV),FColor(255,255,255),FHitProxyId());
			INT V11 = BatchedElements.AddVertex(FVector4(MaxX,MaxY,0,1),FVector2D(MaxU,MaxV),FColor(255,255,255),FHitProxyId());

			// Reset all texture references, to ensure a reference to this render target doesn't remain set.
			UnsetPSTextures();
			UnsetVSTextures();

			// Set the destination texture as the render target.
			Direct3DDevice->SetRenderTarget(0,DestinationSurface);
			Direct3DDevice->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);

			// No alpha blending, no depth tests or writes, no stencil tests or writes, no backface culling.
			RHISetBlendState(TStaticBlendState<>::GetRHI());
			RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
			RHISetStencilState(TStaticStencilState<>::GetRHI());
			RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());

			// Draw a quad using the generated vertices.
			BatchedElements.AddTriangle(V00,V10,V11,&TempTexture,BLEND_Opaque);
			BatchedElements.AddTriangle(V00,V11,V01,&TempTexture,BLEND_Opaque);
			BatchedElements.Draw(FMatrix::Identity,DestinationDesc.Width,DestinationDesc.Height,FALSE);
		}
	}
}

void FD3D9DynamicRHI::CopyFromResolveTargetFast(FSurfaceRHIParamRef DestSurface)
{
	// these need to be referenced in order for the FScreenVertexShader/FScreenPixelShader types to not be compiled out on PC
	TShaderMapRef<FScreenVertexShader> ScreenVertexShader(GetGlobalShaderMap());
	TShaderMapRef<FScreenPixelShader> ScreenPixelShader(GetGlobalShaderMap());
}

void FD3D9DynamicRHI::CopyFromResolveTargetRectFast(FSurfaceRHIParamRef DestSurface,FLOAT X1,FLOAT Y1,FLOAT X2,FLOAT Y2)
{
	// these need to be referenced in order for the FScreenVertexShader/FScreenPixelShader types to not be compiled out on PC
	TShaderMapRef<FScreenVertexShader> ScreenVertexShader(GetGlobalShaderMap());
	TShaderMapRef<FScreenPixelShader> ScreenPixelShader(GetGlobalShaderMap());
}

void FD3D9DynamicRHI::CopyFromResolveTarget(FSurfaceRHIParamRef DestSurface)
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
FTexture2DRHIRef FD3D9DynamicRHI::GetResolveTarget( FSurfaceRHIParamRef SurfaceRHI )
{
	DYNAMIC_CAST_D3D9RESOURCE(Surface,Surface);
	return Surface->ResolveTargetTexture2D.GetReference();
}

void FD3D9DynamicRHI::DiscardRenderBuffer( DWORD RenderBufferTypes )
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
FSurfaceRHIRef FD3D9DynamicRHI::CreateTargetableSurface(
	UINT SizeX,
	UINT SizeY,
	BYTE Format,
	FTexture2DRHIParamRef ResolveTargetTextureRHI,
	DWORD Flags,
	const TCHAR* UsageStr
	)
{
	DYNAMIC_CAST_D3D9RESOURCE(Texture2D,ResolveTargetTexture);

	UBOOL bDepthFormat = (Format == PF_DepthStencil || Format == PF_ShadowDepth|| Format == PF_FilteredShadowDepth || Format == PF_D24);

	// Determine whether to use MSAA for this surface.
	UINT MaxMultiSamples = GSystemSettings.bAllowD3D9MSAA ? GSystemSettings.MaxMultiSamples : 1;
	UINT MultiSampleCount = 0;
	UINT MultiSampleQuality = 0;

	if (MaxMultiSamples > 1 && (Flags & TargetSurfCreate_Multisample))
	{
		GetMultisampleCountAndQuality(MultiSampleCount, MultiSampleQuality);
		DWORD QualityLevels = 0;
		const HRESULT hr = Direct3D->CheckDeviceMultiSampleType(AdapterIndex, DeviceType,(D3DFORMAT)GPixelFormats[Format].PlatformFormat, !bIsFullscreenDevice, (D3DMULTISAMPLE_TYPE)MultiSampleCount, &QualityLevels);
		
		// CheckDeviceMultiSampleType returns the *number* of quality levels so when 8xCSAA is supported, NumQualityLevels is 9.
		// This should never happen because MSAA is disabled in FD3D9DynamicRHI::UpdateD3DDeviceFromViewports()
		// if any of the formats that need MSAA is not supported with the current MSAA mode.
		checkfSlow(hr == D3D_OK && QualityLevels >= MultiSampleQuality, 
			TEXT("This DX9 MSAA mode is not supported (Fmt=%s, S=%d, Q=%d)"),
			*GetD3DTextureFormatString((D3DFORMAT)GPixelFormats[Format].PlatformFormat), 
			MultiSampleCount, 
			MultiSampleQuality);

		// MSAA surfaces can't be shared with a texture.
		Flags |= TargetSurfCreate_Dedicated;
	}

	if(ResolveTargetTexture)
	{
		checkMsg(!(Flags&TargetSurfCreate_Readable),TEXT("Cannot allocate resolvable surfaces with the readable flag."));

		if((Flags&TargetSurfCreate_Dedicated) || REQUIRE_D3D_RESOLVE)
		{
			// Retrieve the texture's surface.
			TRefCountPtr<IDirect3DSurface9> TargetableSurface;

			// If MSAA is enabled create a surface
			if( bDepthFormat )
			{
				// Create a depthstencil surface
				VERIFYD3D9RESULT(Direct3DDevice->CreateDepthStencilSurface(
					SizeX,
					SizeY,
					(D3DFORMAT)GPixelFormats[Format].PlatformFormat,
					(D3DMULTISAMPLE_TYPE) MultiSampleCount,
					MultiSampleQuality,
					FALSE,
					TargetableSurface.GetInitReference(),
					NULL
					));

				return new FD3D9Surface(
					ResolveTargetTexture,
					NULL,
					TargetableSurface
					);
			}
			else if( MultiSampleCount > 1 )
			{
				// Create a rendertarget
				VERIFYD3D9RESULT(Direct3DDevice->CreateRenderTarget(
					SizeX,
					SizeY,
					(D3DFORMAT)GPixelFormats[Format].PlatformFormat,
					(D3DMULTISAMPLE_TYPE) MultiSampleCount,
					MultiSampleQuality,
					FALSE,
					TargetableSurface.GetInitReference(),
					NULL
					));

				return new FD3D9Surface(
					ResolveTargetTexture,
					NULL,
					TargetableSurface
					);
			}
			else
			{
				// Create a targetable texture.
				TRefCountPtr<IDirect3DTexture9> TargetableTexture;
				VERIFYD3D9RESULT(Direct3DDevice->CreateTexture(
					SizeX,
					SizeY,
					1,
					bDepthFormat ? D3DUSAGE_DEPTHSTENCIL : D3DUSAGE_RENDERTARGET,
					(D3DFORMAT)GPixelFormats[Format].PlatformFormat,
					D3DPOOL_DEFAULT,
					TargetableTexture.GetInitReference(),
					NULL
					));

			    // Retrieve the texture's surface.
			    VERIFYD3D9RESULT(TargetableTexture->GetSurfaceLevel(0,TargetableSurface.GetInitReference()));

			    return new FD3D9Surface(
				    ResolveTargetTexture,
				    new FD3D9Texture2D(EPixelFormat(Format),FALSE,FALSE,TargetableTexture),
				    TargetableSurface
				    );
			    }
		}
		else
		{
			// Simply return resolve target texture's surface.
			TRefCountPtr<IDirect3DSurface9> TargetableSurface;
			VERIFYD3D9RESULT((*ResolveTargetTexture)->GetSurfaceLevel(0,TargetableSurface.GetInitReference()));

			return new FD3D9Surface(
				ResolveTargetTexture,
				ResolveTargetTexture,
				TargetableSurface
				);
		}
	}
	else
	{
		if(bDepthFormat)
		{
			// Create a depth-stencil target surface.
			TRefCountPtr<IDirect3DSurface9> Surface;
			VERIFYD3D9RESULT(Direct3DDevice->CreateDepthStencilSurface(
				SizeX,
				SizeY,
				(D3DFORMAT)GPixelFormats[Format].PlatformFormat,
				(D3DMULTISAMPLE_TYPE)MultiSampleCount,
				MultiSampleQuality,
				TRUE,
				Surface.GetInitReference(),
				NULL
				));

			return new FD3D9Surface(
				(FD3D9Texture2D*)NULL,
				NULL,
				Surface
				);
		}
		else
		{
			checkMsg((Flags&TargetSurfCreate_Readable),TEXT("Surface created which isn't readable or resolvable.  Is that intentional?"));

			// Create a render target surface.
			TRefCountPtr<IDirect3DSurface9> Surface;
			VERIFYD3D9RESULT(Direct3DDevice->CreateRenderTarget(
				SizeX,
				SizeY,
				(D3DFORMAT)GPixelFormats[Format].PlatformFormat,
				(D3DMULTISAMPLE_TYPE)MultiSampleCount,
				MultiSampleQuality,
				(Flags&TargetSurfCreate_Readable),
				Surface.GetInitReference(),
				NULL
				));

			return new FD3D9Surface(
				(FD3D9Texture2D*)NULL,
				NULL,
				Surface
				);
		}
	}
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
FSurfaceRHIRef FD3D9DynamicRHI::CreateTargetableCubeSurface(
	UINT SizeX,
	BYTE Format,
	FTextureCubeRHIParamRef ResolveTargetTextureRHI,
	ECubeFace CubeFace,
	DWORD Flags,
	const TCHAR* UsageStr
	)
{
	DYNAMIC_CAST_D3D9RESOURCE(TextureCube,ResolveTargetTexture);

	check(Format != PF_DepthStencil);
	if(!ResolveTargetTexture)
	{
		checkMsg(FALSE,TEXT("No resolve target cube texture specified.  Just use RHICreateTargetableSurface instead."));
	}
	else
	{
		checkMsg(!(Flags&TargetSurfCreate_Readable),TEXT("Cannot allocate resolvable surfaces with the readable flag."));

		// create a dedicated texture which contains the target surface
		if((Flags&TargetSurfCreate_Dedicated) || REQUIRE_D3D_RESOLVE)
		{
			// Create a targetable texture.
			TRefCountPtr<IDirect3DTexture9> TargetableTexture;
			VERIFYD3D9RESULT(Direct3DDevice->CreateTexture(
				SizeX,
				SizeX,
				1,
				(Format == PF_ShadowDepth) ? D3DUSAGE_DEPTHSTENCIL : D3DUSAGE_RENDERTARGET,
				(D3DFORMAT)GPixelFormats[Format].PlatformFormat,
				D3DPOOL_DEFAULT,
				TargetableTexture.GetInitReference(),
				NULL
				));

			// Retrieve the texture's surface.
			TRefCountPtr<IDirect3DSurface9> TargetableSurface;
			VERIFYD3D9RESULT(TargetableTexture->GetSurfaceLevel(0,TargetableSurface.GetInitReference()));

			// use a dedicated texture for the target and the cube texture for resolves
			return new FD3D9Surface(
				ResolveTargetTexture,
				new FD3D9Texture2D(EPixelFormat(Format),FALSE,FALSE,TargetableTexture),
				TargetableSurface
				);
		}
		// use a surface from the resolve texture
		else
		{
			// Simply return resolve target texture's surface corresponding to the given CubeFace.
			TRefCountPtr<IDirect3DSurface9> TargetableSurface;
			VERIFYD3D9RESULT((*ResolveTargetTexture)->GetCubeMapSurface(GetD3D9CubeFace(CubeFace),0,TargetableSurface.GetInitReference()));

			// use the same cube texture as the resolve and target textures 
			return new FD3D9Surface(
				ResolveTargetTexture,
				ResolveTargetTexture,
				TargetableSurface
				);
		}
	}

	return NULL;
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
			DWORD	Mantissa : 23,
					Exponent : 8,
					Sign : 1;
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
			WORD	Mantissa : 10,
					Exponent : 5,
					Sign : 1;
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

void FD3D9DynamicRHI::ReadSurfaceData(FSurfaceRHIParamRef SurfaceRHI,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<BYTE>& OutData, FReadSurfaceDataFlags InFlags)
{
	DYNAMIC_CAST_D3D9RESOURCE(Surface,Surface);

	UINT SizeX = MaxX - MinX + 1;
	UINT SizeY = MaxY - MinY + 1;

	// Check the format of the surface.
	D3DSURFACE_DESC SurfaceDesc;
	VERIFYD3D9RESULT((*Surface)->GetDesc(&SurfaceDesc));

	check( SurfaceDesc.Format == D3DFMT_A8R8G8B8
		|| SurfaceDesc.Format == D3DFMT_A16B16G16R16F
		|| SurfaceDesc.Format == D3DFMT_G16R16F
		|| SurfaceDesc.Format == D3DFMT_R32F
		|| SurfaceDesc.Format == D3DFMT_A16B16G16R16);

	check(sizeof(FD3DFloat16)==sizeof(WORD));

	// Allocate the output buffer.
	OutData.Empty(SizeX * SizeY * sizeof(FColor));

	// Read back the surface data from (MinX,MinY) to (MaxX,MaxY)
	D3DLOCKED_RECT	LockedRect;
	RECT			Rect;
	Rect.left	= MinX;
	Rect.top	= MinY;
	Rect.right	= MaxX + 1;
	Rect.bottom	= MaxY + 1;

	TRefCountPtr<IDirect3DSurface9> DestSurface;

	if( Surface->ResolveTargetTexture2D ||
		Surface->ResolveTargetTextureCube )
	{
		// create a temp 2d texture to copy render target to
		TRefCountPtr<IDirect3DTexture9> Texture2D;
		VERIFYD3D9RESULT(Direct3DDevice->CreateTexture(
			SurfaceDesc.Width,
			SurfaceDesc.Height,
			1,
			0,
			SurfaceDesc.Format,
			D3DPOOL_SYSTEMMEM,
			Texture2D.GetInitReference(),
			NULL
			));
		// get its surface
		VERIFYD3D9RESULT(Texture2D->GetSurfaceLevel(0,DestSurface.GetInitReference()));
		// get the render target surface
		TRefCountPtr<IDirect3DSurface9> SrcSurface;
		if( Surface->ResolveTargetTextureCube )
		{
			VERIFYD3D9RESULT((*Surface->ResolveTargetTextureCube)->GetCubeMapSurface(GetD3D9CubeFace(InFlags.GetCubeFace()),0,SrcSurface.GetInitReference()));
		}
		else
		{
			VERIFYD3D9RESULT((*Surface->ResolveTargetTexture2D)->GetSurfaceLevel(0,SrcSurface.GetInitReference()));
		}		
        // copy render target data to memory (this may silently fail if Device is Lost).
		Direct3DDevice->GetRenderTargetData(SrcSurface,DestSurface.GetReference());
	}

	if (DestSurface.GetReference() == NULL)
	{
		DestSurface = *Surface;
	}
	
	VERIFYD3D9RESULT(DestSurface->LockRect(&LockedRect,&Rect,D3DLOCK_READONLY));

	// [4] as we have 4 texture channels
	FLOAT MinValue[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	FLOAT MaxValue[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	const ERangeCompressionMode CompressionMode = InFlags.GetCompressionMode();
	UBOOL bLinearToGamma = InFlags.GetLinearToGamma();

	if(CompressionMode == RCM_SNorm)
	{
		MinValue[0] = MinValue[1] = MinValue[2] = MinValue[3] = -1.0f;
	}
	else if(CompressionMode == RCM_MinMaxNorm || CompressionMode == RCM_MinMax)
	{
		if(CompressionMode == RCM_MinMax)
		{
			MinValue[0] = MinValue[1] = MinValue[2] = MinValue[3] = FLT_MAX;
			MaxValue[0] = MaxValue[1] = MaxValue[2] = MaxValue[3] = -FLT_MAX;		
		}

		// compute Min and Max depending on the pixel format
		if(SurfaceDesc.Format == D3DFMT_A16B16G16R16F)
		{
			for(UINT Y = MinY; Y <= MaxY; Y++)
			{
				FD3DFloat16* SrcPtr = (FD3DFloat16*)((BYTE*)LockedRect.pBits + (Y - MinY) * LockedRect.Pitch);

				for(UINT X = MinX; X <= MaxX; X++, SrcPtr += 4)
				{
					for(UINT e = 0 ;e < 4; ++e)
					{
						MinValue[e] = Min<FLOAT>(SrcPtr[e], MinValue[e]);
						MaxValue[e] = Max<FLOAT>(SrcPtr[e], MaxValue[e]);
					}
				}
			}
		}	
		else if(SurfaceDesc.Format == D3DFMT_G16R16F)
		{
			for(UINT Y = MinY; Y <= MaxY; Y++)
			{
				FD3DFloat16* SrcPtr = (FD3DFloat16*)((BYTE*)LockedRect.pBits + (Y - MinY) * LockedRect.Pitch);

				for(UINT X = MinX; X <= MaxX; X++, SrcPtr += 2)
				{
					for(UINT e = 0 ;e < 2; ++e)
					{
						MinValue[e] = Min<FLOAT>(SrcPtr[e], MinValue[e]);
						MaxValue[e] = Max<FLOAT>(SrcPtr[e], MaxValue[e]);
					}
				}
			}
		}
		else if( SurfaceDesc.Format == D3DFMT_R32F)
		{
			for(UINT Y = MinY; Y <= MaxY; Y++)
			{
				FLOAT* SrcPtr = (FLOAT*)((BYTE*)LockedRect.pBits + (Y - MinY) * LockedRect.Pitch);

				for(UINT X = MinX; X <= MaxX; X++, ++SrcPtr)
				{
					MinValue[0] = Min<FLOAT>(SrcPtr[0], MinValue[0]);
					MaxValue[0] = Max<FLOAT>(SrcPtr[0], MaxValue[0]);
				}
			}
		}
		else
		{
			// not supported yet
		}
	}

	// [4] as we have 4 texture channels
	FLOAT ScaleValue[4] = 
	{ 
		1.0f / (MaxValue[0] - MinValue[0]),
		1.0f / (MaxValue[1] - MinValue[1]),
		1.0f / (MaxValue[2] - MinValue[2]),
		1.0f / (MaxValue[3] - MinValue[3])
	};

	// finally create the output data
	if(SurfaceDesc.Format == D3DFMT_A8R8G8B8)
	{
		for(UINT Y = MinY;Y <= MaxY;Y++)
		{
			BYTE* SrcPtr = (BYTE*)LockedRect.pBits + (Y - MinY) * LockedRect.Pitch;
			BYTE* DestPtr = (BYTE*)&OutData(OutData.Add(SizeX * sizeof(FColor)));
			appMemcpy(DestPtr,SrcPtr,SizeX * sizeof(FColor));
		}

#if DWTRIOVIZSDK
		DwTriovizImpl_CaptureDepth(MinX,MinY,MaxX,MaxY,OutData);
#endif
	}
	else if(SurfaceDesc.Format == D3DFMT_A16B16G16R16F)
	{
		for(UINT Y = MinY; Y <= MaxY; Y++)
		{
			FD3DFloat16* SrcPtr = (FD3DFloat16*)((BYTE*)LockedRect.pBits + (Y - MinY) * LockedRect.Pitch);
			FColor* DestPtr = (FColor*)(BYTE*)&OutData(OutData.Add(SizeX * sizeof(FColor)));

			for(UINT X = MinX; X <= MaxX; X++, SrcPtr += 4)
			{
				FColor NormalizedColor(FLinearColor(
					(SrcPtr[0] - MinValue[0]) * ScaleValue[0],
					(SrcPtr[1] - MinValue[1]) * ScaleValue[1],
					(SrcPtr[2] - MinValue[2]) * ScaleValue[2],
					(SrcPtr[3] - MinValue[3]) * ScaleValue[3]).ToFColor(bLinearToGamma));
				appMemcpy(DestPtr++,&NormalizedColor,sizeof(FColor));
			}
		}

#if DWTRIOVIZSDK
		DwTriovizImpl_CaptureDepth(MinX,MinY,MaxX,MaxY,OutData);
#endif
	}	
	else if(SurfaceDesc.Format == D3DFMT_G16R16F)
	{
		for(UINT Y = MinY; Y <= MaxY; Y++)
		{
			FD3DFloat16* SrcPtr = (FD3DFloat16*)((BYTE*)LockedRect.pBits + (Y - MinY) * LockedRect.Pitch);
			FColor* DestPtr = (FColor*)(BYTE*)&OutData(OutData.Add(SizeX * sizeof(FColor)));

			for( UINT X = MinX; X <= MaxX; X++, SrcPtr += 2)
			{
				FColor NormalizedColor(FLinearColor(
					(SrcPtr[0] - MinValue[0]) * ScaleValue[0],
					(SrcPtr[1] - MinValue[1]) * ScaleValue[1],
					0,
					0).ToFColor(bLinearToGamma));
				appMemcpy(DestPtr++,&NormalizedColor,sizeof(FColor));
			}
		}
	}
	else if(SurfaceDesc.Format == D3DFMT_R32F)
	{
		for(UINT Y = MinY; Y <= MaxY; Y++)
		{
			FLOAT* SrcPtr = (FLOAT*)((BYTE*)LockedRect.pBits + (Y - MinY) * LockedRect.Pitch);
			FColor* DestPtr = (FColor*)(BYTE*)&OutData(OutData.Add(SizeX * sizeof(FColor)));

			for(UINT X = MinX; X <= MaxX; X++, ++SrcPtr)
			{
				FColor NormalizedColor(FLinearColor(
					(SrcPtr[0]- MinValue[0]) * ScaleValue[0],
					0,
					0,
					0).ToFColor(bLinearToGamma));
				appMemcpy(DestPtr++, &NormalizedColor, sizeof(FColor));
			}
		}
	}	
	else if(SurfaceDesc.Format == D3DFMT_A16B16G16R16)
	{
		// unsigned 16 bit color assumed to be in normalized color range
		const FLOAT Inv = 1.0f / 0xffff;

		for(UINT Y = MinY; Y <= MaxY; Y++)
		{
			USHORT*	SrcPtr = (USHORT*)((BYTE*)LockedRect.pBits + (Y - MinY) * LockedRect.Pitch);
			FColor* DestPtr = (FColor*)(BYTE*)&OutData(OutData.Add(SizeX * sizeof(FColor)));

			for(UINT X = MinX; X <= MaxX; X++, SrcPtr += 4)
			{
				FColor NormalizedColor = FLinearColor(
					SrcPtr[0] * Inv,
					SrcPtr[1] * Inv,
					SrcPtr[2] * Inv,
					SrcPtr[3] * Inv).ToFColor(TRUE);
				appMemcpy(DestPtr++,&NormalizedColor,sizeof(FColor));
			}
		}

#if DWTRIOVIZSDK
		DwTriovizImpl_CaptureDepth(MinX,MinY,MaxX,MaxY,OutData);
#endif
	}

	DestSurface->UnlockRect();
}

void FD3D9DynamicRHI::ReadSurfaceDataMSAA(FSurfaceRHIParamRef SurfaceRHI,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
}

void FD3D9DynamicRHI::ReadSurfaceFloatData(FSurfaceRHIParamRef SurfaceRHI,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<FFloat16Color>& OutData,ECubeFace CubeFace)
{
	DYNAMIC_CAST_D3D9RESOURCE(Surface,Surface);

	UINT SizeX = MaxX - MinX + 1;
	UINT SizeY = MaxY - MinY + 1;

	// Check the format of the surface.
	D3DSURFACE_DESC SurfaceDesc;
	VERIFYD3D9RESULT((*Surface)->GetDesc(&SurfaceDesc));

	if (SurfaceDesc.Format != D3DFMT_A16B16G16R16F)
	{
		return;
	}

	// Allocate the output buffer.
	OutData.Empty(SizeX * SizeY * sizeof(FFloat16) * 4);

	// Read back the surface data from (MinX,MinY) to (MaxX,MaxY)
	D3DLOCKED_RECT LockedRect;
	RECT Rect;
	Rect.left	= MinX;
	Rect.top	= MinY;
	Rect.right	= MaxX + 1;
	Rect.bottom	= MaxY + 1;

	TRefCountPtr<IDirect3DSurface9> DestSurface;

	if( Surface->ResolveTargetTexture2D ||
		Surface->ResolveTargetTextureCube )
	{
		// create a temp 2d texture to copy render target to
		TRefCountPtr<IDirect3DTexture9> Texture2D;
		VERIFYD3D9RESULT(Direct3DDevice->CreateTexture(
			SurfaceDesc.Width,
			SurfaceDesc.Height,
			1,
			0,
			SurfaceDesc.Format,
			D3DPOOL_SYSTEMMEM,
			Texture2D.GetInitReference(),
			NULL
			));
		// get its surface
		VERIFYD3D9RESULT(Texture2D->GetSurfaceLevel(0,DestSurface.GetInitReference()));
		// get the render target surface
		TRefCountPtr<IDirect3DSurface9> SrcSurface;
		if( Surface->ResolveTargetTextureCube )
		{
			VERIFYD3D9RESULT((*Surface->ResolveTargetTextureCube)->GetCubeMapSurface(GetD3D9CubeFace(CubeFace),0,SrcSurface.GetInitReference()));
		}
		else
		{
			VERIFYD3D9RESULT((*Surface->ResolveTargetTexture2D)->GetSurfaceLevel(0,SrcSurface.GetInitReference()));
		}		
		// copy render target data to memory (this may silently fail if Device is Lost).
		Direct3DDevice->GetRenderTargetData(SrcSurface,DestSurface.GetReference());
	}

	if (DestSurface.GetReference() == NULL)
	{
		DestSurface = *Surface;
	}
	
	VERIFYD3D9RESULT(DestSurface->LockRect(&LockedRect,&Rect,D3DLOCK_READONLY));

	// Presize the array
	INT TotalCount = SizeX * SizeY;
	if (TotalCount >= OutData.Num())
	{
		OutData.AddZeroed(TotalCount);
	}

	for(UINT Y = MinY;Y <= MaxY;Y++)
	{
		FD3DFloat16* SrcPtr = (FD3DFloat16*)((BYTE*)LockedRect.pBits + (Y - MinY) * LockedRect.Pitch);
		INT Index = (Y - MinY) * SizeX;
		check(Index < OutData.Num());
		FFloat16Color* DestColor = &OutData(Index);
		FFloat16* DestPtr = (FFloat16*)(DestColor);
		appMemcpy(DestPtr,SrcPtr,SizeX * sizeof(FFloat16) * 4);
	}

	DestSurface->UnlockRect();
}

/**
 *	Copies the contents of the back buffer to specified texture.
 *	@param ResolveParams Required resolve params
 */
void FD3D9DynamicRHI::CopyFrontBufferToTexture( const FResolveParams& ResolveParams )
{
	// Not supported
}


#if !RHI_UNIFIED_MEMORY && !USE_NULL_RHI
void FD3D9DynamicRHI::GetTargetSurfaceSize(FSurfaceRHIParamRef SurfaceRHI, UINT& OutSizeX, UINT& OutSizeY)
{
    if ( SurfaceRHI )
    {
        DYNAMIC_CAST_D3D9RESOURCE(Surface,Surface);

        D3DSURFACE_DESC SurfaceDesc;
        (*Surface)->GetDesc(&SurfaceDesc);
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
