/*=============================================================================
 ES2RHIImplementation.cpp: OpenGL ES 2.0 RHI definitions.
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "Engine.h"
#include "ES2RHIPrivate.h"

#if WITH_ES2_RHI

static DWORD GetTextureSize( FES2Texture2D* ES2Texture )
{
	return ES2Texture ? ES2Texture->GetMemorySize() : 0;
}

WORD FES2Surface::NextUniqueID = 0;

/** Mirror the unreal pixel types to extended GL format information. */
FES2PixelFormat GES2PixelFormats[] = 
{
	{ 0, 0, 0, FALSE }, //	PF_Unknown
	{ 0, 0, 0, FALSE }, //	PF_A32B32G32R32F
#if IPHONE
	{ GL_RGBA, GL_BGRA_EXT, GL_UNSIGNED_BYTE, FALSE },	//	PF_A8R8G8B8 --> relies on APPLE_texture_format_BGRA8888 extension
#else
	{ GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, FALSE },		//	PF_A8R8G8B8 --> TODO: this results in reversed R and B, replace me!
#endif
	{ GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE, FALSE }, //	PF_G8
	{ 0, 0, 0, FALSE }, //	PF_G16
	{ GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG, GL_RGBA, GL_UNSIGNED_BYTE, TRUE }, //	PF_DXT1
	{ GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, GL_RGBA, GL_UNSIGNED_BYTE, TRUE }, //	PF_DXT3
	{ GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, GL_RGBA, GL_UNSIGNED_BYTE, TRUE }, //	PF_DXT5
	{ 0, 0, 0, FALSE }, //	PF_UYVY
	{ 0, 0, 0, FALSE }, //	PF_FloatRGB
	{ 0, 0, 0, FALSE }, //	PF_FloatRGBA
	{ GL_DEPTH_STENCIL_OES, GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES, FALSE }, //	PF_DepthStencil
	{ GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, FALSE }, //	PF_ShadowDepth
	{ 0, 0, 0, FALSE }, //	PF_FilteredShadowDepth
	{ 0, 0, 0, FALSE }, //	PF_R32F
	{ 0, 0, 0, FALSE }, //	PF_G16R16
	{ 0, 0, 0, FALSE }, //	PF_G16R16F
	{ 0, 0, 0, FALSE }, //	PF_G16R16F_FILTER
	{ 0, 0, 0, FALSE }, //	PF_G32R32F
	{ 0, 0, 0, FALSE }, //	PF_A2B10G10R10
	{ 0, 0, 0, FALSE }, //	PF_A16B16G16R16
	{ 0, 0, 0, FALSE }, //	PF_D24
	{ 0, 0, 0, FALSE }, //	PF_R16F
	{ 0, 0, 0, FALSE }, //	PF_R16F_FILTER
	{ 0, 0, 0, FALSE }, //	PF_BC5
	{ 0, 0, 0, FALSE }, //	PF_V8U8
	{ 0, 0, 0, FALSE }, //	PF_A1
	{ 0, 0, 0, FALSE }, //	PF_FloatR11G11B10
#if IPHONE
	{ GL_RGBA, GL_BGRA_EXT, GL_UNSIGNED_SHORT_4_4_4_4, FALSE },	//	PF_A4R4G4B4 --> relies on APPLE_texture_format_BGRA8888 extension
#else
	{ GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, FALSE },		//	PF_A4R4G4B4 --> TODO: this results in reversed R and B, replace me!
#endif
	{ GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, FALSE }, //	PF_R5G6B5
};

void ClearES2PendingResources()
{	
	GShaderManager.ClearGPUResources();
	GRenderManager.ClearGPUResources();
}

FIndexBufferRHIRef FES2RHI::CreateIndexBuffer(UINT Stride,UINT Size,FResourceArrayInterface* ResourceArray, DWORD InUsage) 
{ 
	checkf(Stride == 2, TEXT("Only 16-bit indices are supported on Mobile"));
	
	// let GL allocate an identifier
	GLuint BufferName = 0;
	GLCHECK(glGenBuffers(1, &BufferName));
	
	// @todo: if it's dynamic, we'd want a double buffer possibly? no lock stalls that way?
	UBOOL bIsDynamic = InUsage == RUF_Dynamic;
    UBOOL bIsSmallUpdate = InUsage == RUF_SmallUpdate;
	
	// Bind as Vertex Buffer
	GLCHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName));
	
	// Fill it if we got a resource array
	const GLenum UsageFlag = bIsDynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW;
	
	// Sizes it or fills it from Data.
	GLCHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, Size, ResourceArray ? ResourceArray->GetResourceData() : NULL, UsageFlag));
	INC_TRACKED_OPEN_GL_BUFFER_MEM(Size);
	
	// Full reset
	if (!GAllowFullRHIReset && ResourceArray)
	{
		ResourceArray->Discard();
	}
	
	return new FES2IndexBuffer(BufferName, Size, Stride, bIsDynamic, bIsSmallUpdate);
} 

FIndexBufferRHIRef FES2RHI::CreateInstancedIndexBuffer(UINT Stride,UINT Size,DWORD InUsage,UINT PreallocateInstanceCount,UINT& OutNumInstances) 
{ 
	OutNumInstances = 1;
	
	// let GL allocate an identifier
	GLuint BufferName = 0;
	GLCHECK(glGenBuffers(1, &BufferName));
	
	// Bind as Vertex Buffer
	GLCHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName));
	
	// Sizes it or fills it from Data.
	GLCHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, Size, NULL, GL_STATIC_DRAW));
	INC_TRACKED_OPEN_GL_BUFFER_MEM(Size);
	
	return new FES2IndexBuffer(BufferName, Size, Stride, FALSE, FALSE);
} 

void* FES2RHI::LockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI,UINT Offset,UINT Size) 
{ 
	DYNAMIC_CAST_ES2RESOURCE(IndexBuffer,IndexBuffer);
	return IndexBuffer->Lock(Offset, Size, FALSE, IndexBuffer->IsDynamic());
} 

void FES2RHI::UnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI) 
{ 
	DYNAMIC_CAST_ES2RESOURCE(IndexBuffer,IndexBuffer);
	IndexBuffer->Unlock();
} 

// This could be supported, but using it may cause performance problems.
FIndexBufferRHIRef FES2RHI::CreateAliasedIndexBuffer(FVertexBufferRHIParamRef InBuffer, UINT InStride)
{
    check(0);
    return NULL;
}

FVertexBufferRHIRef FES2RHI::CreateVertexBuffer(UINT Size, FResourceArrayInterface* ResourceArray, DWORD InUsage) 
{
	// let GL allocate an identifier
	GLuint BufferName = 0;
	GLCHECK(glGenBuffers(1, &BufferName));
	
	UBOOL bIsDynamic = InUsage == RUF_Dynamic;
    UBOOL bIsSmallUpdate = InUsage == RUF_SmallUpdate;
	
	// Bind as Vertex Buffer
	GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, BufferName));
	
	// Fill it if we got a resource array
	const GLenum UsageFlag = bIsDynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW;
	
	// Sizes it or fills it from Data.
	GLCHECK(glBufferData(GL_ARRAY_BUFFER, Size, ResourceArray ? ResourceArray->GetResourceData() : NULL, UsageFlag));
	INC_TRACKED_OPEN_GL_BUFFER_MEM(Size);
	
	if (!GAllowFullRHIReset && ResourceArray)
	{
		ResourceArray->Discard();
	}
	
	return new FES2VertexBuffer(BufferName, Size, bIsDynamic, bIsSmallUpdate);
}

void* FES2RHI::LockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI,UINT Offset,UINT SizeRHI,UBOOL bReadOnlyInsteadOfWriteOnly) 
{
	DYNAMIC_CAST_ES2RESOURCE(VertexBuffer,VertexBuffer);
	return VertexBuffer->Lock(Offset, SizeRHI, bReadOnlyInsteadOfWriteOnly, VertexBuffer->IsDynamic());
} 

void FES2RHI::UnlockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI) 
{ 
	DYNAMIC_CAST_ES2RESOURCE(VertexBuffer,VertexBuffer);
	VertexBuffer->Unlock();
} 


UBOOL FES2RHI::GetTextureMemoryStats( INT& AllocatedMemorySize, INT& AvailableMemorySize, INT& PendingMemoryAdjustment ) 
{ 
	return FALSE;
} 

UBOOL FES2RHI::GetTextureMemoryVisualizeData( FColor* TextureData, INT SizeX, INT SizeY, INT Pitch, INT PixelSize )
{
	return FALSE;
}

#if FLASH
FTexture2DRHIRef FES2RHI::CreateFlashTexture2D(UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips,DWORD Flags,void* Mip0, UINT BulkDataSize) 
{ 
//	checkf(Mip0 == NULL, TEXT("No Texture Resource Data implemented for Mobile"));
//	checkf(NumMips > 0, TEXT("Expected nummips to be specified"));

	// allocate a texture id
	GLuint TextureName;
	GLCHECK(glGenTextures(1, &TextureName));
	
	GLenum TextureType = GL_TEXTURE_2D;
	GShaderManager.SetActiveAndBoundTexture(0, TextureName, TextureType, Format);

	// GL needs these to render eventually...they can be changed later
	if( GSystemSettings.MaxAnisotropy > 1 )
	{
		GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAX_ANISOTROPY_EXT, GSystemSettings.MaxAnisotropy));
	}
	GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MIN_FILTER, NumMips > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
	GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	if (!appIsPowerOfTwo(SizeX) || !appIsPowerOfTwo(SizeY) || Flags & TexCreate_ResolveTargetable)
	{
		GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	}

	// for resolve targetable textures, we need to glTexImage2D since they won't be getting Lock called on them later
	if (Flags & TexCreate_ResolveTargetable)
	{
		if (Format == PF_DepthStencil)
		{
			GLCHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL_OES, SizeX, SizeY, 0, GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES, NULL));
		}
		else
		{
			GLCHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SizeX, SizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
		}
	}
	
	return new FES2Texture2D(TextureName, (EPixelFormat)Format, SizeX, SizeY, NumMips, Flags, FALSE, NumMips > 1 ? SF_Trilinear : SF_Bilinear, GL_REPEAT, Mip0, BulkDataSize); 
}
#endif

FTexture2DRHIRef FES2RHI::CreateTexture2D(UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips,DWORD Flags,FResourceBulkDataInterface* BulkData) 
{ 
//	checkf(Mip0 == NULL, TEXT("No Texture Resource Data implemented for Mobile"));
//	checkf(NumMips > 0, TEXT("Expected nummips to be specified"));

	// allocate a texture id
	GLuint TextureName;
	GLCHECK(glGenTextures(1, &TextureName));
	
	GLenum TextureType = GL_TEXTURE_2D;
	GShaderManager.SetActiveAndBoundTexture(0, TextureName, TextureType, Format);

	ESamplerFilter Filter = SF_Point;
    GLenum Address = GL_REPEAT;

	// Render-to-texture?
	if ( Flags & TexCreate_ResolveTargetable )
	{
		// Results in an error if unsupported
		if ( GPlatformFeatures.MaxTextureAnisotropy > 1 )
		{
			GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1));
		}
		GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
		GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
		GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        Address = GL_CLAMP_TO_EDGE;
	}
	else
	{
		// GL needs these to render eventually...they can be changed later
		if( GSystemSettings.MaxAnisotropy > 1 )
		{
			Filter = SF_AnisotropicPoint;
			GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAX_ANISOTROPY_EXT, GSystemSettings.MaxAnisotropy));
			GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MIN_FILTER, NumMips > 1 ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR));
			GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		}
		else if ( NumMips > 1 )
		{
			Filter = SF_Trilinear;
			GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
			GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		}
		else
		{
			Filter = SF_Bilinear;
			GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
			GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		}

		if (!appIsPowerOfTwo(SizeX) || !appIsPowerOfTwo(SizeY))
		{
			GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
            Address = GL_CLAMP_TO_EDGE;
		}
	}

	// for resolve targetable textures, we need to glTexImage2D since they won't be getting Lock called on them later
	if (Flags & TexCreate_ResolveTargetable)
	{
		GLCHECK(glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GES2PixelFormats[Format].InternalFormat,
			SizeX, SizeY, 0,
			GES2PixelFormats[Format].Format,
			GES2PixelFormats[Format].Type,
			NULL
		));
	}

	return new FES2Texture2D(TextureName, (EPixelFormat)Format, SizeX, SizeY, NumMips, Flags, FALSE, Filter, Address); 
} 

#if PLATFORM_SUPPORTS_D3D10_PLUS

FTexture2DArrayRHIRef FES2RHI::CreateTexture2DArray(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,DWORD Flags,FResourceBulkDataInterface* BulkData)
{
	// not supported
	return FTexture2DArrayRHIRef();
}

FTexture3DRHIRef FES2RHI::CreateTexture3D(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,DWORD Flags,const BYTE* Data)
{
	// not supported
	return FTexture3DRHIRef();
}

/** Generates mip maps for the surface. */
void FES2RHI::GenerateMips(FSurfaceRHIParamRef Surface)
{
	// not supported
}

#endif

FTexture2DRHIRef FES2RHI::ReallocateTexture2D(FTexture2DRHIParamRef Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY) 
{
	// not supported
	return NULL;
}

/**
 * Computes the size in memory required by a given texture.
 *
 * @param	TextureRHI		- Texture we want to know the size of
 * @return					- Size in Bytes
 */
UINT FES2RHI::GetTextureSize(FTexture2DRHIParamRef TextureRHI)
{
	// not supported (yet)
	return 0;
}

/**
 * Requests an async reallocation of the specified texture.
 * Returns a new texture that represents if the request was accepted.
 *
 * @param Texture2D		Texture to reallocate
 * @param NewMipCount	New number of mip-levels (including the base mip)
 * @param NewSizeX		New width, in pixels
 * @param NewSizeY		New height, in pixels
 * @param RequestStatus	Thread-safe counter to monitor the status of the request. Decremented by one when the request is completed.
 * @return				Reference to a newly created Texture2D if the request was accepted, or an invalid ref
 */
FTexture2DRHIRef FES2RHI::AsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY, FThreadSafeCounter* RequestStatus )
{
	// not supported
	return NULL;
}

/**
 * Finalizes an async reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to finalize
 * @param bBlockUntilCompleted	If TRUE, blocks until the finalization is fully completed
 * @return						Reallocation status
 */
ETextureReallocationStatus FES2RHI::FinalizeAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, UBOOL bBlockUntilCompleted )
{
	// not supported
	return TexRealloc_Failed;
}

/**
 * Cancels an async reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to cancel
 * @param bBlockUntilCompleted	If TRUE, blocks until the cancellation is fully completed
 * @return						Reallocation status
 */
ETextureReallocationStatus FES2RHI::CancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, UBOOL bBlockUntilCompleted )
{
	// not supported
	return TexRealloc_Failed;
}

FSurfaceRHIRef FES2RHI::CreateTargetableSurface(UINT SizeX,UINT SizeY,BYTE Format,FTexture2DRHIParamRef ResolveTargetTextureRHI,DWORD Flags,const TCHAR* UsageStr) 
{ 
	DYNAMIC_CAST_ES2RESOURCE(Texture2D,ResolveTargetTexture);

	// if we are going to target a texture, make a surface that uses that texture as the 'backing store'
	// @todo: This will break the use case of when reading from the ResolveTarget while writing to the surface
	if (ResolveTargetTexture)
	{
		check(SizeX == ResolveTargetTexture->GetWidth());
		check(SizeY == ResolveTargetTexture->GetHeight());
		return new FES2Surface(ResolveTargetTexture, NULL, Flags);
	}

	// if we want to try for MSAA, pass in a number of samples - this will trigger the constructor to make a depth
	// buffer that has the same MSAA-ness as the back buffer (if we are using render targets, then there won't be a
	// viewport depth buffer made, so Scaleform will make a back buffer that it uses while rendering to back buffer
	// NOTE: This is a large memory hit, but the Framebuffer will fail to create if the back buffer and the depth
	// buffer don't have the same MSAA-ness!!
	UINT NumSamples = 0;
#if IPHONE || ANDROID || FLASH
	if (Format == PF_DepthStencil && 
		GMSAAAllowed && 
		(Flags & TargetSurfCreate_DepthBufferToMatchBackBuffer) != 0)
	{
		// iOS initialization code forces 4 samples, so use that here
		NumSamples = 4;
	}
#endif

	return new FES2Surface(SizeX, SizeY, (EPixelFormat)Format, NumSamples);
} 


/**
 * Calculate the stride of a mip level of a given format
 */
UINT GetMipStride(UINT SizeX, EPixelFormat Format, UINT MipIndex)
{
	if ( GTextureFormatSupport & TEXSUPPORT_PVRTC )
	{
		// calculate how many blocks we have in 1 row
		UINT NumBlocksX = Max<UINT>((SizeX >> MipIndex) / GPixelFormats[Format].BlockSizeX, 
			GES2PixelFormats[Format].bIsCompressed ? 2 : 1); // PVR always has two blocks
		// calculate the number of bytes for one row of blocks
		return NumBlocksX * GPixelFormats[Format].BlockBytes;
	}
	else
	{
		// calculate how many blocks we have in 1 row
		UINT NumBlocksX = Max<UINT>((SizeX >> MipIndex) / GPixelFormats[Format].BlockSizeX, 1);
		// calculate the number of bytes for one row of blocks
		return NumBlocksX * GPixelFormats[Format].BlockBytes;
	}
}

/**
 * Calculate the number of rows of a mip level of a given format
 */
UINT GetMipNumRows(UINT SizeY, EPixelFormat Format, UINT MipIndex)
{
	// calculate how many blocks we have in 1 column
	if ( GTextureFormatSupport & TEXSUPPORT_PVRTC )
	{
		return Max<UINT>((SizeY >> MipIndex) / GPixelFormats[Format].BlockSizeY, 
			GES2PixelFormats[Format].bIsCompressed ? 2 : 1); // PVR always has two blocks
	}
	else
	{
		return Max<UINT>((SizeY >> MipIndex) / GPixelFormats[Format].BlockSizeY, 1);
	}
}

void* FES2RHI::LockTexture2D(FTexture2DRHIParamRef TextureRHI,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail) 
{
	DYNAMIC_CAST_ES2RESOURCE(Texture2D,Texture);

	checkf(bIsDataBeingWrittenTo, TEXT("Mobile currently only supports lock for writing"));
	
	DestStride = GetMipStride(Texture->GetWidth(), Texture->GetFormat(), MipIndex);
	return Texture->Lock(MipIndex);
} 

void FES2RHI::UnlockTexture2D(FTexture2DRHIParamRef TextureRHI,UINT MipIndex,UBOOL bLockWithinMiptail) 
{ 
	DYNAMIC_CAST_ES2RESOURCE(Texture2D,Texture);
	Texture->Unlock(MipIndex);
} 

#if PLATFORM_SUPPORTS_D3D10_PLUS

void* FES2RHI::LockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,UINT TextureIndex,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail)
{
	check(0);
	return NULL;
}

void FES2RHI::UnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,UINT TextureIndex,UINT MipIndex,UBOOL bLockWithinMiptail)
{
	check(0);
}

#endif

FTextureCubeRHIRef FES2RHI::CreateTextureCube(UINT Size,BYTE Format,UINT NumMips,DWORD Flags,FResourceBulkDataInterface* BulkData) 
{ 
	checkf(BulkData == NULL, TEXT("No Texture Resource Data implemented for Mobile"));
	checkf(NumMips > 0, TEXT("Expected nummips to be specified"));
	
	// allocate a texture id
	GLuint TextureName;
	GLCHECK(glGenTextures(1, &TextureName));
	
	GLenum TextureType = GL_TEXTURE_CUBE_MAP;
	GShaderManager.SetActiveAndBoundTexture(0, TextureName, TextureType, Format);
	
	// GL needs these to render eventually...they can be changed later
	GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MIN_FILTER, NumMips > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
	GLCHECK(glTexParameteri(TextureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	
	return new FES2TextureCube(TextureName, (EPixelFormat)Format, Size, NumMips, FALSE, NumMips > 1 ? SF_Trilinear : SF_Bilinear, GL_REPEAT); 
} 


void* FES2RHI::LockTextureCubeFace(FTextureCubeRHIParamRef TextureRHI,UINT FaceIndex,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail) 
{ 
	DYNAMIC_CAST_ES2RESOURCE(TextureCube,Texture);
	DestStride = GetMipStride(Texture->GetWidth(), Texture->GetFormat(), MipIndex);
	return Texture->Lock(MipIndex); 
} 


void FES2RHI::UnlockTextureCubeFace(FTextureCubeRHIParamRef TextureRHI,UINT FaceIndex,UINT MipIndex,UBOOL bLockWithinMiptail) 
{ 
	DYNAMIC_CAST_ES2RESOURCE(TextureCube,Texture);
	Texture->Unlock(MipIndex, FaceIndex);
} 



FES2Buffer::~FES2Buffer()
{
    if (LockBuffer)
    {
        appFree(LockBuffer);
    }
	DEC_TRACKED_OPEN_GL_BUFFER_MEM(BufferSize);
	GLCHECK(glDeleteBuffers(1, &BufferName));
}


void FES2Buffer::Bind()
{
}

BYTE* FES2Buffer::Lock(const UINT Offset, const UINT Size, const UBOOL bReadOnly, const UBOOL bDiscard)
{
	checkf(bReadOnly == 0, TEXT("Read-only buffer locks are not supported on mobile"));

	// Only one outstanding lock is allowed at a time!
	check( bIsSmallUpdate || LockBuffer == NULL );

	// If we're able to discard the current data, do so right away
	if( bDiscard )
	{
		GLCHECK(glBindBuffer(BufferType, BufferName));
		GLCHECK(glBufferData(BufferType, BufferSize, NULL, bIsDynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW));
		INC_TRACKED_OPEN_GL_BUFFER_MEM(BufferSize);
	}

// @todo flash: The void* Data = glMapBufferOES doesn't work with the Flash wrapper for GLCHECK - is MapBuffer supported in Flash?
#if !FLASH && !_WINDOWS
    if( GES2MapBuffer )
    {
        GLCHECK(glBindBuffer(BufferType, BufferName));
        GLCHECK(void* Data = glMapBufferOES(BufferType, GL_WRITE_ONLY_OES));
        if (Data != NULL)
        {
            return ((BYTE*)Data) + Offset;
        }
    }
#endif

    if (bIsSmallUpdate)
    {
        return ((BYTE*)LockBuffer) + Offset;
    }
    else
    {
		// Allocate a temp buffer to write into
		LockSize = Size;
		LockOffset = Offset;
		LockBuffer = appMalloc(Size);
		return (BYTE*)LockBuffer;
	}
}

void FES2Buffer::Unlock()
{
	//XXX AR check( LockBuffer != NULL );
	GLCHECK(glBindBuffer(BufferType, BufferName));

// @todo flash: If we add support for glMapBufferOES, then add support for glUnmapBufferOES
#if !FLASH && !_WINDOWS

    if( GES2MapBuffer )
    {
        GLCHECK(glUnmapBufferOES(BufferType));
        return;
    }
#endif

    if( bIsSmallUpdate )
    {
        // This is often faster than glBufferSubData on a small part of the buffer.
        GLCHECK(glBufferData(BufferType, BufferSize, LockBuffer, bIsDynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW));
        return;
    }

	// Check for the typical, optimized case
	if( LockSize == BufferSize )
	{
		GLCHECK(glBufferData(BufferType, BufferSize, LockBuffer, bIsDynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW));
		check( LockBuffer != NULL );
	}
	else
	{
		// Only updating a subset of the data
		GLCHECK(glBufferSubData(BufferType, LockOffset, LockSize, LockBuffer));
		check( LockBuffer != NULL );
	}
	appFree(LockBuffer);
	LockBuffer = NULL;
}



FES2BaseTexture::FES2BaseTexture(const GLenum InTextureType, const GLint InFaceCount, const GLuint InTextureName, const EPixelFormat InFormat, const GLint InWidth, const GLint InHeight, const GLint InMipCount, const UBOOL InIsSRGB, const ESamplerFilter DefaultFilter, GLenum InAddress, void* Mip0, GLuint BulkDataSize)
	: TextureType(InTextureType)
	, TextureName(InTextureName)
	, MipCount(InMipCount)
	, Width(InWidth)
	, Height(InHeight)
	, Format(InFormat)
	, Filter(DefaultFilter)
    , AddressS(InAddress)
    , AddressT(InAddress)
	, bIsSRGB(InIsSRGB)
	, Locks()
{
#if FLASH
	// use the mip0 data for all the mips, there won't be any locking happening
	
	if (!Mip0 || BulkDataSize == 0)
        return;

	FES2RenderManager::RHIglCompressedTexImage2D(
             TextureType,  // target
             0,  // level
             GES2PixelFormats[Format].InternalFormat,  // internalformat
             Width,  // width
             Height,  // height
             0, // border
             BulkDataSize,  // imageSize
             Mip0); // data
#endif
}

FES2BaseTexture::~FES2BaseTexture()
{
#if USE_DETAILED_IPHONE_MEM_TRACKING
	DEC_TRACKED_OPEN_GL_TEXTURE_MEM(Size);
#endif
	GLCHECK(glDeleteTextures(1, &TextureName));
}


void* FES2BaseTexture::Lock(UINT MipIndex)
{
	// Make sure this mip index isn't already locked
	for( INT CurLockIndex = 0; CurLockIndex < Locks.Num(); ++CurLockIndex )
	{
		if( !ensure( Locks( CurLockIndex ).LockedMipIndex != MipIndex ) )
		{
			// Already locked!
			return NULL;
		}
	}

	UINT Stride = GetMipStride(Width, Format, MipIndex);
	UINT NumRows = GetMipNumRows(Height, Format, MipIndex);
	
	// allocate a temp buffer to write into
	FES2OutstandingTextureLock NewLock;
	NewLock.LockedMipIndex = MipIndex;
	NewLock.LockBuffer = appMalloc(Stride * NumRows);
	Locks.AddItem( NewLock );
	
	return NewLock.LockBuffer;
}


void FES2BaseTexture::Unlock(UINT MipIndex, INT FaceIndex)
{
	// Make sure the specified mip index was locked
	for( INT CurLockIndex = 0; CurLockIndex < Locks.Num(); ++CurLockIndex )
	{
		FES2OutstandingTextureLock& CurLock = Locks( CurLockIndex );
		if( CurLock.LockedMipIndex == MipIndex )
		{
			Bind();
			
			if (GES2PixelFormats[Format].InternalFormat != 0)
			{
				// Update the mipmap
				if (GES2PixelFormats[Format].bIsCompressed)
				{
					UINT Stride = GetMipStride(Width, Format, MipIndex);
					UINT NumRows = GetMipNumRows(Height, Format, MipIndex);
					
					GLCHECK(glCompressedTexImage2D(
						(FaceIndex == -1) ? TextureType : (GL_TEXTURE_CUBE_MAP_POSITIVE_X + FaceIndex),  // target
						MipIndex,  // level
						GES2PixelFormats[Format].InternalFormat,  // internalformat
						Max<UINT>(Width >> MipIndex, 1),  // width
						Max<UINT>(Height >> MipIndex, 1),  // height
						0, // border
						Stride * NumRows,  // imageSize
						CurLock.LockBuffer));  // data
				}
				else
				{
					// get the stride of each row
					UINT Stride = Max<UINT>(Width >> MipIndex, 1);
					// gl needs each row to be aligned to a certain multiple (1, 2, 4, or 8), so
					// for textures with stride of 8, we set the alignment for that row 
					UINT RowAlignment = Min<UINT>(Stride, 8);
					GLCHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, RowAlignment));

					GLCHECK(glTexImage2D(
						(FaceIndex == -1) ? TextureType : (GL_TEXTURE_CUBE_MAP_POSITIVE_X + FaceIndex),  // target
						MipIndex,  // level
						GES2PixelFormats[Format].InternalFormat, // internal format
						Max<UINT>(Width >> MipIndex, 1),  // width
						Max<UINT>(Height >> MipIndex, 1),  // height
						0, // border
						GES2PixelFormats[Format].Format,  // format
						GES2PixelFormats[Format].Type,  // type
						CurLock.LockBuffer));  // data
				}
			}
			

			appFree(CurLock.LockBuffer);
			CurLock.LockBuffer = NULL;

			// Remove this lock from our list
			Locks.RemoveSwap( CurLockIndex );
			break;
		}
	}
}


void FES2BaseTexture::Bind()
{
	GShaderManager.SetActiveAndBoundTexture(0, TextureName, TextureType, Format);
}					


/**
 * Increments a number by the specified amount and returns the previous value.
 */
template <typename NumberType, typename AmountType>
FORCEINLINE NumberType appIncrement( NumberType& Number, AmountType Amount )
{
	NumberType OldValue = Number;
	Number += Amount;
	return OldValue;
}


/**
 * Constructor for a surface with an existing renderbuffer (ie the OS backbuffer)
 */
FES2Surface::FES2Surface(INT InWidth, INT InHeight)
	: Width(InWidth)
	, Height(InHeight)
	, bUsingSeparateStencilBuffer(FALSE)
	, bPlaceholderSurface(TRUE)
	, bBackingRenderBufferOwner(FALSE)
	, BackingRenderBuffer(0xFFFFFFFF)
	, ResolveTexture(NULL)
	, RenderTargetTexture(NULL)
	, ResolveTextureCube(NULL)
	, CurrentResolveTextureIndex(0)
	, UniqueID(NextUniqueID++)
{
	// Nothing to do for a placeholder
}

/**
 * Constructor for a surface with no backing texture, so a new renderbuffer will be created
 */
FES2Surface::FES2Surface(INT InWidth, INT InHeight, EPixelFormat Format, INT Samples)
	: Width(InWidth)
	, Height(InHeight)
	, bUsingSeparateStencilBuffer(FALSE)
	, bPlaceholderSurface(FALSE)
	, ResolveTexture(NULL)
	, RenderTargetTexture(NULL)
	, ResolveTextureCube(NULL)
	, CurrentResolveTextureIndex(0)
	, UniqueID(NextUniqueID++)
{
	// create a new render buffer for later framebuffer use
	GLCHECK(glGenRenderbuffers(1, &BackingRenderBuffer));
	GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, BackingRenderBuffer));

	// Without knowing exactly, we're going to assume 4 bytes per pixel/sample.
	STAT(DWORD MemorySize = Width*Height*4);

 	if (Format == PF_DepthStencil)
 	{
#if IPHONE
		if( GMSAAAllowed && Samples > 0 )
		{
 			GLCHECK(glRenderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, Samples, GL_DEPTH24_STENCIL8_OES, Width, Height));
			STAT(MemorySize *= Samples);
		}
		else
#endif
		{
			// On Android we can't assume packed depth/stencil support
#if ANDROID
			if (!GMobileUsePackedDepthStencil)
			{
				extern INT CallJava_GetDepthSize();
				if (CallJava_GetDepthSize() == 16)
				{
					// Use NonLinear 16-bit depth if available
					if (GSupports16BitNonLinearDepth)
					{
						GLCHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16_NONLINEAR_NV, Width, Height));
					}
					else
					{
						GLCHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, Width, Height));
					}
				}
				else
				{
					GLCHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24_OES, Width, Height));
				}

				GLCHECK(glGenRenderbuffers(1, &BackingStencilBuffer));
				GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, BackingStencilBuffer));
				GLCHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, Width, Height));

				bUsingSeparateStencilBuffer = TRUE;
			}
			else
#endif
			{
				GLCHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, Width, Height));
			}		
		}
 	}
 	else if ( Format == PF_ShadowDepth )
	{
		GLCHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, Width, Height));
	}
	else
 	{
#if IPHONE
		// TODO: Right now, all we support is creating on-screen MSAA surfaces
		// If we want to use off-screen rendering and MSAA, we need to add stuff here
#endif
		GLCHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, Width, Height));
 	}

	INC_DWORD_STAT_BY( STAT_RendertargetMemory, MemorySize );
	INC_TRACKED_OPEN_GL_SURFACE_MEM(MemorySize);

	// Note that we own the renderbuffer, so we can free it up later, if necessary
	bBackingRenderBufferOwner = TRUE;
}

/**
 * Constructor for a surface with an existing renderbuffer (ie the OS backbuffer)
 */
FES2Surface::FES2Surface(INT InWidth, INT InHeight, GLuint ExistingRenderBuffer)
	: Width(InWidth)
	, Height(InHeight)
	, bUsingSeparateStencilBuffer(FALSE)
	, bPlaceholderSurface(FALSE)
	, bBackingRenderBufferOwner(FALSE)
	, BackingRenderBuffer(ExistingRenderBuffer)
	, BackingStencilBuffer(0xFFFFFFFF)
	, ResolveTexture(NULL)
	, RenderTargetTexture(NULL)
	, ResolveTextureCube(NULL)
	, ResolveTextureCubeFace(CubeFace_MAX)
	, CurrentResolveTextureIndex(0)
	, UniqueID(NextUniqueID++)
{
}

/**
 * Constructor for a surface that has a backing texture already made
 */
FES2Surface::FES2Surface(FTexture2DRHIRef InResolveTexture, FTexture2DRHIRef InResolveTexture2, DWORD SurfCreateFlags)
	: bPlaceholderSurface(FALSE)
	, bUsingSeparateStencilBuffer(FALSE)
	, bBackingRenderBufferOwner(FALSE)
	, BackingRenderBuffer(0xFFFFFFFF)
	, BackingStencilBuffer(0xFFFFFFFF)
	, ResolveTexture(InResolveTexture)
	, RenderTargetTexture(NULL)
	, ResolveTextureCube(NULL)
	, ResolveTextureCubeFace(CubeFace_MAX)
	, CurrentResolveTextureIndex(0)
	, UniqueID( (SurfCreateFlags & TargetSurfCreate_Dedicated) ? appIncrement(NextUniqueID,2) : appIncrement(NextUniqueID,1) )
{
	FES2Texture2D* ES2ResolveTexture = ES2CAST( FES2Texture2D, ResolveTexture );

	STAT( DWORD MemorySize = GetTextureSize(ES2ResolveTexture) );

	Width = ES2ResolveTexture->GetWidth();
	Height = ES2ResolveTexture->GetHeight();

	// Should this surface use two dedicated buffers?
	if ( SurfCreateFlags & TargetSurfCreate_Dedicated )
	{
		EPixelFormat Format = ES2ResolveTexture->GetFormat();
		RenderTargetTexture = RHICreateTexture2D(Width, Height, Format, 1, ES2ResolveTexture->GetCreateFlags(), NULL);
		STAT( MemorySize *= 2 );
	}
	else
	{
		RenderTargetTexture = ResolveTexture;
	}

	INC_DWORD_STAT_BY( STAT_RendertargetMemory, MemorySize );
	INC_TRACKED_OPEN_GL_SURFACE_MEM(MemorySize);
}

/**
 * Constructor for a surface with a backing cubemap face already made
 */
FES2Surface::FES2Surface(FTextureCubeRHIRef InResolveTextureCube, ECubeFace InResolveFace)
	: bPlaceholderSurface(FALSE)
	, bUsingSeparateStencilBuffer(FALSE)
	, bBackingRenderBufferOwner(FALSE)
	, BackingRenderBuffer(0xFFFFFFFF)
	, ResolveTextureCube(InResolveTextureCube)
	, ResolveTexture(NULL)
	, RenderTargetTexture(NULL)
	, ResolveTextureCubeFace(InResolveFace)
	, CurrentResolveTextureIndex(0)
	, UniqueID(NextUniqueID++)
{
	FES2TextureCube* ES2ResolveTexture = ES2CAST( FES2TextureCube, ResolveTextureCube );
	Width = Height = ES2ResolveTexture->GetWidth();
}

/**
 * Common destructor
 */
FES2Surface::~FES2Surface()
{
	STAT(DWORD MemorySize = 0);

	// If this surface owns it, clean up the BackingRenderBuffer
	if (bBackingRenderBufferOwner)
	{
		GLCHECK(glDeleteRenderbuffers(1, &BackingRenderBuffer));

		// if a sparatate stencil was created, remove that too
		if (bUsingSeparateStencilBuffer)
		{
			GLCHECK(glDeleteRenderbuffers(1, &BackingStencilBuffer));
		}

		STAT(MemorySize = Width * Height * 4);
		DEC_TRACKED_OPEN_GL_SURFACE_MEM(MemorySize);
	}

	STAT( MemorySize += GetTextureSize( ES2CAST(FES2Texture2D, ResolveTexture) ) );
	STAT( MemorySize += GetTextureSize( ES2CAST(FES2Texture2D, RenderTargetTexture) ) );
	DEC_DWORD_STAT_BY( STAT_RendertargetMemory, MemorySize );

	GRenderManager.RemoveFrameBufferReference(this);
}

/** Swaps between the two resolve targets, if it was created with two dedicated buffers. */
void FES2Surface::SwapResolveTarget()
{
	if ( ResolveTexture != RenderTargetTexture )
	{
		FES2Texture2D* ES2ResolveTexture = ES2CAST( FES2Texture2D, ResolveTexture );
		FES2Texture2D* ES2RenderTargetTexture = ES2CAST( FES2Texture2D, RenderTargetTexture );
		ES2ResolveTexture->SwapTextureName( ES2RenderTargetTexture );
		CurrentResolveTextureIndex = 1 - CurrentResolveTextureIndex;
	}
}

#endif

#if ANDROID

/**
 * Return the name of the texture format extension for this device
 */
const TCHAR* appGetAndroidTextureFormatName()
{
	// choose the appropriate cooked directory
	if (GTextureFormatSupport & TEXSUPPORT_DXT)
	{
		return TEXT("_DXT");
	}
	else if (GTextureFormatSupport & TEXSUPPORT_ATITC)
	{
		return TEXT("_ATITC");
	}
	else if (GTextureFormatSupport & TEXSUPPORT_PVRTC)
	{
		return TEXT("_PVRTC");
	}
	else if (GTextureFormatSupport & TEXSUPPORT_ETC) 
	{
		return TEXT("_ETC");
	}
	return TEXT("_ERROR");
}

/**
 * Return the texture format used on this device
 */
DWORD appGetAndroidTextureFormat()
{
	if (GTextureFormatSupport & TEXSUPPORT_DXT)
	{
		return TEXSUPPORT_DXT;
	}
	else if (GTextureFormatSupport & TEXSUPPORT_PVRTC)
	{
		return TEXSUPPORT_PVRTC;
	}
	else if (GTextureFormatSupport & TEXSUPPORT_ATITC)
	{
		return TEXSUPPORT_ATITC;
	}
	else
	{
		return TEXSUPPORT_ETC;
	}
}

#endif