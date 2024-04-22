/*=============================================================================
	OpenGLUtil.h: OpenGL RHI utility implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

//
// Stat declarations.
//

DECLARE_STATS_GROUP(TEXT("OpenGLRHI"),STATGROUP_OpenGLRHI);
DECLARE_CYCLE_STAT(TEXT("Present time"),STAT_OpenGLPresentTime,STATGROUP_OpenGLRHI);
DECLARE_DWORD_COUNTER_STAT(TEXT("DrawPrimitive calls"),STAT_OpenGLDrawPrimitiveCalls,STATGROUP_OpenGLRHI);
DECLARE_DWORD_COUNTER_STAT(TEXT("Triangles drawn"),STAT_OpenGLTriangles,STATGROUP_OpenGLRHI);
DECLARE_DWORD_COUNTER_STAT(TEXT("Lines drawn"),STAT_OpenGLLines,STATGROUP_OpenGLRHI);
DECLARE_CYCLE_STAT(TEXT("CreateTexture time"),STAT_OpenGLCreateTextureTime,STATGROUP_OpenGLRHI);
DECLARE_CYCLE_STAT(TEXT("LockTexture time"),STAT_OpenGLLockTextureTime,STATGROUP_OpenGLRHI);
DECLARE_CYCLE_STAT(TEXT("UnlockTexture time"),STAT_OpenGLUnlockTextureTime,STATGROUP_OpenGLRHI);
DECLARE_CYCLE_STAT(TEXT("CopyTexture time"),STAT_OpenGLCopyTextureTime,STATGROUP_OpenGLRHI);
DECLARE_CYCLE_STAT(TEXT("CopyMipToMipAsync time"),STAT_OpenGLCopyMipToMipAsyncTime,STATGROUP_OpenGLRHI);
DECLARE_CYCLE_STAT(TEXT("UploadTextureMip time"),STAT_OpenGLUploadTextureMipTime,STATGROUP_OpenGLRHI);
DECLARE_CYCLE_STAT(TEXT("CreateBoundShaderState time"),STAT_OpenGLCreateBoundShaderStateTime,STATGROUP_OpenGLRHI);
DECLARE_CYCLE_STAT(TEXT("Constant buffer update time"),STAT_OpenGLConstantBufferUpdateTime,STATGROUP_OpenGLRHI);

