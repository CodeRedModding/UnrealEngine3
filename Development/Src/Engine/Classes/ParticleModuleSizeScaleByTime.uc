/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleSizeScaleByTime extends ParticleModuleSizeBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 *	The amount the size should be scaled before being used as the size of the particle. 
 *	The value is retrieved using the ABSOLUTE lifetime of the particle during its update.
 */
var()					rawdistributionvector	SizeScaleByTime;
/** If TRUE, scale the X-component of the size. */
var()					bool					bEnableX;
/** If TRUE, scale the Y-component of the size. */
var()					bool					bEnableY;
/** If TRUE, scale the Z-component of the size. */
var()					bool					bEnableZ;

cpptext
{
	/**
	 *	Called during the spawning of a particle.
	 *	
	 *	@param	Owner		The emitter instance that owns the particle.
	 *	@param	Offset		The offset into the particle payload for this module.
	 *	@param	SpawnTime	The spawn time of the particle.
	 */
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);

	/**
	 *	Called during the spawning of particles in the emitter instance.
	 *	
	 *	@param	Owner		The emitter instance that owns the particle.
	 *	@param	Offset		The offset into the particle payload for this module.
	 *	@param	DeltaTime	The time slice for this update.
	 */
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);
	
	/**
	 *	Returns the number of bytes that the module requires in the particle payload block.
	 *
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *
	 *	@return	UINT		The number of bytes the module needs per particle.
	 */
	virtual UINT	RequiredBytes(FParticleEmitterInstance* Owner = NULL);

	/**
	 *	Called when the module is created, this function allows for setting values that make
	 *	sense for the type of emitter they are being used in.
	 *
	 *	@param	Owner			The UParticleEmitter that the module is being added to.
	 */
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner);
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	bEnableX=true
	bEnableY=true
	bEnableZ=true

	Begin Object Class=DistributionVectorConstantCurve Name=DistributionSizeScaleByTime
	End Object
	SizeScaleByTime=(Distribution=DistributionSizeScaleByTime)
}
