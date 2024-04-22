/*=============================================================================
	ParticleModules_WorldForces.cpp: 
	WorldForces-related particle module implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EnginePhysicsClasses.h"

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleWorldForcesBase);

/*-----------------------------------------------------------------------------
	UParticleModuleWorldAttractor implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleWorldAttractor);

/**
 *	Called on a particle that is being updated by its emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	DeltaTime	The time since the last update.
 */
void UParticleModuleWorldAttractor::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	AWorldInfo::AWorldAttractorIter Attractors(GWorld->GetWorldInfo()->GetAttractorIter());
	TArray<AWorldAttractor*> AttractorInstances;
	for(; Attractors; ++Attractors)
	{
		AWorldAttractor* Attractor = *Attractors;

		if(Attractor->bEnabled)
		{
			AttractorInstances.Push(Attractor);
		}
	}

	if(AttractorInstances.Num() > 0)
	{
		BEGIN_UPDATE_LOOP;
		for(INT AttractorCounter(0); AttractorCounter < AttractorInstances.Num(); ++AttractorCounter)
		{
			FLOAT Time = bParticleLifeRelative ? Particle.RelativeTime : Owner->EmitterTime;
			FLOAT AttractorTimeInfluenceScale = AttractorInfluence.GetValue(Time) * DeltaTime;
			FVector VelToAdd(AttractorInstances(AttractorCounter)->GetVelocityForAttraction(Particle.Location, Time, DeltaTime, Particle.Size.Size()));
			FVector Temp(AttractorInstances(AttractorCounter)->Location - Particle.Location);
			FLOAT Distance = Temp.Size();

			if(Distance <= AttractorInstances(AttractorCounter)->Range.GetValue(Time))
			{
				if(Distance <= AttractorInstances(AttractorCounter)->DragRadius.GetValue(Time) && Particle.Size.Size() != 0.0f)
				{
					FVector VelocityForDelta(VelToAdd*DeltaTime);
					VelToAdd -= (AttractorInstances(AttractorCounter)->DragCoefficient.GetValue(Time) * Particle.Size.Size()) * (VelocityForDelta * VelocityForDelta);
				}

			}

			Particle.Velocity	+= VelToAdd * AttractorTimeInfluenceScale;
			Particle.BaseVelocity	+= VelToAdd * AttractorTimeInfluenceScale;
		}
		END_UPDATE_LOOP;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModulePhysicsVolumes implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModulePhysicsVolumes);

/**
 *	Called on a particle that is being updated by its emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	DeltaTime	The time since the last update.
 */
void UParticleModulePhysicsVolumes::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	APhysicsVolume* DefaultPhysicsVolume(GWorld->GetWorldInfo()->GetDefaultPhysicsVolume());
	// check for all volumes at that point
	FMemMark Mark(GMainThreadMemStack);

	UBOOL bWithinRange = FALSE;
	APhysicsVolume* CurrentPhysicsVolume(DefaultPhysicsVolume);

	for( APhysicsVolume* VolumeIter(GWorld->GetWorldInfo()->FirstPhysicsVolume); VolumeIter;)
	{
		if ( VolumeIter && VolumeIter->CollisionComponent && (VolumeIter->Priority > CurrentPhysicsVolume->Priority) && Owner->Component->Bounds.GetBox().Intersect(VolumeIter->CollisionComponent->Bounds.GetBox()))
		{
			bWithinRange = TRUE;
			break;
		}
		VolumeIter = VolumeIter->NextPhysicsVolume;
	}

	if(bWithinRange)
	{
		BEGIN_UPDATE_LOOP;
			APhysicsVolume* CurrentPhysicsVolume(DefaultPhysicsVolume);

			for( APhysicsVolume* VolumeIter(GWorld->GetWorldInfo()->FirstPhysicsVolume); VolumeIter;)
			{
				if ( VolumeIter && (VolumeIter->Priority > CurrentPhysicsVolume->Priority) && VolumeIter->Encompasses(Particle.Location))
				{
					CurrentPhysicsVolume = VolumeIter;
				}

				VolumeIter = VolumeIter->NextPhysicsVolume;
			}

			FVector VelToAdd(CurrentPhysicsVolume->ZoneVelocity * DeltaTime);

			if(CurrentPhysicsVolume == DefaultPhysicsVolume)
			{
				if(LevelInfluenceType == LIT_Never)
				{
					VelToAdd = FVector::ZeroVector;
				}
				else if(LevelInfluenceType == LIT_OutsidePhysicsVolumes)
				{
					VelToAdd += FVector(0.0f, 0.0f, GWorld->GetWorldInfo()->GetDefaultPhysicsVolume()->GetGravityZ() * DeltaTime);
				}
			}
			
			if(LevelInfluenceType == LIT_Always)
			{
				VelToAdd += FVector(0.0f, 0.0f, GWorld->GetWorldInfo()->GetDefaultPhysicsVolume()->GetGravityZ() * DeltaTime);
			}

			Particle.Velocity	+= VelToAdd;
			Particle.BaseVelocity	+= VelToAdd;
		END_UPDATE_LOOP;
	}

	Mark.Pop();
}
