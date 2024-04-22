/*=============================================================================
	RenderResource.h: Render resource definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * A rendering resource which is owned by the rendering thread.
 */
class FRenderResource
{
public:

	/**
	 * @return The global initialized resource list.
	 */
	static TLinkedList<FRenderResource*>*& GetResourceList();

	/**
	 * Minimal initialization constructor.
	 */
	FRenderResource():
		bInitialized(FALSE)
	{}

	/**
	 * Destructor used to catch unreleased resources.
	 */
	virtual ~FRenderResource();

	/**
	 * Initializes the dynamic RHI resource and/or RHI render target used by this resource.
	 * Called when the resource is initialized, or when reseting all RHI resources.
	 * Resources that need to initialize after a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void InitDynamicRHI() {}

	/**
	 * Releases the dynamic RHI resource and/or RHI render target resources used by this resource.
	 * Called when the resource is released, or when reseting all RHI resources.
	 * Resources that need to release before a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseDynamicRHI() {}

	/**
	 * Initializes the RHI resources used by this resource.
	 * Called when the resource is initialized.
	 * This is only called by the rendering thread.
	 */
	virtual void InitRHI() {}

	/**
	 * Releases the RHI resources used by this resource.
	 * Called when the resource is released.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseRHI() {}

	/**
	 * Initializes the resource.
	 * This is only called by the rendering thread.
	 */
	virtual void InitResource();

	/**
	 * Prepares the resource for deletion.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseResource();

	/**
	 * If the resource's RHI has been initialized, then release and reinitialize it.  Otherwise, do nothing.
	 * This is only called by the rendering thread.
	 */
	void UpdateRHI();

	/**
	 * @return The resource's friendly name.  Typically a UObject name.
	 */
	virtual FString GetFriendlyName() const { return TEXT("undefined"); }

	// Assignment operator.
	void operator=(const FRenderResource& OtherResource) {}

	// Accessors.
	FORCEINLINE UBOOL IsInitialized() const { return bInitialized; }

	virtual UBOOL IsGlobal() const { return FALSE; };

protected:

	/** This resource's link in the global resource list. */
	TLinkedList<FRenderResource*> ResourceLink;

	/** True if the resource has been initialized. */
	BITFIELD bInitialized : 1;
};

/**
 * Sends a message to the rendering thread to initialize a resource.
 * This is called in the game thread.
 */
extern void BeginInitResource(FRenderResource* Resource);

/**
 * Sends a message to the rendering thread to update a resource.
 * This is called in the game thread.
 */
extern void BeginUpdateResourceRHI(FRenderResource* Resource);

/**
 * Sends a message to the rendering thread to release a resource.
 * This is called in the game thread.
 */
extern void BeginReleaseResource(FRenderResource* Resource);

/**
 * Sends a message to the rendering thread to release a resource, and spins until the rendering thread has processed the message.
 * This is called in the game thread.
 */
extern void ReleaseResourceAndFlush(FRenderResource* Resource);

/**
 * A texture.
 */
class FTexture : public FRenderResource
{
public:
	/** The texture's RHI resource. */
	FTextureRHIRef		TextureRHI;

	/** The sampler state to use for the texture. */
	FSamplerStateRHIRef SamplerStateRHI;

	/** The last time the texture has been bound */
	mutable DOUBLE		LastRenderTime;

	/** Base values for fading in/out mip-levels. */
	FMipBiasFade		MipBiasFade;

	/** TRUE if the texture is in a greyscale texture format. */
	UBOOL				bGreyScaleFormat;

	/**
	 * TRUE if the texture is in the same gamma space as the intended rendertarget (e.g. screenshots).
	 * The texture will have sRGB==FALSE and bIgnoreGammaConversions==TRUE, causing a non-sRGB texture lookup
	 * and no gamma-correction in the shader.
	 */
	UBOOL				bIgnoreGammaConversions;

	/**
	 * Default constructor, initializing last render time.
	 */
	FTexture()
	: TextureRHI(NULL)
    , SamplerStateRHI(NULL)
	, LastRenderTime(-FLT_MAX)
	, bGreyScaleFormat(FALSE)
	, bIgnoreGammaConversions(FALSE)
	{}

	// Destructor
	virtual ~FTexture() {}

	// FRenderResource interface.
	virtual void ReleaseRHI()
	{
		TextureRHI.SafeRelease();
		SamplerStateRHI.SafeRelease();
	}
	/** Returns the width of the texture in pixels. */
	virtual UINT GetSizeX() const
	{
		return 0;
	}
	/** Returns the height of the texture in pixels. */
	virtual UINT GetSizeY() const
	{
		return 0;
	}

	/**
	* @return The resource's friendly name.
	*/
	virtual FString GetFriendlyName() const { return TEXT("FTexture"); }
};

/**
 * A vertex buffer.
 */
class FVertexBuffer : public FRenderResource
{
public:
	FVertexBufferRHIRef VertexBufferRHI;

	/** Destructor. */
	virtual ~FVertexBuffer() {}

	// FRenderResource interface.
	virtual void ReleaseRHI()
	{
		VertexBufferRHI.SafeRelease();
	}

	/**
	* @return The resource's friendly name.
	*/
	virtual FString GetFriendlyName() const { return TEXT("FVertexBuffer"); }
};

/**
* A vertex buffer with a single color component.  This is used on meshes that don't have a color component
* to keep from needing a separate vertex factory to handle this case.
*/
class FNullColorVertexBuffer : public FVertexBuffer
{
public:
	/** 
	* Initialize the RHI for this rendering resource 
	*/
	virtual void InitRHI()
	{
		// create a static vertex buffer
		VertexBufferRHI = RHICreateVertexBuffer(sizeof(DWORD), NULL, RUF_Static);
		DWORD* Vertices = (DWORD*)RHILockVertexBuffer(VertexBufferRHI, 0, sizeof(DWORD),FALSE);
		Vertices[0] = FColor(255, 255, 255, 255).DWColor();
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}
};

/** The global null color vertex buffer, which is set with a stride of 0 on meshes without a color component. */
extern TGlobalResource<FNullColorVertexBuffer> GNullColorVertexBuffer;

/**
* A vertex buffer with a single shadow value.
*/
class FNullShadowmapVertexBuffer : public FVertexBuffer
{
public:
	/** 
	* Initialize the RHI for this rendering resource 
	*/
	virtual void InitRHI()
	{
		// create a static vertex buffer
		VertexBufferRHI = RHICreateVertexBuffer(sizeof(FLOAT), NULL, RUF_Static);
		FLOAT* Vertices = (FLOAT*)RHILockVertexBuffer(VertexBufferRHI, 0, sizeof(FLOAT),FALSE);
		Vertices[0] = 1.0f;
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}
};

/** The global null shadowmap vertex buffer, which is set with a stride of 0 when needed. */
extern TGlobalResource<FNullShadowmapVertexBuffer> GNullShadowmapVertexBuffer;

/**
 * An index buffer.
 */
class FIndexBuffer : public FRenderResource
{
public:
	FIndexBufferRHIRef IndexBufferRHI;

	/** Destructor. */
	virtual ~FIndexBuffer() {}

	// FRenderResource interface.
	virtual void ReleaseRHI()
	{
		IndexBufferRHI.SafeRelease();
	}

	/**
	* @return The resource's friendly name.
	*/
	virtual FString GetFriendlyName() const { return TEXT("FIndexBuffer"); }
};
