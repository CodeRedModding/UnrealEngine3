/*=============================================================================
	ParticleModules_Location.cpp: 
	Location-related particle module implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EngineAnimClasses.h"
#include "UnParticleHelper.h"

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocationBase);

/*-----------------------------------------------------------------------------
	UParticleModuleLocation implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocation);

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SpawnEx(Owner, Offset, SpawnTime, NULL);
}

/**
 *	Extended version of spawn, allows for using a random stream for distribution value retrieval
 *
 *	@param	Owner				The particle emitter instance that is spawning
 *	@param	Offset				The offset to the modules payload data
 *	@param	SpawnTime			The time of the spawn
 *	@param	InRandomStream		The random stream to use for retrieving random values
 */
void UParticleModuleLocation::SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream)
{
	SPAWN_INIT;
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	FVector Location;

	if (DistributeOverNPoints != 0.0f)
	{
		FLOAT RandomNum = appSRand() * appFractional(Owner->EmitterTime);

		if(RandomNum > DistributeThreshold)
		{
			Location = StartLocation.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);
		}
		else
		{
			FVector Min, Max;
			StartLocation.Distribution->GetRange(Min, Max);
			FVector Lerped = Lerp(Min, Max, appTruncFloat((appSRand() * (DistributeOverNPoints - 1.0f)) + 0.5f)/(DistributeOverNPoints - 1.0f));
			Location.Set(Lerped.X, Lerped.Y, Lerped.Z);
		}
	}
	else
	{
		Location = StartLocation.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);
	}

	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		Particle.Location += Location;
	}
	else
	{
		Location = Owner->Component->LocalToWorld.TransformNormal(Location);
		Particle.Location += Location;
	}
}

void UParticleModuleLocation::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITOR
	// Draw the location as a wire star
	FVector Position = FVector(0.0f);

	if (StartLocation.Distribution)
	{
		// Nothing else to do if it is constant...
		if (StartLocation.Distribution->IsA(UDistributionVectorUniform::StaticClass()))
		{
			// Draw a box showing the min/max extents
			UDistributionVectorUniform* Uniform = CastChecked<UDistributionVectorUniform>(StartLocation.Distribution);
			
			Position = (Uniform->GetMaxValue() + Uniform->GetMinValue()) / 2.0f;

			FVector MinValue = Uniform->GetMinValue();
			FVector MaxValue = Uniform->GetMaxValue();

			FMatrix LocalToWorld;
			LocalToWorld.SetIdentity();
			if ((Owner != NULL) && (Owner->Component != NULL))
			{
				LocalToWorld = Owner->Component->LocalToWorld;
			}
			FVector Extent = (MaxValue - MinValue) / 2.0f;
			FVector Offset = (MaxValue + MinValue) / 2.0f;
			// We just want to rotate the offset
			Offset = LocalToWorld.TransformNormal(Offset);
			DrawOrientedWireBox(PDI, LocalToWorld.GetOrigin() + Offset, 
				LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(1), LocalToWorld.GetAxis(2), 
				Extent, ModuleEditorColor, SDPG_World);
		}
		else if (StartLocation.Distribution->IsA(UDistributionVectorConstantCurve::StaticClass()))
		{
			// Draw a box showing the min/max extents
			UDistributionVectorConstantCurve* Curve = CastChecked<UDistributionVectorConstantCurve>(StartLocation.Distribution);

			//Curve->
			Position = StartLocation.GetValue(0.0f, Owner->Component);
		}
		else if (StartLocation.Distribution->IsA(UDistributionVectorConstant::StaticClass()))
		{
			Position = StartLocation.GetValue(0.0f, Owner->Component);
		}
	}

	if ((Owner != NULL) && (Owner->Component != NULL))
	{
		Position = Owner->Component->LocalToWorld.TransformFVector(Position);
	}
	DrawWireStar(PDI, Position, 10.0f, ModuleEditorColor, SDPG_World);
#endif	//#if WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocation_Seeded implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocation_Seeded);

/**
 *	Called on a particle when it is spawned.
 *
 *	@param	Owner			The emitter instance that spawned the particle
 *	@param	Offset			The payload data offset for this module
 *	@param	SpawnTime		The spawn time of the particle
 */
void UParticleModuleLocation_Seeded::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
	SpawnEx(Owner, Offset, SpawnTime, (Payload != NULL) ? &(Payload->RandomStream) : NULL);
}

/**
 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return UINT		The number of bytes the module needs per emitter instance.
 */
UINT UParticleModuleLocation_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleLocation_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

/** 
 *	Called when an emitter instance is looping...
 *
 *	@param	Owner	The emitter instance that owns this module
 */
void UParticleModuleLocation_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == TRUE)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationWorldOffset implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocationWorldOffset);

void UParticleModuleLocationWorldOffset::SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream)
{
	SPAWN_INIT;
	UParticleLODLevel* LODLevel = Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	if (LODLevel->RequiredModule->bUseLocalSpace == FALSE)
	{
		// Nothing to do here... the distribution value is already being in world space
		Particle.Location += StartLocation.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);
	}
	else
	{
		// We need to inverse transform the location so that the bUseLocalSpace transform uses the proper value
		FMatrix InvMat = Owner->Component->LocalToWorld.Inverse();
		FVector StartLoc = StartLocation.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);
		Particle.Location += InvMat.TransformNormal(StartLoc);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationWorldOffset_Seeded implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocationWorldOffset_Seeded);

/**
 *	Called on a particle when it is spawned.
 *
 *	@param	Owner			The emitter instance that spawned the particle
 *	@param	Offset			The payload data offset for this module
 *	@param	SpawnTime		The spawn time of the particle
 */
void UParticleModuleLocationWorldOffset_Seeded::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
	SpawnEx(Owner, Offset, SpawnTime, (Payload != NULL) ? &(Payload->RandomStream) : NULL);
}

/**
 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return UINT		The number of bytes the module needs per emitter instance.
 */
UINT UParticleModuleLocationWorldOffset_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleLocationWorldOffset_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

/** 
 *	Called when an emitter instance is looping...
 *
 *	@param	Owner	The emitter instance that owns this module
 */
void UParticleModuleLocationWorldOffset_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == TRUE)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationDirect implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocationDirect);

void UParticleModuleLocationDirect::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;

	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		Particle.Location = Location.GetValue(Particle.RelativeTime, Owner->Component);
	}
	else
	{
		FVector StartLoc	= Location.GetValue(Particle.RelativeTime, Owner->Component);
		StartLoc = Owner->Component->LocalToWorld.TransformFVector(StartLoc);
		Particle.Location	= StartLoc;
	}

	PARTICLE_ELEMENT(FVector, LocOffset);
	LocOffset	= LocationOffset.GetValue(Owner->EmitterTime, Owner->Component);
	Particle.Location += LocOffset;
}

void UParticleModuleLocationDirect::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	BEGIN_UPDATE_LOOP;
	{
		FVector	NewLoc;
		UParticleLODLevel* LODLevel = Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
		check(LODLevel);
		if (LODLevel->RequiredModule->bUseLocalSpace)
		{
			NewLoc = Location.GetValue(Particle.RelativeTime, Owner->Component);
		}
		else
		{
			FVector Loc			= Location.GetValue(Particle.RelativeTime, Owner->Component);
			Loc = Owner->Component->LocalToWorld.TransformFVector(Loc);
			NewLoc	= Loc;
		}

		FVector	Scale	= ScaleFactor.GetValue(Particle.RelativeTime, Owner->Component);

		PARTICLE_ELEMENT(FVector, LocOffset);
		NewLoc += LocOffset;

		FVector	Diff		 = (NewLoc - Particle.Location);
		FVector	ScaleDiffA	 = Diff * Scale.X;
		FVector	ScaleDiffB	 = Diff * (1.0f - Scale.X);
		Particle.Velocity	 = ScaleDiffA / DeltaTime;
		Particle.Location	+= ScaleDiffB;
	}
	END_UPDATE_LOOP;
}

UINT UParticleModuleLocationDirect::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FVector);
}

void UParticleModuleLocationDirect::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationEmitter implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocationEmitter);

//    class UParticleEmitter* LocationEmitter;
void UParticleModuleLocationEmitter::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	// We need to look up the emitter instance...
	// This may not need to be done every Spawn, but in the short term, it will to be safe.
	// (In the editor, the source emitter may be deleted, etc...)
	FParticleEmitterInstance* LocationEmitterInst = NULL;
	if (EmitterName != NAME_None)
	{
		for (INT ii = 0; ii < Owner->Component->EmitterInstances.Num(); ii++)
		{
			FParticleEmitterInstance* pkEmitInst = Owner->Component->EmitterInstances(ii);
			if (pkEmitInst && (pkEmitInst->SpriteTemplate->EmitterName == EmitterName))
			{
				LocationEmitterInst = pkEmitInst;
				break;
			}
		}
	}

	if (LocationEmitterInst == NULL)
	{
		// No source emitter, so we don't spawn??
		return;
	}

	check(LocationEmitterInst->CurrentLODLevel);
	check(LocationEmitterInst->CurrentLODLevel->RequiredModule);
	check(Owner->CurrentLODLevel);
	check(Owner->CurrentLODLevel->RequiredModule);
	UBOOL bSourceIsInLocalSpace = LocationEmitterInst->CurrentLODLevel->RequiredModule->bUseLocalSpace;
	UBOOL bInLocalSpace = Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace;

	SPAWN_INIT;
		{
			INT Index = 0;

			switch (SelectionMethod)
			{
			case ELESM_Random:
				{
					Index = appTrunc(appSRand() * LocationEmitterInst->ActiveParticles);
					if (Index >= LocationEmitterInst->ActiveParticles)
					{
						Index = LocationEmitterInst->ActiveParticles - 1;
					}
				}
				break;
			case ELESM_Sequential:
				{
					FLocationEmitterInstancePayload* Payload = 
						(FLocationEmitterInstancePayload*)(Owner->GetModuleInstanceData(this));
					if (Payload != NULL)
					{
						Index = ++(Payload->LastSelectedIndex);
						if (Index >= LocationEmitterInst->ActiveParticles)
						{
							Index = 0;
							Payload->LastSelectedIndex = Index;
						}
					}
					else
					{
						// There was an error...
						//@todo.SAS. How to resolve this situation??
					}
				}
				break;
			}
					
			// Grab a particle from the location emitter instance
			FBaseParticle* pkParticle = LocationEmitterInst->GetParticle(Index);
			if (pkParticle)
			{
				if ((pkParticle->RelativeTime == 0.0f) && (pkParticle->Location == FVector(0.0f)))
				{
					if (bInLocalSpace == FALSE)
					{
						Particle.Location = LocationEmitterInst->Component->LocalToWorld.GetOrigin();
					}
					else
					{
						Particle.Location = FVector(0.0f);
					}
				}
				else
				{
					if (bSourceIsInLocalSpace == bInLocalSpace)
					{
						// Just copy it directly
						Particle.Location = pkParticle->Location;
					}
					else if ((bSourceIsInLocalSpace == TRUE) && (bInLocalSpace == FALSE))
					{
						// We need to transform it into world space
						Particle.Location = LocationEmitterInst->Component->LocalToWorld.TransformFVector(pkParticle->Location);
					}
					else //if ((bSourceIsInLocalSpace == FALSE) && (bInLocalSpace == TRUE))
					{
						// We need to transform it into local space
						Particle.Location = LocationEmitterInst->Component->LocalToWorld.InverseTransformFVector(pkParticle->Location);
					}
				}
				if (InheritSourceVelocity)
				{
					Particle.BaseVelocity	+= pkParticle->Velocity * InheritSourceVelocityScale;
					Particle.Velocity		+= pkParticle->Velocity * InheritSourceVelocityScale;
				}

				if (bInheritSourceRotation)
				{
					// If the ScreenAlignment of the source emitter is PSA_Velocity, 
					// and that of the local is not, then the rotation will NOT be correct!
					Particle.Rotation		+= pkParticle->Rotation * InheritSourceRotationScale;
				}
			}
		}
} 

UINT UParticleModuleLocationEmitter::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return sizeof(FLocationEmitterInstancePayload);
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationEmitterDirect implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocationEmitterDirect);

void UParticleModuleLocationEmitterDirect::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	// We need to look up the emitter instance...
	// This may not need to be done every Spawn, but in the short term, it will to be safe.
	// (In the editor, the source emitter may be deleted, etc...)
	FParticleEmitterInstance* LocationEmitterInst = NULL;
	if (EmitterName != NAME_None)
	{
		for (INT ii = 0; ii < Owner->Component->EmitterInstances.Num(); ii++)
		{
			FParticleEmitterInstance* pkEmitInst = Owner->Component->EmitterInstances(ii);
			if (pkEmitInst && (pkEmitInst->SpriteTemplate->EmitterName == EmitterName))
			{
				LocationEmitterInst = pkEmitInst;
				break;
			}
		}
	}

	if (LocationEmitterInst == NULL)
	{
		// No source emitter, so we don't spawn??
		return;
	}

	SPAWN_INIT;
		INT Index = Owner->ActiveParticles;

		// Grab a particle from the location emitter instance
		FBaseParticle* pkParticle = LocationEmitterInst->GetParticle(Index);
		if (pkParticle)
		{
			Particle.Location		= pkParticle->Location;
			Particle.OldLocation	= pkParticle->OldLocation;
			Particle.Velocity		= pkParticle->Velocity;
			Particle.RelativeTime	= pkParticle->RelativeTime;
		}
} 

void UParticleModuleLocationEmitterDirect::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	// We need to look up the emitter instance...
	// This may not need to be done every Spawn, but in the short term, it will to be safe.
	// (In the editor, the source emitter may be deleted, etc...)
	FParticleEmitterInstance* LocationEmitterInst = NULL;
	if (EmitterName != NAME_None)
	{
		for (INT ii = 0; ii < Owner->Component->EmitterInstances.Num(); ii++)
		{
			FParticleEmitterInstance* pkEmitInst = Owner->Component->EmitterInstances(ii);
			if (pkEmitInst && (pkEmitInst->SpriteTemplate->EmitterName == EmitterName))
			{
				LocationEmitterInst = pkEmitInst;
				break;
			}
		}
	}

	if (LocationEmitterInst == NULL)
	{
		// No source emitter, so we don't spawn??
		return;
	}

	BEGIN_UPDATE_LOOP;
		{
			// Grab a particle from the location emitter instance
			FBaseParticle* pkParticle = LocationEmitterInst->GetParticle(i);
			if (pkParticle)
			{
				Particle.Location		= pkParticle->Location;
				Particle.OldLocation	= pkParticle->OldLocation;
				Particle.Velocity		= pkParticle->Velocity;
				Particle.RelativeTime	= pkParticle->RelativeTime;
			}
		}
	END_UPDATE_LOOP;
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationPrimitiveBase implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocationPrimitiveBase);

void UParticleModuleLocationPrimitiveBase::DetermineUnitDirection(FParticleEmitterInstance* Owner, FVector& vUnitDir, class FRandomStream* InRandomStream)
{
	FVector vRand;

	// Grab 3 random numbers for the axes
	if (InRandomStream == NULL)
	{
		vRand.X	= appSRand();
		vRand.Y = appSRand();
		vRand.Z = appSRand();
	}
	else
	{
		vRand.X	= InRandomStream->GetFraction();
		vRand.Y = InRandomStream->GetFraction();
		vRand.Z = InRandomStream->GetFraction();
	}

	// Set the unit dir
	if (Positive_X && Negative_X)
	{
		vUnitDir.X = vRand.X * 2 - 1;
	}
	else if (Positive_X)
	{
		vUnitDir.X = vRand.X;
	}
	else if (Negative_X)
	{
		vUnitDir.X = -vRand.X;
	}
	else
	{
		vUnitDir.X = 0.0f;
	}

	if (Positive_Y && Negative_Y)
	{
		vUnitDir.Y = vRand.Y * 2 - 1;
	}
	else if (Positive_Y)
	{
		vUnitDir.Y = vRand.Y;
	}
	else if (Negative_Y)
	{
		vUnitDir.Y = -vRand.Y;
	}
	else
	{
		vUnitDir.Y = 0.0f;
	}

	if (Positive_Z && Negative_Z)
	{
		vUnitDir.Z = vRand.Z * 2 - 1;
	}
	else if (Positive_Z)
	{
		vUnitDir.Z = vRand.Z;
	}
	else if (Negative_Z)
	{
		vUnitDir.Z = -vRand.Z;
	}
	else
	{
		vUnitDir.Z = 0.0f;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationPrimitiveCylinder implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocationPrimitiveCylinder);

void UParticleModuleLocationPrimitiveCylinder::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SpawnEx(Owner, Offset, SpawnTime, NULL);
}

/**
 *	Extended version of spawn, allows for using a random stream for distribution value retrieval
 *
 *	@param	Owner				The particle emitter instance that is spawning
 *	@param	Offset				The offset to the modules payload data
 *	@param	SpawnTime			The time of the spawn
 *	@param	InRandomStream		The random stream to use for retrieving random values
 */
void UParticleModuleLocationPrimitiveCylinder::SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream)
{
	SPAWN_INIT;

	INT	RadialIndex0	= 0;	//X
	INT	RadialIndex1	= 1;	//Y
	INT	HeightIndex		= 2;	//Z

	switch (HeightAxis)
	{
	case PMLPC_HEIGHTAXIS_X:
		RadialIndex0	= 1;	//Y
		RadialIndex1	= 2;	//Z
		HeightIndex		= 0;	//X
		break;
	case PMLPC_HEIGHTAXIS_Y:
		RadialIndex0	= 0;	//X
		RadialIndex1	= 2;	//Z
		HeightIndex		= 1;	//Y
		break;
	case PMLPC_HEIGHTAXIS_Z:
		break;
	}

	// Determine the start location for the sphere
	FVector vStartLoc = StartLocation.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);

	FVector vOffset(0.0f);
	FLOAT	fStartRadius	= StartRadius.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
	FLOAT	fStartHeight	= StartHeight.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream) / 2.0f;


	// Determine the unit direction
	FVector vUnitDir, vUnitDirTemp;

	UBOOL bFoundValidValue = FALSE;
	INT NumberOfAttempts = 0;
	FLOAT RadiusSquared = fStartRadius * fStartRadius;
	while (!bFoundValidValue)
	{
		DetermineUnitDirection(Owner, vUnitDirTemp, InRandomStream);
		vUnitDir[RadialIndex0]	= vUnitDirTemp[RadialIndex0];
		vUnitDir[RadialIndex1]	= vUnitDirTemp[RadialIndex1];
		vUnitDir[HeightIndex]	= vUnitDirTemp[HeightIndex];

		FVector2D CheckVal(vUnitDir[RadialIndex0] * fStartRadius, vUnitDir[RadialIndex1] * fStartRadius);
		if (CheckVal.SizeSquared() <= RadiusSquared)
		{
			bFoundValidValue = TRUE;
		}
		else if (NumberOfAttempts >= 50)
		{
			// Just pass the value thru. 
			// It will clamp to the 'circle' but we tried...
			bFoundValidValue = TRUE;
		}
		NumberOfAttempts++;
	}

	FVector vNormalizedDir = vUnitDir;
	vNormalizedDir.Normalize();

	FVector2D vUnitDir2D(vUnitDir[RadialIndex0], vUnitDir[RadialIndex1]);
	FVector2D vNormalizedDir2D = vUnitDir2D.SafeNormal();

	// Determine the position
	// Always want Z in the [-Height, Height] range
	vOffset[HeightIndex] = vUnitDir[HeightIndex] * fStartHeight;

	vNormalizedDir[RadialIndex0] = vNormalizedDir2D.X;
	vNormalizedDir[RadialIndex1] = vNormalizedDir2D.Y;

	if (SurfaceOnly)
	{
		// Clamp the X,Y to the outer edge...
		if (appIsNearlyZero(Abs(vOffset[HeightIndex]) - fStartHeight))
		{
			// On the caps, it can be anywhere within the 'circle'
			vOffset[RadialIndex0] = vUnitDir[RadialIndex0] * fStartRadius;
			vOffset[RadialIndex1] = vUnitDir[RadialIndex1] * fStartRadius;
		}
		else
		{
			// On the sides, it must be on the 'circle'
			vOffset[RadialIndex0] = vNormalizedDir[RadialIndex0] * fStartRadius;
			vOffset[RadialIndex1] = vNormalizedDir[RadialIndex1] * fStartRadius;
		}
	}
	else
	{
		vOffset[RadialIndex0] = vUnitDir[RadialIndex0] * fStartRadius;
		vOffset[RadialIndex1] = vUnitDir[RadialIndex1] * fStartRadius;
	}

	// Clamp to the radius...
	FVector	vMax;

	vMax[RadialIndex0]	= Abs(vNormalizedDir[RadialIndex0]) * fStartRadius;
	vMax[RadialIndex1]	= Abs(vNormalizedDir[RadialIndex1]) * fStartRadius;
	vMax[HeightIndex]	= fStartHeight;

	vOffset[RadialIndex0]	= Clamp<FLOAT>(vOffset[RadialIndex0], -vMax[RadialIndex0], vMax[RadialIndex0]);
	vOffset[RadialIndex1]	= Clamp<FLOAT>(vOffset[RadialIndex1], -vMax[RadialIndex1], vMax[RadialIndex1]);
	vOffset[HeightIndex]	= Clamp<FLOAT>(vOffset[HeightIndex],  -vMax[HeightIndex],  vMax[HeightIndex]);

	// Add in the start location
	vOffset[RadialIndex0]	+= vStartLoc[RadialIndex0];
	vOffset[RadialIndex1]	+= vStartLoc[RadialIndex1];
	vOffset[HeightIndex]	+= vStartLoc[HeightIndex];

	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	if (LODLevel->RequiredModule->bUseLocalSpace == FALSE)
	{
		vOffset = Owner->Component->LocalToWorld.TransformNormal(vOffset);
		if (bAdjustForWorldSpace)
		{
			vStartLoc = Owner->Component->LocalToWorld.TransformNormal(vStartLoc);
		}
	}
	Particle.Location += vOffset;

	if (Velocity)
	{
		FVector vVelocity;
		vVelocity[RadialIndex0]	= vOffset[RadialIndex0]	- vStartLoc[RadialIndex0];
		vVelocity[RadialIndex1]	= vOffset[RadialIndex1]	- vStartLoc[RadialIndex1];
		vVelocity[HeightIndex]	= vOffset[HeightIndex]	- vStartLoc[HeightIndex];

		if (RadialVelocity && (!bAdjustForWorldSpace || (LODLevel->RequiredModule->bUseLocalSpace == TRUE)))
		{
			vVelocity[HeightIndex]	= 0.0f;
		}
		vVelocity	*= VelocityScale.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);

		Particle.Velocity		+= vVelocity;
		Particle.BaseVelocity	+= vVelocity;
	}
}

void UParticleModuleLocationPrimitiveCylinder::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITOR
	// Draw the location as a wire star
	FVector Position = FVector(0.0f);
	FVector OwnerScale = FVector(1.0f);
	FMatrix LocalToWorld;
	FMatrix LocalToWorld_NoScale;
	LocalToWorld.SetIdentity();
	LocalToWorld_NoScale.SetIdentity();
	if ((Owner != NULL) && (Owner->Component != NULL))
	{
		LocalToWorld = Owner->Component->LocalToWorld;
		OwnerScale = LocalToWorld.GetScaleVector();
		LocalToWorld_NoScale = LocalToWorld.GetMatrixWithoutScale();
	}
	Position = LocalToWorld.TransformFVector(Position);
	DrawWireStar(PDI, Position, 10.0f, ModuleEditorColor, SDPG_World);

	if (StartLocation.Distribution)
	{
		if (StartLocation.Distribution->IsA(UDistributionVectorConstant::StaticClass()))
		{
			UDistributionVectorConstant* pkConstant = CastChecked<UDistributionVectorConstant>(StartLocation.Distribution);
			Position = pkConstant->Constant;
		}
		else
		if (StartLocation.Distribution->IsA(UDistributionVectorUniform::StaticClass()))
		{
			// Draw at the avg. of the min/max extents
			UDistributionVectorUniform* pkUniform = CastChecked<UDistributionVectorUniform>(StartLocation.Distribution);
			Position = (pkUniform->GetMaxValue() + pkUniform->GetMinValue()) / 2.0f;
		}
		else
		if (StartLocation.Distribution->IsA(UDistributionVectorConstantCurve::StaticClass()))
		{
			// Draw at the avg. of the min/max extents
			UDistributionVectorConstantCurve* pkCurve = CastChecked<UDistributionVectorConstantCurve>(StartLocation.Distribution);

			//pkCurve->
			Position = StartLocation.GetValue(0.0f, Owner->Component);
		}
	}

	// Draw a wire start at the center position
	Position = LocalToWorld.TransformFVector(Position);
	DrawWireStar(PDI,Position, 10.0f, ModuleEditorColor, SDPG_World);

	FLOAT fStartRadius = StartRadius.GetValue(Owner->EmitterTime, Owner->Component);
	FLOAT fStartHeight = StartHeight.GetValue(Owner->EmitterTime, Owner->Component) / 2.0f;

	FVector	TransformedAxis[3];
	FVector	Axis[3];

	TransformedAxis[0] = LocalToWorld_NoScale.TransformNormal(FVector(1.0f, 0.0f, 0.0f));
	TransformedAxis[1] = LocalToWorld_NoScale.TransformNormal(FVector(0.0f, 1.0f, 0.0f));
	TransformedAxis[2] = LocalToWorld_NoScale.TransformNormal(FVector(0.0f, 0.0f, 1.0f));

	switch (HeightAxis)
	{
	case PMLPC_HEIGHTAXIS_X:
		Axis[0]	= TransformedAxis[1];	//Y
		Axis[1]	= TransformedAxis[2];	//Z
		Axis[2]	= TransformedAxis[0];	//X
		fStartHeight *= OwnerScale.X;
		fStartRadius *= Max<FLOAT>(OwnerScale.Y, OwnerScale.Z);
		break;
	case PMLPC_HEIGHTAXIS_Y:
		Axis[0]	= TransformedAxis[0];	//X
		Axis[1]	= TransformedAxis[2];	//Z
		Axis[2]	= TransformedAxis[1];	//Y
		fStartHeight *= OwnerScale.Y;
		fStartRadius *= Max<FLOAT>(OwnerScale.X, OwnerScale.Z);
		break;
	case PMLPC_HEIGHTAXIS_Z:
		Axis[0]	= TransformedAxis[0];	//X
		Axis[1]	= TransformedAxis[1];	//Y
		Axis[2]	= TransformedAxis[2];	//Z
		fStartHeight *= OwnerScale.Z;
		fStartRadius *= Max<FLOAT>(OwnerScale.X, OwnerScale.Y);
		break;
	}

	DrawWireCylinder(PDI,Position, Axis[0], Axis[1], Axis[2], 
		ModuleEditorColor, fStartRadius, fStartHeight, 16, SDPG_World);
#endif	//#if WITH_EDITOR
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleLocationPrimitiveCylinder::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	// Make corrections in the particle's velocity when not using local space
	bAdjustForWorldSpace = TRUE;
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationPrimitiveCylinder_Seeded implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocationPrimitiveCylinder_Seeded);

/**
 *	Called on a particle when it is spawned.
 *
 *	@param	Owner			The emitter instance that spawned the particle
 *	@param	Offset			The payload data offset for this module
 *	@param	SpawnTime		The spawn time of the particle
 */
void UParticleModuleLocationPrimitiveCylinder_Seeded::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
	SpawnEx(Owner, Offset, SpawnTime, (Payload != NULL) ? &(Payload->RandomStream) : NULL);
}

/**
 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return UINT		The number of bytes the module needs per emitter instance.
 */
UINT UParticleModuleLocationPrimitiveCylinder_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleLocationPrimitiveCylinder_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

/** 
 *	Called when an emitter instance is looping...
 *
 *	@param	Owner	The emitter instance that owns this module
 */
void UParticleModuleLocationPrimitiveCylinder_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == TRUE)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationPrimitiveSphere implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocationPrimitiveSphere);

void UParticleModuleLocationPrimitiveSphere::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SpawnEx(Owner, Offset, SpawnTime, NULL);
}

/**
 *	Extended version of spawn, allows for using a random stream for distribution value retrieval
 *
 *	@param	Owner				The particle emitter instance that is spawning
 *	@param	Offset				The offset to the modules payload data
 *	@param	SpawnTime			The time of the spawn
 *	@param	InRandomStream		The random stream to use for retrieving random values
 */
void UParticleModuleLocationPrimitiveSphere::SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream)
{
	SPAWN_INIT;

	// Determine the start location for the sphere
	FVector vStartLoc = StartLocation.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);

	// Determine the unit direction
	FVector vUnitDir;
	DetermineUnitDirection(Owner, vUnitDir, InRandomStream);

	FVector vNormalizedDir = vUnitDir;
	vNormalizedDir.Normalize();

	// If we want to cover just the surface of the sphere...
	if (SurfaceOnly)
	{
		vUnitDir.Normalize();
	}

	// Determine the position
	FLOAT	fStartRadius	= StartRadius.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
	FVector vStartRadius	= FVector(fStartRadius);
	FVector vOffset			= vUnitDir * vStartRadius;

	// Clamp to the radius...
	FVector	vMax;

	vMax.X	= Abs(vNormalizedDir.X) * fStartRadius;
	vMax.Y	= Abs(vNormalizedDir.Y) * fStartRadius;
	vMax.Z	= Abs(vNormalizedDir.Z) * fStartRadius;

	if (Positive_X || Negative_X)
	{
		vOffset.X = Clamp<FLOAT>(vOffset.X, -vMax.X, vMax.X);
	}
	else
	{
		vOffset.X = 0.0f;
	}
	if (Positive_Y || Negative_Y)
	{
		vOffset.Y = Clamp<FLOAT>(vOffset.Y, -vMax.Y, vMax.Y);
	}
	else
	{
		vOffset.Y = 0.0f;
	}
	if (Positive_Z || Negative_Z)
	{
		vOffset.Z = Clamp<FLOAT>(vOffset.Z, -vMax.Z, vMax.Z);
	}
	else
	{
		vOffset.Z = 0.0f;
	}

	vOffset += vStartLoc;

	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	if (LODLevel->RequiredModule->bUseLocalSpace == FALSE)
	{
		vOffset = Owner->Component->LocalToWorld.TransformNormal(vOffset);
	}
	Particle.Location += vOffset;

	if (Velocity)
	{
		FVector vVelocity		 = (vOffset - vStartLoc) * VelocityScale.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
		Particle.Velocity		+= vVelocity;
		Particle.BaseVelocity	+= vVelocity;
	}
}

void UParticleModuleLocationPrimitiveSphere::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITOR
	FVector Position = FVector(0.0f);

	// Draw the location as a wire star
	if ((Owner != NULL) && (Owner->Component != NULL))
	{
		Position = Owner->Component->LocalToWorld.GetOrigin();
	}
	DrawWireStar(PDI, Position, 10.0f, ModuleEditorColor, SDPG_World);

	if (StartLocation.Distribution)
	{
		if (StartLocation.Distribution->IsA(UDistributionVectorConstant::StaticClass()))
		{
			UDistributionVectorConstant* pkConstant = CastChecked<UDistributionVectorConstant>(StartLocation.Distribution);
			Position = pkConstant->Constant;
		}
		else
		if (StartLocation.Distribution->IsA(UDistributionVectorUniform::StaticClass()))
		{
			// Draw at the avg. of the min/max extents
			UDistributionVectorUniform* pkUniform = CastChecked<UDistributionVectorUniform>(StartLocation.Distribution);
			Position = (pkUniform->GetMaxValue() + pkUniform->GetMinValue()) / 2.0f;
		}
		else
		if (StartLocation.Distribution->IsA(UDistributionVectorConstantCurve::StaticClass()))
		{
			// Draw at the avg. of the min/max extents
			UDistributionVectorConstantCurve* pkCurve = CastChecked<UDistributionVectorConstantCurve>(StartLocation.Distribution);

			//pkCurve->
			Position = StartLocation.GetValue(0.0f, Owner->Component);
		}
	}

	if ((Owner != NULL) && (Owner->Component != NULL))
	{
		Position = Owner->Component->LocalToWorld.TransformFVector(Position);
	}

	// Draw a wire start at the center position
	DrawWireStar(PDI,Position, 10.0f, ModuleEditorColor, SDPG_World);

	FLOAT	fRadius		= StartRadius.GetValue(Owner->EmitterTime, Owner->Component);
	INT		iNumSides	= 32;
	FVector	vAxis[3];

	vAxis[0]	= Owner->Component->LocalToWorld.GetAxis(0);
	vAxis[1]	= Owner->Component->LocalToWorld.GetAxis(1);
	vAxis[2]	= Owner->Component->LocalToWorld.GetAxis(2);

	DrawCircle(PDI,Position, vAxis[0], vAxis[1], ModuleEditorColor, fRadius, iNumSides, SDPG_World);
	DrawCircle(PDI,Position, vAxis[0], vAxis[2], ModuleEditorColor, fRadius, iNumSides, SDPG_World);
	DrawCircle(PDI,Position, vAxis[1], vAxis[2], ModuleEditorColor, fRadius, iNumSides, SDPG_World);
#endif	//#if WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationPrimitiveSphere_Seeded implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocationPrimitiveSphere_Seeded);

/**
 *	Called on a particle when it is spawned.
 *
 *	@param	Owner			The emitter instance that spawned the particle
 *	@param	Offset			The payload data offset for this module
 *	@param	SpawnTime		The spawn time of the particle
 */
void UParticleModuleLocationPrimitiveSphere_Seeded::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
	SpawnEx(Owner, Offset, SpawnTime, (Payload != NULL) ? &(Payload->RandomStream) : NULL);
}

/**
 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return UINT		The number of bytes the module needs per emitter instance.
 */
UINT UParticleModuleLocationPrimitiveSphere_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleLocationPrimitiveSphere_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

/** 
 *	Called when an emitter instance is looping...
 *
 *	@param	Owner	The emitter instance that owns this module
 */
void UParticleModuleLocationPrimitiveSphere_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == TRUE)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationBoneSocket implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocationBoneSocket);

/**
 *	Called when a property has change on an instance of the module.
 *
 *	@param	PropertyChangedEvent		Information on the change that occurred.
 */
void UParticleModuleLocationBoneSocket::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 *	Called on a particle that is freshly spawned by the emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	SpawnTime	The time of the spawn.
 */
void UParticleModuleLocationBoneSocket::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FModuleLocationBoneSocketInstancePayload* InstancePayload = 
		(FModuleLocationBoneSocketInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (InstancePayload == NULL)
	{
		return;
	}

	if (InstancePayload->SourceComponent == NULL)
	{
		// Setup the source skeletal mesh component...
		InstancePayload->SourceComponent = FParticleModuleUtils::GetSkeletalMeshComponentSource(Owner, SkelMeshActorParamName);
		if (InstancePayload->SourceComponent == NULL)
		{
			return;
		}
	}

	// Determine the bone/socket to spawn at
	INT SourceIndex = -1;
	if (SelectionMethod == BONESOCKETSEL_Sequential)
	{
		// Simply select the next socket
		SourceIndex = InstancePayload->LastSelectedIndex++;
		if (InstancePayload->LastSelectedIndex >= SourceLocations.Num())
		{
			InstancePayload->LastSelectedIndex = 0;
		}
	}
	else if (SelectionMethod == BONESOCKETSEL_Random)
	{
		// Note: This can select the same socket over and over...
		SourceIndex = appTrunc(appSRand() * (SourceLocations.Num() - 1));
		InstancePayload->LastSelectedIndex = SourceIndex;
	}
	else //BONESOCKETSEL_RandomExhaustive
	{
		if (InstancePayload->Indices[InstancePayload->CurrentUnused].Num() == 0)
		{
			InstancePayload->CurrentUnused = (InstancePayload->CurrentUnused == 0) ? 1 : 0;
		}
		BYTE TempIndex = appTrunc(appSRand() * InstancePayload->Indices[InstancePayload->CurrentUnused].Num());

		if(InstancePayload->Indices->Num() != 0)
		{
			SourceIndex = (InstancePayload->Indices[InstancePayload->CurrentUnused])(TempIndex);
			InstancePayload->Indices[(InstancePayload->CurrentUnused == 0) ? 1 : 0].AddItem(SourceIndex);
			InstancePayload->Indices[InstancePayload->CurrentUnused].Remove(TempIndex);
		}
		else
		{
			SourceIndex = -1;
		}
	}

	if (SourceIndex == -1)
	{
		// Failed to select a socket?
		return;
	}
	if (SourceIndex >= SourceLocations.Num())
	{
		return;
	}

	FVector SourceLocation;
	FParticleMeshEmitterInstance* MeshInst = (bOrientMeshEmitters == TRUE) ? 
		CastEmitterInstance<FParticleMeshEmitterInstance>(Owner) : NULL;
	FQuat RotationQuat;
	FQuat* SourceRotation = (MeshInst == NULL) ? NULL : &RotationQuat;
	if (GetParticleLocation(Owner, InstancePayload->SourceComponent, SourceIndex, SourceLocation, SourceRotation) == TRUE)
	{
		SPAWN_INIT
		{
			FModuleLocationBoneSocketParticlePayload* ParticlePayload = (FModuleLocationBoneSocketParticlePayload*)((BYTE*)&Particle + Offset);
			ParticlePayload->SourceIndex = SourceIndex;
			Particle.Location = SourceLocation;
			if ((MeshInst != NULL) && (MeshInst->MeshRotationActive == TRUE))
			{
				FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
				PayloadData->Rotation = RotationQuat.Euler();
				if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == TRUE)
				{
					PayloadData->Rotation = Owner->Component->LocalToWorld.InverseTransformNormalNoScale(PayloadData->Rotation);
				}
			}
			if(bInheritVelocityAtSpawn && LastUpdateTime != GWorld->GetTimeSeconds() && InstancePayload->LastPosition.IsValidIndex(SourceIndex))
			{
				Particle.BaseVelocity += (SourceLocation - InstancePayload->LastPosition(SourceIndex)) * ((1.0f / (GWorld->GetTimeSeconds() - LastUpdateTime)));
			}
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
void UParticleModuleLocationBoneSocket::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	if (bUpdatePositionEachFrame == FALSE && bInheritVelocityAtSpawn == FALSE)
	{
		return;
	}

	FModuleLocationBoneSocketInstancePayload* InstancePayload = 
		(FModuleLocationBoneSocketInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (InstancePayload->SourceComponent == NULL)
	{
		//@todo. Should we setup the source skeletal mesh component here too??
		return;
	}

	FVector SourceLocation;
	FParticleMeshEmitterInstance* MeshInst = (bOrientMeshEmitters == TRUE) ? 
		CastEmitterInstance<FParticleMeshEmitterInstance>(Owner) : NULL;
	FQuat RotationQuat;
	FQuat* SourceRotation = (MeshInst == NULL) ? NULL : &RotationQuat;

	if(bInheritVelocityAtSpawn && LastUpdateTime != GWorld->GetTimeSeconds())
	{
		InstancePayload->LastPosition.Empty();
		for(INT SourceIndex = 0; SourceIndex < SourceLocations.Num(); ++SourceIndex)
		{	
			GetParticleLocation(Owner, InstancePayload->SourceComponent, SourceIndex, SourceLocation, SourceRotation);
			InstancePayload->LastPosition.InsertItem(SourceLocation, SourceIndex);
		}
		LastUpdateTime = GWorld->GetTimeSeconds();
	}

	if(bUpdatePositionEachFrame == FALSE)
	{
		return;
	}

	BEGIN_UPDATE_LOOP;
	{
		FModuleLocationBoneSocketParticlePayload* ParticlePayload = (FModuleLocationBoneSocketParticlePayload*)((BYTE*)&Particle + Offset);
		if (GetParticleLocation(Owner, InstancePayload->SourceComponent, ParticlePayload->SourceIndex, SourceLocation, SourceRotation) == TRUE)
		{
			Particle.Location = SourceLocation;
			if ((MeshInst != NULL) && (MeshInst->MeshRotationActive == TRUE))
			{
				FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
				PayloadData->Rotation = RotationQuat.Euler();
				if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == TRUE)
				{
					PayloadData->Rotation = Owner->Component->LocalToWorld.InverseTransformNormalNoScale(PayloadData->Rotation);
				}
			}
		}
	}
	END_UPDATE_LOOP;
}

/**
 *	Called on an emitter when all other update operations have taken place
 *	INCLUDING bounding box cacluations!
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	DeltaTime	The time since the last update.
 */
void UParticleModuleLocationBoneSocket::FinalUpdate(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	Super::FinalUpdate(Owner, Offset, DeltaTime);

	FModuleLocationBoneSocketInstancePayload* InstancePayload = 
		(FModuleLocationBoneSocketInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (InstancePayload->SourceComponent == NULL)
	{
		//@todo. Should we setup the source skeletal mesh component here too??
		return;
	}

	UBOOL bHaveDeadParticles = FALSE;
	BEGIN_UPDATE_LOOP;
	{
		FModuleLocationBoneSocketParticlePayload* ParticlePayload = (FModuleLocationBoneSocketParticlePayload*)((BYTE*)&Particle + Offset);
		if (SourceType == BONESOCKETSOURCE_Sockets)
		{
			if (InstancePayload->SourceComponent && InstancePayload->SourceComponent->SkeletalMesh && SourceLocations.IsValidIndex(ParticlePayload->SourceIndex))
			{
				USkeletalMeshSocket* Socket = InstancePayload->SourceComponent->SkeletalMesh->FindSocket(SourceLocations(ParticlePayload->SourceIndex).BoneSocketName);
				if (Socket)
				{
					//@todo. Can we make this faster???
					INT BoneIndex = InstancePayload->SourceComponent->MatchRefBone(Socket->BoneName);
					if (BoneIndex != INDEX_NONE)
					{
						if ((InstancePayload->SourceComponent->IsBoneHidden(BoneIndex)) || 
							(InstancePayload->SourceComponent->GetBoneAtom(BoneIndex).GetScale() == 0.0f))
						{
							// Kill it
							Particle.RelativeTime = 1.1f;
							bHaveDeadParticles = TRUE;
						}
					}
				}
			}
		}
	}
	END_UPDATE_LOOP;

	if (bHaveDeadParticles == TRUE)
	{
		Owner->KillParticles();
	}
}

/**
 *	Returns the number of bytes that the module requires in the particle payload block.
 *
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return	UINT		The number of bytes the module needs per particle.
 */
UINT UParticleModuleLocationBoneSocket::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FModuleLocationBoneSocketParticlePayload);
}

/**
 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return UINT		The number of bytes the module needs per emitter instance.
 */
UINT UParticleModuleLocationBoneSocket::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return sizeof(FModuleLocationBoneSocketInstancePayload);
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleLocationBoneSocket::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	FModuleLocationBoneSocketInstancePayload* Payload = (FModuleLocationBoneSocketInstancePayload*)InstData;
	if (Payload)
	{
		appMemzero(Payload, sizeof(FModuleLocationBoneSocketInstancePayload));
		Payload->Indices[0].Empty(SourceLocations.Num());
		Payload->Indices[1].Empty(SourceLocations.Num());
		for (INT AddIdx = 0; AddIdx < SourceLocations.Num(); AddIdx++)
		{
			Payload->Indices[0].AddItem(AddIdx);
		}
		return 0;
	}

	return 0xffffffff;
}

/**
 *	Helper function used by the editor to auto-populate a placed AEmitter with any
 *	instance parameters that are utilized.
 *
 *	@param	PSysComp		The particle system component to be populated.
 */
void UParticleModuleLocationBoneSocket::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	UBOOL bFound = FALSE;
	for (INT ParamIdx = 0; ParamIdx < PSysComp->InstanceParameters.Num(); ParamIdx++)
	{
		FParticleSysParam* Param = &(PSysComp->InstanceParameters(ParamIdx));
		if (Param->Name == SkelMeshActorParamName)
		{
			bFound = TRUE;
			break;
		}
	}

	if (bFound == FALSE)
	{
		INT NewParamIndex = PSysComp->InstanceParameters.AddZeroed();
		PSysComp->InstanceParameters(NewParamIndex).Name = SkelMeshActorParamName;
		PSysComp->InstanceParameters(NewParamIndex).ParamType = PSPT_Actor;
		PSysComp->InstanceParameters(NewParamIndex).Actor = NULL;
	}
}

#if WITH_EDITOR
/**
 *	Get the number of custom entries this module has. Maximum of 3.
 *
 *	@return	INT		The number of custom menu entries
 */
INT UParticleModuleLocationBoneSocket::GetNumberOfCustomMenuOptions() const
{
	return 1;
}

/**
 *	Get the display name of the custom menu entry.
 *
 *	@param	InEntryIndex		The custom entry index (0-2)
 *	@param	OutDisplayString	The string to display for the menu
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleLocationBoneSocket::GetCustomMenuEntryDisplayString(INT InEntryIndex, FString& OutDisplayString) const
{
	if (InEntryIndex == 0)
	{
		OutDisplayString = LocalizeUnrealEd("Module_LocationBoneSocket_AutoFill");
		return TRUE;
	}
	return FALSE;
}

/**
 *	Perform the custom menu entry option.
 *
 *	@param	InEntryIndex		The custom entry index (0-2) to perform
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleLocationBoneSocket::PerformCustomMenuEntry(INT InEntryIndex)
{
	if (GIsEditor == TRUE)
	{
		if (InEntryIndex == 0)
		{
			// Fill in the socket names array with the skeletal mesh 
			if (EditorSkelMesh != NULL)
			{
				if (SourceType == BONESOCKETSOURCE_Sockets)
				{
					// Retrieve all the sockets
					if (EditorSkelMesh->Sockets.Num() > 0)
					{
						SourceLocations.Empty();
						for (INT SocketIdx = 0; SocketIdx < EditorSkelMesh->Sockets.Num(); SocketIdx++)
						{
							INT NewItemIdx = SourceLocations.AddZeroed();
							FLocationBoneSocketInfo& Info = SourceLocations(NewItemIdx);
							USkeletalMeshSocket* Socket = EditorSkelMesh->Sockets(SocketIdx);
							if (Socket != NULL)
							{
								Info.BoneSocketName = Socket->SocketName;
							}
							else
							{
								Info.BoneSocketName = NAME_None;
							}
						}
						return TRUE;
					}
					else
					{
						appMsgf(AMT_OK, *LocalizeUnrealEd("Module_LocationBoneSocket_EditorMeshNoSockets"));
					}
				}
				else //BONESOCKETSOURCE_Bones
				{
					// Retrieve all the bones
					if (EditorSkelMesh->RefSkeleton.Num() > 0)
					{
						SourceLocations.Empty();
						for (INT BoneIdx = 0; BoneIdx < EditorSkelMesh->RefSkeleton.Num(); BoneIdx++)
						{
							INT NewItemIdx = SourceLocations.AddZeroed();
							FLocationBoneSocketInfo& Info = SourceLocations(NewItemIdx);
							FMeshBone& Bone = EditorSkelMesh->RefSkeleton(BoneIdx);
							Info.BoneSocketName = Bone.Name;
						}
						return TRUE;
					}
					else
					{
						appMsgf(AMT_OK, *LocalizeUnrealEd("Module_LocationBoneSocket_EditorMeshNoBones"));
					}
				}
			}
			else
			{
				appMsgf(AMT_OK, *LocalizeUnrealEd("Module_LocationBoneSocket_NoEditorMesh"));
			}
		}
	}
	return FALSE;
}
#endif

/**
 *	Retrieve the position for the given socket index.
 *
 *	@param	Owner					The particle emitter instance that is being setup
 *	@param	InSkelMeshComponent		The skeletal mesh component to use as the source
 *	@param	InBoneSocketIndex		The index of the bone/socket of interest
 *	@param	OutPosition				The position for the particle location
 *	@param	OutRotation				Optional orientation for the particle (mesh emitters)
 *	
 *	@return	UBOOL					TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleLocationBoneSocket::GetParticleLocation(FParticleEmitterInstance* Owner, 
	USkeletalMeshComponent* InSkelMeshComponent, INT InBoneSocketIndex, 
	FVector& OutPosition, FQuat* OutRotation)
{
	check(InSkelMeshComponent);

	if (SourceType == BONESOCKETSOURCE_Sockets)
	{
		if (InSkelMeshComponent->SkeletalMesh)
		{
			USkeletalMeshSocket* Socket = InSkelMeshComponent->SkeletalMesh->FindSocket(SourceLocations(InBoneSocketIndex).BoneSocketName);
			if (Socket)
			{
				FVector SocketOffset = SourceLocations(InBoneSocketIndex).Offset + UniversalOffset;
				FRotator SocketRotator(0,0,0);
				FMatrix SocketMatrix;
 				if (Socket->GetSocketMatrixWithOffset(SocketMatrix, InSkelMeshComponent, SocketOffset, SocketRotator) == FALSE)
 				{
 					return FALSE;
 				}
 				OutPosition = SocketMatrix.GetOrigin();
 				if (OutRotation != NULL)
 				{
 					SocketMatrix.RemoveScaling();
 					*OutRotation = SocketMatrix.ToQuat();
 				}
			}
			else
			{
				return FALSE;
			}
		}
		else
		{
			return FALSE;
		}
	}
	else	//BONESOCKETSOURCE_Bones
	{
		INT BoneIndex = InSkelMeshComponent->MatchRefBone(SourceLocations(InBoneSocketIndex).BoneSocketName);
		if (BoneIndex != INDEX_NONE)
		{
			FVector SocketOffset = SourceLocations(InBoneSocketIndex).Offset + UniversalOffset;
			FMatrix WorldBoneTM = InSkelMeshComponent->GetBoneMatrix(BoneIndex);
			FTranslationMatrix OffsetMatrix(SocketOffset);
			FMatrix ResultMatrix = OffsetMatrix * WorldBoneTM;
			OutPosition = ResultMatrix.GetOrigin();
			if (OutRotation != NULL)
			{
				ResultMatrix.RemoveScaling();
				*OutRotation = ResultMatrix.ToQuat();
			}
		}
		else
		{
			return FALSE;
		}
	}

	if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == TRUE)
	{
		OutPosition = Owner->Component->LocalToWorld.InverseTransformFVector(OutPosition);
	}

	return TRUE;
}

/*-----------------------------------------------------------------------------
	UParticleModuleLocationVertSurface implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocationSkelVertSurface);

DECLARE_CYCLE_STAT(TEXT("Particle SkelMeshSurf Time"),STAT_ParticleSkelMeshSurfTime,STATGROUP_Particles);


/**
 *	Called after loading the module.
 */
void UParticleModuleLocationSkelVertSurface::PostLoad()
{
	Super::PostLoad();

	if(NormalCheckToleranceDegrees > 180.0f)
	{
		NormalCheckToleranceDegrees = 180.0f;
	}
	else if(NormalCheckToleranceDegrees < 0.0f)
	{
		NormalCheckToleranceDegrees = 0.0f;
	}

	NormalCheckTolerance = ((1.0f-(NormalCheckToleranceDegrees/180.0f))*2.0f)-1.0f;
}

/**
 *	Called when a property has change on an instance of the module.
 *
 *	@param	PropertyChangedEvent		Information on the change that occurred.
 */
void UParticleModuleLocationSkelVertSurface::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(PropertyChangedEvent.Property->GetName() == "NormalCheckToleranceDegrees")
	{
		if(NormalCheckToleranceDegrees > 180.0f)
		{
			NormalCheckToleranceDegrees = 180.0f;
		}
		else if(NormalCheckToleranceDegrees < 0.0f)
		{
			NormalCheckToleranceDegrees = 0.0f;
		}

		NormalCheckTolerance = ((1.0f-(NormalCheckToleranceDegrees/180.0f))*2.0f)-1.0f;
	}
}

/**
 *	Called on a particle that is freshly spawned by the emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	SpawnTime	The time of the spawn.
 */
void UParticleModuleLocationSkelVertSurface::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleSkelMeshSurfTime);
	FModuleLocationVertSurfaceInstancePayload* InstancePayload = 
		(FModuleLocationVertSurfaceInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (InstancePayload == NULL)
	{
		return;
	}

	if (InstancePayload->SourceComponent == NULL)
	{
		// Setup the source skeletal mesh component...
		InstancePayload->SourceComponent = FParticleModuleUtils::GetSkeletalMeshComponentSource(Owner, SkelMeshActorParamName);
		if (InstancePayload->SourceComponent == NULL)
		{
			return;
		}
	}

	INT TotalTris = 0;
	if(SourceType == VERTSURFACESOURCE_Surface)
	{
		for(INT CurrentSection(0); CurrentSection < InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).Sections.Num(); ++CurrentSection)
		{
			TotalTris += InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).Sections(CurrentSection).NumTriangles;
		}
	}

	// Determine the bone/socket to spawn at
	INT SourceIndex = -1;
	INT SourcesChecked = 0;
	UBOOL bValidIndex = FALSE;
	UBOOL bSourcesLeft = TRUE;
	while(!bValidIndex && bSourcesLeft)
	{
		if (SourceType == VERTSURFACESOURCE_Vert)
		{
			INT SourceLocationsCount(InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).VertexBufferGPUSkin.GetNumVertices());

			if(SourceLocationsCount == 0 || InstancePayload->ValidAssociatedBoneIndices.Num() == 0)
			{
				bSourcesLeft = FALSE;
			}

			SourceIndex = appTrunc(appSRand() * ((FLOAT)SourceLocationsCount) - 1);

			if(SourceIndex != -1)
			{
				if(FParticleModuleUtils::VertInfluencedByActiveBone(Owner, InstancePayload->SourceComponent, SourceIndex, InstancePayload->ValidAssociatedBoneIndices, InstancePayload->ValidAssociatedMaterialIndices))
				{
					bValidIndex = TRUE;
				}
				else
				{
					++SourcesChecked;

					if(SourcesChecked > SourceLocationsCount)
					{
						bSourcesLeft = FALSE;
					}
				}
			}
		}
		else if(SourceType == VERTSURFACESOURCE_Surface)
		{
			INT SectionCount = InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).Sections.Num();

			if(SectionCount == 0 || InstancePayload->ValidAssociatedBoneIndices.Num() == 0)
			{
				bSourcesLeft = FALSE;
			}

			INT RandomSection = appRound(appSRand() * ((FLOAT)SectionCount-1));

			SourceIndex = InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).Sections(RandomSection).BaseIndex +
				(appTrunc(appSRand() * ((FLOAT)InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).Sections(RandomSection).NumTriangles))*3);

			if(SourceIndex != -1)
			{
				INT VertIndex[3];

				VertIndex[0] = InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).MultiSizeIndexContainer.GetIndexBuffer()->Get( SourceIndex );
				VertIndex[1] = InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).MultiSizeIndexContainer.GetIndexBuffer()->Get( SourceIndex+1 );
				VertIndex[2] = InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).MultiSizeIndexContainer.GetIndexBuffer()->Get( SourceIndex+2 );

				if(FParticleModuleUtils::VertInfluencedByActiveBone(Owner, InstancePayload->SourceComponent, VertIndex[0], InstancePayload->ValidAssociatedBoneIndices, InstancePayload->ValidAssociatedMaterialIndices) &&
					FParticleModuleUtils::VertInfluencedByActiveBone(Owner, InstancePayload->SourceComponent, VertIndex[1], InstancePayload->ValidAssociatedBoneIndices, InstancePayload->ValidAssociatedMaterialIndices) && 
					FParticleModuleUtils::VertInfluencedByActiveBone(Owner, InstancePayload->SourceComponent, VertIndex[2], InstancePayload->ValidAssociatedBoneIndices, InstancePayload->ValidAssociatedMaterialIndices))
				{
					bValidIndex = TRUE;
				}
				else
				{
					++SourcesChecked;

					if(SourcesChecked > TotalTris)
					{
						bSourcesLeft = FALSE;
					}
				}
			}
		}
	}

	if (SourceIndex == -1)
	{
		// Failed to select a vert/face?
		SPAWN_INIT
		{
			Particle.RelativeTime = 1.1f;
			Owner->KillParticles();
		}
		return;
	}

	FVector SourceLocation;
	FParticleMeshEmitterInstance* MeshInst = (bOrientMeshEmitters == TRUE) ? 
		CastEmitterInstance<FParticleMeshEmitterInstance>(Owner) : NULL;
	FQuat RotationQuat;
	FQuat* SourceRotation = (MeshInst == NULL) ? NULL : &RotationQuat;
	if (GetParticleLocation(Owner, InstancePayload->SourceComponent, SourceIndex, SourceLocation, SourceRotation, TRUE) == TRUE)
	{
		SPAWN_INIT
		{
			FModuleLocationVertSurfaceParticlePayload* ParticlePayload = (FModuleLocationVertSurfaceParticlePayload*)((BYTE*)&Particle + Offset);
			ParticlePayload->SourceIndex = SourceIndex;
			Particle.Location = SourceLocation;
			if ((MeshInst != NULL) && (MeshInst->MeshRotationActive == TRUE))
			{
				FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
				PayloadData->Rotation = RotationQuat.Euler();
				if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == TRUE)
				{
					PayloadData->Rotation = Owner->Component->LocalToWorld.InverseTransformNormalNoScale(PayloadData->Rotation);
				}
			}
		}
	}
	else
	{
		SPAWN_INIT
		{
			Particle.RelativeTime = 1.1f;
			Owner->KillParticles();
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
void UParticleModuleLocationSkelVertSurface::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleSkelMeshSurfTime);
	if (bUpdatePositionEachFrame == FALSE)
	{
		return;
	}

	FModuleLocationVertSurfaceInstancePayload* InstancePayload = 
		(FModuleLocationVertSurfaceInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (InstancePayload->SourceComponent == NULL)
	{
		//@todo. Should we setup the source skeletal mesh component here too??
		return;
	}

	FVector SourceLocation;
	FParticleMeshEmitterInstance* MeshInst = (bOrientMeshEmitters == TRUE) ? 
		CastEmitterInstance<FParticleMeshEmitterInstance>(Owner) : NULL;
	FQuat RotationQuat;
	FQuat* SourceRotation = (MeshInst == NULL) ? NULL : &RotationQuat;
	BEGIN_UPDATE_LOOP;
	{
		FModuleLocationVertSurfaceParticlePayload* ParticlePayload = (FModuleLocationVertSurfaceParticlePayload*)((BYTE*)&Particle + Offset);
		if (GetParticleLocation(Owner, InstancePayload->SourceComponent, ParticlePayload->SourceIndex, SourceLocation, SourceRotation) == TRUE)
		{
			Particle.Location = SourceLocation;
			if ((MeshInst != NULL) && (MeshInst->MeshRotationActive == TRUE))
			{
				FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
				PayloadData->Rotation = RotationQuat.Euler();
				if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == TRUE)
				{
					PayloadData->Rotation = Owner->Component->LocalToWorld.InverseTransformNormalNoScale(PayloadData->Rotation);
				}
			}
		}
	}
	END_UPDATE_LOOP;
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleLocationSkelVertSurface::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	FModuleLocationVertSurfaceInstancePayload* InstancePayload = 
		(FModuleLocationVertSurfaceInstancePayload*)(Owner->GetModuleInstanceData(this));

	FParticleModuleUtils::UpdateBoneIndicesList(Owner, ValidAssociatedBones, SkelMeshActorParamName, InstancePayload->ValidAssociatedBoneIndices);

	InstancePayload->ValidAssociatedMaterialIndices = ValidMaterialIndices;

	return Super::PrepPerInstanceBlock(Owner, InstData);
}

/**
 *	Returns the number of bytes that the module requires in the particle payload block.
 *
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return	UINT		The number of bytes the module needs per particle.
 */
UINT UParticleModuleLocationSkelVertSurface::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FModuleLocationVertSurfaceParticlePayload);
}

/**
 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return UINT		The number of bytes the module needs per emitter instance.
 */
UINT UParticleModuleLocationSkelVertSurface::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return sizeof(FModuleLocationVertSurfaceInstancePayload);
}

/**
 *	Helper function used by the editor to auto-populate a placed AEmitter with any
 *	instance parameters that are utilized.
 *
 *	@param	PSysComp		The particle system component to be populated.
 */
void UParticleModuleLocationSkelVertSurface::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	UBOOL bFound = FALSE;
	for (INT ParamIdx = 0; ParamIdx < PSysComp->InstanceParameters.Num(); ParamIdx++)
	{
		FParticleSysParam* Param = &(PSysComp->InstanceParameters(ParamIdx));
		if (Param->Name == SkelMeshActorParamName)
		{
			bFound = TRUE;
			break;
		}
	}

	if (bFound == FALSE)
	{
		INT NewParamIndex = PSysComp->InstanceParameters.AddZeroed();
		PSysComp->InstanceParameters(NewParamIndex).Name = SkelMeshActorParamName;
		PSysComp->InstanceParameters(NewParamIndex).ParamType = PSPT_Actor;
		PSysComp->InstanceParameters(NewParamIndex).Actor = NULL;
	}
}

#if WITH_EDITOR
/**
 *	Get the number of custom entries this module has. Maximum of 3.
 *
 *	@return	INT		The number of custom menu entries
 */
INT UParticleModuleLocationSkelVertSurface::GetNumberOfCustomMenuOptions() const
{
	return 1;
}

/**
 *	Get the display name of the custom menu entry.
 *
 *	@param	InEntryIndex		The custom entry index (0-2)
 *	@param	OutDisplayString	The string to display for the menu
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleLocationSkelVertSurface::GetCustomMenuEntryDisplayString(INT InEntryIndex, FString& OutDisplayString) const
{
	if (InEntryIndex == 0)
	{
		OutDisplayString = LocalizeUnrealEd("Module_LocationVertSurface_AutoFill");
		return TRUE;
	}
	return FALSE;
}

/**
 *	Perform the custom menu entry option.
 *
 *	@param	InEntryIndex		The custom entry index (0-2) to perform
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleLocationSkelVertSurface::PerformCustomMenuEntry(INT InEntryIndex)
{
	if (GIsEditor == TRUE)
	{
		if (InEntryIndex == 0)
		{
			// Fill in the socket names array with the skeletal mesh 
			if (EditorSkelMesh != NULL)
			{
				// Retrieve all the bones
				if (EditorSkelMesh->RefSkeleton.Num() > 0)
				{
					ValidAssociatedBones.Empty();
					for (INT BoneIdx = 0; BoneIdx < EditorSkelMesh->RefSkeleton.Num(); BoneIdx++)
					{
						INT NewItemIdx = ValidAssociatedBones.AddZeroed();
						FMeshBone& Bone = EditorSkelMesh->RefSkeleton(BoneIdx);
						ValidAssociatedBones(NewItemIdx) = Bone.Name;
					}
				}
				else
				{
					appMsgf(AMT_OK, *LocalizeUnrealEd("Module_LocationBoneSocket_EditorMeshNoBones"));
				}
			}
			else
			{
				appMsgf(AMT_OK, *LocalizeUnrealEd("Module_LocationBoneSocket_NoEditorMesh"));
			}
		}
	}
	return FALSE;
}
#endif

/**
 *	Retrieve the position for the given socket index.
 *
 *	@param	Owner					The particle emitter instance that is being setup
 *	@param	InSkelMeshComponent		The skeletal mesh component to use as the source
 *	@param	InPrimaryVertexIndex	The index of the only vertex (vert mode) or the first vertex (surface mode)
 *	@param	OutPosition				The position for the particle location
 *	@param	OutRotation				Optional orientation for the particle (mesh emitters)
 *  @param  bSpawning				When TRUE and when using normal check on surfaces, will return false if the check fails.
 *	
 *	@return	UBOOL					TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleLocationSkelVertSurface::GetParticleLocation(FParticleEmitterInstance* Owner, 
	USkeletalMeshComponent* InSkelMeshComponent, INT InPrimaryVertexIndex, 
	FVector& OutPosition, FQuat* OutRotation, UBOOL bSpawning /* = FALSE*/)
{
	check(InSkelMeshComponent);

	if (InSkelMeshComponent->SkeletalMesh)
	{
		if (SourceType == VERTSURFACESOURCE_Vert)
		{
			FVector VertPos = InSkelMeshComponent->GetSkinnedVertexPosition(InPrimaryVertexIndex);
			OutPosition = InSkelMeshComponent->LocalToWorld.TransformFVector(VertPos);
			if (OutRotation != NULL)
			{
				*OutRotation = FRotator(0, 0, 0).Quaternion();
			}
		}
		else if (SourceType == VERTSURFACESOURCE_Surface)
		{
			FVector Verts[3];
			INT VertIndex[3];

			VertIndex[0] = InSkelMeshComponent->SkeletalMesh->LODModels(0).MultiSizeIndexContainer.GetIndexBuffer()->Get( InPrimaryVertexIndex );
			VertIndex[1] = InSkelMeshComponent->SkeletalMesh->LODModels(0).MultiSizeIndexContainer.GetIndexBuffer()->Get( InPrimaryVertexIndex+1 );
			VertIndex[2] = InSkelMeshComponent->SkeletalMesh->LODModels(0).MultiSizeIndexContainer.GetIndexBuffer()->Get( InPrimaryVertexIndex+2 );
			Verts[0] = InSkelMeshComponent->LocalToWorld.TransformFVector(InSkelMeshComponent->GetSkinnedVertexPosition(VertIndex[0]));
			Verts[1] = InSkelMeshComponent->LocalToWorld.TransformFVector(InSkelMeshComponent->GetSkinnedVertexPosition(VertIndex[1]));
			Verts[2] = InSkelMeshComponent->LocalToWorld.TransformFVector(InSkelMeshComponent->GetSkinnedVertexPosition(VertIndex[2]));

			if(bEnforceNormalCheck && bSpawning)
			{

				FVector Direction = (Verts[2]-Verts[0]) ^ (Verts[1]-Verts[0]);
				Direction.Normalize();

				FLOAT Dot = Direction | NormalToCompare;

				if(Dot < ((2.0f*NormalCheckTolerance)-1.0f))
				{
					return FALSE;
				}

				OutPosition = (Verts[0] + Verts[1] + Verts[2]) / 3.0f;
			}
			else
			{
				FVector VertPos;

				OutPosition = (Verts[0] + Verts[1] + Verts[2]) / 3.0f;
			}

			if (OutRotation != NULL)
			{
				*OutRotation = FRotator(0, 0, 0).Quaternion();
			}
		}
	}

	if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == TRUE)
	{
		OutPosition = Owner->Component->LocalToWorld.InverseTransformFVector(OutPosition);
	}

	OutPosition += UniversalOffset;

	return TRUE;
}

/*-----------------------------------------------------------------------------
UParticleModuleLocationStaticVertSurface implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLocationStaticVertSurface);

DECLARE_CYCLE_STAT(TEXT("Particle StaticMeshSurf Time"),STAT_ParticleStaticMeshSurfTime,STATGROUP_Particles);


/**
*	Called after loading the module.
*/
void UParticleModuleLocationStaticVertSurface::PostLoad()
{
	Super::PostLoad();

	if(NormalCheckToleranceDegrees > 180.0f)
	{
		NormalCheckToleranceDegrees = 180.0f;
	}
	else if(NormalCheckToleranceDegrees < 0.0f)
	{
		NormalCheckToleranceDegrees = 0.0f;
	}

	NormalCheckTolerance = ((1.0f-(NormalCheckToleranceDegrees/180.0f))*2.0f)-1.0f;
}

/**
*	Called when a property has change on an instance of the module.
*
*	@param	PropertyChangedEvent		Information on the change that occurred.
*/
void UParticleModuleLocationStaticVertSurface::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(PropertyChangedEvent.Property->GetName() == "NormalCheckToleranceDegrees")
	{
		if(NormalCheckToleranceDegrees > 180.0f)
		{
			NormalCheckToleranceDegrees = 180.0f;
		}
		else if(NormalCheckToleranceDegrees < 0.0f)
		{
			NormalCheckToleranceDegrees = 0.0f;
		}

		NormalCheckTolerance = ((1.0f-(NormalCheckToleranceDegrees/180.0f))*2.0f)-1.0f;
	}
}

/**
*	Called on a particle that is freshly spawned by the emitter.
*	
*	@param	Owner		The FParticleEmitterInstance that spawned the particle.
*	@param	Offset		The modules offset into the data payload of the particle.
*	@param	SpawnTime	The time of the spawn.
*/
void UParticleModuleLocationStaticVertSurface::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleStaticMeshSurfTime);
	FModuleLocationStaticVertSurfaceInstancePayload* InstancePayload = 
		(FModuleLocationStaticVertSurfaceInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (InstancePayload == NULL)
	{
		return;
	}

	if (InstancePayload->SourceComponent == NULL)
	{
		// Setup the source skeletal mesh component...
		InstancePayload->SourceComponent = FParticleModuleUtils::GetStaticMeshComponentSource(Owner, StaticMeshActorParamName);
		if (InstancePayload->SourceComponent == NULL)
		{
			return;
		}
	}

	// Determine the bone/socket to spawn at
	INT SourceIndex = -1;
	if (SourceType == VERTSTATICSURFACESOURCE_Vert)
	{
		INT SourceLocationsCount(InstancePayload->SourceComponent->StaticMesh->LODModels(0).NumVertices);

		SourceIndex = appTrunc(appSRand() * ((FLOAT)SourceLocationsCount) - 1);

		if(SourceIndex == -1)
		{
			return;
		}
	}
	else if(SourceType == VERTSTATICSURFACESOURCE_Surface)
	{
		INT SectionCount = InstancePayload->SourceComponent->StaticMesh->LODModels(0).GetTriangleCount();
		INT RandomSection = appRound(appSRand() * ((FLOAT)SectionCount-1));

		SourceIndex = (appTrunc(appSRand() * ((FLOAT)SectionCount)));

		if(SourceIndex == -1)
		{
			return;
		}
	}

	if (SourceIndex == -1)
	{
		// Failed to select a vert/face?
		return;
	}

	FVector SourceLocation;
	FParticleMeshEmitterInstance* MeshInst = (bOrientMeshEmitters == TRUE) ? 
		CastEmitterInstance<FParticleMeshEmitterInstance>(Owner) : NULL;
	FQuat RotationQuat;
	FQuat* SourceRotation = (MeshInst == NULL) ? NULL : &RotationQuat;
	if (GetParticleLocation(Owner, InstancePayload->SourceComponent, SourceIndex, SourceLocation, SourceRotation, TRUE) == TRUE)
	{
		SPAWN_INIT
		{
			FModuleLocationVertSurfaceParticlePayload* ParticlePayload = (FModuleLocationVertSurfaceParticlePayload*)((BYTE*)&Particle + Offset);
			ParticlePayload->SourceIndex = SourceIndex;
			Particle.Location = SourceLocation;
			if ((MeshInst != NULL) && (MeshInst->MeshRotationActive == TRUE))
			{
				FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
				PayloadData->Rotation = RotationQuat.Euler();
				if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == TRUE)
				{
					PayloadData->Rotation = Owner->Component->LocalToWorld.InverseTransformNormalNoScale(PayloadData->Rotation);
				}
			}
		}
	}
	else
	{
		SPAWN_INIT
		{
			Particle.RelativeTime = 1.1f;
			Owner->KillParticles();
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
void UParticleModuleLocationStaticVertSurface::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleStaticMeshSurfTime);
	if (bUpdatePositionEachFrame == FALSE)
	{
		return;
	}

	FModuleLocationStaticVertSurfaceInstancePayload* InstancePayload = 
		(FModuleLocationStaticVertSurfaceInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (InstancePayload->SourceComponent == NULL)
	{
		//@todo. Should we setup the source skeletal mesh component here too??
		return;
	}

	FVector SourceLocation;
	FParticleMeshEmitterInstance* MeshInst = (bOrientMeshEmitters == TRUE) ? 
		CastEmitterInstance<FParticleMeshEmitterInstance>(Owner) : NULL;
	FQuat RotationQuat;
	FQuat* SourceRotation = (MeshInst == NULL) ? NULL : &RotationQuat;
	BEGIN_UPDATE_LOOP;
	{
		FModuleLocationVertSurfaceParticlePayload* ParticlePayload = (FModuleLocationVertSurfaceParticlePayload*)((BYTE*)&Particle + Offset);
		if (GetParticleLocation(Owner, InstancePayload->SourceComponent, ParticlePayload->SourceIndex, SourceLocation, SourceRotation) == TRUE)
		{
			Particle.Location = SourceLocation;
			if ((MeshInst != NULL) && (MeshInst->MeshRotationActive == TRUE))
			{
				FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
				PayloadData->Rotation = RotationQuat.Euler();
				if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == TRUE)
				{
					PayloadData->Rotation = Owner->Component->LocalToWorld.InverseTransformNormalNoScale(PayloadData->Rotation);
				}
			}
		}
	}
	END_UPDATE_LOOP;
}

/**
 *	Returns the number of bytes that the module requires in the particle payload block.
 *
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return	UINT		The number of bytes the module needs per particle.
 */
UINT	UParticleModuleLocationStaticVertSurface::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return bUpdatePositionEachFrame ? sizeof(FModuleLocationVertSurfaceParticlePayload) : 0;
}

/**
*	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
*	
*	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
*
*	@return UINT		The number of bytes the module needs per emitter instance.
*/
UINT UParticleModuleLocationStaticVertSurface::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return sizeof(FModuleLocationStaticVertSurfaceInstancePayload);
}

/**
*	Helper function used by the editor to auto-populate a placed AEmitter with any
*	instance parameters that are utilized.
*
*	@param	PSysComp		The particle system component to be populated.
*/
void UParticleModuleLocationStaticVertSurface::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	UBOOL bFound = FALSE;
	for (INT ParamIdx = 0; ParamIdx < PSysComp->InstanceParameters.Num(); ParamIdx++)
	{
		FParticleSysParam* Param = &(PSysComp->InstanceParameters(ParamIdx));
		if (Param->Name == StaticMeshActorParamName)
		{
			bFound = TRUE;
			break;
		}
	}

	if (bFound == FALSE)
	{
		INT NewParamIndex = PSysComp->InstanceParameters.AddZeroed();
		PSysComp->InstanceParameters(NewParamIndex).Name = StaticMeshActorParamName;
		PSysComp->InstanceParameters(NewParamIndex).ParamType = PSPT_Actor;
		PSysComp->InstanceParameters(NewParamIndex).Actor = NULL;
	}
}

/**
*	Retrieve the position for the given socket index.
*
*	@param	Owner					The particle emitter instance that is being setup
*	@param	InStaticMeshComponent	The static mesh component to use as the source
*	@param	InPrimaryVertexIndex	The index of the only vertex (vert mode) or the first vertex (surface mode)
*	@param	OutPosition				The position for the particle location
*	@param	OutRotation				Optional orientation for the particle (mesh emitters)
*  @param  bSpawning				When TRUE and when using normal check on surfaces, will return false if the check fails.
*	
*	@return	UBOOL					TRUE if successful, FALSE if not
*/
UBOOL UParticleModuleLocationStaticVertSurface::GetParticleLocation(FParticleEmitterInstance* Owner, 
																    UStaticMeshComponent* InStaticMeshComponent, INT InPrimaryVertexIndex, 
																    FVector& OutPosition, FQuat* OutRotation, UBOOL bSpawning /* = FALSE*/)
{
	check(InStaticMeshComponent);

	if (InStaticMeshComponent->StaticMesh)
	{
		if (SourceType == VERTSTATICSURFACESOURCE_Vert)
		{
			FVector VertPos = InStaticMeshComponent->StaticMesh->LODModels(0).PositionVertexBuffer.VertexPosition(InPrimaryVertexIndex);
			OutPosition = InStaticMeshComponent->LocalToWorld.TransformFVector(VertPos);
			if (OutRotation != NULL)
			{
				*OutRotation = FRotator(0, 0, 0).Quaternion();
			}
		}
		else if (SourceType == VERTSTATICSURFACESOURCE_Surface)
		{
			FVector Verts[3];

			FStaticMeshTriangle* TriangleData = (FStaticMeshTriangle*)InStaticMeshComponent->StaticMesh->LODModels(0).RawTriangles.Lock(LOCK_READ_ONLY);

			Verts[0] = TriangleData[InPrimaryVertexIndex].Vertices[0];
			Verts[1] = TriangleData[InPrimaryVertexIndex].Vertices[1];
			Verts[2] = TriangleData[InPrimaryVertexIndex].Vertices[2];

			if(bEnforceNormalCheck && bSpawning)
			{

				FVector Direction = (Verts[2]-Verts[0]) ^ (Verts[1]-Verts[0]);
				Direction.Normalize();

				FLOAT Dot = Direction | NormalToCompare;

				if(Dot < ((2.0f*NormalCheckTolerance)-1.0f))
				{
					InStaticMeshComponent->StaticMesh->LODModels(0).RawTriangles.Unlock();

					return FALSE;
				}

				OutPosition = (Verts[0] + Verts[1] + Verts[2]) / 3.0f;
			}
			else
			{
				FVector VertPos;

				OutPosition = (Verts[0] + Verts[1] + Verts[2]) / 3.0f;
			}

			if (OutRotation != NULL)
			{
				*OutRotation = FRotator(0, 0, 0).Quaternion();
			}

			OutPosition = InStaticMeshComponent->LocalToWorld.TransformFVector(OutPosition);

			InStaticMeshComponent->StaticMesh->LODModels(0).RawTriangles.Unlock();
		}
	}

	if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == TRUE)
	{
		OutPosition = Owner->Component->LocalToWorld.InverseTransformFVector(OutPosition);
	}

	OutPosition += UniversalOffset;

	return TRUE;
}

/*-----------------------------------------------------------------------------
	UParticleModuleAttractorBoneSocket implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleAttractorBoneSocket);

/**
 *	Called on a particle that is freshly spawned by the emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	SpawnTime	The time of the spawn.
 */
void UParticleModuleAttractorBoneSocket::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FModuleAttractorBoneSocketInstancePayload* InstancePayload = 
		(FModuleAttractorBoneSocketInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (InstancePayload == NULL)
	{
		return;
	}

	if (InstancePayload->SourceComponent == NULL)
	{
		// Setup the source skeletal mesh component...
		InstancePayload->SourceComponent = FParticleModuleUtils::GetSkeletalMeshComponentSource(Owner, SkelMeshActorParamName);
		if (InstancePayload->SourceComponent == NULL)
		{
			return;
		}
	}

	// Determine the bone/socket to spawn at
	INT SourceIndex = -1;
	if (SelectionMethod == BONESOCKETDESTSEL_Sequential)
	{
		// Simply select the next socket
		SourceIndex = InstancePayload->LastSelectedIndex++;
		if (InstancePayload->LastSelectedIndex >= SourceLocations.Num())
		{
			InstancePayload->LastSelectedIndex = 0;
		}
	}
	else if (SelectionMethod == BONESOCKETDESTSEL_Random)
	{
		// Note: This can select the same socket over and over...
		SourceIndex = appTrunc(appSRand() * (SourceLocations.Num() - 1));
		InstancePayload->LastSelectedIndex = SourceIndex;
	}
	else //BONESOCKETDESTSEL_RandomExhaustive
	{
		if (InstancePayload->Indices[InstancePayload->CurrentUnused].Num() == 0)
		{
			InstancePayload->CurrentUnused = (InstancePayload->CurrentUnused == 0) ? 1 : 0;
		}
		BYTE TempIndex = appTrunc(appSRand() * InstancePayload->Indices[InstancePayload->CurrentUnused].Num());

		if(InstancePayload->Indices->Num() != 0)
		{
			SourceIndex = (InstancePayload->Indices[InstancePayload->CurrentUnused])(TempIndex);
			InstancePayload->Indices[(InstancePayload->CurrentUnused == 0) ? 1 : 0].AddItem(SourceIndex);
			InstancePayload->Indices[InstancePayload->CurrentUnused].Remove(TempIndex);
		}
		else
		{
			SourceIndex = -1;
		}
	}

	if (SourceIndex == -1)
	{
		// Failed to select a socket?
		return;
	}
	if (SourceIndex >= SourceLocations.Num())
	{
		return;
	}

	FVector SourceLocation;
	SPAWN_INIT
	{
		FModuleAttractorBoneSocketParticlePayload* ParticlePayload = (FModuleAttractorBoneSocketParticlePayload*)((BYTE*)&Particle + Offset);
		ParticlePayload->SourceIndex = SourceIndex;
		if(bAttractAlongLengthOfBone)
		{
			((FModuleAttractorBoneSocketParticlePayloadWithBoneLerpAlpha*)ParticlePayload)->RandomLerpAlpha = appSRand();
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
void UParticleModuleAttractorBoneSocket::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	FModuleAttractorBoneSocketInstancePayload* InstancePayload = 
		(FModuleAttractorBoneSocketInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (InstancePayload->SourceComponent == NULL)
	{
		// Setup the source skeletal mesh component...
		InstancePayload->SourceComponent = FParticleModuleUtils::GetSkeletalMeshComponentSource(Owner, SkelMeshActorParamName);
		if (InstancePayload->SourceComponent == NULL)
		{
			return;
		}
	}

	FVector SourceLocation;
	INT AttractorCount = SourceLocations.Num();
	INT CurrentAttractor = 0;

	if(AttractorCount == 0)
	{
		return;
	}

	BEGIN_UPDATE_LOOP;
	if(SelectionMethod != BONESOCKETDESTSEL_BlendAll)
	{
		CurrentAttractor = ((FModuleAttractorBoneSocketParticlePayload*)((BYTE*)&Particle + Offset))->SourceIndex;
		AttractorCount = CurrentAttractor + 1;
	}

	for(; CurrentAttractor < AttractorCount ; ++CurrentAttractor)
	{
		FVector VelToAdd, DestinationLocation;
		FLOAT BoneLerpAlpha = 0.0f;
		FLOAT Time = bParticleLifeRelative ? Particle.RelativeTime : Owner->EmitterTime;
		if(bAttractAlongLengthOfBone)
		{
			BoneLerpAlpha = ((FModuleAttractorBoneSocketParticlePayloadWithBoneLerpAlpha*)((BYTE*)&Particle + Offset))->RandomLerpAlpha;
		}
		GetVelocityForAttraction(Owner, InstancePayload->SourceComponent, CurrentAttractor,
								 Particle.Location, Time, BoneLerpAlpha, VelToAdd, DestinationLocation);
		FVector Temp = DestinationLocation - Particle.Location;
		FLOAT Distance = Temp.Size();

		if(Distance <= Range.GetValue(Time))
		{
			if(Distance <= DragRadius.GetValue(Time) && Particle.Size.Size() != 0.0f)
			{
				FVector VelocityForDelta(VelToAdd*DeltaTime);
				VelToAdd -= (DragCoefficient.GetValue(Time) * Particle.Size.Size()) * (VelocityForDelta * VelocityForDelta);
			}
		}

		Particle.Velocity	+= VelToAdd * DeltaTime;
		Particle.BaseVelocity	+= VelToAdd * DeltaTime;
	}
	END_UPDATE_LOOP;
}

/**
 *	Called on an emitter when all other update operations have taken place
 *	INCLUDING bounding box calculations!
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	DeltaTime	The time since the last update.
 */
void UParticleModuleAttractorBoneSocket::FinalUpdate(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	Super::FinalUpdate(Owner, Offset, DeltaTime);

	FModuleAttractorBoneSocketInstancePayload* InstancePayload = 
		(FModuleAttractorBoneSocketInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (InstancePayload->SourceComponent == NULL)
	{
		//@todo. Should we setup the source skeletal mesh component here too??
		return;
	}

	if (SourceLocations.Num() == 0)
	{
		return;
	}

	UBOOL bHaveDeadParticles = FALSE;
	BEGIN_UPDATE_LOOP;
	{
		FModuleAttractorBoneSocketParticlePayload* ParticlePayload = (FModuleAttractorBoneSocketParticlePayload*)((BYTE*)&Particle + Offset);
		if (DestinationType == BONESOCKETSOURCE_Sockets)
		{
			if (InstancePayload->SourceComponent && InstancePayload->SourceComponent->SkeletalMesh && SourceLocations.IsValidIndex(ParticlePayload->SourceIndex))
			{
				USkeletalMeshSocket* Socket = InstancePayload->SourceComponent->SkeletalMesh->FindSocket(SourceLocations(ParticlePayload->SourceIndex).BoneSocketName);
				if (Socket)
				{
					//@todo. Can we make this faster???
					INT BoneIndex = InstancePayload->SourceComponent->MatchRefBone(Socket->BoneName);
					if (BoneIndex != INDEX_NONE)
					{
						if ((InstancePayload->SourceComponent->IsBoneHidden(BoneIndex)) || 
							(InstancePayload->SourceComponent->GetBoneAtom(BoneIndex).GetScale() == 0.0f))
						{
							// Kill it
							Particle.RelativeTime = 1.1f;
							bHaveDeadParticles = TRUE;
						}
					}
				}
			}
		}
	}
	END_UPDATE_LOOP;

	if (bHaveDeadParticles == TRUE)
	{
		Owner->KillParticles();
	}
}

/**
 *	Returns the number of bytes that the module requires in the particle payload block.
 *
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return	UINT		The number of bytes the module needs per particle.
 */
UINT UParticleModuleAttractorBoneSocket::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return bAttractAlongLengthOfBone ? sizeof(FModuleAttractorBoneSocketParticlePayloadWithBoneLerpAlpha) : sizeof(FModuleAttractorBoneSocketParticlePayload);
}

/**
 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return UINT		The number of bytes the module needs per emitter instance.
 */
UINT UParticleModuleAttractorBoneSocket::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return sizeof(FModuleAttractorBoneSocketInstancePayload);
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleAttractorBoneSocket::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	FModuleAttractorBoneSocketInstancePayload* Payload = (FModuleAttractorBoneSocketInstancePayload*)InstData;
	if (Payload)
	{
		appMemzero(Payload, sizeof(FModuleAttractorBoneSocketInstancePayload));
		Payload->Indices[0].Empty(SourceLocations.Num());
		Payload->Indices[1].Empty(SourceLocations.Num());
		for (INT AddIdx = 0; AddIdx < SourceLocations.Num(); AddIdx++)
		{
			Payload->Indices[0].AddItem(AddIdx);
		}
		return 0;
	}

	return 0xffffffff;
}

/**
 *	Helper function used by the editor to auto-populate a placed AEmitter with any
 *	instance parameters that are utilized.
 *
 *	@param	PSysComp		The particle system component to be populated.
 */
void UParticleModuleAttractorBoneSocket::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	UBOOL bFound = FALSE;
	for (INT ParamIdx = 0; ParamIdx < PSysComp->InstanceParameters.Num(); ParamIdx++)
	{
		FParticleSysParam* Param = &(PSysComp->InstanceParameters(ParamIdx));
		if (Param->Name == SkelMeshActorParamName)
		{
			bFound = TRUE;
			break;
		}
	}

	if (bFound == FALSE)
	{
		INT NewParamIndex = PSysComp->InstanceParameters.AddZeroed();
		PSysComp->InstanceParameters(NewParamIndex).Name = SkelMeshActorParamName;
		PSysComp->InstanceParameters(NewParamIndex).ParamType = PSPT_Actor;
		PSysComp->InstanceParameters(NewParamIndex).Actor = NULL;
	}
}

#if WITH_EDITOR
/**
 *	Get the number of custom entries this module has. Maximum of 3.
 *
 *	@return	INT		The number of custom menu entries
 */
INT UParticleModuleAttractorBoneSocket::GetNumberOfCustomMenuOptions() const
{
	return 1;
}

/**
 *	Get the display name of the custom menu entry.
 *
 *	@param	InEntryIndex		The custom entry index (0-2)
 *	@param	OutDisplayString	The string to display for the menu
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleAttractorBoneSocket::GetCustomMenuEntryDisplayString(INT InEntryIndex, FString& OutDisplayString) const
{
	if (InEntryIndex == 0)
	{
		OutDisplayString = LocalizeUnrealEd("Module_LocationBoneSocket_AutoFill");
		return TRUE;
	}
	return FALSE;
}

/**
 *	Perform the custom menu entry option.
 *
 *	@param	InEntryIndex		The custom entry index (0-2) to perform
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleAttractorBoneSocket::PerformCustomMenuEntry(INT InEntryIndex)
{
	if (GIsEditor == TRUE)
	{
		if (InEntryIndex == 0)
		{
			// Fill in the socket names array with the skeletal mesh 
			if (EditorSkelMesh != NULL)
			{
				if (DestinationType == BONESOCKETDEST_Sockets)
				{
					// Retrieve all the sockets
					if (EditorSkelMesh->Sockets.Num() > 0)
					{
						SourceLocations.Empty();
						for (INT SocketIdx = 0; SocketIdx < EditorSkelMesh->Sockets.Num(); SocketIdx++)
						{
							INT NewItemIdx = SourceLocations.AddZeroed();
							FAttractLocationBoneSocketInfo& Info = SourceLocations(NewItemIdx);
							USkeletalMeshSocket* Socket = EditorSkelMesh->Sockets(SocketIdx);
							if (Socket != NULL)
							{
								Info.BoneSocketName = Socket->SocketName;
							}
							else
							{
								Info.BoneSocketName = NAME_None;
							}
						}
						return TRUE;
					}
					else
					{
						appMsgf(AMT_OK, *LocalizeUnrealEd("Module_LocationBoneSocket_EditorMeshNoSockets"));
					}
				}
				else //BONESOCKETDEST_Bones
				{
					// Retrieve all the bones
					if (EditorSkelMesh->RefSkeleton.Num() > 0)
					{
						SourceLocations.Empty();
						for (INT BoneIdx = 0; BoneIdx < EditorSkelMesh->RefSkeleton.Num(); BoneIdx++)
						{
							INT NewItemIdx = SourceLocations.AddZeroed();
							FAttractLocationBoneSocketInfo& Info = SourceLocations(NewItemIdx);
							FMeshBone& Bone = EditorSkelMesh->RefSkeleton(BoneIdx);
							Info.BoneSocketName = Bone.Name;
						}
						return TRUE;
					}
					else
					{
						appMsgf(AMT_OK, *LocalizeUnrealEd("Module_LocationBoneSocket_EditorMeshNoBones"));
					}
				}
			}
			else
			{
				appMsgf(AMT_OK, *LocalizeUnrealEd("Module_LocationBoneSocket_NoEditorMesh"));
			}
		}
	}
	return FALSE;
}
#endif

/**
 *	Retrieve the velocity and position for the given particle/socket combination.
 *
 *	@param	Owner					The particle emitter instance that is being setup
 *	@param	InSkelMeshComponent		The skeletal mesh component to use as the source
 *	@param	InBoneSocketIndex		The index of the bone/socket of interest
 *  @param  InCurrentLocation		The current location of the particle
 *  @param  InCurrentTime			The current time (either particle or emitter depending on module settings)
 *  @param  InBoneLerpAlpha			The alpha value to lerp between the ends of a bone when bAttractAlongLengthOfBone is TRUE
 *	@param	OutVelocityToAdd		The velocity to add for this particle
 *	@param	OutDestinationLocation	Location of the bone or socket attracting the particle
 *	
 *	@return	UBOOL					TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleAttractorBoneSocket::GetVelocityForAttraction(FParticleEmitterInstance* Owner, 
	USkeletalMeshComponent* InSkelMeshComponent, INT InBoneSocketIndex, FVector& InCurrentLocation, FLOAT InCurrentTime, FLOAT InBoneLerpAlpha,
	FVector& OutVelocityToAdd, FVector& OutDestinationLocation)
{
	check(InSkelMeshComponent);

	if(InBoneSocketIndex >= SourceLocations.Num())
	{
		return FALSE;
	}

	FVector Destination = FVector();
	if (DestinationType == BONESOCKETDEST_Sockets)
	{
		if (InSkelMeshComponent->SkeletalMesh)
		{
			USkeletalMeshSocket* Socket = InSkelMeshComponent->SkeletalMesh->FindSocket(SourceLocations(InBoneSocketIndex).BoneSocketName);
			if (Socket)
			{
				FVector SocketOffset = SourceLocations(InBoneSocketIndex).Offset + UniversalOffset;
				FRotator SocketRotator(0,0,0);
				FMatrix SocketMatrix;
 				if (Socket->GetSocketMatrixWithOffset(SocketMatrix, InSkelMeshComponent, SocketOffset, SocketRotator) == FALSE)
 				{
 					return FALSE;
 				}
				Destination = SocketMatrix.GetOrigin();
			}
			else
			{
				return FALSE;
			}
		}
		else
		{
			return FALSE;
		}
	}
	else	//BONESOCKETDEST_Bones
	{
		INT BoneIndex = InSkelMeshComponent->MatchRefBone(SourceLocations(InBoneSocketIndex).BoneSocketName);
		if (BoneIndex != INDEX_NONE)
		{
			FVector SocketOffset = SourceLocations(InBoneSocketIndex).Offset + UniversalOffset;
			FMatrix WorldBoneTM = InSkelMeshComponent->GetBoneMatrix(BoneIndex);
			FTranslationMatrix OffsetMatrix(SocketOffset);
			FMatrix ResultMatrix = OffsetMatrix * WorldBoneTM;
			Destination = ResultMatrix.GetOrigin();
			if(bAttractAlongLengthOfBone)
			{
				BoneIndex = InSkelMeshComponent->MatchRefBone(InSkelMeshComponent->GetParentBone(SourceLocations(InBoneSocketIndex).BoneSocketName));
				if(BoneIndex != INDEX_NONE)
				{
					WorldBoneTM = InSkelMeshComponent->GetBoneMatrix(BoneIndex);
					ResultMatrix = OffsetMatrix * WorldBoneTM;
					Destination = Lerp(Destination, ResultMatrix.GetOrigin(), InBoneLerpAlpha);
				}
			}
		}
		else
		{
			return FALSE;
		}
	}

	FVector VelToAdd(0.0f, 0.0f, 0.0f);
	FVector Temp(Destination - InCurrentLocation);
	FLOAT Distance(Temp.Size());

	if(Distance > Range.GetValue(InCurrentTime))
	{
		OutVelocityToAdd = FVector::ZeroVector;
		OutDestinationLocation = FVector::ZeroVector;
		return FALSE;
	}

	Temp.Normalize();
	switch(FalloffType)
	{
	case BSFOFF_Constant:
		VelToAdd += Temp*Strength.GetValue(InCurrentTime);
		break;
	case BSFOFF_Linear:
		VelToAdd += Temp*(Strength.GetValue(InCurrentTime)*(1.0f - (Distance/Range.GetValue(InCurrentTime))));
		break;
	case BSFOFF_Exponent:
		VelToAdd += Temp*(Strength.GetValue(InCurrentTime)*(1.0f - (Distance/appPow(Range.GetValue(InCurrentTime), Max( (FLOAT) KINDA_SMALL_NUMBER, FalloffExponent.GetValue(InCurrentTime))))));
		break;
	}

	OutVelocityToAdd = VelToAdd;
	OutDestinationLocation = Destination;

	return TRUE;
}

/*-----------------------------------------------------------------------------
	UParticleModuleAttractorSkelVertSurface implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleAttractorSkelVertSurface);

/**
 *	Called after loading the module.
 */
void UParticleModuleAttractorSkelVertSurface::PostLoad()
{
	Super::PostLoad();

	if(NormalCheckToleranceDegrees > 180.0f)
	{
		NormalCheckToleranceDegrees = 180.0f;
	}
	else if(NormalCheckToleranceDegrees < 0.0f)
	{
		NormalCheckToleranceDegrees = 0.0f;
	}

	NormalCheckTolerance = ((1.0f-(NormalCheckToleranceDegrees/180.0f))*2.0f)-1.0f;
}

/**
 *	Called when a property has change on an instance of the module.
 *
 *	@param	PropertyChangedEvent		Information on the change that occurred.
 */
void UParticleModuleAttractorSkelVertSurface::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(PropertyChangedEvent.Property->GetName() == "NormalCheckToleranceDegrees")
	{
		if(NormalCheckToleranceDegrees > 180.0f)
		{
			NormalCheckToleranceDegrees = 180.0f;
		}
		else if(NormalCheckToleranceDegrees < 0.0f)
		{
			NormalCheckToleranceDegrees = 0.0f;
		}

		NormalCheckTolerance = ((1.0f-(NormalCheckToleranceDegrees/180.0f))*2.0f)-1.0f;
	}
}

/**
 *	Called on a particle that is freshly spawned by the emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	SpawnTime	The time of the spawn.
 */
void UParticleModuleAttractorSkelVertSurface::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FModuleAttractorVertSurfaceInstancePayload* InstancePayload = 
		(FModuleAttractorVertSurfaceInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (InstancePayload == NULL)
	{
		return;
	}

	if (InstancePayload->SourceComponent == NULL)
	{
		// Setup the source skeletal mesh component...
		InstancePayload->SourceComponent = FParticleModuleUtils::GetSkeletalMeshComponentSource(Owner, SkelMeshActorParamName);
		if (InstancePayload->SourceComponent == NULL)
		{
			return;
		}
	}

	INT TotalTris = 0;
	if(DestinationType == VERTSURFACESOURCE_Surface)
	{
		for(INT CurrentSection(0); CurrentSection < InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).Sections.Num(); ++CurrentSection)
		{
			TotalTris += InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).Sections(CurrentSection).NumTriangles;
		}
	}

	// Determine the bone/socket to spawn at
	INT SourceIndex = -1;
	INT SourcesChecked = 0;
	UBOOL bValidIndex = FALSE;
	UBOOL bSourcesLeft = TRUE;
	while(!bValidIndex && bSourcesLeft)
	{
		if (DestinationType == VERTSURFACESOURCE_Vert)
		{
			INT SourceLocationsCount(InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).VertexBufferGPUSkin.GetNumVertices());

			if(SourceLocationsCount == 0 || InstancePayload->ValidAssociatedBoneIndices.Num() == 0)
			{
				bSourcesLeft = FALSE;
			}

			SourceIndex = appTrunc(appSRand() * ((FLOAT)SourceLocationsCount) - 1);

			if(SourceIndex != -1)
			{
				if(FParticleModuleUtils::VertInfluencedByActiveBone(Owner, InstancePayload->SourceComponent, SourceIndex, InstancePayload->ValidAssociatedBoneIndices, InstancePayload->ValidAssociatedMaterialIndices))
				{
					bValidIndex = TRUE;
				}
				else
				{
					++SourcesChecked;

					if(SourcesChecked > SourceLocationsCount)
					{
						bSourcesLeft = FALSE;
					}
				}
			}
		}
		else if(DestinationType == VERTSURFACESOURCE_Surface)
		{
			INT SectionCount = InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).Sections.Num();

			if(SectionCount == 0 || InstancePayload->ValidAssociatedBoneIndices.Num() == 0)
			{
				bSourcesLeft = FALSE;
			}

			INT RandomSection = appRound(appSRand() * ((FLOAT)SectionCount-1));

			SourceIndex = InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).Sections(RandomSection).BaseIndex +
				(appTrunc(appSRand() * ((FLOAT)InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).Sections(RandomSection).NumTriangles))*3);

			if(SourceIndex != -1)
			{
				INT VertIndex[3];

				VertIndex[0] = InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).MultiSizeIndexContainer.GetIndexBuffer()->Get( SourceIndex );
				VertIndex[1] = InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).MultiSizeIndexContainer.GetIndexBuffer()->Get( SourceIndex+1 );
				VertIndex[2] = InstancePayload->SourceComponent->SkeletalMesh->LODModels(0).MultiSizeIndexContainer.GetIndexBuffer()->Get( SourceIndex+2 );

				if(FParticleModuleUtils::VertInfluencedByActiveBone(Owner, InstancePayload->SourceComponent, VertIndex[0], InstancePayload->ValidAssociatedBoneIndices, InstancePayload->ValidAssociatedMaterialIndices) &&
					FParticleModuleUtils::VertInfluencedByActiveBone(Owner, InstancePayload->SourceComponent, VertIndex[1], InstancePayload->ValidAssociatedBoneIndices, InstancePayload->ValidAssociatedMaterialIndices) && 
					FParticleModuleUtils::VertInfluencedByActiveBone(Owner, InstancePayload->SourceComponent, VertIndex[2], InstancePayload->ValidAssociatedBoneIndices, InstancePayload->ValidAssociatedMaterialIndices))
				{
					bValidIndex = TRUE;
				}
				else
				{
					++SourcesChecked;

					if(SourcesChecked > TotalTris)
					{
						bSourcesLeft = FALSE;
					}
				}
			}
		}
	}

	if (SourceIndex == -1)
	{
		// Failed to select a vert/face?
		SPAWN_INIT
		{
			Particle.RelativeTime = 1.1f;
			Owner->KillParticles();
		}
		return;
	}

	SPAWN_INIT
	{
		FModuleAttractorVertSurfaceParticlePayload* ParticlePayload = (FModuleAttractorVertSurfaceParticlePayload*)((BYTE*)&Particle + Offset);
		ParticlePayload->SourceIndex = SourceIndex;
	}
}

/**
 *	Called on a particle that is being updated by its emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	DeltaTime	The time since the last update.
 */
void UParticleModuleAttractorSkelVertSurface::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	FModuleAttractorVertSurfaceInstancePayload* InstancePayload = 
		(FModuleAttractorVertSurfaceInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (InstancePayload->SourceComponent == NULL)
	{
		//@todo. Should we setup the source skeletal mesh component here too??
		return;
	}

	if(ValidAssociatedBones.Num() == 0)
	{
		return;
	}

	FVector SourceLocation;
	BEGIN_UPDATE_LOOP;
	{
		FModuleAttractorVertSurfaceParticlePayload* ParticlePayload = (FModuleAttractorVertSurfaceParticlePayload*)((BYTE*)&Particle + Offset);
		FVector VelToAdd, DestinationLocation;
		FLOAT Time = bParticleLifeRelative ? Particle.RelativeTime : Owner->EmitterTime;
		GetVelocityForAttraction(Owner, InstancePayload->SourceComponent, ParticlePayload->SourceIndex,
			Particle.Location, Time, VelToAdd, DestinationLocation);
		FVector Temp = DestinationLocation - Particle.Location;
		FLOAT Distance = Temp.Size();

		if(Distance <= Range.GetValue(Time))
		{
			if(Distance <= DragRadius.GetValue(Time) && Particle.Size.Size() != 0.0f)
			{
				FVector VelocityForDelta(VelToAdd*DeltaTime);
				VelToAdd -= (DragCoefficient.GetValue(Time) * Particle.Size.Size()) * (VelocityForDelta * VelocityForDelta);
			}
		}

		Particle.Velocity	+= VelToAdd * DeltaTime;
		Particle.BaseVelocity	+= VelToAdd * DeltaTime;
	}
	END_UPDATE_LOOP;
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleAttractorSkelVertSurface::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	FModuleAttractorVertSurfaceInstancePayload* InstancePayload = 
		(FModuleAttractorVertSurfaceInstancePayload*)(Owner->GetModuleInstanceData(this));

	FParticleModuleUtils::UpdateBoneIndicesList(Owner, ValidAssociatedBones, SkelMeshActorParamName, InstancePayload->ValidAssociatedBoneIndices);

	InstancePayload->ValidAssociatedMaterialIndices = ValidMaterialIndices;

	return Super::PrepPerInstanceBlock(Owner, InstData);
}

/**
 *	Returns the number of bytes that the module requires in the particle payload block.
 *
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return	UINT		The number of bytes the module needs per particle.
 */
UINT UParticleModuleAttractorSkelVertSurface::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FModuleAttractorVertSurfaceParticlePayload);
}

/**
 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return UINT		The number of bytes the module needs per emitter instance.
 */
UINT UParticleModuleAttractorSkelVertSurface::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return sizeof(FModuleAttractorVertSurfaceInstancePayload);
}

/**
 *	Helper function used by the editor to auto-populate a placed AEmitter with any
 *	instance parameters that are utilized.
 *
 *	@param	PSysComp		The particle system component to be populated.
 */
void UParticleModuleAttractorSkelVertSurface::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	UBOOL bFound = FALSE;
	for (INT ParamIdx = 0; ParamIdx < PSysComp->InstanceParameters.Num(); ParamIdx++)
	{
		FParticleSysParam* Param = &(PSysComp->InstanceParameters(ParamIdx));
		if (Param->Name == SkelMeshActorParamName)
		{
			bFound = TRUE;
			break;
		}
	}

	if (bFound == FALSE)
	{
		INT NewParamIndex = PSysComp->InstanceParameters.AddZeroed();
		PSysComp->InstanceParameters(NewParamIndex).Name = SkelMeshActorParamName;
		PSysComp->InstanceParameters(NewParamIndex).ParamType = PSPT_Actor;
		PSysComp->InstanceParameters(NewParamIndex).Actor = NULL;
	}
}

#if WITH_EDITOR
/**
 *	Get the number of custom entries this module has. Maximum of 3.
 *
 *	@return	INT		The number of custom menu entries
 */
INT UParticleModuleAttractorSkelVertSurface::GetNumberOfCustomMenuOptions() const
{
	return 1;
}

/**
 *	Get the display name of the custom menu entry.
 *
 *	@param	InEntryIndex		The custom entry index (0-2)
 *	@param	OutDisplayString	The string to display for the menu
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleAttractorSkelVertSurface::GetCustomMenuEntryDisplayString(INT InEntryIndex, FString& OutDisplayString) const
{
	if (InEntryIndex == 0)
	{
		OutDisplayString = LocalizeUnrealEd("Module_LocationVertSurface_AutoFill");
		return TRUE;
	}
	return FALSE;
}

/**
 *	Perform the custom menu entry option.
 *
 *	@param	InEntryIndex		The custom entry index (0-2) to perform
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleAttractorSkelVertSurface::PerformCustomMenuEntry(INT InEntryIndex)
{
	if (GIsEditor == TRUE)
	{
		if (InEntryIndex == 0)
		{
			// Fill in the socket names array with the skeletal mesh 
			if (EditorSkelMesh != NULL)
			{
				// Retrieve all the bones
				if (EditorSkelMesh->RefSkeleton.Num() > 0)
				{
					ValidAssociatedBones.Empty();
					for (INT BoneIdx = 0; BoneIdx < EditorSkelMesh->RefSkeleton.Num(); BoneIdx++)
					{
						INT NewItemIdx = ValidAssociatedBones.AddZeroed();
						FMeshBone& Bone = EditorSkelMesh->RefSkeleton(BoneIdx);
						ValidAssociatedBones(NewItemIdx) = Bone.Name;
					}
				}
				else
				{
					appMsgf(AMT_OK, *LocalizeUnrealEd("Module_LocationBoneSocket_EditorMeshNoBones"));
				}
			}
			else
			{
				appMsgf(AMT_OK, *LocalizeUnrealEd("Module_LocationBoneSocket_NoEditorMesh"));
			}
		}
	}
	return FALSE;
}
#endif

/**
 *	Retrieve the velocity and position for the given particle/socket combination.
 *
 *	@param	Owner					The particle emitter instance that is being setup
 *	@param	InSkelMeshComponent		The skeletal mesh component to use as the source
 *	@param	InPrimaryVertexIndex	The index of the primary vertice
 *  @param  InCurrentLocation		The current location of the particle
 *  @param  InCurrentTime			The current time (either particle or emitter depending on module settings)
 *	@param	OutVelocityToAdd		The velocity to add for this particle
 *	@param	OutDestinationLocation	Location of the bone or socket attracting the particle
 *	
 *	@return	UBOOL					TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleAttractorSkelVertSurface::GetVelocityForAttraction(FParticleEmitterInstance* Owner, 
	USkeletalMeshComponent* InSkelMeshComponent, INT InPrimaryVertexIndex, FVector& InCurrentLocation, FLOAT InCurrentTime,
	FVector& OutVelocityToAdd, FVector& OutDestinationLocation)
{
	check(InSkelMeshComponent);

	FVector Destination;
	if (InSkelMeshComponent->SkeletalMesh)
	{
		if (DestinationType == VERTSURFACEDEST_Vert)
		{
			FVector VertPos = InSkelMeshComponent->GetSkinnedVertexPosition(InPrimaryVertexIndex);
			Destination = InSkelMeshComponent->LocalToWorld.TransformFVector(VertPos);
		}
		else if (DestinationType == VERTSURFACEDEST_Surface)
		{
			FVector Verts[3];
			INT VertIndex[3];

			VertIndex[0] = InSkelMeshComponent->SkeletalMesh->LODModels(0).MultiSizeIndexContainer.GetIndexBuffer()->Get( InPrimaryVertexIndex );
			VertIndex[1] = InSkelMeshComponent->SkeletalMesh->LODModels(0).MultiSizeIndexContainer.GetIndexBuffer()->Get( InPrimaryVertexIndex+1 );
			VertIndex[2] = InSkelMeshComponent->SkeletalMesh->LODModels(0).MultiSizeIndexContainer.GetIndexBuffer()->Get( InPrimaryVertexIndex+2 );
			Verts[0] = InSkelMeshComponent->LocalToWorld.TransformFVector(InSkelMeshComponent->GetSkinnedVertexPosition(VertIndex[0]));
			Verts[1] = InSkelMeshComponent->LocalToWorld.TransformFVector(InSkelMeshComponent->GetSkinnedVertexPosition(VertIndex[1]));
			Verts[2] = InSkelMeshComponent->LocalToWorld.TransformFVector(InSkelMeshComponent->GetSkinnedVertexPosition(VertIndex[2]));

			Destination = (Verts[0] + Verts[1] + Verts[2]) / 3.0f;
		}
	}

	if (Owner->CurrentLODLevel->RequiredModule->bUseLocalSpace == TRUE)
	{
		Destination = Owner->Component->LocalToWorld.InverseTransformFVector(Destination);
	}

	Destination += UniversalOffset;

	FVector VelToAdd(0.0f, 0.0f, 0.0f);
	FVector Temp(Destination - InCurrentLocation);
	FLOAT Distance(Temp.Size());

	if(Distance > Range.GetValue(InCurrentTime))
	{
		OutVelocityToAdd = FVector::ZeroVector;
		OutDestinationLocation = FVector::ZeroVector;
		return FALSE;
	}

	Temp.Normalize();
	switch(FalloffType)
	{
	case VSFOFF_Constant:
		VelToAdd += Temp*Strength.GetValue(InCurrentTime);
		break;
	case VSFOFF_Linear:
		VelToAdd += Temp*(Strength.GetValue(InCurrentTime)*(1.0f - (Distance/Range.GetValue(InCurrentTime))));
		break;
	case VSFOFF_Exponent:
		VelToAdd += Temp*(Strength.GetValue(InCurrentTime)*(1.0f - (Distance/appPow(Range.GetValue(InCurrentTime), Max( (FLOAT) KINDA_SMALL_NUMBER, FalloffExponent.GetValue(InCurrentTime))))));
		break;
	}

	OutVelocityToAdd = VelToAdd;
	OutDestinationLocation = Destination;

	return TRUE;
}

/*-----------------------------------------------------------------------------
	FParticleModuleUtils implementation.
-----------------------------------------------------------------------------*/

/**
 *	Updates the indices list with the bone index for each named bone in the editor exposed values.
 *	
 *	@param	Owner						The FParticleEmitterInstance that 'owns' the particle.
 *  @param	InValidNames				An array of valid names to update the index list with.
 *  @param	InSkelMeshActorParamName	The name of the emitter instance parameter that specifies the skel mesh actor.
 *  @param	InOutValidIndices			An array of valid indices to update.
 */
void FParticleModuleUtils::UpdateBoneIndicesList(FParticleEmitterInstance* Owner, TArrayNoInit<FName>& InValidNames, FName SkelMeshActorParamName, TArray<INT>& InOutValidIndices)
{
	InOutValidIndices.Empty(InValidNames.Num());

	AActor* ActorInst = NULL;

	if (Owner->Component->GetActorParameter(SkelMeshActorParamName, ActorInst) && (ActorInst != NULL))
	{
		ASkeletalMeshActor* SkeletalMeshActor = Cast<ASkeletalMeshActor>(ActorInst);
		APawn* Pawn = Cast<APawn>(ActorInst);

		if ((SkeletalMeshActor != NULL) && (SkeletalMeshActor->SkeletalMeshComponent != NULL) && (SkeletalMeshActor->SkeletalMeshComponent->SkeletalMesh != NULL))
		{
			for (INT FindBoneIdx = 0; FindBoneIdx < InValidNames.Num(); FindBoneIdx++)
			{
				const INT BoneIdx = SkeletalMeshActor->SkeletalMeshComponent->SkeletalMesh->MatchRefBone(InValidNames(FindBoneIdx));
				if (BoneIdx != INDEX_NONE)
				{
					InOutValidIndices.AddItem(BoneIdx);
				}
			}
		}
		// if we have a pawn
		else if( Pawn != NULL )
		{
			// look over all of the components looking for a SkelMeshComp and then if we find one we look at it to see if the bones match
			for( INT CompIdx = 0; CompIdx < Pawn->AllComponents.Num(); ++CompIdx )
			{
				USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Pawn->AllComponents( CompIdx ));

				if( ( SkelComp != NULL ) && ( SkelComp->SkeletalMesh != NULL ) )
				{
					for (INT FindBoneIdx = 0; FindBoneIdx < InValidNames.Num(); FindBoneIdx++)
					{
						const INT BoneIdx = SkelComp->SkeletalMesh->MatchRefBone(InValidNames(FindBoneIdx));
						if (BoneIdx != INDEX_NONE)
						{
							InOutValidIndices.AddItem(BoneIdx);
						}
					}
				}
			}
		}
	}
}

/**
 *  Check to see if the vert is influenced by a bone on our approved list.
 *
 *	@param	Owner					The particle emitter instance that is being setup
 *	@param	InSkelMeshComponent		The skeletal mesh component to use as the source
 *  @param  InVertexIndex			The vertex index of the vert to check.
 *  @param	InValidBoneIndices		An array of valid bone indices to check against.
 *	@param	InValidMatIndices		An array of valid material indices to check against.
 *
 *  @return UBOOL					TRUE if it is influenced by an approved bone, FALSE otherwise.
 */
UBOOL FParticleModuleUtils::VertInfluencedByActiveBone(FParticleEmitterInstance* Owner, USkeletalMeshComponent* InSkelMeshComponent, INT InVertexIndex, TArray<INT>& InValidBoneIndices, TArray<INT>& InValidMatIndices)
{
	FStaticLODModel& Model = InSkelMeshComponent->SkeletalMesh->LODModels(0);

	const USkeletalMeshComponent* BaseComponent = InSkelMeshComponent->ParentAnimComponent ?
		InSkelMeshComponent->ParentAnimComponent : InSkelMeshComponent;

	// Find the chunk and vertex within that chunk, and skinning type, for this vertex.
	INT ChunkIndex;
	INT VertIndex;
	UBOOL bSoftVertex;
	Model.GetChunkAndSkinType(InVertexIndex, ChunkIndex, VertIndex, bSoftVertex);

	check(ChunkIndex < Model.Chunks.Num());

	if (InValidMatIndices.Num() > 0)
	{
		for (INT SectIdx = 0; SectIdx < Model.Sections.Num(); SectIdx++)
		{
			FSkelMeshSection& Section = Model.Sections(SectIdx);
			if (Section.ChunkIndex == ChunkIndex)
			{
				// Does the material match one of the valid ones
				UBOOL bFound = FALSE;
				for (INT ValidIdx = 0; ValidIdx < InValidMatIndices.Num(); ValidIdx++)
				{
					if (InValidMatIndices(ValidIdx) == Section.MaterialIndex)
					{
						bFound = TRUE;
						break;
					}
				}

				if (!bFound)
				{
					// Material wasn't in the valid list...
					return FALSE;
				}
			}
		}
	}

	const FSkelMeshChunk& Chunk = Model.Chunks(ChunkIndex);
	const INT RigidInfluenceIndex = SkinningTools::GetRigidInfluenceIndex();

	// Do soft skinning for this vertex.
	if(bSoftVertex)
	{
		const FGPUSkinVertexBase* SrcSoftVertex = Model.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetSoftVertexBufferIndex()+VertIndex);

#if !__INTEL_BYTE_ORDER__
		// BYTE[] elements in LOD.VertexBufferGPUSkin have been swapped for VET_UBYTE4 vertex stream use
		for(INT InfluenceIndex = Chunk.MaxBoneInfluences-1;InfluenceIndex >= 0;InfluenceIndex--)
#else
		for(INT InfluenceIndex = 0;InfluenceIndex < Chunk.MaxBoneInfluences;InfluenceIndex++)
#endif
		{
			BYTE BoneIndex = Chunk.BoneMap(SrcSoftVertex->InfluenceBones[InfluenceIndex]);
			if(InSkelMeshComponent->ParentAnimComponent)
			{		
				check(InSkelMeshComponent->ParentBoneMap.Num() == InSkelMeshComponent->SkeletalMesh->RefSkeleton.Num());
				BoneIndex = InSkelMeshComponent->ParentBoneMap(BoneIndex);
			}
			
			if(InValidBoneIndices.ContainsItem(BoneIndex))
			{
				return TRUE;
			}
		}
	}
	// Do rigid (one-influence) skinning for this vertex.
	else
	{
		const FGPUSkinVertexBase* SrcRigidVertex = Model.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetRigidVertexBufferIndex()+VertIndex);

		BYTE BoneIndex = Chunk.BoneMap(SrcRigidVertex->InfluenceBones[RigidInfluenceIndex]);
		if(InSkelMeshComponent->ParentAnimComponent)
		{
			check(InSkelMeshComponent->ParentBoneMap.Num() == InSkelMeshComponent->SkeletalMesh->RefSkeleton.Num());
			BoneIndex = InSkelMeshComponent->ParentBoneMap(BoneIndex);
		}

		if(InValidBoneIndices.ContainsItem(BoneIndex))
		{
			return TRUE;
		}
	}

	return FALSE;
}
