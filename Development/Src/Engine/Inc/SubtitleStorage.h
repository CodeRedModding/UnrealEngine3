/*=============================================================================
	SubtitleStorage.h: Utility for loading subtitle text
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _SUBTITLE_STORAGE_H_
#define _SUBTITLE_STORAGE_H_

/**
* Utility class to hold the sub-title keys for the bink movies
*/
class FSubtitleStorage 
{
public:
	FSubtitleStorage();
	~FSubtitleStorage();

	void Load( FString const& Filename );
	void Add( FString const& Filename );

	void ActivateMovie( FString const& MovieName );
	FString LookupSubtitle( UINT Time );

private:

	struct FSubtitleKeyFrame
	{
		FString		Subtitle;
		UINT		StartTime;
		UINT		StopTime;
	};

	struct FSubtitleMovie 
	{
		FString						MovieName;
		UBOOL						RandomSelect;
		UINT						RandomSelectCycleFrequency;
		TArray<FSubtitleKeyFrame>	KeyFrames;
	};

	TArray<FSubtitleMovie>	Movies;
	INT                     ActiveMovie;
	INT						ActiveTip;
	UINT					NextRandomSelectCycleTime;
	UINT					LastSubtitleTime;
};

#endif



