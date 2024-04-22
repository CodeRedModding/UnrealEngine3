/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "Stdafx.h"
#include "Buffer.h"

using namespace System::Threading;

namespace ConsoleInterface
{
	Buffer::Buffer(BufferPool ^Owner, ArraySegment<Byte> Buf)
	{
		if(Owner == nullptr)
		{
			throw gcnew ArgumentNullException(L"Owner");
		}

		mOwner = Owner;
		mBuffer = Buf;
	}

	array<Byte>^ Buffer::Array::get()
	{
		return mBuffer.Array;
	}

	int Buffer::Count::get()
	{
		return mBuffer.Count;
	}

	int Buffer::Offset::get()
	{
		return mBuffer.Offset;
	}

	Byte Buffer::default::get(int Index)
	{
		return mBuffer.Array[Index + mBuffer.Offset];
	}

	Buffer::operator ArraySegment<Byte>()
	{
		return mBuffer;
	}

	int Buffer::AddRef()
	{
		return Interlocked::Increment(mRefCount);
	}

	int Buffer::Release()
	{
		return Interlocked::Decrement(mRefCount);
	}
}
