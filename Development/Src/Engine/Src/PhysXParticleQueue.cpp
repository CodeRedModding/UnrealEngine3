/*=============================================================================
	PhysXParticleQueue.cpp: PhysX Emitter Source.
	Copyright 2007-2008 AGEIA Technologies.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

#if WITH_NOVODEX

#include "UnNovodexSupport.h"
#include "PhysXParticleQueue.h" 

FPhysXParticleQueue::FPhysXParticleQueue(WORD InitialSize):
	Heap(NULL),
	HeapSize(0),
	MaxParticles(0)
{
	Resize(InitialSize);
	HeapInit();
}

FPhysXParticleQueue::~FPhysXParticleQueue()
{
	appFree(Heap);
	Heap = NULL;

}

void FPhysXParticleQueue::Resize(UINT NewMax)
{
	if(NewMax == 0)
	{
		if(Heap)
		{
			appFree(Heap);
			Heap = NULL;
		}
		MaxParticles = 0;
	}
	else if(NewMax > MaxParticles)
	{
		_Resize(2*NewMax);

	}
	else if(NewMax*4 < MaxParticles)
	{
		_Resize(NewMax);
	}
}

INT FPhysXParticleQueue::Num()
{
	check(HeapSize > 0);
	return HeapSize-1;
}

void FPhysXParticleQueue::AddParticle(WORD Id, WORD ParticleIndex, FLOAT DeathTime, BYTE* InIndexBase, UINT InIndexStride)
{
	//insert elements into heap, index is updated automatically
	Resize(HeapSize+1);
	IndexBase = InIndexBase;
	IndexStride = InIndexStride;

	QueueParticle Particle(Id, ParticleIndex, DeathTime);
	HeapInsert(Particle);
}

UBOOL FPhysXParticleQueue::RemoveParticleById(WORD Id, BYTE* InIndexBase, UINT InIndexStride)
{
	WORD Index = FindHeapIndex(Id);
	if (Index == 0)
		return FALSE;

	check(HeapSize > 1);
	IndexBase = InIndexBase;
	IndexStride = InIndexStride;

	HeapRemove(Index);
	Resize(HeapSize);
	return TRUE;
}

UBOOL FPhysXParticleQueue::RemoveParticleById(WORD Id, QueueParticle& Removed, BYTE* InIndexBase, UINT InIndexStride)
{
	WORD Index = FindHeapIndex(Id);
	if (Index == 0)
		return FALSE;

	check(HeapSize > 1);
	IndexBase = InIndexBase;
	IndexStride = InIndexStride;

	Removed = Heap[Index];
	HeapRemove(Index);
	Resize(HeapSize);
	return TRUE;
}

UBOOL FPhysXParticleQueue::RemoveParticle(WORD HeapIndex, QueueParticle& Removed, BYTE* InIndexBase, UINT InIndexStride)
{
	check(HeapSize > 1);
	check(HeapIndex < HeapSize);
	IndexBase = InIndexBase;
	IndexStride = InIndexStride;

	Removed = Heap[HeapIndex];
	HeapRemove(HeapIndex);
	Resize(HeapSize);
	return TRUE;
}

UBOOL FPhysXParticleQueue::RemoveParticle(QueueParticle& Removed, BYTE* InIndexBase, UINT InIndexStride)
{
	if(HeapSize <= 1)
		return FALSE;
	
	IndexBase = InIndexBase;
	IndexStride = InIndexStride;

	Removed = Heap[1];
	HeapRemove(1);

	return TRUE;
}

UBOOL FPhysXParticleQueue::Front(QueueParticle& Removed)
{
	if(HeapSize <= 1)
		return FALSE;

	Removed = Heap[1];
	return TRUE; 
}

void FPhysXParticleQueue::UpdateParticleIndex(WORD QueueIndex, WORD NewParticleIndex)
{
	Heap[QueueIndex].Index = NewParticleIndex;
}

FLOAT FPhysXParticleQueue::RemoveParticles(WORD NumParticles, BYTE* InIndexBase, UINT InIndexStride)
{
	IndexBase = InIndexBase;
	IndexStride = InIndexStride;

	FLOAT DeathTime = 0.0f;
	while (HeapSize > 1 && NumParticles--)
	{
		DeathTime = Heap[1].DeathTime;
		HeapRemove(1);
	}

	return DeathTime;
}

void FPhysXParticleQueue::Clear()
{
	HeapInit();
}


//private:

void FPhysXParticleQueue::_Resize(WORD NewMax)
{
	check(NewMax+1 > HeapSize);
	QueueParticle* NewHeap = (QueueParticle*)appMalloc(sizeof(QueueParticle)*(NewMax + 1));
	
	if(Heap)
	{
		appMemcpy(NewHeap, Heap, sizeof(QueueParticle) * (HeapSize));
		appFree(Heap);
	}

	Heap = NewHeap;	
	MaxParticles = NewMax;
}

WORD FPhysXParticleQueue::FindHeapIndex(WORD Id)
{
	WORD i = 1;
	while (i < HeapSize && Id != Heap[i].Id)
		i++;
	
	if(i==HeapSize)
		return 0;

	return i;
}

void FPhysXParticleQueue::HeapInit()
{
	// make heap
	HeapSize = 0;
	Heap[HeapSize++] = QueueParticle();	// dummy, root must be at position 1!
}

void FPhysXParticleQueue::HeapSift(WORD i)
{
	check(1 <= i && i < HeapSize);
	QueueParticle Particle = Heap[i];
	while ((i << 1) < HeapSize) 
	{
		WORD j = i << 1;
		if (j+1 < HeapSize && Heap[j+1].DeathTime < Heap[j].DeathTime)
			j++;
		if (Heap[j].DeathTime < Particle.DeathTime) 
		{
			Heap[i] = Heap[j];
			UpdateIndex(i);
			i = j; 
		}
		else break;
	}
	Heap[i] = Particle; 
	UpdateIndex(i);
}

void FPhysXParticleQueue::HeapUpdate(WORD i)
{
	check(1 <= i && i < HeapSize);
	QueueParticle Particle = Heap[i];
	while (i > 1) 
	{
		WORD j = i >> 1;
		if (Heap[j].DeathTime > Particle.DeathTime) 
		{
			Heap[i] = Heap[j];
			UpdateIndex(i);
			i = j;
		}
		else break;
	}
	while ((i << 1) < HeapSize) 
	{
		WORD j = i << 1;
		if (j+1 < HeapSize && Heap[j+1].DeathTime < Heap[j].DeathTime)
			j++;
		if (Heap[j].DeathTime < Particle.DeathTime) 
		{
			Heap[i] = Heap[j];
			UpdateIndex(i);
			i = j;
		}
		else break;
	}
	Heap[i] = Particle;
	UpdateIndex(i);
}

void FPhysXParticleQueue::HeapRemove(WORD i)
{
	check(1 <= i && i < HeapSize);

	Heap[i] = Heap[HeapSize-1];
	UpdateIndex(i);
	HeapSize--;
		
	if (i < HeapSize) 
	{
		HeapSift(i);
	}
}

void FPhysXParticleQueue::HeapInsert(const QueueParticle& Particle)
{
	check(HeapSize <= MaxParticles + 1);
	Heap[HeapSize] = Particle;
	UpdateIndex(HeapSize);
	HeapSize++;
	HeapUpdate(HeapSize-1);
}

void FPhysXParticleQueue::HeapTest()
{
	for (WORD i = 1; i < HeapSize; i++) 
	{
		if ((i << 1) < HeapSize) 
		{
			WORD j = i << 1;
			if (j+1 < HeapSize && Heap[j+1].DeathTime < Heap[j].DeathTime)
				j++;
			check(Heap[i].DeathTime <= Heap[j].DeathTime);
		}
	}
}

#endif	//#if WITH_NOVODEX
