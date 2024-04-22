/**
 * This is used by the scene management to isolate lights and primitives.  
 * For lighting and actor or component use a DynamicLightEnvironmentComponent.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LightEnvironmentComponent extends ActorComponent
	native(Light);

/** Whether the light environment is used or treated the same as a LightEnvironment=NULL reference. */
var() protected{protected} const bool bEnabled;

/** Whether the light environment should override GSystemSettings.bUseCompositeDynamicLights, and never composite dynamic lights into the light environment. */
var bool bForceNonCompositeDynamicLights;

/** 
 * Whether lit translucency using this light environment is allowed to receive dynamic shadows from the static environment.
 * When FALSE, cheaper on/off shadowing will be applied based on the distance to the dominant shadow transition.
 */
var bool bAllowDynamicShadowsOnTranslucency;

/** Whether primitives using this light environment will create a preshadow (dynamic shadow from the static environment onto a dynamic object). */
var const transient protected {protected} bool bAllowPreShadow;

/** Shadowing factor applied to AffectingDominantLight. */
var const transient protected {protected} float DominantShadowFactor;

/** Contains the shadow factor used on translucency using this light environment when bAllowDynamicShadowsOnTranslucency is FALSE. */
var const transient protected {protected} bool bTranslucencyShadowed;

/** The single dominant light that is allowed to affect this light environment. */
var const transient protected {protected} LightComponent AffectingDominantLight;

/** Array of primitive components which are using this light environment and currently attached. */
var const transient protected {protected} array<PrimitiveComponent> AffectedComponents;

cpptext
{
	/**
	 * Signals to the light environment that a light has changed, so the environment may need to be updated.
	 * @param Light - The light that changed.
	 */
	virtual void UpdateLight(const ULightComponent* Light) {}

	// Methods that update AffectedComponents
	void AddAffectedComponent(UPrimitiveComponent* NewComponent);
	void RemoveAffectedComponent(UPrimitiveComponent* OldComponent);

	// Accessors
	const ULightComponent* GetAffectingDominantLight() const { return AffectingDominantLight; }
	UBOOL AllowDynamicShadowsOnTranslucency() const { return bAllowDynamicShadowsOnTranslucency; }
	UBOOL IsTranslucencyShadowed() const { return bTranslucencyShadowed; }
	UBOOL AllowPreShadow() const { return bAllowPreShadow; }
	FLOAT GetDominantShadowFactor() const { return DominantShadowFactor; }
	virtual void SetNeedsStaticUpdate() {}
	const TArrayNoInit<UPrimitiveComponent*>& GetAffectedComponents() const { return AffectedComponents; }

	/** Adds lights that affect this Light Environment to RelevantLightList. */
	virtual void AddRelevantLights(TArray<ALight*>& RelevantLightList, UBOOL bDominantOnly) const {}

	friend class FDynamicLightEnvironmentState;
	friend void DrawLightEnvironmentDebugInfo(const FSceneView* View,FPrimitiveDrawInterface* PDI);
}

/**
 * Changes the value of bEnabled.
 * @param bNewEnabled - The value to assign to bEnabled.
 */
native final function SetEnabled(bool bNewEnabled);

/** Returns whether the light environment is enabled */
native final function bool IsEnabled() const;

defaultproperties
{
	bEnabled=True
	bAllowPreShadow=True
	DominantShadowFactor=1.0
}
