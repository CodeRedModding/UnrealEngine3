/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleSubUVMovie extends ParticleModuleSubUV
	native(Particle)
	editinlinenew
	hidecategories(Object)
	hidecategories(SubUV);

/**
 *	If TRUE, use the emitter time to look up the frame rate.
 *	If FALSE (default), use the particle relative time.
 */
var(Flipbook)	bool					bUseEmitterTime;

/**
 *	The frame rate the SubUV images should be 'flipped' thru at.
 
 */
var(Flipbook)	rawdistributionfloat	FrameRate;

/**
 *	The starting image index for the SubUV (1 = the first frame).
 *	Assumes order of Left->Right, Top->Bottom
 *	If greater than the last frame, it will clamp to the last one.
 *	If 0, then randomly selects a starting frame.
 */
var(Flipbook)	int						StartingFrame;

cpptext
{
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);

	/**
	 *	Returns the number of bytes that the module requires in the particle payload block.
	 *
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *
	 *	@return	UINT		The number of bytes the module needs per particle.
	 */
	virtual UINT	RequiredBytes(FParticleEmitterInstance* Owner = NULL);
	
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

	/** Fill an array with each Object property that fulfills the FCurveEdInterface interface. */
	virtual void GetCurveObjects(TArray<FParticleCurvePair>& OutCurves);
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	Begin Object Class=DistributionFloatConstant Name=DistributionFrameRate
		Constant=30.0
	End Object
	FrameRate=(Distribution=DistributionFrameRate)

	StartingFrame=1
}
