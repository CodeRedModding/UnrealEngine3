//=============================================================================
// CheatManager
// Object within playercontroller that manages "cheat" commands
// only spawned in single player mode
//=============================================================================

class WarCheatManager extends CheatManager within PlayerController
	native;


/* AllWeapons
	Give player all available weapons
*/
exec function AllWeapons()
{
	if( (Level.Netmode!=NM_Standalone) || (Pawn == None) )
		return;

	// Weapons
	GiveWeapon("WarfareGame.Weap_MX8SnubPistol");
	GiveWeapon("WarfareGame.Weap_AssaultRifle");
	GiveWeapon("WarfareGame.Weap_ScorchRifle");
	GiveWeapon("WarfareGame.Weap_PhysicsGun");
	//GiveWeapon("WarfareGame.Weap_MagnetGun");
	GiveWeapon("WarfareGame.Weap_SledgeCannon");
	GiveWeapon("WarfareGame.Weap_RPG");

	// Grenades
	GiveWeapon("WarfareGame.Weap_FragGrenade");
}