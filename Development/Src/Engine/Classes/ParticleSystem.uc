/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleSystem extends Object
	native(Particle)
	hidecategories(Object);

/**
 *	ParticleSystemUpdateMode
 *	Enumeration indicating the method by which the system should be updated
 */
enum EParticleSystemUpdateMode
{
	/** RealTime	- update via the delta time passed in				*/
	EPSUM_RealTime,
	/** FixedTime	- update via a fixed time step						*/
	EPSUM_FixedTime
};

var()	EParticleSystemUpdateMode		SystemUpdateMode;

/** UpdateTime_FPS	- the frame per second to update at in FixedTime mode		*/
var()	float							UpdateTime_FPS;

/** UpdateTime_Delta	- internal												*/
var		float							UpdateTime_Delta;

/** WarmupTime	- the time to warm-up the particle system when first rendered	*/
var()	float							WarmupTime;

/**	WarmupTickRate - the time step for each tick during warm up.
	Increasing this improves performance. Decreasing, improves accuracy.
	Set to 0 to use the default tick time.										*/
var()	float							WarmupTickRate;

/** Emitters	- internal - the array of emitters in the system				*/
var		editinline	export	array<ParticleEmitter>	Emitters;

/** The component used to preview the particle system in Cascade				*/
var	transient ParticleSystemComponent	PreviewComponent;

/** The angle to use when rendering the thumbnail image							*/
var editoronly rotator ThumbnailAngle;

/** The distance to place the system when rendering the thumbnail image			*/
var editoronly float ThumbnailDistance;

/** The time to warm-up the system for the thumbnail image						*/
var(Thumbnail) editoronly float ThumbnailWarmup;

/** Deprecated, ParticleSystemLOD::bLit is used instead. */
var deprecated const bool bLit;

/** Used for curve editor to remember curve-editing setup.						*/
var		export InterpCurveEdSetup	CurveEdSetup;

/** If true, the system's Z axis will be oriented toward the camera				*/
var()	bool	bOrientZAxisTowardCamera;

//
//	LOD
//
/**
 *	How often (in seconds) the system should perform the LOD distance check.
 */
var(LOD)					float					LODDistanceCheckTime;

/**
 *	ParticleSystemLODMethod
 */
enum ParticleSystemLODMethod
{
	PARTICLESYSTEMLODMETHOD_Automatic,
	PARTICLESYSTEMLODMETHOD_DirectSet,
	PARTICLESYSTEMLODMETHOD_ActivateAutomatic
};

/**
 *	The method of LOD level determination to utilize for this particle system
 *	  PARTICLESYSTEMLODMETHOD_Automatic - Automatically set the LOD level, checking every LODDistanceCheckTime seconds.
 *    PARTICLESYSTEMLODMETHOD_DirectSet - LOD level is directly set by the game code.
 *    PARTICLESYSTEMLODMETHOD_ActivateAutomatic - LOD level is determined at Activation time, then left alone unless directly set by game code.
 */
var(LOD)					ParticleSystemLODMethod		LODMethod;

/**
 *	The array of distances for each LOD level in the system.
 *	Used when LODMethod is set to PARTICLESYSTEMLODMETHOD_Automatic.
 *
 *	Example: System with 3 LOD levels
 *		LODDistances(0) = 0.0
 *		LODDistances(1) = 2500.0
 *		LODDistances(2) = 5000.0
 *
 *		In this case, when the system is [   0.0 ..   2499.9] from the camera, LOD level 0 will be used.
 *										 [2500.0 ..   4999.9] from the camera, LOD level 1 will be used.
 *										 [5000.0 .. INFINITY] from the camera, LOD level 2 will be used.
 *
 */
var(LOD)	editfixedsize	array<float>			LODDistances;

/** LOD setting for intepolation (set by Cascade) Range [0..100]				*/
var editoronly int EditorLODSetting;

/**
 *	Internal value that tracks the regenerate LOD levels preference.
 *	If TRUE, when autoregenerating LOD levels in code, the low level will
 *	be a duplicate of the high.
 */
var			bool									bRegenerateLODDuplicate;

/** Structure containing per-LOD settings that pertain to the entire UParticleSystem. */
struct native ParticleSystemLOD
{
	/** 
	 * Boolean to indicate whether the particle system accepts lights or not.
	 * This must not be changed in-game, it can only be changed safely in the editor through Cascade.
	 */
	var()	bool	bLit;

structcpptext
{
	static FParticleSystemLOD CreateParticleSystemLOD()
	{
		FParticleSystemLOD NewLOD;
		NewLOD.bLit = FALSE;
		return NewLOD;
	}
}
};
var(LOD) array<ParticleSystemLOD> LODSettings;

/** Whether to use the fixed relative bounding box or calculate it every frame. */
var(Bounds)	bool		bUseFixedRelativeBoundingBox;
/**	Fixed relative bounding box for particle system.							*/
var(Bounds)	box			FixedRelativeBoundingBox;
/**
 * Number of seconds of emitter not being rendered that need to pass before it
 * no longer gets ticked/ becomes inactive.
 */
var()		float		SecondsBeforeInactive;

//
//	Cascade 'floor' mesh information
//
var editoronly	string		FloorMesh;
var editoronly	vector		FloorPosition;
var editoronly	rotator		FloorRotation;
var editoronly	float		FloorScale;
var editoronly	vector		FloorScale3D;

/** The background color to display in Cascade */
var editoronly	color		BackgroundColor;

/** EDITOR ONLY: Indicates that Cascade would like to have the PeakActiveParticles count reset */
var			bool			bShouldResetPeakCounts;

/** Set during load time to indicate that physics is used... */
var		transient			bool							bHasPhysics;

/** Inidicates the old 'real-time' thumbnail rendering should be used	*/
var(Thumbnail)	bool		bUseRealtimeThumbnail;
/** Internal: Indicates the PSys thumbnail image is out of date			*/
var				bool		ThumbnailImageOutOfDate;
/** Internal: The PSys thumbnail image									*/
var	editoronly	Texture2D	ThumbnailImage;

/** 
 *	When TRUE, do NOT perform the spawning limiter check.
 *	Intended for effects used in pre-rendered cinematics.
 */
var() bool bSkipSpawnCountCheck;

/** How long this Particle system should delay when ActivateSystem is called on it. */
var(Delay) float Delay;
/** The low end of the emitter delay if using a range. */
var(Delay) float DelayLow;
/**
 *	If TRUE, select the emitter delay from the range 
 *		[DelayLow..Delay]
 */
var(Delay) bool bUseDelayRange;

/** Local space position that UVs generated with the ParticleMacroUV material node will be centered on. */
var(MacroUV) vector MacroUVPosition; 

/** World space radius that UVs generated with the ParticleMacroUV material node will tile based on. */
var(MacroUV) float MacroUVRadius; 

/** Occlusion method enumeration */
enum EParticleSystemOcclusionBoundsMethod
{
	/** Don't determine occlusion on this particle system */
	EPSOBM_None,
	/** Use the bounds of the particle system component when determining occlusion */
	EPSOBM_ParticleBounds,
	/** Use the custom occlusion bounds when determining occlusion */
	EPSOBM_CustomBounds
};

/** 
 *	Which occlusion bounds method to use for this particle system.
 *	EPSOBM_None - Don't determine occlusion for this system.
 *	EPSOBM_ParticleBounds - Use the bounds of the component when determining occlusion.
 */
var(Occlusion)	EParticleSystemOcclusionBoundsMethod	OcclusionBoundsMethod;

/** The occlusion bounds to use if OcclusionBoundsMethod is set to EPSOBM_CustomBounds */
var(Occlusion)	Box										CustomOcclusionBounds;
 
/**
 *	Temporary array for tracking 'solo' emitter mode.
 *	Entry will be true if emitter was enabled 
 */
struct native LODSoloTrack
{
	var transient array<byte>	SoloEnableSetting;
};
var transient array<LODSoloTrack>	SoloTracking;

//
/** Return the currently set LOD method											*/
native function ParticleSystemLODMethod	GetCurrentLODMethod();
/** Return the number of LOD levels for this particle system					*/
native function	int					GetLODLevelCount();
/** Return the distance for the given LOD level									*/
native function	float				GetLODDistance(int LODLevelIndex);
/** Set the LOD method															*/
native function						SetCurrentLODMethod(ParticleSystemLODMethod InMethod);
/** Set the distance for the given LOD index									*/
native function bool				SetLODDistance(int LODLevelIndex, float InDistance);

/** 
 *	Get the longest possible lifespan for this particle system.
 *	
 *	@param	InComponentDelay	The delay from the component using the emitter
 *
 *	@return	FLOAT				The longest lifespan this PSys could have; 0.0f if infinite.
 */
native function float GetMaxLifespan(float InComponentDelay);

//
cpptext
{
	// UObject interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PreSave();
	virtual void PostLoad();

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize();

	void UpdateColorModuleClampAlpha(class UParticleModuleColorBase* ColorModule);

	/**
	 *	CalculateMaxActiveParticleCounts
	 *	Determine the maximum active particles that could occur with each emitter.
	 *	This is to avoid reallocation during the life of the emitter.
	 *
	 *	@return	TRUE	if the numbers were determined for each emitter
	 *			FALSE	if not be determined
	 */
	virtual UBOOL		CalculateMaxActiveParticleCounts();
	
	/**
	 *	Retrieve the parameters associated with this particle system.
	 *
	 *	@param	ParticleSysParamList	The list of FParticleSysParams used in the system
	 *	@param	ParticleParameterList	The list of ParticleParameter distributions used in the system
	 */
	void GetParametersUtilized(TArray<TArray<FString> >& ParticleSysParamList,
							   TArray<TArray<FString> >& ParticleParameterList);

	/**
	 *	Setup the soloing information... Obliterates all current soloing.
	 */
	void SetupSoloing();

	/**
	 *	Toggle the bIsSoloing flag on the given emitter.
	 *
	 *	@param	InEmitter		The emitter to toggle.
	 *
	 *	@return	UBOOL			TRUE if ANY emitters are set to soloing, FALSE if none are.
	 */
	UBOOL ToggleSoloing(class UParticleEmitter* InEmitter);

	/**
	 *	Turn soloing off completely - on every emitter
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if not.
	 */
	UBOOL TurnOffSoloing();

	/**
	 *	Editor helper function for setting the LOD validity flags used in Cascade.
	 */
	void SetupLODValidity();

#if WITH_EDITOR
	/**
	 *	Convert the given module to its randon seed variant.
	 *
	 *	@param	InEmitter				The emitter to convert
	 *	@param	InModuleIdx				The index of the module to convert
	 *	@param	InSeededClass			The seeded variant class
	 *	@param	bInUpdateModuleLists	If TRUE, update the module lists after the conversion
	 *
	 *	@return	UBOOL					TRUE if successful, FALSE if not
	 */
	UBOOL ConvertModuleToSeeded(UParticleEmitter* InEmitter, INT InModuleIdx, UClass* InSeededClass, UBOOL bInUpdateModuleLists);

	/**
	 *	Convert all the modules in this particle system to their random seed variant if available
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if not
	 */
	UBOOL ConvertAllModulesToSeeded();

	/**
	 *	Remove all duplicate modules.
	 *
	 *	@param	bInMarkForCooker	If TRUE, mark removed objects to not cook out.
	 *	@param	OutRemovedModules	Optional map to fill in w/ removed modules...
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not
	 */
	UBOOL RemoveAllDuplicateModules(UBOOL bInMarkForCooker, TMap<UObject*,UBOOL>* OutRemovedModules);

	/**
	 *	Update all emitter module lists
	 */
	void UpdateAllModuleLists();
#endif
}

//
defaultproperties
{
	//bOrientZAxisTowardCamera=TRUE

	ThumbnailDistance=200.0
	ThumbnailWarmup=1.0

	UpdateTime_FPS=60.0
	UpdateTime_Delta=1.0/60.0
	WarmupTime=0.0
	WarmupTickRate=0.0

	bLit=true

	EditorLODSetting=0
	FixedRelativeBoundingBox=(Min=(X=-1,Y=-1,Z=-1),Max=(X=1,Y=1,Z=1))

	LODMethod=PARTICLESYSTEMLODMETHOD_Automatic
	LODDistanceCheckTime=0.25

	bRegenerateLODDuplicate=false
	ThumbnailImageOutOfDate=true

	FloorMesh="EditorMeshes.AnimTreeEd_PreviewFloor"
	FloorPosition=(X=0.000000,Y=0.000000,Z=0.000000)
	FloorRotation=(Pitch=0,Yaw=0,Roll=0)
	FloorScale=1.000000
	FloorScale3D=(X=1.000000,Y=1.000000,Z=1.000000)

	MacroUVPosition=(X=0.000000,Y=0.000000,Z=0.000000)
	MacroUVRadius=200 
}
