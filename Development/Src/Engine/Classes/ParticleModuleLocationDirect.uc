/**
 *	ParticleModuleLocationDirect
 *
 *	Sets the location of particles directly.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class ParticleModuleLocationDirect extends ParticleModuleLocationBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** 
 *	The location of the particle at a give time. Retrieved using the particle RelativeTime. 
 *	IMPORTANT: the particle location is set to this value, thereby over-writing any previous module impacts.
 */
var(Location) rawdistributionvector	Location;
/**
 *	An offset to apply to the position retrieved from the Location calculation. 
 *	The offset is retrieved using the EmitterTime. 
 *	The offset will remain constant over the life of the particle.
 */
var(Location) rawdistributionvector	LocationOffset;
/**
 *	Scales the velocity of the object at a given point in the time-line.
 */
var(Location) rawdistributionvector	ScaleFactor;
/** Currently unused. */
var(Location) rawdistributionvector	Direction;

cpptext
{
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);
	virtual UINT	RequiredBytes(FParticleEmitterInstance* Owner = NULL);
	virtual void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	Begin Object Class=DistributionVectorUniform Name=DistributionLocation
	End Object
	Location=(Distribution=DistributionLocation)

	Begin Object Class=DistributionVectorConstant Name=DistributionLocationOffset
		Constant=(X=0,Y=0,Z=0)
	End Object
	LocationOffset=(Distribution=DistributionLocationOffset)

	Begin Object Class=DistributionVectorConstant Name=DistributionScaleFactor
		Constant=(X=1,Y=1,Z=1)
	End Object
	ScaleFactor=(Distribution=DistributionScaleFactor)

	Begin Object Class=DistributionVectorUniform Name=DistributionDirection
	End Object
	Direction=(Distribution=DistributionDirection)
}
