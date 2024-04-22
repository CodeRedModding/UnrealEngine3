/*=============================================================================
	PrimitiveComponent.h: Primitive component definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Forward declarations.
class FDecalInteraction;
class FDecalRenderData;
class FPrimitiveSceneInfo;
class UDecalComponent;

/**
 * Encapsulates the data which is mirrored to render a primitive parallel to the game thread.
 */
class FPrimitiveSceneProxy
{
public:

	/** optional flags used for DrawDynamicElements */
	enum EDrawDynamicElementFlags
	{		
	};

	/** Initialization constructor. */
	FPrimitiveSceneProxy(const UPrimitiveComponent* InComponent, FName ResourceName = NAME_None);

	/** Virtual destructor. */
	virtual ~FPrimitiveSceneProxy();

	/**
	 * Creates the hit proxies are used when DrawDynamicElements is called.
	 * Called in the game thread.
	 * @param OutHitProxies - Hit proxes which are created should be added to this array.
	 * @return The hit proxy to use by default for elements drawn by DrawDynamicElements.
	 */
	virtual HHitProxy* CreateHitProxies(const UPrimitiveComponent* Component,TArray<TRefCountPtr<HHitProxy> >& OutHitProxies);

	/**
	 * Draws the primitive's static elements.  This is called from the game thread once when the scene proxy is created.
	 * The static elements will only be rendered if GetViewRelevance declares static relevance.
	 * Called in the game thread.
	 * @param PDI - The interface which receives the primitive elements.
	 */
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) {}

	/**
	 * Draws the primitive's dynamic elements.  This is called from the rendering thread for each frame of each view.
	 * The dynamic elements will only be rendered if GetViewRelevance declares dynamic relevance.
	 * Called in the rendering thread.
	 * @param PDI - The interface which receives the primitive elements.
	 * @param View - The view which is being rendered.
	 * @param InDepthPriorityGroup - The DPG which is being rendered.
	 * @param Flags - optional set of flags from EDrawDynamicElementFlags
	 */
	virtual void DrawDynamicElements(
		FPrimitiveDrawInterface* PDI,
		const FSceneView* View,
		UINT InDepthPriorityGroup,
		DWORD Flags=0
		) {}

	/**
	 * Adds a decal interaction to the primitive.  This is called in the rendering thread by AddDecalInteraction_GameThread.
	 */
	virtual void AddDecalInteraction_RenderingThread(const FDecalInteraction& DecalInteraction);
	/**
	 * Adds a decal interaction to the primitive.  This is called in the rendering thread by AddDecalInteraction_GameThread.
	 * @param NewDecalInteraction - New interaction created from the template
	 * @param DecalType - returns if the decal was added to dynamic or static lists
	 */
	void AddDecalInteraction_Internal_RenderingThread(FDecalInteraction* NewDecalInteraction, INT& DecalType);

	/**
	 * Adds a decal interaction to the primitive.  This simply sends a message to the rendering thread to call AddDecalInteraction_RenderingThread.
	 * This is called in the game thread as new decal interactions are created.
	 */
	void AddDecalInteraction_GameThread(const FDecalInteraction& DecalInteraction);

	/**
	 * Removes a decal interaction from the primitive.  This is called in the rendering thread by RemoveDecalInteraction_GameThread.
	 */
	virtual void RemoveDecalInteraction_RenderingThread(UDecalComponent* DecalComponent);

	/**
	* Removes a decal interaction from the primitive.  This simply sends a message to the rendering thread to call RemoveDecalInteraction_RenderingThread.
	* This is called in the game thread when a decal is detached from a primitive which has been added to the scene.
	*/
	void RemoveDecalInteraction_GameThread(UDecalComponent* DecalComponent);

	/**
	 * Updates selection for the primitive proxy. This is called in the rendering thread by SetSelection_GameThread.
	 * @param bInSelected - TRUE if the parent actor is selected in the editor
	 */
	void SetSelection_RenderThread(const UBOOL bInSelected);

	/**
	 * Updates selection for the primitive proxy. This simply sends a message to the rendering thread to call SetSelection_RenderThread.
	 * This is called in the game thread as selection is toggled.
	 * @param bInSelected - TRUE if the parent actor is selected in the editor
	 */
	void SetSelection_GameThread(const UBOOL bInSelected);

    /**
     * Updates hover state for the primitive proxy. This is called in the rendering thread by SetHovered_GameThread.
     * @param bInHovered - TRUE if the parent actor is hovered
     */
    void SetHovered_RenderThread(const UBOOL bInHovered);
    
    /**
     * Updates hover state for the primitive proxy. This simply sends a message to the rendering thread to call SetHovered_RenderThread.
     * This is called in the game thread as hover state changes
     * @param bInHovered - TRUE if the parent actor is hovered
     */
    void SetHovered_GameThread(const UBOOL bInHovered);

	/**
	 * Updates the hidden editor view visibility map on the game thread which just enqueues a command on the render thread
	 */
	void SetHiddenEdViews_GameThread( QWORD InHiddenEditorViews );

	/**
	 * Updates the hidden editor view visibility map on the render thread 
	 */
	void SetHiddenEdViews_RenderThread( QWORD InHiddenEditorViews );

	/** 
	* Rebuilds the static mesh elements for decals that have a missing FStaticMesh* entry for their interactions
	* only called on the game thread
	*/
	void BuildMissingDecalStaticMeshElements_GameThread();

	/**
	* Rebuilds the static mesh elements for decals that have a missing FStaticMesh* entry for their interactions
	* enqued by BuildMissingDecalStaticMeshElements_GameThread on the render thread
	*/
	virtual void BuildMissingDecalStaticMeshElements_RenderThread();

	/**
	* Draws the primitive's static decal elements.  This is called from the game thread whenever this primitive is attached
	* as a receiver for a decal.
	*
	* The static elements will only be rendered if GetViewRelevance declares both static and decal relevance.
	* Called in the game thread.
	*
	* @param PDI - The interface which receives the primitive elements.
	*/
	virtual void DrawStaticDecalElements(FStaticPrimitiveDrawInterface* PDI,const FDecalInteraction& DecalInteraction) {}

	/**
	 * Draws the primitive's dynamic decal elements.  This is called from the rendering thread for each frame of each view.
	 * The dynamic elements will only be rendered if GetViewRelevance declares dynamic relevance.
	 * Called in the rendering thread.
	 *
	 * @param	PDI						The interface which receives the primitive elements.
	 * @param	View					The view which is being rendered.
	 * @param	InDepthPriorityGroup	The DPG which is being rendered.
	 * @param	bDynamicLightingPass	TRUE if drawing dynamic lights, FALSE if drawing static lights.
	 * @param	bDrawOpaqueDecals		TRUE if we want to draw opaque decals
	 * @param	bDrawTransparentDecals	TRUE if we want to draw transparent decals
	 * @param	bTranslucentReceiverPass	TRUE during the decal pass for translucent receivers, FALSE for opaque receivers.
	 */
	virtual void DrawDynamicDecalElements(
		FPrimitiveDrawInterface* PDI,
		const FSceneView* View,
		UINT InDepthPriorityGroup,
		UBOOL bDynamicLightingPass,
		UBOOL bDrawOpaqueDecals,
		UBOOL bDrawTransparentDecals,
		UBOOL bTranslucentReceiverPass
		) {}

	/** Returns the LOD that the primitive will render at for this view. */
	virtual INT GetLOD(const FSceneView* View) const { return INDEX_NONE; }

	/**
	 * Determines the relevance of this primitive's elements to the given view.
	 * Called in the rendering thread.
	 * @param View - The view to determine relevance for.
	 * @return The relevance of the primitive's elements to the view.
	 */
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View);

	/**
	 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
	 *  Only called for primitives that are visible and have bDynamicRelevance
 	 *
	 *	@param	ViewFamily		The ViewFamily to pre-render for
	 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
	 *	@param	FrameNumber		The frame number of this pre-render
	 */
	virtual void PreRenderView(const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber) {}

	/**
	 *	Determines the relevance of this primitive's elements to the given light.
	 *	@param	LightSceneInfo			The light to determine relevance for
	 *	@param	bDynamic (output)		The light is dynamic for this primitive
	 *	@param	bRelevant (output)		The light is relevant for this primitive
	 *	@param	bLightMapped (output)	The light is light mapped for this primitive
	 */
	virtual void GetLightRelevance(const FLightSceneInfo* LightSceneInfo, UBOOL& bDynamic, UBOOL& bRelevant, UBOOL& bLightMapped) const
	{
		// Determine the lights relevance to the primitive.
		bDynamic = TRUE;
		bRelevant = TRUE;
		bLightMapped = FALSE;
	}

	/**
	 * @return		TRUE if the primitive has decals with lit materials that should be rendered in the given view.
	 */
	UBOOL HasLitDecals(const FSceneView* View) const;

	/**
	 *	Called when the rendering thread adds the proxy to the scene.
	 *	This function allows for generating renderer-side resources.
	 *	Called in the rendering thread.
	 */
	virtual UBOOL CreateRenderThreadResources()
	{
		return TRUE;
	}

	/**
	 * Called by the rendering thread to notify the proxy when a light is no longer
	 * associated with the proxy, so that it can clean up any cached resources.
	 * @param Light - The light to be removed.
	 */
	virtual void OnDetachLight(const FLightSceneInfo* Light)
	{
	}

	/**
	 * Called to notify the proxy when its transform has been updated.
	 * Called in the thread that owns the proxy; game or rendering.
	 */
	virtual void OnTransformChanged()
	{
	}

	/**
	* @return TRUE if the proxy requires occlusion queries
	*/
	virtual UBOOL RequiresOcclusion(const FSceneView* View) const
	{
		return TRUE;
	}

	/**
	 * Updates the primitive proxy's cached transforms, and calls OnUpdateTransform to notify it of the change.
	 * Called in the thread that owns the proxy; game or rendering.
	 * @param InLocalToWorld - The new local to world transform of the primitive.
	 * @param InLocalToWorldDeterminant - The new local to world transform's determinant.
	 */
	void SetTransform(const FMatrix& InLocalToWorld,FLOAT InLocalToWorldDeterminant);

	/** Retrieves object type specific positions. */
	virtual void GetObjectPositionAndScale(const FSceneView& View, FVector& ObjectPostProjectionPosition, FVector& ObjectNDCPosition, FVector4& ObjectMacroUVScales) const 
	{
		ObjectPostProjectionPosition = FVector(0,0,0);
		ObjectNDCPosition = FVector(0,0,0);
		ObjectMacroUVScales = FVector4(0,0,0,0);
	}

	virtual void GetFoliageParameters(FVector& OutFoliageImpluseDirection, FVector4& OutFoliageNormalizedRotationAxisAndAngle) const
	{
		OutFoliageImpluseDirection = FVector(0,0,0);
		OutFoliageNormalizedRotationAxisAndAngle = FVector4(0,0,1,0);
	}

	/** Retrieves object occlusion percentage */
	virtual FLOAT GetOcclusionPercentage(const FSceneView& View) const
	{
		return 1.0f;
	}

	/**
	 * Returns whether the owning actor is movable or not.
	 * @return TRUE if the owning actor is movable
	 */
	UBOOL IsMovable() const
	{
		return bMovable;
	}

	/**
	 * Return whether the proxy is selected
	 */
	UBOOL IsSelected() const
	{
		return bSelected;
	}

	/**
	 * Checks if the primitive is owned by the given actor.
	 * @param Actor - The actor to check for ownership.
	 * @return TRUE if the primitive is owned by the given actor.
	 */
	UBOOL IsOwnedBy(const AActor* Actor) const
	{
		return Owners.FindItemIndex(Actor) != INDEX_NONE;
	}

	/** @return TRUE if the primitive is in different DPGs depending on view. */
	UBOOL HasViewDependentDPG() const
	{
		return bUseViewOwnerDepthPriorityGroup;
	}

	/**
	 * Determines the DPG to render the primitive in regardless of view.
	 * Should only be called if HasViewDependentDPG()==TRUE.
	 */
	BYTE GetStaticDepthPriorityGroup() const
	{
		check(!HasViewDependentDPG());
		return StaticDepthPriorityGroup;
	}

	/**
	 * Determines the DPG to render the primitive in for the given view.
	 * May be called regardless of the result of HasViewDependentDPG.
	 * @param View - The view to determine the primitive's DPG for.
	 * @return The DPG the primitive should be rendered in for the given view.
	 */
	BYTE GetDepthPriorityGroup(const FSceneView* View) const
	{
		return (bUseViewOwnerDepthPriorityGroup && IsOwnedBy(View->ViewActor)) ?
			ViewOwnerDepthPriorityGroup :
			StaticDepthPriorityGroup;
	}

	/** @return The local to world transform of the primitive. */
	const FMatrix& GetLocalToWorld() const
	{
		return LocalToWorld;
	}

	FBoxSphereBounds GetBounds() const;

	/** Every derived class should override these functions */
	virtual DWORD GetMemoryFootprint( void ) const = 0;
	DWORD GetAllocatedSize( void ) const { return( Owners.GetAllocatedSize() ); }

	/** The StaticLighting resolution for this mesh */
	FVector2D GetLightMapResolutionScale() const { return LightMapResolutionScale; }
	UBOOL IsLightMapResolutionPadded() const { return bLightMapResolutionPadded; }
	ELightMapInteractionType GetLightMapType() const { return LightMapType; }

	virtual void SetLightMapResolutionScale(FVector2D& InLightMapResolutionScale)
	{
		LightMapResolutionScale = InLightMapResolutionScale;
	}
	virtual void SetIsLightMapResolutionPadded(UBOOL bInLightMapResolutionPadded)
	{
		bLightMapResolutionPadded = bInLightMapResolutionPadded;
	}
	void SetLightMapType(ELightMapInteractionType InLightMapType)
	{
		LightMapType = InLightMapType;
	}
	inline UBOOL IsHiddenGame() const { return bHiddenGame; }
	inline UBOOL IsHiddenEditor() const { return bHiddenEditor; }

	/**
	 * Determine if the primitive supports motion blur velocity rendering by storing
	 * motion blur transform info at the MeshElement level.
	 *
	 * @return TRUE if the primitive supports motion blur velocity rendering in its generated meshes
	 */
	UBOOL HasMotionBlurVelocityMeshes() const
	{
		return bHasMotionBlurVelocityMeshes;
	}

	/** Whether this primitive relies on occlusion queries for the way it looks, as opposed to just using occlusion for culling. */
	inline UBOOL RequiresOcclusionForCorrectness() const
	{
		return bRequiresOcclusionForCorrectness;
	}

	/**
	 * Returns whether or not this component is instanced.
	 * The base implementation returns FALSE.  You should override this method in derived classes.
	 *
	 * @return	TRUE if this component represents multiple instances of a primitive.
	 */
	virtual UBOOL IsPrimitiveInstanced() const
	{
		return FALSE;
	}


	/**
	 * For instanced components, returns the number of instances.
	 * The base implementation returns zero.  You should override this method in derived classes.
	 *
	 * @return	Number of instances
	 */
	virtual INT GetInstanceCount() const
	{
		return 0;
	}


	/**
	 * For instanced components, returns the Local -> World transform for the specific instance number.
	 * If the function is called on non-instanced components, the component's LocalToWorld will be returned.
	 * You should override this method in derived classes that support instancing.
	 *
	 * @param	InInstanceIndex	The index of the instance to return the Local -> World transform for
	 *
	 * @return	Number of instances
	 */
	virtual const FMatrix& GetInstanceLocalToWorld( INT InInstanceIndex ) const
	{
		return LocalToWorld;
	}

	/**
	 *	Returns whether the proxy utilizes custom occlusion bounds or not
	 *
	 *	@return	UBOOL		TRUE if custom occlusion bounds are used, FALSE if not;
	 */
	virtual UBOOL HasCustomOcclusionBounds() const
	{
		return FALSE;
	}

	/**
	 *	Return the custom occlusion bounds for this scene proxy.
	 *	
	 *	@return	FBoxSphereBounds		The custom occlusion bounds.
	 */
	virtual FBoxSphereBounds GetCustomOcclusionBounds() const
	{
		checkf(FALSE, TEXT("GetCustomOcclusionBounds> Should not be called on base scene proxy!"));
		return GetBounds();
	}

protected:

	/** Pointer back to the PrimitiveSceneInfo that owns this Proxy. */
	class FPrimitiveSceneInfo *	PrimitiveSceneInfo;
	friend class FPrimitiveSceneInfo;

	/** The primitive's local to world transform. */
	FMatrix LocalToWorld;

	/** The determinant of the local to world transform. */
	FLOAT LocalToWorldDeterminant;
public:
	enum
	{
		STATIC_DECALS,
		DYNAMIC_DECALS,
		NUM_DECAL_TYPES,
	};
	/** The decals which interact with the primitive [0 = Static, 1=Dynamic]. */
	TArray<FDecalInteraction*> Decals[NUM_DECAL_TYPES];

	/** The name of the resource used by the component. */
	FName ResourceName;

protected:
	/** @return True if the primitive is visible in the given View. */
	UBOOL IsShown(const FSceneView* View) const;
	/** @return True if the primitive is casting a shadow. */
	UBOOL IsShadowCast(const FSceneView* View) const;

	/** @return True if the primitive has decals with static relevance which should be rendered in the given view. */
	UBOOL HasRelevantStaticDecals(const FSceneView* View) const;
	/** @return True if the primitive has decals with dynamic relevance which should be rendered in the given view. */
	UBOOL HasRelevantDynamicDecals(const FSceneView* View) const;

	/** Helper for components that need to initialize view relevance to handle SHOW_Bounds. */
	void SetRelevanceForShowBounds(EShowFlags ShowFlags, FPrimitiveViewRelevance& ViewRelevance) const;

	/** Helper for components that want to render bounds. */
	void RenderBounds(FPrimitiveDrawInterface* PDI, UINT DPGIndex, EShowFlags ShowFlags, const FBoxSphereBounds& Bounds, UBOOL bRenderInEditor) const;

protected:

	BITFIELD bHiddenGame : 1;
	BITFIELD bHiddenEditor : 1;
	BITFIELD bIsNavigationPoint : 1;
	BITFIELD bOnlyOwnerSee : 1;
	BITFIELD bOwnerNoSee : 1;
	BITFIELD bMovable : 1;
	BITFIELD bSelected : 1;
	
	/** TRUE if the mouse is currently hovered over this primitive in a level viewport */
	BITFIELD bHovered : 1;

	/** TRUE if the LightMapResolutionScale value has been padded. */
	BITFIELD bLightMapResolutionPadded : 1;

	/** TRUE if ViewOwnerDepthPriorityGroup should be used. */
	BITFIELD bUseViewOwnerDepthPriorityGroup : 1;

	/** TRUE if the primitive has motion blur velocity meshes */
	BITFIELD bHasMotionBlurVelocityMeshes : 1;

	/** DPG this prim belongs to. */
	BITFIELD StaticDepthPriorityGroup : UCONST_SDPG_NumBits;

	/** DPG this primitive is rendered in when viewed by its owner. */
	BITFIELD ViewOwnerDepthPriorityGroup : UCONST_SDPG_NumBits;

	/** Whether this proxy relies on occlusion queries for the way it looks, as opposed to just using occlusion for culling. */
	BITFIELD bRequiresOcclusionForCorrectness : 1;

	TArray<const AActor*> Owners;

	/** Maximum distance from viewer that this object should be visible */
	FLOAT MaxDrawDistanceSquared;

	/** The StaticLighting resolution for this mesh */
	FVector2D LightMapResolutionScale;

	/** The LightMap method used by the primitive */
	ELightMapInteractionType LightMapType;

#if !CONSOLE
	/** A copy of the actor's group membership for handling per-view group hiding */
	QWORD HiddenEditorViews;
#endif
};

/** Information about a vertex of a primitive's triangle. */
struct FPrimitiveTriangleVertex
{
	FVector WorldPosition;
	FVector WorldTangentX;
	FVector WorldTangentY;
	FVector WorldTangentZ;
};

/** An interface to some consumer of the primitive's triangles. */
class FPrimitiveTriangleDefinitionInterface
{
public:

	/**
	 * Defines a triangle.
	 * @param Vertex0 - The triangle's first vertex.
	 * @param Vertex1 - The triangle's second vertex.
	 * @param Vertex2 - The triangle's third vertex.
	 * @param StaticLighting - The triangle's static lighting information.
	 */
	virtual void DefineTriangle(
		const FPrimitiveTriangleVertex& Vertex0,
		const FPrimitiveTriangleVertex& Vertex1,
		const FPrimitiveTriangleVertex& Vertex2
		) = 0;
};

//
// FForceApplicator - Callback interface to apply a force field to a component(eg Rigid Body, Cloth, Particle System etc)
// Used by AddForceField.
//

class FForceApplicator
{
public:
	
	/**
	Called to compute a number of forces resulting from a force field.

	@param Positions Array of input positions (in world space)
	@param PositionStride Number of bytes between consecutive position vectors
	@param Velocities Array of inpute velocities
	@param VelocityStride Number of bytes between consecutive velocity vectors
	@param OutForce Output array of force vectors, computed from position/velocity, forces are added to the existing value
	@param OutForceStride Number of bytes between consectutiive output force vectors
	@param Count Number of forces to compute
	@param Scale factor to apply to positions before use(ie positions may not be in unreal units)
	@param PositionBoundingBox Bounding box for the positions passed to the call

	@return TRUE if any force is non zero.
	*/

	virtual UBOOL ComputeForce(
		FVector* Positions, INT PositionStride, FLOAT PositionScale,
		FVector* Velocities, INT VelocityStride, FLOAT VelocityScale,
		FVector* OutForce, INT OutForceStride, FLOAT OutForceScale,
		FVector* OutTorque, INT OutTorqueStride, FLOAT OutTorqueScale,
		INT Count, const FBox& PositionBoundingBox) = 0;
};

/** Information about a streaming texture that a primitive uses for rendering. */
struct FStreamingTexturePrimitiveInfo
{
	UTexture* Texture;
	FSphere Bounds;
	FLOAT TexelFactor;
};

//
//	UPrimitiveComponent
//

class UPrimitiveComponent : public UActorComponent
{
	DECLARE_ABSTRACT_CLASS_NOEXPORT(UPrimitiveComponent,UActorComponent,0,Engine);
public:

	static INT CurrentTag;

	INT Tag;
	FBoxSphereBounds Bounds;

	/** The primitive's scene info. */
	class FPrimitiveSceneInfo* SceneInfo;

	/** A fence to track when the primitive is detached from the scene in the rendering thread. */
	FRenderCommandFence DetachFence;

	FLOAT LocalToWorldDeterminant;
	FMatrix LocalToWorld;
	
	// INDEX_NONE or index into MotionBlurInfoArray
	INT MotionBlurInfoIndex;

	/** Current list of active decals attached to the primitive */
	TArray<FDecalInteraction*> DecalList;
	/** Decals that are detached from the primitive and need to be reattached */
	TArray<UDecalComponent*> DecalsToReattach;

	UPrimitiveComponent* ShadowParent;

	/** Replacement primitive to draw instead of this one (multiple UPrim's will point to the same Replacement) */
	UPrimitiveComponent* ReplacementPrimitive;

	/** Keeps track of which fog component this primitive is using. */
	class UFogVolumeDensityComponent* FogVolumeComponent;

	/** If specified, only OverrideLightComponent can affect the primitive. */
	ULightComponent* OverrideLightComponent;

	/** The lighting environment to take the primitive's lighting from. */
	class ULightEnvironmentComponent* LightEnvironment;

private:
	/** Stores the previous light environment if SetLightEnvironment is called while the primitive is attached, so that Detach can notify the previous light environment correctly. */
	ULightEnvironmentComponent* PreviousLightEnvironment;

public:
	/**
	 * The minimum distance at which the primitive should be rendered, 
	 * measured in world space units from the center of the primitive's bounding sphere to the camera position.
	 */
	FLOAT MinDrawDistance;

	/**
	 * The distance at which the renderer will switch from parent (low LOD) to children (high LOD).
	 * This is basically the same as MinDrawDistance, except that the low LOD will draw even up close, if there are no children.
	 * This is needed so the high lod meshes can be in a streamable sublevel, and if streamed out, the low LOD will draw up close.
	 */
	FLOAT MassiveLODDistance;

	/** 
	 * Max draw distance exposed to LDs. The real max draw distance is the min (disregarding 0) of this and volumes affecting this object. 
	 * This is renamed to LDMaxDrawDistance in c++
	 */
	FLOAT LDMaxDrawDistance;

	/**
	 * The distance to cull this primitive at.  
	 * A CachedMaxDrawDistance of 0 indicates that the primitive should not be culled by distance.
	 */
	FLOAT CachedMaxDrawDistance;

	/**
	 * Scalar controlling the amount of motion blur to be applied when object moves.
	 * 0=object motion blur off, 1=full motion blur(default), value should be 0 or bigger
	 */
	FLOAT MotionBlurInstanceScale;

	/** Legacy, renamed to LDMaxDrawDistance */
	FLOAT LDCullDistance;
	/** Legacy, renamed to CachedMaxDrawDistance */
	FLOAT CachedCullDistance_DEPRECATED;

	/** The scene depth priority group to draw the primitive in. */
	BYTE DepthPriorityGroup;

	/** The scene depth priority group to draw the primitive in, if it's being viewed by its owner. */
	BYTE ViewOwnerDepthPriorityGroup;

	/** If detail mode is >= system detail mode, primitive won't be rendered. */
	BYTE DetailMode;
	
	/** Enum indicating what type of object this should be considered for rigid body collision. */
	BYTE		RBChannel;

	/** 
	*	Used for creating one-way physics interactions (via constraints or contacts) 
	*	Groups with lower RBDominanceGroup push around higher values in a 'one way' fashion. Must be <32.
	*/
	BYTE		RBDominanceGroup;

	/** Environment shadow factor used when previewing unbuilt lighting on this primitive. */
	BYTE		PreviewEnvironmentShadowing;

	SCRIPT_ALIGN;

	/** True if the primitive should be rendered using ViewOwnerDepthPriorityGroup if viewed by its owner. */
	BITFIELD	bUseViewOwnerDepthPriorityGroup:1;

	/** Whether to accept cull distance volumes to modify cached cull distance. */
	BITFIELD	bAllowCullDistanceVolume:1;

	BITFIELD	HiddenGame:1;
	BITFIELD	HiddenEditor:1;

	/** If this is True, this component won't be visible when the view actor is the component's owner, directly or indirectly. */
	BITFIELD	bOwnerNoSee:1;

	/** If this is True, this component will only be visible when the view actor is the component's owner, directly or indirectly. */
	BITFIELD	bOnlyOwnerSee:1;

	/** If true, bHidden on the Owner of this component will be ignored. */
	BITFIELD	bIgnoreOwnerHidden : 1;

	/** If this is True, this primitive will be used to occlusion cull other primitives. */
	BITFIELD	bUseAsOccluder:1;

	/** If this is True, this component doesn't need exact occlusion info. */
	BITFIELD	bAllowApproximateOcclusion:1;

	/** If this is True, the component will return 'occluded' for the first frame. */
	BITFIELD	bFirstFrameOcclusion:1;

	/** If True, this component will still be queried for occlusion even when it intersects the near plane. */
	BITFIELD	bIgnoreNearPlaneIntersection:1;

	/** If this is True, this component can be selected in the editor. */
	BITFIELD	bSelectable:1;

	/** If TRUE, forces mips for textures used by this component to be resident when this component's level is loaded. */
	BITFIELD     bForceMipStreaming:1;

	/** deprecated */
	BITFIELD	bAcceptsDecals:1;

	/** deprecated */
	BITFIELD	bAcceptsDecalsDuringGameplay:1;

	/** If TRUE, this primitive accepts static level placed decals in the editor. */
	BITFIELD	bAcceptsStaticDecals:1;

	/** If TRUE, this primitive accepts dynamic decals spawned during gameplay.  */
	BITFIELD	bAcceptsDynamicDecals:1;

	BITFIELD	bIsRefreshingDecals:1;

	BITFIELD	bAllowDecalAutomaticReAttach:1;

	/** If true a hit-proxy will be generated for each instance of instanced static meshes */
	BITFIELD	bUsePerInstanceHitProxies:1;

	// Lighting flags

	BITFIELD	CastShadow:1;

	/** If true, forces all static lights to use light-maps for direct lighting on this primitive, regardless of the light's UseDirectLightMap property. */
	BITFIELD	bForceDirectLightMap : 1;
	
	/** If true, primitive casts dynamic shadows. */
	BITFIELD	bCastDynamicShadow : 1;

	/** Whether the primitive casts static shadows. */
	BITFIELD	bCastStaticShadow : 1;

	/** If true, primitive only self shadows and does not cast shadows on other primitives. */
	BITFIELD	bSelfShadowOnly : 1;

	/** 
	 * For mobile platforms only! If true, the primitive will not receive projected mod shadows, not from itself nor any other mod shadow caster. 
	 * This can be used to avoid self-shadowing artifacts.
	 */
	BITFIELD	bNoModSelfShadow : 1;

	/** 
	 * Optimization for objects which don't need to receive dynamic dominant light shadows. 
	 * This is useful for objects which eat up a lot of GPU time and are heavily texture bound yet never receive noticeable shadows from dominant lights like trees.
	 */
	BITFIELD	bAcceptsDynamicDominantLightShadows:1;
	/** If TRUE, primitive will cast shadows even if bHidden is TRUE. */
	BITFIELD	bCastHiddenShadow:1;

	/** Whether this primitive should cast dynamic shadows as if it were a two sided material. */
	BITFIELD	bCastShadowAsTwoSided:1;
	
	BITFIELD	bAcceptsLights:1;
	
	/** Whether this primitives accepts dynamic lights */
	BITFIELD	bAcceptsDynamicLights:1;

	/** 
	 * If TRUE, lit translucency using this light environment will render in one pass, 
	 * Which is cheaper and ensures correct blending but approximates lighting using one directional light and all other lights in an unshadowed SH environment.
	 * If FALSE, lit translucency will render in multiple passes which uses more shader instructions and results in incorrect blending.
	 * Both settings still work correctly with bAllowDynamicShadowsOnTranslucency.
	 */
	BITFIELD bUseOnePassLightingOnTranslucency : 1;

	/** Whether the primitive supports/ allows static shadowing */
	BITFIELD	bUsePrecomputedShadows:1;

private:
	/** 
	 * TRUE if ShadowParent was set through SetShadowParent, 
	 * FALSE if ShadowParent is set automatically based on Owner->bShadowParented.
	 */
	BITFIELD	bHasExplicitShadowParent:1;

public:

	/**
	* Controls whether ambient occlusion should be allowed on or from this primitive, only has an effect on movable primitives.
	* Note that setting this flag to FALSE will negatively impact performance.
	*/
	BITFIELD	bAllowAmbientOcclusion:1;

	// Collision flags.

	BITFIELD	CollideActors:1;
	BITFIELD	AlwaysCheckCollision:1;
	BITFIELD	BlockActors:1;
	BITFIELD	BlockZeroExtent:1;
	BITFIELD	BlockNonZeroExtent:1;
	BITFIELD	CanBlockCamera:1;
	BITFIELD	BlockRigidBody:1;
	/** If TRUE will block foot placement line checks (default). FALSE will skip right through. */
	BITFIELD	bBlockFootPlacement:1;

	/** Never create any physics engine representation for this body. */
	BITFIELD	bDisableAllRigidBody:1;

	/** When creating rigid body, will skip normal geometry creation step, and will rely on ModifyNxActorDesc to fill in geometry. */
	BITFIELD	bSkipRBGeomCreation:1;

	/** Flag that indicates if OnRigidBodyCollision function should be called for physics collisions involving this PrimitiveComponent. */
	BITFIELD	bNotifyRigidBodyCollision:1;

	// Novodex fluids
	BITFIELD	bFluidDrain:1;
	BITFIELD	bFluidTwoWay:1;

	BITFIELD	bIgnoreRadialImpulse:1;
	BITFIELD	bIgnoreRadialForce:1;

	/** Disables the influence from ALL types of force fields. */
	BITFIELD	bIgnoreForceField:1;

	
	/** Place into a NxCompartment that will run in parallel with the primary scene's physics with potentially different simulation parameters.
	 *  If double buffering is enabled in the WorldInfo then physics will run in parallel with the entire game for this component. */
	BITFIELD	bUseCompartment:1;	// hardware scene support

	BITFIELD	AlwaysLoadOnClient:1;
	BITFIELD	AlwaysLoadOnServer:1;

	BITFIELD	bIgnoreHiddenActorsMembership:1;

	BITFIELD	AbsoluteTranslation:1;
	BITFIELD	AbsoluteRotation:1;
	BITFIELD	AbsoluteScale:1;

	/** Determines whether or not we allow shadowing fading.  Some objects (especially in cinematics) having the shadow fade/pop out looks really bad. **/
	BITFIELD	bAllowShadowFade:1;

	/** Whether or not this primitive type is supported on mobile. For the emulate mobile rendering editor feature. */
	BITFIELD	bSupportedOnMobile:1;

	BITFIELD							bWasSNFiltered:1;
	TArrayNoInit<class FOctreeNode*>	OctreeNodes;
	

	/** 
	* Translucent objects with a lower sort priority draw before objects with a higher priority.
	* Translucent objects with the same priority are rendered from back-to-front based on their bounds origin.
	*
	* Ignored if the object is not translucent.
	* The default priority is zero. 
	**/
	INT TranslucencySortPriority;

	/** Used for precomputed visibility */
	INT VisibilityId;

	/** Lighting channels controlling light/ primitive interaction. Only allows interaction if at least one channel is shared */
	FLightingChannelContainer	LightingChannels;

	/** Types of objects that this physics objects will collide with. */
	FRBCollisionChannelContainer RBCollideWithChannels;





	class UPhysicalMaterial*	PhysMaterialOverride;
	class URB_BodyInstance*		BodyInstance;

	// Copied from TransformComponent
	FMatrix CachedParentToWorld;
	FVector		Translation;
	FRotator	Rotation;
	FLOAT		Scale;
	FVector		Scale3D;
	FLOAT		BoundsScale;
	/** Last time the component was submitted for rendering (called FScene::AddPrimitive). */
	FLOAT		LastSubmitTime;

	/** Last render time in seconds since level started play. Updated to WorldInfo->TimeSeconds so float is sufficient. */
	FLOAT		LastRenderTime;

	/** if > 0, the script RigidBodyCollision() event will be called on our Owner when a physics collision involving
	 * this PrimitiveComponent occurs and the relative velocity is greater than or equal to this
	 */
	FLOAT ScriptRigidBodyCollisionThreshold;

	// Should this Component be in the Octree for collision
	UBOOL ShouldCollide() const;

	void AttachDecal(class UDecalComponent* Decal, class FDecalRenderData* RenderData, const class FDecalState* DecalState);
	void DetachDecal(class UDecalComponent* Decal);

	/** Creates the render data for a decal on this primitive. */
	virtual void GenerateDecalRenderData(class FDecalState* Decal, TArray< FDecalRenderData* >& OutDecalRenderDatas) const;

	/**
	 * Returns True if a primitive cannot move or be destroyed during gameplay, and can thus cast and receive static shadowing.
	 */
	FORCEINLINE UBOOL HasStaticShadowing() const
	{
		return bUsePrecomputedShadows;
	}

	/**
	 * Returns whether this primitive should render selection.
	 *
	 * @return TRUE if the owner is selected and this component is selectable
	 */
	virtual UBOOL ShouldRenderSelected() const
	{
		return IsOwnerSelected() && bSelectable;
	}

	/**
	 * Returns whether this primitive only uses unlit materials.
	 *
	 * @return TRUE if only unlit materials are used for rendering, false otherwise.
	 */
	virtual UBOOL UsesOnlyUnlitMaterials() const;

	/**
	 * Returns the lightmap resolution used for this primitive instance in the case of it supporting texture light/ shadow maps.
	 * 0 if not supported or no static shadowing.
	 *
	 * @param	Width	[out]	Width of light/shadow map
	 * @param	Height	[out]	Height of light/shadow map
	 *
	 * @return	UBOOL			TRUE if LightMap values are padded, FALSE if not
	 */
	virtual UBOOL GetLightMapResolution( INT& Width, INT& Height ) const;

	/**
	 *	Returns the static lightmap resolution used for this primitive.
	 *	0 if not supported or no static shadowing.
	 *
	 * @return	INT		The StaticLightmapResolution for the component
	 */
	virtual INT GetStaticLightMapResolution() const { return 0; }

	/**
	 * Returns the light and shadow map memory for this primite in its out variables.
	 *
	 * Shadow map memory usage is per light whereof lightmap data is independent of number of lights, assuming at least one.
	 *
	 * @param [out] LightMapMemoryUsage		Memory usage in bytes for light map (either texel or vertex) data
	 * @param [out]	ShadowMapMemoryUsage	Memory usage in bytes for shadow map (either texel or vertex) data
	 */
	virtual void GetLightAndShadowMapMemoryUsage( INT& LightMapMemoryUsage, INT& ShadowMapMemoryUsage ) const;

	/**
	 * Validates the lighting channels and makes adjustments as appropriate.
	 */
	void ValidateLightingChannels();

	/**
	 * Called when this actor component has moved, allowing it to discard statically cached lighting information.
	 */
	virtual void InvalidateLightingCache() 
	{
		VisibilityId = INDEX_NONE;
	}

	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

	/**
	 * Sets Bounds.  Default behavior is a bounding box/sphere the size of the world.
	 */
	virtual void UpdateBounds();

	/**
	 * Requests the information about the component that the static lighting system needs.
	 * @param OutPrimitiveInfo - Upon return, contains the component's static lighting information.
	 * @param InRelevantLights - The lights relevant to the primitive.
	 * @param InOptions - The options for the static lighting build.
	 */
	virtual void GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options) {}

	/**
	 *	Requests whether the component will use texture, vertex or no lightmaps.
	 *
	 *	@return	ELightMapInteractionType		The type of lightmap interaction the component will use.
	 */
	virtual ELightMapInteractionType GetStaticLightingType() const	{ return LMIT_None;	}

	/**
	 * Gets the primitive's static triangles.
	 * @param PTDI - An implementation of the triangle definition interface.
	 */
	virtual void GetStaticTriangles(FPrimitiveTriangleDefinitionInterface* PTDI) const {}

	/**
	 * Enumerates the streaming textures used by the primitive.
	 * @param OutStreamingTextures - Upon return, contains a list of the streaming textures used by the primitive.
	 */
	virtual void GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
	{}

	/**
	 * Determines the DPG the primitive's primary elements are drawn in.
	 * Even if the primitive's elements are drawn in multiple DPGs, a primary DPG is needed for occlusion culling and shadow projection.
	 * @return The DPG the primitive's primary elements will be drawn in.
	 */
	virtual BYTE GetStaticDepthPriorityGroup() const
	{
		return DepthPriorityGroup;
	}

	/** 
	 * Retrieves the materials used in this component 
	 * 
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const {}

	/**
	 * Returns the material textures used to render this primitive for the given quality level
	 * Internally calls GetUsedMaterials() and GetUsedTextures() for each material.
	 *
	 * @param OutTextures	[out] The list of used textures.
	 * @param Quality		The platform to get material textures for. If unspecified, it will get textures for current SystemSetting
	 * @param bAllQualities	Whether to iterate for all platforms. The Platform parameter is ignored if this is set to TRUE.
	 */
	void GetUsedTextures(TArray<UTexture*> &OutTextures, EMaterialShaderQuality Quality=MSQ_UNSPECIFIED, UBOOL bAllQualities=FALSE);

	// Collision tests.

	virtual UBOOL PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags) { return 1; }
	virtual UBOOL LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags) { return 1; }

	FVector GetOrigin() const
	{
		return LocalToWorld.GetOrigin();
	}

	/** Removes any scaling from the LocalToWorld matrix and returns it, along with the overall scaling. */
	void GetTransformAndScale(FMatrix& OutTransform, FVector& OutScale);

	/** Returns true if this component is always static*/
	virtual UBOOL IsAlwaysStatic();

	// Rigid-Body Physics
	virtual void InitComponentRBPhys(UBOOL bFixed);
	virtual void SetComponentRBFixed(UBOOL bFixed);
	virtual void TermComponentRBPhys(FRBPhysScene* InScene);

	/** Return the BodySetup to use for this PrimitiveComponent (single body case) */
	virtual class URB_BodySetup* GetRBBodySetup() { return NULL; }

	/** Returns any pre-cooked convex mesh data associated with this PrimitiveComponent. Used by InitBody. */
	virtual FKCachedConvexData* GetCachedPhysConvexData(const FVector& InScale3D) { return NULL; }

	/** 
	 * Returns any pre-cooked convex mesh data associated with this PrimitiveComponent.
	 * UPrimitiveComponent calls GetCachedPhysConvexData(InScale3D).
	 * USkeletalMeshComponent returns data associated with the specified bone.
	 * Used by InitBody. 
	 */
	virtual FKCachedConvexData* GetBoneCachedPhysConvexData(const FVector& InScale3D, const FName& BoneName) 
	{
		// Even if (BoneName != NAME_None) we call GetCachedPhysConvexData(InScale3D) 
		// to be fully compatible with existing behavior.
		return GetCachedPhysConvexData(InScale3D);
	}

	class URB_BodySetup* FindRBBodySetup();
	virtual void AddImpulse(FVector Impulse, FVector Position = FVector(0,0,0), FName BoneName = NAME_None, UBOOL bVelChange=false);
	virtual void AddRadialImpulse(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff, UBOOL bVelChange=false);
	void AddForce(FVector Force, FVector Position = FVector(0,0,0), FName BoneName = NAME_None);
	virtual void AddRadialForce(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff);
	void AddTorque(FVector Torque, FName BoneName = NAME_None);

	/** Applies the affect of a force field to this primitive component. */
	virtual void AddForceField(FForceApplicator* Applicator, const FBox& FieldBoundingBox, UBOOL bApplyToCloth, UBOOL bApplyToRigidBody);

	virtual void SetRBLinearVelocity(const FVector& NewVel, UBOOL bAddToCurrent=false);
	virtual void SetRBAngularVelocity(const FVector& NewAngVel, UBOOL bAddToCurrent=false);
	virtual void RetardRBLinearVelocity(const FVector& RetardDir, FLOAT VelScale);
	virtual void SetRBPosition(const FVector& NewPos, FName BoneName = NAME_None);
	virtual void SetRBRotation(const FRotator& NewRot, FName BoneName = NAME_None);
	virtual void WakeRigidBody( FName BoneName = NAME_None );
	virtual void PutRigidBodyToSleep(FName BoneName = NAME_None);
	virtual UBOOL RigidBodyIsAwake( FName BoneName = NAME_None );
	virtual void SetBlockRigidBody(UBOOL bNewBlockRigidBody);
	void SetRBCollidesWithChannel(ERBCollisionChannel Channel, UBOOL bNewCollides);
	void SetRBCollisionChannels(FRBCollisionChannelContainer Channels);
	void SetRBChannel(ERBCollisionChannel Channel);
	virtual void SetNotifyRigidBodyCollision(UBOOL bNewNotifyRigidBodyCollision);
	
	/** 
	 *	Used for creating one-way physics interactions.
	 *	@see RBDominanceGroup
	 */
	virtual void SetRBDominanceGroup(BYTE InDomGroup);

	/** 
	 *	Changes the current PhysMaterialOverride for this component. 
	 *	Note that if physics is already running on this component, this will _not_ alter its mass/inertia etc, it will only change its 
	 *	surface properties like friction and the damping.
	 */
	virtual void SetPhysMaterialOverride(UPhysicalMaterial* NewPhysMaterial);

	/** returns the physics RB_BodyInstance for the root body of this component (if any) */
	virtual URB_BodyInstance* GetRootBodyInstance();

	virtual void UpdateRBKinematicData();

	/**		**INTERNAL USE ONLY**
	* Implementation required by a primitive component in order to properly work with the closest points algorithms below
	* Given an interface to some other primitive, return the points on each object closest to each other
	* @param ExtentHelper - Interface class returning the supporting points on some other primitive type
	* @param OutPointA - The point closest on the 'other' primitive
	* @param OutPointB - The point closest on this primitive
	* 
	* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
	*/
	virtual /*GJKResult*/ BYTE ClosestPointOnComponentInternal(IGJKHelper* ExtentHelper, FVector& OutPointA, FVector& OutPointB) { return GJK_Fail; }

	/**
	* Calculates the closest point on this primitive to a point given
	* @param POI - Point in world space to determine closest point to
	* @param Extent - Convex primitive 
	* @param OutPointA - The point closest on the extent box
	* @param OutPointB - Point on this primitive closest to the extent box
	* 
	* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
	*/
	/*GJKResult*/ BYTE ClosestPointOnComponentToPoint(const FVector& POI, const FVector& Extent, FVector& OutPointA, FVector& OutPointB);

	/**
	* Calculates the closest point this component to another component
	* @param PrimitiveComponent - Another Primitive Component
	* @param PointOnComponentA - Point on this primitive closest to other primitive
	* @param PointOnComponentB - Point on other primitive closest to this primitive
	* 
	* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
	*/
	virtual /*GJKResult*/ BYTE ClosestPointOnComponentToComponent(class UPrimitiveComponent*& OtherComponent,FVector& PointOnComponentA,FVector& PointOnComponentB);

	// Copied from TransformComponent
	virtual void SetTransformedToWorld();


	/**
	 * Returns whether or not this component is instanced.
	 * The base implementation returns FALSE.  You should override this method in derived classes.
	 *
	 * @return	TRUE if this component represents multiple instances of a primitive.
	 */
	virtual UBOOL IsPrimitiveInstanced() const
	{
		return FALSE;
	}


	/**
	 * For instanced components, returns the number of instances.
	 * The base implementation returns zero.  You should override this method in derived classes.
	 *
	 * @return	Number of instances
	 */
	virtual INT GetInstanceCount() const
	{
		return 0;
	}


	/**
	 * For instanced components, returns the Local -> World transform for the specific instance number.
	 * If the function is called on non-instanced components, the component's LocalToWorld will be returned.
	 * You should override this method in derived classes that support instancing.
	 *
	 * @param	InInstanceIndex	The index of the instance to return the Local -> World transform for
	 *
	 * @return	Number of instances
	 */
	virtual const FMatrix GetInstanceLocalToWorld( INT InInstanceIndex ) const
	{
		return LocalToWorld;
	}


	/**
	 * Test overlap against another primitive component, uses a box for the smaller primcomp
	 * @param Other - the other primitive component to test against 
	 * @param Hit   - the hit result to add results to
	 * @param OverlapAdjust - offset to use for testing against a position this primitive is not currently at
	 * @param bCollideComplex - whether to use complex collision when pointchecking against this
	 * @param bOtherCollideComplex - whether to use complex collision when point checking against other
	 */
	FORCEINLINE UBOOL IsOverlapping(UPrimitiveComponent* Other, FCheckResult* Hit, const FVector& OverlapAdjust, DWORD MyTraceFlags, DWORD OtherTraceFlags)
	{
		FBox OtherBox = Other->Bounds.GetBox();
		FBox MyBox = Bounds.GetBox();

		// worth it to use the smaller?  maybe should just always use one or the other..
		if(OtherBox.GetVolume() < MyBox.GetVolume())
		{
			FVector OtherBoxCenter, OtherBoxExtent;
			// offset other's box along the inverse of our offset since we're not using a box for ourselves
			OtherBox.Min -= OverlapAdjust;
			OtherBox.Max -= OverlapAdjust;
			OtherBox.GetCenterAndExtents(OtherBoxCenter,OtherBoxExtent);
	
			if( PointCheck(*Hit, OtherBoxCenter, OtherBoxExtent, OtherTraceFlags) == 0 )
			{
				Hit->Component = Other;
				Hit->SourceComponent = this;
				return TRUE;
			}
		}
		else
		{
			FVector MyBoxCenter, MyBoxExtent;
			// adjust our box with the overlapadjustment
			MyBox.Min += OverlapAdjust;
			MyBox.Max += OverlapAdjust;
			MyBox.GetCenterAndExtents(MyBoxCenter,MyBoxExtent);
			
			if( Other->PointCheck(*Hit, MyBoxCenter, MyBoxExtent, MyTraceFlags) == 0 )
			{
				Hit->Component = Other;
				Hit->SourceComponent = this;
				return TRUE;
			}
		}

		return FALSE;
	}

	/** allows components with 'AlwaysCheckCollision' set to TRUE to override trace flags during collision testing */
	virtual void OverrideTraceFlagsForNonCollisionComponentChecks( DWORD& Flags ){/*default to do nothing*/}

	/** @return number of material elements in this primitive */
	virtual INT GetNumElements() const
	{
		return 0;
	}
	/** @return MaterialInterface assigned to the given material index (if any) */
	virtual UMaterialInterface* GetElementMaterial(INT ElementIndex) const
	{
		return NULL;
	}
	/** sets the MaterialInterface to use for the given element index (if valid) */
	virtual void SetElementMaterial(INT ElementIndex, UMaterialInterface* InMaterial)
	{
	}

	/**
	 *  Retrieve various actor metrics depending on the provided type.  All of
	 *  these will total the values for this component.
	 *
	 *  @param MetricsType The type of metric to calculate.
	 *
	 *  METRICS_VERTS    - Get the number of vertices.
	 *  METRICS_TRIS     - Get the number of triangles.
	 *  METRICS_SECTIONS - Get the number of sections.
	 *
	 *  @return INT The total of the given type for this component.
	 */
	virtual INT GetActorMetrics(EActorMetricsType MetricsType)
	{
		return 0;
	}

	DECLARE_FUNCTION(execAddImpulse);
	DECLARE_FUNCTION(execAddRadialImpulse);
	DECLARE_FUNCTION(execAddForce);
	DECLARE_FUNCTION(execAddRadialForce);
	DECLARE_FUNCTION(execAddTorque);
	DECLARE_FUNCTION(execSetRBLinearVelocity);
	DECLARE_FUNCTION(execSetRBAngularVelocity);
	DECLARE_FUNCTION(execRetardRBLinearVelocity);
	DECLARE_FUNCTION(execSetRBPosition);
	DECLARE_FUNCTION(execSetRBRotation);
	DECLARE_FUNCTION(execWakeRigidBody);
	DECLARE_FUNCTION(execPutRigidBodyToSleep);
	DECLARE_FUNCTION(execRigidBodyIsAwake);
	DECLARE_FUNCTION(execSetBlockRigidBody);
	DECLARE_FUNCTION(execSetRBCollidesWithChannel);
	DECLARE_FUNCTION(execSetRBCollisionChannels);
	DECLARE_FUNCTION(execSetRBChannel);
	DECLARE_FUNCTION(execSetNotifyRigidBodyCollision);
	DECLARE_FUNCTION(execInitRBPhys);
	DECLARE_FUNCTION(execSetPhysMaterialOverride);
	DECLARE_FUNCTION(execSetRBDominanceGroup);

	DECLARE_FUNCTION(execSetHidden);
	DECLARE_FUNCTION(execSetOwnerNoSee);
	DECLARE_FUNCTION(execSetOnlyOwnerSee);
	DECLARE_FUNCTION(execSetIgnoreOwnerHidden);
	DECLARE_FUNCTION(execSetShadowParent);
	DECLARE_FUNCTION(execSetLightEnvironment);
	DECLARE_FUNCTION(execSetCullDistance);
	DECLARE_FUNCTION(execSetLightingChannels);
	DECLARE_FUNCTION(execSetDepthPriorityGroup);
	DECLARE_FUNCTION(execSetViewOwnerDepthPriorityGroup);
	DECLARE_FUNCTION(execSetTraceBlocking);
	DECLARE_FUNCTION(execSetActorCollision);
	DECLARE_FUNCTION(execGetRootBodyInstance);

	// Copied from TransformComponent
	DECLARE_FUNCTION(execSetTranslation);
	DECLARE_FUNCTION(execSetRotation);
	DECLARE_FUNCTION(execSetScale);
	DECLARE_FUNCTION(execSetScale3D);
	DECLARE_FUNCTION(execSetAbsolute);

	DECLARE_FUNCTION(execGetPosition);
	DECLARE_FUNCTION(execGetRotation);

	DECLARE_FUNCTION(execClosestPointOnComponentToPoint)
	{
		P_GET_STRUCT_REF(FVector,POI);
		P_GET_STRUCT_REF(FVector,Extent);
		P_GET_STRUCT_REF(FVector,OutPointA);
		P_GET_STRUCT_REF(FVector,OutPointB);
		P_FINISH;
		*(BYTE*)Result=ClosestPointOnComponentToPoint(POI,Extent,OutPointA,OutPointB);
	}
	DECLARE_FUNCTION(execClosestPointOnComponentToComponent)
	{
		P_GET_OBJECT_REF(UPrimitiveComponent,OtherComponent);
		P_GET_STRUCT_REF(FVector,OutPointA);
		P_GET_STRUCT_REF(FVector,OutPointB);
		P_FINISH;
		*(BYTE*)Result=ClosestPointOnComponentToComponent(OtherComponent,OutPointA,OutPointB);
	}

#if WITH_NOVODEX
	virtual class NxActor* GetNxActor(FName BoneName = NAME_None);
	virtual class NxActor* GetIndexedNxActor(INT BodyIndex = INDEX_NONE);

	/** Utility for getting all physics bodies contained within this component. */
	virtual void GetAllNxActors(TArray<class NxActor*>& OutActors);
#endif // WITH_NOVODEX

	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy()
	{
		return NULL;
	}

	/**
	 * Determines whether the proxy for this primitive type needs to be recreated whenever the primitive moves.
	 * @return TRUE to recreate the proxy when UpdateTransform is called.
	 */
	virtual UBOOL ShouldRecreateProxyOnUpdateTransform() const
	{
		return FALSE;
	}

protected:
	/** @name UActorComponent interface. */
	//@{
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Attach();
	virtual void UpdateTransform();
	virtual void Detach( UBOOL bWillReattach = FALSE );
	//@}

	/**
	* @return	TRUE if the base primitive component should handle detaching decals when the primitive is detached
	*/
	virtual UBOOL AllowDecalRemovalOnDetach() const
	{
		return TRUE;
	}

	/**
    * Only valid for cases when the primitive will be reattached
	* @return	TRUE if the base primitive component should handle reattaching decals when the primitive is attached
	*/
	virtual UBOOL AllowDecalAutomaticReAttach() const
	{
		return bAllowDecalAutomaticReAttach;
	}

	/** Internal function that updates physics objects to match the RBChannel/RBCollidesWithChannel info. */
	virtual void UpdatePhysicsToRBChannels();
public:

	/** 
	* @return TRUE if the primitive component can render decals
	*/
	virtual UBOOL SupportsDecalRendering() const
	{
		return TRUE;
	}

	/**
	 * Determine if the primitive supports motion blur velocity rendering by storing
	 * motion blur transform info at the MeshElement level.
	 *
	 * @return TRUE if the primitive supports motion blur velocity rendering in its generated meshes
	 */
	virtual UBOOL HasMotionBlurVelocityMeshes() const
	{
		return FALSE;
	}

	// UObject interface.

	virtual void Serialize(FArchive& Ar);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent);

	virtual void PostLoad();
	virtual UBOOL IsReadyForFinishDestroy();
	virtual UBOOL NeedsLoadForClient() const;
	virtual UBOOL NeedsLoadForServer() const;
	virtual void PostCrossLevelFixup();

	virtual void SetOwnerNoSee(UBOOL bNewOwnerNoSee);
	virtual void SetOnlyOwnerSee(UBOOL bNewOnlyOwnerSee);
	virtual void SetIgnoreOwnerHidden(UBOOL bNewIgnoreOwnerHidden);

	/** 
     *  Looking at various values of the component, determines if this
     *  component should be added to the scene
     * @return TRUE if the component is visible and should be added to the scene, FALSE otherwise
     */
	virtual UBOOL ShouldComponentAddToScene() const;
	DECLARE_FUNCTION(execShouldComponentAddToScene)
	{
		P_FINISH;
		*(UBOOL*)Result = ShouldComponentAddToScene();
	}

	virtual void SetHiddenGame(UBOOL NewHidden);

	/**
	 * Pushes new selection state to the render thread primitive proxy
	 * @param bInSelected - TRUE if the proxy should display as if selected
	 */
	void PushSelectionToProxy(const UBOOL bInSelected);

    /**
     * Pushes new hover state to the render thread primitive proxy
     * @param bInHovered - TRUE if the proxy should display as if hovered
     */
	void PushHoveredToProxy(const UBOOL bInHovered);

	/**
	 * Sends editor visibility updates to the render thread
	 */
	void PushEditorVisibilityToProxy( QWORD InVisibility );

	/**
	 *	Sets the HiddenEditor flag and reattaches the component as necessary.
	 *
	 *	@param	NewHidden		New Value fo the HiddenEditor flag.
	 */
	virtual void SetHiddenEditor(UBOOL NewHidden);
	virtual void SetShadowParent(UPrimitiveComponent* NewShadowParent);
	virtual void SetLightEnvironment(ULightEnvironmentComponent* NewLightEnvironment);
	virtual void SetCullDistance(FLOAT NewCullDistance);
	virtual void SetLightingChannels(FLightingChannelContainer NewLightingChannels);
	virtual void SetDepthPriorityGroup(ESceneDepthPriorityGroup NewDepthPriorityGroup);
	virtual void SetViewOwnerDepthPriorityGroup(
		UBOOL bNewUseViewOwnerDepthPriorityGroup,
		ESceneDepthPriorityGroup NewViewOwnerDepthPriorityGroup
		);

	/**
	 * Default constructor, generates a GUID for the primitive.
	 */
	UPrimitiveComponent();

	/** Gets the emissive boost for the primitive component. */
	virtual FLOAT GetEmissiveBoost(INT ElementIndex) const		{ return 1.0f; };
	/** Gets the diffuse boost for the primitive component. */
	virtual FLOAT GetDiffuseBoost(INT ElementIndex) const		{ return 1.0f; };
	/** Gets the specular boost for the primitive component. */
	virtual FLOAT GetSpecularBoost(INT ElementIndex) const		{ return 1.0f; };
	virtual UBOOL GetShadowIndirectOnly() const { return FALSE; }

	/**
	 *	Setup the information required for rendering LightMap Density mode
	 *	for this component.
	 *
	 *	@param	Proxy		The scene proxy for the component (information is set on it)
	 *
	 *	@return	UBOOL		TRUE if successful, FALSE if not.
	 */
	virtual UBOOL SetupLightmapResolutionViewInfo(FPrimitiveSceneProxy& Proxy) const;
};

//
//	UMeshComponent
//

class UMeshComponent : public UPrimitiveComponent
{
	DECLARE_ABSTRACT_CLASS_NOEXPORT(UMeshComponent,UPrimitiveComponent,0,Engine);
public:

	TArrayNoInit<UMaterialInterface*>	Materials;

	/**
	 * Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
	 * asynchronous cleanup process.
	 */
	virtual void BeginDestroy();

	/** @return The total number of elements in the mesh. */
	virtual INT GetNumElements() const
	{
		return 0;
	}

	/** Accesses the material applied to a specific material index. */
	virtual UMaterialInterface* GetMaterial(INT ElementIndex) const;

	/** Sets the material applied to a material index. */
	virtual void SetMaterial(INT ElementIndex,UMaterialInterface* Material);

	/** Accesses the scene relevance information for the materials applied to the mesh. */
	FMaterialViewRelevance GetMaterialViewRelevance() const;

	/**
	 *	Tell the streaming system to start loading all textures with all mip-levels.
	 *	@param Seconds							Number of seconds to force all mip-levels to be resident
	 *	@param bPrioritizeCharacterTextures		Whether character textures should be prioritized for a while by the streaming system
	 *	@param CinematicTextureGroups			Bitfield indicating which texture groups that use extra high-resolution mips
	 */
	void PrestreamTextures( FLOAT Seconds, UBOOL bPrioritizeCharacterTextures, INT CinematicTextureGroups = 0 );

	/**
	 *	Tell the streaming system whether or not all mip levels of all textures used by this component should be loaded and remain loaded.
	 *	@param bForceMiplevelsToBeResident		Whether textures should be forced to be resident or not.
	 */
	void SetTextureForceResidentFlag( UBOOL bForceMiplevelsToBeResident );

	/** @return MaterialInterface assigned to the given material index (if any) */
	virtual UMaterialInterface* GetElementMaterial(INT ElementIndex) const
	{
		return GetMaterial(ElementIndex);
	}
	/** sets the MaterialInterface to use for the given element index (if valid) */
	virtual void SetElementMaterial(INT ElementIndex, UMaterialInterface* InMaterial)
	{
		SetMaterial(ElementIndex, InMaterial);
	}

	// UnrealScript interface.
	DECLARE_FUNCTION(execGetMaterial);
	DECLARE_FUNCTION(execSetMaterial);
	DECLARE_FUNCTION(execGetNumElements);
	DECLARE_FUNCTION(execPrestreamTextures);
};

//
//	UBrushComponent
//

class UBrushComponent : public UPrimitiveComponent
{
	DECLARE_CLASS_NOEXPORT(UBrushComponent,UPrimitiveComponent,0,Engine);

	/** Source brush data.  Can be NULL with cooked data, where the collision or convex data should be used instead. */
	UModel*						Brush;

	/** Simplified collision data for the mesh. */
	FKAggregateGeom				BrushAggGeom;

	/** Physics engine shapes created for this BrushComponent. */
	class NxActorDesc*			BrushPhysDesc;

	/** Cached brush convex-mesh data for use with the physics engine. */
	FKCachedConvexData			CachedPhysBrushData;

	/** 
	 *	Indicates version that CachedPhysBrushData was created at. 
	 *	Compared against CurrentCachedPhysDataVersion.
	 */
	INT							CachedPhysBrushDataVersion;

	/** 
	*	Normally a blocking volume is considered 'pure simplified collision', so when tracing for complex collision, never collide 
	*	This flag overrides that behaviour
	*/
	BITFIELD	bBlockComplexCollisionTrace:1;

	// UObject interface.
	virtual void Serialize( FArchive& Ar );
	virtual void PreSave();
	virtual void FinishDestroy();

	// UPrimitiveComponent interface.
	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();

	virtual void InitComponentRBPhys(UBOOL bFixed);

	virtual UBOOL PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags);
	virtual UBOOL LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags);

	virtual void UpdateBounds();

	/** 
	 * Retrieves the materials used in this component 
	 * 
 	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const;

	virtual BYTE GetStaticDepthPriorityGroup() const;

	// UBrushComponent interface.
	virtual UBOOL IsValidComponent() const;

	// UBrushComponent interface

	/** Create the BrushAggGeom collection-of-convex-primitives from the Brush UModel data. */
	void BuildSimpleBrushCollision();

	/** Build cached convex data for physics engine. */
	void BuildPhysBrushData();
	

	/*GJKResult*/ BYTE ClosestPointOnComponentToComponent(class UPrimitiveComponent*& OtherComponent,FVector& PointOnComponentA,FVector& PointOnComponentB);

	virtual /*GJKResult*/ BYTE ClosestPointOnComponentInternal(IGJKHelper* ExtentHelper, FVector& OutPointA, FVector& OutPointB);

	/**
	 *  Retrieve various actor metrics depending on the provided type.  All of
	 *  these will total the values for this component.
	 *
	 *  @param MetricsType The type of metric to calculate.
	 *
	 *  METRICS_VERTS    - Get the number of vertices.
	 *  METRICS_TRIS     - Get the number of triangles.
	 *  METRICS_SECTIONS - Get the number of sections.
	 *
	 *  @return INT The total of the given type for this component.
	 */
	virtual INT GetActorMetrics(EActorMetricsType MetricsType);
};

//
//	UCylinderComponent
//

class UCylinderComponent : public UPrimitiveComponent
{
	DECLARE_CLASS_NOEXPORT(UCylinderComponent,UPrimitiveComponent,0,Engine);

	FLOAT	CollisionHeight;
	FLOAT	CollisionRadius;

	/** Color used to draw the cylinder. */
	FColor	CylinderColor;

	/**	Whether to draw the red bounding box for this cylinder. */
	BITFIELD	bDrawBoundingBox:1;

	/** If TRUE, this cylinder will always draw when SHOW_Collision is on, even if CollideActors is FALSE. */
	BITFIELD	bDrawNonColliding:1;

	/** If TRUE, this cylinder will always draw when the actor is selected. */
	BITFIELD	bAlwaysRenderIfSelected:1;
		
	// UPrimitiveComponent interface.

	virtual UBOOL PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags);
	virtual UBOOL LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags);

	/**		**INTERNAL USE ONLY**
	* Implementation required by a primitive component in order to properly work with the closest points algorithms below
	* Given an interface to some other primitive, return the points on each object closest to each other
	* @param ExtentHelper - Interface class returning the supporting points on some other primitive type
	* @param OutPointA - The point closest on the 'other' primitive
	* @param OutPointB - The point closest on this primitive
	* 
	* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
	*/
	/*GJKResult*/ BYTE ClosestPointOnComponentInternal(IGJKHelper* ExtentHelper, FVector& OutPointA, FVector& OutPointB);

	/**
	* Calculates the closest point this component to another component
	* @param PrimitiveComponent - Another Primitive Component
	* @param PointOnComponentA - Point on this primitive closest to other primitive
	* @param PointOnComponentB - Point on other primitive closest to this primitive
	* 
	* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
	*/
	/*GJKResult*/ BYTE ClosestPointOnComponentToComponent(class UPrimitiveComponent*& OtherComponent,FVector& PointOnComponentA,FVector& PointOnComponentB);

	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void UpdateBounds();

	// UCylinderComponent interface.

	void SetCylinderSize(FLOAT NewRadius, FLOAT NewHeight);

	// Native script functions.

	DECLARE_FUNCTION(execSetCylinderSize);
};

//
//	UArrowComponent
//

class UArrowComponent : public UPrimitiveComponent
{
	DECLARE_CLASS_NOEXPORT(UArrowComponent,UPrimitiveComponent,0,Engine);

	FColor			ArrowColor;
	FLOAT			ArrowSize;
	UBOOL			bTreatAsASprite;
#if WITH_EDITORONLY_DATA
	FName			SpriteCategoryName;
#endif

	// UPrimitiveComponent interface.
	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void UpdateBounds();
};

//
//	UDrawSphereComponent
//

class UDrawSphereComponent : public UPrimitiveComponent
{
	DECLARE_CLASS_NOEXPORT(UDrawSphereComponent,UPrimitiveComponent,0,Engine);

	FColor				SphereColor;
	UMaterialInterface*	SphereMaterial;
	FLOAT				SphereRadius;
	INT					SphereSides;
	BITFIELD			bDrawWireSphere:1;
	BITFIELD			bDrawLitSphere:1;
	BITFIELD			bDrawOnlyIfSelected:1;

	// UPrimitiveComponent interface.
	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void UpdateBounds();

	/** 
	 * Retrieves the materials used in this component 
	 * 
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const; 
};

//
//	UDrawCylinderComponent
//

class UDrawCylinderComponent : public UPrimitiveComponent
{
	DECLARE_CLASS_NOEXPORT(UDrawCylinderComponent,UPrimitiveComponent,0,Engine);

	FColor						CylinderColor;
	class UMaterialInstance*	CylinderMaterial;
	FLOAT						CylinderRadius;
	FLOAT						CylinderTopRadius;
	FLOAT						CylinderHeight;
	FLOAT						CylinderHeightOffset;
	INT							CylinderSides;
	BITFIELD					bDrawWireCylinder:1;
	BITFIELD					bDrawLitCylinder:1;
	BITFIELD					bDrawOnlyIfSelected:1;

	// UPrimitiveComponent interface.
	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void UpdateBounds();

	/** 
	 * Retrieves the materials used in this component 
	 * 
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const; 
};

//
//	UDrawBoxComponent
//

class UDrawBoxComponent : public UPrimitiveComponent
{
	DECLARE_CLASS_NOEXPORT(UDrawBoxComponent,UPrimitiveComponent,0,Engine);

	FColor				BoxColor;
	UMaterialInstance*	BoxMaterial;
	FVector				BoxExtent;
	BITFIELD			bDrawWireBox:1;
	BITFIELD			bDrawLitBox:1;
	BITFIELD			bDrawOnlyIfSelected:1;

	// UPrimitiveComponent interface.
	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void UpdateBounds();

	/** 
	 * Retrieves the materials used in this component 
	 * 
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const; 
};

//
//	UDrawBoxComponent
//

class UDrawCapsuleComponent : public UPrimitiveComponent
{
	DECLARE_CLASS_NOEXPORT(UDrawCapsuleComponent,UPrimitiveComponent,0,Engine);

	FColor				CapsuleColor;
	UMaterialInstance*	CapsuleMaterial;
	float				CapsuleHeight;
	float				CapsuleRadius;
	BITFIELD			bDrawWireCapsule:1;
	BITFIELD			bDrawLitCapsule:1;
	BITFIELD			bDrawOnlyIfSelected:1;

	// UPrimitiveComponent interface.
	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void UpdateBounds();

	/** 
	 * Retrieves the materials used in this component 
	 * 
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const; 
};

//
//	UDrawFrustumComponent
//

class UDrawFrustumComponent : public UPrimitiveComponent
{
	DECLARE_CLASS_NOEXPORT(UDrawFrustumComponent,UPrimitiveComponent,0,Engine);

	FColor			FrustumColor;
	FLOAT			FrustumAngle;
	FLOAT			FrustumAspectRatio;
	FLOAT			FrustumStartDist;
	FLOAT			FrustumEndDist;
	/** optional texture to show on the near plane */
	UTexture*		Texture;

	// UPrimitiveComponent interface.
	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void UpdateBounds();
};

class UDrawLightRadiusComponent : public UDrawSphereComponent
{
	DECLARE_CLASS_NOEXPORT(UDrawLightRadiusComponent,UDrawSphereComponent,0,Engine);

	// UPrimitiveComponent interface.
	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void UpdateBounds();
};

//
//	UCameraConeComponent
//

class UCameraConeComponent : public UPrimitiveComponent
{
	DECLARE_CLASS_NOEXPORT(UCameraConeComponent,UPrimitiveComponent,0,Engine);

	// UPrimitiveComponent interface.
	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void UpdateBounds();
};


/**
 *	Utility component for drawing a textured quad face. 
 *  Origin is at the component location, frustum points down position X axis.
 */
class UDrawQuadComponent : public UPrimitiveComponent
{
	DECLARE_CLASS_NOEXPORT(UDrawQuadComponent,UPrimitiveComponent,0,Engine);

	/** Texture source to draw on quad face */
	UTexture*	Texture;
	/** Width of quad face */
	FLOAT		Width;
	/** Height of quad face */
	FLOAT		Height;

	// UPrimitiveComponent interface.

	virtual void Render(const FSceneView* View,class FPrimitiveDrawInterface* PDI);
	virtual void UpdateBounds();
};
