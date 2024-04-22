//=============================================================================
// InventoryManager
//	Base class to manage Pawn's inventory
//	This provides a simple interface to control and interact with the Pawn's inventory,
//	such as weapons, items and ammunition.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class InventoryManager extends Actor
	native;


/** First inventory item in inventory linked list */
var Inventory InventoryChain;

/**
 * Player will switch to PendingWeapon, once the current weapon has been put down.
 * @fixme laurent -- PendingWeapon should be made protected, because too many bugs result by setting this variable directly.
 * It's only safe to read it, but to change it, SetCurrentWeapon() should be used.
 */
var			Weapon		PendingWeapon;

var			Weapon		LastAttemptedSwitchToWeapon;

/** if true, don't allow player to put down weapon without switching to another one */
var			bool		bMustHoldWeapon;

/** Holds the current "Fire" status for both firing modes */
var	private Array<INT>  PendingFire;

//
// Network replication.
//
replication
{

	if ( (!bSkipActorPropertyReplication || bNetInitial) && (Role==ROLE_Authority) && bNetDirty && bNetOwner )
		InventoryChain;
}

event PostBeginPlay()
{
	Super.PostBeginPlay();
	Instigator = Pawn(Owner);
}

simulated function INT GetPendingFireLength(Weapon InWeapon)
{
	return PendingFire.Length;
}

simulated function SetPendingFire(Weapon InWeapon, int InFiringMode)
{
	if( InFiringMode < PendingFire.Length )
	{
		PendingFire[InFiringMode] = 1;
	}
}

simulated function ClearPendingFire(Weapon InWeapon, int InFiringMode)
{
	if( InFiringMode < PendingFire.Length )
	{
		PendingFire[InFiringMode] = 0;
	}
}

simulated final function bool IsPendingFire(Weapon InWeapon, INT InFiringMode)
{
	return bool(PendingFire[InFiringMode]);
}

simulated function ClearAllPendingFire(Weapon InWeapon)
{
	local int i;
	for(i=0; i<PendingFire.length; i++)
	{
		PendingFire[i] = 0;
	}
}

/**
 * returns all Inventory Actors of class BaseClass
 *
 * @param	BaseClass	Inventory actors returned are of, or childs of, this base class.
 * @output	Inv			Inventory actors returned.
 * @note this iterator bails if it encounters more than 100 items, since temporary loops in linked list may sometimes be created
 *	on network clients while link pointers are being replicated. For performance reasons you shouldn't have that many inventory items anyway.
 */

native final iterator function InventoryActors( class<Inventory> BaseClass, out Inventory Inv );

/**
 * Setup Inventory for Pawn P.
 * Override this to change inventory assignment (from a pawn to another)
 * Network: Server only
 */
function SetupFor(Pawn P)
{
	Instigator = P;
	SetOwner(P);
}


/**	Event called when inventory manager is destroyed, called from Pawn.Destroyed() */
event Destroyed()
{
	DiscardInventory();
}


/**
 * Handle Pickup. Can Pawn pickup this item?
 *
 * @param	ItemClass Class of Inventory our Owner is trying to pick up
 * @param	Pickup the Actor containing that item (this may be a PickupFactory or it may be a DroppedPickup)
 *
 * @return	whether or not the Pickup actor should give its item to Other
 */
function bool HandlePickupQuery(class<Inventory> ItemClass, Actor Pickup)
{
	local Inventory	Inv;

	if( InventoryChain == None )
	{
		return TRUE;
	}

	// Give other Inventory Items a chance to deny this pickup
	ForEach InventoryActors(class'Inventory', Inv)
	{
		if( Inv.DenyPickupQuery(ItemClass, Pickup) )
		{
			return FALSE;
		}
	}
	return TRUE;
}


/**
 * returns the inventory item of the requested class if it exists in this inventory manager.
 * @param	DesiredClass	class of inventory item we're trying to find.
 * @param	bAllowSubclass whether subclasses of the desired class are acceptable
 * @return	Inventory actor if found, None otherwise.
 */
simulated event Inventory FindInventoryType(class<Inventory> DesiredClass, optional bool bAllowSubclass)
{
	local Inventory		Inv;

	ForEach InventoryActors(DesiredClass, Inv)
	{
		if (bAllowSubclass || Inv.Class == DesiredClass)
		{
			return Inv;
		}
	}
	return None;
}


/**
 * Spawns a new Inventory actor of NewInventoryItemClass type, and adds it to the Inventory Manager.
 * @param	NewInventoryItemClass		Class of inventory item to spawn and add.
 * @return	Inventory actor, None if couldn't be spawned.
 */
simulated function Inventory CreateInventory(class<Inventory> NewInventoryItemClass, optional bool bDoNotActivate)
{
	local Inventory	Inv;

	if( NewInventoryItemClass != None )
	{
		inv = Spawn(NewInventoryItemClass, Owner);
		if( inv != None )
		{
			if( !AddInventory(Inv, bDoNotActivate) )
			{
				`warn("InventoryManager::CreateInventory - Couldn't Add newly created inventory" @ Inv);
				Inv.Destroy();
				Inv = None;
			}
		}
		else
		{
			`warn("InventoryManager::CreateInventory - Couldn't spawn inventory" @ NewInventoryItemClass);
		}
	}

	return Inv;
}

/**
 * Adds an existing inventory item to the list.
 * Returns true to indicate it was added, false if it was already in the list.
 *
 * @param	NewItem		Item to add to inventory manager.
 * @return	true if item was added, false otherwise.
 */
simulated function bool AddInventory(Inventory NewItem, optional bool bDoNotActivate)
{
	local Inventory Item, LastItem;

	// The item should not have been destroyed if we get here.
	if( (NewItem != None) && !NewItem.bDeleteMe )
	{
		// if we don't have an inventory list, start here
		if( InventoryChain == None )
		{
			InventoryChain = newItem;
		}
		else
		{
			// Skip if already in the inventory.
			for (Item = InventoryChain; Item != None; Item = Item.Inventory)
			{
				if( Item == NewItem )
				{
					return FALSE;
				}
				LastItem = Item;
			}
			LastItem.Inventory = NewItem;
		}

		`LogInv("adding" @ NewItem @ "bDoNotActivate:" @ bDoNotActivate);

		NewItem.SetOwner( Instigator );
		NewItem.Instigator = Instigator;
		NewItem.InvManager = Self;
		NewItem.GivenTo( Instigator, bDoNotActivate);

		// Trigger inventory event
		Instigator.TriggerEventClass(class'SeqEvent_GetInventory', NewItem);
		return TRUE;
	}

	return FALSE;
}


/**
 * Attempts to remove an item from the inventory list if it exists.
 *
 * @param	Item	Item to remove from inventory
 */
simulated function RemoveFromInventory(Inventory ItemToRemove)
{
	local Inventory Item;
	local bool		bFound;

	if( ItemToRemove != None )
	{
		if( InventoryChain == ItemToRemove )
		{
			bFound = TRUE;
			InventoryChain = ItemToRemove.Inventory;
		}
		else
		{
			// If this item is in our inventory chain, unlink it.
			for(Item = InventoryChain; Item != None; Item = Item.Inventory)
			{
				if( Item.Inventory == ItemToRemove )
				{
					bFound = TRUE;
					Item.Inventory = ItemToRemove.Inventory;
					break;
				}
			}
		}

		if( bFound )
		{
			`LogInv("removed" @ ItemToRemove);
			ItemToRemove.ItemRemovedFromInvManager();
			ItemToRemove.SetOwner(None);
			ItemToRemove.Inventory = None;
		}

		// make sure we don't have other references to the item
		if( ItemToRemove == Instigator.Weapon )
		{
			Instigator.Weapon = None;
		}

		if (Instigator.Health > 0 && Instigator.Weapon == None)
		{
			if (PendingWeapon != None && PendingWeapon != ItemToRemove)
			{
				`LogInv("Removed current weapon while changing weapons, call ChangedWeapon");
				ChangedWeapon();
			}
			else if(Instigator.Controller != None)
			{
				`LogInv("Calling ClientSwitchToBestWeapon to make sure a weapon is brought up");
				Instigator.Controller.ClientSwitchToBestWeapon(TRUE);
			}
		}
	}
}

/**
 * Discard full inventory, generally because the owner died
 */
simulated event DiscardInventory()
{
	local Inventory	Inv;
	local vector	TossVelocity;
	local bool		bBelowKillZ;

	`LogInv("");

	// don't drop any inventory if below KillZ or out of world
	bBelowKillZ = (Instigator == None) || (Instigator.Location.Z < WorldInfo.KillZ);

	ForEach InventoryActors(class'Inventory', Inv)
	{
		if( Inv.bDropOnDeath && !bBelowKillZ )
		{
			TossVelocity = vector(Instigator.GetViewRotation());
			TossVelocity = TossVelocity * ((Instigator.Velocity dot TossVelocity) + 500.f) + 250.f * VRand() + vect(0,0,250);
			Inv.DropFrom(Instigator.Location, TossVelocity);
		}
		else
		{
			Inv.Destroy();
		}
	}

	// Clear reference to Weapon
	Instigator.Weapon = None;

	// Clear reference to PendingWeapon
	PendingWeapon = None;
}

/** called when our owner is killed */
function OwnerDied()
{
	Destroy();
	if (Instigator.InvManager == self)
	{
		Instigator.InvManager = None;
	}
}

/**
 * Hook called from HUD actor. Gives access to HUD and Canvas
 *
 * @param	H	HUD
 */
simulated function DrawHud( HUD H );

/**
 * Returns a weight reflecting the desire to use the
 * given weapon, used for AI and player best weapon
 * selection.
 *
 * @param	Weapon W
 * @return	Weapon rating (range -1.f to 1.f)
 */
simulated function float GetWeaponRatingFor( Weapon W )
{
	local float Rating;

	if ( !W.HasAnyAmmo() )
		return -1;

	if (!Instigator.IsHumanControlled())
	{
		Rating = W.GetAIRating();
		// tend to stick with same weapon
		if (W == Instigator.Weapon && Instigator.Controller != None && Instigator.Controller.Enemy != None)
		{
			Rating += 0.21;
		}
	}
	else
	{
		Rating = 1;
	}

	return Rating;
}


/**
 * returns the best weapon for this Pawn in loadout
 */
simulated function Weapon GetBestWeapon( optional bool bForceADifferentWeapon  )
{
	local Weapon	W, BestWeapon;
	local float		Rating, BestRating;

	ForEach InventoryActors( class'Weapon', W )
	{
		if( w.HasAnyAmmo() )
		{
			if( bForceADifferentWeapon &&
				W == Instigator.Weapon )
			{
				continue;
			}

			Rating = W.GetWeaponRating();
			if( BestWeapon == None ||
				Rating > BestRating )
			{
				BestWeapon = W;
				BestRating = Rating;
			}
		}
	}

	return BestWeapon;
}


/**
 * Switch to best weapon available in loadout
 * Network: LocalPlayer
 */
simulated function SwitchToBestWeapon( optional bool bForceADifferentWeapon )
{
	local Weapon BestWeapon;

	`LogInv("bForceADifferentWeapon:" @ bForceADifferentWeapon);

	// if we don't already have a pending weapon,
	if( bForceADifferentWeapon ||
		PendingWeapon == None ||
		(AIController(Instigator.Controller) != None) )
	{
		// figure out the new weapon to bring up
		BestWeapon = GetBestWeapon( bForceADifferentWeapon );

		if( BestWeapon == None )
		{
			return;
		}

		// if it matches our current weapon then don't bother switching
		if( BestWeapon == Instigator.Weapon )
		{
			BestWeapon = None;
			PendingWeapon = None;
			Instigator.Weapon.Activate();
		}
	}

	// stop any current weapon fire
	Instigator.Controller.StopFiring();

	// and activate the new pending weapon
	SetCurrentWeapon(BestWeapon);
}


/**
 * Switches to Previous weapon
 * Network: Client
 */
simulated function PrevWeapon()
{
	local Weapon	CandidateWeapon, StartWeapon, W;

	StartWeapon = Instigator.Weapon;
	if ( PendingWeapon != None )
	{
		StartWeapon = PendingWeapon;
	}

	// Get previous
	ForEach InventoryActors( class'Weapon', W )
	{
		if ( W == StartWeapon )
		{
			break;
		}
		CandidateWeapon = W;
	}

	// if none found, get last
	if ( CandidateWeapon == None )
	{
		ForEach InventoryActors( class'Weapon', W )
		{
			CandidateWeapon = W;
		}
	}

	// If same weapon, do not change
	if ( CandidateWeapon == Instigator.Weapon )
	{
		return;
	}

	SetCurrentWeapon(CandidateWeapon);
}


/**
 * Switches to Next weapon
 * Network: Client
 */
simulated function NextWeapon()
{
	local Weapon	StartWeapon, CandidateWeapon, W;
	local bool		bBreakNext;

	StartWeapon = Instigator.Weapon;
	if( PendingWeapon != None )
	{
		StartWeapon = PendingWeapon;
	}

	ForEach InventoryActors( class'Weapon', W )
	{
		if( bBreakNext || (StartWeapon == None) )
		{
			CandidateWeapon = W;
			break;
		}
		if( W == StartWeapon )
		{
			bBreakNext = true;
		}
	}

	if( CandidateWeapon == None )
	{
		ForEach InventoryActors( class'Weapon', W )
		{
			CandidateWeapon = W;
			break;
		}
	}
	// If same weapon, do not change
	if( CandidateWeapon == Instigator.Weapon )
	{
		return;
	}

	SetCurrentWeapon(CandidateWeapon);
}


/**
 * Set DesiredWeapon as Current (Active) Weapon.
 * Network: LocalPlayer
 *
 * @param	DesiredWeapon, Desired weapon to assign to player
 */
reliable client function SetCurrentWeapon(Weapon DesiredWeapon)
{
	// Switch to this weapon
	InternalSetCurrentWeapon(DesiredWeapon);

	// Tell the server we have changed the pending weapon
	if( Role < Role_Authority )
	{
		ServerSetCurrentWeapon(DesiredWeapon);
	}
}

/**
 * ServerSetCurrentWeapon begins the Putdown sequence on the server.  This function makes
 * the assumption that if TryPutDown succeeded on the client, it will succeed on the server.
 * This function shouldn't be called from anywhere except SetCurrentWeapon
 *
 * Network: Dedicated Server
 */
reliable server function ServerSetCurrentWeapon(Weapon DesiredWeapon)
{
	InternalSetCurrentWeapon(DesiredWeapon);
}

simulated private function InternalSetCurrentWeapon(Weapon DesiredWeapon)
{
	local Weapon PrevWeapon;

	PrevWeapon = Instigator.Weapon;

	`LogInv("PrevWeapon:" @ PrevWeapon @ "DesiredWeapon:" @ DesiredWeapon);

	// Make sure we are switching to a new weapon
	// Handle the case where we're selecting again a weapon we've just deselected
	if( PrevWeapon != None && DesiredWeapon == PrevWeapon && !PrevWeapon.IsInState('WeaponPuttingDown') )
	{
		if(!DesiredWeapon.IsInState('Inactive') && !DesiredWeapon.IsInState('PendingClientWeaponSet'))
		{
			`LogInv("DesiredWeapon == PrevWeapon - abort"@DesiredWeapon.GetStateName());
			return;
		}
	}

	// Set the new weapon as pending
	SetPendingWeapon(DesiredWeapon);

	// if there is an old weapon handle it first.
	if( PrevWeapon != None && PrevWeapon != DesiredWeapon && !PrevWeapon.bDeleteMe && !PrevWeapon.IsInState('Inactive') )
	{
		// Try to put the weapon down.
		`LogInv("Try to put down previous weapon first.");
		PrevWeapon.TryPutdown();
	}
	else
	{
		// We don't have a weapon, force the call to ChangedWeapon
		ChangedWeapon();
	}
}


/**
 * Set the pending weapon for switching.
 * This shouldn't be called outside of SetCurrentWeapon()
 */
simulated function SetPendingWeapon(Weapon DesiredWeapon)
{
	`LogInv("SetPendingWeapon to" @ DesiredWeapon);
	// set the new weapon as pending
	PendingWeapon = DesiredWeapon;
}


/** Prevents player from being without a weapon. */
simulated function bool CancelWeaponChange()
{
	`LogInv(`showvar(PendingWeapon));
	// if PendingWeapon is None, prevent instigator from having no weapon,
	// so re-activate current weapon.
	if( PendingWeapon == None && bMustHoldWeapon )
	{
		PendingWeapon = Instigator.Weapon;
	}

	return FALSE;
}

/** Clear pending weapon, put it in a good state. */
simulated function ClearPendingWeapon()
{
	`LogInv(`showvar(PendingWeapon));

	if (PendingWeapon != None)
	{
		PendingWeapon.GotoState('Inactive');
		PendingWeapon = None;
	}
}


/**
 * ChangedWeapon is called when the current weapon is finished being deactivated
 */
simulated function ChangedWeapon()
{
	local Weapon OldWeapon;

	// Save current weapon as old weapon
	OldWeapon = Instigator.Weapon;

	// Make sure we can switch to a null weapon, otherwise, reactivate the current weapon
	`LogInv(`showvar(PendingWeapon)@`showvar(bMustHoldWeapon));
	if( PendingWeapon == None && bMustHoldWeapon )
	{
		if( OldWeapon != None )
		{
			OldWeapon.Activate();
			PendingWeapon = OldWeapon;
		}
	}

	`LogInv("switch from" @ OldWeapon @ "to" @ PendingWeapon);

	// switch to Pending Weapon
	Instigator.Weapon = PendingWeapon;

	// Play any Weapon Switch Animations
	Instigator.PlayWeaponSwitch(OldWeapon, PendingWeapon);

	// If we are going to an actual weapon, activate it.
	if( PendingWeapon != None )
	{
		// Setup the Weapon
		PendingWeapon.Instigator = Instigator;

		// Make some noise
		if( WorldInfo.Game != None )
		{
			Instigator.MakeNoise( 0.1, 'ChangedWeapon' );
		}

		// Activate the Weapon
		PendingWeapon.Activate();
		PendingWeapon = None;
	}

	// Notify of a weapon change
	if( Instigator.Controller != None )
	{
		Instigator.Controller.NotifyChangedWeapon(OldWeapon, Instigator.Weapon);
	}
}


/**
 * Weapon just given to a player, check if player should switch to this weapon
 * Network: LocalPlayer
 * Called from Weapon.ClientWeaponSet()
 */
simulated function ClientWeaponSet(Weapon NewWeapon, bool bOptionalSet, optional bool bDoNotActivate)
{
	local Weapon OldWeapon;

	`LogInv("NewWeapon:" @ NewWeapon @ "bOptionalSet:" @ bOptionalSet @ "bDoNotActivate:" @ bDoNotActivate);
	if( !bDoNotActivate )
	{
		OldWeapon = Instigator.Weapon;

		// If no current weapon, then set this one
		if( OldWeapon == None || OldWeapon.bDeleteMe || OldWeapon.IsInState('Inactive') )
		{
			`LogInv("OldWeapon == None or Inactive - Set new weapon right away" @ NewWeapon);
			SetCurrentWeapon(NewWeapon);
			return;
		}

		if( OldWeapon == NewWeapon )
		{
			if( NewWeapon.IsInState('PendingClientWeaponSet') )
			{
				`LogInv("OldWeapon == NewWeapon - but in PendingClientWeaponSet, so reset." @ NewWeapon);
				SetCurrentWeapon(NewWeapon);
			}
			else
			{
				`LogInv("OldWeapon == NewWeapon - abort" @ NewWeapon);
			}
			return;
		}

		if( bOptionalSet )
		{
			if( OldWeapon.DenyClientWeaponSet() ||
				(Instigator.IsHumanControlled() && PlayerController(Instigator.Controller).bNeverSwitchOnPickup) )
			{
				`LogInv("bOptionalSet && (DenyClientWeaponSet() || bNeverSwitchOnPickup) - abort" @ NewWeapon);

				LastAttemptedSwitchToWeapon = NewWeapon;
				return;
			}
		}

		if( PendingWeapon == None || !PendingWeapon.HasAnyAmmo() || PendingWeapon.GetWeaponRating() < NewWeapon.GetWeaponRating() )
		{
			// Compare switch priority and decide if we should switch to new weapon
			if( !Instigator.Weapon.HasAnyAmmo() || Instigator.Weapon.GetWeaponRating() < NewWeapon.GetWeaponRating() )
			{
				`LogInv("Switch to new weapon:" @ NewWeapon);
				SetCurrentWeapon(NewWeapon);
				return;
			}
		}
	}

	`LogInv("Send to inactive state" @ NewWeapon);
	NewWeapon.GotoState('Inactive');
}

simulated function UpdateController()
{
	local Inventory Item;
	local Weapon Weap;

	for (Item = InventoryChain; Item != None; Item = Item.Inventory)
	{
		Weap = Weapon(Item);
		if ( Weap != None )
		{
			Weap.CacheAIController();
		}
	}
}

defaultproperties
{
	TickGroup=TG_DuringAsyncWork

	bReplicateInstigator=TRUE
	RemoteRole=ROLE_SimulatedProxy
	bOnlyDirtyReplication=TRUE
	bOnlyRelevantToOwner=TRUE
	NetPriority=1.4
	bHidden=TRUE
	Physics=PHYS_None
	bReplicateMovement=FALSE
	bStatic=FALSE
	bNoDelete=FALSE
}
