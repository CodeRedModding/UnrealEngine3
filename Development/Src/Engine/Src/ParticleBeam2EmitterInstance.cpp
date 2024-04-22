/*=============================================================================
	FParticleBeam2EmitterInstance.cpp: 
	Particle beam emitter instance implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EngineMaterialClasses.h"
#include "PrimitiveSceneInfo.h"

IMPLEMENT_PARTICLEEMITTERINSTANCE_TYPE(FParticleBeam2EmitterInstance);

/** Beam particle stat objects */
DECLARE_STATS_GROUP(TEXT("BeamParticles"),STATGROUP_BeamParticles);

DECLARE_DWORD_COUNTER_STAT(TEXT("Beam Particles"),STAT_BeamParticles,STATGROUP_Particles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Beam Ptcl Render Calls"),STAT_BeamParticlesRenderCalls,STATGROUP_BeamParticles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Beam Ptcls Spawned"),STAT_BeamParticlesSpawned,STATGROUP_Particles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Beam Ptcl Update Calls"),STAT_BeamParticlesUpdateCalls,STATGROUP_BeamParticles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Beam Ptcls Updated"),STAT_BeamParticlesUpdated,STATGROUP_BeamParticles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Beam Ptcls Killed"),STAT_BeamParticlesKilled,STATGROUP_Particles);
DECLARE_DWORD_COUNTER_STAT(TEXT("Beam Ptcl Tris"),STAT_BeamParticlesTrianglesRendered,STATGROUP_Particles);

DECLARE_CYCLE_STAT(TEXT("Beam Spawn Time"),STAT_BeamSpawnTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Beam FillVertex Time"),STAT_BeamFillVertexTime,STATGROUP_BeamParticles);
DECLARE_CYCLE_STAT(TEXT("Beam FillIndex Time"),STAT_BeamFillIndexTime,STATGROUP_BeamParticles);
DECLARE_CYCLE_STAT(TEXT("Beam Render Time"),STAT_BeamRenderingTime,STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("Beam Tick Time"),STAT_BeamTickTime,STATGROUP_Particles);

/*-----------------------------------------------------------------------------
	ParticleBeam2EmitterInstance
-----------------------------------------------------------------------------*/
/**
 *	Structure for beam emitter instances
 */
/** Constructor	*/
FParticleBeam2EmitterInstance::FParticleBeam2EmitterInstance() :
	FParticleEmitterInstance()
	, BeamTypeData(NULL)
	, BeamModule_Source(NULL)
	, BeamModule_Target(NULL)
	, BeamModule_Noise(NULL)
	, BeamModule_SourceModifier(NULL)
	, BeamModule_SourceModifier_Offset(-1)
	, BeamModule_TargetModifier(NULL)
	, BeamModule_TargetModifier_Offset(-1)
	, FirstEmission(TRUE)
	, LastEmittedParticleIndex(-1)
	, TickCount(0)
	, ForceSpawnCount(0)
	, BeamMethod(0)
	, BeamCount(0)
	, SourceActor(NULL)
	, SourceEmitter(NULL)
	, TargetActor(NULL)
	, TargetEmitter(NULL)
	, VertexCount(0)
	, TriangleCount(0)
{
	TextureTiles.Empty();
	UserSetSourceArray.Empty();
	UserSetSourceTangentArray.Empty();
	UserSetSourceStrengthArray.Empty();
	DistanceArray.Empty();
	TargetPointArray.Empty();
	TargetTangentArray.Empty();
	UserSetTargetStrengthArray.Empty();
	TargetPointSourceNames.Empty();
	UserSetTargetArray.Empty();
	UserSetTargetTangentArray.Empty();
	BeamTrianglesPerSheet.Empty();
}

/** Destructor	*/
FParticleBeam2EmitterInstance::~FParticleBeam2EmitterInstance()
{
	TextureTiles.Empty();
	UserSetSourceArray.Empty();
	UserSetSourceTangentArray.Empty();
	UserSetSourceStrengthArray.Empty();
	DistanceArray.Empty();
	TargetPointArray.Empty();
	TargetTangentArray.Empty();
	UserSetTargetStrengthArray.Empty();
	TargetPointSourceNames.Empty();
	UserSetTargetArray.Empty();
	UserSetTargetTangentArray.Empty();
	BeamTrianglesPerSheet.Empty();
}

// Accessors
/**
 *	Set the beam type
 *
 *	@param	NewMethod	
 */
void FParticleBeam2EmitterInstance::SetBeamType(INT NewMethod)
{
}

/**
 *	Set the tessellation factor
 *
 *	@param	NewFactor
 */
void FParticleBeam2EmitterInstance::SetTessellationFactor(FLOAT NewFactor)
{
}

/**
 *	Set the end point position
 *
 *	@param	NewEndPoint
 */
void FParticleBeam2EmitterInstance::SetEndPoint(FVector NewEndPoint)
{
	if (UserSetTargetArray.Num() < 1)
	{
		UserSetTargetArray.Add(1);

	}
	UserSetTargetArray(0) = NewEndPoint;
}

/**
 *	Set the distance
 *
 *	@param	Distance
 */
void FParticleBeam2EmitterInstance::SetDistance(FLOAT Distance)
{
}

/**
 *	Set the source point
 *
 *	@param	NewSourcePoint
 *	@param	SourceIndex			The index of the source being set
 */
void FParticleBeam2EmitterInstance::SetSourcePoint(FVector NewSourcePoint,INT SourceIndex)
{
	if (SourceIndex < 0)
		return;

	if (UserSetSourceArray.Num() < (SourceIndex + 1))
	{
		UserSetSourceArray.Add((SourceIndex + 1) - UserSetSourceArray.Num());
	}
	UserSetSourceArray(SourceIndex) = NewSourcePoint;
}

/**
 *	Set the source tangent
 *
 *	@param	NewTangentPoint		The tangent value to set it to
 *	@param	SourceIndex			The index of the source being set
 */
void FParticleBeam2EmitterInstance::SetSourceTangent(FVector NewTangentPoint,INT SourceIndex)
{
	if (SourceIndex < 0)
		return;

	if (UserSetSourceTangentArray.Num() < (SourceIndex + 1))		
	{
		UserSetSourceTangentArray.Add((SourceIndex + 1) - UserSetSourceTangentArray.Num());
	}
	UserSetSourceTangentArray(SourceIndex) = NewTangentPoint;
}

/**
 *	Set the source strength
 *
 *	@param	NewSourceStrength	The source strenght to set it to
 *	@param	SourceIndex			The index of the source being set
 */
void FParticleBeam2EmitterInstance::SetSourceStrength(FLOAT NewSourceStrength,INT SourceIndex)
{
	if (SourceIndex < 0)
		return;

	if (UserSetSourceStrengthArray.Num() < (SourceIndex + 1))
	{
		UserSetSourceStrengthArray.Add((SourceIndex + 1) - UserSetSourceStrengthArray.Num());
	}
	UserSetSourceStrengthArray(SourceIndex) = NewSourceStrength;
}

/**
 *	Set the target point
 *
 *	@param	NewTargetPoint		The target point to set it to
 *	@param	TargetIndex			The index of the target being set
 */
void FParticleBeam2EmitterInstance::SetTargetPoint(FVector NewTargetPoint,INT TargetIndex)
{
	if (TargetIndex < 0)
		return;

	if (UserSetTargetArray.Num() < (TargetIndex + 1))
	{
		UserSetTargetArray.Add((TargetIndex + 1) - UserSetTargetArray.Num());
	}
	UserSetTargetArray(TargetIndex) = NewTargetPoint;
}

/**
 *	Set the target tangent
 *
 *	@param	NewTangentPoint		The tangent to set it to
 *	@param	TargetIndex			The index of the target being set
 */
void FParticleBeam2EmitterInstance::SetTargetTangent(FVector NewTangentPoint,INT TargetIndex)
{
	if (TargetIndex < 0)
		return;

	if (UserSetTargetTangentArray.Num() < (TargetIndex + 1))
	{
		UserSetTargetTangentArray.Add((TargetIndex + 1) - UserSetTargetTangentArray.Num());
	}
	UserSetTargetTangentArray(TargetIndex) = NewTangentPoint;
}

/**
 *	Set the target strength
 *
 *	@param	NewTargetStrength	The strength to set it ot
 *	@param	TargetIndex			The index of the target being set
 */
void FParticleBeam2EmitterInstance::SetTargetStrength(FLOAT NewTargetStrength,INT TargetIndex)
{
	if (TargetIndex < 0)
		return;

	if (UserSetTargetStrengthArray.Num() < (TargetIndex + 1))
	{
		UserSetTargetStrengthArray.Add((TargetIndex + 1) - UserSetTargetStrengthArray.Num());
	}
	UserSetTargetStrengthArray(TargetIndex) = NewTargetStrength;
}

/**
 *	Initialize the parameters for the structure
 *
 *	@param	InTemplate		The ParticleEmitter to base the instance on
 *	@param	InComponent		The owning ParticleComponent
 *	@param	bClearResources	If TRUE, clear all resource data
 */
void FParticleBeam2EmitterInstance::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, UBOOL bClearResources)
{
	FParticleEmitterInstance::InitParameters(InTemplate, InComponent, bClearResources);

	UParticleLODLevel* LODLevel	= InTemplate->GetLODLevel(0);
	check(LODLevel);
	BeamTypeData = CastChecked<UParticleModuleTypeDataBeam2>(LODLevel->TypeDataModule);
	check(BeamTypeData);

	//@todo. Determine if we need to support local space.
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		LODLevel->RequiredModule->bUseLocalSpace	= FALSE;
	}

	BeamModule_Source			= NULL;
	BeamModule_Target			= NULL;
	BeamModule_Noise			= NULL;
	BeamModule_SourceModifier	= NULL;
	BeamModule_TargetModifier	= NULL;

	// Always have at least one beam
	if (BeamTypeData->MaxBeamCount == 0)
	{
		BeamTypeData->MaxBeamCount	= 1;
	}

	BeamCount					= BeamTypeData->MaxBeamCount;
	FirstEmission				= TRUE;
	LastEmittedParticleIndex	= -1;
	TickCount					= 0;
	ForceSpawnCount				= 0;

	BeamMethod					= BeamTypeData->BeamMethod;

	TextureTiles.Empty();
	TextureTiles.AddItem(BeamTypeData->TextureTile);

	UserSetSourceArray.Empty();
	UserSetSourceTangentArray.Empty();
	UserSetSourceStrengthArray.Empty();
	DistanceArray.Empty();
	TargetPointArray.Empty();
	TargetPointSourceNames.Empty();
	UserSetTargetArray.Empty();
	UserSetTargetTangentArray.Empty();
	UserSetTargetStrengthArray.Empty();

	// Resolve any actors...
	ResolveSource();
	ResolveTarget();
}

/**
 *	Initialize the instance
 */
void FParticleBeam2EmitterInstance::Init()
{
	// Setup the modules prior to initializing...
	SetupBeamModules();
	SetupBeamModifierModules();
	FParticleEmitterInstance::Init();
	SetupBeamModifierModulesOffsets();
}

/**
 *	Tick the instance.
 *
 *	@param	DeltaTime			The time slice to use
 *	@param	bSuppressSpawning	If TRUE, do not spawn during Tick
 */
void FParticleBeam2EmitterInstance::Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning)
{
	SCOPE_CYCLE_COUNTER(STAT_BeamTickTime);
	if (Component)
	{
		UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
		check(LODLevel);	// TTP #33141

		// Handle EmitterTime setup, looping, etc.
		FLOAT EmitterDelay = Tick_EmitterTimeSetup(DeltaTime, LODLevel);

		// Kill before the spawn... Otherwise, we can get 'flashing'
		KillParticles();

		// If not suppressing spawning...
		if (!bHaltSpawning && !bSuppressSpawning && (EmitterTime >= 0.0f))
		{
			if ((LODLevel->RequiredModule->EmitterLoops == 0) || 
				(LoopCount < LODLevel->RequiredModule->EmitterLoops) ||
				(SecondsSinceCreation < (EmitterDuration * LODLevel->RequiredModule->EmitterLoops)))
			{
				// For beams, we probably want to ignore the SpawnRate distribution,
				// and focus strictly on the BurstList...
				FLOAT SpawnRate = 0.0f;
				// Figure out spawn rate for this tick.
				SpawnRate = LODLevel->SpawnModule->Rate.GetValue(EmitterTime, Component);
				// Take Bursts into account as well...
				INT		Burst		= 0;
				FLOAT	BurstTime	= GetCurrentBurstRateOffset(DeltaTime, Burst);
				SpawnRate += BurstTime;

				// Spawn new particles...

				//@todo. Fix the issue of 'blanking' beams when the count drops...
				// This is a temporary hack!
				if ((ActiveParticles < BeamCount) && (SpawnRate <= 0.0f))
				{
					// Force the spawn of a single beam...
					SpawnRate = 1.0f / DeltaTime;
				}

				// Force beams if the emitter is marked "AlwaysOn"
				if ((ActiveParticles < BeamCount) && BeamTypeData->bAlwaysOn)
				{
					Burst		= BeamCount;
					if (DeltaTime > KINDA_SMALL_NUMBER)
					{
						BurstTime	 = Burst / DeltaTime;
						SpawnRate	+= BurstTime;
					}
				}

				if (SpawnRate > 0.f)
				{
					SpawnFraction = Spawn(SpawnFraction, SpawnRate, DeltaTime, Burst, BurstTime);
				}
			}
		}

		// Reset particle data
		ResetParticleParameters(DeltaTime, STAT_BeamParticlesUpdated);

		// Not really necessary as beams do not LOD at the moment, but for consistency...
		CurrentMaterial = LODLevel->RequiredModule->Material;

		Tick_ModulePreUpdate(DeltaTime, LODLevel);
		Tick_ModuleUpdate(DeltaTime, LODLevel);
		Tick_ModulePostUpdate(DeltaTime, LODLevel);

		// Calculate bounding box and simulate velocity.
		UpdateBoundingBox(DeltaTime);

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
	INC_DWORD_STAT(STAT_BeamParticlesUpdateCalls);
}

/**
 *	Tick sub-function that handles module post updates
 *
 *	@param	DeltaTime			The current time slice
 *	@param	CurrentLODLevel		The current LOD level for the instance
 */
void FParticleBeam2EmitterInstance::Tick_ModulePostUpdate(FLOAT DeltaTime, UParticleLODLevel* CurrentLODLevel)
{
	UParticleModuleTypeDataBase* TypeData = Cast<UParticleModuleTypeDataBase>(CurrentLODLevel->TypeDataModule);
	if (TypeData)
	{
		// The order of the update here is VERY important
		UINT* Offset;
		if (BeamModule_Source && BeamModule_Source->bEnabled)
		{
			Offset = ModuleOffsetMap.Find(BeamModule_Source);
			BeamModule_Source->Update(this, Offset ? *Offset : 0, DeltaTime);
		}
		if (BeamModule_SourceModifier && BeamModule_SourceModifier->bEnabled)
		{
			INT TempOffset = BeamModule_SourceModifier_Offset;
			BeamModule_SourceModifier->Update(this, TempOffset, DeltaTime);
		}
		if (BeamModule_Target && BeamModule_Target->bEnabled)
		{
			Offset = ModuleOffsetMap.Find(BeamModule_Target);
			BeamModule_Target->Update(this, Offset ? *Offset : 0, DeltaTime);
		}
		if (BeamModule_TargetModifier && BeamModule_TargetModifier->bEnabled)
		{
			INT TempOffset = BeamModule_TargetModifier_Offset;
			BeamModule_TargetModifier->Update(this, TempOffset, DeltaTime);
		}
		if (BeamModule_Noise && BeamModule_Noise->bEnabled)
		{
			Offset = ModuleOffsetMap.Find(BeamModule_Noise);
			BeamModule_Noise->Update(this, Offset ? *Offset : 0, DeltaTime);
		}

		FParticleEmitterInstance::Tick_ModulePostUpdate(DeltaTime, CurrentLODLevel);
	}
}

/**
 *	Set the LOD to the given index
 *
 *	@param	InLODIndex			The index of the LOD to set as current
 *	@param	bInFullyProcess		If TRUE, process burst lists, etc.
 */
void FParticleBeam2EmitterInstance::SetCurrentLODIndex(INT InLODIndex, UBOOL bInFullyProcess)
{
	UBOOL bDifferent = (InLODIndex != CurrentLODLevelIndex);
	FParticleEmitterInstance::SetCurrentLODIndex(InLODIndex, bInFullyProcess);

	// Setup the beam modules!
	BeamTypeData = LOD_BeamTypeData(CurrentLODLevelIndex);
	BeamModule_Source = LOD_BeamModule_Source(CurrentLODLevelIndex);
	BeamModule_Target = LOD_BeamModule_Target(CurrentLODLevelIndex);
	BeamModule_Noise = LOD_BeamModule_Noise(CurrentLODLevelIndex);
	BeamModule_SourceModifier = LOD_BeamModule_SourceModifier(CurrentLODLevelIndex);
	BeamModule_TargetModifier= LOD_BeamModule_TargetModifier(CurrentLODLevelIndex);
}

/**
 *	Update the bounding box for the emitter
 *
 *	@param	DeltaTime		The time slice to use
 */
void FParticleBeam2EmitterInstance::UpdateBoundingBox(FLOAT DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleUpdateBounds);
	if (Component)
	{
		UBOOL bUpdateBox = ((Component->bWarmingUp == FALSE) && (Component->bSkipBoundsUpdate == FALSE) &&
			(Component->Template != NULL) && (Component->Template->bUseFixedRelativeBoundingBox == FALSE));
		FLOAT MaxSizeScale	= 1.0f;
		if (bUpdateBox)
		{
			ParticleBoundingBox.Init();

			//@todo. Currently, we don't support UseLocalSpace for beams
			//if (Template->UseLocalSpace == FALSE) 
			{
				ParticleBoundingBox += Component->LocalToWorld.GetOrigin();
			}
		}

		FVector	NoiseMin(0.0f);
		FVector NoiseMax(0.0f);
		// Noise points have to be taken into account...
		if (BeamModule_Noise)
		{
			BeamModule_Noise->GetNoiseRange(NoiseMin, NoiseMax);
		}

		// Take scale into account as well
		FVector Scale = FVector(1.0f, 1.0f, 1.0f);
		Scale *= Component->Scale * Component->Scale3D;
		AActor* Actor = Component->GetOwner();
		if (Actor && !Component->AbsoluteScale)
		{
			Scale *= Actor->DrawScale * Actor->DrawScale3D;
		}

		// Take each particle into account
		for (INT i=0; i<ActiveParticles; i++)
		{
			DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[i]);

			INT						CurrentOffset		= TypeDataOffset;
			FBeam2TypeDataPayload*	BeamData			= NULL;
			FVector*				InterpolatedPoints	= NULL;
			FLOAT*					NoiseRate			= NULL;
			FLOAT*					NoiseDelta			= NULL;
			FVector*				TargetNoisePoints	= NULL;
			FVector*				NextNoisePoints		= NULL;
			FLOAT*					TaperValues			= NULL;
			FLOAT*					NoiseDistanceScale	= NULL;
			FBeamParticleModifierPayloadData* SourceModifier = NULL;
			FBeamParticleModifierPayloadData* TargetModifier = NULL;

			BeamTypeData->GetDataPointers(this, (const BYTE*)Particle, CurrentOffset, 
				BeamData, InterpolatedPoints, NoiseRate, NoiseDelta, TargetNoisePoints, 
				NextNoisePoints, TaperValues, NoiseDistanceScale,
				SourceModifier, TargetModifier);

			// Do linear integrator and update bounding box
			Particle->OldLocation = Particle->Location;
			Particle->Location	+= DeltaTime * Particle->Velocity;
			Particle->Rotation	+= DeltaTime * Particle->RotationRate;
			FVector Size = Particle->Size * Scale;
			if (bUpdateBox)
			{
				ParticleBoundingBox += Particle->Location;
				ParticleBoundingBox += Particle->Location + NoiseMin;
				ParticleBoundingBox += Particle->Location + NoiseMax;
				ParticleBoundingBox += BeamData->SourcePoint;
				ParticleBoundingBox += BeamData->SourcePoint + NoiseMin;
				ParticleBoundingBox += BeamData->SourcePoint + NoiseMax;
				ParticleBoundingBox += BeamData->TargetPoint;
				ParticleBoundingBox += BeamData->TargetPoint + NoiseMin;
				ParticleBoundingBox += BeamData->TargetPoint + NoiseMax;
			}

			// Do angular integrator, and wrap result to within +/- 2 PI
			Particle->Rotation	 = appFmod(Particle->Rotation, 2.f*(FLOAT)PI);
			MaxSizeScale		 = Max(MaxSizeScale, Size.GetAbsMax()); //@todo particles: this does a whole lot of compares that can be avoided using SSE/ Altivec.
		}
		if (bUpdateBox)
		{
			ParticleBoundingBox = ParticleBoundingBox.ExpandBy(MaxSizeScale);
		}

		//@todo. Transform bounding box into world space if the emitter uses a local space coordinate system.
		/***
		if (Template->UseLocalSpace) 
		{
		ParticleBoundingBox = ParticleBoundingBox.TransformBy(Component->LocalToWorld);
		}
		***/
	}
}

/**
 *	Retrieved the per-particle bytes that this emitter type requires.
 *
 *	@return	UINT	The number of required bytes for particles in the instance
 */
UINT FParticleBeam2EmitterInstance::RequiredBytes()
{
	UINT uiBytes = FParticleEmitterInstance::RequiredBytes();

	// Flag bits indicating particle 
	uiBytes += sizeof(INT);

	return uiBytes;
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
FLOAT FParticleBeam2EmitterInstance::Spawn(FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime, INT Burst, FLOAT BurstTime)
{
	SCOPE_CYCLE_COUNTER(STAT_BeamSpawnTime);

	FLOAT SafetyLeftover = OldLeftover;
	FLOAT	NewLeftover = OldLeftover + DeltaTime * Rate;

	// Ensure continous spawning... lots of fiddling.
	INT		Number		= appFloor(NewLeftover);
	FLOAT	Increment	= 1.f / Rate;
	FLOAT	StartTime	= DeltaTime + OldLeftover * Increment - Increment;
	NewLeftover			= NewLeftover - Number;

	// Always match the burst at a minimum
	if (Number < Burst)
	{
		Number = Burst;
	}

	// Account for burst time simulation
	if (BurstTime > KINDA_SMALL_NUMBER)
	{
		NewLeftover -= BurstTime / Burst;
		NewLeftover	= Clamp<FLOAT>(NewLeftover, 0, NewLeftover);
	}

	// Force a beam
	UBOOL bNoLivingParticles = FALSE;
	if (ActiveParticles == 0)
	{
		bNoLivingParticles = TRUE;
		if (Number == 0)
			Number = 1;
	}

	// Don't allow more than BeamCount beams...
	if (Number + ActiveParticles > BeamCount)
	{
		Number	= BeamCount - ActiveParticles;
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
		UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
		check(LODLevel);

		// Spawn particles.
		for (INT i=0; i<Number; i++)
		{
			INT iParticleIndex = ParticleIndices[ActiveParticles];

			DECLARE_PARTICLE_PTR(pkParticle, ParticleData + ParticleStride * iParticleIndex);

			FLOAT SpawnTime = StartTime - i * Increment;

			PreSpawn(pkParticle);
			for (INT n = 0; n < LODLevel->SpawnModules.Num(); n++)
			{
				UParticleModule* SpawnModule	= LODLevel->SpawnModules(n);
				if (SpawnModule->bEnabled)
				{
					UINT* Offset = ModuleOffsetMap.Find(SpawnModule);
					SpawnModule->Spawn(this, Offset ? *Offset : 0, SpawnTime);
				}
			}

			// The order of the Spawn here is VERY important as the modules may(will) depend on it occuring as such.
			UINT* Offset;
			if (BeamModule_Source && BeamModule_Source->bEnabled)
			{
				Offset = ModuleOffsetMap.Find(BeamModule_Source);
				BeamModule_Source->Spawn(this, Offset ? *Offset : 0, DeltaTime);
			}
			if (BeamModule_SourceModifier && BeamModule_SourceModifier->bEnabled)
			{
				INT Offset = BeamModule_SourceModifier_Offset;
				BeamModule_SourceModifier->Spawn(this, Offset, DeltaTime);
			}
			if (BeamModule_Target && BeamModule_Target->bEnabled)
			{
				Offset = ModuleOffsetMap.Find(BeamModule_Target);
				BeamModule_Target->Spawn(this, Offset ? *Offset : 0, DeltaTime);
			}
			if (BeamModule_TargetModifier && BeamModule_TargetModifier->bEnabled)
			{
				INT Offset = BeamModule_TargetModifier_Offset;
				BeamModule_TargetModifier->Spawn(this, Offset, DeltaTime);
			}
			if (BeamModule_Noise && BeamModule_Noise->bEnabled)
			{
				Offset = ModuleOffsetMap.Find(BeamModule_Noise);
				BeamModule_Noise->Spawn(this, Offset ? *Offset : 0, DeltaTime);
			}
			if (LODLevel->TypeDataModule)
			{
				//@todo. Need to track TypeData offset into payload!
				LODLevel->TypeDataModule->Spawn(this, TypeDataOffset, SpawnTime);
			}

			PostSpawn(pkParticle, 1.f - FLOAT(i+1) / FLOAT(Number), SpawnTime);

			ActiveParticles++;

			INC_DWORD_STAT(STAT_BeamParticlesSpawned);

			LastEmittedParticleIndex = iParticleIndex;
		}

		if (ForceSpawnCount > 0)
		{
			ForceSpawnCount = 0;
		}

		INC_DWORD_STAT_BY(STAT_BeamParticles, ActiveParticles);

		return NewLeftover;
	}

	INC_DWORD_STAT_BY(STAT_BeamParticles, ActiveParticles);

	return SafetyLeftover;
}

/**
 *	Handle any pre-spawning actions required for particles
 *
 *	@param	Particle	The particle being spawned.
 */
void FParticleBeam2EmitterInstance::PreSpawn(FBaseParticle* Particle)
{
	FParticleEmitterInstance::PreSpawn(Particle);
	if (BeamTypeData)
	{
		BeamTypeData->PreSpawn(this, Particle);
	}
}

/**
 *	Kill off any dead particles. (Remove them from the active array)
 */
void FParticleBeam2EmitterInstance::KillParticles()
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
				ParticleIndices[i]	= ParticleIndices[ActiveParticles-1];
				ParticleIndices[ActiveParticles-1]	= CurrentIndex;
				ActiveParticles--;

				INC_DWORD_STAT(STAT_BeamParticlesKilled);
			}
		}
	}
}

/**
 *	Setup the beam module pointers
 */
void FParticleBeam2EmitterInstance::SetupBeamModules()
{
	// Beams are a special case... 
	// We don't want standard Spawn/Update calls occurring on Beam-type modules.
	INT LODCount = SpriteTemplate->LODLevels.Num();

	LOD_BeamTypeData.Empty(LODCount);
	LOD_BeamTypeData.AddZeroed(LODCount);
	LOD_BeamModule_Source.Empty(LODCount);
	LOD_BeamModule_Source.AddZeroed(LODCount);
	LOD_BeamModule_Target.Empty(LODCount);
	LOD_BeamModule_Target.AddZeroed(LODCount);
	LOD_BeamModule_Noise.Empty(LODCount);
	LOD_BeamModule_Noise.AddZeroed(LODCount);

	for (INT LODIdx = 0; LODIdx < LODCount; LODIdx++)
	{
		UParticleLODLevel* LODLevel	= SpriteTemplate->GetLODLevel(LODIdx);
		check(LODLevel);

		LOD_BeamTypeData(LODIdx) = CastChecked<UParticleModuleTypeDataBeam2>(LODLevel->TypeDataModule);
		check(LOD_BeamTypeData(LODIdx));
		if (LODIdx == 0)
		{
			BeamTypeData = LOD_BeamTypeData(LODIdx);
		}

		// Go over all the modules in the LOD level
		for (INT ii = 0; ii < LODLevel->Modules.Num(); ii++)
		{
			UParticleModule* CheckModule = LODLevel->Modules(ii);
			if ((CheckModule->GetModuleType() == EPMT_Beam) && (CheckModule->bEnabled == TRUE))
			{
				UBOOL bRemove = FALSE;

				if (CheckModule->IsA(UParticleModuleBeamSource::StaticClass()))
				{
					if (LOD_BeamModule_Source(LODIdx))
					{
						debugf(TEXT("Warning: Multiple beam source modules!"));
					}
					else
					{
						LOD_BeamModule_Source(LODIdx) = Cast<UParticleModuleBeamSource>(CheckModule);
						if (LODIdx == 0)
						{
							BeamModule_Source = LOD_BeamModule_Source(LODIdx);
						}
					}
					bRemove = TRUE;
				}
				else if (CheckModule->IsA(UParticleModuleBeamTarget::StaticClass()))
				{
					if (LOD_BeamModule_Target(LODIdx))
					{
						debugf(TEXT("Warning: Multiple beam Target modules!"));
					}
					else
					{
						LOD_BeamModule_Target(LODIdx) = Cast<UParticleModuleBeamTarget>(CheckModule);
						if (LODIdx == 0)
						{
							BeamModule_Target = LOD_BeamModule_Target(LODIdx);
						}
					}
					bRemove = TRUE;
				}
				else if (CheckModule->IsA(UParticleModuleBeamNoise::StaticClass()))
				{
					if (LOD_BeamModule_Noise(LODIdx))
					{
						debugf(TEXT("Warning: Multiple beam Noise modules!"));
					}
					else
					{
						LOD_BeamModule_Noise(LODIdx) = Cast<UParticleModuleBeamNoise>(CheckModule);
						if (LODIdx == 0)
						{
							BeamModule_Noise = LOD_BeamModule_Noise(LODIdx);
						}
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
}

/**
*	Setup the beam modifer module pointers
*/
void FParticleBeam2EmitterInstance::SetupBeamModifierModules()
{
	// Beams are a special case... 
	// We don't want standard Spawn/Update calls occuring on Beam-type modules.
	INT LODCount = SpriteTemplate->LODLevels.Num();

	LOD_BeamModule_SourceModifier.Empty(LODCount);
	LOD_BeamModule_SourceModifier.AddZeroed(LODCount);
	LOD_BeamModule_TargetModifier.Empty(LODCount);
	LOD_BeamModule_TargetModifier.AddZeroed(LODCount);

	for (INT LODIdx = 0; LODIdx < LODCount; LODIdx++)
	{
		UParticleLODLevel* LODLevel	= SpriteTemplate->GetLODLevel(LODIdx);
		check(LODLevel);

		// Go over all the modules in the LOD level
		for (INT ii = 0; ii < LODLevel->Modules.Num(); ii++)
		{
			UParticleModule* CheckModule = LODLevel->Modules(ii);
			if (CheckModule->GetModuleType() == EPMT_Beam)
			{
				UBOOL bRemove = FALSE;

				if (CheckModule->IsA(UParticleModuleBeamModifier::StaticClass()))
				{
					UParticleModuleBeamModifier* ModifyModule = Cast<UParticleModuleBeamModifier>(CheckModule);
					if (ModifyModule->PositionOptions.bModify || ModifyModule->TangentOptions.bModify || ModifyModule->StrengthOptions.bModify)
					{
						if (ModifyModule->ModifierType == PEB2MT_Source)
						{
							LOD_BeamModule_SourceModifier(LODIdx) = ModifyModule;
							bRemove = TRUE;
							if (LODIdx == 0)
							{
								// Offset will be setup in a different function
								BeamModule_SourceModifier = LOD_BeamModule_SourceModifier(LODIdx);
							}
						}
						else if (ModifyModule->ModifierType == PEB2MT_Target)
						{
							LOD_BeamModule_TargetModifier(LODIdx) = ModifyModule;
							bRemove = TRUE;
							if (LODIdx == 0)
							{
								// Offset will be setup in a different function
								BeamModule_TargetModifier = LOD_BeamModule_TargetModifier(LODIdx);
							}
						}
					}
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
}

/**
 *	Setup the offsets to the BeamModifier modules...
 *	This must be done after the base Init call as that inserts modules into the offset map.
 */
void FParticleBeam2EmitterInstance::SetupBeamModifierModulesOffsets()
{
	// Beams are a special case... 
	// We don't want standard Spawn/Update calls occuring on Beam-type modules.
	INT LODCount = SpriteTemplate->LODLevels.Num();

	check(SpriteTemplate->LODLevels.Num() > 0);
	UParticleLODLevel* LODLevel	= SpriteTemplate->GetLODLevel(0);
	check(LODLevel);
	if (LOD_BeamModule_SourceModifier.Num() > 0)
	{
		UINT* Offset = ModuleOffsetMap.Find(LOD_BeamModule_SourceModifier(0));
		if (Offset != NULL)
		{
			BeamModule_SourceModifier_Offset = (INT)(*Offset);
		}
	}
	if (LOD_BeamModule_TargetModifier.Num() > 0)
	{
		UINT* Offset = ModuleOffsetMap.Find(LOD_BeamModule_TargetModifier(0));
		if (Offset != NULL)
		{
			BeamModule_TargetModifier_Offset = (INT)(*Offset);
		}
	}
}

/**
 *	Resolve the source for the beam
 */
void FParticleBeam2EmitterInstance::ResolveSource()
{
	if (BeamModule_Source)
	{
		if (BeamModule_Source->SourceName != NAME_None)
		{
			switch (BeamModule_Source->SourceMethod)
			{
			case PEB2STM_Actor:
				{
					FParticleSysParam Param;
					for (INT i = 0; i < Component->InstanceParameters.Num(); i++)
					{
						Param = Component->InstanceParameters(i);
						if (Param.Name == BeamModule_Source->SourceName)
						{
							SourceActor = Param.Actor;
							break;
						}
					}
				}
				break;
			case PEB2STM_Emitter:
			case PEB2STM_Particle:
				if (SourceEmitter == NULL)
				{
					for (INT ii = 0; ii < Component->EmitterInstances.Num(); ii++)
					{
						FParticleEmitterInstance* pkEmitInst = Component->EmitterInstances(ii);
						if (pkEmitInst && (pkEmitInst->SpriteTemplate->EmitterName == BeamModule_Source->SourceName))
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
 *	Resolve the target for the beam
 */
void FParticleBeam2EmitterInstance::ResolveTarget()
{
	if (BeamModule_Target)
	{
		if (BeamModule_Target->TargetName != NAME_None)
		{
			switch (BeamModule_Target->TargetMethod)
			{
			case PEB2STM_Actor:
				{
					FParticleSysParam Param;
					for (INT i = 0; i < Component->InstanceParameters.Num(); i++)
					{
						Param = Component->InstanceParameters(i);
						if (Param.Name == BeamModule_Target->TargetName)
						{
							TargetActor = Param.Actor;
							break;
						}
					}
				}
				break;
			case PEB2STM_Emitter:
				if (TargetEmitter == NULL)
				{
					for (INT ii = 0; ii < Component->EmitterInstances.Num(); ii++)
					{
						FParticleEmitterInstance* pkEmitInst = Component->EmitterInstances(ii);
						if (pkEmitInst && (pkEmitInst->SpriteTemplate->EmitterName == BeamModule_Target->TargetName))
						{
							TargetEmitter = pkEmitInst;
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
 *	Determine the vertex and triangle counts for the emitter
 */
void FParticleBeam2EmitterInstance::DetermineVertexAndTriangleCount()
{
	// Need to determine # tris per beam...
	INT VerticesToRender = 0;
	INT TrianglesToRender = 0;

	check(BeamTypeData);
	INT Sheets = BeamTypeData->Sheets ? BeamTypeData->Sheets : 1;

	BeamTrianglesPerSheet.Empty(ActiveParticles);
	BeamTrianglesPerSheet.AddZeroed(ActiveParticles);
	for (INT i = 0; i < ActiveParticles; i++)
	{
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[i]);

		INT						CurrentOffset		= TypeDataOffset;
		FBeam2TypeDataPayload*	BeamData			= NULL;
		FVector*				InterpolatedPoints	= NULL;
		FLOAT*					NoiseRate			= NULL;
		FLOAT*					NoiseDelta			= NULL;
		FVector*				TargetNoisePoints	= NULL;
		FVector*				NextNoisePoints		= NULL;
		FLOAT*					TaperValues			= NULL;
		FLOAT*					NoiseDistanceScale	= NULL;
		FBeamParticleModifierPayloadData* SourceModifier = NULL;
		FBeamParticleModifierPayloadData* TargetModifier = NULL;

		BeamTypeData->GetDataPointers(this, (const BYTE*)Particle, CurrentOffset, 
			BeamData, InterpolatedPoints, NoiseRate, NoiseDelta, TargetNoisePoints, 
			NextNoisePoints, TaperValues, NoiseDistanceScale,
			SourceModifier, TargetModifier);

		BeamTrianglesPerSheet(i) = BeamData->TriangleCount;

		// Take sheets into account
		INT LocalTriangles = 0;
		if (BeamData->TriangleCount > 0)
		{
			// Stored triangle count is per sheet...
			LocalTriangles	+= BeamData->TriangleCount * Sheets;
			VerticesToRender += (BeamData->TriangleCount + 2) * Sheets;
			// 4 Degenerates Per Sheet (except for last one)
			LocalTriangles	+= (Sheets - 1) * 4;
			TrianglesToRender += LocalTriangles;
			// Multiple beams?
			if (i < (ActiveParticles - 1))
			{
				// 4 Degenerates Per Beam (except for last one)
				TrianglesToRender	+= 4;
			}
		}
	}

	VertexCount = VerticesToRender;
	TriangleCount = TrianglesToRender;
}



/**
 *	Retrieves the dynamic data for the emitter
 *	
 *	@param	bSelected					Whether the emitter is selected in the editor
 *
 *	@return	FDynamicEmitterDataBase*	The dynamic data, or NULL if it shouldn't be rendered
 */
FDynamicEmitterDataBase* FParticleBeam2EmitterInstance::GetDynamicData(UBOOL bSelected)
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	if (IsDynamicDataRequired(LODLevel) == FALSE)
	{
		return NULL;
	}

	//@todo.SAS. Have this call the UpdateDynamicData function to reduce duplicate code!!!
	//@SAS. This removes the need for the assertion in the actual render call...
	if ((ActiveParticles > FDynamicBeam2EmitterData::MaxBeams) ||	// TTP #33330 - Max of 2048 beams from a single emitter
		(ParticleStride >
			((FDynamicBeam2EmitterData::MaxInterpolationPoints + 2) * (sizeof(FVector) + sizeof(FLOAT))) + 
			(FDynamicBeam2EmitterData::MaxNoiseFrequency * (sizeof(FVector) + sizeof(FVector) + sizeof(FLOAT) + sizeof(FLOAT)))
		)	// TTP #33330 - Max of 10k per beam (includes interpolation points, noise, etc.)
		)
	{
#if !(FINAL_RELEASE || SHIPPING_PC_GAME)
		AWorldInfo* WorldInfo = GWorld ? GWorld->GetWorldInfo() : NULL;
		if (WorldInfo)
		{
			FString ErrorMessage = 
				FString::Printf(TEXT("BeamEmitter with too much data: %s"),
					Component ? 
						Component->Template ? 
							*(Component->Template->GetName()) :
							TEXT("No template") :
						TEXT("No component"));
			FColor ErrorColor(255,0,0);
			WorldInfo->AddOnScreenDebugMessage((QWORD)((PTRINT)this), 5.0f, ErrorColor,ErrorMessage);
			debugf(*ErrorMessage);
		}
#endif	//#if !FINAL_RELEASE
		return NULL;
	}

	// Allocate the dynamic data
	FDynamicBeam2EmitterData* NewEmitterData = ::new FDynamicBeam2EmitterData(LODLevel->RequiredModule);
	{
		SCOPE_CYCLE_COUNTER(STAT_ParticleMemTime);
		INC_DWORD_STAT(STAT_DynamicEmitterCount);
		INC_DWORD_STAT(STAT_DynamicBeamCount);
		INC_DWORD_STAT_BY(STAT_DynamicEmitterMem, sizeof(FDynamicBeam2EmitterData));
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
UBOOL FParticleBeam2EmitterInstance::UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected)
{
	if (ActiveParticles <= 0)
	{
		return FALSE;
	}

	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	if ((LODLevel == NULL) || (LODLevel->bEnabled == FALSE))
	{
		return FALSE;
	}

	//@SAS. This removes the need for the assertion in the actual render call...
	if ((ActiveParticles > FDynamicBeam2EmitterData::MaxBeams) ||	// TTP #33330 - Max of 2048 beams from a single emitter
		(ParticleStride >
			((FDynamicBeam2EmitterData::MaxInterpolationPoints + 2) * (sizeof(FVector) + sizeof(FLOAT))) + 
			(FDynamicBeam2EmitterData::MaxNoiseFrequency * (sizeof(FVector) + sizeof(FVector) + sizeof(FLOAT) + sizeof(FLOAT)))
		)	// TTP #33330 - Max of 10k per beam (includes interpolation points, noise, etc.)
		)
	{
#if !(FINAL_RELEASE || SHIPPING_PC_GAME)
		AWorldInfo* WorldInfo = GWorld ? GWorld->GetWorldInfo() : NULL;
		if (WorldInfo)
		{
			FString ErrorMessage = 
				FString::Printf(TEXT("BeamEmitter with too much data: %s"),
					Component ? 
						Component->Template ? 
							*(Component->Template->GetName()) :
							TEXT("No template") :
						TEXT("No component"));
			FColor ErrorColor(255,0,0);
			WorldInfo->AddOnScreenDebugMessage((QWORD)((PTRINT)this), 5.0f, ErrorColor,ErrorMessage);
			debugf(*ErrorMessage);
		}
#endif	//#if !FINAL_RELEASE
		return FALSE;
	}

	checkf((DynamicData->GetSource().eEmitterType == DET_Beam2), TEXT("Beam2::UpdateDynamicData> Invalid DynamicData type!"));

	// Now fill in the source data
	FDynamicBeam2EmitterData* BeamDynamicData = (FDynamicBeam2EmitterData*)DynamicData;
	if( !FillReplayData( BeamDynamicData->Source ) )
	{
		return FALSE;
	}

	// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
	BeamDynamicData->Init( bSelected );

	return TRUE;
}

/**
 *	Retrieves replay data for the emitter
 *
 *	@return	The replay data, or NULL on failure
 */
FDynamicEmitterReplayDataBase* FParticleBeam2EmitterInstance::GetReplayData()
{
	FDynamicEmitterReplayDataBase* NewEmitterReplayData = ::new FDynamicBeam2EmitterReplayData();
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
void FParticleBeam2EmitterInstance::GetAllocatedSize(INT& OutNum, INT& OutMax)
{
	INT Size = sizeof(FParticleBeam2EmitterInstance);
	INT ActiveParticleDataSize = (ParticleData != NULL) ? (ActiveParticles * ParticleStride) : 0;
	INT MaxActiveParticleDataSize = (ParticleData != NULL) ? (MaxActiveParticles * ParticleStride) : 0;
	INT ActiveParticleIndexSize = 0;
	INT MaxActiveParticleIndexSize = 0;

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
INT FParticleBeam2EmitterInstance::GetResourceSize(UBOOL bInExclusiveResourceSizeMode)
{
	INT ResSize = 0;
	if (!bInExclusiveResourceSizeMode || 
		(Component && Component->SceneInfo && Component->SceneInfo->Proxy))
	{
		INT MaxActiveParticleDataSize = (ParticleData != NULL) ? (MaxActiveParticles * ParticleStride) : 0;
		INT MaxActiveParticleIndexSize = 0;
		// Take dynamic data into account as well
		ResSize = sizeof(FDynamicBeam2EmitterData);
		ResSize += MaxActiveParticleDataSize;		// Copy of the particle data on the render thread
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
UBOOL FParticleBeam2EmitterInstance::FillReplayData( FDynamicEmitterReplayDataBase& OutData )
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

	// If the template is disabled, don't return data.
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	if ((LODLevel == NULL) || (LODLevel->bEnabled == FALSE))
	{
		return FALSE;
	}

	// Get the material instance. If there is none, or the material isn't flagged for use with particle systems, use the DefaultMaterial.
	UMaterialInterface* MaterialInst = CurrentMaterial;
	if (MaterialInst == NULL || !MaterialInst->CheckMaterialUsage(MATUSAGE_BeamTrails))
	{
		MaterialInst = GEngine->DefaultMaterial;
	}

	OutData.eEmitterType = DET_Beam2;

	FDynamicBeam2EmitterReplayData* NewReplayData =
		static_cast< FDynamicBeam2EmitterReplayData* >( &OutData );

	NewReplayData->MaterialInterface = MaterialInst;

	// We never want local space for beams
	NewReplayData->bUseLocalSpace = FALSE;

	// Never use axis lock for beams
	NewReplayData->bLockAxis = FALSE;

	DetermineVertexAndTriangleCount();

	NewReplayData->UpVectorStepSize = BeamTypeData->UpVectorStepSize;
	NewReplayData->TrianglesPerSheet.Empty(BeamTrianglesPerSheet.Num());
	NewReplayData->TrianglesPerSheet.AddZeroed(BeamTrianglesPerSheet.Num());
	for (INT BeamIndex = 0; BeamIndex < BeamTrianglesPerSheet.Num(); BeamIndex++)
	{
		NewReplayData->TrianglesPerSheet(BeamIndex) = BeamTrianglesPerSheet(BeamIndex);
	}


	INT IgnoredTaperCount = 0;
	BeamTypeData->GetDataPointerOffsets(this, NULL, TypeDataOffset, 
		NewReplayData->BeamDataOffset, NewReplayData->InterpolatedPointsOffset,
		NewReplayData->NoiseRateOffset, NewReplayData->NoiseDeltaTimeOffset,
		NewReplayData->TargetNoisePointsOffset, NewReplayData->NextNoisePointsOffset, 
		IgnoredTaperCount, NewReplayData->TaperValuesOffset,
		NewReplayData->NoiseDistanceScaleOffset);

	NewReplayData->VertexCount = VertexCount;


	if (BeamModule_Source)
	{
		NewReplayData->bUseSource = TRUE;
	}
	else
	{
		NewReplayData->bUseSource = FALSE;
	}

	if (BeamModule_Target)
	{
		NewReplayData->bUseTarget = TRUE;
	}
	else
	{
		NewReplayData->bUseTarget = FALSE;
	}

	if ((Component->bDeferredBeamUpdate == TRUE) &&
		((BeamModule_Source && BeamModule_Source->bEnabled) || 
		(BeamModule_Target && BeamModule_Target->bEnabled)))
	{
		INT SourceOffset = 0;
		INT TargetOffset = 0;
		UINT* Offset;
		if (BeamModule_Source && BeamModule_Source->bEnabled)
		{
			Offset = ModuleOffsetMap.Find(BeamModule_Source);
			SourceOffset = Offset ? *Offset : 0;
		}
		if (BeamModule_Target && BeamModule_Target->bEnabled)
		{
			Offset = ModuleOffsetMap.Find(BeamModule_Target);
			TargetOffset = Offset ? *Offset : 0;
		}

		for(INT i=ActiveParticles-1; i>=0; i--)
		{
			INT Offset_Source = SourceOffset;
			INT Offset_Target = TargetOffset;
			const INT	CurrentIndex	= ParticleIndices[i];
			const BYTE* ParticleBase	= ParticleData + CurrentIndex * ParticleStride;
			FBaseParticle& Particle		= *((FBaseParticle*) ParticleBase);
			if ((Particle.Flags & STATE_Particle_Freeze) == 0)
			{
				FBeam2TypeDataPayload*	BeamData			= NULL;
				FVector*				InterpolatedPoints	= NULL;
				FLOAT*					NoiseRate			= NULL;
				FLOAT*					NoiseDelta			= NULL;
				FVector*				TargetNoisePoints	= NULL;
				FVector*				NextNoisePoints		= NULL;
				FLOAT*					TaperValues			= NULL;
				FLOAT*					NoiseDistanceScale	= NULL;
				FBeamParticleModifierPayloadData* SourceModifier = NULL;
				FBeamParticleModifierPayloadData* TargetModifier = NULL;

				// Retrieve the payload data offsets
				INT	TempOffset	= TypeDataOffset;
				BeamTypeData->GetDataPointers(this, ParticleBase, TempOffset, BeamData, InterpolatedPoints, 
					NoiseRate, NoiseDelta, TargetNoisePoints, NextNoisePoints, TaperValues,
					NoiseDistanceScale, SourceModifier, TargetModifier);

				// Resolve the source data
				if (BeamModule_Source)
				{
					BeamModule_Source->ResolveSourceData(this, BeamData, ParticleBase, Offset_Source, i, FALSE, SourceModifier);
				}
				else
				{
					BeamData->SourcePoint = Component->LocalToWorld.GetOrigin();
				}

				if (BeamModule_Target)
				{
					BeamModule_Target->ResolveTargetData(this, BeamData, ParticleBase, Offset_Target, i, FALSE, SourceModifier);
				}
			}
		}
	}

	if (BeamModule_Noise)
	{
		NewReplayData->bLowFreqNoise_Enabled = BeamModule_Noise->bLowFreq_Enabled;
		NewReplayData->bHighFreqNoise_Enabled = FALSE;
		NewReplayData->bSmoothNoise_Enabled = BeamModule_Noise->bSmooth;

	}
	else
	{
		NewReplayData->bLowFreqNoise_Enabled = FALSE;
		NewReplayData->bHighFreqNoise_Enabled = FALSE;
		NewReplayData->bSmoothNoise_Enabled = FALSE;
	}
	NewReplayData->Sheets = (BeamTypeData->Sheets > 0) ? BeamTypeData->Sheets : 1;

	NewReplayData->TextureTile = BeamTypeData->TextureTile;
	NewReplayData->TextureTileDistance = BeamTypeData->TextureTileDistance;
	NewReplayData->TaperMethod = BeamTypeData->TaperMethod;
	NewReplayData->InterpolationPoints = BeamTypeData->InterpolationPoints;

	NewReplayData->NoiseTessellation	= 0;
	NewReplayData->Frequency			= 1;
	NewReplayData->NoiseRangeScale		= 1.0f;
	NewReplayData->NoiseTangentStrength= 1.0f;

	INT TessFactor = 1;
	if ((BeamModule_Noise == NULL) || (BeamModule_Noise->bLowFreq_Enabled == FALSE))
	{
		TessFactor	= BeamTypeData->InterpolationPoints ? BeamTypeData->InterpolationPoints : 1;
	}
	else
	{
		NewReplayData->Frequency			= (BeamModule_Noise->Frequency > 0) ? BeamModule_Noise->Frequency : 1;
		NewReplayData->NoiseTessellation	= (BeamModule_Noise->NoiseTessellation > 0) ? BeamModule_Noise->NoiseTessellation : 1;
		NewReplayData->NoiseTangentStrength= BeamModule_Noise->NoiseTangentStrength.GetValue(EmitterTime);
		if (BeamModule_Noise->bNRScaleEmitterTime)
		{
			NewReplayData->NoiseRangeScale = BeamModule_Noise->NoiseRangeScale.GetValue(EmitterTime, Component);
		}
		else
		{
			//@todo.SAS. Need to address this!!!!
			//					check(0 && TEXT("NoiseRangeScale - No way to get per-particle setting at this time."));
			//					NewReplayData->NoiseRangeScale	= BeamModule_Noise->NoiseRangeScale.GetValue(Particle->RelativeTime, Component);
			NewReplayData->NoiseRangeScale = BeamModule_Noise->NoiseRangeScale.GetValue(EmitterTime, Component);
		}
		NewReplayData->NoiseSpeed = BeamModule_Noise->NoiseSpeed.GetValue(EmitterTime);
		NewReplayData->NoiseLockTime = BeamModule_Noise->NoiseLockTime;
		NewReplayData->NoiseLockRadius = BeamModule_Noise->NoiseLockRadius;
		NewReplayData->bTargetNoise = BeamModule_Noise->bTargetNoise;
		NewReplayData->NoiseTension = BeamModule_Noise->NoiseTension;
	}

	INT MaxSegments	= ((TessFactor * NewReplayData->Frequency) + 1 + 1);		// Tessellation * Frequency + FinalSegment + FirstEdge;

	// Determine the index count
	NewReplayData->IndexCount	= 0;
	for (INT Beam = 0; Beam < ActiveParticles; Beam++)
	{
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[Beam]);

		INT						CurrentOffset		= TypeDataOffset;
		FBeam2TypeDataPayload*	BeamData			= NULL;
		FVector*				InterpolatedPoints	= NULL;
		FLOAT*					NoiseRate			= NULL;
		FLOAT*					NoiseDelta			= NULL;
		FVector*				TargetNoisePoints	= NULL;
		FVector*				NextNoisePoints		= NULL;
		FLOAT*					TaperValues			= NULL;
		FLOAT*					NoiseDistanceScale	= NULL;
		FBeamParticleModifierPayloadData* SourceModifier = NULL;
		FBeamParticleModifierPayloadData* TargetModifier = NULL;

		BeamTypeData->GetDataPointers(this, (const BYTE*)Particle, CurrentOffset, BeamData, 
			InterpolatedPoints, NoiseRate, NoiseDelta, TargetNoisePoints, NextNoisePoints, 
			TaperValues, NoiseDistanceScale, SourceModifier, TargetModifier);

		if (BeamData->TriangleCount > 0)
		{
			if (NewReplayData->IndexCount == 0)
			{
				NewReplayData->IndexCount = 2;
			}
			NewReplayData->IndexCount	+= BeamData->TriangleCount * NewReplayData->Sheets;	// 1 index per triangle in the strip PER SHEET
			NewReplayData->IndexCount	+= ((NewReplayData->Sheets - 1) * 4);					// 4 extra indices per stitch (degenerates)
			if (Beam > 0)
			{
				NewReplayData->IndexCount	+= 4;	// 4 extra indices per beam (degenerates)
			}
		}
	}

	if (NewReplayData->IndexCount > 15000)
	{
		NewReplayData->IndexStride	= sizeof(DWORD);
	}
	else
	{
		NewReplayData->IndexStride	= sizeof(WORD);
	}


	//@todo. SORTING IS A DIFFERENT ISSUE NOW! 
	//		 GParticleView isn't going to be valid anymore?
	BYTE* PData = NewReplayData->ParticleData.GetData();
	for (INT i = 0; i < NewReplayData->ActiveParticleCount; i++)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
		appMemcpy(PData, &Particle, ParticleStride);
		PData += ParticleStride;
	}

	// Set the debug rendering flags...
	NewReplayData->bRenderGeometry = BeamTypeData->RenderGeometry;
	NewReplayData->bRenderDirectLine = BeamTypeData->RenderDirectLine;
	NewReplayData->bRenderLines = BeamTypeData->RenderLines;
	NewReplayData->bRenderTessellation = BeamTypeData->RenderTessellation;


	return TRUE;
}
