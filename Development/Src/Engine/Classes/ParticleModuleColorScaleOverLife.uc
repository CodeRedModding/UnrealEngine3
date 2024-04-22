/**
 *	ParticleModuleColorScaleOverLife
 *
 *	The base class for all Beam modules.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class ParticleModuleColorScaleOverLife extends ParticleModuleColorBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** The scale factor for the color.													*/
var(Color)				rawdistributionvector	ColorScaleOverLife;

/** The scale factor for the alpha.													*/
var(Color)				rawdistributionfloat	AlphaScaleOverLife;

/** Whether it is EmitterTime or ParticleTime related.								*/
var(Color)				bool					bEmitterTime;

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

#if WITH_EDITOR
	/**
	 *	Get the number of custom entries this module has. Maximum of 3.
	 *
	 *	@return	INT		The number of custom menu entries
	 */
	virtual INT GetNumberOfCustomMenuOptions() const;

	/**
	 *	Get the display name of the custom menu entry.
	 *
	 *	@param	InEntryIndex		The custom entry index (0-2)
	 *	@param	OutDisplayString	The string to display for the menu
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not
	 */
	virtual UBOOL GetCustomMenuEntryDisplayString(INT InEntryIndex, FString& OutDisplayString) const;

	/**
	 *	Perform the custom menu entry option.
	 *
	 *	@param	InEntryIndex		The custom entry index (0-2) to perform
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not
	 */
	virtual UBOOL PerformCustomMenuEntry(INT InEntryIndex);
#endif

}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true
	bCurvesAsColor=true

	Begin Object Class=DistributionVectorConstantCurve Name=DistributionColorScaleOverLife
	End Object
	ColorScaleOverLife=(Distribution=DistributionColorScaleOverLife)

	Begin Object Class=DistributionFloatConstant Name=DistributionAlphaScaleOverLife
		Constant=1.0f;
	End Object
	AlphaScaleOverLife=(Distribution=DistributionAlphaScaleOverLife)
}
