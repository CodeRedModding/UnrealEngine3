/**
 * Base Weapon implementation.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class Weapon extends Inventory
	native
	abstract
	config(game)
	notplaceable;

/************************************************************************************
 * Firing Mode Definition
 ***********************************************************************************/

/**
 * This enum defines the firing type for the weapon.
 *	EWFT_InstantHit  - The weapon traces a shot to determine a hit and immediately causes an effect
 *	EWFT_Projectile  - The weapon spawns a new projectile pointed at the crosshair
 *	EWFT_Custom      - Requires a custom fire sequence
 */

enum EWeaponFireType
{
	EWFT_InstantHit,
	EWFT_Projectile,
	EWFT_Custom,
	EWFT_None
};

/** Current FireMode*/
var	byte CurrentFireMode;

/** Array of firing states defining available firemodes */
var				Array<Name>					FiringStatesArray;

/** Defines the type of fire (see Enum above) for each mode */
var				Array<EWeaponFireType>		WeaponFireTypes;

/** The Class of Projectile to spawn */
var				Array< class<Projectile> >	WeaponProjectiles;

/** Holds the amount of time a single shot takes */
var()			Array<float>				FireInterval;

/** How much of a spread between shots */
var()			Array<float>				Spread;

/** How much damage does a given instanthit shot do */
var()			Array<float>				InstantHitDamage;

/** momentum transfer scaling for instant hit damage */
var()			Array<float>				InstantHitMomentum;

/** DamageTypes for Instant Hit Weapons */
var				Array< class<DamageType> >	InstantHitDamageTypes;


/************************************************************************************
 * Firing / Timing / States
 ***********************************************************************************/

/** How long does it take to Equip this weapon */
var()			float	EquipTime;

/** How long does it take to put this weapon down */
var()			float	PutDownTime;

/** Holds an offest for spawning protectile effects. */
var()			vector	FireOffset;

/** Set to put weapon down at the end of a state. Typically used to change weapons on state changes (weapon up, stopped firing...) */
var				bool	bWeaponPutDown;

/** Range of Weapon, used for Traces (InstantFire, ProjectileFire, AdjustAim...) */
var()			float	WeaponRange;


/*********************************************************************************************
 * Mesh / Skins / Effects
 ********************************************************************************************* */

/** Weapon Mesh */
var() editinline MeshComponent Mesh;

/** When no duration is specified, speed to play anims. */
var() float	DefaultAnimSpeed;

/*********************************************************************************************
 * Inventory Grouping/etc.
 ********************************************************************************************* */

/** Configurable weapon priority.  Ties (mod weapons) are broken by GroupWeight */
var config	float	Priority;

/** Can player toss his weapon out? Typically false for default inventory. */
var			bool	bCanThrow;

/** Set from ClientWeaponSet() when it has to go through PendingClientWeaponSet, to preserve those variables. */
var bool bWasOptionalSet;
/** Set from ClientWeaponSet() when it has to go through PendingClientWeaponSet, to preserve those variables. */
var bool bWasDoNotActivate;

/*********************************************************************************************
 * AI Hints
 ********************************************************************************************* */
/** Current AI controlling this weapon */
var protectedwrite AIController AIController;

var array<byte> ShouldFireOnRelease;
var bool bInstantHit;
var bool bMeleeWeapon;
var float AIRating;

/** Cache MaxRange of weapon */
var float CachedMaxRange;

/*********************************************************************************************
 * Initialization / System Messages / Utility
 *********************************************************************************************/

/** Event called when weapon actor is destroyed */
simulated event Destroyed()
{
	// detach weapon from instigator
	DetachWeapon();
	super.Destroyed();
}


/**
 * A notification call when this weapon is removed from the Inventory of a pawn
 * @see Inventory::ItemRemovedFromInvManager
 */
function ItemRemovedFromInvManager()
{
	`LogInv("");

	GotoState('Inactive');

	// Stop Firing
	ForceEndFire();
	// detach weapon from instigator
	DetachWeapon();
	// Tell the client the weapon has been thrown
	ClientWeaponThrown();

	Super.ItemRemovedFromInvManager();

	if( Instigator != None && Instigator.Weapon == self )
	{
		Instigator.Weapon = None;
	}
}


/**
 * Pawn holding this weapon as active weapon just died.
 */
function HolderDied()
{
	ServerStopFire( CurrentFireMode );
}


/**
 * hook to override Next weapon call.
 * For example the physics gun uses it to have mouse wheel change the distance of the held object.
 * Warning: only use in firing state, otherwise it breaks weapon switching
 */
simulated function bool DoOverrideNextWeapon()
{
	return false;
}


/**
 * hook to override Previous weapon call.
 */
simulated function bool DoOverridePrevWeapon()
{
	return false;
}


/**
 * Drop this weapon out in to the world
 *
 * @param	StartLocation 		- The World Location to drop this item from
 * @param	StartVelocity		- The initial velocity for the item when dropped
 */
function DropFrom(vector StartLocation, vector StartVelocity)
{
	if( !CanThrow() )
	{
		return;
	}

	// Become inactive
	GotoState('Inactive');

	// Stop Firing
	ForceEndFire();
	// Detach weapon components from instigator
	DetachWeapon();

	// tell the super to DropFrom() which will
	// should remove the item from our inventory
	Super.DropFrom(StartLocation, StartVelocity);

	AIController = None;
}

/**
 * Returns true if this item can be thrown out.
 */
simulated function bool CanThrow()
{
	return bCanThrow;
}

/**
 * This function is called when the client needs to discard the weapon
 */
reliable client function ClientWeaponThrown()
{
	`LogInv("");

	if (WorldInfo.NetMode == NM_Client)
	{
		// Become inactive
		GotoState('Inactive');

		// if this is the weapon we were carrying, set reference to None.
		if( Instigator != None && Instigator.Weapon == Self )
		{
			Instigator.Weapon = None;
		}

		// Stop Firing
		ForceEndFire();
		// Detach weapon components from instigator
		DetachWeapon();
	}
}


/**
 * Returns true if the weapon is firing, used by AI
 */
simulated event bool IsFiring()
{
    return FALSE;
}


/**
 * Returns true if this weapon wants to deny a ClientWeaponSwitch call
 */
simulated function bool DenyClientWeaponSet()
{
	return FALSE;
}


/*********************************************************************************************
 * Debug / Log
 *********************************************************************************************/

/**
 * list important Weapon variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD			- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */

simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local Array<String>	DebugInfo;
	local int			i;

	GetWeaponDebug( DebugInfo );

	Hud.Canvas.SetDrawColor(0,255,0);
	for (i=0;i<DebugInfo.Length;i++)
	{
		Hud.Canvas.DrawText( "  " @ DebugInfo[i] );
		out_YPos += out_YL;
		Hud.Canvas.SetPos(4, out_YPos);
	}
}


/**
 * Retrieves important weapon debug information as an array of strings. That can then be dumped or displayed on HUD.
 */

simulated function GetWeaponDebug( out Array<String> DebugInfo )
{
	local String	    T;
	local int		    i;

	DebugInfo[DebugInfo.Length] = "Weapon:" $ GetItemName(string(Self)) @ "State:" $ GetStateName() @ "Instigator:" $ Instigator @ "Owner:" $ Owner;
	DebugInfo[DebugInfo.Length] = "IsFiring():" $ IsFiring() @ "CurrentFireMode:" $ CurrentFireMode @ "bWeaponPutDown:" $ bWeaponPutDown;
	if (Instigator != None)
	{
		DebugInfo[DebugInfo.Length] = "ShotCount:" $ Instigator.ShotCount @ "FlashCount:" $ Instigator.FlashCount @ "FlashLocation:" $ Instigator.FlashLocation;
	}

	T = "PendingFires:";
	for(i=0; i<GetPendingFireLength(); i++)
	{
		T = T $ PendingFire(i) $ " ";
	}

	DebugInfo[DebugInfo.Length] = T;

    if( Timers.Length > 0 )
    {
    	for (i=0;i<Timers.Length;i++)
    	{
			DebugInfo[DebugInfo.Length] = "Timer" @ Timers[i].FuncName @ Timers[i].Count @ Timers[i].Rate @ int(Timers[i].Count/Timers[i].Rate*100)$"%";
		}
    }
}

/*********************************************************************************************
 * Ammunition / Inventory
 *********************************************************************************************/


/**
 * Consumes ammunition when firing a shot.
 * Subclass me to define weapon ammunition consumption.
 */
function ConsumeAmmo( byte FireModeNum );


/**
 * Add ammo to weapon
 * @param	Amount to add.
 * @return	Amount actually added. (In case magazine is already full and some ammo is left
 *
 * Subclass me to define ammo addition rules.
 */
function int AddAmmo(int Amount);


/**
 * This function checks to see if the weapon has any ammo available for a given fire mode.
 *
 * @param	FireModeNum		- The Fire Mode to Test For
 * @param	Amount			- [Optional] Check to see if this amount is available.
 * @return	true if ammo is available for Firemode FireModeNum.
 */
simulated function bool HasAmmo( byte FireModeNum, optional int Amount )
{
	return true;
}


/**
 * returns true if this weapon has any ammo left, regardless of the actual firing mode.
 */
simulated function bool HasAnyAmmo()
{
	return true;
}

/*********************************************************************************************
 * Pending Fire / Inv Manager
 *********************************************************************************************/

final simulated function INT GetPendingFireLength()
{
	if( InvManager != none )
	{
		return InvManager.GetPendingFireLength(Self);
	}

	return 0;
}

final simulated function bool PendingFire(int FireMode)
{
	if( InvManager != none )
	{
		return InvManager.IsPendingFire(Self, FireMode);
	}
	return false;
}

final simulated function SetPendingFire(int FireMode)
{
	if( InvManager != None )
	{
		InvManager.SetPendingFire(Self, FireMode);
	}
}

final simulated function ClearPendingFire(int FireMode)
{
	if( InvManager != None )
	{
		InvManager.ClearPendingFire(Self, FireMode);
	}
}

/**
 * Returns the type of projectile to spawn.  We use a function so subclasses can
 * override it if needed (case in point, homing rockets).
 */
function class<Projectile> GetProjectileClass()
{
	return (CurrentFireMode < WeaponProjectiles.length) ? WeaponProjectiles[CurrentFireMode] : None;
}


/**
 * Adds any fire spread offset to the passed in rotator
 * @param Aim the base aim direction
 * @return the adjusted aim direction
 */
simulated function rotator AddSpread(rotator BaseAim)
{
	local vector X, Y, Z;
	local float CurrentSpread, RandY, RandZ;

	CurrentSpread = Spread[CurrentFireMode];
	if (CurrentSpread == 0)
	{
		return BaseAim;
	}
	else
	{
		// Add in any spread.
		GetAxes(BaseAim, X, Y, Z);
		RandY = FRand() - 0.5;
		RandZ = Sqrt(0.5 - Square(RandY)) * (FRand() - 0.5);
		return rotator(X + RandY * CurrentSpread * Y + RandZ * CurrentSpread * Z);
	}
}

/**
 * Returns the Maximum Range for this weapon
 */
simulated function float MaxRange()
{
	local int i;

	if ( CachedMaxRange > 0 )
	{
		return CachedMaxRange;
	}

	// return the range of the fire mode that fires farthest
	if (bInstantHit)
	{
		CachedMaxRange = WeaponRange;
	}

	for (i = 0; i < WeaponProjectiles.length; i++)
	{
		if (WeaponProjectiles[i] != None)
		{
			CachedMaxRange = FMax(CachedMaxRange, WeaponProjectiles[i].static.GetRange());
		}
	}
	return CachedMaxRange;
}

/*********************************************************************************************
 * AI interface
 *********************************************************************************************/

function float GetAIRating()
{
	return AIRating;
}

/**
 * Returns a weight reflecting the desire to use the
 * given weapon, used for AI and player best weapon
 * selection.
 *
 * @return	weapon rating (range -1.f to 1.f)
 */
simulated function float GetWeaponRating()
{
	if( InvManager != None )
	{
		return InvManager.GetWeaponRatingFor( Self );
	}

	if( !HasAnyAmmo() )
	{
		return -1;
	}

	return 1;
}

// CanAttack() - return false if out of range, can't see target, etc.
function bool CanAttack(Actor Other)
{
	return true;
}

// tells AI that it needs to release the fire button for this weapon to do anything
function bool FireOnRelease()
{
	return (ShouldFireOnRelease.Length>0 && ShouldFireOnRelease[CurrentFireMode]!=0);
}


/*********************************************************************************************
 * Effects / Mesh / Animations / Sounds
 *********************************************************************************************/

/** Returns the AnimNodeSequence the weapon is using to play animations. */
simulated function AnimNodeSequence GetWeaponAnimNodeSeq()
{
	local AnimTree Tree;
	local AnimNodeSequence AnimSeq;
	local SkeletalMeshComponent SkelMesh;

	SkelMesh = SkeletalMeshComponent(Mesh);
	if(SkelMesh != None)
	{
		//Try getting an animtree first
		Tree = AnimTree(SkelMesh.Animations);
		if (Tree != None)
		{
			AnimSeq = AnimNodeSequence(Tree.Children[0].Anim);
		}
		else
		{
			//Old legacy way without an animtree
			AnimSeq = AnimNodeSequence(SkelMesh.Animations);
		}

		return AnimSeq;
	}

	return None;
}

/**
 * Play an animation on the weapon mesh
 * Network: Local Player and clients
 *
 * @param	Anim Sequence to play on weapon skeletal mesh
 * @param	desired duration, in seconds, animation should be played
 */
event PlayWeaponAnimation( Name Sequence, float fDesiredDuration, optional bool bLoop, optional SkeletalMeshComponent SkelMesh)
{
	local AnimNodeSequence WeapNode;
	local AnimTree Tree;

	// do not play on a dedicated server
	if( WorldInfo.NetMode == NM_DedicatedServer )
	{
		return;
	}

	if ( SkelMesh == None )
	{
		SkelMesh = SkeletalMeshComponent(Mesh);
	}

	// Check we have access to mesh and animations
	if( SkelMesh == None || GetWeaponAnimNodeSeq() == None )
	{
		return;
	}

	if(fDesiredDuration > 0.0)
	{
		// @todo - this should call GetWeaponAnimNodeSeq, move 'duration' code into AnimNodeSequence and use that.
		SkelMesh.PlayAnim(Sequence, fDesiredDuration, bLoop);
	}
	else
	{
		//Try getting an animtree first
		Tree = AnimTree(SkelMesh.Animations);
		if (Tree != None)
		{
			WeapNode = AnimNodeSequence(Tree.Children[0].Anim);
		}
		else
		{
			WeapNode = AnimNodeSequence(SkelMesh.Animations);
		}

		WeapNode.SetAnim(Sequence);
		WeapNode.PlayAnim(bLoop, DefaultAnimSpeed);
	}
}

/**
 * Stops an animation on the weapon mesh
 * Network: Local Player and clients
 *
 */
event StopWeaponAnimation()
{
	local AnimNodeSequence AnimSeq;

	// do not play on a dedicated server
	if( WorldInfo.NetMode == NM_DedicatedServer )
	{
		return;
	}

	AnimSeq = GetWeaponAnimNodeSeq();
	if( AnimSeq != None )
	{
		AnimSeq.StopAnim();
	}
}

/**
 * PlayFireEffects
 * Main function to play Weapon fire effects.
 * This is called from Pawn::WeaponFired in the base implementation.
 */
simulated function PlayFireEffects( byte FireModeNum, optional vector HitLocation );

/**
 * StopFireEffects
 * Main function to stop any active effects
 * This is called from Pawn::WeaponStoppedFiring
 */
simulated function StopFireEffects(byte FireModeNum);


/*********************************************************************************************
 * Timing
 *********************************************************************************************/


/**
 * Returns interval in seconds between each shot, for the firing state of FireModeNum firing mode.
 *
 * @param	FireModeNum	fire mode
 * @return	Period in seconds of firing mode
 */
simulated function float GetFireInterval( byte FireModeNum )
{
	return FireInterval[FireModeNum] > 0 ? FireInterval[FireModeNum] : 0.01;
}


/**
 * Sets the timing for the firing state on server and local client.
 * By default, a constant looping Rate Of Fire (ROF) is set up.
 * When the delay has expired, the RefireCheckTimer event is triggered.
 *
 * Network: LocalPlayer and Server
 *
 * @param	FireModeNum		Fire Mode.
 */
simulated function TimeWeaponFiring( byte FireModeNum )
{
	// if weapon is not firing, then start timer. Firing state is responsible to stopping the timer.
	if( !IsTimerActive('RefireCheckTimer') )
	{
		SetTimer( GetFireInterval(FireModeNum), true, nameof(RefireCheckTimer) );
	}
}

simulated function RefireCheckTimer();

/**
 * Sets the timing for putting a weapon down.  The WeaponIsDown event is trigged when expired
*/
simulated function TimeWeaponPutDown()
{
	SetTimer( PutDownTime>0 ? PutDownTime : 0.01, false, nameof(WeaponIsDown) );
}


/**
 * Sets the timing for equipping a weapon.
 * The WeaponEquipped event is trigged when expired
 */
simulated function TimeWeaponEquipping()
{
	SetTimer( EquipTime>0 ? EquipTime : 0.01 , false, 'WeaponEquipped');
}


/**
 * All inventory use the Activate() function when an item is selected for use.
 * For weapons, this function starts the Equipping process. If the weapon is the inactive state,
 * it will go to the 'WeaponEquipping' followed by 'Active' state, and ready to be fired.
 */
simulated function Activate()
{
	// don't reactivate if already firing
	if (!IsFiring())
	{
		GotoState('WeaponEquipping');
	}
}


/**
 * This function is called to put a weapon down
 */
simulated function PutDownWeapon()
{
	GotoState('WeaponPuttingDown');
}


/**
 * When you pickup an weapon, the inventory system has a chance to restrict the pickup.
 */
function bool DenyPickupQuery(class<Inventory> ItemClass, Actor Pickup)
{
	// By default, you can only carry a single item of a given class.
	if( ItemClass == class )
	{
		return true;
	}

	return false;
}


/**
 * Called when the weapon runs out of ammo during firing
 */
simulated function WeaponEmpty();


/*********************************************************************************************
 * Firtst/Third person weapon attachment functions
 *********************************************************************************************/

/**
 * Increment Pawn's FlashCount variable.
 * This is used to play weapon fire effects on remote clients.
 * Call this on the server and local player.
 *
 * Network: Server and Local Player
 */
simulated function IncrementFlashCount()
{
	if( Instigator != None )
	{
		Instigator.IncrementFlashCount( Self, CurrentFireMode );
	}
}


/**
 * Clear flashCount variable on Pawn. and call WeaponStoppedFiring event.
 * Call this on the server and local player.
 *
 * Network: Server or Local Player
 */
simulated function ClearFlashCount()
{
	if( Instigator != None )
	{
		Instigator.ClearFlashCount( Self );
	}
}


/**
 * This function sets up the Location of a hit to be replicated to all remote clients.
 *
 * Network: Server only
 */
function SetFlashLocation( vector HitLocation )
{
	if( Instigator != None )
	{
		Instigator.SetFlashLocation( Self, CurrentFireMode, HitLocation );
	}
}


/**
 * Reset flash location variable. and call stop firing.
 * Network: Server only
 */
function ClearFlashLocation()
{
	if( Instigator != None )
	{
		Instigator.ClearFlashLocation( Self );
	}
}


/**
 * AttachWeaponTo is called when it's time to attach the weapon's mesh to a location.
 * it should be subclassed.
 */
simulated function AttachWeaponTo( SkeletalMeshComponent MeshCpnt, optional Name SocketName );


/**
 * Detach weapon components from instigator. Perform any clean up.
 * Should be subclassed.
 */
simulated function DetachWeapon();


/*********************************************************************************************
 * Pawn/Controller/View functions
 *********************************************************************************************/

reliable client function ClientGivenTo(Pawn NewOwner, bool bDoNotActivate)
{
	Super.ClientGivenTo(NewOwner, bDoNotActivate);

	// Evaluate if we should switch to this weapon
	ClientWeaponSet(TRUE, bDoNotActivate);
}

/**
 * is called by the server to tell the client about potential weapon changes after the player runs over
 * a weapon (the client decides whether to actually switch weapons or not.
 * Network: LocalPlayer
 *
 * @param	bOptionalSet.	Set to true if the switch is optional. (simple weapon pickup and weight against current weapon).
 * @param	bDoNotActivate.	Override, do not activate this weapon. It's just been received in the inventory.
 */
reliable client function ClientWeaponSet(bool bOptionalSet, optional bool bDoNotActivate)
{
	`LogInv("bOptionalSet:" @ bOptionalSet @ "bDoNotActivate:" @ bDoNotActivate @ "Instigator:" @ Instigator @ "InvManager:" @ InvManager);

	// Save variables in case we need to go to PendingClientWeaponSet to wait for replication
	bWasOptionalSet = bOptionalSet;
	bWasDoNotActivate = bDoNotActivate;

	// If weapon's instigator isn't replicated to client, wait for it in PendingClientWeaponSet state
	if( Instigator == None )
	{
		`LogInv("Instigator == None, going to PendingClientWeaponSet");
		GotoState('PendingClientWeaponSet');
		return;
	}

	// If InvManager isn't replicated to client, wait for it in PendingClientWeaponSet state
	if( InvManager == None )
	{
		`LogInv("InvManager == None, going to PendingClientWeaponSet");
		GotoState('PendingClientWeaponSet');
		return;
	}

	InvManager.ClientWeaponSet(Self, bOptionalSet, bDoNotActivate);
}


/*********************************************************************************************
 * Handling the actual Fire Commands
 *********************************************************************************************/

/* Weapon Firing Logic overiew:

	The weapon system here is designed to be a single code path that follows the same flow on both
	the Authoritive server and the local client.  Remote clients know nothing about the weapon and utilize
	the WeaponAttachment system to see the end results.


	1: The InventoryManager (IM) on the Local Client recieves a StartFire call.  It calls StartFire().

	2: If Local Client is not Authoritive it notifies the server via ServerStartFire().

	3: Both StartFire() and ServerStartFire() sync up by calling BeginFire().

	4: BeginFire sets the PendingFire flag for the incoming fire Mode

	5: BeginFire looks at the current state and if it's in the Active state, it begins the
	   firing sequence by transitioning to the new fire state as defined by the FiringStatesArray
	   array.  This is done by called SendToFiringState.

	6: The Firing Logic is handled in the various firing states.  Firing states are responsible for the
	   following:
	   				a: Continuing to fire if their associated PendingFire is hot
	   				b: Transitioning to a new weapon when out of ammo
	   				c: Transitioning to the "Active" state when no longer firing


    The weapon system also receives a StopFire() event from the IM.  When this occurs, the following
    logic is performed:

    1: The IM on the Local Client calls StopFire().

    2: If Weapon Stop fire is not on the Authoritive process, it notifes the server via the
	   ServerStopFire() event.

	3: Both StopFire() and ServerStopFire() sync up by calling EndFire().

	4: EndFire() clears the PendingFire flag for this outgoing fire mode.


	Firing states should be identical in their execution, branching outwards as need.  For example,
	in the default firing state ('WeaponFiring') the function FireAmmunition() occurs in all applicable processes.
*/


/**
 * Called on the LocalPlayer, Fire sends the shoot request to the server (ServerStartFire)
 * and them simulates the firing effects locally.
 * Call path: PlayerController::StartFire -> Pawn::StartFire -> InventoryManager::StartFire
 * Network: LocalPlayer
 */
simulated function StartFire(byte FireModeNum)
{
	if( Instigator == None || !Instigator.bNoWeaponFiring )
	{
		if( Role < Role_Authority )
		{
			// if we're a client, synchronize server
			ServerStartFire(FireModeNum);
		}

		// Start fire locally
		BeginFire(FireModeNum);
	}
}


/**
 * When StartFire() is called on a client, it replicates the start by calling ServerStartFire.  This
 * begins the event on server.  Server side actors (such as bots) should not call ServerStartFire directly and should
 * instead call StartFire().
 *
 * Network: Dedicated Server only, or Listen Server for remote clients.
 */
reliable server function ServerStartFire(byte FireModeNum)
{
	if( Instigator == None || !Instigator.bNoWeaponFiring )
	{
		// A client has fired, so the server needs to
		// begin to fire as well
		BeginFire(FireModeNum);
	}
}


/**
 * BeginFire is the point at which the server and client sync up their code path.  It's job is to set
 * the weapon in to the firing state.
 * Network: LocalPlayer and Server
 */
simulated function BeginFire(Byte FireModeNum)
{
	// Flag this mode as pending a fire.  The only thing that can remove
	// this flag is a Stop Fire/Putdown command.
	`LogInv("FireModeNum:" @ FireModeNum);
	SetPendingFire(FireModeNum);
}


/**
 * This initiates the shutdown of a weapon that is firing.
 * Network: Local Player
 */

simulated function StopFire(byte FireModeNum)
{
	// Locally shut down the fire sequence
	EndFire(FireModeNum);

	// Notify the server
	if( Role < Role_Authority )
	{
		ServerStopFire(FireModeNum);
	}
}


/**
 * When StopFire is called on a client, ServerStopFire is used to initiate the sequence on the server.
 * Network: Dedicated Server only, or Listen Server for remote clients.
 */
reliable server function ServerStopFire(byte FireModeNum)
{
	EndFire(FireModeNum);
}


/**
 * Like BeginFire, this function puts a client and the server in sync and shuts down the
 * firing sequence on both.
 * Network: LocalPlayer and Server
 */
simulated function EndFire(byte FireModeNum)
{
	// Clear the firing flag for this mode
	ClearPendingFire(FireModeNum);
}


/**
 * Clear all pending fires.
 * This is non replicated flag.
 */
simulated function ForceEndFire()
{
	local int i, Num;

	// Clear all pending fires
	if (InvManager != None)
	{
		Num = GetPendingFireLength();
		for (i = 0; i < Num; i++)
		{
			if (PendingFire(i))
			{
				EndFire(i);
			}
		}
	}
}


/**
 * Send weapon to proper firing state
 * Also sets the CurrentFireMode.
 * Network: LocalPlayer and Server
 *
 * @param	FireModeNum Fire Mode.
 */
simulated function SendToFiringState(byte FireModeNum)
{
	// make sure fire mode is valid
	if( FireModeNum >= FiringStatesArray.Length )
	{
		`LogInv("Invalid FireModeNum");
		return;
	}

	// Needs a state name, and ignores a none fire type
	if( FiringStatesArray[FireModeNum] == '' ||
		WeaponFireTypes[FireModeNum] == EWFT_None )
	{
		return;
	}

	// set current fire mode
	SetCurrentFireMode(FireModeNum);

	`LogInv(FireModeNum @ "Sending to state:" @ FiringStatesArray[FireModeNum]);
	// transition to firing mode state
	GotoState(FiringStatesArray[FireModeNum]);
}


/**
 * Set current firing mode.
 * Network: Local Player and Server.
 */
simulated function SetCurrentFireMode(byte FiringModeNum)
{
	// set weapon's current fire mode
	CurrentFireMode = FiringModeNum;

	// set on instigator, to replicate it to remote clients
	if( Instigator != None )
	{
		Instigator.SetFiringMode(Self, FiringModeNum);
	}
}


/**
 * Event called when Pawn.FiringMode has been changed.
 * bViaReplication indicates if this was the result of a replication call.
 */
simulated function FireModeUpdated(byte FiringMode, bool bViaReplication);


/**
 * FireAmmunition: Perform all logic associated with firing a shot
 * - Fires ammunition (instant hit or spawn projectile)
 * - Consumes ammunition
 * - Plays any associated effects (fire sound and whatnot)
 *
 * Network: LocalPlayer and Server
 */

simulated function FireAmmunition()
{
	// Use ammunition to fire
	ConsumeAmmo( CurrentFireMode );

	// Handle the different fire types
	switch( WeaponFireTypes[CurrentFireMode] )
	{
		case EWFT_InstantHit:
			InstantFire();
			break;

		case EWFT_Projectile:
			ProjectileFire();
			break;

		case EWFT_Custom:
			CustomFire();
			break;
	}

	NotifyWeaponFired( CurrentFireMode );
}


/**
 * GetAdjustedAim begins a chain of function class that allows the weapon, the pawn and the controller to make
 * on the fly adjustments to where this weapon is pointing.
 */
simulated function Rotator GetAdjustedAim( vector StartFireLoc )
{
	local rotator R;

	// Start the chain, see Pawn.GetAdjustedAimFor()
	if( Instigator != None )
	{
		R = Instigator.GetAdjustedAimFor( Self, StartFireLoc );
	}

	return AddSpread(R);
}


/**
 * Range of weapon
 * Used for Traces (CalcWeaponFire, InstantFire, ProjectileFire, AdjustAim...)
 * State scoped accessor function. Override in proper state
 *
 * @return	range of weapon, to be used mainly for traces.
 */
simulated event float GetTraceRange()
{
	return WeaponRange;
}

/** @return the actor that 'owns' this weapon's traces (i.e. can't be hit by them) */
simulated function Actor GetTraceOwner()
{
	return (Instigator != None) ? Instigator : self;
}

/**
 * CalcWeaponFire: Simulate an instant hit shot.
 * This doesn't deal any damage nor trigger any effect. It just simulates a shot and returns
 * the hit information, to be post-processed later.
 *
 * ImpactList returns a list of ImpactInfo containing all listed impacts during the simulation.
 * CalcWeaponFire however returns one impact (return variable) being the first geometry impact
 * straight, with no direction change. If you were to do refraction, reflection, bullet penetration
 * or something like that, this would return exactly when the crosshair sees:
 * The first 'real geometry' impact, skipping invisible triggers and volumes.
 *
 * @param	StartTrace	world location to start trace from
 * @param	EndTrace	world location to end trace at
 * @param	Extent		extent of trace performed
 * @output	ImpactList	list of all impacts that occured during simulation
 * @return	first 'real geometry' impact that occured.
 *
 * @note if an impact didn't occur, and impact is still returned, with its HitLocation being the EndTrace value.
 */
simulated function ImpactInfo CalcWeaponFire(vector StartTrace, vector EndTrace, optional out array<ImpactInfo> ImpactList, optional vector Extent)
{
	local vector			HitLocation, HitNormal, Dir;
	local Actor				HitActor;
	local TraceHitInfo		HitInfo;
	local ImpactInfo		CurrentImpact;
	local PortalTeleporter	Portal;
	local float				HitDist;
	local bool				bOldBlockActors, bOldCollideActors;

	// Perform trace to retrieve hit info
	HitActor = GetTraceOwner().Trace(HitLocation, HitNormal, EndTrace, StartTrace, TRUE, Extent, HitInfo, TRACEFLAG_Bullet);

	// If we didn't hit anything, then set the HitLocation as being the EndTrace location
	if( HitActor == None )
	{
		HitLocation	= EndTrace;
	}

	// Convert Trace Information to ImpactInfo type.
	CurrentImpact.HitActor		= HitActor;
	CurrentImpact.HitLocation	= HitLocation;
	CurrentImpact.HitNormal		= HitNormal;
	CurrentImpact.RayDir		= Normal(EndTrace-StartTrace);
	CurrentImpact.StartTrace	= StartTrace;
	CurrentImpact.HitInfo		= HitInfo;

	// Add this hit to the ImpactList
	ImpactList[ImpactList.Length] = CurrentImpact;

	// check to see if we've hit a trigger.
	// In this case, we want to add this actor to the list so we can give it damage, and then continue tracing through.
	if( HitActor != None )
	{
		if (PassThroughDamage(HitActor))
		{
			// disable collision temporarily for the actor we can pass-through
			HitActor.bProjTarget = false;
			bOldCollideActors = HitActor.bCollideActors;
			bOldBlockActors = HitActor.bBlockActors;
			if (HitActor.IsA('Pawn'))
			{
				// For pawns, we need to disable bCollideActors as well
				HitActor.SetCollision(false, false);

				// recurse another trace
				CalcWeaponFire(HitLocation, EndTrace, ImpactList, Extent);
			}
			else
			{
				if( bOldBlockActors )
				{
					HitActor.SetCollision(bOldCollideActors, false);
				}
				// recurse another trace and override CurrentImpact
				CurrentImpact = CalcWeaponFire(HitLocation, EndTrace, ImpactList, Extent);
			}

			// and reenable collision for the trigger
			HitActor.bProjTarget = true;
			HitActor.SetCollision(bOldCollideActors, bOldBlockActors);
		}
		else
		{
			// if we hit a PortalTeleporter, recurse through
			Portal = PortalTeleporter(HitActor);
			if( Portal != None && Portal.SisterPortal != None )
			{
				Dir = EndTrace - StartTrace;
				HitDist = VSize(HitLocation - StartTrace);
				// calculate new start and end points on the other side of the portal
				StartTrace = Portal.TransformHitLocation(HitLocation);
				EndTrace = StartTrace + Portal.TransformVectorDir(Normal(Dir) * (VSize(Dir) - HitDist));
				//@note: intentionally ignoring return value so our hit of the portal is used for effects
				//@todo: need to figure out how to replicate that there should be effects on the other side as well
				CalcWeaponFire(StartTrace, EndTrace, ImpactList, Extent);
			}
		}
	}

	return CurrentImpact;
}

/**
  * returns true if should pass trace through this hitactor
  */
simulated static function bool PassThroughDamage(Actor HitActor)
{
	return (!HitActor.bBlockActors && (HitActor.IsA('Trigger') || HitActor.IsA('TriggerVolume')))
		|| HitActor.IsA('InteractiveFoliageActor');
}

/**
 * Performs an 'Instant Hit' shot.
 * Also, sets up replication for remote clients,
 * and processes all the impacts to deal proper damage and play effects.
 *
 * Network: Local Player and Server
 */

simulated function InstantFire()
{
	local vector			StartTrace, EndTrace;
	local Array<ImpactInfo>	ImpactList;
	local int				Idx;
	local ImpactInfo		RealImpact;

	// define range to use for CalcWeaponFire()
	StartTrace = Instigator.GetWeaponStartTraceLocation();
	EndTrace = StartTrace + vector(GetAdjustedAim(StartTrace)) * GetTraceRange();

	// Perform shot
	RealImpact = CalcWeaponFire(StartTrace, EndTrace, ImpactList);

	if (Role == ROLE_Authority)
	{
/*		FlushPersistentDebugLines();
		DrawDebugSphere( StartTrace, 10, 10, 0, 255, 0 );
		DrawDebugSphere( EndTrace, 10, 10, 255, 0, 0 );
		DrawDebugSphere( RealImpact.HitLocation, 10, 10, 0, 0, 255 );
		`log( self@GetFuncName()@Instigator@RealImpact.HitLocation@RealImpact.HitActor );*/

		// Set flash location to trigger client side effects.
		// if HitActor == None, then HitLocation represents the end of the trace (maxrange)
		// Remote clients perform another trace to retrieve the remaining Hit Information (HitActor, HitNormal, HitInfo...)
		// Here, The final impact is replicated. More complex bullet physics (bounce, penetration...)
		// would probably have to run a full simulation on remote clients.
		SetFlashLocation(RealImpact.HitLocation);
	}

	// Process all Instant Hits on local player and server (gives damage, spawns any effects).
	for (Idx = 0; Idx < ImpactList.Length; Idx++)
	{
		ProcessInstantHit(CurrentFireMode, ImpactList[Idx]);
	}
}


/**
 * Processes a successful 'Instant Hit' trace and eventually spawns any effects.
 * Network: LocalPlayer and Server
 * @param FiringMode: index of firing mode being used
 * @param Impact: hit information
 * @param NumHits (opt): number of hits to apply using this impact
 * 			this is useful for handling multiple nearby impacts of multihit weapons (e.g. shotguns)
 *			without having to execute the entire damage code path for each one
 *			an omitted or <= 0 value indicates a single hit
 */
simulated function ProcessInstantHit(byte FiringMode, ImpactInfo Impact, optional int NumHits)
{
	local int TotalDamage;
	local KActorFromStatic NewKActor;
	local StaticMeshComponent HitStaticMesh;

	if (Impact.HitActor != None)
	{
		// default damage model is just hits * base damage
		NumHits = Max(NumHits, 1);
		TotalDamage = InstantHitDamage[CurrentFireMode] * NumHits;

		if ( Impact.HitActor.bWorldGeometry )
		{
			HitStaticMesh = StaticMeshComponent(Impact.HitInfo.HitComponent);
			if ( (HitStaticMesh != None) && HitStaticMesh.CanBecomeDynamic() )
			{
				NewKActor = class'KActorFromStatic'.Static.MakeDynamic(HitStaticMesh);
				if ( NewKActor != None )
				{
					Impact.HitActor = NewKActor;
				}
			}
		}
		Impact.HitActor.TakeDamage( TotalDamage, Instigator.Controller,
						Impact.HitLocation, InstantHitMomentum[FiringMode] * Impact.RayDir,
						InstantHitDamageTypes[FiringMode], Impact.HitInfo, self );
	}
}


/**
 * Fires a projectile.
 * Spawns the projectile, but also increment the flash count for remote client effects.
 * Network: Local Player and Server
 */

simulated function Projectile ProjectileFire()
{
	local vector		StartTrace, EndTrace, RealStartLoc, AimDir;
	local ImpactInfo	TestImpact;
	local Projectile	SpawnedProjectile;

	// tell remote clients that we fired, to trigger effects
	IncrementFlashCount();

	if( Role == ROLE_Authority )
	{
		// This is where we would start an instant trace. (what CalcWeaponFire uses)
		StartTrace = Instigator.GetWeaponStartTraceLocation();
		AimDir = Vector(GetAdjustedAim( StartTrace ));

		// this is the location where the projectile is spawned.
		RealStartLoc = GetPhysicalFireStartLoc(AimDir);

		if( StartTrace != RealStartLoc )
		{
			// if projectile is spawned at different location of crosshair,
			// then simulate an instant trace where crosshair is aiming at, Get hit info.
			EndTrace = StartTrace + AimDir * GetTraceRange();
			TestImpact = CalcWeaponFire( StartTrace, EndTrace );

			// Then we realign projectile aim direction to match where the crosshair did hit.
			AimDir = Normal(TestImpact.HitLocation - RealStartLoc);
		}

		// Spawn projectile
		SpawnedProjectile = Spawn(GetProjectileClass(), Self,, RealStartLoc);
		if( SpawnedProjectile != None && !SpawnedProjectile.bDeleteMe )
		{
			SpawnedProjectile.Init( AimDir );
		}

		// Return it up the line
		return SpawnedProjectile;
	}

	return None;
}


/**
 * If the weapon isn't an instant hit, or a simple projectile, it should use the tyoe EWFT_Custom.  In those cases
 * this function will be called.  It should be subclassed by the custom weapon.
 */
simulated function CustomFire();


/**
 * This function returns the world location for spawning the visual effects
 */
simulated event vector GetMuzzleLoc()
{
	if( Instigator != none )
	{
		return Instigator.GetPawnViewLocation() + (FireOffset >> Instigator.GetViewRotation());
	}

	return Location;
}

/**
 * This function returns the world location for spawning the projectile, pulled in to the Pawn's collision along the AimDir direction.
 */
simulated native event vector GetPhysicalFireStartLoc(optional vector AimDir);

/**
 * Put Down current weapon
 * Once the weapon is put down, the InventoryManager will switch to InvManager.PendingWeapon.
 *
 * @return	returns true if the weapon can be put down.
 */
simulated function bool TryPutDown()
{
	bWeaponPutDown = TRUE;
	return TRUE;
}


/*********************************************************************************************
 * State Inactive
 * Default state for a weapon. It is not active, cannot fire and resides in player inventory.
 *********************************************************************************************/

auto state Inactive
{
	/**
	 * Clear out the PendingFires
	 */

	simulated event BeginState( Name PreviousStateName )
	{
//		// Make sure all pending fires are cleared
//		ForceEndFire();
	}

	reliable server function ServerStartFire(byte FireModeNum)
	{
		Global.ServerStartFire(FireModeNum);

		`Warn(WorldInfo.TimeSeconds @ Instigator @ "received ServerStartFire in Inactive State!!!");

		// We haven't received the activate yet so pass it along
		if( Instigator != None && Instigator.Weapon == Self)
		{
			`Warn( " - I'm the current weapon, so gotostate active and start firing" );
			GotoState('Active');
		}
		else if( InvManager != None && InvManager.PendingWeapon == Self )
		{
			// If our weapon is being put down, then let's just trigger the transition now.
			if( Instigator.Weapon.IsInState('WeaponPuttingDown') )
			{
				`Warn( " - I'm the pending weapon, and current weapon is being put down, so force switch now" );
				Instigator.Weapon.WeaponIsDown();
			}
			else
			{
				`Warn( " - I'm the pending weapon, but current weapon is NOT being put down, so resync client and server" );
				InvManager.SetCurrentWeapon(Self);
				InvManager.ServerSetCurrentWeapon(Self);
				if( Instigator.Weapon != Self && InvManager.PendingWeapon == Self && Instigator.Weapon.IsInState('WeaponPuttingDown') )
				{
					Instigator.Weapon.WeaponIsDown();
				}
			}
		}
		else if( Instigator != None )
		{
			// Have the client switch to the current weapon
			`Warn( " - I'm just in the inventory, so resync client and server" );
			InvManager.SetCurrentWeapon(Self);
			InvManager.ServerSetCurrentWeapon(Self);
			if( Instigator.Weapon != Self && InvManager.PendingWeapon == Self && Instigator.Weapon.IsInState('WeaponPuttingDown') )
			{
				Instigator.Weapon.WeaponIsDown();
			}
		}
	}

	reliable server function ServerStopFire( byte FireModeNum )
	{
		ClearPendingFire(FireModeNum);
	}

	/** do not allow firing in the inactive state */
	simulated function StartFire( byte FireModeNum );

	/** can't put down an inactive weapon */
	simulated function bool TryPutDown()
	{
		return FALSE;
	}
}


/*********************************************************************************************
 * State Active
 * A Weapon this is being held by a pawn should be in the active state.  In this state,
 * a weapon should loop any number of idle animations, as well as check the PendingFire flags
 * to see if a shot has been fired.
 *********************************************************************************************/

simulated state Active
{
	/** Initialize the weapon as being active and ready to go. */
	simulated event BeginState(Name PreviousStateName)
	{
		local int i;

		// Cache a reference to the AI controller
		if (Role == ROLE_Authority)
		{
			CacheAIController();
		}

		// Check to see if we need to go down
   		if( bWeaponPutDown )
		{
			`LogInv("Weapon put down requested during transition, put it down now");
			PutDownWeapon();
		}
		else if ( !HasAnyAmmo() )
		{
			WeaponEmpty();
		}
		else
		{
	        // if either of the fire modes are pending, perform them
			for( i=0; i<GetPendingFireLength(); i++ )
			{
				if( PendingFire(i) )
				{
					BeginFire(i);
					break;
				}
			}
		}
	}

	/** Override BeginFire so that it will enter the firing state right away. */
	simulated function BeginFire(byte FireModeNum)
	{
		if( !bDeleteMe && Instigator != None )
		{
			Global.BeginFire(FireModeNum);

			// in the active state, fire right away if we have the ammunition
			if( PendingFire(FireModeNum) && HasAmmo(FireModeNum) )
			{
				SendToFiringState(FireModeNum);
			}
		}
	}

	/**
	 * ReadyToFire() called by NPC firing weapon. bFinished should only be true if called from the Finished() function
 	 */

	simulated function bool ReadyToFire(bool bFinished)
	{
		return true;
	}

	/** Activate() ignored since already active
	*/
	simulated function Activate()
	{
	}


	/**
	 * Put the weapon down
	 */
	simulated function bool TryPutDown()
	{
		PutDownWeapon();
		return TRUE;
	}
}


/*********************************************************************************************
 * state WeaponFiring
 * This is the default Firing State.  It's performed on both the client and the server.
 *********************************************************************************************/

simulated state WeaponFiring
{
	simulated event bool IsFiring()
	{
		return true;
	}

	/**
	 * Timer event, call is set up in Weapon::TimeWeaponFiring().
	 * The weapon is given a chance to evaluate if another shot should be fired.
	 * This event defines the weapon's rate of fire.
	 */
	simulated function RefireCheckTimer()
	{
		// if switching to another weapon, abort firing and put down right away
		if( bWeaponPutDown )
		{
			`LogInv("Weapon put down requested during fire, put it down now");
			PutDownWeapon();
			return;
		}

		// If weapon should keep on firing, then do not leave state and fire again.
		if( ShouldRefire() )
		{
			FireAmmunition();
			return;
		}

		// Otherwise we're done firing
		HandleFinishedFiring();
	}

	simulated event BeginState( Name PreviousStateName )
	{
		`LogInv("PreviousStateName:" @ PreviousStateName);
		// Fire the first shot right away
		FireAmmunition();
		TimeWeaponFiring( CurrentFireMode );
	}

	simulated event EndState( Name NextStateName )
	{
		`LogInv("NextStateName:" @ NextStateName);
		// Set weapon as not firing
		ClearFlashCount();
		ClearFlashLocation();
		ClearTimer( nameof(RefireCheckTimer) );

		NotifyWeaponFinishedFiring( CurrentFireMode );
	}
}

simulated function HandleFinishedFiring()
{
	// Go back to active state.
	GotoState('Active');
}

/**
 *	AI function to handle a single shot being fired
 */
function NotifyWeaponFired( byte FireMode )
{
	if( AIController != None )
	{
		AIController.NotifyWeaponFired( self, FireMode );
	}
}
/**
 *	AI function to handle a firing sequence (ie burst/melee strike/etc) being finished
 */
function NotifyWeaponFinishedFiring( byte FireMode )
{
	if( AIController != None )
	{
		AIController.NotifyWeaponFinishedFiring( self, FireMode );
	}
}

/**
 * Check if current fire mode can/should keep on firing.
 * This is called from a firing state after each shot is fired
 * to decide if the weapon should fire again, or stop and go to the active state.
 * The default behavior, implemented here, is keep on firing while player presses fire
 * and there is enough ammo. (Auto Fire).
 *
 * @return	true to fire again, false to stop firing and return to Active State.
 */
simulated function bool ShouldRefire()
{
	// if doesn't have ammo to keep on firing, then stop
	if( !HasAmmo( CurrentFireMode ) )
	{
		return false;
	}

	// refire if owner is still willing to fire
	return StillFiring( CurrentFireMode );
}

/**
 * This function returns true if the weapon is still firing in a given mode
 */

simulated function bool StillFiring(byte FireMode)
{
	return ( PendingFire(FireMode) );
}

/**
 * State WeaponEquipping
 * The Weapon is in this state while transitioning from Inactive to Active state.
 * Typically, the weapon will remain in this state while its selection animation is being played.
 * While in this state, the weapon cannot be fired.
 */
simulated state WeaponEquipping
{

	simulated event BeginState(Name PreviousStateName)
	{
		`LogInv("");
		TimeWeaponEquipping();
		bWeaponPutDown	= false;
	}

	/** Activate() ignored since already becoming active
	*/
	simulated function Activate()
	{
	}

	simulated event EndState(Name NextStateName)
	{
		ClearTimer('WeaponEquipped');
	}

	simulated function WeaponEquipped()
	{
		if( bWeaponPutDown )
		{
			// if switched to another weapon, put down right away
			PutDownWeapon();
			return;
		}

		GotoState('Active');
	}
}


/**
 * State WeaponPuttingDown
 * Putting down weapon in favor of a new one.
 * Weapon is transitioning to the Inactive state.
 */
simulated state WeaponPuttingDown
{
	/**
	 * Time the process and clear the Firing flags
	 */
	simulated event BeginState(Name PreviousStateName)
	{
		`LogInv("");

		TimeWeaponPutDown();
		bWeaponPutDown = FALSE;

		// Make sure all pending fires are cleared.
		ForceEndFire();
	}

	/**
	 * We are done putting the weapon away, remove the mesh/etc.
	 */
	simulated function WeaponIsDown()
	{
		if( InvManager.CancelWeaponChange() )
		{
			return;
		}

		`LogInv("");

		// This weapon is down, remove it from the mesh
		DetachWeapon();

		// Put weapon to sleep
		//@warning: must be before ChangedWeapon() because that can reactivate this weapon in some cases
		GotoState('Inactive');

		// switch to pending weapon
		InvManager.ChangedWeapon();
	}

	simulated function bool TryPutDown()
	{
		return FALSE;
	}

	reliable client function ClientWeaponThrown()
	{
		// Call Weapon is down before cleaning up this one.
		WeaponIsDown();

		global.ClientWeaponThrown();
	}

	simulated event EndState(Name NextStateName)
	{
		`LogInv("");
		ClearTimer('WeaponIsDown');
	}
}

simulated function WeaponIsDown();

/*********************************************************************************************
 * State PendingClientWeaponSet
 * A weapon sets in this state on a remote client while it awaits full replication of all
 * properties.
 *********************************************************************************************/

State PendingClientWeaponSet
{
	simulated function PendingWeaponSetTimer()
	{
		// When variables are replicated, ClientWeaponSet, will send weapon to another state.
		// Therefore aborting this timer.
		ClientWeaponSet(bWasOptionalSet, bWasDoNotActivate);
	}

	/** Event called when weapon enters this state */
	simulated event BeginState(Name PreviousStateName)
	{
		// Set a timer to keep checking for replicated variables.
		SetTimer(0.03f, TRUE, nameof(PendingWeaponSetTimer) );
	}

	/** Event called when weapon leaves this state */
	simulated event EndState(Name NextStateName)
	{
		ClearTimer( nameof(PendingWeaponSetTimer) );
	}
}

simulated function CacheAIController()
{
	if( Instigator == None )
	{
		AIController = None;
	}
	else
	{
		AIController = AIController(Instigator.Controller);
	}
}

/**
 * Compute the approximate Screen distance from the camera to whatever is at the center of the viewport.
 * Useful for stereoizing the crosshair to reduce eyestrain.
 * NOTE: The dotproduct at the end is currently unnecessary, but if you were to use a different value for
 * TargetLoc that was not at center of screen, it'd become necessary to do the way screen projection works.
 */
simulated function float GetTargetDistance( )
{
	local float VeryFar;
	local vector HitLocation, HitNormal, ProjStart, TargetLoc, X, Y, Z;
	local rotator CameraRot;
	local PlayerController PC;

	VeryFar = 32768;

	PC = PlayerController(Instigator.Controller);
	PC.GetPlayerViewPoint(ProjStart, CameraRot);
	GetAxes(CameraRot, X, Y, Z);

	TargetLoc = ProjStart + X * VeryFar;

	if (None == GetTraceOwner().Trace(HitLocation, HitNormal, TargetLoc, ProjStart, true,,, TRACEFLAG_Bullet))
	{
		return VeryFar;
	}

	return (HitLocation - ProjStart) Dot X;
}

defaultproperties
{
	// Weapons often fire physics impulses which are invalid during physics ticking
	TickGroup=TG_PreAsyncWork

	Components.Remove(Sprite)

	bOnlyRelevantToOwner=true
	DroppedPickupClass=class'DroppedPickup'
	RespawnTime=+00030.000000

	bHidden=true
	MessageClass=class'LocalMessage'

	bCanThrow=true
	bReplicateInstigator=true
	bOnlyDirtyReplication=false
	RemoteRole=ROLE_SimulatedProxy
	AIRating=+0.5
	EquipTime=+0.33
	PutDownTime=+0.33
	WeaponRange=16384
	DefaultAnimSpeed=1.0
}
