/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleCameraOffset extends ParticleModuleCameraBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** 
 *	The camera-relative offset to apply to sprite location
 */
var(Camera)	rawdistributionfloat	CameraOffset;

/** If TRUE, the offset will only be processed at spawn time */
var(Camera) bool					bSpawnTimeOnly;

/**
 *	The update method for the offset
 */
enum EParticleCameraOffsetUpdateMethod
{
	EPCOUM_DirectSet,
	EPCOUM_Additive,
	EPCOUM_Scalar
};

/**
 *	How to update the offset for this module.
 *    DirectSet - Set the value directly (overwrite any previous setting)
 *    Additive  - Add the offset of this module to the existing offset
 *    Scalar    - Scale the existing offset by the value of this module
 */
var(Camera) EParticleCameraOffsetUpdateMethod	UpdateMethod;

cpptext
{
	/**
	 *	Called on a particle that is freshly spawned by the emitter.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	SpawnTime	The time of the spawn.
	 */
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);

	/**
	 *	Called on a particle that is being updated by its emitter.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	DeltaTime	The time since the last update.
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
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	Begin Object Class=DistributionFloatConstant Name=DistributionCameraOffset
		Constant=1.0
	End Object
	CameraOffset=(Distribution=DistributionCameraOffset)

	bSpawnTimeOnly=false
	UpdateMethod=EPCOUM_DirectSet
}
