/*=============================================================================
	ParticleModules_Size.cpp: 
	Size-related particle module implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UParticleModuleSizeBase);

/*-----------------------------------------------------------------------------
	UParticleModuleSize implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSize);

void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
void UParticleModuleSize::SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream)
{
	SPAWN_INIT;
	FVector Size		 = StartSize.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);
	Particle.Size		+= Size;
	Particle.BaseSize	+= Size;
}

/*-----------------------------------------------------------------------------
	UParticleModuleSize_Seeded implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSize_Seeded);

/**
 *	Called on a particle when it is spawned.
 *
 *	@param	Owner			The emitter instance that spawned the particle
 *	@param	Offset			The payload data offset for this module
 *	@param	SpawnTime		The spawn time of the particle
 */
void UParticleModuleSize_Seeded::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
UINT UParticleModuleSize_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleSize_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

/** 
 *	Called when an emitter instance is looping...
 *
 *	@param	Owner	The emitter instance that owns this module
 */
void UParticleModuleSize_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == TRUE)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleSizeMultiplyVelocity implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSizeMultiplyVelocity);

void UParticleModuleSizeMultiplyVelocity::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;
	FVector SizeScale = VelocityMultiplier.GetValue(Particle.RelativeTime, Owner->Component) * Particle.Velocity.Size();
	
	if(MultiplyX)
	{
		Particle.Size.X = ScaleSize( Particle.Size.X, SizeScale.X, CapMinSize.X, CapMaxSize.X );	
	}
	if(MultiplyY)
	{
		Particle.Size.Y = ScaleSize( Particle.Size.Y, SizeScale.Y, CapMinSize.Y, CapMaxSize.Y );	
	}
	if(MultiplyZ)
	{
		Particle.Size.Z = ScaleSize( Particle.Size.Z, SizeScale.Z, CapMinSize.Z, CapMaxSize.Z );	
	}
}

void UParticleModuleSizeMultiplyVelocity::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	BEGIN_UPDATE_LOOP;
	{
		FVector SizeScale = VelocityMultiplier.GetValue(Particle.RelativeTime, Owner->Component) * Particle.Velocity.Size();
		
		if(MultiplyX)
		{
			Particle.Size.X = ScaleSize( Particle.Size.X, SizeScale.X, CapMinSize.X, CapMaxSize.X );	
		}
		if(MultiplyY)
		{
			Particle.Size.Y = ScaleSize( Particle.Size.Y, SizeScale.Y, CapMinSize.Y, CapMaxSize.Y );	
		}
		if(MultiplyZ)
		{
			Particle.Size.Z = ScaleSize( Particle.Size.Z, SizeScale.Z, CapMinSize.Z, CapMaxSize.Z );	
		}
	}
	END_UPDATE_LOOP;
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleSizeMultiplyVelocity::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	UDistributionVectorConstant* VelocityMultiplierDist = Cast<UDistributionVectorConstant>(VelocityMultiplier.Distribution);
	if (VelocityMultiplierDist)
	{
		VelocityMultiplierDist->Constant = FVector(1.0f,1.0f,1.0f);
		VelocityMultiplierDist->bIsDirty = TRUE;
	}
}

/**
 *	Called when a property has change on an instance of the module.
 *
 *	@param	PropertyChangedEvent		Information on the change that occurred.
 */
void UParticleModuleSizeMultiplyVelocity::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//ensure we never set out size caps below 0
	if (CapMaxSize.X < 0)
	{
		CapMaxSize.X = 0;	
	}
	if (CapMaxSize.Y < 0)
	{
		CapMaxSize.Y = 0;	
	}		
	if (CapMaxSize.Z < 0)
	{
		CapMaxSize.Z = 0;	
	}
	if (CapMinSize.X < 0)
	{
		CapMinSize.X = 0;	
	}
	if (CapMinSize.Y < 0)
	{
		CapMinSize.Y = 0;	
	}			
	if (CapMinSize.Z < 0)
	{
		CapMinSize.Z = 0;	
	}											
}

FLOAT UParticleModuleSizeMultiplyVelocity::ScaleSize(FLOAT Size, FLOAT Scale, FLOAT Min, FLOAT Max)
{	
	FLOAT NewSize = Size * Scale;

	if (Min > 0 && NewSize < Min)
	{
		NewSize = Min;	
	}
	if (Max > 0 && NewSize > Max)
	{
		NewSize = Max;
	} 
	return NewSize;
}

/*-----------------------------------------------------------------------------
	UParticleModuleSizeMultiplyLife implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSizeMultiplyLife);

void UParticleModuleSizeMultiplyLife::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;
	FVector SizeScale = LifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
	if(MultiplyX)
	{
		Particle.Size.X *= SizeScale.X;
	}
	if(MultiplyY)
	{
		Particle.Size.Y *= SizeScale.Y;
	}
	if(MultiplyZ)
	{
		Particle.Size.Z *= SizeScale.Z;
	}
}

void UParticleModuleSizeMultiplyLife::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	if ((Owner == NULL) || (Owner->ActiveParticles <= 0) || 
		(Owner->ParticleData == NULL) || (Owner->ParticleIndices == NULL))
	{
		return;
	}
	const FRawDistribution* FastDistribution = LifeMultiplier.GetFastRawDistribution();
	CONSOLE_PREFETCH(Owner->ParticleData + (Owner->ParticleIndices[0] * Owner->ParticleStride));
	CONSOLE_PREFETCH_NEXT_CACHE_LINE(Owner->ParticleData + (Owner->ParticleIndices[0] * Owner->ParticleStride));
	if (MultiplyX && MultiplyY && MultiplyZ)
	{
		if (FastDistribution)
		{
			FVector SizeScale;
			// fast path
			BEGIN_UPDATE_LOOP;
				FastDistribution->GetValue3None(Particle.RelativeTime, &SizeScale.X);
				CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
				CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
				Particle.Size.X *= SizeScale.X;
				Particle.Size.Y *= SizeScale.Y;
				Particle.Size.Z *= SizeScale.Z;
			END_UPDATE_LOOP;
		}
		else
		{
			BEGIN_UPDATE_LOOP
			{
				FVector SizeScale = LifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
				CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
				CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
				Particle.Size.X *= SizeScale.X;
				Particle.Size.Y *= SizeScale.Y;
				Particle.Size.Z *= SizeScale.Z;
			}
			END_UPDATE_LOOP;
		}
	}
	else
	{
		if (
			( MultiplyX && !MultiplyY && !MultiplyZ) ||
			(!MultiplyX &&  MultiplyY && !MultiplyZ) ||
			(!MultiplyX && !MultiplyY &&  MultiplyZ)
			)
		{
			INT Index = MultiplyX ? 0 : (MultiplyY ? 1 : 2);
			BEGIN_UPDATE_LOOP
			{
				FVector SizeScale = LifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
				CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
				CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
				Particle.Size[Index] *= SizeScale[Index];
			}
			END_UPDATE_LOOP;
		}
		else
		{
			BEGIN_UPDATE_LOOP
			{
				FVector SizeScale = LifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
				CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
				CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
				if(MultiplyX)
				{
					Particle.Size.X *= SizeScale.X;
				}
				if(MultiplyY)
				{
					Particle.Size.Y *= SizeScale.Y;
				}
				if(MultiplyZ)
				{
					Particle.Size.Z *= SizeScale.Z;
				}
			}
			END_UPDATE_LOOP;
		}
	}
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleSizeMultiplyLife::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	LifeMultiplier.Distribution = Cast<UDistributionVectorConstantCurve>(StaticConstructObject(UDistributionVectorConstantCurve::StaticClass(), this));
	UDistributionVectorConstantCurve* LifeMultiplierDist = Cast<UDistributionVectorConstantCurve>(LifeMultiplier.Distribution);
	if (LifeMultiplierDist)
	{
		// Add two points, one at time 0.0f and one at 1.0f
		for (INT Key = 0; Key < 2; Key++)
		{
			INT	KeyIndex = LifeMultiplierDist->CreateNewKey(Key * 1.0f);
			for (INT SubIndex = 0; SubIndex < 3; SubIndex++)
			{
				LifeMultiplierDist->SetKeyOut(SubIndex, KeyIndex, 1.0f);
			}
		}
		LifeMultiplierDist->bIsDirty = TRUE;
	}
}


/*-----------------------------------------------------------------------------
	UParticleModuleSizeScale implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSizeScale);

void UParticleModuleSizeScale::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	BEGIN_UPDATE_LOOP;
		FVector ScaleFactor = SizeScale.GetValue(Particle.RelativeTime, Owner->Component);
		Particle.Size = Particle.BaseSize * ScaleFactor;
	END_UPDATE_LOOP;
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleSizeScale::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	UDistributionVectorConstant* SizeScaleDist = Cast<UDistributionVectorConstant>(SizeScale.Distribution);
	if (SizeScaleDist)
	{
		SizeScaleDist->Constant = FVector(1.0f,1.0f,1.0f);
		SizeScaleDist->bIsDirty = TRUE;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleSizeScaleByTime implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSizeScaleByTime);

/**
 *	Called during the spawning of a particle.
 *	
 *	@param	Owner		The emitter instance that owns the particle.
 *	@param	Offset		The offset into the particle payload for this module.
 *	@param	SpawnTime	The spawn time of the particle.
 */
void UParticleModuleSizeScaleByTime::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;
	{
		PARTICLE_ELEMENT(FScaleSizeByLifePayload, Payload);
		Payload.AbsoluteTime = SpawnTime;
	}
}

/**
 *	Called during the spawning of particles in the emitter instance.
 *	
 *	@param	Owner		The emitter instance that owns the particle.
 *	@param	Offset		The offset into the particle payload for this module.
 *	@param	DeltaTime	The time slice for this update.
 */
void UParticleModuleSizeScaleByTime::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	BEGIN_UPDATE_LOOP;
	{
		PARTICLE_ELEMENT(FScaleSizeByLifePayload, Payload);
		Payload.AbsoluteTime += DeltaTime;
		FVector ScaleFactor = SizeScaleByTime.GetValue(Payload.AbsoluteTime, Owner->Component);
		Particle.Size.X = Particle.Size.X * (bEnableX ? ScaleFactor.X : 1.0f);
		Particle.Size.Y = Particle.Size.Y * (bEnableY ? ScaleFactor.Y : 1.0f);
		Particle.Size.Z = Particle.Size.Z * (bEnableZ ? ScaleFactor.Z : 1.0f);
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
UINT UParticleModuleSizeScaleByTime::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FScaleSizeByLifePayload);
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleSizeScaleByTime::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	UDistributionVectorConstantCurve* SizeScaleByTimeDist = Cast<UDistributionVectorConstantCurve>(SizeScaleByTime.Distribution);
	if (SizeScaleByTimeDist)
	{
		// Add two points, one at time 0.0f and one at 1.0f
		for (INT Key = 0; Key < 2; Key++)
		{
			INT	KeyIndex = SizeScaleByTimeDist->CreateNewKey(Key * 1.0f);
			for (INT SubIndex = 0; SubIndex < 3; SubIndex++)
			{
				SizeScaleByTimeDist->SetKeyOut(SubIndex, KeyIndex, 1.0f);
			}
		}
		SizeScaleByTimeDist->bIsDirty = TRUE;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleSizeScaleOverDensity implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSizeScaleOverDensity);
/**
 *	Called during the spawning of a particle.
 *	
 *	@param	Owner		The emitter instance that owns the particle.
 *	@param	Offset		The offset into the particle payload for this module.
 *	@param	SpawnTime	The spawn time of the particle.
 */
void UParticleModuleSizeScaleOverDensity::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	FParticleSpritePhysXEmitterInstance* EmitInst = CastEmitterInstance<FParticleSpritePhysXEmitterInstance>(Owner);
	if (EmitInst != NULL)
	{
		SPAWN_INIT;
		{
			FLOAT Density = *((FLOAT*)(ParticleBase + EmitInst->DensityPayloadOffset));
			FVector SizeVec	= SizeScaleOverDensity.GetValue(Density, Owner->Component);
			Particle.Size.X *= SizeVec.X;
			Particle.Size.Y *= SizeVec.Y;
			Particle.Size.Z *= SizeVec.Z;
		}
	}
#endif
}


void UParticleModuleSizeScaleOverDensity::Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime )
{
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	FParticleSpritePhysXEmitterInstance* EmitInst = CastEmitterInstance<FParticleSpritePhysXEmitterInstance>(Owner);
	if (EmitInst != NULL)
	{
		const FRawDistribution* FastSizeScaleOverDensity = SizeScaleOverDensity.GetFastRawDistribution();
		FVector SizeVec;
		FLOAT Density;
		if( FastSizeScaleOverDensity )
		{
			BEGIN_UPDATE_LOOP;
			{
				Density = *((FLOAT*)(ParticleBase + EmitInst->DensityPayloadOffset));
				FastSizeScaleOverDensity->GetValue3None(Density, &SizeVec.X);
				Particle.Size.X *= SizeVec.X;
				Particle.Size.Y *= SizeVec.Y;
				Particle.Size.Z *= SizeVec.Z;
			}
			END_UPDATE_LOOP;
		}
		else
		{
			BEGIN_UPDATE_LOOP;
			{
				Density = *((FLOAT*)(ParticleBase + EmitInst->DensityPayloadOffset));
				SizeVec = SizeScaleOverDensity.GetValue(Density, Owner->Component);
				Particle.Size.X *= SizeVec.X;
				Particle.Size.Y *= SizeVec.Y;
				Particle.Size.Z *= SizeVec.Z;
			}
			END_UPDATE_LOOP;
		}
	}
#endif
}
/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleSizeScaleOverDensity::SetToSensibleDefaults( UParticleEmitter* Owner )
{
	SizeScaleOverDensity.Distribution = Cast<UDistributionVectorConstantCurve>(StaticConstructObject(UDistributionVectorConstantCurve::StaticClass(), this));
	UDistributionVectorConstantCurve* SizeScaleOverDensityDist = Cast<UDistributionVectorConstantCurve>(SizeScaleOverDensity.Distribution);
	if (SizeScaleOverDensityDist)
	{
		// Add two points, one at time 0.0f and one at 1.0f
		for (INT Key = 0; Key < 2; Key++)
		{
			INT	KeyIndex = SizeScaleOverDensityDist->CreateNewKey(Key * 1.0f);
			for (INT SubIndex = 0; SubIndex < 3; SubIndex++)
			{
				SizeScaleOverDensityDist->SetKeyOut(SubIndex, KeyIndex, 1.0f);
			}
		}
		SizeScaleOverDensityDist->bIsDirty = TRUE;
	}
}
