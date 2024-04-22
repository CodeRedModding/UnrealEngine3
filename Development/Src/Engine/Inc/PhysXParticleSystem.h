/*=============================================================================
	PhysXParticleSystem.h: PhysX Emitter Source.
	Copyright 2007-2008 AGEIA Technologies.
=============================================================================*/

#ifndef __PHYSXPARTICLESYSTEM_H__
#define __PHYSXPARTICLESYSTEM_H__

#include "EngineParticleClasses.h"

#if WITH_NOVODEX && !NX_DISABLE_FLUIDS


#define PHYSX_NUM_DEL_NOT_WRITTEN 0xffffffff

//Vertical Defines... move...
#define VERTICAL_LOD_USE_PACKET_AGE_CULLING 0

#if 0
	#define plotf debugf
#else
	#define plotf(...)
#endif

/**
Particle system class wrapping one NxFluid instance. The class serves the simulation data to 
the registered FPhysXParticleSet instances. It also manages registered FParticleEmitterInstance
instance for spawning particles synchronously to the sdk and LOD settings. As long as 
FPhysXParticleSystem exists it's supposed to be connected to it's NxFluid counterpart. 
*/
class FPhysXParticleSystem
{
public:

	FPhysXParticleSystem(UPhysXParticleSystem& InParams);
	~FPhysXParticleSystem();

	INT AddParticles(NxParticleData& InPd, class FPhysXParticleSet* InParticleSet);
	
	void RemoveAllParticleSets();
	void AddParticleSet(FPhysXParticleSet* InParticleSet);
	void RemoveParticleSet(FPhysXParticleSet* InParticleSet);
	void RemoveAllSpawnInstances();
	void AddSpawnInstance(FParticleEmitterInstance* InSpawnInstance);
	void RemoveSpawnInstance(FParticleEmitterInstance* InSpawnInstance);
	FBox GetWorldBounds();

	FORCEINLINE INT GetSpawnInstanceRefsNum() { return SpawnInstanceRefs.Num(); }
	void PreSyncPhysXData();
	void PostSyncPhysXData();
	void SyncParticleReduction(INT InNumReduction);
	void Tick(FLOAT DeltaTime);

	void RemovedFromScene();
	void GetGravity(NxVec3& gravity);

	UBOOL IsSyncedToSdk();

	//LOD stufff
	FLOAT GetWeightForFifo();
	FLOAT GetWeightForSpawnLod();

	INT GetSpawnVolumeEstimate();
	void SetEmissionBudget(INT EB);

	INT GetNumParticles();

#if WITH_APEX
	class FApexEmitter *ApexEmitter;
#endif
    class NxFluid* Fluid;
	class FRBPhysScene* PhysScene;

    PhysXParticle* ParticlesSdk;
	PhysXParticleEx* ParticlesEx;
	PhysXParticleForceUpdate* ParticleForceUpdates;
	PhysXParticleFlagUpdate* ParticleFlagUpdates;
    NxVec3* ParticleContactsSdk;
    UINT* ParticleCreationIDsSdk;
    UINT* ParticleDeletionIDsSdk;
    NxFluidPacket* PacketsSdk;

    INT NumParticlesSdk;
    INT NumParticlesCreatedSdk;
    INT NumParticlesDeletedSdk;
    INT NumPacketsSdk;
	INT NumPackets;

	INT NumParticleForceUpdates;
	INT NumParticleFlagUpdates;
    
	INT	VerticalPacketLimit;
	FLOAT VerticalPacketRadiusSq;
	
	UPhysXParticleSystem& Params;

	INT DebugNumSDKDel;
	FORCEINLINE void RemoveParticle(UINT Id, UINT IdToFix)
	{
		PhysXParticleEx& ParticleEx = ParticlesEx[Id];
		check(ParticleEx.Index != 0xffff);

		check(NumParticleFlagUpdates < Params.MaxParticles + Params.ParticleSpawnReserve);
		PhysXParticleFlagUpdate& ParticleUpdate = ParticleFlagUpdates[NumParticleFlagUpdates++];
		ParticleUpdate.Id = Id;
		ParticleUpdate.Flags = NX_FP_DELETE;

		ParticlesEx[IdToFix].RenderIndex = ParticleEx.RenderIndex;
		ParticleEx.RenderIndex = 0xffff;
		ParticleEx.PSet = NULL;	
	}

	FORCEINLINE FLOAT GetLODDistanceSq(const FVector& LODCenter, const FVector& Position)
	{
		FVector DistVec = LODCenter - Position;
		
		if(bCylindricalPacketCulling)
		{
			DistVec.Z = 0.0f;
		}
		return DistVec.SizeSquared();
	}

private:

	void InitState();

	struct Packet
	{
		NxFluidPacket* SdkPacket;
		FLOAT DistanceSq;
		FLOAT MeanParticleAge;
		UBOOL IsInFront;
	};

	Packet* Packets;
	UBOOL bLocationSortDone;
	UBOOL bSdkDataSynced;
	UBOOL bProcessSimulationStep;
	UBOOL bCylindricalPacketCulling;

	NxFluidDesc CreateFluidDesc(NxCompartment* Compartment);
	void AssignReduction(INT InNumReduction);
	void InitAsyncParticleUpdates(UINT NumUpdates);
	void SyncParticleForceUpdates();
	void SyncParticleFlagUpdates();
	UBOOL SyncCheckSimulationStep();
	void SyncProcessSDKParticleRemovals();
	void SyncProcessParticles();
	void SyncRemoveHidden(INT& InNumReduction);
	void AsyncPacketCulling();
	UBOOL GetLODOrigin(FVector&);
	UBOOL GetLODNearClippingPlane(FPlane&);

	static QSORT_RETURN ComparePacketSizes( const Packet* A, const Packet* B );
	static QSORT_RETURN ComparePacketAges( const Packet* A, const Packet* B );
	static QSORT_RETURN ComparePacketLocation( const Packet* A, const Packet* B );

	class FPhysXDistribution* Distributer;
	
	TArray<FPhysXParticleSet*> ParticleSetRefs;

	TArray<FParticleEmitterInstance*> SpawnInstanceRefs; 
};

///////////////////////////////////////////////////////////////////////////////////

class FPhysXDistribution
{
public:

	FPhysXDistribution()
	{
	}

	struct Input
	{
		Input(INT _Num, FLOAT _Weight):
			Num(_Num),
			Weight(_Weight)
		{}

		INT Num;
		FLOAT Weight;
	};

	struct Result
	{
		INT Piece;
	};

	TArray<Input>& GetInputBuffer(INT Num)
	{
		InputBuffer.Reserve(Num);
		ResultBuffer.Reserve(Num);
		TempBuffer.Reserve(Num);

		INT Max = InputBuffer.GetSlack() + InputBuffer.Num();

		InputBuffer.Empty(Max);
		ResultBuffer.Empty(Max);
		TempBuffer.Empty(Max);

		return InputBuffer;
	}

	const TArray<Result>& GetResult(INT Cake)
	{
		ComputeResult(Cake);
		return ResultBuffer;
	}

private:

	struct Temp
	{
		INT Num;
		FLOAT Product;
		INT InputIndex;
		INT Piece;
	};

	void ComputeResult(INT Cake);

	TArray<Input> InputBuffer;
	TArray<Result> ResultBuffer;
	TArray<Temp> TempBuffer;
};

#else	//#if WITH_NOVODEX

class FPhysXParticleSystem
{
};

#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

#endif
