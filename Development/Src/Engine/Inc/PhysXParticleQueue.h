/*=============================================================================
	PhysXParticleQueue.h: PhysX Emitter Source.
	Copyright 2007-2008 AGEIA Technologies.
=============================================================================*/

#ifndef __PHYSXPARTICLEQUEUE_H__
#define __PHYSXPARTICLEQUEUE_H__

#include "EngineParticleClasses.h"

#if WITH_NOVODEX

/**
Class providing a priority queue of particles. The particles are defined through 
their id and an index into an external particle buffer. The particles 
are prioritized with respect to their death time, a value which doesn't change 
over time.
*/
class FPhysXParticleQueue
{
public:

	struct QueueParticle
	{
		QueueParticle():
			Id(0xffff),
			Index(0xffff),
			DeathTime(0.0f)
		{}

		QueueParticle(WORD _Id, WORD _Index, FLOAT _DeathTime):
			Id(_Id),
			Index(_Index),
			DeathTime(_DeathTime)
		{}

		WORD	Id;
		WORD	Index;
        FLOAT	DeathTime;
	};

	FPhysXParticleQueue(WORD InitialSize);
	~FPhysXParticleQueue();

	void Resize(UINT NewMax);

	const QueueParticle& GetParticle(INT i) { return Heap[i+1]; }

	void AddParticle(WORD Id, WORD ParticleIndex, FLOAT DeathTime, BYTE* InIndexBase, UINT InIndexStride);
	UBOOL RemoveParticleById(WORD Id, QueueParticle& Removed, BYTE* InIndexBase, UINT InIndexStride);
	UBOOL RemoveParticleById(WORD Id, BYTE* InIndexBase, UINT InIndexStride);
	UBOOL RemoveParticle(WORD HeapIndex, QueueParticle& Removed, BYTE* InIndexBase, UINT InIndexStride);
	UBOOL RemoveParticle(QueueParticle& Removed, BYTE* InIndexBase, UINT InIndexStride);
	FLOAT RemoveParticles(WORD NumParticles, BYTE* InIndexBase, UINT InIndexStride);
	UBOOL Front(QueueParticle& Removed);
	void Clear();
	INT Num();
	void UpdateParticleIndex(WORD QueueIndex, WORD NewParticleIndex);

private:

	FORCEINLINE void UpdateIndex(WORD HeapIndex)
	{
		WORD& Index = *reinterpret_cast<WORD*>(IndexBase + IndexStride*Heap[HeapIndex].Index); 
		Index = HeapIndex;
	}

	WORD FindHeapIndex(WORD Id);
	void _Resize(WORD NewMax);

	void HeapInit();
	void HeapSift(WORD i);
	void HeapUpdate(WORD i);
	void HeapRemove(WORD i);
	void HeapInsert(const QueueParticle& Particle);
	void HeapTest();

	QueueParticle*	Heap;
	WORD			HeapSize;
	WORD			MaxParticles;

	BYTE*			IndexBase;
	UINT			IndexStride;
};

#endif	//#if WITH_NOVODEX

#endif
