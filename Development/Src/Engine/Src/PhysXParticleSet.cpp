/*=============================================================================
	PhysXParticleSet.cpp: PhysX Emitter Source.
	Copyright 2007-2008 AGEIA Technologies.
=============================================================================*/

#include "EnginePrivate.h"

#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

#include "UnNovodexSupport.h"
#include "PhysXParticleSet.h"
#include "PhysXParticleQueue.h"
#include "PhysXParticleSystem.h"

FPhysXParticleSet::FPhysXParticleSet(INT _ElementSize, FPhysXEmitterVerticalLodProperties& _VerticalLod, UPhysXParticleSystem*& InPhysXParSys) : 
	NumReducedParticles(0),
	ReductionPopMeasure(0.0f),
	ReductionEarlyMeasure(0.0f),
	QTarget(0.0f),
	VerticalLod(_VerticalLod),
	PhysXParSys(InPhysXParSys)
{
	RenderParticlesBuffer = NULL;
	RenderParticlesNum = 0;
	ElementSize = (_ElementSize+15)&~15;
	RenderParticlesMax = 32;
	RenderParticlesBuffer = (BYTE*)appMalloc(ElementSize*RenderParticlesMax);
	check(RenderParticlesBuffer);

	check(sizeof(PhysXRenderParticle) <= ElementSize);
	LifetimeQueue = new FPhysXParticleQueue(1024);
	DeathQueue = new FPhysXParticleQueue(128);
}

FPhysXParticleSet::~FPhysXParticleSet()
{
	appFree(RenderParticlesBuffer);
	RenderParticlesBuffer = NULL;
	RenderParticlesMax = 0;
	delete LifetimeQueue;
	LifetimeQueue = NULL;
	delete DeathQueue;
	DeathQueue = NULL;
}

void FPhysXParticleSet::SyncPhysXData()
{
	ReductionPopMeasure = 0.0f;
	ReductionEarlyMeasure = 0.0f;
}

void FPhysXParticleSet::AddParticle(const PhysXRenderParticle* RenderParticle, const UParticleSystemComponent* const OwnerComponent)
{
	INT NewRenderParticlesNum = RenderParticlesNum+1;
	if(RenderParticlesMax < NewRenderParticlesNum)
	{
		INT NewRenderParticlesMax = NewRenderParticlesNum + appTrunc(appSqrt((FLOAT)NewRenderParticlesNum) + 1);
		BYTE* NewRenderParticlesBuffer = (BYTE*)appMalloc(NewRenderParticlesMax*ElementSize);
		appMemcpy(NewRenderParticlesBuffer, RenderParticlesBuffer, RenderParticlesNum*ElementSize);
		appFree(RenderParticlesBuffer);
		RenderParticlesBuffer = NewRenderParticlesBuffer;
		RenderParticlesMax = NewRenderParticlesMax;
		check(RenderParticlesMax >= NewRenderParticlesNum);
	}

	INT ParticleIndex = RenderParticlesNum;
	PhysXRenderParticle* DestPtcl = (PhysXRenderParticle*)(RenderParticlesBuffer + ParticleIndex*ElementSize);
	appMemcpy(DestPtcl, RenderParticle, ElementSize);
	RenderParticlesNum++;
	DestPtcl->Flags = 0;
	DestPtcl->SpawnTime = GWorld->GetTimeSeconds();
	DestPtcl->OwnerEmitterID = (SIZE_T)OwnerComponent;

	BYTE* IndexBase = reinterpret_cast<BYTE*>(&GetRenderParticle(0)->QueueIndex);
	UINT IndexStride = ElementSize;

	LifetimeQueue->AddParticle((WORD)RenderParticle->Id, ParticleIndex, GWorld->GetTimeSeconds(), IndexBase, IndexStride);
}

INT FPhysXParticleSet::RemoveParticle(INT RenderIndex, bool bRemoveFromPSys)
{
	return RemoveParticleFast(RenderIndex, bRemoveFromPSys);
}

INT FPhysXParticleSet::RemoveParticleFast(INT RenderParticleIndex, bool bRemoveFromPSys)
{
	INT ParticleIdToFix;
	INT Id = GetRenderParticle(RenderParticleIndex)->Id;
	RemoveRenderParticle(RenderParticleIndex, ParticleIdToFix);
	if(bRemoveFromPSys)
	{
		GetPSys().RemoveParticle(Id, ParticleIdToFix);
		return -1;
	}
	return ParticleIdToFix;
}

void FPhysXParticleSet::RemoveRenderParticle(INT RenderIndex, INT& ParticleIdToFix)
{
	PhysXRenderParticle* RenderParticle = GetRenderParticle(RenderIndex);

	BYTE* IndexBase = reinterpret_cast<BYTE*>(&GetRenderParticle(0)->QueueIndex);
	UINT IndexStride = ElementSize;
	
	FPhysXParticleQueue::QueueParticle Qp;
	UBOOL IsDeathQueue = (RenderParticle->Flags & PhysXRenderParticle::PXRP_DeathQueue) > 0;
	FPhysXParticleQueue* SrcQueue = IsDeathQueue?DeathQueue:LifetimeQueue;
	UBOOL RemovedFromQueue = SrcQueue->RemoveParticle(RenderParticle->QueueIndex, Qp, IndexBase, IndexStride);
	check(RemovedFromQueue);
	check(Qp.Id == RenderParticle->Id);
	
	appMemcpy(RenderParticle, GetRenderParticle(RenderParticlesNum-1), ElementSize);

	//correct the queue of the moved particle
	if(Qp.Index+1 != RenderParticlesNum)
	{
		IsDeathQueue = (RenderParticle->Flags & PhysXRenderParticle::PXRP_DeathQueue) > 0;
		FPhysXParticleQueue* QueueToCorrect = IsDeathQueue?DeathQueue:LifetimeQueue;
		QueueToCorrect->UpdateParticleIndex(RenderParticle->QueueIndex, static_cast<WORD>(RenderIndex));
	}

	ParticleIdToFix = RenderParticle->Id;

	//Here remove needs to be last, since it actually may result in a reallocation. Well it doesn't anymore, since we do our own...
	RenderParticlesNum--;
}

UBOOL FPhysXParticleSet::MoveParticleFromLifeToDeath(INT RenderParticleIndex)
{
	check(RenderParticleIndex < RenderParticlesNum);
	PhysXRenderParticle* RenderParticle = GetRenderParticle(RenderParticleIndex);
	if(RenderParticle->Flags & PhysXRenderParticle::PXRP_DeathQueue)
	{
		return FALSE;
	}

	FLOAT Deathtime = 0;
	if(RenderParticle->OneOverMaxLifetime > 0.0f)
		Deathtime = GWorld->GetTimeSeconds() + ((1.0f - RenderParticle->RelativeTime) / RenderParticle->OneOverMaxLifetime);

	BYTE* IndexBase = reinterpret_cast<BYTE*>(&GetRenderParticle(0)->QueueIndex);
	UINT IndexStride = ElementSize;

	FPhysXParticleQueue::QueueParticle Qp;
	UBOOL RemovedFromQueue = LifetimeQueue->RemoveParticle(RenderParticle->QueueIndex, Qp, IndexBase, IndexStride);
	check(RemovedFromQueue);
	check(Qp.Id == RenderParticle->Id);
	RenderParticle->Flags |= PhysXRenderParticle::PXRP_DeathQueue;
	DeathQueue->AddParticle(static_cast<WORD>(RenderParticle->Id), static_cast<WORD>(RenderParticleIndex), Deathtime, IndexBase, IndexStride);
	return TRUE;
}

//This is probably not very fast, but we expect to have a low count here.
void FPhysXParticleSet::DeathRowManagment()
{
	FPhysXParticleSystem& PSys = GetPSys();
	for(INT i=0; i<TmpRenderIndices.Num(); i++)
	{
		MoveParticleFromLifeToDeath(TmpRenderIndices(i));
	}
	TmpRenderIndices.Empty();
	
	ReductionEarlyMeasure = 0.0f;
	FPhysXParticleQueue::QueueParticle Qp;
	UINT RemovedCountRegular = 0;
	while(DeathQueue->Front(Qp))
	{
		PhysXRenderParticle* RenderParticle = GetRenderParticle(Qp.Index);
		if(RenderParticle->RelativeTime < 1.0f)
			break;

		UBOOL IsEarlyRemoval = (RenderParticle->Flags & PhysXRenderParticle::PXRP_EarlyReduction) > 0;
		if(IsEarlyRemoval)
		{
			ReductionEarlyMeasure += 1.0f;
		}
		else
		{
			RemovedCountRegular++;
		}

		RemoveParticle(Qp.Index, true);
	}
	//plotf( TEXT("DEBUG_VE_PLOT numASyncRemovalsDeathRowRegular %d"), RemovedCountRegular);
	//plotf( TEXT("DEBUG_VE_PLOT numASyncRemovalsDeathRowEarly %f"), ReductionEarlyMeasure);
}

void FPhysXParticleSet::SyncParticleReduction(UINT NumParticleReduction)
{
	NumReducedParticles = NumParticleReduction;
	
	if(NumParticleReduction == 0)
		return;

	check(NumParticleReduction <= (UINT)RenderParticlesNum);
	FPhysXParticleQueue::QueueParticle Qp;
	FLOAT RelTimeUntilFadeout = 1.0f -  NxMath::clamp(VerticalLod.RelativeFadeoutTime, 1.0f, 0.0f);

	while(NumParticleReduction > 0 && DeathQueue->Front(Qp))
	{
		SyncRemoveParticle(Qp.Id);
		NumParticleReduction--;
	}

	while(NumParticleReduction > 0 && LifetimeQueue->Front(Qp))
	{
		SyncRemoveParticle(Qp.Id);
		NumParticleReduction--;
	}
}

void FPhysXParticleSet::AsyncParticleReduction(FLOAT DeltaTime)
{
	//test
	if(ReductionPopMeasure > 0.0f)
	{
		QTarget += Min(ReductionPopMeasure, (QTarget*0.1f) + 1);
	}
	else
	{
		QTarget -= Min(ReductionEarlyMeasure, QTarget*0.1f);
	}
	
	INT DeathQueueNum = DeathQueue->Num();
	INT KillNum = QTarget > DeathQueueNum ? appFloor(QTarget - DeathQueueNum) : 0;
	//plotf( TEXT("DEBUG_VE_PLOT QTarget %f"), QTarget);
	//plotf( TEXT("DEBUG_VE_PLOT DeathQueueNum %d"), DeathQueueNum);

	//debugf( TEXT("DEBUG_VE Emitter %#x: NumReducedParticles %d, ReductionPopMeasure %f, DeathQueueNum %d, QTarget %f"), this, NumReducedParticles, ReductionPopMeasure, DeathQueueNum, QTarget);
	NumReducedParticles = 0;
	
	if(KillNum == 0)
		return;

	FLOAT Temp = NxMath::clamp(VerticalLod.RelativeFadeoutTime, 1.0f, 0.0f);
	FLOAT TimeUntilFadeout = 1.0f - Temp;

	//Age particles based on reduction number
	while(KillNum)
	{
		FPhysXParticleQueue::QueueParticle Qp;
		if (!LifetimeQueue->Front(Qp))
			break;

		PhysXRenderParticle* RenderParticle = GetRenderParticle(Qp.Index);

		MoveParticleFromLifeToDeath(Qp.Index);
		RenderParticle->RelativeTime = TimeUntilFadeout; 
		RenderParticle->Flags |= PhysXRenderParticle::PXRP_EarlyReduction;
		KillNum--;
	}
}

void FPhysXParticleSet::RemoveAllParticlesInternal()
{
	FPhysXParticleSystem& PSys = GetPSys();
	while(GetNumRenderParticles()>0)
	{
		INT Index = GetNumRenderParticles()-1;

		PhysXRenderParticle* RenderParticle = GetRenderParticle(Index);
		UINT Id = RenderParticle->Id;

		RemoveParticle(Index, true);
	}
}

void FPhysXParticleSet::SyncRemoveParticle(UINT Id)
{
	FPhysXParticleSystem& PSys = GetPSys();
	PhysXParticleEx& ParticleEx = PSys.ParticlesEx[Id];
	check(ParticleEx.Index != 0xffff);
	check(ParticleEx.PSet == this);

	PhysXRenderParticle* RenderParticle = GetRenderParticle(ParticleEx.RenderIndex);

	FLOAT RelativeFadeoutTime = NxMath::clamp(VerticalLod.RelativeFadeoutTime, 1.0f, 0.0f);
	FLOAT Pop;
	if(RelativeFadeoutTime > 0.0f)
	{
		Pop = (1.0f - RenderParticle->RelativeTime)/RelativeFadeoutTime;
		Pop = Min(Pop, 1.0f);
	}
	else
	{
		Pop = 1.0f;
	}

	ReductionPopMeasure += Pop;
	RemoveParticle(ParticleEx.RenderIndex, true);
}

#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
