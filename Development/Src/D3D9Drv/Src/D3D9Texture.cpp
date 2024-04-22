/*=============================================================================
	D3D9Texture.cpp: D3D texture RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D9DrvPrivate.h"

/** In bytes. */
extern INT GCurrentTextureMemorySize;
/** In bytes. 0 means unlimited. */
extern INT GTexturePoolSize;

/** Determines the usage flags of a RHI texture based on the creation flags. */
static DWORD GetD3DTextureUsageFlags(DWORD RHIFlags)
{
	DWORD D3DUsageFlags = 0;
	if (RHIFlags & TexCreate_ResolveTargetable)
	{
		D3DUsageFlags |= D3DUSAGE_RENDERTARGET;
	}
	if (RHIFlags & TexCreate_DepthStencil)
	{
		D3DUsageFlags |= D3DUSAGE_DEPTHSTENCIL;
	}
#if 0
	// Don't use the D3DUSAGE_DYNAMIC flag for dynamic RHI textures, as they can only be written to indirectly
	// through a D3DPOOL_SYSTEMMEM texture.
	if (RHIFlags & TexCreate_Dynamic)
	{
		D3DUsageFlags |= D3DUSAGE_DYNAMIC;
	}
#endif
	return D3DUsageFlags;
}

/** Determines the pool to create a RHI texture in based on the creation flags. */
static D3DPOOL GetD3DTexturePool(DWORD RHIFlags)
{
	if(RHIFlags & (TexCreate_ResolveTargetable | TexCreate_DepthStencil/* | TexCreate_Dynamic*/))
	{
		// Put resolve targets, depth stencil textures, and dynamic textures in the default pool.
		return D3DPOOL_DEFAULT;
	}
	else
	{
		// All other textures go in the managed pool.
		return D3DPOOL_MANAGED;
	}
}

/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

/**
 * Retrieves texture memory stats. Unsupported with this allocator.
 *
 * @return FALSE, indicating that out variables were left unchanged.
 */
UBOOL FD3D9DynamicRHI::GetTextureMemoryStats( INT& AllocatedMemorySize, INT& AvailableMemorySize, INT& OutPendingMemoryAdjustment )
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
UBOOL FD3D9DynamicRHI::GetTextureMemoryVisualizeData( FColor* /*TextureData*/, INT /*SizeX*/, INT /*SizeY*/, INT /*Pitch*/, INT /*PixelSize*/ )
{
	return FALSE;
}

void D3D9TextureAllocated( FD3D9Texture2D& Texture )
{
	UINT NumMips = Texture->GetLevelCount();
	D3DSURFACE_DESC Desc;
	Texture->GetLevelDesc( 0, &Desc );
	INT TextureSize = CalcTextureSize( Desc.Width, Desc.Height, Texture.GetUnrealFormat(), NumMips );
	Texture.SetMemorySize( TextureSize );
	GCurrentTextureMemorySize += TextureSize;
}

void D3D9TextureDeleted( FD3D9Texture2D& Texture )
{
	INT TextureSize = Texture.GetMemorySize();
	GCurrentTextureMemorySize -= TextureSize;
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
FTexture2DRHIRef FD3D9DynamicRHI::CreateTexture2D(UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips,DWORD Flags,FResourceBulkDataInterface* BulkData)
{
    FD3D9Texture2D* Texture2D = new FD3D9Texture2D( EPixelFormat(Format), Flags&TexCreate_SRGB, Flags&TexCreate_Dynamic );
	VERIFYD3D9CREATETEXTURERESULT(Direct3DDevice->CreateTexture(
		SizeX,
		SizeY,
		NumMips,
		GetD3DTextureUsageFlags(Flags),
		(D3DFORMAT)GPixelFormats[Format].PlatformFormat,
		GetD3DTexturePool(Flags),
		Texture2D->GetInitReference(),
		NULL
		),
		SizeX, SizeY, Format, NumMips, GetD3DTextureUsageFlags(Flags));
	if ( (Flags & (TexCreate_ResolveTargetable | TexCreate_DepthStencil)) == 0 )
	{
		D3D9TextureAllocated( *Texture2D );
	}
	return Texture2D;
}

FTexture2DArrayRHIRef FD3D9DynamicRHI::CreateTexture2DArray(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,DWORD Flags,FResourceBulkDataInterface* BulkData)
{
	// not supported
	return FTexture2DArrayRHIRef();
}

FTexture3DRHIRef FD3D9DynamicRHI::CreateTexture3D(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,DWORD Flags,const BYTE* Data)
{
	// not supported
	return FTexture3DRHIRef();
}

/** Generates mip maps for the surface. */
void FD3D9DynamicRHI::GenerateMips(FSurfaceRHIParamRef Surface)
{
	// not supported
}

/**
 * Tries to reallocate the texture without relocation. Returns a new valid reference to the resource if successful.
 * Both the old and new reference refer to the same texture (at least the shared mip-levels) and both can be used or released independently.
 *
 * @param Texture2D		- Texture to reallocate
 * @param NewMipCount	- New number of mip-levels
 * @param NewSizeX		- New width, in pixels
 * @param NewSizeY		- New height, in pixels
 * @return				- New reference to the updated texture, or invalid if the reallocation failed
 */
FTexture2DRHIRef FD3D9DynamicRHI::ReallocateTexture2D( FTexture2DRHIParamRef Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY )
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
UINT FD3D9DynamicRHI::GetTextureSize(FTexture2DRHIParamRef TextureRHI)
{
	if(!TextureRHI)
	{
		return 0;
	}

	DYNAMIC_CAST_D3D9RESOURCE(Texture2D,Texture);
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
 * @param NewSizeX		- New width, in pixels
 * @param NewSizeY		- New height, in pixels
 * @param RequestStatus	- Will be decremented by 1 when the reallocation is complete (success or failure).
 * @return				- New reference to the texture, or an invalid reference upon failure
 */
FTexture2DRHIRef FD3D9DynamicRHI::AsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY, FThreadSafeCounter* RequestStatus )
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
ETextureReallocationStatus FD3D9DynamicRHI::FinalizeAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, UBOOL bBlockUntilCompleted )
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
ETextureReallocationStatus FD3D9DynamicRHI::CancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, UBOOL bBlockUntilCompleted )
{
	// not supported
	return TexRealloc_Failed;
}

/**
* Locks an RHI texture's mip surface for read/write operations on the CPU
* @param Texture - the RHI texture resource to lock
* @param MipIndex - mip level index for the surface to retrieve
* @param bIsDataBeingWrittenTo - used to affect the lock flags 
* @param DestStride - output to retrieve the textures row stride (pitch)
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
* @return pointer to the CPU accessible resource data
*/
void* FD3D9DynamicRHI::LockTexture2D(FTexture2DRHIParamRef TextureRHI,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail)
{
	DYNAMIC_CAST_D3D9RESOURCE(Texture2D,Texture);

	D3DLOCKED_RECT	LockedRect;
	DWORD			LockFlags = D3DLOCK_NOSYSLOCK;
	if( !bIsDataBeingWrittenTo )
	{
		LockFlags |= D3DLOCK_READONLY;
	}
	if( Texture->IsDynamic() )
	{
		// Discard the previous contents of the texture if it's dynamic.
		LockFlags |= D3DLOCK_DISCARD;
	}
	VERIFYD3D9RESULT((*Texture)->LockRect(MipIndex,&LockedRect,NULL,LockFlags));
	DestStride = LockedRect.Pitch;
	return LockedRect.pBits;
}

/**
* Unlocks a previously locked RHI texture resource
* @param Texture - the RHI texture resource to unlock
* @param MipIndex - mip level index for the surface to unlock
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
*/
void FD3D9DynamicRHI::UnlockTexture2D(FTexture2DRHIParamRef TextureRHI,UINT MipIndex,UBOOL bLockWithinMiptail)
{
	DYNAMIC_CAST_D3D9RESOURCE(Texture2D,Texture);
	VERIFYD3D9RESULT((*Texture)->UnlockRect(MipIndex));
}

void* FD3D9DynamicRHI::LockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,UINT TextureIndex,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail)
{
	check(0);
	return NULL;
}

void FD3D9DynamicRHI::UnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,UINT TextureIndex,UINT MipIndex,UBOOL bLockWithinMiptail)
{
	check(0);
}

/**
 * Checks if a texture is still in use by the GPU.
 * @param Texture - the RHI texture resource to check
 * @param MipIndex - Which mipmap we're interested in
 * @return TRUE if the texture is still in use by the GPU, otherwise FALSE
 */
UBOOL FD3D9DynamicRHI::IsBusyTexture2D(FTexture2DRHIParamRef Texture, UINT MipIndex)
{
	//@TODO: Implement somehow!
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
UBOOL FD3D9DynamicRHI::UpdateTexture2D(FTexture2DRHIParamRef TextureRHI,UINT MipIndex,UINT n,const FUpdateTextureRegion2D* rects,UINT pitch,UINT sbpp,BYTE* psrc)
{
    DYNAMIC_CAST_D3D9RESOURCE(Texture2D,Texture);

    DWORD LockFlags = D3DLOCK_NOSYSLOCK;
    if( Texture->IsDynamic() )
    {
        // Discard the previous contents of the texture if it's dynamic.
        LockFlags |= D3DLOCK_DISCARD;
    }
    for (UINT i = 0; i < n; i++)
    {
        const FUpdateTextureRegion2D& rect = rects[i];
        D3DLOCKED_RECT	LockedRect;
        RECT            destr;

        destr.left	 = rect.DestX;
        destr.bottom = rect.DestY + rect.Height;
        destr.right  = rect.DestX + rect.Width;
        destr.top    = rect.DestY;

        VERIFYD3D9RESULT((*Texture)->LockRect(MipIndex,&LockedRect,&destr,LockFlags));

        for (int j = 0; j < rect.Height; j++)
            appMemcpy(((BYTE*)LockedRect.pBits) + j * LockedRect.Pitch,
                psrc + pitch * (j + rect.SrcY) + sbpp * rect.SrcX,
                rect.Width * sbpp);

        VERIFYD3D9RESULT((*Texture)->UnlockRect(MipIndex));
    }

    return TRUE;
}

/**
* For platforms that support packed miptails return the first mip level which is packed in the mip tail
* @return mip level for mip tail or -1 if mip tails are not used
*/
INT FD3D9DynamicRHI::GetMipTailIdx(FTexture2DRHIParamRef Texture)
{
	return -1;
}

/**
* Copies a region within the same mip levels of one texture to another.  An optional region can be speci
* Note that the textures must be the same size and of the same format.
* @param DstTexture - destination texture resource to copy to
* @param MipIdx - mip level for the surface to copy from/to. This mip level should be valid for both source/destination textures
* @param BaseSizeX - width of the texture (base level). Same for both source/destination textures
* @param BaseSizeY - height of the texture (base level). Same for both source/destination textures 
* @param Format - format of the texture. Same for both source/destination textures
* @param Region - list of regions to specify rects and source textures for the copy
*/
void FD3D9DynamicRHI::CopyTexture2D(FTexture2DRHIParamRef DstTextureRHI, UINT MipIdx, INT BaseSizeX, INT BaseSizeY, INT Format, const TArray<FCopyTextureRegion2D>& Regions)
{
	DYNAMIC_CAST_D3D9RESOURCE(Texture2D,DstTexture);
	check( DstTexture );

	// scale the base SizeX,SizeY for the current mip level
	INT DestMipSizeX = Max((INT)GPixelFormats[Format].BlockSizeX,BaseSizeX >> MipIdx);
	INT DestMipSizeY = Max((INT)GPixelFormats[Format].BlockSizeY,BaseSizeY >> MipIdx);

	// lock the destination texture
	UINT DstStride;
	BYTE* DstData = (BYTE*)RHILockTexture2D( DstTexture, MipIdx, TRUE, DstStride, FALSE );

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
	RHIUnlockTexture2D( DstTexture, MipIdx, FALSE );
}

/**
 * Copies texture data from one mip to another
 * Note that the mips must be the same size and of the same format.
 * @param SrcText Source texture to copy from
 * @param SrcMipIndex Mip index into the source texture to copy data from
 * @param DestText Destination texture to copy to
 * @param DestMipIndex Mip index in the destination texture to copy to - note this is probably different from source mip index if the base widths/heights are different
 * @param Size Size of mip data
 * @param Counter Thread safe counter used to flag end of transfer
 */
void FD3D9DynamicRHI::CopyMipToMipAsync(FTexture2DRHIParamRef SrcTextureRHI, INT SrcMipIndex, FTexture2DRHIParamRef DestTextureRHI, INT DestMipIndex, INT Size, FThreadSafeCounter& Counter)
{
	DYNAMIC_CAST_D3D9RESOURCE(Texture2D,SrcTexture);
	DYNAMIC_CAST_D3D9RESOURCE(Texture2D,DestTexture);

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
void FD3D9DynamicRHI::SelectiveCopyMipData(FTexture2DRHIParamRef Texture, BYTE *Src, BYTE *Dst, UINT MemSize, UINT MipIdx)
{
	appMemcpy(Dst, Src, MemSize);
}

void FD3D9DynamicRHI::FinalizeAsyncMipCopy(FTexture2DRHIParamRef SrcTextureRHI, INT SrcMipIndex, FTexture2DRHIParamRef DestTextureRHI, INT DestMipIndex)
{
}

/*-----------------------------------------------------------------------------
Shared texture support.
-----------------------------------------------------------------------------*/

/**
* Create resource memory to be shared by multiple RHI resources
* @param Size - aligned size of allocation
* @return shared memory resource RHI ref
*/
FSharedMemoryResourceRHIRef FD3D9DynamicRHI::CreateSharedMemory(EGPUMemoryType MemType,SIZE_T Size)
{
	// create the shared memory resource
	FSharedMemoryResourceRHIRef SharedMemory(NULL);
	return SharedMemory;
}

/**
 * Creates a RHI texture and if the platform supports it overlaps it in memory with another texture
 * Note that modifying this texture will modify the memory of the overlapped texture as well
 * @param SizeX - The width of the surface to create.
 * @param SizeY - The height of the surface to create.
 * @param Format - The surface format to create.
 * @param ResolveTargetTexture - The 2d texture to use the memory from if the platform allows
 * @param Flags - Surface creation flags
 * @return The surface that was created.
 */
FSharedTexture2DRHIRef FD3D9DynamicRHI::CreateSharedTexture2D(UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips,FSharedMemoryResourceRHIParamRef SharedMemoryRHI,DWORD Flags)
{
	DYNAMIC_CAST_D3D9RESOURCE(SharedMemoryResource,SharedMemory);

	FD3D9SharedTexture2D* Texture2D = new FD3D9SharedTexture2D( EPixelFormat(Format), Flags&TexCreate_SRGB, (GetD3DTextureUsageFlags(Flags)&D3DUSAGE_DYNAMIC) );
	VERIFYD3D9RESULT(Direct3DDevice->CreateTexture(
		SizeX,
		SizeY,
		NumMips,
		GetD3DTextureUsageFlags(Flags),
		(D3DFORMAT)GPixelFormats[Format].PlatformFormat,
		GetD3DTexturePool(Flags),
		Texture2D->GetInitReference(),
		NULL
		));

	return Texture2D;
}

FSharedTexture2DArrayRHIRef FD3D9DynamicRHI::CreateSharedTexture2DArray(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,FSharedMemoryResourceRHIParamRef SharedMemoryRHI,DWORD Flags)
{
	// not supported on that platform
	check(0);
	return NULL;
}

/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/

/**
* Creates a Cube RHI texture resource
* @param Size - width/height of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
*/
FTextureCubeRHIRef FD3D9DynamicRHI::CreateTextureCube( UINT Size, BYTE Format, UINT NumMips, DWORD Flags,FResourceBulkDataInterface* BulkData )
{
	FD3D9TextureCube* TextureCube = new FD3D9TextureCube( EPixelFormat(Format), Flags&TexCreate_SRGB, (GetD3DTextureUsageFlags(Flags)&D3DUSAGE_DYNAMIC) );
	VERIFYD3D9RESULT( Direct3DDevice->CreateCubeTexture(
		Size,
		NumMips,
		GetD3DTextureUsageFlags(Flags),
		(D3DFORMAT)GPixelFormats[Format].PlatformFormat,
		GetD3DTexturePool(Flags),
		TextureCube->GetInitReference(),
		NULL
		));
	return TextureCube;
}

/**
* Locks an RHI texture's mip surface for read/write operations on the CPU
* @param Texture - the RHI texture resource to lock
* @param MipIndex - mip level index for the surface to retrieve
* @param bIsDataBeingWrittenTo - used to affect the lock flags 
* @param DestStride - output to retrieve the textures row stride (pitch)
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
* @return pointer to the CPU accessible resource data
*/
void* FD3D9DynamicRHI::LockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,UINT FaceIndex,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail)
{
	DYNAMIC_CAST_D3D9RESOURCE(TextureCube,TextureCube);

	D3DLOCKED_RECT LockedRect;
	VERIFYD3D9RESULT((*TextureCube)->LockRect( (D3DCUBEMAP_FACES) FaceIndex, MipIndex, &LockedRect, NULL, 0 ));
	DestStride = LockedRect.Pitch;
	return LockedRect.pBits;
}

/**
* Unlocks a previously locked RHI texture resource
* @param Texture - the RHI texture resource to unlock
* @param MipIndex - mip level index for the surface to unlock
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
*/
void FD3D9DynamicRHI::UnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,UINT FaceIndex,UINT MipIndex,UBOOL bLockWithinMiptail)
{
	DYNAMIC_CAST_D3D9RESOURCE(TextureCube,TextureCube);

	VERIFYD3D9RESULT((*TextureCube)->UnlockRect( (D3DCUBEMAP_FACES) FaceIndex, MipIndex ));
}

FTexture2DRHIRef FD3D9DynamicRHI::CreateStereoFixTexture()
{
    using nv::stereo::UE3StereoD3D9;

	// Need to change the EPixelFormat in the FD3D9Texture2D constructor call if this changes.
	checkAtCompileTime( UE3StereoD3D9::Parms::StereoTexFormat == D3DFMT_G32R32F, MismatchingStereoTexFormat );

    FD3D9Texture2D* Texture2D = new FD3D9Texture2D( PF_G32R32F );
	if (FAILED(Direct3DDevice->CreateTexture(
		UE3StereoD3D9::Parms::StereoTexWidth,
		UE3StereoD3D9::Parms::StereoTexHeight,
		1,
		0, 
		UE3StereoD3D9::Parms::StereoTexFormat, 
		D3DPOOL_DEFAULT, 
		Texture2D->GetInitReference(), 
		NULL )))
	{
		delete Texture2D;
		return NULL;
	}

    return Texture2D;
}
