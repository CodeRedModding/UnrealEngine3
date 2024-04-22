/*=============================================================================
	D3D9Resources.h: D3D resource RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "BoundShaderStateCache.h"

/**
* Combined shader state and vertex definition for rendering geometry. 
* Each unique instance consists of a vertex decl, vertex shader, and pixel shader.
*/
class FD3D9BoundShaderState : public FRefCountedObject, public TDynamicRHIResource<RRT_BoundShaderState>
{
public:

	FCachedBoundShaderStateLink CacheLink;

	TRefCountPtr<IDirect3DVertexDeclaration9> VertexDeclaration;
	TRefCountPtr<IDirect3DVertexShader9> VertexShader;
	TRefCountPtr<IDirect3DPixelShader9> PixelShader;

	/** Initialization constructor. */
	FD3D9BoundShaderState(
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		DWORD* InStreamStrides,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI
		);

	/**
	* Equality is based on vertex decl, vertex shader and pixel shader
	* @param Other - instance to compare against
	* @return TRUE if equal
	*/
	UBOOL operator==(const FD3D9BoundShaderState& Other) const
	{
		return (VertexDeclaration == Other.VertexDeclaration && VertexShader == Other.VertexShader && PixelShader == Other.PixelShader);
	}

	/**
	* Get the hash for this type
	* @param Key - struct to hash
	* @return dword hash based on type
	*/
	friend DWORD GetTypeHash(const FD3D9BoundShaderState& Key)
	{
		return PointerHash(
			(IDirect3DVertexDeclaration9*)Key.VertexDeclaration, 
			PointerHash((IDirect3DVertexShader9 *)Key.VertexShader, 
			PointerHash((IDirect3DPixelShader9 *)Key.PixelShader))
			);
	}
};

// Textures.
template<typename D3DResourceType,ERHIResourceTypes ResourceTypeEnum>
class TD3D9Texture : public FRefCountedObject, public TRefCountPtr<D3DResourceType>, public TDynamicRHIResource<ResourceTypeEnum>
{
public:
	TD3D9Texture(EPixelFormat InUnrealFormat, UBOOL bInSRGB = FALSE,UBOOL bInDynamic = FALSE,D3DResourceType* InResource = NULL)
		: TRefCountPtr<D3DResourceType>(InResource)
		, UnrealFormat( InUnrealFormat )
		, MemorySize( 0 )
		, bSRGB(bInSRGB) 
		, bDynamic(bInDynamic)
	{
	}

	virtual ~TD3D9Texture()
	{
		if ( TRefCountPtr<D3DResourceType>::GetRefCount() == 1 )
		{
			D3D9TextureDeleted( *this );
		}
	}

	// Accessors
	UBOOL IsSRGB() const
	{
		return bSRGB; 
	}
	UBOOL IsDynamic() const
	{ 
		return bDynamic;
	}

	EPixelFormat GetUnrealFormat() const
	{
		return UnrealFormat;
	}

	INT GetMemorySize() const
	{
		return MemorySize;
	}

	void SetMemorySize( INT InMemorySize )
	{
		MemorySize = InMemorySize;
	}

private:
	EPixelFormat UnrealFormat;
	INT MemorySize;
	BITFIELD bSRGB:1;
	BITFIELD bDynamic:1;
};

/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
class FD3D9OcclusionQuery : public FRefCountedObject, public TDynamicRHIResource<RRT_OcclusionQuery>
{
public:

	/** The query resource. */
	TRefCountPtr<IDirect3DQuery9> Resource;

	/** The cached query result. */
	DWORD Result;

	/** TRUE if the query's result is cached. */
	UBOOL bResultIsCached : 1;

	/** Initialization constructor. */
	FD3D9OcclusionQuery(IDirect3DQuery9* InResource):
		Resource(InResource),
		bResultIsCached(FALSE)
	{}
};

typedef TD3D9Texture<IDirect3DBaseTexture9,RRT_Texture>		FD3D9Texture;
typedef TD3D9Texture<IDirect3DTexture9,RRT_Texture2D>		FD3D9Texture2D;
typedef TD3D9Texture<IDirect3DTexture9,RRT_Texture2DArray>	FD3D9Texture2DArray;
typedef TD3D9Texture<IDirect3DTexture9,RRT_Texture3D>		FD3D9Texture3D;
typedef TD3D9Texture<IDirect3DCubeTexture9,RRT_TextureCube>	FD3D9TextureCube;
typedef TD3D9Texture<IDirect3DTexture9,RRT_SharedTexture2D>	FD3D9SharedTexture2D;
typedef TD3D9Texture<IDirect3DTexture9,RRT_SharedTexture2DArray>	FD3D9SharedTexture2DArray;

// Resources that directly map to D3D resources.
template<typename D3DResourceType,ERHIResourceTypes ResourceTypeEnum> class TD3D9Resource : public D3DResourceType, public TDynamicRHIResource<ResourceTypeEnum> {};
typedef TD3D9Resource<IDirect3DVertexDeclaration9,RRT_VertexDeclaration>	FD3D9VertexDeclaration;
typedef TD3D9Resource<IDirect3DVertexShader9,RRT_VertexShader>				FD3D9VertexShader;
typedef TD3D9Resource<IDirect3DPixelShader9,RRT_PixelShader>				FD3D9PixelShader;
typedef TD3D9Resource<IDirect3DIndexBuffer9,RRT_IndexBuffer>				FD3D9IndexBuffer;
typedef TD3D9Resource<IDirect3DVertexBuffer9,RRT_VertexBuffer>				FD3D9VertexBuffer;
typedef TD3D9Resource<FRefCountedObject,RRT_SharedMemoryResource>			FD3D9SharedMemoryResource;

template <typename FTextureType>
void D3D9TextureDeleted( FTextureType& Texture )
{
}
void D3D9TextureDeleted( FD3D9Texture2D& Texture );
void D3D9TextureAllocated( FD3D9Texture2D& Texture );

// No GS/HS/DS/CS support in DX9, but need this to properly conform to RHI
typedef FRefCountedObject FD3D9HullShader;
typedef FRefCountedObject FD3D9DomainShader;
typedef FRefCountedObject FD3D9GeometryShader;
typedef FRefCountedObject FD3D9ComputeShader;

