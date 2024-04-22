/*=============================================================================
	OpenGLDevice.cpp: OpenGL device RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

/** Buffer binding changes cache */

static GLuint ArrayBufferBound = 0;
static GLuint ElementArrayBufferBound = 0;
static GLuint PixelUnpackBufferBound = 0;
static GLuint UniformBufferBound = 0;

void CachedBindArrayBuffer( GLuint Buffer )
{
	if( ArrayBufferBound != Buffer )
	{
		glBindBuffer( GL_ARRAY_BUFFER, Buffer );
		ArrayBufferBound = Buffer;
	}
}

void CachedBindElementArrayBuffer( GLuint Buffer )
{
	if( ElementArrayBufferBound != Buffer )
	{
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, Buffer );
		ElementArrayBufferBound = Buffer;
	}
}

void CachedBindPixelUnpackBuffer( GLuint Buffer )
{
	if( PixelUnpackBufferBound != Buffer )
	{
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, Buffer );
		PixelUnpackBufferBound = Buffer;
	}
}

void CachedBindUniformBuffer( GLuint Buffer )
{
	if( UniformBufferBound != Buffer )
	{
		glBindBuffer( GL_UNIFORM_BUFFER_EXT, Buffer );
		UniformBufferBound = Buffer;
	}
}

/** In bytes. 0 means unlimited. */
extern INT GTexturePoolSize;
/** Whether to read the texture pool size from engine.ini on PC. Can be turned on with -UseTexturePool on the command line. */
extern UBOOL GReadTexturePoolSizeFromIni;

/** Device list is necessary for vertex buffers, so they can reach all devices on destruction, and tell them to reset vertex array caches */
TArray<FOpenGLDynamicRHI*> FOpenGLDevicesList;

void OnVertexBufferDeletion( GLuint VertexBufferResource )
{
	if (ArrayBufferBound == VertexBufferResource)
	{
		CachedBindArrayBuffer(0);
	}

	for( int DeviceIndex = 0; DeviceIndex < FOpenGLDevicesList.Num(); ++DeviceIndex )
	{
		FOpenGLDevicesList(DeviceIndex)->OnVertexBufferDeletion( VertexBufferResource );
	}
}

/**
 * Called at startup to initialize the OpenGL RHI.
 * @return The OpenGL RHI
 */
FDynamicRHI* OpenGLCreateRHI()
{
	return new FOpenGLDynamicRHI();
}

FOpenGLDynamicRHI::FOpenGLDynamicRHI()
:	DefaultViewport(NULL)
,	PendingFramebuffer(0)
,	bPendingFramebufferHasRenderTarget(FALSE)
,	PendingNumInstances(0)
,	PendingBoundShaderState(NULL)
,	PendingBegunDrawPrimitiveUP(FALSE)
,	PendingNumVertices(0)
,	PendingVertexDataStride(0)
,	PendingPrimitiveType(0)
,	PendingNumPrimitives(0)
,	PendingMinVertexIndex(0)
,	PendingIndexDataStride(0)
,	bDiscardSharedConstants(FALSE)
{
	// This should be called once at the start 
	check( IsInGameThread() );
	check( !GIsThreadedRendering );

	GTexturePoolSize = 0;
	if ( GReadTexturePoolSizeFromIni )
	{
		GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSize"), GTexturePoolSize, GEngineIni);
		GTexturePoolSize *= 1024*1024;
	}

	// Initialize the RHI capabilities.
	GRHIShaderPlatform = SP_PCOGL;
	GPixelCenterOffset = 0.0f;	// Note that in OpenGL there is no half-texel offset
	GSupportsVertexInstancing = TRUE;
	GSupportsDepthTextures = FALSE;
	GSupportsHardwarePCF = TRUE;
	// currently we miss support for this on our side
	GSupportsVertexTextureFetch = FALSE;
	GSupportsFetch4 = FALSE;
	GSupportsFPFiltering = TRUE;

	// Initialize the platform pixel format map.
	GPixelFormats[ PF_Unknown		].PlatformFormat	= GL_NONE;
	GPixelFormats[ PF_A32B32G32R32F	].PlatformFormat	= GL_RGBA;
	GPixelFormats[ PF_A8R8G8B8		].PlatformFormat	= GL_BGRA_EXT;
	GPixelFormats[ PF_G8			].PlatformFormat	= GL_LUMINANCE;
	GPixelFormats[ PF_G16			].PlatformFormat	= GL_NONE;	// Not supported for rendering.
	GPixelFormats[ PF_DXT1			].PlatformFormat	= GL_RGBA;
	GPixelFormats[ PF_DXT3			].PlatformFormat	= GL_RGBA;
	GPixelFormats[ PF_DXT5			].PlatformFormat	= GL_RGBA;
	GPixelFormats[ PF_UYVY			].PlatformFormat	= GL_NONE;	// @todo opengl: Not supported in OpenGL
	GPixelFormats[ PF_DepthStencil	].PlatformFormat	= GL_DEPTH_STENCIL;
	GPixelFormats[ PF_ShadowDepth	].PlatformFormat	= GL_DEPTH_COMPONENT;
	GPixelFormats[ PF_FilteredShadowDepth ].PlatformFormat = GL_DEPTH_COMPONENT;
	GPixelFormats[ PF_R32F			].PlatformFormat	= GL_RED;
	GPixelFormats[ PF_G16R16		].PlatformFormat	= GL_RG;
	GPixelFormats[ PF_G16R16F		].PlatformFormat	= GL_RG;
	GPixelFormats[ PF_G16R16F_FILTER].PlatformFormat	= GL_RG;
	GPixelFormats[ PF_G32R32F		].PlatformFormat	= GL_RG;
	GPixelFormats[ PF_A2B10G10R10   ].PlatformFormat    = GL_RGBA;
	GPixelFormats[ PF_A16B16G16R16  ].PlatformFormat    = GL_RGBA;
	GPixelFormats[ PF_D24 ].PlatformFormat				= GL_NONE;
	GPixelFormats[ PF_R16F			].PlatformFormat	= GL_RED;
	GPixelFormats[ PF_R16F_FILTER	].PlatformFormat	= GL_RED;

	GPixelFormats[ PF_FloatRGB	].PlatformFormat		= GL_RGB;
	GPixelFormats[ PF_FloatRGB	].BlockBytes			= 8;
	GPixelFormats[ PF_FloatRGBA	].PlatformFormat		= GL_RGBA;
	GPixelFormats[ PF_FloatRGBA	].BlockBytes			= 8;

	GPixelFormats[ PF_V8U8			].PlatformFormat	= GL_RG;
	GPixelFormats[ PF_BC5			].PlatformFormat	= GL_NONE;	// @todo opengl
	GPixelFormats[ PF_A1			].PlatformFormat	= GL_NONE;	// @todo opengl // Not supported for rendering.

	const INT MaxTextureDims = 8128;
	GMaxTextureMipCount = appCeilLogTwo( MaxTextureDims ) + 1;
	GMaxTextureMipCount = Min<INT>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );
	GMaxWholeSceneDominantShadowDepthBufferSize = 4096;

	// Initialize the constant buffers.
	InitConstantBuffers();

	FOpenGLDevicesList.Push( this );
}

FOpenGLDynamicRHI::~FOpenGLDynamicRHI()
{
	check(IsInGameThread() && IsInRenderingThread());

	Cleanup();

	FOpenGLDevicesList.RemoveSingleItem( this );
}

void FOpenGLDynamicRHI::Init()
{
	check(!GIsRHIInitialized);

	// Notify all initialized FRenderResources that there's a valid RHI device to create their RHI resources for now.
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitDynamicRHI();
	}
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitRHI();
	}

	glDisable(GL_DITHER);

	// Set the RHI initialized flag.
	GIsRHIInitialized = TRUE;
}

void FOpenGLDynamicRHI::Cleanup()
{
	check(0);

	if(GIsRHIInitialized)
	{
		// Reset the RHI initialized flag.
		GIsRHIInitialized = FALSE;

		// Ask all initialized FRenderResources to release their RHI resources.
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->ReleaseRHI();
		}
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->ReleaseDynamicRHI();
		}
	}
}

/**
 *	Sets the maximum viewport size the application is expecting to need for the time being, or zero for
 *	no preference.  This is used as a hint to the RHI to reduce redundant device resets when viewports
 *	are created or destroyed (typically in the editor.)
 *
 *	@param NewLargestExpectedViewportWidth Maximum width of all viewports (or zero if not known)
 *	@param NewLargestExpectedViewportHeight Maximum height of all viewports (or zero if not known)
 */
void FOpenGLDynamicRHI::SetLargestExpectedViewportSize( UINT NewLargestExpectedViewportWidth,
													   UINT NewLargestExpectedViewportHeight )
{
	// @todo opengl: Add support for this after we add robust support for editor viewports in OpenGL (similar to D3D9)
}

void FOpenGLDynamicRHI::AcquireThreadOwnership()
{
	if (DefaultViewport && IsInRenderingThread() )
	{
		DefaultViewport->InitRenderThreadContext();
	}
}
void FOpenGLDynamicRHI::ReleaseThreadOwnership()
{
	if (DefaultViewport && IsInRenderingThread() )
	{
		DefaultViewport->DestroyRenderThreadContext();
	}
}

void FOpenGLDynamicRHI::CachedSetActiveTexture( GLenum SamplerIndex )
{
	if( CachedState.ActiveTexture != SamplerIndex )
	{
		glActiveTexture( SamplerIndex );
		CachedState.ActiveTexture = SamplerIndex;
	}
}
