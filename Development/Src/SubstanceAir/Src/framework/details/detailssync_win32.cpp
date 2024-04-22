//! @file detailssync_win32.cpp
//! @brief Substance Air Rendering synchronization API Win32 implementation.
//! @author Christophe Soum - Allegorithmic
//! @date 20110701
//! @copyright Allegorithmic. All rights reserved.


#include "framework/details/detailssync.h"


SubstanceAir::Details::Sync::thread::thread() :
	mThread(NULL),
	mParameters(NULL)
{
}


SubstanceAir::Details::Sync::thread::~thread()
{
	release();
}


void SubstanceAir::Details::Sync::thread::release()
{
	if (mThread)
	{
		join();
		delete[] mParameters;
		CloseHandle(mThread);
	}
}


SubstanceAir::Details::Sync::thread::thread(const thread& t)
{
	*this = t;
}


SubstanceAir::Details::Sync::thread& 
SubstanceAir::Details::Sync::thread::operator=(const thread& t)
{
	release();
	mThread = t.mThread;
	mParameters = t.mParameters;
	const_cast<thread&>(t).mThread = NULL;
	const_cast<thread&>(t).mParameters = NULL;
	return *this;
}


void SubstanceAir::Details::Sync::thread::start(
	LPTHREAD_START_ROUTINE startRoutine)
{
	mThread = CreateThread(
		NULL,
		0,
		startRoutine,
		mParameters,
		0,
		NULL);
}


void SubstanceAir::Details::Sync::thread::join()
{
	if (mThread!=NULL)
	{
		WaitForSingleObject(mThread,INFINITE);
	}
}


SubstanceAir::Details::Sync::mutex::mutex() :
	mMutex(CreateMutex(NULL,0,NULL))
{
}


SubstanceAir::Details::Sync::mutex::~mutex()
{
	CloseHandle(mMutex);
}


void SubstanceAir::Details::Sync::mutex::lock()
{
	WaitForSingleObject(mMutex,INFINITE);
}


bool SubstanceAir::Details::Sync::mutex::try_lock()
{
	return WaitForSingleObject(mMutex,0)!=WAIT_TIMEOUT;
}


void SubstanceAir::Details::Sync::mutex::unlock()
{
	ReleaseMutex(mMutex);
}


SubstanceAir::Details::Sync::mutex::scoped_lock::scoped_lock(mutex& m) :
	mMutex(m)
{
	mMutex.lock();
}


SubstanceAir::Details::Sync::mutex::scoped_lock::~scoped_lock()
{
	mMutex.unlock();
}


SubstanceAir::Details::Sync::condition_variable::condition_variable() :
	mSemaphore(CreateSemaphore(NULL,0,1,NULL))
{
}


SubstanceAir::Details::Sync::condition_variable::~condition_variable()
{
	CloseHandle(mSemaphore);
}


void SubstanceAir::Details::Sync::condition_variable::notify_one()
{
	ReleaseSemaphore(mSemaphore,1,NULL);
}


void SubstanceAir::Details::Sync::condition_variable::wait(mutex::scoped_lock& l)
{
	SignalObjectAndWait(l.mMutex.mMutex,mSemaphore,INFINITE,FALSE);
}
