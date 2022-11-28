//=============================================================================
// InventoryManager
//	Base class to manage Pawn's inventory
//	This provides a simple interface to control and interact with the Pawn's inventory,
//	such as weapons, items and ammunition.
//=============================================================================

class InventoryManager extends Actor
	native;

/** First inventory item in inventory linked list */
var travel	Inventory	InventoryChain;	

/** Player will switch to PendingWeapon, once the current weapon has been put down. */
var			Weapon		PendingWeapon;

//
// Network replication.
//

replication
{

	unreliable if ( (!bSkipActorPropertyReplication || bNetInitial) && (Role==ROLE_Authority) && bNetDirty && bNetOwner )
		InventoryChain;

	// replicated functions sent to server by owning client
	reliable if( Role<ROLE_Authority )
		ServerChangedWeapon;
}

function PostBeginPlay()
{
	Super.PostBeginPlay();
	Instigator = Pawn(Owner);
}

/**
 * returns all Inventory Actors of class BaseClass
 *
 * @param	BaseClass	Inventory actors returned are of, or childs of, this base class.
 * @output	Inv			Inventory actors returned.
 */
native final iterator function InventoryActors( class<Inventory> BaseClass, out Inventory Inv );

/**
 * Setup Inventory for Pawn P.
 * Override this to change inventory assignment (from a pawn to another)
 * Network: Server only
 */
function SetupFor( Pawn P )
{
	if ( Role < Role_Authority )
		return;

	Instigator = P;
	SetOwner( P );
}


/**	Event called when inventory manager is destroyed, called from Pawn.Destroyed() */
event Destroyed()
{
	DiscardInventory();
}

/**
 * Handle Pickup. Can Pawn pickup this item? 
 *
 * @param	Item	Item player is trying to pickup
 *
 * @return	true if player can pickup this item.
 */
function bool HandlePickupQuery( class<Inventory> ItemClass )
{
	local Inventory	Inv;

	if ( InventoryChain == None )
	{
		return true;
	}

	// Give other Inventory Items a chance to deny this pickup
	ForEach InventoryActors( class'Inventory', Inv )
	{
		if ( Inv.DenyPickupQuery( ItemClass ) )
		{
			return false;
		}
	}
	return true;
}

/**
 * returns the inventory item of the requested class if it exists in this inventory manager.
 *
 * @param	DesiredClass	class of inventory item we're trying to find.
 *
 * @return	Inventory actor if found, None otherwise.
 */
simulated event Inventory FindInventoryType( class<Inventory> DesiredClass )
{
	local Inventory		Inv;

	ForEach InventoryActors( DesiredClass, Inv )
		return Inv;
	return None;
}

/**
 * Spawns a new Inventory actor of NewInventoryItemClass type, and adds it to the Inventory Manager.
 *
 * @param	NewInventoryItemClass		Class of inventory item to spawn and add.
 *
 * @return	Inventory actor, None if couldn't be spawned.
 */
function Inventory CreateInventory( class<Inventory> NewInventoryItemClass )
{
	local Inventory	Inv;

	if ( NewInventoryItemClass != None )
	{
		inv = Spawn(NewInventoryItemClass);
		if ( inv != None )
		{
			if ( !AddInventory( Inv ) )
			{
				Warn("InventoryManager::CreateInventory - Couldn't Add newly created inventory" @ Inv);
				Inv.Destroy();
				Inv = None;
			}
		}
		else
			Warn("InventoryManager::CreateInventory - Couldn't spawn inventory" @ NewInventoryItemClass);
	}

	return Inv;
}

/**
 * Adds an existing inventory item to the list.
 * Returns true to indicate it was added, false if it was already in the list.
 *
 * @param	NewItem		Item to add to inventory manager.
 *
 * @return	true if item was added, false otherwise.
 */
function bool AddInventory( Inventory NewItem )
{
	local Inventory Item, LastItem;

	// The item should not have been destroyed if we get here.
	if ( (NewItem != None) && !NewItem.bDeleteMe )
	{
		// if we don't have an inventory list, start here
		if ( InventoryChain == None )
		{
			InventoryChain = newItem;
		}
		else
		{
			// Skip if already in the inventory.
			for (Item = InventoryChain; Item != None; Item = Item.Inventory)
			{
				if ( Item == NewItem )
				{
					return false;
				}
				LastItem = Item;
			}
			LastItem.Inventory = NewItem;
		}

		NewItem.SetOwner( Instigator );
		NewItem.SetBase( Instigator );
		NewItem.Instigator = Instigator;
		NewItem.InvManager = Self;
		newItem.GivenTo( Instigator );
		return true;
	}

	return false;
}

/**
 * Attempts to remove an item from the inventory list if it exists.
 *
 * @param	Item	Item to remove from inventory
 */
function RemoveFromInventory( Inventory ItemToRemove )
{
	local Inventory Item;
	local bool		bFound;

	if ( ItemToRemove != None )
	{
		// make sure we don't have other references to the item
		if ( ItemToRemove == Pawn(Owner).Weapon )
		{
			Pawn(Owner).Weapon = None;
		}

		if ( InventoryChain == ItemToRemove )
		{
			bFound = true;
			InventoryChain = ItemToRemove.Inventory;
		}
		else
		{
			// If this item is in our inventory chain, unlink it.
			for (Item = InventoryChain; Item != None; Item = Item.Inventory)
			{
				if (Item.Inventory == ItemToRemove)
				{
					bFound = true;
					Item.Inventory = ItemToRemove.Inventory;
					break;
				}
			}
		}

		if ( bFound )
		{
			ItemToRemove.ItemRemovedFromInvManager();
			ItemToRemove.SetOwner(None);
			ItemToRemove.Inventory = None;
		}
	}
}

/**
 * Discard full inventory 
 */
function DiscardInventory()
{
	local Inventory	Inv;

	ForEach InventoryActors( class'Inventory', Inv )
	{	
		Inv.Destroy();
	}
}

/**
 * Damage modifier. Is Pawn carrying items that can modify taken damage? 
 * Called from GameInfo.ReduceDamage() 
 */
function int ModifyDamage( int Damage, pawn instigatedBy, vector HitLocation, vector Momentum, class<DamageType> DamageType )
{
	return Damage;
}

/**
 * Used to inform inventory when owner event occurs (for example jumping or weapon change)
 *
 * @param	EventName	Name of event to forward to inventory items.
 */
function OwnerEvent(name EventName)
{
	local Inventory	Inv;

	ForEach InventoryActors( class'Inventory', Inv )
		if ( Inv.bReceiveOwnerEvents )
			Inv.OwnerEvent( EventName );
}

/**
 * Hook called from HUD actor. Gives access to HUD and Canvas
 *
 * @param	H	HUD
 */
simulated function DrawHud( HUD H )
{
	local Inventory Inv;

	// Send RenderOverlays event to Inv Items requesting it
	ForEach InventoryActors( class'Inventory', Inv )
	{
    	if ( Inv.bRenderOverlays )
		{
	    	Inv.RenderOverlays( H );
		}
	}

	// Send ActiveRenderOverlays event to active weapon
	if ( Pawn(Owner).Weapon != None )
	{
		Pawn(Owner).Weapon.ActiveRenderOverlays( H );
	}
}

//
// Weapon Interface
//

/**
 * Pawn desires to fire. By default it fires the Active Weapon if it exists.
 * Called from PlayerController::StartFire() -> Pawn::StartFire()
 * Network: Local Player
 *
 * @param	FireModeNum		Fire mode number.
 */
simulated function StartFire( byte FireModeNum )
{
    if ( Pawn(Owner).Weapon != None )
	{
        Pawn(Owner).Weapon.StartFire( FireModeNum );
	}
}

/**
 * Pawn stops firing. 
 * i.e. player releases fire button, this may not stop weapon firing right away. (for example press button once for a burst fire)
 * Network: Local Player
 *
 * @param	FireModeNum		Fire mode number.
 */
simulated function StopFire( byte FireModeNum )
{
    if ( Pawn(Owner).Weapon != None )
	{
        Pawn(Owner).Weapon.StopFire( FireModeNum );
	}
}

/**
 * returns true if ThisWeapon is the Pawn's active weapon.
 *
 * @param	ThisWeapon	weapon to test if it's the Pawn's active weapon.
 * 
 * @return	true if ThisWeapon is the Pawn's current weapon
 */
simulated function bool IsActiveWeapon( Weapon ThisWeapon )
{
	return (ThisWeapon == Pawn(Owner).Weapon);
}

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

	Rating = 1;
	// tend to stick with same weapon
	if ( !Instigator.IsHumanControlled() && IsActiveWeapon( W ) && (Instigator.Controller.Enemy != None) )
		Rating += 0.21;

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
		if( bForceADifferentWeapon &&
			IsActiveWeapon( W ) )
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

	return BestWeapon;
}

/**
 * Switch to best weapon available in loadout
 * Network: LocalPlayer
 */
simulated function SwitchToBestWeapon( optional bool bForceADifferentWeapon )
{
	// if we don't already have a pending weapon,
	if( bForceADifferentWeapon || 
		PendingWeapon == None || 
		(AIController(Instigator.Controller) != None) )
	{
		// figure out the new weapon to bring up
		PendingWeapon = GetBestWeapon( bForceADifferentWeapon );

		// if it matches our current weapon then don't bother switching
		if( PendingWeapon == Pawn(Owner).Weapon )
		{
			PendingWeapon = None;
		}

		if( PendingWeapon == None )
		{
    		return;
		}
	}

	// stop any current weapon fire
	Instigator.Controller.StopFiring();

	// and activate the new pending weapon
	SetCurrentWeapon( PendingWeapon );
}

/**
 * Tries to find weapon of class ClassName, and switches to it
 *
 * @param	ClassName, class of weapon to switch to
 */
exec function SwitchToWeaponClass( String ClassName )
{
	local Weapon		CandidateWeapon;
	local class<Weapon>	WeapClass;

	WeapClass = class<Weapon>(DynamicLoadObject(ClassName, class'Class'));

	if ( WeapClass != None )
	{
		CandidateWeapon = Weapon(FindInventoryType( WeapClass ));
		if ( CandidateWeapon != None )
		{
			SetCurrentWeapon( CandidateWeapon );
		}
		else
		{
			log("SwitchToWeaponClass weapon not found in inventory" @ String(WeapClass) );
		}
	}
	else
	{
		log("SwitchToWeaponClass weapon class not found" @ ClassName );
	}
}

/**
 * Switches to Previous weapon
 * Network: Client
 */
simulated function PrevWeapon()
{
	local Weapon	CandidateWeapon, StartWeapon, W;

	StartWeapon = Pawn(Owner).Weapon;
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
	if ( CandidateWeapon == Pawn(Owner).Weapon )
	{
		return;
	}

	SetCurrentWeapon( CandidateWeapon );
}

/**
 * Switches to Next weapon
 * Network: Client
 */
simulated function NextWeapon()
{
	local Weapon	StartWeapon, CandidateWeapon, W;
	local bool		bBreakNext;

	StartWeapon = Pawn(Owner).Weapon;
	if ( PendingWeapon != None )
	{
		StartWeapon = PendingWeapon;
	}

	ForEach InventoryActors( class'Weapon', W )
	{
		if ( bBreakNext || (StartWeapon == None) )
		{
			CandidateWeapon = W;
			break;
		}
		if ( W == StartWeapon )
		{
			bBreakNext = true;
		}
	}

	if ( CandidateWeapon == None )
	{
		ForEach InventoryActors( class'Weapon', W )
		{
			CandidateWeapon = W;
			break;
		}
	}
	// If same weapon, do not change
	if ( CandidateWeapon == Pawn(Owner).Weapon )
	{
		return;
	}

	SetCurrentWeapon( CandidateWeapon );
}

/**
 * Set DesiredWeapon as Current (Active) Weapon.
 * Network: LocalPlayer
 *
 * @param	DesiredWeapon, Desired weapon to assign to player
 */
simulated function SetCurrentWeapon( Weapon DesiredWeapon )
{
	if ( (Pawn(Owner).Weapon == None || Pawn(Owner).Weapon.bDeleteMe) && DesiredWeapon != None )
    {
		// If we don't have a current weapon, set desired one right away
        PendingWeapon = DesiredWeapon;
    	ChangedWeapon();
    }
	else if ( Pawn(Owner).Weapon != DesiredWeapon )
    {
		// put weapon down, and switch to desiredweapon
    	PendingWeapon = DesiredWeapon;
		if ( !Pawn(Owner).Weapon.TryPutDown() )
		{
			PendingWeapon = None;
		}
    }
}

/**
 * Current Weapon has changed.
 * Switch to Pending weapon.
 * Network: LocalPlayer
 */
simulated function ChangedWeapon()
{
	local Weapon OldWeapon;

	if ( !Pawn(Owner).IsLocallyControlled() )
	{
		log("ChangedWeapon, must be called locally");
		return;
	}

	// Save current weapon as old weapon
	OldWeapon = Pawn(Owner).Weapon;

	// if switching to same weapon
	if ( Pawn(Owner).Weapon == PendingWeapon )
	{
		if ( Pawn(Owner).Weapon == None )
		{
			// If player has no weapon, then try to find one
			SwitchToBestWeapon();
			return;
		}
		else if ( Pawn(Owner).Weapon.IsInState('PuttingDown') )
		{
			// If switching to same weapon, reactivate it, cancel switch.
			Pawn(Owner).Weapon.GotoState('Active');
		}

		PendingWeapon = None;
		// FIXME LAURENT -- this is unnecessary as 'PuttingDown' is only valid on LocalClient, no need to replicate no change.
		// ServerChangedWeapon(OldWeapon, Pawn(Owner).Weapon);
		return;
	}

	
	/*
	// FIXME LAURENT, why forbid setting current weapon to none?
	if ( PendingWeapon == None )
	{
		Pawn(Owner).Weapon.GotoState('Active');
		PendingWeapon = Pawn(Owner).Weapon;
		return;
	}
	*/
	// Switch to Pending Weapon 
	Pawn(Owner).Weapon = PendingWeapon;

	if ( (Pawn(Owner).Weapon != None) && (Level.NetMode == NM_Client) )
	{
		Pawn(Owner).Weapon.Activate();
	}

	PendingWeapon					= None;
	Pawn(Owner).Weapon.Instigator	= Instigator;

	Instigator.PlayWeaponSwitch( OldWeapon, Pawn(Owner).Weapon );
	ServerChangedWeapon(OldWeapon, Pawn(Owner).Weapon);

	if ( Instigator.Controller != None )
	{
		Instigator.Controller.NotifyChangedWeapon( OldWeapon, Pawn(Owner).Weapon );
	}
}

/**
 * Current Weapon has changed.
 * Network: Server
 *
 * @param	OldWeapon	Old weapon held by Pawn.
 * @param	NewWeapon	New weapon held by Pawn.
 */
function ServerChangedWeapon(Weapon OldWeapon, Weapon NewWeapon)
{
	// Update Pawn's current weapon.
	Pawn(Owner).Weapon = NewWeapon;

	// Fire PlayWeaponSwitch() event
	if ( !Instigator.IsLocallyControlled() )
	{
		Instigator.PlayWeaponSwitch( OldWeapon, NewWeapon);
	}

	// Send old weapon to the Inactive state
	if ( OldWeapon != None && !OldWeapon.IsInState('Inactive') )
	{
		OldWeapon.GotoState('Inactive');
	}

	OwnerEvent('ChangedWeapon');	// tell inventory that weapon changed (in case any effect was being applied)

	if ( Pawn(Owner).Weapon != None )
	{
		if ( OldWeapon == Pawn(Owner).Weapon )
		{
			log("ServerChangedWeapon, switched to same weapon");
			// FIXME LAURENT -- this should never happen. 'PuttingDown' is on local player, never on dedicated server. We already tested for it.
			/*
			if ( Pawn(Owner).Weapon.IsInState('PuttingDown') )
			{
				Pawn(Owner).Weapon.Activate();
			}
			*/
			return;
		}
		else if ( Level.Game != None )
		{
			MakeNoise(0.1 * Level.Game.GameDifficulty);
		}

		Pawn(Owner).Weapon.Activate();
	}
}

/**
 * Weapon just given to a player, check if player should switch to this weapon
 * Network: LocalPlayer
 * Called from Weapon.ClientWeaponSet()
 */
simulated function ClientWeaponSet( Weapon newWeapon, bool bOptionalSet )
{
	local Weapon	W;

	// If no current weapon, then set this one
	if ( Pawn(Owner).Weapon == None )
	{
		SetCurrentWeapon( newWeapon );
		return;
	}

	if ( Pawn(Owner).Weapon == newWeapon )
	{
		return;
	}

	if ( bOptionalSet && (Instigator.IsHumanControlled() && PlayerController(Instigator.Controller).bNeverSwitchOnPickup) )
	{
		return;
	}

	// Compare switch priority and decide if we should switch to new weapon
	if ( Pawn(Owner).Weapon.GetWeaponRating() < newWeapon.GetWeaponRating() )
	{
		W = PendingWeapon;
		PendingWeapon = newWeapon;
		newWeapon.GotoState('Inactive');

		if ( !Pawn(Owner).Weapon.TryPutDown() )
		{
			PendingWeapon = W;
		}
		return;
	}

	newWeapon.GotoState('Inactive');
}

//
// Ammunition Interface
//

/**
 * consume Ammunition for specified firemode
 * 
 * @param	W				Weapon
 * @param	FireModeNum		fire mode number
 */
function WeaponConsumeAmmo( Weapon W, optional byte FireModeNum );

/**
 * Returns percentage of full ammo
 *
 * @param	W				Weapon
 * @param	FireModeNum		Fire mode number
 *
 * @return	percentage of ammo left (0 to 1.f range)
 */
simulated function float GetAmmoLeftPercentFor( Weapon W, optional byte FireModeNum )
{
	return 1.f;
}

/**
 * Returns true if the weapon has ammo to fire for the specified firemode
 *
 * @param	W				Weapon
 * @param	FireModeNum		Fire mode number
 *
 * @return	true if the weapon has ammo to fire for the specified firemode
 */
simulated function bool WeaponHasAmmo( Weapon W, optional byte FireModeNum )
{
	return true;
}

/**
 * Returns true if the weapon has any type of ammunition left to fire (maybe not for the current firemode)
 *
 * @param	W	Weapon
 *
 * @return	true if weapon has ammution for this weapon (anything left for any fire mode)
 */
simulated function bool WeaponHasAnyAmmo( Weapon W )
{
	return true;
}

defaultproperties
{
	CollisionComponent=None
	Components.Remove(CollisionCylinder)
	bReplicateInstigator=true
	RemoteRole=ROLE_SimulatedProxy
	bOnlyDirtyReplication=true
	bOnlyRelevantToOwner=true
	bTravel=true
	NetPriority=1.4
	bHidden=true
	Physics=PHYS_None
	bReplicateMovement=false
	bStatic=false
	bNoDelete=false
}
