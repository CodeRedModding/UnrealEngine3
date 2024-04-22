/*=============================================================================
	D3D11VertexBuffer.cpp: D3D texture RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D11DrvPrivate.h"

/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/
/** In bytes. */
extern INT GCurrentTextureMemorySize;
/** In bytes. 0 means unlimited. */
extern INT GTexturePoolSize;

void D3D11TextureAllocated( FD3D11Texture2D& Texture )
{
	D3D11_TEXTURE2D_DESC Desc;
	Texture.Resource->GetDesc( &Desc );
	INT TextureSize = CalcTextureSize( Desc.Width, Desc.Height, Texture.Format, Desc.MipLevels );
	
	Texture.SetMemorySize( TextureSize );
	GCurrentTextureMemorySize += TextureSize;
}

void D3D11TextureDeleted( FD3D11Texture2D& Texture )
{
	INT TextureSize = Texture.GetMemorySize();
	GCurrentTextureMemorySize -= TextureSize;
}

/**
	* Retrieves texture memory stats. 
	*
	* @return UBOOL indicating that out variables were left unchanged.
	*/
UBOOL FD3D11DynamicRHI::GetTextureMemoryStats( INT& AllocatedMemorySize, INT& AvailableMemorySize, INT& OutPendingMemoryAdjustmen )
{
	if ( GTexturePoolSize > 0 )
	{
		AllocatedMemorySize = GCurrentTextureMemorySize;
		AvailableMemorySize = Max(GTexturePoolSize - GCurrentTextureMemorySize, 0);
		OutPendingMemoryAdjustmen = 0;
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
UBOOL FD3D11DynamicRHI::GetTextureMemoryVisualizeData( FColor* /*TextureData*/, INT /*SizeX*/, INT /*SizeY*/, INT /*Pitch*/, INT /*PixelSize*/ )
{
	return FALSE;
}

FD3D11Texture2D* FD3D11DynamicRHI::CreateD3D11Texture2D(UINT SizeX,UINT SizeY,UINT SizeZ,UBOOL bTextureArray,UBOOL CubeTexture,BYTE Format,UINT NumMips,DWORD Flags)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11CreateTextureTime);

	DXGI_FORMAT PlatformFormat = FindDXGIFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat,Flags&TexCreate_SRGB);
	if( GIsEditor && (Flags & TexCreate_ResolveTargetable) == 0 )
	{
		// In the editor we'll use a typeless format so that we can dynamically switch between SRGB and linear
		// texture sampling using separate shader resource views.  This is used for mobile rendering emulation.
		PlatformFormat = FindTypelessDXGIFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat);
	}
	D3D11_TEXTURE2D_DESC TextureDesc;
	TextureDesc.Width = SizeX;
	TextureDesc.Height = SizeY;
	TextureDesc.MipLevels = NumMips;
	TextureDesc.ArraySize = SizeZ;
	TextureDesc.Format = PlatformFormat;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.SampleDesc.Quality = 0;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = CubeTexture ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

	if (Flags & TexCreate_GenerateMipCapable)
	{
		// Set the flag that allows us to call GenerateMips on this texture later
		TextureDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
	}

	if (Flags & TexCreate_DepthStencil)
	{
		TextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL; 
	}
	else if (Flags & TexCreate_ResolveTargetable)
	{
		if(Format == PF_DepthStencil || Format == PF_ShadowDepth|| Format == PF_FilteredShadowDepth || Format == PF_D24)
		{
			TextureDesc.BindFlags |= D3D11_BIND_DEPTH_STENCIL; 
		}
		else
		{
			TextureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET; 
		}
	}

	if (PlatformFormat == DXGI_FORMAT_R24G8_TYPELESS)
	{
		// A typeless resource format implies that the resource will be bound as a shader resource
		TextureDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	}

	TRefCountPtr<ID3D11Texture2D> TextureResource;
	VERIFYD3D11CREATETEXTURERESULT(
		Direct3DDevice->CreateTexture2D(
			&TextureDesc,
			NULL,
			TextureResource.GetInitReference()),
		SizeX,
		SizeY,
		PlatformFormat,
		NumMips,
		TextureDesc.BindFlags
		);

	// Create a shader resource view for the texture.
	TRefCountPtr<ID3D11ShaderResourceView> TextureView;
	TRefCountPtr<ID3D11ShaderResourceView> TextureViewLinear;
	if ((TextureDesc.BindFlags & D3D11_BIND_RENDER_TARGET && !(Flags&TargetSurfCreate_Dedicated)) ||
		TextureDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		SRVDesc.Format = FindDXGIFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat,Flags&TexCreate_SRGB);;

		if (TextureDesc.Format == DXGI_FORMAT_R24G8_TYPELESS)
		{
			// Use the typed shader resource view format corresponding to DXGI_FORMAT_R24G8_TYPELESS
			SRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		}

		SRVDesc.ViewDimension = CubeTexture ? D3D11_SRV_DIMENSION_TEXTURECUBE : (bTextureArray ? D3D_SRV_DIMENSION_TEXTURE2DARRAY : D3D11_SRV_DIMENSION_TEXTURE2D);

		if (SRVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBE)
		{
			SRVDesc.TextureCube.MostDetailedMip = 0;
			SRVDesc.TextureCube.MipLevels = NumMips;
		}
		else if (SRVDesc.ViewDimension == D3D_SRV_DIMENSION_TEXTURE2DARRAY)
		{
			SRVDesc.Texture2DArray.MostDetailedMip = 0;
			SRVDesc.Texture2DArray.MipLevels = NumMips;
			SRVDesc.Texture2DArray.FirstArraySlice = 0;
			SRVDesc.Texture2DArray.ArraySize = TextureDesc.ArraySize;
		}
		else if (SRVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D)
		{
			SRVDesc.Texture2D.MostDetailedMip = 0;
			SRVDesc.Texture2D.MipLevels = NumMips;
		}

		VERIFYD3D11RESULT(Direct3DDevice->CreateShaderResourceView(TextureResource,&SRVDesc,TextureView.GetInitReference()));

		if( GIsEditor && (Flags&TexCreate_SRGB) && (Flags&TexCreate_ResolveTargetable)==0 )
		{
			SRVDesc.Format = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;
			if (TextureDesc.Format == DXGI_FORMAT_R24G8_TYPELESS)
			{
				// Use the typed shader resource view format corresponding to DXGI_FORMAT_R24G8_TYPELESS
				SRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			}
			VERIFYD3D11RESULT(Direct3DDevice->CreateShaderResourceView(TextureResource,&SRVDesc,TextureViewLinear.GetInitReference()));
		}
	}

	TRefCountPtr<ID3D11RenderTargetView> TextureRTV;
	TRefCountPtr<ID3D11DepthStencilView> TextureDSV;
	if(Flags & TexCreate_ResolveTargetable)
	{
		if((TextureDesc.BindFlags & D3D11_BIND_RENDER_TARGET) && !CubeTexture)
		{
			// Create a render-target-view for the texture if it is resolve targetable.
			D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
			RTVDesc.Format = FindDXGIFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat,Flags&TexCreate_SRGB);
			RTVDesc.ViewDimension = bTextureArray ? D3D11_RTV_DIMENSION_TEXTURE2DARRAY : D3D11_RTV_DIMENSION_TEXTURE2D;
			if (RTVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
			{
				RTVDesc.Texture2DArray.FirstArraySlice = 0;
				RTVDesc.Texture2DArray.ArraySize = TextureDesc.ArraySize;
				RTVDesc.Texture2DArray.MipSlice = 0;
			}
			else if (RTVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D)
			{
				RTVDesc.Texture2D.MipSlice = 0;
			}
			
			VERIFYD3D11RESULT(Direct3DDevice->CreateRenderTargetView(TextureResource,&RTVDesc,TextureRTV.GetInitReference()));
		}
		else if((TextureDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL) && !CubeTexture)
		{
			// Create a depth-stencil-view for the texture if it is resolve targetable.
			D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc;
			DSVDesc.Flags = 0;
				
			if(TextureDesc.Format == DXGI_FORMAT_R24G8_TYPELESS)
			{
				// Use the typed depth format corresponding to DXGI_FORMAT_R24G8_TYPELESS
				DSVDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			}
			else
			{
				DSVDesc.Format = TextureDesc.Format;
			}

			DSVDesc.ViewDimension = bTextureArray ? D3D11_DSV_DIMENSION_TEXTURE2DARRAY : D3D11_DSV_DIMENSION_TEXTURE2D;
			if (DSVDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY)
			{
				DSVDesc.Texture2DArray.FirstArraySlice = 0;
				DSVDesc.Texture2DArray.ArraySize = TextureDesc.ArraySize;
				DSVDesc.Texture2DArray.MipSlice = 0;
			}
			else if (DSVDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D)
			{
				DSVDesc.Texture2D.MipSlice = 0;
			}
			
			VERIFYD3D11RESULT(Direct3DDevice->CreateDepthStencilView(TextureResource,&DSVDesc,TextureDSV.GetInitReference()));
		}
	}

	FD3D11Texture2D* Texture2D = new FD3D11Texture2D(this,TextureResource,TextureView,TextureViewLinear,TextureRTV,TextureDSV,SizeX,SizeY,SizeZ,NumMips,(EPixelFormat)Format,CubeTexture,Flags);
	if ( CubeTexture == FALSE && (Flags & (TexCreate_ResolveTargetable | TexCreate_DepthStencil)) == 0 )
	{
		D3D11TextureAllocated( *Texture2D );
	}
	return Texture2D;
}

FD3D11Texture3D* FD3D11DynamicRHI::CreateD3D11Texture3D(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,DWORD Flags,const BYTE* Data)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11CreateTextureTime);

	DXGI_FORMAT PlatformFormat = FindDXGIFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat,Flags&TexCreate_SRGB);
	if( GIsEditor )
	{
		// In the editor we'll use a typeless format so that we can dynamically switch between SRGB and linear
		// texture sampling using separate shader resource views.  This is used for mobile rendering emulation.
		PlatformFormat = FindTypelessDXGIFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat );
	}

	D3D11_TEXTURE3D_DESC TextureDesc;
	TextureDesc.Width = SizeX;
	TextureDesc.Height = SizeY;
	TextureDesc.Depth = SizeZ;
	TextureDesc.MipLevels = NumMips;
	TextureDesc.Format = PlatformFormat;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;

	TArray<D3D11_SUBRESOURCE_DATA> SubResourceData;
	SubResourceData.AddZeroed(SizeZ);
	for (UINT i = 0; i < SizeZ; i++)
	{
		SubResourceData(i).pSysMem = &Data[i * SizeY * SizeX * GPixelFormats[Format].BlockBytes];
		SubResourceData(i).SysMemPitch = SizeX * GPixelFormats[Format].BlockBytes;
		SubResourceData(i).SysMemSlicePitch = SizeY * GPixelFormats[Format].BlockBytes * SizeX;
	}

	TRefCountPtr<ID3D11Texture3D> TextureResource;
	VERIFYD3D11CREATETEXTURERESULT(
		Direct3DDevice->CreateTexture3D(
			&TextureDesc,
			(const D3D11_SUBRESOURCE_DATA*)SubResourceData.GetData(),
			TextureResource.GetInitReference()),
		SizeX,
		SizeY,
		PlatformFormat,
		NumMips,
		TextureDesc.BindFlags
		);

	// Create a shader resource view for the texture.
	TRefCountPtr<ID3D11ShaderResourceView> TextureView;
	TRefCountPtr<ID3D11ShaderResourceView> TextureViewLinear;
	if ((TextureDesc.BindFlags & D3D11_BIND_RENDER_TARGET && !(Flags&TargetSurfCreate_Dedicated)) ||
		TextureDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		SRVDesc.Format = FindDXGIFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat,Flags&TexCreate_SRGB);
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		SRVDesc.Texture3D.MipLevels = NumMips;
		SRVDesc.Texture3D.MostDetailedMip = 0;
		VERIFYD3D11RESULT(Direct3DDevice->CreateShaderResourceView(TextureResource,&SRVDesc,TextureView.GetInitReference()));

		if( GIsEditor && (Flags&TexCreate_SRGB) )
		{
			SRVDesc.Format = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;
			VERIFYD3D11RESULT(Direct3DDevice->CreateShaderResourceView(TextureResource,&SRVDesc,TextureViewLinear.GetInitReference()));
		}
	}

	FD3D11Texture3D* Texture3D = new FD3D11Texture3D(this,TextureResource,TextureView,TextureViewLinear,SizeX,SizeY,SizeZ,NumMips,(EPixelFormat)Format);
	return Texture3D;
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
FTexture2DRHIRef FD3D11DynamicRHI::CreateTexture2D(UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips,DWORD Flags,FResourceBulkDataInterface* BulkData)
{
	return CreateD3D11Texture2D(SizeX,SizeY,1,FALSE,FALSE,Format,NumMips,Flags);
}

FTexture2DArrayRHIRef FD3D11DynamicRHI::CreateTexture2DArray(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,DWORD Flags,FResourceBulkDataInterface* BulkData)
{
	check(SizeZ >= 1);
	return (FD3D11Texture2DArray*)CreateD3D11Texture2D(SizeX,SizeY,SizeZ,TRUE,FALSE,Format,NumMips,Flags);
}

FTexture3DRHIRef FD3D11DynamicRHI::CreateTexture3D(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,DWORD Flags,const BYTE* Data)
{
	check(SizeZ >= 1);
	return CreateD3D11Texture3D(SizeX,SizeY,SizeZ,Format,NumMips,Flags,Data);
}

/** Generates mip maps for the surface. */
void FD3D11DynamicRHI::GenerateMips(FSurfaceRHIParamRef SourceSurfaceRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(Surface, SourceSurface);
	// Surface must have been created with D3D11_BIND_RENDER_TARGET for GenerateMips to work
	checkSlow(SourceSurface->ShaderResourceView && SourceSurface->RenderTargetView);
	Direct3DDeviceIMContext->GenerateMips(SourceSurface->ShaderResourceView);
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
FTexture2DRHIRef FD3D11DynamicRHI::ReallocateTexture2D( FTexture2DRHIParamRef Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY )
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
UINT FD3D11DynamicRHI::GetTextureSize(FTexture2DRHIParamRef TextureRHI)
{
	if(!TextureRHI)
	{
		return 0;
	}

	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,Texture);
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
FTexture2DRHIRef FD3D11DynamicRHI::AsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY, FThreadSafeCounter* RequestStatus)
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
ETextureReallocationStatus FD3D11DynamicRHI::FinalizeAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, UBOOL bBlockUntilCompleted )
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
ETextureReallocationStatus FD3D11DynamicRHI::CancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, UBOOL bBlockUntilCompleted )
{
	// not supported
	return TexRealloc_Failed;
}

template<ERHIResourceTypes ResourceTypeEnum>
void* TD3D11Texture2D<ResourceTypeEnum>::Lock(UINT MipIndex,UINT ArrayIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11LockTextureTime);

	// Calculate the subresource index corresponding to the specified mip-map.
	const UINT Subresource = D3D11CalcSubresource(MipIndex,ArrayIndex,NumMips);

	// Calculate the dimensions of the mip-map.
	const UINT BlockSizeX = GPixelFormats[Format].BlockSizeX;
	const UINT BlockSizeY = GPixelFormats[Format].BlockSizeY;
	const UINT BlockBytes = GPixelFormats[Format].BlockBytes;
	const UINT MipSizeX = Max(SizeX >> MipIndex,BlockSizeX);
	const UINT MipSizeY = Max(SizeY >> MipIndex,BlockSizeY);
	const UINT NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	const UINT NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;
	const UINT MipBytes = NumBlocksX * NumBlocksY * BlockBytes;
	
	FD3D11LockedData LockedData;
	if( bIsDataBeingWrittenTo && ( (Flags & TexCreate_Dynamic ) == 0 ))
	{
		// If we're writing to the texture, allocate a system memory buffer to receive the new contents.
		LockedData.Data = new BYTE[ MipBytes ];
		LockedData.Pitch = DestStride = NumBlocksX * BlockBytes;
	}
	else
	{
		// If we're reading from the texture, or writing to a dynamic texture, we create a staging resource, 
		// copy the texture contents to it, and map it.

		// Create the staging texture.
		D3D11_TEXTURE2D_DESC StagingTextureDesc;
		Resource->GetDesc(&StagingTextureDesc);
		StagingTextureDesc.Width = MipSizeX;
		StagingTextureDesc.Height = MipSizeY;
		StagingTextureDesc.MipLevels = 1;
		StagingTextureDesc.ArraySize = 1;
		StagingTextureDesc.Usage = D3D11_USAGE_STAGING;
		StagingTextureDesc.BindFlags = 0;
		StagingTextureDesc.CPUAccessFlags = bIsDataBeingWrittenTo ? D3D11_CPU_ACCESS_WRITE : D3D11_CPU_ACCESS_READ;
		TRefCountPtr<ID3D11Texture2D> StagingTexture;
		VERIFYD3D11CREATETEXTURERESULT(
			D3DRHI->GetDevice()->CreateTexture2D(&StagingTextureDesc,NULL,StagingTexture.GetInitReference()),
			SizeX,
			SizeY,
			StagingTextureDesc.Format,
			1,
			0
			);
		LockedData.StagingResource = StagingTexture;

		// Copy the mip-map data from the real resource into the staging resource
		D3DRHI->GetDeviceContext()->CopySubresourceRegion(StagingTexture,0,0,0,0,Resource,Subresource,NULL);

		// Map the staging resource, and return the mapped address.
		D3D11_MAPPED_SUBRESOURCE MappedTexture;
		VERIFYD3D11RESULT(D3DRHI->GetDeviceContext()->Map(StagingTexture,0,bIsDataBeingWrittenTo ? D3D11_MAP_WRITE : D3D11_MAP_READ,0,&MappedTexture));
		LockedData.Data = (BYTE*)MappedTexture.pData;
		LockedData.Pitch = DestStride = MappedTexture.RowPitch;
	}

	// Add the lock to the outstanding lock list.
	D3DRHI->OutstandingLocks.Set(FD3D11LockedKey(Resource,Subresource),LockedData);

	return (void*)LockedData.Data;
}

template<ERHIResourceTypes ResourceTypeEnum>
void TD3D11Texture2D<ResourceTypeEnum>::Unlock(UINT MipIndex,UINT ArrayIndex, UBOOL bDiscardUpdate)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11UnlockTextureTime);

	// Calculate the subresource index corresponding to the specified mip-map.
	const UINT Subresource = D3D11CalcSubresource(MipIndex,ArrayIndex,NumMips);

	// Find the object that is tracking this lock
	const FD3D11LockedKey LockedKey(Resource,Subresource);
	const FD3D11LockedData* LockedData = D3DRHI->OutstandingLocks.Find(LockedKey);
	check(LockedData);

	BOOL ShouldUpdate = !LockedData->StagingResource || Flags & TexCreate_Dynamic;

	if( ShouldUpdate )
	{
        if (!bDiscardUpdate)
        {
		    // PF_A8R8G8B8 memory layout is differently in D3D11 from D3D9, swizzle at the latest moment so that we hide that from the engine
		    if(Format == PF_A8R8G8B8)
		    {
			    const UINT MipSizeX = Max(SizeX >> MipIndex, (UINT)1);
			    const UINT MipSizeY = Max(SizeY >> MipIndex, (UINT)1);

			    for(UINT y = 0; y < MipSizeY; ++y)
			    {
				    UINT* p = (UINT*)&LockedData->Data[y * LockedData->Pitch];

				    for(UINT x = 0; x < MipSizeX; ++x)
				    {
					    UINT argb = *p;
					    UINT a = argb >> 24;
					    UINT r = (argb >> 16) & 0xff;
					    UINT g = (argb >> 8) & 0xff;
					    UINT b = argb & 0xff;

					    *p++ = (a << 24) | (b << 16) | (g << 8) | r;
				    }
			    }
		    }

		    // If we're writing, we need to update the subresource
		    D3DRHI->GetDeviceContext()->UpdateSubresource(Resource,Subresource,NULL,LockedData->Data,LockedData->Pitch,0);
        }
		if ( !LockedData->StagingResource )
		{
			delete[] LockedData->Data;
		}
	}

	// Remove the lock from the outstanding lock list.
	D3DRHI->OutstandingLocks.Remove(LockedKey);
}

void* FD3D11DynamicRHI::LockTexture2D(FTexture2DRHIParamRef TextureRHI,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail)
{
	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,Texture);
	return Texture->Lock(MipIndex,0,bIsDataBeingWrittenTo,DestStride);
}

void FD3D11DynamicRHI::UnlockTexture2D(FTexture2DRHIParamRef TextureRHI,UINT MipIndex,UBOOL bLockWithinMiptail)
{
	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,Texture);
	Texture->Unlock(MipIndex,0,bLockWithinMiptail);
}

void* FD3D11DynamicRHI::LockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,UINT TextureIndex,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail)
{
	DYNAMIC_CAST_D3D11RESOURCE(Texture2DArray,Texture);
	return ((FD3D11Texture2D*)Texture)->Lock(MipIndex,TextureIndex,bIsDataBeingWrittenTo,DestStride);
}

void FD3D11DynamicRHI::UnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,UINT TextureIndex,UINT MipIndex,UBOOL bLockWithinMiptail)
{
	DYNAMIC_CAST_D3D11RESOURCE(Texture2DArray,Texture);
	((FD3D11Texture2D*)Texture)->Unlock(MipIndex,TextureIndex);
}

/**
	* Checks if a texture is still in use by the GPU.
	* @param Texture - the RHI texture resource to check
	* @param MipIndex - Which mipmap we're interested in
	* @return TRUE if the texture is still in use by the GPU, otherwise FALSE
	*/
UBOOL FD3D11DynamicRHI::IsBusyTexture2D(FTexture2DRHIParamRef Texture,UINT MipIndex)
{
	//@TODO: Implement somehow! (Perhaps with D3D11_MAP_FLAG_DO_NOT_WAIT)
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
UBOOL FD3D11DynamicRHI::UpdateTexture2D(FTexture2DRHIParamRef TextureRHI,UINT MipIndex,UINT n,const FUpdateTextureRegion2D* rects,UINT pitch,UINT sbpp,BYTE* psrc)
{
	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,Texture);

	for (UINT i = 0; i < n; i++)
	{
		D3D11_BOX dest = { rects[i].DestX, rects[i].DestY, 0,
			rects[i].DestX + rects[i].Width, rects[i].DestY + rects[i].Height, 1 };

		if (Texture->Format == PF_A8R8G8B8)
		{
			UINT* tmp = new UINT[rects[i].Width * rects[i].Height];
			// Swizzle
			for(INT y = 0; y < rects[i].Height; ++y)
			{
				for(INT x = 0; x < rects[i].Width; ++x)
				{
					UINT* p = (UINT *)(psrc + pitch * (rects[i].SrcY+y) + sbpp * (rects[i].SrcX+x));

					UINT argb = *p;
					UINT a = argb >> 24;
					UINT r = (argb >> 16) & 0xff;
					UINT g = (argb >> 8) & 0xff;
					UINT b = argb & 0xff;

					tmp[x + y*rects[i].Width] = (a << 24) | (b << 16) | (g << 8) | r;
				}
			}
			Direct3DDeviceIMContext->UpdateSubresource(Texture->Resource, MipIndex, &dest,
				tmp, sbpp * rects[i].Width, 0);
			delete [] tmp;
		}
		else
		{
			Direct3DDeviceIMContext->UpdateSubresource(Texture->Resource, MipIndex, &dest,
				psrc + pitch * rects[i].SrcY + sbpp * rects[i].SrcX, pitch, 0);
		}
	}

	return TRUE;
}

INT FD3D11DynamicRHI::GetMipTailIdx(FTexture2DRHIParamRef Texture)
{
	return -1;
}

void FD3D11DynamicRHI::CopyTexture2D(FTexture2DRHIParamRef DestTextureRHI, UINT MipIdx, INT BaseSizeX, INT BaseSizeY, INT Format, const TArray<FCopyTextureRegion2D>& Regions)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11CopyTextureTime);

	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,DestTexture);
	check( DestTexture );

	// scale the base SizeX,SizeY for the current mip level
	const INT MipSizeX = Max(BaseSizeX >> MipIdx,(INT)GPixelFormats[Format].BlockSizeX);
	const INT MipSizeY = Max(BaseSizeY >> MipIdx,(INT)GPixelFormats[Format].BlockSizeY);

	for( INT RegionIdx=0; RegionIdx < Regions.Num(); RegionIdx++ )		
	{
		const FCopyTextureRegion2D& Region = Regions(RegionIdx);
		FTexture2DRHIParamRef SourceTextureRHI = Region.SrcTexture;
		DYNAMIC_CAST_D3D11RESOURCE(Texture2D,SourceTexture);

		// align/truncate the region offset to block size
		const UINT RegionOffsetX = (UINT)(Clamp( Region.OffsetX, 0, MipSizeX - GPixelFormats[Format].BlockSizeX ) / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockSizeX;
		const UINT RegionOffsetY = (UINT)(Clamp( Region.OffsetY, 0, MipSizeY - GPixelFormats[Format].BlockSizeY ) / GPixelFormats[Format].BlockSizeY) * GPixelFormats[Format].BlockSizeY;
		// scale region size to the current mip level. Size is aligned to the block size
		check(Region.SizeX != 0 && Region.SizeY != 0);
		UINT RegionSizeX = (UINT)Clamp( Align( Region.SizeX, GPixelFormats[Format].BlockSizeX), 0, MipSizeX );
		UINT RegionSizeY = (UINT)Clamp( Align( Region.SizeY, GPixelFormats[Format].BlockSizeY), 0, MipSizeY );
		// handle special case for full copy
		if( Region.SizeX == -1 || Region.SizeY == -1 )
		{
			RegionSizeX = MipSizeX;
			RegionSizeY = MipSizeY;
		}

		// Set up a box for the copy region.
		D3D11_BOX CopyBox;
		CopyBox.left = RegionOffsetX;
		CopyBox.top = RegionOffsetY;
		CopyBox.right = RegionOffsetX + RegionSizeX;
		CopyBox.bottom = RegionOffsetY + RegionSizeY;
		CopyBox.front = 0;
		CopyBox.back = 1;

		// Use the GPU to copy the texture region.
		Direct3DDeviceIMContext->CopySubresourceRegion(
			DestTexture->Resource,
			D3D11CalcSubresource(MipIdx,0,1),
			RegionOffsetX,
			RegionOffsetY,
			0,
			SourceTexture->Resource,
			D3D11CalcSubresource(Region.FirstMipIdx + MipIdx,0,1),
			&CopyBox
			);
	}
}

void FD3D11DynamicRHI::CopyMipToMipAsync(FTexture2DRHIParamRef SrcTextureRHI, INT SrcMipIndex, FTexture2DRHIParamRef DestTextureRHI, INT DestMipIndex, INT Size, FThreadSafeCounter& Counter)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11CopyMipToMipAsyncTime);

	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,SrcTexture);
	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,DestTexture);

	// Use the GPU to copy between mip-maps.
	// This is serialized with other D3D commands, so it isn't necessary to increment Counter to signal a pending asynchronous copy.
	Direct3DDeviceIMContext->CopySubresourceRegion(
		DestTexture->Resource,
		D3D11CalcSubresource(DestMipIndex,0,DestTexture->NumMips),
		0,
		0,
		0,
		SrcTexture->Resource,
		D3D11CalcSubresource(SrcMipIndex,0,SrcTexture->NumMips),
		NULL
		);
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
void FD3D11DynamicRHI::SelectiveCopyMipData(FTexture2DRHIParamRef Texture, BYTE *Src, BYTE *Dst, UINT MemSize, UINT MipIdx)
{
	appMemcpy(Dst, Src, MemSize);
}

void FD3D11DynamicRHI::FinalizeAsyncMipCopy(FTexture2DRHIParamRef SrcTextureRHI, INT SrcMipIndex, FTexture2DRHIParamRef DestTextureRHI, INT DestMipIndex)
{
	// We don't need to do any work here, because the asynchronous mip copy is performed by the GPU and serialized with other D3D commands.
}

/*-----------------------------------------------------------------------------
Shared texture support.
-----------------------------------------------------------------------------*/
FSharedMemoryResourceRHIRef FD3D11DynamicRHI::CreateSharedMemory(EGPUMemoryType MemType,SIZE_T Size)
{
	// create the shared memory resource
	FSharedMemoryResourceRHIRef SharedMemory(NULL);
	return SharedMemory;
}
FSharedTexture2DRHIRef FD3D11DynamicRHI::CreateSharedTexture2D(UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips,FSharedMemoryResourceRHIParamRef SharedMemoryRHI,DWORD Flags)
{
	DYNAMIC_CAST_D3D11RESOURCE(SharedMemoryResource,SharedMemory);
	return FSharedTexture2DRHIRef();
}

FSharedTexture2DArrayRHIRef FD3D11DynamicRHI::CreateSharedTexture2DArray(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,FSharedMemoryResourceRHIParamRef SharedMemoryRHI,DWORD Flags)
{
	// not supported on that platform
	check(0);
	return NULL;
}

/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/
FTextureCubeRHIRef FD3D11DynamicRHI::CreateTextureCube( UINT Size, BYTE Format, UINT NumMips, DWORD Flags, FResourceBulkDataInterface* BulkData )
{
	return (FD3D11TextureCube*)CreateD3D11Texture2D(Size,Size,6,FALSE,TRUE,Format,NumMips,Flags);
}
void* FD3D11DynamicRHI::LockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,UINT FaceIndex,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail)
{
	DYNAMIC_CAST_D3D11RESOURCE(TextureCube,TextureCube);
	return TextureCube->Lock(MipIndex,FaceIndex,bIsDataBeingWrittenTo,DestStride);
}
void FD3D11DynamicRHI::UnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,UINT FaceIndex,UINT MipIndex,UBOOL bLockWithinMiptail)
{
	DYNAMIC_CAST_D3D11RESOURCE(TextureCube,TextureCube);
	TextureCube->Unlock(MipIndex,FaceIndex);
}

FTexture2DRHIRef FD3D11DynamicRHI::CreateStereoFixTexture()
{
	using nv::stereo::UE3StereoD3D11;

	UINT SizeX = UE3StereoD3D11::Parms::StereoTexWidth;
	UINT SizeY = UE3StereoD3D11::Parms::StereoTexHeight;

	D3D11_TEXTURE2D_DESC TextureDesc;
	TextureDesc.Width = SizeX;
	TextureDesc.Height = SizeY;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = UE3StereoD3D11::Parms::StereoTexFormat;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.SampleDesc.Quality = 0;
	TextureDesc.Usage = D3D11_USAGE_DYNAMIC;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	TextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	TextureDesc.MiscFlags = 0;

	TRefCountPtr<ID3D11Texture2D> TextureResource;
	VERIFYD3D11CREATETEXTURERESULT(
		Direct3DDevice->CreateTexture2D(
			&TextureDesc,
			NULL,
			TextureResource.GetInitReference()),
		SizeX,
		SizeY,
		UE3StereoD3D11::Parms::StereoTexFormat,
		1,
		TextureDesc.BindFlags
		);

	// Create a shader resource view for the texture.
	TRefCountPtr<ID3D11ShaderResourceView> TextureView;
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	SRVDesc.Format = TextureDesc.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2DArray.MostDetailedMip = 0;
	SRVDesc.Texture2DArray.MipLevels = 1;
	SRVDesc.Texture2DArray.FirstArraySlice = 0;
	SRVDesc.Texture2DArray.ArraySize = TextureDesc.ArraySize;
	VERIFYD3D11RESULT(Direct3DDevice->CreateShaderResourceView(TextureResource,&SRVDesc,TextureView.GetInitReference()));

	TRefCountPtr<ID3D11ShaderResourceView> TextureViewLinear;	// NOTE: Intentionally left null (not needed for stereo textures.)
	return new FD3D11Texture2D(this,TextureResource,TextureView,TextureViewLinear,NULL,NULL,SizeX,SizeY,1,1,PF_G32R32F,false,0);
}
