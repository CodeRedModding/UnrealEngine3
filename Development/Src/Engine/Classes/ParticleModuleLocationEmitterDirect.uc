//=============================================================================
// ParticleModuleLocationEmitterDirect
//
// A location module that uses particles from another emitters particles as
// position for its particles.
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class ParticleModuleLocationEmitterDirect extends ParticleModuleLocationBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

//=============================================================================
// Variables
//=============================================================================
// LocationEmitter

/** The name of the emitter to use as a source for the location of the particles. */
var(Location) export		noclear	name			EmitterName;

//=============================================================================
// C++ functions
//=============================================================================
cpptext
{
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);
}

//=============================================================================
// Default properties
//=============================================================================
defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	EmitterName=None
}
