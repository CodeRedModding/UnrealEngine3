/*=============================================================================
	ScenePrivate.h: Private scene manager definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SCENEPRIVATE_H__
#define __SCENEPRIVATE_H__

// Shortcuts for the allocators used by scene rendering.
class SceneRenderingAllocator
	: public TMemStackAllocator<GRenderingThreadMemStack>
{
};


class SceneRenderingBitArrayAllocator
	: public TInlineAllocator<4,SceneRenderingAllocator>
{
};

class SceneRenderingSparseArrayAllocator
	: public TSparseArrayAllocator<SceneRenderingAllocator,SceneRenderingBitArrayAllocator>
{
};

class SceneRenderingSetAllocator
	: public TSetAllocator<SceneRenderingSparseArrayAllocator,TInlineAllocator<1,SceneRenderingAllocator> >
{
};

// Forward declarations.
class FScene;

/** max DPG for scene rendering */
enum { SDPG_MAX_SceneRender = SDPG_PostProcess };

// Dependencies.
#include "StaticBoundShaderState.h"
#include "BatchedElements.h"
#include "SceneRenderTargets.h"
#include "GenericOctree.h"
#include "SceneCore.h"
#include "PrimitiveSceneInfo.h"
#include "LightSceneInfo.h"
#include "TessellationRendering.h"
#include "DrawingPolicy.h"
#include "DepthRendering.h"
#include "SceneHitProxyRendering.h"
#include "ShaderComplexityRendering.h"
#include "DepthDependentHaloRendering.h"
#include "FogRendering.h"
#include "FogVolumeRendering.h"
#include "DOFAndBloomEffect.h"
#include "BasePassRendering.h"
#include "ShadowRendering.h"
#include "BranchingPCFShadowRendering.h"
#include "DistortionRendering.h"
#include "SceneRendering.h"
#include "StaticMeshDrawList.h"
#include "DynamicPrimitiveDrawing.h"
#include "TranslucentRendering.h"
#include "VelocityRendering.h"
#include "TextureDensityRendering.h"
#include "LightMapDensityRendering.h"
#include "UnDecalRenderData.h"
#include "HitMaskRendering.h"
#include "UnTextureLayout.h"

/** Factor by which to grow and scale occlusion tests **/
#define OCCLUSION_SLOP (1.1f)
/** Twice the above, used to approximate the full computation **/
#define OCCLUSION_SLOP2 (2.2f)

#if WITH_SLI
/** Holds information about a single primitive's occlusion. */
class FPrimitiveOcclusionHistory
{
public:
	/** The primitive the occlusion information is about. */
	const UPrimitiveComponent* Primitive;

	/** The occlusion query which contains the primitive's pending occlusion results. */
	TArray<FOcclusionQueryRHIRef, TInlineAllocator<1> > PendingOcclusionQuery;

	/** The last time the primitive was visible. */
	FLOAT LastVisibleTime;

	/** The last time the primitive was in the view frustum. */
	FLOAT LastConsideredTime;

	/** 
	 *	The pixels that were rendered the last time the primitive was drawn.
	 *	It is the ratio of pixels unoccluded to the resolution of the scene.
	 */
	FLOAT LastPixelsPercentage;

	/** whether or not this primitive was grouped the last time it was queried */
	UBOOL bGroupedQuery;

	/** 
	 * Number of frames to buffer the occlusion queries. 
	 * Larger numbers allow better SLI scaling but introduce latency in the results.
	 */
	INT NumBufferedFrames;

	/** Initialization constructor. */
	FPrimitiveOcclusionHistory(const UPrimitiveComponent* InPrimitive = NULL):
		Primitive(InPrimitive),
		LastVisibleTime(0.0f),
		LastConsideredTime(0.0f),
		LastPixelsPercentage(0.0f),
		bGroupedQuery(FALSE)
	{
		// If we're running with SLI, assume throughput is more important than latency, and buffer an extra frame
		NumBufferedFrames = GNumActiveGPUsForRendering == 1 ? 1 : GNumActiveGPUsForRendering + 1;
		PendingOcclusionQuery.Empty(NumBufferedFrames);
		PendingOcclusionQuery.AddZeroed(NumBufferedFrames);
	}

	void ReleaseQueries(FOcclusionQueryPool& Pool);

	FOcclusionQueryRHIRef& GetPastQuery(UINT FrameNumber)
	{
		// Get the oldest occlusion query
		const UINT QueryIndex = (FrameNumber - NumBufferedFrames + 1) % NumBufferedFrames;
		return PendingOcclusionQuery(QueryIndex);
	}

	void SetCurrentQuery(UINT FrameNumber, FOcclusionQueryRHIParamRef NewQuery)
	{
		// Get the current occlusion query
		const UINT QueryIndex = FrameNumber % NumBufferedFrames;
		PendingOcclusionQuery(QueryIndex) = NewQuery;
	}

	/** Destructor. Note that the query should have been released already. */
	~FPrimitiveOcclusionHistory()
	{
//		check( !IsValidRef(PendingOcclusionQuery) );
	}
};

#else
/** Holds information about a single primitive's occlusion. */
class FPrimitiveOcclusionHistory
{
public:
	/** The primitive the occlusion information is about. */
	const UPrimitiveComponent* Primitive;

	/** The occlusion query which contains the primitive's pending occlusion results. */
	FOcclusionQueryRHIRef PendingOcclusionQuery;

	/** The last time the primitive was visible. */
	FLOAT LastVisibleTime;

	/** The last time the primitive was in the view frustum. */
	FLOAT LastConsideredTime;

	/** 
	 *	The pixels that were rendered the last time the primitive was drawn.
	 *	It is the ratio of pixels unoccluded to the resolution of the scene.
	 */
	FLOAT LastPixelsPercentage;

	/** whether or not this primitive was grouped the last time it was queried */
	UBOOL bGroupedQuery;

	/** Initialization constructor. */
	FORCEINLINE FPrimitiveOcclusionHistory(const UPrimitiveComponent* InPrimitive = NULL):
		Primitive(InPrimitive),
		LastVisibleTime(0.0f),
		LastConsideredTime(0.0f),
		LastPixelsPercentage(0.0f),
		bGroupedQuery(FALSE)
	{
	}
	template<class TOcclusionQueryPool> // here we use a template just to allow this to be inlined without sorting out the header order
	FORCEINLINE void ReleaseQueries(TOcclusionQueryPool& Pool)
	{
		Pool.ReleaseQuery(PendingOcclusionQuery);
	}

	FORCEINLINE FOcclusionQueryRHIRef& GetPastQuery(UINT)
	{
		return PendingOcclusionQuery;
	}

	FORCEINLINE void SetCurrentQuery(UINT, FOcclusionQueryRHIParamRef NewQuery)
	{
		PendingOcclusionQuery = NewQuery;
	}
};

#endif

/** Defines how the hash set indexes the FPrimitiveOcclusionHistory objects. */
struct FPrimitiveOcclusionHistoryKeyFuncs : BaseKeyFuncs<FPrimitiveOcclusionHistory,const UPrimitiveComponent*>
{
	static KeyInitType GetSetKey(const FPrimitiveOcclusionHistory& Element)
	{
		return Element.Primitive;
	}

	static UBOOL Matches(KeyInitType A,KeyInitType B)
	{
		return A == B;
	}

	static DWORD GetKeyHash(KeyInitType Key)
	{
		return PointerHash(Key);
	}
};


/**
 * A pool of occlusion queries which are allocated individually, and returned to the pool as a group.
 */
class FOcclusionQueryPool
{
public:
	FOcclusionQueryPool()	{ }
	virtual ~FOcclusionQueryPool();

	/** Releases all the occlusion queries in the pool. */
	void Release();

	/** Allocates an occlusion query from the pool. */
	FOcclusionQueryRHIRef AllocateQuery();

	/** De-reference an occlusion query, returning it to the pool instead of deleting it when the refcount reaches 0. */
	void ReleaseQuery( FOcclusionQueryRHIRef &Query );

private:
	/** Container for available occlusion queries. */
	TArray<FOcclusionQueryRHIRef> OcclusionQueries;
};


/**
 * Stores visibility state for a single primitive in a single view
 */
class FSceneViewPrimitiveVisibilityState
{

public:

	/** Currently visible? */
	UBOOL bIsVisible;

	/** LOD index that the primitive last rendered at. */
	SBYTE LODIndex;
};


/** Maps a single primitive to it's per-view visibility state data */
// @todo: Should use TSparseArray for this based on primitives view index
typedef TMap<const UPrimitiveComponent*, FSceneViewPrimitiveVisibilityState> FPrimitiveVisibilityStateMap;



/**
 * Screen door patterns.  This is used to cross-dissolve primitives that overlap while avoiding depth contention.
 */
namespace EScreenDoorPattern
{
	enum Type
	{
		// Screens pixel based on opacity
		Normal = 0,

		// Screens opposite pixels from Normal based on opacity
		Inverse,
	};
}


/**
 * Stores fading state for a single primitive in a single view
 */
class FSceneViewPrimitiveFadingState
{

public:

	/** Index of the LOD fading in. */
	SBYTE FadingInLODIndex;

	/** Index of the LOD fading out. */
	SBYTE FadingOutLODIndex;

	/** Current fade opacity.  Used for fading primitives in and out over time. */
	FLOAT FadeOpacity;

	/** Target fade opacity */
	FLOAT TargetFadeOpacity;

	/** Which dissolve pattern to use when rendering */
	EScreenDoorPattern::Type ScreenDoorPattern;
};


/** Maps a single primitive to it's per-view fading state data */
typedef TMap<const UPrimitiveComponent*, FSceneViewPrimitiveFadingState> FPrimitiveFadingStateMap;

class FOcclusionRandomStream
{
	enum {NumSamples = 3571};
public:

	/** Default constructor - should set seed prior to use. */
	FOcclusionRandomStream()
		: CurrentSample(0)
	{
		FRandomStream RandomStream(0x83246);
		for (INT Index = 0; Index < NumSamples; Index++)
		{
			Samples[Index] = RandomStream.GetFraction();
		}
		Samples[0] = 0.0f; // we want to make sure we have at least a few zeros
		Samples[NumSamples/3] = 0.0f; // we want to make sure we have at least a few zeros
		Samples[(NumSamples*2)/3] = 0.0f; // we want to make sure we have at least a few zeros
	}

	/** @return A random number between 0 and 1. */
	FORCEINLINE FLOAT GetFraction()
	{
		if (CurrentSample >= NumSamples)
		{
			CurrentSample = 0;
		}
		return Samples[CurrentSample++];
	}
private:

	/** Index of the last sample we produced **/
	UINT CurrentSample;
	/** A list of float random samples **/
	FLOAT Samples[NumSamples];
};

/** Random table for occlusion **/
extern FOcclusionRandomStream GOcclusionRandomStream;

/**
 * The scene manager's private implementation of persistent view state.
 * This class is associated with a particular camera across multiple frames by the game thread.
 * The game thread calls AllocateViewState to create an instance of this private implementation.
 */
class FSceneViewState : public FSceneViewStateInterface, public FDeferredCleanupInterface, public FRenderResource
{
public:

    class FProjectedShadowKey
    {
	public:

        FORCEINLINE UBOOL operator == (const FProjectedShadowKey &Other) const
        {
            return (Primitive == Other.Primitive && Light == Other.Light && SplitIndex == Other.SplitIndex);
        }

        FProjectedShadowKey(const UPrimitiveComponent* InPrimitive, const ULightComponent* InLight, INT InSplitIndex):
            Primitive(InPrimitive),
            Light(InLight),
			SplitIndex(InSplitIndex)
        {
		}

		friend FORCEINLINE DWORD GetTypeHash(const FSceneViewState::FProjectedShadowKey& Key)
		{
			return PointerHash(Key.Light,PointerHash(Key.Primitive));
		}

	private:
		const UPrimitiveComponent* Primitive;
        const ULightComponent* Light;
		INT SplitIndex;
    };

    class FPrimitiveComponentKey
    {
	public:

        FORCEINLINE UBOOL operator == (const FPrimitiveComponentKey &Other)
        {
            return (Primitive == Other.Primitive);
        }

        FPrimitiveComponentKey(const UPrimitiveComponent* InPrimitive):
            Primitive(InPrimitive)
        {
        }

		friend FORCEINLINE DWORD GetTypeHash(const FSceneViewState::FPrimitiveComponentKey& Key)
		{
			return PointerHash(Key.Primitive);
		}

	private:
		
        const UPrimitiveComponent* Primitive;
    };

    TMap<FProjectedShadowKey,FOcclusionQueryRHIRef> ShadowOcclusionQueryMap;

	/** The view's occlusion query pool. */
	FOcclusionQueryPool OcclusionQueryPool;

	/** Cached visibility data from the last call to GetPrecomputedVisibilityData. */
	TArray<BYTE>* CachedVisibilityChunk;
	INT CachedVisibilityHandlerId;
	INT CachedVisibilityBucketIndex;
	INT CachedVisibilityChunkIndex;

	/** Parameter to keep track of previous frame. Managed by the rendering thread. */
	FMatrix		PendingPrevTranslatedViewProjectionMatrix;
	FMatrix		PrevTranslatedViewProjectionMatrix;
	FMatrix		PendingPrevProjMatrix;
	FMatrix		PrevProjMatrix;
	FMatrix		PendingPrevViewMatrix;
	FMatrix		PrevViewMatrix;
	FVector		PendingPrevPreViewTranslation;
	FVector		PrevPreViewTranslation;
	FVector		PendingPrevViewOrigin;
	FVector		PrevViewOrigin;
	FLOAT		LastRenderTime;
	FLOAT		LastRenderTimeDelta;
	FLOAT		MotionBlurTimeScale;
	FMatrix		PrevViewMatrixForOcclusionQuery;
	FVector		PrevViewOriginForOcclusionQuery;

	/** Used by states that have IsViewParent() == TRUE to store primitives for child states. */
	TSet<const UPrimitiveComponent*> ParentPrimitives;

	/** For this view, whether each primitive is currently visible or not. */
	FPrimitiveVisibilityStateMap PrimitiveVisibilityStates;

	/** For this view, the set of primitives that are currently fading, either in or out. */
	FPrimitiveFadingStateMap PrimitiveFadingStates;

#if !FINAL_RELEASE
	/** Are we currently in the state of freezing rendering? (1 frame where we gather what was rendered) */
	UBOOL bIsFreezing;

	/** Is rendering currently frozen? */
	UBOOL bIsFrozen;

	/** The set of primitives that were rendered the frame that we froze rendering */
	TSet<const UPrimitiveComponent*> FrozenPrimitives;
#endif

	/** Default constructor. */
    FSceneViewState()
    {
		LastRenderTime = -FLT_MAX;
		LastRenderTimeDelta = 0.0f;
		MotionBlurTimeScale = 1.0f;
		PendingPrevProjMatrix.SetIdentity();
		PrevProjMatrix.SetIdentity();
		PendingPrevViewMatrix.SetIdentity();
		PrevViewMatrix.SetIdentity();
		PendingPrevViewOrigin = FVector(0,0,0);
		PrevViewOrigin = FVector(0,0,0);
		PendingPrevPreViewTranslation = FVector(0,0,0);
		PrevPreViewTranslation = FVector(0,0,0);
		PrevViewMatrixForOcclusionQuery.SetIdentity();
		PrevViewOriginForOcclusionQuery = FVector(0,0,0);
#if !FINAL_RELEASE
		bIsFreezing = FALSE;
		bIsFrozen = FALSE;
#endif

		// Register this object as a resource, so it will receive device reset notifications.
		if ( IsInGameThread() )
		{
			BeginInitResource(this);
		}
		else
		{
			InitResource();
		}
		CachedVisibilityChunk = NULL;
		CachedVisibilityHandlerId = INDEX_NONE;
		CachedVisibilityBucketIndex = INDEX_NONE;
		CachedVisibilityChunkIndex = INDEX_NONE;
    }

	virtual ~FSceneViewState()
	{
		delete CachedVisibilityChunk;
	}

	/** 
	 * Returns an array of visibility data for the given view, or NULL if none exists. 
	 * The data bits are indexed by VisibilityId of each primitive in the scene.
	 * This method decompresses data if necessary and caches it based on the bucket and chunk index in the view state.
	 */
	BYTE* GetPrecomputedVisibilityData(FViewInfo& View, const FScene* Scene);

	/**
	 * Cleans out old entries from the primitive occlusion history, and resets unused pending occlusion queries.
	 * @param MinHistoryTime - The occlusion history for any primitives which have been visible and unoccluded since
	 *							this time will be kept.  The occlusion history for any primitives which haven't been
	 *							visible and unoccluded since this time will be discarded.
	 * @param MinQueryTime - The pending occlusion queries older than this will be discarded.
	 */
	void TrimOcclusionHistory(FLOAT MinHistoryTime,FLOAT MinQueryTime,INT FrameNumber);

	/**
	 * Checks whether a primitive is occluded this frame.  Also updates the occlusion history for the primitive.
	 * @param Primitive - The primitive to perform the occlusion query for; this is a template parameter which should have the following interface:
	 *     UPrimitiveComponent* GetComponent() const
	 *     UBOOL IgnoresNearPlaneIntersection() const
	 *     UBOOL AllowsApproximateOcclusion() const
	 *     UBOOL IsOccludable(FViewInfo& View) const
	 *     FLOAT PixelPercentageOnFirstFrame() const
	 *     FBoxSphereBounds GetOccluderBounds() const
	 *   See FPrimitiveComponentOcclusionWrapper or FCompactPrimitiveSceneInfoOcclusionWrapper for an example.
	 * @param View - The frame's view corresponding to this view state.
	 * @param CurrentRealTime - The current frame's real time.
	 * @param bOutPrimitiveIsDefinitelyUnoccluded - Upon return contains true if the primitive was definitely un-occluded, and not merely
	 *			estimated to be un-occluded.
	 */
	template<typename PrimitiveType>
	UBOOL UpdatePrimitiveOcclusion(
		const PrimitiveType& Primitive,
		FViewInfo& View,
		FLOAT CurrentRealTime,
		UBOOL bIsSceneCapture,
		UBOOL& bOutPrimitiveIsDefinitelyUnoccluded)
	{
		// Only allow primitives that are in the world DPG to be occluded.
		// If SLI is enabled, only allow occlusion queries for primitives which need it to look correct (lens flares).
		// With SLI enabled, occlusion queries for visibility culling leads to a lot of popping due to the buffering required.
		const UBOOL bIsOccludable = Primitive.IsOccludable(View) && (GNumActiveGPUsForRendering == 1 || Primitive.RequiresOcclusionForCorrectness());

		if (View.PrecomputedVisibilityData && bIsOccludable)
		{
		const INT VisibilityId = Primitive.GetVisibilityId();
			if (VisibilityId >= 0)
		{
			const INT ByteIndex = VisibilityId / 8;
			const INT BitIndex = VisibilityId % 8;
			const BYTE VisibilityData = View.PrecomputedVisibilityData[ByteIndex];
			// Check if the primitive is determined occluded from this cell from the precomputed visibility
			if (!(VisibilityData & (1 << BitIndex)))
			{
				if (!bIsSceneCapture)
				{
					INC_DWORD_STAT_BY(STAT_StaticallyOccludedPrimitives,1);
				}
				return TRUE;
			}
		}
		}

		extern UBOOL GIgnoreAllOcclusionQueries;
		if( GIgnoreAllOcclusionQueries )
		{
			bOutPrimitiveIsDefinitelyUnoccluded = TRUE;
			return FALSE;
		}
		UBOOL bIsOccluded = FALSE;

		// Find the primitive's occlusion history.
		UPrimitiveComponent* Component = Primitive.GetComponent();
		FPrimitiveOcclusionHistory* PrimitiveOcclusionHistory = PrimitiveOcclusionHistorySet.Find(Component);
		UBOOL bOcclusionStateIsDefinite = FALSE;
		if (!PrimitiveOcclusionHistory)
		{
			// If the primitive doesn't have an occlusion history yet, create it.
			PrimitiveOcclusionHistory = &PrimitiveOcclusionHistorySet(
				PrimitiveOcclusionHistorySet.Add(FPrimitiveOcclusionHistory(Component))
				);

			// If the primitive hasn't been visible recently enough to have a history, treat it as unoccluded this frame so it will be rendered as an occluder and its true occlusion state can be determined.
			// already set bIsOccluded = FALSE;

			// Flag the primitive's occlusion state as indefinite, which will force it to be queried this frame.
			// The exception is if the primitive isn't occludable, in which case we know that it's definitely unoccluded.
			bOcclusionStateIsDefinite = bIsOccludable ? FALSE : TRUE;
		}
		else
		{
			if (View.bIgnoreExistingQueries)
			{
				// If the view is ignoring occlusion queries, the primitive is definitely unoccluded.
				// already set bIsOccluded = FALSE;
				bOcclusionStateIsDefinite = View.bDisableQuerySubmissions;
			}
			else if (bIsOccludable)
			{
				// Read the occlusion query results.
				DWORD NumPixels = 0;
				FOcclusionQueryRHIRef& PastQuery = PrimitiveOcclusionHistory->GetPastQuery(View.FrameNumber);
				if (IsValidRef(PastQuery))
				{
					// NOTE: RHIGetOcclusionQueryResult should never fail when using a blocking call, rendering artifacts may show up.
					if ( RHIGetOcclusionQueryResult(PastQuery,NumPixels,TRUE) )
					{
						// The primitive is occluded if none of its bounding box's pixels were visible in the previous frame's occlusion query.
						bIsOccluded = (NumPixels == 0);
						if (!bIsOccluded)
						{
							checkSlow(View.OneOverNumPossiblePixels > 0.0f);
							PrimitiveOcclusionHistory->LastPixelsPercentage = NumPixels * View.OneOverNumPossiblePixels;
						}
						else
						{
							PrimitiveOcclusionHistory->LastPixelsPercentage = 0.0f;
						}


						// Flag the primitive's occlusion state as definite if it wasn't grouped.
						bOcclusionStateIsDefinite = !PrimitiveOcclusionHistory->bGroupedQuery;
					}
					else
					{
						// If the occlusion query failed, treat the primitive as visible.  
						// already set bIsOccluded = FALSE;
					}
				}
				else
				{
					// If there's no occlusion query for the primitive, set it's visibility state to whether it has been unoccluded recently.
					bIsOccluded = (PrimitiveOcclusionHistory->LastVisibleTime + GEngine->PrimitiveProbablyVisibleTime < CurrentRealTime);
					if (bIsOccluded)
					{
						PrimitiveOcclusionHistory->LastPixelsPercentage = 0.0f;
					}
					else
					{
						PrimitiveOcclusionHistory->LastPixelsPercentage = Primitive.PixelPercentageOnFirstFrame();
					}

					// the state was definite last frame, otherwise we would have ran a query
					bOcclusionStateIsDefinite = TRUE;
				}
			}
			else
			{
				// Primitives that aren't occludable are considered definitely unoccluded.
				// already set bIsOccluded = FALSE;
				bOcclusionStateIsDefinite = TRUE;
			}

			// Clear the primitive's pending occlusion query.ONLY when doing normal renders (not hit proxies)
			UBOOL bHitProxies = ((View.Family->ShowFlags & SHOW_HitProxies) != 0);
			if (!bHitProxies)
			{
				OcclusionQueryPool.ReleaseQuery( PrimitiveOcclusionHistory->GetPastQuery(View.FrameNumber) );
			}
		}

		// Set the primitive's considered time to keep its occlusion history from being trimmed.
		PrimitiveOcclusionHistory->LastConsideredTime = CurrentRealTime;

		// Enqueue the next frame's occlusion query for the primitive.
		if (!View.bDisableQuerySubmissions
			&& bIsOccludable
#if !FINAL_RELEASE
			&& !HasViewParent() && !bIsFrozen
#endif
			)
		{
			// If the primitive is ignoring the near-plane check, we must check whether the camera is inside the occlusion box.
			UBOOL bAllowBoundsTest;
			if (View.bHasNearClippingPlane)
			{
				if (Primitive.IgnoresNearPlaneIntersection())
			{
				// Disallow occlusion bound testing if the camera is inside, since we're not doing double-sided testing.
					// this is a slow path
					bAllowBoundsTest = !Primitive.GetOccluderBounds().GetBox().IsInside(View.ViewOrigin);
				}
				else
				{
					bAllowBoundsTest = View.NearClippingPlane.PlaneDot(Primitive.GetOccluderBounds().Origin) < 
						-(FBoxPushOut(View.NearClippingPlane,Primitive.GetOccluderBounds().BoxExtent));
				}
			}
			else
			{
				bAllowBoundsTest = Primitive.GetOccluderBounds().SphereRadius < HALF_WORLD_MAX;
			}

			if ( bAllowBoundsTest )
			{
				// decide if a query should be run this frame
				UBOOL bRunQuery,bGroupedQuery;
				if (Primitive.AllowsApproximateOcclusion())
				{
					if (bIsOccluded)
					{
						// Primitives that were occluded the previous frame use grouped queries.
						bGroupedQuery = TRUE;
						bRunQuery = TRUE;
					}
					else if (bOcclusionStateIsDefinite)
					{
						// If the primitive's is definitely unoccluded, only requery it occasionally.
						FLOAT FractionMultiplier = Max(PrimitiveOcclusionHistory->LastPixelsPercentage/GEngine->MaxOcclusionPixelsFraction, 1.0f);
						bRunQuery = (FractionMultiplier * GOcclusionRandomStream.GetFraction() < GEngine->MaxOcclusionPixelsFraction);
						bGroupedQuery = FALSE;
					}
					else
					{
						bGroupedQuery = FALSE;
						bRunQuery = TRUE;
					}
				}
				else
				{
					// Primitives that need precise occlusion results use individual queries.
					bGroupedQuery = FALSE;
					bRunQuery = TRUE;
				}

				// Don't actually queue the primitive's occlusion query in wireframe, since it will not be submitted.
				if (bRunQuery && !(View.Family->ShowFlags & SHOW_Wireframe))
				{
					PrimitiveOcclusionHistory->SetCurrentQuery(View.FrameNumber, 
						bGroupedQuery ? 
							View.GroupedOcclusionQueries.BatchPrimitive(Primitive.GetOccluderBounds().Origin + View.PreViewTranslation,Primitive.GetOccluderBounds().BoxExtent) :
							View.IndividualOcclusionQueries.BatchPrimitive(Primitive.GetOccluderBounds().Origin + View.PreViewTranslation,Primitive.GetOccluderBounds().BoxExtent)
						);
				}
				PrimitiveOcclusionHistory->bGroupedQuery = bGroupedQuery;
			}
			else
			{
				// If the primitive's bounding box intersects the near clipping plane, treat it as definitely unoccluded.
				bIsOccluded = FALSE;
				bOcclusionStateIsDefinite = TRUE;
			}
		}

		if((bOcclusionStateIsDefinite 
#if !FINAL_RELEASE
			|| bIsFrozen
#endif
			) && !bIsOccluded)
		{
			// Update the primitive's visibility time in the occlusion history.
			// This is only done when we have definite occlusion results for the primitive.
			PrimitiveOcclusionHistory->LastVisibleTime = CurrentRealTime;

			bOutPrimitiveIsDefinitelyUnoccluded = TRUE;
		}
		else
		{
			bOutPrimitiveIsDefinitelyUnoccluded = FALSE;
		}

		return bIsOccluded;
	}

	/**
	 * Checks whether a shadow is occluded this frame.
	 * @param Primitive - The shadow subject.
	 * @param Light - The shadow source.
	 */
	UBOOL IsShadowOccluded(const UPrimitiveComponent* Primitive, const ULightComponent* Light, INT SplitIndex) const;

	/**
	 *	Retrieves the percentage of the views render target the primitive touched last time it was rendered.
	 *
	 *	@param	Primitive				The primitive of interest.
	 *	@param	OutCoveragePercentage	REFERENCE: The screen coverate percentage. (OUTPUT)
	 *	@return	UBOOL					TRUE if the primitive was found and the results are valid, FALSE is not
	 */
	UBOOL GetPrimitiveCoveragePercentage(const UPrimitiveComponent* Primitive, FLOAT& OutCoveragePercentage);
	
	/**
	 * Returns the current fade opacity for the specified primitive in this view.  This value will change
	 * frame to frame.  For primitives that aren't currently visible (fade-wise), zero will be returned
	 *
	 * @param	PrimitiveComponent		Component to test (pointer not dereferenced by this method.)
	 * @param	OutScreenDoorPattern	Screen door pattern to use when rendering
	 *
	 * @return	Current fade opacity (0.0 - 1.0)
	 */
	FLOAT GetPrimitiveFadeOpacity( const UPrimitiveComponent* PrimitiveComponent, SBYTE LODIndex, EScreenDoorPattern::Type& OutScreenDoorPattern ) const
	{
		checkSlow( PrimitiveComponent != NULL );

		FLOAT FadeOpacity = 1.0f;
		OutScreenDoorPattern = EScreenDoorPattern::Normal;

		// Grab the view-specific fading state for this component.  If we're not fading yet, we may
		// not even have any state to grab.
		const FSceneViewPrimitiveFadingState* FadingState = PrimitiveFadingStates.Find( PrimitiveComponent );
		if( FadingState != NULL )
		{
			if (LODIndex == INDEX_NONE || FadingState->FadingOutLODIndex == INDEX_NONE)
			{
				FadeOpacity = FadingState->FadeOpacity;
				OutScreenDoorPattern = FadingState->ScreenDoorPattern;
			}
			else
			{
				// Invert the opacity for the LOD fading in
				FadeOpacity = LODIndex == FadingState->FadingOutLODIndex ? FadingState->FadeOpacity : 1.0f - FadingState->FadeOpacity;
				// Alternate the pattern for odd and even LODs
				OutScreenDoorPattern = ( EScreenDoorPattern::Type )( LODIndex % 2 );
			}
		}

		return FadeOpacity;
	}

	/**
	 * Returns true if the primitive is currently fading in or out and has special shading considerations
	 *
	 * @param	PrimitiveComponent	Component to test (pointer not dereferenced by this method.)
	 *
	 * @return	TRUE if the primitive is currently fading, otherwise FALSE
	 */
	UBOOL IsPrimitiveFading( const UPrimitiveComponent* PrimitiveComponent ) const
	{
		checkSlow( PrimitiveComponent != NULL );

		// If we have a valid fading state then we're currently fading
		const FSceneViewPrimitiveFadingState* FadingState = PrimitiveFadingStates.Find( PrimitiveComponent );
		return ( FadingState != NULL );
	}

	UBOOL IsPrimitiveFading( const UPrimitiveComponent* PrimitiveComponent, INT& FadingInLODIndex, INT& FadingOutLODIndex ) const
	{
		checkSlow( PrimitiveComponent != NULL );

		// If we have a valid fading state then we're currently fading
		const FSceneViewPrimitiveFadingState* FadingState = PrimitiveFadingStates.Find( PrimitiveComponent );
		FadingInLODIndex = FadingState ? FadingState->FadingInLODIndex : INDEX_NONE;
		FadingOutLODIndex = FadingState ? FadingState->FadingOutLODIndex : INDEX_NONE;
		return ( FadingState != NULL );
	}

    // FRenderResource interface.
    virtual void ReleaseDynamicRHI()
    {
        ShadowOcclusionQueryMap.Reset();
		PrimitiveOcclusionHistorySet.Empty();
		OcclusionQueryPool.Release();
    }

	// FSceneViewStateInterface
	virtual void Destroy()
	{
		if ( IsInGameThread() )
		{
			// Release the occlusion query data.
			BeginReleaseResource(this);

			// Defer deletion of the view state until the rendering thread is done with it.
			BeginCleanup(this);
		}
		else
		{
			ReleaseResource();
			FinishCleanup();
		}
	}
	
	// FDeferredCleanupInterface
	virtual void FinishCleanup()
	{
		delete this;
	}

	virtual SIZE_T GetSizeBytes() const;

private:

	/** Information about visibility/occlusion states in past frames for individual primitives. */
	TSet<FPrimitiveOcclusionHistory,FPrimitiveOcclusionHistoryKeyFuncs> PrimitiveOcclusionHistorySet;
};

class FDepthPriorityGroup
{
public:

	enum EBasePassDrawListType
	{
		EBasePass_Default=0,
		EBasePass_Masked,
		EBasePass_Decals,
		EBasePass_Decals_Translucent,
		EBasePass_MAX
	};

	// various static draw lists for this DPG

	/** position-only opaque depth draw list */
	TStaticMeshDrawList<FPositionOnlyDepthDrawingPolicy> PositionOnlyDepthDrawList;
	/** opaque depth draw list */
	TStaticMeshDrawList<FDepthDrawingPolicy> DepthDrawList;
	/** masked depth draw list */
	TStaticMeshDrawList<FDepthDrawingPolicy> MaskedDepthDrawList;
	/** soft masked depth draw list */
	TStaticMeshDrawList<FDepthDrawingPolicy> SoftMaskedDepthDrawList;
	/** Base pass draw list - no light map */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FNoLightMapPolicy,FNoDensityPolicy> > BasePassNoLightMapDrawList[EBasePass_MAX];
	/** Base pass draw list - directional vertex light maps */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FDirectionalVertexLightMapPolicy,FNoDensityPolicy> > BasePassDirectionalVertexLightMapDrawList[EBasePass_MAX];
	/** Base pass draw list - simple vertex light maps */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FSimpleVertexLightMapPolicy,FNoDensityPolicy> > BasePassSimpleVertexLightMapDrawList[EBasePass_MAX];
	/** Base pass draw list - directional texture light maps */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FDirectionalLightMapTexturePolicy,FNoDensityPolicy> > BasePassDirectionalLightMapTextureDrawList[EBasePass_MAX];
	/** Base pass draw list - simple texture light maps */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FSimpleLightMapTexturePolicy,FNoDensityPolicy> > BasePassSimpleLightMapTextureDrawList[EBasePass_MAX];
	/** Base pass draw list - directional light only */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FDirectionalLightLightMapPolicy,FNoDensityPolicy> > BasePassDirectionalLightDrawList[EBasePass_MAX];
	/** Base pass draw list - SH light + directional light */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FSHLightLightMapPolicy,FNoDensityPolicy> > BasePassSHLightDrawList[EBasePass_MAX];
	/** Base pass draw list - dynamically shadowed point, spot or directional light */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FDynamicallyShadowedMultiTypeLightLightMapPolicy,FNoDensityPolicy> > BasePassDynamicallyShadowedDynamicLightDrawList[EBasePass_MAX];
	/** Base pass draw list - SH light + dynamically shadowed point, spot or directional light */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FSHLightAndMultiTypeLightMapPolicy,FNoDensityPolicy> > BasePassSHLightAndDynamicLightDrawList[EBasePass_MAX];
	/** Base pass draw list - vertex shadow mapped dynamic light + directional vertex light maps */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FShadowedDynamicLightDirectionalVertexLightMapPolicy,FNoDensityPolicy> > BasePassShadowedDynamicLightDirectionalVertexLightMapDrawList[EBasePass_MAX];
	/** Base pass draw list - texture shadow mapped dynamic light + directional texture light maps */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FShadowedDynamicLightDirectionalLightMapTexturePolicy,FNoDensityPolicy> > BasePassShadowedDynamicLightDirectionalLightMapTextureDrawList[EBasePass_MAX];
	/** Base pass draw list - texture distance field shadowed dynamic light + directional texture light maps */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicy,FNoDensityPolicy> > BasePassDistanceFieldShadowedDynamicLightDirectionalLightMapTextureDrawList[EBasePass_MAX];

	/** hit proxy draw list (includes both opaque and translucent objects) */
	TStaticMeshDrawList<FHitProxyDrawingPolicy> HitProxyDrawList;

	/** hit proxy draw list, with only opaque objects */
	TStaticMeshDrawList<FHitProxyDrawingPolicy> HitProxyDrawList_OpaqueOnly;

	/** draw list for motion blur velocities */
	TStaticMeshDrawList<FVelocityDrawingPolicy> VelocityDrawList;

	/** Draw list used for rendering whole scene shadow depths. */
	TStaticMeshDrawList<FShadowDepthDrawingPolicy> WholeSceneShadowDepthDrawList;

	/** Maps a light-map type to the appropriate base pass draw list. */
	template<typename LightMapPolicyType>
	TStaticMeshDrawList<TBasePassDrawingPolicy<LightMapPolicyType,FNoDensityPolicy> >& GetBasePassDrawList(EBasePassDrawListType DrawType);
};

/** Information about the primitives and lights in a light environment. */
class FLightEnvironmentSceneInfo
{
public:

	/** The primitives in the light environment. */
	TArray<FPrimitiveSceneInfo*> Primitives;

	/** The lights in the light environment. */
	TArray<FLightSceneInfo*,TInlineAllocator<3> > Lights;
};

/** Information about the primitives in a shadow group. */
class FShadowGroupSceneInfo
{
public:

	/** The primitives in the shadow group. */
	TArray<FPrimitiveSceneInfo*> Primitives;
};

/** 
 *	Helper structure for setting up portal scene capture information
 */
struct FSceneCaptureViewInfoData
{
	FVector PlayerViewOrigin;
	FLOAT PlayerViewScreenSize;
	FLOAT PlayerViewFOVScreenSize;
};

class FScene : public FSceneInterface
{
public:

	/** An optional world associated with the level. */
	UWorld* World;

	/** The draw lists for the scene, sorted by DPG and translucency layer. */
	FDepthPriorityGroup DPGs[SDPG_MAX_SceneRender];

	/** The primitives in the scene. */
	TSparseArray<FPrimitiveSceneInfo*> Primitives;

	/** The scene capture probes for rendering the scene to texture targets */
	TSparseArray<FCaptureSceneInfo*> SceneCapturesRenderThread;

	/** Game thread copy of scene capture probes for rendering the scene to texture targets */
	TSparseArray<FCaptureSceneInfo*> SceneCapturesGameThread;

	/** Game thread container for storing all attached fluidsurfaces. */
	TArray<UFluidSurfaceComponent*> FluidSurfacesGameThread;

	/** Rendering thread list of active fluid surfaces. */
	TMap<const UFluidSurfaceComponent*, const class FFluidGPUResource*> FluidSurfacesRenderThread;

	/** The lights in the scene. */
	TSparseArray<FLightSceneInfoCompact> Lights;

	/** The static meshes in the scene. */
	TSparseArray<FStaticMesh*> StaticMeshes;

	/** The decal static meshes in the scene. Added by each FDecalInteraction during attachment */
	TSparseArray<FStaticMesh*> DecalStaticMeshes;

	/** The fog components in the scene. */
	TArray<FHeightFogSceneInfo> Fogs;

	/** The exponential fog components in the scene. */
	TArray<FExponentialHeightFogSceneInfo> ExponentialFogs;

	/** The wind sources in the scene. */
	TArray<class FWindSourceSceneProxy*> WindSources;

	/** Maps a primitive component to the fog volume density information associated with it. */
	TMap<const UPrimitiveComponent*, FFogVolumeDensitySceneInfo*> FogVolumes;

	/** Image based reflections in the scene. */
	TMap<const UActorComponent*, FImageReflectionSceneInfo*> ImageReflections;

	/** Image reflection shadow planes in the scene. */
	TMap<const UActorComponent*, FPlane> ImageReflectionShadowPlanes;

	/** Scene maintained texture array that contains all the image reflection textures. */
	FTexture2DArrayResource ImageReflectionTextureArray;

	/** Texture that represents the environment when rendering image reflections. */
	const UTexture2D* ImageReflectionEnvironmentTexture;

	/** Color to multiply against ImageReflectionEnvironmentTexture. */
	FVector EnvironmentColor;

	/** Angle to rotate the environment around the Z axis, in degrees. */
	FLOAT EnvironmentRotation;

	/** The light environments in the scene. */
	TMap<const ULightEnvironmentComponent*,FLightEnvironmentSceneInfo> LightEnvironments;

	/** The shadow groups in the scene.  The map key is the shadow group's parent primitive. */
	TMap<const UPrimitiveComponent*,FShadowGroupSceneInfo> ShadowGroups;

	/** The radial blur proxies created for the scene mapped to their components */
	TMap<const class URadialBlurComponent*, FRadialBlurSceneProxy*> RadialBlurInfos;

	/** Precomputed visibility data for the scene. */
	const FPrecomputedVisibilityHandler* PrecomputedVisibilityHandler;

	/** Distance field volume texture for the scene, used for shadowing by image based reflections. */
	FVolumeTextureResource PrecomputedDistanceFieldVolumeTexture;

	/** Maximum world space distance stored in PrecomputedDistanceFieldVolumeTexture. */
	FLOAT VolumeDistanceFieldMaxDistance;

	/** World space bounding box of PrecomputedDistanceFieldVolumeTexture. */
	FBox VolumeDistanceFieldBox;

	/** Preshadows that are currently cached in the PreshadowCache render target. */
	TArray<TRefCountPtr<FProjectedShadowInfo> > CachedPreshadows;

	/** Texture layout that tracks current allocations in the PreshadowCache render target. */
	FTextureLayout PreshadowCacheLayout;

	/** An octree containing the lights in the scene. */
	FSceneLightOctree LightOctree;

	/** An octree containing the primitives in the scene. */
	FScenePrimitiveOctree PrimitiveOctree;

	/** Indicates this scene always allows audio playback. */
	UBOOL bAlwaysAllowAudioPlayback;

	/** Indicates whether this scene requires hit proxy rendering. */
	UBOOL bRequiresHitProxies;

	/** Set by the rendering thread to signal to the game thread that the scene needs a static lighting build. */
	volatile mutable INT NumUncachedStaticLightingInteractions;

	/** Set by the rendering thread to signal to the game thread when there are errors from multiple dominant lights affecting one primitive. */
	volatile mutable INT NumMultipleDominantLightInteractions;

	/** Number of lights casting whole scene shadows. */
	INT NumWholeSceneShadowLights;

	/** Number of directional lights in the scene that use lighting functions */
	INT NumDirectionalLightFunctions;

	/** The skylight color used on statically lit primitives that are unbuilt. */
	FLinearColor PreviewSkyLightColor;

	/** Default constructor. */
	FScene()
	:	ImageReflectionEnvironmentTexture(NULL)
	,	EnvironmentColor(FVector(1,1,1))
	,	EnvironmentRotation(0)
	,	PrecomputedVisibilityHandler(NULL)
	,	PreshadowCacheLayout(0, 0, 0, 0, FALSE, FALSE)
	,	LightOctree(FVector(0,0,0),HALF_WORLD_MAX)
	,	PrimitiveOctree(FVector(0,0,0),HALF_WORLD_MAX)
	,	NumUncachedStaticLightingInteractions(0)
	,	NumMultipleDominantLightInteractions(0)
	,	NumWholeSceneShadowLights(0)
	,	NumDirectionalLightFunctions(0)
	,	PreviewSkyLightColor(FLinearColor::Black)
	{}

	~FScene()
	{
		PrecomputedDistanceFieldVolumeTexture.ReleaseResource();
		ImageReflectionTextureArray.ReleaseResource();
	}

	// FSceneInterface interface.
	virtual void AddPrimitive(UPrimitiveComponent* Primitive);
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive, UBOOL bWillReattach);
	virtual void UpdatePrimitiveTransform(UPrimitiveComponent* Primitive);
	virtual void UpdatePrimitiveAffectingDominantLight(UPrimitiveComponent* Primitive, ULightComponent* NewAffectingDominantLight);
	virtual void AddLight(ULightComponent* Light);
	virtual void RemoveLight(ULightComponent* Light);
	virtual void UpdateLightTransform(ULightComponent* Light);
	virtual void UpdateLightColorAndBrightness(ULightComponent* Light);
	virtual void UpdatePreviewSkyLightColor(const FLinearColor& NewColor);
	virtual void AddHeightFog(UHeightFogComponent* FogComponent);
	virtual void RemoveHeightFog(UHeightFogComponent* FogComponent);
	virtual void AddExponentialHeightFog(UExponentialHeightFogComponent* FogComponent);
	virtual void RemoveExponentialHeightFog(UExponentialHeightFogComponent* FogComponent);
	virtual void AddWindSource(UWindDirectionalSourceComponent* WindComponent);
	virtual void RemoveWindSource(UWindDirectionalSourceComponent* WindComponent);
	virtual const TArray<FWindSourceSceneProxy*>& GetWindSources_RenderThread() const;
	virtual FVector4 GetWindParameters(const FVector& Position) const;
	virtual void DumpLightIteractions( FOutputDevice& Ar ) const;

	/**
	 * Creates a unique radial blur proxy for the given component and adds it to the scene.
	 *
	 * @param RadialBlurComponent - component being added to the scene
	 */
	virtual void AddRadialBlur(class URadialBlurComponent* RadialBlurComponent);
	/**
	 * Removes the radial blur proxy for the given component from the scene.
	 *
	 * @param RadialBlurComponent - component being added to the scene
	 */
	virtual void RemoveRadialBlur(class URadialBlurComponent* RadialBlurComponent);

	/**
	 * Adds a default FFogVolumeConstantDensitySceneInfo to UPrimitiveComponent pair to the Scene's FogVolumes map.
	 */
	virtual void AddFogVolume(const UPrimitiveComponent* VolumeComponent);

	/**
	 * Adds a FFogVolumeDensitySceneInfo to UPrimitiveComponent pair to the Scene's FogVolumes map.
	 */
	virtual void AddFogVolume(const UFogVolumeDensityComponent* FogVolumeComponent, const UPrimitiveComponent* MeshComponent);

	/**
	 * Removes an entry by UPrimitiveComponent from the Scene's FogVolumes map.
	 */
	virtual void RemoveFogVolume(const UPrimitiveComponent* VolumeComponent);

	/** Adds an image based reflection component to the scene. */
	virtual void AddImageReflection(const UActorComponent* Component, UTexture2D* InReflectionTexture, FLOAT ReflectionScale, const FLinearColor& InReflectionColor, UBOOL bInTwoSided, UBOOL bEnabled);

	/** Updates an image based reflection's transform. */
	virtual void UpdateImageReflection(const UActorComponent* Component, UTexture2D* InReflectionTexture, FLOAT ReflectionScale, const FLinearColor& InReflectionColor, UBOOL bInTwoSided, UBOOL bEnabled);

	/** Updates the image reflection texture array. */
	virtual void UpdateImageReflectionTextureArray(UTexture2D* Texture);

	/** Removes an image based reflection component from the scene. */
	virtual void RemoveImageReflection(const UActorComponent* Component);

	/** Adds an image reflection shadow plane component to the scene. */
	virtual void AddImageReflectionShadowPlane(const UActorComponent* Component, const FPlane& InPlane);

	/** Removes an image reflection shadow plane component from the scene. */
	virtual void RemoveImageReflectionShadowPlane(const UActorComponent* Component);

	/** Sets scene image reflection environment parameters. */
	virtual void SetImageReflectionEnvironmentTexture(const UTexture2D* NewTexture, const FLinearColor& Color, FLOAT Rotation);

	/** Sets the scene in a state where it will not reallocate the image reflection texture array. */
	virtual void BeginPreventIRReallocation();

	/** Restores the scene's default state where it will reallocate the image reflection texture array as needed. */
	virtual void EndPreventIRReallocation();

	/**
	 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
	 *
	 * @param	Primitive				Primitive to retrieve interacting lights for
	 * @param	RelevantLights	[out]	Array of lights interacting with primitive
	 */
	virtual void GetRelevantLights( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const;

	/**
	 * Create the scene capture info for a capture component and add it to the scene
	 * @param CaptureComponent - component to add to the scene 
	 */
	virtual void AddSceneCapture(USceneCaptureComponent* CaptureComponent);

	/**
	 * Remove the scene capture info for a capture component from the scene
	 * @param CaptureComponent - component to remove from the scene 
	 */
	virtual void RemoveSceneCapture(USceneCaptureComponent* CaptureComponent);

	/**
	 * Adds a fluidsurface to the scene (gamethread)
	 * @param FluidComponent - component to add to the scene 
	 */
	virtual void AddFluidSurface(UFluidSurfaceComponent* FluidComponent);

	/**
	 * Removes a fluidsurface from the scene (gamethread)
	 * @param CaptureComponent - component to remove from the scene 
	 */
	virtual void RemoveFluidSurface(UFluidSurfaceComponent* FluidComponent);

	/**
	 * Retrieves a pointer to the fluidsurface container.
	 * @return TArray pointer, or NULL if the scene doesn't support fluidsurfaces.
	 **/
	virtual const TArray<UFluidSurfaceComponent*>* GetFluidSurfaces();

	/** Returns a pointer to the first fluid surface detail normal texture in the scene. */
	virtual const FTexture2DRHIRef* GetFluidDetailNormal() const;

	/** Sets the precomputed visibility handler for the scene, or NULL to clear the current one. */
	virtual void SetPrecomputedVisibility(const FPrecomputedVisibilityHandler* PrecomputedVisibilityHandler);

	/** Sets the precomputed volume distance field for the scene, or NULL to clear the current one. */
	virtual void SetPrecomputedVolumeDistanceField(const class FPrecomputedVolumeDistanceField* PrecomputedVolumeDistanceField);

	virtual void Release();
	virtual UWorld* GetWorld() const { return World; }

	/** Indicates if sounds in this should be allowed to play. */
	virtual UBOOL AllowAudioPlayback();

	/**
	 * @return		TRUE if hit proxies should be rendered in this scene.
	 */
	virtual UBOOL RequiresHitProxies() const;

	/**
	 * Accesses the scene info for a light environment.
	 * @param	LightEnvironment - The light environment to access scene info for.
	 * @return	The scene info for the light environment.
	 */
	FLightEnvironmentSceneInfo& GetLightEnvironmentSceneInfo(const ULightEnvironmentComponent* LightEnvironment);

	SIZE_T GetSizeBytes() const;

	/**
	* Return the scene to be used for rendering
	*/
	virtual class FScene* GetRenderScene()
	{
		return this;
	}

	/** 
	 *	Set the primitives motion blur info
	 * 
	 *	@param PrimitiveSceneInfo	The primitive to add
	 *	@param bRemoving			TRUE if the primitive is being removed
	 *  @param FrameNumber			is only used if !bRemoving
	 */
	static void UpdatePrimitiveMotionBlur(FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMatrix& LocalToWorld, UBOOL bRemoving);

	/**
	 * Creates any needed motion blur infos if needed and saves the transforms of the frame we just completed
	 */
	static void UpdateMotionBlurCache();

	/**
	 *	Clear out the motion blur info.
	 */
	static void ClearMotionBlurInfo();

	/**
	 * Reset all the dirty bits for motion blur so they can be updated for the next frame
	 */
	static void ResetMotionBlurInfoDirty();

	/**
	 * Remove motion blur infos for primitives that were not updated the last frame
	 */
	static void ClearStaleMotionBlurInfo();

	/**
	 * Clear an entry in the motion blur info array
	 *
	 * @param MBInfoIndex index of motion blur entry to clear
	 */
	static void ClearMotionBlurInfoIndex(INT MBInfoIndex);

	/** 
	 *	Get the primitives motion blur info
	 * 
	 *	@param	PrimitiveSceneInfo	The primitive to retrieve the motion blur info for
	 *
	 *	@return	UBOOL				TRUE if the primitive info was found and set
	 */
	static UBOOL GetPrimitiveMotionBlurInfo(const FPrimitiveSceneInfo* PrimitiveSceneInfo, FMatrix& OutPreviousLocalToWorld, const FMotionBlurParams& MotionBlurParams);

	/**
	 *	Add the scene captures view info to the streaming manager
	 *
	 *	@param	StreamingManager			The streaming manager to add the view info to.
	 *	@param	View						The scene view information
	 */
	virtual void AddSceneCaptureViewInformation(FStreamingManagerCollection* StreamingManager, FSceneView* View);

	/**
	*	Clears Hit Mask when component is detached
	*
	*	@param	ComponentToDetach		SkeletalMeshComponent To Detach
	*/
	virtual void ClearHitMask(const UPrimitiveComponent* ComponentToDetach);

	/**
	*	Update Hit Mask when component is gets reattached
	*
	*	@param	ComponentToUpdate		SkeletalMeshComponent To Update
	*/
	virtual void UpdateHitMask(const UPrimitiveComponent* ComponentToUpdate);

	/**
	 * Dumps dynamic lighting and shadow interactions for scene to log.
	 *
	 * @param	bOnlyIncludeShadowCastingInteractions	Whether to only include shadow casting interactions
	 */
	virtual void DumpDynamicLightShadowInteractions( UBOOL bOnlyIncludeShadowCastingInteractions ) const;

	/**
	 * Exports the scene.
	 *
	 * @param	Ar		The Archive used for exporting.
	 **/
	virtual void Export( FArchive& Ar ) const;

private:

	/**
	 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
	 * Render thread version of function.
	 * @param	Primitive				Primitive to retrieve interacting lights for
	 * @param	RelevantLights	[out]	Array of lights interacting with primitive
	 */
	void GetRelevantLights_RenderThread( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const;

	/**
	 * Adds a primitive to the scene.  Called in the rendering thread by AddPrimitive.
	 * @param PrimitiveSceneInfo - The primitive being added.
	 */
	void AddPrimitiveSceneInfo_RenderThread(FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/**
	 * Removes a primitive from the scene.  Called in the rendering thread by RemovePrimitive.
	 * @param PrimitiveSceneInfo - The primitive being removed.
	 */
	void RemovePrimitiveSceneInfo_RenderThread(FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/**
	 * Adds a light to the scene.  Called in the rendering thread by AddLight.
	 * @param LightSceneInfo - The light being added.
	 */
	void AddLightSceneInfo_RenderThread(FLightSceneInfo* LightSceneInfo);

	/**
	 * Removes a light from the scene.  Called in the rendering thread by RemoveLight.
	 * @param LightSceneInfo - The light being removed.
	 */
	void RemoveLightSceneInfo_RenderThread(FLightSceneInfo* LightSceneInfo);

	void UpdateLightTransform_RenderThread(FLightSceneInfo* LightSceneInfo, const struct FUpdateLightTransformParameters& Parameters);

	/**
	 * Dumps dynamic lighting and shadow interactions for scene to log.
	 *
	 * @param	bOnlyIncludeShadowCastingInteractions	Whether to only include shadow casting interactions
	 */
	void DumpDynamicLightShadowInteractions_RenderThread( UBOOL bOnlyIncludeShadowCastingInteractions ) const;

	/** The motion blur info entries for the frame. Accessed on Renderthread only! */
	static TArray<FMotionBlurInfo> MotionBlurInfoArray;
	/** Stack of indices for available entries in MotionBlurInfoArray. Accessed on Renderthread only! */
	static TArray<INT> MotionBlurFreeEntries;
	/** Array of components that we need to update at the end of the frame */
	static TArray<FPrimitiveSceneInfo*> PrimitiveSceneInfosToUpdateAtFrameEnd;
	/** Unique "frame number" counter to make sure we don't double update */
	static UINT CacheUpdateCount;
};

#endif // __SCENEPRIVATE_H__
