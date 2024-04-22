/*=============================================================================
	ParticleEmitterInstances.cpp: Particle emitter instance implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EngineMaterialClasses.h"
#include "LevelUtils.h"
#include "PrimitiveSceneInfo.h"

IMPLEMENT_PARTICLEEMITTERINSTANCE_TYPE(FParticleSpriteEmitterInstance);
IMPLEMENT_PARTICLEEMITTERINSTANCE_TYPE(FParticleSpriteSubUVEmitterInstance);
IMPLEMENT_PARTICLEEMITTERINSTANCE_TYPE(FParticleMeshEmitterInstance);

/*-----------------------------------------------------------------------------
FParticlesStatGroup
-----------------------------------------------------------------------------*/
DECLARE_STATS_GROUP(TEXT("Particles"),STATGROUP_Particles);

DECLARE_DWORD_COUNTER_STAT(TEXT("Particle Draw Calls"),STAT_ParticleDrawCalls,STATGROUP_Particles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Sprite Particles"),STAT_SpriteParticles,STATGROUP_Particles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Sprite Ptcls Spawned"),STAT_SpriteParticlesSpawned,STATGROUP_Particles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Sprite Ptcls Updated"),STAT_SpriteParticlesUpdated,STATGROUP_Particles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Sprite Ptcls Killed"),STAT_SpriteParticlesKilled,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Sort Time"),STAT_SortingTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Sprite Render Time"),STAT_SpriteRenderingTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Sprite Tick Time"),STAT_SpriteTickTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("PSys Comp Tick Time"),STAT_PSysCompTickTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Particle Tick Time"),STAT_ParticleTickTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Particle Spawn Time"),STAT_ParticleSpawnTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Particle Update Time"),STAT_ParticleUpdateTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Particle Render Time"),STAT_ParticleRenderingTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Particle Packing Time"),STAT_ParticlePackingTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("SetTemplate Time"),STAT_ParticleSetTemplateTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Initialize Time"),STAT_ParticleInitializeTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Activate Time"),STAT_ParticleActivateTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Update Bounds Time"),STAT_ParticleUpdateBounds,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("UpdateInstances Time"),STAT_ParticleUpdateInstancesTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Async Work Time"),STAT_ParticleAsyncTime,STATGROUP_Particles);           // regardless of if it is actually performed on other threads or not
DECLARE_CYCLE_STAT(TEXT("Wait For ASync Time"),STAT_ParticleAsyncWaitTime,STATGROUP_Particles);   // can be either performed on this thread or a true wait

DECLARE_STATS_GROUP(TEXT("MeshParticles"),STATGROUP_MeshParticles);

DECLARE_DWORD_COUNTER_STAT(TEXT("Mesh Particles"),STAT_MeshParticles,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Mesh Render Time"),STAT_MeshRenderingTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Mesh Tick Time"),STAT_MeshTickTime,STATGROUP_Particles);

/** Particle memory stats */
DECLARE_STATS_GROUP(TEXT("ParticleMem"),STATGROUP_ParticleMem);
DECLARE_CYCLE_STAT(TEXT("Particle Memory Time"),STAT_ParticleMemTime,STATGROUP_ParticleMem);
DECLARE_MEMORY_STAT(TEXT("Ptcls Data GT Mem"),STAT_GTParticleData,STATGROUP_ParticleMem);
DECLARE_MEMORY_STAT(TEXT("DynSprite GT Mem"),STAT_DynamicSpriteGTMem,STATGROUP_ParticleMem);
DECLARE_MEMORY_STAT(TEXT("DynSubUV GT Mem"),STAT_DynamicSubUVGTMem,STATGROUP_ParticleMem);
DECLARE_MEMORY_STAT(TEXT("DynMesh GT Mem"),STAT_DynamicMeshGTMem,STATGROUP_ParticleMem);
DECLARE_MEMORY_STAT(TEXT("DynBeam GT Mem"),STAT_DynamicBeamGTMem,STATGROUP_ParticleMem);
DECLARE_MEMORY_STAT(TEXT("DynTrail GT Mem"),STAT_DynamicTrailGTMem,STATGROUP_ParticleMem);
DECLARE_MEMORY_STAT(TEXT("DynRibbon GT Mem"),STAT_DynamicRibbonGTMem,STATGROUP_ParticleMem);
DECLARE_MEMORY_STAT(TEXT("DynAnimTrail GT Mem"),STAT_DynamicAnimTrailGTMem,STATGROUP_ParticleMem);
DECLARE_MEMORY_STAT(TEXT("DynUntracked Mem"),STAT_DynamicUntrackedGTMem,STATGROUP_ParticleMem);

DECLARE_DWORD_COUNTER_STAT(TEXT("Ptcls Data RT Mem"),STAT_RTParticleData,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("Ptcls Data GT Mem MAX"),STAT_GTParticleData_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("Ptcls Data RT Mem MAX"),STAT_RTParticleData_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("Ptcls Data RT Largest"),STAT_RTParticleData_Largest,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("Ptcls Data RT Largest MAX"),STAT_RTParticleData_Largest_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynPSysComp Mem"),STAT_DynamicPSysCompMem,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynPSysComp Mem MAX"),STAT_DynamicPSysCompMem_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynPSysComp Count"),STAT_DynamicPSysCompCount,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynPSysComp Count MAX"),STAT_DynamicPSysCompCount_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynEmitter Mem"),STAT_DynamicEmitterMem,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynEmitter Mem MAX"),STAT_DynamicEmitterMem_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynEmitter Count"),STAT_DynamicEmitterCount,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynEmitter Count Max"),STAT_DynamicEmitterCount_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynEmitter GTMem Waste"),STAT_DynamicEmitterGTMem_Waste,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynEmitter GTMem Waste MAX"),STAT_DynamicEmitterGTMem_Waste_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynEmitter GTMem Largest"),STAT_DynamicEmitterGTMem_Largest,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynEmitter GTMem Largest MAX"),STAT_DynamicEmitterGTMem_Largest_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynSprite Count"),STAT_DynamicSpriteCount,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynSprite Max"),STAT_DynamicSpriteCount_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynSprite GT Mem Max"),STAT_DynamicSpriteGTMem_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynSubUV Count"),STAT_DynamicSubUVCount,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynSubUV Max"),STAT_DynamicSubUVCount_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynSubUV GT Mem Max"),STAT_DynamicSubUVGTMem_Max,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynMesh Count"),STAT_DynamicMeshCount,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynMesh Max"),STAT_DynamicMeshCount_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynMesh GT Mem Max"),STAT_DynamicMeshGTMem_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynBeam Count"),STAT_DynamicBeamCount,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynBeam Max"),STAT_DynamicBeamCount_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynBeam GT Mem Max"),STAT_DynamicBeamGTMem_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynTrail Count"),STAT_DynamicTrailCount,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynTrail Max"),STAT_DynamicTrailCount_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynTrail GT Mem Max"),STAT_DynamicTrailGTMem_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynRibbon Count"),STAT_DynamicRibbonCount,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynRibbon Max"),STAT_DynamicRibbonCount_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynRibbon GT Mem Max"),STAT_DynamicRibbonGTMem_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynAnimTrail Count"),STAT_DynamicAnimTrailCount,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynAnimTrail Max"),STAT_DynamicAnimTrailCount_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynAnimTrail GT Mem Max"),STAT_DynamicAnimTrailGTMem_MAX,STATGROUP_ParticleMem);
DECLARE_DWORD_COUNTER_STAT(TEXT("DynUntracked Mem Max"),STAT_DynamicUntrackedGTMem_MAX,STATGROUP_ParticleMem);

/*-----------------------------------------------------------------------------
	FParticleEmitterInstance
-----------------------------------------------------------------------------*/
/**
 *	ParticleEmitterInstance
 *	The base structure for all emitter instance classes
 */
FParticleEmitterInstanceType FParticleEmitterInstance::StaticType(TEXT("FParticleEmitterInstance"),NULL);

// Only update the PeakActiveParticles if the frame rate is 20 or better
const FLOAT FParticleEmitterInstance::PeakActiveParticleUpdateDelta = 0.05f;

/** Constructor	*/
FParticleEmitterInstance::FParticleEmitterInstance() :
	  SpriteTemplate(NULL)
    , Component(NULL)
    , CurrentLODLevelIndex(0)
    , CurrentLODLevel(NULL)
    , TypeDataOffset(0)
	, TypeDataInstanceOffset(-1)
    , SubUVDataOffset(0)
	, DynamicParameterDataOffset(0)
	, OrbitModuleOffset(0)
	, CameraPayloadOffset(0)
    , KillOnDeactivate(0)
    , bKillOnCompleted(0)
	, bHaltSpawning(0)
	, SortMode(PSORTMODE_None)
    , ParticleData(NULL)
    , ParticleIndices(NULL)
    , InstanceData(NULL)
    , InstancePayloadSize(0)
    , PayloadOffset(0)
    , ParticleSize(0)
    , ParticleStride(0)
    , ActiveParticles(0)
    , MaxActiveParticles(0)
    , SpawnFraction(0.0f)
    , SecondsSinceCreation(0.0f)
    , EmitterTime(0.0f)
    , LoopCount(0)
	, IsRenderDataDirty(0)
    , Module_AxisLock(NULL)
    , EmitterDuration(0.0f)
	, TrianglesToRender(0)
	, MaxVertexIndex(0)
	, CurrentMaterial(NULL)
#if !FINAL_RELEASE
	, EventCount(0)
	, MaxEventCount(0)
#endif	//#if !FINAL_RELEASE
{
}

/** Destructor	*/
FParticleEmitterInstance::~FParticleEmitterInstance()
{
	appFree(ParticleData);
	appFree(ParticleIndices);
    appFree(InstanceData);
	BurstFired.Empty();
}

#if STATS
void FParticleEmitterInstance::PreDestructorCall()
{
	// Update the memory stat
	INT TotalMem = (MaxActiveParticles * ParticleStride) + (MaxActiveParticles * sizeof(WORD));
	DEC_DWORD_STAT_BY(STAT_GTParticleData, TotalMem);
	DWORD StatId = GetGameThreadDataStat();
	DEC_DWORD_STAT_BY(StatId, TotalMem);
}
#endif

/**
 *	Set the KillOnDeactivate flag to the given value
 *
 *	@param	bKill	Value to set KillOnDeactivate to.
 */
void FParticleEmitterInstance::SetKillOnDeactivate(UBOOL bKill)
{
	KillOnDeactivate = bKill;
}

/**
 *	Set the KillOnCompleted flag to the given value
 *
 *	@param	bKill	Value to set KillOnCompleted to.
 */
void FParticleEmitterInstance::SetKillOnCompleted(UBOOL bKill)
{
	bKillOnCompleted = bKill;
}

/**
 *	Initialize the parameters for the structure
 *
 *	@param	InTemplate		The ParticleEmitter to base the instance on
 *	@param	InComponent		The owning ParticleComponent
 *	@param	bClearResources	If TRUE, clear all resource data
 */
void FParticleEmitterInstance::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, UBOOL bClearResources)
{
	SpriteTemplate = CastChecked<UParticleSpriteEmitter>(InTemplate);
	Component = InComponent;
	SetupEmitterDuration();
}

/**
 *	Initialize the instance
 */
void FParticleEmitterInstance::Init()
{
	// This assert makes sure that packing is as expected.
	// Added FBaseColor...
	// Linear color change
	// Added Flags field
	check(sizeof(FBaseParticle) == 128);

	// Calculate particle struct size, size and average lifetime.
	ParticleSize = sizeof(FBaseParticle);
	INT	ReqBytes;
	INT ReqInstanceBytes = 0;
	INT TempInstanceBytes;

	TypeDataOffset = 0;
	UParticleLODLevel* HighLODLevel = SpriteTemplate->GetLODLevel(0);
	check(HighLODLevel);
	UParticleModule* TypeDataModule = HighLODLevel->TypeDataModule;
	if (TypeDataModule)
	{
		ReqBytes = TypeDataModule->RequiredBytes(this);
		if (ReqBytes)
		{
			TypeDataOffset	 = ParticleSize;
			ParticleSize	+= ReqBytes;
		}

		TempInstanceBytes = TypeDataModule->RequiredBytesPerInstance(this);
		if (TempInstanceBytes)
		{
			TypeDataInstanceOffset = ReqInstanceBytes;
			ReqInstanceBytes += TempInstanceBytes;
		}
	}

	// Set the current material
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	check(LODLevel);
	check(LODLevel->RequiredModule);
	CurrentMaterial = LODLevel->RequiredModule->Material;

	// NOTE: This code assumes that the same module order occurs in all LOD levels
	DynamicParameterDataOffset = 0;
	CameraPayloadOffset = 0;
	bRequiresLoopNotification = FALSE;
	for (INT ModuleIdx = 0; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
	{
		UParticleModule* ParticleModule = LODLevel->Modules(ModuleIdx);
		check(ParticleModule);

		// Loop notification?
		bRequiresLoopNotification |= (ParticleModule->bEnabled && ParticleModule->RequiresLoopingNotification());

		// We always use the HighModule as the look-up in the offset maps...
		UParticleModule* HighModule = HighLODLevel->Modules(ModuleIdx);
		check(HighModule);
		check(HighModule->GetClass() == ParticleModule->GetClass());

		if (ParticleModule->IsA(UParticleModuleTypeDataBase::StaticClass()) == FALSE)
		{
			ReqBytes = ParticleModule->RequiredBytes(this);
			if (ReqBytes)
			{
				ModuleOffsetMap.Set(HighModule, ParticleSize);
				if (ParticleModule->IsA(UParticleModuleParameterDynamic::StaticClass()) && (DynamicParameterDataOffset == 0))
				{
					DynamicParameterDataOffset = ParticleSize;
				}
				if (ParticleModule->IsA(UParticleModuleCameraOffset::StaticClass()) && (CameraPayloadOffset == 0))
				{
					CameraPayloadOffset = ParticleSize;
				}
				ParticleSize	+= ReqBytes;
			}

			TempInstanceBytes = ParticleModule->RequiredBytesPerInstance(this);
			if (TempInstanceBytes)
			{
				// Add the high-lodlevel offset to the lookup map
				ModuleInstanceOffsetMap.Set(HighModule, ReqInstanceBytes);
				// Add all the other LODLevel modules, using the same offset.
				// This removes the need to always also grab the HighestLODLevel pointer.
				for (INT LODIdx = 1; LODIdx < SpriteTemplate->LODLevels.Num(); LODIdx++)
				{
					UParticleLODLevel* LODLevel = SpriteTemplate->LODLevels(LODIdx);
					ModuleInstanceOffsetMap.Set(LODLevel->Modules(ModuleIdx), ReqInstanceBytes);
				}
				ReqInstanceBytes += TempInstanceBytes;
			}
		}

		if (ParticleModule->IsA(UParticleModuleOrientationAxisLock::StaticClass()))
		{
			Module_AxisLock	= Cast<UParticleModuleOrientationAxisLock>(ParticleModule);
		}
	}

	if ((InstanceData == NULL) || (ReqInstanceBytes > InstancePayloadSize))
	{
		InstanceData = (BYTE*)(appRealloc(InstanceData, ReqInstanceBytes));
		InstancePayloadSize = ReqInstanceBytes;
	}

	appMemzero(InstanceData, InstancePayloadSize);

	for (INT ModuleIdx = 0; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
	{
		UParticleModule* ParticleModule = LODLevel->Modules(ModuleIdx);
		check(ParticleModule);
		BYTE* PrepInstData = GetModuleInstanceData(ParticleModule);
		if (PrepInstData)
		{
			ParticleModule->PrepPerInstanceBlock(this, (void*)PrepInstData);
		}
	}

	// Offset into emitter specific payload (e.g. TrailComponent requires extra bytes).
	PayloadOffset = ParticleSize;
	
	// Update size with emitter specific size requirements.
	ParticleSize += RequiredBytes();

	// Make sure everything is at least 16 byte aligned so we can use SSE for FVector.
	ParticleSize = Align(ParticleSize, 16);

	// E.g. trail emitters store trailing particles directly after leading one.
	ParticleStride			= CalculateParticleStride(ParticleSize);

	// Set initial values.
	SpawnFraction			= 0;
	SecondsSinceCreation	= 0;
	
	Location				= Component->LocalToWorld.GetOrigin();
	OldLocation				= Location;
	
	TrianglesToRender		= 0;
	MaxVertexIndex			= 0;

	if (ParticleData == NULL)
	{
		MaxActiveParticles	= 0;
		ActiveParticles		= 0;
	}

	ParticleBoundingBox.Init();
	check(LODLevel->RequiredModule);
	if (LODLevel->RequiredModule->RandomImageChanges == 0)
	{
		LODLevel->RequiredModule->RandomImageTime	= 1.0f;
	}
	else
	{
		LODLevel->RequiredModule->RandomImageTime	= 0.99f / (LODLevel->RequiredModule->RandomImageChanges + 1);
	}

	// Resize to sensible default.
	if (GIsGame == TRUE &&
		// Only presize if any particles will be spawned 
		(GSystemSettings.DetailMode == DM_High || SpriteTemplate->MediumDetailSpawnRateScale > 0))
	{
		if ((LODLevel->PeakActiveParticles > 0) || (SpriteTemplate->InitialAllocationCount > 0))
		{
			// In-game... we assume the editor has set this properly, but still clamp at 100 to avoid wasting
			// memory.
			if (SpriteTemplate->InitialAllocationCount > 0)
			{
				Resize(Min( SpriteTemplate->InitialAllocationCount, 100 ));
			}
			else
			{
				Resize(Min( LODLevel->PeakActiveParticles, 100 ));
			}
		}
		else
		{
			// This is to force the editor to 'select' a value
			Resize(10);
		}
	}

	LoopCount = 0;

	// Propagate killon flags
	SetKillOnDeactivate(LODLevel->RequiredModule->bKillOnDeactivate);
	SetKillOnCompleted(LODLevel->RequiredModule->bKillOnCompleted);

	// Propagate sorting flag.
	SortMode = LODLevel->RequiredModule->SortMode;

	// Reset the burst lists
	if (BurstFired.Num() < SpriteTemplate->LODLevels.Num())
	{
		BurstFired.AddZeroed(SpriteTemplate->LODLevels.Num() - BurstFired.Num());
	}
	for (INT LODIndex = 0; LODIndex < SpriteTemplate->LODLevels.Num(); LODIndex++)
	{
		LODLevel = SpriteTemplate->LODLevels(LODIndex);
		check(LODLevel);
		FLODBurstFired& LocalBurstFired = BurstFired(LODIndex);
		if (LocalBurstFired.Fired.Num() < LODLevel->SpawnModule->BurstList.Num())
		{
			LocalBurstFired.Fired.AddZeroed(LODLevel->SpawnModule->BurstList.Num() - LocalBurstFired.Fired.Num());
		}
	}
	ResetBurstList();

	// Tag it as dirty w.r.t. the renderer
	IsRenderDataDirty	= 1;
}

/**
 *	Resize the particle data array
 *
 *	@param	NewMaxActiveParticles	The new size to use
 *
 *	@return	UBOOL					TRUE if the resize was successful
 */
UBOOL FParticleEmitterInstance::Resize(INT NewMaxActiveParticles, UBOOL bSetMaxActiveCount)
{
	if (GEngine->MaxParticleResize > 0)
	{
		if ((NewMaxActiveParticles < 0) || (NewMaxActiveParticles > GEngine->MaxParticleResize))
		{
			if ((NewMaxActiveParticles < 0) || (NewMaxActiveParticles > GEngine->MaxParticleResizeWarn))
			{
				warnf(TEXT("Emitter::Resize> Invalid NewMaxActive (%d) for Emitter in PSys %s"),
					NewMaxActiveParticles, 
					Component	? 
								Component->Template ? *(Component->Template->GetPathName()) 
													: *(Component->GetName()) 
								:
								TEXT("INVALID COMPONENT"));
			}

			return FALSE;
		}
	}

	if (NewMaxActiveParticles > MaxActiveParticles)
	{
		// Alloc (or realloc) the data array
		// Allocations > 16 byte are always 16 byte aligned so ParticleData can be used with SSE.
		// NOTE: We don't have to zero the memory here... It gets zeroed when grabbed later.
#if STATS
		{
			SCOPE_CYCLE_COUNTER(STAT_ParticleMemTime);
			// Update the memory stat
			INT OldMem = (MaxActiveParticles * ParticleStride) + (MaxActiveParticles * sizeof(WORD));
			INT NewMem = (NewMaxActiveParticles * ParticleStride) + (NewMaxActiveParticles * sizeof(WORD));
			DEC_DWORD_STAT_BY(STAT_GTParticleData, OldMem);
			DEC_DWORD_STAT_BY(GetGameThreadDataStat(), OldMem);
			INC_DWORD_STAT_BY(STAT_GTParticleData, NewMem);
			INC_DWORD_STAT_BY(GetGameThreadDataStat(), NewMem);
			SET_DWORD_STAT(STAT_DynamicEmitterGTMem_Largest, Max<DWORD>(NewMem, GET_DWORD_STAT(STAT_DynamicEmitterGTMem_Largest)));
		}
#endif
		ParticleData = (BYTE*) appRealloc(ParticleData, ParticleStride * NewMaxActiveParticles);
		check(ParticleData);

		// Allocate memory for indices.
		if (ParticleIndices == NULL)
		{
			// Make sure that we clear all when it is the first alloc
			MaxActiveParticles = 0;
		}
		ParticleIndices	= (WORD*) appRealloc(ParticleIndices, sizeof(WORD) * (NewMaxActiveParticles + 1));

		// Fill in default 1:1 mapping.
		for (INT i=MaxActiveParticles; i<NewMaxActiveParticles; i++)
		{
			ParticleIndices[i] = i;
		}

		// Set the max count
		MaxActiveParticles = NewMaxActiveParticles;
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_ParticleMemTime);
		INT WastedMem = 
			((MaxActiveParticles * ParticleStride) + (MaxActiveParticles * sizeof(WORD))) - 
			((ActiveParticles * ParticleStride) + (ActiveParticles * sizeof(WORD)));
		INC_DWORD_STAT_BY(STAT_DynamicEmitterGTMem_Waste,WastedMem);
	}

	// Set the PeakActiveParticles
	if (bSetMaxActiveCount)
	{
		UParticleLODLevel* LODLevel	= SpriteTemplate->GetLODLevel(0);
		check(LODLevel);
		if (MaxActiveParticles > LODLevel->PeakActiveParticles)
		{
			LODLevel->PeakActiveParticles = MaxActiveParticles;
		}
	}

	return TRUE;
}

/**
 *	Tick the instance.
 *
 *	@param	DeltaTime			The time slice to use
 *	@param	bSuppressSpawning	If TRUE, do not spawn during Tick
 */
void FParticleEmitterInstance::Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning)
{
	// If the Particle is a mesh particle, dont track sprite tick stat
	FParticleMeshEmitterInstance* MeshEmitterInstance = (FParticleMeshEmitterInstance*) this;
	SCOPE_CONDITIONAL_CYCLE_COUNTER( STAT_SpriteTickTime, MeshEmitterInstance == NULL );

	check(SpriteTemplate);
	check(SpriteTemplate->LODLevels.Num() > 0);

	// If this the FirstTime we are being ticked?
	UBOOL bFirstTime = (SecondsSinceCreation > 0.0f) ? FALSE : TRUE;

	// Grab the current LOD level
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	check(LODLevel);

	// Handle EmitterTime setup, looping, etc.
	FLOAT EmitterDelay = Tick_EmitterTimeSetup(DeltaTime, LODLevel);

	// Kill off any dead particles
	KillParticles();

	// If not suppressing spawning...
	SpawnFraction = Tick_SpawnParticles(DeltaTime, LODLevel, bSuppressSpawning, bFirstTime);

	// Reset particle parameters.
	ResetParticleParameters(DeltaTime, STAT_SpriteParticlesUpdated);

	// Update the particles
	SCOPE_CYCLE_COUNTER(STAT_ParticleUpdateTime);

	CurrentMaterial = LODLevel->RequiredModule->Material;

	Tick_ModulePreUpdate(DeltaTime, LODLevel);
	Tick_ModuleUpdate(DeltaTime, LODLevel);
	Tick_ModulePostUpdate(DeltaTime, LODLevel);

	if (ActiveParticles > 0)
	{
		// Update the orbit data...
		UpdateOrbitData(DeltaTime);
		// Calculate bounding box and simulate velocity.
		UpdateBoundingBox(DeltaTime);
	}

	Tick_ModuleFinalUpdate(DeltaTime, LODLevel);

	// Invalidate the contents of the vertex/index buffer.
	IsRenderDataDirty = 1;

	// 'Reset' the emitter time so that the delay functions correctly
	EmitterTime += EmitterDelay;

	INC_DWORD_STAT_BY(STAT_SpriteParticles, ActiveParticles);
}

/**
 *	Tick sub-function that handle EmitterTime setup, looping, etc.
 *
 *	@param	DeltaTime			The current time slice
 *	@param	CurrentLODLevel		The current LOD level for the instance
 *
 *	@return	FLOAT				The EmitterDelay
 */
FLOAT FParticleEmitterInstance::Tick_EmitterTimeSetup(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel)
{
	// Make sure we don't try and do any interpolation on the first frame we are attached (OldLocation is not valid in this circumstance)
	if (Component->bJustAttached)
	{
		Location	= Component->LocalToWorld.GetOrigin();
		OldLocation	= Location;
	}
	else
	{
		// Keep track of location for world- space interpolation and other effects.
		OldLocation	= Location;
		Location	= Component->LocalToWorld.GetOrigin();
	}

	SecondsSinceCreation += DeltaTime;

	// Update time within emitter loop.
	UBOOL bValidDuration = TRUE;
	UBOOL bLooped = FALSE;
	if (CurrentLODLevel->RequiredModule->bUseLegacyEmitterTime == FALSE)
	{
		EmitterTime += DeltaTime;
		bLooped = (EmitterDuration > 0.0f) && (EmitterTime >= EmitterDuration);
	}
	else
	{
		EmitterTime = SecondsSinceCreation;
		if (EmitterDuration > KINDA_SMALL_NUMBER)
		{
			EmitterTime = appFmod(SecondsSinceCreation, EmitterDuration);
			bLooped = ((SecondsSinceCreation - (EmitterDuration * LoopCount)) >= EmitterDuration);
		}
		else
		{
			bValidDuration = TRUE;
		}
	}

	// Get the emitter delay time
	FLOAT EmitterDelay = CurrentDelay;

	// Determine if the emitter has looped
	if (bValidDuration && bLooped)
	{
		LoopCount++;
		ResetBurstList();
#if !FINAL_RELEASE
		// Reset the event count each loop...
		if (EventCount > MaxEventCount)
		{
			MaxEventCount = EventCount;
		}
		EventCount = 0;
#endif	//#if !FINAL_RELEASE

		if (CurrentLODLevel->RequiredModule->bUseLegacyEmitterTime == FALSE)
		{
			EmitterTime -= EmitterDuration;
		}

		if ((CurrentLODLevel->RequiredModule->bDurationRecalcEachLoop == TRUE)
			|| ((CurrentLODLevel->RequiredModule->bDelayFirstLoopOnly == TRUE) && (LoopCount == 1))
			)
		{
			SetupEmitterDuration();
		}

		if (bRequiresLoopNotification == TRUE)
		{
			for (INT ModuleIdx = -3; ModuleIdx < CurrentLODLevel->Modules.Num(); ModuleIdx++)
			{
				INT ModuleFetchIdx;
				switch (ModuleIdx)
				{
				case -3:	ModuleFetchIdx = INDEX_REQUIREDMODULE;	break;
				case -2:	ModuleFetchIdx = INDEX_SPAWNMODULE;		break;
				case -1:	ModuleFetchIdx = INDEX_TYPEDATAMODULE;	break;
				default:	ModuleFetchIdx = ModuleIdx;				break;
				}

				UParticleModule* Module = CurrentLODLevel->GetModuleAtIndex(ModuleFetchIdx);
				if (Module != NULL)
				{
					if (Module->RequiresLoopingNotification() == TRUE)
					{
						Module->EmitterLoopingNotify(this);
					}
				}
			}
		}
	}

	// Don't delay unless required
	if ((CurrentLODLevel->RequiredModule->bDelayFirstLoopOnly == TRUE) && (LoopCount > 0))
	{
		EmitterDelay = 0;
	}

	// 'Reset' the emitter time so that the modules function correctly
	EmitterTime -= EmitterDelay;

	return EmitterDelay;
}

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
FLOAT FParticleEmitterInstance::Tick_SpawnParticles(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel, UBOOL bSuppressSpawning, UBOOL bFirstTime)
{
	if (!bHaltSpawning && !bSuppressSpawning && (EmitterTime >= 0.0f))
	{
		SCOPE_CYCLE_COUNTER(STAT_ParticleSpawnTime);
		// If emitter is not done - spawn at current rate.
		// If EmitterLoops is 0, then we loop forever, so always spawn.
		if ((CurrentLODLevel->RequiredModule->EmitterLoops == 0) || 
			(LoopCount < CurrentLODLevel->RequiredModule->EmitterLoops) ||
			(SecondsSinceCreation < (EmitterDuration * CurrentLODLevel->RequiredModule->EmitterLoops)) ||
			bFirstTime)
		{
            bFirstTime = FALSE;
			SpawnFraction = Spawn(DeltaTime);
		}
	}
	
	return SpawnFraction;
}

/**
 *	Tick sub-function that handles module pre updates
 *
 *	@param	DeltaTime			The current time slice
 *	@param	CurrentLODLevel		The current LOD level for the instance
 */
void FParticleEmitterInstance::Tick_ModulePreUpdate(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel)
{
	UParticleModuleTypeDataBase* TypeData = Cast<UParticleModuleTypeDataBase>(CurrentLODLevel->TypeDataModule);
	if (TypeData)
	{
		TypeData->PreUpdate(this, TypeDataOffset, DeltaTime);
	}
}

/**
 *	Tick sub-function that handles module updates
 *
 *	@param	DeltaTime			The current time slice
 *	@param	CurrentLODLevel		The current LOD level for the instance
 */
void FParticleEmitterInstance::Tick_ModuleUpdate(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel)
{
	UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels(0);
	check(HighestLODLevel);
	for (INT ModuleIndex = 0; ModuleIndex < CurrentLODLevel->UpdateModules.Num(); ModuleIndex++)
	{
		UParticleModule* CurrentModule	= CurrentLODLevel->UpdateModules(ModuleIndex);
		if (CurrentModule && CurrentModule->bEnabled && CurrentModule->bUpdateModule)
		{
			UINT* Offset = ModuleOffsetMap.Find(HighestLODLevel->UpdateModules(ModuleIndex));
			CurrentModule->Update(this, Offset ? *Offset : 0, DeltaTime);
		}
	}
}

/**
 *	Tick sub-function that handles module post updates
 *
 *	@param	DeltaTime			The current time slice
 *	@param	CurrentLODLevel		The current LOD level for the instance
 */
void FParticleEmitterInstance::Tick_ModulePostUpdate(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel)
{
	// Handle the TypeData module
	UParticleModuleTypeDataBase* TypeData = Cast<UParticleModuleTypeDataBase>(CurrentLODLevel->TypeDataModule);
	if (TypeData)
	{
		TypeData->Update(this, TypeDataOffset, DeltaTime);
		TypeData->PostUpdate(this, TypeDataOffset, DeltaTime);
	}
}

/**
 *	Tick sub-function that handles module FINAL updates
 *
 *	@param	DeltaTime			The current time slice
 *	@param	CurrentLODLevel		The current LOD level for the instance
 */
void FParticleEmitterInstance::Tick_ModuleFinalUpdate(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel)
{
	UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels(0);
	check(HighestLODLevel);
	for (INT ModuleIndex = 0; ModuleIndex < CurrentLODLevel->UpdateModules.Num(); ModuleIndex++)
	{
		UParticleModule* CurrentModule	= CurrentLODLevel->UpdateModules(ModuleIndex);
		if (CurrentModule && CurrentModule->bEnabled && CurrentModule->bFinalUpdateModule)
		{
			UINT* Offset = ModuleOffsetMap.Find(HighestLODLevel->UpdateModules(ModuleIndex));
			CurrentModule->FinalUpdate(this, Offset ? *Offset : 0, DeltaTime);
		}
	}

	if (CurrentLODLevel->TypeDataModule && CurrentLODLevel->TypeDataModule->bEnabled && CurrentLODLevel->TypeDataModule->bFinalUpdateModule)
	{
		UINT* Offset = ModuleOffsetMap.Find(HighestLODLevel->TypeDataModule);
		CurrentLODLevel->TypeDataModule->FinalUpdate(this, Offset ? *Offset : 0, DeltaTime);
	}
}

/**
 *	Set the LOD to the given index
 *
 *	@param	InLODIndex			The index of the LOD to set as current
 *	@param	bInFullyProcess		If TRUE, process burst lists, etc.
 */
void FParticleEmitterInstance::SetCurrentLODIndex(INT InLODIndex, UBOOL bInFullyProcess)
{
	if (SpriteTemplate != NULL)
	{
		CurrentLODLevelIndex = InLODIndex;
		// check to make certain the data in the content actually represents what we are being asked to render
		if (SpriteTemplate->LODLevels.Num() > CurrentLODLevelIndex)
		{
			CurrentLODLevel	= SpriteTemplate->LODLevels(CurrentLODLevelIndex);
		}
		// set to the LOD which is guaranteed to exist
		else
		{
			CurrentLODLevelIndex = 0;
			CurrentLODLevel = SpriteTemplate->LODLevels(CurrentLODLevelIndex);
		}
		EmitterDuration = EmitterDurations(CurrentLODLevelIndex);

		check(CurrentLODLevel);
		check(CurrentLODLevel->RequiredModule);

		if (bInFullyProcess == TRUE)
		{
			bKillOnCompleted = CurrentLODLevel->RequiredModule->bKillOnCompleted;
			KillOnDeactivate = CurrentLODLevel->RequiredModule->bKillOnDeactivate;

			// Check for bursts that should have been fired already...
			UParticleModuleSpawn* SpawnModule = CurrentLODLevel->SpawnModule;
			FLODBurstFired* LocalBurstFired = NULL;

			if (CurrentLODLevelIndex + 1 > BurstFired.Num())
			{
				// This should not happen, but catch it just in case...
				BurstFired.AddZeroed(CurrentLODLevelIndex - BurstFired.Num() + 1);
			}
			LocalBurstFired = &(BurstFired(CurrentLODLevelIndex));

			if (LocalBurstFired->Fired.Num() < SpawnModule->BurstList.Num())
			{
				LocalBurstFired->Fired.AddZeroed(SpawnModule->BurstList.Num() - LocalBurstFired->Fired.Num());
			}

			for (INT BurstIndex = 0; BurstIndex < SpawnModule->BurstList.Num(); BurstIndex++)
			{
				if (CurrentLODLevel->RequiredModule->EmitterDelay + SpawnModule->BurstList(BurstIndex).Time < EmitterTime)
				{
					LocalBurstFired->Fired(BurstIndex) = TRUE;
				}
			}
		}

		if ((GIsGame == TRUE) && (CurrentLODLevel->bEnabled == FALSE))
		{
			// Kill active particles...
			KillParticlesForced();
		}
	}
	else
	{
		// This is a legitimate case when PSysComponents are cached...
		// However, with the addition of the bIsActive flag to that class, this should 
		// never be called when the component has not had it's instances initialized/activated.
#if defined(_PSYSCOMP_DEBUG_INVALID_EMITTER_INSTANCE_TEMPLATES_)
		// need better debugging here
		warnf(TEXT("Template of emitter instance %d (%s) a ParticleSystemComponent (%s) was NULL: %s" ), 
			i, *GetName(), *Template->GetName(), *this->GetFullName());
#endif	//#if defined(_PSYSCOMP_DEBUG_INVALID_EMITTER_INSTANCE_TEMPLATES_)
	}
}

/**
 *	Rewind the instance.
 */
void FParticleEmitterInstance::Rewind()
{
	SecondsSinceCreation = 0;
	EmitterTime = 0;
	LoopCount = 0;
	ResetBurstList();
}

/**
 *	Retrieve the bounding box for the instance
 *
 *	@return	FBox	The bounding box
 */
FBox FParticleEmitterInstance::GetBoundingBox()
{ 
	return ParticleBoundingBox;
}

/**
 *	Update the bounding box for the emitter
 *
 *	@param	DeltaTime		The time slice to use
 */
void FParticleEmitterInstance::UpdateBoundingBox(FLOAT DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleUpdateBounds);
	if (Component)
	{
		UBOOL bUpdateBox = ((Component->bWarmingUp == FALSE) && (Component->bSkipBoundsUpdate == FALSE) &&
			(Component->Template != NULL) && (Component->Template->bUseFixedRelativeBoundingBox == FALSE));

		// Take component scale into account
		FVector Scale = FVector(1.0f, 1.0f, 1.0f);
		Scale *= Component->Scale * Component->Scale3D;
		AActor* Actor = Component->GetOwner();
		if (Actor && !Component->AbsoluteScale)
		{
			Scale *= Actor->DrawScale * Actor->DrawScale3D;
		}

		UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
		check(LODLevel);

		FVector	NewLocation;
		FLOAT	NewRotation;
		if (bUpdateBox)
		{
			ParticleBoundingBox.Init();
		}
		UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels(0);
		check(HighestLODLevel);

		// Store off the orbit offset, if there is one
		INT OrbitOffsetValue = -1;
		if (LODLevel->OrbitModules.Num() > 0)
		{
			UParticleModuleOrbit* OrbitModule = HighestLODLevel->OrbitModules(LODLevel->OrbitModules.Num() - 1);
			if (OrbitModule)
			{
				UINT* OrbitOffsetIndex = ModuleOffsetMap.Find(OrbitModule);
				if (OrbitOffsetIndex)
				{
					OrbitOffsetValue = *OrbitOffsetIndex;
				}
			}
		}

		// For each particle, offset the box appropriately 
		FVector MinVal((WORLD_MAX/2.0f));
		FVector MaxVal(-(WORLD_MAX/2.0f));
		FLOAT LocalMax(0.0f);
		for (INT i=0; i<ActiveParticles; i++)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
			
			// Do linear integrator and update bounding box
			// Do angular integrator, and wrap result to within +/- 2 PI
			Particle.OldLocation	= Particle.Location;
			if ((Particle.Flags & STATE_Particle_Freeze) == 0)
			{
				if ((Particle.Flags & STATE_Particle_FreezeTranslation) == 0)
				{
					NewLocation	= Particle.Location + (DeltaTime * Particle.Velocity);
				}
				else
				{
					NewLocation	= Particle.Location;
				}
				if ((Particle.Flags & STATE_Particle_FreezeRotation) == 0)
				{
					NewRotation = (DeltaTime * Particle.RotationRate) + Particle.Rotation;
				}
				else
				{
					NewRotation	= Particle.Rotation;
				}
			}
			else
			{
				NewLocation	= Particle.Location;
				NewRotation	= Particle.Rotation;
			}

			if (bUpdateBox)
			{	
				if (OrbitOffsetValue == -1)
				{
					LocalMax = (Particle.Size * Scale).GetAbsMax();
				}
				else
				{
					INT CurrentOffset = OrbitOffsetValue;
					const BYTE* ParticleBase = (const BYTE*)&Particle;
					PARTICLE_ELEMENT(FOrbitChainModuleInstancePayload, OrbitPayload);
					LocalMax = OrbitPayload.Offset.GetAbsMax();
				}
			}

			Particle.Location	 = NewLocation;
			Particle.Rotation	 = appFmod(NewRotation, 2.f*(FLOAT)PI);

			if (bUpdateBox)
			{	
				// Treat each particle as a cube whose sides are the length of the maximum component
				// This handles the particle's extents changing due to being camera facing
				MinVal[0] = Min<FLOAT>(MinVal[0], NewLocation.X - LocalMax);
				MaxVal[0] = Max<FLOAT>(MaxVal[0], NewLocation.X + LocalMax);
				MinVal[1] = Min<FLOAT>(MinVal[1], NewLocation.Y - LocalMax);
				MaxVal[1] = Max<FLOAT>(MaxVal[1], NewLocation.Y + LocalMax);
				MinVal[2] = Min<FLOAT>(MinVal[2], NewLocation.Z - LocalMax);
				MaxVal[2] = Max<FLOAT>(MaxVal[2], NewLocation.Z + LocalMax);
			}
		}

		if (bUpdateBox)
		{
			ParticleBoundingBox = FBox(MinVal, MaxVal);
			// Transform bounding box into world space if the emitter uses a local space coordinate system.
			if (LODLevel->RequiredModule->bUseLocalSpace) 
			{
				ParticleBoundingBox = ParticleBoundingBox.TransformBy(Component->LocalToWorld);
			}
		}
	}
}

/**
 *	Retrieved the per-particle bytes that this emitter type requires.
 *
 *	@return	UINT	The number of required bytes for particles in the instance
 */
UINT FParticleEmitterInstance::RequiredBytes()
{
	// If ANY LOD level has subUV, the size must be taken into account.
	UINT uiBytes = 0;
	UBOOL bHasSubUV = FALSE;
	for (INT LODIndex = 0; (LODIndex < SpriteTemplate->LODLevels.Num()) && !bHasSubUV; LODIndex++)
	{
		// This code assumes that the module stacks are identical across LOD levevls...
		UParticleLODLevel* LODLevel = SpriteTemplate->GetLODLevel(LODIndex);
		
		if (LODLevel)
		{
			EParticleSubUVInterpMethod	InterpolationMethod	= (EParticleSubUVInterpMethod)LODLevel->RequiredModule->InterpolationMethod;
			if (LODIndex > 0)
			{
				if ((InterpolationMethod != PSUVIM_None) && (bHasSubUV == FALSE))
				{
					warnf(TEXT("Emitter w/ mismatched SubUV settings: %s"),
						Component ? 
							Component->Template ? 
								*(Component->Template->GetPathName()) :
								*(Component->GetFullName()) :
							TEXT("INVALID PSYS!"));
				}

				if ((InterpolationMethod == PSUVIM_None) && (bHasSubUV == TRUE))
				{
					warnf(TEXT("Emitter w/ mismatched SubUV settings: %s"),
						Component ? 
						Component->Template ? 
						*(Component->Template->GetPathName()) :
					*(Component->GetFullName()) :
					TEXT("INVALID PSYS!"));
				}
			}
			// Check for SubUV utilization, and update the required bytes accordingly
			if (InterpolationMethod != PSUVIM_None)
			{
				bHasSubUV = TRUE;
			}
		}
	}

	if (bHasSubUV)
	{
		SubUVDataOffset = PayloadOffset;
		uiBytes	= sizeof(FFullSubUVPayload);
	}

	return uiBytes;
}

/**
 *	Get the pointer to the instance data allocated for a given module.
 *
 *	@param	Module		The module to retrieve the data block for.
 *	@return	BYTE*		The pointer to the data
 */
BYTE* FParticleEmitterInstance::GetModuleInstanceData(UParticleModule* Module)
{
	// If there is instance data present, look up the modules offset
	if (InstanceData)
	{
		UINT* Offset = ModuleInstanceOffsetMap.Find(Module);
		if (Offset)
		{
			if (*Offset < (UINT)InstancePayloadSize)
			{
				return &(InstanceData[*Offset]);
			}
		}
	}
	return NULL;
}

/**
 *	Get the pointer to the instance data allocated for type data module.
 *
 *	@return	BYTE*		The pointer to the data
 */
BYTE* FParticleEmitterInstance::GetTypeDataModuleInstanceData()
{
	if (InstanceData && (TypeDataInstanceOffset != -1))
	{
		return &(InstanceData[TypeDataInstanceOffset]);
	}
	return NULL;
}

/**
 *	Calculate the stride of a single particle for this instance
 *
 *	@param	ParticleSize	The size of the particle
 *
 *	@return	UINT			The stride of the particle
 */
UINT FParticleEmitterInstance::CalculateParticleStride(UINT ParticleSize)
{
	return ParticleSize;
}

/**
 *	Reset the burst list information for the instance
 */
void FParticleEmitterInstance::ResetBurstList()
{
	for (INT BurstIndex = 0; BurstIndex < BurstFired.Num(); BurstIndex++)
	{
		FLODBurstFired& CurrBurstFired = BurstFired(BurstIndex);
		for (INT FiredIndex = 0; FiredIndex < CurrBurstFired.Fired.Num(); FiredIndex++)
		{
			CurrBurstFired.Fired(FiredIndex) = FALSE;
		}
	}
}

/**
 *	Get the current burst rate offset (delta time is artifically increased to generate bursts)
 *
 *	@param	DeltaTime	The time slice (In/Out)
 *	@param	Burst		The number of particles to burst (Output)
 *
 *	@return	FLOAT		The time slice increase to use
 */
FLOAT FParticleEmitterInstance::GetCurrentBurstRateOffset(FLOAT& DeltaTime, INT& Burst)
{
	FLOAT SpawnRateInc = 0.0f;

	// Grab the current LOD level
	UParticleLODLevel* LODLevel	= SpriteTemplate->GetCurrentLODLevel(this);
	check(LODLevel);
	if (LODLevel->SpawnModule->BurstList.Num() > 0)
    {
		// For each burst in the list
        for (INT BurstIdx = 0; BurstIdx < LODLevel->SpawnModule->BurstList.Num(); BurstIdx++)
        {
            FParticleBurst* BurstEntry = &(LODLevel->SpawnModule->BurstList(BurstIdx));
			// If it hasn't been fired
			if (LODLevel->Level < BurstFired.Num())
			{
				FLODBurstFired& LocalBurstFired = BurstFired(LODLevel->Level);
				if (BurstIdx < LocalBurstFired.Fired.Num())
				{
					if (LocalBurstFired.Fired(BurstIdx) == FALSE)
					{
						// If it is time to fire it
						if (EmitterTime >= BurstEntry->Time)
						{
							// Make sure there is a valid time slice
							if (DeltaTime < 0.00001f)
							{
								DeltaTime = 0.00001f;
							}
							// Calculate the increase time slice
							INT Count = BurstEntry->Count;
							if (BurstEntry->CountLow > -1)
							{
								Count = BurstEntry->CountLow + appRound(appSRand() * (FLOAT)(BurstEntry->Count - BurstEntry->CountLow));
							}
							SpawnRateInc += Count / DeltaTime;
							Burst += Count;
							LocalBurstFired.Fired(BurstIdx) = TRUE;
						}
					}
				}
			}
        }
   }

	return SpawnRateInc;
}

/**
 *	Reset the particle parameters
 */
void FParticleEmitterInstance::ResetParticleParameters(FLOAT DeltaTime, DWORD StatId)
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	check(LODLevel);
	UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels(0);
	check(HighestLODLevel);

	// Store off any orbit offset values
	TArray<INT> OrbitOffsets;
	INT OrbitCount = LODLevel->OrbitModules.Num();
	for (INT OrbitIndex = 0; OrbitIndex < OrbitCount; OrbitIndex++)
	{
		UParticleModuleOrbit* OrbitModule = HighestLODLevel->OrbitModules(OrbitIndex);
		if (OrbitModule)
		{
			UINT* OrbitOffset = ModuleOffsetMap.Find(OrbitModule);
			if (OrbitOffset)
			{
				OrbitOffsets.AddItem(*OrbitOffset);
			}
		}
	}

	for (INT ParticleIndex = 0; ParticleIndex < ActiveParticles; ParticleIndex++)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[ParticleIndex]);
		Particle.Velocity		= Particle.BaseVelocity;
		Particle.Size			= Particle.BaseSize;
		Particle.RotationRate	= Particle.BaseRotationRate;
		Particle.Color			= Particle.BaseColor;
		Particle.RelativeTime	+= Particle.OneOverMaxLifetime * DeltaTime;

		if (CameraPayloadOffset > 0)
		{
			INT CurrentOffset = CameraPayloadOffset;
			const BYTE* ParticleBase = (const BYTE*)&Particle;
			PARTICLE_ELEMENT(FCameraOffsetParticlePayload, CameraOffsetPayload);
			CameraOffsetPayload.Offset = CameraOffsetPayload.BaseOffset;
		}
		for (INT OrbitIndex = 0; OrbitIndex < OrbitOffsets.Num(); OrbitIndex++)
		{
			INT CurrentOffset = OrbitOffsets(OrbitIndex);
			const BYTE* ParticleBase = (const BYTE*)&Particle;
			PARTICLE_ELEMENT(FOrbitChainModuleInstancePayload, OrbitPayload);
			OrbitPayload.PreviousOffset = OrbitPayload.Offset;
			OrbitPayload.Offset = OrbitPayload.BaseOffset;
			OrbitPayload.RotationRate = OrbitPayload.BaseRotationRate;
		}
	}
	INC_DWORD_STAT_BY(StatId, ActiveParticles);
}

/**
 *	Calculate the orbit offset data.
 */
void FParticleEmitterInstance::CalculateOrbitOffset(FOrbitChainModuleInstancePayload& Payload, 
	FVector& AccumOffset, FVector& AccumRotation, FVector& AccumRotationRate, 
	FLOAT DeltaTime, FVector& Result, FMatrix& RotationMat)
{
	AccumRotation += AccumRotationRate * DeltaTime;
	Payload.Rotation = AccumRotation;
	if (AccumRotation.IsNearlyZero() == FALSE)
	{
		FVector RotRot = RotationMat.TransformNormal(AccumRotation);
		FVector ScaledRotation = RotRot * 360.0f;
		FRotator Rotator = FRotator::MakeFromEuler(ScaledRotation);
		FMatrix RotMat = FRotationMatrix(Rotator);

		RotationMat *= RotMat;

		Result = RotationMat.TransformFVector(AccumOffset);
	}
	else
	{
		Result = AccumOffset;
	}

	AccumOffset.X = 0.0f;;
	AccumOffset.Y = 0.0f;;
	AccumOffset.Z = 0.0f;;
	AccumRotation.X = 0.0f;;
	AccumRotation.Y = 0.0f;;
	AccumRotation.Z = 0.0f;;
	AccumRotationRate.X = 0.0f;;
	AccumRotationRate.Y = 0.0f;;
	AccumRotationRate.Z = 0.0f;;
}

void FParticleEmitterInstance::UpdateOrbitData(FLOAT DeltaTime)
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	check(LODLevel);
	INT ModuleCount = LODLevel->OrbitModules.Num();
	if (ModuleCount > 0)
	{
		UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels(0);
		check(HighestLODLevel);

		TArray<FVector> Offsets;
		Offsets.AddZeroed(ModuleCount + 1);

		TArray<INT> ModuleOffsets;
		ModuleOffsets.AddZeroed(ModuleCount + 1);
		for (INT ModOffIndex = 0; ModOffIndex < ModuleCount; ModOffIndex++)
		{
			UParticleModuleOrbit* HighestOrbitModule = HighestLODLevel->OrbitModules(ModOffIndex);
			check(HighestOrbitModule);

			UINT* ModuleOffset = ModuleOffsetMap.Find(HighestOrbitModule);
			if (ModuleOffset == NULL)
			{
				ModuleOffsets(ModOffIndex) = 0;
			}
			else
			{
				ModuleOffsets(ModOffIndex) = (INT)(*ModuleOffset);
			}
		}

		for(INT i=ActiveParticles-1; i>=0; i--)
		{
			INT OffsetIndex = 0;
			const INT	CurrentIndex	= ParticleIndices[i];
			const BYTE* ParticleBase	= ParticleData + CurrentIndex * ParticleStride;
			FBaseParticle& Particle		= *((FBaseParticle*) ParticleBase);
			if ((Particle.Flags & STATE_Particle_Freeze) == 0)
			{
				FVector AccumulatedOffset = FVector(0.0f);
				FVector AccumulatedRotation = FVector(0.0f);
				FVector AccumulatedRotationRate = FVector(0.0f);

				FOrbitChainModuleInstancePayload* LocalOrbitPayload = NULL;
				FOrbitChainModuleInstancePayload* PrevOrbitPayload = NULL;
				FMatrix AccumRotMatrix;
				AccumRotMatrix.SetIdentity();

				INT ActiveOrbitIndex = 0;

				for (INT OrbitIndex = 0; OrbitIndex < ModuleCount; OrbitIndex++)
				{
					INT CurrentOffset = ModuleOffsets(OrbitIndex);
					UParticleModuleOrbit* OrbitModule = LODLevel->OrbitModules(OrbitIndex);
					check(OrbitModule);

					if (OrbitModule->bEnabled == FALSE || CurrentOffset == 0)
					{
						continue;
					}

					PARTICLE_ELEMENT(FOrbitChainModuleInstancePayload, OrbitPayload);

					LocalOrbitPayload = &OrbitPayload;

					// Determine the offset, rotation, rotationrate for the current particle
					if (OrbitModule->ChainMode == EOChainMode_Add)
					{
						AccumulatedOffset += OrbitPayload.Offset;
						AccumulatedRotation += OrbitPayload.Rotation;
						AccumulatedRotationRate += OrbitPayload.RotationRate;
					}
					else
					if (OrbitModule->ChainMode == EOChainMode_Scale)
					{
						AccumulatedOffset *= OrbitPayload.Offset;
						AccumulatedRotation *= OrbitPayload.Rotation;
						AccumulatedRotationRate *= OrbitPayload.RotationRate;
					}
					else
					if (OrbitModule->ChainMode == EOChainMode_Link)
					{
						//We do not want to do this on the first active orbit module.
						if(ActiveOrbitIndex > 0)
						{
							// Calculate the offset with the current accumulation
							FVector ResultOffset;
							CalculateOrbitOffset(*PrevOrbitPayload, 
								AccumulatedOffset, AccumulatedRotation, AccumulatedRotationRate, 
								DeltaTime, ResultOffset, AccumRotMatrix);
							Offsets(OffsetIndex++) = ResultOffset;
						}

						AccumulatedOffset = OrbitPayload.Offset;
						AccumulatedRotation = OrbitPayload.Rotation;
						AccumulatedRotationRate = OrbitPayload.RotationRate;
					}

					PrevOrbitPayload = &OrbitPayload;

					ActiveOrbitIndex++;
				}

				INT CurrentOffset = ModuleOffsets(ModuleCount - 1);

				if(LocalOrbitPayload != NULL)
				{
					// Push the current offset into the array
					FVector ResultOffset;
					CalculateOrbitOffset(*LocalOrbitPayload, 
						AccumulatedOffset, AccumulatedRotation, AccumulatedRotationRate, 
						DeltaTime, ResultOffset, AccumRotMatrix);
					Offsets(OffsetIndex++) = ResultOffset;
				}

				// The last orbit module holds the last final offset position
				PARTICLE_ELEMENT(FOrbitChainModuleInstancePayload, OrbitPayload);
				LocalOrbitPayload = &OrbitPayload;

				//Combine all the previously saved offsets into the final payload.
				if (LocalOrbitPayload != NULL)
				{
					LocalOrbitPayload->Offset = FVector(0.0f);
					for (INT AccumIndex = 0; AccumIndex < OffsetIndex; AccumIndex++)
					{
						LocalOrbitPayload->Offset += Offsets(AccumIndex);
					}

					appMemzero(Offsets.GetData(), sizeof(FVector) * (ModuleCount + 1));
				}
			}
		}
	}
}

void FParticleEmitterInstance::ParticlePrefetch()
{
	for (INT ParticleIndex = 0; ParticleIndex < ActiveParticles; ParticleIndex++)
	{
		PARTICLE_INSTANCE_PREFETCH(this, ParticleIndex);
	}
}

void FParticleEmitterInstance::CheckSpawnCount(UBOOL bIsSubUV, INT InNewCount, INT InMaxCount)
{
#if !(FINAL_RELEASE || SHIPPING_PC_GAME)
	AWorldInfo* WorldInfo = GWorld ? GWorld->GetWorldInfo() : NULL;
	if (WorldInfo)
	{
		const INT SizeScalar = (bIsSubUV == FALSE) ? sizeof(FParticleSpriteVertex) : sizeof(FParticleSpriteSubUVVertex);

		INT MyIndex = -1;
		for (INT CheckIdx = 0; CheckIdx < Component->EmitterInstances.Num(); CheckIdx++)
		{
			if (Component->EmitterInstances(CheckIdx) == this)
			{
				MyIndex = CheckIdx;
				break;
			}
		}

		FString ErrorMessage = 
			FString::Printf(TEXT("Emitter %2d spawn vertices: %10d (%8.3f kB of verts), clamp to %10d (%8.3f kB) - spawned %4d: %s"),
			MyIndex,
			InNewCount, 
			(FLOAT)(InNewCount * 4 * SizeScalar) / 1024.0f,
			InMaxCount, 
			(FLOAT)(InMaxCount * 4 * SizeScalar) / 1024.0f,
			InNewCount - ActiveParticles,
			Component ? Component->Template ? *(Component->Template->GetPathName()) : TEXT("No template") : TEXT("No component"));
		FColor ErrorColor(255,255,0);
		if (WorldInfo->OnScreenDebugMessageExists((QWORD)(0x8000000 | (PTRINT)this)) == FALSE)
		{
			debugf(*ErrorMessage);
		}
		WorldInfo->AddOnScreenDebugMessage((QWORD)(0x8000000 | (PTRINT)this), 5.0f, ErrorColor,ErrorMessage);
	}
#endif	//#if !FINAL_RELEASE
}

/**
 *	Spawn particles for this emitter instance
 *
 *	@param	DeltaTime		The time slice to spawn over
 *
 *	@return	FLOAT			The leftover fraction of spawning
 */
FLOAT FParticleEmitterInstance::Spawn(FLOAT DeltaTime)
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	check(LODLevel);
	check(LODLevel->RequiredModule);

	// For beams, we probably want to ignore the SpawnRate distribution,
	// and focus strictly on the BurstList...
	FLOAT SpawnRate = 0.0f;
	INT SpawnCount = 0;
	INT BurstCount = 0;
	FLOAT SpawnRateDivisor = 0.0f;
	FLOAT OldLeftover = SpawnFraction;

	UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels(0);

	UBOOL bProcessSpawnRate = TRUE;
	UBOOL bProcessBurstList = TRUE;

	if ((GSystemSettings.DetailMode == DM_High) || (SpriteTemplate->MediumDetailSpawnRateScale > 0.0f))
	{
		// Process all Spawning modules that are present in the emitter.
		for (INT SpawnModIndex = 0; SpawnModIndex < LODLevel->SpawningModules.Num(); SpawnModIndex++)
		{
			UParticleModuleSpawnBase* SpawnModule = LODLevel->SpawningModules(SpawnModIndex);
			if (SpawnModule && SpawnModule->bEnabled)
			{
				UParticleModule* OffsetModule = HighestLODLevel->SpawningModules(SpawnModIndex);
				UINT* Offset = ModuleOffsetMap.Find(OffsetModule);

				// Update the spawn rate
				INT Number = 0;
				FLOAT Rate = 0.0f;
				if (SpawnModule->GetSpawnAmount(this, Offset ? *Offset : 0, OldLeftover, DeltaTime, Number, Rate) == FALSE)
				{
					bProcessSpawnRate = FALSE;
				}

				Number = Max<INT>(0, Number);
				Rate = Max<FLOAT>(0.0f, Rate);

				SpawnCount += Number;
				SpawnRate += Rate;
				// Update the burst list
				INT BurstNumber = 0;
				if (SpawnModule->GetBurstCount(this, Offset ? *Offset : 0, OldLeftover, DeltaTime, BurstNumber) == FALSE)
				{
					bProcessBurstList = FALSE;
				}

				BurstCount += BurstNumber;
			}
		}

		// Figure out spawn rate for this tick.
		if (bProcessSpawnRate)
		{
			FLOAT RateScale = LODLevel->SpawnModule->RateScale.GetValue(EmitterTime, Component);
			SpawnRate += LODLevel->SpawnModule->Rate.GetValue(EmitterTime, Component) * RateScale;
			SpawnRate = Max<FLOAT>(0.0f, SpawnRate);
		}

		// Take Bursts into account as well...
		if (bProcessBurstList)
		{
			INT Burst = 0;
			FLOAT BurstTime = GetCurrentBurstRateOffset(DeltaTime, Burst);
			BurstCount += Burst;
		}

		if (GSystemSettings.DetailMode != DM_High)
		{
			SpawnRate = Max<FLOAT>(0.0f, SpawnRate * SpriteTemplate->MediumDetailSpawnRateScale);
			BurstCount = appCeil(BurstCount * SpriteTemplate->MediumDetailSpawnRateScale);
		}
	}
	else
	{
		// Disable any spawning if MediumDetailSpawnRateScale is 0 and we are not in high detail mode
		SpawnRate = 0.0f;
		SpawnCount = 0;
		BurstCount = 0;
	}

	// Spawn new particles...
	if ((SpawnRate > 0.f) || (BurstCount > 0))
	{
		FLOAT SafetyLeftover = OldLeftover;
		// Ensure continuous spawning... lots of fiddling.
		FLOAT	NewLeftover = OldLeftover + DeltaTime * SpawnRate;
		INT		Number		= appFloor(NewLeftover);
		FLOAT	Increment	= (SpawnRate > 0.0f) ? (1.f / SpawnRate) : 0.0f;
		FLOAT	StartTime	= DeltaTime + OldLeftover * Increment - Increment;
		NewLeftover			= NewLeftover - Number;

		// Handle growing arrays.
		UBOOL bProcessSpawn = TRUE;
		INT NewCount = ActiveParticles + Number + BurstCount;

#if 0
		//@todo.SAS. Check something similar to MaxDrawCount here...
		if (LODLevel->RequiredModule && LODLevel->RequiredModule->bUseMaxDrawCount)
		{
			if (NewCount > LODLevel->RequiredModule->MaxDrawCount)
			{
				NewCount = LODLevel->RequiredModule->MaxDrawCount;
			}
		}
#endif
		if ((GEngine->MaxParticleVertexMemory > 0) && Component && (Component->bSkipSpawnCountCheck == FALSE))
		{
			INT MaxCount;
			if (LODLevel->RequiredModule->InterpolationMethod == PSUVIM_None)
			{
				check(GEngine->MaxParticleSpriteCount > 0);
				MaxCount = GEngine->MaxParticleSpriteCount;
			}
			else
			{
				check(GEngine->MaxParticleSubUVCount > 0);
				MaxCount = GEngine->MaxParticleSubUVCount;
			}
			if (NewCount > MaxCount)
			{
#if !(FINAL_RELEASE || SHIPPING_PC_GAME)
				CheckSpawnCount((LODLevel->RequiredModule->InterpolationMethod != PSUVIM_None), NewCount, MaxCount);
#endif	//#if !FINAL_RELEASE
				NewCount = MaxCount;
				INT NewParticleCount = NewCount - ActiveParticles;
				// Burst gets priority
				if ((NewParticleCount > 0) && (BurstCount > 0))
				{
					BurstCount = Min<INT>(NewParticleCount, BurstCount);
					BurstCount = Clamp<INT>(BurstCount, 0, NewParticleCount);
					NewParticleCount -= BurstCount;
				}
				else
				{
					BurstCount = 0;
				}
				if ((NewParticleCount > 0) && (Number > 0))
				{
					Number = Min<INT>(NewParticleCount, Number);
					Number = Clamp<INT>(Number, 0, NewParticleCount);
				}
				else
				{
					Number = 0;
				}
			}
		}

		if (NewCount >= MaxActiveParticles)
		{
			if (DeltaTime < PeakActiveParticleUpdateDelta)
			{
				bProcessSpawn = Resize(NewCount + appTrunc(appSqrt(appSqrt((FLOAT)NewCount)) + 1));
			}
			else
			{
				bProcessSpawn = Resize((NewCount + appTrunc(appSqrt(appSqrt((FLOAT)NewCount)) + 1)), FALSE);
			}
		}

		if (bProcessSpawn == TRUE)
		{
			FParticleEventInstancePayload* EventPayload = NULL;
			if (LODLevel->EventGenerator)
			{
				EventPayload = (FParticleEventInstancePayload*)GetModuleInstanceData(LODLevel->EventGenerator);
				if (EventPayload && (EventPayload->bSpawnEventsPresent == FALSE))
				{
					EventPayload = NULL;
				}
			}

			// Spawn particles.
			UParticleLODLevel* HighestLODLevel2 = SpriteTemplate->LODLevels(0);
			for (INT i=0; i<Number; i++)
			{
				check(ActiveParticles <= MaxActiveParticles);
				DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[ActiveParticles]);

				FLOAT SpawnTime = StartTime - i * Increment;

				PreSpawn(Particle);

				if (LODLevel->TypeDataModule)
				{
					UParticleModuleTypeDataBase* pkBase = Cast<UParticleModuleTypeDataBase>(LODLevel->TypeDataModule);
					pkBase->Spawn(this, TypeDataOffset, SpawnTime);
				}

				for (INT ModuleIndex = 0; ModuleIndex < LODLevel->SpawnModules.Num(); ModuleIndex++)
				{
					UParticleModule* SpawnModule	= LODLevel->SpawnModules(ModuleIndex);
					if (SpawnModule->bEnabled)
					{
						UParticleModule* OffsetModule	= HighestLODLevel2->SpawnModules(ModuleIndex);
						UINT* Offset = ModuleOffsetMap.Find(OffsetModule);
						SpawnModule->Spawn(this, Offset ? *Offset : 0, SpawnTime);
					}
				}
				PostSpawn(Particle, 1.f - FLOAT(i+1) / FLOAT(Number), SpawnTime);

				ActiveParticles++;

				if (EventPayload)
				{
					LODLevel->EventGenerator->HandleParticleSpawned(this, EventPayload, Particle);
				}

				INC_DWORD_STAT(STAT_SpriteParticlesSpawned);
			}

			// Burst particles.
			for (INT BurstIndex = 0; BurstIndex < BurstCount; BurstIndex++)
			{
				check(ActiveParticles <= MaxActiveParticles);
				DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[ActiveParticles]);

				FLOAT SpawnTime = 0.0f;

				PreSpawn(Particle);

				if (LODLevel->TypeDataModule)
				{
					UParticleModuleTypeDataBase* pkBase = Cast<UParticleModuleTypeDataBase>(LODLevel->TypeDataModule);
					pkBase->Spawn(this, TypeDataOffset, SpawnTime);
				}

				for (INT ModuleIndex = 0; ModuleIndex < LODLevel->SpawnModules.Num(); ModuleIndex++)
				{
					UParticleModule* SpawnModule	= LODLevel->SpawnModules(ModuleIndex);
					if (SpawnModule->bEnabled)
					{
						UParticleLODLevel* HighestLODLevel2 = SpriteTemplate->LODLevels(0);
						UParticleModule* OffsetModule	= 	HighestLODLevel2->SpawnModules(ModuleIndex);
						UINT* Offset = ModuleOffsetMap.Find(OffsetModule);
						SpawnModule->Spawn(this, Offset ? *Offset : 0, SpawnTime);
					}
				}
				PostSpawn(Particle, 0.0f, SpawnTime);

				ActiveParticles++;

				if (EventPayload)
				{
					LODLevel->EventGenerator->HandleParticleSpawned(this, EventPayload, Particle);
				}

				INC_DWORD_STAT(STAT_SpriteParticlesSpawned);
			}

			return NewLeftover;
		}
		return SafetyLeftover;
	}

	return SpawnFraction;
}

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
void FParticleEmitterInstance::ForceSpawn(FLOAT DeltaTime, INT InSpawnCount, INT InBurstCount, 
	FVector& InLocation, FVector& InVelocity)
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	check(LODLevel);

	// For beams, we probably want to ignore the SpawnRate distribution,
	// and focus strictly on the BurstList...
	INT SpawnCount = InSpawnCount;
	INT BurstCount = InBurstCount;
	FLOAT SpawnRateDivisor = 0.0f;
	FLOAT OldLeftover = 0.0f;

	UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels(0);

	UBOOL bProcessSpawnRate = TRUE;
	UBOOL bProcessBurstList = TRUE;

	// Spawn new particles...
	if ((SpawnCount > 0) || (BurstCount > 0))
	{
		INT		Number		= SpawnCount;
		FLOAT	Increment	= (SpawnCount > 0) ? (DeltaTime / SpawnCount) : 0;
		FLOAT	StartTime	= DeltaTime;
		
		// Handle growing arrays.
		UBOOL bProcessSpawn = TRUE;
		INT NewCount = ActiveParticles + Number + BurstCount;
		if (NewCount >= MaxActiveParticles)
		{
			if (DeltaTime < PeakActiveParticleUpdateDelta)
			{
				bProcessSpawn = Resize(NewCount + appTrunc(appSqrt(appSqrt((FLOAT)NewCount)) + 1));
			}
			else
			{
				bProcessSpawn = Resize((NewCount + appTrunc(appSqrt(appSqrt((FLOAT)NewCount)) + 1)), FALSE);
			}
		}

		if (bProcessSpawn == TRUE)
		{
/***
			//@todo.SAS. If we are allowing events-->process-->events, then this goes back in!
			FParticleEventInstancePayload* EventPayload = NULL;
			if (LODLevel->EventGenerator)
			{
				EventPayload = (FParticleEventInstancePayload*)GetModuleInstanceData(LODLevel->EventGenerator);
				if (EventPayload && (EventPayload->bSpawnEventsPresent == FALSE))
				{
					EventPayload = NULL;
				}
			}
***/
			// Spawn particles.
			UParticleLODLevel* HighestLODLevel2 = SpriteTemplate->LODLevels(0);
			for (INT i=0; i<Number; i++)
			{
				check(ActiveParticles <= MaxActiveParticles);
				DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[ActiveParticles]);

				FLOAT SpawnTime = StartTime - i * Increment;

				PreSpawn(Particle);

				// Override the location and velocity!
				Particle->Location = InLocation;
				Particle->BaseVelocity = InVelocity;
				Particle->Velocity = InVelocity;

				if (LODLevel->TypeDataModule)
				{
					UParticleModuleTypeDataBase* pkBase = Cast<UParticleModuleTypeDataBase>(LODLevel->TypeDataModule);
					pkBase->Spawn(this, TypeDataOffset, SpawnTime);
				}

				for (INT ModuleIndex = 0; ModuleIndex < LODLevel->SpawnModules.Num(); ModuleIndex++)
				{
					UParticleModule* SpawnModule	= LODLevel->SpawnModules(ModuleIndex);
					UParticleModule* OffsetModule	= HighestLODLevel2->SpawnModules(ModuleIndex);
					UINT* Offset = ModuleOffsetMap.Find(OffsetModule);
					if (SpawnModule->bEnabled)
					{
						SpawnModule->Spawn(this, Offset ? *Offset : 0, SpawnTime);
					}
				}
				PostSpawn(Particle, 1.f - FLOAT(i+1) / FLOAT(Number), SpawnTime);

				ActiveParticles++;

/***
			//@todo.SAS. If we are allowing events-->process-->events, then this goes back in!
				if (EventPayload)
				{
					LODLevel->EventGenerator->HandleParticleSpawned(this, EventPayload, Particle);
				}
***/
				INC_DWORD_STAT(STAT_SpriteParticlesSpawned);
			}

			// Burst particles.
			for (INT BurstIndex = 0; BurstIndex < BurstCount; BurstIndex++)
			{
				check(ActiveParticles <= MaxActiveParticles);
				DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[ActiveParticles]);

				FLOAT SpawnTime = 0.0f;

				PreSpawn(Particle);	

				// Override the location and velocity!
				Particle->Location = InLocation;
				Particle->BaseVelocity = InVelocity;
				Particle->Velocity = InVelocity;

				if (LODLevel->TypeDataModule)
				{
					UParticleModuleTypeDataBase* pkBase = Cast<UParticleModuleTypeDataBase>(LODLevel->TypeDataModule);
					pkBase->Spawn(this, TypeDataOffset, SpawnTime);
				}

				for (INT ModuleIndex = 0; ModuleIndex < LODLevel->SpawnModules.Num(); ModuleIndex++)
				{
					UParticleModule* SpawnModule	= LODLevel->SpawnModules(ModuleIndex);

					UParticleLODLevel* HighestLODLevel2 = SpriteTemplate->LODLevels(0);
					UParticleModule* OffsetModule	= 	HighestLODLevel2->SpawnModules(ModuleIndex);
					UINT* Offset = ModuleOffsetMap.Find(OffsetModule);

					if (SpawnModule->bEnabled)
					{
						SpawnModule->Spawn(this, Offset ? *Offset : 0, SpawnTime);
					}
				}
				PostSpawn(Particle, 0.0f, SpawnTime);

				ActiveParticles++;

/***
			//@todo.SAS. If we are allowing events-->process-->events, then this goes back in!
				if (EventPayload)
				{
					LODLevel->EventGenerator->HandleParticleSpawned(this, EventPayload, Particle);
				}
***/
				INC_DWORD_STAT(STAT_SpriteParticlesSpawned);
			}
		}
	}
}

/**
 *	Spawn particles for this instance
 *
 *	@param	OldLeftover		The leftover time from the last spawn
 *	@param	Rate			The rate at which particles should be spawned
 *	@param	DeltaTime		The time slice to spawn over
 *	@param	Burst			The number of burst particle
 *	@param	BurstTime		The burst time addition (faked time slice)
 *
 *	@return	FLOAT			The leftover fraction of spawning
 */
FLOAT FParticleEmitterInstance::Spawn(FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime, INT Burst, FLOAT BurstTime)
{
	FLOAT SafetyLeftover = OldLeftover;
	// Ensure continous spawning... lots of fiddling.
	FLOAT	NewLeftover = OldLeftover + DeltaTime * Rate;
	INT		Number		= appFloor(NewLeftover);
	FLOAT	Increment	= 1.f / Rate;
	FLOAT	StartTime	= DeltaTime + OldLeftover * Increment - Increment;
	NewLeftover			= NewLeftover - Number;

	// If we have calculated less than the burst count, force the burst count
	if (Number < Burst)
	{
		Number = Burst;
	}

	// Take the burst time fakery into account
	if (BurstTime > 0.0f)
	{
		NewLeftover -= BurstTime / Burst;
		NewLeftover	= Clamp<FLOAT>(NewLeftover, 0, NewLeftover);
	}

	// Handle growing arrays.
	UBOOL bProcessSpawn = TRUE;
	INT NewCount = ActiveParticles + Number;
	if (NewCount >= MaxActiveParticles)
	{
		if (DeltaTime < PeakActiveParticleUpdateDelta)
		{
			bProcessSpawn = Resize(NewCount + appTrunc(appSqrt((FLOAT)NewCount)) + 1);
		}
		else
		{
			bProcessSpawn = Resize((NewCount + appTrunc(appSqrt((FLOAT)NewCount)) + 1), FALSE);
		}
	}

	if (bProcessSpawn == TRUE)
	{
		UParticleLODLevel* LODLevel	= SpriteTemplate->GetCurrentLODLevel(this);
		check(LODLevel);
		// Spawn particles.
		for (INT i=0; i<Number; i++)
		{
			check(ActiveParticles <= MaxActiveParticles);
			DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[ActiveParticles]);

			FLOAT SpawnTime = StartTime - i * Increment;
		
			PreSpawn(Particle);

			if (LODLevel->TypeDataModule)
			{
				UParticleModuleTypeDataBase* pkBase = Cast<UParticleModuleTypeDataBase>(LODLevel->TypeDataModule);
				pkBase->Spawn(this, TypeDataOffset, SpawnTime);
			}

			for (INT ModuleIndex = 0; ModuleIndex < LODLevel->SpawnModules.Num(); ModuleIndex++)
			{
				UParticleModule* SpawnModule	= LODLevel->SpawnModules(ModuleIndex);

				UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels(0);
				UParticleModule* OffsetModule	= 	HighestLODLevel->SpawnModules(ModuleIndex);
				UINT* Offset = ModuleOffsetMap.Find(OffsetModule);
				
				if (SpawnModule->bEnabled)
				{
					SpawnModule->Spawn(this, Offset ? *Offset : 0, SpawnTime);
				}
			}
			PostSpawn(Particle, 1.f - FLOAT(i+1) / FLOAT(Number), SpawnTime);

			//@todo. Check particle lifetime and if >= 1.0, kill it.
			// This will allow for Spawn modules to 'kill off' a particle.

			ActiveParticles++;
		}
		INC_DWORD_STAT_BY(STAT_SpriteParticlesSpawned,Number);

		return NewLeftover;
	}

	return SafetyLeftover;
}

/**
 *	Handle any pre-spawning actions required for particles
 *
 *	@param	Particle	The particle being spawned.
 */
void FParticleEmitterInstance::PreSpawn(FBaseParticle* Particle)
{
	check(Particle);
	// This isn't a problem w/ the appMemzero call - it's a problem in general!
	check(ParticleSize > 0);

	// By default, just clear out the particle
	appMemzero(Particle, ParticleSize);

#if WITH_MOBILE_RHI
	if (GUsingMobileRHI || GEmulateMobileRendering)
	{
		//For legacy reasons, all platforms except mobile should defaul their Color to black
		//For mobile, since we always use vertex color, we need to force color to white when unspecified
		//Otherwise, no particles will draw
		Particle->BaseColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
		Particle->Color = Particle->BaseColor;
	}
#endif

	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	check(LODLevel);
	if (LODLevel->RequiredModule->bUseLocalSpace == FALSE)
	{
		// If not using local space, initialize the particle location
		Particle->Location = Location;
	}
}

/**
 *	Has the instance completed it's run?
 *
 *	@return	UBOOL	TRUE if the instance is completed, FALSE if not
 */
UBOOL FParticleEmitterInstance::HasCompleted()
{
	// Validity check
	if (SpriteTemplate == NULL)
	{
		return TRUE;
	}

	// If it hasn't finished looping or if it loops forever, not completed.
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	check(LODLevel);
	if ((LODLevel->RequiredModule->EmitterLoops == 0) || 
		(SecondsSinceCreation < (EmitterDuration * LODLevel->RequiredModule->EmitterLoops)))
	{
		return FALSE;
	}

	// If there are active particles, not completed
	if (ActiveParticles > 0)
	{
		return FALSE;
	}

	return TRUE;
}

/**
 *	Handle any post-spawning actions required by the instance
 *
 *	@param	Particle					The particle that was spawned
 *	@param	InterpolationPercentage		The percentage of the time slice it was spawned at
 *	@param	SpawnTIme					The time it was spawned at
 */
void FParticleEmitterInstance::PostSpawn(FBaseParticle* Particle, FLOAT InterpolationPercentage, FLOAT SpawnTime)
{
	// Interpolate position if using world space.
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	check(LODLevel);
	if (LODLevel->RequiredModule->bUseLocalSpace == FALSE)
	{
		if (FDistSquared(OldLocation, Location) > 1.f)
		{
			Particle->Location += InterpolationPercentage * (OldLocation - Location);	
		}
	}

	// Offset caused by any velocity
	Particle->OldLocation = Particle->Location;
	Particle->Location   += SpawnTime * Particle->Velocity;
}

/**
 *	Kill off any dead particles. (Remove them from the active array)
 */
void FParticleEmitterInstance::KillParticles()
{
	if (ActiveParticles > 0)
	{
		UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
		check(LODLevel);
		FParticleEventInstancePayload* EventPayload = NULL;
		if (LODLevel->EventGenerator)
		{
			EventPayload = (FParticleEventInstancePayload*)GetModuleInstanceData(LODLevel->EventGenerator);
			if (EventPayload && (EventPayload->bDeathEventsPresent == FALSE))
			{
				EventPayload = NULL;
			}
		}

		// Loop over the active particles... If their RelativeTime is > 1.0f (indicating they are dead),
		// move them to the 'end' of the active particle list.
		for (INT i=ActiveParticles-1; i>=0; i--)
		{
			const INT	CurrentIndex	= ParticleIndices[i];
			const BYTE* ParticleBase	= ParticleData + CurrentIndex * ParticleStride;
			FBaseParticle& Particle		= *((FBaseParticle*) ParticleBase);
			if (Particle.RelativeTime > 1.0f)
			{
				if (EventPayload)
				{
					LODLevel->EventGenerator->HandleParticleKilled(this, EventPayload, &Particle);
				}
				// Move it to the 'back' of the list
				ParticleIndices[i] = ParticleIndices[ActiveParticles-1];
				ParticleIndices[ActiveParticles-1]	= CurrentIndex;
				ActiveParticles--;

				INC_DWORD_STAT(STAT_SpriteParticlesKilled);
			}
		}
	}
}

/**
 *	Kill the particle at the given instance
 *
 *	@param	Index		The index of the particle to kill.
 */
void FParticleEmitterInstance::KillParticle(INT Index)
{
	if (Index < ActiveParticles)
	{
		UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
		check(LODLevel);
		FParticleEventInstancePayload* EventPayload = NULL;
		if (LODLevel->EventGenerator)
		{
			EventPayload = (FParticleEventInstancePayload*)GetModuleInstanceData(LODLevel->EventGenerator);
			if (EventPayload && (EventPayload->bDeathEventsPresent == FALSE))
			{
				EventPayload = NULL;
			}
		}

		INT KillIndex = ParticleIndices[Index];

		// Handle the kill event, if needed
		if (EventPayload)
		{
			const BYTE* ParticleBase	= ParticleData + KillIndex * ParticleStride;
			FBaseParticle& Particle		= *((FBaseParticle*) ParticleBase);
			LODLevel->EventGenerator->HandleParticleKilled(this, EventPayload, &Particle);
		}

		// Move it to the 'back' of the list
		for (INT i=Index; i < ActiveParticles - 1; i++)
		{
			ParticleIndices[i] = ParticleIndices[i+1];
		}
		ParticleIndices[ActiveParticles-1] = KillIndex;
		ActiveParticles--;

		INC_DWORD_STAT(STAT_SpriteParticlesKilled);
	}
}

/**
 *	This is used to force "kill" particles irrespective of their duration.
 *	Basically, this takes all particles and moves them to the 'end' of the 
 *	particle list so we can insta kill off trailed particles in the level.
 */
void FParticleEmitterInstance::KillParticlesForced(UBOOL bFireEvents)
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	check(LODLevel);
	FParticleEventInstancePayload* EventPayload = NULL;
	if (bFireEvents == TRUE)
	{
		if (LODLevel->EventGenerator)
		{
			EventPayload = (FParticleEventInstancePayload*)GetModuleInstanceData(LODLevel->EventGenerator);
			if (EventPayload && (EventPayload->bDeathEventsPresent == FALSE))
			{
				EventPayload = NULL;
			}
		}
	}

	// Loop over the active particles and kill them.
	// Move them to the 'end' of the active particle list.
	for (INT KillIdx = ActiveParticles - 1; KillIdx >= 0; KillIdx--)
	{
		const INT CurrentIndex = ParticleIndices[KillIdx];
		// Handle the kill event, if needed
		if (EventPayload)
		{
			const BYTE* ParticleBase = ParticleData + CurrentIndex * ParticleStride;
			FBaseParticle& Particle = *((FBaseParticle*) ParticleBase);
			LODLevel->EventGenerator->HandleParticleKilled(this, EventPayload, &Particle);
		}
		ParticleIndices[KillIdx] = ParticleIndices[ActiveParticles - 1];
		ParticleIndices[ActiveParticles - 1] = CurrentIndex;
		ActiveParticles--;

		INC_DWORD_STAT(STAT_SpriteParticlesKilled);
	}
}

/**
 *	Called when the instance if removed from the scene
 *	Perform any actions required, such as removing components, etc.
 */
void FParticleEmitterInstance::RemovedFromScene()
{
}

/**
 *	Retrieve the particle at the given index
 *
 *	@param	Index			The index of the particle of interest
 *
 *	@return	FBaseParticle*	The pointer to the particle. NULL if not present/active
 */
FBaseParticle* FParticleEmitterInstance::GetParticle(INT Index)
{
	// See if the index is valid. If not, return NULL
	if ((Index >= ActiveParticles) || (Index < 0))
	{
		return NULL;
	}

	// Grab and return the particle
	DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[Index]);
	return Particle;
}

FBaseParticle* FParticleEmitterInstance::GetParticleDirect(INT InDirectIndex)
{
	if ((ActiveParticles > 0) && (InDirectIndex < MaxActiveParticles))
	{
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * InDirectIndex);
		return Particle;
	}
	return NULL;
}

/**
 *	Calculates the emitter duration for the instance.
 */
void FParticleEmitterInstance::SetupEmitterDuration()
{
	// Validity check
	if (SpriteTemplate == NULL)
	{
		return;
	}

	// Set up the array for each LOD level
	INT EDCount = EmitterDurations.Num();
	if ((EDCount == 0) || (EDCount != SpriteTemplate->LODLevels.Num()))
	{
		EmitterDurations.Empty();
		EmitterDurations.Insert(0, SpriteTemplate->LODLevels.Num());
	}

	// Calculate the duration for each LOD level
	for (INT LODIndex = 0; LODIndex < SpriteTemplate->LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* TempLOD = SpriteTemplate->LODLevels(LODIndex);
		UParticleModuleRequired* RequiredModule = TempLOD->RequiredModule;

		CurrentDelay = RequiredModule->EmitterDelay + Component->EmitterDelay;
		if (RequiredModule->bEmitterDelayUseRange)
		{
			const FLOAT	Rand	= appFrand();
			CurrentDelay	    = RequiredModule->EmitterDelayLow + 
				((RequiredModule->EmitterDelay - RequiredModule->EmitterDelayLow) * Rand) + Component->EmitterDelay;
		}


		if (RequiredModule->bEmitterDurationUseRange)
		{
			const FLOAT	Rand		= appFrand();
			const FLOAT	Duration	= RequiredModule->EmitterDurationLow + 
				((RequiredModule->EmitterDuration - RequiredModule->EmitterDurationLow) * Rand);
			EmitterDurations(TempLOD->Level) = Duration + CurrentDelay;
		}
		else
		{
			EmitterDurations(TempLOD->Level) = RequiredModule->EmitterDuration + CurrentDelay;
		}

		if ((LoopCount == 1) && (RequiredModule->bDelayFirstLoopOnly == TRUE) && 
			((RequiredModule->EmitterLoops == 0) || (RequiredModule->EmitterLoops > 1)))
		{
			EmitterDurations(TempLOD->Level) -= CurrentDelay;
		}
	}

	// Set the current duration
	EmitterDuration	= EmitterDurations(CurrentLODLevelIndex);
}

/**
 *	Checks some common values for GetDynamicData validity
 *
 *	@return	UBOOL		TRUE if GetDynamicData should continue, FALSE if it should return NULL
 */
UBOOL FParticleEmitterInstance::IsDynamicDataRequired(UParticleLODLevel* CurrentLODLevel)
{
	if ((ActiveParticles <= 0) || 
		(SpriteTemplate && (SpriteTemplate->EmitterRenderMode == ERM_None)))
	{
		return FALSE;
	}

	if ((CurrentLODLevel == NULL) || (CurrentLODLevel->bEnabled == FALSE) || 
		((CurrentLODLevel->RequiredModule->bUseMaxDrawCount == TRUE) && (CurrentLODLevel->RequiredModule->MaxDrawCount == 0)))
	{
		return FALSE;
	}

	if (Component == NULL)
	{
		return FALSE;
	}
	return TRUE;
}

/**
 *	Process received events.
 */
void FParticleEmitterInstance::ProcessParticleEvents(FLOAT DeltaTime, UBOOL bSuppressSpawning)
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	check(LODLevel);
	if (LODLevel->EventReceiverModules.Num() > 0)
	{
		for (INT EventModIndex = 0; EventModIndex < LODLevel->EventReceiverModules.Num(); EventModIndex++)
		{
			INT EventIndex;
			UParticleModuleEventReceiverBase* EventRcvr = LODLevel->EventReceiverModules(EventModIndex);
			check(EventRcvr);

			if (EventRcvr->WillProcessParticleEvent(EPET_Spawn) && (Component->SpawnEvents.Num() > 0))
			{
				for (EventIndex = 0; EventIndex < Component->SpawnEvents.Num(); EventIndex++)
				{
					EventRcvr->ProcessParticleEvent(this, Component->SpawnEvents(EventIndex), DeltaTime);
				}
			}

			if (EventRcvr->WillProcessParticleEvent(EPET_Death) && (Component->DeathEvents.Num() > 0))
			{
				for (EventIndex = 0; EventIndex < Component->DeathEvents.Num(); EventIndex++)
				{
					EventRcvr->ProcessParticleEvent(this, Component->DeathEvents(EventIndex), DeltaTime);
				}
			}

			if (EventRcvr->WillProcessParticleEvent(EPET_Collision) && (Component->CollisionEvents.Num() > 0))
			{
				for (EventIndex = 0; EventIndex < Component->CollisionEvents.Num(); EventIndex++)
				{
					EventRcvr->ProcessParticleEvent(this, Component->CollisionEvents(EventIndex), DeltaTime);
				}
			}

			if (EventRcvr->WillProcessParticleEvent(EPET_Kismet) && (Component->KismetEvents.Num() > 0))
			{
				for (EventIndex = 0; EventIndex < Component->KismetEvents.Num(); EventIndex++)
				{
					EventRcvr->ProcessParticleEvent(this, Component->KismetEvents(EventIndex), DeltaTime);
				}
			}

			if (EventRcvr->WillProcessParticleEvent(EPET_WorldAttractorCollision) && (Component->AttractorCollisionEvents.Num() > 0))
			{
				for (EventIndex = 0; EventIndex < Component->AttractorCollisionEvents.Num(); EventIndex++)
				{
					EventRcvr->ProcessParticleEvent(this, Component->AttractorCollisionEvents(EventIndex), DeltaTime);
				}
			}
		}
	}
}


/**
 * Captures dynamic replay data for this particle system.
 *
 * @param	OutData		[Out] Data will be copied here
 *
 * @return Returns TRUE if successful
 */
UBOOL FParticleEmitterInstance::FillReplayData( FDynamicEmitterReplayDataBase& OutData )
{
	// NOTE: This the base class implementation that should ONLY be called by derived classes' FillReplayData()!

	// Make sure there is a template present
	if (!SpriteTemplate)
	{
		return FALSE;
	}

	// Allocate it for now, but we will want to change this to do some form
	// of caching
	if (ActiveParticles <= 0)
	{
		return FALSE;
	}
	// If the template is disabled, don't return data.
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	if ((LODLevel == NULL) || (LODLevel->bEnabled == FALSE))
	{
		return FALSE;
	}

	// Make sure we will not be allocating enough memory
	check(MaxActiveParticles >= ActiveParticles);

	// Must be filled in by implementation in derived class
	OutData.eEmitterType = DET_Unknown;

	OutData.ActiveParticleCount = ActiveParticles;
	OutData.ParticleStride = ParticleStride;
	OutData.SortMode = SortMode;

	// Take scale into account
	OutData.Scale = FVector(1.0f, 1.0f, 1.0f);
	if (Component)
	{
		OutData.Scale *= Component->Scale * Component->Scale3D;
		AActor* Actor = Component->GetOwner();
		if (Actor && !Component->AbsoluteScale)
		{
			OutData.Scale *= Actor->DrawScale * Actor->DrawScale3D;
		}
	}

	INT ParticleMemSize = MaxActiveParticles * ParticleStride;
	INT IndexMemSize = MaxActiveParticles * sizeof(WORD);
	{
		SCOPE_CYCLE_COUNTER(STAT_ParticleMemTime);
		// This is 'render thread' memory in that we pass it over to the render thread for consumption
		INC_DWORD_STAT_BY(STAT_RTParticleData, ParticleMemSize + IndexMemSize);
		SET_DWORD_STAT(STAT_RTParticleData_Largest, Max<DWORD>(ParticleMemSize + IndexMemSize, GET_DWORD_STAT(STAT_RTParticleData_Largest)));
	}

	// Allocate particle memory
	OutData.ParticleData.Empty(ParticleMemSize);
	OutData.ParticleData.Add(ParticleMemSize);
	appBigBlockMemcpy(OutData.ParticleData.GetData(), ParticleData, ParticleMemSize);
	// Allocate particle index memory
	OutData.ParticleIndices.Empty(MaxActiveParticles);
	OutData.ParticleIndices.Add(MaxActiveParticles);
	appBigBlockMemcpy(OutData.ParticleIndices.GetData(), ParticleIndices, IndexMemSize);

	// All particle emitter types derived from sprite emitters, so we can fill that data in here too!
	{
		FDynamicSpriteEmitterReplayDataBase* NewReplayData =
			static_cast< FDynamicSpriteEmitterReplayDataBase* >( &OutData );

		NewReplayData->MaterialInterface = NULL;	// Must be set by derived implementation

		NewReplayData->MaxDrawCount =
			(LODLevel->RequiredModule->bUseMaxDrawCount == TRUE) ? LODLevel->RequiredModule->MaxDrawCount : -1;
		NewReplayData->ScreenAlignment	= LODLevel->RequiredModule->ScreenAlignment;
		NewReplayData->bUseLocalSpace = LODLevel->RequiredModule->bUseLocalSpace;
		NewReplayData->bAllowImageFlipping = LODLevel->RequiredModule->bAllowImageFlipping;
		NewReplayData->bSquareImageFlipping = LODLevel->RequiredModule->bSquareImageFlipping;
		NewReplayData->EmitterRenderMode = SpriteTemplate->EmitterRenderMode;
		NewReplayData->DynamicParameterDataOffset = DynamicParameterDataOffset;
		NewReplayData->CameraPayloadOffset = CameraPayloadOffset;

        NewReplayData->bOverrideSystemMacroUV = LODLevel->RequiredModule->bOverrideSystemMacroUV;
        NewReplayData->MacroUVRadius = LODLevel->RequiredModule->MacroUVRadius;
        NewReplayData->MacroUVPosition = LODLevel->RequiredModule->MacroUVPosition;
        
		NewReplayData->bLockAxis = FALSE;
		if (Module_AxisLock && (Module_AxisLock->bEnabled == TRUE))
		{
			NewReplayData->LockAxisFlag = Module_AxisLock->LockAxisFlags;
			if (Module_AxisLock->LockAxisFlags != EPAL_NONE)
			{
				NewReplayData->bLockAxis = TRUE;
			}
		}

		// If there are orbit modules, add the orbit module data
		if (LODLevel->OrbitModules.Num() > 0)
		{
			UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels(0);
			UParticleModuleOrbit* LastOrbit = HighestLODLevel->OrbitModules(LODLevel->OrbitModules.Num() - 1);
			check(LastOrbit);

			UINT* LastOrbitOffset = ModuleOffsetMap.Find(LastOrbit);
			NewReplayData->OrbitModuleOffset = *LastOrbitOffset;
		}

		NewReplayData->EmitterNormalsMode = LODLevel->RequiredModule->EmitterNormalsMode;
		NewReplayData->NormalsSphereCenter = LODLevel->RequiredModule->NormalsSphereCenter;
		NewReplayData->NormalsCylinderDirection = LODLevel->RequiredModule->NormalsCylinderDirection;
	}

#if !(FINAL_RELEASE || SHIPPING_PC_GAME)
	if (GEngine->bCheckParticleRenderSize)
	{
		INT CheckParticleCount = ActiveParticles;
		if 	(LODLevel->RequiredModule->bUseMaxDrawCount == TRUE)
		{
			CheckParticleCount = Min<INT>(ActiveParticles, LODLevel->RequiredModule->MaxDrawCount);
		}

		INT CheckSize = (CheckParticleCount * 4 * sizeof(FParticleSpriteVertex));
		//@todo.SAS. This matches the 360 limiter... Make this an ini setting.
		if (CheckSize > GEngine->MaxParticleVertexMemory)
		{
			AWorldInfo* WorldInfo = GWorld ? GWorld->GetWorldInfo() : NULL;
			if (WorldInfo)
			{
				INT MyIndex = -1;
				for (INT CheckIdx = 0; CheckIdx < Component->EmitterInstances.Num(); CheckIdx++)
				{
					if (Component->EmitterInstances(CheckIdx) == this)
					{
						MyIndex = CheckIdx;
						break;
					}
				}

				FString ErrorMessage = 
					FString::Printf(TEXT("Emitter %2d has too many vertices: %4d verts for %6d kB: %s"),
						MyIndex, 
						ActiveParticles * 4, CheckSize / 1024, 
						Component ? 
							Component->Template ? 
								*(Component->Template->GetName()) :
								TEXT("No template") :
							TEXT("No component"));
				FColor ErrorColor(255,0,0);
				WorldInfo->AddOnScreenDebugMessage((QWORD)((PTRINT)this), 5.0f, ErrorColor,ErrorMessage);
				debugf(*ErrorMessage);
			}
		}
	}
#endif	//#if !FINAL_RELEASE


	return TRUE;
}



/*-----------------------------------------------------------------------------
	ParticleSpriteEmitterInstance
-----------------------------------------------------------------------------*/
/**
 *	ParticleSpriteEmitterInstance
 *	The structure for a standard sprite emitter instance.
 */
/** Constructor	*/
FParticleSpriteEmitterInstance::FParticleSpriteEmitterInstance() :
	FParticleEmitterInstance()
{
}

/** Destructor	*/
FParticleSpriteEmitterInstance::~FParticleSpriteEmitterInstance()
{
}



/**
 *	Retrieves the dynamic data for the emitter
 *	
 *	@param	bSelected					Whether the emitter is selected in the editor
 *
 *	@return	FDynamicEmitterDataBase*	The dynamic data, or NULL if it shouldn't be rendered
 */
FDynamicEmitterDataBase* FParticleSpriteEmitterInstance::GetDynamicData(UBOOL bSelected)
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	if (IsDynamicDataRequired(LODLevel) == FALSE)
	{
		return NULL;
	}

	// Allocate the dynamic data
	FDynamicSpriteEmitterData* NewEmitterData = ::new FDynamicSpriteEmitterData(LODLevel->RequiredModule);
	{
		SCOPE_CYCLE_COUNTER(STAT_ParticleMemTime);
		INC_DWORD_STAT(STAT_DynamicEmitterCount);
		INC_DWORD_STAT(STAT_DynamicSpriteCount);
		INC_DWORD_STAT_BY(STAT_DynamicEmitterMem, sizeof(FDynamicSpriteEmitterData));
	}

	// Now fill in the source data
	if( !FillReplayData( NewEmitterData->Source ) )
	{
		delete NewEmitterData;
		return NULL;
	}

	// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
	NewEmitterData->Init( bSelected );

	return NewEmitterData;
}

/**
 *	Updates the dynamic data for the instance
 *
 *	@param	DynamicData		The dynamic data to fill in
 *	@param	bSelected		TRUE if the particle system component is selected
 */
UBOOL FParticleSpriteEmitterInstance::UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected)
{
	checkf((DynamicData->GetSource().eEmitterType == DET_Sprite), TEXT("Sprite::UpdateDynamicData> Invalid DynamicData type!"));

	if (ActiveParticles <= 0)
	{
		return FALSE;
	}

	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	if ((LODLevel == NULL) || (LODLevel->bEnabled == FALSE))
	{
		return FALSE;
	}
	FDynamicSpriteEmitterData* SpriteDynamicData = (FDynamicSpriteEmitterData*)DynamicData;
	// Now fill in the source data
	if( !FillReplayData( SpriteDynamicData->Source ) )
	{
		return FALSE;
	}

	// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
	SpriteDynamicData->Init( bSelected );

	return TRUE;
}

/**
 *	Retrieves replay data for the emitter
 *
 *	@return	The replay data, or NULL on failure
 */
FDynamicEmitterReplayDataBase* FParticleSpriteEmitterInstance::GetReplayData()
{
	if (ActiveParticles <= 0)
	{
		return NULL;
	}

	FDynamicEmitterReplayDataBase* NewEmitterReplayData = ::new FDynamicSpriteEmitterReplayData();
	check( NewEmitterReplayData != NULL );

	if( !FillReplayData( *NewEmitterReplayData ) )
	{
		delete NewEmitterReplayData;
		return NULL;
	}

	return NewEmitterReplayData;
}

/**
 *	Retrieve the allocated size of this instance.
 *
 *	@param	OutNum			The size of this instance
 *	@param	OutMax			The maximum size of this instance
 */
void FParticleSpriteEmitterInstance::GetAllocatedSize(INT& OutNum, INT& OutMax)
{
	INT Size = sizeof(FParticleSpriteEmitterInstance);
	INT ActiveParticleDataSize = (ParticleData != NULL) ? (ActiveParticles * ParticleStride) : 0;
	INT MaxActiveParticleDataSize = (ParticleData != NULL) ? (MaxActiveParticles * ParticleStride) : 0;
	INT ActiveParticleIndexSize = (ParticleIndices != NULL) ? (ActiveParticles * sizeof(WORD)) : 0;
	INT MaxActiveParticleIndexSize = (ParticleIndices != NULL) ? (MaxActiveParticles * sizeof(WORD)) : 0;

	OutNum = ActiveParticleDataSize + ActiveParticleIndexSize + Size;
	OutMax = MaxActiveParticleDataSize + MaxActiveParticleIndexSize + Size;
}

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 *	@param	bInExclusiveResourceSizeMode	UObject::GExclusiveResourceSizeMode value
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT FParticleSpriteEmitterInstance::GetResourceSize(UBOOL bInExclusiveResourceSizeMode)
{
	INT ResSize = 0;
	if (!bInExclusiveResourceSizeMode ||
		(Component && Component->SceneInfo && Component->SceneInfo->Proxy))
	{
		INT MaxActiveParticleDataSize = (ParticleData != NULL) ? (MaxActiveParticles * ParticleStride) : 0;
		INT MaxActiveParticleIndexSize = (ParticleIndices != NULL) ? (MaxActiveParticles * sizeof(WORD)) : 0;
		// Take dynamic data into account as well
		ResSize = sizeof(FDynamicSpriteEmitterData);
		ResSize += MaxActiveParticleDataSize;								// Copy of the particle data on the render thread
		ResSize += MaxActiveParticleIndexSize;								// Copy of the particle indices on the render thread
		if (DynamicParameterDataOffset == 0)
		{
			ResSize += MaxActiveParticles * sizeof(FParticleSpriteVertex);		// The vertex data array
		}
		else
		{
			ResSize += MaxActiveParticles * sizeof(FParticleSpriteVertexDynamicParameter);
		}
	}
	return ResSize;
}

/**
 * Captures dynamic replay data for this particle system.
 *
 * @param	OutData		[Out] Data will be copied here
 *
 * @return Returns TRUE if successful
 */
UBOOL FParticleSpriteEmitterInstance::FillReplayData( FDynamicEmitterReplayDataBase& OutData )
{
	if (ActiveParticles <= 0)
	{
		return FALSE;
	}

	// Call parent implementation first to fill in common particle source data
	if( !FParticleEmitterInstance::FillReplayData( OutData ) )
	{
		return FALSE;
	}

	OutData.eEmitterType = DET_Sprite;

	FDynamicSpriteEmitterReplayData* NewReplayData =
		static_cast< FDynamicSpriteEmitterReplayData* >( &OutData );

	// Get the material instance. If there is none, or the material isn't flagged for use with particle systems, use the DefaultMaterial.
	UMaterialInterface* MaterialInst = CurrentMaterial;
	if ((MaterialInst == NULL) || (MaterialInst->CheckMaterialUsage(MATUSAGE_ParticleSprites) == FALSE))
	{
		MaterialInst = GEngine->DefaultMaterial;
	}
	NewReplayData->MaterialInterface = MaterialInst;

	return TRUE;
}


/*-----------------------------------------------------------------------------
	ParticleSpriteSubUVEmitterInstance
-----------------------------------------------------------------------------*/
/**
 *	ParticleSpriteSubUVEmitterInstance
 *	Structure for SubUV sprite instances
 */
/** Constructor	*/
FParticleSpriteSubUVEmitterInstance::FParticleSpriteSubUVEmitterInstance() :
	FParticleEmitterInstance()
{
}

/** Destructor	*/
FParticleSpriteSubUVEmitterInstance::~FParticleSpriteSubUVEmitterInstance()
{
}

/**
 *	Kill off any dead particles. (Remove them from the active array)
 */
void FParticleSpriteSubUVEmitterInstance::KillParticles()
{
	if (ActiveParticles > 0)
	{
		UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
		check(LODLevel);
		FParticleEventInstancePayload* EventPayload = NULL;
		if (LODLevel->EventGenerator)
		{
			EventPayload = (FParticleEventInstancePayload*)GetModuleInstanceData(LODLevel->EventGenerator);
			if (EventPayload && (EventPayload->bDeathEventsPresent == FALSE))
			{
				EventPayload = NULL;
			}
		}

		// Loop over the active particles... If their RelativeTime is > 1.0f (indicating they are dead),
		// move them to the 'end' of the active particle list.
		for (INT i=ActiveParticles-1; i>=0; i--)
		{
			const INT	CurrentIndex	= ParticleIndices[i];
			const BYTE* ParticleBase	= ParticleData + CurrentIndex * ParticleStride;
			FBaseParticle& Particle		= *((FBaseParticle*) ParticleBase);
			if (Particle.RelativeTime > 1.0f)
			{
				FLOAT* pkFloats = (FLOAT*)((BYTE*)&Particle + PayloadOffset);
				pkFloats[0] = 0.0f;
				pkFloats[1] = 0.0f;
				pkFloats[2] = 0.0f;
				pkFloats[3] = 0.0f;
				pkFloats[4] = 0.0f;

				if (EventPayload)
				{
					LODLevel->EventGenerator->HandleParticleKilled(this, EventPayload, &Particle);
				}

				ParticleIndices[i]	= ParticleIndices[ActiveParticles-1];
				ParticleIndices[ActiveParticles-1]	= CurrentIndex;
				ActiveParticles--;
			}
		}
	}
}



/**
 *	Retrieves the dynamic data for the emitter
 *	
 *	@param	bSelected					Whether the emitter is selected in the editor
 *
 *	@return	FDynamicEmitterDataBase*	The dynamic data, or NULL if it shouldn't be rendered
 */
FDynamicEmitterDataBase* FParticleSpriteSubUVEmitterInstance::GetDynamicData(UBOOL bSelected)
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	if (IsDynamicDataRequired(LODLevel) == FALSE)
	{
		return NULL;
	}

	// Allocate the dynamic data
	FDynamicSubUVEmitterData* NewEmitterData = ::new FDynamicSubUVEmitterData(LODLevel->RequiredModule);
	{
		SCOPE_CYCLE_COUNTER(STAT_ParticleMemTime);
		INC_DWORD_STAT(STAT_DynamicEmitterCount);
		INC_DWORD_STAT(STAT_DynamicSubUVCount);
		INC_DWORD_STAT_BY(STAT_DynamicEmitterMem, sizeof(FDynamicSubUVEmitterData));
	}

	// Now fill in the source data
	if( !FillReplayData( NewEmitterData->Source ) )
	{
		delete NewEmitterData;
		return NULL;
	}

	// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
	NewEmitterData->Init( bSelected );

	return NewEmitterData;
}

/**
 *	Updates the dynamic data for the instance
 *
 *	@param	DynamicData		The dynamic data to fill in
 *	@param	bSelected		TRUE if the particle system component is selected
 */
UBOOL FParticleSpriteSubUVEmitterInstance::UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected)
{
	checkf((DynamicData->GetSource().eEmitterType == DET_SubUV), TEXT("SubUV::UpdateDynamicData> Invalid DynamicData type!"));

	if (ActiveParticles <= 0)
	{
		return FALSE;
	}
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	if ((LODLevel == NULL) || (LODLevel->bEnabled == FALSE))
	{
		return FALSE;
	}
	FDynamicSubUVEmitterData* SubUVDynamicData = (FDynamicSubUVEmitterData*)DynamicData;
	// Now fill in the source data
	if( !FillReplayData( SubUVDynamicData->Source ) )
	{
		return FALSE;
	}

	// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
	SubUVDynamicData->Init( bSelected );

	return TRUE;
}

/**
 *	Retrieves replay data for the emitter
 *
 *	@return	The replay data, or NULL on failure
 */
FDynamicEmitterReplayDataBase* FParticleSpriteSubUVEmitterInstance::GetReplayData()
{
	if (ActiveParticles <= 0)
	{
		return NULL;
	}

	FDynamicEmitterReplayDataBase* NewEmitterReplayData = ::new FDynamicSubUVEmitterReplayData();
	check( NewEmitterReplayData != NULL );

	if( !FillReplayData( *NewEmitterReplayData ) )
	{
		delete NewEmitterReplayData;
		return NULL;
	}

	return NewEmitterReplayData;
}

/**
 *	Retrieve the allocated size of this instance.
 *
 *	@param	OutNum			The size of this instance
 *	@param	OutMax			The maximum size of this instance
 */
void FParticleSpriteSubUVEmitterInstance::GetAllocatedSize(INT& OutNum, INT& OutMax)
{
	INT Size = sizeof(FParticleSpriteSubUVEmitterInstance);
	INT ActiveParticleDataSize = (ParticleData != NULL) ? (ActiveParticles * ParticleStride) : 0;
	INT MaxActiveParticleDataSize = (ParticleData != NULL) ? (MaxActiveParticles * ParticleStride) : 0;
	INT ActiveParticleIndexSize = (ParticleIndices != NULL) ? (ActiveParticles * sizeof(WORD)) : 0;
	INT MaxActiveParticleIndexSize = (ParticleIndices != NULL) ? (MaxActiveParticles * sizeof(WORD)) : 0;

	OutNum = ActiveParticleDataSize + ActiveParticleIndexSize + Size;
	OutMax = MaxActiveParticleDataSize + MaxActiveParticleIndexSize + Size;
}

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 *	@param	bInExclusiveResourceSizeMode	UObject::GExclusiveResourceSizeMode value
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT FParticleSpriteSubUVEmitterInstance::GetResourceSize(UBOOL bInExclusiveResourceSizeMode)
{
	INT ResSize = 0;
	if (!bInExclusiveResourceSizeMode ||
		(Component && Component->SceneInfo && Component->SceneInfo->Proxy))
	{
		INT MaxActiveParticleDataSize = (ParticleData != NULL) ? (MaxActiveParticles * ParticleStride) : 0;
		INT MaxActiveParticleIndexSize = (ParticleIndices != NULL) ? (MaxActiveParticles * sizeof(WORD)) : 0;
		// Take dynamic data into account as well
		ResSize = sizeof(FDynamicSubUVEmitterData);
		ResSize += MaxActiveParticleDataSize;									// Copy of the particle data on the render thread
		ResSize += MaxActiveParticleIndexSize;									// Copy of the particle indices on the render thread
		if (DynamicParameterDataOffset == 0)
		{
			ResSize += MaxActiveParticles * sizeof(FParticleSpriteSubUVVertex);	// The vertex data array
		}
		else
		{
			ResSize += MaxActiveParticles * sizeof(FParticleSpriteSubUVVertexDynamicParameter);
		}
	}
	return ResSize;
}

/**
 * Captures dynamic replay data for this particle system.
 *
 * @param	OutData		[Out] Data will be copied here
 *
 * @return Returns TRUE if successful
 */
UBOOL FParticleSpriteSubUVEmitterInstance::FillReplayData( FDynamicEmitterReplayDataBase& OutData )
{
	// Call parent implementation first to fill in common particle source data
	if( !FParticleEmitterInstance::FillReplayData( OutData ) )
	{
		return FALSE;
	}

	// Grab the LOD level
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	if ((LODLevel == NULL) || (LODLevel->bEnabled == FALSE))
	{
		return FALSE;
	}

	OutData.eEmitterType = DET_SubUV;

	FDynamicSubUVEmitterReplayData* NewReplayData =
		static_cast< FDynamicSubUVEmitterReplayData* >( &OutData );

	// Get the material instance. If there is none, or the material isn't flagged for use with particle systems, use the DefaultMaterial.
	UMaterialInterface* MaterialInst = CurrentMaterial;
	if ((MaterialInst == NULL) || (MaterialInst->CheckMaterialUsage(MATUSAGE_ParticleSubUV) == FALSE))
	{
		MaterialInst = GEngine->DefaultMaterial;
	}

	NewReplayData->MaterialInterface = MaterialInst;


	NewReplayData->SubUVDataOffset = SubUVDataOffset;
	NewReplayData->SubImages_Horizontal = LODLevel->RequiredModule->SubImages_Horizontal;
	NewReplayData->SubImages_Vertical = LODLevel->RequiredModule->SubImages_Vertical;
	NewReplayData->bDirectUV = LODLevel->RequiredModule->bDirectUV;


	return TRUE;
}


/*-----------------------------------------------------------------------------
	ParticleMeshEmitterInstance
-----------------------------------------------------------------------------*/
/**
 *	Structure for mesh emitter instances
 */

/** Constructor	*/
FParticleMeshEmitterInstance::FParticleMeshEmitterInstance() :
	  FParticleEmitterInstance()
	, MeshTypeData(NULL)
	, MeshComponentIndex(-1)
	, MeshRotationActive(FALSE)
	, MeshRotationOffset(0)
	, bIgnoreComponentScale(FALSE)
{
}

/**
 *	Initialize the parameters for the structure
 *
 *	@param	InTemplate		The ParticleEmitter to base the instance on
 *	@param	InComponent		The owning ParticleComponent
 *	@param	bClearResources	If TRUE, clear all resource data
 */
void FParticleMeshEmitterInstance::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, UBOOL bClearResources)
{
	FParticleEmitterInstance::InitParameters(InTemplate, InComponent, bClearResources);

	// Get the type data module
	UParticleLODLevel* LODLevel	= InTemplate->GetLODLevel(0);
	check(LODLevel);
	MeshTypeData = CastChecked<UParticleModuleTypeDataMesh>(LODLevel->TypeDataModule);
	check(MeshTypeData);

    // Grab the MeshRotationRate module offset, if there is one...
    MeshRotationActive = FALSE;
	if (LODLevel->RequiredModule->ScreenAlignment == PSA_Velocity)
	{
		MeshRotationActive = TRUE;
	}
	else
	{
	    for (INT i = 0; i < LODLevel->Modules.Num(); i++)
	    {
			if (LODLevel->Modules(i)->TouchesMeshRotation() == TRUE)
	        {
	            MeshRotationActive = TRUE;
	            break;
	        }
		}
    }
}

/**
 *	Initialize the instance
 */
void FParticleMeshEmitterInstance::Init()
{
	FParticleEmitterInstance::Init();

	// If there is a mesh present (there should be!)
	if (MeshTypeData->Mesh && (MeshTypeData->Mesh->LODModels.Num() > 0))
	{
		const FStaticMeshRenderData& MeshLODModel = MeshTypeData->Mesh->LODModels(0);

		AEmitterPool* EmitterPool = NULL;
		// If the net index is NONE, it is *NOT* serialized from a package - ie it is dynamic
		if ((Component != NULL) && (Component->GetNetIndex() == INDEX_NONE))
		{
			if (GWorld && GWorld->GetWorldInfo())
			{
				EmitterPool = GWorld->GetWorldInfo()->MyEmitterPool;
			}
		}

		UStaticMeshComponent* MeshComponent = NULL;

		// If the index is set, try to retrieve it from the component
		if (MeshComponentIndex == -1)
		{
			// See if the SMC we are interested in is already present...
			for (INT SMIndex = 0; SMIndex < Component->SMComponents.Num(); SMIndex++)
			{
				UStaticMeshComponent* SMComp = Component->SMComponents(SMIndex);
				if (SMComp)
				{
					if (SMComp->StaticMesh == MeshTypeData->Mesh)
					{
						MeshComponentIndex = SMIndex;
						break;
					}
				}
			}
		}
		if (MeshComponentIndex != -1)
		{
			if (MeshComponentIndex < Component->SMComponents.Num())
			{
				MeshComponent = Component->SMComponents(MeshComponentIndex);
			}

			if (MeshComponent && (MeshComponent->StaticMesh != MeshTypeData->Mesh))
			{
				// Incorrect mesh!
				// We can't assume that another emitter in the PSystem isn't use it,
				// so simply regrab another one...
				// This should be safe as the system will release all of them.
				MeshComponent = NULL;
			}

			// If it wasn't retrieved, force it to get recreated
			if (MeshComponent == NULL)
			{
				MeshComponentIndex = -1;
			}
		}

		if (MeshComponentIndex == -1)
		{
			// try to find a free component instead of creating one
			if (EmitterPool)
			{
				MeshComponent = EmitterPool->GetFreeStaticMeshComponent();
			}

			// create the component if necessary
			if (MeshComponent == NULL)
			{
				// If the pool did not return one to us, create one w/ the PSysComponent as the outer.
				MeshComponent = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(), Component);
				MeshComponent->bAcceptsStaticDecals = FALSE;
				MeshComponent->bAcceptsDynamicDecals = FALSE;
				MeshComponent->CollideActors		= FALSE;
				MeshComponent->BlockActors			= FALSE;
				MeshComponent->BlockZeroExtent		= FALSE;
				MeshComponent->BlockNonZeroExtent	= FALSE;
				MeshComponent->BlockRigidBody		= FALSE;
			}

			// allocate space for material instance constants
			INT Diff = MeshComponent->Materials.Num() - MeshLODModel.Elements.Num();
			if (Diff > 0)
			{
				MeshComponent->Materials.Remove(MeshComponent->Materials.Num() - Diff - 1, Diff);
			}
			else if (Diff < 0)
			{
				MeshComponent->Materials.AddZeroed(-Diff);
			}

			check(MeshComponent->Materials.Num() == MeshLODModel.Elements.Num());

			MeshComponent->StaticMesh		= MeshTypeData->Mesh;
			MeshComponent->CastShadow		= MeshTypeData->CastShadows;
			MeshComponent->bAcceptsLights	= Component->bAcceptsLights;

			for (INT SlotIndex = 0; SlotIndex < Component->SMComponents.Num(); SlotIndex++)
			{
				if (Component->SMComponents(SlotIndex) == NULL)
				{
					MeshComponentIndex = SlotIndex;
					Component->SMComponents(SlotIndex) = MeshComponent;
				}
			}
			if (MeshComponentIndex == -1)
			{
				MeshComponentIndex = Component->SMComponents.AddItem(MeshComponent);
			}
		}
		check(MeshComponent);
		check(MeshComponent->Materials.Num() >= MeshLODModel.Elements.Num());

		// Constructing MaterialInstanceConstant for each mesh instance is done so that
		// particle 'vertex color' can be set on each individual mesh.
		// They are tagged as transient so they don't get saved in the package.
		for (INT MatIndex = 0; MatIndex < MeshComponent->Materials.Num(); MatIndex++)
		{
			const FStaticMeshElement* MeshElement = &(MeshLODModel.Elements(MatIndex));
			if (MeshElement == NULL)
			{
				// This should not happen...
				continue;
			}

			UMaterialInterface* Parent = NULL;

			// Determine what the material should be...

			// First check the emitter instance Materials array, which is set from the 
			// MeshMaterial module...
			if (CurrentMaterials.Num() > MatIndex)
			{
				Parent = CurrentMaterials(MatIndex);
			}

			if (Parent == NULL)
			{
				// Not set yet, so check for the bOverrideMaterial flag
				if (MeshTypeData->bOverrideMaterial == TRUE)
				{
					Parent = CurrentLODLevel->RequiredModule->Material;
				}
			}

			if (Parent == NULL)
			{
				// Not set yet, so use the StaticMesh material that is assigned.
				Parent = MeshElement->Material;
			}

			if (Parent == NULL)
			{
				// This is really bad... no material found.
				// But, we don't want to crash, so use the Default one.
				Parent = GEngine->DefaultMaterial;
			}

			check(Parent);

			UMaterialInstanceConstant* MatInst = NULL;
			if (MeshComponent->Materials.Num() > MatIndex)
			{
				MatInst = Cast<UMaterialInstanceConstant>(MeshComponent->Materials(MatIndex));
			}

			if (MatInst == NULL)
			{
				if (EmitterPool)
				{
					MatInst = EmitterPool->GetFreeMatInstConsts();
				}

				if (MatInst == NULL)
				{
					// create the instance constant if necessary
					MatInst = ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass(), MeshComponent);
				}

				if (MeshComponent->Materials.Num() > MatIndex)
				{
					MeshComponent->Materials(MatIndex) = MatInst;
				}
				else
				{
					INT CheckIndex = MeshComponent->Materials.AddItem(MatInst);
					check(CheckIndex == MatIndex);
				}
			}

			check(MatInst);

			MatInst->SetParent(Parent);

			if (GEmulateMobileRendering == TRUE)
			{
				//Make sure the mobile properties are set correctly when emulating 
				MatInst->SetupMobileProperties();
			}

			MatInst->SetFlags(RF_Transient);
		}
	}
}

/**
 *	Resize the particle data array
 *
 *	@param	NewMaxActiveParticles	The new size to use
 *
 *	@return	UBOOL					TRUE if the resize was successful
 */
UBOOL FParticleMeshEmitterInstance::Resize(INT NewMaxActiveParticles, UBOOL bSetMaxActiveCount)
{
	INT OldMaxActiveParticles = MaxActiveParticles;
	if (FParticleEmitterInstance::Resize(NewMaxActiveParticles, bSetMaxActiveCount) == TRUE)
	{
		if (MeshRotationActive)
		{
			for (INT i = OldMaxActiveParticles; i < NewMaxActiveParticles; i++)
			{
				DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
				FMeshRotationPayloadData* PayloadData	= (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshRotationOffset);
				PayloadData->RotationRateBase			= FVector(0.0f);
			}
		}

		return TRUE;
	}

	return FALSE;
}

/**
 *	Tick the instance.
 *
 *	@param	DeltaTime			The time slice to use
 *	@param	bSuppressSpawning	If TRUE, do not spawn during Tick
 */
void FParticleMeshEmitterInstance::Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning)
{
	SCOPE_CYCLE_COUNTER(STAT_MeshTickTime);

	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	// See if we are handling mesh rotation
    if (MeshRotationActive)
    {
		// Update the rotation for each particle
        for (INT i = 0; i < ActiveParticles; i++)
	    {
		    DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
            FMeshRotationPayloadData* PayloadData	= (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshRotationOffset);
			PayloadData->RotationRate				= PayloadData->RotationRateBase;
			if (LODLevel->RequiredModule->ScreenAlignment == PSA_Velocity)
			{
				// Determine the rotation to the velocity vector and apply it to the mesh
				FVector	NewDirection	= Particle.Velocity;
				
				//check if an orbit module should affect the velocity...		
				if (LODLevel->RequiredModule->bOrbitModuleAffectsVelocityAlignment &&
					LODLevel->OrbitModules.Num() > 0)
				{
					UParticleModuleOrbit* LastOrbit = SpriteTemplate->LODLevels(0)->OrbitModules(LODLevel->OrbitModules.Num() - 1);
					check(LastOrbit);
					
					UINT OrbitModuleOffset = *ModuleOffsetMap.Find(LastOrbit);
					if (OrbitModuleOffset != 0)
					{
						FOrbitChainModuleInstancePayload &OrbitPayload = *(FOrbitChainModuleInstancePayload*)((BYTE*)&Particle + OrbitModuleOffset);

						FVector OrbitOffset = OrbitPayload.Offset;
						FVector PrevOrbitOffset = OrbitPayload.PreviousOffset;
						FVector Location = Particle.Location;
						FVector OldLocation = Particle.OldLocation;

						//this should be our current position
						FVector NewPos = Location + OrbitOffset;	
						//this should be our previous position
						FVector OldPos = OldLocation + PrevOrbitOffset;

						NewDirection = NewPos - OldPos;
					}	
				}			              
               
				NewDirection.Normalize();
				FVector	OldDirection(1.0f, 0.0f, 0.0f);

				FQuat Rotation	= FQuatFindBetween(OldDirection, NewDirection);
				FVector Euler	= Rotation.Euler();
				PayloadData->Rotation.X	= Euler.X;
				PayloadData->Rotation.Y	= Euler.Y;
				PayloadData->Rotation.Z	= Euler.Z;
			}
	    }
    }

	// Call the standard tick
	FParticleEmitterInstance::Tick(DeltaTime, bSuppressSpawning);

	// Apply rotation if it is active
    if (MeshRotationActive)
    {
        for (INT i = 0; i < ActiveParticles; i++)
	    {
		    DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
			if ((Particle.Flags & STATE_Particle_FreezeRotation) == 0)
			{
	            FMeshRotationPayloadData* PayloadData	 = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshRotationOffset);
				PayloadData->Rotation					+= DeltaTime * PayloadData->RotationRate;
			}
        }
    }

	// With this here when you DeactivateSystem() all of the meshes go away when they should stick around
	// Do we need to tick the mesh instances or will the engine do it?
	if ((ActiveParticles == 0) && bSuppressSpawning)
	{
		RemovedFromScene();
	}

	// Remove from the Sprite count... happens because we use the Super::Tick
	DEC_DWORD_STAT_BY(STAT_SpriteParticles, ActiveParticles);
	INC_DWORD_STAT_BY(STAT_MeshParticles, ActiveParticles);
}

/**
 *	Update the bounding box for the emitter
 *
 *	@param	DeltaTime		The time slice to use
 */
void FParticleMeshEmitterInstance::UpdateBoundingBox(FLOAT DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleUpdateBounds);
	//@todo. Implement proper bound determination for mesh emitters.
	// Currently, just 'forcing' the mesh size to be taken into account.
	if ((Component != NULL) && (ActiveParticles > 0))
	{
		UBOOL bUpdateBox = ((Component->bWarmingUp == FALSE) && (Component->bSkipBoundsUpdate == FALSE) &&
			(Component->Template != NULL) && (Component->Template->bUseFixedRelativeBoundingBox == FALSE));

		// Take scale into account
		FVector Scale = FVector(1.0f, 1.0f, 1.0f);
		if (!bIgnoreComponentScale)
		{
			Scale *= Component->Scale * Component->Scale3D;
		}
		AActor* Actor = Component->GetOwner();
		if (Actor && !Component->AbsoluteScale)
		{
			Scale *= Actor->DrawScale * Actor->DrawScale3D;
		}

		// Get the static mesh bounds
		FBoxSphereBounds MeshBound;
		if (Component->bWarmingUp == FALSE)
		{	
			if (MeshTypeData->Mesh)
			{
				MeshBound = MeshTypeData->Mesh->Bounds;
			}
			else
			{
				debugfSlow(TEXT("MeshEmitter with no mesh set?? - %s"), Component->Template ? *(Component->Template->GetPathName()) : TEXT("??????"));
				MeshBound = FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0);
			}
		}
		else
		{
			// This isn't used anywhere if the bWarmingUp flag is false, but GCC doesn't like it not touched.
			appMemzero(&MeshBound, sizeof(FBoxSphereBounds));
		}

		FVector	NewLocation;
		FLOAT	NewRotation;
		if (bUpdateBox)
		{
			ParticleBoundingBox.Init();
		}
		// For each particle, offset the box appropriately 
		FVector MinVal((WORLD_MAX/2.0f));
		FVector MaxVal(-(WORLD_MAX/2.0f));
		FVector LocalExtent;
		CONSOLE_PREFETCH(ParticleData + ParticleStride * ParticleIndices[0]);
		CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[0] * ParticleStride));
		for (INT i=0; i<ActiveParticles; i++)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
			CONSOLE_PREFETCH(ParticleData + ParticleStride * ParticleIndices[i+1]);
			CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));

			// Do linear integrator and update bounding box
			Particle.OldLocation = Particle.Location;
			if ((Particle.Flags & STATE_Particle_Freeze) == 0)
			{
				if ((Particle.Flags & STATE_Particle_FreezeTranslation) == 0)
				{
					NewLocation	= Particle.Location + DeltaTime * Particle.Velocity;
				}
				else
				{
					NewLocation = Particle.Location;
				}
				if ((Particle.Flags & STATE_Particle_FreezeRotation) == 0)
				{
					NewRotation	= Particle.Rotation + DeltaTime * Particle.RotationRate;
				}
				else
				{
					NewRotation = Particle.Rotation;
				}
			}
			else
			{
				// Don't move it...
				NewLocation = Particle.Location;
				NewRotation = Particle.Rotation;
			}

			LocalExtent = MeshBound.GetBox().GetExtent() * Particle.Size * Scale;

			// Do angular integrator, and wrap result to within +/- 2 PI
			Particle.Rotation = appFmod(NewRotation, 2.f*(FLOAT)PI);
			Particle.Location = NewLocation;
			if (bUpdateBox)
			{	
				MinVal[0] = Min<FLOAT>(MinVal[0], NewLocation.X - LocalExtent.X);
				MaxVal[0] = Max<FLOAT>(MaxVal[0], NewLocation.X + LocalExtent.X);
				MinVal[1] = Min<FLOAT>(MinVal[1], NewLocation.Y - LocalExtent.Y);
				MaxVal[1] = Max<FLOAT>(MaxVal[1], NewLocation.Y + LocalExtent.Y);
				MinVal[2] = Min<FLOAT>(MinVal[2], NewLocation.Z - LocalExtent.Z);
				MaxVal[2] = Max<FLOAT>(MaxVal[2], NewLocation.Z + LocalExtent.Z);
			}
		}

		if (bUpdateBox)
		{	
			ParticleBoundingBox = FBox(MinVal, MaxVal);

			// Transform bounding box into world space if the emitter uses a local space coordinate system.
			UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
			check(LODLevel);
			if (LODLevel->RequiredModule->bUseLocalSpace) 
			{
				ParticleBoundingBox = ParticleBoundingBox.TransformBy(Component->LocalToWorld);
			}
		}
	}
}

/**
 *	Retrieved the per-particle bytes that this emitter type requires.
 *
 *	@return	UINT	The number of required bytes for particles in the instance
 */
UINT FParticleMeshEmitterInstance::RequiredBytes()
{
	UINT uiBytes = FParticleEmitterInstance::RequiredBytes();
	MeshRotationOffset	= PayloadOffset + uiBytes;
	uiBytes += sizeof(FMeshRotationPayloadData);
	return uiBytes;
}

/**
 *	Handle any post-spawning actions required by the instance
 *
 *	@param	Particle					The particle that was spawned
 *	@param	InterpolationPercentage		The percentage of the time slice it was spawned at
 *	@param	SpawnTIme					The time it was spawned at
 */
void FParticleMeshEmitterInstance::PostSpawn(FBaseParticle* Particle, FLOAT InterpolationPercentage, FLOAT SpawnTime)
{
	FParticleEmitterInstance::PostSpawn(Particle, InterpolationPercentage, SpawnTime);
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	if (LODLevel->RequiredModule->ScreenAlignment == PSA_Velocity)
	{
		// Determine the rotation to the velocity vector and apply it to the mesh
		FVector	NewDirection	= Particle->Velocity;
		NewDirection.Normalize();
		FVector	OldDirection(1.0f, 0.0f, 0.0f);

		FQuat Rotation	= FQuatFindBetween(OldDirection, NewDirection);
		FVector Euler	= Rotation.Euler();

		FMeshRotationPayloadData* PayloadData	= (FMeshRotationPayloadData*)((BYTE*)Particle + MeshRotationOffset);
		PayloadData->Rotation.X	+= Euler.X;
		PayloadData->Rotation.Y	+= Euler.Y;
		PayloadData->Rotation.Z	+= Euler.Z;
		//
	}
}



/**
 *	Retrieves the dynamic data for the emitter
 *	
 *	@param	bSelected					Whether the emitter is selected in the editor
 *
 *	@return	FDynamicEmitterDataBase*	The dynamic data, or NULL if it shouldn't be rendered
 */
FDynamicEmitterDataBase* FParticleMeshEmitterInstance::GetDynamicData(UBOOL bSelected)
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	if (IsDynamicDataRequired(LODLevel) == FALSE)
	{
		return NULL;
	}

	if ((MeshComponentIndex == -1) || (MeshComponentIndex >= Component->SMComponents.Num()))
	{
		// Not initialized?
		return NULL;
	}

	UStaticMeshComponent* MeshComponent = Component->SMComponents(MeshComponentIndex);
	if (MeshComponent == NULL)
	{
		// The mesh component has been GC'd?
		return NULL;
	}

	// Allocate the dynamic data
	FDynamicMeshEmitterData* NewEmitterData = ::new FDynamicMeshEmitterData(LODLevel->RequiredModule);
	{
		SCOPE_CYCLE_COUNTER(STAT_ParticleMemTime);
		INC_DWORD_STAT(STAT_DynamicEmitterCount);
		INC_DWORD_STAT(STAT_DynamicMeshCount);
		INC_DWORD_STAT_BY(STAT_DynamicEmitterMem, sizeof(FDynamicMeshEmitterData));
	}

	// Now fill in the source data
	if( !FillReplayData( NewEmitterData->Source ) )
	{
		delete NewEmitterData;
		return NULL;
	}


	// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
	NewEmitterData->Init(
		bSelected,
		this,
		MeshTypeData->Mesh,
		MeshComponent,
		FALSE );		// Use Nx fluid?

	return NewEmitterData;
}

/**
 *	Updates the dynamic data for the instance
 *
 *	@param	DynamicData		The dynamic data to fill in
 *	@param	bSelected		TRUE if the particle system component is selected
 */
UBOOL FParticleMeshEmitterInstance::UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected)
{
	if (ActiveParticles <= 0)
	{
		return FALSE;
	}

	if ((MeshComponentIndex == -1) || (MeshComponentIndex >= Component->SMComponents.Num()))
	{
		// Not initialized?
		return FALSE;
	}

	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	if ((LODLevel == NULL) || (LODLevel->bEnabled == FALSE))
	{
		return FALSE;
	}
	UStaticMeshComponent* MeshComponent = Component->SMComponents(MeshComponentIndex);
	if (MeshComponent == NULL)
	{
		// The mesh component has been GC'd?
		return FALSE;
	}

	checkf((DynamicData->GetSource().eEmitterType == DET_Mesh), TEXT("Mesh::UpdateDynamicData> Invalid DynamicData type!"));

	FDynamicMeshEmitterData* MeshDynamicData = (FDynamicMeshEmitterData*)DynamicData;
	// Now fill in the source data
	if( !FillReplayData( MeshDynamicData->Source ) )
	{
		return FALSE;
	}

	// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
	MeshDynamicData->Init(
		bSelected,
		this,
		MeshTypeData->Mesh,
		MeshComponent,
		FALSE );		// Use Nx fluid?

	return TRUE;
}

/**
 *	Retrieves replay data for the emitter
 *
 *	@return	The replay data, or NULL on failure
 */
FDynamicEmitterReplayDataBase* FParticleMeshEmitterInstance::GetReplayData()
{
	if (ActiveParticles <= 0)
	{
		return NULL;
	}

	FDynamicEmitterReplayDataBase* NewEmitterReplayData = ::new FDynamicMeshEmitterReplayData();
	check( NewEmitterReplayData != NULL );

	if( !FillReplayData( *NewEmitterReplayData ) )
	{
		delete NewEmitterReplayData;
		return NULL;
	}

	return NewEmitterReplayData;
}

/**
 *	Retrieve the allocated size of this instance.
 *
 *	@param	OutNum			The size of this instance
 *	@param	OutMax			The maximum size of this instance
 */
void FParticleMeshEmitterInstance::GetAllocatedSize(INT& OutNum, INT& OutMax)
{
	INT Size = sizeof(FParticleMeshEmitterInstance);
	INT ActiveParticleDataSize = (ParticleData != NULL) ? (ActiveParticles * ParticleStride) : 0;
	INT MaxActiveParticleDataSize = (ParticleData != NULL) ? (MaxActiveParticles * ParticleStride) : 0;
	INT ActiveParticleIndexSize = (ParticleIndices != NULL) ? (ActiveParticles * sizeof(WORD)) : 0;
	INT MaxActiveParticleIndexSize = (ParticleIndices != NULL) ? (MaxActiveParticles * sizeof(WORD)) : 0;

	OutNum = ActiveParticleDataSize + ActiveParticleIndexSize + Size;
	OutMax = MaxActiveParticleDataSize + MaxActiveParticleIndexSize + Size;
}

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 *	@param	bInExclusiveResourceSizeMode	UObject::GExclusiveResourceSizeMode value
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT FParticleMeshEmitterInstance::GetResourceSize(UBOOL bInExclusiveResourceSizeMode)
{
	INT ResSize = 0;
	if (!bInExclusiveResourceSizeMode ||
		(Component && Component->SceneInfo && Component->SceneInfo->Proxy))
	{
		INT MaxActiveParticleDataSize = (ParticleData != NULL) ? (MaxActiveParticles * ParticleStride) : 0;
		INT MaxActiveParticleIndexSize = (ParticleIndices != NULL) ? (MaxActiveParticles * sizeof(WORD)) : 0;
		// Take dynamic data into account as well
		ResSize = sizeof(FDynamicMeshEmitterData);
		ResSize += MaxActiveParticleDataSize;								// Copy of the particle data on the render thread
		ResSize += MaxActiveParticleIndexSize;								// Copy of the particle indices on the render thread
	}
	return ResSize;
}

/**
 * Captures dynamic replay data for this particle system.
 *
 * @param	OutData		[Out] Data will be copied here
 *
 * @return Returns TRUE if successful
 */
UBOOL FParticleMeshEmitterInstance::FillReplayData( FDynamicEmitterReplayDataBase& OutData )
{
	// Call parent implementation first to fill in common particle source data
	if( !FParticleEmitterInstance::FillReplayData( OutData ) )
	{
		return FALSE;
	}

	// Grab the LOD level
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	if ((LODLevel == NULL) || (LODLevel->bEnabled == FALSE))
	{
		return FALSE;
	}

	CurrentMaterial = LODLevel->RequiredModule->Material;

	OutData.eEmitterType = DET_Mesh;

	FDynamicMeshEmitterReplayData* NewReplayData =
		static_cast< FDynamicMeshEmitterReplayData* >( &OutData );

	// Don't need this for meshes
	NewReplayData->MaterialInterface = NULL;

	// Mesh settings
	NewReplayData->bScaleUV = LODLevel->RequiredModule->bScaleUV;
	NewReplayData->SubUVInterpMethod = LODLevel->RequiredModule->InterpolationMethod;
	NewReplayData->SubUVDataOffset = SubUVDataOffset;
	NewReplayData->SubImages_Horizontal = LODLevel->RequiredModule->SubImages_Horizontal;
	NewReplayData->SubImages_Vertical = LODLevel->RequiredModule->SubImages_Vertical;
	NewReplayData->MeshRotationOffset = MeshRotationOffset;
	NewReplayData->bMeshRotationActive = MeshRotationActive;
	NewReplayData->MeshAlignment = MeshTypeData->MeshAlignment;

	// Scale needs to be handled in a special way for meshes.  The parent implementation set this
	// itself, but we'll recompute it here.
	NewReplayData->Scale = FVector(1.0f, 1.0f, 1.0f);
	if (Component)
	{
		check(SpriteTemplate);
		UParticleLODLevel* LODLevel2 = SpriteTemplate->GetCurrentLODLevel(this);
		check(LODLevel2);
		check(LODLevel2->RequiredModule);
		// Take scale into account
		if (LODLevel2->RequiredModule->bUseLocalSpace == FALSE)
		{
			if (!bIgnoreComponentScale)
			{
				NewReplayData->Scale *= Component->Scale * Component->Scale3D;
			}
			AActor* Actor = Component->GetOwner();
			if (Actor && !Component->AbsoluteScale)
			{
				NewReplayData->Scale *= Actor->DrawScale * Actor->DrawScale3D;
			}
		}
	}

	// See if the new mesh locked axis is being used...
	if (MeshTypeData->AxisLockOption == EPAL_NONE)
	{
		if (Module_AxisLock && (Module_AxisLock->bEnabled == TRUE))
		{
			NewReplayData->LockAxisFlag = Module_AxisLock->LockAxisFlags;
			if (Module_AxisLock->LockAxisFlags != EPAL_NONE)
			{
				NewReplayData->bLockAxis = TRUE;
				switch (Module_AxisLock->LockAxisFlags)
				{
				case EPAL_X:
					NewReplayData->LockedAxis = FVector(1,0,0);
					break;
				case EPAL_Y:
					NewReplayData->LockedAxis = FVector(0,1,0);
					break;
				case EPAL_NEGATIVE_X:
					NewReplayData->LockedAxis = FVector(-1,0,0);
					break;
				case EPAL_NEGATIVE_Y:
					NewReplayData->LockedAxis = FVector(0,-1,0);
					break;
				case EPAL_NEGATIVE_Z:
					NewReplayData->LockedAxis = FVector(0,0,-1);
					break;
				case EPAL_Z:
				case EPAL_NONE:
				default:
					NewReplayData->LockedAxis = FVector(0,0,1);
					break;
				}
			}
		}
	}

	return TRUE;
}

FDynamicEmitterDataBase::FDynamicEmitterDataBase(const UParticleModuleRequired* RequiredModule)
	: bSelected(FALSE)
	, DownsampleThresholdScreenFraction(RequiredModule->DownsampleThresholdScreenFraction)
	, SceneProxy(NULL)
{
}

UBOOL FDynamicEmitterDataBase::ShouldRenderDownsampled(const FSceneView* View, const FBoxSphereBounds& Bounds) const
{
	const FLOAT DistanceSquared = (Bounds.Origin - View->ViewOrigin).SizeSquared();
	// A DownsampleThresholdScreenFraction of 0 is overloaded to mean never use downsampled translucency
	// Otherwise, use downsampled translucency on this mesh element if the bounding sphere is larger than the specified fraction of the screen
	return DownsampleThresholdScreenFraction > 0 ? 
		Square(Bounds.SphereRadius) > DownsampleThresholdScreenFraction * DistanceSquared * Square(View->LODDistanceFactor) :
		FALSE;
}

/** FDynamicSpriteEmitterReplayDataBase Serialization */
void FDynamicSpriteEmitterReplayDataBase::Serialize( FArchive& Ar )
{
	// Call parent implementation
	FDynamicEmitterReplayDataBase::Serialize( Ar );

	Ar << ScreenAlignment;
	Ar << bUseLocalSpace;
//	if (Ar.LicenseeVer() >= VER_PARTICLE_IMAGEFLIPPING)
	if (Ar.Ver() >= VER_MOBILE_MATERIAL_PARAMETER_RENAME)
	{
		Ar << bAllowImageFlipping;
	}
	if (Ar.Ver() >= VER_PARTICLE_SQUARE_IMAGE_FLIPPING)
	{
		Ar << bSquareImageFlipping;
	}
	Ar << bLockAxis;
	Ar << LockAxisFlag;
	Ar << MaxDrawCount;
	Ar << EmitterRenderMode;
	Ar << OrbitModuleOffset;
	Ar << DynamicParameterDataOffset;
	if (Ar.Ver() >= VER_PARTICLE_ADDED_CAMERA_OFFSET)
	{
		Ar << CameraPayloadOffset;
	}

	if (Ar.Ver() >= VER_ANALYTICAL_PARTICLE_NORMALS)
	{
		Ar << EmitterNormalsMode;
		Ar << NormalsSphereCenter;
		Ar << NormalsCylinderDirection;
	}
	else
	{
		EmitterNormalsMode = 0;
	}

	Ar << MaterialInterface;
}
