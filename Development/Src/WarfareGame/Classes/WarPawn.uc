class WarPawn extends Pawn
	config(Game)
	native
	abstract;

/**
 * Represents the current evade direciton, mainly for animation
 * polling purposes.
 */
enum EEvadeDirection
{
	/** Default, no evade */
	ED_None,
	/** Forward */
	ED_Forward,
	/** Backward */
	ED_Backward,
	/** Left */
	ED_Left,
	/** Right */
	ED_Right,
};
var EEvadeDirection EvadeDirection;

/** Constant offset applied to mesh transform when using cover */
var const vector CoverTranslationOffset;

/** Array of movement scales to set when in the matching CoverType */
var array<float> CoverMovementPct;

/** Represents the type of cover currently being used, for animation */
var CoverNode.ECoverType CoverType;

/**
 * Represents the current action this pawn is performing at the
 * current cover node.
 */
var CoverNode.ECoverAction CoverAction;

var bool	bIsSprinting;		// currently sprinting (can't strafe as well, faster movement)

var float	SprintingPct;	// pct. of running speed that sprinting speed is
var float	MovementPct;	// Global modifier that effects all movement modes

/** Default inventory added via AddDefaultInventory() */
var config array<class<Inventory> >	DefaultInventory;

/** @deprecated - used to determine what type of voice to use */
var string VoiceType;

var int spree;

/** hack for greenlight deathanims */
var		bool	bPlayDeathAnim;

var()	config	float	ShieldAmount;
var()	config	float	ShieldRechargeRate;
var()	config	float	ShieldRechargeDelay;
var				float	LastShieldHit;

/**
 * structure defining a group (body part) for locational damage.
 * Allows body specific properties, like damage modifier and effects.
 */
struct native	LocDmgStruct
{
	/** 
	 * List of bone names in skeletal mesh that define this body zone.
	 * Note: only list bones where RB_BodySetup are attached to (in PhAT).
	 * Please refer to the RB_BodySetup and PhAT to set these up.
	 */
	var()	Name			BoneNameArray[4];

	/** Name of body part, for display and debugging */
	var() localized string	BodyPartName;

	/** Damage is multiplied by this scalar. For example 2x for head shots */
	var() float				fDamageMultiplier;

	structdefaults
	{
		fDamageMultiplier=1.f
	}
};
var()	Array<LocDmgStruct>	LocDmgArray;

/** Character sound group class */
var()	class<WarSoundGroup> SoundGroupClass;

//
// Cash / bonus / rewards
//

/** Cash awarded for killing this Pawn */
var()	config	float	KillReward;
/** Extra cash awarded if enemy killed fast (ie HeadShot) */
var()	config	float	FastKillBonus;
/** True if this is an Elite (higher ranking) character. Then the following EliteKillBonus will be awarded when killed */
var()	config	bool	bAwardEliteKillBonus;
/** extra cash awarded, because this Pawn was higher ranking */
var()	config	float	EliteKillBonus;

/** Are we currently jumping down a ledge? */
var() transient bool bJumpingDownLedge;

/** Max distance to check downwards for a jumpable ledge */
var() const config float MaxJumpDownDistance;

/** Min distance to allow a jump down */
var() const config float MinJumpDownDistance;

var JumpPoint JumpPoint;

/** 
 * FOV at which a player is considered safe behind cover.
 * Enemy shots with a direction within that FOV will be ignored
 */
var()	config	vector2d	CoverProtectionFOV;

cpptext
{
	virtual FLOAT MaxSpeedModifier();
    FVector CheckForLedges(FVector AccelDir, FVector Delta, FVector GravDir, int &bCheckedFall, int &bMustJump );
}

replication
{
	reliable if( bNetDirty && (Role==ROLE_Authority) )
        bIsSprinting;

	reliable if( bNetDirty && bNetOwner && Role==ROLE_Authority )
		ShieldAmount;

	unreliable if ( (Role==ROLE_Authority) && !bNetOwner && bNetDirty )
        CoverType, EvadeDirection;

	unreliable if ( (Role==ROLE_Authority) && !bNetOwner && bNetDirty && CoverType != CT_None )
		CoverAction;
}

function Restart()
{
	super.Restart();

	SetCoverType( CT_None );
	SetCoverAction( CA_Default );
}

simulated function ClientRestart()
{
	super.ClientRestart();

	SetCoverType( CT_None );
	SetCoverAction( CA_Default );
}

/** debug function to get all body setup bone names, for locational damage */
exec function LogBodySetupBoneNames()
{
	local int			i, max;
	local PhysicsAsset	PA;

	PA = Mesh.PhysicsAsset;
	max = PA.BodySetup.Length;
	log("logging BodySetup bone names!");
	for (i=0; i<max; i++)
	{
		log("BoneName:" @ PA.BodySetup[i].BoneName );
	}
}

/**
 * Checks if a locational damage on this character registers.
 * The weapon passes the HitInfo properties which contains a BoneName if it hit a SkeletalMeshComponent.
 * This is used to retrieve body part properties returned within the LocDmgStruct.
 * @NOTE	Quick test. This should probably be consolidated within RB_BodySetup?
 *
 * @param	HitInfo, Trace Hit Info returned when the SkeletalMeshComponent was hit, containing BoneName.
 * @output	out_LocDmgInfo, locational damage info about the bodygroup that was hit. To be used by weapon for postprocesssing.
 * @return	true if the hit registers as a locational hit. (body group info could be retrieved).
 */
function bool RegisterLocDmg( out TraceHitInfo HitInfo, out LocDmgStruct out_LocDmgInfo )
{
	local int	BodyGroup, BoneNB;

	for (BodyGroup=0; BodyGroup<LocDmgArray.Length; BodyGroup++)
	{
		for (BoneNB=0; BoneNB<4; BoneNB++)
		{
			if( LocDmgArray[BodyGroup].BoneNameArray[BoneNB] == HitInfo.BoneName )
			{
				out_LocDmgInfo = LocDmgArray[BodyGroup];
				return true;
			}
		}
	}
	return false;
}

function bool Dodge(eDoubleClickDir DoubleClickMove)
{
	return false;
}

/** Debug locational damage, output to log file */
function DebugLocDamage( Pawn InstigatedBy, LocDmgStruct LocDmgInfo )
{
	local String Msg;

	//log("WarPawn::DebugLocDamage LocDamage:" @ LocDmgInfo.BodyPartName );
	Msg = "BodyPart:" @ LocDmgInfo.BodyPartName @ "mult:" $ LocDmgInfo.fDamageMultiplier $ "x";

	if( InstigatedBy != None && PlayerController(InstigatedBy.Controller) != None )
	{
		PlayerController(InstigatedBy.Controller).ClientMessage( "Gave Damage" @ Msg, 'Event' );
	}

	if( PlayerController(Controller) != None )
	{
		PlayerController(Controller).ClientMessage( "Received Damage" @ Msg, 'Event' );
	}
}

/** 
 * returns true if player is safe from cover 
 * This function decides if a shot should be ignored because the player is considered safely protected by its current cover.
 */
final function bool IsProtectedByCover( vector ShotDirection )
{
	local vector2D	AngularDist;
	local vector	AxisX, AxisY, AxisZ;
	local bool		bIsInFront;

	// Make sure player has valid cover
	// This CoverProtection FOV is not applied to AI characters.
	if( CoverType == CT_None ||
		CoverType == CT_Crouching ||
		PlayerController(Controller) == None )
	{
		return false;
	}

	// If Pawn is leaning, then cover is not safe
	if( CoverAction == CA_LeanLeft ||
		CoverAction == CA_LeanRight ||
		CoverAction == CA_StepLeft ||
		CoverAction == CA_StepRight ||
		CoverAction == CA_PopUp )
	{
		return false;
	}

	GetAxes(Rotation, AxisX, AxisY, AxisZ);
	ShotDirection = -1.f * Normal(ShotDirection);
	bIsInFront = GetAngularDistance(AngularDist, ShotDirection, AxisX, AxisY, AxisZ );
	GetAngularDegreesFromRadians( AngularDist );
	
	// check shot's angle of attack against cover's orientation
	if( bIsInFront &&
		Abs(AngularDist.X) < CoverProtectionFOV.X &&
		Abs(AngularDist.Y) < CoverProtectionFOV.Y )
	{
		return true;
	}
	
	return false;
}

simulated function Tick( float DeltaTime )
{
	super.Tick( DeltaTime );

	// Recharge Shield
	if( Role == Role_Authority &&
		Health > 0 &&
		default.ShieldAmount > 0 &&
		ShieldAmount < default.ShieldAmount &&
		Level.TimeSeconds > LastShieldHit + ShieldRechargeDelay )
	{
		ShieldAmount += DeltaTime * default.ShieldAmount * ShieldRechargeRate;

		if( ShieldAmount > default.ShieldAmount )
		{
			ShieldAmount = default.ShieldAmount;
		}
	}

	// draw debug cover protection FOV
	if ( (WarPC(Controller) != None) && WarPC(Controller).bDebugCover && (LocalPlayer(PlayerController(Controller).Player) != None) )
	{
		WarPlayerInput(WarPC(Controller).PlayerInput).DebugDrawFOV(CoverProtectionFOV, Location, Rotation, MakeColor(128,255,64,255), 1024 );
	}
}

/** 
 * Adjust Pawn Damage
 * - Locational Damage
 * - adjust based on cover (makes cover safe)
 */
function AdjustPawnDamage
( 
				out	int					Damage,
				out	LocDmgStruct		LocDmgInfo,
					Pawn				instigatedBy, 
					Vector				hitlocation,
					Vector				momentum, 
					class<DamageType>	damageType, 
	optional	out	TraceHitInfo		HitInfo 
)
{
	if( IsProtectedByCover(Momentum) )
	{
		Damage = 0;
		return;
	}

	//DumpDamageInfo(Damage, LocDmgInfo, instigatedBy, hitlocation, momentum, damageType, HitInfo);

	// Make sure we have a valid TraceHitInfo with our SkeletalMesh
	// we need a bone to apply proper impulse
	CheckHitInfo( HitInfo, Mesh, Normal(Momentum), hitlocation );

	// Locational Damage
	// Check if we hit a SkeletalMeshComponent, and if we can retrieve locational damage info
	if( SkeletalMeshComponent(HitInfo.HitComponent) != None )
	{
		if( HitInfo.BoneName != '' )
		{
			// Locational Damage
			if( RegisterLocDmg(HitInfo, LocDmgInfo) )
			{
				// Damage multiplier
				Damage *= LocDmgInfo.fDamageMultiplier;
				//DebugLocDamage( InstigatedBy, LocDmgInfo );
			}
			else
			{
				Warn("Locational Damage info not found for bone:" @ HitInfo.BoneName );
			}
		}
		else
		{
			Warn("No bone passed on hit... cannot do locational damage");
		}
	}
	else
	{
		Warn("No skeletalmesh component hit... cannot do locational damage");
	}

	// Driven Vehicle
	if( DrivenVehicle != None )
	{
        DrivenVehicle.AdjustDriverDamage( Damage, InstigatedBy, HitLocation, Momentum, DamageType );
	}
}

/** Debug function, dumps damage info to log file */
function DumpDamageInfo
( 
				out	int					Damage,
				out	LocDmgStruct		LocDmgInfo,
					Pawn				instigatedBy, 
					Vector				hitlocation,
					Vector				momentum, 
					class<DamageType>	damageType, 
	optional	out	TraceHitInfo		HitInfo 
)
{
	log("++ DumpDamageInfo ++");
	log("instigatedBy:" @ instigatedBy @ "victim:" @ self );
	log("damageType:" @ damageType );
	log("momentum:" @ momentum );
}

/* @see Actor::TakeDamage */
function TakeDamage
( 
				int					Damage, 
				Pawn				instigatedBy, 
				Vector				hitlocation,
				Vector				momentum, 
				class<DamageType>	damageType, 
	optional	TraceHitInfo		HitInfo 
)
{
	local int			actualDamage;
	local bool			bAlreadyDead;
	local Controller	Killer;
	local LocDmgStruct	LocDmgInfo;

	if ( damagetype == None )
	{
		warn("No damagetype for damage by "$instigatedby$" with weapon "$InstigatedBy.Weapon );
		damageType = class'DamageType';
	}

	if ( Role < ROLE_Authority )
	{
		log(self$" client damage type "$damageType$" by "$instigatedBy);
		return;
	}

	bAlreadyDead = (Health <= 0);

	if ( bAlreadyDead )
	{
		log("TakeDamage, bAlreadyDead" @ Self @ "Health:" $ Health @ "InstigatedBy:" $ InstigatedBy @ "damage:" $ Damage @ "DamageType:" $ DamageType );
		log("Controller:" $ Controller @ "State:" @ Controller.GetStateName() );
		return;
	}

	if ( (Physics == PHYS_None) && (DrivenVehicle == None) )
		SetMovementPhysics();

	//if (Physics == PHYS_Walking)
	//	momentum.Z = FMax(momentum.Z, 0.4 * VSize(momentum));
	if ( (instigatedBy == self)
		|| ((Controller != None) && (InstigatedBy != None) && (InstigatedBy.Controller != None) && Level.GRI.OnSameTeam(InstigatedBy.Controller,Controller)) )
		momentum *= 0.6;
	momentum = momentum/Mass;

	// adjust damage based on various stuffs (cover, locational damage...)
	AdjustPawnDamage( Damage, LocDmgInfo, instigatedBy, hitlocation, momentum, damageType, HitInfo );

	ActualDamage = Damage;
	Level.Game.ReduceDamage(ActualDamage, self, instigatedBy, HitLocation, Momentum, DamageType);
	
	// Shield
	if( ShieldAmount > 0 && actualDamage > 0 )
	{
		LastShieldHit = Level.TimeSeconds;
		if( ShieldAmount >= actualDamage )
		{
			ShieldAmount -= actualDamage;
			actualDamage = 0;
		}
		else
		{
			actualDamage -= ShieldAmount;
			ShieldAmount = 0;
		}
	}
	// call Actor's version to handle any SeqEvent_TakeDamage for scripting
	Super(Actor).TakeDamage(actualDamage,instigatedBy,hitlocation,momentum,damageType,HitInfo);

	Health -= actualDamage;
	if ( HitLocation == vect(0,0,0) )
	{
		HitLocation = Location;
	}

	if ( bAlreadyDead )
	{
		Warn(self$" took regular damage "$damagetype$" from "$instigatedby$" while already dead at "$Level.TimeSeconds);
		ChunkUp(Rotation, DamageType);
		return;
	}

	// Notify instigator controller of given damage
	if( InstigatedBy != None && WarPC(InstigatedBy.Controller) != None )
	{
		WarPC(InstigatedBy.Controller).NotifyGiveDamage( Self, DamageType, LocDmgInfo );
	}

	PlayHit(actualDamage, InstigatedBy, hitLocation, damageType, Momentum);

	if ( Health <= 0 )
	{
		// pawn died
		if ( instigatedBy != None )
		{
			Killer = instigatedBy.GetKillerController();
		}
		TearOffMomentum = momentum;
		Died(Killer, damageType, HitLocation);
	}
	else
	{
		if ( (InstigatedBy != None) && (InstigatedBy != self) && (Controller != None)
			&& (InstigatedBy.Controller != None) && Level.GRI.OnSameTeam(InstigatedBy.Controller,Controller) )
			Momentum *= 0.5;

		AddVelocity( momentum );
		if ( Controller != None )
			Controller.NotifyTakeHit(instigatedBy, HitLocation, actualDamage, DamageType, Momentum);
	}
	MakeNoise(1.0);
}

function PlayHit(float Damage, Pawn InstigatedBy, vector HitLocation, class<DamageType> damageType, vector Momentum)
{
	local vector HitNormal;

	if ( (Damage <= 0) && ((Controller == None) || !Controller.bGodMode) )
	{
		return;
	}

	if ( Damage > 0 ) //spawn some blood
	{
		HitNormal = Normal(HitLocation - Location);

		// Play any set effect
		if ( EffectIsRelevant(Location, true) )
		{
			Spawn( class'Emit_BloodSpray',,, HitLocation + HitNormal*3, rotator(HitNormal) );
		}
	}
	if ( Health <= 0 )
	{
		if ( PhysicsVolume.bDestructive && (WaterVolume(PhysicsVolume) != None) && (WaterVolume(PhysicsVolume).ExitActor != None) )
		{
			Spawn(WaterVolume(PhysicsVolume).ExitActor);
		}
		return;
	}

	if ( Level.TimeSeconds - LastPainTime > 0.1 )
	{
		PlayTakeHit(HitLocation, Damage, damageType);
		LastPainTime = Level.TimeSeconds;
	}
}

function PlayTakeHit(vector HitLoc, int Damage, class<DamageType> damageType)
{
	if ( Level.NetMode != NM_DedicatedServer )
	{
		SoundGroupClass.static.PlayTakeHitSound( Self );
	}
}

/**
 * Responsible for playing any death effects, animations, etc.
 * 
 * @param 	DamageType - type of damage responsible for this pawn's death
 * 
 * @param	HitLoc - location of the final shot
 */
simulated function PlayDying(class<DamageType> DamageType, vector HitLoc)
{
	local Vector		ApplyImpulse;
	local TraceHitInfo	HitInfo;

    bCanTeleport		= false;
    bReplicateMovement	= false;
    bTearOff			= true;
    bPlayedDeath		= true;

	// these are replicated to other clients
	HitDamageType		= DamageType; 
    TakeHitLocation		= HitLoc;

	// greenlight hack... death anims instead of ragdolls on Xenon.
	if( Level.IsConsoleBuild() )
	{
		GreenlightDeathAnims();
		return;
	}

	if ( InitRagdoll() )
	{
		CheckHitInfo( HitInfo, Mesh, Normal(TearOffMomentum), TakeHitLocation );
		ApplyImpulse	= Normal(TearOffMomentum) * DamageType.default.KDamageImpulse + Vect(0,0,1) * DamageType.default.KDeathUpKick;

		Mesh.AddImpulse(ApplyImpulse, TakeHitLocation, HitInfo.BoneName);
		GotoState('Dying');
	}
	else
	{
		warn("PlayDying Ragdoll init failed");
		destroy();
	}
}

function GreenlightDeathAnims()
{
	local float	RandomizeThis;
	local AnimNodeSequence DeathNode;

	// play death anims
	bPlayDeathAnim = true;
	
	// set random anim... 
	// @todo make it fancy and select anim based on tear off momentum direction!
	DeathNode = AnimNodeSequence( Mesh.Animations.FindAnimNode('DeathNode') );
	if(DeathNode != None)
	{
		RandomizeThis = FRand();
		if( RandomizeThis < 0.25 )
		{
			DeathNode.SetAnim('DtBd01');
		}
		else if( RandomizeThis < 0.50 )
		{
			DeathNode.SetAnim('DtFd01');
		}
		else if( RandomizeThis < 0.75 )
		{
			DeathNode.SetAnim('DtRt01');
		}
		else
		{
			DeathNode.SetAnim('DtLt01');
		}
	}

	GotoState('Dying');
}

function PlayDyingSound()
{
	if ( Level.NetMode != NM_DedicatedServer )
	{
		SoundGroupClass.static.PlayDyingSound( Self );
	}

}

State Dying
{
ignores AnimEnd, Bump, HitWall, HeadVolumeChange, PhysicsVolumeChange, Falling, BreathTimer;

	/**
	 *	Calculate camera view point, when viewing this pawn.
	 *
	 * @param	fDeltaTime	delta time seconds since last update
	 * @param	out_CamLoc	Camera Location
	 * @param	out_CamRot	Camera Rotation
	 * @param	out_FOV		Field of View
	 *
	 * @return	true if Pawn should provide the camera point of view.
	 */
	simulated function bool PawnCalcCamera( float fDeltaTime, out vector out_CamLoc, out rotator out_CamRot, out float out_FOV )
	{
		// place holder death view to see ragdolls.
		// fixed camera, but targeting the player's head.
		out_CamRot = Rotator(Mesh.GetBoneLocation( 'Neck' ) - out_CamLoc);
		return true;
	}

	function TakeDamage( int Damage, Pawn instigatedBy, Vector hitlocation,
						Vector momentum, class<DamageType> damageType, optional TraceHitInfo HitInfo);
}

/**
 * Handles setting the a new cover type for this pawn, for both
 * animation and physics purposes.
 * 
 * @param	newCoverType - new cover type now active
 */
simulated function SetCoverType(ECoverType newCoverType)
{
	if( WarPC(Controller) != None )
	{
		WarPC(Controller).CoverLog("newCoverType:"$newCoverType, "WarPawn::SetCoverType");
	}

	// Do nothing if state isn't changing
	if( NewCoverType != CoverType )
	{
		// and modify movement pct
		if( newCoverType == CT_None )
		{
			MeshTransform.SetTranslation(MeshTransform.Translation - CoverTranslationOffset);
			MovementPct = default.MovementPct;
		}
		else
		{
			if( CoverType == CT_None )
			{
				MeshTransform.SetTranslation(MeshTransform.Translation + CoverTranslationOffset);
			}
			MovementPct = CoverMovementPct[int(newCoverType)];
		}
		// set the enum for animation
		CoverType = newCoverType;
	}
}

/**
 * Handles setting the new cover action for this pawn
 * Including toggling on/off camera modifiers
 *
 * @param	NewCoverAction - new cover action type activated
 */
simulated function SetCoverAction(ECoverAction newCoverAction)
{
	if( WarPC(Controller) != None )
	{
		WarPC(Controller).CoverLog("newCoverAction:"$newCoverAction, "WarPawn::SetCoverAction");
	}

	// Do nothing if state isn't changing
	if( newCoverAction != CoverAction )
	{
		CoverAction = newCoverAction;
	}
}

/**
 * Overridden to return the actual player name from this Pawn's
 * PlayerReplicationInfo (PRI) if available.
 */
function String GetDebugName()
{
	// return the actual player name from the PRI if available
	if (PlayerReplicationInfo != None)
	{
		return PlayerReplicationInfo.PlayerName;
	}
	// otherwise return the formatted object name
	return GetItemName(string(self));
}

/**
 * Called every frame from PlayerInput or PlayerController::MoveAutonomous()
 * Sets bIsWalking flag, which defines if the Pawn is walking or not (affects velocity)
 *
 * @param	bNewIsWalking, new walking state.
 */
event SetWalking( bool bNewIsWalking )
{
	// Give Inventory Manager a chance to force or deny player walking.
	if ( WarInventoryManager(InvManager) != None )
		bNewIsWalking = WarInventoryManager(InvManager).ForceSetWalking( bNewIsWalking );

	super.SetWalking( bNewIsWalking );
}

/**
 * Responsible for playing any sounds and/or creating any effects.
 * 
 * @see		CheckBob 
 */
simulated function HandleFootstep()
{
}

/**
 * Overridden to check for local footsteps.
 */
function CheckBob(float DeltaTime, vector Y)
{
	local float OldBobTime;
	local int m,n;

	OldBobTime = BobTime;
	Super.CheckBob(DeltaTime,Y);

	if ( (Physics != PHYS_Walking) || (VSize(Velocity) < 10)
		|| ((PlayerController(Controller) != None) && PlayerController(Controller).PlayerCamera != None) )
		return;

	m = int(0.5 * Pi + 9.0 * OldBobTime/Pi);
	n = int(0.5 * Pi + 9.0 * BobTime/Pi);

	if ( (m != n) && !bIsWalking && !bIsCrouched )
	{
		HandleFootstep();
	}
}

/**
 * Called from PlayerController UpdateRotation() -> ProcessViewRotation() to (pre)process player ViewRotation
 * adds delta rot (player input), applies any limits and post-processing
 * returns the final ViewRotation set on PlayerController
 *
 * @param	DeltaTime, time since last frame
 * @param	ViewRotation, actual PlayerController view rotation
 * @input	out_DeltaRot, delta rotation to be applied on ViewRotation. Represents player's input.
 * @return	processed ViewRotation to be set on PlayerController.
 */
simulated function ProcessViewRotation( float DeltaTime, out rotator out_ViewRotation, out Rotator out_DeltaRot )
{
	// Give Inventory Manager a chance to affect player's view rotation
	if ( WarInventoryManager(InvManager) != None )
	{
		WarInventoryManager(InvManager).ProcessViewRotation( DeltaTime, out_ViewRotation, out_DeltaRot );
	}

	super.ProcessViewRotation( DeltaTime, out_ViewRotation, out_DeltaRot );
}

/**
 * Overridden to return camera values specific to WarPawn.
 * 
 * @param		RequestedBy
 */
simulated function name GetDefaultCameraMode( PlayerController RequestedBy )
{
	return 'default';
}

/**
 * Overridden to iterate through the DefaultInventory array and
 * give each item to this Pawn.
 * 
 * @see			GameInfo.AddDefaultInventory
 */
function AddDefaultInventory()
{
	local int		i;
	local Inventory	Inv;
	local WarPC wfPC;
	wfPC = WarPC(Controller);
	if (wfPC == None)
	{
		for (i=0; i<DefaultInventory.Length; i++)
		{
			// Ensure we don't give duplicate items
			if (FindInventoryType( DefaultInventory[i] ) == None)
			{
				Inv = CreateInventory( DefaultInventory[i] );
				if (Weapon(Inv) != None)
				{
					// don't allow default weapon to be thrown out
					Weapon(Inv).bCanThrow = false;
				}
			}
		}
	}
	else
	{
		CreateInventory(WarPC(Controller).SecondaryWeaponClass);
		CreateInventory(WarPC(Controller).PrimaryWeaponClass);
	}
}

/**
 * Overridden to notify camera that this Pawn has become a viewtarget.
 * 
 * @param		PC - the playercontroller that is now viewing this pawn
 */
event BecomeViewTarget( PlayerController PC )
{
	super.BecomeViewTarget( PC );
	//WarCam.bTeleportCamera = true;
}

/**
 * Overridden to notify camera of the teleport.
 * 
 * @param		bOut - true indicates that this is an arrival teleport
 * 
 * @param		bSound - should the teleport sound be played?
 */
function PlayTeleportEffect(bool bOut, bool bSound)
{
	super.PlayTeleportEffect(bOut, bSound);
	//WarCam.bTeleportCamera = true;
}

/**
 * Iterates through the list of item classes specified in the action
 * and creates instances that are addeed to this Pawn's inventory.
 * 
 * @param		inAction - scripted action that was activated
 */
simulated function OnGiveInventory(SeqAct_GiveInventory inAction)
{
	local int				idx;
	local class<Inventory>	invClass;

	if ( inAction.InventoryList.Length > 0 )
	{
		for (idx = 0; idx < inAction.InventoryList.Length; idx++)
		{
			invClass = inAction.InventoryList[idx];
			if ( invClass != None )
				CreateInventory( invClass );
			else
				Warn("Attempting to give NULL inventory!");
		}
	}
	else
	{
		Warn("Give inventory without any inventory specified!");
	}
}

/**
 * Perform a trace on and to crosshair.
 * the specified Range is calculated so it's relative to the pawn's location and not the camera location (since camera is variable).
 * @note	if HitActor == None, out_HitLocation == EndTrace
 *
 * @param	fRange				trace range from player pawn
 * @output	out_HitLocation		location of hit. returns EndTrace location if no hit.
 * @output	out_HitNormal		hit normal.
 * @param	TraceExtent			extent of trace
 *
 * @return	HitActor			returns hit actor.
 */
simulated function Actor TraceWithPawnOffset( float fRange, out vector out_HitLocation, out vector out_HitNormal, optional vector TraceExtent )
{
	local Vector	StartTrace, EndTrace;
	local Rotator	AimRot;
	local Actor		HitActor;

	// Get adjusted start trace postion. From camera world location + offset to range is constant from pawn.
	StartTrace	= GetWeaponStartTraceLocation();
	// Get base, non corrected, aim direction.
	AimRot		= GetBaseAimRotation();
	// define end trace
	EndTrace	= StartTrace + vector(AimRot) * fRange;
	// perform trace
	HitActor	= Trace(out_HitLocation, out_HitNormal, EndTrace, StartTrace, true, TraceExtent);

	// If we didn't hit anything, then set the HitLocation as being the EndTrace location
	if ( HitActor == None )
	{
		out_HitLocation = EndTrace;
	}
	return HitActor;
}

/**
 * returns true if melee attack can be successfully performed
 * used by HUD indicator, and USE action prioritization
 */
simulated function bool SuggestMeleeAttack()
{
	local vector	out_HitLocation, out_HitNormal;
	local Actor		HitActor;

	if ( WarWeapon(Weapon) != None && WarWeapon(Weapon).CanPerformMeleeAttack() )
	{
		// Check that player is within striking range
		HitActor = TraceWithPawnOffset( WarWeapon(Weapon).fMeleeRange, out_HitLocation, out_HitNormal);
		return ( Pawn(HitActor) != None && !Level.GRI.OnSameTeam(self,HitActor) );
	}
	
	return false;
}

/**
 * Player does a melee attack
 * called from WarPC::ServerUse
 */
simulated function DoMeleeAttack()
{
	if ( WarWeapon(Weapon) != None && WarWeapon(Weapon).CanPerformMeleeAttack() )
	{
		WarWeapon(Weapon).StartMeleeAttack();
	}
}

/**
 * Return world location to start a weapon fire trace from.
 *
 * @return	World location where to start weapon fire traces from
 */
simulated function Vector GetWeaponStartTraceLocation()
{
	Local vector StartLoc, AimDir, PawnEyesLoc, offset;

	// if not owned by a human player, don't perform any correction
	if ( !IsHumanControlled() )
	{
		//@temp - adjust slightly if behind crouch cover
		if (CoverType == CT_Crouching)
		{
			return Super.GetWeaponStartTraceLocation() + vect(0,0,32);
		}
		else
		{
			return Super.GetWeaponStartTraceLocation();
		}
	}

	// Get camera location (which is where the fixed crosshair is in world space)
	StartLoc	= super.GetWeaponStartTraceLocation();
	// get pawn eyes location. We assume weapon range is from this location
	PawnEyesLoc = GetPawnViewLocation();
	
	// if our original start trace is from the pawn's eyes, then we do not need to do any compensation...
	// (for example from first person view)
	if ( StartLoc == PawnEyesLoc )
	{
		return PawnEyesLoc;
	}
	else
	{
		// our start trace starts from the camera location... which is variable.
		// we need to compensate that so the given range is from the pawn's location.
		// otherwise moving the camera will change the range of the weapon (d'oh!)

		// Get base aim direction without any sort of autocorrection or aim error
		AimDir	= Vector(GetBaseAimRotation());
		// calculate simple offset so range is pretty much constant
		Offset	= ((PawnEyesLoc - StartLoc) dot AimDir) * AimDir;
		// and that's our final start location for traces!
		return StartLoc + Offset;
	}
}

/** Start an evade move */
simulated function Evade( EEvadeDirection EvadeDir )
{
	local PlayerController	PC;
	local float				EvadeDist;
	local vector			X, Y, Z;

	PC = PlayerController(Controller);
	if( PC == None ||
		EvadeDir == ED_None )
	{
		return;
	}

	GetAxes(Rotation, X, Y, Z);
	EvadeDist = 512.f;
	SoundGroupClass.static.PlayEvadeSound( Self );
	PC.bMovementInputEnabled = false;
	PC.bTurningInputEnabled = false;
	EvadeDirection = EvadeDir;
	//@temp - try just setting destination and let physics move us
	PC.bPreciseDestination = true;
	switch( EvadeDir )
	{
		case ED_Backward:
			PC.Destination = Location - EvadeDist * X;	break;
		case ED_Right:
			PC.Destination = Location + EvadeDist * Y;	break;
		case ED_Left:
			PC.Destination = Location - EvadeDist * Y;	break;
		case ED_Forward:
		default:
			PC.Destination = Location + EvadeDist * X;	break;

	}
}

event OnAnimEnd(AnimNodeSequence SeqNode)
{
	local WarPC pc;
	local bool bEvadeNode;

	pc = WarPC(Controller);
	
	// make sure the anim end is from an evade node
	if(Mesh.Animations != None)
	{
		if( SeqNode.NodeName == 'EvadeForward' ||
			SeqNode.NodeName == 'EvadeBackward' ||
			SeqNode.NodeName == 'EvadeLeft' ||
			SeqNode.NodeName == 'EvadeRight' )
		{
			bEvadeNode = true;
		}
	}

	if( bEvadeNode )
	{
		// reset root motion
		EvadeDirection = ED_None;
		if (pc != None)
		{
			pc.bPreciseDestination = false;
			// notify our playercontroller that the evade has finished
			pc.NotifyEvadeFinished();
		}
	}

	Super.OnAnimEnd(SeqNode);
}

/** Attempts to move the pawn down the nearest ledge */
native function bool JumpDownLedge();

/** !!! BEGIN M10 HACK !!! */

/**
 * We should revisit this post milestone to find a more reliable way to move the character up the
 * ledge.  Most likely a separate physics mode that will override PHYS_Falling would work best,
 * eliminating gravity and acceleration, just moving the character directly up and over.
 */
simulated function bool JumpUpLedge()
{
	local JumpPoint pt;
	local vector cameraLoc;
	local rotator cameraRot;
	local float zdiff;
	local WarPC pc;
	pc = WarPC(Controller);
	Controller.GetPlayerViewPoint(cameraLoc,cameraRot);
	foreach AllActors(class'JumpPoint',pt)
	{
		zdiff = (pt.Location.Z - Location.Z + CylinderComponent.CollisionHeight);
		if (VSize2D(pt.Location-Location) < 128.f &&
			zdiff > CylinderComponent.CollisionHeight &&
			Normal(pt.Location-Location) dot vector(cameraRot) > 0.f)
		{
			AirControl = 1.f;
			pc.bMovementInputEnabled = false;
			pc.bTurningInputEnabled = false;
			AddVelocity(vect(0,0,150) * zdiff/(CylinderComponent.CollisionHeight));
			JumpPoint = pt;
			SetTimer(0.1f,true,'CheckJump');
			return true;
		}
	}
	return false;
}

simulated event ModifyVelocity(float DeltaTime,vector OldVelocity)
{
	local float zdiff;
	local vector jumpDir;
	if (JumpPoint != None)
	{
		zdiff = (JumpPoint.Location.Z - Location.Z + CylinderComponent.CollisionHeight);
		Velocity.Z += 600 * DeltaTime * zdiff/(CylinderComponent.CollisionHeight);
		if (zdiff < 32.f)
		{
			jumpDir = Normal(JumpPoint.Location-Location);
			jumpDir.Z = 0;
			Velocity += jumpDir * 1500 * DeltaTime * VSize2D(JumpPoint.Location-Location)/128.f;
		}
	}
	else
	{
		Super.ModifyVelocity(DeltaTime,OldVelocity);
	}
}

function CheckJump()
{
	local WarPC pc;
	pc = WarPC(Controller);
	if (Physics == PHYS_Walking)
	{
		AirControl = default.AirControl;
		ClearTimer('CheckJump');
		bJumpingDownLedge = false;
		JumpPoint = None;
		pc.bMovementInputEnabled = true;
		pc.bTurningInputEnabled = true;
	}
}
/** !!! END M10 HACK !!! */

event Landed(vector HitNormal)
{
	if (bJumpingDownLedge)
	{
		bCanJump = false;
		bJumpingDownLedge = false;
		if (Controller != None)
		{
			Controller.bPreciseDestination = false;
		}
	}
	Super.Landed(HitNormal);
}

/** Physical fire start location. (from weapon's barrel in world coordinates) */
simulated function vector GetPhysicalFireStartLoc( vector FireOffset )
{
	local Vector	X, Y, Z;

	GetAxes(QuatToRotator(Mesh.GetBoneQuaternion('rthand')), X, Y, Z);
	return (Mesh.GetBoneLocation( 'rthand' ) + FireOffset.X*X + FireOffset.Y*Y + FireOffset.Z*Z);
}

/**
 * Player just changed weapon
 * Network: Server, Local Player, Clients
 *
 * @param	OldWeapon	Old weapon held by Pawn.
 * @param	NewWeapon	New weapon held by Pawn.
 */
simulated function PlayWeaponSwitch(Weapon OldWeapon, Weapon NewWeapon)
{
	//log("PlayWeaponSwitch OldWeapon:" $ OldWeapon @ "NewWeapon:" $ NewWeapon );
	// detach old weapon
	if ( WarWeapon(OldWeapon) != None )
	{
		WarWeapon(OldWeapon).DetachWeaponFrom( Mesh );	
	}

	// attach new weapon
	if ( WarWeapon(NewWeapon) != None )
	{
		WarWeapon(NewWeapon).AttachWeaponTo( Mesh, 'rthand' );
	}

	UpdatePlayerAnimations( NewWeapon );
}

/**
 * Adjust Pawn animations when switching to a new weapon
 * Network: ALL
 *
 * @param	NewWeapon	Player just switched to this weapon
 */
simulated function UpdatePlayerAnimations( Weapon NewWeapon )
{
	local AnimNodeCrossfader	ACrossfader;

	if ( WarWeapon(NewWeapon) == None )
		return;

	// Find standing idle node
	ACrossfader = AnimNodeCrossfader( Mesh.Animations.FindAnimNode('anStandIdle') );
	if ( ACrossfader != None && ACrossFader.GetActiveAnimSeq() != WarWeapon(NewWeapon).PawnIdleAnim )
	{
		// If weapon requires a different standing posture, blend to it.
		ACrossfader.BlendToLoopingAnim( WarWeapon(NewWeapon).PawnIdleAnim, 0.15, 1.f );
	}

	// Find standing firing node
	ACrossfader = AnimNodeCrossfader( Mesh.Animations.FindAnimNode('anFire') );
	if ( ACrossfader != None && ACrossFader.GetActiveAnimSeq() != WarWeapon(NewWeapon).PawnFiringAnim )
	{
		// If weapon has a different firing posture, set it as the pawn's default firing animation.
		ACrossFader.GetActiveChild().SetAnim( WarWeapon(NewWeapon).PawnFiringAnim );
	}
}

function PlaySound( SoundCue InSoundCue )
{
	local AudioComponent comp;
	comp = CreateAudioComponent(InSoundCue,false);
	if (comp != None)
	{
		comp.bAllowSpatialization = (PlayerController(Controller) == None);
		comp.bAutoDestroy = true;
		comp.Play();
	}
}

defaultproperties
{
	bReplicateWeapon=true
	ControllerClass=class'WarAIController'
	InventoryManagerClass=class'WarInventoryManager'
	SoundGroupClass=class'WarSoundGroup'
	MeleeRange=+20.0
	bMuffledHearing=true

    Buoyancy=+000.99000000
    UnderWaterTime=+00020.000000
    BaseEyeHeight=+00060.000000
    EyeHeight=+00060.000000
	CrouchHeight=+64.0
	CrouchRadius=+34.0
    GroundSpeed=+00350.000000
    AirSpeed=+00600.000000
    WaterSpeed=+00300.000000
    AccelRate=+02048.000000
    JumpZ=+00540.000000
    bCanStrafe=True
	bCanSwim=true
    RotationRate=(Pitch=0,Yaw=20000,Roll=2048)
	AirControl=+0.35
	bStasis=false
	bCanCrouch=true

	bCanJump=false
	bStopAtLedges=true

	bCanClimbLadders=True
	bCanPickupInventory=True
	WalkingPct=+0.6
	SprintingPct=+2.0
	MovementPct=1.0
	SightRadius=+12000.0

	Begin Object Name=CollisionCylinder
		BlockZeroExtent=false
	End Object

	Begin Object Class=SkeletalMeshComponent Name=WarPawnMesh
		BlockZeroExtent=true
		CollideActors=true
		BlockRigidBody=true
	End Object
	Mesh=WarPawnMesh

	Begin Object Class=TransformComponent Name=WarPawnTransform
    	TransformedComponent=WarPawnMesh
    End Object

    MeshTransform=WarPawnTransform
	Components.Add(WarPawnTransform)
	Components.Remove(Sprite)

	CoverMovementPct=(1.f,0.5f,0.7f,0.5f)
	CoverTranslationOffset=(X=38,Y=0,Z=0)
	CoverProtectionFOV=(X=60,Y=30)

	MaxStepHeight=40.f

	MinJumpDownDistance=48.f
	MaxJumpDownDistance=128.f
}