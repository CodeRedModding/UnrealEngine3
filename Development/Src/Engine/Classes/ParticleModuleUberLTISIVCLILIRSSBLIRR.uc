/**
 *	ParticleModuleUberLTISIVCLILIRSSBLIRR
 *
 *	Uber-module replacing the following classes:
 *		LT   - Lifetime
 *		IS   - Initial Size
 *		IV   - Initial Velocity
 *		CL   - Color over Life
 *      IL   - Initial Location
 *		IR   - Initial Rotation
 *		SSBL - Scale Size By Life
 *		IRR  - Initial Rotation Rate
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class ParticleModuleUberLTISIVCLILIRSSBLIRR extends ParticleModuleUberBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

//------------------------------------------------------------------------------------------------
// Members
//------------------------------------------------------------------------------------------------
/** Lifetime - Gives the lifetime of the particles												*/
var(Lifetime)	export noclear		rawdistributionfloat		Lifetime;	

/** Size - Gives the size of the particles														*/
var(Size)		export noclear		rawdistributionvector		StartSize;

/** StartVelociy - Gives the start velocity of the particles									*/
var(Velocity)	export noclear		rawdistributionvector		StartVelocity;
/** StartRadialVelociy - Gives the start radial velocity of the particles						*/
var(Velocity)	export noclear		rawdistributionfloat		StartVelocityRadial;

/** ColorOverLife - Gives the color to apply to the particles									*/
var(Color)		export noclear		rawdistributionvector		ColorOverLife;
/** AlphaOverLife - Gives the alpha to apply to the particles									*/
var(Color)		export noclear		rawdistributionfloat		AlphaOverLife;

/** StartLocation - Gives the start location of particles										*/
var(Location)	export noclear		rawdistributionvector		StartLocation;

/** StartRotation - Gives the rotation of particles in turns (1 = 360 degrees)					*/
var(Rotation)	export noclear		rawdistributionfloat		StartRotation;

/** SizeLifeMultiplier - Size scale factor														*/
var(Size)		export noclear		rawdistributionvector		SizeLifeMultiplier;
/** MultiplyX - If TRUE, scale along the X size axis											*/
var(Size)							bool					SizeMultiplyX;
/** MultiplyY - If TRUE, scale along the Y size axis											*/
var(Size)							bool					SizeMultiplyY;
/** MultiplyZ - If TRUE, scale along the Z size axis											*/
var(Size)							bool					SizeMultiplyZ;

/** StartRotationRate - Gives the rotation rate of particles in turns per second				*/
var(Rotation)	export noclear		rawdistributionfloat		StartRotationRate;

//------------------------------------------------------------------------------------------------
// C++ Text
//------------------------------------------------------------------------------------------------
cpptext
{
	/** Copy the contents of the modules to the UberModule								*/
	virtual	UBOOL				ConvertToUberModule(UParticleEmitter* InputEmitter);

	/** Spawn - called when spawning particles											*/
	virtual void				Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	/** Update - called when updating particles											*/
	virtual void				Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);
}

//------------------------------------------------------------------------------------------------
// Default Properties
//------------------------------------------------------------------------------------------------
defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	// Lifetime
	Begin Object Class=DistributionFloatUniform Name=DistributionLifetime
		Min=1
		Max=1
	End Object
	Lifetime=(Distribution=DistributionLifetime)
	
	// Size
	Begin Object Class=DistributionVectorUniform Name=DistributionStartSize
		Min=(X=1,Y=1,Z=1)
		Max=(X=1,Y=1,Z=1)
	End Object
	StartSize=(Distribution=DistributionStartSize)
	
	// Velocity
	Begin Object Class=DistributionVectorUniform Name=DistributionStartVelocity
		Min=(X=0,Y=0,Z=0)
		Max=(X=0,Y=0,Z=10)
	End Object
	StartVelocity=(Distribution=DistributionStartVelocity)
	
	Begin Object Class=DistributionFloatUniform Name=DistributionStartVelocityRadial
	End Object
	StartVelocityRadial=(Distribution=DistributionStartVelocityRadial)
	
	// ColorOverLife
	Begin Object Class=DistributionVectorConstantCurve Name=DistributionColorOverLife
	End Object
	ColorOverLife=(Distribution=DistributionColorOverLife)

	Begin Object Class=DistributionFloatConstant Name=DistributionAlphaOverLife
		Constant=255.9f;
	End Object
	AlphaOverLife=(Distribution=DistributionAlphaOverLife)

	// Location
	Begin Object Class=DistributionVectorUniform Name=DistributionStartLocation
	End Object
	StartLocation=(Distribution=DistributionStartLocation)

	// Rotation
	Begin Object Class=DistributionFloatUniform Name=DistributionStartRotation
		Min=0.0
		Max=1.0
	End Object
	StartRotation=(Distribution=DistributionStartRotation)

	// SizeMultipleLife
	SizeMultiplyX=true
	SizeMultiplyY=true
	SizeMultiplyZ=true

	Begin Object Class=DistributionVectorConstant Name=DistributionLifeMultiplier
		Constant=(X=1,Y=1,Z=1)
	End Object
	SizeLifeMultiplier=(Distribution=DistributionLifeMultiplier)
	
	// RotationRate
	Begin Object Class=DistributionFloatConstant Name=DistributionStartRotationRate
	End Object
	StartRotationRate=(Distribution=DistributionStartRotationRate)

	// RequiredModules
 	RequiredModules(0)=ParticleModuleLifetime
	RequiredModules(1)=ParticleModuleSize
	RequiredModules(2)=ParticleModuleVelocity
	RequiredModules(3)=ParticleModuleColorOverLife
	RequiredModules(4)=ParticleModuleLocation
	RequiredModules(5)=ParticleModuleRotation
	RequiredModules(6)=ParticleModuleSizeMultiplyLife
	RequiredModules(7)=ParticleModuleRotationRate
}
