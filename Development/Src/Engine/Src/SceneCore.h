/*=============================================================================
	SceneCore.h: Core scene definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Forward declarations.
class FStaticMesh;
class FScene;
class FPrimitiveSceneInfo;
class FLightSceneInfo;

/**
 * An interaction between a light and a primitive.
 */
class FLightPrimitiveInteraction
{
public:
	/** Percent to start fading modulative shadows out at. */
	FLOAT ModShadowStartFadeOutPercent;

	/** Percent to start fading modulative shadows in at. */
	FLOAT ModShadowStartFadeInPercent;

	/** Creates an interaction for a light-primitive pair. */
	static void InitializeMemoryPool();
	static void Create(FLightSceneInfo* LightSceneInfo,FPrimitiveSceneInfo* PrimitiveSceneInfo);
	static void Destroy(FLightPrimitiveInteraction* LightPrimitiveInteraction);

	/** Returns current size of memory pool */
	static DWORD GetMemoryPoolSize();

	// Accessors.
	UBOOL HasShadow() const { return bCastShadow; }
	UBOOL IsLightMapped() const { return bLightMapped; }
	UBOOL IsDynamic() const { return bIsDynamic; }
	UBOOL IsUncachedStaticLighting() const { return bUncachedStaticLighting; }
	/** Returns TRUE if the primitive's static meshes should be added to the light's static draw lists. */
	UBOOL ShouldAddStaticMeshesToLightingDrawLists() const;
	FLightSceneInfo *	GetLight() const { return LightSceneInfo; }
	INT GetLightId() const { return LightId; }
	FPrimitiveSceneInfo* GetPrimitiveSceneInfo() const { return PrimitiveSceneInfo; }
	FLightPrimitiveInteraction* GetNextPrimitive() const { return NextPrimitive; }
	FLightPrimitiveInteraction* GetNextLight() const { return NextLight; }

#if USE_MASSIVE_LOD
	FLightPrimitiveInteraction* GetParentPrimitive() const { return ParentPrimitive; }

	FORCEINLINE const TArray<FLightPrimitiveInteraction*>& GetChildInteractions() const
	{
		return ChildInteractions;
	}

	FORCEINLINE TArray<FLightPrimitiveInteraction*>& GetChildInteractions()
	{
		return ChildInteractions;
	}

	FORCEINLINE UBOOL IsParentOnly() const
	{
		return bIsLODParentOnly;
	}
#endif

	/** Hash function required for TMap support */
	friend DWORD GetTypeHash( const FLightPrimitiveInteraction* Interaction )
	{
		return (DWORD)Interaction->LightId;
	}

	/** Custom new/delete */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

	/** Set if the interaction needs a light rendering pass. Called by FSceneRenderer::ProcessVisible() */
	void SetNeedsLightRenderingPass(UBOOL InbNeedsLightRenderingPass) { bNeedsLightRenderingPass = InbNeedsLightRenderingPass; }

	/* Return TRUE if the interaction needs a light rendering pass. Checked by Lit Translucency */
	UBOOL NeedsLightRenderingPass() const { return bNeedsLightRenderingPass; }

private:
	/** The index into Scene->Lights of the light which affects the primitive. */
	INT LightId;

	/** The light which affects the primitive. */
	FLightSceneInfo* LightSceneInfo;

	/** The primitive which is affected by the light. */
	FPrimitiveSceneInfo* PrimitiveSceneInfo;

	/** True if the primitive casts a shadow from the light. */
	BITFIELD bCastShadow : 1;

	/** True if the primitive has a light-map containing the light. */
	BITFIELD bLightMapped : 1;

	/** True if the interaction is dynamic. */
	BITFIELD bIsDynamic : 1;

	/** True if the interaction is an uncached static lighting interaction. */
	BITFIELD bUncachedStaticLighting : 1;

	/** True if the interaction needs a light rendering pass. 
	  * Calculated by FSceneRenderer::ProcessVisible() and checked by Lit Translucency */
	BITFIELD bNeedsLightRenderingPass : 1;

#if USE_MASSIVE_LOD
	/** True if the interaction is solely used as a parent for dynamic children */
	BITFIELD bIsLODParentOnly : 1;

	/** For MassieLOD children, we need a hierarchy of interactions so we use proper shadowing for the proper LOD level */
	TArray<FLightPrimitiveInteraction*> ChildInteractions;

	/** The parent interaction if this interaction is in someone else's ChildInteractions array */
	FLightPrimitiveInteraction* ParentPrimitive;

#endif

	/** A pointer to the NextPrimitive member of the previous interaction in the light's interaction list. */
	FLightPrimitiveInteraction** PrevPrimitiveLink;

	/** The next interaction in the light's interaction list. */
	FLightPrimitiveInteraction* NextPrimitive;

	/** A pointer to the NextLight member of the previous interaction in the primitive's interaction list. */
	FLightPrimitiveInteraction** PrevLightLink;

	/** The next interaction in the primitive's interaction list. */
	FLightPrimitiveInteraction* NextLight;


	/** Initialization constructor. */
	FLightPrimitiveInteraction(FLightSceneInfo* InLightSceneInfo,FPrimitiveSceneInfo* InPrimitiveSceneInfo,UBOOL bIsDynamic,UBOOL bInLightMapped);

	/** Hide dtor */
	~FLightPrimitiveInteraction();

};

/**
* The information used to a render a scene capture.  This is the rendering thread's mirror of the game thread's USceneCaptureComponent.
*/
class FCaptureSceneInfo
{
public:

	/** The capture probe for the component. */
	FSceneCaptureProbe* SceneCaptureProbe;

	/** The USceneCaptureComponent this scene info is for.  It is not safe to dereference this pointer.  */
	const USceneCaptureComponent* Component;

	/** 
	 *The index of the info in Scene->SceneCaptures for each thread. 
	 *There is one for each thread due to a circular dependency between SceneInfos (RT) and ViewInfos (GT)
	 */
	INT RenderThreadId;
	INT GameThreadId;

	/** Owner name, used for profiling. */
	FName OwnerName;

	/** Scene currently using this capture info */
	FScene* Scene;

	/** 
	* Constructor 
	* @param InComponent - mirrored scene capture component requesting the capture
	* @param InSceneCaptureProbe - new probe for capturing the scene
	*/
	FCaptureSceneInfo(USceneCaptureComponent* InComponent,FSceneCaptureProbe* InSceneCaptureProbe);

	/** 
	* Destructor
	*/
	~FCaptureSceneInfo();

	/**
	* Capture the scene
	* @param SceneRenderer - original scene renderer so that we can match certain view settings
	*/
	void CaptureScene(class FSceneRenderer* SceneRenderer);

	/**
	* Add this capture scene info to a scene 
	* @param InScene - scene to add to
	*/
	void AddToScene(class FScene* InScene);

	/**
	* Remove this capture scene info from a scene 
	* @param InScene - scene to remove from
	*/
	void RemoveFromScene(class FScene* InScene);
};

/**
 * A mesh which is defined by a primitive at scene segment construction time and never changed.
 * Lights are attached and detached as the segment containing the mesh is added or removed from a scene.
 */
class FStaticMesh : public FMeshBatch
{
public:

	/**
	 * An interface to a draw list's reference to this static mesh.
	 * used to remove the static mesh from the draw list without knowing the draw list type.
	 */
	class FDrawListElementLink : public FRefCountedObject
	{
	public:
		virtual UBOOL IsInDrawList(const class FStaticMeshDrawListBase* DrawList) const = 0;
		virtual void Remove() = 0;
	};

	/** The squared minimum distance to draw the primitive at. */
	FLOAT MinDrawDistanceSquared;

	/** The squared maximum distance to draw the primitive at. */
	FLOAT MaxDrawDistanceSquared;

	/** The render info for the primitive which created this mesh. */
	FPrimitiveSceneInfo* PrimitiveSceneInfo;

	/** The ID of the hit proxy which represents this static mesh. */
	FHitProxyId HitProxyId;

	/** The index of the mesh in the scene's static meshes array. */
	INT Id;

	// Constructor/destructor.
	FStaticMesh(
		FPrimitiveSceneInfo* InPrimitiveSceneInfo,
		const FMeshBatch& InMesh,
		FLOAT InMinDrawDistanceSquared,
		FLOAT InMaxDrawDistanceSquared,
		FHitProxyId InHitProxyId
		):
		FMeshBatch(InMesh),
		MinDrawDistanceSquared(InMinDrawDistanceSquared),
		MaxDrawDistanceSquared(InMaxDrawDistanceSquared),
		PrimitiveSceneInfo(InPrimitiveSceneInfo),
		HitProxyId(InHitProxyId),
		Id(INDEX_NONE)
	{
		// If the static mesh is in an invalid DPG, move it to the world DPG.
        if(DepthPriorityGroup >= SDPG_MAX_SceneRender)
		{
			DepthPriorityGroup = SDPG_World;
		}
	}
	~FStaticMesh();

	/** Adds a link from the mesh to its entry in a draw list. */
	void LinkDrawList(FDrawListElementLink* Link);

	/** Removes a link from the mesh to its entry in a draw list. */
	void UnlinkDrawList(FDrawListElementLink* Link);

	/** Adds the static mesh to the appropriate draw lists in a scene. */
	void AddToDrawLists(FScene* Scene);

	/** Removes the static mesh from all draw lists. */
	void RemoveFromDrawLists();

	/** Returns TRUE if the mesh is linked to the given draw list. */
	UBOOL IsLinkedToDrawList(const FStaticMeshDrawListBase* DrawList) const;

private:
	/** Links to the draw lists this mesh is an element of. */
	TArray<TRefCountPtr<FDrawListElementLink> > DrawListLinks;

	/** Private copy constructor. */
	FStaticMesh(const FStaticMesh& InStaticMesh):
		FMeshBatch(InStaticMesh),
		MinDrawDistanceSquared(InStaticMesh.MinDrawDistanceSquared),
		MaxDrawDistanceSquared(InStaticMesh.MaxDrawDistanceSquared),
		PrimitiveSceneInfo(InStaticMesh.PrimitiveSceneInfo),
		HitProxyId(InStaticMesh.HitProxyId),
		Id(InStaticMesh.Id)
	{}
};

/** The properties of a height fog layer which are used for rendering. */
class FHeightFogSceneInfo
{
public:

	/** The fog component the scene info is for. */
	const UHeightFogComponent* Component;
	/** z-height for the fog plane - updated by the owning actor */
	FLOAT Height;
	/** affects the scale for the fog layer's thickness */
	FLOAT Density;
	/** Fog color to blend with the scene */
	FLinearColor LightColor;
	/** The distance at which light passing through the fog is 100% extinguished. */
	FLOAT ExtinctionDistance;
	/** distance at which fog starts affecting the scene */
	FLOAT StartDistance;

	/** Initialization constructor. */
	FHeightFogSceneInfo(const UHeightFogComponent* InComponent);
};

IMPLEMENT_COMPARE_CONSTREF(FHeightFogSceneInfo,SceneCore,{ return A.Height < B.Height ? +1 : (A.Height > B.Height ? -1 : 0); });

/** The properties of a exponential height fog layer which are used for rendering. */
class FExponentialHeightFogSceneInfo
{
public:

	/** The fog component the scene info is for. */
	const UExponentialHeightFogComponent* Component;
	FLOAT FogHeight;
	FLOAT FogDensity;
	FLOAT FogHeightFalloff;
	FLOAT FogMaxOpacity;
	FLOAT StartDistance;
	FLOAT LightTerminatorAngle;
	FLinearColor OppositeLightColor;
	FLinearColor LightInscatteringColor;

	/** Initialization constructor. */
	FExponentialHeightFogSceneInfo(const UExponentialHeightFogComponent* InComponent);
};
