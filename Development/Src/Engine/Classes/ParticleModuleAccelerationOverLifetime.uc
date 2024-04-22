/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleAccelerationOverLifetime extends ParticleModuleAccelerationBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** 
 *	The acceleration of the particle over its lifetime.
 *	Value is obtained using the RelativeTime of the partice.
 *	The current and base velocity values of the particle 
 *	are then updated using the formula 
 *		velocity += acceleration* DeltaTime
 *	where DeltaTime is the time passed since the last frame.
 */
var(Acceleration) rawdistributionvector	AccelOverLife;

cpptext
{
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);
}

defaultproperties
{
	bSpawnModule=false
	bUpdateModule=true

	Begin Object Class=DistributionVectorConstantCurve Name=DistributionAccelOverLife
	End Object
	AccelOverLife=(Distribution=DistributionAccelOverLife)
}
