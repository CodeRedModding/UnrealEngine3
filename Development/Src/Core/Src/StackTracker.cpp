/*=============================================================================
StackTracker.cpp: Stack Tracking within Unreal Engine.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "CorePrivate.h"

#include "StackTracker.h"



#if TRACK_ARRAY_SLACK
FStackTracker* GSlackTracker = NULL;
#endif

/**
 * Captures the current stack and updates stack tracking information.
 * optionally stores a user data pointer that the tracker will take ownership of and delete upon reset
 * you must allocate the memory with appMalloc()
 */
void FStackTracker::CaptureStackTrace(INT EntriesToIgnore /*=2*/, void* UserData /*=NULL*/)
{
	// Avoid re-rentrancy as the code uses TArray/TMap.
	if( !bAvoidCapturing && bIsEnabled )
	{
		// Scoped TRUE/ FALSE.
		bAvoidCapturing = TRUE;

		// Capture callstack and create CRC.
		QWORD* FullBackTrace = NULL;
		FullBackTrace = static_cast<QWORD*>(appAlloca((MAX_BACKTRACE_DEPTH + EntriesToIgnore) * sizeof(QWORD)));

// @todo flash implement this to empty
#if !FLASH
		appCaptureStackBackTrace( FullBackTrace, MAX_BACKTRACE_DEPTH + EntriesToIgnore );
#endif
		// Skip first NUM_ENTRIES_TO_SKIP entries as they are inside this code
		QWORD* BackTrace = &FullBackTrace[EntriesToIgnore];
		DWORD CRC = appMemCrc( BackTrace, MAX_BACKTRACE_DEPTH * sizeof(QWORD), 0 );
        
		// Use index if found
		INT* IndexPtr = CRCToCallStackIndexMap.Find( CRC );
        
		if( IndexPtr )
		{
			// Increase stack count for existing callstack.
			CallStacks(*IndexPtr).StackCount++;
			if (UpdateFn)
			{
				UpdateFn(CallStacks(*IndexPtr), UserData);
			}

			//We can delete this since the user gives ownership at the beginning of this call
			//and had a chance to update their data inside the above callback
			if (UserData)
			{
				appFree(UserData);
			}
		}
		// Encountered new call stack, add to array and set index mapping.
		else
		{
			// Add to array and set mapping for future use.
			INT Index = CallStacks.Add();
			CRCToCallStackIndexMap.Set( CRC, Index );

			// Fill in callstack and count.
			FCallStack& CallStack = CallStacks(Index);
			appMemcpy( CallStack.Addresses, BackTrace, sizeof(QWORD) * MAX_BACKTRACE_DEPTH );
			CallStack.StackCount = 1;
			CallStack.UserData = UserData;
		}

		// We're done capturing.
		bAvoidCapturing = FALSE;
	}
}

/**
 * Dumps capture stack trace summary to the passed in log.
 */
void FStackTracker::DumpStackTraces( INT StackThreshold, FOutputDevice& Ar )
{
	// Avoid distorting results while we log them.
	check( !bAvoidCapturing );
	bAvoidCapturing = TRUE;

	// Make a copy as sorting causes index mismatch with TMap otherwise.
	TArray<FCallStack> SortedCallStacks = CallStacks;
	// Sort callstacks in descending order by stack count.
	Sort<USE_COMPARE_CONSTREF(FCallStack,StackTracker)>( SortedCallStacks.GetTypedData(), SortedCallStacks.Num() );

	// Iterate over each callstack to get total stack count.
	QWORD TotalStackCount = 0;
	for( INT CallStackIndex=0; CallStackIndex<SortedCallStacks.Num(); CallStackIndex++ )
	{
		const FCallStack& CallStack = SortedCallStacks(CallStackIndex);
		TotalStackCount += CallStack.StackCount;
	}

	// Calculate the number of frames we captured.
	INT FramesCaptured = 0;
	if( bIsEnabled )
	{
		FramesCaptured = GFrameCounter - StartFrameCounter;
	}
	else
	{
		FramesCaptured = StopFrameCounter - StartFrameCounter;
	}

	// Log quick summary as we don't log each individual so totals in CSV won't represent real totals.
	Ar.Logf(TEXT("Captured %i unique callstacks totalling %i function calls over %i frames, averaging %5.2f calls/frame, Avg Per Frame"), SortedCallStacks.Num(), (int)TotalStackCount, FramesCaptured, (FLOAT) TotalStackCount / FramesCaptured);

	// Iterate over each callstack and write out info in human readable form in CSV format
	for( INT CallStackIndex=0; CallStackIndex<SortedCallStacks.Num(); CallStackIndex++ )
	{
		const FCallStack& CallStack = SortedCallStacks(CallStackIndex);

		// Avoid log spam by only logging above threshold.
		if( CallStack.StackCount > StackThreshold )
		{
			// First row is stack count.
			FString CallStackString = appItoa((INT)CallStack.StackCount);
			CallStackString += FString::Printf( TEXT(",%5.2f"), static_cast<FLOAT>(CallStack.StackCount)/static_cast<FLOAT>(FramesCaptured) );
			

			// Iterate over all addresses in the callstack to look up symbol name.
			for( INT AddressIndex=0; AddressIndex<ARRAY_COUNT(CallStack.Addresses) && CallStack.Addresses[AddressIndex]; AddressIndex++ )
			{
				ANSICHAR AddressInformation[512];
				AddressInformation[0] = 0;
				appProgramCounterToHumanReadableString( CallStack.Addresses[AddressIndex], AddressInformation, ARRAY_COUNT(AddressInformation)-1, VF_DISPLAY_FILENAME );
				CallStackString = CallStackString + LINE_TERMINATOR TEXT(",,,") + FString(AddressInformation);
			}

			// Finally log with ',' prefix so "Log:" can easily be discarded as row in Excel.
			Ar.Logf(TEXT(",%s"),*CallStackString);
            
			//Append any user information before moving on to the next callstack
			if (ReportFn)
			{
				ReportFn(CallStack, CallStack.StackCount, Ar);
			}
		}
	}

	// Done logging.
	bAvoidCapturing = FALSE;
}

/** Resets stack tracking. Deletes all user pointers passed in via CaptureStackTrace() */
void FStackTracker::ResetTracking()
{
	check(!bAvoidCapturing);
	CRCToCallStackIndexMap.Empty();

	//Clean up any user data 
	for(INT i=0; i<CallStacks.Num(); i++)
	{
		if (CallStacks(i).UserData)
		{
			appFree(CallStacks(i).UserData);
		}
	}

	CallStacks.Empty();

    //Reset the markers
	StartFrameCounter = GFrameCounter;
	StopFrameCounter = GFrameCounter;
}

/** Toggles tracking. */
void FStackTracker::ToggleTracking()
{
	bIsEnabled = !bIsEnabled;
	// Enabled
	if( bIsEnabled )
	{
		debugf(TEXT("Stack tracking is now enabled."));
		StartFrameCounter = GFrameCounter;
	}
	// Disabled.
	else
	{
		StopFrameCounter = GFrameCounter;
		debugf(TEXT("Stack tracking is now disabled."));
	}
}

/**
* Captures the current stack and updates stack tracking information.
*/
void FScriptStackTracker::CaptureStackTrace(const FFrame* StackFrame, INT EntriesToIgnore /*=0*/)
{
	// Avoid re-rentrancy as the code uses TArray/TMap.
	if( !bAvoidCapturing && bIsEnabled )
	{
		// Scoped TRUE/ FALSE.
		bAvoidCapturing = TRUE;

		// Capture callstack and create CRC.

		FString StackTrace = StackFrame->GetStackTrace();
		DWORD CRC = appMemCrc( *StackTrace, StackTrace.Len(), 0 );

		// Use index if found
		INT* IndexPtr = CRCToCallStackIndexMap.Find( CRC );
		if( IndexPtr )
		{
			// Increase stack count for existing callstack.
			CallStacks(*IndexPtr).StackCount++;
		}
		// Encountered new call stack, add to array and set index mapping.
		else
		{
			FCallStack NewCallStack;
			NewCallStack.StackCount = 1;
			NewCallStack.StackTrace = StackTrace;
			INT Index = CallStacks.AddItem(NewCallStack);
			CRCToCallStackIndexMap.Set( CRC, Index );
		}

		// We're done capturing.
		bAvoidCapturing = FALSE;
	}
}

/**
* Dumps capture stack trace summary to the passed in log.
*/
void FScriptStackTracker::DumpStackTraces( INT StackThreshold, FOutputDevice& Ar )
{
	// Avoid distorting results while we log them.
	check( !bAvoidCapturing );
	bAvoidCapturing = TRUE;

	// Make a copy as sorting causes index mismatch with TMap otherwise.
	TArray<FCallStack> SortedCallStacks = CallStacks;
	// Sort callstacks in descending order by stack count.
	Sort<USE_COMPARE_CONSTREF(FCallStack,StackTracker)>( SortedCallStacks.GetTypedData(), SortedCallStacks.Num() );

	// Iterate over each callstack to get total stack count.
	QWORD TotalStackCount = 0;
	for( INT CallStackIndex=0; CallStackIndex<SortedCallStacks.Num(); CallStackIndex++ )
	{
		const FCallStack& CallStack = SortedCallStacks(CallStackIndex);
		TotalStackCount += CallStack.StackCount;
	}

	// Calculate the number of frames we captured.
	INT FramesCaptured = 0;
	if( bIsEnabled )
	{
		FramesCaptured = GFrameCounter - StartFrameCounter;
	}
	else
	{
		FramesCaptured = StopFrameCounter - StartFrameCounter;
	}

	// Log quick summary as we don't log each individual so totals in CSV won't represent real totals.
	Ar.Logf(TEXT("Captured %i unique callstacks totalling %i function calls over %i frames, averaging %5.2f calls/frame"), SortedCallStacks.Num(), (int)TotalStackCount, FramesCaptured, (FLOAT) TotalStackCount / FramesCaptured);

	// Iterate over each callstack and write out info in human readable form in CSV format
	for( INT CallStackIndex=0; CallStackIndex<SortedCallStacks.Num(); CallStackIndex++ )
	{
		const FCallStack& CallStack = SortedCallStacks(CallStackIndex);

		// Avoid log spam by only logging above threshold.
		if( CallStack.StackCount > StackThreshold )
		{
			// First row is stack count.
			FString CallStackString = appItoa((INT)CallStack.StackCount);
			CallStackString += LINE_TERMINATOR;
			CallStackString += CallStack.StackTrace;

			// Finally log with ',' prefix so "Log:" can easily be discarded as row in Excel.
			Ar.Logf(TEXT(",%s"),*CallStackString);
		}
	}

	// Done logging.
	bAvoidCapturing = FALSE;
}

/** Resets stack tracking. */
void FScriptStackTracker::ResetTracking()
{
	check(!bAvoidCapturing);
	CRCToCallStackIndexMap.Empty();
	CallStacks.Empty();

    //Reset the markers
	StartFrameCounter = GFrameCounter;
	StopFrameCounter = GFrameCounter;
}

/** Toggles tracking. */
void FScriptStackTracker::ToggleTracking()
{
	bIsEnabled = !bIsEnabled;
	// Enabled
	if( bIsEnabled )
	{
		debugf(TEXT("Script stack tracking is now enabled."));
		StartFrameCounter = GFrameCounter;
	}
	// Disabled.
	else
	{
		StopFrameCounter = GFrameCounter;
		debugf(TEXT("Script stack tracking is now disabled."));
	}
}
