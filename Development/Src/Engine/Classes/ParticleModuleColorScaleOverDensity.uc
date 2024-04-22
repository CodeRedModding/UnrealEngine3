//-----------------------------------------------------------
// Particle Module to scale color and alpha over density
//-----------------------------------------------------------
class ParticleModuleColorScaleOverDensity extends ParticleModuleColorBase
    native(Particle)
	editinlinenew
	hidecategories(Object);;

/** The color to apply to the particle, as a function of the particle Density. */
var(Color)					rawdistributionvector	ColorScaleOverDensity;
/** The alpha to apply to the particle, as a function of the particle Density. */
var(Color)					rawdistributionfloat	AlphaScaleOverDensity;

cpptext
{
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

DefaultProperties
{
    bSpawnModule=true
	bUpdateModule=true

	Begin Object Class=DistributionVectorConstantCurve Name=DistributionColorScaleOverDensity
	End Object
	ColorScaleOverDensity=(Distribution=DistributionColorScaleOverDensity)

	Begin Object Class=DistributionFloatConstant Name=DistributionAlphaScaleOverDensity
	End Object
	AlphaScaleOverDensity=(Distribution=DistributionAlphaScaleOverDensity)
}
