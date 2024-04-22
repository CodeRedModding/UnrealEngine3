/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleCollisionBase extends ParticleModule
	native(Particle)
	editinlinenew
	hidecategories(Object)
	abstract;

/**
 *	Flags indicating what to do with the particle when MaxCollisions is reached
 */
enum EParticleCollisionComplete
{
	/**	Kill the particle when MaxCollisions is reached		*/
	EPCC_Kill,
	/**	Freeze the particle in place						*/
	EPCC_Freeze,
	/**	Stop collision checks, but keep updating			*/
	EPCC_HaltCollisions,
	/**	Stop translations of the particle					*/
	EPCC_FreezeTranslation,
	/**	Stop rotations of the particle						*/
	EPCC_FreezeRotation,
	/**	Stop all movement of the particle					*/
	EPCC_FreezeMovement
};

