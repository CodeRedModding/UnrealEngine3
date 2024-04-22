/**
 *	ParticleModuleUberRainSplashA
 *
 *	Uber-module replacing the following classes:
 *		LT  - Lifetime
 *		IS  - Initial Size
 *		COL - Color Over Life
 *
 *	Intended for use in the Rain particle system.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class ParticleModuleUberRainSplashA extends ParticleModuleUberBase
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
/** MeshRotation Module Members													*/
//*-----------------------------------------------------------------------------*/
var(Rotation)	rawdistributionvector		StartRotation;
var(Rotation)	bool						bInheritParent;

//*-----------------------------------------------------------------------------*/
/** SizeByLife Module Members													*/
//*-----------------------------------------------------------------------------*/
var(Size)		rawdistributionvector		LifeMultiplier;
var(Size)		bool						MultiplyX;
var(Size)		bool						MultiplyY;
var(Size)		bool						MultiplyZ;

//*-----------------------------------------------------------------------------*/
/** ColorOverLife Module Members												*/
//*-----------------------------------------------------------------------------*/
var(Color)		rawdistributionvector		ColorOverLife;
var(Color)		rawdistributionfloat		AlphaOverLife;

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

	/**
	 *	Return TRUE if this module impacts rotation of Mesh emitters
	 *	@return	UBOOL		TRUE if the module impacts mesh emitter rotation
	 */
	virtual UBOOL	TouchesMeshRotation() const	{ return TRUE; }
}

//*-----------------------------------------------------------------------------*/
//*-----------------------------------------------------------------------------*/
defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

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
	/** MeshRotation Module Defaults												*/
	//*-----------------------------------------------------------------------------*/
	Begin Object Class=DistributionVectorUniform Name=DistributionStartRotation
		Min=(X=0.0,Y=0.0,Z=0.0)
		Max=(X=360.0,Y=360.0,Z=360.0)
	End Object
	StartRotation=(Distribution=DistributionStartRotation)
	
	bInheritParent=false

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
}
