/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleVelocityBase extends ParticleModule
	native(Particle)
	editinlinenew
	hidecategories(Object)
	abstract;

/**
 *	If true, then treat the velocity as world-space defined.
 *	NOTE: LocalSpace emitters that are moving will see strange results...
 */
var(Velocity) bool bInWorldSpace;

/** If true, then apply the particle system components scale to the velocity value. */
var(Velocity) bool bApplyOwnerScale;
