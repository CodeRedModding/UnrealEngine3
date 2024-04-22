//=============================================================================
// Vehicle: The base class of all vehicles.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class Vehicle extends Pawn
	ClassGroup(Vehicles)
    native(Pawn)
    nativereplication
    abstract;

/** Pawn driving this vehicle. */
var	repnotify	Pawn	Driver;

/** true if vehicle is being driven. */
var  repnotify  bool    bDriving;

/** Positions (relative to vehicle) to try putting the player when exiting. Optional -
automatic system for determining exitpositions if none is specified. */
var()	Array<Vector>	ExitPositions;

/** Radius for automatic exit positions. */
var				float					ExitRadius;

/** Offset from center for Exit test circle. */
var				vector					ExitOffset;

/** whether to render driver seated in vehicle */
var	bool	bDriverIsVisible;

// generic controls (set by controller, used by concrete derived classes)
var ()	float			Steering; // between -1 and 1
var ()	float			Throttle; // between -1 and 1
var ()	float			Rise;	  // between -1 and 1

/** If true, attach the driver to the vehicle when he starts using it. */
var		bool			bAttachDriver;


/** Adjust position that NPCs should aim at when firing at this vehicle */
var vector TargetLocationAdjustment;

/** damage to the driver is multiplied by this value */
var float DriverDamageMult;

/** damage momentum multiplied by this value before being applied to vehicle */
var() float MomentumMult;

var class<DamageType>	CrushedDamageType;

/** If going less than this speed, don't crush the pawn. */
var float				MinCrushSpeed;

/** If this vehicle penetrates more than this, even if going less than MinCrushSpeed, crush the pawn. */
var float				ForceCrushPenetration;

/** AI control */
var		bool	bTurnInPlace;			// whether vehicle can turn in place
var		bool	bSeparateTurretFocus;	// hint for AI (for tank type turreted vehicles)
var		bool	bFollowLookDir;			// used by AI to know that controller's rotation determines vehicle rotation
var		bool	bHasHandbrake;			// hint for AI
var		bool	bScriptedRise;			// hint for AI
var		bool	bDuckObstacles;			// checks for and ducks under obstacles

/** if set, AI avoids going in reverse unless it has to */
var		bool	bAvoidReversing;

var		byte	StuckCount;				// used by AI

var float		ThrottleTime;			/** last time at which throttle was 0 (used by AI) */
var float		StuckTime;
/** steering value used last tick */
var float OldSteering;
/** when AI started using only steering (so it doesn't get stuck doing that when it isn't working) */
var float OnlySteeringStartTime;

/** Used by AI during three point turns, to make sure it doesn't get into a state where the throttle is reversed every tick */
var float		OldThrottle;

var const float AIMoveCheckTime;
var float		VehicleMovingTime; // used by AI C++
var float		TurnTime;			/** AI hint - how long it takes to turn vehicle 180 degrees (for setting MoveTimer) */

/** if set and pathfinding fails, retry with vehicle driver - ContinueOnFoot() will be called when AI can't go any further in vehicle */
var bool bRetryPathfindingWithDriver;

/** TRUE for vehicle to ignore the StallZ value, FALSE to respect it normally */
var() bool		bIgnoreStallZ;

/** If true, do extra traces to vehicle extremities for net relevancy checks */
var bool bDoExtraNetRelevancyTraces;

cpptext
{
	virtual INT* GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel);
	virtual UBOOL IsNetRelevantFor(APlayerController* RealViewer, AActor* Viewer, const FVector& SrcLocation);
	virtual UBOOL ReachedBy(APawn* P, const FVector& TestPosition, const FVector& Dest);
	virtual ANavigationPoint* CheckDetour(ANavigationPoint* BestDest, ANavigationPoint* Start, UBOOL bWeightDetours);
	virtual void performPhysics(FLOAT DeltaSeconds);
	virtual UBOOL HasRelevantDriver();
	virtual AVehicle* GetAVehicle() { return this; }

	/**
	 * Check if this actor is the owner when doing relevancy checks for actors marked bOnlyRelevantToOwner
	 * 
	 * @param ReplicatedActor - the actor we're doing a relevancy test on
	 * 
	 * @param ActorOwner - the owner of ReplicatedActor
	 * 
	 * @param ConnectionActor - the controller of the connection that we're doing relevancy checks for
	 * 
	 * @return TRUE if this actor should be considered the owner
	 */
	virtual UBOOL IsRelevancyOwnerFor(AActor* ReplicatedActor, AActor* ActorOwner, AActor* ConnectionActor);

	// AI Interface
	virtual void setMoveTimer(FVector MoveDir);
	virtual UBOOL IsStuck();
	virtual UBOOL AdjustFlight(FLOAT ZDiff, UBOOL bFlyingDown, FLOAT Distance, AActor* GoalActor);
	virtual void SteerVehicle(FVector Direction);
	virtual void AdjustThrottle( FLOAT Distance );
	virtual UBOOL moveToward(const FVector &Dest, AActor *GoalActor);
	virtual void rotateToward(FVector FocalPoint);
	virtual UBOOL JumpOutCheck(AActor *GoalActor, FLOAT Distance, FLOAT ZDiff);
	virtual void MarkEndPoints(ANavigationPoint* EndAnchor, AActor* Goal, const FVector& GoalLocation);
	virtual FLOAT SecondRouteAttempt(ANavigationPoint* Anchor, ANavigationPoint* EndAnchor, NodeEvaluator NodeEval, FLOAT BestWeight, AActor *goal, const FVector& GoalLocation, FLOAT StartDist, FLOAT EndDist, INT MaxPathLength, INT SoftMaxNodes);
	virtual UBOOL IsGlider();
}

replication
{
	if (bNetDirty && Role == ROLE_Authority)
		bDriving;
	if (bNetDirty && (bNetOwner || Driver == None || !Driver.bHidden) && Role == ROLE_Authority)
		Driver;
}

/** NotifyTeamChanged()
Called when PlayerReplicationInfo is replicated to this pawn, or PlayerReplicationInfo team property changes.

Network:  client
*/
simulated function NotifyTeamChanged()
{
	// notify driver as well
	if ( (PlayerReplicationInfo != None) && (Driver != None) )
		Driver.NotifyTeamChanged();
}

/** @See Actor::DisplayDebug */
simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local string DriverText;

	super.DisplayDebug(HUD, out_YL, out_YPos );

	HUD.Canvas.SetDrawColor(255,255,255);
	HUD.Canvas.DrawText("Steering "$Steering$" throttle "$Throttle$" rise "$Rise);
	out_YPos += out_YL;
	HUD.Canvas.SetPos(4,out_YPos);

	HUD.Canvas.SetDrawColor(255,0,0);


	out_YPos += out_YL;
	HUD.Canvas.SetPos(4,out_YPos);
	if ( Driver == None )
	{
		DriverText = "NO DRIVER";
	}
	else
	{
		DriverText = "Driver Mesh "$Driver.Mesh$" hidden "$Driver.bHidden;
	}
	HUD.Canvas.DrawText(DriverText);
	out_YPos += out_YL;
	HUD.Canvas.SetPos(4,out_YPos);
}

function Suicide()
{
	if ( Driver != None )
		Driver.KilledBy(Driver);
	else
		KilledBy(self);
}

/**
  * @RETURNS max rise force for this vehicle (used by AI)
  */
native function float GetMaxRiseForce();


/**
 * @returns Figure out who we are targetting.
 */
simulated native function vector GetTargetLocation(optional Actor RequestedBy, optional bool bRequestAlternateLoc) const;


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
	if ( Role == ROLE_Authority )
	{
		Super.TakeRadiusDamage(InstigatedBy, BaseDamage, DamageRadius, DamageType, Momentum, HurtOrigin, bFullDamage, DamageCauser, DamageFalloffExponent);

		if (Health > 0)
		{
			DriverRadiusDamage(BaseDamage, DamageRadius, InstigatedBy, DamageType, Momentum, HurtOrigin, DamageCauser);
		}
	}
}

/** DriverRadiusDamage()
determine if radius damage that hit the vehicle should damage the driver
*/
function DriverRadiusDamage(float DamageAmount, float DamageRadius, Controller EventInstigator, class<DamageType> DamageType, float Momentum, vector HitLocation, Actor DamageCauser, optional float DamageFalloffExponent=1.f)
{
	//if driver has collision, whatever is causing the radius damage will hit the driver by itself
	if (EventInstigator != None && Driver != None && bAttachDriver && !Driver.bCollideActors && !Driver.bBlockActors)
	{
		Driver.TakeRadiusDamage(EventInstigator, DamageAmount, DamageRadius, DamageType, Momentum, HitLocation, false, DamageCauser, DamageFalloffExponent);
	}
}

function PlayerChangedTeam()
{
	if ( Driver != None )
		Driver.KilledBy(Driver);
	else
		Super.PlayerChangedTeam();
}

simulated function SetBaseEyeheight()
{
	BaseEyeheight = Default.BaseEyeheight;
	Eyeheight = BaseEyeheight;
}

event PostBeginPlay()
{
	super.PostBeginPlay();

	if ( !bDeleteMe )
	{
		AddDefaultInventory();
	}
}

function bool CheatWalk()
{
	return false;
}

function bool CheatGhost()
{
	return false;
}

function bool CheatFly()
{
	return false;
}

simulated event Destroyed()
{
	if ( Driver != None )
		Destroyed_HandleDriver();

	super.Destroyed();
}

simulated function Destroyed_HandleDriver()
{
	local Pawn		OldDriver;

	Driver.LastRenderTime = LastRenderTime;
	if ( Role == ROLE_Authority )
	{
		OldDriver = Driver;
		Driver = None;
		OldDriver.DrivenVehicle = None;
		OldDriver.Destroy();
	}
	else if ( Driver.DrivenVehicle == self )
		Driver.StopDriving(self);
}

/** CanEnterVehicle()
return true if Pawn P is allowed to enter this vehicle
*/
function bool CanEnterVehicle(Pawn P)
{
	return ( !bDeleteMe && AnySeatAvailable() && (!bAttachDriver || !P.bIsCrouched) && P.DrivenVehicle == None &&
		P.Controller != None && P.Controller.bIsPlayer && !P.IsA('Vehicle') && Health > 0 );
}

/** SeatAvailable()
returns true if a seat is available for a pawn
*/
function bool AnySeatAvailable()
{
	return (Driver == none);
}

/** TryToDrive()
returns true if Pawn P successfully became driver of this vehicle
*/
function bool TryToDrive(Pawn P)
{
	if( !CanEnterVehicle( P ) )
	{
		return FALSE;
	}

	return DriverEnter( P );
}

/** DriverEnter()
 * Make Pawn P the new driver of this vehicle
 * Changes controller ownership across pawns
 */
function bool DriverEnter(Pawn P)
{
	local Controller C;

	// Set pawns current controller to control the vehicle pawn instead
	C = P.Controller;
	Driver = P;
	Driver.StartDriving( self );
	if ( Driver.Health <= 0 )
	{
		Driver = None;
		return false;
	}
	SetDriving(true);

	// Disconnect PlayerController from Driver and connect to Vehicle.
	C.Unpossess();
	Driver.SetOwner( Self ); // This keeps the driver relevant.
	C.Possess( Self, true );

	if( PlayerController(C) != None && !C.IsChildState(C.GetStateName(), LandMovementState) )
	{
		PlayerController(C).GotoState( LandMovementState );
	}

	WorldInfo.Game.DriverEnteredVehicle(self, P);
	return true;
}

function PossessedBy(Controller C, bool bVehicleTransition)
{
	super.PossessedBy( C, bVehicleTransition );

	EntryAnnouncement(C);
	NetPriority = 3;
	NetUpdateFrequency = 100;
	ThrottleTime = WorldInfo.TimeSeconds;
	OnlySteeringStartTime = WorldInfo.TimeSeconds;
}

/* EntryAnnouncement() - Called when Controller possesses vehicle, for any visual/audio effects
*/
function EntryAnnouncement(Controller C);

/**
 * Attach driver to vehicle.
 * Sets up the Pawn to drive the vehicle (rendering, physics, collision..).
 * Called only if bAttachDriver is true.
 * Network : ALL
 */
simulated function AttachDriver( Pawn P )
{
	if( !bAttachDriver )
		return;

	P.SetCollision( false, false);
	P.bCollideWorld = false;
	P.SetBase(none);
	P.SetHardAttach(true);
	P.SetPhysics( PHYS_None );

	if ( (P.Mesh != None) && (Mesh != None) )
		P.Mesh.SetShadowParent(Mesh);

	if ( !bDriverIsVisible )
	{
		P.SetHidden(True);
		P.SetLocation( Location );
	}
	P.SetBase(self);
	// need to set PHYS_None again, because SetBase() changes physics to PHYS_Falling
	P.SetPhysics( PHYS_None );
}

/**
 * Detach Driver from vehicle.
 * Network : ALL
 */
simulated function DetachDriver( Pawn P )
{
}

/**
ContinueOnFoot() - used by AI
Called from route finding if route can only be continued on foot.
Returns true if driver left vehicle */
event bool ContinueOnFoot()
{
	if (AIController(Controller) != None)
	{
		return DriverLeave(false);
	}
	else
	{
		return false;
	}
}

function rotator GetExitRotation(Controller C)
{
	local rotator rot;
	rot.Yaw = C.Rotation.Yaw;
	return rot;
}

/**
Called from the Controller when player wants to get out. */
event bool DriverLeave( bool bForceLeave )
{
	local Controller		C;
	local PlayerController	PC;
	local Rotator ExitRotation;

	if (Role < ROLE_Authority)
	{
		`Warn("DriverLeave() called on client");
		ScriptTrace();
		return false;
	}

	if( !bForceLeave && !WorldInfo.Game.CanLeaveVehicle(self, Driver) )
	{
		return false;
	}

	// Do nothing if we're not being driven
	if ( Controller == None )
	{
		return false;
	}

	// Before we can exit, we need to find a place to put the driver.
	// Iterate over array of possible exit locations.
	if ( Driver != None )
    {
	    Driver.SetHardAttach(false);
	    Driver.bCollideWorld = true;
	    Driver.SetCollision(true, true);

		if ( !PlaceExitingDriver() )
	    {
			if ( !bForceLeave )
			{
				// If we could not find a place to put the driver, leave driver inside as before.
			    Driver.SetHardAttach(true);
				Driver.bCollideWorld = false;
				Driver.SetCollision(false, false);
				return false;
			}
			else
			{
				Driver.SetLocation(GetTargetLocation());
			}
	    }
	}

	ExitRotation = GetExitRotation(Controller);
	SetDriving(False);

	// Reconnect Controller to Driver.
	C = Controller;
	if (C.RouteGoal == self)
	{
		C.RouteGoal = None;
	}
	if (C.MoveTarget == self)
	{
		C.MoveTarget = None;
	}
	Controller.UnPossess();

	if ( (Driver != None) && (Driver.Health > 0) )
	{
		Driver.SetRotation(ExitRotation);
		Driver.SetOwner( C );
		C.Possess( Driver, true );

		PC = PlayerController(C);
		if ( PC != None )
		{
			PC.ClientSetViewTarget( Driver ); // Set playercontroller to view the person that got out
		}

		Driver.StopDriving( Self );
	}

	if ( C == Controller )	// If controller didn't change, clear it...
	{
		Controller = None;
	}

	WorldInfo.Game.DriverLeftVehicle(self, Driver);

	// Vehicle now has no driver
	DriverLeft();
	return true;
}

simulated function SetInputs(float InForward, float InStrafe, float InUp)
{
	Throttle = InForward;
	Steering = InStrafe;
	Rise = InUp;
}

// DriverLeft() called by DriverLeave()
function DriverLeft()
{
	Driver = None;
	SetDriving(false);
}

/** PlaceExitingDriver()
Find an acceptable position to place the exiting driver pawn, and move it there.
Returns true if pawn was successfully placed.
*/
function bool PlaceExitingDriver(optional Pawn ExitingDriver)
{
	local int i;
	local vector tryPlace, Extent, HitLocation, HitNormal, ZOffset;

	if ( ExitingDriver == None )
		ExitingDriver = Driver;

	if ( ExitingDriver == None )
		return false;

	Extent = ExitingDriver.GetCollisionRadius() * vect(1,1,0);
	Extent.Z = ExitingDriver.GetCollisionHeight();
	ZOffset = Extent.Z * vect(0,0,1);

	if ( ExitPositions.Length > 0 )
	{
		// specific exit positions specified
		for( i=0; i<ExitPositions.Length; i++)
		{
			if ( ExitPositions[0].Z != 0 )
				ZOffset = Vect(0,0,1) * ExitPositions[0].Z;
			else
				ZOffset = ExitingDriver.CylinderComponent.default.CollisionHeight * vect(0,0,2);

			tryPlace = Location + ( (ExitPositions[i]-ZOffset) >> Rotation) + ZOffset;

			// First, do a line check (stops us passing through things on exit).
			if( Trace(HitLocation, HitNormal, tryPlace, Location + ZOffset, false, Extent) != None )
				continue;

			// Then see if we can place the player there.
			if ( !ExitingDriver.SetLocation(tryPlace) )
				continue;

			return true;
		}
	}
	else
	{
		return FindAutoExit(ExitingDriver);
	}
	return false;
}

/** FindAutoExit(Pawn ExitingDriver)
Tries to find exit position on either side of vehicle, in back, or in front
returns true if driver successfully exited. */
function bool FindAutoExit(Pawn ExitingDriver)
{
	local vector FacingDir, CrossProduct;
	local float PlaceDist;

	FacingDir = vector(Rotation);
	CrossProduct = Normal(FacingDir cross vect(0,0,1));

	if ( ExitRadius == 0 )
	{
		ExitRadius = GetCollisionRadius() + ExitingDriver.VehicleCheckRadius;
	}
	PlaceDist = ExitRadius + ExitingDriver.GetCollisionRadius();

	return (	TryExitPos(ExitingDriver, GetTargetLocation() + ExitOffset + PlaceDist * CrossProduct, false)
			||	TryExitPos(ExitingDriver, GetTargetLocation() + ExitOffset - PlaceDist * CrossProduct, false)
			||	TryExitPos(ExitingDriver, GetTargetLocation() + ExitOffset - PlaceDist * FacingDir, false)
			||	TryExitPos(ExitingDriver, GetTargetLocation() + ExitOffset + PlaceDist * FacingDir, false) );
}

/* TryExitPos()
Used by PlaceExitingDriver() to evaluate automatically generated exit positions
*/
function bool TryExitPos(Pawn ExitingDriver, vector ExitPos, bool bMustFindGround)
{
	local vector Slice, HitLocation, HitNormal, StartLocation, NewActorPos;
	local actor HitActor;

	//DrawDebugBox(ExitPos, ExitingDriver.GetCollisionExtent(), 255,255,0, TRUE);

	Slice = ExitingDriver.GetCollisionRadius() * vect(1,1,0);
	Slice.Z = 2;

	// First, do a line check (stops us passing through things on exit).
	StartLocation = GetTargetLocation();
	if( Trace(HitLocation, HitNormal, ExitPos, StartLocation, false, Slice) != None )
	{
		return false;
	}

	// Now trace down, to find floor
	HitActor = Trace(HitLocation, HitNormal, ExitPos - ExitingDriver.GetCollisionHeight()*vect(0,0,5), ExitPos, true, Slice);

	if ( HitActor == None )
	{
		if ( bMustFindGround )
		{
			return false;
		}
		HitLocation = ExitPos;
	}

	NewActorPos = HitLocation + (ExitingDriver.GetCollisionHeight()+ExitingDriver.MaxStepHeight)*vect(0,0,1);

	// Check this wont overlap this vehicle.
	if( PointCheckComponent(Mesh, NewActorPos, ExitingDriver.GetCollisionExtent()) )
	{
		return false;
	}

	// try placing driver on floor
	return ExitingDriver.SetLocation(NewActorPos);
}

function UnPossessed()
{
	NetPriority = default.NetPriority;		// restore original netpriority changed when possessing
	bForceNetUpdate = TRUE;
	NetUpdateFrequency	= 8;

	super.UnPossessed();
}

function controller SetKillInstigator(Controller InstigatedBy, class<DamageType> DamageType)
{
	return InstigatedBy;
}

event TakeDamage(int Damage, Controller EventInstigator, vector HitLocation, vector Momentum, class<DamageType> DamageType, optional TraceHitInfo HitInfo, optional Actor DamageCauser)
{
	bForceNetUpdate = TRUE; // force quick net update

	if ( DamageType != None )
	{
		Damage *= DamageType.static.VehicleDamageScalingFor(self);
		momentum *= DamageType.default.VehicleMomentumScaling * MomentumMult;
	}

	super.TakeDamage(Damage, EventInstigator, HitLocation, Momentum, DamageType, HitInfo, DamageCauser);
}

function AdjustDriverDamage(out int Damage, Controller InstigatedBy, Vector HitLocation, out Vector Momentum, class<DamageType> DamageType)
{
	if ( InGodMode() )
	{
 		Damage = 0;
 	}
	else
	{
 		Damage *= DriverDamageMult;
 	}
}

function ThrowActiveWeapon( optional bool bDestroyWeap );

function bool Died(Controller Killer, class<DamageType> DamageType, vector HitLocation)
{
	if (Super.Died(Killer, DamageType, HitLocation))
	{
		SetDriving(false);
		return true;
	}
	else
	{
		return false;
	}
}

function DriverDied(class<DamageType> DamageType)
{
	local Controller C;
	local PlayerReplicationInfo RealPRI;

	if ( Driver == None )
		return;

	WorldInfo.Game.DiscardInventory( Driver );

	C = Controller;
	Driver.StopDriving( Self );
	Driver.Controller = C;
	Driver.DrivenVehicle = Self; //for in game stats, so it knows pawn was killed inside a vehicle

	if ( Controller == None )
		return;

	if ( PlayerController(Controller) != None )
	{
		Controller.SetLocation( Location );
		PlayerController(Controller).SetViewTarget( Driver );
		PlayerController(Controller).ClientSetViewTarget( Driver );
	}

	Controller.Unpossess();
	if ( Controller == C )
		Controller = None;
	C.Pawn = Driver;

	// make sure driver has PRI temporarily
	RealPRI = Driver.PlayerReplicationInfo;
	if ( RealPRI == None )
	{
		Driver.PlayerReplicationInfo = C.PlayerReplicationInfo;
	}
	WorldInfo.Game.DriverLeftVehicle(self, Driver);
	Driver.PlayerReplicationInfo = RealPRI;

	// Car now has no driver
	DriverLeft();
}

/* PlayDying() is called on server/standalone game when killed
and also on net client when pawn gets bTearOff set to true (and bPlayedDeath is false)
*/
simulated function PlayDying(class<DamageType> DamageType, vector HitLoc)
{}

simulated function name GetDefaultCameraMode( PlayerController RequestedBy )
{
	if ( (RequestedBy != None) && (RequestedBy.PlayerCamera != None) && (RequestedBy.PlayerCamera.CameraStyle == 'Fixed') )
		return 'Fixed';

	return 'ThirdPerson';
}

/** Vehicles ignore 'face rotation'. */
simulated function FaceRotation( rotator NewRotation, float DeltaTime ) {}

/** Vehicles dont get telefragged. */
event EncroachedBy(Actor Other) {}

/** @return the Controller that should receive credit for damage caused by this vehicle colliding with others */
function Controller GetCollisionDamageInstigator()
{
	if (Controller != None)
	{
		return Controller;
	}
	else
	{
		return (Instigator != None) ? Instigator.Controller : None;
	}
}

/** called when this Actor is encroaching on Other and we couldn't find an appropriate place to push Other to
 * @return true to abort the move, false to allow it
 * @warning do not abort moves of PHYS_RigidBody actors as that will cause the Unreal location and physics engine location to mismatch
 */
event bool EncroachingOn(Actor Other)
{
	local Pawn P;
	local vector PushVelocity, CheckExtent;
	local bool bSlowEncroach;
	local bool bDeepEncroach;

	P = Pawn(Other);
	if ( P == None )
		return false;

	// See if we are moving slowly
	bSlowEncroach = (VSize(Velocity) < MinCrushSpeed);

	// If we are moving slowly, see how 'deeply' we are penetrating
	if(bSlowEncroach)
	{
		CheckExtent.X = P.CylinderComponent.CollisionRadius - ForceCrushPenetration;
		CheckExtent.Y = CheckExtent.X;
		CheckExtent.Z = P.CylinderComponent.CollisionHeight - ForceCrushPenetration;

		bDeepEncroach = PointCheckComponent(CollisionComponent, P.Location, CheckExtent);
	}

	if ( ((Other == Instigator) && !bDeepEncroach)  || (Vehicle(Other) != None) || Other.Role != ROLE_Authority || (!Other.bCollideActors && !Other.bBlockActors) || (bSlowEncroach && !bDeepEncroach))
	{
		if ( (P.Velocity Dot (Location - P.Location)) > 0 )
		{
			// push away other pawn
			PushVelocity = Normal(P.Location - Location) * 200;
			PushVelocity.Z = 100;
			P.AddVelocity(PushVelocity, Location, CrushedDamageType);
		}
		return false;
	}

	if (P.Base == self)
	{
		// try pushing pawn off first
		RanInto(P);
		if (P.Base != None)
		{
			P.JumpOffPawn();
		}
		if (P.Base == None)
		{
			return false;
		}
	}

	// If its a non-vehicle pawn, do lots of damage.
	PancakeOther(P);
	return false;
}

/** Crush the pawn vehicle is encroaching */
function PancakeOther(Pawn Other)
{
	Other.TakeDamage(10000, GetCollisionDamageInstigator(), Other.Location, Velocity * Other.Mass, CrushedDamageType);
}

/** CrushedBy()
Called for pawns that have bCanBeBaseForPawns=false when another pawn becomes based on them
*/
function CrushedBy(Pawn OtherPawn) {}

simulated event vector GetEntryLocation()
{
	return Location;
}

/*
 *   Change the driving status of the vehicle
 * replicates to clients and notifies via DrivingStatusChanged()
 * @param b - TRUE for actively driving the vehicle, FALSE otherwise
 */
simulated function SetDriving(bool b)
{
	if (bDriving != b)
	{
		bDriving = b;
		DrivingStatusChanged();
	}
}

simulated function DrivingStatusChanged()
{
	if (!bDriving)
	{
		// Put brakes on before driver gets out! :)
		Throttle = 0;
		Steering = 0;
		Rise = 0;
	}
}

simulated event ReplicatedEvent(name VarName)
{
	if (VarName == 'bDriving')
	{
		DrivingStatusChanged();
	}
	else if (VarName == 'Driver')
	{
		if ( (PlayerReplicationInfo != None) && (Driver != None) )
			Driver.NotifyTeamChanged();
	}
	else
	{
		Super.ReplicatedEvent(VarName);
	}
}

/** called when the driver of this vehicle takes damage */
function NotifyDriverTakeHit(Controller InstigatedBy, vector HitLocation, int Damage, class<DamageType> DamageType, vector Momentum);

simulated function ZeroMovementVariables()
{
	Super.ZeroMovementVariables();

	Steering = 0.f;
	Rise	 = 0.f;
	Throttle = 0.f;
}

defaultproperties
{
	Components.Remove(Arrow)
	Components.Remove(Sprite)

	LandMovementState=PlayerDriving
	bDontPossess=true

	bCanBeBaseForPawns=true
	MomentumMult=1.0
	bAttachDriver=true
	CrushedDamageType=class'DmgType_Crushed'
	TurnTime=2.0

	MinCrushSpeed=20.0
	ForceCrushPenetration=10.0
	bDoExtraNetRelevancyTraces=true
	bRetryPathfindingWithDriver=true
	bPathfindsAsVehicle=true
	bReplicateHealthToAll=true
}
