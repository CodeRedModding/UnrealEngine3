/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameExplosion extends Object
	native
	editinlinenew;

//
// Gameplay parameters
//

/**
 *  If TRUE, this will be a "directional" explosion, meaning that all radial effects will be applied only
 *  within DirectionalExplosionAngleDeg degrees of the blast's facing direction (which should be supplied via Explode()).
 */
var() bool  bDirectionalExplosion;
/** Half-angle, in degrees, of the cone that defines the effective area of a directional explosion. */
var() float DirectionalExplosionAngleDeg;

/** Delay before applying damage after spawning FX, 0.f == no delay */
var() float DamageDelay;
/** Amount of damage done at the epicenter. */
var() float Damage;
/** Damage range. */
var() float DamageRadius;
/** Defines how damage falls off.  High numbers cause faster falloff, lower (closer to zero) cause slower falloff.  1 is linear. */
var() float DamageFalloffExponent;
/** Optional actor that does not receive any radial damage, to be specified at runtime */
var transient Actor ActorToIgnoreForDamage;
/** If set, ignore instigator when doing damage/effects. Can be set in addition to above */
var transient bool bIgnoreInstigator;

/** The actor class to ignore for damage from this explosion **/
var() class<Actor> ActorClassToIgnoreForDamage;

/** The actor class to ignore for knockdowns and cringes from this explosion **/
var() class<Actor> ActorClassToIgnoreForKnockdownsAndCringes;

/** True to allow teammates to cringe, regardless of friendly fire setting. */
var() bool bAllowTeammateCringes;

/** Unused? Option to force full damage to the attachee actor. */
var transient bool	bFullDamageToAttachee;

/** What damagetype to use */
var() class<DamageType> MyDamageType<AllowAbstract>;

/** radius at which people will be knocked down/ragdolled by the projectile's explosion **/
var() float	KnockDownRadius;
/** @fixme, base this on MomentumTransferScale? */
var() float	KnockDownStrength;


/** radius at which people will cringe from the explosion **/
var() float	CringeRadius;
/** duration of the cringe.  X=duration at epicenter, Y=duration at CringeRadius. Values <0 mean use default cringe. */
var() vector2d CringeDuration;

/** Percentage of damagetype's momentum to apply. */
var() float MomentumTransferScale;

/** Whether or not we should attach something to the attachee **/
var() bool bAttachExplosionEmitterToAttachee;


//
// Particle effect parameters
//

/** Which particle effect to play. */
var() ParticleSystem	ParticleEmitterTemplate;
/** Scalar for increasing/decreasing explosion effect size. */
var() float				ExplosionEmitterScale;


/** Track if we've hit an actor, used to handle cases such as kidnapper protected from hostage damage */
var Actor HitActor;

/** We need the hit location and hit normal so we can trace down to the actor to apply the decal  (e.g. hitting wall or floor) **/
var vector HitLocation;
var vector HitNormal;

//
// Audio parameters
//

/** Audio to play at explosion time. */
var() SoundCue	ExplosionSound;
/** Audio to play at explosion time if at least one pawn got hurt. Does not work for delayed damage. As we typically don't want to delay the explosion sound. */
var() SoundCue  ExplosionSoundHurtSomeone;

//
// Dynamic light parameters
//

/** Defines the dynamic light cast by the explosion */
var() PointLightComponent	ExploLight;
/** Dynamic Light fade out time, in seconds */
var() float					ExploLightFadeOutTime;

/** If true, will perform an EffectIsRelevant check before spawning the radial blur */
var() bool					bPerformRadialBlurRelevanceCheck;
/** Defines the blurred region for the explosion */
var() RadialBlurComponent	ExploRadialBlur;
/** Radial blur fade out time, in seconds */
var() float					ExploRadialBlurFadeOutTime;
/** Radial blur max blur amount */
var() float					ExploRadialBlurMaxBlur;

//
// Fractured mesh parameters
//

/** Controls if this explosion will cause fracturing */
var() bool					bCausesFracture;
/** How far away from explosion we break bits off */
var() float					FractureMeshRadius;
/** How hard to throw broken off pieces */
var() float					FracturePartVel;

/** If true, attempt to get effect information from the physical material system.  If false or a physicalmaterial is unavailable, just use the information above. */
var() bool bAllowPerMaterialFX;

/** So for tagged grenades we need override the particle system but still want material based decals and such. **/
var() bool bParticleSystemIsBeingOverriddenDontUsePhysMatVersion;

/** If true, the PhysMat's default particle system will not override the one already set in the explosion **/
var() bool bSkipDefaultPhysMatParticleSystem;

/** This tells the explosion to look in the Map's MapSpecific info **/
var() bool bUseMapSpecificValues;

var() bool bUseOverlapCheck;

//
// Camera parameters
//

/** TRUE to rotate CamShake to play radially relative to the explosion.  Left/Right/Rear will be ignored. */
var() bool						bOrientCameraShakeTowardsEpicenter;
/** Shake to play when source is in front of the camera, or when directional variants are unspecified. */
var() editinline CameraShake    CamShake;
/** Anim to play when the source event is to the left of the camera.  If None, CamShake will be used instead. */
var() editinline CameraShake    CamShake_Left;
/** Anim to play when the source event is to the right of the camera.  If None, CamShake will be used instead. */
var() editinline CameraShake    CamShake_Right;
/** Anim to play when the source event is behind of the camera.  If None, CamShake will be used instead. */
var() editinline CameraShake    CamShake_Rear;

/** Radius within which to play full-powered camera shake (will be scaled within radius) */
var() float		                CamShakeInnerRadius;
/** Between inner and outer radii, scale shake from full to zero */
var() float		                CamShakeOuterRadius;
/** Exponent for intensity falloff between inner and outer radii. */
var() float		                CamShakeFalloff;

/** TRUE to attempt to automatically do force feedback to match the camera shake */
var() bool                      bAutoControllerVibration;


/** Play this CameraLensEffect when ever damage of this type is given.  This will primarily be used by explosions.  But could be used for other impacts too! **/
var() class<EmitterCameraLensEffectBase>    CameraLensEffect;
/** This is the radius to play the camera effect on **/
var() float                                 CameraLensEffectRadius;


defaultproperties
{
	ExplosionEmitterScale=1.f
	MomentumTransferScale=1.f
	bCausesFracture=TRUE
	bPerformRadialBlurRelevanceCheck=false
	ExploRadialBlurMaxBlur=2.0
	CringeDuration=(X=-1.f,Y=-1.f)

	CamShakeFalloff=2.f
	bAutoControllerVibration=true
}
