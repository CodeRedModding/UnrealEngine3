/*=============================================================================
	InterpEditorDraw.cpp: Functions covering drawing the Matinee window
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "InterpEditor.h"
#include "UnLinkedObjDrawUtils.h"

static const INT GroupHeadHeight = 24;
static const INT TrackHeight = 32;
static const INT LabelWidth = 140;
static const INT TrackTitleMargin = 15;
static const INT HeadTitleMargin = 4;

static const INT TimelineHeight = 40;
static const INT NavHeight = 24;
static const INT TotalBarHeight = (TimelineHeight + NavHeight);

static const INT TimeIndHalfWidth = 2;
static const INT RangeTickHeight = 8;

static const FColor HeadSelectedColor(255, 100, 0);

static const FColor DirHeadUnselectedColor(200, 180, 160);
static const FColor HeadUnselectedColor(160, 160, 160);
static const FColor	TrackUnselectedColor(220, 220, 220);

static const FColor NullRegionColor(60, 60, 60);
static const FColor NullRegionBorderColor(255, 255, 255);

static const FColor InterpMarkerColor(255, 80, 80);
static const FColor SectionMarkerColor(80, 255, 80);

static const FColor KeyRangeMarkerColor(255, 183, 111);

static FLOAT GetGridSpacing(INT GridNum)
{
	if(GridNum & 0x01) // Odd numbers
	{
		return appPow( 10.f, 0.5f*((FLOAT)(GridNum-1)) + 1.f );
	}
	else // Even numbers
	{
		return 0.5f * appPow( 10.f, 0.5f*((FLOAT)(GridNum)) + 1.f );
	}
}

/** Draw gridlines and time labels. */
void FInterpEdViewportClient::DrawGrid(FChildViewport* Viewport, FRenderInterface* RI)
{
	INT ViewX = Viewport->GetSizeX();
	INT ViewY = Viewport->GetSizeY();
	UBOOL bHitTesting = RI->IsHitTesting();

	// Calculate desired grid spacing.
	INT MinPixelsPerGrid = 35;
	INT GridNum = 0;
	FLOAT MinGridSpacing = 0.001f;

	FLOAT GridSpacing = MinGridSpacing;
	while( GridSpacing * PixelsPerSec < MinPixelsPerGrid )
	{
		GridSpacing = MinGridSpacing * GetGridSpacing(GridNum);
		GridNum++;
	}

	// Then draw lines across viewed area
	INT LineNum = appFloor(ViewStartTime/GridSpacing);
	while( LineNum*GridSpacing < ViewEndTime)
	{
		FLOAT LineTime = LineNum*GridSpacing;
		INT LinePosX = LabelWidth + (LineTime - ViewStartTime) * PixelsPerSec;

		// Draw grid lines in track view section
		RI->DrawLine2D( LinePosX, 0, LinePosX, ViewY - TotalBarHeight, FColor(125,125,125) );

		// Draw grid lines and labels in timeline section
		if( bHitTesting ) RI->SetHitProxy( new HInterpEdTimelineBkg() );
		RI->DrawLine2D( LinePosX, ViewY - TotalBarHeight, LinePosX, ViewY, FColor(125,125,125) );

		FString Label = FString::Printf( TEXT("%3.2f"), LineTime );
		RI->DrawString( LinePosX + 2, ViewY - NavHeight - 17, *Label, GEngine->SmallFont, FColor(200, 200, 200) );
		if( bHitTesting ) RI->SetHitProxy( NULL );

		LineNum++;
	}
}

/** Draw the timeline control at the bottom of the editor. */
void FInterpEdViewportClient::DrawTimeline(FChildViewport* Viewport, FRenderInterface* RI)
{
	INT ViewX = Viewport->GetSizeX();
	INT ViewY = Viewport->GetSizeY();
	UBOOL bHitTesting = RI->IsHitTesting();

	//////// DRAW TIMELINE
	// Entire length is clickable.

	if( bHitTesting ) RI->SetHitProxy( new HInterpEdTimelineBkg() );
	RI->DrawTile(LabelWidth, ViewY - TotalBarHeight, ViewX - LabelWidth, TimelineHeight, 0.f, 0.f, 0.f, 0.f, FColor(100,100,100) );
	if( bHitTesting ) RI->SetHitProxy( NULL );

	DrawGrid(Viewport, RI);

	// Draw black line separating nav from timeline.
	RI->DrawTile(0, ViewY - TotalBarHeight, ViewX, 1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );

	DrawMarkers(Viewport, RI);

	//////// DRAW NAVIGATOR
	INT ViewStart = LabelWidth + ViewStartTime * NavPixelsPerSecond;
	INT ViewEnd = LabelWidth + ViewEndTime * NavPixelsPerSecond;

	if( bHitTesting ) RI->SetHitProxy( new HInterpEdNavigator() );
	RI->DrawTile(LabelWidth, ViewY - NavHeight, ViewX - LabelWidth, NavHeight, 0.f, 0.f, 0.f, 0.f, FColor(140,140,140) );
	RI->DrawTile(0, ViewY - NavHeight, ViewX, 1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );

	RI->DrawTile( ViewStart, ViewY - NavHeight, ViewEnd - ViewStart, NavHeight, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );
	RI->DrawTile( ViewStart+1, ViewY - NavHeight + 1, ViewEnd - ViewStart - 2, NavHeight - 2, 0.f, 0.f, 1.f, 1.f, FLinearColor::White );
	if( bHitTesting ) RI->SetHitProxy( NULL );


	// Tick indicating current position in global navigator
	RI->DrawTile( LabelWidth + InterpEd->Interp->Position * NavPixelsPerSecond, ViewY - 0.5*NavHeight - 4, 2, 8, 0.f, 0.f, 0.f, 0.f, FColor(80,80,80) );

	//////// DRAW INFO BOX

	RI->DrawTile( 0, ViewY - TotalBarHeight, LabelWidth, TotalBarHeight, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );

	// Draw current time in bottom left.
	INT XL, YL;
	FString PosString = FString::Printf( TEXT("%3.3f / %3.3f"), InterpEd->Interp->Position, InterpEd->IData->InterpLength );
	RI->StringSize( GEngine->SmallFont, XL, YL, *PosString );
	RI->DrawString( HeadTitleMargin, ViewY - YL - HeadTitleMargin, *PosString, GEngine->SmallFont, FLinearColor(0,1,0) );

	// If adjusting current keyframe - draw little record message in bottom-left
	if(InterpEd->bAdjustingKeyframe)
	{
		FLinkedObjDrawUtils::DrawNGon(RI, FIntPoint(HeadTitleMargin + 5, ViewY - 1.5*YL - 2*HeadTitleMargin), FColor(255,0,0), 12, 5);

		check(InterpEd->Opt->SelectedKeys.Num() == 1);
		FInterpEdSelKey& SelKey = InterpEd->Opt->SelectedKeys(0);
		FString AdjustString = FString::Printf( TEXT("KEY %d"), SelKey.KeyIndex );

		RI->DrawString( 2*HeadTitleMargin + 10, ViewY - 2*YL - 2*HeadTitleMargin, *AdjustString, GEngine->SmallFont, FColor(255,0,0));
	}

	///////// DRAW SELECTED KEY RANGE

	if(InterpEd->Opt->SelectedKeys.Num() > 0)
	{
		FLOAT KeyStartTime, KeyEndTime;
		InterpEd->CalcSelectedKeyRange(KeyStartTime, KeyEndTime);

		FLOAT KeyRange = KeyEndTime - KeyStartTime;
		if( KeyRange > KINDA_SMALL_NUMBER && (KeyStartTime < ViewEndTime) && (KeyEndTime > ViewStartTime) )
		{
			// Find screen position of beginning and end of range.
			INT KeyStartX = LabelWidth + (KeyStartTime - ViewStartTime) * PixelsPerSec;
			INT ClipKeyStartX = ::Max(KeyStartX, LabelWidth);

			INT KeyEndX = LabelWidth + (KeyEndTime - ViewStartTime) * PixelsPerSec;
			INT ClipKeyEndX = ::Min(KeyEndX, ViewX);

			// Draw vertical ticks
			if(KeyStartX >= LabelWidth)
			{
				RI->DrawLine2D(KeyStartX, ViewY - TotalBarHeight - RangeTickHeight, KeyStartX, ViewY - TotalBarHeight, KeyRangeMarkerColor);

				// Draw time above tick.
				FString StartString = FString::Printf( TEXT("%3.2fs"), KeyStartTime );
				RI->StringSize( GEngine->SmallFont, XL, YL, *StartString);
				RI->DrawShadowedString( KeyStartX - XL, ViewY - TotalBarHeight - RangeTickHeight - YL - 2, *StartString, GEngine->SmallFont, KeyRangeMarkerColor );
			}

			if(KeyEndX <= ViewX)
			{
				RI->DrawLine2D(KeyEndX, ViewY - TotalBarHeight - RangeTickHeight, KeyEndX, ViewY - TotalBarHeight, KeyRangeMarkerColor);

				// Draw time above tick.
				FString EndString = FString::Printf( TEXT("%3.2fs"), KeyEndTime );
				RI->StringSize( GEngine->SmallFont, XL, YL, *EndString);
				RI->DrawShadowedString( KeyEndX, ViewY - TotalBarHeight - RangeTickHeight - YL - 2, *EndString, GEngine->SmallFont, KeyRangeMarkerColor );
			}

			// Draw line connecting them.
			INT RangeLineY = ViewY - TotalBarHeight - 0.5f*RangeTickHeight;
			RI->DrawLine2D( ClipKeyStartX, RangeLineY, ClipKeyEndX, RangeLineY, KeyRangeMarkerColor);

			// Draw range label above line
			// First find size of range string
			FString RangeString = FString::Printf( TEXT("%3.2fs"), KeyRange );
			RI->StringSize( GEngine->SmallFont, XL, YL, *RangeString);

			// Find X position to start label drawing.
			INT RangeLabelX = ClipKeyStartX + 0.5f*(ClipKeyEndX-ClipKeyStartX) - 0.5f*XL;
			INT RangeLabelY = ViewY - TotalBarHeight - RangeTickHeight - YL;

			RI->DrawShadowedString(RangeLabelX, RangeLabelY, *RangeString, GEngine->SmallFont, KeyRangeMarkerColor);
		}
		else
		{
			UInterpGroup* Group = InterpEd->Opt->SelectedKeys(0).Group;
			UInterpTrack* Track = Group->InterpTracks( InterpEd->Opt->SelectedKeys(0).TrackIndex );
			FLOAT KeyTime = Track->GetKeyframeTime( InterpEd->Opt->SelectedKeys(0).KeyIndex );

			INT KeyX = LabelWidth + (KeyTime - ViewStartTime) * PixelsPerSec;
			if((KeyX >= LabelWidth) && (KeyX <= ViewX))
			{
				RI->DrawLine2D(KeyX, ViewY - TotalBarHeight - RangeTickHeight, KeyX, ViewY - TotalBarHeight, KeyRangeMarkerColor);

				FString KeyString = FString::Printf( TEXT("%3.2fs"), KeyTime );
				RI->StringSize( GEngine->SmallFont, XL, YL, *KeyString);

				INT KeyLabelX = KeyX - 0.5f*XL;
				INT KeyLabelY = ViewY - TotalBarHeight - RangeTickHeight - YL - 3;

				RI->DrawShadowedString(KeyLabelX, KeyLabelY, *KeyString, GEngine->SmallFont, KeyRangeMarkerColor);		
			}
		}
	}
}

/** Draw various markers on the timeline */
void FInterpEdViewportClient::DrawMarkers(FChildViewport* Viewport, FRenderInterface* RI)
{
	UBOOL bHitTesting = RI->IsHitTesting();
	INT ViewX = Viewport->GetSizeX();
	INT ViewY = Viewport->GetSizeY();
	INT ScaleTopY = ViewY - TotalBarHeight + 1;

	// Calculate screen X position that indicates current position in track.
	INT TrackPosX = LabelWidth + (InterpEd->Interp->Position - ViewStartTime) * PixelsPerSec;

	// Draw position indicator and line (if in viewed area)
	if( TrackPosX + TimeIndHalfWidth >= LabelWidth && TrackPosX <= ViewX)
	{
		if( bHitTesting ) RI->SetHitProxy( new HInterpEdTimelineBkg() );
		RI->DrawTile( TrackPosX - TimeIndHalfWidth - 1, ScaleTopY, (2*TimeIndHalfWidth) + 1, TimelineHeight, 0.f, 0.f, 0.f, 0.f, FColor(10,10,10) );
		if( bHitTesting ) RI->SetHitProxy( NULL );
	}

	INT MarkerArrowSize = 8;

	FIntPoint StartA = FIntPoint(0,					ScaleTopY);
	FIntPoint StartB = FIntPoint(0,					ScaleTopY+MarkerArrowSize);
	FIntPoint StartC = FIntPoint(-MarkerArrowSize,	ScaleTopY);

	FIntPoint EndA = FIntPoint(0,					ScaleTopY);
	FIntPoint EndB = FIntPoint(MarkerArrowSize,		ScaleTopY);
	FIntPoint EndC = FIntPoint(0,					ScaleTopY+MarkerArrowSize);


	// Draw sequence start/end markers.
	FIntPoint StartPos( LabelWidth + (0.f - ViewStartTime) * PixelsPerSec, 0 );
	if(bHitTesting) RI->SetHitProxy( new HInterpEdMarker(ISM_SeqStart) );
	RI->DrawTriangle2D( StartA + StartPos, 0.f, 0.f, StartB + StartPos, 0.f, 0.f, StartC + StartPos, 0.f, 0.f, InterpMarkerColor );
	if(bHitTesting) RI->SetHitProxy( NULL );

	FIntPoint EndPos( LabelWidth + (InterpEd->IData->InterpLength - ViewStartTime) * PixelsPerSec, 0 );
	if(bHitTesting) RI->SetHitProxy( new HInterpEdMarker(ISM_SeqEnd) );
	RI->DrawTriangle2D( EndA + EndPos, 0.f, 0.f, EndB + EndPos, 0.f, 0.f, EndC + EndPos, 0.f, 0.f, InterpMarkerColor );
	if(bHitTesting) RI->SetHitProxy( NULL );


	// Draw loop section start/end
	FIntPoint EdStartPos( LabelWidth + (InterpEd->IData->EdSectionStart - ViewStartTime) * PixelsPerSec, 0 );
	if(bHitTesting) RI->SetHitProxy( new HInterpEdMarker(ISM_LoopStart) );
	RI->DrawTriangle2D( StartA + EdStartPos, 0.f, 0.f, StartB + EdStartPos, 0.f, 0.f, StartC + EdStartPos, 0.f, 0.f, SectionMarkerColor );
	if(bHitTesting) RI->SetHitProxy( NULL );


	FIntPoint EdEndPos( LabelWidth + (InterpEd->IData->EdSectionEnd - ViewStartTime) * PixelsPerSec, 0 );
	if(bHitTesting) RI->SetHitProxy( new HInterpEdMarker(ISM_LoopEnd) );
	RI->DrawTriangle2D( EndA + EdEndPos, 0.f, 0.f, EndB + EdEndPos, 0.f, 0.f, EndC + EdEndPos, 0.f, 0.f, SectionMarkerColor );
	if(bHitTesting) RI->SetHitProxy( NULL );


	// Draw little tick indicating path-building time.
	INT PathBuildPosX = LabelWidth + (InterpEd->IData->PathBuildTime - ViewStartTime) * PixelsPerSec;
	if( PathBuildPosX >= LabelWidth && PathBuildPosX <= ViewX)
	{
		RI->DrawTile( PathBuildPosX, ViewY - NavHeight - 10, 1, 11, 0.f, 0.f, 0.f, 0.f, FColor(200, 200, 255) );
	}

}

/** Draw the track editor using the supplied 2D RenderInterface. */
void FInterpEdViewportClient::Draw(FChildViewport* Viewport, FRenderInterface* RI)
{
	RI->SetOrigin2D( 0, 0 );

	// Erase background
	RI->Clear( FColor(112,112,112) );

	INT ViewX = Viewport->GetSizeX();
	INT ViewY = Viewport->GetSizeY();
	UBOOL bHitTesting = RI->IsHitTesting();

	TrackViewSizeX = ViewX - LabelWidth;

	// Calculate ratio between screen pixels and elapsed time.
	PixelsPerSec = FLOAT(ViewX - LabelWidth)/FLOAT(ViewEndTime - ViewStartTime);
	NavPixelsPerSecond = FLOAT(ViewX - LabelWidth)/InterpEd->IData->InterpLength;

	// Draw 'null regions' if viewing past start or end.
	INT StartPosX = LabelWidth + (0.f - ViewStartTime) * PixelsPerSec;
	INT EndPosX = LabelWidth + (InterpEd->IData->InterpLength - ViewStartTime) * PixelsPerSec;
	if(ViewStartTime < 0.f)
	{
		RI->DrawTile(0, 0, StartPosX, ViewY, 0.f, 0.f, 1.f, 1.f, NullRegionColor);
	}

	if(ViewEndTime > InterpEd->IData->InterpLength)
	{
		RI->DrawTile(EndPosX, 0, ViewX-EndPosX, ViewY, 0.f, 0.f, 1.f, 1.f, NullRegionColor);
	}

	// Draw grid and timeline stuff.
	DrawTimeline(Viewport, RI);

	// Draw lines on borders of 'null area'
	if(ViewStartTime < 0.f)
	{
		RI->DrawLine2D(StartPosX, 0, StartPosX, ViewY - TotalBarHeight, NullRegionBorderColor);
	}

	if(ViewEndTime > InterpEd->IData->InterpLength)
	{
		RI->DrawLine2D(EndPosX, 0, EndPosX, ViewY - TotalBarHeight, NullRegionBorderColor);
	}

	// Draw loop region.
	INT EdStartPosX = LabelWidth + (InterpEd->IData->EdSectionStart - ViewStartTime) * PixelsPerSec;
	INT EdEndPosX = LabelWidth + (InterpEd->IData->EdSectionEnd - ViewStartTime) * PixelsPerSec;

	RI->DrawTile(EdStartPosX, 0, EdEndPosX - EdStartPosX, ViewY - TotalBarHeight, 0.f, 0.f, 1.f, 1.f, InterpEd->RegionFillColor);


	// Draw titles block down left.
	if(bHitTesting) RI->SetHitProxy( new HInterpEdTrackBkg() );
	RI->DrawTile( 0, 0, LabelWidth, ViewY - TotalBarHeight, 0.f, 0.f, 0.f, 0.f, FColor(160,160,160) );
	if(bHitTesting) RI->SetHitProxy( NULL );

	INT YOffset = 0;

	for(INT i=0; i<InterpEd->IData->InterpGroups.Num(); i++)
	{
		RI->SetOrigin2D( 0, YOffset );

		// Draw group header
		UInterpGroup* Group = InterpEd->IData->InterpGroups(i);

		FColor GroupUnselectedColor = (Group->IsA(UInterpGroupDirector::StaticClass())) ? DirHeadUnselectedColor : HeadUnselectedColor;
		FColor GroupColor = (Group == InterpEd->ActiveGroup) ? HeadSelectedColor : GroupUnselectedColor;

		if(bHitTesting) RI->SetHitProxy( new HInterpEdGroupTitle(Group) );
		RI->DrawTile( 0, 0, ViewX, GroupHeadHeight, 0.f, 0.f, 1.f, 1.f, GroupColor, InterpEd->BarGradText );
		if(bHitTesting) RI->SetHitProxy( NULL );

		// Draw little collapse-group widget
		INT HalfColArrowSize = 6;
		FIntPoint A,B,C;
	
		if(Group->bCollapsed)
		{
			A = FIntPoint(HeadTitleMargin + HalfColArrowSize,		0.5*GroupHeadHeight - HalfColArrowSize);
			B = FIntPoint(HeadTitleMargin + HalfColArrowSize,		0.5*GroupHeadHeight + HalfColArrowSize);
			C = FIntPoint(HeadTitleMargin + 2*HalfColArrowSize,		0.5*GroupHeadHeight);
		}
		else
		{
			A = FIntPoint(HeadTitleMargin,							0.5*GroupHeadHeight);
			B = FIntPoint(HeadTitleMargin + HalfColArrowSize,		0.5*GroupHeadHeight + HalfColArrowSize);
			C = FIntPoint(HeadTitleMargin + 2*HalfColArrowSize,		0.5*GroupHeadHeight);
		}

		if(bHitTesting) RI->SetHitProxy( new HInterpEdGroupCollapseBtn(Group) );
		RI->DrawTriangle2D( A, 0.f, 0.f, B, 0.f, 0.f, C, 0.f, 0.f, FLinearColor::Black );
		if(bHitTesting) RI->SetHitProxy( NULL );

		// Draw the group name
		INT XL, YL;
		RI->StringSize(GEngine->SmallFont, XL, YL, *Group->GroupName);
		FColor GroupNameColor = Group->IsA(UInterpGroupDirector::StaticClass()) ? FColor(255,255,128) : FColor(255,255,255);
		RI->DrawShadowedString( 2*HeadTitleMargin + 2*HalfColArrowSize, 0.5*GroupHeadHeight - 0.5*YL, *Group->GroupName, GEngine->SmallFont, GroupNameColor );
		
		// Draw button for binding camera to this group.
		FColor ButtonColor = (Group == InterpEd->CamViewGroup) ? FColor(255,200,0) : FColor(112,112,112);
		if(bHitTesting) RI->SetHitProxy( new HInterpEdGroupLockCamBtn(Group) );
		RI->DrawTile( LabelWidth - 22, (0.5*GroupHeadHeight)-4, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );
		RI->DrawTile( LabelWidth - 21, (0.5*GroupHeadHeight)-3, 6, 6, 0.f, 0.f, 1.f, 1.f, ButtonColor );
		if(bHitTesting) RI->SetHitProxy( NULL );

		// Draw little bar showing group colour
		RI->DrawTile( LabelWidth - 6, 0, 6, GroupHeadHeight, 0.f, 0.f, 1.f, 1.f, Group->GroupColor, InterpEd->BarGradText );

		// Draw line under group header
		RI->DrawTile( 0, GroupHeadHeight-1, ViewX, 1, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );
			
		YOffset += GroupHeadHeight;

		if(!Group->bCollapsed)
		{
			// Draw each track in this group.
			for(INT j=0; j<Group->InterpTracks.Num(); j++)
			{
				UInterpTrack* Track = Group->InterpTracks(j);

				RI->SetOrigin2D( LabelWidth, YOffset );

				UBOOL bTrackSelected = false;
				if( Group == InterpEd->ActiveGroup && j == InterpEd->ActiveTrackIndex )
					bTrackSelected = true;

				FColor TrackColor = bTrackSelected ? HeadSelectedColor : TrackUnselectedColor;

				// Call virtual function to draw actual track data.
				Track->DrawTrack( RI, j, ViewX - LabelWidth, TrackHeight-1, ViewStartTime, PixelsPerSec, InterpEd->Opt->SelectedKeys );

				// Track title block on left.
				if(bHitTesting) RI->SetHitProxy( new HInterpEdTrackTitle(Group, j) );
				RI->DrawTile( -LabelWidth, 0, LabelWidth, TrackHeight - 1, 0.f, 0.f, 1.f, 1.f, TrackColor, InterpEd->BarGradText );

				// Truncate from front if name is too long
				FString TrackTitle = FString( *Track->TrackTitle );
				RI->StringSize(GEngine->SmallFont, XL, YL, *TrackTitle);

				// If too long to fit in label - truncate. TODO: Actually truncate by necessary amount!
				if(XL > LabelWidth - TrackTitleMargin - 2)
				{
					TrackTitle = FString::Printf( TEXT("...%s"), *(TrackTitle.Right(13)) );
					RI->StringSize(GEngine->SmallFont, XL, YL, *TrackTitle);
				}

				RI->DrawShadowedString( -LabelWidth + TrackTitleMargin, 0.5*TrackHeight - 0.5*YL, *TrackTitle, GEngine->SmallFont, FLinearColor::White );
				if(bHitTesting) RI->SetHitProxy( NULL );

				UInterpTrackEvent* EventTrack = Cast<UInterpTrackEvent>(Track);
				if(EventTrack)
				{
					FColor ForwardButtonColor = (EventTrack->bFireEventsWhenForwards) ? FColor(100,200,200) : FColor(112,112,112);
					FColor BackwardButtonColor = (EventTrack->bFireEventsWhenBackwards) ? FColor(100,200,200) : FColor(112,112,112);

					if(bHitTesting) RI->SetHitProxy( new HInterpEdEventDirBtn(Group, j, IED_Backward) );
					RI->DrawTile( -32, TrackHeight-11, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );
					RI->DrawTile( -31, TrackHeight-10, 6, 6, 0.f, 0.f, 1.f, 1.f, BackwardButtonColor );
					if(bHitTesting) RI->SetHitProxy( NULL );

					if(bHitTesting) RI->SetHitProxy( new HInterpEdEventDirBtn(Group, j, IED_Forward) );
					RI->DrawTile( -22, TrackHeight-11, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );
					RI->DrawTile( -21, TrackHeight-10, 6, 6, 0.f, 0.f, 1.f, 1.f, ForwardButtonColor );
					if(bHitTesting) RI->SetHitProxy( NULL );
				}

				if( Track->IsA(UInterpTrackFloatBase::StaticClass()) || Track->IsA(UInterpTrackMove::StaticClass()) )
				{
					FColor ButtonColor = InterpEd->IData->CurveEdSetup->ShowingCurve(Track) ? FColor(100,200,100) : FColor(112,112,112);

					// Draw button for pushing properties onto graph view.
					if(bHitTesting) RI->SetHitProxy( new HInterpEdTrackGraphPropBtn(Group, j) );
					RI->DrawTile( -22, TrackHeight-11, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );
					RI->DrawTile( -21, TrackHeight-10, 6, 6, 0.f, 0.f, 1.f, 1.f, ButtonColor );
					if(bHitTesting) RI->SetHitProxy( NULL );
				}

				// Draw line under each track
				RI->DrawTile( -LabelWidth, TrackHeight - 1, ViewX, 1, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );

				YOffset += TrackHeight;
			}
		}
	}

	RI->SetOrigin2D( 0, 0 );

	// Draw line between title block and track view for empty space
	RI->DrawTile( LabelWidth, YOffset-1, 1, ViewY - YOffset, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );

	// Draw vertical position line
	INT TrackPosX = LabelWidth + (InterpEd->Interp->Position - ViewStartTime) * PixelsPerSec;
	if( TrackPosX >= LabelWidth && TrackPosX <= ViewX)
	{
		RI->DrawLine2D( TrackPosX, 0, TrackPosX, ViewY - TotalBarHeight, InterpEd->PosMarkerColor);
	}

	// Draw the box select box
	if(bBoxSelecting)
	{
		INT MinX = Min(BoxStartX, BoxEndX);
		INT MinY = Min(BoxStartY, BoxEndY);
		INT MaxX = Max(BoxStartX, BoxEndX);
		INT MaxY = Max(BoxStartY, BoxEndY);

		RI->DrawLine2D(MinX, MinY, MaxX, MinY, FColor(255,0,0));
		RI->DrawLine2D(MaxX, MinY, MaxX, MaxY, FColor(255,0,0));
		RI->DrawLine2D(MaxX, MaxY, MinX, MaxY, FColor(255,0,0));
		RI->DrawLine2D(MinX, MaxY, MinX, MinY, FColor(255,0,0));
	}
}