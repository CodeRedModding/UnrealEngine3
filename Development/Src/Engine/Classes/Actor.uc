//=============================================================================
// Actor: The base class of all actors.
// Actor is the base class of all gameplay objects.
// A large number of properties, behaviors and interfaces are implemented in Actor, including:
//
// -	Display
// -	Animation
// -	Physics and world interaction
// -	Making sounds
// -	Networking properties
// -	Actor creation and destruction
// -	Actor iterator functions
// -	Message broadcasting
//
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class Actor extends Object
	abstract
	native
	nativereplication;

// Actor components.

var const array<ActorComponent>	Components;

// Priority Parameters
// Actor's current physics mode.
var(Movement) const enum EPhysics
{
	PHYS_None,
	PHYS_Walking,
	PHYS_Falling,
	PHYS_Swimming,
	PHYS_Flying,
	PHYS_Rotating,
	PHYS_Projectile,
	PHYS_Interpolating,
	PHYS_Spider,
	PHYS_Ladder,
	PHYS_RigidBody,
	PHYS_Articulated
} Physics;

// Owner.
var const Actor	Owner;			// Owner actor.
var const Actor	Base;           // Actor we're standing on.

struct native TimerData
{
	var bool			bLoop;
	var Name			FuncName;
	var float			Rate, Count;
};
var const array<TimerData>			Timers;			// list of currently active timers

// Flags.
var			  const bool	bStatic;			// Does not move or change over time. Don't let L.D.s change this - screws up net play

/** If this is True, all PrimitiveComponents of the actor are hidden.  If this is false, only PrimitiveComponents with HiddenGame=True are hidden. */
var(Advanced)		bool	bHidden;

var(Advanced) const bool	bNoDelete;			// Cannot be deleted during play.
var			  const	bool	bDeleteMe;			// About to be deleted.
var transient const bool	bTicked;			// Actor has been updated.
var					bool    bOnlyOwnerSee;		// Only owner can see this actor.
var(Advanced)		bool	bStasis;			// In StandAlone games, turn off if not in a recently rendered zone turned off if  bStasis  and physics = PHYS_None or PHYS_Rotating.
var					bool	bWorldGeometry;		// Collision and Physics treats this actor as world geometry
var					bool	bIgnoreVehicles;			// Ignore collisions between vehicles and this actor (only relevant if bIgnoreEncroachers is false)
var					bool	bOrientOnSlope;		// when landing, orient base on slope of floor
var			  const	bool	bOnlyAffectPawns;	// Optimisation - only test ovelap against pawns. Used for influences etc.
var(Movement)		bool	bIgnoreEncroachers; // Ignore collisions between movers and this actor

// Networking flags
var			  const	bool	bNetTemporary;				// Tear-off simulation in network play.
var					bool	bOnlyRelevantToOwner;			// this actor is only relevant to its owner.
var transient const	bool	bNetDirty;					// set when any attribute is assigned a value in unrealscript, reset when the actor is replicated
var					bool	bAlwaysRelevant;			// Always relevant for network.
var					bool	bReplicateInstigator;		// Replicate instigator to client (used by bNetTemporary projectiles).
var					bool	bReplicateMovement;			// if true, replicate movement/location related properties
var					bool	bSkipActorPropertyReplication; // if true, don't replicate actor class variables for this actor
var					bool	bUpdateSimulatedPosition;	// if true, update velocity/location after initialization for simulated proxies
var					bool	bTearOff;					// if true, this actor is no longer replicated to new clients, and
														// is "torn off" (becomes a ROLE_Authority) on clients to which it was being replicated.
var					bool	bOnlyDirtyReplication;		// if true, only replicate actor if bNetDirty is true - useful if no C++ changed attributes (such as physics)
														// bOnlyDirtyReplication only used with bAlwaysRelevant actors
var const           bool    bNetInitialRotation;        // Should replicate initial rotation
var					bool	bCompressedPosition;		// used by networking code to flag compressed position replication
var		const		bool	bClientAuthoritative;		// Remains ROLE_Authority on client (only valid for bStatic or bNoDelete actors)
var					bool	bBadStateCode;				// used for recovering from illegal state transitions (hack)

// Net variables.
enum ENetRole
{
	ROLE_None,              // No role at all.
	ROLE_SimulatedProxy,	// Locally simulated proxy of this actor.
	ROLE_AutonomousProxy,	// Locally autonomous proxy of this actor.
	ROLE_Authority,			// Authoritative control over the actor.
};
var ENetRole RemoteRole, Role;
var const transient int		NetTag;
var float NetUpdateTime;	// time of last update
var float NetUpdateFrequency; // How many net updates per second.
var float NetPriority; // Higher priorities means update it more frequently.
var Pawn                  Instigator;    // Pawn responsible for damage caused by this actor.

var       const LevelInfo Level;         // Level this actor is on.
var transient const Level	XLevel;			// Level object.
var(Advanced)	float		LifeSpan;		// How old the object lives before dying, 0=forever.
var const float CreationTime;				// The time this actor was created, relative to Level.TimeSeconds

//-----------------------------------------------------------------------------
// Structures.

// Identifies a unique convex volume in the world.
struct PointRegion
{
	var zoneinfo Zone;       // Zone.
	var int      iLeaf;      // Bsp leaf.
	var byte     ZoneNumber; // Zone number.
};

struct native TraceHitInfo
{
	var material			Material; // Material we hit.
	var int					Item; // Extra info about thing we hit.
	var name				BoneName; // Name of bone if we hit a skeletal mesh.
	var PrimitiveComponent	HitComponent; // Component of the actor that we hit.
};

//-----------------------------------------------------------------------------
// Major actor properties.

var const PointRegion     Region;        // Region this actor is in.
var transient float		LastRenderTime;	// last time this actor was rendered.
var(Events) name			Tag;			// Actor's tag name.
var(Object) name InitialState;
var(Object) name Group;

// Internal.
var const array<Actor>		Touching;		 // List of touching actors.
var const actor				Deleted;       // Next actor in just-deleted chain.
var const float				LatentFloat;   // Internal latent function use.
var const AnimNodeSequence	LatentSeqNode; // Internal latent function use.

// The actor's position and rotation.
var const	PhysicsVolume	PhysicsVolume;	// physics volume this actor is currently in
var(Movement) const vector	Location;		// Actor's location; use Move to set.
var(Movement) const rotator Rotation;		// Rotation.
var(Movement) vector		Velocity;		// Velocity.
var			  vector        Acceleration;	// Acceleration.

// Attachment related variables
var(Movement) SkeletalMeshComponent	BaseSkelComponent;
var(Movement) name					BaseBoneName;

var(Movement)	name	AttachTag;
var const array<Actor>  Attached;			// array of actors attached to this actor.
var const vector		RelativeLocation;	// location relative to base/bone (valid if base exists)
var const rotator		RelativeRotation;	// rotation relative to base/bone (valid if base exists)

var(Movement) bool bHardAttach;             // Uses 'hard' attachment code. bBlockActor must also be false.
											// This actor cannot then move relative to base (setlocation etc.).
											// Dont set while currently based on something!

//-----------------------------------------------------------------------------
// Display properties.

var(Display) const interp float	DrawScale;		// Scaling factor, 1.0=normal size.
var(Display) const vector	DrawScale3D;		// Scaling vector, (1.0,1.0,1.0)=normal size.
var(Display) const vector	PrePivot;			// Offset from box center for drawing.

// Advanced.
var			  bool		bHurtEntry;				// keep HurtRadius from being reentrant
var			  bool		bGameRelevant;			// Always relevant for game
var(Advanced) bool		bCollideWhenPlacing;	// This actor collides with the world when placing.
var			  bool		bTravel;				// Actor is capable of travelling among servers.
var const     bool		bMovable;				// Actor can be moved.
var			  bool		bDestroyInPainVolume;	// destroy this actor if it enters a pain volume
var			  bool		bCanBeDamaged;			// can take damage
var(Advanced) bool		bShouldBaseAtStartup;	// if true, find base for this actor at level startup, if collides with world and PHYS_None or PHYS_Rotating
var			  bool		bPendingDelete;			// set when actor is about to be deleted (since endstate and other functions called
												// during deletion process before bDeleteMe is set).
var(Advanced)		bool	bCanTeleport;		// This actor can be teleported.
var			  const	bool	bAlwaysTick;		// Update even when players-only.

//-----------------------------------------------------------------------------
// Collision.

// Collision primitive.
var(Collision) PrimitiveComponent	CollisionComponent;

var			native int	  OverlapTag;

// Collision flags.
var(Collision) const bool bCollideActors;		// Collides with other actors.
var(Collision) bool       bCollideWorld;		// Collides with the world.
var(Collision) bool       bBlockActors;			// Blocks other nonplayer actors.
var(Collision) bool       bProjTarget;			// Projectiles should potentially target this actor.
var			   bool		  bBlocksTeleport;

//-----------------------------------------------------------------------------
// Physics.

// Options.
var			  bool		  bIgnoreOutOfWorld; // Don't destroy if enters zone zero
var			  bool        bBounce;           // Bounces when hits ground fast.
var			  const bool  bJustTeleported;   // Used by engine physics - not valid for scripts.

// Physics properties.
var(Movement) float       Mass;				// Mass of this actor.
var(Movement) float       Buoyancy;			// Water buoyancy. A ratio (1.0 = neutral buoyancy, 0.0 = no buoyancy)
var(Movement) rotator	  RotationRate;		// Change in rotation per second.
var(Movement) rotator     DesiredRotation;	// Physics will smoothly rotate actor to this rotation.
var			  Actor		  PendingTouch;		// Actor touched during move which wants to add an effect after the movement completes

const MINFLOORZ = 0.7; // minimum z value for floor normal (if less, not a walkable floor)
					   // 0.7 ~= 45 degree angle for floor

struct RigidBodyState
{
	var vector	Position;
	var Quat	Quaternion;
	var vector	LinVel;
	var vector	AngVel;
};

// endif

//-----------------------------------------------------------------------------
// Networking.

// Symmetric network flags, valid during replication only.
var const bool bNetInitial;       // Initial network update.
var const bool bNetOwner;         // Player owns this actor.
var const bool bNetRelevant;      // Actor is currently relevant. Only valid server side, only when replicating variables.

//Editing flags
var(Advanced) edlayer     Layer;		// The layer this actor belongs to in UnrealEd
var(Advanced) bool        bHiddenEd;     // Is hidden during editing.
var(Advanced) bool        bHiddenEdGroup;// Is hidden by the group brower.
var(Advanced) bool        bEdShouldSnap; // Snap to grid in editor.
var transient bool        bEdSnap;       // Should snap to grid in UnrealEd.
var transient const bool  bTempEditor;   // Internal UnrealEd.
var(Collision) bool		  bPathColliding;// this actor should collide (if bWorldGeometry && bBlockActors is true) during path building (ignored if bStatic is true, as actor will always collide during path building)
var transient bool		  bPathTemp;	 // Internal/path building
var	bool				  bScriptInitialized; // set to prevent re-initializing of actors spawned during level startup
var(Advanced) bool        bLockLocation; // Prevent the actor from being moved in the editor.

var class<LocalMessage> MessageClass;

//-----------------------------------------------------------------------------
// Enums.

// Travelling from server to server.
enum ETravelType
{
	TRAVEL_Absolute,	// Absolute URL.
	TRAVEL_Partial,		// Partial (carry name, reset server).
	TRAVEL_Relative,	// Relative URL.
};


// double click move direction.
enum EDoubleClickDir
{
	DCLICK_None,
	DCLICK_Left,
	DCLICK_Right,
	DCLICK_Forward,
	DCLICK_Back,
	DCLICK_Active,
	DCLICK_Done
};

//-----------------------------------------------------------------------------
// Scripting support

/** List of all events that this actor can support, for use by the editor */
var const array<class<SequenceEvent> > SupportedEvents;

/** List of all events currently associated with this actor, that can be generated */
var() const array<SequenceEvent> GeneratedEvents;

/** List of all latent actions currently active on this actor */
var transient array<SeqAct_Latent> LatentActions;

//-----------------------------------------------------------------------------
// natives.

// Execute a console command in the context of the current level and game engine.
native function string ConsoleCommand( string Command, optional bool bWriteToLog );

//-----------------------------------------------------------------------------
// Network replication.

replication
{
	// Location
	unreliable if ( (!bSkipActorPropertyReplication || bNetInitial) && bReplicateMovement
					&& (((RemoteRole == ROLE_AutonomousProxy) && bNetInitial)
						|| ((RemoteRole == ROLE_SimulatedProxy) && (bNetInitial || bUpdateSimulatedPosition) && ((Base == None) || Base.bWorldGeometry))) )
		Location;

	unreliable if ( (!bSkipActorPropertyReplication || bNetInitial) && bReplicateMovement
					&& (((RemoteRole == ROLE_AutonomousProxy) && bNetInitial)
						|| ((RemoteRole == ROLE_SimulatedProxy) && (bNetInitial || bUpdateSimulatedPosition) && ((Base == None) || Base.bWorldGeometry))) )
		Rotation;

	unreliable if ( (!bSkipActorPropertyReplication || bNetInitial) && bReplicateMovement
					&& RemoteRole==ROLE_SimulatedProxy )
		Base;

	unreliable if( (!bSkipActorPropertyReplication || bNetInitial) && bReplicateMovement
					&& RemoteRole==ROLE_SimulatedProxy && (Base != None) && !Base.bWorldGeometry)
		RelativeRotation, RelativeLocation;

	// Physics
	unreliable if( (!bSkipActorPropertyReplication || bNetInitial) && bReplicateMovement
					&& ((RemoteRole == ROLE_SimulatedProxy) && (bNetInitial || bUpdateSimulatedPosition)) )
		Velocity, Physics;

	unreliable if( (!bSkipActorPropertyReplication || bNetInitial) && bReplicateMovement
					&& (RemoteRole == ROLE_SimulatedProxy) && (Physics == PHYS_Rotating) )
		RotationRate, DesiredRotation;

	// Animation.
	unreliable if ( (!bSkipActorPropertyReplication || bNetInitial) && (Role==ROLE_Authority) )
		bHidden, bHardAttach;

	// Properties changed using accessor functions (Owner, rendering, and collision)
	unreliable if ( (!bSkipActorPropertyReplication || bNetInitial) && (Role==ROLE_Authority) && bNetDirty )
		bOnlyOwnerSee;

	unreliable if ( (!bSkipActorPropertyReplication || bNetInitial) && (Role==ROLE_Authority) && bNetDirty
					&& (bCollideActors || bCollideWorld) )
		bProjTarget, bBlockActors;

	// Properties changed only when spawning or in script (relationships, rendering, lighting)
	unreliable if ( (!bSkipActorPropertyReplication || bNetInitial) && (Role==ROLE_Authority) )
		Role,RemoteRole,bNetOwner,bTearOff;

	unreliable if ( (!bSkipActorPropertyReplication || bNetInitial) && (Role==ROLE_Authority)
					&& bNetDirty && bReplicateInstigator )
		Instigator;

	// Infrequently changed mesh properties
	unreliable if ( (!bSkipActorPropertyReplication || bNetInitial) && (Role==ROLE_Authority)
					&& bNetDirty )
		PrePivot;

	// Properties changed using accessor functions (Owner, rendering, and collision)
	unreliable if ( (!bSkipActorPropertyReplication || bNetInitial) && (Role==ROLE_Authority) && bNetDirty )
		Owner;


}

//=============================================================================
// Actor error handling.

// Handle an error and kill this one actor.
native(233) final function Error( coerce string S );

//=============================================================================
// General functions.

// Latent functions.
native(256) final latent function Sleep( float Seconds );
native(261) final latent function FinishAnim( AnimNodeSequence SeqNode );

// Collision.
native(262) final function SetCollision( optional bool NewColActors, optional bool NewBlockActors );
native(283) final function bool SetCollisionSize( float NewRadius, float NewHeight );
native final function SetDrawScale(float NewScale);
native final function SetDrawScale3D(vector NewScale3D);
native final function SetPrePivot(vector NewPrePivot);
native final function OnlyAffectPawns(bool B);

// Movement.
native(266) final function bool Move( vector Delta );
native(267) final function bool SetLocation( vector NewLocation );
native(299) final function bool SetRotation( rotator NewRotation );

// SetRelativeRotation() sets the rotation relative to the actor's base
native final function bool SetRelativeRotation( rotator NewRotation );
native final function bool SetRelativeLocation( vector NewLocation );

native(3969) final function bool MoveSmooth( vector Delta );
native(3971) final function AutonomousPhysics(float DeltaSeconds);

// Relations.
native(298) final function SetBase( actor NewBase, optional vector NewFloor, optional SkeletalMeshComponent SkelComp, optional name BoneName );
native(272) final function SetOwner( actor NewOwner );

event ReplicatedEvent(string VarName);	// Called when a variable with the property flag "RepNotify" is replicated

//=========================================================================
// Rendering.

native final function DrawDebugLine( vector LineStart, vector LineEnd, byte R, byte G, byte B); // SLOW! Use for debugging only!

/** Draw a persistent line */
native final function DrawPersistentDebugLine( vector LineStart, vector LineEnd, byte R, byte G, byte B); // SLOW! Use for debugging only!

/** Flush persistent lines */
native final function FlushPersistentDebugLines();

/** Draw some value over time onto the StatChart. Toggle on and off with */
native final function ChartData(string DataName, float DataValue);

//=========================================================================
// Physics.

native final function DebugClock();
native final function DebugUnclock();

native(3970) final function SetPhysics( EPhysics newPhysics );

// Timing
native final function Clock(out float time);
native final function UnClock(out float time);

// Components
native final function ActorComponent AddComponent(ActorComponent NewComponent, bool bDuplicate);

//=========================================================================
// Engine notification functions.

//
// Major notifications.
//
event Destroyed();
event GainedChild( Actor Other );
event LostChild( Actor Other );
event Tick( float DeltaTime );

//
// Physics & world interaction.
//
event Timer();
event HitWall( vector HitNormal, actor HitWall );
event Falling();
event Landed( vector HitNormal );
event ZoneChange( ZoneInfo NewZone );
event PhysicsVolumeChange( PhysicsVolume NewVolume );
event Touch( Actor Other, vector HitLocation, vector HitNormal );
event PostTouch( Actor Other ); // called for PendingTouch actor after physics completes
event UnTouch( Actor Other );
event Bump( Actor Other, Vector HitNormal );
event BaseChange();
event Attach( Actor Other );
event Detach( Actor Other );
event Actor SpecialHandling(Pawn Other);
event bool EncroachingOn( actor Other );
event EncroachedBy( actor Other );
event RanInto( Actor Other );	// called for encroaching actors which successfully moved the other actor out of the way

/** Clamps out_Rot between the upper and lower limits offset from the base */
simulated final native function bool ClampRotation( out Rotator out_Rot, Rotator rBase, Rotator rUpperLimits, Rotator rLowerLimits );
/** Called by ClampRotation if the rotator was outside of the limits */
simulated event bool OverRotated( out Rotator out_Desired, out Rotator out_Actual );

/**
 * Called when being activated by the specified pawn.  Default
 * implementation searches for any SeqEvent_Used and activates
 * them.
 * 
 * @return		true to indicate this actor was activated
 */
event bool UsedBy(Pawn user)
{
	local int idx, activatedCnt;
	local SeqEvent_Used usedEvent;
	for (idx = 0; idx < GeneratedEvents.Length; idx++)
	{
		usedEvent = SeqEvent_Used(GeneratedEvents[idx]);
		if (usedEvent != None &&
			usedEvent.bEnabled)
		{
			ActivateEvent(usedEvent,user);
			activatedCnt++;
		}
	}
	return (activatedCnt > 0);
}

simulated event FellOutOfWorld(class<DamageType> dmgType)
{
	SetPhysics(PHYS_None);
	Destroy();
}

/**
 * Trace a line and see what it collides with first.
 * Takes this actor's collision properties into account.
 * Returns first hit actor, Level if hit level, or None if hit nothing.
 */
native(277) final function Actor Trace
(
	out vector					HitLocation,
	out vector					HitNormal,
	vector						TraceEnd,
	optional vector				TraceStart,
	optional bool				bTraceActors,
	optional vector				Extent,
	optional out TraceHitInfo	HitInfo,
	optional bool				bTraceWater
);

/** 
 *	Run a line check against just this PrimitiveComponent. Return TRUE if we hit. 
 *  NOTE: the actual Actor we call this on is irrelevant!
 */
native final function bool TraceComponent
(	
	out vector						HitLocation,
	out vector						HitNormal,
	PrimitiveComponent				InComponent,
	vector							TraceEnd,
	optional vector					TraceStart,
	optional vector					Extent,
	optional out TraceHitInfo		HitInfo 
);

// returns true if did not hit world geometry
native(548) final function bool FastTrace
(
	vector          TraceEnd,
	optional vector TraceStart
);

native final function bool ContainsPoint(vector Spot);
native final function bool TouchingActor(Actor A);
native final function GetComponentsBoundingBox(out box ActorBox);
native final function GetBoundingCylinder(out float CollisionRadius, out float CollisionHeight);

//
// Spawn an actor. Returns an actor of the specified class, not
// of class Actor (this is hardcoded in the compiler). Returns None
// if the actor could not be spawned (either the actor wouldn't fit in
// the specified location, or the actor list is full).
// Defaults to spawning at the spawner's location.
//
native(278) final function actor Spawn
(
	class<actor>      SpawnClass,
	optional actor	  SpawnOwner,
	optional name     SpawnTag,
	optional vector   SpawnLocation,
	optional rotator  SpawnRotation
);

//
// Destroy this actor. Returns true if destroyed, false if indestructable.
// Destruction is latent. It occurs at the end of the tick.
//
native(279) final function bool Destroy();

// Networking - called on client when actor is torn off (bTearOff==true)
event TornOff();

//=============================================================================
// Timing.

/* epic ===============================================
* ::SetTimer
*
* Sets a timer to call the given function at a set
* interval.  Defaults to calling the 'Timer' event if
* no function is specified.  If inRate is set to
* 0.f it will effectively disable the previous timer.
*
* NOTE: Functions with parameters are not supported!
*
* =====================================================
*/
native(280) final function SetTimer(float inRate, optional bool inbLoop, optional Name inTimerFunc);

/* epic ===============================================
* ::ClearTimer
*
* Clears a previously set timer, identical to calling
* SetTimer() with a <= 0.f rate.
*
* =====================================================
*/
native final function ClearTimer(optional Name inTimerFunc);

/* epic ===============================================
* ::IsTimerActive
*
* Returns true if the specified timer is active, defaults
* to 'Timer' if no function is specified.
*
* =====================================================
*/
native final function bool IsTimerActive(optional Name inTimerFunc);

/* epic ===============================================
* ::GetTimerCount
*
* Gets the current count for the specified timer, defaults
* to 'Timer' if no function is specified.  Returns -1.f
* if the timer is not currently active.
*
* =====================================================
*/
native final function float GetTimerCount(optional Name inTimerFunc);

//=============================================================================
// Sound functions.

/* Create an audio component.
*/
native final function AudioComponent CreateAudioComponent( SoundCue InSoundCue, optional bool Play );

/* Play a sound.
*/

function PlaySound( SoundCue InSoundCue )
{
	CreateAudioComponent( InSoundCue, true );
}

//=============================================================================
// AI functions.

/* Inform other creatures that you've made a noise
 they might hear (they are sent a HearNoise message)
 Senders of MakeNoise should have an instigator if they are not pawns.
*/
native(512) final function MakeNoise( float Loudness );

/* PlayerCanSeeMe returns true if any player (server) or the local player (standalone
or client) has a line of sight to actor's location.
*/
native(532) final function bool PlayerCanSeeMe();

native final function vector SuggestFallVelocity(vector Destination, vector Start, float MaxZ, float MaxXYSpeed);

//=============================================================================
// Regular engine functions.

// Teleportation.
event bool PreTeleport( Teleporter InTeleporter );
event PostTeleport( Teleporter OutTeleporter );

// Level state.
event BeginPlay();

//========================================================================
// Disk access.

// Find files.
native(547) final function string GetURLMap();
native final function bool GetCacheEntry( int Num, out string GUID, out string Filename );
native final function bool MoveCacheEntry( string GUID, optional string NewFilename );

//=============================================================================
// Iterator functions.

// Iterator functions for dealing with sets of actors.

/* AllActors() - avoid using AllActors() too often as it iterates through the whole actor list and is therefore slow
*/
native(304) final iterator function AllActors     ( class<actor> BaseClass, out actor Actor, optional name MatchTag );

/* DynamicActors() only iterates through the non-static actors on the list (still relatively slow, bu
 much better than AllActors).  This should be used in most cases and replaces AllActors in most of
 Epic's game code.
*/
native(313) final iterator function DynamicActors     ( class<actor> BaseClass, out actor Actor, optional name MatchTag );

/* ChildActors() returns all actors owned by this actor.  Slow like AllActors()
*/
native(305) final iterator function ChildActors   ( class<actor> BaseClass, out actor Actor );

/* BasedActors() returns all actors based on the current actor (slow, like AllActors)
*/
native(306) final iterator function BasedActors   ( class<actor> BaseClass, out actor Actor );

/* TouchingActors() returns all actors touching the current actor (fast)
*/
native(307) final iterator function TouchingActors( class<actor> BaseClass, out actor Actor );

/* TraceActors() return all actors along a traced line.  Reasonably fast (like any trace)
*/
native(309) final iterator function TraceActors   ( class<actor> BaseClass, out actor Actor, out vector HitLoc, out vector HitNorm, vector End, optional vector Start, optional vector Extent );

/* VisibleActors() returns all visible (not bHidden) actors within a radius
for which a trace from Loc (which defaults to caller's Location) to that actor's Location does not hit the world.
Slow like AllActors(). Use VisibleCollidingActors() instead if desired actor types are in the collision hash (bCollideActors is true)
*/
native(311) final iterator function VisibleActors ( class<actor> BaseClass, out actor Actor, optional float Radius, optional vector Loc );

/* VisibleCollidingActors() returns all colliding (bCollideActors==true) actors within a certain radius
for which a trace from Loc (which defaults to caller's Location) to that actor's Location does not hit the world.
Much faster than AllActors() since it uses the collision hash
*/
native(312) final iterator function VisibleCollidingActors ( class<actor> BaseClass, out actor Actor, float Radius, optional vector Loc, optional bool bIgnoreHidden );

/* CollidingActors() returns colliding (bCollideActors==true) actors within a certain radius.
Much faster than AllActors() for reasonably small radii since it uses the collision hash
*/
native(321) final iterator function CollidingActors ( class<actor> BaseClass, out actor Actor, float Radius, optional vector Loc );

/**
 * Returns colliding (bCollideActors==true) which overlap a Sphere from location 'Loc' and 'Radius' radius.
 *
 * @param BaseClass		The Actor returns must be a subclass of this.
 * @param out_Actor		returned Actor at each iteration.
 * @param Radius		Radius of sphere for overlapping check.
 * @param Loc			Center of sphere for overlapping check. (Optional, caller's location is used otherwise).
 * @param bIgnoreHidden	if true, ignore bHidden actors.
 */
native final iterator function OverlappingActors( class<Actor> BaseClass, out Actor out_Actor, float Radius, optional vector Loc, optional bool bIgnoreHidden );

/**
 iterator LocalPlayerControllers()
 returns all locally rendered/controlled player controllers (typically 1 per client, unless split screen)
*/
native final iterator function LocalPlayerControllers(out PlayerController PC);

//=============================================================================
// Color functions
native(549) static final operator(20) color -     ( color A, color B );
native(550) static final operator(16) color *     ( float A, color B );
native(551) static final operator(20) color +     ( color A, color B );
native(552) static final operator(16) color *     ( color A, float B );

//=============================================================================
// Scripted Actor functions.

/* RecoverFromBadStateCode()
Called when state code execution pointer is borked, typically because the state
was changed by a GotoState() call from a function called during execution of a state
code expression.  To recover, set bBadStateCode to false and goto a valid label of a 
valid state
*/
event RecoverFromBadStateCode();

//
// Called immediately before gameplay begins.
//
event PreBeginPlay()
{
	// Handle autodestruction if desired.
	if( !bGameRelevant && (Level.NetMode != NM_Client) && !Level.Game.CheckRelevance(Self) )
		Destroy();
}

//
// Broadcast a localized message to all players.
// Most message deal with 0 to 2 related PRIs.
// The LocalMessage class defines how the PRI's and optional actor are used.
//
event BroadcastLocalizedMessage( class<LocalMessage> MessageClass, optional int Switch, optional PlayerReplicationInfo RelatedPRI_1, optional PlayerReplicationInfo RelatedPRI_2, optional Object OptionalObject )
{
	Level.Game.BroadcastLocalized( self, MessageClass, Switch, RelatedPRI_1, RelatedPRI_2, OptionalObject );
}

// Called immediately after gameplay begins.
//
event PostBeginPlay();

// Called after PostBeginPlay.
//
simulated event SetInitialState()
{
	bScriptInitialized = true;
	if( InitialState!='' )
		GotoState( InitialState );
	else
		GotoState( 'Auto' );
}

// called after PostBeginPlay.  On a net client, PostNetBeginPlay() is spawned after replicated variables have been initialized to
// their replicated values
event PostNetBeginPlay();

/* HurtRadius()
 Hurt locally authoritative actors within the radius.
*/
simulated final function HurtRadius
( 
	float				BaseDamage, 
	float				DamageRadius, 
	class<DamageType>	DamageType, 
	float				Momentum, 
	vector				HurtOrigin 
)
{
	local Actor	Victim;

	// Prevent Hurt Radius from being reentrant.
	if ( bHurtEntry )
		return;

	bHurtEntry = true;
	foreach VisibleCollidingActors( class'Actor', Victim, DamageRadius, HurtOrigin )
	{
		// don't let blast damage affect fluid - VisibleCollisingActors doesn't really work for them - jag
		if ( (Victim != self) && (Victim.Role == ROLE_Authority) )
		{
			Victim.TakeRadiusDamage( Instigator, BaseDamage, DamageRadius, DamageType, Momentum, HurtOrigin );
		}
	}
	bHurtEntry = false;
}

//
// Damage and kills.
//
event KilledBy( pawn EventInstigator );
event TakeDamage( int Damage, Pawn EventInstigator, vector HitLocation, vector Momentum, class<DamageType> DamageType, optional TraceHitInfo HitInfo)
{
	local int idx;
	local SeqEvent_TakeDamage dmgEvent;
	// search for any damage events
	for (idx = 0; idx < GeneratedEvents.Length; idx++)
	{
		dmgEvent = SeqEvent_TakeDamage(GeneratedEvents[idx]);
		if (dmgEvent != None)
		{
			// notify the event of the damage received
			dmgEvent.HandleDamage(self,EventInstigator,DamageType,Damage);
		}
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
	local float		ColRadius, ColHeight;
	local float		damageScale, dist;
	local vector	dir;

	GetBoundingCylinder(ColRadius, ColHeight);

	dir			= Location - HurtOrigin;
	dist		= FMax(1,VSize(dir));
	dir			= dir/dist;

	dist		= FClamp(dist - ColRadius, 0.f, DamageRadius);
	damageScale = 1 - dist/DamageRadius;

	TakeDamage
	(
		damageScale * BaseDamage,
		InstigatedBy,
		Location - 0.5 * (ColHeight + ColRadius) * dir,
		(damageScale * Momentum * dir),
		DamageType,
	);
}

/**
 * Make sure we pass along a valid HitInfo struct for damage.
 * The main reason behind this is that SkeletalMeshes do require a BoneName to receive and process an impulse...
 * So if we don't have access to it (through touch() or for any non trace damage results), we need to perform an extra trace call().
 *
 * @param	HitInfo, initial structure to check
 * @param	FallBackComponent, PrimitiveComponent to use if HitInfo.HitComponent is none
 * @param	Dir, Direction to use if a Trace needs to be performed to find BoneName on skeletalmesh. Trace from HitLocation.
 * @param	out_HitLocation, HitLocation to use for potential Trace, will get updated by Trace.
 */
final simulated function CheckHitInfo
(
	out	TraceHitInfo		HitInfo, 
		PrimitiveComponent	FallBackComponent, 
		Vector				Dir, 
	out Vector				out_HitLocation 
)
{
	local vector			out_NewHitLocation, out_HitNormal, TraceEnd, TraceStart;
	local TraceHitInfo		newHitInfo;

	//log("Actor::CheckHitInfo - HitInfo.HitComponent:" @ HitInfo.HitComponent @ "FallBackComponent:" @ FallBackComponent );

	// we're good, return!
	if( SkeletalMeshComponent(HitInfo.HitComponent) != None && HitInfo.BoneName != '' )
	{ 
		return;
	}

	// Use FallBack PrimitiveComponent if possible
	if( HitInfo.HitComponent == None ||
		(SkeletalMeshComponent(HitInfo.HitComponent) == None && SkeletalMeshComponent(FallBackComponent) != None) )
	{
		HitInfo.HitComponent = FallBackComponent;
	}

	// if we do not have a valid BoneName, perform a trace against component to try to find one.
	if( SkeletalMeshComponent(HitInfo.HitComponent) != None && HitInfo.BoneName == '' )
	{
		if( IsZero(Dir) )
		{
			warn("passed zero dir for trace");
			Dir = Vector(Rotation);
		}

		if( IsZero(out_HitLocation) )
		{
			warn("IsZero(out_HitLocation)");
			assert(false);
		}

		TraceStart	= out_HitLocation - 128 * Normal(Dir);
		TraceEnd	= out_HitLocation + 128 * Normal(Dir);

		if( TraceComponent( out_NewHitLocation, out_HitNormal, HitInfo.HitComponent, TraceEnd, TraceStart, vect(0,0,0), newHitInfo ) )
		{	// Update HitLocation
			HitInfo.BoneName	= newHitInfo.BoneName;
			out_HitLocation		= out_NewHitLocation;
		}
		else
		{
			// FIXME LAURENT -- The test fails when a just spawned projectile triggers a touch() event, the trace performed will be slightly off and fail.
			log("Actor::CheckHitInfo non successful TraceComponent!!");
			log("HitInfo.HitComponent:" @ HitInfo.HitComponent );
			log("TraceEnd:" @ TraceEnd );
			log("TraceStart:" @ TraceStart );
			log("out_HitLocation:" @ out_HitLocation );

			ScriptTrace();
			//DrawPersistentDebugLine(TraceEnd, TraceStart, 255, 0, 0 );
			//DebugFreezeGame();
		}
	}
}

/**
 * Debug Freeze Game
 * dumps the current script function stack and pauses the game with PlayersOnly (still allowing the player to move around).
 */
function DebugFreezeGame()
{
	local PlayerController	PC;

	ScriptTrace();
	ForEach LocalPlayerControllers(PC)
	{
		PC.ConsoleCommand("PlayersOnly");
		return;
	}
}

function bool CheckForErrors();

// Called when carried onto a new level, before AcceptInventory.
//
event TravelPreAccept();

// Called when carried into a new level, after AcceptInventory.
//
event TravelPostAccept();

/* BecomeViewTarget
	Called by Camera when this actor becomes its ViewTarget */
event BecomeViewTarget( PlayerController PC );

// Returns the string representation of the name of an object without the package
// prefixes.
//
function String GetItemName( string FullName )
{
	local int pos;

	pos = InStr(FullName, ".");
	While ( pos != -1 )
	{
		FullName = Right(FullName, Len(FullName) - pos - 1);
		pos = InStr(FullName, ".");
	}

	return FullName;
}

// Returns the human readable string representation of an object.
//
simulated function String GetHumanReadableName()
{
	return GetItemName(string(class));
}

final function ReplaceText(out string Text, string Replace, string With)
{
	local int i;
	local string Input;

	Input = Text;
	Text = "";
	i = InStr(Input, Replace);
	while(i != -1)
	{
		Text = Text $ Left(Input, i) $ With;
		Input = Mid(Input, i + Len(Replace));
		i = InStr(Input, Replace);
	}
	Text = Text $ Input;
}

// Get localized message string associated with this actor
static function string GetLocalString(
	optional int Switch,
	optional PlayerReplicationInfo RelatedPRI_1,
	optional PlayerReplicationInfo RelatedPRI_2
	)
{
	return "";
}

function MatchStarting(); // called when gameplay actually starts
function SetGRI(GameReplicationInfo GRI);

function String GetDebugName()
{
	return GetItemName(string(self));
}

/** 
 * list important Actor variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local string	T;
	local Actor		A;
	local float MyRadius, MyHeight;
	local Canvas Canvas;

	Canvas = HUD.Canvas;

	Canvas.SetPos(4, out_YPos);
	Canvas.SetDrawColor(255,0,0);
	T = GetDebugName();
	if ( bDeleteMe )
		T = T$" DELETED (bDeleteMe == true)";
	if ( T != "" )
	{
		Canvas.DrawText(T, false);
		out_YPos += out_YL;
		Canvas.SetPos(4, out_YPos);
	}
	Canvas.SetDrawColor(255,255,255);

	if ( HUD.bShowNetworkDebug )
	{
		if ( Level.NetMode != NM_Standalone )
		{
			// networking attributes
			T = "ROLE ";
			Switch(Role)
			{
				case ROLE_None: T=T$"None"; break;
				case ROLE_SimulatedProxy: T=T$"SimulatedProxy"; break;
				case ROLE_AutonomousProxy: T=T$"AutonomousProxy"; break;
				case ROLE_Authority: T=T$"Authority"; break;
			}
			T = T$" REMOTE ROLE ";
			Switch(RemoteRole)
			{
				case ROLE_None: T=T$"None"; break;
				case ROLE_SimulatedProxy: T=T$"SimulatedProxy"; break;
				case ROLE_AutonomousProxy: T=T$"AutonomousProxy"; break;
				case ROLE_Authority: T=T$"Authority"; break;
			}
			if ( bTearOff )
				T = T$" Tear Off";
			Canvas.DrawText(T, false);
			out_YPos += out_YL;
			Canvas.SetPos(4, out_YPos);
		}
	}

	if ( HUD.bShowPhysicsDebug )
	{
		T = "Physics" @ GetPhysicsName() @ "in physicsvolume" @ GetItemName(string(PhysicsVolume)) @ "on base" @ GetItemName(string(Base)) @ "gravity" @ PhysicsVolume.Gravity.Z;
		if ( bBounce )
			T = T$" - will bounce";
		Canvas.DrawText(T, false);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);

		Canvas.DrawText("Location: "$Location$" Rotation "$Rotation, false);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
		Canvas.DrawText("Velocity: "$Velocity$" Speed "$VSize(Velocity)$" Speed2D "$VSize(Velocity-Velocity.Z*vect(0,0,1)), false);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
		Canvas.DrawText("Acceleration: "$Acceleration, false);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}

	if ( HUD.bShowCollisionDebug )
	{
		Canvas.DrawColor.B = 0;
		GetBoundingCylinder(MyRadius,MyHeight);
		Canvas.DrawText("Collision Radius "$MyRadius$" Height "$MyHeight);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);

		Canvas.DrawText("Collides with Actors "$bCollideActors$", world "$bCollideWorld$", proj. target "$bProjTarget);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
		Canvas.DrawText("Blocks Actors "$bBlockActors);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);

		T = "Touching ";
		ForEach TouchingActors(class'Actor', A)
			T = T$GetItemName(string(A))$" ";
		if ( T == "Touching ")
			T = "Touching nothing";
		Canvas.DrawText(T, false);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}

	Canvas.DrawColor.B = 255;
	Canvas.DrawText("Tag: "$Tag$" STATE: "$GetStateName(), false);
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);

	Canvas.DrawText("Instigator "$GetItemName(string(Instigator))$" Owner "$GetItemName(string(Owner)));
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);
}

simulated function String GetPhysicsName()
{
	Switch( PHYSICS )
	{
		case PHYS_None:				return "None"; break;
		case PHYS_Walking:			return "Walking"; break;
		case PHYS_Falling:			return "Falling"; break;
		case PHYS_Swimming:			return "Swimming"; break;
		case PHYS_Flying:			return "Flying"; break;
		case PHYS_Rotating:			return "Rotating"; break;
		case PHYS_Projectile:		return "Projectile"; break;
		case PHYS_Interpolating:	return "Interpolating"; break;
		case PHYS_Spider:			return "Spider"; break;
		case PHYS_Ladder:			return "Ladder"; break;
	}
}

/* Reset()
reset actor to initial state - used when restarting level without reloading.
*/
function Reset();

function bool IsInVolume(Volume aVolume)
{
	local Volume V;

	ForEach TouchingActors(class'Volume',V)
		if ( V == aVolume )
			return true;
	return false;
}

function bool IsInPain()
{
	local PhysicsVolume V;

	ForEach TouchingActors(class'PhysicsVolume',V)
		if ( V.bPainCausing && (V.DamagePerSec > 0) )
			return true;
	return false;
}

function PlayTeleportEffect(bool bOut, bool bSound);

simulated function bool CanSplash()
{
	return false;
}

simulated function bool EffectIsRelevant(vector SpawnLocation, bool bForceDedicated )
{
	local PlayerController	P;
	local bool				bResult;

	if ( Level.NetMode == NM_DedicatedServer )
	{
		return bForceDedicated;
	}

	if ( Level.NetMode != NM_Client )
	{
		bResult = true;
	}
	else if ( (Instigator != None) && Instigator.IsHumanControlled() )
	{
		return true;
	}
	else if ( SpawnLocation == Location )
	{
		bResult = ( Level.TimeSeconds - LastRenderTime < 3 );
	}
	else if ( (Instigator != None) && (Level.TimeSeconds - Instigator.LastRenderTime < 3) )
	{
		bResult = true;
	}

	if ( bResult )
	{
		bResult = false;
		ForEach LocalPlayerControllers(P)
		{
			if ( P.ViewTarget != None )
			{
				if ( P.Pawn == Instigator )
				{
					bResult = true; // FIXME CheckMaxEffectDistance(P, SpawnLocation);
					break;
				}
				else if ( (Vector(P.Rotation) Dot (SpawnLocation - P.ViewTarget.Location)) < 0.0 )
				{
					bResult = (VSize(P.ViewTarget.Location - SpawnLocation) < 1600);
					break;
				}
				else
				{
					bResult = true; // FIXME CheckMaxEffectDistance(P, SpawnLocation);
					break;
				}
			}
		}
	}
	return bResult;
}

//-----------------------------------------------------------------------------
// Scripting support

/**
 * Handles activating the specified event using this actor as
 * the instigator.
 * 
 * @return		true if the event was activated
 * @see			UnSequence.cpp for implementation
 */
final native function bool ActivateEvent(SequenceEvent inEvent,Actor inInstigator);

/**
 * Iterates through the GeneratedEvents list and looks for all
 * matching events, activating them as found.
 * 
 * @return		true if an event was found and activated
 */
final function bool ActivateEventClass(class<SequenceEvent> inClass,Actor inInstigator)
{
	local int idx;
	local bool bActivated;
	for (idx = 0; idx < GeneratedEvents.Length; idx++)
	{
		if (ClassIsChildOf(GeneratedEvents[idx].Class,inClass))
		{
			bActivated = bActivated || ActivateEvent(GeneratedEvents[idx],inInstigator);
		}
	}
	return bActivated;
}

/**
 * Builds a list of all events of the specified class.
 * 
 * @param	eventClass - type of event to search for
 * @param	out_EventList - list of found events
 * 
 * @return	true if any events were found
 */
final function bool FindEventsOfClass(class<SequenceEvent> eventClass, out array<SequenceEvent> out_EventList)
{
	local int idx;
	local bool bFoundEvent;
	for (idx = 0; idx < GeneratedEvents.Length; idx++)
	{
		if (ClassIsChildOf(GeneratedEvents[idx].Class,eventClass) &&
			GeneratedEvents[idx].bEnabled &&
			(GeneratedEvents[idx].MaxTriggerCount == 0 ||
			 GeneratedEvents[idx].MaxTriggerCount > GeneratedEvents[idx].TriggerCount))
		{
			out_EventList[out_EventList.Length] = GeneratedEvents[idx];
			bFoundEvent = true;
		}
	}
	return bFoundEvent;
}

/**
 * Clears all latent actions of the specified class.
 * 
 * @param	actionClass - type of latent action to clear
 * @param	bAborted - was this latent action aborted?
 * @param	exceptionAction - action to skip
 */
final function ClearLatentAction(class<SeqAct_Latent> actionClass,optional bool bAborted,optional SeqAct_Latent exceptionAction)
{
	local int idx;
	for (idx = 0; idx < LatentActions.Length; idx++)
	{
		if (LatentActions[idx] == None)
		{
			// remove dead entry
			LatentActions.Remove(idx--,1);
		}
		else
		if (ClassIsChildOf(LatentActions[idx].class,actionClass) &&
			LatentActions[idx] != exceptionAction)
		{
			// if aborted,
			if (bAborted)
			{
				// then notify the action
				LatentActions[idx].AbortFor(self);
			}
			// remove action from list
			LatentActions.Remove(idx--,1);
		}
	}
}

/**
 * If this actor is not already scheduled for destruction,
 * destroy it now.
 */
simulated function OnDestroy(SeqAct_Destroy inAction)
{
	if (!bDeleteMe)
	{
		Destroy();
	}
}

/**
 * Called upon receiving a SeqAct_CauseDamage action, calls
 * TakeDamage() with the given parameters.
 * 
 * @param	inAction - damage action that was activated
 */
simulated function OnCauseDamage(SeqAct_CauseDamage inAction)
{
	//@todo - figure out the instigator
	TakeDamage(inAction.DamageAmount,None,Location,vect(0,0,0),inAction.DamageType);
}

/**
 * Called upon receiving a SeqAct_Teleport action.  Grabs
 * the first destination available and attempts to teleport
 * this actor.
 * 
 * @param	inAction - teleport action that was activated
 */
simulated function OnTeleport(SeqAct_Teleport inAction)
{
	local array<Object> objVars;
	local int idx;
	local Actor destActor;
	local Controller C;

	// find the first supplied actor
	inAction.GetObjectVars(objVars,"Destination");
	for (idx = 0; idx < objVars.Length && destActor == None; idx++)
	{
		destActor = Actor(objVars[idx]);

		// If its a player variable, teleport to the Pawn not the Controller.
		C = Controller(destActor);
		if(C != None && C.Pawn != None)
		{
			destActor = C.Pawn;
		}
	}

	// and set to that actor's location
	if (destActor != None && SetLocation(destActor.Location))
	{
		PlayTeleportEffect(false, true);
	}
	else
	{
		Warn("Unable to teleport to"@destActor);
	}
}

/** 
 *	Handler for the SeqAct_SetBlockRigidBody action. Allows level designer to toggle the rigid-body blocking
 *	flag on an Actor, and will handle updating the physics engine etc.
 */
simulated function OnSetBlockRigidBody(SeqAct_SetBlockRigidBody inAction)
{
	if(CollisionComponent != None)
	{
		// Turn on
		if(inAction.InputLinks[0].bHasImpulse)
		{
			CollisionComponent.SetBlockRigidBody(true);
		}
		// Turn off
		else if(inAction.InputLinks[1].bHasImpulse)
		{
			CollisionComponent.SetBlockRigidBody(false);
		}
	}
}

/** Handler for the SeqAct_SetPhysics action, allowing designer to change the Physics mode of an Actor. */
simulated function OnSetPhysics(SeqAct_SetPhysics inAction)
{
	SetPhysics( inAction.NewPhysics );
}

/**
 * Event called when an AnimNodeSequence (in the animation tree of one of this Actor's SkeletalMeshComponents) reaches the end and stops.
 * Will not get called if bLooping is 'true' on the AnimNodeSequence.
 * bCauseActorAnimEnd must be set 'true' on the AnimNodeSequence for this event to get generated.
 *
 * @param	SeqNode - Node that finished playing. You can get to the SkeletalMeshComponent by looking at SeqNode->SkelComponent
 */
event OnAnimEnd(AnimNodeSequence SeqNode);

/**
 * Event called when a PlayAnim is called AnimNodeSequence in the animation tree of one of this Actor's SkeletalMeshComponents.
 * bCauseActorAnimPlay must be set 'true' on the AnimNodeSequence for this event to get generated.
 *
 * @param	SeqNode - Node had PlayAnim called. You can get to the SkeletalMeshComponent by looking at SeqNode->SkelComponent
 */
event OnAnimPlay(AnimNodeSequence SeqNode);


/**
 * returns the point of view of the actor.
 * note that this doesn't mean the camera, but the 'eyes' of the actor.
 * For example, for a Pawn, this would define the eye height location, 
 * and view rotation (which is different from the pawn rotation which has a zeroed pitch component).
 * A camera first person view will typically use this view point. Most traces (weapon, AI) will be done from this view point.
 *
 * @param	out_Location - location of view point
 * @param	out_Rotation - view rotation of actor.
 */
simulated event GetActorEyesViewPoint( out vector out_Location, out Rotator out_Rotation )
{
	out_Location = Location;
	out_Rotation = Rotation;
}

/**
 * Searches the owner chain looking for a player.
 */
static function bool IsPlayerOwned(Actor inActor)
{
	local Actor testActor;
	testActor = inActor;
	while (testActor != None)
	{
		if (testActor.IsA('PlayerController'))
		{
			return true;
		}
		testActor = testActor.Owner;
	}
	return false;
}

/* PawnBaseDied()
The pawn on which this actor is based has just died
*/
function PawnBaseDied();

simulated function byte GetTeamNum()
{
	return 255;
}

simulated function string GetLocationStringFor(PlayerReplicationInfo PRI)
{
	return "";
}

defaultproperties
{
	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EngineResources.S_Actor'
		HiddenGame=true
	End Object
	Components.Add(Sprite)

	Begin Object Class=CylinderComponent Name=CollisionCylinder
		CollisionRadius=10
		CollisionHeight=10
		AlwaysLoadOnClient=True
		AlwaysLoadOnServer=True
	End Object
	Components.Add(CollisionCylinder)
	CollisionComponent=CollisionCylinder

	DrawScale=+00001.000000
	DrawScale3D=(X=1,Y=1,Z=1)
	bJustTeleported=true
	Mass=+00100.000000
	Role=ROLE_Authority
	RemoteRole=ROLE_None
	NetPriority=+00001.000000
	bMovable=true
	InitialState=None
	NetUpdateFrequency=100
	MessageClass=class'LocalMessage'
	bHiddenEdGroup=false
	bReplicateMovement=true

	SupportedEvents(0)=class'SeqEvent_Touch'
	SupportedEvents(1)=class'SeqEvent_UnTouch'
	SupportedEvents(2)=class'SeqEvent_Destroyed'
	SupportedEvents(3)=class'SeqEvent_TakeDamage'
}