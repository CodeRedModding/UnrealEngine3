/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNSUBTITLEMANAGER_H__
#define __UNSUBTITLEMANAGER_H__

#define SUBTITLE_SCREEN_DEPTH_FOR_3D 0.1f

/**
 * A collection of subtitles, rendered at a certain priority.
 */
class FActiveSubtitle
{
public:
	FActiveSubtitle( INT InIndex, FLOAT InPriority, UBOOL InbSplit, UBOOL InbSingleLine, const TArray<FSubtitleCue>& InSubtitles )
		:	Index( InIndex )
		,	Priority( InPriority )
		,	bSplit( InbSplit )
		,	bSingleLine( InbSingleLine )
		,	Subtitles( InSubtitles )
	{}

	/** Index into the Subtitles array for the currently active subtitle in this set. */
	INT						Index;
	/** Priority for this set of subtitles, used by FSubtitleManager to determine which subtitle to play. */
	FLOAT					Priority;
	/** TRUE if the subtitles have come in pre split, or after they have been split programmatically. */
	UBOOL					bSplit;
	/** TRUE if the subtitles should be displayed as a sequence of single lines */
	UBOOL					bSingleLine;
	/** A set of subtitles. */
	TArray<FSubtitleCue>	Subtitles;
};

/**
 * Subtitle manager.  Handles prioritization and rendering of subtitles.
 */
class FSubtitleManager
{
public:
	/**
	 * Kills all the subtitles
	 */
	void		KillAllSubtitles( void );

	/**
	 * Kills all the subtitles associated with the subtitle ID
	 */
	void		KillSubtitles( PTRINT SubtitleID );

	/**
	 * Trim the SubtitleRegion to the safe 80% of the canvas 
	 */
	void		TrimRegionToSafeZone( FCanvas * Canvas, FIntRect & SubtitleRegion );

	/**
	 * If any of the active subtitles need to be split into multiple lines, do so now
	 * - caveat - this assumes the width of the subtitle region does not change while the subtitle is active
	 */
	void		SplitLinesToSafeZone( FIntRect & SubtitleRegion );

	/**
	 * Find the highest priority subtitle from the list of currently active ones
	 */
	PTRINT		FindHighestPrioritySubtitle( FLOAT CurrentTime );

	/**
	 * Add an array of subtitles to the active list
	 *
	 * @param  SubTitleID - the controlling id that groups sets of subtitles together
	 * @param  Priority - used to prioritize subtitles; higher values have higher priority.  Subtitles with a priority 0.0f are not displayed.
	 * @param  StartTime - float of seconds that the subtitles start at
	 * @param  SoundDuration - time in seconds after which the subtitles do not display
	 * @param  Subtitles - TArray of lines of subtitle and time offset to play them
	 */
	void		QueueSubtitles( PTRINT SubtitleID, FLOAT Priority, UBOOL bManualWordWrap, UBOOL bSingleLine, FLOAT SoundDuration, TArray<FSubtitleCue>& Subtitles );

	/**
	 * Draws a subtitle at the specified pixel location.
	 */
	void		DisplaySubtitle( FCanvas *Canvas, FActiveSubtitle* Subtitle, FIntRect & Parms, const FLinearColor& Color );

	/**
	 * Display the currently queued subtitles and cleanup after they have finished rendering
	 *
	 * @param  Canvas - where to render the subtitles
	 * @param  CurrentTime - current world time
	 */
	void		DisplaySubtitles( FCanvas *Canvas, FIntRect & SubtitleRegion );

	/** @return TRUE if there are any active subtitles */
	UBOOL		HasSubtitles( void ) 
	{ 
		return ActiveSubtitles.Num() > 0; 
	}

	/**
	 * @return The scale of the font for the currently rendered subtitles
	 */
	FLOAT GetCurrentFontScale( void )
	{
		return FontScale;
	}

	/**
	 * @return The scale of the font for the currently rendered subtitles
	 */
	void SetCurrentFontScale( FLOAT NewScale )
	{
		FontScale = NewScale;
	}

	/**
	 * @return The height of the currently rendered subtitles, 0 if none are rendering
	 */
	FLOAT GetCurrentSubtitlesHeight( void )
	{
		return CurrentSubtitleHeight;
	}

	/**
	 * Returns the singleton subtitle manager instance, creating it if necessary.
	 */
	static FSubtitleManager* GetSubtitleManager( void );

private:
	/**
	 * The set of active, prioritized subtitles.
	 */
	TMap<PTRINT, FActiveSubtitle>	ActiveSubtitles;

	/**
	 * The scale of the subtitle font
	 */
	FLOAT FontScale;

	/**
	 * The current height of the subtitles
	 */
	FLOAT CurrentSubtitleHeight;
};

#endif // __UNSUBTITLEMANAGER_H__
