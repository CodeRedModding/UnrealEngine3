//=============================================================================
// Pawn, the base class of all actors that can be controlled by players or AI.
//
// Pawns are the physical representations of players and creatures in a level.
// Pawns have a mesh, collision, and physics.  Pawns can take damage, make sounds,
// and hold weapons and other inventory.  In short, they are responsible for all
// physical interaction between the player or AI and the world.
//
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class Pawn extends Actor
	abstract
	native
	placeable
	config(Game)
	nativereplication;

var const float			MaxStepHeight,
						MaxJumpHeight;

//-----------------------------------------------------------------------------
// Pawn variables.

var Controller Controller;

// cache net relevancy test
var float				NetRelevancyTime;
var playerController	LastRealViewer;
var actor				LastViewer;

// Physics related flags.
var bool		bJustLanded;		// used by eyeheight adjustment
var bool		bLandRecovery;		// used by eyeheight adjustment
var bool		bUpAndOut;			// used by swimming
var bool		bIsWalking;			// currently walking (can't jump, affects animations)

// Crouching
var				bool	bWantsToCrouch;		// if true crouched (physics will automatically reduce collision height to CrouchHeight)
var		const	bool	bIsCrouched;		// set by physics to specify that pawn is currently crouched
var		const	bool	bTryToUncrouch;		// when auto-crouch during movement, continually try to uncrouch
var()			bool	bCanCrouch;			// if true, this pawn is capable of crouching
var		const	float	UncrouchTime;		// when auto-crouch during movement, continually try to uncrouch once this decrements to zero
var				float	CrouchHeight;		// CollisionHeight when crouching
var				float	CrouchRadius;		// CollisionRadius when crouching

var bool		bCrawler;			// crawling - pitch and roll based on surface pawn is on
var const bool	bReducedSpeed;		// used by movement natives
var bool		bJumpCapable;
var	bool		bCanJump;			// movement capabilities - used by AI
var	bool 		bCanWalk;
var	bool		bCanSwim;
var	bool		bCanFly;
var	bool		bCanClimbLadders;
var	bool		bCanStrafe;
var	bool		bAvoidLedges;		// don't get too close to ledges
var	bool		bStopAtLedges;		// if bAvoidLedges and bStopAtLedges, Pawn doesn't try to walk along the edge at all
var	bool		bNoJumpAdjust;		// set to tell controller not to modify velocity of a jump/fall
var	bool		bCountJumps;		// if true, inventory wants message whenever this pawn jumps
var const bool	bSimulateGravity;	// simulate gravity for this pawn on network clients when predicting position (true if pawn is walking or falling)
var	bool		bUpdateEyeheight;	// if true, UpdateEyeheight will get called every tick
var	bool		bIgnoreForces;		// if true, not affected by external forces
var const bool	bNoVelocityUpdate;	// used by C++ physics
var	bool		bCanWalkOffLedges;	// Can still fall off ledges, even when walking (for Player Controlled pawns)
var bool		bCanBeBaseForPawns;	// all your 'base', are belong to us
var bool		bClientCollision;	// used on clients when temporarily turning off collision
var const bool	bSimGravityDisabled;	// used on network clients
var bool		bDirectHitWall;		// always call pawn hitwall directly (no controller notifyhitwall)
var const bool	bPushesRigidBodies;	// Will do a check to find nearby PHYS_RigidBody actors and will give them a 'soft' push.
var bool        bFlyingRigidBody;      // Tells AI that this vehicle can be flown like PHYS_Flying even though it has rigid body physics

// used by dead pawns (for bodies landing and changing collision box)
var		bool	bInvulnerableBody;

// AI related flags
var		bool	bIsFemale;
var		bool	bCanPickupInventory;	// if true, will pickup inventory when touching pickup actors
var		bool	bAmbientCreature;		// AIs will ignore me
var(AI) bool	bLOSHearing;			// can hear sounds from line-of-sight sources (which are close enough to hear)
										// bLOSHearing=true is like UT/Unreal hearing
var(AI) bool	bSameZoneHearing;		// can hear any sound in same zone (if close enough to hear)
var(AI) bool	bAdjacentZoneHearing;	// can hear any sound in adjacent zone (if close enough to hear)
var(AI) bool	bMuffledHearing;		// can hear sounds through walls (but muffled - sound distance increased to double plus 4x the distance through walls
var(AI) bool	bAroundCornerHearing;	// Hear sounds around one corner (slightly more expensive, and bLOSHearing must also be true)
var(AI) bool	bDontPossess;			// if true, Pawn won't be possessed at game start
var		bool	bAutoFire;				// used for third person weapon anims/effects
var		bool	bRollToDesired;			// Update roll when turning to desired rotation (normally false)

var		bool	bCachedRelevant;		// network relevancy caching flag
var		bool	bUseCompressedPosition;	// use compressed position in networking - true unless want to replicate roll, or very high velocities
var		globalconfig bool bWeaponBob;
var		bool	bSpecialHUD;
var		bool	bIsTyping;

var		byte	FlashCount;				// used for third person weapon anims/effects

// AI basics.
var 	byte	Visibility;			//How visible is the pawn? 0=invisible, 128=normal, 255=highly visible
var		float	DesiredSpeed;
var		float	MaxDesiredSpeed;
var(AI) float	HearingThreshold;	// max distance at which a makenoise(1.0) loudness sound can be heard
var(AI)	float	Alertness;			// -1 to 1 ->Used within specific states for varying reaction to stimuli
var(AI)	float	SightRadius;		// Maximum seeing distance.
var(AI)	float	PeripheralVision;	// Cosine of limits of peripheral vision.
var const float	AvgPhysicsTime;		// Physics updating time monitoring (for AI monitoring reaching destinations)
var		float	MeleeRange;			// Max range for melee attack (not including collision radii)
var NavigationPoint Anchor;			// current nearest path;
var const NavigationPoint LastAnchor;		// recent nearest path
var		float	FindAnchorFailedTime;	// last time a FindPath() attempt failed to find an anchor.
var		float	LastValidAnchorTime;	// last time a valid anchor was found
var		float	DestinationOffset;	// used to vary destination over NavigationPoints
var		float	NextPathRadius;		// radius of next path in route
var		vector	SerpentineDir;		// serpentine direction
var		float	SerpentineDist;
var		float	SerpentineTime;		// how long to stay straight before strafing again
var		float	SpawnTime;

// Movement.
var float   GroundSpeed;    // The maximum ground speed.
var float   WaterSpeed;     // The maximum swimming speed.
var float   AirSpeed;		// The maximum flying speed.
var float	LadderSpeed;	// Ladder climbing speed
var float	AccelRate;		// max acceleration rate
var float	JumpZ;      	// vertical acceleration w/ jump
var float   AirControl;		// amount of AirControl available to the pawn
var float	WalkingPct;		// pct. of running speed that walking speed is
var float	CrouchedPct;	// pct. of running speed that crouched walking speed is
var float	MaxFallSpeed;	// max speed pawn can land without taking damage (also limits what paths AI can use)

// scale to apply to all movements for purposes of ReachedDestination
var float	MoveThresholdScale;

var float      		BaseEyeHeight; 	// Base eye height above collision center.
var float        	EyeHeight;     	// Current eye height, adjusted for bobbing and stairs.
var	vector			Floor;			// Normal of floor pawn is standing on (only used by PHYS_Spider and PHYS_Walking)
var float			SplashTime;		// time of last splash
var float			OldZ;			// Old Z Location - used for eyeheight smoothing
var PhysicsVolume	HeadVolume;		// physics volume of head
var int				HealthMax;		// maximum health
var travel int      Health;         // Health: 100 = normal maximum
var	float			BreathTime;		// used for getting BreathTimer() messages (for no air, etc.)
var float			UnderWaterTime; // how much time pawn can go without air (in seconds)
var	float			LastPainTime;	// last time pawn played a takehit animation (updated in PlayHit())
var class<DamageType> ReducedDamageType; // which damagetype this creature is protected from (used by AI)
var float			HeadScale;

// Sound and noise management
// remember location and position of last noises propagated
var const 	vector 		noise1spot;
var const 	float 		noise1time;
var const	pawn		noise1other;
var const	float		noise1loudness;
var const 	vector 		noise2spot;
var const 	float 		noise2time;
var const	pawn		noise2other;
var const	float		noise2loudness;
var			float		LastPainSound;

// view bob
var	globalconfig	float	Bob;
var					float	LandBob, AppliedBob;
var					float	bobtime;
var					vector	WalkBob;

var float SoundDampening;
var float DamageScaling;

var localized  string MenuName; // Name used for this pawn type in menus (e.g. player selection)

var class<AIController> ControllerClass;	// default class to use when pawn is controlled by AI

var PlayerReplicationInfo PlayerReplicationInfo;

var LadderVolume OnLadder;		// ladder currently being climbed

var name LandMovementState;		// PlayerControllerState to use when moving on land or air
var name WaterMovementState;	// PlayerControllerState to use when moving in water

var PlayerStart LastStartSpot;	// used to avoid spawn camping
var float LastStartTime;

var vector				TakeHitLocation;		// location of last hit (for playing hit/death anims)
var class<DamageType>	HitDamageType;			// damage type of last hit (for playing hit/death anims)
var vector				TearOffMomentum;		// momentum to apply when torn off (bTearOff == true)
var bool				bPlayedDeath;			// set when death animation has been played (used in network games)

var transient CompressedPosition PawnPosition;

var() SkeletalMeshComponent	Mesh;
var() TransformComponent MeshTransform;

var	CylinderComponent		CylinderComponent;

var()	float				RBPushRadius; // Unreal units
var()	float				RBPushStrength;

var		Vehicle DrivenVehicle;

/** replicated to we can see where remote clients are looking */
var		const	byte	RemoteViewPitch;

var()	float	ViewPitchMin;
var()	float	ViewPitchMax;

/** Inventory Manager */
var class<InventoryManager>		InventoryManagerClass;
var InventoryManager			InvManager;

/** Weapon currently held by Pawn */
var()	Weapon					Weapon;

/** 
 * Replicate Currently Held weapon to all actors but the owner 
 * Usually this shouldn't be needed if replicating weapon attachments instead 
 */
var		bool					bReplicateWeapon;

replication
{
	// Variables the server should send ALL clients.
	reliable if( bNetDirty && (Role==ROLE_Authority) )
        bSimulateGravity, bIsWalking, bIsTyping, PlayerReplicationInfo, HitDamageType, 
		TakeHitLocation, HeadScale, DrivenVehicle, Health;

	// Variables the server should send ALL clients but the owner if bReplicateWeapon is set
	reliable if( (bReplicateWeapon || bNetOwner) && bNetDirty && (Role==ROLE_Authority) )
		Weapon;

	// variables sent to owning client
	reliable if ( bNetDirty && bNetOwner && Role==ROLE_Authority )
		InvManager, Controller, GroundSpeed, WaterSpeed, AirSpeed, AccelRate, JumpZ, AirControl;

	// sent to non owning clients
	reliable if ( bNetDirty && !bNetOwner && Role==Role_Authority )
		bIsCrouched;

	// variable sent to all clients when Pawn has been torn off. (bTearOff)
	reliable if( bTearOff && bNetDirty && (Role==ROLE_Authority) )
		TearOffMomentum;

	// variables sent to all but the owning client
    unreliable if ( !bNetOwner && Role==ROLE_Authority )
		PawnPosition, RemoteViewPitch;
}


// =============================================================

simulated event SetHeadScale(float NewScale);

/** 
 * Set Pawn ViewPitch, so we can see where remote clients are looking.
 *
 * @param	NewRemoteViewPitch	Pitch component to replicate to remote (non owned) clients.
 */
native final function SetRemoteViewPitch( int NewRemoteViewPitch );

native function bool ReachedDestination(Actor Goal);
native function ForceCrouch();
native function SetPushesRigidBodies( bool NewPush );

/**
 * Does the following:
 *  - Assign the SkeletalMeshComponent 'Mesh' to the CollisionComponent
 *  - Call InitArticulated on the SkeletalMeshComponent.
 *  - Change the physics mode to PHYS_Articulated.
 */
native function bool InitRagdoll();

function PlayerChangedTeam()
{
	Died( None, class'DamageType', Location );
}

/* Reset()
reset actor to initial state - used when restarting level without reloading.
*/
function Reset()
{
	if ( (Controller == None) || Controller.bIsPlayer )
	{
		DetachFromController();
		Destroy();
	}
	else
		super.Reset();
}

/**
 * Pawn starts firing!
 * Called from PlayerController::StartFiring
 * Network: Local Player
 *
 * @param	FireModeNum		fire mode number
 */
simulated function StartFire( byte FireModeNum )
{
    if ( InvManager != None )
	{
        InvManager.StartFire( FireModeNum );
	}
}

/**
 * Pawn stops firing!
 * i.e. player releases fire button, this may not stop weapon firing right away. (for example press button once for a burst fire)
 * Network: Local Player
 *
 * @param	FireModeNum		fire mode number
 */
simulated function StopFire( byte FireModeNum )
{
    if ( InvManager != None )
	{
        InvManager.StopFire( FireModeNum );
	}
}

/**
 * Is player pressing fire button for fire mode FireModeNum?
 * Note: this is only relevant on the LocalPlayer, as bFire is not replicated to the server.
 * Network: Local Player
 *
 * @param	FireModeNum		fire mode number
 *
 * @return	true if player is pressing fire button for fire mode FireModeNum
 */
simulated function bool IsPressingFire( optional byte FireModeNum )
{
	return ( (Controller != None) && (Controller.bFire != 0) );
}

simulated function String GetHumanReadableName()
{
	if ( PlayerReplicationInfo != None )
		return PlayerReplicationInfo.PlayerName;
	return MenuName;
}

function PlayTeleportEffect(bool bOut, bool bSound)
{
	MakeNoise(1.0);
}

function NotifyTeamChanged();

/* PossessedBy()
 Pawn is possessed by Controller
*/
function PossessedBy(Controller C)
{
	Controller			= C;
	NetPriority			= 3;
	NetUpdateFrequency	= 100;
	NetUpdateTime		= Level.TimeSeconds - 1;

	if ( C.PlayerReplicationInfo != None )
	{
		PlayerReplicationInfo = C.PlayerReplicationInfo;
	}
	if ( C.IsA('PlayerController') )
	{
		if ( Level.NetMode != NM_Standalone )
			RemoteRole = ROLE_AutonomousProxy;
	}
	else
		RemoteRole = Default.RemoteRole;

	SetOwner(Controller);	// for network replication
	Eyeheight = BaseEyeHeight;
//	ChangeAnimation();
}

function UnPossessed()
{
	NetUpdateTime = Level.TimeSeconds - 1;
	if ( DrivenVehicle != None )
		NetUpdateFrequency = 5;

	PlayerReplicationInfo = None;
	SetOwner(None);
	Controller = None;
}

/**
 * returns default camera mode when viewing this pawn.
 * Mainly called when controller possesses this pawn.
 *
 * @param	PlayerController requesting the default camera view
 * @return	default camera view player should use when controlling this pawn.
 */
simulated function name GetDefaultCameraMode( PlayerController RequestedBy )
{
	if ( RequestedBy != None && RequestedBy.PlayerCamera != None && RequestedBy.PlayerCamera.CameraStyle == 'Fixed' )
		return 'Fixed';

	return 'FirstPerson';
}

/* BecomeViewTarget
	Called by Camera when this actor becomes its ViewTarget */
event BecomeViewTarget( PlayerController PC )
{
	bUpdateEyeHeight = true;
}

function DropToGround()
{
	bCollideWorld = True;
	if ( Health > 0 )
	{
		SetCollision(true,true);
		SetPhysics(PHYS_Falling);
		if ( IsHumanControlled() )
			Controller.GotoState(LandMovementState);
	}
}

function bool CanGrabLadder()
{
	return ( bCanClimbLadders
			&& (Controller != None)
			&& (Physics != PHYS_Ladder)
			&& ((Physics != Phys_Falling) || (abs(Velocity.Z) <= JumpZ)) );
}

/**
 * Called every frame from PlayerInput or PlayerController::MoveAutonomous()
 * Sets bIsWalking flag, which defines if the Pawn is walking or not (affects velocity)
 *
 * @param	bNewIsWalking, new walking state.
 */
event SetWalking( bool bNewIsWalking )
{
	if ( bNewIsWalking != bIsWalking )
	{
		bIsWalking = bNewIsWalking;
	}
}

simulated function bool CanSplash()
{
	if ( (Level.TimeSeconds - SplashTime > 0.15)
		&& ((Physics == PHYS_Falling) || (Physics == PHYS_Flying))
		&& (Abs(Velocity.Z) > 100) )
	{
		SplashTime = Level.TimeSeconds;
		return true;
	}
	return false;
}

function EndClimbLadder(LadderVolume OldLadder)
{
	if ( Controller != None )
		Controller.EndClimbLadder();
	if ( Physics == PHYS_Ladder )
		SetPhysics(PHYS_Falling);
}

function ClimbLadder(LadderVolume L)
{
	OnLadder = L;
	SetRotation(OnLadder.WallDir);
	SetPhysics(PHYS_Ladder);
	if ( IsHumanControlled() )
		Controller.GotoState('PlayerClimbing');
}

/** 
 * list important Pawn variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local string				T;
	local Canvas Canvas;

	Canvas = HUD.Canvas;

	if ( PlayerReplicationInfo == None )
	{
		Canvas.DrawText("NO PLAYERREPLICATIONINFO", false);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}
	else
	{
		PlayerReplicationInfo.DisplayDebug(HUD,out_YL,out_YPos);
	}

	super.DisplayDebug(HUD, out_YL, out_YPos);

	Canvas.SetDrawColor(255,255,255);

	Canvas.DrawText("Health "$Health);
	out_YPos += out_YL;
	Canvas.SetPos(4, out_YPos);

	if ( HUD.bShowAIDebug )
	{
		Canvas.DrawText("Anchor "$Anchor$" Serpentine Dist "$SerpentineDist$" Time "$SerpentineTime);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}

	if ( HUD.bShowPhysicsDebug )
	{
		T = "Floor "$Floor$" DesiredSpeed "$DesiredSpeed$" Crouched "$bIsCrouched;
		if ( (OnLadder != None) || (Physics == PHYS_Ladder) )
			T=T$" on ladder "$OnLadder;
		Canvas.DrawText(T);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}

	Canvas.DrawText("EyeHeight "$Eyeheight$" BaseEyeHeight "$BaseEyeHeight);
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);

	// Controller
	if ( Controller == None )
	{
		Canvas.SetDrawColor(255,0,0);
		Canvas.DrawText("NO CONTROLLER");
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}
	else
	{
		Controller.DisplayDebug(HUD, out_YL, out_YPos);
	}

	// Weapon
	if ( HUD.bShowWeaponDebug )
	{
		if ( Weapon == None )
		{
			Canvas.SetDrawColor(0,255,0);
			Canvas.DrawText("NO WEAPON");
			out_YPos += out_YL;
			Canvas.SetPos(4, out_YPos);
		}
		else
			Weapon.DisplayDebug(HUD, out_YL, out_YPos);
	}
}

simulated function vector WeaponBob(float BobDamping)
{
	Local Vector WBob;

	WBob = BobDamping * WalkBob;
	WBob.Z = (0.45 + 0.55 * BobDamping) * WalkBob.Z;
	WBob.Z += LandBob;
	return WBob;
}

function CheckBob(float DeltaTime, vector Y)
{
	local float Speed2D;

    if( !bWeaponBob || bJustLanded )
    {
		BobTime = 0;
		WalkBob = Vect(0,0,0);
        return;
    }
	Bob = FClamp(Bob, -0.01, 0.01);
	if (Physics == PHYS_Walking )
	{
		Speed2D = VSize(Velocity);
		if ( Speed2D < 10 )
			BobTime += 0.2 * DeltaTime;
		else
			BobTime += DeltaTime * (0.3 + 0.7 * Speed2D/GroundSpeed);
		WalkBob = Y * Bob * Speed2D * sin(8 * BobTime);
		AppliedBob = AppliedBob * (1 - FMin(1, 16 * deltatime));
		WalkBob.Z = AppliedBob;
		if ( Speed2D > 10 )
			WalkBob.Z = WalkBob.Z + 0.75 * Bob * Speed2D * sin(16 * BobTime);
	}
	else if ( Physics == PHYS_Swimming )
	{
		BobTime += DeltaTime;
		Speed2D = Sqrt(Velocity.X * Velocity.X + Velocity.Y * Velocity.Y);
		WalkBob = Y * Bob *  0.5 * Speed2D * sin(4.0 * BobTime);
		WalkBob.Z = Bob * 1.5 * Speed2D * sin(8.0 * BobTime);
	}
	else
	{
		BobTime = 0;
		WalkBob = WalkBob * (1 - FMin(1, 8 * deltatime));
	}
}

//***************************************
// Interface to Pawn's Controller

// return true if controlled by a Player (AI or human)
simulated function bool IsPlayerPawn()
{
	return ( (Controller != None) && Controller.bIsPlayer );
}

// return true if was controlled by a Player (AI or human)
simulated function bool WasPlayerPawn()
{
	return false;
}

// return true if controlled by a real live human
simulated function bool IsHumanControlled()
{
	return ( PlayerController(Controller) != None );
}

// return true if controlled by local (not network) player
simulated function bool IsLocallyControlled()
{
	if ( Level.NetMode == NM_Standalone )
		return true;
	if ( Controller == None )
		return false;
	if ( PlayerController(Controller) == None )
		return true;

	return ( LocalPlayer(PlayerController(Controller).Player) != None );
}

// return true if viewing this pawn in first person pov. useful for determining what and where to spawn effects
simulated function bool IsFirstPerson()
{
    local PlayerController PC;

    PC = PlayerController(Controller);
    return ( PC!=None && ((PC.PlayerCamera == None) || (PC.PlayerCamera.CameraStyle == 'FirstPerson')) && LocalPlayer(PC.Player) != None );
}

event UpdateEyeHeight( float DeltaTime )
{
	local float smooth, MaxEyeHeight;
	local float OldEyeHeight;
	local Actor HitActor;
	local vector HitLocation,HitNormal;

	if (Controller == None )
	{
		EyeHeight = 0;
		return;
	}
	if ( bTearOff )
	{
		EyeHeight = Default.BaseEyeheight;
		bUpdateEyeHeight = false;
		return;
	}
	HitActor = trace(HitLocation,HitNormal,Location + (CylinderComponent.CollisionHeight + MaxStepHeight + 14) * vect(0,0,1),
					Location + CylinderComponent.CollisionHeight * vect(0,0,1),true);
	if ( HitActor == None )
		MaxEyeHeight = CylinderComponent.CollisionHeight + MaxStepHeight;
	else
		MaxEyeHeight = HitLocation.Z - Location.Z - 14;

	if ( abs(Location.Z - OldZ) > 15 )
	{
		bJustLanded = false;
		bLandRecovery = false;
	}

	// smooth up/down stairs
	if ( !bJustLanded )
	{
		smooth = FMin(0.9, 10.0 * DeltaTime/Level.TimeDilation);
		LandBob *= (1 - smooth);
		if( Controller.WantsSmoothedView() )
		{
			OldEyeHeight = EyeHeight;
			EyeHeight = FClamp((EyeHeight - Location.Z + OldZ) * (1 - smooth) + BaseEyeHeight * smooth,
								-0.5 * CylinderComponent.CollisionHeight, MaxEyeheight);
		}
		else
		    EyeHeight = FMin(EyeHeight * ( 1 - smooth) + BaseEyeHeight * smooth, MaxEyeHeight);
	}
	else if ( bLandRecovery )
	{
		smooth = FMin(0.9, 10.0 * DeltaTime);
		OldEyeHeight = EyeHeight;
	    EyeHeight = FMin(EyeHeight * ( 1 - 0.6*smooth) + BaseEyeHeight * 0.6*smooth, BaseEyeHeight);
		LandBob *= (1 - smooth);
		if ( Eyeheight >= BaseEyeheight - 1)
		{
			bJustLanded = false;
			bLandRecovery = false;
			Eyeheight = BaseEyeheight;
		}
	}
	else
	{
		smooth = FMin(0.65, 10.0 * DeltaTime);
		OldEyeHeight = EyeHeight;
		EyeHeight = FMin(EyeHeight * (1 - 1.5*smooth), MaxEyeHeight);
		LandBob += 0.03 * (OldEyeHeight - Eyeheight);
		if ( (Eyeheight < 0.25 * BaseEyeheight + 1) || (LandBob > 3)  )
		{
			bLandRecovery = true;
			Eyeheight = 0.25 * BaseEyeheight + 1;
		}
	}
	Controller.AdjustView( DeltaTime );
}

/* EyePosition()
Called by PlayerController to determine camera position in first person view.  Returns
the offset from the Pawn's location at which to place the camera
*/
simulated function vector EyePosition()
{
	/* Eye Height for first person view, with bobing
	if ( PlayerController(Controller) != None )
		return Vect(0,0,1) * EyeHeight + WalkBob;
	else */
		return Vect(0,0,1) * BaseEyeHeight;
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
	// Add Delta Rotation
	out_ViewRotation	+= out_DeltaRot;
	out_DeltaRot		 = rot(0,0,0);

	// Limit Player View Pitch
	out_ViewRotation.Pitch = out_ViewRotation.Pitch & 65535;
    if ( out_ViewRotation.Pitch > ViewPitchMax && out_ViewRotation.Pitch < (65535+ViewPitchMin))
	{
		if ( out_ViewRotation.Pitch < 32768 )
		{
			out_ViewRotation.Pitch = ViewPitchMax;
		}
		else
		{
			out_ViewRotation.Pitch = 65535 + ViewPitchMin;
		}
	}
}

/**
 * returns the point of view of the actor.
 * note that this doesn't mean the camera, but the 'eyes' of the actor.
 * For example, for a Pawn, this would define the eye height location, 
 * and view rotation (which is different from the pawn rotation which has a zeroed pitch component).
 * A camera first person view will typically use this view point. Most traces (weapon, AI) will be done from this view point.
 *
 * @output	out_Location, location of view point
 * @output	out_Rotation, view rotation of actor.
 */
simulated event GetActorEyesViewPoint( out vector out_Location, out Rotator out_Rotation )
{
	out_Location = GetPawnViewLocation();
	
	if ( Controller != None )
	{
		out_Rotation = Controller.Rotation;
	}
	else
	{
		out_Rotation = Rotation;
	}
}

/**
 * returns the Eye location of the Pawn.
 *
 * @return	Pawn's eye location
 */
simulated function Vector GetPawnViewLocation()
{
	return Location + EyePosition();
}

/**
 * Return world location to start a weapon fire trace from.
 *
 * @return	World location where to start weapon fire traces from
 */
simulated function Vector GetWeaponStartTraceLocation()
{
	local vector	POVLoc;
	local rotator	POVRot;

	// If we have a controller, by default we start tracing from the player's 'eyes' location
	// that is by default Controller.Location for AI, and camera (crosshair) location for human players.
	if ( Controller != None )
	{
		Controller.GetPlayerViewPoint( POVLoc, POVRot );
		return POVLoc;
	}

	// If we have no controller, we simply traces from pawn eyes location
	return GetPawnViewLocation();
}

/**
 * returns base Aim Rotation without any adjustment (no aim error, no autolock, no adhesion.. just clean initial aim rotation!)
 *
 * @return	base Aim rotation.
 */
simulated function Rotator GetBaseAimRotation()
{
	local vector	POVLoc;
	local rotator	POVRot;

	// If we have a controller, by default we aim at the player's 'eyes' direction
	// that is by default Controller.Rotation for AI, and camera (crosshair) rotation for human players.
	if( Controller != None && !InFreeCam() )
	{
		Controller.GetPlayerViewPoint( POVLoc, POVRot );
		return POVRot;
	}

	// If we have no controller, we simply use our rotation
	POVRot = Rotation;

	// If our Pitch is 0, then use RemoveViewPitch
	if ( POVRot.Pitch == 0 )
	{
		POVRot.Pitch = RemoteViewPitch << 8;
	}

	return POVRot;
}

/** return true if player is viewing this Pawn in FreeCam */
simulated function bool InFreeCam()
{
	return (PlayerController(Controller) != None && PlayerController(Controller).PlayerCamera != None && PlayerController(Controller).PlayerCamera.CameraStyle == 'FreeCam');
}

/**
 * Adjusts weapon aiming direction.
 * Gives Pawn a chance to modify its aiming. For example aim error, auto aiming, adhesion, AI help...
 * Requested by weapon prior to firing.
 *
 * @param	W, weapon about to fire
 * @param	StartFireLoc, world location of weapon fire start trace, or projectile spawn loc.
 * @param	BaseAimRot, original aiming rotation without any modifications.
 */
simulated function Rotator GetAdjustedAimFor( Weapon W, vector StartFireLoc )
{
	local Rotator	BaseAimRot;

	BaseAimRot = GetBaseAimRotation();

	// If controller doesn't exist or we're a client, get the where the Pawn is aiming at
	if ( Controller == None || Role < Role_Authority )
		return BaseAimRot;

	// otherwise, give a chance to controller to adjust this Aim Rotation
	return Controller.GetAdjustedAimFor( W, StartFireLoc, BaseAimRot );
}

simulated function SetViewRotation(rotator NewRotation )
{
	if ( Controller != None )
		Controller.SetRotation(NewRotation);
}

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
	return false;
}

final function bool InGodMode()
{
	return ( (Controller != None) && Controller.bGodMode );
}

function bool NearMoveTarget()
{
	if ( (Controller == None) || (Controller.MoveTarget == None) )
		return false;

	return ReachedDestination(Controller.MoveTarget);
}

function Actor GetMoveTarget()
{
	if ( Controller == None )
		return None;

	return Controller.MoveTarget;
}

function SetMoveTarget(Actor NewTarget )
{
	if ( Controller != None )
		Controller.MoveTarget = NewTarget;
}

function bool LineOfSightTo(actor Other)
{
	return ( (Controller != None) && Controller.LineOfSightTo(Other) );
}

function Actor ShootSpecial(Actor A)
{
	if ( !Controller.bCanDoSpecial || (Weapon == None) )
		return None;

	Controller.FireWeaponAt(A);
	Controller.bFire = 0;
	return A;
}

/* return a value (typically 0 to 1) adjusting pawn's perceived strength if under some special influence (like berserk)
*/
function float AdjustedStrength()
{
	return 0;
}

function HandlePickup(Inventory Inv)
{
	MakeNoise(0.2);
	if ( Controller != None )
		Controller.HandlePickup(Inv);
}

function ReceiveLocalizedMessage( class<LocalMessage> Message, optional int Switch, optional PlayerReplicationInfo RelatedPRI_1, optional PlayerReplicationInfo RelatedPRI_2, optional Object OptionalObject )
{
	if ( PlayerController(Controller) != None )
		PlayerController(Controller).ReceiveLocalizedMessage( Message, Switch, RelatedPRI_1, RelatedPRI_2, OptionalObject );
}

event ClientMessage( coerce string S, optional Name Type )
{
	if ( PlayerController(Controller) != None )
		PlayerController(Controller).ClientMessage( S, Type );
}

//***************************************



function FinishedInterpolation()
{
	DropToGround();
}

function JumpOutOfWater(vector jumpDir)
{
	Falling();
	Velocity = jumpDir * WaterSpeed;
	Acceleration = jumpDir * AccelRate;
	velocity.Z = FMax(380,JumpZ); //set here so physics uses this for remainder of tick
	bUpAndOut = true;
}

/*
Modify velocity called by physics before applying new velocity
for this tick.

Velocity,Acceleration, etc. have been updated by the physics, but location hasn't
*/
simulated event ModifyVelocity(float DeltaTime, vector OldVelocity);

event FellOutOfWorld(class<DamageType> dmgType)
{
	if (Role == ROLE_Authority)
	{
		Health = -1;
if ( (dmgType == class'DmgType_Suicided') && (Physics != PHYS_RigidBody) && (Physics != PHYS_Articulated) )
SetPhysics(PHYS_None);

		Died( None, dmgType, Location );
	}
}

/**
 * Makes sure a Pawn is not crouching, telling it to stand if necessary.
 */
simulated function UnCrouch()
{
	if( bIsCrouched || bWantsToCrouch )
	{
		ShouldCrouch( false );
	}
}

/**
 * Controller is requesting that pawn crouches.
 * This is not guaranteed as it depends if crouching collision cylinder can fit when Pawn is located.
 *
 * @param	bCrouch		true if Pawn should crouch.
 */
function ShouldCrouch( bool bCrouch )
{
	if( bWantsToCrouch != bCrouch )
	{
		bWantsToCrouch = bCrouch;
	}
}

/**
 * Event called from native code when Pawn stops crouching.
 * Called on non owned Pawns through bIsCrouched replication.
 * Network: ALL
 *
 * @param	HeightAdjust	height difference in unreal units between default collision height, and actual crouched cylinder height.
 */
simulated event EndCrouch( float HeightAdjust )
{
	EyeHeight		-= HeightAdjust;
	OldZ			+= HeightAdjust;
	BaseEyeHeight	 = default.BaseEyeHeight;

	SetMeshTransformOffset(MeshTransform.Translation - Vect(0,0,1) * HeightAdjust);
}

/**
 * Event called from native code when Pawn starts crouching.
 * Called on non owned Pawns through bIsCrouched replication.
 * Network: ALL
 *
 * @param	HeightAdjust	height difference in unreal units between default collision height, and actual crouched cylinder height.
 */
simulated event StartCrouch( float HeightAdjust )
{
	EyeHeight		+= HeightAdjust;
	OldZ			-= HeightAdjust;
	BaseEyeHeight	 = FMin(0.8 * CrouchHeight, CrouchHeight - 10);

	SetMeshTransformOffset(MeshTransform.Translation + Vect(0,0,1) * HeightAdjust);
}

function RestartPlayer();
function AddVelocity( vector NewVelocity)
{
	if ( bIgnoreForces || (NewVelocity == vect(0,0,0)) )
		return;
	if ( (Physics == PHYS_Walking)
		|| (((Physics == PHYS_Ladder) || (Physics == PHYS_Spider)) && (NewVelocity.Z > Default.JumpZ)) )
		SetPhysics(PHYS_Falling);
	if ( (Velocity.Z > 380) && (NewVelocity.Z > 0) )
		NewVelocity.Z *= 0.5;
	Velocity += NewVelocity;
}

function KilledBy( pawn EventInstigator )
{
	local Controller Killer;

	Health = 0;
	if ( EventInstigator != None )
		Killer = EventInstigator.Controller;
	Died( Killer, class'DmgType_Suicided', Location );
}

function TakeFallingDamage()
{
	local float EffectiveSpeed;

	if (Velocity.Z < -0.5 * MaxFallSpeed)
	{
		if ( Role == ROLE_Authority )
		{
		    MakeNoise(1.0);
		    if (Velocity.Z < -1 * MaxFallSpeed)
		    {
			    EffectiveSpeed = Velocity.Z;
			    if ( TouchingWaterVolume() )
				    EffectiveSpeed += 100;
			    if ( EffectiveSpeed < -1 * MaxFallSpeed )
				    TakeDamage(-100 * (EffectiveSpeed + MaxFallSpeed)/MaxFallSpeed, None, Location, vect(0,0,0), class'DmgType_Fell');
	        }
		}
	}
	else if (Velocity.Z < -1.4 * JumpZ)
		MakeNoise(0.5);
}

function Restart();

function ClientReStart()
{
	Velocity = vect(0,0,0);
	Acceleration = vect(0,0,0);
	BaseEyeHeight = Default.BaseEyeHeight;
	EyeHeight = BaseEyeHeight;
}

function ClientSetLocation( vector NewLocation, rotator NewRotation )
{
	if ( Controller != None )
		Controller.ClientSetLocation(NewLocation, NewRotation);
}

function ClientSetRotation( rotator NewRotation )
{
	if ( Controller != None )
		Controller.ClientSetRotation(NewRotation);
}

simulated function FaceRotation( rotator NewRotation, float DeltaTime )
{
	local Rotator	Rot;

	if ( InFreeCam() )
	{
		// Do not update Pawn's rotation depending on controller's ViewRotation if in FreeCam.
		return;
	}

	Rot = NewRotation;
	if ( Physics == PHYS_Ladder )
	{
		Rot = OnLadder.Walldir;
	}
	else if ( (Physics == PHYS_Walking) || (Physics == PHYS_Falling) )
	{
		Rot.Pitch = 0;
	}
	
	SetRotation( Rot );
}

function bool InCurrentCombo()
{
	return false;
}

//==============
// Encroachment
event bool EncroachingOn( actor Other )
{
	if ( Other.bWorldGeometry || Other.bBlocksTeleport )
		return true;

	if ( ((Controller == None) || !Controller.bIsPlayer) && (Pawn(Other) != None) )
		return true;

	return false;
}

event EncroachedBy( actor Other )
{
	// Allow encroachment by Vehicles so they can push the pawn out of the way
	if ( Pawn(Other) != None && Vehicle(Other) == None )
		gibbedBy(Other);
}

function gibbedBy(actor Other)
{
	if ( Role < ROLE_Authority )
		return;
	if ( Pawn(Other) != None )
		Died(Pawn(Other).Controller, class'DmgType_Telefragged', Location);
	else
		Died(None, class'DmgType_Gibbed', Location);
}

//Base change - if new base is pawn or decoration, damage based on relative mass and old velocity
// Also, non-players will jump off pawns immediately
function JumpOffPawn()
{
	Velocity += (100 + CylinderComponent.CollisionRadius) * VRand();
	Velocity.Z = 200 + CylinderComponent.CollisionHeight;
	SetPhysics(PHYS_Falling);
	bNoJumpAdjust = true;
}

singular event BaseChange()
{
	if ( (base == None) && (Physics == PHYS_None) )
		SetPhysics(PHYS_Falling);
	// Pawns can only set base to non-pawns, or pawns which specifically allow it.
	// Otherwise we do some damage and jump off.
	else if ( Pawn(Base) != None && Base != DrivenVehicle )
	{
		if ( !Pawn(Base).bCanBeBaseForPawns )
		{
			Base.TakeDamage( (1-Velocity.Z/400)* Mass/Base.Mass, Self,Location,0.5 * Velocity , class'DmgType_Crushed');
			JumpOffPawn();
		}
	}
}

//=============================================================================

/**
 * Call this function to detach safely pawn from its controller
 *
 * @param	bDestroyController	if true, then destroy controller. (only AI Controllers, not players)
 */
function DetachFromController( optional bool bDestroyController )
{
	local Controller OldController;

	// if we have a controller, notify it we're getting destroyed
	// be careful with bTearOff, we're authority on client! Make sure our controller and pawn match up.
	if ( Controller != None && Controller.Pawn == Self )
	{
		OldController = Controller;
		Controller.PawnDied( Self );
		if ( Controller != None )
		{
			Controller.UnPossess();
		}
		
		if ( OldController != None && !OldController.bDeleteMe && PlayerController(OldController) == None )
		{
			OldController.Destroy();
		}

		Controller = None;
	}
}

simulated event Destroyed()
{
	DetachFromController();

	if ( Level.NetMode == NM_Client )
		return;

	if ( InvManager != None )
		InvManager.Destroy();

	Weapon = None;
	super.Destroyed();
}

//=============================================================================
//
// Called immediately before gameplay begins.
//
event PreBeginPlay()
{
	super.PreBeginPlay();

	Instigator = self;
	DesiredRotation = Rotation;
	if ( bDeleteMe )
		return;

	if ( BaseEyeHeight == 0 )
		BaseEyeHeight = 0.8 * CylinderComponent.CollisionHeight;
	EyeHeight = BaseEyeHeight;
}

event PostBeginPlay()
{
	super.PostBeginPlay();

	SplashTime	= 0;
	SpawnTime	= Level.TimeSeconds;
	EyeHeight	= BaseEyeHeight;

	// automatically add controller to pawns which were placed in level
	// NOTE: pawns spawned during gameplay are not automatically possessed by a controller
	if ( Level.bStartup && (Health > 0) && !bDontPossess )
	{
		SpawnDefaultController();
	}

	// Spawn Inventory Container
	if ( Role == Role_Authority && InvManager == None )
	{
		InvManager = Spawn(InventoryManagerClass, Self);
		if ( InvManager == None )
			log("Warning! Couldn't spawn InventoryManager" @ InventoryManagerClass @ "for" @ Self @ GetHumanReadableName() ); 
		else
			InvManager.SetupFor( Self );
	}
}

/**
 * Spawn default controller for this Pawn, get possessed by it.
 */
function SpawnDefaultController()
{
	if ( Controller != None )
	{
		log("SpawnDefaultController" @ Self @ ", Controller != None" @ Controller );
		return;
	}

	if ( ControllerClass != None )
	{
		Controller = Spawn(ControllerClass);
	}

	if ( Controller != None )
	{
		Controller.Possess( Self );
	}
}

/**
 * Nukes the current controller if it exists and creates a new one
 * using the specified class.
 * Event called from Kismet.
 * 
 * @param		inAction - scripted action that was activated
 */
function OnAssignController(SeqAct_AssignController inAction)
{

	if ( inAction.ControllerClass != None )
	{
		if ( Controller != None )
		{
			DetachFromController( true );
		}

		Controller = Spawn(inAction.ControllerClass);
		Controller.Possess( Self );

		// Set class as the default one if pawn is restarted.
		if ( Controller.IsA('AIController') )
		{
			ControllerClass = class<AIController>(Controller.Class);
		}
	}
	else
	{
		Warn("Assign controller w/o a class specified!");
	}
}

// called after PostBeginPlay on net client
simulated event PostNetBeginPlay()
{
	if ( Role == ROLE_Authority )
		return;
	if ( (Controller != None) && (Controller.Pawn == None) )
	{
		Controller.Pawn = self;
		if ( (PlayerController(Controller) != None)
			&& (PlayerController(Controller).ViewTarget == Controller) )
			PlayerController(Controller).SetViewTarget(self);
	}

	if ( Role == ROLE_AutonomousProxy )
		bUpdateEyeHeight = true;

	if ( (PlayerReplicationInfo != None)
		&& (PlayerReplicationInfo.Owner == None) )
		PlayerReplicationInfo.SetOwner(Controller);
}

function Gasp();

function SetMovementPhysics()
{
		// check for water volume
		if (PhysicsVolume.bWaterVolume)
		{
			SetPhysics(PHYS_Swimming);
		}
	else if (Physics != PHYS_Falling)
	{
		SetPhysics(PHYS_Falling);
	}
}

function TakeDamage( int Damage, Pawn instigatedBy, Vector hitlocation,
						Vector momentum, class<DamageType> damageType, optional TraceHitInfo HitInfo)
{
	local int			actualDamage;
	local bool			bAlreadyDead;
	local PlayerController PC;
	local Controller	Killer;

	if ( damagetype == None )
	{
		//FIXMESTEVE warn("No damagetype for damage by "$instigatedby$" with weapon "$InstigatedBy.Weapon);
		DamageType = class'DamageType';
	}

	if ( Role < ROLE_Authority )
	{
		log(self$" client damage type "$damageType$" by "$instigatedBy);
		return;
	}

	bAlreadyDead = (Health <= 0);

	if ( (Physics == PHYS_None) && (DrivenVehicle == None) )
		SetMovementPhysics();
	if (Physics == PHYS_Walking)
		momentum.Z = FMax(momentum.Z, 0.4 * VSize(momentum));
	if ( (instigatedBy == self)
		|| ((Controller != None) && (InstigatedBy != None) && (InstigatedBy.Controller != None) && Level.GRI.OnSameTeam(InstigatedBy.Controller,Controller)) )
		momentum *= 0.6;
	momentum = momentum/Mass;

	if ( DrivenVehicle != None )
        DrivenVehicle.AdjustDriverDamage( Damage, InstigatedBy, HitLocation, Momentum, DamageType );

	ActualDamage = Damage;
	Level.Game.ReduceDamage(ActualDamage, self, instigatedBy, HitLocation, Momentum, DamageType);

	// call Actor's version to handle any SeqEvent_TakeDamage for scripting
	Super.TakeDamage(actualDamage,instigatedBy,hitlocation,momentum,damageType,HitInfo);

	Health -= actualDamage;
	if ( HitLocation == vect(0,0,0) )
		HitLocation = Location;
	if ( bAlreadyDead )
	{
		Warn(self$" took regular damage "$damagetype$" from "$instigatedby$" while already dead at "$Level.TimeSeconds);
		ChunkUp(Rotation, DamageType);
		return;
	}

	PlayHit(actualDamage,InstigatedBy, hitLocation, damageType, Momentum);
	if ( Health <= 0 )
	{
		PC = PlayerController(Controller);
		// play force feedback for death
		if (PC != None)
		{
			PC.ClientPlayForceFeedbackWaveform(damageType.default.KilledFFWaveform);
		}
		// pawn died
		if ( instigatedBy != None )
			Killer = instigatedBy.GetKillerController();
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

simulated function byte GetTeamNum()
{
	if ( Controller != None )
	{
		return Controller.GetTeamNum();
	}

	if ( (DrivenVehicle != None) && (DrivenVehicle.Controller != None) )
	{
		return DrivenVehicle.Controller.GetTeamNum();
	}

	if ( (PlayerReplicationInfo == None) || (PlayerReplicationInfo.Team == None) )
	{
		return 255;
	}

	return PlayerReplicationInfo.Team.TeamIndex;
}

function TeamInfo GetTeam()
{
	if ( PlayerReplicationInfo != None )
		return PlayerReplicationInfo.Team;
	if ( (DrivenVehicle != None) && (DrivenVehicle.PlayerReplicationInfo != None) )
		return DrivenVehicle.PlayerReplicationInfo.Team;
	return None;
}

function Controller GetKillerController()
{
	return Controller;
}

function Died(Controller Killer, class<DamageType> damageType, vector HitLocation)
{
    local Vector	TossVel, POVLoc;
	local Rotator	POVRot;
	local int idx;

	if ( bDeleteMe )
	{
		return; //already destroyed
	}

	// mutator hook to prevent deaths
	// WARNING - don't prevent bot suicides - they suicide when really needed
	if ( Level.Game.PreventDeath(self, Killer, damageType, HitLocation) )
	{
		Health = max(Health, 1); //mutator should set this higher
		return;
	}
	Health = Min(0, Health);

	// activate death events
	ActivateEventClass(class'SeqEvent_Death', self);
	// and abort any latent actions
	for (idx = 0; idx < LatentActions.Length; idx++)
	{
		if (LatentActions[idx] != None)
		{
			LatentActions[idx].AbortFor(self);
		}
	}
	LatentActions.Length = 0;

    if ( Weapon != None && DrivenVehicle == None )
    {
		GetActorEyesViewPoint( POVLoc, POVRot );
        TossVel = Vector(POVRot);
        TossVel = TossVel * ((Velocity Dot TossVel) + 500) + Vect(0,0,200);
        TossWeapon(TossVel);
    }

	if ( DrivenVehicle != None )
	{
		Velocity = DrivenVehicle.Velocity;
		DrivenVehicle.DriverDied();
	}

	if ( Controller != None )
	{
		Level.Game.Killed(Killer, Controller, self, damageType);
	}
	else
		Level.Game.Killed(Killer, Controller(Owner), self, damageType);

	DrivenVehicle = None;

	Velocity.Z *= 1.3;
	if ( IsHumanControlled() )
		PlayerController(Controller).ForceDeathUpdate();

	if ( !IsLocallyControlled() )
		Controller.ClientDying(DamageType, HitLocation);

    if ( (DamageType != None) && DamageType.default.bAlwaysGibs )
		ChunkUp( Rotation, DamageType );
	else
	{
		PlayDying(DamageType, HitLocation);
	}
}

function bool Gibbed(class<DamageType> damageType)
{
	if ( damageType.default.GibModifier == 0 )
		return false;
	if ( damageType.default.GibModifier >= 100 )
		return true;
	if ( (Health < -80) || ((Health < -40) && (FRand() < 0.6)) )
		return true;
	return false;
}

event Falling();

event HitWall(vector HitNormal, actor Wall);

event Landed(vector HitNormal)
{
	TakeFallingDamage();
	if ( Health > 0 )
		PlayLanded(Velocity.Z);
	if ( (Velocity.Z < -200) && (PlayerController(Controller) != None) )
	{
		OldZ = Location.Z;
		bJustLanded = PlayerController(Controller).bLandingShake;
	}
}

event HeadVolumeChange(PhysicsVolume newHeadVolume)
{
	if ( (Level.NetMode == NM_Client) || (Controller == None) )
		return;
	if ( HeadVolume.bWaterVolume )
	{
		if (!newHeadVolume.bWaterVolume)
		{
			if ( Controller.bIsPlayer && (BreathTime > 0) && (BreathTime < 8) )
				Gasp();
			BreathTime = -1.0;
		}
	}
	else if ( newHeadVolume.bWaterVolume )
		BreathTime = UnderWaterTime;
}

function bool TouchingWaterVolume()
{
	local PhysicsVolume V;

	ForEach TouchingActors(class'PhysicsVolume',V)
		if ( V.bWaterVolume )
			return true;

	return false;
}

//Pain timer just expired.
//Check what zone I'm in (and which parts are)
//based on that cause damage, and reset BreathTime

function bool IsInPain()
{
	local PhysicsVolume V;

	ForEach TouchingActors(class'PhysicsVolume',V)
		if ( V.bPainCausing && (V.DamageType != ReducedDamageType)
			&& (V.DamagePerSec > 0) )
			return true;
	return false;
}

event BreathTimer()
{
	if ( (Health < 0) || (Level.NetMode == NM_Client) || (DrivenVehicle != None) )
		return;
	TakeDrowningDamage();
	if ( Health > 0 )
		BreathTime = 2.0;
}

function TakeDrowningDamage();

function bool CheckWaterJump(out vector WallNormal)
{
	local actor HitActor;
	local vector HitLocation, HitNormal, checkpoint, start, checkNorm, Extent;

	if ( AIController(Controller) != None )
	{
		checkpoint = Acceleration;
		checkpoint.Z = 0.0;
	}
	if ( checkpoint == vect(0,0,0))
	checkpoint = vector(Rotation);
	checkpoint.Z = 0.0;
	checkNorm = Normal(checkpoint);
	checkPoint = Location + 1.2 * CylinderComponent.CollisionRadius * checkNorm;
	Extent = CylinderComponent.CollisionRadius * vect(1,1,0);
	Extent.Z = CylinderComponent.CollisionHeight;
	HitActor = Trace(HitLocation, HitNormal, checkpoint, Location, true, Extent);
	if ( (HitActor != None) && (Pawn(HitActor) == None) )
	{
		WallNormal = -1 * HitNormal;
		start = Location;
		start.Z += 1.1 * MaxStepHeight;
		checkPoint = start + 2 * CylinderComponent.CollisionRadius * checkNorm;
		HitActor = Trace(HitLocation, HitNormal, checkpoint, start, true);
		if ( (HitActor == None) || (HitNormal.Z > 0.7) )
			return true;
	}

	return false;
}

function UpdateRocketAcceleration(float DeltaTime, float YawChange, float PitchChange);

//Player Jumped
function bool DoJump( bool bUpdating )
{
	if ( !bIsCrouched && !bWantsToCrouch && ((Physics == PHYS_Walking) || (Physics == PHYS_Ladder) || (Physics == PHYS_Spider)) )
	{
		if ( Role == ROLE_Authority )
		{
			if ( (Level.Game != None) && (Level.Game.GameDifficulty > 2) )
				MakeNoise(0.1 * Level.Game.GameDifficulty);
			if ( bCountJumps && (InvManager != None) )
				InvManager.OwnerEvent('Jumped');
		}
		if ( Physics == PHYS_Spider )
			Velocity = JumpZ * Floor;
		else if ( Physics == PHYS_Ladder )
			Velocity.Z = 0;
		else if ( bIsWalking )
			Velocity.Z = Default.JumpZ;
		else
			Velocity.Z = JumpZ;
		if ( (Base != None) && !Base.bWorldGeometry )
			Velocity.Z += Base.Velocity.Z;
		SetPhysics(PHYS_Falling);
        return true;
	}
    return false;
}

function PlayDyingSound();

function PlayHit(float Damage, Pawn InstigatedBy, vector HitLocation, class<DamageType> damageType, vector Momentum)
{
	if ( (Damage <= 0) && ((Controller == None) || !Controller.bGodMode) )
		return;

	if ( Health <= 0 )
	{
		if ( PhysicsVolume.bDestructive && (WaterVolume(PhysicsVolume) != None) && (WaterVolume(PhysicsVolume).ExitActor != None) )
			Spawn(WaterVolume(PhysicsVolume).ExitActor);
		return;
	}

	if ( Level.TimeSeconds - LastPainTime > 0.1 )
	{
		PlayTakeHit(HitLocation,Damage,damageType);
		LastPainTime = Level.TimeSeconds;
	}
}

/*
Pawn was killed - detach any controller, and die
*/

// blow up into little pieces (implemented in subclass)

simulated function ChunkUp( Rotator HitRotation, class<DamageType> D )
{
	log("Pawn::ChunkUp");
	if ( (Level.NetMode != NM_Client) && (Controller != None) )
	{
		DetachFromController( true );
	}
	Destroy();
}

simulated function TurnOff()
{
// FIXMESTEVE - turn off sounds, animation, and stop weapon firing here
	SetCollision(true,false);
    Velocity = vect(0,0,0);
    SetPhysics(PHYS_None);
    bIgnoreForces = true;
}

State Dying
{
ignores Bump, HitWall, HeadVolumeChange, PhysicsVolumeChange, Falling, BreathTimer;

	event ChangeAnimation(AnimNode Node) {}
	function PlayFiring(float Rate, name FiringMode) {}
	simulated function PlayWeaponSwitch(Weapon OldWeapon, Weapon NewWeapon) {}
	function PlayTakeHit(vector HitLoc, int Damage, class<DamageType> damageType) {}
	simulated function PlayNextAnimation() {}

	function Died(Controller Killer, class<DamageType> damageType, vector HitLocation);

	function Timer()
	{
		if ( !PlayerCanSeeMe() )
		{
			// if we're still attached to a controller, leave it
			//DetachFromController();
			Destroy();
		}
		else
		{
			SetTimer(2.0, false);
		}
	}

	function Landed(vector HitNormal)
	{
		local rotator finalRot;

		if( Velocity.Z < -500 )
			TakeDamage( (1-Velocity.Z/30), Instigator, Location, vect(0,0,0), class'DmgType_Crushed');

		finalRot = Rotation;
		finalRot.Roll = 0;
		finalRot.Pitch = 0;
		setRotation(finalRot);
		SetPhysics(PHYS_None);
		SetCollision(true, false);
	}

	singular function BaseChange()
	{
		if( base == None )
			SetPhysics(PHYS_Falling);
		else if ( Pawn(base) != None ) // don't let corpse ride around on someone's head
        	ChunkUp( Rotation, class'DmgType_Fell' );
	}

	function TakeDamage( int Damage, Pawn instigatedBy, Vector hitlocation,
							Vector momentum, class<DamageType> damageType, optional TraceHitInfo HitInfo)
	{
		SetPhysics(PHYS_Falling);

		if ( (Physics == PHYS_None) && (Momentum.Z < 0) )
			Momentum.Z *= -1;

		Velocity += 3 * momentum/(Mass + 200);

		if ( bInvulnerableBody )
			return;

		Damage *= DamageType.default.GibModifier;
		Health -=Damage;
//FIXME		if ( ((Damage > 30) || !Mesh.IsAnimating()) && (Health < -80) )
//        	ChunkUp( Rotation, DamageType );
	}

	function BeginState()
	{
		local int i;

		if ( (LastStartSpot != None) && (LastStartTime - Level.TimeSeconds < 6) )
			LastStartSpot.LastSpawnCampTime = Level.TimeSeconds;

		if ( bTearOff && (Level.NetMode == NM_DedicatedServer) )
			LifeSpan = 2.0;
		else
			SetTimer(5.0, false);
		
		if ( Physics != PHYS_Articulated )
		{
			SetPhysics(PHYS_Falling);
		}

		SetCollision(true, false);
		bInvulnerableBody = true;
		
		if ( Controller != None )
		{
			if ( Controller.bIsPlayer )
			{
				DetachFromController();
			}
			else
			{
				Controller.Destroy();
			}
		}

		for (i = 0; i < Attached.length; i++)
			if (Attached[i] != None)
				Attached[i].PawnBaseDied();
	}

Begin:
	Sleep(0.2);
	bInvulnerableBody = false;
	PlayDyingSound();
}

//=============================================================================
// Animation interface for controllers

/* PlayXXX() function called by controller to play transient animation actions
*/
/* PlayDying() is called on server/standalone game when killed
and also on net client when pawn gets bTearOff set to true (and bPlayedDeath is false)
*/ 
simulated event PlayDying(class<DamageType> DamageType, vector HitLoc)
{
	GotoState('Dying');
	bReplicateMovement = false;
	bTearOff = true;
	Velocity += TearOffMomentum;
	SetPhysics(PHYS_Falling);
	bPlayedDeath = true;
}

simulated function PlayFiring(float Rate, name FiringMode);

function PlayTakeHit(vector HitLoc, int Damage, class<DamageType> damageType);

/* PlayFootStepSound()
called by AnimNotify_Footstep

FootDown specifies which foot hit
*/
event PlayFootStepSound(int FootDown);

//=============================================================================
// Pawn internal animation functions

simulated event ChangeAnimation(AnimNode Node)
{
}

// Animation group checks (usually implemented in subclass)

function bool CannotJumpNow()
{
	return false;
}

function PlayLanded(float impactVel);

function HoldCarriedObject(CarriedObject O, name AttachmentBone);

// JAG_COLRADIUS_HACK (moved from Actor)
function vector GetCollisionExtent()
{
	local vector Extent;

	Extent = CylinderComponent.CollisionRadius * vect(1,1,0);
	Extent.Z = CylinderComponent.CollisionHeight;
	return Extent;
}

// Mesh Transform helpers.

simulated event SetMeshTransformOffset(vector NewOffset)
{
	if (MeshTransform!=None)
    	MeshTransform.SetTranslation(NewOffset);
}

simulated event SetMeshTransformRotation(Rotator NewRotation)
{
	if (MeshTransform!=None)
    	MeshTransform.SetRotation(NewRotation);
}

simulated event SetMeshTransformScale(float NewScale)
{
	if (MeshTransform!=None)
    	MeshTransform.SetScale(NewScale);
}

simulated event SetMeshTransformScale3D(vector NewScale)
{
	if (MeshTransform!=None)
    	MeshTransform.SetScale3D(NewScale);
}

function float CollisionHeight()
{
	if (CylinderComponent!=None)
    	return CylinderComponent.CollisionHeight;
    else
    	return -1;

}

function Suicide()
{
	KilledBy(self);
}

function float CollisionRadius()
{
	if (CylinderComponent!=None)
    	return CylinderComponent.CollisionRadius;
    else
    	return -1;
}

// toss out a weapon

// check before throwing
simulated function bool CanThrowWeapon()
{
    return ( (Weapon != None) && Weapon.CanThrow() );
}

//***************************************
// Vehicle driving
// StartDriving() and StopDriving() also called on client
// on transitions of bIsDriving setting

simulated event StartDriving(Vehicle V)
{
	DrivenVehicle = V;
	NetUpdateTime = Level.TimeSeconds - 1;

	// Move the driver into position, and attach to car.
	ShouldCrouch(false);
	bIgnoreForces	= true;
	Velocity		= vect(0,0,0);
	Acceleration	= vect(0,0,0);
	bCanTeleport	= false;

	//if ( !V.bRemoteControlled || V.bHideRemoteDriver )
    //{
		SetCollision( false, false);
		bCollideWorld = false;

		if(V.bAttachDriver)
		{
			V.AttachDriver( Self );
		}

	   	bHidden = true;
    //}

	//TODO: animation support for riding in vehicles
}

simulated event StopDriving(Vehicle V)
{
	NetUpdateTime = Level.TimeSeconds - 1;

	DrivenVehicle	= None;
	bIgnoreForces	= false;
	bHardAttach		= false;
	bCanTeleport	= true;
	bCollideWorld	= true;

	if ( V != None )
		V.DetachDriver( Self );

	SetCollision(true, true);

	if ( (Role == ROLE_Authority) && (Health > 0) )
	{
		Acceleration = vect(0, 0, 24000);
		SetPhysics(PHYS_Falling);
		SetBase(None);
		bHidden = false;
	}
}

//
// Inventory related functions
//

/* AddDefaultInventory:
	Add Pawn default Inventory.
	Called from GameInfo.AddDefaultInventory()
*/
function AddDefaultInventory();

/* epic ===============================================
* ::CreateInventory
*
* Create Inventory Item, adds it to the Pawn's Inventory
* And returns it for post processing.
*
* =====================================================
*/
event final Inventory CreateInventory( class<Inventory> NewInvClass )
{
	if ( InvManager != None )
		return InvManager.CreateInventory( NewInvClass );

	return None;
}

/* FindInventoryType:
	returns the inventory item of the requested class if it exists in this Pawn's inventory
*/
simulated final function Inventory FindInventoryType( class<Inventory> DesiredClass )
{
	if ( InvManager != None )
		return InvManager.FindInventoryType( DesiredClass );

	return None;
}

/** Hook called from HUD actor. Gives access to HUD and Canvas */
simulated function DrawHUD( HUD H )
{
	if ( InvManager != None )
	{
		InvManager.DrawHUD( H );
	}
}

// toss out a weapon
function TossWeapon(Vector TossVel)
{
	local Vector X,Y,Z;

	GetAxes(Rotation,X,Y,Z);
	Weapon.DropFrom(Location + 0.8 * CylinderComponent.CollisionRadius * X - 0.5 * CylinderComponent.CollisionRadius * Y, TossVel);
}

/* SetActiveWeapon
	Set this weapon as the Pawn's active weapon
*/
simulated function SetActiveWeapon( Weapon NewWeapon )
{	
	if ( InvManager != None )
	{
		InvManager.SetCurrentWeapon( NewWeapon );
	}
}



/**
 * Player just changed weapon
 * Network: Server, Local Player, Clients
 *
 * @param	OldWeapon	Old weapon held by Pawn.
 * @param	NewWeapon	New weapon held by Pawn.
 */
simulated event PlayWeaponSwitch(Weapon OldWeapon, Weapon NewWeapon);

// Cheats - invoked by CheatManager
function bool CheatWalk()
{
	UnderWaterTime = Default.UnderWaterTime;
	SetCollision(true, true);
	SetPhysics(PHYS_Walking);
	bCollideWorld = true;
	SetPushesRigidBodies(Default.bPushesRigidBodies);
	return true;
}

function bool CheatGhost()
{
	UnderWaterTime = -1.0;
	SetCollision(false, false);
	bCollideWorld = false;
	SetPushesRigidBodies(false);
	return true;
}

function bool CheatFly()
{
	UnderWaterTime = Default.UnderWaterTime;
	SetCollision(true, true);
	bCollideWorld = true;
	return true;
}

defaultproperties
{
	InventoryManagerClass=class'InventoryManager'
	ControllerClass=class'AIController'

	ViewPitchMin=-11000
	ViewPitchMax=11000
	bCanBeDamaged=true
	bJumpCapable=true
	bCanJump=true
	bCanWalk=true
	bCanSwim=false
	bCanFly=false
	bUpdateSimulatedPosition=true
	BaseEyeHeight=+00064.000000
	EyeHeight=+00054.000000
	GroundSpeed=+00600.000000
	AirSpeed=+00600.000000
	WaterSpeed=+00300.000000
	AccelRate=+02048.000000
	JumpZ=+00420.000000
	MaxFallSpeed=+1200.0
	bLOSHearing=true
	HearingThreshold=+2800.0
	Health=100
     HealthMax=100
	Visibility=128
	LadderSpeed=+200.0
	noise1time=-00010.000000
	noise2time=-00010.000000
	AvgPhysicsTime=+00000.100000
	SoundDampening=+00001.000000
	DamageScaling=+00001.000000
	bCanTeleport=true
	bStasis=true
	bCollideActors=true
	bCollideWorld=true
	bBlockActors=true
	bProjTarget=true
	bCanCrouch=false
	RotationRate=(Pitch=4096,Yaw=20000,Roll=3072)
	RemoteRole=ROLE_SimulatedProxy
	NetPriority=+00002.000000
	AirControl=+0.05
	MaxDesiredSpeed=+00001.000000
	DesiredSpeed=+00001.000000
	LandMovementState=PlayerWalking
	WaterMovementState=PlayerSwimming
	SightRadius=+05000.000000
	WalkingPct=+0.5
	CrouchedPct=+0.5
	bTravel=true
	bShouldBaseAtStartup=true
    Bob=0.0060
	bWeaponBob=true
	bUseCompressedPosition=true
	 bSimulateGravity=true
	HeadScale=+1.0

	bPushesRigidBodies=true
	RBPushRadius=10.0
	RBPushStrength=50.0

	CrouchHeight=+40.0
	CrouchRadius=+34.0
	Begin Object Name=CollisionCylinder
		CollisionRadius=+0034.000000
		CollisionHeight=+0078.000000
		BlockNonZeroExtent=true
		BlockZeroExtent=true
		BlockActors=true
		CollideActors=true
	End Object
	CylinderComponent=CollisionCylinder

	Begin Object Class=ArrowComponent Name=Arrow
		ArrowColor=(R=150,G=200,B=255)
	End Object
	Components.Add(Arrow)

	MaxStepHeight=35.f
	MaxJumpHeight=96.f

	MoveThresholdScale=1.f
}