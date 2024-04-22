/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class TriggerVolume extends Volume
	native
	placeable;

cpptext
{
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
}

simulated event PostBeginPlay()
{
	Super.PostBeginPlay();

	// match bProjTarget to weapons (zero extent) collision setting
	if (BrushComponent != None)
	{
		bProjTarget = BrushComponent.BlockZeroExtent;
	}
}

simulated function bool StopsProjectile(Projectile P)
{
	return false;
}

defaultproperties
{
	bColored=true
	BrushColor=(R=100,G=255,B=100,A=255)

	bCollideActors=true
	bProjTarget=true
	SupportedEvents.Empty
	SupportedEvents(0)=class'SeqEvent_Touch'
	SupportedEvents(1)=class'SeqEvent_TakeDamage'
}
