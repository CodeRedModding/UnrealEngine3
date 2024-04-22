/*=============================================================================
	SubtitleStorage.cpp: Utility for loading subtitle text
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "SubtitleStorage.h"

FSubtitleStorage::FSubtitleStorage()
: ActiveMovie(-1),
  ActiveTip(-1)
{
}

FSubtitleStorage::~FSubtitleStorage()
{
}

void FSubtitleStorage::Load( FString const& Filename )
{
	Movies.Empty();
	Add( Filename );
}

void FSubtitleStorage::Add( FString const& Filename )
{
	FFilename Path = Filename;
	FFilename LocPath = Path;

	FString LangExt(appGetLanguageExt());
	if( LangExt != TEXT("INT") )
	{
		FString LocFilename = LocPath.GetLocalizedFilename(*LangExt);		
		if( GFileManager->FileSize(*LocFilename) != -1 )
		{
			LocPath = LocFilename;
		}
	}

	FString LocFileContents;
	if( !appLoadFileToString( LocFileContents, *LocPath ) )
	{
		return;
	}

	FSubtitleMovie Movie;

	Movie.MovieName = Path.GetBaseFilename();
	Movie.RandomSelect = FALSE;
	Movie.RandomSelectCycleFrequency = 0;

	// By default the values are specified in ms.
	UINT TimeScale = 1000; 

	TArray<FString> Subtitles;
	LocFileContents.ReplaceInline( TEXT( "\r" ), TEXT( "\n" ) );
	LocFileContents.ParseIntoArray( &Subtitles, TEXT( "\n" ), TRUE );

	if( Subtitles.Num() > 1 )
	{
		TArray<FString> Parameters;
		INT Count = Subtitles( 0 ).ParseIntoArrayWS( &Parameters );
		if( Count == 1 || Count == 2 )
		{
			// Grab the frame rate and optional random select frequency
			TimeScale = appAtoi( *Parameters( 0 ) );
			if( Count > 1 )
			{
				Movie.RandomSelectCycleFrequency = appAtoi( *Parameters( 1 ) );
			}

			// Grab all the subtitles
			for( INT SubtitleIndex = 1; SubtitleIndex < Subtitles.Num(); SubtitleIndex++ )
			{
				UINT StartTime;
				UINT StopTime;

				INT Count = Subtitles( SubtitleIndex ).ParseIntoArrayWS( &Parameters );
				if( Count == 3 )
				{
					StartTime = appAtoi( *Parameters( 0 ) );
					StopTime = appAtoi( *Parameters( 1 ) );
					
					FSubtitleKeyFrame KeyFrame;
					KeyFrame.StartTime = StartTime * 1000 / TimeScale;
					KeyFrame.StopTime = StopTime * 1000 / TimeScale;
					KeyFrame.Subtitle = Parameters( 2 );

					Movie.KeyFrames.Push( KeyFrame );

					// If start and end times are both zero - enable random selection
					if( StartTime == 0 && StopTime == 0 )
					{
						Movie.RandomSelect = TRUE;
					}
				}
				else
				{
					warnf( NAME_Warning, TEXT( "Unexpected number of parameters on %d line of '%s'. Got %d, expected 3" ), SubtitleIndex, *LocPath, Parameters.Num() );
				}
			}

			Movies.Push( Movie );
		}
		else
		{
			warnf( NAME_Warning, TEXT( "Unexpected number of parameters on first line of '%s'. Got %d, expected 1 or 2" ), *LocPath, Parameters.Num() );
		}
	}
}

void FSubtitleStorage::ActivateMovie( FString const& MovieName )
{
	FString LookupName = FFilename(MovieName).GetBaseFilename();

	for (INT MovieIndex = 0; MovieIndex < Movies.Num(); ++MovieIndex)
	{
		FSubtitleMovie const& Movie = Movies(MovieIndex);
		if (LookupName == Movie.MovieName)
		{
			ActiveMovie = MovieIndex;
			// Initialize random number generator.
			if( !GIsBenchmarking && !ParseParam(appCmdLine(),TEXT("FIXEDSEED")) )
			{
				appRandInit( appCycles() );
			}
			ActiveTip = ( appRand() * Movie.KeyFrames.Num() ) / RAND_MAX;
			NextRandomSelectCycleTime = Movie.RandomSelectCycleFrequency;
			LastSubtitleTime = 0;
			return;
		}
	}

	ActiveTip = -1;
	ActiveMovie = -1;
}

FString FSubtitleStorage::LookupSubtitle( UINT Time )
{
	if ((ActiveMovie != -1) && (ActiveMovie < Movies.Num()))
	{
		FSubtitleMovie& Movie = Movies(ActiveMovie);

		if( Movie.RandomSelect )
		{
			if( ActiveTip > -1 )
			{
				if ( LastSubtitleTime > Time )
				{
					// adjust for the loop case
					// this isn't perfect, because it loses the time between the LastSubtitleTime
					// and the end of the movie, but it's close enough.
					NextRandomSelectCycleTime = (NextRandomSelectCycleTime - LastSubtitleTime);
				}

				if ( (Movie.RandomSelectCycleFrequency > 0) && (Time > NextRandomSelectCycleTime) )
				{
					INT Delta = ( appRand() * (Movie.KeyFrames.Num() - 1) ) / RAND_MAX;
					ActiveTip = (ActiveTip + Delta) % Movie.KeyFrames.Num();
					NextRandomSelectCycleTime += Movie.RandomSelectCycleFrequency;
				}

				LastSubtitleTime = Time;

				if (ActiveTip > -1)
				{
					FSubtitleKeyFrame const& KeyFrame = Movie.KeyFrames( ActiveTip );
					return KeyFrame.Subtitle;
				}
			}
		}
		else
		{
			for (INT FrameIndex = 0; FrameIndex < Movie.KeyFrames.Num(); ++FrameIndex)
			{
				FSubtitleKeyFrame const& KeyFrame = Movie.KeyFrames(FrameIndex);
				if (KeyFrame.StartTime > Time)
				{
					break;
				}

				if (KeyFrame.StartTime <= Time && Time <= KeyFrame.StopTime)
				{
					return KeyFrame.Subtitle;
				}
			}
		}
	}

	return FString();
}



