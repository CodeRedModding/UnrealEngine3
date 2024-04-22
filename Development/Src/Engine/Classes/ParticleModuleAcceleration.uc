/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleAcceleration extends ParticleModuleAccelerationBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 *	The initial acceleration of the particle.
 *	Value is obtained using the EmitterTime at particle spawn.
 *	Each frame, the current and base velocity of the particle 
 *	is then updated using the formula 
 *		velocity += acceleration * DeltaTime
 *	where DeltaTime is the time passed since the last frame.
 */
var(Acceleration) rawdistributionvector	Acceleration;

/**
 *	If true, then apply the particle system components scale 
 *	to the acceleration value.
 */
var(Acceleration) bool bApplyOwnerScale;

cpptext
{
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);
	virtual UINT	RequiredBytes(FParticleEmitterInstance* Owner = NULL);
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	Begin Object Class=DistributionVectorUniform Name=DistributionAcceleration
	End Object
	Acceleration=(Distribution=DistributionAcceleration)
}
