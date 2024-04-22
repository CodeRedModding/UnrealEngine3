/*=============================================================================
	OpenGLCommands.cpp: OpenGL RHI commands implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

// Vertex state.
void FOpenGLDynamicRHI::SetStreamSource(UINT StreamIndex,FVertexBufferRHIParamRef VertexBufferRHI,UINT Stride,UINT Offset,UBOOL bUseInstanceIndex,UINT NumVerticesPerInstance,UINT NumInstances)
{
    DYNAMIC_CAST_OPENGLRESOURCE(VertexBuffer,VertexBuffer);
    PendingStreams[StreamIndex].VertexBuffer = VertexBuffer;
    PendingStreams[StreamIndex].Stride = Stride;
    PendingStreams[StreamIndex].Offset = Offset;
	if (bUseInstanceIndex || NumInstances > 1)
	{
		PendingStreams[StreamIndex].Frequency = bUseInstanceIndex ? 1 : 0;
	}
	else
	{
		PendingStreams[StreamIndex].Frequency = 0;
	}
	PendingNumInstances = NumInstances;
}

// Rasterizer state.
void FOpenGLDynamicRHI::SetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(RasterizerState,NewState);

	if (CachedState.RasterizerState.FillMode != NewState->FillMode)
	{
		glPolygonMode(GL_FRONT_AND_BACK, NewState->FillMode);
		CachedState.RasterizerState.FillMode = NewState->FillMode;
	}

	if (CachedState.RasterizerState.CullMode != NewState->CullMode)
	{
		if (NewState->CullMode != GL_NONE)
		{
			glEnable(GL_CULL_FACE);
			glCullFace(NewState->CullMode);
		}
		else
		{
			glDisable(GL_CULL_FACE);
		}
		CachedState.RasterizerState.CullMode = NewState->CullMode;
	}

	// Convert our platform independent depth bias into an OpenGL depth bias.
	const FLOAT BiasScale = FLOAT((1<<24)-1);
	extern FLOAT GDepthBiasOffset;
	FLOAT DepthBias = (NewState->DepthBias + GDepthBiasOffset) * BiasScale;
	if (CachedState.RasterizerState.DepthBias != DepthBias
		|| CachedState.RasterizerState.SlopeScaleDepthBias != NewState->SlopeScaleDepthBias)
	{
		if ((DepthBias == 0.0f) && (NewState->SlopeScaleDepthBias == 0.0f))
		{
			if (CachedState.RasterizerState.DepthBias != 0.0f || CachedState.RasterizerState.SlopeScaleDepthBias != 0.0f)
			{
				glDisable(GL_POLYGON_OFFSET_FILL);
				glDisable(GL_POLYGON_OFFSET_LINE);
				glDisable(GL_POLYGON_OFFSET_POINT);
			}
		}
		else
		{
			if (CachedState.RasterizerState.DepthBias == 0.0f && CachedState.RasterizerState.SlopeScaleDepthBias == 0.0f)
			{
				glEnable(GL_POLYGON_OFFSET_FILL);
				glEnable(GL_POLYGON_OFFSET_LINE);
				glEnable(GL_POLYGON_OFFSET_POINT);
			}
			glPolygonOffset(NewState->SlopeScaleDepthBias, DepthBias);
		}

		CachedState.RasterizerState.DepthBias = DepthBias;
		CachedState.RasterizerState.SlopeScaleDepthBias = NewState->SlopeScaleDepthBias;
	}
}

void FOpenGLDynamicRHI::InternalSetViewport(UINT MinX,UINT MinY,FLOAT MinZ,UINT MaxX,UINT MaxY,FLOAT MaxZ, UBOOL ForceVieportChange)
{
	if (ForceVieportChange || CachedState.Viewport.Min.X != MinX || CachedState.Viewport.Min.Y != MinY
		|| CachedState.Viewport.Max.X != MaxX || CachedState.Viewport.Max.Y != MaxY)
	{
		glViewport(MinX, CachedState.RenderTargetHeight - MaxY, MaxX - MinX, MaxY - MinY);
		CachedState.Viewport.Min.X = MinX;
		CachedState.Viewport.Min.Y = MinY;
		CachedState.Viewport.Max.X = MaxX;
		CachedState.Viewport.Max.Y = MaxY;
	}

	if (CachedState.MinZ != MinZ || CachedState.MaxZ != MaxZ)
	{
		glDepthRange(MinZ, MaxZ);
		CachedState.MinZ = MinZ;
		CachedState.MaxZ = MaxZ;
	}
}

void FOpenGLDynamicRHI::SetViewport(UINT MinX,UINT MinY,FLOAT MinZ,UINT MaxX,UINT MaxY,FLOAT MaxZ)
{
	InternalSetViewport( MinX, MinY, MinZ, MaxX, MaxY, MaxZ, FALSE );
}

void FOpenGLDynamicRHI::SetScissorRect(UBOOL bEnable,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY)
{
	if (CachedState.bScissorEnabled != bEnable)
	{
		if (bEnable)
		{
			glEnable(GL_SCISSOR_TEST);
		}
		else
		{
			glDisable(GL_SCISSOR_TEST);
		}
		CachedState.bScissorEnabled = bEnable;
	}

	if( CachedState.Scissor.Min.X != MinX ||
		CachedState.Scissor.Min.Y != MinY ||
		CachedState.Scissor.Max.X != MaxX ||
		CachedState.Scissor.Max.Y != MaxY )
	{
		glScissor(MinX, CachedState.RenderTargetHeight - MaxY, MaxX - MinX, MaxY - MinY);
		CachedState.Scissor.Min.X = MinX;
		CachedState.Scissor.Min.Y = MinY;
		CachedState.Scissor.Max.X = MaxX;
		CachedState.Scissor.Max.Y = MaxY;
	}
}

void FOpenGLDynamicRHI::UpdateScissorRectOnRenderTargetHeightChange()
{
	glScissor(CachedState.Scissor.Min.X, CachedState.RenderTargetHeight - CachedState.Scissor.Max.Y, CachedState.Scissor.Max.X - CachedState.Scissor.Min.X, CachedState.Scissor.Max.Y - CachedState.Scissor.Min.Y);
}

/**
 * Set depth bounds test state.
 * When enabled, incoming fragments are killed if the value already in the depth buffer is outside of [MinZ, MaxZ]
 */
void FOpenGLDynamicRHI::SetDepthBoundsTest( UBOOL bEnable, const FVector4 &ClipSpaceNearPos, const FVector4 &ClipSpaceFarPos)
{
	// not supported
}

/**
* Set bound shader state. This will set the vertex decl/shader, and pixel shader
* @param BoundShaderState - state resource
*/
void FOpenGLDynamicRHI::SetBoundShaderState( FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(BoundShaderState,BoundShaderState);
	PendingBoundShaderState = BoundShaderState;

	// @todo opengl : really should only discard the constants if the shader state has actually changed.
	bDiscardSharedConstants = TRUE;
}

void FOpenGLDynamicRHI::SetSamplerStateOnly(FPixelShaderRHIParamRef PixelShaderRHI,UINT /*SamplerIndex*/,FSamplerStateRHIParamRef NewStateRHI)
{
	// Not implemented yet
	check(0);
}

void FOpenGLDynamicRHI::SetTextureParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	// Not implemented yet
	check(0);
}

void FOpenGLDynamicRHI::SetSurfaceParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,FSurfaceRHIParamRef NewSurfaceRHI)
{
	// Not implemented yet
	check(0);
}

#if WITH_D3D11_TESSELLATION
void FOpenGLDynamicRHI::SetSurfaceParameter(FComputeShaderRHIParamRef ComputeShaderRHI,UINT TextureIndex,FSurfaceRHIParamRef NewSurfaceRHI)
{
	// Not supported by DX9
	check(0);
}

void FOpenGLDynamicRHI::SetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,UINT TextureIndex,FSurfaceRHIParamRef NewSurfaceRHI)
{
	// Not supported by DX9
	check(0);
}
#endif

/**
 * Sets sampler state.
 *
 * @param PixelShader	The pixelshader using the sampler for the next drawcalls.
 * @param TextureIndex	Used as sampler index on all platforms except D3D10, where it's the texture resource index.
 * @param SamplerIndex	Ignored on all platforms except D3D10, where it's the sampler index and OpenGL where it's texture unit.
 * @param BaseIndex		Ignoewd on all platforms except OpenGL, where it's used to calculate sampler index
 * @param MipBias		Mip bias to use for the texture
 * @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D10)
 * @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
 */
void FOpenGLDynamicRHI::SetSamplerState(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT LargestMip, FLOAT /*SmallestMip*/,UBOOL /*bForceLinearMinFilter*/)
{
	DYNAMIC_CAST_OPENGLRESOURCE(SamplerState,NewState);
	DYNAMIC_CAST_OPENGLRESOURCE(Texture,NewTexture);

	// Force linear mip-filter if MipBias has a fractional part.
	GLenum MinFilter = GL_LINEAR_MIPMAP_LINEAR;
	if (NewState->MipMapLODBias || appIsNearlyEqual(appTruncFloat(MipBias), MipBias))
	{
		MinFilter = NewState->MinFilter;
	}

	FLOAT ResultMip = (LargestMip < 0.0f) ? 0 : appTrunc(LargestMip);
	UBOOL bNeedsToChangeSamplerState = NewTexture->NeedsToChangeSamplerState(NewState, MinFilter, ResultMip);

	INT NewMipBias = NewState->MipMapLODBias ? NewState->MipMapLODBias : MipBias;
	UBOOL bNeedsToChangeLODBias = (CachedState.Textures[SamplerIndex].MipMapLODBias != NewMipBias);

	UBOOL bNeedsToChangeTexture = ((CachedState.Textures[SamplerIndex].Target != NewTexture->Target) || (CachedState.Textures[SamplerIndex].Resource != NewTexture->Resource));

	if( bNeedsToChangeSamplerState || bNeedsToChangeLODBias || bNeedsToChangeTexture)
	{
		CachedSetActiveTexture(GL_TEXTURE0 + SamplerIndex);

		if (bNeedsToChangeSamplerState || bNeedsToChangeTexture)
		{
			glBindTexture(NewTexture->Target, NewTexture->Resource);
			CachedState.Textures[SamplerIndex].Target = NewTexture->Target;
			CachedState.Textures[SamplerIndex].Resource = NewTexture->Resource;

			NewTexture->SetSamplerState(NewState, MinFilter, ResultMip);
		}

		if (bNeedsToChangeLODBias)
		{
			glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, NewMipBias);
			CachedState.Textures[SamplerIndex].MipMapLODBias = NewMipBias;
		}
	}
}

void FOpenGLDynamicRHI::SetSamplerState(FVertexShaderRHIParamRef VertexShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{
	// OGL set sampler state just passes through to the VTF call
	SetVertexTexture(SamplerIndex,NewTextureRHI);
}

void FOpenGLDynamicRHI::SetVertexTexture(UINT SamplerIndex,FTextureRHIParamRef NewTextureRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Texture,NewTexture);

	SamplerIndex = 15 - SamplerIndex;

	CachedSetActiveTexture(GL_TEXTURE0 + SamplerIndex);
	glBindTexture(NewTexture->Target, NewTexture->Resource);
	CachedState.Textures[SamplerIndex].Target = NewTexture->Target;
	CachedState.Textures[SamplerIndex].Resource = NewTexture->Resource;
}

void FOpenGLDynamicRHI::ResetTrackedPrimitive()
{
	// Not implemented yet
}

void FOpenGLDynamicRHI::CycleTrackedPrimitiveMode()
{
	// Not implemented yet
}

void FOpenGLDynamicRHI::IncrementTrackedPrimitive(const INT InDelta)
{
	// Not implemented yet
}

void FOpenGLDynamicRHI::ClearSamplerBias()
{
	// Not used
}

void FOpenGLDynamicRHI::SetVertexShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	VSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)NewValue,BaseIndex,NumBytes);
}

void FOpenGLDynamicRHI::SetVertexShaderFloatArray(FVertexShaderRHIParamRef VertexShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumValues,const FLOAT* FloatValues, INT ParamIndex)
{
	check(0);
	// @todo opengl - on D3D this function is more complicated for performance reasons. Does OpenGL need it as well?
	FOpenGLDynamicRHI::SetVertexShaderParameter(VertexShaderRHI, BufferIndex, BaseIndex, NumValues * sizeof(FLOAT), FloatValues, ParamIndex);
}

void FOpenGLDynamicRHI::SetVertexShaderBoolParameter(FVertexShaderRHIParamRef VertexShaderRHI,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue)
{
	VSConstantBuffers(VS_BOOL_CONSTANT_BUFFER)->UpdateConstant((const BYTE*)&NewValue,BaseIndex/4,sizeof(UINT));
}

void FOpenGLDynamicRHI::SetPixelShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	PSConstantBuffers(BufferIndex)->UpdateConstant((const BYTE*)NewValue,BaseIndex,NumBytes);
}

void FOpenGLDynamicRHI::SetPixelShaderBoolParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue)
{
	PSConstantBuffers(PS_BOOL_CONSTANT_BUFFER)->UpdateConstant((const BYTE*)&NewValue,BaseIndex/4,sizeof(UINT));
}

void FOpenGLDynamicRHI::SetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	FOpenGLDynamicRHI::SetVertexShaderParameter(VertexShaderRHI, BufferIndex, BaseIndex, NumBytes, NewValue, ParamIndex);
}

void FOpenGLDynamicRHI::SetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	FOpenGLDynamicRHI::SetPixelShaderParameter(PixelShaderRHI, BufferIndex, BaseIndex, NumBytes, NewValue, ParamIndex);
}

/**
 * Set engine shader parameters for the view.
 * @param View					The current view
 */
void FOpenGLDynamicRHI::SetViewParameters( const FSceneView& View )
{
	FOpenGLDynamicRHI::SetViewParametersWithOverrides(View, View.TranslatedViewProjectionMatrix, View.DiffuseOverrideParameter, View.SpecularOverrideParameter);
}

/**
 * Set engine shader parameters for the view.
 * @param View					The current view
 * @param ViewProjectionMatrix	Matrix that transforms from world space to projection space for the view
 * @param DiffuseOverride		Material diffuse input override
 * @param SpecularOverride		Material specular input override
 */
void FOpenGLDynamicRHI::SetViewParametersWithOverrides( const FSceneView& View, const FMatrix& ViewProjectionMatrix, const FVector4& DiffuseOverride, const FVector4& SpecularOverride )
{
	const FVector4 TranslatedViewOrigin = View.ViewOrigin + FVector4(View.PreViewTranslation,0);

	FOpenGLOffsetConstantBufferContentsVS VSCBContents;

	// OpenGL uses [-1..1] space to clip all coordinates, while D3D uses [0..1] for Z, so we need to modify the ViewProjectionMatrix to get the correct result
	FScaleMatrix ClipSpaceFixScale(FVector(1.0f, 1.0f, 2.0f));
	FTranslationMatrix ClipSpaceFixTranslate(FVector(0.0f, 0.0f, -1.0f));	
	VSCBContents.ViewProjectionMatrix = ViewProjectionMatrix * ClipSpaceFixScale * ClipSpaceFixTranslate;
	VSCBContents.ViewOrigin = TranslatedViewOrigin;
	VSCBContents.PreViewTranslation = View.PreViewTranslation;

	VSConstantBuffers(GLOBAL_CONSTANT_BUFFER)->UpdateConstant((BYTE*)&VSCBContents,VS_GLOBAL_CONSTANT_BASE_INDEX * sizeof(FVector4),sizeof(VSCBContents));

	FOpenGLOffsetConstantBufferContentsPS PSCBContents;
	PSCBContents.ScreenPositionScaleBias = View.ScreenPositionScaleBias;
	PSCBContents.MinZ_MaxZRatio = View.InvDeviceZToWorldZTransform;
	PSCBContents.DiffuseOverrideParameter = DiffuseOverride;
	PSCBContents.SpecularOverrideParameter = SpecularOverride;
	PSCBContents.NvStereoEnabled = FVector4(0.0f,0.0f,0.0f,0.0f); //nv::stereo::IsStereoEnabled() ? 1.0f : 0.0f; // @todo opengl
	PSCBContents.ViewOrigin = View.ViewOrigin;
	PSCBContents.ScreenAndTexelSize = FVector4(View.SizeX, View.SizeY, 1.0f / (FLOAT)View.RenderTargetSizeX, 1.0f / (FLOAT)View.RenderTargetSizeY);

	PSConstantBuffers(GLOBAL_CONSTANT_BUFFER)->UpdateConstant((BYTE*)&PSCBContents,PS_GLOBAL_CONSTANT_BASE_INDEX * sizeof(FVector4) + sizeof(FVector4),sizeof(PSCBContents));
}

void FOpenGLDynamicRHI::SetDepthState(FDepthStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(DepthState,NewState);

	if (CachedState.DepthState.bZEnable != NewState->bZEnable)
	{
		if (NewState->bZEnable)
		{
			glEnable(GL_DEPTH_TEST);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}

		CachedState.DepthState.bZEnable = NewState->bZEnable;
	}

	if (NewState->bZEnable && CachedState.DepthState.ZFunc != NewState->ZFunc)
	{
		glDepthFunc(NewState->ZFunc);
		CachedState.DepthState.ZFunc = NewState->ZFunc;
	}

	if (CachedState.DepthState.bZWriteEnable != NewState->bZWriteEnable)
	{
		glDepthMask(NewState->bZWriteEnable);
		CachedState.DepthState.bZWriteEnable = NewState->bZWriteEnable;
	}
}

void FOpenGLDynamicRHI::SetStencilState(FStencilStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(StencilState,NewState);

	if (CachedState.StencilState.bStencilEnable != NewState->bStencilEnable)
	{
		if (NewState->bStencilEnable)
		{
			glEnable(GL_STENCIL_TEST);
		}
		else
		{
			glDisable(GL_STENCIL_TEST);
		}
		CachedState.StencilState.bStencilEnable = NewState->bStencilEnable;
	}

	if (NewState->bStencilEnable)
	{
		if (NewState->bTwoSidedStencilMode)
		{
			if (CachedState.StencilState.StencilFunc != NewState->StencilFunc
				|| CachedState.StencilState.StencilRef != NewState->StencilRef
				|| CachedState.StencilState.StencilReadMask != NewState->StencilReadMask)
			{
				glStencilFuncSeparate(GL_BACK, NewState->StencilFunc, NewState->StencilRef, NewState->StencilReadMask);
				CachedState.StencilState.StencilFunc = NewState->StencilFunc;
				CachedState.StencilState.StencilRef = NewState->StencilRef;
				CachedState.StencilState.StencilReadMask = NewState->StencilReadMask;
			}

			if (CachedState.StencilState.StencilFail != NewState->StencilFail
				|| CachedState.StencilState.StencilZFail != NewState->StencilZFail
				|| CachedState.StencilState.StencilPass != NewState->StencilPass)
			{
				glStencilOpSeparate(GL_BACK, NewState->StencilFail, NewState->StencilZFail, NewState->StencilPass);
				CachedState.StencilState.StencilFail = NewState->StencilFail;
				CachedState.StencilState.StencilZFail = NewState->StencilZFail;
				CachedState.StencilState.StencilPass = NewState->StencilPass;
			}

			if (CachedState.StencilState.CCWStencilFunc != NewState->CCWStencilFunc
				|| CachedState.StencilState.StencilRef != NewState->StencilRef
				|| CachedState.StencilState.StencilReadMask != NewState->StencilReadMask)
			{
				glStencilFuncSeparate(GL_FRONT, NewState->CCWStencilFunc, NewState->StencilRef, NewState->StencilReadMask);
				CachedState.StencilState.CCWStencilFunc = NewState->CCWStencilFunc;
			}

			if (CachedState.StencilState.CCWStencilFail != NewState->CCWStencilFail
				|| CachedState.StencilState.CCWStencilZFail != NewState->CCWStencilZFail
				|| CachedState.StencilState.CCWStencilPass != NewState->CCWStencilPass)
			{
				glStencilOpSeparate(GL_FRONT, NewState->CCWStencilFail, NewState->CCWStencilZFail, NewState->CCWStencilPass);
				CachedState.StencilState.CCWStencilFail = NewState->CCWStencilFail;
				CachedState.StencilState.CCWStencilZFail = NewState->CCWStencilZFail;
				CachedState.StencilState.CCWStencilPass = NewState->CCWStencilPass;
			}
		}
		else
		{
			if (CachedState.StencilState.StencilFunc != NewState->StencilFunc
				|| CachedState.StencilState.CCWStencilFunc != NewState->StencilFunc
				|| CachedState.StencilState.StencilRef != NewState->StencilRef
				|| CachedState.StencilState.StencilReadMask != NewState->StencilReadMask)
			{
				glStencilFunc(NewState->StencilFunc, NewState->StencilRef, NewState->StencilReadMask);
				CachedState.StencilState.StencilFunc = NewState->StencilFunc;
				CachedState.StencilState.CCWStencilFunc = NewState->StencilFunc;
				CachedState.StencilState.StencilRef = NewState->StencilRef;
				CachedState.StencilState.StencilReadMask = NewState->StencilReadMask;
			}

			if (CachedState.StencilState.StencilFail != NewState->StencilFail
				|| CachedState.StencilState.StencilZFail != NewState->StencilZFail
				|| CachedState.StencilState.StencilPass != NewState->StencilPass
				|| CachedState.StencilState.CCWStencilFail != NewState->StencilFail
				|| CachedState.StencilState.CCWStencilZFail != NewState->StencilZFail
				|| CachedState.StencilState.CCWStencilPass != NewState->StencilPass)
			{
				glStencilOp(NewState->StencilFail, NewState->StencilZFail, NewState->StencilPass);
				CachedState.StencilState.StencilFail = NewState->StencilFail;
				CachedState.StencilState.StencilZFail = NewState->StencilZFail;
				CachedState.StencilState.StencilPass = NewState->StencilPass;
				CachedState.StencilState.CCWStencilFail = NewState->StencilFail;
				CachedState.StencilState.CCWStencilZFail = NewState->StencilZFail;
				CachedState.StencilState.CCWStencilPass = NewState->StencilPass;
			}
		}

		if (CachedState.StencilState.StencilWriteMask != NewState->StencilWriteMask)
		{
			glStencilMask(NewState->StencilWriteMask);
			CachedState.StencilState.StencilWriteMask = NewState->StencilWriteMask;
		}
	}
}

void FOpenGLDynamicRHI::SetBlendState(FBlendStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(BlendState,NewState);

	if (CachedState.BlendState.bAlphaBlendEnable != NewState->bAlphaBlendEnable)
	{
		if (NewState->bAlphaBlendEnable)
		{
			glEnable(GL_BLEND);
		}
		else
		{
			glDisable(GL_BLEND);
		}
		CachedState.BlendState.bAlphaBlendEnable = NewState->bAlphaBlendEnable;
	}

	if (NewState->bAlphaBlendEnable)
	{
		if (NewState->bSeparateAlphaBlendEnable)
		{
			if (CachedState.BlendState.ColorSourceBlendFactor != NewState->ColorSourceBlendFactor
				|| CachedState.BlendState.ColorDestBlendFactor != NewState->ColorDestBlendFactor
				|| CachedState.BlendState.AlphaSourceBlendFactor != NewState->AlphaSourceBlendFactor
				|| CachedState.BlendState.AlphaDestBlendFactor != NewState->AlphaDestBlendFactor)
			{
				glBlendFuncSeparate(NewState->ColorSourceBlendFactor, NewState->ColorDestBlendFactor,
					NewState->AlphaSourceBlendFactor, NewState->AlphaDestBlendFactor);
				CachedState.BlendState.ColorSourceBlendFactor = NewState->ColorSourceBlendFactor;
				CachedState.BlendState.ColorDestBlendFactor = NewState->ColorDestBlendFactor;
				CachedState.BlendState.AlphaSourceBlendFactor = NewState->AlphaSourceBlendFactor;
				CachedState.BlendState.AlphaDestBlendFactor = NewState->AlphaDestBlendFactor;
			}

			if (CachedState.BlendState.ColorBlendOperation != NewState->ColorBlendOperation
				|| CachedState.BlendState.AlphaBlendOperation != NewState->AlphaBlendOperation)
			{
				glBlendEquationSeparate(NewState->ColorBlendOperation, NewState->AlphaBlendOperation);
				CachedState.BlendState.ColorBlendOperation = NewState->ColorBlendOperation;
				CachedState.BlendState.AlphaBlendOperation = NewState->AlphaBlendOperation;
			}
		}
		else
		{
			if (CachedState.BlendState.ColorSourceBlendFactor != NewState->ColorSourceBlendFactor
				|| CachedState.BlendState.ColorDestBlendFactor != NewState->ColorDestBlendFactor
				|| CachedState.BlendState.AlphaSourceBlendFactor != NewState->ColorSourceBlendFactor
				|| CachedState.BlendState.AlphaDestBlendFactor != NewState->ColorDestBlendFactor)
			{
				glBlendFunc(NewState->ColorSourceBlendFactor, NewState->ColorDestBlendFactor);
				CachedState.BlendState.ColorSourceBlendFactor = NewState->ColorSourceBlendFactor;
				CachedState.BlendState.ColorDestBlendFactor = NewState->ColorDestBlendFactor;
				CachedState.BlendState.AlphaSourceBlendFactor = NewState->ColorSourceBlendFactor;
				CachedState.BlendState.AlphaDestBlendFactor = NewState->ColorDestBlendFactor;
			}

			if (CachedState.BlendState.ColorBlendOperation != NewState->ColorBlendOperation)
			{
				glBlendEquation(NewState->ColorBlendOperation);
				CachedState.BlendState.ColorBlendOperation = NewState->ColorBlendOperation;
				CachedState.BlendState.AlphaBlendOperation = NewState->ColorBlendOperation;
			}
		}
	}

	if (CachedState.BlendState.bAlphaTestEnable != NewState->bAlphaTestEnable)
	{
		if (NewState->bAlphaTestEnable)
		{
			glEnable(GL_ALPHA_TEST);
		}
		else
		{
			glDisable(GL_ALPHA_TEST);
		}
		CachedState.BlendState.bAlphaTestEnable = NewState->bAlphaTestEnable;
	}

	if (NewState->bAlphaTestEnable)
	{
		if (CachedState.BlendState.AlphaFunc != NewState->AlphaFunc || CachedState.BlendState.AlphaRef != NewState->AlphaRef)
		{
			glAlphaFunc(NewState->AlphaFunc, NewState->AlphaRef);
			CachedState.BlendState.AlphaFunc = NewState->AlphaFunc;
			CachedState.BlendState.AlphaRef = NewState->AlphaRef;
		}
	}
}

void FOpenGLDynamicRHI::SetMRTBlendState(FBlendStateRHIParamRef NewStateRHI, UINT TargetIndex)
{
	//@todo opengl: MRT support for OpenGL
	check(0);
}

void FOpenGLDynamicRHI::SetRenderTarget( FSurfaceRHIParamRef NewRenderTargetRHI, FSurfaceRHIParamRef NewDepthStencilTargetRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Surface,NewRenderTarget);

	PendingFramebuffer = GetOpenGLFramebuffer(NewRenderTargetRHI, NewDepthStencilTargetRHI);
	bPendingFramebufferHasRenderTarget = (NewRenderTarget != NULL);

	// Warning: GetOpenGLFramebuffer() calls glBindFramebuffer() when it's generating new buffer.
	// But when it does that,
	// 1) it sets the framebuffer to the one that should be set in this call, and
	// 2) just-made framebuffer won't be set in cache.
	// So the line below is still a valid optimisation.
	if( CachedState.Framebuffer == PendingFramebuffer )
	{
		return;
	}
	BindPendingFramebuffer();

	int OldRenderTargetHeight = CachedState.RenderTargetHeight;

	// Detect when the back buffer is being set, and set the correct viewport.
	if (DrawingViewport && (!NewRenderTarget || NewRenderTarget == DefaultViewport->GetBackBuffer()) )
	{
		CachedState.RenderTargetWidth = DrawingViewport->GetSizeX();
		CachedState.RenderTargetHeight = DrawingViewport->GetSizeY();
	}
	else if (NewRenderTarget)
	{
		CachedState.RenderTargetWidth = NewRenderTarget->SizeX;
		CachedState.RenderTargetHeight = NewRenderTarget->SizeY;
	}

	if( OldRenderTargetHeight != CachedState.RenderTargetHeight )
	{
		UpdateScissorRectOnRenderTargetHeightChange();
	}

	InternalSetViewport( 0, 0, CachedState.MinZ, CachedState.RenderTargetWidth, CachedState.RenderTargetHeight, CachedState.MaxZ, TRUE );
}

void FOpenGLDynamicRHI::SetMRTRenderTarget( FSurfaceRHIParamRef NewRenderTargetRHI, UINT TargetIndex)
{
	//@todo opengl: MRT support for OpenGL
	check(0);
}

void FOpenGLDynamicRHI::SetColorWriteEnable(UBOOL bEnable)
{
	SetColorWriteMask( bEnable ? CW_RGBA : 0 );
}

void FOpenGLDynamicRHI::SetMRTColorWriteEnable(UBOOL bEnable, UINT TargetIndex)
{
	//@todo opengl: MRT support for OpenGL
	check(0);
}

void FOpenGLDynamicRHI::SetColorWriteMask(UINT ColorWriteMask)
{
	if( CachedState.ColorWriteEnabled != ( ColorWriteMask & CW_RGBA ) )
	{
		glColorMask((ColorWriteMask & CW_RED), (ColorWriteMask & CW_GREEN), (ColorWriteMask & CW_BLUE), (ColorWriteMask & CW_ALPHA));
		CachedState.ColorWriteEnabled = ( ColorWriteMask & CW_RGBA );
	}
}

void FOpenGLDynamicRHI::SetMRTColorWriteMask(UINT ColorWriteMask, UINT TargetIndex)
{
	//@todo opengl: MRT support for OpenGL
	check(0);
}

// Not supported
void FOpenGLDynamicRHI::BeginHiStencilRecord(UBOOL bCompareFunctionEqual, UINT RefValue) { }
void FOpenGLDynamicRHI::BeginHiStencilPlayback(UBOOL bFlush) { }
void FOpenGLDynamicRHI::EndHiStencil() { }

// Primitive drawing.

void FOpenGLDynamicRHI::EnableVertexElement(const OpenGLVertexElement &VertexElement, GLsizei Stride, void *Pointer, GLuint Buffer)
{
	FOpenGLCachedAttr &Attr = CachedVertexAttrs[VertexElement.Usage];
	if( !Attr.Enabled )
	{
		glEnableVertexAttribArray(VertexElement.Usage);
		Attr.Enabled = TRUE;
	}

	if( (Attr.Pointer != Pointer) ||
		(Attr.Buffer != Buffer) ||
		(Attr.Usage != VertexElement.Usage) ||
		(Attr.Size != VertexElement.Size) ||
		(Attr.Type != VertexElement.Type) ||
		(Attr.bNormalized != VertexElement.bNormalized) ||
		(Attr.Stride != Stride) )
	{
		CachedBindArrayBuffer( Buffer);
		glVertexAttribPointer(VertexElement.Usage, VertexElement.Size, VertexElement.Type, VertexElement.bNormalized, Stride, Pointer);
		Attr.Pointer = Pointer;
		Attr.Buffer = Buffer;
		Attr.Usage = VertexElement.Usage;
		Attr.Size = VertexElement.Size;
		Attr.Type = VertexElement.Type;
		Attr.bNormalized = VertexElement.bNormalized;
		Attr.Stride = Stride;
	}
}

void FOpenGLDynamicRHI::SetupVertexArrays( UINT BaseVertexIndex )
{
	UBOOL UsedVertexArrays[NumVertexStreams] = { 0 };

	FOpenGLVertexDeclaration *VertexDeclaration = PendingBoundShaderState->VertexDeclaration;
	for (INT ElementIndex = 0; ElementIndex < VertexDeclaration->VertexElements.Num(); ElementIndex++)
	{
		OpenGLVertexElement &VertexElement = VertexDeclaration->VertexElements(ElementIndex);
		FOpenGLStream* Stream = &PendingStreams[VertexElement.StreamIndex];
		UINT Frequency = Stream->Frequency;
		UINT Stride = Stream->Stride;

		EnableVertexElement( VertexElement, Stride, (void *)(BaseVertexIndex * Stride + Stream->Offset + VertexElement.Offset), Stream->VertexBuffer->Resource );
		UsedVertexArrays[VertexElement.Usage] = TRUE;
		if (VertexElement.Usage != GLAttr_Position)
		{
			glVertexAttribDivisorARB(VertexElement.Usage, Frequency);
		}
	}

	// Disable remaining vertex arrays
	for (GLuint AttribIndex = 0; AttribIndex < NumVertexStreams; AttribIndex++)
	{
		if( ( UsedVertexArrays[AttribIndex] == FALSE ) && CachedVertexAttrs[AttribIndex].Enabled )
		{
			glDisableVertexAttribArray(AttribIndex);
			CachedVertexAttrs[AttribIndex].Enabled = FALSE;
		}
	}
}

void FOpenGLDynamicRHI::SetupVertexArraysWithData( const void* VertexData, UINT VertexDataStride )
{
	UBOOL UsedVertexArrays[NumVertexStreams] = { 0 };

	FOpenGLVertexDeclaration *VertexDeclaration = PendingBoundShaderState->VertexDeclaration;
	for (INT ElementIndex = 0; ElementIndex < VertexDeclaration->VertexElements.Num(); ElementIndex++)
	{
		OpenGLVertexElement &VertexElement = VertexDeclaration->VertexElements(ElementIndex);
		EnableVertexElement(VertexElement, VertexDataStride, (void *)((BYTE *)VertexData + VertexElement.Offset), 0 );
		UsedVertexArrays[VertexElement.Usage] = TRUE;
		if (VertexElement.Usage != GLAttr_Position)
		{
			glVertexAttribDivisorARB(VertexElement.Usage, 0);
		}
	}

	// Disable remaining vertex arrays
	for (GLuint AttribIndex = 0; AttribIndex < NumVertexStreams; AttribIndex++)
	{
		if( ( UsedVertexArrays[AttribIndex] == FALSE ) && CachedVertexAttrs[AttribIndex].Enabled )
		{
			glDisableVertexAttribArray(AttribIndex);
			CachedVertexAttrs[AttribIndex].Enabled = FALSE;
		}
	}
}

void FOpenGLDynamicRHI::OnVertexBufferDeletion( GLuint VertexBufferResource )
{
	for (GLuint AttribIndex = 0; AttribIndex < NumVertexStreams; AttribIndex++)
	{
		if( CachedVertexAttrs[AttribIndex].Buffer == VertexBufferResource )
		{
			CachedVertexAttrs[AttribIndex].Pointer = FOpenGLCachedAttr_Invalid;	// that'll enforce state update on next cache test
		}
	}
}

void FOpenGLDynamicRHI::CommitNonComputeShaderConstants()
{
#if OPENGL_USE_BINDABLE_UNIFORMS
	GLuint Program = PendingBoundShaderState->Resource;

	for(INT BufferIndex = 0;BufferIndex < MAX_VS_CONSTANT_BUFFER_SLOTS;BufferIndex++)
	{
		FOpenGLConstantBuffer *ConstantBuffer = VSConstantBuffers(BufferIndex);
		UBOOL bChanged = ConstantBuffer->CommitConstantsToDevice(bDiscardSharedConstants);

		FOpenGLUniformBuffer *UniformBuffer = ConstantBuffer->GetUniformBuffer();
		switch (BufferIndex)
		{
		case LOCAL_CONSTANT_BUFFER:
			if (PendingBoundShaderState->VConstFloatLocation != -1)
			{
				glUniformBufferEXT(Program, PendingBoundShaderState->VConstFloatLocation, UniformBuffer->Resource);
			}
			break;

		case GLOBAL_CONSTANT_BUFFER:
			if (PendingBoundShaderState->VConstGlobalLocation != -1)
			{
				glUniformBufferEXT(Program, PendingBoundShaderState->VConstGlobalLocation, UniformBuffer->Resource);
			}
			break;

		case VS_BONE_CONSTANT_BUFFER:
			if (PendingBoundShaderState->VConstBonesLocation != -1)
			{
				glUniformBufferEXT(Program, PendingBoundShaderState->VConstBonesLocation, UniformBuffer->Resource);
			}
			break;

		case VS_BOOL_CONSTANT_BUFFER:
			if (PendingBoundShaderState->VConstBoolLocation != -1)
			{
				glUniformBufferEXT(Program, PendingBoundShaderState->VConstBoolLocation, UniformBuffer->Resource);
			}
			break;
		}
		CheckOpenGLErrors();
	}

	for(INT BufferIndex = 0;BufferIndex < MAX_PS_CONSTANT_BUFFER_SLOTS;BufferIndex++)
	{
		FOpenGLConstantBuffer *ConstantBuffer = PSConstantBuffers(BufferIndex);
		UBOOL bChanged = ConstantBuffer->CommitConstantsToDevice(bDiscardSharedConstants);

		FOpenGLUniformBuffer *UniformBuffer = ConstantBuffer->GetUniformBuffer();
		switch (BufferIndex)
		{
		case LOCAL_CONSTANT_BUFFER:
			if (PendingBoundShaderState->PConstFloatLocation != -1)
			{
				glUniformBufferEXT(Program, PendingBoundShaderState->PConstFloatLocation, UniformBuffer->Resource);
			}
			break;

		case GLOBAL_CONSTANT_BUFFER:
			if (PendingBoundShaderState->PConstGlobalLocation != -1)
			{
				glUniformBufferEXT(Program, PendingBoundShaderState->PConstGlobalLocation, UniformBuffer->Resource);
			}
			break;

		case PS_BOOL_CONSTANT_BUFFER:
			if (PendingBoundShaderState->PConstBoolLocation != -1)
			{
				glUniformBufferEXT(Program, PendingBoundShaderState->PConstBoolLocation, UniformBuffer->Resource);
			}
			break;
		}
		CheckOpenGLErrors();
	}
#else
	for (INT BufferIndex = 0; BufferIndex < MAX_VS_CONSTANT_BUFFER_SLOTS; BufferIndex++)
	{
		FOpenGLConstantBuffer *ConstantBuffer = VSConstantBuffers(BufferIndex);
		ConstantBuffer->CommitConstantsToDevice(bDiscardSharedConstants);
		PendingBoundShaderState->UpdateUniforms(BufferIndex, ConstantBuffer->GetData(), ConstantBuffer->GetUpdateSize());
	}

	for (INT BufferIndex = 0; BufferIndex < MAX_PS_CONSTANT_BUFFER_SLOTS; BufferIndex++)
	{
		FOpenGLConstantBuffer *ConstantBuffer = PSConstantBuffers(BufferIndex);
		ConstantBuffer->CommitConstantsToDevice(bDiscardSharedConstants);
		PendingBoundShaderState->UpdateUniforms(BufferIndex + MAX_VS_CONSTANT_BUFFER_SLOTS, ConstantBuffer->GetData(), ConstantBuffer->GetUpdateSize());
	}
#endif
	bDiscardSharedConstants = FALSE;
}

void FOpenGLDynamicRHI::DrawPrimitive(UINT PrimitiveType,UINT BaseVertexIndex,UINT NumPrimitives)
{
	INC_DWORD_STAT(STAT_OpenGLDrawPrimitiveCalls);
	INC_DWORD_STAT_BY(STAT_OpenGLTriangles,(DWORD)(PrimitiveType != PT_LineList ? NumPrimitives : 0));
	INC_DWORD_STAT_BY(STAT_OpenGLLines,(DWORD)(PrimitiveType == PT_LineList ? NumPrimitives : 0));

	BindPendingFramebuffer();

	GLenum DrawMode = GL_TRIANGLES;
	GLsizei NumElements = 0;
	FindPrimitiveType(PrimitiveType, NumPrimitives, DrawMode, NumElements);

	PendingBoundShaderState->Bind();
	CommitNonComputeShaderConstants();
	CachedBindElementArrayBuffer(0);
	SetupVertexArrays( BaseVertexIndex );

	if (PendingNumInstances == 1)
	{
		glDrawArrays(DrawMode, 0, NumElements);
	}
	else
	{
		glDrawArraysInstancedARB(DrawMode, 0, NumElements, PendingNumInstances);
	}
	CheckOpenGLErrors();
}

void FOpenGLDynamicRHI::DrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI,UINT PrimitiveType,INT BaseVertexIndex,UINT MinIndex,UINT NumVertices,UINT StartIndex,UINT NumPrimitives)
{
	DYNAMIC_CAST_OPENGLRESOURCE(IndexBuffer,IndexBuffer);

	INC_DWORD_STAT(STAT_OpenGLDrawPrimitiveCalls);
	INC_DWORD_STAT_BY(STAT_OpenGLTriangles,(DWORD)(PrimitiveType != PT_LineList ? NumPrimitives : 0));
	INC_DWORD_STAT_BY(STAT_OpenGLLines,(DWORD)(PrimitiveType == PT_LineList ? NumPrimitives : 0));

	BindPendingFramebuffer();

	GLenum DrawMode = GL_TRIANGLES;
	GLsizei NumElements = 0;
	FindPrimitiveType(PrimitiveType, NumPrimitives, DrawMode, NumElements);

	GLenum IndexType = IndexBuffer->bIs32Bit ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
	StartIndex *= IndexBuffer->bIs32Bit ? sizeof(DWORD) : sizeof(WORD);

	PendingBoundShaderState->Bind();
	CommitNonComputeShaderConstants();
	CachedBindElementArrayBuffer(IndexBuffer->Resource);
	SetupVertexArrays( BaseVertexIndex );

	if (PendingNumInstances == 1)
	{
		glDrawRangeElements(DrawMode, MinIndex, MinIndex + NumVertices, NumElements, IndexType, (void *)StartIndex);
	}
	else
	{
		glDrawElementsInstancedARB(DrawMode, NumElements, IndexType, (void *)StartIndex, PendingNumInstances);
	}
	CheckOpenGLErrors();
}

void FOpenGLDynamicRHI::DrawIndexedPrimitive_PreVertexShaderCulling(FIndexBufferRHIParamRef IndexBuffer,UINT PrimitiveType,INT BaseVertexIndex,UINT MinIndex,UINT NumVertices,UINT StartIndex,UINT NumPrimitives,const FMatrix& LocalToWorld,const void* PlatformMeshData)
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
void FOpenGLDynamicRHI::BeginDrawPrimitiveUP( UINT PrimitiveType, UINT NumPrimitives, UINT NumVertices, UINT VertexDataStride, void*& OutVertexData)
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
void FOpenGLDynamicRHI::EndDrawPrimitiveUP()
{
	check(PendingBegunDrawPrimitiveUP);
	PendingBegunDrawPrimitiveUP = FALSE;

	// for now (while RHIDrawPrimitiveUP still exists), just call it because it does the same work we need here
	RHIDrawPrimitiveUP(PendingPrimitiveType, PendingNumPrimitives, &PendingDrawPrimitiveUPVertexData(0), PendingVertexDataStride);
}

/**
 * Draw a primitive using the vertices passed in
 * VertexData is NOT created by BeginDrawPrimitveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param VertexData A reference to memory preallocate in RHIBeginDrawPrimitiveUP
 * @param VertexDataStride The size of one vertex
 */
void FOpenGLDynamicRHI::DrawPrimitiveUP( UINT PrimitiveType, UINT NumPrimitives, const void* VertexData,UINT VertexDataStride)
{
	INC_DWORD_STAT(STAT_OpenGLDrawPrimitiveCalls);
	INC_DWORD_STAT_BY(STAT_OpenGLTriangles,(DWORD)(PrimitiveType != PT_LineList ? NumPrimitives : 0));
	INC_DWORD_STAT_BY(STAT_OpenGLLines,(DWORD)(PrimitiveType == PT_LineList ? NumPrimitives : 0));

	BindPendingFramebuffer();

	GLenum DrawMode = GL_TRIANGLES;
	GLsizei NumElements = 0;
	FindPrimitiveType(PrimitiveType, NumPrimitives, DrawMode, NumElements);

	PendingBoundShaderState->Bind();
	CommitNonComputeShaderConstants();
	CachedBindElementArrayBuffer(0);
	SetupVertexArraysWithData(VertexData, VertexDataStride);

	glDrawArrays(DrawMode, 0, NumElements);
	CheckOpenGLErrors();
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
void FOpenGLDynamicRHI::BeginDrawIndexedPrimitiveUP( UINT PrimitiveType, UINT NumPrimitives, UINT NumVertices, UINT VertexDataStride, void*& OutVertexData, UINT MinVertexIndex, UINT NumIndices, UINT IndexDataStride, void*& OutIndexData)
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
void FOpenGLDynamicRHI::EndDrawIndexedPrimitiveUP()
{
	check(PendingBegunDrawPrimitiveUP);
	PendingBegunDrawPrimitiveUP = FALSE;

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
void FOpenGLDynamicRHI::DrawIndexedPrimitiveUP( UINT PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT NumPrimitives, const void* IndexData, UINT IndexDataStride, const void* VertexData, UINT VertexDataStride)
{
	INC_DWORD_STAT(STAT_OpenGLDrawPrimitiveCalls);
	INC_DWORD_STAT_BY(STAT_OpenGLTriangles,(DWORD)(PrimitiveType != PT_LineList ? NumPrimitives : 0));
	INC_DWORD_STAT_BY(STAT_OpenGLLines,(DWORD)(PrimitiveType == PT_LineList ? NumPrimitives : 0));

	BindPendingFramebuffer();

	GLenum DrawMode = GL_TRIANGLES;
	GLsizei NumElements = 0;
	FindPrimitiveType(PrimitiveType, NumPrimitives, DrawMode, NumElements);

	GLenum IndexType = (IndexDataStride == sizeof(DWORD)) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

	PendingBoundShaderState->Bind();
	CommitNonComputeShaderConstants();
	CachedBindElementArrayBuffer(0);
	SetupVertexArraysWithData(VertexData, VertexDataStride);

	glDrawRangeElements(DrawMode, MinVertexIndex, MinVertexIndex + NumVertices, NumElements, IndexType, IndexData);
	CheckOpenGLErrors();
}

/**
 * Draw a sprite particle emitter.
 *
 * @param Mesh The mesh element containing the data for rendering the sprite particles
 */
void FOpenGLDynamicRHI::DrawSpriteParticles( const FMeshBatch& Mesh)
{
	checkSlow(Mesh.DynamicVertexData);
	FDynamicSpriteEmitterData* SpriteData = (FDynamicSpriteEmitterData*)(Mesh.DynamicVertexData);

	// Sort the particles if required
	INT ParticleCount = SpriteData->GetSource().ActiveParticleCount;

	// 'clamp' the number of particles actually drawn
	//@todo opengl.SAS. If sorted, we really want to render the front 'N' particles...
	// right now it renders the back ones. (Same for SubUV draws)
	INT StartIndex = 0;
	INT EndIndex = ParticleCount;
	if ((SpriteData->Source.MaxDrawCount >= 0) && (ParticleCount > SpriteData->Source.MaxDrawCount))
	{
		ParticleCount = SpriteData->Source.MaxDrawCount;
	}

	// todo : support batching
	TArray<FParticleOrder>* ParticleOrder = (TArray<FParticleOrder>*)(Mesh.Elements(0).DynamicIndexData);

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
void FOpenGLDynamicRHI::DrawSubUVParticles( const FMeshBatch& Mesh)
{
	checkSlow(Mesh.DynamicVertexData);
	FDynamicSubUVEmitterData* SubUVData = (FDynamicSubUVEmitterData*)(Mesh.DynamicVertexData);

	// Sort the particles if required
	INT ParticleCount = SubUVData->Source.ActiveParticleCount;

	// 'clamp' the number of particles actually drawn
	//@todo opengl.SAS. If sorted, we really want to render the front 'N' particles...
	// right now it renders the back ones. (Same for SubUV draws)
	INT StartIndex = 0;
	INT EndIndex = ParticleCount;
	if ((SubUVData->Source.MaxDrawCount >= 0) && (ParticleCount > SubUVData->Source.MaxDrawCount))
	{
		ParticleCount = SubUVData->Source.MaxDrawCount;
	}

	// todo : support batching
	TArray<FParticleOrder>* ParticleOrder = (TArray<FParticleOrder>*)(Mesh.Elements(0).DynamicIndexData);

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
void FOpenGLDynamicRHI::DrawPointSpriteParticles(const FMeshBatch& Mesh) 
{
	// Not implemented yet!
}

// Raster operations.
void FOpenGLDynamicRHI::Clear(UBOOL bClearColor,const FLinearColor& Color,UBOOL bClearDepth,FLOAT Depth,UBOOL bClearStencil,DWORD Stencil)
{
	BindPendingFramebuffer();

	FIntRect CachedScissor = CachedState.Scissor;
	UBOOL EnabledScissor = CachedState.bScissorEnabled;

	INT ViewportWidth = CachedState.Viewport.Max.X - CachedState.Viewport.Min.X;
	INT ViewportHeight = CachedState.Viewport.Max.Y - CachedState.Viewport.Min.Y;
	if (CachedState.Viewport.Min.X != 0 || CachedState.Viewport.Min.Y != 0 || ViewportWidth != CachedState.RenderTargetWidth || ViewportHeight != CachedState.RenderTargetHeight)
	{
		SetScissorRect(TRUE,CachedState.Viewport.Min.X, CachedState.Viewport.Min.Y, CachedState.Viewport.Max.X, CachedState.Viewport.Max.Y);
	}
	else if (EnabledScissor)
	{
		SetScissorRect(FALSE,CachedScissor.Min.X, CachedScissor.Min.Y, CachedScissor.Max.X, CachedScissor.Max.Y);
	}

	GLuint ClearMask = 0;
	if (bClearColor)
	{
		if (CachedState.ClearColor != Color)
		{
			glClearColor(Color.R, Color.G, Color.B, Color.A);
			glClearStencil(Stencil);
			CachedState.ClearColor = Color;
		}
		if (CachedState.ColorWriteEnabled != CW_RGBA)
		{
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		ClearMask |= GL_COLOR_BUFFER_BIT;
	}
	if (bClearDepth)
	{
		if (CachedState.ClearDepth != Depth)
		{
			glClearDepth(Depth);
			CachedState.ClearDepth = Depth;
		}

		if (!CachedState.DepthState.bZWriteEnable)
		{
			glDepthMask(GL_TRUE);
		}
		ClearMask |= GL_DEPTH_BUFFER_BIT;
	}
	if (bClearStencil)
	{
		if (CachedState.ClearStencil != Stencil)
		{
			glClearStencil(Stencil);
			CachedState.ClearStencil = Stencil;
		}

		if (CachedState.StencilState.StencilWriteMask != 0xFFFFFFFF)
		{
			glStencilMask(0xFFFFFFFF);
		}
		ClearMask |= GL_STENCIL_BUFFER_BIT;
	}

	glClear(ClearMask);

	SetScissorRect(EnabledScissor,CachedScissor.Min.X, CachedScissor.Min.Y, CachedScissor.Max.X, CachedScissor.Max.Y);

	if (bClearDepth && !CachedState.DepthState.bZWriteEnable)
	{
		glDepthMask(GL_FALSE);
	}

	if (bClearStencil && CachedState.StencilState.StencilWriteMask != 0xFFFFFFFF)
	{
		glStencilMask(CachedState.StencilState.StencilWriteMask);
	}

	if( bClearColor && (CachedState.ColorWriteEnabled != CW_RGBA) )
	{
		glColorMask((CachedState.ColorWriteEnabled & CW_RED), (CachedState.ColorWriteEnabled & CW_GREEN), (CachedState.ColorWriteEnabled & CW_BLUE), (CachedState.ColorWriteEnabled & CW_ALPHA));
	}
}

// Functions to yield and regain rendering control from OpenGL

void FOpenGLDynamicRHI::SuspendRendering()
{
	// Not supported
}

void FOpenGLDynamicRHI::ResumeRendering()
{
	// Not supported
}

UBOOL FOpenGLDynamicRHI::IsRenderingSuspended()
{
	// Not supported
	return FALSE;
}

// Kick the rendering commands that are currently queued up in the GPU command buffer.
void FOpenGLDynamicRHI::KickCommandBuffer()
{
	// Not really supported
}

// Blocks the CPU until the GPU catches up and goes idle.
void FOpenGLDynamicRHI::BlockUntilGPUIdle()
{
	// Not really supported
}

/*
 * Returns the total GPU time taken to render the last frame. Same metric as appCycles().
 */
DWORD FOpenGLDynamicRHI::GetGPUFrameCycles()
{
	//@todo opengl
	return 0;
}

/*
 * Returns an approximation of the available memory that textures can use, which is video + AGP where applicable, rounded to the nearest MB, in MB.
 */
DWORD FOpenGLDynamicRHI::GetAvailableTextureMemory()
{
	//apparently GetAvailableTextureMem() returns available bytes (the docs don't say) rounded to the nearest MB.
	//TODO: Get this through DXGI or WMI channels
	return 512;
	//return Direct3DDevice->GetAvailableTextureMem() / 1048576;
}

// not used on PC
void FOpenGLDynamicRHI::SetViewPixelParameters(const FSceneView* View,FPixelShaderRHIParamRef PixelShader,const class FShaderParameter* SceneDepthCalcParameter,const class FShaderParameter* ScreenPositionScaleBiasParameter,const class FShaderParameter* ScreenAndTexelSizeParameter){}
void FOpenGLDynamicRHI::SetRenderTargetBias(  FLOAT ColorBias ){}
void FOpenGLDynamicRHI::SetShaderRegisterAllocation(UINT NumVertexShaderRegisters, UINT NumPixelShaderRegisters){}
void FOpenGLDynamicRHI::ReduceTextureCachePenalty( FPixelShaderRHIParamRef PixelShader ){}
void FOpenGLDynamicRHI::RestoreColorDepth(FTexture2DRHIParamRef ColorTexture, FTexture2DRHIParamRef DepthTexture){}
void FOpenGLDynamicRHI::SetTessellationMode( ETessellationMode TessellationMode, FLOAT MinTessellation, FLOAT MaxTessellation ){}
void FOpenGLDynamicRHI::UpdateStereoFixTexture(FTexture2DRHIParamRef TextureRHI){}

// Only used on mobile platforms
INT FOpenGLDynamicRHI::GetMobileUniformSlotIndexByName(FName ParamName,WORD& OutNumBytes){return -1;}
void FOpenGLDynamicRHI::SetMobileTextureSamplerState( FPixelShaderRHIParamRef PixelShader, const INT MobileTextureUnit, FSamplerStateRHIParamRef NewState, FTextureRHIParamRef NewTextureRHI, FLOAT MipBias, FLOAT LargestMip, FLOAT SmallestMip ){}
void FOpenGLDynamicRHI::SetMobileSimpleParams(EBlendMode InBlendMode){}
void FOpenGLDynamicRHI::SetMobileMaterialVertexParams(const FMobileMaterialVertexParams& InVertexParams){}
void FOpenGLDynamicRHI::SetMobileMaterialPixelParams(const FMobileMaterialPixelParams& InPixelParams){}
void FOpenGLDynamicRHI::SetMobileMeshVertexParams(const FMobileMeshVertexParams& InMeshParams){}
void FOpenGLDynamicRHI::SetMobileMeshPixelParams(const FMobileMeshPixelParams& InMeshParams){}
FLOAT FOpenGLDynamicRHI::GetMobilePercentColorFade(void){return 0.0f;}
void FOpenGLDynamicRHI::SetMobileFogParams (const UBOOL bInEnabled, const FLOAT InFogStart, const FLOAT InFogEnd, const FColor& InFogColor){}
void FOpenGLDynamicRHI::SetMobileHeightFogParams(const FHeightFogParams& Params) {}

void FOpenGLDynamicRHI::SetMobileBumpOffsetParams(const UBOOL bInEnabled, const FLOAT InBumpEnd){}
void FOpenGLDynamicRHI::SetMobileGammaCorrection(const UBOOL bInEnabled){}

void FOpenGLDynamicRHI::SetMobileTextureTransformOverride(TMatrix<3,3>& InOverrideTransform){}
void FOpenGLDynamicRHI::SetMobileDistanceFieldParams(const struct FMobileDistanceFieldParams& Params){}

void FOpenGLDynamicRHI::SetMobileColorGradingParams(const FMobileColorGradingParams& Params) { }

void* FOpenGLDynamicRHI::GetMobileProgramInstance()
{
	// Only used on mobile platforms
	return NULL;
}

void FOpenGLDynamicRHI::SetMobileProgramInstance(void* ProgramInstance)
{
	// Only used on mobile platforms
}

// Tessellation is not supported in OpenGL
#if WITH_D3D11_TESSELLATION
FHullShaderRHIRef FOpenGLDynamicRHI::CreateHullShader(const TArray<BYTE>& Code) { appErrorf(TEXT("OpenGL Render path does not support Hull or Domain shaders for tessellation!")); return NULL; }
FDomainShaderRHIRef FOpenGLDynamicRHI::CreateDomainShader(const TArray<BYTE>& Code) { appErrorf(TEXT("OpenGL Render path does not support Hull or Domain shaders for tessellation!")); return NULL; }
FBoundShaderStateRHIRef FOpenGLDynamicRHI::CreateBoundShaderStateD3D11(FVertexDeclarationRHIParamRef VertexDeclaration, DWORD *StreamStrides, FVertexShaderRHIParamRef VertexShader, FHullShaderRHIParamRef HullShader, FDomainShaderRHIParamRef DomainShader, FPixelShaderRHIParamRef PixelShader, FGeometryShaderRHIParamRef GeometryShader, EMobileGlobalShaderType MobileGlobalShaderType)
{ 
	checkSlow(!HullShader);
	checkSlow(!DomainShader);
	checkSlow(!GeometryShader);

	return CreateBoundShaderState(VertexDeclaration, StreamStrides, VertexShader, PixelShader, MobileGlobalShaderType);
}
#endif

#if WITH_D3D11_TESSELLATION
void FOpenGLDynamicRHI::SetSamplerState(FDomainShaderRHIParamRef DomainShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ appErrorf(TEXT("OpenGL Render path does not support Hull or Domain shaders for tessellation!")); }
void FOpenGLDynamicRHI::SetSamplerState(FHullShaderRHIParamRef HullShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ appErrorf(TEXT("OpenGL Render path does not support Hull or Domain shaders for tessellation!")); }
void FOpenGLDynamicRHI::SetShaderBoolParameter(FHullShaderRHIParamRef HullShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue)
{ appErrorf(TEXT("OpenGL Render path does not support Hull or Domain shaders for tessellation!")); }
void FOpenGLDynamicRHI::SetShaderBoolParameter(FDomainShaderRHIParamRef DomainShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue)
{ appErrorf(TEXT("OpenGL Render path does not support Hull or Domain shaders for tessellation!")); }
void FOpenGLDynamicRHI::SetShaderParameter(FHullShaderRHIParamRef HullShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("OpenGL Render path does not support Hull or Domain shaders for tessellation!")); }
void FOpenGLDynamicRHI::SetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("OpenGL Render path does not support Hull or Domain shaders for tessellation!")); }
void FOpenGLDynamicRHI::SetDomainShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("OpenGL Render path does not support Hull or Domain shaders for tessellation!")); }
void FOpenGLDynamicRHI::SetHullShaderParameter(FHullShaderRHIParamRef HullShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("OpenGL Render path does not support Hull or Domain shaders for tessellation!")); }
void FOpenGLDynamicRHI::SetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("OpenGL Render path does not support Geometry shaders!")); }
FGeometryShaderRHIRef FOpenGLDynamicRHI::CreateGeometryShader(const TArray<BYTE>& Code)
{ appErrorf(TEXT("OpenGL Render path does not support Geometry shaders!")); return NULL; }
void FOpenGLDynamicRHI::SetSamplerState(FGeometryShaderRHIParamRef GeometryShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ appErrorf(TEXT("OpenGL Render path does not support Geometry shaders!")); }
void FOpenGLDynamicRHI::SetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("OpenGL Render path does not support Compute shaders!")); }
FComputeShaderRHIRef FOpenGLDynamicRHI::CreateComputeShader(const TArray<BYTE>& Code)
{ appErrorf(TEXT("OpenGL Render path does not support Compute shaders!")); return NULL; }
void FOpenGLDynamicRHI::DispatchComputeShader(FComputeShaderRHIParamRef ComputeShader, UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{ appErrorf(TEXT("OpenGL Render path does not support Compute shaders!")); }
void FOpenGLDynamicRHI::SetSamplerState(FComputeShaderRHIParamRef ComputeShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ appErrorf(TEXT("OpenGL Render path does not support Compute shaders!")); }
#endif

void FOpenGLDynamicRHI::SetMultipleViewports(UINT Count, FViewPortBounds* Data)
{ appErrorf(TEXT("OpenGL Render path does not support multiple Viewports!")); }
FBlendStateRHIRef FOpenGLDynamicRHI::CreateMRTBlendState(const FMRTBlendStateInitializerRHI&)
{ appErrorf(TEXT("OpenGL Render path does not support CreateMRTBlendState!")); return NULL; }
