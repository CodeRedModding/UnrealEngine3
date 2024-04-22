/**
 *	ParticleModuleTrailSpawn
 *	The trail spawn module.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleTrailSpawn extends ParticleModuleTrailBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

//*************************************************************************************************
// Trail Spawning Variables
//*************************************************************************************************
enum ETrail2SpawnMethod
{
	/** Use the emitter spawn settings									*/
	PET2SM_Emitter,
	/** Spawn based on the velocity of the source						*/
	PET2SM_Velocity,
	/** Spawn base on the distanced covered by the source				*/
	PET2SM_Distance
};

/** 
 *	This parameter will map a given distance range [MinInput..MaxInput]
 *	to the given spawn values [MinOutput..MaxOutput]
 *	Anything below the MinOutput will result in no particles being spawned
 *	NOTE: The distance travelled is accumulated. If it takes 10 frames to travel the min.
 *	distance, then MinOutput particles will be spawned every 10 frames...
 *	IMPORTANT! This type must be a floatparticleparam type, but nothing is forcing it now!
 */
var(Spawn)	export noclear		distributionfloatparticleparameter	SpawnDistanceMap;

/** 
 *	The minimum velocity the source must be travelling at in order to spawn particles.
 */
var(Spawn)						float								MinSpawnVelocity;

//*************************************************************************************************
// C++ Text
//*************************************************************************************************
cpptext
{
	virtual void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void	PostLoad();

	// Trail
//	virtual void	GetDataPointers(FParticleEmitterInstance* Owner, const BYTE* ParticleBase, 
//			INT& CurrentOffset, FBeam2TypeDataPayload*& BeamData, FVector*& InterpolatedPoints, 
//			FLOAT*& NoiseRate, FVector*& TargetNoisePoints, FVector*& CurrentNoisePoints, 
//			FLOAT*& TaperValues);

			UINT	GetSpawnCount(FParticleTrail2EmitterInstance* TrailInst, FLOAT DeltaTime);
}

//*************************************************************************************************
// Default properties
//*************************************************************************************************
defaultproperties
{	
	Begin Object Class=DistributionFloatParticleParameter Name=DistributionSpawnDistanceMap
		ParameterName="None"
		MinInput=10.0
		MaxInput=100.0
		MinOutput=1.0
		MaxOutput=5.0
		Constant=1.0
	End Object
	SpawnDistanceMap=DistributionSpawnDistanceMap
	
	MinSpawnVelocity=0.0
}
