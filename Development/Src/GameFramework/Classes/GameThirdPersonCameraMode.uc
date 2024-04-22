/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameThirdPersonCameraMode extends Object
	config(Camera)
	native(Camera);

/**
 * Contains all the information needed to define a camera
 * mode.
 */

/** Ref to the camera object that owns this mode object. */
var transient GameThirdPersonCamera	ThirdPersonCam;

/** FOV for camera to use */
var() const config float			FOVAngle;

/** Blend Time to and from this view mode */
var() float					        BlendTime;

/**
 * True if, while in this mode, the camera should be tied to the viewtarget rotation.
 * This is typical for the normal walking-around camera, since the controls rotate the controller
 * and the camera follows.  This can be false if you want free control of the camera, independent
 * of the viewtarget's orient -- we use this for vehicles.  Note that if this is false,
 */
var() const protected bool bLockedToViewTarget;

/**
 * True if, while in this mode, looking around should be directly mapped to stick position
 * as opposed to relative to previous camera positions.
 */
var() const protected bool bDirectLook;

/**
 * True if, while in this mode, the camera should interpolate towards a following position
 * in relation to the target and it's motion.  Ignored if bLockedToViewTarget is set to true.
 */
var() const protected bool bFollowTarget;

/*
 * How fast the camera should track to follow behind the viewtarget.  0.f for no following.
 * Only used if bLockedToViewTarget is FALSE
 */
var() const protected float		FollowingInterpSpeed_Pitch;
var() const protected float		FollowingInterpSpeed_Yaw;
var() const protected float		FollowingInterpSpeed_Roll;

/** Actual following interp speed gets scaled from FollowingInterpSpeed to zero between velocities of this value and zero. */
var() const protected float		FollowingCameraVelThreshold;


/** True means camera will attempt to smoothly interpolate to its new position.  False will snap it to it's new position. */
var() bool		bInterpLocation;
/** Controls interpolation speed of location for camera origin.  Ignored if bInterpLocation is false. */
var() protected float	    OriginLocInterpSpeed;

/** 
 *  This is a special case of origin location interpolation. If true, interpolaton will be done on each axis independently, with the specified speeds.
 *  Ignored if bInterpLocation is false. 
 */
var() protected bool		bUsePerAxisOriginLocInterp;
var() protected vector		PerAxisOriginLocInterpSpeed;

/** True means camera will attempt to smoothly interpolate to its new rotation.  False will snap it to it's new rotation. */
var() bool		bInterpRotation;
/** Controls interpolation speed of rotation for the camera origin.  Ignored if bInterpRotation is false. */
var() protected float		OriginRotInterpSpeed;
/** Whether rotation interpolation happens at constant speed or not */
var() bool		bRotInterpSpeedConstant;


/** Adjustment vector to apply to camera view offset when target is strafing to the left */
var() const protected vector	StrafeLeftAdjustment;
/** Adjustment vector to apply to camera view offset when target is strafing to the right */
var() const protected vector	StrafeRightAdjustment;
/** Velocity at (and above) which the full adjustment should be applied. */
var() const protected float		StrafeOffsetScalingThreshold;
/** Interpolation speed for interpolating to a NONZERO strafe offsets.  Higher is faster/tighter interpolation. */
var() const protected float		StrafeOffsetInterpSpeedIn;
/** Interpolation speed for interpolating to a ZERO strafe offset.  Higher is faster/tighter interpolation. */
var() const protected float		StrafeOffsetInterpSpeedOut;
/** Strafe offset last tick, used for interpolation. */
var protected transient vector LastStrafeOffset;

/** Adjustment vector to apply to camera view offset when target is moving forward */
var() const protected vector	RunFwdAdjustment;
/** Adjustment vector to apply to camera view offset when target is moving backward */
var() const protected vector	RunBackAdjustment;
/** Velocity at (and above) which the full adjustment should be applied. */
var() const protected float		RunOffsetScalingThreshold;
/** Interpolation speed for interpolating to a NONZERO offset.  Higher is faster/tighter interpolation. */
var() const protected float		RunOffsetInterpSpeedIn;
/** Interpolation speed for interpolating to a ZERO offset.  Higher is faster/tighter interpolation. */
var() const protected float		RunOffsetInterpSpeedOut;
/** Run offset last tick, used for interpolation. */
var protected transient vector LastRunOffset;

/** 
 *  An offset from the location of the viewtarget, in the viewtarget's local space.
 *  Used to calculate the "worst case" camera location, which is where the camera should retreat to
 *  if tightly obstructed. 
 */
var() protected vector	WorstLocOffset;

/** True to turn do predictive camera avoidance, false otherwise */
var() const bool		bDoPredictiveAvoidance;

/** TRUE to do a raytrace from camera base loc to worst loc, just to be sure it's cool.  False to skip it */
var() const bool		bValidateWorstLoc;

/** If TRUE, all camera collision is disabled */
var() bool		    bSkipCameraCollision;

/** Offset, in the camera target's local space, from the camera target to the camera's origin. */
var() const protected vector TargetRelativeCameraOriginOffset;

/** Supported viewport configurations. */
enum ECameraViewportTypes
{
	CVT_16to9_Full,
	CVT_16to9_VertSplit,
	CVT_16to9_HorizSplit,
	CVT_4to3_Full,
	CVT_4to3_HorizSplit,
	CVT_4to3_VertSplit,
};

struct native ViewOffsetData
{
	/** View point offset for high player view pitch */
	var() vector OffsetHigh;
	/** View point offset for medium (horizon) player view pitch */
	var() vector OffsetMid;
	/** View point offset for low player view pitch */
	var() vector OffsetLow;
};

/** contains offsets from camera target to camera loc */
var() const protected ViewOffsetData	ViewOffset;

/** TRUE to smooth the interp between high/mid/low, false to blend linearly between high <-> mid <-> low */
var() const protected bool              bSmoothViewOffsetPitchChanges;

/** viewoffset adjustment vectors for each possible viewport type, so the game looks close to the same in each */
var() const protected ViewOffsetData	ViewOffset_ViewportAdjustments[ECameraViewportTypes.EnumCount];
/** whether delta or actual view offset should be applied to the camera location */
var() bool bApplyDeltaViewOffset;


/** Optional parameters for DOF adjustments. */
var(DepthOfField) const protected bool	    bAdjustDOF;
var(DepthOfField) const protected float	    DOF_FalloffExponent;
var(DepthOfField) const protected float	    DOF_BlurKernelSize;
var(DepthOfField) const protected float	    DOF_FocusInnerRadius;
var(DepthOfField) const protected float	    DOF_MaxNearBlurAmount;
var(DepthOfField) const protected float	    DOF_MaxFarBlurAmount;
var transient protected bool	bDOFUpdated;
var protected transient float	LastDOFRadius;
var protected transient float	LastDOFDistance;
var(DepthOfField) protected const float		DOFDistanceInterpSpeed;
var(DepthOfField) protected const vector	DOFTraceExtent;

/** Maps out how the DOF inner radius changes over distance. */
var(DepthOfField) protected const float		DOF_RadiusFalloff;
var(DepthOfField) protected const vector2d	DOF_RadiusRange;
var(DepthOfField) protected const vector2d	DOF_RadiusDistRange;

/** Whether camera anims and other post processing should change the FOV */
var bool bNoFOVPostProcess;

/** If TRUE ViewOffset will only be interpolated between camera mode transitions, and then be instantaneous */
var() bool bInterpViewOffsetOnlyForCamTransition;
/** Keep track of our ViewOffset Interpolation factor */
var   float ViewOffsetInterp;

/** We optionally interpolate the results of AdjustViewOffset() to prevent pops when a cameramode changes its adjustment suddenly. */
var() protected float OffsetAdjustmentInterpSpeed;

/** Keep track of the current viewport type as computed by GetViewOffset() */
var protected transient ECameraViewportTypes CurrentViewportType;

cpptext
{
	// GearThirdPersonCameraMode interface

	/**
	 * Calculates and returns the ideal view offset for the specified camera mode.
	 * The offset is relative to the Camera's pos/rot and calculated by interpolating
	 * 2 ideal view points based on the player view pitch.
	 *
	 * @param	ViewedPawn			Camera target pawn
	 * @param	DeltaTime			Delta time since last frame.
	 * @param	ViewRotation		Rot of the camera
	 */
	virtual FVector GetViewOffset(class APawn* ViewedPawn, FLOAT DeltaTime, const FVector& ViewOrigin, const FRotator& ViewRotation);
	FLOAT GetViewOffsetInterpSpeed(class APawn* ViewedPawn, FLOAT DeltaTime);
	virtual FRotator GetViewOffsetRotBase( class APawn* ViewedPawn, const FTViewTarget& VT );

	/** Returns View relative offsets */
	virtual void GetBaseViewOffsets(class APawn* ViewedPawn, BYTE ViewportConfig, FLOAT DeltaTime, FVector& out_Low, FVector& out_Mid, FVector& out_High);

	/** Returns true if mode should be using direct-look mode, false otherwise */
	virtual UBOOL UseDirectLookMode(class APawn* CameraTarget);

	/** Returns true if mode should lock camera to view target, false otherwise */
	virtual UBOOL LockedToViewTarget(class APawn* CameraTarget);

	/**
	 * Returns true if this mode should do target following.  If true is returned, interp speeds are filled in.
	 * If false is returned, interp speeds are not altered.
	 */
	virtual UBOOL ShouldFollowTarget(class APawn* CameraTarget, FLOAT& PitchInterpSpeed, FLOAT& YawInterpSpeed, FLOAT& RollInterpSpeed);

	/** Returns an offset, in pawn-local space, to be applied to the camera origin. */
	virtual FVector GetTargetRelativeOriginOffset(class APawn* TargetPawn);

	/**
	 * Returns location and rotation, in world space, of the camera's basis point.  The camera will rotate
	 * around this point, offsets are applied from here, etc.
	 */
	virtual void GetCameraOrigin(class APawn* TargetPawn, FVector& OriginLoc, FRotator& OriginRot);

	/**
	 * Interpolates from previous location/rotation toward desired location/rotation
	 */
	virtual void InterpolateCameraOrigin( class APawn* TargetPawn, FLOAT DeltaTime, FVector& out_ActualCameraOrigin, FVector const& IdealCameraOrigin, FRotator& out_ActualCameraOriginRot, FRotator const& IdealCameraOriginRot );

	/** Returns time to interpolate location/rotation changes. */
	virtual FLOAT GetBlendTime(class APawn* Pawn);

	/** Returns time to interpolate FOV changes. */
	virtual FLOAT GetFOVBlendTime(class APawn* Pawn);

	/*
	 *   Interpolates a camera's origin from the last location to a new ideal location
	 *   @CameraTargetRot - Rotation of the camera target, used to create a reference frame for interpolation
	 *   @LastLoc - Last location of the camera
	 *   @IdealLoc - Ideal location for the camera this frame
	 *   @DeltaTime - time step
	 *   @return if bInterpLocation is false, returns IdealLoc, otherwise if bUsePerAxisOriginLocInterp is TRUE it interpolates relative to the target axis via PerAxisOriginLocInterpSpeed, else via constant OriginLocInterpSpeed
	 */
	virtual FVector InterpolateCameraOriginLoc(class APawn* TargetPawn, FRotator const& CameraTargetRot, FVector const& LastLoc, FVector const& IdealLoc, FLOAT DeltaTime);
	virtual FRotator InterpolateCameraOriginRot(class APawn* TargetPawn, FRotator const& LastRot, FRotator const& IdealRot, FLOAT DeltaTime);

	virtual FVector	ApplyViewOffset( class APawn* ViewedPawn, const FVector& CameraOrigin, const FVector& ActualViewOffset, const FVector& DeltaViewOffset, const FTViewTarget& OutVT );

protected:
	virtual FLOAT GetViewPitch(APawn* TargetPawn, FRotator const& ViewRotation) const;
public:
};

/** Called when this mode is initially created/new'd */
function Init();

/** Called when Camera mode becomes active */
function OnBecomeActive(Pawn TargetPawn, GameThirdPersonCameraMode PrevMode)
{
	// Setup BlendTime for ViewOffsets
	if( BlendTime > 0.f )
	{
		ViewOffsetInterp = 1 / BlendTime;
	}
	else
	{
		ViewOffsetInterp = 0;
	}
}

/** Called when camera mode becomes inactive */
function OnBecomeInActive(Pawn TargetPawn, GameThirdPersonCameraMode NewMode);

/** 
 * Allows mode to make any final situational adjustments to the base view offset.
 */
event vector AdjustViewOffset( Pawn P, vector Offset )
{
	// no adjustment by default
	return Offset;
}

/** Returns FOV that this camera mode desires. */
function float GetDesiredFOV( Pawn ViewedPawn )
{
	return FOVAngle;
}


/**
 * Returns the "worst case" camera location for this camera mode.
 * This is the position that the camera penetration avoidance is based off of,
 * so it should be a guaranteed safe place to put the camera.
 * @param TargetPawn Pawn being viewed.
 * @param CurrentViewTarget current desired camera POV. Minus penetration checks.
 */
simulated event vector GetCameraWorstCaseLoc(Pawn TargetPawn, TViewTarget CurrentViewTarget)
{
	return TargetPawn.Location + (WorstLocOffset >> TargetPawn.Rotation);
}

/**
 * Camera mode has a chance to set a focus point, if it so chooses.
 * Return true if setting one, false if not.
 */
simulated function bool SetFocusPoint(Pawn ViewedPawn)
{
	// no focus point by default
	return FALSE;
}

simulated function ProcessViewRotation( float DeltaTime, Actor ViewTarget, out Rotator out_ViewRotation, out Rotator out_DeltaRot );


simulated protected function vector GetDOFFocusLoc(Actor TraceOwner, vector StartTrace, vector EndTrace)
{
	return DOFTrace(TraceOwner, StartTrace, EndTrace);
}

/** Modeled after CalcWeaponFire to avoid triggers.  Consider moving to native code and using a single line check call? */
simulated protected function vector DOFTrace(Actor TraceOwner, vector StartTrace, vector EndTrace)
{
	local vector	HitLocation, HitNormal;
	local Actor		HitActor;

	// Perform trace to retrieve hit info
	HitActor = TraceOwner.Trace(HitLocation, HitNormal, EndTrace, StartTrace, TRUE, DOFTraceExtent,, TraceOwner.TRACEFLAG_Bullet);

	// If we didn't hit anything, then set the HitLocation as being the EndTrace location
	if( HitActor == None )
	{
		HitLocation	= EndTrace;
	}

//	`log("DOF trace hit"@HitActor);

	// check to see if we've hit a trigger.
	// In this case, we want to add this actor to the list so we can give it damage, and then continue tracing through.
	if( HitActor != None )
	{
		if ( !HitActor.bBlockActors && 
			 ( HitActor.IsA('Trigger') || HitActor.IsA('TriggerVolume') ) )
		{	
			// disable collision temporarily for the trigger so that we can catch anything inside the trigger
			HitActor.bProjTarget = false;
			// recurse another trace
//			`log("... recursing!");
			HitLocation = DOFTrace(TraceOwner, HitLocation, EndTrace);
			// and reenable collision for the trigger
			HitActor.bProjTarget = true;
		}
	}

	return HitLocation;
}


/** Gives mode a chance to adjust/override postprocess as desired. */
simulated function UpdatePostProcess(const out TViewTarget VT, float DeltaTime)
{
	local vector FocusLoc, StartTrace, EndTrace, CamDir;
	local float FocusDist, SubjectDist, Pct;

	bDOFUpdated = FALSE;

	if (bAdjustDOF)
	{
		// nudge forward a little, in case camera gets itself in a wierd place.
		CamDir = vector(VT.POV.Rotation);
		StartTrace = VT.POV.Location + CamDir * 10;
		EndTrace = StartTrace + CamDir * 50000;
		FocusLoc = GetDOFFocusLoc(VT.Target, StartTrace, EndTrace);
		SubjectDist = VSize(FocusLoc - StartTrace);

		if (!ThirdPersonCam.bResetCameraInterpolation)
		{
			FocusDist = FInterpTo(LastDOFDistance, SubjectDist, DeltaTime, DOFDistanceInterpSpeed);
		}
		else
		{
			FocusDist = SubjectDist;
		}
		LastDOFDistance = FocusDist;

		// find focus radius
		// simpler method, with better-feeling results than actual optics math
		Pct = GetRangePctByValue(DOF_RadiusDistRange, FocusDist);
		LastDOFRadius = GetRangeValueByPct(DOF_RadiusRange, FClamp(Pct, 0.f, 1.f)**DOF_RadiusFalloff);

		bDOFUpdated = TRUE;
	}
}

simulated function ModifyPostProcessSettings(out PostProcessSettings PP)
{
	if (bDOFUpdated)
	{
		PP.bEnableDOF = TRUE;
		PP.DOF_FalloffExponent = DOF_FalloffExponent;
		PP.DOF_BlurKernelSize = DOF_BlurKernelSize;
		PP.DOF_MaxNearBlurAmount = DOF_MaxNearBlurAmount;
		PP.DOF_MaxFarBlurAmount = DOF_MaxFarBlurAmount;
		PP.DOF_FocusType = FOCUS_Distance;
		PP.DOF_FocusInnerRadius = DOF_FocusInnerRadius;

		PP.DOF_FocusDistance = LastDOFDistance;
		PP.DOF_FocusInnerRadius = LastDOFRadius;

		bDOFUpdated = FALSE;
	}
}

native final function SetViewOffset(const out ViewOffsetData NewViewOffset);

defaultproperties
{
	BlendTime=0.67

	bLockedToViewTarget=TRUE
	bDoPredictiveAvoidance=TRUE
	bValidateWorstLoc=TRUE

	bInterpLocation=TRUE
	OriginLocInterpSpeed=8

	StrafeLeftAdjustment=(X=0,Y=0,Z=0)
	StrafeRightAdjustment=(X=0,Y=0,Z=0)
	StrafeOffsetInterpSpeedIn=12.f
	StrafeOffsetInterpSpeedOut=20.f
	RunFwdAdjustment=(X=0,Y=0,Z=0)
	RunBackAdjustment=(X=0,Y=0,Z=0)
	RunOffsetInterpSpeedIn=6.f
	RunOffsetInterpSpeedOut=12.f

	WorstLocOffset=(X=-8,Y=1,Z=90)

	bInterpViewOffsetOnlyForCamTransition=TRUE
	/** all offsets are hand-crafted for this mode, so they should theoretically be all zeroes for all modes */
	ViewOffset_ViewportAdjustments(CVT_16to9_Full)={(
		OffsetHigh=(X=0,Y=0,Z=0),
		OffsetLow=(X=0,Y=0,Z=0),
		OffsetMid=(X=0,Y=0,Z=0),
		)}

	bAdjustDOF=FALSE
	DOF_FalloffExponent=1.f
	DOF_BlurKernelSize=3.f
//	DOF_FocusInnerRadius=2500.f
	DOF_MaxNearBlurAmount=0.6f
	DOF_MaxFarBlurAmount=1.f
	DOFDistanceInterpSpeed=10.f
	DOFTraceExtent=(X=0.f,Y=0.f,Z=0.f)

	DOF_RadiusFalloff=1.f
	DOF_RadiusRange=(X=2500,Y=60000)
	DOF_RadiusDistRange=(X=1000,Y=50000)

	OffsetAdjustmentInterpSpeed=12.f
}





