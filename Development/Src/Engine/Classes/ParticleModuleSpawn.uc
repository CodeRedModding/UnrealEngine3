/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleSpawn extends ParticleModuleSpawnBase
	native(Particle)
	editinlinenew
	hidecategories(Object)
	hidecategories(ParticleModuleSpawnBase);

/** The rate at which to spawn particles. */
var(Spawn)						rawdistributionfloat	Rate;

/** The scalar to apply to the rate. */
var(Spawn)						rawdistributionfloat	RateScale;

/** The method to utilize when burst-emitting particles. */
var(Burst)						EParticleBurstMethod	ParticleBurstMethod;

/** The array of burst entries. */
var(Burst)	export noclear		array<ParticleBurst>	BurstList;

cpptext
{
	virtual void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/**
	 *	Retrieve the spawn amount this module is contributing.
	 *	Note that if multiple Spawn-specific modules are present, if any one
	 *	of them ignores the SpawnRate processing it will be ignored.
	 *
	 *	@param	Owner		The particle emitter instance that is spawning.
	 *	@param	Offset		The offset into the particle payload for the module.
	 *	@param	OldLeftover	The bit of timeslice left over from the previous frame.
	 *	@param	DeltaTime	The time that has expired since the last frame.
	 *	@param	Number		The number of particles to spawn. (OUTPUT)
	 *	@param	Rate		The spawn rate of the module. (OUTPUT)
	 *
	 *	@return	UBOOL		FALSE if the SpawnRate should be ignored.
	 *						TRUE if the SpawnRate should still be processed.
	 */
	virtual UBOOL GetSpawnAmount(FParticleEmitterInstance* Owner, INT Offset, FLOAT OldLeftover, 
		FLOAT DeltaTime, INT& Number, FLOAT& Rate);

	virtual UBOOL	GenerateLODModuleValues(UParticleModule* SourceModule, FLOAT Percentage, UParticleLODLevel* LODLevel);

	/**
	 *	Retrieve the maximum spawn rate for this module...
	 *	Used in estimating the number of particles that could be used.
	 *
	 *	@return	FLOAT			The maximum spawn rate
	 */
	virtual FLOAT GetMaximumSpawnRate();

	/**
	 *	Retrieve the estimated spawn rate for this module...
	 *	Used in estimating the number of particles that could be used.
	 *
	 *	@return	FLOAT			The maximum spawn rate
	 */
	virtual FLOAT GetEstimatedSpawnRate();

	/**
	 *	Retrieve the maximum number of particles this module could burst.
	 *	Used in estimating the number of particles that could be used.
	 *
	 *	@return	INT			The maximum burst count
	 */
	virtual INT GetMaximumBurstCount();
}

defaultproperties
{
	bProcessSpawnRate=true

	Begin Object Class=DistributionFloatConstant Name=RequiredDistributionSpawnRate
		Constant=20.0
	End Object
	Rate=(Distribution=RequiredDistributionSpawnRate)

	Begin Object Class=DistributionFloatConstant Name=RequiredDistributionSpawnRateScale
		Constant=1.0
	End Object
	RateScale=(Distribution=RequiredDistributionSpawnRateScale)

	LODDuplicate=false
}
