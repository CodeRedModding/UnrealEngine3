/**
 * With this you can store the exact time a particle system was spawned.  This is useful for additional effects based off the 
 * time when the parent effect's particles were spawned.  You need to the spawn time because the RelativeTime is for when that
 * specific particle system will die.  Due to having random durations you are not guarenteed for that value to represent the order in
 * which the individual particles were spawned.
 * 
 * 
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleStoreSpawnTime extends ParticleModuleStoreSpawnTimeBase
	native(Particle)
	editinlinenew
	hidecategories(Object);


cpptext
{
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);

	/**
	 *	Returns the number of bytes that the module requires in the particle payload block.
	 *
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *
	 *	@return	UINT		The number of bytes the module needs per particle.
	 */
	virtual UINT	RequiredBytes(FParticleEmitterInstance* Owner = NULL);

}

defaultproperties
{
}
