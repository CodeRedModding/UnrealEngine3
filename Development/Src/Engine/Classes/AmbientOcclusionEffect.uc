/**
 * AmbientOcclusionEffect - A screen space ambient occlusion implementation.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AmbientOcclusionEffect extends PostProcessEffect
	native;

/** The color that will replace scene color where there is a lot of occlusion. */
var(Color) interp LinearColor OcclusionColor;

/** 
 * Power to apply to the calculated occlusion value. 
 * Higher powers result in more contrast, but will need other factors like OcclusionScale to be tweaked as well. 
 */
var(Color) float OcclusionPower <UIMin=0.1 | UIMax=20.0>;

/** Scale to apply to the calculated occlusion value. */
var(Color) float OcclusionScale <UIMin=0.0 | UIMax=10.0>;

/** Bias to apply to the calculated occlusion value. */
var(Color) float OcclusionBias <UIMin=-1.0 | UIMax=4.0>;

/** Minimum occlusion value after all other transforms have been applied. */
var(Color) float MinOcclusion;

/** SSAO2 is SSAO with quality improvements, it is now the new method so the flag is no longer needed */
var deprecated bool SSAO2;

/** SSAO quality improvements, less noise, more detail, no darkening of flat surfaces, no overbright on convex, parameter retweak needed */
var(Occlusion) bool bAngleBasedSSAO;

/** Distance to check around each pixel for occluders, in world units. */
var(Occlusion) float OcclusionRadius <UIMin=0.0 | UIMax=256.0>;

/** Attenuation factor that determines how much to weigh in samples based on distance, larger values result in a faster falloff over distance. */
var deprecated float OcclusionAttenuation <UIMin=0.0 | UIMax=10.0>;

enum EAmbientOcclusionQuality
{
	AO_High,
	AO_Medium,
	AO_Low
};

/** 
 * Quality of the ambient occlusion effect.  Low quality gives the best performance and is appropriate for gameplay.  
 * Medium quality smooths noise between frames at a slightly higher performance cost.  High quality uses extra samples to preserve detail.
 */
var(Occlusion) EAmbientOcclusionQuality OcclusionQuality;

/** 
 * Distance at which to start fading out the occlusion factor, in world units. 
 * This is useful for hiding distant artifacts on skyboxes.
 */
var(Occlusion) float OcclusionFadeoutMinDistance;

/** Distance at which the occlusion factor should be fully faded, in world units. */
var(Occlusion) float OcclusionFadeoutMaxDistance;

/** 
 * Distance in front of a pixel that an occluder must be to be considered a different object, in world units.  
 * This threshold is used to identify halo regions around nearby objects, for example a first person weapon.
 */
var(Halo) float HaloDistanceThreshold;

/** 
 * Scale factor to increase HaloDistanceThreshold for distant pixels.  
 * A value of .001 would result in HaloDistanceThreshold being 1 unit larger at a distance of 1000 world units. 
 */
var(Halo) float HaloDistanceScale;

/** 
 * Occlusion factor to assign to samples determined to be contributing to a halo.  
 * 0 would result in full occlusion for that sample, increasing values map to quadratically decreasing occlusion values.
 */
var(Halo) float HaloOcclusion;

/** Difference in depth that two pixels must be to be considered an edge, and therefore not blurred across, in world units. */
var(Filter) float EdgeDistanceThreshold;

/** 
 * Scale factor to increase EdgeDistanceThreshold for distant pixels.  
 * A value of .001 would result in EdgeDistanceThreshold being 1 unit larger at a distance of 1000 world units. 
 */
var(Filter) float EdgeDistanceScale;

/** 
 * Distance in world units which should map to the kernel size in screen space.  
 * This is useful to reduce filter kernel size for distant pixels and keep detail, at the cost of leaving more noise in the result.
 */
var(Filter) float FilterDistanceScale;

/** Size of the blur filter, in pixels. */
var deprecated int FilterSize;

/** 
 * Time in which the occlusion history should approximately converge.  
 * Longer times (.5s) allow more smoothing between frames and less noise but history streaking is more noticeable.
 * 0 means the feature is off (less GPU performance and memory overhead)
 */
var(History) float HistoryConvergenceTime;

/** 
 * Time in which the weight history should approximately converge.  
 */
var float HistoryWeightConvergenceTime;

cpptext
{
    // UPostProcessEffect interface

	/**
	 * Creates a proxy to represent the render info for a post process effect
	 * @param WorldSettings - The world's post process settings for the view.
	 * @return The proxy object.
	 */
	virtual class FPostProcessSceneProxy* CreateSceneProxy(const FPostProcessSettings* WorldSettings);

	/**
	 * @param View - current view
	 * @return TRUE if the effect should be rendered
	 */
	virtual UBOOL IsShown(const FSceneView* View) const;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
}

defaultproperties
{
	bAffectsLightingOnly=TRUE
	SceneDPG = SDPG_World;
	OcclusionColor=(R=0.0,G=0.0,B=0.0,A=1.0)
	OcclusionPower=4.0
	OcclusionScale=20.0
	OcclusionBias=0
	MinOcclusion=.1
	OcclusionRadius=25.0
	OcclusionQuality=AO_Medium
	OcclusionFadeoutMinDistance=4000.0
	OcclusionFadeoutMaxDistance=4500.0
	HaloDistanceThreshold=40.0
	HaloDistanceScale=.1
	HaloOcclusion=.04
	EdgeDistanceThreshold=10.0
	EdgeDistanceScale=.003
	FilterDistanceScale=10.0
	HistoryConvergenceTime=0
	HistoryWeightConvergenceTime=.07
	bAngleBasedSSAO=FALSE
}