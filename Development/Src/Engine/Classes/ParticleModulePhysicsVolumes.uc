/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModulePhysicsVolumes extends ParticleModuleWorldForcesBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** Scales the physics volume forces for this particular emitter. */
var() rawdistributionfloat GlobalInfluence;

enum EParticleLevelInfluenceType
{
	LIT_Never,
	LIT_OutsidePhysicsVolumes,
	LIT_Always
};

/** Determines if and when the level's global physics will impact the particles.
 *
 *  LIT_Never - Never use the level's global influence.
 *  LIT_OutsidePhysicsVolumes - Only use the level's global influence if the particle is outside all physics volumes.
 *  LIT_Always - Always use the level's global influence.
 */
var() EParticleLevelInfluenceType LevelInfluenceType;

cpptext
{
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);
}

defaultproperties
{
	bUpdateModule=true

	bSupported3DDrawMode=false

	Begin Object Class=DistributionFloatConstant Name=DistributionInfluence
		Constant=0.0f;
	End Object
	GlobalInfluence=(Distribution=DistributionInfluence)

	LevelInfluenceType=LIT_Never
}
