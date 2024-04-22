/*=============================================================================
	ParticlePhysXSpriteEmitterInstance.cpp: PhysX Emitter Source.
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
#include "PhysXParticleSetSprite.h"

IMPLEMENT_PARTICLEEMITTERINSTANCE_TYPE(FParticleSpritePhysXEmitterInstance);

FParticleSpritePhysXEmitterInstance::FParticleSpritePhysXEmitterInstance(class UParticleModuleTypeDataPhysX &TypeData) : 
	PhysXTypeData(TypeData), 
	NumSpawnedParticles(0),
	DensityPayloadOffset(0), 
	SpawnEstimateTime(0.0f),
	PSet(NULL)
{
	SpawnEstimateRate = 0.0f;
	SpawnEstimateLife = 0.0f;
	LodEmissionBudget = INT_MAX;
	LodEmissionRemainder = 0.0f;
}

FParticleSpritePhysXEmitterInstance::~FParticleSpritePhysXEmitterInstance()
{
	CleanUp();
}

void FParticleSpritePhysXEmitterInstance::KillParticlesForced(UBOOL bFireEvents)
{
	CleanUp();
}

void FParticleSpritePhysXEmitterInstance::CleanUp()
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


/** This will look at all of the emitters in this particle system and return their locations in world space and their spawn time as the W compoenent. **/
void FParticleSpritePhysXEmitterInstance::GetLocationsOfAllParticles( TArray<FVector4>& LocationsOfEmitters )
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


void FParticleSpritePhysXEmitterInstance::RemovedFromScene()
{
	CleanUp();
}

FPhysXParticleSystem& FParticleSpritePhysXEmitterInstance::GetPSys()
{
	check(PhysXTypeData.PhysXParSys && PhysXTypeData.PhysXParSys->PSys);
	return *PhysXTypeData.PhysXParSys->PSys;
}

UBOOL FParticleSpritePhysXEmitterInstance::TryConnect()
{
	if(Component->bWarmingUp)
		return FALSE;

	if(!PhysXTypeData.PhysXParSys)
		return FALSE;

	if(!PhysXTypeData.PhysXParSys->TryConnect())
		return FALSE;

	// create a new set of rendering data for this emitter instance
	if(!PSet)
	{
		PSet = new FPhysXParticleSetSprite(*this);
	}
	return TRUE;
}

void FParticleSpritePhysXEmitterInstance::AsyncProccessSpawnedParticles(FLOAT DeltaTime, INT FirstIndex)
{
	FPhysXParticleSystem& PSys = GetPSys();
	FVector LODOrigin(0,0,0);
	UBOOL bUseDistCulling = GetLODOrigin(LODOrigin);

	FLOAT LifetimeSum = 0.0f;
	UBOOL bBoundingBoxNeedsUpdate = FALSE;
	//Filter particle for LOD and convert some data.
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

void FParticleSpritePhysXEmitterInstance::Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning)
{
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
	FParticleSpriteSubUVEmitterInstance::Tick(DeltaTime, bSuppressSpawning);
	RestoreUnsupported();

	AsyncProccessSpawnedParticles(DeltaTime, ActiveParticlesOld);
	//plotf( TEXT("DEBUG_VE_PLOT numSpawnedEmitterRangeClamp %d"), NumSpawnedParticles);
}

void FParticleSpritePhysXEmitterInstance::UpdateBoundingBox(FLOAT DeltaTime)
{
	// The body of this function was copied from FParticleEmitterInstance::UpdateBoundingBox with
	// the position integration removed in order to make PSA_Velocity and SizeByVel work properly.
	if (Component)
	{
		// Take component scale into account
		FVector Scale = FVector(1.0f, 1.0f, 1.0f);
		Scale *= Component->Scale * Component->Scale3D;
		AActor* Actor = Component->GetOwner();
		if (Actor && !Component->AbsoluteScale)
		{
			Scale *= Actor->DrawScale * Actor->DrawScale3D;
		}

		UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
		check(LODLevel);

		FLOAT	NewRotation;
		ParticleBoundingBox.Init();
		UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels(0);
		check(HighestLODLevel);
		// For each particle, offset the box appropriately 
		for (INT i=0; i<ActiveParticles; i++)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
			
			// Do angular integrator, and wrap result to within +/- 2 PI
			if ((Particle.Flags & STATE_Particle_Freeze) == 0)
			{
				if ((Particle.Flags & STATE_Particle_FreezeRotation) == 0)
				{
					NewRotation = (DeltaTime * Particle.RotationRate) + Particle.Rotation;
				}
				else
				{
					NewRotation	= Particle.Rotation;
				}
			}
			else
			{
				NewRotation	= Particle.Rotation;
			}

			Particle.Rotation	 = appFmod(NewRotation, 2.f*(FLOAT)PI);

			if (Component->bWarmingUp == FALSE)
			{	
				if (LODLevel->OrbitModules.Num() > 0)
				{
					UParticleModuleOrbit* OrbitModule = HighestLODLevel->OrbitModules(LODLevel->OrbitModules.Num() - 1);
					if (OrbitModule)
					{
						UINT* OrbitOffsetIndex = ModuleOffsetMap.Find(OrbitModule);
						if (OrbitOffsetIndex)
						{
							INT CurrentOffset = *(OrbitOffsetIndex);
							const BYTE* ParticleBase = (const BYTE*)&Particle;
							PARTICLE_ELEMENT(FOrbitChainModuleInstancePayload, OrbitPayload);
							const FLOAT Max = OrbitPayload.Offset.GetAbsMax();
							FVector OrbitOffset(Max, Max, Max);
							ParticleBoundingBox += (Particle.Location  + OrbitOffset);
							ParticleBoundingBox += (Particle.Location  - OrbitOffset);
						}
					}
				}
				else
				{
					const FLOAT MaxComponentSize = (Particle.Size * Scale).GetAbsMax();
					const FVector ParticleExtent(MaxComponentSize / 2.0f, MaxComponentSize / 2.0f, MaxComponentSize / 2.0f);
					// Treat each particle as a cube whose sides are the length of the maximum component
					// This handles the particle's extents changing due to being camera facing
					ParticleBoundingBox += FBox(Particle.Location - ParticleExtent, Particle.Location + ParticleExtent);
				}
			}
		}

		if (Component->bWarmingUp == FALSE)
		{
			// Transform bounding box into world space if the emitter uses a local space coordinate system.
			if (LODLevel->RequiredModule->bUseLocalSpace) 
			{
				ParticleBoundingBox = ParticleBoundingBox.TransformBy(Component->LocalToWorld);
			}
		}
	}
}


FParticleSystemSceneProxy* FParticleSpritePhysXEmitterInstance::GetSceneProxy()
{
	if(!Component)
		return NULL;

	return (FParticleSystemSceneProxy*)Scene_GetProxyFromInfo(Component->SceneInfo);
}

UBOOL FParticleSpritePhysXEmitterInstance::GetLODOrigin(FVector& OutLODOrigin)
{
	FParticleSystemSceneProxy* SceneProxy = GetSceneProxy();
	if(SceneProxy)
	{
		OutLODOrigin = SceneProxy->GetLODOrigin();
	}
	return SceneProxy != NULL;
}

UBOOL FParticleSpritePhysXEmitterInstance::GetLODNearClippingPlane(FPlane& OutClippingPlane)
{
	FParticleSystemSceneProxy* SceneProxy = GetSceneProxy();
	if(!SceneProxy)
	{
		return FALSE;
	}
	UBOOL HasCP = SceneProxy->GetNearClippingPlane(OutClippingPlane);
	return HasCP;
}

/**
Assumes the base class emitter instance has spawned particles, which now need to be added to the 
FPhysXParticleSystem and to the RenderInstance.

UE Particle lifetimes are converted to PhysX SDK lifetimes.
Adds particles to the PhysXParticle system and creates entries in the RenderInstance 
with SDK particle Ids.

Sets appropriate initial sizes and max lifetimes for the new particles in the RenderInstance.
*/
void FParticleSpritePhysXEmitterInstance::SpawnSyncPhysX()
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
		PhysXRenderParticleSprite RenderParticle;
		
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

void FParticleSpritePhysXEmitterInstance::PostSpawnVerticalLod()
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

FLOAT FParticleSpritePhysXEmitterInstance::Spawn(FLOAT DeltaTime)
{
	FLOAT result = FParticleSpriteSubUVEmitterInstance::Spawn(DeltaTime);
	return result;
}

FLOAT FParticleSpritePhysXEmitterInstance::Spawn(FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime, INT Burst, FLOAT BurstTime)
{
	FLOAT result = FParticleSpriteSubUVEmitterInstance::Spawn(OldLeftover, Rate, DeltaTime, Burst, BurstTime);
	return result;
}

void FParticleSpritePhysXEmitterInstance::RemoveParticles()
{
	ActiveParticles = 0;
	NumSpawnedParticles = 0;
}

#define PHYSX_SPAWN_ESTIMATE_MAX 8

void FParticleSpritePhysXEmitterInstance::SpawnEstimateUpdate(FLOAT DeltaTime, INT NumSpawnedParticles, FLOAT LifetimeSum)
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

	if(SpawnEstimateSamples.Num() > 1)	// We need at least 2 samples for MeanDeltaTime
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

INT FParticleSpritePhysXEmitterInstance::GetSpawnVolumeEstimate()
{
	return appFloor(SpawnEstimateRate * SpawnEstimateLife);
}

void FParticleSpritePhysXEmitterInstance::SetEmissionBudget(INT Budget)
{
	LodEmissionBudget = Budget;
}

FLOAT FParticleSpritePhysXEmitterInstance::GetWeightForSpawnLod()
{
	check(PSet);
	return PSet->GetWeightForSpawnLod();
}

void FParticleSpritePhysXEmitterInstance::RemoveParticleFromActives(INT RemovedParticleIndex)
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
		INC_DWORD_STAT(STAT_SpriteParticlesKilled);
	}
}

void FParticleSpritePhysXEmitterInstance::OverwriteUnsupported()
{
	//Not supporting bUseLocalSpace:
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	Stored_bUseLocalSpace = LODLevel->RequiredModule->bUseLocalSpace;
	LODLevel->RequiredModule->bUseLocalSpace = FALSE;	
}

void FParticleSpritePhysXEmitterInstance::RestoreUnsupported()
{
	UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
	LODLevel->RequiredModule->bUseLocalSpace = Stored_bUseLocalSpace;
}

FDynamicEmitterDataBase* FParticleSpritePhysXEmitterInstance::GetDynamicData(UBOOL bSelected)
{
	OverwriteUnsupported();
	FDynamicEmitterDataBase* Ret = FParticleSpriteSubUVEmitterInstance::GetDynamicData(bSelected);
	RestoreUnsupported();
	return Ret;
}

/**
 *	Updates the dynamic data for the instance
 *
 *	@param	DynamicData		The dynamic data to fill in
 *	@param	bSelected		TRUE if the particle system component is selected
 */
UBOOL FParticleSpritePhysXEmitterInstance::UpdateDynamicData(FDynamicEmitterDataBase* DynamicData, UBOOL bSelected)
{
	OverwriteUnsupported();
	UBOOL bResult = FParticleSpriteSubUVEmitterInstance::UpdateDynamicData(DynamicData, bSelected);
	RestoreUnsupported();
	return bResult;
}

// Sprite PhysX Emitter Requires SubUV
UINT FParticleSpritePhysXEmitterInstance::RequiredBytes()
{
	SubUVDataOffset = PayloadOffset;
	UINT uiBytes	= sizeof(FFullSubUVPayload);
	DensityPayloadOffset = PayloadOffset + uiBytes;
	uiBytes  += sizeof(FLOAT);
	return uiBytes;
}
#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
