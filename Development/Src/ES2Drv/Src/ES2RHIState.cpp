/*=============================================================================
 ES2RHIImplementation.cpp: OpenGL ES 2.0 RHI definitions.
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "Engine.h"
#include "ES2RHIPrivate.h"

#if WITH_ES2_RHI

FStateShadow::FStateShadow()
:	RenderTargetWidth(0)
,	RenderTargetHeight(0)
,	CurrentRenderTargetRHI(NULL)
,	CurrentDepthTargetRHI(NULL)
,	CurrentRenderTargetID(0)
,	CurrentDepthStencilTargetID(0)
,	ColorWriteEnable(TRUE)
,	ColorWriteMask(CW_RGBA)
,	bIsUsingDummyDepthStencilBuffer(FALSE)
,	ActiveTexture(0)
,	BoundTextureMask(0)
,	ElementArrayBuffer(0)
{
	for ( INT Index=0; Index < 16; ++Index )
	{
		ArrayBuffer[Index] = 0;
		VertexAttribCount[Index] = 0;
		VertexAttribFormat[Index] = GL_FLOAT;
		VertexAttribNormalize[Index] = GL_FALSE;
		VertexAttribStride[Index] = 0;
		VertexAttribAddress[Index] = NULL;
	}
}

void FStateShadow::InvalidateAndResetDevice(void)
{
	// reset GL state
	RHISetColorWriteEnable(TRUE);
	const FRasterizerStateInitializerRHI RasterState = { FM_Solid,CM_CW, 0.f, 0.0f, FALSE };
	RHISetRasterizerStateImmediate(RasterState);
	RHISetDepthState(TStaticDepthState<TRUE, CF_LessEqual>::GetRHI());
	GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
	GLCHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

	for (INT TexUnit = GL_TEXTURE0; TexUnit < GL_TEXTURE0+MAX_Mapped_MobileTexture; ++TexUnit)
	{
		GLCHECK(glActiveTexture(TexUnit));
		GLCHECK(glBindTexture(GL_TEXTURE_2D, 0));
		GLCHECK(glBindTexture(GL_TEXTURE_CUBE_MAP, 0));
	}

	extern GLint GMaxVertexAttribsGLSL;
	for( INT Stream = 0; Stream < GMaxVertexAttribsGLSL; Stream++ )
	{
		GLCHECK(glDisableVertexAttribArray(Stream));
#if FLASH
		GLCHECK(glVertexAttribPointer(
			Stream,
			4,
			GL_FLOAT,
			0,
			0,
			NULL,
			0));
#else
		GLCHECK(glVertexAttribPointer(
			Stream,
			4,
			GL_FLOAT,
			0,
			0,
			NULL));
#endif
	}
	//@TODO - consider merging this mask with the shadow state
	GRenderManager.ResetAttribMask();

	GLCHECK(glUseProgram(0));
	extern void ResetCurrentProgram();
	ResetCurrentProgram();
	
	// mark the state shadow to out of range values to force them to be set next time
	ColorWriteEnable = INDEX_NONE;

	ElementArrayBuffer = 0;
	for (INT i = 0; i < 16; ++i)
	{
		ArrayBuffer[i] = 0;
		VertexAttribCount[i] = INDEX_NONE;
		VertexAttribFormat[i] = static_cast<GLenum>(INDEX_NONE);
		VertexAttribNormalize[i] = INDEX_NONE;
		VertexAttribStride[i] = INDEX_NONE;
		VertexAttribAddress[i] = NULL;
	}

	ActiveTexture = INDEX_NONE;
	for (INT i = 0; i < MAX_MobileTexture; ++i)
	{
		BoundTextureName[i] = INDEX_NONE;
		BoundTextureType[i] = INDEX_NONE;
		BoundTextureFormat[i] = static_cast<EPixelFormat>(INDEX_NONE);
	}

	// these states are set via RHI calls, and should be correctly mirrored in the state shadow 
	//Blend.AlphaTest = (ECompareFunction)INDEX_NONE;
	//Rasterizer.FillMode = (ERasterizerFillMode)INDEX_NONE;
	//Rasterizer.CullMode = (ERasterizerCullMode)INDEX_NONE;
	//Depth.bEnableDepthWrite = INDEX_NONE;
	//Depth.DepthTest = (ECompareFunction)INDEX_NONE;
	//ColorWriteMask = INDEX_NONE;
}

FStateShadow GStateShadow;

static DWORD TranslateUnrealCullMode(ERasterizerCullMode CullMode)
{
	switch(CullMode)
	{
		case CM_CW: return GL_CCW;
		case CM_CCW: return GL_CW;
		default: return 0;
	};
}

static GLenum TranslateUnrealStencilFunction(ECompareFunction CompareFunction)
{
	switch(CompareFunction)
	{
	case CF_Less: return GL_LESS;
	case CF_LessEqual: return GL_LEQUAL;
	case CF_Greater: return GL_GREATER;
	case CF_GreaterEqual: return GL_LESS;
	case CF_Equal: return GL_EQUAL;
	case CF_NotEqual: return GL_NOTEQUAL;
	case CF_Never: return GL_NEVER;
	case CF_Always: return GL_ALWAYS;
	default: return GL_ALWAYS;
	};
}

static GLenum TranslateUnrealStencilOp(EStencilOp StencilOp)
{
	switch(StencilOp)
	{
	case SO_Keep: return GL_KEEP;
	case SO_Zero: return GL_ZERO;
	case SO_Replace: return GL_REPLACE;
	case SO_SaturatedIncrement: return GL_INCR;
	case SO_SaturatedDecrement: return GL_DECR;
	case SO_Invert: return GL_INVERT;
	case SO_Increment: return GL_INCR_WRAP;
	case SO_Decrement: return GL_DECR_WRAP;
	default: return SO_Keep;
	};
}

static DWORD TranslateUnrealBlendOp(EBlendOperation BlendOp)
{
	switch(BlendOp)
	{
	case BO_Subtract: return GL_FUNC_SUBTRACT;
	case BO_Min:
	case BO_Max: appErrorf(TEXT("BO_Min/BO_Max not supported on mobile devices"));
	default: return GL_FUNC_ADD;
	};
}

static GLint TranslateUnrealBlendFactor(EBlendFactor BlendFactor)
{
	switch(BlendFactor)
	{
	case BF_One: return GL_ONE;
	case BF_SourceColor: return GL_SRC_COLOR;
	case BF_InverseSourceColor: return GL_ONE_MINUS_SRC_COLOR;
	case BF_SourceAlpha: return GL_SRC_ALPHA;
	case BF_InverseSourceAlpha: return GL_ONE_MINUS_SRC_ALPHA;
	case BF_DestAlpha: return GL_DST_ALPHA;
	case BF_InverseDestAlpha: return GL_ONE_MINUS_DST_ALPHA;
	case BF_DestColor: return GL_DST_COLOR;
	case BF_InverseDestColor: return GL_ONE_MINUS_DST_COLOR;
	case BF_ConstantBlendColor: return GL_CONSTANT_COLOR;
	default: return GL_ZERO;
	};
}

static GLenum TranslateUnrealSamplerAddress(ESamplerAddressMode AddressMode)
{
    switch(AddressMode)
    {
    case AM_Clamp: return GL_CLAMP_TO_EDGE;
    case AM_Mirror: return GL_MIRRORED_REPEAT;
    default: return GL_REPEAT;
    }
}


FSamplerStateRHIRef FES2RHI::CreateSamplerState(const FSamplerStateInitializerRHI& Initializer) 
{ 
	FES2SamplerState* SamplerState = new FES2SamplerState(Initializer);
	return SamplerState;
} 

FRasterizerStateRHIRef FES2RHI::CreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer) 
{ 
	FES2RasterizerState* RasterizerState = new FES2RasterizerState;
	*((FRasterizerStateInitializerRHI*)RasterizerState) = Initializer;
	return RasterizerState;
} 

FDepthStateRHIRef FES2RHI::CreateDepthState(const FDepthStateInitializerRHI& Initializer) 
{ 
	FES2DepthState *DepthState = new FES2DepthState;
	*((FDepthStateInitializerRHI *)DepthState) = Initializer;
	return DepthState; 
} 

FStencilStateRHIRef FES2RHI::CreateStencilState(const FStencilStateInitializerRHI& Initializer) 
{ 
	FES2StencilState *StencilState = new FES2StencilState;
	*((FStencilStateInitializerRHI *)StencilState) = Initializer;
	return StencilState;
} 

FBlendStateRHIRef FES2RHI::CreateBlendState(const FBlendStateInitializerRHI& Initializer) 
{ 
	FES2BlendState *BlendState = new FES2BlendState;
	*((FBlendStateInitializerRHI *)BlendState) = Initializer;
	return BlendState;
} 

void FES2RHI::SetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI ) 
{ 
	DYNAMIC_CAST_ES2RESOURCE( RasterizerState, NewState );
	// our ParamRef is just a copy of the initializer, so we can re-use the immediate function
	RHISetRasterizerStateImmediate(*NewState);
} 


void FES2RHI::SetRasterizerStateImmediate(const FRasterizerStateInitializerRHI& ImmediateState) 
{ 
	// Backface culling
	if ( ImmediateState.CullMode != GStateShadow.Rasterizer.CullMode )
	{
		GStateShadow.Rasterizer.CullMode = ImmediateState.CullMode;
		if (ImmediateState.CullMode == CM_None)
		{
			GLCHECK(glDisable(GL_CULL_FACE));
		}
		else
		{
			GLCHECK(glEnable(GL_CULL_FACE));
			GLCHECK(glFrontFace(TranslateUnrealCullMode(ImmediateState.CullMode)));
		}	
	}


	// Depth bias
	if( ImmediateState.DepthBias != GStateShadow.Rasterizer.DepthBias ||
		ImmediateState.SlopeScaleDepthBias != GStateShadow.Rasterizer.SlopeScaleDepthBias )
	{
		GStateShadow.Rasterizer.DepthBias = ImmediateState.DepthBias;
		GStateShadow.Rasterizer.SlopeScaleDepthBias = ImmediateState.SlopeScaleDepthBias;

		// From RHI.cpp
		extern FLOAT GDepthBiasOffset;
		const FLOAT BiasScale = FLOAT((1<<24)-1);

		if ( Abs(ImmediateState.SlopeScaleDepthBias) > 0.000001f || Abs(ImmediateState.DepthBias) > 0.000001f )
		{
			// bias (slope * NewState->SlopeScaleDepthBias + R * NewState->DepthBias
			GLCHECK( glPolygonOffset( ImmediateState.SlopeScaleDepthBias, (ImmediateState.DepthBias + GDepthBiasOffset) * BiasScale) );
			GLCHECK( glEnable( GL_POLYGON_OFFSET_FILL ) );
		}
		else
		{
			GLCHECK( glDisable( GL_POLYGON_OFFSET_FILL ) );
		}
	}
} 


void FES2RHI::SetMobileSimpleParams(EBlendMode InBlendMode)
{
	GShaderManager.SetMobileSimpleParams(InBlendMode);
}

void FES2RHI::SetMobileMaterialVertexParams(const FMobileMaterialVertexParams& InVertexParams)
{
	GShaderManager.SetMobileMaterialVertexParams(InVertexParams);
}

void FES2RHI::SetMobileMaterialPixelParams(const FMobileMaterialPixelParams& InPixelParams)
{
	GShaderManager.SetMobileMaterialPixelParams(InPixelParams);
}


void FES2RHI::SetMobileMeshVertexParams(const FMobileMeshVertexParams& InMeshParams)
{
	GShaderManager.SetMobileMeshVertexParams(InMeshParams);
}


void FES2RHI::SetMobileMeshPixelParams(const FMobileMeshPixelParams& InMeshParams)
{
	GShaderManager.SetMobileMeshPixelParams(InMeshParams);
}


FLOAT FES2RHI::GetMobilePercentColorFade(void)
{
	return GShaderManager.GetMobilePercentColorFade();
}


void FES2RHI::SetMobileFogParams(const UBOOL bInEnabled, const FLOAT InFogStart, const FLOAT InFogEnd, const FColor& InFogColor)
{
	GShaderManager.SetFog(bInEnabled, InFogStart, InFogEnd, InFogColor);
}

void FES2RHI::SetMobileHeightFogParams(const FHeightFogParams& Params)
{
	GShaderManager.SetHeightFogParams(Params);
}

void FES2RHI::SetMobileBumpOffsetParams(const UBOOL bInEnabled, const FLOAT InBumpEnd)
{
	GShaderManager.SetBumpOffset(bInEnabled, InBumpEnd);
}

void FES2RHI::SetMobileGammaCorrection(const UBOOL bInEnabled)
{
	GShaderManager.SetGammaCorrection(bInEnabled);
}

void FES2RHI::SetMobileTextureTransformOverride(TMatrix<3,3>& InOverrideTransform)
{
	GShaderManager.SetMobileTextureTransformOverride(InOverrideTransform);
}

void FES2RHI::SetMobileDistanceFieldParams(const struct FMobileDistanceFieldParams& Params)
{
	GShaderManager.SetMobileDistanceFieldParams(Params);
}

void FES2RHI::SetMobileColorGradingParams(const FMobileColorGradingParams& Params)
{
	GShaderManager.SetMobileColorGradingParams(Params);
}

void FES2RHI::ResetTrackedPrimitive()
{
	GShaderManager.ResetTrackedPrimitive();
}

void FES2RHI::CycleTrackedPrimitiveMode()
{
	GShaderManager.CycleTrackedPrimitiveMode();
}

void FES2RHI::IncrementTrackedPrimitive(const INT InDelta)
{
	GShaderManager.ChangeTrackedPrimitive(InDelta);
}

void FES2RHI::SetMobileTextureSamplerState(FPixelShaderRHIParamRef PixelShader,const INT MobileTextureUnit,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias, FLOAT LargestMip, FLOAT SmallestMip) 
{
	DYNAMIC_CAST_ES2RESOURCE( Texture, NewTexture );
	DYNAMIC_CAST_ES2RESOURCE( SamplerState, NewState );

	if (NewTexture != NULL && 
		(NewTexture->GetFormat() == PF_DXT1 || NewTexture->GetFormat() == PF_DXT3 || NewTexture->GetFormat() == PF_DXT5 || NewTexture->GetFormat() == PF_A8R8G8B8 || NewTexture->GetFormat() == PF_R5G6B5 ||
		 NewTexture->GetFormat() == PF_G8 || NewTexture->GetFormat() == PF_ShadowDepth || NewTexture->GetFormat() == PF_DepthStencil) &&
		NewTexture->GetTextureType() == GL_TEXTURE_2D)
	{
		const EPixelFormat TextureFormat = NewTexture->GetFormat();
		GLenum TextureType = NewTexture->GetTextureType();

		// get the texture unit from what's passed in if the special hint is given
		UINT TextureUnit = MobileTextureUnit;
		// make the specified texture active and bound
		GShaderManager.SetActiveAndBoundTexture(TextureUnit, NewTexture->GetTextureName(), TextureType, TextureFormat);

		GLenum Address = TranslateUnrealSamplerAddress(NewState->AddressU);
        if ( NewTexture->GetAddressS() != Address )
        {
            NewTexture->SetAddressS(Address);
            GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_WRAP_S, Address));
        }

		Address = TranslateUnrealSamplerAddress(NewState->AddressV);
        if ( NewTexture->GetAddressT() != Address )
        {
            NewTexture->SetAddressT(Address);
            GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_WRAP_T, Address));
        }

		if ( NewTexture->GetFilter() != NewState->Filter )
		{
			NewTexture->SetFilter(NewState->Filter);
			switch ( NewState->Filter )
			{
				case SF_Point:
				{
					// only set it if we could have set it away - helps with platforms that don't support it
					if (GSystemSettings.MaxAnisotropy > 1)
					{
						GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1));
					}
					GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
					GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
					break;
				}
				case SF_Bilinear:
				{
					// only set it if we could have set it away - helps with platforms that don't support it
					if (GSystemSettings.MaxAnisotropy > 1)
					{
						GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1));
					}
					GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MIN_FILTER, NewTexture->GetMipCount() > 1 ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR));
					GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
					break;
				}
				case SF_Trilinear:
				{
					// only set it if we could have set it away - helps with platforms that don't support it
					if (GSystemSettings.MaxAnisotropy > 1)
					{
						GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1));
					}
					GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MIN_FILTER, NewTexture->GetMipCount() > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
					GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
					break;
				}
				case SF_AnisotropicPoint:
				{
					// only set it if we could have set it away - helps with platforms that don't support it
					if (GSystemSettings.MaxAnisotropy > 1)
					{
						GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAX_ANISOTROPY_EXT, Max(GSystemSettings.MaxAnisotropy, 1)));
					}
					GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MIN_FILTER, NewTexture->GetMipCount() > 1 ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR));
					GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
					break;
				}
				case SF_AnisotropicLinear:
				{
					// only set it if we could have set it away - helps with platforms that don't support it
					if (GSystemSettings.MaxAnisotropy > 1)
					{
						GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAX_ANISOTROPY_EXT, Max(GSystemSettings.MaxAnisotropy, 1)));
					}
					GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MIN_FILTER, NewTexture->GetMipCount() > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
					GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
					break;
				}
			}
		}
	}
} 

void FES2RHI::SetSamplerStateOnly(FPixelShaderRHIParamRef PixelShaderRHI,UINT /*SamplerIndex*/,FSamplerStateRHIParamRef NewStateRHI)
{
	// Not implemented yet
	check(0);
}

void FES2RHI::SetTextureParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	// Not implemented yet
	check(0);
}

void FES2RHI::SetSurfaceParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,FSurfaceRHIParamRef NewTextureRHI)
{
	// Not implemented yet
	check(0);
}

void FES2RHI::SetSamplerState(FPixelShaderRHIParamRef PixelShader,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewState,FTextureRHIParamRef NewTexture,FLOAT MipBias, FLOAT LargestMip, FLOAT SmallestMip, UBOOL bForceLinearMinFilter) 
{
	// any textures passed through this function just are assumed to go to a single base texture in the shader
	RHISetMobileTextureSamplerState(PixelShader, Base_MobileTexture, NewState, NewTexture, MipBias, LargestMip, SmallestMip);
}

void FES2RHI::SetSamplerState(FVertexShaderRHIParamRef PixelShader,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewState,FTextureRHIParamRef NewTexture,FLOAT MipBias, FLOAT LargestMip, FLOAT SmallestMip, UBOOL bForceLinearMinFilter) 
{
	// Not implemented
	check(0);
}

void FES2RHI::SetVertexTexture(UINT SamplerIndex,FTextureRHIParamRef NewTextureRHI)
{
	// Not implemented
	check(0);
}

void FES2RHI::SetDepthState(FDepthStateRHIParamRef NewStateRHI) 
{ 
	DYNAMIC_CAST_ES2RESOURCE( DepthState, NewState );

	UBOOL bEnableDepthWrite = NewState->bEnableDepthWrite;
	ECompareFunction DepthTest = NewState->DepthTest;

	// Are we currently using a dummy DepthStencil buffer (the user set NULL and we're overriding that)?
	if ( GStateShadow.bIsUsingDummyDepthStencilBuffer )
	{
		bEnableDepthWrite = FALSE;
		DepthTest = CF_Always;
	}

	if( bEnableDepthWrite == GStateShadow.Depth.bEnableDepthWrite &&
		DepthTest == GStateShadow.Depth.DepthTest )
	{
		return;
	}
	GStateShadow.Depth.bEnableDepthWrite = bEnableDepthWrite;
	GStateShadow.Depth.DepthTest = DepthTest;

	GLCHECK( glDepthMask( bEnableDepthWrite ? GL_TRUE : GL_FALSE ) );
	
	GLenum CompareFunc[] = { GL_LESS, GL_LEQUAL, GL_GREATER, GL_GEQUAL, GL_EQUAL, GL_NOTEQUAL, GL_NEVER, GL_ALWAYS };
	GLCHECK( glDepthFunc( CompareFunc[ DepthTest ] ) );
} 

void FES2RHI::SetStencilState(FStencilStateRHIParamRef NewStateRHI) 
{ 
	DYNAMIC_CAST_ES2RESOURCE( StencilState, NewState );

	UBOOL bEnableFrontFaceStencil = NewState->bEnableFrontFaceStencil;
	UBOOL bEnableBackFaceStencil = NewState->bEnableBackFaceStencil;

	// Are we currently using a dummy DepthStencil buffer (the user set NULL and we're overriding that)?
	if ( GStateShadow.bIsUsingDummyDepthStencilBuffer )
	{
		bEnableFrontFaceStencil = FALSE;
		bEnableBackFaceStencil = FALSE;
	}

	if (bEnableFrontFaceStencil || bEnableBackFaceStencil)
	{
		GLCHECK(glEnable(GL_STENCIL_TEST));
        GLCHECK(glStencilMask(NewState->StencilWriteMask));
		if (NewState->bEnableBackFaceStencil)
		{
            GLCHECK(glStencilFuncSeparate(GL_FRONT, TranslateUnrealStencilFunction(NewState->FrontFaceStencilTest), NewState->StencilRef, NewState->StencilReadMask));
            GLCHECK(glStencilOpSeparate(GL_FRONT,
                TranslateUnrealStencilOp(NewState->FrontFaceStencilFailStencilOp),
                TranslateUnrealStencilOp(NewState->FrontFaceDepthFailStencilOp), 
                TranslateUnrealStencilOp(NewState->FrontFacePassStencilOp)));

            GLCHECK(glStencilFuncSeparate(GL_BACK, TranslateUnrealStencilFunction(NewState->BackFaceStencilTest), NewState->StencilRef, NewState->StencilReadMask));
			GLCHECK(glStencilOpSeparate(GL_BACK,
				TranslateUnrealStencilOp(NewState->BackFaceStencilFailStencilOp),
				TranslateUnrealStencilOp(NewState->BackFaceDepthFailStencilOp), 
				TranslateUnrealStencilOp(NewState->BackFacePassStencilOp)));
		}
		else
		{
            GLCHECK(glStencilFunc(TranslateUnrealStencilFunction(NewState->FrontFaceStencilTest), NewState->StencilRef, NewState->StencilReadMask));
            GLCHECK(glStencilOp(
                TranslateUnrealStencilOp(NewState->FrontFaceStencilFailStencilOp),
                TranslateUnrealStencilOp(NewState->FrontFaceDepthFailStencilOp), 
                TranslateUnrealStencilOp(NewState->FrontFacePassStencilOp)));
		}
	}
	else
	{
		GLCHECK(glDisable(GL_STENCIL_TEST));
	}
} 

void FES2RHI::SetBlendState(FBlendStateRHIParamRef NewStateRHI) 
{
	DYNAMIC_CAST_ES2RESOURCE( BlendState, NewState );

	// will it blend?
	UBOOL bIsAlphaBlendEnabled = (NewState->ColorDestBlendFactor != BF_Zero || NewState->ColorSourceBlendFactor != BF_One);

	if( NewState->ColorBlendOperation != GStateShadow.Blend.ColorBlendOperation ||
		NewState->ColorSourceBlendFactor != GStateShadow.Blend.ColorSourceBlendFactor ||
		NewState->ColorDestBlendFactor != GStateShadow.Blend.ColorDestBlendFactor ||
		NewState->AlphaBlendOperation != GStateShadow.Blend.AlphaBlendOperation ||
		NewState->AlphaSourceBlendFactor != GStateShadow.Blend.AlphaSourceBlendFactor ||
		NewState->AlphaDestBlendFactor != GStateShadow.Blend.AlphaDestBlendFactor ||
		NewState->ConstantBlendColor != GStateShadow.Blend.ConstantBlendColor)
	{
		GStateShadow.Blend.ColorBlendOperation = NewState->ColorBlendOperation;
		GStateShadow.Blend.ColorSourceBlendFactor = NewState->ColorSourceBlendFactor;
		GStateShadow.Blend.ColorDestBlendFactor = NewState->ColorDestBlendFactor;
		GStateShadow.Blend.AlphaBlendOperation = NewState->AlphaBlendOperation;
		GStateShadow.Blend.AlphaSourceBlendFactor = NewState->AlphaSourceBlendFactor;
		GStateShadow.Blend.AlphaDestBlendFactor = NewState->AlphaDestBlendFactor;
		GStateShadow.Blend.ConstantBlendColor = NewState->ConstantBlendColor;

		if (bIsAlphaBlendEnabled)
		{
			GLCHECK(glEnable(GL_BLEND));

			if (NewState->ColorSourceBlendFactor == BF_ConstantBlendColor
				|| NewState->ColorDestBlendFactor == BF_ConstantBlendColor)
			{
				GLCHECK(glBlendColor(NewState->ConstantBlendColor.R, NewState->ConstantBlendColor.G, NewState->ConstantBlendColor.B, NewState->ConstantBlendColor.A));
			}

			GLCHECK(glBlendFuncSeparate(
				TranslateUnrealBlendFactor(NewState->ColorSourceBlendFactor),
				TranslateUnrealBlendFactor(NewState->ColorDestBlendFactor), 
				TranslateUnrealBlendFactor(NewState->AlphaSourceBlendFactor), 
				TranslateUnrealBlendFactor(NewState->AlphaDestBlendFactor)));

			GLCHECK(glBlendEquationSeparate(TranslateUnrealBlendOp(NewState->ColorBlendOperation), TranslateUnrealBlendOp(NewState->AlphaBlendOperation)));
		}
		else
		{
			GLCHECK(glDisable(GL_BLEND));
		}
	}

	EBlendMode BlendMode = BLEND_Opaque;
	if ( bIsAlphaBlendEnabled )
	{
		UBOOL bIsAdditive = (NewState->ColorSourceBlendFactor == BF_One && NewState->ColorDestBlendFactor == BF_One);
		if ( bIsAdditive )
		{
			BlendMode = BLEND_Additive;
		}
		else
		{
			BlendMode = BLEND_Translucent;
		}
	}
	else
	{
		BlendMode = BLEND_Opaque;
	}
	if ( NewState->AlphaTest != CF_Always )
	{
		BlendMode = BLEND_Masked;
	}
	GShaderManager.SetMobileBlendMode( BlendMode );	

	// On platforms that don't support discard enable blending so a faked alpha kill can occur
	// This has to occur outside of the alphatest state check b/c other blend modes can modify this state
	if (!GMobileAllowShaderDiscard)	
	{
		if (BlendMode == BLEND_Masked)
		{
			// Ensure blending is on
			GLCHECK(glEnable(GL_BLEND));

			// Check necessary state for changes
			if (GStateShadow.Blend.ColorBlendOperation != BO_Add
				|| GStateShadow.Blend.ColorSourceBlendFactor != BF_SourceAlpha
				|| GStateShadow.Blend.ColorDestBlendFactor != BF_InverseSourceAlpha)
			{
				GStateShadow.Blend.ColorBlendOperation = BO_Add;
				GStateShadow.Blend.ColorSourceBlendFactor = BF_SourceAlpha;
				GStateShadow.Blend.ColorDestBlendFactor = BF_InverseSourceAlpha;

				GLCHECK(glBlendFuncSeparate(
					TranslateUnrealBlendFactor(GStateShadow.Blend.ColorSourceBlendFactor),
					TranslateUnrealBlendFactor(GStateShadow.Blend.ColorDestBlendFactor), 
					TranslateUnrealBlendFactor(GStateShadow.Blend.AlphaSourceBlendFactor), 
					TranslateUnrealBlendFactor(GStateShadow.Blend.AlphaDestBlendFactor)));

				GLCHECK(glBlendEquationSeparate(TranslateUnrealBlendOp(GStateShadow.Blend.ColorBlendOperation), TranslateUnrealBlendOp(GStateShadow.Blend.AlphaBlendOperation)));
			}
		}
		else if (!bIsAlphaBlendEnabled)
		{
			// If not otherwise blending, turn off blending
			GLCHECK(glDisable(GL_BLEND));
		}
	}

	if( NewState->AlphaTest != GStateShadow.Blend.AlphaTest ||
		NewState->AlphaRef != GStateShadow.Blend.AlphaRef )
	{
		GStateShadow.Blend.AlphaTest = NewState->AlphaTest;
		GStateShadow.Blend.AlphaRef = NewState->AlphaRef;

		if (NewState->AlphaTest != CF_Always )
		{
			// set a delayed-set shader parameter for the alpha ref value
			FLOAT AlphaRef = (FLOAT)NewState->AlphaRef * (1.0f / 255.0f);
			GShaderManager.SetAlphaTest(TRUE, AlphaRef);
		}
		else
		{
			GShaderManager.SetAlphaTest(FALSE, 0.0f);
		}
	}
} 

void FES2RHI::SetColorWriteEnable(UBOOL bEnable) 
{ 
	if( GStateShadow.ColorWriteEnable == bEnable )
	{
		return;
	}
	GStateShadow.ColorWriteEnable = bEnable;
	GLboolean bEnableWrites = bEnable ? GL_TRUE : GL_FALSE;
	GLCHECK( glColorMask( bEnableWrites, bEnableWrites, bEnableWrites, bEnableWrites ) );
} 

void FES2RHI::SetColorWriteMask(UINT ColorWriteMask) 
{ 
	if( GStateShadow.ColorWriteMask == ColorWriteMask )
	{
		return;
	}
	GStateShadow.ColorWriteMask = ColorWriteMask;
	GLboolean bEnableRed = (ColorWriteMask & CW_RED) ? GL_TRUE : GL_FALSE;
	GLboolean bEnableGreen = (ColorWriteMask & CW_GREEN) ? GL_TRUE : GL_FALSE;
	GLboolean bEnableBlue = (ColorWriteMask & CW_BLUE) ? GL_TRUE : GL_FALSE;
	GLboolean bEnableAlpha = (ColorWriteMask & CW_ALPHA) ? GL_TRUE : GL_FALSE;
	GLCHECK( glColorMask( bEnableRed, bEnableGreen, bEnableBlue, bEnableAlpha ) );
} 

#endif
