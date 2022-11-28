/**
 * Created By:	Joe Wilcox
 * Copyright:	(c) 2004
 * Company:		Epic Games
*/

class UTWeapon_ShockRifle extends UTWeap_InstagibRifle;

defaultproperties
{
	AttachmentClass=class'UTGame.UTAttachment_ShockRifle'
	PickupMessage="You picked up a Shock Rifle"

	WeaponFireTypes(0)=EWFT_InstantHit
	WeaponFireTypes(1)=EWFT_Projectile
	WeaponProjectiles(1)=class'UTProj_ShockBall'

	InstantHitDamage(0)=45
	FireInterval(0)=+0.7
	FireInterval(1)=+0.6
 	InstantHitDamageTypes(0)=class'UTDmgType_ShockPrimary'
	InstantHitDamageTypes(1)=none

	// Lighting

	Begin Object name=MuzzleFlashLightC
		Color=(R=255,G=80,B=200)
	End Object

	ItemName="Shock Rifle"

    WeaponEquipSnd=SoundCue'ShockRifle.SC_SwitchToShockRifle'
    WeaponPutDownSnd=SoundCue'ShockRifle.SC_SwitchToShockRifle'

    WeaponFireSnd(0)=SoundCue'ShockRifle.SC_ShockRifleFire'
    WeaponFireSnd(1)=SoundCue'ShockRifle.SC_ShockRifleAltFire'

}
