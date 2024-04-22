//=============================================================================
// ParticleModuleLocationEmitter
//
// A location module that uses particles from another emitters particles as
// spawn points for its particles.
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class ParticleModuleLocationEmitter extends ParticleModuleLocationBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

//=============================================================================
// Variables
//=============================================================================
// LocationEmitter

/** The name of the emitter to use that the source location for particle. */
var(Location)						export		noclear	name					EmitterName;

enum ELocationEmitterSelectionMethod
{
	ELESM_Random,
	ELESM_Sequential
};
/** 
 *	The method to use when selecting a spawn target particle from the emitter.
 *	Can be one of the following:
 *		ELESM_Random		Randomly select a particle from the source emitter.
 *		ELESM_Sequential	Step through each particle from the source emitter in order.
 */
var(Location)	ELocationEmitterSelectionMethod									SelectionMethod;

/** If TRUE, the spawned particle should inherit the velocity of the source particle. */
var(Location)	bool															InheritSourceVelocity;
/** Amount to scale the source velocity by when inheriting it. */
var(Location)	float															InheritSourceVelocityScale;
/** If TRUE, the spawned particle should inherit the rotation of the source particle. */
var(Location)	bool															bInheritSourceRotation;
/** Amount to scale the source rotation by when inheriting it. */
var(Location)	float															InheritSourceRotationScale;

//=============================================================================
// C++ functions
//=============================================================================
cpptext
{
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	virtual UINT	RequiredBytesPerInstance(FParticleEmitterInstance* Owner = NULL);
}

//=============================================================================
// Script functions
//=============================================================================

//=============================================================================
// Default properties
//=============================================================================
defaultproperties
{
	bSpawnModule=true

	SelectionMethod=ELESM_Random
	EmitterName=None
	InheritSourceVelocity=false
	InheritSourceVelocityScale=1.0
	bInheritSourceRotation=false
	InheritSourceRotationScale=1.0
}
