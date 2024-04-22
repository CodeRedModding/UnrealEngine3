/*=============================================================================
	FMultiThreadedRingBuffer.h:	Ring buffer ready to use in a multi-threaded environment.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MULTI_THREADED_RING_BUFFER_HEADER__
#define __MULTI_THREADED_RING_BUFFER_HEADER__

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

/**
 * Used for passing buffered data between one sender thread and one
 * receiver thread. Secure wrt. multi-threaded access.
 */
class FMultiThreadedRingBuffer
{
public:

	/** Data packet representation. */
	struct BufferData
	{
		/** Data pointer. */
		BYTE* Buffer;
		/** Data size. */
		INT Size;
	};

	FMultiThreadedRingBuffer( INT InBufferSize, INT InMaxPacketSize );
	~FMultiThreadedRingBuffer();

	UBOOL				BeginPush( BufferData &Data, INT Size );
	void				EndPush( BufferData &Data, UBOOL bForcePush=FALSE );
	UBOOL				Peek( BufferData &Entry  );
	void				Pop( BufferData &Entry );
	void				WaitForData( DWORD MilliSeconds = INFINITE );
	void				KickBuffer( );

protected:
	void				ReadEntry(BufferData &Entry);

	/** Maximal allowed packet size. */
	INT					MaxPacketSize;
	/** No. packets in buffer. */
	INT					NumPackets;
	/** The actual data buffer. */
	BYTE*				RingBuffer;
	/** Buffer size. */
	INT					BufferSize;
	/** Current reading index. */
	INT					ReadIndex;
	/** Current writing index. */
	INT					WriteIndex;
	/** An event used to wake up 'data retriever'. */
	FEvent*				WorkEvent;
	/** Buffer synchronization. */
	FCriticalSection*	BufferMutex;
};

#endif	//#if WITH_UE3_NETWORKING

#endif	// __MULTI_THREADED_RING_BUFFER_HEADER__
