class Weap_WretchMelee extends WarWeapon;

/** 
 * Wretch melee weapon
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

/** Auto fire state */
state Firing
{
	/** @see Weapon::GetTraceRange() */
	simulated function float GetTraceRange()
	{
		return fMeleeRange;
	}
}

defaultproperties
{
	WeaponEquipSound=None
	WeaponReloadSound=None
	fMeleeRange=160
	ReloadDuration=2.5
	ItemName="Wretch Melee"
	FireSound=SoundCue'Geist_Wretch.Sounds.WretchAttack'
	FiringStatesArray(0)=Firing
	FireModeInfoArray(0)=(Damage=20,FireRate=100,Recoil=0,Inaccuracy=3,Name="Melee")
}