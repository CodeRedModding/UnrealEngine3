//! @file detailssync.h
//! @brief Substance Air Rendering synchronization API definitions.
//! @author Christophe Soum - Allegorithmic
//! @date 20110701
//! @copyright Allegorithmic. All rights reserved.
//!

#ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSSYNC_H
#define _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSSYNC_H

#ifdef _XBOX
#include <xtl.h>
#define AIR_SYNC_THREADQUALIFIER __stdcall
#else // ifdef _XBOX

// Define WIN32_LEAN_AND_MEAN to exclude rarely-used services from windows headers.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define AIR_SYNC_THREADQUALIFIER WINAPI
#endif // ifdef _XBOX

#include <utility>

#include "SubstanceAirUncopyable.h"

namespace SubstanceAir
{
namespace Details
{
namespace Sync
{

template <class F,class A1>
static DWORD AIR_SYNC_THREADQUALIFIER threadStartArg1(LPVOID ptr)
{
	std::pair<F,A1> p = *(std::pair<F,A1>*)ptr;
	p.first(p.second);
	return 0;
}


template <class F>
static DWORD AIR_SYNC_THREADQUALIFIER threadStartVoid(LPVOID ptr)
{
	F f = *(F*)ptr;
	f();
	return 0;
}


//! @brief See boost::thread for documentation
class thread
{
public:
	thread();
	~thread();
	thread(const thread&);

	template <class F,class A1>
	thread(F f,A1 a1)
	{
		std::pair<F,A1> p(f,a1);
		mParameters = new char[sizeof(p)];
		memcpy(mParameters,&p,sizeof(p)); 
		start(threadStartArg1<F,A1>);
	}
	
	template <class F>
	explicit thread(F f)
	{
		mParameters = new char[sizeof(f)];
		memcpy(mParameters,&f,sizeof(f));
		start(threadStartVoid<F>);
	}
	
	thread& operator=(const thread&);

	void join();

protected:
	HANDLE mThread;
	char* mParameters;
	
	void start(DWORD (AIR_SYNC_THREADQUALIFIER*startRoutine)(LPVOID));
	void release();
};


//! @brief See boost::thread for documentation
class mutex : Uncopyable
{
public:
	mutex();
	~mutex();

	void lock();
	bool try_lock();
	void unlock();

	class scoped_lock : Uncopyable
	{
	public:
		scoped_lock(mutex&);
		~scoped_lock();
	protected:
		mutex &mMutex;
		friend class condition_variable;
	};

protected:
	HANDLE mMutex;
	friend class condition_variable;
};


//! @brief See boost::thread for documentation
class condition_variable
{
public:
	condition_variable();
	~condition_variable();

	void notify_one();

	void wait(mutex::scoped_lock&);
	
protected:
	HANDLE mSemaphore;
};


} // namespace Sync
} // namespace Details
} // namespace SubstanceAir


// Interlocked (atomics) functions defines

#if defined(ANDROID)
	#include <sched.h>
#elif defined(_XBOX)
	#include <Xtl.h>
#elif defined(__INTEL_COMPILER) || defined(_MSC_VER)
	extern "C" long _InterlockedExchange(long volatile *, long);
	extern "C" long _InterlockedExchangeAdd(long volatile *, long);
	#pragma intrinsic(_InterlockedExchange,_InterlockedExchangeAdd)
	#if defined(_M_IA64) || defined (_M_X64)
		extern "C" __int64 _InterlockedExchange64(__int64 volatile *,__int64);
		extern "C" __int64 _InterlockedExchangeAdd64(__int64 volatile *,__int64);
		#pragma intrinsic(_InterlockedExchange64,_InterlockedExchangeAdd64)
	#endif
#elif defined(SN_TARGET_PS3)
	#include <cell/atomic.h>
#elif defined(SN_TARGET_PSP2)
	#include <sce_atomic.h>
#elif defined(__APPLE__)
	#include <libkern/OSAtomic.h>
#endif


namespace SubstanceAir
{
namespace Details
{
namespace Sync
{
	//! @brief Interlocked pointer exchange
	//! @param dest Pointer to the destination value. 
	//! @param val The new value
	//! @return Return the original value
	inline void* interlockedSwapPointer(void*volatile* dest, void* val)
	{
		#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
			#if defined(_M_IA64) || defined (_M_X64)
				return (void*)_InterlockedExchange64(
					(volatile long long *)dest,(__int64)val);
			#else
				return (void*)_InterlockedExchange(
					(volatile long *)dest,(long)val);
			#endif
		#elif defined(__GNUC__)
			#if defined(__APPLE__)
				#if defined(__LP64__)
					unsigned long long oldValue = 0;
					do 
					{ 
						oldValue = (unsigned long long)*dest;
					}
					while (OSAtomicCompareAndSwap64(
						(unsigned long long)oldValue,
						(unsigned long long)val,
						(long long*)dest) == false);
					return (void*)oldValue;
				#else
					unsigned int oldValue = 0;
					do 
					{ 
						oldValue = (unsigned int)*dest; 
					}
					while (OSAtomicCompareAndSwap32(
						(unsigned int)oldValue,
						(unsigned int)val,
						(int*)dest) == false);
					return (void*)oldValue;
				#endif
			#else
				#ifdef __LP64__
					return (void*)__sync_lock_test_and_set(
						(volatile unsigned long long*)dest,
						(unsigned long long)val);
				#else
					return (void*)__sync_lock_test_and_set(
						(volatile unsigned int*)dest,
						(unsigned int)val);	
				#endif
			#endif
		#elif defined(SN_TARGET_PSP2)
			return (void*)sceAtomicExchange32((volatile int*)dest,(int)val);
		#endif
	}
	
} // namespace Sync
} // namespace Details
} // namespace SubstanceAir

#endif // _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSSYNC_H
