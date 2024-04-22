/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#pragma once

using namespace System;
using namespace System::Collections::Generic;

namespace ConsoleInterface
{
	ref class BufferPool;
	ref class Buffer;

	public ref class BufferPoolMgr
	{
	private:
		List<BufferPool^> ^mBufferPools;

	public:
		BufferPoolMgr();

		Buffer^ GetBuffer();
		void ReturnBuffer(Buffer ^Buf);
	};
}
