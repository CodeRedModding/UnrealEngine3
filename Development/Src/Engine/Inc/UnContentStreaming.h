/*=============================================================================
	UnContentStreaming.h: Definitions of classes used for content streaming.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Stats.
-----------------------------------------------------------------------------*/

/**
 * Streaming stats
 */
enum EStreamingStats
{
	STAT_StreamingTextures = STAT_AsyncLoadingTime + 1,
	STAT_StreamingTexturesSize,
	STAT_StreamingTexturesMaxSize,
	STAT_LightmapMemorySize,
	STAT_LightmapDiskSize,
	STAT_TexturePoolAllocatedSize,
	STAT_RequestsInCancelationPhase,
	STAT_RequestsInUpdatePhase,
	STAT_RequestsInFinalizePhase,
	STAT_IntermediateTextures,
	STAT_IntermediateTexturesSize,
	STAT_RequestSizeCurrentFrame,
	STAT_RequestSizeTotal,
	STAT_LightmapRequestSizeTotal,
	STAT_StreamingFudgeFactor,
	STAT_GameThreadUpdateTime,
	STAT_RenderingThreadUpdateTime,
	STAT_RenderingThreadFinalizeTime,
	STAT_StreamingLatency,
	STAT_StreamingBandwidth,
	STAT_DynamicStreamingTotal,
	STAT_AudioResourceCreationTime,
	STAT_VolumeStreamingTickTime,
	STAT_VolumeStreamingChecks,
	STAT_GrowingReallocations,
	STAT_ShrinkingReallocations,
	STAT_FullReallocations,
	STAT_FailedReallocations,
	STAT_PanicDefragmentations,
	STAT_AddToWorldTime,
	STAT_RemoveFromWorldTime,
	STAT_UpdateLevelStreamingTime,
	STAT_OptimalTextureSize,
	STAT_StreamingOverBudget,
	STAT_StreamingUnderBudget,
	STAT_NumWantingTextures,
	STAT_NumStreamingTextureInstances,
	STAT_NumStreamingLightmapInstances,
	STAT_TotalStaticTextureHeuristicSize,
	STAT_TotalLastRenderHeuristicSize,
	STAT_TotalDynamicHeuristicSize,
	STAT_TotalForcedHeuristicSize,
};

// Forward declarations
struct FStreamingTexture;
struct FStreamingContext;
class FAsyncTextureStreaming;
class UActorComponent;
class UPrimitiveComponent;
class AActor;
template<typename T>
class FAsyncTask;

/*-----------------------------------------------------------------------------
	Base streaming classes.
-----------------------------------------------------------------------------*/

enum EDynamicPrimitiveType
{
	DPT_Level,
	DPT_Spawned,
	DPT_MAX
};

enum ERemoveStreamingViews
{
	/** Removes normal views, but leaves override views. */
	RemoveStreamingViews_Normal,
	/** Removes all views. */
	RemoveStreamingViews_All
};


/**
 * Helper structure containing all relevant information for streaming.
 */
struct FStreamingViewInfo
{
	FStreamingViewInfo( const FVector& InViewOrigin, FLOAT InScreenSize, FLOAT InFOVScreenSize, FLOAT InBoostFactor, UBOOL bInOverrideLocation, FLOAT InDuration )
	:	ViewOrigin( InViewOrigin )
	,	ScreenSize( InScreenSize )
	,	FOVScreenSize( InFOVScreenSize )
	,	BoostFactor( InBoostFactor )
	,	Duration( InDuration )
	,	bOverrideLocation( bInOverrideLocation )
	{
	}
	/** View origin */
	FVector ViewOrigin;
	/** Screen size, not taking FOV into account */
	FLOAT	ScreenSize;
	/** Screen size, taking FOV into account */
	FLOAT	FOVScreenSize;
	/** A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa. */
	FLOAT	BoostFactor;
	/** How long the streaming system should keep checking this location, in seconds. 0 means just for the next Tick. */
	FLOAT	Duration;
	/** Whether this is an override location, which forces the streaming system to ignore all other regular locations */
	UBOOL	bOverrideLocation;
};

/**
 * Pure virtual base class of a streaming manager.
 */
struct FStreamingManagerBase
{
	FStreamingManagerBase()
	:	NumWantingResources(0)
	,	NumWantingResourcesCounter(0)
	{
	}

	/** Virtual destructor */
	virtual ~FStreamingManagerBase()
	{}

	/**
	 * Calls UpdateResourceStreaming(), and does per-frame cleaning. Call once per frame.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If TRUE, process all resources with no throttling limits
	 */
	void Tick( FLOAT DeltaTime, UBOOL bProcessEverything=FALSE );

	/**
	 * Updates streaming, taking into account all current view infos. Can be called multiple times per frame.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If TRUE, process all resources with no throttling limits
	 */
	virtual void UpdateResourceStreaming( FLOAT DeltaTime, UBOOL bProcessEverything=FALSE ) = 0;

	/**
	 * Streams in/out all resources that wants to and blocks until it's done.
	 *
	 * @param bIncludePlayerLocations	If TRUE, accounts for the location of all local players in the heuristics
	 * @param TimeLimit					Maximum number of seconds to wait for streaming I/O. If zero, uses .ini setting
	 * @return							Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual INT StreamAllResources( UBOOL bIncludePlayerLocations, FLOAT TimeLimit=0.0f )
	{
		return 0;
	}

	/**
	 * Updates streaming for an individual texture, taking into account all view infos.
	 *
	 * @param Texture		Texture to update
	 */
	virtual void UpdateIndividualResource( UTexture2D* Texture )
	{
	}

	/**
	 * Blocks till all pending requests are fulfilled.
	 *
	 * @param TimeLimit		Optional time limit for processing, in seconds. Specifying 0 means infinite time limit.
	 * @param bLogResults	Whether to dump the results to the log.
	 * @return				Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual INT BlockTillAllRequestsFinished( FLOAT TimeLimit = 0.0f, UBOOL bLogResults = FALSE ) = 0;

	/**
	 * Cancels the timed Forced resources (i.e used the Kismet action "Stream In Textures").
	 */
	virtual void CancelForcedResources() = 0;

	/**
	 * Notifies manager of "level" change.
	 */
	virtual void NotifyLevelChange() = 0;

	/**
	 * Removes streaming views from the streaming manager. This is also called by Tick().
	 *
	 * @param RemovalType	What types of views to remove (all or just the normal views)
	 */
	void RemoveStreamingViews( ERemoveStreamingViews RemovalType );

	/**
	 * Adds the passed in view information to the static array.
	 *
	 * @param ScreenSize			Screen size
	 * @param FOVScreenSize			Screen size taking FOV into account
	 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
	 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other regular locations
	 * @param Duration				How long the streaming system should keep checking this location, in seconds. 0 means just for the next Tick.
	 */
	void AddViewInformation( const FVector& ViewOrigin, FLOAT ScreenSize, FLOAT FOVScreenSize, FLOAT BoostFactor=1.0f, UBOOL bOverrideLocation=FALSE, FLOAT Duration=0.0f );

	/**
	 * Queue up view "slave" locations to the streaming system. These locations will be added properly at the next call to AddViewInformation,
	 * re-using the screensize and FOV settings.
	 *
	 * @param SlaveLocation			World-space view origin
	 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
	 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other locations
	 * @param Duration				How long the streaming system should keep checking this location, in seconds. 0 means just for the next Tick.
	 */
	void AddViewSlaveLocation( const FVector& SlaveLocation, FLOAT BoostFactor=1.0f, UBOOL bOverrideLocation=FALSE, FLOAT Duration=0.0f );

	/** Don't stream world resources for the next NumFrames. */
	virtual void SetDisregardWorldResourcesForFrames( INT NumFrames ) = 0;

	/**
	 * Temporarily boosts the streaming distance factor by the specified number.
	 * This factor is automatically reset to 1.0 after it's been used for mip-calculations.
	 */
	virtual void BoostTextures( AActor* Actor, FLOAT BoostFactor ) = 0;

	/**
	 *	Try to stream out texture mip-levels to free up more memory.
	 *	@param RequiredMemorySize	- Required minimum available texture memory
	 *	@return						- Whether it succeeded or not
	 **/
	virtual UBOOL StreamOutTextureData( INT RequiredMemorySize )
	{
		return FALSE;
	}

	/**
	 * Allows the streaming manager to process exec commands.
	 *
	 * @param Cmd	Exec command
	 * @param Ar	Output device for feedback
	 * @return		TRUE if the command was handled
	 */
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar ) 
	{
		return FALSE;
	}

	/** Adds a ULevel to the streaming manager. */
	virtual void AddLevel( class ULevel* Level ) = 0;

	/** Removes a ULevel from the streaming manager. */
	virtual void RemoveLevel( class ULevel* Level ) = 0;

	/** Adds a new texture to the streaming manager. */
	virtual void AddStreamingTexture( UTexture2D* Texture )
	{
	}

	/** Removes a texture from the streaming manager. */
	virtual void RemoveStreamingTexture( UTexture2D* Texture )
	{
	}

	/** Called when an actor is spawned. */
	virtual void NotifyActorSpawned( AActor* Actor )
	{
	}

	/** Called when a spawned actor is destroyed. */
	virtual void NotifyActorDestroyed( AActor* Actor )
	{
	}

	/**
	 * Called when a primitive is attached to an actor or another component.
	 * Replaces previous info, if the primitive was already attached.
	 *
	 * @param InPrimitive	Newly attached dynamic/spawned primitive
	 */
	virtual void NotifyPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType )
	{
	}

	/** Called when a primitive is detached from an actor or another component. */
	virtual void NotifyPrimitiveDetached( const UPrimitiveComponent* Primitive )
	{
	}

	/**
	 * Called when a LastRenderTime primitive is attached to an actor or another component.
	 * Modifies the LastRenderTimeRefCount for the textures used, so that those textures can
	 * use both distance-based and LastRenderTime heuristics.
	 *
	 * @param InPrimitive	Newly attached dynamic/spawned primitive
	 */
	virtual void NotifyTimedPrimitiveAttached( const UPrimitiveComponent* InPrimitive, EDynamicPrimitiveType DynamicType )
	{
	}

	/**
	 * Called when a LastRenderTime primitive is detached from an actor or another component.
	 * Modifies the LastRenderTimeRefCount for the textures used, so that those textures can
	 * use both distance-based and LastRenderTime heuristics.
	 *
	 * @param InPrimitive	Newly detached dynamic/spawned primitive
	 */
	virtual void NotifyTimedPrimitiveDetached( const UPrimitiveComponent* InPrimitive )
	{
	}

	/**
	 * Called when a primitive has had its textured changed.
	 * Only affects primitives that were already attached.
	 * Replaces previous info.
	 */
	virtual void NotifyPrimitiveUpdated( const UPrimitiveComponent* Primitive )
	{
	}

	/** Returns the number of view infos. */
	INT GetNumViews() const
	{
		return CurrentViewInfos.Num();
	}

	/** Returns the view info by the specified index. */
	const FStreamingViewInfo& GetViewInformation( INT ViewIndex ) const
	{
		return CurrentViewInfos( ViewIndex );
	}

	/** Returns the number of resources that currently wants to be streamed in. */
	virtual INT GetNumWantingResources() const
	{
		return NumWantingResources;
	}

	/**
	 * Returns the current ID for GetNumWantingResources().
	 * The ID is incremented every time NumWantingResources is updated by the streaming system (every few frames).
	 * Can be used to verify that any changes have been fully examined, by comparing current ID with
	 * what it was when the changes were made.
	 */
	virtual INT GetNumWantingResourcesID() const
	{
		return NumWantingResourcesCounter;
	}

	/** Returns TRUE if this is a streaming resource that is managed by the streaming manager. */
	virtual UBOOL IsManagedStreamingResource( const UTexture2D* Texture2D ) = 0;

protected:

	/**
	 * Sets up the CurrentViewInfos array based on PendingViewInfos, LastingViewInfos and SlaveLocations.
	 * Removes out-dated LastingViewInfos.
	 *
	 * @param DeltaTime		Time since last call in seconds
	 */
	void SetupViewInfos( FLOAT DeltaTime );

	/**
	 * Adds the passed in view information to the static array.
	 *
	 * @param ViewInfos				Array to add the view to
	 * @param ViewOrigin			View origin
	 * @param ScreenSize			Screen size
	 * @param FOVScreenSize			Screen size taking FOV into account
	 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
	 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other regular locations
	 * @param Duration				How long the streaming system should keep checking this location (in seconds). 0 means just for the next Tick.
	 */
	static void AddViewInfoToArray( TArray<FStreamingViewInfo> &ViewInfos, const FVector& ViewOrigin, FLOAT ScreenSize, FLOAT FOVScreenSize, FLOAT BoostFactor, UBOOL bOverrideLocation, FLOAT Duration );

	/**
	 * Remove view infos with the same location from the given array.
	 *
	 * @param ViewInfos				[in/out] Array to remove the view from
	 * @param ViewOrigin			View origin
	 */
	static void RemoveViewInfoFromArray( TArray<FStreamingViewInfo> &ViewInfos, const FVector& ViewOrigin );

	struct FSlaveLocation
	{
		FSlaveLocation( const FVector& InLocation, FLOAT InBoostFactor, UBOOL bInOverrideLocation, FLOAT InDuration )
		:	Location( InLocation )
		,	BoostFactor( InBoostFactor )
		,	Duration( InDuration )
		,	bOverrideLocation( bInOverrideLocation )
		{
		}
		/** A location to use for distance-based heuristics next Tick(). */
		FVector		Location;
		/** A boost factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa. */
		FLOAT		BoostFactor;
		/** How long the streaming system should keep checking this location (in seconds). 0 means just for the next Tick. */
		FLOAT		Duration;
		/** Whether this is an override location, which forces the streaming system to ignore all other locations */
		UBOOL		bOverrideLocation;
	};

	/** Current collection of views that need to be taken into account for streaming. Emptied every frame. */
	static TArray<FStreamingViewInfo> CurrentViewInfos;

	/** Pending views. Emptied every frame. */
	static TArray<FStreamingViewInfo> PendingViewInfos;

	/** Views that stick around for a while. Override views are ignored if no movie is playing. */
	static TArray<FStreamingViewInfo> LastingViewInfos;

	/** Collection of view locations that will be added at the next call to AddViewInformation. */
	static TArray<FSlaveLocation> SlaveLocations;

	/** Set when Tick() has been called. The first time a new view is added, it will clear out all old views. */
	static UBOOL bPendingRemoveViews;

	/** Number of resources that currently wants to be streamed in. */
	INT		NumWantingResources;

	/**
	 * The current counter for NumWantingResources.
	 * This counter is bumped every time NumWantingResources is updated by the streaming system (every few frames).
	 * Can be used to verify that any changes have been fully examined, by comparing current counter with
	 * what it was when the changes were made.
	 */
	INT		NumWantingResourcesCounter;
};

/**
 * Streaming manager collection, routing function calls to streaming managers that have been added
 * via AddStreamingManager.
 */
struct FStreamingManagerCollection : public FStreamingManagerBase
{
	/** Default constructor, initializing all member variables. */
	FStreamingManagerCollection();

	/**
	 * Updates streaming, taking into account all current view infos. Can be called multiple times per frame.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If TRUE, process all resources with no throttling limits
	 */
	virtual void UpdateResourceStreaming( FLOAT DeltaTime, UBOOL bProcessEverything=FALSE );

	/**
	 * Streams in/out all resources that wants to and blocks until it's done.
	 *
	 * @param bIncludePlayerLocations	If TRUE, accounts for the location of all local players in the heuristics
	 * @param TimeLimit					Maximum number of seconds to wait for streaming I/O. If zero, uses .ini setting
	 * @return							Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual INT StreamAllResources( UBOOL bIncludePlayerLocations, FLOAT TimeLimit=0.0f );

	/**
	 * Updates streaming for an individual texture, taking into account all view infos.
	 *
	 * @param Texture	Texture to update
	 */
	virtual void UpdateIndividualResource( UTexture2D* Texture );

	/**
	 * Blocks till all pending requests are fulfilled.
	 *
	 * @param TimeLimit		Optional time limit for processing, in seconds. Specifying 0 means infinite time limit.
	 * @param bLogResults	Whether to dump the results to the log.
	 * @return				Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual INT BlockTillAllRequestsFinished( FLOAT TimeLimit = 0.0f, UBOOL bLogResults = FALSE );

	/** Returns the number of resources that currently wants to be streamed in. */
	virtual INT GetNumWantingResources() const;

	/**
	 * Returns the current ID for GetNumWantingResources().
	 * The ID is bumped every time NumWantingResources is updated by the streaming system (every few frames).
	 * Can be used to verify that any changes have been fully examined, by comparing current ID with
	 * what it was when the changes were made.
	 */
	virtual INT GetNumWantingResourcesID() const;

	/**
	 * Cancels the timed Forced resources (i.e used the Kismet action "Stream In Textures").
	 */
	virtual void CancelForcedResources();

	/**
	 * Notifies manager of "level" change.
	 */
	virtual void NotifyLevelChange();

	/**
	 * Adds a streaming manager to the array of managers to route function calls to.
	 *
	 * @param StreamingManager	Streaming manager to add
	 */
	void AddStreamingManager( FStreamingManagerBase* StreamingManager );

	/**
	 * Removes a streaming manager from the array of managers to route function calls to.
	 *
	 * @param StreamingManager	Streaming manager to remove
	 */
	void RemoveStreamingManager( FStreamingManagerBase* StreamingManager );

	/**
	 * Sets the number of iterations to use for the next time UpdateResourceStreaming is being called. This 
	 * is being reset to 1 afterwards.
	 *
	 * @param NumIterations	Number of iterations to perform the next time UpdateResourceStreaming is being called.
	 */
	void SetNumIterationsForNextFrame( INT NumIterations );

	/** Don't stream world resources for the next NumFrames. */
	virtual void SetDisregardWorldResourcesForFrames( INT NumFrames );

	/**
	 * Temporarily boosts the streaming distance factor by the specified number.
	 * This factor is automatically reset to 1.0 after it's been used for mip-calculations.
	 */
	virtual void BoostTextures( AActor* Actor, FLOAT BoostFactor );

	/**
	 * Disables resource streaming. Enable with EnableResourceStreaming. Disable/enable can be called multiple times nested
	 */
	void DisableResourceStreaming();

	/**
	 * Enables resource streaming, previously disabled with enableResourceStreaming. Disable/enable can be called multiple times nested
	 * (this will only actually enable when all disables are matched with enables)
	 */
	void EnableResourceStreaming();

	/**
	 *	Try to stream out texture mip-levels to free up more memory.
	 *	@param RequiredMemorySize	- Required minimum available texture memory
	 *	@return						- Whether it succeeded or not
	 **/
	virtual UBOOL StreamOutTextureData( INT RequiredMemorySize );

	/**
	 * Allows the streaming manager to process exec commands.
	 *
	 * @param Cmd	Exec command
	 * @param Ar	Output device for feedback
	 * @return		TRUE if the command was handled
	 */
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar ) ;

	/** Adds a new texture to the streaming manager. */
	virtual void AddStreamingTexture( UTexture2D* Texture );

	/** Removes a texture from the streaming manager. */
	virtual void RemoveStreamingTexture( UTexture2D* Texture );

	/** Adds a ULevel to the streaming manager. */
	virtual void AddLevel( class ULevel* Level );

	/** Removes a ULevel from the streaming manager. */
	virtual void RemoveLevel( class ULevel* Level );

	/** Called when an actor is spawned. */
	virtual void NotifyActorSpawned( AActor* Actor );

	/** Called when a spawned actor is destroyed. */
	virtual void NotifyActorDestroyed( AActor* Actor );

	/**
	 * Called when a primitive is attached to an actor or another component.
	 * Replaces previous info, if the primitive was already attached.
	 *
	 * @param InPrimitive	Newly attached dynamic/spawned primitive
	 */
	virtual void NotifyPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType );

	/** Called when a primitive is detached from an actor or another component. */
	virtual void NotifyPrimitiveDetached( const UPrimitiveComponent* Primitive );

	/**
	 * Called when a primitive has had its textured changed.
	 * Only affects primitives that were already attached.
	 * Replaces previous info.
	 */
	virtual void NotifyPrimitiveUpdated( const UPrimitiveComponent* Primitive );

	/** Returns TRUE if this is a streaming resource that is managed by the streaming manager. */
	virtual UBOOL IsManagedStreamingResource( const UTexture2D* Texture2D );

protected:
	/** Array of streaming managers to route function calls to */
	TArray<FStreamingManagerBase*> StreamingManagers;
	/** Number of iterations to perform. Gets reset to 1 each frame. */
	INT NumIterations;

	/** Count of how many nested DisableResourceStreaming's were called - will enable when this is 0 */
	INT DisableResourceStreamingCount;

	/** Maximum number of seconds to block in StreamAllResources(), by default (.ini setting). */
	FLOAT LoadMapTimeLimit;
};

/*-----------------------------------------------------------------------------
	Texture streaming helper structs.
-----------------------------------------------------------------------------*/

/**
 * Structure containing all information needed for determining the screen space
 * size of an object/ texture instance.
 */
struct FStreamableTextureInstance
{
	/** Bounding sphere/ box of object */
	FSphere BoundingSphere;
	/** Object (and bounding sphere) specific texel scale factor  */
	FLOAT	TexelFactor;

	/**
	 * FStreamableTextureInstance serialize operator.
	 *
	 * @param	Ar					Archive to to serialize object to/ from
	 * @param	TextureInstance		Object to serialize
	 * @return	Returns the archive passed in
	 */
	friend FArchive& operator<<( FArchive& Ar, FStreamableTextureInstance& TextureInstance );
};

/**
 * Serialized ULevel information about dynamic texture instances
 */
struct FDynamicTextureInstance : public FStreamableTextureInstance
{
	/** Texture that is used by a dynamic UPrimitiveComponent. */
	UTexture2D*					Texture;
	/** Whether the primitive that uses this texture is attached to the scene or not. */
	UBOOL						bAttached;
	/** Original bounding sphere radius, at the time the TexelFactor was calculated originally. */
	FLOAT						OriginalRadius;

	/**
	 * FDynamicTextureInstance serialize operator.
	 *
	 * @param	Ar					Archive to to serialize object to/ from
	 * @param	TextureInstance		Object to serialize
	 * @return	Returns the archive passed in
	 */
	friend FArchive& operator<<( FArchive& Ar, FDynamicTextureInstance& TextureInstance );
};

struct FSpawnedTextureInstance
{
	FSpawnedTextureInstance( UTexture2D* InTexture2D, FLOAT InTexelFactor, FLOAT InOriginalRadius )
	:	Texture2D( InTexture2D )
	,	TexelFactor( InTexelFactor )
	,	InvOriginalRadius( (InOriginalRadius > 0.0f) ? (1.0f/InOriginalRadius) : 1.0f )
	{
	}
	/** Texture that is used by a dynamic UPrimitiveComponent. */
	UTexture2D*		Texture2D;
	/** Texture specific texel scale factor  */
	FLOAT			TexelFactor;
	/** 1.0f / OriginalBoundingSphereRadius, at the time the TexelFactor was calculated originally. */
	FLOAT			InvOriginalRadius;
};

/**
 * Helper class for tracking upcoming changes to the texture memory,
 * and how much of the currently allocated memory is used temporarily for streaming.
 */
struct FStreamMemoryTracker
{
	/** Stream-in memory that hasn't been allocated yet. */
	volatile INT PendingStreamIn;
	/** Temp memory that hasn't been allocated yet. */
	volatile INT PendingTempMemory;
	/** Stream-out memory that is allocated but hasn't been freed yet. */
	volatile INT CurrentStreamOut;
	/** Temp memory that is allocated, but not freed yet. */
	volatile INT CurrentTempMemory;

	//@TODO: Possibly track canceling early (on the gamethread), not just in finalize.
	//@TODO: Add support for pre-planned in-place reallocs

	/** Constructor for the texture streaming memory tracker. */
	FStreamMemoryTracker();

	/** Track 'start streaming' on the gamethread. (Memory not yet affected.) */
	void GameThread_BeginUpdate( const UTexture2D& Texture );

	/**
	 * Track 'start streaming' on the renderthread. (Memory is now allocated/deallocated.)
	 * 
	 * @param Texture				Texture that is beginning to stream
	 * @param bUsingInPlaceRealloc	Whether it's using in-place reallocation
	 * @param bSuccessful			Whether the update succeeded or not
	 */
	void RenderThread_Update( const UTexture2D& Texture, UBOOL bUsingInPlaceRealloc, UBOOL bSuccessful );

	/**
	 * Track 'streaming finished' on the renderthread.
	 * Note: Only called if the RenderThread Update was successful.
	 *
	 * @param Texture				Texture that is being finalized
	 * @param bUsingInPlaceRealloc	Whether it's using in-place reallocation
	 * @param bSuccessful			Whether the finalize succeeded or not
	 */
	void RenderThread_Finalize( const UTexture2D& Texture, UBOOL bUsingInPlaceRealloc, UBOOL bSuccessful );

	/** Calculate how much texture memory is currently available for streaming. */
	INT CalcAvailableNow( INT TotalFreeMemory, INT MemoryMargin );

	/** Calculate how much texture memory will available later for streaming. */
	INT CalcAvailableLater( INT TotalFreeMemory, INT MemoryMargin );

	/** Calculate how much texture memory is currently being used for temporary texture data during streaming. */
	INT CalcTempMemory();
};

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

/** Global streaming manager */
extern FStreamingManagerCollection*	GStreamingManager;

/** Texture streaming memory tracker. */
extern FStreamMemoryTracker GStreamMemoryTracker;


/*-----------------------------------------------------------------------------
	Texture streaming.
-----------------------------------------------------------------------------*/

struct FStreamingHandlerTextureBase;
#define NUM_BANDWIDTHSAMPLES 512
#define NUM_LATENCYSAMPLES 512

typedef TKeyValuePair< FLOAT, INT > FTexturePriority;
typedef TArray<INT, TMemStackAllocator<GMainThreadMemStack> > FStreamingRequests;

enum FStreamoutLogic
{
	StreamOut_UnwantedMips,
	StreamOut_AllMips,
};


/**
 * Structure containing all information needed for determining the screen space
 * size of an object/ texture instance.
 */
struct FStreamableTextureInstance4
{
	FStreamableTextureInstance4()
	:	BoundingSphereX( 3.402823466e+38F, 3.402823466e+38F, 3.402823466e+38F, 3.402823466e+38F )
	,	BoundingSphereY( 0, 0, 0, 0 )
	,	BoundingSphereZ( 0, 0, 0, 0 )
	,	BoundingSphereRadius( 0, 0, 0, 0 )
	,	TexelFactor( 0, 0, 0, 0 )
	{
	}
	/** X coordinates for the bounding sphere origin of 4 texture instances */
	FVector4 BoundingSphereX;
	/** Y coordinates for the bounding sphere origin of 4 texture instances */
	FVector4 BoundingSphereY;
	/** Z coordinates for the bounding sphere origin of 4 texture instances */
	FVector4 BoundingSphereZ;
	/** Sphere radii for the bounding sphere of 4 texture instances */
	FVector4 BoundingSphereRadius;
	/** Texel scale factors for 4 texture instances */
	FVector4 TexelFactor;
};


/**
 * Streaming manager dealing with textures.
 */
struct FStreamingManagerTexture : public FStreamingManagerBase
{
	/** Constructor, initializing all members */
	FStreamingManagerTexture();

	virtual ~FStreamingManagerTexture();

	/**
	 * Updates streaming, taking into account all current view infos. Can be called multiple times per frame.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If TRUE, process all resources with no throttling limits
	 */
	virtual void UpdateResourceStreaming( FLOAT DeltaTime, UBOOL bProcessEverything=FALSE );

	/**
	 * New texture streaming system, based on texture priorities and asynchronous processing.
	 * Updates streaming, taking into account all view infos.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If TRUE, process all resources with no throttling limits
	 */
	void NewUpdateResourceStreaming( FLOAT DeltaTime, UBOOL bProcessEverything=FALSE );

	/**
	 * Updates streaming for an individual texture, taking into account all view infos.
	 *
	 * @param Texture	Texture to update
	 */
	virtual void UpdateIndividualResource( UTexture2D* Texture );

	/**
	 * Blocks till all pending requests are fulfilled.
	 *
	 * @param TimeLimit		Optional time limit for processing, in seconds. Specifying 0 means infinite time limit.
	 * @param bLogResults	Whether to dump the results to the log.
	 * @return				Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual INT BlockTillAllRequestsFinished( FLOAT TimeLimit = 0.0f, UBOOL bLogResults = FALSE );

	/**
	 * Cancels the timed Forced resources (i.e used the Kismet action "Stream In Textures").
	 */
	virtual void CancelForcedResources();

	/**
	 * Notifies manager of "level" change so it can prioritize character textures for a few frames.
	 */
	virtual void NotifyLevelChange();

	/**
	 * Adds a textures streaming handler to the array of handlers used to determine which
	 * miplevels need to be streamed in/ out.
	 *
	 * @param TextureStreamingHandler	Handler to add
	 */
	void AddTextureStreamingHandler( FStreamingHandlerTextureBase* TextureStreamingHandler );

	/**
	 * Removes a textures streaming handler from the array of handlers used to determine which
	 * miplevels need to be streamed in/ out.
	 *
	 * @param TextureStreamingHandler	Handler to remove
	 */
	void RemoveTextureStreamingHandler( FStreamingHandlerTextureBase* TextureStreamingHandler );

	/** Don't stream world resources for the next NumFrames. */
	virtual void SetDisregardWorldResourcesForFrames( INT NumFrames );

	/**
	 *	Try to stream out texture mip-levels to free up more memory.
	 *	@param RequiredMemorySize	- Required minimum available texture memory
	 *	@return						- Whether it succeeded or not
	 **/
	virtual UBOOL StreamOutTextureData( INT RequiredMemorySize );

	/**
	 * Allows the streaming manager to process exec commands.
	 *
	 * @param Cmd	Exec command
	 * @param Ar	Output device for feedback
	 * @return		TRUE if the command was handled
	 */
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar ) ;

	/** Adds a new texture to the streaming manager. */
	virtual void AddStreamingTexture( UTexture2D* Texture );

	/** Removes a texture from the streaming manager. */
	virtual void RemoveStreamingTexture( UTexture2D* Texture );

	/** Adds a ULevel to the streaming manager. */
	virtual void AddLevel( class ULevel* Level );

	/** Removes a ULevel from the streaming manager. */
	virtual void RemoveLevel( class ULevel* Level );

	/** Called when an actor is spawned. */
	virtual void NotifyActorSpawned( AActor* Actor );

	/** Called when a spawned actor is destroyed. */
	virtual void NotifyActorDestroyed( AActor* Actor );

	/**
	 * Called when a primitive is attached to an actor or another component.
	 * Replaces previous info, if the primitive was already attached.
	 *
	 * @param InPrimitive	Newly attached dynamic/spawned primitive
	 */
	virtual void NotifyPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType );

	/** Called when a primitive is detached from an actor or another component. */
	virtual void NotifyPrimitiveDetached( const UPrimitiveComponent* Primitive );

	/**
	 * Called when a LastRenderTime primitive is attached to an actor or another component.
	 * Modifies the LastRenderTimeRefCount for the textures used, so that those textures can
	 * use both distance-based and LastRenderTime heuristics.
	 *
	 * @param Primitive	Newly attached dynamic/spawned primitive
	 */
	virtual void NotifyTimedPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType );

	/**
	 * Called when a LastRenderTime primitive is detached from an actor or another component.
	 * Modifies the LastRenderTimeRefCount for the textures used, so that those textures can
	 * use both distance-based and LastRenderTime heuristics.
	 *
	 * @param Primitive	Newly detached dynamic/spawned primitive
	 */
	virtual void NotifyTimedPrimitiveDetached( const UPrimitiveComponent* Primitive );

	/**
	 * Called when a primitive has had its textured changed.
	 * Only affects primitives that were already attached.
	 * Replaces previous info.
	 */
	virtual void NotifyPrimitiveUpdated( const UPrimitiveComponent* Primitive );

	UBOOL AddDynamicPrimitive( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType );
	UBOOL RemoveDynamicPrimitive( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType );

	/** Returns the corresponding FStreamingTexture for a UTexture2D. */
	FStreamingTexture& GetStreamingTexture( const UTexture2D* Texture2D );

	/** Returns TRUE if this is a streaming texture that is managed by the streaming manager. */
	UBOOL IsManagedStreamingTexture( const UTexture2D* Texture2D );

	/** Returns TRUE if this is a streaming resource that is managed by the streaming manager. */
	virtual UBOOL IsManagedStreamingResource( const UTexture2D* Texture2D )
	{
		return IsManagedStreamingTexture( Texture2D );
	}

	/** Updates the I/O state of a texture (allowing it to progress to the next stage) and some stats. */
	void UpdateTextureStatus( FStreamingTexture& StreamingTexture, FStreamingContext& Context );

	/**
	 * Starts streaming in/out a texture.
	 *
	 * @param StreamingTexture			Texture to start to stream in/out
	 * @param WantedMips				Number of mips we want in memory for this texture.
	 * @param Context					Context for the current frame
	 * @param bIgnoreMemoryConstraints	Whether to ignore memory constraints and always start streaming
	 * @return							TRUE if the texture is now in flight
	 */
	UBOOL StartStreaming( FStreamingTexture& StreamingTexture, INT WantedMips, FStreamingContext& Context, UBOOL bIgnoreMemoryConstraints );

	/**
	 * Conditionally cancels a current streaming request, if we've changed our mind on the number of mip-levels we want in memory.
	 *
	 * @param StreamingTexture		Texture to potentially cancel streaming for
	 * @param Context				Streaming context
	 * @return						TRUE if a streaming request was canceled
	 */
	UBOOL ConditionallyCancelTextureStreaming( FStreamingTexture& StreamingTexture, FStreamingContext& Context );

	/**
	 * Cancels the current streaming request for the specified texture.
	 *
	 * @param StreamingTexture		Texture to cancel streaming for
	 * @return						TRUE if a streaming request was canceled
	 */
	UBOOL CancelStreamingRequest( FStreamingTexture& StreamingTexture );

	/** Legacy: Returns the current fudge factor. */
	FLOAT GetFudgeFactor() const
	{
		return FudgeFactor;
	}

	/** Resets the streaming statistics to zero. */
	void ResetStreamingStats();

	/**
	 * Updates the streaming statistics with current frame's worth of stats.
	 *
	 * @param Context					Context for the current frame
	 * @param bAllTexturesProcessed		Whether all processing is complete
	 */
	void UpdateStreamingStats( const FStreamingContext& Context, UBOOL bAllTexturesProcessed );

	/** Returns the number of cached view infos (thread-safe). */
	INT ThreadNumViews()
	{
		return ThreadSettings.ThreadViewInfos.Num();
	}

	/** Returns a cached view info for the specified index (thread-safe). */
	const FStreamingViewInfo& ThreadGetView( INT ViewIndex )
	{
		return ThreadSettings.ThreadViewInfos( ViewIndex );
	}

	/** Returns whether we're using the new priority-based streaming system or not. */
	UBOOL IsUsingPriorityStreaming() const
	{
		return bUsePriorityStreaming;
	}

#if STATS
	/** Ringbuffer of bandwidth samples for streaming in mip-levels (MB/sec). */
	static FLOAT BandwidthSamples[NUM_BANDWIDTHSAMPLES];
	/** Number of bandwidth samples that have been filled in. Will stop counting when it reaches NUM_BANDWIDTHSAMPLES. */
	static INT NumBandwidthSamples;
	/** Current sample index in the ring buffer. */
	static INT BandwidthSampleIndex;
	/** Average of all bandwidth samples in the ring buffer, in MB/sec. */
	static FLOAT BandwidthAverage;
	/** Maximum bandwidth measured since the start of the game.  */
	static FLOAT BandwidthMaximum;
	/** Minimum bandwidth measured since the start of the game.  */
	static FLOAT BandwidthMinimum;
#endif
	/** Number of streaming texture instances (including lightmaps). */
	DWORD NumStreamingTextureInstances;
	/** Number of streaming lightmap instances. */
	DWORD NumStreamingLightmapInstances;


protected:
//BEGIN: Thread-safe functions and data
		friend class FAsyncTextureStreaming;
		friend struct FStreamingHandlerTextureStatic;

		/** Calculates the minimum and maximum number of mip-levels for a streaming texture. */
		void CalcMinMaxMips( FStreamingTexture& StreamingTexture );

		/** Calculates the number of mip-levels we would like to have in memory for a texture. */
		void CalcWantedMips( FStreamingTexture& StreamingTexture );

		/**
		 * Fallback handler to catch textures that have been orphaned recently.
		 * This handler prevents massive spike in texture memory usage.
		 * Orphaned textures were previously streamed based on distance but those instance locations have been removed -
		 * for instance because a ULevel is being unloaded. These textures are still active and in memory until they are garbage collected,
		 * so we must ensure that they do not start using the LastRenderTime fallback handler and suddenly stream in all their mips -
		 * just to be destroyed shortly after by a garbage collect.
		 */
		INT GetWantedMipsForOrphanedTexture( FStreamingTexture& StreamingTexture, FLOAT& Distance );

		/** Updates this frame's STATs by one texture. */
		void UpdateFrameStats( FStreamingTexture& StreamingTexture, FStreamingContext& Context );

		/**
		 * Not thread-safe: Updates a portion (as indicated by 'StageIndex') of all streaming textures,
		 * allowing their streaming state to progress.
		 *
		 * @param Context			Context for the current stage (frame)
		 * @param StageIndex		Current stage index
		 * @param NumUpdateStages	Number of texture update stages
		 */
		void UpdateStreamingTextures( FStreamingContext& Context, INT StageIndex, INT NumStages );

		/** Adds new textures and level data on the gamethread (while the worker thread isn't active). */
		void UpdateThreadData();

		/**
		 * Temporarily boosts the streaming distance factor by the specified number.
		 * This factor is automatically reset to 1.0 after it's been used for mip-calculations.
		 */
		void BoostTextures( AActor* Actor, FLOAT BoostFactor );

		/** Updates the thread-safe cache information for dynamic primitives. */
		void UpdateDynamicPrimitiveCache();

		/** Calculates DynamicWantedMips and DynamicMinDistance for all dynamic textures. */
		void CalcDynamicWantedMips();

		/**
		 * Stream textures in/out, based on the priorities calculated by the async work.
		 *
		 * @param bProcessEverything	Whether we're processing all textures in one go
		 */
		void StreamTextures( UBOOL bProcessEverything );

		/**
		 * Try to stream out textures based on the specified logic.
		 *
		 * @param StreamoutLogic		The logic to use for streaming out
		 * @param AvailableLater		[in/out] Estimated amount of memory that will be free at a later time (after all pending stream in/out)
		 * @param TempMemoryUsed		[in/out] Estimated amount of temp memory required for streaming
		 * @param StartIndex			First priority index to try
		 * @param StopIndex				Last priority index to try
		 * @param LowPrioIndex			[in/out] The lowest-priority texture that can stream out
		 * @param PrioritizedTextures	Indices to the working set of streaming textures, sorted from highest priority to lowest
		 * @param StreamingRequests		[in/out] Indices to textures that are going to be streamed this frame
		 * @return						The last priority index considered (may be sooner that StopIndex)
		 */
		INT StreamoutTextures( FStreamoutLogic StreamoutLogic, INT& AvailableLater, INT& TempMemoryUsed, INT StartIndex, INT StopIndex, INT& LowPrioIndex, const TArray<FTexturePriority>& PrioritizedTextures, FStreamingRequests& StreamingRequests );

		/**
		 * Stream textures in/out, when no texture pool with limited size is used by the platform.
		 *
		 * @param Context				Context for the current stage
		 * @param PrioritizedTextures	Array of prioritized textures to process
		 * @param TempMemoryUsed		Current amount of temporary memory used by the streaming system, in bytes
		 */
		void StreamTexturesUnlimited( FStreamingContext& Context, const TArray<FTexturePriority>& PrioritizedTextures, INT TempMemoryUsed );

		/** Thread-safe helper struct for per-level information. */
		struct FThreadLevelData
		{
			FThreadLevelData()
			:	bRemove( FALSE )
			{
			}

			/** Whether this level has been removed. */
			UBOOL bRemove;

			/** Texture instances used in this level, stored in SIMD-friendly format. */
			TMap<const UTexture2D*,TArray<FStreamableTextureInstance4> > ThreadTextureInstances;
		};

		/** Texture instance data for a spawned primitive. Stored in the ThreadSettings.SpawnedPrimitives map. */
		struct FSpawnedPrimitiveData
		{
			FSpawnedPrimitiveData()
			:	bAttached( FALSE )
			,	bPendingUpdate( FALSE )
			{
			}
			/** Texture instances used by primitive. */
			TArray<FSpawnedTextureInstance> TextureInstances;
			/** Bounding sphere of primitive. */
			FSphere		BoundingSphere;
			/** Dynamic primitive tracking type. */
			EDynamicPrimitiveType DynamicType;
			/** Whether the primitive that uses this texture is currently attached to the scene or not. */
			BITFIELD	bAttached : 1;
			/** Set to TRUE when it's marked for Attach or Detach. Don't touch this primitive until it's been fully updated. */
			BITFIELD	bPendingUpdate : 1;
		};

		typedef TKeyValuePair< class ULevel*, FThreadLevelData >	FLevelData;

		/** Thread-safe helper struct for streaming information. */
		struct FThreadSettings
		{
			/** Cached from the system settings. */
			INT NumStreamedMips[TEXTUREGROUP_MAX];

			/** Cached from each ULevel. */
			TArray< FLevelData > LevelData;

			/** Cached from FStreamingManagerBase. */
			TArray<FStreamingViewInfo> ThreadViewInfos;

			/** Maps spawned primitives to texture instances. Owns the instance data. */
			TMap<const UPrimitiveComponent*,FSpawnedPrimitiveData> SpawnedPrimitives;
		};

		/** Thread-safe helper data for streaming information. */
		FThreadSettings	ThreadSettings;

		/** All streaming UTexture2D objects. */
		TArray<FStreamingTexture> StreamingTextures;

		/** Index of the StreamingTexture that will be updated next by UpdateStreamingTextures(). */
		INT CurrentUpdateStreamingTextureIndex;

		/** Remaining ticks to disregard world textures. */
		INT RemainingTicksToDisregardWorldTextures;
//END: Thread-safe functions and data

	/**
	 * Mark the textures instances with a timestamp. They're about to loose their location-based heuristic and we don't want them to
	 * start using LastRenderTime heuristic for a few seconds until they are garbage collected!
	 *
	 * @param PrimitiveData		Our data structure for the spawned primitive that is being detached.
	 */
	void	SetInstanceRemovedTimestamp( FSpawnedPrimitiveData& PrimitiveData );

	void	DumpTextureGroupStats( UBOOL bDetailedStats );

	/**
	 * Prints out detailed information about streaming textures that has a name that contains the given string.
	 * Triggered by the InvestigateTexture exec command.
	 *
	 * @param InvestigateTextureName	Partial name to match textures against
	 */
	void	InvestigateTexture( const FString& InvestigateTextureName );

	void	DumpTextureInstances( const UPrimitiveComponent* Primitive, FSpawnedPrimitiveData& PrimitiveData, UTexture2D* Texture2D );

	/** Next sync, dump texture group stats. */
	UBOOL	bTriggerDumpTextureGroupStats;

	/** Whether to the dumped texture group stats should contain extra information. */
	UBOOL	bDetailedDumpTextureGroupStats;

	/** Next sync, dump all information we have about a certain texture. */
	UBOOL	bTriggerInvestigateTexture;

	/** Name of a texture to investigate. Can be partial name. */
	FString	InvestigateTextureName;

	/** Async work for calculating priorities for all textures. */
	FAsyncTask<FAsyncTextureStreaming>*	AsyncWork;

	/** New textures, before they've been added to the thread-safe container. */
	TArray<UTexture2D*>		PendingStreamingTextures;

	struct FPendingPrimitiveType
	{
		FPendingPrimitiveType( EDynamicPrimitiveType InDynamicType, UBOOL bInShouldTrack )
		:	DynamicType( InDynamicType)
		,	bShouldTrack( bInShouldTrack )
		{
		}
		EDynamicPrimitiveType DynamicType;
		UBOOL bShouldTrack;
	};
	/** Textures on newly spawned primitives, before they've been added to the thread-safe container. */
	TMap<const UPrimitiveComponent*,FPendingPrimitiveType>	PendingSpawnedPrimitives;

	/** New levels, before they've been added to the thread-safe container. */
	TArray<class ULevel*>	PendingLevels;

	/** Stages [0,N-2] is non-threaded data collection, Stage N-1 is wait-for-AsyncWork-and-finalize. */
	INT						ProcessingStage;

	/** Total number of processing stages (N). */
	INT						NumTextureProcessingStages;

	/** Maximum amount of temp memory used for streaming, at any given time. */
	INT						MaxTempMemoryUsed;

	/** Whether we're using the new priority-based streaming system or not. */
	UBOOL					bUsePriorityStreaming;

	/** Whether we support the TogglePriorityStreaming exec command to switch streaming system at run-time. */
	UBOOL					bAllowSwitchingStreamingSystem;

	/** Whether to support texture instance streaming for dynamic (movable/spawned) objects. */
	UBOOL					bUseDynamicStreaming;

	FLOAT					BoostPlayerTextures;

	/** Array of texture streaming objects to use during update. */
	TArray<FStreamingHandlerTextureBase*> TextureStreamingHandlers;

	/** Remaining ticks to suspend texture requests for. Used to sync up with async memory allocations. */
	INT	RemainingTicksToSuspendActivity;

	/** Fudge factor used to balance memory allocations/ use. */
	FLOAT FudgeFactor;
	/** Fudge factor rate of change. */
	FLOAT FudgeFactorRateOfChange;
	/** Last time NotifyLevelChange was called. */
	DOUBLE LastLevelChangeTime;
	/** Whether to use MinRequestedMipsToConsider limit. */
	UBOOL bUseMinRequestLimit;
	/** Time of last full iteration over all textures. */
	FLOAT LastFullIterationTime;
	/** Time of current iteration over all textures. */
	FLOAT CurrentFullIterationTime;

	// Config variables used by the new streaming system.
	/** Amount of memory to leave free in the texture pool. */
	INT MemoryMargin;

	// Config variables used by the old streaming system.
	/** Limit for change of fudge factor range to avoid hysteresis. */
	INT MemoryHysteresisLimit;
	/** Determines when to fiddle with fudge factor to stream out miplevels. */
	INT	MemoryDropMipLevelsLimit;
	/** Determines when to start disallowing texture miplevel increases. */
	INT MemoryStopIncreasingLimit;
	/** Limit when stop issuing new streaming requests altogether. */
	INT	MemoryStopStreamingLimit;
	/** Minimum value of fudge factor. A fudge factor of 1 means neutral though with sufficient memory we can prefetch textures. */
	FLOAT MinFudgeFactor;
	/** Rate of change to use when increasing fudge factor */
	FLOAT FudgeFactorIncreaseRateOfChange;
	/** Rate of change to use when decreasing fudge factor */
	FLOAT FudgeFactorDecreaseRateOfChange;
	/** 
	 * Minimum number of requested mips at which texture stream-in request is still going to be considered. 
	 * This is used by the texture priming code to prioritize large request for higher miplevels before smaller
	 * ones for background textures to avoid seeking and texture popping.
	 */
	INT MinRequestedMipsToConsider;
	/** Minimum amount of time in seconds to guarantee MinRequestedMipsToConsider to be respected. */
	FLOAT MinTimeToGuaranteeMinMipCount;
	/** Maximum amount of time in seconds to guarantee MinRequestedMipsToConsider to be respected. */
	FLOAT MaxTimeToGuaranteeMinMipCount;

	// Config variables used by both new and old streaming system
	/** Minimum number of bytes to evict when we need to stream out textures because of a failed allocation. */
	INT MinEvictSize;

	/** If set, UpdateResourceStreaming() will only process this texture. */
	UTexture2D* IndividualStreamingTexture;

	// Stats we need to keep across frames as we only iterate over a subset of textures.

	/** Number of streaming textures */
	DWORD NumStreamingTextures;
	/** Number of requests in cancelation phase. */
	DWORD NumRequestsInCancelationPhase;
	/** Number of requests in mip update phase. */
	DWORD NumRequestsInUpdatePhase;
	/** Number of requests in mip finalization phase. */
	DWORD NumRequestsInFinalizePhase;
	/** Size ot all intermerdiate textures in flight. */
	DWORD TotalIntermediateTexturesSize;
	/** Number of intermediate textures in flight. */
	DWORD NumIntermediateTextures;
	/** Size of all streaming testures. */
	DWORD TotalStreamingTexturesSize;
	/** Maximum size of all streaming textures. */
	DWORD TotalStreamingTexturesMaxSize;
	/** Total number of bytes in memory, for all streaming lightmap textures. */
	DWORD TotalLightmapMemorySize;
	/** Total number of bytes on disk, for all streaming lightmap textures. */
	DWORD TotalLightmapDiskSize;
	/** Number of mip count increase requests in flight. */
	DWORD TotalMipCountIncreaseRequestsInFlight;
	/** Total number of bytes in memory, if all textures were streamed perfectly with a 1.0 fudge factor. */
	DWORD TotalOptimalWantedSize;
	/** Total number of bytes using StaticTexture heuristics, currently in memory. */
	DWORD TotalStaticTextureHeuristicSize;
	/** Total number of bytes using DynmicTexture heuristics, currently in memory. */
	DWORD TotalDynamicTextureHeuristicSize;
	/** Total number of bytes using LastRenderTime heuristics, currently in memory. */
	DWORD TotalLastRenderHeuristicSize;
	/** Total number of bytes using ForcedIntoMemory heuristics, currently in memory. */
	DWORD TotalForcedHeuristicSize;

	/** Unmodified texture pool size, in bytes, as specified in the .ini file. */
	INT OriginalTexturePoolSize;

	/** Whether to collect, and optionally report, texture stats for the next run. */
	UBOOL   bCollectTextureStats;
	UBOOL   bReportTextureStats;
	TArray<FString> TextureStatsReport;

	/** Optional string to match against the texture names when collecting stats. */
	FString CollectTextureStatsName;

	/** Whether texture streaming is paused or not. When paused, it won't stream any textures in or out. */
	UBOOL bPauseTextureStreaming;

#if STATS
	/**
	 * Ring buffer containing all latency samples we keep track of, as measured in seconds from the time the streaming system
	 * detects that a change is required until the data has been streamed in and the new texture is ready to be used.
	 */
	FLOAT LatencySamples[NUM_LATENCYSAMPLES];
	/** Number of latency samples that have been filled in. Will stop counting when it reaches NUM_LATENCYSAMPLES. */
	INT NumLatencySamples;
	/** Current sample index in the ring buffer. */
	INT LatencySampleIndex;
	/** Average of all latency samples in the ring buffer, in seconds. */
	FLOAT LatencyAverage;
	/** Maximum latency measured since the start of the game.  */
	FLOAT LatencyMaximum;

	/**
	 * Updates the streaming latency STAT for a texture.
	 *
	 * @param Texture		Texture to update for
	 * @param WantedMips	Number of desired mip-levels for the texture
	 * @param bInFlight		Whether the texture is currently being streamed
	 */
	void UpdateLatencyStats( UTexture2D* Texture, INT WantedMips, UBOOL bInFlight );

	/** Total time taken for each processing stage. */
	TArray<DOUBLE> StreamingTimes;
#endif

#if STATS_FAST
	DWORD MaxStreamingTexturesSize;
	DWORD MaxOptimalTextureSize;
	INT MaxStreamingOverBudget;
	DWORD MaxTexturePoolAllocatedSize;
	DWORD MinLargestHoleSize;
	DWORD MaxNumWantingTextures;
#endif
};

/*-----------------------------------------------------------------------------
	Texture streaming handler.
-----------------------------------------------------------------------------*/

/**
 * Base of texture streaming handler functionality.
 */
struct FStreamingHandlerTextureBase
{
	/**
	 * Returns mip count wanted by this handler for the passed in texture. 
	 * 
	 * @param	StreamingManager	Streaming manager
	 * @param	Streaming Texture	Texture to determine wanted mip count for
	 * @param	PerfectWantedMips	[out] Desired number of miplevels if the fudge factor was 1.0, or INDEX_NONE if not handled or STATS isn't defined.
	 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
	 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
	 */
	virtual INT GetWantedMips( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, INT& OptimalWantedMips, FLOAT& MinDistance ) = 0;
};

/**
 * Static texture streaming handler. Used to stream textures on static level geometry.
 */
struct FStreamingHandlerTextureStatic : public FStreamingHandlerTextureBase
{
	/**
	 * Returns mip count wanted by this handler for the passed in texture. 
	 * 
	 * @param	StreamingManager	Streaming manager
	 * @param	Streaming Texture	Texture to determine wanted mip count for
	 * @param	PerfectWantedMips	[out] Desired number of miplevels if the fudge factor was 1.0, or INDEX_NONE if not handled or STATS isn't defined.
	 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
	 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
	 */
	virtual INT GetWantedMips( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, INT& OptimalWantedMips, FLOAT& MinDistance );
	/**
	 * Returns mip count wanted by this handler for the passed in texture.
	 * New version for the priority system, using SIMD and no fudge factor.
	 * 
	 * @param	StreamingManager	Streaming manager
	 * @param	Streaming Texture	Texture to determine wanted mip count for
	 * @param	PerfectWantedMips	[out] Desired number of miplevels if the fudge factor was 1.0, or INDEX_NONE if not handled or STATS isn't defined.
	 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
	 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
	 */
	INT GetWantedMips2( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, INT& OptimalWantedMips, FLOAT& MinDistance );
};

/**
 * Streaming handler that bases its decision on the last render time.
 */
struct FStreamingHandlerTextureLastRender : public FStreamingHandlerTextureBase
{
	/**
	 * Returns mip count wanted by this handler for the passed in texture. 
	 * 
	 * @param	StreamingManager	Streaming manager
	 * @param	Streaming Texture	Texture to determine wanted mip count for
	 * @param	PerfectWantedMips	[out] Desired number of miplevels if the fudge factor was 1.0, or INDEX_NONE if not handled or STATS isn't defined.
	 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
	 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
	 */
	virtual INT GetWantedMips( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, INT& OptimalWantedMips, FLOAT& MinDistance );
};

/**
 *	Streaming handler that bases its decision on the bForceMipStreaming flag in PrimitiveComponent.
 */
struct FStreamingHandlerTextureLevelForced : public FStreamingHandlerTextureBase
{
	/**
	 * Returns mip count wanted by this handler for the passed in texture. 
	 * 
	 * @param	StreamingManager	Streaming manager
	 * @param	Streaming Texture	Texture to determine wanted mip count for
	 * @param	PerfectWantedMips	[out] Desired number of miplevels if the fudge factor was 1.0, or INDEX_NONE if not handled or STATS isn't defined.
	 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
	 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
	 */
	virtual INT GetWantedMips( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, INT& OptimalWantedMips, FLOAT& MinDistance );
	/**
	 * Returns mip count wanted by this handler for the passed in texture. 
	 * New version for the priority system, using SIMD and no fudge factor.
	 * 
	 * @param	StreamingManager	Streaming manager
	 * @param	Streaming Texture	Texture to determine wanted mip count for
	 * @param	PerfectWantedMips	[out] Desired number of miplevels if the fudge factor was 1.0, or INDEX_NONE if not handled or STATS isn't defined.
	 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
	 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
	 */
	INT GetWantedMips2( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, INT& PerfectWantedMips, FLOAT& MinDistance );
};
