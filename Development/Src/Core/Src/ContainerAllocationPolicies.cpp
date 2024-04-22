/*=============================================================================
	ContainerAllocationPolicies.cpp: Container allocation policy implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#include "StackTracker.h"


/** Updates an existing call stack trace with new data for this particular call*/
static void SlackTrackerUpdateFn(const FStackTracker::FCallStack& CallStack, void* UserData)
{
	if( UserData != NULL )
	{
		//Callstack has been called more than once, aggregate the data
		FSlackTrackData* NewLCData = static_cast<FSlackTrackData*>(UserData);
		FSlackTrackData* CurrLCData = static_cast<FSlackTrackData*>(CallStack.UserData);

		CurrLCData->NumElements += NewLCData->NumElements;
		CurrLCData->NumSlackElements += NewLCData->NumSlackElements;
		//CurrLCData->Foo += 1;
	}
}


static void SlackTrackerReportFn(const FStackTracker::FCallStack& CallStack, QWORD TotalStackCount, FOutputDevice& Ar)
{
	//Output to a csv file any relevant data
	FSlackTrackData* const LCData = static_cast<FSlackTrackData*>(CallStack.UserData);
	if( LCData != NULL )
	{
		FString UserOutput = LINE_TERMINATOR TEXT(",,,");

		UserOutput += FString::Printf(TEXT("NumElements: %f, NumSlackElements: %f, Curr: %f TotalStackCount: %d "), (FLOAT)LCData->NumElements/TotalStackCount, (FLOAT)LCData->NumSlackElements/TotalStackCount, (FLOAT)LCData->CurrentSlackNum/TotalStackCount, TotalStackCount );

		Ar.Log(*UserOutput);
	}
}





INT DefaultCalculateSlack(INT NumElements,INT NumAllocatedElements,UINT BytesPerElement)
{
	INT Retval;


#if DEBUG_MEMORY_ISSUES
	// Don't use slack when debugging memory issues as they are usually hidden by it.
	return NumElements;
#else
	if(NumElements < NumAllocatedElements)
	{
		// If the container has too much slack, shrink it to exactly fit the number of elements.
		const UINT CurrentSlackElements = NumAllocatedElements-NumElements;
		const UINT CurrentSlackBytes = (NumAllocatedElements-NumElements)*BytesPerElement;
		const UBOOL bTooManySlackBytes = CurrentSlackBytes >= 16384;
		const UBOOL bTooManySlackElements = 3*NumElements < 2*NumAllocatedElements;
		if(	(bTooManySlackBytes || bTooManySlackElements) && (CurrentSlackElements > 64 || !NumElements) ) //  hard coded 64 :-(
		{
			Retval = NumElements;
		}
		else
		{
			Retval = NumAllocatedElements;
		}
	}
	else if(NumElements > 0)
	{
		const INT FirstAllocation = 4;
		if (!NumAllocatedElements && NumElements <= FirstAllocation )
		{
			// 17 is too large for an initial allocation. Many arrays never have more one or two elements.
			Retval = FirstAllocation;
		}
		else
		{
			// Allocate slack for the array proportional to its size.
			check(NumElements < MAXINT);
			Retval = NumElements + 3*NumElements/8 + 16;
			// NumElements and MaxElements are stored in 32 bit signed integers so we must be careful not to overflow here.
			if (NumElements > Retval)
			{
				Retval = MAXINT;
			}
		}
	}
	else
	{
		Retval = 0;
	}
#if XBOX
	if (Retval)
#else
	if (0) // The api supports this on all platforms, however only the xbox supports it internally so we avoid wasting the performance of a few useless calls on other platforms.
#endif
	{
		INT NewNumElements = appMallocQuantizeSize(Retval * BytesPerElement) / BytesPerElement;
		check(NewNumElements >= Retval);
		Retval = NewNumElements;
	}
#endif



#if TRACK_ARRAY_SLACK 
	if( !GSlackTracker )
	{
		GSlackTracker = new FStackTracker( SlackTrackerUpdateFn, SlackTrackerReportFn );
	}
#define SLACK_TRACE_TO_SKIP 4
	FSlackTrackData* const LCData = static_cast<FSlackTrackData*>(appMalloc(sizeof(FSlackTrackData)));
	appMemset(LCData, 0, sizeof(FSlackTrackData));

	LCData->NumElements = NumElements;
	LCData->NumSlackElements = Retval;
	LCData->CurrentSlackNum = NumAllocatedElements-NumElements;

	GSlackTracker->CaptureStackTrace(SLACK_TRACE_TO_SKIP, static_cast<void*>(LCData));
#endif


	return Retval;
}

