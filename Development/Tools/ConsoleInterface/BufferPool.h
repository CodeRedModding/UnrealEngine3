/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#pragma once

using namespace System;
using namespace System::Collections::Generic;

namespace ConsoleInterface
{
	ref class Buffer;

	public ref class BufferPool
	{
	private:
		array<Byte> ^mBuffer;
		Stack<Buffer^> ^mBufferPool;

	public:
		BufferPool();

		bool TryGetBuffer(Buffer ^%OutPool);
		void ReturnBuffer(Buffer ^Pool);
		bool OwnsBuffer(Buffer ^Buf);
	};
}
