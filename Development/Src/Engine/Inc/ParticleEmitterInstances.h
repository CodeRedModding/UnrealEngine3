/*=============================================================================
	UnParticleEmitterInstances.h: 
	Particle emitter instance definitions/ macros.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
/*-----------------------------------------------------------------------------
	Helper macros.
-----------------------------------------------------------------------------*/
//	Macro fun.

/*-----------------------------------------------------------------------------
	Forward declarations
-----------------------------------------------------------------------------*/
//	Emitter and module types
class UParticleEmitter;
class UParticleSpriteEmitter;
class UParticleModule;
// Data types
class UParticleModuleTypeDataMesh;
class UParticleModuleTypeDataTrail;
class UParticleModuleTypeDataBeam;
class UParticleModuleTypeDataBeam2;
class UParticleModuleTypeDataTrail2;
class UParticleModuleTypeDataRibbon;
class UParticleModuleTypeDataAnimTrail;

class UStaticMeshComponent;
class UParticleSystemComponent;

class UParticleModuleBeamSource;
class UParticleModuleBeamTarget;
class UParticleModuleBeamNoise;

class UParticleModuleTrailSource;
class UParticleModuleTrailSpawn;
class UParticleModuleTrailTaper;

class UParticleModuleSpawnPerUnit;

class UParticleModuleOrientationAxisLock;

class UParticleLODLevel;

class FParticleSystemSceneProxy;
class FParticleDynamicData;
struct FDynamicBeam2EmitterData;
struct FDynamicTrail2EmitterData;

class UAnimNotify_Trails;

/*-----------------------------------------------------------------------------
	Particle Emitter Instance type
-----------------------------------------------------------------------------*/
struct FParticleEmitterInstanceType
{
	const TCHAR* Name;
	FParticleEmitterInstanceType* Super;

	/** Constructor */
	FParticleEmitterInstanceType(const TCHAR* InName, FParticleEmitterInstanceType* InSuper) :
		  Name(InName)
		, Super(InSuper)
	{
	}

	/** 
	 *	IsA 
	 *	@param	Type			The type to check for
	 *	@return UBOOL	TRUE	if the type is exactly or is derived from the given type.
	 *					FALSE	if not
	 */
	FORCEINLINE UBOOL IsA(FParticleEmitterInstanceType& Type)
	{
		FParticleEmitterInstanceType* CurrentSuper = this;
		while (CurrentSuper)
		{
			if (CurrentSuper == &Type)
			{
				return TRUE;
			}
			CurrentSuper = CurrentSuper->Super;
		}

		return FALSE;
	}
};

#define DECLARE_PARTICLEEMITTERINSTANCE_TYPE(TypeName, SuperType)	\
	typedef SuperType SuperResource;								\
	static FParticleEmitterInstanceType StaticType;					\
	virtual FParticleEmitterInstanceType* Type()					\
	{																\
		return &StaticType;											\
	}

#define IMPLEMENT_PARTICLEEMITTERINSTANCE_TYPE(TypeName)			\
	FParticleEmitterInstanceType TypeName::StaticType(				\
		TEXT(#TypeName), &TypeName::SuperResource::StaticType);

struct FLODBurstFired
{
    TArray<UBOOL> Fired;
};

/*-----------------------------------------------------------------------------
	FParticleEmitterInstance
-----------------------------------------------------------------------------*/
// This is the base-class 'IMPLEMENT' line
//FParticleEmitterInstanceType FParticleEmitterInstance::StaticType(TEXT("FParticleEmitterInstance"),NULL);
struct FParticleEmitterInstance
{
public:
	static FParticleEmitterInstanceType	StaticType;

	/** The maximum DeltaTime allowed for updating PeakActiveParticle tracking.
	 *	Any delta time > this value will not impact active particle tracking.
	 */
	static const FLOAT PeakActiveParticleUpdateDelta;

	/** The template this instance is based on.							*/
	UParticleSpriteEmitter* SpriteTemplate;
	/** The component who owns it.										*/
	UParticleSystemComponent* Component;
	/** The index of the currently set LOD level.						*/
	INT CurrentLODLevelIndex;
	/** The currently set LOD level.									*/
	UParticleLODLevel* CurrentLODLevel;
	/** The offset to the TypeDate payload in the particle data.		*/
	INT TypeDataOffset;
	/** The offset to the TypeDate instance payload.					*/
	INT TypeDataInstanceOffset;
	/** The offset to the SubUV payload in the particle data.			*/
	INT SubUVDataOffset;
	/** The offset to the dynamic parameter payload in the particle data*/
	INT DynamicParameterDataOffset;
	/** The offset to the Orbit module payload in the particle data.	*/
	INT OrbitModuleOffset;
	/** The offset to the Camera payload in the particle data.			*/
	INT CameraPayloadOffset;
	/** The location of the emitter instance							*/
	FVector Location;
	/** If TRUE, kill this emitter instance when it is deactivated.		*/
	BITFIELD KillOnDeactivate:1;
	/** if TRUE, kill this emitter instance when it has completed.		*/
	BITFIELD bKillOnCompleted:1;
	/** Whether this emitter requires sorting as specified by artist.	*/
	BITFIELD bRequiresSorting:1;
	/** If TRUE, halt spawning for this instance.						*/
	BITFIELD bHaltSpawning:1;
	/** If TRUE, the emitter has modules that require loop notification.*/
	BITFIELD bRequiresLoopNotification:1;
	/** The sort mode to use for this emitter as specified by artist.	*/
	INT SortMode;
	/** Pointer to the particle data array.								*/
	BYTE* ParticleData;
	/** Pointer to the particle index array.							*/
	WORD* ParticleIndices;
	/** Map module pointers to their offset into the particle data.		*/
	TMap<UParticleModule*,UINT> ModuleOffsetMap;
	/** Pointer to the instance data array.								*/
	BYTE* InstanceData;
	/** The size of the Instance data array.							*/
	INT InstancePayloadSize;
	/** Map module pointers to their offset into the instance data.		*/
	TMap<UParticleModule*,UINT> ModuleInstanceOffsetMap;
	/** The offset to the particle data.								*/
	INT PayloadOffset;
	/** The total size of a particle (in bytes).						*/
	INT ParticleSize;
	/** The stride between particles in the ParticleData array.			*/
	INT ParticleStride;
	/** The number of particles currently active in the emitter.		*/
	INT ActiveParticles;
	/** The maximum number of active particles that can be held in 
	 *	the particle data array.
	 */
	INT MaxActiveParticles;
	/** The fraction of time left over from spawning.					*/
	FLOAT SpawnFraction;
	/** The number of seconds that have passed since the instance was
	 *	created.
	 */
	FLOAT SecondsSinceCreation;
	/** */
	FLOAT EmitterTime;
	/** The previous location of the instance.							*/
	FVector OldLocation;
	/** The bounding box for the particles.								*/
	FBox ParticleBoundingBox;
	/** The BurstFire information.										*/
	TArray<struct FLODBurstFired> BurstFired;
	/** The number of loops completed by the instance.					*/
	INT LoopCount;
	/** Flag indicating if the render data is dirty.					*/
	INT IsRenderDataDirty;
	/** The AxisLock module - to avoid finding each Tick.				*/
	UParticleModuleOrientationAxisLock* Module_AxisLock;
	/** The current duration fo the emitter instance.					*/
	FLOAT EmitterDuration;
	/** The emitter duration at each LOD level for the instance.		*/
	TArray<FLOAT> EmitterDurations;
	/** The emitter's delay for the current loop		*/
	FLOAT CurrentDelay;


	/** The number of triangles to render								*/
	INT	TrianglesToRender;
	INT MaxVertexIndex;

	/** The material to render this instance with.						*/
	UMaterialInterface* CurrentMaterial;
#if !FINAL_RELEASE
	/** Number of events this emitter has generated... */
	INT EventCount;
	INT MaxEventCount;
#endif	//#if !FINAL_RELEASE

	/** Constructor	*/
	FParticleEmitterInstance();

	/** Destructor	*/
	virtual ~FParticleEmitterInstance();

	// Type interface
	virtual FParticleEmitterInstanceType* Type()			{	return &StaticType;		}

#if STATS
	virtual EParticleStats GetGameThreadDataStat() { return STAT_DynamicUntrackedGTMem; }
	void PreDestructorCall();
#endif

	//
	virtual void SetKillOnDeactivate(UBOOL bKill);
	virtual void SetKillOnCompleted(UBOOL bKill);
	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, UBOOL bClearResources = TRUE);
	virtual void Init();
	virtual UBOOL Resize(INT NewMaxActiveParticles, UBOOL bSetMaxActiveCount = TRUE);
	virtual void Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning);
	/**
	 *	Tick sub-function that handles EmitterTime setup, looping, etc.
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 *
	 *	@return	FLOAT				The EmitterDelay
	 */
	virtual FLOAT Tick_EmitterTimeSetup(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel);
	/**
	 *	Tick sub-function that handles spawning of particles
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 *	@param	bSuppressSpawning	TRUE if spawning has been supressed on the owning particle system component
	 *	@param	bFirstTime			TRUE if this is the first time the instance has been ticked
	 *
	 *	@return	FLOAT				The SpawnFraction remaining
	 */
	virtual FLOAT Tick_SpawnParticles(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel, UBOOL bSuppressSpawning, UBOOL bFirstTime);
	/**
	 *	Tick sub-function that handles module pre updates
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_ModulePreUpdate(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel);
	/**
	 *	Tick sub-function that handles module updates
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_ModuleUpdate(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel);
	/**
	 *	Tick sub-function that handles module post updates
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_ModulePostUpdate(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel);
	/**
	 *	Tick sub-function that handles module FINAL updates
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_ModuleFinalUpdate(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel);

	/**
	 *	Set the LOD to the given index
	 *
	 *	@param	InLODIndex			The index of the LOD to set as current
	 *	@param	bInFullyProcess		If TRUE, process burst lists, etc.
	 */
	virtual void SetCurrentLODIndex(INT InLODIndex, UBOOL bInFullyProcess);

	virtual void Rewind();
	virtual FBox GetBoundingBox();
	virtual void UpdateBoundingBox(FLOAT DeltaTime);
	virtual UINT RequiredBytes();
	virtual BYTE* GetModuleInstanceData(UParticleModule* Module);
	virtual BYTE* GetTypeDataModuleInstanceData();
	virtual UINT CalculateParticleStride(UINT ParticleSize);
	virtual void ResetBurstList();
	virtual FLOAT GetCurrentBurstRateOffset(FLOAT& DeltaTime, INT& Burst);
	virtual void ResetParticleParameters(FLOAT DeltaTime, DWORD StatId);
	void CalculateOrbitOffset(FOrbitChainModuleInstancePayload& Payload, 
		FVector& AccumOffset, FVector& AccumRotation, FVector& AccumRotationRate, 
		FLOAT DeltaTime, FVector& Result, FMatrix& RotationMat);
	virtual void UpdateOrbitData(FLOAT DeltaTime);
	virtual void ParticlePrefetch();

	/**
	 *	Spawn particles for this emitter instance
	 *
	 *	@param	DeltaTime		The time slice to spawn over
	 *
	 *	@return	FLOAT			The leftover fraction of spawning
	 */
	virtual FLOAT Spawn(FLOAT DeltaTime);
	/**
	 *	Spawn/burst the given particles...
	 *
	 *	@param	DeltaTime		The time slice to spawn over.
	 *	@param	InSpawnCount	The number of particles to forcibly spawn.
	 *	@param	InBurstCount	The number of particles to forcibly burst.
	 *	@param	InLocation		The location to spawn at.
	 *	@param	InVelocity		OPTIONAL velocity to have the particle inherit.
	 *
	 */
	virtual void ForceSpawn(FLOAT DeltaTime, INT InSpawnCount, INT InBurstCount, FVector& InLocation, FVector& InVelocity);
	virtual FLOAT Spawn(FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime, INT Burst = 0, FLOAT BurstTime = 0.0f);
	void CheckSpawnCount(UBOOL bIsSubUV, INT InNewCount, INT InMaxCount);
	virtual void PreSpawn(FBaseParticle* Particle);
	virtual UBOOL HasCompleted();
	virtual void PostSpawn(FBaseParticle* Particle, FLOAT InterpolationPercentage, FLOAT SpawnTime);
	virtual void KillParticles();
	/**
	 *	Kill the particle at the given instance
	 *
	 *	@param	Index		The index of the particle to kill.
	 */
	virtual void KillParticle(INT Index);

	/**
	 *	Force kill all particles in the emitter.
	 *
	 *	@param	bFireEvents		If TRUE, fire events for the particles being killed.
	 */
	virtual void KillParticlesForced(UBOOL bFireEvents = FALSE);

	/** Set the HaltSpawning flag */
	virtual void SetHaltSpawning(UBOOL bInHaltSpawning)
	{
		bHaltSpawning = bInHaltSpawning;
	}

	virtual void RemovedFromScene();
	virtual FBaseParticle* GetParticle(INT Index);
	/**
	 *	Get the physical index of the particle at the given index
	 *	(ie, the contents of ParticleIndices[InIndex])
	 *
	 *	@param	InIndex			The index of the particle of interest
	 *
	 *	@return	INT				The direct index of the particle
	 */
	FORCEINLINE INT GetParticleDirectIndex(INT InIndex)
	{
		if (InIndex < MaxActiveParticles)
		{
			return ParticleIndices[InIndex];
		}
		return -1;
	}
	/**
	 *	Get the particle at the given direct index
	 *
	 *	@param	InDirectIndex		The direct index of the particle of interest
	 *
	 *	@return	FBaseParticle*		The particle, if valid index was given; NULL otherwise
	 */
	virtual FBaseParticle* GetParticleDirect(INT InDirectIndex);

	/**
	 *	Calculates the emitter duration for the instance.
	 */
	void SetupEmitterDuration();
	
	/**
	 * Returns whether the system has any active particles.
	 *
	 * @return TRUE if there are active particles, FALSE otherwise.
	 */
	UBOOL HasActiveParticles()
	{
		return ActiveParticles > 0;
	}

	/**
	 *	Checks some common values for GetDynamicData validity
	 *
	 *	@return	UBOOL		TRUE if GetDynamicData should continue, FALSE if it should return NULL
	 */
	virtual UBOOL IsDynamicDataRequired(UParticleLODLevel* CurrentLODLevel);

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(UBOOL bSelected)
	{
		return NULL;
	}

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		TRUE if the particle system component is selected
	 */
	virtual UBOOL UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected)
	{
		// Base class does nothing...
		return FALSE;
	}

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData()
	{
		return NULL;
	}

	/**
	 *	Retrieve the allocated size of this instance.
	 *
	 *	@param	OutNum			The size of this instance
	 *	@param	OutMax			The maximum size of this instance
	 */
	virtual void GetAllocatedSize(INT& OutNum, INT& OutMax)
	{
		OutNum = 0;
		OutMax = 0;
	}
	
	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 *	@param	bInExclusiveResourceSizeMode	UObject::GExclusiveResourceSizeMode value
	 *
	 *	@return	INT			The size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize(UBOOL bInExclusiveResourceSizeMode)
	{
		return 0;
	}

	/**
	 * Init physics data associated with this emitter.
	 */
	virtual void InitPhysicsParticleData(DWORD NumParticles) {}
	/**
	 * Sync physics data associated with this emitter.
	 */
	virtual void SyncPhysicsData(void) {}

	/**
	 *	Process received events.
	 */
	virtual void ProcessParticleEvents(FLOAT DeltaTime, UBOOL bSuppressSpawning);

	/**
	 *	Called when the particle system is deactivating...
	 */
	virtual void OnDeactivateSystem()
	{
	}

	/**
	 * Called by AnimNotify_Trails
	 *
	 * @param AnimNotifyData The AnimNotify_Trails which will have all of the various params on it
	 */
	virtual void TrailsNotify(const UAnimNotify_Trails* AnimNotifyData)
	{
	}

	/**
	 * Called by AnimNotify_Trails
	 *
	 * @param AnimNotifyData The AnimNotify_Trails which will have all of the various params on it
	 */
	virtual void TrailsNotifyTick(const UAnimNotify_Trails* AnimNotifyData)
	{
	}

	/**
	 * Called by AnimNotify_Trails
	 *
	 * @param AnimNotifyData The AnimNotify_Trails which will have all of the various params on it
	 */
	virtual void TrailsNotifyEnd(const UAnimNotify_Trails* AnimNotifyData)
	{
	}

protected:

	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns TRUE if successful
	 */
	virtual UBOOL FillReplayData( FDynamicEmitterReplayDataBase& OutData );

};

/*-----------------------------------------------------------------------------
	Helper functions.
-----------------------------------------------------------------------------*/
/**
 * Safely casts a FParticleEmitterInstance to a specific type.
 *
 * @param 	template	type of result
 * @param	Src			instance to be cast
 * @return	Src	if instance is of the right type, NULL otherwise
 */
template<class T> T* CastEmitterInstance(FParticleEmitterInstance* Src)
{
	return Src && Src->Type()->IsA(T::StaticType) ? (T*)Src : NULL;
}

/**
 * Casts a FParticleEmitterInstance to a specific type and asserts if Src is 
 * not a subclass of the requested type.
 *
 * @param 	template	type of result
 * @param	Src			instance to be cast
 * @return	Src cast to the destination type
 */
template<class T, class U> T* CastEmitterInstanceChecked(U* Src)
{
	if (!Src || !Src->Type()->IsA(T::StaticType))
	{
		appErrorf(TEXT("Cast of %s to %s failed"), Src ? Src->Type()->Name : TEXT("NULL"), T::StaticType.Name);
	}
	return (T*)Src;
}

/*-----------------------------------------------------------------------------
	ParticleSpriteEmitterInstance
-----------------------------------------------------------------------------*/
struct FParticleSpriteEmitterInstance : public FParticleEmitterInstance
{
	DECLARE_PARTICLEEMITTERINSTANCE_TYPE(FParticleSpriteEmitterInstance, FParticleEmitterInstance);

	/** Constructor	*/
	FParticleSpriteEmitterInstance();

	/** Destructor	*/
	virtual ~FParticleSpriteEmitterInstance();

#if STATS
	virtual EParticleStats GetGameThreadDataStat() { return STAT_DynamicSpriteGTMem; }
#endif

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(UBOOL bSelected);

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		TRUE if the particle system component is selected
	 */
	virtual UBOOL UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected);

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData();

	/**
	 *	Retrieve the allocated size of this instance.
	 *
	 *	@param	OutNum			The size of this instance
	 *	@param	OutMax			The maximum size of this instance
	 */
	virtual void GetAllocatedSize(INT& OutNum, INT& OutMax);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 *	@param	bInExclusiveResourceSizeMode	UObject::GExclusiveResourceSizeMode value
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize(UBOOL bInExclusiveResourceSizeMode);

protected:

	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns TRUE if successful
	 */
	virtual UBOOL FillReplayData( FDynamicEmitterReplayDataBase& OutData );

};

/*-----------------------------------------------------------------------------
	ParticleSpriteSubUVEmitterInstance
-----------------------------------------------------------------------------*/
struct FParticleSpriteSubUVEmitterInstance : public FParticleEmitterInstance
{
	DECLARE_PARTICLEEMITTERINSTANCE_TYPE(FParticleSpriteSubUVEmitterInstance, FParticleEmitterInstance);

	/** Constructor	*/
	FParticleSpriteSubUVEmitterInstance();

	/** Destructor	*/
	virtual ~FParticleSpriteSubUVEmitterInstance();

#if STATS
	virtual EParticleStats GetGameThreadDataStat() { return STAT_DynamicSubUVGTMem; }
#endif


	virtual void KillParticles();

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(UBOOL bSelected);

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		TRUE if the particle system component is selected
	 */
	virtual UBOOL UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected);

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData();

	/**
	 *	Retrieve the allocated size of this instance.
	 *
	 *	@param	OutNum			The size of this instance
	 *	@param	OutMax			The maximum size of this instance
	 */
	virtual void GetAllocatedSize(INT& OutNum, INT& OutMax);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 *	@param	bInExclusiveResourceSizeMode	UObject::GExclusiveResourceSizeMode value
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize(UBOOL bInExclusiveResourceSizeMode);

protected:
	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns TRUE if successful
	 */
	virtual UBOOL FillReplayData( FDynamicEmitterReplayDataBase& OutData );
};

/*-----------------------------------------------------------------------------
	ParticleMeshEmitterInstance
-----------------------------------------------------------------------------*/
struct FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
	DECLARE_PARTICLEEMITTERINSTANCE_TYPE(FParticleMeshEmitterInstance, FParticleEmitterInstance);

	UParticleModuleTypeDataMesh* MeshTypeData;
	INT MeshComponentIndex;
	UBOOL MeshRotationActive;
	INT MeshRotationOffset;
	UBOOL bIgnoreComponentScale;

	/** The materials to render this instance with.	*/
	TArray<UMaterialInterface*> CurrentMaterials;

	/** Constructor	*/
	FParticleMeshEmitterInstance();

#if STATS
	virtual EParticleStats GetGameThreadDataStat() { return STAT_DynamicMeshGTMem; }
#endif


	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, UBOOL bClearResources = TRUE);
	virtual void Init();
	virtual UBOOL Resize(INT NewMaxActiveParticles, UBOOL bSetMaxActiveCount = TRUE);
	virtual void Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning);
	virtual void UpdateBoundingBox(FLOAT DeltaTime);
	virtual UINT RequiredBytes();
	virtual void PostSpawn(FBaseParticle* Particle, FLOAT InterpolationPercentage, FLOAT SpawnTime);

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(UBOOL bSelected);

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		TRUE if the particle system component is selected
	 */
	virtual UBOOL UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected);

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData();

	/**
	 *	Retrieve the allocated size of this instance.
	 *
	 *	@param	OutNum			The size of this instance
	 *	@param	OutMax			The maximum size of this instance
	 */
	virtual void GetAllocatedSize(INT& OutNum, INT& OutMax);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 *	@param	bInExclusiveResourceSizeMode	UObject::GExclusiveResourceSizeMode value
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize(UBOOL bInExclusiveResourceSizeMode);

protected:

	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns TRUE if successful
	 */
	virtual UBOOL FillReplayData( FDynamicEmitterReplayDataBase& OutData );
};

#if WITH_NOVODEX
/*-----------------------------------------------------------------------------
	ParticleMeshPhysXEmitterInstance
-----------------------------------------------------------------------------*/
struct FParticleMeshPhysXEmitterInstance : public FParticleMeshEmitterInstance
{
	DECLARE_PARTICLEEMITTERINSTANCE_TYPE(FParticleMeshPhysXEmitterInstance, FParticleMeshEmitterInstance);

	FParticleMeshPhysXEmitterInstance(class UParticleModuleTypeDataMeshPhysX &TypeData);
	virtual ~FParticleMeshPhysXEmitterInstance();

	UParticleModuleTypeDataMeshPhysX	&PhysXTypeData;

	virtual void Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning);
	
	/** Does nothing for this emitter instance type. */
	virtual void ParticlePrefetch();

	virtual FLOAT Spawn(FLOAT DeltaTime);
	virtual FLOAT Spawn(FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime, INT Burst = 0, FLOAT BurstTime = 0.0f);

	virtual void RemovedFromScene();

	/** Forwards call to correspnding FParticleMeshPhysXEmitterRenderInstance object. */
	virtual FDynamicEmitterDataBase* GetDynamicData(UBOOL bSelected);

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		TRUE if the particle system component is selected
	 */
	virtual UBOOL UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected);

	/** Lifetime particle reduction is performed by FPhysXParticleSet */
	virtual void KillParticles() {}

	/** Lifetime particle reduction is performed by FPhysXParticleSet */
	virtual void KillParticlesForced(UBOOL bFireEvents = FALSE);

	void RemoveParticles();
	/** Fast active particle removal (replace with last) */
	void RemoveParticleFromActives(INT Index);
	/** Executes spawning of particles in sync with PhysX SDK */
	void SpawnSyncPhysX();

	/** Provides LOD relevant camera position. TODO: figure out how this should be done properly. */
	UBOOL GetLODOrigin(FVector& OutLODOrigin);
	
	/** Provides LOD relevant camera near plane. TODO: figure out how this should be done properly. */
	UBOOL GetLODNearClippingPlane(FPlane& OutClippingPlane);

	/** Returns smoothed estimate for rate*lifetime in [#particles]*/
	INT GetSpawnVolumeEstimate();

	/** Sets spawn budget for this emitter instance in [#particles] */
	void SetEmissionBudget(INT Budget);

	/** See ParticleModuleTypeDataPhysX.uc */
	FLOAT GetWeightForSpawnLod();

    /** This will look at all of the emitters in this particle system and return their locations in world space and their spawn time as the W compoenent. **/
	void GetLocationsOfAllParticles( TArray<FVector4>& LocationsOfEmitters );
	virtual void UpdateBoundingBox(FLOAT DeltaTime);

	INT NumSpawnedParticles;
protected:

	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns TRUE if successful
	 */
	virtual UBOOL FillReplayData( FDynamicEmitterReplayDataBase& OutData );


private:

	void CleanUp();
	UBOOL TryConnect();
	class FPhysXParticleSystem& GetPSys();
	FParticleSystemSceneProxy* GetSceneProxy();
	void AsyncProccessSpawnedParticles(FLOAT DeltaTime, INT FirstIndex);
	void PostSpawnVerticalLod();
	void SpawnEstimateUpdate(FLOAT DeltaTime, INT NumSpawnedParticles, FLOAT LifetimeSum);
	void OverwriteUnsupported();
	void RestoreUnsupported();
	INT LodEmissionBudget;
	FLOAT LodEmissionRemainder;

	struct SpawnEstimateSample
	{
		FLOAT DeltaTime;
		INT NumSpawned;
		FLOAT LifetimeSum;
	};

	TArray<SpawnEstimateSample> SpawnEstimateSamples;
	FLOAT SpawnEstimateTime;
	FLOAT SpawnEstimateRate;
	FLOAT SpawnEstimateLife;

	UBOOL Stored_bUseLocalSpace;
	class FPhysXParticleSetMesh* PSet;
};

/*-----------------------------------------------------------------------------
	ParticleSpritePhysXEmitterInstance
-----------------------------------------------------------------------------*/
struct FParticleSpritePhysXEmitterInstance : public FParticleSpriteSubUVEmitterInstance
{
	DECLARE_PARTICLEEMITTERINSTANCE_TYPE(FParticleSpritePhysXEmitterInstance, FParticleSpriteSubUVEmitterInstance);
    
	FParticleSpritePhysXEmitterInstance(class UParticleModuleTypeDataPhysX &TypeData);
	virtual ~FParticleSpritePhysXEmitterInstance();

	class UParticleModuleTypeDataPhysX&		PhysXTypeData;

	virtual void Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning);

	virtual void UpdateBoundingBox(FLOAT DeltaTime);

	virtual FLOAT Spawn(FLOAT DeltaTime);
	virtual FLOAT Spawn(FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime, INT Burst = 0, FLOAT BurstTime = 0.0f);

	virtual void RemovedFromScene(void);

	/** Executes method of base class, but forces bUseLocalSpace to FALSE */
	virtual FDynamicEmitterDataBase* GetDynamicData(UBOOL bSelected);

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		TRUE if the particle system component is selected
	 */
	virtual UBOOL UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected);

	/** Lifetime particle reduction is performed by FPhysXParticleSet */
	virtual void KillParticles() {}

	/** Particle reduction modules are currently not supported */
	virtual void KillParticle(INT Index) {}

	/**
	 *	Force kill all particles in the emitter.
	 *
	 *	@param	bFireEvents		If TRUE, fire events for the particles being killed.
	 */
	virtual void KillParticlesForced(UBOOL bFireEvents = FALSE);

	// PhysX sprite emitter requires SubUV
	virtual UINT RequiredBytes();

	/** Removes all active particles*/
	void RemoveParticles();

	/** Fast active particle removal (replace with last) */
	void RemoveParticleFromActives(INT Index);

	/** Executes spawning of particles in sync with PhysX SDK */
	void SpawnSyncPhysX();

	/** Provides LOD relevant camera position. TODO: figure out how this should be done properly. */
	UBOOL GetLODOrigin(FVector& OutLODOrigin);

	/** Provides LOD relevant camera near plane. TODO: figure out how this should be done properly. */
	UBOOL GetLODNearClippingPlane(FPlane& OutClippingPlane);

	/** Returns smoothed estimate for rate*lifetime in [#particles]*/
	INT GetSpawnVolumeEstimate();

	/** Sets spawn budget for this emitter instance in [#particles] */
	void SetEmissionBudget(INT Budget);

	/** See ParticleModuleTypeDataPhysX.uc */
	FLOAT GetWeightForSpawnLod();

	FORCEINLINE void SetBounds(const FBox& InBounds) { ParticleBoundingBox = InBounds; }

	INT NumSpawnedParticles;
	INT DensityPayloadOffset;

	/** This will look at all of the emitters in this particle system and return their locations in world space. **/
	void GetLocationsOfAllParticles( TArray<FVector4>& LocationsOfEmitters );

private:
	
	void CleanUp();
	UBOOL TryConnect();
	class FPhysXParticleSystem& GetPSys();
	FParticleSystemSceneProxy* GetSceneProxy();
	void PostSpawnVerticalLod();
	void AsyncProccessSpawnedParticles(FLOAT DeltaTime, INT FirstIndex);
	void SpawnEstimateUpdate(FLOAT DeltaTime, INT NumSpawnedParticles, FLOAT LifetimeSum);
	void OverwriteUnsupported();
	void RestoreUnsupported();
	
	INT LodEmissionBudget;
	FLOAT LodEmissionRemainder;

	struct SpawnEstimateSample
	{
		FLOAT DeltaTime;
		INT NumSpawned;
		FLOAT LifetimeSum;
	};

	TArray<SpawnEstimateSample> SpawnEstimateSamples;
	FLOAT SpawnEstimateTime;
	FLOAT SpawnEstimateRate;
	FLOAT SpawnEstimateLife;

	UBOOL Stored_bUseLocalSpace;

	class FPhysXParticleSetSprite* PSet;
};
#endif	//#if WITH_NOVODEX

/*-----------------------------------------------------------------------------
	ParticleBeam2EmitterInstance
-----------------------------------------------------------------------------*/
struct FParticleBeam2EmitterInstance : public FParticleEmitterInstance
{
	DECLARE_PARTICLEEMITTERINSTANCE_TYPE(FParticleBeam2EmitterInstance, FParticleEmitterInstance);

	UParticleModuleTypeDataBeam2*	BeamTypeData;
	UParticleModuleBeamSource*		BeamModule_Source;
	UParticleModuleBeamTarget*		BeamModule_Target;
	UParticleModuleBeamNoise*		BeamModule_Noise;
	UParticleModuleBeamModifier*	BeamModule_SourceModifier;
	INT								BeamModule_SourceModifier_Offset;
	UParticleModuleBeamModifier*	BeamModule_TargetModifier;
	INT								BeamModule_TargetModifier_Offset;

	TArray<UParticleModuleTypeDataBeam2*>	LOD_BeamTypeData;
	TArray<UParticleModuleBeamSource*>		LOD_BeamModule_Source;
	TArray<UParticleModuleBeamTarget*>		LOD_BeamModule_Target;
	TArray<UParticleModuleBeamNoise*>		LOD_BeamModule_Noise;
	TArray<UParticleModuleBeamModifier*>	LOD_BeamModule_SourceModifier;
	TArray<UParticleModuleBeamModifier*>	LOD_BeamModule_TargetModifier;

	UBOOL							FirstEmission;
	INT								LastEmittedParticleIndex;
	INT								TickCount;
	INT								ForceSpawnCount;
	/** The method to utilize when forming the beam.							*/
	INT								BeamMethod;
	/** How many times to tile the texture along the beam.						*/
	TArray<INT>						TextureTiles;
	/** The number of live beams												*/
	INT								BeamCount;
	/** The actor to get the source point from.									*/
	AActor*							SourceActor;
	/** The emitter to get the source point from.								*/
	FParticleEmitterInstance*		SourceEmitter;
	/** User set Source points of each beam - primarily for weapon effects.		*/
	TArray<FVector>					UserSetSourceArray;
	/** User set Source tangents of each beam - primarily for weapon effects.	*/
	TArray<FVector>					UserSetSourceTangentArray;
	/** User set Source strengths of each beam - primarily for weapon effects.	*/
	TArray<FLOAT>					UserSetSourceStrengthArray;
	/** The distance of each beam, if utilizing the distance method.			*/
	TArray<FLOAT>					DistanceArray;
	/** The target point of each beam, when using the end point method.			*/
	TArray<FVector>					TargetPointArray;
	/** The target tangent of each beam, when using the end point method.		*/
	TArray<FVector>					TargetTangentArray;
	/** User set Target strengths of each beam - primarily for weapon effects.	*/
	TArray<FLOAT>					UserSetTargetStrengthArray;
	/** The actor to get the target point from.									*/
	AActor*							TargetActor;
	/** The emitter to get the Target point from.								*/
	FParticleEmitterInstance*		TargetEmitter;
	/** The target point sources of each beam, when using the end point method.	*/
	TArray<FName>					TargetPointSourceNames;
	/** User set target points of each beam - primarily for weapon effects.		*/
	TArray<FVector>					UserSetTargetArray;
	/** User set target tangents of each beam - primarily for weapon effects.	*/
	TArray<FVector>					UserSetTargetTangentArray;

	/** The number of vertices and triangles, for rendering						*/
	INT								VertexCount;
	INT								TriangleCount;
	TArray<INT>						BeamTrianglesPerSheet;

	/** Constructor	*/
	FParticleBeam2EmitterInstance();

	/** Destructor	*/
	virtual ~FParticleBeam2EmitterInstance();

#if STATS
	virtual EParticleStats GetGameThreadDataStat() { return STAT_DynamicBeamGTMem; }
#endif

	// Accessors
    virtual void SetBeamType(INT NewMethod);
    virtual void SetTessellationFactor(FLOAT NewFactor);
    virtual void SetEndPoint(FVector NewEndPoint);
    virtual void SetDistance(FLOAT Distance);
    virtual void SetSourcePoint(FVector NewSourcePoint,INT SourceIndex);
    virtual void SetSourceTangent(FVector NewTangentPoint,INT SourceIndex);
    virtual void SetSourceStrength(FLOAT NewSourceStrength,INT SourceIndex);
    virtual void SetTargetPoint(FVector NewTargetPoint,INT TargetIndex);
    virtual void SetTargetTangent(FVector NewTangentPoint,INT TargetIndex);
    virtual void SetTargetStrength(FLOAT NewTargetStrength,INT TargetIndex);

	//
	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, UBOOL bClearResources = TRUE);
	virtual void Init();
	virtual void Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning);
	/**
	 *	Tick sub-function that handles module post updates
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_ModulePostUpdate(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel);

	/**
	 *	Set the LOD to the given index
	 *
	 *	@param	InLODIndex			The index of the LOD to set as current
	 *	@param	bInFullyProcess		If TRUE, process burst lists, etc.
	 */
	virtual void SetCurrentLODIndex(INT InLODIndex, UBOOL bInFullyProcess);

	virtual void UpdateBoundingBox(FLOAT DeltaTime);
	virtual UINT RequiredBytes();
	virtual FLOAT Spawn(FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime, INT Burst = 0, FLOAT BurstTime = 0.0f);
	virtual void PreSpawn(FBaseParticle* Particle);
	virtual void KillParticles();
	void SetupBeamModules();
	void SetupBeamModifierModules();
	/**
	 *	Setup the offsets to the BeamModifier modules...
	 *	This must be done after the base Init call as that inserts modules into the offset map.
	 */
	void SetupBeamModifierModulesOffsets();
	void ResolveSource();
	void ResolveTarget();
	void DetermineVertexAndTriangleCount();

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(UBOOL bSelected);

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		TRUE if the particle system component is selected
	 */
	virtual UBOOL UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected);

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData();

	/**
	 *	Retrieve the allocated size of this instance.
	 *
	 *	@param	OutNum			The size of this instance
	 *	@param	OutMax			The maximum size of this instance
	 */
	virtual void GetAllocatedSize(INT& OutNum, INT& OutMax);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 *	@param	bInExclusiveResourceSizeMode	UObject::GExclusiveResourceSizeMode value
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize(UBOOL bInExclusiveResourceSizeMode);

protected:

	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns TRUE if successful
	 */
	virtual UBOOL FillReplayData( FDynamicEmitterReplayDataBase& OutData );

};

/*-----------------------------------------------------------------------------
	OLD - Get rid of these...
-----------------------------------------------------------------------------*/
struct FParticleBeamEmitterInstance : public FParticleEmitterInstance
{
};

struct FParticleTrailEmitterInstance : public FParticleEmitterInstance
{
};

/*-----------------------------------------------------------------------------
	ParticleTrail2EmitterInstance.
-----------------------------------------------------------------------------*/
struct FParticleTrail2EmitterInstance : public FParticleEmitterInstance
{
	DECLARE_PARTICLEEMITTERINSTANCE_TYPE(FParticleTrail2EmitterInstance, FParticleEmitterInstance);

    UParticleModuleTypeDataTrail2*	TrailTypeData;
    UParticleModuleTrailSource*		TrailModule_Source;
    INT								TrailModule_Source_Offset;
    UParticleModuleTrailSpawn*		TrailModule_Spawn;
    INT								TrailModule_Spawn_Offset;
    UParticleModuleTrailTaper*		TrailModule_Taper;
    INT								TrailModule_Taper_Offset;

	BITFIELD FirstEmission:1;
	BITFIELD bClearTangents:1;
    INT LastEmittedParticleIndex;
    INT LastSelectedParticleIndex;
    INT TickCount;
    INT ForceSpawnCount;
    INT VertexCount;
    INT TriangleCount;
    INT Tessellation;
    TArray<INT> TextureTiles;
    INT TrailCount;
    INT MaxTrailCount;
    TArray<FLOAT> TrailSpawnTimes;
    TArray<FVector> SourcePosition;
    TArray<FVector> LastSourcePosition;
    TArray<FVector> CurrentSourcePosition;
    TArray<FVector> LastSpawnPosition;
    TArray<FVector> LastSpawnTangent;
    TArray<FLOAT> SourceDistanceTravelled;
    AActor* SourceActor;
    TArray<FVector> SourceOffsets;
    FParticleEmitterInstance* SourceEmitter;
    INT ActuallySpawned;

	/** Constructor	*/
	FParticleTrail2EmitterInstance();

	/** Destructor	*/
	virtual ~FParticleTrail2EmitterInstance();

#if STATS
	virtual EParticleStats GetGameThreadDataStat() { return STAT_DynamicTrailGTMem; }
#endif

	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, UBOOL bClearResources = TRUE);
	virtual void Init();
	virtual void Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning);

	virtual void UpdateBoundingBox(FLOAT DeltaTime);

	virtual FLOAT Spawn(FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime, INT Burst = 0, FLOAT BurstTime = 0.0f);
	virtual void PreSpawn(FBaseParticle* Particle);
	virtual void KillParticles();
	
			void SetupTrailModules();
			void ResolveSource();
			void UpdateSourceData(FLOAT DeltaTime);
			void DetermineVertexAndTriangleCount();

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(UBOOL bSelected);

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		TRUE if the particle system component is selected
	 */
	virtual UBOOL UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected);

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData();

	/**
	 *	Retrieve the allocated size of this instance.
	 *
	 *	@param	OutNum			The size of this instance
	 *	@param	OutMax			The maximum size of this instance
	 */
	virtual void GetAllocatedSize(INT& OutNum, INT& OutMax);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 *	@param	bInExclusiveResourceSizeMode	UObject::GExclusiveResourceSizeMode value
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize(UBOOL bInExclusiveResourceSizeMode);

protected:

	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns TRUE if successful
	 */
	virtual UBOOL FillReplayData( FDynamicEmitterReplayDataBase& OutData );

};

/*-----------------------------------------------------------------------------
	FParticleRibbonEmitterInstance.
-----------------------------------------------------------------------------*/
struct FParticleTrailsEmitterInstance_Base : public FParticleEmitterInstance
{
	DECLARE_PARTICLEEMITTERINSTANCE_TYPE(FParticleTrailsEmitterInstance_Base, FParticleEmitterInstance);

	/** The vertex count for this emitter */
	INT VertexCount;
	/** The triangle count for this emitter */
	INT TriangleCount;
	/** The number of active trails in this emitter */
	INT TrailCount;
	/** The max number of trails this emitter is allowed to have */
	INT MaxTrailCount;
	/** The running time for this instance w/ ActiveParticles > 0 */
	FLOAT RunningTime;
	/** The last time the emitter instance was ticked */
	FLOAT LastTickTime;
	/** If TRUE, mark trails dead on deactivate */
	BITFIELD bDeadTrailsOnDeactivate:1;

	/** The Spawn times for each trail in this emitter */
	TArray<FLOAT> TrailSpawnTimes;
	/** The last time a spawn happened for each trail in this emitter */
	TArray<FLOAT> LastSpawnTime;
	/** The distance traveled by each source of each trail in this emitter */
	TArray<FLOAT> SourceDistanceTraveled;
	/** The distance traveled by each source of each trail in this emitter */
	TArray<FLOAT> TiledUDistanceTraveled;
	/** If TRUE, this emitter has not been updated yet... */
	BITFIELD bFirstUpdate:1;
	/**
	 *	If true, when the system checks for particles to kill, it
	 *	will use elapsed gametime to make the determination. This
	 *	will result in emitters that were inactive due to not being
	 *	rendered killing off old particles.
	 */
	BITFIELD bEnableInactiveTimeTracking:1;

	/** Constructor	*/
	FParticleTrailsEmitterInstance_Base() :
		  FParticleEmitterInstance()
		, VertexCount(0)
		, TriangleCount(0)
		, TrailCount(0)
		, MaxTrailCount(0)
		, RunningTime(0.0f)
		, LastTickTime(0.0f)
		, bDeadTrailsOnDeactivate(FALSE)
		, bFirstUpdate(TRUE)
		, bEnableInactiveTimeTracking(FALSE)
	{
		TrailSpawnTimes.Empty();
		LastSpawnTime.Empty();
		SourceDistanceTraveled.Empty();
		TiledUDistanceTraveled.Empty();
	}

	/** Destructor	*/
	virtual ~FParticleTrailsEmitterInstance_Base()
	{
	}

	virtual void Init();
	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, UBOOL bClearResources = TRUE);
	virtual void Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning);

	/**
	 *	Tick sub-function that handles recalculation of tangents
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_RecalculateTangents(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel);
	virtual void UpdateBoundingBox(FLOAT DeltaTime);
	virtual FLOAT Spawn(FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime, INT Burst = 0, FLOAT BurstTime = 0.0f);
	virtual void PreSpawn(FBaseParticle* Particle);
	virtual void KillParticles();

	/**
	 *	Kill the given number of particles from the end of the trail.
	 *
	 *	@param	InTrailIdx		The trail to kill particles in.
	 *	@param	InKillCount		The number of particles to kill off.
	 */
	virtual void KillParticles(INT InTrailIdx, INT InKillCount);

	virtual void SetupTrailModules() {};
	virtual void UpdateSourceData(FLOAT DeltaTime, UBOOL bFirstTime);
	// Update the start particles of the trail, if needed
//	virtual void UpdateStartParticles(FLOAT DeltaTime, UBOOL bFirstTime) {}

	/**
	 *	Called when the particle system is deactivating...
	 */
	virtual void OnDeactivateSystem();

protected:
	enum EGetTrailDirection
	{
		GET_Prev,
		GET_Next
	};
	enum EGetTrailParticleOption
	{
		GET_Any,			// Grab the prev/next particle
		GET_Spawned,		// Grab the first prev/next particle that was true spawned
		GET_Interpolated,	// Grab the first prev/next particle that was interpolation spawned
		GET_Start,			// Grab the start particle for the trail the particle is in
		GET_End,			// Grab the end particle for the trail the particle is in
	};

	/**
	 *	Retrieve the particle in the trail that meets the given criteria
	 *
	 *	@param	bSkipStartingParticle		If TRUE, don't check the starting particle for meeting the criteria
	 *	@param	InStartingFromParticle		The starting point for the search.
	 *	@param	InStartingTrailData			The trail data for the starting point.
	 *	@param	InGetDirection				Direction to search the trail.
	 *	@param	InGetOption					Options for defining the type of particle.
	 *	@param	OutParticle					The particle that meets the criteria.
	 *	@param	OutTrailData				The trail data of the particle that meets the criteria.
	 *
	 *	@return	UBOOL						TRUE if found, FALSE if not.
	 */
	UBOOL GetParticleInTrail(
		UBOOL bSkipStartingParticle,
		FBaseParticle* InStartingFromParticle,
		FTrailsBaseTypeDataPayload* InStartingTrailData,
		EGetTrailDirection InGetDirection,
		EGetTrailParticleOption InGetOption,
		FBaseParticle*& OutParticle,
		FTrailsBaseTypeDataPayload*& OutTrailData);
};

struct FParticleRibbonEmitterInstance : public FParticleTrailsEmitterInstance_Base
{
	DECLARE_PARTICLEEMITTERINSTANCE_TYPE(FParticleRibbonEmitterInstance, FParticleTrailsEmitterInstance_Base);

	/** The TypeData module for this trail emitter */
	UParticleModuleTypeDataRibbon*	TrailTypeData;

	/** SpawnPerUnit module (hijacking it for trails here) */
	UParticleModuleSpawnPerUnit*	SpawnPerUnitModule;

	/** Source module */
	UParticleModuleTrailSource*		SourceModule;
	/** Payload offset for source module */
    INT								TrailModule_Source_Offset;

	/** The current source position for each trail in this emitter */
	TArray<FVector> CurrentSourcePosition;
	/** The current source rotation for each trail in this emitter */
	TArray<FQuat> CurrentSourceRotation;
	/** The current source up for each trail in this emitter */
	TArray<FVector> CurrentSourceUp;
	/** The current source tangent for each trail in this emitter */
	TArray<FVector> CurrentSourceTangent;
	/** The current source tangent strength for each trail in this emitter */
	TArray<FLOAT> CurrentSourceTangentStrength;
	/** The previous source position for each trail in this emitter */
	TArray<FVector> LastSourcePosition;
	/** The last source rotation for each trail in this emitter */
	TArray<FQuat> LastSourceRotation;
	/** The previous source up for each trail in this emitter */
	TArray<FVector> LastSourceUp;
	/** The previous source tangent for each trail in this emitter */
	TArray<FVector> LastSourceTangent;
	/** The previous source tangent strength for each trail in this emitter */
	TArray<FLOAT> LastSourceTangentStrength;
	/** If the source is an actor, this is it */
	AActor* SourceActor;
	/** The offset from the source for each trail in this emitter */
	TArray<FVector> SourceOffsets;
	/** If the source is an emitter, this is it */
	FParticleEmitterInstance* SourceEmitter;
	/** The last selected source index (for sequential selection) */
	INT LastSelectedParticleIndex;
	/** The indices for the source of each trail (if required) */
	TArray<INT> SourceIndices;
	/** The time of the last partice source update */
	TArray<FLOAT> SourceTimes;
	/** The time of the last partice source update */
	TArray<FLOAT> LastSourceTimes;
	/** The lifetime to use for each ribbon */
	TArray<FLOAT> CurrentLifetimes;
	/** The size to use for each ribbon */
	TArray<FLOAT> CurrentSizes;
	/** The direct index of the particle that is the start of each ribbon */
//	TArray<INT> CurrentStartIndices;
	/** If TRUE, then do not render the source (a real spawn just happened) */
//	TArray<UBOOL> SkipSourceSegment;

	/** The number of "head only" active particles */
	INT HeadOnlyParticles;

	/** Constructor	*/
	FParticleRibbonEmitterInstance();

	/** Destructor	*/
	virtual ~FParticleRibbonEmitterInstance();

#if STATS
	virtual EParticleStats GetGameThreadDataStat() { return STAT_DynamicRibbonGTMem; }
#endif

	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, UBOOL bClearResources = TRUE);

	/**
	 *	Tick sub-function that handles recalculation of tangents
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_RecalculateTangents(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel);

	/**
	 *	Tick sub-function that handles module post updates
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_ModulePostUpdate(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel);

	virtual UBOOL GetSpawnPerUnitAmount(FLOAT DeltaTime, INT InTrailIdx, INT& OutCount, FLOAT& OutRate);

	/**
	 *	Get the lifetime and size for a particle being added to the given trail
	 *	
	 *	@param	InTrailIdx				The index of the trail the particle is being added to
	 *	@param	InParticle				The particle that is being added
	 *	@param	bInNoLivingParticles	TRUE if there are no particles in the trail, FALSE if there already are
	 *	@param	OutOneOverMaxLifetime	The OneOverMaxLifetime value to use for the particle
	 *	@param	OutSize					The Size value to use for the particle
	 */
	void GetParticleLifetimeAndSize(INT InTrailIdx, const FBaseParticle* InParticle, UBOOL bInNoLivingParticles, FLOAT& OutOneOverMaxLifetime, FLOAT& OutSize);

	virtual FLOAT Spawn(FLOAT DeltaTime);
	/**
	 *	Spawn source-based ribbon particles.
	 *
	 *	@param	DeltaTime			The current time slice
	 *
	 *	@return	UBOOL				TRUE if SpawnRate should be processed.
	 */
	UBOOL Spawn_Source(FLOAT DeltaTime);
	/**
	 *	Spawn ribbon particles from SpawnRate and Burst settings.
	 *
	 *	@param	DeltaimTime			The current time slice
	 *	
	 *	@return	FLOAT				The spawnfraction left over from this time slice
	 */
	FLOAT Spawn_RateAndBurst(FLOAT DeltaTime);
	
	virtual void SetupTrailModules();
	void ResolveSource();
	virtual void UpdateSourceData(FLOAT DeltaTime, UBOOL bFirstTime);
	// Update the start particles of the trail, if needed
//	virtual void UpdateStartParticles(FLOAT DeltaTime, UBOOL bFirstTime);
	UBOOL ResolveSourcePoint(INT InTrailIdx, FVector& OutPosition, FQuat& OutRotation, FVector& OutUp, FVector& OutTangent, FLOAT& OutTangentStrength);

	/** Determine the number of vertices and triangles in each trail */
	void DetermineVertexAndTriangleCount();

	/**
	 *	Checks some common values for GetDynamicData validity
	 *
	 *	@return	UBOOL		TRUE if GetDynamicData should continue, FALSE if it should return NULL
	 */
	virtual UBOOL IsDynamicDataRequired(UParticleLODLevel* CurrentLODLevel);

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(UBOOL bSelected);

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		TRUE if the particle system component is selected
	 */
	virtual UBOOL UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected);

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData();

	/**
	 *	Retrieve the allocated size of this instance.
	 *
	 *	@param	OutNum			The size of this instance
	 *	@param	OutMax			The maximum size of this instance
	 */
	virtual void GetAllocatedSize(INT& OutNum, INT& OutMax);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 *	@param	bInExclusiveResourceSizeMode	UObject::GExclusiveResourceSizeMode value
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize(UBOOL bInExclusiveResourceSizeMode);

protected:

	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns TRUE if successful
	 */
	virtual UBOOL FillReplayData( FDynamicEmitterReplayDataBase& OutData );
};

/*-----------------------------------------------------------------------------
	FParticleAnimTrailEmitterInstance.
-----------------------------------------------------------------------------*/
struct FAnimTrailSocketSample
{
	/** Position of the socket relative to the root-bone at the sample point */
	FVector Position;
	/** Velocity of the socket at the sample point */
	FVector Velocity;
};

struct FAnimTrailSamplePoint
{
	/** The time value at this sample point, relative to the starting time. */
	FLOAT RelativeTime;
	/** The time step taken for this sample point */
	FLOAT TimeStep;
	/** The current time of the animation for this sample point */
	FLOAT AnimCurrentTime;
	/** The time of the animation at this sample point */
	FLOAT AnimSampleTime;
	/** The sample for the first edge */
	FAnimTrailSocketSample FirstEdgeSample;
	/** The sample for the second edge */
	FAnimTrailSocketSample SecondEdgeSample;
	/** The sample for the control point */
	FAnimTrailSocketSample ControlPointSample;

};

struct FAnimTrailOwnerData
{
	/** These values are for interpolating the intermediate points... */
	/** The position of the source */
	FVector	Position;
	/** The rotation of the source */
	FQuat Rotation;
};

struct FParticleAnimTrailEmitterInstance : public FParticleTrailsEmitterInstance_Base
{
	DECLARE_PARTICLEEMITTERINSTANCE_TYPE(FParticleAnimTrailEmitterInstance, FParticleTrailsEmitterInstance_Base);

	/** The TypeData module for this trail emitter */
	UParticleModuleTypeDataAnimTrail*	TrailTypeData;

	/** SpawnPerUnit module (hijacking it for trails here) */
	UParticleModuleSpawnPerUnit*		SpawnPerUnitModule;

	/** The current source data for each trail in this emitter */
	FAnimTrailSamplePoint CurrentSourceData;
	/** The previous source data for each trail in this emitter */
	FAnimTrailSamplePoint LastSourceData;
	/** The last animation time that was sampled */
	FLOAT LastAnimSampleTime;
	/** The last animation time that was PROCESS */
	FLOAT LastAnimProcessedTime;

	/** 
	 *	The current animation data to process in the spawn 
	 *	This is necessary as we will not have access to the AnimNotify when ticked...
	 */
	TArray<FAnimTrailSamplePoint> AnimData;
	INT CurrentAnimDataCount;
	INT LastTrailIndex;
	INT LastSourceSampleTrailIndex;

	/** The time step used for sampling the animation data. */
	FLOAT AnimSampleTimeStep;

	/** The previous position and rotation of the skel component */
	FAnimTrailOwnerData LastOwnerData;
	FLOAT LastSourceUpdateTime;
	FAnimTrailOwnerData CurrentOwnerData;
	FLOAT CurrentSourceUpdateTime;

	UBOOL bTagTrailAsDead;
	UBOOL bTagEmitterAsDead;

	/** Constructor	*/
	FParticleAnimTrailEmitterInstance();

	/** Destructor	*/
	virtual ~FParticleAnimTrailEmitterInstance();

#if STATS
	virtual EParticleStats GetGameThreadDataStat() { return STAT_DynamicAnimTrailGTMem; }
#endif

	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, UBOOL bClearResources = TRUE);

	/**
	 *	Helper function for recalculating tangents...
	 *
	 *	@param	PrevParticle		The previous particle in the trail
	 *	@param	PrevTrailData		The payload of the previous particle in the trail
	 *	@param	CurrParticle		The current particle in the trail
	 *	@param	CurrTrailData		The payload of the current particle in the trail
	 *	@param	NextParticle		The next particle in the trail
	 *	@param	NextTrailData		The payload of the next particle in the trail
	 */
	virtual void RecalculateTangent(
		FBaseParticle* PrevParticle, FAnimTrailTypeDataPayload* PrevTrailData, 
		FBaseParticle* CurrParticle, FAnimTrailTypeDataPayload* CurrTrailData, 
		FBaseParticle* NextParticle, FAnimTrailTypeDataPayload* NextTrailData);

	/**
	 *	Tick sub-function that handles recalculation of tangents
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_RecalculateTangents(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel);

	/**
	 *	Tick sub-function that handles module post updates
	 *
	 *	@param	DeltaTime			The current time slice
	 *	@param	CurrentLODLevel		The current LOD level for the instance
	 */
	virtual void Tick_ModulePostUpdate(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel);

	virtual UBOOL GetSpawnPerUnitAmount(FLOAT DeltaTime, INT InTrailIdx, INT& OutCount, FLOAT& OutRate);
	virtual FLOAT Spawn(FLOAT DeltaTime);
	
	virtual void SetupTrailModules();
	void ResolveSource();
	virtual void UpdateSourceData(FLOAT DeltaTime, UBOOL bFirstTime);

	/** Determine the number of vertices and triangles in each trail */
	void DetermineVertexAndTriangleCount();

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(UBOOL bSelected);

	/**
	 *	Updates the dynamic data for the instance
	 *
	 *	@param	DynamicData		The dynamic data to fill in
	 *	@param	bSelected		TRUE if the particle system component is selected
	 */
	virtual UBOOL UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected);

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData();

	/**
	 *	Retrieve the allocated size of this instance.
	 *
	 *	@param	OutNum			The size of this instance
	 *	@param	OutMax			The maximum size of this instance
	 */
	virtual void GetAllocatedSize(INT& OutNum, INT& OutMax);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 *	@param	bInExclusiveResourceSizeMode	UObject::GExclusiveResourceSizeMode value
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize(UBOOL bInExclusiveResourceSizeMode);

	/**
	 * Called by AnimNotify_Trails
	 *
	 * @param AnimNotifyData The AnimNotify_Trails which will have all of the various params on it
	 */
	virtual void TrailsNotify(const UAnimNotify_Trails* AnimNotifyData);

	/**
	 * Called by AnimNotify_Trails
	 *
	 * @param AnimNotifyData The AnimNotify_Trails which will have all of the various params on it
	 */
	virtual void TrailsNotifyTick(const UAnimNotify_Trails* AnimNotifyData);

	/**
	 * Called by AnimNotify_Trails
	 *
	 * @param AnimNotifyData The AnimNotify_Trails which will have all of the various params on it
	 */
	virtual void TrailsNotifyEnd(const UAnimNotify_Trails* AnimNotifyData);

	/**
	 * Called by various TrailsNotify functions to actually sample the data
	 *
	 * @param AnimNotifyData The AnimNotify_Trails which will have all of the various params on it
	 */
	virtual void TrailsNotify_UpdateData(const UAnimNotify_Trails* AnimNotifyData);

protected:

	/**
	 * Captures dynamic replay data for this particle system.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns TRUE if successful
	 */
	virtual UBOOL FillReplayData( FDynamicEmitterReplayDataBase& OutData );
};
