/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleColorOverLife extends ParticleModuleColorBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** The color to apply to the particle, as a function of the particle RelativeTime. */
var(Color)					rawdistributionvector	ColorOverLife;
/** The alpha to apply to the particle, as a function of the particle RelativeTime. */
var(Color)					rawdistributionfloat	AlphaOverLife;
/** If TRUE, the alpha value will be clamped to the [0..1] range. */
var(Color)					bool					bClampAlpha;

cpptext
{
	virtual void	PostLoad();
	virtual	void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual	void	AddModuleCurvesToEditor(UInterpCurveEdSetup* EdSetup);
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);

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
	bUpdateModule=true
	bCurvesAsColor=true
	bClampAlpha=true

	Begin Object Class=DistributionVectorConstantCurve Name=DistributionColorOverLife
	End Object
	ColorOverLife=(Distribution=DistributionColorOverLife)

	Begin Object Class=DistributionFloatConstant Name=DistributionAlphaOverLife
		Constant=1.0f;
	End Object
	AlphaOverLife=(Distribution=DistributionAlphaOverLife)
}
