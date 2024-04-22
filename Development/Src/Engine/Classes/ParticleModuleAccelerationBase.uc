/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleAccelerationBase extends ParticleModule
	native(Particle)
	editinlinenew
	hidecategories(Object)
	abstract;

/**
 *	If true, then treat the acceleration as world-space
 */
var(Acceleration) bool bAlwaysInWorldSpace;

cpptext
{
	/**
	 *	Called when the module is created, this function allows for setting values that make
	 *	sense for the type of emitter they are being used in.
	 *
	 *	@param	Owner			The UParticleEmitter that the module is being added to.
	 */
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner);
}
