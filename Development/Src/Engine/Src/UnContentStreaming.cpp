/*=============================================================================
	UnContentStreaming.cpp: Implementation of content streaming classes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#include "EngineDecalClasses.h"

/*-----------------------------------------------------------------------------
	Stats.
-----------------------------------------------------------------------------*/

//@DEBUG:
// Set to 1 to log all dynamic component notifications
#define STREAMING_LOG_DYNAMIC		0
// Set to 1 to log when we change a view
#define STREAMING_LOG_VIEWCHANGES	0
// Set to 1 to log when levels are added/removed
#define STREAMING_LOG_LEVELS		0
// Set to 1 to log textures that are canceled by CancelForcedTextures()
#define STREAMING_LOG_CANCELFORCED	0

STAT_MAKE_AVAILABLE_FAST(STAT_TexturePool_LargestHole);

/** Streaming stats */
DECLARE_STATS_GROUP(TEXT("Streaming"),STATGROUP_Streaming);
DECLARE_STATS_GROUP(TEXT("StreamingDetails"),STATGROUP_StreamingDetails);

DECLARE_CYCLE_STAT(TEXT("Game Thread Update Time"),STAT_GameThreadUpdateTime,STATGROUP_Streaming);
DECLARE_MEMORY_STAT2_FAST(TEXT("Pool Memory Used"), STAT_TexturePoolAllocatedSize, STATGROUP_Streaming, MCR_TexturePool1, FALSE);
DECLARE_MEMORY_STAT2_FAST(TEXT("Textures In Memory, Target"),STAT_OptimalTextureSize,STATGROUP_Streaming,MCR_TexturePool1,TRUE);
DECLARE_MEMORY_STAT2_FAST(TEXT("Textures In Memory, Current"),STAT_StreamingTexturesSize,STATGROUP_Streaming,MCR_TexturePool1,TRUE);
DECLARE_MEMORY_STAT2(TEXT("Textures On Disk"),STAT_StreamingTexturesMaxSize,STATGROUP_Streaming,MCR_TexturePool1,TRUE);
DECLARE_MEMORY_STAT2_FAST(TEXT("Over Budget"),STAT_StreamingOverBudget,STATGROUP_Streaming,MCR_TexturePool1,TRUE);
DECLARE_MEMORY_STAT2_FAST(TEXT("Under Budget"),STAT_StreamingUnderBudget,STATGROUP_StreamingDetails,MCR_TexturePool1,TRUE);
DECLARE_DWORD_ACCUMULATOR_STAT_FAST(TEXT("Num Wanting Textures"),STAT_NumWantingTextures,STATGROUP_Streaming);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Streaming Textures"),STAT_StreamingTextures,STATGROUP_Streaming);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Streaming Fudge Factor"),STAT_StreamingFudgeFactor,STATGROUP_Streaming);

DECLARE_CYCLE_STAT(TEXT("Rendering Thread Update Time"),STAT_RenderingThreadUpdateTime,STATGROUP_StreamingDetails);
DECLARE_CYCLE_STAT(TEXT("Rendering Thread Finalize Time"),STAT_RenderingThreadFinalizeTime,STATGROUP_StreamingDetails);

DECLARE_MEMORY_STAT2(TEXT("Static Textures In Memory"),STAT_TotalStaticTextureHeuristicSize,STATGROUP_StreamingDetails,MCR_TexturePool1,FALSE);
DECLARE_MEMORY_STAT2(TEXT("Dynamic Textures In Memory"),STAT_TotalDynamicHeuristicSize,STATGROUP_StreamingDetails,MCR_TexturePool1,FALSE);
DECLARE_MEMORY_STAT2(TEXT("LastRenderTime Textures In Memory"),STAT_TotalLastRenderHeuristicSize,STATGROUP_StreamingDetails,MCR_TexturePool1,FALSE);
DECLARE_MEMORY_STAT2(TEXT("Forced Textures In Memory"),STAT_TotalForcedHeuristicSize,STATGROUP_StreamingDetails,MCR_TexturePool1,FALSE);

DECLARE_MEMORY_STAT2(TEXT("Lightmaps In Memory"),STAT_LightmapMemorySize,STATGROUP_StreamingDetails,MCR_TexturePool1,FALSE);
DECLARE_MEMORY_STAT2(TEXT("Lightmaps On Disk"),STAT_LightmapDiskSize,STATGROUP_StreamingDetails,MCR_TexturePool1,FALSE);
DECLARE_MEMORY_STAT2(TEXT("Intermediate Textures Size"),STAT_IntermediateTexturesSize,STATGROUP_StreamingDetails,MCR_TexturePool1,TRUE);
DECLARE_MEMORY_STAT2(TEXT("Textures Streamed In, Frame"),STAT_RequestSizeCurrentFrame,STATGROUP_StreamingDetails,MCR_TexturePool1,TRUE);
DECLARE_MEMORY_STAT2(TEXT("Textures Streamed In, Total"),STAT_RequestSizeTotal,STATGROUP_StreamingDetails,MCR_TexturePool1,TRUE);
DECLARE_MEMORY_STAT2(TEXT("Lightmaps Streamed In, Total"),STAT_LightmapRequestSizeTotal,STATGROUP_StreamingDetails,MCR_TexturePool1,FALSE);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Intermediate Textures"),STAT_IntermediateTextures,STATGROUP_StreamingDetails);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Requests In Cancelation Phase"),STAT_RequestsInCancelationPhase,STATGROUP_StreamingDetails);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Requests In Update Phase"),STAT_RequestsInUpdatePhase,STATGROUP_StreamingDetails);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Requests in Finalize Phase"),STAT_RequestsInFinalizePhase,STATGROUP_StreamingDetails);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Streaming Latency, Average (sec)"),STAT_StreamingLatency,STATGROUP_StreamingDetails);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Streaming Bandwidth, Average (MB/s)"),STAT_StreamingBandwidth,STATGROUP_StreamingDetails);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Growing Reallocations"),STAT_GrowingReallocations,STATGROUP_StreamingDetails);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Shrinking Reallocations"),STAT_ShrinkingReallocations,STATGROUP_StreamingDetails);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Full Reallocations"),STAT_FullReallocations,STATGROUP_StreamingDetails);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Failed Reallocations"),STAT_FailedReallocations,STATGROUP_StreamingDetails);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Panic Defragmentations"),STAT_PanicDefragmentations,STATGROUP_StreamingDetails);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Texture Instances"),STAT_NumStreamingTextureInstances,STATGROUP_StreamingDetails);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Lightmap Instances"),STAT_NumStreamingLightmapInstances,STATGROUP_StreamingDetails);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Dynamic Streaming Total Time (sec)"),STAT_DynamicStreamingTotal,STATGROUP_StreamingDetails);

/** Accumulated total time spent on dynamic primitives, in seconds. */
DOUBLE GStreamingDynamicPrimitivesTime = 0.0;


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

/** Global streaming manager */
FStreamingManagerCollection* GStreamingManager;

/** Collection of views that need to be taken into account for streaming. */
TArray<FStreamingViewInfo> FStreamingManagerBase::CurrentViewInfos;

/** Pending views. Emptied every frame. */
TArray<FStreamingViewInfo> FStreamingManagerBase::PendingViewInfos;

/** Views that stick around for a while. Override views are ignored if no movie is playing. */
TArray<FStreamingViewInfo> FStreamingManagerBase::LastingViewInfos;

/** Collection of view locations that will be added at the next call to AddViewInformation. */
TArray<FStreamingManagerBase::FSlaveLocation> FStreamingManagerBase::SlaveLocations;

/** Set when Tick() has been called. The first time a new view is added, it will clear out all old views. */
UBOOL FStreamingManagerBase::bPendingRemoveViews = FALSE;

/** The lightmap used by the currently selected component (toggledebugcamera), if it's a static mesh component. */
extern FLightMap2D* GDebugSelectedLightmap;

/** Whether the texture pool size has been artificially lowered for testing. */
UBOOL GIsOperatingWithReducedTexturePool = FALSE;

/** Smaller value will stream out lightmaps more aggressively. */
FLOAT GLightmapStreamingFactor = 0.03f;

/** Smaller value will stream out shadowmaps more aggressively. */
FLOAT GShadowmapStreamingFactor = 0.09f;

/** For testing, finding useless textures or special demo purposes. If TRUE, textures will never be streamed out (but they can be GC'd). 
* Caution: this only applies to unlimited texture pools (i.e. not consoles)
*/
UBOOL GNeverStreamOutTextures = FALSE;

/** Bias applied for ALL mip calculations. 1.0 = standard logic. < 1.0 = less texture resultion, > 1.0 more texture resolution.
 * Caution: this is probably not useful with limited texture pools, which try to balance this automatically, and dynamically.
*/
FLOAT GOverallTextureStreamingBias = 1.0f;

/** Minimum allowed distance squared from the camera to the target mesh for the purposes of streaming.
 * If this is less than one, the normal rules apply and when the camera is less than 1 unit from the
 * the mesh, ALL mip levels are request.
 * For tweaking via GOverallTextureStreamingBias, it is helpful to have this number larger than 1 so 
 * that control of all textures is possible. 
 *
*/
FLOAT GMinimumStreamingCameraToMeshDistance = 0.0f;

/** Streaming priority: Linear distance factor from 0 to MAX_STREAMINGDISTANCE. */
#define MAX_STREAMINGDISTANCE	10000.0f
#define MAX_MIPDELTA			5.0f
#define MAX_LASTRENDERTIME		90.0f

/** For debugging purposes: Whether to consider the time factor when streaming. Turning it off is easier for debugging. */
UBOOL GStreamWithTimeFactor		= TRUE;

extern UPrimitiveComponent* GDebugSelectedComponent;

#if STATS
/** Ringbuffer of bandwidth samples for streaming in mip-levels (MB/sec). */
FLOAT FStreamingManagerTexture::BandwidthSamples[NUM_BANDWIDTHSAMPLES];
/** Number of bandwidth samples that have been filled in. Will stop counting when it reaches NUM_BANDWIDTHSAMPLES. */
INT FStreamingManagerTexture::NumBandwidthSamples = 0;
/** Current sample index in the ring buffer. */
INT FStreamingManagerTexture::BandwidthSampleIndex = 0;
/** Average of all bandwidth samples in the ring buffer, in MB/sec. */
FLOAT FStreamingManagerTexture::BandwidthAverage = 0.0f;
/** Maximum bandwidth measured since the start of the game.  */
FLOAT FStreamingManagerTexture::BandwidthMaximum = 0.0f;
/** Minimum bandwidth measured since the start of the game.  */
FLOAT FStreamingManagerTexture::BandwidthMinimum = 0.0f;
#endif

/**
 * Helper function to flush resource streaming from within Core project.
 */
void FlushResourceStreaming()
{
	RETURN_IF_EXIT_REQUESTED;
	GStreamingManager->BlockTillAllRequestsFinished();
}

/**
 * Helper function to map screen texels to a mip level, includes the global bias
 */
FORCEINLINE INT ScreenTexelsToMipLevel(FLOAT ScreenSizeInTexels)
{
	return 1 + appCeilLogTwo( appTrunc(ScreenSizeInTexels * GOverallTextureStreamingBias) );
}

/**
 * Helper function to clamp the mesh to camera distance
 */
FORCEINLINE FLOAT ClampMeshToCameraDistanceSquared(FLOAT MeshToCameraDistanceSquared)
{
	return Max<FLOAT>(MeshToCameraDistanceSquared,GMinimumStreamingCameraToMeshDistance);
}

/** Maximum number of bytes to change per frame */
#define MAX_PER_FRAME_REQUEST_LIMIT (3 * 1024 * 1024)

/** Number of ticks for full iteration over texture list. */
#define NUM_TICKS_FOR_FULL_ITERATION 10


/*-----------------------------------------------------------------------------
	FStreamingTexture, the streaming system's version of UTexture2D.
-----------------------------------------------------------------------------*/

enum ETextureStreamingType
{
	StreamType_Static,
	StreamType_Dynamic,
	StreamType_Forced,
	StreamType_LastRenderTime,
	StreamType_Orphaned,
	StreamType_Other,
};

static const TCHAR* GStreamTypeNames[] =
{
	TEXT("Static"),
	TEXT("Dynamic"),
	TEXT("Forced"),
	TEXT("LastRenderTime"),
	TEXT("Orphaned"),
	TEXT("Other"),
};

/** Self-contained structure to manage a streaming texture, possibly on a separate thread. */
struct FStreamingTexture
{
	FStreamingTexture( UTexture2D* InTexture )
	{
		Texture = InTexture;
		WantedMips = InTexture->ResidentMips;
		MipCount = InTexture->Mips.Num();
		STAT_FAST( PerfectWantedMips = InTexture->ResidentMips );
		STAT_FAST( MostResidentMips = InTexture->ResidentMips );
		LODGroup = (TextureGroup) InTexture->LODGroup;
		NumMipTailLevels = Max(0, InTexture->Mips.Num() - InTexture->MipTailBaseIdx);
		ForceLoadRefCount = 0;
		bIsStreamingLightmap = IsStreamingLightmap( Texture );
		bUsesStaticHeuristics = FALSE;
		bUsesDynamicHeuristics = FALSE;
		bUsesLastRenderHeuristics = FALSE;
		bUsesForcedHeuristics = FALSE;
		bUsesOrphanedHeuristics = FALSE;
		bNeedPrimitiveUpdate = FALSE;
		BoostFactor = 1.0f;
		InstanceRemovedTimestamp = -FLT_MAX;
		LastRenderTimeRefCountTimestamp = -FLT_MAX;
		LastRenderTimeRefCount = 0;

		for ( INT MipIndex=1; MipIndex <= MAX_TEXTURE_MIP_COUNT; ++MipIndex )
		{
			TextureSizes[MipIndex - 1] = Texture->CalcTextureMemorySize( Min(MipIndex, MipCount) );
		}

		UpdateCachedInfo();
	}

	/** Reinitializes the cached variables from UTexture2D. */
	void UpdateCachedInfo( )
	{
		ResidentMips = Texture->ResidentMips;
		RequestedMips = Texture->RequestedMips;
		MinAllowedMips = 1;			//Min(Texture->ResidentMips, Texture->RequestedMips);
		MaxAllowedMips = MipCount;	//Max(Texture->ResidentMips, Texture->RequestedMips);
		STAT_FAST( MaxAllowedOptimalMips = MaxAllowedMips );
		STAT_FAST( MostResidentMips = Max(MostResidentMips, Texture->ResidentMips) );
		LastRenderTime = (GCurrentTime > Texture->Resource->LastRenderTime) ? FLOAT( GCurrentTime - Texture->Resource->LastRenderTime ) : 0.0f;
		MinDistance = MAX_STREAMINGDISTANCE;
		bForceFullyLoad = Texture->ShouldMipLevelsBeForcedResident() || (ForceLoadRefCount > 0);
		TextureLODBias = Texture->GetCachedLODBias();
		bInFlight = FALSE;
		bReadyForStreaming = IsStreamingTexture( Texture ) && IsReadyForStreaming( Texture );
		NumCinematicMipLevels = Texture->bUseCinematicMipLevels ? Texture->NumCinematicMipLevels : 0;
	}

	ETextureStreamingType GetStreamingType() const
	{
		if ( bUsesForcedHeuristics || bForceFullyLoad )
		{
			return StreamType_Forced;
		}
		else if ( bUsesDynamicHeuristics )
		{
			return StreamType_Dynamic;
		}
		else if ( bUsesStaticHeuristics )
		{
			return StreamType_Static;
		}
		else if ( bUsesOrphanedHeuristics )
		{
			return StreamType_Orphaned;
		}
		else if ( bUsesLastRenderHeuristics )
		{
			return StreamType_LastRenderTime;
		}
		return StreamType_Other;
	}

	/**
	 * Checks whether a UTexture2D is supposed to be streaming.
	 * @param Texture	Texture to check
	 * @return			TRUE if the UTexture2D is supposed to be streaming
	 */
	static UBOOL IsStreamingTexture( const UTexture2D* Texture2D )
	{
		return Texture2D && Texture2D->bIsStreamable && Texture2D->NeverStream == FALSE && Texture2D->Mips.Num() > GMinTextureResidentMipCount;
	}

	/**
	 * Checks whether a UTexture2D is ready for streaming.
	 *
	 * @param Texture	Texture to check
	 * @return			TRUE if the UTexture2D is ready to be streamed in or out
	 */
	static UBOOL IsReadyForStreaming( UTexture2D* Texture )
	{
		return Texture->IsReadyForStreaming();
	}

	/**
	 * Checks whether a UTexture2D is a streaming lightmap or shadowmap.
	 *
	 * @param Texture	Texture to check
	 * @return			TRUE if the UTexture2D is a streaming lightmap or shadowmap
	 */
	static UBOOL IsStreamingLightmap( UTexture2D* Texture )
	{
		ULightMapTexture2D* Lightmap = Cast<ULightMapTexture2D>(Texture);
		UShadowMapTexture2D* Shadowmap = Cast<UShadowMapTexture2D>(Texture);
		if ( Lightmap && Lightmap->LightmapFlags & LMF_Streamed )
		{
			return TRUE;
		}
		else if ( Shadowmap && Shadowmap->ShadowmapFlags & SMF_Streamed )
		{
			return TRUE;
		}
		return FALSE;
	}

	/**
	 * Returns the amount of memory used by the texture given a specified number of mip-maps, in bytes.
	 *
	 * @param MipCount	Number of mip-maps to account for
	 * @return			Total amount of memory used for the specified mip-maps, in bytes
	 */
	INT GetSize( INT MipCount ) const
	{
		check( MipCount >= 0 && MipCount <= MAX_TEXTURE_MIP_COUNT );
		return TextureSizes[ MipCount - 1 ];
	}

	/**
	 * Calculates a priority value for the texture. Higher value means more important.
	 * @return		Priority value
	 */
	FLOAT CalcPriority( )
	{
//		FLOAT DistanceFactor = 1.0f - Clamp( MinDistance, 1.0f, MAX_STREAMINGDISTANCE ) / MAX_STREAMINGDISTANCE;
		FLOAT DistanceFactor = 1 - appSqrt( Clamp( MinDistance, 1.0f, MAX_STREAMINGDISTANCE ) / MAX_STREAMINGDISTANCE );
//		FLOAT MipFactor = WantedMips - ResidentMips;
		FLOAT MipFactor = FLOAT(WantedMips) / FLOAT(MAX_TEXTURE_MIP_COUNT);
		FLOAT TimeFactor = GStreamWithTimeFactor ? (Clamp( LastRenderTime, 1.0f, MAX_LASTRENDERTIME ) / MAX_LASTRENDERTIME) : 0.0f;
		FLOAT Priority = MipFactor + DistanceFactor * (1.0f - TimeFactor * 0.5f) + bForceFullyLoad * 100.0f;
		return Priority;
	}

	/**
	 * Not thread-safe: Sets the streaming index in the corresponding UTexture2D.
	 * @param ArrayIndex	Index into the FStreamingManagerTexture::StreamingTextures array
	 */
	void SetStreamingIndex( INT ArrayIndex )
	{
		Texture->StreamingIndex = ArrayIndex;
	}

	/** Texture to manage. */
	UTexture2D*		Texture;

	/** Cached number of mip-maps in the UTexture2D mip array (including the base mip) */
	INT				MipCount;
	/** Cached number of mip-maps in memory (including the base mip) */
	INT				ResidentMips;
	/** Cached number of mip-maps requested by the streaming system. */
	INT				RequestedMips;
	/** Cached number of mip-maps we would like to have in memory. */
	INT				WantedMips;
	/** Largest screen size occupied by this texture, counting dynamic primitives only. Call ScreenTexelsToMipLevel() to get DynamicWantedMips. */
	FLOAT			DynamicScreenSize;
#if STATS_FAST
	/** Legacy: Same as WantedMips, but not clamped by fudge factor. */
	INT				PerfectWantedMips;
	/** Same as MaxAllowedMips, but not clamped by the -reducepoolsize command line option. */
	INT				MaxAllowedOptimalMips;
	/** Most number of mip-levels this texture has ever had resident in memory. */
	INT				MostResidentMips;
#endif
	/** Minimum number of mip-maps that this texture must keep in memory. */
	INT				MinAllowedMips;
	/** Maximum number of mip-maps that this texture can have in memory. */
	INT				MaxAllowedMips;
	/** Cached memory sizes for each possible mipcount. */
	INT				TextureSizes[MAX_TEXTURE_MIP_COUNT + 1];
	/** Ref-count for how many ULevels want this texture fully-loaded. */
	INT				ForceLoadRefCount;

	/** Cached texture group. */
	TextureGroup	LODGroup;
	/** Cached LOD bias. */
	INT				TextureLODBias;
	/** Cached number of mip-maps in the mip-tail (on Xbox). */
	INT				NumMipTailLevels;
	/** Cached number of cinematic (high-resolution) mip-maps. Normally not streamed in, unless the texture is forcibly fully loaded. */
	INT				NumCinematicMipLevels;

	/** Last time this texture was rendered, 0-based from GStartTime. */
	FLOAT			LastRenderTime;
	/** Distance to the closest instance of this texture, in world space units. Defaults to a specific value for non-staticmesh textures. */
	FLOAT			MinDistance;
	/** Squared distance to the closest dynamic instance of this texture. */
	FLOAT			DynamicMinDistanceSq;
	/** If non-zero, the most recent time an instance location was removed for this texture. */
	DOUBLE			InstanceRemovedTimestamp;

	/** Set to GCurrentTime every time LastRenderTimeRefCount is modified. */
	DOUBLE			LastRenderTimeRefCountTimestamp;
	/** Current number of instances that need LRT heuristics for this texture. */
	INT				LastRenderTimeRefCount;

	/**
	 * Temporary boost of the streaming distance factor.
	 * This factor is automatically reset to 1.0 after it's been used for mip-calculations.
	 */
	FLOAT			BoostFactor;

	/** Whether the texture should be forcibly fully loaded. */
	DWORD			bForceFullyLoad : 1;
	/** Whether the texture is ready to be streamed in/out (cached from IsReadyForStreaming()). */
	DWORD			bReadyForStreaming : 1;
	/** Whether the texture is currently being streamed in/out. */
	DWORD			bInFlight : 1;
	/** Whether this is a streaming lightmap or shadowmap (cached from IsStreamingLightmap()). */
	DWORD			bIsStreamingLightmap : 1;
	/** Whether this texture uses StaticTexture heuristics. */
	DWORD			bUsesStaticHeuristics : 1;
	/** Whether this texture uses DynamicTexture heuristics. */
	DWORD			bUsesDynamicHeuristics : 1;
	/** Whether this texture uses LastRenderTime heuristics. */
	DWORD			bUsesLastRenderHeuristics : 1;
	/** Whether this texture uses ForcedIntoMemory heuristics. */
	DWORD			bUsesForcedHeuristics : 1;
	/** Whether this texture uses the OrphanedTexture heuristics. */
	DWORD			bUsesOrphanedHeuristics : 1;
	/** Whether this texture has been replaced in a material and all primitives using it call NotifyPrimitiveUpdated(). */
	DWORD			bNeedPrimitiveUpdate : 1;
};


/*-----------------------------------------------------------------------------
	Texture tracking.
-----------------------------------------------------------------------------*/

/** Turn on ENABLE_TEXTURE_TRACKING and setup GTrackedTextures to track specific textures through the streaming system. */
#define ENABLE_TEXTURE_TRACKING !FINAL_RELEASE
#define ENABLE_TEXTURE_LOGGING 1
#if ENABLE_TEXTURE_TRACKING
struct FTrackedTextureEvent
{
	FTrackedTextureEvent( TCHAR* InTextureName=NULL )
	:	TextureName(InTextureName)
	,	NumResidentMips(0)
	,	NumRequestedMips(0)
	,	WantedMips(0)
	,	DynamicWantedMips(0)
	,	StreamingStatus(0)
	,	Timestamp(0.0f)
	,	StreamType(StreamType_Other)
	,	BoostFactor(1.0f)
	{
	}
	FTrackedTextureEvent( const FString& InTextureNameString )
	:	NumResidentMips(0)
	,	NumRequestedMips(0)
	,	WantedMips(0)
	,	DynamicWantedMips(0)
	,	StreamingStatus(0)
	,	Timestamp(0.0f)
	,	StreamType(StreamType_Other)
	,	BoostFactor(1.0f)
	{
		TextureName = new TCHAR[InTextureNameString.Len() + 1];
		appMemcpy( TextureName, *InTextureNameString, (InTextureNameString.Len() + 1)*sizeof(TCHAR) );
	}
	/** Partial name of the texture (not case-sensitive). */
	TCHAR*	TextureName;
	/** Number of mip-levels currently in memory. */
	INT		NumResidentMips;
	/** Number of mip-levels requested. */
	INT		NumRequestedMips;
	/** Number of wanted mips. */
	INT		WantedMips;
	/** Number of wanted mips, as calculated by the heuristics for dynamic primitives. */
	INT		DynamicWantedMips;
	/** Streaming status. */
	INT		StreamingStatus;
	/** Timestamp, in seconds from startup. */
	FLOAT	Timestamp;
	/** Which streaming heuristic this texture uses. */
	ETextureStreamingType	StreamType;
	/** Currently used boost factor for the streaming distance. */
	FLOAT	BoostFactor;
};
/** List of textures to track (using stristr for name comparison). */
TArray<FString> GTrackedTextureNames;
UBOOL GTrackedTexturesInitialized = FALSE;
#define NUM_TRACKEDTEXTUREEVENTS 512
FTrackedTextureEvent GTrackedTextureEvents[NUM_TRACKEDTEXTUREEVENTS];
INT GTrackedTextureEventIndex = -1;
TArray<FTrackedTextureEvent> GTrackedTextures;

/**
 * Initializes the texture tracking. Called when GTrackedTexturesInitialized is FALSE.
 */
void TrackTextureInit()
{
	if ( GConfig && GConfig->Num() > 0 )
	{
		GTrackedTextureNames.Empty();
		GTrackedTexturesInitialized = TRUE;
		GConfig->GetArray( TEXT("TextureTracking"), TEXT("TextureName"), GTrackedTextureNames, GEngineIni );
	}
}

/**
 * Adds a (partial) texture name to track in the streaming system and updates the .ini setting.
 *
 * @param TextureName	Partial name of a new texture to track (not case-sensitive)
 * @return				TRUE if the name was added
 */
UBOOL TrackTexture( const FString& TextureName )
{
	if ( GConfig && TextureName.Len() > 0 )
	{
		for ( INT TrackedTextureIndex=0; TrackedTextureIndex < GTrackedTextureNames.Num(); ++TrackedTextureIndex )
		{
			const FString& TrackedTextureName = GTrackedTextureNames(TrackedTextureIndex);
			if ( appStricmp(*TextureName, *TrackedTextureName) == 0 )
			{
				return FALSE;	
			}
		}

		GTrackedTextureNames.AddItem( *TextureName );
		GConfig->SetArray( TEXT("TextureTracking"), TEXT("TextureName"), GTrackedTextureNames, GEngineIni );
		return TRUE;
	}
	return FALSE;
}

/**
 * Removes a texture name from being tracked in the streaming system and updates the .ini setting.
 * The name must match an existing tracking name, but isn't case-sensitive.
 *
 * @param TextureName	Name of a new texture to stop tracking (not case-sensitive)
 * @return				TRUE if the name was added
 */
UBOOL UntrackTexture( const FString& TextureName )
{
	if ( GConfig && TextureName.Len() > 0 )
	{
		INT TrackedTextureIndex = 0;
		for ( ; TrackedTextureIndex < GTrackedTextureNames.Num(); ++TrackedTextureIndex )
		{
			const FString& TrackedTextureName = GTrackedTextureNames(TrackedTextureIndex);
			if ( appStricmp(*TextureName, *TrackedTextureName) == 0 )
			{
				break;
			}
		}
		if ( TrackedTextureIndex < GTrackedTextureNames.Num() )
		{
			GTrackedTextureNames.Remove( TrackedTextureIndex );
			GConfig->SetArray( TEXT("TextureTracking"), TEXT("TextureName"), GTrackedTextureNames, GEngineIni );

			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Lists all currently tracked texture names in the specified log.
 *
 * @param Ar			Desired output log
 * @param NumTextures	Maximum number of tracked texture names to output. Outputs all if NumTextures <= 0.
 */
void ListTrackedTextures( FOutputDevice& Ar, INT NumTextures )
{
	NumTextures = (NumTextures > 0) ? Min(NumTextures, GTrackedTextureNames.Num()) : GTrackedTextureNames.Num();
	for ( INT TrackedTextureIndex=0; TrackedTextureIndex < NumTextures; ++TrackedTextureIndex )
	{
		const FString& TrackedTextureName = GTrackedTextureNames(TrackedTextureIndex);
		Ar.Log( TrackedTextureName );
	}
	Ar.Logf( TEXT("%d textures are being tracked."), NumTextures );
}

/**
 * Checks a texture and tracks it if its name contains any of the tracked textures names (GTrackedTextureNames).
 *
 * @param Texture						Texture to check
 * @param bIsDestroying					Whether the texture is currently being destroyed
 * @param bEnableLogging				Whether changes to this texture should be logged
 * @param bForceMipLEvelsToBeResident	Whether all mip-levels in the texture are forced to be resident
 */
UBOOL TrackTextureEvent( FStreamingTexture* StreamingTexture, UTexture2D* Texture, UBOOL bIsDestroying, UBOOL bEnableLogging, UBOOL bForceMipLevelsToBeResident )
{
	// Initialize the tracking system, if necessary.
	if ( GTrackedTexturesInitialized == FALSE )
	{
		TrackTextureInit();
	}

	INT NumTrackedTextures = GTrackedTextureNames.Num();
	if ( NumTrackedTextures )
	{
		// See if it matches any of the texture names that we're tracking.
		FString TextureNameString = Texture->GetFullName();
		const TCHAR* TextureName = *TextureNameString;
		for ( INT TrackedTextureIndex=0; TrackedTextureIndex < NumTrackedTextures; ++TrackedTextureIndex )
		{
			const FString& TrackedTextureName = GTrackedTextureNames(TrackedTextureIndex);
			if ( appStristr(TextureName, *TrackedTextureName) != NULL )
			{
				if ( bEnableLogging )
				{
					// Find the last event for this particular texture.
					FTrackedTextureEvent* LastEvent = NULL;
					for ( INT LastEventIndex=0; LastEventIndex < GTrackedTextures.Num(); ++LastEventIndex )
					{
						FTrackedTextureEvent* Event = &GTrackedTextures(LastEventIndex);
						if ( appStrcmp(TextureName, Event->TextureName) == 0 )
						{
							LastEvent = Event;
							break;
						}
					}
					// Didn't find any recorded event?
					if ( LastEvent == NULL )
					{
						INT NewIndex = GTrackedTextures.AddItem(FTrackedTextureEvent(TextureNameString));
						LastEvent = &GTrackedTextures(NewIndex);
					}

					INT StreamingStatus		= Texture->PendingMipChangeRequestStatus.GetValue();
					INT WantedMips			= Texture->RequestedMips;
					INT DynamicWantedMips	= INDEX_NONE;
					FLOAT BoostFactor		= 1.0f;
					ETextureStreamingType StreamType = bForceMipLevelsToBeResident ? StreamType_Forced : StreamType_Other;
					if ( StreamingTexture )
					{
						WantedMips			= StreamingTexture->WantedMips;
						DynamicWantedMips	= ScreenTexelsToMipLevel( StreamingTexture->DynamicScreenSize );
						StreamType			= StreamingTexture->GetStreamingType();
						BoostFactor			= StreamingTexture->BoostFactor;
					}

					if ( LastEvent->NumResidentMips != Texture->ResidentMips ||
						 LastEvent->NumRequestedMips != Texture->RequestedMips ||
						 LastEvent->StreamingStatus != StreamingStatus ||
						 LastEvent->WantedMips != WantedMips ||
						 LastEvent->DynamicWantedMips != DynamicWantedMips ||
						 LastEvent->StreamType != StreamType ||
						 LastEvent->BoostFactor != BoostFactor ||
						 bIsDestroying )
					{
						GTrackedTextureEventIndex		= (GTrackedTextureEventIndex + 1) % NUM_TRACKEDTEXTUREEVENTS;
						FTrackedTextureEvent& NewEvent	= GTrackedTextureEvents[GTrackedTextureEventIndex];
						NewEvent.TextureName			= LastEvent->TextureName;
						NewEvent.NumResidentMips		= LastEvent->NumResidentMips	= Texture->ResidentMips;
						NewEvent.NumRequestedMips		= LastEvent->NumRequestedMips	= Texture->RequestedMips;
						NewEvent.WantedMips				= LastEvent->WantedMips			= WantedMips;
						NewEvent.DynamicWantedMips		= LastEvent->DynamicWantedMips	= DynamicWantedMips;
						NewEvent.StreamingStatus		= LastEvent->StreamingStatus	= StreamingStatus;
						NewEvent.Timestamp				= LastEvent->Timestamp			= FLOAT(appSeconds() - GStartTime);
						NewEvent.StreamType				= LastEvent->StreamType			= StreamType;
						NewEvent.BoostFactor			= LastEvent->BoostFactor		= BoostFactor;
						debugf( NAME_DevStreaming, TEXT("Texture: \"%s\", ResidentMips: %d/%d, RequestedMips: %d, WantedMips: %d, DynamicWantedMips: %d, StreamingStatus: %d, StreamType: %s, Boost: %.1f (%s)"),
							TextureName, LastEvent->NumResidentMips, Texture->Mips.Num(), bIsDestroying ? 0 : LastEvent->NumRequestedMips,
							LastEvent->WantedMips, LastEvent->DynamicWantedMips, LastEvent->StreamingStatus, GStreamTypeNames[StreamType],
							BoostFactor, bIsDestroying ? TEXT("DESTROYED") : TEXT("updated") );
					}
				}
				return TRUE;
			}
		}
	}
	return FALSE;
}
#else
UBOOL TrackTexture( const FString& /*TextureName*/ )
{
	return FALSE;
}
UBOOL UntrackTexture( const FString& /*TextureName*/ )
{
	return FALSE;
}
void ListTrackedTextures( FOutputDevice& /*Ar*/, INT /*NumTextures*/ )
{
}
UBOOL TrackTextureEvent( FStreamingTexture* StreamingTexture, UTexture2D* Texture, UBOOL bIsDestroying, UBOOL bEnableLogging, UBOOL bForceMipLevelsToBeResident )
{
	return FALSE;
}
#endif

/*-----------------------------------------------------------------------------
	Asynchronous texture streaming.
-----------------------------------------------------------------------------*/

#if STATS
struct FTextureStreamingStats
{
	FTextureStreamingStats( UTexture2D* InTexture2D, ETextureStreamingType InType, INT InResidentMips, INT InWantedMips, INT InMostResidentMips, INT InResidentSize, INT InWantedSize, INT InMaxSize, INT InMostResidentSize, FLOAT InBoostFactor )
		:	TextureName( InTexture2D->GetFullName() )
		,	SizeX( InTexture2D->SizeX )
		,	SizeY( InTexture2D->SizeY )
		,	NumMips( InTexture2D->Mips.Num() )
		,	LODBias( InTexture2D->GetCachedLODBias() )
		,	LastRenderTime( Clamp( InTexture2D->Resource ? (GCurrentTime - InTexture2D->Resource->LastRenderTime) : 1000000.0, 0.0, 1000000.0) )
		,	StreamType( InType )
		,	ResidentMips( InResidentMips )
		,	WantedMips( InWantedMips )
		,	MostResidentMips( InMostResidentMips )
		,	ResidentSize( InResidentSize )
		,	WantedSize( InWantedSize )
		,	MaxSize( InMaxSize )
		,	MostResidentSize( InMostResidentSize )
		,	BoostFactor( InBoostFactor )
	{
	}
	/** Mirror of UTexture2D::GetName() */
	FString					TextureName;
	/** Mirror of UTexture2D::SizeX */
	INT						SizeX;
	/** Mirror of UTexture2D::SizeY */
	INT						SizeY;
	/** Mirror of UTexture2D::Mips.Num() */
	INT						NumMips;
	/** Mirror of UTexture2D::GetCachedLODBias() */
	INT						LODBias;
	/** How many seconds since it was last rendered: GCurrentTime - UTexture2D::Resource->LastRenderTime */
	FLOAT					LastRenderTime;
	/** What streaming heuristics were primarily used for this texture. */
	ETextureStreamingType	StreamType;
	/** Number of resident mip levels. */
	INT						ResidentMips;
	/** Number of wanted mip levels. */
	INT						WantedMips;
	/** Most number of mip-levels this texture has ever had resident in memory. */
	INT						MostResidentMips;
	/** Number of bytes currently in memory. */
	INT						ResidentSize;
	/** Number of bytes we want in memory. */
	INT						WantedSize;
	/** Number of bytes we could potentially stream in. */
	INT						MaxSize;
	/** Most number of bytes this texture has ever had resident in memory. */
	INT						MostResidentSize;
	/** Temporary boost of the streaming distance factor. */
	FLOAT					BoostFactor;
};
#endif

/**
 * Helper struct for temporary information for one frame of processing texture streaming.
 */
struct FStreamingContext
{
	FStreamingContext( UBOOL bProcessEverything, UTexture2D* IndividualStreamingTexture, UBOOL bCollectTextureStats )
	{
		Reset( bProcessEverything, IndividualStreamingTexture, bCollectTextureStats );
	}

	/**
	 * Initializes all variables for the one frame.
	 * @param bProcessEverything			If TRUE, process all resources with no throttling limits
	 * @param IndividualStreamingTexture	A specific texture to be fully processed this frame, or NULL
	 * @param bInCollectTextureStats		Whether to fill in the TextureStats array this frame
	 */
	void Reset( UBOOL bProcessEverything, UTexture2D* IndividualStreamingTexture, UBOOL bInCollectTextureStats )
	{
		bCollectTextureStats							= bInCollectTextureStats;
		ThisFrameTotalRequestSize						= 0;
		ThisFrameTotalLightmapRequestSize				= 0;
		ThisFrameNumStreamingTextures					= 0;
		ThisFrameNumRequestsInCancelationPhase			= 0;
		ThisFrameNumRequestsInUpdatePhase				= 0;
		ThisFrameNumRequestsInFinalizePhase				= 0;
		ThisFrameTotalIntermediateTexturesSize			= 0;
		ThisFrameNumIntermediateTextures				= 0;
		ThisFrameTotalStreamingTexturesSize				= 0;
		ThisFrameTotalStreamingTexturesMaxSize			= 0;
		ThisFrameTotalLightmapMemorySize				= 0;
		ThisFrameTotalLightmapDiskSize					= 0;
		ThisFrameTotalMipCountIncreaseRequestsInFlight	= 0;
		ThisFrameOptimalWantedSize						= 0;
		ThisFrameTotalStaticTextureHeuristicSize		= 0;
		ThisFrameTotalDynamicTextureHeuristicSize		= 0;
		ThisFrameTotalLastRenderHeuristicSize			= 0;
		ThisFrameTotalForcedHeuristicSize				= 0;

		STAT( TextureStats.Empty() );

		// Available texture memory, if supported by RHI. This stat is async as the renderer allocates the memory in its own thread so we
		// only query once and roughly adjust the values as needed.
		AllocatedMemorySize	= INDEX_NONE;
		AvailableMemorySize	= INDEX_NONE;
		PendingMemoryAdjustment = INDEX_NONE;
		bRHISupportsMemoryStats = RHIGetTextureMemoryStats( AllocatedMemorySize, AvailableMemorySize, PendingMemoryAdjustment );

		// Update stats if supported.
		if( bRHISupportsMemoryStats )
		{
			// set total size for the pool (used to available)
			STAT(GStatManager.SetAvailableMemory(MCR_TexturePool1, AvailableMemorySize + AllocatedMemorySize));
			SET_DWORD_STAT_FAST(STAT_TexturePoolAllocatedSize, AllocatedMemorySize);
		}
		else
		{
			STAT(GStatManager.SetAvailableMemory(MCR_TexturePool1, 0));
			SET_DWORD_STAT_FAST(STAT_TexturePoolAllocatedSize,0);
		}

		if ( bProcessEverything )
		{
			MaxPerFrameRequestLimit = 0xffffffff;
			MaxTexturesToProcess = Max( 1, UTexture2D::GetNumStreamableTextures() );
		}
		else
		{
			MaxPerFrameRequestLimit = MAX_PER_FRAME_REQUEST_LIMIT;
			MaxTexturesToProcess = Max( 1, UTexture2D::GetNumStreamableTextures() / NUM_TICKS_FOR_FULL_ITERATION );
		}
		MaxTexturesToProcess = IndividualStreamingTexture ? 1 : MaxTexturesToProcess;
	}

	/**
	 * Adds in the stats from another context.
	 *
	 * @param Other		Context to add stats from
	 */
	void AddStats( const FStreamingContext& Other )
	{
		ThisFrameTotalRequestSize						+= Other.ThisFrameTotalRequestSize;						
		ThisFrameTotalLightmapRequestSize				+= Other.ThisFrameTotalLightmapRequestSize;
		ThisFrameNumStreamingTextures					+= Other.ThisFrameNumStreamingTextures;
		ThisFrameNumRequestsInCancelationPhase			+= Other.ThisFrameNumRequestsInCancelationPhase;
		ThisFrameNumRequestsInUpdatePhase				+= Other.ThisFrameNumRequestsInUpdatePhase;
		ThisFrameNumRequestsInFinalizePhase				+= Other.ThisFrameNumRequestsInFinalizePhase;
		ThisFrameTotalIntermediateTexturesSize			+= Other.ThisFrameTotalIntermediateTexturesSize;
		ThisFrameNumIntermediateTextures				+= Other.ThisFrameNumIntermediateTextures;
		ThisFrameTotalStreamingTexturesSize				+= Other.ThisFrameTotalStreamingTexturesSize;
		ThisFrameTotalStreamingTexturesMaxSize			+= Other.ThisFrameTotalStreamingTexturesMaxSize;
		ThisFrameTotalLightmapMemorySize				+= Other.ThisFrameTotalLightmapMemorySize;
		ThisFrameTotalLightmapDiskSize					+= Other.ThisFrameTotalLightmapDiskSize;
		ThisFrameTotalMipCountIncreaseRequestsInFlight	+= Other.ThisFrameTotalMipCountIncreaseRequestsInFlight;
		ThisFrameOptimalWantedSize						+= Other.ThisFrameOptimalWantedSize;
		ThisFrameTotalStaticTextureHeuristicSize		+= Other.ThisFrameTotalStaticTextureHeuristicSize;
		ThisFrameTotalDynamicTextureHeuristicSize		+= Other.ThisFrameTotalDynamicTextureHeuristicSize;
		ThisFrameTotalLastRenderHeuristicSize			+= Other.ThisFrameTotalLastRenderHeuristicSize;
		ThisFrameTotalForcedHeuristicSize				+= Other.ThisFrameTotalForcedHeuristicSize;
		bCollectTextureStats							= bCollectTextureStats || Other.bCollectTextureStats;
		STAT( TextureStats.Append( Other.TextureStats ) );
	}

	/** Whether the platform RHI supports texture memory stats. */
	UBOOL bRHISupportsMemoryStats;
	/** Currently allocated number of bytes in the texture pool, or INDEX_NONE if bRHISupportsMemoryStats is FALSE. */
	INT AllocatedMemorySize;
	/** Currently available number of bytes in the texture pool, or INDEX_NONE if bRHISupportsMemoryStats is FALSE. */
	INT AvailableMemorySize;
	/** Pending Adjustments to allocated texture memory, due to async reallocations, etc. */
	INT PendingMemoryAdjustment;
	/** Legacy: Max number of bytes to stream in this frame. */
	DWORD MaxPerFrameRequestLimit;
	/** Legacy: Max number of textures to process this frame. */
	INT MaxTexturesToProcess;
	/** Whether to fill in TextureStats this frame. */
	UBOOL bCollectTextureStats;

#if STATS
	/** Stats for all textures. */
	TArray<FTextureStreamingStats>	TextureStats;
#endif

	// Stats for this frame.
	DWORD ThisFrameTotalRequestSize;
	DWORD ThisFrameTotalLightmapRequestSize;
	DWORD ThisFrameNumStreamingTextures;
	DWORD ThisFrameNumRequestsInCancelationPhase;
	DWORD ThisFrameNumRequestsInUpdatePhase;
	DWORD ThisFrameNumRequestsInFinalizePhase;
	DWORD ThisFrameTotalIntermediateTexturesSize;
	DWORD ThisFrameNumIntermediateTextures;
	DWORD ThisFrameTotalStreamingTexturesSize;
	DWORD ThisFrameTotalStreamingTexturesMaxSize;
	DWORD ThisFrameTotalLightmapMemorySize;
	DWORD ThisFrameTotalLightmapDiskSize;
	DWORD ThisFrameTotalMipCountIncreaseRequestsInFlight;
	DWORD ThisFrameOptimalWantedSize;
	/** Number of bytes using StaticTexture heuristics this frame, currently in memory. */
	DWORD ThisFrameTotalStaticTextureHeuristicSize;
	/** Number of bytes using DynmicTexture heuristics this frame, currently in memory. */
	DWORD ThisFrameTotalDynamicTextureHeuristicSize;
	/** Number of bytes using LastRenderTime heuristics this frame, currently in memory. */
	DWORD ThisFrameTotalLastRenderHeuristicSize;
	/** Number of bytes using ForcedIntoMemory heuristics this frame, currently in memory. */
	DWORD ThisFrameTotalForcedHeuristicSize;
};


/** Async work class for calculating priorities for all textures. */
// this could implement a better abandon, but give how it is used, it does that anyway via the abort mechanism
class FAsyncTextureStreaming : public FNonAbandonableTask
{
public:
	FAsyncTextureStreaming( FStreamingManagerTexture* InStreamingManager )
	:	StreamingManager( *InStreamingManager )
	,	ThreadContext( FALSE, NULL, FALSE )
	,	bAbort( FALSE )
	{
		Reset(FALSE);
	}

	/** Resets the state to start a new async job. */
	void Reset( UBOOL bCollectTextureStats )
	{
		bAbort = FALSE;
		ThreadContext.Reset( FALSE, NULL, bCollectTextureStats );
		ThreadStats.Reset();
		PrioritizedTextures.Empty( StreamingManager.StreamingTextures.Num() );
	}

	/** Notifies the async work that it should abort the thread ASAP. */
	void Abort()
	{
		bAbort = TRUE;
	}

	/** Whether the async work is being aborted. Can be used in conjunction with IsDone() to see if it has finished. */
	UBOOL IsAborted() const
	{
		return bAbort;
	}

	/** Statistics for the async work. */
	struct FThreadStats
	{
		FThreadStats()
		{
			Reset();
		}
		/** Resets the statistics to zero. */
		void Reset()
		{
			TotalResidentSize = 0;
			STAT_FAST( TotalRequiredSize = 0 );
			TempStreamingSize = 0;
			PendingStreamInSize = 0;
			PendingStreamOutSize = 0;
			WantedInSize = 0;
			WantedOutSize = 0;
			NumWantingTextures = 0;
		}
		/** Total number of bytes currently in memory */
		INT TotalResidentSize;
		/** Total number of bytes required, using PerfectWantedMips */
		STAT_FAST( INT TotalRequiredSize );
		/** Currently being streamed in, in bytes. */
		INT PendingStreamInSize;
		/** Currently being streamed out, in bytes. */
		INT PendingStreamOutSize;
		/** How much we want to stream in, in bytes. */
		INT WantedInSize;
		/** How much we want to stream out, in bytes.  */
		INT WantedOutSize;
		/** How much memory we're temporarily using for streaming in/out, in bytes. */
		INT TempStreamingSize;
		/** Number of textures that want more mips. */
		INT NumWantingTextures;
	};

	/** Returns the resulting priorities, matching the FStreamingManagerTexture::StreamingTextures array. */
	const TArray<FTexturePriority>& GetPrioritizedTextures() const
	{
		return PrioritizedTextures;
	}

	/** Returns the context (temporary info) used for the async work. */
	const FStreamingContext& GetContext() const
	{
		return ThreadContext;
	}

	/** Returns the thread statistics. */
	const FThreadStats& GetStats() const
	{
		return ThreadStats;
	}

private:
	friend class FAsyncTask<FAsyncTextureStreaming>;
	/** Performs the async work. */
	void DoWork()
	{
		PrioritizedTextures.Empty( StreamingManager.StreamingTextures.Num() );

		// Calculate DynamicWantedMips and DynamicMinDistanceSq for all dynamic textures.
		//@TODO: This is not thread-safe because it looks up UTexture2D to get to the FStreamingTexture...
//		StreamingManager.CalcDynamicWantedMips();

		// Number of textures that want more mips.
		ThreadStats.NumWantingTextures = 0;

		for ( INT Index=0; Index < StreamingManager.StreamingTextures.Num() && !IsAborted(); ++Index )
		{
			FStreamingTexture& StreamingTexture = StreamingManager.StreamingTextures( Index );

			INT ResidentTextureSize = StreamingTexture.GetSize( StreamingTexture.ResidentMips );
			ThreadStats.TotalResidentSize += ResidentTextureSize;

			UBOOL bAcceptStreamingTexture =
				StreamingManager.RemainingTicksToDisregardWorldTextures == 0
				|| StreamingTexture.LODGroup == TEXTUREGROUP_Character
				|| StreamingTexture.LODGroup == TEXTUREGROUP_CharacterSpecular
				|| StreamingTexture.LODGroup == TEXTUREGROUP_CharacterNormalMap;

			StreamingTexture.bUsesStaticHeuristics = FALSE;
			StreamingTexture.bUsesDynamicHeuristics = (StreamingTexture.DynamicScreenSize > 0.0F) ? TRUE : FALSE;
			StreamingTexture.bUsesLastRenderHeuristics = FALSE;
			StreamingTexture.bUsesForcedHeuristics = FALSE;
			StreamingTexture.bUsesOrphanedHeuristics = FALSE;

			if ( StreamingTexture.bReadyForStreaming && bAcceptStreamingTexture )
			{
				// Figure out max number of miplevels allowed by LOD code.
				StreamingManager.CalcMinMaxMips( StreamingTexture );

				// Determine how many mips this texture should have in memory.
				StreamingManager.CalcWantedMips( StreamingTexture );

				if ( StreamingTexture.WantedMips > StreamingTexture.ResidentMips )
				{
					ThreadStats.NumWantingTextures++;
				}

				UBOOL bTrackedTexture = TrackTextureEvent( &StreamingTexture, StreamingTexture.Texture, FALSE, ENABLE_TEXTURE_LOGGING, StreamingTexture.bForceFullyLoad );

				// Add to sort list, if it wants to stream in or could potentially stream out.
				if ( StreamingTexture.WantedMips > StreamingTexture.ResidentMips || StreamingTexture.ResidentMips > StreamingTexture.MinAllowedMips )
				{
					FTexturePriority* TexturePriority = new (PrioritizedTextures) FTexturePriority( StreamingTexture.CalcPriority(), Index );
				}

				// Accumulate streaming numbers.
				INT WantedTextureSize = StreamingTexture.GetSize( StreamingTexture.WantedMips );
				if ( StreamingTexture.bInFlight )
				{
					INT RequestedTextureSize = StreamingTexture.GetSize( StreamingTexture.RequestedMips );
					ThreadStats.TempStreamingSize += ResidentTextureSize;	//@TODO: 0 for in-place reallocations.
					if ( StreamingTexture.RequestedMips > StreamingTexture.ResidentMips )
					{
						ThreadStats.PendingStreamInSize += Abs(RequestedTextureSize - ResidentTextureSize);
					}
					else
					{
						ThreadStats.PendingStreamOutSize += Abs(RequestedTextureSize - ResidentTextureSize);
					}
				}
				else
				{
					if ( StreamingTexture.WantedMips > StreamingTexture.ResidentMips )
					{
						ThreadStats.WantedInSize += Abs(WantedTextureSize - ResidentTextureSize);
					}
					else
					{
						// Counting on shrinking reallocation.
						ThreadStats.WantedOutSize += Abs(WantedTextureSize - ResidentTextureSize);
					}
				}
			}

			STAT_FAST( INT PerfectWantedTextureSize = StreamingTexture.GetSize( StreamingTexture.PerfectWantedMips ) );
			STAT_FAST( ThreadStats.TotalRequiredSize += PerfectWantedTextureSize );
			StreamingManager.UpdateFrameStats( StreamingTexture, ThreadContext );

			// Reset the boost factor
			StreamingTexture.BoostFactor = 1.0f;
		}

		// Sort the candidates.
		::Sort<FTexturePriority, FTexturePriority>( PrioritizedTextures.GetTypedData(), PrioritizedTextures.Num() );
	}

	/**
	 * Give the name for external event viewers
	 * @return	the name to display in external event viewers
	 */ 
	static const TCHAR *Name()
	{
		return TEXT("FAsyncTextureStreaming");
	}

	/** Reference to the owning streaming manager, for accessing the thread-safe data. */
	FStreamingManagerTexture&	StreamingManager;
	/** Resulting priorities, matching the FStreamingManagerTexture::StreamingTextures array. */
	TArray<FTexturePriority>	PrioritizedTextures;
	/** Context (temporary info) used for the async work. */
	FStreamingContext			ThreadContext;
	/** Thread statistics. */
	FThreadStats				ThreadStats;
	/** Whether the async work should abort its processing. */
	volatile UBOOL				bAbort;
};


/*-----------------------------------------------------------------------------
	FStreamingManagerBase implementation.
-----------------------------------------------------------------------------*/

/**
 * Adds the passed in view information to the static array.
 *
 * @param ViewInfos				[in/out] Array to add the view to
 * @param ViewOrigin			View origin
 * @param ScreenSize			Screen size
 * @param FOVScreenSize			Screen size taking FOV into account
 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other regular locations
 * @param Duration				How long the streaming system should keep checking this location (in seconds). 0 means just for the next Tick.
 */
void FStreamingManagerBase::AddViewInfoToArray( TArray<FStreamingViewInfo> &ViewInfos, const FVector& ViewOrigin, FLOAT ScreenSize, FLOAT FOVScreenSize, FLOAT BoostFactor, UBOOL bOverrideLocation, FLOAT Duration )
{
	// Check for duplicates and existing overrides.
	UBOOL bShouldAddView = TRUE;
	for ( INT ViewIndex=0; ViewIndex < ViewInfos.Num(); ++ViewIndex )
	{
		FStreamingViewInfo& ViewInfo = ViewInfos( ViewIndex );
		if ( ViewOrigin.Equals( ViewInfo.ViewOrigin, 0.5f ) &&
			appIsNearlyEqual( ScreenSize, ViewInfo.ScreenSize ) &&
			appIsNearlyEqual( FOVScreenSize, ViewInfo.FOVScreenSize ) &&
			ViewInfo.bOverrideLocation == bOverrideLocation )
		{
			// Update duration
			ViewInfo.Duration = Duration;
			// Overwrite BoostFactor if it isn't default 1.0
			ViewInfo.BoostFactor = appIsNearlyEqual(BoostFactor, 1.0f) ? ViewInfo.BoostFactor : BoostFactor;
			bShouldAddView = FALSE;
		}
	}

	if ( bShouldAddView )
	{
		new (ViewInfos) FStreamingViewInfo( ViewOrigin, ScreenSize, FOVScreenSize, BoostFactor, bOverrideLocation, Duration );
	}
}

/**
 * Remove view infos with the same location from the given array.
 *
 * @param ViewInfos				[in/out] Array to remove the view from
 * @param ViewOrigin			View origin
 */
void FStreamingManagerBase::RemoveViewInfoFromArray( TArray<FStreamingViewInfo> &ViewInfos, const FVector& ViewOrigin )
{
	for ( INT ViewIndex=0; ViewIndex < ViewInfos.Num(); ++ViewIndex )
	{
		FStreamingViewInfo& ViewInfo = ViewInfos( ViewIndex );
		if ( ViewOrigin.Equals( ViewInfo.ViewOrigin, 0.5f ) )
		{
			ViewInfos.RemoveSwap( ViewIndex-- );
		}
	}
}

#if STREAMING_LOG_VIEWCHANGES
TArray<FStreamingViewInfo> GPrevViewLocations;
#endif

/**
 * Sets up the CurrentViewInfos array based on PendingViewInfos, LastingViewInfos and SlaveLocations.
 * Removes out-dated LastingViewInfos.
 *
 * @param DeltaTime		Time since last call in seconds
 */
void FStreamingManagerBase::SetupViewInfos( FLOAT DeltaTime )
{
	// Reset CurrentViewInfos
	CurrentViewInfos.Empty( PendingViewInfos.Num() + LastingViewInfos.Num() + SlaveLocations.Num() );

	UBOOL bHaveMultiplePlayerViews = (PendingViewInfos.Num() > 1) ? TRUE : FALSE;

	// Add the slave locations.
	FLOAT ScreenSize = 1280.0f;
	FLOAT FOVScreenSize = ScreenSize / appTan( 80.0f * FLOAT(PI) / 360.0f );
	if ( PendingViewInfos.Num() > 0 )
	{
		ScreenSize = PendingViewInfos(0).ScreenSize;
		FOVScreenSize = PendingViewInfos(0).FOVScreenSize;
	}
	else if ( LastingViewInfos.Num() > 0 )
	{
		ScreenSize = LastingViewInfos(0).ScreenSize;
		FOVScreenSize = LastingViewInfos(0).FOVScreenSize;
	}
	// Add them to the appropriate array (pending views or lasting views).
	for ( INT SlaveLocationIndex=0; SlaveLocationIndex < SlaveLocations.Num(); SlaveLocationIndex++ )
	{
		const FSlaveLocation& SlaveLocation = SlaveLocations( SlaveLocationIndex );
		AddViewInformation( SlaveLocation.Location, ScreenSize, FOVScreenSize, SlaveLocation.BoostFactor, SlaveLocation.bOverrideLocation, SlaveLocation.Duration );
	}

	// Apply a split-screen factor if we have multiple players on the same machine, and they currently have individual views.
	FLOAT SplitScreenFactor = 1.0f;
	if ( GEngine->IsSplitScreen() && bHaveMultiplePlayerViews )
	{
		SplitScreenFactor = 0.75f;
	}

	// Should we use override views this frame? (If we have both a fullscreen movie and an override view.)
	UBOOL bUseOverrideViews = FALSE;
	UBOOL bIsFullscreenMoviePlaying = GFullScreenMovie && GFullScreenMovie->GameThreadIsMoviePlaying(TEXT(""));
	if ( bIsFullscreenMoviePlaying )
	{
		// Check if we have any override views.
		for ( INT ViewIndex=0; !bUseOverrideViews && ViewIndex < LastingViewInfos.Num(); ++ViewIndex )
		{
			const FStreamingViewInfo& ViewInfo = LastingViewInfos( ViewIndex );
			if ( ViewInfo.bOverrideLocation )
			{
				bUseOverrideViews = TRUE;
			}
		}
		for ( INT ViewIndex=0; !bUseOverrideViews && ViewIndex < PendingViewInfos.Num(); ++ViewIndex )
		{
			const FStreamingViewInfo& ViewInfo = PendingViewInfos( ViewIndex );
			if ( ViewInfo.bOverrideLocation )
			{
				bUseOverrideViews = TRUE;
			}
		}
	}

	// Add the lasting views.
	for ( INT ViewIndex=0; ViewIndex < LastingViewInfos.Num(); ++ViewIndex )
	{
		const FStreamingViewInfo& ViewInfo = LastingViewInfos( ViewIndex );
		if ( bUseOverrideViews == ViewInfo.bOverrideLocation )
		{
			AddViewInfoToArray( CurrentViewInfos, ViewInfo.ViewOrigin, ViewInfo.ScreenSize * SplitScreenFactor, ViewInfo.FOVScreenSize, ViewInfo.BoostFactor, ViewInfo.bOverrideLocation, ViewInfo.Duration );
		}
	}

	// Add the regular views.
	for ( INT ViewIndex=0; ViewIndex < PendingViewInfos.Num(); ++ViewIndex )
	{
		const FStreamingViewInfo& ViewInfo = PendingViewInfos( ViewIndex );
		if ( bUseOverrideViews == ViewInfo.bOverrideLocation )
		{
			AddViewInfoToArray( CurrentViewInfos, ViewInfo.ViewOrigin, ViewInfo.ScreenSize * SplitScreenFactor, ViewInfo.FOVScreenSize, ViewInfo.BoostFactor, ViewInfo.bOverrideLocation, ViewInfo.Duration );
		}
	}

	// Update duration for the lasting views, removing out-dated ones.
	for ( INT ViewIndex=0; ViewIndex < LastingViewInfos.Num(); ++ViewIndex )
	{
		FStreamingViewInfo& ViewInfo = LastingViewInfos( ViewIndex );
		ViewInfo.Duration -= DeltaTime;

		// Remove old override locations.
		if ( ViewInfo.Duration <= 0.0f )
		{
			LastingViewInfos.RemoveSwap( ViewIndex-- );
		}
	}

#if STREAMING_LOG_VIEWCHANGES
	{
		// Check if we're adding any new locations.
		for ( INT ViewIndex = 0; ViewIndex < CurrentViewInfos.Num(); ++ViewIndex )
		{
			FStreamingViewInfo& ViewInfo = CurrentViewInfos( ViewIndex );
			UBOOL bFound = FALSE;
			for ( INT PrevView=0; PrevView < GPrevViewLocations.Num(); ++PrevView )
			{
				if ( (ViewInfo.ViewOrigin - GPrevViewLocations(PrevView).ViewOrigin).Size() < 100.0f )
				{
					bFound = TRUE;
					break;
				}
			}
			if ( !bFound )
			{
				debugf( NAME_DevStreaming, TEXT("SMEDIS: Adding location: X=%.1f, Y=%.1f, Z=%.1f (override=%d, boost=%.1f)"), ViewInfo.ViewOrigin.X, ViewInfo.ViewOrigin.Y, ViewInfo.ViewOrigin.Z, ViewInfo.bOverrideLocation, ViewInfo.BoostFactor );
			}
		}

		// Check if we're removing any locations.
		for ( INT PrevView=0; PrevView < GPrevViewLocations.Num(); ++PrevView )
		{
			UBOOL bFound = FALSE;
			for ( INT ViewIndex = 0; ViewIndex < CurrentViewInfos.Num(); ++ViewIndex )
			{
				FStreamingViewInfo& ViewInfo = CurrentViewInfos( ViewIndex );
				if ( (ViewInfo.ViewOrigin - GPrevViewLocations(PrevView).ViewOrigin).Size() < 100.0f )
				{
					bFound = TRUE;
					break;
				}
			}
			if ( !bFound )
			{
				FStreamingViewInfo& PrevViewInfo = GPrevViewLocations(PrevView);
				debugf( NAME_DevStreaming, TEXT("SMEDIS: Removing location: X=%.1f, Y=%.1f, Z=%.1f (override=%d, boost=%.1f)"), PrevViewInfo.ViewOrigin.X, PrevViewInfo.ViewOrigin.Y, PrevViewInfo.ViewOrigin.Z, PrevViewInfo.bOverrideLocation, PrevViewInfo.BoostFactor );
			}
		}

		// Save the locations.
		GPrevViewLocations.Empty(CurrentViewInfos.Num());
		for ( INT ViewIndex = 0; ViewIndex < CurrentViewInfos.Num(); ++ViewIndex )
		{
			FStreamingViewInfo& ViewInfo = CurrentViewInfos( ViewIndex );
			GPrevViewLocations.AddItem( ViewInfo );
		}
	}
#endif
}

/**
 * Adds the passed in view information to the static array.
 *
 * @param ViewOrigin			View origin
 * @param ScreenSize			Screen size
 * @param FOVScreenSize			Screen size taking FOV into account
 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other regular locations
 * @param Duration				How long the streaming system should keep checking this location (in seconds). 0 means just for the next Tick.
 */
void FStreamingManagerBase::AddViewInformation( const FVector& ViewOrigin, FLOAT ScreenSize, FLOAT FOVScreenSize, FLOAT BoostFactor/*=1.0f*/, UBOOL bOverrideLocation/*=FALSE*/, FLOAT Duration/*=0.0f*/ )
{
	if ( bPendingRemoveViews )
	{
		bPendingRemoveViews = FALSE;

		// Remove out-dated override views and empty the PendingViewInfos/SlaveLocation arrays to be populated again during next frame.
		RemoveStreamingViews( RemoveStreamingViews_Normal );
	}

	// Remove a lasting location if we're given the same location again but with 0 duration.
	if ( Duration <= 0.0f )
	{
		RemoveViewInfoFromArray( LastingViewInfos, ViewOrigin );
	}

	// Should we remember this location for a while?
	if ( Duration > 0.0f )
	{
		AddViewInfoToArray( LastingViewInfos, ViewOrigin, ScreenSize, FOVScreenSize, BoostFactor, bOverrideLocation, Duration );
	}
	else
	{
		AddViewInfoToArray( PendingViewInfos, ViewOrigin, ScreenSize, FOVScreenSize, BoostFactor, bOverrideLocation, 0.0f );
	}
}

/**
 * Queue up view "slave" locations to the streaming system. These locations will be added properly at the next call to AddViewInformation,
 * re-using the screensize and FOV settings.
 *
 * @param SlaveLocation			World-space view origin
 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other locations
 * @param Duration				How long the streaming system should keep checking this location (in seconds). 0 means just for the next Tick.
 */
void FStreamingManagerBase::AddViewSlaveLocation( const FVector& SlaveLocation, FLOAT BoostFactor/*=1.0f*/, UBOOL bOverrideLocation/*=FALSE*/, FLOAT Duration/*=0.0f*/ )
{
	if ( bPendingRemoveViews )
	{
		bPendingRemoveViews = FALSE;

		// Remove out-dated override views and empty the PendingViewInfos/SlaveLocation arrays to be populated again during next frame.
		RemoveStreamingViews( RemoveStreamingViews_Normal );
	}

	new (SlaveLocations) FSlaveLocation(SlaveLocation, BoostFactor, bOverrideLocation, Duration );
}

/**
 * Removes streaming views from the streaming manager. This is also called by Tick().
 *
 * @param RemovalType	What types of views to remove (all or just the normal views)
 */
void FStreamingManagerBase::RemoveStreamingViews( ERemoveStreamingViews RemovalType )
{
	PendingViewInfos.Empty();
	SlaveLocations.Empty();
	if ( RemovalType == RemoveStreamingViews_All )
	{
		LastingViewInfos.Empty();
	}
}

/**
 * Calls UpdateResourceStreaming(), and does per-frame cleaning. Call once per frame.
 *
 * @param DeltaTime				Time since last call in seconds
 * @param bProcessEverything	[opt] If TRUE, process all resources with no throttling limits
 */
void FStreamingManagerBase::Tick( FLOAT DeltaTime, UBOOL bProcessEverything/*=FALSE*/ )
{
	UpdateResourceStreaming( DeltaTime, bProcessEverything );

	// Trigger a call to bPendingRemoveViews( Normal ) next time we add a view.
	bPendingRemoveViews = TRUE;
}


/*-----------------------------------------------------------------------------
	FStreamingManagerCollection implementation.
-----------------------------------------------------------------------------*/

FStreamingManagerCollection::FStreamingManagerCollection()
:	NumIterations(1)
,	DisableResourceStreamingCount(0)
,	LoadMapTimeLimit( 5.0f )
{
	GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("LoadMapTimeLimit"), LoadMapTimeLimit, GEngineIni );
}

/**
 * Sets the number of iterations to use for the next time UpdateResourceStreaming is being called. This 
 * is being reset to 1 afterwards.
 *
 * @param InNumIterations	Number of iterations to perform the next time UpdateResourceStreaming is being called.
 */
void FStreamingManagerCollection::SetNumIterationsForNextFrame( INT InNumIterations )
{
	NumIterations = InNumIterations;
}

/**
 * Updates streaming, taking into account all current view infos. Can be called multiple times per frame.
 *
 * @param DeltaTime				Time since last call in seconds
 * @param bProcessEverything	[opt] If TRUE, process all resources with no throttling limits
 */
void FStreamingManagerCollection::UpdateResourceStreaming( FLOAT DeltaTime, UBOOL bProcessEverything/*=FALSE*/ )
{
	SetupViewInfos( DeltaTime );

	// only allow this if its not disabled
	if (DisableResourceStreamingCount == 0)
	{
		for( INT Iteration=0; Iteration<NumIterations; Iteration++ )
		{
			// Flush rendering commands in the case of multiple iterations to sync up resource streaming
			// with the GPU/ rendering thread.
			if( Iteration > 0 )
			{
				FlushRenderingCommands();
			}

			// Route to streaming managers.
			for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
			{
				FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
				StreamingManager->UpdateResourceStreaming( DeltaTime, bProcessEverything );
			}
		}

		// Reset number of iterations to 1 for next frame.
		NumIterations = 1;
	}
}

/**
 * Streams in/out all resources that wants to and blocks until it's done.
 *
 * @param bIncludePlayerLocations	If TRUE, accounts for the location of all local players in the heuristics
 * @param TimeLimit					Maximum number of seconds to wait for streaming I/O. If zero, uses .ini setting
 * @return							Number of streaming requests still in flight, if the time limit was reached before they were finished.
 */
INT FStreamingManagerCollection::StreamAllResources( UBOOL bIncludePlayerLocations, FLOAT TimeLimit/*=0.0f*/ )
{
	// Disable mip-fading for upcoming texture updates.
	FLOAT PrevMipLevelFadingState = GEnableMipLevelFading;
	GEnableMipLevelFading = -1.0f;

	FlushRenderingCommands();

	if ( bIncludePlayerLocations && GEngine )
	{
		// Add all local player locations are known by the streaming system.
		for  (INT PlayerIndex = 0; PlayerIndex < GEngine->GamePlayers.Num(); PlayerIndex++ )
		{
			ULocalPlayer* Player = GEngine->GamePlayers( PlayerIndex );
			if (Player->Actor)
			{
				// Get the player's view information.
				FVector		ViewLocation;
				FRotator	ViewRotation;
				if ( Player->bOverrideView )
				{
					ViewLocation = Player->OverrideLocation;
					ViewRotation = Player->OverrideRotation;
				}
				else
				{
//					Player->Actor->PlayerCamera->eventUpdateCamera(0.0f);
					Player->Actor->eventGetPlayerViewPoint( ViewLocation, ViewRotation );
				}

				// Add this location.
				AddViewSlaveLocation( ViewLocation );
			}
		}
	}

	if ( appIsNearlyZero(TimeLimit) )
	{
		TimeLimit = LoadMapTimeLimit;
	}

	// Update resource streaming, making sure we process all textures.
	UpdateResourceStreaming( 0, TRUE );

	// Block till requests are finished, or time limit was reached.
	INT NumPendingRequests = BlockTillAllRequestsFinished( TimeLimit, TRUE );

	GEnableMipLevelFading = PrevMipLevelFadingState;

	return NumPendingRequests;
}

/**
 * Updates streaming for an individual texture, taking into account all view infos.
 *
 * @param Texture	Texture to update
 */
void FStreamingManagerCollection::UpdateIndividualResource( UTexture2D* Texture )
{
	// only allow this if its not disabled
	if (DisableResourceStreamingCount == 0)
	{
		// Route to streaming managers.
		for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
		{
			FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
			StreamingManager->UpdateIndividualResource( Texture );
		}
	}
}

/**
 * Blocks till all pending requests are fulfilled.
 *
 * @param TimeLimit		Optional time limit for processing, in seconds. Specifying 0 means infinite time limit.
 * @param bLogResults	Whether to dump the results to the log.
 * @return				Number of streaming requests still in flight, if the time limit was reached before they were finished.
 */
INT FStreamingManagerCollection::BlockTillAllRequestsFinished( FLOAT TimeLimit /*= 0.0f*/, UBOOL bLogResults /*= FALSE*/ )
{
	INT NumPendingRequests = 0;

	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		NumPendingRequests += StreamingManager->BlockTillAllRequestsFinished( TimeLimit, bLogResults );
	}

	return NumPendingRequests;
}

/** Returns the number of resources that currently wants to be streamed in. */
INT FStreamingManagerCollection::GetNumWantingResources() const
{
	INT NumWantingResources = 0;

	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		NumWantingResources += StreamingManager->GetNumWantingResources();
	}

	return NumWantingResources;
}

/**
 * Returns the current ID for GetNumWantingResources().
 * The ID is bumped every time NumWantingResources is updated by the streaming system (every few frames).
 * Can be used to verify that any changes have been fully examined, by comparing current ID with
 * what it was when the changes were made.
 */
INT FStreamingManagerCollection::GetNumWantingResourcesID() const
{
	INT NumWantingResourcesCounter = MAXINT;

	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		NumWantingResourcesCounter = Min( NumWantingResourcesCounter, StreamingManager->GetNumWantingResourcesID() );
	}

	return NumWantingResourcesCounter;
}

/**
 * Cancels the timed Forced resources (i.e used the Kismet action "Stream In Textures").
 */
void FStreamingManagerCollection::CancelForcedResources()
{
	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		StreamingManager->CancelForcedResources();
	}
}

/**
 * Notifies managers of "level" change.
 */
void FStreamingManagerCollection::NotifyLevelChange()
{
	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		StreamingManager->NotifyLevelChange();
	}
}

/** Don't stream world resources for the next NumFrames. */
void FStreamingManagerCollection::SetDisregardWorldResourcesForFrames(INT NumFrames )
{
	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		StreamingManager->SetDisregardWorldResourcesForFrames(NumFrames);
	}
}

/**
 * Temporarily boosts the streaming distance factor by the specified number.
 * This factor is automatically reset to 1.0 after it's been used for mip-calculations.
 */
void FStreamingManagerCollection::BoostTextures( AActor* Actor, FLOAT BoostFactor )
{
	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		StreamingManager->BoostTextures(Actor, BoostFactor);
	}
}

/**
 * Adds a streaming manager to the array of managers to route function calls to.
 *
 * @param StreamingManager	Streaming manager to add
 */
void FStreamingManagerCollection::AddStreamingManager( FStreamingManagerBase* StreamingManager )
{
	StreamingManagers.AddItem( StreamingManager );
}

/**
 * Removes a streaming manager from the array of managers to route function calls to.
 *
 * @param StreamingManager	Streaming manager to remove
 */
void FStreamingManagerCollection::RemoveStreamingManager( FStreamingManagerBase* StreamingManager )
{
	StreamingManagers.RemoveItem( StreamingManager );
}

/**
 * Disables resource streaming. Enable with EnableResourceStreaming. Disable/enable can be called multiple times nested
 */
void FStreamingManagerCollection::DisableResourceStreaming()
{
	// push on a disable
	appInterlockedIncrement(&DisableResourceStreamingCount);
}

/**
 * Enables resource streaming, previously disabled with enableResourceStreaming. Disable/enable can be called multiple times nested
 * (this will only actually enable when all disables are matched with enables)
 */
void FStreamingManagerCollection::EnableResourceStreaming()
{
	// pop off a disable
	appInterlockedDecrement(&DisableResourceStreamingCount);

	checkf(DisableResourceStreamingCount >= 0, TEXT("Mismatched number of calls to FStreamingManagerCollection::DisableResourceStreaming/EnableResourceStreaming"));
}

/**
 *	Try to stream out texture mip-levels to free up more memory.
 *	@param RequiredMemorySize	- Required minimum available texture memory
 *	@return						- Whether it succeeded or not
 **/
UBOOL FStreamingManagerCollection::StreamOutTextureData( INT RequiredMemorySize )
{
	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		if ( StreamingManager->StreamOutTextureData( RequiredMemorySize ) )
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Allows the streaming manager to process exec commands.
 *
 * @param Cmd	Exec command
 * @param Ar	Output device for feedback
 * @return		TRUE if the command was handled
 */
UBOOL FStreamingManagerCollection::Exec( const TCHAR* Cmd, FOutputDevice& Ar ) 
{
	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		if ( StreamingManager->Exec( Cmd, Ar ) )
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Adds a new texture to the streaming manager.
 */
void FStreamingManagerCollection::AddStreamingTexture( UTexture2D* Texture )
{
	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		StreamingManager->AddStreamingTexture( Texture );
	}
}

/**
 * Removes a texture from the streaming manager.
 */
void FStreamingManagerCollection::RemoveStreamingTexture( UTexture2D* Texture )
{
	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		StreamingManager->RemoveStreamingTexture( Texture );
	}
}

/** Adds a ULevel to the streaming manager. */
void FStreamingManagerCollection::AddLevel( ULevel* Level )
{
#if STREAMING_LOG_LEVELS
	debugf(TEXT("FStreamingManagerCollection::AddLevel(\"%s\")"), *Level->GetOutermost()->GetName());
#endif

	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		StreamingManager->AddLevel( Level );
	}
}

/** Removes a ULevel from the streaming manager. */
void FStreamingManagerCollection::RemoveLevel( ULevel* Level )
{
#if STREAMING_LOG_LEVELS
	debugf(TEXT("FStreamingManagerCollection::RemoveLevel(\"%s\")"), *Level->GetOutermost()->GetName());
#endif

	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		StreamingManager->RemoveLevel( Level );
	}
}

/** Called when an actor is spawned. */
void FStreamingManagerCollection::NotifyActorSpawned( AActor* Actor )
{
	STAT( DOUBLE StartTime = appSeconds() );

	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		StreamingManager->NotifyActorSpawned( Actor );
	}
	STAT( GStreamingDynamicPrimitivesTime += appSeconds() - StartTime );
}

/** Called when a spawned actor is destroyed. */
void FStreamingManagerCollection::NotifyActorDestroyed( AActor* Actor )
{
	STAT( DOUBLE StartTime = appSeconds() );

	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		StreamingManager->NotifyActorDestroyed( Actor );
	}
	STAT( GStreamingDynamicPrimitivesTime += appSeconds() - StartTime );
}

/**
 * Called when a primitive is attached to an actor or another component.
 * Replaces previous info, if the primitive was already attached.
 *
 * @param InPrimitive	Newly attached dynamic/spawned primitive
 */
void FStreamingManagerCollection::NotifyPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType )
{
	STAT( DOUBLE StartTime = appSeconds() );

	// Check for spawned primitives that should be using LastRenderTime heuristics
	if ( Primitive->IsA( UDecalComponent::StaticClass() ) )
	{
		for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
		{
			FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
			StreamingManager->NotifyTimedPrimitiveAttached( Primitive, DynamicType );
		}
	}
	// Distance-based heuristics only handle skeletalmeshes and staticmeshes.
	else if ( Primitive->IsA( USkeletalMeshComponent::StaticClass() ) || Primitive->IsA( UStaticMeshComponent::StaticClass() ) )
	{
		for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
		{
			FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
			StreamingManager->NotifyPrimitiveAttached( Primitive, DynamicType );
		}
	}
	STAT( GStreamingDynamicPrimitivesTime += appSeconds() - StartTime );
}

/** Called when a primitive is detached from an actor or another component. */
void FStreamingManagerCollection::NotifyPrimitiveDetached( const UPrimitiveComponent* Primitive )
{
	STAT( DOUBLE StartTime = appSeconds() );

	// Check for spawned primitives that should be using LastRenderTime heuristics
	if ( Primitive->IsA( UDecalComponent::StaticClass() ) )
	{
		for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
		{
			FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
			StreamingManager->NotifyTimedPrimitiveDetached( Primitive );
		}
	}
	// Distance-based heuristics only handle skeletalmeshes and staticmeshes.
	else if ( Primitive->IsA( USkeletalMeshComponent::StaticClass() ) || Primitive->IsA( UStaticMeshComponent::StaticClass() ) )
	{
		// Route to streaming managers.
		for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
		{
			FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
			StreamingManager->NotifyPrimitiveDetached( Primitive );
		}
	}
	STAT( GStreamingDynamicPrimitivesTime += appSeconds() - StartTime );
}

/**
 * Called when a primitive has had its textured changed.
 * Only affects primitives that were already attached.
 * Replaces previous info.
 */
void FStreamingManagerCollection::NotifyPrimitiveUpdated( const UPrimitiveComponent* Primitive )
{
	STAT( DOUBLE StartTime = appSeconds() );

	if ( Primitive->IsA( USkeletalMeshComponent::StaticClass() ) || Primitive->IsA( UStaticMeshComponent::StaticClass() ) )
	{
		// Route to streaming managers.
		for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
		{
			FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
			StreamingManager->NotifyPrimitiveUpdated( Primitive );
		}
	}
	STAT( GStreamingDynamicPrimitivesTime += appSeconds() - StartTime );
}

/** Returns TRUE if this is a streaming resource that is managed by the streaming manager. */
UBOOL FStreamingManagerCollection::IsManagedStreamingResource( const UTexture2D* Texture2D )
{
	// Route to streaming managers.
	for( INT ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		FStreamingManagerBase* StreamingManager = StreamingManagers(ManagerIndex);
		if ( StreamingManager->IsManagedStreamingResource( Texture2D ) )
		{
			return TRUE;
		}
	}
	return FALSE;
}


/*-----------------------------------------------------------------------------
	FStreamingManagerTexture implementation.
-----------------------------------------------------------------------------*/

/** Constructor, initializing all members and  */
FStreamingManagerTexture::FStreamingManagerTexture()
:	NumStreamingTextureInstances(0)
,	NumStreamingLightmapInstances(0)
,	CurrentUpdateStreamingTextureIndex(0)
,	RemainingTicksToDisregardWorldTextures(0)
,	bTriggerDumpTextureGroupStats( FALSE )
,	bDetailedDumpTextureGroupStats( FALSE )
,	bTriggerInvestigateTexture( FALSE )
,	AsyncWork( NULL )
,	ProcessingStage( 0 )
,	NumTextureProcessingStages(5)
,	MaxTempMemoryUsed( 5*1024*1024 )
,	bUsePriorityStreaming( FALSE )
,	bAllowSwitchingStreamingSystem( FALSE )
,	bUseDynamicStreaming( FALSE )
,	BoostPlayerTextures( 3.0f )
,	RemainingTicksToSuspendActivity(0)
,	FudgeFactor(1.f)
,	FudgeFactorRateOfChange(0.f)
,	LastLevelChangeTime(0.0)
,	bUseMinRequestLimit(FALSE)
,	LastFullIterationTime(0)
,	CurrentFullIterationTime(0)
,	MemoryMargin(0)
,	MemoryHysteresisLimit(0)
,	MemoryDropMipLevelsLimit(0)
,	MemoryStopIncreasingLimit(0)
,	MemoryStopStreamingLimit(0)
,	FudgeFactorIncreaseRateOfChange(0)
,	FudgeFactorDecreaseRateOfChange(0)
,	MinRequestedMipsToConsider(0)
,	MinTimeToGuaranteeMinMipCount(0)
,	MaxTimeToGuaranteeMinMipCount(0)
,	MinEvictSize(0)
,	IndividualStreamingTexture(NULL)
,	NumStreamingTextures(0)
,	NumRequestsInCancelationPhase(0)
,	NumRequestsInUpdatePhase(0)
,	NumRequestsInFinalizePhase(0)
,	TotalIntermediateTexturesSize(0)
,	NumIntermediateTextures(0)
,	TotalStreamingTexturesSize(0)
,	TotalStreamingTexturesMaxSize(0)
,	TotalLightmapMemorySize(0)
,	TotalLightmapDiskSize(0)
,	TotalMipCountIncreaseRequestsInFlight(0)
,	TotalOptimalWantedSize(0)
,	TotalStaticTextureHeuristicSize(0)
,	TotalDynamicTextureHeuristicSize(0)
,	TotalLastRenderHeuristicSize(0)
,	TotalForcedHeuristicSize(0)
,	OriginalTexturePoolSize(0)
,	bCollectTextureStats(FALSE)
,	bReportTextureStats(FALSE)
,	bPauseTextureStreaming(FALSE)
{
	// Read settings from ini file.
	verify( GConfig->GetInt( TEXT("TextureStreaming"), TEXT("MemoryMargin"),				MemoryMargin,						GEngineIni ) );
	verify( GConfig->GetInt( TEXT("TextureStreaming"), TEXT("HysteresisLimit"),				MemoryHysteresisLimit,				GEngineIni ) );
	verify( GConfig->GetInt( TEXT("TextureStreaming"), TEXT("DropMipLevelsLimit"),			MemoryDropMipLevelsLimit,			GEngineIni ) );
	verify( GConfig->GetInt( TEXT("TextureStreaming"), TEXT("StopIncreasingLimit"),			MemoryStopIncreasingLimit,			GEngineIni ) );
	verify( GConfig->GetInt( TEXT("TextureStreaming"), TEXT("StopStreamingLimit"),			MemoryStopStreamingLimit,			GEngineIni ) );
	verify( GConfig->GetInt( TEXT("TextureStreaming"), TEXT("MinRequestedMipsToConsider"),	MinRequestedMipsToConsider,			GEngineIni ) );
	verify( GConfig->GetInt( TEXT("TextureStreaming"), TEXT("MinEvictSize"),				MinEvictSize,						GEngineIni ) );

	verify( GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("MinTimeToGuaranteeMinMipCount"),		MinTimeToGuaranteeMinMipCount,	GEngineIni ) );
	verify( GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("MaxTimeToGuaranteeMinMipCount"),		MaxTimeToGuaranteeMinMipCount,	GEngineIni ) );
	verify( GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("LightmapStreamingFactor"),			GLightmapStreamingFactor,		GEngineIni ) );
	verify( GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("ShadowmapStreamingFactor"),			GShadowmapStreamingFactor,		GEngineIni ) );

	verify( GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("MinFudgeFactor"),					MinFudgeFactor, GEngineIni ) );
	verify( GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("FudgeFactorIncreaseRateOfChange"),	FudgeFactorIncreaseRateOfChange, GEngineIni ) );
	verify( GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("FudgeFactorDecreaseRateOfChange"),	FudgeFactorDecreaseRateOfChange, GEngineIni ) );
	GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSize"), OriginalTexturePoolSize, GEngineIni);
	GConfig->GetBool(TEXT("TextureStreaming"), TEXT("UsePriorityStreaming"), bUsePriorityStreaming, GEngineIni);
	GConfig->GetBool(TEXT("TextureStreaming"), TEXT("bAllowSwitchingStreamingSystem"), bAllowSwitchingStreamingSystem, GEngineIni);
	GConfig->GetBool(TEXT("TextureStreaming"), TEXT("UseDynamicStreaming"), bUseDynamicStreaming, GEngineIni);
	GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("BoostPlayerTextures"), BoostPlayerTextures, GEngineIni );
	GConfig->GetBool(TEXT("TextureStreaming"), TEXT("NeverStreamOutTextures"), GNeverStreamOutTextures, GEngineIni);
	if (ParseParam(appCmdLine(), TEXT("NeverStreamOutTextures")))
	{
		GNeverStreamOutTextures = TRUE;
	}
	if (GIsEditor)
	{
		 // this would not be good or useful in the editor
		GNeverStreamOutTextures = FALSE;
	}
	if (GNeverStreamOutTextures)
	{
		debugf(TEXT("Textures will NEVER stream out!"));
	}

	GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("OverallTextureStreamingBias"),			GOverallTextureStreamingBias,		GEngineIni );
	check(GOverallTextureStreamingBias > 0.0f);
	if (GOverallTextureStreamingBias != 1.0f)
	{
		debugf(TEXT("OverallTextureStreamingBias = %5.3f"),GOverallTextureStreamingBias);
	}
	GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("MinimumStreamingCameraToMeshDistance"),	GMinimumStreamingCameraToMeshDistance,GEngineIni );
	if (GIsEditor)
	{
		// you want to be able to see full res in the editor
		GMinimumStreamingCameraToMeshDistance = 0.0f;
	}
	if (GMinimumStreamingCameraToMeshDistance > 1.0f)
	{
		debugf(TEXT("MinimumStreamingCameraToMeshDistance = %6.3f"),GMinimumStreamingCameraToMeshDistance);
	}

	OriginalTexturePoolSize *= 1024*1024;

	// Convert from MByte to byte.
	MemoryHysteresisLimit		*= 1024 * 1024;
	MemoryDropMipLevelsLimit	*= 1024 * 1024;
	MemoryStopIncreasingLimit	*= 1024 * 1024;
	MemoryStopStreamingLimit	*= 1024 * 1024;
	MinEvictSize				*= 1024 * 1024;
	MemoryMargin				*= 1024 * 1024;

	// Allow half of the memory margin for temporary streaming waste.
	MaxTempMemoryUsed = MemoryMargin / 2;

#if STATS
	NumLatencySamples = 0;
	LatencySampleIndex = 0;
	LatencyAverage = 0.0f;
	LatencyMaximum = 0.0f;
	for ( INT LatencyIndex=0; LatencyIndex < NUM_LATENCYSAMPLES; ++LatencyIndex )
	{
		LatencySamples[ LatencyIndex ] = 0.0f;
	}
#endif

#if STATS_FAST
	MaxStreamingTexturesSize = 0;
	MaxOptimalTextureSize = 0;
	MaxStreamingOverBudget = MININT;
	MaxTexturePoolAllocatedSize = 0;
	MinLargestHoleSize = OriginalTexturePoolSize;
	MaxNumWantingTextures = 0;
#endif

	for ( INT LODGroup=0; LODGroup < TEXTUREGROUP_MAX; ++LODGroup )
	{
		FTextureLODSettings::FTextureLODGroup& TexGroup = GSystemSettings.TextureLODSettings.GetTextureLODGroup( LODGroup );
		ThreadSettings.NumStreamedMips[LODGroup] = TexGroup.NumStreamedMips;
	}

	ProcessingStage = 0;
	AsyncWork = new FAsyncTask<FAsyncTextureStreaming>(this);
}

FStreamingManagerTexture::~FStreamingManagerTexture()
{
	AsyncWork->GetTask().Abort();
	AsyncWork->EnsureCompletion();
	delete AsyncWork;
}

/**
 * Cancels the timed Forced resources (i.e used the Kismet action "Stream In Textures").
 */
void FStreamingManagerTexture::CancelForcedResources()
{
	FLOAT CurrentTime = FLOAT(appSeconds() - GStartTime);

	// Update textures that are Forced on a timer.
	for ( INT TextureIndex=0; TextureIndex < StreamingTextures.Num(); ++TextureIndex )
	{
		FStreamingTexture& StreamingTexture = StreamingTextures( TextureIndex );

		// Make sure this streaming texture hasn't been marked for removal.
		if ( StreamingTexture.Texture )
		{
			// Remove any prestream requests from textures
			FLOAT TimeLeft = StreamingTexture.Texture->ForceMipLevelsToBeResidentTimestamp - CurrentTime;
			if ( TimeLeft > 0.0f )
			{
				StreamingTexture.Texture->SetForceMipLevelsToBeResident( -1.0f );
				StreamingTexture.InstanceRemovedTimestamp = -FLT_MAX;
				if ( StreamingTexture.Texture->Resource )
				{
					StreamingTexture.Texture->Resource->LastRenderTime = -FLT_MAX;
				}
#if STREAMING_LOG_CANCELFORCED
				debugf( NAME_DevStreaming, TEXT("Canceling forced texture: %s (had %.1f seconds left)"), *StreamingTexture.Texture->GetFullName(), TimeLeft );
#endif
			}
		}
	}

	// Reset the streaming system, so it picks up any changes to UTexture2D right away.
	ProcessingStage = 0;
}

/**
 * Notifies manager of "level" change so it can prioritize character textures for a few frames.
 */
void FStreamingManagerTexture::NotifyLevelChange()
{
	// Disregard world textures for one iteration to prioritize other requests.
	RemainingTicksToDisregardWorldTextures = NUM_TICKS_FOR_FULL_ITERATION;
	// Keep track of last time this function was called.
	LastLevelChangeTime = appSeconds();
	// Prioritize higher miplevels to avoid texture popping on initial level load.
	bUseMinRequestLimit = TRUE;
}

/** Don't stream world resources for the next NumFrames. */
void FStreamingManagerTexture::SetDisregardWorldResourcesForFrames( INT NumFrames )
{
	RemainingTicksToDisregardWorldTextures = NumFrames;
}

/**
 *	Helper struct that represents a texture and the parameters used for sorting and streaming out high-res mip-levels.
 **/
struct FTextureSortElement
{
	/**
	 *	Constructor.
	 *
	 *	@param InTexture					- The texture to represent
	 *	@param InSize						- Size of the whole texture and all current mip-levels, in bytes
	 *	@param bInIsCharacterTexture		- 1 if this is a character texture, otherwise 0
	 *	@param InTextureDataAddress			- Starting address of the texture data
	 *	@param InNumRequiredResidentMips	- Minimum number of mip-levels required to stay in memory
	 */
	FTextureSortElement( UTexture2D* InTexture, INT InSize, INT bInIsCharacterTexture, DWORD InTextureDataAddress, INT InNumRequiredResidentMips )
	:	Texture( InTexture )
	,	Size( InSize )
	,	bIsCharacterTexture( bInIsCharacterTexture )
	,	TextureDataAddress( InTextureDataAddress )
	,	NumRequiredResidentMips( InNumRequiredResidentMips )
	{
	}
	/** The texture that this element represents */
	UTexture2D*	Texture;
	/** Size of the whole texture and all current mip-levels, in bytes. */
	INT			Size;
	/** 1 if this is a character texture, otherwise 0 */
	INT			bIsCharacterTexture;
	/** Starting address of the texture data. */
	DWORD		TextureDataAddress;			
	/** Minimum number of mip-levels required to stay in memory. */
	INT			NumRequiredResidentMips;
};

/**
 *	Helper struct to compare two FTextureSortElement objects.
 **/
struct FTextureStreamingCompare
{
	/** 
	 *	Called by Sort<>() to compare two elements.
	 *	@param Texture1		- First object to compare
	 *	@param Texture2		- Second object to compare
	 *	@return				- Negative value if Texture1 should be sorted earlier in the array than Texture2, zero if arbitrary order, positive if opposite order.
	 */
	static INT Compare( const FTextureSortElement& Texture1, const FTextureSortElement& Texture2 )
	{
		// Character textures get lower priority (sorted later in the array).
		if ( Texture1.bIsCharacterTexture != Texture2.bIsCharacterTexture )
		{
			return Texture1.bIsCharacterTexture - Texture2.bIsCharacterTexture;
		}

		// Larger textures get higher priority (sorted earlier in the array).
		if ( Texture2.Size - Texture1.Size )
		{
			return Texture2.Size - Texture1.Size;
		}

		// Then sort on baseaddress, so that textures at the end of the texture pool gets higher priority (sorted earlier in the array).
		// (It's faster to defrag the texture pool when the holes are at the end.)
		return INT(Texture2.TextureDataAddress - Texture1.TextureDataAddress);
	}
};

/**
 *	Renderthread function: Try to stream out texture mip-levels to free up more memory.
 *	@param InCandidateTextures	- Array of possible textures to shrink
 *	@param RequiredMemorySize	- Amount of memory to try to free up, in bytes
 *	@param bSucceeded			- [out] Upon return, whether it succeeded or not
 **/
void Renderthread_StreamOutTextureData( TArray<FTextureSortElement>* InCandidateTextures, INT RequiredMemorySize, volatile UBOOL* bSucceeded )
{
	*bSucceeded = FALSE;

#if !USE_NULL_RHI
	RHIBeginScene();

	INT OldAllocatedMemorySize = INDEX_NONE;
	INT OldAvailableMemorySize = INDEX_NONE;
	INT OldPendingMemoryAdjustment = INDEX_NONE;
	UBOOL bRHISupportsMemoryStats = RHIGetTextureMemoryStats( OldAllocatedMemorySize, OldAvailableMemorySize, OldPendingMemoryAdjustment );

	// Makes sure that texture memory can get freed up right away.
	RHIBlockUntilGPUIdle();

	FTextureSortElement* CandidateTextures = InCandidateTextures->GetTypedData();

#if XBOX && !USE_NULL_RHI
	// Fill in the texture base address
	for ( INT TextureIndex=0; TextureIndex < InCandidateTextures->Num(); ++TextureIndex )
	{
		FTextureSortElement& SortElement = CandidateTextures[TextureIndex];
		FXeTextureBase* XeTexture = SortElement.Texture->Resource->TextureRHI;
		SortElement.TextureDataAddress = DWORD(XeTexture->BaseAddress);
	}
#endif

	// Sort the candidates.
	::Sort<FTextureSortElement, FTextureStreamingCompare>( CandidateTextures, InCandidateTextures->Num() );

	// Attempt to shrink enough candidates to free up the required memory size. One mip-level at a time.
	INT SavedMemory = 0;
	UBOOL bKeepShrinking = TRUE;
	UBOOL bShrinkCharacterTextures = FALSE;	// Don't shrink any character textures the first loop.
	while ( SavedMemory < RequiredMemorySize && bKeepShrinking )
	{
		// If we can't shrink anything in the inner-loop, stop the outer-loop as well.
		bKeepShrinking = !bShrinkCharacterTextures;

		for ( INT TextureIndex=0; TextureIndex < InCandidateTextures->Num() && SavedMemory < RequiredMemorySize; ++TextureIndex )
		{
			FTextureSortElement& SortElement = CandidateTextures[TextureIndex];
			INT NewMipCount = SortElement.Texture->ResidentMips - 1;
			if ( (!SortElement.bIsCharacterTexture || bShrinkCharacterTextures) && NewMipCount >= SortElement.NumRequiredResidentMips )
			{
				FTexture2DResource* Resource = (FTexture2DResource*) SortElement.Texture->Resource;

				UBOOL bReallocationSucceeded = Resource->TryReallocate( SortElement.Texture->ResidentMips, NewMipCount );
				if ( bReallocationSucceeded )
				{
					// Start using the new one.
					INT OldSize = SortElement.Size;
					INT NewSize = SortElement.Texture->CalcTextureMemorySize(NewMipCount);
					INT Savings = OldSize - NewSize;

					// Set up UTexture2D
					SortElement.Texture->ResidentMips = NewMipCount;
					SortElement.Texture->RequestedMips = NewMipCount;

					// Ok, we found at least one we could shrink. Lets try to find more! :)
					bKeepShrinking = TRUE;

					SavedMemory += Savings;
				}
			}
		}

		// Start shrinking character textures as well, if we have to.
		bShrinkCharacterTextures = TRUE;
	}

	// Block until all shrinking is finished.
#if XBOX
	GTexturePool.ForceSync();
#elif PS3
	//@TODO: Make a ForceSync for PS3.
	GPS3Gcm->GetTexturePool()->FinishAllRelocations();
#endif

	INT NewAllocatedMemorySize = INDEX_NONE;
	INT NewAvailableMemorySize = INDEX_NONE;
	INT NewPendingMemoryAdjustment = INDEX_NONE;
	bRHISupportsMemoryStats = RHIGetTextureMemoryStats( NewAllocatedMemorySize, NewAvailableMemorySize, NewPendingMemoryAdjustment );

	debugf(TEXT("Streaming out texture memory! Saved %.2f MB."), FLOAT(SavedMemory)/1024.0f/1024.0f);

	// Return the result.
	*bSucceeded = SavedMemory >= RequiredMemorySize;

	RHIEndScene();
#endif
}

/**
 *	Try to stream out texture mip-levels to free up more memory.
 *	@param RequiredMemorySize	- Additional texture memory required
 *	@return						- Whether it succeeded or not
 **/
UBOOL FStreamingManagerTexture::StreamOutTextureData( INT RequiredMemorySize )
{
	RequiredMemorySize = Max<INT>(RequiredMemorySize, MinEvictSize);

	// Array of candidates for reducing mip-levels.
	TArray<FTextureSortElement> CandidateTextures;
	CandidateTextures.Reserve( 1024 );

	// Don't stream out character textures (to begin with)
	FLOAT CurrentTime = FLOAT(appSeconds() - GStartTime);

	// Collect all textures will be considered for streaming out.
	TLinkedList<UTexture2D*>* CurrentStreamableLink = UTexture2D::GetStreamableList();
	while ( CurrentStreamableLink )
	{
		// Advance streamable link.
		UTexture2D* Texture		= **CurrentStreamableLink;
		CurrentStreamableLink	= CurrentStreamableLink->Next();

		// Skyboxes should not stream out.
		if ( Texture->LODGroup == TEXTUREGROUP_Skybox )
			continue;

		// Number of mip-levels that must be resident due to mip-tails and GMinTextureResidentMipCount.
		INT NumRequiredResidentMips = (Texture->MipTailBaseIdx >= 0) ? Max<INT>(Texture->Mips.Num() - Texture->MipTailBaseIdx, 0 ) : 0;
		NumRequiredResidentMips = Max<INT>(NumRequiredResidentMips, GMinTextureResidentMipCount);

		// Only consider streamable textures that have enough miplevels, and who are currently ready for streaming.
		if ( Texture->bIsStreamable && Texture->NeverStream == FALSE && Texture->ResidentMips > NumRequiredResidentMips && Texture->IsReadyForStreaming() )
		{
			// We can't stream out mip-tails.
			INT CurrentBaseMip = Texture->Mips.Num() - Texture->ResidentMips;
			if ( Texture->MipTailBaseIdx < 0 || CurrentBaseMip < Texture->MipTailBaseIdx )
			{
				// Figure out whether texture should be forced resident based on bools and forced resident time.
				UBOOL bForceMipLevelsToBeResident = (Texture->ShouldMipLevelsBeForcedResident() || Texture->ForceMipLevelsToBeResidentTimestamp >= CurrentTime);
				if ( bForceMipLevelsToBeResident == FALSE && Texture->Resource )
				{
					// Don't try to stream out if the texture isn't ready.
					UBOOL bSafeToStream = (Texture->UpdateStreamingStatus() == FALSE);
					if ( bSafeToStream )
					{
						UBOOL bIsCharacterTexture =	Texture->LODGroup == TEXTUREGROUP_Character ||
													Texture->LODGroup == TEXTUREGROUP_CharacterSpecular ||
													Texture->LODGroup == TEXTUREGROUP_CharacterNormalMap;
						DWORD TextureDataAddress = 0;
						CandidateTextures.AddItem( FTextureSortElement(Texture, Texture->CalcTextureMemorySize(Texture->ResidentMips), bIsCharacterTexture ? 1 : 0, TextureDataAddress, NumRequiredResidentMips) );
					}
				}
			}
		}
	}

	volatile UBOOL bSucceeded = FALSE;

	// Queue up the process on the render thread.
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		StreamOutTextureDataCommand,
		TArray<FTextureSortElement>*,CandidateTextures,&CandidateTextures,
		INT,RequiredMemorySize,RequiredMemorySize,
		volatile UBOOL*,bSucceeded,&bSucceeded,
	{
		Renderthread_StreamOutTextureData( CandidateTextures, RequiredMemorySize, bSucceeded );
	});

	// Block until the command has finished executing.
	FlushRenderingCommands();

	// Reset the streaming system, so it picks up any changes to UTexture2D ResidentMips and RequestedMips.
	ProcessingStage = 0;

	return bSucceeded;
}

/**
 * Adds new textures and level data on the gamethread (while the worker thread isn't active).
 */
void FStreamingManagerTexture::UpdateThreadData()
{
	// Add new textures.
	StreamingTextures.Reserve( StreamingTextures.Num() + PendingStreamingTextures.Num() );
	for ( INT TextureIndex=0; TextureIndex < PendingStreamingTextures.Num(); ++TextureIndex )
	{
		UTexture2D* Texture = PendingStreamingTextures( TextureIndex );
		FStreamingTexture* StreamingTexture = new (StreamingTextures) FStreamingTexture( Texture );
		StreamingTexture->SetStreamingIndex( StreamingTextures.Num() - 1 );
	}
	PendingStreamingTextures.Empty();

	// Remove old levels. Note: Don't try to access the ULevel object, it may have been deleted already!
	for ( INT LevelIndex=0; LevelIndex < ThreadSettings.LevelData.Num(); ++LevelIndex )
	{
		FLevelData& LevelData = ThreadSettings.LevelData( LevelIndex );
		if ( LevelData.Value.bRemove )
		{
			ThreadSettings.LevelData.RemoveSwap( LevelIndex-- );
		}
	}

	// Add or remove pending spawned primitives.
	for ( TMap<const UPrimitiveComponent*,FPendingPrimitiveType>::TIterator It(PendingSpawnedPrimitives); It; ++It )
	{
		const UPrimitiveComponent* Primitive = It.Key();
		FPendingPrimitiveType PendingType = It.Value();

		if ( PendingType.bShouldTrack )
		{
			AddDynamicPrimitive( Primitive, PendingType.DynamicType );
		}
		else
		{
			// Remove from our Primitive-to-Instance mapping.
			RemoveDynamicPrimitive( Primitive, PendingType.DynamicType );
		}
	}
	PendingSpawnedPrimitives.Empty();

	// Add new levels.
	for ( INT LevelIndex=0; LevelIndex < PendingLevels.Num(); ++LevelIndex )
	{
		ULevel* Level = PendingLevels( LevelIndex );
		FLevelData* LevelData = new (ThreadSettings.LevelData) FLevelData( Level );
		FThreadLevelData& ThreadLevelData = LevelData->Value;

		// Increase the the force-fully-load refcount.
		for ( TMap<UTexture2D*,UBOOL>::TIterator It(Level->ForceStreamTextures); It; ++It )
		{
			UTexture2D* Texture2D = It.Key();
			if ( Texture2D && FStreamingTexture::IsStreamingTexture( Texture2D ) )
			{
				FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
				check( StreamingTexture.ForceLoadRefCount >= 0 );
				StreamingTexture.ForceLoadRefCount++;
			}
		}

		// Copy all texture instances into ThreadLevelData.
		if ( bAllowSwitchingStreamingSystem || bUsePriorityStreaming )
		{
			//Note: ULevel could save it in this form, except it's harder to debug.
			ThreadLevelData.ThreadTextureInstances.Empty( Level->TextureToInstancesMap.Num() );
			for ( TMap<UTexture2D*,TArray<FStreamableTextureInstance> >::TIterator It(Level->TextureToInstancesMap); It; ++It )
			{
				// Convert to SIMD-friendly form
				const UTexture2D* Texture2D = It.Key();
				if ( Texture2D )
				{
					TArray<FStreamableTextureInstance>& TextureInstances = It.Value();
					TArray<FStreamableTextureInstance4>& TextureInstances4 = ThreadLevelData.ThreadTextureInstances.Set( Texture2D, TArray<FStreamableTextureInstance4>() );
					TextureInstances4.Reserve( (TextureInstances.Num() + 3) / 4 );

					for ( INT InstanceIndex=0; InstanceIndex < TextureInstances.Num(); ++InstanceIndex )
					{
						if ( (InstanceIndex & 3) == 0 )
						{
							TextureInstances4.AddItem(FStreamableTextureInstance4());
						}
						const FStreamableTextureInstance& Instance = TextureInstances( InstanceIndex );
						FStreamableTextureInstance4& Instance4 = TextureInstances4( InstanceIndex/4 );
						Instance4.BoundingSphereX[ InstanceIndex & 3 ] = Instance.BoundingSphere.Center.X;
						Instance4.BoundingSphereY[ InstanceIndex & 3 ] = Instance.BoundingSphere.Center.Y;
						Instance4.BoundingSphereZ[ InstanceIndex & 3 ] = Instance.BoundingSphere.Center.Z;
						Instance4.BoundingSphereRadius[ InstanceIndex & 3 ] = Instance.BoundingSphere.W;
						Instance4.TexelFactor[ InstanceIndex & 3 ] = Instance.TexelFactor;
					}

					// Note: The old instance array must remain, in case a level is added/removed and then added again.
				}
			}
		}
	}
	PendingLevels.Empty();

	// Boost local player textures.
	if ( GEngine )
	{
		for  (INT PlayerIndex = 0; PlayerIndex < GEngine->GamePlayers.Num(); PlayerIndex++ )
		{
			ULocalPlayer* Player = GEngine->GamePlayers( PlayerIndex );
			if ( Player && Player->Actor && Player->Actor->ViewTarget )
			{
				BoostTextures( Player->Actor->ViewTarget, BoostPlayerTextures );
			}
		}
	}

	// Cache the view information.
	ThreadSettings.ThreadViewInfos = CurrentViewInfos;

	// Update the thread-safe cache information for dynamic primitives.
	UpdateDynamicPrimitiveCache();
}

/**
 * Temporarily boosts the streaming distance factor by the specified number.
 * This factor is automatically reset to 1.0 after it's been used for mip-calculations.
 */
void FStreamingManagerTexture::BoostTextures( AActor* Actor, FLOAT BoostFactor )
{
	if ( Actor )
	{
		TArray<UTexture*> Textures;
		Textures.Empty( 32 );

		for(INT ComponentIndex = 0;ComponentIndex < Actor->AllComponents.Num();ComponentIndex++)
		{
			UActorComponent* Component = Actor->AllComponents(ComponentIndex);
			if ( Component )
			{
				// Only handle skeletalmeshes and staticmeshes.
				UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
				if ( Primitive && ( Primitive->IsA(USkeletalMeshComponent::StaticClass()) || Primitive->IsA(UStaticMeshComponent::StaticClass()) ) )
				{
					Textures.Reset();
					Primitive->GetUsedTextures( Textures );
					for ( INT TextureIndex=0; TextureIndex < Textures.Num(); ++TextureIndex )
					{
						UTexture2D* Texture2D = Cast<UTexture2D>( Textures( TextureIndex ) );
						if ( Texture2D && IsManagedStreamingTexture(Texture2D) )
						{
							FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
							StreamingTexture.BoostFactor = Max( StreamingTexture.BoostFactor, BoostFactor );
						}
					}
				}
			}
		}
	}
}

/**
 * Updates the thread-safe cache information for dynamic primitives.
 */
void FStreamingManagerTexture::UpdateDynamicPrimitiveCache()
{
	STAT( DOUBLE StartTime = appSeconds() );

	for ( TMap<const UPrimitiveComponent*,FSpawnedPrimitiveData>::TIterator It(ThreadSettings.SpawnedPrimitives); It; ++It )
	{
		const UPrimitiveComponent* Primitive = It.Key();
		FSpawnedPrimitiveData& PrimitiveData = It.Value();
		UBOOL bIsAttachedNow = Primitive->IsAttached();
		if ( !PrimitiveData.bPendingUpdate && PrimitiveData.bAttached && !bIsAttachedNow )
		{
			// Mark the texture instances so we don't immediately switch to LastRenderTime unnecessarily.
			SetInstanceRemovedTimestamp( PrimitiveData );
		}
		PrimitiveData.BoundingSphere = Primitive->Bounds.GetSphere();
		PrimitiveData.bAttached = bIsAttachedNow;
	}
	STAT( GStreamingDynamicPrimitivesTime += appSeconds() - StartTime );
}

UBOOL FStreamingManagerTexture::AddDynamicPrimitive( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType )
{
	//@TODO: Uncomment this check, if possible.
//	check(Primitive->IsValidComponent());

	INT NumTexturesAdded = 0;

	if (Primitive && Primitive->IsValidComponent())
	{
		TArray<FStreamingTexturePrimitiveInfo> TextureInstanceInfos;
		Primitive->GetStreamingTextureInfo( TextureInstanceInfos );

		FSpawnedPrimitiveData* PrimitiveData = NULL;
		TArray<FSpawnedTextureInstance>* TextureInstances = NULL;
		for ( INT InstanceIndex=0; InstanceIndex < TextureInstanceInfos.Num(); ++InstanceIndex )
		{
			FStreamingTexturePrimitiveInfo& Info = TextureInstanceInfos(InstanceIndex);
			UTexture2D* Texture2D = Cast<UTexture2D>(Info.Texture);
			if ( Texture2D && IsManagedStreamingTexture(Texture2D) && Info.TexelFactor > 0.0f && Info.Bounds.W > 0.0f )
			{
				// Create instance array on first use
				if ( TextureInstances == NULL )
				{
					PrimitiveData = &ThreadSettings.SpawnedPrimitives.Set( Primitive, FSpawnedPrimitiveData() );
					TextureInstances = &PrimitiveData->TextureInstances;
				}
				// Check against duplicate instances
				UBOOL bExistedAlready = FALSE;
				for ( INT ExistingTextureIndex=0; ExistingTextureIndex < TextureInstances->Num(); ++ExistingTextureIndex )
				{
					const FSpawnedTextureInstance& ExistingInstance = (*TextureInstances)(ExistingTextureIndex);
					if ( ExistingInstance.Texture2D == Texture2D &&
						 appIsNearlyEqual(ExistingInstance.TexelFactor, Info.TexelFactor) && 
						 appIsNearlyEqual(ExistingInstance.InvOriginalRadius, 1.0f/Info.Bounds.W) )
					{
						bExistedAlready = TRUE;
						break;
					}
				}
				if ( !bExistedAlready )
				{
					FSpawnedTextureInstance* TextureInstance = new (*TextureInstances) FSpawnedTextureInstance(Texture2D, Info.TexelFactor, Info.Bounds.W);
				}
			}
		}
		if ( TextureInstances )
		{
			PrimitiveData->BoundingSphere	= Primitive->Bounds.GetSphere();
			PrimitiveData->bAttached		= TRUE;
			PrimitiveData->bPendingUpdate	= FALSE;
			PrimitiveData->DynamicType		= DynamicType;
			NumTexturesAdded += TextureInstances->Num();
		}
	}

#if STREAMING_LOG_DYNAMIC
	const USkeletalMeshComponent* SkeletalMeshComponent = ConstCast<USkeletalMeshComponent>(Primitive);
	const UStaticMeshComponent* StaticMeshComponent = ConstCast<UStaticMeshComponent>(Primitive);
	if ( SkeletalMeshComponent && SkeletalMeshComponent->SkeletalMesh )
	{
		debugf(TEXT("AddDynamicPrimitive(\"%s\", \"%s\"), IsValid=%d, NumTextures=%d"), *SkeletalMeshComponent->SkeletalMesh->GetName(), *SkeletalMeshComponent->GetFullName(), Primitive->IsValidComponent(), NumTexturesAdded);
#if _DEBUG
//@DEBUG: To set a breakpoint on a specific skelmesh
		if ( SkeletalMeshComponent->SkeletalMesh->GetName().InStr(TEXT("SpecificMeshName")) != INDEX_NONE )
		{
			INT Q=0;
		}
#endif
	}
	else if ( StaticMeshComponent && StaticMeshComponent->StaticMesh )
	{
		debugf(TEXT("AddDynamicPrimitive(\"%s\"), IsValid=%d, NumTextures=%d"), *StaticMeshComponent->StaticMesh->GetName(), Primitive->IsValidComponent(), NumTexturesAdded);
	}
	else
	{
		debugf(TEXT("AddDynamicPrimitive(\"%s\"), IsValid=%d, NumTextures=%d"), *Primitive->GetName(), Primitive->IsValidComponent(), NumTexturesAdded);
	}
#endif

	if ( NumTexturesAdded > 0 )
	{
		return TRUE;
	}
	else
	{
		// It didn't have any interesting textures.
		return FALSE;
	}
}

UBOOL FStreamingManagerTexture::RemoveDynamicPrimitive( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType )
{
	// Remove from our Primitive-to-Instance mapping.
	return ThreadSettings.SpawnedPrimitives.Remove( Primitive ) > 0;
}

/** Adds a ULevel to the streaming manager. */
void FStreamingManagerTexture::AddLevel( ULevel* Level )
{
	PendingLevels.AddUniqueItem( Level );

	if ( bUseDynamicStreaming )
	{
		// Add all dynamic primitives from the ULevel.
		for ( TMap<UPrimitiveComponent*,TArray<FDynamicTextureInstance> >::TIterator It(Level->DynamicTextureInstances); It; ++It )
		{
			UPrimitiveComponent* Primitive = It.Key();
			TArray<FDynamicTextureInstance>& TextureInstances = It.Value();
			NotifyPrimitiveAttached( Primitive, DPT_Level );

			//@TODO: This variable is deprecated, remove from serialization.
			TextureInstances.Empty();
		}
	}
}

/** Removes a ULevel from the streaming manager. */
void FStreamingManagerTexture::RemoveLevel( ULevel* Level )
{
	// Remove from pending new levels, if it's been added very recently (this frame)...
	PendingLevels.RemoveItem( Level );

	// Mark the level for removal (will take effect when we're syncing threads).
	FLevelData* LevelData = ThreadSettings.LevelData.FindItemByKey( Level );
	if ( LevelData && !LevelData->Value.bRemove )
	{
		FThreadLevelData& ThreadLevelData = LevelData->Value;
		ThreadLevelData.bRemove = TRUE;

		// Mark all textures with a timestamp. They're about to loose their location-based heuristic and we don't want them to
		// start using LastRenderTime heuristic for a few seconds until they are garbage collected!
		for( TMap<UTexture2D*,TArray<FStreamableTextureInstance> >::TIterator It(Level->TextureToInstancesMap); It; ++It )
		{
			const UTexture2D* Texture2D = It.Key();
			if ( Texture2D && IsManagedStreamingTexture( Texture2D ) )
			{
				FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
				StreamingTexture.InstanceRemovedTimestamp = GCurrentTime;
			}
		}

		// Decrease the the force-fully-load refcount. 
		// Note: Only update ForceLoadRefCount if the level was in the ThreadSettings and therefore bumped ForceLoadRefCount earlier.
		// Note: We do this immediately, since ULevel may be being deleted.
		for ( TMap<UTexture2D*,UBOOL>::TIterator It(Level->ForceStreamTextures); It; ++It )
		{
			UTexture2D* Texture2D = It.Key();
			if ( Texture2D && IsManagedStreamingTexture( Texture2D ) )
			{
				FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
				// Note: This refcount should always be positive, but for some reason it may not be when BuildStreamingData() is called in the editor...
				if ( StreamingTexture.ForceLoadRefCount > 0 )
				{
					StreamingTexture.ForceLoadRefCount--;
				}
			}
		}
	}

	if ( bUseDynamicStreaming )
	{
		// Mark all dynamic primitives from the ULevel (they'll be removed completely next sync).
		for ( TMap<UPrimitiveComponent*,TArray<FDynamicTextureInstance> >::TIterator It(Level->DynamicTextureInstances); It; ++It )
		{
			UPrimitiveComponent* Primitive = It.Key();
			NotifyPrimitiveDetached( Primitive );
		}
	}
}

/**
 * Adds a new texture to the streaming manager.
 */
void FStreamingManagerTexture::AddStreamingTexture( UTexture2D* Texture )
{
	// Adds the new texture to the Pending list, to avoid reallocation of the thread-safe StreamingTextures array.
	check( Texture->StreamingIndex == -1 );
	INT Index = PendingStreamingTextures.AddItem( Texture );
	Texture->StreamingIndex = Index;
}

/**
 * Removes a texture from the streaming manager.
 */
void FStreamingManagerTexture::RemoveStreamingTexture( UTexture2D* Texture )
{
	// Removes the texture from the Pending list or marks the StreamingTextures slot as unused.

	// Remove it from the Pending list.
	INT Index = Texture->StreamingIndex;
	if ( Index >= 0 && Index < PendingStreamingTextures.Num() )
	{
		UTexture2D* ExistingPendingTexture = PendingStreamingTextures( Index );
		if ( ExistingPendingTexture == Texture )
		{
			PendingStreamingTextures.RemoveSwap( Index );
			if ( Index != PendingStreamingTextures.Num() )
			{
				UTexture2D* SwappedPendingTexture = PendingStreamingTextures( Index );
				SwappedPendingTexture->StreamingIndex = Index;
			}
			Texture->StreamingIndex = -1;
		}
	}

	Index = Texture->StreamingIndex;
	if ( Index >= 0 && Index < StreamingTextures.Num() )
	{
		FStreamingTexture& ExistingStreamingTexture = StreamingTextures( Index );
		if ( ExistingStreamingTexture.Texture == Texture )
		{
			if ( bUsePriorityStreaming )
			{
				// If using the new priority system, mark StreamingTextures slot as unused.
				ExistingStreamingTexture.Texture = NULL;
			}
			else
			{
				// If using old system, remove it immediately.
				StreamingTextures.RemoveSwap( Index );
				if ( Index != StreamingTextures.Num() )
				{
					FStreamingTexture& SwappedTexture = StreamingTextures( Index );
					// Note: The swapped texture may also be pending deletion.
					if ( SwappedTexture.Texture )
					{
						SwappedTexture.Texture->StreamingIndex = Index;
					}
				}
			}
			Texture->StreamingIndex = -1;
		}
	}

//	checkSlow( Texture->StreamingIndex == -1 );	// The texture should have been in one of the two arrays!
	Texture->StreamingIndex = -1;
}

/** Called when an actor is spawned. */
void FStreamingManagerTexture::NotifyActorSpawned( AActor* Actor )
{
	if ( bUseDynamicStreaming )
	{
		for(INT ComponentIndex = 0;ComponentIndex < Actor->AllComponents.Num();ComponentIndex++)
		{
			UActorComponent* Component = Actor->AllComponents(ComponentIndex);
			if ( Component )
			{
				// Only handle skeletalmeshes and staticmeshes.
				const UPrimitiveComponent* Primitive = ConstCast<UPrimitiveComponent>(Component);
				if ( Primitive && ( Primitive->IsA(USkeletalMeshComponent::StaticClass()) || Primitive->IsA(UStaticMeshComponent::StaticClass()) ) )
				{
					NotifyPrimitiveAttached( Primitive, DPT_Spawned );
				}
			}
		}
	}
}

/** Called when a spawned primitive is deleted. */
void FStreamingManagerTexture::NotifyActorDestroyed( AActor* Actor )
{
	for(INT ComponentIndex = 0;ComponentIndex < Actor->AllComponents.Num();ComponentIndex++)
	{
		UActorComponent* Component = Actor->AllComponents(ComponentIndex);
		if ( Component )
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>( Component );
			if ( Primitive )
			{
				NotifyPrimitiveDetached( Primitive );
			}
		}
	}
}

/**
 * Called when a primitive is attached to an actor or another component.
 * Replaces previous info, if the primitive was already attached.
 *
 * @param InPrimitive	Newly attached dynamic/spawned primitive
 */
void FStreamingManagerTexture::NotifyPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType )
{
	// Only add it if it's a UMeshComponent, since we only track those in UMeshComponent::BeginDestroy().
	if ( bUseDynamicStreaming && Primitive && Primitive->IsValidComponent() && Primitive->IsA(UMeshComponent::StaticClass()) )
	{

#if STREAMING_LOG_DYNAMIC
		const USkeletalMeshComponent* SkeletalMeshComponent = ConstCast<USkeletalMeshComponent>(Primitive);
		const UStaticMeshComponent* StaticMeshComponent = ConstCast<UStaticMeshComponent>(Primitive);
		if ( SkeletalMeshComponent && SkeletalMeshComponent->SkeletalMesh )
		{
			debugf(TEXT("NotifyPrimitiveAttached(\"%s\"), IsValid=%d"), *SkeletalMeshComponent->SkeletalMesh->GetName(), Primitive->IsValidComponent());
		}
		else if ( StaticMeshComponent && StaticMeshComponent->StaticMesh )
		{
			debugf(TEXT("NotifyPrimitiveAttached(\"%s\"), IsValid=%d"), *StaticMeshComponent->StaticMesh->GetName(), Primitive->IsValidComponent());
		}
		else
		{
			debugf(TEXT("NotifyPrimitiveAttached(\"%s\"), IsValid=%d"), *Primitive->GetName(), Primitive->IsValidComponent());
		}
#endif

		// If we already have a pending update, keep its current dynamic type.
		FPendingPrimitiveType* PendingType = PendingSpawnedPrimitives.Find( Primitive );
		if ( PendingType )
		{
			DynamicType = PendingType->DynamicType;
		}

		// If we're already tracking this primitive, keep its current dynamic type.
		FSpawnedPrimitiveData* PrimitiveData = ThreadSettings.SpawnedPrimitives.Find( Primitive );
		if ( PrimitiveData )
		{
			DynamicType = PrimitiveData->DynamicType;
			// Set the pending update flag, to indicate that it shouldn't be processed until it's been fully updated.
			PrimitiveData->bPendingUpdate = TRUE;
		}

		// Mark the primitive for add.
		PendingSpawnedPrimitives.Set( Primitive, FPendingPrimitiveType(DynamicType, TRUE) );
	}
}

/**
 * Called when a primitive is detached from an actor or another component.
 * Note: We should not be accessing the primitive or the UTexture2D after this call!
 */
void FStreamingManagerTexture::NotifyPrimitiveDetached( const UPrimitiveComponent* Primitive )
{
	if ( bUseDynamicStreaming && Primitive )
	{
#if STREAMING_LOG_DYNAMIC
		const USkeletalMeshComponent* SkeletalMeshComponent = ConstCast<USkeletalMeshComponent>(Primitive);
		const UStaticMeshComponent* StaticMeshComponent = ConstCast<UStaticMeshComponent>(Primitive);
		if ( SkeletalMeshComponent && SkeletalMeshComponent->SkeletalMesh )
		{
			debugf(TEXT("NotifyPrimitiveDetached(\"%s\", \"%s\"), IsValid=%d"), *SkeletalMeshComponent->SkeletalMesh->GetName(), *SkeletalMeshComponent->GetFullName(), Primitive->IsValidComponent());
		}
		else if ( StaticMeshComponent && StaticMeshComponent->StaticMesh )
		{
			debugf(TEXT("NotifyPrimitiveDetached(\"%s\"), IsValid=%d"), *StaticMeshComponent->StaticMesh->GetName(), Primitive->IsValidComponent());
		}
		else
		{
			debugf(TEXT("NotifyPrimitiveDetached(\"%s\"), IsValid=%d"), *Primitive->GetName(), Primitive->IsValidComponent());
		}
#endif

		// Did we already mark this as detached?
		FPendingPrimitiveType* CurrentPendingType = PendingSpawnedPrimitives.Find( Primitive );
		if ( CurrentPendingType && CurrentPendingType->bShouldTrack == FALSE )
		{
			// Don't touch anything.
			return;
		}

		FSpawnedPrimitiveData* PrimitiveData = ThreadSettings.SpawnedPrimitives.Find( Primitive );
		if ( PrimitiveData )
		{
			UBOOL bWasAlreadyPendingUpdate = PrimitiveData->bPendingUpdate;

			// Mark the primitive as detached, if we're already using it.
			PrimitiveData->bAttached = FALSE;

			// Set the pending update flag, to indicate that it shouldn't be processed until it's been fully updated.
			PrimitiveData->bPendingUpdate = TRUE;

			// Queue the primitive for removal.
			PendingSpawnedPrimitives.Set( Primitive, FPendingPrimitiveType(PrimitiveData->DynamicType, FALSE) );

			// Was it already marked for a full refresh?
			if ( bWasAlreadyPendingUpdate )
			{
				// Don't touch it. The original primitive could've been detached (destroyed) at some point recently.
				return;
			}

			SetInstanceRemovedTimestamp( *PrimitiveData );
		}
		else
		{
			PendingSpawnedPrimitives.Remove( Primitive );
		}
	}
}

/**
 * Mark the textures instances with a timestamp. They're about to loose their location-based heuristic and we don't want them to
 * start using LastRenderTime heuristic for a few seconds until they are garbage collected!
 *
 * @param PrimitiveData		Our data structure for the spawned primitive that is being detached.
 */
void FStreamingManagerTexture::SetInstanceRemovedTimestamp( FSpawnedPrimitiveData& PrimitiveData )
{
	for ( INT InstanceIndex=0; InstanceIndex < PrimitiveData.TextureInstances.Num(); ++InstanceIndex )
	{
		UTexture2D* Texture2D = PrimitiveData.TextureInstances( InstanceIndex ).Texture2D;
		if ( Texture2D && IsManagedStreamingTexture(Texture2D) )
		{
			FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
			StreamingTexture.InstanceRemovedTimestamp = GCurrentTime;
		}
	}
}

/**
 * Called when a primitive has had its textured changed.
 * Only affects primitives that were already attached.
 * Replaces previous info.
 */
void FStreamingManagerTexture::NotifyPrimitiveUpdated( const UPrimitiveComponent* Primitive )
{
	// Only update it if we're currently tracking it, and it's not about to be removed.
	FPendingPrimitiveType* PendingType = PendingSpawnedPrimitives.Find(Primitive);
	UBOOL bIsCurrentlyTracked = PendingType || ThreadSettings.SpawnedPrimitives.HasKey(Primitive);
	UBOOL bIsBeingRemoved = PendingType && !PendingType->bShouldTrack;
	if ( bIsCurrentlyTracked && !bIsBeingRemoved )
	{
#if STREAMING_LOG_DYNAMIC
		const USkeletalMeshComponent* SkeletalMeshComponent = ConstCast<USkeletalMeshComponent>(Primitive);
		const UStaticMeshComponent* StaticMeshComponent = ConstCast<UStaticMeshComponent>(Primitive);
		if ( SkeletalMeshComponent && SkeletalMeshComponent->SkeletalMesh )
		{
			debugf(TEXT("NotifyPrimitiveUpdated(\"%s\"), IsValid=%d"), *SkeletalMeshComponent->SkeletalMesh->GetName(), Primitive->IsValidComponent());
		}
		else if ( StaticMeshComponent && StaticMeshComponent->StaticMesh )
		{
			debugf(TEXT("NotifyPrimitiveUpdated(\"%s\"), IsValid=%d"), *StaticMeshComponent->StaticMesh->GetName(), Primitive->IsValidComponent());
		}
		else
		{
			debugf(TEXT("NotifyPrimitiveUpdated(\"%s\"), IsValid=%d"), *Primitive->GetName(), Primitive->IsValidComponent());
		}
#endif

		if ( Primitive->IsValidComponent() )
		{
			// Reattach the primitive, replacing previous info.
			NotifyPrimitiveAttached( Primitive, DPT_Spawned );
		}
		else
		{
			// Detach the primitive.
			NotifyPrimitiveDetached( Primitive );
		}
	}
}

/**
 * Called when a LastRenderTime primitive is attached to an actor or another component.
 * Modifies the LastRenderTimeRefCount for the textures used, so that those textures can
 * use both distance-based and LastRenderTime heuristics.
 *
 * @param Primitive		Newly attached dynamic/spawned primitive
 */
void FStreamingManagerTexture::NotifyTimedPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType )
{
	if ( Primitive && Primitive->IsValidComponent() )
	{
		TArray<FStreamingTexturePrimitiveInfo> TextureInstanceInfos;
		Primitive->GetStreamingTextureInfo( TextureInstanceInfos );

		FSpawnedPrimitiveData* PrimitiveData = NULL;
		TArray<FSpawnedTextureInstance>* TextureInstances = NULL;
		for ( INT InstanceIndex=0; InstanceIndex < TextureInstanceInfos.Num(); ++InstanceIndex )
		{
			FStreamingTexturePrimitiveInfo& Info = TextureInstanceInfos(InstanceIndex);
			UTexture2D* Texture2D = Cast<UTexture2D>(Info.Texture);
			if ( Texture2D && IsManagedStreamingTexture(Texture2D) )
			{
				// Note: Doesn't have to be cycle-perfect for thread safety.
				FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
				StreamingTexture.LastRenderTimeRefCount++;
				StreamingTexture.LastRenderTimeRefCountTimestamp = GCurrentTime;
			}
		}
	}
}

/**
 * Called when a LastRenderTime primitive is detached from an actor or another component.
 * Modifies the LastRenderTimeRefCount for the textures used, so that those textures can
 * use both distance-based and LastRenderTime heuristics.
 *
 * @param Primitive		Newly detached dynamic/spawned primitive
 */
void FStreamingManagerTexture::NotifyTimedPrimitiveDetached( const UPrimitiveComponent* Primitive )
{
	if ( Primitive && Primitive->IsValidComponent() )
	{
		TArray<FStreamingTexturePrimitiveInfo> TextureInstanceInfos;
		Primitive->GetStreamingTextureInfo( TextureInstanceInfos );

		FSpawnedPrimitiveData* PrimitiveData = NULL;
		TArray<FSpawnedTextureInstance>* TextureInstances = NULL;
		for ( INT InstanceIndex=0; InstanceIndex < TextureInstanceInfos.Num(); ++InstanceIndex )
		{
			FStreamingTexturePrimitiveInfo& Info = TextureInstanceInfos(InstanceIndex);
			UTexture2D* Texture2D = Cast<UTexture2D>(Info.Texture);
			if ( Texture2D && IsManagedStreamingTexture(Texture2D) )
			{
				// Note: Doesn't have to be cycle-perfect for thread safety.
				FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
				if ( StreamingTexture.LastRenderTimeRefCount > 0 )
				{
					StreamingTexture.LastRenderTimeRefCount--;
					StreamingTexture.LastRenderTimeRefCountTimestamp = GCurrentTime;
				}
			}
		}
	}
}

/**
 * Returns the corresponding FStreamingTexture for a UTexture2D.
 */
FStreamingTexture& FStreamingManagerTexture::GetStreamingTexture( const UTexture2D* Texture2D )
{
	return StreamingTextures( Texture2D->StreamingIndex );
}

/** Returns TRUE if this is a streaming texture that is managed by the streaming manager. */
UBOOL FStreamingManagerTexture::IsManagedStreamingTexture( const UTexture2D* Texture2D )
{
	return Texture2D->StreamingIndex >= 0 && Texture2D->StreamingIndex < StreamingTextures.Num() && FStreamingTexture::IsStreamingTexture( Texture2D );
}

/**
 * Updates streaming for an individual texture, taking into account all view infos.
 *
 * @param Texture	Texture to update
 */
void FStreamingManagerTexture::UpdateIndividualResource( UTexture2D* Texture )
{
	IndividualStreamingTexture = Texture;
	UpdateResourceStreaming( 0.0f );
	IndividualStreamingTexture = NULL;
}

/**
 * Resets the streaming statistics to zero.
 */
void FStreamingManagerTexture::ResetStreamingStats()
{
	NumStreamingTextures								= 0;
	NumRequestsInCancelationPhase						= 0;
	NumRequestsInUpdatePhase							= 0;
	NumRequestsInFinalizePhase							= 0;
	TotalIntermediateTexturesSize						= 0;
	NumIntermediateTextures								= 0;
	TotalStreamingTexturesSize							= 0;
	TotalStreamingTexturesMaxSize						= 0;
	TotalLightmapMemorySize								= 0;
	TotalLightmapDiskSize								= 0;
	TotalMipCountIncreaseRequestsInFlight				= 0;
	NumStreamingTextureInstances						= 0;
	NumStreamingLightmapInstances						= 0;
	LastFullIterationTime								= CurrentFullIterationTime;
	CurrentFullIterationTime							= 0;
	TotalOptimalWantedSize								= 0;
	TotalStaticTextureHeuristicSize						= 0;
	TotalDynamicTextureHeuristicSize					= 0;
	TotalLastRenderHeuristicSize						= 0;
	TotalForcedHeuristicSize							= 0;
}

/**
 * Updates the streaming statistics with current frame's worth of stats.
 *
 * @param Context					Context for the current frame
 * @param bAllTexturesProcessed		Whether all processing is complete
 */
void FStreamingManagerTexture::UpdateStreamingStats( const FStreamingContext& Context, UBOOL bAllTexturesProcessed )
{
	NumStreamingTextures					+= Context.ThisFrameNumStreamingTextures;
	NumRequestsInCancelationPhase			+= Context.ThisFrameNumRequestsInCancelationPhase;
	NumRequestsInUpdatePhase				+= Context.ThisFrameNumRequestsInUpdatePhase;
	NumRequestsInFinalizePhase				+= Context.ThisFrameNumRequestsInFinalizePhase;
	TotalIntermediateTexturesSize			= GStreamMemoryTracker.CalcTempMemory();
//	TotalIntermediateTexturesSize			+= Context.ThisFrameTotalIntermediateTexturesSize;
	NumIntermediateTextures					+= Context.ThisFrameNumIntermediateTextures;
	TotalStreamingTexturesSize				+= Context.ThisFrameTotalStreamingTexturesSize;
	TotalStreamingTexturesMaxSize			+= Context.ThisFrameTotalStreamingTexturesMaxSize;
	TotalLightmapMemorySize					+= Context.ThisFrameTotalLightmapMemorySize;
	TotalLightmapDiskSize					+= Context.ThisFrameTotalLightmapDiskSize;
	TotalMipCountIncreaseRequestsInFlight	+= Context.ThisFrameTotalMipCountIncreaseRequestsInFlight;
	TotalOptimalWantedSize					+= Context.ThisFrameOptimalWantedSize;
	TotalStaticTextureHeuristicSize			+= Context.ThisFrameTotalStaticTextureHeuristicSize;
	TotalDynamicTextureHeuristicSize		+= Context.ThisFrameTotalDynamicTextureHeuristicSize;
	TotalLastRenderHeuristicSize			+= Context.ThisFrameTotalLastRenderHeuristicSize;
	TotalForcedHeuristicSize				+= Context.ThisFrameTotalForcedHeuristicSize;


	// Set the stats on wrap-around. Reset happens independently to correctly handle resetting in the middle of iteration.
	if ( bAllTexturesProcessed )
	{
		SET_DWORD_STAT( STAT_StreamingTextures,				NumStreamingTextures			);
		SET_DWORD_STAT( STAT_RequestsInCancelationPhase,	NumRequestsInCancelationPhase	);
		SET_DWORD_STAT( STAT_RequestsInUpdatePhase,			NumRequestsInUpdatePhase		);
		SET_DWORD_STAT( STAT_RequestsInFinalizePhase,		NumRequestsInFinalizePhase		);
		SET_DWORD_STAT( STAT_IntermediateTexturesSize,		TotalIntermediateTexturesSize	);
		SET_DWORD_STAT( STAT_IntermediateTextures,			NumIntermediateTextures			);
		SET_DWORD_STAT( STAT_StreamingTexturesMaxSize,		TotalStreamingTexturesMaxSize	);
		SET_DWORD_STAT( STAT_LightmapMemorySize,			TotalLightmapMemorySize			);
		SET_DWORD_STAT( STAT_LightmapDiskSize,				TotalLightmapDiskSize			);
		SET_FLOAT_STAT( STAT_StreamingBandwidth,			BandwidthAverage/1024.0f/1024.0f);
		SET_FLOAT_STAT( STAT_StreamingLatency,				LatencyAverage					);
		SET_DWORD_STAT( STAT_NumStreamingTextureInstances,	NumStreamingTextureInstances	);
		SET_DWORD_STAT( STAT_NumStreamingLightmapInstances,	NumStreamingLightmapInstances	);
		SET_MEMORY_STAT_FAST( STAT_StreamingTexturesSize,	TotalStreamingTexturesSize		);
		SET_MEMORY_STAT_FAST( STAT_OptimalTextureSize,		TotalOptimalWantedSize			);
		SET_MEMORY_STAT( STAT_TotalStaticTextureHeuristicSize,	TotalStaticTextureHeuristicSize	);
		SET_MEMORY_STAT( STAT_TotalDynamicHeuristicSize,		TotalDynamicTextureHeuristicSize );
		SET_MEMORY_STAT( STAT_TotalLastRenderHeuristicSize,		TotalLastRenderHeuristicSize	);
		SET_MEMORY_STAT( STAT_TotalForcedHeuristicSize,			TotalForcedHeuristicSize		);

		if ( !bUsePriorityStreaming )
		{
			STAT_FAST( INT MemoryOverBudget = 0 );
			if ( Context.bRHISupportsMemoryStats )
			{
				STAT_FAST( const DWORD PoolSize = OriginalTexturePoolSize );
				STAT_FAST( const DWORD MyAllocatedSize = TotalStreamingTexturesSize + TotalIntermediateTexturesSize + Context.PendingMemoryAdjustment );
				STAT_FAST( const DWORD NonStreamingSize = Context.AllocatedMemorySize - MyAllocatedSize );
				STAT_FAST( const DWORD MemoryBudget = PoolSize - NonStreamingSize - MemoryDropMipLevelsLimit );
				STAT_FAST( MemoryOverBudget = INT(TotalOptimalWantedSize) - INT(MemoryBudget) );
			}
			SET_DWORD_STAT_FAST( STAT_StreamingOverBudget,	Max(MemoryOverBudget,0)	);
			SET_DWORD_STAT_FAST( STAT_StreamingUnderBudget,	Max(-MemoryOverBudget,0) );
		}

#if STATS_FAST
		MaxStreamingTexturesSize = Max(MaxStreamingTexturesSize, GET_MEMORY_STAT_FAST(STAT_StreamingTexturesSize));
		MaxOptimalTextureSize = Max(MaxOptimalTextureSize, GET_MEMORY_STAT_FAST(STAT_OptimalTextureSize));
		MaxStreamingOverBudget = Max<INT>(MaxStreamingOverBudget, INT(GET_MEMORY_STAT_FAST(STAT_StreamingOverBudget)) - INT(GET_MEMORY_STAT_FAST(STAT_StreamingUnderBudget)));
		MaxTexturePoolAllocatedSize = Max(MaxTexturePoolAllocatedSize, GET_MEMORY_STAT_FAST(STAT_TexturePoolAllocatedSize));
		MinLargestHoleSize = Min(MinLargestHoleSize, GET_MEMORY_STAT_FAST(STAT_TexturePool_LargestHole));
		MaxNumWantingTextures = Max(MaxNumWantingTextures, GET_MEMORY_STAT_FAST(STAT_NumWantingTextures));
#endif
	}

	SET_DWORD_STAT(		STAT_RequestSizeCurrentFrame,		Context.ThisFrameTotalRequestSize		);
	INC_DWORD_STAT_BY(	STAT_RequestSizeTotal,				Context.ThisFrameTotalRequestSize		);
	INC_DWORD_STAT_BY(	STAT_LightmapRequestSizeTotal,		Context.ThisFrameTotalLightmapRequestSize );
	SET_FLOAT_STAT(		STAT_StreamingFudgeFactor,			FudgeFactor );
	STAT( GStatManager.SetStatVisibility( STAT_StreamingFudgeFactor, bUsePriorityStreaming == FALSE ) );
}

/**
 * Updates streaming, taking into account all current view infos. Can be called multiple times per frame.
 *
 * @param DeltaTime				Time since last call in seconds
 * @param bProcessEverything	[opt] If TRUE, process all resources with no throttling limits
 */
void FStreamingManagerTexture::UpdateResourceStreaming( FLOAT DeltaTime, UBOOL bProcessEverything/*=FALSE*/ )
{
	SCOPE_CYCLE_COUNTER(STAT_GameThreadUpdateTime);

	if ( bUsePriorityStreaming )
	{
		NewUpdateResourceStreaming( DeltaTime, bProcessEverything );
		return;
	}

	// Setup a context for this run.
	FStreamingContext Context( bProcessEverything, IndividualStreamingTexture, bCollectTextureStats );

	UpdateThreadData();

	// Reset stats first texture. Split up from updating as GC might cause a reset in the middle of iteration
	// and we don't want to report bogus stats in this case.
	if( UTexture2D::GetCurrentStreamableLink() == UTexture2D::GetStreamableList() )
	{
		ResetStreamingStats();
	}

	UBOOL bUseMinRequestLimitOriginal = bUseMinRequestLimit;
	if ( bProcessEverything )
	{
		TLinkedList<UTexture2D*>*& CurrentStreamableLink = UTexture2D::GetCurrentStreamableLink();
		CurrentStreamableLink = UTexture2D::GetStreamableList();
		bUseMinRequestLimit = FALSE;
		RemainingTicksToDisregardWorldTextures = 0;
	}

	// Keep track of time a full iteration takes.
	CurrentFullIterationTime += DeltaTime;

	// Decrease counter to disregard world textures if it's set.
	if( RemainingTicksToDisregardWorldTextures > 0 )
	{
		RemainingTicksToDisregardWorldTextures--;
	}

	// Update suspend count.
	if( RemainingTicksToSuspendActivity > 0 )
	{
		RemainingTicksToSuspendActivity--;
	}

	if ( IndividualStreamingTexture == NULL )
	{
		// Update fudge factor after clamping delta time to something reasonable.
		MinFudgeFactor	= Clamp( MinFudgeFactor, 0.1f, 10.f );
		FudgeFactor		= Clamp( FudgeFactor + FudgeFactorRateOfChange * Min( 0.1f, DeltaTime ), MinFudgeFactor, 10.f );
	}

	// Get current streamable link and iterate over subset of textures each frame.
	TLinkedList<UTexture2D*>*& CurrentStreamableLink = UTexture2D::GetCurrentStreamableLink();
	while ( (IndividualStreamingTexture || CurrentStreamableLink) && Context.MaxTexturesToProcess-- )
	{
		// Advance streamable link.
		UTexture2D* Texture;
		if ( IndividualStreamingTexture == NULL )
		{
			Texture = **CurrentStreamableLink;
			CurrentStreamableLink = CurrentStreamableLink->Next();
		}
		else
		{
			Texture = IndividualStreamingTexture;
		}
		// This section can be used to track how specific textures are streamed.
		UBOOL bTrackedTexture = TrackTextureEvent( NULL, Texture, FALSE, ENABLE_TEXTURE_LOGGING, FALSE );

#ifdef _DEBUG
		if ( GDebugSelectedLightmap && GDebugSelectedLightmap->GetTexture(0) == Texture )
		{
			INT q=0;	// To be able to set breakpoints here.
		}
#endif

		// Skip world textures to allow e.g. character textures to stream in first.
		if( RemainingTicksToDisregardWorldTextures 
		&&	((	Texture->LODGroup != TEXTUREGROUP_Character)
			&&	Texture->LODGroup != TEXTUREGROUP_CharacterSpecular
			&&	Texture->LODGroup != TEXTUREGROUP_CharacterNormalMap) )
		{
			continue;
		}

		// If TRUE we're so low on memory that we cannot request any changes.
		// And due to the async nature of memory requests we must suspend activity for at least 2 frames.
		if ( Context.bRHISupportsMemoryStats && (Context.AvailableMemorySize < MemoryStopStreamingLimit) )
		{
			// Value gets decremented at top so setting it to 3 will suspend 2 ticks.
			RemainingTicksToSuspendActivity = 3;
		}
		UBOOL bAllowRequests = RemainingTicksToSuspendActivity == 0;

		// Only work on streamable textures.
		if ( FStreamingTexture::IsStreamingTexture(Texture) )
		{
			FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture );
			StreamingTexture.UpdateCachedInfo();

			// Figure out min and max number of miplevels allowed by LOD code.
			CalcMinMaxMips( StreamingTexture );

			// No need to figure out which miplevels we want if the texture is still in the process of being
			// created for the first time. We also cannot change the texture during that time outside of 
			// UpdateResource.
			if ( Texture->IsReadyForStreaming() )
			{
				// Determine how many mips this texture should have in memory.
				CalcWantedMips( StreamingTexture );

				// Determine whether we can update this texture.
				UpdateTextureStatus( StreamingTexture, Context );

				// Cancel current streaming request if necessary.
				ConditionallyCancelTextureStreaming( StreamingTexture, Context );

				// Request a change if it's safe and requested mip count differs from resident.
				if ( bAllowRequests && StreamingTexture.bInFlight == FALSE )
				{
					StartStreaming( StreamingTexture, StreamingTexture.WantedMips, Context, FALSE );
				}
			}

			UpdateFrameStats( StreamingTexture, Context );

			// This section can be used to track how specific textures are streamed.
			TrackTextureEvent( NULL, Texture, FALSE, ENABLE_TEXTURE_LOGGING, StreamingTexture.bForceFullyLoad );
		}
	}

	// Update fudge factor rate of change if memory stats are supported.
	if( Context.bRHISupportsMemoryStats )
	{
		if( Context.AvailableMemorySize <= MemoryDropMipLevelsLimit )
		{
			FudgeFactorRateOfChange = FudgeFactorIncreaseRateOfChange;
		}
		else if( Context.AvailableMemorySize > MemoryHysteresisLimit )
		{
			FudgeFactorRateOfChange	= FudgeFactorDecreaseRateOfChange;
			// Special handling if fudge factor is going to be less than 1.
			if( FudgeFactor < (1 - FudgeFactorDecreaseRateOfChange) )
			{
				// Multiply by min fudge factor to linearize range 0.1 .. 1
				FudgeFactorRateOfChange *= MinFudgeFactor;
			}
		}
		else
		{
			FudgeFactorRateOfChange = 0;
		}
	}

	if ( bProcessEverything )
	{
		bUseMinRequestLimit = bUseMinRequestLimitOriginal;
	}

	// Determine whether should disable this feature.
	if ( CurrentStreamableLink == NULL && bUseMinRequestLimit )
	{
		FLOAT TimeSinceLastLevelChange = appSeconds() - LastLevelChangeTime;
		// No longer use min request limit if we're out of requests and the min guaranteed time has elapsed...
		if( (TimeSinceLastLevelChange > MinTimeToGuaranteeMinMipCount && Context.ThisFrameTotalMipCountIncreaseRequestsInFlight == 0)
			// ... or if the max allowed time has elapsed.
			||	(TimeSinceLastLevelChange > MaxTimeToGuaranteeMinMipCount) )
		{
			bUseMinRequestLimit = FALSE;
		}
	}

	// Update running counts with this frames stats.
	UpdateStreamingStats( Context, CurrentStreamableLink == NULL );
}

/**
 * Not thread-safe: Updates a portion (as indicated by 'StageIndex') of all streaming textures,
 * allowing their streaming state to progress.
 *
 * @param Context			Context for the current stage (frame)
 * @param StageIndex		Current stage index
 * @param NumUpdateStages	Number of texture update stages
 */
void FStreamingManagerTexture::UpdateStreamingTextures( FStreamingContext& Context, INT StageIndex, INT NumUpdateStages )
{
	if ( StageIndex == 0 )
	{
		CurrentUpdateStreamingTextureIndex = 0;
	}
	INT StartIndex = CurrentUpdateStreamingTextureIndex;
	INT EndIndex = StreamingTextures.Num() * (StageIndex + 1) / NumUpdateStages;
	for ( INT Index=StartIndex; Index < EndIndex; ++Index )
	{
		FStreamingTexture& StreamingTexture = StreamingTextures( Index );
		PREFETCH( &StreamingTexture + 1 );
		PREFETCH( StreamingTexture.Texture );
		PREFETCH_NEXT_CACHE_LINE( StreamingTexture.Texture );

		// Is this texture marked for removal?
		if ( StreamingTexture.Texture == NULL )
		{
			StreamingTextures.RemoveSwap( Index );
			if ( Index != StreamingTextures.Num() )
			{
				FStreamingTexture& SwappedTexture = StreamingTextures( Index );
				// Note: The swapped texture may also be pending deletion.
				if ( SwappedTexture.Texture )
				{
					SwappedTexture.Texture->StreamingIndex = Index;
				}
			}
			--Index;
			--EndIndex;
			continue;
		}

		StreamingTexture.UpdateCachedInfo();

		if ( StreamingTexture.bReadyForStreaming )
		{
			UpdateTextureStatus( StreamingTexture, Context );
		}
	}
	CurrentUpdateStreamingTextureIndex = EndIndex;
}

#if STATS
IMPLEMENT_COMPARE_CONSTREF( FTextureStreamingStats, UnContentStreaming, { return B.WantedSize - A.WantedSize; } )
#endif

/**
 * Stream textures in/out, based on the priorities calculated by the async work.
 * @param bProcessEverything	Whether we're processing all textures in one go
 */
void FStreamingManagerTexture::StreamTextures( UBOOL bProcessEverything )
{
	// Setup a context for this run.
	FStreamingContext Context( bProcessEverything, IndividualStreamingTexture, bCollectTextureStats );

	const TArray<FTexturePriority>& PrioritizedTextures = AsyncWork->GetTask().GetPrioritizedTextures();
	FAsyncTextureStreaming::FThreadStats ThreadStats = AsyncWork->GetTask().GetStats();
	Context.AddStats( AsyncWork->GetTask().GetContext() );

#if STATS
	// Did we collect texture stats? Triggered by the ListStreamingTextures exec command.
	if ( Context.TextureStats.Num() > 0 )
	{
		// Reinitialize each time
		TextureStatsReport.Empty();

		// Sort textures by cost.
		Sort<USE_COMPARE_CONSTREF(FTextureStreamingStats,UnContentStreaming)>(Context.TextureStats.GetTypedData(),Context.TextureStats.Num());
		INT TotalCurrentSize	= 0;
		INT TotalWantedSize		= 0;
		INT TotalMaxSize		= 0;
		TextureStatsReport.AddItem( FString( TEXT(",Current,Wanted,Max,Largest Resident,Current Size (KB),Wanted Size (KB),Max Size (KB),Largest Resident Size (KB),Streaming Type,Last Rendered,BoostFactor,Name") ) );
		for( INT TextureIndex=0; TextureIndex<Context.TextureStats.Num(); TextureIndex++ )
		{
			const FTextureStreamingStats& TextureStat = Context.TextureStats(TextureIndex);
			INT LODBias				= TextureStat.LODBias;
			INT CurrDroppedMips		= TextureStat.NumMips - TextureStat.ResidentMips;
			INT WantedDroppedMips	= TextureStat.NumMips - TextureStat.WantedMips;
			INT MostDroppedMips		= TextureStat.NumMips - TextureStat.MostResidentMips;
			TextureStatsReport.AddItem( FString::Printf( TEXT(",%ix%i,%ix%i,%ix%i,%ix%i,%i,%i,%i,%i,%s,%3f sec,%.1f,%s"),
				TextureStat.SizeX >> CurrDroppedMips,
				TextureStat.SizeY >> CurrDroppedMips,
				TextureStat.SizeX >> WantedDroppedMips,
				TextureStat.SizeY >> WantedDroppedMips,
				TextureStat.SizeX >> LODBias,
				TextureStat.SizeY >> LODBias,
				TextureStat.SizeX >> MostDroppedMips,
				TextureStat.SizeY >> MostDroppedMips,
				TextureStat.ResidentSize/1024,
				TextureStat.WantedSize/1024,
				TextureStat.MaxSize/1024,
				TextureStat.MostResidentSize/1024,
				GStreamTypeNames[TextureStat.StreamType],
				TextureStat.LastRenderTime,
				TextureStat.BoostFactor,
				*TextureStat.TextureName
				) );
			TotalCurrentSize	+= TextureStat.ResidentSize;
			TotalWantedSize		+= TextureStat.WantedSize;
			TotalMaxSize		+= TextureStat.MaxSize;
		}
		TextureStatsReport.AddItem( FString::Printf( TEXT("Total size: Current= %d  Wanted=%d  Max= %d"), TotalCurrentSize/1024, TotalWantedSize/1024, TotalMaxSize/1024 ) );
		Context.TextureStats.Empty();

		if( bReportTextureStats )
		{
			if( TextureStatsReport.Num() > 0 )
			{
				debugf( TEXT("Listing collected stats for all streaming textures") );
				for( INT ReportIndex = 0; ReportIndex < TextureStatsReport.Num(); ReportIndex++ )
				{
					debugf(*(TextureStatsReport(ReportIndex)));
				}
				TextureStatsReport.Empty();
			}
			bReportTextureStats = FALSE;
		}
		bCollectTextureStats = FALSE;
	}
#endif

	INT TotalMemoryUsed = 0;
	INT TotalMemoryFree = 0;
	INT PendingMemoryAdjustment = 0;
	UBOOL bLimitedPoolSize = RHIGetTextureMemoryStats( TotalMemoryUsed, TotalMemoryFree, PendingMemoryAdjustment );
	INT TexturePoolSize = TotalMemoryUsed + TotalMemoryFree;
	ThreadStats.PendingStreamInSize += (PendingMemoryAdjustment > 0) ? PendingMemoryAdjustment : 0;
	ThreadStats.PendingStreamOutSize += (PendingMemoryAdjustment < 0) ? -PendingMemoryAdjustment : 0;

	INT AvailableNow, AvailableLater, TempMemoryUsed;
	if ( bLimitedPoolSize )
	{
		ThreadStats.TempStreamingSize = GStreamMemoryTracker.CalcTempMemory();
		TempMemoryUsed = ThreadStats.TempStreamingSize;

		// Note: 'AvailableNow' and 'AvailableLater' are allowed to be negative.
		AvailableNow = GStreamMemoryTracker.CalcAvailableNow( TotalMemoryFree, MemoryMargin );
		AvailableLater = GStreamMemoryTracker.CalcAvailableLater( TotalMemoryFree, MemoryMargin );

		STAT_FAST( INT NonStreamingUsage = TotalMemoryUsed - ThreadStats.TotalResidentSize - ThreadStats.PendingStreamInSize - TempMemoryUsed );
		STAT_FAST( INT MemoryBudget = OriginalTexturePoolSize - NonStreamingUsage - MemoryMargin );
		STAT_FAST( INT MemoryOverBudget = ThreadStats.TotalRequiredSize - MemoryBudget );
		SET_DWORD_STAT_FAST( STAT_StreamingOverBudget,	Max(MemoryOverBudget,0)	);
		SET_DWORD_STAT_FAST( STAT_StreamingUnderBudget,	Max(-MemoryOverBudget,0) );
	}
	else
	{
		TempMemoryUsed = ThreadStats.TempStreamingSize;
		AvailableNow = MAXINT;
		AvailableLater = MAXINT;
		SET_DWORD_STAT_FAST( STAT_StreamingOverBudget, 0 );
		SET_DWORD_STAT_FAST( STAT_StreamingUnderBudget, 0 );
	}
	NumWantingResources = ThreadStats.NumWantingTextures;
	NumWantingResourcesCounter++;
	SET_DWORD_STAT_FAST( STAT_NumWantingTextures, ThreadStats.NumWantingTextures );

	if ( bLimitedPoolSize && !bPauseTextureStreaming )
	{
		// Stream in high-priority textures if possible, and stream out as many low-priority textures as we need.
		// Keep track of how much temp memory we use separately, not letting it go over a certain threshold.
		INT HighPrioIndex = 0;
		INT LowPrioIndex = PrioritizedTextures.Num() - 1;

		FMemMark Mark(GMainThreadMemStack);
		FStreamingRequests StreamingRequests;
		INT LowPrioIndex_UnwantedMips = LowPrioIndex;

		while ( HighPrioIndex <= LowPrioIndex && TempMemoryUsed < MaxTempMemoryUsed )
		{
			const FTexturePriority& TexturePriority = PrioritizedTextures( HighPrioIndex );
			FStreamingTexture& HighPrioTexture = StreamingTextures( TexturePriority.Value );
			UBOOL bStreamInTexture = FALSE;

			// Check that texture hasn't been marked for removal.
			if ( HighPrioTexture.Texture != NULL )
			{
				// Do we want to cancel unload? Do we want more mips than requested?
				if ( HighPrioTexture.bInFlight && HighPrioTexture.RequestedMips < HighPrioTexture.ResidentMips && HighPrioTexture.RequestedMips < HighPrioTexture.WantedMips )
				{
					// Should we cancel?
					INT RequestedSize = HighPrioTexture.GetSize( HighPrioTexture.RequestedMips );
					INT StreamSize = HighPrioTexture.GetSize( HighPrioTexture.ResidentMips ) - RequestedSize;
					if ( StreamSize > AvailableLater )
					{
						// Try to cancel.
						UBOOL bCancelled = CancelStreamingRequest( HighPrioTexture );
						if ( bCancelled )
						{
							AvailableLater -= StreamSize;
						}
					}
				}

				// Would we like to stream in this texture?
				if ( HighPrioTexture.bInFlight == FALSE && HighPrioTexture.WantedMips > HighPrioTexture.ResidentMips )
				{
					//@TODO: TempSize=0 when using in-place reallocation, but we're conservative here.
					INT TempSize = HighPrioTexture.GetSize( HighPrioTexture.ResidentMips );
					INT StreamSize = HighPrioTexture.GetSize( HighPrioTexture.WantedMips ) - TempSize;
					AvailableLater -= StreamSize;

					// Can we stream in this texture now?
					if ( StreamSize <= AvailableNow && TempMemoryUsed < MaxTempMemoryUsed )
					{
						AvailableNow -= StreamSize;
						TempMemoryUsed += TempSize;
						StartStreaming( HighPrioTexture, HighPrioTexture.WantedMips, Context, TRUE );
					}
				}
			}

			// Do we need to stream something out?
			if ( AvailableLater < 0 )
			{
				// Try streaming out the unwanted mips, from LowPrioIndex_UnwantedMips to 0.
				LowPrioIndex_UnwantedMips = StreamoutTextures( StreamOut_UnwantedMips, AvailableLater, TempMemoryUsed, LowPrioIndex_UnwantedMips, 0, LowPrioIndex, PrioritizedTextures, StreamingRequests );
			}

			// Do we STILL need to stream something out?
			if ( AvailableLater < 0 )
			{
				// Try streaming out all mips, from LowPrioIndex to HighPrioIndex.
				// Note: AvailableLater, TempMemoryUsed, LowPriorityIndex and StreamingRequests may be altered by this call.
				INT TriedIndex = StreamoutTextures( StreamOut_AllMips, AvailableLater, TempMemoryUsed, LowPrioIndex, HighPrioIndex, LowPrioIndex, PrioritizedTextures, StreamingRequests );
				if ( LowPrioIndex_UnwantedMips > TriedIndex )
				{
					LowPrioIndex_UnwantedMips = TriedIndex;
				}
			}
			HighPrioIndex++;
		}

		for ( INT RequestIndex=0; RequestIndex < StreamingRequests.Num(); ++RequestIndex )
		{
			INT StreamingTextureIndex = StreamingRequests( RequestIndex );
			FStreamingTexture& StreamingTexture = StreamingTextures( StreamingTextureIndex );
			StartStreaming( StreamingTexture, StreamingTexture.RequestedMips, Context, TRUE );
		}

		Mark.Pop();
	}
	else if ( !bPauseTextureStreaming )
	{
		// Simple streaming when no texture pool with limited memory size is used.
		StreamTexturesUnlimited( Context, PrioritizedTextures, TempMemoryUsed );
	}

	UpdateStreamingStats( Context, TRUE );

	STAT( StreamingTimes(ProcessingStage) += appSeconds() );
}

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
INT FStreamingManagerTexture::StreamoutTextures( FStreamoutLogic StreamoutLogic, INT& AvailableLater, INT& TempMemoryUsed, INT StartIndex, INT StopIndex, INT& LowPrioIndex, const TArray<FTexturePriority>& PrioritizedTextures, FStreamingRequests& StreamingRequests )
{
	// Only bump LowPrioIndex if we're starting from there.
	UBOOL bBumpLowPrioIndex = (StartIndex == LowPrioIndex);

	INT Index = StartIndex;
	for ( ; AvailableLater < 0 && Index > StopIndex && TempMemoryUsed < MaxTempMemoryUsed; --Index )
	{
		const FTexturePriority& TexturePriority = PrioritizedTextures( Index );
		FStreamingTexture& StreamingTexture = StreamingTextures( TexturePriority.Value );

		// Check that this texture hasn't been marked for removal, and that we can perform streaming actions on it.
		if ( StreamingTexture.Texture != NULL && StreamingTexture.bReadyForStreaming )
		{
			if ( StreamingTexture.bInFlight )
			{
				// Cancel load operation?
				UBOOL bIsLoadRequest = StreamingTexture.RequestedMips > StreamingTexture.ResidentMips;
				if ( bIsLoadRequest &&
					 (StreamoutLogic == StreamOut_AllMips ||
					 (StreamoutLogic == StreamOut_UnwantedMips && StreamingTexture.RequestedMips > StreamingTexture.WantedMips) ) )
				{
					// Try to cancel.
					UBOOL bCancelled = CancelStreamingRequest( StreamingTexture );
					if ( bCancelled )
					{
						INT CurrentSize = StreamingTexture.GetSize( StreamingTexture.ResidentMips );
						INT StreamSize = StreamingTexture.GetSize( StreamingTexture.RequestedMips ) - CurrentSize;
						AvailableLater += StreamSize;
					}
				}
			}
			else
			{
				INT WantedMips = ( StreamoutLogic == StreamOut_AllMips ) ?  StreamingTexture.MinAllowedMips : StreamingTexture.WantedMips;

				// Note: When first attempting streamout, RequestedMips == ResidentMips. After the first attempt, it will be what we attempted last.
				INT CurrentMips = StreamingTexture.RequestedMips;

				// Can we stream out this texture?
				if ( WantedMips < CurrentMips )
				{
					//@TODO: Temp memory used=0 when using in-place reallocation. Though this is conservative and should be fine.
					INT CurrentSize = StreamingTexture.GetSize( CurrentMips );
					INT StreamSize = CurrentSize - StreamingTexture.GetSize( WantedMips );
					AvailableLater += StreamSize;
					TempMemoryUsed += CurrentSize;

					// If we haven't added it to the request array yet, do so now.
					if ( StreamingTexture.RequestedMips == StreamingTexture.ResidentMips )
					{
						StreamingRequests.AddItem( TexturePriority.Value );
					}
					StreamingTexture.RequestedMips = WantedMips;
				}

				// Can stream out more from this texture later, if we need to?
				if ( StreamingTexture.RequestedMips > StreamingTexture.MinAllowedMips )
				{
					bBumpLowPrioIndex = FALSE;
				}
			}
		}

		// Bump the low prio index when we can't do anything more with the lowest-priority textures.
		if ( bBumpLowPrioIndex )
		{
			LowPrioIndex--;
		}
	}

	return Index;
}


/**
 * Stream textures in/out, when no texture pool with limited size is used by the platform.
 *
 * @param Context				Context for the current stage
 * @param PrioritizedTextures	Array of prioritized textures to process
 * @param TempMemoryUsed		Current amount of temporary memory used by the streaming system, in bytes
 */
void FStreamingManagerTexture::StreamTexturesUnlimited( FStreamingContext& Context, const TArray<FTexturePriority>& PrioritizedTextures, INT TempMemoryUsed )
{
	for ( INT PrioIndex=0; PrioIndex < PrioritizedTextures.Num() && TempMemoryUsed < MaxTempMemoryUsed; ++PrioIndex )
	{
		const FTexturePriority& TexturePriority = PrioritizedTextures( PrioIndex );
		FStreamingTexture& StreamingTexture = StreamingTextures( TexturePriority.Value );

		// Has this texture been marked for removal?
		if ( StreamingTexture.Texture == NULL )
		{
			// Ignore it.
		}
		// Cancel load operation?
		else if ( StreamingTexture.bInFlight && StreamingTexture.RequestedMips > StreamingTexture.ResidentMips && StreamingTexture.RequestedMips > StreamingTexture.WantedMips )
		{
			if (!GNeverStreamOutTextures)
			{
				CancelStreamingRequest( StreamingTexture );
			}
		}
		// Cancel unload operation?
		else if ( StreamingTexture.bInFlight && StreamingTexture.RequestedMips < StreamingTexture.ResidentMips && StreamingTexture.RequestedMips < StreamingTexture.WantedMips )
		{
			CancelStreamingRequest( StreamingTexture );
		}
		// Stream this texture?
		else if ( StreamingTexture.bInFlight == FALSE && StreamingTexture.ResidentMips != StreamingTexture.WantedMips )
		{
			if (!GNeverStreamOutTextures || StreamingTexture.ResidentMips < StreamingTexture.WantedMips)
			{
				//@TODO: Temp memory used=0 when using in-place reallocation.
				INT CurrentSize = StreamingTexture.GetSize( StreamingTexture.ResidentMips );
				TempMemoryUsed += CurrentSize;
				StartStreaming( StreamingTexture, StreamingTexture.WantedMips, Context, TRUE );
			}
		}
	}
}

/**
 * New texture streaming system, based on texture priorities and asynchronous processing.
 * Updates streaming, taking into account all view infos.
 *
 * @param DeltaTime				Time since last call in seconds
 * @param bProcessEverything	[opt] If TRUE, process all resources with no throttling limits
 */
void FStreamingManagerTexture::NewUpdateResourceStreaming( FLOAT DeltaTime, UBOOL bProcessEverything/*=FALSE*/ )
{
#if STREAMING_LOG_VIEWCHANGES
	static UBOOL bWasLocationOveridden = FALSE;
	UBOOL bIsLocationOverriden = FALSE;
	for ( INT ViewIndex=0; ViewIndex < CurrentViewInfos.Num(); ++ViewIndex )
	{
		FStreamingViewInfo& ViewInfo = CurrentViewInfos( ViewIndex );
		if ( ViewInfo.bOverrideLocation )
		{
			bIsLocationOverriden = TRUE;
			break;
		}
	}
	if ( bIsLocationOverriden != bWasLocationOveridden )
	{
		debugf(TEXT("Texture streaming view location is now %s."), bIsLocationOverriden ? TEXT("OVERRIDEN") : TEXT("normal") );
		bWasLocationOveridden = bIsLocationOverriden;
	}
#endif

	INT OldNumTextureProcessingStages = NumTextureProcessingStages;
	if ( bProcessEverything || IndividualStreamingTexture )
	{
		AsyncWork->EnsureCompletion();
		ProcessingStage = 0;
		NumTextureProcessingStages = 1;
		RemainingTicksToDisregardWorldTextures = 0;
	}

	// Turn off the fudge factor so we get the raw output from GetWantedMips().
	FudgeFactor = 1.0f;
	// No limit on small mip increases.
	bUseMinRequestLimit = FALSE;

#if STATS
	if ( ProcessingStage == 0 )
	{
		STAT( StreamingTimes.Empty( NumTextureProcessingStages ) );
		STAT( StreamingTimes.AddZeroed( NumTextureProcessingStages ) );
	}
	STAT( StreamingTimes(ProcessingStage) -= appSeconds() );
#endif

	// Init.
	if ( ProcessingStage == 0 )
	{
		ResetStreamingStats();
		UpdateThreadData();

		//@TODO: Move this to the worker thread once it's thread-safe.
		//Note: Must be done with updated primitives (right after UpdateThreadData), because we can't ignore a primitive just because it moved.
		CalcDynamicWantedMips();

		if ( bTriggerDumpTextureGroupStats )
		{
			DumpTextureGroupStats( bDetailedDumpTextureGroupStats );
		}
		if ( bTriggerInvestigateTexture )
		{
			InvestigateTexture( InvestigateTextureName );
		}
	}

	// Non-threaded data collection.
	INT NumDataCollectionStages = Max( NumTextureProcessingStages - 1, 1 );
	if ( ProcessingStage < NumDataCollectionStages )
	{
		// Setup a context for this run. Note that we only (potentially) collect texture stats from the AsyncWork.
		FStreamingContext Context( bProcessEverything, IndividualStreamingTexture, FALSE );
		UpdateStreamingTextures( Context, ProcessingStage, NumDataCollectionStages );
		UpdateStreamingStats( Context, FALSE );
	}

	// Start async task after the last data collection stage (if we're not paused).
	if ( ProcessingStage == NumDataCollectionStages - 1 && bPauseTextureStreaming == FALSE )
	{
		//@FIXME: temp hack to work around async work crash
		while (!AsyncWork->IsDone())
		{
			appSleep(0);
		}

		// Is the AsyncWork is running for some reason? (E.g. we reset the system by simply setting ProcessingStage to 0.)
		if ( AsyncWork->IsDone() == FALSE )
		{
			// Wait until it's done and ignore its results.
			AsyncWork->GetTask().Abort();
			AsyncWork->EnsureCompletion();
		}

		AsyncWork->GetTask().Reset(bCollectTextureStats);
		if ( NumTextureProcessingStages > 1 )
		{
			AsyncWork->StartBackgroundTask();
		}
		else
		{
			// Perform the work synchronously on this thread.
			AsyncWork->StartSynchronousTask();
		}
	}

	// Are we still in the non-threaded data collection stages?
	if ( ProcessingStage < NumTextureProcessingStages - 1 )
	{
		STAT( StreamingTimes(ProcessingStage) += appSeconds() );
		ProcessingStage++;
	}
	// Are we waiting for the async work to finish?
	else if ( AsyncWork->IsDone() == FALSE )
	{
		STAT( StreamingTimes(ProcessingStage) += appSeconds() );
	}
	else
	{
		// All priorities have been calculated, do all streaming.
		StreamTextures( bProcessEverything );
		ProcessingStage = 0;
	}

	NumTextureProcessingStages = OldNumTextureProcessingStages;
	RemainingTicksToDisregardWorldTextures = 0;
	SET_FLOAT_STAT( STAT_DynamicStreamingTotal, FLOAT(GStreamingDynamicPrimitivesTime) );
}

/**
 * Blocks till all pending requests are fulfilled.
 *
 * @param TimeLimit		Optional time limit for processing, in seconds. Specifying 0 means infinite time limit.
 * @param bLogResults	Whether to dump the results to the log.
 * @return				Number of streaming requests still in flight, if the time limit was reached before they were finished.
 */
INT FStreamingManagerTexture::BlockTillAllRequestsFinished( FLOAT TimeLimit /*= 0.0f*/, UBOOL bLogResults /*= FALSE*/ )
{
	DOUBLE StartTime = appSeconds();
	FLOAT ElapsedTime = 0.0f;

	FMemMark Mark(GMainThreadMemStack);

	INT NumPendingUpdates = 0;
	INT MaxPendingUpdates = 0;

	// Add all textures to the initial pending array.
	TArray<INT, TMemStackAllocator<GMainThreadMemStack> > PendingTextures[2];
	PendingTextures[0].Empty( StreamingTextures.Num() );
	for ( INT TextureIndex=0; TextureIndex < StreamingTextures.Num(); ++TextureIndex )
	{
		PendingTextures[0].AddItem( TextureIndex );
	}

	INT CurrentArray = 0;
	do 
	{
		// Flush rendering commands.
		FlushRenderingCommands();

		// Update the textures in the current pending array, and add the ones that are still pending to the other array.
		PendingTextures[1 - CurrentArray].Empty( StreamingTextures.Num() );
		for ( INT Index=0; Index < PendingTextures[CurrentArray].Num(); ++Index )
		{
			INT TextureIndex = PendingTextures[CurrentArray]( Index );
			FStreamingTexture& StreamingTexture = StreamingTextures( TextureIndex );

			// Make sure this streaming texture hasn't been marked for removal.
			if ( StreamingTexture.Texture )
			{
				if ( StreamingTexture.Texture->UpdateStreamingStatus() )
				{
					PendingTextures[ 1 - CurrentArray ].AddItem( TextureIndex );
				}
				TrackTextureEvent( &StreamingTexture, StreamingTexture.Texture, FALSE, ENABLE_TEXTURE_LOGGING, StreamingTexture.bForceFullyLoad );
			}
		}

		// Swap arrays.
		CurrentArray = 1 - CurrentArray;

		NumPendingUpdates = PendingTextures[ CurrentArray ].Num();
		MaxPendingUpdates = Max( MaxPendingUpdates, NumPendingUpdates );

		// Check for time limit.
		ElapsedTime = FLOAT(appSeconds() - StartTime);
		if ( TimeLimit > 0.0f && ElapsedTime > TimeLimit )
		{
			break;
		}

		if ( NumPendingUpdates )
		{
			appSleep( 0.010f );
		}
	} while ( NumPendingUpdates );

	Mark.Pop();

	if ( bLogResults )
	{
		debugf( NAME_DevStreaming, TEXT("Blocking on texture streaming: %.1f ms (%d textures updated, %d still pending)"), ElapsedTime*1000.0f, MaxPendingUpdates, NumPendingUpdates );
#if STREAMING_LOG_VIEWCHANGES
		for ( INT ViewIndex=0; ViewIndex < CurrentViewInfos.Num(); ++ViewIndex )
		{
			FStreamingViewInfo& ViewInfo = CurrentViewInfos( ViewIndex );
			if ( ViewInfo.bOverrideLocation )
			{
				debugf( NAME_DevStreaming, TEXT("Texture streaming view: X=%1.f, Y=%.1f, Z=%.1f (Override=%d, Boost=%.1f)"), ViewInfo.ViewOrigin.X, ViewInfo.ViewOrigin.Y, ViewInfo.ViewOrigin.Z, ViewInfo.bOverrideLocation, ViewInfo.BoostFactor );
				break;
			}
		}
#endif
	}
	return NumPendingUpdates;
}

/**
 * Adds a textures streaming handler to the array of handlers used to determine which
 * miplevels need to be streamed in/ out.
 *
 * @param TextureStreamingHandler	Handler to add
 */
void FStreamingManagerTexture::AddTextureStreamingHandler( FStreamingHandlerTextureBase* TextureStreamingHandler )
{
	AsyncWork->EnsureCompletion();
	TextureStreamingHandlers.AddItem( TextureStreamingHandler );
}

/**
 * Removes a textures streaming handler from the array of handlers used to determine which
 * miplevels need to be streamed in/ out.
 *
 * @param TextureStreamingHandler	Handler to remove
 */
void FStreamingManagerTexture::RemoveTextureStreamingHandler( FStreamingHandlerTextureBase* TextureStreamingHandler )
{
	AsyncWork->EnsureCompletion();
	TextureStreamingHandlers.RemoveItem( TextureStreamingHandler );
}

/**
 * Calculates the minimum and maximum number of mip-levels for a streaming texture.
 */
void FStreamingManagerTexture::CalcMinMaxMips( FStreamingTexture& StreamingTexture )
{
	INT TextureLODBias = StreamingTexture.TextureLODBias;

	// Figure out whether texture should be forced resident based on bools and forced resident time.
	if( StreamingTexture.bForceFullyLoad )
	{
		// If the texture has cinematic high-res mip-levels, allow them to be streamed in.
		TextureLODBias = Max( TextureLODBias - StreamingTexture.NumCinematicMipLevels, 0 );
	}

	// We only stream in skybox textures as you are always within the bounding box and those textures
	// tend to be huge. A streaming fudge factor fluctuating around 1 will cause them to be streamed in
	// and out, making it likely for the allocator to fragment.
	if( StreamingTexture.LODGroup == TEXTUREGROUP_Skybox )
	{
		StreamingTexture.bForceFullyLoad = TRUE;
	}

	// Don't stream in all referenced textures but rather only those that have been rendered in the last 5 minutes if
	// we only stream in textures. This means you still might see texture popping, but the option is designed to avoid
	// hitching due to CPU overhead, which is still taken care off by the 5 minute rule.
	if( GSystemSettings.bOnlyStreamInTextures )
	{
		FLOAT SecondsSinceLastRender = StreamingTexture.LastRenderTime;
		if( SecondsSinceLastRender < 300 )
		{
			StreamingTexture.bForceFullyLoad = TRUE;
		}
	}

	// Calculate the minimum number of mip-levels required.
	StreamingTexture.MinAllowedMips = Min( StreamingTexture.MipCount - TextureLODBias, GMinTextureResidentMipCount );
	StreamingTexture.MinAllowedMips	= Max( StreamingTexture.MinAllowedMips, StreamingTexture.NumMipTailLevels );

	// Calculate the maximum number of mip-levels.
	INT MaxTextureMipCount = GMaxTextureMipCount;
	if ( GIsOperatingWithReducedTexturePool )
	{				
		MaxTextureMipCount = Max( GMaxTextureMipCount - 2, 0 );
	}
	StreamingTexture.MaxAllowedMips = Max( StreamingTexture.MipCount - TextureLODBias, StreamingTexture.MinAllowedMips );
	STAT_FAST( StreamingTexture.MaxAllowedOptimalMips = Min( StreamingTexture.MaxAllowedMips, GMaxTextureMipCount ) );
	StreamingTexture.MaxAllowedMips = Min( StreamingTexture.MaxAllowedMips, MaxTextureMipCount );

	// Should this texture be fully streamed in?
	if ( StreamingTexture.bForceFullyLoad )
	{
		StreamingTexture.MinAllowedMips = StreamingTexture.MaxAllowedMips;
	}
	// Check if the texture LOD group restricts the number of streaming mip-levels (in absolute terms).
	else if ( ThreadSettings.NumStreamedMips[StreamingTexture.LODGroup] >= 0 )
	{
		StreamingTexture.MinAllowedMips = Clamp( StreamingTexture.MipCount - ThreadSettings.NumStreamedMips[StreamingTexture.LODGroup], StreamingTexture.MinAllowedMips, StreamingTexture.MaxAllowedMips );
	}

	check( StreamingTexture.MinAllowedMips > 0 && StreamingTexture.MinAllowedMips <= StreamingTexture.MipCount );
	check( StreamingTexture.MaxAllowedMips >= StreamingTexture.MinAllowedMips && StreamingTexture.MaxAllowedMips <= StreamingTexture.MipCount );
}

/**
 * Updates this frame's STATs by one texture.
 */
void FStreamingManagerTexture::UpdateFrameStats( FStreamingTexture& StreamingTexture, FStreamingContext& Context )
{
#if STATS_FAST || STATS
	STAT_FAST(INT ResidentSize = StreamingTexture.GetSize(StreamingTexture.ResidentMips));
	STAT_FAST(Context.ThisFrameTotalStreamingTexturesSize += ResidentSize);
	STAT_FAST(INT PerfectWantedMips = Clamp(StreamingTexture.PerfectWantedMips, StreamingTexture.MinAllowedMips, StreamingTexture.MaxAllowedOptimalMips) );
	STAT_FAST(INT PerfectWantedSize = StreamingTexture.GetSize( PerfectWantedMips ));
	STAT_FAST(INT MostResidentSize = StreamingTexture.GetSize( StreamingTexture.MostResidentMips ) );
	STAT_FAST(Context.ThisFrameOptimalWantedSize += PerfectWantedSize );
#endif

#if STATS
	INT PotentialSize = StreamingTexture.GetSize(StreamingTexture.MaxAllowedMips);
	ETextureStreamingType StreamType = StreamingTexture.GetStreamingType();
	switch ( StreamType )
	{
		case StreamType_Forced:
			Context.ThisFrameTotalForcedHeuristicSize += PerfectWantedSize;
			break;
		case StreamType_LastRenderTime:
			Context.ThisFrameTotalLastRenderHeuristicSize += PerfectWantedSize;
			break;
		case StreamType_Dynamic:
			Context.ThisFrameTotalDynamicTextureHeuristicSize += PerfectWantedSize;
			break;
		case StreamType_Static:
			Context.ThisFrameTotalStaticTextureHeuristicSize += PerfectWantedSize;
			break;
	}
	if ( Context.bCollectTextureStats )
	{
		FString TextureName = StreamingTexture.Texture->GetFullName();
		if ( CollectTextureStatsName.Len() == 0 || TextureName.InStr( CollectTextureStatsName, FALSE, TRUE ) >= 0 )
		{
			new (Context.TextureStats) FTextureStreamingStats( StreamingTexture.Texture, StreamType, StreamingTexture.ResidentMips, PerfectWantedMips, StreamingTexture.MostResidentMips, ResidentSize, PerfectWantedSize, PotentialSize, MostResidentSize, StreamingTexture.BoostFactor );
		}
	}
	Context.ThisFrameNumStreamingTextures++;
	Context.ThisFrameTotalStreamingTexturesMaxSize += PotentialSize;
	Context.ThisFrameTotalLightmapMemorySize += StreamingTexture.bIsStreamingLightmap ? ResidentSize : 0;
	Context.ThisFrameTotalLightmapDiskSize += StreamingTexture.bIsStreamingLightmap ? PotentialSize : 0;
#endif
}

/**
 * Calculates the number of mip-levels we would like to have in memory for a texture.
 */
void FStreamingManagerTexture::CalcWantedMips( FStreamingTexture& StreamingTexture )
{
	INT WantedMips = StreamingTexture.bUsesDynamicHeuristics ? ScreenTexelsToMipLevel( StreamingTexture.DynamicScreenSize ) : INDEX_NONE;
	FLOAT MinDistance = StreamingTexture.bUsesDynamicHeuristics ? appSqrt(StreamingTexture.DynamicMinDistanceSq) : FLT_MAX;
	STAT_FAST(INT PerfectWantedMips = WantedMips);

	// Figure out miplevels to request based on handlers and whether streaming is enabled.
	if ( StreamingTexture.MinAllowedMips != StreamingTexture.MaxAllowedMips && GUseTextureStreaming )
	{
		// Iterate over all handlers and figure out the maximum requested number of mips.
		for( INT HandlerIndex=0; HandlerIndex<TextureStreamingHandlers.Num(); HandlerIndex++ )
		{
			FStreamingHandlerTextureBase* TextureStreamingHandler = TextureStreamingHandlers(HandlerIndex);
			FLOAT HandlerDistance = FLT_MAX;
			INT HandlerPerfectWantedMips = INDEX_NONE;
			INT HandlerWantedMips = TextureStreamingHandler->GetWantedMips( *this, StreamingTexture, HandlerPerfectWantedMips, HandlerDistance );
			WantedMips = Max( WantedMips, HandlerWantedMips );
			STAT_FAST( PerfectWantedMips = Max( PerfectWantedMips, HandlerPerfectWantedMips ) );
			MinDistance = Min( MinDistance, HandlerDistance );
		}

		UBOOL bShouldAlsoUseLastRenderTime = FALSE;
		if ( StreamingTexture.LastRenderTimeRefCount > 0 || (GCurrentTime - StreamingTexture.LastRenderTimeRefCountTimestamp) < 91.0 )
		{
			bShouldAlsoUseLastRenderTime = TRUE;

#ifdef _DEBUG
			//@DEBUG: To be able to set a break-point here.
			if ( WantedMips != INDEX_NONE )
			{
				INT Q=0;
			}
#endif
		}

		// Not handled by any handler, use fallback handlers.
		if ( WantedMips == INDEX_NONE || bShouldAlsoUseLastRenderTime )
		{
			// Try the handler for textures that has recently lost instance locations.
			FLOAT HandlerDistance = FLT_MAX;
			INT HandlerWantedMips = GetWantedMipsForOrphanedTexture( StreamingTexture, HandlerDistance );
			WantedMips = Max( WantedMips, HandlerWantedMips );
			MinDistance = Min( MinDistance, HandlerDistance );
			STAT_FAST( PerfectWantedMips = Max( PerfectWantedMips, HandlerWantedMips ) );

			// Still wasn't handled? Use the LastRenderTime handler as last resort.
			if ( WantedMips == INDEX_NONE || bShouldAlsoUseLastRenderTime )
			{
				// Fallback handler used if texture is not handled by any other handlers. Guaranteed to handle texture and not return INDEX_NONE.
				FStreamingHandlerTextureLastRender FallbackStreamingHandler;
				FLOAT HandlerDistance = FLT_MAX;
				INT HandlerPerfectWantedMips = INDEX_NONE;
				INT HandlerWantedMips = FallbackStreamingHandler.GetWantedMips( *this, StreamingTexture, HandlerPerfectWantedMips, HandlerDistance );
				WantedMips = Max( WantedMips, HandlerWantedMips );
				MinDistance = Min( MinDistance, HandlerDistance );
				STAT_FAST( PerfectWantedMips = Max( PerfectWantedMips, HandlerPerfectWantedMips ) );
			}
		}
	}
	// Stream in all allowed miplevels if requested.
	else
	{
		STAT_FAST( PerfectWantedMips = StreamingTexture.MaxAllowedOptimalMips );
		WantedMips = StreamingTexture.MaxAllowedMips;
	}

	StreamingTexture.WantedMips = Clamp( WantedMips, StreamingTexture.MinAllowedMips, StreamingTexture.MaxAllowedMips );
	StreamingTexture.MinDistance = MinDistance;
	STAT_FAST( StreamingTexture.PerfectWantedMips = Clamp( PerfectWantedMips, StreamingTexture.MinAllowedMips, StreamingTexture.MaxAllowedOptimalMips ) );
}

/**
 * Fallback handler to catch textures that have been orphaned recently.
 * This handler prevents massive spike in texture memory usage.
 * Orphaned textures were previously streamed based on distance but those instance locations have been removed -
 * for instance because a ULevel is being unloaded. These textures are still active and in memory until they are garbage collected,
 * so we must ensure that they do not start using the LastRenderTime fallback handler and suddenly stream in all their mips -
 * just to be destroyed shortly after by a garbage collect.
 */
INT FStreamingManagerTexture::GetWantedMipsForOrphanedTexture( FStreamingTexture& StreamingTexture, FLOAT& Distance )
{
	INT WantedMips = INDEX_NONE;

	// Did we recently remove instance locations for this texture?
	const FLOAT TimeSinceInstanceWasRemoved = FLOAT(GCurrentTime - StreamingTexture.InstanceRemovedTimestamp);

	// Was it less than 91 seconds ago?
	if ( TimeSinceInstanceWasRemoved < 91.0f )
	{
		const FLOAT TimeSinceTextureWasRendered = StreamingTexture.LastRenderTime;

		// Check if it hasn't been rendered since the instances were removed (with five second margin).
		if ( (TimeSinceTextureWasRendered - TimeSinceInstanceWasRemoved) > -5.0f )
		{
			// It may be garbage-collected soon. Stream out the highest mip, if currently loaded.
			WantedMips = Min( StreamingTexture.ResidentMips, StreamingTexture.MaxAllowedMips - 1 );
			Distance = 1000.0f;
			StreamingTexture.bUsesOrphanedHeuristics = TRUE;
		}
	}

	return WantedMips;
}

/**
 * Updates the I/O state of a texture (allowing it to progress to the next stage) and some stats.
 */
void FStreamingManagerTexture::UpdateTextureStatus( FStreamingTexture& StreamingTexture, FStreamingContext& Context )
{
	UTexture2D* Texture = StreamingTexture.Texture;

	// Update streaming status. A return value of FALSE means that we're done streaming this texture
	// so we can potentially request another change.
	StreamingTexture.bInFlight		= Texture->UpdateStreamingStatus( TRUE );
	StreamingTexture.ResidentMips	= Texture->ResidentMips;
	StreamingTexture.RequestedMips	= Texture->RequestedMips;
	INT		RequestStatus			= Texture->PendingMipChangeRequestStatus.GetValue();
	UBOOL	bHasCancelationPending	= Texture->bHasCancelationPending;

	if( bHasCancelationPending )
	{
		Context.ThisFrameNumRequestsInCancelationPhase++;
	}
	else if( RequestStatus >= TexState_ReadyFor_Finalization )
	{
		Context.ThisFrameNumRequestsInUpdatePhase++;
	}
	else if( RequestStatus == TexState_InProgress_Finalization )
	{
		Context.ThisFrameNumRequestsInFinalizePhase++;
	}

	// Request is in flight so there is an intermediate texture with RequestedMips miplevels.
	if( RequestStatus > 0 )
	{
		Context.ThisFrameTotalIntermediateTexturesSize += StreamingTexture.GetSize(StreamingTexture.RequestedMips);
		Context.ThisFrameNumIntermediateTextures++;
		// Update texture increase request stats.
		if( StreamingTexture.RequestedMips > StreamingTexture.ResidentMips )
		{
			Context.ThisFrameTotalMipCountIncreaseRequestsInFlight++;
		}
	}

	STAT( UpdateLatencyStats( Texture, StreamingTexture.WantedMips, StreamingTexture.bInFlight ) );

	if ( StreamingTexture.bInFlight == FALSE )
	{
		check( RequestStatus == TexState_ReadyFor_Requests );
	}
}

/**
 * Starts streaming in/out a texture.
 *
 * @param StreamingTexture			Texture to start to stream in/out
 * @param WantedMips				Number of mips we want in memory for this texture.
 * @param Context					Context for the current frame
 * @param bIgnoreMemoryConstraints	Whether to ignore memory constraints and always start streaming
 * @return							TRUE if the texture is now in flight
 */
UBOOL FStreamingManagerTexture::StartStreaming( FStreamingTexture& StreamingTexture, INT WantedMips, FStreamingContext& Context, UBOOL bIgnoreMemoryConstraints )
{
	UTexture2D* Texture = StreamingTexture.Texture;
	FTexture2DResource* Texture2DResource = (FTexture2DResource*) Texture->Resource;

	StreamingTexture.WantedMips = WantedMips;

	if ( StreamingTexture.WantedMips != StreamingTexture.ResidentMips && StreamingTexture.bReadyForStreaming )
	{
		UBOOL bCanRequestTextureIncrease = TRUE;

		UBOOL bIsLoadingRequest = StreamingTexture.WantedMips > StreamingTexture.ResidentMips;

		// If memory stats are supported we respect the memory limit to not stream in miplevels.
		if( ( Context.bRHISupportsMemoryStats && (Context.AvailableMemorySize <= MemoryStopIncreasingLimit) )
			// Don't request increase if requested mip count is below threshold.
			||	(bUseMinRequestLimit && StreamingTexture.WantedMips < MinRequestedMipsToConsider) )
		{
			bCanRequestTextureIncrease = bIgnoreMemoryConstraints;
		}

		// Only request change if it's a decrease or we can request an increase.
		if ( !bIsLoadingRequest || bCanRequestTextureIncrease )						
		{
			// Keep track of current allocations this frame and stop iteration if we've exceeded frame limit.
			if ( bIsLoadingRequest )
			{
				// Manually update size as allocations are deferred/ happening in rendering thread.
				INT CurrentRequestSize		= StreamingTexture.GetSize(StreamingTexture.WantedMips);
				INT StreamInSize			= CurrentRequestSize - StreamingTexture.GetSize(StreamingTexture.ResidentMips);
				Context.ThisFrameTotalRequestSize	+= StreamInSize;
				Context.ThisFrameTotalLightmapRequestSize += StreamingTexture.bIsStreamingLightmap ? StreamInSize : 0;
				Context.AvailableMemorySize	-= CurrentRequestSize;

				if( Context.ThisFrameTotalRequestSize > Context.MaxPerFrameRequestLimit )
				{
					// We've exceeded the request limit for this frame.
					Context.MaxTexturesToProcess = 0;
				}
			}

			if ( Texture->PendingMipChangeRequestStatus.GetValue() == TexState_ReadyFor_Requests &&
				 StreamingTexture.WantedMips != StreamingTexture.Texture->ResidentMips )
			{
				check(!Texture->bHasCancelationPending);
				// Set new requested mip count.
				Texture->RequestedMips = StreamingTexture.WantedMips;
				StreamingTexture.RequestedMips = StreamingTexture.WantedMips;
				// Enqueue command to update mip count.
				UBOOL bShouldPrioritizeAsyncIORequest = RemainingTicksToDisregardWorldTextures || StreamingTexture.bForceFullyLoad;
				Texture2DResource->BeginUpdateMipCount( bShouldPrioritizeAsyncIORequest );
				StreamingTexture.bInFlight = TRUE;
				TrackTextureEvent( &StreamingTexture, StreamingTexture.Texture, FALSE, ENABLE_TEXTURE_LOGGING, StreamingTexture.bForceFullyLoad );
			}
			else
			{
				// Did UpdateStreamingTextures() miss a texture? Should never happen!
				warnf(TEXT("BeginUpdateMipCount failure! PendingMipChangeRequestStatus == %d, Resident=%d, Requested=%d, Wanted=%d"), Texture->PendingMipChangeRequestStatus.GetValue(), Texture->ResidentMips, Texture->RequestedMips, StreamingTexture.WantedMips );
			}

			// Mark as unavailable for further streaming actions this frame.
			StreamingTexture.bReadyForStreaming = FALSE;
		}
	}

	return StreamingTexture.bInFlight;
}

/**
 * Conditionally cancels a current streaming request, if we've changed our mind on the number of mip-levels we want in memory.
 *
 * @param StreamingTexture		Texture to potentially cancel streaming for
 * @param Context				Streaming context
 * @return						TRUE if a streaming request was canceled
 */
UBOOL FStreamingManagerTexture::ConditionallyCancelTextureStreaming( FStreamingTexture& StreamingTexture, FStreamingContext& Context )
{
	UBOOL bCancelledStreaming = FALSE;
	UBOOL bCanCancel = StreamingTexture.bInFlight && !StreamingTexture.Texture->bHasCancelationPending;

	// Checked whether we should cancel the pending request if the new one is different.
	if( bCanCancel && (StreamingTexture.RequestedMips != StreamingTexture.WantedMips) )
	{
		// Cancel load if we want fewer mip-levels nowadays.
		if( (StreamingTexture.WantedMips < StreamingTexture.RequestedMips) && (StreamingTexture.WantedMips >= StreamingTexture.ResidentMips) )
		{
			bCancelledStreaming = CancelStreamingRequest( StreamingTexture );
		}
		// Cancel unload if want more mip-levels nowadays.
		else if( (StreamingTexture.WantedMips > StreamingTexture.RequestedMips) && (StreamingTexture.WantedMips <= StreamingTexture.ResidentMips) )
		{
			bCancelledStreaming = CancelStreamingRequest( StreamingTexture );
		}

		if( bCancelledStreaming )
		{
			Context.ThisFrameNumRequestsInCancelationPhase++;
		}
	}

	return bCancelledStreaming;
}

/**
 * Cancels the current streaming request for the specified texture.
 *
 * @param StreamingTexture		Texture to cancel streaming for
 * @return						TRUE if a streaming request was canceled
 */
UBOOL FStreamingManagerTexture::CancelStreamingRequest( FStreamingTexture& StreamingTexture )
{
	// Mark as unavailable for further streaming action this frame.
	StreamingTexture.bReadyForStreaming = FALSE;

	StreamingTexture.WantedMips = StreamingTexture.ResidentMips;
	return StreamingTexture.Texture->CancelPendingMipChangeRequest();
}

#if STATS
/**
 * Updates the streaming latency STAT for a texture.
 *
 * @param Texture		Texture to update for
 * @param WantedMips	Number of desired mip-levels for the texture
 * @param bInFlight		Whether the texture is currently being streamed
 */
void FStreamingManagerTexture::UpdateLatencyStats( UTexture2D* Texture, INT WantedMips, UBOOL bInFlight )
{
	// Is the texture currently not updating?
	if ( bInFlight == FALSE )
	{
		// Did we measure the latency time?
		if ( Texture->Timer > 0.0f )
		{
			DOUBLE TotalLatency = LatencyAverage*NumLatencySamples - LatencySamples[LatencySampleIndex] + Texture->Timer;
			LatencySamples[LatencySampleIndex] = Texture->Timer;
			LatencySampleIndex = (LatencySampleIndex + 1) % NUM_LATENCYSAMPLES;
			NumLatencySamples = ( NumLatencySamples == NUM_LATENCYSAMPLES ) ? NumLatencySamples : (NumLatencySamples+1);
			LatencyAverage = TotalLatency / NumLatencySamples;
			LatencyMaximum = Max(LatencyMaximum, Texture->Timer);
		}

		// Have we detected that the texture should stream in more mips?
		if ( WantedMips > Texture->ResidentMips )
		{
			// Set the start time. Make it negative so we can differentiate it with a measured time.
			// Add in the average (half) of the total time it takes to check all textures, as an estimation
			// of the latency we add by not processing all textures in one go.
			Texture->Timer = -FLOAT(appSeconds() - GStartTime + LastFullIterationTime*0.5f );
		}
		else
		{
			Texture->Timer = 0.0f;
		}
	}
}
#endif

/**
 * Allows the streaming manager to process exec commands.
 * @param Cmd	Exec command
 * @param Ar	Output device for feedback
 * @return		TRUE if the command was handled
 */
UBOOL FStreamingManagerTexture::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
#if STATS_FAST
	if (ParseCommand(&Cmd,TEXT("DumpTextureStreamingStats")))
	{
		Ar.Logf( TEXT("Current Texture Streaming Stats") );
		Ar.Logf( TEXT("  Textures In Memory, Current (KB) = %f"), MaxStreamingTexturesSize / 1024.0f);
		Ar.Logf( TEXT("  Textures In Memory, Target (KB) =  %f"), MaxOptimalTextureSize / 1024.0f );
		Ar.Logf( TEXT("  Over Budget (KB) =                 %f"), MaxStreamingOverBudget / 1024.0f );
		Ar.Logf( TEXT("  Pool Memory Used (KB) =            %f"), MaxTexturePoolAllocatedSize / 1024.0f );
		Ar.Logf( TEXT("  Largest free memory hole (KB) =    %f"), MinLargestHoleSize / 1024.0f );
		Ar.Logf( TEXT("  Num Wanting Textures =             %d"), MaxNumWantingTextures );
		MaxStreamingTexturesSize = 0;
		MaxOptimalTextureSize = 0;
		MaxStreamingOverBudget = MININT;
		MaxTexturePoolAllocatedSize = 0;
		MinLargestHoleSize = OriginalTexturePoolSize;
		MaxNumWantingTextures = 0;
		return TRUE;
	}
#endif
#if STATS
	if (ParseCommand(&Cmd,TEXT("ListStreamingTextures")))
	{
		// Collect and report stats
		CollectTextureStatsName = ParseToken(Cmd, 0);
		bCollectTextureStats = TRUE;
		bReportTextureStats = TRUE;
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("ListStreamingTexturesCollect")))
	{
		// Collect stats, but disable automatic reporting
		CollectTextureStatsName = ParseToken(Cmd, 0);
		bCollectTextureStats = TRUE;
		bReportTextureStats = FALSE;
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("ListStreamingTexturesReportReady")))
	{
		if( bCollectTextureStats )
		{
			if( TextureStatsReport.Num() > 0 )
			{
				return TRUE;
			}
			return FALSE;
		}
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("ListStreamingTexturesReport")))
	{
		// TextureStatsReport is assumed to have been populated already via ListStreamingTexturesCollect
		if( TextureStatsReport.Num() > 0 )
		{
			Ar.Logf( TEXT("Listing collected stats for all streaming textures") );
			for( INT ReportIndex = 0; ReportIndex < TextureStatsReport.Num(); ReportIndex++ )
			{
				Ar.Logf(*(TextureStatsReport(ReportIndex)));
			}
			TextureStatsReport.Empty();
		}
		else
		{
			Ar.Logf( TEXT("No stats have been collected for streaming textures. Use ListStreamingTexturesCollect or the -WAITFORTEXSTREAMING option to MemLeakCheck to do so.") );
		}
		return TRUE;
	}
#endif
#if !FINAL_RELEASE
	if (ParseCommand(&Cmd,TEXT("LightmapStreamingFactor")))
	{
		FString FactorString(ParseToken(Cmd, 0));
		FLOAT NewFactor = ( FactorString.Len() > 0 ) ? appAtof(*FactorString) : GLightmapStreamingFactor;
		if ( NewFactor >= 0.0f )
		{
			GLightmapStreamingFactor = NewFactor;
		}
		Ar.Logf( TEXT("Lightmap streaming factor: %.3f (lower values makes streaming more aggressive)."), GLightmapStreamingFactor );
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("CancelTextureStreaming")))
	{
		UTexture2D::CancelPendingTextureStreaming();
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("ShadowmapStreamingFactor")))
	{
		FString FactorString(ParseToken(Cmd, 0));
		FLOAT NewFactor = ( FactorString.Len() > 0 ) ? appAtof(*FactorString) : GShadowmapStreamingFactor;
		if ( NewFactor >= 0.0f )
		{
			GShadowmapStreamingFactor = NewFactor;
		}
		Ar.Logf( TEXT("Shadowmap streaming factor: %.3f (lower values makes streaming more aggressive)."), GShadowmapStreamingFactor );
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("NumStreamedMips")))
	{
		FString NumTextureString(ParseToken(Cmd, 0));
		FString NumMipsString(ParseToken(Cmd, 0));
		INT LODGroup = ( NumTextureString.Len() > 0 ) ? appAtoi(*NumTextureString) : MAXINT;
		INT NumMips = ( NumMipsString.Len() > 0 ) ? appAtoi(*NumMipsString) : MAXINT;
		if ( LODGroup >= 0 && LODGroup < TEXTUREGROUP_MAX )
		{
			FTextureLODSettings::FTextureLODGroup& TexGroup = GSystemSettings.TextureLODSettings.GetTextureLODGroup(LODGroup);
			if ( NumMips >= -1 && NumMips <= MAX_TEXTURE_MIP_COUNT )
			{
				TexGroup.NumStreamedMips = NumMips;
			}
			Ar.Logf( TEXT("%s.NumStreamedMips = %d"), UTexture::GetTextureGroupString(TextureGroup(LODGroup)), TexGroup.NumStreamedMips );
		}
		else
		{
			Ar.Logf( TEXT("Usage: NumStreamedMips TextureGroupIndex <N>") );
		}
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("TrackTexture")))
	{
		FString TextureName(ParseToken(Cmd, 0));
		if ( TrackTexture( TextureName ) )
		{
			Ar.Logf( TEXT("Textures containing \"%s\" are now tracked."), *TextureName );
		}
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("ListTrackedTextures")))
	{
		FString NumTextureString(ParseToken(Cmd, 0));
		INT NumTextures = ( NumTextureString.Len() > 0 ) ? appAtoi(*NumTextureString) : -1;
		ListTrackedTextures( Ar, NumTextures );
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("UntrackTexture")))
	{
		FString TextureName(ParseToken(Cmd, 0));
		if ( UntrackTexture( TextureName ) )
		{
			Ar.Logf( TEXT("Textures containing \"%s\" are no longer tracked."), *TextureName );
		}
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("StreamOut")))
	{
		FString Parameter(ParseToken(Cmd, 0));
		INT FreeMB = (Parameter.Len() > 0) ? appAtoi(*Parameter) : 0;
		if ( FreeMB > 0 )
		{
			UBOOL bSucceeded = GStreamingManager->StreamOutTextureData( FreeMB * 1024 * 1024 );
			Ar.Logf( TEXT("Tried to stream out %d MB of texture data: %s"), FreeMB, bSucceeded ? TEXT("Succeeded") : TEXT("Failed") );
		}
		else
		{
			Ar.Logf( TEXT("Usage: StreamOut <N> (in MB)") );
		}
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("PauseTextureStreaming")))
	{
		bPauseTextureStreaming = !bPauseTextureStreaming;
		Ar.Logf( TEXT("Texture streaming is now \"%s\"."), bPauseTextureStreaming ? TEXT("PAUSED") : TEXT("UNPAUSED") );
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("TogglePriorityStreaming")))
	{
		if ( bAllowSwitchingStreamingSystem )
		{
			bUsePriorityStreaming = !bUsePriorityStreaming;
			ProcessingStage = 0;
		}
		else
		{
			Ar.Logf( TEXT("Can't toggle texture streaming system because of the bAllowSwitchingStreamingSystem .ini setting.") );
		}
		Ar.Logf( TEXT("Texture streaming based on priority: %s"), bUsePriorityStreaming ? TEXT("Enabled") : TEXT("Disabled") );
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("StreamingManagerMemory")))
	{
		INT OldStreamingTextures = 0;
		INT NewStreamingTextures = StreamingTextures.Num() * sizeof( FStreamingTexture );
		for ( TObjectIterator<UTexture2D> It; It; ++It )
		{
			OldStreamingTextures += sizeof(TLinkedList<UTexture2D*>);
			NewStreamingTextures += sizeof(INT);
		}

		INT OldSystem = 0;
		INT OldSystemSlack = 0;
		for( INT LevelIndex=0; LevelIndex<GWorld->Levels.Num(); LevelIndex++ )
		{
			// Find instances of the texture in this level.
			ULevel* Level = GWorld->Levels(LevelIndex);
			for ( TMap<UTexture2D*,TArray<FStreamableTextureInstance> >::TConstIterator It(Level->TextureToInstancesMap); It; ++It )
			{
				const TArray<FStreamableTextureInstance>& TextureInstances = It.Value();
				OldSystem += TextureInstances.Num() * sizeof(FStreamableTextureInstance);
				OldSystemSlack += TextureInstances.GetSlack() * sizeof(FStreamableTextureInstance);
			}
		}

		INT NewSystem = 0;
		INT NewSystemSlack = 0;
		for ( INT LevelIndex=0; LevelIndex < ThreadSettings.LevelData.Num(); ++LevelIndex )
		{
			const FLevelData& LevelData = ThreadSettings.LevelData( LevelIndex );
			for ( TMap<const UTexture2D*,TArray<FStreamableTextureInstance4> >::TConstIterator It(LevelData.Value.ThreadTextureInstances); It; ++It )
			{
				const TArray<FStreamableTextureInstance4>& TextureInstances = It.Value();
				NewSystem += TextureInstances.Num() * sizeof(FStreamableTextureInstance4);
				NewSystemSlack += TextureInstances.GetSlack() * sizeof(FStreamableTextureInstance4);
			}
		}
		Ar.Logf( TEXT("Old: %.2f KB used, %.2f KB slack, %.2f KB texture info"), OldSystem/1024.0f, OldSystemSlack/1024.0f, OldStreamingTextures/1024.0f );
		Ar.Logf( TEXT("New: %.2f KB used, %.2f KB slack, %.2f KB texture info"), NewSystem/1024.0f, NewSystemSlack/1024.0f, NewStreamingTextures/1024.0f );

		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("TextureGroups")))
	{
		bDetailedDumpTextureGroupStats = ParseParam(Cmd, TEXT("Detailed"));
		bTriggerDumpTextureGroupStats = TRUE;
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("InvestigateTexture")))
	{
		FString TextureName(ParseToken(Cmd, 0));
		if ( TextureName.Len() )
		{
			bTriggerInvestigateTexture = TRUE;
			InvestigateTextureName = TextureName;
		}
		else
		{
			Ar.Logf( TEXT("Usage: InvestigateTexture <name>") );
		}
		return TRUE;
	}
#endif // !FINAL_RELEASE

	return FALSE;
}

void FStreamingManagerTexture::DumpTextureGroupStats( UBOOL bDetailedStats )
{
	bTriggerDumpTextureGroupStats = FALSE;
#if !FINAL_RELEASE
	struct FTextureGroupStats
	{
		FTextureGroupStats()
		{
			appMemzero( this, sizeof(FTextureGroupStats) );
		}
		INT NumTextures;
		INT NumNonStreamingTextures;
		INT CurrentTextureSize;
		INT WantedTextureSize;
		INT MaxTextureSize;
		INT NonStreamingSize;
	};
	FTextureGroupStats TextureGroupStats[TEXTUREGROUP_MAX];
	FTextureGroupStats TextureGroupWaste[TEXTUREGROUP_MAX];
	INT NumNonStreamingTextures = 0;
	INT NonStreamingSize = 0;
	INT NumNonStreamingPoolTextures = 0;
	INT NonStreamingPoolSize = 0;
	INT TotalSavings = 0;
//	INT UITexels = 0;
	INT NumDXT[PF_MAX];
	INT NumNonSaved[PF_MAX];
	INT NumOneMip[PF_MAX];
	INT NumBadAspect[PF_MAX];
	INT NumTooSmall[PF_MAX];
	INT NumNonPow2[PF_MAX];
	INT NumNULLResource[PF_MAX];
	appMemzero( &NumDXT, sizeof(NumDXT) );
	appMemzero( &NumNonSaved, sizeof(NumNonSaved) );
	appMemzero( &NumOneMip, sizeof(NumOneMip) );
	appMemzero( &NumBadAspect, sizeof(NumBadAspect) );
	appMemzero( &NumTooSmall, sizeof(NumTooSmall) );
	appMemzero( &NumNonPow2, sizeof(NumNonPow2) );
	appMemzero( &NumNULLResource, sizeof(NumNULLResource) );

	// Gather stats.
	for( TObjectIterator<UTexture> It; It; ++It )
	{
		UTexture* Texture = *It;
		FTextureGroupStats& Stat = TextureGroupStats[Texture->LODGroup];
		FTextureGroupStats& Waste = TextureGroupWaste[Texture->LODGroup];
		UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
		if ( Texture2D && Texture2D->StreamingIndex >= 0 )
		{
			FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
			Stat.NumTextures++;
			Stat.CurrentTextureSize += StreamingTexture.GetSize( StreamingTexture.ResidentMips );
			Stat.WantedTextureSize += StreamingTexture.GetSize( StreamingTexture.WantedMips );
			Stat.MaxTextureSize += StreamingTexture.GetSize( StreamingTexture.MaxAllowedMips );
			INT WasteCurrent = StreamingTexture.GetSize( StreamingTexture.ResidentMips ) - CalcTextureSize( Texture2D->SizeX, Texture2D->SizeY, (EPixelFormat)Texture2D->Format, StreamingTexture.ResidentMips );
			INT WasteWanted = StreamingTexture.GetSize( StreamingTexture.WantedMips ) - CalcTextureSize( Texture2D->SizeX, Texture2D->SizeY, (EPixelFormat)Texture2D->Format, StreamingTexture.WantedMips );
			INT WasteMaxSize = StreamingTexture.GetSize( StreamingTexture.MaxAllowedMips ) - CalcTextureSize( Texture2D->SizeX, Texture2D->SizeY, (EPixelFormat)Texture2D->Format, StreamingTexture.MaxAllowedMips );
			Waste.NumTextures++;
			Waste.CurrentTextureSize += Max(WasteCurrent,0);
			Waste.WantedTextureSize += Max(WasteWanted,0);
			Waste.MaxTextureSize += Max(WasteMaxSize,0);
		}
		else
		{
			UBOOL bIsPooledTexture = Texture->Resource && IsValidRef(Texture->Resource->TextureRHI) && appIsPoolTexture( Texture->Resource->TextureRHI );
			INT TextureSize = Texture->CalcTextureMemorySize(TMC_ResidentMips);
			Stat.NumNonStreamingTextures++;
			Stat.NonStreamingSize += TextureSize;
			if ( Texture2D && Texture2D->Resource )
			{
				INT WastedSize = TextureSize - CalcTextureSize( Texture2D->SizeX, Texture2D->SizeY, (EPixelFormat)Texture2D->Format, Texture2D->Mips.Num() );
				Waste.NumNonStreamingTextures++;
				Waste.NonStreamingSize += Max(WastedSize, 0);
			}
			if ( bIsPooledTexture )
			{
				NumNonStreamingPoolTextures++;
				NonStreamingPoolSize += TextureSize;
			}
			else
			{
				NumNonStreamingTextures++;
				NonStreamingSize += TextureSize;
			}
		}

// 		if ( Texture2D && Texture2D->Resource && Texture2D->LODGroup == TEXTUREGROUP_UI )
// 		{
// 			UITexels += Texture2D->SizeX * Texture2D->SizeY;
// 		}
// 
		if ( Texture2D && (Texture2D->Format == PF_DXT1 || Texture2D->Format == PF_DXT5) )
		{
			NumDXT[Texture2D->Format]++;
			if ( Texture2D->Resource )
			{
				UINT UnusedMipTailSize = XeCalcUnusedMipTailSize( Texture2D->SizeX, Texture2D->SizeY, (EPixelFormat)Texture2D->Format, Texture2D->Mips.Num(), (Texture2D->MipTailBaseIdx == -1) ? FALSE : TRUE );
				if ( UnusedMipTailSize > 0 )
				{
					TotalSavings += UnusedMipTailSize;
				}
				else
				{
					// Track the reasons we couldn't save any memory from the mip-tail.
					NumNonSaved[Texture2D->Format]++;
					if ( Texture2D->Mips.Num() < 2 )
					{
						NumOneMip[Texture2D->Format]++;
					}
					else if ( Texture2D->SizeX > Texture2D->SizeY * 2 || Texture2D->SizeY > Texture2D->SizeX * 2 )
					{
						NumBadAspect[Texture2D->Format]++;
					}
					else if ( Texture2D->SizeX < 16 || Texture2D->SizeY < 16 || Texture2D->Mips.Num() < 5 )
					{
						NumTooSmall[Texture2D->Format]++;
					}
					else if ( (Texture2D->SizeX & (Texture2D->SizeX - 1)) != 0 || (Texture2D->SizeY & (Texture2D->SizeY - 1)) != 0 )
					{
						NumNonPow2[Texture2D->Format]++;
					}
					else
					{
						// Unknown reason
						INT Q=0;
					}
				}
			}
			else
			{
				NumNULLResource[Texture2D->Format]++;
			}
		}
	}

	// Output stats.
	{
		debugf(TEXT("Texture memory usage:"));
		FTextureGroupStats TotalStats;
		for ( INT GroupIndex=0; GroupIndex < TEXTUREGROUP_MAX; ++GroupIndex )
		{
			FTextureGroupStats& Stat = TextureGroupStats[GroupIndex];
			TotalStats.NumTextures				+= Stat.NumTextures;
			TotalStats.NumNonStreamingTextures	+= Stat.NumNonStreamingTextures;
			TotalStats.CurrentTextureSize		+= Stat.CurrentTextureSize;
			TotalStats.WantedTextureSize		+= Stat.WantedTextureSize;
			TotalStats.MaxTextureSize			+= Stat.MaxTextureSize;
			TotalStats.NonStreamingSize			+= Stat.NonStreamingSize;
#if USE_LS_SPEC_FOR_UNICODE
			debugf( TEXT("%34ls: NumTextures=%4d, Current=%8.1f KB, Wanted=%8.1f KB, OnDisk=%8.1f KB, NumNonStreaming=%4d, NonStreaming=%8.1f KB"),
#else
			debugf( TEXT("%34s: NumTextures=%4d, Current=%8.1f KB, Wanted=%8.1f KB, OnDisk=%8.1f KB, NumNonStreaming=%4d, NonStreaming=%8.1f KB"),
#endif
				UTexture::GetTextureGroupString((TextureGroup)GroupIndex),
				Stat.NumTextures,
				Stat.CurrentTextureSize / 1024.0f,
				Stat.WantedTextureSize / 1024.0f,
				Stat.MaxTextureSize / 1024.0f,
				Stat.NumNonStreamingTextures,
				Stat.NonStreamingSize / 1024.0f );
		}
#if USE_LS_SPEC_FOR_UNICODE
		debugf( TEXT("%34ls: NumTextures=%4d, Current=%8.1f KB, Wanted=%8.1f KB, OnDisk=%8.1f KB, NumNonStreaming=%4d, NonStreaming=%8.1f KB"),
#else
		debugf( TEXT("%34s: NumTextures=%4d, Current=%8.1f KB, Wanted=%8.1f KB, OnDisk=%8.1f KB, NumNonStreaming=%4d, NonStreaming=%8.1f KB"),
#endif
			TEXT("Total"),
			TotalStats.NumTextures,
			TotalStats.CurrentTextureSize / 1024.0f,
			TotalStats.WantedTextureSize / 1024.0f,
			TotalStats.MaxTextureSize / 1024.0f,
			TotalStats.NumNonStreamingTextures,
			TotalStats.NonStreamingSize / 1024.0f );
	}
	if ( bDetailedStats )
	{
		debugf(TEXT("Wasted memory due to inefficient texture storage:"));
		FTextureGroupStats TotalStats;
		for ( INT GroupIndex=0; GroupIndex < TEXTUREGROUP_MAX; ++GroupIndex )
		{
			FTextureGroupStats& Stat = TextureGroupWaste[GroupIndex];
			TotalStats.NumTextures				+= Stat.NumTextures;
			TotalStats.NumNonStreamingTextures	+= Stat.NumNonStreamingTextures;
			TotalStats.CurrentTextureSize		+= Stat.CurrentTextureSize;
			TotalStats.WantedTextureSize		+= Stat.WantedTextureSize;
			TotalStats.MaxTextureSize			+= Stat.MaxTextureSize;
			TotalStats.NonStreamingSize			+= Stat.NonStreamingSize;
#if USE_LS_SPEC_FOR_UNICODE
			debugf( TEXT("%34ls: NumTextures=%4d, Current=%8.1f KB, Wanted=%8.1f KB, OnDisk=%8.1f KB, NumNonStreaming=%4d, NonStreaming=%8.1f KB"),
#else
			debugf( TEXT("%34s: NumTextures=%4d, Current=%8.1f KB, Wanted=%8.1f KB, OnDisk=%8.1f KB, NumNonStreaming=%4d, NonStreaming=%8.1f KB"),
#endif
				UTexture::GetTextureGroupString((TextureGroup)GroupIndex),
				Stat.NumTextures,
				Stat.CurrentTextureSize / 1024.0f,
				Stat.WantedTextureSize / 1024.0f,
				Stat.MaxTextureSize / 1024.0f,
				Stat.NumNonStreamingTextures,
				Stat.NonStreamingSize / 1024.0f );
		}
#if USE_LS_SPEC_FOR_UNICODE
		debugf( TEXT("%34ls: NumTextures=%4d, Current=%8.1f KB, Wanted=%8.1f KB, OnDisk=%8.1f KB, NumNonStreaming=%4d, NonStreaming=%8.1f KB"),
#else
		debugf( TEXT("%34s: NumTextures=%4d, Current=%8.1f KB, Wanted=%8.1f KB, OnDisk=%8.1f KB, NumNonStreaming=%4d, NonStreaming=%8.1f KB"),
#endif
			TEXT("Total Wasted"),
			TotalStats.NumTextures,
			TotalStats.CurrentTextureSize / 1024.0f,
			TotalStats.WantedTextureSize / 1024.0f,
			TotalStats.MaxTextureSize / 1024.0f,
			TotalStats.NumNonStreamingTextures,
			TotalStats.NonStreamingSize / 1024.0f );
	}

	//@TODO: Calculate memory usage for non-pool textures properly!
//	debugf( TEXT("%34s: NumTextures=%4d, Current=%7.1f KB"), TEXT("Non-streaming pool textures"), NumNonStreamingPoolTextures, NonStreamingPoolSize/1024.0f );
//	debugf( TEXT("%34s: NumTextures=%4d, Current=%7.1f KB"), TEXT("Non-streaming non-pool textures"), NumNonStreamingTextures, NonStreamingSize/1024.0f );
#endif
}

/**
 * Prints out detailed information about streaming textures that has a name that contains the given string.
 * Triggered by the InvestigateTexture exec command.
 *
 * @param InvestigateTextureName	Partial name to match textures against
 */
void FStreamingManagerTexture::InvestigateTexture( const FString& InvestigateTextureName )
{
	bTriggerInvestigateTexture = FALSE;
#if !FINAL_RELEASE
	for ( INT TextureIndex=0; TextureIndex < StreamingTextures.Num(); ++TextureIndex )
	{
		FStreamingTexture& StreamingTexture = StreamingTextures(TextureIndex);
		FString TextureName = StreamingTexture.Texture->GetFullName();
		if ( TextureName.InStr( InvestigateTextureName, FALSE, TRUE ) >= 0 )
		{
			UTexture2D* Texture2D = StreamingTexture.Texture;
			ETextureStreamingType StreamType = StreamingTexture.GetStreamingType();
			INT CurrentMipIndex = Max(Texture2D->Mips.Num() - StreamingTexture.ResidentMips, 0);
			INT WantedMipIndex = Max(Texture2D->Mips.Num() - StreamingTexture.WantedMips, 0);
			INT MaxMipIndex = Max(Texture2D->Mips.Num() - StreamingTexture.MaxAllowedMips, 0);
			debugf( TEXT("Texture: %s"), *TextureName );
			debugf( TEXT("  Texture group:   %s"), UTexture::GetTextureGroupString(StreamingTexture.LODGroup) );
			if ( StreamType != StreamType_LastRenderTime && StreamingTexture.bUsesLastRenderHeuristics )
			{
				debugf( TEXT("  Stream logic:    %s and %s (%d references)"), GStreamTypeNames[StreamType], GStreamTypeNames[StreamType_LastRenderTime], StreamingTexture.LastRenderTimeRefCount );
			}
			else
			{
				debugf( TEXT("  Stream logic:    %s"), GStreamTypeNames[StreamType] );
			}
			if ( StreamingTexture.LastRenderTime > 1000000.0f )
			{
				debugf( TEXT("  Last rendertime: Never") );
			}
			else
			{
				debugf( TEXT("  Last rendertime: %.3f seconds ago"), StreamingTexture.LastRenderTime );
			}
			if ( StreamingTexture.ForceLoadRefCount > 0 )
			{
				debugf( TEXT("  Force all mips:  %d level references"), StreamingTexture.ForceLoadRefCount );
			}
			else if ( Texture2D->bGlobalForceMipLevelsToBeResident )
			{
				debugf( TEXT("  Force all mips:  bGlobalForceMipLevelsToBeResident") );
			}
			else if ( Texture2D->bForceMiplevelsToBeResident )
			{
				debugf( TEXT("  Force all mips:  bForceMiplevelsToBeResident") );
			}
			else if ( Texture2D->ShouldMipLevelsBeForcedResident() )
			{
				FLOAT CurrentTime = FLOAT(appSeconds() - GStartTime);
				FLOAT TimeLeft = CurrentTime - Texture2D->ForceMipLevelsToBeResidentTimestamp;
				debugf( TEXT("  Force all mips:  %.1f seconds left"), Max(TimeLeft,0.0f) );
			}
			else if ( StreamingTexture.MipCount == 1 )
			{
				debugf( TEXT("  Force all mips:  No mip-maps") );
			}
			debugf( TEXT("  Current size:    %dx%d"), Texture2D->Mips(CurrentMipIndex).SizeX, Texture2D->Mips(CurrentMipIndex).SizeY );
			debugf( TEXT("  Wanted size:     %dx%d"), Texture2D->Mips(WantedMipIndex).SizeX, Texture2D->Mips(WantedMipIndex).SizeY );
			debugf( TEXT("  Max size:        %dx%d"), Texture2D->Mips(MaxMipIndex).SizeX, Texture2D->Mips(MaxMipIndex).SizeY );
			debugf( TEXT("  Boost factor:    %.1f"), StreamingTexture.BoostFactor );

			FLOAT ScreenSizeFactor;
			switch ( StreamingTexture.LODGroup )
			{
				case TEXTUREGROUP_Lightmap:
					ScreenSizeFactor = GLightmapStreamingFactor;
					break;
				case TEXTUREGROUP_Shadowmap:
					ScreenSizeFactor = GShadowmapStreamingFactor;
					break;
				default:
					ScreenSizeFactor = 1.0f;
			}
			ScreenSizeFactor *= StreamingTexture.BoostFactor;

			// Iterate over all associated/ visible levels.
			for ( INT LevelIndex=0; LevelIndex < ThreadSettings.LevelData.Num(); ++LevelIndex )
			{
				// Find instances of the texture in this level.
				FStreamingManagerTexture::FThreadLevelData& LevelData = ThreadSettings.LevelData( LevelIndex ).Value;
				TArray<FStreamableTextureInstance4>* TextureInstances = LevelData.ThreadTextureInstances.Find( StreamingTexture.Texture );
				if ( TextureInstances )
				{
					for ( INT InstanceIndex=0; InstanceIndex < TextureInstances->Num(); ++InstanceIndex )
					{
						const FStreamableTextureInstance4& Instance = (*TextureInstances)(InstanceIndex);
						const FVector4& CenterX = Instance.BoundingSphereX;
						const FVector4& CenterY = Instance.BoundingSphereY;
						const FVector4& CenterZ = Instance.BoundingSphereZ;
						for ( INT PartialIndex=0; PartialIndex < 4; ++PartialIndex )
						{
							if ( CenterX[PartialIndex] < 3.402823466e+30F )
							{
								FVector Center( CenterX[PartialIndex], CenterY[PartialIndex], CenterZ[PartialIndex] );
								FLOAT Radius = Instance.BoundingSphereRadius[PartialIndex];
								FLOAT TexelFactor = Instance.TexelFactor[PartialIndex];
								FLOAT MinDistance = MAX_FLT;
								INT WantedMipCount = 0;
								for( INT ViewIndex=0; ViewIndex < ThreadNumViews(); ViewIndex++ )
								{
									// Calculate distance of viewer to bounding sphere.
									const FStreamingViewInfo& ViewInfo = ThreadGetView(ViewIndex);
									FLOAT Distance = (ViewInfo.ViewOrigin - Center).Size();
									MinDistance = Min(MinDistance, Distance);
									FLOAT DistSqMinusRadiusSq = ClampMeshToCameraDistanceSquared(Square(Distance) - Square(Radius));
									if( DistSqMinusRadiusSq > 1.f )
									{
										// Outside the texture instance bounding sphere, calculate miplevel based on screen space size of bounding sphere.
										// Calculate the maximum screen space dimension in pixels.
										const FLOAT ScreenSize = ViewInfo.ScreenSize * ViewInfo.BoostFactor * ScreenSizeFactor;
										const FLOAT	ScreenSizeInTexels = TexelFactor * appInvSqrtEst( DistSqMinusRadiusSq ) * ScreenSize;
										// WantedMipCount is the number of mips so we need to adjust with "+ 1".
										WantedMipCount = Max<INT>( WantedMipCount, ScreenTexelsToMipLevel(ScreenSizeInTexels) );
									}
									else
									{
										// Request all miplevels to be loaded if we're inside the bounding sphere.
										WantedMipCount = StreamingTexture.MaxAllowedMips;
										break;
									}
								}
								WantedMipCount = Clamp( WantedMipCount, StreamingTexture.MinAllowedMips, StreamingTexture.MaxAllowedMips );
								INT WantedMipIndex = Max(Texture2D->Mips.Num() - WantedMipCount, 0);
								debugf( TEXT("Static: Wanted=%dx%d, Distance=%.1f, TexelFactor=%.2f, Radius=%5.1f, Position=(%d,%d,%d)"),
									Texture2D->Mips(WantedMipIndex).SizeX, Texture2D->Mips(WantedMipIndex).SizeY,
									MinDistance, TexelFactor, Radius, INT(Center.X), INT(Center.Y), INT(Center.Z) );
							}
						}
					}
				}
			}

			// Dump all dynamic instances using this texture:
			for ( TMap<const UPrimitiveComponent*,FSpawnedPrimitiveData>::TIterator It(ThreadSettings.SpawnedPrimitives); It; ++It )
			{
				const UPrimitiveComponent* Primitive = It.Key();
				FSpawnedPrimitiveData& PrimitiveData = It.Value();
				DumpTextureInstances( Primitive, PrimitiveData, Texture2D );
			}
		}
	}
#endif
}

void FStreamingManagerTexture::DumpTextureInstances( const UPrimitiveComponent* Primitive, FSpawnedPrimitiveData& PrimitiveData, UTexture2D* Texture2D )
{
#if !FINAL_RELEASE
	const TCHAR* TypeString = (PrimitiveData.DynamicType == DPT_Level) ? TEXT("Dynamic (level)") : TEXT("Dynamic (spawned)");
	for ( INT InstanceIndex=0; InstanceIndex < PrimitiveData.TextureInstances.Num(); ++InstanceIndex )
	{
		FSpawnedTextureInstance& Instance = PrimitiveData.TextureInstances(InstanceIndex);
		if ( Instance.Texture2D == Texture2D )
		{
			const FStreamingTexture& StreamingTexture = GetStreamingTexture(Texture2D);
			FLOAT ScreenSizeFactor;
			switch ( StreamingTexture.LODGroup )
			{
				case TEXTUREGROUP_Lightmap:
					ScreenSizeFactor = GLightmapStreamingFactor;
					break;
				case TEXTUREGROUP_Shadowmap:
					ScreenSizeFactor = GShadowmapStreamingFactor;
					break;
				default:
					ScreenSizeFactor = 1.0f;
			}
			ScreenSizeFactor *= StreamingTexture.BoostFactor;

			const UStaticMeshComponent* StaticMeshComponent = ConstCast<UStaticMeshComponent>(Primitive);
			const USkeletalMeshComponent* SkeletalMeshComponent = ConstCast<USkeletalMeshComponent>(Primitive);
			FString MeshName;
			if ( StaticMeshComponent )
			{
				MeshName = StaticMeshComponent->StaticMesh->GetName();
			}
			else if ( SkeletalMeshComponent )
			{
				MeshName = SkeletalMeshComponent->SkeletalMesh->GetName();
			}
			FLOAT MinDistance = MAX_FLT;
			INT WantedMipCount = 0;
			for( INT ViewIndex=0; ViewIndex < ThreadNumViews(); ViewIndex++ )
			{
				// Calculate distance of viewer to bounding sphere.
				const FStreamingViewInfo& ViewInfo = ThreadGetView(ViewIndex);
				FLOAT Distance = (ViewInfo.ViewOrigin - PrimitiveData.BoundingSphere.Center).Size();
				MinDistance = Min(MinDistance, Distance);
				const FLOAT ScreenSize = ViewInfo.ScreenSize * ViewInfo.BoostFactor * ScreenSizeFactor;
				FLOAT DistSqMinusRadiusSq = ClampMeshToCameraDistanceSquared(Square(Distance) - Square(PrimitiveData.BoundingSphere.W));
				if( DistSqMinusRadiusSq > 1.f )
				{
					FLOAT TexelFactorScale = PrimitiveData.BoundingSphere.W * Instance.InvOriginalRadius;
					FLOAT TexelFactor = Instance.TexelFactor * TexelFactorScale;

					// Outside the texture instance bounding sphere, calculate miplevel based on screen space size of bounding sphere.
					// Calculate the maximum screen space dimension in pixels.
					const FLOAT	ScreenSizeInTexels = TexelFactor * appInvSqrtEst( DistSqMinusRadiusSq ) * ScreenSize;
					// WantedMipCount is the number of mips so we need to adjust with "+ 1".
					WantedMipCount = Max<INT>( WantedMipCount, ScreenTexelsToMipLevel(ScreenSizeInTexels) );
				}
				else
				{
					// Request all miplevels to be loaded if we're inside the bounding sphere.
					WantedMipCount = StreamingTexture.MaxAllowedMips;
					break;
				}
			}
			WantedMipCount = Clamp( WantedMipCount, StreamingTexture.MinAllowedMips, StreamingTexture.MaxAllowedMips );
			INT WantedMipIndex = Max(Texture2D->Mips.Num() - WantedMipCount, 0);
			debugf( TEXT("%s: Wanted=%dx%d, Distance=%.1f, TexelFactor=%.2f, CurrentRadius=%5.1f, OriginalRadius=%5.1f, Position=(%d,%d,%d), IsAttached=%d, Mesh=\"%s\", Component=\"%s\""),
				TypeString, Texture2D->Mips(WantedMipIndex).SizeX, Texture2D->Mips(WantedMipIndex).SizeY,
				MinDistance, Instance.TexelFactor, PrimitiveData.BoundingSphere.W, 1.0f / Instance.InvOriginalRadius,
				INT(PrimitiveData.BoundingSphere.Center.X), INT(PrimitiveData.BoundingSphere.Center.Y), INT(PrimitiveData.BoundingSphere.Center.Z),
				PrimitiveData.bAttached ? TRUE : FALSE, *MeshName,
				*Primitive->GetFullName() );
		}
	}
#endif
}


/*-----------------------------------------------------------------------------
	Streaming handler implementations.
-----------------------------------------------------------------------------*/

/**
 * Returns mip count wanted by this handler for the passed in texture. 
 *
 * @param	StreamingManager	Streaming manager
 * @param	Streaming Texture	Texture to determine wanted mip count for
 * @param	PerfectWantedMips	[out] Desired number of miplevels if the fudge factor was 1.0, or INDEX_NONE if not handled or STATS isn't defined.
 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
 */
INT FStreamingHandlerTextureStatic::GetWantedMips( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, INT& PerfectWantedMips, FLOAT& MinDistance )
{
	if ( StreamingManager.IsUsingPriorityStreaming() )
	{
		return GetWantedMips2( StreamingManager, StreamingTexture, PerfectWantedMips, MinDistance );
	}

	// INDEX_NONE signals handler  not handling the texture.
	INT		WantedMipCount		= INDEX_NONE;
	PerfectWantedMips			= INDEX_NONE;
	// Whether we should early out if we e.g. know that we need all allowed mips.
	UBOOL	bShouldAbortLoop	= FALSE;

	// Nothing do to if there are no views.
	if( StreamingManager.GetNumViews() )
	{
		FLOAT ScreenSizeFactor;
		switch ( StreamingTexture.LODGroup )
		{
			case TEXTUREGROUP_Lightmap:
				ScreenSizeFactor = GLightmapStreamingFactor;
				break;
			case TEXTUREGROUP_Shadowmap:
				ScreenSizeFactor = GShadowmapStreamingFactor;
				break;
			default:
				ScreenSizeFactor = 1.0f;
		}

		ScreenSizeFactor *= StreamingTexture.BoostFactor;

		// Cached so it's only calculated once per texture.
		const FLOAT SquaredFudgeFactor = Square(StreamingManager.GetFudgeFactor());
		FLOAT MinDistanceSq = FLT_MAX;
		UTexture2D* Texture = StreamingTexture.Texture;

		// Iterate over all associated/ visible levels.
		for( INT LevelIndex=0; LevelIndex<GWorld->Levels.Num() && !bShouldAbortLoop; LevelIndex++ )
		{
			// Find instances of the texture in this level.
			ULevel*								Level				= GWorld->Levels(LevelIndex);
			TArray<FStreamableTextureInstance>* TextureInstances	= Level->TextureToInstancesMap.Find( Texture );

			// Nothing to do if there are no instances.
			if( TextureInstances && TextureInstances->Num() )
			{
				STAT( StreamingManager.NumStreamingTextureInstances += TextureInstances->Num() );

				UBOOL bDebug = FALSE;
				FSphere DebugSphere;
				if ( StreamingTexture.LODGroup == TEXTUREGROUP_Lightmap || StreamingTexture.LODGroup == TEXTUREGROUP_Shadowmap )
				{
					STAT( StreamingManager.NumStreamingLightmapInstances += TextureInstances->Num() );
#ifdef _DEBUG
					if ( GDebugSelectedLightmap && GDebugSelectedLightmap->GetTexture(0) == Texture )
					{
						if ( GDebugSelectedComponent )
						{
							DebugSphere = GDebugSelectedComponent->Bounds.GetSphere();
							bDebug = TRUE;
						}
					}
#endif
				}

				// Iterate over all view infos.
				for( INT ViewIndex=0; ViewIndex < StreamingManager.GetNumViews() && !bShouldAbortLoop; ViewIndex++ )
				{
					const FStreamingViewInfo& ViewInfo	= StreamingManager.GetViewInformation(ViewIndex);
					const FLOAT ScreenSize = ViewInfo.ScreenSize * ViewInfo.BoostFactor * ScreenSizeFactor;

					// Iterate over all instances of the texture in the level.
					for( INT InstanceIndex=0; InstanceIndex < TextureInstances->Num() && !bShouldAbortLoop; InstanceIndex++ )
					{
						FStreamableTextureInstance& TextureInstance = (*TextureInstances)(InstanceIndex);

#ifdef _DEBUG
						if ( bDebug && TextureInstance.BoundingSphere.Equals(DebugSphere) )
						{
							INT q = 0;	// To be able to set breakpoints here.
						}
#endif

						// Calculate distance of viewer to bounding sphere.
						const FLOAT	DistSq					= FDistSquared( ViewInfo.ViewOrigin, TextureInstance.BoundingSphere.Center ) * SquaredFudgeFactor;
						const FLOAT	DistSqMinusRadiusSq		= ClampMeshToCameraDistanceSquared(DistSq - Square(TextureInstance.BoundingSphere.W));

						// Outside the texture instance bounding sphere, calculate miplevel based on screen space size of bounding sphere.
						if( DistSqMinusRadiusSq > 1.f )
						{
							// Calculate the maximum screen space dimension in pixels.
							const FLOAT	ScreenSizeInTexels	= TextureInstance.TexelFactor * appInvSqrtEst( DistSqMinusRadiusSq ) * ScreenSize;
							// WantedMipCount is the number of mips so we need to adjust with "+ 1".
							WantedMipCount				= Max<INT>( WantedMipCount, ScreenTexelsToMipLevel(ScreenSizeInTexels) );
							MinDistanceSq				= Min<FLOAT>( MinDistanceSq, DistSqMinusRadiusSq );
						}
						// Request all miplevels to be loaded if we're inside the bounding sphere.
						else
						{
							WantedMipCount				= StreamingTexture.MaxAllowedMips;
							MinDistanceSq				= 1.0f;
							bShouldAbortLoop			= TRUE;
						}

#if STATS_FAST
						const FLOAT	OptimalDistSq = FDistSquared( ViewInfo.ViewOrigin, TextureInstance.BoundingSphere.Center );
						const FLOAT	OptimalDistSqMinusRadiusSq = ClampMeshToCameraDistanceSquared(OptimalDistSq - Square(TextureInstance.BoundingSphere.W));
						if( OptimalDistSqMinusRadiusSq > 1.f )
						{
							const FLOAT	ScreenSizeInTexels	= TextureInstance.TexelFactor * appInvSqrtEst( OptimalDistSqMinusRadiusSq ) * ScreenSize;
							PerfectWantedMips = Max<INT>( PerfectWantedMips, ScreenTexelsToMipLevel(ScreenSizeInTexels) );
						}
						else
						{
							PerfectWantedMips = StreamingTexture.MipCount;
						}
#endif

						// Early out if we're already at the max.
						if( WantedMipCount >= StreamingTexture.MaxAllowedMips && !bDebug )
						{
							bShouldAbortLoop = TRUE;
						}
					}
				}
			}
		}
		MinDistance = appSqrt( MinDistanceSq );
	}

	return WantedMipCount;
}

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
INT FStreamingHandlerTextureStatic::GetWantedMips2( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, INT& PerfectWantedMips, FLOAT& MinDistance )
{
	// INDEX_NONE signals that the handler isn't handling the texture.
	INT		WantedMipCount		= INDEX_NONE;
	PerfectWantedMips			= INDEX_NONE;
	UBOOL bShouldAbortLoop		= FALSE;

	// Nothing do to if there are no views or instances.
	if( StreamingManager.ThreadNumViews() /*&& StreamingTexture.Instances.Num() > 0*/ )
	{
		FLOAT ScreenSizeFactor;
		switch ( StreamingTexture.LODGroup )
		{
			case TEXTUREGROUP_Lightmap:
				ScreenSizeFactor = GLightmapStreamingFactor;
				break;
			case TEXTUREGROUP_Shadowmap:
				ScreenSizeFactor = GShadowmapStreamingFactor;
				break;
			default:
				ScreenSizeFactor = 1.0f;
		}

		ScreenSizeFactor *= StreamingTexture.BoostFactor;

		VectorRegister MinDistanceSq4 = VectorSet((FLOAT)FLT_MAX, (FLOAT)FLT_MAX, (FLOAT)FLT_MAX, (FLOAT)FLT_MAX);
		VectorRegister MaxTexels = VectorSet(-(FLOAT)FLT_MAX, -(FLOAT)FLT_MAX, -(FLOAT)FLT_MAX, -(FLOAT)FLT_MAX);

		for ( INT LevelIndex=0; LevelIndex < StreamingManager.ThreadSettings.LevelData.Num(); ++LevelIndex )
		{
			FStreamingManagerTexture::FThreadLevelData& LevelData = StreamingManager.ThreadSettings.LevelData( LevelIndex ).Value;
			TArray<FStreamableTextureInstance4>* TextureInstances = LevelData.ThreadTextureInstances.Find( StreamingTexture.Texture );
			if ( TextureInstances )
			{
				for ( INT InstanceIndex=0; InstanceIndex < TextureInstances->Num() && !bShouldAbortLoop; ++InstanceIndex )
				{
					const FStreamableTextureInstance4& TextureInstance = (*TextureInstances)(InstanceIndex);

					// Calculate distance of viewer to bounding sphere.
					const VectorRegister CenterX = VectorLoadAligned( &TextureInstance.BoundingSphereX );
					const VectorRegister CenterY = VectorLoadAligned( &TextureInstance.BoundingSphereY );
					const VectorRegister CenterZ = VectorLoadAligned( &TextureInstance.BoundingSphereZ );

					// Iterate over all view infos.
					for( INT ViewIndex=0; ViewIndex < StreamingManager.ThreadNumViews() && !bShouldAbortLoop; ViewIndex++ )
					{
						const FStreamingViewInfo& ViewInfo = StreamingManager.ThreadGetView(ViewIndex);
						const FLOAT ScreenSizeFloat = ViewInfo.ScreenSize * ViewInfo.BoostFactor * ScreenSizeFactor;
						const VectorRegister ScreenSize = VectorLoadFloat1( &ScreenSizeFloat );
						const VectorRegister ViewOriginX = VectorLoadFloat1( &ViewInfo.ViewOrigin.X );
						const VectorRegister ViewOriginY = VectorLoadFloat1( &ViewInfo.ViewOrigin.Y );
						const VectorRegister ViewOriginZ = VectorLoadFloat1( &ViewInfo.ViewOrigin.Z );

						//const FLOAT DistSq = FDistSquared( ViewInfo.ViewOrigin, TextureInstance.BoundingSphere.Center ) * SquaredFudgeFactor;
						VectorRegister Temp = VectorSubtract( ViewOriginX, CenterX );
						VectorRegister DistSq = VectorMultiply( Temp, Temp );
						Temp = VectorSubtract( ViewOriginY, CenterY );
						DistSq = VectorMultiplyAdd( Temp, Temp, DistSq );
						Temp = VectorSubtract( ViewOriginZ, CenterZ );
						DistSq = VectorMultiplyAdd( Temp, Temp, DistSq );
//						DistSq = VectorMultiply( DistSq, SquaredFudgeFactor );

//						const FLOAT DistSqMinusRadiusSq = DistSq - Square(TextureInstance.BoundingSphere.W);
						VectorRegister DistSqMinusRadiusSq = VectorLoadAligned( &TextureInstance.BoundingSphereRadius );
						DistSqMinusRadiusSq = VectorMultiply( DistSqMinusRadiusSq, DistSqMinusRadiusSq );
						DistSqMinusRadiusSq = VectorSubtract( DistSq, DistSqMinusRadiusSq );
						MinDistanceSq4 = VectorMin( MinDistanceSq4, DistSqMinusRadiusSq );

						if ( VectorAllGreaterThan( DistSqMinusRadiusSq, VectorOne() ) )
						{
							if (StreamingTexture.LODGroup != TEXTUREGROUP_Terrain_Heightmap)
							{
								// Calculate the maximum screen space dimension in pixels.
								//const FLOAT ScreenSizeInTexels = TextureInstance.TexelFactor * appInvSqrtEst( DistSqMinusRadiusSq ) * ScreenSize;
								DistSqMinusRadiusSq = VectorMax( DistSqMinusRadiusSq, VectorOne() );
								VectorRegister Texels = VectorMultiply( VectorLoadAligned( &TextureInstance.TexelFactor ), VectorReciprocalSqrt(DistSqMinusRadiusSq) );
								Texels = VectorMultiply( Texels, ScreenSize );
								MaxTexels = VectorMax( MaxTexels, Texels );
							}
							else
							{
								// To check Forced LOD...
								VectorRegister MinForcedLODs = VectorLoadAligned( &TextureInstance.TexelFactor );
								UBOOL AllForcedLOD = VectorAllLesserThan(MinForcedLODs, VectorZero());
								MinForcedLODs = VectorMin( MinForcedLODs, VectorSwizzle(MinForcedLODs, 2, 3, 0, 1) );
								MinForcedLODs = VectorMin( MinForcedLODs, VectorReplicate(MinForcedLODs, 1) );
								FLOAT MinLODValue;
								VectorStoreFloat1( MinForcedLODs, &MinLODValue );

								if (MinLODValue <= 0)
								{
									WantedMipCount = Max<INT>(WantedMipCount, StreamingTexture.MaxAllowedMips - 13 - appFloor(MinLODValue));
									if (WantedMipCount == StreamingTexture.MaxAllowedMips)
									{
										MinDistance = 1.0f;
										bShouldAbortLoop = TRUE;
									}
								}

								if (!AllForcedLOD)
								{
									// Calculate the maximum screen space dimension in pixels.
									DistSqMinusRadiusSq = VectorMax( DistSqMinusRadiusSq, VectorOne() );
									VectorRegister Texels = VectorMultiply( VectorLoadAligned( &TextureInstance.TexelFactor ), VectorReciprocalSqrt(DistSqMinusRadiusSq) );
									Texels = VectorMultiply( Texels, ScreenSize );
									MaxTexels = VectorMax( MaxTexels, Texels );
								}
							}
						}
						else
						{
							WantedMipCount = StreamingTexture.MaxAllowedMips;
							MinDistance = 1.0f;
							bShouldAbortLoop = TRUE;
						}
						StreamingTexture.bUsesStaticHeuristics = TRUE;
					}
				}

				if (StreamingTexture.LODGroup == TEXTUREGROUP_Terrain_Heightmap && VectorAllLesserThan(MaxTexels, VectorZero())) // All ForcedLOD cases...
				{
					MinDistance = 1.0f;
					bShouldAbortLoop = TRUE;
				}

				if ( StreamingTexture.bUsesStaticHeuristics && !bShouldAbortLoop )
				{
					MinDistanceSq4 = VectorMin( MinDistanceSq4, VectorSwizzle(MinDistanceSq4, 2, 3, 0, 1) );
					MinDistanceSq4 = VectorMin( MinDistanceSq4, VectorReplicate(MinDistanceSq4, 1) );
					FLOAT MinDistanceSq;
					VectorStoreFloat1( MinDistanceSq4, &MinDistanceSq );
					//@todo LHS?
					MinDistanceSq = ClampMeshToCameraDistanceSquared(MinDistanceSq);
					if ( MinDistanceSq > 1.0f )
					{
						MaxTexels = VectorMax( MaxTexels, VectorSwizzle(MaxTexels, 2, 3, 0, 1) );
						MaxTexels = VectorMax( MaxTexels, VectorReplicate(MaxTexels, 1) );
						FLOAT ScreenSizeInTexels;
						VectorStoreFloat1( MaxTexels, &ScreenSizeInTexels );
						// WantedMipCount is the number of mips so we need to adjust with "+ 1".
						WantedMipCount = Max<INT>(WantedMipCount, ScreenTexelsToMipLevel(ScreenSizeInTexels));
						MinDistance = Min( MinDistanceSq, appSqrt( MinDistanceSq ) );
					}
					else
					{
						WantedMipCount = StreamingTexture.MaxAllowedMips;
						MinDistance = 1.0f;
					}
				}
			}
		}
	}

	PerfectWantedMips = WantedMipCount;
	return WantedMipCount;
}

/**
 * Calculates DynamicWantedMips and DynamicMinDistanceSq for all dynamic textures.
 */
void FStreamingManagerTexture::CalcDynamicWantedMips()
{
	STAT( DOUBLE StartTime = appSeconds() );

	// Reset the dynamic variables for all textures.
	const INT PrefetchCount = AlignArbitrary( 4*CACHE_LINE_SIZE, sizeof(FStreamingTexture) ) / sizeof(FStreamingTexture);
	for ( INT Index=0; Index < StreamingTextures.Num() - PrefetchCount; ++Index )
	{
		FStreamingTexture& StreamingTexture = StreamingTextures( Index );
		CONSOLE_PREFETCH( ((BYTE*)&StreamingTexture) + CACHE_LINE_SIZE );
		CONSOLE_PREFETCH( ((BYTE*)&StreamingTexture) + CACHE_LINE_SIZE*2 );
		CONSOLE_PREFETCH( ((BYTE*)&StreamingTexture) + CACHE_LINE_SIZE*3 );
		StreamingTexture.DynamicScreenSize = 0.0F;
		StreamingTexture.DynamicMinDistanceSq = FLT_MAX;
	}
	// The last ones
	for ( INT Index=Max(StreamingTextures.Num() - PrefetchCount, 0); Index < StreamingTextures.Num(); ++Index )
	{
		FStreamingTexture& StreamingTexture = StreamingTextures( Index );
		StreamingTexture.DynamicScreenSize = 0.0F;
		StreamingTexture.DynamicMinDistanceSq = FLT_MAX;
	}

	// Iterate over all dynamic primitives.
	for ( TMap<const UPrimitiveComponent*,FSpawnedPrimitiveData>::TIterator It(ThreadSettings.SpawnedPrimitives); It; ++It )
	{
		const UPrimitiveComponent* Primitive = It.Key();
		FSpawnedPrimitiveData& PrimitiveData = It.Value();

		UBOOL bIsPrimitiveUpToDate = TRUE;

		// Only account for this instance if the corresponding primitive is fully attached to the scene, and has a size.
		if ( PrimitiveData.bAttached && !PrimitiveData.bPendingUpdate && !appIsNearlyZero(PrimitiveData.BoundingSphere.W) )
		{
			for ( INT TextureIndex=0; TextureIndex < PrimitiveData.TextureInstances.Num(); ++TextureIndex )
			{
				if ( TextureIndex < PrimitiveData.TextureInstances.Num() - 2 )
				{
					// Prefetch PrimitiveData
					CONSOLE_PREFETCH( &PrimitiveData.TextureInstances(TextureIndex + 2) );
				}
				if ( TextureIndex < PrimitiveData.TextureInstances.Num() - 1 )
				{
					// Prefetch some members of UTexture2D
					FSpawnedTextureInstance& TextureInstance = PrimitiveData.TextureInstances(TextureIndex + 1);
					CONSOLE_PREFETCH( ((BYTE*)TextureInstance.Texture2D) + sizeof(USurface) );		// NeverStream
					CONSOLE_PREFETCH( &TextureInstance.Texture2D->Mips );
					CONSOLE_PREFETCH( ((BYTE*)&TextureInstance.Texture2D->AddressY) + 1 );			// bIsStreamable
					CONSOLE_PREFETCH( &TextureInstance.Texture2D->StreamingIndex );
				}

				FSpawnedTextureInstance& TextureInstance = PrimitiveData.TextureInstances(TextureIndex);
				bIsPrimitiveUpToDate = bIsPrimitiveUpToDate && IsManagedStreamingTexture(TextureInstance.Texture2D);
				if ( bIsPrimitiveUpToDate )
				{
					//@TODO: Accessing UTexture2D here prevents this function from being moved into the worker thread.
					FStreamingTexture& StreamingTexture = GetStreamingTexture( TextureInstance.Texture2D );
					bIsPrimitiveUpToDate = !StreamingTexture.bNeedPrimitiveUpdate;
					if ( bIsPrimitiveUpToDate )
					{
						StreamingTexture.bUsesDynamicHeuristics = TRUE;
						const FLOAT SquaredFudgeFactor = 1.0f;

						FLOAT ScreenSizeFactor = 1.0f;
						switch ( StreamingTexture.LODGroup )
						{
							case TEXTUREGROUP_Lightmap:
								ScreenSizeFactor = GLightmapStreamingFactor;
								break;
							case TEXTUREGROUP_Shadowmap:
								ScreenSizeFactor = GShadowmapStreamingFactor;
								break;
						}

						ScreenSizeFactor *= StreamingTexture.BoostFactor;

						// Iterate over all view infos.
						for( INT ViewIndex=0; ViewIndex < ThreadNumViews(); ViewIndex++ )
						{
							const FStreamingViewInfo& ViewInfo	= ThreadGetView(ViewIndex);
							const FLOAT ScreenSize = ViewInfo.ScreenSize * ViewInfo.BoostFactor * ScreenSizeFactor;

							// Calculate distance of viewer to bounding sphere.
							const FLOAT	DistSq					= FDistSquared( ViewInfo.ViewOrigin, PrimitiveData.BoundingSphere.Center ) * SquaredFudgeFactor;
							const FLOAT	DistSqMinusRadiusSq		= ClampMeshToCameraDistanceSquared(DistSq - Square(PrimitiveData.BoundingSphere.W));

							// Outside the texture instance bounding sphere, calculate miplevel based on screen space size of bounding sphere.
							if( DistSqMinusRadiusSq > 1.f )
							{
								// Scale the texture density based on current size vs original size.
								FLOAT TexelFactorScale			= PrimitiveData.BoundingSphere.W * TextureInstance.InvOriginalRadius;
								FLOAT TexelFactor				= TextureInstance.TexelFactor * TexelFactorScale;

								// Calculate the maximum screen space dimension in pixels.
								const FLOAT	ScreenSizeInTexels			= TexelFactor * appInvSqrtEst( DistSqMinusRadiusSq ) * ScreenSize;
								StreamingTexture.DynamicScreenSize		= Max<FLOAT>( StreamingTexture.DynamicScreenSize, ScreenSizeInTexels );
								StreamingTexture.DynamicMinDistanceSq	= Min<FLOAT>( StreamingTexture.DynamicMinDistanceSq, DistSqMinusRadiusSq );
							}
							// Request all miplevels to be loaded if we're inside the bounding sphere.
							else
							{
//								StreamingTexture.DynamicScreenSize		= StreamingTexture.MaxAllowedMips;
								StreamingTexture.DynamicScreenSize		= 1024.0f*1024.0f;	// Let's say we want a 1 Mpix resolution mip, so we get max...
								StreamingTexture.DynamicMinDistanceSq	= 1.0f;
								break;
							}
						}
					}

					//@DEBUG:
//					UBOOL bTrackedTexture = TrackTextureEvent( &StreamingTexture, StreamingTexture.Texture, FALSE, ENABLE_TEXTURE_LOGGING, StreamingTexture.bForceFullyLoad );
				}
			}
		}

		if ( !bIsPrimitiveUpToDate )
		{
			NotifyPrimitiveUpdated( Primitive );
		}
	}
	STAT( GStreamingDynamicPrimitivesTime += appSeconds() - StartTime );
}

/**
 * Returns mip count wanted by this handler for the passed in texture. 
 * 
 * @param	StreamingManager	Streaming manager
 * @param	Streaming Texture	Texture to determine wanted mip count for
 * @param	PerfectWantedMips	[out] Desired number of miplevels if the fudge factor was 1.0, or INDEX_NONE if not handled or STATS isn't defined.
 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
 */
INT FStreamingHandlerTextureLastRender::GetWantedMips( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, INT& PerfectWantedMips, FLOAT& MinDistance )
{
	FLOAT SecondsSinceLastRender = StreamingTexture.LastRenderTime;	//(GCurrentTime - Texture->Resource->LastRenderTime); // * FudgeFactor;
	StreamingTexture.bUsesLastRenderHeuristics = TRUE;

	if( SecondsSinceLastRender < 45.0f && GStreamWithTimeFactor )
	{
		PerfectWantedMips = StreamingTexture.MaxAllowedMips;
		MinDistance = 0;
		return StreamingTexture.MaxAllowedMips;
	}
	else if( SecondsSinceLastRender < 90.0f && GStreamWithTimeFactor )
	{
		PerfectWantedMips = StreamingTexture.MaxAllowedMips - 1;
		MinDistance = 1000.0f;
		return StreamingTexture.MaxAllowedMips - 1;
	}
	else
	{
		MinDistance = 10000.0f;
		PerfectWantedMips = 0;
		return 0;
	}
}

/**
 * Returns mip count wanted by this handler for the passed in texture. 
 * 
 * @param	StreamingManager	Streaming manager
 * @param	Streaming Texture	Texture to determine wanted mip count for
 * @param	PerfectWantedMips	[out] Desired number of miplevels if the fudge factor was 1.0, or INDEX_NONE if not handled or STATS isn't defined.
 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
 */
INT FStreamingHandlerTextureLevelForced::GetWantedMips( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, INT& PerfectWantedMips, FLOAT& MinDistance )
{
	if ( StreamingManager.IsUsingPriorityStreaming() )
	{
		return GetWantedMips2( StreamingManager, StreamingTexture, PerfectWantedMips, MinDistance );
	}
	UTexture2D* Texture = StreamingTexture.Texture;
	INT WantedMipCount = INDEX_NONE;
	for( INT LevelIndex=0; LevelIndex<GWorld->Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = GWorld->Levels(LevelIndex);
		if( Level->ForceStreamTextures.Find( Texture ) )
		{
			WantedMipCount = StreamingTexture.MaxAllowedMips;
			break;
		}
	}
	PerfectWantedMips = WantedMipCount;
	return WantedMipCount;
}

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
INT FStreamingHandlerTextureLevelForced::GetWantedMips2( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, INT& PerfectWantedMips, FLOAT& MinDistance )
{
	INT WantedMipCount = INDEX_NONE;
	if ( StreamingTexture.ForceLoadRefCount > 0 )
	{
		StreamingTexture.bUsesForcedHeuristics = TRUE;
		WantedMipCount = StreamingTexture.MaxAllowedMips;
	}
	PerfectWantedMips = WantedMipCount;
	return WantedMipCount;
}

/*-----------------------------------------------------------------------------
	Texture streaming helper structs.
-----------------------------------------------------------------------------*/

/**
 * FStreamableTextureInstance serialize operator.
 *
 * @param	Ar					Archive to to serialize object to/ from
 * @param	TextureInstance		Object to serialize
 * @return	Returns the archive passed in
 */
FArchive& operator<<( FArchive& Ar, FStreamableTextureInstance& TextureInstance )
{
	Ar << TextureInstance.BoundingSphere;
	Ar << TextureInstance.TexelFactor;
	return Ar;
}

/**
 * FDynamicTextureInstance serialize operator.
 *
 * @param	Ar					Archive to to serialize object to/ from
 * @param	TextureInstance		Object to serialize
 * @return	Returns the archive passed in
 */
FArchive& operator<<( FArchive& Ar, FDynamicTextureInstance& TextureInstance )
{
	Ar << TextureInstance.BoundingSphere;
	Ar << TextureInstance.TexelFactor;
	Ar << TextureInstance.Texture;
	Ar << TextureInstance.bAttached;
	Ar << TextureInstance.OriginalRadius;
	return Ar;
}


/** Constructor for the texture streaming memory tracker. */
FStreamMemoryTracker::FStreamMemoryTracker()
:	PendingStreamIn(0)
,	PendingTempMemory(0)
,	CurrentStreamOut(0)
,	CurrentTempMemory(0)
{
}

/** Track 'start streaming' on the gamethread. (Memory not yet affected.) */
void FStreamMemoryTracker::GameThread_BeginUpdate( const UTexture2D& Texture )
{
	INT ResidentSize = Texture.CalcTextureMemorySize( Texture.ResidentMips );
	INT RequestedSize = Texture.CalcTextureMemorySize( Texture.RequestedMips );

	// Always plan on allocating temporary memory (we don't know if we're going to use in-place realloc yet).
	appInterlockedAdd( &PendingTempMemory, ResidentSize );	//@TODO: 0 for pre-planned in-place realloc 

	if ( RequestedSize > ResidentSize )
	{
		// Plan on allocating new mips.
		appInterlockedAdd( &PendingStreamIn, RequestedSize - ResidentSize );
	}
	else
	{
		// Plan on freeing mips.
		appInterlockedAdd( &CurrentStreamOut, ResidentSize - RequestedSize );
	}
}

/**
 * Track 'start streaming' on the renderthread. (Memory is now allocated/deallocated.)
 * 
 * @param Texture				Texture that is beginning to stream
 * @param bUsingInPlaceRealloc	Whether it's using in-place reallocation
 * @param bSuccessful			Whether the update succeeded or not
 */
void FStreamMemoryTracker::RenderThread_Update( const UTexture2D& Texture, UBOOL bUsingInPlaceRealloc, UBOOL bSuccessful )
{
	INT ResidentSize = Texture.CalcTextureMemorySize( Texture.ResidentMips );
	INT RequestedSize = Texture.CalcTextureMemorySize( Texture.RequestedMips );

	// We're not planning to use temp memory anymore - we're now either using it or not.
	appInterlockedAdd( &PendingTempMemory, -ResidentSize );		//@TODO: 0 for pre-planned in-place realloc

	// Did we allocate temp memory?
	if ( !bUsingInPlaceRealloc && bSuccessful )
	{
		// When not using in-place realloc, track allocated temp memory.
		appInterlockedAdd( &CurrentTempMemory, ResidentSize );
	}

	// Are we streaming in?
	if ( RequestedSize > ResidentSize )
	{
		// New mips have now been allocated, they're not pending anymore.
		appInterlockedAdd( &PendingStreamIn, -(RequestedSize - ResidentSize) );
	}
}

/**
 * Track 'streaming finished' on the renderthread.
 * Note: Only called if the RenderThread Update was successful.
 *
 * @param Texture				Texture that is being finalized
 * @param bUsingInPlaceRealloc	Whether it's using in-place reallocation
 * @param bSuccessful			Whether the finalize succeeded or not
 */
void FStreamMemoryTracker::RenderThread_Finalize( const UTexture2D& Texture, UBOOL bUsingInPlaceRealloc, UBOOL bSuccessful )
{
	INT ResidentSize = Texture.CalcTextureMemorySize( Texture.ResidentMips );
	INT RequestedSize = Texture.CalcTextureMemorySize( Texture.RequestedMips );

	if ( !bUsingInPlaceRealloc )
	{
		// When not using in-place realloc, track freed temp memory.
		appInterlockedAdd( &CurrentTempMemory, -ResidentSize );	
	}

	// Are we streaming out?
	if ( RequestedSize < ResidentSize )
	{
		// The mips have been freed or canceled.
		appInterlockedAdd( &CurrentStreamOut, -(ResidentSize - RequestedSize) );
	}
}

/** Calculate how much texture memory is currently available for streaming. */
INT FStreamMemoryTracker::CalcAvailableNow( INT TotalFreeMemory, INT MemoryMargin )
{
	// Conservative.
	return TotalFreeMemory - PendingStreamIn - PendingTempMemory - MemoryMargin;
}

/** Calculate how much texture memory will available later for streaming. */
INT FStreamMemoryTracker::CalcAvailableLater( INT TotalFreeMemory, INT MemoryMargin )
{
	return TotalFreeMemory - PendingStreamIn + CurrentStreamOut + CurrentTempMemory - MemoryMargin;
}

/** Calculate how much texture memory is currently being used for temporary texture data during streaming. */
INT FStreamMemoryTracker::CalcTempMemory()
{
	return PendingTempMemory + CurrentTempMemory;
}

/** Texture streaming memory tracker. */
FStreamMemoryTracker GStreamMemoryTracker;
