/**
 * PerfMem
 *
 * NOTE:  For the DBProxy, we need to not send the Exec command with " " around it as:  Don't use double quotes around strings, use single quotes.
 *        This is the most bulletproof solution, since then it doesn't matter if QUOTED_IDENTIFIER is set to either OFF or ON. Remember, in 
 *        Transact-SQL, always use single quotes, never double quotes. If, somehow you can't avoid to use double quotes denoting strings, then you 
 *        must ensure that for the connection QUOTED_IDENTIFIER is set to OFF. It's a bit more hassle this way, since each connection can override
 *         the serversetting - you have to enforce this everywhere in the clientcode making the connection..
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


#include "CorePrivate.h"
#include "PerfMem.h"
#include "Database.h"

PerfMem::PerfMem( const FVector& InLocation, const FRotator& InRotation ):
    Location(InLocation), Rotation(InRotation)
{
}


FString PerfMem::GetLocationRotationString() const
{
	FString Retval;

	Retval = FString::Printf( TEXT( "@LocX=%d, @LocY=%d, @LocZ=%d, @RotYaw=%d, @RotPitch=%d, @RotRoll=%d" )
		, appTrunc(Location.X)
		, appTrunc(Location.Y)
		, appTrunc(Location.Z)
		, Rotation.Yaw
		, Rotation.Pitch
		, Rotation.Roll 
		);

	return Retval;
}


/**
 * This will add a stat to the DB.
 *
 * @param InStatGroupName  Name of the StatGroup this stat belongs to
 * @param InStatName Name of the actual stat to be added
 * @param InStatValue value for the stat (all values are FLOATs ATM)
 * @param InDivideBy the InStatValue will be divided by this amount.  This is needed for memory values where we want to see things in KB for the most part so we can easily read them and not have to have a ton of extra dividing in all our charts and everywhere else.
 *
 **/
void PerfMem::AddStatToDB( const FString& InStatGroupName, const FString& InStatName, FLOAT InStatValue, FLOAT InDivideBy )
{
	const FString StatGroupName = InStatGroupName.Replace( TEXT(" "), TEXT("_") );
	const FString StatName = InStatName.Replace( TEXT(" "), TEXT("_") );
	extern const FString GetNonPersistentMapNameStatic();

	const FString AddRunData = FString::Printf(TEXT("EXEC AddRunData @RunID=%i, @StatGroupName='%s', @StatName='%s', %s, @StatValue=%f, @SubLevelName='%s'")
		, GSentinelRunID
		, *StatGroupName
		, *StatName
		, *GetLocationRotationString()
		, InStatValue/InDivideBy
		, *GetNonPersistentMapNameStatic()
		);

	//warnf( TEXT("%s"), *AddRunData );

	GTaskPerfMemDatabase->SendExecCommand( *AddRunData );
}


/**
 * This will eventually dump out all stats to the DB 
 **/
void PerfMem::AddAllStatsToDB()
{
	warnf( TEXT( "Not functional yet." ) );
}



/**
 * This will take the stat memory  data and add them all to the DB for this location and rotation
 **/
void PerfMem::AddMemoryStatsForLocationRotation()
{
#if STATS
	// get all of the column names
	const FStatGroup* TheStatGroup = GStatManager.GetGroup( STATGROUP_Memory );

	const FMemoryCounter* CurrMem = TheStatGroup->FirstMemoryCounter;

	while( CurrMem != NULL )
	{
		AddStatToDB( FString(TheStatGroup->Desc), FString(GStatManager.GetStatName( CurrMem->StatId )), CurrMem->Value, 1024.0f );

		CurrMem = (const FMemoryCounter*)CurrMem->Next;
	}

#endif // STATS
}



/**
 * This will take a bunch of perf stats and add them to the DB for this location and rotation
 **/
void PerfMem::AddPerfStatsForLocationRotation()
{
#if STATS
	extern FLOAT GUnit_FrameTime;
	extern FLOAT GUnit_GameThreadTime;
	extern FLOAT GUnit_RenderThreadTime;
	extern FLOAT GUnit_GPUFrameTime;

	AddStatToDB( FString(TEXT("UnitFPS")), FString(TEXT("FrameTime")), GUnit_FrameTime, 1.0f );
	AddStatToDB( FString(TEXT("UnitFPS")), FString(TEXT("Game_thread_time")), GUnit_GameThreadTime, 1.0f );
	AddStatToDB( FString(TEXT("UnitFPS")), FString(TEXT("Render_thread_time")), GUnit_RenderThreadTime, 1.0f );
	AddStatToDB( FString(TEXT("UnitFPS")), FString(TEXT("GPU_time")), GUnit_GPUFrameTime, 1.0f );

#endif // STATS
}



/**
 * This will take a bunch of perf stats and add them to the DB for this location and rotation
 **/
void PerfMem::AddViewDependentMemoryStatsForLocationRotation()
{
#if STATS

//	const FStatGroup* TheGroup2 = GStatManager.GetGroup( STATGROUP_SceneRendering );
//	const FStatCounterDWORD* CurrDWORD = TheGroup2->FirstDwordCounter;
// 	while( CurrDWORD != NULL )
// 	{
// 		SQLCommand += FString::Printf( TEXT( ", @%s="), *FString(GStatManager.GetStatName( CurrDWORD->StatId )).Replace( TEXT(" "), TEXT("_") ) ); 
// 		SQLCommand += FString::Printf( TEXT( "%.2f"), CurrDWORD->History.GetAverage() ); 
// 
// 		//warnf( TEXT( "%s %.2f"), GStatManager.GetStatName( CurrDWORD->StatId ), CurrDWORD->History.GetAverage() );
// 
// 		CurrDWORD = (const FStatCounterDWORD*)CurrDWORD->Next;
// 	}


	// we have to do this pain as we don't have an iterator atm

	const FStatGroup* TheStatGroup = GStatManager.GetGroup( STATGROUP_Streaming );

	const FMemoryCounter* CurrMem = TheStatGroup->FirstMemoryCounter;

	while( CurrMem != NULL )
	{
		AddStatToDB( FString(TheStatGroup->Desc), FString(GStatManager.GetStatName( CurrMem->StatId )), CurrMem->Value, 1024.0f );

		CurrMem = (const FMemoryCounter*)CurrMem->Next;
	}


	const FStatAccumulatorFLOAT* CurrAccumFloat = TheStatGroup->FirstFloatAccumulator;

	while( CurrAccumFloat != NULL )
	{
		AddStatToDB( FString(TheStatGroup->Desc), FString(GStatManager.GetStatName( CurrAccumFloat->StatId )), CurrAccumFloat->Value, 1.0f );

		CurrAccumFloat = (const FStatAccumulatorFLOAT*)CurrAccumFloat->Next;
	}

#endif // STATS
}




/**
 * This will take the set of stats we are using for "TimePeriod" based stat gathering (e.g. MP_PlayTests).
 * It will log them out with the location and rotation.
 **/
void PerfMem::AddPerfStatsForLocationRotation_TimePeriod()
{
#if STATS
	extern FLOAT GUnit_FrameTime;
	extern FLOAT GUnit_GameThreadTime;
	extern FLOAT GUnit_RenderThreadTime;
	extern FLOAT GUnit_GPUFrameTime;

	AddStatToDB( FString(TEXT("UnitFPS")), FString(TEXT("FrameTime")), GUnit_FrameTime, 1.0f );
	AddStatToDB( FString(TEXT("UnitFPS")), FString(TEXT("Game_thread_time")), GUnit_GameThreadTime, 1.0f );
	AddStatToDB( FString(TEXT("UnitFPS")), FString(TEXT("Render_thread_time")), GUnit_RenderThreadTime, 1.0f );
// GPU time is not recorded in FINAL_RELEASE for XBOX
#if !FINAL_RELEASE || !XBOX
	AddStatToDB( FString(TEXT("UnitFPS")), FString(TEXT("GPU_time")), GUnit_GPUFrameTime, 1.0f );
#endif // !FINAL_RELEASE
#endif
}

