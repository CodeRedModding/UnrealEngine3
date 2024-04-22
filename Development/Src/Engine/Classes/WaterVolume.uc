/**d
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 Games should create placeable subclasses of WaterVolume for use in game levels.
 */
class WaterVolume extends PhysicsVolume
	notplaceable;

/** Sound played when touched by an actor that can splash */
var() SoundCue  EntrySound;	

/** Effect spawned when touched by an actor that can splash */
var() class<actor> EntryActor;	

/** Sound played when untouched by an actor that can splash */
var() SoundCue  ExitSound;	

/** Effect spawned when untouched by an actor that can splash */
var() class<actor> ExitActor;	

simulated event Touch( Actor Other, PrimitiveComponent OtherComp, vector HitLocation, vector HitNormal )
{
	Super.Touch(Other, OtherComp, HitLocation, HitNormal);

	if ( Other.CanSplash() )
		PlayEntrySplash(Other);
}

function PlayEntrySplash(Actor Other)
{
	if( EntrySound != None )
	{
		Other.PlaySound(EntrySound);
		if ( Other.Instigator != None )
			Other.MakeNoise(1.0);
	}
	if( EntryActor != None )
	{
		Spawn(EntryActor);
	}
}

event untouch(Actor Other)
{
	if ( Other.CanSplash() )
		PlayExitSplash(Other);
}

function PlayExitSplash(Actor Other)
{
	if ( ExitSound != None )
	{
		Other.PlaySound(ExitSound);
		if ( Other.Instigator != None )
			Other.MakeNoise(1.0);
	}
	if( ExitActor != None )
	{
		Spawn(ExitActor);
	}
}

defaultproperties
{
	Begin Object Name=BrushComponent0
		RBChannel=RBCC_Water
		bDisableAllRigidBody=false
	End Object

	bWaterVolume=True
    FluidFriction=+00002.400000
}
