/*=============================================================================
	ParticleModules_Spawn.cpp: Particle spawn-related module implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSpawnBase);

/*-----------------------------------------------------------------------------
	UParticleModuleSpawn implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSpawn);

void UParticleModuleSpawn::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	for (INT Index = 0; Index < BurstList.Num(); Index++)
	{
		FParticleBurst* Burst = &(BurstList(Index));

		// Clamp them to positive numbers...
		Burst->Count = Max<INT>(0, Burst->Count);
		if (Burst->CountLow > -1)
		{
			Burst->CountLow = Min<INT>(Burst->Count, Burst->CountLow);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 *	Retrieve the spawn amount this module is contributing.
 *	Note that if multiple Spawn-specific modules are present, if any one
 *	of them ignores the SpawnRate processing it will be ignored.
 *
 *	@param	Owner		The particle emitter instance that is spawning.
 *	@param	Offset		The offset into the particle payload for the module.
 *	@param	OldLeftover	The bit of timeslice left over from the previous frame.
 *	@param	DeltaTime	The time that has expired since the last frame.
 *	@param	Number		The number of particles to spawn. (OUTPUT)
 *	@param	Rate		The spawn rate of the module. (OUTPUT)
 *
 *	@return	UBOOL		FALSE if the SpawnRate should be ignored.
 *						TRUE if the SpawnRate should still be processed.
 */
UBOOL UParticleModuleSpawn::GetSpawnAmount(FParticleEmitterInstance* Owner, 
	INT Offset, FLOAT OldLeftover, FLOAT DeltaTime, INT& Number, FLOAT& Rate)
{
	check(Owner);
	return FALSE;
}

UBOOL UParticleModuleSpawn::GenerateLODModuleValues(UParticleModule* SourceModule, FLOAT Percentage, UParticleLODLevel* LODLevel)
{
	// Convert the module values
	UParticleModuleSpawn* SpawnSource = Cast<UParticleModuleSpawn>(SourceModule);
	if (!SpawnSource)
	{
		return FALSE;
	}

	UBOOL bResult	= TRUE;
#if !CONSOLE
	//SpawnRate
	// @GEMINI_TODO: Make sure these functions are never called on console, or when the UDistributions are missing
	if (ConvertFloatDistribution(Rate.Distribution, SpawnSource->Rate.Distribution, Percentage) == FALSE)
	{
		bResult	= FALSE;
	}

	//ParticleBurstMethod
	//BurstList
	check(BurstList.Num() == SpawnSource->BurstList.Num());
	for (INT BurstIndex = 0; BurstIndex < SpawnSource->BurstList.Num(); BurstIndex++)
	{
		FParticleBurst* SourceBurst	= &(SpawnSource->BurstList(BurstIndex));
		FParticleBurst* Burst		= &(BurstList(BurstIndex));

		Burst->Time = SourceBurst->Time;
		// Don't drop below 1...
		if (Burst->Count > 0)
		{
			Burst->Count = appTrunc(SourceBurst->Count * (Percentage / 100.0f));
			if (Burst->Count == 0)
			{
				Burst->Count = 1;
			}
		}
	}
#endif	//#if !CONSOLE
	return bResult;
}

/**
 *	Retrieve the maximum spawn rate for this module...
 *	Used in estimating the number of particles that could be used.
 *
 *	@return	FLOAT			The maximum spawn rate
 */
FLOAT UParticleModuleSpawn::GetMaximumSpawnRate()
{
	FLOAT MinSpawn, MaxSpawn;
	FLOAT MinScale, MaxScale;

	Rate.GetOutRange(MinSpawn, MaxSpawn);
	RateScale.GetOutRange(MinScale, MaxScale);

	return (MaxSpawn * MaxScale);
}

/**
 *	Retrieve the estimated spawn rate for this module...
 *	Used in estimating the number of particles that could be used.
 *
 *	@return	FLOAT			The maximum spawn rate
 */
FLOAT UParticleModuleSpawn::GetEstimatedSpawnRate()
{
	FLOAT MinSpawn, MaxSpawn;
	FLOAT MinScale, MaxScale;

	Rate.GetOutRange(MinSpawn, MaxSpawn);
	RateScale.GetOutRange(MinScale, MaxScale);

	UDistributionFloatConstantCurve* RateScaleCurve = Cast<UDistributionFloatConstantCurve>(RateScale.Distribution);
	if (RateScaleCurve != NULL)
	{
		// We need to walk the curve and determine the average
		INT KeyCount = RateScaleCurve->GetNumKeys();
		if (KeyCount > 1)
		{
			FLOAT SummedAverage = 0.0f;
			FLOAT LastKeyIn = RateScaleCurve->GetKeyIn(KeyCount - 1);
			FLOAT PrevKeyIn = Max<FLOAT>(0.0f, RateScaleCurve->GetKeyIn(0));
			FLOAT TotalTime = Max<FLOAT>(1.0f, LastKeyIn - PrevKeyIn);
			FLOAT PrevKeyOut = RateScaleCurve->GetKeyOut(0, 0);
			for (INT KeyIndex = 1; KeyIndex < KeyCount; KeyIndex++)
			{
				FLOAT KeyIn = RateScaleCurve->GetKeyIn(KeyIndex);
				FLOAT KeyOut = RateScaleCurve->GetKeyOut(0, KeyIndex);

				FLOAT Delta = (KeyIn - PrevKeyIn) / TotalTime;
				FLOAT Avg = (KeyOut + PrevKeyOut) / 2.0f;
				SummedAverage += Delta * Avg;

				PrevKeyIn = KeyIn;
				PrevKeyOut = KeyOut;
			}

			MaxScale = SummedAverage;
		}
	}

	// We need to estimate the value for curves to prevent short spikes from inflating the value... 
	UDistributionFloatConstantCurve* RateCurve = Cast<UDistributionFloatConstantCurve>(Rate.Distribution);
	if (RateCurve != NULL)
	{
		// We need to walk the curve and determine the average
		INT KeyCount = RateCurve->GetNumKeys();
		if (KeyCount > 1)
		{
			FLOAT SummedAverage = 0.0f;
			FLOAT LastKeyIn = RateCurve->GetKeyIn(KeyCount - 1);
			FLOAT PrevKeyIn = Max<FLOAT>(0.0f, RateCurve->GetKeyIn(0));
			FLOAT TotalTime = Max<FLOAT>(1.0f, LastKeyIn - PrevKeyIn);
			FLOAT PrevKeyOut = RateCurve->GetKeyOut(0, 0);
			for (INT KeyIndex = 1; KeyIndex < KeyCount; KeyIndex++)
			{
				FLOAT KeyIn = RateCurve->GetKeyIn(KeyIndex);
				FLOAT KeyOut = RateCurve->GetKeyOut(0, KeyIndex);

				FLOAT Delta = (KeyIn - PrevKeyIn) / TotalTime;
				FLOAT Avg = ((KeyOut + PrevKeyOut) * MaxScale) / 2.0f;
				SummedAverage += Delta * Avg;

				PrevKeyIn = KeyIn;
				PrevKeyOut = KeyOut;
			}

			MaxSpawn = SummedAverage;// / (Max<FLOAT>(1.0f, LastKeyIn));
			return MaxSpawn;
		}
	}

	return (MaxSpawn * MaxScale);
}

/**
 *	Retrieve the maximum number of particles this module could burst.
 *	Used in estimating the number of particles that could be used.
 *
 *	@return	INT			The maximum burst count
 */
INT UParticleModuleSpawn::GetMaximumBurstCount()
{
	// Note that this does not take into account entries could be outside of the emitter duration!
	INT MaxBurst = 0;
	for (INT BurstIndex = 0; BurstIndex < BurstList.Num(); BurstIndex++)
	{
		MaxBurst += BurstList(BurstIndex).Count;
	}
	return MaxBurst;
}

/*-----------------------------------------------------------------------------
	UParticleModuleSpawnPerUnit implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSpawnPerUnit);

/**
 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return UINT		The number fo bytes the module needs per emitter instance.
 */
UINT UParticleModuleSpawnPerUnit::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return sizeof(FParticleSpawnPerUnitInstancePayload);
}

/**
 *	Retrieve the spawn amount this module is contributing.
 *	Note that if multiple Spawn-specific modules are present, if any one
 *	of them ignores the SpawnRate processing it will be ignored.
 *
 *	@param	Owner		The particle emitter instance that is spawning.
 *	@param	Offset		The offset into the particle payload for the module.
 *	@param	OldLeftover	The bit of timeslice left over from the previous frame.
 *	@param	DeltaTime	The time that has expired since the last frame.
 *	@param	Number		The number of particles to spawn. (OUTPUT)
 *	@param	Rate		The spawn rate of the module. (OUTPUT)
 *
 *	@return	UBOOL		FALSE if the SpawnRate should be ignored.
 *						TRUE if the SpawnRate should still be processed.
 */
UBOOL UParticleModuleSpawnPerUnit::GetSpawnAmount(FParticleEmitterInstance* Owner, 
	INT Offset, FLOAT OldLeftover, FLOAT DeltaTime, INT& Number, FLOAT& Rate)
{
	check(Owner);

	UBOOL bMoved = FALSE;
	FParticleSpawnPerUnitInstancePayload* SPUPayload = NULL;
	FLOAT NewTravelLeftover = 0.0f;

	FLOAT ParticlesPerUnit = SpawnPerUnit.GetValue(Owner->EmitterTime, Owner->Component) / UnitScalar;
	// Allow for PPU of 0.0f to allow for 'turning off' an emitter when moving
	if (ParticlesPerUnit >= 0.0f)
	{
		FLOAT LeftoverTravel = 0.0f;
		BYTE* InstData = Owner->GetModuleInstanceData(this);
		if (InstData)
		{
			SPUPayload = (FParticleSpawnPerUnitInstancePayload*)InstData;
			LeftoverTravel = SPUPayload->CurrentDistanceTravelled;
		}

		// Calculate movement delta over last frame, include previous remaining delta
		FVector TravelDirection = Owner->Location - Owner->OldLocation;
		FVector RemoveComponentMultiplier(
			bIgnoreMovementAlongX ? 0.0f : 1.0f,
			bIgnoreMovementAlongY ? 0.0f : 1.0f,
			bIgnoreMovementAlongZ ? 0.0f : 1.0f
			);
		TravelDirection *= RemoveComponentMultiplier;

		// Calculate distance traveled
		FLOAT TravelDistance = TravelDirection.Size();
		if (MaxFrameDistance > 0.0f)
		{
			if (TravelDistance > MaxFrameDistance)
			{
				// Clear it out!
				//@todo. Need to 'shift' the start point closer so we can still spawn...
				TravelDistance = 0.0f;
				SPUPayload->CurrentDistanceTravelled = 0.0f;
			}
		}

		if (TravelDistance < MinFrameDistance)
		{
			bMoved = FALSE;
			Number = 0;
			Rate = 0.0f;
		}
		else if (TravelDistance > 0.0f)
		{
			if (TravelDistance > (MovementTolerance * UnitScalar))
			{
				bMoved = TRUE;
			}

			// Normalize direction for use later
			TravelDirection.Normalize();

			// Calculate number of particles to emit
			FLOAT NewLeftover = (TravelDistance + LeftoverTravel) * ParticlesPerUnit;
			Number = appFloor(NewLeftover);
			Rate = Number / DeltaTime;
			NewTravelLeftover = (TravelDistance + LeftoverTravel) - (Number * UnitScalar);
			if (SPUPayload)
			{
				SPUPayload->CurrentDistanceTravelled = Max<FLOAT>(0.0f, NewTravelLeftover);
			}

		}
		else
		{
			Number = 0;
			Rate = 0.0f;
		}
	}
	else
	{
		Number = 0;
		Rate = 0.0f;
	}

	if (bIgnoreSpawnRateWhenMoving == TRUE)
	{
		if (bMoved == TRUE)
		{
			return FALSE;
		}
		return TRUE;
	}

	return bProcessSpawnRate;
}


IMPLEMENT_CLASS(UParticleModuleStoreSpawnTimeBase)
IMPLEMENT_CLASS(UParticleModuleStoreSpawnTime)

void UParticleModuleStoreSpawnTime::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;
	PARTICLE_ELEMENT(FStoreSpawnTimePayload, SpawntimePayload);
	SpawntimePayload.SpawnTime = GWorld->GetTimeSeconds();
}


/**
 *	Returns the number of bytes that the module requires in the particle payload block.
 *
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return	UINT		The number of bytes the module needs per particle.
 */
UINT UParticleModuleStoreSpawnTime::RequiredBytes(FParticleEmitterInstance* Owner )
{
	return sizeof(FStoreSpawnTimePayload);
}


