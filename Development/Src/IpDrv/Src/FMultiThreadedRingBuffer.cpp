/*=============================================================================
	FMultiThreadedRingBuffer.cpp: Ring buffer ready to use in a multi-threaded environment.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnIpDrv.h"
#include "FMultiThreadedRingBuffer.h"

#if WITH_UE3_NETWORKING

FMultiThreadedRingBuffer::FMultiThreadedRingBuffer( INT InBufferSize, INT InMaxPacketSize )
:	MaxPacketSize(InMaxPacketSize)
,	NumPackets(0)
,	BufferSize( InBufferSize )
,	ReadIndex(0)
,	WriteIndex(0)
{
	RingBuffer = new BYTE[ InBufferSize ];
	WorkEvent = GSynchronizeFactory->CreateSynchEvent();
	BufferMutex = GSynchronizeFactory->CreateCriticalSection();
}

FMultiThreadedRingBuffer::~FMultiThreadedRingBuffer()
{
	GSynchronizeFactory->Destroy(BufferMutex);
	GSynchronizeFactory->Destroy(WorkEvent);
	delete [] RingBuffer;
}

UBOOL FMultiThreadedRingBuffer::BeginPush( BufferData &Data, INT Size )
{
	FScopeLock ScopeLock(BufferMutex);

	// Increase requested size by space required to store data packet size
	const INT TotalSize = Size + sizeof(INT);

	// Too large for a single UDP packet?
	if (TotalSize > MaxPacketSize)
	{
		return FALSE;
	}

	// Find out free space for the packet or fail
	if (ReadIndex == WriteIndex && NumPackets > 0)
	{
		KickBuffer();
		return FALSE;
	}

	if (ReadIndex <= WriteIndex)
	{
		// Need to wrap around?
		if (WriteIndex + MaxPacketSize > BufferSize)
		{
			WriteIndex = 0;
			// Not enough space?
			if (TotalSize > ReadIndex)
			{
				KickBuffer();
				return FALSE;
			}
		}
	}
	// Not enough space?
	else if (WriteIndex + TotalSize > ReadIndex)
	{
		KickBuffer();
		return FALSE;
	}
	
	// Compose output data
	*(INT*) (RingBuffer + WriteIndex) = Size;
	Data.Buffer = RingBuffer + sizeof(INT) + WriteIndex;
	Data.Size = Size;

	return TRUE;
}

void FMultiThreadedRingBuffer::EndPush( BufferData &Data, UBOOL bForcePush/*=FALSE*/ )
{
	FScopeLock ScopeLock(BufferMutex);

	// Update write index and no. packets
	WriteIndex += Data.Size + sizeof(INT);
	NumPackets++;
}

UBOOL FMultiThreadedRingBuffer::Peek( BufferData &Entry  )
{
	FScopeLock ScopeLock(BufferMutex);

	if (NumPackets == 0)
	{
		return FALSE;
	}

	// Need to wrap around read index?
	if (ReadIndex + MaxPacketSize > BufferSize)
	{
		ReadIndex = 0;
	}

	ReadEntry(Entry);

	return TRUE;
}

void FMultiThreadedRingBuffer::Pop( BufferData &Entry )
{
	FScopeLock ScopeLock(BufferMutex);

	if (NumPackets == 0)
	{
		return;
	}

	ReadEntry(Entry);
	ReadIndex += Entry.Size + sizeof(INT);

	NumPackets--;
}

void FMultiThreadedRingBuffer::KickBuffer()
{
	WorkEvent->Trigger();
}

void FMultiThreadedRingBuffer::WaitForData( DWORD MilliSeconds /*= INFINITE*/ )
{
	WorkEvent->Wait( MilliSeconds );
}

void FMultiThreadedRingBuffer::ReadEntry(BufferData &Entry)
{
	Entry.Size = *(INT*) (RingBuffer + ReadIndex);
	Entry.Buffer = RingBuffer + sizeof(INT) + ReadIndex;
}

#endif	//#if WITH_UE3_NETWORKING
