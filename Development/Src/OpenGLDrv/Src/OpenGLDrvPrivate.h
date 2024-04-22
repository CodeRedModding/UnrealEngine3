/*=============================================================================
	OpenGLDrvPrivate.h: Private OpenGL RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __OPENGLDRVPRIVATE_H__
#define __OPENGLDRVPRIVATE_H__

// Dependencies
#include "Engine.h"
#include "OpenGLDrv.h"

/**
 * The OpenGL RHI stats.
 */
enum EOpenGLRHIStats
{
	STAT_OpenGLPresentTime = STAT_OpenGLRHIFirstStat,
	STAT_OpenGLDrawPrimitiveCalls,
	STAT_OpenGLTriangles,
	STAT_OpenGLLines,
	STAT_OpenGLCreateTextureTime,
	STAT_OpenGLLockTextureTime,
	STAT_OpenGLUnlockTextureTime,
	STAT_OpenGLCopyTextureTime,
	STAT_OpenGLCopyMipToMipAsyncTime,
	STAT_OpenGLUploadTextureMipTime,
	STAT_OpenGLCreateBoundShaderStateTime,
	STAT_OpenGLConstantBufferUpdateTime,
};

inline UBOOL FindInternalFormatAndType(DWORD InFormat, GLenum &InternalFormat, GLenum &Type, UBOOL bSRGB)
{
	switch(InFormat)
	{
	case PF_A32B32G32R32F:
		InternalFormat = bSRGB ? GL_NONE : GL_RGBA32F_ARB;
		Type = GL_FLOAT;
		return TRUE;
	case PF_A8R8G8B8:
		InternalFormat = bSRGB ? GL_SRGB8_ALPHA8_EXT : GL_RGBA8;
		Type = GL_UNSIGNED_INT_8_8_8_8_REV;
		return TRUE;
	case PF_DXT1:
		InternalFormat = bSRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		Type = GL_UNSIGNED_BYTE;
		return TRUE;
	case PF_DXT3:
		InternalFormat = bSRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		Type = GL_UNSIGNED_BYTE;
		return TRUE;
	case PF_DXT5:
		InternalFormat = bSRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		Type = GL_UNSIGNED_BYTE;
		return TRUE;
	case PF_G8:
		InternalFormat = bSRGB ? GL_RGB8 : GL_LUMINANCE8;
		Type = GL_UNSIGNED_BYTE;
		return TRUE;
	case PF_DepthStencil:
		InternalFormat = bSRGB ? GL_NONE : GL_DEPTH24_STENCIL8_EXT;
		Type = GL_UNSIGNED_INT_24_8_EXT;
		return TRUE;
	case PF_D24:
	case PF_ShadowDepth:
	case PF_FilteredShadowDepth:
		InternalFormat = bSRGB ? GL_NONE : GL_DEPTH_COMPONENT24_ARB;
		Type = GL_UNSIGNED_INT;
		return TRUE;
	case PF_R32F:
		InternalFormat = bSRGB ? GL_NONE : GL_R32F;
		Type = GL_FLOAT;
		return TRUE;
	case PF_G16R16:
		InternalFormat = bSRGB ? GL_RGBA16 : GL_RG16;
		Type = GL_UNSIGNED_SHORT;
		return TRUE;
	case PF_G16R16F:
	case PF_G16R16F_FILTER:
		InternalFormat = bSRGB ? GL_RGBA16F_ARB : GL_RG16F;
		Type = GL_HALF_FLOAT_ARB;
		return TRUE;
	case PF_G32R32F:
		InternalFormat = bSRGB ? GL_RGBA32F_ARB : GL_RG32F;
		Type = GL_FLOAT;
		return TRUE;
	case PF_A2B10G10R10:
		InternalFormat = bSRGB ? GL_NONE : GL_RGB10_A2;
		Type = GL_UNSIGNED_INT_2_10_10_10_REV;
		return TRUE;
	case PF_A16B16G16R16:
		InternalFormat = bSRGB ? GL_NONE : GL_RGBA16F_ARB;
		Type = GL_HALF_FLOAT_ARB;
		return TRUE;
	case PF_R16F:
	case PF_R16F_FILTER:
		InternalFormat = bSRGB ? GL_NONE : GL_R16F;
		Type = GL_HALF_FLOAT_ARB;
		return TRUE;
	case PF_FloatRGB:
		InternalFormat = bSRGB ? GL_NONE : GL_RGB16F_ARB;
		Type = GL_HALF_FLOAT_ARB;
		return TRUE;
	case PF_FloatRGBA:
		InternalFormat = bSRGB ? GL_NONE : GL_RGBA16F_ARB;
		Type = GL_HALF_FLOAT_ARB;
		return TRUE;
	case PF_V8U8:
		InternalFormat = bSRGB ? GL_NONE : GL_RG8;
		Type = GL_BYTE;
		return TRUE;
	default:
		return FALSE;
	}
}

inline DWORD FindMaxMipmapLevel(DWORD Size)
{
	DWORD MipCount = 1;
	while( Size >>= 1 )
	{
		MipCount++;
	}
	return MipCount;
}

inline DWORD FindMaxMipmapLevel(DWORD Width, DWORD Height)
{
	return FindMaxMipmapLevel((Width > Height) ? Width : Height);
}

inline DWORD FindMaxMipmapLevel(DWORD Width, DWORD Height, DWORD Depth)
{
	return FindMaxMipmapLevel((Width > Height) ? Width : Height, Depth);
}

inline void FindPrimitiveType(UINT InPrimitiveType, UINT InNumPrimitives, GLenum &DrawMode, GLsizei &NumElements)
{
	DrawMode = GL_TRIANGLES;
	NumElements = InNumPrimitives;

	switch (InPrimitiveType)
	{
	case PT_TriangleList:
		DrawMode = GL_TRIANGLES;
		NumElements = InNumPrimitives * 3;
		break;
	case PT_TriangleStrip:
		DrawMode = GL_TRIANGLE_STRIP;
		NumElements = InNumPrimitives + 2;
		break;
	case PT_LineList:
		DrawMode = GL_LINES;
		NumElements = InNumPrimitives * 2;
		break;
	case PT_QuadList:
		DrawMode = GL_QUADS;
		NumElements = InNumPrimitives * 4;
		break;
	default:
		appErrorf(TEXT("Unsupported primitive type %u"), InPrimitiveType);
		break;
	}
}

inline UINT FindUniformElementSize(GLenum UniformType)
{
	switch (UniformType)
	{
	case GL_FLOAT:
		return sizeof(FLOAT);
	case GL_FLOAT_VEC2:
		return sizeof(FLOAT) * 2;
	case GL_FLOAT_VEC3:
		return sizeof(FLOAT) * 3;
	case GL_FLOAT_VEC4:
		return sizeof(FLOAT) * 4;

	case GL_INT:
	case GL_BOOL:
		return sizeof(UINT);
	case GL_INT_VEC2:
	case GL_BOOL_VEC2:
		return sizeof(UINT) * 2;
	case GL_INT_VEC3:
	case GL_BOOL_VEC3:
		return sizeof(UINT) * 3;
	case GL_INT_VEC4:
	case GL_BOOL_VEC4:
		return sizeof(UINT) * 4;

	case GL_FLOAT_MAT2:
		return sizeof(FLOAT) * 4;
	case GL_FLOAT_MAT3:
		return sizeof(FLOAT) * 9;
	case GL_FLOAT_MAT4:
		return sizeof(FLOAT) * 16;
	case GL_FLOAT_MAT2x3:
		return sizeof(FLOAT) * 6;
	case GL_FLOAT_MAT2x4:
		return sizeof(FLOAT) * 8;
	case GL_FLOAT_MAT3x2:
		return sizeof(FLOAT) * 6;
	case GL_FLOAT_MAT3x4:
		return sizeof(FLOAT) * 12;
	case GL_FLOAT_MAT4x2:
		return sizeof(FLOAT) * 8;
	case GL_FLOAT_MAT4x3:
		return sizeof(FLOAT) * 12;

	case GL_SAMPLER_1D:
	case GL_SAMPLER_2D:
	case GL_SAMPLER_3D:
	case GL_SAMPLER_CUBE:
	case GL_SAMPLER_1D_SHADOW:
	case GL_SAMPLER_2D_SHADOW:
	default:
		return sizeof(UINT);
	}
}

#if _DEBUG
inline void CheckOpenGLErrors()
{
	GLenum Error = glGetError();
	if (Error != GL_NO_ERROR)
	{
		appErrorf(TEXT("OpenGL error 0x%x"), Error);
	}
}
#else
#define CheckOpenGLErrors()
#endif

// Lines below are needed so we can compile HLSL shaders using D3DX to get the bytecode we then convert to GLSL

#if _WINDOWS
#include "D3D9Drv.h"
#endif

/** Information about a constant passed back from the worker application. */
struct FD3D9ConstantDesc
{
	enum { MaxNameLength = 256 };

	char Name[MaxNameLength];
	UBOOL bIsSampler;
	UINT RegisterIndex;
	UINT RegisterCount;
};

#endif // __OPENGLDRVPRIVATE_H__
