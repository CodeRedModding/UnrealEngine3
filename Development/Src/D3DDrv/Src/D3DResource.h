/*=============================================================================
	D3DResource.h: Unreal Direct3D resource definition.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

//
//	FD3DResourceSet
//

struct FD3DResourceSet
{
	struct FD3DResource* FirstResource;

	// Constructor/destructor.

	FD3DResourceSet(UD3DRenderDevice* RenderDevice);
	~FD3DResourceSet();

	// Flush

	virtual void Flush() {}

	// FreeResource - Called when a potentially cached resource has been freed.

	virtual void FreeResource(FResource* Resource) {}

	// FlushResource - Removes a resource from the set.

	virtual void FlushResource(FD3DResource* Resource) {}

	// Temporary console helper.

	virtual void UploadToVRAM() {} //@todo xenon: remove with UMA
};

//
//	FD3DResource
//

struct FD3DResource
{
	FD3DResourceSet*	Set;
	UBOOL				Cached;
	UINT				NumRefs;
	FString				Description;
	FResourceType*		ResourceType;
#ifdef XBOX
	void*				BaseAddress;
#endif
	/** Size in bytes																								*/
	UINT				Size;
	/** Whether this resource is dynamic or not	(is being updated via the resource manager UpateResource interface)	*/
	UBOOL				Dynamic;
	

	FD3DResource* PrevResource;
	FD3DResource* NextResource;

	// Constructor/destructor.

	FD3DResource(FD3DResourceSet* InSet);
	virtual ~FD3DResource();

	// Reference counting.

	void AddRef() { NumRefs++; }
	void RemoveRef() { if(--NumRefs == 0) delete this; }

	// Debugging.

	virtual FString GetTypeDescription() { return TEXT("Resource"); }
	virtual void Dump(FOutputDevice& Ar);
	virtual void DumpShader() {}

	/**
	 * Returns the size in bytes this resource occupies.
	 *
	 * @return Size of uploaded resource in bytes.
	 */
	virtual UINT GetSize() { return Size; }

	/**
	 * Returns the size in bytes this resource occupies at the maximum LOD level.
	 *
	 * @return maximum Size of uploaded resource in bytes.
	 */
	virtual UINT GetMaxSize() { return Size; }

	/**
	 * Cleans up resources like e.g. new'ed D3D resources or physical memory allocated (BaseAddress)
	 * on platforms where we can't use Release() to clean up.
	 */
	virtual void Cleanup() {}

	// Temporary console helper.

	virtual void UploadToVRAM() {} //@todo xenon: remove with UMA
};

//
//	TD3DResourceCache
//

template<class IdType,class ResourceType> struct TD3DResourceCache: FD3DResourceSet
{
	TDynamicMap<IdType,ResourceType*>	ResourceMap;

	// Constructor.

	TD3DResourceCache(UD3DRenderDevice* InRenderDevice):
		FD3DResourceSet(InRenderDevice)
	{}

	// Flush

	void Flush()
	{
		for(TDynamicMap<IdType,ResourceType*>::TIterator It(ResourceMap);It;++It)
		{
			ResourceType*	CachedResource = It.Value();
			It.RemoveCurrent();
			CachedResource->Cached = 0;
			CachedResource->RemoveRef();
		}
	}

	// UploadToVRAM

	void UploadToVRAM()
	{
		for(TDynamicMap<IdType,ResourceType*>::TIterator It(ResourceMap);It;++It)
		{
			ResourceType* CachedResource = It.Value();
			CachedResource->UploadToVRAM();
		}
	}

	// AddCachedResource

	void AddCachedResource(const IdType& Id,ResourceType* Resource)
	{
		ResourceMap.Set(Id,Resource);
		Resource->Cached = 1;
		Resource->AddRef();
	}

	// GetCachedResource

	ResourceType* GetCachedResource(const IdType& Id)
	{
		return ResourceMap.FindRef(Id);
	}

	// FlushResourceId

	void FlushResourceId(const IdType& Id)
	{
		ResourceType*	CachedResource = ResourceMap.FindRef(Id);

		if(CachedResource)
		{
			ResourceMap.Remove(Id);
			CachedResource->Cached = 0; // Make sure that if the following line deletes the resource, it doesn't try to remove itself from the TDynamicMap we're iterating over.
			CachedResource->RemoveRef();
		}
	}

	// FD3DResourceCacheBase interface.

	virtual void FlushResource(FD3DResource* Resource)
	{
		for(TDynamicMap<IdType,ResourceType*>::TIterator It(ResourceMap);It;++It)
		{
			ResourceType*	CachedResource = It.Value();
			if(CachedResource == Resource)
			{
				It.RemoveCurrent();
				CachedResource->Cached = 0; // Make sure that if the following line deletes the resource, it doesn't try to remove itself from the TDynamicMap we're iterating over.
				CachedResource->RemoveRef();
			}
		}
	}
};

/**
 * The base texture class FD3DTexture[2D/3D/Cube] derive from.
 */
struct FD3DTexture: FD3DResource
{
	/** Unique resource index used for hash															*/
	INT								ResourceIndex;
	
	/** D3D base resource																			*/
	TD3DRef<IDirect3DBaseTexture9>	Texture;

	/** Intermediate texture used for streaming in mips. Gets switched with used texture in Switcheroo */
	TD3DRef<IDirect3DBaseTexture9>	IntermediateTexture;
#ifdef XBOX
	/** Base address of intermediate texture														*/
	void*							IntermediateBaseAddress;
#endif

	/** Whether this is an sRGB texture or not														*/
	UBOOL							GammaCorrected;
	/** Whether this texture requires point filtering or not - e.g. RGBE requires point filtering	*/
	UBOOL							RequiresPointFiltering;
	/** Wrap mode in U direction																	*/ 
	UBOOL							ClampU;
	/** Wrap mode in V direction																	*/ 
	UBOOL							ClampV;
	/** Wrap mode in W direction																	*/ 
	UBOOL							ClampW;
	/** Whether texture is streamed																	*/
	UBOOL							IsStreamingTexture;

	/** Full width of source texture																*/
	UINT							SourceSizeX;
	/** Full height of source texture																*/
	UINT							SourceSizeY;
	/** Full depth of source texture																*/
	UINT							SourceSizeZ;
	/** Full amount of miplevels in source texture													*/
	UINT							SourceNumMips;

	/** Used width of texture																		*/
	UINT							UsedSizeX;
	/** Used height of texture																		*/
	UINT							UsedSizeY;
	/** Used depth of texture																		*/
	UINT							UsedSizeZ;
	/** Used number of faces - can either be 1 or 6													*/
	UINT							UsedNumFaces;
	/** Used number of miplevels																	*/
	UINT							UsedNumMips;
	/** Used offset to first miplevel in base FTexture												*/
	UINT							UsedFirstMip;
	/** Used Unreal pixel format																	*/
	EPixelFormat					UsedFormat;

	/**
	 * Constructs a FD3DTexture in a resource set with the previously allocated resource index and
	 * also initializes all member variables.
	 *
	 * @param	InSet				resource set this resource is created in.
	 * @param	InResourceIndex		unique resource index this resource was allocated.
	 */
	FD3DTexture( FD3DResourceSet* InSet, INT InResourceIndex )
	:	FD3DResource(InSet), 
		ResourceIndex(InResourceIndex),
		GammaCorrected(0),
		RequiresPointFiltering(0),
		ClampU(0),
		ClampV(0),
		ClampW(0),
#ifdef XBOX
		IntermediateBaseAddress(NULL),
#endif
		SourceSizeX(0),
		SourceSizeY(0),
		SourceSizeZ(0),
		SourceNumMips(0),
		UsedSizeX(0),
		UsedSizeY(0),
		UsedSizeZ(0),
		UsedNumFaces(0),
		UsedNumMips(0),
		UsedFirstMip(0),
		UsedFormat(PF_Unknown)
	{}

	/**
	 *  Destructor, calls Cleanup.
	 */
	virtual ~FD3DTexture();

	/**
	 * Cleans up resources like e.g. new'ed D3D resources or physical memory allocated (BaseAddress)
	 * on platforms where we can't use Release() to clean up.
	 */
	virtual void Cleanup();

	/**
	 * Calculates and caches the width, height and all other resource settings based on the base texture
	 * being passed in.
	 *
	 * @param	Source	texture to cache
	 * @return	TRUE if resource needs to be recreated or FALSE if it is okay to only reupload the miplevels
	 */
	UBOOL CacheBaseInfo( FTextureBase* Source );

	/**
	 * Updates the texture (both info and data) based on the source template.
	 *
	 * @param	Source	texture used ot get mip dimensions, format and data from
	 */
	virtual void UpdateTexture( FTextureBase* Source );
	
	/**
	 * Creates a new mipchain after releasing the existing one and uploads the texture data
	 *
	 * @param	Source	texture used
	 */
	virtual void CreateMipChain( FTextureBase* Source ) = 0;

	/**
	 * Uploads a particular miplevel of the passed in texture.
	 *
	 * @param	Source		source texture used
	 * @param	MipLevel	index of miplevel to upload
	 */
	virtual void UploadMipLevel( FTextureBase* Source, UINT MipLevel ) = 0;

	/**
	 * Returns the size in bytes this resource currently occupies.	
	 *
	 * @return size in bytes this resource currently occupies
	 */
	UINT GetSize();

	/**
	 * Returns the maximum size in bytes this resource could potentially occupy. This value
	 * e.g. differs from GetSize for textures that don't have all miplevels streamed in.
	 *
	 * @return maximum size in bytes this resource could potentially occupy
	 */
	UINT GetMaxSize();

	/**
	 * Returns the size in bytes used by a specified miplevel regardless of whether
	 * it is present or not.
	 *
	 * @param MipLevel 0- based (largest) index
	 * @return size in bytes the specified milevel occupies
	 */
	UINT GetMipSize( UINT MipLevel );

	/**
	 * Console specific helper function required for memory image loading code. 
	 *
	 * See D3DResourceXenon.cpp for details.
	 */
	virtual void UploadToVRAM();

	/**
	 * Creates an intermediate texture with a specified amount of miplevels and copies
	 * over as many miplevels from the existing texture as possible.
	 *
	 * @param MipLevels Number of miplevels in newly created intermediate texture
	 */
	virtual void CopyToIntermediate( UINT MipLevels ) {}

	/**
	 * Performs a switcheroo of intermediate and used texture, releasing the previously used one.	
	 */
	virtual void Switcheroo() {}
	
	/**
	 * Finalizes a mip request. This gets called after all requested data has finished loading.	
	 *
	 * @param	TextureMipRequest	Mip request that is being finalized.
	 */
	virtual void FinalizeMipRequest( FTextureMipRequest* TextureMipRequest ) {}

	/**
	 * Request an increase or decrease in the amount of miplevels a texture uses.
	 *
	 * @param	Texture				texture to adjust
	 * @param	RequestedMips		miplevels the texture should have
	 * @return	NULL if a decrease is requested, pointer to information about request otherwise (NOTE: pointer gets freed in FinalizeMipRequest)
	 */
	virtual void RequestMips( UINT NewNumMips, FTextureMipRequest* TextureMipRequest ) {}
};

/** 
 * D3D Texture
 */
struct FD3DTexture2D: FD3DTexture
{
	/**
	 * Creates a new mipchain after releasing the existing one and uploads the texture data
	 *
	 * @param	Source	texture used
	 */
	virtual void CreateMipChain( FTextureBase* Source );

	/**
	 * Uploads a particular miplevel of the passed in texture.
	 *
	 * @param	Source		source texture used
	 * @param	MipLevel	index of miplevel to upload
	 */
	virtual void UploadMipLevel( FTextureBase* Source, UINT MipLevel );

	/**
	 * Constructor
	 *
	 * @param	InSet				resource set this resource is created in.
	 * @param	InSource			texture that is being cached
	 */
	FD3DTexture2D( FD3DResourceSet* InSet, FTexture2D* Source ) 
	:	FD3DTexture( InSet, Source->ResourceIndex )
	{ 
		UpdateTexture( Source ); 
	}
	
	// FD3DResource interface.
	
	/**
	 * Returns the resource type's type description.
	 *
	 * @return Resource's type description
	 */
	virtual FString GetTypeDescription() { return TEXT("Texture2D"); }

	/**
	 * Creates an intermediate texture with a specified amount of miplevels and copies
	 * over as many miplevels from the existing texture as possible.
	 *
	 * @param MipLevels Number of miplevels in newly created intermediate texture
	 */
	virtual void CopyToIntermediate( UINT MipLevels );

	/**
	 * Performs a switcheroo of intermediate and used texture, releasing the previously used one.	
	 */
	virtual void Switcheroo();

	/**
	 * Finalizes a mip request. This gets called after all requested data has finished loading.	
	 *
	 * @param	TextureMipRequest	Mip request that is being finalized.
	 */
	virtual void FinalizeMipRequest( FTextureMipRequest* TextureMipRequest );

	/**
	 * Request an increase or decrease in the amount of miplevels a texture uses.
	 *
	 * @param	Texture				texture to adjust
	 * @param	RequestedMips		miplevels the texture should have
	 * @param	TextureMipRequest	NULL if a decrease is requested, otherwise pointer to information 
	 *								about request that needs to be filled in.
	 */
	virtual void RequestMips( UINT NewNumMips, FTextureMipRequest* TextureMipRequest );
};

/** 
 * D3D Volume texture.
 */
struct FD3DTexture3D: FD3DTexture
{
	/**
	 * Creates a new mipchain after releasing the existing one and uploads the texture data
	 *
	 * @param	Source	texture used
	 */	
	virtual void CreateMipChain( FTextureBase* Source );

	/**
	 * Uploads a particular miplevel of the passed in texture.
	 *
	 * @param	Source		source texture used
	 * @param	MipLevel	index of miplevel to upload
	 */
	virtual void UploadMipLevel( FTextureBase* Source, UINT MipLevel );

	/**
	 * Constructor
	 *
	 * @param	InSet				resource set this resource is created in.
	 * @param	InSource			volume texture that is being cached
	 */
	FD3DTexture3D( FD3DResourceSet* InSet, FTexture3D* Source ) 
	:	FD3DTexture( InSet, Source->ResourceIndex )
	{ 
		UpdateTexture( Source ); 
	}
	
	// FD3DResource interface.

	/**
	 * Returns the resource type's type description.
	 *
	 * @return Resource's type description
	 */
	virtual FString GetTypeDescription() { return TEXT("Texture3D"); }
};

/**
 * D3D Cube map.
 */
struct FD3DTextureCube: FD3DTexture
{
	/**
	 * Creates a new mipchain after releasing the existing one and uploads the texture data
	 *
	 * @param	Source	texture used
	 */
	virtual void CreateMipChain( FTextureBase* Source );

	/**
	 * Uploads a particular miplevel of the passed in texture.
	 *
	 * @param	Source		source texture used
	 * @param	MipLevel	index of miplevel to upload
	 */
	virtual void UploadMipLevel( FTextureBase* Source, UINT MipLevel );

	/**
	 * Constructor
	 *
	 * @param	InSet				resource set this resource is created in.
	 * @param	InSource			cube map that is being cached
	 */
	FD3DTextureCube( FD3DResourceSet* InSet, FTextureCube* Source ) 
	:	FD3DTexture( InSet, Source->ResourceIndex ) 
	{ 
		UpdateTexture( Source ); 
	}
	
	// FD3DResource interface.

	/**
	 * Returns the resource type's type description.
	 *
	 * @return Resource's type description
	 */
	virtual FString GetTypeDescription() { return TEXT("TextureCube"); }
};

/**
 * D3D static ndex buffer	
 */
struct FD3DIndexBuffer: FD3DResource
{
	/** Unique resource index used for hash															*/
	INT								ResourceIndex;
	/** D3D index buffer resource																	*/
	TD3DRef<IDirect3DIndexBuffer9>	IndexBuffer;

	// Constructor/destructor.
	FD3DIndexBuffer(FD3DResourceSet* InSet,FIndexBuffer* SourceIndexBuffer): FD3DResource(InSet), ResourceIndex(SourceIndexBuffer->ResourceIndex) { Cache(SourceIndexBuffer); }
	~FD3DIndexBuffer();

	/**
	 * Cleans up resources like e.g. the D3D resource that might have
	 * been new'ed or physical memory allocated (BaseAddress).
	 */
	virtual void Cleanup();

	// FD3DIndexBuffer interface.
	void Cache(FIndexBuffer* SourceIndexBuffer);

	// FD3DResource interface.
	virtual FString GetTypeDescription() { return TEXT("IndexBuffer"); }
	virtual void UploadToVRAM();
};

//
//	FD3DDynamicIndexBuffer
//

struct FD3DDynamicIndexBuffer: FD3DResource
{
	TD3DRef<IDirect3DIndexBuffer9>	IndexBuffer;
	UINT							Stride,
									Offset;

	// Constructor/destructor.

	FD3DDynamicIndexBuffer(FD3DResourceSet* InSet,UINT InSize,UINT InStride);
	~FD3DDynamicIndexBuffer();

	/**
	 * Cleans up resources like e.g. new'ed D3D resources or physical memory allocated (BaseAddress)
	 * on platforms where we can't use Release() to clean up.
	 */
	virtual void Cleanup();

	// Set - Copies dynamic vertices into the buffer and sets the buffer to stream 0.

	UINT Set(FIndexBuffer* DynamicIndices);
	
	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("DynamicIndexBuffer"); }
	virtual void UploadToVRAM();
};

//
//	FD3DVertexBuffer
//

struct FD3DVertexBuffer: FD3DResource
{
	INT ResourceIndex;
	UINT Stride;

	// Cache

	void UpdateBuffer(FVertexBuffer* SourceVertexBuffer);

	// Constructor/destructor.

	FD3DVertexBuffer(FD3DResourceSet* InSet,FVertexBuffer* SourceVertexBuffer): FD3DResource(InSet), ResourceIndex(SourceVertexBuffer->ResourceIndex), Stride(0) { UpdateBuffer(SourceVertexBuffer); }
	~FD3DVertexBuffer();

	/**
	 * Cleans up resources like e.g. new'ed D3D resources or physical memory allocated (BaseAddress)
	 * on platforms where we can't use Release() to clean up.
	 */
	virtual void Cleanup();

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("VertexBuffer"); }
	virtual void UploadToVRAM();

	// Accessors.

	IDirect3DVertexBuffer9* GetVertexBuffer(FVertexBuffer* SourceVertexBuffer)
	{
		if(!Valid)
			UpdateBuffer(SourceVertexBuffer);
		return VertexBuffer;
	}

	void Invalidate()
	{
		Valid = 0;
	}

private:

	/** The D3D vertex buffer containing the cached vertices. */
	TD3DRef<IDirect3DVertexBuffer9>	VertexBuffer;

	/** True if this cached resource reflects the latest update to the source resource. */
	UBOOL Valid;
};

//
//	FD3DVertexFactory
//

struct FD3DVertexFactory: FD3DResource
{
	struct FD3DVertexStream
	{
		TD3DRef<IDirect3DVertexBuffer9> VertexBuffer;
		UINT Stride;
	};

	INT										ResourceIndex;
	TD3DRef<IDirect3DVertexDeclaration9>	VertexDeclaration;
	TArray<FD3DVertexStream>				VertexStreams;

	// Constructor/destructor.

	FD3DVertexFactory(FD3DResourceSet* InSet,INT InResourceIndex):
		FD3DResource(InSet),
		ResourceIndex(InResourceIndex)
	{}

    // CacheStreams - Links the vertex factory to cached versions of the vertex buffers required.

	void CacheStreams(const TArray<FVertexBuffer*>& SourceStreams);

	// Set - Sets the D3D device to render vertices from this factory.

	virtual UINT Set(FVertexFactory* VertexFactory);
};

//
//	FD3DLocalVertexFactory
//

struct FD3DLocalVertexFactory: FD3DVertexFactory
{
	// Cache

	void Cache(FLocalVertexFactory* SourceVertexFactory);

	// Constructor/destructor.

	FD3DLocalVertexFactory(FD3DResourceSet* InSet,FLocalVertexFactory* SourceVertexFactory):
		FD3DVertexFactory(InSet,SourceVertexFactory->ResourceIndex)
	{
		Cache(SourceVertexFactory);
	}
	
	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("LocalVertexFactory"); }
};

//
//	FD3DLocalShadowVertexFactory
//

struct FD3DLocalShadowVertexFactory: FD3DVertexFactory
{
	// Cache

	void Cache(FLocalShadowVertexFactory* SourceVertexFactory);

	// Constructor/destructor.

	FD3DLocalShadowVertexFactory(FD3DResourceSet* InSet,FLocalShadowVertexFactory* SourceVertexFactory):
		FD3DVertexFactory(InSet,SourceVertexFactory->ResourceIndex)
	{
		Cache(SourceVertexFactory);
	}
	
	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("LocalShadowVertexFactory"); }
};

//
//	FD3DTerrainVertexFactory
//

struct FD3DTerrainVertexFactory :  FD3DVertexFactory
{
	// Cache

	void Cache(FTerrainVertexFactory* SourceVertexFactory);

	// Constructor/destructor.

	FD3DTerrainVertexFactory(FD3DResourceSet* InSet,FTerrainVertexFactory* SourceVertexFactory):
		FD3DVertexFactory(InSet,SourceVertexFactory->ResourceIndex)
	{
		Cache(SourceVertexFactory);
	}
	
	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("TerrainVertexFactory"); }
};

//
//	FD3DFoliageVertexFactory
//

struct FD3DFoliageVertexFactory: FD3DVertexFactory
{
	// Cache

	void Cache(FFoliageVertexFactory* SourceVertexFactory);

	// Constructor/destructor.

	FD3DFoliageVertexFactory(FD3DResourceSet* InSet,FFoliageVertexFactory* SourceVertexFactory):
		FD3DVertexFactory(InSet,SourceVertexFactory->ResourceIndex)
	{
		Cache(SourceVertexFactory);
	}
	
	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("FoliageVertexFactory"); }
};

//
//	FD3DParticleVertexFactory
//

struct FD3DParticleVertexFactory :  FD3DVertexFactory
{
	// Cache

	void Cache(FParticleVertexFactory* SourceVertexFactory);

	// Constructor/destructor.

	FD3DParticleVertexFactory(FD3DResourceSet* InSet,FParticleVertexFactory* SourceVertexFactory):
		FD3DVertexFactory(InSet,SourceVertexFactory->ResourceIndex)
	{
		Cache(SourceVertexFactory);
	}
	
	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("ParticleVertexFactory"); }
};

//
//	FD3DParticleSubUVVertexFactory
//

struct FD3DParticleSubUVVertexFactory : FD3DVertexFactory
{
	// Cache

	void Cache(FParticleSubUVVertexFactory* SourceVertexFactory);

	// Constructor/destructor.

	FD3DParticleSubUVVertexFactory(FD3DResourceSet* InSet, FParticleSubUVVertexFactory* SourceVertexFactory):
		FD3DVertexFactory(InSet,SourceVertexFactory->ResourceIndex)
	{
		Cache(SourceVertexFactory);
	}
	
	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("FD3DParticleSubUVVertexFactory"); }
};

//
//	FD3DSceneViewState
//

struct FD3DSceneViewState : FD3DResource
{
	TD3DRef<IDirect3DTexture9>	ExposureTextures[2];
	UINT						LastExposureTexture;
	DOUBLE						LastExposureTime;
	INT							ResourceIndex;

	TMap<UPrimitiveComponent*,UBOOL> UnoccludedPrimitives;

	// Cache

	void Cache(FSceneViewState* SourceState);

	// Constructor/destructor.

	FD3DSceneViewState(FD3DResourceSet* InSet,FSceneViewState* SourceState):
		FD3DResource(InSet),
		ResourceIndex(SourceState->ResourceIndex),
		LastExposureTime(appSeconds()),
		LastExposureTexture(0)
	{
		Cache(SourceState);
	}

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("SceneViewState"); }
};

//
//	FD3DResourceCache
//

struct FD3DResourceCache: TD3DResourceCache<INT,FD3DResource>
{
	// Constructor.

	FD3DResourceCache(UD3DRenderDevice* InRenderDevice):
		TD3DResourceCache<INT,FD3DResource>(InRenderDevice)
	{}

	// GetCachedResource

	FD3DResource* GetCachedResource(FResource* Source);

	// FD3DResourceSet interface.

	virtual void FreeResource(FResource* Resource)
	{
		FlushResourceId(Resource->ResourceIndex);
	}
};

//
//	FD3DRenderTarget
//

struct FD3DRenderTarget
{
private:
	// Variables.
	TD3DRef<IDirect3DTexture9> RenderTexture;
	TD3DRef<IDirect3DSurface9> RenderSurface;

public:
	// Constructor/ Destructor.
	FD3DRenderTarget( UINT Width, UINT Height, D3DFORMAT Format, const TCHAR* Usage );
	virtual ~FD3DRenderTarget();

	// FD3DRenderTarget interface.
	const TD3DRef<IDirect3DTexture9>& GetTexture() const;
	const TD3DRef<IDirect3DSurface9>& GetSurface() const;
	void Resolve();
};

//
//	FD3DDepthStencilSurface
//

struct FD3DDepthStencilSurface
{
private:
	// Variables.
	TD3DRef<IDirect3DSurface9> DepthSurface;

public:
	// Constructor/ Destructor.
	FD3DDepthStencilSurface( UINT Width, UINT Height, D3DFORMAT Format, const TCHAR* Usage );
	FD3DDepthStencilSurface( TD3DRef<IDirect3DSurface9>& InDepthSurface );
	
	virtual ~FD3DDepthStencilSurface();

	// FD3DRenderTarget interface.
	const TD3DRef<IDirect3DSurface9>& GetSurface() const;
};