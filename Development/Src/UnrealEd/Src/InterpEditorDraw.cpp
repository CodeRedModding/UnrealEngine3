/*=============================================================================
	InterpEditorDraw.cpp: Functions covering drawing the Matinee window
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineAnimClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "InterpEditor.h"
#include "UnLinkedObjDrawUtils.h"

static const INT GroupHeadHeight = 24;
static const INT TrackHeight = 24;
static const INT SubTrackHeight = 19;
static const INT HeadTitleMargin = 4;

static const INT TimelineHeight = 40;
static const INT NavHeight = 24;
static const INT TotalBarHeight = (TimelineHeight + NavHeight);

static const INT TimeIndHalfWidth = 2;
static const INT RangeTickHeight = 8;

static const FColor NullRegionColor(60, 60, 60);
static const FColor NullRegionBorderColor(255, 255, 255);

static const FColor InterpMarkerColor(255, 80, 80);
static const FColor SectionMarkerColor(80, 255, 80);

static const FColor KeyRangeMarkerColor(255, 183, 111);


namespace InterpEdGlobals
{
	/** How far to indent the tree labels from the right side of the track editor scroll bar */
	static const INT TreeLabelsMargin = HeadTitleMargin + 16;

	/** Number of pixels that child groups (and their tracks) should be indented */
	static const INT NumPixelsToIndentChildGroups = 14;

	/** Number of pixels that track labels should be indented */
	static const INT TrackTitleMargin = NumPixelsToIndentChildGroups;

	/** How far to offset the 'disable track' check box from the right side of the track editor scroll bar */
	static const INT DisableTrackCheckBoxHorizOffset = 2;

	/** Size of the 'disable track' check box in pixels */
	static const FVector2D DisableTrackIconSize( 12, 12 );

	/** Horizontal offset for vertical separator line (between track check box and title names) */
	static const INT TreeLabelSeparatorOffset = 17;

	/** Color of group label text */
	static const FColor GroupNameTextColor( 255, 255, 255 );

	/** The color of a selected group or track heading. (transparent) */
	static const FColor GroupOrTrackSelectedColor(255, 130, 30, 60);

	/** The color of a selected group or track border */
	static const FColor GroupOrTrackSelectedBorder(255, 130, 30);

	/** This is the color of a group heading label when a sub-track is currently selected. (transparent) */
	static const FColor GroupColorWithTrackSelected(255, 130, 30, 30);

	/** This is the border color of a group heading label when a sub-track is currently selected */
	static const FColor GroupBorderWithTrackSelected(128, 65, 15);

	/** Color of a folder label */
	static const FColor FolderLabelColor( 80, 80, 80 );

	/** Color of a default (uncategorized) group label */
	static const FColor DefaultGroupLabelColor(130, 130, 130);

	/** Color of a director group label */
	static const FColor DirGroupLabelColor(140, 130, 130);

	/** Color of background area on the left side of the track editor (where users can right click to summon a pop up menu) */
	static const FColor TrackLabelAreaBackgroundColor( 60, 60, 60 );
}


/* Draws shadowed text; ensures that the text is pixel-aligned for readability */
INT DrawLabel( FCanvas* Canvas, FLOAT StartX, FLOAT StartY, const TCHAR* Text, const FLinearColor& Color )
{
	return DrawShadowedString( Canvas, appTruncFloat(StartX), appTruncFloat(StartY), Text, GEditor->EditorFont, Color );
}


FLOAT GetGridSpacing(INT GridNum)
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

/** Calculate the best frames' density. */
UINT CalculateBestFrameStep(FLOAT SnapAmount, FLOAT PixelsPerSec, FLOAT MinPixelsPerGrid)
{
	UINT FrameRate = appCeil(1.0f / SnapAmount);
	UINT FrameStep = 1;
	
	// Calculate minimal-symmetric integer divisor.
	UINT MinFrameStep = FrameRate;
	UINT i = 2;	
	while ( i < MinFrameStep )
	{
		if ( MinFrameStep % i == 0 )
		{
			MinFrameStep /= i;
			i = 1;
		}
		i++;
	}	

	// Find the best frame step for certain grid density.
	while (	FrameStep * SnapAmount * PixelsPerSec < MinPixelsPerGrid )
	{
		FrameStep++;
		if ( FrameStep < FrameRate )
		{
			// Must be divisible by MinFrameStep and divisor of FrameRate.
			while ( !(FrameStep % MinFrameStep == 0 && FrameRate % FrameStep == 0) )
			{
				FrameStep++;
			}
		}
		else
		{
			// Must be multiple of FrameRate.
			while ( FrameStep % FrameRate != 0 )
			{
				FrameStep++;
			}
		}
	}

	return FrameStep;
}



/**
 * Locates the director group in our list of groups (if there is one)
 *
 * @param OutDirGroupIndex	The index of the director group in the list (if it was found)
 *
 * @return Returns true if a director group was found
 */
UBOOL WxInterpEd::FindDirectorGroup( INT& OutDirGroupIndex ) const
{
	// @todo: For much better performance, cache the director group index

	// Check to see if we have a director group.  If so, we'll want to draw it on top of the other items!
	UBOOL bHaveDirGroup = FALSE;
	OutDirGroupIndex = 0;
	for( INT i = 0; i < IData->InterpGroups.Num(); ++i )
	{
		const UInterpGroup* Group = IData->InterpGroups( i );

		UBOOL bIsDirGroup = Group->IsA( UInterpGroupDirector::StaticClass() );

		if( bIsDirGroup )
		{
			// Found the director group; we're done!
			bHaveDirGroup = TRUE;
			OutDirGroupIndex = i;
			break;
		}
	}

	return bHaveDirGroup;
}




/**
 * Remaps the specified group index such that the director's group appears as the first element
 *
 * @param DirGroupIndex	The index of the 'director group' in the group list
 * @param ElementIndex	The original index into the group list
 *
 * @return Returns the reordered element index for the specified element index
 */
INT WxInterpEd::RemapGroupIndexForDirGroup( const INT DirGroupIndex, const INT ElementIndex ) const
{
	INT NewElementIndex = ElementIndex;

	if( ElementIndex == 0 )
	{
		// The first element should always be the director group.  We want it displayed on top.
		NewElementIndex = DirGroupIndex;
	}
	else
	{
		// For any elements up to the director group in the list, we'll need to adjust their element index
		// to account for the director group being remapped to the top of the list
		if( ElementIndex <= DirGroupIndex )
		{
			NewElementIndex = ElementIndex - 1;
		}
	}

	return NewElementIndex;
}

/**
 * Calculates the viewport vertical location for the given group.
 *
 * @param	InGroup		The group that owns the track.
 * @param	LabelTopPosition	The viewport vertical location for the group's label top. 
 * @param	LabelBottomPosition	The viewport vertical location for the group's label bottom. This is not the height of the label.
 */
void WxInterpEd::GetGroupLabelPosition( UInterpGroup* InGroup, INT& LabelTopPosition, INT& LabelBottomPosition ) const
{
	INT TopPosition = 0;

	if(InGroup != NULL && InGroup->bVisible)
	{
		// The director group is always visually at the top of the list, so we don't need to do any scrolling
		// if the caller asked us for that group.
		if( !InGroup->IsA( UInterpGroupDirector::StaticClass() ) )
		{
			// Check to see if we have a director group.  If so, we'll want to draw it on top of the other items!
			INT DirGroupIndex = 0;
			UBOOL bHaveDirGroup = FindDirectorGroup( DirGroupIndex ); // Out

			const UInterpGroup* CurParentGroup = NULL;

			// Loop through groups adding height contribution till we find the group that we are scrolling to.
			for(INT Idx = 0; Idx < IData->InterpGroups.Num(); ++Idx )
			{
				INT CurGroupIndex = Idx;

				// If we have a director group then remap the group indices such that the director group is always drawn first
				if( bHaveDirGroup )
				{
					CurGroupIndex = RemapGroupIndexForDirGroup( DirGroupIndex, Idx );
				}

				const UInterpGroup* CurGroup = IData->InterpGroups( CurGroupIndex );

				// If this is the group we are looking for, stop searching
				if( CurGroup == InGroup)
				{
					break;
				}
				else
				{
					// Just skip the director group if we find it, it's never visible with the rest of the groups
					if( !CurGroup->IsA( UInterpGroupDirector::StaticClass() ) )
					{
						UBOOL bIsGroupVisible = CurGroup->bVisible;
						if( CurGroup->bIsParented )
						{
							// If we're parented then we're only visible if our parent group is not collapsed
							check( CurParentGroup != NULL );
							if( CurParentGroup->bCollapsed )
							{
								// Parent group is collapsed, so we should not be rendered
								bIsGroupVisible = FALSE;
							}
						}
						else
						{
							// If this group is not parented, then we clear our current parent
							CurParentGroup = NULL;
						}

						// If the group is visible, add the number of tracks in it as visible as well if it is not collapsed.
						if( bIsGroupVisible )
						{
							TopPosition += GroupHeadHeight;

							if( CurGroup->bCollapsed == FALSE )
							{
								// Account for visible tracks in this group
								for( INT CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
								{
									if( CurGroup->InterpTracks( CurTrackIndex )->bVisible )
									{
										TopPosition += TrackHeight;
									}
								}
							}
						}
					}
				}

				// If the current group is not parented, then it becomes our current parent group
				if( !CurGroup->bIsParented )
				{
					CurParentGroup = CurGroup;
				}
			}
		}
	}

	LabelTopPosition = TopPosition;
	LabelBottomPosition = LabelTopPosition + GroupHeadHeight;
}

/**
 * Calculates the viewport vertical location for the given track.
 *
 * @note	This helper function is useful for determining if a track label is currently viewable.
 *
 * @param	InGroup				The group that owns the track.
 * @param	InTrackIndex		The index of the track in the group's interp track array. 
 * @param	LabelTopPosition	The viewport vertical location for the track's label top. 
 * @param	LabelBottomPosition	The viewport vertical location for the track's label bottom. This is not the height of the label.
 */
void WxInterpEd::GetTrackLabelPositions( UInterpGroup* InGroup, INT InTrackIndex, INT& LabelTopPosition, INT& LabelBottomPosition ) const
{
	INT TopPosition = 0;

	if(InGroup != NULL)
	{
		// First find the position of the group that owns the track. 
		INT GroupLabelBottom = 0;
		GetGroupLabelPosition( InGroup, TopPosition, GroupLabelBottom );

		// Now, add the height of all the tracks that come before this one. 
		if( InGroup->bCollapsed == FALSE )
		{
			// Start from the bottom of the group label instead. 
			TopPosition = GroupLabelBottom;

			// Account for visible tracks in this group
			for( INT TrackIndex = 0; TrackIndex < InGroup->InterpTracks.Num(); ++TrackIndex )
			{
				// If we found the track we were looking for, 
				// we don't need to add anymore height pixels. 
				if( TrackIndex == InTrackIndex )
				{
					break;
				}
				else if( InGroup->InterpTracks( TrackIndex )->bVisible )
				{
					TopPosition += TrackHeight;
				}
			}
		}
	}

	LabelTopPosition = TopPosition;
	LabelBottomPosition = LabelTopPosition + TrackHeight;
}

/**
 * Scrolls the view to the specified group if it is visible, otherwise it scrolls to the top of the screen.
 *
 * @param InGroup	Group to scroll the view to.
 */
void WxInterpEd::ScrollToGroup(class UInterpGroup* InGroup)
{
	INT ScrollPos = 0;
	INT GroupLabelBottom = 0;
	GetGroupLabelPosition(InGroup, ScrollPos, GroupLabelBottom);

	// Set final scroll pos.
	if(TrackWindow != NULL && TrackWindow->ScrollBar_Vert != NULL)
	{
		// Adjust our scroll position by the size of the viewable area.  This prevents us from scrolling
		// the list such that there are elements above the the top of the window that cannot be reached
		// with the scroll bar.  Plus, it just feels better!
		{
			wxRect rc = TrackWindow->GetClientRect();
			const UINT ViewportHeight = rc.GetHeight();
			INT ContentBoxHeight = TrackWindow->InterpEdVC->ComputeGroupListBoxHeight( ViewportHeight );

			ScrollPos -= ContentBoxHeight - GroupHeadHeight;
			if( ScrollPos < 0 )
			{
				ScrollPos = 0;
			}
		}

		TrackWindow->ScrollBar_Vert->SetThumbPosition(ScrollPos);
		TrackWindow->InterpEdVC->ThumbPos_Vert = -ScrollPos;

		if (TrackWindow->InterpEdVC->Viewport)
		{
			// Force it to draw so the view change is seen
			TrackWindow->InterpEdVC->Viewport->Invalidate();
			TrackWindow->InterpEdVC->Viewport->Draw();
		}
	}
}



/**
 * Updates the track window list scroll bar's vertical range to match the height of the window's content
 */
void WxInterpEd::UpdateTrackWindowScrollBars()
{
	// Simply ask our track window to update its scroll bar
	if( TrackWindow != NULL )
	{
		TrackWindow->AdjustScrollBar();
	}
	if( DirectorTrackWindow != NULL )
	{
		DirectorTrackWindow->AdjustScrollBar();
	}
}



/**
 * Dirty the contents of the track window viewports
 */
void WxInterpEd::InvalidateTrackWindowViewports()
{
	if( TrackWindow != NULL )
	{
		TrackWindow->InterpEdVC->Viewport->Invalidate();
	}
	if( DirectorTrackWindow != NULL )
	{
		DirectorTrackWindow->InterpEdVC->Viewport->Invalidate();
	}
}



/**
 * Creates a string with timing/frame information for the specified time value in seconds
 *
 * @param InTime The time value to create a timecode for
 * @param bIncludeMinutes TRUE if the returned string should includes minute information
 *
 * @return The timecode string
 */
FString WxInterpEd::MakeTimecodeString( FLOAT InTime, UBOOL bIncludeMinutes ) const
{
	// SMPTE-style timecode
	const INT MinutesVal = appTrunc( InTime / 60.0f );
	const INT SecondsVal = appTrunc( InTime );
	const FLOAT Frames = InTime / SnapAmount;
	const INT FramesVal = appRound( Frames );
	const FLOAT Subseconds = appFractional( InTime );
	const FLOAT SubsecondFrames = Subseconds / SnapAmount;
	const INT SubsecondFramesVal = appRound( SubsecondFrames );
	const FLOAT SubframeDiff = Frames - ( FLOAT )FramesVal;

	// Are we currently between frames?
	const BOOL bIsBetweenFrames = !appIsNearlyEqual( SubframeDiff, 0.0f );

	const TCHAR SubframeSignChar = ( SubframeDiff >= 0.0f ) ? TCHAR( '+' ) : TCHAR( '-' );

	FString TimecodeString;
	if( bIncludeMinutes )
	{
		TimecodeString =
			FString::Printf(
				TEXT( "%02i:%02i:%02i %s" ),
				MinutesVal,
				SecondsVal,
				SubsecondFramesVal,
				bIsBetweenFrames ? *FString::Printf( TEXT( "%c%.2f" ), SubframeSignChar, Abs( SubframeDiff ) ) : TEXT( "" ) );
	}
	else
	{
		TimecodeString =
			FString::Printf(
				TEXT( "%02i:%02i %s" ),
				SecondsVal,
				SubsecondFramesVal,
				bIsBetweenFrames ? *FString::Printf( TEXT( "%c%.2f" ), SubframeSignChar, Abs( SubframeDiff ) ) : TEXT( "" ) );
	}

	return TimecodeString;
}


/** Draw gridlines and time labels. */
void FInterpEdViewportClient::DrawGrid(FViewport* Viewport, FCanvas* Canvas, UBOOL bDrawTimeline)
{
	const INT ViewX = Viewport->GetSizeX();
	const INT ViewY = Viewport->GetSizeY();

	// Calculate desired grid spacing.
	INT MinPixelsPerGrid = 35;
	FLOAT MinGridSpacing = 0.001f;
	FLOAT GridSpacing = MinGridSpacing;
	UINT FrameStep = 1; // Important frames' density.
	UINT AuxFrameStep = 1; // Auxiliary frames' density.
	
	// Time.
	if (!InterpEd->bSnapToFrames)
	{
		INT GridNum = 0;
		while( GridSpacing * InterpEd->PixelsPerSec < MinPixelsPerGrid )
		{
			GridSpacing = MinGridSpacing * GetGridSpacing(GridNum);
			GridNum++;
		}
	} 	
	else // Frames.
	{
		GridSpacing  = InterpEd->SnapAmount;	
		FrameStep = CalculateBestFrameStep(InterpEd->SnapAmount, InterpEd->PixelsPerSec, MinPixelsPerGrid);
		AuxFrameStep = CalculateBestFrameStep(InterpEd->SnapAmount, InterpEd->PixelsPerSec, 6);
	}

	INT LineNum = appFloor(InterpEd->ViewStartTime/GridSpacing);
	while( LineNum*GridSpacing < InterpEd->ViewEndTime )
	{
		FLOAT LineTime = LineNum*GridSpacing;
		INT LinePosX = InterpEd->LabelWidth + (LineTime - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
		
		FColor LineColor(110, 110, 110);
		
		// Change line color for important frames.
		if ( InterpEd->bSnapToFrames && LineNum % FrameStep == 0 )
		{
			LineColor = FColor(140,140,140);
		}

		if(bDrawTimeline)
		{
			
			// Show time or important frames' numbers (based on FrameStep).
			if ( !InterpEd->bSnapToFrames || Abs(LineNum) % FrameStep == 0 )
			{				
				// Draw grid lines and labels in timeline section.
				if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( new HInterpEdTimelineBkg() );						


				FString Label;
				if (InterpEd->bSnapToFrames)
				{
					// Show frames' numbers.
					Label = FString::Printf( TEXT("%d"), LineNum );
				}
				else
				{
					// Show time.
					Label = FString::Printf( TEXT("%3.2f"), LineTime);
				}
				DrawLine2D(Canvas, FVector2D(LinePosX, ViewY - TotalBarHeight), FVector2D(LinePosX, ViewY), LineColor );
				DrawString(Canvas, LinePosX + 2, ViewY - NavHeight - 17, *Label, GEditor->SmallFont, FColor(175, 175, 175) );

				if( InterpEd->bSnapToFrames )
				{
					// Draw timecode info above the frame number
					const UBOOL bIncludeMinutesInTimecode = FALSE;
					FString TimecodeString = InterpEd->MakeTimecodeString( LineTime, bIncludeMinutesInTimecode );
					DrawString( Canvas, LinePosX + 2, ViewY - NavHeight - 32, *TimecodeString, GEditor->SmallFont, FColor(140, 140, 140), 0.9f, 0.9f );
				}

				if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( NULL );
			}						
		}
		else
		{
			// Draw grid lines in track view section. 
			if (!InterpEd->bSnapToFrames || (Abs(LineNum) % AuxFrameStep == 0))
			{
				INT TrackAreaHeight = ViewY;
				if( bDrawTimeline )
				{
					TrackAreaHeight -= TotalBarHeight;
				}
				DrawLine2D(Canvas, FVector2D(LinePosX, 0), FVector2D(LinePosX, TrackAreaHeight), LineColor );
			}
		}		
		LineNum++;
	}
	
}

/** Draw the timeline control at the bottom of the editor. */
void FInterpEdViewportClient::DrawTimeline(FViewport* Viewport, FCanvas* Canvas)
{
	INT ViewX = Viewport->GetSizeX();
	INT ViewY = Viewport->GetSizeY();

	//////// DRAW TIMELINE
	// Entire length is clickable.

	if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( new HInterpEdTimelineBkg() );
	DrawTile(Canvas,InterpEd->LabelWidth, ViewY - TotalBarHeight, ViewX - InterpEd->LabelWidth, TimelineHeight, 0.f, 0.f, 0.f, 0.f, FColor(80,80,80) );
	if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( NULL );

	DrawGrid(Viewport, Canvas, true);

	// Draw black line separating nav from timeline.
	DrawTile(Canvas,0, ViewY - TotalBarHeight, ViewX, 1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );

	DrawMarkers(Viewport, Canvas);

	//////// DRAW NAVIGATOR
	{
		INT ViewStart = InterpEd->LabelWidth + InterpEd->ViewStartTime * InterpEd->NavPixelsPerSecond;
		INT ViewEnd = InterpEd->LabelWidth + InterpEd->ViewEndTime * InterpEd->NavPixelsPerSecond;

		// Background
		if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( new HInterpEdNavigatorBackground() );
		DrawTile(Canvas,InterpEd->LabelWidth, ViewY - NavHeight, ViewX - InterpEd->LabelWidth, NavHeight, 0.f, 0.f, 0.f, 0.f, FColor(140,140,140) );
		DrawTile(Canvas,0, ViewY - NavHeight, ViewX, 1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );
		if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( NULL );

		// Foreground
		if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( new HInterpEdNavigator() );
		DrawTile(Canvas, ViewStart, ViewY - NavHeight, ViewEnd - ViewStart, NavHeight, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );
		DrawTile(Canvas, ViewStart+1, ViewY - NavHeight + 1, ViewEnd - ViewStart - 2, NavHeight - 2, 0.f, 0.f, 1.f, 1.f, FLinearColor::White );

		// Tick indicating current position in global navigator
		DrawTile(Canvas, InterpEd->LabelWidth + InterpEd->Interp->Position * InterpEd->NavPixelsPerSecond, ViewY - 0.5*NavHeight - 4, 2, 8, 0.f, 0.f, 0.f, 0.f, FColor(80,80,80) );
		if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( NULL );
	}



	//////// DRAW INFO BOX

	DrawTile(Canvas, 0, ViewY - TotalBarHeight, InterpEd->LabelWidth, TotalBarHeight, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );

	// Draw current time in bottom left.
	INT XL, YL;

	FString PosString = FString::Printf( TEXT("%3.3f / %3.3f %s"), InterpEd->Interp->Position, InterpEd->IData->InterpLength, *LocalizeUnrealEd("InterpEd_TimelineInfo_Seconds") );
	StringSize( GEditor->SmallFont, XL, YL, *PosString );
	DrawString(Canvas, HeadTitleMargin, ViewY - YL - HeadTitleMargin, *PosString, GEditor->SmallFont, FLinearColor(0,1,0) );

	FString SnapPosString = "";

	const INT SelIndex = InterpEd->ToolBar->SnapCombo->GetSelection();

	// Determine if time should be drawn including frames or keys
	if(SelIndex == ARRAY_COUNT(InterpEdSnapSizes)+ARRAY_COUNT(InterpEdFPSSnapSizes))
	{
		UInterpTrack* Track = NULL;
		INT SelKeyIndex = 0;

		// keys
		SnapPosString = FString::Printf( TEXT("%3.0f %s"), 0.0, *LocalizeUnrealEd("KeyFrames") );

		// Work with the selected keys in a given track for a given group
		// Show the timeline if only 1 track is selected.
		if( InterpEd->GetSelectedTrackCount() == 1 )
		{
			Track = *InterpEd->GetSelectedTrackIterator();

			if(InterpEd->Opt->SelectedKeys.Num() > 0)
			{
				FInterpEdSelKey& SelKey = InterpEd->Opt->SelectedKeys(0);
				SelKeyIndex = SelKey.KeyIndex + 1;
			}

			if(Track)
			{
				SnapPosString = FString::Printf( TEXT("%3.0f / %3.0f %s"), SelKeyIndex * 1.0, Track->GetNumKeyframes() * 1.0, *LocalizeUnrealEd("KeyFrames") );
			}
		}

		StringSize( GEditor->SmallFont, XL, YL, *SnapPosString );
	}
	else if(SelIndex < ARRAY_COUNT(InterpEdFPSSnapSizes)+ARRAY_COUNT(InterpEdSnapSizes) && SelIndex >= ARRAY_COUNT(InterpEdSnapSizes))
	{
		// frames

		// Timecode string
		SnapPosString = InterpEd->MakeTimecodeString( InterpEd->Interp->Position );
		const INT StringXPos = HeadTitleMargin + 116;
		StringSize( GEditor->SmallFont, XL, YL, *SnapPosString );
		DrawString( Canvas, StringXPos, ViewY - YL - INT(2.5 * YL) - HeadTitleMargin, *SnapPosString, GEditor->SmallFont, FLinearColor( 1,1,0 ) );

		// Frame counts
		SnapPosString = FString::Printf( TEXT("%3.1f / %3.1f %s"), (1.0 / InterpEd->SnapAmount) * InterpEd->Interp->Position, (1.0 / InterpEd->SnapAmount) * InterpEd->IData->InterpLength, *LocalizeUnrealEd("InterpEd_TimelineInfo_Frames") );
		StringSize( GEditor->SmallFont, XL, YL, *SnapPosString );
	}
	else if(SelIndex < ARRAY_COUNT(InterpEdSnapSizes))
	{
		// seconds
		SnapPosString = "";
	}
	else
	{
		// nothing
		SnapPosString = "";
	}
	DrawString(Canvas, HeadTitleMargin, ViewY - YL - INT(2.5 * YL) - HeadTitleMargin, *SnapPosString, GEditor->SmallFont, FLinearColor(0,1,0) );

	// If adjusting current keyframe - draw little record message in bottom-left
	if(InterpEd->Opt->bAdjustingKeyframe)
	{
		check(InterpEd->Opt->SelectedKeys.Num() == 1);

		FInterpEdSelKey& rSelKey = InterpEd->Opt->SelectedKeys(0);
		FString KeyTitle = FString::Printf( TEXT("%s%d"), ( rSelKey.Track ? *rSelKey.Track->TrackTitle : TEXT( "?" ) ), rSelKey.KeyIndex );
		FString AdjustString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("Key_F"), *KeyTitle ) );

		FLinkedObjDrawUtils::DrawNGon(Canvas, FIntPoint(HeadTitleMargin + 5, ViewY - 1.5*YL - 2*HeadTitleMargin), FColor(255,0,0), 12, 5);
		DrawString(Canvas, 2*HeadTitleMargin + 10, ViewY - 2*YL - 2*HeadTitleMargin, *AdjustString, GEditor->SmallFont, FColor(255,0,0));
	}
	else if(InterpEd->Opt->bAdjustingGroupKeyframes)
	{
		check(InterpEd->Opt->SelectedKeys.Num() > 1);

		// Make a list of all the unique subgroups within the selection, cache for fast lookup
		TArray<FString> UniqueSubGroupNames;
		TArray<FString> KeySubGroupNames;
		TArray<FString> KeyTitles;
		for( INT iSelectedKey = 0; iSelectedKey < InterpEd->Opt->SelectedKeys.Num(); iSelectedKey++ )
		{
			FInterpEdSelKey& rSelKey = InterpEd->Opt->SelectedKeys(iSelectedKey);
			FString SubGroupName = rSelKey.GetOwningTrackSubGroupName();
			UniqueSubGroupNames.AddUniqueItem( SubGroupName );
			KeySubGroupNames.AddItem( SubGroupName );
			FString KeyTitle = FString::Printf( TEXT("%s%d"), ( rSelKey.Track ? *rSelKey.Track->TrackTitle : TEXT( "?" ) ), rSelKey.KeyIndex );
			KeyTitles.AddItem( KeyTitle );
		}

		// Order the string in the format subgroup[tracktrack] subgroup[track]
		FString AdjustString( "Keys_F " );
		for( INT iUSubGroupName = 0; iUSubGroupName < UniqueSubGroupNames.Num(); iUSubGroupName++ )
		{
			FString& rUniqueSubGroupName = UniqueSubGroupNames(iUSubGroupName);
			AdjustString += rUniqueSubGroupName;
			AdjustString += TEXT( "[" );
			for( INT iKSubGroupName = 0; iKSubGroupName < KeySubGroupNames.Num(); iKSubGroupName++ )
			{
				FString& KeySubGroupName = KeySubGroupNames( iKSubGroupName );
				if ( rUniqueSubGroupName == KeySubGroupName )
				{
					AdjustString += KeyTitles( iKSubGroupName );
				}
			}
			AdjustString += TEXT( "] " );
		}

		FLinkedObjDrawUtils::DrawNGon(Canvas, FIntPoint(HeadTitleMargin + 5, ViewY - 1.5*YL - 2*HeadTitleMargin), FColor(255,0,0), 12, 5);
		DrawString(Canvas, 2*HeadTitleMargin + 10, ViewY - 2*YL - 2*HeadTitleMargin, *AdjustString, GEditor->SmallFont, FColor(255,0,0));
	}

	///////// DRAW SELECTED KEY RANGE

	if(InterpEd->Opt->SelectedKeys.Num() > 0)
	{
		FLOAT KeyStartTime, KeyEndTime;
		InterpEd->CalcSelectedKeyRange(KeyStartTime, KeyEndTime);

		FLOAT KeyRange = KeyEndTime - KeyStartTime;
		if( KeyRange > KINDA_SMALL_NUMBER && (KeyStartTime < InterpEd->ViewEndTime) && (KeyEndTime > InterpEd->ViewStartTime) )
		{
			// Find screen position of beginning and end of range.
			INT KeyStartX = InterpEd->LabelWidth + (KeyStartTime - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
			INT ClipKeyStartX = ::Max(KeyStartX, InterpEd->LabelWidth);

			INT KeyEndX = InterpEd->LabelWidth + (KeyEndTime - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
			INT ClipKeyEndX = ::Min(KeyEndX, ViewX);

			// Draw vertical ticks
			if(KeyStartX >= InterpEd->LabelWidth)
			{
				DrawLine2D(Canvas,FVector2D(KeyStartX, ViewY - TotalBarHeight - RangeTickHeight), FVector2D(KeyStartX, ViewY - TotalBarHeight), KeyRangeMarkerColor);

				// Draw time above tick.
				FString StartString = FString::Printf( TEXT("%3.2fs"), KeyStartTime );
				if ( InterpEd->bSnapToFrames )
				{
					StartString += FString::Printf( TEXT(" / %df"), appRound(KeyStartTime / InterpEd->SnapAmount));
				}
				StringSize( GEditor->SmallFont, XL, YL, *StartString);
				DrawLabel(Canvas, KeyStartX - XL, ViewY - TotalBarHeight - RangeTickHeight - YL - 2, *StartString, KeyRangeMarkerColor);
			}

			if(KeyEndX <= ViewX)
			{
				DrawLine2D(Canvas,FVector2D(KeyEndX, ViewY - TotalBarHeight - RangeTickHeight), FVector2D(KeyEndX, ViewY - TotalBarHeight), KeyRangeMarkerColor);

				// Draw time above tick.
				FString EndString = FString::Printf( TEXT("%3.2fs"), KeyEndTime );
				if ( InterpEd->bSnapToFrames )
				{
					EndString += FString::Printf( TEXT(" / %df"), appRound(KeyEndTime / InterpEd->SnapAmount));
				}

				StringSize( GEditor->SmallFont, XL, YL, *EndString);
				DrawLabel(Canvas, KeyEndX, ViewY - TotalBarHeight - RangeTickHeight - YL - 2, *EndString, KeyRangeMarkerColor);
			}

			// Draw line connecting them.
			INT RangeLineY = ViewY - TotalBarHeight - 0.5f*RangeTickHeight;
			DrawLine2D(Canvas, FVector2D(ClipKeyStartX, RangeLineY), FVector2D(ClipKeyEndX, RangeLineY), KeyRangeMarkerColor);

			// Draw range label above line
			// First find size of range string
			FString RangeString = FString::Printf( TEXT("%3.2fs"), KeyRange );
			if ( InterpEd->bSnapToFrames )
			{
				RangeString += FString::Printf( TEXT(" / %df"), appRound(KeyRange / InterpEd->SnapAmount));
			}

			StringSize( GEditor->SmallFont, XL, YL, *RangeString);

			// Find X position to start label drawing.
			INT RangeLabelX = ClipKeyStartX + 0.5f*(ClipKeyEndX-ClipKeyStartX) - 0.5f*XL;
			INT RangeLabelY = ViewY - TotalBarHeight - RangeTickHeight - YL;

			DrawLabel(Canvas,RangeLabelX, RangeLabelY, *RangeString, KeyRangeMarkerColor);
		}
		else
		{
			UInterpGroup* Group = InterpEd->Opt->SelectedKeys(0).Group;
			UInterpTrack* Track = InterpEd->Opt->SelectedKeys(0).Track;
			FLOAT KeyTime = Track->GetKeyframeTime( InterpEd->Opt->SelectedKeys(0).KeyIndex );

			INT KeyX = InterpEd->LabelWidth + (KeyTime - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
			if((KeyX >= InterpEd->LabelWidth) && (KeyX <= ViewX))
			{
				DrawLine2D(Canvas,FVector2D(KeyX, ViewY - TotalBarHeight - RangeTickHeight), FVector2D(KeyX, ViewY - TotalBarHeight), KeyRangeMarkerColor);

				FString KeyString = FString::Printf( TEXT("%3.2fs"), KeyTime );				
				if ( InterpEd->bSnapToFrames )
				{
					KeyString += FString::Printf( TEXT(" / %df"), appRound(KeyTime / InterpEd->SnapAmount));
				}

				StringSize( GEditor->SmallFont, XL, YL, *KeyString);

				INT KeyLabelX = KeyX - 0.5f*XL;
				INT KeyLabelY = ViewY - TotalBarHeight - RangeTickHeight - YL - 3;

				DrawLabel(Canvas,KeyLabelX, KeyLabelY, *KeyString, KeyRangeMarkerColor);
			}
		}
	}
}

/** Draw various markers on the timeline */
void FInterpEdViewportClient::DrawMarkers(FViewport* Viewport, FCanvas* Canvas)
{
	INT ViewX = Viewport->GetSizeX();
	INT ViewY = Viewport->GetSizeY();
	INT ScaleTopY = ViewY - TotalBarHeight + 1;

	// Calculate screen X position that indicates current position in track.
	INT TrackPosX = InterpEd->LabelWidth + (InterpEd->Interp->Position - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;

	// Draw position indicator and line (if in viewed area)
	if( TrackPosX + TimeIndHalfWidth >= InterpEd->LabelWidth && TrackPosX <= ViewX)
	{
		if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( new HInterpEdTimelineBkg() );
		DrawTile(Canvas, TrackPosX - TimeIndHalfWidth - 1, ScaleTopY, (2*TimeIndHalfWidth) + 1, TimelineHeight, 0.f, 0.f, 0.f, 0.f, FColor(10,10,10) );
		if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( NULL );
	}

	INT MarkerArrowSize = 8;

	FIntPoint StartA = FIntPoint(0,					ScaleTopY);
	FIntPoint StartB = FIntPoint(0,					ScaleTopY+MarkerArrowSize);
	FIntPoint StartC = FIntPoint(-MarkerArrowSize,	ScaleTopY);

	FIntPoint EndA = FIntPoint(0,					ScaleTopY);
	FIntPoint EndB = FIntPoint(MarkerArrowSize,		ScaleTopY);
	FIntPoint EndC = FIntPoint(0,					ScaleTopY+MarkerArrowSize);


	// NOTE:	Each marker is drawn with an invisible square behind it to increase the clickable 
	//			space for marker selection. However, the markers are represented visually as triangles.

	// Draw loop section start/end
	FIntPoint EdStartPos( InterpEd->LabelWidth + (InterpEd->IData->EdSectionStart - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec, MarkerArrowSize );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HInterpEdMarker(ISM_LoopStart) );
	DrawTile(Canvas, EdStartPos.X - MarkerArrowSize, EdStartPos.Y + ScaleTopY, MarkerArrowSize, MarkerArrowSize, 0.f, 0.f, 1.f, 1.f, FLinearColor( 0.0f, 0.0f, 0.0f, 0.01f ) );
	DrawTriangle2D(Canvas, StartA + EdStartPos, FVector2D(0,0), StartB + EdStartPos, FVector2D(0,0), StartC + EdStartPos, FVector2D(0,0), SectionMarkerColor );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );


	FIntPoint EdEndPos( InterpEd->LabelWidth + (InterpEd->IData->EdSectionEnd - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec, MarkerArrowSize );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HInterpEdMarker(ISM_LoopEnd) );
	DrawTile(Canvas, EdEndPos.X, EdEndPos.Y + ScaleTopY, MarkerArrowSize, MarkerArrowSize, 0.f, 0.f, 1.f, 1.f, FLinearColor( 0.0f, 0.0f, 0.0f, 0.01f ) );
	DrawTriangle2D(Canvas, EndA + EdEndPos, FVector2D(0,0), EndB + EdEndPos, FVector2D(0,0), EndC + EdEndPos, FVector2D(0,0), SectionMarkerColor );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

	// Draw sequence start/end markers.
	FIntPoint StartPos( InterpEd->LabelWidth + (0.f - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec, 0 );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HInterpEdMarker(ISM_SeqStart) );
	DrawTile(Canvas, StartPos.X - MarkerArrowSize, StartPos.Y + ScaleTopY, MarkerArrowSize, MarkerArrowSize, 0.f, 0.f, 1.f, 1.f, FLinearColor( 0.0f, 0.0f, 0.0f, 0.01f ) );
	DrawTriangle2D( Canvas, StartA + StartPos, FVector2D(0,0), StartB + StartPos, FVector2D(0,0), StartC + StartPos, FVector2D(0,0), InterpMarkerColor );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

	FIntPoint EndPos( InterpEd->LabelWidth + (InterpEd->IData->InterpLength - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec, 0 );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HInterpEdMarker(ISM_SeqEnd) );
	DrawTile(Canvas, EndPos.X, EndPos.Y + ScaleTopY, MarkerArrowSize, MarkerArrowSize, 0.f, 0.f, 1.f, 1.f, FLinearColor( 0.0f, 0.0f, 0.0f, 0.01f ) );
	DrawTriangle2D( Canvas,  EndA + EndPos, FVector2D(0,0), EndB + EndPos, FVector2D(0,0), EndC + EndPos, FVector2D(0,0), InterpMarkerColor );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

	// Draw little tick indicating path-building time.
	INT PathBuildPosX = InterpEd->LabelWidth + (InterpEd->IData->PathBuildTime - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
	if( PathBuildPosX >= InterpEd->LabelWidth && PathBuildPosX <= ViewX)
	{
		DrawTile(Canvas, PathBuildPosX, ViewY - NavHeight - 10, 1, 11, 0.f, 0.f, 0.f, 0.f, FColor(200, 200, 255) );
	}

}

namespace
{
	const FColor TabColorNormal(128,128,128);
	const FColor TabColorSelected(192,160,128);
	const INT TabPadding = 1;
	const INT TabSpacing = 4;
	const INT TabRowHeight = 22;
}



/** 
 * Returns the vertical size of the entire group list for this viewport, in pixels
 */
INT FInterpEdViewportClient::ComputeGroupListContentHeight() const
{
	INT HeightInPixels = 0;

	// Loop through groups adding height contribution
	if( InterpEd->IData != NULL )
	{
		for( INT GroupIdx = 0; GroupIdx < InterpEd->IData->InterpGroups.Num(); ++GroupIdx )
		{
			const UInterpGroup* CurGroup = InterpEd->IData->InterpGroups( GroupIdx );

			// If this is a director group and the current window is not a director track window, then we'll skip over
			// the director group.  Similarly, for director track windows we'll skip over all non-director groups.
			if( CurGroup->IsA( UInterpGroupDirector::StaticClass() ) == bIsDirectorTrackWindow )
			{
				// If the group is visible, add the number of tracks in it as visible as well if it is not collapsed.
				if( CurGroup->bVisible )
				{
					HeightInPixels += GroupHeadHeight;

					// Also count the size of any expanded tracks in this group
					if( CurGroup->bCollapsed == FALSE )
					{
						// Account for visible tracks in this group
						for( INT CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
						{
							UInterpTrack* Track = CurGroup->InterpTracks( CurTrackIndex );
							if( Track->bVisible )
							{
								HeightInPixels += TrackHeight;

								if( Track->IsA( UInterpTrackMove::StaticClass() ) && Track->SubTracks.Num() > 0 )
								{
									// Move tracks have a 'group' for translation and rotation which increases the total height by 2 times track height
									HeightInPixels += (TrackHeight * 2);
								}

								// Increase height based on how many sub tracks are visible 
								for( INT SubTrackIndex = 0; SubTrackIndex < Track->SubTracks.Num(); ++SubTrackIndex )
								{
									UInterpTrack* SubTrack = Track->SubTracks( SubTrackIndex );
									if( SubTrack->bVisible )
									{
										HeightInPixels += SubTrackHeight;
									}
								}
							}
						}
					}
				}
			}
		}

		// For non-director track windows, we add some additional height so that we have a small empty area beneath
		// the list of groups that the user can right click on to summon the pop up menu to add new groups with.
		if( !bIsDirectorTrackWindow )
		{
			HeightInPixels += GroupHeadHeight;
		}
	}

	return HeightInPixels;
}

/** 
 * Returns the height of the viewable group list content box in pixels
 *
 *  @param ViewportHeight The size of the viewport in pixels
 *
 *  @return The height of the viewable content box (may be zero!)
 */
INT FInterpEdViewportClient::ComputeGroupListBoxHeight( const INT ViewportHeight ) const
{
	INT HeightOfExtras = 0;
	if( bWantFilterTabs )
	{
		HeightOfExtras += TabRowHeight;
	}
	if( bWantTimeline )
	{
		HeightOfExtras += TotalBarHeight;	// TimelineHeight + NavHeight
	}

	// Compute the height of the group list viewable area
	INT GroupListHeight = ViewportHeight - HeightOfExtras;
	return Max( 0, GroupListHeight );
}


/** 
 * Draws a subtrack group label.
 *
 *  @param Canvas	Canvas to draw on
 *	@param Track	Track which owns this group
 *  @param InGroup	Group to draw
 * 	@param GroupIndex	Index of the group in the ParentTrack's sub group array
 *	@param LabelDrawParams	Parameters for how to draw the label
 */
void FInterpEdViewportClient::DrawSubTrackGroup( FCanvas* Canvas, UInterpTrack* Track, const FSubTrackGroup& InGroup, INT GroupIndex, const FInterpTrackLabelDrawParams& LabelDrawParams, UInterpGroup* Group )
{
	// Track title block on left.
	if( Canvas->IsHitTesting() ) 
	{
		Canvas->SetHitProxy( new HInterpEdSubGroupTitle( Track, GroupIndex ) );
	}

	// Darken sub group labels
	FColor LabelColor = LabelDrawParams.GroupLabelColor;
	LabelColor.B -= 10;
	LabelColor.R -= 10;
	LabelColor.G -= 10;
	DrawTile(Canvas, -InterpEd->LabelWidth + InterpEdGlobals::TreeLabelSeparatorOffset, 0, InterpEd->LabelWidth - InterpEdGlobals::TreeLabelSeparatorOffset, TrackHeight - 1, 0.f, 0.f, 1.f, 1.f, LabelColor );

	// Draw a background for the check boxes on the left of the track label list
	DrawTile( Canvas, -InterpEd->LabelWidth, 0, InterpEdGlobals::TreeLabelSeparatorOffset, TrackHeight, 0.0f, 0.0f, 1.0f, 1.0f, InterpEdGlobals::TrackLabelAreaBackgroundColor );

	// Draw a line to separate the track check boxes from everything else
	DrawLine2D( Canvas, FVector2D( -InterpEd->LabelWidth + InterpEdGlobals::TreeLabelSeparatorOffset, 0 ), FVector2D( -InterpEd->LabelWidth + InterpEdGlobals::TreeLabelSeparatorOffset, TrackHeight - 1 ), FLinearColor::Black );

	// Highlight selected groups
	if( InGroup.bIsSelected )
	{
		DrawTile(Canvas, -InterpEd->LabelWidth, 0, InterpEd->LabelWidth, TrackHeight - 1, 0.f, 0.f, 1.f, 1.f, InterpEdGlobals::GroupOrTrackSelectedColor );

		// Also, we'll draw a rectangle around the selection
		INT MinX = -InterpEd->LabelWidth + 1;
		INT MinY = 0;
		INT MaxX = -1;
		INT MaxY = TrackHeight - 1;

		DrawLine2D(Canvas,FVector2D(MinX, MinY), FVector2D(MaxX, MinY), InterpEdGlobals::GroupOrTrackSelectedBorder);
		DrawLine2D(Canvas,FVector2D(MaxX, MinY), FVector2D(MaxX, MaxY), InterpEdGlobals::GroupOrTrackSelectedBorder);
		DrawLine2D(Canvas,FVector2D(MaxX, MaxY), FVector2D(MinX, MaxY), InterpEdGlobals::GroupOrTrackSelectedBorder);
		DrawLine2D(Canvas,FVector2D(MinX, MaxY), FVector2D(MinX, MinY), InterpEdGlobals::GroupOrTrackSelectedBorder);
	}


	INT IndentPixels = LabelDrawParams.IndentPixels;

	// Draw some 'tree view' lines to indicate the track is parented to a group
	{
		const FLOAT	HalfTrackHight = 0.5 * TrackHeight;
		const FLinearColor TreeNodeColor( 0.025f, 0.025f, 0.025f );
		const INT TreeNodeLeftPos = -InterpEd->LabelWidth + IndentPixels + 6;
		const INT TreeNodeTopPos = 2;
		const INT TreeNodeRightPos = -InterpEd->LabelWidth + IndentPixels + InterpEdGlobals::NumPixelsToIndentChildGroups;
		const INT TreeNodeBottomPos = HalfTrackHight;

		DrawLine2D( Canvas, FVector2D( TreeNodeLeftPos, TreeNodeTopPos ), FVector2D( TreeNodeLeftPos, TreeNodeBottomPos ), TreeNodeColor );
		DrawLine2D( Canvas, FVector2D( TreeNodeLeftPos, TreeNodeBottomPos ), FVector2D( TreeNodeRightPos, TreeNodeBottomPos ), TreeNodeColor );
	}

	const INT TrackIconSize = 16;
	const INT PaddedTrackIconSize = 20;
	INT TrackTitleIndentPixels = InterpEdGlobals::TrackTitleMargin + PaddedTrackIconSize + IndentPixels;

	// Draw Track Icon
	FString Text = InGroup.GroupName;
	// Truncate from front if name is too long
	INT XL, YL;
	StringSize(GEditor->SmallFont, XL, YL, *Text);

	// If too long to fit in label - truncate. TODO: Actually truncate by necessary amount!
 	if(XL > InterpEd->LabelWidth - TrackTitleIndentPixels - 2)
 	{
 		Text = FString::Printf( TEXT("...%s"), *(Text.Right(13)) );
 		StringSize(GEditor->SmallFont, XL, YL, *Text);
	}

	// Ghost out disabled groups
	FLinearColor TextColor;
	if( Track->IsDisabled() == FALSE )
	{
		TextColor = FLinearColor::White;
	}
	else
	{
		TextColor = FLinearColor(0.5f,0.5f,0.5f);
	}

	DrawLabel(Canvas, -InterpEd->LabelWidth + TrackTitleIndentPixels, appTruncFloat(0.5*TrackHeight - 0.5*YL), *Text, TextColor);
	if( Canvas->IsHitTesting() )
	{
		Canvas->SetHitProxy( NULL );
	}

	// Draw line under each track
	DrawTile(Canvas, -InterpEd->LabelWidth, TrackHeight - 1, LabelDrawParams.ViewX, 1, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );

	Canvas->PushRelativeTransform( FTranslationMatrix( FVector(-InterpEd->LabelWidth, 0, 0 ) ) );
	const INT HalfColArrowSize = 6;
	IndentPixels += InterpEdGlobals::NumPixelsToIndentChildGroups;
	// Draw little collapse widget.
	FIntPoint A,B,C;
	if( InGroup.bIsCollapsed )
	{
		INT HorizOffset = IndentPixels - 4;	// Extra negative offset for centering
		INT VertOffset = 0.5 * GroupHeadHeight;
		A = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset - HalfColArrowSize);
		B = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset + HalfColArrowSize);
		C = FIntPoint(HorizOffset + 2*HalfColArrowSize, VertOffset );
	}
	else
	{
		INT HorizOffset = IndentPixels;
		INT VertOffset = 0.5 * GroupHeadHeight - 3; // Extra negative offset for centering
		A = FIntPoint(HorizOffset, VertOffset);
		B = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset + HalfColArrowSize);
		C = FIntPoint(HorizOffset + 2*HalfColArrowSize, VertOffset);
	}

	DrawTriangle2D(Canvas, A, FVector2D(0.f, 0.f), B, FVector2D(0.f, 0.f), C, FVector2D(0.f, 0.f), FLinearColor::Black );

	// Invisible hit test geometry for the collapse/expand widget
	if( Canvas->IsHitTesting() ) 
	{
		Canvas->SetHitProxy( new HInterpEdTrackCollapseBtn( Track, GroupIndex ) );
	}

	DrawTile(
		Canvas,
		IndentPixels, 0.5 * GroupHeadHeight - HalfColArrowSize,	  // X, Y
		2 * HalfColArrowSize, 2 * HalfColArrowSize,               // Width, Height
		0.0f, 0.0f,                                               // U, V
		1.0f, 1.0f,                                               // USize, VSize
		FLinearColor( 0.0f, 0.0f, 0.0f, 0.01f ) );				  // Color and opacity

	if( Canvas->IsHitTesting() ) 
	{
		Canvas->SetHitProxy( NULL );
	};

	Canvas->PopTransform();

	CreatePushPropertiesOntoGraphButton( Canvas, Track, Group, GroupIndex, LabelDrawParams, TRUE );
}

/** 
 * Populates a list of drawing information for a specified sub track group.  
 * This is so all of the keyframes in all of the tracks in the group can be drawn directly on the group
 *
 * @param InterpEd			The interp editor which contains information for drawing
 * @param SubGroupOwner		The track which owns the sub group
 * @param InSubTrackGroup	The track to get drawing information for
 * @param KeySize			The size of each keyframe
 * @param OutDrawInfo		Array of drawing information that was created
 */
static void GetSubTrackGroupKeyDrawInfos( WxInterpEd& InterpEd, UInterpTrack* SubGroupOwner, const FSubTrackGroup& InSubTrackGroup, const FVector2D& KeySize, TArray<FKeyframeDrawInfo>& OutDrawInfos )
{
	UInterpGroup* TrackGroup = SubGroupOwner->GetOwningGroup();
	for( INT TrackIndex = 0; TrackIndex < InSubTrackGroup.TrackIndices.Num(); ++TrackIndex )
	{
		// For each track in the subgroup create a drawing information for each keyframe in each track.
		UInterpTrack* Track = SubGroupOwner->SubTracks( InSubTrackGroup.TrackIndices( TrackIndex ) );
		for(INT KeyframeIdx=0; KeyframeIdx<Track->GetNumKeyframes(); KeyframeIdx++)
		{
			// Create a new draw info
			FKeyframeDrawInfo DrawInfo;		
			DrawInfo.KeyTime = Track->GetKeyframeTime(KeyframeIdx);
			// Check to see if a keyframe at this position is already being drawn
			// We will not draw keyframes at the same location more than once
			INT ExistingInfoIndex = OutDrawInfos.FindItemIndex( DrawInfo );
			if( ExistingInfoIndex == INDEX_NONE )
			{
				// This is a new keyframe that has not been found yet
				DrawInfo.KeyColor = Track->GetKeyframeColor(KeyframeIdx);
				FVector2D TickPos;

				DrawInfo.KeyPos.X =  -KeySize.X / 2.0f + DrawInfo.KeyTime * InterpEd.PixelsPerSec;
				DrawInfo.KeyPos.Y =  (GroupHeadHeight - 1 - KeySize.Y) / 2.0f;

				// Is the keyframe selected
				DrawInfo.bSelected = InterpEd.KeyIsInSelection( TrackGroup, Track, KeyframeIdx );
				OutDrawInfos.AddItem( DrawInfo );
			}
			else
			{
				// A keyframe at this time is already being drawn, determine if it should be selected.  Group keyframes should only be selected if all tracks with a keyframe at that time are selected
				FKeyframeDrawInfo& ExistingInfo = OutDrawInfos(ExistingInfoIndex);
				ExistingInfo.bSelected = ExistingInfo.bSelected && InterpEd.KeyIsInSelection( TrackGroup, Track, KeyframeIdx );
			}

		}
	}
}

/** 
 * Draws a track in the interp editor
 * @param Canvas		Canvas to draw on
 * @param Track			Track to draw
 * @param Group			Group containing the track to draw
 * @param TrackDrawParams	Params for drawing the track
 * @params LabelDrawParams	Params for drawing the track label
 */
INT FInterpEdViewportClient::DrawTrack( FCanvas* Canvas, UInterpTrack* Track, UInterpGroup* Group, const FInterpTrackDrawParams& TrackDrawParams, const FInterpTrackLabelDrawParams& LabelDrawParams )
{
	INT TrackIndex = TrackDrawParams.TrackIndex;

	UBOOL bIsSubtrack = Track->GetOuter()->IsA( UInterpTrack::StaticClass() );
	// If we are drawing a subtrack, use the subtracks height and not the track height
	INT TrackHeightToUse = bIsSubtrack ? SubTrackHeight : TrackHeight; 
	INT TotalHeightAdded = 0;
	
	UBOOL bIsTrackWithinScrollArea =
		( LabelDrawParams.YOffset + TrackHeightToUse >= 0 ) && ( LabelDrawParams.YOffset <= LabelDrawParams.TrackAreaHeight );

	if( bIsTrackWithinScrollArea )
	{
		if( Canvas->IsHitTesting() )
		{
			Canvas->SetHitProxy( new HInterpEdTrackTimeline(Group, Track) );
		}
		DrawTile( Canvas, -InterpEd->LabelWidth + InterpEdGlobals::TreeLabelSeparatorOffset, 0, LabelDrawParams.ViewX - InterpEdGlobals::TreeLabelSeparatorOffset, TrackHeight - 1, 0.f, 0.f, 1.f, 1.f, FLinearColor( 0.0f, 0.0f, 0.0f, 0.01f ) );
		if( Canvas->IsHitTesting() ) 
		{
			Canvas->SetHitProxy( NULL );
		}

		Track->DrawTrack( Canvas, Group, TrackDrawParams );

		// If the track is in the visible scroll area, draw the tracks label
		DrawTrackLabel( Canvas, Track, Group, TrackDrawParams, LabelDrawParams );
	}

	TotalHeightAdded += TrackHeightToUse;

	// A list of keyframes to draw on track sub groups
	TArray< TArray<FKeyframeDrawInfo> > GroupDrawInfos;

	// Draw subtracks
	INT IndentPixels = LabelDrawParams.IndentPixels;
	if( Track->SubTracks.Num() > 0 )
	{
		// Track has subtracks, indent all subtracks
		IndentPixels += InterpEdGlobals::NumPixelsToIndentChildGroups;

		// Get all sub track keyframe drawing information
		const FVector2D KeySize(3.0f, GroupHeadHeight * 0.5f);
		for( INT GroupIndex = 0; GroupIndex < Track->SubTrackGroups.Num(); ++GroupIndex )
		{
			FSubTrackGroup& SubGroup = Track->SubTrackGroups( GroupIndex );
			GroupDrawInfos.AddItem( TArray<FKeyframeDrawInfo>() );
			GetSubTrackGroupKeyDrawInfos( *InterpEd, Track, SubGroup, KeySize, GroupDrawInfos(GroupIndex) );
		}

		// Draw subtracks if the track is not collapsed
		if( !Track->bIsCollapsed )
		{
			FInterpTrackLabelDrawParams SubLabelDrawParams = LabelDrawParams;
			FInterpTrackDrawParams SubTrackDrawParams = TrackDrawParams;

			// Draw subtracks based on grouping if there are any subgroups
			if( Track->SubTrackGroups.Num() > 0 )
			{
				IndentPixels += InterpEdGlobals::NumPixelsToIndentChildGroups;
				for( INT GroupIndex = 0; GroupIndex < Track->SubTrackGroups.Num(); ++GroupIndex )
				{
					FSubTrackGroup& SubGroup = Track->SubTrackGroups( GroupIndex );
					SubLabelDrawParams.IndentPixels = IndentPixels;
					SubLabelDrawParams.YOffset = TotalHeightAdded;

					// Determine if all tracks in a group are selected
					UBOOL bAllSubTracksSelected = TRUE;
			
					for( INT TrackIndex  = 0; TrackIndex < SubGroup.TrackIndices.Num(); ++TrackIndex )
					{
						if( !Track->SubTracks( SubGroup.TrackIndices( TrackIndex ) )->bIsSelected )
						{
							bAllSubTracksSelected = FALSE;
							break;
						}
					}
					SubGroup.bIsSelected = bAllSubTracksSelected;

					UBOOL bIsGroupWithinScrollArea = ( SubLabelDrawParams.YOffset + TrackHeightToUse >= 0 ) && ( SubLabelDrawParams.YOffset <= LabelDrawParams.TrackAreaHeight );

					if( bIsGroupWithinScrollArea )
					{
						// Draw the group if it should be visible
						Canvas->PushRelativeTransform(FTranslationMatrix(FVector(0,SubLabelDrawParams.YOffset,0)));
						DrawSubTrackGroup( Canvas, Track, SubGroup, GroupIndex, SubLabelDrawParams, Group );
						Canvas->PopTransform();

						// Draw keys on the group
						FVector2D TrackPos(InterpEd->LabelWidth - InterpEd->ViewStartTime * InterpEd->PixelsPerSec, SubLabelDrawParams.YOffset );
						Canvas->PushRelativeTransform(FTranslationMatrix(FVector(TrackPos.X-InterpEd->LabelWidth,TrackPos.Y,0)));
						DrawSubTrackGroupKeys( Canvas, Track, GroupIndex, GroupDrawInfos(GroupIndex), TrackPos, KeySize );
						Canvas->PopTransform();
					}

					// Further indent subtracks which are grouped
					IndentPixels += InterpEdGlobals::NumPixelsToIndentChildGroups;
					TotalHeightAdded += TrackHeight;

					if( !SubGroup.bIsCollapsed )
					{
						// Draw each track.  This part is recursive.
						for( INT TrackIndex  = 0; TrackIndex < SubGroup.TrackIndices.Num(); ++TrackIndex )
						{
							UInterpTrack* SubTrack = Track->SubTracks( SubGroup.TrackIndices( TrackIndex ) );
							SubTrackDrawParams.TrackHeight = SubTrackHeight;
							SubLabelDrawParams.IndentPixels = IndentPixels;
							SubLabelDrawParams.bTrackSelected = SubTrack->bIsSelected;
							
							Canvas->PushRelativeTransform(FTranslationMatrix(FVector(0,TotalHeightAdded,0)));
							TotalHeightAdded += DrawTrack( Canvas, SubTrack, Group, SubTrackDrawParams, SubLabelDrawParams );
							Canvas->PopTransform();
						}
					}
		
					IndentPixels -= InterpEdGlobals::NumPixelsToIndentChildGroups;
				}
			}
			else
			{
				// The track has no sub groups, just draw each subtrack directly.
				SubLabelDrawParams.IndentPixels = IndentPixels;
				for( INT SubTrackIndex = 0; SubTrackIndex < Track->SubTracks.Num(); ++SubTrackIndex )
				{
					UInterpTrack* SubTrack = Track->SubTracks( SubTrackIndex );
					SubTrackDrawParams.TrackHeight = SubTrackHeight;
					SubLabelDrawParams.bTrackSelected = SubTrack->bIsSelected;

					Canvas->PushRelativeTransform(FTranslationMatrix(FVector(0,TotalHeightAdded,0)));
					TotalHeightAdded += DrawTrack( Canvas, SubTrack, Group, SubTrackDrawParams, SubLabelDrawParams );
					Canvas->PopTransform();
				}
			}
		}

		if( bIsTrackWithinScrollArea && GroupDrawInfos.Num() > 0 )
		{
			// Draw keys on the parent track which correspond to all keyframes in subtracks
 			const FVector2D TickSize(3.0f, GroupHeadHeight * 0.5f);
 			FVector2D TrackPos(InterpEd->LabelWidth - InterpEd->ViewStartTime * InterpEd->PixelsPerSec, LabelDrawParams.YOffset );
			Canvas->PushAbsoluteTransform(FTranslationMatrix(FVector(TrackPos.X,TrackPos.Y,0)));
 		
			for( INT GroupIndex = 0; GroupIndex < Track->SubTrackGroups.Num(); ++GroupIndex )
			{
				FSubTrackGroup& SubGroup = Track->SubTrackGroups( GroupIndex );
				DrawSubTrackGroupKeys( Canvas, Track, INDEX_NONE, GroupDrawInfos(GroupIndex), TrackPos, KeySize );
			}
 
			Canvas->PopTransform();
		}
	}

	return TotalHeightAdded;
}

void FInterpEdViewportClient::CreatePushPropertiesOntoGraphButton( FCanvas* Canvas, UInterpTrack* Track, UInterpGroup* Group, INT GroupIndex, const FInterpTrackLabelDrawParams& LabelDrawParams, UBOOL bIsSubTrack )
{
	INT TrackHeightToUse = bIsSubTrack ? SubTrackHeight : TrackHeight;

	if(
		Track->IsA(UInterpTrackFloatBase::StaticClass()) ||
		Track->IsA(UInterpTrackVectorBase::StaticClass()) || 
		Track->IsA(UInterpTrackMove::StaticClass()) || 
		Track->IsA(UInterpTrackLinearColorBase::StaticClass()) ||
		Track->IsA(UInterpTrackVectorBase::StaticClass())
	)
	{
		UMaterial* GraphMat = NULL;
		if( Track->SubTracks.Num() )
		{
			UBOOL bSubtracksInCurveEd = FALSE;
			// see if any subtracks are in the curve editor
			if( GroupIndex == -1 )
			{
				for( INT SubTrackIndex = 0; SubTrackIndex < Track->SubTracks.Num(); ++SubTrackIndex )
				{
					if( InterpEd->IData->CurveEdSetup->ShowingCurve( Track->SubTracks(SubTrackIndex) ) )
					{
						bSubtracksInCurveEd = TRUE;
						break;
					}
				}
			}
			else
			{
				FSubTrackGroup& Group = Track->SubTrackGroups( GroupIndex );
				for( INT Index = 0; Index < Group.TrackIndices.Num(); ++Index )
				{
					if( InterpEd->IData->CurveEdSetup->ShowingCurve( Track->SubTracks( Group.TrackIndices( Index ) ) ) )
					{
						bSubtracksInCurveEd = TRUE;
						break;
					}
				}

			}
			GraphMat = bSubtracksInCurveEd ? LabelDrawParams.GraphOnMat : LabelDrawParams.GraphOffMat;
		}
		else
		{
			GraphMat = InterpEd->IData->CurveEdSetup->ShowingCurve(Track) ? LabelDrawParams.GraphOnMat : LabelDrawParams.GraphOffMat;
		}

		// Draw button for pushing properties onto graph view.
		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HInterpEdTrackGraphPropBtn( Group, GroupIndex, Track ) );
		DrawTile(Canvas, -14, TrackHeightToUse-11, 8, 8, 0.f, 0.f, 1.f, 1.f, GraphMat->GetRenderProxy(0) );
		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
	}
}

/** 
 * Draws a track label for a track
 * @param Canvas		Canvas to draw on
 * @param Track			Track that needs a label drawn for it.
 * @param Group			Group containing the track to draw
 * @param TrackDrawParams	Params for drawing the track
 * @params LabelDrawParams	Params for drawing the track label
 */
void FInterpEdViewportClient::DrawTrackLabel( FCanvas* Canvas, UInterpTrack* Track, UInterpGroup* Group, const FInterpTrackDrawParams& TrackDrawParams, const FInterpTrackLabelDrawParams& LabelDrawParams )
{
	const INT TrackIndex = TrackDrawParams.TrackIndex;

	const UBOOL bIsSubTrack = Track->GetOuter()->IsA( UInterpTrack::StaticClass() );
	INT TrackHeightToUse = bIsSubTrack ? SubTrackHeight : TrackHeight;

	// The track color will simply be a brighter copy of the group color.  We do this so that the colors will match.
	FColor TrackLabelColor = LabelDrawParams.GroupLabelColor;
	TrackLabelColor += FColor( 40, 40, 40 );

	// Track title block on left.
	if( Canvas->IsHitTesting() ) 
	{
		Canvas->SetHitProxy( new HInterpEdTrackTitle( Group, Track ) );
	}

	DrawTile(Canvas, -InterpEd->LabelWidth + InterpEdGlobals::TreeLabelSeparatorOffset, 0, InterpEd->LabelWidth - InterpEdGlobals::TreeLabelSeparatorOffset, TrackHeightToUse - 1, 0.f, 0.f, 1.f, 1.f, TrackLabelColor );

	// Draw a background for the check boxes on the left of the track label list
	DrawTile( Canvas, -InterpEd->LabelWidth, 0, InterpEdGlobals::TreeLabelSeparatorOffset, TrackHeightToUse, 0.0f, 0.0f, 1.0f, 1.0f, InterpEdGlobals::TrackLabelAreaBackgroundColor );

	// Draw a line to separate the track check boxes from everything else
	DrawLine2D( Canvas, FVector2D( -InterpEd->LabelWidth + InterpEdGlobals::TreeLabelSeparatorOffset, 0 ), FVector2D( -InterpEd->LabelWidth + InterpEdGlobals::TreeLabelSeparatorOffset, TrackHeightToUse - 1 ), FLinearColor::Black );


	if( LabelDrawParams.bTrackSelected )
	{
		DrawTile(Canvas, -InterpEd->LabelWidth, 0, InterpEd->LabelWidth, TrackHeightToUse - 1, 0.f, 0.f, 1.f, 1.f, InterpEdGlobals::GroupOrTrackSelectedColor );

		// Also, we'll draw a rectangle around the selection
		INT MinX = -InterpEd->LabelWidth + 1;
		INT MinY = 0;
		INT MaxX = -1;
		INT MaxY = TrackHeightToUse - 1;

		DrawLine2D(Canvas,FVector2D(MinX, MinY), FVector2D(MaxX, MinY), InterpEdGlobals::GroupOrTrackSelectedBorder);
		DrawLine2D(Canvas,FVector2D(MaxX, MinY), FVector2D(MaxX, MaxY), InterpEdGlobals::GroupOrTrackSelectedBorder);
		DrawLine2D(Canvas,FVector2D(MaxX, MaxY), FVector2D(MinX, MaxY), InterpEdGlobals::GroupOrTrackSelectedBorder);
		DrawLine2D(Canvas,FVector2D(MinX, MaxY), FVector2D(MinX, MinY), InterpEdGlobals::GroupOrTrackSelectedBorder);
	}


	INT IndentPixels = LabelDrawParams.IndentPixels;

	// Draw some 'tree view' lines to indicate the track is parented to a group
	{
		const FLOAT	HalfTrackHight = 0.5 * TrackHeightToUse;
		const FLinearColor TreeNodeColor( 0.025f, 0.025f, 0.025f );
		const INT TreeNodeLeftPos = -InterpEd->LabelWidth + IndentPixels + 6;
		const INT TreeNodeTopPos = 2;
		const INT TreeNodeRightPos = -InterpEd->LabelWidth + IndentPixels + (Track->SubTracks.Num() > 0 ? InterpEdGlobals::NumPixelsToIndentChildGroups : InterpEdGlobals::NumPixelsToIndentChildGroups*2);
		const INT TreeNodeBottomPos = HalfTrackHight;

		DrawLine2D( Canvas, FVector2D( TreeNodeLeftPos, TreeNodeTopPos ), FVector2D( TreeNodeLeftPos, TreeNodeBottomPos ), TreeNodeColor );
		DrawLine2D( Canvas, FVector2D( TreeNodeLeftPos, TreeNodeBottomPos ), FVector2D( TreeNodeRightPos, TreeNodeBottomPos ), TreeNodeColor );
	}

	if( !bIsSubTrack )
	{
		IndentPixels += InterpEdGlobals::NumPixelsToIndentChildGroups;
	}


	const INT TrackIconSize = 16;
	const INT PaddedTrackIconSize = 20;
	INT TrackTitleIndentPixels = InterpEdGlobals::TrackTitleMargin + PaddedTrackIconSize + IndentPixels;

	// Draw Track Icon
	UMaterial* TrackIconMat = Track->GetTrackIcon();
	if( TrackIconMat )
	{
		DrawTile(Canvas, -InterpEd->LabelWidth + TrackTitleIndentPixels - PaddedTrackIconSize, 0.5*(TrackHeightToUse - TrackIconSize), TrackIconSize, TrackIconSize, 0.f, 0.f, 1.f, 1.f, TrackIconMat->GetRenderProxy(0) );
	}

	// Truncate from front if name is too long
	FString TrackTitle = FString( *Track->TrackTitle );
	INT XL, YL;
	StringSize(GEditor->SmallFont, XL, YL, *TrackTitle);

	if(XL > InterpEd->LabelWidth - TrackTitleIndentPixels - 2)
	{
		TrackTitle = FString::Printf( TEXT("...%s"), *(TrackTitle.Right(13)) );
		StringSize(GEditor->SmallFont, XL, YL, *TrackTitle);
	}

	FLinearColor TextColor;

	if(Track->IsDisabled() == FALSE)
	{
		TextColor = FLinearColor::White;
	}
	else
	{
		TextColor = FLinearColor(0.5f,0.5f,0.5f);
	}

	DrawLabel(Canvas, -InterpEd->LabelWidth + TrackTitleIndentPixels, 0.5*TrackHeightToUse - 0.5*YL, *TrackTitle, TextColor);
	if( Canvas->IsHitTesting() )
	{
		Canvas->SetHitProxy( NULL );
	}

	UInterpTrackEvent* EventTrack = Cast<UInterpTrackEvent>(Track);
	if(EventTrack)
	{
		UMaterial* ForwardMat = (EventTrack->bFireEventsWhenForwards) ? LabelDrawParams.ForwardEventOnMat : LabelDrawParams.ForwardEventOffMat;
		UMaterial* BackwardMat = (EventTrack->bFireEventsWhenBackwards) ? LabelDrawParams.BackwardEventOnMat : LabelDrawParams.BackwardEventOffMat;

		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HInterpEdEventDirBtn(Group, TrackIndex, IED_Backward) );
		DrawTile(Canvas, -24, TrackHeightToUse-11, 8, 8, 0.f, 0.f, 1.f, 1.f, BackwardMat->GetRenderProxy(0) );
		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HInterpEdEventDirBtn(Group, TrackIndex, IED_Forward) );
		DrawTile(Canvas, -14, TrackHeightToUse-11, 8, 8, 0.f, 0.f, 1.f, 1.f, ForwardMat->GetRenderProxy(0) );
		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
	}

	// For Movement tracks, draw a button that toggles display of the 3D trajectory for this track
	if( Track->IsA( UInterpTrackMove::StaticClass() ) )
	{
		UInterpTrackMove* MovementTrack = CastChecked<UInterpTrackMove>( Track );
		UMaterial* TrajectoryButtonMat = MovementTrack->bHide3DTrack ? LabelDrawParams.GraphOffMat : LabelDrawParams.TrajectoryOnMat;

		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HInterpEdTrackTrajectoryButton( Group, Track ) );
		DrawTile(Canvas, -24, TrackHeightToUse-11, 8, 8, 0.f, 0.f, 1.f, 1.f, TrajectoryButtonMat->GetRenderProxy(0) );
		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
	}

	CreatePushPropertiesOntoGraphButton( Canvas, Track, Group, -1, LabelDrawParams, bIsSubTrack );

	// Draw line under each track
	DrawTile(Canvas, -InterpEd->LabelWidth, TrackHeightToUse - 1, LabelDrawParams.ViewX, 1, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );

	if( !bIsSubTrack )
	{
		// Draw an icon to let the user enable/disable a track.
		if( Canvas->IsHitTesting() ) 
		{
			Canvas->SetHitProxy( new HInterpEdTrackDisableTrackBtn( Group, Track ) );
		}

		FLOAT YPos = (TrackHeightToUse - InterpEdGlobals::DisableTrackIconSize.Y) / 2.0f;
		DrawTile(Canvas, -InterpEd->LabelWidth + InterpEdGlobals::DisableTrackCheckBoxHorizOffset, YPos, InterpEdGlobals::DisableTrackIconSize.X, InterpEdGlobals::DisableTrackIconSize.Y, 0,0,1,1, FLinearColor::Black);

		if( Track->IsDisabled() == FALSE )
		{
			DrawTile(Canvas, -InterpEd->LabelWidth + InterpEdGlobals::DisableTrackCheckBoxHorizOffset, YPos, InterpEdGlobals::DisableTrackIconSize.X, InterpEdGlobals::DisableTrackIconSize.Y, 0,0,1,1, LabelDrawParams.DisableTrackMat->GetRenderProxy(0));
		}

		if( Canvas->IsHitTesting() )
		{
			Canvas->SetHitProxy( NULL );
		}
	}

	// If the track has subtracks, draw a collapse widget to collapse the track
	if( Track->SubTracks.Num() > 0 )
	{
		Canvas->PushRelativeTransform( FTranslationMatrix( FVector(-InterpEd->LabelWidth, 0, 0 ) ) );
		const INT HalfColArrowSize = 6;

		FIntPoint A,B,C;
		if( Track->bIsCollapsed )
		{
			INT HorizOffset = IndentPixels - 4;	// Extra negative offset for centering
			INT VertOffset = 0.5 * GroupHeadHeight;
			A = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset - HalfColArrowSize);
			B = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset + HalfColArrowSize);
			C = FIntPoint(HorizOffset + 2*HalfColArrowSize, VertOffset );
		}
		else
		{
			INT HorizOffset = IndentPixels;
			INT VertOffset = 0.5 * GroupHeadHeight - 3; // Extra negative offset for centering
			A = FIntPoint(HorizOffset, VertOffset);
			B = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset + HalfColArrowSize);
			C = FIntPoint(HorizOffset + 2*HalfColArrowSize, VertOffset);
		}

		DrawTriangle2D(Canvas, A, FVector2D(0.f, 0.f), B, FVector2D(0.f, 0.f), C, FVector2D(0.f, 0.f), FLinearColor::Black );

		// Invisible hit test geometry for the collapse/expand widget
		if( Canvas->IsHitTesting() ) 
		{
			Canvas->SetHitProxy( new HInterpEdTrackCollapseBtn( Track ) );
		}

		DrawTile(
			Canvas,
			IndentPixels, 0.5 * GroupHeadHeight - HalfColArrowSize,	  // X, Y
			2 * HalfColArrowSize, 2 * HalfColArrowSize,                   // Width, Height
			0.0f, 0.0f,                                                   // U, V
			1.0f, 1.0f,                                                   // USize, VSize
			FLinearColor( 0.0f, 0.0f, 0.0f, 0.01f ) );				  // Color and opacity

		if( Canvas->IsHitTesting() ) 
		{
			Canvas->SetHitProxy( NULL );
		};

		Canvas->PopTransform();
	}
}

/** Draw the track editor using the supplied 2D RenderInterface. */
void FInterpEdViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	Canvas->PushAbsoluteTransform(FMatrix::Identity);

	// @todo frick: make sure the viewport is only drawn when IsShown is true!

	// Erase background
	Clear(Canvas, FColor(162,162,162) );

	const INT ViewX = Viewport->GetSizeX();
	const INT ViewY = Viewport->GetSizeY();

	// @todo frick: Weird to compute this here and storing it in parent
	InterpEd->TrackViewSizeX = ViewX - InterpEd->LabelWidth;

	// Calculate ratio between screen pixels and elapsed time.
	// @todo frick: Weird to compute this here and storing it in parent
	InterpEd->PixelsPerSec = Max( 1.0f, FLOAT(ViewX - InterpEd->LabelWidth)/FLOAT(InterpEd->ViewEndTime - InterpEd->ViewStartTime) );
	InterpEd->NavPixelsPerSecond = Max( 0.0f, FLOAT(ViewX - InterpEd->LabelWidth)/InterpEd->IData->InterpLength );

	DrawGrid(Viewport, Canvas, false);

	// Draw 'null regions' if viewing past start or end.
	INT StartPosX = InterpEd->LabelWidth + (0.f - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
	INT EndPosX = InterpEd->LabelWidth + (InterpEd->IData->InterpLength - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
	if(InterpEd->ViewStartTime < 0.f)
	{
		DrawTile(Canvas,0, 0, StartPosX, ViewY, 0.f, 0.f, 1.f, 1.f, NullRegionColor);
	}

	if(InterpEd->ViewEndTime > InterpEd->IData->InterpLength)
	{
		DrawTile(Canvas,EndPosX, 0, ViewX-EndPosX, ViewY, 0.f, 0.f, 1.f, 1.f, NullRegionColor);
	}

	// Draw lines on borders of 'null area'
	INT TrackAreaHeight = ViewY;
	if( bWantTimeline )
	{
		TrackAreaHeight -= TotalBarHeight;
	}
	if(InterpEd->ViewStartTime < 0.f)
	{
		DrawLine2D(Canvas,FVector2D(StartPosX, 0), FVector2D(StartPosX, TrackAreaHeight), NullRegionBorderColor);
	}

	if(InterpEd->ViewEndTime > InterpEd->IData->InterpLength)
	{
		DrawLine2D(Canvas,FVector2D(EndPosX, 0), FVector2D(EndPosX, TrackAreaHeight), NullRegionBorderColor);
	}

	// Draw loop region.	
	INT EdStartPosX = InterpEd->LabelWidth + (InterpEd->IData->EdSectionStart - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
	INT EdEndPosX = InterpEd->LabelWidth + (InterpEd->IData->EdSectionEnd - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;

	DrawTile(Canvas,EdStartPosX, 0, EdEndPosX - EdStartPosX, TrackAreaHeight, 0.f, 0.f, 1.f, 1.f, InterpEd->RegionFillColor);

	// Draw titles block down left.
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HInterpEdTrackBkg() );
	DrawTile(Canvas, 0, 0, InterpEd->LabelWidth, TrackAreaHeight, 0.f, 0.f, 0.f, 0.f, InterpEdGlobals::TrackLabelAreaBackgroundColor );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

	FInterpTrackLabelDrawParams LabelDrawParams;

	LabelDrawParams.ViewX = ViewX;
	LabelDrawParams.ViewY = ViewY;

	// Get materials for cam-locked icon.
	LabelDrawParams.CamLockedIcon = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(), NULL, TEXT("EditorMaterials.MatineeGroups.MAT_Groups_View_On_Mat"), NULL, LOAD_None, NULL );
	check(LabelDrawParams.CamLockedIcon);

	LabelDrawParams.CamUnlockedIcon = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(), NULL, TEXT("EditorMaterials.MatineeGroups.MAT_Groups_View_Off_Mat"), NULL, LOAD_None, NULL );
	check(LabelDrawParams.CamUnlockedIcon);

	// Get materials for Event direction buttons.
	LabelDrawParams.ForwardEventOnMat = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(), NULL, TEXT("EditorMaterials.MatineeGroups.MAT_Groups_Right_On_Mat"), NULL, LOAD_None, NULL );
	check(LabelDrawParams.ForwardEventOnMat);

	LabelDrawParams.ForwardEventOffMat = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(), NULL, TEXT("EditorMaterials.MatineeGroups.MAT_Groups_Right_Off_Mat"), NULL, LOAD_None, NULL );
	check(LabelDrawParams.ForwardEventOffMat);

	LabelDrawParams.BackwardEventOnMat = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(), NULL, TEXT("EditorMaterials.MatineeGroups.MAT_Groups_Left_On_Mat"), NULL, LOAD_None, NULL );
	check(LabelDrawParams.BackwardEventOnMat);

	LabelDrawParams.BackwardEventOffMat = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(), NULL, TEXT("EditorMaterials.MatineeGroups.MAT_Groups_Left_Off_Mat"), NULL, LOAD_None, NULL );
	check(LabelDrawParams.BackwardEventOffMat);

	LabelDrawParams.DisableTrackMat = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(), NULL, TEXT("EditorMaterials.Matinee.MAT_EnableTrack_Mat"), NULL, LOAD_None, NULL );
	check(LabelDrawParams.DisableTrackMat);


	// Get materials for sending to curve editor
	LabelDrawParams.GraphOnMat = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(), NULL, TEXT("EditorMaterials.MatineeGroups.MAT_Groups_Graph_On_Mat"), NULL, LOAD_None, NULL );
	check(LabelDrawParams.GraphOnMat);

	LabelDrawParams.GraphOffMat = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(), NULL, TEXT("EditorMaterials.MatineeGroups.MAT_Groups_Graph_Off_Mat"), NULL, LOAD_None, NULL );
	check(LabelDrawParams.GraphOffMat);

	
	// Get material for toggle trajectories
	LabelDrawParams.TrajectoryOnMat = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(), NULL, TEXT("EditorMaterials.MatineeGroups.MAT_Groups_Trajectory_On_Mat"), NULL, LOAD_None, NULL );
	check( LabelDrawParams.TrajectoryOnMat != NULL );

	// Check to see if we have a director group.  If so, we'll want to draw it on top of the other items!
	INT DirGroupIndex = 0;
	UBOOL bHaveDirGroup = InterpEd->FindDirectorGroup( DirGroupIndex ); // Out

	// Compute vertical start offset
	INT StartYOffset = ThumbPos_Vert;
	if( bWantFilterTabs )
	{
		StartYOffset += TabRowHeight;
	}
	INT YOffset = StartYOffset;

	
	// Setup draw params which will be passed to the track rendering function for every visible track.  We'll
	// make additional changes to this after each track is rendered.
	FInterpTrackDrawParams TrackDrawParams;
	TrackDrawParams.TrackIndex = INDEX_NONE;
	TrackDrawParams.TrackWidth = ViewX - InterpEd->LabelWidth;
	TrackDrawParams.TrackHeight = TrackHeight - 1;
	TrackDrawParams.StartTime = InterpEd->ViewStartTime;
	TrackDrawParams.PixelsPerSec = InterpEd->PixelsPerSec;
	TrackDrawParams.TimeCursorPosition = InterpEd->Interp->Position;
	TrackDrawParams.SnapAmount = InterpEd->SnapAmount;
	TrackDrawParams.bPreferFrameNumbers = InterpEd->bSnapToFrames && InterpEd->bPreferFrameNumbers;
	TrackDrawParams.bShowTimeCursorPosForAllKeys = InterpEd->bShowTimeCursorPosForAllKeys;
	TrackDrawParams.bAllowKeyframeBarSelection = InterpEd->IsKeyframeBarSelectionAllowed();
	TrackDrawParams.bAllowKeyframeTextSelection = InterpEd->IsKeyframeTextSelectionAllowed();
	TrackDrawParams.SelectedKeys = InterpEd->Opt->SelectedKeys;


	UInterpGroup* CurParentGroup = NULL;

	// Draw visible groups/tracks
	for( INT CurGroupIndex = 0; CurGroupIndex < InterpEd->IData->InterpGroups.Num(); ++CurGroupIndex )
	{
		// Draw group header
		UInterpGroup* Group = InterpEd->IData->InterpGroups( CurGroupIndex );

		UBOOL bIsGroupVisible = Group->bVisible;
		if( Group->bIsParented )
		{
			// If we're parented then we're only visible if our parent group is not collapsed
			check( CurParentGroup != NULL );
			if( CurParentGroup->bCollapsed )
			{
				// Parent group is collapsed, so we should not be rendered
				bIsGroupVisible = FALSE;
			}
		}
		else
		{
			// If this group is not parented, then we clear our current parent
			CurParentGroup = NULL;
		}

		// If this is a director group and the current window is not a director track window, then we'll skip over
		// the director group.  Similarly, for director track windows we'll skip over all non-director groups.
		UBOOL bIsGroupAppropriateForWindow =
		  ( Group->IsA( UInterpGroupDirector::StaticClass() ) == bIsDirectorTrackWindow );

		// Only draw if the group isn't filtered and isn't culled
		if( bIsGroupVisible && bIsGroupAppropriateForWindow )
		{
			// If this is a child group then we'll want to indent everything a little bit
			INT IndentPixels = InterpEdGlobals::TreeLabelsMargin;  // Also extend past the 'track enabled' check box column
			if( Group->bIsParented )
			{
				IndentPixels += InterpEdGlobals::NumPixelsToIndentChildGroups;
			}


			// Does the group have an actor associated with it?
			AActor* GroupActor = NULL;
			{
				// @todo Performance: Slow to do a linear search here in the middle of our draw call
				UInterpGroupInst* GrInst = InterpEd->Interp->FindFirstGroupInst(Group);
				if( GrInst )
				{
					GroupActor = GrInst->GroupActor;
				}
			}

			// Select color for group label
			FColor GroupLabelColor = ChooseLabelColorForGroupActor( Group, GroupActor );
			
			// Check to see if we're out of view (scrolled away).  If so, then we don't need to draw!
			UBOOL bIsGroupWithinScrollArea =
				( YOffset + GroupHeadHeight >= 0 ) && ( YOffset <= TrackAreaHeight );
			if( bIsGroupWithinScrollArea )
			{
				Canvas->PushRelativeTransform(FTranslationMatrix(FVector(0,YOffset,0)));

				if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HInterpEdGroupTitle(Group) );
				INT MinTitleX = Group->bIsFolder ? 0 : InterpEdGlobals::TreeLabelSeparatorOffset;
				DrawTile(Canvas, MinTitleX, 0, ViewX - MinTitleX, GroupHeadHeight, 0.f, 0.f, 1.f, 1.f, GroupLabelColor );
				if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );


				if( !Group->bIsFolder )
				{
					// Draw a background for the check boxes on the left of the track label list
					DrawTile( Canvas, 0, 0, InterpEdGlobals::TreeLabelSeparatorOffset, GroupHeadHeight, 0.0f, 0.0f, 1.0f, 1.0f, InterpEdGlobals::TrackLabelAreaBackgroundColor );

					// Draw a line to separate the track check boxes from everything else
					DrawLine2D( Canvas, FVector2D( InterpEdGlobals::TreeLabelSeparatorOffset, 0 ), FVector2D( InterpEdGlobals::TreeLabelSeparatorOffset, GroupHeadHeight - 1 ), FLinearColor::Black );
				}


				// Select color for group label
				if( InterpEd->IsGroupSelected(Group) )
				{
					FColor GroupColor = InterpEdGlobals::GroupOrTrackSelectedColor;
					FColor GroupBorder = InterpEdGlobals::GroupOrTrackSelectedBorder;

					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HInterpEdGroupTitle(Group) );
					{
						DrawTile(Canvas, 0, 0, ViewX, GroupHeadHeight, 0.f, 0.f, 1.f, 1.f, GroupColor );

						// Also, we'll draw a rectangle around the selection
						INT MinX = 1;
						INT MinY = 0;
						INT MaxX = ViewX - 1;
						INT MaxY = GroupHeadHeight - 1;
						
						DrawLine2D(Canvas,FVector2D(MinX, MinY), FVector2D(MaxX, MinY), GroupBorder);
						DrawLine2D(Canvas,FVector2D(MaxX, MinY), FVector2D(MaxX, MaxY), GroupBorder);
						DrawLine2D(Canvas,FVector2D(MaxX, MaxY), FVector2D(MinX, MaxY), GroupBorder);
						DrawLine2D(Canvas,FVector2D(MinX, MaxY), FVector2D(MinX, MinY), GroupBorder);
					}
					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
				}


				// Peek ahead to see if we have any tracks or groups parented to this group
				INT NumChildGroups = 0;
				if( !Group->bIsParented )
				{
					for( INT OtherGroupIndex = CurGroupIndex + 1; OtherGroupIndex < InterpEd->IData->InterpGroups.Num(); ++OtherGroupIndex )
					{
						UInterpGroup* OtherGroup = InterpEd->IData->InterpGroups( OtherGroupIndex );

						// If this is a director group and the current window is not a director track window, then we'll skip over
						// the director group.  Similarly, for director track windows we'll skip over all non-director groups.
						UBOOL bIsOtherGroupAppropriateForWindow =
							( OtherGroup->IsA( UInterpGroupDirector::StaticClass() ) == bIsDirectorTrackWindow );

						// Only consider the group if it isn't filtered and isn't culled
						if( OtherGroup->bVisible && bIsOtherGroupAppropriateForWindow )
						{
							if( OtherGroup->bIsParented )
							{
								++NumChildGroups;
							}
							else
							{
								// We've reached a group that isn't parented (thus it's a root), so we can just bail
								break;
							}
						}
					}
				}


				// Does the group have anything parented to it?  If so we'll draw a widget that can be used to expand or
				// collapse the group.
				INT HalfColArrowSize = 6;
				bool bCurGroupHasAnyChildTracksOrGroups = ( Group->InterpTracks.Num() > 0 || NumChildGroups > 0 );
				if( bCurGroupHasAnyChildTracksOrGroups )
				{
					// Draw little collapse-group widget.
					FIntPoint A,B,C;
					if(Group->bCollapsed)
					{
						INT HorizOffset = IndentPixels - 4;	// Extra negative offset for centering
						INT VertOffset = 0.5 * GroupHeadHeight;
						A = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset - HalfColArrowSize);
						B = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset + HalfColArrowSize);
						C = FIntPoint(HorizOffset + 2*HalfColArrowSize, VertOffset );
					}
					else
					{
						INT HorizOffset = IndentPixels;
						INT VertOffset = 0.5 * GroupHeadHeight - 3; // Extra negative offset for centering
						A = FIntPoint(HorizOffset, VertOffset);
						B = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset + HalfColArrowSize);
						C = FIntPoint(HorizOffset + 2*HalfColArrowSize, VertOffset);
					}

					DrawTriangle2D(Canvas, A, FVector2D(0.f, 0.f), B, FVector2D(0.f, 0.f), C, FVector2D(0.f, 0.f), FLinearColor::Black );

					// Invisible hit test geometry for the collapse/expand widget
					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HInterpEdGroupCollapseBtn(Group) );
					DrawTile(
						Canvas,
						IndentPixels, 0.5 * GroupHeadHeight - HalfColArrowSize,	  // X, Y
						2 * HalfColArrowSize, 2 * HalfColArrowSize,                   // Width, Height
						0.0f, 0.0f,                                                   // U, V
						1.0f, 1.0f,                                                   // USize, VSize
						FLinearColor( 0.0f, 0.0f, 0.0f, 0.01f ) );                    // Color and opacity
					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
				}


				// If this is child group, then draw some 'tree view' lines to indicate that
				if( Group->bIsParented )
				{
					const FLOAT	HalfHeadHeight = 0.5 * GroupHeadHeight;
					const FLinearColor TreeNodeColor( 0.025f, 0.025f, 0.025f );
					const INT TreeNodeLeftPos = InterpEdGlobals::TreeLabelsMargin + 6;
					const INT TreeNodeTopPos = 2;
					const INT TreeNodeBottomPos = HalfHeadHeight;

					// If we're drawing an expand/collapse widget, then we'll make sure the line doesn't extend beyond that
					INT TreeNodeRightPos = InterpEdGlobals::TreeLabelsMargin + InterpEdGlobals::NumPixelsToIndentChildGroups + 1;
					if( !bCurGroupHasAnyChildTracksOrGroups )
					{
						TreeNodeRightPos += HalfColArrowSize * 2;
					}

					DrawLine2D( Canvas, FVector2D( TreeNodeLeftPos, TreeNodeTopPos ), FVector2D( TreeNodeLeftPos, TreeNodeBottomPos ), TreeNodeColor );
					DrawLine2D( Canvas, FVector2D( TreeNodeLeftPos, TreeNodeBottomPos ), FVector2D( TreeNodeRightPos, TreeNodeBottomPos ), TreeNodeColor );
				}


				// Draw the group name
				INT XL, YL;
				StringSize(GEditor->SmallFont, XL, YL, *Group->GroupName.ToString());
				DrawLabel(Canvas, IndentPixels + HeadTitleMargin + 2*HalfColArrowSize, 0.5*GroupHeadHeight - 0.5*YL, *Group->GroupName.ToString(), InterpEdGlobals::GroupNameTextColor );
				

				// Draw button for binding camera to this group, but only if we need to.  If the group has an actor bound to
				// it, or is a director group, then it gets a camera!
				if( GroupActor != NULL || Group->IsA( UInterpGroupDirector::StaticClass() ) )
				{
					UMaterial* ButtonMat = (Group == InterpEd->CamViewGroup) ? LabelDrawParams.CamLockedIcon : LabelDrawParams.CamUnlockedIcon;
					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HInterpEdGroupLockCamBtn(Group) );
					DrawTile(Canvas, InterpEd->LabelWidth - 26, (0.5*GroupHeadHeight)-8, 16, 16, 0.f, 0.f, 1.f, 1.f, ButtonMat->GetRenderProxy(0) );
					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
				}

				if( !Group->bIsFolder )
				{
					// Draw little bar showing group colour
					DrawTile(Canvas, InterpEd->LabelWidth - 6, 0, 6, GroupHeadHeight, 0.f, 0.f, 1.f, 1.f, Group->GroupColor, InterpEd->BarGradText->Resource );
				}

				// Draw line under group header
				DrawTile(Canvas, 0, GroupHeadHeight-1, ViewX, 1, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );

				Canvas->PopTransform();
			}

			// Advance vertical position passed group row
			YOffset += GroupHeadHeight;

			if(!Group->bCollapsed)
			{
				// Draw each track in this group.
				for(INT CurTrackIndex=0; CurTrackIndex<Group->InterpTracks.Num(); CurTrackIndex++)
				{
					UInterpTrack* Track = Group->InterpTracks(CurTrackIndex);
					INT TotalHeightAdded = 0;
					// Is this track visible?  It might be filtered out
					if( Track->bVisible )
					{
						UBOOL bTrackSelected = Track->bIsSelected;

						// Setup additional draw parameters
						TrackDrawParams.TrackIndex = CurTrackIndex;

						LabelDrawParams.IndentPixels = IndentPixels;
						LabelDrawParams.YOffset = YOffset;
						LabelDrawParams.GroupLabelColor = GroupLabelColor;
						LabelDrawParams.bTrackSelected = bTrackSelected;
						LabelDrawParams.TrackAreaHeight = TrackAreaHeight;

						Canvas->PushRelativeTransform(FTranslationMatrix(FVector(InterpEd->LabelWidth,LabelDrawParams.YOffset,0)));
						TotalHeightAdded = DrawTrack( Canvas, Track, Group, TrackDrawParams, LabelDrawParams );
						Canvas->PopTransform();
						
						// Advance vertical position
						YOffset += TotalHeightAdded;
					}
				}
			}
			else
			{
				if( bIsGroupWithinScrollArea )
				{
					const FVector2D TickSize(2.0f, GroupHeadHeight * 0.5f);

					// We'll iterate not only over ourself, but also all of our child groups
					for( INT CollapsedGroupIndex = CurGroupIndex; CollapsedGroupIndex < InterpEd->IData->InterpGroups.Num(); ++CollapsedGroupIndex )
					{
						UInterpGroup* CurCollapsedGroup = InterpEd->IData->InterpGroups( CollapsedGroupIndex );

						// We're interested either in ourselves or any of our children
						if( CurCollapsedGroup == Group || CurCollapsedGroup->bIsParented )
						{
							// Since the track is collapsed, draw ticks for each of the track's keyframes.
							for(INT TrackIdx=0; TrackIdx<CurCollapsedGroup->InterpTracks.Num(); TrackIdx++)
							{
								UInterpTrack* Track = CurCollapsedGroup->InterpTracks(TrackIdx);

								FVector2D TrackPos(InterpEd->LabelWidth - InterpEd->ViewStartTime * InterpEd->PixelsPerSec, YOffset - GroupHeadHeight);

								Canvas->PushRelativeTransform(FTranslationMatrix(FVector(TrackPos.X,TrackPos.Y,0)));
								DrawCollapsedTrackKeys( Canvas, Track, TrackPos, TickSize );
								Canvas->PopTransform();
							}
						}
						else
						{
							// Not really a child, but instead another root group.  We're done!
							break;
						}
					}
				}
			}
		}

		// If the current group is not parented, then it becomes our current parent group
		if( !Group->bIsParented )
		{
			CurParentGroup = Group;
		}
	}


	if( bWantTimeline )
	{
		// Draw grid and timeline stuff.
		DrawTimeline(Viewport, Canvas);
	}

	// Draw line between title block and track view for empty space
	DrawTile(Canvas, InterpEd->LabelWidth, YOffset-1, 1, ViewY - YOffset, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );

	// Draw snap-to line, if mouse button is down.
	UBOOL bMouseDownInAnyViewport =
		InterpEd->TrackWindow->InterpEdVC->bMouseDown ||
		InterpEd->DirectorTrackWindow->InterpEdVC->bMouseDown;
	if(bMouseDownInAnyViewport && InterpEd->bDrawSnappingLine)
	{
		INT SnapPosX = InterpEd->LabelWidth + (InterpEd->SnappingLinePosition - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
		DrawLine2D(Canvas, FVector2D(SnapPosX, 0), FVector2D(SnapPosX, TrackAreaHeight), FColor(0,0,0));
	}
	else
	{
		InterpEd->bDrawSnappingLine = false;
	}

	// Draw vertical position line
	INT TrackPosX = InterpEd->LabelWidth + (InterpEd->Interp->Position - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
	if( TrackPosX >= InterpEd->LabelWidth && TrackPosX <= ViewX)
	{
		DrawLine2D(Canvas, FVector2D(TrackPosX, 0), FVector2D(TrackPosX, TrackAreaHeight), InterpEd->PosMarkerColor);
	}

	// Draw the box select box
	if(bBoxSelecting)
	{
		INT MinX = Min(BoxStartX, BoxEndX);
		INT MinY = Min(BoxStartY, BoxEndY);
		INT MaxX = Max(BoxStartX, BoxEndX);
		INT MaxY = Max(BoxStartY, BoxEndY);

		DrawLine2D(Canvas,FVector2D(MinX, MinY), FVector2D(MaxX, MinY), FColor(255,0,0));
		DrawLine2D(Canvas,FVector2D(MaxX, MinY), FVector2D(MaxX, MaxY), FColor(255,0,0));
		DrawLine2D(Canvas,FVector2D(MaxX, MaxY), FVector2D(MinX, MaxY), FColor(255,0,0));
		DrawLine2D(Canvas,FVector2D(MinX, MaxY), FVector2D(MinX, MinY), FColor(255,0,0));
	}

	if( bWantFilterTabs )
	{
		// Draw filter tabs
		DrawTabs(Viewport, Canvas);
	}

	Canvas->PopTransform();
}

/** 
 * Draws keyframes for all subtracks in a subgroup.  The keyframes are drawn directly on the group.
 * @param Canvas		Canvas to draw on
 * @param SubGroupOwner		Track that owns the subgroup 			
 * @param GroupIndex		Index of a subgroup to draw
 * @param DrawInfos		An array of draw information for each keyframe that needs to be drawn
 * @param TrackPos		Starting position where the keyframes should be drawn.  
 * @param KeySize		Draw size of each keyframe
 */
void FInterpEdViewportClient::DrawSubTrackGroupKeys( FCanvas* Canvas, UInterpTrack* SubGroupOwner, INT GroupIndex, const TArray<FKeyframeDrawInfo>& KeyDrawInfos, const FVector2D& TrackPos, const FVector2D& KeySize )
{
	UInterpGroup* TrackGroup = SubGroupOwner->GetOwningGroup();
	// Determine if we are drawing on something that is collapsed.  INDEX_NONE for GroupIndex indicates we are drawing on the parent track.
	UBOOL bDrawCollapsed = GroupIndex == INDEX_NONE ? SubGroupOwner->bIsCollapsed : SubGroupOwner->SubTrackGroups( GroupIndex ).bIsCollapsed;
	// If the track is collapsed draw each keyframe with no transparency.  If the track is not collapsed, blend each keyframe with the background
	// This reduces clutter when there are lots of keyframes
	const BYTE Alpha = bDrawCollapsed ? 255 : 85;

	// Draw each keyframe
	for( INT DrawInfoIndex = 0; DrawInfoIndex < KeyDrawInfos.Num(); ++DrawInfoIndex )
	{
		const FKeyframeDrawInfo& DrawInfo = KeyDrawInfos( DrawInfoIndex );
		const FVector2D& KeyPos = DrawInfo.KeyPos;
	
		// Draw a tick mark.
		if(KeyPos.X >= InterpEd->ViewStartTime * InterpEd->PixelsPerSec)
		{
			FColor KeyColor = DrawInfo.KeyColor;
			KeyColor.A = Alpha;
			FColor SelectedColor = FColor(255,128,0,Alpha);
			if(Canvas->IsHitTesting()) 
			{
				Canvas->SetHitProxy( new HInterpTrackSubGroupKeypointProxy(SubGroupOwner, DrawInfo.KeyTime, GroupIndex ) );
			}
			if( bDrawCollapsed )
			{
				// If the group is collapsed draw each keyframe as a triangle
				const INT KeyHalfTriSize = 6;
				const INT KeyVertOffset = 3;
				INT PixelPos = appTrunc(DrawInfo.KeyTime * InterpEd->PixelsPerSec);

				FIntPoint A(PixelPos - KeyHalfTriSize,	TrackHeight - KeyVertOffset);
				FIntPoint B(PixelPos + KeyHalfTriSize,	TrackHeight - KeyVertOffset);
				FIntPoint C(PixelPos,					TrackHeight - KeyVertOffset - KeyHalfTriSize);
				
				if(DrawInfo.bSelected)
				{
					DrawTriangle2D(Canvas, A+FIntPoint(-2,1), FVector2D(0,0), B+FIntPoint(2,1), FVector2D(0,0), C+FIntPoint(0,-2), FVector2D(0,0), SelectedColor );
				}

				DrawTriangle2D(Canvas, A, FVector2D(0,0), B, FVector2D(0,0), C, FVector2D(0,0), KeyColor );
				if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
			}
			else
			{
				// Draw each keyframe is a vertical bar if the group is not collapsed
				if( DrawInfo.bSelected )
				{
					DrawTile(Canvas, KeyPos.X-1,KeyPos.Y-1, KeySize.X+2, KeySize.Y+2, 0.f, 0.f, 1.f, 1.f, SelectedColor );
				}
				DrawTile(Canvas, KeyPos.X, KeyPos.Y, KeySize.X, KeySize.Y, 0.f, 0.f, 1.f, 1.f, KeyColor );
			}
 			
			if( Canvas->IsHitTesting() )
			{
				Canvas->SetHitProxy( NULL );
			}
		}
	}
}

void FInterpEdViewportClient::DrawCollapsedTrackKeys( FCanvas* Canvas, UInterpTrack* Track, const FVector2D& TrackPos, const FVector2D& TickSize )
{
	for(INT KeyframeIdx=0; KeyframeIdx<Track->GetNumKeyframes(); KeyframeIdx++)
	{
		FLOAT KeyframeTime = Track->GetKeyframeTime(KeyframeIdx);
		FColor KeyframeColor = Track->GetKeyframeColor(KeyframeIdx);
		FVector2D TickPos;

		TickPos.X =  -TickSize.X / 2.0f + KeyframeTime * InterpEd->PixelsPerSec;
		TickPos.Y =  (GroupHeadHeight - 1 - TickSize.Y) / 2.0f;

		// Draw a tick mark.
		if(TickPos.X >= InterpEd->ViewStartTime * InterpEd->PixelsPerSec)
		{
			DrawTile(Canvas, TickPos.X, TickPos.Y, TickSize.X, TickSize.Y, 0.f, 0.f, 1.f, 1.f, KeyframeColor );
		}
	}

	for( INT SubTrackIndex = 0; SubTrackIndex < Track->SubTracks.Num(); ++SubTrackIndex )
	{
		DrawCollapsedTrackKeys( Canvas, Track->SubTracks( SubTrackIndex ), TrackPos, TickSize );
	}
}
/**
 * Draws filter tabs for the matinee editor.
 */
void FInterpEdViewportClient::DrawTabs(FViewport* Viewport,FCanvas* Canvas)
{
	// Draw tab background
	DrawTile(Canvas, 0,0, Viewport->GetSizeX(), TabRowHeight, 0,0,1,1, FColor(64,64,64));
	
	// Draw all tabs
	INT TabOffset = TabSpacing;

	// Draw static filters first
	for(INT TabIdx=0; TabIdx<InterpEd->IData->DefaultFilters.Num(); TabIdx++)
	{
		UInterpFilter* Filter = InterpEd->IData->DefaultFilters(TabIdx);
		FVector2D TabSize = DrawTab(Viewport, Canvas, TabOffset, Filter);
		TabOffset += TabSize.X + TabSpacing;
	}

	// Draw user custom filters last.
	for(INT TabIdx=0; TabIdx<InterpEd->IData->InterpFilters.Num(); TabIdx++)
	{
		UInterpFilter* Filter = InterpEd->IData->InterpFilters(TabIdx);
		FVector2D TabSize = DrawTab(Viewport, Canvas, TabOffset, Filter);
		TabOffset += TabSize.X + TabSpacing;
	}
}

/**
 * Draws a filter tab for matinee.
 *
 * @return Size of the tab that was drawn.
 */
FVector2D FInterpEdViewportClient::DrawTab(FViewport* Viewport, FCanvas* Canvas, INT &TabOffset, UInterpFilter* Filter)
{
	FColor TabColor = (Filter == InterpEd->IData->SelectedFilter) ? TabColorSelected : TabColorNormal;
	FVector2D TabPosition(TabOffset,TabSpacing);
	FVector2D TabSize(0,16);

	FIntPoint TabCaptionSize;
	StringSize(GEditor->SmallFont, TabCaptionSize.X, TabCaptionSize.Y, *Filter->Caption);
	TabSize.X = TabCaptionSize.X + 2 + TabPadding*2;
	TabSize.Y = TabCaptionSize.Y + 2 + TabPadding*2;

	Canvas->PushRelativeTransform(FTranslationMatrix(FVector(TabPosition.X, TabPosition.Y,0)));
	{
		Canvas->SetHitProxy(new HInterpEdTab(Filter));
		{
			DrawTile(Canvas, 0, 0, TabSize.X, TabSize.Y, 0, 0, 1, 1, TabColor);
			DrawLine2D(Canvas, FVector2D(0, 0), FVector2D(0, TabSize.Y), FColor(192,192,192));				// Left
			DrawLine2D(Canvas, FVector2D(0, 0), FVector2D(TabSize.X, 0), FColor(192,192,192));				// Top
			DrawLine2D(Canvas, FVector2D(TabSize.X, 0), FVector2D(TabSize.X, TabSize.Y), FColor(0,0,0));	// Right	
			DrawLine2D(Canvas, FVector2D(0, TabSize.Y), FVector2D(TabSize.X, TabSize.Y), FColor(0,0,0));	// Bottom
			DrawString(Canvas, TabPadding, TabPadding, *Filter->Caption, GEditor->SmallFont, FColor(0,0,0));
		}
		Canvas->SetHitProxy(NULL);
	}
	Canvas->PopTransform();

	return TabSize;
}



/**
 * Selects a color for the specified group (bound to the given group actor)
 *
 * @param Group The group to select a label color for
 * @param GroupActorOrNull The actor currently bound to the specified, or NULL if none is bounds
 *
 * @return The color to use to draw the group label
 */
FColor FInterpEdViewportClient::ChooseLabelColorForGroupActor( UInterpGroup* Group, AActor* GroupActorOrNull ) const
{
	check( Group != NULL );

	FColor GroupLabelColor = InterpEdGlobals::DefaultGroupLabelColor;

	if( Group->IsA( UInterpGroupDirector::StaticClass() ) )
	{
		GroupLabelColor = InterpEdGlobals::DirGroupLabelColor;
	}
	else if( Group->bIsFolder )
	{
		GroupLabelColor = InterpEdGlobals::FolderLabelColor;
	}
	else if( GroupActorOrNull != NULL )
	{
		AActor* GroupActor = GroupActorOrNull;

		if( GroupActor->IsA( ACameraActor::StaticClass() ) )
		{
			// Camera actor
			GroupLabelColor = FColor( 130, 130, 150 );
		}
		else if( GroupActor->IsA( ASkeletalMeshActor::StaticClass() ) )
		{
			// Skeletal mesh actor
			GroupLabelColor = FColor( 130, 150, 130 );
		}
		else if( GroupActor->IsA( AStaticMeshActor::StaticClass() ) )
		{
			// Static mesh actor
			GroupLabelColor = FColor( 150, 130, 130 );
		}
		else if( GroupActor->IsA( ABrush::StaticClass() ) )
		{
			// Brush actor
			GroupLabelColor = FColor( 130, 145, 145 );
		}
		else if( GroupActor->IsA( ALight::StaticClass() ) )
		{
			// Light actor
			GroupLabelColor = FColor( 145, 145, 130 );
		}
		else if( GroupActor->IsA( AMaterialInstanceActor::StaticClass() ) )
		{
			// Material instance actor
			GroupLabelColor = FColor( 145, 130, 145 );
		}
		else if( GroupActor->IsA( AEmitter::StaticClass() ) )
		{
			// Emitter
			GroupLabelColor = FColor( 115, 95, 150 );
		}
		else if( GroupActor->IsA( AInterpActor::StaticClass() ) )
		{
			// Interp actor
			GroupLabelColor = FColor( 95, 150, 115 );
		}
		else
		{
			// Unrecognized actor type
		}
	}

	return GroupLabelColor;
}
