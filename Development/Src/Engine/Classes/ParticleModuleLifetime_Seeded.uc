/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleLifetime_Seeded extends ParticleModuleLifetime
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** The random seed(s) to use for looking up values in StartLocation */
var(RandomSeed) ParticleRandomSeedInfo		RandomSeedInfo;

cpptext
{
	/**
	 *	Called on a particle when it is spawned.
	 *
	 *	@param	Owner			The emitter instance that spawned the particle
	 *	@param	Offset			The payload data offset for this module
	 *	@param	SpawnTime		The spawn time of the particle
	 */
	virtual void Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	/**
	 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *
	 *	@return UINT		The number of bytes the module needs per emitter instance.
	 */
	virtual UINT	RequiredBytesPerInstance(FParticleEmitterInstance* Owner = NULL);
	/**
	 *	Allows the module to prep its 'per-instance' data block.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	InstData	Pointer to the data block for this module.
	 */
	virtual UINT	PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData);

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

	/**
	 *	Retrieve the random seed info for this module.
	 *
	 *	@return	FParticleRandomSeedInfo*	The random seed info; NULL if not supported
	 */
	virtual FParticleRandomSeedInfo* GetRandomSeedInfo()
	{
		return &RandomSeedInfo;
	}

	/** 
	 *	Called when an emitter instance is looping...
	 *
	 *	@param	Owner	The emitter instance that owns this module
	 */
	virtual void EmitterLoopingNotify(FParticleEmitterInstance* Owner);
}

defaultproperties
{
	bSpawnModule=true
	bSupportsRandomSeed=true
	bRequiresLoopingNotification=true
}
