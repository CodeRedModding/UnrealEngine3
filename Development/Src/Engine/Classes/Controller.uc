//=============================================================================
// Controller, the base class of players or AI.
//
// Controllers are non-physical actors that can be attached to a pawn to control
// its actions.  PlayerControllers are used by human players to control pawns, while
// AIControFllers implement the artificial intelligence for the pawns they control.
// Controllers take control of a pawn using their Possess() method, and relinquish
// control of the pawn by calling UnPossess().
//
// Controllers receive notifications for many of the events occuring for the Pawn they
// are controlling.  This gives the controller the opportunity to implement the behavior
// in response to this event, intercepting the event and superceding the Pawn's default
// behavior.
//
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class Controller extends Actor
	native
	nativereplication
	abstract;

//=============================================================================
// BASE VARIABLES

var 	Pawn							Pawn;
var PlayerReplicationInfo 				PlayerReplicationInfo;
var const int							PlayerNum;						// The player number - per-match player number.
var const Controller					NextController;					// chained Controller list

var		float							FOVAngle;						// X field of view angle in degrees, usually 90.
var		bool        					bIsPlayer;						// Pawn is a player or a player-bot.
var		bool							bGodMode;		   				// cheat - when true, can't be killed or hurt
var		bool		bSoaking;			// pause and focus on this bot if it encounters a problem
var		bool		bSlowerZAcquire;	// acquire targets above or below more slowly than at same height //FIXMESTEVE move out of Controller
var		bool		bForceStrafe;
var		bool		bNotifyPostLanded;

// Input buttons.
var input byte							bFire;

//=============================================================================
// PHYSICS VARIABLES

var		bool							bNotifyApex;					// event NotifyJumpApex() when at apex of jump
var	 	float							MinHitWall;						// Minimum HitNormal dot Velocity.Normal to get a HitWall event from the physics

//=============================================================================
// NAVIGATION VARIABLES

var		bool							bAdvancedTactics;				// serpentine movement between pathnodes
var		bool							bCanOpenDoors;					// are we able to traverse R_DOOR reach specs?
var		bool							bCanDoSpecial;					// are we able to traverse R_SPECIAL reach specs?
var		bool							bAdjusting;						// adjusting around obstacle
var		bool							bPreparingMove;					// set true while pawn sets up for a latent move
var 	float							MoveTimer;						// internal timer for latent moves, useful for setting a max duration
var 	Actor							MoveTarget;						// actor being moved toward
var		vector	 						Destination;					// location being moved toward
var	 	vector							FocalPoint;						// location being looked at
var		Actor							Focus;							// actor being looked at
var		float		FocusLead;		// how much to lead view of focus
var		Actor							GoalList[4];					// used by navigation AI - list of intermediate goals
var		int								AcquisitionYawRate;
var		vector							AdjustLoc;						// location to move to while adjusting around obstacle
var NavigationPoint 					StartSpot;  					// where player started the match

/** Cached list of nodes filled in by the last call to FindPathXXX */
var array<Actor> RouteCache;

var 	ReachSpec						CurrentPath;
var ReachSpec	NextRoutePath;
var 	vector							CurrentPathDir;
var 	Actor							RouteGoal;						// final destination for current route
var 	float							RouteDist;						// total distance for current route
var		float							LastRouteFind;					// time at which last route finding occured

var float GroundPitchTime;
var vector ViewX, ViewY, ViewZ;	// Viewrotation encoding for PHYS_Spider

var Pawn ShotTarget; //FIXMESTEVE - this is hacky
var const Actor							LastFailedReach;				// cache to avoid trying failed actorreachable more than once per frame
var const float 						FailedReachTime;
var const vector 						FailedReachLocation;

const LATENT_MOVETOWARD = 503; // LatentAction number for Movetoward() latent function

//=============================================================================
// AI VARIABLES

var const bool							bLOSflag;						// used for alternating LineOfSight traces
var		bool							bUsePlayerHearing;
var		bool							bJumpOverWall;					// true when jumping to clear obstacle
var		bool							bEnemyAcquired;
var		bool							bNotifyFallingHitWall;
var		float							SightCounter;					// Used to keep track of when to check player visibility

// Enemy information
var	 	Pawn					    	Enemy;
var		Actor							Target;

/** Forces physics to use Controller.DesiredRotation regardless of focus etc */
var transient bool bForceDesiredRotation;

/** Forces all velocity to be directed towards reaching Destination */
var transient bool bPreciseDestination;

replication
{
	reliable if (bNetDirty && Role==ROLE_Authority)
		PlayerReplicationInfo, Pawn;

	// Functions the server calls on the client side.
	reliable if( RemoteRole==ROLE_AutonomousProxy )
		ClientGameEnded, ClientRoundEnded, ClientDying, ClientSetRotation, ClientSetLocation,
		ClientSwitchToBestWeapon, ClientSetWeapon;

	// Functions the client calls on the server.
	reliable if ( Role < ROLE_Authority )
		ServerRestartPlayer;
}

/* epic ===============================================
* ::PostBeginPlay
*
* Overridden to create the player replication info and
* perform other mundane initialization tasks.
*
* =====================================================
*/
event PostBeginPlay()
{
	Super.PostBeginPlay();
	if ( !bDeleteMe && bIsPlayer && (Level.NetMode != NM_Client) )
	{
		// create a new player replication info
		PlayerReplicationInfo = Spawn(Level.Game.PlayerReplicationInfoClass, Self,,vect(0,0,0),rot(0,0,0));
		InitPlayerReplicationInfo();
	}
	// randomly offset the sight counter to avoid hitches
	SightCounter = 0.2 * FRand();
}

/* epic ===============================================
* ::Reset
*
* Resets various properties to their default state, used
* for round resetting, etc.
*
* =====================================================
*/
function Reset()
{
	super.Reset();
	Enemy			= None;
	StartSpot		= None;
	bAdjusting		= false;
	bPreparingMove	= false;
	bJumpOverWall	= false;
	bEnemyAcquired	= false;
	MoveTimer		= -1;
	MoveTarget		= None;
	CurrentPath		= None;
	RouteGoal		= None;
}

/* epic ===============================================
* ::ClientSetLocation
*
* Replicated function to set the pawn location and
* rotation, allowing server to force (ex. teleports).
*
* =====================================================
*/
function ClientSetLocation( vector NewLocation, rotator NewRotation )
{
	SetRotation(NewRotation);
	If ( (Rotation.Pitch > RotationRate.Pitch)
		&& (Rotation.Pitch < 65536 - RotationRate.Pitch) )
	{
		If (Rotation.Pitch < 32768)
			NewRotation.Pitch = RotationRate.Pitch;
		else
			NewRotation.Pitch = 65536 - RotationRate.Pitch;
	}
	if ( Pawn != None )
	{
		NewRotation.Roll  = 0;
		Pawn.SetRotation( NewRotation );
		Pawn.SetLocation( NewLocation );
	}
}

/* epic ===============================================
* ::ClientSetRotation
*
* Replicated function to set the pawn rotation, allowing
* the server to force.
*
* =====================================================
*/
function ClientSetRotation( rotator NewRotation )
{
	SetRotation(NewRotation);
	if ( Pawn != None )
	{
		NewRotation.Pitch = 0;
		NewRotation.Roll  = 0;
		Pawn.SetRotation( NewRotation );
	}
}

/* epic ===============================================
 * ::ClientDying
 *
 * Replicated function indicating that our pawn has
 * kicked the bucket.
 *
 * =====================================================
 */
function ClientDying(class<DamageType> DamageType, vector HitLocation)
{
	GotoState('Dead');
}

/** Kistmet Action to possess a Pawn */
simulated function OnPossess(SeqAct_Possess inAction)
{
	if( inAction.PawnToPossess != None )
	{
		if( Pawn!= None && Vehicle(inAction.PawnToPossess) != None )
		{
			Vehicle(inAction.PawnToPossess).TryToDrive( Pawn );
		}
		else
		{
			Possess( inAction.PawnToPossess );
		}
	}
	else
	{
		warn( "PlayerController::OnPossess, PawnToPossess == None" );
	}
}

/* epic ===============================================
* ::Possess
*
* Handles attaching this controller to the specified
* pawn.
*
* =====================================================
*/
event Possess(Pawn inPawn)
{
	inPawn.PossessedBy(self);
	Pawn = inPawn;
	if (PlayerReplicationInfo != None)
	{
		if (Pawn.IsA('Vehicle') &&
			Vehicle(Pawn).Driver != None)
		{
			PlayerReplicationInfo.bIsFemale = Vehicle(Pawn).Driver.bIsFemale;
		}
		else
		{
			PlayerReplicationInfo.bIsFemale = Pawn.bIsFemale;
		}
	}
	// preserve Pawn's rotation initially for placed Pawns
	FocalPoint = Pawn.Location + 512*vector(Pawn.Rotation);
	Restart();
}

/* epic ===============================================
* ::UnPossess
*
* Handles detaching our current pawn, if any.
*
* =====================================================
*/
event UnPossess()
{
    if ( Pawn != None )
	{
        Pawn.UnPossessed();
		Pawn = None;
	}
}

/* epic ===============================================
* ::PawnDied
*
* Called once our currently possessed pawn has died,
* responsible for proper unpossession, etc.
*
* =====================================================
*/
function PawnDied(Pawn inPawn)
{
	local int idx;

	if ( inPawn != Pawn )
	{	// if that's not our current pawn, leave
		return;
	}

	// activate death events
	ActivateEventClass(class'SeqEvent_Death', self);
	// abort any latent actions
	for (idx = 0; idx < LatentActions.Length; idx++)
	{
		if (LatentActions[idx] != None)
		{
			LatentActions[idx].AbortFor(self);
		}
	}
	LatentActions.Length = 0;

	if ( Pawn != None )
	{
		SetLocation(Pawn.Location);
		Pawn.UnPossessed();
	}
	Pawn = None;

	// if we are a player, transition to the dead state
	if ( bIsPlayer )
	{
		// only if the game hasn't ended,
		if ( !GamePlayEndedState() )
		{
			// so that we can respawn
			GotoState('Dead');
		}
	}
	// otherwise destroy this controller
	else
	{
		if ( Pawn != None )
		{
			SetLocation( Pawn.Location );
			UnPossess();
		}
		Destroy();
	}
}

function bool GamePlayEndedState()
{
	return false;
}

/* epic ===============================================
* ::NotifyPostLanded
*
* Called after pawn lands after falling if bNotifyPostLanded is true
*
* =====================================================
*/
event NotifyPostLanded();

/* epic ===============================================
* ::Destroyed
*
* Called once this controller has been nuked, overridden
* to cleanup any lingering references, etc.
*
* =====================================================
*/
simulated event Destroyed()
{
	if (Role == ROLE_Authority)
	{
		// if we are a player, log out
		if (bIsPlayer &&
			Level.Game != None)
		{
			Level.Game.logout(self);
		}
		// if we have a valid PRI,
		if (PlayerReplicationInfo != None)
		{
			// remove from team if applicable
			if (!PlayerReplicationInfo.bOnlySpectator &&
				PlayerReplicationInfo.Team != None)
			{
				PlayerReplicationInfo.Team.RemoveFromTeam(self);
			}
			// and nuke the PRI
			PlayerReplicationInfo.Destroy();
			PlayerReplicationInfo = None;
		}
	}
	Super.Destroyed();
}

/* epic ===============================================
* ::Restart
*
* Called upon possessing a new pawn, perform any specific
* cleanup/initialization here.
*
* =====================================================
*/
function Restart()
{
	Pawn.Restart();
	Enemy = None;
}

/* epic ===============================================
* ::NotifyTakeHit
*
* Notification from pawn that it has received damage
* via TakeDamage().
*
* =====================================================
*/
function NotifyTakeHit(pawn InstigatedBy, vector HitLocation, int Damage, class<DamageType> damageType, vector Momentum);

function InitPlayerReplicationInfo()
{
	if (PlayerReplicationInfo.PlayerName == "")
		PlayerReplicationInfo.SetPlayerName(class'GameInfo'.Default.DefaultPlayerName);
}

/* epic ===============================================
* ::GetTeamNum
*
* Queries the PRI and returns our current team index.
*
* =====================================================
*/
simulated function byte GetTeamNum()
{
	if ( (PlayerReplicationInfo == None) || (PlayerReplicationInfo.Team == None) )
	{
		// 255 == no team
		return 255;
	}
	return PlayerReplicationInfo.Team.TeamIndex;
}

/* epic ===============================================
* ::ServerRestartPlayer
*
* Attempts to restart this player, generally called from
* the client upon respawn request.
*
* =====================================================
*/
function ServerRestartPlayer()
{
	if (Level.NetMode != NM_Client &&
		Pawn != None)
	{
		ServerGivePawn();
	}
}

/* epic ===============================================
* ::ServerGivePawn
*
* Requests a pawn from the server for this controller,
* as part of the respawn process.
*
* =====================================================
*/
function ServerGivePawn();

/* epic ===============================================
* ::SetCharacter
*
* Sets the character for this controller for future
* pawn spawns.
*
* =====================================================
*/
function SetCharacter(string inCharacter);

/* epic ===============================================
* ::GameHasEnded
*
* Called from game info upon end of the game, used to
* transition to proper state.
*
* =====================================================
*/
function GameHasEnded()
{
	// and transition to the game ended state
	GotoState('GameEnded');
}

/* epic ===============================================
* ::ClientGameEnded
*
* Replicated equivalent to GameHasEnded().
*
* =====================================================
*/
function ClientGameEnded()
{
	GotoState('GameEnded');
}

/* epic ===============================================
* ::NotifyKilled
*
* Notification from game that a pawn has been killed.
*
* =====================================================
*/
function NotifyKilled(Controller Killer, Controller Killed, pawn Other)
{
	if (Enemy == Other)
	{
		Enemy = None;
	}
}

//=============================================================================
// INVENTORY FUNCTIONS

/* epic ===============================================
* ::RatePickup
*
* Callback from PickupFactory that allows players to
* additionally weight certain pickups.
*
* =====================================================
*/
event float RatePickup(PickupFactory inPickup);

/* epic ===============================================
* ::FireWeaponAt
*
* Should cause this player to fire a weapon at the given
* actor.
*
* =====================================================
*/
function bool FireWeaponAt(Actor inActor);

/* epic ===============================================
* ::StopFiring
*
* Stop firing of our current weapon.
*
* =====================================================
*/
function StopFiring()
{
	bFire = 0;
	if (Pawn.Weapon != None)
	{
		Pawn.Weapon.StopFire(0);
	}
}

/* epic ===============================================
* ::RoundHasEnded
*
*
* =====================================================
*/
function RoundHasEnded()
{
	GotoState('RoundEnded');
}

function ClientRoundEnded()
{
	GotoState('RoundEnded');
}

/* epic ===============================================
* ::HandlePickup
*
* Called whenever our pawn runs over a new pickup.
*
* =====================================================
*/
function HandlePickup(Inventory Inv);

/**
 * Adjusts weapon aiming direction.
 * Gives controller a chance to modify the aiming of the pawn. For example aim error, auto aiming, adhesion, AI help...
 * Requested by weapon prior to firing.
*
 * @param	W, weapon about to fire
 * @param	StartFireLoc, world location of weapon fire start trace, or projectile spawn loc.
 * @param	BaseAimRot, original aiming rotation without any modifications.
 */
function Rotator GetAdjustedAimFor( Weapon W, vector StartFireLoc, rotator BaseAimRot )
{
	// by default, return Rotation. This is the standard aim for controllers
	// see implementation for PlayerController.
	return BaseAimRot;
}

/* epic ===============================================
* ::AdjustToss
*
* Callback from weapon fire that allows the firing of
* projectiles to be adjusted.
*
* =====================================================
*/
function vector AdjustToss(float TSpeed, vector Start, vector End, bool bNormalize)
{
	local vector Dest2D, Result, Vel2D;
	local float Dist2D;

	if ( Start.Z > End.Z + 64 )
	{
		Dest2D = End;
		Dest2D.Z = Start.Z;
		Dist2D = VSize(Dest2D - Start);
		TSpeed *= Dist2D/VSize(End - Start);
		Result = SuggestFallVelocity(Dest2D,Start,TSpeed,TSpeed);
		Vel2D = result;
		Vel2D.Z = 0;
		Result.Z = Result.Z + (End.Z - Start.Z) * VSize(Vel2D)/Dist2D;
	}
	else
	{
		Result = SuggestFallVelocity(End,Start,TSpeed,TSpeed);
	}
	if ( bNormalize )
		return TSpeed * Normal(Result);
	else
		return Result;
}

/* epic ===============================================
* ::InstantWarnTarget
*
* Warn a target it is about to be shot at with an instant hit
* weapon.
*
* =====================================================
*/
function InstantWarnTarget(Actor Target, Weapon FiredWeapon, vector FireDir)
{
	local float Dist;

	if ( (FiredWeapon != None) && (Pawn(Target) != None) && (Pawn(Target).Controller != None)  )
	{
		Dist = VSize(Pawn.Location - Target.Location);
		if ( VSize(FireDir * Dist - Target.Location) < Pawn(Target).CylinderComponent.CollisionRadius )
			return;
		if ( FRand() < FiredWeapon.WarnTargetPct )
			Pawn(Target).Controller.ReceiveWarning(Pawn, -1, FireDir);
		return;
	}
}

/* epic ===============================================
* ::ReceiveWarning
*
* Notification that the pawn is about to be shot by a
* projectile.
*
* =====================================================
*/
function ReceiveWarning(Pawn shooter, float projSpeed, vector FireDir);

/* epic ===============================================
* ::SwitchToBestWeapon
*
* Rates the pawn's weapon loadout and chooses the best
* weapon, bringing it up as the active weapon.
*
* =====================================================
*/

exec function SwitchToBestWeapon()
{
	if ( Pawn == None || Pawn.InvManager == None )
		return;

	Pawn.InvManager.SwitchToBestWeapon();
}

/* epic ===============================================
* ::ClientSwitchToBestWeapon
*
* Forces the client to switch to a new weapon, allowing
* the server control.
*
* =====================================================
*/
function ClientSwitchToBestWeapon()
{
    SwitchToBestWeapon();
}

/* ClientSetWeapon:
	Forces client to switch to this weapon if it can be found in loadout
*/
function ClientSetWeapon( class<Weapon> WeaponClass )
{
    local Inventory Inv;

	if ( Pawn == None )
		return;

	Inv = Pawn.FindInventoryType( WeaponClass );
	if ( Weapon(Inv) != None )
		Pawn.SetActiveWeapon( Weapon(Inv) );
}

/* epic ===============================================
* ::NotifyChangedWeapon
*
* Notification from pawn that the current weapon has
* changed.
* Network: LocalPlayer
* =====================================================
*/
function NotifyChangedWeapon( Weapon PrevWeapon, Weapon NewWeapon );

//=============================================================================
// AI FUNCTIONS

/* epic ===============================================
* ::LineOfSightTo
*
* Returns true if the specified actor has line of sight
* to our pawn.
*
* NOTE: No FOV is accounted for, use CanSee().
*
* =====================================================
*/
native(514) final function bool LineOfSightTo(actor Other, optional vector chkLocation);

/* epic ===============================================
* ::CanSee
*
* Returns true if the specified pawn is visible within
* our peripheral vision.  If the pawn is not our current
* enemy then LineOfSightTo() will be called instead.
*
* =====================================================
*/
native(533) final function bool CanSee(Pawn Other);

/* epic ===============================================
* ::PickTarget
*
* Evaluates pawns in the local area and returns the
* one that is closest to the specified FireDir.
*
* =====================================================
*/
native(531) final function Pawn PickTarget(out float bestAim, out float bestDist, vector FireDir, vector projStart, float MaxRange);

/* epic ===============================================
* ::PickAnyTarget
*
* Evaluates all actors in the local area with bProjTarget
* set to true, and that aren't Pawns.  Returns the one
* that is closest to the specified FireDir.
*
* =====================================================
*/
native(534) final function Actor PickAnyTarget(out float bestAim, out float bestDist, vector FireDir, vector projStart);

/* epic ===============================================
* ::HearNoise
*
* Counterpart to the Actor::MakeNoise() function, called
* whenever this player is within range of a given noise.
* Used as AI audio cues, instead of processing actual
* sounds.
*
* =====================================================
*/
event HearNoise( float Loudness, Actor NoiseMaker);

/* epic ===============================================
* ::SeePlayer
*
* Called whenever Seen is within of our line of sight
* if Seen.bIsPlayer==true.
*
* =====================================================
*/
event SeePlayer( Pawn Seen );

/* epic ===============================================
* ::SeeMonster
*
* Called whenever Seen is within of our line of sight
* if Seen.bIsPlayer==false.
*
* =====================================================
*/
event SeeMonster( Pawn Seen );

/* epic ===============================================
* ::EnemyNotVisible
*
* Called whenever Enemy is no longer within of our line
* of sight.
*
* =====================================================
*/
event EnemyNotVisible();

//=============================================================================
// NAVIGATION FUNCTIONS

/* epic ===============================================
* ::MoveTo
*
* Latently moves our pawn to the desired location, which
* is cached in Destination.
*
* =====================================================
*/
native(500) final latent function MoveTo( vector NewDestination, optional Actor ViewFocus, optional bool bShouldWalk);

/* epic ===============================================
* ::MoveToward
*
* Latently moves our pawn to the desired actor, which
* is cached in MoveTarget.  Takes advantage of the
* navigation network anchors when moving to other Pawn
* and Inventory actors.
*
* =====================================================
*/
native(502) final latent function MoveToward(actor NewTarget, optional Actor ViewFocus, optional float DestinationOffset, optional bool bUseStrafing, optional bool bShouldWalk);

/* epic ===============================================
* ::PrepareForMove
*
* Called upon movement to a new node in latent movement
* to allow special preparation (ex. crouching under
* intermediate obstactles).
*
* =====================================================
*/
event PrepareForMove(NavigationPoint Goal, ReachSpec Path);

/* epic ===============================================
* ::SetupSpecialPathAbilities
*
* Called before path finding to allow setup of transient
* navigation flags.
*
* =====================================================
*/
event SetupSpecialPathAbilities();

/* epic ===============================================
* ::FinishRotation
*
* Latently waits for our pawn's rotation to match the
* pawn's DesiredRotation.
*
* =====================================================
*/
native(508) final latent function FinishRotation();

/* epic ===============================================
* ::FindPathTo
*
* Searches the navigation network for a path to the
* node closest to the given point.
*
* =====================================================
*/
native(518) final function Actor FindPathTo(vector aPoint);

/* epic ===============================================
* ::FindPathToward
*
* Searches the navigation network for a path to the
* node closest to the given actor.
*
* =====================================================
*/
native(517) final function Actor FindPathToward(actor anActor, optional bool bWeightDetours);

/* epic ===============================================
* ::FindPathTowardNearest
*
* Searches the navigation network for a path to the
* closest node of the specified type.
*
* =====================================================
*/
native final function Actor FindPathTowardNearest(class<NavigationPoint> GoalClass, optional bool bWeightDetours);

/* epic ===============================================
* ::FindRandomDest
*
* Returns a random node on the network.
*
* =====================================================
*/
native(525) final function NavigationPoint FindRandomDest();

native final function Actor FindPathToIntercept(Pawn P, Actor RouteGoal, optional bool bWeightDetours);

/* epic ===============================================
* ::PointReachable
*
* Returns true if the given point is directly reachable
* given our pawn's current movement capabilities.
*
* NOTE: This function is potentially expensive and should
* be used sparingly.  If at all possible, use ActorReachable()
* instead, since that has more possible optimizations
* over PointReachable().
*
* =====================================================
*/
native(521) final function bool PointReachable(vector aPoint);

/* epic ===============================================
* ::ActorReachable
*
* Returns true if the given actor is directly reachable
* given our pawn's current movement capabilities.  Takes
* advantage of the navigation network anchors when
* possible.
*
* NOTE: This function is potentially expensive and should
* be used sparingly.
*
* =====================================================
*/
native(520) final function bool ActorReachable(actor anActor);

/* epic ===============================================
* ::PickWallAdjust
*
* Checks if we could jump over obstruction (if within
* knee height), or attempts to move to either side of
* the obstruction.
*
* =====================================================
*/
native(526) final function bool PickWallAdjust(vector HitNormal);

/* epic ===============================================
* ::WaitForLanding
*
* Latently waits until the pawn has landed.  Only valid
* with PHYS_Falling.  Optionally specify the max amount
* of time to wait, which defaults to 4 seconds.
*
* NOTE: If the pawn hasn't landed before the duration
* is exceeded, it will abort and call LongFall().
*
* =====================================================
*/
native(527) final latent function WaitForLanding(optional float waitDuration);

/* epic ===============================================
* ::LongFall
*
* Called once the duration for WaitForLanding has been
* exceeded without hitting ground.
*
* =====================================================
*/
event LongFall();

/* epic ===============================================
* ::EndClimbLadder
*
* Aborts a latent move action if the MoveTarget is
* currently a ladder.
*
* =====================================================
*/
native function EndClimbLadder();

/* epic ===============================================
* ::MayFall
*
* Called when a pawn is about to fall off a ledge, and
* allows the controller to prevent a fall by toggling
* bCanJump.
*
* =====================================================
*/
event MayFall();

/* epic ===============================================
* ::AllowDetourTo
*
* Return true to allow a detour to a given point, or
* false to avoid it.  Called during any sort of path
* finding (FindPathXXX() functions).
*
* =====================================================
*/
event bool AllowDetourTo(NavigationPoint N)
{
	return true;
}

//=============================================================================
// CAMERA FUNCTIONS

/* epic ===============================================
* ::GetFOVAngle
*
* Returns the current FOV angle.
*
* =====================================================
*/
event float	GetFOVAngle()
{
	return FOVAngle;
}

/**
 * Returns Player's Point of View
 * For the AI this means the Pawn's 'Eyes' ViewPoint
 * For a Human player, this means the Camera's ViewPoint 
*
 * @output	out_Location, view location of player
 * @output	out_rotation, view rotation of player
*/
simulated event GetPlayerViewPoint( out vector out_Location, out Rotator out_rotation )
{
	out_Location = Location;
	out_Rotation = Rotation;
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
	// If we have a Pawn, this is our view point.
	if ( Pawn != None )
	{
		Pawn.GetActorEyesViewPoint( out_Location, out_Rotation );
	}
	else
	{	// Otherwise controller location/rotation is our view point.
		out_Location = Location;
		out_Rotation = Rotation;
	}
}

/* epic ===============================================
* ::AdjustView
*
* Checks to see if the pawn still needs to update eye
* height (only if currently used as a viewtarget).
*
* =====================================================
*/
function AdjustView( float DeltaTime )
{
	local Controller C;
	local bool bViewTarget;
	// if currently updating the eye height
	if (Pawn.bUpdateEyeHeight)
	{
		// search for a player using our pawn as a viewtarget
		for (C = Level.ControllerList; C != None && !bViewTarget; C = C.NextController)
		{
			if (C.IsA('PlayerController') &&
				PlayerController(C).ViewTarget == Pawn)
			{
				bViewTarget = true;
			}
		}
		// if not a view target,
		if (!bViewTarget)
		{
			// disable eye height updates
			Pawn.bUpdateEyeHeight = false;
		}
	}
	Pawn.Eyeheight = Pawn.BaseEyeheight;
}

/* epic ===============================================
* ::WantsSmoothedView
*
* Should the eye height updates be smoothed?
*
* =====================================================
*/
function bool WantsSmoothedView()
{
	return (Pawn != None &&
			!Pawn.bJustLanded &&
			(Pawn.Physics == PHYS_Walking ||
			 Pawn.Physics==PHYS_Spider));
}

//=============================================================================
// PHYSICS FUNCTIONS

/* epic ===============================================
* ::NotifyPhysicsVolumeChange
*
* Called when our pawn enters a new physics volume.
*
* =====================================================
*/
event NotifyPhysicsVolumeChange(PhysicsVolume NewVolume);

/* epic ===============================================
* ::NotifyHeadVolumeChange
*
* Called when our pawn's head enters a new physics
* volume.
* return true to prevent HeadVolumeChange() notification on the pawn.
*
* =====================================================
*/
event bool NotifyHeadVolumeChange(PhysicsVolume NewVolume);

/* epic ===============================================
* ::NotifyLanded
*
* Called when our pawn has landed from a fall, return
* true to prevent Landed() notification on the pawn.
*
* =====================================================
*/
event bool NotifyLanded(vector HitNormal);

/* epic ===============================================
* ::NotifyHitWall
*
* Called when our pawn has collided with a blocking
* piece of world geometry, return true to prevent
* HitWall() notification on the pawn.
*
* =====================================================
*/
event bool NotifyHitWall(vector HitNormal, actor Wall);

/* epic ===============================================
* ::NotifyFallingHitWall
*
* Called when our pawn has collided with a blocking
* piece of world geometry while falling, only called if
* bNotifyFallingHitWall is true
* =====================================================
*/
event NotifyFallingHitWall(vector HitNormal, actor Wall); 

/* epic ===============================================
* ::NotifyBump
*
* Called when our pawn has collided with a blocking player,
* return true to prevent Bump() notification on the pawn.
*
* =====================================================
*/
event bool NotifyBump(Actor Other, Vector HitNormal);

/* epic ===============================================
* ::NotifyJumpApex
*
* Called when our pawn has reached the apex of a jump.
*
* =====================================================
*/
event NotifyJumpApex();

/* epic ===============================================
* ::NotifyMissedJump
*
* Called when our pawn misses a jump while performing
* latent movement (ie MoveToward()).
*
* =====================================================
*/
event NotifyMissedJump();

//=============================================================================
// MISC FUNCTIONS

/* epic ===============================================
* ::InLatentExecution
*
* Returns true if currently in the specified latent
* action.
*
* =====================================================
*/
native final function bool InLatentExecution(int LatentActionNumber);

/* epic ===============================================
* ::StopLatentExecution
*
* Stops any active latent action.
*
* =====================================================
*/
native final function StopLatentExecution();

/** 
 * list important Controller variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local Canvas Canvas;

	Canvas = HUD.Canvas;

	if ( Pawn == None )
	{
		if ( PlayerReplicationInfo == None )
		{
			Canvas.DrawText("NO PLAYERREPLICATIONINFO", false);
		}
		else
		{
			PlayerReplicationInfo.DisplayDebug(HUD,out_YL,out_YPos);
		}
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);

		super.DisplayDebug(HUD,out_YL,out_YPos);
		return;
	}

	Canvas.SetDrawColor(255,0,0);
	Canvas.DrawText("CONTROLLER "$GetItemName(string(Self))$" Pawn "$GetItemName(string(Pawn)));
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);

	if ( HUD.bShowAIDebug )
	{
		if ( Enemy != None )
		{
			Canvas.DrawText("     STATE: "$GetStateName()$" Enemy "$Enemy.GetHumanReadableName(), false);
		}
		else
		{
			Canvas.DrawText("     STATE: "$GetStateName()$" NO Enemy ", false);
		}
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}
}

/* epic ===============================================
* ::GetHumanReadableName
*
* Returns the PRI player name if available,	or of this
* controller object.
*
* =====================================================
*/
simulated function String GetHumanReadableName()
{
	if ( PlayerReplicationInfo != None )
	{
		return PlayerReplicationInfo.PlayerName;
	}
	else
	{
		return GetItemName(String(self));
	}
}

//=============================================================================
// CONTROLLER STATES

/* epic ===============================================
* state Dead
*
* Base state entered upon pawn death, if bIsPlayer is
* set to true.
*
* =====================================================
*/
State Dead
{
ignores SeePlayer, HearNoise, KilledBy;

	function PawnDied(Pawn P)
	{
		if ( Level.NetMode != NM_Client )
			warn( Self @ "Pawndied while dead" );
	}

	/* epic ===============================================
	* ::ServerRestartPlayer
	*
	* Attempts to restart the player if not a client.
	*
	* =====================================================
	*/
	function ServerReStartPlayer()
	{
		if ( Level.NetMode == NM_Client )
			return;
		
		// If we're still attached to a Pawn, leave it
		if ( Pawn != None )
		{
			UnPossess();
		}

		Level.Game.RestartPlayer( Self );
	}
}

/* epic ===============================================
* state GameEnded
*
* Base state entered upon the end of the game, instigated
* by the
*
* =====================================================
*/
state GameEnded
{
ignores SeePlayer, HearNoise, KilledBy, NotifyBump, HitWall, NotifyPhysicsVolumeChange, NotifyHeadVolumeChange, Falling, TakeDamage, ReceiveWarning;

	function bool GamePlayEndedState()
	{
		return true;
	}

	function BeginState()
	{
		// if we still have a valid pawn,
		if (Pawn != None)
		{
			// stop it in midair and detach
			Pawn.TurnOff();
			Pawn.UnPossessed();
			Pawn = None;
		}
		if (!bIsPlayer)
		{
			Destroy();
		}
	}
}

/**
 * Overridden to redirect to pawn, since teleporting the controller
 * would be useless.
 */
function OnTeleport(SeqAct_Teleport inAction)
{
	if (Pawn != None)
	{
		Pawn.OnTeleport(inAction);
	}
	else
	{
		Super.OnTeleport(inAction);
	}
}

/**
 * Sets god mode based on the activated link.
 */
function OnToggleGodMode(SeqAct_ToggleGodMode inAction)
{
	if (inAction.InputLinks[0].bHasImpulse)
	{
		bGodMode = true;
	}
	else
	if (inAction.InputLinks[1].bHasImpulse)
	{
		bGodMode = false;
	}
	else
	{
		bGodMode = !bGodMode;
	}
}

/**
 * Returns the collision radius of our Pawn's cylinder
 * collision component.
 * 
 * @return	the collision radius of our pawn
 */
final function float GetCollisionRadius()
{
	if( Pawn != None )
	{
		return CylinderComponent(Pawn.CollisionComponent).CollisionRadius;
	}
}

/**
 * Returns the collision height of our Pawn's cylinder
 * collision component.
 * 
 * @return	collision height of our pawn
 */
final function float GetCollisionHeight()
{
	if( Pawn != None )
	{
		return CylinderComponent(Pawn.CollisionComponent).CollisionHeight;
	}
}

/**
 * Called from CoverNode.OnToggle() when bEnabled is set to
 * false, so that the controller can properly handle their
 * cover being disabled.
 * 
 * @param	disabledNode - node that has been disabled
 */
simulated function NotifyCoverDisabled(CoverNode disabledNode);

/**
 * Called from CoverNode.Claim() when another player steals the
 * claim on our current cover.
 * 
 * @param	revokedNode - node that our claim has been revoked for
 */
simulated function NotifyCoverClaimRevoked(CoverNode revokedNode);

/**
 * Called when shot at and nearly missed.
 */
function NotifyNearMiss(Pawn shooter);

state RoundEnded
{
ignores SeePlayer, HearNoise, KilledBy, NotifyBump, HitWall, NotifyPhysicsVolumeChange, NotifyHeadVolumeChange, Falling, TakeDamage, ReceiveWarning;

	function bool GamePlayEndedState()
	{
		return true;
	}

	function BeginState()
	{
		if ( Pawn != None )
		{
			// FIXMESTEVE if ( Pawn.Weapon != None )
			//	Pawn.Weapon.HolderDied();
			Pawn.TurnOff();
			Pawn.UnPossessed();
			Pawn = None;
		}
		if ( !bIsPlayer )
			Destroy();
	}
}

defaultproperties
{
	RotationRate=(Pitch=3072,Yaw=30000,Roll=2048)
	AcquisitionYawRate=20000
	FOVAngle=+00090.000000
	bHidden=true
	bHiddenEd=true
	MinHitWall=-1.f
    bSlowerZAcquire=true
    RemoteRole=ROLE_None
    bOnlyRelevantToOwner=true

}