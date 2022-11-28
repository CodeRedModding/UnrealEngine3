//=============================================================================
// Vehicle: The base class of all vehicles.
//=============================================================================
// This is a built-in Unreal Engine class and it shouldn't be modified.
//=============================================================================

class Vehicle extends Pawn
    native
	config(Game)
    abstract;
    

var		Pawn	Driver;
var		bool	bDriving;
var		bool	bTeamLocked;		// Team defines which players are allowed to enter the vehicle

var()	Array<Vector>	ExitPositions;		// Positions (rel to vehicle) to try putting the player when exiting.

// generic controls (set by controller, used by concrete derived classes)
var ()	float			Steering; // between -1 and 1
var ()	float			Throttle; // between -1 and 1
var ()	float			Rise;	  // between -1 and 1

/** True if exit positions are relative offsets to vehicle's location, false if they are absolute world locations */
var		bool			bRelativeExitPos;

/** If true, attach the driver to the vehicle when he starts using it. */
var		bool			bAttachDriver;

var Vehicle NextVehicle;

cpptext
{
	virtual UBOOL IgnoreBlockingBy( const AActor *Other ) const;
	virtual UBOOL moveToward(const FVector &Dest, AActor *GoalActor);
	virtual void rotateToward(AActor *Focus, FVector FocalPoint);
	virtual int pointReachable(FVector aPoint, int bKnowVisible=0);
	virtual int actorReachable(AActor *Other, UBOOL bKnowVisible=0, UBOOL bNoAnchorCheck=0);
	UBOOL IsNetRelevantFor( APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation );
	virtual FVector GetDestination();
}

replication
{
	reliable if( Role==ROLE_Authority )
		ClientDriverEnter, ClientDriverLeave;

	unreliable if( bNetDirty && Role==ROLE_Authority )
        bDriving, Driver;
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
 */
function TakeRadiusDamage
( 
	Pawn				InstigatedBy, 
	float				BaseDamage, 
	float				DamageRadius, 
	class<DamageType>	DamageType, 
	float				Momentum, 
	vector				HurtOrigin 
)
{
	Super.TakeRadiusDamage(InstigatedBy, BaseDamage, DamageRadius, DamageType, Momentum, HurtOrigin);

	if ( (InstigatedBy != None) && (Health > 0) )
		DriverRadiusDamage(BaseDamage, DamageRadius, InstigatedBy.Controller, DamageType, Momentum, HurtOrigin);
}

//determine if radius damage that hit the vehicle should damage the driver
function DriverRadiusDamage(float DamageAmount, float DamageRadius, Controller EventInstigator, class<DamageType> DamageType, float Momentum, vector HitLocation)
{
	/* FIXMESTEVE
	local float damageScale, dist;
	local vector dir;

	//if driver has collision, whatever is causing the radius damage will hit the driver by itself
	if (EventInstigator == None || Driver == None || Driver.bCollideActors || bRemoteControlled)
		return;

	dir = Driver.Location - HitLocation;
	dist = FMax(1, VSize(dir));
	dir = dir/dist;
	damageScale = 1 - FMax(0,(dist - Driver.CollisionRadius)/DamageRadius);
	if (damageScale <= 0)
		return;

	Driver.SetDelayedDamageInstigatorController(EventInstigator);
	Driver.TakeDamage( damageScale * DamageAmount, EventInstigator.Pawn, Driver.Location - 0.5 * (Driver.CollisionHeight + Driver.CollisionRadius) * dir,
			   damageScale * Momentum * dir, DamageType );
   */
}

function PostBeginPlay()
{
	super.PostBeginPlay();
	
	if ( !bDeleteMe )
	{
		AddDefaultInventory();
		Level.Game.RegisterVehicle(self);
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

event Bump( Actor Other, Vector HitNormal )
{
	local Pawn OtherPawn;
	OtherPawn = Pawn(Other);

	if(OtherPawn != None)
	{
		TryToDrive(OtherPawn);
	}
}

function bool TryToDrive(Pawn P)
{
	log("Vehicle::TryToDrive P:" $ P.GetHumanReadableName() );
	if ( (Driver != None) || P.bIsCrouched || (P.DrivenVehicle != None) || (P.Controller == None) || !P.Controller.bIsPlayer
	     || P.IsA('Vehicle') || Health <= 0 )
		return false;

	DriverEnter( P );
	return true;
}

function DriverEnter(Pawn P)
{
	local Controller C;
	local PlayerController PC;

	log("Vehicle::DriverEnter P:" $ P.GetHumanReadableName() );
	bDriving = true;

	// Set pawns current controller to control the vehicle pawn instead
	C = P.Controller;
	Driver = P;
	Driver.StartDriving( Self );

	// Disconnect PlayerController from Driver and connect to Vehicle.
	C.Unpossess();
	Driver.SetOwner( Self ); // This keeps the driver relevant.
	C.Possess( Self );

	PC = PlayerController(C);
	if( PC != None )
	{
		PC.GotoState( LandMovementState );
	}
}

function PossessedBy(Controller C)
{
	local PlayerController PC;

	log("Vehicle::PossessedBy C:" $ C.GetHumanReadableName() );

	super.PossessedBy( C );

	NetPriority = 3;
	NetUpdateFrequency = 100;

	PC = PlayerController(C);
	if ( PC != None )
		ClientDriverEnter( PC );
}

simulated function ClientDriverEnter(PlayerController PC)
{
}

simulated function AttachDriver(Pawn P)
{ 
	P.bHardAttach = true;
	P.SetLocation( Location );
	P.SetPhysics( PHYS_None );
	P.SetBase( Self );
	P.SetRelativeRotation( rot(0,0,0) );
}

simulated function DetachDriver(Pawn P) {}

event bool DriverLeave( bool bForceLeave )
{
	local Controller		C;
	local PlayerController	PC;
	local bool				havePlaced;

	log("Vehicle::DriverLeave bForceLeave:" $ bForceLeave );

	// Do nothing if we're not being driven
	if ( Controller == None )
		return false;

	// Before we can exit, we need to find a place to put the driver.
	// Iterate over array of possible exit locations.
	if ( Driver != None )
    {
	    Driver.bHardAttach = false;
	    Driver.bCollideWorld = true;
	    Driver.SetCollision(true, true);
	    havePlaced = PlaceExitingDriver();

	    // If we could not find a place to put the driver, leave driver inside as before.
	    if ( !havePlaced && !bForceLeave )
	    {
	        Driver.bHardAttach = true;
	        Driver.bCollideWorld = false;
	        Driver.SetCollision(false, false);
	        return false;
	    }
	}

	// Reconnect Controller to Driver.
	C = Controller;
	if (C.RouteGoal == self)
		C.RouteGoal = None;
	if (C.MoveTarget == self)
		C.MoveTarget = None;
	Controller.UnPossess();

	if ( (Driver != None) && (Driver.Health > 0) )
	{
		Driver.SetOwner( C );
		C.Possess( Driver );

		PC = PlayerController(C);
		if ( PC != None )
			PC.ClientSetViewTarget( Driver ); // Set playercontroller to view the person that got out

		Driver.StopDriving( Self );
	}

	if ( C == Controller )	// If controller didn't change, clear it...
		Controller = None;

	// Vehicle now has no driver
	Driver		= None;
	bDriving	= false;

	// Put brakes on before you get out :)
    Throttle	= 0;
    Steering	= 0;
	Rise		= 0;

    return true;
}

function bool PlaceExitingDriver()
{
	local int		i, j;
	local vector	tryPlace, Extent, HitLocation, HitNormal, ZOffset, RandomSphereLoc;

	Extent		= Driver.CylinderComponent.default.CollisionRadius * vect(1,1,0);
	Extent.Z	= Driver.CylinderComponent.default.CollisionHeight;
	ZOffset		= Driver.CylinderComponent.default.CollisionHeight * vect(0,0,1);

	/*
	// avoid running driver over by placing in direction perpendicular to velocity
	if ( VSize(Velocity) > 100 )
	{
		tryPlace = Normal(Velocity cross vect(0,0,1)) * (CollisionRadius + Driver.default.CollisionRadius ) * 1.25 ;
		if ( FRand() < 0.5 )
			tryPlace *= -1; //randomly prefer other side
		if ( (Trace(HitLocation, HitNormal, Location + tryPlace + ZOffset, Location + ZOffset, false, Extent) == None && Driver.SetLocation(Location + tryPlace + ZOffset))
		     || (Trace(HitLocation, HitNormal, Location - tryPlace + ZOffset, Location + ZOffset, false, Extent) == None && Driver.SetLocation(Location - tryPlace + ZOffset)) )
			return true;
	}
	*/

	if ( !bRelativeExitPos )
	{
		for( i=0; i<ExitPositions.Length; i++)
		{
			tryPlace = ExitPositions[i];

			if ( Driver.SetLocation(tryPlace) )
			{
				return true;
			}
			else
			{
				for (j=0; j<10; j++) // try random positions in a sphere...
				{
					RandomSphereLoc = VRand()*200* FMax(FRand(),0.5);
					RandomSphereLoc.Z = Extent.Z * FRand();

					// First, do a line check (stops us passing through things on exit).
					if( Trace(HitLocation, HitNormal, tryPlace+RandomSphereLoc, tryPlace, false, Extent) == None )
					{
						if ( Driver.SetLocation(tryPlace+RandomSphereLoc) )
						{
							return true;
						}
					}
					else if( Driver.SetLocation(HitLocation) )
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	for( i=0; i<ExitPositions.Length; i++)
	{
		if ( ExitPositions[0].Z != 0 )
			ZOffset = Vect(0,0,1) * ExitPositions[0].Z;
		else
			ZOffset = Driver.CylinderComponent.default.CollisionHeight * vect(0,0,2);

		tryPlace = Location + ( (ExitPositions[i]-ZOffset) >> Rotation) + ZOffset;

		// First, do a line check (stops us passing through things on exit).
		if( Trace(HitLocation, HitNormal, tryPlace, Location + ZOffset, false, Extent) != None )
			continue;

		// Then see if we can place the player there.
		if ( !Driver.SetLocation(tryPlace) )
			continue;

		return true;
	}

	return false;
}

function UnPossessed()
{
	local PlayerController	PC;

	log("Vehicle::UnPossessed C:" $ Controller );

	PC = PlayerController(Controller);

    if ( PC != None )
        ClientDriverLeave( PC );

	NetPriority			= default.NetPriority;		// restore original netpriority changed when possessing
	NetUpdateTime		= Level.TimeSeconds - 1;
	NetUpdateFrequency	= 8;

	super.UnPossessed();
}

simulated function ClientDriverLeave(PlayerController PC)
{
}

function TakeDamage( int Damage, Pawn instigatedBy, Vector hitlocation, Vector momentum, 
					class<DamageType> damageType, optional TraceHitInfo HitInfo)
{
	if ( DamageType != None )
		Damage = Damage * DamageType.default.VehicleDamageScaling;
	
	super.TakeDamage(Damage, InstigatedBy, HitLocation, Momentum, DamageType, HitInfo);
}

function AdjustDriverDamage(out int Damage, Pawn InstigatedBy, Vector HitLocation, out Vector Momentum, class<DamageType> DamageType)
{
	if ( InGodMode() )
 		Damage = 0;
}

function DriverDied()
{
	local Controller C;

	Level.Game.DiscardInventory( Driver );

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

	// Car now has no driver
	Driver		= None;
	bDriving	= false;

	// Put brakes on before driver gets out! :)
    Throttle	= 0;
    Steering	= 0;
	Rise		= 0;
}

simulated function name GetDefaultCameraMode( PlayerController RequestedBy )
{
	if ( (RequestedBy != None) && (RequestedBy.PlayerCamera != None) && (RequestedBy.PlayerCamera.CameraStyle == 'Fixed') )
		return 'Fixed';

	return 'ThirdPerson';
}

// AI code
function bool Occupied()
{
	return ( Controller != None );
}

function Actor GetBestEntry(Pawn P)
{
	return Self;
}

// Vehicles dont get telefragged.
event EncroachedBy(Actor Other) {}



defaultproperties
{
	bRelativeExitPos=true
	LandMovementState=PlayerDriving
	Components.Remove(Arrow)
	Components.Remove(Sprite)
}