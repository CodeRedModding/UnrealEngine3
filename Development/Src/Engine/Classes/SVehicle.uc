//=============================================================================
// SVehicle
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class SVehicle extends Vehicle
	native(Physics)
	nativereplication
	abstract;

cpptext
{
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// Actor interface.
	virtual void physRigidBody(FLOAT DeltaTime);
	virtual void TickSimulated( FLOAT DeltaSeconds );
	virtual void TickAuthoritative( FLOAT DeltaSeconds );
	virtual void setPhysics(BYTE NewPhysics, AActor *NewFloor, FVector NewFloorV);
	virtual void PostNetReceiveLocation();
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );

	// SVehicle interface.
	virtual void VehiclePackRBState();
	virtual void VehicleUnpackRBState();
	virtual FVector GetDampingForce(const FVector& InForce);

#if WITH_NOVODEX

    virtual void OnRigidBodyCollision(const FRigidBodyCollisionInfo& MyInfo, const FRigidBodyCollisionInfo& OtherInfo, const FCollisionImpactData& RigidCollisionData);
	virtual void ModifyNxActorDesc(NxActorDesc& ActorDesc,UPrimitiveComponent* PrimComp, const class NxGroupsMask& GroupsMask, UINT MatIndex);
	virtual void PostInitRigidBody(NxActor* nActor, NxActorDesc& ActorDesc, UPrimitiveComponent* PrimComp);
	virtual void PreTermRigidBody(NxActor* nActor);
	virtual void TermRBPhys(FRBPhysScene* Scene);
#endif

	virtual void UpdateVehicle(ASVehicle* Vehicle, FLOAT DeltaTime) {}

	/** Set any params on wheel particle effect */
	virtual void SetWheelEffectParams(USVehicleWheel* VW, FLOAT SlipVel);
}

/** Object containing the actual vehicle simulation paramters and code. Allows you to 'plug' different vehicle types in. */
var() noclear const			SVehicleSimBase			SimObj;

/** Data for each wheel. */
var() editinline export		array<SVehicleWheel>	Wheels;

/** Center of mass location of the vehicle, in local space. */
var()						vector					COMOffset;

/** Inertia Tensor Multipler - allows you to scale the components of the pre-calculated inertia tensor */
var()						vector					InertiaTensorMultiplier;

/** Use the stay-upright world constraint */
var(UprightConstraint)      bool                    bStayUpright;

/** Angle at which the vehicle will resist rolling */
var(UprightConstraint)      float                   StayUprightRollResistAngle;

/** Angle at which the vehicle will resist pitching */
var(UprightConstraint)      float                   StayUprightPitchResistAngle;

/** Amount of spring past the limit angles */
var(UprightConstraint)      float                   StayUprightStiffness;

/** Amount of dampening past the limit angles */
var(UprightConstraint)      float                   StayUprightDamping;

var editinline export	    RB_StayUprightSetup     StayUprightConstraintSetup;
var editinline export	    RB_ConstraintInstance   StayUprightConstraintInstance;

var                         bool                    bUseSuspensionAxis;

/** set this flag to cause wheel shape parameters to be updated.  Cleared after update occurs. */
var							bool					bUpdateWheelShapes;

/** Percent the Suspension must want to move in one go for the heavy shift function to be called*/
var							float					HeavySuspensionShiftPercent;

/** Vehicle total speed will be capped to this value */
var()						float					MaxSpeed;

/** Vehicle angular velocity will be capped to this value */
var()						float					MaxAngularVelocity;

/** OUTPUT: True if _any_ SVehicleWheel is currently touching the ground (ignores contacts with chassis etc) */
var	const bool			bVehicleOnGround;

/** OUTPUT: Time that bVehicleOnGround has been false for. */
var const float			TimeOffGround;

/** OUTPUT: True if _any_ SVehicleWheel is currently touching water */
var	const bool			bVehicleOnWater;

/** OUTPUT: True if vehicle is mostly upside down. */
var	const bool			bIsInverted;

/** OUTPUT: True if there are any contacts between the chassis of the vehicle and the ground. */
var	const bool			bChassisTouchingGround;

/** OUTPUT: True if there were any contacts between the chassis of the vehicle and the ground last tick */
var	const bool			bWasChassisTouchingGroundLastTick;

/** @name Vehicle uprighting */
//@{

/** true if vehicle can be uprighted by player */
var				bool		bCanFlip;
/** Scales the lifting force applied to the vehicle during uprighting.	*/
var(Uprighting)	float		UprightLiftStrength;
/** Scales the torque applied to the vehicle during uprighting.			*/
var(Uprighting)	float		UprightTorqueStrength;
/** Time in seconds to apply uprighting force/torque.					*/
var(Uprighting)	float		UprightTime;

/** Changes the direction the car gets flipped (used to prevent flipping car onto player	*/
var				bool		bFlipRight;
/** Internal variable.  True while uprighting forces are being applied.	*/
var				bool		bIsUprighting;

/** Internal variable.  Marks the time that uprighting began.			*/
var				float		UprightStartTime;
//@}

/** @name Sounds													*/
//@{
/** Ambient engine-running sound.  Pitch modulated based on RPMS.	*/
var(Sounds) editconst const AudioComponent EngineSound;

/** Volume-modulated wheel squeeling.								*/
var(Sounds) editconst const AudioComponent SquealSound;

/** Played when the vehicle slams into things.						*/
var(Sounds) SoundCue CollisionSound;

/** Engine startup sound played upon entering the vehicle.			*/
var(Sounds) SoundCue EnterVehicleSound;

/** Engine switch-off sound played upon exiting the vhicle.			*/
var(Sounds) SoundCue ExitVehicleSound;

/** Minimum time passed between the triggering collision sounds; generally set to longest collision sound. */
var(Sounds) float CollisionIntervalSecs;

/** Slip velocity cuttoff below which no wheel squealing is heard. */
var(Sounds) const float	SquealThreshold;

/** Lateral Slip velocity cut off below which no wheel squealing is heard */
var(Sounds) const float SquealLatThreshold;

/** multiplier for volume level of Lateral squeals relative to straight slip squeals. */
var(Sounds) const float LatAngleVolumeMult;

/** Time delay between the engine startup sound and the engine idling sound.						*/
var(Sounds) const float	EngineStartOffsetSecs;
/** Time delay between the engine shutdown sound and the deactivation of the engine idling sound.	*/
var(Sounds) const float	EngineStopOffsetSecs;

/** Internal variable; prevents collision sounds from being triggered too frequently.	*/
var float LastCollisionSoundTime;
//@}

// Internal
var		float				OutputBrake;
var		float				OutputGas;
var		float				OutputSteering;
var		float				OutputRise;
var		bool				bOutputHandbrake;
var     bool                bHoldingDownHandbrake;

var		float				ForwardVel;
var		int					NumPoweredWheels;

// camera
var()	vector				BaseOffset;
var()	float				CamDist;

var		int					DriverViewPitch;      // The driver's view pitch
var		int					DriverViewYaw;        // The driver's view yaw

// replication

struct native VehicleState
{
	var	RigidBodyState		RBState;

	var byte				ServerBrake;
	var byte				ServerGas;
	var	byte				ServerSteering;
	var byte				ServerRise;
	var bool				bServerHandbrake;
	var int					ServerView; // Pitch and Yaw - 16 bits each
};

var native const VehicleState	VState;
var	native const float			AngErrorAccumulator;

/** Hacky - used to scale hurtradius applied impulses */
var float RadialImpulseScaling;

replication
{
	if (Physics == PHYS_RigidBody)
		VState, MaxSpeed;
}

// Physics interface
native function AddForce(Vector Force);
native function AddImpulse(Vector Impulse);
native function AddTorque(Vector Torque);
native function bool IsSleeping();

/** turns on or off a wheel's collision
 * @param WheelNum the index of the wheel in the Wheels array to change
 * @param bCollision whether to turn collision on or off
 */
native final function SetWheelCollision(int WheelNum, bool bCollision);

/**
 * Called when gameplay begins. Used to make sure the engine sound audio component
 * has the right properties set to ensure it gets restarted if it has been cut out
 * due to the audio code running out of HW channels to play the sound in.
 */
simulated event PostBeginPlay()
{
	Super.PostBeginPlay();
	if( EngineSound != None )
	{
		EngineSound.bShouldRemainActiveIfDropped = TRUE;
	}
	if (CollisionSound != None && CollisionIntervalSecs <= 0.0)
	{
		CollisionIntervalSecs = CollisionSound.GetCueDuration() / WorldInfo.TimeDilation;
	}
}

/** Store pointer to each wheel's SkelControlWheel. */
simulated event PostInitAnimTree(SkeletalMeshComponent SkelComp)
{
	local int WheelIndex;
	local SVehicleWheel Wheel;

	Super.PostInitAnimTree(SkelComp);

	if (SkelComp == Mesh)
	{
		for(WheelIndex = 0; WheelIndex<Wheels.length; WheelIndex++)
		{
			Wheel = Wheels[WheelIndex];
			Wheel.WheelControl = SkelControlWheel( Mesh.FindSkelControl(Wheel.SkelControlName) );
		}
	}
}

simulated event Destroyed()
{
	Super.Destroyed();
	StopVehicleSounds();
}

/** TurnOff()
Freeze pawn - stop sounds, animations, physics, weapon firing
*/
simulated function TurnOff()
{
	Super.TurnOff();
	StopVehicleSounds();
}

simulated function StopVehicleSounds()
{
	if ( EngineSound != None )
	{
		EngineSound.Stop();
	}

	if ( SquealSound != None )
	{
		SquealSound.Stop();
	}
}

/**
 * Take Radius Damage
 * by default scales damage based on distance from HurtOrigin to Actor's location.
 * This can be overriden by the actor receiving the damage for special conditions (see KAsset.uc).
 *
 * @param	InstigatedBy, instigator of the damage
 * @param	Base Damage
 * @param	Damage Radius (from Origin)
 * @param	DamageType class
 * @param	Momentum (float)
 * @param	HurtOrigin, origin of the damage radius.
 * @param	bFullDamage, if true, damage not scaled based on distance HurtOrigin
 * @param DamageCauser the Actor that directly caused the damage (i.e. the Projectile that exploded, the Weapon that fired, etc)
 */
simulated function TakeRadiusDamage
(
	Controller			InstigatedBy,
	float				BaseDamage,
	float				DamageRadius,
	class<DamageType>	DamageType,
	float				Momentum,
	vector				HurtOrigin,
	bool				bFullDamage,
	Actor               DamageCauser,
	optional float      DamageFalloffExponent=1.f
)
{
	local vector HitLocation, Dir, NewDir;
	local float Dist, DamageScale;
	local TraceHitInfo HitInfo;

	if ( Role < ROLE_Authority )
		return;

	// calculate actual hit position on mesh, rather than approximating with cylinder
	HitLocation = Location;
	Dir = Location - HurtOrigin;

	CheckHitInfo( HitInfo, Mesh, Dir, HitLocation );
	NewDir = HitLocation - HurtOrigin;
	Dist = VSize(NewDir);

	if ( bFullDamage )
	{
		DamageScale = 1.f;
	}
	else if ( dist > DamageRadius )
		return;
	else
	{
		DamageScale = FMax(0,1 - Dist/DamageRadius);
		DamageScale = DamageScale ** DamageFalloffExponent;
	}

	RadialImpulseScaling = DamageScale;

	TakeDamage
	(
		BaseDamage * DamageScale,
		InstigatedBy,
		HitLocation,
		(DamageScale * Momentum * Normal(dir)),
		DamageType,
		HitInfo,
		DamageCauser
	);
	RadialImpulseScaling = 1.0;
	if (Health > 0)
	{
		DriverRadiusDamage(BaseDamage, DamageRadius, InstigatedBy, DamageType, Momentum, HurtOrigin, DamageCauser);
	}
}

/**
 *	Utility for switching the vehicle from a single body to an articulated ragdoll-like one given a new mesh and physics asset.
 *	ActorMove is an extra translation applied to Actor during the transition, which can be useful to avoid ragdoll mesh penetrating into the ground.
 */
native function InitVehicleRagdoll( SkeletalMesh RagdollMesh, PhysicsAsset RagdollPhysAsset, vector ActorMove, bool bClearAnimTree );

function AddVelocity( vector NewVelocity, vector HitLocation,class<DamageType> DamageType, optional TraceHitInfo HitInfo )
{
	if (!IsZero(NewVelocity))
	{
		NewVelocity = RadialImpulseScaling * MomentumMult * DamageType.Default.VehicleMomentumScaling * DamageType.Default.KDamageImpulse * Normal(NewVelocity);
		if (!bIgnoreForces && !IsZero(NewVelocity))
		{
			if (Location.Z > WorldInfo.StallZ)
			{
				NewVelocity.Z = FMin(NewVelocity.Z, 0);
			}
			if (InGodMode())
			{
				NewVelocity *= 0.25;
			}
			Mesh.AddImpulse(NewVelocity, HitLocation);
		}
	}
	RadialImpulseScaling = 1.0;
}

function bool Died(Controller Killer, class<DamageType> DamageType, vector HitLocation)
{
	if ( Super.Died(Killer, DamageType, HitLocation) )
	{
		bDriving = false;
		AddVelocity(TearOffMomentum, HitLocation, DamageType);
		return true;
	}
	return false;
}

/**
 *	Calculate camera view point, when viewing this actor.
 *
 * @param	fDeltaTime	delta time seconds since last update
 * @param	out_CamLoc	Camera Location
 * @param	out_CamRot	Camera Rotation
 * @param	out_FOV		Field of View
 *
 * @return	true if Pawn should provide the camera point of view.
 */
simulated function bool CalcCamera( float fDeltaTime, out vector out_CamLoc, out rotator out_CamRot, out float out_FOV )
{
	local vector	Pos, HitLocation, HitNormal;

	// Simple third person view implementation
	GetActorEyesViewPoint( out_CamLoc, out_CamRot );

	out_CamLoc += BaseOffset;
	Pos = out_CamLoc - Vector(out_CamRot) * CamDist;
	if( Trace(HitLocation, HitNormal, Pos, out_CamLoc, false, vect(0,0,0)) != None )
	{
		out_CamLoc = HitLocation + HitNormal*2;
	}
	else
	{
		out_CamLoc = Pos;
	}

	return true;
}

simulated function name GetDefaultCameraMode( PlayerController RequestedBy )
{
	return 'Default';
}

function bool TryToDrive(Pawn P)
{
	// Does the vehicle need to be uprighted?
	if ( bIsInverted && !bVehicleOnGround && VSize(Velocity) <= 0.1f )
	{
		if ( bCanFlip )
		{
			bIsUprighting = true;
			UprightStartTime = WorldInfo.TimeSeconds;
		}
		return false;
	}

	return Super.TryToDrive(P);
}

/** HasWheelsOnGround()
returns true if any of vehicles wheels are currently in contact with the ground (wheel has bWheelOnGround==true)
*/
simulated native function bool HasWheelsOnGround();

/** turns on the engine sound */
simulated function StartEngineSound()
{
	if (EngineSound != None)
	{
		EngineSound.Play();
	}
	ClearTimer('StartEngineSound');
	ClearTimer('StopEngineSound');
}

/** starts a timer of EngineStartOffsetSecs duration to turn on the engine sound */
simulated function StartEngineSoundTimed()
{
	if (EngineStartOffsetSecs > 0.f)
	{
		ClearTimer('StopEngineSound');
		SetTimer( EngineStartOffsetSecs, false, nameof(StartEngineSound) );
	}
	else
	{
		StartEngineSound();
	}
}

/** turns off the engine sound */
simulated function StopEngineSound()
{
	if (EngineSound != None)
	{
		EngineSound.Stop();
	}
	ClearTimer('StartEngineSound');
	ClearTimer('StopEngineSound');
}

/** starts a timer of EngineStopOffsetSecs duration to turn off the engine sound */
simulated function StopEngineSoundTimed()
{
	if (EngineStopOffsetSecs > 0.f)
	{
		ClearTimer('StartEngineSound');
		SetTimer( EngineStopOffsetSecs, false, nameof(StopEngineSound) );
	}
	else
	{
		StopEngineSound();
	}
}

simulated function VehiclePlayEnterSound()
{
	// For efficiency we're removing this fix and replacing the sounds with cues instead of components.
	// Deactivate any engine stopping sounds that were happening.

	// Trigger the engine starting sound.
	if (EnterVehicleSound != None)
	{
		PlaySound(EnterVehicleSound);
	}
	StartEngineSoundTimed();

}

simulated function VehiclePlayExitSound()
{
	// For efficiency we're removing this fix and replacing the sounds with cues instead of components.
	// Deactivate any engine starting sounds that were happening.

	// Trigger the engine stopping sound.
	if (ExitVehicleSound != None)
	{
		PlaySound(ExitVehicleSound);
	}
	StopEngineSoundTimed(); 
}

simulated function DrivingStatusChanged()
{
	// turn parking friction on or off
	bUpdateWheelShapes = true;

	if ( bDriving )
	{
		VehiclePlayEnterSound();
	}
	else if ( Health > 0 )
	{
		VehiclePlayExitSound();
	}
}

simulated event RigidBodyCollision( PrimitiveComponent HitComponent, PrimitiveComponent OtherComponent,
					const out CollisionImpactData RigidCollisionData, int ContactIndex )
{
	if( CollisionSound != None && WorldInfo.TimeSeconds - LastCollisionSoundTime > CollisionIntervalSecs )
	{
		PlaySound(CollisionSound, true);
		LastCollisionSoundTime = WorldInfo.TimeSeconds;
	}
}

/** called when the suspension moves a large amount, passes the delta*/
simulated event SuspensionHeavyShift(float Delta);

function PostTeleport(Teleporter OutTeleporter)
{
	Mesh.SetRBPosition(Location);
}

simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local Array<String>	DebugInfo;
	local int			i;

	super.DisplayDebug(HUD, out_YL, out_YPOS);

	GetSVehicleDebug( DebugInfo );

	Hud.Canvas.SetDrawColor(0,255,0);
	for (i=0;i<DebugInfo.Length;i++)
	{
		Hud.Canvas.DrawText( "  " @ DebugInfo[i] );
		out_YPos += out_YL;
		Hud.Canvas.SetPos(4, out_YPos);
	}

    // Uncomment to see detailed per-wheel debug info
	DisplayWheelsDebug(HUD, out_YL);
}

/**
 * Special debug information for the wheels that is displayed at each wheel's location
 */
simulated function DisplayWheelsDebug(HUD HUD, float YL)
{
    local int i, j;
    local vector WorldLoc, ScreenLoc, X, Y, Z, EndPoint, ScreenEndPoint;
    local Color SaveColor;
    local float LastForceValue;
    local float GraphScale;
    local float ForceValue;
    local vector ForceValueLoc;

    if (SimObj == None)
	{
		return;
	}

    GraphScale = 100.0f;
    SaveColor = HUD.Canvas.DrawColor;

	for (i=0; i<Wheels.Length; i++)
	{
    	GetAxes(Rotation, X, Y, Z);
    	WorldLoc =  Location + (Wheels[i].WheelPosition >> Rotation);
    	ScreenLoc = HUD.Canvas.Project(WorldLoc);
    	
		if( ScreenLoc.X >= 0 &&	ScreenLoc.X < HUD.Canvas.ClipX && ScreenLoc.Y >= 0 && ScreenLoc.Y < HUD.Canvas.ClipY )
		{
			// Draw Text
			HUD.Canvas.DrawColor = MakeColor(255,255,0,255);
			HUD.Canvas.SetPos(ScreenLoc.X, ScreenLoc.Y);
			HUD.Canvas.DrawText("Force "$Wheels[i].ContactForce);
			HUD.Canvas.SetPos(ScreenLoc.X, ScreenLoc.Y - (1 * YL));
			HUD.Canvas.DrawText("SR "$Wheels[i].LongSlipRatio);
			HUD.Canvas.SetPos(ScreenLoc.X, ScreenLoc.Y - (2 * YL));
			HUD.Canvas.DrawText("SA "$Wheels[i].LatSlipAngle * RadToDeg$" ("$Wheels[i].LatSlipAngle$")");
   			HUD.Canvas.SetPos(ScreenLoc.X, ScreenLoc.Y - (3 * YL));
  			HUD.Canvas.DrawText("Torque "$Wheels[i].MotorTorque);
			HUD.Canvas.SetPos(ScreenLoc.X, ScreenLoc.Y - (4 * YL));
			HUD.Canvas.DrawText("SpinVel "$Wheels[i].SpinVel);

			// Draw Lines
			HUD.Canvas.DrawColor = HUD.RedColor;
			EndPoint = WorldLoc + (Wheels[i].LongImpulse * 100 * Wheels[i].LongDirection) - (Wheels[i].WheelRadius * Z);
			ScreenEndPoint = HUD.Canvas.Project(EndPoint);
			DrawDebugLine(WorldLoc - (Wheels[i].WheelRadius * Z), EndPoint, 255, 0, 0);
			HUD.Canvas.SetPos(ScreenEndPoint.X, ScreenEndPoint.Y);
			HUD.Canvas.DrawText(Wheels[i].LongImpulse);

			HUD.Canvas.DrawColor = HUD.GreenColor;
			EndPoint = WorldLoc + (Wheels[i].LatImpulse * 100 * Wheels[i].LatDirection) - (Wheels[i].WheelRadius * Z);
			ScreenEndPoint = HUD.Canvas.Project(EndPoint);
			DrawDebugLine(WorldLoc - (Wheels[i].WheelRadius * Z), EndPoint, 0, 255, 0);
			HUD.Canvas.SetPos(ScreenEndPoint.X, ScreenEndPoint.Y);
			HUD.Canvas.DrawText(Wheels[i].LatImpulse);
			HUD.Canvas.SetPos(ScreenLoc.X, ScreenLoc.Y + YL);
			HUD.Canvas.DrawText(Wheels[i].LatImpulse);

			// Draw Axes
			HUD.Canvas.DrawColor = MakeColor(255,255,255,255);
			HUD.Draw2DLine(ScreenLoc.X, ScreenLoc.Y, ScreenLoc.X + GraphScale, ScreenLoc.Y, MakeColor(0,0,255,255));
			HUD.Canvas.SetPos(ScreenLoc.X + GraphScale, ScreenLoc.Y);
			HUD.Canvas.DrawText(PI * 0.5f);
			HUD.Draw2DLine(ScreenLoc.X, ScreenLoc.Y, ScreenLoc.X, ScreenLoc.Y - GraphScale, MakeColor(0,0,255,255));
			HUD.Canvas.SetPos(ScreenLoc.X, ScreenLoc.Y - GraphScale);
			HUD.Canvas.DrawText(SimObj.WheelLatExtremumValue);

			// Draw Graph
			LastForceValue = 0.0f;
			for (j=0; j<=GraphScale; j++)
			{
				ForceValue = HermiteEval(j * ((PI * 0.5f) / GraphScale));
				ForceValue = (ForceValue / SimObj.WheelLatExtremumValue) * GraphScale;
				HUD.Draw2DLine(ScreenLoc.X + (j - 1), ScreenLoc.Y - LastForceValue, ScreenLoc.X + j, ScreenLoc.Y - ForceValue, MakeColor(0,255,0,255));
				LastForceValue = ForceValue;
			}

			// Draw Force Value
			ForceValue = HermiteEval(Abs(Wheels[i].LatSlipAngle));
			ForceValueLoc.X = ScreenLoc.X + (Abs(Wheels[i].LatSlipAngle) / (PI * 0.5f)) * GraphScale;
			ForceValueLoc.Y = ScreenLoc.Y - (ForceValue / SimObj.WheelLatExtremumValue) * GraphScale;
			HUD.Draw2DLine(ForceValueLoc.X - 5, ForceValueLoc.Y, ForceValueLoc.X + 5, ForceValueLoc.Y, MakeColor(255,0,0,255));
			HUD.Draw2DLine(ForceValueLoc.X, ForceValueLoc.Y - 5, ForceValueLoc.X, ForceValueLoc.Y + 5, MakeColor(255,0,0,255));
			HUD.Canvas.SetPos(ScreenLoc.X, ForceValueLoc.Y);
			HUD.Canvas.DrawText(ForceValue);
			HUD.Canvas.SetPos(ForceValueLoc.X, ScreenLoc.Y + YL);
			HUD.Canvas.DrawText(Wheels[i].LatSlipAngle);
		}
	}

    HUD.Canvas.DrawColor = SaveColor;
}

/**
 *  Hermite spline evaluation for use with the slip curve debug display
 */
simulated function float HermiteEval(float Slip)
{
    local float LatExtremumSlip;
    local float LatExtremumValue;
    local float LatAsymptoteSlip;
    local float LatAsymptoteValue;
    local float SlipSquared, SlipCubed;
    local float C0, C1, C3;

    LatExtremumSlip = SimObj.WheelLatExtremumSlip;
    LatExtremumValue = SimObj.WheelLatExtremumValue;
    LatAsymptoteSlip = SimObj.WheelLatAsymptoteSlip;
    LatAsymptoteValue = SimObj.WheelLatAsymptoteValue;

    if (Slip < LatExtremumSlip)
    {
	Slip /= LatExtremumSlip;
	SlipSquared = Slip * Slip;
	SlipCubed = SlipSquared * Slip;
	C3 = -2.0f * SlipCubed + 3.0f * SlipSquared;
	C1 = SlipCubed - 2.0f * SlipSquared + Slip;
	return ((C1 + C3) * LatExtremumValue);
    }
    else if (Slip > LatAsymptoteSlip)
    {
	return (LatAsymptoteValue);
    }
    else
    {
	Slip /= (LatAsymptoteSlip - LatExtremumSlip);
	Slip -= LatExtremumSlip;
	SlipSquared = Slip * Slip;
	SlipCubed = SlipSquared * Slip;
	C3 = -2.0f * SlipCubed + 3.0f * SlipSquared;
	C0 = 2.0f * SlipCubed - 3.0f * SlipSquared + 1.0f;
	return (C0 * LatExtremumValue + C3 * LatAsymptoteValue);
    }
}

/**
 * Retrieves important SVehicle debug information as an array of strings. That can then be dumped or displayed on HUD.
 */
simulated function GetSVehicleDebug( out Array<String> DebugInfo )
{
	DebugInfo[DebugInfo.Length] = "----Vehicle----: ";
	DebugInfo[DebugInfo.Length] = "Speed: "$VSize(Velocity)$" Unreal -- "$VSize(Velocity) * 0.0426125$" MPH";
	if (Wheels.length > 0)
	{
		DebugInfo[DebugInfo.Length] = "MotorTorque: "$Wheels[0].MotorTorque;
	}
	DebugInfo[DebugInfo.Length] = "Throttle: "$OutputGas;
	DebugInfo[DebugInfo.Length] = "Brake: "$OutputBrake;
}

defaultproperties
{
	TickGroup=TG_PostAsyncWork

	Begin Object Class=SkeletalMeshComponent Name=SVehicleMesh
		RBChannel=RBCC_Vehicle
		RBCollideWithChannels=(Default=TRUE,BlockingVolume=TRUE,GameplayPhysics=TRUE,EffectPhysics=TRUE,Vehicle=TRUE)
		BlockActors=true
		BlockZeroExtent=true
		BlockRigidBody=true
		BlockNonzeroExtent=true
		CollideActors=true
		bForceDiscardRootMotion=true
		bUseSingleBodyPhysics=1
		bNotifyRigidBodyCollision=true
		ScriptRigidBodyCollisionThreshold=250.0
	End Object
	CollisionComponent=SVehicleMesh
	Mesh=SVehicleMesh
	Components.Add(SVehicleMesh)

	BaseOffset=(Z=128)
	CamDist=512

	Physics=PHYS_RigidBody
	bEdShouldSnap=true
	bStatic=false
	bCollideActors=true
	bCollideWorld=true
	bProjTarget=true
	bBlockActors=true
	bWorldGeometry=false
	bCanBeBaseForPawns=true
	bAlwaysRelevant=false
	RemoteRole=ROLE_SimulatedProxy
	bNetInitialRotation=true
	bBlocksTeleport=TRUE

	// Stay-upright constraint
	Begin Object Class=RB_StayUprightSetup Name=MyStayUprightSetup
	End Object
	StayUprightConstraintSetup=MyStayUprightSetup

	Begin Object Class=RB_ConstraintInstance Name=MyStayUprightConstraintInstance
	End Object
	StayUprightConstraintInstance=MyStayUprightConstraintInstance

	// Absolute max physics speed
	MaxSpeed=2500

	// Absolute max physics angular velocity (Unreal angular units)
	MaxAngularVelocity=75000.0

	// Inertia Tensor
	InertiaTensorMultiplier=(x=1.0,y=1.0,z=1.0)

	// Initialize uprighting parameters.
	bCanFlip=true
	UprightLiftStrength = 225.0;
	UprightTorqueStrength = 50.0;
	UprightTime = 1.5;
	bIsUprighting = false;

	// Initialize sound members.
	SquealThreshold = 250.0;
	SquealLatThreshold=250.0f;
	LatAngleVolumeMult=1.0f;
	EngineStartOffsetSecs = 2.0;
	EngineStopOffsetSecs = 1.0;
	HeavySuspensionShiftPercent=0.5f;

	RadialImpulseScaling=1.0
}
