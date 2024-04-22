/**
 * All of the native engine level functions for Sentinel should be placed in this file.
 * This will allow easy back porting of sentinel.  Also it should make it easier to
 * find the various disparate functions that Sentinel uses as they are located in a number
 * of engine objects.
 *
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"
#include "EngineDecalClasses.h"
#include "UnNet.h"
#include "EngineSequenceClasses.h"
#include "UnPath.h"
#include "EngineAudioDeviceClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineAnimClasses.h"
#include "DemoRecording.h"
#include "EngineUserInterfaceClasses.h"
#include "PerfMem.h"
#include "Database.h"
#include "EngineSplineClasses.h"

/** 
 * This will start a SentinelRun in the DB.  Setting up the Run table with all of metadata that a run has.
 * This will also set the GSentinelRunID so that the engine knows what the current run is.
 *
 * @param TaskDescription The name/description of the task that we are running
 * @param TaskParameter Any Parameters that the task needs
 * @param TagDesc A specialized tag (e.g. We are doing Task A with Param B and then "MapWithParticlesAdded" so it can be found)
 **/
void AAutoTestManager::BeginSentinelRun( const FString& TaskDescription, const FString& TaskParameter, const FString& TagDesc )
{
	extern const FString GetMapNameStatic();
	extern INT GScreenWidth;
	extern INT GScreenHeight;

	const FString BeginRun = FString::Printf( TEXT("EXEC BeginRun @PlatformName='%s', @MachineName='%s', @UserName='%s', @Changelist='%d', @GameName='%s', @ResolutionName='%s', @ConfigName='%s', @CmdLine='%s', @GameType='%s', @LevelName='%s', @TaskDescription='%s', @TaskParameter='%s', @Tag='%s'")
		//, appTimestamp() // need to pass in the date for when the compile was done  format:  03/18/08 20:35:48
		, (appGetPlatformType() == UE3::PLATFORM_AnyWindows) ? TEXT("Windows") : *appGetPlatformString()
		, appComputerName()
		, appUserName() 
		, GetChangeListNumberForPerfTesting()
		, appGetGameName()
		, *FString::Printf(TEXT("%dx%d"), GScreenWidth, GScreenHeight ) // resolution is:  width by height
		, *GetConfigName()
		, appCmdLine()
		, *this->GetName()
		, *GetMapNameStatic()
		//	BeginSentinelRun( "TravelTheWorld", "TaskParameter", "TagDesc" );
		, *TaskDescription
		, *TaskParameter
		, *TagDesc
		);

	FDataBaseRecordSet* RecordSet = NULL;
	if( GTaskPerfMemDatabase->SendExecCommandRecordSet( *BeginRun, RecordSet ) && RecordSet )
	{
		// Retrieve RunID from recordset. It's the return value of the EXEC.
		GSentinelRunID = RecordSet->GetInt(TEXT("Return Value"));
		//warnf( TEXT("RunID: %d"), GSentinelRunID );
	}

	warnf( TEXT("%s %d"), *BeginRun, GSentinelRunID );

	delete RecordSet;
	RecordSet = NULL;
}


/**
 * This will output some set of data that we care about when we are doing Sentinel runs while we are
 * doing a MP test or a BVT. 
 * Prob just stat unit and some other random stats (streaming fudge factor and such)
 **/
void AAutoTestManager::AddSentinelPerTimePeriodStats( const FVector InLocation, const FRotator InRotation )
{
	if( GSentinelRunID != - 1 )
	{
		PerfMem Datum( InLocation, InRotation );
		Datum.AddPerfStatsForLocationRotation_TimePeriod();
	}
}

/** 
 * Add the audio related stats to the database
 */
void AAutoTestManager::HandlePerLoadedMapAudioStats()
{
	// Get the memory used by each sound group
	if( GEngine && GEngine->Client )
	{
		TMap<FName, FAudioClassInfo> AudioClassInfos;

		UAudioDevice* AudioDevice = GEngine->Client->GetAudioDevice();
		AudioDevice->GetSoundClassInfo( AudioClassInfos );

		for( TMap<FName, FAudioClassInfo>::TIterator AGIIter( AudioClassInfos ); AGIIter; ++AGIIter )
		{
			FName SoundClassFName = AGIIter.Key();
			FString SoundClassName = FString::Printf( TEXT( "SC_%s" ), *SoundClassFName.ToString() );
			FAudioClassInfo* ACI = AudioClassInfos.Find( SoundClassFName );

			extern const FString GetNonPersistentMapNameStatic();
			FString AddRunData = FString::Printf( TEXT( "EXEC AddRunData @RunID=%i, @StatGroupName='%s', @StatName='%s', @StatValue=%f, @SubLevelName='%s'" )  
				, GSentinelRunID  
				, TEXT( "SoundClass" )  
				, *SoundClassName
				, ACI->SizeResident / 1024.0f 
				, *GetNonPersistentMapNameStatic()
				);  

			GTaskPerfMemDatabase->SendExecCommand( *AddRunData ); 
		}
	}
}


/** 
 * This will run on every map load.  (e.g. You have P map which consists of N sublevels.  For each SubLevel this will run. 
 **/
void AAutoTestManager::DoSentinelActionPerLoadedMap()
{
	HandlePerLoadedMapAudioStats();
}

/** 
 * This will tell the DB to end the current Sentinel run (i.e. GSentinelRunID) and set that Run's RunResult to the passed in var.
 *
 * @param RunResult The result of this Sentinel run (e.g. OOM, Passed, etc.)
 **/
void AAutoTestManager::EndSentinelRun( BYTE RunResult )
{
	if( GSentinelRunID != - 1 )
	{
		const FString EndRun = FString::Printf(TEXT("EXEC EndRun @RunID=%i, @ResultDescription='%s'")
			, GSentinelRunID
			, *PerfMemRunResultStrings[RunResult] 
		);

		//warnf( TEXT("%s"), *EndRun );
		GTaskPerfMemDatabase->SendExecCommand( *EndRun );
	}
}

// TODO:  need to convert this into .ini for stats to gather so we don't have to have this code madness and LDs could do some sweeet sweetness of choosing which stats they want to see
// pain:  need to have some scaling factor attached to each  (e.g. for the memory stats so we don't jsut see bytes which are impossible to understand

void AAutoTestManager::DoSentinel_MemoryAtSpecificLocation( const FVector InLocation, const FRotator InRotation )
{
	PerfMem Datum( InLocation, InRotation );
	//Datum.WriteInsertSQLToBatFile();
	Datum.AddMemoryStatsForLocationRotation();
}


void AAutoTestManager::DoSentinel_PerfAtSpecificLocation( const FVector& InLocation, const FRotator& InRotation )
{
	PerfMem Datum( InLocation, InRotation );
	//Datum.WriteInsertSQLToBatFile();
	Datum.AddPerfStatsForLocationRotation();
}

void AAutoTestManager::DoSentinel_ViewDependentMemoryAtSpecificLocation( const FVector& InLocation, const FRotator& InRotation )
{
	PerfMem Datum( InLocation, InRotation );
	//Datum.WriteInsertSQLToBatFile();
	Datum.AddViewDependentMemoryStatsForLocationRotation();
}


namespace
{
	TArray<FVector> SentinelCoverLinks;
	TArray<FVector> SentinelPlayerStarts;
	TArray<FVector> SentinelPathNodes;
	TArray<FVector> SentinelPickUpFactories;
	TArray<FVector> SentinelPylons;
	TArray<FVector> SentinelSplines;
}

void AAutoTestManager::GetTravelLocations( FName LevelName, APlayerController* PC, TArray<FVector>& TravelPoints )
{
	warnf( TEXT("GetTravelLocations %d"), GWorld->Levels.Num() );

	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

	for( INT RealLevelIdx = 0; RealLevelIdx < GWorld->Levels.Num(); ++RealLevelIdx )
	{
		warnf( TEXT("Level: %d %s %d"), GWorld->Levels.Num(), *GWorld->Levels(RealLevelIdx)->GetOutermost()->GetName(), GWorld->Levels(RealLevelIdx)->Actors.Num() );


		for( INT ActorIdx = 0; ActorIdx < GWorld->Levels(RealLevelIdx)->Actors.Num(); ++ActorIdx )
		{
			AActor* Actor = GWorld->Levels(RealLevelIdx)->Actors(ActorIdx);

			//ANavigationPoint* NavPoint = Cast<ANavigationPoint>(Actor);
			ACoverLink* CoverLink = Cast<ACoverLink>(Actor);
			APlayerStart* PlayerStart = Cast<APlayerStart>(Actor);
			APathNode* PathNode = Cast<APathNode>(Actor);
			APickupFactory* PickUpFactory = Cast<APickupFactory>(Actor);
			APylon* Pylon = Cast<APylon>(Actor);
			ASplineActor* Spline = Cast<ASplineActor>(Actor);


			if( ( CoverLink != NULL ) || ( PathNode != NULL ) || ( PickUpFactory != NULL ) || ( PlayerStart != NULL ) || ( Pylon != NULL ) || ( Spline != NULL ) )
			{
				//warnf( TEXT("Adding NavPoint: %s"), *Actor->GetName() );
				TravelPoints.AddUniqueItem( Actor->Location );
			}

			if( CoverLink != NULL )
			{
				SentinelCoverLinks.AddUniqueItem( Actor->Location );
			}

			if( PlayerStart != NULL ) 
			{
				SentinelPathNodes.AddUniqueItem( Actor->Location );
			}

			if( PathNode != NULL ) 
			{
				SentinelPathNodes.AddUniqueItem( Actor->Location );
				//TravelPoints.AddUniqueItem( Actor->Location );
			}

			if( PickUpFactory != NULL ) 
			{
				SentinelPickUpFactories.AddUniqueItem( Actor->Location );
			}

			if( Pylon != NULL ) 
			{
				SentinelPylons.AddUniqueItem( Pylon->Location );
			}

			if( Spline != NULL ) 
			{
				SentinelSplines.AddUniqueItem( Spline->Location );
			}

		}
	}

	// when we return to the .uc land we will set our PC to bIsUsingStreamingVolumes = TRUE 
	warnf( TEXT( "Total Number of TravelPoints: %d  SentinelCoverLinks: %d  SentinelPlayerStarts: %d  SentinelPathNodes: %d  SentinelPickUpFactories: %d  SentinelPylons: %d  SentinelSplines: %d" ), TravelPoints.Num(), SentinelCoverLinks.Num(), SentinelPlayerStarts.Num(), SentinelPathNodes.Num(), SentinelPickUpFactories.Num(), SentinelPylons.Num(), SentinelSplines.Num() );
}

/** Native access function to do the actual memory tracking work, needs access to DefaultTransition and WorldName */
void AAutoTestManager::DoMemoryTracking()
{	
	if(!AutomatedMapTestingTransitionMap.IsEmpty())
	{
		// Auto map list does tracking only on the specified transition map
		if( AutomatedTestingMapIndex < 0 )
		{
			WorldInfo->DoMemoryTracking();
		}
	}
	else if ( GWorld->GetFullName().InStr(FURL::DefaultTransitionMap) != -1 )
	{
		WorldInfo->DoMemoryTracking();
	}
}



