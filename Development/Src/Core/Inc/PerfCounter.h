/*=============================================================================
	PerfCounter.h: Performance counter class declarations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PERFCOUNTER_H__
#define __PERFCOUNTER_H__

// Required includes:
// needs SDK include for FLT_MAX
// needs platform include (i.e. UnVcWin32.h, etc.) for types e.g. DOUBLE, INT, etc.
// needs UnObjBas.h for UObject methods
// needs UnClass.h for UStruct declaration
// needs UnTemplate.h for Min/Max and TMap

/**
 * Tracks time spent executing an arbitrary event.
 */
struct FPerformanceData
{
	/** the total time (in milliseconds) spent executing the event being tracked by this struct */
	DOUBLE	TotalTime;

	/** the minimum amount of time (in milliseconds) spent executing a single occurrence of the event being tracked by this struct */
	DOUBLE	MinTime;

	/** the maximum amount of time (in milliseconds) spent executing a single occurrence of the event being tracked by this struct */
	DOUBLE	MaxTime;

	/** the number of times this event was executed */
	INT		Count;

	/**
	 * Default Constructor
	 */
	FPerformanceData();

	/**
	 * Integrates data from the event being tracked by this struct.
	 *
	 * @param	EventTime	the time the event/s took to execute; should be in milliseconds
	 * @param	EventCount	the number of events that EventTime represents
	 */
	void TrackEvent( DOUBLE EventTime, INT EventCount=1 );

	/**
	 * Returns the average time the event being tracked took to execute.
	 *
	 * @return	the average time (in milliseconds) spent executing the event being tracked.
	 */
	DOUBLE GetAverageTime() const;
};

/**
 * Used for sorting results.
 */
struct FStructPerformanceData
{
	/** the name of the struct this performance data is associated with */
	const FName&		StructName;

	/** the performance data recorded for this struct */
	FPerformanceData*	StructEventData;

	/** the average time spent processing the event for the associated struct */
	DOUBLE				AvgEventTime;

	/**
	 * Default Constructor
	 */
	FStructPerformanceData( const FName& InName, FPerformanceData* InEventData );
};

/**
 * A wrapper class for tracking performance data of structs; encapsulates a mapping from UStruct pathname to performance data.
 */
class FStructEventMap : protected TMap<FName,TScopedPointer<FPerformanceData> >
{
public:

	/**
	 * Integrates performance data for one or more occurrences of a struct event; handles inclusive time.
	 *
	 * @param	Struct				the struct to record data for
	 * @param	StartingEventTime	the total amount of time spent in this event prior to executing this occurrence; used for converting inclusive time
	 *								into exclusive time.  GetTotalEventTime should be called to retrieve this value prior to executing the event.
	 *								(in milliseconds)
	 * @param	InclusiveTime		the time the event/s took to execute; (in milliseconds)
	 * @param	NumberOfEvents		the number of events represented by EventTime
	 *
	 * @return	a pointer to the performance data corresponding to the specified struct 
	 */
	FPerformanceData* TrackEvent( UStruct* Struct, DOUBLE StartingEventTime, DOUBLE InclusiveTime, INT NumberOfEvents=1 );

	/**
	 * Returns the performance data associated with the specified struct
	 *
	 * @param	Struct	the struct to find performance data for
	 *
	 * @return	a pointer to the performance data corresponding to the specified struct, or NULL if there is no existing performance data for that struct
	 */
	FPerformanceData* GetPerformanceData( UStruct* Struct ) const;

	/**
	 * Returns whether this performance tracker has any associated data
	 */
	UBOOL HasPerformanceData() const
	{
		return Pairs.Num() > 0;
	}

	/**
	 * Returns the total time spent executing the event for the specified struct.
	 *
	 * @param	Struct	the struct to return total time for
	 *
	 * @return	the total time spent executing the event for the specified struct so far (in milliseconds)
	 */
	DOUBLE GetTotalEventTime( UStruct* Struct ) const;

	/**
	 * Cleans up all performance data allocated by this map.
	 */
	void ClearEvents();

	/**
	 * Dumps the performance data results to the specified output device.
	 */
	void DumpPerformanceData( class FOutputDevice* OutputAr=GLog );
};

#if DEDICATED_SERVER
#define MAX_PERFCOUNTER_CONNECTIONS 32

/**
 *  Counters for tracking dedicated server performance
 */
struct FDedicatedServerPerfCounters
{
	/** Number of currently connected clients */
	INT NumClientConnections;

	/** Per client perf counters */
	struct ClientConnectionPerfCounters
	{
		/** Client ping */
		INT Ping;
		/** Bytes received from the client this frame*/
		INT InBytes;
		/** Bytes sent to the client this frame */
		INT OutBytes;
		/** Total incoming/outgoing packetloss */
		INT PacketLoss;
	};

	ClientConnectionPerfCounters ClientPerfCounters[MAX_PERFCOUNTER_CONNECTIONS];
};

extern FDedicatedServerPerfCounters GDedicatedServerPerfCounters;

#endif //DEDICATED_SERVER


#endif	//__PERFCOUNTER_H__
// EOF

