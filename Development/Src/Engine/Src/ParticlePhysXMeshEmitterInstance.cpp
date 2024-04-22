/*=============================================================================
	ParticlePhysXMeshEmitterInstance.cpp: PhysX Emitter Source.
	Copyright 2007-2008 AGEIA Technologies.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EngineMaterialClasses.h"
#include "EnginePhysicsClasses.h"
#include "UnFracturedStaticMesh.h"
#include "EngineMeshClasses.h"
#include "LevelUtils.h"

#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
#include "UnNovodexSupport.h"
#include "PhysXParticleSystem.h"
#include "PhysXVerticalEmitter.h"
#include "PhysXParticleSetMesh.h"

IMPLEMENT_PARTICLEEMITTERINSTANCE_TYPE(FParticleMeshPhysXEmitterInstance);

FParticleMeshPhysXEmitterInstance::FParticleMeshPhysXEmitterInstance(class UParticleModuleTypeDataMeshPhysX &TypeData) : 
	PhysXTypeData(TypeData), 
	NumSpawnedParticles(0),
	SpawnEstimateTime(0.0f),
	PSet(NULL)
{
	SpawnEstimateRate = 0.0f;
	SpawnEstimateLife = 0.0f;
	LodEmissionBudget = INT_MAX;
	LodEmissionRemainder = 0.0f;
}

FParticleMeshPhysXEmitterInstance::~FParticleMeshPhysXEmitterInstance()
{
	CleanUp();
}

void FParticleMeshPhysXEmitterInstance::KillParticlesForced(UBOOL bFireEvents)
{
	CleanUp();
}

void FParticleMeshPhysXEmitterInstance::CleanUp()
{
	if(PhysXTypeData.PhysXParSys)
	{
		UPhysXParticleSystem& ParSys = *PhysXTypeData.PhysXParSys;
		ParSys.RemoveSpawnInstance(this);
		if(ParSys.PSys && PSet)
		{
			ParSys.PSys->RemoveParticleSet(PSet);
		}
	}

	if(PSet)
	{
		delete PSet;
		PSet = NULL;
	}
	ActiveParticles = 0;
}

void FParticleMeshPhysXEmitterInstance::RemovedFromScene()
{
	CleanUp();
}

FPhysXParticleSystem& FParticleMeshPhysXEmitterInstance::GetPSys()
{
	check(PhysXTypeData.PhysXParSys && PhysXTypeData.PhysXParSys->PSys);
	return *PhysXTypeData.PhysXParSys->PSys;
}

UBOOL FParticleMeshPhysXEmitterInstance::TryConnect()
{
	if(Component->bWarmingUp)
		return FALSE;

	if(!PhysXTypeData.PhysXParSys)
		return FALSE;

	if(!PhysXTypeData.PhysXParSys->TryConnect())
		return FALSE;
	if(!PSet)
	{
		PSet = new FPhysXParticleSetMesh(*this);
	}
	return TRUE;
}
void FParticleMeshPhysXEmitterInstance::AsyncProccessSpawnedParticles( FLOAT DeltaTime, INT FirstIndex )
{
	FPhysXParticleSystem& PSys = GetPSys();
	FVector LODOrigin(0,0,0);
	UBOOL bUseDistCulling = GetLODOrigin(LODOrigin);

	FLOAT LifetimeSum = 0.0f;

	//Filter particle for LOD and convert some data.
	UBOOL bBoundingBoxNeedsUpdate = FALSE;

	INT LastParticleIndex = ActiveParticles-1;
	for(INT i=LastParticleIndex; i>=FirstIndex; i--)
	{
		INT NoOfSpawnParticles = LastParticleIndex + 1 - FirstIndex;
		FBaseParticle *Particle = (FBaseParticle*)((BYTE*)ParticleData + ParticleStride*ParticleIndices[i]);
		// if the spawn particles exceeds the number of max particles for the PhysX Particle system, remove it and reupdate the bounding box.
		if(PSys.GetNumParticles() + NoOfSpawnParticles > PSys.Params.MaxParticles)
		{
			RemoveParticleFromActives(i);
			LastParticleIndex--;
			bBoundingBoxNeedsUpdate = TRUE;
			continue;
		}

		if(bUseDistCulling && PSys.GetLODDistanceSq(LODOrigin, Particle->Location) > PSys.VerticalPacketRadiusSq)
		{
			RemoveParticleFromActives(i);
			LastParticleIndex--;
			bBoundingBoxNeedsUpdate = TRUE;
			continue;
		}
		LifetimeSum += (Particle->OneOverMaxLifetime > 0.0f)?(1.0f/Particle->OneOverMaxLifetime):0.0f;
	}
	NumSpawnedParticles = LastParticleIndex + 1 - FirstIndex;
	SpawnEstimateUpdate(DeltaTime, NumSpawnedParticles, LifetimeSum);

	if(bBoundingBoxNeedsUpdate)
	{
		UpdateBoundingBox(DeltaTime);
	}
}

void FParticleMeshPhysXEmitterInstance::Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning)
{
	// checks if the PhysX Fluid has been created
	if(!TryConnect())
	{
		return;
	}
	FRBPhysScene* Scene = PhysXTypeData.PhysXParSys->GetScene();
	check(Scene);

	//The timestep passed into here should result in the right compartment timimg, we query after, in editor.
	Scene->PhysXEmitterManager->Tick(DeltaTime);

	//This relies on NxCompartment::setTiming() be called befor this tick.
	DeltaTime = Scene->PhysXEmitterManager->GetEffectiveStepSize();

	FPhysXParticleSystem& PSys = GetPSys();
	PSys.AddSpawnInstance(this);
	NumSpawnedParticles = 0;
	INT ActiveParticlesOld = ActiveParticles;
	OverwriteUnsupported();
	FParticleEmitterInstance::Tick(DeltaTime, bSuppressSpawning);
	RestoreUnsupported();

	AsyncProccessSpawnedParticles(DeltaTime, ActiveParticlesOld);
	//plotf( TEXT("DEBUG_VE_PLOT numSpawnedEmitterRangeClamp %d"), NumSpawnedParticles);
}

FParticleSystemSceneProxy* FParticleMeshPhysXEmitterInstance::GetSceneProxy()
{
	if(!Component)
		return NULL;

	return (FParticleSystemSceneProxy*)Scene_GetProxyFromInfo(Component->SceneInfo);
}

UBOOL FParticleMeshPhysXEmitterInstance::GetLODOrigin(FVector& OutLODOrigin)
{
	FParticleSystemSceneProxy* SceneProxy = GetSceneProxy();
	if(SceneProxy)
	{
		OutLODOrigin = SceneProxy->GetLODOrigin();
	}
	return SceneProxy != NULL;
}

UBOOL FParticleMeshPhysXEmitterInstance::GetLODNearClippingPlane(FPlane& OutClippingPlane)
{
	FParticleSystemSceneProxy* SceneProxy = GetSceneProxy();
	if(!SceneProxy)
	{
		return FALSE;
	}
	UBOOL HasCP = SceneProxy->GetNearClippingPlane(OutClippingPlane);
	return HasCP;
}

FDynamicEmitterDataBase* FParticleMeshPhysXEmitterInstance::GetDynamicData(UBOOL bSelected)
{
	OverwriteUnsupported();
	FDynamicEmitterDataBase* Ret = FParticleMeshEmitterInstance::GetDynamicData(bSelected);
	RestoreUnsupported();
	return Ret;
}

/**
 *	Updates the dynamic data for the instance
 *
 *	@param	DynamicData		The dynamic data to fill in
 *	@param	bSelected		TRUE if the particle system component is selected
 */
UBOOL FParticleMeshPhysXEmitterInstance::UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected)
{
	OverwriteUnsupported();
	UBOOL bResult = FParticleMeshEmitterInstance::UpdateDynamicData(DynamicData, bSelected);
	RestoreUnsupported();
	return bResult;
}

void SetRotation(PhysXRenderParticleMesh& RenderParticle, const FMeshRotationPayloadData& PayloadData)
{
	//This still seems kinda broken, but less.
	FRotator Rotator;
	Rotator = FRotator::MakeFromEuler(PayloadData.Rotation);
	RenderParticle.Rot = Rotator.Quaternion();
	RenderParticle.Rot.Normalize();

	Rotator = FRotator::MakeFromEuler(PayloadData.RotationRateBase);

	//x:roll, y:pitch, z:yaw
	//Wx = rollrate - yawrate * sin(pitch)
	//Wy = pitchrate * cos(roll) + yawrate * sin(roll) * cos(pitch) 
	//Wz = yawrate * cos(roll) * cos(pitch) - pitchrate * sin(roll)

	FVector Rot = PayloadData.Rotation*(2*PI)/360.0f;
	FVector Rate = PayloadData.RotationRateBase*(2*PI)/360.0f;

	RenderParticle.AngVel.X = Rate.X - Rate.Z*appSin(Rot.Y);
	RenderParticle.AngVel.Y = Rate.Y * appCos(Rot.X) + Rate.Z * appSin(Rot.X) * appCos(Rot.Y);
	RenderParticle.AngVel.Z = Rate.Z * appCos(Rot.X) * appCos(Rot.Y) - Rate.Y*appSin(Rot.X);
}

/**
Assumes the base class emitter instance has spawned particles, which now need to be added to the 
FPhysXParticleSystem and to the RenderInstance.

UE Particle lifetimes are converted to PhysX SDK lifetimes.
Adds particles to the PhysXParticle system and creates entries in the RenderInstance 
with SDK particle Ids.

Sets appropriate initial sizes and max lifetimes for the new particles in the RenderInstance.
*/
void FParticleMeshPhysXEmitterInstance::SpawnSyncPhysX()
{
	PostSpawnVerticalLod();

	if(NumSpawnedParticles == 0)
	{
		return;
	}

	FPhysXParticleSystem& PSys = GetPSys();
	INT FirstSpawnedIndex = ActiveParticles - NumSpawnedParticles;

	check(PSet);
	if( PSet->GetNumRenderParticles() != FirstSpawnedIndex )
	{
		// This should not happen, but just in case it does we try to handle it
		// ActiveParticles out of sync with PSet render particles, delete particles to put them back in sync
		INT NumRenderParticles = PSet->GetNumRenderParticles();
		debugf( NAME_DevPhysics, TEXT("Warning, particles out of sync: RenderParticles = %d, FirstSpawnedIndex = %d"),
			NumRenderParticles, FirstSpawnedIndex );

		// First remove any newly created particles from emitter instance
		while( ActiveParticles > FirstSpawnedIndex )
		{
			RemoveParticleFromActives(ActiveParticles-1);
		}

		// Now clean up PSet or emitter instance to get them back in sync
		if( NumRenderParticles > ActiveParticles )
		{
			debugf( NAME_DevPhysics, TEXT("Removing PSet particles to get particles back in sync") );
			while( NumRenderParticles > ActiveParticles )
			{
				PSet->RemoveParticle(NumRenderParticles-1, true);
				--NumRenderParticles;
			}
		}
		else if( NumRenderParticles < ActiveParticles )
		{
			debugf( NAME_DevPhysics, TEXT("Removing new active particles to get particles back in sync") );
			while( NumRenderParticles < ActiveParticles )
			{
				RemoveParticleFromActives(ActiveParticles-1);
			}
		}

		NumSpawnedParticles = 0;
		return;
	}

	//At least temporarily do local allocation:
	struct FTempParticle
	{
		NxVec3 Pos;
		NxVec3 Vel;
	};
	FTempParticle* TempParticleBuffer = (FTempParticle*)appAlloca(NumSpawnedParticles*sizeof(FTempParticle));

	NxF32* BufferLife = 0;
	NxU32 BufferLifeStride = 0;
	if (PhysXTypeData.PhysXParSys->ParticleSpawnReserve > 0)
	{
		BufferLife = (NxF32*)appAlloca(sizeof(NxF32) * NumSpawnedParticles);
		BufferLifeStride = sizeof(NxF32);
		for(INT i=0; i<NumSpawnedParticles; ++i)
		{
			DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[FirstSpawnedIndex+i]);
			FTempParticle& Dest = TempParticleBuffer[i]; 			
			Dest.Pos = U2NPosition(Particle->Location);
			Dest.Vel = U2NPosition(Particle->Velocity);
			// Set lifetime to something large but finite, or particle priority mode won't work
			BufferLife[i] = Particle->OneOverMaxLifetime ? 1.0f / Particle->OneOverMaxLifetime : 10000.0f;
		}
	}
	else
	{
		for(INT i = 0; i<NumSpawnedParticles; i++)
		{
			DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[FirstSpawnedIndex+i]);
			FTempParticle& Dest = TempParticleBuffer[i]; 			
			Dest.Pos = U2NPosition(Particle->Location);
			Dest.Vel = U2NPosition(Particle->Velocity);
		}
	}

	NxParticleData particleData;
	particleData.bufferPos = (NxF32*)&TempParticleBuffer[0].Pos.x;
	particleData.bufferVel = (NxF32*)&TempParticleBuffer[0].Vel.x;
	particleData.bufferLife = BufferLife;
	particleData.numParticlesPtr = (NxU32*)&NumSpawnedParticles;
	particleData.bufferPosByteStride = sizeof(FTempParticle);
	particleData.bufferVelByteStride = sizeof(FTempParticle);
	particleData.bufferLifeByteStride = BufferLifeStride;
	check(particleData.isValid());

	check(PSet);

	INT SdkIndex = PSys.NumParticlesSdk;
	INT NumCreated = PSys.AddParticles(particleData, PSet);

	INT NumFailed = NumSpawnedParticles - NumCreated;
	for(INT i=0; i<NumFailed; i++)
	{
		RemoveParticleFromActives(ActiveParticles-1);
	}

	//Creation for FPhysXParticleSet
	for(INT i=0; i<NumCreated; i++)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[FirstSpawnedIndex+i]);
		PhysXParticle& SdkParticle = PSys.ParticlesSdk[SdkIndex];
		PhysXRenderParticleMesh RenderParticle;
		if(MeshRotationActive)
		{
			FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshRotationOffset);
			SetRotation(RenderParticle, *PayloadData);
		}
		else
		{
			RenderParticle.Rot = FQuat::Identity;
			RenderParticle.AngVel = FVector(0,0,0);
		}
		RenderParticle.Id = SdkParticle.Id;
		RenderParticle.ParticleIndex = SdkIndex;
		RenderParticle.OneOverMaxLifetime = Particle.OneOverMaxLifetime;
		RenderParticle.RelativeTime = 0.0f;
		check(FirstSpawnedIndex + i == PSet->GetNumRenderParticles());
		PSet->AddParticle(RenderParticle, Component);
		SdkIndex++;
	}

	NumSpawnedParticles = 0;
}

FLOAT FParticleMeshPhysXEmitterInstance::Spawn(FLOAT DeltaTime)
{
	FLOAT result = FParticleMeshEmitterInstance::Spawn(DeltaTime);
	return result;
}

FLOAT FParticleMeshPhysXEmitterInstance::Spawn(FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime, INT Burst, FLOAT BurstTime)
{
	FLOAT result = FParticleMeshEmitterInstance::Spawn(OldLeftover, Rate, DeltaTime, Burst, BurstTime);
	return result;
}

void FParticleMeshPhysXEmitterInstance::RemoveParticles()
{
	ActiveParticles = 0;
	NumSpawnedParticles = 0;
}

void FParticleMeshPhysXEmitterInstance::ParticlePrefetch() {}

#define PHYSX_SPAWN_ESTIMATE_MAX 8

void FParticleMeshPhysXEmitterInstance::SpawnEstimateUpdate(FLOAT DeltaTime, INT NumSpawnedParticles, FLOAT LifetimeSum)
{	
	SpawnEstimateRate = 0.0f;
	SpawnEstimateLife = 0.0f;

	SpawnEstimateTime += DeltaTime;
	
	//A ringbuffer would be nice here...
	if(NumSpawnedParticles > 0)
	{
		if(SpawnEstimateSamples.Num() == PHYSX_SPAWN_ESTIMATE_MAX)
		{
			SpawnEstimateSamples.Pop();
		}

		if(SpawnEstimateSamples.Num() > 0)
		{
			SpawnEstimateSamples(0).DeltaTime = SpawnEstimateTime;
		}

		SpawnEstimateSamples.Insert(0);
		SpawnEstimateSample& NewSample = SpawnEstimateSamples(0);
		NewSample.DeltaTime = 0.0f;
		NewSample.NumSpawned = NumSpawnedParticles;
		NewSample.LifetimeSum = LifetimeSum;
		
		SpawnEstimateTime = 0.0f;
	}

	if(SpawnEstimateSamples.Num() > 1) // We need at least 2 samples for MeanDeltaTime
	{
		SpawnEstimateSamples(0).DeltaTime = SpawnEstimateTime;
		
		//Find mean sample time. (Don't include latest sample)
		FLOAT MeanDeltaTime = 0.0f;
		for(INT i=1; i<SpawnEstimateSamples.Num(); i++)
		{
			SpawnEstimateSample& Sample = SpawnEstimateSamples(i);
			MeanDeltaTime += Sample.DeltaTime;
		}
		if(MeanDeltaTime > 0.0f)
		{
			MeanDeltaTime /= (SpawnEstimateSamples.Num()-1);
		}


		INT StartIndex = 1;
		if(SpawnEstimateTime > MeanDeltaTime) //include current measurment
		{
			StartIndex = 0;
		}

		FLOAT WeightSum = 0.0f;
		FLOAT TimeSum = 0.0f;
		for(INT i=StartIndex; i<SpawnEstimateSamples.Num(); i++)
		{
			SpawnEstimateSample& Sample = SpawnEstimateSamples(i);
			TimeSum += Sample.DeltaTime;
			FLOAT Weight = 1.0f/(TimeSum);
			WeightSum += Weight;

			SpawnEstimateRate += Weight*Sample.NumSpawned;
			SpawnEstimateLife += Weight*Sample.LifetimeSum/Sample.NumSpawned;
		}

		if(WeightSum > 0.0f)
		{
			SpawnEstimateRate/=(WeightSum*MeanDeltaTime);
			SpawnEstimateLife/=WeightSum;
		}

	}

	//plotf( TEXT("DEBUG_VE_PLOT %x_SpawnEstimateRate %f"), this, SpawnEstimateRate);
	//plotf( TEXT("DEBUG_VE_PLOT %x_SpawnEstimateLife %f"), this, SpawnEstimateLife);
	//plotf( TEXT("DEBUG_VE_PLOT %x_NumSpawnedParticles %d"), this, NumSpawnedParticles);
	//plotf( TEXT("DEBUG_VE_PLOT %x_DeltaTime %f"), this, DeltaTime);
}


/** This will look at all of the emitters in this particle system and return their locations in world space and their spawn time as the W compoenent. **/
void FParticleMeshPhysXEmitterInstance::GetLocationsOfAllParticles( TArray<FVector4>& LocationsOfEmitters )
{
	FBaseParticle* pBaseParticle = (FBaseParticle*)ParticleData;

	const INT LastParticleIndex = ActiveParticles - 1;
	for( INT i = LastParticleIndex; i >= 0; i-- )
	{
		FBaseParticle* Particle = (FBaseParticle*)(((BYTE*)pBaseParticle) + ParticleStride*i);
		if( Particle != NULL )
		{
			LocationsOfEmitters.AddItem( Particle->Location );
		}
	}
}


INT FParticleMeshPhysXEmitterInstance::GetSpawnVolumeEstimate()
{
	return appFloor(SpawnEstimateRate * SpawnEstimateLife);
}

void FParticleMeshPhysXEmitterInstance::SetEmissionBudget(INT Budget)
{
	LodEmissionBudget = Budget;
}

FLOAT FParticleMeshPhysXEmitterInstance::GetWeightForSpawnLod()
{
	check(PSet);
	return PSet->GetWeightForSpawnLod();
}

void FParticleMeshPhysXEmitterInstance::OverwriteUnsupported()
{
	//Not supporting bUseLocalSpace:
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	Stored_bUseLocalSpace = LODLevel->RequiredModule->bUseLocalSpace;
	LODLevel->RequiredModule->bUseLocalSpace = FALSE;	
}

void FParticleMeshPhysXEmitterInstance::RestoreUnsupported()
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	LODLevel->RequiredModule->bUseLocalSpace = Stored_bUseLocalSpace;
}


/**
 * Captures dynamic replay data for this particle system.
 *
 * @param	OutData		[Out] Data will be copied here
 *
 * @return Returns TRUE if successful
 */
UBOOL FParticleMeshPhysXEmitterInstance::FillReplayData( FDynamicEmitterReplayDataBase& OutData )
{
	// Call parent implementation first to fill in common particle source data
	// NOTE: We skip the Mesh implementation since we're replacing it with our own here
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


	OutData.eEmitterType = DET_Mesh;

	FDynamicMeshEmitterReplayData* NewReplayData =
		static_cast< FDynamicMeshEmitterReplayData* >( &OutData );


	// Get the material instance. If none is present, use the DefaultMaterial
	UMaterialInterface* MatInterface = NULL;
	if (LODLevel->TypeDataModule)
	{
		UParticleModuleTypeDataMesh* MeshTD = CastChecked<UParticleModuleTypeDataMesh>(LODLevel->TypeDataModule);
		if (MeshTD->bOverrideMaterial == TRUE)
		{
			if (CurrentMaterial)
			{
				MatInterface = CurrentMaterial;
			}
		}
	}
	NewReplayData->MaterialInterface = MatInterface;


	// If there are orbit modules, add the orbit module data
	if (LODLevel->OrbitModules.Num() > 0)
	{
		UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels(0);
		UParticleModuleOrbit* LastOrbit = HighestLODLevel->OrbitModules(LODLevel->OrbitModules.Num() - 1);
		check(LastOrbit);

		UINT* LastOrbitOffset = ModuleOffsetMap.Find(LastOrbit);
		NewReplayData->OrbitModuleOffset = *LastOrbitOffset;
	}



	// Mesh settings
	NewReplayData->bScaleUV = LODLevel->RequiredModule->bScaleUV;
	NewReplayData->SubUVInterpMethod = LODLevel->RequiredModule->InterpolationMethod;
	NewReplayData->SubUVDataOffset = SubUVDataOffset;
	NewReplayData->SubImages_Horizontal = LODLevel->RequiredModule->SubImages_Horizontal;
	NewReplayData->SubImages_Vertical = LODLevel->RequiredModule->SubImages_Vertical;
	NewReplayData->MeshRotationOffset = MeshRotationOffset;
	NewReplayData->bMeshRotationActive = MeshRotationActive;
	NewReplayData->MeshAlignment = MeshTypeData->MeshAlignment;


	// Never use 'local space' for PhysX meshes
	NewReplayData->bUseLocalSpace = FALSE;


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
		NewReplayData->Scale *= Component->Scale * Component->Scale3D;
		AActor* Actor = Component->GetOwner();
		if (Actor && !Component->AbsoluteScale)
		{
			NewReplayData->Scale *= Actor->DrawScale * Actor->DrawScale3D;
		}
	}

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


	return TRUE;
}

void FParticleMeshPhysXEmitterInstance::UpdateBoundingBox( FLOAT DeltaTime )
{
	if (Component)
	{
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
				warnf(TEXT("MeshEmitter with no mesh set?? - %s"), Component->Template ? *(Component->Template->GetPathName()) : TEXT("??????"));
				MeshBound = FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0);
			}
		}
		else
		{
			// This isn't used anywhere if the bWarmingUp flag is false, but GCC doesn't like it not touched.
			appMemzero(&MeshBound, sizeof(FBoxSphereBounds));
		}

		if (ActiveParticles > 0)
		{
			DECLARE_PARTICLE_PTR(NextParticle, ParticleData + ParticleStride * ParticleIndices[0]);
			PREFETCH(NextParticle);
		}

		ParticleBoundingBox.Init();
		for (INT i=0; i<ActiveParticles; i++)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);

			if (i + 1 < ActiveParticles)
			{
				const INT NextIndex = ParticleIndices[i+1];
				DECLARE_PARTICLE_PTR(NextParticle, ParticleData + ParticleStride * NextIndex);
				PREFETCH(NextParticle);
			}

			if (Component->bWarmingUp == FALSE)
			{	
				const FVector ParticleExtent = MeshBound.GetBox().GetExtent() * Particle.Size * Scale;
				ParticleBoundingBox += FBox(Particle.Location - ParticleExtent, Particle.Location + ParticleExtent);
			}
		}

		if (Component->bWarmingUp == FALSE)
		{	
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
void FParticleMeshPhysXEmitterInstance::PostSpawnVerticalLod()
{
	FLOAT LodEmissionRate = 0.0f;
	FLOAT LodEmissionLife = 0.0f;

	//Clamp emission number with LOD
	if(LodEmissionBudget < INT_MAX)
	{
		FLOAT ParamBias = NxMath::clamp(PhysXTypeData.VerticalLod.SpawnLodRateVsLifeBias, 1.0f, 0.0f);

		FLOAT Alpha = ((FLOAT)LodEmissionBudget)/(SpawnEstimateRate*SpawnEstimateLife);

		LodEmissionRate = SpawnEstimateRate * NxMath::pow(Alpha, ParamBias);
		LodEmissionLife = SpawnEstimateLife * NxMath::pow(Alpha, 1.0f - ParamBias);

		LodEmissionRemainder += LodEmissionRate*GDeltaTime;

		FLOAT RemainderInt = NxMath::floor(LodEmissionRemainder);

		while(NumSpawnedParticles > RemainderInt)
		{
			RemoveParticleFromActives(ActiveParticles-1);
			NumSpawnedParticles--;
		}
		LodEmissionRemainder -= (FLOAT)NumSpawnedParticles;
	}
	else
	{
		LodEmissionRemainder = 0.0f;
	}

	//Clamp emission lifetime with LOD
	if (LodEmissionLife > 0.0f)
	{
		FLOAT OneOverLodEmissionLife = 1.0f/LodEmissionLife;
		INT FirstSpawnedIndex = ActiveParticles - NumSpawnedParticles;
		for(INT i = 0; i<NumSpawnedParticles; i++)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[FirstSpawnedIndex+i]);
			if(Particle.OneOverMaxLifetime < OneOverLodEmissionLife)
			{
				Particle.OneOverMaxLifetime = OneOverLodEmissionLife;
			}
		}
	}
}

void FParticleMeshPhysXEmitterInstance::RemoveParticleFromActives( INT RemovedParticleIndex )
{
	if (RemovedParticleIndex < ActiveParticles)
	{
		// 0..ActiveParticles-1 are "living" particles, everything else is unused. the tail end of 
		//  this range stores newly spawned particles that are "in-utero" and haven't been committed
		//  to physx yet. these waiting particles need to remain at the end, as that's where 
		//  SpawnSyncPhysX will look for them. fill the hole left by the removed particle carefully, by
		//  bubbling the removed particle up through each sub-range...

		// RemovedParticleIndex is an index into an array of indices
		INT LastActiveParticleIndex = ActiveParticles-1;
		INT LastCommittedIndex = LastActiveParticleIndex - NumSpawnedParticles;

		// if we're removing a committed particle, we swap the last committed particle for this one.
		if (RemovedParticleIndex < LastCommittedIndex )
		{
			Exchange(ParticleIndices[RemovedParticleIndex], ParticleIndices[LastCommittedIndex]);
			RemovedParticleIndex = LastCommittedIndex;
			--LastCommittedIndex;
		}

		// this is where the newly spawned particle range should begin. note that if 
		//  NumSpawnedParticles==0, this is basically a no-op. 
		Exchange(ParticleIndices[RemovedParticleIndex], ParticleIndices[LastActiveParticleIndex]);

		ActiveParticles--;
	}
}


#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
