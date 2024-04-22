/*=============================================================================
	OpenGLState.h: OpenGL state definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

class FOpenGLSamplerState : public FRefCountedObject, public TDynamicRHIResource<RRT_SamplerState>
{
public:

	GLenum MagFilter;
	GLenum MinFilter;
	GLenum AddressU;
	GLenum AddressV;
	GLenum AddressW;
	FLOAT MaxAnisotropy;
	INT MipMapLODBias;

	FOpenGLSamplerState()
	:	MagFilter(GL_LINEAR)
	,	MinFilter(GL_LINEAR)
	,	AddressU(GL_CLAMP_TO_EDGE)
	,	AddressV(GL_CLAMP_TO_EDGE)
	,	AddressW(GL_CLAMP_TO_EDGE)
	,	MaxAnisotropy(1.0f)
	,	MipMapLODBias(0)
	{
	}
};

class FOpenGLRasterizerState : public FRefCountedObject, public TDynamicRHIResource<RRT_RasterizerState>
{
public:

	GLenum FillMode;
	GLenum CullMode;
	FLOAT DepthBias;
	FLOAT SlopeScaleDepthBias;

	FOpenGLRasterizerState()
	:	FillMode(GL_FILL)
	,	CullMode(GL_NONE)
	,	DepthBias(0.0f)
	,	SlopeScaleDepthBias(0.0f)
	{
	}
};

class FOpenGLDepthState : public FRefCountedObject, public TDynamicRHIResource<RRT_DepthState>
{
public:

	UBOOL bZEnable;
	UBOOL bZWriteEnable;
	GLenum ZFunc;

	FOpenGLDepthState()
	:	bZEnable(GL_FALSE)
	,	bZWriteEnable(GL_TRUE)
	,	ZFunc(GL_LESS)
	{
	}
};

class FOpenGLStencilState : public FRefCountedObject, public TDynamicRHIResource<RRT_StencilState>
{
public:

	UBOOL bStencilEnable;
	UBOOL bTwoSidedStencilMode;
	GLenum StencilFunc;
	GLenum StencilFail;
	GLenum StencilZFail;
	GLenum StencilPass;
	GLenum CCWStencilFunc;
	GLenum CCWStencilFail;
	GLenum CCWStencilZFail;
	GLenum CCWStencilPass;
	DWORD StencilReadMask;
	DWORD StencilWriteMask;
	DWORD StencilRef;

	FOpenGLStencilState()
	:	bStencilEnable(GL_FALSE)
	,	bTwoSidedStencilMode(GL_FALSE)
	,	StencilFunc(GL_ALWAYS)
	,	StencilFail(GL_KEEP)
	,	StencilZFail(GL_KEEP)
	,	StencilPass(GL_KEEP)
	,	CCWStencilFunc(GL_ALWAYS)
	,	CCWStencilFail(GL_KEEP)
	,	CCWStencilZFail(GL_KEEP)
	,	CCWStencilPass(GL_KEEP)
	,	StencilReadMask(0xFFFFFFFF)
	,	StencilWriteMask(0xFFFFFFFF)
	,	StencilRef(0)
	{
	}
};

class FOpenGLBlendState : public FRefCountedObject, public TDynamicRHIResource<RRT_BlendState>
{
public:

	UBOOL bAlphaBlendEnable;
	GLenum ColorBlendOperation;
	GLenum ColorSourceBlendFactor;
	GLenum ColorDestBlendFactor;
	UBOOL bSeparateAlphaBlendEnable;
	GLenum AlphaBlendOperation;
	GLenum AlphaSourceBlendFactor;
	GLenum AlphaDestBlendFactor;
	UBOOL bAlphaTestEnable;
	GLenum AlphaFunc;
	DWORD AlphaRef;

	FOpenGLBlendState()
	:	bAlphaBlendEnable(GL_FALSE)
	,	ColorBlendOperation(GL_FUNC_ADD)
	,	ColorSourceBlendFactor(GL_ONE)
	,	ColorDestBlendFactor(GL_ZERO)
	,	bSeparateAlphaBlendEnable(GL_FALSE)
	,	AlphaBlendOperation(GL_FUNC_ADD)
	,	AlphaSourceBlendFactor(GL_ONE)
	,	AlphaDestBlendFactor(GL_ZERO)
	,	bAlphaTestEnable(GL_FALSE)
	,	AlphaFunc(GL_ALWAYS)
	,	AlphaRef(0)
	{
	}
};

struct FTextureStage
{
	GLenum Target;
	GLuint Resource;
	INT MipMapLODBias;

	FTextureStage()
	:	Target(GL_NONE)
	,	Resource(0)
	,	MipMapLODBias(0)
	{
	}
};

struct FOpenGLStateCache
{
	FOpenGLRasterizerState RasterizerState;
	FOpenGLDepthState DepthState;
	FOpenGLStencilState StencilState;
	FOpenGLBlendState BlendState;
	FTextureStage Textures[16];
	GLuint Framebuffer;
	DWORD RenderTargetWidth;
	DWORD RenderTargetHeight;
	GLuint OcclusionQuery;
	GLuint Program;
	GLenum ActiveTexture;
	UBOOL bScissorEnabled;
	FIntRect Scissor;
	FIntRect Viewport;
	FLOAT MinZ;
	FLOAT MaxZ;
	UINT ColorWriteEnabled;
	FLinearColor	ClearColor;
	FLOAT			ClearDepth;
	DWORD			ClearStencil;

	FOpenGLStateCache()
	:	Framebuffer(0)
	,	RenderTargetWidth(0)
	,	RenderTargetHeight(0)
	,	OcclusionQuery(0)
	,	Program(0)
	,	ActiveTexture(GL_TEXTURE0)
	,	bScissorEnabled(FALSE)
	,	MinZ(0.0f)
	,	MaxZ(1.0f)
	,	ColorWriteEnabled(CW_RGBA)
	,	ClearColor(0.0f,0.0f,0.0f,0.0f)
	,	ClearDepth(1.0f)
	,	ClearStencil(0)
	{
		Scissor.Min.X = Scissor.Min.Y = Scissor.Max.X = Scissor.Max.Y = 0;
		Viewport.Min.X = Viewport.Min.Y = Viewport.Max.X = Viewport.Max.Y = 0;
	}
};
