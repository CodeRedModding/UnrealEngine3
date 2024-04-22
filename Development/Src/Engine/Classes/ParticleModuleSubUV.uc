/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleSubUV extends ParticleModuleSubUVBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 *	The index of the sub-image that should be used for the particle.
 *	The value is retrieved using the RelativeTime of the particles.
 */
var(SubUV) rawdistributionfloat	SubImageIndex;

/** 
 *	If TRUE, use *real* time when updating the image index.
 *	The movie will update regardless of the slomo settings of the game.
 */
var(RealTime)	bool				bUseRealTime;

cpptext
{
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);

	/**
	 *	Determine the current image index to use...
	 *
	 *	@param	Owner					The emitter instance being updated.
	 *	@param	Offset					The offset to the particle payload for this module.
	 *	@param	Particle				The particle that the image index is being determined for.
	 *	@param	eMethod					The EParticleSubUVInterpMethod method used to update the subUV.
	 *	@param	SubUVPayload			The FFullSubUVPayload for this particle.
	 *	@param	ImageIndex		[out]	The image index to use for the particle.
	 *	@param	Interp			[out]	The current interpolation value (for blending 2 sub-images).
	 *	@param	DeltaTime				The time slice for this update.
	 *
	 *	@return	UBOOL					TRUE if successful, FALSE if not.
	 */
	virtual UBOOL	DetermineImageIndex(FParticleEmitterInstance* Owner, INT Offset, FBaseParticle* Particle, 
						EParticleSubUVInterpMethod eMethod, FFullSubUVPayload& SubUVPayload, 
						INT& ImageIndex, FLOAT& Interp, FLOAT DeltaTime);
	
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

	Begin Object Class=DistributionFloatConstant Name=DistributionSubImage
	End Object
	SubImageIndex=(Distribution=DistributionSubImage)
}
