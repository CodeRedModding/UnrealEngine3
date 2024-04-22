/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DirectionalLightComponent extends LightComponent
	native(Light)
	hidecategories(Object)
	dependson(EngineTypes)
	editinlinenew;

/**
 * Trace distance for static lighting. Objects further than TraceDistance away from an object won't be taken into 
 * account for static shadowing applied to said object. This is used to work around floating point consistency
 * issues in the collision code with regard to very long traces. The old default was WORLD_MAX.
 */
var(AdvancedLighting)	float	TraceDistance;

/** 
 * Radius of the whole scene dynamic shadow centered on the viewer, which replaces the precomputed shadows based on distance from the camera.  
 * A Radius of 0 disables the dynamic shadow. This feature is currently only supported on dominant directional lights.
 */
var(CascadedShadowMaps) interp float WholeSceneDynamicShadowRadius;

/** 
 * Number of cascades to split the view frustum into for the whole scene dynamic shadow.  
 * More cascades result in better shadow resolution and allow WholeSceneDynamicShadowRadius to be further, but add rendering cost.
 */
var(CascadedShadowMaps) const int NumWholeSceneDynamicShadowCascades;

/** 
 * Exponent that is applied to the cascade transition distances as a fraction of WholeSceneDynamicShadowRadius.
 * An exponent of 1 means that cascade transitions will happen at a distance proportional to their resolution.
 * A value greater than 1 brings transitions closer to the camera.
 */
var(CascadedShadowMaps) const float CascadeDistributionExponent;

/** The Lightmass settings for this object. */
var(Lightmass) LightmassDirectionalLightSettings	LightmassSettings <ScriptOrder=true>;

cpptext
{
	virtual FLightSceneInfo* CreateSceneInfo() const;
	virtual FVector4 GetPosition() const;
	virtual ELightComponentType GetLightType() const;

	/**
	 * Called after property has changed via e.g. property window or set command.
	 *
	 * @param	PropertyThatChanged	UProperty that has been changed, NULL if unknown
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
}

/** Called from matinee code when LightColor property changes. */
function OnUpdatePropertyLightColor()
{
	UpdateColorAndBrightness();
}

/** Called from matinee code when Brightness property changes. */
function OnUpdatePropertyBrightness()
{
	UpdateColorAndBrightness();
}

defaultproperties
{
	TraceDistance=100000
	NumWholeSceneDynamicShadowCascades=1
	CascadeDistributionExponent=4
}
