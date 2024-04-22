/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * 
 * Camera modifier that provides support for code-based oscillating camera shakes.
 */
class CameraModifier_CameraShake extends CameraModifier
	native(Camera)
	dependson(CameraShake)
	config(Camera);


struct native CameraShakeInstance
{
	/** source shake */
	var CameraShake    SourceShake;
	
	/** Used to identify shakes when single instances are desired */
	var name SourceShakeName;

	/** <0.f means play infinitely. */
	var float   OscillatorTimeRemaining;

	/** blend vars */
	var bool bBlendingIn;
	var float CurrentBlendInTime;
	var bool bBlendingOut;
	var float CurrentBlendOutTime;

	/** Current offsets. */
	var	vector	LocSinOffset;
	var	vector	RotSinOffset;
	var	float	FOVSinOffset;

	var float   Scale;

	var CameraAnimInst AnimInst;

	/** What space to play the shake in before applying to the camera.  Affects Anim and Oscillation both. */
	var ECameraAnimPlaySpace PlaySpace;
	/** Matrix defining the playspace, used when PlaySpace == CAPS_UserDefined */
	var matrix UserPlaySpaceMatrix;
};



/** Active CameraShakes array */
var		Array<CameraShakeInstance>	ActiveShakes;

/** Always active ScreenShake for testing purposes */
//var()	CameraShake			TestShake;

/** Scalar applied to all camera shakes in splitscreen. Normally used to dampen, since shakes feel more intense in a smaller viewport. */
var() protected const float SplitScreenShakeScale;

cpptext
{
protected:
	/** For situational scaling of individual shakes. */
	virtual FLOAT GetShakeScale(FCameraShakeInstance const& ShakeInst) const;
public:
};


static protected function float InitializeOffset( const out FOscillator Param )
{
	Switch( Param.InitialOffset )
	{
		case EOO_OffsetRandom	: return FRand() * 2 * Pi;	break;
		case EOO_OffsetZero		: return 0;					break;
	}

	return 0;
}


function protected ReinitShake(int ActiveShakeIdx, float Scale)
{
	local CameraShake SourceShake;
	local float		Duration;
	local bool		bRandomStart, bLoop;

	if (class'Engine'.static.IsSplitScreen())
	{
		Scale *= SplitScreenShakeScale;
	}
	ActiveShakes[ActiveShakeIdx].Scale = Scale;

	SourceShake = ActiveShakes[ActiveShakeIdx].SourceShake;

	if (SourceShake.OscillationDuration != 0.f)
	{
		ActiveShakes[ActiveShakeIdx].OscillatorTimeRemaining = SourceShake.OscillationDuration;

		if (ActiveShakes[ActiveShakeIdx].bBlendingOut)
		{
			ActiveShakes[ActiveShakeIdx].bBlendingOut = FALSE;
			ActiveShakes[ActiveShakeIdx].CurrentBlendOutTime = 0.f;

			// stop any blendout and reverse it to a blendin
			ActiveShakes[ActiveShakeIdx].bBlendingIn = TRUE;
			ActiveShakes[ActiveShakeIdx].CurrentBlendInTime = ActiveShakes[ActiveShakeIdx].SourceShake.OscillationBlendInTime * (1.f - ActiveShakes[ActiveShakeIdx].CurrentBlendOutTime / ActiveShakes[ActiveShakeIdx].SourceShake.OscillationBlendOutTime);
		}
	}

	if (SourceShake.Anim != None)
	{
		if (SourceShake.bRandomAnimSegment)
		{
			bLoop = TRUE;
			bRandomStart = TRUE;
			Duration = SourceShake.RandomAnimSegmentDuration;
		}

		ActiveShakes[ActiveShakeIdx].AnimInst = CameraOwner.PlayCameraAnim(SourceShake.Anim,
																			SourceShake.AnimPlayRate, 
																			Scale, 
																			SourceShake.AnimBlendInTime,
																			SourceShake.AnimBlendOutTime,
																			bLoop,
																			bRandomStart,
																			Duration,
																			TRUE);
	}

}

/** Initialize camera shake structure */
function protected CameraShakeInstance InitializeShake(CameraShake NewShake, float Scale, ECameraAnimPlaySpace PlaySpace, optional rotator UserPlaySpaceRot)
{
	local CameraShakeInstance Inst;
	local float		Duration;
	local bool		bRandomStart, bLoop;

	Inst.SourceShakeName = NewShake.Name;
	
	// Create a copy of NewShake.
	// Keeping the reference to NewShake can sometimes cause problems during GC, if NewShake lives in the level.
	Inst.SourceShake = new(self) NewShake.Class();
	Inst.SourceShake.OscillationDuration = NewShake.OscillationDuration;
	Inst.SourceShake.OscillationBlendInTime = NewShake.OscillationBlendInTime;
	Inst.SourceShake.OscillationBlendOutTime = NewShake.OscillationBlendOutTime;
	Inst.SourceShake.RotOscillation = NewShake.RotOscillation;
	Inst.SourceShake.LocOscillation = NewShake.LocOscillation;
	Inst.SourceShake.FOVOscillation = NewShake.FOVOscillation;
	Inst.SourceShake.Anim = NewShake.Anim;
	Inst.SourceShake.AnimPlayRate = NewShake.AnimPlayRate;
	Inst.SourceShake.AnimBlendInTime = NewShake.AnimBlendInTime;
	Inst.SourceShake.AnimBlendOutTime = NewShake.AnimBlendOutTime;
	Inst.SourceShake.bRandomAnimSegment = NewShake.bRandomAnimSegment;
	Inst.SourceShake.RandomAnimSegmentDuration = NewShake.RandomAnimSegmentDuration;

	Inst.Scale = Scale;
	if (class'Engine'.static.IsSplitScreen())
	{
		Scale *= SplitScreenShakeScale;
	}

	// init oscillations
	if ( NewShake.OscillationDuration != 0.f )
	{
		Inst.RotSinOffset.X		= InitializeOffset( NewShake.RotOscillation.Pitch );
		Inst.RotSinOffset.Y		= InitializeOffset( NewShake.RotOscillation.Yaw );
		Inst.RotSinOffset.Z		= InitializeOffset( NewShake.RotOscillation.Roll );
		
		Inst.LocSinOffset.X		= InitializeOffset( NewShake.LocOscillation.X );
		Inst.LocSinOffset.Y		= InitializeOffset( NewShake.LocOscillation.Y );
		Inst.LocSinOffset.Z		= InitializeOffset( NewShake.LocOscillation.Z );
		
		Inst.FOVSinOffset		= InitializeOffset( NewShake.FOVOscillation );
		
		Inst.OscillatorTimeRemaining = NewShake.OscillationDuration;

		if (NewShake.OscillationBlendInTime > 0.f)
		{
			Inst.bBlendingIn = TRUE;
			Inst.CurrentBlendInTime = 0.f;
		}
	}

	// init anims
	if (NewShake.Anim != None)
	{
		if (NewShake.bRandomAnimSegment)
		{
			bLoop = TRUE;
			bRandomStart = TRUE;
			Duration = NewShake.RandomAnimSegmentDuration;
		}

		if (Scale > 0.f)
		{
			Inst.AnimInst = CameraOwner.PlayCameraAnim(NewShake.Anim, NewShake.AnimPlayRate, Scale, NewShake.AnimBlendInTime, NewShake.AnimBlendOutTime, bLoop, bRandomStart, Duration, NewShake.bSingleInstance);
			if (PlaySpace != CAPS_CameraLocal && Inst.AnimInst != None)
			{
				Inst.AnimInst.SetPlaySpace(PlaySpace, UserPlaySpaceRot);
			}

		}
	}

	Inst.PlaySpace = PlaySpace;
	if (Inst.PlaySpace == CAPS_UserDefined)
	{
		Inst.UserPlaySpaceMatrix = MakeRotationMatrix(UserPlaySpaceRot);
	}

	return Inst;
}

/** Add a new screen shake to the list */
function AddCameraShake( CameraShake NewShake, float Scale, optional ECameraAnimPlaySpace PlaySpace=CAPS_CameraLocal, optional rotator UserPlaySpaceRot )
{
	local int ShakeIdx, NumShakes;

	if (NewShake != None)
	{
		if (NewShake.bSingleInstance)
		{
			ShakeIdx = ActiveShakes.Find('SourceShakeName', NewShake.Name);
			if (ShakeIdx != INDEX_NONE)
			{
				ReinitShake(ShakeIdx, Scale);
				return;
			}
		}

		NumShakes = ActiveShakes.Length;

		// Initialize new shake and add it to the list of active shakes
		ActiveShakes[NumShakes] = InitializeShake(NewShake, Scale, PlaySpace, UserPlaySpaceRot);
	}
}


function RemoveCameraShake(CameraShake Shake)
{
	local int Idx;
	local CameraAnimInst AnimInst;

	Idx = ActiveShakes.Find('SourceShakeName', Shake.Name);

	if (Idx != INDEX_NONE)
	{
		AnimInst = ActiveShakes[Idx].AnimInst;
		if ( (AnimInst != None) && !AnimInst.bFinished )
		{
			CameraOwner.StopCameraAnim(AnimInst, true);
		}

		ActiveShakes.Remove(Idx,1);
	}
}

function RemoveAllCameraShakes()
{
	local int Idx;
	local CameraAnimInst AnimInst;

	// clean up any active camera shake anims
	for (Idx=0; Idx<ActiveShakes.length; ++Idx)
	{
		AnimInst = ActiveShakes[Idx].AnimInst;
		if ( (AnimInst != None) && !AnimInst.bFinished )
		{
			CameraOwner.StopCameraAnim(AnimInst, true);
		}
	}

	// clear ActiveShakes array
	ActiveShakes.Length = 0;
}

/** Update a CameraShake */
native function UpdateCameraShake(float DeltaTime, out CameraShakeInstance Shake, out TPOV OutPOV);

/** @see CameraModifer::ModifyCamera */
native function bool ModifyCamera
(
		Camera	Camera,
		float	DeltaTime,
	out TPOV	OutPOV
);

defaultproperties
{
	SplitScreenShakeScale=0.5f
}




