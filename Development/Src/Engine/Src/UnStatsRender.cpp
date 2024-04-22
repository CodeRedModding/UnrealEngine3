/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This file contains the rendering functions used in the stats code
 */

#include "EnginePrivate.h"

#if STATS

#define STAT_FONT			GEngine->SmallFont
#define FONT_HEIGHT			12

// The default small font (which the stat rendering code was written around) is
// actually 16 in height... So we will use this to calculate the scale factor
// of the actual font being used to render.
#define IDEAL_FONT_HEIGHT	16

// This is the number of W characters to leave spacing for in the stat name column
#define STAT_COLUMN_WIDTH appSpc(22, 'W')

// This is the number of W characters to leave spacing for in the other stat columns
#define TIME_COLUMN_WIDTH appSpc(8, 'W')


/**
 * Renders the stat at the specified location in grouped form
 *
 * @param RI the render interface to draw with
 * @param X the X location to start drawing at
 * @param Y the Y location to start drawing at
 * @param bShowInclusive whether to draw inclusive values
 * @param bShowExclusive whether to draw exclusive values
 * @param AfterNameColumnOffset how far over to move to draw the first column after the stat name
 * @param InterColumnOffset the space between columns to add
 * @param StatColor the color to draw the stat in
 */
INT FCycleStat::RenderGrouped(class FCanvas* Canvas,INT X,INT Y,UBOOL bShowInclusive,
	UBOOL bShowExclusive,INT AfterNameColumnOffset,INT InterColumnOffset,
	const FLinearColor& StatColor)
{
	FColor Color = StatColor;	

	// Color will be determined by the average value of history
	// If show inclusive and and show exclusive is on, then it will choose color based on inclusive average
	if (bShowInclusive)
	{
		GEngine->GetStatValueColoration(CounterName, GSecondsPerCycle * 1000.f * History.GetAverage(), Color);
	}
	// And the exclusive if asked
	else if (bShowExclusive)
	{
		GEngine->GetStatValueColoration(CounterName, GSecondsPerCycle * 1000.f * ExclusiveHistory.GetAverage(), Color);
	}

	FLOAT FontScalingFactor = STAT_FONT->GetFontScalingFactor();
	INT LocalInterColumnOffset = InterColumnOffset;

	DrawShadowedString(Canvas,X,Y,CounterName,STAT_FONT,Color);

	INT CurrX = X + AfterNameColumnOffset;

	// Now append the call count
	DrawShadowedString(Canvas,CurrX,Y,*FString::Printf(TEXT("%.2f"),NumCallsHistory.GetAverage()), STAT_FONT,Color);
	CurrX += LocalInterColumnOffset;

	// Add the two inclusive columns if asked
	if (bShowInclusive)
	{
		DrawShadowedString(Canvas,CurrX,Y,*FString::Printf(TEXT("%1.2f ms"), GSecondsPerCycle * 1000.f * History.GetAverage()), STAT_FONT,Color);
		CurrX += LocalInterColumnOffset;
		
		DrawShadowedString(Canvas,CurrX,Y,*FString::Printf(TEXT("%1.2f ms"), GSecondsPerCycle * 1000.f * History.GetMax()), STAT_FONT,Color);
		CurrX += LocalInterColumnOffset;
	}

	// And the exclusive if asked
	if (bShowExclusive)
	{
		DrawShadowedString(Canvas,CurrX,Y,*FString::Printf(TEXT("%1.2f ms"), GSecondsPerCycle * 1000.f * ExclusiveHistory.GetAverage()), STAT_FONT,Color);

		CurrX += InterColumnOffset;

		DrawShadowedString(Canvas,CurrX,Y,*FString::Printf(TEXT("%1.2f ms"), GSecondsPerCycle * 1000.f * ExclusiveHistory.GetMax()), STAT_FONT,Color);
	}

	return (INT)((FLOAT)FONT_HEIGHT * FontScalingFactor);
}

/**
 * Renders the stat at the specified location in hierarchical form
 *
 * @param RI the render interface to draw with
 * @param X the X location to start drawing at
 * @param Y the Y location to start drawing at
 * @param StatNum the number to associate with this stat
 * @param bShowInclusive whether to draw inclusive values
 * @param bShowExclusive whether to draw exclusive values
 * @param AfterNameColumnOffset how far over to move to draw the first column after the stat name
 * @param InterColumnOffset the space between columns to add
 * @param StatColor the color to draw the stat in
 */
INT FCycleStat::RenderHierarchical(class FCanvas* Canvas,INT X,INT Y,DWORD StatNum,
	UBOOL bShowInclusive,UBOOL bShowExclusive,INT AfterNameColumnOffset,
	INT InterColumnOffset,const FLinearColor& StatColor)
{
	// Draw the number that is used to navigate the hierarchy
	DrawShadowedString(Canvas,X,Y,*FString::Printf(TEXT("%d"),StatNum), STAT_FONT,StatColor);

	// Use the grouped rendering to render the remaining data
	return RenderGrouped(Canvas,X + (InterColumnOffset / 3),Y,bShowInclusive,bShowExclusive, AfterNameColumnOffset,InterColumnOffset,StatColor);
}

/**
 * Gets the scale for the stats font.
 *
 * @return Font scale adjusted for screen resolution.
 */
FLOAT FStatManager::GetFontScale ()
{
	if (!GIsEditor && (GEngine->GameViewport != NULL) && (GEngine->GameViewport->Viewport != NULL))
	{
		if (appIsNearlyZero(StatFontScale))
		{
			StatFontScale = 1.0f;

			INT StatFontHeight = STAT_FONT->GetMaxCharHeight();
			FLOAT CheckFontHeight = FLOAT(IDEAL_FONT_HEIGHT);

			if (StatFontHeight > CheckFontHeight)
			{
				StatFontScale = CheckFontHeight / StatFontHeight;
			}
		}

		const FViewport* Viewport = GEngine->GetAViewport();

		return (StatFontScale * (Viewport ? Viewport->GetSizeX() : 1280.0f) / 1280.0f);
	}

	return 1.0f;
}

/**
 * Renders the stats data
 *
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 */
void FStatManager::Render(class FCanvas* Canvas,INT X,INT Y)
{
	FLOAT SavedFontScale = GEngine->SmallFont->GetFontScalingFactor();

	GEngine->SmallFont->SetFontScalingFactor(GetFontScale());

	if (StatRenderingMode == SRM_Grouped)
	{
		// If no groups are active, skip rendering
		if (NumRenderedGroups > 0)
		{
			RenderGrouped(Canvas,X,Y);
		}
	}
	else if (StatRenderingMode == SRM_Slow)
	{
		RenderSlow(Canvas,X,Y);
	}
	else
	{
		if (bShowHierarchical == TRUE)
		{
			RenderHierarchical(Canvas,X,Y);
		}
	}

	STAT_FONT->SetFontScalingFactor(SavedFontScale);
}

/**
 * Renders cycle stats above a certain threshold.
 *
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 */
void FStatManager::RenderSlow(class FCanvas* Canvas,INT X,INT Y)
{
	FLOAT FontScalingFactor = STAT_FONT->GetFontScalingFactor();

	// grab texture for rendering text background
	UTexture2D* BackgroundTexture = UCanvas::StaticClass()->GetDefaultObject<UCanvas>()->DefaultTexture;

	// Render columns headings
	Y += RenderGroupedHeadings(Canvas,X,Y);

	// For each group of stats, work through each stat rendering
	for (FStatGroup* Group = FirstGroup; Group != NULL; Group = Group->NextGroup)
	{
		// Skip the default group as it isn't really valid
		if (Group->GroupId != STATGROUP_Default && Group->CanonicalCycleStats.Num() > 0)
		{
			// Figure out whether there are any slow cycle stats in this group and also "tick" cycle stats.
			UBOOL bHasSlowCycleStats = FALSE;

			for (FStatGroup::FCanonicalStatIterator It(Group->CanonicalCycleStats); It; ++It)
			{
				FCycleStat* CycleStat = It.Value();

				// Determine whether we spent more than the threshold time in this cycle stat and update
				// the last time stat was slow variable.
				if( CycleStat->History.GetMax() * GSecondsPerCycle > SlowStatThreshold )
				{
					CycleStat->LastTimeStatWasSlow = GCurrentTime;
					bHasSlowCycleStats = TRUE;
				}

				// Render stats that have been slow in the last few seconds to avoid popping.
				if( GCurrentTime - CycleStat->LastTimeStatWasSlow < MinSlowStatDuration )
				{
					bHasSlowCycleStats = TRUE;
				}
			}

			// Only render group name for groups that display any stats.
			if( bHasSlowCycleStats )
			{
				// Render group name
				DrawShadowedString(Canvas,X,Y,Group->Desc,STAT_FONT,GroupColor);

				Y += FONT_HEIGHT + (FONT_HEIGHT / 2);

				// Render each cycle stat
				UBOOL bDrawBackground = FALSE;

				for (FStatGroup::FCanonicalStatIterator It(Group->CanonicalCycleStats); It; ++It)
				{
					FCycleStat* CycleStat = It.Value();

					if (CycleStat->bShowStat == TRUE && (GCurrentTime - CycleStat->LastTimeStatWasSlow < MinSlowStatDuration))
					{
						if (bDrawBackground && BackgroundTexture != NULL)
						{
							DrawTile(
								Canvas,
								X,
								Y + 1,
								AfterNameColumnOffset + InterColumnOffset * 3,
								(INT)((FLOAT)FONT_HEIGHT * FontScalingFactor),
								0,
								0,
								BackgroundTexture->SizeX,
								BackgroundTexture->SizeY,
								BackgroundColor,
								BackgroundTexture->Resource,
								TRUE);
						}

						bDrawBackground = !bDrawBackground;

						Y += CycleStat->RenderGrouped(Canvas,X,Y,bShowInclusive,FALSE, AfterNameColumnOffset,InterColumnOffset,StatColor);
					}
				}

				// Skip a line to demark counters
				Y += FONT_HEIGHT;
			}
		}
	}
}


/**
 * Renders stats using groups
 *
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 */
void FStatManager::RenderGrouped(class FCanvas* Canvas,INT X,INT Y)
{
	// grab texture for rendering text background
	UTexture2D* BackgroundTexture = UCanvas::StaticClass()->GetDefaultObject<UCanvas>()->DefaultTexture;

	FLOAT FontScalingFactor = STAT_FONT->GetFontScalingFactor();

	// For each group of stats, work through each stat rendering
	for (FStatGroup* Group = FirstGroup; Group != NULL; Group = Group->NextGroup)
	{

		// Skip the default group as it isn't really valid
		if (Group->GroupId != STATGROUP_Default && Group->bShowGroup == TRUE)
		{
			// Render group name
			DrawShadowedString(Canvas,X,Y,Group->Desc,STAT_FONT,GroupColor);
			Y += (INT)(((FLOAT)FONT_HEIGHT + (FONT_HEIGHT / 2)) * FontScalingFactor);

			// Render the cycle counters if requested
			if (bShowCycleCounters && Group->CanonicalCycleStats.Num() > 0)
			{
				UBOOL bAlreadyRenderedGroupedHeadings = FALSE;

				// Render each cycle stat
				UBOOL bDrawBackground = FALSE;

				for (FStatGroup::FCanonicalStatIterator It(Group->CanonicalCycleStats); It; ++It)
				{
					FCycleStat* CycleStat = It.Value();

					if (CycleStat->bShowStat == TRUE)
					{
						// Only render headings if there are visible stats.
						if( !bAlreadyRenderedGroupedHeadings )
						{
							// Render columns headings
							Y += RenderGroupedHeadings(Canvas,X,Y);
							bAlreadyRenderedGroupedHeadings = TRUE;
						}

						if (bDrawBackground && BackgroundTexture != NULL)
						{
							DrawTile(
								Canvas,
								X,
								Y + 1,
								AfterNameColumnOffset + InterColumnOffset * 3,
								(INT)((FLOAT)FONT_HEIGHT * FontScalingFactor),
								0,
								0,
								BackgroundTexture->SizeX,
								BackgroundTexture->SizeY,
								BackgroundColor,
								BackgroundTexture->Resource,
								TRUE);
						}

						bDrawBackground = !bDrawBackground;

						Y += CycleStat->RenderGrouped(Canvas,X,Y,bShowInclusive,FALSE, AfterNameColumnOffset,InterColumnOffset,StatColor);
					}
				}

				// Skip a line to demark counters
				if( bAlreadyRenderedGroupedHeadings )
				{
					Y += (INT)((FLOAT)FONT_HEIGHT * FontScalingFactor);
				}
			}

			// If we are rendering counters, do so now
			if (bShowCounters)
			{		
				UBOOL bHasRenderedStats = FALSE;

				// Don't show the headings if there aren't any
				if( HasRenderableStats<FMemoryCounter>(Group->FirstMemoryCounter) )
				{
					// Show memory headings
					Y += RenderMemoryHeadings(Canvas,X,Y);

					// Render each memory counter
					Y += RenderMemoryCounters(Group->FirstMemoryCounter,Canvas,X,Y, AfterNameColumnOffset,InterColumnOffset,StatColor);

					bHasRenderedStats = TRUE;
				}

				// Don't show the headings if there aren't any
				if (HasRenderableStats<FStatAccumulatorFLOAT>(Group->FirstFloatAccumulator) ||
					HasRenderableStats<FStatAccumulatorDWORD>(Group->FirstDwordAccumulator) ||
					HasRenderableStats<FStatCounterFLOAT>(Group->FirstFloatCounter) ||
					HasRenderableStats<FStatCounterDWORD>(Group->FirstDwordCounter))
				{
					Y += (INT)((FLOAT)FONT_HEIGHT * FontScalingFactor);

					// Show counter headings
					Y += RenderCounterHeadings(Canvas,X,Y);
					
					// Render each float accumulator
					Y += RenderAccumulatorList<FStatAccumulatorFLOAT,FLOAT>(Group->FirstFloatAccumulator, Canvas,X,Y,AfterNameColumnOffset,InterColumnOffset,StatColor);
					
					// Render each dword accumulator
					Y += RenderAccumulatorList<FStatAccumulatorDWORD,DWORD>(Group->FirstDwordAccumulator, Canvas,X,Y,AfterNameColumnOffset,InterColumnOffset,StatColor);
					
					// Render each float counter
					Y += RenderCounterList<FStatCounterFLOAT,FLOAT>(Group->FirstFloatCounter, Canvas,X,Y,AfterNameColumnOffset,InterColumnOffset,StatColor);
					
					// Render each dword counter
					Y += RenderCounterList<FStatCounterDWORD,DWORD>(Group->FirstDwordCounter, Canvas,X,Y,AfterNameColumnOffset,InterColumnOffset,StatColor);

					bHasRenderedStats = TRUE;
				}

				// Skip a line for transition to next group.
				if (bHasRenderedStats)
				{
					Y += (INT)((FLOAT)FONT_HEIGHT * FontScalingFactor);
				}
			}
		}
	}
}

/**
 * Renders stats by showing hierarchy
 *
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 */
void FStatManager::RenderHierarchical(class FCanvas* Canvas,INT X,INT Y)
{
	FLOAT FontScalingFactor = STAT_FONT->GetFontScalingFactor();

	// grab texture for rendering text background
	UTexture2D* BackgroundTexture = UCanvas::StaticClass()->GetDefaultObject<UCanvas>()->DefaultTexture;

	// Render columns headings
	Y += RenderHierarchicalHeadings(Canvas,X,Y);

	// Find the first stat, if not set
	InitCurrentRenderedStat();

	if ( CurrentRenderedStat )
	{
		// Render the parent stat
		Y += CurrentRenderedStat->RenderHierarchical(Canvas,X,Y,0,bShowInclusive,bShowExclusive, AfterNameColumnOffset,InterColumnOffset,StatColor);

		DWORD StatNum = 1;

		// For each child in the current stat, render it's info and a number
		// that can be used to navigate it
		UBOOL bDrawBackground = TRUE;

		for (FCycleStat::FChildStatMapIterator It(CurrentRenderedStat->Children); It; ++It)
		{
			if (bDrawBackground && BackgroundTexture != NULL)
			{
				DrawTile(
					Canvas,
					X,
					Y + 1,
					AfterNameColumnOffset + InterColumnOffset * 3,
					(INT)((FLOAT)FONT_HEIGHT * FontScalingFactor),
					0,
					0,
					BackgroundTexture->SizeX,
					BackgroundTexture->SizeY,
					BackgroundColor,
					BackgroundTexture->Resource,
					TRUE);
			}

			bDrawBackground = !bDrawBackground;

			// Render the stat moving to the next line when done
			Y += It.Value()->RenderHierarchical(Canvas,X,Y,StatNum,bShowInclusive, bShowExclusive,AfterNameColumnOffset,InterColumnOffset,StatColor);

			StatNum++;
		}
	}
}

/**
 * Renders the headings for grouped rendering
 *
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 *
 * @return the height of headings rendered
 */
INT FStatManager::RenderGroupedHeadings(class FCanvas* Canvas,INT X,INT Y)
{
	// The heading looks like:
	// Stat		CallCount	IncAvg	IncMax	ExcAvg	ExcMax
	FLOAT FontScalingFactor = STAT_FONT->GetFontScalingFactor();
	INT LocalInterColumnOffset = InterColumnOffset;

	// get the size of the spaces, since we can't render the width calculation strings
	INT StatColumnSpaceSizeX, StatColumnSpaceSizeY;
	INT TimeColumnSpaceSizeX, TimeColumnSpaceSizeY;

	StringSize(STAT_FONT, StatColumnSpaceSizeX, StatColumnSpaceSizeY, STAT_COLUMN_WIDTH);
	StringSize(STAT_FONT, TimeColumnSpaceSizeX, TimeColumnSpaceSizeY, TIME_COLUMN_WIDTH);

	DrawShadowedString(Canvas,X,Y,TEXT("Stat"),STAT_FONT,HeadingColor);

	// Determine where the first column goes
    AfterNameColumnOffset = StatColumnSpaceSizeX;
	INT CurrX = X + AfterNameColumnOffset;

	DrawShadowedString(Canvas,CurrX,Y,TEXT("CallCount"),STAT_FONT,HeadingColor);

	// Determine the width of subsequent columns
	InterColumnOffset = TimeColumnSpaceSizeX;
	CurrX += LocalInterColumnOffset;

	// Only append inclusive columns if requested
	if (bShowInclusive)
	{
		DrawShadowedString(Canvas,CurrX,Y,TEXT("IncAvg"),STAT_FONT,HeadingColor);
		CurrX += LocalInterColumnOffset;

		DrawShadowedString(Canvas,CurrX,Y,TEXT("IncMax"),STAT_FONT,HeadingColor);
		CurrX += LocalInterColumnOffset;
	}

	return (INT)(((FLOAT)FONT_HEIGHT + (FONT_HEIGHT / 3)) * FontScalingFactor);
}

/**
 * Renders the counter headings for grouped rendering
 *
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 *
 * @return the height of headings rendered
 */
INT FStatManager::RenderCounterHeadings(class FCanvas* Canvas,INT X,INT Y)
{
	// The heading looks like:
	// Stat		Value	Average

	// get the size of the spaces, since we can't render the width calculation strings
// @todo josh: do this one time only unless we want to allow for changing fonts

	FLOAT FontScalingFactor = STAT_FONT->GetFontScalingFactor();

	INT StatColumnSpaceSizeX, StatColumnSpaceSizeY;
	INT TimeColumnSpaceSizeX, TimeColumnSpaceSizeY;

	StringSize(STAT_FONT, StatColumnSpaceSizeX, StatColumnSpaceSizeY, STAT_COLUMN_WIDTH);
	StringSize(STAT_FONT, TimeColumnSpaceSizeX, TimeColumnSpaceSizeY, TIME_COLUMN_WIDTH);

	DrawShadowedString(Canvas,X,Y,TEXT("Stat"),STAT_FONT,HeadingColor);

	// Determine where the first column goes
	INT CurrX = X + AfterNameColumnOffset;
	AfterNameColumnOffset = StatColumnSpaceSizeX;

	// Determine the width of subsequent columns
	InterColumnOffset = TimeColumnSpaceSizeX;

	// Draw the average column label.
	DrawShadowedString(Canvas,CurrX,Y,TEXT("Average"),STAT_FONT,HeadingColor);

	return (INT)((FLOAT)(FONT_HEIGHT + (FONT_HEIGHT / 3)) * FontScalingFactor);
}

/**
 * Renders the memory headings for grouped rendering
 *
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 *
 * @return the height of headings rendered
 */
INT FStatManager::RenderMemoryHeadings(class FCanvas* Canvas,INT X,INT Y)
{
	// The heading looks like:
	// Stat		MemUsed		% PhysMem

	FLOAT FontScalingFactor = STAT_FONT->GetFontScalingFactor();
	INT LocalInterColumnOffset = InterColumnOffset;

	// get the size of the spaces, since we can't render the width calculation strings
	INT StatColumnSpaceSizeX, StatColumnSpaceSizeY;
	INT TimeColumnSpaceSizeX, TimeColumnSpaceSizeY;
	
	StringSize(STAT_FONT, StatColumnSpaceSizeX, StatColumnSpaceSizeY, STAT_COLUMN_WIDTH);
	StringSize(STAT_FONT, TimeColumnSpaceSizeX, TimeColumnSpaceSizeY, TIME_COLUMN_WIDTH);

	DrawShadowedString(Canvas,X,Y,TEXT("Stat"),STAT_FONT,HeadingColor);

	// Determine where the first column goes
	INT CurrX = X + AfterNameColumnOffset;
	AfterNameColumnOffset = StatColumnSpaceSizeX;

	// Determine the width of subsequent columns
	InterColumnOffset = TimeColumnSpaceSizeX;

	DrawShadowedString(Canvas,CurrX,Y,TEXT("MemUsedAvg"),STAT_FONT,HeadingColor);
	CurrX += LocalInterColumnOffset;

	DrawShadowedString(Canvas,CurrX,Y,TEXT("MemUsedMax"),STAT_FONT,HeadingColor);
	CurrX += LocalInterColumnOffset;

	DrawShadowedString(Canvas,CurrX,Y,TEXT("% of Total"),STAT_FONT,HeadingColor);

	return (INT)((FLOAT)(FONT_HEIGHT + (FONT_HEIGHT / 3)) * FontScalingFactor);
}

/**
 * Renders the headings for hierarchical rendering
 *
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 *
 * @return the height of headings rendered
 */
INT FStatManager::RenderHierarchicalHeadings(class FCanvas* Canvas,INT X,INT Y)
{
	// The heading looks like:
	// #	Stat		CallCount	IncAvg	IncMax	ExcAvg	ExcMax
	// 0	Parent
	// 1	<stat name>
	FLOAT FontScalingFactor = STAT_FONT->GetFontScalingFactor();

	INT StatColumnSpaceSizeX, StatColumnSpaceSizeY;
	INT TimeColumnSpaceSizeX, TimeColumnSpaceSizeY;

	StringSize(STAT_FONT, StatColumnSpaceSizeX, StatColumnSpaceSizeY, STAT_COLUMN_WIDTH);
	StringSize(STAT_FONT, TimeColumnSpaceSizeX, TimeColumnSpaceSizeY, TIME_COLUMN_WIDTH);

	DrawShadowedString(Canvas,X,Y,TEXT("#"),STAT_FONT,HeadingColor);
	
	// Determine the width of subsequent columns
	InterColumnOffset = TimeColumnSpaceSizeX;
	INT CurrX = X + InterColumnOffset / 3;

	// Show the stat name column
	DrawShadowedString(Canvas,CurrX,Y,TEXT("Stat"),STAT_FONT,HeadingColor);
	
	// Determine where the first column goes
	AfterNameColumnOffset = StatColumnSpaceSizeX;
	CurrX += AfterNameColumnOffset;

	DrawShadowedString(Canvas,CurrX,Y,TEXT("CallCount"),STAT_FONT,HeadingColor);
	CurrX += InterColumnOffset;
	
	// Only append inclusive columns if requested
	if (bShowInclusive)
	{
		DrawShadowedString(Canvas,CurrX,Y,TEXT("IncAvg"),STAT_FONT,HeadingColor);
		CurrX += InterColumnOffset;

		DrawShadowedString(Canvas,CurrX,Y,TEXT("IncMax"),STAT_FONT,HeadingColor);
		CurrX += InterColumnOffset;
	}
	
	// Only append exclusive columns if requested
	if (bShowExclusive)
	{
		DrawShadowedString(Canvas,CurrX,Y,TEXT("ExcAvg"),STAT_FONT,HeadingColor);
		CurrX += InterColumnOffset;

		DrawShadowedString(Canvas,CurrX,Y,TEXT("ExcMax"),STAT_FONT,HeadingColor);
		CurrX += InterColumnOffset;
	}

	return (INT)(((FLOAT)2 * FONT_HEIGHT) * FontScalingFactor);
}

/**
 * Iterates through a set of memory stats and renders them.
 *
 * @param FirstStat the first stat in the list to render
 * @param RI the render interface to use
 * @param X the xoffset to start drawing at
 * @param Y the y offset to start drawing at
 * @param AfterNameColumnOffset the space to skip between counter name and first stat
 * @param InterColumnOffset the space to skip between columns
 * @param StatColor the color to draw the stat with
 */
INT FStatManager::RenderMemoryCounters(
	FMemoryCounter* FirstStat,
	class FCanvas* Canvas,
	INT X,
	INT Y,
	DWORD AfterNameColumnOffset,
	DWORD InterColumnOffset,
	const FLinearColor& StatColor)
{
	FLOAT FontScalingFactor = STAT_FONT->GetFontScalingFactor();
	INT OriginalY = Y;

	// grab texture for rendering text background
	UTexture2D* BackgroundTexture = UCanvas::StaticClass()->GetDefaultObject<UCanvas>()->DefaultTexture;

	// Render each stat
	UBOOL bDrawBackground = FALSE;

	for (FMemoryCounter* Stat = FirstStat; Stat != NULL; Stat = (FMemoryCounter*)Stat->Next)
	{
		// should we show this stat?
		if (Stat->bShowStat == TRUE && (Stat->Value != 0 || Stat->bDisplayZeroStats))
		{
			if (bDrawBackground && BackgroundTexture != NULL)
			{
				DrawTile(
					Canvas,
					X,
					Y + 1,
					AfterNameColumnOffset + InterColumnOffset * 3,
					(INT)((FLOAT)FONT_HEIGHT * FontScalingFactor),
					0,
					0,
					BackgroundTexture->SizeX,
					BackgroundTexture->SizeY,
					BackgroundColor,
					BackgroundTexture->Resource,
					TRUE);
			}

			bDrawBackground = !bDrawBackground;

			const TCHAR* Units[] =
			{
				TEXT("Bytes"),
				TEXT("KB"),
				TEXT("MB"),
				TEXT("GB")
			};

			INT UnitIndex = 0;
			INT MaxUnitIndex = 0;

			FLOAT MemUsed = (FLOAT)Stat->History.GetAverage();
			FLOAT MaxMemUsed = (FLOAT)Stat->History.GetMax();

			// Determine the units of memory (KB, MB, etc)
			while (MemUsed >= 1024.f)
			{
				MemUsed /= 1024.f;
				UnitIndex++;
			}

			// Determine the units of memory (KB, MB, etc)
			while (MaxMemUsed >= 1024.f)
			{
				MaxMemUsed /= 1024.f;
				MaxUnitIndex++;
			}

			// Get the % of mem
			FLOAT TotalAvail = TotalMemAvail[Stat->Region];
			FLOAT PercentOfMax = TotalAvail ? ((((FLOAT)Stat->Value) / (FLOAT)TotalAvail) * 100.0f) : 0.0f;

			// Draw the label
			DrawShadowedString(Canvas,X,Y,Stat->CounterName,STAT_FONT,StatColor);
			INT CurrX = X + AfterNameColumnOffset;

			// Now append the value of the stat
			DrawShadowedString(Canvas,CurrX,Y,*FString::Printf(TEXT("%.2f %s"),MemUsed,Units[UnitIndex]), STAT_FONT,StatColor);
			CurrX += InterColumnOffset;

			// Now append the max value of the stat
			DrawShadowedString(Canvas,CurrX,Y,*FString::Printf(TEXT("%.2f %s"),MaxMemUsed,Units[MaxUnitIndex]), STAT_FONT,StatColor);
			CurrX += InterColumnOffset;

			// convert total avail into a readable number
			UnitIndex = 0;

			while (TotalAvail >= 1024.f)
			{
				TotalAvail /= 1024.f;
				UnitIndex++;
			}

			DrawShadowedString(Canvas,CurrX,Y,*FString::Printf(TEXT("%.2f%% [%d %s]"),PercentOfMax, appRound(TotalAvail), Units[UnitIndex]), STAT_FONT,StatColor);

			Y += (INT)((FLOAT)FONT_HEIGHT * FontScalingFactor);
		}
	}

	return Y - OriginalY;
}

/**
 * Helper function that hides redundant code. Iterates through a set of
 * stats and has them render.
 *
 * @param FirstStat the first stat in the list to render
 * @param RI the render interface to use
 * @param X the xoffset to start drawing at
 * @param Y the y offset to start drawing at
 * @param AfterNameColumnOffset the space to skip between counter name and first stat
 * @param InterColumnOffset the space to skip between columns
 * @param StatColor the color to draw the stat with
 */
template<class TYPE,typename DTYPE> INT FStatManager::RenderAccumulatorList(
	TYPE* FirstStat,
	class FCanvas* Canvas,
	INT X,
	INT Y,
	DWORD AfterNameColumnOffset,
	DWORD InterColumnOffset,
	const FLinearColor& StatColor)
{
	FLOAT FontScalingFactor = STAT_FONT->GetFontScalingFactor();

	INT OriginalY = Y;

	// Render each counter/accumulator stat
	for (TYPE* Stat = FirstStat; Stat != NULL; Stat = (TYPE*)Stat->Next)
	{
		if (Stat->bShowStat == TRUE)
		{
			// Draw the label
			DrawShadowedString(Canvas,X,Y,Stat->CounterName,STAT_FONT,StatColor);

			INT CurrX = X + AfterNameColumnOffset;

			// Now append the value of the stat
			DrawShadowedString(Canvas,CurrX,Y,*FString::Printf(GetStatFormatString<DTYPE>(),Stat->Value), STAT_FONT,StatColor);

			Y += (INT)((FLOAT)FONT_HEIGHT * FontScalingFactor);
		}
	}
	return Y - OriginalY;
}

/**
 * Helper function that hides redundant code. Iterates through a set of
 * stats and has them render.
 *
 * @param FirstStat the first stat in the list to render
 * @param RI the render interface to use
 * @param X the xoffset to start drawing at
 * @param Y the y offset to start drawing at
 * @param AfterNameColumnOffset the space to skip between counter name and first stat
 * @param InterColumnOffset the space to skip between columns
 * @param StatColor the color to draw the stat with
 */
template<class TYPE,typename DTYPE> INT FStatManager::RenderCounterList(
	TYPE* FirstStat,
	class FCanvas* Canvas,
	INT X,
	INT Y,
	DWORD AfterNameColumnOffset,
	DWORD InterColumnOffset,
	const FLinearColor& StatColor)
{
	FLOAT FontScalingFactor = STAT_FONT->GetFontScalingFactor();

	INT OriginalY = Y;

	// Render each counter/accumulator stat
	for (TYPE* Stat = FirstStat; Stat != NULL; Stat = (TYPE*)Stat->Next)
	{
		if (Stat->bShowStat == TRUE)
		{
			// Draw the label
			DrawShadowedString(Canvas,X,Y,Stat->CounterName,STAT_FONT,StatColor);

			INT CurrX = X + AfterNameColumnOffset;
			
			// Append the average
			DrawShadowedString(Canvas,CurrX,Y,*FString::Printf(TEXT("%.2f"),Stat->History.GetAverage()), STAT_FONT,StatColor);

			Y += (INT)((FLOAT)FONT_HEIGHT * FontScalingFactor);
		}
	}
	return Y - OriginalY;
}

/**
 * Returns whether there are any active stats to be rendered in this linked list.
 *
 * @param FirstStat the first stat in the list to check
 * @return TRUE if there are, FALSE otherwise
 */
template <class TYPE> UBOOL FStatManager::HasRenderableStats( const TYPE* FirstStat ) const
{
	for (const TYPE* Stat=FirstStat; Stat!=NULL; Stat=(TYPE*)Stat->Next)
	{
		if (Stat->bShowStat == TRUE)
		{
			return TRUE;
		}
	}

	return FALSE;
}

#endif
