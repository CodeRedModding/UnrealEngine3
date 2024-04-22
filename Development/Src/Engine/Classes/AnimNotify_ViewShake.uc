/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AnimNotify_ViewShake extends AnimNotify_Scripted
	native(Anim);

/** 
 *  Note: these shake-defining params are deprecated.  Use ShakeParams.
 *  Leaving them here for now for compatibility with existing content (which will be 
 *  upgraded via PostLoad()).
 */
/** Duration in seconds of shake */
var	private editconst float	Duration;
/** view rotation amplitude (pitch,yaw,roll) */
var	private editconst vector	RotAmplitude;
/** frequency of rotation shake */
var	private editconst vector	RotFrequency;
/** relative view offset amplitude (x,y,z) */
var	private editconst vector	LocAmplitude;
/** frequency of view offset shake */
var	private editconst vector	LocFrequency;
/** fov shake amplitude */
var	private editconst float	FOVAmplitude;
/** fov shake frequency */
var	private editconst float	FOVFrequency;

var() bool bDoControllerVibration;

/** Radius within which to shake player views. If 0 only plays on the animated player */
var() float	ShakeRadius;

/** Should use a bone location as the shake's epicenter? */
var() bool	bUseBoneLocation;
/** if so, bone name to use */
var() name	BoneName;

var() export editinline CameraShake ShakeParams;

cpptext
{
	virtual void PostLoad();
	virtual FString GetEditorComment() { return TEXT("CameraShake"); }
};

/**
 * Trigger the view shake
 * 
 * @param Owner - the actor that is playing this animation
 * 
 * @param AnimSeqInstigator - the anim sequence that triggered the notify
 */
event Notify(Actor Owner, AnimNodeSequence AnimSeqInstigator)
{
	local vector ViewShakeOrigin;
	local Pawn P;
	local PlayerController PC;

	if (ShakeRadius == 0)
	{
		P = Pawn(Owner);
		if (P != None && P.IsLocallyControlled())
		{
			PC = PlayerController(P.Controller);
			if (PC != None)
			{
				PC.ClientPlayCameraShake(ShakeParams);
			}
		}
	}
	else
	{
		// Figure out world origin of view shake
		if( bUseBoneLocation &&
			AnimSeqInstigator != None &&
			AnimSeqInstigator.SkelComponent != None )
		{
			ViewShakeOrigin = AnimSeqInstigator.SkelComponent.GetBoneLocation( BoneName );
		}
		else
		{
			ViewShakeOrigin = Owner.Location;
		}

		// propagate to all player controllers
		if (Owner != None)
		{
			class'Camera'.static.PlayWorldCameraShake(ShakeParams, Owner, ViewShakeOrigin, 0.f, ShakeRadius, 1.f, bDoControllerVibration);
		}
	}
}

defaultproperties
{
	ShakeRadius=4096.0
	Duration=1.f
	RotAmplitude=(X=100,Y=100,Z=200)
	RotFrequency=(X=10,Y=10,Z=25)
	LocAmplitude=(X=0,Y=3,Z=6)
	LocFrequency=(X=1,Y=10,Z=20)
	FOVAmplitude=2
	FOVFrequency=5
}
