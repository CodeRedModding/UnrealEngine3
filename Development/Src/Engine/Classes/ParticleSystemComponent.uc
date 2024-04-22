/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleSystemComponent extends PrimitiveComponent
	native(Particle)
	hidecategories(Object)
	hidecategories(Physics)
	hidecategories(Collision)
	editinlinenew
	dependson(ParticleSystem);

var()				const	ParticleSystem							Template;

/** Class of the light environment that will get created for lit particle systems. */
var					class<ParticleLightEnvironmentComponent>		LightEnvironmentClass;

/** Lit particle components created from the emitter pool will only share particle DLE's if they have matching SharedInstigator's. */
var					transient	Actor								LightEnvironmentSharedInstigator;

/** 
 * Limit on the number of particle components that can reuse the same particle light environment. 
 * This controls the trade off between performance and particle lighting update rate.  
 */
var					transient	int									MaxLightEnvironmentPooledReuses;

struct ParticleEmitterInstance
{
	// No UObject reference
};
var		native transient	const	array<pointer>					EmitterInstances{struct FParticleEmitterInstance};

/**
 *	The static mesh components for a mesh emitter.
 *	This is to prevent the SMCs from being garbage collected.
 */
var private transient duplicatetransient const array<StaticMeshComponent> SMComponents;
/**
 *	The static mesh MaterialInterfaces for a mesh emitter.
 *	This is to prevent them from being garbage collected.
 */
var private transient duplicatetransient const array<MaterialInterface> SMMaterialInterfaces;
/**
 *	The skeletal mesh components used with the socket location module.
 *	This is to prevent them from being garbage collected.
 */
var private transient duplicatetransient const array<SkeletalMeshComponent> SkelMeshComponents;

/**
 * Stores motion blur transform info for particles
 */
struct native ParticleEmitterInstanceMotionBlurInfo 
{
	/** Maps unique particle Id to its motion blur info */
	var	const native transient Map_Mirror ParticleMBInfoMap{TMap<INT, struct FMeshElementMotionBlurInfo>};
};
/**
 * Stores motion blur transform info for emitter instances
 */
struct native ViewParticleEmitterInstanceMotionBlurInfo 
{
	/** Maps unique emitter instance via ptr to its particle motion blur info */
	var	const native transient Map_Mirror EmitterInstanceMBInfoMap{TMap<const struct FParticleMeshEmitterInstance*, struct FParticleEmitterInstanceMotionBlurInfo>};
};
/** Emitter instance motion blur info stored per view */
var	const native transient	array<ViewParticleEmitterInstanceMotionBlurInfo> ViewMBInfoArray;

/** If true, activate on creation. */
var()						bool									bAutoActivate;
var	transient 		const	bool									bWasCompleted;
var	transient 		const	bool									bSuppressSpawning;
var	transient 		const	bool									bWasDeactivated;
var()						bool									bResetOnDetach;
/** whether to update the particle system on dedicated servers */
var 						bool 									bUpdateOnDedicatedServer;

/** Indicates that the component has not been ticked since being attached. */
var transient 				bool									bJustAttached;

/** INTERNAL
 *	Set to TRUE when InitParticles has been called.
 *	Set to FALSE when ResetParticles has been called.
 *	Used to quick-out of Tick and Render calls
 * (when caching PSysComps and emitter instances).
 */
var	transient				bool									bIsActive;

/** This flag will be set the first time the PSysComp is activated... used to prevent auto activated PSysComps from calling InitParticles twice on level load */
var	transient				bool									bHasBeenActivated;

/** Enum for specifying type of a name instance parameter. */
enum EParticleSysParamType
{
	PSPT_None,
	PSPT_Scalar,
	PSPT_ScalarRand,
	PSPT_Vector,
	PSPT_VectorRand,
	PSPT_Color,
	PSPT_Actor,
	PSPT_Material
};

/** Struct used for a particular named instance parameter for this ParticleSystemComponent. */
struct native ParticleSysParam
{
	/** The name of the parameter */
	var()	name					Name;
	/** 
	 *	The type of parameters
	 *	PSPT_None       - There is no data type
	 *	PSPT_Scalar     - Use the scalar value
	 *	PSPT_ScalarRand - Select a scalar value in the range [Scalar_Low..Scalar)
	 *	PSPT_Vector     - Use the vector value
	 *	PSPT_VectorRand - Select a vector value in the range [Vector_Low..Vector)
	 *	PSPT_Color      - Use the color value
	 *	PSPT_Actor      - Use the actor value
	 *	PSPT_Material   - Use the material value
	 */
	var()	EParticleSysParamType	ParamType;

	var()	float					Scalar;
	var()	float					Scalar_Low;
	var()	vector					Vector;
	var()	vector					Vector_Low;
	var()	color					Color;
	var()	actor					Actor;
	var()	MaterialInterface		Material;
};

/**
 *	Array holding name instance parameters for this ParticleSystemComponent.
 *	Parameters can be used in Cascade using DistributionFloat/VectorParticleParameters.
 */
var()	editinline array<ParticleSysParam>		InstanceParameters;

var		vector									OldPosition;
var		vector									PartSysVelocity;

var		float									WarmupTime;
var		float									WarmupTickRate;
var 	bool 									bWarmingUp;
var		private{private} transient int			LODLevel;

/**
 * bCanBeCachedInPool
 *
 * If this is true, when the PSC completes it will do the following:
 *    bHidden = TRUE
 *
 * This is used for Particles which are cached in a pool where you need
 * to make certain to NOT kill off the EmitterInstances so we do not
 * re allocate.
 *
 * @see ActivateSystem() where it rewinds the indiv emitters if they need it
 */

var  	bool									bIsCachedInPool;


/**
 * Number of seconds of emitter not being rendered that need to pass before it
 * no longer gets ticked/ becomes inactive.
 */
var()	float									SecondsBeforeInactive;

/** Tracks the time since the last forced UpdateTransform. */
var	private{private} transient float			TimeSinceLastForceUpdateTransform;

/** 
 * Time between forced UpdateTransforms for systems that use dynamically calculated bounds,
 * Which is effectively how often the bounds are shrunk.
 */
var		float									MaxTimeBeforeForceUpdateTransform;


/**
 *	INTERNAL. Used by the editor to set the LODLevel
 */
var	editoronly	int								EditorLODLevel;

/**
 * Used for applying Cascade's detail mode setting to in-level particle systems
 */
var editoronly int								EditorDetailMode;

/** Used to accumulate total tick time to determine whether system can be skipped ticking if not visible. */
var	transient	float							AccumTickTime;

/** indicates that the component's LODMethod overrides the Template's */
var(LOD) bool bOverrideLODMethod;
/** The method of LOD level determination to utilize for this particle system */
var(LOD) ParticleSystemLODMethod LODMethod;

/**
 *	Flag indicating that dynamic updating of render data should NOT occur during Tick.
 *	This is used primarily to allow for warming up and simulated effects to a certain state.
 */
var		bool									bSkipUpdateDynamicDataDuringTick;

/**
 *	Flag indicating that the bounds should not be updated for the particle system.
 *	Should only be used when the particle system is being ticked in a controlled environment.
 */
var		bool									bSkipBoundsUpdate;

/**
 *	Set this to TRUE to have the PSysComponent update during the tick if 'dirty'.
 */
var		bool									bUpdateComponentInTick;

/**
 *	Set this to TRUE to have beam emitters defer their update until the data is being passed to the render thread.
 */
var		bool									bDeferredBeamUpdate;

/** This is set when any of our "don't tick me" timeout values have fired */
var transient bool bForcedInActive;

/** This is set when the particle system component is warming up */
var transient bool bIsWarmingUp;

/** The view relevance flags for each LODLevel. */
var		transient	const	array<MaterialViewRelevance>	CachedViewRelevanceFlags;

/** If TRUE, the ViewRelevanceFlags are dirty and should be recached */
var		transient			bool							bIsViewRelevanceDirty;

/** If TRUE, the VRF were updated and should be passed to the proxy. */
var		transient			bool							bRecacheViewRelevance;



/** Array of replay clips for this particle system component.  These are serialized to disk.  You really should never add anything to this in the editor.  It's exposed so that you can delete clips if you need to, but be careful when doing so! */
var() const editinline array<ParticleSystemReplay> ReplayClips;

/** Particle system replay state */
enum ParticleReplayState
{
	/** Replay system is disabled.  Particles are simulated and rendered normally. */
	PRS_Disabled,

	/** Capture all particle data to the clip specified by ReplayClipIDNumber.  The frame to capture
	    must be specified using the ReplayFrameIndex */
	PRS_Capturing,

	/** Replay captured particle state from the clip specified by ReplayClipIDNumber.  The frame to play
	    must be specified using the ReplayFrameIndex */
	PRS_Replaying,
};
				
/** Current particle 'replay state'.  This setting controls whether we're currently simulating/rendering particles normally, or whether we should capture or playback particle replay data instead. */
var transient const ParticleReplayState ReplayState;

/** Clip ID number we're either playing back or capturing to, depending on the value of ReplayState. */
var transient const int ReplayClipIDNumber;

/** The current replay frame for playback */
var transient const int ReplayFrameIndex;

/** LOD updating... */
var transient float AccumLODDistanceCheckTime;
var transient bool bLODUpdatePending;

/** Check the spawn count and govern if needed */
var	transient bool bSkipSpawnCountCheck;

/** 
 *	Event type
 */
enum EParticleEventType
{
	/** Any - allow any event */
	EPET_Any,
	/** Spawn - a particle spawn event */
	EPET_Spawn,
	/** Death - a particle death event */
	EPET_Death,
	/** Collision - a particle collision event */
	EPET_Collision,
	/** World Attractor Collision - special handling for collisions with world attractors */
	EPET_WorldAttractorCollision,
	/** Kismet - an event generated by Kismet */
	EPET_Kismet
};

/**
 *	The base class for all particle event data.
 */
struct native ParticleEventData
{
	/** The type of event that was generated. */
	var int Type;
	/** The name of the event. */
	var name EventName;
	/** The emitter time at the event. */
	var float EmitterTime;
	/** The location of the event. */
	var vector Location;
	/** The direction of the particle at the time of the event. */
	var vector Direction;
	/** The velocity at the time of the event. */
	var vector Velocity;
};

/**
 *	Spawn particle event data.
 */
struct native ParticleEventSpawnData extends ParticleEventData
{
};

/**
 *	Killed particle event data.
 */
struct native ParticleEventDeathData extends ParticleEventData
{
	/** The particle time at its death.			*/
	var float ParticleTime;
};

/**
 *	Collision particle event data.
 */
struct native ParticleEventCollideData extends ParticleEventData
{
	/** The particle time at collision.			*/
	var float ParticleTime;
	/** Normal vector in coordinate system of the returner. Zero=none. */
	var vector Normal;
	/** Time until hit, if line check. */
	var float Time;
	/** Primitive data item which was hit, INDEX_NONE=none. */
	var int Item;
	/** Name of bone we hit (for skeletal meshes). */
	var name BoneName;
};

/**
 *	Attractor collision particle event data.
 */
struct native ParticleEventAttractorCollideData extends ParticleEventCollideData
{
};

/**
 *	Kismet particle event data.
 */
struct native ParticleEventKismetData extends ParticleEventData
{
	/** If TRUE, use the particle system component location as spawn location. */
	var bool UsePSysCompLocation;
	/** Normal vector in coordinate system of the returner. Zero=none. */
	var vector Normal;
};

/** The Spawn events that occurred in this PSysComp. */
var	transient array<ParticleEventSpawnData>		SpawnEvents;
/** The Death events that occurred in this PSysComp. */
var	transient array<ParticleEventDeathData>		DeathEvents;
/** The Collision events that occurred in this PSysComp. */
var	transient array<ParticleEventCollideData>	CollisionEvents;
/** The Kismet events that occurred for this PSysComp. */
var transient array<ParticleEventKismetData>	KismetEvents;
/** The Attractor Collision events that occurred in this PSysComp. */
var transient array<ParticleEventAttractorCollideData> AttractorCollisionEvents;

/** Command fence used to shut down properly */
var const native transient pointer ReleaseResourcesFence{class FRenderCommandFence};

/** Scales DeltaTime in UParticleSystemComponent::Tick(...) */
var() float CustomTimeDilation;

/** This is created at start up and then added to each emitter */
var transient float EmitterDelay;

//
delegate OnSystemFinished(ParticleSystemComponent PSystem);	// Called when the particle system is done

native final function SetTemplate(ParticleSystem NewTemplate);
native final function ActivateSystem(bool bFlagAsJustAttached = false);
native final function DeactivateSystem();
native final function KillParticlesForced();

/** 
 *	Kill the particles in the specified emitter(s)
 *
 *	@param	InEmitterName		The name of the emitter to kill the particles in.
 */
native final function KillParticlesInEmitter(name InEmitterName);

/**
 *	Function for setting the bSkipUpdateDynamicDataDuringTick flag.
 */
native final function SetSkipUpdateDynamicDataDuringTick(bool bInSkipUpdateDynamicDataDuringTick);

/**
 *	Function for retrieving the bSkipUpdateDynamicDataDuringTick flag.
 */
native final function bool GetSkipUpdateDynamicDataDuringTick();

/**
 *	Function for setting the bSkipBoundsUpdate flag.
 */
native final function SetSkipBoundsUpdate(bool bInSkipBoundsUpdate);

/**
 *	Function for retrieving the bSkipBoundsUpdate flag.
 */
native final function bool GetSkipBoundsUpdate();

/**
 * SetKillOnDeactivate is used to set the KillOnDeactivate flag. If true, when
 * the particle system is deactivated, it will immediately kill the emitter
 * instance. If false, the emitter instance live particles will complete their
 * lifetime.
 *
 * Set this to true for cached ParticleSystems
 *
 *	@param	EmitterIndex		The index of the emitter to set it on
 *	@param	bKill				value to set KillOnDeactivate to
 */
native function SetKillOnDeactivate(int EmitterIndex, bool bKill);

/**
 * SetKillOnDeactivate is used to set the KillOnCompleted( flag. If true, when
 * the particle system is completed, it will immediately kill the emitter
 * instance.
 *
 * Set this to true for cached ParticleSystems
 *
 *	@param	EmitterIndex		The index of the emitter to set it on
 *	@param	bKill				The value to set it to
 **/
native function SetKillOnCompleted(int EmitterIndex, bool bKill);

/**
 * Rewind emitter instances.
 **/
native function RewindEmitterInstance(int EmitterIndex);
native function RewindEmitterInstances();

/**
 *	Beam-related script functions
 */
/**
 *	Set the beam type
 *
 *	@param	EmitterIndex		The index of the emitter to set it on
 *	@param	NewMethod			The new method/type of beam to generate
 */
native function SetBeamType(int EmitterIndex, int NewMethod);
/**
 *	Set the beam tessellation factor
 *
 *	@param	EmitterIndex		The index of the emitter to set it on
 *	@param	NewFactor			The value to set it to
 */
native function SetBeamTessellationFactor(int EmitterIndex, float NewFactor);
/**
 *	Set the beam end point
 *
 *	@param	EmitterIndex		The index of the emitter to set it on
 *	@param	NewEndPoint			The value to set it to
 */
native function SetBeamEndPoint(int EmitterIndex, vector NewEndPoint);
/**
 *	Set the beam distance
 *
 *	@param	EmitterIndex		The index of the emitter to set it on
 *	@param	Distance			The value to set it to
 */
native function SetBeamDistance(int EmitterIndex, float Distance);
/**
 *	Set the beam source point
 *
 *	@param	EmitterIndex		The index of the emitter to set it on
 *	@param	NewSourcePoint		The value to set it to
 *	@param	SourceIndex			Which beam within the emitter to set it on
 */
native function SetBeamSourcePoint(int EmitterIndex, vector NewSourcePoint, int SourceIndex);
/**
 *	Set the beam source tangent
 *
 *	@param	EmitterIndex		The index of the emitter to set it on
 *	@param	NewTangentPoint		The value to set it to
 *	@param	SourceIndex			Which beam within the emitter to set it on
 */
native function SetBeamSourceTangent(int EmitterIndex, vector NewTangentPoint, int SourceIndex);
/**
 *	Set the beam source strength
 *
 *	@param	EmitterIndex		The index of the emitter to set it on
 *	@param	NewSourceStrength	The value to set it to
 *	@param	SourceIndex			Which beam within the emitter to set it on
 */
native function SetBeamSourceStrength(int EmitterIndex, float NewSourceStrength, int SourceIndex);
/**
 *	Set the beam target point
 *
 *	@param	EmitterIndex		The index of the emitter to set it on
 *	@param	NewTargetPoint		The value to set it to
 *	@param	TargetIndex			Which beam within the emitter to set it on
 */
native function SetBeamTargetPoint(int EmitterIndex, vector NewTargetPoint, int TargetIndex);
/**
 *	Set the beam target tangent
 *
 *	@param	EmitterIndex		The index of the emitter to set it on
 *	@param	NewTangentPoint		The value to set it to
 *	@param	TargetIndex			Which beam within the emitter to set it on
 */
native function SetBeamTargetTangent(int EmitterIndex, vector NewTangentPoint, int TargetIndex);
/**
 *	Set the beam target strength
 *
 *	@param	EmitterIndex		The index of the emitter to set it on
 *	@param	NewTargetStrength	The value to set it to
 *	@param	TargetIndex			Which beam within the emitter to set it on
 */
native function SetBeamTargetStrength(int EmitterIndex, float NewTargetStrength, int TargetIndex);

/**
 * This will determine which LOD to use based off the specific ParticleSystem passed in
 * and the distance to where that PS is being displayed.
 *
 * NOTE:  This is distance based LOD not perf based.  Perf and distance are orthogonal concepts.
 **/
native function int DetermineLODLevelForLocation(const out vector EffectLocation);

/** 
 *	Get the longest possible lifespan for this particle system.
 *	
 *	@return	FLOAT				The longest lifespan this PSys could have; 0.0f if infinite.
 */
native function float GetMaxLifespan();

/** Return the HasCompleted result */
native function bool SystemHasCompleted();

cpptext
{
	// ActorComponent interface.
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

	// UObject interface
	virtual void PostLoad();
	virtual void BeginDestroy();
	virtual void FinishDestroy();
	virtual void PreEditChange(UProperty* PropertyThatWillChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent);
	virtual void Serialize(FArchive& Ar);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize();

	// Collision Handling...
	virtual UBOOL SingleLineCheck(FCheckResult& Hit, AActor* SourceActor, const FVector& End, const FVector& Start, DWORD TraceFlags, const FVector& Extent);

protected:
	// UActorComponent interface.
	virtual void Attach();
	virtual void UpdateTransform();
	virtual void Detach( UBOOL bWillReattach = FALSE );
	virtual void UpdateLODInformation();

	/**
	 * Static: Supplied with a chunk of replay data, this method will create dynamic emitter data that can
	 * be used to render the particle system
	 *
	 * @param	EmitterInstance		Emitter instance this replay is playing on
	 * @param	EmitterReplayData	Incoming replay data of any time, cannot be NULL
	 * @param	bSelected			TRUE if the particle system is currently selected
	 *
	 * @return	The newly created dynamic data, or NULL on failure
	 */
	static FDynamicEmitterDataBase* CreateDynamicDataFromReplay( FParticleEmitterInstance* EmitterInstance, const FDynamicEmitterReplayDataBase* EmitterReplayData, UBOOL bSelected );

	/**
	 * Creates dynamic particle data for rendering the particle system this frame.  This function
	 * handle creation of dynamic data for regularly simulated particles, but also handles capture
	 * and playback of particle replay data.
	 *
	 * @return	Returns the dynamic data to render this frame
	 */
	FParticleDynamicData* CreateDynamicData();

	/** Orients the Z axis of the ParticleSystemComponent toward the camera while preserving the X axis direction */
	void OrientZAxisTowardCamera();
	
	/** 
	 * Returns the the correct LocalToWorld transform for this system if attached to a skeletal mesh
	 * @param	Transform		output transform if it exists
	 * @return					Returns true if the transform exists
	 */
	bool GetSkeletalMeshAttachmentTransform(FMatrix &Transform);

public:
	FORCEINLINE INT GetCurrentLODIndex() const
	{
		return LODLevel;
	}

	virtual void UpdateDynamicData();
	virtual void UpdateDynamicData(FParticleSystemSceneProxy* Proxy);
	virtual void UpdateViewRelevance(FParticleSystemSceneProxy* Proxy);

	// UPrimitiveComponent interface
	virtual void UpdateBounds();
	virtual void Tick(FLOAT DeltaTime);

	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void SetLightEnvironment(ULightEnvironmentComponent* NewLightEnvironment);

	/** 
	 * Retrieves the materials used in this component 
	 * 
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const;

	/**
	 * Determine if the primitive supports motion blur velocity rendering by storing
	 * motion blur transform info at the MeshElement level.
	 *
	 * @return TRUE if the primitive supports motion blur velocity rendering in its generated meshes
	 */
	virtual UBOOL HasMotionBlurVelocityMeshes() const;

	/**
	 * Determine if the given LODLevel requires motion blur velocity rendering.
	 *
	 *	@param	InLODIndex	The index of LOD level of interest
	 *	@return TRUE		if the given LODLevel requires motion blur velocity rendering in its generated meshes
	 */
	virtual UBOOL LODLevelHasMotionBlurVelocityMeshes(INT InLODIndex) const;

	// UParticleSystemComponent interface
	virtual void InitParticles();
	void ResetParticles(UBOOL bEmptyInstances = FALSE);
	void ResetBurstLists();
	void UpdateInstances();
	UBOOL HasCompleted();

	void InitializeSystem();

#if USE_GAMEPLAY_PROFILER
    /** 
     * This function actually does the work for the GetProfilerAssetObject and is virtual.  
     * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
     **/
	virtual UObject* GetProfilerAssetObjectInternal() const;
#endif

	/**
	 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
	 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
	 * you have a component of interest but what you really want is some characteristic that you can use to track
	 * down where it came from.  
	 * 
	 */
	virtual FString GetDetailedInfoInternal() const;

	/**
	 *	Cache the view-relevance for each emitter at each LOD level.
	 *
	 *	@param	NewTemplate		The UParticleSystem* to use as the template.
	 *							If NULL, use the currently set template.
	 */
	void CacheViewRelevanceFlags(class UParticleSystem* NewTemplate = NULL);

	/**
	*	DetermineLODLevel - determines the appropriate LOD level to utilize.
	*/
	INT DetermineLODLevel(const FSceneView* View);

	void	AutoPopulateInstanceProperties();

	void	FlushSMComponentsArray();

	/** Event reporting... */
	/** 
	 *	Record a spawning event. 
	 *
	 *	@param	InEventName			The name of the event that fired.
	 *	@param	InEmitterTime		The emitter time when the event fired.
	 *	@param	InLocation			The location of the particle when the event fired.
	 *	@param	InVelocity			The velocity of the particle when the event fired.
	 */
	void ReportEventSpawn(FName& InEventName, FLOAT InEmitterTime, 
		FVector& InLocation, FVector& InVelocity);
	/** 
	 *	Record a death event.
	 *
	 *	@param	InEventName			The name of the event that fired.
	 *	@param	InEmitterTime		The emitter time when the event fired.
	 *	@param	InLocation			The location of the particle when the event fired.
	 *	@param	InVelocity			The velocity of the particle when the event fired.
	 *	@param	InParticleTime		The relative life of the particle when the event fired.
	 */
	void ReportEventDeath(FName& InEventName, FLOAT InEmitterTime, 
		FVector& InLocation, FVector& InVelocity, FLOAT InParticleTime);
	/** 
	 *	Record a collision event.
	 *
	 *	@param	InEventName		The name of the event that fired.
	 *	@param	InEmitterTime	The emitter time when the event fired.
	 *	@param	InLocation		The location of the particle when the event fired.
	 *	@param	InDirection		The direction of the particle when the event fired.
	 *	@param	InVelocity		The velocity of the particle when the event fired.
	 *	@param	InParticleTime	The relative life of the particle when the event fired.
	 *	@param	InNormal		Normal vector of the collision in coordinate system of the returner. Zero=none.
	 *	@param	InTime			Time until hit, if line check.
	 *	@param	InItem			Primitive data item which was hit, INDEX_NONE=none.
	 *	@param	InBoneName		Name of bone we hit (for skeletal meshes).
	 */
	void ReportEventCollision(FName& InEventName, FLOAT InEmitterTime, FVector& InLocation, 
		FVector& InDirection, FVector& InVelocity, FLOAT InParticleTime, FVector& InNormal, 
		FLOAT InTime, INT InItem, FName& InBoneName);
	/** 
	 *	Record a world attractor collision event.
	 *
	 *	@param	InEventName		The name of the event that fired.
	 *	@param	InEmitterTime	The emitter time when the event fired.
	 *	@param	InLocation		The location of the particle when the event fired.
	 *	@param	InDirection		The direction of the particle when the event fired.
	 *	@param	InVelocity		The velocity of the particle when the event fired.
	 *	@param	InParticleTime	The relative life of the particle when the event fired.
	 *	@param	InNormal		Normal vector of the collision in coordinate system of the returner. Zero=none.
	 *	@param	InTime			Time until hit, if line check.
	 *	@param	InItem			Primitive data item which was hit, INDEX_NONE=none.
	 *	@param	InBoneName		Name of bone we hit (for skeletal meshes).
	 */
	void ReportEventAttractorCollision(FName& InEventName, FLOAT InEmitterTime, FVector& InLocation, 
		FVector& InDirection, FVector& InVelocity, FLOAT InParticleTime, FVector& InNormal, 
		FLOAT InTime, INT InItem, FName& InBoneName);
	/** 
	 *	Record a kismet event.
	 *
	 *	@param	InEventName				The name of the event that fired.
	 *	@param	InEmitterTime			The emitter time when the event fired.
	 *	@param	InLocation				The location of the particle when the event fired.
	 *	@param	InVelocity				The velocity of the particle when the event fired.
	 *	@param	bInUsePSysCompLocation	If TRUE, use the particle system component location as spawn location.
	 *	@param	InNormal				Normal vector of the collision in coordinate system of the returner. Zero=none.
	 */
	void ReportEventKismet(FName& InEventName, FLOAT InEmitterTime, FVector& InLocation, 
		FVector& InDirection, FVector& InVelocity, UBOOL bInUsePSysCompLocation, FVector& InNormal);


	/**
	 * Finds the replay clip of the specified ID number
	 *
	 * @return Returns the replay clip or NULL if none
	 */
	UParticleSystemReplay* FindReplayClipForIDNumber( const INT InClipIDNumber );

	/**
	 * Called by AnimNotify_Trails
	 *
	 * @param AnimNotifyData The AnimNotify_Trails which will have all of the various params on it
	 */
	void TrailsNotify(const UAnimNotify_Trails* AnimNotifyData);

	/**
	 * Called by AnimNotify_Trails
	 *
	 * @param AnimNotifyData The AnimNotify_Trails which will have all of the various params on it
	 */
	void TrailsNotifyTick(const UAnimNotify_Trails* AnimNotifyData);

	/**
	 * Called by AnimNotify_Trails
	 *
	 * @param AnimNotifyData The AnimNotify_Trails which will have all of the various params on it
	 */
	void TrailsNotifyEnd(const UAnimNotify_Trails* AnimNotifyData);
}

/**
 *	SetLODLevel - sets the LOD level to use for this instance.
 */
native final function			SetLODLevel(int InLODLevel);
native final function			SetEditorLODLevel(int InLODLevel);

/**
 *	GetLODLevel - gets the LOD level currently set.
 */
native final function int		GetLODLevel();
native final function int		GetEditorLODLevel();

native final function SetFloatParameter(name ParameterName, float Param);
native final function SetFloatRandParameter(name ParameterName, float Param, float ParamLow);
native final function SetVectorParameter(name ParameterName, vector Param);
native final function SetVectorRandParameter(name ParameterName, const out vector Param, const out vector ParamLow);
native final function SetColorParameter(name ParameterName, color Param);
native final function SetActorParameter(name ParameterName, actor Param);
native final function SetMaterialParameter(name ParameterName, MaterialInterface Param);

/**
 *	Retrieve the Float parameter value for the given name.
 *
 *	@param	InName		Name of the parameter
 *	@param	OutFloat	The value of the parameter found
 *
 *	@return	TRUE		Parameter was found - OutFloat is valid
 *			FALSE		Parameter was not found - OutFloat is invalid
 */
native function bool GetFloatParameter(const name InName, out float OutFloat);
/**
 *	Retrieve the Vector parameter value for the given name.
 *
 *	@param	InName		Name of the parameter
 *	@param	OutVector	The value of the parameter found
 *
 *	@return	TRUE		Parameter was found - OutVector is valid
 *			FALSE		Parameter was not found - OutVector is invalid
 */
native function bool GetVectorParameter(const name InName, out vector OutVector);
/**
 *	Retrieve the Color parameter value for the given name.
 *
 *	@param	InName		Name of the parameter
 *	@param	OutColor	The value of the parameter found
 *
 *	@return	TRUE		Parameter was found - OutColor is valid
 *			FALSE		Parameter was not found - OutColor is invalid
 */
native function bool GetColorParameter(const name InName, out color OutColor);
/**
 *	Retrieve the Actor parameter value for the given name.
 *
 *	@param	InName		Name of the parameter
 *	@param	OutActor	The value of the parameter found
 *
 *	@return	TRUE		Parameter was found - OutActor is valid
 *			FALSE		Parameter was not found - OutActor is invalid
 */
native function bool GetActorParameter(const name InName, out actor OutActor);
/**
 *	Retrieve the Material parameter value for the given name.
 *
 *	@param	InName		Name of the parameter
 *	@param	OutMaterial	The value of the parameter found
 *
 *	@return	TRUE		Parameter was found - OutMaterial is valid
 *			FALSE		Parameter was not found - OutMaterial is invalid
 */
native function bool GetMaterialParameter(const name InName, out MaterialInterface OutMaterial);

/** clears the specified parameter, returning it to the default value set in the template
 * @param ParameterName name of parameter to remove
 * @param ParameterType type of parameter to remove; if omitted or PSPT_None is specified, all parameters with the given name are removed
 */
native final function ClearParameter(name ParameterName, optional EParticleSysParamType ParameterType);

/** calls ActivateSystem() or DeactivateSystem() only if the component is not already activated/deactivated
 * necessary because ActivateSystem() resets already active emitters so it shouldn't be called multiple times on looping effects
 * @param bNowActive - whether the system should be active
 * @param bFlagAsJustAttached - if TRUE, then pass the 'JustAttached' value into the ActivateSystem call
 */
native final function SetActive(bool bNowActive, optional bool bFlagAsJustAttached);

/** stops the emitter, detaches the component, and resets the component's properties to the values of its template */
native final function ResetToDefaults();

/** 
 *	Calls SetStopSpawning with the given emitter instance passing in the given value.
 *
 *	@param	InEmitterIndex		The index of the emitter instance to call SetHaltSpawning on; -1 for ALL
 *	@param	bInStopSpawning		The value to pass into the EmitterInstance SetHaltSpawning call
 */
native final function SetStopSpawning(int InEmitterIndex, bool bInStopSpawning);

defaultproperties
{
	LightEnvironmentClass=class'ParticleLightEnvironmentComponent'
	// Share light environment with many other particle components by default to be inexpensive
	// This means particle lighting will update fairly slowly by default as well
	MaxLightEnvironmentPooledReuses=10
	bTickInEditor=true

	MaxTimeBeforeForceUpdateTransform=5

	bAutoActivate=true
	bResetOnDetach=false
	OldPosition=(X=0,Y=0,Z=0)
	PartSysVelocity=(X=0,Y=0,Z=0)
	WarmupTime=0

	SecondsBeforeInactive=1.0

	bSkipUpdateDynamicDataDuringTick=false

	TickGroup=TG_DuringAsyncWork

	bIsViewRelevanceDirty=true

	CustomTimeDilation=1.f

	EditorDetailMode=-1
}
