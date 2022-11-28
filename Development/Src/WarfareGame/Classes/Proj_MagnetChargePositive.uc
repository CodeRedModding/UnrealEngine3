/** 
 * MagnetGun Magnet Charge Projectile
 * Positive charge4
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class Proj_MagnetChargePositive extends Proj_MagnetCharge;

var()	RB_Spring			ChargeSpring;

simulated function Destroyed()
{
	if ( ChargeSpring != None )
	{
		ChargeSpring.Clear();
	}

	super.Destroyed();
}

defaultproperties
{
	Begin Object Name=StaticMeshComponent0
		Materials(0)=Material'COG_MagnetGun.Materials.PositiveChargeMat'
    End Object

	Begin Object Class=RB_Spring Name=RB_Spring0
		bEnableForceMassRatio=true
		MaxForceMassRatio=600.0
		DampSaturateVel=0.1
		DampMaxForce=20.0
		SpringSaturateDist=0.1
		SpringMaxForce=200.0
		SpringMaxForceTimeScale=(Points=((InVal=0.0,OutVal=0.0),(InVal=0.8,OutVal=1.0),(InVal=3.0,OutVal=1.0),(InVal=4.0,OutVal=0.0)))
	End Object
	Components.Add(RB_Spring0)
	ChargeSpring=RB_Spring0
}