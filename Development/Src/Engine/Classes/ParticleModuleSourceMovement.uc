/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleSourceMovement extends ParticleModuleLocationBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** 
 *	The scale factor to apply to the source movement before adding to the particle location.
 *	The value is looked up using the particles RELATIVE time [0..1].
 */
var(SourceMovement) rawdistributionvector	SourceMovementScale;

cpptext
{
	/**
	 *	Called on an emitter when all other update operations have taken place
	 *	INCLUDING bounding box calculations!
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	DeltaTime	The time since the last update.
	 */
	virtual void	FinalUpdate(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);
}

defaultproperties
{
	bFinalUpdateModule=true

	Begin Object Class=DistributionVectorConstant Name=DistributionSourceMovementScale
		Constant=(X=1.0,Y=1.0,Z=1.0)
	End Object
	SourceMovementScale=(Distribution=DistributionSourceMovementScale)
}
