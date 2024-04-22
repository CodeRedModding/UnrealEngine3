/**
 *	ParticleModuleUberLTISIVCLIL
 *
 *	Uber-module replacing the following classes:
 *		LT - Lifetime
 *		IS - Initial Size
 *		IV - Initial Velocity
 *		CL - Color over Life
 *      IL - Initial Location
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class ParticleModuleUberLTISIVCLIL extends ParticleModuleUberBase
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

	// RequiredModules
 	RequiredModules(0)=ParticleModuleLifetime
	RequiredModules(1)=ParticleModuleSize
	RequiredModules(2)=ParticleModuleVelocity
	RequiredModules(3)=ParticleModuleColorOverLife
	RequiredModules(4)=ParticleModuleLocation
}
