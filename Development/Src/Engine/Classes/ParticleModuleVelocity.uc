/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleVelocity extends ParticleModuleVelocityBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** 
 *	The velocity to apply to a particle when it is spawned.
 *	Value is retrieved using the EmitterTime of the emitter.
 */
var(Velocity) rawdistributionvector	StartVelocity;
/** 
 *	The velocity to apply to a particle along its radial direction.
 *	Direction is determined by subtracting the location of the emitter from the particle location at spawn.
 *	Value is retrieved using the EmitterTime of the emitter.
 */
var(Velocity) rawdistributionfloat	StartVelocityRadial;

cpptext
{
	virtual void Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	/**
	 *	Extended version of spawn, allows for using a random stream for distribution value retrieval
	 *
	 *	@param	Owner				The particle emitter instance that is spawning
	 *	@param	Offset				The offset to the modules payload data
	 *	@param	SpawnTime			The time of the spawn
	 *	@param	InRandomStream		The random stream to use for retrieving random values
	 */
	void SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream);
}

defaultproperties
{
	bSpawnModule=true

	Begin Object Class=DistributionVectorUniform Name=DistributionStartVelocity
	End Object
	StartVelocity=(Distribution=DistributionStartVelocity)

	Begin Object Class=DistributionFloatUniform Name=DistributionStartVelocityRadial
	End Object
	StartVelocityRadial=(Distribution=DistributionStartVelocityRadial)
}
