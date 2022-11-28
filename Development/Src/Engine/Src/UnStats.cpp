/*=============================================================================
	UnStats.cpp: Performance stats framework.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

//
//	Stat globals.
//

FStatGroup*			GFirstStatGroup = NULL;
FEngineStatGroup		GEngineStats;
FCollisionStatGroup	GCollisionStats;
FPhysicsStatGroup	GPhysicsStats;
FVisibilityStatGroup	GVisibilityStats;
FAudioStatGroup		GAudioStats;
FStreamingStatGroup	GStreamingStats;
FMemoryStatGroup		GMemoryStats;

//
//	FStatGroup::FStatGroup
//

FStatGroup::FStatGroup(const TCHAR* InLabel):
	FirstStat(NULL),
	Enabled(0)
{
	appStrncpy(Label,InLabel,MAX_STAT_LABEL);

	NextGroup = GFirstStatGroup;
	GFirstStatGroup = this;
}

//
//	FStatGroup::~FStatGroup
//

FStatGroup::~FStatGroup()
{
	FStatGroup** Group = &GFirstStatGroup;
	while(*Group != this)
		Group = &(*Group)->NextGroup;
	*Group = NextGroup;
}

//
//	FStatGroup::AdvanceFrame
//

void FStatGroup::AdvanceFrame()
{
	for(FStatBase* Counter = FirstStat;Counter;Counter = Counter->NextStat)
		Counter->AdvanceFrame();
}

//
//	FStatGroup::Render
//

INT FStatGroup::Render(FRenderInterface* RI,INT X,INT Y)
{
	INT	Height = 0;

	RI->DrawShadowedString(
		X,
		Y,
		*FString::Printf(TEXT("%s"),Label),
		GEngine->SmallFont,
		FLinearColor::White
		);
	Height += 12;

	for(FStatBase* Counter = FirstStat;Counter;Counter = Counter->NextStat)
		Height += Counter->Render(RI,X + 4,Y + Height);

	return Height;

}

//
//	FStatCounter::Render
//

INT FStatCounter::Render(FRenderInterface* RI,INT X,INT Y)
{
	DOUBLE	Average = 0.0;

	for(DWORD HistoryIndex = 0;HistoryIndex < STAT_HISTORY_SIZE;HistoryIndex++)
		Average += History[HistoryIndex] / DOUBLE(STAT_HISTORY_SIZE);

	FString Text;
	if( AppendValue )
	{
		Text = FString::Printf(TEXT("%s: %u (%.1f)"),Label,History[(HistoryHead + STAT_HISTORY_SIZE - 1) % STAT_HISTORY_SIZE],Average);
	}
	else
	{
		Text = FString::Printf(TEXT("%u (%.1f) %s"),History[(HistoryHead + STAT_HISTORY_SIZE - 1) % STAT_HISTORY_SIZE],Average,Label);
	}

	RI->DrawShadowedString(
		X,
		Y,
		*Text,
		GEngine->SmallFont,
		FLinearColor(0,1,0)
		);
	return 12;
}

/**
 * Renders the stat using the engine's small font.
 *
 * @param RI	render interface used for rendering
 * @param X		x screen position
 * @param Y		y screen position
 *
 * @return The height of the used font
 */
INT FStatCounterFloat::Render(FRenderInterface* RI,INT X,INT Y)
{
	DOUBLE	Average = 0.0;

	for(DWORD HistoryIndex = 0;HistoryIndex < STAT_HISTORY_SIZE;HistoryIndex++)
		Average += History[HistoryIndex] / DOUBLE(STAT_HISTORY_SIZE);

	FString Text;
	if( AppendValue )
	{
		Text = FString::Printf(TEXT("%s: %.2f (%.1f)"),Label,History[(HistoryHead + STAT_HISTORY_SIZE - 1) % STAT_HISTORY_SIZE],Average);
	}
	else
	{
		Text = FString::Printf(TEXT("%.2f (%.1f) %s"),History[(HistoryHead + STAT_HISTORY_SIZE - 1) % STAT_HISTORY_SIZE],Average,Label);
	}

	RI->DrawShadowedString(
		X,
		Y,
		*Text,
		GEngine->SmallFont,
		FLinearColor(0,1,0)
		);
	return 12;
}

//
//	FCycleCounter::Render
//

INT FCycleCounter::Render(FRenderInterface* RI,INT X,INT Y)
{
	DOUBLE	Average = 0.0;

	for(DWORD HistoryIndex = 0;HistoryIndex < STAT_HISTORY_SIZE;HistoryIndex++)
		Average += History[HistoryIndex] / DOUBLE(STAT_HISTORY_SIZE);

	FString Text;
	if( AppendValue )
	{
		Text = FString::Printf(TEXT("%s: %2.1f ms (%2.1f ms)"),Label,GSecondsPerCycle * 1000.0 * History[(HistoryHead + STAT_HISTORY_SIZE - 1) % STAT_HISTORY_SIZE],GSecondsPerCycle * 1000.0 * Average);
	}
	else
	{
		Text = FString::Printf(TEXT("%2.1f ms (%2.1f ms) %s"),GSecondsPerCycle * 1000.0 * History[(HistoryHead + STAT_HISTORY_SIZE - 1) % STAT_HISTORY_SIZE],GSecondsPerCycle * 1000.0 * Average,Label);
	}

	RI->DrawShadowedString(
		X,
		Y,
		*Text,
		GEngine->SmallFont,
		FLinearColor(0,1,0)
		);
	return 12;
}
