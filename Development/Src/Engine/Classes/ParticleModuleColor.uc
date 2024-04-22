/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleColor extends ParticleModuleColorBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** Initial color for a particle as a function of Emitter time. */
var(Color) rawdistributionvector	StartColor;
/** Initial alpha for a particle as a function of Emitter time. */
var(Color) rawdistributionfloat		StartAlpha;
/** If TRUE, the alpha value will be clamped to the [0..1] range. */
var(Color) bool						bClampAlpha;

cpptext
{
	virtual void	PostLoad();
	virtual	void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual	void	AddModuleCurvesToEditor(UInterpCurveEdSetup* EdSetup);
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	/**
	 *	Extended version of spawn, allows for using a random stream for distribution value retrieval
	 *
	 *	@param	Owner				The particle emitter instance that is spawning
	 *	@param	Offset				The offset to the modules payload data
	 *	@param	SpawnTime			The time of the spawn
	 *	@param	InRandomStream		The random stream to use for retrieving random values
	 */
	void SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream);

	/**
	 *	Called when the module is created, this function allows for setting values that make
	 *	sense for the type of emitter they are being used in.
	 *
	 *	@param	Owner			The UParticleEmitter that the module is being added to.
	 */
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner);
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=false
	bCurvesAsColor=true
	bClampAlpha=true

	Begin Object Class=DistributionVectorConstant Name=DistributionStartColor
	End Object
	StartColor=(Distribution=DistributionStartColor)

	Begin Object Class=DistributionFloatConstant Name=DistributionStartAlpha
		Constant=1.0f;
	End Object
	StartAlpha=(Distribution=DistributionStartAlpha)
}
