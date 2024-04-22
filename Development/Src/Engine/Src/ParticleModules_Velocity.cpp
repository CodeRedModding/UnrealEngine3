/*=============================================================================
	ParticleModules_Velocity.cpp: 
	Velocity-related particle module implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleVelocityBase);

/*-----------------------------------------------------------------------------
	UParticleModuleVelocity implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleVelocity);

void UParticleModuleVelocity::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
void UParticleModuleVelocity::SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream)
{
	SPAWN_INIT;
	{
		FVector FromOrigin;
		FVector Vel = StartVelocity.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);

		FVector OwnerScale(1.0f);
		if ((bApplyOwnerScale == TRUE) && Owner && Owner->Component)
		{
			OwnerScale = Owner->Component->Scale * Owner->Component->Scale3D;
			AActor* Actor = Owner->Component->GetOwner();
			if (Actor && !Owner->Component->AbsoluteScale)
			{
				OwnerScale *= Actor->DrawScale * Actor->DrawScale3D;
			}
		}

		UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
		check(LODLevel);
		if (LODLevel->RequiredModule->bUseLocalSpace)
		{
			FromOrigin = Particle.Location.SafeNormal();
			if (bInWorldSpace == TRUE)
			{
				Vel = Owner->Component->LocalToWorld.InverseTransformNormal(Vel);
			}
		}
		else
		{
			FromOrigin = (Particle.Location - Owner->Location).SafeNormal();
			if (bInWorldSpace == FALSE)
			{
				Vel = Owner->Component->LocalToWorld.TransformNormal(Vel);
			}
		}
		Vel *= OwnerScale;
		Vel += FromOrigin * StartVelocityRadial.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream) * OwnerScale;
		Particle.Velocity		+= Vel;
		Particle.BaseVelocity	+= Vel;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleVelocityInheritParent implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleVelocity_Seeded);

/**
 *	Called on a particle when it is spawned.
 *
 *	@param	Owner			The emitter instance that spawned the particle
 *	@param	Offset			The payload data offset for this module
 *	@param	SpawnTime		The spawn time of the particle
 */
void UParticleModuleVelocity_Seeded::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
UINT UParticleModuleVelocity_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleVelocity_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

/** 
 *	Called when an emitter instance is looping...
 *
 *	@param	Owner	The emitter instance that owns this module
 */
void UParticleModuleVelocity_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == TRUE)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleVelocityInheritParent implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleVelocityInheritParent);

void UParticleModuleVelocityInheritParent::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;

	FVector Vel = FVector(0.0f);

	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		Vel = Owner->Component->LocalToWorld.InverseTransformNormal(Owner->Component->PartSysVelocity);
	}
	else
	{
		Vel = Owner->Component->PartSysVelocity;
	}

	FVector vScale = Scale.GetValue(Owner->EmitterTime, Owner->Component);

	Vel *= vScale;

	Particle.Velocity		+= Vel;
	Particle.BaseVelocity	+= Vel;
}

/*-----------------------------------------------------------------------------
	UParticleModuleVelocityOverLifetime implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleVelocityOverLifetime);

void UParticleModuleVelocityOverLifetime::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	if (Absolute)
	{
		SPAWN_INIT;
		FVector OwnerScale(1.0f);
		if ((bApplyOwnerScale == TRUE) && Owner && Owner->Component)
		{
			OwnerScale = Owner->Component->Scale * Owner->Component->Scale3D;
			AActor* Actor = Owner->Component->GetOwner();
			if (Actor && !Owner->Component->AbsoluteScale)
			{
				OwnerScale *= Actor->DrawScale * Actor->DrawScale3D;
			}
		}
		FVector Vel = VelOverLife.GetValue(Particle.RelativeTime, Owner->Component) * OwnerScale;
		Particle.Velocity		= Vel;
		Particle.BaseVelocity	= Vel;
	}
}

void UParticleModuleVelocityOverLifetime::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	FVector OwnerScale(1.0f);
	if ((bApplyOwnerScale == TRUE) && Owner && Owner->Component)
	{
		OwnerScale = Owner->Component->Scale * Owner->Component->Scale3D;
		AActor* Actor = Owner->Component->GetOwner();
		if (Actor && !Owner->Component->AbsoluteScale)
		{
			OwnerScale *= Actor->DrawScale * Actor->DrawScale3D;
		}
	}
	if (Absolute)
	{
		if (LODLevel->RequiredModule->bUseLocalSpace == FALSE)
		{
			if (bInWorldSpace == FALSE)
			{
				FVector Vel;
				BEGIN_UPDATE_LOOP;
				{
					Vel = VelOverLife.GetValue(Particle.RelativeTime, Owner->Component);
					Particle.Velocity = Owner->Component->LocalToWorld.TransformNormal(Vel) * OwnerScale;
				}
				END_UPDATE_LOOP;
			}
			else
			{
				BEGIN_UPDATE_LOOP;
				{
					Particle.Velocity = VelOverLife.GetValue(Particle.RelativeTime, Owner->Component) * OwnerScale;
				}
				END_UPDATE_LOOP;
			}
		}
		else
		{
			if (bInWorldSpace == FALSE)
			{
				BEGIN_UPDATE_LOOP;
				{
					Particle.Velocity = VelOverLife.GetValue(Particle.RelativeTime, Owner->Component) * OwnerScale;
				}
				END_UPDATE_LOOP;
			}
			else
			{
				FVector Vel;
				FMatrix InvMat = Owner->Component->LocalToWorld.Inverse();
				BEGIN_UPDATE_LOOP;
				{
					Vel = VelOverLife.GetValue(Particle.RelativeTime, Owner->Component);
					Particle.Velocity = InvMat.TransformNormal(Vel) * OwnerScale;
				}
				END_UPDATE_LOOP;
			}
		}
	}
	else
	{
		if (LODLevel->RequiredModule->bUseLocalSpace == FALSE)
		{
			FVector Vel;
			if (bInWorldSpace == FALSE)
			{
				BEGIN_UPDATE_LOOP;
				{
					Vel = VelOverLife.GetValue(Particle.RelativeTime, Owner->Component);
					Particle.Velocity *= Owner->Component->LocalToWorld.TransformNormal(Vel) * OwnerScale;
				}
				END_UPDATE_LOOP;
			}
			else
			{
				BEGIN_UPDATE_LOOP;
				{
					Particle.Velocity *= VelOverLife.GetValue(Particle.RelativeTime, Owner->Component) * OwnerScale;
				}
				END_UPDATE_LOOP;
			}
		}
		else
		{
			if (bInWorldSpace == FALSE)
			{
				BEGIN_UPDATE_LOOP;
				{
					Particle.Velocity *= VelOverLife.GetValue(Particle.RelativeTime, Owner->Component) * OwnerScale;
				}
				END_UPDATE_LOOP;
			}
			else
			{
				FVector Vel;
				FMatrix InvMat = Owner->Component->LocalToWorld.Inverse();
				BEGIN_UPDATE_LOOP;
				{
					Vel = VelOverLife.GetValue(Particle.RelativeTime, Owner->Component);
					Particle.Velocity *= InvMat.TransformNormal(Vel) * OwnerScale;
				}
				END_UPDATE_LOOP;
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleVelocityCone implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleVelocityCone);

/**
 *	Called on a particle that is freshly spawned by the emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	SpawnTime	The time of the spawn.
 */
void UParticleModuleVelocityCone::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
void UParticleModuleVelocityCone::SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream)
{
	static const FLOAT TwoPI = 2.0f * PI;
	static const FLOAT ToRads = PI / 180.0f;
	static const INT UUPerRad = 10430;
	static const FVector DefaultDirection(0.0f, 0.0f, 1.0f);
	
	// Calculate the owning actor's scale
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	FVector OwnerScale(1.0f);
	if ((bApplyOwnerScale == TRUE) && Owner && Owner->Component)
	{
		OwnerScale = Owner->Component->Scale * Owner->Component->Scale3D;
		AActor* Actor = Owner->Component->GetOwner();
		if (Actor && !Owner->Component->AbsoluteScale)
		{
			OwnerScale *= Actor->DrawScale * Actor->DrawScale3D;
		}
	}
	
	// Spawn particles
	SPAWN_INIT
	{
		// Calculate the default position (prior to the Direction vector being factored in)
		const FLOAT SpawnAngle = Angle.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
		const FLOAT SpawnVelocity = Velocity.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
		const FLOAT LatheAngle = appSRand() * TwoPI;
		const FRotator DefaultDirectionRotater((INT)(SpawnAngle * ToRads * UUPerRad), (INT)(LatheAngle * UUPerRad), 0);
		const FRotationMatrix DefaultDirectionRotation(DefaultDirectionRotater);
		const FVector DefaultSpawnDirection = DefaultDirectionRotation.TransformFVector(DefaultDirection);

		// Orientate the cone along the direction vector		
		const FVector ForwardDirection = (Direction != FVector::ZeroVector)? Direction.SafeNormal(): DefaultDirection;
		FVector UpDirection(0.0f, 0.0f, 1.0f);
		FVector RightDirection(1.0f, 0.0f, 0.0f);

		if ((ForwardDirection != UpDirection) && (-ForwardDirection != UpDirection))
		{
			RightDirection = UpDirection ^ ForwardDirection;
			UpDirection = ForwardDirection ^ RightDirection;
		}
		else
		{
			UpDirection = ForwardDirection ^ RightDirection;
			RightDirection = UpDirection ^ ForwardDirection;
		}

		FMatrix DirectionRotation;
		DirectionRotation.SetIdentity();
		DirectionRotation.SetAxis(0, RightDirection.SafeNormal());
		DirectionRotation.SetAxis(1, UpDirection.SafeNormal());
		DirectionRotation.SetAxis(2, ForwardDirection);
		FVector SpawnDirection = DirectionRotation.TransformFVector(DefaultSpawnDirection);
	
		// Transform according to world and local space flags 
		if (!LODLevel->RequiredModule->bUseLocalSpace && !bInWorldSpace)
		{
			SpawnDirection = Owner->Component->LocalToWorld.TransformNormal(SpawnDirection);
		}
		else if (LODLevel->RequiredModule->bUseLocalSpace && bInWorldSpace)
		{
			SpawnDirection = Owner->Component->LocalToWorld.InverseTransformNormal(SpawnDirection);
		}

		// Set final velocity vector
		const FVector FinalVelocity = SpawnDirection * SpawnVelocity * OwnerScale;
		Particle.Velocity += FinalVelocity;
		Particle.BaseVelocity += FinalVelocity;
	}
}

/** 
 *	Render the modules 3D visualization helper primitive.
 *
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the module.
 *	@param	View		The scene view that is being rendered.
 *	@param	PDI			The FPrimitiveDrawInterface to use for rendering.
 */
void UParticleModuleVelocityCone::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITOR
	FLOAT ConeMaxAngle = 0.0f;
	FLOAT ConeMinAngle = 0.0f;
	Angle.GetOutRange(ConeMinAngle, ConeMaxAngle);

	FLOAT ConeMaxVelocity = 0.0f;
	FLOAT ConeMinVelocity = 0.0f;
	Velocity.GetOutRange(ConeMinVelocity, ConeMaxVelocity);

	FLOAT MaxLifetime = 0.0f;
	TArray<UParticleModule*>& Modules = Owner->SpriteTemplate->GetCurrentLODLevel(Owner)->Modules;
	for (INT ModuleIndex = 0; ModuleIndex < Modules.Num(); ModuleIndex++)
	{
		UParticleModuleLifetimeBase* LifetimeMod = Cast<UParticleModuleLifetimeBase>(Modules(ModuleIndex));
		if (LifetimeMod != NULL)
		{
			MaxLifetime = LifetimeMod->GetMaxLifetime();
			break;
		}
	}

	const INT ConeSides = 16;
	const FLOAT ConeRadius = ConeMaxVelocity * MaxLifetime;

	// Calculate direction transform
	const FVector DefaultDirection(0.0f, 0.0f, 1.0f);
	const FVector ForwardDirection = (Direction != FVector::ZeroVector)? Direction.SafeNormal(): DefaultDirection;
	FVector UpDirection(0.0f, 0.0f, 1.0f);
	FVector RightDirection(1.0f, 0.0f, 0.0f);

	if ((ForwardDirection != UpDirection) && (-ForwardDirection != UpDirection))
	{
		RightDirection = UpDirection ^ ForwardDirection;
		UpDirection = ForwardDirection ^ RightDirection;
	}
	else
	{
		UpDirection = ForwardDirection ^ RightDirection;
		RightDirection = UpDirection ^ ForwardDirection;
	}

	FMatrix DirectionRotation;
	DirectionRotation.SetIdentity();
	DirectionRotation.SetAxis(0, RightDirection.SafeNormal());
	DirectionRotation.SetAxis(1, UpDirection.SafeNormal());
	DirectionRotation.SetAxis(2, ForwardDirection);

	// Calculate the owning actor's scale and rotation
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	FVector OwnerScale(1.0f);
	FRotationMatrix OwnerRotation(FRotator(0, 0, 0));
	if (Owner && Owner->Component)
	{
		AActor* Actor = Owner->Component->GetOwner();
		if (Actor)
		{
			if (bApplyOwnerScale == TRUE)
			{
				OwnerScale = Owner->Component->Scale * Owner->Component->Scale3D;
				if (Actor && !Owner->Component->AbsoluteScale)
				{
					OwnerScale *= Actor->DrawScale * Actor->DrawScale3D;
				}
			}

			OwnerRotation = FRotationMatrix(Actor->Rotation);
		}
	}

	const FVector LocalToWorldOrigin = Owner->Component->LocalToWorld.GetOrigin();
	FMatrix LocalToWorld = Owner->Component->LocalToWorld.RemoveTranslation();
	LocalToWorld.RemoveScaling();
	
	FMatrix Transform;
	Transform.SetIdentity();

	// DrawWireCone() draws a cone down the X axis, but this cone's default direction is down Z
	const FRotationMatrix XToZRotation(FRotator((INT)(HALF_PI * 10430), 0, 0));
	Transform *= XToZRotation;

	// Apply scale
	Transform.SetAxis(0, Transform.GetAxis(0) * OwnerScale.X);
	Transform.SetAxis(1, Transform.GetAxis(1) * OwnerScale.Y);
	Transform.SetAxis(2, Transform.GetAxis(2) * OwnerScale.Z);

	// Apply direction transform
	Transform *= DirectionRotation;

	// Transform according to world and local space flags 
	if (!LODLevel->RequiredModule->bUseLocalSpace && !bInWorldSpace)
	{
		Transform *= LocalToWorld;
	}
	else if (LODLevel->RequiredModule->bUseLocalSpace && bInWorldSpace)
	{
		Transform *= OwnerRotation;
		Transform *= LocalToWorld.Inverse();
	}
	else if (!bInWorldSpace)
	{
		Transform *= OwnerRotation;
	}

	// Apply translation
	Transform.SetOrigin(LocalToWorldOrigin);

	TArray<FVector> OuterVerts;
	TArray<FVector> InnerVerts;

	// Draw inner and outer cones
	DrawWireCone(PDI, Transform, ConeRadius, ConeMinAngle, ConeSides, ModuleEditorColor, SDPG_World, InnerVerts);
	DrawWireCone(PDI, Transform, ConeRadius, ConeMaxAngle, ConeSides, ModuleEditorColor, SDPG_World, OuterVerts);

	// Draw radial spokes
	for (INT i = 0; i < ConeSides; ++i)
	{
		PDI->DrawLine( OuterVerts(i), InnerVerts(i), ModuleEditorColor, SDPG_World );
	}
#endif
}
