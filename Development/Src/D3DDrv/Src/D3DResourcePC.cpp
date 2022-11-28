 /*=============================================================================
	D3DResourcePC.cpp: Unreal Direct3D resource implementation for PC.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel

	WARNING: make sure to update Xenon version when touching this file!!
=============================================================================*/

#include "D3DDrv.h"

/**
 * Not used on PC.	
 */
FD3DTexture::~FD3DTexture() 
{}

/**
 * Not used on PC.
 */
void FD3DTexture::UploadToVRAM()
{}

/**
 * Not used on PC.	
 */
void FD3DTexture::Cleanup() 
{}

/**
 * Creates a new mipchain after releasing the existing one and uploads the
 * texture data.
 *
 * @param	Source	texture used
 */
void FD3DTexture2D::CreateMipChain( FTextureBase* SourceBase )
{
	FTexture2D*	Source = CastResourceChecked<FTexture2D>( SourceBase );

	Texture = NULL;

	HRESULT Result = GDirect3DDevice->CreateTexture(
						UsedSizeX,
						UsedSizeY,
						UsedNumMips,
						0,
						(D3DFORMAT) GPixelFormats[UsedFormat].PlatformFormat,
						D3DPOOL_MANAGED,
						(IDirect3DTexture9**)&Texture.Handle,
						NULL
						);

	if( FAILED( Result ) )
		appErrorf(TEXT("CreateTexture failed: %s"),*D3DError(Result));
	
	for( UINT MipIndex=0; MipIndex<UsedNumMips; MipIndex++ )
		UploadMipLevel( Source, MipIndex );
}

/**
 * Uploads a particular miplevel of the passed in texture.
 *
 * @param	Source		source texture used
 * @param	MipLevel	index of miplevel to upload
 */
void FD3DTexture2D::UploadMipLevel( FTextureBase* SourceBase, UINT MipLevel )
{
	D3DLOCKED_RECT		LockedRect;
	HRESULT				Result;
	IDirect3DTexture9*	Texture2D	= (IDirect3DTexture9*) Texture.Handle;
	FTexture2D*			Source		= CastResourceChecked<FTexture2D>( SourceBase );

	if( FAILED( Result = Texture2D->LockRect( MipLevel, &LockedRect, NULL, 0 ) ) )
		appErrorf(TEXT("LockRect failed: %s"),*D3DError(Result));

	Source->GetData( LockedRect.pBits, MipLevel + UsedFirstMip, LockedRect.Pitch );

	Texture2D->UnlockRect( MipLevel );
}

/**
 * Creates an intermediate texture with a specified amount of miplevels and copies
 * over as many miplevels from the existing texture as possible.
 *
 * @param MipLevels Number of miplevels in newly created intermediate texture
 */
void FD3DTexture2D::CopyToIntermediate( UINT MipLevels )
{
	check( Texture.Handle != NULL );
	check( IntermediateTexture.Handle == NULL );
	check( MipLevels <= SourceNumMips );

	UINT OldNumMips	= UsedNumMips;

	UsedFirstMip	= SourceNumMips - MipLevels;
	UsedNumMips		= MipLevels;
	UsedSizeX		= Max<UINT>( GPixelFormats[UsedFormat].BlockSizeX, SourceSizeX >> UsedFirstMip );
	UsedSizeY		= Max<UINT>( GPixelFormats[UsedFormat].BlockSizeY, SourceSizeY >> UsedFirstMip );
	//UsedSizeZ		= Max<UINT>( GPixelFormats[UsedFormat].BlockSizeZ, SourceSizeZ >> UsedFirstMip );

	HRESULT Result = GDirect3DDevice->CreateTexture(
						UsedSizeX,
						UsedSizeY,
						UsedNumMips,
						0,
						(D3DFORMAT) GPixelFormats[UsedFormat].PlatformFormat,
						D3DPOOL_MANAGED,
						(IDirect3DTexture9**)&IntermediateTexture.Handle,
						NULL
						);

	if( FAILED( Result ) )
		appErrorf(TEXT("CreateTexture failed: %s"),*D3DError(Result));

	D3DLOCKED_RECT		SrcRect;
	D3DLOCKED_RECT		DstRect;
	IDirect3DTexture9*	SrcTexture		= (IDirect3DTexture9*) Texture.Handle;
	IDirect3DTexture9*	DstTexture		= (IDirect3DTexture9*) IntermediateTexture.Handle;
	UINT				SrcMipOffset	= Max<INT>( 0, OldNumMips - UsedNumMips );
	UINT				DstMipOffset	= Max<INT>( 0, UsedNumMips - OldNumMips );

	for( UINT MipLevel=0; MipLevel<Min(OldNumMips, UsedNumMips); MipLevel++ )
	{
		verify( SUCCEEDED( SrcTexture->LockRect( MipLevel + SrcMipOffset, &SrcRect, NULL, D3DLOCK_READONLY ) ) );
		verify( SUCCEEDED( DstTexture->LockRect( MipLevel + DstMipOffset, &DstRect, NULL, D3DLOCK_DISCARD ) ) );

		check(SrcRect.Pitch == DstRect.Pitch);
		appMemcpy( DstRect.pBits, SrcRect.pBits, ((UsedSizeY >> (MipLevel+DstMipOffset)) / GPixelFormats[UsedFormat].BlockSizeY) * SrcRect.Pitch );

		SrcTexture->UnlockRect( MipLevel + SrcMipOffset );
		DstTexture->UnlockRect( MipLevel + DstMipOffset);
	}
}

/**
 * Request an increase or decrease in the amount of miplevels a texture uses.
 *
 * @param	Texture				texture to adjust
 * @param	RequestedMips		miplevels the texture should have
 * @param	TextureMipRequest	NULL if a decrease is requested, otherwise pointer to information 
 *								about request that needs to be filled in.
 */
void FD3DTexture2D::RequestMips( UINT NewNumMips, FTextureMipRequest* TextureMipRequest )
{
	if( TextureMipRequest )
	{
		UINT				OldNumMips	= UsedNumMips;
		CopyToIntermediate( NewNumMips );

		IDirect3DTexture9*	DstTexture	= (IDirect3DTexture9*) IntermediateTexture.Handle;	
		UINT				MipOffset	= SourceNumMips - TextureMipRequest->RequestedMips;

		for( UINT MipIndex=SourceNumMips-NewNumMips; MipIndex<SourceNumMips-OldNumMips; MipIndex++ )
		{
			D3DLOCKED_RECT DstRect;
			verify( SUCCEEDED( DstTexture->LockRect( MipIndex - MipOffset, &DstRect, NULL, D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK ) ) );

			TextureMipRequest->Mips[MipIndex].Size = GetMipSize( MipIndex );
			TextureMipRequest->Mips[MipIndex].Data = DstRect.pBits;

			//@warning: Texture is left locked till call to FinalizeMipRequest!!!
		}
	}
	else
	{
		// Removing miplevels can be done immediately.
		CopyToIntermediate( NewNumMips );
		Switcheroo();
	}
}

/**
 * Finalizes a mip request. This gets called after all requested data has finished loading.	
 *
 * @param	TextureMipRequest	Mip request that is being finalized.
 */
void FD3DTexture2D::FinalizeMipRequest( FTextureMipRequest* TextureMipRequest )
{
	check( IntermediateTexture.Handle != NULL );
	
	IDirect3DTexture9*	DstTexture	= (IDirect3DTexture9*) IntermediateTexture.Handle;
	UINT				MipOffset	= SourceNumMips - TextureMipRequest->RequestedMips;

 	for( UINT MipLevel=0; MipLevel<ARRAY_COUNT(TextureMipRequest->Mips); MipLevel++ )
	{
		if( TextureMipRequest->Mips[MipLevel].Data )
		{
			check(MipLevel - MipOffset < UsedNumMips);
			DstTexture->UnlockRect( MipLevel - MipOffset );			
			TextureMipRequest->Mips[MipLevel].Data = NULL;
			TextureMipRequest->Mips[MipLevel].Size = 0;
		}
	}

	Switcheroo();
}

/**
 * Performs a switcheroo of intermediate and used texture, releasing the previously used one.	
 */
void FD3DTexture2D::Switcheroo()
{
	check( IntermediateTexture.Handle != NULL );
	Exchange( Texture, IntermediateTexture );
	IntermediateTexture = NULL;
}

/**
 * Creates a new mipchain after releasing the existing one and uploads the
 * texture data.
 *
 * @param	Source	texture used
 */
void FD3DTexture3D::CreateMipChain( FTextureBase* SourceBase )
{
	FTexture3D*	Source = CastResourceChecked<FTexture3D>( SourceBase );
	
	Texture = NULL;

	HRESULT Result = GDirect3DDevice->CreateVolumeTexture(
						UsedSizeX,
						UsedSizeY,
						UsedSizeZ,
						UsedNumMips,
						0,
						(D3DFORMAT) GPixelFormats[UsedFormat].PlatformFormat,
						D3DPOOL_MANAGED,
						(IDirect3DVolumeTexture9**)&Texture.Handle,
						NULL
						);

	if( FAILED( Result ) )
		appErrorf(TEXT("CreateTexture failed: %s"),*D3DError(Result));
	
	for( UINT MipIndex=0; MipIndex<UsedNumMips; MipIndex++ )
		UploadMipLevel( Source, MipIndex );
}

/**
 * Uploads a particular miplevel of the passed in texture.
 *
 * @param	Source		source texture used
 * @param	MipLevel	index of miplevel to upload
 */
void FD3DTexture3D::UploadMipLevel( FTextureBase* SourceBase, UINT MipLevel )
{
	D3DLOCKED_BOX				LockedBox;
	HRESULT						Result;
	IDirect3DVolumeTexture9*	Texture3D	= (IDirect3DVolumeTexture9*) Texture.Handle;
	FTexture3D*					Source		= CastResourceChecked<FTexture3D>( SourceBase );


	if( FAILED( Result = Texture3D->LockBox( MipLevel, &LockedBox, NULL, 0 ) ) )
		appErrorf(TEXT("LockRect failed: %s"),*D3DError(Result));

	Source->GetData( LockedBox.pBits, MipLevel + UsedFirstMip, LockedBox.RowPitch, LockedBox.SlicePitch );

	Texture3D->UnlockBox( MipLevel );
}

/**
 * Creates a new mipchain after releasing the existing one and uploads the
 * texture data.
 *
 * @param	Source	texture used
 */
void FD3DTextureCube::CreateMipChain( FTextureBase* SourceBase )
{
	FTextureCube* Source = CastResourceChecked<FTextureCube>( SourceBase );
	
	Texture = NULL;

	HRESULT Result = GDirect3DDevice->CreateCubeTexture(
						UsedSizeX,
						UsedNumMips,
						0,
						(D3DFORMAT) GPixelFormats[UsedFormat].PlatformFormat,
						D3DPOOL_MANAGED,
						(IDirect3DCubeTexture9**)&Texture.Handle,
						NULL
						);

	if( FAILED( Result ) )
		appErrorf(TEXT("CreateTexture failed: %s"),*D3DError(Result));
	
	for( UINT MipIndex=0; MipIndex<UsedNumMips; MipIndex++ )
		UploadMipLevel( Source, MipIndex );
}

/**
 * Uploads a particular miplevel of the passed in texture.
 *
 * @param	Source		source texture used
 * @param	MipLevel	index of miplevel to upload
 */
void FD3DTextureCube::UploadMipLevel( FTextureBase* SourceBase, UINT MipLevel )
{
	D3DLOCKED_RECT			LockedRect;
	HRESULT					Result;
	IDirect3DCubeTexture9*	TextureCube	= (IDirect3DCubeTexture9*) Texture.Handle;
	FTextureCube*			Source		= CastResourceChecked<FTextureCube>( SourceBase );

	for(UINT FaceIndex = 0;FaceIndex < 6;FaceIndex++)
	{	
		if( FAILED( Result = TextureCube->LockRect( (D3DCUBEMAP_FACES) FaceIndex, MipLevel, &LockedRect, NULL, 0 ) ) )
			appErrorf(TEXT("LockRect failed: %s"),*D3DError(Result));

		Source->GetData( LockedRect.pBits, FaceIndex, MipLevel + UsedFirstMip, LockedRect.Pitch );

		TextureCube->UnlockRect( (D3DCUBEMAP_FACES) FaceIndex, MipLevel );
	}
}

//
//	FD3DIndexBuffer implementation
//

/**
 * Not used on PC.	
 */
void FD3DIndexBuffer::Cleanup()
{}

/**
 * Not used on PC.	
 */
FD3DIndexBuffer::~FD3DIndexBuffer()
{}

void FD3DIndexBuffer::Cache(FIndexBuffer* SourceIndexBuffer)
{
	// Create the new cached index buffer.

	HRESULT Result = GDirect3DDevice->CreateIndexBuffer(SourceIndexBuffer->Size,D3DUSAGE_WRITEONLY,SourceIndexBuffer->Stride < 4 ? D3DFMT_INDEX16 : D3DFMT_INDEX32,D3DPOOL_MANAGED,&IndexBuffer.Handle,NULL);
	if(FAILED(Result))
		appErrorf(TEXT("CreateIndexBuffer failed: %s"),*D3DError(Result));

	// Copy the contents of the index buffer into the cached index buffer.

	void*	IndexBufferContents = NULL;
	Result = IndexBuffer->Lock(0,SourceIndexBuffer->Size,&IndexBufferContents,0);
	if(FAILED(Result))
		appErrorf(TEXT("Lock failed: %s"),*D3DError(Result));

	SourceIndexBuffer->GetData(IndexBufferContents);

	IndexBuffer->Unlock();

	// Keep track of size for stats.
	Size	= SourceIndexBuffer->Size;
	Dynamic	= SourceIndexBuffer->Dynamic;
}

/**
 * Not used on PC.	
 */
void FD3DIndexBuffer::UploadToVRAM()
{}

//
//	FD3DDynamicIndexBuffer implementation.
//

/**
 * Not used on PC.	
 */
void FD3DDynamicIndexBuffer::Cleanup()
{}

/**
 * Not used on PC.	
 */
FD3DDynamicIndexBuffer::~FD3DDynamicIndexBuffer()
{}

FD3DDynamicIndexBuffer::FD3DDynamicIndexBuffer(FD3DResourceSet* InSet,UINT InSize,UINT InStride):
	FD3DResource(InSet),
	Stride(InStride),
	Offset(0)
{
	Size = InSize;
	D3DFORMAT Format = D3DFMT_UNKNOWN;
	if(Stride == 2)
		Format = D3DFMT_INDEX16;
	else if(Stride == 4)
		Format = D3DFMT_INDEX32;
	else
		appErrorf(TEXT("Invalid index buffer stride: %u"),Stride);

	HRESULT	Result = GDirect3DDevice->CreateIndexBuffer(Size,D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,Format,D3DPOOL_DEFAULT,&IndexBuffer.Handle,NULL);
	if(FAILED(Result))
		appErrorf(TEXT("Failed to create dynamic index buffer(%u,%u): %s"),Stride,Size,*D3DError(Result));
}

UINT FD3DDynamicIndexBuffer::Set(FIndexBuffer* DynamicIndices)
{
	DWORD	Flags = D3DLOCK_NOOVERWRITE;

	if(Offset + DynamicIndices->Size > Size)
	{
		Offset = 0;
		Flags = D3DLOCK_DISCARD;
	}

	void*	LockedData = NULL;
	HRESULT	Result = IndexBuffer->Lock(Offset,DynamicIndices->Size,&LockedData,Flags);
	if(FAILED(Result))
		appErrorf(TEXT("Failed to lock dynamic index buffer(%u,%u): %s"),DynamicIndices->Size,Offset,*D3DError(Result));

	DynamicIndices->GetData(LockedData);

	Result = IndexBuffer->Unlock();
	if(FAILED(Result))
		appErrorf(TEXT("Failed to unlock dynamic index buffer: %s"),*D3DError(Result));

	Offset += DynamicIndices->Size;

	return (Offset - DynamicIndices->Size) / DynamicIndices->Stride;
}

/**
 * Not used on PC.	
 */
void FD3DDynamicIndexBuffer::UploadToVRAM()
{}

//
//	FD3DVertexBuffer implementation.
//

/**
 * Not used on PC.	
 */
void FD3DVertexBuffer::Cleanup()
{}

/**
 * Not used on PC.	
 */
FD3DVertexBuffer::~FD3DVertexBuffer()
{}

void FD3DVertexBuffer::UpdateBuffer(FVertexBuffer* SourceVertexBuffer)
{
	HRESULT Result;

	check(SourceVertexBuffer->Size);

	if(!VertexBuffer || Size != SourceVertexBuffer->Size || Dynamic != SourceVertexBuffer->Dynamic )
	{
		VertexBuffer = NULL;

		// Create the new cached vertex buffer.

		Result = GDirect3DDevice->CreateVertexBuffer(
			SourceVertexBuffer->Size,
			(SourceVertexBuffer->Dynamic ? D3DUSAGE_DYNAMIC : 0) | D3DUSAGE_WRITEONLY,
			0,
			SourceVertexBuffer->Dynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED,
			&VertexBuffer.Handle,
			NULL
			);
		if(FAILED(Result))
			appErrorf(TEXT("CreateVertexBuffer failed: %s"),*D3DError(Result));

		Size = SourceVertexBuffer->Size;
	}

	// Copy the contents of the vertex buffer into the cached vertex buffer.

	void*	VertexBufferContents = NULL;
	Result = VertexBuffer->Lock(0,SourceVertexBuffer->Size,&VertexBufferContents,SourceVertexBuffer->Dynamic ? D3DLOCK_DISCARD : 0);
	if(FAILED(Result))
		appErrorf(TEXT("Lock failed: %s"),*D3DError(Result));

	SourceVertexBuffer->GetData(VertexBufferContents);

	VertexBuffer->Unlock();

	Stride	= SourceVertexBuffer->Stride;
	Dynamic	= SourceVertexBuffer->Dynamic;
	Valid = 1;
}

void FD3DVertexBuffer::UploadToVRAM(){}

// 
//	FD3DRenderTarget implementation.
//

FD3DRenderTarget::FD3DRenderTarget( UINT Width, UINT Height, D3DFORMAT Format, const TCHAR* Usage )
{
	HRESULT Result;
	if( FAILED( Result = GDirect3DDevice->CreateTexture( Width, Height, 1, D3DUSAGE_RENDERTARGET, Format, D3DPOOL_DEFAULT, &RenderTexture.Handle, NULL ) ) )
		appErrorf( TEXT("Failed creating %ix%i %i render target: %s"), Width, Height, Format, *D3DError(Result) );

	if( FAILED( Result = RenderTexture->GetSurfaceLevel( 0, &RenderSurface.Handle ) ) )
		appErrorf( TEXT("Failed obtaining surface level from %ix%i %i render target: %s"), Width, Height, Format, *D3DError(Result) );
}

FD3DRenderTarget::~FD3DRenderTarget()
{
	RenderSurface = NULL;
	RenderTexture = NULL;
}

const TD3DRef<IDirect3DTexture9>& FD3DRenderTarget::GetTexture() const
{
	return RenderTexture;
}

const TD3DRef<IDirect3DSurface9>& FD3DRenderTarget::GetSurface() const
{
	return RenderSurface;
}
	
void FD3DRenderTarget::Resolve()
{
}

/* ============================================================================
 * FD3DDepthStencilSurface::FD3DDepthStencilSurface
 * 
 * Constructs a depth stencil surface.
 *
 * ============================================================================
 */
FD3DDepthStencilSurface::FD3DDepthStencilSurface( UINT Width, UINT Height, D3DFORMAT Format, const TCHAR* Usage )
{
	HRESULT Result;
	if( FAILED( Result = GDirect3DDevice->CreateDepthStencilSurface( Width, Height, Format, D3DMULTISAMPLE_NONE, 0, FALSE, &DepthSurface.Handle, NULL ) ) )
		appErrorf(TEXT("Creation of depth stencil surface failed: %s"),*D3DError(Result));
}

/* ============================================================================
 * FD3DDepthStencilSurface::FD3DDepthStencilSurface
 * 
 * Constructs a depth stencil surface based on an existing D3D9 one.
 *
 * ============================================================================
 */
FD3DDepthStencilSurface::FD3DDepthStencilSurface( TD3DRef<IDirect3DSurface9>& InDepthSurface )
:	DepthSurface( InDepthSurface )
{}

/* ============================================================================
 * FD3DDepthStencilSurface::~FD3DDepthStencilSurface
 * 
 * Destructor, setting DepthSurface to NULL decreases the reference count.
 *
 * ============================================================================
 */
FD3DDepthStencilSurface::~FD3DDepthStencilSurface()
{
	DepthSurface = NULL;
}

/* ============================================================================
 * FD3DDepthStencilSurface::GetSurface
 * 
 * Returns a const reference to the D3D9 surface TD3DRef for usage with e.g.
 * SetDepthStencilSurface.
 *
 * ============================================================================
 */
const TD3DRef<IDirect3DSurface9>& FD3DDepthStencilSurface::GetSurface() const
{
	return DepthSurface;
}