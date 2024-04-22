/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
/**
 * This is a content only archetype object to allow artists to update explosion
 * content without programmer intervention
 */
class GameExplosionContent extends Object
	editinlinenew;

/** TRUE to attempt to automatically do force feedback to match the camera shake */
var() const bool                      bAutoControllerVibration;

//
// Audio parameters
//

/** Audio to play at explosion time. */
var(Audio) const SoundCue ExplosionSound;

//
// Camera parameters
//

/** TRUE to rotate CamShake to play radially relative to the explosion.  Left/Right/Rear will be ignored. */
var(Camera) const bool						bOrientCameraShakeTowardsEpicenter;
/** Shake to play when source is in front of the camera, or when directional variants are unspecified. */
var(Camera) const CameraShake    CamShake;
/** Anim to play when the source event is to the left of the camera.  If None, CamShake will be used instead. */
var(Camera) const CameraShake    CamShake_Left;
/** Anim to play when the source event is to the right of the camera.  If None, CamShake will be used instead. */
var(Camera) const CameraShake    CamShake_Right;
/** Anim to play when the source event is behind of the camera.  If None, CamShake will be used instead. */
var(Camera) const CameraShake    CamShake_Rear;

/** Radius within which to play full-powered camera shake (will be scaled within radius) const */
var(Camera) const float		                CamShakeInnerRadius;
/** Between inner and outer radii, scale shake from full to zero */
var(Camera) const float		                CamShakeOuterRadius;
/** Exponent for intensity falloff between inner and outer radii. */
var(Camera) const float		                CamShakeFalloff;

/** Play this CameraLensEffect when ever damage of this type is given.  This will primarily be used by explosions.  But could be used for other impacts too! **/
var(Camera) const class<EmitterCameraLensEffectBase>    CameraLensEffect;
/** This is the radius to play the camera effect on **/
var(Camera) const float                                 CameraLensEffectRadius;

//
// Dynamic light parameters
//

/** Defines the dynamic light cast by the explosion */
var(Light) const editinline PointLightComponent	ExploLight;
/** Dynamic Light fade out time, in seconds */
var(Light) const float					ExploLightFadeOutTime;

/** Defines the blurred region for the explosion */
var(Blur) const editinline RadialBlurComponent	ExploRadialBlur;
/** Radial blur fade out time, in seconds */
var(Blur) const float					ExploRadialBlurFadeOutTime;
/** Radial blur max blur amount */
var(Blur) const float					ExploRadialBlurMaxBlur;

//
// Particle effect parameters
//

/** Which particle effect to play. */
var(Particle) const ParticleSystem	ParticleEmitterTemplate;

//
// Fog volume parameters
//

/** The archetype that the artists set up that will be spawned with this explosion **/
var(Fog) const FogVolumeSphericalDensityInfo FogVolumeArchetype;
