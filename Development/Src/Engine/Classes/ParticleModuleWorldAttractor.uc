/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleWorldAttractor extends ParticleModuleWorldForcesBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** When TRUE the attraction force is particle life relative, when FALSE it is emitter life relative. */
var() bool bParticleLifeRelative;

/** Scales the attraction forces for this particular emitter. */
var() rawdistributionfloat AttractorInfluence;

cpptext
{
	/**
	 *	Called on a particle that is being updated by its emitter.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	DeltaTime	The time since the last update.
	 */
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);
}

defaultproperties
{
	bUpdateModule=true

	bSupported3DDrawMode=false

	bParticleLifeRelative=false

	Begin Object Class=DistributionFloatConstant Name=DistributionInfluence
		Constant=1.0f;
	End Object
	AttractorInfluence=(Distribution=DistributionInfluence)
}
