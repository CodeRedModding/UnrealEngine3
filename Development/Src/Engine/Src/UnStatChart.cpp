/*=============================================================================
	UnStatChart.cpp: Stats Charting Utility
	Copyright 2004 Epic MegaGames, Inc.
	Revision history:
		* Created by James Golding
=============================================================================*/

#include "EnginePrivate.h"
#include "UnStatChart.h"

FStatChart* GStatChart;


// Size of history for each line
#define STATCHART_HISTORY_SIZE 256


FStatChart::FStatChart()
{
	bHideChart = 1;
	bLockScale = 0;

	ChartSize[0] = 250;
	ChartSize[1] = 150;
	
	ChartOrigin[0] = 100;
	ChartOrigin[1] = 200;
	
	XRange=100;
	bHideKey=0;
	ZeroYRatio=0.33f;
	BackgroundAlpha=100;
}

FStatChart::~FStatChart()
{
	
}

void FStatChart::AddLine(const FString& LineName, FColor Color, FLOAT YRangeMin, FLOAT YRangeMax)
{
	FStatChartLine* line = new(Lines) FStatChartLine();

	// Should probably be in constructor...
	line->bHideLine = 0;
	line->DataHistory.AddZeroed(STATCHART_HISTORY_SIZE);
	line->DataPos = 0;
	line->LineColor = Color;
	line->LineName = LineName;
	line->YRange[0] = YRangeMin;
	line->YRange[1] = YRangeMax;
	line->XSpacing = 0.2f;
	line->bAutoScale = 0;

	// Add to name->line map
	LineNameMap.Set(*(line->LineName), Lines.Num()-1);
}

void FStatChart::AddLineAutoRange(const FString& LineName, FColor Color)
{
	// Create a line in the usual way, but with zero range. Then set AutoRange to true.
	AddLine(LineName, Color, 0, 0);
	INT* lineIx = LineNameMap.Find(LineName);
	check(lineIx && *lineIx < Lines.Num());
	FStatChartLine* line = &Lines(*lineIx);
	check(line);

	line->bAutoScale = 1;
}

// Remove all chart lines.
void FStatChart::Reset()
{
	Lines.Empty();
	LineNameMap.Empty();
}

void FStatChart::AddDataPoint(const FString& LineName, FLOAT Data)
{
	// Try and find the existing line for this name.
	INT* lineIx = LineNameMap.Find(LineName);
	FStatChartLine* line;

	// If we didn't find it, add one automatically if desired.
	if(!lineIx)
	{
		// Pick a hue and do HSV->RGB
		//BYTE Hue = appRound(FRange(0, 255).GetRand());
		BYTE Hue = (Lines.Num() * 40)%255;
		FColor randomColor(FGetHSV(Hue, 128, 255));
		randomColor.A = 255;
		
		AddLineAutoRange(LineName, randomColor);

		lineIx = LineNameMap.Find(LineName);
		check(lineIx && *lineIx < Lines.Num());
	}

	if(lineIx)
	{
		line = &Lines(*lineIx);
		check(line);
	}
	else
	{
		return;
	}

	line->DataHistory(line->DataPos) = Data;
	line->DataPos++;
	if(line->DataPos > STATCHART_HISTORY_SIZE-1)
	{
		line->DataPos = 0;
	}

	if(line->bAutoScale)
	{
		line->YRange[0] = Min(line->YRange[0], Data);
		line->YRange[1] = Max(line->YRange[1], Data);
	}
}

static void ReScale(FStatChart* Chart)
{
	for(INT i=0; i<Chart->Lines.Num(); i++)
	{
		FStatChartLine* line = &Chart->Lines(i);

		if(!line->bAutoScale)
		{
			continue;
		}

		line->YRange[0] = 0;
		line->YRange[1] = 0;

		for(INT j=0; j<STATCHART_HISTORY_SIZE; j++)
		{
			line->YRange[0] = Min(line->YRange[0], line->DataHistory(j));
			line->YRange[1] = Max(line->YRange[1], line->DataHistory(j));
		}
	}	
}

UBOOL FStatChart::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if( ParseCommand(&Cmd,TEXT("CHART")) )
	{
		if( ParseCommand(&Cmd,TEXT("SHOW")) )
		{
			bHideChart = !bHideChart;
			return 1;
		}
		else if( ParseCommand(&Cmd,TEXT("KEY")) )
		{
			bHideKey = !bHideKey;
			return 1;
		}
		else if( ParseCommand(&Cmd, TEXT("LOCKSCALE")) )
		{
			bLockScale = !bLockScale;
			return 1;
		}
		else if( ParseCommand(&Cmd, TEXT("RESCALE")) )
		{
			ReScale(this);
			return 1;
		}

		Parse(Cmd, TEXT("XRANGE="), XRange);
		Parse(Cmd, TEXT("XSIZE="), ChartSize[0]);
		Parse(Cmd, TEXT("YSIZE="), ChartSize[1]);
		Parse(Cmd, TEXT("XPOS="), ChartOrigin[0]);
		Parse(Cmd, TEXT("YPOS="), ChartOrigin[1]);
		Parse(Cmd, TEXT("ALPHA="), BackgroundAlpha);
		Parse(Cmd, TEXT("FILTER="), FilterString);

		if( FilterString == FString(TEXT("None")) )
		{
			FilterString = TEXT("");
		}

		return 1;
	}
	else
	{
		return 0;
	}
}

#define BG_BORDER			12
#define BG_KEY_GAP			8
#define BG_KEY_WID			100
#define BG_KEY_LINESPACE	10

void FStatChart::Render(FChildViewport* Viewport, FRenderInterface* RI)
{
	if(bHideChart)
		return;

	if(Lines.Num() == 0)
		return;

	// Draw Chart background
	RI->DrawTile( 
		ChartOrigin[0]-BG_BORDER, ChartOrigin[1]+BG_BORDER, 
		ChartSize[0]+(2*BG_BORDER), -(ChartSize[1]+(2*BG_BORDER)), 
		0.f, 0.f, 1.f, 1.f, 
		FColor(0,0,0,BackgroundAlpha) );

	if(!bHideKey)
	{
		RI->DrawTile(
			ChartOrigin[0]+ChartSize[0]+BG_BORDER+BG_KEY_GAP, ChartOrigin[1]+BG_BORDER, 
			BG_KEY_WID + (2*BG_BORDER), -(ChartSize[1]+(2*BG_BORDER)), 
		0.f, 0.f, 1.f, 1.f, 
		FColor(0,0,0,BackgroundAlpha) );
	}

	FLOAT ZeroY = ChartOrigin[1] - (ChartSize[1] * ZeroYRatio);

	// Chart y axis
	RI->DrawLine2D(ChartOrigin[0], ChartOrigin[1], 
		ChartOrigin[0], ChartOrigin[1]-ChartSize[1], 
		FColor(255,255,255));
	
	// Draw x axis.
	RI->DrawLine2D(ChartOrigin[0], ZeroY, 
		ChartOrigin[0]+ChartSize[0], ZeroY, 
		FColor(255,255,255));	

	UBOOL doFilter = (FilterString.Len() > 0);

	// Find overall max and min y (used if bLockScale == true)
	FLOAT TotalYRange[2] = {0, 0};
	if(bLockScale)
	{
		for(INT i=0; i<Lines.Num(); i++)
		{
			FStatChartLine* line = &Lines(i);

			if(line->bHideLine || (doFilter && line->LineName.Caps().InStr(FilterString.Caps()) == -1))
				continue;

			TotalYRange[0] = Min(TotalYRange[0], line->YRange[0]);
			TotalYRange[1] = Max(TotalYRange[1], line->YRange[1]);
		}
	}

	// Draw data line.
	INT drawCount = 0;
	for(INT i=0; i<Lines.Num(); i++)
	{
		FStatChartLine* line = &Lines(i);

		// If this line is hidden, or its name doesn't match the filter, skip.
		if(line->bHideLine || (doFilter && line->LineName.Caps().InStr(FilterString.Caps()) == -1))
			continue;
	
		// Draw key entry if desired.
		if(!bHideKey)
		{
			TCHAR keyEntry[1024];
			appSprintf(keyEntry, TEXT("%s: %f"), *(line->LineName), line->YRange[1]);

			INT KeyX = ChartOrigin[0] + ChartSize[0] + (2*BG_BORDER) + BG_KEY_GAP;
			INT KeyY = ChartOrigin[1] - ChartSize[1] + (BG_KEY_LINESPACE*(drawCount));
			RI->DrawShadowedString( KeyX, KeyY, keyEntry, GEngine->SmallFont, line->LineColor );
		}

		// Factor to scale all data by to fit onto same Chart as other lines.
		FLOAT lineScale, s1=1000000000.0f, s2=1000000000.0f;
	
		if(bLockScale)
		{
			// Max scale to make minimum fit on Chart
			if(line->YRange[0] < -0.001f)
			{
				s1 = -(ZeroYRatio*ChartSize[1])/TotalYRange[0];
			}

			// Max scale to make maximum fit on Chart
			if(line->YRange[1] > 0.001f)
			{
				s2 = ((1-ZeroYRatio)*ChartSize[1])/TotalYRange[1];			
			}
		}
		else
		{
			// Max scale to make minimum fit on Chart
			if(line->YRange[0] < -0.001f)
			{
				s1 = -(ZeroYRatio*ChartSize[1])/line->YRange[0];
			}

			// Max scale to make maximum fit on Chart
			if(line->YRange[1] > 0.001f)
			{
				s2 = ((1-ZeroYRatio)*ChartSize[1])/line->YRange[1];
			}
		}
		

		lineScale = Min(s1, s2);

		// Start drawing from most recent data (right hand edge)
		INT dataPos = line->DataPos-1;
		if(dataPos == -1)
		{
			dataPos = STATCHART_HISTORY_SIZE-1;
		}

		INT oldDataPos = dataPos;

		FLOAT xPos = ChartOrigin[0] + ChartSize[0];
		FLOAT oldXPos = xPos;

		// Keep working from right to left until we hit y axis, or run out of history.
		while(xPos > ChartOrigin[0] && dataPos != (line->DataPos) )
		{
			FLOAT y0 = line->DataHistory(dataPos) * lineScale;
			FLOAT y1 = line->DataHistory(oldDataPos) * lineScale;

			RI->DrawLine2D(xPos, ZeroY - y0, 
				oldXPos, ZeroY - y1, 
				line->LineColor);

			// Move back in time/left along Chart
			oldXPos = xPos;
			xPos -= (ChartSize[0]/XRange);

			oldDataPos = dataPos;
			dataPos--;
			if(dataPos == -1)
			{
				dataPos = STATCHART_HISTORY_SIZE-1;
			}
		}

		drawCount++;
	}
}
