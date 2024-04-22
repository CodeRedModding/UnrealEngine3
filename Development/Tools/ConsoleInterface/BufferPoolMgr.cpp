/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "Stdafx.h"
#include "BufferPoolMgr.h"
#include "BufferPool.h"
#include "Buffer.h"

using namespace System::Threading;

namespace ConsoleInterface
{
	BufferPoolMgr::BufferPoolMgr() : mBufferPools(gcnew List<BufferPool^>())
	{
		mBufferPools->Add(gcnew BufferPool());
	}

	Buffer^ BufferPoolMgr::GetBuffer()
	{
		bool bHasPool = false;
		Buffer ^Buf = nullptr;

		Monitor::Enter(mBufferPools);

		try
		{
			for(int i = 0; i < mBufferPools->Count; ++i)
			{
				if(mBufferPools[i]->TryGetBuffer(Buf))
				{
					bHasPool = true;
					break;
				}
			}

			if(!bHasPool)
			{
				BufferPool ^BufPool = gcnew BufferPool();
				mBufferPools->Add(BufPool);

				BufPool->TryGetBuffer(Buf);
			}
		}
		finally
		{
			Monitor::Exit(mBufferPools);
		}

		return Buf;
	}

	void BufferPoolMgr::ReturnBuffer(Buffer^ Buf)
	{
		Monitor::Enter(mBufferPools);

		try
		{
			for(int i = 0; i < mBufferPools->Count; ++i)
			{
				if(mBufferPools[i]->OwnsBuffer(Buf))
				{
					mBufferPools[i]->ReturnBuffer(Buf);
					break;
				}
			}
		}
		finally
		{
			Monitor::Exit(mBufferPools);
		}
	}
}
