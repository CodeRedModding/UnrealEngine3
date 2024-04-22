//=============================================================================
// ParticleModuleLocationPrimitiveBase
// Base class for setting particle spawn locations based on primitives.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class ParticleModuleLocationPrimitiveBase extends ParticleModuleLocationBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** If TRUE, the positive X axis is valid for spawning. */
var(Location) bool					Positive_X;
/** If TRUE, the positive Y axis is valid for spawning. */
var(Location) bool					Positive_Y;
/** If TRUE, the positive Z axis is valid for spawning. */
var(Location) bool					Positive_Z;
/** If TRUE, the negative X axis is valid for spawning. */
var(Location) bool					Negative_X;
/** If TRUE, the negative Y axis is valid for spawning. */
var(Location) bool					Negative_Y;
/** If TRUE, the negative Zaxis is valid for spawning. */
var(Location) bool					Negative_Z;
/** If TRUE, particles will only spawn on the surface of the primitive. */
var(Location) bool					SurfaceOnly;
/** If TRUE, the particle should get its velocity from the position within the primitive. */
var(Location) bool					Velocity;
/** The scale applied to the velocity. (Only used if 'Velocity' is checked). */
var(Location) rawdistributionfloat	VelocityScale;
/** The location of the bounding primitive relative to the position of the emitter. */
var(Location) rawdistributionvector	StartLocation;

cpptext
{
	virtual void	DetermineUnitDirection(FParticleEmitterInstance* Owner, FVector& vUnitDir, class FRandomStream* InRandomStream);
}

defaultproperties
{
	bSpawnModule=true

	Positive_X=true
	Positive_Y=true
	Positive_Z=true
	Negative_X=true
	Negative_Y=true
	Negative_Z=true

	SurfaceOnly=false
	Velocity=false

	Begin Object Class=DistributionFloatConstant Name=DistributionVelocityScale
		Constant=1
	End Object
	VelocityScale=(Distribution=DistributionVelocityScale)

	Begin Object Class=DistributionVectorConstant Name=DistributionStartLocation
		Constant=(X=0,Y=0,Z=0)
	End Object
	StartLocation=(Distribution=DistributionStartLocation)
}
