/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DragTool_Measure.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FDragTool_Measure
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FDragTool_Measure::FDragTool_Measure()
{
	bUseSnapping = TRUE;
	bConvertDelta = FALSE;
}

void FDragTool_Measure::Render3D(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	PDI->DrawLine( Start, End, FColor(255,0,0), SDPG_Foreground );
}

void FDragTool_Measure::Render(const FSceneView* View,FCanvas* Canvas)
{
	const INT dist = appCeil((End - Start).Size());
	if( dist == 0 )
	{
			return;
	}

	const FVector WorldMid = Start + ((End - Start) / 2);
	FVector2D PixelMid;
	if(View->ScreenToPixel(View->WorldToScreen(WorldMid),PixelMid))
	{
		DrawStringCenteredZ( Canvas, appFloor(PixelMid.X), appFloor(PixelMid.Y), 1.0f, *FString::Printf(TEXT("%d"),dist), GEngine->SmallFont, FColor(255,255,255) );
	}
}
