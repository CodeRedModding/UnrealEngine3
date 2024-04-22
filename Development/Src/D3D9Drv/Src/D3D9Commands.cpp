/*=============================================================================
	D3D9Commands.cpp: D3D RHI commands implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D9DrvPrivate.h"

#include "EngineParticleClasses.h"
#include "ChartCreation.h"

#include <xnamath.h>
// Globals

/**
 *	Used for checking if the device is lost or not.
 *	Using the global variable is not enough, since it's updated to rarely.
 */
extern UBOOL GParanoidDeviceLostChecking /*= TRUE*/;
#define IsDeviceLost()	(GParanoidDeviceLostChecking && Direct3DDevice->TestCooperativeLevel() != D3D_OK)

// Vertex state.
void FD3D9DynamicRHI::SetStreamSource(UINT StreamIndex,FVertexBufferRHIParamRef VertexBufferRHI,UINT Stride,UINT Offset,UBOOL bUseInstanceIndex,UINT NumVerticesPerInstance,UINT NumInstances)
{
    check(StreamIndex < NumVertexStreams);

    DYNAMIC_CAST_D3D9RESOURCE(VertexBuffer,VertexBuffer);

    UBOOL bUseStride = TRUE;
    DWORD Frequency = 1;
    // Set the vertex stream frequency.
    if(GSupportsVertexInstancing)
    {
        PendingNumInstances = 1;
        if (bUseInstanceIndex || NumInstances > 1)
        {
            Frequency = bUseInstanceIndex ? 
                (D3DSTREAMSOURCE_INSTANCEDATA | 1) :
            (D3DSTREAMSOURCE_INDEXEDDATA | NumInstances);
        }
    }
    else
    {
        PendingNumInstances = NumInstances;
        // some instanced calls may have only one instance (ProcBuildings)
        if (/*NumInstances > 1 && */ bUseInstanceIndex)
        {
            PendingStreams[StreamIndex].VertexBuffer = VertexBuffer;
            PendingStreams[StreamIndex].Stride = Stride;
            PendingStreams[StreamIndex].Offset = Offset;
            PendingStreams[StreamIndex].NumVerticesPerInstance = NumVerticesPerInstance;
            bUseStride = FALSE; // We don't want this to advance per instance, but rather be advanced manually for fake instancing
            UpdateStreamForInstancingMask |= 1<<StreamIndex;
        }
    }
    check(VertexBuffer);
    Direct3DDevice->SetStreamSource(StreamIndex,VertexBuffer,Offset,bUseStride ? Stride : 0);
    Direct3DDevice->SetStreamSourceFreq(StreamIndex,Frequency);
}

// Rasterizer state.
void FD3D9DynamicRHI::SetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_D3D9RESOURCE(RasterizerState,NewState);

	Direct3DDevice->SetRenderState(D3DRS_FILLMODE,NewState->FillMode);
	Direct3DDevice->SetRenderState(D3DRS_CULLMODE,NewState->CullMode);
	// Add the global depth bias
	extern FLOAT GDepthBiasOffset;
	Direct3DDevice->SetRenderState(D3DRS_DEPTHBIAS,FLOAT_TO_DWORD(NewState->DepthBias + GDepthBiasOffset));
	Direct3DDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,FLOAT_TO_DWORD(NewState->SlopeScaleDepthBias));
}
void FD3D9DynamicRHI::SetViewport(UINT MinX,UINT MinY,FLOAT MinZ,UINT MaxX,UINT MaxY,FLOAT MaxZ)
{
	D3DVIEWPORT9 Viewport = { MinX, MinY, MaxX - MinX, MaxY - MinY, MinZ, MaxZ };
	//avoid setting a 0 extent viewport, which the debug runtime doesn't like
	if (Viewport.Width > 0 && Viewport.Height > 0)
	{
		Direct3DDevice->SetViewport(&Viewport);
	}
}

void FD3D9DynamicRHI::SetScissorRect(UBOOL bEnable,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY)
{
	// Defined in UnPlayer.cpp. Used here to disable scissors when doing highres screenshots.
	extern UBOOL GIsTiledScreenshot;
	bEnable = GIsTiledScreenshot ? FALSE : bEnable;

	if(bEnable)
	{
		RECT ScissorRect;
		ScissorRect.left = MinX;
		ScissorRect.right = MaxX;
		ScissorRect.top = MinY;
		ScissorRect.bottom = MaxY;
		Direct3DDevice->SetScissorRect(&ScissorRect);
	}
	Direct3DDevice->SetRenderState(D3DRS_SCISSORTESTENABLE,bEnable);
}

/**
 * Set depth bounds test state.  
 * When enabled, incoming fragments are killed if the value already in the depth buffer is outside [ClipSpaceNearPos, ClipSpaceFarPos]
 *
 * @param bEnable - whether to enable or disable the depth bounds test
 * @param ClipSpaceNearPos - near point in clip space
 * @param ClipSpaceFarPos - far point in clip space
 */
void FD3D9DynamicRHI::SetDepthBoundsTest(UBOOL bEnable, const FVector4 &ClipSpaceNearPos, const FVector4 &ClipSpaceFarPos)
{
	if (!bDepthBoundsHackSupported)
	{
		// On graphic cards that don't support depth bound test we had code emulating
		// the feature with user clip planes (see source code history) 
		// but that can cause z-fighting (If clipping triangles is implemented by splitting triangles the plane equation for z changes).
		return;
	}

	if (bEnable)
	{
		// convert to normalized device coordinates, which are the units used by Nvidia's D3D depth bounds test driver hack.
		// clamp to valid ranges
		FLOAT MinZ = Clamp(Max(ClipSpaceNearPos.Z, 0.0f) / ClipSpaceNearPos.W, 0.0f, 1.0f);
		FLOAT MaxZ = Clamp(ClipSpaceFarPos.Z / ClipSpaceFarPos.W, 0.0f, 1.0f);

		// enable the depth bounds test
		Direct3DDevice->SetRenderState(D3DRS_ADAPTIVETESS_X,MAKEFOURCC('N','V','D','B'));

		// only set depth bounds if ranges are valid
		if (MinZ <= MaxZ)
		{
			// set the overridden render states which define near and far depth bounds in normalized device coordinates
			// Note: Depth bounds test operates on the value already in the depth buffer, not the incoming fragment!
			Direct3DDevice->SetRenderState(D3DRS_ADAPTIVETESS_Z, FLOAT_TO_DWORD(MinZ));
			Direct3DDevice->SetRenderState(D3DRS_ADAPTIVETESS_W, FLOAT_TO_DWORD(MaxZ));
		}
	}
	else
	{
		// disable depth bounds test
		Direct3DDevice->SetRenderState(D3DRS_ADAPTIVETESS_X,0);
	}
}

/**
* Set bound shader state. This will set the vertex decl/shader, and pixel shader
* @param BoundShaderState - state resource
*/
void FD3D9DynamicRHI::SetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	DYNAMIC_CAST_D3D9RESOURCE(BoundShaderState,BoundShaderState);

	// Clear the vertex streams that were used by the old bound shader state.
	ResetVertexStreams();

	check(BoundShaderState->VertexDeclaration);
	Direct3DDevice->SetVertexDeclaration(BoundShaderState->VertexDeclaration);
	Direct3DDevice->SetVertexShader(BoundShaderState->VertexShader);
	if ( BoundShaderState->PixelShader )
	{
		Direct3DDevice->SetPixelShader(BoundShaderState->PixelShader);
	}
	else
	{
		// use special null pixel shader when PixelSahder was set to NULL
		FPixelShaderRHIParamRef NullPixelShaderRHI = TShaderMapRef<FNULLPixelShader>(GetGlobalShaderMap())->GetPixelShader();
		DYNAMIC_CAST_D3D9RESOURCE(PixelShader,NullPixelShader);
		Direct3DDevice->SetPixelShader(NullPixelShader);
	}

	// Prevent transient bound shader states from being recreated for each use by keeping a history of the most recently used bound shader states.
	// The history keeps them alive, and the bound shader state cache allows them to be reused if needed.
	BoundShaderStateHistory.Add(BoundShaderState);
}

void FD3D9DynamicRHI::SetSamplerStateOnly(FPixelShaderRHIParamRef PixelShaderRHI,UINT /*SamplerIndex*/,FSamplerStateRHIParamRef NewStateRHI)
{
	// Not implemented yet
	check(0);
}

void FD3D9DynamicRHI::SetTextureParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	// Not implemented yet
	check(0);
}

void FD3D9DynamicRHI::SetSurfaceParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,FSurfaceRHIParamRef NewSurfaceRHI)
{
	// Not implemented yet
	check(0);
}

void FD3D9DynamicRHI::SetSurfaceParameter(FComputeShaderRHIParamRef ComputeShaderRHI,UINT TextureIndex,FSurfaceRHIParamRef NewSurfaceRHI)
{
	// Not supported by DX9
	check(0);
}

void FD3D9DynamicRHI::SetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,UINT TextureIndex,FSurfaceRHIParamRef NewSurfaceRHI)
{
	// Not supported by DX9
	check(0);
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
void FD3D9DynamicRHI::SetSamplerState(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,UINT /*SamplerIndex*/,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT LargestMip,FLOAT /*SmallestMip*/,UBOOL bForceLinearMinFilter)
{
	DYNAMIC_CAST_D3D9RESOURCE(PixelShader,PixelShader);
	DYNAMIC_CAST_D3D9RESOURCE(SamplerState,NewState);
	DYNAMIC_CAST_D3D9RESOURCE(Texture,NewTexture);

	// Force linear mip-filter if MipBias has a fractional part.
	D3DTEXTUREFILTERTYPE MipFilter, MinFilter;
	if (NewState->MipMapLODBias || appIsNearlyEqual(appTruncFloat(MipBias), MipBias))
	{
		MipFilter = NewState->MipFilter;
		MinFilter = NewState->MinFilter;
	}
	else
	{
		MipFilter = D3DTEXF_LINEAR;
		MinFilter = D3DTEXF_LINEAR;
	}

	Direct3DDevice->SetTexture(TextureIndex,*NewTexture);

	// If we're emulating mobile rendering and gamma correction for mobile is not enabled, then we'll
	// switch to a linear shader resource view to avoid SRGB correction on texture lookup
	const UBOOL bUseSRGB = 
#if !CONSOLE && !FINAL_RELEASE
		( !GEmulateMobileRendering || GUseGammaCorrectionForMobileEmulation ) &&
#endif
		NewTexture->IsSRGB();

	Direct3DDevice->SetSamplerState(TextureIndex,D3DSAMP_SRGBTEXTURE,bUseSRGB);
	Direct3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MAGFILTER,NewState->MagFilter);
	Direct3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MINFILTER,bForceLinearMinFilter ? D3DTEXF_LINEAR : MinFilter);
	Direct3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MIPFILTER,MipFilter);
	Direct3DDevice->SetSamplerState(TextureIndex,D3DSAMP_ADDRESSU,NewState->AddressU);
	Direct3DDevice->SetSamplerState(TextureIndex,D3DSAMP_ADDRESSV,NewState->AddressV);
	Direct3DDevice->SetSamplerState(TextureIndex,D3DSAMP_ADDRESSW,NewState->AddressW);
	Direct3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MIPMAPLODBIAS,NewState->MipMapLODBias ? NewState->MipMapLODBias : FLOAT_TO_DWORD(MipBias));
	Direct3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MAXMIPLEVEL,(LargestMip < 0.0f) ? 0 : appTrunc(LargestMip));
	Direct3DDevice->SetSamplerState(TextureIndex,D3DSAMP_BORDERCOLOR,NewState->BorderColor);
}

void FD3D9DynamicRHI::SetSamplerState(FVertexShaderRHIParamRef VertexShaderRHI,UINT /*TextureIndex*/,UINT SamplerIndex,FSamplerStateRHIParamRef /*NewStateRHI*/,FTextureRHIParamRef NewTextureRHI,FLOAT /*MipBias*/,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ 
	// DX9 set sample state just passes through to the VTF call
	SetVertexTexture(SamplerIndex,NewTextureRHI);
}

/**
* Sets vertex sampler state.
*
* @param SamplerIndex	Vertex texture sampler index.
* @param Texture		Texture to set
*/
void FD3D9DynamicRHI::SetVertexTexture(UINT SamplerIndex,FTextureRHIParamRef NewTextureRHI)
{
	DYNAMIC_CAST_D3D9RESOURCE(Texture,NewTexture);
	Direct3DDevice->SetTexture(D3DVERTEXTEXTURESAMPLER0+SamplerIndex,*NewTexture);
	Direct3DDevice->SetSamplerState(D3DVERTEXTEXTURESAMPLER0+SamplerIndex, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	Direct3DDevice->SetSamplerState(D3DVERTEXTEXTURESAMPLER0+SamplerIndex, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	Direct3DDevice->SetSamplerState(D3DVERTEXTEXTURESAMPLER0+SamplerIndex, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
}


/**
 * Returns the slot index and the size of a mobile uniform parameter.
 *
 * @param ParamName		Name of the uniform parameter to check for.
 * @param OutNumBytes	[out] Set to the size of the parameter value, in bytes, if the parameter was found.
 * @return				Parameter slot index, or -1 if the parameter was not found.
 */
INT FD3D9DynamicRHI::GetMobileUniformSlotIndexByName(FName ParamName, WORD& OutNumBytes)
{
	// Only used on mobile platforms
	return -1;
}


void FD3D9DynamicRHI::SetMobileTextureSamplerState( FPixelShaderRHIParamRef PixelShader, const INT MobileTextureUnit, FSamplerStateRHIParamRef NewState, FTextureRHIParamRef NewTextureRHI, FLOAT MipBias, FLOAT LargestMip, FLOAT SmallestMip ) 
{
	// Only used on mobile platforms
} 


void FD3D9DynamicRHI::SetMobileSimpleParams(EBlendMode InBlendMode)
{
	// Only used on mobile platforms
}

void FD3D9DynamicRHI::SetMobileMaterialVertexParams(const FMobileMaterialVertexParams& InVertexParams)
{
	// Only used on mobile platforms
}

void FD3D9DynamicRHI::SetMobileMaterialPixelParams(const FMobileMaterialPixelParams& InPixelParams)
{
	// Only used on mobile platforms
}


void FD3D9DynamicRHI::SetMobileMeshVertexParams(const FMobileMeshVertexParams& InMeshParams)
{
	// Only used on mobile platforms
}


void FD3D9DynamicRHI::SetMobileMeshPixelParams(const FMobileMeshPixelParams& InMeshParams)
{
	// Only used on mobile platforms
}


FLOAT FD3D9DynamicRHI::GetMobilePercentColorFade(void)
{
	// Only used on mobile platforms
	return 0.0f;
}


void FD3D9DynamicRHI::SetMobileFogParams (const UBOOL bInEnabled, const FLOAT InFogStart, const FLOAT InFogEnd, const FColor& InFogColor)
{
#if WITH_EDITOR
	// Only used on mobile platforms
	if (GEmulateMobileRendering == TRUE)
	{
		FMobileEmulationMaterialManager::GetManager()->SetMobileFogParams(bInEnabled, InFogStart, InFogEnd, InFogColor);
	}
#endif
}

void FD3D9DynamicRHI::SetMobileHeightFogParams(const FHeightFogParams& Params)
{
	// Only used on mobile platforms
}

void FD3D9DynamicRHI::SetMobileBumpOffsetParams(const UBOOL bInEnabled, const FLOAT InBumpEnd)
{
	// Only used on mobile platforms
}

void FD3D9DynamicRHI::SetMobileGammaCorrection(const UBOOL bInEnabled)
{
#if WITH_EDITOR
	// Only used on mobile platforms
	if (GEmulateMobileRendering == TRUE)
	{
		FMobileEmulationMaterialManager::GetManager()->SetGammaCorrectionEnabled(bInEnabled);
	}
#endif
}

void FD3D9DynamicRHI::SetMobileTextureTransformOverride(TMatrix<3,3>& InOverrideTransform)
{
	// Only used on mobile platforms
}

void FD3D9DynamicRHI::SetMobileDistanceFieldParams (const struct FMobileDistanceFieldParams& Params)
{
	// Only used on mobile platforms
}

void FD3D9DynamicRHI::SetMobileColorGradingParams(const FMobileColorGradingParams& Params)
{
#if WITH_EDITOR
	// Only used on mobile platforms
	if (GEmulateMobileRendering == TRUE)
	{
		FMobileEmulationMaterialManager::GetManager()->SetMobileColorGradingParams(Params);
	}
#endif
}

void* FD3D9DynamicRHI::GetMobileProgramInstance()
{
	// Only used on mobile platforms
	return NULL;
}

void FD3D9DynamicRHI::SetMobileProgramInstance(void* ProgramInstance)
{
	// Only used on mobile platforms
}

void FD3D9DynamicRHI::ResetTrackedPrimitive()
{
	// Not implemented yet
}

void FD3D9DynamicRHI::CycleTrackedPrimitiveMode()
{
	// Not implemented yet
}

void FD3D9DynamicRHI::IncrementTrackedPrimitive(const INT InDelta)
{
	// Not implemented yet
}


void FD3D9DynamicRHI::SetVertexShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI,UINT /*BufferIndex*/,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT /*ParamIndex*/)
{
	Direct3DDevice->SetVertexShaderConstantF(
		BaseIndex / NumBytesPerShaderRegister,
		(FLOAT*)GetPaddedShaderParameterValue(NewValue,NumBytes),
		(NumBytes + NumBytesPerShaderRegister - 1) / NumBytesPerShaderRegister
		);
}

/*
 * *** PERFORMANCE WARNING *****
 * This code is from Mikey W @ Xbox 
 * This function is to support single float array parameter in shader
 * This pads 3 more floats for each element - at the expensive of CPU/stack memory
 * Do not overuse this. If you can, use float4 in shader. 
 */
void FD3D9DynamicRHI::SetVertexShaderFloatArray(FVertexShaderRHIParamRef VertexShaderRHI,UINT /*BufferIndex*/,UINT BaseIndex,UINT NumValues,const FLOAT* FloatValues, INT /*ParamIndex*/)
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
		Direct3DDevice->SetVertexShaderConstantF( BaseIndex / NumBytesPerShaderRegister, (FLOAT*)pPaddedVectorValues, (NumValuesThisGroup*sizeof(FLOAT)*4 + NumBytesPerShaderRegister - 1) / NumBytesPerShaderRegister  );

		// In case we need to continue processing more values, advance to the next group of values
		NumValues	-= NumValuesThisGroup;
		FloatValues	+= NumValuesThisGroup;
		BaseIndex	+= NumValuesThisGroup*NumBytesPerShaderRegister;
	}
}

void FD3D9DynamicRHI::SetVertexShaderBoolParameter(FVertexShaderRHIParamRef VertexShader,UINT /*BufferIndex*/,UINT BaseIndex,UBOOL NewValue)
{
	UINT RegisterIndex = BaseIndex / NumBytesPerShaderRegister;
	BOOL Value = NewValue;
	Direct3DDevice->SetVertexShaderConstantB( RegisterIndex, &Value, 1 );
}

void FD3D9DynamicRHI::SetPixelShaderParameter(FPixelShaderRHIParamRef PixelShader,UINT /*BufferIndex*/,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT /*ParamIndex*/)
{
	Direct3DDevice->SetPixelShaderConstantF(
		BaseIndex / NumBytesPerShaderRegister,
		(FLOAT*)GetPaddedShaderParameterValue(NewValue,NumBytes),
		(NumBytes + NumBytesPerShaderRegister - 1) / NumBytesPerShaderRegister
		);
}

void FD3D9DynamicRHI::SetPixelShaderBoolParameter(FPixelShaderRHIParamRef PixelShader,UINT /*BufferIndex*/,UINT BaseIndex,UBOOL NewValue)
{
	UINT RegisterIndex = BaseIndex / NumBytesPerShaderRegister;
	BOOL Value = NewValue;
	Direct3DDevice->SetPixelShaderConstantB( RegisterIndex, &Value, 1 );
}

void FD3D9DynamicRHI::SetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	FD3D9DynamicRHI::SetVertexShaderParameter(VertexShaderRHI, BufferIndex, BaseIndex, NumBytes, NewValue, ParamIndex);
}

void FD3D9DynamicRHI::SetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	FD3D9DynamicRHI::SetPixelShaderParameter(PixelShaderRHI, BufferIndex, BaseIndex, NumBytes, NewValue, ParamIndex);
}

/**
 * Set engine shader parameters for the view.
 * @param View					The current view
 */
void FD3D9DynamicRHI::SetViewParameters( const FSceneView& View )
{
	FD3D9DynamicRHI::SetViewParametersWithOverrides(View, View.TranslatedViewProjectionMatrix, View.DiffuseOverrideParameter, View.SpecularOverrideParameter);
}

/**
 * Set engine shader parameters for the view.
 * @param View					The current view
 * @param ViewProjectionMatrix	Matrix that transforms from world space to projection space for the view
 * @param DiffuseOverride		Material diffuse input override
 * @param SpecularOverride		Material specular input override
 */
void FD3D9DynamicRHI::SetViewParametersWithOverrides( const FSceneView& View, const FMatrix& ViewProjectionMatrix, const FVector4& DiffuseOverride, const FVector4& SpecularOverride )
{
	const FVector4 TranslatedViewOrigin = View.ViewOrigin + FVector4(View.PreViewTranslation,0);
	const FVector4 PreViewTranslation = View.PreViewTranslation;
	const FVector4 NvStereoEnabled = nv::stereo::IsStereoEnabled() ? FVector4(1, 1, 1, 1) : FVector4(0, 0, 0, 0);
	const FVector4 ScreenAndTexelSize = FVector4(View.SizeX, View.SizeY, 1.0f / (FLOAT)View.RenderTargetSizeX, 1.0f / (FLOAT)View.RenderTargetSizeY);

	Direct3DDevice->SetVertexShaderConstantF( VSR_ViewProjMatrix, (const FLOAT*) &ViewProjectionMatrix, 4 );
	Direct3DDevice->SetVertexShaderConstantF( VSR_ViewOrigin, (const FLOAT*) &TranslatedViewOrigin, 1 );
	Direct3DDevice->SetVertexShaderConstantF( VSR_PreViewTranslation, (const FLOAT*) &PreViewTranslation, 1 );
	Direct3DDevice->SetPixelShaderConstantF( PSR_MinZ_MaxZ_Ratio, (const FLOAT*) &View.InvDeviceZToWorldZTransform, 1 );
	Direct3DDevice->SetPixelShaderConstantF( PSR_ScreenPositionScaleBias, (const FLOAT*) &View.ScreenPositionScaleBias, 1 );
	Direct3DDevice->SetPixelShaderConstantF( PSR_NvStereoEnabled, (const FLOAT*) &NvStereoEnabled, 1 );
	Direct3DDevice->SetPixelShaderConstantF( PSR_DiffuseOverride, (const FLOAT*) &DiffuseOverride, 1 );
	Direct3DDevice->SetPixelShaderConstantF( PSR_SpecularOverride, (const FLOAT*) &SpecularOverride, 1 );
	Direct3DDevice->SetPixelShaderConstantF( PSR_ViewOrigin, (const FLOAT*) &View.ViewOrigin, 1 );
	Direct3DDevice->SetPixelShaderConstantF( PSR_ScreenAndTexelSize, (const FLOAT*) &ScreenAndTexelSize, 1 );
    // WITH_REALD BEGIN
#if WITH_REALD
	const FVector4 RealDBAVars1       = View.bRealDStereoEnabled ? View.RealDCoefficients : FVector4(100, 0, -100, 0);
	Direct3DDevice->SetVertexShaderConstantF( VSR_RealDCoefficients1, (const FLOAT*) &RealDBAVars1, 1 );
	 
	Direct3DDevice->SetPixelShaderConstantF ( PSR_RealDCoefficients1, (const FLOAT*) &RealDBAVars1, 1 );
#else
	const FVector4 RealDBAVars1       = FVector4(100, 0, -100, 0);
#endif
    // WITH_REALD END
}

/**
 * Not used on PC
 */
void FD3D9DynamicRHI::SetViewPixelParameters(const FSceneView* View,FPixelShaderRHIParamRef PixelShader,const class FShaderParameter* SceneDepthCalcParameter,const class FShaderParameter* ScreenPositionScaleBiasParameter,const class FShaderParameter* ScreenAndTexelSizeParameter)
{
}
void FD3D9DynamicRHI::SetRenderTargetBias( FLOAT ColorBias )
{
}
void FD3D9DynamicRHI::SetShaderRegisterAllocation(UINT NumVertexShaderRegisters, UINT NumPixelShaderRegisters)
{
}
void FD3D9DynamicRHI::ReduceTextureCachePenalty( FPixelShaderRHIParamRef PixelShader )
{
}

// Output state.
void FD3D9DynamicRHI::SetDepthState(FDepthStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_D3D9RESOURCE(DepthState,NewState);

	Direct3DDevice->SetRenderState(D3DRS_ZENABLE,NewState->bZEnable);
	Direct3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,NewState->bZWriteEnable);
	Direct3DDevice->SetRenderState(D3DRS_ZFUNC,NewState->ZFunc);
}
void FD3D9DynamicRHI::SetStencilState(FStencilStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_D3D9RESOURCE(StencilState,NewState);

	Direct3DDevice->SetRenderState(D3DRS_STENCILENABLE,NewState->bStencilEnable);
	Direct3DDevice->SetRenderState(D3DRS_STENCILFUNC,NewState->StencilFunc);
	Direct3DDevice->SetRenderState(D3DRS_STENCILFAIL,NewState->StencilFail);
	Direct3DDevice->SetRenderState(D3DRS_STENCILZFAIL,NewState->StencilZFail);
	Direct3DDevice->SetRenderState(D3DRS_STENCILPASS,NewState->StencilPass);
	Direct3DDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE,NewState->bTwoSidedStencilMode);
	Direct3DDevice->SetRenderState(D3DRS_CCW_STENCILFUNC,NewState->CCWStencilFunc);
	Direct3DDevice->SetRenderState(D3DRS_CCW_STENCILFAIL,NewState->CCWStencilFail);
	Direct3DDevice->SetRenderState(D3DRS_CCW_STENCILZFAIL,NewState->CCWStencilZFail);
	Direct3DDevice->SetRenderState(D3DRS_CCW_STENCILPASS,NewState->CCWStencilPass);
	Direct3DDevice->SetRenderState(D3DRS_STENCILMASK,NewState->StencilReadMask);
	Direct3DDevice->SetRenderState(D3DRS_STENCILWRITEMASK,NewState->StencilWriteMask);
	Direct3DDevice->SetRenderState(D3DRS_STENCILREF,NewState->StencilRef);
}

void FD3D9DynamicRHI::SetBlendState(FBlendStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_D3D9RESOURCE(BlendState,NewState);

	Direct3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,NewState->bAlphaBlendEnable);
	Direct3DDevice->SetRenderState(D3DRS_BLENDOP,NewState->ColorBlendOperation);
	Direct3DDevice->SetRenderState(D3DRS_SRCBLEND,NewState->ColorSourceBlendFactor);
	Direct3DDevice->SetRenderState(D3DRS_DESTBLEND,NewState->ColorDestBlendFactor);
	Direct3DDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,NewState->bSeparateAlphaBlendEnable);
	Direct3DDevice->SetRenderState(D3DRS_BLENDOPALPHA,NewState->AlphaBlendOperation);
	Direct3DDevice->SetRenderState(D3DRS_SRCBLENDALPHA,NewState->AlphaSourceBlendFactor);
	Direct3DDevice->SetRenderState(D3DRS_DESTBLENDALPHA,NewState->AlphaDestBlendFactor);
	Direct3DDevice->SetRenderState(D3DRS_ALPHATESTENABLE,NewState->bAlphaTestEnable);
	Direct3DDevice->SetRenderState(D3DRS_ALPHAFUNC,NewState->AlphaFunc);
	Direct3DDevice->SetRenderState(D3DRS_ALPHAREF,NewState->AlphaRef);
	Direct3DDevice->SetRenderState(D3DRS_BLENDFACTOR,NewState->BlendFactor);
}

void FD3D9DynamicRHI::SetMRTBlendState(FBlendStateRHIParamRef NewStateRHI, UINT TargetIndex)
{
	//@todo: MRT support for D3D9
	check(0);
}

void FD3D9DynamicRHI::SetRenderTarget(FSurfaceRHIParamRef NewRenderTargetRHI, FSurfaceRHIParamRef NewDepthStencilTargetRHI)
{
	DYNAMIC_CAST_D3D9RESOURCE(Surface,NewRenderTarget);
	DYNAMIC_CAST_D3D9RESOURCE(Surface,NewDepthStencilTarget);

	// Reset all texture references, to ensure a reference to this render target doesn't remain set.
	UnsetPSTextures();
	UnsetVSTextures();

	if(!NewRenderTarget)
	{
		// 1. If we're setting a NULL color buffer, we must also set a NULL depth buffer.
		// 2. If we're setting a NULL color buffer, we're going to use the back buffer instead (D3D shortcoming).
		check( BackBuffer );
		Direct3DDevice->SetRenderTarget(0,*BackBuffer);
	}
	else
	{
		Direct3DDevice->SetRenderTarget(0,*NewRenderTarget);
	}

	Direct3DDevice->SetDepthStencilSurface((NewDepthStencilTarget ? (IDirect3DSurface9*)*NewDepthStencilTarget : NULL));

	// Detect when the back buffer is being set, and set the correct viewport.
	if( DrawingViewport && (!NewRenderTarget || NewRenderTarget == BackBuffer) )
	{
		D3DVIEWPORT9 D3DViewport = { 0, 0, DrawingViewport->GetSizeX(), DrawingViewport->GetSizeY(), 0.0f, 1.0f };
		Direct3DDevice->SetViewport(&D3DViewport);
	}
}
void FD3D9DynamicRHI::SetMRTRenderTarget(FSurfaceRHIParamRef NewRenderTargetRHI, UINT TargetIndex)
{
	DYNAMIC_CAST_D3D9RESOURCE(Surface,NewRenderTarget);

	// Reset all texture references, to ensure a reference to this render target doesn't remain set.
	UnsetPSTextures();
	UnsetVSTextures();

	Direct3DDevice->SetRenderTarget(TargetIndex,NewRenderTarget ? *NewRenderTarget : (IDirect3DSurface9*)NULL);
}
void FD3D9DynamicRHI::SetColorWriteEnable(UBOOL bEnable)
{
	DWORD EnabledStateValue = D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED;
	Direct3DDevice->SetRenderState(D3DRS_COLORWRITEENABLE,bEnable ? EnabledStateValue : 0);
}

// Map the render target index to the appropriate D3DRS_COLORWRITEENABLEx state
static const D3DRENDERSTATETYPE MRTColorWriteEnableStates[4] = {D3DRS_COLORWRITEENABLE,D3DRS_COLORWRITEENABLE1,D3DRS_COLORWRITEENABLE2,D3DRS_COLORWRITEENABLE3};

void FD3D9DynamicRHI::SetMRTColorWriteEnable(UBOOL bEnable, UINT TargetIndex)
{
	DWORD EnabledStateValue = D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED;
	Direct3DDevice->SetRenderState(MRTColorWriteEnableStates[TargetIndex],bEnable ? EnabledStateValue : 0);
}
void FD3D9DynamicRHI::SetColorWriteMask(UINT ColorWriteMask)
{
	DWORD EnabledStateValue;
	EnabledStateValue  = (ColorWriteMask & CW_RED) ? D3DCOLORWRITEENABLE_RED : 0;
	EnabledStateValue |= (ColorWriteMask & CW_GREEN) ? D3DCOLORWRITEENABLE_GREEN : 0;
	EnabledStateValue |= (ColorWriteMask & CW_BLUE) ? D3DCOLORWRITEENABLE_BLUE : 0;
	EnabledStateValue |= (ColorWriteMask & CW_ALPHA) ? D3DCOLORWRITEENABLE_ALPHA : 0;
	Direct3DDevice->SetRenderState( D3DRS_COLORWRITEENABLE, EnabledStateValue );
}
void FD3D9DynamicRHI::SetMRTColorWriteMask(UINT ColorWriteMask, UINT TargetIndex)
{
	DWORD EnabledStateValue;
	EnabledStateValue  = (ColorWriteMask & CW_RED) ? D3DCOLORWRITEENABLE_RED : 0;
	EnabledStateValue |= (ColorWriteMask & CW_GREEN) ? D3DCOLORWRITEENABLE_GREEN : 0;
	EnabledStateValue |= (ColorWriteMask & CW_BLUE) ? D3DCOLORWRITEENABLE_BLUE : 0;
	EnabledStateValue |= (ColorWriteMask & CW_ALPHA) ? D3DCOLORWRITEENABLE_ALPHA : 0;
	Direct3DDevice->SetRenderState( MRTColorWriteEnableStates[TargetIndex], EnabledStateValue );
}

// Not supported
void FD3D9DynamicRHI::BeginHiStencilRecord(UBOOL bCompareFunctionEqual, UINT RefValue) { }
void FD3D9DynamicRHI::BeginHiStencilPlayback(UBOOL bFlush) { }
void FD3D9DynamicRHI::EndHiStencil() { }

// Occlusion queries.
void FD3D9DynamicRHI::BeginOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQueryRHI)
{
	if ( !bDeviceLost )
	{
		DYNAMIC_CAST_D3D9RESOURCE(OcclusionQuery,OcclusionQuery);
		HRESULT D3DResult = OcclusionQuery->Resource->Issue(D3DISSUE_BEGIN);
	}
}
void FD3D9DynamicRHI::EndOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQueryRHI)
{
	if ( !bDeviceLost )
	{
		DYNAMIC_CAST_D3D9RESOURCE(OcclusionQuery,OcclusionQuery);
		HRESULT D3DResult = OcclusionQuery->Resource->Issue(D3DISSUE_END);
	}
}

// Primitive drawing.

static D3DPRIMITIVETYPE GetD3D9PrimitiveType(UINT PrimitiveType)
{
	switch(PrimitiveType)
	{
	case PT_TriangleList: return D3DPT_TRIANGLELIST;
	case PT_TriangleStrip: return D3DPT_TRIANGLESTRIP;
	case PT_LineList: return D3DPT_LINELIST;
	default: appErrorf(TEXT("Unknown primitive type: %u"),PrimitiveType);
	};
	return D3DPT_TRIANGLELIST;
}

void FD3D9DynamicRHI::DrawPrimitive(UINT PrimitiveType,UINT BaseVertexIndex,UINT NumPrimitives)
{
	checkSlow( NumPrimitives > 0 );

	INC_DWORD_STAT(STAT_D3D9DrawPrimitiveCalls);
	INC_DWORD_STAT_BY(STAT_D3D9Triangles,(DWORD)(PrimitiveType != PT_LineList ? NumPrimitives : 0));
	INC_DWORD_STAT_BY(STAT_D3D9Lines,(DWORD)(PrimitiveType == PT_LineList ? NumPrimitives : 0));

	if (bTrackingEvents && CurrentEventNode)
	{
		CurrentEventNode->NumDraws++;
		CurrentEventNode->NumPrimitives += NumPrimitives;
	}

	if ( !IsDeviceLost() )
	{
		if ( NumPrimitives > 0 )
		{
		    Direct3DDevice->DrawPrimitive(
			    GetD3D9PrimitiveType(PrimitiveType),
			    BaseVertexIndex,
			    NumPrimitives
			    );
		}
		check(UpdateStreamForInstancingMask == 0);
		check(PendingNumInstances < 2);
	}
}

void FD3D9DynamicRHI::DrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI,UINT PrimitiveType,INT BaseVertexIndex,UINT MinIndex,UINT NumVertices,UINT StartIndex,UINT NumPrimitives)
{
	DYNAMIC_CAST_D3D9RESOURCE(IndexBuffer,IndexBuffer);

	checkSlow( NumPrimitives > 0 );

	INC_DWORD_STAT(STAT_D3D9DrawPrimitiveCalls);
	INC_DWORD_STAT_BY(STAT_D3D9Triangles,(DWORD)(PrimitiveType != PT_LineList ? NumPrimitives : 0));
	INC_DWORD_STAT_BY(STAT_D3D9Lines,(DWORD)(PrimitiveType == PT_LineList ? NumPrimitives : 0));

	if (bTrackingEvents && CurrentEventNode)
	{
		CurrentEventNode->NumDraws++;
		CurrentEventNode->NumPrimitives += NumPrimitives;
	}

	if ( !IsDeviceLost() && IndexBuffer )
	{
		check(IndexBuffer);
		Direct3DDevice->SetIndices(IndexBuffer);
		if ( NumPrimitives > 0 )
		{
		    Direct3DDevice->DrawIndexedPrimitive(
			    GetD3D9PrimitiveType(PrimitiveType),
			    BaseVertexIndex,
			    MinIndex,
			    NumVertices,
			    StartIndex,
			    NumPrimitives
			    );
		}

		if (PendingNumInstances > 1 && UpdateStreamForInstancingMask)
		{
			for (UINT Instance = 1; Instance < PendingNumInstances; Instance++)
			{
				// Set the instance-indexed vertex streams with a base address of the current instance.
				UINT InstancingMask = UpdateStreamForInstancingMask;
				for (UINT StreamIndex = 0; StreamIndex < NumVertexStreams && InstancingMask; StreamIndex++)
				{
					if (InstancingMask & 1)
					{
						FD3D9VertexBuffer* VertexBuffer = (FD3D9VertexBuffer*)PendingStreams[StreamIndex].VertexBuffer;
						Direct3DDevice->SetStreamSource(
							StreamIndex,
							VertexBuffer,
							PendingStreams[StreamIndex].Stride * Instance,
                            PendingStreams[StreamIndex].Offset
							);
					}
					InstancingMask >>= 1;
				}

				// Draw this instance.
		        if ( NumPrimitives > 0 )
		        {
				    Direct3DDevice->DrawIndexedPrimitive(
					    GetD3D9PrimitiveType(PrimitiveType),
					    BaseVertexIndex,
					    MinIndex,
					    NumVertices,
					    StartIndex,
					    NumPrimitives
					    );
				}
			}

			// Reset the instanced vertex state.
			UINT InstancingMask = UpdateStreamForInstancingMask;
			for (UINT StreamIndex = 0; StreamIndex < NumVertexStreams && InstancingMask; StreamIndex++)
			{
				if (InstancingMask & 1)
				{
					PendingStreams[StreamIndex].VertexBuffer = 0;
				}
				InstancingMask >>= 1;
			}
			UpdateStreamForInstancingMask = 0;
			PendingNumInstances = 1;
		}
	}
}

void FD3D9DynamicRHI::DrawIndexedPrimitive_PreVertexShaderCulling(FIndexBufferRHIParamRef IndexBuffer,UINT PrimitiveType,INT BaseVertexIndex,UINT MinIndex,UINT NumVertices,UINT StartIndex,UINT NumPrimitives,const FMatrix& LocalToWorld,const void *PlatformMeshData)
{
	// On PC, don't use pre-vertex-shader culling.
	DrawIndexedPrimitive(IndexBuffer,PrimitiveType,BaseVertexIndex,MinIndex,NumVertices,StartIndex,NumPrimitives);
}

/**
 * Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawPrimitiveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param NumVertices The number of vertices to be written
 * @param VertexDataStride Size of each vertex 
 * @param OutVertexData Reference to the allocated vertex memory
 */
void FD3D9DynamicRHI::BeginDrawPrimitiveUP(UINT PrimitiveType, UINT NumPrimitives, UINT NumVertices, UINT VertexDataStride, void*& OutVertexData)
{
	check(!PendingBegunDrawPrimitiveUP);

	if((UINT)PendingDrawPrimitiveUPVertexData.Num() < NumVertices * VertexDataStride)
	{
		PendingDrawPrimitiveUPVertexData.Empty(NumVertices * VertexDataStride);
		PendingDrawPrimitiveUPVertexData.Add(NumVertices * VertexDataStride - PendingDrawPrimitiveUPVertexData.Num());
	}
	OutVertexData = &PendingDrawPrimitiveUPVertexData(0);

	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingNumVertices = NumVertices;
	PendingVertexDataStride = VertexDataStride;
	PendingBegunDrawPrimitiveUP = TRUE;
}

/**
 * Draw a primitive using the vertex data populated since RHIBeginDrawPrimitiveUP and clean up any memory as needed
 */
void FD3D9DynamicRHI::EndDrawPrimitiveUP()
{
	check(PendingBegunDrawPrimitiveUP);
	PendingBegunDrawPrimitiveUP = FALSE;

	if (bTrackingEvents && CurrentEventNode)
	{
		CurrentEventNode->NumDraws++;
		CurrentEventNode->NumPrimitives += PendingNumPrimitives;
	}

	// for now (while RHIDrawPrimitiveUP still exists), just call it because it does the same work we need here
	RHIDrawPrimitiveUP(PendingPrimitiveType, PendingNumPrimitives, &PendingDrawPrimitiveUPVertexData(0), PendingVertexDataStride);
}
/**
 * Draw a primitive using the vertices passed in
 * VertexData is NOT created by BeginDrawPrimitiveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param VertexData A reference to memory preallocate in RHIBeginDrawPrimitiveUP
 * @param VertexDataStride The size of one vertex
 */
void FD3D9DynamicRHI::DrawPrimitiveUP(UINT PrimitiveType, UINT NumPrimitives, const void* VertexData,UINT VertexDataStride)
{
	checkSlow( NumPrimitives > 0 );

	INC_DWORD_STAT(STAT_D3D9DrawPrimitiveCalls);
	INC_DWORD_STAT_BY(STAT_D3D9Triangles,(DWORD)(PrimitiveType != PT_LineList ? NumPrimitives : 0));
	INC_DWORD_STAT_BY(STAT_D3D9Lines,(DWORD)(PrimitiveType == PT_LineList ? NumPrimitives : 0));

	if (bTrackingEvents && CurrentEventNode)
	{
		CurrentEventNode->NumDraws++;
		CurrentEventNode->NumPrimitives += NumPrimitives;
	}

	if ( !IsDeviceLost() )
	{
		// Reset vertex stream 0's frequency.
		Direct3DDevice->SetStreamSourceFreq(0,1);

		if ( NumPrimitives > 0 )
		{
			Direct3DDevice->DrawPrimitiveUP(
				GetD3D9PrimitiveType(PrimitiveType),
				NumPrimitives,
				VertexData,
				VertexDataStride
				);
		}
	}
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
void FD3D9DynamicRHI::BeginDrawIndexedPrimitiveUP(UINT PrimitiveType, UINT NumPrimitives, UINT NumVertices, UINT VertexDataStride, void*& OutVertexData, UINT MinVertexIndex, UINT NumIndices, UINT IndexDataStride, void*& OutIndexData)
{
	check(!PendingBegunDrawPrimitiveUP);

	if((UINT)PendingDrawPrimitiveUPVertexData.Num() < NumVertices * VertexDataStride)
	{
		PendingDrawPrimitiveUPVertexData.Empty(NumVertices * VertexDataStride);
		PendingDrawPrimitiveUPVertexData.Add(NumVertices * VertexDataStride - PendingDrawPrimitiveUPVertexData.Num());
	}
	OutVertexData = &PendingDrawPrimitiveUPVertexData(0);

	if((UINT)PendingDrawPrimitiveUPIndexData.Num() < NumIndices * IndexDataStride)
	{
		PendingDrawPrimitiveUPIndexData.Empty(NumIndices * IndexDataStride);
		PendingDrawPrimitiveUPIndexData.Add(NumIndices * IndexDataStride - PendingDrawPrimitiveUPIndexData.Num());
	}
	OutIndexData = &PendingDrawPrimitiveUPIndexData(0);

	check((sizeof(WORD) == IndexDataStride) || (sizeof(DWORD) == IndexDataStride));

	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingMinVertexIndex = MinVertexIndex;
	PendingIndexDataStride = IndexDataStride;

	PendingNumVertices = NumVertices;
	PendingVertexDataStride = VertexDataStride;
	
	PendingBegunDrawPrimitiveUP = TRUE;
}

/**
 * Draw a primitive using the vertex and index data populated since RHIBeginDrawIndexedPrimitiveUP and clean up any memory as needed
 */
void FD3D9DynamicRHI::EndDrawIndexedPrimitiveUP()
{
	check(PendingBegunDrawPrimitiveUP);
	PendingBegunDrawPrimitiveUP = FALSE;

	if (bTrackingEvents && CurrentEventNode)
	{
		CurrentEventNode->NumDraws++;
		CurrentEventNode->NumPrimitives += PendingNumPrimitives;
	}

	// for now (while RHIDrawPrimitiveUP still exists), just call it because it does the same work we need here
	RHIDrawIndexedPrimitiveUP(PendingPrimitiveType, PendingMinVertexIndex, PendingNumVertices, PendingNumPrimitives, &PendingDrawPrimitiveUPIndexData(0), PendingIndexDataStride, &PendingDrawPrimitiveUPVertexData(0), PendingVertexDataStride);
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
void FD3D9DynamicRHI::DrawIndexedPrimitiveUP(UINT PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT NumPrimitives, const void* IndexData, UINT IndexDataStride, const void* VertexData, UINT VertexDataStride)
{
	checkSlow( NumPrimitives > 0 );

	INC_DWORD_STAT(STAT_D3D9DrawPrimitiveCalls);
	INC_DWORD_STAT_BY(STAT_D3D9Triangles,(DWORD)(PrimitiveType != PT_LineList ? NumPrimitives : 0));
	INC_DWORD_STAT_BY(STAT_D3D9Lines,(DWORD)(PrimitiveType == PT_LineList ? NumPrimitives : 0));

	if (bTrackingEvents && CurrentEventNode)
	{
		CurrentEventNode->NumDraws++;
		CurrentEventNode->NumPrimitives += NumPrimitives;
	}

	if ( !IsDeviceLost() )
	{
		// Reset vertex stream 0's frequency.
		Direct3DDevice->SetStreamSourceFreq(0,1);

		if ( NumPrimitives > 0 )
		{
			Direct3DDevice->DrawIndexedPrimitiveUP(
				GetD3D9PrimitiveType(PrimitiveType),
				MinVertexIndex,
				NumVertices,
				NumPrimitives,
				IndexData,
				IndexDataStride == sizeof(WORD) ? D3DFMT_INDEX16 : D3DFMT_INDEX32,
				VertexData,
				VertexDataStride
				);
		}
	}
}

/**
 * Draw a sprite particle emitter.
 *
 * @param Mesh The mesh element containing the data for rendering the sprite particles
 */
void FD3D9DynamicRHI::DrawSpriteParticles(const FMeshBatch& Mesh)
{
	check(Mesh.DynamicVertexData);
	FDynamicSpriteEmitterData* SpriteData = (FDynamicSpriteEmitterData*)(Mesh.DynamicVertexData);

	// Sort the particles if required
	INT ParticleCount = SpriteData->Source.ActiveParticleCount;

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
	RHIBeginDrawIndexedPrimitiveUP(PT_TriangleList, 
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
void FD3D9DynamicRHI::DrawSubUVParticles(const FMeshBatch& Mesh)
{
	check(Mesh.DynamicVertexData);
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
	RHIBeginDrawIndexedPrimitiveUP(PT_TriangleList, 
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
void FD3D9DynamicRHI::DrawPointSpriteParticles(const FMeshBatch& Mesh) 
{
	// Not implemented yet!
}


// Raster operations.
void FD3D9DynamicRHI::Clear(UBOOL bClearColor,const FLinearColor& Color,UBOOL bClearDepth,FLOAT Depth,UBOOL bClearStencil,DWORD Stencil)
{
	// Determine the clear flags.
	DWORD Flags = 0;
	if(bClearColor)
	{
		Flags |= D3DCLEAR_TARGET;
	}
	if(bClearDepth)
	{
		Flags |= D3DCLEAR_ZBUFFER;
	}
	if(bClearStencil)
	{
		Flags |= D3DCLEAR_STENCIL;
	}

	if (bTrackingEvents && CurrentEventNode)
	{
		CurrentEventNode->NumDraws++;
	}

	// Clear the render target/depth-stencil surfaces based on the flags.
	FColor QuantizedColor(Color.Quantize());
	Direct3DDevice->Clear(0,NULL,Flags,D3DCOLOR_RGBA(QuantizedColor.R,QuantizedColor.G,QuantizedColor.B,QuantizedColor.A),Depth,Stencil);
}

// Functions to yield and regain rendering control from D3D

void FD3D9DynamicRHI::SuspendRendering()
{
	// Not supported
}

void FD3D9DynamicRHI::ResumeRendering()
{
	// Not supported
}

UBOOL FD3D9DynamicRHI::IsRenderingSuspended()
{
	// Not supported
	return FALSE;
}

// Kick the rendering commands that are currently queued up in the GPU command buffer.
void FD3D9DynamicRHI::KickCommandBuffer()
{
	// Not really supported
}

// Blocks the CPU until the GPU catches up and goes idle.
void FD3D9DynamicRHI::BlockUntilGPUIdle()
{
	// Not really supported
}

/*
 * Returns the total GPU time taken to render the last frame. Same metric as appCycles().
 */
DWORD FD3D9DynamicRHI::GetGPUFrameCycles()
{
	return GGPUFrameTime;
}

/*
 * Returns an approximation of the available memory that textures can use, which is video + AGP where applicable, rounded to the nearest MB, in MB.
 */
DWORD FD3D9DynamicRHI::GetAvailableTextureMemory()
{
	//apparently GetAvailableTextureMem() returns available bytes (the docs don't say) rounded to the nearest MB.
	return Direct3DDevice->GetAvailableTextureMem() / 1048576;
}

// not used on PC
void FD3D9DynamicRHI::RestoreColorDepth(FTexture2DRHIParamRef ColorTexture, FTexture2DRHIParamRef DepthTexture)
{
}
void FD3D9DynamicRHI::SetTessellationMode( ETessellationMode TessellationMode, FLOAT MinTessellation, FLOAT MaxTessellation )
{
}

void FD3D9DynamicRHI::UpdateStereoFixTexture(FTexture2DRHIParamRef TextureRHI)
{
	if (!nv::stereo::IsStereoEnabled()) 
	{
		return;
	}

    if (!StereoUpdater)
	{
        StereoUpdater = new nv::stereo::UE3StereoD3D9(false);
	}

    DYNAMIC_CAST_D3D9RESOURCE(Texture2D,Texture);
    StereoUpdater->UpdateStereoTexture(Direct3DDevice, *Texture, bDeviceLost != 0);
}



// Tessellation is not supported on Dx9
FHullShaderRHIRef FD3D9DynamicRHI::CreateHullShader(const TArray<BYTE>& Code) { appErrorf(TEXT("D3D9 Render path does not support Hull shaders!")); return NULL; }
FDomainShaderRHIRef FD3D9DynamicRHI::CreateDomainShader(const TArray<BYTE>& Code) { appErrorf(TEXT("D3D9 Render path does not support Domain shaders!")); return NULL; }
FBoundShaderStateRHIRef FD3D9DynamicRHI::CreateBoundShaderStateD3D11(FVertexDeclarationRHIParamRef VertexDeclaration, DWORD *StreamStrides, FVertexShaderRHIParamRef VertexShader, FHullShaderRHIParamRef HullShader, FDomainShaderRHIParamRef DomainShader, FPixelShaderRHIParamRef PixelShader, FGeometryShaderRHIParamRef GeometryShader, EMobileGlobalShaderType MobileGlobalShaderType)
{ 
	checkSlow(!HullShader);
	checkSlow(!DomainShader);
	checkSlow(!GeometryShader);
	
	return CreateBoundShaderState(VertexDeclaration, StreamStrides, VertexShader, PixelShader, MobileGlobalShaderType);
}

void FD3D9DynamicRHI::SetSamplerState(FDomainShaderRHIParamRef DomainShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ appErrorf(TEXT("D3D9 Render path does not support Hull or Domain shaders!")); }
void FD3D9DynamicRHI::SetSamplerState(FHullShaderRHIParamRef HullShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ appErrorf(TEXT("D3D9 Render path does not support Hull or Domain shaders!")); }
void FD3D9DynamicRHI::SetShaderBoolParameter(FHullShaderRHIParamRef HullShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue)
{ appErrorf(TEXT("D3D9 Render path does not support Hull or Domain shaders!")); }
void FD3D9DynamicRHI::SetShaderBoolParameter(FDomainShaderRHIParamRef DomainShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue)
{ appErrorf(TEXT("D3D9 Render path does not support Hull or Domain shaders!")); }
void FD3D9DynamicRHI::SetShaderParameter(FHullShaderRHIParamRef HullShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("D3D9 Render path does not support Hull or Domain shaders!")); }
void FD3D9DynamicRHI::SetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("D3D9 Render path does not support Hull or Domain shaders!")); }
void FD3D9DynamicRHI::SetDomainShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("D3D9 Render path does not support Hull or Domain shaders!")); }
void FD3D9DynamicRHI::SetHullShaderParameter(FHullShaderRHIParamRef HullShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("D3D9 Render path does not support Hull or Domain shaders!")); }
void FD3D9DynamicRHI::SetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("D3D9 Render path does not support Geometry shaders!")); }
FGeometryShaderRHIRef FD3D9DynamicRHI::CreateGeometryShader(const TArray<BYTE>& Code)
{ appErrorf(TEXT("D3D9 Render path does not support Geometry shaders!")); return NULL; }
void FD3D9DynamicRHI::SetSamplerState(FGeometryShaderRHIParamRef GeometryShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ appErrorf(TEXT("D3D9 Render path does not support Geometry shaders!")); }
void FD3D9DynamicRHI::SetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("D3D9 Render path does not support Compute shaders!")); }
FComputeShaderRHIRef FD3D9DynamicRHI::CreateComputeShader(const TArray<BYTE>& Code)
{ appErrorf(TEXT("D3D9 Render path does not support Compute shaders!")); return NULL; }
void FD3D9DynamicRHI::DispatchComputeShader(FComputeShaderRHIParamRef ComputeShader, UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{ appErrorf(TEXT("D3D9 Render path does not support Compute shaders!")); }
void FD3D9DynamicRHI::SetSamplerState(FComputeShaderRHIParamRef ComputeShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ appErrorf(TEXT("D3D9 Render path does not support Compute shaders!")); }
void FD3D9DynamicRHI::SetMultipleViewports(UINT Count, FViewPortBounds* Data)
{ appErrorf(TEXT("D3D9 Render path does not support multiple Viewports!")); }
FBlendStateRHIRef FD3D9DynamicRHI::CreateMRTBlendState(const FMRTBlendStateInitializerRHI&)
{ appErrorf(TEXT("D3D9 Render path does not support CreateMRTBlendState!")); return NULL; }

