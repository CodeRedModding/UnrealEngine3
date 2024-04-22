/**
 * PerfMem
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


#ifndef __PERF_MEM_H__
#define __PERF_MEM_H__



/**
 * This will get the changelist that should be used with the Automated Performance Testing
 * If one is passed in we use that otherwise we use the GBuiltFromChangeList.  This allows
 * us to have build machine built .exe and test them. 
 *
 * NOTE: had to use AutomatedBenchmarking as the parsing code is flawed and doesn't match
 *       on whole words.  so automatedperftestingChangelist was failing :-( 
 **/
static INT GetChangeListNumberForPerfTesting()
{
	INT Retval = GBuiltFromChangeList;

	// if we have passed in the changelist to use then use it
	INT FromCommandLine = 0;
	Parse( appCmdLine(), TEXT("-gABC="), FromCommandLine );

	// we check for 0 here as the CIS always appends -AutomatedPerfChangelist but if it
	// is from the "built" builds when it will be a 0
	if( FromCommandLine != 0 )
	{
		Retval = FromCommandLine;
	}

	return Retval;
}


/** This will return the Configuration name that we use for DB operations **/
static FString GetConfigName()
{
	FString Retval;

#if _DEBUG
	Retval = TEXT("DEBUG");
#elif FINAL_RELEASE_DEBUGCONSOLE
	Retval = TEXT("FINAL_RELEASE_DEBUGCONSOLE");
#elif FINAL_RELEASE
	Retval = TEXT("FINAL_RELEASE");
#else
	Retval = TEXT("RELEASE");
#endif

	return Retval;
}





// TODO:  switch to use: for( FStatGroup::FStatIterator StatItr(MemGroup); StatItr; ++StatItr )    but that is a FStatCommonData which has
//        no notion of value.   Should have some high level GetValue() for each stat type.  Which either returns the running average or the value 
//        something sane for the type (even if it not 100% precise)
//
//  Refactor the EStatGroups InStatGroup  to be a TArray of EStatGroups that want to be store dout
//
//  Convert the Stored Procedures to take arrays of strings so we can have generic code



/** 
 * This class holds the functions for creating StoreProcedure calls to update specific tables.
 * Specifically:  Most of these will update the RunData table with Stat data for a location.
 *
 **/
class PerfMem
{

	PerfMem(); // private

	/** This is the location where the stat data was taken from **/
	FVector Location;
	/** This is the rotation where the stat data was taken from **/
	FRotator Rotation;


public:
	/** 
	 * Constructor 
	 *
	 * @param Location where this perf data is from
	 * @param Rotation where this perf data is from
	 **/
	PerfMem( const FVector& InLocation, const FRotator& InRotation );

	/** Creates the StroedProcedure formated string for the Location/Rotation **/
	FString GetLocationRotationString() const;


	//// Functions that will AddSpecific StatGroups to the DB

	/**
	 * This will add a stat to the DB.
	 *
	 * @param InStatGroupName  Name of the StatGroup this stat belongs to
	 * @param InStatName Name of the actual stat to be added
	 * @param InStatValue value for the stat (all values are FLOATs ATM)
	 * @param InDivideBy the InStatValue will be divided by this amount.  This is needed for memory values where we want to see things in KB for the most part so we can easily read them and not have to have a ton of extra dividing in all our charts and everywhere else.
	 *
	 **/
	void AddStatToDB( const FString& InStatGroupName, const FString& InStatName, FLOAT InStatValue, FLOAT InDivideBy );


	/**
	 * This will eventually dump out all stats to the DB 
	 **/
	void AddAllStatsToDB();


	/**
	 * This will take the stat memory  data and add them all to the DB for this location and rotation
	 **/
	void AddMemoryStatsForLocationRotation();


	/**
	 * This will take a bunch of perf stats and add them to the DB for this location and rotation
	 **/
	void AddPerfStatsForLocationRotation();

	/**
	 * This will take a bunch of view dependent perf stats and add them to the DB for this location and rotation
	 **/
	void AddViewDependentMemoryStatsForLocationRotation();

	/**
	 * This will take the set of stats we are using for "TimePeriod" based stat gathering (e.g. MP_PlayTests).
	 * It will log them out with the location and rotation.
	 **/
	void AddPerfStatsForLocationRotation_TimePeriod();

};


#endif // __PERF_MEM_H__

