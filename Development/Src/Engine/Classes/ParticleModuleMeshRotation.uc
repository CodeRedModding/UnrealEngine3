/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleMeshRotation extends ParticleModuleRotationBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 *	Initial rotation in ROTATIONS PER SECOND (1 = 360 degrees).
 *	The value is retrieved using the EmitterTime.
 */
var(Rotation)	rawdistributionvector	StartRotation;

/** If TRUE, apply the parents rotation as well. */
var(Rotation)	bool					bInheritParent;

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
	 *	Return TRUE if this module impacts rotation of Mesh emitters
	 *	@return	UBOOL		TRUE if the module impacts mesh emitter rotation
	 */
	virtual UBOOL	TouchesMeshRotation() const	{ return TRUE; }
}

defaultproperties
{
	bSpawnModule=true

	Begin Object Class=DistributionVectorUniform Name=DistributionStartRotation
		Min=(X=0.0,Y=0.0,Z=0.0)
		Max=(X=1.0,Y=1.0,Z=1.0)
	End Object
	StartRotation=(Distribution=DistributionStartRotation)
	
	bInheritParent=false
}
