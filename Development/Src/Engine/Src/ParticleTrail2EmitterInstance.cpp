/*=============================================================================
	ParticleTrail2EmitterInstance.cpp: 
	Particle trail2 emitter instance implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EngineMaterialClasses.h"
#include "EngineAnimClasses.h"
#include "PrimitiveSceneInfo.h"

/** trail stats */
DECLARE_STATS_GROUP(TEXT("TrailParticles"),STATGROUP_TrailParticles);

DECLARE_DWORD_COUNTER_STAT(TEXT("Trail Particles"),STAT_TrailParticles,STATGROUP_Particles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Trail Ptcl Render Calls"),STAT_TrailParticlesRenderCalls,STATGROUP_TrailParticles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Trail Ptcls Spawned"),STAT_TrailParticlesSpawned,STATGROUP_Particles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Trail Ptcls Updated"),STAT_TrailParticlesUpdated,STATGROUP_TrailParticles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Trail Tick Calls"),STAT_TrailParticlesTickCalls,STATGROUP_TrailParticles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Trail Ptcls Killed"),STAT_TrailParticlesKilled,STATGROUP_Particles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Trail Ptcl Tris"),STAT_TrailParticlesTrianglesRendered,STATGROUP_Particles);

DECLARE_CYCLE_STAT(TEXT("Trail Spawn Time"),STAT_TrailSpawnTime,STATGROUP_TrailParticles);
DECLARE_CYCLE_STAT(TEXT("Trail FillVertex Time"),STAT_TrailFillVertexTime,STATGROUP_TrailParticles);
DECLARE_CYCLE_STAT(TEXT("Trail FillIndex Time"),STAT_TrailFillIndexTime,STATGROUP_TrailParticles);
DECLARE_CYCLE_STAT(TEXT("Trail Render Time"),STAT_TrailRenderingTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Trail Tick Time"),STAT_TrailTickTime,STATGROUP_Particles);

DECLARE_CYCLE_STAT(TEXT("AnimTrail Notify Time"),STAT_AnimTrailNotifyTime,STATGROUP_Particles);

#define MAX_TRAIL_INDICES	65535
/*-----------------------------------------------------------------------------
	ParticleTrail2EmitterInstance.
-----------------------------------------------------------------------------*/
IMPLEMENT_PARTICLEEMITTERINSTANCE_TYPE(FParticleTrail2EmitterInstance);

/**
 *	Structure for trail emitter instances
 */

/** Constructor	*/
FParticleTrail2EmitterInstance::FParticleTrail2EmitterInstance() :
	FParticleEmitterInstance()
	, TrailTypeData(NULL)
	, TrailModule_Source(NULL)
	, TrailModule_Source_Offset(0)
	, TrailModule_Spawn(NULL)
	, TrailModule_Spawn_Offset(0)
	, TrailModule_Taper(NULL)
	, TrailModule_Taper_Offset(0)
	, FirstEmission(0)
	, bClearTangents(TRUE)
	, LastEmittedParticleIndex(-1)
	, LastSelectedParticleIndex(-1)
	, TickCount(0)
	, ForceSpawnCount(0)
	, VertexCount(0)
	, TriangleCount(0)
	, Tessellation(0)
	, TrailCount(0)
	, MaxTrailCount(0)
	, SourceActor(NULL)
	, SourceEmitter(NULL)
	, ActuallySpawned(0)
{
	TextureTiles.Empty();
	TrailSpawnTimes.Empty();
	SourcePosition.Empty();
	LastSourcePosition.Empty();
	CurrentSourcePosition.Empty();
	LastSpawnPosition.Empty();
	LastSpawnTangent.Empty();
	SourceDistanceTravelled.Empty();
	SourceOffsets.Empty();
}

/** Destructor	*/
FParticleTrail2EmitterInstance::~FParticleTrail2EmitterInstance()
{
	TextureTiles.Empty();
	TrailSpawnTimes.Empty();
	SourcePosition.Empty();
	LastSourcePosition.Empty();
	CurrentSourcePosition.Empty();
	LastSpawnPosition.Empty();
	LastSpawnTangent.Empty();
	SourceDistanceTravelled.Empty();
	SourceOffsets.Empty();
}

/**
 *	Initialize the parameters for the structure
 *
 *	@param	InTemplate		The ParticleEmitter to base the instance on
 *	@param	InComponent		The owning ParticleComponent
 *	@param	bClearResources	If TRUE, clear all resource data
 */
void FParticleTrail2EmitterInstance::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, UBOOL bClearResources)
{
	FParticleEmitterInstance::InitParameters(InTemplate, InComponent, bClearResources);

	// We don't support LOD on trails
	UParticleLODLevel* LODLevel	= InTemplate->GetLODLevel(0);
	check(LODLevel);
	TrailTypeData	= CastChecked<UParticleModuleTypeDataTrail2>(LODLevel->TypeDataModule);
	check(TrailTypeData);

	TrailModule_Source			= NULL;
	TrailModule_Source_Offset	= 0;
	TrailModule_Spawn			= NULL;
	TrailModule_Spawn_Offset	= 0;
	TrailModule_Taper			= NULL;
	TrailModule_Taper_Offset	= 0;

	// Always have at least one trail
	if (TrailTypeData->MaxTrailCount <= 0)
	{
		TrailTypeData->MaxTrailCount	= 1;
	}

	//@todo. Remove this statement once multiple trails per emitter is implemented. 
	TrailTypeData->MaxTrailCount	= 1;

	// Always have at least one particle per trail
	if (TrailTypeData->MaxParticleInTrailCount == 0)
	{
		// Doesn't make sense to have 0 for this...
		warnf(TEXT("TrailEmitter %s --> MaxParticleInTrailCount == 0!"), *(InTemplate->GetPathName()));
		TrailTypeData->MaxParticleInTrailCount	= 1;
	}

	MaxTrailCount = TrailTypeData->MaxTrailCount;
	TrailSpawnTimes.Empty(MaxTrailCount);
	TrailSpawnTimes.AddZeroed(MaxTrailCount);
	SourceDistanceTravelled.Empty(MaxTrailCount);
	SourceDistanceTravelled.AddZeroed(MaxTrailCount);
	SourcePosition.Empty(MaxTrailCount);
	SourcePosition.AddZeroed(MaxTrailCount);
	LastSourcePosition.Empty(MaxTrailCount);
	LastSourcePosition.AddZeroed(MaxTrailCount);
	CurrentSourcePosition.Empty(MaxTrailCount);
	CurrentSourcePosition.AddZeroed(MaxTrailCount);
	LastSpawnPosition.Empty(MaxTrailCount);
	LastSpawnPosition.AddZeroed(MaxTrailCount);
	LastSpawnTangent.Empty(MaxTrailCount);
	LastSpawnTangent.AddZeroed(MaxTrailCount);
	SourceDistanceTravelled.Empty(MaxTrailCount);
	SourceDistanceTravelled.AddZeroed(MaxTrailCount);
	FirstEmission				= TRUE;
	bClearTangents				= TrailTypeData->bClearTangents;
	LastEmittedParticleIndex	= -1;
	LastSelectedParticleIndex	= -1;
	TickCount					= 0;
	ForceSpawnCount				= 0;

	VertexCount					= 0;
	TriangleCount				= 0;

	TextureTiles.Empty();
	TextureTiles.AddItem(TrailTypeData->TextureTile);

	// Resolve any actors...
	ResolveSource();
}

/**
 *	Initialize the instance
 */
void FParticleTrail2EmitterInstance::Init()
{
	FParticleEmitterInstance::Init();
	// Setup the modules prior to initializing...
	SetupTrailModules();
}

/**
 *	Tick the instance.
 *
 *	@param	DeltaTime			The time slice to use
 *	@param	bSuppressSpawning	If TRUE, do not spawn during Tick
 */
void FParticleTrail2EmitterInstance::Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning)
{
	SCOPE_CYCLE_COUNTER(STAT_TrailTickTime);
	if (Component)
	{
		// Only support the high LOD
		UParticleLODLevel* LODLevel	= SpriteTemplate->GetLODLevel(0);
		check(LODLevel);

		// Handle EmitterTime setup, looping, etc.
		FLOAT EmitterDelay = Tick_EmitterTimeSetup(DeltaTime, LODLevel);

		// Update the source data (position, etc.)
		UpdateSourceData(DeltaTime);

		// Kill before the spawn... Otherwise, we can get 'flashing'
		KillParticles();

		// We need to update the source travelled distance
		for (INT i=0; i<ActiveParticles; i++)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);

			INT						CurrentOffset		= TypeDataOffset;
			FLOAT*					TaperValues			= NULL;

			FTrail2TypeDataPayload* TrailData = ((FTrail2TypeDataPayload*)((BYTE*)&Particle + CurrentOffset));
			if (TRAIL_EMITTER_IS_START(TrailData->Flags))
			{
				UBOOL	bGotSource	= FALSE;

				FVector LastPosition = SourcePosition(TrailData->TrailIndex);
				FVector Position;

				if (TrailModule_Source)
				{
					Position = CurrentSourcePosition(TrailData->TrailIndex);
					bGotSource	= TRUE;
				}

				if (!bGotSource)
				{
					// Assume it should be taken from the emitter...
					Position	= Component->LocalToWorld.GetOrigin();
				}

				FVector Travelled	= Position - LastPosition;
				FLOAT	Distance	= Travelled.Size();

				SourceDistanceTravelled(TrailData->TrailIndex) += Distance;
				if (Distance > KINDA_SMALL_NUMBER)
				{
					SourcePosition(TrailData->TrailIndex) = Position;
				}
			}
			else
			{
				// Nothing...
			}
		}

		// If not suppressing spawning...
		if (!bHaltSpawning && !bSuppressSpawning)
		{
			if ((LODLevel->RequiredModule->EmitterLoops == 0) || 
				(LoopCount < LODLevel->RequiredModule->EmitterLoops) ||
				(SecondsSinceCreation < (EmitterDuration * LODLevel->RequiredModule->EmitterLoops)))
			{
				// For Trails, we probably want to ignore the SpawnRate distribution,
				// and focus strictly on the BurstList...
				FLOAT SpawnRate = 0.0f;
				// Figure out spawn rate for this tick.
				SpawnRate = LODLevel->SpawnModule->Rate.GetValue(EmitterTime, Component);

				// Take Bursts into account as well...
				INT		Burst		= 0;
				FLOAT	BurstTime	= GetCurrentBurstRateOffset(DeltaTime, Burst);
				SpawnRate += BurstTime;

				// Spawn new particles...

				//@todo. Fix the issue of 'blanking' Trails when the count drops...
				// This is a temporary hack!
				if ((ActiveParticles < MaxTrailCount) && (SpawnRate <= KINDA_SMALL_NUMBER))
				{
					// Force the spawn of a single Trail...
					SpawnRate = 1.0f / DeltaTime;
				}

				if (TrailModule_Spawn && TrailModule_Spawn->bEnabled)
				{
					INT	SpawnModCount = TrailModule_Spawn->GetSpawnCount(this, DeltaTime);
					INT	MaxParticlesAllowed	= MaxTrailCount * TrailTypeData->MaxParticleInTrailCount;
					if ((SpawnModCount + ActiveParticles) > MaxParticlesAllowed)
					{
						SpawnModCount	= MaxParticlesAllowed - ActiveParticles - 1;
						if (SpawnModCount < 0)
						{
							SpawnModCount = 0;
						}
					}

					// check to see if we are out of particles and then stop spawning
					if (ActiveParticles >= (TrailTypeData->MaxParticleInTrailCount * MaxTrailCount))
					{
						SpawnModCount = 0;
					}

					if (SpawnModCount)
					{
						//debugf(TEXT("SpawnModCount = %d"), SpawnModCount);
						// Set the burst for this, if there are any...
						SpawnFraction	= 0.0f;
						Burst			= SpawnModCount;
						SpawnRate		= Burst / DeltaTime;
					}
				}
				else
				{
					if ((ActiveParticles > 0) && (SourceDistanceTravelled(0) == 0.0f))
					{
						SpawnRate = 0.0f;
						//debugf(TEXT("Killing SpawnRate (no distance travelled)"));
					}
				}

				if (SpawnRate > 0.f)
				{
					SpawnFraction = Spawn(SpawnFraction, SpawnRate, DeltaTime, Burst, BurstTime);
				}
			}
		}

		// Reset velocity and size.
		ResetParticleParameters(DeltaTime, STAT_TrailParticlesUpdated);

		UParticleModuleTypeDataBase* pkBase = 0;
		if (LODLevel->TypeDataModule)
		{
			pkBase = Cast<UParticleModuleTypeDataBase>(LODLevel->TypeDataModule);
			//@todo. Need to track TypeData offset into payload!
			pkBase->PreUpdate(this, TypeDataOffset, DeltaTime);
		}

		// Store off the module offsets to avoid doing the 'find' for each particle
		TArray<UINT> UpdateModuleOffsets;
		UpdateModuleOffsets.Empty(LODLevel->UpdateModules.Num());
		UpdateModuleOffsets.AddZeroed(LODLevel->UpdateModules.Num());
		for (INT ModIdx = 0; ModIdx < LODLevel->UpdateModules.Num(); ModIdx++)
		{
			UParticleModule* ParticleModule	= LODLevel->UpdateModules(ModIdx);
			if (ParticleModule && ParticleModule->bEnabled)
			{
				UINT* Offset = ModuleOffsetMap.Find(ParticleModule);
				if (Offset != NULL)
				{
					UpdateModuleOffsets(ModIdx) = *Offset;
				}
			}
		}
		const UINT* UpdateModuleOffsetData = UpdateModuleOffsets.GetData();

		// Update existing particles (might respawn dying ones).
		for (INT i=0; i<LODLevel->UpdateModules.Num(); i++)
		{
			UParticleModule* ParticleModule	= LODLevel->UpdateModules(i);
			if (ParticleModule && ParticleModule->bEnabled && ParticleModule->bUpdateModule)
			{
				ParticleModule->Update(this, UpdateModuleOffsetData[i], DeltaTime);
			}
		}

		//@todo. This should ALWAYS be true for Trails...
		if (pkBase)
		{
			// The order of the update here is VERY important
			if (TrailModule_Source && TrailModule_Source->bEnabled)
			{
				TrailModule_Source->Update(this, TrailModule_Source_Offset, DeltaTime);
			}
			if (TrailModule_Spawn && TrailModule_Spawn->bEnabled)
			{
				TrailModule_Spawn->Update(this, TrailModule_Spawn_Offset, DeltaTime);
			}
			if (TrailModule_Taper && TrailModule_Taper->bEnabled)
			{
				TrailModule_Taper->Update(this, TrailModule_Taper_Offset, DeltaTime);
			}

			//@todo. Need to track TypeData offset into payload!
			pkBase->Update(this, TypeDataOffset, DeltaTime);
			pkBase->PostUpdate(this, TypeDataOffset, DeltaTime);
		}

		// Calculate bounding box and simulate velocity.
		UpdateBoundingBox(DeltaTime);

		//DetermineVertexAndTriangleCount();

		if (!bSuppressSpawning)
		{
			// Ensure that we flip the 'FirstEmission' flag
			FirstEmission = FALSE;
		}

		// Invalidate the contents of the vertex/index buffer.
		IsRenderDataDirty = 1;

		// Bump the tick count
		TickCount++;

		// 'Reset' the emitter time so that the delay functions correctly
		EmitterTime += CurrentDelay;
	}
	INC_DWORD_STAT(STAT_TrailParticlesTickCalls);
}

/**
 *	Update the bounding box for the emitter
 *
 *	@param	DeltaTime		The time slice to use
 */
void FParticleTrail2EmitterInstance::UpdateBoundingBox(FLOAT DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleUpdateBounds);
	if (Component)
	{
		UBOOL bUpdateBox = ((Component->bWarmingUp == FALSE) && (Component->bSkipBoundsUpdate == FALSE) &&
			(Component->Template != NULL) && (Component->Template->bUseFixedRelativeBoundingBox == FALSE));
		// Handle local space usage
		check(SpriteTemplate->LODLevels.Num() > 0);
		UParticleLODLevel* LODLevel = SpriteTemplate->LODLevels(0);
		check(LODLevel);

		if (bUpdateBox)
		{
			if (LODLevel->RequiredModule->bUseLocalSpace == FALSE) 
			{
				ParticleBoundingBox.Max = Component->LocalToWorld.GetOrigin();
				ParticleBoundingBox.Min = ParticleBoundingBox.Max;
			}
			else
			{
				ParticleBoundingBox.Max = FVector(0.0f);
				ParticleBoundingBox.Min = ParticleBoundingBox.Max;
			}
		}
		ParticleBoundingBox.IsValid = TRUE;

		// Take scale into account
		FVector Scale = FVector(1.0f, 1.0f, 1.0f);
		Scale *= Component->Scale * Component->Scale3D;
		if (Component->AbsoluteScale == FALSE)
		{
			AActor* Actor = Component->GetOwner();
			if (Actor != NULL)
			{
				Scale *= Actor->DrawScale * Actor->DrawScale3D;
			}
		}

		// As well as each particle
		if( ActiveParticles > 0 )
		{
			FVector MinPos(FLT_MAX);
			FVector MaxPos(-FLT_MAX);
			for (INT i=0; i<ActiveParticles; i++)
			{
				DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
				FVector Size = Particle->Size * Scale;
				// Do linear integrator and update bounding box
				Particle->OldLocation = Particle->Location;
				Particle->Location += DeltaTime * Particle->Velocity;
				Particle->Rotation += DeltaTime * Particle->RotationRate;
				CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
				CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
				if (bUpdateBox)
				{
					FVector TempMin = Particle->Location - Size;
					FVector TempMax = Particle->Location + Size;
					MinPos.X = Min(TempMin.X, MinPos.X);
					MinPos.Y = Min(TempMin.Y, MinPos.Y);
					MinPos.Z = Min(TempMin.Z, MinPos.Z);
					MaxPos.X = Max(TempMin.X, MaxPos.X);
					MaxPos.Y = Max(TempMin.Y, MaxPos.Y);
					MaxPos.Z = Max(TempMin.Z, MaxPos.Z);
					MinPos.X = Min(TempMax.X, MinPos.X);
					MinPos.Y = Min(TempMax.Y, MinPos.Y);
					MinPos.Z = Min(TempMax.Z, MinPos.Z);
					MaxPos.X = Max(TempMax.X, MaxPos.X);
					MaxPos.Y = Max(TempMax.Y, MaxPos.Y);
					MaxPos.Z = Max(TempMax.Z, MaxPos.Z);
				}

				// Do angular integrator, and wrap result to within +/- 2 PI
				Particle->Rotation	 = appFmod(Particle->Rotation, 2.f*(FLOAT)PI);
			}
			if (bUpdateBox)
			{
				ParticleBoundingBox += MinPos;
				ParticleBoundingBox += MaxPos;
			}
		}

		// Transform bounding box into world space if the emitter uses a local space coordinate system.
		if (bUpdateBox)
		{
			if (LODLevel->RequiredModule->bUseLocalSpace) 
			{
				ParticleBoundingBox = ParticleBoundingBox.TransformBy(Component->LocalToWorld);
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
FLOAT FParticleTrail2EmitterInstance::Spawn(FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime, INT Burst, FLOAT BurstTime)
{
	SCOPE_CYCLE_COUNTER(STAT_TrailSpawnTime);
	// If not a trail, get out
	if (!TrailTypeData)
	{
		return OldLeftover;
	}

	// Determine if no particles are alive
	UBOOL bNoLivingParticles = (ActiveParticles == 0);

	FLOAT	NewLeftover;

	UParticleLODLevel* LODLevel	= SpriteTemplate->GetLODLevel(0);
	check(LODLevel);

	FLOAT SafetyLeftover = OldLeftover;

	// Ensure continous spawning... lots of fiddling.
	NewLeftover = OldLeftover + DeltaTime * Rate;

	INT		Number		= appFloor(NewLeftover);
	FLOAT	Increment	= 1.f / Rate;
	FLOAT	StartTime	= DeltaTime + OldLeftover * Increment - Increment;
	NewLeftover			= NewLeftover - Number;

	// Always at least match the burst
	Number = Max(Number, Burst);

	// Offset burst time
	if (BurstTime > KINDA_SMALL_NUMBER)
	{
		NewLeftover -= BurstTime / Burst;
		NewLeftover	= Clamp<FLOAT>(NewLeftover, 0, NewLeftover);
	}

	Number = bNoLivingParticles ? 1 : Number;

	INT	MaxParticlesAllowed	= MaxTrailCount * TrailTypeData->MaxParticleInTrailCount;
	// Spawn for each trail
	if ((Number > 0) && (Number < TrailCount))
	{
		Number	= TrailCount;
	}

	// Don't allow more than TrailCount trails...
	if ((Number + ActiveParticles) > MaxParticlesAllowed)
	{
		Number	= MaxParticlesAllowed - ActiveParticles - 1;
		if (Number < 0)
		{
			Number = 0;
		}
	}

	// Handle growing arrays.
	UBOOL bProcessSpawn = TRUE;
	INT NewCount = ActiveParticles + Number;
	if (NewCount >= MaxActiveParticles)
	{
		if (DeltaTime < 0.25f)
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
		// Spawn particles.
		for (INT i = 0; (i < Number) && (((INT)ActiveParticles + 1) < MaxParticlesAllowed); i++)
		{
			INT		ParticleIndex	= ParticleIndices[ActiveParticles];

			DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndex);

			INT		TempOffset		= TypeDataOffset;

			INT						CurrentOffset		= TypeDataOffset;
			FLOAT*					TaperValues			= NULL;

			FTrail2TypeDataPayload* TrailData = ((FTrail2TypeDataPayload*)((BYTE*)Particle + CurrentOffset));
			FLOAT SpawnTime = StartTime - i * Increment;

			PreSpawn(Particle);
			for (INT n=0; n<LODLevel->SpawnModules.Num(); n++)
			{
				UParticleModule* SpawnModule = LODLevel->SpawnModules(n);
				if (!SpawnModule || !SpawnModule->bEnabled)
				{
					continue;
				}

				UINT* Offset = ModuleOffsetMap.Find(SpawnModule);
				SpawnModule->Spawn(this, Offset ? *Offset : 0, SpawnTime);
			}

			if ((1.0f / Particle->OneOverMaxLifetime) < 0.001f)
			{
				Particle->OneOverMaxLifetime = 1.f / 0.001f;
			}

			// The order of the Spawn here is VERY important as the modules may(will) depend on it occuring as such.
			if (TrailModule_Source && TrailModule_Source->bEnabled)
			{
				TrailModule_Source->Spawn(this, TrailModule_Source_Offset, DeltaTime);
			}
			if (TrailModule_Spawn && TrailModule_Spawn->bEnabled)
			{
				TrailModule_Spawn->Spawn(this, TrailModule_Spawn_Offset, DeltaTime);
			}
			if (TrailModule_Taper && TrailModule_Taper->bEnabled)
			{
				TrailModule_Taper->Spawn(this, TrailModule_Taper_Offset, DeltaTime);
			}
			if (LODLevel->TypeDataModule)
			{
				//@todo. Need to track TypeData offset into payload!
				LODLevel->TypeDataModule->Spawn(this, TypeDataOffset, SpawnTime);
			}

			PostSpawn(Particle, 1.f - FLOAT(i+1) / FLOAT(Number), SpawnTime);

			SourceDistanceTravelled(TrailData->TrailIndex) = 0.0f;
			LastSourcePosition(TrailData->TrailIndex)	= SourcePosition(TrailData->TrailIndex);
			SourcePosition(TrailData->TrailIndex) = Particle->Location;

			//debugf(TEXT("TrailEmitter: Spawn with tangent %s"), *(TrailData->Tangent.ToString()));
			FVector	SrcPos			= SourcePosition(TrailData->TrailIndex);
			FVector	LastSrcPos		= LastSourcePosition(TrailData->TrailIndex);
			FVector	CheckTangent	= SrcPos - LastSrcPos;
			CheckTangent.Normalize();
			//debugf(TEXT("TrailEmitter: CheckTangent       %s (%s - %s"), *CheckTangent.ToString(), *SrcPos.ToString(), *LastSrcPos.ToString());

			// Clear the next and previous - just to be safe
			TrailData->Flags = TRAIL_EMITTER_SET_NEXT(TrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
			TrailData->Flags = TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);

			// Set the tangents
			FVector	Dir	= Component->LocalToWorld.GetOrigin() - OldLocation;
			Dir.Normalize();
			TrailData->Tangent	=  Dir;

			UBOOL bAddedParticle = FALSE;
			// Determine which trail to attach to
			if (bNoLivingParticles)
			{
				// These are the first particles!
				// Tag it as the 'only'
				TrailData->Flags = TRAIL_EMITTER_SET_ONLY(TrailData->Flags);
				bNoLivingParticles	= FALSE;
				bAddedParticle		= TRUE;
			}
			else
			{
				INT iNextIndex = TRAIL_EMITTER_NULL_NEXT;
				INT iPrevIndex = TRAIL_EMITTER_NULL_PREV;

				// We need to check for existing particles, and 'link up' with them
				for (INT CheckIndex = 0; CheckIndex < ActiveParticles; CheckIndex++)
				{
					// Only care about 'head' particles...
					INT CheckParticleIndex = ParticleIndices[CheckIndex];

					// Don't check the particle of interest...
					// although this should never happen...
					if (ParticleIndex == CheckParticleIndex)
					{
						continue;
					}

					// Grab the particle and its associated trail data
					DECLARE_PARTICLE_PTR(CheckParticle, ParticleData + ParticleStride * CheckParticleIndex);

					CurrentOffset		= TypeDataOffset;

					FLOAT*					CheckTaperValues	= NULL;

					FTrail2TypeDataPayload* CheckTrailData = ((FTrail2TypeDataPayload*)((BYTE*)CheckParticle + CurrentOffset));
					//@todo. Determine how to handle multiple trails...
					if (TRAIL_EMITTER_IS_ONLY(CheckTrailData->Flags))
					{
						CheckTrailData->Flags	= TRAIL_EMITTER_SET_END(CheckTrailData->Flags);
						CheckTrailData->Flags	= TRAIL_EMITTER_SET_NEXT(CheckTrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
						CheckTrailData->Flags	= TRAIL_EMITTER_SET_PREV(CheckTrailData->Flags, ParticleIndex);

						// Now, 'join' them
						TrailData->Flags		= TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);
						TrailData->Flags		= TRAIL_EMITTER_SET_NEXT(TrailData->Flags, CheckParticleIndex);
						TrailData->Flags		= TRAIL_EMITTER_SET_START(TrailData->Flags);

						bAddedParticle = TRUE;
						break;
					}
					else
					// ISSUE: How do we determine which 'trail' to join up with????
					if (TRAIL_EMITTER_IS_START(CheckTrailData->Flags))
					{
						check(TRAIL_EMITTER_GET_NEXT(CheckTrailData->Flags) != TRAIL_EMITTER_NULL_NEXT);

						CheckTrailData->Flags	= TRAIL_EMITTER_SET_MIDDLE(CheckTrailData->Flags);
						CheckTrailData->Flags	= TRAIL_EMITTER_SET_PREV(CheckTrailData->Flags, ParticleIndex);
						// Now, 'join' them
						TrailData->Flags		= TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);
						TrailData->Flags		= TRAIL_EMITTER_SET_NEXT(TrailData->Flags, CheckParticleIndex);
						TrailData->Flags		= TRAIL_EMITTER_SET_START(TrailData->Flags);

						//SourceDistanceTravelled(TrailData->TrailIndex) += SourceDistanceTravelled(CheckTrailData->TrailIndex);

						bAddedParticle = TRUE;
						break;
					}
				}
			}

			if (bAddedParticle)
			{
				if (bClearTangents == TRUE)
				{
					TrailData->Tangent = FVector(0.0f);
				}
				ActiveParticles++;

				check((INT)ActiveParticles < TrailTypeData->MaxParticleInTrailCount);

				INC_DWORD_STAT(STAT_TrailParticlesSpawned);

				LastEmittedParticleIndex = ParticleIndex;
			}
			else
			{
				check(TEXT("Failed to add particle to trail!!!!"));
			}
		}

		if (ForceSpawnCount > 0)
		{
			ForceSpawnCount = 0;
		}

		INC_DWORD_STAT_BY(STAT_TrailParticles, ActiveParticles);

		return NewLeftover;
	}

	INC_DWORD_STAT_BY(STAT_TrailParticles, ActiveParticles);

	return SafetyLeftover;
}

/**
 *	Handle any pre-spawning actions required for particles
 *
 *	@param	Particle	The particle being spawned.
 */
void FParticleTrail2EmitterInstance::PreSpawn(FBaseParticle* Particle)
{
	FParticleEmitterInstance::PreSpawn(Particle);
	if (TrailTypeData)
	{
		TrailTypeData->PreSpawn(this, Particle);
	}
}

/**
 *	Kill off any dead particles. (Remove them from the active array)
 */
void FParticleTrail2EmitterInstance::KillParticles()
{
	if (ActiveParticles)
	{
		// Loop over the active particles... If their RelativeTime is > 1.0f (indicating they are dead),
		// move them to the 'end' of the active particle list.
		for (INT i = ActiveParticles - 1; i >= 0; i--)
		{
			const INT	CurrentIndex	= ParticleIndices[i];

			DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * CurrentIndex);
			INT						CurrentOffset		= TypeDataOffset;
			FLOAT*					TaperValues			= NULL;

			FTrail2TypeDataPayload* TrailData = ((FTrail2TypeDataPayload*)((BYTE*)Particle + CurrentOffset));
			if (Particle->RelativeTime > 1.0f)
			{
#if defined(_TRAILS_DEBUG_KILL_PARTICLES_)
				debugf(TEXT("Killing Particle %4d - Next = %4d, Prev = %4d, Type = %8s"), 
					CurrentIndex, 
					TRAIL_EMITTER_GET_NEXT(TrailData->Flags),
					TRAIL_EMITTER_GET_PREV(TrailData->Flags),
					TRAIL_EMITTER_IS_ONLY(TrailData->Flags) ? TEXT("ONLY") :
					TRAIL_EMITTER_IS_START(TrailData->Flags) ? TEXT("START") :
					TRAIL_EMITTER_IS_END(TrailData->Flags) ? TEXT("END") :
					TRAIL_EMITTER_IS_MIDDLE(TrailData->Flags) ? TEXT("MIDDLE") :
					TRAIL_EMITTER_IS_DEADTRAIL(TrailData->Flags) ? TEXT("DEAD") :
					TEXT("????")
					);
#endif	//#if defined(_TRAILS_DEBUG_KILL_PARTICLES_)

				if (TRAIL_EMITTER_IS_START(TrailData->Flags))
				{
					// Set the 'next' one in the list to the start
					INT Next = TRAIL_EMITTER_GET_NEXT(TrailData->Flags);
					if (Next != TRAIL_EMITTER_NULL_NEXT)
					{
						DECLARE_PARTICLE_PTR(NextParticle, ParticleData + ParticleStride * Next);

						CurrentOffset		= TypeDataOffset;

						FLOAT*					NextTaperValues	= NULL;

						FTrail2TypeDataPayload* NextTrailData = ((FTrail2TypeDataPayload*)((BYTE*)NextParticle + CurrentOffset));

						if (TRAIL_EMITTER_IS_END(NextTrailData->Flags))
						{
							NextTrailData->Flags = TRAIL_EMITTER_SET_ONLY(NextTrailData->Flags);
							check(TRAIL_EMITTER_GET_NEXT(NextTrailData->Flags) == TRAIL_EMITTER_NULL_NEXT);
						}
						else
						{
							NextTrailData->Flags = TRAIL_EMITTER_SET_START(NextTrailData->Flags);
						}
						NextTrailData->Flags = TRAIL_EMITTER_SET_PREV(NextTrailData->Flags, TRAIL_EMITTER_NULL_PREV);
					}
				}
				else
				if (TRAIL_EMITTER_IS_END(TrailData->Flags))
				{
					// See if there is a 'prev'
					INT Prev = TRAIL_EMITTER_GET_PREV(TrailData->Flags);
					if (Prev != TRAIL_EMITTER_NULL_PREV)
					{
						DECLARE_PARTICLE_PTR(PrevParticle, ParticleData + ParticleStride * Prev);
						CurrentOffset		= TypeDataOffset;

						FLOAT*					PrevTaperValues	= NULL;

						FTrail2TypeDataPayload* PrevTrailData = ((FTrail2TypeDataPayload*)((BYTE*)PrevParticle + CurrentOffset));
						if (TRAIL_EMITTER_IS_START(PrevTrailData->Flags))
						{
							PrevTrailData->Flags = TRAIL_EMITTER_SET_ONLY(PrevTrailData->Flags);
						}
						else
						{
							PrevTrailData->Flags = TRAIL_EMITTER_SET_END(PrevTrailData->Flags);
						}
						PrevTrailData->Flags = TRAIL_EMITTER_SET_NEXT(PrevTrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
					}
				}
				else
				if (TRAIL_EMITTER_IS_MIDDLE(TrailData->Flags))
				{
					// Break the trail? Or kill off from here to the end

					INT	Next	= TRAIL_EMITTER_GET_NEXT(TrailData->Flags);
					INT	Prev	= TRAIL_EMITTER_GET_PREV(TrailData->Flags);

#define _TRAIL_KILL_BROKEN_SEGMENT_
#if defined(_TRAIL_KILL_BROKEN_SEGMENT_)
					if (Prev != TRAIL_EMITTER_NULL_PREV)
					{
						DECLARE_PARTICLE_PTR(PrevParticle, ParticleData + ParticleStride * Prev);
						CurrentOffset		= TypeDataOffset;

						FLOAT*					PrevTaperValues	= NULL;

						FTrail2TypeDataPayload* PrevTrailData = ((FTrail2TypeDataPayload*)((BYTE*)PrevParticle + CurrentOffset));
						if (TRAIL_EMITTER_IS_START(PrevTrailData->Flags))
						{
							PrevTrailData->Flags = TRAIL_EMITTER_SET_ONLY(PrevTrailData->Flags);
						}
						else
						{
							PrevTrailData->Flags = TRAIL_EMITTER_SET_END(PrevTrailData->Flags);
						}
						PrevTrailData->Flags = TRAIL_EMITTER_SET_NEXT(PrevTrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
					}

					while (Next != TRAIL_EMITTER_NULL_NEXT)
					{
						DECLARE_PARTICLE_PTR(NextParticle, ParticleData + ParticleStride * Next);

						CurrentOffset		= TypeDataOffset;

						FLOAT*					NextTaperValues	= NULL;

						FTrail2TypeDataPayload* NextTrailData = ((FTrail2TypeDataPayload*)((BYTE*)NextParticle + CurrentOffset));

						Next	= TRAIL_EMITTER_GET_NEXT(NextTrailData->Flags);
						NextTrailData->Flags = TRAIL_EMITTER_SET_FORCEKILL(NextTrailData->Flags);
					}
#else	//#if defined(_TRAIL_KILL_BROKEN_SEGMENT_)
					//@todo. Fill in code to make the broken segment a new trail??
					if (Next != TRAIL_EMITTER_NULL_NEXT)
					{
					}
#endif	//#if defined(_TRAIL_KILL_BROKEN_SEGMENT_)
				}
				else
				if (TRAIL_EMITTER_IS_FORCEKILL(TrailData->Flags))
				{
				}
				else
				{
					check(!TEXT("What the hell are you doing in here?"));
				}

				// Clear it out...
				TrailData->Flags	= TRAIL_EMITTER_SET_NEXT(TrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
				TrailData->Flags	= TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);

				ParticleIndices[i]	= ParticleIndices[ActiveParticles-1];
				ParticleIndices[ActiveParticles-1]	= CurrentIndex;
				ActiveParticles--;

				//DEC_DWORD_STAT(STAT_TrailParticles);
				INC_DWORD_STAT(STAT_TrailParticlesKilled);
			}
		}
	}
}

/**
 *	Setup the modules for the trail emitter
 */
void FParticleTrail2EmitterInstance::SetupTrailModules()
{
	// Trails are a special case... 
	// We don't want standard Spawn/Update calls occuring on Trail-type modules.
	UParticleLODLevel* LODLevel = SpriteTemplate->GetLODLevel(0);
	check(LODLevel);
	for (INT ii = 0; ii < LODLevel->Modules.Num(); ii++)
	{
		UParticleModule* CheckModule = LODLevel->Modules(ii);
		if (CheckModule->GetModuleType() == EPMT_Trail)
		{
			UBOOL bRemove = FALSE;

			UINT* Offset;
			if (CheckModule->IsA(UParticleModuleTrailSource::StaticClass()))
			{
				if (TrailModule_Source)
				{
					debugf(TEXT("Warning: Multiple Trail Source modules!"));
				}
				TrailModule_Source	= Cast<UParticleModuleTrailSource>(CheckModule);
				Offset = ModuleOffsetMap.Find(TrailModule_Source);
				if (Offset)
				{
					TrailModule_Source_Offset	= *Offset;
				}
				bRemove	= TRUE;
			}
			else if (CheckModule->IsA(UParticleModuleTrailSpawn::StaticClass()))
			{
				if (TrailModule_Spawn)
				{
					debugf(TEXT("Warning: Multiple Trail spawn modules!"));
				}
				TrailModule_Spawn	= Cast<UParticleModuleTrailSpawn>(CheckModule);
				Offset = ModuleOffsetMap.Find(TrailModule_Spawn);
				if (Offset)
				{
					TrailModule_Spawn_Offset	= *Offset;
				}
				bRemove = TRUE;
			}
			else if (CheckModule->IsA(UParticleModuleTrailTaper::StaticClass()))
			{
				if (TrailModule_Taper)
				{
					debugf(TEXT("Warning: Multiple Trail taper modules!"));
				}
				TrailModule_Taper	= Cast<UParticleModuleTrailTaper>(CheckModule);
				Offset = ModuleOffsetMap.Find(TrailModule_Taper);
				if (Offset)
				{
					TrailModule_Taper_Offset	= *Offset;
				}
				bRemove = TRUE;
			}

			//@todo. Remove from the Update/Spawn lists???
			if (bRemove)
			{
				for (INT jj = 0; jj < LODLevel->UpdateModules.Num(); jj++)
				{
					if (LODLevel->UpdateModules(jj) == CheckModule)
					{
						LODLevel->UpdateModules.Remove(jj);
						break;
					}
				}

				for (INT kk = 0; kk < LODLevel->SpawnModules.Num(); kk++)
				{
					if (LODLevel->SpawnModules(kk) == CheckModule)
					{
						LODLevel->SpawnModules.Remove(kk);
						break;
					}
				}
			}
		}
	}
}

/**
 *	Resolve the source of the trail
 */
void FParticleTrail2EmitterInstance::ResolveSource()
{
	if (TrailModule_Source)
	{
		if (TrailModule_Source->SourceName != NAME_None)
		{
			switch (TrailModule_Source->SourceMethod)
			{
			case PET2SRCM_Actor:
				if (SourceActor == NULL)
				{
					FParticleSysParam Param;
					for (INT i = 0; i < Component->InstanceParameters.Num(); i++)
					{
						Param = Component->InstanceParameters(i);
						if (Param.Name == TrailModule_Source->SourceName)
						{
							SourceActor = Param.Actor;
							break;
						}
					}

					if (TrailModule_Source->SourceOffsetCount > 0)
					{
						for (INT i = 0; i < Component->InstanceParameters.Num(); i++)
						{
							Param = Component->InstanceParameters(i);
							FString ParamName = Param.Name.ToString();
							TCHAR* TrailSourceOffset	= appStrstr(*ParamName, TEXT("TrailSourceOffset"));
							if (TrailSourceOffset)
							{
								// Parse off the digit
								INT	Index	= appAtoi(TrailSourceOffset);
								if (Index >= 0)
								{
									if (Param.ParamType	== PSPT_Vector)
									{
										SourceOffsets.Insert(Index);
										SourceOffsets(Index)	= Param.Vector;
									}
									else
										if (Param.ParamType == PSPT_Scalar)
										{
											SourceOffsets.InsertZeroed(Index);
											SourceOffsets(Index)	= FVector(Param.Scalar, 0.0f, 0.0f);
										}
								}
							}
						}
					}
				}
				break;
			case PET2SRCM_Particle:
				if (SourceEmitter == NULL)
				{
					for (INT ii = 0; ii < Component->EmitterInstances.Num(); ii++)
					{
						FParticleEmitterInstance* pkEmitInst = Component->EmitterInstances(ii);
						if (pkEmitInst && (pkEmitInst->SpriteTemplate->EmitterName == TrailModule_Source->SourceName))
						{
							SourceEmitter = pkEmitInst;
							break;
						}
					}
				}
				break;
			}
		}
	}
}

/**
 *	Update the source data for the trail
 *
 *	@param	DeltaTime		The time slice to use for the update
 */
void FParticleTrail2EmitterInstance::UpdateSourceData(FLOAT DeltaTime)
{
	FVector	Position = Component->LocalToWorld.GetOrigin();
	FVector	Dir	= Component->LocalToWorld.GetAxis(0);
	if (TrailModule_Source == NULL)
	{
		Dir.Normalize();
	}

	for (INT i = 0; i < ActiveParticles; i++)
	{
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[i]);

		INT						CurrentOffset		= TypeDataOffset;
		FLOAT*					TaperValues			= NULL;

		FTrail2TypeDataPayload* TrailData = ((FTrail2TypeDataPayload*)((BYTE*)Particle + CurrentOffset));
		if (TRAIL_EMITTER_IS_START(TrailData->Flags))
		{
			FVector	Tangent;
			if (TrailModule_Source)
			{
				TrailModule_Source->ResolveSourcePoint(this, *Particle, *TrailData, Position, Tangent);
			}
			else
			{
				Tangent		=  Dir;
			}

			//FVector	Delta = Position - CurrentSourcePosition(TrailData->TrailIndex);
#if 0
			FVector	Delta	= CurrentSourcePosition(TrailData->TrailIndex) - LastSourcePosition(TrailData->TrailIndex);
			debugf(TEXT("\tTrail %d (0x%08x) --> %s - Distance = %s (%f) | %s vs %s"), 
				TrailData->TrailIndex, (DWORD)this,
				*Position.ToString(),
				*Delta.ToString(), Delta.Size(),
				*CurrentSourcePosition(TrailData->TrailIndex).ToString(),
				*LastSourcePosition(TrailData->TrailIndex).ToString()
				);
#endif
			CurrentSourcePosition(TrailData->TrailIndex)	= Position;
		}
	}
}

/**
 *	Determine the vertex and triangle counts for the emitter
 */
void FParticleTrail2EmitterInstance::DetermineVertexAndTriangleCount()
{
	UINT	NewSize		= 0;
	INT		TessFactor	= TrailTypeData->TessellationFactor ? TrailTypeData->TessellationFactor : 1;
	INT		Sheets		= TrailTypeData->Sheets ? TrailTypeData->Sheets : 1;
	INT		TheTrailCount	= 0;
	INT		IndexCount	= 0;

	VertexCount		= 0;
	TriangleCount	= 0;

	INT		CheckParticleCount = 0;

	for (INT ii = 0; ii < ActiveParticles; ii++)
	{
		INT		LocalVertexCount	= 0;
		INT		LocalIndexCount		= 0;

		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[ii]);

		INT						CurrentOffset		= TypeDataOffset;
		FLOAT*					TaperValues			= NULL;

		FTrail2TypeDataPayload* TrailData = ((FTrail2TypeDataPayload*)((BYTE*)Particle + CurrentOffset));
		FTrail2TypeDataPayload*	StartTrailData	= NULL;

		if (TRAIL_EMITTER_IS_START(TrailData->Flags))
		{
			StartTrailData		 = TrailData;

			INT	ParticleCount = 0;

			// if we are clipping the source segment then all of these will be zero
			if( !TrailTypeData->bClipSourceSegement )
			{
				// Count the number of particles in this trail
				ParticleCount	 = 1;
				CheckParticleCount++;

				LocalVertexCount	+= 2;
				VertexCount			+= 2;
				LocalIndexCount		+= 2;
			}


			UBOOL	bDone	= FALSE;

			while (!bDone)
			{
				ParticleCount++;
				CheckParticleCount++;

#if !defined(_TRAIL2_TESSELLATE_TO_SOURCE_)
				if ( (TRAIL_EMITTER_IS_START(TrailData->Flags)) )
				{
					if( !TrailTypeData->bClipSourceSegement )
					{
						LocalVertexCount	+= 2 * Sheets;
						VertexCount			+= 2 * Sheets;
						LocalIndexCount		+= 2 * Sheets;
					}
				}
				else
#endif	//#if !defined(_TRAIL2_TESSELLATE_TO_SOURCE_)
				{
					LocalVertexCount	+= 2 * TessFactor * Sheets;
					VertexCount			+= 2 * TessFactor * Sheets;
					LocalIndexCount		+= 2 * TessFactor * Sheets;
				}

				// The end will have Next set to the NULL flag...
				INT	Next	= TRAIL_EMITTER_GET_NEXT(TrailData->Flags);
				if (Next == TRAIL_EMITTER_NULL_NEXT)
				{
					bDone = TRUE;
				}
				else
				{
					DECLARE_PARTICLE_PTR(NextParticle, ParticleData + ParticleStride * Next);

					CurrentOffset		= TypeDataOffset;
					TrailData			= NULL;
					TaperValues			= NULL;
					TrailData = ((FTrail2TypeDataPayload*)((BYTE*)NextParticle + CurrentOffset));
				}
			}

			// @todo: We're going and modifying the original ParticleData here!  This is kind of sketchy
			//    since it's not supposed to be changed at this phase
			StartTrailData->TriangleCount	= LocalIndexCount - 2;

			// Handle degenerates - 4 tris per stitch
			LocalIndexCount	+= ((Sheets - 1) * 4);

			IndexCount	+= LocalIndexCount;

			if( ParticleCount > 1 )
			{
				TheTrailCount++;
			}
		}
	}

	if (TheTrailCount > 0)
	{
		IndexCount		+= 4 * (TheTrailCount - 1);	// 4 extra indices per Trail (degenerates)
		TriangleCount	 = IndexCount - 2;
	}
	else
	{
		IndexCount		= 0;
		TriangleCount	= 0;
	}
	//	TriangleCount	-= 1;

	//#define _TRAILS_DEBUG_VERT_TRI_COUNTS_
#if defined(_TRAILS_DEBUG_VERT_TRI_COUNTS_)
	debugf(TEXT("Trail VertexCount = %3d, TriangleCount = %3d"), VertexCount, TriangleCount);
#endif	//#if defined(_TRAILS_DEBUG_VERT_TRI_COUNTS_)
}




/**
 *	Retrieves the dynamic data for the emitter
 *	
 *	@param	bSelected					Whether the emitter is selected in the editor
 *
 *	@return	FDynamicEmitterDataBase*	The dynamic data, or NULL if it shouldn't be rendered
 */
FDynamicEmitterDataBase* FParticleTrail2EmitterInstance::GetDynamicData(UBOOL bSelected)
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetLODLevel(0);
	if (IsDynamicDataRequired(LODLevel) == FALSE)
	{
		return NULL;
	}

	// Allocate the dynamic data
	FDynamicTrail2EmitterData* NewEmitterData = ::new FDynamicTrail2EmitterData(LODLevel->RequiredModule);
	{
		SCOPE_CYCLE_COUNTER(STAT_ParticleMemTime);
		INC_DWORD_STAT(STAT_DynamicEmitterCount);
		INC_DWORD_STAT(STAT_DynamicTrailCount);
		INC_DWORD_STAT_BY(STAT_DynamicEmitterMem, sizeof(FDynamicTrail2EmitterData));
	}

	NewEmitterData->bClipSourceSegement = TrailTypeData->bClipSourceSegement;

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
UBOOL FParticleTrail2EmitterInstance::UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected)
{
	if (ActiveParticles <= 0)
	{
		return FALSE;
	}

	if (DynamicData->GetSource().eEmitterType != DET_Trail2)
	{
		checkf(0, TEXT("UpdateDynamicData> NOT A TRAIL EMITTER!"));
		return FALSE;
	}

	checkf((DynamicData->GetSource().eEmitterType == DET_Trail2), TEXT("Trail2::UpdateDynamicData> Invalid DynamicData type!"));

	FDynamicTrail2EmitterData* TrailDynamicData = (FDynamicTrail2EmitterData*)DynamicData;
	// Now fill in the source data
	if( !FillReplayData( TrailDynamicData->Source ) )
	{
		return FALSE;
	}

	// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
	TrailDynamicData->Init( bSelected );

	return TRUE;
}

/**
 *	Retrieves replay data for the emitter
 *
 *	@return	The replay data, or NULL on failure
 */
FDynamicEmitterReplayDataBase* FParticleTrail2EmitterInstance::GetReplayData()
{
	if (ActiveParticles <= 0)
	{
		return NULL;
	}

	FDynamicEmitterReplayDataBase* NewEmitterReplayData = ::new FDynamicTrail2EmitterReplayData();
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
void FParticleTrail2EmitterInstance::GetAllocatedSize(INT& OutNum, INT& OutMax)
{
	INT Size = sizeof(FParticleTrail2EmitterInstance);
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
INT FParticleTrail2EmitterInstance::GetResourceSize(UBOOL bInExclusiveResourceSizeMode)
{
	INT ResSize = 0;
	if (!bInExclusiveResourceSizeMode ||
		(Component && Component->SceneInfo && Component->SceneInfo->Proxy))
	{
		INT MaxActiveParticleDataSize = (ParticleData != NULL) ? (MaxActiveParticles * ParticleStride) : 0;
		INT MaxActiveParticleIndexSize = (ParticleIndices != NULL) ? (MaxActiveParticles * sizeof(WORD)) : 0;
		// Take dynamic data into account as well
		ResSize = sizeof(FDynamicTrail2EmitterData);
		ResSize += MaxActiveParticleDataSize;								// Copy of the particle data on the render thread
		ResSize += MaxActiveParticleIndexSize;								// Copy of the particle indices on the render thread
		if (DynamicParameterDataOffset == 0)
		{
			ResSize += MaxActiveParticles * sizeof(FParticleBeamTrailVertex);	// The vertex data array
		}
		else
		{
			ResSize += MaxActiveParticles * sizeof(FParticleBeamTrailVertexDynamicParameter);
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
UBOOL FParticleTrail2EmitterInstance::FillReplayData( FDynamicEmitterReplayDataBase& OutData )
{
	if (ActiveParticles <= 0)
	{
		return FALSE;
	}
	// This function can modify the ParticleData (changes TriangleCount of trail payloads), so we
	// we need to call it before calling the parent implementation of FillReplayData, since that
	// will memcpy the particle data to the render thread's buffer.
	DetermineVertexAndTriangleCount();

	const INT IndexCount = TriangleCount + 2;
	if (IndexCount > MAX_TRAIL_INDICES)
	{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		warnf(NAME_Warning, TEXT("TRAIL    : FillReplayData failed."));
		warnf(NAME_Warning, TEXT("\tIndexCount (%d) exceeds allowed value (%d)."), IndexCount, MAX_TRAIL_INDICES);
		warnf(NAME_Warning, TEXT("\tActiveParticleCount = %d."), ActiveParticles);
		warnf(NAME_Warning, TEXT("\tTriangleCount = %d."), TriangleCount);
		warnf(NAME_Warning, TEXT("\tTrailCount = %d."), TrailCount);
		warnf(NAME_Warning, TEXT("\t%s"), Component ? Component->Template ? 
			*(Component->Template->GetPathName()) : *(Component->GetName()) : TEXT("NO COMPONENT"));
#endif
		return FALSE;
	}

	// Call parent implementation first to fill in common particle source data
	if( !FParticleEmitterInstance::FillReplayData( OutData ) )
	{
		return FALSE;
	}

	// If the template is disabled, don't return data.
	UParticleLODLevel* LODLevel = SpriteTemplate->GetLODLevel(0);
	if ((LODLevel == NULL) || (LODLevel->bEnabled == FALSE))
	{
		return FALSE;
	}

	// Get the material instance. If there is none, or the material isn't flagged for use with particle systems, use the DefaultMaterial.
	UMaterialInterface* MaterialInst = LODLevel->RequiredModule->Material;
	if (MaterialInst == NULL || !MaterialInst->CheckMaterialUsage(MATUSAGE_BeamTrails))
	{
		MaterialInst = GEngine->DefaultMaterial;
	}


	if (TriangleCount <= 0)
	{
		if (ActiveParticles > 0)
		{

			if (!TrailTypeData->bClipSourceSegement)
			{
// 				warnf(TEXT("TRAIL: GetDynamicData -- TriangleCount == 0 (APC = %4d) for PSys %s"),
// 					ActiveParticles, 
// 					Component ? (Component->Template ? *Component->Template->GetName() : 
// 					TEXT("No Template")) : TEXT("No Component"));
#if !(FINAL_RELEASE || SHIPPING_PC_GAME)
				AWorldInfo* WorldInfo = GWorld ? GWorld->GetWorldInfo() : NULL;
				if (WorldInfo)
				{
					FString ErrorMessage = 
						FString::Printf(TEXT("TRAIL: GetDynamicData -- TriangleCount == 0 (APC = %4d) for PSys %s"),
						ActiveParticles, 
						Component ? (Component->Template ? *Component->Template->GetName() : 
						TEXT("No Template")) : TEXT("No Component"));
					FColor ErrorColor(255,0,0);
					WorldInfo->AddOnScreenDebugMessage((QWORD)((PTRINT)this), 5.0f, ErrorColor,ErrorMessage);
					debugf(*ErrorMessage);
				}
#endif	//#if !FINAL_RELEASE
			}
		}
		return FALSE;
	}


	OutData.eEmitterType = DET_Trail2;

	FDynamicTrail2EmitterReplayData* NewReplayData =
		static_cast< FDynamicTrail2EmitterReplayData* >( &OutData );

	NewReplayData->MaterialInterface = MaterialInst;

	// We never want local space for trails
	NewReplayData->bUseLocalSpace = FALSE;

	// Never use axis lock for trails
	NewReplayData->bLockAxis = FALSE;


	
	NewReplayData->TessFactor = TrailTypeData->TessellationFactor ? TrailTypeData->TessellationFactor : 1;
	NewReplayData->TessStrength = appTrunc(TrailTypeData->TessellationStrength);
	NewReplayData->TessFactorDistance = TrailTypeData->TessellationFactorDistance;
	NewReplayData->Sheets = TrailTypeData->Sheets ? TrailTypeData->Sheets : 1;

	NewReplayData->VertexCount = VertexCount;
	NewReplayData->IndexCount = TriangleCount + 2;
	NewReplayData->PrimitiveCount = TriangleCount;
	NewReplayData->TrailCount = TrailCount;

	//@todo.SAS. Check for requiring DWORD sized indices?
	NewReplayData->IndexStride = sizeof(WORD);

	TrailTypeData->GetDataPointerOffsets(this, NULL, TypeDataOffset,
		NewReplayData->TrailDataOffset, NewReplayData->TaperValuesOffset);
	NewReplayData->ParticleSourceOffset = -1;
	if (TrailModule_Source)
	{
		TrailModule_Source->GetDataPointerOffsets(this, NULL, 
			TrailModule_Source_Offset, NewReplayData->ParticleSourceOffset);
	}

	//@todo. SORTING IS A DIFFERENT ISSUE NOW! 
	//		 GParticleView isn't going to be valid anymore?

	//@todo.SAS. Optimize this nonsense...
	INT Index;
	NewReplayData->TrailSpawnTimes.Empty(TrailSpawnTimes.Num());
	for (Index = 0; Index < TrailSpawnTimes.Num(); Index++)
	{
		NewReplayData->TrailSpawnTimes.Add(appTrunc(TrailSpawnTimes(Index)));
	}
	NewReplayData->SourcePosition.Empty(SourcePosition.Num());
	for (Index = 0; Index < SourcePosition.Num(); Index++)
	{
		NewReplayData->SourcePosition.AddItem(SourcePosition(Index));
	}
	NewReplayData->LastSourcePosition.Empty(LastSourcePosition.Num());
	for (Index = 0; Index < LastSourcePosition.Num(); Index++)
	{
		NewReplayData->LastSourcePosition.AddItem(LastSourcePosition(Index));
	}
	NewReplayData->CurrentSourcePosition.Empty(CurrentSourcePosition.Num());
	for (Index = 0; Index < CurrentSourcePosition.Num(); Index++)
	{
		NewReplayData->CurrentSourcePosition.AddItem(CurrentSourcePosition(Index));
	}
	NewReplayData->LastSpawnPosition.Empty(LastSpawnPosition.Num());
	for (Index = 0; Index < LastSpawnPosition.Num(); Index++)
	{
		NewReplayData->LastSpawnPosition.AddItem(LastSpawnPosition(Index));
	}
	NewReplayData->LastSpawnTangent.Empty(LastSpawnTangent.Num());
	for (Index = 0; Index < LastSpawnTangent.Num(); Index++)
	{
		NewReplayData->LastSpawnTangent.AddItem(LastSpawnTangent(Index));
	}
	NewReplayData->SourceDistanceTravelled.Empty(SourceDistanceTravelled.Num());
	for (Index = 0; Index < SourceDistanceTravelled.Num(); Index++)
	{
		NewReplayData->SourceDistanceTravelled.AddItem(SourceDistanceTravelled(Index));
	}
	NewReplayData->SourceOffsets.Empty(SourceOffsets.Num());
	for (Index = 0; Index < SourceOffsets.Num(); Index++)
	{
		NewReplayData->SourceOffsets.AddItem(SourceOffsets(Index));
	}

	return TRUE;
}

/*-----------------------------------------------------------------------------
	FParticleTrailsEmitterInstance_Base.
-----------------------------------------------------------------------------*/
IMPLEMENT_PARTICLEEMITTERINSTANCE_TYPE(FParticleTrailsEmitterInstance_Base);

void FParticleTrailsEmitterInstance_Base::Init()
{
	FParticleEmitterInstance::Init();
	SetupTrailModules();
}

void FParticleTrailsEmitterInstance_Base::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, UBOOL bClearResources)
{
	FParticleEmitterInstance::InitParameters(InTemplate, InComponent, bClearResources);
	if (GIsEditor)
	{
		UParticleLODLevel* LODLevel	= InTemplate->GetLODLevel(0);
		check(LODLevel);
		UMaterialInterface* MaterialInst = LODLevel->RequiredModule->Material;
		if (MaterialInst != NULL)
		{
			MaterialInst->CheckMaterialUsage(MATUSAGE_BeamTrails);
		}
	}
}

void FParticleTrailsEmitterInstance_Base::Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning)
{
	SCOPE_CYCLE_COUNTER(STAT_TrailTickTime);
	if (Component)
	{
		check(SpriteTemplate);
		check(SpriteTemplate->LODLevels.Num() > 0);

		// If this the FirstTime we are being ticked?
		UBOOL bFirstTime = (SecondsSinceCreation > 0.0f) ? FALSE : TRUE;

		// Grab the current LOD level
		UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
		check(LODLevel);

// 		if (ActiveParticles == 0)
// 		{
// 			RunningTime = 0.0f;
// 		}
		check(DeltaTime >= 0.0f);

		// Handle EmitterTime setup, looping, etc.
		FLOAT EmitterDelay = Tick_EmitterTimeSetup(DeltaTime, LODLevel);

		// Update the source data (position, etc.)
		UpdateSourceData(DeltaTime, bFirstTime);

		// Kill off any dead particles
		KillParticles();

		// Spawn Particles...
		SpawnFraction = Tick_SpawnParticles(DeltaTime, LODLevel, bSuppressSpawning, bFirstTime);

		// Reset particle parameters.
		ResetParticleParameters(DeltaTime, STAT_SpriteParticlesUpdated);

		// Update the particles
		SCOPE_CYCLE_COUNTER(STAT_ParticleUpdateTime);

		// Module pre update
		Tick_ModulePreUpdate(DeltaTime, LODLevel);

		// Update existing particles (might respawn dying ones).
		Tick_ModuleUpdate(DeltaTime, LODLevel);

		// Module post update 
		Tick_ModulePostUpdate(DeltaTime, LODLevel);

		// Update the orbit data...
	// 	UpdateOrbitData(DeltaTime);

		// Calculate bounding box and simulate velocity.
		UpdateBoundingBox(DeltaTime);

		// Perform any final updates...
		Tick_ModuleFinalUpdate(DeltaTime, LODLevel);

		// Recalculate tangents, if enabled
		Tick_RecalculateTangents(DeltaTime, LODLevel);

		// Invalidate the contents of the vertex/index buffer.
		IsRenderDataDirty = 1;

		// 'Reset' the emitter time so that the delay functions correctly
		EmitterTime += CurrentDelay;
		RunningTime += DeltaTime;
	}
	LastTickTime = GWorld ? GWorld->GetTimeSeconds() : 0.0f;
	INC_DWORD_STAT(STAT_TrailParticlesTickCalls);
}

/**
 *	Tick sub-function that handles recalculation of tangents
 *
 *	@param	DeltaTime			The current time slice
 *	@param	CurrentLODLevel		The current LOD level for the instance
 */
void FParticleTrailsEmitterInstance_Base::Tick_RecalculateTangents(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel)
{
}

void FParticleTrailsEmitterInstance_Base::UpdateBoundingBox(FLOAT DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleUpdateBounds);
	if (Component)
	{
		UBOOL bUpdateBox = ((Component->bWarmingUp == FALSE) && (Component->bSkipBoundsUpdate == FALSE) &&
			(Component->Template != NULL) && (Component->Template->bUseFixedRelativeBoundingBox == FALSE));
		// Handle local space usage
		check(SpriteTemplate->LODLevels.Num() > 0);
		UParticleLODLevel* LODLevel = SpriteTemplate->LODLevels(0);
		check(LODLevel);

		if (bUpdateBox)
		{
			// Set the min/max to the position of the trail
			if (LODLevel->RequiredModule->bUseLocalSpace == FALSE) 
			{
				FVector Origin = Component->LocalToWorld.GetOrigin();
				ParticleBoundingBox.Max = Origin;
				ParticleBoundingBox.Min = Origin;
			}
			else
			{
				ParticleBoundingBox.Max = FVector(0.0f);
				ParticleBoundingBox.Min = FVector(0.0f);
			}
		}
		ParticleBoundingBox.IsValid = TRUE;

		// Take scale into account
		FVector Scale = FVector(1.0f, 1.0f, 1.0f);
		Scale *= Component->Scale * Component->Scale3D;
		if (Component->AbsoluteScale == FALSE)
		{
			AActor* Actor = Component->GetOwner();
			if (Actor != NULL)
			{
				Scale *= Actor->DrawScale * Actor->DrawScale3D;
			}
		}

		// As well as each particle
		INT LocalActiveParticles = ActiveParticles;
		if (LocalActiveParticles > 0)
		{
			FVector MinPos(FLT_MAX);
			FVector MaxPos(-FLT_MAX);
			FVector TempMin;
			FVector TempMax;
			for (INT i = 0; i < LocalActiveParticles; i++)
			{
				DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
				FVector Size = Particle->Size * Scale;
				// Do linear integrator and update bounding box
				Particle->Location	+= DeltaTime * Particle->Velocity;
				Particle->Rotation	+= DeltaTime * Particle->RotationRate;
				CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
				CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
				Particle->OldLocation = Particle->Location;
				if (bUpdateBox)
				{
					TempMin = Particle->Location - Size;
					TempMax = Particle->Location + Size;
					MinPos.X = Min(TempMin.X, MinPos.X);
					MinPos.Y = Min(TempMin.Y, MinPos.Y);
					MinPos.Z = Min(TempMin.Z, MinPos.Z);
					MaxPos.X = Max(TempMin.X, MaxPos.X);
					MaxPos.Y = Max(TempMin.Y, MaxPos.Y);
					MaxPos.Z = Max(TempMin.Z, MaxPos.Z);
					MinPos.X = Min(TempMax.X, MinPos.X);
					MinPos.Y = Min(TempMax.Y, MinPos.Y);
					MinPos.Z = Min(TempMax.Z, MinPos.Z);
					MaxPos.X = Max(TempMax.X, MaxPos.X);
					MaxPos.Y = Max(TempMax.Y, MaxPos.Y);
					MaxPos.Z = Max(TempMax.Z, MaxPos.Z);
				}

				// Do angular integrator, and wrap result to within +/- 2 PI
				Particle->Rotation	 = appFmod(Particle->Rotation, 2.f*(FLOAT)PI);
			}
			if (bUpdateBox)
			{
				ParticleBoundingBox += MinPos;
				ParticleBoundingBox += MaxPos;
			}
		}

		// Transform bounding box into world space if the emitter uses a local space coordinate system.
		if (bUpdateBox)
		{
			if (LODLevel->RequiredModule->bUseLocalSpace) 
			{
				ParticleBoundingBox = ParticleBoundingBox.TransformBy(Component->LocalToWorld);
			}
		}
	}
}

void FParticleTrailsEmitterInstance_Base::UpdateSourceData(FLOAT DeltaTime, UBOOL bFirstTime)
{
}

FLOAT FParticleTrailsEmitterInstance_Base::Spawn(FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime, INT Burst, FLOAT BurstTime)
{
	return 0.0f;
}

void FParticleTrailsEmitterInstance_Base::PreSpawn(FBaseParticle* Particle)
{
	// Don't need to do anything
	FParticleEmitterInstance::PreSpawn(Particle);
}

void FParticleTrailsEmitterInstance_Base::KillParticles()
{
	if (ActiveParticles > 0)
	{
		FLOAT CurrentTickTime = GWorld ? GWorld->GetTimeSeconds() : 0.0f;
		UBOOL bHasForceKillParticles = FALSE;
		// Loop over the active particles... If their RelativeTime is > 1.0f (indicating they are dead),
		// move them to the 'end' of the active particle list.
		for (INT ParticleIdx = ActiveParticles - 1; ParticleIdx >= 0; ParticleIdx--)
		{
			const INT CurrentIndex = ParticleIndices[ParticleIdx];

			DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * CurrentIndex);
			FTrailsBaseTypeDataPayload* TrailData = ((FTrailsBaseTypeDataPayload*)((BYTE*)Particle + TypeDataOffset));
			if ((Particle->RelativeTime > 1.0f) ||
				((bEnableInactiveTimeTracking == TRUE) && 
				 (CurrentTickTime != 0.0f) && 
				 ((CurrentTickTime - LastTickTime) > (1.0f / Particle->OneOverMaxLifetime)))
				)
			{
#if defined(_TRAILS_DEBUG_KILL_PARTICLES_)
				debugf(TEXT("Killing Particle %4d - Next = %4d, Prev = %4d, Type = %8s"), 
					CurrentIndex, 
					TRAIL_EMITTER_GET_NEXT(TrailData->Flags),
					TRAIL_EMITTER_GET_PREV(TrailData->Flags),
					TRAIL_EMITTER_IS_ONLY(TrailData->Flags) ? TEXT("ONLY") :
					TRAIL_EMITTER_IS_START(TrailData->Flags) ? TEXT("START") :
					TRAIL_EMITTER_IS_END(TrailData->Flags) ? TEXT("END") :
					TRAIL_EMITTER_IS_MIDDLE(TrailData->Flags) ? TEXT("MIDDLE") :
					TRAIL_EMITTER_IS_DEADTRAIL(TrailData->Flags) ? TEXT("DEAD") :
					TEXT("????")
					);
#endif	//#if defined(_TRAILS_DEBUG_KILL_PARTICLES_)

				if (TRAIL_EMITTER_IS_HEAD(TrailData->Flags))
				{
					// Set the 'next' one in the list to the start
					INT Next = TRAIL_EMITTER_GET_NEXT(TrailData->Flags);
					if (Next != TRAIL_EMITTER_NULL_NEXT)
					{
						DECLARE_PARTICLE_PTR(NextParticle, ParticleData + ParticleStride * Next);
						FTrailsBaseTypeDataPayload* NextTrailData = ((FTrailsBaseTypeDataPayload*)((BYTE*)NextParticle + TypeDataOffset));
						if (TRAIL_EMITTER_IS_END(NextTrailData->Flags))
						{
							if (TRAIL_EMITTER_IS_START(TrailData->Flags))
							{
								NextTrailData->Flags = TRAIL_EMITTER_SET_ONLY(NextTrailData->Flags);
							}
							else if (TRAIL_EMITTER_IS_DEADTRAIL(TrailData->Flags))
							{
								NextTrailData->Flags = TRAIL_EMITTER_SET_DEADTRAIL(NextTrailData->Flags);
							}
							check(TRAIL_EMITTER_GET_NEXT(NextTrailData->Flags) == TRAIL_EMITTER_NULL_NEXT);
						}
						else
						{
							if (TRAIL_EMITTER_IS_START(TrailData->Flags))
							{
								NextTrailData->Flags = TRAIL_EMITTER_SET_START(NextTrailData->Flags);
							}
							else
							{
								NextTrailData->Flags = TRAIL_EMITTER_SET_DEADTRAIL(NextTrailData->Flags);
							}
						}
						NextTrailData->Flags = TRAIL_EMITTER_SET_PREV(NextTrailData->Flags, TRAIL_EMITTER_NULL_PREV);
					}
				}
				else if (TRAIL_EMITTER_IS_END(TrailData->Flags))
				{
					// See if there is a 'prev'
					INT Prev = TRAIL_EMITTER_GET_PREV(TrailData->Flags);
					if (Prev != TRAIL_EMITTER_NULL_PREV)
					{
						DECLARE_PARTICLE_PTR(PrevParticle, ParticleData + ParticleStride * Prev);
						FTrailsBaseTypeDataPayload* PrevTrailData = ((FTrailsBaseTypeDataPayload*)((BYTE*)PrevParticle + TypeDataOffset));
						if (TRAIL_EMITTER_IS_START(PrevTrailData->Flags))
						{
							PrevTrailData->Flags = TRAIL_EMITTER_SET_ONLY(PrevTrailData->Flags);
						}
						else if (TRAIL_EMITTER_IS_DEADTRAIL(PrevTrailData->Flags))
						{
							// Nothing to do in this case.
							PrevTrailData->TriangleCount = 0;
							PrevTrailData->RenderingInterpCount = 1;
						}
						else
						{
							PrevTrailData->Flags = TRAIL_EMITTER_SET_END(PrevTrailData->Flags);
						}
						PrevTrailData->Flags = TRAIL_EMITTER_SET_NEXT(PrevTrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
					}
				}
				else if (TRAIL_EMITTER_IS_MIDDLE(TrailData->Flags))
				{
					// Break the trail? Or kill off from here to the end
					INT	Next = TRAIL_EMITTER_GET_NEXT(TrailData->Flags);
					INT	Prev = TRAIL_EMITTER_GET_PREV(TrailData->Flags);

					// Kill off the broken segment...
					if (Prev != TRAIL_EMITTER_NULL_PREV)
					{
						DECLARE_PARTICLE_PTR(PrevParticle, ParticleData + ParticleStride * Prev);
						FTrailsBaseTypeDataPayload* PrevTrailData = ((FTrailsBaseTypeDataPayload*)((BYTE*)PrevParticle + TypeDataOffset));
						if (!TRAIL_EMITTER_IS_HEAD(PrevTrailData->Flags))
						{
							PrevTrailData->Flags = TRAIL_EMITTER_SET_END(PrevTrailData->Flags);
						}
						PrevTrailData->Flags = TRAIL_EMITTER_SET_NEXT(PrevTrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
					}

					while (Next != TRAIL_EMITTER_NULL_NEXT)
					{
						DECLARE_PARTICLE_PTR(NextParticle, ParticleData + ParticleStride * Next);
						FTrailsBaseTypeDataPayload* NextTrailData = ((FTrailsBaseTypeDataPayload*)((BYTE*)NextParticle + TypeDataOffset));
						Next = TRAIL_EMITTER_GET_NEXT(NextTrailData->Flags);
						NextTrailData->Flags = TRAIL_EMITTER_SET_FORCEKILL(NextTrailData->Flags);
						bHasForceKillParticles = TRUE;
					}
				}
				else if (TRAIL_EMITTER_IS_FORCEKILL(TrailData->Flags))
				{
				}
				else
				{
					check(!TEXT("What the hell are you doing in here?"));
				}

				// Clear it out... just to be safe when it gets pulled back to active
				TrailData->Flags = TRAIL_EMITTER_SET_NEXT(TrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
				TrailData->Flags = TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);
				ParticleIndices[ParticleIdx] = ParticleIndices[ActiveParticles-1];
				ParticleIndices[ActiveParticles-1]	= CurrentIndex;
				ActiveParticles--;

				//DEC_DWORD_STAT(STAT_TrailParticles);
				INC_DWORD_STAT(STAT_TrailParticlesKilled);
			}
		}

		if (bHasForceKillParticles == TRUE)
		{
			// need to kill all these off as well...
			for (INT ParticleIdx = ActiveParticles - 1; ParticleIdx >= 0; ParticleIdx--)
			{
				const INT CurrentIndex = ParticleIndices[ParticleIdx];
				DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * CurrentIndex);
				FTrailsBaseTypeDataPayload* TrailData = ((FTrailsBaseTypeDataPayload*)((BYTE*)Particle + TypeDataOffset));
				if (TRAIL_EMITTER_IS_FORCEKILL(TrailData->Flags))
				{
					TrailData->Flags = TRAIL_EMITTER_SET_NEXT(TrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
					TrailData->Flags = TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);
					ParticleIndices[ParticleIdx] = ParticleIndices[ActiveParticles-1];
					ParticleIndices[ActiveParticles-1]	= CurrentIndex;
					ActiveParticles--;
				}
			}
		}
	}
}

/**
 *	Kill the given number of particles from the end of the trail.
 *
 *	@param	InTrailIdx		The trail to kill particles in.
 *	@param	InKillCount		The number of particles to kill off.
 */
void FParticleTrailsEmitterInstance_Base::KillParticles(INT InTrailIdx, INT InKillCount)
{
	if (ActiveParticles)
	{
		INT KilledCount = 0;
		// Loop over the active particles...
		for (INT ParticleIdx = ActiveParticles - 1; ParticleIdx >= 0 && (KilledCount < InKillCount); ParticleIdx--)
		{
			// Find the end particle
			INT CurrentIndex = ParticleIndices[ParticleIdx];
			DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * CurrentIndex);
			FTrailsBaseTypeDataPayload* TrailData = ((FTrailsBaseTypeDataPayload*)((BYTE*)Particle + TypeDataOffset));
			if (TRAIL_EMITTER_IS_END(TrailData->Flags))
			{
				if (TrailData->TrailIndex == InTrailIdx)
				{
					while ((TrailData != NULL) && (KilledCount < InKillCount))
					{
						// Mark it for death...
						Particle->RelativeTime = 1.1f;
						KilledCount++;
						// See if there is a 'prev'
						INT Prev = TRAIL_EMITTER_GET_PREV(TrailData->Flags);
						if (Prev != TRAIL_EMITTER_NULL_PREV)
						{
							Particle = (FBaseParticle*)(ParticleData + (ParticleStride * Prev));
							TrailData = ((FTrailsBaseTypeDataPayload*)((BYTE*)Particle + TypeDataOffset));
							if (TRAIL_EMITTER_IS_START(TrailData->Flags))
							{
								// Don't kill the start, no matter what...
								TrailData = NULL;
							}
							else if (TRAIL_EMITTER_IS_DEADTRAIL(TrailData->Flags))
							{
								// Nothing to do in this case.
								TrailData->TriangleCount = 0;
								TrailData->RenderingInterpCount = 1;
							}
						}
					}

					if (TrailData == NULL)
					{
						// Force it to exit the loop...
						KilledCount = InKillCount;
					}
				}
			}
		}

		if (KilledCount > 0)
		{
			// Now use the standard KillParticles call...
			KillParticles();
		}
	}
}

/**
 *	Called when the particle system is deactivating...
 */
void FParticleTrailsEmitterInstance_Base::OnDeactivateSystem()
{
	FParticleEmitterInstance::OnDeactivateSystem();

	// Mark trails as dead if the option has been enabled...
	if (bDeadTrailsOnDeactivate == TRUE)
	{
		for (INT ParticleIdx = 0; ParticleIdx < ActiveParticles; ParticleIdx++)
		{
			DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[ParticleIdx]);
			FBaseParticle* CurrParticle = Particle;
			FTrailsBaseTypeDataPayload*	CurrTrailData = ((FTrailsBaseTypeDataPayload*)((BYTE*)Particle + TypeDataOffset));
			if (TRAIL_EMITTER_IS_START(CurrTrailData->Flags))
			{
				CurrTrailData->Flags = TRAIL_EMITTER_SET_DEADTRAIL(CurrTrailData->Flags);
			}
		}
	}
}

/**
 *	Retrieve the particle in the trail that meets the given criteria
 *
 *	@param	bSkipStartingParticle		If TRUE, don't check the starting particle for meeting the criteria
 *	@param	InStartingFromParticle		The starting point for the search.
 *	@param	InStartingTrailData			The trail data for the starting point.
 *	@param	bPrevious					If TRUE, search PREV entries. Search NEXT entries otherwise.
 *	@param	InGetOption					Options for defining the type of particle.
 *	@param	OutParticle					The particle that meets the criteria.
 *	@param	OutTrailData				The trail data of the particle that meets the criteria.
 *
 *	@return	UBOOL						TRUE if found, FALSE if not.
 */
UBOOL FParticleTrailsEmitterInstance_Base::GetParticleInTrail(
	UBOOL bSkipStartingParticle,
	FBaseParticle* InStartingFromParticle,
	FTrailsBaseTypeDataPayload* InStartingTrailData,
	EGetTrailDirection InGetDirection, 
	EGetTrailParticleOption InGetOption,
	FBaseParticle*& OutParticle,
	FTrailsBaseTypeDataPayload*& OutTrailData)
{
	OutParticle = NULL;
	OutTrailData = NULL;
	if ((InStartingFromParticle == NULL) || (InStartingTrailData == NULL))
	{
		return FALSE;
	}

	if ((InGetOption == GET_End) && (InGetDirection == GET_Prev))
	{
		// Wrong direction!
		warnf(TEXT("GetParticleInTrail: END particle will always be in the NEXT direction!"));
	}
	if ((InGetOption == GET_Start) && (InGetDirection == GET_Next))
	{
		// Wrong direction!
		warnf(TEXT("GetParticleInTrail: START particle will always be in the PREV direction!"));
	}

	UBOOL bDone = FALSE;
	FBaseParticle* CheckParticle = InStartingFromParticle;
	FTrailsBaseTypeDataPayload* CheckTrailData = InStartingTrailData;
	UBOOL bCheckIt = !bSkipStartingParticle;
	while (!bDone)
	{
		if (bCheckIt == TRUE)
		{
			UBOOL bItsGood = FALSE;
			switch (InGetOption)
			{
			case GET_Any:
				bItsGood = TRUE;
				break;
			case GET_Spawned:
				if (CheckTrailData->bInterpolatedSpawn == FALSE)
				{
					bItsGood = TRUE;
				}
				break;
			case GET_Interpolated:
				if (CheckTrailData->bInterpolatedSpawn == TRUE)
				{
					bItsGood = TRUE;
				}
				break;
			case GET_Start:
				if (TRAIL_EMITTER_IS_START(CheckTrailData->Flags))
				{
					bItsGood = TRUE;
				}
				break;
			case GET_End:
				if (TRAIL_EMITTER_IS_END(CheckTrailData->Flags))
				{
					bItsGood = TRUE;
				}
				break;
			}

			if (bItsGood == TRUE)
			{
				OutParticle = CheckParticle;
				OutTrailData = CheckTrailData;
				bDone = TRUE;
			}
		}

		INT Index = -1;
		if (!bDone)
		{
			// Keep looking...
			if (InGetDirection == GET_Prev)
			{
				Index = TRAIL_EMITTER_GET_PREV(CheckTrailData->Flags);
				if (Index == TRAIL_EMITTER_NULL_PREV)
				{
					Index = -1;
				}
			}
			else
			{
				Index = TRAIL_EMITTER_GET_NEXT(CheckTrailData->Flags);
				if (Index == TRAIL_EMITTER_NULL_NEXT)
				{
					Index = -1;
				}
			}
		}

		if (Index != -1)
		{
			DECLARE_PARTICLE_PTR(TempParticle, ParticleData + ParticleStride * Index);
			CheckParticle = TempParticle;
			CheckTrailData = ((FTrailsBaseTypeDataPayload*)((BYTE*)CheckParticle + TypeDataOffset));
			bCheckIt = TRUE;
		}
		else
		{
			bDone = TRUE;
		}
	}

	return ((OutParticle != NULL) && (OutTrailData != NULL));
}

/*-----------------------------------------------------------------------------
	FParticleRibbonEmitterInstance.
-----------------------------------------------------------------------------*/
IMPLEMENT_PARTICLEEMITTERINSTANCE_TYPE(FParticleRibbonEmitterInstance);

/** Constructor	*/
FParticleRibbonEmitterInstance::FParticleRibbonEmitterInstance() :
	  FParticleTrailsEmitterInstance_Base()
	, TrailTypeData(NULL)
	, SpawnPerUnitModule(NULL)
	, SourceModule(NULL)
	, TrailModule_Source_Offset(-1)
	, SourceActor(NULL)
	, SourceEmitter(NULL)
	, LastSelectedParticleIndex(-1)
	, HeadOnlyParticles(0)
{
	// Always want this true for ribbons...
	bEnableInactiveTimeTracking = TRUE;
	CurrentSourcePosition.Empty();
	CurrentSourceRotation.Empty();
	CurrentSourceUp.Empty();
	CurrentSourceTangent.Empty();
	CurrentSourceTangentStrength.Empty();
	LastSourcePosition.Empty();
	LastSourceRotation.Empty();
	LastSourceUp.Empty();
	LastSourceTangent.Empty();
	LastSourceTangentStrength.Empty();
	SourceOffsets.Empty();
	SourceIndices.Empty();
	SourceTimes.Empty();
	LastSourceTimes.Empty();
	CurrentLifetimes.Empty();
}

/** Destructor	*/
FParticleRibbonEmitterInstance::~FParticleRibbonEmitterInstance()
{
}

void FParticleRibbonEmitterInstance::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, UBOOL bClearResources)
{
	FParticleTrailsEmitterInstance_Base::InitParameters(InTemplate, InComponent, bClearResources);

	// We don't support LOD on trails
	UParticleLODLevel* LODLevel	= InTemplate->GetLODLevel(0);
	check(LODLevel);
	TrailTypeData	= CastChecked<UParticleModuleTypeDataRibbon>(LODLevel->TypeDataModule);
	check(TrailTypeData);

	// Always have at least one trail
	if (TrailTypeData->MaxTrailCount <= 0)
	{
		TrailTypeData->MaxTrailCount = 1;
	}

	bDeadTrailsOnDeactivate = TrailTypeData->bDeadTrailsOnDeactivate;

	MaxTrailCount = TrailTypeData->MaxTrailCount;
	TrailSpawnTimes.Empty(MaxTrailCount);
	TrailSpawnTimes.AddZeroed(MaxTrailCount);
	CurrentSourcePosition.Empty(MaxTrailCount);
	CurrentSourcePosition.AddZeroed(MaxTrailCount);
	CurrentSourceRotation.Empty(MaxTrailCount);
	CurrentSourceRotation.AddZeroed(MaxTrailCount);
	CurrentSourceUp.Empty(MaxTrailCount);
	CurrentSourceUp.AddZeroed(MaxTrailCount);
	CurrentSourceTangent.Empty(MaxTrailCount);
	CurrentSourceTangent.AddZeroed(MaxTrailCount);
	CurrentSourceTangentStrength.Empty(MaxTrailCount);
	CurrentSourceTangentStrength.AddZeroed(MaxTrailCount);
	LastSourcePosition.Empty(MaxTrailCount);
	LastSourcePosition.AddZeroed(MaxTrailCount);
	LastSourceRotation.Empty(MaxTrailCount);
	LastSourceRotation.AddZeroed(MaxTrailCount);
	LastSourceUp.Empty(MaxTrailCount);
	LastSourceUp.AddZeroed(MaxTrailCount);
	LastSourceTangent.Empty(MaxTrailCount);
	LastSourceTangent.AddZeroed(MaxTrailCount);
	LastSourceTangentStrength.Empty(MaxTrailCount);
	LastSourceTangentStrength.AddZeroed(MaxTrailCount);
	SourceDistanceTraveled.Empty(MaxTrailCount);
	SourceDistanceTraveled.AddZeroed(MaxTrailCount);
	TiledUDistanceTraveled.Empty(MaxTrailCount);
	TiledUDistanceTraveled.AddZeroed(MaxTrailCount);
	SourceOffsets.Empty(MaxTrailCount);
	SourceOffsets.AddZeroed(MaxTrailCount);
	SourceIndices.Empty(MaxTrailCount);
	SourceIndices.AddZeroed(MaxTrailCount);
	appMemset(SourceIndices.GetData(), 0xff, MaxTrailCount * sizeof(INT));
	SourceTimes.Empty(MaxTrailCount);
	SourceTimes.AddZeroed(MaxTrailCount);
	LastSourceTimes.Empty(MaxTrailCount);
	LastSourceTimes.AddZeroed(MaxTrailCount);
	CurrentLifetimes.Empty(MaxTrailCount);
	CurrentLifetimes.AddZeroed(MaxTrailCount);
	CurrentSizes.Empty(MaxTrailCount);
	CurrentSizes.AddZeroed(MaxTrailCount);

	//
	VertexCount = 0;
	TriangleCount = 0;

	// Resolve any actors...
	ResolveSource();
}

UBOOL TrailsBase_CalculateTangent(
	FBaseParticle* InPrevParticle, FRibbonTypeDataPayload* InPrevTrailData, 
	FBaseParticle* InNextParticle, FRibbonTypeDataPayload* InNextTrailData,
	FLOAT InCurrNextDelta, FRibbonTypeDataPayload* InOutCurrTrailData)
{
	// Recalculate the current tangent...
	// Calculate the new tangent from the previous and next position...
	// LastSourcePosition will be that position of the first particle that will be spawned this frame
	FVector PositionDelta = InPrevParticle->Location - InNextParticle->Location;
	FLOAT TimeDelta = InPrevTrailData->SpawnTime - InNextTrailData->SpawnTime;
	// Disabling the check as it apparently is causing problems 
	//check(TimeDelta >= 0.0f);
	TimeDelta = (TimeDelta > 0.0f) ? TimeDelta : ((TimeDelta < 0.0f) ? Abs(TimeDelta) : 0.0032f);
	FVector NewTangent = (PositionDelta / TimeDelta);
	UBOOL bItsGood = TRUE;
	if (NewTangent.IsNearlyZero() == TRUE)
	{
		bItsGood = FALSE;
	}
	NewTangent *= InCurrNextDelta;
	if (NewTangent.IsNearlyZero() == TRUE)
	{
		bItsGood = FALSE;
	}
	NewTangent *= (1.0f / InOutCurrTrailData->SpawnedTessellationPoints);
	if (NewTangent.IsNearlyZero() == TRUE)
	{
		bItsGood = FALSE;
	}

	if (bItsGood == TRUE)
	{
		InOutCurrTrailData->Tangent = NewTangent;
	}

	return bItsGood;
}

/**
 *	Tick sub-function that handles recalculation of tangents
 *
 *	@param	DeltaTime			The current time slice
 *	@param	CurrentLODLevel		The current LOD level for the instance
 */
void FParticleRibbonEmitterInstance::Tick_RecalculateTangents(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel)
{
	if (TrailTypeData->bTangentRecalculationEveryFrame == TRUE)
	{
		for (INT TrailIdx = 0; TrailIdx < MaxTrailCount; TrailIdx++)
		{
			// Find the Start particle of the current trail...
			FBaseParticle* StartParticle = NULL;
			FRibbonTypeDataPayload* StartTrailData = NULL;
			for (INT FindTrailIdx = 0; FindTrailIdx < ActiveParticles; FindTrailIdx++)
			{
				INT CheckStartIndex = ParticleIndices[FindTrailIdx];
				DECLARE_PARTICLE_PTR(CheckParticle, ParticleData + ParticleStride * CheckStartIndex);
				FRibbonTypeDataPayload* CheckTrailData = ((FRibbonTypeDataPayload*)((BYTE*)CheckParticle + TypeDataOffset));
				if (TRAIL_EMITTER_IS_START(CheckTrailData->Flags))
				{
					if (CheckTrailData->TrailIndex == TrailIdx)
					{
						StartParticle = CheckParticle;
						StartTrailData = CheckTrailData;
						break;
					}
				}
			}

			// Recalculate tangents at each particle to properly handle moving particles...
			if ((StartParticle != NULL) && (TRAIL_EMITTER_IS_ONLY(StartTrailData->Flags) == 0))
			{
				// For trails, particles go:
				//     START, next, next, ..., END
				// Coming from the end,
				//     END, prev, prev, ..., START
				FBaseParticle* PrevParticle = StartParticle;
				FRibbonTypeDataPayload* PrevTrailData = StartTrailData;
				FBaseParticle* CurrParticle = NULL;
				FRibbonTypeDataPayload* CurrTrailData = NULL;
				FBaseParticle* NextParticle = NULL;
				FTrailsBaseTypeDataPayload* TempPayload = NULL;
				FRibbonTypeDataPayload* NextTrailData = NULL;

				GetParticleInTrail(TRUE, PrevParticle, PrevTrailData, GET_Next, GET_Any, CurrParticle, TempPayload);
				CurrTrailData = (FRibbonTypeDataPayload*)(TempPayload);

				// Deal with the start particle...
				if (CurrParticle != NULL)
				{
					TrailsBase_CalculateTangent(PrevParticle, PrevTrailData, CurrParticle, CurrTrailData, 
						(PrevTrailData->SpawnTime - CurrTrailData->SpawnTime), 
						PrevTrailData);
				}

				while (CurrParticle != NULL)
				{
					// Grab the next particle in the trail...
					GetParticleInTrail(TRUE, CurrParticle, CurrTrailData, GET_Next, GET_Any, NextParticle, TempPayload);
					NextTrailData = (FRibbonTypeDataPayload*)(TempPayload);

					check(CurrParticle != PrevParticle);
					check(CurrParticle != NextParticle);

					if (NextParticle != NULL)
					{
						TrailsBase_CalculateTangent(PrevParticle, PrevTrailData, NextParticle, NextTrailData, 
							(CurrTrailData->SpawnTime - NextTrailData->SpawnTime),
							CurrTrailData);
					}
					else
					{
						// The start particle... should we recalc w/ the current source position???
						TrailsBase_CalculateTangent(PrevParticle, PrevTrailData, CurrParticle, CurrTrailData, 
							(PrevTrailData->SpawnTime - CurrTrailData->SpawnTime),
							CurrTrailData);
					}

					// Move up the chain...
					PrevParticle = CurrParticle;
					PrevTrailData = CurrTrailData;
					CurrParticle = NextParticle;
					CurrTrailData = NextTrailData;
				}
			}
		}
	}
}

/**
 *	Tick sub-function that handles module post updates
 *
 *	@param	DeltaTime			The current time slice
 *	@param	CurrentLODLevel		The current LOD level for the instance
 */
void FParticleRibbonEmitterInstance::Tick_ModulePostUpdate(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel)
{
	UParticleModuleTypeDataRibbon* TypeDataRibbon = Cast<UParticleModuleTypeDataRibbon>(CurrentLODLevel->TypeDataModule);
	if (TypeDataRibbon)
	{
		// Update/postupdate
		TypeDataRibbon->Update(this, TypeDataOffset, DeltaTime);	
		TypeDataRibbon->PostUpdate(this, TypeDataOffset, DeltaTime);
	}
}

UBOOL FParticleRibbonEmitterInstance::GetSpawnPerUnitAmount(FLOAT DeltaTime, INT InTrailIdx, INT& OutCount, FLOAT& OutRate)
{
	check(CurrentSourcePosition.Num() > InTrailIdx);
	check(LastSourcePosition.Num() > InTrailIdx);

	if (SpawnPerUnitModule && SpawnPerUnitModule->bEnabled)
	{
		UBOOL bMoved = FALSE;
		FLOAT NewTravelLeftover = 0.0f;
		FLOAT ParticlesPerUnit = SpawnPerUnitModule->SpawnPerUnit.GetValue(EmitterTime, Component) / SpawnPerUnitModule->UnitScalar;
		// Allow for PPU of 0.0f to allow for 'turning off' an emitter when moving
		if (ParticlesPerUnit >= 0.0f)
		{
			FLOAT LeftoverTravel = SourceDistanceTraveled(InTrailIdx);
			// Calculate movement delta over last frame, include previous remaining delta
			FVector TravelDirection = CurrentSourcePosition(InTrailIdx) - LastSourcePosition(InTrailIdx);
			// Calculate distance traveled
			FLOAT TravelDistance = TravelDirection.Size();
			if (((SpawnPerUnitModule->MaxFrameDistance > 0.0f) && (TravelDistance > SpawnPerUnitModule->MaxFrameDistance)) ||
				(TravelDistance > HALF_WORLD_MAX))
			{
				// Clear it out!
				BYTE* InstData = GetModuleInstanceData(SpawnPerUnitModule);
				FParticleSpawnPerUnitInstancePayload* SPUPayload = NULL;
				SPUPayload = (FParticleSpawnPerUnitInstancePayload*)InstData;

				//@todo. Need to 'shift' the start point closer so we can still spawn...
				TravelDistance = 0.0f;
				SPUPayload->CurrentDistanceTravelled = 0.0f;
				LastSourcePosition(InTrailIdx) = CurrentSourcePosition(InTrailIdx);
			}

			// Check the change in tangent from last to this...
			FLOAT CheckTangent = 0.0f;
			if (TrailTypeData->TangentSpawningScalar > 0.0f)
			{
				FLOAT ElapsedTime = RunningTime;//SecondsSinceCreation;
				if (ActiveParticles == 0)
				{
					LastSourceTangent(InTrailIdx) = (CurrentSourcePosition(InTrailIdx) - LastSourcePosition(InTrailIdx)) / ElapsedTime;
				}
				FVector CurrTangent = TravelDirection / (ElapsedTime - TrailSpawnTimes(InTrailIdx));
				CurrTangent.Normalize();
				FVector PrevTangent = LastSourceTangent(InTrailIdx);
				PrevTangent.Normalize();
				CheckTangent = (CurrTangent | PrevTangent);
				// Map the tangent difference to [0..1] for [0..180]
				//  1.0 = parallel    --> -1 = 0
				//  0.0 = orthogonal  --> -1 = -1 --> * -0.5 = 0.5
				// -1.0 = oppositedir --> -1 = -2 --> * -0.5 = 1.0
				CheckTangent = (CheckTangent - 1.0f) * -0.5f;
			}

			if (TravelDistance > 0.0f)
			{
				if (TravelDistance > (SpawnPerUnitModule->MovementTolerance * SpawnPerUnitModule->UnitScalar))
				{
					bMoved = TRUE;
				}

				// Normalize direction for use later
				TravelDirection.Normalize();

				// Calculate number of particles to emit
				FLOAT NewLeftover = (TravelDistance + LeftoverTravel) * ParticlesPerUnit;

				NewLeftover += CheckTangent * TrailTypeData->TangentSpawningScalar;

				OutCount = (TrailTypeData->bSpawnInitialParticle && !ActiveParticles && (NewLeftover < 1.0f))? 1: appFloor(NewLeftover);
				if (OutCount < 0 || OutCount >= MAXINT)
				{
					//a licensee has found instances where we may picked up some bad data,
					//new support for max frame data should account for this, but just in case,
					//set our count to 0
					OutCount = 0;
				}

				OutRate = OutCount / DeltaTime;
				NewTravelLeftover = (TravelDistance + LeftoverTravel) - (OutCount * SpawnPerUnitModule->UnitScalar);
				SourceDistanceTraveled(InTrailIdx) = Max<FLOAT>(0.0f, NewTravelLeftover);
			}
			else
			{
				OutCount = 0;
				OutRate = 0.0f;
			}
		}
		else
		{
			OutCount = 0;
			OutRate = 0.0f;
		}

		if (SpawnPerUnitModule->bIgnoreSpawnRateWhenMoving == TRUE)
		{
			if (bMoved == TRUE)
			{
				return FALSE;
			}
			return TRUE;
		}
	}

	return SpawnPerUnitModule->bProcessSpawnRate;
}

inline UBOOL TrailsBase_AddParticleHelper(INT InTrailIdx,
	INT StartParticleIndex, FTrailsBaseTypeDataPayload* StartTrailData,
	INT ParticleIndex, FTrailsBaseTypeDataPayload* TrailData
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	, UParticleSystemComponent* InPsysComp
#endif
	)
{
	UBOOL bAddedParticle = FALSE;

	TrailData->TrailIndex = InTrailIdx;
	//@todo. Determine how to handle multiple trails...
	if (TRAIL_EMITTER_IS_ONLY(StartTrailData->Flags))
	{
		StartTrailData->Flags	= TRAIL_EMITTER_SET_END(StartTrailData->Flags);
		StartTrailData->Flags	= TRAIL_EMITTER_SET_NEXT(StartTrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
		StartTrailData->Flags	= TRAIL_EMITTER_SET_PREV(StartTrailData->Flags, ParticleIndex);

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		if (TrailData->SpawnTime < StartTrailData->SpawnTime)
		{
			debugf(TEXT("BAD SPAWN TIME! Curr %8.6f (%9s), Start %8.6f (%9s), %s (%s)"), 
				TrailData->SpawnTime, 
				TrailData->bMovementSpawned ? TEXT("MOVESPAWN") : TEXT("SPAWNRATE"),
				StartTrailData->SpawnTime,
				StartTrailData->bMovementSpawned ? TEXT("MOVESPAWN") : TEXT("SPAWNRATE"),
				InPsysComp ? 
					(InPsysComp->Template ? *(InPsysComp->Template->GetPathName()) : TEXT("*** No Template")) :
					TEXT("*** No Component"),
				InPsysComp ? *(InPsysComp->GetPathName()) : TEXT("*** No Components")
				);
		}
#endif

		// Now, 'join' them
		TrailData->Flags		= TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);
		TrailData->Flags		= TRAIL_EMITTER_SET_NEXT(TrailData->Flags, StartParticleIndex);
		TrailData->Flags		= TRAIL_EMITTER_SET_START(TrailData->Flags);

		bAddedParticle = TRUE;
	}
	else
	{
		// It better be the start!!!
		check(TRAIL_EMITTER_IS_START(StartTrailData->Flags));
		check(TRAIL_EMITTER_GET_NEXT(StartTrailData->Flags) != TRAIL_EMITTER_NULL_NEXT);

		StartTrailData->Flags	= TRAIL_EMITTER_SET_MIDDLE(StartTrailData->Flags);
		StartTrailData->Flags	= TRAIL_EMITTER_SET_PREV(StartTrailData->Flags, ParticleIndex);

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		if (TrailData->SpawnTime < StartTrailData->SpawnTime)
		{
			debugf(TEXT("BAD SPAWN TIME! Curr %8.6f (%9s), Start %8.6f (%9s), %s (%s)"), 
				TrailData->SpawnTime, 
				TrailData->bMovementSpawned ? TEXT("MOVESPAWN") : TEXT("SPAWNRATE"),
				StartTrailData->SpawnTime,
				StartTrailData->bMovementSpawned ? TEXT("MOVESPAWN") : TEXT("SPAWNRATE"),
				InPsysComp ? 
					(InPsysComp->Template ? *(InPsysComp->Template->GetPathName()) : TEXT("*** No Template")) :
					TEXT("*** No Component"),
				InPsysComp ? *(InPsysComp->GetPathName()) : TEXT("*** No Components"));
		}
#endif

		// Now, 'join' them
		TrailData->Flags		= TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);
		TrailData->Flags		= TRAIL_EMITTER_SET_NEXT(TrailData->Flags, StartParticleIndex);
		TrailData->Flags		= TRAIL_EMITTER_SET_START(TrailData->Flags);

		//SourceDistanceTravelled(TrailData->TrailIndex) += SourceDistanceTravelled(CheckTrailData->TrailIndex);

		bAddedParticle = TRUE;
	}

	return bAddedParticle;
}

/**
 *	Get the lifetime and size for a particle being added to the given trail
 *	
 *	@param	InTrailIdx				The index of the trail the particle is being added to
 *	@param	InParticle				The particle that is being added
 *	@param	bInNoLivingParticles	TRUE if there are no particles in the trail, FALSE if there already are
 *	@param	OutOneOverMaxLifetime	The OneOverMaxLifetime value to use for the particle
 *	@param	OutSize					The Size value to use for the particle
 */
void FParticleRibbonEmitterInstance::GetParticleLifetimeAndSize(INT InTrailIdx, const FBaseParticle* InParticle, UBOOL bInNoLivingParticles, FLOAT& OutOneOverMaxLifetime, FLOAT& OutSize)
{
	if (bInNoLivingParticles == TRUE)
	{
		UParticleLODLevel* LODLevel = SpriteTemplate->LODLevels(0);
		check(LODLevel);

		// Find the lifetime module
		FLOAT CurrLifetime = 0.0f;
		for (INT ModuleIdx = 0; ModuleIdx < LODLevel->SpawnModules.Num(); ModuleIdx++)
		{
			UParticleModuleLifetime* LifetimeModule = Cast<UParticleModuleLifetime>(LODLevel->SpawnModules(ModuleIdx));
			if (LifetimeModule != NULL)
			{
				FLOAT MaxLifetime = LifetimeModule->GetLifetimeValue(this, EmitterTime, Component);
				if (CurrLifetime > 0.f)
				{
					// Another module already modified lifetime.
					CurrLifetime = 1.f / (MaxLifetime + (1.f / CurrLifetime));
				}
				else
				{
					// First module to modify lifetime.
					CurrLifetime = (MaxLifetime > 0.f) ? (1.f / MaxLifetime) : 0.f;
				}
			}
		}
		if (CurrLifetime == 0.0f)
		{
			// We can't allow this...
			CurrLifetime = 1.0f;
		}

		if ((1.0f / CurrLifetime) < 0.001f)
		{
			CurrLifetime = 1.f / 0.001f;
		}

		CurrentLifetimes(InTrailIdx) = CurrLifetime;
		CurrentSizes(InTrailIdx) = InParticle->Size.X;
	}
	OutOneOverMaxLifetime = CurrentLifetimes(InTrailIdx);
	OutSize = CurrentSizes(InTrailIdx);
}

FLOAT FParticleRibbonEmitterInstance::Spawn(FLOAT DeltaTime)
{
	UBOOL bProcessSpawnRate = Spawn_Source(DeltaTime);
	if (bProcessSpawnRate == FALSE)
	{
		return SpawnFraction;
	}

//static INT TickCount = 0;
	UParticleLODLevel* LODLevel = SpriteTemplate->LODLevels(0);
	check(LODLevel);
	check(LODLevel->RequiredModule);

	// Iterate over each trail
//	for (INT TrailIdx = 0; TrailIdx < MaxTrailCount; TrailIdx++)
	INT TrailIdx = 0;

	FLOAT MovementSpawnRate = 0.0f;
	INT MovementSpawnCount = 0;
	FLOAT SpawnRate = 0.0f;
	INT SpawnCount = 0;
	INT BurstCount = 0;
	FLOAT OldLeftover = SpawnFraction;
	// For now, we are not supporting bursts on trails...
	UBOOL bProcessBurstList = FALSE;

	// Figure out spawn rate for this tick.
	if (bProcessSpawnRate)
	{
		FLOAT RateScale = LODLevel->SpawnModule->RateScale.GetValue(EmitterTime, Component);
		if (GSystemSettings.DetailMode != DM_High)
		{
			RateScale *= SpriteTemplate->MediumDetailSpawnRateScale;
		}
		SpawnRate += LODLevel->SpawnModule->Rate.GetValue(EmitterTime, Component) * Clamp<FLOAT>(RateScale, 0.0f, RateScale);
	}

	// Take Bursts into account as well...
	if (bProcessBurstList)
	{
		INT Burst = 0;
		FLOAT BurstTime = GetCurrentBurstRateOffset(DeltaTime, Burst);
		BurstCount += Burst;
	}

	const INT LocalMaxParticleInTrailCount = TrailTypeData->MaxParticleInTrailCount;
	FLOAT SafetyLeftover = OldLeftover;
	FLOAT NewLeftover = OldLeftover + DeltaTime * SpawnRate;
	INT SpawnNumber	= appFloor(NewLeftover);
	FLOAT SliceIncrement = (SpawnRate > 0.0f) ? (1.f / SpawnRate) : 0.0f;
	FLOAT SpawnStartTime = DeltaTime + OldLeftover * SliceIncrement - SliceIncrement;
	SpawnFraction = NewLeftover - SpawnNumber;
	// Do the resize stuff here!!!!!!!!!!!!!!!!!!!
	INT TotalCount = MovementSpawnCount + SpawnNumber + BurstCount;
	// Determine if no particles are alive
	UBOOL bNoLivingParticles = (ActiveParticles == 0);

	//@todo. Don't allow more than TrailCount trails...
	if (LocalMaxParticleInTrailCount > 0)
	{
		INT KillCount = (TotalCount + ActiveParticles) - LocalMaxParticleInTrailCount;
		if (KillCount > 0)
		{
			KillParticles(TrailIdx, KillCount);
		}

		// Don't allow the spawning of more particles than allowed...
		TotalCount = Max<INT>(TotalCount,LocalMaxParticleInTrailCount);
	}

	// Handle growing arrays.
	UBOOL bProcessSpawn = TRUE;
	INT NewCount = ActiveParticles + TotalCount;
	if (NewCount >= MaxActiveParticles)
	{
		if (DeltaTime < 0.25f)
		{
			bProcessSpawn = Resize(NewCount + appTrunc(appSqrt((FLOAT)NewCount)) + 1);
		}
		else
		{
			bProcessSpawn = Resize((NewCount + appTrunc(appSqrt((FLOAT)NewCount)) + 1), FALSE);
		}
	}

	if (bProcessSpawn == FALSE)
	{
		return SafetyLeftover;
	}

	//@todo. Support multiple trails per emitter
	// Find the start particle of the current trail...
	FBaseParticle* StartParticle = NULL;
	INT StartParticleIndex = -1;
	FRibbonTypeDataPayload* StartTrailData = NULL;
	for (INT FindTrailIdx = 0; FindTrailIdx < ActiveParticles; FindTrailIdx++)
	{
		INT CheckStartIndex = ParticleIndices[FindTrailIdx];
		DECLARE_PARTICLE_PTR(CheckParticle, ParticleData + ParticleStride * CheckStartIndex);
		FRibbonTypeDataPayload* CheckTrailData = ((FRibbonTypeDataPayload*)((BYTE*)CheckParticle + TypeDataOffset));
		if (CheckTrailData->TrailIndex == TrailIdx)
		{
			if (TRAIL_EMITTER_IS_START(CheckTrailData->Flags))
			{
				StartParticle = CheckParticle;
				StartParticleIndex = CheckStartIndex;
				StartTrailData = CheckTrailData;
				break;
			}
		}
	}

	bNoLivingParticles = (StartParticle == NULL);
	UBOOL bTilingTrail = !appIsNearlyZero(TrailTypeData->TilingDistance);

	FParticleEventInstancePayload* EventPayload = NULL;
	if (LODLevel->EventGenerator)
	{
		EventPayload = (FParticleEventInstancePayload*)GetModuleInstanceData(LODLevel->EventGenerator);
		if (EventPayload && (EventPayload->bSpawnEventsPresent == FALSE))
		{
			EventPayload = NULL;
		}
	}

	FLOAT ElapsedTime = RunningTime;//SecondsSinceCreation;
	// Do we have SpawnRate driven spawning?
	if ((SpawnRate > 0.0f) && (SpawnNumber > 0))
	{
		FLOAT Increment = (SpawnRate > 0.0f) ? (1.f / SpawnRate) : 0.0f;
		FLOAT StartTime = DeltaTime + OldLeftover * Increment - Increment;

		// Spawn particles.
		// NOTE: SpawnRate assumes that the ParticleSystemComponent is the 'source'
		FVector CurrentUp;
		if (TrailTypeData->RenderAxis == Trails_SourceUp)
		{
			CurrentUp = Component->LocalToWorld.TransformNormal(FVector(0,0,1));
		}
		else
		{
			CurrentUp = FVector(0.0f, 0.0f, 1.0f);
		}

		FLOAT InvCount = 1.0f / SpawnNumber;

		for (INT SpawnIdx = 0; SpawnIdx < SpawnNumber; SpawnIdx++)
		{
			check(ActiveParticles <= MaxActiveParticles);
			INT ParticleIndex = ParticleIndices[ActiveParticles];
			DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndex);
			FRibbonTypeDataPayload* TrailData = ((FRibbonTypeDataPayload*)((BYTE*)Particle + TypeDataOffset));

			FLOAT SpawnTime = StartTime - SpawnIdx * Increment;
			FLOAT TimeStep = Clamp<FLOAT>(InvCount * (SpawnIdx + 1), 0.0f, 1.0f);
			FLOAT StoredSpawnTime = DeltaTime * TimeStep;

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
					UParticleModule* OffsetModule	= LODLevel->SpawnModules(ModuleIndex);
					UINT* Offset = ModuleOffsetMap.Find(OffsetModule);
					SpawnModule->Spawn(this, Offset ? *Offset : 0, SpawnTime);
				}
			}
			PostSpawn(Particle, 1.f - FLOAT(SpawnIdx + 1) / FLOAT(SpawnNumber), SpawnTime);

			GetParticleLifetimeAndSize(TrailIdx, Particle, bNoLivingParticles, Particle->OneOverMaxLifetime, Particle->Size.X);
			Particle->RelativeTime = SpawnTime * Particle->OneOverMaxLifetime;
			Particle->Size.Y = Particle->Size.X;
			Particle->Size.Z = Particle->Size.Z;
			Particle->BaseSize = Particle->Size;

			if (EventPayload)
			{
				LODLevel->EventGenerator->HandleParticleSpawned(this, EventPayload, Particle);
			}

			// Trail specific...
			// Clear the next and previous - just to be safe
			TrailData->Flags = TRAIL_EMITTER_SET_NEXT(TrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
			TrailData->Flags = TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);
			// Set the trail-specific data on this particle
			TrailData->TrailIndex = TrailIdx;
			TrailData->Tangent = -Particle->Velocity * DeltaTime;
			TrailData->SpawnTime = ElapsedTime + StoredSpawnTime;
			TrailData->SpawnDelta = SpawnIdx * Increment;
			// Set the location and up vectors
			TrailData->Up = CurrentUp;

			TrailData->bMovementSpawned = FALSE;

			// If this is the true spawned particle, store off the spawn interpolated count
			TrailData->bInterpolatedSpawn = FALSE; 
			TrailData->SpawnedTessellationPoints = 1;

			UBOOL bAddedParticle = FALSE;
			// Determine which trail to attach to
			if (bNoLivingParticles)
			{
				// These are the first particles!
				// Tag it as the 'only'
				TrailData->Flags = TRAIL_EMITTER_SET_ONLY(TrailData->Flags);
				TiledUDistanceTraveled(TrailIdx) = 0.0f;
				TrailData->TiledU = 0.0f;
				bNoLivingParticles	= FALSE;
				bAddedParticle		= TRUE;
			}
			else if (StartParticle)
			{
				bAddedParticle = TrailsBase_AddParticleHelper(TrailIdx, 
					StartParticleIndex, (FTrailsBaseTypeDataPayload*)StartTrailData, 
					ParticleIndex, (FTrailsBaseTypeDataPayload*)TrailData
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
					, Component
#endif
					);
			}

			if (bAddedParticle)
			{
				if (bTilingTrail == TRUE)
				{
					if (StartParticle == NULL)
					{
						TrailData->TiledU = 0.0f;
					}
					else
					{
						FVector PositionDelta = Particle->Location - StartParticle->Location;
						TiledUDistanceTraveled(TrailIdx) += PositionDelta.Size();
						TrailData->TiledU = TiledUDistanceTraveled(TrailIdx) / TrailTypeData->TilingDistance;
						//@todo. Is there going to be a problem when distance gets REALLY high?
					}
				}

				StartParticle = Particle;
				StartParticleIndex = ParticleIndex;
				StartTrailData = TrailData;

				ActiveParticles++;

				if (StartTrailData->Tangent.IsNearlyZero())
				{
					FBaseParticle* NextSpawnedParticle = NULL;
					FRibbonTypeDataPayload* NextSpawnedTrailData = NULL;
					FTrailsBaseTypeDataPayload* TempPayload = NULL;
					GetParticleInTrail(TRUE, StartParticle, StartTrailData, GET_Next, GET_Spawned, NextSpawnedParticle, TempPayload);
					NextSpawnedTrailData = (FRibbonTypeDataPayload*)(TempPayload);
					if (NextSpawnedParticle != NULL)
					{
						FVector PositionDelta = (StartParticle->Location - NextSpawnedParticle->Location);
						FLOAT TimeDelta = StartTrailData->SpawnTime - NextSpawnedTrailData->SpawnTime;
						StartTrailData->Tangent = PositionDelta / TimeDelta;
					}
				}

#if !(FINAL_RELEASE || SHIPPING_PC_GAME)
				if ((INT)ActiveParticles > LocalMaxParticleInTrailCount)
				{
					AWorldInfo* WorldInfo = GWorld ? GWorld->GetWorldInfo() : NULL;
					if (WorldInfo)
					{
						FString ErrorMessage = 
							FString::Printf(TEXT("Ribbon with too many particles: %5d vs. %5d, %s"), 
								ActiveParticles, LocalMaxParticleInTrailCount,
								Component ? Component->Template ? *(Component->Template->GetName()) : TEXT("No template") : TEXT("No component"));
						FColor ErrorColor(255,0,0);
						WorldInfo->AddOnScreenDebugMessage((QWORD)((PTRINT)this), 5.0f, ErrorColor,ErrorMessage);
						debugf(*ErrorMessage);
					}
				}
#endif	//#if !FINAL_RELEASE
				INC_DWORD_STAT(STAT_TrailParticlesSpawned);

				if ((TrailTypeData->bEnablePreviousTangentRecalculation == TRUE)
					&& (TrailTypeData->bTangentRecalculationEveryFrame == FALSE))
				{
					// Find the 2 next SPAWNED particles in the trail (not interpolated).
					// If there are 2, then the one about to be spawned will make a chain of 3 
					// giving us data to better calculate the middle ones tangent.
					// After doing so, we must also recalculate the tangents of the interpolated
					// particles in the chain.

					// The most recent spawned particle in the trail...
					FBaseParticle* NextSpawnedParticle = NULL;
					FRibbonTypeDataPayload* NextSpawnedTrailData = NULL;
					// The second most recent spawned particle in the trail...
					FBaseParticle* NextNextSpawnedParticle = NULL;
					FRibbonTypeDataPayload* NextNextSpawnedTrailData = NULL;

					FTrailsBaseTypeDataPayload* TempPayload = NULL;

					// Grab the latest two spawned particles in the trail
					GetParticleInTrail(TRUE, StartParticle, StartTrailData, GET_Next, GET_Spawned, NextSpawnedParticle, TempPayload);
					NextSpawnedTrailData = (FRibbonTypeDataPayload*)(TempPayload);
					GetParticleInTrail(TRUE, NextSpawnedParticle, NextSpawnedTrailData, GET_Next, GET_Spawned, NextNextSpawnedParticle, TempPayload);
					NextNextSpawnedTrailData = (FRibbonTypeDataPayload*)(TempPayload);

					if (NextSpawnedParticle != NULL)
					{
						FVector NewTangent;
						if (NextNextSpawnedParticle != NULL)
						{
							// Calculate the new tangent from the previous and next position...
							// LastSourcePosition will be that position of the first particle that will be spawned this frame
							FVector PositionDelta = (StartParticle->Location - NextNextSpawnedParticle->Location);
							FLOAT TimeDelta = StartTrailData->SpawnTime - NextNextSpawnedTrailData->SpawnTime;
							NewTangent = PositionDelta / TimeDelta;
							NextSpawnedTrailData->Tangent = NewTangent;
						}
		 				else //if (NextNextSpawnedParticle == NULL)
		 				{
		 					// This is the second spawned particle in a trail...
		 					// Calculate the new tangent from the previous and next position...
		 					// LastSourcePosition will be that position of the first particle that will be spawned this frame
		 					FVector PositionDelta = (StartParticle->Location - NextSpawnedParticle->Location);
		 					FLOAT TimeDelta = StartTrailData->SpawnTime - NextSpawnedTrailData->SpawnTime;
		 					NewTangent = PositionDelta / TimeDelta;
		 					NextSpawnedTrailData->Tangent = NewTangent;
		 				}
					}
				}

				TrailSpawnTimes(0) = TrailData->SpawnTime;
			}
			else
			{
				check(TEXT("Failed to add particle to trail!!!!"));
			}

			INC_DWORD_STAT_BY(STAT_TrailParticles, ActiveParticles);
			INC_DWORD_STAT(STAT_SpriteParticlesSpawned);
		}
	}

//TickCount++;
	return SpawnFraction;
}

/**
 *	Spawn source-based ribbon particles.
 *
 *	@param	DeltaTime			The current time slice
 *
 *	@return	UBOOL				TRUE if SpawnRate should be processed.
 */
UBOOL FParticleRibbonEmitterInstance::Spawn_Source(FLOAT DeltaTime)
{
	UBOOL bProcessSpawnRate = TRUE;
	UParticleLODLevel* LODLevel = SpriteTemplate->LODLevels(0);
	check(LODLevel);
	check(LODLevel->RequiredModule);

	const INT LocalMaxParticleInTrailCount = TrailTypeData->MaxParticleInTrailCount;
	// Iterate over each trail
	for (INT TrailIdx = 0; TrailIdx < MaxTrailCount; TrailIdx++)
	{
		FLOAT MovementSpawnRate = 0.0f;
		INT MovementSpawnCount = 0;

		// Process the SpawnPerUnit, if present.
		if ((SpawnPerUnitModule != NULL) && (SpawnPerUnitModule->bEnabled == TRUE))
		{
			// We are hijacking the settings from this - not using it to calculate the value
			// Update the spawn rate
			INT Number = 0;
			FLOAT Rate = 0.0f;
			bProcessSpawnRate = GetSpawnPerUnitAmount(DeltaTime, TrailIdx, Number, Rate);
			MovementSpawnCount += Number;
			MovementSpawnRate += Rate;
		}

		// Do the resize stuff here!!!!!!!!!!!!!!!!!!!
		// Determine if no particles are alive
		UBOOL bNoLivingParticles = (ActiveParticles == 0);

		// Don't allow more than TrailCount trails...
		if (LocalMaxParticleInTrailCount > 0)
		{
			INT KillCount = (MovementSpawnCount + ActiveParticles) - LocalMaxParticleInTrailCount;
			if (KillCount > 0)
			{
				KillParticles(TrailIdx, KillCount);
			}

			if ((MovementSpawnCount + ActiveParticles) > LocalMaxParticleInTrailCount)
			{
				// We kill all the ones we could... so now we have to fall back to clamping
				MovementSpawnCount = LocalMaxParticleInTrailCount - ActiveParticles;
				if (MovementSpawnCount < 0)
				{
					MovementSpawnCount = 0;
				}
			}
		}

		// Handle growing arrays.
		UBOOL bProcessSpawn = TRUE;
		INT NewCount = ActiveParticles + MovementSpawnCount;
		if (NewCount >= MaxActiveParticles)
		{
			if (DeltaTime < 0.25f)
			{
				bProcessSpawn = Resize(NewCount + appTrunc(appSqrt((FLOAT)NewCount)) + 1);
			}
			else
			{
				bProcessSpawn = Resize((NewCount + appTrunc(appSqrt((FLOAT)NewCount)) + 1), FALSE);
			}
		}

		if (bProcessSpawn == FALSE)
		{
			continue;
		}

		//@todo. Support multiple trails per emitter
		// Find the start particle of the current trail...
		FBaseParticle* StartParticle = NULL;
		INT StartParticleIndex = -1;
		FRibbonTypeDataPayload* StartTrailData = NULL;
		for (INT FindTrailIdx = 0; FindTrailIdx < ActiveParticles; FindTrailIdx++)
		{
			INT CheckStartIndex = ParticleIndices[FindTrailIdx];
			DECLARE_PARTICLE_PTR(CheckParticle, ParticleData + ParticleStride * CheckStartIndex);
			FRibbonTypeDataPayload* CheckTrailData = ((FRibbonTypeDataPayload*)((BYTE*)CheckParticle + TypeDataOffset));
			if (CheckTrailData->TrailIndex == TrailIdx)
			{
				if (TRAIL_EMITTER_IS_START(CheckTrailData->Flags))
				{
					StartParticle = CheckParticle;
					StartParticleIndex = CheckStartIndex;
					StartTrailData = CheckTrailData;
					break;
				}
			}
		}

		// If we are particle sourced, and the source time is NEWER than the last source time,
		// then our source particle died... Mark the trial as dead.
		if ((TrailTypeData->bDeadTrailsOnSourceLoss == TRUE) && (LastSourceTimes(TrailIdx) > SourceTimes(TrailIdx)))
		{
			if (StartTrailData != NULL)
			{
				StartTrailData->Flags = TRAIL_EMITTER_SET_DEADTRAIL(StartTrailData->Flags);
			}
			StartParticle = NULL;
			StartParticleIndex = NULL;
			StartTrailData = NULL;
			LastSourcePosition(TrailIdx) = CurrentSourcePosition(TrailIdx);
			LastSourceRotation(TrailIdx) = CurrentSourceRotation(TrailIdx);
			LastSourceTangent(TrailIdx) = CurrentSourceTangent(TrailIdx);
			LastSourceUp(TrailIdx) = CurrentSourceUp(TrailIdx);
			LastSourceTimes(TrailIdx) = SourceTimes(TrailIdx);

			MovementSpawnCount = 0;

			// Force it to pick a new particle
			SourceIndices(TrailIdx) = -1;

			// skip to the next trail...
			continue;
		}

		bNoLivingParticles = (StartParticle == NULL);
		UBOOL bTilingTrail = !appIsNearlyZero(TrailTypeData->TilingDistance);

		FParticleEventInstancePayload* EventPayload = NULL;
		if (LODLevel->EventGenerator)
		{
			EventPayload = (FParticleEventInstancePayload*)GetModuleInstanceData(LODLevel->EventGenerator);
			if (EventPayload && (EventPayload->bSpawnEventsPresent == FALSE))
			{
				EventPayload = NULL;
			}
		}

		FLOAT ElapsedTime = RunningTime;//SecondsSinceCreation;

		// Do we have movement based spawning?
		// If so, then interpolate the position/tangent data between 
		// CurrentSource<Position/Tangent> and LastSource<Position/Tangent>
		if (MovementSpawnCount > 0)
		{
			if (SecondsSinceCreation < TrailSpawnTimes(TrailIdx))
			{
				// Fix up the starting source tangent
				LastSourceTangent(TrailIdx) = (CurrentSourcePosition(TrailIdx) - LastSourcePosition(TrailIdx)) / ElapsedTime;
			}

			if ((TrailTypeData->bEnablePreviousTangentRecalculation == TRUE)
				&& (TrailTypeData->bTangentRecalculationEveryFrame == FALSE))
			{
				// Find the 2 next SPAWNED particles in the trail (not interpolated).
				// If there are 2, then the one about to be spawned will make a chain of 3 
				// giving us data to better calculate the middle ones tangent.
				// After doing so, we must also recalculate the tangents of the interpolated
				// particles in the chain.

				// The most recent spawned particle in the trail...
				FBaseParticle* NextSpawnedParticle = NULL;
				FRibbonTypeDataPayload* NextSpawnedTrailData = NULL;
				// The second most recent spawned particle in the trail...
				FBaseParticle* NextNextSpawnedParticle = NULL;
				FRibbonTypeDataPayload* NextNextSpawnedTrailData = NULL;

				FTrailsBaseTypeDataPayload* TempPayload = NULL;

				// Grab the latest two spawned particles in the trail
				GetParticleInTrail(FALSE, StartParticle, StartTrailData, GET_Next, GET_Spawned, NextSpawnedParticle, TempPayload);
				NextSpawnedTrailData = (FRibbonTypeDataPayload*)(TempPayload);
				GetParticleInTrail(TRUE, NextSpawnedParticle, NextSpawnedTrailData, GET_Next, GET_Spawned, NextNextSpawnedParticle, TempPayload);
				NextNextSpawnedTrailData = (FRibbonTypeDataPayload*)(TempPayload);

				if ((NextSpawnedParticle != NULL) && (NextNextSpawnedParticle != NULL))
				{
					FVector NewTangent;
					if (NextNextSpawnedParticle != NULL)
					{
						// Calculate the new tangent from the previous and next position...
						// LastSourcePosition will be that position of the first particle that will be spawned this frame
						FVector PositionDelta = (CurrentSourcePosition(TrailIdx) - NextNextSpawnedParticle->Location);
						FLOAT TimeDelta = ElapsedTime - NextNextSpawnedTrailData->SpawnTime;
						NewTangent = PositionDelta / TimeDelta;
						// Calculate new tangents for all the interpolated particles between NextNext and Next
						if (NextSpawnedTrailData->SpawnedTessellationPoints > 0)
						{
							INT Prev = TRAIL_EMITTER_GET_PREV(NextNextSpawnedTrailData->Flags);
							check(Prev != TRAIL_EMITTER_NULL_PREV);
							DECLARE_PARTICLE_PTR(PrevParticle, ParticleData + ParticleStride * Prev);
							FBaseParticle* CurrentParticle = PrevParticle;
							FRibbonTypeDataPayload* CurrentTrailData = ((FRibbonTypeDataPayload*)((BYTE*)CurrentParticle + TypeDataOffset));

							// Fix up the next ones...
							FLOAT Diff = NextSpawnedTrailData->SpawnTime - NextNextSpawnedTrailData->SpawnTime;
							FVector CurrUp = FVector(0.0f, 0.0f, 1.0f);
							FLOAT InvCount = 1.0f / NextSpawnedTrailData->SpawnedTessellationPoints;
							// Spawn the given number of particles, interpolating between the current and last position/tangent
							//@todo. Recalculate the number of interpolated spawn particles???
							for (INT SpawnIdx = 0; SpawnIdx < NextSpawnedTrailData->SpawnedTessellationPoints; SpawnIdx++)
							{
								FLOAT TimeStep = InvCount * (SpawnIdx + 1);
								FVector CurrPosition = CubicInterp<FVector>(
									NextNextSpawnedParticle->Location, NextNextSpawnedTrailData->Tangent,
									NextSpawnedParticle->Location, NewTangent * Diff, 
									TimeStep);
								FVector CurrTangent = CubicInterpDerivative<FVector>(
									NextNextSpawnedParticle->Location, NextNextSpawnedTrailData->Tangent,
									NextSpawnedParticle->Location, NewTangent * Diff,
									TimeStep);

								// Trail specific...
								CurrentParticle->OldLocation = CurrentParticle->Location;
								CurrentParticle->Location = CurrPosition;
								CurrentTrailData->Tangent = CurrTangent * InvCount;

								// Get the next particle in the trail (previous)
								if ((SpawnIdx + 1) < NextSpawnedTrailData->SpawnedTessellationPoints)
								{
									INT Prev = TRAIL_EMITTER_GET_PREV(CurrentTrailData->Flags);
									check(Prev != TRAIL_EMITTER_NULL_PREV);
									DECLARE_PARTICLE_PTR(PrevParticle, ParticleData + ParticleStride * Prev);
									CurrentParticle = PrevParticle;
									CurrentTrailData = ((FRibbonTypeDataPayload*)((BYTE*)CurrentParticle + TypeDataOffset));
								}
							}
						}
					}

					// Set it for the new spawn interpolation
					LastSourceTangent(TrailIdx) = NewTangent;
				}
			}

			FLOAT LastTime = TrailSpawnTimes(TrailIdx);
			FLOAT Diff = ElapsedTime - LastTime;
			check(Diff >= 0.0f);
			FVector CurrUp = FVector(0.0f, 0.0f, 1.0f);
			FLOAT InvCount = 1.0f / MovementSpawnCount;
			FLOAT Increment = DeltaTime / MovementSpawnCount;

			FMatrix SavedLocalToWorld = Component->LocalToWorld;

			// Spawn the given number of particles, interpolating between the current and last position/tangent
			FLOAT CurrTimeStep = InvCount;
			for (INT SpawnIdx = 0; SpawnIdx < MovementSpawnCount; SpawnIdx++, CurrTimeStep += InvCount)
			{
				FLOAT TimeStep = Clamp<FLOAT>(CurrTimeStep, 0.0f, 1.0f);
				FVector CurrPosition = CubicInterp<FVector>(
					LastSourcePosition(TrailIdx), LastSourceTangent(TrailIdx) * Diff,
					CurrentSourcePosition(TrailIdx), CurrentSourceTangent(TrailIdx) * Diff,
					TimeStep);
				FQuat CurrRotation = SlerpQuat(LastSourceRotation(TrailIdx), CurrentSourceRotation(TrailIdx), TimeStep);
				FVector CurrTangent = CubicInterpDerivative<FVector>(
					LastSourcePosition(TrailIdx), LastSourceTangent(TrailIdx) * Diff,
					CurrentSourcePosition(TrailIdx), CurrentSourceTangent(TrailIdx) * Diff,
					TimeStep);
				if (TrailTypeData->RenderAxis == Trails_SourceUp)
				{
					// Only interpolate the Up if using the source Up
					CurrUp = Lerp<FVector>(LastSourceUp(TrailIdx), CurrentSourceUp(TrailIdx), TimeStep);
				}
				else if (TrailTypeData->RenderAxis == Trails_WorldUp)
				{
					CurrUp = FVector(0.0f, 0.0f, 1.0f);
				}

				//@todo. Need to interpolate colors here as well!!!!

				INT ParticleIndex = ParticleIndices[ActiveParticles];
				DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndex);
				FRibbonTypeDataPayload* TrailData = ((FRibbonTypeDataPayload*)((BYTE*)Particle + TypeDataOffset));

				// We are going from 'oldest' to 'newest' for this spawn, so reverse the time
				FLOAT StoredSpawnTime = Diff * (1.0f - TimeStep);
				FLOAT SpawnTime = DeltaTime - (SpawnIdx * Increment);
				FLOAT TrueSpawnTime = Diff * TimeStep;

				Component->LocalToWorld = FRotationTranslationMatrix(CurrRotation.Rotator(), CurrPosition);

				// Standard spawn setup
				PreSpawn(Particle);
				for (INT SpawnModuleIdx = 0; SpawnModuleIdx < LODLevel->SpawnModules.Num(); SpawnModuleIdx++)
				{
					UParticleModule* SpawnModule = LODLevel->SpawnModules(SpawnModuleIdx);
					if (!SpawnModule || !SpawnModule->bEnabled)
					{
						continue;
					}

					UINT* Offset = ModuleOffsetMap.Find(SpawnModule);
					SpawnModule->Spawn(this, Offset ? *Offset : 0, SpawnTime);
				}

				if (LODLevel->TypeDataModule)
				{
					//@todo. Need to track TypeData offset into payload!
					LODLevel->TypeDataModule->Spawn(this, TypeDataOffset, SpawnTime);
				}
				PostSpawn(Particle, 1.f - FLOAT(SpawnIdx + 1) / FLOAT(MovementSpawnCount), SpawnTime);

				GetParticleLifetimeAndSize(TrailIdx, Particle, bNoLivingParticles, Particle->OneOverMaxLifetime, Particle->Size.X);
				Particle->RelativeTime = SpawnTime * Particle->OneOverMaxLifetime;
				Particle->Size.Y = Particle->Size.X;
				Particle->Size.Z = Particle->Size.Z;
				Particle->BaseSize = Particle->Size;

				Component->LocalToWorld = SavedLocalToWorld;

				// Trail specific...
				// Clear the next and previous - just to be safe
				TrailData->Flags = TRAIL_EMITTER_SET_NEXT(TrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
				TrailData->Flags = TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);
				// Set the trail-specific data on this particle
				TrailData->TrailIndex = TrailIdx;
				TrailData->Tangent = CurrTangent * InvCount;
				TrailData->SpawnTime = ElapsedTime - StoredSpawnTime;
				TrailData->SpawnDelta = TrueSpawnTime;
				// Set the location and up vectors
				//@todo. Need to add support for offsetting from the source...
				Particle->Location = CurrPosition;
				Particle->OldLocation = CurrPosition;

				TrailData->Up = CurrUp;

				TrailData->bMovementSpawned = TRUE;

				if (SpawnIdx == (MovementSpawnCount-1))
				{
					// If this is the true spawned particle, store off the spawn interpolated count
					TrailData->bInterpolatedSpawn = FALSE; 
					TrailData->SpawnedTessellationPoints = MovementSpawnCount;
				}
				else
				{
					TrailData->bInterpolatedSpawn = TRUE; 
					TrailData->SpawnedTessellationPoints = 1;
				}
				TrailData->SpawnedTessellationPoints = MovementSpawnCount;

				UBOOL bAddedParticle = FALSE;
				// Determine which trail to attach to
				if (bNoLivingParticles)
				{
					// These are the first particles!
					// Tag it as the 'only'
					TrailData->Flags = TRAIL_EMITTER_SET_ONLY(TrailData->Flags);
					TiledUDistanceTraveled(TrailIdx) = 0.0f;
					TrailData->TiledU = 0.0f;
					bNoLivingParticles	= FALSE;
					bAddedParticle		= TRUE;
				}
				else if (StartParticle)
				{
					bAddedParticle = TrailsBase_AddParticleHelper(TrailIdx,
						StartParticleIndex, (FTrailsBaseTypeDataPayload*)StartTrailData, 
						ParticleIndex, (FTrailsBaseTypeDataPayload*)TrailData
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
						, Component
#endif
						);
				}

				if (bAddedParticle)
				{
					if (bTilingTrail == TRUE)
					{
						if (StartParticle == NULL)
						{
							TrailData->TiledU = 0.0f;
						}
						else
						{
							FVector PositionDelta = Particle->Location - StartParticle->Location;
							TiledUDistanceTraveled(TrailIdx) += PositionDelta.Size();
							TrailData->TiledU = TiledUDistanceTraveled(TrailIdx) / TrailTypeData->TilingDistance;
							//@todo. Is there going to be a problem when distance gets REALLY high?
						}
					}

					StartParticle = Particle;
					StartParticleIndex = ParticleIndex;
					StartTrailData = TrailData;

					ActiveParticles++;
					//				check((INT)ActiveParticles < TrailTypeData->MaxParticleInTrailCount);
#if !(FINAL_RELEASE || SHIPPING_PC_GAME)
					if ((INT)ActiveParticles > LocalMaxParticleInTrailCount)
					{
						AWorldInfo* WorldInfo = GWorld ? GWorld->GetWorldInfo() : NULL;
						if (WorldInfo)
						{
							FString ErrorMessage = 
								FString::Printf(TEXT("Ribbon with too many particles: %5d vs. %5d, %s"), 
								ActiveParticles, LocalMaxParticleInTrailCount,
								Component ? Component->Template ? *(Component->Template->GetName()) : TEXT("No template") : TEXT("No component"));
							FColor ErrorColor(255,0,0);
							WorldInfo->AddOnScreenDebugMessage((QWORD)((PTRINT)this), 5.0f, ErrorColor,ErrorMessage);
							debugf(*ErrorMessage);
						}
					}
#endif	//#if !FINAL_RELEASE
					INC_DWORD_STAT(STAT_TrailParticlesSpawned);
					//					LastEmittedParticleIndex = ParticleIndex;
				}
				else
				{
					check(TEXT("Failed to add particle to trail!!!!"));
				}

				INC_DWORD_STAT_BY(STAT_TrailParticles, ActiveParticles);
			}

			// Update the last position
			LastSourcePosition(TrailIdx) = CurrentSourcePosition(TrailIdx);
			LastSourceRotation(TrailIdx) = CurrentSourceRotation(TrailIdx);
			LastSourceTangent(TrailIdx) = CurrentSourceTangent(TrailIdx);
			LastSourceUp(TrailIdx) = CurrentSourceUp(TrailIdx);
			TrailSpawnTimes(TrailIdx) = ElapsedTime;
			LastSourceTimes(TrailIdx) = SourceTimes(TrailIdx);
			if ((SourceModule != NULL) && (SourceModule->SourceMethod == PET2SRCM_Particle))
			{
				if (SourceTimes(TrailIdx) > 1.0f)
				{
					StartTrailData->Flags = TRAIL_EMITTER_SET_DEADTRAIL(StartTrailData->Flags);
					SourceIndices(TrailIdx) = -1;
				}
			}
		}
	}

	return bProcessSpawnRate;
}

/**
 *	Spawn ribbon particles from SpawnRate and Burst settings.
 *
 *	@param	DeltaimTime			The current time slice
 *	
 *	@return	FLOAT				The spawnfraction left over from this time slice
 */
FLOAT FParticleRibbonEmitterInstance::Spawn_RateAndBurst(FLOAT DeltaTime)
{
	return SpawnFraction;
}

void FParticleRibbonEmitterInstance::SetupTrailModules()
{
	// Trails are a special case... 
	// We don't want standard Spawn/Update calls occuring on Trail-type modules.
	UParticleLODLevel* LODLevel = SpriteTemplate->GetLODLevel(0);
	check(LODLevel);
	for (INT ModuleIdx = 0; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
	{
		UBOOL bRemoveIt = FALSE;
		UParticleModule* CheckModule = LODLevel->Modules(ModuleIdx);
		UParticleModuleSpawnPerUnit* CheckSPUModule = Cast<UParticleModuleSpawnPerUnit>(CheckModule);
		UParticleModuleTrailSource*	CheckSourceModule = Cast<UParticleModuleTrailSource>(CheckModule);

		if (CheckSPUModule != NULL)
		{
			SpawnPerUnitModule = CheckSPUModule;
			bRemoveIt = TRUE;
		}
		else if (CheckSourceModule != NULL)
		{
			SourceModule = CheckSourceModule;
			UINT* Offset = ModuleOffsetMap.Find(CheckSourceModule);
			if (Offset != NULL)
			{
				TrailModule_Source_Offset = *Offset;
			}
			bRemoveIt = TRUE;
		}

		if (bRemoveIt == TRUE)
		{
			// Remove it from any lists...
			for (INT UpdateIdx = LODLevel->UpdateModules.Num() - 1; UpdateIdx >= 0; UpdateIdx--)
			{
				if (LODLevel->UpdateModules(UpdateIdx) == CheckModule)
				{
					LODLevel->UpdateModules.Remove(UpdateIdx);
				}
			}

			for (INT SpawnIdx = LODLevel->SpawnModules.Num() - 1; SpawnIdx >= 0; SpawnIdx--)
			{
				if (LODLevel->SpawnModules(SpawnIdx) == CheckModule)
				{
					LODLevel->SpawnModules.Remove(SpawnIdx);
				}
			}

			for (INT SpawningIdx = LODLevel->SpawningModules.Num() - 1; SpawningIdx >= 0; SpawningIdx--)
			{
				if (LODLevel->SpawningModules(SpawningIdx) == CheckModule)
				{
					LODLevel->SpawningModules.Remove(SpawningIdx);
				}
			}
		}
	}
}

void FParticleRibbonEmitterInstance::ResolveSource()
{
	if (SourceModule && SourceModule->SourceName != NAME_None)
	{
		switch (SourceModule->SourceMethod)
		{
		case PET2SRCM_Actor:
			if (SourceActor == NULL)
			{
				FParticleSysParam Param;
				for (INT ParamIdx = 0; ParamIdx < Component->InstanceParameters.Num(); ParamIdx++)
				{
					Param = Component->InstanceParameters(ParamIdx);
					if (Param.Name == SourceModule->SourceName)
					{
						SourceActor = Param.Actor;
						break;
					}
				}

				if (SourceModule->SourceOffsetCount > 0)
				{
					for (INT ParamIdx = 0; ParamIdx < Component->InstanceParameters.Num(); ParamIdx++)
					{
						Param = Component->InstanceParameters(ParamIdx);
						FString ParamName = Param.Name.ToString();
						TCHAR* TrailSourceOffset = appStrstr(*ParamName, TEXT("TrailSourceOffset"));
						if (TrailSourceOffset)
						{
							// Parse off the digit
							INT	Index	= appAtoi(TrailSourceOffset);
							if (Index >= 0)
							{
								if (Param.ParamType	== PSPT_Vector)
								{
									SourceOffsets.Insert(Index);
									SourceOffsets(Index) = Param.Vector;
								}
								else if (Param.ParamType == PSPT_Scalar)
								{
									SourceOffsets.InsertZeroed(Index);
									SourceOffsets(Index) = FVector(Param.Scalar, 0.0f, 0.0f);
								}
							}
						}
					}
				}
			}
			break;
		case PET2SRCM_Particle:
			if (SourceEmitter == NULL)
			{
				for (INT EmitterIdx = 0; EmitterIdx < Component->EmitterInstances.Num(); EmitterIdx++)
				{
					FParticleEmitterInstance* EmitInst = Component->EmitterInstances(EmitterIdx);
					if (EmitInst && (EmitInst->SpriteTemplate->EmitterName == SourceModule->SourceName))
					{
						SourceEmitter = EmitInst;
						break;
					}
				}
			}
			break;
		}
	}
}

void FParticleRibbonEmitterInstance::UpdateSourceData(FLOAT DeltaTime, UBOOL bFirstTime)
{
	FVector	Position;
	FQuat Rotation;
	FVector	Tangent;
	FVector	Up;
	FLOAT TangentStrength;
	// For each possible trail in this emitter, update it's source information
	FLOAT ElapsedTime = RunningTime;//SecondsSinceCreation;
	for (INT TrailIdx = 0; TrailIdx < MaxTrailCount; TrailIdx++)
	{
		UBOOL bNewSource = (SourceIndices(TrailIdx) == -1);
		if (ResolveSourcePoint(TrailIdx, Position, Rotation, Up, Tangent, TangentStrength) == TRUE)
		{
			if ((bFirstTime == TRUE) || 
				((bNewSource == TRUE) && ((SourceModule != NULL) && (SourceModule->SourceMethod == PET2SRCM_Particle))))
			{
				LastSourcePosition(TrailIdx) = Position;
				LastSourceTangent(TrailIdx) = FVector(0,0,0);//Component->LocalToWorld.TransformNormal(FVector(1,0,0));
				LastSourceTangentStrength(TrailIdx) = TangentStrength;
				LastSourceUp(TrailIdx) = Up;
				TrailSpawnTimes(TrailIdx) = RunningTime;
			}
			CurrentSourcePosition(TrailIdx) = Position;
			CurrentSourceRotation(TrailIdx) = Rotation;
			CurrentSourceTangent(TrailIdx) = (CurrentSourcePosition(TrailIdx) - LastSourcePosition(TrailIdx)) / (ElapsedTime - TrailSpawnTimes(TrailIdx));
			CurrentSourceTangentStrength(TrailIdx) = TangentStrength;
			CurrentSourceUp(TrailIdx) = Up;
			if (bFirstTime == TRUE)
			{
				LastSourceRotation(TrailIdx) = CurrentSourceRotation(TrailIdx);
			}
		}
	}
}

/**
 *	Resolve the source point for the given trail index.
 *
 *	@param	InTrailIdx			The index of the trail to resolve
 *	@param	OutPosition			The position of the source
 *	@param	OutRotation			The rotation of the source
 *	@param	OutUp				The 'up' of the source (if required)
 *	@param	OutTangent			The tangent of the source
 *	@param	OutTangentStrength	The strength of the tangent of the source
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL FParticleRibbonEmitterInstance::ResolveSourcePoint(INT InTrailIdx, 
	FVector& OutPosition, FQuat& OutRotation, FVector& OutUp, FVector& OutTangent, FLOAT& OutTangentStrength)
{
	UBOOL bSourceWasSet = FALSE;
	// Resolve the source point...
	if (SourceModule)
	{
		switch (SourceModule->SourceMethod)
		{
		case PET2SRCM_Particle:
			{
				if (SourceEmitter == NULL)
				{
					// Is this the first time?
					ResolveSource();
				}

				if (SourceEmitter)
				{
					if (SourceIndices(InTrailIdx) != -1)
					{
						FBaseParticle* SourceParticle = SourceEmitter->GetParticleDirect(SourceIndices(InTrailIdx));
						if (SourceParticle == NULL)
						{
							// If the previous particle is not found, force the trail to pick a new one
							SourceIndices(InTrailIdx) = -1;
						}
					}

					if (SourceIndices(InTrailIdx) == -1)
					{
						INT Index = 0;
						switch (SourceModule->SelectionMethod)
						{
						case EPSSM_Random:
							{
								Index = appTrunc(appFrand() * SourceEmitter->ActiveParticles);
							}
							break;
						case EPSSM_Sequential:
							{
								UBOOL bDone = FALSE;

								INT CheckSelIndex = ++LastSelectedParticleIndex;
								if (CheckSelIndex >= SourceEmitter->ActiveParticles)
								{
									CheckSelIndex = 0;
								}
								INT StartIdx = CheckSelIndex;
								
								Index = -1;
								while (!bDone)
								{
									INT CheckIndex = SourceEmitter->GetParticleDirectIndex(CheckSelIndex);
									if (CheckIndex == -1)
									{
										bDone = TRUE;
									}
									else
									{
										UBOOL bFound = FALSE;
										for (INT TrailCheckIdx = 0; TrailCheckIdx < MaxTrailCount; TrailCheckIdx++)
										{
											if (TrailCheckIdx != InTrailIdx)
											{
												if (SourceIndices(TrailCheckIdx) == CheckIndex)
												{
													bFound = TRUE;
												}
											}
										}

										if (bFound == FALSE)
										{
											bDone = TRUE;
											Index = CheckIndex;
										}
										else
										{
											CheckSelIndex++;
											if (CheckSelIndex >= SourceEmitter->ActiveParticles)
											{
												CheckSelIndex = 0;
											}
										}
									}

									if (CheckSelIndex == StartIdx)
									{
										bDone = TRUE;
									}
								}

								if (Index != -1)
								{
									LastSelectedParticleIndex = CheckSelIndex;
								}
							}
							break;
						}

						SourceIndices(InTrailIdx) = Index;
					}

					// Grab the particle
					FBaseParticle* SourceParticle = (SourceIndices(InTrailIdx) != -1) ? SourceEmitter->GetParticleDirect(SourceIndices(InTrailIdx)) : NULL;
					if (SourceParticle != NULL)
					{
						OutPosition = SourceParticle->Location;
						OutTangent = SourceParticle->Location - SourceParticle->OldLocation;
						SourceTimes(InTrailIdx) = SourceParticle->RelativeTime;
					}
					else
					{
						// Fall back to the emitter location??
						OutPosition = SourceEmitter->Component->LocalToWorld.GetOrigin();
						OutTangent = Component->PartSysVelocity;
						//@todo. How to handle this... can potentially cause a jump from the emitter to the
						// particle...
						SourceTimes(InTrailIdx) = 0.0f;
					}
					OutTangentStrength = OutTangent.SizeSquared();
					//@todo. Allow particle rotation to define up??
					OutUp = SourceEmitter->Component->LocalToWorld.TransformNormal(FVector(0,0,1));

					//@todo. Where to get rotation from????
					OutRotation = FQuat(0,0,0,1);

					//@todo. Support source offset

					bSourceWasSet = TRUE;
				}
			}
			break;
		case PET2SRCM_Actor:
			if (SourceModule->SourceName != NAME_None)
			{
				if (SourceActor == NULL)
				{
					ResolveSource();
				}

				if (SourceActor)
				{
					OutPosition = SourceActor->LocalToWorld().GetOrigin();
					FRotator TempRotator = SourceActor->LocalToWorld().Rotator();
					OutRotation = FQuat(TempRotator);
					OutTangent = SourceActor->Velocity;
					OutTangentStrength = OutTangent.SizeSquared();
					OutUp = SourceActor->LocalToWorld().TransformNormal(FVector(0,0,1));

					bSourceWasSet = TRUE;
				}
			}
			break;
		}
	}

	if (bSourceWasSet == FALSE)
	{
		OutPosition = Component->LocalToWorld.GetOrigin();
		if (SourceModule && (SourceModule->SourceOffsetCount > 0))
		{
			FVector SourceOffsetValue;
			if (SourceModule->ResolveSourceOffset(InTrailIdx, this, SourceOffsetValue) == TRUE)
			{
				if (CurrentLODLevel && (CurrentLODLevel->RequiredModule->bUseLocalSpace == FALSE))
				{
					// Transform it
					SourceOffsetValue = Component->LocalToWorld.TransformNormal(SourceOffsetValue);
				}
				OutPosition += SourceOffsetValue;
			}
		}
		FRotator TempRotator = Component->LocalToWorld.Rotator();
		OutRotation = FQuat(TempRotator);
		OutTangent = Component->PartSysVelocity;
		OutTangentStrength = OutTangent.SizeSquared();
		OutUp = Component->LocalToWorld.TransformNormal(FVector(0,0,1));

		bSourceWasSet = TRUE;
	}

	return bSourceWasSet;
}

/** Determine the number of vertices and triangles in each trail */
void FParticleRibbonEmitterInstance::DetermineVertexAndTriangleCount()
{
	UINT NewSize = 0;
	INT Sheets = 1;//TrailTypeData->Sheets ? TrailTypeData->Sheets : 1;
	INT TheTrailCount = 0;
	INT IndexCount = 0;

	VertexCount		= 0;
	TriangleCount	= 0;
	HeadOnlyParticles = 0;

	INT CheckParticleCount = 0;

	INT TempVertexCount;
	UBOOL bApplyDistanceTessellation = !appIsNearlyZero(TrailTypeData->DistanceTessellationStepSize);
	FLOAT DistTessStep = TrailTypeData->DistanceTessellationStepSize;
	const FLOAT ScaleStepFactor = 0.5f;
	UBOOL bScaleTessellation = TrailTypeData->bEnableTangentDiffInterpScale;

	FLOAT DistDiff = 0.0f;
	FLOAT CheckTangent = 0.0f;
	UBOOL bCheckTangentValue = !appIsNearlyZero(TrailTypeData->TangentTessellationScalar) || bScaleTessellation;
	// 
	for (INT ii = 0; ii < ActiveParticles; ii++)
	{
		INT LocalIndexCount = 0;
		INT	ParticleCount = 0;
		INT LocalVertexCount = 0;
		INT LocalTriCount = 0;

		UBOOL bProcessParticle = FALSE;

		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[ii]);
		FBaseParticle* CurrParticle = Particle;
		FRibbonTypeDataPayload*	CurrTrailData = ((FRibbonTypeDataPayload*)((BYTE*)Particle + TypeDataOffset));
		if (TRAIL_EMITTER_IS_HEADONLY(CurrTrailData->Flags))
		{
			// If there is only a single particle in the trail, then we only want to render it
			// if we are connecting to the source...
			//@todo. Support clip source segment
			CurrTrailData->RenderingInterpCount = 0;
			CurrTrailData->TriangleCount = 0;
			++HeadOnlyParticles;
		}
		else if (TRAIL_EMITTER_IS_END(CurrTrailData->Flags))
		{
			// Walk from the end of the trail to the front
			FBaseParticle* PrevParticle = NULL;
			FRibbonTypeDataPayload*	PrevTrailData = NULL;
			INT	Prev = TRAIL_EMITTER_GET_PREV(CurrTrailData->Flags);
			if (Prev != TRAIL_EMITTER_NULL_PREV)
			{
				DECLARE_PARTICLE_PTR(InnerParticle, ParticleData + ParticleStride * Prev);
				PrevParticle = InnerParticle;
				PrevTrailData = ((FRibbonTypeDataPayload*)((BYTE*)InnerParticle + TypeDataOffset));

				UBOOL bDone = FALSE;
				// The end of the trail, so there MUST be another particle
				while (!bDone)
				{
					ParticleCount++;
					// Determine the number of rendered interpolated points between these two particles
					FLOAT CheckDistance = (CurrParticle->Location - PrevParticle->Location).Size();
					FVector SrcTangent = CurrTrailData->Tangent;
					SrcTangent.Normalize();
					FVector PrevTangent = PrevTrailData->Tangent;
					PrevTangent.Normalize();
					if (bCheckTangentValue == TRUE)
					{
						CheckTangent = (SrcTangent | PrevTangent);
						// Map the tangent difference to [0..1] for [0..180]
						//  1.0 = parallel    --> -1 = 0
						//  0.0 = orthogonal  --> -1 = -1 --> * -0.5 = 0.5
						// -1.0 = oppositedir --> -1 = -2 --> * -0.5 = 1.0
						CheckTangent = (CheckTangent - 1.0f) * -0.5f;
					}

					if (bApplyDistanceTessellation == TRUE)
					{
						DistDiff = CheckDistance / DistTessStep;
						if (bScaleTessellation && (CheckTangent < ScaleStepFactor))
						{
							// Scale the tessellation step size so that 
							// parallel .. orthogonal maps to [0..1] (and > 90 also == 1.0)
							DistDiff *= (2.0f * Clamp<FLOAT>(CheckTangent, 0.0f, 0.5f));
						}
					}

					//@todo. Need to adjust the tangent diff count when the distance is REALLY small...
					FLOAT TangDiff = CheckTangent * TrailTypeData->TangentTessellationScalar;
					INT InterpCount = appTrunc(DistDiff) + appTrunc(TangDiff);

					// There always is at least 1 point (the source particle itself)
					InterpCount = (InterpCount > 0) ? InterpCount : 1;

					// Store off the rendering interp count for this particle
					CurrTrailData->RenderingInterpCount = InterpCount;
					if (CheckTangent <= 0.5f)
					{
						CurrTrailData->PinchScaleFactor = 1.0f;
					}
					else
					{
						CurrTrailData->PinchScaleFactor = 1.0f - (CheckTangent * 0.5f);
					}

					// Tally up the vertex and index counts for this segment...
					TempVertexCount = 2 * InterpCount * Sheets;
					VertexCount += TempVertexCount;
					LocalVertexCount += TempVertexCount;
					LocalIndexCount += TempVertexCount;

					// Move to the previous particle in the chain
					CurrParticle = PrevParticle;
					CurrTrailData = PrevTrailData;
					Prev = TRAIL_EMITTER_GET_PREV(CurrTrailData->Flags);
					if (Prev != TRAIL_EMITTER_NULL_PREV)
					{
						DECLARE_PARTICLE_PTR(InnerParticle, ParticleData + ParticleStride * Prev);
						PrevParticle = InnerParticle;
						PrevTrailData = ((FRibbonTypeDataPayload*)((BYTE*)InnerParticle + TypeDataOffset));
					}
					else
					{
						// The START particle will have a previous index of NULL, so we're done
						bDone = TRUE;
					}
				}

				bProcessParticle = TRUE;
			}
			else
			{
				// This means there is only a single particle in the trail - the end...
// 				check(!TEXT("FAIL"));
				bProcessParticle = FALSE;
			}
		}

		if (bProcessParticle == TRUE)
		{
			// The last step is the last interpolated step to the Curr (which should be the start)
			ParticleCount++;
			TempVertexCount = 2 * Sheets;
			VertexCount += TempVertexCount;
			LocalVertexCount += TempVertexCount;
			LocalIndexCount += TempVertexCount;

			// If we are running up to the current source, take that into account as well.
			//@todo. Support clip source segment

			// Handle degenerates - 4 tris per stitch
			LocalIndexCount	+= ((Sheets - 1) * 4);

			// @todo: We're going and modifying the original ParticleData here!  This is kind of sketchy
			//    since it's not supposed to be changed at this phase
			check(TRAIL_EMITTER_IS_HEAD(CurrTrailData->Flags));
			CurrTrailData->TriangleCount = LocalIndexCount - 2;

			// The last particle in the chain will always have 1 here!
			CurrTrailData->RenderingInterpCount = 1;

			// Increment the total index count
			IndexCount += LocalIndexCount;
			TheTrailCount++;
		}
	}

	TrailCount = TheTrailCount;
	if (TheTrailCount > 0)
	{
		IndexCount += 4 * (TheTrailCount - 1);	// 4 extra indices per Trail (degenerates)
		TriangleCount = IndexCount - (2 * TheTrailCount);
	}
	else
	{
		IndexCount = 0;
		TriangleCount = 0;
	}
}

/**
 *	Checks some common values for GetDynamicData validity
 *
 *	@return	UBOOL		TRUE if GetDynamicData should continue, FALSE if it should return NULL
 */
UBOOL FParticleRibbonEmitterInstance::IsDynamicDataRequired(UParticleLODLevel* CurrentLODLevel)
{
	if (FParticleEmitterInstance::IsDynamicDataRequired(CurrentLODLevel) == TRUE)
	{
		if (/*(TrailTypeData->bClipSourceSegement == TRUE) &&*/ (ActiveParticles < 2))
		{
			return FALSE;
		}
	}
	return TRUE;
}

/**
 *	Retrieves the dynamic data for the emitter
 */
FDynamicEmitterDataBase* FParticleRibbonEmitterInstance::GetDynamicData(UBOOL bSelected)
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetLODLevel(0);
	if (IsDynamicDataRequired(LODLevel) == FALSE)
	{
		return NULL;
	}

	// Allocate the dynamic data
	FDynamicRibbonEmitterData* NewEmitterData = ::new FDynamicRibbonEmitterData(LODLevel->RequiredModule);
	{
		SCOPE_CYCLE_COUNTER(STAT_ParticleMemTime);
		INC_DWORD_STAT(STAT_DynamicEmitterCount);
		INC_DWORD_STAT(STAT_DynamicRibbonCount);
		INC_DWORD_STAT_BY(STAT_DynamicEmitterMem, sizeof(FDynamicRibbonEmitterData));
	}

	NewEmitterData->bClipSourceSegement = TrailTypeData->bClipSourceSegement;
	NewEmitterData->bRenderGeometry = TrailTypeData->bRenderGeometry;
	NewEmitterData->bRenderParticles = TrailTypeData->bRenderSpawnPoints;
	NewEmitterData->bRenderTangents = TrailTypeData->bRenderTangents;
	NewEmitterData->bRenderTessellation = TrailTypeData->bRenderTessellation;
	NewEmitterData->DistanceTessellationStepSize = TrailTypeData->DistanceTessellationStepSize;
	NewEmitterData->TangentTessellationScalar = TrailTypeData->TangentTessellationScalar;
	NewEmitterData->RenderAxisOption = TrailTypeData->RenderAxis;
	NewEmitterData->TextureTileDistance = TrailTypeData->TilingDistance;
	if (NewEmitterData->TextureTileDistance > 0.0f)
	{
		NewEmitterData->bTextureTileDistance = TRUE;
	}
	else
	{
		NewEmitterData->bTextureTileDistance = FALSE;
	}

	// Now fill in the source data
	if (!FillReplayData(NewEmitterData->Source))
	{
		delete NewEmitterData;
		return NULL;
	}

	// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
	NewEmitterData->Init(bSelected);

	return NewEmitterData;
}

/**
 *	Updates the dynamic data for the instance
 *
 *	@param	DynamicData		The dynamic data to fill in
 *	@param	bSelected		TRUE if the particle system component is selected
 */
UBOOL FParticleRibbonEmitterInstance::UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected)
{
	if (ActiveParticles <= 0)
	{
		return FALSE;
	}

	if (DynamicData->GetSource().eEmitterType != DET_Ribbon)
	{
		warnf(TEXT("UpdateDynamicData> NOT A TRAILS EMITTER!"));
		return FALSE;
	}

	FDynamicRibbonEmitterData* TrailDynamicData = (FDynamicRibbonEmitterData*)DynamicData;
	// Now fill in the source data
	if (!FillReplayData(TrailDynamicData->Source))
	{
		return FALSE;
	}

	TrailDynamicData->bRenderGeometry = TrailTypeData->bRenderGeometry;
	TrailDynamicData->bRenderParticles = TrailTypeData->bRenderSpawnPoints;
	TrailDynamicData->bRenderTangents = TrailTypeData->bRenderTangents;
	TrailDynamicData->bRenderTessellation = TrailTypeData->bRenderTessellation;
	TrailDynamicData->DistanceTessellationStepSize = TrailTypeData->DistanceTessellationStepSize;
	TrailDynamicData->TangentTessellationScalar = TrailTypeData->TangentTessellationScalar;

	// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
	TrailDynamicData->Init(bSelected);

	return TRUE;
}

/**
 *	Retrieves replay data for the emitter
 *
 *	@return	The replay data, or NULL on failure
 */
FDynamicEmitterReplayDataBase* FParticleRibbonEmitterInstance::GetReplayData()
{
	if (ActiveParticles <= 0)
	{
		return NULL;
	}

	FDynamicEmitterReplayDataBase* NewEmitterReplayData = ::new FDynamicRibbonEmitterReplayData();
	check(NewEmitterReplayData != NULL);

	if (!FillReplayData(*NewEmitterReplayData))
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
void FParticleRibbonEmitterInstance::GetAllocatedSize(INT& OutNum, INT& OutMax)
{
	INT Size = sizeof(FParticleRibbonEmitterInstance);
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
INT FParticleRibbonEmitterInstance::GetResourceSize(UBOOL bInExclusiveResourceSizeMode)
{
	INT ResSize = 0;
	if (!bInExclusiveResourceSizeMode ||
		(Component && Component->SceneInfo && Component->SceneInfo->Proxy))
	{
		INT MaxActiveParticleDataSize = (ParticleData != NULL) ? (MaxActiveParticles * ParticleStride) : 0;
		INT MaxActiveParticleIndexSize = (ParticleIndices != NULL) ? (MaxActiveParticles * sizeof(WORD)) : 0;
		// Take dynamic data into account as well
		ResSize = sizeof(FParticleTrailEmitterInstance);
		ResSize += MaxActiveParticleDataSize;								// Copy of the particle data on the render thread
		ResSize += MaxActiveParticleIndexSize;								// Copy of the particle indices on the render thread
		if (DynamicParameterDataOffset == 0)
		{
			ResSize += MaxActiveParticles * sizeof(FParticleBeamTrailVertex);	// The vertex data array
		}
		else
		{
			ResSize += MaxActiveParticles * sizeof(FParticleBeamTrailVertexDynamicParameter);
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
UBOOL FParticleRibbonEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
	if (ActiveParticles <= 0)
	{
		return FALSE;
	}

	// If the template is disabled, don't return data.
	UParticleLODLevel* LODLevel = SpriteTemplate->GetLODLevel(0);
	if ((LODLevel == NULL) || (LODLevel->bEnabled == FALSE))
	{
		return FALSE;
	}

	// This function can modify the ParticleData (changes TriangleCount of trail payloads), so we
	// we need to call it before calling the parent implementation of FillReplayData, since that
	// will memcpy the particle data to the render thread's buffer.
	DetermineVertexAndTriangleCount();

	const INT IndexCount = TriangleCount + 2;
	if (IndexCount > MAX_TRAIL_INDICES)
	{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		warnf(NAME_Warning, TEXT("RIBBON   : FillReplayData failed."));
		warnf(NAME_Warning, TEXT("\tIndexCount (%d) exceeds allowed value (%d)."), IndexCount, MAX_TRAIL_INDICES);
		warnf(NAME_Warning, TEXT("\tActiveParticleCount = %d."), ActiveParticles);
		warnf(NAME_Warning, TEXT("\tTriangleCount = %d."), TriangleCount);
		warnf(NAME_Warning, TEXT("\tTrailCount = %d."), TrailCount);
		warnf(NAME_Warning, TEXT("\t%s"), Component ? Component->Template ? 
			*(Component->Template->GetPathName()) : *(Component->GetName()) : TEXT("NO COMPONENT"));
#endif
		return FALSE;
	}

#if XBOX 
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	INT CheckStride = (DynamicParameterDataOffset > 0) ? sizeof(FParticleBeamTrailVertexDynamicParameter) : sizeof(FParticleBeamTrailVertex);
	if ((VertexCount * CheckStride) > GDrawUPVertexCheckCount)
	{
		warnf(NAME_Warning,
			TEXT("Skipping ribbons render as call would use too much space in the ringbuffer! %s (%s)"), 
			Component ? 
				(Component->Template ? *(Component->Template->GetPathName()) : TEXT("*** No Template")) :
				TEXT("*** No Component"),
			Component ? *(Component->GetPathName()) : TEXT("*** No Components"));
		warnf(NAME_Warning,
			TEXT("\tSize: %8i Verts: %8i Stride: %8i"), 
			VertexCount * CheckStride, 
			VertexCount, 
			CheckStride);
		warnf(NAME_Warning,
			TEXT("\tActive Particles: %8i"), 
			ActiveParticles);
		return FALSE;
	}
#endif
#endif

	// Call parent implementation first to fill in common particle source data
	if (!FParticleEmitterInstance::FillReplayData(OutData))
	{
		return FALSE;
	}

	// Get the material instance. If there is none, or the material isn't flagged for use with particle systems, use the DefaultMaterial.
	UMaterialInterface* MaterialInst = LODLevel->RequiredModule->Material;
	if (MaterialInst == NULL || !MaterialInst->CheckMaterialUsage(MATUSAGE_BeamTrails))
	{
		MaterialInst = GEngine->DefaultMaterial;
	}

	if (TriangleCount <= 0)
	{
		if ((ActiveParticles > 0) && (ActiveParticles != HeadOnlyParticles))
		{
			if (!TrailTypeData->bClipSourceSegement)
			{
// 				warnf(TEXT("TRAIL: GetDynamicData -- TriangleCount == 0 (APC = %4d) for PSys %s"),
// 					ActiveParticles, 
// 					Component ? (Component->Template ? *Component->Template->GetName() : 
// 					TEXT("No Template")) : TEXT("No Component"));
#if !(FINAL_RELEASE || SHIPPING_PC_GAME)
				AWorldInfo* WorldInfo = GWorld ? GWorld->GetWorldInfo() : NULL;
				if (WorldInfo)
				{
					FString ErrorMessage = 
						FString::Printf(TEXT("RIBBON: GetDynamicData -- TriangleCount == %d (APC = %4d) for PSys %s"),
							TriangleCount, ActiveParticles, 
							Component ? (Component->Template ? *Component->Template->GetName() : 
							TEXT("No Template")) : TEXT("No Component"));
					FColor ErrorColor(255,0,0);
					WorldInfo->AddOnScreenDebugMessage((QWORD)((PTRINT)this), 5.0f, ErrorColor,ErrorMessage);
					debugf(*ErrorMessage);
				}
#endif	//#if !FINAL_RELEASE
			}
		}
		return FALSE;
	}

	OutData.eEmitterType = DET_Ribbon;
	FDynamicRibbonEmitterReplayData* NewReplayData = static_cast<FDynamicRibbonEmitterReplayData*>( &OutData );

	NewReplayData->MaterialInterface = MaterialInst;
	// We never want local space for trails
	NewReplayData->bUseLocalSpace = FALSE;
	// Never use axis lock for trails
	NewReplayData->bLockAxis = FALSE;
	
	NewReplayData->MaxActiveParticleCount = MaxActiveParticles;
	NewReplayData->MaxTessellationBetweenParticles = TrailTypeData->MaxTessellationBetweenParticles ? TrailTypeData->MaxTessellationBetweenParticles : 1;
	NewReplayData->Sheets = TrailTypeData->SheetsPerTrail ? TrailTypeData->SheetsPerTrail : 1;

	NewReplayData->VertexCount = VertexCount;
	NewReplayData->IndexCount = TriangleCount + 2;
	NewReplayData->PrimitiveCount = TriangleCount;
	NewReplayData->TrailCount = TrailCount;

	//@todo.SAS. Check for requiring DWORD sized indices?
	NewReplayData->IndexStride = sizeof(WORD);

	NewReplayData->TrailDataOffset = TypeDataOffset;

	return TRUE;
}

//
//	FParticleAnimTrailEmitterInstance
//
IMPLEMENT_PARTICLEEMITTERINSTANCE_TYPE(FParticleAnimTrailEmitterInstance);

/** Constructor	*/
FParticleAnimTrailEmitterInstance::FParticleAnimTrailEmitterInstance() :
	  FParticleTrailsEmitterInstance_Base()
	, TrailTypeData(NULL)
	, SpawnPerUnitModule(NULL)
	, CurrentAnimDataCount(0)
	, CurrentSourceUpdateTime(0.0f)
	, bTagTrailAsDead(FALSE)
	, bTagEmitterAsDead(FALSE)
{
	appMemzero(&CurrentSourceData, sizeof(CurrentSourceData));
	appMemzero(&LastSourceData, sizeof(CurrentSourceData));
	appMemzero(&LastAnimSampleTime, sizeof(FLOAT));
}

/** Destructor	*/
FParticleAnimTrailEmitterInstance::~FParticleAnimTrailEmitterInstance()
{
}

void FParticleAnimTrailEmitterInstance::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, UBOOL bClearResources)
{
	FParticleTrailsEmitterInstance_Base::InitParameters(InTemplate, InComponent, bClearResources);

	// We don't support LOD on trails
	UParticleLODLevel* LODLevel	= InTemplate->GetLODLevel(0);
	check(LODLevel);
	TrailTypeData = CastChecked<UParticleModuleTypeDataAnimTrail>(LODLevel->TypeDataModule);
	check(TrailTypeData);

	CurrentSourceUpdateTime = 0.0f;

	bDeadTrailsOnDeactivate = TrailTypeData->bDeadTrailsOnDeactivate;

	TrailSpawnTimes.Empty(1);
	TrailSpawnTimes.AddZeroed(1);
	SourceDistanceTraveled.Empty(1);
	SourceDistanceTraveled.AddZeroed(1);
	TiledUDistanceTraveled.Empty(1);
	TiledUDistanceTraveled.AddZeroed(1);

	//
	VertexCount = 0;
	TriangleCount = 0;
}

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
void FParticleAnimTrailEmitterInstance::RecalculateTangent(
	FBaseParticle* PrevParticle, FAnimTrailTypeDataPayload* PrevTrailData, 
	FBaseParticle* CurrParticle, FAnimTrailTypeDataPayload* CurrTrailData, 
	FBaseParticle* NextParticle, FAnimTrailTypeDataPayload* NextTrailData)
{
	if (NextParticle != NULL)
	{
		check(CurrParticle && PrevParticle);
		// Recalculate the current tangent...
		// Calculate the new tangent from the previous and next position...
		// LastSourcePosition will be that position of the first particle that will be spawned this frame
		FVector PositionDelta = PrevParticle->Location - NextParticle->Location;
		FVector FirstDelta = PrevTrailData->FirstEdge - NextTrailData->FirstEdge;
		FVector SecondDelta = PrevTrailData->SecondEdge - NextTrailData->SecondEdge;
		FLOAT TimeDelta = 2.0f * AnimSampleTimeStep;
		FVector NewTangent = (PositionDelta / TimeDelta);
		FVector NewFirstTangent = (FirstDelta / TimeDelta);
		FVector NewSecondTangent = (SecondDelta / TimeDelta);
		if (NewTangent.IsNearlyZero() == FALSE)
		{
			CurrTrailData->ControlVelocity = NewTangent;
		}
		if (NewFirstTangent.IsNearlyZero() == FALSE)
		{
			CurrTrailData->FirstVelocity = NewFirstTangent;
		}
		if (NewSecondTangent.IsNearlyZero() == FALSE)
		{
			CurrTrailData->SecondVelocity = NewSecondTangent;
		}
	}
	else if (PrevParticle != NULL)
	{
		check(CurrParticle);
		// The start particle... should we recalc w/ the current source position???
		// Recalculate the current tangent...
		// Calculate the new tangent from the previous and next position...
		// LastSourcePosition will be that position of the first particle that will be spawned this frame
		FVector PositionDelta = PrevParticle->Location - CurrParticle->Location;
		FVector FirstDelta = PrevTrailData->FirstEdge - CurrTrailData->FirstEdge;
		FVector SecondDelta = PrevTrailData->SecondEdge - CurrTrailData->SecondEdge;
		FLOAT TimeDelta = AnimSampleTimeStep;
		FVector NewTangent = (PositionDelta / TimeDelta);
		FVector NewFirstTangent = (FirstDelta / TimeDelta);
		FVector NewSecondTangent = (SecondDelta / TimeDelta);
		if (NewTangent.IsNearlyZero() == FALSE)
		{
			CurrTrailData->ControlVelocity = NewTangent;
		}
		if (NewFirstTangent.IsNearlyZero() == FALSE)
		{
			CurrTrailData->FirstVelocity = NewFirstTangent;
		}
		if (NewSecondTangent.IsNearlyZero() == FALSE)
		{
			CurrTrailData->SecondVelocity = NewSecondTangent;
		}
	}
}

/**
 *	Tick sub-function that handles recalculation of tangents
 *
 *	@param	DeltaTime			The current time slice
 *	@param	CurrentLODLevel		The current LOD level for the instance
 */
void FParticleAnimTrailEmitterInstance::Tick_RecalculateTangents(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel)
{
	if (TrailTypeData->bTangentRecalculationEveryFrame == TRUE)
	{
		//@todo. Multiple trails, single emitter
		INT TrailIdx = 0;
		// Find the Start particle of the current trail...
		FBaseParticle* StartParticle = NULL;
		FAnimTrailTypeDataPayload* StartTrailData = NULL;
		for (INT FindTrailIdx = 0; FindTrailIdx < ActiveParticles; FindTrailIdx++)
		{
			INT CheckStartIndex = ParticleIndices[FindTrailIdx];
			DECLARE_PARTICLE_PTR(CheckParticle, ParticleData + ParticleStride * CheckStartIndex);
			FAnimTrailTypeDataPayload* CheckTrailData = ((FAnimTrailTypeDataPayload*)((BYTE*)CheckParticle + TypeDataOffset));
			if (TRAIL_EMITTER_IS_START(CheckTrailData->Flags))
			{
				if (CheckTrailData->TrailIndex == TrailIdx)
				{
					StartParticle = CheckParticle;
					StartTrailData = CheckTrailData;
					break;
				}
			}
		}

		// Recalculate tangents at each particle to properly handle moving particles...
		if ((StartParticle != NULL) && (TRAIL_EMITTER_IS_ONLY(StartTrailData->Flags) == 0))
		{
			// For trails, particles go:
			//     START, next, next, ..., END
			// Coming from the end,
			//     END, prev, prev, ..., START
			FBaseParticle* PrevParticle = StartParticle;
			FAnimTrailTypeDataPayload* PrevTrailData = StartTrailData;
			FBaseParticle* CurrParticle = NULL;
			FAnimTrailTypeDataPayload* CurrTrailData = NULL;
			FBaseParticle* NextParticle = NULL;
			FAnimTrailTypeDataPayload* NextTrailData = NULL;

			FTrailsBaseTypeDataPayload* TempPayload = NULL;

			GetParticleInTrail(TRUE, PrevParticle, PrevTrailData, GET_Next, GET_Any, CurrParticle, TempPayload);
			CurrTrailData = (FAnimTrailTypeDataPayload*)(TempPayload);
			while (CurrParticle != NULL)
			{
				// Grab the next particle in the trail...
				GetParticleInTrail(TRUE, CurrParticle, CurrTrailData, GET_Next, GET_Any, NextParticle, TempPayload);
				NextTrailData = (FAnimTrailTypeDataPayload*)(TempPayload);

				check(CurrParticle != PrevParticle);
				check(CurrParticle != NextParticle);

				RecalculateTangent(PrevParticle, PrevTrailData, CurrParticle, CurrTrailData, NextParticle, NextTrailData);

				// Move up the chain...
				PrevParticle = CurrParticle;
				PrevTrailData = CurrTrailData;
				CurrParticle = NextParticle;
				CurrTrailData = NextTrailData;
			}
		}
	}
}

/**
 *	Tick sub-function that handles module post updates
 *
 *	@param	DeltaTime			The current time slice
 *	@param	CurrentLODLevel		The current LOD level for the instance
 */
void FParticleAnimTrailEmitterInstance::Tick_ModulePostUpdate(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel)
{
	if (TrailTypeData)
	{
		// Update/postupdate
		TrailTypeData->Update(this, TypeDataOffset, DeltaTime);	
		TrailTypeData->PostUpdate(this, TypeDataOffset, DeltaTime);	
	}
}

UBOOL FParticleAnimTrailEmitterInstance::GetSpawnPerUnitAmount(FLOAT DeltaTime, INT InTrailIdx, INT& OutCount, FLOAT& OutRate)
{
	return FALSE;
}

FLOAT FParticleAnimTrailEmitterInstance::Spawn(FLOAT DeltaTime)
{
static INT TickCount = 0;
	if (CurrentAnimDataCount <= 0)
	{
		// Nothing to spawn this tick...
		return SpawnFraction;
	}

	UParticleLODLevel* LODLevel = SpriteTemplate->LODLevels(0);
	check(LODLevel);
	check(LODLevel->RequiredModule);

	// Iterate over each trail
	//@todo. Add support for multiple trails?
//	for (INT TrailIdx = 0; TrailIdx < MaxTrailCount; TrailIdx++)
	INT TrailIdx = 0;

	FLOAT SpawnRate = 0.0f;
	INT SpawnCount = 0;
	INT BurstCount = 0;
	FLOAT OldLeftover = SpawnFraction;
	// For now, we are not supporting bursts on trails...
	UBOOL bProcessSpawnRate = FALSE;
	UBOOL bProcessBurstList = FALSE;

	// Figure out spawn rate for this tick.
	if (bProcessSpawnRate)
	{
		FLOAT RateScale = LODLevel->SpawnModule->RateScale.GetValue(EmitterTime, Component);
		if (GSystemSettings.DetailMode != DM_High)
		{
			RateScale *= SpriteTemplate->MediumDetailSpawnRateScale;
		}
		SpawnRate += LODLevel->SpawnModule->Rate.GetValue(EmitterTime, Component) * Clamp<FLOAT>(RateScale, 0.0f, RateScale);
	}

	// Take Bursts into account as well...
	if (bProcessBurstList)
	{
		INT Burst = 0;
		FLOAT BurstTime = GetCurrentBurstRateOffset(DeltaTime, Burst);
		BurstCount += Burst;
	}

	FLOAT SafetyLeftover = OldLeftover;
	FLOAT NewLeftover = OldLeftover + DeltaTime * SpawnRate;
	INT SpawnNumber	= appFloor(NewLeftover);
	FLOAT SliceIncrement = (SpawnRate > 0.0f) ? (1.f / SpawnRate) : 0.0f;
	FLOAT SpawnStartTime = DeltaTime + OldLeftover * SliceIncrement - SliceIncrement;
	SpawnFraction = NewLeftover - SpawnNumber;
	// Do the resize stuff here!!!!!!!!!!!!!!!!!!!
	INT TotalCount = CurrentAnimDataCount + SpawnNumber + BurstCount;
	// Determine if no particles are alive
	UBOOL bNoLivingParticles = (ActiveParticles == 0);

	// Don't allow more than TrailCount trails...
// 	INT	MaxParticlesAllowed	= MaxTrailCount * TrailTypeData->MaxParticleInTrailCount;
// 	if ((TotalCount + ActiveParticles) > MaxParticlesAllowed)
// 	{
// 		TotalCount = MaxParticlesAllowed - ActiveParticles - 1;
// 		if (TotalCount < 0)
// 		{
// 			TotalCount = 0;
// 		}
// 	}

	// Handle growing arrays.
	UBOOL bProcessSpawn = TRUE;
	INT NewCount = ActiveParticles + TotalCount;
	if (NewCount >= MaxActiveParticles)
	{
		if (DeltaTime < 0.25f)
		{
			bProcessSpawn = Resize(NewCount + appTrunc(appSqrt((FLOAT)NewCount)) + 1);
		}
		else
		{
			bProcessSpawn = Resize((NewCount + appTrunc(appSqrt((FLOAT)NewCount)) + 1), FALSE);
		}
	}

	if (bProcessSpawn == FALSE)
	{
		return SafetyLeftover;
	}

	//@todo. Support multiple trails per emitter
	// Find the start particle of the current trail...
	FBaseParticle* StartParticle = NULL;
	INT StartParticleIndex = -1;
	FAnimTrailTypeDataPayload* StartTrailData = NULL;
	for (INT FindTrailIdx = 0; FindTrailIdx < ActiveParticles; FindTrailIdx++)
	{
		INT CheckStartIndex = ParticleIndices[FindTrailIdx];
		DECLARE_PARTICLE_PTR(CheckParticle, ParticleData + ParticleStride * CheckStartIndex);
		FAnimTrailTypeDataPayload* CheckTrailData = ((FAnimTrailTypeDataPayload*)((BYTE*)CheckParticle + TypeDataOffset));
		if (CheckTrailData->TrailIndex == TrailIdx)
		{
			if (TRAIL_EMITTER_IS_START(CheckTrailData->Flags))
			{
				StartParticle = CheckParticle;
				StartParticleIndex = CheckStartIndex;
				StartTrailData = CheckTrailData;
				break;
			}
		}
	}

	bNoLivingParticles = (StartParticle == NULL);
	UBOOL bTilingTrail = !appIsNearlyZero(TrailTypeData->TilingDistance);

	// Do we have movement based spawning?
	// If so, then interpolate the position/tangent data between 
	// CurrentSource<Position/Tangent> and LastSource<Position/Tangent>
	// Don't allow new spawning if the emitter is finished
	if (CurrentAnimDataCount > 0 && !bTagEmitterAsDead)
	{
		FLOAT ElapsedTime = RunningTime;//SecondsSinceCreation;
		FLOAT LastTime = TrailSpawnTimes(TrailIdx);
		FLOAT Diff = ElapsedTime - LastTime;
		FLOAT InvCount = 1.0f / CurrentAnimDataCount;
		// SpawnTime increment
		FLOAT Increment = DeltaTime / CurrentAnimDataCount;

		FMatrix SavedLocalToWorld = Component->LocalToWorld;

		check(CurrentAnimDataCount <= AnimData.Num());

		FLOAT ProcessedTime = 0.0f;

//		debugf(TEXT("TickCount %5d: Processing %2d AnimData samples!"), TickCount, CurrentAnimDataCount);

		// Source time difference for interpolating the source position
		FLOAT SourceTimeDiff = CurrentSourceUpdateTime - LastSourceUpdateTime;
		for (INT SpawnIdx = 0; SpawnIdx < CurrentAnimDataCount; SpawnIdx++)
		{
			FLOAT TimeStep = InvCount * SpawnIdx;

			FAnimTrailSamplePoint& SamplePoint = AnimData(SpawnIdx);

// 			debugf(TEXT("\tSAMPLED %8.6f, CURR %8.6f, REL %8.6f, TS %8.6f: Position %8.5f,%8.5f,%8.5f - Velocity %8.5f,%8.5f,%8.5f"),
// 				SamplePoint.AnimSampleTime, SamplePoint.AnimCurrentTime, SamplePoint.RelativeTime, SamplePoint.TimeStep,
// 				SamplePoint.ControlPointSample.Position.X, SamplePoint.ControlPointSample.Position.Y, SamplePoint.ControlPointSample.Position.Z,
// 				SamplePoint.ControlPointSample.Velocity.X, SamplePoint.ControlPointSample.Velocity.Y, SamplePoint.ControlPointSample.Velocity.Z);

			FLOAT SampleTimeDiff = SamplePoint.AnimCurrentTime - LastSourceUpdateTime;
			FLOAT SourceInterpStep = (SourceTimeDiff > 0.0f) ? (SampleTimeDiff / SourceTimeDiff) : 0.0f;

			FVector InterpSourcePos = Lerp<FVector>(LastOwnerData.Position, CurrentOwnerData.Position, SourceInterpStep);
			FQuat InterpSourceRot = SlerpQuat(LastOwnerData.Rotation, CurrentOwnerData.Rotation, SourceInterpStep);
			FRotator InterpSourceRotator = InterpSourceRot.Rotator();
			FMatrix InterpSourceMatrix = FRotationTranslationMatrix(InterpSourceRotator, InterpSourcePos);

			FVector TransformedControlPosition = SamplePoint.ControlPointSample.Position;
			FVector TransformedControlVelocity = SamplePoint.ControlPointSample.Velocity;
			FVector TransformedFirstEdgePosition = SamplePoint.FirstEdgeSample.Position;
			FVector TransformedFirstEdgeVelocity = SamplePoint.FirstEdgeSample.Velocity;
			FVector TransformedSecondEdgePosition = SamplePoint.SecondEdgeSample.Position;
			FVector TransformedSecondEdgeVelocity = SamplePoint.SecondEdgeSample.Velocity;

			INT ParticleIndex = ParticleIndices[ActiveParticles];
			DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndex);
			FAnimTrailTypeDataPayload* TrailData = ((FAnimTrailTypeDataPayload*)((BYTE*)Particle + TypeDataOffset));

			// We are going from 'oldest' to 'newest' for this spawn, so reverse the time
			FLOAT StoredSpawnTime = Diff * (1.0f - TimeStep);
			FLOAT SpawnTime = DeltaTime - (SpawnIdx * Increment);
			FLOAT TrueSpawnTime = Diff * TimeStep;

			Component->LocalToWorld = InterpSourceMatrix;

			// Standard spawn setup
			PreSpawn(Particle);
			for (INT SpawnModuleIdx = 0; SpawnModuleIdx < LODLevel->SpawnModules.Num(); SpawnModuleIdx++)
			{
				UParticleModule* SpawnModule = LODLevel->SpawnModules(SpawnModuleIdx);
				if (!SpawnModule || !SpawnModule->bEnabled)
				{
					continue;
				}

				UINT* Offset = ModuleOffsetMap.Find(SpawnModule);
				SpawnModule->Spawn(this, Offset ? *Offset : 0, SpawnTime);
			}

			if ((1.0f / Particle->OneOverMaxLifetime) < 0.001f)
			{
				Particle->OneOverMaxLifetime = 1.f / 0.001f;
			}

			if (LODLevel->TypeDataModule)
			{
				//@todo. Need to track TypeData offset into payload!
				LODLevel->TypeDataModule->Spawn(this, TypeDataOffset, SpawnTime);
			}
			PostSpawn(Particle, 1.f - FLOAT(SpawnIdx + 1) / FLOAT(CurrentAnimDataCount), SpawnTime);

			Component->LocalToWorld = SavedLocalToWorld;

			ProcessedTime = SamplePoint.AnimCurrentTime;

			// Trail specific...
			// Clear the next and previous - just to be safe
			TrailData->Flags = TRAIL_EMITTER_SET_NEXT(TrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
			TrailData->Flags = TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);
			// Set the trail-specific data on this particle
			TrailData->TrailIndex = TrailIdx;
			TrailData->FirstEdge = TransformedFirstEdgePosition;
			TrailData->FirstVelocity = TransformedFirstEdgeVelocity;
			TrailData->SecondEdge = TransformedSecondEdgePosition;
			TrailData->SecondVelocity = TransformedSecondEdgeVelocity;
			TrailData->ControlVelocity = TransformedControlVelocity;
			TrailData->SpawnTime = ElapsedTime - StoredSpawnTime;
			TrailData->SpawnDelta = TrueSpawnTime;
			// Set the location and up vectors
			Particle->Location = TransformedControlPosition;
			Particle->OldLocation = TransformedControlPosition;

			// If this is the true spawned particle, store off the spawn interpolated count
			TrailData->bInterpolatedSpawn = FALSE; 
			TrailData->SpawnedTessellationPoints = 1;

			UBOOL bAddedParticle = FALSE;
			// Determine which trail to attach to
			if (bNoLivingParticles)
			{
				// These are the first particles!
				// Tag it as the 'only'
				TrailData->Flags = TRAIL_EMITTER_SET_ONLY(TrailData->Flags);
				TiledUDistanceTraveled(TrailIdx) = 0.0f;
				TrailData->TiledU = 0.0f;
				bNoLivingParticles	= FALSE;
				bAddedParticle		= TRUE;
			}
			else if (StartParticle)
			{
				//@todo. Determine how to handle multiple trails...
				if (TRAIL_EMITTER_IS_ONLY(StartTrailData->Flags))
				{
						StartTrailData->Flags	= TRAIL_EMITTER_SET_END(StartTrailData->Flags);
						StartTrailData->Flags	= TRAIL_EMITTER_SET_NEXT(StartTrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
						StartTrailData->Flags	= TRAIL_EMITTER_SET_PREV(StartTrailData->Flags, ParticleIndex);

						if (TrailData->SpawnTime < StartTrailData->SpawnTime)
						{
							debugf(TEXT("BAD SPAWN TIME! Curr %8.6f, Start %8.6f"), TrailData->SpawnTime, StartTrailData->SpawnTime);
						}

						// Now, 'join' them
						TrailData->Flags		= TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);
						TrailData->Flags		= TRAIL_EMITTER_SET_NEXT(TrailData->Flags, StartParticleIndex);
						TrailData->Flags		= TRAIL_EMITTER_SET_START(TrailData->Flags);

						bAddedParticle = TRUE;
				}
				else
				{
					// It better be the start!!!
					check(TRAIL_EMITTER_IS_START(StartTrailData->Flags));
					check(TRAIL_EMITTER_GET_NEXT(StartTrailData->Flags) != TRAIL_EMITTER_NULL_NEXT);

					StartTrailData->Flags	= TRAIL_EMITTER_SET_MIDDLE(StartTrailData->Flags);
					StartTrailData->Flags	= TRAIL_EMITTER_SET_PREV(StartTrailData->Flags, ParticleIndex);

					if (TrailData->SpawnTime < StartTrailData->SpawnTime)
					{
						checkf(0, TEXT("BAD SPAWN TIME! Curr %8.6f, Start %8.6f"), TrailData->SpawnTime, StartTrailData->SpawnTime);
					}

					// Now, 'join' them
					TrailData->Flags		= TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);
					TrailData->Flags		= TRAIL_EMITTER_SET_NEXT(TrailData->Flags, StartParticleIndex);
					TrailData->Flags		= TRAIL_EMITTER_SET_START(TrailData->Flags);

					//SourceDistanceTravelled(TrailData->TrailIndex) += SourceDistanceTravelled(CheckTrailData->TrailIndex);

					bAddedParticle = TRUE;
				}

				if ((TrailTypeData->bEnablePreviousTangentRecalculation == TRUE)&& 
					(TrailTypeData->bTangentRecalculationEveryFrame == FALSE))
				{
					FBaseParticle* PrevParticle = Particle;
					FAnimTrailTypeDataPayload* PrevTrailData = TrailData;
					FBaseParticle* CurrParticle = StartParticle;
					FAnimTrailTypeDataPayload* CurrTrailData = StartTrailData;
					FBaseParticle* NextParticle = NULL;
					FAnimTrailTypeDataPayload* NextTrailData = NULL;

					INT StartNext = TRAIL_EMITTER_GET_NEXT(StartTrailData->Flags);
					if (StartNext != TRAIL_EMITTER_NULL_NEXT)
					{
						DECLARE_PARTICLE_PTR(TempParticle, ParticleData + ParticleStride * StartNext);
						NextParticle = TempParticle;
						NextTrailData = ((FAnimTrailTypeDataPayload*)((BYTE*)NextParticle + TypeDataOffset));
					}
					RecalculateTangent(PrevParticle, PrevTrailData, CurrParticle, CurrTrailData, NextParticle, NextTrailData);
				}
			}

			if (bAddedParticle)
			{
				if (bTilingTrail == TRUE)
				{
					if (StartParticle == NULL)
					{
						TrailData->TiledU = 0.0f;
					}
					else
					{
						FVector PositionDelta = Particle->Location - StartParticle->Location;
						TiledUDistanceTraveled(TrailIdx) += PositionDelta.Size();
						TrailData->TiledU = TiledUDistanceTraveled(TrailIdx) / TrailTypeData->TilingDistance;
						//@todo. Is there going to be a problem when distance gets REALLY high?
					}
				}

				StartParticle = Particle;
				StartParticleIndex = ParticleIndex;
				StartTrailData = TrailData;

				ActiveParticles++;
//				check((INT)ActiveParticles < TrailTypeData->MaxParticleInTrailCount);
				INC_DWORD_STAT(STAT_TrailParticlesSpawned);
//					LastEmittedParticleIndex = ParticleIndex;
			}
			else
			{
				check(TEXT("Failed to add particle to trail!!!!"));
			}

			INC_DWORD_STAT_BY(STAT_TrailParticles, ActiveParticles);
		}

		// Update the last position
		TrailSpawnTimes(0) = RunningTime;
		CurrentAnimDataCount = 0;
		LastAnimProcessedTime = ProcessedTime;
		LastSourceUpdateTime = CurrentSourceUpdateTime;
	}

	if (bTagTrailAsDead == TRUE)
	{
		for (INT FindTrailIdx = 0; FindTrailIdx < ActiveParticles; FindTrailIdx++)
		{
			INT CheckStartIndex = ParticleIndices[FindTrailIdx];
			DECLARE_PARTICLE_PTR(CheckParticle, ParticleData + ParticleStride * CheckStartIndex);
			FAnimTrailTypeDataPayload* CheckTrailData = ((FAnimTrailTypeDataPayload*)((BYTE*)CheckParticle + TypeDataOffset));
			if (CheckTrailData->TrailIndex == TrailIdx)
			{
				if (TRAIL_EMITTER_IS_START(CheckTrailData->Flags))
				{
					CheckTrailData->Flags = TRAIL_EMITTER_SET_DEADTRAIL(CheckTrailData->Flags);
				}
			}
		}
		bTagTrailAsDead = FALSE;
	}
TickCount++;
	return SpawnFraction;
}

void FParticleAnimTrailEmitterInstance::SetupTrailModules()
{
	// Trails are a special case... 
	// We don't want standard Spawn/Update calls occurring on Trail-type modules.
	UParticleLODLevel* LODLevel = SpriteTemplate->GetLODLevel(0);
	check(LODLevel);
	for (INT ModuleIdx = 0; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
	{
		UBOOL bRemoveIt = FALSE;
		UParticleModule* CheckModule = LODLevel->Modules(ModuleIdx);
		UParticleModuleSpawnPerUnit* CheckSPUModule = Cast<UParticleModuleSpawnPerUnit>(CheckModule);
		if (CheckSPUModule != NULL)
		{
			SpawnPerUnitModule = CheckSPUModule;
			bRemoveIt = TRUE;
		}

		if (bRemoveIt == TRUE)
		{
			// Remove it from any lists...
			for (INT UpdateIdx = LODLevel->UpdateModules.Num() - 1; UpdateIdx >= 0; UpdateIdx--)
			{
				if (LODLevel->UpdateModules(UpdateIdx) == CheckModule)
				{
					LODLevel->UpdateModules.Remove(UpdateIdx);
				}
			}

			for (INT SpawnIdx = LODLevel->SpawnModules.Num() - 1; SpawnIdx >= 0; SpawnIdx--)
			{
				if (LODLevel->SpawnModules(SpawnIdx) == CheckModule)
				{
					LODLevel->SpawnModules.Remove(SpawnIdx);
				}
			}

			for (INT SpawningIdx = LODLevel->SpawningModules.Num() - 1; SpawningIdx >= 0; SpawningIdx--)
			{
				if (LODLevel->SpawningModules(SpawningIdx) == CheckModule)
				{
					LODLevel->SpawningModules.Remove(SpawningIdx);
				}
			}
		}
	}
}

void FParticleAnimTrailEmitterInstance::UpdateSourceData(FLOAT DeltaTime, UBOOL bFirstTime)
{
}

/** Determine the number of vertices and triangles in each trail */
void FParticleAnimTrailEmitterInstance::DetermineVertexAndTriangleCount()
{
	UINT NewSize = 0;
	INT Sheets = 1;//TrailTypeData->Sheets ? TrailTypeData->Sheets : 1;
	INT TheTrailCount = 0;
	INT IndexCount = 0;

	VertexCount		= 0;
	TriangleCount	= 0;

	INT CheckParticleCount = 0;

	INT TempVertexCount;
	// 
	for (INT ii = 0; ii < ActiveParticles; ii++)
	{
		INT LocalIndexCount = 0;
		INT	ParticleCount = 0;
		INT LocalVertexCount = 0;
		INT LocalTriCount = 0;

		UBOOL bProcessParticle = FALSE;

		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[ii]);
		FBaseParticle* CurrParticle = Particle;
		FAnimTrailTypeDataPayload*	CurrTrailData = ((FAnimTrailTypeDataPayload*)((BYTE*)Particle + TypeDataOffset));
		if (TRAIL_EMITTER_IS_HEADONLY(CurrTrailData->Flags))
		{
			// If there is only a single particle in the trail, then we only want to render it
			// if we are connecting to the source...
//			if (TrailTypeData->bClipSourceSegement == FALSE)
			{
				// Store off the rendering interp count for this particle
// 				CurrTrailData->RenderingInterpCount = 1;
// 				CurrTrailData->PinchScaleFactor = 1.0f;
// 				bProcessParticle = TRUE;
			}
			CurrTrailData->RenderingInterpCount = 0;
			CurrTrailData->TriangleCount = 0;
		}
		else if (TRAIL_EMITTER_IS_END(CurrTrailData->Flags))
		{
			// Walk from the end of the trail to the front
			FBaseParticle* PrevParticle = NULL;
			FAnimTrailTypeDataPayload*	PrevTrailData = NULL;
			INT	Prev = TRAIL_EMITTER_GET_PREV(CurrTrailData->Flags);
			if (Prev != TRAIL_EMITTER_NULL_PREV)
			{
				DECLARE_PARTICLE_PTR(InnerParticle, ParticleData + ParticleStride * Prev);
				PrevParticle = InnerParticle;
				PrevTrailData = ((FAnimTrailTypeDataPayload*)((BYTE*)InnerParticle + TypeDataOffset));

				UBOOL bDone = FALSE;
				// The end of the trail, so there MUST be another particle
				while (!bDone)
				{
					ParticleCount++;
					// Determine the number of rendered interpolated points between these two particles
					FLOAT CheckDistance = (CurrParticle->Location - PrevParticle->Location).Size();
					FVector SrcTangent = CurrTrailData->ControlVelocity;
					SrcTangent.Normalize();
					FVector PrevTangent = PrevTrailData->ControlVelocity;
					PrevTangent.Normalize();
					FLOAT CheckTangent = (SrcTangent | PrevTangent);
					// Map the tangent difference to [0..1] for [0..180]
					//  1.0 = parallel    --> -1 = 0
					//  0.0 = orthogonal  --> -1 = -1 --> * -0.5 = 0.5
					// -1.0 = oppositedir --> -1 = -2 --> * -0.5 = 1.0
					CheckTangent = (CheckTangent - 1.0f) * -0.5f;

					FLOAT DistDiff = (TrailTypeData->DistanceTessellationStepSize > 0.0f) ? CheckDistance / TrailTypeData->DistanceTessellationStepSize : 0.0f;
					//@todo. Need to adjust the tangent diff count when the distance is REALLY small...
					FLOAT TangDiff = CheckTangent * TrailTypeData->TangentTessellationScalar;
					INT InterpCount = appTrunc(DistDiff) + appTrunc(TangDiff);

					// There always is at least 1 point (the source particle itself)
					InterpCount = (InterpCount > 0) ? InterpCount : 1;

					// Store off the rendering interp count for this particle
					CurrTrailData->RenderingInterpCount = InterpCount;
					if (CheckTangent <= 0.5f)
					{
						CurrTrailData->PinchScaleFactor = 1.0f;
					}
					else
					{
						CurrTrailData->PinchScaleFactor = 1.0f - (CheckTangent * 0.5f);
					}

					// Tally up the vertex and index counts for this segment...
					TempVertexCount = 2 * InterpCount * Sheets;
					VertexCount += TempVertexCount;
					LocalVertexCount += TempVertexCount;
					LocalIndexCount += TempVertexCount;

					// Move to the previous particle in the chain
					CurrParticle = PrevParticle;
					CurrTrailData = PrevTrailData;
					Prev = TRAIL_EMITTER_GET_PREV(CurrTrailData->Flags);
					if (Prev != TRAIL_EMITTER_NULL_PREV)
					{
						DECLARE_PARTICLE_PTR(InnerParticle, ParticleData + ParticleStride * Prev);
						PrevParticle = InnerParticle;
						PrevTrailData = ((FAnimTrailTypeDataPayload*)((BYTE*)InnerParticle + TypeDataOffset));
					}
					else
					{
						// The START particle will have a previous index of NULL, so we're done
						bDone = TRUE;
					}
				}

				bProcessParticle = TRUE;
			}
			else
			{
				// This means there is only a single particle in the trail - the end...
// 				check(!TEXT("FAIL"));
				bProcessParticle = FALSE;
			}
		}

		if (bProcessParticle == TRUE)
		{
			// The last step is the last interpolated step to the Curr (which should be the start)
			ParticleCount++;
			TempVertexCount = 2 * Sheets;
			VertexCount += TempVertexCount;
			LocalVertexCount += TempVertexCount;
			LocalIndexCount += TempVertexCount;

			// If we are running up to the current source, take that into account as well.
// 			if (TrailTypeData->bClipSourceSegement == FALSE)
// 			{
// 				//@todo. We should interpolate to the source as well!
// 				TempVertexCount = 2 * Sheets;
// 				VertexCount += TempVertexCount;
// 				LocalIndexCount += TempVertexCount;
// 			}

			// Handle degenerates - 4 tris per stitch
			LocalIndexCount	+= ((Sheets - 1) * 4);

			// @todo: We're going and modifying the original ParticleData here!  This is kind of sketchy
			//    since it's not supposed to be changed at this phase
			check(TRAIL_EMITTER_IS_HEAD(CurrTrailData->Flags));
			CurrTrailData->TriangleCount = LocalIndexCount - 2;

			// The last particle in the chain will always have 1 here!
			CurrTrailData->RenderingInterpCount = 1;

			// Increment the total index count
			IndexCount += LocalIndexCount;
			TheTrailCount++;
		}
	}

	TrailCount = TheTrailCount;
	if (TheTrailCount > 0)
	{
		IndexCount += 4 * (TheTrailCount - 1);	// 4 extra indices per Trail (degenerates)
		TriangleCount = IndexCount - (2 * TheTrailCount);
	}
	else
	{
		IndexCount = 0;
		TriangleCount = 0;
	}
}

/**
 *	Retrieves the dynamic data for the emitter
 */
FDynamicEmitterDataBase* FParticleAnimTrailEmitterInstance::GetDynamicData(UBOOL bSelected)
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetLODLevel(0);
	if (IsDynamicDataRequired(LODLevel) == FALSE)
	{
		return NULL;
	}

	// Allocate the dynamic data
	FDynamicAnimTrailEmitterData* NewEmitterData = ::new FDynamicAnimTrailEmitterData(LODLevel->RequiredModule);
	{
		SCOPE_CYCLE_COUNTER(STAT_ParticleMemTime);
		INC_DWORD_STAT(STAT_DynamicEmitterCount);
		INC_DWORD_STAT(STAT_DynamicAnimTrailCount);
		INC_DWORD_STAT_BY(STAT_DynamicEmitterMem, sizeof(FDynamicAnimTrailEmitterData));
	}

	NewEmitterData->bClipSourceSegement = TrailTypeData->bClipSourceSegement;
	NewEmitterData->bRenderGeometry = TrailTypeData->bRenderGeometry;
	NewEmitterData->bRenderParticles = TrailTypeData->bRenderSpawnPoints;
	NewEmitterData->bRenderTangents = TrailTypeData->bRenderTangents;
	NewEmitterData->bRenderTessellation = TrailTypeData->bRenderTessellation;
	NewEmitterData->DistanceTessellationStepSize = TrailTypeData->DistanceTessellationStepSize;
	NewEmitterData->TangentTessellationScalar = TrailTypeData->TangentTessellationScalar;
	NewEmitterData->TextureTileDistance = TrailTypeData->TilingDistance;
	NewEmitterData->AnimSampleTimeStep = AnimSampleTimeStep;
	if (NewEmitterData->TextureTileDistance > 0.0f)
	{
		NewEmitterData->bTextureTileDistance = TRUE;
	}
	else
	{
		NewEmitterData->bTextureTileDistance = FALSE;
	}

	// Now fill in the source data
	if (!FillReplayData(NewEmitterData->Source))
	{
		delete NewEmitterData;
		return NULL;
	}

	// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
	NewEmitterData->Init(bSelected);

	return NewEmitterData;
}

/**
 *	Updates the dynamic data for the instance
 *
 *	@param	DynamicData		The dynamic data to fill in
 *	@param	bSelected		TRUE if the particle system component is selected
 */
UBOOL FParticleAnimTrailEmitterInstance::UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected)
{
	if (ActiveParticles <= 0)
	{
		return FALSE;
	}

	if (DynamicData->GetSource().eEmitterType != DET_AnimTrail)
	{
		warnf(TEXT("UpdateDynamicData> NOT A TRAILS EMITTER!"));
		return FALSE;
	}

	FDynamicAnimTrailEmitterData* TrailDynamicData = (FDynamicAnimTrailEmitterData*)DynamicData;
	// Now fill in the source data
	if (!FillReplayData(TrailDynamicData->Source))
	{
		return FALSE;
	}

	TrailDynamicData->bRenderGeometry = TrailTypeData->bRenderGeometry;
	TrailDynamicData->bRenderParticles = TrailTypeData->bRenderSpawnPoints;
	TrailDynamicData->bRenderTangents = TrailTypeData->bRenderTangents;
	TrailDynamicData->bRenderTessellation = TrailTypeData->bRenderTessellation;
	TrailDynamicData->DistanceTessellationStepSize = TrailTypeData->DistanceTessellationStepSize;
	TrailDynamicData->TangentTessellationScalar = TrailTypeData->TangentTessellationScalar;

	// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
	TrailDynamicData->Init(bSelected);

	return TRUE;
}

/**
 *	Retrieves replay data for the emitter
 *
 *	@return	The replay data, or NULL on failure
 */
FDynamicEmitterReplayDataBase* FParticleAnimTrailEmitterInstance::GetReplayData()
{
	if (ActiveParticles <= 0)
	{
		return NULL;
	}

	FDynamicTrailsEmitterReplayData* NewEmitterReplayData = ::new FDynamicTrailsEmitterReplayData();
	check(NewEmitterReplayData != NULL);

	if (!FillReplayData(*NewEmitterReplayData))
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
void FParticleAnimTrailEmitterInstance::GetAllocatedSize(INT& OutNum, INT& OutMax)
{
	INT Size = sizeof(FParticleTrailEmitterInstance);
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
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT FParticleAnimTrailEmitterInstance::GetResourceSize(UBOOL bInExclusiveResourceSizeMode)
{
	INT ResSize = 0;
	if (!bInExclusiveResourceSizeMode ||
		(Component && Component->SceneInfo && Component->SceneInfo->Proxy))
	{
		INT MaxActiveParticleDataSize = (ParticleData != NULL) ? (MaxActiveParticles * ParticleStride) : 0;
		INT MaxActiveParticleIndexSize = (ParticleIndices != NULL) ? (MaxActiveParticles * sizeof(WORD)) : 0;
		// Take dynamic data into account as well
		ResSize = sizeof(FParticleTrailEmitterInstance);
		ResSize += MaxActiveParticleDataSize;								// Copy of the particle data on the render thread
		ResSize += MaxActiveParticleIndexSize;								// Copy of the particle indices on the render thread
		if (DynamicParameterDataOffset == 0)
		{
			ResSize += MaxActiveParticles * sizeof(FParticleBeamTrailVertex);	// The vertex data array
		}
		else
		{
			ResSize += MaxActiveParticles * sizeof(FParticleBeamTrailVertexDynamicParameter);
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
UBOOL FParticleAnimTrailEmitterInstance::FillReplayData( FDynamicEmitterReplayDataBase& OutData )
{
	if (ActiveParticles <= 0)
	{
		return FALSE;
	}

	// If the template is disabled, don't return data.
	UParticleLODLevel* LODLevel = SpriteTemplate->GetLODLevel(0);
	if ((LODLevel == NULL) || (LODLevel->bEnabled == FALSE))
	{
		return FALSE;
	}

	// This function can modify the ParticleData (changes TriangleCount of trail payloads), so we
	// we need to call it before calling the parent implementation of FillReplayData, since that
	// will memcpy the particle data to the render thread's buffer.
	DetermineVertexAndTriangleCount();

	const INT IndexCount = TriangleCount + 2;
	if (IndexCount > MAX_TRAIL_INDICES)
	{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		warnf(NAME_Warning, TEXT("ANIMTRAIL: FillReplayData failed."));
		warnf(NAME_Warning, TEXT("\tIndexCount (%d) exceeds allowed value (%d)."), IndexCount, MAX_TRAIL_INDICES);
		warnf(NAME_Warning, TEXT("\tActiveParticleCount = %d."), ActiveParticles);
		warnf(NAME_Warning, TEXT("\tTriangleCount = %d."), TriangleCount);
		warnf(NAME_Warning, TEXT("\tTrailCount = %d."), TrailCount);
		warnf(NAME_Warning, TEXT("\t%s"), Component ? Component->Template ? 
			*(Component->Template->GetPathName()) : *(Component->GetName()) : TEXT("NO COMPONENT"));
#endif
		return FALSE;
	}

	// Call parent implementation first to fill in common particle source data
	if (!FParticleEmitterInstance::FillReplayData(OutData))
	{
		return FALSE;
	}

	// Get the material instance. If there is none, or the material isn't flagged for use with particle systems, use the DefaultMaterial.
	UMaterialInterface* MaterialInst = LODLevel->RequiredModule->Material;
	if (MaterialInst == NULL || !MaterialInst->CheckMaterialUsage(MATUSAGE_BeamTrails))
	{
		MaterialInst = GEngine->DefaultMaterial;
	}

	if (TriangleCount <= 0)
	{
		if (ActiveParticles > 0)
		{
			if (!TrailTypeData->bClipSourceSegement)
			{
// 				warnf(TEXT("TRAIL: GetDynamicData -- TriangleCount == 0 (APC = %4d) for PSys %s"),
// 					ActiveParticles, 
// 					Component ? (Component->Template ? *Component->Template->GetName() : 
// 					TEXT("No Template")) : TEXT("No Component"));
#if !(FINAL_RELEASE || SHIPPING_PC_GAME)
				AWorldInfo* WorldInfo = GWorld ? GWorld->GetWorldInfo() : NULL;
				if (WorldInfo)
				{
					FString ErrorMessage = 
						FString::Printf(TEXT("ANIMTRAIL: GetDynamicData -- TriangleCount == 0 (APC = %4d) for PSys %s"),
						ActiveParticles, 
						Component ? (Component->Template ? *Component->Template->GetName() : 
						TEXT("No Template")) : TEXT("No Component"));
					FColor ErrorColor(255,0,0);
					WorldInfo->AddOnScreenDebugMessage((QWORD)((PTRINT)this), 5.0f, ErrorColor,ErrorMessage);
					debugf(*ErrorMessage);
				}
#endif	//#if !FINAL_RELEASE
			}
		}
		return FALSE;
	}

	OutData.eEmitterType = DET_AnimTrail;
	FDynamicTrailsEmitterReplayData* NewReplayData = static_cast<FDynamicTrailsEmitterReplayData*>( &OutData );

	NewReplayData->MaterialInterface = MaterialInst;
	// We never want local space for trails
	NewReplayData->bUseLocalSpace = FALSE;
	// Never use axis lock for trails
	NewReplayData->bLockAxis = FALSE;
	
	NewReplayData->MaxActiveParticleCount = MaxActiveParticles;
	NewReplayData->Sheets = TrailTypeData->SheetsPerTrail ? TrailTypeData->SheetsPerTrail : 1;

	NewReplayData->VertexCount = VertexCount;
	NewReplayData->IndexCount = TriangleCount + (2 * TrailCount);
	NewReplayData->PrimitiveCount = TriangleCount;
	NewReplayData->TrailCount = TrailCount;

	//@todo.SAS. Check for requiring DWORD sized indices?
	NewReplayData->IndexStride = sizeof(WORD);

	NewReplayData->TrailDataOffset = TypeDataOffset;

	return TRUE;
}

/**
 * Called by AnimNotify_Trails
 *
 * @param AnimNotifyData The AnimNotify_Trails which will have all of the various params on it
 */
void FParticleAnimTrailEmitterInstance::TrailsNotify(const UAnimNotify_Trails* AnimNotifyData)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimTrailNotifyTime);
	check(TrailTypeData);
	// @todo ib2merge: Chair had commented out this entire if block with this comment: doesn't serve any purpose as far as I can tell, but makes reuse with multiple trails more difficult */
	if (AnimNotifyData->ControlPointSocketName != TrailTypeData->ControlEdgeName)
	{
		return;
	}
	if (AnimNotifyData->TrailSampledData.Num() <= 0)
	{
		warnf(TEXT("TrailsNotify: ABORTING!!! NO sample data for %s"), 
			AnimNotifyData->AnimNodeSeq ? *(AnimNotifyData->AnimNodeSeq->AnimSeqName.ToString()) : TEXT("Unknown anim sequence"));
		return;
	}

	// Mark any existing trails as dead
	for (INT FindTrailIdx = 0; FindTrailIdx < ActiveParticles; FindTrailIdx++)
	{
		INT CheckStartIndex = ParticleIndices[FindTrailIdx];
		DECLARE_PARTICLE_PTR(CheckParticle, ParticleData + ParticleStride * CheckStartIndex);
		FAnimTrailTypeDataPayload* CheckTrailData = ((FAnimTrailTypeDataPayload*)((BYTE*)CheckParticle + TypeDataOffset));
		if (CheckTrailData->TrailIndex == 0)
		{
			if (TRAIL_EMITTER_IS_START(CheckTrailData->Flags))
			{
				CheckTrailData->Flags = TRAIL_EMITTER_SET_DEADTRAIL(CheckTrailData->Flags);
			}
		}
	}
	bTagTrailAsDead = FALSE;
	bTagEmitterAsDead = FALSE;

	if (AnimData.Num() == 0)
	{
		AnimData.Empty(1);
		AnimData.AddZeroed(1);
	}
	CurrentAnimDataCount = 1;
	AnimSampleTimeStep = 1.0f / AnimNotifyData->SamplesPerSecond;

	// Grab the source localtoworld
	// Update the current source data here...
	USkeletalMeshComponent* SkelMesh = AnimNotifyData->AnimNodeSeq ? AnimNotifyData->AnimNodeSeq->SkelComponent : NULL;
	if (SkelMesh == NULL)
	{
		warnf(TEXT("TrailsNotify: ABORTING!!! NO skeletal mesh for %s"), 
			AnimNotifyData->AnimNodeSeq ? *(AnimNotifyData->AnimNodeSeq->AnimSeqName.ToString()) : TEXT("Unknown anim sequence"));
		return;
	}

	FMatrix SourceMatrix = SkelMesh->GetTransformMatrix();
	
	LastOwnerData.Position = SourceMatrix.GetOrigin();
	FRotator Rotation = SourceMatrix.Rotator();
	LastOwnerData.Rotation = FQuat(Rotation);
	CurrentOwnerData.Position = LastOwnerData.Position;
	CurrentOwnerData.Rotation = LastOwnerData.Rotation;

	LastSourceUpdateTime = AnimNotifyData->CurrentTime;
	CurrentSourceUpdateTime = AnimNotifyData->CurrentTime;
	LastSourceSampleTrailIndex = 0;

	FAnimTrailSamplePoint& SamplePoint = AnimData(0);
	const FTrailSample& AnimSamplePoint = AnimNotifyData->TrailSampledData(0);
	SamplePoint.ControlPointSample.Position = SourceMatrix.TransformFVector(AnimSamplePoint.ControlPointSample);
	SamplePoint.ControlPointSample.Velocity = FVector(0.0f);
	SamplePoint.FirstEdgeSample.Position = SourceMatrix.TransformFVector(AnimSamplePoint.FirstEdgeSample);
	SamplePoint.FirstEdgeSample.Velocity = FVector(0.0f);
	SamplePoint.SecondEdgeSample.Position = SourceMatrix.TransformFVector(AnimSamplePoint.SecondEdgeSample);
	SamplePoint.SecondEdgeSample.Velocity = FVector(0.0f);
	SamplePoint.RelativeTime = AnimNotifyData->TrailSampledData(0).RelativeTime;
	SamplePoint.AnimSampleTime = AnimNotifyData->LastStartTime + SamplePoint.RelativeTime;

	LastAnimSampleTime = AnimNotifyData->CurrentTime;
	LastAnimProcessedTime = AnimNotifyData->CurrentTime;
	LastTrailIndex = 0;
}

/**
 * Called by AnimNotify_Trails
 *
 * @param AnimNotifyData The AnimNotify_Trails which will have all of the various params on it
 */
void FParticleAnimTrailEmitterInstance::TrailsNotifyTick(const UAnimNotify_Trails* AnimNotifyData)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimTrailNotifyTime);
	TrailsNotify_UpdateData(AnimNotifyData);
}

/**
 * Called by AnimNotify_Trails
 *
 * @param AnimNotifyData The AnimNotify_Trails which will have all of the various params on it
 */
void FParticleAnimTrailEmitterInstance::TrailsNotifyEnd(const UAnimNotify_Trails* AnimNotifyData)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimTrailNotifyTime);
	// If we have no notify data then prevent any further updates, also allows for early kill of systems by passing NULL
	if(AnimNotifyData == NULL)
	{
		bTagEmitterAsDead = TRUE;
	}
	else
	{
		TrailsNotify_UpdateData(AnimNotifyData);
	}
	// Mark the trail as dead??
	bTagTrailAsDead = TRUE;
}

/**
 * Called by various TrailsNotify functions to actually sample the data
 *
 * @param AnimNotifyData The AnimNotify_Trails which will have all of the various params on it
 */
void FParticleAnimTrailEmitterInstance::TrailsNotify_UpdateData(const UAnimNotify_Trails* AnimNotifyData)
{
	check(TrailTypeData);
	// @todo ib2merge: Chair had commented out this entire if block with this comment: doesn't serve any purpose as far as I can tell, but makes reuse with multiple trails more difficult */
	if (AnimNotifyData->ControlPointSocketName != TrailTypeData->ControlEdgeName)
	{
		return;
	}
	if (LastTrailIndex < AnimNotifyData->TrailSampledData.Num())
	{
		USkeletalMeshComponent* SkelMesh = AnimNotifyData->AnimNodeSeq ? AnimNotifyData->AnimNodeSeq->SkelComponent : NULL;
		if (SkelMesh == NULL)
		{
			warnf(TEXT("TrailsNotify_UpdateData: ABORTING!!! NO skeletal mesh for %s"), 
				AnimNotifyData->AnimNodeSeq ? *(AnimNotifyData->AnimNodeSeq->AnimSeqName.ToString()) : TEXT("Unknown anim sequence"));
			return;
		}
	
		FMatrix SourceMatrix = SkelMesh->GetTransformMatrix();
		FMatrix ScaleMatrix = FScaleMatrix(SourceMatrix.GetScaleVector());
		{
			CurrentOwnerData.Position = SourceMatrix.GetOrigin();
			FRotator Rotation = SourceMatrix.Rotator();
			CurrentOwnerData.Rotation = FQuat(Rotation);
		}

		if (LastTrailIndex >= 0)
		{
			INT NumSteps = AnimNotifyData->GetNumSteps(LastTrailIndex);

			if (NumSteps > 0)
			{
				FLOAT CurrEndTime = AnimNotifyData->CurrentTime + AnimNotifyData->TimeStep;

				// Don't step on the first one...
				INT CurrCount = AnimData.Num();
				if (CurrCount < (NumSteps + CurrentAnimDataCount))
				{
					AnimData.AddZeroed((NumSteps + CurrentAnimDataCount) - CurrCount);
				}

				FLOAT InterpTimeStep = 1.0f / NumSteps;
				INT NumStepsTaken = 0;
				for (INT StepIdx = 0; StepIdx < NumSteps; StepIdx++)
				{
					// Interpolation method...
					FLOAT InterpTime = (StepIdx + 1) * InterpTimeStep;

					FVector InterpPosition = Lerp<FVector>(LastOwnerData.Position, CurrentOwnerData.Position, InterpTime);
					FQuat InterpRotation = SlerpQuat(LastOwnerData.Rotation, CurrentOwnerData.Rotation, InterpTime);
					FRotator InterpRotator(InterpRotation);
					FMatrix InterpMatrix = ScaleMatrix*FRotationTranslationMatrix(InterpRotator, InterpPosition);
					
					FAnimTrailSamplePoint& SamplePoint = AnimData(StepIdx + CurrentAnimDataCount);
					SamplePoint.TimeStep = AnimNotifyData->TimeStep;

					if ((LastTrailIndex + 1 + StepIdx) < AnimNotifyData->TrailSampledData.Num())
					{
						const FTrailSample& AnimSamplePoint = AnimNotifyData->TrailSampledData(LastTrailIndex + 1 + StepIdx);

						SamplePoint.AnimCurrentTime = AnimNotifyData->CurrentTime;
						SamplePoint.RelativeTime = AnimSamplePoint.RelativeTime;
						SamplePoint.ControlPointSample.Position = InterpMatrix.TransformFVector(AnimSamplePoint.ControlPointSample);
						SamplePoint.ControlPointSample.Velocity = FVector(0.0f);
						SamplePoint.FirstEdgeSample.Position = InterpMatrix.TransformFVector(AnimSamplePoint.FirstEdgeSample);
						SamplePoint.FirstEdgeSample.Velocity = FVector(0.0f);
						SamplePoint.SecondEdgeSample.Position = InterpMatrix.TransformFVector(AnimSamplePoint.SecondEdgeSample);
						SamplePoint.SecondEdgeSample.Velocity = FVector(0.0f);
						SamplePoint.AnimSampleTime = AnimNotifyData->LastStartTime + AnimSamplePoint.RelativeTime;

						NumStepsTaken++;
					}
					else
					{
						// We should never hit this as the check was done earlier...
						debugf(TEXT("Bad TrailSampleData B in %s?"), 
							AnimNotifyData->AnimNodeSeq ? *(AnimNotifyData->AnimNodeSeq->AnimSeqName.ToString()) : TEXT("Unknown anim sequence"));
					}
				}

				LastTrailIndex += NumStepsTaken;
				LastAnimSampleTime = CurrEndTime;
				CurrentAnimDataCount += NumStepsTaken;

				LastOwnerData.Position = CurrentOwnerData.Position;
				LastOwnerData.Rotation = CurrentOwnerData.Rotation;

				CurrentSourceUpdateTime = AnimNotifyData->CurrentTime;
			}
		}
	}
}
