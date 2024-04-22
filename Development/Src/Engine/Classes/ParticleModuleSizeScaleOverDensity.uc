//-----------------------------------------------------------
// Particle Module to scale size over density
//-----------------------------------------------------------
class ParticleModuleSizeScaleOverDensity extends ParticleModuleSizeBase
    native(Particle)
	editinlinenew
	hidecategories(Object);;

/** The size scale to apply to the particle, as a function of the particle Density. */
var(Size)					rawdistributionvector	SizeScaleOverDensity;


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

	Begin Object Class=DistributionVectorConstantCurve Name=DistributionSizeScaleOverDensity
	End Object
	SizeScaleOverDensity=(Distribution=DistributionSizeScaleOverDensity)
}
