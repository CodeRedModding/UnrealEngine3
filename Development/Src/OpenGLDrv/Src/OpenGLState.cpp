/*=============================================================================
	OpenGLState.cpp: OpenGL state implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

static GLenum TranslateAddressMode(ESamplerAddressMode AddressMode)
{
	switch(AddressMode)
	{
	case AM_Clamp: return GL_CLAMP_TO_EDGE;
	case AM_Mirror: return GL_MIRRORED_REPEAT;
	default: return GL_REPEAT;
	};
}

static INT TranslateMipBias(ESamplerMipMapLODBias MipBias)
{
	// @todo opengl: MIPBIAS_Get4
	return INT(MipBias);
}

static GLenum TranslateCullMode(ERasterizerCullMode CullMode)
{
	switch(CullMode)
	{
	case CM_CW: return GL_BACK;
	case CM_CCW: return GL_FRONT;
	default: return GL_NONE;
	};
}

static GLenum TranslateFillMode(ERasterizerFillMode FillMode)
{
	switch(FillMode)
	{
	case FM_Point: return GL_POINT;
	case FM_Wireframe: return GL_LINE;
	default: return GL_FILL;
	};
}

static GLenum TranslateCompareFunction(ECompareFunction CompareFunction)
{
	switch(CompareFunction)
	{
	case CF_Less: return GL_LESS;
	case CF_LessEqual: return GL_LEQUAL;
	case CF_Greater: return GL_GREATER;
	case CF_GreaterEqual: return GL_GEQUAL;
	case CF_Equal: return GL_EQUAL;
	case CF_NotEqual: return GL_NOTEQUAL;
	case CF_Never: return GL_NEVER;
	default: return GL_ALWAYS;
	};
}

static GLenum TranslateStencilOp(EStencilOp StencilOp)
{
	switch(StencilOp)
	{
	case SO_Zero: return GL_ZERO;
	case SO_Replace: return GL_REPLACE;
	case SO_SaturatedIncrement: return GL_INCR;
	case SO_SaturatedDecrement: return GL_DECR;
	case SO_Invert: return GL_INVERT;
	case SO_Increment: return GL_INCR_WRAP;
	case SO_Decrement: return GL_DECR_WRAP;
	default: return GL_KEEP;
	};
}

static GLenum TranslateBlendOp(EBlendOperation BlendOp)
{
	switch(BlendOp)
	{
	case BO_Subtract: return GL_FUNC_SUBTRACT;
	case BO_Min: return GL_MIN;
	case BO_Max: return GL_MAX;
	case BO_ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
	default: return GL_FUNC_ADD;
	};
}

static GLenum TranslateBlendFactor(EBlendFactor BlendFactor)
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
	default: return GL_ZERO;
	};
}

FSamplerStateRHIRef FOpenGLDynamicRHI::CreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
	FOpenGLSamplerState* SamplerState = new FOpenGLSamplerState;
	SamplerState->AddressU = TranslateAddressMode(Initializer.AddressU);
	SamplerState->AddressV = TranslateAddressMode(Initializer.AddressV);
	SamplerState->AddressW = TranslateAddressMode(Initializer.AddressW);
	SamplerState->MipMapLODBias = TranslateMipBias(Initializer.MipBias);
	FLOAT MaxAnisotropy = Clamp(GSystemSettings.MaxAnisotropy,1,16);
	switch(Initializer.Filter)
	{
	case SF_AnisotropicLinear:
		SamplerState->MagFilter	= GL_LINEAR;
		SamplerState->MinFilter	= GL_LINEAR_MIPMAP_LINEAR;
		SamplerState->MaxAnisotropy = MaxAnisotropy;
		break;
	case SF_AnisotropicPoint:
		SamplerState->MagFilter	= GL_LINEAR;
		SamplerState->MinFilter	= GL_NEAREST_MIPMAP_NEAREST;
		SamplerState->MaxAnisotropy = MaxAnisotropy;
		break;
	case SF_Trilinear:
		SamplerState->MagFilter	= GL_LINEAR;
		SamplerState->MinFilter	= GL_LINEAR_MIPMAP_LINEAR;
		SamplerState->MaxAnisotropy = 1.0f;
		break;
	case SF_Bilinear:
		SamplerState->MagFilter	= GL_LINEAR;
		SamplerState->MinFilter	= GL_LINEAR_MIPMAP_NEAREST;
		SamplerState->MaxAnisotropy = 1.0f;
		break;
	case SF_Point:
		SamplerState->MagFilter	= GL_NEAREST;
		SamplerState->MinFilter	= GL_NEAREST_MIPMAP_NEAREST;
		SamplerState->MaxAnisotropy = MaxAnisotropy;
		break;
	}
	return SamplerState;
}

FRasterizerStateRHIRef FOpenGLDynamicRHI::CreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	FOpenGLRasterizerState* RasterizerState = new FOpenGLRasterizerState;
	RasterizerState->CullMode = TranslateCullMode(Initializer.CullMode);
	RasterizerState->FillMode = TranslateFillMode(Initializer.FillMode);
	RasterizerState->DepthBias = Initializer.DepthBias;
	RasterizerState->SlopeScaleDepthBias = Initializer.SlopeScaleDepthBias;
	return RasterizerState;
}

/**
* Sets a rasterizer state using direct values instead of a cached object.  
* This is preferable to calling RHISetRasterizerState(Context, RHICreateRasterizerState(Initializer)) 
* since at least some hardware platforms can optimize this path.
*/
void FOpenGLDynamicRHI::SetRasterizerStateImmediate(const FRasterizerStateInitializerRHI &ImmediateState)
{
	// TODO: determine the frequency of this call
	FRasterizerStateRHIRef RasterizerState = CreateRasterizerState(ImmediateState);
	SetRasterizerState(RasterizerState);
}

FDepthStateRHIRef FOpenGLDynamicRHI::CreateDepthState(const FDepthStateInitializerRHI& Initializer)
{
	FOpenGLDepthState* DepthState = new FOpenGLDepthState;
	DepthState->bZEnable = Initializer.DepthTest != CF_Always || Initializer.bEnableDepthWrite;
	DepthState->bZWriteEnable = Initializer.bEnableDepthWrite;
	DepthState->ZFunc = TranslateCompareFunction(Initializer.DepthTest);
	return DepthState;
}

FStencilStateRHIRef FOpenGLDynamicRHI::CreateStencilState(const FStencilStateInitializerRHI& Initializer)
{
	FOpenGLStencilState* StencilState = new FOpenGLStencilState;
	StencilState->bStencilEnable = Initializer.bEnableFrontFaceStencil || Initializer.bEnableBackFaceStencil;
	StencilState->bTwoSidedStencilMode = Initializer.bEnableBackFaceStencil;
	StencilState->StencilFunc = TranslateCompareFunction(Initializer.FrontFaceStencilTest);
	StencilState->StencilFail = TranslateStencilOp(Initializer.FrontFaceStencilFailStencilOp);
	StencilState->StencilZFail = TranslateStencilOp(Initializer.FrontFaceDepthFailStencilOp);
	StencilState->StencilPass = TranslateStencilOp(Initializer.FrontFacePassStencilOp);
	StencilState->CCWStencilFunc = TranslateCompareFunction(Initializer.BackFaceStencilTest);
	StencilState->CCWStencilFail = TranslateStencilOp(Initializer.BackFaceStencilFailStencilOp);
	StencilState->CCWStencilZFail = TranslateStencilOp(Initializer.BackFaceDepthFailStencilOp);
	StencilState->CCWStencilPass = TranslateStencilOp(Initializer.BackFacePassStencilOp);
	StencilState->StencilReadMask = Initializer.StencilReadMask;
	StencilState->StencilWriteMask = Initializer.StencilWriteMask;
	StencilState->StencilRef = Initializer.StencilRef;
	return StencilState;
}


FBlendStateRHIRef FOpenGLDynamicRHI::CreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
	FOpenGLBlendState* BlendState = new FOpenGLBlendState;
	BlendState->bAlphaBlendEnable = 
		Initializer.ColorBlendOperation != BO_Add || Initializer.ColorDestBlendFactor != BF_Zero || Initializer.ColorSourceBlendFactor != BF_One ||
		Initializer.AlphaBlendOperation != BO_Add || Initializer.AlphaDestBlendFactor != BF_Zero || Initializer.AlphaSourceBlendFactor != BF_One;
	BlendState->ColorBlendOperation = TranslateBlendOp(Initializer.ColorBlendOperation);
	BlendState->ColorSourceBlendFactor = TranslateBlendFactor(Initializer.ColorSourceBlendFactor);
	BlendState->ColorDestBlendFactor = TranslateBlendFactor(Initializer.ColorDestBlendFactor);
	BlendState->bSeparateAlphaBlendEnable =
		Initializer.AlphaDestBlendFactor != Initializer.ColorDestBlendFactor ||
		Initializer.AlphaSourceBlendFactor != Initializer.ColorSourceBlendFactor;
	BlendState->AlphaBlendOperation = TranslateBlendOp(Initializer.AlphaBlendOperation);
	BlendState->AlphaSourceBlendFactor = TranslateBlendFactor(Initializer.AlphaSourceBlendFactor);
	BlendState->AlphaDestBlendFactor = TranslateBlendFactor(Initializer.AlphaDestBlendFactor);
	BlendState->bAlphaTestEnable = Initializer.AlphaTest != CF_Always;
	BlendState->AlphaFunc = TranslateCompareFunction(Initializer.AlphaTest);
	BlendState->AlphaRef = Initializer.AlphaRef;
	return BlendState;
}
