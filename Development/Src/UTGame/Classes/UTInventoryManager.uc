/**
 * UTInventoryManager
 * UT inventory definition
 *
 * Created by:	Steven Polge
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class UTInventoryManager extends InventoryManager;

/**
 * Switches to Previous weapon
 * Network: Client
 */
simulated function PrevWeapon()
{
	if ( UTWeapon(Pawn(Owner).Weapon) != None && UTWeapon(Pawn(Owner).Weapon).DoOverridePrevWeapon() )
		return;

	super.PrevWeapon();
}

/**
 *	Switches to Next weapon
 *	Network: Client
 */
simulated function NextWeapon()
{
	if ( UTWeapon(Pawn(Owner).Weapon) != None && UTWeapon(Pawn(Owner).Weapon).DoOverrideNextWeapon() )
		return;

	super.NextWeapon();
}

function AllAmmo()
{
	local Inventory Inv;

	for( Inv=InventoryChain; Inv!=None; Inv=Inv.Inventory )
		if ( UTWeapon(Inv)!=None )
			UTWeapon(Inv).Loaded();
}


/**
 * Pawn desires to fire. By default it fires the Active Weapon if it exists.
 * Called from PlayerController::StartFire() -> Pawn::StartFire()
 * Network: Local Player
 *
 * @param	FireModeNum		Fire mode number.
 */
simulated function StartFire( byte FireModeNum )
{
	local Pawn POwner;
	POwner = Pawn(Owner);
    if ( UTWeapon(POwner.Weapon) != None )
	{
       UTWeapon(POwner.Weapon).WeaponStartFire( FireModeNum );
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
	local Pawn POwner;
	POwner = Pawn(Owner);
    if ( UTWeapon(POwner.Weapon) != None )
	{
        UTWeapon(POwner.Weapon).WeaponStopFire( FireModeNum );
	}
}


defaultproperties
{
}
