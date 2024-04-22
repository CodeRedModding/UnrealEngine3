/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class EmitterSpawnable extends Emitter
	notplaceable;

var repnotify ParticleSystem ParticleTemplate;

replication
{
	if (bNetInitial)
		ParticleTemplate;
}

simulated event SetTemplate(ParticleSystem NewTemplate, optional bool bDestroyOnFinish)
{
	Super.SetTemplate(NewTemplate, bDestroyOnFinish);

	ParticleTemplate = NewTemplate;
}

simulated event ReplicatedEvent(name VarName)
{
	if (VarName == 'ParticleTemplate')
	{
		SetTemplate(ParticleTemplate, bDestroyOnSystemFinish);
		ParticleSystemComponent.ActivateSystem();
		if (ParticleTemplate == None && bDestroyOnSystemFinish)
		{
			// prevent emitter from hanging around forever with no template
			Destroy();
		}
	}
	else
	{
		Super.ReplicatedEvent(VarName);
	}
}

defaultproperties
{
	Begin Object Name=ParticleSystemComponent0
		SecondsBeforeInactive=0
	End Object

	bNoDelete=false
	bDestroyOnSystemFinish=true
	bNetTemporary=true
}
