/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleSizeScale extends ParticleModuleSizeBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 *	The amount the BaseSize should be scaled before being used as the size of the particle. 
 *	The value is retrieved using the RelativeTime of the particle during its update.
 *	NOTE: this module overrides any size adjustments made prior to this module in that frame.
 */
var()					rawdistributionvector	SizeScale;
/** Ignored */
var()					bool					EnableX;
/** Ignored */
var()					bool					EnableY;
/** Ignored */
var()					bool					EnableZ;

cpptext
{
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
	bSpawnModule=false
	bUpdateModule=true

	EnableX=true
	EnableY=true
	EnableZ=true

	Begin Object Class=DistributionVectorConstant Name=DistributionSizeScale
	End Object
	SizeScale=(Distribution=DistributionSizeScale)
}
