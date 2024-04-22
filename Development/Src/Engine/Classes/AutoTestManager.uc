//=============================================================================
// AutoTestManager
//
// The AutoTestManager is spawned by the GameInfo if requested by the URL.
// It provides an interface for performing in gameplay automated testing.
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class AutoTestManager extends Info
	native
	config(Game);
	
//=================================================================
// AUTOMATED PERFORMANCE TESTING PROPERTIES
/** Whether the game is currently in automated perf test mode. */
var bool bAutomatedPerfTesting;

/** Amount of time remaining before match ends -- used for auto performance test shutdown */
var int AutomatedPerfRemainingTime;

/** This will auto continue to the next round.  Very useful doing soak testing and testing traveling to next level **/
var bool bAutoContinueToNextRound;

/** Whether or not we are using the Automated Testing Map list **/
var bool bUsingAutomatedTestingMapList;

/** If TRUE, use OpenMap to transition; if FALSE, use SeamlessTravel **/
var bool bAutomatedTestingWithOpen;

/**
 *	The index of the current automated testing map.
 *	If < 0 we are in the transition map.
 **/
var int AutomatedTestingMapIndex;

/** List of maps that we are going to be using for the AutomatedMapTesting **/
var globalconfig array<string> AutomatedMapTestingList;

/** Number of times to run through the list.  (0 in infinite) **/
var globalconfig int NumAutomatedMapTestingCycles;

/** Whether to exit when NumAutomatedMapTestingCycles is reached */
var globalconfig bool bExitOnCyclesComplete;

/**
 *	Number of matches played (maybe remove this before shipping)
 *	This is really useful for doing soak testing and such to see how long you lasted!
 *	NOTE:  This is not replicated out to clients atm.
 **/
var int NumberOfMatchesPlayed;

/** Keeps track of the current run so when we have repeats and such we know how far along we are **/
var int NumMapListCyclesDone;

/** This will be run at the start of each start match **/
var string AutomatedTestingExecCommandToRunAtStartMatch;

/** This will be the 'transition' map used w/ OpenMap runs **/
var string AutomatedMapTestingTransitionMap;

/**
 * Whether or not this game should check for fragmentation.  This can be used to have a specific game type check for fragmentation at some point
 * (e.g. start/end of match, time period)
 **/
var bool bCheckingForFragmentation;

/** Whether or not this game should check for memory leaks **/
var bool bCheckingForMemLeaks;

//====================================================================
// SENTINEL PROPERTIES
/** Whether or this game is doing a bDoingASentinelRun test **/
var bool bDoingASentinelRun;

/** Used for the BeginRun Task___ strings, examples "FlyThrough", "FlyThroughSplitScreen", "BVT" */
var String SentinelTaskDescription;

/** Used for the BeginRun Task___ strings */
var String SentinelTaskParameter;

/** Used for the BeginRun Task___ strings */
var String SentinelTagDesc;

/** PlayerController used for Sentinel - picked randomly */
var transient PlayerController SentinelPC;

/** Locations where sentinel should go to */
var transient array<vector> SentinelTravelArray;

/** Iterator for looping through SentinelTravelArray - for loop is in state code, so can't define locally */
var transient int SentinelNavigationIdx;

/** Iterator for various sentinel state code loops - for loop is in state code, so can't define locally */
var transient int SentinelIdx;

/** Used to delay until streaming levels are fully loaded */
var transient bool bSentinelStreamingLevelStillLoading;

/** Change increments for iterating through rotations used at sentinel travel locations */
var transient int NumRotationsIncrement;

/** Change increments for iterating through sentinel travel locations */
var transient int TravelPointsIncrement;

/** How many minutes per map we are allowed to run. **/
var config int NumMinutesPerMap;

/** 
 * At each TravelTheWorld node we fire off all of the commands in this array.  This is good for being able to
 * do things like fire off a debug command without having to recook the entire map (e.g. MemLeakCheck at each node).
 **/
var config array<String> CommandsToRunAtEachTravelTheWorldNode;

/** Transient string that we need for our foreach in the TravelTheWorld state code **/
var transient String CommandStringToExec;

event PostBeginPlay()
{
	Super.PostBeginPlay();
	
	SetTimer(1.0, true);
}

/**
  * Base AutoTestManager timer ticks once per second
  * Checks if perf test timer has run out
  */
event Timer()
{
	if( bAutomatedPerfTesting && (AutomatedPerfRemainingTime > 0) && !bAutoContinueToNextRound )
	{
		AutomatedPerfRemainingTime--;
		if( AutomatedPerfRemainingTime <= 0 )
		{
			// Exit at the end of the match if automated perf testing is enabled.
			ConsoleCommand("EXIT");
		}
	}
}
	
/**
  * Initialize AutoTestManager based on command line options
  * @param Options is the full command line options string passed to GameInfo
  */
function InitializeOptions(string Options)
{
	local string InOpt;

	AutomatedPerfRemainingTime = 60 * WorldInfo.Game.TimeLimit;

	bAutomatedPerfTesting = ( WorldInfo.Game.ParseOption( Options, "AutomatedPerfTesting" ) ~= "1" ) || ( WorldInfo.Game.ParseOption( Options, "gAPT" ) ~= "1" );

	bCheckingForFragmentation = ( WorldInfo.Game.ParseOption( Options, "CheckingForFragmentation" ) ~= "1" ) || ( WorldInfo.Game.ParseOption( Options, "gCFF" ) ~= "1" );
	
	bCheckingForMemLeaks = ( WorldInfo.Game.ParseOption( Options, "CheckingForMemLeaks" ) ~= "1" ) || ( WorldInfo.Game.ParseOption( Options, "gCFML" ) ~= "1" );
	bDoingASentinelRun = ( WorldInfo.Game.ParseOption( Options, "DoingASentinelRun" ) ~= "1" ) || ( WorldInfo.Game.ParseOption( Options, "gDASR" ) ~= "1" );

	SentinelTaskDescription = ( WorldInfo.Game.ParseOption( Options, "SentinelTaskDescription" ) );
	if( SentinelTaskDescription == "" ) 
	{
		SentinelTaskDescription = ( WorldInfo.Game.ParseOption( Options, "gSTD" ) );
	}

	SentinelTaskParameter = ( WorldInfo.Game.ParseOption( Options, "SentinelTaskParameter" ) );
	if( SentinelTaskParameter == "" ) 
	{
		SentinelTaskParameter = ( WorldInfo.Game.ParseOption( Options, "gSTP" ) );
	}

	SentinelTagDesc = ( WorldInfo.Game.ParseOption( Options, "SentinelTagDesc" ) );
	if( SentinelTagDesc == "" ) 
	{
		SentinelTagDesc = ( WorldInfo.Game.ParseOption( Options, "gSTDD" ) );
	}

	InOpt = WorldInfo.Game.ParseOption( Options, "AutoContinueToNextRound");
	if( InOpt != "" )
	{
		`log("AutoContinueToNextRound: "$bool(InOpt));
		bAutoContinueToNextRound = bool(InOpt);
	}
	

	InOpt = WorldInfo.Game.ParseOption( Options, "bUsingAutomatedTestingMapList");
	if( InOpt != "" )
	{
		`log("bUsingAutomatedTestingMapList: "$bool(InOpt));
		bUsingAutomatedTestingMapList = bool(InOpt);
	}

	if ( bUsingAutomatedTestingMapList )
	{
		if (AutomatedMapTestingList.length == 0)
		{
			`log("*** No maps in automated test map list... Disabling bUsingAutomatedTestingMapList");
			bUsingAutomatedTestingMapList = false;
		}
	}

	InOpt = WorldInfo.Game.ParseOption( Options, "bAutomatedTestingWithOpen");
	if( InOpt != "" )
	{
		`log("bAutomatedTestingWithOpen: "$bool(InOpt));
		bAutomatedTestingWithOpen = bool(InOpt);
	}

	AutomatedTestingExecCommandToRunAtStartMatch = WorldInfo.Game.ParseOption( Options, "AutomatedTestingExecCommandToRunAtStartMatch");
	`log("AutomatedTestingExecCommandToRunAtStartMatch: "$AutomatedTestingExecCommandToRunAtStartMatch);
	
	AutomatedMapTestingTransitionMap = WorldInfo.Game.ParseOption( Options, "AutomatedMapTestingTransitionMap");
	`log("AutomatedMapTestingTransitionMap: "$AutomatedMapTestingTransitionMap);

	InOpt = WorldInfo.Game.ParseOption(Options, "AutomatedTestingMapIndex");
	if (InOpt != "")
	{
		`log("AutomatedTestingMapIndex: " $ int(InOpt));
		AutomatedTestingMapIndex = int(InOpt);
	}

	if ( bAutomatedTestingWithOpen )
	{
		InOpt = WorldInfo.Game.ParseOption(Options, "NumberOfMatchesPlayed");
		if (InOpt != "")
		{
			`log("NumberOfMatchesPlayed: " $ int(InOpt));
			NumberOfMatchesPlayed = int(InOpt);
		}

		InOpt = WorldInfo.Game.ParseOption(Options, "NumMapListCyclesDone");
		if (InOpt != "")
		{
			`log("NumMapListCyclesDone: " $ int(InOpt));
			NumMapListCyclesDone = int(InOpt);
		}
	}
	else
	{
		// Server travel uses a transition map automatically...
		`log("*** Disabling automated transition map for ServerTravel");
		AutomatedMapTestingTransitionMap = "";
	}
} 

//// SENTINEL FUNCTIONS START
/**
 * This will start a SentinelRun in the DB.  Setting up the Run table with all of metadata that a run has.
 * This will also set the GSentinelRunID so that the engine knows what the current run is.
 *
 * @param TaskDescription The name/description of the task that we are running
 * @param TaskParameter Any Parameters that the task needs
 * @param TagDesc A specialized tag (e.g. We are doing Task A with Param B and then "MapWithParticlesAdded" so it can be found)
 **/
native function BeginSentinelRun( const string TaskDescription, const string TaskParameter, const string TagDesc );


/**
 * This will output some set of data that we care about when we are doing Sentinel runs while we are
 * doing a MP test or a BVT.
 * Prob just stat unit and some other random stats (streaming fudge factor and such)
 **/
native function AddSentinelPerTimePeriodStats( const vector InLocation, const rotator InRotation );


/**
 * This will tell the DB to end the current Sentinel run (i.e. GSentinelRunID) and set that Run's RunResult to the passed in var.
 *
 * @param RunResult The result of this Sentinel run (e.g. OOM, Passed, etc.)
 **/
native function EndSentinelRun( EAutomatedRunResult RunResult );

//// This code is our "travel the map and get Sentinel data at each point ////
// so we need to do:
// 0) turn off streaming volume streaming of levels   bIsUsingStreamingVolumes
// 1) unload all levels     StreamLevelOut( )
// 2) for each level in the P level
//      load it and then grab all of the locations of the NavigationPoints and store it
// 3) turn back on the streaming volume support
// 4) iterate down the list of locations

/** 
  * function to start the world traveling 
  **/
function DoTravelTheWorld()
{
	GotoState( 'TravelTheWorld' );
}

/** 
  * This our state which allows us to have delayed actions while traveling the world (e.g. waiting for levels to stream in) 
  **/
state TravelTheWorld
{
	function BeginState( name PreviousStateName )
	{
		local PlayerController PC;
		`log( "BeginState TravelTheWorld" );

		Super.BeginState( PreviousStateName );

		foreach LocalPlayerControllers( class'PlayerController', PC )
		{
			SentinelPC = PC;
			SentinelPC.Sentinel_SetupForGamebasedTravelTheWorld();
			break;
		}

		SentinelPC.bIsUsingStreamingVolumes = FALSE;

		BeginSentinelRun( SentinelTaskDescription, SentinelTaskParameter, SentinelTagDesc );
	}

	function float CalcTravelTheWorldTime( const int NumTravelLocations, const int NumRotations )
	{
		local float TotalTimeInSeconds;
		local float PerTravelLocTime;

		// streaming out all maps
		TotalTimeInSeconds += WorldInfo.StreamingLevels.length * 2.0f;
		TotalTimeInSeconds += 10.0f;

		// gathering travel locations
		TotalTimeInSeconds += WorldInfo.StreamingLevels.length * 10.0f;
		TotalTimeInSeconds += 10.0f;

		// wait for kill all
		TotalTimeInSeconds += 10.0f;

		// 4.0f is the avg cost for waiting for levels to stream in (guess basically)
		// we do two rotations as we want to capture UnitFPS data without any text on the screen
		PerTravelLocTime = 0.5f + 4.0f + 1.0f + 0.5f + 1.0f + (NumRotations*1.5f) + (NumRotations*1.5f);;

		TotalTimeInSeconds += (PerTravelLocTime * NumTravelLocations);


		return TotalTimeInSeconds;
	}

	function PrintOutTravelWorldTimes( const int TotalTimeInSeconds )
	{
		`if(`notdefined(FINAL_RELEASE))
			local int Hours;
		local int Minutes;
		local int Seconds;

		Hours = TotalTimeInSeconds / (60 * 60);
		Minutes = (TotalTimeInSeconds -(Hours * 60 * 60)) / 60;
		Seconds = TotalTimeInSeconds - (Minutes * 60) - (Hours * 60 * 60);

		`log( WorldInfo.GetMapName() $ ": Traveling this map will take approx TotalSeconds: " $ TotalTimeInSeconds $ "   Hours: " $ Hours $ "  Minutes: " $ Minutes $ "  Seconds: " $ Seconds );
		`endif
	}

	/**
	  * Modify our Increments so that we get the most number of nodes traveled to
	  * best is to travel to all doing 8 directions
	  * next is to travel to all and do 4 directions
	  * next is to travel to as many as possible across the map doing 4 directions
	  * @param NumTravelLocations is the total number of destination positions
	  */
	function SetIncrementsForLoops( const float NumTravelLocations )
	{
		local float TimeWeGetInSeconds;

		// @todo be able to pass in how much time we get in seconds
		TimeWeGetInSeconds = NumMinutesPerMap * 60;

		// if we will be able to travel to all points! so only increment by 1
		if( CalcTravelTheWorldTime( NumTravelLocations, 8 ) < TimeWeGetInSeconds )
		{
			TravelPointsIncrement = 1;
			NumRotationsIncrement = 1;
			`log( WorldInfo.GetMapName() $ " SetIncrementsForLoops: TravelPointsIncrement: " $ TravelPointsIncrement $ " NumRotationsIncrement: " $ NumRotationsIncrement $ " for NumTravelLocations: " $ NumTravelLocations);
			PrintOutTravelWorldTimes( CalcTravelTheWorldTime( NumTravelLocations, 8 ) );
		}
		// we can't get to all points so let's start reducing what we do
		else
		{
			// if we can get to all points but only 4 rotations
			if( CalcTravelTheWorldTime( NumTravelLocations, 4 ) < TimeWeGetInSeconds )
			{
				TravelPointsIncrement = 1;
				NumRotationsIncrement = 2; // (8/4)
				`log( WorldInfo.GetMapName() $ " SetIncrementsForLoops: TravelPointsIncrement: " $ TravelPointsIncrement $ " NumRotationsIncrement: " $ NumRotationsIncrement $ " for NumTravelLocations: " $ NumTravelLocations);
				PrintOutTravelWorldTimes( CalcTravelTheWorldTime( NumTravelLocations, 4 ) );

			}
			// we can't get to all points with 4 rotations so we need to increment our travelpoints faster
			else
			{
				// not 100% precise but the travel time is bounded by num points
				TravelPointsIncrement = CalcTravelTheWorldTime( NumTravelLocations, 4 ) / TimeWeGetInSeconds;

				NumRotationsIncrement = 2; // (8/4)
				`log( WorldInfo.GetMapName() $ " SetIncrementsForLoops: TravelPointsIncrement: " $ TravelPointsIncrement $ " NumRotationsIncrement: " $ NumRotationsIncrement $ " for NumTravelLocations: " $ NumTravelLocations);
				PrintOutTravelWorldTimes( CalcTravelTheWorldTime( NumTravelLocations/TravelPointsIncrement, 4 ) );
			}
		}

	}


Begin:
	// james pointed out that we could just save this out in the WorldInfo But that will take extra memory which could be thrown
	// away for final release

	SentinelPC.Sentinel_PreAcquireTravelTheWorldPoints();

	// we need to do some madness here as the async nature of the streaming makes it hard to just call and have the state be correct
	for( SentinelIdx = 0; SentinelIdx < WorldInfo.StreamingLevels.length; ++SentinelIdx )
	{
		`log( "StreamLevelOut: " $ Worldinfo.StreamingLevels[SentinelIdx].PackageName );
		SentinelPC.ClientUpdateLevelStreamingStatus( Worldinfo.StreamingLevels[SentinelIdx].PackageName, FALSE, FALSE, TRUE );
	}
	sleep( 10.0f );
	WorldInfo.ForceGarbageCollection( TRUE );

	for( SentinelIdx = 0; SentinelIdx < WorldInfo.StreamingLevels.length; ++SentinelIdx )
	{
		`log( "Gathering locations for: " $ Worldinfo.StreamingLevels[SentinelIdx].PackageName );
		SentinelPC.ClientUpdateLevelStreamingStatus( Worldinfo.StreamingLevels[SentinelIdx].PackageName, TRUE, TRUE, TRUE );
		sleep( 7.0f ); // wait for the level to be streamed back in

		GetTravelLocations( WorldInfo.StreamingLevels[SentinelIdx].PackageName, SentinelPC, SentinelTravelArray );

		DoSentinelActionPerLoadedMap();
		// turn on Memory checking
		SentinelPC.ConsoleCommand( "FractureAllMeshesToMaximizeMemoryUsage" );
		SentinelPC.ConsoleCommand( "stat memory" );
		Sleep( 0.5f );
		DoSentinel_MemoryAtSpecificLocation( vect(0,0,0), rot(0,0,0) );
		SentinelPC.ConsoleCommand( "stat memory" );

		SentinelPC.ClientUpdateLevelStreamingStatus( Worldinfo.StreamingLevels[SentinelIdx].PackageName, FALSE, FALSE, TRUE );
		sleep( 3.0f );
		WorldInfo.ForceGarbageCollection( TRUE );
	}


	// this is the Single Level case (e.g. MP_ levels)
	if( WorldInfo.StreamingLevels.length == 0 )
	{
		GetTravelLocations( WorldInfo.StreamingLevels[SentinelIdx].PackageName, SentinelPC, SentinelTravelArray );
		DoSentinelActionPerLoadedMap();
		// turn on Memory checking
		SentinelPC.ConsoleCommand( "FractureAllMeshesToMaximizeMemoryUsage" );
		SentinelPC.ConsoleCommand( "stat memory" );
		Sleep( 0.5f );
		DoSentinel_MemoryAtSpecificLocation( vect(0,0,0), rot(0,0,0) );
		SentinelPC.ConsoleCommand( "stat memory" );

		sleep( 3.0f );
		WorldInfo.ForceGarbageCollection( TRUE );
	}

	`log( WorldInfo.GetMapName() $ " COMPLETED LEVEL INTEROGATION!! Total TravelPoints: " $ SentinelTravelArray.Length );
	SetIncrementsForLoops( SentinelTravelArray.Length );


	//// so now turn back on streaming AND turn back on streaming for _LOD levels

	for( SentinelIdx = 0; SentinelIdx < WorldInfo.StreamingLevels.length; ++SentinelIdx )
	{
		if( LevelStreamingAlwaysLoaded(Worldinfo.StreamingLevels[SentinelIdx]) != none )
		{
			`log( "   Found a LevelStreamingAlwaysLoaded" @ Worldinfo.StreamingLevels[SentinelIdx].PackageName );
			SentinelPC.ClientUpdateLevelStreamingStatus( Worldinfo.StreamingLevels[SentinelIdx].PackageName, TRUE, TRUE, TRUE );
		}
	}

	SentinelPC.bIsUsingStreamingVolumes = TRUE;


	sleep( 10.0f );

	SentinelPC.Sentinel_PostAcquireTravelTheWorldPoints();

	sleep( 10.0f );

	// add the first point in the list to the end  (so we return back there for our final MemLeakCheck)
	SentinelTravelArray.AddItem( SentinelTravelArray[0] );

	`log( "Starting Traversal" );
	`log( "   SentinelTravelArray.length " $ SentinelTravelArray.length );
	for( SentinelNavigationIdx = 0; SentinelNavigationIdx < SentinelTravelArray.length; SentinelNavigationIdx+=TravelPointsIncrement )
	{
		`log( "Going to:" @ SentinelTravelArray[SentinelNavigationIdx] @ SentinelNavigationIdx $ " of " $ SentinelTravelArray.length );

		SentinelPC.SetLocation( SentinelTravelArray[SentinelNavigationIdx] );
		SentinelPC.SetRotation( rot(0,0,0) );

		sleep( 0.5f );

		// wait until all levels are streamed back in
		do
		{
			bSentinelStreamingLevelStillLoading = FALSE;

			for( SentinelIdx = 0; SentinelIdx < WorldInfo.StreamingLevels.length; ++SentinelIdx )
			{
				if( WorldInfo.StreamingLevels[SentinelIdx].bHasLoadRequestPending == TRUE )
				{
					`log( "levels not streamed in yet sleeping 1s" );
					bSentinelStreamingLevelStillLoading = TRUE;
					Sleep( 1.0f );
					break;
				}
			}
		} until( bSentinelStreamingLevelStillLoading == FALSE );

		WorldInfo.ForceGarbageCollection( TRUE );
		sleep( 1.0f );

		// this is our first point so grab the MemoryData
		if( SentinelNavigationIdx == 0 )
		{
			ConsoleCommand( "MemLeakCheck" );
		}


		// turn on Memory checking
		SentinelPC.ConsoleCommand( "stat memory" );
		Sleep( 0.5f );
		DoSentinel_MemoryAtSpecificLocation( SentinelPC.Location, SentinelPC.Rotation );
		SentinelPC.ConsoleCommand( "stat memory" );

		// turn on stat unit and stat Scene rendering
		SentinelPC.ConsoleCommand( "stat scenerendering" );
		SentinelPC.ConsoleCommand( "stat streaming" );
		Sleep( 1.0f );

		for( SentinelIdx = 0; SentinelIdx < 8; SentinelIdx+=NumRotationsIncrement )
		{
			//`log( "Setting rotation to: " $ 8192*SentinelIdx );
			SentinelPC.SetRotation( rot(0,1,0)*(8192*SentinelIdx) );
			Sleep( 1.5f );
			DoSentinel_ViewDependentMemoryAtSpecificLocation( SentinelPC.Location, SentinelPC.Rotation );
		}

		// turn off stat unit and stat scenerendering
		SentinelPC.ConsoleCommand( "stat scenerendering" );
		SentinelPC.ConsoleCommand( "stat streaming" );


		// get UnitFPS data at each rotation
		for( SentinelIdx = 0; SentinelIdx < 8; SentinelIdx+=NumRotationsIncrement )
		{
			//`log( "Setting rotation to: " $ 8192*SentinelIdx );
			SentinelPC.SetRotation( rot(0,1,0)*(8192*SentinelIdx) );
			Sleep( 1.5f );
			DoSentinel_PerfAtSpecificLocation( SentinelPC.Location, SentinelPC.Rotation );
		}

		//ConsoleCommand( "MemLeakCheck" );
		foreach CommandsToRunAtEachTravelTheWorldNode( CommandStringToExec )
		{
			//`log( `showvar(CommandStringToExec) );
			ConsoleCommand( CommandStringToExec );
		}
	}

	ConsoleCommand( "MemLeakCheck" );

	`log( "COMPLETED!!!!!!!" );
	ConsoleCommand( "exit" );
}

/** 
  *  State code for handling GameInfo CauseEventCommand
  */
state SentinelHandleCauseEventCommand
{
	Begin:
 		// wait until all levels are streamed back in
		do
		{
			bSentinelStreamingLevelStillLoading = FALSE;

			for( SentinelIdx = 0; SentinelIdx < WorldInfo.StreamingLevels.length; ++SentinelIdx )
			{
				if( WorldInfo.StreamingLevels[SentinelIdx].bHasLoadRequestPending == TRUE )
				{
					`log( "levels not streamed in yet sleeping 1s" );
					bSentinelStreamingLevelStillLoading = TRUE;
					Sleep( 1.0f );
					break;
				}
			}

		} until( bSentinelStreamingLevelStillLoading == FALSE );

		// check to see if we should fire off the FlyThrough event again as preround starting usually stops the first event
		if( WorldInfo.Game.CauseEventCommand != "" )
		{
			foreach WorldInfo.AllControllers(class'PlayerController', SentinelPC)
			{
				SentinelPC.ConsoleCommand( "ce " $ WorldInfo.Game.CauseEventCommand );
				break;
			}
		}

		// wait 500 ms to let the switching camera Hitch work itself out
		if( ( SentinelTaskDescription == "FlyThrough" ) || ( SentinelTaskDescription == "FlyThroughSplitScreen" ) )
		{
			SetTimer( 0.500f, TRUE, nameof(DoTimeBasedSentinelStatGathering) );
		}
}

/** This will run on every map load.  (e.g. You have P map which consists of N sublevels.  For each SubLevel this will run. **/
native function DoSentinelActionPerLoadedMap();

/** Add the audio related stats to the database **/
native function HandlePerLoadedMapAudioStats();

/** This will look at the levels and then gather all of the travel points we are interested in **/
native function GetTravelLocations( name LevelName, PlayerController PC, out array<vector> TravelPoints );

/** This will write out the Sentinel data at this location / rotation **/
native function DoSentinel_MemoryAtSpecificLocation( const vector InLocation, const rotator InRotation );

native function DoSentinel_PerfAtSpecificLocation( const out vector InLocation, const out rotator InRotation );

native function DoSentinel_ViewDependentMemoryAtSpecificLocation( const out vector InLocation, const out rotator InRotation );



/** This function should be triggered via SetTimer ever few seconds to do the Per Time Period stats gathering **/
function DoTimeBasedSentinelStatGathering()
{
	local PlayerController PC;
	local vector	ViewLocation;
	local rotator	ViewRotation;

	foreach LocalPlayerControllers( class'PlayerController', PC )
	{
		break;
	}

	PC.GetPlayerViewPoint( ViewLocation, ViewRotation );

	// flythroughs uses the PC and not the pawn
	if( ( SentinelTaskDescription != "FlyThrough" ) && ( SentinelTaskDescription != "FlyThroughSplitScreen" ) )
	{
		if( PC.Pawn != None )
		{
			ViewLocation = PC.Pawn.Location;
		}
	}

	//`log( "DoTimeBasedSentinelStatGathering: " $ ViewLocation @ ViewRotation );
	AddSentinelPerTimePeriodStats( ViewLocation, ViewRotation );
}


//// SENTINEL FUNCTIONS END

/** Determine if memory tracking will be triggered */
native function DoMemoryTracking();

//  AUTOMATED MAP TESTING FUNCTIONS START
/**
 * Start the AutomatedMapTest transition timer which will sit there and poll the status of the streaming levels.
 * When we are doing malloc profiling and such loading is a lot slower so we can't just assume some time limit before moving on.
 **/
event StartAutomatedMapTestTimer()
{
	SetTimer( 5.0, TRUE, nameof(StartAutomatedMapTestTimerWorker) );
}

/** This will look to make certain that all of the streaming levels are finished streaming **/
function StartAutomatedMapTestTimerWorker()
{
	local int LevelIdx;

	if( WorldInfo != none )
	{
		for( LevelIdx = 0; LevelIdx < WorldInfo.StreamingLevels.length; ++LevelIdx )
		{
			if( WorldInfo.StreamingLevels[LevelIdx].bHasLoadRequestPending == TRUE )
			{
				`log( "levels not streamed in yet sleeping 5s" );
				return;
			}
		}

		// now determine whether or not to check for mem leaks
		if( bCheckingForMemLeaks )
		{
			DoMemoryTracking();
		}
	}

	ClearTimer( 'StartAutomatedMapTestTimerWorker' );
	SetTimer( 15.0,false,nameof(CloseAutomatedMapTestTimer) );
}

/** 
  *  Restart the game when timer pops
  */
function CloseAutomatedMapTestTimer()
{
	if( Len(AutomatedMapTestingTransitionMap) > 0)
	{
		if (AutomatedTestingMapIndex < 0)
		{
			WorldInfo.Game.RestartGame();
		}
	}
	else
	{
		WorldInfo.Game.RestartGame();
	}
}

function IncrementAutomatedTestingMapIndex()
{
	if( bUsingAutomatedTestingMapList == TRUE )
	{
		if( bAutomatedTestingWithOpen == TRUE )
		{
			`log( "  NumMapListCyclesDone: " $ NumMapListCyclesDone $ " / " $ NumAutomatedMapTestingCycles );
		}
		else
		{
			if (AutomatedTestingMapIndex >= 0)
			{
				AutomatedTestingMapIndex++;
			}
		}
		`log( "  NextIncrementAutomatedTestingMapIndex: " $ AutomatedTestingMapIndex $ " / " $ AutomatedMapTestingList.Length );
	}
}

function IncrementNumberOfMatchesPlayed()
{
	`log( "  Num Matches Played: " $ NumberOfMatchesPlayed );
	NumberOfMatchesPlayed++;
}


/** @return the map we should travel to during automated testing */
function string GetNextAutomatedTestingMap()
{
	local string MapName;
	local PlayerController PC;
	local bool bResetMapIndex;

	if (bUsingAutomatedTestingMapList)
	{
		// check to see if we are over the end of the list and then increment num cycles and restart
		if ((AutomatedTestingMapIndex >= 0) && (Len(AutomatedMapTestingTransitionMap) > 0))
		{
			// If the testing map index is >= 0, we are in the transition map
			// Increment the map index now... this is to avoid ever trying to set -0
			AutomatedTestingMapIndex++;
			//
			AutomatedTestingMapIndex *= -1;
			MapName = AutomatedMapTestingTransitionMap;
		}
		else
		{
			// Remove the negative if we are using a transition map
			if (Len(AutomatedMapTestingTransitionMap) > 0)
			{
				AutomatedTestingMapIndex *= -1;
			}

			if (AutomatedTestingMapIndex >= AutomatedMapTestingList.Length)
			{
				AutomatedTestingMapIndex = 0;
				NumMapListCyclesDone++;
				bResetMapIndex = true;
			}
			MapName = AutomatedMapTestingList[AutomatedTestingMapIndex];
		}

		if (bAutomatedTestingWithOpen == true)
		{
			// see if we have done all of the cycles we were asked to do
			if ((NumMapListCyclesDone >= NumAutomatedMapTestingCycles) && (NumAutomatedMapTestingCycles != 0))
			{
				if ( bCheckingForMemLeaks )
				{
					ConsoleCommand( "DEFERRED_STOPMEMTRACKING_AND_DUMP" );
				}

				if (bExitOnCyclesComplete)
				{
					ConsoleCommand( "EXIT" );
				}
			}
		}
		else
		{
			foreach WorldInfo.AllControllers(class'PlayerController', PC)
			{
				// check to see if we are over the end of the list and then increment num cycles and restart
				if (bResetMapIndex)
				{
					PC.PlayerReplicationInfo.AutomatedTestingData.NumMapListCyclesDone++;
				}

				// see if we have done all of the cycles we were asked to do
				if ((PC.PlayerReplicationInfo.AutomatedTestingData.NumMapListCyclesDone >= NumAutomatedMapTestingCycles)
					&& (NumAutomatedMapTestingCycles != 0)
					)
				{
					if( bCheckingForMemLeaks )
					{
						ConsoleCommand( "DEFERRED_STOPMEMTRACKING_AND_DUMP" );
					}

					if (bExitOnCyclesComplete)
					{
						ConsoleCommand( "EXIT" );
					}
				}
			}
		}

		`log("NextAutomatedTestingMap: " $ MapName);
		return MapName;
	}

	return "";
}

/**
  * Used to initialize automated testing as needed when match starts.
  * Called from GameInfo.StartMatch().
  */
function StartMatch()
{
	local PlayerController PC;
	
	if ( bAutomatedTestingWithOpen )
	{
		IncrementNumberOfMatchesPlayed();
	}
	else
	{
		foreach WorldInfo.AllControllers(class'PlayerController', PC)
		{
			PC.IncrementNumberOfMatchesPlayed();
			break;
		}
	}
	IncrementAutomatedTestingMapIndex();

	if( bCheckingForFragmentation )
	{
		ConsoleCommand( "MemFragCheck" );
	}

	if( AutomatedTestingExecCommandToRunAtStartMatch != "" )
	{
		`log( "AutomatedTestingExecCommandToRunAtStartMatch: " $ AutomatedTestingExecCommandToRunAtStartMatch );
		ConsoleCommand( AutomatedTestingExecCommandToRunAtStartMatch );
	}
}

/**
  * Start Sentinel Run if needed
  * @return true if normal gameinfo startmatch should be aborted
  */
function bool CheckForSentinelRun()
{
	if( bDoingASentinelRun )
	{
		`log( "DoingASentinelRun! task "$SentinelTaskDescription );
		
		// this will take over the normal match rules and do its own thing
		if( SentinelTaskDescription ~= "TravelTheWorld" )
		{
			WorldInfo.Game.DoTravelTheWorld();
			return true;
		}
		// any of these types are going to run in addition to what ever the player is doing
		// they just go gather stats based on a timer
		else
		{
			BeginSentinelRun( SentinelTaskDescription, SentinelTaskParameter, SentinelTagDesc );
			SetTimer( 3.0f, TRUE, nameof(DoTimeBasedSentinelStatGathering) );
		}
	}
	return false;
}

