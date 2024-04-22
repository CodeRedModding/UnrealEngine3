/**
 *	ParticleModuleUberRainSplashB
 *
 *	Uber-module replacing the following classes:
 *		LT  - Lifetime
 *		IS  - Initial Size
 *		COL - Color Over Life
 *		SBL	- Size By Life
 *		IRR	- Initial Rotation Rate
 *
 *	Intended for use in the Rain particle system.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class ParticleModuleUberRainSplashB extends ParticleModuleUberBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

//*-----------------------------------------------------------------------------*/
/** Lifetime Module Members														*/
//*-----------------------------------------------------------------------------*/
var(Lifetime)	rawdistributionfloat		Lifetime;

//*-----------------------------------------------------------------------------*/
/** Size Module Members															*/
//*-----------------------------------------------------------------------------*/
var(Size)		rawdistributionvector		StartSize;

//*-----------------------------------------------------------------------------*/
/** ColorOverLife Module Members												*/
//*-----------------------------------------------------------------------------*/
var(Color)		rawdistributionvector		ColorOverLife;
var(Color)		rawdistributionfloat		AlphaOverLife;

//*-----------------------------------------------------------------------------*/
/** SizeByLife Module Members													*/
//*-----------------------------------------------------------------------------*/
var(Size)		rawdistributionvector		LifeMultiplier;
var(Size)		bool						MultiplyX;
var(Size)		bool						MultiplyY;
var(Size)		bool						MultiplyZ;

//*-----------------------------------------------------------------------------*/
/** Initial RotationRate Module Members											*/
//*-----------------------------------------------------------------------------*/
var(Rotation)	rawdistributionfloat		StartRotationRate;

//*-----------------------------------------------------------------------------*/
//*-----------------------------------------------------------------------------*/
cpptext
{
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);

	/** Used by derived classes to indicate they could be used on the given emitter.	*/
	virtual	UBOOL	IsCompatible(UParticleEmitter* InputEmitter);
	
	/** Copy the contents of the modules to the UberModule								*/
	virtual	UBOOL	ConvertToUberModule(UParticleEmitter* InputEmitter);
}

//*-----------------------------------------------------------------------------*/
//*-----------------------------------------------------------------------------*/
defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true
	bSupported3DDrawMode=true

	//*-----------------------------------------------------------------------------*/
	/** Lifetime Module Defaults													*/
	//*-----------------------------------------------------------------------------*/
	Begin Object Class=DistributionFloatUniform Name=DistributionLifetime
	End Object
	Lifetime=(Distribution=DistributionLifetime)

	//*-----------------------------------------------------------------------------*/
	/** Size Module Defaults														*/
	//*-----------------------------------------------------------------------------*/
	Begin Object Class=DistributionVectorUniform Name=DistributionStartSize
		Min=(X=1,Y=1,Z=1)
		Max=(X=1,Y=1,Z=1)
	End Object
	StartSize=(Distribution=DistributionStartSize)

	//*-----------------------------------------------------------------------------*/
	/** ColorOverLife Module Defaults												*/
	//*-----------------------------------------------------------------------------*/
	// This will screw up all the other curves...
	//bCurvesAsColor=true

	Begin Object Class=DistributionVectorConstantCurve Name=DistributionColorOverLife
	End Object
	ColorOverLife=(Distribution=DistributionColorOverLife)

	Begin Object Class=DistributionFloatConstant Name=DistributionAlphaOverLife
		Constant=255.9f;
	End Object
	AlphaOverLife=(Distribution=DistributionAlphaOverLife)

	//*-----------------------------------------------------------------------------*/
	/** SizeByLife Module Defaults													*/
	//*-----------------------------------------------------------------------------*/
	MultiplyX=true
	MultiplyY=true
	MultiplyZ=true

	Begin Object Class=DistributionVectorConstant Name=DistributionLifeMultiplier
	End Object
	LifeMultiplier=(Distribution=DistributionLifeMultiplier)

	//*-----------------------------------------------------------------------------*/
	/** Initial RotationRate Module Defaults										*/
	//*-----------------------------------------------------------------------------*/
	Begin Object Class=DistributionFloatConstant Name=DistributionStartRotationRate
	End Object
	StartRotationRate=(Distribution=DistributionStartRotationRate)
}
