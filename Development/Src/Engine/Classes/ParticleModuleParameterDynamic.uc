/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 */
class ParticleModuleParameterDynamic extends ParticleModuleParameterBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 *	EmitterDynamicParameterValue
 *	Enumeration indicating the way a dynamic parameter should be set.
 */
enum EEmitterDynamicParameterValue
{
	/** UserSet - use the user set values in the distribution (the default) */
	EDPV_UserSet,
	/** VelocityX - pass the particle velocity along the X-axis thru */
	EDPV_VelocityX,
	/** VelocityY - pass the particle velocity along the Y-axis thru */
	EDPV_VelocityY,
	/** VelocityZ - pass the particle velocity along the Z-axis thru */
	EDPV_VelocityZ,
	/** VelocityMag - pass the particle velocity magnitude thru */
	EDPV_VelocityMag
};

/** Helper structure for displaying the parameter. */
struct native EmitterDynamicParameter
{
	/** The parameter name - from the material DynamicParameter expression. READ-ONLY */
	var() editconst name					ParamName;
	/** If TRUE, use the EmitterTime to retrieve the value, otherwise use Particle RelativeTime. */
	var() bool								bUseEmitterTime;
	/** If TRUE, only set the value at spawn time of the particle, otherwise update each frame. */
	var() bool								bSpawnTimeOnly;
	/** Where to get the parameter value from. */
	var() EEmitterDynamicParameterValue		ValueMethod;
	/** If TRUE, scale the velocity value selected in ValueMethod by the evaluated ParamValue. */
	var() bool								bScaleVelocityByParamValue;
	/** The distriubtion for the parameter value. */
	var() rawdistributionfloat				ParamValue;
};

/** The dynamic parameters this module uses. */
var() editfixedsize array<EmitterDynamicParameter>	DynamicParams;

/** Flags for optimizing update */
var int UpdateFlags;
var bool bUsesVelocity;

cpptext
{
	/**
	 *	Called after an object has been loaded
	 */
	virtual void	PostLoad();

	/**
	 *	Called on a particle that is freshly spawned by the emitter.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	SpawnTime	The time of the spawn.
	 */
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
	 *	Called on a particle that is being updated by its emitter.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	DeltaTime	The time since the last update.
	 */
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);

	/**
	 *	Returns the number of bytes that the module requires in the particle payload block.
	 *
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *
	 *	@return	UINT		The number of bytes the module needs per particle.
	 */
	virtual UINT	RequiredBytes(FParticleEmitterInstance* Owner = NULL);

	// For Cascade
	/**
	 *	Called when the module is created, this function allows for setting values that make
	 *	sense for the type of emitter they are being used in.
	 *
	 *	@param	Owner			The UParticleEmitter that the module is being added to.
	 */
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner);

	/** 
	 *	PostEditChange...
	 */
	virtual void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/** 
	 *	Fill an array with each Object property that fulfills the FCurveEdInterface interface.
	 *
	 *	@param	OutCurve	The array that should be filled in.
	 */
	virtual void	GetCurveObjects(TArray<FParticleCurvePair>& OutCurves);

	/**
	 *	Returns TRUE if the results of LOD generation for the given percentage will result in a 
	 *	duplicate of the module.
	 *
	 *	@param	SourceLODLevel		The source LODLevel
	 *	@param	DestLODLevel		The destination LODLevel
	 *	@param	Percentage			The percentage value that should be used when setting values
	 *
	 *	@return	UBOOL				TRUE if the generated module will be a duplicate.
	 *								FALSE if not.
	 */
	virtual UBOOL WillGeneratedModuleBeIdentical(UParticleLODLevel* SourceLODLevel, UParticleLODLevel* DestLODLevel, FLOAT Percentage)
	{
		// The assumption is that at 100%, ANY module will be identical...
		// (Although this is virtual to allow over-riding that assumption on a case-by-case basis!)
		return TRUE;
	}

	/**
	 *	Retrieve the ParticleSysParams associated with this module.
	 *
	 *	@param	ParticleSysParamList	The list of FParticleSysParams to add to
	 */
	virtual void GetParticleSysParamsUtilized(TArray<FString>& ParticleSysParamList);

	/**
	 *	Retrieve the distributions that use ParticleParameters in this module.
	 *
	 *	@param	ParticleParameterList	The list of ParticleParameter distributions to add to
	 */
	virtual void GetParticleParametersUtilized(TArray<FString>& ParticleParameterList);
	
	/**
	 *	Update the parameter names with the given material...
	 *
	 *	@param	InMaterialInterface	Pointer to the material interface
	 *	@param	bIsMeshEmitter		TRUE if the emitter is a mesh emitter...
	 *
	 */
	virtual void UpdateParameterNames(UMaterialInterface* InMaterialInterface, UBOOL bIsMeshEmitter);

	/**
	 *	Refresh the module...
	 */
	virtual void RefreshModule(UInterpCurveEdSetup* EdSetup, UParticleEmitter* InEmitter, INT InLODLevel);
	
	/**
	 *	Retrieve the value for the parameter at the given index.
	 *
	 *	@param	InDynParams		The FEmitterDynamicParameter to fetch the value for
	 *	@param	Particle		The particle we are getting the value for.
	 *	@param	Owner			The FParticleEmitterInstance owner of the particle.
	 *	@param	InRandomStream	The random stream to use when retrieving the value
	 *
	 *	@return	FLOAT			The value for the parameter.
	 */
	FORCEINLINE FLOAT GetParameterValue(FEmitterDynamicParameter& InDynParams, FBaseParticle& Particle, FParticleEmitterInstance* Owner, class FRandomStream* InRandomStream)
	{
		FLOAT ScaleValue = 1.0f;
		FLOAT DistributionValue = 1.0f;
		FLOAT TimeValue = InDynParams.bUseEmitterTime ? Owner->EmitterTime : Particle.RelativeTime;
		switch (InDynParams.ValueMethod)
		{
		case EDPV_VelocityX:
		case EDPV_VelocityY:
		case EDPV_VelocityZ:
			ScaleValue = Particle.Velocity[InDynParams.ValueMethod - 1];
			break;
		case EDPV_VelocityMag:
			ScaleValue = Particle.Velocity.Size();
			break;
		default:
			//case EDPV_UserSet:
			break;
		}

		if ((InDynParams.bScaleVelocityByParamValue == TRUE) || (InDynParams.ValueMethod == EDPV_UserSet))
		{
			DistributionValue = InDynParams.ParamValue.GetValue(TimeValue, Owner->Component, InRandomStream);
		}

		return DistributionValue * ScaleValue;
	}

	/**
	 *	Retrieve the value for the parameter at the given index.
	 *
	 *	@param	InDynParams		The FEmitterDynamicParameter to fetch the value for
	 *	@param	Particle		The particle we are getting the value for.
	 *	@param	Owner			The FParticleEmitterInstance owner of the particle.
	 *	@param	InRandomStream	The random stream to use when retrieving the value
	 *
	 *	@return	FLOAT			The value for the parameter.
	 */
	FORCEINLINE FLOAT GetParameterValue_UserSet(FEmitterDynamicParameter& InDynParams, FBaseParticle& Particle, FParticleEmitterInstance* Owner, class FRandomStream* InRandomStream)
	{
		return InDynParams.ParamValue.GetValue(InDynParams.bUseEmitterTime ? Owner->EmitterTime : Particle.RelativeTime, Owner->Component, InRandomStream);
	}

	/**
	 *	Set the UpdatesFlags and bUsesVelocity
	 */
	virtual	void UpdateUsageFlags();

#if WITH_MOBILE_RHI || WITH_EDITOR
	/**
	 *	Retrieve the index of the Time variable.
	 *	@param	MaxIndex	The max value of index the Time could be stored in
	 *	@return	INT			The index of the time variable or -1 if not found.
	 */
	INT ParticleDynamicParameter_GetTimeIndex(INT MaxIndex)
	{
		int TimeIndex = -1;
		for (int i = 0; i < MaxIndex && TimeIndex==-1; ++i)
		{
			if (DynamicParams(i).ParamName == FName("Time"))
			{
				TimeIndex = i;
			}
		}
		return TimeIndex;
	}
#endif //WITH_MOBILE_RHI || WITH_EDITOR
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	Begin Object Class=DistributionFloatConstant Name=DistributionParam1
	End Object
	DynamicParams(0)=(ParamName="",bUseEmitterTime=false,ValueMethod=EDPV_UserSet,ParamValue=(Distribution=DistributionParam1))
	Begin Object Class=DistributionFloatConstant Name=DistributionParam2
	End Object
	DynamicParams(1)=(ParamName="",bUseEmitterTime=false,ValueMethod=EDPV_UserSet,ParamValue=(Distribution=DistributionParam2))
	Begin Object Class=DistributionFloatConstant Name=DistributionParam3
	End Object
	DynamicParams(2)=(ParamName="",bUseEmitterTime=false,ValueMethod=EDPV_UserSet,ParamValue=(Distribution=DistributionParam3))
	Begin Object Class=DistributionFloatConstant Name=DistributionParam4
	End Object
	DynamicParams(3)=(ParamName="",bUseEmitterTime=false,ValueMethod=EDPV_UserSet,ParamValue=(Distribution=DistributionParam4))
}
