/*=============================================================================
	RingBuffer.cpp: Ring buffer definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#define USE_DATA_WRITTEN_EVENT (!CONSOLE || GIsHighPrecisionThreadingEnabled)

FRingBuffer::FRingBuffer(UINT BufferSize, UINT InAlignment)
:	DataWrittenEvent(NULL)
{
	Data = new BYTE[appRoundUpToPowerOfTwo(BufferSize)];
	DataEnd = Data + BufferSize;
	Alignment = appRoundUpToPowerOfTwo(InAlignment);
	ReadPointer = WritePointer = Data;
}

FRingBuffer::AllocationContext::AllocationContext(FRingBuffer& InRingBuffer,UINT InAllocationSize)
:	RingBuffer(InRingBuffer)
{
	// Only allow a single AllocationContext at a time for the ring buffer.
	check(!RingBuffer.bIsWriting);
	RingBuffer.bIsWriting = TRUE;

	// Check that the allocation will fit in the buffer.
	const UINT AlignedAllocationSize = Align(InAllocationSize,RingBuffer.Alignment);
	const UINT BufferSize = (UINT)(RingBuffer.DataEnd - RingBuffer.Data);
	check(AlignedAllocationSize < BufferSize);

	// Use the memory referenced by WritePointer for the allocation, wrapped around to the beginning of the buffer
	// if it was at the end.
	AllocationStart = RingBuffer.WritePointer != RingBuffer.DataEnd ? RingBuffer.WritePointer : RingBuffer.Data;

	// If there isn't enough space left in the buffer to allocate the full size, allocate all the remaining bytes in the buffer.
	AllocationEnd = Min(RingBuffer.DataEnd,AllocationStart + AlignedAllocationSize);

	// Wait until the reading thread has finished reading the area of the buffer we want to allocate.
	while(TRUE)
	{
		// Make a snapshot of a recent value of ReadPointer.
		BYTE* CurrentReadPointer = RingBuffer.ReadPointer;

		// If the ReadPointer and WritePointer are the same, the buffer is empty and there's no risk of overwriting unread data.
		if(CurrentReadPointer == RingBuffer.WritePointer)
		{
			break;
		}
		else
		{
			// If the allocation doesn't contain the read pointer, the allocation won't overwrite unread data.
			// Note that it needs to also prevent advancing WritePointer to match the current ReadPointer, since that would signal that the
			// buffer is empty instead of the expected full.
			const UBOOL bAllocationContainsReadPointer =
				AllocationStart <= CurrentReadPointer && 
				CurrentReadPointer <= AllocationEnd;
			if(!bAllocationContainsReadPointer)
			{
				break;
			}
		}
	}

}

FRingBuffer::AllocationContext::~AllocationContext()
{
	Commit();
}

void FRingBuffer::AllocationContext::Commit()
{
	if(AllocationStart)
	{
		// Use a memory barrier to ensure that data written to the ring buffer is visible to the reading thread before the WritePointer
		// update.
		appMemoryBarrier();

		// Advance the write pointer to the next unallocated byte.
		RingBuffer.WritePointer = AllocationEnd;

		// Reset the bIsWriting flag to allow other AllocationContexts to be created for the ring buffer.
		RingBuffer.bIsWriting = FALSE;

		// Clear the allocation pointer, to signal that it has been committed.
		AllocationStart = NULL;

		if(USE_DATA_WRITTEN_EVENT)
		{
			// Lazily create the data-written event.  It can't be done in the FRingBuffer constructor because GSynchronizeFactory may not
			// be initialized at that point.
			if(!RingBuffer.DataWrittenEvent)
			{
				RingBuffer.DataWrittenEvent = GSynchronizeFactory->CreateSynchEvent();
				checkf(RingBuffer.DataWrittenEvent,TEXT("Failed to create data-write event for FRingBuffer"));
			}

			// Trigger the data-written event to wake the reader thread.
			RingBuffer.DataWrittenEvent->Trigger();
		}
	}
}

UBOOL FRingBuffer::BeginRead(void*& OutReadPointer,UINT& OutReadSize)
{
	// Make a snapshot of a recent value of WritePointer, and use a memory barrier to ensure that reads from the data buffer
	// will see writes no older than this snapshot of the WritePointer.
	BYTE* CurrentWritePointer = WritePointer;
	appMemoryBarrier();

	// Determine whether the write pointer or the buffer end should delimit this contiguous read.
	BYTE* ReadEndPointer;
	if(CurrentWritePointer >= ReadPointer)
	{
		ReadEndPointer = CurrentWritePointer;
	}
	else
	{
		// If the read pointer has reached the end of readable data in the buffer, reset it to the beginning of the buffer.
		if(ReadPointer == DataEnd)
		{
			ReadPointer = Data;
			ReadEndPointer = CurrentWritePointer;
		}
		else
		{
			ReadEndPointer = DataEnd;
		}
	}

	// Determine whether there's data to read, and how much.
	if(ReadPointer < ReadEndPointer)
	{
		OutReadPointer = ReadPointer;
		OutReadSize = ReadEndPointer - ReadPointer;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void FRingBuffer::FinishRead(UINT ReadSize)
{
	ReadPointer += Align( ReadSize, Alignment );
}

void FRingBuffer::WaitForRead(DWORD WaitTime)
{
	// If the buffer is empty, wait for the data-written event to be triggered.
	if(ReadPointer == WritePointer)
	{
		if(USE_DATA_WRITTEN_EVENT)
		{
			if(DataWrittenEvent)
			{
				DataWrittenEvent->Wait(WaitTime);
			}
		}
		else
		{
			appSleep(0);
		}
	}
}
