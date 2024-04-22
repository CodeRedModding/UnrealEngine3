/*=============================================================================
	RingBuffer.h: Ring buffer definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

#include "UnThreadingBase.h"

/**
 * A ring buffer for use with two threads: a reading thread and a writing thread.
 */
class FRingBuffer
{
public:

	/**
	 * Minimal initialization constructor.
	 * @param BufferSize - The size of the data buffer to allocate.
	 * @param InAlignment - Alignment of each allocation unit (in bytes)
	 */
	FRingBuffer(UINT BufferSize,UINT InAlignment = 1);

	/**
	 * A reference to an allocated chunk of the ring buffer.
	 * Upon destruction of the context, the chunk is committed as written.
	 */
	class AllocationContext
	{
	public:

		/**
		 * Upon construction, AllocationContext allocates a chunk from the ring buffer.
		 * @param InRingBuffer - The ring buffer to allocate from.
		 * @param InAllocationSize - The size of the allocation to make.
		 */
		AllocationContext(FRingBuffer& InRingBuffer,UINT InAllocationSize);

		/** Upon destruction, the allocation is committed, if Commit hasn't been called manually. */
		~AllocationContext();

		/** Commits the allocated chunk of memory to the ring buffer. */
		void Commit();

		// Accessors.
		void* GetAllocation() const { return AllocationStart; }
		UINT GetAllocatedSize() const { return AllocationEnd - AllocationStart; }

	private:
		FRingBuffer& RingBuffer;
		BYTE* AllocationStart;
		BYTE* AllocationEnd;
	};

	/**
	 * Checks if there is data to be read from the ring buffer, and if so accesses the pointer to the data to be read.
	 * @param OutReadPointer - When returning TRUE, this will hold the pointer to the data to read.
	 * @param MaxReadSize - When returning TRUE, this will hold the number of bytes available to read.
	 * @return TRUE if there is data to be read.
	 */
	UBOOL BeginRead(void*& OutReadPointer,UINT& OutReadSize);

	/**
	 * Frees the first ReadSize bytes available for reading via BeginRead to the writing thread.
	 * @param ReadSize - The number of bytes to free.
	 */
	void FinishRead(UINT ReadSize);

	/**
	 * Waits for data to be available for reading.
	 * @param WaitTime Time in milliseconds to wait before returning.
	 */
	void WaitForRead(DWORD WaitTime = INFINITE);

private:

	/** The data buffer. */
	BYTE* Data;

	/** The first byte after end the of the data buffer. */
	BYTE* DataEnd;

	/** The next byte to be written to. */
	BYTE* volatile WritePointer;

	/** TRUE if there is an AllocationContext outstanding for this ring buffer. */
	UBOOL bIsWriting;

	/** The next byte to be read from. */
	BYTE* volatile ReadPointer;

	/** Alignment of each allocation unit (in bytes). */
	UINT Alignment;

	/** The event used to signal the reader thread when the ring buffer has data to read. */
	class FEvent* DataWrittenEvent;
};

#endif
