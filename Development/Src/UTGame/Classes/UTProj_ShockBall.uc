/**
 * Created By:	Joe Wilcox
 * Copyright:	(c) 2004
 * Company:		Epic Games
*/

class UTProj_ShockBall extends UTProjectile;

var class<UTDamageType>	ComboDamageType;

event TakeDamage( int Damage, Pawn EventInstigator, vector HitLocation, vector Momentum, class<DamageType> DamageType, optional TraceHitInfo HitInfo)
{
	if ( DamageType==ComboDamageType )
	{
		// COMBO

	}

	super.TakeDamage(Damage,EventInstigator,HitLocation,Momentum,DamageType, HitInfo);
}


defaultproperties
{

	ProjFlightTemplate=ParticleSystem'ShockRifle.FX.P_Weapons_ShockRifle_FX_AltFire'

	Begin Object class=PointLightComponent name=ShockLight
		Brightness=2.0
		Color=(R=255,G=128,B=200)
		Radius=180
		CastShadows=True
		bEnabled=true
	End Object

	Begin Object Class=TransformComponent Name=ShockTransform
    	TransformedComponent=ShockLight
    	Translation=(X=-200,Z=5)
    End Object

	Components.Remove(Sprite)

//	Components.Add(ShockTransform);

	Begin Object Name=CollisionCylinder
		CollisionRadius=10
		CollisionHeight=10
		AlwaysLoadOnClient=True
		AlwaysLoadOnServer=True
		BlockNonZeroExtent=true
		BlockZeroExtent=true
		BlockActors=true
		CollideActors=true
	End Object


    Speed=1150
    MaxSpeed=1150

    Damage=45
    DamageRadius=150
    MomentumTransfer=70000

    MyDamageType=class'UTDmgType_ShockBall'
    LifeSpan=8.0

    bCollideWorld=true
    DrawScale=0.7
    bProjTarget=True

	ComboDamageType=class'UTDmgType_ShockPrimary'

}
