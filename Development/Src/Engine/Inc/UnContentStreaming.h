/*=============================================================================
	UnContentStreaming.h: Definitions of classes used for content streaming.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

// Forward declarations.
struct FTextureBase;
struct FSceneContext;
struct FResource;
class ULocalPlayer;
class UPrimitiveComponent;
class UTexture;

/**
 * Structure containing all information needed for determining the screen space
 * size of an object.
 */
struct FStreamableTextureInstance
{
	/** Bounding sphere/ box of object */
	FSphere BoundingSphere;
	/** Object (and bounding sphere) specfic texel scale factor  */
	FLOAT	TexelFactor;

	/**
	 * FStreamTextureInstance serialize operator.
	 *
	 * @param	Ar					Archive to to serialize object to/ from
	 * @param	TextureInstance		Object to serialize
	 * @return	Returns the archive passed in
	 */
	friend FArchive& operator<<( FArchive& Ar, FStreamableTextureInstance& TextureInstance );
};

/**
 * Structure containing all information needed for determining the screen space
 * size of a texture. Note that a texture can be applied to multiple objects.
 */
struct FStreamableTextureInfo
{
	/** Associated UTexture object */
	UTexture*							Texture;
	/** Array of texture instances, each containing object specific bounds and texel factors */
	TArray<FStreamableTextureInstance>	TextureInstances;

	/**
	 * FStreamableTextureInfo serialize operator.
	 *
	 * @param	Ar					Archive to to serialize object to/ from
	 * @param	TextureInfo			Object to serialize
	 * @return	Returns the archive passed in
	 */
	friend FArchive& operator<<( FArchive& Ar, FStreamableTextureInfo& TextureInfo );
};

/**
 * Structure containing all information required for requesting removal or addition
 * of mip levels from/ to a texture.
 */
struct FTextureMipRequest
{
	/** Texture to add/ remove miplevels to/ from */
	FTextureBase*		Texture;

	/** Helper structure, bundling per miplevel specific data */
	struct FTextureMipData
	{
		/** Pointer to where mipdate should be copied */
		void*	Data;
		/** Size of memory region, used for verification and error checking */
		UINT	Size;
		/** IO request index used for cancelation */
		QWORD	IORequestIndex;
	}	
	/** Static array of miplevels that might need to be filled in. */
	Mips[14];

	/** Loader used to fulfill this request */
	struct FBackgroundLoader*	Loader;	

	/** Number of miplevels the texture should have after the operation completes */
	UINT						RequestedMips;
	/** Thread safe counter used to determine completion (== reaches 0). */
	FThreadSafeCounter			OutstandingMipRequests;
};

/**
 * Virtual base class for a content streaming class.	
 */
struct FContentStreamer
{
	/**
	 * Flushs resources. Waits for all outstanding requests to be fulfilled and optionally also
	 * removes any pending requests.
	 *
	 * @param OnlyOutstanding	Whether to flush all resources or simply wait till outstanding requests are fulfilled
	 */
	virtual void Flush( UBOOL OnlyOutstanding ) = 0;

	/**
	 * Take the current player/ context combination into account for streaming.
	 *
	 * @param Player	Player to get level from
	 * @param Context	Scene context used to retrieve location
	 */
	virtual void ProcessViewer( ULocalPlayer* Player, FSceneContext* Context ) = 0;

	/**
	 * Tick function, called before Viewport->Draw.
	 */
	virtual void Tick() = 0;

	/**
	 * Returns whether this content streamer currently considers a passed in resource
	 * for streaming.
	 *
	 * @param	Resource	resource to check for
	 * @return	TRUE if streamer is aware of resource, FALSE otherwise
	 */
	virtual UBOOL IsResourceConsidered( FResource* Resource ) = 0;
};

/**
 * Static texture streaming, obtaining data from prebuilt data stored in ULevel.	
 */
struct FStaticTextureStreamer : public FContentStreamer
{
	/**
	 * Default constructor	
	 */
	FStaticTextureStreamer()
	:	CurrentIndexOffset(0),
		InitialProcessViewerCall(1)
	{}

	/**
	 * Flushs resources. Waits for all outstanding requests to be fulfilled and optionally also
	 * removes any pending requests.
	 *
	 * @param OnlyOutstanding	Whether to flush all resources or simply wait till outstanding requests are fulfilled
	 */
	virtual void Flush( UBOOL OnlyOutstanding );
	
	/**
	 * Take the current player/ context combination into account for streaming.
	 *
	 * @param Player	Player to get level from
	 * @param Context	Scene context used to retrieve location
	 */
	virtual void ProcessViewer( ULocalPlayer* Player, FSceneContext* Context );

	/**
	 * Tick function, called before Viewport->Draw.
	 */
	virtual void Tick();

	/**
	 * Returns whether this content streamer currently considers a passed in resource
	 * for streaming.
	 *
	 * @param	Resource	resource to check for
	 * @return	TRUE if streamer is aware of resource, FALSE otherwise
	 */
	virtual UBOOL IsResourceConsidered( FResource* Resource );

protected:
	/** Dynamic map of textures to requested miplevels */
	TDynamicMap<FTextureBase*,UINT>					PendingRequests;
	/** Dynamic map of textures to outstanding mip requests */
	TDynamicMap<FTextureBase*,FTextureMipRequest*>	OutstandingRequests;

	/** Current offset into StaticStreamableTextures array */
	UINT	CurrentIndexOffset;
	/** Boolean indicating whether it's the first call to ProcessViewer either after flush or construction */
	UBOOL	InitialProcessViewerCall;
};

/**
 * Streaming manager, basically a collection of content streamers the ProcessViewer, Tick et al
 * calls get routed to.
 */
struct FStreamingManager
{	
	/**
	 * Take the current player/ context combination into account for streaming.
	 *
	 * @param Player	Player to get level from
	 * @param Context	Scene context used to retrieve location
	 */
	void ProcessViewer( ULocalPlayer* Player, FSceneContext* Context );

	/**
	 * Tick function, called before Viewport->Draw.
	 */
	void Tick();

	/**
	 * Flushs resources from all streamers. Waits for all outstanding requests to be fulfilled and 
	 * optionally also removes any pending requests.
	 *
	 * @param OnlyOutstanding	Whether to flush all resources or simply wait till outstanding requests are fulfilled
	 */
	void Flush( UBOOL OnlyOutstanding );

	/**
	 * Returns whether any content streamer currently considers a passed in resource
	 * for streaming.
	 *
	 * @param	Resource	resource to check for
	 * @return	TRUE if a streamer is aware of resource, FALSE otherwise
	 */
	UBOOL IsResourceConsidered( FResource* Resource );

	/**
	 * Adds a streamer to the list of content streamers to route requests to.	
	 *
	 * @param Streamer	Streamer to add.
	 */
	void AddStreamer( FContentStreamer* Streamer );
	
	/**
	 * Removes a streamer from the list of content streamers to route requests to.	
	 *
	 * @param Streamer	Streamer to remove.
	 */
	void RemoveStreamer( FContentStreamer* Streamer );

private:
	/** Array of content streamers functions gets routed to */
	TArray<FContentStreamer*> ContentStreamers;
};

/**
 * Virtual base class of a background loader used to request e.g. mipmaps from.	
 */
struct FBackgroundLoader
{
	/**
	 * Requests (potentially async) loading of miplevels.
	 *
	 * @param	TextureMipRequest	Structure containing information about requested mips (e.g. where to load them to)
	 * @return	TRUE if this loader can handle the request, FALSE otherwise
	 */
	virtual UBOOL LoadTextureMips( FTextureMipRequest* TextureMipRequest ) = 0;

	/**
	 * Requests the cancelation of a mip request if it isn't already in the process of being fulfilled.
	 *
 	 * @param	TextureMipRequest	Mip request to cancel.
	 * @return	TRUE if mip request was successfully canceled, FALSE if it is currently in the process of being 
	 *			fulfilled and hence wasn't canceled.
	 */
	virtual UBOOL CancelMipRequest( FTextureMipRequest* TextureMipRequest ) = 0;
};

/**
 * Base loader implemented using the default sync. Unreal loading approach.	
 */
struct FBlockingLoaderUnreal : public FBackgroundLoader
{
	/**
	 * Requests (potentially async) loading of miplevels.
	 *
	 * @param	TextureMipRequest	Structure containing information about requested mips (e.g. where to load them to)
	 * @return	TRUE if this loader can handle the request, FALSE otherwise
	 */
	virtual UBOOL LoadTextureMips( FTextureMipRequest* TextureMipRequest );

	/**
	 * Requests the cancelation of a mip request if it isn't already in the process of being fulfilled.
	 *
	 * @param	TextureMipRequest	Unused
	 * @return	FALSE as we're a blocking loader and hence have already fulfilled the request.
	 */
	virtual UBOOL CancelMipRequest( FTextureMipRequest* TextureMipRequest ) { return FALSE; }
};

/** 
 * Loader implemented using an async loading approach to load the data.
 */
struct FBackgroundLoaderUnreal : public FBackgroundLoader
{
	/**
	 * Requests (potentially async) loading of miplevels.
	 *
	 * @param	TextureMipRequest	Structure containing information about requested mips (e.g. where to load them to)
	 * @return	TRUE if this loader can handle the request, FALSE otherwise
	 */
	virtual UBOOL LoadTextureMips( FTextureMipRequest* TextureMipRequest );

	/**
	 * Requests the cancelation of a mip request if it isn't already in the process of being fulfilled.
	 *
	 * @param	TextureMipRequest	Mip request to cancel.
	 * @return	TRUE if mip request was successfully canceled, FALSE if it is currently in the process of being 
	 *			fulfilled and hence wasn't canceled.
	 */
	virtual UBOOL CancelMipRequest( FTextureMipRequest* TextureMipRequest );
};

/**
 * This class servers as a collection of loaders resource loading requests get routed to.
 */
struct FResourceLoader
{
	/**
	 * Requests (potentially async) loading of miplevels.
	 *
	 * @param	TextureMipRequest	Structure containing information about requested mips (e.g. where to load them to)
	 * @return	TRUE if any loader can handle the request, FALSE otherwise
	 */
	UBOOL LoadTextureMips( FTextureMipRequest* TextureMipRequest );

	/** 
	 * Adds a loader to the array of loaders taken into account.
	 *
	 * @param	BackgroundLoader	Loader to add to list.
	 */
	void AddLoader( FBackgroundLoader* BackgroundLoader );

	/** 
	 * Removes a loader from the array of loaders taken into account.
	 *
	 * @param	BackgroundLoader	Loader to remove from list.
	 */
	void RemoveLoader( FBackgroundLoader* BackgroundLoader );

protected:
	/** Array of loaders requests get routed to */
	TArray<FBackgroundLoader*> BackgroundLoaders;
};

/**
 * Virtual base class of async. IO manager, explicitely thread aware.	
 */
struct FAsyncIOManager : public FRunnable
{
	/**
	 * Requests data to be loaded async. Returns immediately.
	 *
	 * @param	Filename	Filename to load
	 * @param	Offset		Offset into file
	 * @param	Size		Size of load request
	 * @param	Dest		Pointer to load data into
	 * @param	Counter		Thread safe counter to decrement when loading has finished
	 *
	 * @return Returns an index to the request that can be used for canceling or 0 if the request failed.
	 */
	virtual QWORD LoadData( FString Filename, UINT Offset, UINT Size, void* Dest, FThreadSafeCounter* Counter ) = 0;
	
	/**
	 * Removes N outstanding requests from the queue in an atomic all or nothing operation.
	 * NOTE: Requests are only canceled if ALL requests are still pending and neither one
	 * is currently in flight.
	 *
	 * @param	RequestIndices	Indices of requests to cancel.
	 * @return	TRUE if all requests were still outstanding, FALSE otherwise
	 */
	virtual UBOOL CancelRequests( QWORD* RequestIndices, UINT NumIndices ) = 0;
	
	/**
	 * Blocks till all currently outstanding requests are canceled.
	 */
	virtual void CancelAllRequests() = 0;
};

/**
 * Windows implementation of an async IO manager.	
 */
struct FAsyncIOManagerWindows : public FAsyncIOManager
{
	// FAsyncIOManager interface.

	/**
	 * Requests data to be loaded async. Returns immediately.
	 *
	 * @param	Filename	Filename to load
	 * @param	Offset		Offset into file
	 * @param	Size		Size of load request
	 * @param	Dest		Pointer to load data into
	 * @param	Counter		Thread safe counter to decrement when loading has finished
	 *
	 * @return Returns an index to the request that can be used for canceling or 0 if the request failed.
	 */
	virtual QWORD LoadData( FString Filename, UINT Offset, UINT Size, void* Dest, FThreadSafeCounter* Counter );

	/**
	 * Removes N outstanding requests from the queue in an atomic all or nothing operation.
	 * NOTE: Requests are only canceled if ALL requests are still pending and neither one
	 * is currently in flight.
	 *
	 * @param	RequestIndices	Indices of requests to cancel.
	 * @return	TRUE if all requests were still outstanding, FALSE otherwise
	 */
	virtual UBOOL CancelRequests( QWORD* RequestIndices, UINT NumIndices );
	
	/**
	 * Blocks till all currently outstanding requests are canceled.
	 */
	virtual void CancelAllRequests();

	// FRunnable interface.

	/**
	 * Initializes critical section, event and other used variables. 
	 *
	 * This is called in the context of the thread object that aggregates 
	 * this, not the thread that passes this runnable to a new thread.
	 *
	 * @return True if initialization was successful, false otherwise
	 */
	virtual UBOOL Init();

	/**
	 * Called in the context of the aggregating thread to perform cleanup.
	 */
	virtual void Exit();

	/**
	 * This is where all the actual loading is done. This is only called
	 * if the initialization was successful.
	 *
	 * @return always 0
	 */
	virtual DWORD Run();
	
	/**
	 * This is called if a thread is requested to terminate early
	 */
	virtual void Stop();
	
protected:
	struct FAsyncIORequest
	{
		/** Index of request */
		QWORD				RequestIndex;
		/** Handle to file */
		HANDLE				FileHandle;
		/** Offset into file */
		UINT				Offset;
		/** Size in bytes of data to read */
		UINT				Size;
		/** Pointer to memory region used to read data into */
		void*				Dest;
		/** Thread safe counter that is decremented once work is done */
		FThreadSafeCounter* Counter;
	};

	/** Critical section used to syncronize access to outstanding requests map */
	FCriticalSection*		CriticalSection;
	/** TMap of file names to file handles */
	TMap<FString,HANDLE>	NameToHandleMap;
	/** Array of outstanding requests, processed in FIFO order */
	TArray<FAsyncIORequest>	OutstandingRequests;
	/** Event that is signaled if there are outstanding requests */
	FEvent*					OutstandingRequestsEvent;
	/** Thread safe counter that is 1 if the thread is currently reading from disk, 0 otherwise */
	FThreadSafeCounter		BusyReading;
	/** Thread safe counter that is 1 if the thread is available to process requests, 0 otherwise */
	FThreadSafeCounter		IsRunning;
	/** Current request index. We don't really worry about wrapping around with a QWORD */
	QWORD					RequestIndex;
};

/** Global async IO manager */
extern FAsyncIOManager*		GAsyncIOManager;
/** Global resource loader */
extern FResourceLoader*		GResourceLoader;
/** Global streaming manager */
extern FStreamingManager*	GStreamingManager;

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
