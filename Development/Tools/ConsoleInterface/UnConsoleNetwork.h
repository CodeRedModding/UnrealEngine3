// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#ifndef _UNCONSOLENETWORK_H_
#define _UNCONSOLENETWORK_H_

// don't include this on consoles
#if !CONSOLE && !PLATFORM_MACOSX && WITH_UE3_NETWORKING

#pragma pack(push,8)

#include <WinSock2.h>
#include <objbase.h>
#include <assert.h>
#include <stdio.h>
#include <process.h>
#include <string>
#include <map>
#include <vector>
#include <comutil.h>
#include <time.h>

using namespace std;

#include "..\..\Src\Engine\Inc\UnConsoleTools.h"

/// Converts wide-char string to ansi string
inline string ToString(const wstring& Src)
{
	string Dest;
	Dest.assign(Src.begin(), Src.end());
	return Dest;
}

/// Converts ansi string to wide-char string
inline wstring ToWString(const string& Src)
{
	wstring Dest;
	Dest.assign(Src.begin(), Src.end());
	return Dest;
}

/**
 *	Simple byte stream converter for basic types: int, string, etc.
 */
class CByteStreamConverter
{
public:
	/// Retrieves int from buffer assuming big endian is used
	static unsigned int LoadIntBE(char*& Data, int& DataSize)
	{
		if(DataSize >= 4)
		{
			const unsigned int Value =
				((unsigned char) Data[0]) << 24 |
				((unsigned char) Data[1]) << 16 |
				((unsigned char) Data[2]) << 8 |
				((unsigned char) Data[3]);

			Data += 4;
			DataSize -= 4;

			return Value;
		}

		return 0;
	}

	/// Retrieves int from buffer assuming little endian is used
	static unsigned int LoadIntLE(char*& Data, int& DataSize)
	{
		if(DataSize >= 4)
		{
			const unsigned int Value =
				((unsigned char) Data[3]) << 24 |
				((unsigned char) Data[2]) << 16 |
				((unsigned char) Data[1]) << 8 |
				((unsigned char) Data[0]);

			Data += 4;
			DataSize -= 4;

			return Value;
		}

		return 0;
	}

	/// Retrieves string from buffer assuming big endian is used
	static string LoadStringBE(char*& Data, int& DataSize)
	{
		int StringLen = LoadIntBE(Data, DataSize);

		if(StringLen > DataSize)
		{
			StringLen = DataSize;
		}

		if(StringLen > 0)
		{
			char* Buffer = new char[StringLen + 1];
			memcpy(Buffer, Data, StringLen);
			Buffer[StringLen] = '\0';

			Data += StringLen;
			DataSize -= StringLen;

			string Result = Buffer;
			delete[] Buffer;

			return Result;
		}

		return "";
	}

	/// Retrieves string from buffer assuming big endian is used
	static string LoadStringLE(char*& Data, int& DataSize)
	{
		int StringLen = LoadIntLE(Data, DataSize);

		if(StringLen > DataSize)
		{
			StringLen = DataSize;
		}

		if(StringLen > 0)
		{
			char* Buffer = new char[StringLen + 1];
			memcpy(Buffer, Data, StringLen);
			Buffer[StringLen] = '\0';

			Data += StringLen;
			DataSize -= StringLen;

			string Result = Buffer;
			delete[] Buffer;
			return Result;
		}

		return "";
	}

	static void StoreIntBE(char* Data, unsigned int Value)
	{
		Data[0] = (char) (Value >> 24) & 255;
		Data[1] = (char) (Value >> 16) & 255;
		Data[2] = (char) (Value >> 8) & 255;
		Data[3] = (char) (Value & 255);
	}
};

/**
* This is the Windows version of a critical section. It uses an aggregate
* CRITICAL_SECTION to implement its locking.
*/
class FCriticalSectionLite
{
	/**
	* The windows specific critical section
	*/
	CRITICAL_SECTION CriticalSection;

public:
	/**
	* Constructor that initializes the aggregated critical section
	*/
	FORCEINLINE FCriticalSectionLite(void)
	{
		InitializeCriticalSection(&CriticalSection);
	}

	/**
	* Destructor cleaning up the critical section
	*/
	FORCEINLINE virtual ~FCriticalSectionLite(void)
	{
		DeleteCriticalSection(&CriticalSection);
	}

	/**
	* Locks the critical section
	*/
	FORCEINLINE void Lock(void)
	{
		EnterCriticalSection(&CriticalSection);
	}

	/**
	* Releases the lock on the critical seciton
	*/
	FORCEINLINE void Unlock(void)
	{
		LeaveCriticalSection(&CriticalSection);
	}
};

template <class T>
class FReferenceCountPtr;

/**
 * Base class for reference counted targets.
 */
template <class T>
class FRefCountedTarget
{
	friend class FReferenceCountPtr<T>;

private:
	volatile LONG ReferenceCount;

	/**
	* Increments the reference count.
	*
	* @return	The new reference count.
	*/
	inline LONG IncrementReferenceCount()
	{
		return InterlockedIncrement(&ReferenceCount);
	}

	/**
	* Decrements the reference count.
	*
	* @return	The new reference count.
	*/
	inline LONG DecrementReferenceCount()
	{
		return InterlockedDecrement(&ReferenceCount);
	}

	/**
	* Returns the current reference count.
	*/
	inline LONG GetReferenceCount()
	{
		return InterlockedAnd(&ReferenceCount, ~0);
	}

public:
	FRefCountedTarget() : ReferenceCount(0)
	{

	}

	virtual ~FRefCountedTarget()
	{

	}
};

/**
 * Smart pointer to reference counted targets.
 */
template <class T>
class FReferenceCountPtr
{
private:
	FRefCountedTarget<T>* Target;

	/**
	* Increments the reference count.
	*
	* @return	The new reference count.
	*/
	LONG IncrementReferenceCount()
	{
		LONG Count = 0;

		if(Target)
		{
			Count = Target->IncrementReferenceCount();
		}

		return Count;
	}

	/**
	* Decrements the reference count.
	*
	* @return	The new reference count.
	*/
	LONG DecrementReferenceCount()
	{
		LONG Count = 0;

		if(Target)
		{
			Count = Target->DecrementReferenceCount();

			if(Count <= 0)
			{
				delete Target;
			}

			Target = NULL;
		}

		return Count;
	}

public:
	FReferenceCountPtr() : Target(NULL)
	{

	}

	FReferenceCountPtr(T* NewTarget) : Target(NewTarget)
	{
		IncrementReferenceCount();
	}

	FReferenceCountPtr(const FReferenceCountPtr<T>& Ptr) : Target(NULL)
	{
		operator=(Ptr);
	}

	virtual ~FReferenceCountPtr()
	{
		DecrementReferenceCount();
	}

	inline TARGETHANDLE GetHandle()
	{
		return (TARGETHANDLE)Target;
	}

	inline T* operator*()
	{
		return (T*)Target;
	}

	inline T* operator->()
	{
		return (T*)Target;
	}

	inline operator bool() const
	{
		return Target != NULL;
	}

	FReferenceCountPtr<T>& operator=(T* Ptr)
	{
		DecrementReferenceCount();
		Target = Ptr;
		IncrementReferenceCount();

		return *this;
	}

	FReferenceCountPtr<T>& operator=(const FReferenceCountPtr<T>& Ptr)
	{
		DecrementReferenceCount();
		Target = Ptr.Target;
		IncrementReferenceCount();

		return *this;
	}

	inline bool operator==(const FReferenceCountPtr<T>& Ptr) const
	{
		return Target == Ptr.Target;
	}
};

#endif // !CONSOLE && !PLATFORM_MACOSX && WITH_UE3_NETWORKING

#endif // _UNCONSOLENETWORK_H_
