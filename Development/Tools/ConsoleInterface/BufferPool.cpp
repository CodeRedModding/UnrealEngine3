/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "Stdafx.h"
#include "BufferPool.h"
#include "Buffer.h"

#define BUFFER_SIZE 1024 * 1024 * 2 // 2mb
#define BUFFER_SEGMENT_SIZE 2048 // 2kb

using namespace System::Threading;

namespace ConsoleInterface
{
	BufferPool::BufferPool() : mBuffer(gcnew array<Byte>(BUFFER_SIZE)), mBufferPool(gcnew Stack<Buffer^>())
	{
		for(int i = 0; i < BUFFER_SIZE; i += BUFFER_SEGMENT_SIZE)
		{
			mBufferPool->Push(gcnew Buffer(this, ArraySegment<Byte>(mBuffer, i, BUFFER_SEGMENT_SIZE)));
		}
	}

	bool BufferPool::TryGetBuffer(Buffer ^%OutPool)
	{
		bool bHasBuffer = false;
		OutPool = nullptr;

		Monitor::Enter(mBufferPool);

		try
		{
			if(mBufferPool->Count > 0)
			{
				OutPool = mBufferPool->Pop();
				bHasBuffer = true;
			}
		}
		finally
		{
			Monitor::Exit(mBufferPool);
		}

		return bHasBuffer;
	}

	void BufferPool::ReturnBuffer(Buffer ^Pool)
	{
		if(Pool->Array == mBuffer)
		{
			Monitor::Enter(mBufferPool);

			try
			{
				mBufferPool->Push(Pool);
			}
			finally
			{
				Monitor::Exit(mBufferPool);
			}
		}
		else
		{
			throw gcnew ArgumentException(L"Pool");
		}
	}

	bool BufferPool::OwnsBuffer(Buffer ^Buf)
	{
		return Buf->Array == mBuffer;
	}
}
