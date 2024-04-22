/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleVelocityInheritParent extends ParticleModuleVelocityBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 *	The scale to apply tot he parent velocity prior to adding it to the particle velocity during spawn.
 *	Value is retrieved using the EmitterTime of the emitter.
 */
var(Velocity) rawdistributionvector	Scale;

cpptext
{
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
}

defaultproperties
{
	bSpawnModule=true

	Begin Object Class=DistributionVectorConstant Name=DistributionScale
		Constant=(X=1.0,Y=1.0,Z=1.0)
	End Object
	Scale=(Distribution=DistributionScale)
}
