/**
 * Created By:	Joe Wilcox
 * Copyright:	(c) 2004
 * Company:		Epic Games
*/

class UTProj_FlakShell extends UTProjectile;

defaultproperties
{

    speed=1200.000000
    Damage=90.000000
    MomentumTransfer=75000
    MyDamageType=class'UTDmgType_FlakShell'
    LifeSpan=8.0
    RotationRate=(Pitch=50000)
    DesiredRotation=(Pitch=30000)
    bCollideWorld=true
	TossZ=+225.0

	ProjFlightTemplate=ParticleSystem'RocketLauncher.FX.P_Weapons_RocketLauncher_FX_RocketTrail'
    ProjExplosionTemplate=ParticleSystem'RocketLauncher.FX.P_Weapons_RocketLauncher_FX_RocketExplosion'

	Physics=PHYS_Falling

	Begin Object class=PointLightComponent name=FlakLight
		Brightness=2.0
		Color=(R=255,G=150,B=40)
		Radius=180
		CastShadows=True
		bEnabled=true
	End Object

	Begin Object Class=TransformComponent Name=FlakTransform
    	TransformedComponent=FlakLight
    	Translation=(X=-200,Z=5)
    End Object

	Components.Add(FlakTransform);
	Components.Remove(Sprite)

	DrawScale=1.5
}
