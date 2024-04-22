/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleLocation extends ParticleModuleLocationBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** 
 *	The location the particle should be emitted.
 *	Relative in local space to the emitter by default.
 *	Relative in world space as a WorldOffset module or when the emitter's UseLocalSpace is off.
 *	Retrieved using the EmitterTime at the spawn of the particle.
 */
var(Location) rawdistributionvector	StartLocation;

/**
 *  When set to a non-zero value this will force the particles to only spawn on evenly distributed
 *  positions between the two points specified.
 */
var(Location) float DistributeOverNPoints;

/**
 *  When DistributeOverNPoints is set to a non-zero value, this specifies the ratio of particles spawned
 *  that should use the distribution.  (For example setting this to 1 will cause all the particles to
 *  be distributed evenly whereas .75 would cause 1/4 of the particles to be randomly placed).
 */
var(Location) float DistributeThreshold;

cpptext
{
protected:
	/**
	 *	Extended version of spawn, allows for using a random stream for distribution value retrieval
	 *
	 *	@param	Owner				The particle emitter instance that is spawning
	 *	@param	Offset				The offset to the modules payload data
	 *	@param	SpawnTime			The time of the spawn
	 *	@param	InRandomStream		The random stream to use for retrieving random values
	 */
	virtual void SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream);

public:
	virtual void Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	virtual void Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI);
}

defaultproperties
{
	bSpawnModule=true

	Begin Object Class=DistributionVectorUniform Name=DistributionStartLocation
	End Object
	StartLocation=(Distribution=DistributionStartLocation)

	bSupported3DDrawMode=true
	DistributeOverNPoints=0.0
}

