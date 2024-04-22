/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleMeshRotationRateOverLife extends ParticleModuleRotationRateBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 *	The rotation rate desired.
 *	The value is retrieved using the RelativeTime of the particle.
 */
var(Rotation) rawdistributionvector	RotRate;

/**
 *	If TRUE, scale the current rotation rate by the value retrieved.
 *	Otherwise, set the rotation rate to the value retrieved.
 */
var(Rotation) bool					bScaleRotRate;

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
	 *	Called when the module is created, this function allows for setting values that make
	 *	sense for the type of emitter they are being used in.
	 *
	 *	@param	Owner			The UParticleEmitter that the module is being added to.
	 */
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner);

	/**
	 *	Return TRUE if this module impacts rotation of Mesh emitters
	 *	@return	UBOOL		TRUE if the module impacts mesh emitter rotation
	 */
	virtual UBOOL	TouchesMeshRotation() const	{ return TRUE; }
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	Begin Object Class=DistributionVectorConstantCurve Name=DistributionRotRate
	End Object
	RotRate=(Distribution=DistributionRotRate)
}
