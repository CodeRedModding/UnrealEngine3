/*=============================================================================
	ParticleModules_Event.cpp: Particle event-related module implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleEventBase);

/*-----------------------------------------------------------------------------
	UParticleModuleEventGenerator implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleEventGenerator);


IMPLEMENT_CLASS(AParticleEventManager)
IMPLEMENT_CLASS(UParticleModuleEventSendToGame)


/**
 *	Called on a particle that is freshly spawned by the emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	SpawnTime	The time of the spawn.
 */
void UParticleModuleEventGenerator::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
}

/**
 *	Called on a particle that is being updated by its emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	DeltaTime	The time since the last update.
 */
void UParticleModuleEventGenerator::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
}

/**
 *	Returns the number of bytes that the module requires in the particle payload block.
 *
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return	UINT		The number of bytes the module needs per particle.
 */
UINT UParticleModuleEventGenerator::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return 0;
}

/**
 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return UINT		The number fo bytes the module needs per emitter instance.
 */
UINT UParticleModuleEventGenerator::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return sizeof(FParticleEventInstancePayload);
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleEventGenerator::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	FParticleEventInstancePayload* Payload = (FParticleEventInstancePayload*)InstData;
	if (Payload)
	{
		for (INT EventGenIndex = 0; EventGenIndex < Events.Num(); EventGenIndex++)
		{
			switch (Events(EventGenIndex).Type)
			{
			case EPET_Spawn:					Payload->bSpawnEventsPresent = TRUE;				break;
			case EPET_Death:					Payload->bDeathEventsPresent = TRUE;				break;
			case EPET_Collision:				Payload->bCollisionEventsPresent = TRUE;			break;
			case EPET_WorldAttractorCollision:	Payload->bAttractorCollisionEventsPresent = TRUE;	break;
			}
		}
		return 0;
	}

	return 0xffffffff;
}

/**
 *	Called when the properties change in the property window.
 *
 *	@param	PropertyThatChanged		The property that was edited...
 */
void UParticleModuleEventGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UObject* OuterObj = GetOuter();
	check(OuterObj);
	UParticleLODLevel* LODLevel = Cast<UParticleLODLevel>(OuterObj);
	if (LODLevel)
	{
		// The outer is incorrect - warn the user and handle it
		OuterObj = LODLevel->GetOuter();
		UParticleEmitter* Emitter = Cast<UParticleEmitter>(OuterObj);
		check(Emitter);
		OuterObj = Emitter->GetOuter();
	}
	UParticleSystem* PartSys = PartSys = CastChecked<UParticleSystem>(OuterObj);
	if (PartSys)
	{
		PartSys->PostEditChangeProperty(PropertyChangedEvent);
	}
}


/** 
 * This is our helper function to send out data to the Game.  We currently only are supporting a few types of data (primarily related to
 * collision events)
 *
 *	@param	ParticleModuleEventsToSendToGame  Events to send
 *	@param	InCollideDirection  Direction the particle was traveling
 *	@param	InHitLocation		Location where the particle hit
 *	@param	InHitNormal			Normal of the hit location
 *	@param	InBoneName			Bone name the particle hit
 *
 **/
static void DoHandleParticleModuleEventSendToGame( const TArray<class UParticleModuleEventSendToGame*>& ParticleModuleEventsToSendToGame, const FVector& InCollideDirection, const FVector& InHitLocation, const FVector& InHitNormal, const FName& InBoneName )
{
	if( ( GWorld != NULL ) && ( GWorld->GetWorldInfo() != NULL ) && ( GWorld->GetWorldInfo()->MyParticleEventManager != NULL ) )
	{
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

		for( INT SendToGameIdx = 0; SendToGameIdx < ParticleModuleEventsToSendToGame.Num(); ++SendToGameIdx )
		{
			WorldInfo->MyParticleEventManager->eventHandleParticleModuleEventSendToGame( ParticleModuleEventsToSendToGame(SendToGameIdx), InCollideDirection, InHitLocation, InHitNormal, InBoneName );
		}
	}
}


/**
 *	Called when a particle is spawned and an event payload is present.
 *
 *	@param	Owner			Pointer to the owning FParticleEmitterInstance.
 *	@param	EventPayload	Pointer to the event instance payload data.
 *	@param	NewParticle		Pointer to the particle that was spawned.
 *
 *	@return	UBOOL			TRUE if processed, FALSE if not.
 */
UBOOL UParticleModuleEventGenerator::HandleParticleSpawned(FParticleEmitterInstance* Owner, 
	FParticleEventInstancePayload* EventPayload, FBaseParticle* NewParticle)
{
	check(Owner && EventPayload && NewParticle);

	EventPayload->SpawnTrackingCount++;

	UBOOL bProcessed = FALSE;
	for (INT EventIndex = 0; EventIndex < Events.Num(); EventIndex++)
	{
		FParticleEvent_GenerateInfo& EventGenInfo = Events(EventIndex);
		if (EventGenInfo.Type == EPET_Spawn)
		{
			if (EventGenInfo.Frequency > 0)
			{
				if ((EventPayload->SpawnTrackingCount % EventGenInfo.Frequency) == 0)
				{
					Owner->Component->ReportEventSpawn(EventGenInfo.CustomName, 
						Owner->EmitterTime, NewParticle->Location, NewParticle->Velocity);
					bProcessed = TRUE;
#if !FINAL_RELEASE
					Owner->EventCount++;
#endif	//#if !FINAL_RELEASE
				}
			}
			else
			{
				Owner->Component->ReportEventSpawn(EventGenInfo.CustomName, Owner->EmitterTime, 
					NewParticle->Location, NewParticle->Velocity);
				bProcessed = TRUE;
#if !FINAL_RELEASE
				Owner->EventCount++;
#endif	//#if !FINAL_RELEASE
			}
		}
	}

	return bProcessed;
}

/**
 *	Called when a particle is killed and an event payload is present.
 *
 *	@param	Owner			Pointer to the owning FParticleEmitterInstance.
 *	@param	EventPayload	Pointer to the event instance payload data.
 *	@param	DeadParticle	Pointer to the particle that is being killed.
 *
 *	@return	UBOOL			TRUE if processed, FALSE if not.
 */
UBOOL UParticleModuleEventGenerator::HandleParticleKilled(FParticleEmitterInstance* Owner, 
	FParticleEventInstancePayload* EventPayload, FBaseParticle* DeadParticle)
{
	check(Owner && EventPayload && DeadParticle);

	EventPayload->DeathTrackingCount++;

	UBOOL bProcessed = FALSE;
	for (INT EventIndex = 0; EventIndex < Events.Num(); EventIndex++)
	{
		FParticleEvent_GenerateInfo& EventGenInfo = Events(EventIndex);
		if (EventGenInfo.Type == EPET_Death)
		{
			if (EventGenInfo.Frequency > 0)
			{
				if ((EventPayload->DeathTrackingCount % EventGenInfo.Frequency) == 0)
				{
					Owner->Component->ReportEventDeath(EventGenInfo.CustomName, 
						Owner->EmitterTime, DeadParticle->Location, 
						DeadParticle->Velocity, DeadParticle->RelativeTime);
					bProcessed = TRUE;
#if !FINAL_RELEASE
					Owner->EventCount++;
#endif	//#if !FINAL_RELEASE
				}
			}
			else
			{
				Owner->Component->ReportEventDeath(EventGenInfo.CustomName, 
					Owner->EmitterTime, DeadParticle->Location, 
					DeadParticle->Velocity, DeadParticle->RelativeTime);
				bProcessed = TRUE;
#if !FINAL_RELEASE
				Owner->EventCount++;
#endif	//#if !FINAL_RELEASE
			}
		}
	}

	return bProcessed;
}

/**
 *	Called when a particle collides and an event payload is present.
 *
 *	@param	Owner				Pointer to the owning FParticleEmitterInstance.
 *	@param	EventPayload		Pointer to the event instance payload data.
 *	@param	CollidePayload		Pointer to the collision payload data.
 *	@param	Hit					The CheckResult for the collision.
 *	@param	CollideParticle		Pointer to the particle that has collided.
 *	@param	CollideDirection	The direction the particle was traveling when the collision occurred.
 *
 *	@return	UBOOL				TRUE if processed, FALSE if not.
 */
UBOOL UParticleModuleEventGenerator::HandleParticleCollision(FParticleEmitterInstance* Owner, 
															 FParticleEventInstancePayload* EventPayload, FParticleCollisionPayload* CollidePayload, 
															 FCheckResult* Hit, FBaseParticle* CollideParticle, FVector& CollideDirection)
{
	check(Owner && EventPayload && CollideParticle);

	EventPayload->CollisionTrackingCount++;

	UBOOL bProcessed = FALSE;
	for (INT EventIndex = 0; EventIndex < Events.Num(); EventIndex++)
	{
		FParticleEvent_GenerateInfo& EventGenInfo = Events(EventIndex);
		if (EventGenInfo.Type == EPET_Collision)
		{
			if (EventGenInfo.FirstTimeOnly == TRUE)
			{
				if ((CollideParticle->Flags & STATE_Particle_CollisionHasOccurred) != 0)
				{
					continue;
				}
			}
			else
			if (EventGenInfo.LastTimeOnly == TRUE)
			{
				if (CollidePayload->UsedCollisions != 0)
				{
					continue;
				}
			}

			if (EventGenInfo.Frequency > 0)
			{
				if ((EventPayload->CollisionTrackingCount % EventGenInfo.Frequency) == 0)
				{
					Owner->Component->ReportEventCollision(
						EventGenInfo.CustomName, 
						Owner->EmitterTime, 
						Hit->Location,
						CollideDirection, 
						CollideParticle->Velocity, 
						CollideParticle->RelativeTime, 
						Hit->Normal, 
						Hit->Time, 
						Hit->Item, 
						Hit->BoneName);
					bProcessed = TRUE;
#if !FINAL_RELEASE
					Owner->EventCount++;
#endif	//#if !FINAL_RELEASE

					DoHandleParticleModuleEventSendToGame( EventGenInfo.ParticleModuleEventsToSendToGame, CollideDirection, Hit->Location, Hit->Normal, Hit->BoneName );
				}
			}
			else
			{
				Owner->Component->ReportEventCollision(
					EventGenInfo.CustomName, 
					Owner->EmitterTime, 
					Hit->Location,
					CollideDirection, 
					CollideParticle->Velocity, 
					CollideParticle->RelativeTime, 
					Hit->Normal, 
					Hit->Time, 
					Hit->Item, 
					Hit->BoneName);
				bProcessed = TRUE;
#if !FINAL_RELEASE
				Owner->EventCount++;
#endif	//#if !FINAL_RELEASE

				DoHandleParticleModuleEventSendToGame( EventGenInfo.ParticleModuleEventsToSendToGame, CollideDirection, Hit->Location, Hit->Normal, Hit->BoneName );
			}
		}
	}

	return bProcessed;
}

/**
 *	Called when a particle collides with a world attractor and an event payload is present.
 *
 *	@param	Owner				Pointer to the owning FParticleEmitterInstance.
 *	@param	EventPayload		Pointer to the event instance payload data.
 *	@param	CollidePayload		Pointer to the collision payload data.
 *	@param	Hit					The CheckResult for the collision.
 *	@param	CollideParticle		Pointer to the particle that has collided.
 *	@param	CollideDirection	The direction the particle was traveling when the collision occurred.
 *
 *	@return	UBOOL				TRUE if processed, FALSE if not.
 */
UBOOL UParticleModuleEventGenerator::HandleParticleAttractorCollision(FParticleEmitterInstance* Owner, 
																	  FParticleEventInstancePayload* EventPayload, FParticleCollisionPayload* CollidePayload, 
																	  FCheckResult* Hit, FBaseParticle* CollideParticle, FVector& CollideDirection)
{
	check(Owner && EventPayload && CollideParticle);

	EventPayload->AttractorCollisionTrackingCount++;

	UBOOL bProcessed = FALSE;
	for (INT EventIndex = 0; EventIndex < Events.Num(); EventIndex++)
	{
		FParticleEvent_GenerateInfo& EventGenInfo = Events(EventIndex);
		if (EventGenInfo.Type == EPET_WorldAttractorCollision)
		{
			if (EventGenInfo.FirstTimeOnly == TRUE)
			{
				if ((CollideParticle->Flags & STATE_Particle_CollisionHasOccurred) != 0)
				{
					continue;
				}
			}
			else
			if (EventGenInfo.LastTimeOnly == TRUE)
			{
				if (CollidePayload->UsedCollisions != 0)
				{
					continue;
				}
			}

			if (EventGenInfo.Frequency > 0)
			{
				if ((EventPayload->AttractorCollisionTrackingCount % EventGenInfo.Frequency) == 0)
				{
					Owner->Component->ReportEventAttractorCollision(
						EventGenInfo.CustomName, 
						Owner->EmitterTime, 
						Hit->Location,
						CollideDirection, 
						CollideParticle->Velocity, 
						CollideParticle->RelativeTime, 
						Hit->Normal, 
						Hit->Time, 
						Hit->Item, 
						Hit->BoneName);
					bProcessed = TRUE;
#if !FINAL_RELEASE
					Owner->EventCount++;
#endif	//#if !FINAL_RELEASE

					DoHandleParticleModuleEventSendToGame( EventGenInfo.ParticleModuleEventsToSendToGame, CollideDirection, Hit->Location, Hit->Normal, Hit->BoneName );
				}
			}
			else
			{
				Owner->Component->ReportEventAttractorCollision(
					EventGenInfo.CustomName, 
					Owner->EmitterTime, 
					Hit->Location,
					CollideDirection, 
					CollideParticle->Velocity, 
					CollideParticle->RelativeTime, 
					Hit->Normal, 
					Hit->Time, 
					Hit->Item, 
					Hit->BoneName);
				bProcessed = TRUE;
#if !FINAL_RELEASE
				Owner->EventCount++;
#endif	//#if !FINAL_RELEASE

				DoHandleParticleModuleEventSendToGame( EventGenInfo.ParticleModuleEventsToSendToGame, CollideDirection, Hit->Location, Hit->Normal, Hit->BoneName );
			}
		}
	}

	return bProcessed;
}

/*-----------------------------------------------------------------------------
	UParticleModuleEventReceiverBase implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleEventReceiverBase);

/*-----------------------------------------------------------------------------
	UParticleModuleEventReceiver implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleEventReceiverKillParticles);

/**
 *	Process the event...
 *
 *	@param	Owner		The FParticleEmitterInstance this module is contained in.
 *	@param	InEvent		The FParticleEventData that occurred.
 *	@param	DeltaTime	The time slice of this frame.
 *
 *	@return	UBOOL		TRUE if the event was processed; FALSE if not.
 */
UBOOL UParticleModuleEventReceiverKillParticles::ProcessParticleEvent(FParticleEmitterInstance* Owner, FParticleEventData& InEvent, FLOAT DeltaTime)
{
	if ((InEvent.EventName == EventName) && ((EventGeneratorType == EPET_Any) || (EventGeneratorType == InEvent.Type)))
	{
		Owner->KillParticlesForced(TRUE);
		if (bStopSpawning == TRUE)
		{
			Owner->SetHaltSpawning(TRUE);
		}
		return TRUE;
	}

	return FALSE;
}

/*-----------------------------------------------------------------------------
	UParticleModuleEventReceiver implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleEventReceiverSpawn);

/**
 *	Process the event...
 *
 *	@param	Owner		The FParticleEmitterInstance this module is contained in.
 *	@param	InEvent		The FParticleEventData that occurred.
 *	@param	DeltaTime	The time slice of this frame.
 *
 *	@return	UBOOL		TRUE if the event was processed; FALSE if not.
 */
UBOOL UParticleModuleEventReceiverSpawn::ProcessParticleEvent(FParticleEmitterInstance* Owner, FParticleEventData& InEvent, FLOAT DeltaTime)
{
	if ((InEvent.EventName == EventName) && ((EventGeneratorType == EPET_Any) || (EventGeneratorType == InEvent.Type)))
	{
		UBOOL bLocationFlag = bUsePSysLocation;
		INT Count = 0;

		switch (InEvent.Type)
		{
		case EPET_Spawn:
			Count = appRound(SpawnCount.GetValue(InEvent.EmitterTime));
			break;
		case EPET_Death:
			{
				FParticleEventDeathData* DeathData = (FParticleEventDeathData*)(&InEvent);
				Count = appRound(SpawnCount.GetValue(bUseParticleTime ? DeathData->ParticleTime : InEvent.EmitterTime));
			}
			break;
		case EPET_Collision:
			{
				FParticleEventCollideData* CollideData = (FParticleEventCollideData*)(&InEvent);
				Count = appRound(SpawnCount.GetValue(bUseParticleTime ? CollideData->ParticleTime : InEvent.EmitterTime));
			}
			break;
		case EPET_WorldAttractorCollision:
			{
				FParticleEventAttractorCollideData* CollideData = (FParticleEventAttractorCollideData*)(&InEvent);
				Count = appRound(SpawnCount.GetValue(bUseParticleTime ? CollideData->ParticleTime : InEvent.EmitterTime));
			}
			break;
		case EPET_Kismet:
			{
				FParticleEventKismetData* KismetData = (FParticleEventKismetData*)(&InEvent);
				bLocationFlag = KismetData->UsePSysCompLocation;
				Count = appRound(SpawnCount.GetValue(InEvent.EmitterTime));
			}
			break;
		}

		if (Count > 0)
		{
			FVector SpawnLocation = bLocationFlag ? Owner->Location : InEvent.Location;
			FVector Velocity = bInheritVelocity ? 
				(InEvent.Velocity * InheritVelocityScale.GetValue(InEvent.EmitterTime)) : FVector(0.0f);
			if ((GSystemSettings.DetailMode != DM_High) && (Owner->SpriteTemplate != NULL))
			{
				Count *= Owner->SpriteTemplate->MediumDetailSpawnRateScale;
			}			
			Owner->ForceSpawn(DeltaTime, 0, Count, SpawnLocation, Velocity);
		}

		return TRUE;
	}

	return FALSE;
}
