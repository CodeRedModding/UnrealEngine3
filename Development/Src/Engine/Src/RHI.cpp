/*=============================================================================
	RHI.cpp: Render Hardware Interface implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

//
// RHI globals.
//

UBOOL GIsRHIInitialized = FALSE;
INT GMaxTextureMipCount = MAX_TEXTURE_MIP_COUNT;
INT GMinTextureResidentMipCount = -1; // set via .ini now!
UBOOL GSupportsDepthTextures = FALSE;
UBOOL GSupportsHardwarePCF = FALSE;
UBOOL GSupportsVertexTextureFetch = FALSE;
UBOOL GSupportsFetch4 = FALSE;
UBOOL GSupportsFPFiltering = TRUE;
UBOOL GSupportsRenderTargetFormat_PF_G8 = TRUE;
UBOOL GSupportsQuads = FALSE;
UBOOL GUsesInvertedZ = FALSE;
FLOAT GPixelCenterOffset = 0.5f;
INT GMaxPerObjectShadowDepthBufferSizeX = 2048;
INT GMaxPerObjectShadowDepthBufferSizeY = 2048;
INT GMaxWholeSceneDominantShadowDepthBufferSize = 2048;
INT GCurrentColorExpBias = 0;
#if USE_NULL_RHI
	UBOOL GUsingNullRHI = TRUE;
#else
	UBOOL GUsingNullRHI = FALSE;
#endif
UBOOL GUsingES2RHI = FALSE;
UBOOL GUsingMobileRHI = FALSE;
UBOOL GMobileTiledRenderer = TRUE;
UBOOL GMobileUsePackedDepthStencil = FALSE;

/** Whether to use the post-process code path on mobile. */
UBOOL GMobileAllowPostProcess = FALSE;

/** 
 *	Whether the current mobile implementation supports shader discard.  Some devices (i.e. Adreno 205)
 *  crash if shaders use the discard instruction
 */
UBOOL GMobileAllowShaderDiscard = TRUE;

/**
 *	Whether the current device supports bump offset
 *	Workaround for a shader compilation issue preventing the uv from being modified correctly on Mali GPUs
 */
UBOOL GMobileDeviceAllowBumpOffset = TRUE;

/** Whether the current mobile device can make reliable framebuffer status checks */ 
UBOOL GMobileAllowFramebufferStatusCheck = TRUE;

INT GDrawUPVertexCheckCount = MAXINT;
INT GDrawUPIndexCheckCount = MAXINT;
UBOOL GSupportsVertexInstancing = FALSE;
UBOOL GSupportsEmulatedVertexInstancing = FALSE;
UBOOL GVertexElementsCanShareStreamOffset = TRUE;
INT GOptimalMSAALevel = 4;
UBOOL GProfilingGPU = FALSE;

/** Whether we are profiling GPU hitches. */
UBOOL GProfilingGPUHitches = FALSE;

/** Bit flags from ETextureFormatSupport, specifying what texture formats a platform supports. */
DWORD GTextureFormatSupport = TEXSUPPORT_DXT;

// if we can reload ALL rhi resources (really only working fully on android)
UBOOL GAllowFullRHIReset = FALSE;

/* A global depth bias to use when user clip planes are enabled, to avoid z-fighting. */
FLOAT GDepthBiasOffset = 0.0f;

#if WITH_SLI
INT GNumActiveGPUsForRendering = 1;
#endif

FVertexElementTypeSupportInfo GVertexElementTypeSupport;

/** FPlatformFeatures constructor. Sets up all default values. */
FPlatformFeatures::FPlatformFeatures()
:	MaxTextureAnisotropy( 1 )
,	bSupportsRendertargetDiscard( FALSE )
{
}

/** Features supported by the platform. */
FPlatformFeatures	GPlatformFeatures;
