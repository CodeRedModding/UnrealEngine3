/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleSpawnBase extends ParticleModule
	native(Particle)
	editinlinenew
	hidecategories(Object)
	abstract;

/** 
 *	If TRUE, the SpawnRate of the SpawnModule of the emitter will be processed.
 *	If mutliple Spawn modules are 'stacked' in an emitter, if ANY of them 
 *	have this set to FALSE, it will not process the SpawnModule SpawnRate.
 */
var(Spawn)	bool				bProcessSpawnRate;

/** 
 *	If TRUE, the BurstList of the SpawnModule of the emitter will be processed.
 *	If mutliple Spawn modules are 'stacked' in an emitter, if ANY of them 
 *	have this set to FALSE, it will not process the SpawnModule BurstList.
 */
var(Burst)	bool				bProcessBurstList;

cpptext
{
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
		FLOAT DeltaTime, INT& Number, FLOAT& Rate)
	{
		return bProcessSpawnRate;
	}
	
	/**
	 *	Retrieve the burst count this module is contributing.
	 *	Note that if multiple Spawn-specific modules are present, if any one
	 *	of them ignores the default BurstList, it will be ignored.
	 *
	 *	@param	Owner		The particle emitter instance that is spawning.
	 *	@param	Offset		The offset into the particle payload for the module.
	 *	@param	OldLeftover	The bit of timeslice left over from the previous frame.
	 *	@param	DeltaTime	The time that has expired since the last frame.
	 *	@param	Number		The number of particles to burst. (OUTPUT)
	 *
	 *	@return	UBOOL		FALSE if the default BurstList should be ignored.
	 *						TRUE if the default BurstList should still be processed.
	 */
	virtual UBOOL GetBurstCount(FParticleEmitterInstance* Owner, INT Offset, FLOAT OldLeftover, 
		FLOAT DeltaTime, INT& Number)
	{
		Number = 0;
		return bProcessBurstList;
	}

	/**
	 *	Retrieve the maximum spawn rate for this module...
	 *	Used in estimating the number of particles that could be used.
	 *
	 *	@return	FLOAT		The maximum spawn rate
	 */
	virtual FLOAT GetMaximumSpawnRate() { return 0.0f; }

	/**
	 *	Retrieve the estimated spawn rate for this module...
	 *	Used in estimating the number of particles that could be used.
	 *
	 *	@return	FLOAT			The maximum spawn rate
	 */
	virtual FLOAT GetEstimatedSpawnRate() { return 0.0f; }

	/**
	 *	Retrieve the maximum number of particles this module could burst.
	 *	Used in estimating the number of particles that could be used.
	 *
	 *	@return	INT			The maximum burst count
	 */
	virtual INT GetMaximumBurstCount() { return 0; }

	/**
	 *	Retrieve the ModuleType of this module.
	 *
	 *	@return	EModuleType		The type of module this is.
	 */
	virtual EModuleType	GetModuleType() const	{	return EPMT_Spawn;	}
}

defaultproperties
{
	bProcessSpawnRate=true
	bProcessBurstList=true
}
