/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleRotationOverLifetime extends ParticleModuleRotationBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** 
 *	The rotation of the particle (1.0 = 360 degrees).
 *	The value is retrieved using the RelativeTime of the particle.
 */
var(Rotation) rawdistributionfloat	RotationOverLife;

/**
 *	If TRUE,  the particle rotation is multiplied by the value retrieved from RotationOverLife.
 *	If FALSE, the particle rotation is incremented by the value retrieved from RotationOverLife.
 */
var(Rotation)					bool				Scale;

cpptext
{
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);
}

defaultproperties
{
	bSpawnModule=false
	bUpdateModule=true

	Begin Object Class=DistributionFloatConstantCurve Name=DistributionRotOverLife
	End Object
	RotationOverLife=(Distribution=DistributionRotOverLife)
	
	// Setting to true to support existing modules...
	Scale=true
}
