/*=============================================================================
	PhysXParticleSystem.cpp: PhysX Emitter Source.
	Copyright 2007-2008 AGEIA Technologies.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif

#include "PhysXParticleSystem.h"
#include "PhysXVerticalEmitter.h"
#include "PhysXParticleSetMesh.h"

IMPLEMENT_CLASS(UPhysXParticleSystem)

void UPhysXParticleSystem::FinishDestroy()
{
	RemovedFromScene();

#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	if(GWorld && GWorld->RBPhysScene)
	{
		GWorld->RBPhysScene->PhysXEmitterManager->RemoveParticleSystem(this);
		if (PSys)
		{
			UBOOL Disconnected = SyncDisconnect();
			check(Disconnected);
		}
	}
#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	Super::FinishDestroy();
}

/**
 * Creates the native wrapper class for PhysX Fluids which will be created in the scene returned by GetScene
 */

UBOOL UPhysXParticleSystem::SyncConnect()
{
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	check(!PSys);

	// if the the creation failed once, prevent it from creating it again 
	if(bSyncFailed) 
		return FALSE;

	PSys = new FPhysXParticleSystem(*this);
	check(PSys);

	// check whether the PhysX Fluid has been successfully created
	if(!PSys->IsSyncedToSdk())
	{
		GWarn->Logf(NAME_Warning, TEXT("FPhysXParticleSystem %s failed to be created in PhysX Sdk!"), *GetName());
		delete PSys;
		PSys = NULL;
		bSyncFailed = TRUE;
		return FALSE;
	}
	
	// Don't destroy as long as PhysX Fluid exists in the PhysX scene
	bDestroy = FALSE;
	check(PSys);

#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

	return TRUE;
}

/**
 * SyncDisconnect is called every frame to check whether the particle system has been removed from the PhysX Scene.
 * If it has, do a clean-up.
 */

UBOOL UPhysXParticleSystem::SyncDisconnect()
{
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	// Don't do the clean up until, the PhysX Fluid has been removed from the PhysXScene
	if(!bDestroy)
		return FALSE;

	check(PSys);
	delete PSys;
	PSys = NULL;
	bSyncFailed = FALSE;
	bDestroy = FALSE;

	if(CascadeScene)
	{
		DestroyRBPhysScene(CascadeScene);
		CascadeScene = NULL;
	}
#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	return TRUE;
}

/**
Makes sure that a FPhysXParticleSystem is created and linked to an 
appropriate scene, and it's vertical emitter manager. Returns FALSE 
on failure, and TRUE on success. 
To which scene the FPhysXParticleSystem is connected, depends on running 
Cascade or Game. If an existing connection needs to be broken down, due 
to change from Cascade to Game or vice versa, FALSE is returned.
*/
UBOOL UPhysXParticleSystem::TryConnect()
{
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	// Get the appropriate scene for this particle system. Create the cascade scene if necessary.
	// Scene may have switched here
	FRBPhysScene* Scene = GetScene();

	if(!Scene)
	{
		return FALSE;
	}

	if(!PSys)
	{
		Scene->PhysXEmitterManager->AddParticleSystem(this);
		if(CascadeScene)
		{
			//Sync with PhysX objects
			Scene->PhysXEmitterManager->PreSyncPhysXData();
			Scene->PhysXEmitterManager->PostSyncPhysXData();
		}
		return FALSE;
	}

    // if there is a switch in scenes, remove it from the previous scene
	if(CascadeScene && bIsInGame)
	{
		RemovedFromScene();
		return FALSE;
	}
#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	return TRUE;
}

FRBPhysScene* UPhysXParticleSystem::GetScene()
{
	FRBPhysScene* Scene = NULL;
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	if(GIsEditor == TRUE && GIsGame == FALSE && !bIsInGame) //cascade mode
	{
		if(!CascadeScene)
		{
			AWorldInfo *pInfo = (AWorldInfo*)AWorldInfo::StaticClass()->GetDefaultObject();
			check(pInfo);
			FVector Gravity(0, 0, pInfo->DefaultGravityZ * pInfo->RBPhysicsGravityScaling);
			CascadeScene = CreateRBPhysScene(Gravity);
			check(CascadeScene);

			NxPlaneShapeDesc PlaneShape;
			NxActorDesc Actor;
			PlaneShape.normal.set(0.0f, 0.0f, 1.0f);
			PlaneShape.d = -5.0f;
			FRBCollisionChannelContainer CollidesWith(0);
			CollidesWith.SetChannel(RBCC_Default, TRUE);
			PlaneShape.groupsMask = CreateGroupsMask(RBCC_Default, &CollidesWith);
			Actor.shapes.pushBack(&PlaneShape);
			NxScene* SceneNx = CascadeScene->GetNovodexPrimaryScene();
			check(SceneNx);
			SceneNx->createActor(Actor);
		}
		Scene = CascadeScene;
	
	}
	else if(GIsGame)
	{
		check(GWorld && GWorld->RBPhysScene);
		Scene = GWorld->RBPhysScene;

		//Mark scene for PIE over Cascade precedence
		bIsInGame = TRUE;
	}	
#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

	return Scene;
}
/**
 * Do any particle updates that need to be done after simulate.
 * Resyncs the rendering particles to the PhysX particles.
 */
void UPhysXParticleSystem::PostSyncPhysXData()
{
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	PSys->PostSyncPhysXData();
#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
}
/**
 * Do any particle updates that need to be done before simulate.
 * Informs PhysX of any particle spawning or deletion.
 */
void UPhysXParticleSystem::PreSyncPhysXData()
{
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	PSys->PreSyncPhysXData();
#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
}

/**
 * Updates the rendering data from the PhysX data.
 */
void UPhysXParticleSystem::Tick(FLOAT DeltaTime)
{
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	check(PSys);
	PSys->Tick(DeltaTime);
#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
}

static INT GetPacketSizeMultiplier(BYTE PacketSizeMultiplierEnum)
{
	INT PacketSizeMultiplierInt = 0;
	switch (PacketSizeMultiplierEnum)
	{
	case EPSM_4:
		PacketSizeMultiplierInt = 4;
		break;
	case EPSM_8:
		PacketSizeMultiplierInt = 8;
		break;
	case EPSM_16:
		PacketSizeMultiplierInt = 16;
		break;
	case EPSM_32:
		PacketSizeMultiplierInt = 32;
		break;
	case EPSM_64:
		PacketSizeMultiplierInt = 64;
		break;
	case EPSM_128:
		PacketSizeMultiplierInt = 128;
		break;
	}
	return PacketSizeMultiplierInt;
}

static void SetPacketSizeMultiplier(BYTE &PacketSizeMultiplierEnum, INT PacketSizeMultiplierInt)
{
	switch (PacketSizeMultiplierInt)
	{
	case 4:
		PacketSizeMultiplierEnum = EPSM_4;
		break;
	case 8:
		PacketSizeMultiplierEnum = EPSM_8;
		break;
	case 16:
		PacketSizeMultiplierEnum = EPSM_16;
		break;
	case 32:
		PacketSizeMultiplierEnum = EPSM_32;
		break;
	case 64:
		PacketSizeMultiplierEnum = EPSM_64;
		break;
	case 128:
		PacketSizeMultiplierEnum = EPSM_128;
		break;
	default:
		// shouldn't ever happen, just leave it unassigned
		debugf(NAME_DevPhysics, TEXT("Invalid packet size multiplier %d!"), PacketSizeMultiplierInt);
		break;
	}
}


void UPhysXParticleSystem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		// MaxParticles and ParticleSpawnReserve are interdependent
		// MaxParticles+ParticleSpawnReserve <= 65535
		// ParticleSpawnReserve <= MaxParticles
		if (PropertyThatChanged->GetFName() == FName(TEXT("MaxParticles")))
		{
			// PhysX has a total particle buffer size limit of 65535 particles.
			// The buffers hold MaxParticles+ParticleSpawnReserve particles.
			MaxParticles = Clamp(MaxParticles, 1, 65535 - ParticleSpawnReserve);
		}
		else if (PropertyThatChanged->GetFName() == FName(TEXT("ParticleSpawnReserve")))
		{
			// PhysX has a total particle buffer size limit of 65535 particles.
			// The buffers hold MaxParticles+ParticleSpawnReserve particles.
			ParticleSpawnReserve = Clamp(ParticleSpawnReserve, 0, 65535 - MaxParticles);
			ParticleSpawnReserve = Clamp(ParticleSpawnReserve, 0, MaxParticles);
		}

		// CollisionDistance, PacketSizeMultiplier, KernelRadiusMultiplier, MaxMotionDistance, 
		// and RestParticleDistance are interdependent
		// CollisionDistance <= PacketSizeMultiplier*KernelRadiusMultiplier*RestParticleDistance
		// MaxMotionDistance <= PacketSizeMultiplier*KernelRadiusMultiplier*RestParticleDistance
		else if (PropertyThatChanged->GetFName() == FName(TEXT("CollisionDistance")))
		{	
			// CollisionDistance <= PacketSizeMultiplier*KernelRadiusMultiplier*RestParticleDistance
			CollisionDistance = Clamp(CollisionDistance, (FLOAT)KINDA_SMALL_NUMBER, (FLOAT)GetPacketSizeMultiplier(PacketSizeMultiplier)*KernelRadiusMultiplier*RestParticleDistance);
		}
		else if (PropertyThatChanged->GetFName() == FName(TEXT("MaxMotionDistance")))
		{
			// MaxMotionDistance <= PacketSizeMultiplier*KernelRadiusMultiplier*RestParticleDistance
			MaxMotionDistance = Clamp(MaxMotionDistance, 0.0f, (FLOAT)GetPacketSizeMultiplier(PacketSizeMultiplier)*KernelRadiusMultiplier*RestParticleDistance);
		}
		else if (PropertyThatChanged->GetFName() == FName(TEXT("RestParticleDistance")))
		{
			// MaxMotionDistance <= PacketSizeMultiplier*KernelRadiusMultiplier*RestParticleDistance
			RestParticleDistance = Clamp(RestParticleDistance, MaxMotionDistance / ((FLOAT)GetPacketSizeMultiplier(PacketSizeMultiplier)*KernelRadiusMultiplier), BIG_NUMBER);
			RestParticleDistance = Clamp(RestParticleDistance, CollisionDistance / ((FLOAT)GetPacketSizeMultiplier(PacketSizeMultiplier)*KernelRadiusMultiplier), BIG_NUMBER);
		}
		else if (PropertyThatChanged->GetFName() == FName(TEXT("PacketSizeMultiplier")))
		{
			// CollisionDistance <= PacketSizeMultiplier*KernelRadiusMultiplier*RestParticleDistance
			// MaxMotionDistance <= PacketSizeMultiplier*KernelRadiusMultiplier*RestParticleDistance
			INT PacketSizeMultiplierInt = GetPacketSizeMultiplier(PacketSizeMultiplier);
			PacketSizeMultiplierInt = Clamp(PacketSizeMultiplierInt, (INT)(MaxMotionDistance/(KernelRadiusMultiplier*RestParticleDistance)), 128);
			PacketSizeMultiplierInt = Clamp(PacketSizeMultiplierInt, (INT)(CollisionDistance/(KernelRadiusMultiplier*RestParticleDistance)), 128);
			if (PacketSizeMultiplierInt != GetPacketSizeMultiplier(PacketSizeMultiplier))
			{
				// Round PacketSizeMultiplierInt up to the nearest power of two
				PacketSizeMultiplierInt = appRoundUpToPowerOfTwo(PacketSizeMultiplierInt);
#if !FINAL_RELEASE
				ensure((PacketSizeMultiplierInt >= 4) && (PacketSizeMultiplierInt <= 128));
#endif
				SetPacketSizeMultiplier(PacketSizeMultiplier, PacketSizeMultiplierInt);
			}
		}
		else if (PropertyThatChanged->GetFName() == FName(TEXT("KernelRadiusMultiplier")))
		{
			// CollisionDistance <= PacketSizeMultiplier*KernelRadiusMultiplier*RestParticleDistance
			// MaxMotionDistance <= PacketSizeMultiplier*KernelRadiusMultiplier*RestParticleDistance
			// KernelRadiusMultiplier >= 1.0
			KernelRadiusMultiplier = Clamp(KernelRadiusMultiplier, 1.0f, BIG_NUMBER);
			KernelRadiusMultiplier = Clamp(KernelRadiusMultiplier, MaxMotionDistance / ((FLOAT)GetPacketSizeMultiplier(PacketSizeMultiplier)*RestParticleDistance), BIG_NUMBER);
			KernelRadiusMultiplier = Clamp(KernelRadiusMultiplier, CollisionDistance / ((FLOAT)GetPacketSizeMultiplier(PacketSizeMultiplier)*RestParticleDistance), BIG_NUMBER);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UPhysXParticleSystem::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	FlushRenderingCommands();
	RemovedFromScene();
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	if(PSys && PSys->PhysScene)
	{
		PSys->PhysScene->PhysXEmitterManager->RemoveParticleSystem(this);
	}
#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
}

/**
 * Remove the PhysX fluid from the PhysX scene. 
 */
void UPhysXParticleSystem::RemovedFromScene()
{
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	if(!CascadeScene)
	{
		bIsInGame = FALSE;
	}

	bSyncFailed = FALSE;
	
	if(!PSys)
		return;

	bDestroy = TRUE;

	PSys->RemovedFromScene();
	
	if(CascadeScene)
	{
		if(CascadeScene->CompartmentsRunning)
			WaitRBPhysScene(CascadeScene);

		SyncDisconnect();
		check(!CascadeScene);
	}
#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
}

/**
 * Remove a spawn instance from the particle system.
 */
void UPhysXParticleSystem::RemoveSpawnInstance(struct FParticleEmitterInstance* SpawnInstance)
{
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	if(!PSys)
		return;

	check(SpawnInstance);

	PSys->RemoveSpawnInstance(SpawnInstance);
	if(PSys->GetSpawnInstanceRefsNum() == 0)
		RemovedFromScene();
#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
}

////////////////////////////////////////////////////////////////////////////////////
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

FPhysXParticleSystem::FPhysXParticleSystem(UPhysXParticleSystem& InParams):
	Fluid(NULL),
	ParticlesSdk(NULL),
	ParticlesEx(NULL),
	ParticleForceUpdates(NULL),
	ParticleFlagUpdates(NULL),
	ParticleContactsSdk(NULL),
	ParticleCreationIDsSdk(NULL),
	ParticleDeletionIDsSdk(NULL),
	PacketsSdk(NULL),
	Params(InParams),
	Packets(NULL),
	Distributer(NULL)
{
	FRBPhysScene* InRBPhysScene = Params.GetScene();
	check(InRBPhysScene);

	PhysScene = InRBPhysScene;


	NxScene* Scene = PhysScene->GetNovodexPrimaryScene();
	NxCompartment* Compartment = PhysScene->GetNovodexFluidCompartment(); 
	
	if(!Scene || !Scene->isWritable() || !Compartment)
	{
		return;
	}

	INT ParticleBufferSize = Params.MaxParticles + Params.ParticleSpawnReserve;
	ParticlesSdk = (PhysXParticle*)appMalloc(ParticleBufferSize * sizeof(PhysXParticle));
	check(ParticlesSdk);
	ParticlesEx = (PhysXParticleEx*)appMalloc(ParticleBufferSize * sizeof(PhysXParticleEx));
	check(ParticlesEx);
	ParticleContactsSdk = (NxVec3*)appMalloc(ParticleBufferSize * sizeof(NxVec3));
	check(ParticleContactsSdk);
	ParticleCreationIDsSdk = (UINT*)appMalloc(ParticleBufferSize * sizeof(UINT));
	check(ParticleCreationIDsSdk);
	ParticleDeletionIDsSdk = (UINT*)appMalloc(ParticleBufferSize * sizeof(UINT));
	check(ParticleDeletionIDsSdk);

	UINT MaxPackets = appFloor(GNovodexSDK->getParameter(NX_CONSTANT_FLUID_MAX_PACKETS));
	PacketsSdk = (NxFluidPacket*)appMalloc(MaxPackets * sizeof(NxFluidPacket));
	check(PacketsSdk);
	Packets = (Packet*)appMalloc(MaxPackets * sizeof(Packet));
	check(Packets);

	//ParticleForceUpdates = (PhysXParticleForceUpdate*)appMalloc(ParticleBufferSize * sizeof(PhysXParticleForceUpdate));
	//check(ParticleForceUpdates);
	ParticleForceUpdates = NULL;
	ParticleFlagUpdates = (PhysXParticleFlagUpdate*)appMalloc(ParticleBufferSize * sizeof(PhysXParticleFlagUpdate));
	check(ParticleFlagUpdates);

	InitState();

	NxFluidDesc FluidDesc = CreateFluidDesc(Compartment);
	if(!FluidDesc.isValid())
		return;

	Fluid = Scene->createFluid(FluidDesc);
	check(Fluid);

	Distributer = new FPhysXDistribution();
}

/**
 * Initializes/Reset the state of particle system
 */

void FPhysXParticleSystem::InitState()
{
	NumParticlesSdk = 0;
	NumParticlesCreatedSdk = 0;
	//This is used to mark, that fetch results wasn't executed.
	NumParticlesDeletedSdk = PHYSX_NUM_DEL_NOT_WRITTEN;
	NumPacketsSdk = 0;
	NumPackets = 0;

	NumParticleForceUpdates = 0;
	NumParticleFlagUpdates = 0;

	VerticalPacketLimit = appFloor(GNovodexSDK->getParameter(NX_CONSTANT_FLUID_MAX_PACKETS));
	VerticalPacketRadiusSq = FLT_MAX;

	DebugNumSDKDel = 0;

	bSdkDataSynced = FALSE;
	bProcessSimulationStep = FALSE;
	bLocationSortDone = FALSE;
	bCylindricalPacketCulling = GWorld->GetWorldInfo()->VerticalProperties.Emitters.bApplyCylindricalPacketCulling;
}

/**
 * Checks if the fluid has been created
 */

UBOOL FPhysXParticleSystem::IsSyncedToSdk()
{
	return Fluid != NULL;
}

FPhysXParticleSystem::~FPhysXParticleSystem()
{
	check(ParticleSetRefs.Num() == 0);
	check(SpawnInstanceRefs.Num() == 0);
	
	if(Fluid)
	{
		NxScene *Scene = &Fluid->getScene();
		check(Scene);
		Scene->releaseFluid(*Fluid);
		Fluid = NULL;
	}
	if(ParticlesSdk)
	{
		appFree(ParticlesSdk);
		ParticlesSdk = NULL;
	}

	if(ParticlesEx)
	{
		appFree(ParticlesEx);
		ParticlesEx = NULL;
	}

	if(ParticleContactsSdk)
	{
		appFree(ParticleContactsSdk);
		ParticleContactsSdk = NULL;
	}

	if(ParticleCreationIDsSdk)
	{
		appFree(ParticleCreationIDsSdk);
		ParticleCreationIDsSdk = NULL;
	}

	if(ParticleDeletionIDsSdk)
	{
		appFree(ParticleDeletionIDsSdk);
		ParticleDeletionIDsSdk = NULL;
	}

	if(PacketsSdk)
	{
		appFree(PacketsSdk);
		PacketsSdk = NULL;
	}

	if(Packets)
	{
		appFree(Packets);
		Packets = NULL;
	}

	if(ParticleForceUpdates)
	{
		appFree(ParticleForceUpdates);
		ParticleForceUpdates = NULL;
	}

	if(ParticleFlagUpdates)
	{
		appFree(ParticleFlagUpdates);
		ParticleFlagUpdates = NULL;
	}

	if(Distributer)
	{
		delete Distributer;
	}
}

/**
 * Removes all the particle sets, spawn instances and fluid particles
 */

void FPhysXParticleSystem::RemovedFromScene()
{
	RemoveAllParticleSets();
	RemoveAllSpawnInstances();

	//Init Sdk state variables.
	if (Fluid)
	{
		Fluid->removeAllParticles();
	}
}

/**
 *  Update PhysX Particle Forces
 */

void FPhysXParticleSystem::SyncParticleForceUpdates()
{
	//Remove unused and obsolete updates	
	for(INT i=NumParticleForceUpdates-1; i>=0;i--)
	{
		PhysXParticleForceUpdate& ParticleForceUpdate = ParticleForceUpdates[i];

		UBOOL Unused = (ParticleForceUpdate.Vec.isZero());
		if(Unused || ParticlesEx[ParticleForceUpdate.Id].Index == 0xffff)
		{
			ParticleForceUpdate = ParticleForceUpdates[NumParticleForceUpdates-1];
			NumParticleForceUpdates--;
		}
	}

	if (NumParticleForceUpdates > 0)
	{
		//debugf( TEXT("DEBUG_VE SyncParticleUpdates() NumDels %d"), NumDels);
		NxParticleUpdateData UpdateData;
		UpdateData.forceMode = NX_ACCELERATION;
		UpdateData.bufferId = &ParticleForceUpdates[0].Id;
		UpdateData.bufferForce = &ParticleForceUpdates[0].Vec.x;
		UpdateData.bufferIdByteStride = sizeof(PhysXParticleForceUpdate);
		UpdateData.bufferForceByteStride = sizeof(PhysXParticleForceUpdate);
		UpdateData.numUpdates = NumParticleForceUpdates;
		Fluid->updateParticles(UpdateData);
		NumParticleForceUpdates = 0;
	}
}

/**
 *  Update PhysX Particle Flags. Currently used to signal to PhysX that it should delete that particle.
 */

void FPhysXParticleSystem::SyncParticleFlagUpdates()
{
	if (NumParticleFlagUpdates == 0)
		return;

#if defined(_DEBUG)
	for(INT i=0; i<NumParticleFlagUpdates; i++)
	{
		const PhysXParticleFlagUpdate &ParticleUpdate = ParticleFlagUpdates[i];
		const PhysXParticleEx         &ParticleEx     = ParticlesEx[ParticleUpdate.Id];
		check(ParticleUpdate.Flags == NX_FP_DELETE);
		check(ParticleEx.PSet        == NULL);
		check(ParticleEx.Index       != 0xffff);
		check(ParticleEx.RenderIndex == 0xffff);
	}
#endif

	//This assumes, the flag updates are actually set to particle deletions
	check(DebugNumSDKDel == 0);
	DebugNumSDKDel = NumParticleFlagUpdates;
	
	NxParticleUpdateData UpdateData;
	UpdateData.bufferId = &ParticleFlagUpdates[0].Id;
	UpdateData.bufferFlag = &ParticleFlagUpdates[0].Flags;
	UpdateData.bufferIdByteStride = sizeof(PhysXParticleFlagUpdate);
	UpdateData.bufferFlagByteStride = sizeof(PhysXParticleFlagUpdate);
	UpdateData.numUpdates = NumParticleFlagUpdates;
	Fluid->updateParticles(UpdateData);
	NumParticleFlagUpdates = 0;
}

/**
 *  Checks whether there are pending SDK particle removals. If yes, the simulation didn't execute, usually because of too small timesteps.
 */

UBOOL FPhysXParticleSystem::SyncCheckSimulationStep()
{
	return !(DebugNumSDKDel > 0 && NumParticlesDeletedSdk == 0) && NumParticlesDeletedSdk != PHYSX_NUM_DEL_NOT_WRITTEN;
}

/**
 *  Update particle extensions, render instances and async particle updates,
 *  given the notification of deleted Ids from the SDK.
 */

void FPhysXParticleSystem::SyncProcessSDKParticleRemovals()
{
	if(!bProcessSimulationStep)
	{
		return;
	}
	// Number of Rendering Particles that are going to be deleted
	INT DebugNumSdkInternalDel = 0;

	for (INT i = 0; i < NumParticlesDeletedSdk; i++)
	{
		UINT Id = ParticleDeletionIDsSdk[i];
		PhysXParticleEx& ParticleEx = ParticlesEx[Id];
		FPhysXParticleSet* PSet = ParticleEx.PSet;

		//SDK side deletions!
		if (PSet != NULL)
		{
			INT FixId;
			INT RenderIndex = ParticleEx.RenderIndex;
			FixId = PSet->RemoveParticle(RenderIndex, false);
			ParticlesEx[FixId].RenderIndex = RenderIndex;
			check(ParticlesEx[FixId].PSet == PSet);
			ParticleEx.PSet = NULL;
			ParticleEx.RenderIndex = 0xffff;

			//PhysXParticleForceUpdate& ParticleForceUpdate = ParticleForceUpdates[ParticleEx.Index];
			//check(ParticleForceUpdate.Id == 0 || ParticleForceUpdate.Id == Id);
			//appMemset(&ParticleForceUpdate, 0, sizeof(PhysXParticleForceUpdate));
			DebugNumSdkInternalDel++;
		}
		else if(NumParticlesDeletedSdk > DebugNumSDKDel + DebugNumSdkInternalDel)
		{
			// in the scenario where the FIFO deletes the particle and UE3 submits the same
			// particle for deletion on the next frame we need to make sure we remove the
			// particle from the deletion list.
			for(INT j=0; j<NumParticleFlagUpdates; j++)
			{
				const PhysXParticleFlagUpdate &ParticleUpdate = ParticleFlagUpdates[j];
				if(ParticleUpdate.Id == Id)
				{
					ParticleFlagUpdates[j] = ParticleFlagUpdates[NumParticleFlagUpdates-1];
					NumParticleFlagUpdates--;
					DebugNumSdkInternalDel++;
					break;
				}
			}
		}
		ParticleEx.Index = 0xffff;
	}

	check(NumParticlesDeletedSdk == DebugNumSDKDel + DebugNumSdkInternalDel);
	DebugNumSDKDel = 0;

	INT NumEmitterParticles = GetNumParticles();
	check(NumEmitterParticles + NumParticleFlagUpdates == NumParticlesSdk);
	
	NumParticlesDeletedSdk = 0;
}

/**
 *  Updates the extension table. Clean up async updates 
 *  Updates the relation of rendering data to PhysX Data
 *  Update per packet mean particle age
 */

void FPhysXParticleSystem::SyncProcessParticles()
{
	if(!bProcessSimulationStep)
	{
		return;
	}

	FVector LODOrigin;
	FPlane LODPlane;
	UBOOL bApplyPacketDistanceLOD = GetLODOrigin(LODOrigin);
	UBOOL bApplyPacketClippingLOD = GetLODNearClippingPlane(LODPlane);
	UBOOL bApplyLOD = bApplyPacketDistanceLOD || bApplyPacketClippingLOD;

	NumPackets = NumPacketsSdk;
	for(INT i = 0; i < NumPacketsSdk; i++)
	{
		Packet& P = Packets[i];
		NxFluidPacket& SdkPacket = PacketsSdk[i];
		P.SdkPacket = &SdkPacket;
		
		//Particle loop
		P.MeanParticleAge = 0.0f;
		for( UINT j = 0; j < SdkPacket.numParticles; ++j )
		{
			UINT ParticleIndex = SdkPacket.firstParticleIndex + j;
			PhysXParticle& Particle = ParticlesSdk[ParticleIndex];
			INT Id = Particle.Id;
			PhysXParticleEx& ParticleEx = ParticlesEx[Id];

			FPhysXParticleSet* PSet = ParticleEx.PSet;
			if(PSet != NULL)
			{
				INT RenderIndex = ParticleEx.RenderIndex;
				PhysXRenderParticle* RenderParticle = PSet->GetRenderParticle(RenderIndex);
				RenderParticle->ParticleIndex = ParticleIndex;			
				P.MeanParticleAge += RenderParticle->RelativeTime;				
			}

			ParticleEx.Index = ParticleIndex;
		}
		P.MeanParticleAge /= SdkPacket.numParticles;
		if(bApplyLOD)
		{
			NxVec3 nxCenter;
			SdkPacket.aabb.getCenter(nxCenter);
			FVector Center = N2UPosition(nxCenter);
			P.DistanceSq = bApplyPacketDistanceLOD ? GetLODDistanceSq(LODOrigin, Center) : P.DistanceSq = 0.0f;
			P.IsInFront	= bApplyPacketClippingLOD ? LODPlane.PlaneDot(Center) < 0.0f : TRUE;
		}
		else
		{
			P.DistanceSq = 0.0f;
			P.IsInFront = TRUE;
		}
	}
}
/**
 * Executes SDK particle spawning. Notifies SDK of the particle that needs to be deleted by their particle IDs.
 */
void FPhysXParticleSystem::PreSyncPhysXData()
{
	for(INT i=0; i<ParticleSetRefs.Num(); i++)
		ParticleSetRefs(i)->SyncPhysXData();

	for(INT i=0; i<SpawnInstanceRefs.Num(); i++)
	{
		FParticleEmitterInstance* SpawnEmitter = SpawnInstanceRefs(i);
		if(SpawnEmitter->Type()->IsA(FParticleSpritePhysXEmitterInstance::StaticType))
		{
			FParticleSpritePhysXEmitterInstance* SpawnSpriteEmitter = (FParticleSpritePhysXEmitterInstance*)SpawnEmitter;
			SpawnSpriteEmitter->SpawnSyncPhysX();
		}
		else if(SpawnEmitter->Type()->IsA(FParticleMeshPhysXEmitterInstance::StaticType))
		{
			FParticleMeshPhysXEmitterInstance* SpawnMeshEmitter = (FParticleMeshPhysXEmitterInstance*)SpawnEmitter;
			SpawnMeshEmitter->SpawnSyncPhysX();
		}
	}
	NumParticlesDeletedSdk = PHYSX_NUM_DEL_NOT_WRITTEN;
	if(bProcessSimulationStep)
	{
		//NumParticleForceUpdates = NumParticlesSdk;
		//appMemset(ParticleForceUpdates, 0, NumParticleForceUpdates * sizeof(PhysXParticleForceUpdate));
		SyncParticleFlagUpdates();
		SyncParticleForceUpdates();

		check(GetNumParticles() + DebugNumSDKDel + NumParticleFlagUpdates == NumParticlesSdk);
		bLocationSortDone = FALSE;

		
		if( (Fluid != 0) && (NumParticlesSdk == 0) )
		{
			// Delete empty NxFluids to save memory
			NxScene *Scene = &Fluid->getScene();
			check(Scene);
			Scene->releaseFluid(*Fluid);
			Fluid = NULL;
		}
	}
}

/**
 * Notify the rendering particles of any removed PhysX Particle. 
 * Reassigns the Rendering Index and the Index of the Rendering particles to refer to the correct SDK particle.
 */
void FPhysXParticleSystem::PostSyncPhysXData()
{
	// check whether a simulation step has occured this frame
	bProcessSimulationStep = SyncCheckSimulationStep();
	SyncProcessSDKParticleRemovals();
	SyncProcessParticles();
}


void FPhysXParticleSystem::AssignReduction(INT InNumReduction)
{
	check(InNumReduction > 0);

	TArray<FPhysXDistribution::Input>& InputBuffer = Distributer->GetInputBuffer(ParticleSetRefs.Num());
	for(INT i=0; i<ParticleSetRefs.Num(); i++)
	{
		FPhysXParticleSet& PSet = *ParticleSetRefs(i);
		InputBuffer.AddItem(FPhysXDistribution::Input(PSet.GetNumRenderParticles(), PSet.GetWeightForFifo()));
	}
	
	const TArray<FPhysXDistribution::Result>& ResultBuffer = Distributer->GetResult(InNumReduction);

	for(INT i=0; i<ResultBuffer.Num(); i++)
	{
		FPhysXParticleSet& PSet = *ParticleSetRefs(i);
		const FPhysXDistribution::Result& R = ResultBuffer(i);
		PSet.SyncParticleReduction(R.Piece);
	}
}

void FPhysXParticleSystem::SyncParticleReduction(INT InNumReduction)
{
	if(!bProcessSimulationStep)
	{
		return;
	}

	if(InNumReduction > 0)
		SyncRemoveHidden(InNumReduction);

	if(InNumReduction > 0)
	{
		AssignReduction(InNumReduction);
		SyncParticleFlagUpdates();
	}
	//plotf( TEXT("DEBUG_VE_PLOT numParticles %d"), NumParticles);
}

void FPhysXParticleSystem::SyncRemoveHidden(INT& InNumReduction)
{
	check(bProcessSimulationStep);
	appQsort( Packets, NumPackets, sizeof(Packet), (QSORT_COMPARE)ComparePacketLocation );
	bLocationSortDone = TRUE;

	INT i=0; 
	while(i < NumPackets && InNumReduction > 0)
	{
		Packet& P = Packets[i];
		NxFluidPacket& SdkPacket = *P.SdkPacket;
		
		if(!P.IsInFront && SdkPacket.packetID != 0xffff)
		{
			for( UINT j = 0; j < SdkPacket.numParticles && InNumReduction > 0; ++j )
			{
				UINT ParticleIndex = SdkPacket.firstParticleIndex + j;
				PhysXParticle& Particle = ParticlesSdk[ParticleIndex];
				PhysXParticleEx& ParticleEx = ParticlesEx[Particle.Id];

				if(ParticleEx.PSet == NULL)
					continue;

				ParticleEx.PSet->SyncRemoveParticle(Particle.Id);
				
				InNumReduction--;
			}	
		}
		i++;
	}
}

/**
 * Notify the rendering particles of any removed PhysX Particle. 
 * Reassigns the Rendering Index and the Index of the Rendering particles to refer to the correct SDK particle.
 */
void FPhysXParticleSystem::Tick(FLOAT DeltaTime)
{
	for(INT i=0; i<ParticleSetRefs.Num(); i++)
	{
		ParticleSetRefs(i)->AsyncUpdate(DeltaTime, bProcessSimulationStep);
	}
	
	if(bProcessSimulationStep)
	{
		AsyncPacketCulling();
	}
}

/**
 * Removes all particle rendering data 
 */
void FPhysXParticleSystem::RemoveAllParticleSets()
{
	while(ParticleSetRefs.Num())
	{
		RemoveParticleSet(ParticleSetRefs.Last());
	}
}

/**
 * Adds a new set of rendering data
 */
void FPhysXParticleSystem::AddParticleSet(FPhysXParticleSet* InParticleSet)
{
	if(Params.bDestroy)
		return;

	ParticleSetRefs.AddUniqueItem(InParticleSet);
}

/**
 * Removes a Particle Set
 */
void FPhysXParticleSystem::RemoveParticleSet(FPhysXParticleSet* InParticleSet)
{
	InParticleSet->RemoveAllParticles();
	
	INT OldNum = ParticleSetRefs.Num();
	ParticleSetRefs.RemoveItem(InParticleSet);
	
	if(ParticleSetRefs.Num() == OldNum)
	{
		return;
	}

	if(!ParticlesEx)
		return;

	INT ParticleBufferSize = Params.MaxParticles + Params.ParticleSpawnReserve;
	for (INT i = 0; i < ParticleBufferSize; i++)
	{
		if (ParticlesEx[i].PSet == InParticleSet)
			ParticlesEx[i].PSet = NULL;
	}
}

/**
 * Removes all spawn instances
 */

void FPhysXParticleSystem::RemoveAllSpawnInstances()
{
	while(SpawnInstanceRefs.Num())
	{
		RemoveSpawnInstance(SpawnInstanceRefs.Last());
	}
}

/**
 * Add a spawn instance
 */
void FPhysXParticleSystem::AddSpawnInstance(FParticleEmitterInstance* InSpawnInstance)
{
	if(Params.bDestroy)
		return;
	//INT OldNum = SpawnInstanceRefs.Num();
	SpawnInstanceRefs.AddUniqueItem(InSpawnInstance);
	
	//if(SpawnInstanceRefs.Num() > OldNum)
}

/**
 * Remove a spawn instance
 */
void FPhysXParticleSystem::RemoveSpawnInstance(FParticleEmitterInstance* InSpawnInstance)
{
	INT NumRemoved = SpawnInstanceRefs.RemoveItem(InSpawnInstance);

	if(InSpawnInstance->Type()->IsA(FParticleSpritePhysXEmitterInstance::StaticType))
	{
		FParticleSpritePhysXEmitterInstance* SpawnSpriteEmitter = (FParticleSpritePhysXEmitterInstance*)InSpawnInstance;
		SpawnSpriteEmitter->RemoveParticles();
	}
	else if(InSpawnInstance->Type()->IsA(FParticleMeshPhysXEmitterInstance::StaticType))
	{
		FParticleMeshPhysXEmitterInstance* SpawnMeshEmitter = (FParticleMeshPhysXEmitterInstance*)InSpawnInstance;
		SpawnMeshEmitter->RemoveParticles();
	}
}

/**
 * Adds particles to the PhysX Particle System
 */

INT FPhysXParticleSystem::AddParticles(NxParticleData& InParticleBuffer, FPhysXParticleSet* InParticleSet)
{
	if( Fluid == NULL )
	{
		NxScene* Scene = PhysScene->GetNovodexPrimaryScene();
		NxCompartment* Compartment = PhysScene->GetNovodexFluidCompartment(); 
		NxFluidDesc FluidDesc = CreateFluidDesc(Compartment);
		if(!FluidDesc.isValid())
		{
			return 0;
		}
		InitState();
		Fluid = Scene->createFluid(FluidDesc);
	}
	check(Fluid != NULL);
	if( Fluid == NULL )
	{
		return 0;
	}

	AddParticleSet(InParticleSet);
	UINT RIOffset =  InParticleSet->GetNumRenderParticles();

	UINT NumOld = NumParticlesSdk;
	Fluid->addParticles(InParticleBuffer);
	check(NumOld + NumParticlesCreatedSdk == NumParticlesSdk);
	for (INT i = 0; i < NumParticlesCreatedSdk; i++)
	{
		UINT Id = ParticleCreationIDsSdk[i];
		PhysXParticleEx& ParticleEx = ParticlesEx[Id];
		ParticleEx.PSet = InParticleSet;
		ParticleEx.RenderIndex = RIOffset + i;
		ParticleEx.Index = NumOld + i;
	}
	return NumParticlesCreatedSdk;
}

/**
 * Returns the gravity of the scene
 */

void FPhysXParticleSystem::GetGravity(NxVec3& gravity)
{
	PhysScene->GetNovodexPrimaryScene()->getGravity(gravity);
}

/**
 * Initializes the fluid descriptor 
 */

NxFluidDesc FPhysXParticleSystem::CreateFluidDesc(NxCompartment* Compartment)
{
	NxFluidDesc fluidDesc;
	fluidDesc.setToDefault();
	fluidDesc.maxParticles = Params.MaxParticles + Params.ParticleSpawnReserve;
	fluidDesc.numReserveParticles = Params.ParticleSpawnReserve;
	fluidDesc.restParticlesPerMeter = 1.0f/(Params.RestParticleDistance*U2PScale);
	fluidDesc.restDensity = Params.RestDensity;
	fluidDesc.kernelRadiusMultiplier = Params.KernelRadiusMultiplier;
	fluidDesc.motionLimitMultiplier = Params.MaxMotionDistance/Params.RestParticleDistance;
	fluidDesc.collisionDistanceMultiplier = Params.CollisionDistance/Params.RestParticleDistance;
	fluidDesc.stiffness = Params.Stiffness;
	fluidDesc.viscosity = Params.Viscosity;
	fluidDesc.damping = Params.Damping;
	fluidDesc.restitutionForStaticShapes  = Params.RestitutionWithStaticShapes;
	fluidDesc.dynamicFrictionForStaticShapes = Params.FrictionWithStaticShapes;
	fluidDesc.staticFrictionForStaticShapes = Params.StaticFrictionWithStaticShapes;
	fluidDesc.restitutionForDynamicShapes = Params.RestitutionWithDynamicShapes;
	fluidDesc.dynamicFrictionForDynamicShapes = Params.FrictionWithDynamicShapes;
	fluidDesc.staticFrictionForDynamicShapes = Params.StaticFrictionWithDynamicShapes;
	fluidDesc.collisionResponseCoefficient = Params.CollisionResponseCoefficient;
	fluidDesc.externalAcceleration = U2NPosition(Params.ExternalAcceleration);
	fluidDesc.packetSizeMultiplier = GetPacketSizeMultiplier(Params.PacketSizeMultiplier);
	switch (Params.SimulationMethod)
	{
	case ESM_SPH:
		fluidDesc.simulationMethod = NX_F_SPH;
		break;
	case ESM_NO_PARTICLE_INTERACTION:
		fluidDesc.simulationMethod = NX_F_NO_PARTICLE_INTERACTION;
		break;
	case ESM_MIXED_MODE:
		fluidDesc.simulationMethod = NX_F_MIXED_MODE;
		break;
	}
	if (Params.bStaticCollision)
		fluidDesc.collisionMethod |= NX_F_STATIC;
	else
		fluidDesc.collisionMethod &= ~NX_F_STATIC;
	if (Params.bDynamicCollision)
		fluidDesc.collisionMethod |= NX_F_DYNAMIC;
	else
		fluidDesc.collisionMethod &= ~NX_F_DYNAMIC;
	fluidDesc.compartment = Compartment;
	fluidDesc.flags = NX_FF_VISUALIZATION | NX_FF_ENABLED;
	if (Params.bTwoWayCollision)
		fluidDesc.flags |= NX_FF_COLLISION_TWOWAY;
	else
		fluidDesc.flags &= ~NX_FF_COLLISION_TWOWAY;
	if (fluidDesc.numReserveParticles > 0)
		fluidDesc.flags |= NX_FF_PRIORITY_MODE;
	else
		fluidDesc.flags &= ~NX_FF_PRIORITY_MODE;
	if(IsPhysXHardwarePresent() && Compartment->getDeviceCode() != NX_DC_CPU)
		fluidDesc.flags |= NX_FF_HARDWARE;
	else
		fluidDesc.flags &= ~NX_FF_HARDWARE;
	if (Params.bDisableGravity)
		fluidDesc.flags |= NX_FF_DISABLE_GRAVITY;
	else
		fluidDesc.flags &= ~NX_FF_DISABLE_GRAVITY;

	// Use the user input to create collision group mask
	fluidDesc.groupsMask = CreateGroupsMask(Params.RBChannel,&Params.RBCollideWithChannels);

	fluidDesc.collisionGroup = UNX_GROUP_DEFAULT;

	NxParticleData ParticlesWriteData;
	check(NumParticlesSdk == 0);	// if this check fails, we need to implement bufferLife too
	ParticlesWriteData.numParticlesPtr = (NxU32*)&NumParticlesSdk;
	ParticlesWriteData.bufferPos = &ParticlesSdk[0].Pos.x;
	ParticlesWriteData.bufferVel = &ParticlesSdk[0].Vel.x;

	ParticlesWriteData.bufferDensity = (NxF32*)&ParticlesSdk[0].Density;

	ParticlesWriteData.bufferId = (NxU32*)&ParticlesSdk[0].Id;
	ParticlesWriteData.bufferCollisionNormal = (NxF32*)&ParticleContactsSdk[0].x;
	ParticlesWriteData.bufferPosByteStride = sizeof(PhysXParticle);
	ParticlesWriteData.bufferVelByteStride = sizeof(PhysXParticle);
	
	ParticlesWriteData.bufferDensityByteStride = sizeof(PhysXParticle);

	ParticlesWriteData.bufferIdByteStride = sizeof(PhysXParticle);
	ParticlesWriteData.bufferCollisionNormalByteStride = sizeof(NxVec3);	
	fluidDesc.particlesWriteData = ParticlesWriteData;
	
	fluidDesc.particleCreationIdWriteData.numIdsPtr = (NxU32*)&NumParticlesCreatedSdk;
	fluidDesc.particleCreationIdWriteData.bufferId = (NxU32*)ParticleCreationIDsSdk;
	fluidDesc.particleCreationIdWriteData.bufferIdByteStride = sizeof(UINT);
	
	fluidDesc.particleDeletionIdWriteData.numIdsPtr = (NxU32*)&NumParticlesDeletedSdk;
	fluidDesc.particleDeletionIdWriteData.bufferId = (NxU32*)ParticleDeletionIDsSdk;
	fluidDesc.particleDeletionIdWriteData.bufferIdByteStride = sizeof(UINT);

	fluidDesc.fluidPacketData.bufferFluidPackets = PacketsSdk;
	fluidDesc.fluidPacketData.numFluidPacketsPtr = (NxU32*)&NumPacketsSdk;

	return fluidDesc;
}

FLOAT FPhysXParticleSystem::GetWeightForFifo()
{
	FLOAT TotalLodWeight = 0.0f;
	for(INT i=0; i<ParticleSetRefs.Num(); i++)
	{
		FPhysXParticleSet& PSet = *ParticleSetRefs(i);
		TotalLodWeight += PSet.GetWeightForFifo();
	}
	return TotalLodWeight;
}

FLOAT FPhysXParticleSystem::GetWeightForSpawnLod()
{
	FLOAT TotalLodWeight = 0.0f;
	for(INT i=0; i<ParticleSetRefs.Num(); i++)
	{
		FPhysXParticleSet& PSet = *ParticleSetRefs(i);
		TotalLodWeight += PSet.GetWeightForSpawnLod();
	}
	return TotalLodWeight;
}

INT FPhysXParticleSystem::GetSpawnVolumeEstimate()
{
	INT SpawnVolumeSum = 0;
	for(INT i=0; i<SpawnInstanceRefs.Num(); i++)
	{
		FParticleEmitterInstance* Emitter = SpawnInstanceRefs(i);
		
		if(Emitter->Type()->IsA(FParticleSpritePhysXEmitterInstance::StaticType))
		{
			FParticleSpritePhysXEmitterInstance* SpawnSpriteEmitter = (FParticleSpritePhysXEmitterInstance*)Emitter;
			SpawnVolumeSum += SpawnSpriteEmitter->GetSpawnVolumeEstimate();
		}
		else if(Emitter->Type()->IsA(FParticleMeshPhysXEmitterInstance::StaticType))
		{
			FParticleMeshPhysXEmitterInstance* SpawnMeshEmitter = (FParticleMeshPhysXEmitterInstance*)Emitter;
			SpawnVolumeSum += SpawnMeshEmitter->GetSpawnVolumeEstimate();
		}
	}
	return SpawnVolumeSum;
}

void FPhysXParticleSystem::SetEmissionBudget(INT EmissionBudget)
{
	if(EmissionBudget == INT_MAX)
	{
		for(INT i=0; i<SpawnInstanceRefs.Num(); i++)
		{
			FParticleEmitterInstance* Emitter = SpawnInstanceRefs(i);

			if(Emitter->Type()->IsA(FParticleSpritePhysXEmitterInstance::StaticType))
			{
				FParticleSpritePhysXEmitterInstance* SpawnSpriteEmitter = (FParticleSpritePhysXEmitterInstance*)Emitter;
				SpawnSpriteEmitter->SetEmissionBudget(INT_MAX);
			}
			else if(Emitter->Type()->IsA(FParticleMeshPhysXEmitterInstance::StaticType))
			{
				FParticleMeshPhysXEmitterInstance* SpawnMeshEmitter = (FParticleMeshPhysXEmitterInstance*)Emitter;
				SpawnMeshEmitter->SetEmissionBudget(INT_MAX);
			}
		}
		return;
	}

	TArray<FPhysXDistribution::Input>& InputBuffer = Distributer->GetInputBuffer(SpawnInstanceRefs.Num());
	for(INT i=0; i<SpawnInstanceRefs.Num(); i++)
	{
		FParticleEmitterInstance* Emitter = SpawnInstanceRefs(i);
		INT SpawnVolumeEstimate = 0;
		FLOAT WeightForSpawnLod = 0.0f;

		if(Emitter->Type()->IsA(FParticleSpritePhysXEmitterInstance::StaticType))
		{
			FParticleSpritePhysXEmitterInstance* SpawnSpriteEmitter = (FParticleSpritePhysXEmitterInstance*)Emitter;
			SpawnVolumeEstimate = SpawnSpriteEmitter->GetSpawnVolumeEstimate();
			WeightForSpawnLod = SpawnSpriteEmitter->GetWeightForSpawnLod();
		}
		else if(Emitter->Type()->IsA(FParticleMeshPhysXEmitterInstance::StaticType))
		{
			FParticleMeshPhysXEmitterInstance* SpawnMeshEmitter = (FParticleMeshPhysXEmitterInstance*)Emitter;
			SpawnVolumeEstimate = SpawnMeshEmitter->GetSpawnVolumeEstimate();
			WeightForSpawnLod = SpawnMeshEmitter->GetWeightForSpawnLod();
		}
		
		InputBuffer.AddItem(FPhysXDistribution::Input(SpawnVolumeEstimate, WeightForSpawnLod));
	}
	
	const TArray<FPhysXDistribution::Result>& ResultBuffer = Distributer->GetResult(EmissionBudget);

	for(INT i=0; i<ResultBuffer.Num(); i++)
	{
		const FPhysXDistribution::Result& R = ResultBuffer(i);		
		
		FParticleEmitterInstance* Emitter = SpawnInstanceRefs(i);

		if(Emitter->Type()->IsA(FParticleSpritePhysXEmitterInstance::StaticType))
		{
			FParticleSpritePhysXEmitterInstance* SpawnSpriteEmitter = (FParticleSpritePhysXEmitterInstance*)Emitter;
			SpawnSpriteEmitter->SetEmissionBudget(R.Piece);
		}
		else if(Emitter->Type()->IsA(FParticleMeshPhysXEmitterInstance::StaticType))
		{
			FParticleMeshPhysXEmitterInstance* SpawnMeshEmitter = (FParticleMeshPhysXEmitterInstance*)Emitter;
			SpawnMeshEmitter->SetEmissionBudget(R.Piece);
		}
	}
}

/**
 * Returns the total number of rendering particles linked this particle system.
 */

INT FPhysXParticleSystem::GetNumParticles()
{
	//return NumParticles;
	INT NumEmitterParticles = 0;
	for(INT i=0; i<ParticleSetRefs.Num(); i++)
	{
		FPhysXParticleSet& PSet = *ParticleSetRefs(i);
		NumEmitterParticles += PSet.GetNumRenderParticles();
	}

	return NumEmitterParticles;
}

FBox FPhysXParticleSystem::GetWorldBounds()
{
	FBox Bounds;
	Bounds.Init();
	if(!Fluid)
		return Bounds;

	NxBounds3 NxBounds;
	Fluid->getWorldBounds(NxBounds);
	if(NxBounds.isEmpty())
		return Bounds;

	Bounds.Min =  N2UPosition(NxBounds.min);
	Bounds.Max =  N2UPosition(NxBounds.max);
	//Bounds = Bounds.ExpandBy(1.0f);
	Bounds.IsValid = TRUE;
	return Bounds;
}


QSORT_RETURN
FPhysXParticleSystem::ComparePacketSizes( const Packet* A, const Packet* B )
{ 
	return A->SdkPacket->numParticles > B->SdkPacket->numParticles ? 1 : A->SdkPacket->numParticles < B->SdkPacket->numParticles? -1 : 0; 
}

QSORT_RETURN
FPhysXParticleSystem::ComparePacketAges( const Packet* A, const Packet* B )
{ 
	return B->MeanParticleAge > A->MeanParticleAge ? 1 : B->MeanParticleAge < A->MeanParticleAge? -1 : 0; 
}

QSORT_RETURN
FPhysXParticleSystem::ComparePacketLocation( const Packet* A, const Packet* B )
{
	if(B->IsInFront < A->IsInFront)
		return 1;

	return B->DistanceSq > A->DistanceSq ? 1 : B->DistanceSq < A->DistanceSq? -1 : 0; 
}

void FPhysXParticleSystem::AsyncPacketCulling()
{
	check(bProcessSimulationStep);

	if(VerticalPacketLimit >= NumPackets)
	{
		VerticalPacketRadiusSq *= 1.01f;
		return;
	}

	INT VerticalPacketReduction = (NumPackets > VerticalPacketLimit) ? NumPackets - VerticalPacketLimit : 0;

#if VERTICAL_LOD_USE_PACKET_AGE_CULLING
	//sort according to mean age
	appQsort( Packets, NumParticlePackets, sizeof(Packet), (QSORT_COMPARE)ComparePacketAges );	
#else
	//sort according to distance/clipping plane
	if (!bLocationSortDone)
	{
		appQsort( Packets, NumPackets, sizeof(Packet), (QSORT_COMPARE)ComparePacketLocation );
	}
#endif
	
	//sort selection according to packet size
	INT NumPacketSortForSize = Min(NumPackets/2, VerticalPacketReduction*2);
	appQsort( Packets, NumPacketSortForSize, sizeof(Packet), (QSORT_COMPARE)ComparePacketSizes );

	FLOAT MeanRadiusSq = 0.0f;

	//mark particles as deleted
	UINT RemovalCount = 0;
	for(INT i = 0; i < VerticalPacketReduction; ++i )
	{
		Packet& P = Packets[i];
		NxFluidPacket& SdkPacket = *P.SdkPacket;
		
		MeanRadiusSq += P.DistanceSq;

		for( UINT j = 0; j < SdkPacket.numParticles; ++j )
		{
			UINT ParticleIndex = SdkPacket.firstParticleIndex + j;
			PhysXParticle& Particle = ParticlesSdk[ParticleIndex];
			PhysXParticleEx& ParticleEx = ParticlesEx[Particle.Id];

			if(ParticleEx.PSet == NULL)
				continue;

			check(ParticleEx.Index != 0xffff);
			ParticleEx.PSet->RemoveParticle(ParticleEx.RenderIndex, true);		
			RemovalCount++;
		}
	}
	//plotf( TEXT("DEBUG_VE_PLOT numASyncRemovalsPacketCulling %d"), RemovalCount);
		
	MeanRadiusSq /= VerticalPacketReduction;

	//Some continuity...
	if(VerticalPacketRadiusSq > 2*MeanRadiusSq)
	{
		VerticalPacketRadiusSq = MeanRadiusSq;
	}
	else
	{
		VerticalPacketRadiusSq *= 0.99f;
		if(VerticalPacketRadiusSq < 1.0f)
		{
			VerticalPacketRadiusSq = 1.0f;
		}
	}
}

UBOOL FPhysXParticleSystem::GetLODOrigin(FVector& OutLODOrigin)
{
	if(SpawnInstanceRefs.Num() == 0)
		return FALSE;

	FParticleEmitterInstance* Emitter = SpawnInstanceRefs(0);

	if(Emitter->Type()->IsA(FParticleSpritePhysXEmitterInstance::StaticType))
	{
		FParticleSpritePhysXEmitterInstance* SpawnSpriteEmitter = (FParticleSpritePhysXEmitterInstance*)Emitter;
		return SpawnSpriteEmitter->GetLODOrigin(OutLODOrigin);
	}
	else if(Emitter->Type()->IsA(FParticleMeshPhysXEmitterInstance::StaticType))
	{
		FParticleMeshPhysXEmitterInstance* SpawnMeshEmitter = (FParticleMeshPhysXEmitterInstance*)Emitter;
		return SpawnMeshEmitter->GetLODOrigin(OutLODOrigin);
	}

	return FALSE;
}	

UBOOL FPhysXParticleSystem::GetLODNearClippingPlane(FPlane& OutNearClippingPlane)
{
	if(SpawnInstanceRefs.Num() == 0)
		return FALSE;

	FParticleEmitterInstance* Emitter = SpawnInstanceRefs(0);

	if(Emitter->Type()->IsA(FParticleSpritePhysXEmitterInstance::StaticType))
	{
		FParticleSpritePhysXEmitterInstance* SpawnSpriteEmitter = (FParticleSpritePhysXEmitterInstance*)Emitter;
		return SpawnSpriteEmitter->GetLODNearClippingPlane(OutNearClippingPlane);
	}
	else if(Emitter->Type()->IsA(FParticleMeshPhysXEmitterInstance::StaticType))
	{
		FParticleMeshPhysXEmitterInstance* SpawnMeshEmitter = (FParticleMeshPhysXEmitterInstance*)Emitter;
		return SpawnMeshEmitter->GetLODNearClippingPlane(OutNearClippingPlane);
	}

	return FALSE;
}

#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

///////////////////////////////////////////////////////////////////////////////////
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

void FPhysXDistribution::ComputeResult(INT Cake)
{
	if( InputBuffer.Num() == 0 )
	{
		return;
	}

	FLOAT ProductSum = 0.0f;
	for(INT i=0; i<InputBuffer.Num(); i++)
	{
		Input& In = InputBuffer(i);
		Temp Tmp;
		Tmp.Num = In.Num;
		Tmp.InputIndex = i;
		Tmp.Product = In.Num * In.Weight;
		Tmp.Piece = 0;
		TempBuffer.AddItem(Tmp);
		ProductSum += Tmp.Product;
	}
	if(ProductSum == 0.0f)
	{
		for(INT i=0; i<TempBuffer.Num(); i++)
		{
			Temp& Tmp = TempBuffer(i);
			Tmp.Product = Tmp.Num;
			ProductSum += Tmp.Product;
		}
	}

	//Bubblesort, In order to prevent round up errors
	if(TempBuffer.Num())
	{
		UBOOL Bubbled = TRUE;
		UINT N = TempBuffer.Num()-1;
		while(Bubbled)
		{
			Bubbled = FALSE;
			for(UINT i=0; i<N; i++)
			{
				Temp& Tmp0 = TempBuffer(i);
				Temp& Tmp1 = TempBuffer(i+1);

				if(Tmp0.Product < Tmp1.Product)
				{
					TempBuffer.SwapItems(i, i+1);
					Bubbled = TRUE;
				}
			}
			N = N-1;
		}
	}

	//Distribute Sum according to the product of Num and Weight.
	FLOAT NFactor = 1.0f / ProductSum;
	INT Leftover = Cake;
	INT NewParticleSum = 0;
	for(INT i=0; i<TempBuffer.Num(); i++)
	{
		Temp& Tmp = TempBuffer(i);

		INT Piece = (INT)NxMath::ceil(Cake*Tmp.Product*NFactor);
		Piece = Min<UINT>(Piece, Leftover);
		Piece = Min<UINT>(Piece, Tmp.Num);
		Leftover -= Piece;
		Tmp.Num -= Piece;
		Tmp.Piece = Piece;
		NewParticleSum += Tmp.Num;
	}

	if(Leftover > 0)
	{

		//Distribute rest proportional to particles left
		NFactor = 1.0f / NewParticleSum;
		Cake = Leftover;
		for(INT i=0; i<TempBuffer.Num(); i++)
		{
			Temp& Tmp = TempBuffer(i); 

			INT Piece = (INT)NxMath::ceil(Cake*Tmp.Num*NFactor);
			Piece = Min<UINT>(Piece, Leftover);
			Piece = Min<UINT>(Piece, Tmp.Num);
			Leftover -= Piece;
			Tmp.Num -= Piece;
			Tmp.Piece += Piece;
		}
	}
	check(Leftover == 0);

	ResultBuffer.Add(TempBuffer.Num());
	for(INT i=0; i<TempBuffer.Num(); i++)
	{
		Temp& Tmp = TempBuffer(i);
		Result& Result = ResultBuffer(Tmp.InputIndex);
		Result.Piece = Tmp.Piece;
	}
}

#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
