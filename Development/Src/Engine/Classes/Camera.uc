/**
 *	Camera: defines the Point of View of a player in world space.
 * 	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class Camera extends Actor
	notplaceable
	native(Camera)
	dependson(EngineBaseTypes)
	transient;

/** PlayerController Owning this Camera Actor */
var		PlayerController	PCOwner;

/** Camera Mode */
var		Name 	CameraStyle;
/** default FOV */
var		float	DefaultFOV;
/** true if FOV is locked to a constant value*/
var		bool	bLockedFOV;
/** value FOV is locked at */
var		float	LockedFOV;

/** If we should insert black areas when rendering the scene to ensure an aspect ratio of ConstrainedAspectRatio */
var		bool	bConstrainAspectRatio;
/** If bConstrainAspectRatio is true, add black regions to ensure aspect ratio is this. Ratio is horizontal/vertical. */
var		float	ConstrainedAspectRatio;
/** Default aspect ratio */
var		float	DefaultAspectRatio;

/** Off-axis yaw angle offset */
var		float	OffAxisYawAngle;

/** Off-axis pitch angle offset */
var		float	OffAxisPitchAngle;

/** If we should apply FadeColor/FadeAmount to the screen. */
var		bool	bEnableFading;
/** Color to fade to. */
var		color	FadeColor;
/** Amount of fading to apply. */
var		float	FadeAmount;

/** Apply fading of audio alongside the video */
var     bool    bFadeAudio;

/** Indicates if CamPostProcessSettings should be used when using this Camera to view through. */
var		float				CamOverridePostProcessAlpha;

/** Post-process settings to use if bCamOverridePostProcess is TRUE. */
var		PostProcessSettings	CamPostProcessSettings;

/** Rendering overrides that are active on this camera. */
var		RenderingPerformanceOverrides	RenderingOverrides;

/** 
 * Forces Temporal AA to be disabled when True. 
 * This is useful for hiding reprojection artifacts while certain game specific translucent effects are active.
 */
var		transient bool bForceDisableTemporalAA;

/** Turn on scaling of color channels in final image using ColorScale property. */
var		bool	bEnableColorScaling;
/** Allows control over scaling individual color channels in the final image. */
var		vector	ColorScale;
/** Should interpolate color scale values */
var		bool	bEnableColorScaleInterp;
/** Desired color scale which ColorScale will interpolate to */
var		vector	DesiredColorScale;
/** Color scale value at start of interpolation */
var		vector	OriginalColorScale;
/** Total time for color scale interpolation to complete */
var		float	ColorScaleInterpDuration;
/** Time at which interpolation started */
var		float	ColorScaleInterpStartTime;

/** The actors which the camera shouldn't see. Used to hide actors which the camera penetrates. */
//var array<Actor> HiddenActors;

/* Caching Camera, for optimization */
struct native TCameraCache
{
	/** Cached Time Stamp */
	var float	TimeStamp;
	/** cached Point of View */
	var TPOV	POV;
};
var	TCameraCache	CameraCache, LastFrameCameraCache;


/**
 * View Target definition
 * A View Target is responsible for providing the Camera with an ideal Point of View (POV)
 */
struct native TViewTarget
{
	/** Target Actor used to compute ideal POV */
	var()	Actor					Target;
	/** Controller of Target (only for non Locally controlled Pawns) */
	var()	Controller				Controller;
	/** Point of View */
	var()	TPOV					POV;
	/** Aspect ratio */
	var()	float					AspectRatio;
	/** PlayerReplicationInfo (used to follow same player through pawn transitions, etc., when spectating) */
	var()	PlayerReplicationInfo	PRI;

};

/** Current ViewTarget */
var TViewTarget	ViewTarget;
/** Pending view target for blending */
var	TViewTarget	PendingViewTarget;
/** Time left when blending to pending view target */
var float		BlendTimeToGo;

enum EViewTargetBlendFunction
{
	/** Camera does a simple linear interpolation. */
	VTBlend_Linear,
	/** Camera has a slight ease in and ease out, but amount of ease cannot be tweaked. */
	VTBlend_Cubic,
	/** Camera immediately accelerates, but smoothly decelerates into the target.  Ease amount controlled by BlendExp. */
	VTBlend_EaseIn,
	/** Camera smoothly accelerates, but does not decelerate into the target.  Ease amount controlled by BlendExp. */
	VTBlend_EaseOut,
	/** Camera smoothly accelerates and decelerates.  Ease amount controlled by BlendExp. */
	VTBlend_EaseInOut,
};

/** A set of parameters to describe how to transition between viewtargets. */
struct native ViewTargetTransitionParams
{
	/** Total duration of blend to pending view target.  0 means no blending. */
	var() float						BlendTime;
	/** Function to apply to the blend parameter */
	var() EViewTargetBlendFunction	BlendFunction;
	/** Exponent, used by certain blend functions to control the shape of the curve. */
	var() float						BlendExp;
	/** If TRUE, lock outgoing viewtarget to last frame's camera position for the remainder of the blend.
	 *  This is useful if you plan to teleport the viewtarget, but want to keep the camera motion smooth. */
	var() bool                      bLockOutgoing;

	structdefaultproperties
	{
		BlendFunction=VTBlend_Cubic
		BlendExp=2.f
		bLockOutgoing=FALSE
	}

	// providing the constructor by hand here, because we pass this as an optional parameter
	// and when the parameter isn't there, the default contructor is called.
	structcpptext
	{
		FViewTargetTransitionParams()
		{}
		FViewTargetTransitionParams(EEventParm)
		: BlendTime(0.f), BlendFunction(VTBlend_Cubic), BlendExp(2.f), bLockOutgoing(FALSE)
		{}
	}
};

var ViewTargetTransitionParams BlendParams;

/** List of camera modifiers to apply during update of camera position/ rotation */
var Array<CameraModifier>	ModifierList;

/** Distance to place free camera from view target */
var float		FreeCamDistance;

/** Offset to Z free camera position */
var vector		FreeCamOffset;

/** camera fade management */
var vector2d FadeAlpha;
var float FadeTime, FadeTimeRemaining;


// "Lens" effects (e.g. blood, dirt on camera)
/** CameraBlood emitter attached to this camera */
var protected transient array<EmitterCameraLensEffectBase> CameraLensEffects;



/////////////////////
// Camera Modifiers
/////////////////////

/** Camera modifier for cone-driven screen shakes */
var() editinline transient CameraModifier_CameraShake   CameraShakeCamMod;
/** Class to use when instantiating screenshake modifier object.  Provided to support overrides. */
var() protected class<CameraModifier_CameraShake>       CameraShakeCamModClass;

enum ECameraAnimPlaySpace
{
	/** This anim is applied in camera space */
	CAPS_CameraLocal,
	/** This anim is applied in world space */
	CAPS_World,
	/** This anim is applied in a user-specified space (defined by UserPlaySpaceMatrix) */
	CAPS_UserDefined,
};

////////////////////////
// CameraAnim support
////////////////////////

const MAX_ACTIVE_CAMERA_ANIMS = 8;

/** Pool of anim instance objects available with which to play camera animations */
var protected CameraAnimInst			AnimInstPool[MAX_ACTIVE_CAMERA_ANIMS];

/** Array of anim instances that are currently playing and in-use */
var protected array<CameraAnimInst>		ActiveAnims;
/** Array of anim instances that are not playing and available */
var protected array<CameraAnimInst>		FreeAnims;

/** Internal.  Receives the output of individual camera animations. */
var protected transient DynamicCameraActor AnimCameraActor;

/** if true, server will use camera positions replicated from the client instead of calculating locally. */
var bool bUseClientSideCameraUpdates;

/** If true, replicate the client side camera position but don't use it, and draw the positions on the server */
var bool bDebugClientSideCamera;

/** if true, send a camera update to the server on next update */
var bool bShouldSendClientSideCameraUpdate;

cpptext
{
protected:
	void InitTempCameraActor(class ACameraActor* CamActor, class UCameraAnim* AnimToInitFor) const;
	void ApplyAnimToCamera(class ACameraActor const* AnimatedCamActor, class UCameraAnimInst const* AnimInst, FTPOV& OutPOV);

	UCameraAnimInst* AllocCameraAnimInst();
	void ReleaseCameraAnimInst(UCameraAnimInst* Inst);
	UCameraAnimInst* FindExistingCameraAnimInst(UCameraAnim const* Anim);

public:
	virtual void ModifyPostProcessSettings(FPostProcessSettings& PPSettings) const;

	void	AssignViewTarget(AActor* NewTarget, FTViewTarget& VT, struct FViewTargetTransitionParams TransitionParams=FViewTargetTransitionParams(EC_EventParm));
	AActor* GetViewTarget();
	virtual UBOOL	PlayerControlled();
}

/**
 * Internal. Creates and initializes a new camera modifier of the specified class, returns the object ref.
 */
protected function CameraModifier CreateCameraModifier(class<CameraModifier> ModifierClass)
{
	local CameraModifier NewMod;
	NewMod = new(Outer) ModifierClass;
	NewMod.Init();
	NewMod.AddCameraModifier(Self);
	return NewMod;
}


function PostBeginPlay()
{
	local int Idx;

	super.PostBeginPlay();

 	// Setup camera modifiers
 	if( (CameraShakeCamMod == None) && (CameraShakeCamModClass != None) )
 	{
 		CameraShakeCamMod = CameraModifier_CameraShake(CreateCameraModifier(CameraShakeCamModClass));
 	}

	// create CameraAnimInsts in pool
	for (Idx=0; Idx<MAX_ACTIVE_CAMERA_ANIMS; ++Idx)
	{
		AnimInstPool[Idx] = new(Self) class'CameraAnimInst';

		// add everything to the free list initially
		FreeAnims[Idx] = AnimInstPool[Idx];
	}

	// spawn the two temp CameraActors used for updating CameraAnims
	AnimCameraActor = Spawn(class'DynamicCameraActor', self,,,,, TRUE);
}

event Destroyed()
{
	// clean up the temp camera actors
	AnimCameraActor.Destroy();
	super.Destroyed();
}

/**
 * Apply modifiers on Camera.
 * @param	DeltaTime	Time is seconds since last update
 * @param	OutPOV		Point of View
 */

native function ApplyCameraModifiers(float DeltaTime, out TPOV OutPOV);

/**
 * Initialize Camera for associated PlayerController
 * @param	PC	PlayerController attached to this Camera.
 */
function InitializeFor(PlayerController PC)
{
	CameraCache.POV.FOV = DefaultFOV;
	PCOwner				= PC;

	SetViewTarget(PC.ViewTarget);

	// set the level default scale
	SetDesiredColorScale(WorldInfo.DefaultColorScale, 5.f);

	// Force camera update so it doesn't sit at (0,0,0) for a full tick.
	// This can have side effects with streaming.
	UpdateCamera(0.f);
}


/**
 * returns camera's current FOV angle
 */
function float GetFOVAngle()
{
	if( bLockedFOV )
	{
		return LockedFOV;
	}

	return CameraCache.POV.FOV;
}


/**
 * Lock FOV to a specific value.
 * A value of 0 to beyond 170 will unlock the FOV setting.
 */
function SetFOV(float NewFOV)
{
	if( NewFOV < 1 || NewFOV > 170 )
	{
		bLockedFOV = FALSE;
		return;
	}

	bLockedFOV	= TRUE;
	LockedFOV	= NewFOV;
}


/**
 * Master function to retrieve Camera's actual view point.
 * do not call this directly, call PlayerController::GetPlayerViewPoint() instead.
 *
 * @param	OutCamLoc	Camera Location
 * @param	OutCamRot	Camera Rotation
 */
final function GetCameraViewPoint(out vector OutCamLoc, out rotator OutCamRot)
{
	// @debug: find out which calls are being made before the camera has been ticked
	//			and have therefore one frame of lag.
	/*
	if( CameraCache.TimeStamp != WorldInfo.TimeSeconds )
	{
		`Log(WorldInfo.TimeSeconds @ GetFuncName() @ "one frame of lag");
		ScriptTrace();
	}
	*/

	OutCamLoc = CameraCache.POV.Location;
	OutCamRot = CameraCache.POV.Rotation;
}

final function rotator GetCameraRotation()
{
	return CameraCache.POV.Rotation;
}

/**
 * Sets the new desired color scale and enables interpolation.
 */
simulated function SetDesiredColorScale(vector NewColorScale, float InterpTime)
{
	// if color scaling is not enabled
	if (!bEnableColorScaling)
	{
		// set the default color scale
		bEnableColorScaling = TRUE;
		ColorScale.X = 1.f;
		ColorScale.Y = 1.f;
		ColorScale.Z = 1.f;
	}

	// Don't bother interpolating if we're already scaling at the desired color
	if( NewColorScale != ColorScale )
	{
		// save the current as original
		OriginalColorScale = ColorScale;
		// set the new desired scale
		DesiredColorScale = NewColorScale;
		// set the interpolation duration/time
		ColorScaleInterpStartTime = WorldInfo.TimeSeconds;
		ColorScaleInterpDuration = InterpTime;
		// and enable color scale interpolation
		bEnableColorScaleInterp = TRUE;
	}
}

/**
 * Performs camera update.
 * Called once per frame after all actors have been ticked.
 * Non-local players replicate the POV if bUseClientSideCameraUpdates is true
 */
simulated event UpdateCamera(float DeltaTime)
{
	if ( PCOwner.IsLocalPlayerController() || !bUseClientSideCameraUpdates || bDebugClientSideCamera )
	{
		DoUpdateCamera(DeltaTime);

		if (WorldInfo.NetMode == NM_Client && bShouldSendClientSideCameraUpdate)
		{
			PCOwner.ServerUpdateCamera(CameraCache.POV.Location, (CameraCache.POV.Rotation.Pitch & 65535) + ((CameraCache.POV.Rotation.Yaw & 65535) << 16));
			bShouldSendClientSideCameraUpdate = FALSE;
		}
	}
}

simulated function DoUpdateCamera(float DeltaTime)
{
	local TPOV		NewPOV;
	local float		DurationPct, BlendPct;

	// update color scale interpolation
	if (bEnableColorScaleInterp)
	{
		BlendPct = FClamp(`TimeSince(ColorScaleInterpStartTime)/ColorScaleInterpDuration,0.f,1.f);
		ColorScale = VLerp(OriginalColorScale,DesiredColorScale,BlendPct);
		// if we've maxed
		if (BlendPct == 1.f)
		{
			// disable further interpolation
			bEnableColorScaleInterp = FALSE;
		}
	}

	// Reset aspect ratio and postprocess override associated with CameraActor.
	bConstrainAspectRatio = FALSE;
	CamOverridePostProcessAlpha = 0.f;

	// Don't update outgoing viewtarget during an interpolation when bLockOutgoing is set.
	if( PendingViewTarget.Target == None || !BlendParams.bLockOutgoing )
	{
		// Update current view target
		CheckViewTarget(ViewTarget);
		UpdateViewTarget(ViewTarget, DeltaTime);
	}

	// our camera is now viewing there
	NewPOV					= ViewTarget.POV;
	ConstrainedAspectRatio	= ViewTarget.AspectRatio;

	// if we have a pending view target, perform transition from one to another.
	if( PendingViewTarget.Target != None )
	{
		BlendTimeToGo -= DeltaTime;

		// Reset aspect ratio.  The call to UpdateViewTarget() may turn this back on.
		bConstrainAspectRatio = FALSE;

		// Update pending view target
		CheckViewTarget(PendingViewTarget);
		UpdateViewTarget(PendingViewTarget, DeltaTime);

		// blend....
		if( BlendTimeToGo > 0 )
		{
			DurationPct	= (BlendParams.BlendTime - BlendTimeToGo) / BlendParams.BlendTime;

			switch (BlendParams.BlendFunction)
			{
			case VTBlend_Linear:
				BlendPct = Lerp(0.f, 1.f, DurationPct);
				break;
			case VTBlend_Cubic:
				BlendPct = FCubicInterp(0.f, 0.f, 1.f, 0.f, DurationPct);
				break;
			case VTBlend_EaseIn:
				BlendPct = FInterpEaseIn(0.f, 1.f, DurationPct, BlendParams.BlendExp);
				break;
			case VTBlend_EaseOut:
				BlendPct = FInterpEaseOut(0.f, 1.f, DurationPct, BlendParams.BlendExp);
				break;
			case VTBlend_EaseInOut:
				BlendPct = FInterpEaseInOut(0.f, 1.f, DurationPct, BlendParams.BlendExp);
				break;
			}
			//BlendPct	= FCubicInterp(0.f, class'DialogueManager'.default.OutTan, 1.f, class'DialogueManager'.default.InTan, 1.f - DurationPct);

			// Update pending view target blend
			NewPOV = BlendViewTargets(ViewTarget, PendingViewTarget, BlendPct);
		}
		else
		{
			// we're done blending, set new view target
			ViewTarget = PendingViewTarget;

			// clear pending view target
			PendingViewTarget.Target		= None;
			PendingViewTarget.Controller	= None;

			BlendTimeToGo = 0;

			// our camera is now viewing there
			NewPOV = PendingViewTarget.POV;
		}

		if( bConstrainAspectRatio )
		{
			// NOTE: We don't interpolate aspect ratio since either the prior or pending view target's AspectRatio
			//       may be the default value (1.3333) unless the view target has a camera actor set to override
			//       the aspect ratio.  We'll just use the pending view target's aspect.
			ConstrainedAspectRatio = PendingViewTarget.AspectRatio;
		}
	}

	// Cache results
	FillCameraCache(NewPOV);

	if (bEnableFading && FadeTimeRemaining > 0.0)
	{
		FadeTimeRemaining = FMax(FadeTimeRemaining - DeltaTime, 0.0);
		if (FadeTime > 0.0)
		{
			FadeAmount = FadeAlpha.X + ((1.f - FadeTimeRemaining/FadeTime) * (FadeAlpha.Y - FadeAlpha.X));
		}

		if (bFadeAudio)
		{
			ApplyAudioFade();
			if (FadeAmount == 0)
			{
				bFadeAudio = false;
			}
		}
	}
}

/** Apply audio fading */
native function ApplyAudioFade();

/**
 * Blend 2 viewtargets.
 *
 * @param	A		Source view target
 * @paramn	B		destination view target
 * @param	Alpha	Alpha, % of blend from A to B.
 */
final function TPOV BlendViewTargets(const out TViewTarget A,const out TViewTarget B, float Alpha)
{
	local TPOV	POV;

	POV.Location	= VLerp(A.POV.Location, B.POV.Location, Alpha);
	POV.FOV			= Lerp(A.POV.FOV, B.POV.FOV, Alpha);
	POV.Rotation	= RLerp(A.POV.Rotation, B.POV.Rotation, Alpha, TRUE);

	return POV;
}


/**
 * Cache update results
 */
final function FillCameraCache(const out TPOV NewPOV)
{
	// Backup last frame results.
	if( CameraCache.TimeStamp != WorldInfo.TimeSeconds )
	{
		LastFrameCameraCache = CameraCache;
	}
	CameraCache.TimeStamp	= WorldInfo.TimeSeconds;
	CameraCache.POV			= NewPOV;
}


/**
 * Make sure ViewTarget is valid
 */
native function CheckViewTarget(out TViewTarget VT);


/**
 * Query ViewTarget and outputs Point Of View.
 *
 * @param	OutVT		ViewTarget to use.
 * @param	DeltaTime	Delta Time since last camera update (in seconds).
 */
function UpdateViewTarget(out TViewTarget OutVT, float DeltaTime)
{
	local vector		Loc, Pos, HitLocation, HitNormal;
	local rotator		Rot;
	local Actor			HitActor;
	local CameraActor	CamActor;
	local bool			bDoNotApplyModifiers;
	local TPOV			OrigPOV;
	local Pawn          TPawn;

	// Don't update outgoing viewtarget during an interpolation 
	if( PendingViewTarget.Target != None && OutVT == ViewTarget && BlendParams.bLockOutgoing )
	{
		return;
	}

	// store previous POV, in case we need it later
	OrigPOV = OutVT.POV;

	// Default FOV on viewtarget
	OutVT.POV.FOV = DefaultFOV;

	// Viewing through a camera actor.
	CamActor = CameraActor(OutVT.Target);
	if( CamActor != None )
	{
		CamActor.GetCameraView(DeltaTime, OutVT.POV);

		// Grab aspect ratio from the CameraActor.
		bConstrainAspectRatio	= bConstrainAspectRatio || CamActor.bConstrainAspectRatio;
		OutVT.AspectRatio		= CamActor.AspectRatio;

		// See if the CameraActor wants to override the PostProcess settings used.
		CamOverridePostProcessAlpha = CamActor.CamOverridePostProcessAlpha;
		CamPostProcessSettings = CamActor.CamOverridePostProcess;
	}
	else
	{
		TPawn = Pawn(OutVT.Target);
		// Give Pawn Viewtarget a chance to dictate the camera position.
		// If Pawn doesn't override the camera view, then we proceed with our own defaults
		if( TPawn == None || !TPawn.CalcCamera(DeltaTime, OutVT.POV.Location, OutVT.POV.Rotation, OutVT.POV.FOV) )
		{
			// don't apply modifiers when using these debug camera modes.
			bDoNotApplyModifiers = TRUE;

			switch( CameraStyle )
			{
				case 'Fixed'		:	// do not update, keep previous camera position by restoring
										// saved POV, in case CalcCamera changes it but still returns false
										OutVT.POV = OrigPOV;
										break;

				case 'ThirdPerson'	: // Simple third person view implementation
				case 'FreeCam'		:
				case 'FreeCam_Default':
										Loc = OutVT.Target.Location;
										Rot = OutVT.Target.Rotation;

										// Take into account Mesh Translation so it takes into account the PostProcessing we do there.
										if ((TPawn != None) && (TPawn.Mesh != None))
										{
											Loc += (TPawn.Mesh.Translation - TPawn.default.Mesh.Translation) >> OutVT.Target.Rotation;
										}

										//OutVT.Target.GetActorEyesViewPoint(Loc, Rot);
										if( CameraStyle == 'FreeCam' || CameraStyle == 'FreeCam_Default' )
										{
											Rot = PCOwner.Rotation;
										}
										Loc += FreeCamOffset >> Rot;

										Pos = Loc - Vector(Rot) * FreeCamDistance;
										// @fixme, respect BlockingVolume.bBlockCamera=false
										HitActor = Trace(HitLocation, HitNormal, Pos, Loc, FALSE, vect(12,12,12));
										OutVT.POV.Location = (HitActor == None) ? Pos : HitLocation;
										OutVT.POV.Rotation = Rot;
										break;

				case 'FirstPerson'	: // Simple first person, view through viewtarget's 'eyes'
				default				:	OutVT.Target.GetActorEyesViewPoint(OutVT.POV.Location, OutVT.POV.Rotation);
										// Take into account Mesh Translation so it takes into account the PostProcessing we do there.
										if ((TPawn != None) && (TPawn.Mesh != None))
										{
											OutVT.POV.Location += (TPawn.Mesh.Translation - TPawn.default.Mesh.Translation) >> OutVT.Target.Rotation;
										}
										break;

			}
		}
	}

	if( !bDoNotApplyModifiers )
	{
		// Apply camera modifiers at the end (view shakes for example)
		ApplyCameraModifiers(DeltaTime, OutVT.POV);
	}
	//`log( WorldInfo.TimeSeconds  @ GetFuncName() @ OutVT.Target @ OutVT.POV.Location @ OutVT.POV.Rotation @ OutVT.POV.FOV );
}


/**
 * Set a new ViewTarget with optional BlendTime
 */
native final function SetViewTarget(Actor NewViewTarget, optional ViewTargetTransitionParams TransitionParams);


/**
 * Give each modifier a chance to change view rotation/deltarot
 */
function ProcessViewRotation(float DeltaTime, out rotator OutViewRotation, out Rotator OutDeltaRot)
{
	local int ModifierIdx;

	for( ModifierIdx = 0; ModifierIdx < ModifierList.Length; ModifierIdx++ )
	{
		if( ModifierList[ModifierIdx] != None )
		{
			if( ModifierList[ModifierIdx].ProcessViewRotation(ViewTarget.Target, DeltaTime, OutViewRotation, OutDeltaRot) )
			{
				break;
			}
		}
	}
}

/**
 * list important Camera variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local Vector	EyesLoc;
	local Rotator	EyesRot;
	local Canvas	Canvas;

	Canvas = HUD.Canvas;
	Canvas.SetDrawColor(255,255,255);

	Canvas.DrawText("	Camera Style:" $ CameraStyle @ "main ViewTarget:" $ ViewTarget.Target);
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);

	Canvas.DrawText("   CamLoc:" $ CameraCache.POV.Location @ "CamRot:" $ CameraCache.POV.Rotation @ "FOV:" $ CameraCache.POV.FOV);
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);

	Canvas.DrawText("   AspectRatio:" $ ConstrainedAspectRatio);
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);

	if( ViewTarget.Target != None )
	{
		ViewTarget.Target.GetActorEyesViewPoint(EyesLoc, EyesRot);
		Canvas.DrawText("   EyesLoc:" $ EyesLoc @ "EyesRot:" $ EyesRot);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}
}



////////////////////////////////
// Camera Lens Effects
////////////////////////////////

/** Finds the first instance of a lens effect of the given class, using linear search. */
function EmitterCameraLensEffectBase FindCameraLensEffect(class<EmitterCameraLensEffectBase> LensEffectEmitterClass)
{
	local EmitterCameraLensEffectBase LensEffect;

	foreach CameraLensEffects(LensEffect)
	{
		if ( !LensEffect.bDeleteMe &&
			 ( (LensEffect.Class == LensEffectEmitterClass) ||
			   (LensEffect.EmittersToTreatAsSame.Find(LensEffectEmitterClass) != INDEX_NONE) ||
			   (LensEffectEmitterClass.default.EmittersToTreatAsSame.Find(LensEffect.Class) != INDEX_NONE ) ) )
		{
			return LensEffect;
		}
	}

	return None;
}

/**
 *  Initiates a camera lens effect of the given class on this camera.
 */
function AddCameraLensEffect(class<EmitterCameraLensEffectBase> LensEffectEmitterClass)
{
	local vector CamLoc;
	local rotator CamRot;
	local EmitterCameraLensEffectBase LensEffect;

	if (LensEffectEmitterClass != None)
	{
		if (!LensEffectEmitterClass.default.bAllowMultipleInstances)
		{
			LensEffect = FindCameraLensEffect(LensEffectEmitterClass);

			if (LensEffect != None)
			{
				LensEffect.NotifyRetriggered();
			}
		}

		if (LensEffect == None)
		{
			// spawn with viewtarget as the owner so bOnlyOwnerSee works as intended
			LensEffect = Spawn( LensEffectEmitterClass, PCOwner.GetViewTarget() );
			if (LensEffect != None)
			{
				GetCameraViewPoint(CamLoc, CamRot);
				LensEffect.UpdateLocation(CamLoc, CamRot, GetFOVAngle());
				LensEffect.RegisterCamera(self);

				CameraLensEffects.AddItem(LensEffect);
			}
		}
	}
}

/** Removes this particular lens effect from the camera. */
function RemoveCameraLensEffect(EmitterCameraLensEffectBase Emitter)
{
	CameraLensEffects.RemoveItem(Emitter);
}

/** Removes all Camera Lens Effects. */
function ClearCameraLensEffects()
{
	local EmitterCameraLensEffectBase LensEffect;

	foreach CameraLensEffects(LensEffect)
	{
		LensEffect.Destroy();
	}

	// empty the array.  unnecessary, since destruction will call RemoveCameraLensEffect,
	// but this gets it done in one fell swoop.
	CameraLensEffects.length = 0;
}

/** ------------------------------------------------------------
 *  Camera Shakes
 *  ------------------------------------------------------------ */


/**
 * Play a camera shake
 */
function PlayCameraShake(CameraShake Shake, float Scale, optional ECameraAnimPlaySpace PlaySpace=CAPS_CameraLocal, optional rotator UserPlaySpaceRot)
{
	if (Shake != None)
	{
		CameraShakeCamMod.AddCameraShake(Shake, Scale, PlaySpace, UserPlaySpaceRot);
	}
}

/** Stop playing a camera shake. */
function StopCameraShake(CameraShake Shake)
{
	if (Shake != None)
	{
		CameraShakeCamMod.RemoveCameraShake(Shake);
	}
}


/** Internal.  Returns intensity scalar in the range [0..1] for a shake originating at Epicenter. */
static function float CalcRadialShakeScale(Camera Cam, vector Epicenter, float InnerRadius, float OuterRadius, float Falloff)
{
	local Vector			POVLoc;
	local float				DistPct;

	// using camera location so stuff like spectator cameras get shakes applied sensibly as well
	// need to ensure server has reasonably accurate camera position
	POVLoc = Cam.Location;

	if (InnerRadius < OuterRadius)
	{
		DistPct = (VSize(Epicenter - POVLoc) - InnerRadius) / (OuterRadius - InnerRadius);
		DistPct = 1.f - FClamp(DistPct, 0.f, 1.f);
		return DistPct ** Falloff;
	}
	else
	{
		// ignore OuterRadius and do a cliff falloff at InnerRadius
		return (VSize(Epicenter - POVLoc) < InnerRadius) ? 1.f : 0.f;
	}
}


/**
 * Static.  Plays an in-world camera shake that affects all nearby players, with distance-based attenuation.
 */
static function PlayWorldCameraShake(CameraShake Shake, Actor ShakeInstigator, vector Epicenter, float InnerRadius, float OuterRadius, float Falloff, bool bTryForceFeedback, optional bool bOrientShakeTowardsEpicenter )
{
	local PlayerController	PC;
	local float ShakeScale;
	local Rotator CamRot;
	local vector CamLoc;

	if( ShakeInstigator != None )
	{
		foreach ShakeInstigator.LocalPlayerControllers(class'PlayerController', PC)
		{
			if (PC.PlayerCamera != None)
			{
				ShakeScale = CalcRadialShakeScale(PC.PlayerCamera, Epicenter, InnerRadius, OuterRadius, Falloff);

				if (bOrientShakeTowardsEpicenter && PC.Pawn != None)
				{
					PC.PlayerCamera.GetCameraViewPoint(CamLoc, CamRot);
					PC.ClientPlayCameraShake(Shake, ShakeScale, bTryForceFeedback, CAPS_UserDefined, rotator(Epicenter - CamLoc));
				}
				else
				{
					PC.ClientPlayCameraShake(Shake, ShakeScale, bTryForceFeedback);

				}
			}
		}
	}
}

function ClearAllCameraShakes()
{
	CameraShakeCamMod.RemoveAllCameraShakes();
//	StopAllCameraAnims(TRUE);
}


/** ------------------------------------------------------------
 *  CameraAnim support
 *  ------------------------------------------------------------ */

/** Play the indicated CameraAnim on this camera.  Returns the CameraAnim instance. */
simulated native function CameraAnimInst PlayCameraAnim(CameraAnim Anim, optional float Rate=1.f, optional float Scale=1.f, optional float BlendInTime, optional float BlendOutTime, optional bool bLoop, optional bool bRandomStartTime, optional float Duration, optional bool bSingleInstance);

/**
 * Stop playing all instances of the indicated CameraAnim.
 * bImmediate: TRUE to stop it right now, FALSE to blend it out over BlendOutTime.
 */
simulated native function StopAllCameraAnims(optional bool bImmediate);

/**
 * Stop playing all instances of the indicated CameraAnim.
 * bImmediate: TRUE to stop it right now, FALSE to blend it out over BlendOutTime.
 */
simulated native function StopAllCameraAnimsByType(CameraAnim Anim, optional bool bImmediate);

/**
 * Stops the given CameraAnim instance from playing.  The given pointer should be considered invalid after this.
 */
simulated native function StopCameraAnim(CameraAnimInst AnimInst, optional bool bImmediate);


defaultproperties
{
	DefaultFOV=95.f
	DefaultAspectRatio=AspectRatio4x3
	bHidden=TRUE
	RemoteRole=ROLE_None
	FreeCamDistance=256.f
	bUseClientSideCameraUpdates=TRUE
	bDebugClientSideCamera=FALSE

	CameraShakeCamModClass=class'CameraModifier_CameraShake'
}
