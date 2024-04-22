/*=============================================================================
StackTracker.h: Stack Tracking within Unreal Engine.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef _STACK_TRACKER_H_
#define _STACK_TRACKER_H_

#include "UMemoryDefines.h"


/** Whether array slack is being tracked. */
#define TRACK_ARRAY_SLACK 0


struct FSlackTrackData
{
	//// Because this code is used for a number of metric gathering tasks that are all not just
	//// count the total number / avg per frame   We will just add in the specific data that we want to 
	//// use elsewhere

	QWORD NumElements;

	/** NumSlackElements in DefaultCalculateSlack call */
	QWORD NumSlackElements;

	//QWORD Foo;

	QWORD CurrentSlackNum;

	// maybe store off policy also

};



/**
 * Stack tracker. Used to identify callstacks at any point in the codebase.
 */
struct FStackTracker
{
public:
	/** Maximum number of backtrace depth. */
	static const INT MAX_BACKTRACE_DEPTH = 50;
	/** Helper structure to capture callstack addresses and stack count. */
	struct FCallStack
	{
		/** Stack count, aka the number of calls to CalculateStack */
		SQWORD StackCount;
		/** Program counter addresses for callstack. */
		QWORD Addresses[MAX_BACKTRACE_DEPTH];
		/** User data to store with the stack trace for later use */
		void* UserData;
	};

	/** Used to optionally update the information currently stored with the callstack */
	typedef void (*StackTrackerUpdateFn)( const FCallStack& CallStack, void* UserData);
	/** Used to optionally report information based on the current stack */
	typedef void (*StackTrackerReportFn)( const FCallStack& CallStack, QWORD TotalStackCount, FOutputDevice& Ar );

	/** Constructor, initializing all member variables */
	FStackTracker(StackTrackerUpdateFn InUpdateFn = NULL, StackTrackerReportFn InReportFn = NULL, UBOOL bInIsEnabled = FALSE)
		:	bAvoidCapturing(FALSE)
		,	bIsEnabled(bInIsEnabled)
		,	StartFrameCounter(0)
		,	StopFrameCounter(0)
		,   UpdateFn(InUpdateFn)
		,   ReportFn(InReportFn)
	{}

	/**
	 * Captures the current stack and updates stack tracking information.
	 * optionally stores a user data pointer that the tracker will take ownership of and delete upon reset
	 * you must allocate the memory with appMalloc()
	 */
	void CaptureStackTrace( INT EntriesToIgnore = 2, void* UserData = NULL );

	/**
	 * Dumps capture stack trace summary to the passed in log.
	 */
	void DumpStackTraces( INT StackThreshold, FOutputDevice& Ar );

	/** Resets stack tracking. Deletes all user pointers passed in via CaptureStackTrace() */
	void ResetTracking();

	/** Toggles tracking. */
	void ToggleTracking();

private:
	/** Compare function, sorting callstack by stack count in descending order. */
	IMPLEMENT_COMPARE_CONSTREF_( SQWORD, FCallStack, StackTracker, { return B.StackCount - A.StackCount; } );
	/** Array of unique callstacks. */
	TArray<FCallStack> CallStacks;
	/** Mapping from callstack CRC to index in callstack array. */
	TMap<DWORD,INT> CRCToCallStackIndexMap;
	/** Whether we are currently capturing or not, used to avoid re-entrancy. */
	UBOOL bAvoidCapturing;
	/** Whether stack tracking is enabled. */
	UBOOL bIsEnabled;
	/** Frame counter at the time tracking was enabled. */
	QWORD StartFrameCounter;
	/** Frame counter at the time tracking was disabled. */
	QWORD StopFrameCounter;

	/** Used to optionally update the information currently stored with the callstack */
	StackTrackerUpdateFn UpdateFn;
	/** Used to optionally report information based on the current stack */
	StackTrackerReportFn ReportFn;
};

struct FScriptStackTracker
{
private:
	/** Maximum number of backtrace depth. */
	static const INT MAX_BACKTRACE_DEPTH = 50;

	/** Helper structure to capture callstack addresses and stack count. */
	struct FCallStack
	{
		/** Stack count, aka the number of calls to CalculateStack */
		SQWORD StackCount;
		/** String representation of script callstack. */
		FString StackTrace;
	};

	/** Compare function, sorting callstack by stack count in descending order. */
	IMPLEMENT_COMPARE_CONSTREF_( SQWORD, FCallStack, StackTracker, { return B.StackCount - A.StackCount; } );

	/** Array of unique callstacks. */
	TArray<FCallStack> CallStacks;
	/** Mapping from callstack CRC to index in callstack array. */
	TMap<DWORD,INT> CRCToCallStackIndexMap;
	/** Whether we are currently capturing or not, used to avoid re-entrancy. */
	UBOOL bAvoidCapturing;
	/** Whether stack tracking is enabled. */
	UBOOL bIsEnabled;
	/** Frame counter at the time tracking was enabled. */
	QWORD StartFrameCounter;
	/** Frame counter at the time tracking was disabled. */
	QWORD StopFrameCounter;

public:
	/** Constructor, initializing all member variables */
	FScriptStackTracker( UBOOL bInIsEnabled = FALSE )
		:	bAvoidCapturing(FALSE)
		,	bIsEnabled(bInIsEnabled)
		,	StartFrameCounter(0)
		,	StopFrameCounter(0)
	{}

	/**
	 * Captures the current stack and updates stack tracking information.
	 */
	void CaptureStackTrace(const FFrame* StackFrame, INT EntriesToIgnore = 0);

	/**
	 * Dumps capture stack trace summary to the passed in log.
	 */
	void DumpStackTraces( INT StackThreshold, FOutputDevice& Ar );

	/** Resets stack tracking. */
	void ResetTracking();

	/** Toggles tracking. */
	void ToggleTracking();
};



#if TRACK_ARRAY_SLACK
extern FStackTracker* GSlackTracker;
#endif



#endif //_STACK_TRACKER_H_

