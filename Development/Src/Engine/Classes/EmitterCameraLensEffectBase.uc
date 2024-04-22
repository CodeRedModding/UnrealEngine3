/**
 * Base class for Camera Lens Effects.  Needed so we can have AnimNotifies be able to show camera effects
 * in a nice drop down
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class EmitterCameraLensEffectBase extends Emitter
	abstract
	dependson(Emitter)
	native(Particle);

/** Particle System to use */
var protected ParticleSystem PS_CameraEffect;

/** The effect to use for non extreme content */
var protected ParticleSystem PS_CameraEffectNonExtremeContent;

/** In order to get the particle effect looking correct we need to have a base FOV which we just to move the particle closer/further from the camera **/
var float BaseFOV;


/** 
 *  How far in front of the camera this emitter should live, assuming an FOV of 80 degrees. 
 *  Note that the actual distance will be automatically adjusted to account for the actual FOV.
 */
var() protected const float				    DistFromCamera;

/** TRUE if multiple instances of this emitter can exist simultaneously, FALSE otherwise.  */
var() protectedwrite const bool				bAllowMultipleInstances;

/** 
 *  If an emitter class in this array is currently playing, do not play this effect.
 *  Useful for preventing multiple similar or expensive camera effects from playing simultaneously.
 */
var array<class<EmitterCameraLensEffectBase> > EmittersToTreatAsSame;

/** Camera this emitter is attached to, will be notified when emitter is destroyed */
var protected transient Camera	            BaseCamera;

function Destroyed()
{
	if (BaseCamera != None)
	{
		BaseCamera.RemoveCameraLensEffect(self);
	}
	super.Destroyed();
}

/** Tell the emitter what camera it is attached to. */
function RegisterCamera(Camera C)
{
	BaseCamera = C;
}

/** Called when this emitter is re-triggered, for bAllowMultipleInstances=FALSE emitters. */
function NotifyRetriggered();





 simulated function PostBeginPlay()
{
	ParticleSystemComponent.SetDepthPriorityGroup(SDPG_Foreground);

	Super.PostBeginPlay();

	ActivateLensEffect();
}


/** This will actually activate the lens Effect.  We want this separated from PostBeginPlay so we can cache these emitters **/
simulated function ActivateLensEffect()
{
	local ParticleSystem PSToActuallySpawn;

	// only play the camera effect on clients
	if( WorldInfo.NetMode != NM_DedicatedServer )
	{
		if( WorldInfo.GRI.ShouldShowGore() )
		{
			PSToActuallySpawn = PS_CameraEffect;
		}
		else
		{
			PSToActuallySpawn = PS_CameraEffectNonExtremeContent;
		}

		if( PSToActuallySpawn != None )
		{
			SetTemplate( PS_CameraEffect, bDestroyOnSystemFinish );
		}
	}
}

/** Given updated camera information, adjust this effect to display appropriately. */
simulated native function UpdateLocation(const out vector CamLoc, const out rotator CamRot, float CamFOVDeg);


DefaultProperties
{
	DistFromCamera=90
	LifeSpan=10.0f
	BaseFOV=80.0f

	// makes sure I tick after the camera
	TickGroup=TG_PostAsyncWork

	bDestroyOnSystemFinish=TRUE
	bNetInitialRotation=TRUE
	bNoDelete=FALSE

	Begin Object Name=ParticleSystemComponent0
		bOnlyOwnerSee=TRUE
		SecondsBeforeInactive=0.0
	End Object

	Components.Remove(ArrowComponent0)
	Components.Remove(Sprite)
}
