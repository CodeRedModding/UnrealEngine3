/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModule extends Object
	native(Particle)
	editinlinenew
	hidecategories(Object)
	abstract;

/** If TRUE, the module performs operations on particles during Spawning		*/
var				bool			bSpawnModule;
/** If TRUE, the module performs operations on particles during Updating		*/
var				bool			bUpdateModule;
/** If TRUE, the module performs operations on particles during final update	*/
var				bool			bFinalUpdateModule;
/** If TRUE, the module displays vector curves as colors						*/
var				bool			bCurvesAsColor;
/** If TRUE, the module should render its 3D visualization helper				*/
var(Cascade)	bool			b3DDrawMode;
/** If TRUE, the module supports rendering a 3D visualization helper			*/
var				bool			bSupported3DDrawMode;
/** If TRUE, the module is enabled												*/
var				bool			bEnabled;
/** If TRUE, the module has had editing enabled on it							*/
var				bool			bEditable;
/**
*	If TRUE, this flag indicates that auto-generation for LOD will result in
*	an exact duplicate of the module, regardless of the percentage.
*	If FALSE, it will result in a module with different settings.
*/
var				bool			LODDuplicate;
/** If TRUE, the module supports RandomSeed setting */
var				bool			bSupportsRandomSeed;
/** If TRUE, the module should be told when looping */
var				bool			bRequiresLoopingNotification;

/**
 *	The LOD levels this module is present in.
 *	Bit-flags are used to indicate validity for a given LOD level.
 *	For example, if
 *		((1 << Level) & LODValidity) != 0
 *	then the module is used in that LOD.
 */
var const byte					LODValidity;

/** The color to draw the modules curves in the curve editor. 
 *	If bCurvesAsColor is TRUE, it overrides this value.
 */
var(Cascade) editoronly color ModuleEditorColor;


struct native transient ParticleCurvePair
{
	var		string	CurveName;
	var		object	CurveObject;
};

/** ModuleType
 *	Indicates the kind of emitter the module can be applied to.
 *	ie, EPMT_Beam - only applies to beam emitters.
 *
 *	The TypeData field is present to speed up finding the TypeData module.
 */
enum EModuleType
{
	/** General - all emitter types can use it			*/
	EPMT_General,
	/** TypeData - TypeData modules						*/
	EPMT_TypeData,
	/** Beam - only applied to beam emitters			*/
	EPMT_Beam,
	/** Trail - only applied to trail emitters			*/
	EPMT_Trail,
	/** Spawn - all emitter types REQUIRE it			*/
	EPMT_Spawn,
	/** Required - all emitter types REQUIRE it			*/
	EPMT_Required,
	/** Event - event related modules					*/
	EPMT_Event
};

/** 
 *	Particle Selection Method, for any emitters that utilize particles
 *	as the source points.
 */
enum EParticleSourceSelectionMethod
{
	/** Random		- select a particle at random		*/
	EPSSM_Random,
	/** Sequential	- select a particle in order		*/
	EPSSM_Sequential
};

struct native ParticleRandomSeedInfo
{
	/** The name to expose to the placed instances for setting this seed */
	var() name			ParameterName;
	/** 
	 *	If TRUE, the module will attempt to get the seed from the owner
	 *	instance. If that fails, it will fall back to getting it from
	 *	the RandomSeeds array.
	 */
	var() bool			bGetSeedFromInstance;
	/** 
	 *	If TRUE, the seed value retrieved from teh instance will be an
	 *	index into the array of seeds.
	 */
	var() bool			bInstanceSeedIsIndex;
	/** 
	 *	If TRUE, then reset the seed upon the emitter looping.
	 *	For looping environmental effects this should likely be set to FALSE to avoid
	 *	a repeating pattern.
	 */
	var() bool			bResetSeedOnEmitterLooping;
	/** 
	 *	The random seed values to utilize for the module. 
	 *	More than 1 means the instance will randomly select one.
	 */
	var() array<int>	RandomSeeds;

	structcpptext
	{
		FORCEINLINE INT GetInstancePayloadSize() const
		{
			return ((RandomSeeds.Num() > 0) ? sizeof(FParticleRandomSeedInstancePayload) : 0);
		}
	}

 	structdefaultproperties
 	{
 		bResetSeedOnEmitterLooping=true
 	}
};

cpptext
{
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
	 *	Called on a particle that is being updated by its emitter.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	DeltaTime	The time since the last update.
	 */
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);
	/**
	 *	Called on an emitter when all other update operations have taken place
	 *	INCLUDING bounding box cacluations!
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	DeltaTime	The time since the last update.
	 */
	virtual void	FinalUpdate(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);

	/**
	 *	Returns the number of bytes that the module requires in the particle payload block.
	 *
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *
	 *	@return	UINT		The number of bytes the module needs per particle.
	 */
	virtual UINT	RequiredBytes(FParticleEmitterInstance* Owner = NULL);
	/**
	 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *
	 *	@return UINT		The number of bytes the module needs per emitter instance.
	 */
	virtual UINT	RequiredBytesPerInstance(FParticleEmitterInstance* Owner = NULL);
	/**
	 *	Allows the module to prep its 'per-instance' data block.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	InstData	Pointer to the data block for this module.
	 */
	virtual UINT	PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData);

	// For Cascade
	/**
	 *	Called when the module is created, this function allows for setting values that make
	 *	sense for the type of emitter they are being used in.
	 *
	 *	@param	Owner			The UParticleEmitter that the module is being added to.
	 */
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner);
	
	/** 
	 *	Fill an array with each Object property that fulfills the FCurveEdInterface interface.
	 *
	 *	@param	OutCurve	The array that should be filled in.
	 */
	virtual void	GetCurveObjects(TArray<FParticleCurvePair>& OutCurves);
	/** 
	 *	Add all curve-editable Objects within this module to the curve editor.
	 *
	 *	@param	EdSetup		The CurveEd setup to use for adding curved.
	 */
	virtual	void	AddModuleCurvesToEditor(UInterpCurveEdSetup* EdSetup);
	/** 
	 *	Remove all curve-editable Objects within this module from the curve editor.
	 *
	 *	@param	EdSetup		The CurveEd setup to remove the curve from.
	 */
	void	RemoveModuleCurvesFromEditor(UInterpCurveEdSetup* EdSetup);
	/** 
	 *	Does the module contain curves?
	 *
	 *	@return	UBOOL		TRUE if it does, FALSE if not.
	 */
	UBOOL	ModuleHasCurves();
	/** 
	 *	Are the modules curves displayed in the curve editor?
	 *
	 *	@param	EdSetup		The CurveEd setup to check.
	 *
	 *	@return	UBOOL		TRUE if they are, FALSE if not.
	 */
	UBOOL	IsDisplayedInCurveEd(UInterpCurveEdSetup* EdSetup);
	/** 
	 *	Helper function for updating the curve editor when the module editor color changes.
	 *
	 *	@param	Color		The new color the module is using.
	 *	@param	EdSetup		The CurveEd setup for the module.
	 */
	void	ChangeEditorColor(FColor& Color, UInterpCurveEdSetup* EdSetup);

	/** 
	 *	Render the modules 3D visualization helper primitive.
	 *
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the module.
	 *	@param	View		The scene view that is being rendered.
	 *	@param	PDI			The FPrimitiveDrawInterface to use for rendering.
	 */
	virtual void Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)	{};

	/**
	 *	Retrieve the ModuleType of this module.
	 *
	 *	@return	EModuleType		The type of module this is.
	 */
	virtual EModuleType	GetModuleType() const	{	return EPMT_General;	}

	/**
	 *	Helper function used by the editor to auto-populate a placed AEmitter with any
	 *	instance parameters that are utilized.
	 *
	 *	@param	PSysComp		The particle system component to be populated.
	 */
	virtual void	AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp);
	
	/**
	 *	Helper function used by the editor to auto-generate LOD values from a source module
	 *	and a percentage value used to scale its values.
	 *
	 *	@param	SourceModule	The ParticleModule to utilize as the template.
	 *	@param	Percentage		The value to use when scaling the source values.
	 */
	virtual UBOOL	GenerateLODModuleValues(UParticleModule* SourceModule, FLOAT Percentage, UParticleLODLevel* LODLevel);

	/**
	 *	Conversion functions for distributions.
	 *	Used to setup new distributions to a percentage value of the source.
	 */
	/**
	 *	Store the given percentage of the SourceFloat distribution in the FloatDist
	 *
	 *	@param	FloatDist			The distribution to put the result into.
	 *	@param	SourceFloatDist		The distribution of use as the source.
	 *	@param	Percentage			The percentage of the source value to use [0..100]
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not.
	 */
	virtual UBOOL	ConvertFloatDistribution(UDistributionFloat* FloatDist, UDistributionFloat* SourceFloatDist, FLOAT Percentage);
	/**
	 *	Store the given percentage of the SourceVector distribution in the VectorDist
	 *
	 *	@param	VectorDist			The distribution to put the result into.
	 *	@param	SourceVectorDist	The distribution of use as the source.
	 *	@param	Percentage			The percentage of the source value to use [0..100]
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not.
	 */
	virtual UBOOL	ConvertVectorDistribution(UDistributionVector* VectorDist, UDistributionVector* SourceVectorDist, FLOAT Percentage);
	/**
	 *	Returns whether the module is SizeMultipleLife or not.
	 *
	 *	@return	UBOOL	TRUE if the module is a UParticleModuleSizeMultipleLife
	 *					FALSE if not
	 */
	virtual UBOOL   IsSizeMultiplyLife() { return FALSE; };

	/**
	 *	Returns whether the module supports the RandomSeed functionality
	 *
	 *	@return	UBOOL	TRUE if it supports it; FALSE if not
	 */
	UBOOL SupportsRandomSeed() const
	{
		return bSupportsRandomSeed;
	}

	/**
	 *	Returns whether the module requires notification when an emitter loops.
	 *
	 *	@return	UBOOL	TRUE if the module required looping notification
	 */
	UBOOL RequiresLoopingNotification() const
	{
		return bRequiresLoopingNotification;
	}

	/** 
	 *	Called when an emitter instance is looping...
	 *
	 *	@param	Owner					The emitter instance that owns this module
	 */
	virtual void EmitterLoopingNotify(FParticleEmitterInstance* Owner)
	{
	}

	/**
	 *	Comparison routine...
	 *	Intended for editor-use only, this function will return TRUE if the given
	 *	particle module settings are identical to the one the function is called on.
	 *
	 *	@param	InModule	The module to compare against.
	 *
	 *	@return	TRUE		If the modules have all the relevant settings the same.
	 *			FALSE		If they don't.
	 */
	virtual UBOOL	IsIdentical_Deprecated(const UParticleModule* InModule) const;

	/**
	 *	Used by the comparison routine to check for properties that are irrelevant.
	 *
	 *	@param	InPropName	The name of the property being checked.
	 *
	 *	@return	TRUE		If the property is relevant.
	 *			FALSE		If it isn't.
	 */
	virtual UBOOL	PropertyIsRelevantForIsIdentical_Deprecated(const FName& InPropName) const;

	/**
	 *	Generates a new module for LOD levels, setting the values appropriately.
	 *	Note that the module returned could simply be the module it was called on.
	 *
	 *	@param	SourceLODLevel		The source LODLevel
	 *	@param	DestLODLevel		The destination LODLevel
	 *	@param	Percentage			The percentage value that should be used when setting values
	 *
	 *	@return	UParticleModule*	The generated module, or this if percentage == 100.
	 */
	virtual UParticleModule* GenerateLODModule(UParticleLODLevel* SourceLODLevel, UParticleLODLevel* DestLODLevel, FLOAT Percentage, 
		UBOOL bGenerateModuleData, UBOOL bForceModuleConstruction = FALSE);

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

		if (Percentage != 100.0f)
		{
			return LODDuplicate;
		}

		return TRUE;
	}

	/**
	 *	Returns TRUE if the module validiy flags indicate this module is used in the given LOD level.
	 *
	 *	@param	SourceLODIndex		The index of the source LODLevel
	 *
	 *	@return	UBOOL				TRUE if the generated module is used, FALSE if not.
	 */
	virtual UBOOL IsUsedInLODLevel(INT SourceLODIndex) const;

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
	 *	Refresh the module...
	 */
	virtual void RefreshModule(UInterpCurveEdSetup* EdSetup, UParticleEmitter* InEmitter, INT InLODLevel) {}

	/**
	 *	Return TRUE if this module impacts rotation of Mesh emitters
	 *	@return	UBOOL		TRUE if the module impacts mesh emitter rotation
	 */
	virtual UBOOL	TouchesMeshRotation() const	{ return FALSE; }

	/** 
	 *	Prepare a random seed instance payload...
	 *
	 *	@param	Owner					The emitter instance that owns this module
	 *	@param	InRandSeedPayload		The random seed instance payload to initialize
	 *	@param	InRandSeedInfo			The random seed info of the module
	 *
	 *	@return	UINT					0xffffffff is failed
	 */
	virtual UINT	PrepRandomSeedInstancePayload(FParticleEmitterInstance* Owner, FParticleRandomSeedInstancePayload* InRandSeedPayload, const FParticleRandomSeedInfo& InRandSeedInfo);

	/**
	 *	Retrieve the random seed info for this module.
	 *
	 *	@return	FParticleRandomSeedInfo*	The random seed info; NULL if not supported
	 */
	virtual FParticleRandomSeedInfo* GetRandomSeedInfo()
	{
		return NULL;
	}

	/**
	 *	Set the random seed info entry at the given index to the given seed
	 *
	 *	@param	InIndex			The index of the entry to set
	 *	@param	InRandomSeed	The seed to set the entry to
	 *
	 *	@return	UBOOL			TRUE if successful; FALSE if not (not found, etc.)
	 */
	virtual UBOOL SetRandomSeedEntry(INT InIndex, INT InRandomSeed);

#if WITH_EDITOR
	/**
	 *	Custom Cascade module menu entries support
	 */
	/**
	 *	Get the number of custom entries this module has. Maximum of 3.
	 *
	 *	@return	INT		The number of custom menu entries
	 */
	virtual INT GetNumberOfCustomMenuOptions() const { return 0; }

	/**
	 *	Get the display name of the custom menu entry.
	 *
	 *	@param	InEntryIndex		The custom entry index (0-2)
	 *	@param	OutDisplayString	The string to display for the menu
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not
	 */
	virtual UBOOL GetCustomMenuEntryDisplayString(INT InEntryIndex, FString& OutDisplayString) const { return FALSE; }

	/**
	 *	Perform the custom menu entry option.
	 *
	 *	@param	InEntryIndex		The custom entry index (0-2) to perform
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not
	 */
	virtual UBOOL PerformCustomMenuEntry(INT InEntryIndex) { return FALSE; }
#endif
}

defaultproperties
{
	bSupported3DDrawMode=false
	b3DDrawMode=false
	bEnabled=true
	bEditable=true

	LODDuplicate=true
	bSupportsRandomSeed=false
	bRequiresLoopingNotification=false
}
