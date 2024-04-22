/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameThirdPersonCamera extends GameCameraBase
	config(Camera)
	native(Camera);


/** obstruction pct from origin to worstloc origin */
var		float	WorstLocBlockedPct;
/** camera extent scale to use when calculating penetration for this segment */
var()	float	WorstLocPenetrationExtentScale;

/** Time to transition from blocked location to ideal position, after camera collision with geometry. */
var()	float	PenetrationBlendOutTime;
/** Time to transition from ideal location to blocked position, after camera collision with geometry. (used only by predictive feelers) */
var()	float	PenetrationBlendInTime;
/** Percentage of distance blocked by collision. From worst location, to desired location. */
var protected float	PenetrationBlockedPct;
/** camera extent scale to use when calculating penetration for this segment */
var()	float	PenetrationExtentScale;


/**
 * Last pawn relative offset, for slow offsets interpolation.
 * This is because this offset is relative to the Pawn's rotation, which can change abruptly (when snapping to cover).
 * Used to adjust the camera origin (evade, lean, pop up, blind fire, reload..)
 */
var		transient	vector	LastActualOriginOffset;

/** Last actual camera origin position, for lazy cam interpolation. It's only applied to player's origin, not view offsets, for faster/smoother response */
var	transient vector	LastActualCameraOrigin;
/** Last actual camera origin rotation, for lazy cam interpolation. It's only applied to player's origin, not view offsets, for faster/smoother response */
var	transient rotator	LastActualCameraOriginRot;
/** origin offset interpolation speed */
var()				float	OriginOffsetInterpSpeed;

/** View relative offset. This offset is relative to Controller's rotation, mainly used for Pitch positioning. */
var		transient	vector	LastViewOffset;
/** last CamFOV for war cam interpolation */
var		transient	float	LastCamFOV;

/** Cached ideal pivot loc */
var     transient   vector  LastIdealCameraOrigin;
/** Cached ideal pivot rot */
var     transient   rotator LastIdealCameraOriginRot;


/*********** CAMERA VARIABLES ***********/
/******* CAMERA MODES *******/
/** Base camera position when walking */
var() protected editinline	GameThirdPersonCameraMode   	ThirdPersonCamDefault;
var() protected class<GameThirdPersonCameraMode>            ThirdPersonCamDefaultClass;

// 
//
// Player 'GearCam' camera mode system
//

/** Current GearCam Mode */
var() editinline transient GameThirdPersonCameraMode	CurrentCamMode;

//
// Focus Point adjustment
//

/** last offset adjustment, for smooth blend out */
var transient	float	LastHeightAdjustment;
/** last adjusted pitch, for smooth blend out */
var transient	float	LastPitchAdjustment;
/** last adjusted Yaw, for smooth blend out */
var transient	float	LastYawAdjustment;
/** pitch adjustment when keeping target is done in 2 parts.  this is the amount to pitch in part 2 (post view offset application) */
var transient	float	LeftoverPitchAdjustment;

/**  move back pct based on move up */
var(Focus)			float		Focus_BackOffStrength;
/** Z offset step for every try */
var(Focus)			float		Focus_StepHeightAdjustment;
/** number of tries to have focus in view */
var(Focus)			int			Focus_MaxTries;
/** time it takes for fast interpolation speed to kick in */
var(Focus)			float		Focus_FastAdjustKickInTime;
/** Last time focus point changed (location) */
var transient protected float		LastFocusChangeTime;
var transient protected vector		ActualFocusPointWorldLoc;
/** Last focus point location */
var	transient protected vector		LastFocusPointLoc;

/** Camera focus point definition */
struct native CamFocusPointParams
{
	/** Actor to focus on. */
	var()	Actor		FocusActor;
	/** Bone name to focus on.  Ignored if FocusActor is None or has no SkeletalMeshComponent */
	var()	Name		FocusBoneName;
	/** Focus point location in world space.  Ignored if FocusActor is not None. */
	var()	vector		FocusWorldLoc;

	/** If >0, FOV to force upon camera while looking at this point (degrees) */
	var()	float		CameraFOV;
	/** Interpolation speed (X=slow/focus loc moving, Y=fast/focus loc steady/blending out) */
	var()	vector2d	InterpSpeedRange;
	/** FOV where target is considered in focus, no correction is made.  X is yaw tolerance, Y is pitch tolerance. */
	var()	vector2d	InFocusFOV;
	/** If FALSE, focus only if point roughly in view; if TRUE, focus no matter where player is looking */
	var()	bool		bAlwaysFocus;
	/** If TRUE, camera adjusts to keep player in view, if FALSE the camera remains fixed and just rotates in place */
	var()	bool		bAdjustCamera;
	/** If TRUE, ignore world trace to find a good spot */
	var()	bool		bIgnoreTrace;

	/** Offsets the pitch.  e.g. 20 will look 20 degrees above the target */
	var()	float		FocusPitchOffsetDeg;
};
/** current focus point */
var(Focus)	CamFocusPointParams	FocusPoint;
/** do we have a focus point set? */
var		bool					bFocusPointSet;
/** Internal.  TRUE if the focus point was good and the camera looked at it, FALSE otherwise (e.g. failed the trace). */
var protected transient bool	bFocusPointSuccessful;

/** Vars for code-driven camera turns */
var	protected float			TurnCurTime;
var	           bool			bDoingACameraTurn;
var	protected int			TurnStartAngle;
var	protected int			TurnEndAngle;
var	protected float			TurnTotalTime;
var	protected float			TurnDelay;
var	protected bool			bTurnAlignTargetWhenFinished;
/** Saved data for camera turn "align when finished" functionality */
var protected transient int	LastPostCamTurnYaw;

/** toggles debug mode */
var() bool		        bDrawDebug;

/** direct look vars */
var	transient int		DirectLookYaw;
var	transient bool	    bDoingDirectLook;
var() float             DirectLookInterpSpeed;

var() float					WorstLocInterpSpeed;
var transient vector		LastWorstLocationLocal;
var transient vector		LastWorstLocation;

/** Last location and rotation of the camera, cached before camera modifiers are applied. */
var transient vector		LastPreModifierCameraLoc;
var transient rotator		LastPreModifierCameraRot;


/**
 * Struct defining a feeler ray used for camera penetration avoidance.
 */
struct native PenetrationAvoidanceFeeler
{
	/** rotator describing deviance from main ray */
	var() Rotator	AdjustmentRot;
	/** how much this feeler affects the final position if it hits the world */
	var() float		WorldWeight;
	/** how much this feeler affects the final position if it hits a Pawn (setting to 0 will not attempt to collide with pawns at all) */
	var() float		PawnWeight;
	/** extent to use for collision when firing this ray */
	var() vector	Extent;
	/** minimum frame interval between traces with this feeler if nothing was hit last frame */
	var() int TraceInterval;
	/** number of frames since this feeler was used */
	var transient int FramesUntilNextTrace;
};
var() array<PenetrationAvoidanceFeeler> PenetrationAvoidanceFeelers;

/** Offset adjustment from last tick, used for interpolation. */
var protectedwrite transient vector LastOffsetAdjustment;

/** Change in camera mode happened this frame - reset on first call to PlayerUpdateCamera */
var(Debug) bool bDebugChangedCameraMode;

/** Set to TRUE when the Pivot makes a big jump if you want to keep the camera in place across that transition. */
var transient bool bDoSeamlessPivotTransition;

cpptext
{
protected:
	/**
	 * Interpolates from previous location/rotation toward desired location/rotation
	 */
	virtual void InterpolateCameraOrigin( class APawn* TargetPawn, FLOAT DeltaTime, FVector& out_ActualCameraOrigin, FVector const& IdealCameraOrigin, FRotator& out_ActualCameraOriginRot, FRotator const& IdealCameraOriginRot );

	/** Returns the focus location, adjusted to compensate for the third-person camera offset. */
	FVector GetEffectiveFocusLoc(const FVector& CamLoc, const FVector& FocusLoc, const FVector& ViewOffset);
	void AdjustToFocusPointKeepingTargetInView(class APawn* P, FLOAT DeltaTime, FVector& CamLoc, FRotator& CamRot, const FVector& ViewOffset);
	void AdjustToFocusPoint(class APawn* P, FLOAT DeltaTime, FVector& CamLoc, FRotator& CamRot);
	void PreventCameraPenetration(class APawn* P, class AGamePlayerCamera* CameraActor, const FVector& WorstLocation, FVector& DesiredLocation, FLOAT DeltaTime, FLOAT& DistBlockedPct, FLOAT CameraExtentScale, UBOOL bSingleRayOnly=FALSE);
	void UpdateForMovingBase(class AActor* BaseActor);

	/** Returns desired camera origin offset that should be applied AFTER the interpolation, if any. */
	virtual FVector  GetPostInterpCameraOriginLocationOffset(APawn* TargetPawn) const { return FVector::ZeroVector; }
	virtual FRotator GetPostInterpCameraOriginRotationOffset(APawn* TargetPawn) const { return FRotator::ZeroRotator; }

	virtual FMatrix GetWorstCaseLocTransform(APawn* P) const;

	virtual UBOOL ShouldIgnorePenetrationHit(FCheckResult const* Hit, APawn* TargetPawn) const;
	virtual UBOOL ShouldDoPerPolyPenetrationTests(APawn* TargetPawn) const { return FALSE; };
	virtual UBOOL ShouldDoPredictavePenetrationAvoidance(APawn* TargetPawn) const;
	virtual void HandlePawnPenetration(FTViewTarget& OutVT);
	
	virtual UBOOL HandleCameraSafeZone( FVector& CameraOrigin, FRotator& CameraRotation, FLOAT DeltaTime ) { return FALSE; }
public:

};


/** Internal. */
protected function GameThirdPersonCameraMode CreateCameraMode(class<GameThirdPersonCameraMode> ModeClass)
{
	local GameThirdPersonCameraMode NewMode;
	NewMode = new(self) ModeClass;
	NewMode.ThirdPersonCam = self;
	NewMode.Init();
	return NewMode;
}

// reset the camera to a good state
function Reset()
{
	bResetCameraInterpolation = TRUE;
}


function Init()
{
	// Setup camera modes
	if (ThirdPersonCamDefault == None)
	{
		ThirdPersonCamDefault = CreateCameraMode(ThirdPersonCamDefaultClass);
	}
}

/** returns camera mode desired FOV */
event float GetDesiredFOV( Pawn ViewedPawn )
{
	if ( bFocusPointSet && (FocusPoint.CameraFOV > 0.f) && bFocusPointSuccessful )
	{
		return FocusPoint.CameraFOV;
	}

	return CurrentCamMode.GetDesiredFOV(ViewedPawn);
}


/**
* Player Update Camera code
*/
function UpdateCamera(Pawn P, GamePlayerCamera CameraActor, float DeltaTime, out TViewTarget OutVT)
{
	if( P == None && OutVT.Target != None )
	{
		OutVT.Target.GetActorEyesViewPoint( OutVT.POV.Location, OutVT.POV.Rotation );
		PlayerCamera.ApplyCameraModifiers(DeltaTime, OutVT.POV);
	}
	// give pawn chance to hijack the camera and do it's own thing.
	else if( (P != None) && P.CalcCamera(DeltaTime, OutVT.POV.Location, OutVT.POV.Rotation, OutVT.POV.FOV) )
	{
		//@fixme, move this call up into GearPlayerCamera??? look into it.
		PlayerCamera.ApplyCameraModifiers(DeltaTime, OutVT.POV);
		return;
	}
	else
	{
		UpdateCameraMode(P);
		if( CurrentCamMode != None )
		{
			PlayerUpdateCamera(P,CameraActor, DeltaTime, OutVT);
			CurrentCamMode.UpdatePostProcess(OutVT, DeltaTime);
		}
		else
		{
			`warn(GetFuncName() @ "CameraMode == None!!!");	
		}
		// prints local space camera offset (from pawnloc).  useful for determining camera anim test offsets
		//`log("***"@((OutVT.POV.Location - P.Location) << P.Controller.Rotation));
	}

	// if we had to reset camera interpolation, then turn off flag once it's been processed.
	bResetCameraInterpolation = FALSE;
}

/** Internal camera updating code */
native protected function PlayerUpdateCamera(Pawn P, GamePlayerCamera CameraActor, float DeltaTime, out TViewTarget OutVT);


/******************************
 * Camera turns
 ******************************/

/**
 * Initiates a forced camera rotation.
 * @param		StartAngle		Starting Yaw offset (in Rotator units)
 * @param		EndAngle		Finishing Yaw offset (in Rotator units)
 * @param		TimeSec			How long the rotation should take
 * @param		DelaySec		How long to wait before starting the rotation
 */
function BeginTurn(int StartAngle, int EndAngle, float TimeSec, optional float DelaySec, optional bool bAlignTargetWhenFinished)
{
	// We're not updating the camera on the server, bDoingACameraTurn will prevent firing
	if (PlayerCamera.bUseClientSideCameraUpdates && !PlayerCamera.PCOwner.IsLocalPlayerController())
	{
		return;
	}

	bDoingACameraTurn = TRUE;
	TurnTotalTime = TimeSec;
	TurnDelay = DelaySec;
	TurnCurTime = 0.f;
	TurnStartAngle = StartAngle;
	TurnEndAngle = EndAngle;
	bTurnAlignTargetWhenFinished = bAlignTargetWhenFinished;
}

/**
 * Stops a camera rotation.
 */
native function EndTurn();

/**
* Adjusts a camera rotation.  Useful for situations where the basis of the rotation
* changes.
* @param	AngleOffset		Yaw adjustment to apply (in Rotator units)
*/
function AdjustTurn(int AngleOffset)
{
	TurnStartAngle += AngleOffset;
	TurnEndAngle += AngleOffset;
}

/******************************
 * Focus Point functionality
 ******************************/

/** Tells camera to focus on the given world position. */
function SetFocusOnLoc
(
	vector			FocusWorldLoc,

	Vector2d		InterpSpeedRange,
	Vector2d		InFocusFOV,
	optional float	CameraFOV,
	optional bool	bAlwaysFocus,
	optional bool	bAdjustCamera,
	optional bool	bIgnoreTrace,
	optional float	FocusPitchOffsetDeg
)
{
	// if replacing a bAdjustCamera focus point with a !bAdjustCamera focus point,
	// do a clear first so the !bAdjustCamera one will work relative to where the first
	// one was at the time of the interruption
	if ( ((LastPitchAdjustment != 0) || (LastYawAdjustment != 0))
		&& !bAdjustCamera
		&& FocusPoint.bAdjustCamera )
	{
		ClearFocusPoint(TRUE);
	}

	FocusPoint.FocusWorldLoc	= FocusWorldLoc;
	FocusPoint.FocusActor		= None;
	FocusPoint.FocusBoneName	= '';

	FocusPoint.InterpSpeedRange	= InterpSpeedRange;
	FocusPoint.InFocusFOV		= InFocusFOV;
	FocusPoint.CameraFOV		= CameraFOV;
	FocusPoint.bAlwaysFocus		= bAlwaysFocus;
	FocusPoint.bAdjustCamera	= bAdjustCamera;
	FocusPoint.bIgnoreTrace		= bIgnoreTrace;
	FocusPoint.FocusPitchOffsetDeg = FocusPitchOffsetDeg;
	bFocusPointSet				= TRUE;

	LastFocusChangeTime			= PlayerCamera.WorldInfo.TimeSeconds;
	LastFocusPointLoc			= GetActualFocusLocation();
	bFocusPointSuccessful		= FALSE;
}

/** Tells camera to focus on the given actor. */
function SetFocusOnActor
(
	Actor			FocusActor,
	Name			FocusBoneName,

	Vector2d		InterpSpeedRange,
	Vector2d		InFocusFOV,
	optional float	CameraFOV,
	optional bool	bAlwaysFocus,
	optional bool	bAdjustCamera,
	optional bool	bIgnoreTrace,
	optional float	FocusPitchOffsetDeg
)
{
	// if replacing a bAdjustCamera focus point with a !bAdjustCamera focus point,
	// do a clear first so the !bAdjustCamera one will work relative to where the first
	// one was at the time of the interruption
	if ( ((LastPitchAdjustment != 0) || (LastYawAdjustment != 0))
		&& !bAdjustCamera
		&& FocusPoint.bAdjustCamera )
	{
		ClearFocusPoint(TRUE);
	}

	FocusPoint.FocusActor		= FocusActor;
	FocusPoint.FocusBoneName	= FocusBoneName;
	FocusPoint.InterpSpeedRange	= InterpSpeedRange;
	FocusPoint.InFocusFOV		= InFocusFOV;
	FocusPoint.CameraFOV		= CameraFOV;
	FocusPoint.bAlwaysFocus		= bAlwaysFocus;
	FocusPoint.bAdjustCamera	= bAdjustCamera;
	FocusPoint.bIgnoreTrace		= bIgnoreTrace;
	FocusPoint.FocusPitchOffsetDeg = FocusPitchOffsetDeg;
	bFocusPointSet				= TRUE;

	LastFocusChangeTime			= PlayerCamera.WorldInfo.TimeSeconds;
	LastFocusPointLoc			= GetActualFocusLocation();
	bFocusPointSuccessful		= FALSE;
}

/**
 * Returns ref to the actor currently being used as a focus point, if any.
 */
function Actor GetFocusActor()
{
	return bFocusPointSet ? FocusPoint.FocusActor : None;
}

/** Clear focus point */
function ClearFocusPoint(optional bool bLeaveCameraRotation)
{
	bFocusPointSet = FALSE;

	// note that bAdjustCamera must be true to leave camera rotation.
	// otherwise, there will be a large camera jolt as the player and camera
	// realign themselves (which they have to do, since the camera rotated away from
	// the player)
	if ( bLeaveCameraRotation && FocusPoint.bAdjustCamera )
	{
		LastPitchAdjustment = 0;
		LastYawAdjustment = 0;
		LeftoverPitchAdjustment = 0;
		if (PlayerCamera.PCOwner != None)
		{
			PlayerCamera.PCOwner.SetRotation(LastPreModifierCameraRot);
		}
	}
}

/**
 * Per-tick focus point processing, for polling gamestate and adjusting as desired.
 * Override if you want other systems or criteria to set focus points.
 */
protected event UpdateFocusPoint( Pawn P )
{
	if (bDoingACameraTurn)
	{
		// no POIs during camera turns
		ClearFocusPoint();
	}
	// give the camera mode a crack at it
	else if ( (CurrentCamMode == None) || (CurrentCamMode.SetFocusPoint(P) == FALSE) )
	{
		// Otherwise, clear focus point
		ClearFocusPoint();
	}

	if (bFocusPointSet)
	{
		// store old focus loc
		LastFocusPointLoc = ActualFocusPointWorldLoc;
		ActualFocusPointWorldLoc = GetActualFocusLocation();
	}

}

/** Internal.  Returns the world space position of the current focus point. */
protected function vector GetActualFocusLocation()
{
	local vector FocusLoc;
	local SkeletalMeshComponent ComponentIt;

	if (FocusPoint.FocusActor != None)
	{
		// actor's loc by default
		FocusLoc = FocusPoint.FocusActor.Location;

		// ... but use bone loc if possible
		if (FocusPoint.FocusBoneName != '')
		{
			foreach FocusPoint.FocusActor.ComponentList(class'SkeletalMeshComponent',ComponentIt)
			{
				//`log( ComponentIt.Owner @ `showvar(ComponentIt) @ ComponentIt.SkeletalMesh );

				// if the bone we are looking for exists use that otherwise keep looking
				// as there could be multiple SkelComps on this FocusActor
				if( ComponentIt.MatchRefBone(FocusPoint.FocusBoneName) != INDEX_NONE )
				{
					FocusLoc = ComponentIt.GetBoneLocation(FocusPoint.FocusBoneName);
					break;
				}
			}
		}
	}
	else
	{
		// focused world location, just use that
		FocusLoc = FocusPoint.FocusWorldLoc;
	}

	return FocusLoc;
}


/** 
 *  Use this if you keep the same focus point, but move the camera basis around underneath it 
 *  e.g. you want the camera to hold steady focus, but the camera target is rotating
 */
function AdjustFocusPointInterpolation(rotator Delta)
{
	if (bFocusPointSet && FocusPoint.bAdjustCamera)
	{
		Delta = Normalize(Delta);
		LastYawAdjustment -= Delta.Yaw;
		LastPitchAdjustment -= Delta.Pitch;
	}
}


/**
 * Evaluates the game state and returns the proper camera mode.
 *
 * @return 	  	new camera mode to use
 */
function GameThirdPersonCameraMode FindBestCameraMode(Pawn P)
{
	if (P != None)
	{
		// Just stick to default here, games should override this with appropriate modes if desired.
		return ThirdPersonCamDefault;
	}

	return None;
}


/**
 * Update current camera modes. Pick Best, handle transitions, etc.
 */
final protected function UpdateCameraMode(Pawn P)
{
	local GameThirdPersonCameraMode	NewCamMode;

	// Pick most suitable camera mode
	NewCamMode = FindBestCameraMode(P);

	if ( NewCamMode != CurrentCamMode )
	{
		// handle mode change
		if( CurrentCamMode != None )
		{
			CurrentCamMode.OnBecomeInActive(P, NewCamMode);
		}
		if( NewCamMode != None )
		{
			NewCamMode.OnBecomeActive(P, CurrentCamMode);
		}

`if(`notdefined(FINAL_RELEASE)) 
		bDebugChangedCameraMode = TRUE;
		//`log( "Camera Mode Changed"@`showvar(CurrentCamMode)@`showvar(NewCamMode), FALSE );
`endif

		CurrentCamMode = NewCamMode;		
	}
}


/**
 * Gives cameras a chance to change player view rotation
 */
function ProcessViewRotation( float DeltaTime, Actor ViewTarget, out Rotator out_ViewRotation, out Rotator out_DeltaRot )
{
	// see if camera mode wants to manipulate the view rotation
	if( CurrentCamMode != None )
	{
		CurrentCamMode.ProcessViewRotation(DeltaTime, ViewTarget, out_ViewRotation, out_DeltaRot);
	}
}

/** Called when Camera mode becomes active */
function OnBecomeActive( GameCameraBase OldCamera )
{
	if (!PlayerCamera.bInterpolateCamChanges)
	{
		Reset();
	}
	super.OnBecomeActive( OldCamera );
}

event ModifyPostProcessSettings(out PostProcessSettings PP)
{
	if( CurrentCamMode != none )
	{
		CurrentCamMode.ModifyPostProcessSettings(PP);
	}
}

function ResetInterpolation()
{
	Super.ResetInterpolation();

	LastHeightAdjustment = 0.f;
	LastYawAdjustment = 0.f;
	LastPitchAdjustment = 0.f;
	LeftoverPitchAdjustment = 0.f;
}



defaultproperties
{
	PenetrationBlendOutTime=0.15f
	PenetrationBlendInTime=0.1f
	PenetrationBlockedPct=1.f
	PenetrationExtentScale=1.f

	WorstLocPenetrationExtentScale=1.f
	WorstLocInterpSpeed=8

	bResetCameraInterpolation=TRUE	// set to true by default, so first frame is never interpolated

	OriginOffsetInterpSpeed=8

	Focus_BackOffStrength=0.33f
	Focus_StepHeightAdjustment= 64
	Focus_MaxTries=4
	Focus_FastAdjustKickInTime=0.5

	bDoingACameraTurn=FALSE

	DirectLookInterpSpeed=6.f

	// ray 0 is the main ray
	PenetrationAvoidanceFeelers(0)=(AdjustmentRot=(Pitch=0,Yaw=0,Roll=0),WorldWeight=1.f,PawnWeight=1.f,Extent=(X=14,Y=14,Z=14))

	// horizontally offset
	PenetrationAvoidanceFeelers(1)=(AdjustmentRot=(Pitch=0,Yaw=3072,Roll=0),WorldWeight=0.75f,PawnWeight=0.75f,Extent=(X=0,Y=0,Z=0), TraceInterval=3)
	PenetrationAvoidanceFeelers(2)=(AdjustmentRot=(Pitch=0,Yaw=-3072,Roll=0),WorldWeight=0.75f,PawnWeight=0.75f,Extent=(X=0,Y=0,Z=0), TraceInterval=3)
	PenetrationAvoidanceFeelers(3)=(AdjustmentRot=(Pitch=0,Yaw=6144,Roll=0),WorldWeight=0.5f,PawnWeight=0.5f,Extent=(X=0,Y=0,Z=0), TraceInterval=5)
	PenetrationAvoidanceFeelers(4)=(AdjustmentRot=(Pitch=0,Yaw=-6144,Roll=0),WorldWeight=0.5f,PawnWeight=0.5f,Extent=(X=0,Y=0,Z=0), TraceInterval=5)

	// vertically offset
	PenetrationAvoidanceFeelers(5)=(AdjustmentRot=(Pitch=3640,Yaw=0,Roll=0),WorldWeight=1.f,PawnWeight=1.f,Extent=(X=0,Y=0,Z=0), TraceInterval=4)
	PenetrationAvoidanceFeelers(6)=(AdjustmentRot=(Pitch=-3640,Yaw=0,Roll=0),WorldWeight=0.5f,PawnWeight=0.5f,Extent=(X=0,Y=0,Z=0), TraceInterval=4)

	ThirdPersonCamDefaultClass=class'GameThirdPersonCameraMode_Default'
}

