/*=============================================================================
	SceneRendering.h: Scene rendering definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/** This define control whether we measure distance to center of bounds, or to surface of bounding box, when evaluating Min/MaxDrawDistance and MassiveLODDistance */
#define CULL_DISTANCE_TO_BOUNDS (1)

/**
* Calculates the distance squared from a bounds object to a point (to the surface of the bounds if CULL_DISTANCE_TO_BOUNDS != 0, otherwise to the center)
* @param Bounds - Bounds to calc distance to
* @param Point - Point calculating distance from
* @return Distance squared
*/
inline FLOAT CalculateDistanceSquaredForLOD(const FBoxSphereBounds& Bounds, const FVector4& Point)
{
#if CULL_DISTANCE_TO_BOUNDS
	return Bounds.GetBox().ComputeSquaredDistanceToPoint(Point);
#else
	return (Bounds.Origin - Point).SizeSquared();
#endif
}

/**
* Calculates the distance squared from a bounds object to a point (to the surface of the bounds if CULL_DISTANCE_TO_BOUNDS != 0, otherwise to the center)
* @param Bounds - Bounds to calc distance to
* @param Point - Point calculating distance from
* @return Distance squared
*/
inline FLOAT CalculateDistanceSquaredForLOD(const FBoxSphereBounds& Bounds, const FVector& Point)
{
#if CULL_DISTANCE_TO_BOUNDS
	return Bounds.GetBox().ComputeSquaredDistanceToPoint(Point);
#else
	return (Bounds.Origin - Point).SizeSquared();
#endif
}

/** An association between a hit proxy and a mesh. */
class FHitProxyMeshPair : public FMeshBatch
{
public:
	FHitProxyId HitProxyId;

	/** Initialization constructor. */
	FHitProxyMeshPair(const FMeshBatch& InMesh,FHitProxyId InHitProxyId):
		FMeshBatch(InMesh),
		HitProxyId(InHitProxyId)
	{}
};

/** Information about a visible light which is specific to the view it's visible in. */
class FVisibleLightViewInfo
{
public:

	/** Information about a visible light in a specific DPG */
	class FDPGInfo
	{
	public:
		FDPGInfo()
			:	bHasVisibleLitPrimitives(FALSE)
		{}

		/** The dynamic primitives which are both visible and affected by this light. */
		TArray<FPrimitiveSceneInfo*,SceneRenderingAllocator> VisibleDynamicLitPrimitives;

		/** The primitives which are visible, affected by this light and receiving lit decals. */
		TArray<FPrimitiveSceneInfo*,SceneRenderingAllocator> VisibleLitDecalPrimitives;

		/** Whether the light has any visible lit primitives (static or dynamic) in the DPG */
		UBOOL bHasVisibleLitPrimitives;
	};

	/** true if this light in the view frustum (dir/sky lights always are). */
	UBOOL bInViewFrustum;

	/** Information about the light in each DPG. */
	FDPGInfo DPGInfo[SDPG_MAX_SceneRender];
	
	/** Whether each shadow in the corresponding FVisibleLightInfo::AllProjectedShadows array is visible. */
	TBitArray<SceneRenderingBitArrayAllocator> ProjectedShadowVisibilityMap;

	/** The view relevance of each shadow in the corresponding FVisibleLightInfo::AllProjectedShadows array. */
	TArray<FPrimitiveViewRelevance,SceneRenderingAllocator> ProjectedShadowViewRelevanceMap;

	/** Initialization constructor. */
	FVisibleLightViewInfo()
	:	bInViewFrustum(FALSE)
	{}

	/** Check whether the Light has visible lit primitives in any DPG */
	UBOOL HasVisibleLitPrimitives() const 
	{ 
		for (UINT DPGIndex=0;DPGIndex<SDPG_MAX_SceneRender;++DPGIndex)
		{
			if (DPGInfo[DPGIndex].bHasVisibleLitPrimitives) 
			{
				return TRUE;
			}
		}
		return FALSE;
	}
};

/** Information about a visible light which isn't view-specific. */
class FVisibleLightInfo
{
public:

	/** Projected shadows allocated on the scene rendering mem stack. */
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> MemStackProjectedShadows;

	/** All visible projected shadows. */
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> AllProjectedShadows;

	/** All visible projected preshdows.  These are not allocated on the mem stack so they are refcounted. */
	TArray<TRefCountPtr<FProjectedShadowInfo>,SceneRenderingAllocator> ProjectedPreShadows;
};


/** 
* Set of sorted translucent scene prims  
*/
class FTranslucentPrimSet
{
public:

	/** 
	* Iterate over the sorted list of prims and draw them
	* @param View - current view used to draw items
	* @param DPGIndex - current DPG used to draw items
	* @param PhaseSortedPrimitives - array with the primitives we want to draw
	* @param PassId 0:before SceneColor resolve 1:using SceneColor 2:separate translucency
	* @return TRUE if anything was drawn
	*/
	UBOOL Draw(const class FViewInfo& View,FSceneRenderer& Renderer,UINT DPGIndex, UINT PassId) const;

	/** 
	* Render Lit Translucent prims that need a depth prepass
	* @param View - current view used to draw items
	* @param DPGIndex - current DPG used to draw items
	* @return TRUE if anything was drawn
	*/
	UBOOL DrawPrepass(const class FViewInfo& View,UINT DPGIndex);	

	/** 
	* Render Lit Translucent prims that need a post-rendering depth pass
	* @param View - current view used to draw items
	* @param DPGIndex - current DPG used to draw items
	* @return TRUE if anything was drawn
	*/
	UBOOL DrawPostpass(const class FViewInfo& View,UINT DPGIndex);	

	/** 
	* Render Soft masked prims depth pass
	* @param View - current view used to draw items
	* @param DPGIndex - current DPG used to draw items
	* @return TRUE if anything was drawn
	*/
	UBOOL DrawSoftMaskedDepth(const class FViewInfo& View, UINT DPGIndex);	

	/** 
	* Render Soft masked prims after depth pass, sorted back to front
	* @param View - current view used to draw items
	* @param DPGIndex - current DPG used to draw items
	* @return TRUE if anything was drawn
	*/
	UBOOL DrawSoftMaskedBase(const class FViewInfo& View, UINT DPGIndex);	

	/**
	* Add a new primitive to the list of sorted prims
	* @param PrimitiveSceneInfo - primitive info to add. Origin of bounds is used for sort.
	* @param ViewInfo - used to transform bounds to view space
	* @param bUsesSceneColor - primitive samples from scene color
	* @param bNeedsDepthPrepass - primitive needs a depth prepass (for Opacity=1.0 pixels)
	* @param bNeedsDepthPostpass - primitive needs a post-render pass (for Opacity>0.0 pixels)
	* @param bUseSeparateTranslucency - primitive is rendered in a separate translucency pass (not affected by DepthOfField)
	*/
	void AddScenePrimitive(
		FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FViewInfo& ViewInfo, 
		UBOOL bUsesSceneColor, 
		UBOOL bSceneTextureRenderBehindTranslucency,
		UBOOL bNeedsDepthPrepass, 
		UBOOL bNeedsDepthPostpass, 
		UBOOL bUseSeparateTranslucency);

	/**
	* Add a new primitive to the list of sorted prims
	* @param PrimitiveSceneInfo - primitive info to add. Origin of bounds is used for sort.
	*/
	void AddScenePrimitiveSoftMasked(FPrimitiveSceneInfo* PrimitiveSceneInfo, const FViewInfo& ViewInfo);

	/**
	* Sort any primitives that were added to the set back-to-front
	*/
	void SortPrimitives();

	/** 
	* @return number of prims to render
	*/
	INT NumPrims() const
	{
		return SortedPrims.Num() + SortedPreSceneColorPrims.Num() + SortedSceneColorPrims.Num() + SortedSeparateTranslucencyPrims.Num();
	}
	
	/** 
	* @return number of prims that read from scene color
	*/
	INT NumSceneColorPrims() const
	{
		return SortedSceneColorPrims.Num();
	}

	/** 
	* @return number of prims that read from scene color and should be rendered before everying else
	*/
	INT NumPreSceneColorPrims() const
	{
		return SortedPreSceneColorPrims.Num();
	}

	/** 
	* @return number of lit translucent prims that need a depth prepass
	*/
	INT NumDepthPrepassPrims() const
	{
		return LitTranslucencyDepthPrepassPrims.Num();
	}

	/** 
	* @return number of prims that render as separate translucency
	*/
	INT NumSeparateTranslucencyPrims() const
	{
		return SortedSeparateTranslucencyPrims.Num();
	}

	/** 
	* @return number of lit translucent prims that need a depth post-pass
	*/
	INT NumDepthPostpassPrims() const
	{
		return LitTranslucencyDepthPostpassPrims.Num();
	}

	/** 
	* @return number of soft masked prims sorted back to front
	*/
	INT NumSoftMaskedSortedPrims() const
	{
		return SoftMaskedSortedPrims.Num();
	}

	/** 
	* @return the interface to a primitive which requires scene color
	*/
	const FPrimitiveSceneInfo* GetSceneColorPrim(INT i)const
	{
		check(i>=0 && i<NumSceneColorPrims());
		return SortedSceneColorPrims(i).PrimitiveSceneInfo;
	}

	/** 
	* @return the interface to a primitive which requires scene color
	*/
	const FPrimitiveSceneInfo* GetPreSceneColorPrim(INT i)const
	{
		check(i>=0 && i<NumPreSceneColorPrims());
		return SortedPreSceneColorPrims(i).PrimitiveSceneInfo;
	}

	/** 
	* @return the interface to a primitive which render in separate translucency
	*/
	const FPrimitiveSceneInfo* GetSeparateTranslucencyPrim(INT i)const
	{
		check(i>=0 && i<NumSeparateTranslucencyPrims());
		return SortedSeparateTranslucencyPrims(i).PrimitiveSceneInfo;
	}

private:

	/** contains a sort key */
	struct FDepthSortedPrim
	{
		/** Default constructor. */
		FDepthSortedPrim() {}

		FDepthSortedPrim(FPrimitiveSceneInfo* InPrimitiveSceneInfo, FLOAT InSortKey)
			:	PrimitiveSceneInfo(InPrimitiveSceneInfo)
			,	SortKey(InSortKey)
		{
		}

		FPrimitiveSceneInfo* PrimitiveSceneInfo;
		FLOAT SortKey;
	};

	/** contains a scene prim and its sort key */
	struct FSortedPrim :public FDepthSortedPrim
	{
		/** Default constructor. */
		FSortedPrim() {}

		FSortedPrim(FPrimitiveSceneInfo* InPrimitiveSceneInfo,FLOAT InSortKey,INT InSortPriority)
			:	FDepthSortedPrim(InPrimitiveSceneInfo, InSortKey)
			,	SortPriority(InSortPriority)
		{
		}

		INT SortPriority;
	};

	/** sortkey compare class */
	IMPLEMENT_COMPARE_CONSTREF( FDepthSortedPrim,TranslucentRender,
	{ 
		// sort from back to front
		return A.SortKey <= B.SortKey ? 1 : -1;
	} )
	/** sortkey compare class */
	IMPLEMENT_COMPARE_CONSTREF( FSortedPrim,TranslucentRender,
	{ 
		if (A.SortPriority == B.SortPriority)
		{
			// sort normally from back to front
			return A.SortKey <= B.SortKey ? 1 : -1;
		}

		// else lower sort priorities should render first
		return A.SortPriority > B.SortPriority ? 1 : -1; 
	} )

	/** list of sorted translucent primitives that use the scene color, and want to be drawn before all other translucency. */
	TArray<FSortedPrim,SceneRenderingAllocator> SortedPreSceneColorPrims;
	/** list of sorted translucent primitives */
	TArray<FSortedPrim,SceneRenderingAllocator> SortedPrims;
	/** list of sorted translucent primitives that use the scene color. These are drawn after all other translucent prims */
	TArray<FSortedPrim,SceneRenderingAllocator> SortedSceneColorPrims;
	/** list of sorted translucent primitives that render in separate translucency. Those are not blurred by Depth of Field and don't affect bloom. */
	TArray<FSortedPrim,SceneRenderingAllocator> SortedSeparateTranslucencyPrims;

	/** list of lit translucent primitives that need a depth prepass */
	TArray<FPrimitiveSceneInfo*> LitTranslucencyDepthPrepassPrims;

	/** list of lit translucent primitives that need a post-render depth pass */
	TArray<FPrimitiveSceneInfo*> LitTranslucencyDepthPostpassPrims;

	/** list of lit translucent primitives that need a post-render depth pass sorted back to front*/
	TArray<FDepthSortedPrim,SceneRenderingAllocator> SoftMaskedSortedPrims;
};

/** MotionBlur parameters */
struct FMotionBlurParams
{
	FMotionBlurParams()
		:	MotionBlurAmount( 1.0f )
		,	MaxVelocity( 1.0f )
		,	bFullMotionBlur( TRUE )
		,	RotationThreshold( 45.0f )
		,	TranslationThreshold( 10000.0f )
		,	bPlayersOnly(FALSE)
	{
	}
	FLOAT MotionBlurAmount;
	FLOAT MaxVelocity;
	UBOOL bFullMotionBlur;
	FLOAT RotationThreshold;
	FLOAT TranslationThreshold;
	/** renderthread copy from GWorld->GetWorldInfo()->bPlayersOnly */
	UBOOL bPlayersOnly;
};

/** A batched occlusion primitive. */
struct FOcclusionPrimitive
{
	FVector Origin;
	FVector Extent;
};

/**
 * Combines consecutive primitives which use the same occlusion query into a single DrawIndexedPrimitive call.
 */
class FOcclusionQueryBatcher
{
public:

	/** The maximum number of consecutive previously occluded primitives which will be combined into a single occlusion query. */
	enum { OccludedPrimitiveQueryBatchSize = 8 };

	/** Initialization constructor. */
	FOcclusionQueryBatcher(class FSceneViewState* ViewState,UINT InMaxBatchedPrimitives);

	/** Destructor. */
	~FOcclusionQueryBatcher();

	/** Renders the current batch and resets the batch state. */
	void Flush();

	/**
	 * Batches a primitive's occlusion query for rendering.
	 * @param Bounds - The primitive's bounds.
	 */
	FOcclusionQueryRHIParamRef BatchPrimitive(const FVector& BoundsOrigin,const FVector& BoundsBoxExtent);

private:

	/** The pending batches. */
	TArray<FOcclusionQueryRHIRef,SceneRenderingAllocator> BatchOcclusionQueries;

	/** The pending primitives. */
	TArray<FOcclusionPrimitive,SceneRenderingAllocator> Primitives;

	/** The batch new primitives are being added to. */
	FOcclusionQueryRHIParamRef CurrentBatchOcclusionQuery;

	/** The maximum number of primitives in a batch. */
	const UINT MaxBatchedPrimitives;

	/** The number of primitives in the current batch. */
	UINT NumBatchedPrimitives;

	/** The pool to allocate occlusion queries from. */
	class FOcclusionQueryPool* OcclusionQueryPool;
};

/** The actor visibility set that is passed back to the game thread when the scene rendering is done. */
class FActorVisibilitySet : public FActorVisibilityHistoryInterface
{
public:

#if USE_ACTOR_VISIBILITY_HISTORY
	/**
	 * Adds an actor to the visibility set.  Ensures that duplicates are not added.
	 * @param VisibleActor - The actor which is visible.
	 */
	void AddActor(const AActor* VisibleActor)
	{
		VisibleActors.Add(VisibleActor);
	}

	// FActorVisibilityHistoryInterface
	virtual UBOOL GetActorVisibility(const AActor* TestActor) const
	{
		return VisibleActors.Find(TestActor) != NULL;
	}

	//@todo debug -
	UBOOL DebugVerifyHash(const AActor* VisibleActor);

private:

	// The set of visible actors.
	TSet<const AActor*,DefaultKeyFuncs<const AActor*>,TInlineSetAllocator<4096> > VisibleActors;
#endif	
};

/** A FSceneView with additional state used by the scene renderer. */
class FViewInfo : public FSceneView
{
public:

	/** A map from primitive ID to a boolean visibility value. */
	TBitArray<SceneRenderingBitArrayAllocator> PrimitiveVisibilityMap;

#if USE_MASSIVE_LOD
	/** A map from primitive ID to a boolean value denoting if the primitive was a parent and it was processed (even if not visible). */
	TBitArray<SceneRenderingBitArrayAllocator> PrimitiveParentProcessedMap;
#endif

	/** A map from primitive ID to the primitive's view relevance. */
	TArray<FPrimitiveViewRelevance,SceneRenderingAllocator> PrimitiveViewRelevanceMap;

	/** A map from static mesh ID to a boolean visibility value. */
	TBitArray<SceneRenderingBitArrayAllocator> StaticMeshVisibilityMap;

	/** A map from static mesh ID to a boolean occluder value. */
	TBitArray<SceneRenderingBitArrayAllocator> StaticMeshOccluderMap;

	/** A map from static mesh ID to a boolean velocity visibility value. */
	TBitArray<SceneRenderingBitArrayAllocator> StaticMeshVelocityMap;

	/** A map from static mesh ID to a boolean shadow depth visibility value. */
	TBitArray<SceneRenderingBitArrayAllocator> StaticMeshShadowDepthMap;

	/** A map from decal static mesh ID to a boolean visibility value. */
	TBitArray<SceneRenderingBitArrayAllocator> DecalStaticMeshVisibilityMap;

	/** The dynamic primitives visible in this view. */
	TArray<const FPrimitiveSceneInfo*,SceneRenderingAllocator> VisibleDynamicPrimitives;

	/** The dynamic opaque decal primitives visible in this view. */
	TArray<const FPrimitiveSceneInfo*,SceneRenderingAllocator> VisibleOpaqueDynamicDecalPrimitives[SDPG_MAX_SceneRender];
	/** The dynamic translucent decal primitives visible in this view. */
	TArray<const FPrimitiveSceneInfo*,SceneRenderingAllocator> VisibleTranslucentDynamicDecalPrimitives[SDPG_MAX_SceneRender];

	/** Set of translucent prims for this view - one for each DPG */
	FTranslucentPrimSet TranslucentPrimSet[SDPG_MAX_SceneRender];

	/** Set of distortion prims for this view - one for each DPG */
	FDistortionPrimSet DistortionPrimSet[SDPG_MAX_SceneRender];
	
	/** A map from light ID to a boolean visibility value. */
	TArray<FVisibleLightViewInfo,SceneRenderingAllocator> VisibleLightInfos;

	/** The view's batched elements, sorted by DPG. */
	FBatchedElements BatchedViewElements[SDPG_MAX_SceneRender];

	/** The view's mesh elements, sorted by DPG. */
	TIndirectArray<FHitProxyMeshPair> ViewMeshElements[SDPG_MAX_SceneRender];

	/** TRUE if the DPG has at least one mesh in ViewMeshElements[DPGIndex] with a translucent material. */
	BITFIELD bHasTranslucentViewMeshElements : SDPG_MAX_SceneRender;

	/** The dynamic resources used by the view elements. */
	TArray<FDynamicPrimitiveResource*> DynamicResources;

	/** fog params for 4 layers of height fog */
	FHeightFogParams HeightFogParams;

	/** Parameters for exponential height fog. */
	FVector4 ExponentialFogParameters;
	FVector ExponentialFogColor;
	FVector LightInscatteringColor;
	FVector DominantDirectionalLightDirection;
	FLOAT FogMaxOpacity;

	/** Whether exponential height fog is enabled. */
	BITFIELD bRenderExponentialFog : 1;

	/** Whether FSceneRenderer needs to output velocities during pre-pass. */
	BITFIELD bRequiresVelocities : 1;

	/** Whether the view should store the previous frame's transforms.  This is always true if bRequiresVelocities is true. */
	BITFIELD bRequiresPrevTransforms : 1;

	/** Indicates whether previous frame transforms were reset this frame for any reason. */
	BITFIELD bPrevTransformsReset : 1;

	/** Whether we should ignore queries from last frame (useful to ignoring occlusions on the first frame after a large camera movement). */
	BITFIELD bIgnoreExistingQueries : 1;

	/** Whether we should submit new queries this frame. (used to disable occlusion queries completely. */
	BITFIELD bDisableQuerySubmissions : 1;

	/** Whether one layer height fog has already been rendered with ambient occlusion. */
	BITFIELD bOneLayerHeightFogRenderedInAO : 1;

	/** Whether we should disable distance-based fade transitions for this frame (usually after a large camera movement.) */
	BITFIELD bDisableDistanceBasedFadeTransitions : 1;

	/** TRUE if SDPG_World rendered anything to the DoF blur buffer */
	BITFIELD bRenderedToDoFBlurBuffer : 1;

	/** Last frame's view and projection matrices, only tracked if bRequiresPrevTransforms is TRUE. */
	FMatrix					PrevViewProjMatrix;

	/** Last frame's view rotation and projection matrices, only tracked if bRequiresPrevTransforms is TRUE. */
	FMatrix					PrevViewRotationProjMatrix;

	/** Last frame's translated view and projection matrix, only tracked if bRequiresPrevTransforms is TRUE. */
	FMatrix					PrevTranslatedViewProjectionMatrix;

	/** Last frame's view origin, only tracked if bRequiresPrevTransforms is TRUE. */
	FVector					PrevViewOrigin;
	FMotionBlurParams		MotionBlurParams;

	/** Last frame's PreViewTranslation, only tracked if bRequiresPrevTransforms is TRUE. */
	FVector					PrevPreViewTranslation;


	/** An intermediate number of visible static meshes.  Doesn't account for occlusion until after FinishOcclusionQueries is called. */
	INT NumVisibleStaticMeshElements;

	/** Precomputed visibility data, the bits are indexed by VisibilityId of a primitive component. */
	BYTE* PrecomputedVisibilityData;

	FOcclusionQueryBatcher IndividualOcclusionQueries;
	FOcclusionQueryBatcher GroupedOcclusionQueries;

	/** The actor visibility set for this view. */
	FActorVisibilitySet* ActorVisibilitySet;

	/** Used by occlusion for percent unoccluded calculations. */
	FLOAT OneOverNumPossiblePixels;

	/** 
	 * Initialization constructor. Passes all parameters to FSceneView constructor
	 */
	FViewInfo(
		const FSceneViewFamily* InFamily,
		FSceneViewStateInterface* InState,
		INT InParentViewIndex,
		const FSceneViewFamily* InParentViewFamily,
		FSynchronizedActorVisibilityHistory* InHistory,
		const AActor* InViewActor,
		const UPostProcessChain* InPostProcessChain,
		const FPostProcessSettings* InPostProcessSettings,
		FViewElementDrawer* InDrawer,
		FLOAT InX,
		FLOAT InY,
		FLOAT InClipX,
		FLOAT InClipY,
		FLOAT InSizeX,
		FLOAT InSizeY,
		const FMatrix& InViewMatrix,
		const FMatrix& InProjectionMatrix,
		const FLinearColor& InBackgroundColor,
		const FLinearColor& InOverlayColor,
		const FLinearColor& InColorScale,
		const TSet<UPrimitiveComponent*>& InHiddenPrimitives,
		const FRenderingPerformanceOverrides& RenderingOverrides = FRenderingPerformanceOverrides(E_ForceInit),
		FLOAT InLODDistanceFactor = 1.0f
		);

	/** 
	* Initialization constructor. 
	* @param InView - copy to init with
	*/
	explicit FViewInfo(const FSceneView* InView);

	/** 
	* Destructor. 
	*/
	~FViewInfo();

	/** TRUE if both the view and the console variable enables it */
	UBOOL UseFullMotionBlur() const;

	/** 
	* Initializes the dynamic resources used by this view's elements. 
	*/
	void InitDynamicResources();

	/** 
	* Add FPostProcessSceneProxy and update some internals. 
	* @param InProxy can be 0
	*/
	void AddPostProcessProxy(FPostProcessSceneProxy* InProxy);

	/** 
	* To output Warning that uber post process is required. 
	*/
	UBOOL RequiresMotionBlurButHasNoUberPostProcess(const FViewInfo &View) const;

private:

	/** Initialization that is common to the constructors. */
	void Init();
};


/**
 * Used to hold combined stats for a shadow. In the case of projected shadows the shadows
 * for the preshadow and subject are combined in this stat and so are primitives with a shadow parent.
 */
struct FCombinedShadowStats
{
	/** Array of shadow subjects. The first one is the shadow parent in the case of multiple entries.	*/
	FProjectedShadowInfo::PrimitiveArrayType	SubjectPrimitives;
	/** Array of preshadow primitives in the case of projected shadows.									*/
	FProjectedShadowInfo::PrimitiveArrayType	PreShadowPrimitives;
	/** Shadow resolution in the case of projected shadows												*/
	INT									ShadowResolution;
	/** Shadow pass number in the case of projected shadows												*/
	INT									ShadowPassNumber;

	/**
	 * Default constructor. 
	 */
	FCombinedShadowStats()
	:	ShadowResolution(INDEX_NONE)
	,	ShadowPassNumber(INDEX_NONE)
	{}
};

/**
 * Used as the scope for scene rendering functions.
 * It is initialized in the game thread by FSceneViewFamily::BeginRender, and then passed to the rendering thread.
 * The rendering thread calls Render(), and deletes the scene renderer when it returns.
 */
class FSceneRenderer
{
public:

	/** The scene being rendered. */
	FScene* Scene;

	/** The view family being rendered.  This references the Views array. */
	FSceneViewFamily ViewFamily;

	/** The views being rendered. */
	TArray<FViewInfo> Views;

	/** Information about the visible lights. */
	TArray<FVisibleLightInfo,SceneRenderingAllocator> VisibleLightInfos;

	/** Planar reflection shadows used by image reflections to handle shadows of dynamic objects. */
	TArray<FReflectionPlanarShadowInfo,SceneRenderingAllocator> PlanarReflectionShadows;
	
	/** The canvas transform used to render the scene. */
	FMatrix CanvasTransform;

	/** The width in screen pixels of the view family being rendered. */
	UINT FamilySizeX;

	/** The height in screen pixels of the view family being rendered. */
	UINT FamilySizeY;

	/** If a freeze request has been made */
	UBOOL bHasRequestedToggleFreeze;

	/** Whether to use a depth only pass before the base pass to maximize zcull efficiency. */
	UBOOL bUseDepthOnlyPass;

	/** The max draw distance override.  When doing visibility calculations, the minimum of
	    this and the object min draw distance will be used.  The default is +inf. */
	FLOAT MaxViewDistanceSquaredOverride;

	/* Copy from main thread GFrameNumber to be accessable on renderthread side */
	UINT FrameNumber;

	/** Initialization constructor. */
	FSceneRenderer(const FSceneViewFamily* InViewFamily,FHitProxyConsumer* HitProxyConsumer,const FMatrix& InCanvasTransform,UBOOL bInIsSceneCapture=FALSE);

	/** Destructor, stringifying stats if stats gathering was enabled. */
	~FSceneRenderer();

    /** Clears a view */
    void ClearView();

    /** Renders the basepass for the static data of a given DPG and View. */
    UBOOL RenderDPGBasePassStaticData(UINT DPGIndex, FViewInfo& View);
	UBOOL RenderDPGBasePassStaticDataMasked(UINT DPGIndex, FViewInfo& View);
	UBOOL RenderDPGBasePassStaticDataDefault(UINT DPGIndex, FViewInfo& View);

    /** Renders the basepass for the dynamic data of a given DPG and View. */
    UBOOL RenderDPGBasePassDynamicData(UINT DPGIndex, FViewInfo& View);

    /** Renders the basepass for a given DPG and View. */
    UBOOL RenderDPGBasePass(UINT DPGIndex, FViewInfo& View);

	/** Begins the rendering for a given DPG.  */
    UBOOL RenderDPGBegin(UINT DPGIndex, UBOOL& bRequiresClear, UBOOL& bSceneColorDirty, UBOOL bIsOcclusionTesting);

	/** Ends the rendering for a given DPG. */
    void RenderDPGEnd(UINT DPGIndex, UBOOL bDeferPrePostProcessResolve, UBOOL& bSceneColorDirty, UBOOL bIsOcclusionTesting);

	/** Finishes the view family rendering. */
    void RenderFinish (UBOOL bDeferPrePostProcessResolve);

	/** Renders the view family. */
	void Render();

	/** Renders the previously captured scene color for the view, bypasses all other rendering */
	void RenderCapturedSceneColor();

	/** Renders only the final post processing for the view */
	void RenderPostProcessOnly();

	/** Render the view family's hit proxies. */
	void RenderHitProxies();

	/** Renders the scene to capture target textures */
	void RenderSceneCaptures();

    /** Processes a primitive that is determined to be visible. */
    UBOOL ProcessVisible(
        INT ViewIndex,
        DWORD& ViewVisibilityMap,
        const FPrimitiveSceneInfoCompact& CompactPrimitiveSceneInfo,
        FLOAT DistanceSquared,
        UBOOL bIsDefinitelyUnoccluded
        );

	/** Renders shadow depths for lit translucency. */
	const FProjectedShadowInfo* RenderTranslucentShadowDepths(const FLightSceneInfo* LightSceneInfo, const FPrimitiveSceneInfo* PrimitiveSceneInfo, UINT DPGIndex);

private:
	/** Bit flags that specifies what RenderDPGBasePassDynamicPrimitives() should draw. */
	enum EBasePassDynamicBitFlags
	{
		// Draw dynamic primitives that aren't casting projected mod shadows
		BasePassDynamic_NonSelfShadowing = 0x01,
		// Draw dynamic primitives that are casting projected mod shadows
		BasePassDynamic_SelfShadowing = 0x02,
		// Draw all dynamic primitives
		BasePassDynamic_All = BasePassDynamic_NonSelfShadowing | BasePassDynamic_SelfShadowing,
	};


	/** Visible shadow casting lights in any DPG. */
	TArray<const FLightSceneInfo*,SceneRenderingAllocator> VisibleShadowCastingLightInfos;

	/** TRUE if any dominant lights are casting dynamic shadows. */
	UBOOL bDominantShadowsActive;

	/** TRUE if any visible materials have bInheritDominantShadowsRelevance. */
	UBOOL bHasInheritDominantShadowMaterials;

	/** TRUE if the scene renderer is used for rendering a scene capture. */
	UBOOL bIsSceneCapture;

	/** The world time of the previous frame. */
	FLOAT PreviousFrameTime;

	/** 
	 * Renders lights and shadows for a given DPG.
	 *
	 * @param DPGIndex - current depth priority group index
	 * @param bSceneColorDirty - TRUE if the scene color is left dirty
	 */
	void RenderDPGLights(UINT DPGIndex, UBOOL& bSceneColorDirty);

#if WITH_MOBILE_RHI
	/** 
	 * Renders lights and shadows for a given DPG on mobile
	 *
	 * @param DPGIndex - current depth priority group index
	 * @return TRUE if anything was rendered to scene color
	 */
	UBOOL PrepareMobileDPGLights( UINT DPGIndex );
	UBOOL ApplyMobileDPGLights( UINT DPGIndex );
#endif

	/**
	 * Renders the basepass dynamic primitives for a given DPG and View.
	 * A set of bit flags (DrawFlags) acts as a filter and determines if each primitive should be drawn or not.
	 *
	 * @param DPGIndex					- current depth priority group index
	 * @param View						- current view
	 * @param DrawFlags					- Bit flags that determines what kind of primitives to draw or ignore
	 * @param OutNumIgnoredPrimitives	- [Out] Number of primitives that were ignored
	 * @return TRUE if anything was rendered to scene color
	 */
	UBOOL RenderDPGBasePassDynamicPrimitives( UINT DPGIndex, FViewInfo& View, EBasePassDynamicBitFlags DrawFlags, INT& OutNumIgnoredPrimitives );

	/**
	 * Generates recursive list of child scene infos
	 * @param InPrimitiveSceneInfo - Root of the shadow tree
	 * @param OutChildSceneInfos - List of all child scene infos connected to the input scene info for shadowing
	 */
	void GenerateChildSceneInfos(const FPrimitiveSceneInfo* InPrimitiveSceneInfo, UBOOL bEditor, TArray <FPrimitiveSceneInfo*, SceneRenderingAllocator>& OutChildSceneInfos) const;

	/**
	 * Creates a projected shadow for light-primitive interaction.
	 * @param Interaction - The interaction to create a shadow for.
	 * @param OutPreShadows - If the primitive has a preshadow, CreateProjectedShadow adds it to OutPreShadows.
	 * @return new FProjectedShadowInfo if one was created
	 */
	FProjectedShadowInfo* CreateProjectedShadow(
		FLightPrimitiveInteraction* Interaction,
		TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& OutPreShadows,
		const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ViewDependentWholeSceneShadows
		);

	/** Returns a cached preshadow matching the input criteria if one exists. */
	TRefCountPtr<FProjectedShadowInfo> GetCachedPreshadow(
		const FLightPrimitiveInteraction* InParentInteraction, 
		const FProjectedShadowInitializer& Initializer,
		const FBoxSphereBounds& Bounds,
		UINT InResolutionX,
		UINT InResolutionY);

	/** Removes stale shadows and attempts to add new preshadows to the cache. */
	void UpdatePreshadowCache();

	/** Creates planar reflection shadows for this frame. */
	void CreatePlanarReflectionShadows();

	/**
	 * Creates a projected shadow for all primitives affected by a light.  If the light doesn't support whole-scene shadows, it returns FALSE.
	 * @param LightSceneInfo - The light to create a shadow for.
	 * @return TRUE if a whole scene shadow was created
	 */
	UBOOL CreateWholeSceneProjectedShadow(FLightSceneInfo* LightSceneInfo);

	/**
	 * Checks to see if this primitive is affected by any of the given modulate better or pre shadows (used 
	 * to filter out modulate better shadows on emissive/backfaces, as well as check for pre-shadow interactions)
	 *
	 * @param PrimitiveSceneInfoCompact The primitive to check for shadow interaction
	 * @param ValidViews Which views to process this primitive for
	 * @param PreShadows The list of pre-shadows to check against
	 *
	 * @return The views that the children should be checked in
	 */
	BYTE GatherShadowsForPrimitiveInner(const FPrimitiveSceneInfoCompact& PrimitiveSceneInfoCompact, BYTE ValidViews,
		const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& PreShadows,
		const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ForwardObjectShadows,
		const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ViewDependentWholeSceneShadows);

	/**
	 * Calls ProcessPrimitiveCullingInner for a list of primitives, and process any children
	 *
	 * @param Primitives List of primitives to process (along with their children)
	 * @param NumPrimitives Number of primitives in Primitives
	 * @param ValidViews Views for which to process these primitives
	 */
	void GatherShadowsForPrimitives(const FPrimitiveSceneInfoCompact** Primitives, INT NumPrimitives, BYTE ValidViews,
		const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& PreShadows,
		const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ForwardObjectShadows,
		const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ViewDependentWholeSceneShadows);


	/** Gathers the list of primitives used to draw pre-shadows and modulate-better shadows. */
	void GatherShadowPrimitives(
		const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& PreShadows,
		const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ForwardObjectShadows,
		const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ViewDependentWholeSceneShadows
		);

	/** Calculates projected shadow visibility. */
	void InitProjectedShadowVisibility();

	/** Finds the visible dynamic shadows for each view. */
	void InitDynamicShadows();

	/**
	 * Calculates what shadows are made by the given the LightPrimInteractions
	 * When using MassiveLOD, if the primitive in the interaction isn't drawn due to MinDrawDistance,
	 * then the children light primitive interactions will be checked (by recursively calling this function).
	 * If the primitive is drawn, it will never iterate over the children interactions
	 *
	 * @param Interactions A list of interactions to check for shadowing
	 * @param VisibleLightInfo Information about the current light, will be updated
	 * @param PreShadows Collects any preshadows
	 * @param Origin View origin (used to calculate LOD)
	 */
	void SetupInteractionShadows(TArray<FLightPrimitiveInteraction*>& Interaction, 
		FVisibleLightInfo& VisibleLightInfo, 
		TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& PreShadows, 
		TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ForwardObjectShadows, 
		const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ViewDependentWholeSceneShadows,
		const FVector& Origin);

	/** Does view frustum culling. */
    void PerformViewFrustumCulling();

	/** Determines which primitives are visible for each view. */
	void InitViews(); 

	/** Initialized the fog constants for each view. */
	void InitFogConstants();

	/**
	 * Renders the scene's prepass and occlusion queries.
	 * If motion blur is enabled, it will also render velocities for dynamic primitives to the velocity buffer and
	 * flag those pixels in the stencil buffer.
	 * @param DPGIndex - current depth priority group index
	 * @param bIsOcclusionTesting - TRUE if testing occlusion
	 * @param ViewIndex - view to render; -1 for all views
	 * @return TRUE if anything was rendered
	 */
	UBOOL RenderPrePass(UINT DPGIndex,UBOOL bIsOcclusionTesting, INT ViewIndex);

	/**
	 * Renders the scene's prepass and occlusion queries.
	 * Used by RenderPrePass
	 */
	UBOOL RenderPrePassInner(UINT DPGIndex,UBOOL bIsOcclusionTesting,UINT ViewIndex);

	/**
	 * Renders the prepass for the given DPG and View.
	 */
	UBOOL RenderDPGPrePass(UINT DPGIndex, FViewInfo& View);

	/** 
	* Renders the scene's base pass 
	*
	* @param DPGIndex - current depth priority group index
	* @return TRUE if anything was rendered
	*/
	UBOOL RenderBasePass(UINT DPGIndex);

	/** 
	* Renders the scene's decals
	*
	* @param DPGIndex - current depth priority group index
	* @param bTranslucentPass - if TRUE render translucent decals on opqaue receivers
	*							if FALSE render opqaue decals on opaque/translucent receivers
	* @return TRUE if anything was rendered to scene color
	*/
	UBOOL RenderDecals(const FViewInfo& View, UINT DPGIndex, UBOOL bTranslucentPass);

	/** 
	* Issues occlusion tests if a depth pass was not rendered.
	*/
	void BeginOcclusionTests();

	/** Renders deferred image reflections. */
	UBOOL RenderImageReflections(UINT DPGIndex);

	/** Renders light shafts. */
	UBOOL RenderLightShafts();

	/**
	 * Returns the mobile post-process settings for the specified view.
	 *
	 * @param View			View to query settings for
	 * @param OutSettings	[out] Will be filled in with the settings upon return
	 * @return				TRUE if any mobile post-process effect should be rendered
	 */
	UBOOL GetMobilePostProcessSettings( const FSceneView& View, FPostProcessSettings& OutSettings ) const;

	/** Renders the scene's fogging. */
	UBOOL RenderFog(UINT DPGIndex);

	/** Renders the scene's lighting. */
	UBOOL RenderLights(UINT DPGIndex,UBOOL bAffectedByModulatedShadows, UBOOL bWasSceneColorDirty);

	/** Renders the scene's subsurface scattering. */
	UBOOL RenderSubsurfaceScattering(UINT DPGIndex);

	/** Renders the scene's distortion */
	UBOOL RenderDistortion(UINT DPGIndex);
	
	/** 
	 * Renders the scene's translucency.
	 *
	 * @param	DPGIndex	Current DPG used to draw items.
	 * @return				TRUE if anything was drawn.
	 */
	UBOOL RenderTranslucency(UINT DPGIndex);

	/** 
	* Renders any depths for pixels touched by lit translucent objects, for correct fog and DoF.
	*
	* @param	DPGIndex	Current DPG used to draw items.
	* @return				TRUE if anything was drawn.
	*/
	UBOOL RenderPostTranslucencyDepths(UINT DPGIndex);

	/** 
	* Renders soft masked objects depth pass (only required on hardware where we cannot access the native z buffer).
	*
	* @param	DPGIndex	Current DPG used to draw items.
	* @return				TRUE if anything was drawn.
	*/
	UBOOL RenderSoftMaskedDepth(UINT DPGIndex);

	/** 
	* Renders soft masked objects as alpha transparent, sorted back to front.
	*
	* @param	DPGIndex	Current DPG used to draw items.
	* @return				TRUE if anything was drawn.
	*/
	UBOOL RenderSoftMaskedBase(UINT DPGIndex);

	/**
	 * Renders radial blur using multiple samples of the scene color buffer
	 *
	 * @param DPGIndex - current depth priority group index
	 * @param bSceneColorIsDirty - TRUE if the current scene color needs a resolve
	 * @return TRUE if anything was rendered
	 */
	UBOOL RenderRadialBlur(UINT DPGIndex,UBOOL bSceneColorIsDirty);

	/** Renders the velocities of movable objects for the motion blur effect. */
	void RenderVelocities(UINT DPGIndex);

	/** Renders world-space texture density instead of the normal color. */
	UBOOL RenderTextureDensities(UINT DPGIndex);

	/** Renders world-space lightmap density instead of the normal color. */
	UBOOL RenderLightMapDensities(UINT DPGIndex);

	/** bound shader state for occlusion test prims */
	static FGlobalBoundShaderState OcclusionTestBoundShaderState;

	/** Renders the post process effects for a view. */
	UBOOL RenderPostProcessEffects(UINT DPGIndex, UBOOL bAffectLightingOnly = FALSE);

	/** Updates the downsized depth buffer with the current full resolution depth buffer. */
	void UpdateDownsampledDepthSurface();

	/** Resolves an MSAA depth surface to the ResolvedDepthBuffer texture. */
	void ResolveMSAADepthSurface( const FViewInfo& View );

	/**
	* Finish rendering a view, mapping accumulated shader complexity to a color.
	* @param View - The view to process.
	*/
	void RenderShaderComplexity(const FViewInfo* View);
	/** bound shader state for full-screen shader complexity apply pass */
	static FGlobalBoundShaderState ShaderComplexityBoundShaderState;

	/**
	 * Finish rendering a view, by rendering depth dependent halos around large changes in depth
	 * @param View - The view to process
	 * @return - TRUE if the post process was actually applied
	 */
	UBOOL RenderDepthDependentHalo(const FViewInfo* View);
	/** bound shader state for full-screen depth dependent halo apply pass */
	static FGlobalBoundShaderState DepthDependentHaloBoundShaderState;

	/**
	 * Finish rendering a view, writing the contents to ViewFamily.RenderTarget.
	 * @param View - The view to process.
	 * @param bIgnoreScaling - if true, this will not consider scaling when deciding to early out
	*/
	void FinishRenderViewTarget(const FViewInfo* View, UBOOL bIgnoreScaling = FALSE);
	/** bound shader state for full-screen gamma correction pass */
	static FGlobalBoundShaderState PostProcessBoundShaderState;

	/** Returns TRUE if any dominant lights are casting dynamic shadows. */
	UBOOL AreDominantShadowsActive(UINT DPGIndex) const;

	/** Renders whole scene dominant shadow depths. */
	void RenderWholeSceneDominantShadowDepth(UINT DPGIndex);

	/** Accumulates normal shadows from the dominant light into the light attenuation buffer. */
	void RenderDominantLightShadowsForBasePass(UINT DPGIndex);

	/**
	  * Used by RenderLights to figure out if projected shadows need to be rendered to the attenuation buffer.
	  *
	  * @param LightSceneInfo Represents the current light
	  * @return TRUE if anything needs to be rendered
	  */
	UBOOL CheckForProjectedShadows( const FLightSceneInfo* LightSceneInfo, UINT DPGIndex ) const;

	/** Returns TRUE if any projected shadows need to be rendered for the given view. */
	UBOOL CheckForProjectedShadows( const FViewInfo& View, const FLightSceneInfo* LightSceneInfo, UINT DPGIndex, FLOAT& ClosestDistanceFromViews ) const;

	/** Renders preshadow depths for any preshadows whose depths aren't cached yet, and renders the projections of preshadows with opaque relevance. */
	UBOOL RenderCachedPreshadows(
		const FLightSceneInfo* LightSceneInfo, 
		UINT DPGIndex, 
		UBOOL bRenderingBeforeLight);

	/** Renders one pass point light shadows. */
	UBOOL RenderOnePassPointLightShadows(
		const FLightSceneInfo* LightSceneInfo, 
		UINT DPGIndex, 
		UBOOL bRenderingBeforeLight);

	/** Renders the projections of the given Shadows to the appropriate color render target. */
	void RenderProjections(
		const FLightSceneInfo* LightSceneInfo, 
		const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& Shadows, 
		UINT DPGIndex, 
		UBOOL bRenderingBeforeLight);

	/**
	  * Used by RenderLights to render projected shadows to the attenuation buffer.
	  *
	  * @param LightSceneInfo Represents the current light
	  * @return TRUE if anything got rendered
	  */
	UBOOL RenderProjectedShadows( const FLightSceneInfo* LightSceneInfo, UINT DPGIndex, UBOOL bRenderingBeforeLight );

#if WITH_MOBILE_RHI
	/**
	  * Three-pass projected shadow rendering for mobile
	  * 1. Gather the shadows to render
	  * 2. Render the shadows into appropriate shadow buffers
	  * 3. Apply the rendered shadows to the scene
	  *
	  * @param LightSceneInfo Represents the current light
	  * @return TRUE if anything got rendered
	  */
	UBOOL GatherMobileProjectedShadows( UINT DPGIndex, const FLightSceneInfo* LightSceneInfo );
	UBOOL RenderMobileProjectedShadows( UINT DPGIndex );
	UBOOL ApplyMobileProjectedShadows( UINT DPGIndex );
	void RenderMobileProjections( UINT DPGIndex );
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> MobileProjectedShadows;
#endif

	/**
	  * Used by RenderLights to figure out if light functions need to be rendered to the attenuation buffer.
	  *
	  * @param LightSceneInfo Represents the current light
	  * @return TRUE if anything got rendered
	  */
	UBOOL CheckForLightFunction( const FLightSceneInfo* LightSceneInfo, UINT DPGIndex ) const;

	/** Returns TRUE if a light function needs to be rendered for the given view. */
	UBOOL CheckForLightFunction( const FViewInfo& View, const FLightSceneInfo* LightSceneInfo, UINT DPGIndex, FLOAT& ClosestDistanceFromViews ) const;

	/**
	  * Used by RenderLights to render a light function to the attenuation buffer.
	  *
	  * @param LightSceneInfo Represents the current light
	  * @param LightIndex The light's index into FScene::Lights
	  */
	UBOOL RenderLightFunction( const FLightSceneInfo* LightSceneInfo, UINT DPGIndex, UBOOL bShadowsRendered );

	/**
	  * Used by RenderLights to render a light to the scene color buffer.
	  *
	  * @param LightSceneInfo Represents the current light
	  * @param LightIndex The light's index into FScene::Lights
	  * @return TRUE if anything got rendered
	  */
	UBOOL RenderLight( const FLightSceneInfo* LightSceneInfo, UINT DPGIndex );

	/** Renders the light's influence on the scene in a deferred pass. */
	UBOOL RenderLightDeferred(const FLightSceneInfo* LightSceneInfo, UINT DPGIndex);

	/**
	 * Renders all the modulated shadows to the scene color buffer.
	 * @param	DPGIndex					Current DPG used to draw items.
	 * @return TRUE if anything got rendered
	 */
	UBOOL RenderModulatedShadows(UINT DPGIndex);

#if WITH_MOBILE_RHI
	/**
	 * Prepares and renders all the modulated shadows to the scene color buffer on mobile.
	 * @param	DPGIndex					Current DPG used to draw items.
	 * @return TRUE if anything got rendered
	 */
	UBOOL PrepareMobileModulatedShadows(UINT DPGIndex);
	UBOOL ApplyMobileModulatedShadows(UINT DPGIndex);
#endif

	/**
	* Clears the scene color depth (stored in alpha channel) to max depth
	* This is needed for depth bias blend materials to show up correctly
	*/
	void ClearSceneColorDepth();

	/** Saves the actor and primitive visibility states for the game thread. */
	void SaveVisibilityState();

	/** Helper used to compute the minimual screen bounds of all translucent primitives which require scene color */
	UBOOL ComputeTranslucencyResolveRectangle(INT DPGIndex, FIntRect& PixelRect);

	/** Helper used to compute the Squared Screen Sized threshold used to determine if a primitive should render to depth */
	FLOAT GetScreenThresholdSq() const;

	/** Helper used to compute the minimual screen bounds of all distortion primitives which require scene color */
	UBOOL ComputeDistortionResolveRectangle(INT DPGIndex, FIntRect& PixelRect);

	/** Helper function to determine post process resolve optimization. */
	UBOOL DeferPrePostProcessResolve();

	/**
	 * Draws a alpha-blended quad over the entire viewport for any views that have overlay opacity > 0
	 */
	void RenderOverlayQuadsIfNeeded();

	/**
	 * Draws the contents of one of the statically allocated render targets over the entire screen
	 */
	void RenderOverlayRenderTarget(ESceneRenderTargetTypes RenderTarget);

	/**
	 * Prepares texture pool memory to fit temporal AA allocation, if needed.
	 * Must be called on the gamethread.
	 */
	void PrepareTemporalAAAllocation();

	/** Renders temporal anti-aliasing. */
	void RenderTemporalAA();

	// Massive LOD Support

#if !CONSOLE
	/** Whether or not to perform min distance checks during primitive culling */
	UBOOL bPerformMinDistanceChecks;
#endif

	/** Temp stat variables for gathering stats for one frame of culling */
	STAT(INT NumOccludedPrimitives);
	STAT(INT NumProcessedPrimitives);
	STAT(INT NumCulledPrimitives);
	STAT(INT NumLODDroppedPrimitives);
	STAT(INT NumMinDrawDroppedPrimitives);
	STAT(INT NumMaxDrawDroppedPrimitives);

	/**
	 * Performs culling for one primitive, and returns what views it was drawn in
	 *
	 * @param CompactPrimitiveSceneInfo The primitive to process
	 * @param ValidViews Which views to process this primitive for
	 * @param FullyInViews Which views this primitive is known to be in the view frustum
	 * @param Depth Current hierarchy depth (0 for root primitives)
	 *
	 * @return The views that we want to render the children primitives in
	 */
	BYTE ProcessPrimitiveCullingInner(const FPrimitiveSceneInfoCompact& CompactPrimitiveSceneInfo, BYTE ValidViews, BYTE FullyInViews, INT Depth);

	/**
	 * Updates screen door fading for a primitive.  Called during the primitive culling phase.
	 *
	 * @param	View	View the primitive's visibility is being computed for
	 * @param	Depth	Hierarchical primitive depth
	 * @param	bShouldBeVisible	True if the culling phase has determined the primitive should be visible
	 * @param	CompactPrimitiveSceneInfo       The primitive to process
	 *
	 * @return	True if the primitive is still fading in or out and should be rendered even if culled
	 */
	UBOOL UpdatePrimitiveFading( FViewInfo& View, const INT Depth, const UBOOL bShouldBeVisible, const FPrimitiveSceneInfoCompact& CompactPrimitiveSceneInfo );

	/** 
	 * Kicks off a fade if the new LOD does not match the previous LOD that this primitive was rendered with. 
	 * Returns an LOD that should be rendered instead of the new LOD level.
	 */
	SBYTE UpdatePrimitiveLODUsed( FViewInfo& View, SBYTE NewLODIndex, const FPrimitiveSceneInfoCompact& CompactPrimitiveSceneInfo );

	/**
	 * Calls ProcessPrimitiveCullingInner for a list of primitives, and process any children
	 *
	 * @param Primitives List of primitives to process (along with their children)
	 * @param NumPrimitives Number of primitives in Primitives
	 * @param ValidViews Views for which to process these primitives
	 * @param FullyInViews Which views this primitive is known to be in the view frustum
	 * @param Depth Current hierarchy depth (0 for root primitives)
	 */
	void ProcessPrimitiveCulling(const FPrimitiveSceneInfoCompact** Primitives, INT NumPrimitives, BYTE ValidViews, BYTE FullyInViews, INT Depth);

	/**
	 * Render texture content to screen, for debugging purpose, activated by console command "VisualizeTexture"
	 */
	void VisualizeSceneTexture();

	/**
	 * Processes debug options at the end of a render frame.
	 */
	void ProcessAndRenderDebugOptions();
};

/** Helper function to capture scene renderers and properly protect the post process proxies that are held by the game thread and should not be deleted */
FSceneRenderer* CreateSceneCaptureRenderer(FSceneView* InView, FSceneViewFamily* InViewFamily, TArray<class FPostProcessSceneProxy*>& InPostProcessSceneProxies, FHitProxyConsumer* HitProxyConsumer,const FMatrix& InCanvasTransform,UBOOL bInIsSceneCapture=FALSE);

/** Helper function to delete the capture scene renderer and properly protect the post process proxy */
void DeleteSceneCaptureRenderer(FSceneRenderer* InSceneRenderer);


#if WITH_MOBILE_RHI
/*-----------------------------------------------------------------------------
	FMobilePostProcessSceneProxy
-----------------------------------------------------------------------------*/

class FMobilePostProcessSceneProxy : public FPostProcessSceneProxy
{
public:

	FMobilePostProcessSceneProxy(const FViewInfo& View);

	/**
	* Render the post process effect
	* Called by the rendering thread during scene rendering
	* @param InDepthPriorityGroup - scene DPG currently being rendered
	* @param View - current view
	* @param CanvasTransform - same canvas transform used to render the scene
	* @param LDRInfo - helper information about SceneColorLDR
	* @return TRUE if anything was rendered
	*/
	virtual UBOOL Render(const FScene* Scene, UINT InDepthPriorityGroup,FViewInfo& View,const FMatrix& CanvasTransform,FSceneColorLDRInfo& LDRInfo);

protected:

	UBOOL IsColorGradingNeeded() const;

	// copy of the post process settings for the render thread
	FPostProcessSettings Settings;
};
#endif //WITH_MOBILE_RHI
