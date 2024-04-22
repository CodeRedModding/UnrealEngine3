/*=============================================================================
	PerfCounter.cpp: Performance counter class implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

/* ==========================================================================================================
	FPerformanceData
========================================================================================================== */

/**
 * Default Constructor
 */
FPerformanceData::FPerformanceData()
: TotalTime(0.f), MinTime(FLT_MAX), MaxTime(0.f), Count(0)
{
}

/**
 * Integrates data from the event being tracked by this struct.
 *
 * @param	EventTime	the time the event/s took to execute; should be in milliseconds
 * @param	EventCount	the number of events that EventTime represents
 */
void FPerformanceData::TrackEvent( DOUBLE EventTime, INT EventCount/*=1*/ )
{
	Count += EventCount;

	TotalTime += EventTime;
	MinTime = Min(EventTime, MinTime);
	MaxTime = Max(EventTime, MaxTime);
}

/**
 * Returns the average time the event being tracked took to execute.
 *
 * @return	the average time (in milliseconds) spent executing the event being tracked.
 */
DOUBLE FPerformanceData::GetAverageTime() const
{
	return Count > 0 ? (TotalTime / Count) : 0.f;
}

/* ==========================================================================================================
	FStructPerformanceData
========================================================================================================== */
/**
 * Default Constructor
 */
FStructPerformanceData::FStructPerformanceData( const FName& InName, FPerformanceData* InEventData )
: StructName(InName), StructEventData(InEventData)
{
	check(StructEventData);
	AvgEventTime = StructEventData->GetAverageTime();
}

/* ==========================================================================================================
	FStructEventMap
========================================================================================================== */

/**
 * Integrates performance data for one or more occurrences of a struct event; handles inclusive time.
 *
 * @param	Struct				the struct to record data for
 * @param	PreviousTotalTime	the total amount of time spent in this event prior to executing this occurrence; used for converting inclusive time
 *								into exclusive time.  GetTotalEventTime should be called to retrieve this value prior to executing the event.
 *								(in milliseconds)
 * @param	EventTime			the time the event/s took to execute; (in milliseconds)
 * @param	NumberOfEvents		the number of events represented by EventTime
 *
 * @return	a pointer to the performance data corresponding to the specified struct 
 */
FPerformanceData* FStructEventMap::TrackEvent( UStruct* Struct, DOUBLE PreviousTotalTime, DOUBLE EventTime, INT NumberOfEvents/*=1*/ )
{
	FPerformanceData* EventData = NULL;
	if ( Struct != NULL && !(GIsCooking || GIsUCCMake) )
	{
		// lookup the performance data for this struct
		const TScopedPointer<FPerformanceData>* EventDataPointer = Find(Struct->GetFName());
		if ( EventDataPointer == NULL )
		{
			EventDataPointer = &Set(Struct->GetFName(), new FPerformanceData());
		}

		check(EventDataPointer);
		EventData = *EventDataPointer;

		// the current value of EventData->TotalTime represents the total amount of time spent executing this event, EXCLUDING this occurrence; if the current TotalTime
		// is different from the PreviousTotalTime, it means the event was executed recursively for the same struct and the difference is the amount of time that was spent 
		// on the recursive execution.
		DOUBLE ExclusiveTime = EventTime - (EventData->TotalTime - PreviousTotalTime);
		EventData->TrackEvent(ExclusiveTime, NumberOfEvents);
	}

	return EventData;
}

/**
 * Returns the performance data associated with the specified struct
 *
 * @param	Struct	the struct to find performance data for
 *
 * @return	a pointer to the performance data corresponding to the specified struct, or NULL if there is no existing performance data for that struct
 */
FPerformanceData* FStructEventMap::GetPerformanceData( UStruct* Struct ) const
{
	FPerformanceData* EventData = NULL;
	if ( Struct != NULL )
	{
		const TScopedPointer<FPerformanceData>* EventDataPointer = Find(Struct->GetFName());
		if(EventDataPointer)
		{
			EventData = *EventDataPointer;
		}
	}
	return EventData;
}

/**
 * Returns the total time spent executing the event for the specified struct.
 *
 * @param	Struct	the struct to return total time for
 *
 * @return	the total time spent executing the event for the specified struct so far (in milliseconds)
 */
DOUBLE FStructEventMap::GetTotalEventTime( UStruct* Struct ) const
{
	DOUBLE Result = 0.f;

	FPerformanceData* EventData = GetPerformanceData(Struct);
	if ( EventData != NULL )
	{
		Result = EventData->TotalTime;
	}

	return Result;
}

/**
 * Cleans up all performance data allocated by this map.
 */
void FStructEventMap::ClearEvents()
{
	Empty();
}

IMPLEMENT_COMPARE_POINTER(FStructPerformanceData,PerfCounter,{ return (B->StructEventData->MaxTime > A->StructEventData->MaxTime) ? 1 : -1; })


/**
 * Dumps the performance data results to the specified output device.
 */
void FStructEventMap::DumpPerformanceData( FOutputDevice* OutputAr/*=GLog*/ )
{
	TIndirectArray<FStructPerformanceData> PerformanceData;

	INT StructNamePadding = 0;

	for ( FStructEventMap::TIterator It(*this); It; ++It )
	{
		const FName& StructName = It.Key();
		FPerformanceData* StructPerformanceData = It.Value();
		new(PerformanceData) FStructPerformanceData(StructName, StructPerformanceData);

		// calculate the padding values
		StructNamePadding = Max(StructNamePadding, StructName.ToString().Len());
	}

	FStructPerformanceData** pData = (FStructPerformanceData**)PerformanceData.GetData();
	Sort<USE_COMPARE_POINTER(FStructPerformanceData,PerfCounter)>( pData, PerformanceData.Num() );

#if USE_LS_SPEC_FOR_UNICODE
	OutputAr->Logf(NAME_PerfEvent, TEXT("%*ls %9ls %11ls %11ls %11ls %11ls"), StructNamePadding, TEXT("Struct"), TEXT("Count"), TEXT("Total"), TEXT("Min"), TEXT("Max"), TEXT("Average"));
#else
	OutputAr->Logf(NAME_PerfEvent, TEXT("%*s %9s %11s %11s %11s %11s"), StructNamePadding, TEXT("Struct"), TEXT("Count"), TEXT("Total"), TEXT("Min"), TEXT("Max"), TEXT("Average"));
#endif
	for ( INT DataIndex = 0; DataIndex < PerformanceData.Num(); DataIndex++ )
	{
		FStructPerformanceData& Data = PerformanceData(DataIndex);

	// 	debugf(TEXT(".....12345678....12345678.1234....12345678.1234....12345678.1234....12345678.1234"));
#if USE_LS_SPEC_FOR_UNICODE
		OutputAr->Logf(NAME_PerfEvent, TEXT("%*ls %9i %11.4f %11.4f %11.4f %11.4f"), StructNamePadding, *Data.StructName.ToString(), Data.StructEventData->Count,
#else
		OutputAr->Logf(NAME_PerfEvent, TEXT("%*s %9i %11.4f %11.4f %11.4f %11.4f"), StructNamePadding, *Data.StructName.ToString(), Data.StructEventData->Count,
#endif
			Data.StructEventData->TotalTime, Data.StructEventData->MinTime, Data.StructEventData->MaxTime, Data.AvgEventTime);
	}
}



// EOF




