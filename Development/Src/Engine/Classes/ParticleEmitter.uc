//=============================================================================
// ParticleEmitter
// The base class for any particle emitter objects.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class ParticleEmitter extends Object
	native(Particle)
	dependson(ParticleLODLevel)
	hidecategories(Object)
	editinlinenew
	abstract;

//=============================================================================
//	General variables
//=============================================================================
/** The name of the emitter. */
var(Particle)				name						EmitterName;

//=============================================================================
//	Burst emissions
//=============================================================================
enum EParticleBurstMethod
{
	EPBM_Instant,
	EPBM_Interpolated
};

struct native ParticleBurst
{
	/** The number of particles to burst */
	var()				int		Count;
	/** If >= 0, use as a range [CountLow..Count] */
	var()				int		CountLow;
	/** The time at which to burst them (0..1: emitter lifetime) */
	var()				float	Time;

	structdefaultproperties
	{
		CountLow=-1		// Disabled by default...
	}
};

//=============================================================================
//	SubUV-related
//=============================================================================
enum EParticleSubUVInterpMethod
{
	PSUVIM_None,
    PSUVIM_Linear,
    PSUVIM_Linear_Blend,
    PSUVIM_Random,
    PSUVIM_Random_Blend
};

var	transient				int							SubUVDataOffset;

//=============================================================================
//	Cascade-related
//=============================================================================
enum EEmitterRenderMode
{
	ERM_Normal,
	ERM_Point,
	ERM_Cross,
	ERM_None
};

/**
 *	How to render the emitter particles. Can be one of the following:
 *		ERM_Normal	- As the intended sprite/mesh
 *		ERM_Point	- As a 2x2 pixel block with no scaling and the color set in EmitterEditorColor
 *		ERM_Cross	- As a cross of lines, scaled to the size of the particle in EmitterEditorColor
 *		ERM_None	- Do not render
 */
var(Cascade) EEmitterRenderMode EmitterRenderMode;
/**
 *	The color of the emitter in the curve editor and debug rendering modes.
 */
var(Cascade) editoronly color EmitterEditorColor;

//=============================================================================
//	'Private' data - not required by the editor
//=============================================================================
var editinline export		array<ParticleLODLevel>		LODLevels;
var							bool						ConvertedModules;
var							int							PeakActiveParticles;

//=============================================================================
//	Performance/LOD Data
//=============================================================================

/**
 *	Initial allocation count - overrides calculated peak count if > 0
 */
var(Particle)				int							InitialAllocationCount;

/** 
 * Scales the spawn rate of this emitter when the engine is running in medium or low detail mode.
 * This can be used to optimize particle draw cost in splitscreen.
 * A value of 0 effectively disables this emitter outside of high detail mode,
 * And this does not affect spawn per unit, unless the value is 0.
 */
var(Particle)				float						MediumDetailSpawnRateScale;

/** This value indicates the emitter should be drawn 'collapsed' in Cascade */
var(Cascade) editoronly		bool						bCollapsed;

/** If TRUE, then show only this emitter in the editor */
var transient				bool						bIsSoloing;

/** 
 *	If TRUE, then this emitter was 'cooked out' by the cooker. 
 *	This means it was completely disabled, but to preserve any
 *	indexing schemes, it is left in place.
 */
var bool bCookedOut;


/** 
 *	Get the longest possible lifespan for this emitter.
 *	
 *	@param	InComponentDelay	The delay from the component using the emitter
 *
 *	@return	FLOAT				The longest lifespan this PSys could have; 0.0f if infinite.
 */
native function float GetMaxLifespan(float InComponentDelay);

//=============================================================================
//	C++
//=============================================================================
cpptext
{
	virtual void PreEditChange(UProperty* PropertyThatWillChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* InComponent);

	virtual void SetToSensibleDefaults() {}

	virtual void PostLoad();
	virtual void UpdateModuleLists();

	void SetEmitterName(FName Name);
	FName& GetEmitterName();
	virtual	void						SetLODCount(INT LODCount);

	// For Cascade
	void	AddEmitterCurvesToEditor(UInterpCurveEdSetup* EdSetup);
	void	RemoveEmitterCurvesFromEditor(UInterpCurveEdSetup* EdSetup);
	void	ChangeEditorColor(FColor& Color, UInterpCurveEdSetup* EdSetup);

	void	AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp);

	// LOD
	INT					CreateLODLevel(INT LODLevel, UBOOL bGenerateModuleData = TRUE);
	UBOOL				IsLODLevelValid(INT LODLevel);

	/** GetCurrentLODLevel
	*	Returns the currently set LODLevel. Intended for game-time usage.
	*	Assumes that the given LODLevel will be in the [0..# LOD levels] range.
	*	
	*	@return NULL if the requested LODLevel is not valid.
	*			The pointer to the requested UParticleLODLevel if valid.
	*/
	FORCEINLINE UParticleLODLevel* GetCurrentLODLevel(FParticleEmitterInstance* Instance)
	{
#if CONSOLE
		return Instance->CurrentLODLevel;
#else
		// for the game (where we care about perf) we don't branch
		if (GIsGame == TRUE)
		{
			return Instance->CurrentLODLevel;
		}
		else
		{
			EditorUpdateCurrentLOD( Instance );
			return Instance->CurrentLODLevel;
		}
#endif // if CONSOLE
	}

	void EditorUpdateCurrentLOD(FParticleEmitterInstance* Instance);

	UParticleLODLevel*	GetLODLevel(INT LODLevel);
	
	virtual UBOOL		AutogenerateLowestLODLevel(UBOOL bDuplicateHighest = FALSE);
	
	/**
	 *	CalculateMaxActiveParticleCount
	 *	Determine the maximum active particles that could occur with this emitter.
	 *	This is to avoid reallocation during the life of the emitter.
	 *
	 *	@return	TRUE	if the number was determined
	 *			FALSE	if the number could not be determined
	 */
	virtual UBOOL		CalculateMaxActiveParticleCount();

	/**
	 *	Retrieve the parameters associated with this particle system.
	 *
	 *	@param	ParticleSysParamList	The list of FParticleSysParams used in the system
	 *	@param	ParticleParameterList	The list of ParticleParameter distributions used in the system
	 */
	void GetParametersUtilized(TArray<FString>& ParticleSysParamList,
							   TArray<FString>& ParticleParameterList);
}

//=============================================================================
//	Default properties
//=============================================================================
defaultproperties
{
	EmitterName="Particle Emitter"
	ConvertedModules=true
	PeakActiveParticles=0
	MediumDetailSpawnRateScale=1
	EmitterEditorColor=(R=0,G=150,B=150,A=255)
}
