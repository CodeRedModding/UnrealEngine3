/**
 * This is used to light components / actors during the game.  Doing something like:
 * LightEnvironment=FooLightEnvironment
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DynamicLightEnvironmentComponent extends LightEnvironmentComponent
	native(Light);

/** The current state of the light environment. */
var private native transient const pointer State{class FDynamicLightEnvironmentState};

/** The number of seconds between light environment updates for actors which aren't visible. */
var() float InvisibleUpdateTime;

/** Minimum amount of time that needs to pass between full environment updates. */
var() float MinTimeBetweenFullUpdates;

/** Scales the velocity based update factor.  Values near 0 mean velocity will not be a factor in update rate. */
var float VelocityUpdateTimeScale;

/** 
 * Speed to interpolate the current shadow to the newly captured shadow.  
 * A value of .01 means the interpolation will be complete after the DLE moves 100 Unreal Units. 
 */
var float ShadowInterpolationSpeed;

/** The number of visibility samples to use within the primitive's bounding volume. */
var() int NumVolumeVisibilitySamples;

/** Scales the bounds used for light environment calculations. */
var() float LightingBoundsScale;

/** The color of the ambient shadow. */
var LinearColor AmbientShadowColor;

/** The direction of the ambient shadow source. */
var vector AmbientShadowSourceDirection;

/** Ambient color added in addition to the level's lighting. */
var LinearColor AmbientGlow;

/** The distance to create the light from the owner's origin, in radius units. */
var float LightDistance;

/** The distance for the shadow to project beyond the owner's origin, in radius units. */
var float ShadowDistance;

/** Whether the light environment should cast shadows */
var() bool bCastShadows;

/** Whether the light environment's shadow includes the effect of dynamic lights. */
var bool bCompositeShadowsFromDynamicLights;

/** Whether to represent all lights with the light environment, including dominant lights which are usually rendered separately. */
var bool bForceCompositeAllLights;

/** 
 * Whether to be affected by small dynamic lights (like muzzle flashes) which may expose artifacts since the whole DLE will be lit up by them. 
 * If FALSE, dynamic lights smaller than the DLE will not affect the DLE.
 */
var bool bAffectedBySmallDynamicLights;

/** 
 * Whether to use cheap on/off shadowing from the environment or allow a dynamic preshadow. 
 */
var() bool bUseBooleanEnvironmentShadowing;

/** Whether the light environment should be shadowed by the static environment. */
var bool bShadowFromEnvironment;

/** Time since the caster was last visible at which the mod shadow will fade out completely.  */
var float ModShadowFadeoutTime;

/** Exponent that controls mod shadow fadeout curve. */
var float ModShadowFadeoutExponent;

/** Brightest ModulatedShadowColor allowed for the shadow.  This can be used to limit the DLE's shadow to a specified darkness. */
var LinearColor MaxModulatedShadowColor;

/** 
 * The distance from the dominant light shadow transition at which to start fading out the DLE's modulated shadow and primary light. 
 * This must be larger than DominantShadowTransitionEndDistance.
 */
var float DominantShadowTransitionStartDistance;

/** 
 * The distance from the dominant light shadow transition at which to end fading out the DLE's modulated shadow and primary light. 
 * This must be smaller than DominantShadowTransitionStartDistance.
 */
var float DominantShadowTransitionEndDistance;

/** Whether the light environment should be dynamically updated. */
var() bool bDynamic;

/** Whether a directional light should be used to synthesize the dominant lighting in the environment. */
var bool bSynthesizeDirectionalLight;

/**
 * Whether a SH light should be used to synthesize all light not accounted for by the synthesized directional light.
 * If not, a sky light is used instead.  Using an SH light gives higher quality secondary lighting, but at a steeper performance cost.
 */
var() bool bSynthesizeSHLight;

/**
 * The minimum angle to allow between the shadow direction and horizontal.  An angle > 0 constrains the shadow to never be cast from a light
 * below horizontal.
 */
var float MinShadowAngle;

/** Whether this is an actor that can't tolerate latency in lighting updates; a full lighting update is done every frame. */
var() bool bRequiresNonLatentUpdates;

/* 
 * Whether to do visibility traces from the closest point on the bounds to the light, or just from the center of the bounds. 
 * This is useful when using a DLE on an object that is likely embedded in shadow casting objects (ie fractured meshes).
 */
var bool bTraceFromClosestBoundsPoint;

/** 
 * Whether this light environment is being applied to a character 
 * And should be affected by character specific lighting like WorldInfo's CharacterLightingContrastFactor. 
 */
var() bool bIsCharacterLightEnvironment;

/** 
 * Methods used to calculate the bounds that this light environment will use as a representation of what it is lighting.
 * The default settings will trace one ray from the center of the calculated bounds to each relevant light.
 */
enum EDynamicLightEnvironmentBoundsMethod
{
	/** The default DLE bounds method, starts with a small sphere at the Owner's origin and adds each component of Owner using this DLE. */
	DLEB_OwnerComponents,
	/** Uses OverriddenBounds, doesn't depend on Owner at all. */
	DLEB_ManualOverride,
	/** 
	 * Accumulates the bounds of attached components on any actor using this DLE.  
	 * This is useful when the DLE is lighting something whose Owner is placed in the world, like a pool actor.
	 * This method only works when the components using this DLE are attached before the DLE is updated.
	 */
	DLEB_ActiveComponents
};

var EDynamicLightEnvironmentBoundsMethod BoundsMethod;

/* The bounds to use for visibility calculations if BoundsMethod==DLEB_ManualOverride. */
var BoxSphereBounds OverriddenBounds;

/* Whether to override the lighting channels of the owner with OverriddenLightingChannels. */
var bool bOverrideOwnerLightingChannels;

/* The lighting channels to use if bOverrideOwnerLightingChannels is enabled. */
var LightingChannelContainer OverriddenLightingChannels;

/** Light components which override lights in GWorld, useful for rendering light environments in preview scenes. */
var const array<LightComponent> OverriddenLightComponents;

/** When enabled for light environments influenced by a dominant directional light, this indicates that the object should always be treated as influenced by the directional light and the engine should never search for a dominant shadow transition.  Enabling this can greatly improve performance for actors with huge bounds that are dynamically lit by dominant directional lights. */
var bool bAlwaysInfluencedByDominantDirectionalLight;

cpptext
{
	// UObject interface.
	virtual void FinishDestroy();
	virtual void AddReferencedObjects( TArray<UObject*>& ObjectArray );
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// UActorComponent interface.
	virtual void Tick(FLOAT DeltaTime);
	virtual void Attach();
	virtual void UpdateTransform();
	virtual void Detach( UBOOL bWillReattach = FALSE );
	virtual void BeginPlay();
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
	/** Adds lights that affect this DLE to RelevantLightList. */
	virtual void AddRelevantLights(TArray<ALight*>& RelevantLightList, UBOOL bDominantOnly) const;

	// ULightEnvironmentComponent interface.
	virtual void UpdateLight(const ULightComponent* Light);
	virtual void SetNeedsStaticUpdate();

	friend class FDynamicLightEnvironmentState;

protected:

	virtual UBOOL NeedsUpdateBasedOnComponent(UPrimitiveComponent* Component) const;

#if USE_GAMEPLAY_PROFILER
    /** 
     * This function actually does the work for the GetProfilerAssetObject and is virtual.  
     * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
     */
	virtual UObject* GetProfilerAssetObjectInternal() const;
#endif

	/**
     * This function actually does the work for the GetDetailInfo and is virtual.
     * It should only be called from GetDetailedInfo as GetDetailedInfo is safe to call on NULL object pointers
     */
	virtual FString GetDetailedInfoInternal() const;
}

/* Forces a full update the of the dynamic and static environments on the next Tick. */
native final function ResetEnvironment();

defaultproperties
{
	InvisibleUpdateTime=5.0
	MinTimeBetweenFullUpdates=1.0
	NumVolumeVisibilitySamples=1
	// By default, don't update fast moving objects more often
	VelocityUpdateTimeScale=0.000001
	LightingBoundsScale=1
	// Using a relatively slow speed so that the shadow is mostly interpolating which hides the low update frequency
	ShadowInterpolationSpeed=.004
	AmbientShadowColor=(R=0.001,G=0.001,B=0.001)
	AmbientShadowSourceDirection=(X=0.01,Y=0,Z=0.99)
	LightDistance=10.0
	ShadowDistance=5.0
	// bRequiresNonLatentUpdates sets it to TG_PostUpdateWork in BeginPlay()
	TickGroup=TG_DuringAsyncWork
	bCastShadows=TRUE
	bCompositeShadowsFromDynamicLights=TRUE
	bAffectedBySmallDynamicLights=TRUE
	// Cheap default
	bUseBooleanEnvironmentShadowing=TRUE
	bShadowFromEnvironment=TRUE
	ModShadowFadeoutExponent=3.0
    MaxModulatedShadowColor=(R=0.5,G=0.5,B=0.5)
    DominantShadowTransitionStartDistance=100
    DominantShadowTransitionEndDistance=10
	bDynamic=TRUE
	bSynthesizeDirectionalLight=TRUE
	bSynthesizeSHLight=FALSE
	MinShadowAngle=25.0
	BoundsMethod=DLEB_OwnerComponents
}
