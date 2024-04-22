/*=============================================================================
	UnTex.h: Unreal texture related classes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/** Thread-safe counter indicating the texture streaming state. The definitions below are mirrored in Texture2D.uc */
enum ETextureStreamingState
{
	// The renderer hasn't created the resource yet.
	TexState_InProgress_Initialization	= -1,
	// There are no pending requests/ all requests have been fulfilled.
	TexState_ReadyFor_Requests			= 0,
	// Finalization has been kicked off and is in progress.
	TexState_InProgress_Finalization	= 1,
	// Initial request has completed and finalization needs to be kicked off.
	TexState_ReadyFor_Finalization		= 2,
	// We're currently loading in mip data.
	TexState_InProgress_Loading			= 3,
	// ...
	// States 2+N means we're currently loading in N mips
	// ...
	// Memory has been allocated and we're ready to start loading in mips.
	TexState_ReadyFor_Loading			= 100,
	// We're currently allocating/preparing memory for the new mip count.
	TexState_InProgress_Allocation		= 101,
	// The RHI is asynchronously allocating/preparing memory for the new mip count.
	TexState_InProgress_AsyncAllocation = 102
};

/** 
 * The rendering resource which represents a texture.
 */
class FTextureResource : public FTexture
{
public:

	FRenderCommandFence ReleaseFence;

	FTextureResource()
	{}
	virtual ~FTextureResource() {}
};

/**
 * FTextureResource implementation for streamable 2D textures.
 */
class FTexture2DResource : public FTextureResource
{
public:
	/**
	 * Minimal initialization constructor.
	 *
	 * @param InOwner			UTexture2D which this FTexture2DResource represents.
	 * @param InitialMipCount	Initial number of miplevels to upload to card
	 * @param InFilename		Filename to read data from
 	 */
	FTexture2DResource( UTexture2D* InOwner, INT InitialMipCount, const FString& InFilename );

	/**
	 * Destructor, freeing MipData in the case of resource being destroyed without ever 
	 * having been initialized by the rendering thread via InitRHI.
	 */
	virtual ~FTexture2DResource();

	// FRenderResource interface.

	/**
	 * Called when the resource is initialized. This is only called by the rendering thread.
	 */
	virtual void InitRHI();
	/**
	 * Called when the resource is released. This is only called by the rendering thread.
	 */
	virtual void ReleaseRHI();

	/** Returns the width of the texture in pixels. */
	virtual UINT GetSizeX() const;

	/** Returns the height of the texture in pixels. */
	virtual UINT GetSizeY() const;

	/**
	 * Called from the game thread to kick off a change in ResidentMips after modifying RequestedMips.
	 * @param bShouldPrioritizeAsyncIORequest	- Whether the Async I/O request should have higher priority
	 */
	void BeginUpdateMipCount( UBOOL bShouldPrioritizeAsyncIORequest );
	/**
	 * Called from the game thread to kick off async I/O to load in new mips.
	 */
	void BeginLoadMipData();
	/**
	 * Called from the game thread to kick off finalization of mip change.
	 */
	void BeginFinalizeMipCount();
	/**
	 * Called from the game thread to kick off cancelation of async operations for request.
	 */
	void BeginCancelUpdate();

	/** 
	 * Accessor
	 * @return Texture2DRHI
	 */
	FTexture2DRHIRef GetTexture2DRHI()
	{
		return Texture2DRHI;
	}

	UBOOL DidUpdateMipCountFail() const
	{
		return NumFailedReallocs > 0;
	}

	/**
	 *	Tries to reallocate the texture for a new mip count.
	 *	@param OldMipCount	- The old mip count we're currently using.
	 *	@param NewMipCount	- The new mip count to use.
	 */
	UBOOL TryReallocate( INT OldMipCount, INT NewMipCount );

	virtual FString GetFriendlyName() const;

	UBOOL IsBeingReallocated() const
	{
		return bUsingInPlaceRealloc && IsValidRef(IntermediateTextureRHI);
	}

	//Returns the raw data for a particular mip level
	void* GetRawMipData( UINT MipIndex);

private:
	/** Texture streaming command classes that need to be friends in order to call Update/FinalizeMipCount.	*/
	friend class FUpdateMipCountCommand;
	friend class FFinalinzeMipCountCommand;
	friend class FCancelUpdateCommand;

	/** The UTexture2D which this resource represents.														*/
	const UTexture2D*	Owner;
	/** Resource memory allocated by the owner for serialize bulk mip data into								*/
	FTexture2DResourceMem* ResourceMem;
	
	/** First miplevel used.																				*/
	INT					FirstMip;
	/** Cached filename.																					*/
	FString				Filename;

	/** Local copy/ cache of mip data between creation and first call to InitRHI.							*/
	void*				MipData[MAX_TEXTURE_MIP_COUNT];
	/** Potentially outstanding texture I/O requests.														*/
	QWORD				IORequestIndices[MAX_TEXTURE_MIP_COUNT];
	/** Number of file I/O requests for current request														*/
	INT					IORequestCount;

	/** 2D texture version of TextureRHI which is used to lock the 2D texture during mip transitions.		*/
	FTexture2DRHIRef	Texture2DRHI;
	/** Intermediate texture used to fulfill mip change requests. Swapped in FinalizeMipCount.				*/
	FTexture2DRHIRef	IntermediateTextureRHI;
	/** Whether IntermediateTextureRHI is pointing to the same memory.										*/
	BITFIELD			bUsingInPlaceRealloc:1;
	/** Whether the current stream request is prioritized higher than normal.	*/
	BITFIELD			bPrioritizedIORequest:1;
	/** Number of times UpdateMipCount has failed to reallocate memory.										*/
	INT					NumFailedReallocs;

#if STATS
	/** Cached texture size for stats.																		*/
	INT					TextureSize;
	/** Cached intermediate texture size for stats.															*/
	INT					IntermediateTextureSize;
#if _WINDOWS	// The TextureMemory stat will be what is used on Xbox...
	/** Cached texture size on 360 for stats. */
	INT					TextureSize_360;
	/** Cached intermediate texture size on 360 for stats.															*/
	INT					IntermediateTextureSize_360;
#endif
#endif

	/**
	 * Writes the data for a single mip-level into a destination buffer.
	 * @param MipIndex	The index of the mip-level to read.
	 * @param Dest		The address of the destination buffer to receive the mip-level's data.
	 * @param DestPitch	Number of bytes per row
	 */
	void GetData( UINT MipIndex,void* Dest,UINT DestPitch );

	/**
	 * Called from the rendering thread to perform the work to kick off a change in ResidentMips.
	 */
	void UpdateMipCount();
	/**
	 * Called from the rendering thread to start async I/O to load in new mips.
	 */
	void LoadMipData();
	/**
	 * Called from the rendering thread to finalize a mip change.
	 */
	void FinalizeMipCount();
	/**
	 * Called from the rendering thread to cancel async operations for request.
	 */
	void CancelUpdate();
};

/** A dynamic 2D texture resource. */
class FTexture2DDynamicResource : public FTextureResource
{
public:
	/** Initialization constructor. */
	FTexture2DDynamicResource(class UTexture2DDynamic* InOwner);

	/** Returns the width of the texture in pixels. */
	virtual UINT GetSizeX() const;

	/** Returns the height of the texture in pixels. */
	virtual UINT GetSizeY() const;

	/** Called when the resource is initialized. This is only called by the rendering thread. */
	virtual void InitRHI();

	/** Called when the resource is released. This is only called by the rendering thread. */
	virtual void ReleaseRHI();

	/** Returns the Texture2DRHI, which can be used for locking/unlocking the mips. */
	FTexture2DRHIRef GetTexture2DRHI();

private:
	/** The owner of this resource. */
	class UTexture2DDynamic* Owner;
	/** Texture2D reference, used for locking/unlocking the mips. */
	FTexture2DRHIRef Texture2DRHI;
};

/** Stores information about a mip map, used by FTexture2DArrayResource to mirror game thread data. */
class FMipMapDataEntry
{
public:
	UINT SizeX;
	UINT SizeY;
	TArray<BYTE> Data;
};

/** Stores information about a single texture in FTexture2DArrayResource. */
class FTextureArrayDataEntry
{
public:

	FTextureArrayDataEntry() : 
		NumRefs(0)
	{}

	/** Number of FTexture2DArrayResource::AddTexture2D calls that specified this texture. */
	INT NumRefs;

	/** Mip maps of the texture. */
	TArray<FMipMapDataEntry, TInlineAllocator<MAX_TEXTURE_MIP_COUNT> > MipData;
};

/** 
 * Stores information about a UTexture2D so the rendering thread can access it, 
 * Even though the UTexture2D may have changed by the time the rendering thread gets around to it.
 */
class FIncomingTextureArrayDataEntry : public FTextureArrayDataEntry
{
public:

	FIncomingTextureArrayDataEntry() {}

	FIncomingTextureArrayDataEntry(UTexture2D* InTexture);

	INT SizeX;
	INT SizeY;
	INT NumMips;
	TextureGroup LODGroup;
	EPixelFormat Format;
	ESamplerFilter Filter;
	UBOOL bSRGB;
};

/** Represents a 2D Texture Array to the renderer. */
class FTexture2DArrayResource : public FTextureResource
{
public:

	FTexture2DArrayResource() :
		SizeX(0),
		bDirty(FALSE),
		bPreventingReallocation(FALSE)
	{}

	// Rendering thread functions

	/** 
	 * Adds a texture to the texture array.  
	 * This is called on the rendering thread, so it must not dereference NewTexture.
	 */
	void AddTexture2D(UTexture2D* NewTexture, const FIncomingTextureArrayDataEntry* InEntry);

	/** Removes a texture from the texture array, and potentially removes the CachedData entry if the last ref was removed. */
	void RemoveTexture2D(const UTexture2D* NewTexture);

	/** Updates a CachedData entry (if one exists for this texture), with a new texture. */
	void UpdateTexture2D(UTexture2D* NewTexture, const FIncomingTextureArrayDataEntry* InEntry);

	/** Initializes the texture array resource if needed, and re-initializes if the texture array has been made dirty since the last init. */
	void UpdateResource();

	/** Returns the index of a given texture in the texture array. */
	INT GetTextureIndex(const UTexture2D* Texture) const;
	INT GetNumValidTextures() const;

	/**
	* Called when the resource is initialized. This is only called by the rendering thread.
	*/
	virtual void InitRHI();

	/** Returns the width of the texture in pixels. */
	virtual UINT GetSizeX() const
	{
		return SizeX;
	}

	/** Returns the height of the texture in pixels. */
	virtual UINT GetSizeY() const
	{
		return SizeY;
	}

	/** Prevents reallocation from removals of the texture array until EndPreventReallocation is called. */
	void BeginPreventReallocation();

	/** Restores the ability to reallocate the texture array. */
	void EndPreventReallocation();

private:

	/** Texture data, has to persist past the first InitRHI call, because more textures may be added later. */
	TMap<const UTexture2D*, FTextureArrayDataEntry> CachedData;
	UINT SizeX;
	UINT SizeY;
	UINT NumMips;
	BYTE LODGroup;
	EPixelFormat Format;
	ESamplerFilter Filter;

	UBOOL bSRGB;
	UBOOL bDirty;
	UBOOL bPreventingReallocation;

	/** Copies data from DataEntry into Dest, taking stride into account. */
	void GetData(const FTextureArrayDataEntry& DataEntry, INT MipIndex, void* Dest, UINT DestPitch);
};

/** 3D texture resource */
class FVolumeTextureResource : public FTextureResource
{
public:

	FVolumeTextureResource() :
		Data(NULL)
	{}

	virtual void InitRHI()
	{
#if PLATFORM_SUPPORTS_D3D10_PLUS
		check(Data);
		const DWORD TexCreateFlags = bSRGB ? TexCreate_SRGB : 0;
		FTexture3DRHIRef Texture3D = RHICreateTexture3D(SizeX, SizeY, SizeZ, Format, 1, TexCreateFlags, Data);
		TextureRHI = Texture3D;
#endif
	}

	EPixelFormat Format;
	UBOOL bSRGB;

	INT SizeX;
	INT SizeY;
	INT SizeZ;
	const BYTE* Data;
};

/**
 * FDeferredUpdateResource for resources that need to be updated after scene rendering has begun
 * (should only be used on the rendering thread)
 */
class FDeferredUpdateResource
{
public:
	/**
	 * Constructor, initializing UpdateListLink.
	 */
	FDeferredUpdateResource()
		:	UpdateListLink(NULL)
		,	bOnlyUpdateOnce(FALSE)
	{}

	/**
	 * Iterate over the global list of resources that need to
	 * be updated and call UpdateResource on each one.
	 */
	static void UpdateResources();

	/** 
	 * This is reset after all viewports have been rendered
	 */
	static void ResetNeedsUpdate()
	{
		bNeedsUpdate = TRUE;
	}

	// FDeferredUpdateResource interface

	/**
	 * Updates the resource
	 */
	virtual void UpdateResource() = 0;

protected:

	/**
	 * Add this resource to deferred update list
	 * @param OnlyUpdateOnce - flag this resource for a single update if TRUE
	 */
	void AddToDeferredUpdateList( UBOOL OnlyUpdateOnce=FALSE );

	/**
	 * Remove this resource from deferred update list
	 */
	void RemoveFromDeferredUpdateList();

private:
	/** 
	 * Resources can be added to this list if they need a deferred update during scene rendering.
	 * @return global list of resource that need to be updated. 
	 */
	static TLinkedList<FDeferredUpdateResource*>*& GetUpdateList();
	/** This resource's link in the global list of resources needing clears. */
	TLinkedList<FDeferredUpdateResource*> UpdateListLink;
	/** if TRUE then UpdateResources needs to be called */
	static UBOOL bNeedsUpdate;
	/** if TRUE then remove this resource from the update list after a single update */
	UBOOL bOnlyUpdateOnce;
};

/**
 * FTextureResource type for render target textures.
 */
class FTextureRenderTargetResource : public FTextureResource, public FRenderTarget, public FDeferredUpdateResource
{
public:
	/**
	 * Constructor, initializing ClearLink.
	 */
	FTextureRenderTargetResource()
	{}

	/** 
	 * Return true if a render target of the given format is allowed
	 * for creation
	 */
	static UBOOL IsSupportedFormat( EPixelFormat Format );

	// FTextureRenderTargetResource interface
	
	virtual class FTextureRenderTarget2DResource* GetTextureRenderTarget2DResource()
	{
		return NULL;
	}
	virtual class FTextureRenderTargetCubeResource* GetTextureRenderTargetCubeResource()
	{
		return NULL;
	}
	virtual void ClampSize(INT SizeX,INT SizeY) {}

	// FRenderTarget interface.
	virtual UINT GetSizeX() const = 0;
	virtual UINT GetSizeY() const = 0;

	/** 
	 * Render target resource should be sampled in linear color space
	 *
	 * @return display gamma expected for rendering to this render target 
	 */
	virtual FLOAT GetDisplayGamma() const;
};

/**
 * FTextureResource type for 2D render target textures.
 */
class FTextureRenderTarget2DResource : public FTextureRenderTargetResource
{
public:
	
	/** 
	 * Constructor
	 * @param InOwner - 2d texture object to create a resource for
	 */
	FTextureRenderTarget2DResource(const class UTextureRenderTarget2D* InOwner);

	FORCEINLINE FLinearColor GetClearColor()
	{
		return ClearColor;
	}

	// FTextureRenderTargetResource interface

	/** 
	 * 2D texture RT resource interface 
	 */
	virtual class FTextureRenderTarget2DResource* GetTextureRenderTarget2DResource()
	{
		return this;
	}

	/**
	 * Clamp size of the render target resource to max values
	 *
	 * @param MaxSizeX max allowed width
	 * @param MaxSizeY max allowed height
	 */
	virtual void ClampSize(INT SizeX,INT SizeY);
	
	// FRenderResource interface.

	/**
	 * Initializes the dynamic RHI resource and/or RHI render target used by this resource.
	 * Called when the resource is initialized, or when reseting all RHI resources.
	 * Resources that need to initialize after a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void InitDynamicRHI();

	/**
	 * Releases the dynamic RHI resource and/or RHI render target resources used by this resource.
	 * Called when the resource is released, or when reseting all RHI resources.
	 * Resources that need to release before a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseDynamicRHI();

	// FDeferredClearResource interface

	/**
	 * Clear contents of the render target
	 */
	virtual void UpdateResource();

	// FRenderTarget interface.

	virtual UINT GetSizeX() const;
	virtual UINT GetSizeY() const;	

	/** 
	 * Render target resource should be sampled in linear color space
	 *
	 * @return display gamma expected for rendering to this render target 
	 */
	virtual FLOAT GetDisplayGamma() const;

	/** 
	 * @return TextureRHI for rendering 
	 */
	FTexture2DRHIRef GetTextureRHI() { return Texture2DRHI; }

private:
	/** The UTextureRenderTarget2D which this resource represents. */
	const class UTextureRenderTarget2D* Owner;
    /** Texture resource used for rendering with and resolving to */
    FTexture2DRHIRef Texture2DRHI;
	/** the color the texture is cleared to */
	FLinearColor ClearColor;
	INT TargetSizeX,TargetSizeY;
};

/**
 * FTextureResource type for cube render target textures.
 */
class FTextureRenderTargetCubeResource : public FTextureRenderTargetResource
{
public:

	/** 
	 * Constructor
	 * @param InOwner - cube texture object to create a resource for
	 */
	FTextureRenderTargetCubeResource(const class UTextureRenderTargetCube* InOwner)
		:	Owner(InOwner)
	{
	}

	/**
	 * We can only render to one face as a time. So, set the current 
	 * face which will be used as the render target surface.
	 * @param Face - face to use as current target face
	 */
	void SetCurrentTargetFace(ECubeFace Face);

	// FTextureRenderTargetResource interface

	/** 
	 * Cube texture RT resource interface 
	 */
	virtual class FTextureRenderTargetCubeResource* GetTextureRenderTargetCubeResource()
	{
		return this;
	}

	// FRenderResource interface.

	/**
	 * Initializes the dynamic RHI resource and/or RHI render target used by this resource.
	 * Called when the resource is initialized, or when reseting all RHI resources.
	 * Resources that need to initialize after a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void InitDynamicRHI();

	/**
	 * Releases the dynamic RHI resource and/or RHI render target resources used by this resource.
	 * Called when the resource is released, or when reseting all RHI resources.
	 * Resources that need to release before a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseDynamicRHI();	

	// FDeferredClearResource interface

	/**
	 * Clear contents of the render target. Clears each face of the cube
	 * This is only called by the rendering thread.
	 */
	virtual void UpdateResource();

	// FRenderTarget interface.

	/**
	 * @return width of the target
	 */
	virtual UINT GetSizeX() const;
	/**
	 * @return height of the target
	 */
	virtual UINT GetSizeY() const;

private:
	/** The UTextureRenderTargetCube which this resource represents. */
	const class UTextureRenderTargetCube* Owner;
	/** Texture resource used for rendering with and resolving to */
	FTextureCubeRHIRef TextureCubeRHI;
	/** Target surfaces for each cube face */
	FSurfaceRHIRef CubeFaceSurfacesRHI[CubeFace_MAX];
	/** Face currently used for target surface */
	ECubeFace CurrentTargetFace;
};

/**
 * FTextureResource type for movie textures.
 */
class FTextureMovieResource : public FTextureResource, public FRenderTarget, public FDeferredUpdateResource
{
public:

	/** 
	 * Constructor
	 * @param InOwner - movie texture object to create a resource for
	 */
	FTextureMovieResource(const class UTextureMovie* InOwner)
		:	Owner(InOwner)
	{
	}

	// FRenderResource interface.

	/**
	 * Initializes the dynamic RHI resource and/or RHI render target used by this resource.
	 * Called when the resource is initialized, or when reseting all RHI resources.
	 * Resources that need to initialize after a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void InitDynamicRHI();

	/**
	 * Releases the dynamic RHI resource and/or RHI render target resources used by this resource.
	 * Called when the resource is released, or when reseting all RHI resources.
	 * Resources that need to release before a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseDynamicRHI();	

	// FDeferredClearResource interface

	/**
	 * Decodes the next frame of the movie stream and renders the result to this movie texture target
	 */
	virtual void UpdateResource();

	// FRenderTarget interface.

	virtual UINT GetSizeX() const;
	virtual UINT GetSizeY() const;

private:
	/** The UTextureRenderTarget2D which this resource represents. */
	const class UTextureMovie* Owner;
	/** Texture resource used for rendering with and resolving to */
	FTexture2DRHIRef Texture2DRHI;
};

/**
 * Structure containing all information related to an LOD group and providing helper functions to calculate
 * the LOD bias of a given group.
 */
struct FTextureLODSettings
{
	/**
	 * Initializes LOD settings by reading them from the passed in filename/ section.
	 *
	 * @param	IniFilename		Filename of ini to read from.
	 * @param	IniSection		Section in ini to look for settings
	 */
	void Initialize( const TCHAR* IniFilename, const TCHAR* IniSection );

	/**
	 * Calculates and returns the LOD bias based on texture LOD group, LOD bias and maximum size.
	 *
	 * @param	Texture		Texture object to calculate LOD bias for.
	 * @return	LOD bias
	 */
	INT CalculateLODBias( UTexture* Texture ) const;

	/** 
	* Useful for stats in the editor.
	*
	* @param LODBias			Default LOD at which the texture renders. Platform dependent, call FTextureLODSettings::CalculateLODBias(Texture)
	*/
	void ComputeInGameMaxResolution(INT LODBias, UTexture& Texture, UINT& OutSizeX, UINT& OutSizeY) const;

	void GetMipGenSettings(UTexture& Texture, FLOAT& OutSharpen, UINT& OutKernelSize, UBOOL& bOutDownsampleWithAverage, UBOOL& bOutSharpenWithoutColorShift, UBOOL &bOutBorderColorBlack) const;

	/**
	 * Will return the LODBias for a passed in LODGroup
	 *
	 * @param	InLODGroup		The LOD Group ID 
	 * @return	LODBias
	 */
	INT GetTextureLODGroupLODBias( INT InLODGroup ) const;

	/**
	 * Returns the LODGroup setting for number of streaming mip-levels.
	 * -1 means that all mip-levels are allowed to stream.
	 *
	 * @param	InLODGroup		The LOD Group ID 
	 * @return	Number of streaming mip-levels for textures in the specified LODGroup
	 */
	INT GetNumStreamedMips( INT InLODGroup ) const;

	/**
	 * Returns the filter state that should be used for the passed in texture, taking
	 * into account other system settings.
	 *
	 * @param	Texture		Texture to retrieve filter state for
	 * @return	Filter sampler state for passed in texture
	 */
	ESamplerFilter GetSamplerFilter( const UTexture* Texture ) const;

	/**
	 * Returns the texture group names, sorted like enum.
	 *
	 * @return array of texture group names
	 */
	static TArray<FString> GetTextureGroupNames();

	/** LOD settings for a single texture group. */
	struct FTextureLODGroup 
	{
		FTextureLODGroup()
		:	MinLODMipCount(0)
		,	MaxLODMipCount(12)
		,	LODBias(0) 
		,	Filter(SF_AnisotropicPoint)
		,	NumStreamedMips(-1)
		,	MipGenSettings(TMGS_SimpleAverage)
		{}
		/** Minimum LOD mip count below which the code won't bias.						*/
		INT MinLODMipCount;
		/** Maximum LOD mip count. Bias will be adjusted so texture won't go above.		*/
		INT MaxLODMipCount;
		/** Group LOD bias.																*/
		INT LODBias;
		/** Sampler filter state.														*/
		ESamplerFilter Filter;
		/** Number of mip-levels that can be streamed. -1 means all mips can stream.	*/
		INT NumStreamedMips;
		/** Defines how the the mip-map generation works, e.g. sharpening				*/
		TextureMipGenSettings MipGenSettings;
	};

protected:
	/**
	 * Reads a single entry and parses it into the group array.
	 *
	 * @param	GroupId			Id/ enum of group to parse
	 * @param	GroupName		Name of group to look for in ini
	 * @param	IniFilename		Filename of ini to read from.
	 * @param	IniSection		Section in ini to look for settings
	 */
	void ReadEntry( INT GroupId, const TCHAR* GroupName, const TCHAR* IniFilename, const TCHAR* IniSection );

	/**
	 * TextureLODGroups access with bounds check
	 *
	 * @param   GroupIndex      usually from Texture.LODGroup
	 * @return                  A handle to the indexed LOD group. 
	 */
	const FTextureLODGroup& GetTextureLODGroup(TextureGroup GroupIndex) const;

	/** Array of LOD settings with entries per group. */
	FTextureLODGroup TextureLODGroups[TEXTUREGROUP_MAX];
};








