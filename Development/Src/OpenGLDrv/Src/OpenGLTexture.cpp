/*=============================================================================
	OpenGLVertexBuffer.cpp: OpenGL texture RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

/** In bytes. */
extern INT GCurrentTextureMemorySize;
/** In bytes. 0 means unlimited. */
extern INT GTexturePoolSize;

void OpenGLTextureAllocated( FOpenGLTexture2D& Texture )
{
	INT TextureSize = CalcTextureSize( Texture.SizeX, Texture.SizeY, Texture.Format, Texture.NumMips );
	if (Texture.bCubemap)
	{
		TextureSize *= 6;
	}
	Texture.SetMemorySize( TextureSize );
	GCurrentTextureMemorySize += TextureSize;
}

void OpenGLTextureDeleted( FOpenGLTexture2D& Texture )
{
	INT TextureSize = Texture.GetMemorySize();
	GCurrentTextureMemorySize -= TextureSize;
}

/**
 * Retrieves texture memory stats. Unsupported with this allocator.
 *
 * @return FALSE, indicating that out variables were left unchanged.
 */
UBOOL FOpenGLDynamicRHI::GetTextureMemoryStats( INT& AllocatedMemorySize, INT& AvailableMemorySize, INT& OutPendingMemoryAdjustment )
{
	if ( GTexturePoolSize > 0 )
	{
		AllocatedMemorySize = GCurrentTextureMemorySize;
		AvailableMemorySize = Max(GTexturePoolSize - GCurrentTextureMemorySize, 0);
		OutPendingMemoryAdjustment = 0;
		return TRUE;
	}
	return FALSE;
}

/**
 * Fills a texture with to visualize the texture pool memory.
 *
 * @param	TextureData		Start address
 * @param	SizeX			Number of pixels along X
 * @param	SizeY			Number of pixels along Y
 * @param	Pitch			Number of bytes between each row
 * @param	PixelSize		Number of bytes each pixel represents
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FOpenGLDynamicRHI::GetTextureMemoryVisualizeData( FColor* /*TextureData*/, INT /*SizeX*/, INT /*SizeY*/, INT /*Pitch*/, INT /*PixelSize*/ )
{
	return FALSE;
}

FOpenGLTexture2D* FOpenGLDynamicRHI::CreateOpenGLTexture(UINT SizeX,UINT SizeY,UBOOL CubeTexture,BYTE Format,UINT NumMips,DWORD Flags)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLCreateTextureTime);

	if(NumMips == 0)
	{
		NumMips = FindMaxMipmapLevel(SizeX, SizeY);
	}

	GLuint TextureID = 0;
	glGenTextures(1, &TextureID);

	GLenum Target = CubeTexture ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

	CachedSetActiveTexture(GL_TEXTURE0);
	glBindTexture(Target, TextureID);

	FOpenGLSamplerState SamplerState;

	glTexParameteri(Target, GL_TEXTURE_WRAP_S, SamplerState.AddressU);
	glTexParameteri(Target, GL_TEXTURE_WRAP_T, SamplerState.AddressV);
	glTexParameteri(Target, GL_TEXTURE_WRAP_R, SamplerState.AddressW);
	glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, SamplerState.MagFilter);
	glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, SamplerState.MinFilter);
	glTexParameteri(Target, GL_TEXTURE_MAX_ANISOTROPY_EXT, SamplerState.MaxAnisotropy);
	glTexParameteri(Target, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(Target, GL_TEXTURE_MAX_LEVEL, NumMips - 1);

	GLenum InternalFormat = GL_RGBA;
	GLenum Type = GL_UNSIGNED_BYTE;
	if (!FindInternalFormatAndType(Format, InternalFormat, Type, (Flags&TexCreate_SRGB)))
	{
		appErrorf(TEXT("FindInternalFormatAndType failed for Format %d"),(int)Format);
	}

	// Make sure PBO is disabled
	CachedBindPixelUnpackBuffer(0);

	for(DWORD MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		GLenum FirstTarget = CubeTexture ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : Target;
		DWORD NumTargets = CubeTexture ? 6 : 1;

		for(DWORD TargetIndex = 0; TargetIndex < NumTargets; TargetIndex++)
		{
			glTexImage2D(FirstTarget + TargetIndex,
						 MipIndex,
						 InternalFormat,
						 Max<UINT>(1,(SizeX >> MipIndex)),
						 Max<UINT>(1,(SizeY >> MipIndex)),
						 0,
						 (GLenum)GPixelFormats[Format].PlatformFormat,
						 Type,
						 NULL);
			CheckOpenGLErrors();
		}
	}

	FOpenGLTexture2D* Texture2D = new FOpenGLTexture2D(this,TextureID,Target,InternalFormat,Type,SizeX,SizeY,NumMips,(EPixelFormat)Format,CubeTexture,Flags&TexCreate_Dynamic);
	if ((Flags & (TexCreate_ResolveTargetable | TexCreate_DepthStencil)) == 0)
	{
		OpenGLTextureAllocated( *Texture2D );
	}

	if (Flags & TexCreate_ResolveTargetable)
	{
		Texture2D->ResolveTarget = CreateSurface(SizeX, SizeY, Format, TextureID, 0, Target);
	}

	// Restore the texture on stage 0
	if (CachedState.Textures[0].Target != GL_NONE)
	{
		glBindTexture(CachedState.Textures[0].Target, CachedState.Textures[0].Resource);
	}

	return Texture2D;
}

template<ERHIResourceTypes ResourceTypeEnum>
void* TOpenGLTexture<ResourceTypeEnum>::Lock(UINT MipIndex,UINT ArrayIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLLockTextureTime);

	// Calculate the dimensions of the mip-map.
	const UINT BlockSizeX = GPixelFormats[Format].BlockSizeX;
	const UINT BlockSizeY = GPixelFormats[Format].BlockSizeY;
	const UINT BlockBytes = GPixelFormats[Format].BlockBytes;
	const UINT MipSizeX = Max(SizeX >> MipIndex,BlockSizeX);
	const UINT MipSizeY = Max(SizeY >> MipIndex,BlockSizeY);
	const UINT NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	const UINT NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;
	const UINT MipBytes = NumBlocksX * NumBlocksY * BlockBytes;

	DestStride = NumBlocksX * BlockBytes;

	const INT BufferIndex = MipIndex * (bCubemap ? 6 : 1) + ArrayIndex;
	if (!IsValidRef(PixelBuffers(BufferIndex)))
	{
		PixelBuffers(BufferIndex) = new FOpenGLPixelBuffer(MipBytes, bDynamic);
	}

	TRefCountPtr<FOpenGLPixelBuffer> PixelBuffer = PixelBuffers(BufferIndex);
	check(!PixelBuffer->IsLocked());

	return PixelBuffer->Lock(0, PixelBuffer->GetSize(), !bIsDataBeingWrittenTo, bDynamic && bIsDataBeingWrittenTo);
}

template<ERHIResourceTypes ResourceTypeEnum>
void TOpenGLTexture<ResourceTypeEnum>::Unlock(UINT MipIndex,UINT ArrayIndex)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLUnlockTextureTime);

	const INT BufferIndex = MipIndex * (bCubemap ? 6 : 1) + ArrayIndex;
	check(IsValidRef(PixelBuffers(BufferIndex)));

	TRefCountPtr<FOpenGLPixelBuffer> PixelBuffer = PixelBuffers(BufferIndex);
	PixelBuffer->Unlock();
	if (!PixelBuffer->IsLockReadOnly())
	{
		OpenGLRHI->CachedSetActiveTexture(GL_TEXTURE0);
		glBindTexture(Target, Resource);

		const UBOOL bIsCompressed = (Format == PF_DXT1) || (Format == PF_DXT3) || (Format == PF_DXT5);
		if (bIsCompressed)
		{
			glCompressedTexImage2D(
				bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
				MipIndex,
				InternalFormat,
				Max<UINT>(1,(SizeX >> MipIndex)),
				Max<UINT>(1,(SizeY >> MipIndex)),
				0,
				PixelBuffer->GetSize(),
				0);	// offset into PBO
		}
		else
		{
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(
				bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
				MipIndex,
				InternalFormat,
				Max<UINT>(1,(SizeX >> MipIndex)),
				Max<UINT>(1,(SizeY >> MipIndex)),
				0,
				(GLenum)GPixelFormats[Format].PlatformFormat,
				Type,
				0);	// offset into PBO
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		}
		CheckOpenGLErrors();

		// Restore the texture on stage 0
		if (OpenGLRHI->CachedState.Textures[0].Target != GL_NONE)
		{
			glBindTexture(OpenGLRHI->CachedState.Textures[0].Target, OpenGLRHI->CachedState.Textures[0].Resource);
		}
	}
	CachedBindPixelUnpackBuffer(0);
}

template<ERHIResourceTypes ResourceTypeEnum>
TOpenGLTexture<ResourceTypeEnum>::~TOpenGLTexture()
{
	OpenGLTextureDeleted( *this );

	if( Resource != 0 )
	{
		OpenGLRHI->InvalidateTextureResourceInCache( Resource );
		glDeleteTextures( 1, &Resource );
	}
}

/*-----------------------------------------------------------------------------
	2D texture support.
-----------------------------------------------------------------------------*/

/**
* Creates a 2D RHI texture resource
* @param SizeX - width of the texture to create
* @param SizeY - height of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
*/
FTexture2DRHIRef FOpenGLDynamicRHI::CreateTexture2D(UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips,DWORD Flags,FResourceBulkDataInterface* BulkData)
{
	return CreateOpenGLTexture(SizeX,SizeY,FALSE,Format,NumMips,Flags);
}

#if PLATFORM_SUPPORTS_D3D10_PLUS
FTexture2DArrayRHIRef FOpenGLDynamicRHI::CreateTexture2DArray(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,DWORD Flags,FResourceBulkDataInterface* BulkData)
{
	// not supported
	return FTexture2DArrayRHIRef();
}

FTexture3DRHIRef FOpenGLDynamicRHI::CreateTexture3D(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,DWORD Flags,const BYTE* Data)
{
	// not supported
	return FTexture3DRHIRef();
}

/** Generates mip maps for the surface. */
void FOpenGLDynamicRHI::GenerateMips(FSurfaceRHIParamRef Surface)
{
	// not supported
}
#endif

/**
 * Tries to reallocate the texture without relocation. Returns a new valid reference to the resource if successful.
 * Both the old and new reference refer to the same texture (at least the shared mip-levels) and both can be used or released independently.
 *
 * @param	Texture2D		- Texture to reallocate
 * @param	NewMipCount		- New number of mip-levels
 * @return					- New reference to the updated texture, or invalid if the reallocation failed
 */
FTexture2DRHIRef FOpenGLDynamicRHI::ReallocateTexture2D( FTexture2DRHIParamRef Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY )
{
	// not supported
	return FTexture2DRHIRef();
}

/**
 * Computes the size in memory required by a given texture.
 *
 * @param	TextureRHI		- Texture we want to know the size of
 * @return					- Size in Bytes
 */
UINT FOpenGLDynamicRHI::GetTextureSize(FTexture2DRHIParamRef TextureRHI)
{
	if(!TextureRHI)
	{
		return 0;
	}

	DYNAMIC_CAST_OPENGLRESOURCE(Texture2D,Texture);
	return Texture->GetMemorySize();
}

/**
 * Starts an asynchronous texture reallocation. It may complete immediately if the reallocation
 * could be performed without any reshuffling of texture memory, or if there isn't enough memory.
 * The specified status counter will be decremented by 1 when the reallocation is complete (success or failure).
 *
 * Returns a new reference to the texture, which will represent the new mip count when the reallocation is complete.
 * RHIGetAsyncReallocateTexture2DStatus() can be used to check the status of an ongoing or completed reallocation.
 *
 * @param Texture2D		- Texture to reallocate
 * @param NewMipCount	- New number of mip-levels
 * @param RequestStatus	- Will be decremented by 1 when the reallocation is complete (success or failure).
 * @return				- New reference to the texture, or an invalid reference upon failure
 */
FTexture2DRHIRef FOpenGLDynamicRHI::AsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY, FThreadSafeCounter* RequestStatus)
{
	// not supported
	return FTexture2DRHIRef();
}

/**
 * Returns the status of an ongoing or completed texture reallocation:
 *	TexRealloc_Succeeded	- The texture is ok, reallocation is not in progress.
 *	TexRealloc_Failed		- The texture is bad, reallocation is not in progress.
 *	TexRealloc_InProgress	- The texture is currently being reallocated async.
 *
 * @param Texture2D		- Texture to check the reallocation status for
 * @return				- Current reallocation status
 */
ETextureReallocationStatus FOpenGLDynamicRHI::FinalizeAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, UBOOL bBlockUntilCompleted )
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
ETextureReallocationStatus FOpenGLDynamicRHI::CancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, UBOOL bBlockUntilCompleted )
{
	// not supported
	return TexRealloc_Failed;
}

void* FOpenGLDynamicRHI::LockTexture2D(FTexture2DRHIParamRef TextureRHI,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Texture2D,Texture);
	return Texture->Lock(MipIndex,0,bIsDataBeingWrittenTo,DestStride);
}

void FOpenGLDynamicRHI::UnlockTexture2D(FTexture2DRHIParamRef TextureRHI,UINT MipIndex,UBOOL bLockWithinMiptail)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Texture2D,Texture);
	Texture->Unlock(MipIndex, 0);
}

#if PLATFORM_SUPPORTS_D3D10_PLUS
void* FOpenGLDynamicRHI::LockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,UINT TextureIndex,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail)
{
	check(0);
	return NULL;
}

void FOpenGLDynamicRHI::UnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,UINT TextureIndex,UINT MipIndex,UBOOL bLockWithinMiptail)
{
	check(0);
}
#endif

/**
 * Checks if a texture is still in use by the GPU.
 * @param Texture - the RHI texture resource to check
 * @param MipIndex - Which mipmap we're interested in
 * @return TRUE if the texture is still in use by the GPU, otherwise FALSE
 */
UBOOL FOpenGLDynamicRHI::IsBusyTexture2D(FTexture2DRHIParamRef Texture,UINT MipIndex)
{
	//@todo opengl: Implement somehow!
	return FALSE;
}

/**
* Updates a region of a 2D texture from system memory
* @param Texture - the RHI texture resource to update
* @param MipIndex - mip level index to be modified
* @param n - number of rectangles to copy
* @param rects - rectangles to copy from source image data
* @param pitch - size in bytes of each line of source image
* @param sbpp - size in bytes of each pixel of source image (must match texture, passed in because some drivers do not maintain it in refs)
* @param psrc - source image data (in same pixel format as texture)
*/
UBOOL FOpenGLDynamicRHI::UpdateTexture2D(FTexture2DRHIParamRef TextureRHI,UINT MipIndex,UINT n,const FUpdateTextureRegion2D* rects,UINT pitch,UINT sbpp,BYTE* psrc)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Texture2D,Texture);

	CachedSetActiveTexture(GL_TEXTURE0);
	glBindTexture(Texture->Target, Texture->Resource);

	CachedBindPixelUnpackBuffer(0);

	const UBOOL bIsCompressed = (Texture->Format == PF_DXT1) || (Texture->Format == PF_DXT3) || (Texture->Format == PF_DXT5);

	for (UINT i = 0; i < n; i++)
	{
		const FUpdateTextureRegion2D& rect = rects[i];
		BYTE *Pixels = psrc + pitch * rect.SrcY + sbpp * rect.SrcX;

		glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / sbpp);

		if (bIsCompressed)
		{
			UINT BytesPerPixel = (Texture->Format == PF_DXT1) ? 2 : 4;
			UINT RectSize = ((rect.Width + 3) / 4) * ((rect.Height + 3) / 4) * 4 * BytesPerPixel;
			glCompressedTexSubImage2D(Texture->Target, MipIndex, rect.DestX, rect.DestY, rect.Width, rect.Height,
				(GLenum)GPixelFormats[Texture->Format].PlatformFormat, RectSize, Pixels);
		}
		else
		{
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexSubImage2D(Texture->Target, MipIndex, rect.DestX, rect.DestY, rect.Width, rect.Height,
				(GLenum)GPixelFormats[Texture->Format].PlatformFormat, Texture->Type, Pixels);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		}

		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		CheckOpenGLErrors();
	}

	// Restore the texture on stage 0
	if (CachedState.Textures[0].Target != GL_NONE)
	{
		glBindTexture(CachedState.Textures[0].Target, CachedState.Textures[0].Resource);
	}

	return TRUE;
}

INT FOpenGLDynamicRHI::GetMipTailIdx(FTexture2DRHIParamRef Texture)
{
	return -1;
}

void FOpenGLDynamicRHI::CopyTexture2D(FTexture2DRHIParamRef DestTextureRHI, UINT MipIdx, INT BaseSizeX, INT BaseSizeY, INT Format, const TArray<FCopyTextureRegion2D>& Regions)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Texture2D,DestTexture);
	check( DestTexture );

	// scale the base SizeX,SizeY for the current mip level
	INT DestMipSizeX = Max((INT)GPixelFormats[Format].BlockSizeX,BaseSizeX >> MipIdx);
	INT DestMipSizeY = Max((INT)GPixelFormats[Format].BlockSizeY,BaseSizeY >> MipIdx);

	// lock the destination texture
	UINT DstStride;
	BYTE* DstData = (BYTE*)RHILockTexture2D( DestTexture, MipIdx, TRUE, DstStride, FALSE );

	for( INT RegionIdx=0; RegionIdx < Regions.Num(); RegionIdx++ )		
	{
		const FCopyTextureRegion2D& Region = Regions(RegionIdx);
		check( Region.SrcTexture );

		// Get the source texture for this region
		UObject* BaseTextureObject = (UObject*)Region.SrcTextureObject;
		UTexture2D* SrcTexture = Cast<UTexture2D>(BaseTextureObject);
		check( SrcTexture );

		// lock source RHI texture
		UINT SrcStride=0;
		BYTE* SrcData = (BYTE*)RHILockTexture2D( 
			Region.SrcTexture,
			MipIdx,
			FALSE,
			SrcStride,
			FALSE
			);	

		// Calculate source values
		INT SrcSizeX = SrcTexture->SizeX >> (MipIdx + Region.FirstMipIdx);
		INT SrcSizeY = SrcTexture->SizeY >> (MipIdx + Region.FirstMipIdx);
		INT SrcMipSizeX = Max((INT)GPixelFormats[Format].BlockSizeX,SrcSizeX);
		INT SrcMipSizeY = Max((INT)GPixelFormats[Format].BlockSizeY,SrcSizeY);

		// Source region offsets
		INT SrcRegionOffsetX = (Clamp( Region.OffsetX, 0, SrcMipSizeX - GPixelFormats[Format].BlockSizeX ) / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockSizeX;
		INT SrcRegionOffsetY = (Clamp( Region.OffsetY, 0, SrcMipSizeY - GPixelFormats[Format].BlockSizeY ) / GPixelFormats[Format].BlockSizeY) * GPixelFormats[Format].BlockSizeY;

		// Destination region offsets
		INT DestRegionOffsetX = SrcRegionOffsetX;
		if( Region.DestOffsetX >= 0 )
		{
			DestRegionOffsetX = (Clamp( Region.DestOffsetX, 0, DestMipSizeX - GPixelFormats[Format].BlockSizeX ) / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockSizeX;
		}
		INT DestRegionOffsetY = SrcRegionOffsetY;
		if( Region.DestOffsetY >= 0 )
		{
			DestRegionOffsetY = (Clamp( Region.DestOffsetY, 0, DestMipSizeY - GPixelFormats[Format].BlockSizeY ) / GPixelFormats[Format].BlockSizeY) * GPixelFormats[Format].BlockSizeY;
		}
		
		
		// scale region size to the current mip level. Size is aligned to the block size
		check(Region.SizeX != 0 && Region.SizeY != 0);
		INT RegionSizeX = Clamp( Align( Region.SizeX, GPixelFormats[Format].BlockSizeX), 0, SrcMipSizeX );
		INT RegionSizeY = Clamp( Align( Region.SizeY, GPixelFormats[Format].BlockSizeY), 0, SrcMipSizeY );
		// handle special case for full copy
		if( Region.SizeX == -1 || Region.SizeY == -1 )
		{
			RegionSizeX = SrcMipSizeX;
			RegionSizeY = SrcMipSizeY;
		}

		// size in bytes of an entire row for this mip
		DWORD SrcPitchBytes = (SrcMipSizeX / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockBytes;
		DWORD DestPitchBytes = (DestMipSizeX / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockBytes;

		// size in bytes of the offset to the starting part of the row to copy for this mip
		DWORD SrcRowOffsetBytes = (SrcRegionOffsetX / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockBytes;
		DWORD DestRowOffsetBytes = (DestRegionOffsetX / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockBytes;

		// size in bytes of the amount to copy within each row
		DWORD RowSizeBytes = (RegionSizeX / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockBytes;

		// copy each region row in increments of the block size
		INT CurDestOffsetY = DestRegionOffsetY;
		for( INT CurSrcOffsetY=SrcRegionOffsetY; CurSrcOffsetY < (SrcRegionOffsetY+RegionSizeY); CurSrcOffsetY += GPixelFormats[Format].BlockSizeY )
		{
			INT CurSrcBlockOffsetY = CurSrcOffsetY / GPixelFormats[Format].BlockSizeY;
			INT CurDestBlockOffsetY = CurDestOffsetY / GPixelFormats[Format].BlockSizeY;

			BYTE* SrcOffset = SrcData + (CurSrcBlockOffsetY * SrcPitchBytes) + SrcRowOffsetBytes;
			BYTE* DstOffset = DstData + (CurDestBlockOffsetY * DestPitchBytes) + DestRowOffsetBytes;
			appMemcpy( DstOffset, SrcOffset, RowSizeBytes );

			CurDestOffsetY += GPixelFormats[Format].BlockSizeY;
		}

		// done reading from source mip so unlock it
		RHIUnlockTexture2D( Region.SrcTexture, MipIdx, FALSE );
	}

	// unlock the destination texture
	RHIUnlockTexture2D( DestTexture, MipIdx, FALSE );
}

void FOpenGLDynamicRHI::CopyMipToMipAsync(FTexture2DRHIParamRef SrcTextureRHI, INT SrcMipIndex, FTexture2DRHIParamRef DestTextureRHI, INT DestMipIndex, INT Size, FThreadSafeCounter& Counter)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Texture2D,SrcTexture);
	DYNAMIC_CAST_OPENGLRESOURCE(Texture2D,DestTexture);

	// Lock old and new texture.
	UINT SrcStride;
	UINT DestStride;

	void* Src = RHILockTexture2D( SrcTexture, SrcMipIndex, FALSE, SrcStride, FALSE );
	void* Dst = RHILockTexture2D( DestTexture, DestMipIndex, TRUE, DestStride, FALSE );
	check(SrcStride == DestStride);
	appMemcpy( Dst, Src, Size );
	RHIUnlockTexture2D( SrcTexture, SrcMipIndex, FALSE );
	RHIUnlockTexture2D( DestTexture, DestMipIndex, FALSE );
}

/**
 * Copies mip data from one location to another, selectively copying only used memory based on
 * the texture tiling memory layout of the given mip level
 * Note that the mips must be the same size and of the same format.
 * @param Texture - texture to base memory layout on
 * @param Src - source memory base address to copy from
 * @param Dst - destination memory base address to copy to
 * @param MemSize - total size of mip memory
 * @param MipIdx - mip index to base memory layout on
 */
void FOpenGLDynamicRHI::SelectiveCopyMipData(FTexture2DRHIParamRef Texture, BYTE *Src, BYTE *Dst, UINT MemSize, UINT MipIdx)
{
	appMemcpy(Dst, Src, MemSize);
}

void FOpenGLDynamicRHI::FinalizeAsyncMipCopy(FTexture2DRHIParamRef SrcTextureRHI, INT SrcMipIndex, FTexture2DRHIParamRef DestTextureRHI, INT DestMipIndex)
{
}

void FOpenGLDynamicRHI::InvalidateTextureResourceInCache(GLuint Resource)
{
	for (INT SamplerIndex = 0; SamplerIndex < 16; ++SamplerIndex)
	{
		if (CachedState.Textures[SamplerIndex].Resource == Resource)
		{
			CachedState.Textures[SamplerIndex].Resource = 0;
		}
	}
}

/*-----------------------------------------------------------------------------
Shared texture support.
-----------------------------------------------------------------------------*/
FSharedMemoryResourceRHIRef FOpenGLDynamicRHI::CreateSharedMemory(EGPUMemoryType MemType,SIZE_T Size)
{
	// create the shared memory resource
	FSharedMemoryResourceRHIRef SharedMemory(NULL);
	return SharedMemory;
}

FSharedTexture2DRHIRef FOpenGLDynamicRHI::CreateSharedTexture2D(UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips,FSharedMemoryResourceRHIParamRef SharedMemoryRHI,DWORD Flags)
{
	DYNAMIC_CAST_OPENGLRESOURCE(SharedMemoryResource,SharedMemory);
	return FSharedTexture2DRHIRef();
}

FSharedTexture2DArrayRHIRef FOpenGLDynamicRHI::CreateSharedTexture2DArray(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,FSharedMemoryResourceRHIParamRef SharedMemoryRHI,DWORD Flags)
{
	// not supported on that platform
	check(0);
	return NULL;
}

/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/
FTextureCubeRHIRef FOpenGLDynamicRHI::CreateTextureCube( UINT Size, BYTE Format, UINT NumMips, DWORD Flags, FResourceBulkDataInterface* BulkData )
{
	return (FOpenGLTextureCube*)CreateOpenGLTexture(Size,Size,TRUE,Format,NumMips,Flags);
}

void* FOpenGLDynamicRHI::LockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,UINT FaceIndex,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail)
{
	DYNAMIC_CAST_OPENGLRESOURCE(TextureCube,TextureCube);
	return TextureCube->Lock(MipIndex,FaceIndex,bIsDataBeingWrittenTo,DestStride);
}

void FOpenGLDynamicRHI::UnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,UINT FaceIndex,UINT MipIndex,UBOOL bLockWithinMiptail)
{
	DYNAMIC_CAST_OPENGLRESOURCE(TextureCube,TextureCube);
	TextureCube->Unlock(MipIndex,FaceIndex);
}

FTexture2DRHIRef FOpenGLDynamicRHI::CreateStereoFixTexture()
{
	// @todo opengl
	return NULL;
}
