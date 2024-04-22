/*=============================================================================
	UnActorComponent.h: Actor component definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

//
//	UActorComponent
//

class UActorComponent : public UComponent
{
	DECLARE_ABSTRACT_CLASS_NOEXPORT(UActorComponent,UComponent,0,Engine);
protected:

	friend class FComponentReattachContext;

	/**
	 * Sets the ParentToWorld transform the component is attached to.
	 * This needs to be followed by a call to Attach or UpdateTransform.
	 * @param ParentToWorld - The ParentToWorld transform the component is attached to.
	 */
	virtual void SetParentToWorld(const FMatrix& ParentToWorld) {}

	/**
	 * Attaches the component to a ParentToWorld transform, owner and scene.
	 * Requires IsValidComponent() == true.
	 */
	virtual void Attach();

	/**
	 * Updates state dependent on the ParentToWorld transform.
	 * Requires bAttached == true
	 */
	virtual void UpdateTransform();

	/**
	 * Detaches the component from the scene it is in.
	 * Requires bAttached == true
	 *
	 * @param bWillReattach TRUE is passed if Attach will be called immediately afterwards.  This can be used to
	 *                      preserve state between reattachments.
	 */
	virtual void Detach( UBOOL bWillReattach = FALSE );

	/**
	 * Starts gameplay for this component.
	 * Requires bAttached == true.
	 */
	virtual void BeginPlay();

	/**
	 * Updates time dependent state for this component.
	 * Requires bAttached == true.
	 * @param DeltaTime - The time since the last tick.
	 */
	virtual void Tick(FLOAT DeltaTime);

	/**
	 * Checks the components that are indirectly attached via this component for pending lazy updates, and applies them.
	 */
	virtual void UpdateChildComponents() {}

	FSceneInterface*	Scene;
	class AActor*		Owner;
	BITFIELD			bAttached:1;

public:
	BITFIELD			bTickInEditor:1;
	BITFIELD			bNeedsReattach:1;
	BITFIELD			bNeedsUpdateTransform:1;
	SCRIPT_ALIGN;

public:

	/** The ticking group this component belongs to */
	BYTE				TickGroup;
	SCRIPT_ALIGN;

	/**
	 * Conditionally calls Attach if IsValidComponent() == true.
	 * @param InScene - The scene to attach the component to.
	 * @param InOwner - The actor which the component is directly or indirectly attached to.
	 * @param ParentToWorld - The ParentToWorld transform the component is attached to.
	 */
	void ConditionalAttach(FSceneInterface* InScene,AActor* InOwner,const FMatrix& ParentToWorld);

	/**
	 * Conditionally calls UpdateTransform if bAttached == true.
	 * @param ParentToWorld - The ParentToWorld transform the component is attached to.
	 */
	void ConditionalUpdateTransform(const FMatrix& ParentToWorld);

	/**
	 * Conditionally calls UpdateTransform if bAttached == true.
	 */
	void ConditionalUpdateTransform();

	/**
	 * Conditionally calls Detach if bAttached == true.
	 *
	 * @param bWillReattach TRUE is passed if Attach will be called immediately afterwards.  This can be used to
	 *                      preserve state between reattachments.
	 */
	void ConditionalDetach( UBOOL bWillReattach = FALSE );

	/**
	 * Conditionally calls BeginPlay if bAttached == true.
	 */
	void ConditionalBeginPlay();

	/**
	 * Conditionally calls Tick if bAttached == true.
	 * @param DeltaTime - The time since the last tick.
	 */
	void ConditionalTick(FLOAT DeltaTime);

	/**
	 * Returns whether the component's owner is selected.
	 */
	UBOOL IsOwnerSelected() const;

	/**
	 * Returns whether the component is valid to be attached.
	 * This should be implemented in subclasses to validate parameters.
	 * If this function returns false, then Attach won't be called.
	 */
	virtual UBOOL IsValidComponent() const;

	/**
	 * Called when this actor component has moved, allowing it to discard statically cached lighting information.
	 */
	virtual void InvalidateLightingCache() {}

	/**
	 * Marks the appropriate world as lighting requiring a rebuild.
	 */
	void MarkLightingRequiringRebuild();

	/**
	 * For initialising any rigid-body physics related data in this component.
	 */
	virtual void InitComponentRBPhys(UBOOL bFixed) {}

	/**
	 * For changing physics information between dynamic and simulating and 'fixed' (ie infinitely massive).
	 */
	virtual void SetComponentRBFixed(UBOOL bFixed) {}

	/**
	 * For terminating any rigid-body physics related data in this component.
	 */
	virtual void TermComponentRBPhys(class FRBPhysScene* InScene) {}

	/**
	 * Called during map error checking .  Derived class implementations should call up to their parents.
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

	/** if the component is attached, finds out what it's attached to and detaches it from that thing
	 * slower, so use only when you don't have an easy way to find out where it's attached
	 */
	void DetachFromAny();

	/**
	 * If the component needs attachment, or has a lazy update pending, perform it.  Also calls UpdateChildComponents to update indirectly attached components.
	 * @param InScene - The scene which the component should be attached to.
	 * @param InOwner - The actor which the component should be attached to.
	 * @param InActorToWorld - The transform which the component should be attached to.
	 */
	void UpdateComponent(FSceneInterface* InScene,AActor* InOwner,const FMatrix& InLocalToWorld,UBOOL bCollisionUpdate=FALSE);

	/**
	 * Begins a deferred component reattach.
	 * If the game is running, this will defer reattaching the component until the end of the game tick.
	 * If the editor is running, this will immediately reattach the component.
	 */
	void BeginDeferredReattach();

	/**
	 * Begins a deferred component reattach.
	 * If the game is running, this will defer reattaching the component until the end of the game tick.
	 * If the editor is running, this will immediately reattach the component.
	 */
	void BeginDeferredUpdateTransform();

	/**
	 * Dirties transform flag to ensure that UpdateComponent updates transform.
	 */
	void DirtyTransform() { bNeedsUpdateTransform = TRUE; }

	/**
	 * Returns whether component should be marked as pending kill inside AActor::MarkComponentsAsPendingKill if it has
	 * a say in it. This is e.g. used by audio components to not be removed by the garbage collector while they are
	 * playing.
	 *
	 * @return TRUE
	 */
	virtual UBOOL AllowBeingMarkedPendingKill()
	{
		return TRUE;
	}

	/** @name Accessors. */
	FSceneInterface* GetScene() const { return Scene; }
	class AActor* GetOwner() const { return Owner; }
	void SetOwner(class AActor* NewOwner) { Owner = NewOwner; }
	UBOOL IsAttached() const { return bAttached; }
	UBOOL NeedsReattach() const { return bNeedsReattach; }
	UBOOL NeedsUpdateTransform() const { return bNeedsUpdateTransform; }

	/** UObject interface. */
	virtual void BeginDestroy();
	virtual UBOOL NeedsLoadForClient() const;
	virtual UBOOL NeedsLoadForServer() const;
	virtual void PreEditChange(UProperty* PropertyThatWillChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	DECLARE_FUNCTION(execSetComponentRBFixed)
	{
		P_GET_UBOOL(bFixed);
		P_FINISH;
		SetComponentRBFixed(bFixed);
	}

    DECLARE_FUNCTION(execSetTickGroup)
    {
        P_GET_BYTE(NewTickGroup);
        P_FINISH;
        SetTickGroup(NewTickGroup);
    }
	/**
	 * Sets the ticking group for this component
	 *
	 * @param NewTickGroup the new group to assign
	 */
	void SetTickGroup(BYTE NewTickGroup);

	DECLARE_FUNCTION(execForceUpdate);

	DECLARE_FUNCTION(execDetachFromAny)
	{
		P_FINISH;
		DetachFromAny();
	}
};


/**
 * Light Classification Enum
 *
 * @warning: this structure is manually mirrored in LightComponent.uc
 */
enum ELightAffectsClassification
{
	LAC_USER_SELECTED                = 0,
	LAC_DYNAMIC_AFFECTING            = 1,
	LAC_STATIC_AFFECTING             = 2,
	LAC_DYNAMIC_AND_STATIC_AFFECTING = 3,
	LAC_MAX                          = 4
};

/**
* Type of shadowing to apply for the light
* @warning: this structure is manually mirrored in LightComponent.uc
*/
enum ELightShadowMode
{
	/** Shadows rendered due to absence of light when doing dynamic lighting. Default */
	LightShadow_Normal=0,
	/** Shadows rendered as a fullscreen pass by modulating entire scene by a shadow factor. */
	LightShadow_Modulate,
	/** Deprecated */
	LightShadow_ModulateBetter
};

/**
* Type of shadow projection to apply for the light
* @warning: this structure is manually mirrored in LightComponent.uc
*/
enum EShadowProjectionTechnique
{
	/** Shadow projection is rendered using either PCF/VSM based on global settings  */
	ShadowProjTech_Default=0,
	/** Shadow projection is rendered using the PCF (Percentage Closer Filtering) technique. May have heavy banding artifacts */
	ShadowProjTech_PCF,
	/** Shadow projection is rendered using the VSM (Variance Shadow Map) technique. May have shadow offset and light bleed artifacts */
	ShadowProjTech_VSM,
	/** Shadow projection is rendered using the Low quality Branching PCF technique. May have banding and penumbra detection artifacts */
	ShadowProjTech_BPCF_Low,
	/** Shadow projection is rendered using the Medium quality Branching PCF technique. May have banding and penumbra detection artifacts */
	ShadowProjTech_BPCF_Medium,
	/** Shadow projection is rendered using the High quality Branching PCF technique. May have banding and penumbra detection artifacts */
	ShadowProjTech_BPCF_High,
};

/**
* Quality settings for projected shadow buffer filtering
* @warning: this structure is manually mirrored in LightComponent.uc
*/
enum EShadowFilterQuality
{
	SFQ_Low=0,
	SFQ_Medium,
	SFQ_High,
	SFQ_Num
};

/**
 * Lighting channel container.
 *
 * @warning: this structure is manually mirrored in LightComponent.uc
 * @warning: this structure cannot exceed 32 bits
 */
struct FLightingChannelContainer
{
	union
	{
		struct
		{
			/** Whether the lighting channel has been initialized. Used to determine whether UPrimitveComponent::Attach should set defaults. */
			BITFIELD bInitialized:1;

			// User settable channels that are auto set and also default to true for lights.
			BITFIELD BSP:1;
			BITFIELD Static:1;
			BITFIELD Dynamic:1;
			// User set channels.
			BITFIELD CompositeDynamic:1;
			BITFIELD Skybox:1;
			BITFIELD Unnamed_1:1;
			BITFIELD Unnamed_2:1;
			BITFIELD Unnamed_3:1;
			BITFIELD Unnamed_4:1;
			BITFIELD Unnamed_5:1;
			BITFIELD Unnamed_6:1;
			BITFIELD Cinematic_1:1;
			BITFIELD Cinematic_2:1;
			BITFIELD Cinematic_3:1;
			BITFIELD Cinematic_4:1;
			BITFIELD Cinematic_5:1;
			BITFIELD Cinematic_6:1;
			BITFIELD Cinematic_7:1;
			BITFIELD Cinematic_8:1;
			BITFIELD Cinematic_9:1;
			BITFIELD Cinematic_10:1;
			BITFIELD Gameplay_1:1;
			BITFIELD Gameplay_2:1;
			BITFIELD Gameplay_3:1;
			BITFIELD Gameplay_4:1;
			BITFIELD Crowd:1;
		};
		DWORD Bitfield;
	};

	/**
	 * Returns whether the passed in lighting channel container shares any set channels with the current one.
	 *
	 * @param	Other	other lighting channel to check against
	 * @return	TRUE if there's overlap, FALSE otherwise
	 */
	UBOOL OverlapsWith( const FLightingChannelContainer& Other ) const
	{
		// We need to mask out bInitialized when determining overlap.
		FLightingChannelContainer Mask;
		Mask.Bitfield		= 0;
		Mask.bInitialized	= TRUE;
		DWORD BitfieldMask	= ~Mask.Bitfield;
		return Bitfield & Other.Bitfield & BitfieldMask ? TRUE : FALSE;
	}

	/**
	 * Sets all channels.
	 */
	void SetAllChannels()
	{
		FLightingChannelContainer Mask;
		Mask.Bitfield		= 0;
		Mask.bInitialized	= TRUE;
		DWORD BitfieldMask	= ~Mask.Bitfield;
		Bitfield = Bitfield | BitfieldMask;
	}

	/**
	 * Clears all channels.
	 */
	void ClearAllChannels()
	{
		FLightingChannelContainer Mask;
		Mask.Bitfield		= 0;
		Mask.bInitialized	= TRUE;
		DWORD BitfieldMask	= Mask.Bitfield;
		Bitfield = Bitfield & BitfieldMask;
	}

	/** Lighting channel hash function. */
	friend DWORD GetTypeHash(const FLightingChannelContainer& LightingChannels)
	{
		return LightingChannels.Bitfield >> 1;
	}

	/** Comparison operator. */
	friend UBOOL operator==(const FLightingChannelContainer& A,const FLightingChannelContainer& B)
	{
		return A.Bitfield == B.Bitfield;
	}

	/** Generates a 4 bit lighting channel mask used by deferred shading for handling lighting channels. */
	inline BYTE GetDeferredShadingChannelMask() const
	{
		// Use bit 0 to store whether any of the default lighting channels are set
		return (BSP || Static || Dynamic)
			| Cinematic_1 << 1
			| Cinematic_2 << 2
			| Cinematic_3 << 3;
	}
};

/** different light component types */
enum ELightComponentType
{
	LightType_Sky,
	LightType_SphericalHarmonic,
	LightType_Directional,
	LightType_DominantDirectional,
	LightType_Point,
	LightType_DominantPoint,
	LightType_Spot,
	LightType_DominantSpot,
	LightType_MAX
};

/** Returns TRUE if LightType is one of the dominant light types. */
inline UBOOL IsDominantLightType(BYTE LightType)
{
	return LightType == LightType_DominantDirectional || LightType == LightType_DominantPoint || LightType == LightType_DominantSpot;
}

/** Returns TRUE if a dominant light with the given light type and modifiers is active. */
inline UBOOL UsingDominantLight(BYTE LightType, UBOOL bPrimitiveAllowsDominantDirectionalLightInfluence, UBOOL bPrimitiveAllowsAnyDominantLightInfluence)
{
	return bPrimitiveAllowsAnyDominantLightInfluence
		&& ((LightType == LightType_DominantDirectional && bPrimitiveAllowsDominantDirectionalLightInfluence)
		|| LightType == LightType_DominantPoint 
		|| LightType == LightType_DominantSpot);
}

//
//	ULightComponent
//

class ULightComponent : public UActorComponent
{
	DECLARE_ABSTRACT_CLASS_NOEXPORT(ULightComponent,UActorComponent,0,Engine)
public:

	/** The light's scene info. */
	FLightSceneInfo* SceneInfo;

	FMatrix WorldToLight;
	FMatrix LightToWorld;

	/**
	 * GUID used to associate a light component with precomputed shadowing information across levels.
	 * The GUID changes whenever the light position changes.
	 */
	FGuid					LightGuid;
	/**
	 * GUID used to associate a light component with precomputed shadowing information across levels.
	 * The GUID changes whenever any of the lighting relevant properties changes.
	 */
	FGuid					LightmapGuid;

	FLOAT					Brightness;
	FColor					LightColor;
	class ULightFunction*	Function;
	
	BITFIELD bEnabled : 1;

	/** True if the light can be blocked by shadow casting primitives. */
	BITFIELD CastShadows : 1;

	/** True if the light can be blocked by static shadow casting primitives. */
	BITFIELD CastStaticShadows : 1;

	/** True if the light can be blocked by dynamic shadow casting primitives. */
	BITFIELD CastDynamicShadows : 1;

	/** True if the light should cast shadow from primitives which use a composite light environment. */
	BITFIELD bCastCompositeShadow : 1;

	/** If bCastCompositeShadow=TRUE, whether the light should affect the composite shadow direction. */
	BITFIELD bAffectCompositeShadowDirection : 1;

	/** If enabled and the light casts modulated shadows, this will cause self-shadowing of shadows rendered from this light to use normal shadow blending. */
	BITFIELD bNonModulatedSelfShadowing : 1;

	/** Causes shadows from this light to only self shadow. */
	BITFIELD bSelfShadowOnly : 1;

	/** Whether to allow preshadows (the static environment casting dynamic shadows on dynamic objects) from this light. */
	BITFIELD bAllowPreShadow : 1;

	/** True if this light should use dynamic shadows for all primitives. */
	BITFIELD bForceDynamicLight : 1;

	/** Set to True to store the direct flux of this light in a light-map. */
	BITFIELD UseDirectLightMap : 1;

	/** Whether light has ever been built into a lightmap */
	BITFIELD bHasLightEverBeenBuiltIntoLightMap : 1;

	/** Whether the light can affect dynamic primitives even though the light is not affecting the dynamic channel. */
	BITFIELD bCanAffectDynamicPrimitivesOutsideDynamicChannel : 1;

	/** Whether to render light shafts from this light. */
	BITFIELD bRenderLightShafts : 1;

	/** Whether to replace this light's analytical specular with image based specular on materials that support it. */
	BITFIELD bUseImageReflectionSpecular : 1;

	/** The precomputed lighting for that light source is valid. It might become invalid if some properties change (e.g. position, brightness). */
	BITFIELD bPrecomputedLightingIsValid : 1;

	/** Whether this light is being used as the OverrideLightComponent on a primitive and shouldn't affect any other primitives. */
	BITFIELD bExplicitlyAssignedLight : 1;

	/** Whether this light can be combined into the DLE normally.  Overridden to false in the case of muzzle flashes to prevent SH artifacts */
	BITFIELD bAllowCompositingIntoDLE : 1;

	/**
	 * The light environment which the light affects.
	 * NULL represents an implicit default light environment including all primitives and lights with LightEnvironment=NULL.
	 */
	class ULightEnvironmentComponent* LightEnvironment;

	/** Lighting channels controlling light/ primitive interaction. Only allows interaction if at least one channel is shared */
	FLightingChannelContainer LightingChannels;

	/**
	 * This is the classification of this light.  This is used for placing a light for an explicit
     * purpose.  Basically you can now have "type" information with lights and understand the
	 * intent of why a light was placed.  This is very useful for content people getting maps
	 * from others and understanding why there is a dynamic affect light in the middle of the world
	 * with a radius of 32k!  And also useful for being able to do searches such as the following:
	 * show me all lights which affect dynamic objects.  Now show me the set of lights which are
	 * not explicitly set as Dynamic Affecting lights.
	 *
	 **/
	BYTE LightAffectsClassification;

	/** Type of shadowing to apply for the light */
	BYTE LightShadowMode;

	/** Shadow color for modulating entire scene */
	FLinearColor ModShadowColor;

	/** Time since the caster was last visible at which the mod shadow will fade out completely.  */
	FLOAT ModShadowFadeoutTime;

	/** Exponent that controls mod shadow fadeout curve. */
	FLOAT ModShadowFadeoutExponent;

private:
	/**
	 * The munged index of this light in the light list
	 *
	 * > 0 == static light list
	 *   0 == not part of any light list
	 * < 0 == dynamic light list
	 */
	INT LightListIndex;

public:
	/** Type of shadow projection to use for this light */
	BYTE ShadowProjectionTechnique;

	/** Quality of shadow buffer filtering to use for this light's shadows */
	BYTE ShadowFilterQuality;

	/**
	 * Override for min dimensions (in texels) allowed for rendering shadow subject depths.
	 * This also controls shadow fading, once the shadow resolution reaches MinShadowResolution it will be faded out completely.
	 * A value of 0 defaults to MinShadowResolution in SystemSettings.
	 */
	INT MinShadowResolution;

	/**
	 * Override for max square dimensions (in texels) allowed for rendering shadow subject depths.
	 * A value of 0 defaults to MaxShadowResolution in SystemSettings.
	 */
	INT MaxShadowResolution;

	/** 
	 * Resolution in texels below which shadows begin to be faded out. 
	 * Once the shadow resolution reaches MinShadowResolution it will be faded out completely.
	 * A value of 0 defaults to ShadowFadeResolution in SystemSettings.
	 */
	INT ShadowFadeResolution;

	/** Everything closer to the camera than this distance will occlude light shafts. */
	FLOAT OcclusionDepthRange;

	/** Scales the additive color. */
	FLOAT BloomScale;

	/** Scene color must be larger than this to create bloom in the light shafts. */
	FLOAT BloomThreshold;

	/** 
	 * Scene color luminance must be less than this to receive bloom from light shafts. 
	 * This behaves like Photoshop's screen blend mode and prevents over saturation from adding bloom to already bright areas.
	 * The default value of 1 means that a pixel with a luminance of 1 won't receive any bloom, but a luminance of .5 will receive half bloom.
	 */
	FLOAT BloomScreenBlendThreshold;

	/** Multiplies against scene color to create the bloom color. */
	FColor BloomTint;

	/** 100 is maximum blur length, 0 is no blur. */
	FLOAT RadialBlurPercent;

	/** Controls how dark the occlusion masking is, a value of .5 would mean that an occlusion of 0 only darkens underlying color by half. */
	FLOAT OcclusionMaskDarkness;

	/** Scales the contribution of the reflection specular highlight. */
	FLOAT ReflectionSpecularBrightness;

	/**
	 * Creates a proxy to represent the light to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FLightSceneInfo* CreateSceneInfo() const PURE_VIRTUAL(ULightComponent::CreateSceneInfo,return NULL;);

	/**
	 * Tests whether this light affects the given primitive.  This checks both the primitive and light settings for light relevance
	 * and also calls AffectsBounds.
	 * @param PrimitiveSceneInfo - The primitive to test.
	 * @return True if the light affects the primitive.
	 */
	UBOOL AffectsPrimitive(const UPrimitiveComponent* Primitive, UBOOL bCompareLightingChannels) const;

	/**
	 * Tests whether the light affects the given bounding volume.
	 * @param Bounds - The bounding volume to test.
	 * @return True if the light affects the bounding volume
	 */
	virtual UBOOL AffectsBounds(const FBoxSphereBounds& Bounds) const;

	/**
	 * Returns the world-space bounding box of the light's influence.
	 */
	virtual FBox GetBoundingBox() const { return FBox(FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX),FVector(HALF_WORLD_MAX,HALF_WORLD_MAX,HALF_WORLD_MAX)); }

	// GetDirection

	FVector GetDirection() const { return FVector(WorldToLight.M[0][2],WorldToLight.M[1][2],WorldToLight.M[2][2]); }

	// GetOrigin

	FVector GetOrigin() const { return LightToWorld.GetOrigin(); }

	/**
	 * Returns the homogenous position of the light.
	 */
	virtual FVector4 GetPosition() const PURE_VIRTUAL(ULightComponent::GetPosition,return FVector4(););

	/**
	* @return ELightComponentType for the light component class
	*/
	virtual ELightComponentType GetLightType() const PURE_VIRTUAL(ULightComponent::GetLightType,return LightType_MAX;);

	/**
	 * Computes the intensity of the direct lighting from this light on a specific point.
	 */
	virtual FLinearColor GetDirectIntensity(const FVector& Point) const;

	/**
	 * Updates/ resets light GUIDs.
	 */
	virtual void UpdateLightGUIDs();

	/**
	 * Validates light GUIDs and resets as appropriate.
	 */
	void ValidateLightGUIDs();

	/**
	 * Checks whether a given primitive will cast shadows from this light.
	 * @param Primitive - The potential shadow caster.
	 * @return Returns True if a primitive blocks this light.
	 */
	UBOOL IsShadowCast(UPrimitiveComponent* Primitive) const;

	/**
	 * Returns True if a light cannot move or be destroyed during gameplay, and can thus cast static shadowing.
	 */
	UBOOL HasStaticShadowing() const;

	/**
	 * Returns True if dynamic primitives should cast projected shadows for this light.
	 */
	UBOOL HasProjectedShadowing() const;

	/**
	 * Returns True if a light's parameters as well as its position is static during gameplay, and can thus use static lighting.
	 */
	UBOOL HasStaticLighting() const;

	/**
	 * Returns whether static lighting, aka lightmaps, is being used for primitive/ light
	 * interaction.
	 *
	 * @param bForceDirectLightMap	Whether primitive is set to force lightmaps
	 * @return TRUE if lightmaps/ static lighting is being used, FALSE otherwise
	 */
	UBOOL UseStaticLighting( UBOOL bForceDirectLightMap ) const;

	/**
	 * Toggles the light on or off
	 *
	 * @param bSetEnabled TRUE to enable the light or FALSE to disable it
	 */
	void SetEnabled( UBOOL bSetEnabled );

	/** Set Light Property for native access **/
	void SetLightProperties(FLOAT NewBrightness, const FColor & NewLightColor, ULightFunction* NewLightFunction);
	
	/** Updates the selection state of this light. */
	void UpdateSelection(UBOOL bInSelected);

	/** Updates the list of components that are allowed to receive forward shadows from this light. */
	void UpdateForwardShadowReceivers(const TArray<UPrimitiveComponent*>& Receivers);

	DECLARE_FUNCTION(execSetEnabled);
	DECLARE_FUNCTION(execSetLightProperties);
	DECLARE_FUNCTION(execGetOrigin);
	DECLARE_FUNCTION(execGetDirection);
	DECLARE_FUNCTION(execUpdateColorAndBrightness);
	DECLARE_FUNCTION(execUpdateLightShaftParameters);

	// UObject interface
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();
	virtual void PreEditUndo();
	virtual void PostEditUndo();

	// UActorComponent interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/**
	 * Called after duplication & serialization and before PostLoad. Used to e.g. make sure GUIDs remains globally unique.
	 */
	virtual void PostDuplicate();

	/**
	 * Called after importing property values for this object (paste, duplicate or .t3d import)
	 * Allow the object to perform any cleanup for properties which shouldn't be duplicated or
	 * are unsupported by the script serialization
	 */
	virtual void PostEditImport();

    /** This will set the light classification based on the current settings **/
	virtual void SetLightAffectsClassificationBasedOnSettings();

    /** determines if this LightComponent meets the constraints for the dynamic affecting classifcation **/
	virtual UBOOL IsLACDynamicAffecting();

    /** determines if this LightComponent meets the constraints for the static affecting classifcation **/
	virtual UBOOL IsLACStaticAffecting();

    /** determines if this LightComponent meets the constraints for the dynamic and static affecting classifcation **/
	virtual UBOOL IsLACDynamicAndStaticAffecting();

	/**
	 * Invalidates lightmap data of affected primitives if this light has ever been built
	 * into a lightmap.
	 */
	virtual void InvalidateLightmapData(UBOOL bOnlyVisible = FALSE);

	/**
	 * Functions to access the LightListIndex correctly
	 *
	 * > 0 == static light list
	 *   0 == not part of any light list
	 * < 0 == dynamic light list
	 */

	/**
	 * Returns whether light is currently in a light list.
	 *
	 * @return TRUE if light is in a light list, false otherwise
	 */
	FORCEINLINE UBOOL IsInLightList()
	{
		return LightListIndex != 0;
	}
	/**
	 * Returns whether the light is part of the static light list
	 *
	 * @return TRUE if light is in static light list, FALSE otherwise
	 */
	FORCEINLINE UBOOL IsInStaticLightList()
	{
		return LightListIndex > 0;
	}
	/**
	 * Returns whether the light is part of the dynamic light list
	 *
	 * @return TRUE if light is in dynamic light list, FALSE otherwise
	 */
	FORCEINLINE UBOOL IsInDynamicLightList()
	{
		return LightListIndex < 0;
	}
	/**
	 * Returns the light list index. The calling code is responsible for determining
	 * which list the index is for by using the other accessor functions.
	 *
	 * @return Index in light list
	 */
	FORCEINLINE INT	GetLightListIndex()
	{
		if( LightListIndex > 0 )
		{
			return LightListIndex-1;
		}
		// Technically should be an if < 0 but we assume that this function is only called
		// on valid light list indices for performance reasons.
		else
		{
			return -LightListIndex-1;
		}
	}
	/**
	 * Sets the static light list index.
	 *
	 * @param Index		New index to set
	 */
	FORCEINLINE void SetStaticLightListIndex(INT InIndex)
	{
		LightListIndex = InIndex+1;
	}
	/**
	 * Sets the dynamic light list index.
	 *
	 * @param Index		New index to set
	 */
	FORCEINLINE void SetDynamicLightListIndex(INT InIndex)
	{
		LightListIndex = -InIndex-1;
	}
	/**
	 * Invalidates the light list index.
	 */
	FORCEINLINE void InvalidateLightListIndex()
	{
		LightListIndex = 0;
	}

	/**
	 * Adds this light to the appropriate light list.
	 */
	void AddToLightList();

	/** @return number of material elements in this primitive */
	virtual INT GetNumElements() const;
	/** @return MaterialInterface assigned to the given material index (if any) */
	virtual UMaterialInterface* GetElementMaterial(INT ElementIndex) const;
	/** sets the MaterialInterface to use for the given element index (if valid) */
	virtual void SetElementMaterial(INT ElementIndex, UMaterialInterface* InMaterial);

protected:
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Attach();
	virtual void UpdateTransform();
	virtual void Detach( UBOOL bWillReattach = FALSE );
public:
	virtual void InvalidateLightingCache();

	/** Invalidates the light's cached lighting with the option to recreate the light Guids. */
	void InvalidateLightingCacheInner(UBOOL bRecreateLightGuids, UBOOL bOnlyVisible);
};

enum ERadialImpulseFalloff
{
	RIF_Constant = 0,
	RIF_Linear = 1
};

//
//	UAudioComponent
//

class USoundCue;
class USoundNode;
class USoundNodeWave;
class UAudioComponent;

enum ELoopingMode
{
	/** One shot sound */
	LOOP_Never,
	/** Call the user callback on each loop for dynamic control */
	LOOP_WithNotification,
	/** Loop the sound forever */
	LOOP_Forever
};

/**
 * Structure encapsulating all information required to play a USoundNodeWave on a channel/source. This is required
 * as a single USoundNodeWave object can be used in multiple active cues or multiple times in the same cue.
 */
struct FWaveInstance
{
	/** Static helper to create good unique type hashes */
	static DWORD TypeHashCounter;

	/** Wave data */
	USoundNodeWave*		WaveData;
	/** Sound node to notify when the current audio buffer finishes */
	USoundNode*			NotifyBufferFinishedHook;
	/** Audio component this wave instance belongs to */
	UAudioComponent*	AudioComponent;

	/** Current volume */
	FLOAT				Volume;
	/** Current volume multiplier - used to zero the volume without stopping the source */
	FLOAT				VolumeMultiplier;
	/** Current priority */
	FLOAT				PlayPriority;
	/** Voice center channel volume */
	FLOAT				VoiceCenterChannelVolume;
	/** Volume of the radio filter effect */
	FLOAT				RadioFilterVolume;
	/** The volume at which the radio filter kicks in */
	FLOAT				RadioFilterVolumeThreshold;
	/** Set to TRUE if the sound nodes state that the radio filter should be applied */
	UBOOL				bApplyRadioFilter;
	/** Looping mode - None, loop with notification, forever */
	INT					LoopingMode;

	/** Whether wave instanced has been started */
	UBOOL				bIsStarted;
	/** Whether wave instanced is finished */
	UBOOL				bIsFinished;
	/** Whether the notify finished hook has been called since the last update/parsenodes */
	UBOOL				bAlreadyNotifiedHook;
	/** Whether to use spatialization */
	UBOOL				bUseSpatialization;
	/** If TRUE, wave instance is requesting a restart */
	UBOOL				bIsRequestingRestart;

	/** The amount of stereo sounds to bleed to the rear speakers */
	FLOAT				StereoBleed;
	/** The amount of a sound to bleed to the LFE channel */
	FLOAT				LFEBleed;

	/** Whether to apply audio effects */
	UBOOL				bEQFilterApplied;
	/** Whether to artificially keep active even at zero volume */
	UBOOL				bAlwaysPlay;
	/** Whether or not this sound plays when the game is paused in the UI */
	UBOOL				bIsUISound;
	/** Whether or not this wave is music */
	UBOOL				bIsMusic;
	/** Whether or not this wave has reverb applied */
	UBOOL				bReverb;
	/** Whether or not this sound class forces sounds to the center channel */
	UBOOL				bCenterChannelOnly;

	/** Low pass filter setting */
	FLOAT				HighFrequencyGain;
	/** Current pitch */
	FLOAT				Pitch;
	/** Current velocity */
	FVector				Velocity;
	/** Current location */
	FVector				Location;
	/** At what distance we start transforming into omnidirectional soundsource */
	FLOAT				OmniRadius;
	/** Cached type hash */
	DWORD				TypeHash;
	/** GUID for mapping of USoundNode reference to USoundNodeWave. */
	QWORD				ParentGUID;

	/**
	 * Constructor, initializing all member variables.
	 *
	 * @param InAudioComponent	Audio component this wave instance belongs to.
	 */
	FWaveInstance( UAudioComponent* InAudioComponent );

	/**
	 * Stops the wave instance without notifying NotifyWaveInstanceFinishedHook. This will NOT stop wave instance
	 * if it is set up to loop indefinitely or set to remain active.
 	 */
	void StopWithoutNotification( void );

	/**
	 * Notifies the wave instance that the current playback buffer has finished.
	 */
	UBOOL NotifyFinished( void );

	/**
	 * Friend archive function used for serialization.
	 */
	friend FArchive& operator<<( FArchive& Ar, FWaveInstance* WaveInstance );
};

inline DWORD GetTypeHash( FWaveInstance* A ) { return A->TypeHash; }

struct FAudioComponentSavedState
{
	USoundNode*								CurrentNotifyBufferFinishedHook;
	FVector									CurrentLocation;
	FLOAT									CurrentVolume;
	FLOAT									CurrentPitch;
	FLOAT									CurrentHighFrequencyGain;
	UBOOL									CurrentUseSpatialization;
	UBOOL									CurrentNotifyOnLoop;

	void Set( UAudioComponent* AudioComponent );
	void Restore( UAudioComponent* AudioComponent );

	static void Reset( UAudioComponent* AudioComonent );
};

struct FAudioComponentParam
{
	FName	ParamName;
	FLOAT	FloatParam;
	USoundNodeWave* WaveParam;
};

struct AudioComponent_eventOnAudioFinished_Parms
{
    class UAudioComponent* AC;
    AudioComponent_eventOnAudioFinished_Parms(EEventParm)
    {
    }
};

struct FSubtitleCue;
struct AudioComponent_eventOnQueueSubtitles_Parms
{
	TArray<FSubtitleCue>	Subtitles;
	FLOAT	CueDuration;
	AudioComponent_eventOnQueueSubtitles_Parms(EEventParm)
	{
	}
};

struct AudioComponent_eventOcclusionChanged_Parms
{
    UBOOL bNowOccluded;
    AudioComponent_eventOcclusionChanged_Parms(EEventParm)
    {
    }
};

//@warning: manually mirrored from ReverbVolume.uc
struct FInteriorSettings
{
    BITFIELD bIsWorldInfo:1;
    FLOAT ExteriorVolume;
    FLOAT ExteriorTime;
    FLOAT ExteriorLPF;
    FLOAT ExteriorLPFTime;
    FLOAT InteriorVolume;
    FLOAT InteriorTime;
    FLOAT InteriorLPF;
    FLOAT InteriorLPFTime;

    /** Constructors */
    FInteriorSettings() {}
    FInteriorSettings(EEventParm)
    {
        appMemzero(this, sizeof(FInteriorSettings));
    }
};

class UAudioComponent : public UActorComponent
{
	DECLARE_CLASS_NOEXPORT(UAudioComponent,UActorComponent,0,Engine);

	// Variables.
	class USoundCue*						SoundCue;
	USoundNode*								CueFirstNode;

	TArray<FAudioComponentParam>			InstanceParameters;

	/** Spatialise to the owner's coordinates */
	BITFIELD								bUseOwnerLocation:1;
	/** Auto start this component on creation */
	BITFIELD								bAutoPlay:1;
	/** Auto destroy this component on completion */
	BITFIELD								bAutoDestroy:1;
	/** Stop sound when owner is destroyed */
	BITFIELD								bStopWhenOwnerDestroyed:1;
	/** Whether the wave instances should remain active if they're dropped by the prioritization code. Useful for e.g. vehicle sounds that shouldn't cut out. */
	BITFIELD								bShouldRemainActiveIfDropped:1;
	/** whether we were occluded the last time we checked */
	BITFIELD								bWasOccluded:1;
	/** If true, subtitles in the sound data will be ignored. */
	BITFIELD								bSuppressSubtitles:1;
	/** Set to true when the component has resources that need cleanup */
	BITFIELD								bWasPlaying:1;
	/** Is this audio component allowed to be spatialized? */
	BITFIELD								bAllowSpatialization:1;
	/** Whether the current component has finished playing */
	BITFIELD								bFinished:1;
	/** If TRUE, this sound will not be stopped when flushing the audio device. */
	BITFIELD								bApplyRadioFilter:1;
	/** If TRUE, the decision on whether to apply the radio filter has been made. */
	BITFIELD								bRadioFilterSelected:1;
	/** Whether this audio component is previewing a sound */
	BITFIELD								bPreviewComponent:1;
	/** If TRUE, this sound will not be stopped when flushing the audio device. */
	BITFIELD								bIgnoreForFlushing:1;

	/**
	 * Properties of the audio component set by its owning sound class
	 */

	/** The amount of stereo sounds to bleed to the rear speakers */
	FLOAT									StereoBleed;
	/** The amount of a sound to bleed to the LFE channel */
	FLOAT									LFEBleed;

	/** Whether audio effects are applied */
	BITFIELD								bEQFilterApplied:1;
	/** Whether to artificially prioritise the component to play */
	BITFIELD								bAlwaysPlay:1;
	/** Whether or not this sound plays when the game is paused in the UI */
	BITFIELD								bIsUISound:1;
	/** Whether or not this audio component is a music clip */
	BITFIELD								bIsMusic:1;
	/** Whether or not the audio component has reverb applied */
	BITFIELD								bReverb:1;
	/** Whether or not this sound class forces sounds to the center channel */
	BITFIELD								bCenterChannelOnly:1;

	TArray<FWaveInstance*>					WaveInstances;
	TArray<BYTE>							SoundNodeData;
	TMap<USoundNode*,UINT>					SoundNodeOffsetMap;
	TMultiMap<USoundNode*,FWaveInstance*>	SoundNodeResetWaveMap;
	const struct FListener*					Listener;

	FLOAT									PlaybackTime;
	class APortalVolume*					PortalVolume;
	FVector									Location;
	FVector									ComponentLocation;

	/** Remember the last owner so we can remove it from the actor's component array even if it's already been detached */
	class AActor*							LastOwner;

	/** Used by the subtitle manager to prioritize subtitles wave instances spawned by this component. */
	FLOAT									SubtitlePriority;

	FLOAT									FadeInStartTime;
	FLOAT									FadeInStopTime;
	/** This is the volume level we are fading to **/
	FLOAT									FadeInTargetVolume;

	FLOAT									FadeOutStartTime;
	FLOAT									FadeOutStopTime;
	/** This is the volume level we are fading to **/
	FLOAT									FadeOutTargetVolume;

	FLOAT									AdjustVolumeStartTime;
	FLOAT									AdjustVolumeStopTime;
	/** This is the volume level we are adjusting to **/
	FLOAT									AdjustVolumeTargetVolume;
	FLOAT									CurrAdjustVolumeTargetVolume;

	// Temporary variables for node traversal.
	USoundNode*								CurrentNotifyBufferFinishedHook;
	FVector									CurrentLocation;
	FVector									CurrentVelocity;
	FLOAT									CurrentVolume;
	FLOAT									CurrentPitch;
	FLOAT									CurrentHighFrequencyGain;
	UBOOL									CurrentUseSpatialization;
	UBOOL									CurrentNotifyOnLoop;
	FLOAT									OmniRadius;

	// Multipliers used before propagation to WaveInstance
	FLOAT									CurrentVolumeMultiplier;
	FLOAT									CurrentPitchMultiplier;
	FLOAT									CurrentHighFrequencyGainMultiplier;

	// Accumulators used before propagation to WaveInstance (not simply multiplicative)
	FLOAT									CurrentVoiceCenterChannelVolume;
	FLOAT									CurrentRadioFilterVolume;
	FLOAT									CurrentRadioFilterVolumeThreshold;

	DOUBLE									LastUpdateTime;
	FLOAT									SourceInteriorVolume;
	FLOAT									SourceInteriorLPF;
	FLOAT									CurrentInteriorVolume;
	FLOAT									CurrentInteriorLPF;

	/** location last time playback was updated */
	FVector LastLocation;

	/** cache what volume settings we had last time so we don't have to search again if we didn't move */
	FInteriorSettings LastInteriorSettings;
	INT LastReverbVolumeIndex;

	// Serialized multipliers used to e.g. override volume for ambient sound actors.
	FLOAT									VolumeMultiplier;
	FLOAT									PitchMultiplier;
	FLOAT									HighFrequencyGainMultiplier;

	/** while playing, this component will check for occlusion from its closest listener every this many seconds
	 * and call OcclusionChanged() if the status changes
	 */
	FLOAT									OcclusionCheckInterval;
	/** last time we checked for occlusion */
	FLOAT									LastOcclusionCheckTime;

	class UDrawSoundRadiusComponent*		PreviewSoundRadius;

	FScriptDelegate __OnAudioFinished__Delegate;
	FScriptDelegate __OnQueueSubtitles__Delegate;

	// UObject interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void Serialize(FArchive& Ar);
	virtual void FinishDestroy();

#if USE_GAMEPLAY_PROFILER
	/** 
	 * This function actually does the work for the GetProfilerAssetObject and is virtual.  
	 * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
	 **/
	virtual UObject* GetProfilerAssetObjectInternal() const;
#endif

	/**
	 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
	 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
	 * you have a component of interest but what you really want is some characteristic that you can use to track
	 * down where it came from.  
	 *
	 */
	virtual FString GetDetailedInfoInternal() const;

	/** @name ActorComponent interface. */
	//@{
	virtual void Attach();
	virtual void Detach( UBOOL bWillReattach = FALSE );
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	/**
	 * Returns whether component should be marked as pending kill inside AActor::MarkComponentsAsPendingKill if it has
	 * a say in it. Components that should continue playback after the deletion of their owner return FALSE in this case.
	 *
	 * @return TRUE if we allow being marked as pending kill, FALSE otherwise
	 */
	virtual UBOOL AllowBeingMarkedPendingKill();
	//@}

	// UAudioComponent interface.
	void SetSoundCue( USoundCue* NewSoundCue );
	virtual void Play( void );
	virtual void Stop( void );
	virtual void UpdateWaveInstances( UAudioDevice* AudioDevice, TArray<FWaveInstance*> &WaveInstances, const TArray<struct FListener>& InListeners, FLOAT DeltaTime );
	
	/** 
	 * @param InListeners all listeners list
	 * @param ClosestListenerIndexOut through this variable index of the closest listener is returned 
	 * @return Closest RELATIVE location of sound (relative to position of the closest listener). 
	 */
	virtual FVector FindClosestLocation( const TArray<struct FListener>& InListeners, INT& ClosestListenerIndexOut );

	/**
	 * Return point, that should be used for evaluation distance, between listener, and sound source. That distance is used for attenuation.
	 * The function is needed when the speaker sound's position is estimated from a shape (AmbientSoundSpline)
	 */
	virtual FVector GetPointForDistanceEval();

	/** Returns TRUE if this component is currently playing a SoundCue. */
	UBOOL IsPlaying( void );

	/** @return TRUE if this component is currently fading in. */
	UBOOL IsFadingIn();

	/** @return TRUE if this component is currently fading out. */
	UBOOL IsFadingOut();

	/**
 	 * This is called in place of "play".  So you will say AudioComponent->FadeIn().
	 * This is useful for fading in music or some constant playing sound.
	 *
	 * If FadeTime is 0.0, this is the same as calling Play() but just modifying the volume by
	 * FadeVolumeLevel. (e.g. you will play instantly but the FadeVolumeLevel will affect the AudioComponent
	 *
	 * If FadeTime is > 0.0, this will call Play(), and then increase the volume level of this
	 * AudioCompoenent to the passed in FadeVolumeLevel over FadeInTime seconds.
	 *
	 * The VolumeLevel is MODIFYING the AudioComponent's "base" volume.  (e.g.  if you have an
	 * AudioComponent that is volume 1000 and you pass in .5 as your VolumeLevel then you will fade to 500 )
	 *
	 * @param FadeInDuration how long it should take to reach the FadeVolumeLevel
	 * @param FadeVolumeLevel the percentage of the AudioComponents's calculated volume in which to fade to
	 **/
	void FadeIn( FLOAT FadeInDuration, FLOAT FadeVolumeLevel );

	/**
	 * This is called in place of "stop".  So you will say AudioComponent->FadeOut().
	 * This is useful for fading out music or some constant playing sound.
	 *
	 * If FadeTime is 0.0, this is the same as calling Stop().
	 *
	 * If FadeTime is > 0.0, this will decrease the volume level of this
	 * AudioCompoenent to the passed in FadeVolumeLevel over FadeInTime seconds.
	 *
	 * The VolumeLevel is MODIFYING the AudioComponent's "base" volume.  (e.g.  if you have an
	 * AudioComponent that is volume 1000 and you pass in .5 as your VolumeLevel then you will fade to 500 )
	 *
	 * @param FadeOutDuration how long it should take to reach the FadeVolumeLevel
	 * @param FadeVolumeLevel the percentage of the AudioComponents's calculated volume in which to fade to
	 **/
	void FadeOut( FLOAT FadeOutDuration, FLOAT FadeVolumeLevel );

	/**
 	 * This will allow one to adjust the volume of an AudioComponent on the fly
	 **/
	void AdjustVolume( FLOAT AdjustVolumeDuration, FLOAT AdjustVolumeLevel );

	/** if OcclusionCheckInterval > 0.0, checks if the sound has become (un)occluded during playback
	 * and calls eventOcclusionChanged() if so
	 * primarily used for gameplay-relevant ambient sounds
	 * CurrentLocation is the location of this component that will be used for playback
	 * @param ListenerLocation location of the closest listener to the sound
	 */
	void CheckOcclusion(const FVector& ListenerLocation);

	/**
	 * Apply the interior settings to the ambient sound as appropriate
	 * TrueSpeakerPosition is needed for Spline actors, because the InteriorVolume should not use the Virtual Speaker Position, for regular sounds it is just CurrentLocation 
	 */
	void HandleInteriorVolumes( UAudioDevice* AudioDevice, class AWorldInfo* WorldInfo, UBOOL AlwaysRecalculate, const FVector& TrueSpeakerPosition );

	void SetFloatParameter(FName InName, FLOAT InFloat);
	void SetWaveParameter(FName InName, USoundNodeWave* InWave);
	UBOOL GetFloatParameter(FName InName, FLOAT& OutFloat);
	UBOOL GetWaveParameter(FName InName, USoundNodeWave*& OutWave);

	virtual FLOAT GetDuration();

	/**
	 * Dissociates component from audio device and deletes wave instances.
	 */
	virtual void Cleanup( void );

	/** stops the audio (if playing), detaches the component, and resets the component's properties to the values of its template */
	void ResetToDefaults();

	DECLARE_FUNCTION(execPlay);
    DECLARE_FUNCTION(execPause);
	DECLARE_FUNCTION(execStop);
	DECLARE_FUNCTION(execIsPlaying);
	DECLARE_FUNCTION(execIsFadingIn);
	DECLARE_FUNCTION(execIsFadingOut);
	DECLARE_FUNCTION(execSetFloatParameter);
	DECLARE_FUNCTION(execSetWaveParameter);
	DECLARE_FUNCTION(execFadeIn);
	DECLARE_FUNCTION(execFadeOut);
	DECLARE_FUNCTION(execAdjustVolume);
	DECLARE_FUNCTION(execResetToDefaults)
	{
		P_FINISH;
		ResetToDefaults();
	}

	void delegateOnAudioFinished(UAudioComponent* AC)
	{
		AudioComponent_eventOnAudioFinished_Parms Parms(EC_EventParm);
		Parms.AC = AC;
		ProcessDelegate(NAME_OnAudioFinished, &__OnAudioFinished__Delegate, &Parms);
	}

	void delegateOnQueueSubtitles(TArray<FSubtitleCue> Subtitles, FLOAT CueDuration)
	{
		AudioComponent_eventOnQueueSubtitles_Parms Parms(EC_EventParm);
		Parms.Subtitles = Subtitles;
		Parms.CueDuration = CueDuration;
		ProcessDelegate(FName(TEXT("OnQueueSubtitles")), &__OnQueueSubtitles__Delegate, &Parms);
	}

	void eventOcclusionChanged(UBOOL bNowOccluded)
	{
		AudioComponent_eventOcclusionChanged_Parms Parms(EC_EventParm);
		Parms.bNowOccluded=bNowOccluded ? FIRST_BITFIELD : FALSE;
		ProcessEvent(FindFunctionChecked(FName(TEXT("OcclusionChanged"))), &Parms);
	}

protected:
	FLOAT GetFadeInMultiplier() const;
	FLOAT GetFadeOutMultiplier() const;
	/** Helper function to do determine the fade volume value based on start, start, target volume levels **/
	FLOAT FadeMultiplierHelper( FLOAT FadeStartTime, FLOAT FadeStopTime, FLOAT FadeTargetValue ) const;

	FLOAT GetAdjustVolumeOnFlyMultiplier();
};

/**
 * AudioComponent, where source of sound is defined by spline curve. (Current location of sound is in the closest point on spline to the listener location.)
 */
typedef FInterpCurveVector::FPointOnSpline FInterpPointOnSpline;

/**
 * Detaches a component for the lifetime of this class.
 *
 * Typically used by constructing the class on the stack:
 * {
 *		FComponentReattachContext ReattachContext(this);
 *		// The component is removed from the scene here as ReattachContext is constructed.
 *		...
 * }	// The component is returned to the scene here as ReattachContext is destructed.
 */
class FComponentReattachContext
{
private:
	UActorComponent* Component;
	FSceneInterface* Scene;
	AActor* Owner;

public:
	FComponentReattachContext(UActorComponent* InComponent)
		: Scene(NULL)
		, Owner(NULL)
	{
		check(InComponent);
		checkf(!InComponent->HasAnyFlags(RF_Unreachable), TEXT("%s"), *InComponent->GetFullName());
		if((InComponent->IsAttached() || !InComponent->IsValidComponent()) && InComponent->GetScene())
		{
			Component = InComponent;

			// Detach the component from the scene.
			if(Component->bAttached)
			{
				Component->Detach( TRUE );	// Will reattach?
			}

			// Save the component's owner.
			Owner = Component->GetOwner();
			Component->Owner = NULL;

			// Save the scene and set the component's scene to NULL to prevent a nested FComponentReattachContext from reattaching this component.
			Scene = Component->GetScene();
			Component->Scene = NULL;
		}
		else
		{
			Component = NULL;
		}
	}
	~FComponentReattachContext()
	{
		if( Component )
		{
			const UBOOL bIsValidComponent = Component->IsValidComponent();
			if( bIsValidComponent )
			{
				// If the component has been reattached since the recreate context was constructed, detach it so Attach isn't called on an already
				// attached component.
				if( Component->IsAttached() )
				{
					Component->Detach( TRUE );	// WIll reattach?
				}
			}

			// Since the component was attached when this context was constructed, we can assume that ParentToWorld has been called at some point.
			Component->Scene = Scene;
			Component->Owner = Owner;

			if( bIsValidComponent )
			{
				Component->Attach();
			}

			// Notify the streaming system. Will only update the component data if it's already tracked.
			const UPrimitiveComponent* Primitive = ConstCast<UPrimitiveComponent>(Component);
			if ( Primitive )
			{
				GStreamingManager->NotifyPrimitiveUpdated( Primitive );
			}
		}
	}
};

/**
 * Removes a component from the scene for the lifetime of this class. This code does NOT handle nested calls.
 *
 * Typically used by constructing the class on the stack:
 * {
 *		FPrimitiveSceneAttachmentContext ReattachContext(this);
 *		// The component is removed from the scene here as ReattachContext is constructed.
 *		...
 * }	// The component is returned to the scene here as ReattachContext is destructed.
 */
class FPrimitiveSceneAttachmentContext
{
private:
	/** Cached primitive passed in to constructor, can be NULL if we don't need to re-add. */
	UPrimitiveComponent* Primitive;
	/** Scene of cached primitive */
	FSceneInterface* Scene;

public:
	/** Constructor, removing primitive from scene and caching it if we need to readd */
	FPrimitiveSceneAttachmentContext(UPrimitiveComponent* InPrimitive );
	/** Destructor, adding primitive to scene again if needed. */
	~FPrimitiveSceneAttachmentContext();
};

/** Causes all components to be re-attached to the scene after class goes out of scope. */
class FGlobalPrimitiveSceneAttachmentContext
{
public:
	/** Initialization constructor. */
	FGlobalPrimitiveSceneAttachmentContext()
	{
		// wait until resources are released
		FlushRenderingCommands();

		// Detach all primitive components from the scene.
		for(TObjectIterator<UPrimitiveComponent> ComponentIt;ComponentIt;++ComponentIt)
		{
			new(ComponentContexts) FPrimitiveSceneAttachmentContext(*ComponentIt);
		}
	}

private:
	/** The recreate contexts for the individual components. */
	TIndirectArray<FPrimitiveSceneAttachmentContext> ComponentContexts;
};

/** Removes all components from their scenes for the lifetime of the class. */
class FGlobalComponentReattachContext
{
public:
	/** 
	* Initialization constructor. 
	*/
	FGlobalComponentReattachContext();
	
	/** 
	* Initialization constructor. 
	*
	* @param ExcludeComponents - Component types to exclude when reattaching 
	*/
	FGlobalComponentReattachContext(const TArray<UClass*>& ExcludeComponents);

	/**
	 * Initialization constructor
	 * Only re-attach those components whose replacement primitive is in a direct child of one of the InParentActors
	 * 
	 * @param InParentActors - list of actors called out for reattachment
	 */
	 FGlobalComponentReattachContext(const TArray<AActor*>& InParentActors);


	/** Destructor */
	~FGlobalComponentReattachContext();

	/** Indicates that a FGlobalComponentReattachContext is currently active */
	static INT ActiveGlobalReattachContextCount;

private:
	/** The recreate contexts for the individual components. */
	TIndirectArray<FComponentReattachContext> ComponentContexts;
};

/** Removes all components of the templated type from their scenes for the lifetime of the class. */
template<class ComponentType>
class TComponentReattachContext
{
public:
	/** Initialization constructor. */
	TComponentReattachContext()
	{
		// Don't do this on ES2 since it may block on shader compiling
		if ( !GUsingES2RHI )
		{
			// wait until resources are released
			FlushRenderingCommands();
		}

		// Detach all components of the templated type.
		for(TObjectIterator<ComponentType> ComponentIt;ComponentIt;++ComponentIt)
		{
			new(ComponentContexts) FComponentReattachContext(*ComponentIt);
		}
	}

private:
	/** The recreate contexts for the individual components. */
	TIndirectArray<FComponentReattachContext> ComponentContexts;
};

/** Represents a wind source component to the scene manager in the rendering thread. */
class FWindSourceSceneProxy
{
public:

	/** Initialization constructor. */
	FWindSourceSceneProxy(const FVector& InDirection,FLOAT InStrength,FLOAT InSpeed):
		Position(FVector(0,0,0)),
		Direction(InDirection),
		Strength(InStrength),
		Speed(InSpeed),
		Radius(0),
		bIsPointSource(FALSE)
	{}

	/** Initialization constructor. */
	FWindSourceSceneProxy(const FVector& InPosition,FLOAT InStrength,FLOAT InSpeed,FLOAT InRadius):
		Position(InPosition),
		Direction(FVector(0,0,0)),
		Strength(InStrength),
		Speed(InSpeed),
		Radius(InRadius),
		bIsPointSource(TRUE)
	{}

	UBOOL GetWindParameters(const FVector& EvaluatePosition, FVector4& WindDirectionAndSpeed, FLOAT& Strength) const;

private:

	FVector Position;
	FVector	Direction;
	FLOAT Strength;
	FLOAT Speed;
	FLOAT Radius;
	UBOOL bIsPointSource;
};

/** Represents the scene render data for a URadialBlurComponent */
class FRadialBlurSceneProxy
{
public:
	/** The component attached to the scene */
	const URadialBlurComponent* RadialBlurComponent;
	/** Current world position of the radial blur component */
	const FVector WorldPosition;
	/** Material to be applied to the screen blur */
	FMaterialRenderProxy* MaterialProxy;
	/** Cached bound shader state for the pixel/vertex/decl of the effect */
	FBoundShaderStateRHIRef BoundShaderState;
	/** Cached bound shader state for the pixel/vertex/decl of the effect for rendering velocities */
	FBoundShaderStateRHIRef BoundShaderStateVelocity;
	/** Desired Depth Priority Group ordering of the effect */
	const UINT DesiredDPG;
	/** Scale for the overall blur vectors */
	const FLOAT BlurScale;
	/** Exponent for falloff rate of blur vectors */
	const FLOAT BlurFalloffExp;
	/** Amount to alpha blend the blur effect with the existing scene */
	const FLOAT BlurOpacity;
	/** Max distance where effect is rendered. If further than this then culled */
	const FLOAT  MaxCullDistance;
	/** Rate of falloff based on distance from view origin */
	const FLOAT  DistanceFalloffExponent;
	/** 
	 * if TRUE then radial blur vectors are rendered to the velocity buffer 
	 * instead of being used to manually sample scene color values 
	 */
	const UBOOL bRenderAsVelocity;
	
	/**
	 * Constructor
	 * @param InRadialBlurComponent - Component this proxy is being created for
	 */
	FRadialBlurSceneProxy(class URadialBlurComponent* InRadialBlurComponent);
	
	/**
	 * Draw the radial blur effect for this proxy directly to the scene color buffer.
	 *
	 * @param View - View region that is currently being rendered for the scene
	 * @param DPGIndex - Current Depth Priority Group being rendered for the scene
	 * @param Flags - Scene rendering flags
	 * @return TRUE if the effect was rendered
	 */
	UBOOL Draw(const class FSceneView* View,UINT DPGIndex,DWORD Flags);

	/**
	 * Draw the radial blur effect for this proxy to the velocity buffer.
	 *
	 * @param View - View region that is currently being rendered for the scene
	 * @param DPGIndex - Current Depth Priority Group being rendered for the scene
	 * @param Flags - Scene rendering flags
	 * @return TRUE if the effect was rendered
	 */
	UBOOL DrawVelocity(const class FSceneView* View,UINT DPGIndex,DWORD Flags);

	/**
	 * Determine if the radial blur proxy is renderable for the current view
	 *
	 * @param View - View region that is currently being rendered for the scene
	 * @param DPGIndex - Current Depth Priority Group being rendered for the scene
	 * @param Flags - Scene rendering flags
	 * @return TRUE if the effect should be rendered
	 */
	UBOOL IsRenderable(const class FSceneView* View,UINT DPGIndex,DWORD Flags) const;

	/**
	 * Calculate the final blur scale to render radial blur based on 
	 * configured parameters and distance to the view origin.
	 *
	 * @param View - View region that is currently being rendered for the scene
	 * @return Blur scale float (can be negative)
	 */
	FLOAT CalcBlurScale(const class FSceneView* View) const;
};
