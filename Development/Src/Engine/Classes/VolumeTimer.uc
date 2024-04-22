/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class VolumeTimer extends Info;

var PhysicsVolume V;

event PostBeginPlay()
{
	Super.PostBeginPlay();
	V = PhysicsVolume(Owner);
	SetTimer(V.PainInterval, true);
}

event Timer()
{
	V.TimerPop(self);
}

defaultproperties
{
	TickGroup=TG_PreAsyncWork

	bStatic=false
	RemoteRole=ROLE_None
}
