//=============================================================================
// CheatManager
// Object within playercontroller that manages "cheat" commands
// only spawned in single player mode
//=============================================================================

class UTCheatManager extends CheatManager within PlayerController
	native;


/* AllWeapons
	Give player all available weapons
*/
exec function AllWeapons()
{
	if( (Level.Netmode!=NM_Standalone) || (Pawn == None) )
		return;

	// Weapons
	GiveWeapon("UTGame.UTWeap_InstagibRifle");
}

/* AllAmmo
	Sets maximum ammo on all weapons
*/
exec function AllAmmo()
{
	if ( (Pawn != None) && (UTInventoryManager(Pawn.InvManager) != None) )
		UTInventoryManager(Pawn.InvManager).AllAmmo();
}
