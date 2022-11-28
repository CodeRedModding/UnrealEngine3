/**
 * Weapon
 * Defines the basic functionality of a weapon.
 * Is it gametype unspecific. Subclass for game specific implementation (UTWeapon for example).
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Weapon extends Inventory
	abstract
	native
	nativereplication;

/** Array of firing states defining available firemodes */
var	Array<Name>	FiringStatesArray;

/** 
 * When incremented, calls PlayFireEffects() on network clients, except local player. (PlayFireEffects() is called directly). 
 * Can be reset to zero (when weapon is being reloaded for example */
var		byte	ShotCount;

/** Current FireMode. Replicated to all clients, and used in functions where FireMode cannot be passed as a parameter.
 * (PlayeFireEffects() on clients, BeginState(), EndState(), SynchEvent()...) */
var		byte	CurrentFireMode;

/** 
 * If true, the weapon will fire immediatly when the next SynchEvent() is called. (For example when animation finishes). 
 * bForceFire is used to keep server in synch with client. */
var		bool	bForceFire;

/** Forced FireMode number, set along with bForceFire. This is used for state transitioning. */
var		byte	ForceFireMode;

/** Set to put weapon down at the end of a state. Typically used to change weapons on state changes (weapon up, stopped firing...) */
var		bool	bWeaponPutDown;

/** Can player toss his weapon out? Typically false for default inventory. */
var		bool	bCanThrow;

/** Range of Weapon, used for Traces (TraceFire, ProjectileFire, AdjustAim...) */
var		float	WeaponRange; 

/** Pct. of time to warn target a shot has been fired at it (for AI) */
var		float	WarnTargetPct;

cpptext
{
	// Constructors.
	AWeapon() {}

	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	void PreNetReceive();
	void PostNetReceive();
	UBOOL IsNetRelevantFor( APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation );
}

//
// Network replication
//

replication
{
	// Things the server should send to ALL clients but the local player.
	reliable if( Role==ROLE_Authority && !bNetOwner && bNetDirty )
		ShotCount, CurrentFireMode;

	// Functions called by server on client
	reliable if( Role==ROLE_Authority )
		ClientWeaponSet;

	// functions called by client on server
	reliable if( Role<ROLE_Authority )
		ServerStartFire, ServerStopFire;
}

//
// Weapon Debugging
//

/**
 * Prints out weapon debug information to log file
 *
 * @param	Msg		String to display
 * @param	FuncStr	String telling where the log came from (format: Class::Function)
 */
simulated function WeaponLog( String Msg, String FuncStr )
{
	log( "[" $ Level.TimeSeconds $"]" @ Msg @ "(" $ FuncStr $ ")" );
}

simulated function PostNetBeginPlay()
{
	super.PostNetBeginPlay();

	if ( Instigator != None )
	{
		InvManager = Instigator.InvManager;
	}
}

/** @see Inventory::ItemRemovedFromInvManager */
function ItemRemovedFromInvManager()
{
	if( IsActiveWeapon() )
	{
		Pawn(Owner).Weapon = None;
	}

	super.ItemRemovedFromInvManager();
}

/**
 * Informs is this weapon is active for the player
 *
 * @return	true if this an active weapon for the player
 */
simulated function bool IsActiveWeapon()
{
	if ( InvManager != None )
	{
		return InvManager.IsActiveWeapon( Self );
	}

	return false;
}

/** 
 * list important Weapon variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local string T;
	local Canvas Canvas;

	Canvas = HUD.Canvas;
	Canvas.SetDrawColor(0,255,0);
	Canvas.DrawText("WEAPON" @ GetItemName(string(Self)) );
	out_YPos += out_YL;
	Canvas.SetPos(4, out_YPos);

	T = "     STATE:"$GetStateName() @ "Timer:"$GetTimerCount();

	Canvas.DrawText(T, false);
	out_YPos += out_YL;
	Canvas.SetPos(4, out_YPos);

	T = "     ShotCount:"$ShotCount;
	Canvas.DrawText(T, false);
	out_YPos += out_YL;
	Canvas.SetPos(4, out_YPos);
}

//
// AI
//

/**
 * Returns a weight reflecting the desire to use the
 * given weapon, used for AI and player best weapon
 * selection.
 * 
 * @return	weapon rating (range -1.f to 1.f)
 */
simulated function float GetWeaponRating()
{
	if ( InvManager != None )
	{
		return InvManager.GetWeaponRatingFor( Self );
	}

	if ( !HasAnyAmmo() )
	{
		return -1;
	}

	return 1;
}

/**
 * This Weapon has just been given to this Pawn
 *
 * @param	thisPawn	new weapon owner
 */
function GivenTo( Pawn thisPawn )
{
	super.GivenTo( thisPawn );
	ClientWeaponSet( true ); // Evaluate if we should switch to this weapon 
}

//
// Ammunition
//

/**
 * Consume Ammunition for specified firemode 
 * Network: Local player and server
 *
 * @param	FireModeNum	Fire mode
 */
function ConsumeAmmo( byte FireModeNum )
{
	if ( InvManager != None )
	{
		InvManager.WeaponConsumeAmmo( Self, FireModeNum );
	}
}

/**
 * Returns percentage of full ammo
 *
 * @param	FireModeNum	Firemode
 *
 * @return	percentage of ammo left (0 to 1.f range)
 */
simulated function float GetAmmoLeftPercent( byte FireModeNum )
{
	if ( InvManager != None )
	{
		return InvManager.GetAmmoLeftPercentFor( Self, FireModeNum );
	}
	return 1.f;
}

/**
 * Returns true if the weapon has ammo to fire for the current firemode
 * 
 * @param	FireModeNum		fire mode
 *
 * @return	true if ammo is available for Firemode FireModeNum.
 */
simulated function bool HasAmmo( byte FireModeNum )
{
	if ( InvManager != None )
	{
		return InvManager.WeaponHasAmmo( Self, FireModeNum );
	}
	return true;
}

/** 
 * Returns true if the weapon has any type of ammunition left to fire (maybe not for the current firemode)
 * 
 * @return	true if weapon has any ammo left (for any firemode)
 */
simulated function bool HasAnyAmmo()
{
	if ( InvManager != None )
	{
		return InvManager.WeaponHasAnyAmmo( Self );
	}
	return true;
}

/**
 * is called by the server to tell the client about potential weapon changes after the player runs over
 * a weapon (the client decides whether to actually switch weapons or not.
 * Network: LocalPlayer
 *
 * @param	bOptionalSet.	Set to true if the switch is optional. (simple weapon pickup and weight against current weapon).
 */
simulated function ClientWeaponSet( bool bOptionalSet )
{
	// If weapon's instigator isn't replicated to client, wait for it in PendingClientWeaponSet state
	if ( Instigator == None ) 
	{
		WeaponLog("Instigator == None, going to PendingClientWeaponSet", "Weapon::ClientWeaponSet");
		GotoState('PendingClientWeaponSet');
		return;
	}
	InvManager = Instigator.InvManager;

	// If weapon's instigator isn't replicated to client, wait for it in PendingClientWeaponSet state
	if ( InvManager == None ) 
	{
		WeaponLog("InvManager == None, going to PendingClientWeaponSet", "Weapon::ClientWeaponSet");
		GotoState('PendingClientWeaponSet');
		return;
	}

	InvManager.ClientWeaponSet( Self, bOptionalSet );
}


//
// Weapon functionality
//

/**
 * Activates the weapon. If the weapon is the inactive state, 
 * it will go to the 'Equiping' followed by 'Active' state, and ready to be fired.
 */
simulated function Activate();

/**
 * Put Down current weapon
 * Once the weapon is put down, the InventoryManager will switch to InvManager.PendingWeapon.
 *
 * @return	returns true if the weapon can be put down.
 */
simulated function bool TryPutDown()
{
	bWeaponPutDown = true;
	return true;
}

/**
 * Called on the LocalPlayer, Fire sends the shoot request to the server (ServerStartFire)
 * and them simulates the firing effects locally.
 * path: Called from PlayerController::StartFire -> Pawn::StartFire -> InventoryManager::StartFire
 * Network: LocalPlayer
 *
 * @param	FireModeNum		Fire Mode.
 */
simulated function StartFire( byte FireModeNum )
{
	if ( !HasAmmo( FireModeNum ) )
	{
		return;
	}

	//log("Weapon::Fire" @ Self @ "F:" @ F );
	if ( Role < ROLE_Authority )
	{
		// If we're a client, call server version
		ServerStartFire( FireModeNum );
	}
	SendToFiringState( FireModeNum );
}

/**
 * This function starts the firing sequence on the server.
 * Network: Server, called from LocalPlayer
 *
 * @param	FireModeNum		Fire Mode.
 */
function ServerStartFire( byte FireModeNum )
{
	if ( !HasAmmo(FireModeNum) )
	{
		return;
	}

	SendToFiringState( FireModeNum );
}

/**
 * Player released button for firemode FireModeNum
 * Network: LocalPlayer
 *
 * @param	FireModeNum		Fire Mode.
 */
simulated function StopFire( byte FireModeNum )
{
	if ( Role < Role_Authority )
	{
		ServerStopFire( FireModeNum );
	}
}

/**
 * informs server the player released button for firemode FireModeNum.
 * Network: Server
 *
 * @param	FireModeNum		Fire Mode.
 */
function ServerStopFire( byte FireModeNum )
{
	bForceFire = false;
}

/**
 * Send weapon to proper firing state
 * Also sets the CurrentFireMode.
 * Network: LocalPlayer and Server
 *
 * @param	FireModeNum Fire Mode.
 */
simulated function SendToFiringState( byte FireModeNum )
{
	// make sure fire mode is valid
	if ( FireModeNum >= FiringStatesArray.Length )
	{
		WeaponLog("Invalid FireModeNum", "Weapon::SendToFiringState");
		GotoState('Active');
		return;
	}

	CurrentFireMode = FireModeNum;
	GotoState( FiringStatesArray[FireModeNum] );
}

/**
 * Plays the firing effects locally.
 * State scoped function, called from FiringState::BeginState. Override in proper state
 * Network: LocalPlayer and Server
 *
 * @param	FireModeNum		Fire Mode.
 */
simulated function LocalFire( byte FireModeNum )
{
	if ( Owner == None )
	{
		WeaponLog("Owner == None, gotostate Inactive", "Weapon::LocalFire");
		GotoState('Inactive');
		return;
	}

	TimeWeaponFiring( FireModeNum );

	ShotCount++;
	PlayFireEffects( FireModeNum );
	
	if ( Role == Role_Authority )
	{
		ConsumeAmmo( FireModeNum );
		FireAmmunition( FireModeNum );
		Owner.MakeNoise(1.0);
	}
}

/**
 * Sets the timing for the firing state on server and client.
 * For example plays the fire animation locally, should always be subclassed (must call SynchEvent() to properly time firing)
 * State scoped function. Override in proper state
 * Network: LocalPlayer and Server
 *
 * @param	FireModeNum		Fire Mode.
 */
simulated function TimeWeaponFiring( byte FireModeNum );

/**
 * Called when a Shot is fired.
 * Called by C++ AWeapon::PostNetReceive() on the client if ShotCount incremented on the server
 * OR called locally for local player.
 * NOTE:	this is not a state scoped function (non local clients ignore the state the weapon is in)
 *			non local clients use the replicated CurrentFireMode (passed as parameter F from native code).
 * Network: ALL
 *
 * @param	FireModeNum		Fire Mode.
 */
simulated event PlayFireEffects( byte FireModeNum );

/**
 * Sets the timing for the PuttingDown state on server and client.
 * For example plays the 'PutDown' animation locally, should always be subclassed (must call SynchEvent() to properly time and exit state)
 * Network: LocalPlayer and Server
 */
simulated function TimeWeaponPutDown()
{
	SetTimer(0.1, false);
}


/**
 * Sets the timing for the Equiping state on server and client.
 * For example plays the 'Activating / Equip' animation locally, should always be subclassed (must call SynchEvent() to properly time and exit state)
 * Network: LocalPlayer and Server
 */
simulated function TimeWeaponEquiping()
{
	SetTimer(0.1, false);
}

/**
 * Play the idle Animation
 * Note: This is not required by default for timing and synchronisation
 * Network: LocalPlayer and Server
 */
simulated function PlayWeaponIdle();

//
// Aiming
//

/**
 * Physical fire start location. (from weapon's barrel in world coordinates) 
 * Typically used when spawning projectiles or visible effects. Traces use Instigator.GetWeaponStartTraceLocation()
 * State scoped function. Override in proper state.
 */
simulated function Vector GetPhysicalFireStartLoc()
{
	return Instigator.GetPawnViewLocation();
}

/**
 * Called before firing weapon to adjust Trace/Projectile aiming
 * State scoped function. Override in proper state
 * Network: Server
 *
 * @param	StartFireLoc,	world location of weapon start trace or projectile spawning
 */
simulated function Rotator GetAdjustedAim( vector StartFireLoc )
{
	return Instigator.GetAdjustedAimFor( Self, StartFireLoc );
}

/**
 * Fires the weapon = spawn projectile or perform a trace
 * Override and call TraceFire() or ProjectileFire() from proper state
 * State scoped function. Override in proper state
 * Network: Server
 *
 * @param	FireModeNum		Fire Mode
 */
function FireAmmunition( byte FireModeNum )
{
	TraceFire();
}

/**
 * Fires a projectile 
 * State scoped function. Override in proper state
 * Network: Server
 */
function ProjectileFire()
{
	local vector		RealStartLoc, HitLocation, HitNormal, AimDir;
	local TraceHitInfo	HitInfo;
	local Projectile	SpawnedProjectile;

	// Trace where crosshair is aiming at. Get hit info.
	CalcWeaponFire( HitLocation, HitNormal, AimDir, HitInfo );

	// this is the location where the projectile is spawned
	RealStartLoc	= GetPhysicalFireStartLoc();

	SpawnedProjectile = Spawn(GetProjectileClass(),,, RealStartLoc);
	if ( SpawnedProjectile != None )
		SpawnedProjectile.Init(Normal(HitLocation - RealStartLoc));
}

/**
 * Performs a trace to simulate a shot.
 *
 * @ouput	out_HitLocation, Hit Location of the shot
 * @output	out_HitNormal, hit normal of the shot
 * @output	out_Aim, Aim direction of the shot. Returned for postprocessing as Aim direction can be randomized (spread/inaccuracy).
 * @param	HitInto, TraceHitInfo struct returning useful info like component hit, bone, material..
 * @return	HitActor, None if didn't hit anything.
 * 
 * @note	if the trace didn't hit anything, out_HitLocation returns the EndTrace location.
 */
simulated function Actor CalcWeaponFire( out vector out_HitLocation, out vector out_HitNormal, out vector out_Aim, out TraceHitInfo out_HitInfo )
{
	local vector		StartTrace, EndTrace;
	local actor			HitActor;

	StartTrace	= Instigator.GetWeaponStartTraceLocation();							
	out_Aim		= Vector(GetAdjustedAim( StartTrace ));			// get fire aim direction
    EndTrace	= StartTrace + out_Aim * GetTraceRange();		// define end of trace
	HitActor	= Trace(out_HitLocation, out_HitNormal, EndTrace, StartTrace, true, vect(0,0,0), out_HitInfo);

	// If we didn't hit anything, then set the HitLocation as being the EndTrace location
	if ( HitActor == None )
	{
		out_HitLocation = EndTrace;
	}

	return HitActor;
}

/**
 * Performs an 'Instant Hit' trace and processes a hit 
 * State scoped function. Override in proper state
 * Network: Server
 *
 * @param	damage given assigned to HitActor
 * @param	DamageType class
 */
function TraceFire()
{
	local Vector		HitLocation, HitNormal, AimDir;
	local Actor			HitActor;
	local TraceHitInfo	HitInfo;

	HitActor = CalcWeaponFire( HitLocation, HitNormal, AimDir, HitInfo );

    if ( HitActor != None )
    {
		ProcessInstantHit( HitActor, AimDir, HitLocation, HitNormal, HitInfo );
	}
}

simulated function bool CanThrow()
{
	return false;
}

/**
 * Processes an 'Instant Hit' trace and spawns any effects 
 * State scoped function. Override in proper state
 * Network: Server
 *
 * @param	HitActor = Actor hit by trace
 * @param	HitLocation = world location vector where HitActor was hit by trace
 * @param	HitNormal = hit normal vector
 * @param	HitInto	= TraceHitInfo struct returning useful info like component hit, bone, material..
 */
function ProcessInstantHit( Actor HitActor, vector AimDir, Vector HitLocation, Vector HitNormal, TraceHitInfo HitInfo )
{
	HitActor.TakeDamage(15, Instigator, HitLocation, AimDir, class'DamageType', HitInfo);
}

//
// Weapon properties
//

/**
 * Range of weapon
 * Used for Traces (CalcWeaponFire, TraceFire, ProjectileFire, AdjustAim...)
 * State scoped accessor function. Override in proper state
 *
 * @return	range of weapon, to be used mainly for traces.
 */
simulated function float GetTraceRange()
{
	return WeaponRange;
}

/**
 * is weapon fire mode F instant hit?
 * State scoped accessor function. Override in proper state
 */
simulated function bool IsInstantHit()
{
	return true;
}

/**
 * get projectile class to spawn
 * State scoped accessor function. Override in proper state
 */
function class<Projectile> GetProjectileClass()
{
	return class'Projectile';
}

/**
 * Event to synchronize weapon states. Defines the timing of the weapon.
 * Can be for example called from a Timer or an Animation.AnimEnd event.
 * State scoped function. Override in proper state
 */
simulated function SynchEvent();

// by default timer is used to time weapon states, and calls SynchEvent
simulated function Timer()
{
	SynchEvent();
}

/**
 * Used to query is a weapon is IDLE, or busy (firing, reloading, being switched)
 *
 * @return	true, if weapon is IDLE
 */
simulated function bool IsIdle()
{
	return false;
}

//
// Weapon States
//

/** 
 * State Inactive
 * Default state for a weapon. It is not active, cannot fire and resides in player inventory.
 */
auto state Inactive
{
	/** @see Weapon::ServerStartFire */
	function ServerStartFire( byte FireModeNum );

	/** @see Weapon::StartFire */
	simulated function StartFire( byte FireModeNum );

	/** @see Weapon::Active */
	simulated function Activate()
	{
		GotoState('Equiping');
	}

	/** @see Weapon::TryPutDown */
	simulated function bool TryPutDown()
	{
		if ( bWeaponPutDown || InvManager.PendingWeapon != None )
		{
			InvManager.ChangedWeapon();
			return true;
		}
		return false;
	}
}

/** 
 * State Active 
 * When a weapon is in the active state, it's up, ready but not doing anything. (idle)
 */
state Active
{
	/**
	* Used to query the firing state of this weapon, return true if
	* within the Firing state.
	*
	* @return			true if weapon is in firing state
	*/
	simulated function bool IsIdle()
	{
		return true;
	}

	simulated function SynchEvent()
	{
		PlayWeaponIdle();
	}

	/** @see Weapon::TryPutDown() */
	simulated function bool TryPutDown()
	{
		GotoState('PuttingDown');
		return true;
	}

	simulated function BeginState()
	{
		PlayWeaponIdle();
	}

	function bool CanThrow()
	{
		return ( bCanThrow && HasAnyAmmo() );
	}

Begin:
}

/** 
 * State Firing
 * Basic default firing state. Handles continuous, auto firing behavior.
 */
state Firing
{
	/** @see Weapon::StartFire */
	simulated function StartFire( byte FireModeNum )
	{
		if ( Role < Role_Authority )
		{
			ServerStartFire( FireModeNum );
		}
	}

	/** @see Weapon::ServerStartFire */
	function ServerStartFire( byte FireModeNum )
	{
		bForceFire		= true;
		ForceFireMode	= FireModeNum;
	}

	/** @see Weapon::SynchEvent */
	simulated function SynchEvent()
	{
		if ( bWeaponPutDown )
		{
			// if switched to another weapon, put down right away
			GotoState('PuttingDown');
			return;
		}

		// If weapon should keep on firing, then do not leave state and fire again.
		if ( ShouldRefire( CurrentFireMode ) )
		{
			LocalFire( CurrentFireMode );
			return;
		}

		// Otherwise we're done firing, so go back to active state.
		GotoState('Active');
	}

	/** Called when weapon enters firing state */
	simulated function BeginState()
	{
		bForceFire = true;
		LocalFire( CurrentFireMode );
	}

	/** Called when weapon leaves firing state */
	simulated function EndState()
	{
		bForceFire = false;
	}

Begin:
}

/**
 * Check if current fire mode can/should keep on firing.
 * This is called from a firing state after each shot is fired 
 * to decide if the weapon should fire again, or stop and go to the active state.
 * The default behavior, implemented here, is keep on firing while player presses fire 
 * and there is enough ammo. (Auto Fire).
 *
 * @param	FireModeNum		Fire mode number.
 *
 * @return	true to fire again, false to stop firing and return to Active State.
 */
simulated function bool ShouldRefire( byte FireModeNum )
{
	if ( !HasAmmo( FireModeNum ) )
	{
		// if doesn't have ammo to keep on firing, then stop
		return false;
	}

	if ( Instigator.IsLocallyControlled() )
	{
		// if locally controlled and still pressing fire, then continue
		return ( Instigator.IsPressingFire( FireModeNum ) );
	}

	// On server, return bForceFire
	return bForceFire;
}

/**
 * The Weapon is in this state while transitioning from Inactive to Active state. 
 * Typically, the weapon will remain in this state while its selection animation is being played.
 * While in this state, the weapon cannot be fired.
 */
state Equiping
{
	/** @see Weapon::StartFire */
	simulated function StartFire( byte FireModeNum );

	/** @see Weapon::SynchEvent */
	simulated function SynchEvent()
	{
		if ( bWeaponPutDown )
		{
			// if switched to another weapon, put down right away
			GotoState('PuttingDown');
			return;
		}

		// if pressing fire (local player), fire
		if ( Instigator.IsLocallyControlled() && Instigator.IsPressingFire( CurrentFireMode ) && HasAmmo(CurrentFireMode) )		
		{
			Global.StartFire( CurrentFireMode );
			return;
		}

		GotoState('Active');
	}

	/** Event called when weapon enters this state */
	simulated function BeginState()
	{
		TimeWeaponEquiping();
		bWeaponPutDown	= false;
	}


}

/**
 * Putting down weapon in favor of a new one.  No firing in this state.
 * Weapon is transitioning to the Inactive state.
 * This state is only executed on the LocalPlayer, not on a dedicated server.
 * On the dedicated server, the weapon stops firing and waits to be sent to the 'Inactive' state by InvManager::ServerChangedWeapon()
 */
State PuttingDown
{
	/** @see Weapon::StartFire */
	simulated function StartFire( byte FireModeNum );

	/** @see Weapon::TryPutDown */
	simulated function bool TryPutDown()
	{
		return true; //just keep putting it down
	}

	/** @see Weapon::SynchEvent */
	simulated function SynchEvent()
	{
		// Switch to pending weapon
		if ( Instigator.IsLocallyControlled() )
		{
			InvManager.ChangedWeapon();
		}

		// Put weapon to sleep
		GotoState('Inactive');
	}

	/** Event called when weapon enters this state */
	simulated function BeginState()
	{
		TimeWeaponPutDown();
		bWeaponPutDown = false;	

		// Tell weapon to stop firing server side
		// Note: this doesn't cancel firing flags on Controller, so next weapon can start firing right away
		if ( Role < Role_Authority && Instigator.IsLocallyControlled() && Instigator.IsPressingFire(CurrentFireMode) )
		{
			StopFire( CurrentFireMode );
		}
	}
}

/**
 * Weapon on network client side may be set here by the replicated function
 * ClientWeaponSet(), to wait, if needed properties have not yet been replicated.
 */
State PendingClientWeaponSet
{
	/** @see Weapon::Activate() */
	simulated function Activate()
	{
		GotoState('Equiping');
	}

	simulated function Timer()
	{
		// When variables are replicated, ClientWeaponSet, will send weapon to another state.
		// Therefore aborting this timer.
		ClientWeaponSet( true );
	}

	/** Event called when weapon enters this state */
	simulated function BeginState()
	{
		// Set a timer to keep checking for replicated variables.
		SetTimer(0.05, true);
	}

	/** Event called when weapon leaves this state */
	simulated function EndState()
	{
		SetTimer(0.0, false);
	}
}

defaultproperties
{
	FiringStatesArray(0)=Firing
	WeaponRange=16384
	bCanThrow=true
	ItemName="Weapon"
	bReplicateInstigator=true
	bOnlyRelevantToOwner=false
	bOnlyDirtyReplication=false
	RemoteRole=ROLE_SimulatedProxy
	WarnTargetPct=+0.9
}