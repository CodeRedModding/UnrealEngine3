/*=============================================================================
	D3DResource.cpp: Unreal Direct3D resource implementation.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "D3DDrv.h"

//
//	UD3DRenderDevice::FreeResource
//

void UD3DRenderDevice::FreeResource(FResource* Resource)
{
	for(UINT SetIndex = 0;SetIndex < (UINT)ResourceSets.Num();SetIndex++)
		ResourceSets(SetIndex)->FreeResource(Resource);
}

//
//	UD3DRenderDevice::UpdateResource
//

void UD3DRenderDevice::UpdateResource(FResource* Resource)
{
	FD3DResource* D3DResource = Resources.ResourceMap.FindRef(Resource->ResourceIndex);
	
	if( Resource->Type()->IsA(FTexture2D::StaticType) )
	{	
		FTexture2D*	Texture2D = (FTexture2D*) Resource;
		FD3DTexture2D* D3DTexture2D = (FD3DTexture2D*) D3DResource;
		if(D3DTexture2D)
			D3DTexture2D->UpdateTexture(Texture2D);
	}
	else if( Resource->Type()->IsA(FTextureCube::StaticType) )
	{
		FTextureCube* TextureCube = (FTextureCube*) Resource;
		FD3DTextureCube* D3DTextureCube = (FD3DTextureCube*) D3DResource;
		if(D3DTextureCube)
			D3DTextureCube->UpdateTexture(TextureCube);
	}
	else if( Resource->Type()->IsA(FTexture3D::StaticType) )
	{
		FTexture3D*	Texture3D = (FTexture3D*) Resource;
		FD3DTexture3D* D3DTexture3D	= (FD3DTexture3D*) D3DResource;
		if(D3DTexture3D)
			D3DTexture3D->UpdateTexture(Texture3D);
	}
	else if( Resource->Type()->IsA(FVertexBuffer::StaticType))
	{
		FVertexBuffer* VertexBuffer = (FVertexBuffer*)Resource;
		FD3DVertexBuffer* D3DVertexBuffer = (FD3DVertexBuffer*)D3DResource;
		if(D3DVertexBuffer)
		{
			if(D3DVertexBuffer->Size != VertexBuffer->Size)
				FreeResource(VertexBuffer);
			else
				D3DVertexBuffer->Invalidate();
		}
	}
	else
		FreeResource(Resource);
}

/**
 * Request an increase or decrease in the amount of miplevels a texture uses.
 *
 * @param	Texture				texture to adjust
 * @param	RequestedMips		miplevels the texture should have
 * @return	NULL if a decrease is requested, pointer to information about request otherwise (NOTE: pointer gets freed in FinalizeMipRequest)
 */
void UD3DRenderDevice::RequestMips( FTextureBase* Texture, UINT RequestedMips, FTextureMipRequest* TextureMipRequest )
{
	if( Texture->Type()->IsA( FTexture2D::StaticType ) )
	{
		FTexture2D*		Texture2D		= (FTexture2D*) Texture;
		FD3DTexture2D*	D3DTexture2D	= (FD3DTexture2D*) Resources.ResourceMap.FindRef(Texture->ResourceIndex);
	
		if( D3DTexture2D )
		{			
			D3DTexture2D->RequestMips( RequestedMips, TextureMipRequest );
		}
	}
}

/**
 * Finalizes a mip request. This gets called after all requested data has finished loading.	
 *
 * @param	TextureMipRequest	Mip request that is being finalized.
 */
void UD3DRenderDevice::FinalizeMipRequest( FTextureMipRequest* TextureMipRequest )
{	
	FTextureBase* Texture = TextureMipRequest->Texture;

	if( Texture->Type()->IsA( FTexture2D::StaticType ) )
	{
		FTexture2D*		Texture2D		= (FTexture2D*) Texture;
		FD3DTexture2D*	D3DTexture2D	= (FD3DTexture2D*) Resources.ResourceMap.FindRef(Texture->ResourceIndex);
	
		if( D3DTexture2D )
		{			
			D3DTexture2D->FinalizeMipRequest( TextureMipRequest );
		}
	}
}


//
//	UD3DRenderDevice::CacheResource
//

void UD3DRenderDevice::CacheResource(FResource* Resource)
{
	if(Resource->Type()->IsA(FMaterial::StaticType))
		CompileMaterial((FMaterial*)Resource);
	else
		Resources.GetCachedResource(Resource);
}

//
//	FD3DResourceSet::FD3DResourceSet
//

FD3DResourceSet::FD3DResourceSet(UD3DRenderDevice* RenderDevice)
{
	RenderDevice->ResourceSets.AddItem(this);
}

//
//	FD3DResourceSet::~FD3DResourceSet
//

FD3DResourceSet::~FD3DResourceSet()
{
	GD3DRenderDevice->ResourceSets.RemoveItem(this);
}

//
//	FD3DResource::FD3DResource
//

FD3DResource::FD3DResource(FD3DResourceSet* InSet):
	Set(InSet),
	Cached(0),
	NumRefs(0),
	ResourceType(NULL),
#ifdef XBOX
	BaseAddress(0),
#endif
	Size(0),
	Dynamic(0)
{
	PrevResource = NULL;
	NextResource = Set->FirstResource;
	Set->FirstResource = this;

	if(NextResource)
		NextResource->PrevResource = this;
}

//
//	FD3DResource::~FD3DResource
//

FD3DResource::~FD3DResource()
{
	if(PrevResource)
	{
		PrevResource->NextResource = NextResource;
	}
	else
	{
		Set->FirstResource = NextResource;
	}

	if(NextResource)
	{
		NextResource->PrevResource = PrevResource;
	}

	if(Cached)
		Set->FlushResource(this);

	// This could potentially be the second call to Cleanup in a row so Cleanup has to play nice.
	Cleanup();
}

//
//	FD3DResource::Dump
//

void FD3DResource::Dump(FOutputDevice& Ar)
{
	Ar.Logf(TEXT("%s: Cached=%u, NumRefs=%u"),Description.Len() ? *Description : *GetTypeDescription(),Cached,NumRefs);
}

/**
 * Calculates and caches the width, height and all other resource settings based on the base texture
 * being passed in. E.g. the width and height depend on the MaxTextureSize, format and whether the 
 * texture can be streamed, the wrapping behaviour on whether it is a cubemap or not and so on.
 *
 * @param	Source	texture to cache
 * @return	TRUE if resource needs to be recreated or FALSE if it is okay to only reupload the miplevels
*/
UBOOL FD3DTexture::CacheBaseInfo( FTextureBase* Source )
{
	GammaCorrected			= Source->IsGammaCorrected();
	RequiresPointFiltering	= Source->IsRGBE();

	if( Source->Type()->IsA(FTextureCube::StaticType) )
	{
		UsedNumFaces		= 6;
		ClampU				= 0;
		ClampV				= 0;
		ClampW				= 0;
	}
	else
	{
		UsedNumFaces		= 1;
		ClampU				= Source->GetClampX();
		ClampV				= Source->GetClampY();
		ClampW				= Source->GetClampZ();
	}

	IsStreamingTexture		= !GIsEditor && GEngine->UseStreaming && GStreamingManager->IsResourceConsidered( Source );

	if( IsStreamingTexture )
	{
		UsedFirstMip		= Max<INT>( 0, Source->NumMips - GEngine->MinStreamedInMips );
	}
	else
	{
		UsedFirstMip		= 0;
	}

	if( GD3DRenderDevice->MaxTextureSize > 0 )
		UsedFirstMip = Max<INT>( UsedFirstMip, Source->NumMips - appCeilLogTwo(GD3DRenderDevice->MaxTextureSize) - 1 );
	UsedFirstMip			= Min( UsedFirstMip, Source->NumMips - 1 );

	Source->CurrentMips		= Source->NumMips - UsedFirstMip;

	EPixelFormat CurrentFormat;
	CurrentFormat			= UsedFormat;

	UINT	CurrentSizeX	= UsedSizeX,
			CurrentSizeY	= UsedSizeY,
			CurrentSizeZ	= UsedSizeZ,
			CurrentNumMips	= UsedNumMips,
			CurrentDynamic	= Dynamic;

	UINT	BlockSizeX		= GPixelFormats[Source->Format].BlockSizeX,
			BlockSizeY		= GPixelFormats[Source->Format].BlockSizeY,
			BlockSizeZ		= GPixelFormats[Source->Format].BlockSizeZ;

	SourceSizeX				= Source->SizeX;
	SourceSizeY				= Source->SizeY;
	SourceSizeZ				= Source->SizeZ;
	SourceNumMips			= Source->NumMips;

	UsedSizeX				= Max<UINT>( BlockSizeX, Source->SizeX >> UsedFirstMip );
	UsedSizeY				= Max<UINT>( BlockSizeY, Source->SizeY >> UsedFirstMip );
	UsedSizeZ				= Max<UINT>( BlockSizeZ, Source->SizeZ >> UsedFirstMip );
	UsedFormat				= (EPixelFormat) Source->Format;
	UsedNumMips				= Source->NumMips - UsedFirstMip;
	Dynamic					= Source->Dynamic;
		

	UBOOL NeedToRecreate	= (CurrentSizeX != UsedSizeX) || (CurrentSizeY != UsedSizeY) || (CurrentSizeZ != UsedSizeZ) || (CurrentNumMips != UsedNumMips) || (UsedFormat != CurrentFormat) || (Dynamic != CurrentDynamic);

	return NeedToRecreate;
}

/**
 * Updates the texture (both info and data) based on the source template.
 *
 * @param	Source	texture used ot get mip dimensions, format and data from
 */
void FD3DTexture::UpdateTexture( FTextureBase* Source )
{
	UBOOL RequiresRecreation = CacheBaseInfo( Source );

	if( RequiresRecreation )
	{
		CreateMipChain( Source );
	}
	else
	{
		for( UINT MipIndex=0; MipIndex<UsedNumMips; MipIndex++ )
			UploadMipLevel( Source, MipIndex );
	}
}

/**
 * Returns the size in bytes this resource currently occupies.	
 *
 * @return size in bytes this resource currently occupies
 */
UINT FD3DTexture::GetSize()
{
	UINT Size = 0;
	
	for( UINT MipIndex=UsedFirstMip; MipIndex<UsedFirstMip+UsedNumMips; MipIndex++ )
		Size += GetMipSize( MipIndex );

	return Size;
}

/**
 * Returns the maximum size in bytes this resource could potentially occupy. This value
 * e.g. differs from GetSize for textures that don't have all miplevels streamed in.
 *
 * @return maximum size in bytes this resource could potentially occupy
 */
UINT FD3DTexture::GetMaxSize()
{
	UINT Size = 0;
	
	for( UINT MipIndex=0; MipIndex<SourceNumMips; MipIndex++ )
		Size += GetMipSize( MipIndex );

	return Size;
}

/**
 * Returns the size in bytes used by a specified miplevel regardless of whether
 * it is present or not.
 *
 * @param MipLevel 0- based (largest) index
 * @return size in bytes the specified milevel occupies
 */
UINT FD3DTexture::GetMipSize( UINT MipIndex )
{
	UINT Size = 0;
	
	UINT	MipSizeX = Max( SourceSizeX >> MipIndex, GPixelFormats[UsedFormat].BlockSizeX ),
			MipSizeY = Max( SourceSizeY >> MipIndex, GPixelFormats[UsedFormat].BlockSizeY ),
			MipSizeZ = Max( SourceSizeZ >> MipIndex, GPixelFormats[UsedFormat].BlockSizeZ );

	if( UsedSizeZ )
	{
		// Volume texture.
		Size += (MipSizeX / GPixelFormats[UsedFormat].BlockSizeX) * (MipSizeY / GPixelFormats[UsedFormat].BlockSizeY) * (MipSizeZ / GPixelFormats[UsedFormat].BlockSizeZ) * GPixelFormats[UsedFormat].BlockBytes;
	}
	else
	{
		// Regular texure or cubemap.
		Size += (MipSizeX / GPixelFormats[UsedFormat].BlockSizeX) * (MipSizeY / GPixelFormats[UsedFormat].BlockSizeY) * GPixelFormats[UsedFormat].BlockBytes;
	}

	// Cubemaps have six faces, other textures have one.
	return Size * UsedNumFaces;
}

//
//	UD3DRenderDevice::GetDynamicIndexBuffer
//

#define MIN_DYNAMIC_IB_SIZE	65536

FD3DDynamicIndexBuffer* UD3DRenderDevice::GetDynamicIndexBuffer(UINT MinSize,UINT Stride)
{
	check(Stride == 2 || Stride == 4);

	FD3DDynamicIndexBuffer*&	DynamicIndexBuffer = (Stride == 4) ? DynamicIndexBuffer32 : DynamicIndexBuffer16;

	if(DynamicIndexBuffer && DynamicIndexBuffer->Size < MinSize)
	{
		delete DynamicIndexBuffer;
		DynamicIndexBuffer = NULL;
	}

	if(!DynamicIndexBuffer)
		DynamicIndexBuffer = new FD3DDynamicIndexBuffer(&MiscResources,Max<UINT>(MinSize,MIN_DYNAMIC_IB_SIZE),Stride);

	return DynamicIndexBuffer;
}

//
//	D3DVERTEXELEMENT9 Compare
//

IMPLEMENT_COMPARE_CONSTREF( D3DVERTEXELEMENT9, D3DResource, { return ((INT)A.Offset + A.Stream * MAXWORD) - ((INT)B.Offset + B.Stream * MAXWORD); } )

//
//	UD3DRenderDevice::CreateVertexDeclaration
//

IDirect3DVertexDeclaration9* UD3DRenderDevice::CreateVertexDeclaration(const TArray<D3DVERTEXELEMENT9>& Elements)
{
	FD3DVertexDeclarationId	Id;
	appMemcpy(&Id.Elements(Id.Elements.Add(Elements.Num())),&Elements(0),Elements.Num() * sizeof(D3DVERTEXELEMENT9));

	Sort<USE_COMPARE_CONSTREF(D3DVERTEXELEMENT9, D3DResource)>(&Id.Elements(0),Id.Elements.Num());

#if (defined _DEBUG) && !(defined XBOX)
	UBOOL CanShareOffsets = GD3DRenderDevice->DeviceCaps.DevCaps2 & D3DDEVCAPS2_VERTEXELEMENTSCANSHARESTREAMOFFSET;
	if( !CanShareOffsets )
		for( INT i=0; i<Id.Elements.Num()-1; i++ )
			check( (Id.Elements(i).Stream < Id.Elements(i+1).Stream) || (Id.Elements(i).Offset < Id.Elements(i+1).Offset) );
#endif

	D3DVERTEXELEMENT9	TerminatorElement = D3DDECL_END();
	Id.Elements.AddItem(TerminatorElement);

	TD3DRef<IDirect3DVertexDeclaration9>*	CachedVertexDeclaration = CachedVertexDeclarations.Find(Id);
	if(CachedVertexDeclaration)
		return *CachedVertexDeclaration;

	TD3DRef<IDirect3DVertexDeclaration9>	VertexDeclaration;

	USE_OWN_ALLOCATOR;
	HRESULT									Result = GDirect3DDevice->CreateVertexDeclaration(&Id.Elements(0),&VertexDeclaration.Handle);
	USE_DEFAULT_ALLOCATOR;

	if(FAILED(Result))
		appErrorf(TEXT("CreateVertexDeclaration failed: %s"),*D3DError(Result));

	CachedVertexDeclarations.Set(Id,VertexDeclaration);

	return VertexDeclaration;
}

IDirect3DVertexDeclaration9* UD3DRenderDevice::GetCanvasVertexDeclaration()
{
	if(!CanvasVertexDeclaration)
	{
		// Create a canvas vertex declaration.
		TArray<D3DVERTEXELEMENT9> CanvasVertexElements;
		AddVertexElement(CanvasVertexElements,0,STRUCT_OFFSET(FD3DCanvasVertex,Position),D3DDECLTYPE_FLOAT4,D3DDECLUSAGE_POSITION,0);
		AddVertexElement(CanvasVertexElements,0,STRUCT_OFFSET(FD3DCanvasVertex,U),D3DDECLTYPE_FLOAT3,D3DDECLUSAGE_TEXCOORD,0);
		AddVertexElement(CanvasVertexElements,0,STRUCT_OFFSET(FD3DCanvasVertex,Color),D3DDECLTYPE_D3DCOLOR,D3DDECLUSAGE_TEXCOORD,1);
		CanvasVertexDeclaration = GD3DRenderDevice->CreateVertexDeclaration(CanvasVertexElements);
	}
	return CanvasVertexDeclaration;
}

//
//	FD3DVertexFactory::CacheStreams
//

void FD3DVertexFactory::CacheStreams(const TArray<FVertexBuffer*>& SourceStreams)
{
	for(UINT StreamIndex = 0;StreamIndex < (UINT)SourceStreams.Num();StreamIndex++)
	{
		FD3DVertexBuffer* CachedVertexBuffer = (FD3DVertexBuffer*)GD3DRenderDevice->Resources.GetCachedResource(SourceStreams(StreamIndex));
		FD3DVertexStream* Stream = new(VertexStreams) FD3DVertexStream;
		Stream->VertexBuffer = CachedVertexBuffer->GetVertexBuffer(SourceStreams(StreamIndex));
		Stream->Stride = SourceStreams(StreamIndex)->Stride;
	}
}

//
//	FD3DVertexFactory::Set
//

UINT FD3DVertexFactory::Set(FVertexFactory* VertexFactory)
{
	GDirect3DDevice->SetVertexDeclaration(VertexDeclaration);

	for(UINT StreamIndex = 0;StreamIndex < (UINT)VertexStreams.Num();StreamIndex++)
	{
		GDirect3DDevice->SetStreamSource(StreamIndex,VertexStreams(StreamIndex).VertexBuffer,0,VertexStreams(StreamIndex).Stride);
	}

	return 0;

}

//
//	FD3DLocalVertexFactory::Cache
//

void FD3DLocalVertexFactory::Cache(FLocalVertexFactory* SourceVertexFactory)
{
	TArray<FVertexBuffer*>	SourceStreams;

#ifndef XBOX
		  UBOOL CanShareOffsets = GD3DRenderDevice->DeviceCaps.DevCaps2 & D3DDEVCAPS2_VERTEXELEMENTSCANSHARESTREAMOFFSET;
#else
	const UBOOL CanShareOffsets = 1;
#endif

	// Create a new vertex declaration.

	check(SourceVertexFactory->PositionComponent.Type == VCT_Float3);

	TArray<D3DVERTEXELEMENT9>	VertexElements;

	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->PositionComponent.VertexBuffer),
		SourceVertexFactory->PositionComponent.Offset,
		D3DDECLTYPE_FLOAT3,
		D3DDECLUSAGE_POSITION,
		0
		);

	if(SourceVertexFactory->LightMaskComponent.VertexBuffer && SourceVertexFactory->LightMaskComponent.Type == VCT_Float1)
		AddVertexElement(
			VertexElements,
			SourceStreams.AddUniqueItem(SourceVertexFactory->LightMaskComponent.VertexBuffer),
			SourceVertexFactory->LightMaskComponent.Offset,
			D3DDECLTYPE_FLOAT1,
			D3DDECLUSAGE_BLENDWEIGHT,
			0
			);

	if(SourceVertexFactory->StaticLightingComponent.VertexBuffer && SourceVertexFactory->StaticLightingComponent.Type == VCT_Float3)
		AddVertexElement(
			VertexElements,
			SourceStreams.AddUniqueItem(SourceVertexFactory->StaticLightingComponent.VertexBuffer),
			SourceVertexFactory->StaticLightingComponent.Offset,
			D3DDECLTYPE_FLOAT3,
			D3DDECLUSAGE_BLENDWEIGHT,
			1
			);

	D3DDECLUSAGE	TangentComponentUsage[3] = { D3DDECLUSAGE_TANGENT, D3DDECLUSAGE_BINORMAL, D3DDECLUSAGE_NORMAL };
	for(UINT TangentComponentIndex = 0;TangentComponentIndex < 3;TangentComponentIndex++)
		if(SourceVertexFactory->TangentBasisComponents[TangentComponentIndex].Type == VCT_PackedNormal)
			AddVertexElement(
				VertexElements,
				SourceStreams.AddUniqueItem(SourceVertexFactory->TangentBasisComponents[TangentComponentIndex].VertexBuffer),
				SourceVertexFactory->TangentBasisComponents[TangentComponentIndex].Offset,
				D3DDECLTYPE_UBYTE4,
				TangentComponentUsage[TangentComponentIndex],
				0
				);

	for(UINT TexCoordIndex = 0;TexCoordIndex < SourceVertexFactory->TextureCoordinates.Num && TexCoordIndex < MAX_TEXCOORDS;TexCoordIndex++)
	{
		check(SourceVertexFactory->TextureCoordinates(TexCoordIndex).Type == VCT_Float2);
		AddVertexElement(
			VertexElements,
			SourceStreams.AddUniqueItem(SourceVertexFactory->TextureCoordinates(TexCoordIndex).VertexBuffer),
			SourceVertexFactory->TextureCoordinates(TexCoordIndex).Offset,
			D3DDECLTYPE_FLOAT2,
			D3DDECLUSAGE_TEXCOORD,
			TexCoordIndex
			);
	}

	if(SourceVertexFactory->TextureCoordinates.Num)
	{
		for(UINT TexCoordIndex = SourceVertexFactory->TextureCoordinates.Num;TexCoordIndex < MAX_TEXCOORDS;TexCoordIndex++)
		{
			INT	SourceTexCoordIndex = SourceVertexFactory->TextureCoordinates.Num - 1;
			check(SourceVertexFactory->TextureCoordinates(SourceTexCoordIndex).Type == VCT_Float2);
			AddVertexElement(
				VertexElements,
				CanShareOffsets ?	SourceStreams.AddUniqueItem(SourceVertexFactory->TextureCoordinates(SourceTexCoordIndex).VertexBuffer)
								:	SourceStreams.AddItem(SourceVertexFactory->TextureCoordinates(SourceTexCoordIndex).VertexBuffer),
				SourceVertexFactory->TextureCoordinates(SourceTexCoordIndex).Offset,
				D3DDECLTYPE_FLOAT2,
				D3DDECLUSAGE_TEXCOORD,
				TexCoordIndex
				);
		}
	}

	if(SourceVertexFactory->LightMapCoordinateComponent.VertexBuffer && SourceVertexFactory->LightMapCoordinateComponent.Type == VCT_Float2)
	{
		AddVertexElement(
			VertexElements,
			SourceStreams.AddItem(SourceVertexFactory->LightMapCoordinateComponent.VertexBuffer),
			SourceVertexFactory->LightMapCoordinateComponent.Offset,
			D3DDECLTYPE_FLOAT2,
			D3DDECLUSAGE_COLOR,
			0
			);
	}
	else if(SourceVertexFactory->TextureCoordinates.Num)
	{
		AddVertexElement(
			VertexElements,
			CanShareOffsets ?	SourceStreams.AddUniqueItem(SourceVertexFactory->TextureCoordinates(SourceVertexFactory->TextureCoordinates.Num - 1).VertexBuffer)
							:	SourceStreams.AddItem(SourceVertexFactory->TextureCoordinates(SourceVertexFactory->TextureCoordinates.Num - 1).VertexBuffer),
			SourceVertexFactory->TextureCoordinates(SourceVertexFactory->TextureCoordinates.Num - 1).Offset,
			D3DDECLTYPE_FLOAT2,
			D3DDECLUSAGE_COLOR,
			0
			);
	}

	VertexDeclaration = GD3DRenderDevice->CreateVertexDeclaration(VertexElements);
	CacheStreams(SourceStreams);

}

//
//	FD3DLocalShadowVertexFactory::Cache
//

void FD3DLocalShadowVertexFactory::Cache(FLocalShadowVertexFactory* SourceVertexFactory)
{
	TArray<FVertexBuffer*>	SourceStreams;

	// Create a new vertex declaration.

	check(SourceVertexFactory->PositionComponent.Type == VCT_Float3);
	check(SourceVertexFactory->ExtrusionComponent.Type == VCT_Float1);

	TArray<D3DVERTEXELEMENT9>	VertexElements;
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->PositionComponent.VertexBuffer),
		SourceVertexFactory->PositionComponent.Offset,
		D3DDECLTYPE_FLOAT3, 
		D3DDECLUSAGE_POSITION,
		0
		);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->ExtrusionComponent.VertexBuffer),
		SourceVertexFactory->ExtrusionComponent.Offset,
		D3DDECLTYPE_FLOAT1,
		D3DDECLUSAGE_TEXCOORD,
		0
		);

	VertexDeclaration = GD3DRenderDevice->CreateVertexDeclaration(VertexElements);
	CacheStreams(SourceStreams);
}

//
//	FD3DTerrainVertexFactory::Cache
//

void FD3DTerrainVertexFactory::Cache(FTerrainVertexFactory* SourceVertexFactory)
{
	TArray<FVertexBuffer*>	SourceStreams;

	// Create a new vertex declaration.

	check(SourceVertexFactory->PositionComponent.Type == VCT_UByte4 || SourceVertexFactory->PositionComponent.Type == VCT_Float4);
	check(SourceVertexFactory->GradientsComponent.Type == VCT_Short2);

	TArray<D3DVERTEXELEMENT9>	VertexElements;
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->PositionComponent.VertexBuffer),
		SourceVertexFactory->PositionComponent.Offset,
		D3DDECLTYPE_UBYTE4,
		D3DDECLUSAGE_POSITION,
		0
		);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->GradientsComponent.VertexBuffer),
		SourceVertexFactory->GradientsComponent.Offset,
		D3DDECLTYPE_SHORT2,
		D3DDECLUSAGE_TANGENT,
		0
		);

	if(SourceVertexFactory->DisplacementComponent.VertexBuffer && SourceVertexFactory->DisplacementComponent.Type == VCT_Float1)
		AddVertexElement(
			VertexElements,
			SourceStreams.AddUniqueItem(SourceVertexFactory->DisplacementComponent.VertexBuffer),
			SourceVertexFactory->DisplacementComponent.Offset,
			D3DDECLTYPE_FLOAT1,
			D3DDECLUSAGE_BLENDWEIGHT,
			0
			);

	VertexDeclaration = GD3DRenderDevice->CreateVertexDeclaration(VertexElements);
	CacheStreams(SourceStreams);
}

//
//	FD3DFoliageVertexFactory::Cache
//

void FD3DFoliageVertexFactory::Cache(FFoliageVertexFactory* SourceVertexFactory)
{
	TArray<FVertexBuffer*>	SourceStreams;

	// Create a new vertex declaration.

	check(SourceVertexFactory->PositionComponent.Type == VCT_Float3);

	TArray<D3DVERTEXELEMENT9>	VertexElements;
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->PositionComponent.VertexBuffer),
		SourceVertexFactory->PositionComponent.Offset,
		D3DDECLTYPE_FLOAT3,
		D3DDECLUSAGE_POSITION,
		0
		);

	if(SourceVertexFactory->PointSourcesComponent.VertexBuffer && SourceVertexFactory->PointSourcesComponent.Type == VCT_UByte4)
		AddVertexElement(
			VertexElements,
			SourceStreams.AddUniqueItem(SourceVertexFactory->PointSourcesComponent.VertexBuffer),
			SourceVertexFactory->PointSourcesComponent.Offset,
			D3DDECLTYPE_UBYTE4,
			D3DDECLUSAGE_BLENDINDICES,
			0
			);

	if(SourceVertexFactory->SwayStrengthComponent.VertexBuffer && SourceVertexFactory->SwayStrengthComponent.Type == VCT_Float1)
		AddVertexElement(
			VertexElements,
			SourceStreams.AddUniqueItem(SourceVertexFactory->SwayStrengthComponent.VertexBuffer),
			SourceVertexFactory->SwayStrengthComponent.Offset,
			D3DDECLTYPE_FLOAT1,
			D3DDECLUSAGE_BLENDWEIGHT,
			0
			);

	if(SourceVertexFactory->StaticLightingComponent.VertexBuffer && SourceVertexFactory->StaticLightingComponent.Type == VCT_Float3)
		AddVertexElement(
			VertexElements,
			SourceStreams.AddUniqueItem(SourceVertexFactory->StaticLightingComponent.VertexBuffer),
			SourceVertexFactory->StaticLightingComponent.Offset,
			D3DDECLTYPE_FLOAT3,
			D3DDECLUSAGE_BLENDWEIGHT,
			1
			);

	if(SourceVertexFactory->TextureCoordinateComponent.VertexBuffer && SourceVertexFactory->TextureCoordinateComponent.Type == VCT_Float2)
		AddVertexElement(
			VertexElements,
			SourceStreams.AddUniqueItem(SourceVertexFactory->TextureCoordinateComponent.VertexBuffer),
			SourceVertexFactory->TextureCoordinateComponent.Offset,
			D3DDECLTYPE_FLOAT2,
			D3DDECLUSAGE_TEXCOORD,
			0
			);

	if(SourceVertexFactory->LightMapCoordinateComponent.VertexBuffer && SourceVertexFactory->LightMapCoordinateComponent.Type == VCT_Float2)
		AddVertexElement(
			VertexElements,
			SourceStreams.AddUniqueItem(SourceVertexFactory->LightMapCoordinateComponent.VertexBuffer),
			SourceVertexFactory->LightMapCoordinateComponent.Offset,
			D3DDECLTYPE_FLOAT2,
			D3DDECLUSAGE_TEXCOORD,
			1
			);

	D3DDECLUSAGE	TangentComponentUsage[3] = { D3DDECLUSAGE_TANGENT, D3DDECLUSAGE_BINORMAL, D3DDECLUSAGE_NORMAL };
	for(UINT TangentComponentIndex = 0;TangentComponentIndex < 3;TangentComponentIndex++)
		if(SourceVertexFactory->TangentBasisComponents[TangentComponentIndex].Type == VCT_PackedNormal)
			AddVertexElement(
				VertexElements,
				SourceStreams.AddUniqueItem(SourceVertexFactory->TangentBasisComponents[TangentComponentIndex].VertexBuffer),
				SourceVertexFactory->TangentBasisComponents[TangentComponentIndex].Offset,
				D3DDECLTYPE_UBYTE4,
				TangentComponentUsage[TangentComponentIndex],
				0
				);

	VertexDeclaration = GD3DRenderDevice->CreateVertexDeclaration(VertexElements);
	CacheStreams(SourceStreams);
}

//
//	FD3DParticleVertexFactory::Cache
//

void FD3DParticleVertexFactory::Cache(FParticleVertexFactory* SourceVertexFactory)
{
	TArray<FVertexBuffer*>	SourceStreams;

	// Create a new vertex declaration.

	TArray<D3DVERTEXELEMENT9>	VertexElements;
	
	check(SourceVertexFactory->PositionComponent.Type == VCT_Float3);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->PositionComponent.VertexBuffer), 
		SourceVertexFactory->PositionComponent.Offset, 
		D3DDECLTYPE_FLOAT3,
		D3DDECLUSAGE_POSITION, 
		0
		);

	check(SourceVertexFactory->OldPositionComponent.Type == VCT_Float3);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->OldPositionComponent.VertexBuffer), 
		SourceVertexFactory->OldPositionComponent.Offset, 
		D3DDECLTYPE_FLOAT3, 
		D3DDECLUSAGE_NORMAL, 
		0
		);

	check(SourceVertexFactory->SizeComponent.Type == VCT_Float3);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->SizeComponent.VertexBuffer), 
		SourceVertexFactory->SizeComponent.Offset, 
		D3DDECLTYPE_FLOAT3,
		D3DDECLUSAGE_TANGENT, 
		0
		);

	check(SourceVertexFactory->UVComponent.Type == VCT_Float2);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->UVComponent.VertexBuffer),
		SourceVertexFactory->UVComponent.Offset,
		D3DDECLTYPE_FLOAT2,
		D3DDECLUSAGE_TEXCOORD,
		0
		);

	check(SourceVertexFactory->RotationComponent.Type == VCT_Float1);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->RotationComponent.VertexBuffer),
		SourceVertexFactory->RotationComponent.Offset,
		D3DDECLTYPE_FLOAT1,
		D3DDECLUSAGE_BLENDWEIGHT,
		0
		);

	check(SourceVertexFactory->ColorComponent.Type == VCT_UByte4);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->ColorComponent.VertexBuffer),
		SourceVertexFactory->ColorComponent.Offset,
		D3DDECLTYPE_D3DCOLOR,
		D3DDECLUSAGE_COLOR,
		0
		);


	VertexDeclaration = GD3DRenderDevice->CreateVertexDeclaration(VertexElements);
	CacheStreams(SourceStreams);
}

//
//	FD3DParticleSubUVVertexFactory::Cache
//

void FD3DParticleSubUVVertexFactory::Cache(FParticleSubUVVertexFactory* SourceVertexFactory)
{
	TArray<FVertexBuffer*>	SourceStreams;

	// Create a new vertex declaration.

	TArray<D3DVERTEXELEMENT9>	VertexElements;
	
	check(SourceVertexFactory->PositionComponent.Type == VCT_Float3);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->PositionComponent.VertexBuffer), 
		SourceVertexFactory->PositionComponent.Offset, 
		D3DDECLTYPE_FLOAT3,
		D3DDECLUSAGE_POSITION, 
		0
		);

	check(SourceVertexFactory->OldPositionComponent.Type == VCT_Float3);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->OldPositionComponent.VertexBuffer), 
		SourceVertexFactory->OldPositionComponent.Offset, 
		D3DDECLTYPE_FLOAT3, 
		D3DDECLUSAGE_NORMAL, 
		0
		);

	check(SourceVertexFactory->SizeComponent.Type == VCT_Float3);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->SizeComponent.VertexBuffer), 
		SourceVertexFactory->SizeComponent.Offset, 
		D3DDECLTYPE_FLOAT3,
		D3DDECLUSAGE_TANGENT, 
		0
		);

	check(SourceVertexFactory->UVComponent.Type == VCT_Float2);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->UVComponent.VertexBuffer),
		SourceVertexFactory->UVComponent.Offset,
		D3DDECLTYPE_FLOAT2,
		D3DDECLUSAGE_TEXCOORD,
		0
		);

	check(SourceVertexFactory->RotationComponent.Type == VCT_Float1);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->RotationComponent.VertexBuffer),
		SourceVertexFactory->RotationComponent.Offset,
		D3DDECLTYPE_FLOAT1,
		D3DDECLUSAGE_BLENDWEIGHT,
		0
		);

	check(SourceVertexFactory->ColorComponent.Type == VCT_UByte4);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->ColorComponent.VertexBuffer),
		SourceVertexFactory->ColorComponent.Offset,
		D3DDECLTYPE_D3DCOLOR,
		D3DDECLUSAGE_COLOR,
		0
		);

	check(SourceVertexFactory->UVComponent2.Type == VCT_Float2);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->UVComponent2.VertexBuffer),
		SourceVertexFactory->UVComponent2.Offset,
		D3DDECLTYPE_FLOAT2,
		D3DDECLUSAGE_TEXCOORD,
		1
		);

	check(SourceVertexFactory->Interpolation.Type == VCT_Float2);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->Interpolation.VertexBuffer),
		SourceVertexFactory->Interpolation.Offset,
		D3DDECLTYPE_FLOAT2,
		D3DDECLUSAGE_TEXCOORD,
		2
		);

	check(SourceVertexFactory->Interpolation.Type == VCT_Float2);
	AddVertexElement(
		VertexElements,
		SourceStreams.AddUniqueItem(SourceVertexFactory->Interpolation.VertexBuffer),
		SourceVertexFactory->SizeScaler.Offset,
		D3DDECLTYPE_FLOAT2,
		D3DDECLUSAGE_TEXCOORD,
		3
		);

	VertexDeclaration = GD3DRenderDevice->CreateVertexDeclaration(VertexElements);
	CacheStreams(SourceStreams);
}

//
//	FD3DSceneViewState::Cache
//

void FD3DSceneViewState::Cache(FSceneViewState* SourceState)
{
#ifndef XBOX
	// Create two 1x1 textures which contain the exposure for the current frame and the previous frame.

	for(UINT TextureIndex = 0;TextureIndex < 2;TextureIndex++)
	{
		HRESULT	Result = GDirect3DDevice->CreateTexture(1,1,1,D3DUSAGE_RENDERTARGET,D3DFMT_R32F,D3DPOOL_DEFAULT,&ExposureTextures[TextureIndex].Handle,NULL);
		if(FAILED(Result))
			appErrorf(TEXT("CreateTexture failed: %s"),*D3DError(Result));

		Result = GDirect3DDevice->ColorFill(GetTextureSurface(ExposureTextures[TextureIndex]),NULL,D3DCOLOR_RGBA(1,0,0,0));
		if(FAILED(Result))
			appErrorf(TEXT("ColorFill failed: %s"),*D3DError(Result));
	}
#endif
}

//
//	FD3DResourceCache::GetCachedResource
//

FD3DResource* FD3DResourceCache::GetCachedResource(FResource* Source)
{
	FD3DResource*	CachedResource = TD3DResourceCache<INT,FD3DResource>::GetCachedResource(Source->ResourceIndex);
	if(!CachedResource)
	{
		FResourceType*	Type = Source->Type();
		if(Type->IsA(FTexture2D::StaticType))
			CachedResource = new FD3DTexture2D(this,(FTexture2D*)Source);
		else if(Type->IsA(FTexture3D::StaticType))
			CachedResource = new FD3DTexture3D(this,(FTexture3D*)Source);
		else if(Type->IsA(FTextureCube::StaticType))
			CachedResource = new FD3DTextureCube(this,(FTextureCube*)Source);
		else if(Type->IsA(FIndexBuffer::StaticType))
			CachedResource = new FD3DIndexBuffer(this,(FIndexBuffer*)Source);
		else if(Type->IsA(FVertexBuffer::StaticType))
			CachedResource = new FD3DVertexBuffer(this,(FVertexBuffer*)Source);
		else if(Type->IsA(FLocalVertexFactory::StaticType))
			CachedResource = new FD3DLocalVertexFactory(this,(FLocalVertexFactory*)Source);
		else if(Type->IsA(FLocalShadowVertexFactory::StaticType))
			CachedResource = new FD3DLocalShadowVertexFactory(this,(FLocalShadowVertexFactory*)Source);
		else if(Type->IsA(FTerrainVertexFactory::StaticType))
			CachedResource = new FD3DTerrainVertexFactory(this,(FTerrainVertexFactory*)Source);
		else if(Type->IsA(FFoliageVertexFactory::StaticType))
			CachedResource = new FD3DFoliageVertexFactory(this,(FFoliageVertexFactory*)Source);
		else if(Type->IsA(FParticleSubUVVertexFactory::StaticType))
			CachedResource = new FD3DParticleSubUVVertexFactory(this,(FParticleSubUVVertexFactory*)Source);
		else if(Type->IsA(FParticleVertexFactory::StaticType))
			CachedResource = new FD3DParticleVertexFactory(this,(FParticleVertexFactory*)Source);
		else if(Type->IsA(FSceneViewState::StaticType))
			CachedResource = new FD3DSceneViewState(this,(FSceneViewState*)Source);
		else
			appErrorf(TEXT("Cannot cache resource type %s"),Type->Name);

		CachedResource->Description		= Source->DescribeResource();
		CachedResource->ResourceType	= Source->Type();

		AddCachedResource(Source->ResourceIndex,CachedResource);
	}
	return CachedResource;
}
