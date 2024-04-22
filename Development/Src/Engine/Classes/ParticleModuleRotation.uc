/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleRotation extends ParticleModuleRotationBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 *	Initial rotation of the particle (1 = 360 degrees).
 *	The value is retrieved using the EmitterTime.
 */
var(Rotation) rawdistributionfloat	StartRotation;

cpptext
{
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
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

	Begin Object Class=DistributionFloatUniform Name=DistributionStartRotation
		Min=0.0
		Max=1.0
	End Object
	StartRotation=(Distribution=DistributionStartRotation)
}

