/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleOrbitBase extends ParticleModule
	native(Particle)
	editinlinenew
	hidecategories(Object)
	abstract;

/** 
 *	If TRUE, distribution values will be retrieved using the EmitterTime.
 *	If FALSE (default), they will be retrieved using the Particle.RelativeTime.
 */
var(Orbit)	bool	bUseEmitterTime;
