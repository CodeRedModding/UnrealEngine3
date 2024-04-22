/*=============================================================================
	ParticleModules_Camera.cpp: 
	Camera-related particle module implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleCameraBase);

/*-----------------------------------------------------------------------------
	UParticleModuleCameraOffset
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleCameraOffset);

    //## BEGIN PROPS ParticleModuleCameraOffset
//    struct FRawDistributionFloat CameraOffset;
    //## END PROPS ParticleModuleCameraOffset

/**
 *	Called on a particle that is freshly spawned by the emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	SpawnTime	The time of the spawn.
 */
void UParticleModuleCameraOffset::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FLOAT ScaleFactor = 1.0f;

	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	if (LODLevel->RequiredModule->bUseLocalSpace == FALSE)
	{
		if (Owner != NULL)
		{
			if (Owner->Component != NULL)
			{
				// Take component scale into account
				FVector Scale = FVector(1.0f, 1.0f, 1.0f);
				Scale *= Owner->Component->Scale * Owner->Component->Scale3D;
				AActor* Actor = Owner->Component->GetOwner();
				if (Actor && !Owner->Component->AbsoluteScale)
				{
					Scale *= Actor->DrawScale * Actor->DrawScale3D;
				}

				ScaleFactor = Scale.GetMax();
			}
		}
	}
	SPAWN_INIT;
	{
		CurrentOffset = Owner ? ((Owner->CameraPayloadOffset != 0) ? Owner->CameraPayloadOffset : Offset) : Offset;
		PARTICLE_ELEMENT(FCameraOffsetParticlePayload, CameraPayload);
		FLOAT CameraOffsetValue = CameraOffset.GetValue(Particle.RelativeTime, Owner->Component) * ScaleFactor;
		if (UpdateMethod == EPCOUM_DirectSet)
		{
			CameraPayload.BaseOffset = CameraOffsetValue;
			CameraPayload.Offset = CameraOffsetValue;
		}
		else if (UpdateMethod == EPCOUM_Additive)
		{
			CameraPayload.Offset += CameraOffsetValue;
		}
		else //if (UpdateMethod == EPCOUM_Scalar)
		{
			CameraPayload.Offset *= CameraOffsetValue;
		}
	}
}

/**
 *	Called on a particle that is being updated by its emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	DeltaTime	The time since the last update.
 */
void UParticleModuleCameraOffset::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	if (bSpawnTimeOnly == FALSE)
	{
		BEGIN_UPDATE_LOOP;
		{
			CurrentOffset = Owner ? ((Owner->CameraPayloadOffset != 0) ? Owner->CameraPayloadOffset : Offset) : Offset;
			PARTICLE_ELEMENT(FCameraOffsetParticlePayload, CameraPayload);
			FLOAT CameraOffsetValue = CameraOffset.GetValue(Particle.RelativeTime, Owner->Component);
			if (UpdateMethod == EPCOUM_Additive)
			{
				CameraPayload.Offset += CameraOffsetValue;
			}
			else if (UpdateMethod == EPCOUM_Scalar)
			{
				CameraPayload.Offset *= CameraOffsetValue;
			}
			else //if (UpdateMethod == EPCOUM_DirectSet)
			{
				CameraPayload.Offset = CameraOffsetValue;
			}
		}
		END_UPDATE_LOOP;
	}
}

/**
 *	Returns the number of bytes that the module requires in the particle payload block.
 *
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return	UINT		The number of bytes the module needs per particle.
 */
UINT UParticleModuleCameraOffset::RequiredBytes(FParticleEmitterInstance* Owner)
{
	if ((Owner == NULL) || (Owner->CameraPayloadOffset == 0))
	{
		return sizeof(FCameraOffsetParticlePayload);
	}
	// Only the first module needs to setup the payload
	return 0;
}
