/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#pragma once

using namespace System;

namespace ConsoleInterface
{
	ref class BufferPool;

	public ref class Buffer
	{
	private:
		BufferPool ^mOwner;
		ArraySegment<Byte> mBuffer;
		int mRefCount;

	internal:
		Buffer(BufferPool ^Owner, ArraySegment<Byte> Buf);

	public:
		property array<Byte>^ Array
		{
			array<Byte>^ get();
		}

		property int Offset
		{
			int get();
		}

		property int Count
		{
			int get();
		}

		property Byte default[int]
		{
			Byte get(int Index);
		}

	public:
		operator ArraySegment<Byte>();
		int AddRef();
		int Release();
	};
}
