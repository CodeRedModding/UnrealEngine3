/** 
 * ChartCreation
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "EnginePrivate.h"
#include "ChartCreation.h"
#include "PerfMem.h"
#include "Database.h"
#include "ProfilingHelpers.h"

/** The GPU time taken to render the last frame. Same metric as appCycles(). */
DWORD			GGPUFrameTime = 0;
/** The total GPU time taken to render all frames. In seconds. */
DOUBLE			GTotalGPUTime = 0;

#if DO_CHARTING

// NOTE:  if you add any new stats make certain to update the ResetFPSChart()

/** Start time of current FPS chart.										*/
DOUBLE			GFPSChartStartTime;
/** FPS chart information. Time spent for each bucket and count.			*/
FFPSChartEntry	GFPSChart[STAT_FPSChartLastBucketStat - STAT_FPSChartFirstStat];

/** Hitch histogram.  How many times the game experienced hitches of various lengths. */
FHitchChartEntry GHitchChart[ STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat ];


/** Time thresholds for the various hitch buckets in milliseconds */
const INT GHitchThresholds[ STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat ] =
	{ 5000, 2500, 2000, 1500, 1000, 750, 500, 300, 200, 150, 100 };

/** Peak lightmap memory. Used by ChartCreation.cpp. */
STAT( extern DWORD GMaxTextureLightmapMemory );
/** Peak shadowmap memory. Used by ChartCreation.cpp. */
STAT( extern DWORD GMaxTextureShadowmapMemory );
/** Peak lightmap memory, if this was running on Xbox. Used by ChartCreation.cpp. */
STATWIN( extern DWORD GMaxTextureLightmapMemoryXbox );
/** Peak shadowmap memory, if this was running on Xbox. Used by ChartCreation.cpp. */
STATWIN( extern DWORD GMaxTextureShadowmapMemoryXbox );


#if CHART_DISTANCE_FACTORS
/** Chart of DistanceFactors that skeletal meshes are rendered at. */
INT				GDistanceFactorChart[NUM_DISTANCEFACTOR_BUCKETS];
/** Range for each DistanceFactor bucket. */
FLOAT			GDistanceFactorDivision[NUM_DISTANCEFACTOR_BUCKETS-1]={0.01f,0.02f,0.03f,0.05f,0.075f,0.1f,0.15f,0.25f,0.5f,0.75f,1.f,1.5f,2.f};
#endif

/** Number of frames for each time of <boundtype> **/
UINT GNumFramesBound_GameThread = 0;
UINT GNumFramesBound_RenderThread = 0;
UINT GNumFramesBound_GPU = 0;

DOUBLE GTotalFramesBoundTime_GameThread = 0;
DOUBLE GTotalFramesBoundTime_RenderThread = 0;
DOUBLE GTotalFramesBoundTime_GPU = 0;

/** Whether capturing FPS chart info is enabled. */
UBOOL GIsCapturingFPSChartInfo = FALSE;

/** Arrays of render/game/GPU and total frame times. Captured and written out if FPS charting is enabled */
TArray<FLOAT> GRenderThreadFrameTimes;
TArray<FLOAT> GGameThreadFrameTimes;
TArray<FLOAT> GGPUFrameTimes;
TArray<FLOAT> GFrameTimes;

// Memory 
/** Contains all of the memory info */
TArray<FMemoryChartEntry> GMemoryChart;

/** How long between memory chart updates **/
FLOAT GTimeBetweenMemoryChartUpdates = 0;

/** When the memory chart was last updated **/
DOUBLE GLastTimeMemoryChartWasUpdated = 0;


/** 
 * This will look at where the map is doing a bot test or a flythrough test.
 *
 * @ return the name of the chart to use
 **/
static FString GetFPSChartType()
{
	FString Retval;

	// causeevent=FlyThrough is what the FlyThrough event is
	if( FString(appCmdLine()).InStr( TEXT( "causeevent=FlyThrough" ), FALSE, TRUE ) != -1 )
	{
		Retval = TEXT ( "FlyThrough" );
	}
	else
	{
		Retval = TEXT ( "FPS" );
	}

	return Retval;
}


/**
 * This will create the file name for the file we are saving out.
 *
 * @param the type of chart we are making
 **/
FString CreateFileNameForChart( const FString& ChartType, const FString& FileExtension, UBOOL bOutputToGlobalLog )
{
	FString Retval;

	// Map name we're dumping.
	FString MapName;
	if( bOutputToGlobalLog == TRUE )
	{
		MapName = TEXT( "Global" );
	}
	else
	{
		MapName = GWorld ? GWorld->GetMapName() : TEXT("None");
	}

	// Create FPS chart filename.
	FString Platform;
	// determine which platform we are
#if XBOX
	Platform = TEXT( "Xbox360" );
#elif PS3
	Platform = TEXT( "PS3" );
#else
	Platform = TEXT( "PC" );
#endif

	Retval = MapName + TEXT("-") + ChartType + TEXT("-") + Platform + FileExtension;

	return Retval;
}


/**
* Ticks the FPS chart.
*
* @param DeltaSeconds	Time in seconds passed since last tick.
*/
void UEngine::TickFPSChart( FLOAT DeltaSeconds )
{
	// We can't trust delta seconds if frame time clamping is enabled or if we're benchmarking so we simply
	// calculate it ourselves.
	static DOUBLE LastTime = 0;

	// Keep track of our previous frame's statistics
	static FLOAT LastDeltaSeconds = 0.0f;

	static DOUBLE LastHitchTime = 0;

	const DOUBLE CurrentTime = appSeconds();
	if( LastTime > 0 )
	{
		DeltaSeconds = CurrentTime - LastTime;
	}
	LastTime = CurrentTime;

	if (GEngine->GamePlayers.Num())
	{
		ULocalPlayer* Player = GEngine->GamePlayers(0);

		UBOOL bShouldTrackFPSWhenNonInteractive = FALSE;
		GConfig->GetBool( TEXT("FPSChartTracking"), TEXT("ShouldTrackFPSWhenNonInteractive"), (UBOOL&)bShouldTrackFPSWhenNonInteractive, GEngineIni );

		if (Player && Player->Actor && !bShouldTrackFPSWhenNonInteractive && !Player->Actor->bInteractiveMode )
		{
			// Update state that will be used next frame before we early out
			LastHitchTime = CurrentTime;
			LastDeltaSeconds = DeltaSeconds;

			// Skip fps tracking if we are in a non-interactive sequence, as that would have a different set of targets
			//@todo - Split fps stats into groups, so that we can track interactive and non-interactive sequences separately
			return;
		}
	}

	// now gather some stats on what this frame was bound by (game, render, gpu)
	extern DWORD GGameThreadTime;
	extern DWORD GRenderThreadTime;
	extern DWORD GGPUFrameTime;

	// Copy these locally since the RT may update it between reads otherwise
	const DWORD LocalRenderThreadTime = GRenderThreadTime;
	const DWORD LocalGPUFrameTime = GGPUFrameTime;

	// determine which Time is the greatest and less than 33.33 (as if we are above 30 fps we are happy and thus not "bounded" )
	const FLOAT Epsilon = 0.250f;
	DWORD MaxThreadTimeValue = Max3<DWORD>( LocalRenderThreadTime, GGameThreadTime, LocalGPUFrameTime );
	const FLOAT FrameTime = MaxThreadTimeValue * GSecondsPerCycle;

	// so we want to try and guess when we are bound by the GPU even when on the xenon we do not get that info
	// If the frametime is bigger than 35 ms we can take DeltaSeconds into account as we're not VSYNCing in that case.
	DWORD PossibleGPUTime = LocalGPUFrameTime;
	if( PossibleGPUTime == 0 )
	{
		// if we are over
		PossibleGPUTime = static_cast<DWORD>(Max( FrameTime, DeltaSeconds ) / GSecondsPerCycle );
		MaxThreadTimeValue = Max3<DWORD>( GGameThreadTime, LocalRenderThreadTime, PossibleGPUTime );
	}

	// Disregard frames that took longer than one second when accumulating data.
	if( DeltaSeconds < 1.f )
	{
		const FLOAT CurrentFPS = 1 / DeltaSeconds;

		if( CurrentFPS < 5 )
		{
			GFPSChart[ STAT_FPSChart_0_5 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_0_5 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 10 )
		{
			GFPSChart[ STAT_FPSChart_5_10 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_5_10 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 15 )
		{
			GFPSChart[ STAT_FPSChart_10_15 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_10_15 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 20 )
		{
			GFPSChart[ STAT_FPSChart_15_20 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_15_20 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 25 )
		{
			GFPSChart[ STAT_FPSChart_20_25 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_20_25 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 30 )
		{
			GFPSChart[ STAT_FPSChart_25_30 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_25_30 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 35 )
		{
			GFPSChart[ STAT_FPSChart_30_35 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_30_35 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 40 )
		{
			GFPSChart[ STAT_FPSChart_35_40 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_35_40 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 45 )
		{
			GFPSChart[ STAT_FPSChart_40_45 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_40_45 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 50 )
		{
			GFPSChart[ STAT_FPSChart_45_50 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_45_50 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 55 )
		{
			GFPSChart[ STAT_FPSChart_50_55 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_50_55 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 60 )
		{
			GFPSChart[ STAT_FPSChart_55_60 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_55_60 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else
		{
			GFPSChart[ STAT_FPSChart_60_INF - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_60_INF - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}

		GTotalGPUTime += LocalGPUFrameTime * GSecondsPerCycle;

		// if Frametime is > 33 ms then we are bounded by something
		if( CurrentFPS < 30 )
		{
			// If GPU time is inferred we can only determine GPU > 33 ms if we are GPU bound.
			UBOOL bAreWeGPUBoundIfInferred = TRUE;
			// 33.333ms
			const FLOAT TargetThreadTimeSeconds = .0333333f;

			if( GGameThreadTime * GSecondsPerCycle >= TargetThreadTimeSeconds )
			{
				GNumFramesBound_GameThread++;
				GTotalFramesBoundTime_GameThread += DeltaSeconds;
				bAreWeGPUBoundIfInferred = FALSE;
			}

			if( LocalRenderThreadTime * GSecondsPerCycle >= TargetThreadTimeSeconds )
			{
				GNumFramesBound_RenderThread++;
				GTotalFramesBoundTime_RenderThread += DeltaSeconds;
				bAreWeGPUBoundIfInferred = FALSE;
			}			

			// Consider this frame GPU bound if we have an actual measurement which is over the limit,
			if( LocalGPUFrameTime != 0 && LocalGPUFrameTime * GSecondsPerCycle >= TargetThreadTimeSeconds
				// Or if we don't have a measurement but neither of the other threads were the slowest
				|| LocalGPUFrameTime == 0 && bAreWeGPUBoundIfInferred && PossibleGPUTime == MaxThreadTimeValue )
			{
				GTotalFramesBoundTime_GPU += DeltaSeconds;
				GNumFramesBound_GPU++;
			}
		}
	}

	// Track per frame stats.
	if( GIsCapturingFPSChartInfo )
	{		
		GGameThreadFrameTimes.AddItem( GGameThreadTime * GSecondsPerCycle );
		GRenderThreadFrameTimes.AddItem( LocalRenderThreadTime * GSecondsPerCycle );
		GGPUFrameTimes.AddItem( LocalGPUFrameTime * GSecondsPerCycle );
		GFrameTimes.AddItem(DeltaSeconds);
	}

	// Check for hitches
	{
		// Minimum time quantum before we'll even consider this a hitch
		const FLOAT MinFrameTimeToConsiderAsHitch = 0.1f;	// 10 FPS

		// Minimum time passed before we'll record a new hitch
		const FLOAT MinTimeBetweenHitches = 0.5f;

		// For the current frame to be considered a hitch, it must have run at least this many times slower than
		// the previous frame
		const FLOAT HitchMultiplierAmount = 1.75f;

		// Ignore frames faster than our threshold
		if( DeltaSeconds >= MinFrameTimeToConsiderAsHitch )
		{
			// How long has it been since the last hitch we detected?
			const FLOAT TimeSinceLastHitch = ( FLOAT )( CurrentTime - LastHitchTime );

			// Make sure at least a little time has passed since the last hitch we reported
			if( TimeSinceLastHitch >= MinTimeBetweenHitches )
			{
				// If our frame time is much larger than our last frame time, we'll count this as a hitch!
				if( DeltaSeconds > LastDeltaSeconds * HitchMultiplierAmount )
				{
					// We have a hitch!
					
					// Track the hitch by bucketing it based on time severity
					const INT HitchBucketCount = STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat;
					for( INT CurBucketIndex = 0; CurBucketIndex < HitchBucketCount; ++CurBucketIndex )
					{
						FLOAT HitchThresholdInSeconds = ( FLOAT )GHitchThresholds[ CurBucketIndex ] * 0.001f;
						if( DeltaSeconds >= HitchThresholdInSeconds )
						{
							// Increment the hitch count for this bucket
							++GHitchChart[ CurBucketIndex ].HitchCount;

						
							// Check to see what we were limited by this frame
							if( GGameThreadTime >= (MaxThreadTimeValue - Epsilon) )
							{
								// Bound by game thread
								++GHitchChart[ CurBucketIndex ].GameThreadBoundHitchCount;
							}
							else if( LocalRenderThreadTime >= (MaxThreadTimeValue - Epsilon) )
							{
								// Bound by render thread
								++GHitchChart[ CurBucketIndex ].RenderThreadBoundHitchCount;
							}
							else if( PossibleGPUTime == MaxThreadTimeValue )
							{
								// Bound by GPU
								++GHitchChart[ CurBucketIndex ].GPUBoundHitchCount;
							}


							// Found the bucket for this hitch!  We're done now.
							break;
						}
					}

					LastHitchTime = CurrentTime;
				}
			}
		}

		// Store stats for the next frame to look at
		LastDeltaSeconds = DeltaSeconds;
	}

#if STATS
	// Propagate gathered data to stats system.

	// Iterate over all buckets, gathering total frame count and cumulative time.
	FLOAT TotalTime = 0;
	INT	  NumFrames = 0;
	for( INT BucketIndex=0; BucketIndex<ARRAY_COUNT(GFPSChart); BucketIndex++ )
	{
		NumFrames += GFPSChart[BucketIndex].Count;
		TotalTime += GFPSChart[BucketIndex].CummulativeTime;
	}

	// Set percentage stats.
	FLOAT ThirtyPlusPercentage = 0;
	for( INT BucketIndex=0; BucketIndex<ARRAY_COUNT(GFPSChart); BucketIndex++ )
	{
		FLOAT BucketPercentage = 0.f;
		if( TotalTime > 0.f )
		{
			BucketPercentage = (GFPSChart[BucketIndex].CummulativeTime / TotalTime) * 100;
		}
		if( (BucketIndex + STAT_FPSChartFirstStat) >= STAT_FPSChart_30_35 )
		{
			ThirtyPlusPercentage += BucketPercentage;
		}
		SET_FLOAT_STAT( BucketIndex + STAT_FPSChartFirstStat, BucketPercentage );
	}

	// Update unaccounted time and frame count stats.
	const FLOAT UnaccountedTime = appSeconds() - GFPSChartStartTime - TotalTime;
	SET_FLOAT_STAT( STAT_FPSChart_30Plus, ThirtyPlusPercentage );
	SET_FLOAT_STAT( STAT_FPSChart_UnaccountedTime, UnaccountedTime );
	SET_DWORD_STAT( STAT_FPSChart_FrameCount, NumFrames );


	// Update hitch stats
	{
		INT TotalHitchCount = 0;
		INT TotalGameThreadBoundHitches = 0;
		INT TotalRenderThreadBoundHitches = 0;
		INT TotalGPUBoundHitches = 0;
		for( INT BucketIndex = 0; BucketIndex < ARRAY_COUNT( GHitchChart ); ++BucketIndex )
		{
			SET_DWORD_STAT( STAT_FPSChart_FirstHitchStat + BucketIndex, GHitchChart[ BucketIndex ].HitchCount );

			// Count up the total number of hitches
			TotalHitchCount += GHitchChart[ BucketIndex ].HitchCount;
			TotalGameThreadBoundHitches += GHitchChart[ BucketIndex ].GameThreadBoundHitchCount;
			TotalRenderThreadBoundHitches += GHitchChart[ BucketIndex ].RenderThreadBoundHitchCount;
			TotalGPUBoundHitches += GHitchChart[ BucketIndex ].GPUBoundHitchCount;
		}

		SET_DWORD_STAT( STAT_FPSChart_TotalHitchCount, TotalHitchCount );
	}
#endif
}

/**
 * Resets the FPS chart data.
 */
void UEngine::ResetFPSChart()
{
	for( INT BucketIndex=0; BucketIndex<ARRAY_COUNT(GFPSChart); BucketIndex++ )
	{
		GFPSChart[BucketIndex].Count = 0;
		GFPSChart[BucketIndex].CummulativeTime = 0;
	}
	GFPSChartStartTime = appSeconds();

	for( INT BucketIndex = 0; BucketIndex < ARRAY_COUNT( GHitchChart ); ++BucketIndex )
	{
		GHitchChart[ BucketIndex ].HitchCount = 0;
		GHitchChart[ BucketIndex ].GameThreadBoundHitchCount = 0;
		GHitchChart[ BucketIndex ].RenderThreadBoundHitchCount = 0;
		GHitchChart[ BucketIndex ].GPUBoundHitchCount = 0;
	}

	// Pre-allocate 10 minutes worth of frames at 30 Hz.
	INT NumFrames = 10 * 60 * 30;
	GRenderThreadFrameTimes.Reset(NumFrames);
	GGPUFrameTimes.Reset(NumFrames);
	GGameThreadFrameTimes.Reset(NumFrames);
	GFrameTimes.Reset(NumFrames);

	GTotalGPUTime = 0;
	GGPUFrameTime = 0;

	GNumFramesBound_GameThread = 0;
	GNumFramesBound_RenderThread = 0;
	GNumFramesBound_GPU = 0;

	GTotalFramesBoundTime_GameThread = 0;
	GTotalFramesBoundTime_RenderThread = 0;
	GTotalFramesBoundTime_GPU = 0;
}


/**
 * Dumps the FPS chart information to the log.
 */
void UEngine::DumpFPSChartToLog( FLOAT TotalTime, FLOAT DeltaTime, INT NumFrames )
{
	// Map name we're dumping.
	const FString MapName = GWorld ? GWorld->GetMapName() : TEXT("None");

	INT NumFramesBelow30 = 0; // keep track of the number of frames below 30 FPS
	FLOAT PctTimeAbove30 = 0; // Keep track of percentage of time at 30+ FPS.

	debugf(TEXT("--- Begin : FPS chart dump for level '%s'"), *MapName);

	// Iterate over all buckets, dumping percentages.
	for( INT BucketIndex=0; BucketIndex<ARRAY_COUNT(GFPSChart); BucketIndex++ )
	{
		// Figure out bucket time and frame percentage.
		const FLOAT BucketTimePercentage  = 100.f * GFPSChart[BucketIndex].CummulativeTime / TotalTime;
		const FLOAT BucketFramePercentage = 100.f * GFPSChart[BucketIndex].Count / NumFrames;

		// Figure out bucket range.
		const INT StartFPS	= BucketIndex * 5;
		INT EndFPS		= StartFPS + 5;
		if( BucketIndex + STAT_FPSChartFirstStat == STAT_FPSChart_60_INF )
		{
			EndFPS = 99;
		}

		// Keep track of time spent at 30+ FPS.
		if( StartFPS >= 30 )
		{
			PctTimeAbove30 += BucketTimePercentage;
		}
		else
		{
			NumFramesBelow30 += GFPSChart[BucketIndex].Count;
		}

		// Log bucket index, time and frame Percentage.
		debugf(TEXT("Bucket: %2i - %2i  Time: %5.2f  Frame: %5.2f"), StartFPS, EndFPS, BucketTimePercentage, BucketFramePercentage );

		if( GSentinelRunID != -1 )
		{
			// send the FPS Bucket info to the DB
			const FString StatGroupName = TEXT( "FPSBuckets" );
			const FString StatName = FString::Printf(TEXT("%d-%d"), StartFPS, EndFPS );
			const FString AddRunData = FString::Printf(TEXT("EXEC AddRunData @RunID=%i, @StatGroupName='%s', @StatName='%s', @StatValue=%f")
				, GSentinelRunID
				, *StatGroupName
				, *StatName
				, BucketFramePercentage
				);

			//debugf( *AddRunData );

			GTaskPerfMemDatabase->SendExecCommand( *AddRunData );
		}
	}

	if( GSentinelRunID != -1 )
	{
		FString AddRunData = FString::Printf(TEXT("EXEC AddRunData @RunID=%i, @StatGroupName='%s', @StatName='%s', @StatValue=%f")
			, GSentinelRunID
			, TEXT( "FPSBuckets" )
			, TEXT( "PctFramesAbove30" )
			, FLOAT(NumFrames - NumFramesBelow30) / FLOAT(NumFrames)*100.0f
			);
		GTaskPerfMemDatabase->SendExecCommand( *AddRunData );

		AddRunData = FString::Printf(TEXT("EXEC AddRunData @RunID=%i, @StatGroupName='%s', @StatName='%s', @StatValue=%f")
			, GSentinelRunID
			, TEXT( "FPSBuckets" )
			, TEXT( "PctBound_GameThread" )
			, (FLOAT(GNumFramesBound_GameThread)/FLOAT(NumFrames))*100.0f
			);
		GTaskPerfMemDatabase->SendExecCommand( *AddRunData );

		AddRunData = FString::Printf(TEXT("EXEC AddRunData @RunID=%i, @StatGroupName='%s', @StatName='%s', @StatValue=%f")
			, GSentinelRunID
			, TEXT( "FPSBuckets" )
			, TEXT( "PctBound_RenderThread" )
			, (FLOAT(GNumFramesBound_RenderThread)/FLOAT(NumFrames))*100.0f
			);
		GTaskPerfMemDatabase->SendExecCommand( *AddRunData );

		AddRunData = FString::Printf(TEXT("EXEC AddRunData @RunID=%i, @StatGroupName='%s', @StatName='%s', @StatValue=%f")
			, GSentinelRunID
			, TEXT( "FPSBuckets" )
			, TEXT( "PctBound_GPU" )
			, (FLOAT(GNumFramesBound_GPU)/FLOAT(NumFrames))*100.0f
			);
		GTaskPerfMemDatabase->SendExecCommand( *AddRunData );



		AddRunData = FString::Printf(TEXT("EXEC AddRunData @RunID=%i, @StatGroupName='%s', @StatName='%s', @StatValue=%f")
			, GSentinelRunID
			, TEXT( "FPSBuckets" )
			, TEXT( "PctTimeAbove30" )
			, PctTimeAbove30
			);
		GTaskPerfMemDatabase->SendExecCommand( *AddRunData );

		AddRunData = FString::Printf(TEXT("EXEC AddRunData @RunID=%i, @StatGroupName='%s', @StatName='%s', @StatValue=%f")
			, GSentinelRunID
			, TEXT( "FPSBuckets" )
			, TEXT( "PctTimeBound_GameThread" )
			, (GTotalFramesBoundTime_GameThread/DeltaTime)*100.0f
			);
		GTaskPerfMemDatabase->SendExecCommand( *AddRunData );

		AddRunData = FString::Printf(TEXT("EXEC AddRunData @RunID=%i, @StatGroupName='%s', @StatName='%s', @StatValue=%f")
			, GSentinelRunID
			, TEXT( "FPSBuckets" )
			, TEXT( "PctTimeBound_RenderThread" )
			, ((GTotalFramesBoundTime_RenderThread)/DeltaTime)*100.0f
			);
		GTaskPerfMemDatabase->SendExecCommand( *AddRunData );

		AddRunData = FString::Printf(TEXT("EXEC AddRunData @RunID=%i, @StatGroupName='%s', @StatName='%s', @StatValue=%f")
			, GSentinelRunID
			, TEXT( "FPSBuckets" )
			, TEXT( "PctTimeBound_GPU" )
			, ((GTotalFramesBoundTime_GPU)/DeltaTime)*100.0f
			);
		GTaskPerfMemDatabase->SendExecCommand( *AddRunData );

	}


	debugf(TEXT("%i frames collected over %4.2f seconds, disregarding %4.2f seconds for a %4.2f FPS average, %4.2f percent of time spent > 30 FPS"),
		NumFrames, 
		DeltaTime, 
		Max<FLOAT>( 0, DeltaTime - TotalTime ), 
		NumFrames / TotalTime,
		PctTimeAbove30);
	debugf(TEXT("Average GPU frametime: %4.2f ms"), FLOAT((GTotalGPUTime / NumFrames)*1000.0));
	debugf(TEXT("BoundGameThreadPct: %4.2f  BoundRenderThreadPct: %4.2f  BoundGPUPct: %4.2f PercentFrames30+: %f   BoundGameTime: %f  BoundRenderTime: %f  BoundGPUTime: %f  PctTimeAbove30: %f ")
		, (FLOAT(GNumFramesBound_GameThread)/FLOAT(NumFrames))*100.0f
		, (FLOAT(GNumFramesBound_RenderThread)/FLOAT(NumFrames))*100.0f
		, (FLOAT(GNumFramesBound_GPU)/FLOAT(NumFrames))*100.0f
		, FLOAT(NumFrames - NumFramesBelow30) / FLOAT(NumFrames)*100.0f
		, (GTotalFramesBoundTime_GameThread/DeltaTime)*100.0f
		, ((GTotalFramesBoundTime_RenderThread)/DeltaTime)*100.0f
		, ((GTotalFramesBoundTime_GPU)/DeltaTime)*100.0f
		, PctTimeAbove30
		);


	debugf(TEXT("--- End"));


	// Dump hitch data
	{
		debugf( TEXT( "--- Begin : Hitch chart dump for level '%s'" ), *MapName );

		INT TotalHitchCount = 0;
		INT TotalGameThreadBoundHitches = 0;
		INT TotalRenderThreadBoundHitches = 0;
		INT TotalGPUBoundHitches = 0;
		for( INT BucketIndex = 0; BucketIndex < ARRAY_COUNT( GHitchChart ); ++BucketIndex )
		{
			const FLOAT HitchThresholdInSeconds = ( FLOAT )GHitchThresholds[ BucketIndex ] * 0.001f;

			FString RangeName;
			if( BucketIndex == 0 )
			{
				// First bucket's end threshold is infinitely large
				RangeName = FString::Printf( TEXT( "%0.2fs - inf" ), HitchThresholdInSeconds );
			}
			else
			{
				const FLOAT PrevHitchThresholdInSeconds = ( FLOAT )GHitchThresholds[ BucketIndex - 1 ] * 0.001f;

				// Set range from current bucket threshold to the last bucket's threshold
				RangeName = FString::Printf( TEXT( "%0.2fs - %0.2fs" ), HitchThresholdInSeconds, PrevHitchThresholdInSeconds );
			}

			debugf( TEXT( "Bucket: %s  Count: %i " ), *RangeName, GHitchChart[ BucketIndex ].HitchCount );


			// Count up the total number of hitches
			TotalHitchCount += GHitchChart[ BucketIndex ].HitchCount;
			TotalGameThreadBoundHitches += GHitchChart[ BucketIndex ].GameThreadBoundHitchCount;
			TotalRenderThreadBoundHitches += GHitchChart[ BucketIndex ].RenderThreadBoundHitchCount;
			TotalGPUBoundHitches += GHitchChart[ BucketIndex ].GPUBoundHitchCount;
		}

		const INT HitchBucketCount = STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat;
		debugf( TEXT( "Total hitch count (at least %ims):  %i" ), GHitchThresholds[ HitchBucketCount - 1 ], TotalHitchCount );
		debugf( TEXT( "Hitch frames bound by game thread:  %i  (%0.1f percent)" ), TotalGameThreadBoundHitches, 
			(TotalHitchCount > 0) ? ( ( FLOAT )TotalGameThreadBoundHitches / ( FLOAT )TotalHitchCount * 100.0f ) : 0.0f );
		debugf( TEXT( "Hitch frames bound by render thread:  %i  (%0.1f percent)" ), TotalRenderThreadBoundHitches, TotalHitchCount > 0 ? ( ( FLOAT )TotalRenderThreadBoundHitches / ( FLOAT )TotalHitchCount * 0.0f ) : 0.0f  );
		debugf( TEXT( "Hitch frames bound by GPU:  %i  (%0.1f  percent)" ), TotalGPUBoundHitches, TotalHitchCount > 0 ? ( ( FLOAT )TotalGPUBoundHitches / ( FLOAT )TotalHitchCount * 100.0f ) : 0.0f );

		debugf( TEXT( "--- End" ) );
	}
}



void UEngine::DumpDistanceFactorChart()
{
#if CHART_DISTANCE_FACTORS
	INT TotalStats = 0;
	for(INT i=0; i<NUM_DISTANCEFACTOR_BUCKETS; i++)
	{
		TotalStats += GDistanceFactorChart[i];
	}
	const FLOAT TotalStatsF = (FLOAT)TotalStats;

	if(TotalStats > 0)
	{
		debugf(TEXT("--- DISTANCEFACTOR CHART ---"));
		debugf(TEXT("<%2.3f\t%3.2f"), GDistanceFactorDivision[0], 100.f*GDistanceFactorChart[0]/TotalStatsF);
		for(INT i=1; i<NUM_DISTANCEFACTOR_BUCKETS-1; i++)
		{
			const FLOAT BucketStart = GDistanceFactorDivision[i-1];
			const FLOAT BucketEnd = GDistanceFactorDivision[i];
			debugf(TEXT("%2.3f-%2.3f\t%3.2f"), BucketStart, BucketEnd, 100.f*GDistanceFactorChart[i]/TotalStatsF);
		}
		debugf(TEXT(">%2.3f\t%3.2f"), GDistanceFactorDivision[NUM_DISTANCEFACTOR_BUCKETS-2], 100.f*GDistanceFactorChart[NUM_DISTANCEFACTOR_BUCKETS-1]/TotalStatsF);
	}
#endif // CHART_DISTANCE_FACTORS
}


/**
 * Dumps the frame times information to the special stats log file.
 */
void UEngine::DumpFrameTimesToStatsLog( FLOAT TotalTime, FLOAT DeltaTime, INT NumFrames )
{
#if ALLOW_DEBUG_FILES
	// Create folder for FPS chart data.
	FString OutputDir = appProfilingDir() + GSystemStartTime + PATH_SEPARATOR + TEXT("FrameTimeStats") + PATH_SEPARATOR;
	GFileManager->MakeDirectory( *OutputDir, TRUE );

	// Create archive for log data.
	const FString ChartType = GetFPSChartType();
	const FString ChartName = (OutputDir + CreateFileNameForChart( ChartType, TEXT( ".csv" ), FALSE ) );
	FArchive* OutputFile = GFileManager->CreateDebugFileWriter( *ChartName );

	if( OutputFile )
	{
		OutputFile->Logf(TEXT("Time (sec),Frame (ms), GT (ms), RT (ms), GPU (ms)"));
		FLOAT ElapsedTime = 0;
		for( INT i=0; i<GFrameTimes.Num(); i++ )
		{
			OutputFile->Logf(TEXT("%f,%f,%f,%f,%f"),ElapsedTime,GFrameTimes(i)*1000,GGameThreadFrameTimes(i)*1000,GRenderThreadFrameTimes(i)*1000,GGPUFrameTimes(i)*1000);
			ElapsedTime += GFrameTimes(i);
		}
		delete OutputFile;
	}
#endif
}


/**
 * Dumps the FPS chart information to the special stats log file.
 */
void UEngine::DumpFPSChartToStatsLog( FLOAT TotalTime, FLOAT DeltaTime, INT NumFrames )
{
#if ALLOW_DEBUG_FILES
	// Create folder for FPS chart data.
	FString OutputDir = appProfilingDir() + GSystemStartTime + PATH_SEPARATOR + TEXT("FPSChartStats") + PATH_SEPARATOR;
	GFileManager->MakeDirectory( *OutputDir );
	
	// Create archive for log data.
	const FString ChartType = GetFPSChartType();
	const FString ChartName = (OutputDir + CreateFileNameForChart( ChartType, TEXT( ".log" ), FALSE ) );
	FArchive* OutputFile = GFileManager->CreateDebugFileWriter( *ChartName, FILEWRITE_Append );

	if( OutputFile )
	{
		OutputFile->Logf(TEXT("Dumping FPS chart at %s using build %i built from changelist %i"), *appSystemTimeString(), GEngineVersion, GetChangeListNumberForPerfTesting() );

		INT NumFramesBelow30 = 0; // keep track of the number of frames below 30 FPS
		FLOAT PctTimeAbove30 = 0; // Keep track of percentage of time at 30+ FPS.

		// Iterate over all buckets, dumping percentages.
		for( INT BucketIndex=0; BucketIndex<ARRAY_COUNT(GFPSChart); BucketIndex++ )
		{
			// Figure out bucket time and frame percentage.
			const FLOAT BucketTimePercentage  = 100.f * GFPSChart[BucketIndex].CummulativeTime / TotalTime;
			const FLOAT BucketFramePercentage = 100.f * GFPSChart[BucketIndex].Count / NumFrames;

			// Figure out bucket range.
			const INT StartFPS	= BucketIndex * 5;
			INT EndFPS		= StartFPS + 5;
			if( BucketIndex + STAT_FPSChartFirstStat == STAT_FPSChart_60_INF )
			{
				EndFPS = 99;
			}

			// Keep track of time spent at 30+ FPS.
			if( StartFPS >= 30 )
			{
				PctTimeAbove30 += BucketTimePercentage;
			}
			else
			{
				NumFramesBelow30 += GFPSChart[BucketIndex].Count;
			}

			// Log bucket index, time and frame Percentage.
			OutputFile->Logf(TEXT("Bucket: %2i - %2i  Time: %5.2f  Frame: %5.2f"), StartFPS, EndFPS, BucketTimePercentage, BucketFramePercentage);
		}

		OutputFile->Logf(TEXT("%i frames collected over %4.2f seconds, disregarding %4.2f seconds for a %4.2f FPS average, %4.2f percent of time spent > 30 FPS"), 
			NumFrames, 
			DeltaTime, 
			Max<FLOAT>( 0, DeltaTime - TotalTime ), 
			NumFrames / TotalTime,
			PctTimeAbove30);
		OutputFile->Logf(TEXT("Average GPU frame time: %4.2f ms"), FLOAT((GTotalGPUTime / NumFrames)*1000.0));
		OutputFile->Logf(TEXT("BoundGameThreadPct: %4.2f  BoundRenderThreadPct: %4.2f  BoundGPUPct: %4.2f PercentFrames30+: %f   BoundGameTime: %f  BoundRenderTime: %f  BoundGPUTime: %f  PctTimeAbove30: %f ")
			, (FLOAT(GNumFramesBound_GameThread)/FLOAT(NumFrames))*100.0f
			, (FLOAT(GNumFramesBound_RenderThread)/FLOAT(NumFrames))*100.0f
			, (FLOAT(GNumFramesBound_GPU)/FLOAT(NumFrames))*100.0f
			, FLOAT(NumFrames - NumFramesBelow30) / FLOAT(NumFrames)*100.0f
			, (GTotalFramesBoundTime_GameThread/DeltaTime)*100.0f
			, ((GTotalFramesBoundTime_RenderThread)/DeltaTime)*100.0f
			, ((GTotalFramesBoundTime_GPU)/DeltaTime)*100.0f
			, PctTimeAbove30
			);

		// Dump hitch data
		{
			OutputFile->Logf( TEXT( "Hitch chart:" ) );

			INT TotalHitchCount = 0;
			INT TotalGameThreadBoundHitches = 0;
			INT TotalRenderThreadBoundHitches = 0;
			INT TotalGPUBoundHitches = 0;
			for( INT BucketIndex = 0; BucketIndex < ARRAY_COUNT( GHitchChart ); ++BucketIndex )
			{
				const FLOAT HitchThresholdInSeconds = ( FLOAT )GHitchThresholds[ BucketIndex ] * 0.001f;

				FString RangeName;
				if( BucketIndex == 0 )
				{
					// First bucket's end threshold is infinitely large
					RangeName = FString::Printf( TEXT( "%0.2fs - inf" ), HitchThresholdInSeconds );
				}
				else
				{
					const FLOAT PrevHitchThresholdInSeconds = ( FLOAT )GHitchThresholds[ BucketIndex - 1 ] * 0.001f;

					// Set range from current bucket threshold to the last bucket's threshold
					RangeName = FString::Printf( TEXT( "%0.2fs - %0.2fs" ), HitchThresholdInSeconds, PrevHitchThresholdInSeconds );
				}

				OutputFile->Logf( TEXT( "Bucket: %s  Count: %i " ), *RangeName, GHitchChart[ BucketIndex ].HitchCount );


				// Count up the total number of hitches
				TotalHitchCount += GHitchChart[ BucketIndex ].HitchCount;
				TotalGameThreadBoundHitches += GHitchChart[ BucketIndex ].GameThreadBoundHitchCount;
				TotalRenderThreadBoundHitches += GHitchChart[ BucketIndex ].RenderThreadBoundHitchCount;
				TotalGPUBoundHitches += GHitchChart[ BucketIndex ].GPUBoundHitchCount;
			}

			const INT HitchBucketCount = STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat;
			OutputFile->Logf( TEXT( "Total hitch count (at least %ims):  %i" ), GHitchThresholds[ HitchBucketCount - 1 ], TotalHitchCount );
			OutputFile->Logf( TEXT( "Hitch frames bound by game thread:  %i  (%0.1f%%)" ), TotalGameThreadBoundHitches, TotalHitchCount > 0 ? ( ( FLOAT )TotalGameThreadBoundHitches / ( FLOAT )TotalHitchCount * 100.0f ) : 0.0f );
			OutputFile->Logf( TEXT( "Hitch frames bound by render thread:  %i  (%0.1f%%)" ), TotalRenderThreadBoundHitches, TotalHitchCount > 0 ? ( ( FLOAT )TotalRenderThreadBoundHitches / ( FLOAT )TotalHitchCount * 0.0f ) : 0.0f  );
			OutputFile->Logf( TEXT( "Hitch frames bound by GPU:  %i  (%0.1f%%)" ), TotalGPUBoundHitches, TotalHitchCount > 0 ? ( ( FLOAT )TotalGPUBoundHitches / ( FLOAT )TotalHitchCount * 100.0f ) : 0.0f );
		}

		OutputFile->Logf( LINE_TERMINATOR LINE_TERMINATOR LINE_TERMINATOR );

		// Flush, close and delete.
		delete OutputFile;

		SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!FPSCHART:"), *(ChartName) );
	}
#endif // ALLOW_DEBUG_FILES
}

/**
* Dumps the FPS chart information to HTML.
*/
void UEngine::DumpFPSChartToHTML( FLOAT TotalTime, FLOAT DeltaTime, INT NumFrames, UBOOL bOutputToGlobalLog )
{
#if ALLOW_DEBUG_FILES
	// Load the HTML building blocks from the Engine\Stats folder.
	FString FPSChartPreamble;
	FString FPSChartPostamble;
	FString FPSChartRow;
	UBOOL	bAreAllHTMLPartsLoaded = TRUE;

	bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && appLoadFileToString( FPSChartPreamble,	*(appEngineDir() + TEXT("Stats\\FPSChart_Preamble.html")	) );
	bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && appLoadFileToString( FPSChartPostamble,	*(appEngineDir() + TEXT("Stats\\FPSChart_Postamble.html")	) );
	bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && appLoadFileToString( FPSChartRow,		*(appEngineDir() + TEXT("Stats\\FPSChart_Row.html")			) );

#if XBOX
	if (bAreAllHTMLPartsLoaded == FALSE)
	{
		bAreAllHTMLPartsLoaded = TRUE;
		// If we failed, try loading from the \\DEVKIT folder on 360.
		// This is primarily for DVD emulation runs.
		if (FPSChartPreamble.Len() == 0)
		{
			bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && appLoadFileToString( FPSChartPreamble,	*(appProfilingDir() + TEXT("Stats\\FPSChart_Preamble.html")	) );
		}
		if (FPSChartPostamble.Len() == 0)
		{
			bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && appLoadFileToString( FPSChartPostamble,	*(appProfilingDir() + TEXT("Stats\\FPSChart_Postamble.html")	) );
		}
		if (FPSChartRow.Len() == 0)
		{
			bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && appLoadFileToString( FPSChartRow,		*(appProfilingDir() + TEXT("Stats\\FPSChart_Row.html")			) );
		}
	}
#endif

	// Successfully loaded all HTML templates.
	if( bAreAllHTMLPartsLoaded )
	{
		// Keep track of percentage of time at 30+ FPS.
		FLOAT PctTimeAbove30 = 0;

		// Iterate over all buckets, updating row 
		for( INT BucketIndex=0; BucketIndex<ARRAY_COUNT(GFPSChart); BucketIndex++ )
		{
			// Figure out bucket time and frame percentage.
			const FLOAT BucketTimePercentage  = 100.f * GFPSChart[BucketIndex].CummulativeTime / TotalTime;
			const FLOAT BucketFramePercentage = 100.f * GFPSChart[BucketIndex].Count / NumFrames;

			// Figure out bucket range.
			const INT StartFPS	= BucketIndex * 5;
			INT EndFPS		= StartFPS + 5;
			if( BucketIndex + STAT_FPSChartFirstStat == STAT_FPSChart_60_INF )
			{
				EndFPS = 99;
			}

			// Keep track of time spent at 30+ FPS.
			if( StartFPS >= 30 )
			{
				PctTimeAbove30 += BucketTimePercentage;
			}

			const FString SrcToken = FString::Printf( TEXT("TOKEN_%i_%i"), StartFPS, EndFPS );
			const FString DstToken = FString::Printf( TEXT("%5.2f"), BucketTimePercentage );

			// Replace token with actual values.
			FPSChartRow	= FPSChartRow.Replace( *SrcToken, *DstToken );
		}


		// Update hitch data
		{
			INT TotalHitchCount = 0;
			INT TotalGameThreadBoundHitches = 0;
			INT TotalRenderThreadBoundHitches = 0;
			INT TotalGPUBoundHitches = 0;
			for( INT BucketIndex = 0; BucketIndex < ARRAY_COUNT( GHitchChart ); ++BucketIndex )
			{
				FString SrcToken;
				if( BucketIndex == 0 )
				{
					SrcToken = FString::Printf( TEXT("TOKEN_HITCH_%i_PLUS"), GHitchThresholds[ BucketIndex ] );
				}
				else
				{
					SrcToken = FString::Printf( TEXT("TOKEN_HITCH_%i_%i"), GHitchThresholds[ BucketIndex ], GHitchThresholds[ BucketIndex - 1 ] );
				}

				const FString DstToken = FString::Printf( TEXT("%i"), GHitchChart[ BucketIndex ].HitchCount );

				// Replace token with actual values.
				FPSChartRow	= FPSChartRow.Replace( *SrcToken, *DstToken );

				// Count up the total number of hitches
				TotalHitchCount += GHitchChart[ BucketIndex ].HitchCount;
				TotalGameThreadBoundHitches += GHitchChart[ BucketIndex ].GameThreadBoundHitchCount;
				TotalRenderThreadBoundHitches += GHitchChart[ BucketIndex ].RenderThreadBoundHitchCount;
				TotalGPUBoundHitches += GHitchChart[ BucketIndex ].GPUBoundHitchCount;
			}

			// Total hitch count
			FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_HITCH_TOTAL"), *FString::Printf(TEXT("%i"), TotalHitchCount) );
			FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_HITCH_GAME_BOUND_COUNT"), *FString::Printf(TEXT("%i"), TotalGameThreadBoundHitches) );
			FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_HITCH_RENDER_BOUND_COUNT"), *FString::Printf(TEXT("%i"), TotalRenderThreadBoundHitches) );
			FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_HITCH_GPU_BOUND_COUNT"), *FString::Printf(TEXT("%i"), TotalGPUBoundHitches) );
		}


		// Update non- bucket stats.
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_MAPNAME"),		    *FString::Printf(TEXT("%s"), GWorld ? *GWorld->GetMapName() : TEXT("None") ) );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_CHANGELIST"),		*FString::Printf(TEXT("%i"), GetChangeListNumberForPerfTesting() ) );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_DATESTAMP"),         *FString::Printf(TEXT("%s"), *appSystemTimeString() ) );

		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_AVG_FPS"),			*FString::Printf(TEXT("%4.2f"), NumFrames / TotalTime) );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_PCT_ABOVE_30"),		*FString::Printf(TEXT("%4.2f"), PctTimeAbove30) );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_TIME_DISREGARDED"),	*FString::Printf(TEXT("%4.2f"), Max<FLOAT>( 0, DeltaTime - TotalTime ) ) );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_TIME"),				*FString::Printf(TEXT("%4.2f"), DeltaTime) );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_FRAMECOUNT"),		*FString::Printf(TEXT("%i"), NumFrames) );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_AVG_GPUTIME"),		*FString::Printf(TEXT("%4.2f ms"), FLOAT((GTotalGPUTime / NumFrames)*1000.0) ) );

		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_BOUND_GAME_THREAD_PERCENT"),		*FString::Printf(TEXT("%4.2f"), (FLOAT(GNumFramesBound_GameThread)/FLOAT(NumFrames))*100.0f ) );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_BOUND_RENDER_THREAD_PERCENT"),		*FString::Printf(TEXT("%4.2f"), (FLOAT(GNumFramesBound_RenderThread)/FLOAT(NumFrames))*100.0f ) );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_BOUND_GPU_PERCENT"),		*FString::Printf(TEXT("%4.2f"), (FLOAT(GNumFramesBound_GPU)/FLOAT(NumFrames))*100.0f ) );

		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_BOUND_GAME_THREAD_TIME"),		*FString::Printf(TEXT("%4.2f"), (GTotalFramesBoundTime_GameThread/DeltaTime)*100.0f ) );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_BOUND_RENDER_THREAD_TIME"),		*FString::Printf(TEXT("%4.2f"), ((GTotalFramesBoundTime_RenderThread)/DeltaTime)*100.0f ) );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_BOUND_GPU_TIME"),		*FString::Printf(TEXT("%4.2f"), ((GTotalFramesBoundTime_GPU)/DeltaTime)*100.0f ) );


		// Create folder for FPS chart data.
		const FString OutputDir = appProfilingDir() + GSystemStartTime + PATH_SEPARATOR + TEXT("FPSChartStats") + PATH_SEPARATOR;
		GFileManager->MakeDirectory( *OutputDir );

		// Map name we're dumping.
		const FString MapName = GWorld ? GWorld->GetMapName() : TEXT("None");

		// Create FPS chart filename.
		const FString ChartType = GetFPSChartType();

		const FString& FPSChartFilename = OutputDir + CreateFileNameForChart( ChartType, TEXT( ".html" ), bOutputToGlobalLog );
		FString FPSChart;

		// See whether file already exists and load it into string if it does.
		if( appLoadFileToString( FPSChart, *FPSChartFilename, GFileManager, 0, FILEREAD_SaveGame ) )
		{
			// Split string where we want to insert current row.
			const FString HeaderSeparator = TEXT("<UE3></UE3>");
			FString FPSChartBeforeCurrentRow, FPSChartAfterCurrentRow;
			FPSChart.Split( *HeaderSeparator, &FPSChartBeforeCurrentRow, &FPSChartAfterCurrentRow );

			// Assemble FPS chart by inserting current row at the top.
			FPSChart = FPSChartPreamble + FPSChartRow + FPSChartAfterCurrentRow;
		}
		// Assemble from scratch.
		else
		{
			FPSChart = FPSChartPreamble + FPSChartRow + FPSChartPostamble;
		}

		// Save the resulting file back to disk.
		appSaveStringToFile( FPSChart, *FPSChartFilename );

		SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!FPSCHART:"), *(FPSChartFilename) );
	}
	else
	{
		debugf(TEXT("Missing FPS chart template files."));
	}

#endif // ALLOW_DEBUG_FILES
}

/**
 * Dumps the FPS chart information to the passed in archive.
 *
 * @param	bForceDump	Whether to dump even if FPS chart info is not enabled.
 */
void UEngine::DumpFPSChart( UBOOL bForceDump )
{
#if FINAL_RELEASE_DEBUGCONSOLE || !FINAL_RELEASE
	// Iterate over all buckets, gathering total frame count and cumulative time.
	FLOAT TotalTime = 0;
	const FLOAT DeltaTime = appSeconds() - GFPSChartStartTime;
	INT	  NumFrames = 0;
	for( INT BucketIndex=0; BucketIndex<ARRAY_COUNT(GFPSChart); BucketIndex++ )
	{
		NumFrames += GFPSChart[BucketIndex].Count;
		TotalTime += GFPSChart[BucketIndex].CummulativeTime;
	}

	// check to see if we should auto start a sentinel run as there is not one in existence
	if( GSentinelRunID == -1 )
	{
		UBOOL bAutoStartSentinelRun = FALSE;
		ParseUBOOL( appCmdLine(), TEXT("-gASSR="), bAutoStartSentinelRun );

		// if the command line wants it or we are forcing a dump of the fps charts
		if( ( bAutoStartSentinelRun == TRUE ) || ( bForceDump == TRUE ) )
		{
			if( GWorld != NULL )
			{
				AGameInfo* GameInfo = GWorld->GetGameInfo();
				if( GameInfo != NULL && GameInfo->MyAutoTestManager != NULL )
				{
					GameInfo->MyAutoTestManager->BeginSentinelRun( GameInfo->MyAutoTestManager->SentinelTaskDescription, GameInfo->MyAutoTestManager->SentinelTaskParameter, GameInfo->MyAutoTestManager->SentinelTagDesc );
				}
			}
		}
	}

	UBOOL bFPSChartIsActive = bForceDump || GIsCapturingFPSChartInfo;	

	UBOOL bMemoryChartIsActive = FALSE;
	if( ParseParam( appCmdLine(), TEXT("CaptureMemoryChartInfo") ) || ParseParam( appCmdLine(), TEXT("gCMCI") ) )
	{
		bMemoryChartIsActive = TRUE;
	}

	if( ( bFPSChartIsActive == TRUE )
		&& ( TotalTime > 0.f ) 
		&& ( NumFrames > 0 )
		&& ( bMemoryChartIsActive == FALSE ) // we do not want to dump out FPS stats if we have been gathering mem stats as that will probably throw off the stats
		)
	{
		// Log chart info to the log.
		DumpFPSChartToLog( TotalTime, DeltaTime, NumFrames );

		// Only log FPS chart data to file in the game and not PIE.
		if( GIsGame && !GIsEditor )
		{
			DumpFPSChartToStatsLog( TotalTime, DeltaTime, NumFrames );
			DumpFrameTimesToStatsLog( TotalTime, DeltaTime, NumFrames );

			DumpFPSChartToHTML( TotalTime, DeltaTime, NumFrames, FALSE );
			DumpFPSChartToHTML( TotalTime, DeltaTime, NumFrames, TRUE );
		}


	}
#endif // FINAL_RELEASE_DEBUGCONSOLE || !FINAL_RELEASE
}

/**
* Ticks the Memory chart.
*
* @param DeltaSeconds	Time in seconds passed since last tick.
*/
void UEngine::TickMemoryChart( FLOAT DeltaSeconds )
{
#if !FINAL_RELEASE

	static UBOOL bHasCheckedForCommandLineOption = FALSE;
	static UBOOL bMemoryChartIsActive = FALSE;

	if( bHasCheckedForCommandLineOption == FALSE )
	{
		Parse( appCmdLine(), TEXT("-TimeBetweenMemoryChartUpdates="), GTimeBetweenMemoryChartUpdates );

		// check to see if we have a value  else default this to 30.0 seconds
		if( GTimeBetweenMemoryChartUpdates == 0.0f )
		{
			GTimeBetweenMemoryChartUpdates = 30.0f;  
		}

		ParseUBOOL( appCmdLine(),TEXT("-CaptureMemoryChartInfo="), bMemoryChartIsActive );

		bHasCheckedForCommandLineOption = TRUE;
	}


	if( bMemoryChartIsActive == TRUE )
	{
		const DOUBLE TimeSinceLastUpdate = appSeconds() - GLastTimeMemoryChartWasUpdated;

		// test to see if we should update the memory chart this tick
		if( TimeSinceLastUpdate > GTimeBetweenMemoryChartUpdates )
		{
			FMemoryChartEntry NewMemoryEntry = FMemoryChartEntry();
			NewMemoryEntry.UpdateMemoryChartStats();

			GMemoryChart.AddItem( NewMemoryEntry );

			GLastTimeMemoryChartWasUpdated = appSeconds();
		}
	}

#endif //!FINAL_RELEASE
}



void FMemoryChartEntry::UpdateMemoryChartStats()
{
#if STATS && !FINAL_RELEASE

	const FStatGroup* MemGroup = GStatManager.GetGroup( STATGROUP_Memory );

	const FMemoryCounter* Curr = MemGroup->FirstMemoryCounter;

	while( Curr != NULL )
	{
		const EMemoryStats CurrStat = static_cast<EMemoryStats>(Curr->StatId);

		switch( CurrStat )
		{
			// stored in KB (as otherwise it is too hard to read)
#if !PS3
		case STAT_VirtualAllocSize: VirtualMemUsed = Curr->Value/1024.0f; break;
#endif // !PS3
		case STAT_PhysicalAllocSize: PhysicalMemUsed = Curr->Value/1024.0f; break;

		case STAT_AudioMemory: AudioMemUsed = Curr->Value/1024.0f; break;
		case STAT_TextureMemory: TextureMemUsed = Curr->Value/1024.0f; break;
		case STAT_MemoryNovodexTotalAllocationSize: NovodexMemUsed = Curr->Value/1024.0f; break;
		case STAT_VertexLightingAndShadowingMemory: VertexLightingMemUsed = Curr->Value/1024.0f; break;

		case STAT_StaticMeshTotalMemory: StaticMeshTotalMemUsed = Curr->Value/1024.0f; break;
		case STAT_SkeletalMeshVertexMemory: SkeletalMeshVertexMemUsed = Curr->Value/1024.0f; break;
		case STAT_SkeletalMeshIndexMemory: SkeletalMeshIndexMemUsed = Curr->Value/1024.0f; break;
		case STAT_SkeletalMeshMotionBlurSkinningMemory: SkeletalMeshMotionBlurSkinningMemUsed = Curr->Value/1024.0f; break;

		case STAT_VertexShaderMemory: VertexShaderMemUsed = Curr->Value/1024.0f; break;
		case STAT_PixelShaderMemory: PixelShaderMemUsed = Curr->Value/1024.0f; break;

		default: break;
		}

		Curr = (const FMemoryCounter*)Curr->Next;
	}

	STAT( TextureLightmapMemUsed = GMaxTextureLightmapMemory/1024.0f );
	STAT( TextureShadowmapMemUsed = GMaxTextureShadowmapMemory/1024.0f );
	STATWIN( TextureLightmapMemUsedXbox = GMaxTextureLightmapMemoryXbox/1024.0f );
	STATWIN( TextureShadowmapMemUsedXbox = GMaxTextureShadowmapMemoryXbox/1024.0f );
	// Reset the max to the current value.
	STAT( GMaxTextureLightmapMemory = GStatManager.GetStatValueDWORD(STAT_TextureLightmapMemory) );
	STAT( GMaxTextureShadowmapMemory = GStatManager.GetStatValueDWORD(STAT_TextureShadowmapMemory) );
	STATWIN( GMaxTextureLightmapMemoryXbox = GStatManager.GetStatValueDWORD(STAT_XboxTextureLightmapMemory) );
	STATWIN( GMaxTextureShadowmapMemoryXbox = GStatManager.GetStatValueDWORD(STAT_XboxTextureShadowmapMemory) );

	PhysicalTotal = GStatManager.GetAvailableMemory( MCR_Physical )/1024.0f;
	GPUTotal = GStatManager.GetAvailableMemory( MCR_GPU )/1024.0f;


	UBOOL bDoDetailedMemStatGathering = FALSE;
	ParseUBOOL( appCmdLine(),TEXT("-DoDetailedMemStatGathering="), bDoDetailedMemStatGathering );
	if( bDoDetailedMemStatGathering == TRUE )
	{
		// call the platform specific version to fill in the goodies
		appUpdateMemoryChartStats( *this );
	}

#endif //!FINAL_RELEASE
}


/**
* Resets the Memory chart data.
*/
void UEngine::ResetMemoryChart()
{
	GMemoryChart.Empty();
}

/**
 * Dumps the Memory chart information to various places.
 *
 * @param	bForceDump	Whether to dump even if no info has been captured yet (will force an update in that case).
 */
void UEngine::DumpMemoryChart( UBOOL bForceDump /*= FALSE*/ )
{
#if !FINAL_RELEASE
	if( bForceDump && GMemoryChart.Num() == 0 )
	{
		FMemoryChartEntry NewMemoryEntry = FMemoryChartEntry();
		NewMemoryEntry.UpdateMemoryChartStats();
		GMemoryChart.AddItem( NewMemoryEntry );
	}

	if( GMemoryChart.Num() > 0 )
	{
		// Only log FPS chart data to file in the game and not PIE.
		if( GIsGame && !GIsEditor )
		{
			DumpMemoryChartToStatsLog( 0, 0, 0 );
			DumpMemoryChartToHTML( 0, 0, 0, FALSE );
			DumpMemoryChartToHTML( 0, 0, 0, TRUE );
		}
	}
#endif
}

/**
* Dumps the Memory chart information to HTML.
*/
void UEngine::DumpMemoryChartToHTML( FLOAT TotalTime, FLOAT DeltaTime, INT NumFrames, UBOOL bOutputToGlobalLog )
{
#if ALLOW_DEBUG_FILES

	// Load the HTML building blocks from the Engine\Stats folder.
	FString MemoryChartPreamble;
	FString MemoryChartPostamble;
	FString MemoryChartRowTemplate;
	UBOOL	bAreAllHTMLPartsLoaded = TRUE;
	bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && appLoadFileToString( MemoryChartPreamble,	*(appEngineDir() + TEXT("Stats\\MemoryChart_Preamble.html")	) );
	bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && appLoadFileToString( MemoryChartPostamble,	*(appEngineDir() + TEXT("Stats\\MemoryChart_Postamble.html")	) );
	bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && appLoadFileToString( MemoryChartRowTemplate,		*(appEngineDir() + TEXT("Stats\\MemoryChart_Row.html")			) );

#if XBOX
	if (bAreAllHTMLPartsLoaded == FALSE)
	{
		bAreAllHTMLPartsLoaded = TRUE;
		// If we failed, try loading from the \\DEVKIT folder on 360.
		// This is primarily for DVD emulation runs.
		if (MemoryChartPreamble.Len() == 0)
		{
			bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && appLoadFileToString( MemoryChartPreamble,	*(appProfilingDir() + TEXT("Stats\\MemoryChart_Preamble.html")	) );
		}
		if (MemoryChartPostamble.Len() == 0)
		{
			bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && appLoadFileToString( MemoryChartPostamble,	*(appProfilingDir() + TEXT("Stats\\MemoryChart_Postamble.html")	) );
		}
		if (MemoryChartRowTemplate.Len() == 0)
		{
			bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && appLoadFileToString( MemoryChartRowTemplate,		*(appProfilingDir() + TEXT("Stats\\MemoryChart_Row.html")			) );
		}
	}
#endif

	FString MemoryChartRows;

	// Successfully loaded all HTML templates.
	if( bAreAllHTMLPartsLoaded )
	{
		// Iterate over all data
		for( INT MemoryIndex=0; MemoryIndex < GMemoryChart.Num(); ++MemoryIndex )
		{
			// so for the outputting to the global file we only output the first and last entry in the saved data
			if( ( bOutputToGlobalLog == TRUE ) && 
				( ( MemoryIndex != 0 ) && ( MemoryIndex != GMemoryChart.Num() - 1 ) )
				)
			{
				continue;
			}

			// add a new row to the table
			MemoryChartRows += MemoryChartRowTemplate + GMemoryChart(MemoryIndex).ToHTMLString();
			MemoryChartRows = MemoryChartRows.Replace( TEXT("TOKEN_DURATION"), *FString::Printf(TEXT("%5.2f"), (GTimeBetweenMemoryChartUpdates * MemoryIndex)  ) );
			MemoryChartRows += TEXT( "</TR>" );
		}

		// Update non- bucket stats.
		MemoryChartRows = MemoryChartRows.Replace( TEXT("TOKEN_MAPNAME"),		*FString::Printf(TEXT("%s"), GWorld ? *GWorld->GetMapName() : TEXT("None") ) );
		MemoryChartRows = MemoryChartRows.Replace( TEXT("TOKEN_CHANGELIST"), *FString::Printf(TEXT("%i"), GetChangeListNumberForPerfTesting() ) );
		MemoryChartRows = MemoryChartRows.Replace( TEXT("TOKEN_DATESTAMP"), *FString::Printf(TEXT("%s"), *appSystemTimeString() ) );

		// Create folder for Memory chart data.
		const FString OutputDir = appProfilingDir() + GSystemStartTime + PATH_SEPARATOR + TEXT("MemoryChartStats") + PATH_SEPARATOR;
		GFileManager->MakeDirectory( *OutputDir );

		const FString& MemoryChartFilename = OutputDir + CreateFileNameForChart( TEXT( "Mem" ), TEXT( ".html" ), bOutputToGlobalLog );

		FString MemoryChart;
		// See whether file already exists and load it into string if it does.
		if( appLoadFileToString( MemoryChart, *MemoryChartFilename ) )
		{
			// Split string where we want to insert current row.
			const FString HeaderSeparator = TEXT("<UE3></UE3>");
			FString MemoryChartBeforeCurrentRow, MemoryChartAfterCurrentRow;
			MemoryChart.Split( *HeaderSeparator, &MemoryChartBeforeCurrentRow, &MemoryChartAfterCurrentRow );

			// Assemble Memory chart by inserting current row at the top.
			MemoryChart = MemoryChartPreamble + MemoryChartRows + MemoryChartAfterCurrentRow;
		}
		// Assemble from scratch.
		else
		{
			MemoryChart = MemoryChartPreamble + MemoryChartRows + MemoryChartPostamble;
		}

		// Save the resulting file back to disk.
		appSaveStringToFile( MemoryChart, *MemoryChartFilename );
	}
	else
	{
		debugf(TEXT("Missing Memory chart template files."));
	}
#endif
}

/**
* Dumps the Memory chart information to the log.
*/
void UEngine::DumpMemoryChartToStatsLog( FLOAT TotalTime, FLOAT DeltaTime, INT NumFrames )
{
#if ALLOW_DEBUG_FILES
	// Map name we're dumping.
	const FString MapName = GWorld ? GWorld->GetMapName() : TEXT("None");

	// Create folder for Memory chart data.
	const FString OutputDir = appProfilingDir() + GSystemStartTime + PATH_SEPARATOR + TEXT("MemoryChartStats") + PATH_SEPARATOR;
	GFileManager->MakeDirectory( *OutputDir );
	// Create archive for log data.
	FArchive* OutputFile = GFileManager->CreateFileWriter( *(OutputDir + CreateFileNameForChart( TEXT( "Mem" ), TEXT( ".log" ), FALSE ) ), FILEWRITE_Append );

	if( OutputFile )
	{
		if( GMemoryChart.Num() > 0 )
		{
			OutputFile->Logf(TEXT("Dumping Memory chart at %s using build %i built from changelist %i   %i"), *appSystemTimeString(), GEngineVersion, GetChangeListNumberForPerfTesting(), GMemoryChart.Num() );

			OutputFile->Logf( *GMemoryChart(0).GetHeaders() ); 

			// Iterate over all data
			for( INT MemoryIndex=0; MemoryIndex < GMemoryChart.Num(); ++MemoryIndex )
			{
				const FMemoryChartEntry& MemEntry = GMemoryChart(MemoryIndex);

				// Log bucket index, time and frame Percentage.
				OutputFile->Logf( *MemEntry.ToString() );
			}

			OutputFile->Logf( LINE_TERMINATOR LINE_TERMINATOR LINE_TERMINATOR );

			// Flush, close and delete.
			delete OutputFile;
		}
	}
#endif
}


/**
* Dumps the Memory chart information to the special stats log file.
*/
void UEngine::DumpMemoryChartToLog( FLOAT TotalTime, FLOAT DeltaTime, INT NumFrames )
{
	// don't dump out memory data to log during normal running or ever!
}

FString FMemoryChartEntry::GetHeaders() const
{
	return TEXT( "MemoryUsed: ,PhysicalTotal, PhysicalMemUsed, VirtualMemUsed, GPUTotal, GPUMemUsed, AudioMemUsed, TextureMemUsed, NovodexMemUsed, TextureLightmapMemUsed, TextureLightmapMemUsedXbox, TextureShadowmapMemUsed, TextureShadowmapMemUsedXbox, VertexLightingMemUsed, StaticMeshVertexMemUsed, VertexColorResourceMemUsed, VertexColorInstMemUsed, StaticMeshIndexMemUsed, SkeletalMeshVertexMemUsed, SkeletalMeshIndexMemUsed, SkeletalMeshMotionBlurSkinningMemUsed, VertexShaderMemUsed, PixelShaderMemUsed, NumAllocations, AllocationOverhead, AllignmentWaste" );
}


FString FMemoryChartEntry::ToString() const
{
	FString Entry = FString::Printf( TEXT( "MemoryUsed: ,%5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f" ) 
		,PhysicalTotal 
		,PhysicalMemUsed 
		,VirtualMemUsed

		,GPUTotal
		,GPUMemUsed 

		,AudioMemUsed 
		,TextureMemUsed
		,NovodexMemUsed 
		,TextureLightmapMemUsed
		,TextureLightmapMemUsedXbox
		,TextureShadowmapMemUsed
		,TextureShadowmapMemUsedXbox
		,VertexLightingMemUsed
		);

	return Entry + FString::Printf( TEXT( ", %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f, %5.0f" )
		,StaticMeshTotalMemUsed
		,SkeletalMeshVertexMemUsed 
		,SkeletalMeshIndexMemUsed
		,SkeletalMeshMotionBlurSkinningMemUsed

		,VertexShaderMemUsed 
		,PixelShaderMemUsed

		,NumAllocations
		,AllocationOverhead 
		,AllignmentWaste
		);
}

FString FMemoryChartEntry::ToHTMLString() const
{
	FString Retval;

#if ALLOW_DEBUG_FILES
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), PhysicalTotal );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), PhysicalMemUsed );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), VirtualMemUsed );

	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), GPUTotal );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), GPUMemUsed );

	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), AudioMemUsed );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), TextureMemUsed );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), NovodexMemUsed );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), TextureLightmapMemUsed );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), TextureLightmapMemUsedXbox );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), TextureShadowmapMemUsed );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), TextureShadowmapMemUsedXbox );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), VertexLightingMemUsed );

	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), StaticMeshTotalMemUsed );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), SkeletalMeshVertexMemUsed );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), SkeletalMeshIndexMemUsed );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), SkeletalMeshMotionBlurSkinningMemUsed );

	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), VertexShaderMemUsed );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), PixelShaderMemUsed );


	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), NumAllocations );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), AllocationOverhead );
	Retval += FString::Printf( TEXT( "<TD><DIV CLASS=\"value\">%5.0f</DIV></TD> \r\n" ), AllignmentWaste );

#endif
	return Retval;
}

#endif // DO_CHARTING




