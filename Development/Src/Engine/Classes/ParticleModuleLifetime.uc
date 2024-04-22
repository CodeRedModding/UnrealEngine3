/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleLifetime extends ParticleModuleLifetimeBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** The lifetime of the particle, in seconds. Retrieved using the EmitterTime at the spawn of the particle. */
var(Lifetime) rawdistributionfloat	Lifetime;

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

	/**
	 *	Called when the module is created, this function allows for setting values that make
	 *	sense for the type of emitter they are being used in.
	 *
	 *	@param	Owner			The UParticleEmitter that the module is being added to.
	 */
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner);

	virtual FLOAT	GetMaxLifetime();

	/**
	 *	Call to retrieve the lifetime value at the given time.
	 *
	 *	@param	Owner		The emitter instance that owns this module
	 *	@param	InTime		The time input for retrieving the lifetime value
	 *	@param	Data		The data associated with the distribution
	 *
	 *	@return	FLOAT		The Lifetime value
	 */
	virtual FLOAT	GetLifetimeValue(FParticleEmitterInstance* Owner, FLOAT InTime, UObject* Data = NULL);
}

defaultproperties
{
	bSpawnModule=true

	Begin Object Class=DistributionFloatUniform Name=DistributionLifetime
	End Object
	Lifetime=(Distribution=DistributionLifetime)
}
