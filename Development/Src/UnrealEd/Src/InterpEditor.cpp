/*=============================================================================
	InterpEditor.cpp: Interpolation editing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "CurveEd.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "InterpEditor.h"
#include "UnLinkedObjDrawUtils.h"
#include "PropertyWindow.h"
#include "Kismet.h"
#include "CameraController.h"
#include "UnConsoleSupportContainer.h"
#include "UnSubtitleManager.h"

#if WITH_MANAGED_CODE
#include "MatineeDirectorWindowShared.h"
#include "MatineeRecordWindowShared.h"
#endif

IMPLEMENT_CLASS(UInterpEdOptions);


static const FLOAT InterpEditor_ZoomIncrement = 1.2f;

static const FColor PositionMarkerLineColor(255, 222, 206);
static const FColor LoopRegionFillColor(80,255,80,24);
static const FColor Track3DSelectedColor(255,255,0);

/*-----------------------------------------------------------------------------
	UInterpEdTransBuffer / FInterpEdTransaction
-----------------------------------------------------------------------------*/

void UInterpEdTransBuffer::BeginSpecial(const TCHAR* SessionName)
{
	CheckState();
	if( ActiveCount++==0 )
	{
		// Cancel redo buffer.
		//debugf(TEXT("BeginTrans %s"), SessionName);
		if( UndoCount )
		{
			UndoBuffer.Remove( UndoBuffer.Num()-UndoCount, UndoCount );
		}

		UndoCount = 0;

		// Purge previous transactions if too much data occupied.
		while( GetUndoSize() > MaxMemory )
		{
			UndoBuffer.Remove( 0 );
		}

		// Begin a new transaction.
		GUndo = new(UndoBuffer)FInterpEdTransaction( SessionName, 1 );
	}
	CheckState();
}

void UInterpEdTransBuffer::EndSpecial()
{
	CheckState();
	check(ActiveCount>=1);
	if( --ActiveCount==0 )
	{
		GUndo = NULL;
	}
	CheckState();
}

void FInterpEdTransaction::SaveObject( UObject* Object )
{
	check(Object);

	if( Object->IsA( USeqAct_Interp::StaticClass() ) ||
		Object->IsA( UInterpData::StaticClass() ) ||
		Object->IsA( UInterpGroup::StaticClass() ) ||
		Object->IsA( UInterpTrack::StaticClass() ) ||
		Object->IsA( UInterpGroupInst::StaticClass() ) ||
		Object->IsA( UInterpTrackInst::StaticClass() ) ||
		Object->IsA( UInterpEdOptions::StaticClass() ) )
	{
		// Save the object.
		new( Records )FObjectRecord( this, Object, NULL, 0, 0, 0, 0, NULL, NULL );
	}
}

void FInterpEdTransaction::SaveArray( UObject* Object, FScriptArray* Array, INT Index, INT Count, INT Oper, INT ElementSize, STRUCT_AR Serializer, STRUCT_DTOR Destructor )
{
	// Never want this.
}

IMPLEMENT_CLASS(UInterpEdTransBuffer);

/*-----------------------------------------------------------------------------
 FInterpEdViewportClient
-----------------------------------------------------------------------------*/

FInterpEdViewportClient::FInterpEdViewportClient( class WxInterpEd* InInterpEd )
{
	InterpEd = InInterpEd;

	// This window will be 2D/canvas only, so set the viewport type to None
	ViewportType = LVT_None;

	// Set defaults for members.  These should be initialized by the owner after construction.
	bIsDirectorTrackWindow = FALSE;
	bWantFilterTabs = FALSE;
	bWantTimeline = FALSE;

	// Scroll bar starts at the top of the list!
	ThumbPos_Vert = 0;

	OldMouseX = 0;
	OldMouseY = 0;

	DistanceDragged = 0;

	BoxStartX = 0;
	BoxStartY = 0;
	BoxEndX = 0;
	BoxEndY = 0;

	bPanning = FALSE;
	bMouseDown = FALSE;
	bGrabbingHandle = FALSE;
	bBoxSelecting = FALSE;
	bTransactionBegun = FALSE;
	bNavigating = FALSE;
	bGrabbingMarker	= FALSE;

	DragObject = NULL;

	PressedCoordinates = FVector2D(0.0f, 0.0f);

	SetRealtime( FALSE );
}

FInterpEdViewportClient::~FInterpEdViewportClient()
{

}

void FInterpEdViewportClient::AddKeysFromHitProxy( const HHitProxy* HitProxy, TArray<FInterpEdSelKey>& Selections ) const
{
	// Find how much (in time) 1.5 pixels represents on the screen.
	const FLOAT PixelTime = 1.5f/InterpEd->PixelsPerSec;

	if( HitProxy )
	{
		if( HitProxy->IsA( HInterpTrackSubGroupKeypointProxy::StaticGetType() ) )
		{
			HInterpTrackSubGroupKeypointProxy* KeyProxy = ( ( HInterpTrackSubGroupKeypointProxy* )HitProxy );
			FLOAT KeyTime = KeyProxy->KeyTime;
			UInterpTrack* Track = KeyProxy->Track;
			UInterpGroup* Group = Track->GetOwningGroup();
			INT GroupIndex = KeyProxy->GroupIndex;

			// add all keyframes at the specified time
			if( Track->SubTracks.Num() > 0 )
			{
				if( GroupIndex == INDEX_NONE )
				{
					// The keyframe was drawn on the parent track, add all keyframes in all groups at the specified time
					for( INT SubGroupIndex = 0; SubGroupIndex < Track->SubTrackGroups.Num(); ++SubGroupIndex )
					{
						FSubTrackGroup& SubTrackGroup = Track->SubTrackGroups( SubGroupIndex );
						for( INT TrackIndex = 0; TrackIndex < SubTrackGroup.TrackIndices.Num(); ++TrackIndex )
						{
							UInterpTrack* SubTrack = Track->SubTracks( SubTrackGroup.TrackIndices( TrackIndex ) );

							// Get the keyframe index from the specified time.  We cant directly store the index as each subtrack may have a keyframe with the same time at a different index
							INT KeyIndex = SubTrack->GetKeyframeIndex( KeyTime );
							if( KeyIndex != INDEX_NONE )
							{
								Selections.AddUniqueItem( FInterpEdSelKey(Group, SubTrack , KeyIndex) );
							}
						}
					}
				}
				else
				{
					// The keyframe was drawn on a sub track group, select all keyframes in that group's tracks at the specified time.
					FSubTrackGroup& SubTrackGroup = Track->SubTrackGroups( GroupIndex );
					for( INT TrackIndex = 0; TrackIndex < SubTrackGroup.TrackIndices.Num(); ++TrackIndex )
					{
						UInterpTrack* SubTrack = Track->SubTracks( SubTrackGroup.TrackIndices( TrackIndex ) );
						// Get the keyframe index from the specified time.  We cant directly store the index as each subtrack may have a keyframe with the same time at a different index
						INT KeyIndex = SubTrack->GetKeyframeIndex( KeyTime );
						if( KeyIndex != INDEX_NONE )
						{
							Selections.AddUniqueItem( FInterpEdSelKey(Group, SubTrack , KeyIndex) );
						}
					}
				}

			}
		}
		else if( HitProxy->IsA( HInterpTrackKeypointProxy::StaticGetType() ) )
		{
			HInterpTrackKeypointProxy* KeyProxy = ( ( HInterpTrackKeypointProxy* )HitProxy );
			UInterpGroup* Group = KeyProxy->Group;
			UInterpTrack* Track = KeyProxy->Track;
			const INT KeyIndex = KeyProxy->KeyIndex;

			// Because AddKeyToSelection might invalidate the display, we just remember all the keys here and process them together afterwards.
			Selections.AddUniqueItem( FInterpEdSelKey(Group, Track , KeyIndex) );

			// Slight hack here. We select any other keys on the same track which are within 1.5 pixels of this one.
			const FLOAT SelKeyTime = Track->GetKeyframeTime(KeyIndex);

			for(INT i=0; i<Track->GetNumKeyframes(); i++)
			{
				const FLOAT KeyTime = Track->GetKeyframeTime(i);
				if( Abs(KeyTime - SelKeyTime) < PixelTime )
				{
					Selections.AddUniqueItem( FInterpEdSelKey(Group, Track, i) );
				}
			}
		}
	}
}

UBOOL FInterpEdViewportClient::InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT /*AmountDepressed*/,UBOOL /*Gamepad*/)
{
	const UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	const UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	const UBOOL bAltDown = Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt);

	const INT HitX = Viewport->GetMouseX();
	const INT HitY = Viewport->GetMouseY();

	const HHitProxy* HitResult = Viewport->GetHitProxy(HitX,HitY);

	UBOOL bClickedTrackViewport = FALSE;

	if( Key == KEY_LeftMouseButton )
	{
		switch( Event )
		{
		case IE_Pressed:
			{
				PressedCoordinates = FVector2D(HitX, HitY);

				if(DragObject == NULL)
				{
					if(HitResult)
					{
						if(HitResult->IsA(HInterpEdGroupTitle::StaticGetType()))
						{
							UInterpGroup* Group = ((HInterpTrackKeypointProxy*)HitResult)->Group;

							if( bCtrlDown && !InterpEd->HasATrackSelected() )
							{ 
								if( InterpEd->IsGroupSelected(Group) )
								{
									// Deselect a selected group when ctrl + clicking it.
									InterpEd->DeselectGroup(Group);
								}
								else
								{
									// Otherwise, just select the group, but don't deselect other selected tracks.
									InterpEd->SelectGroup(Group, FALSE);
								}
							}
							else
							{
								// Since ctrl is not down, select this group only. 
								InterpEd->SelectGroup(Group, TRUE);
							}
						}
						else if(HitResult->IsA(HInterpEdGroupCollapseBtn::StaticGetType()))
						{
							UInterpGroup* Group = ((HInterpEdGroupCollapseBtn*)HitResult)->Group;

							// Deselect existing selected groups only if ctrl is not down. 
							InterpEd->SelectGroup(Group, !bCtrlDown);
							Group->bCollapsed = !Group->bCollapsed;

							// A group has been expanded or collapsed, so we need to update our scroll bar
							InterpEd->UpdateTrackWindowScrollBars();
						}
						else if(HitResult->IsA(HInterpEdTrackCollapseBtn::StaticGetType()))
						{
							// A collapse widget on a track with subtracks was hit
							HInterpEdTrackCollapseBtn* Proxy = ((HInterpEdTrackCollapseBtn*)HitResult);
							UInterpTrack* Track = Proxy->Track;
							// GroupIndex indicates what sub group to collapse.  INDEX_NONE means collapse the whole track
							INT GroupIndex = Proxy->SubTrackGroupIndex;
							
							if( GroupIndex != INDEX_NONE )
							{
								// A subgroup was collapsed
								FSubTrackGroup& Group = Track->SubTrackGroups( GroupIndex );
								Group.bIsCollapsed = !Group.bIsCollapsed;
							}
							else
							{
								// A track was collapsed
								InterpEd->SelectTrack( Track->GetOwningGroup(), Track, !bCtrlDown );
								Track->bIsCollapsed = !Track->bIsCollapsed;
							}

							// Recompute the track window scroll bar position.
							InterpEd->UpdateTrackWindowScrollBars();
						}
						else if(HitResult->IsA(HInterpEdGroupLockCamBtn::StaticGetType()))
						{
							UInterpGroup* Group = ((HInterpEdGroupLockCamBtn*)HitResult)->Group;

							if(Group == InterpEd->CamViewGroup)
							{
								InterpEd->LockCamToGroup(NULL);
							}
							else
							{
								InterpEd->LockCamToGroup(Group);
							}
						}
						else if(HitResult->IsA(HInterpEdTrackTitle::StaticGetType()))
						{
							HInterpEdTrackTitle* HitProxy = (HInterpEdTrackTitle*)HitResult;
							UInterpGroup* Group = HitProxy->Group;
							UInterpTrack* TrackToSelect = HitProxy->Track;
							check( TrackToSelect );

							if( bCtrlDown && !InterpEd->HasAGroupSelected() )
							{
								if( TrackToSelect->bIsSelected )
								{
									// Deselect a selected track when ctrl + clicking it. 
									InterpEd->DeselectTrack( Group, TrackToSelect );
								}
								else
								{
									// Otherwise, select the track, but don't deselect any other selected tracks.
									InterpEd->SelectTrack( Group, TrackToSelect, FALSE);
								}
							}
							else
							{
								// Since ctrl is not down, just select this track only.
								InterpEd->SelectTrack( Group, TrackToSelect, TRUE);
							}
							
						}
						else if( HitResult->IsA(HInterpEdSubGroupTitle::StaticGetType()) )
						{
							// A track sub group was hit
							HInterpEdSubGroupTitle* HitProxy = ( (HInterpEdSubGroupTitle*)HitResult );
							UInterpTrack* Track = HitProxy->Track;
							UInterpGroup* TrackGroup = Track->GetOwningGroup();
							INT SubGroupIndex = HitProxy->SubGroupIndex;

							if( bCtrlDown && !InterpEd->HasAGroupSelected() )
							{
								// Get the sub group from the hit proxy index
								FSubTrackGroup& SubGroup = Track->SubTrackGroups( SubGroupIndex );
								if( SubGroup.bIsSelected )
								{
									// SubGroup was already selected, unselect it and all of the tracks in the group.
									SubGroup.bIsSelected = FALSE;
									for( INT TrackIndex = 0; TrackIndex < SubGroup.TrackIndices.Num(); ++TrackIndex )
									{
										InterpEd->DeselectTrack( TrackGroup, Track->SubTracks( SubGroup.TrackIndices( TrackIndex ) ) );
									}
								}
								else
								{
									// SubGroup was not selected, select it and all of the tracks in the group.
									SubGroup.bIsSelected = TRUE;
									for( INT TrackIndex = 0; TrackIndex < SubGroup.TrackIndices.Num(); ++TrackIndex )
									{
										InterpEd->SelectTrack( TrackGroup, Track->SubTracks( SubGroup.TrackIndices( TrackIndex ) ), FALSE );
									}
								}
							}
							else
							{
								// control is not down, we should empty our selection
								InterpEd->DeselectAllTracks();
								for( INT GroupIndex = 0; GroupIndex < Track->SubTrackGroups.Num(); ++GroupIndex )
								{
									FSubTrackGroup& SubGroup = Track->SubTrackGroups( GroupIndex );
									SubGroup.bIsSelected = FALSE;
								}

								// Now select the group and all of its tracks
								FSubTrackGroup& SubGroup = Track->SubTrackGroups( SubGroupIndex );
								SubGroup.bIsSelected = TRUE;
								for( INT TrackIndex = 0; TrackIndex < SubGroup.TrackIndices.Num(); ++TrackIndex )
								{
									InterpEd->SelectTrack( TrackGroup, Track->SubTracks( SubGroup.TrackIndices( TrackIndex ) ), FALSE );
								}
							}
						}
						// Did the user select the space in the track viewport associated to a given track?
						else if( HitResult->IsA(HInterpEdTrackTimeline::StaticGetType()) )
						{
							// When the user first clicks in this space, Matinee should interpret this as if the 
							// user clicked on the empty space in the track viewport that doesn't belong to any 
							// track. This enables panning and box-selecting in addition to the ability to select a 
							// track just by clicking on the space in the track viewport associated to a given track. 
							bClickedTrackViewport = TRUE;
						}
						else if(HitResult->IsA(HInterpEdTrackTrajectoryButton::StaticGetType()))
						{
							UInterpGroup* Group = ((HInterpEdTrackTrajectoryButton*)HitResult)->Group;
							UInterpTrack* Track = ((HInterpEdTrackTrajectoryButton*)HitResult)->Track;

							// Should always be a movement track
							UInterpTrackMove* MovementTrack = Cast<UInterpTrackMove>( Track	);
							if( MovementTrack != NULL )
							{
								// Toggle the 3D trajectory for this track
								InterpEd->InterpEdTrans->BeginSpecial( *LocalizeUnrealEd( "InterpEd_Undo_ToggleTrajectory" ) );
								MovementTrack->Modify();
								MovementTrack->bHide3DTrack = !MovementTrack->bHide3DTrack;
								InterpEd->InterpEdTrans->EndSpecial();
							}
						}
						else if(HitResult->IsA(HInterpEdTrackGraphPropBtn::StaticGetType()))
						{
							UInterpGroup* Group = ((HInterpEdTrackGraphPropBtn*)HitResult)->Group;
							UInterpTrack* Track = ((HInterpEdTrackGraphPropBtn*)HitResult)->Track;

							// create an array of tracks that we're interested in - either subtracks or just the main track
							TArray<UInterpTrack*> TracksArray;
							INT SubTrackGroupIndex = ((HInterpEdTrackGraphPropBtn*)HitResult)->SubTrackGroupIndex;
							if( SubTrackGroupIndex != -1 )
							{
								FSubTrackGroup& SubTrackGroup = Track->SubTrackGroups( SubTrackGroupIndex );
								for( INT Index = 0; Index < SubTrackGroup.TrackIndices.Num(); ++Index )
								{
									INT SubTrackIndex = SubTrackGroup.TrackIndices( Index );
									TracksArray.AddItem( Track->SubTracks( SubTrackIndex ) );
								}
							}
							else
							{
								if( Track->SubTracks.Num() > 0 )
								{
									// If the track has subtracks, add all subtracks instead
									for( INT SubTrackIndex = 0; SubTrackIndex < Track->SubTracks.Num(); ++SubTrackIndex )
									{
										TracksArray.AddItem( Track->SubTracks( SubTrackIndex ) );
									}
								}
								else
								{
									TracksArray.AddItem( Track );
								}
							}

							// find out whether or not ALL of the tracks are currently shown in the curve editor
							// start by assuming that all are - then make this FALSE if any of them aren't
							UBOOL bAllSubtracksShown = TRUE;
							for( INT Index = 0; Index < TracksArray.Num(); ++Index )
							{
								if( !InterpEd->IData->CurveEdSetup->ShowingCurve( TracksArray( Index ) ) )
								{
									bAllSubtracksShown = FALSE;
									break;
								}
							}

							// toggle tracks ON if NONE or SOME of them are currently shown
							// toggle tracks OFF if ALL of them are currently shown
							UBOOL bToggleTracksOn = ( !bAllSubtracksShown );

							// add tracks to the curve editor
							for( INT Index = 0; Index < TracksArray.Num(); ++Index )
							{
								InterpEd->AddTrackToCurveEd( *Group->GroupName.ToString(), Group->GroupColor, TracksArray( Index ), bToggleTracksOn );
							}
						}
						else if(HitResult->IsA(HInterpEdEventDirBtn::StaticGetType()))
						{
							UInterpGroup* Group = ((HInterpEdEventDirBtn*)HitResult)->Group;
							const INT TrackIndex = ((HInterpEdEventDirBtn*)HitResult)->TrackIndex;
							EInterpEdEventDirection Dir = ((HInterpEdEventDirBtn*)HitResult)->Dir;

							UInterpTrackEvent* EventTrack = CastChecked<UInterpTrackEvent>( Group->InterpTracks(TrackIndex) );

							if(Dir == IED_Forward)
							{
								EventTrack->bFireEventsWhenForwards = !EventTrack->bFireEventsWhenForwards;
							}
							else
							{
								EventTrack->bFireEventsWhenBackwards = !EventTrack->bFireEventsWhenBackwards;
							}
						}
						else if(HitResult->IsA(HInterpEdTrackBkg::StaticGetType()))
						{
							InterpEd->DeselectAll();
						}
						else if(HitResult->IsA(HInterpEdTimelineBkg::StaticGetType()))
						{
							FLOAT NewTime = InterpEd->ViewStartTime + ((HitX - InterpEd->LabelWidth) / InterpEd->PixelsPerSec);
							if( InterpEd->bSnapToFrames && InterpEd->bSnapTimeToFrames )
							{
								NewTime = InterpEd->SnapTimeToNearestFrame( NewTime );
							}

							// When jumping to location by clicking, stop playback.
							InterpEd->Interp->Stop();
							SetRealtime( FALSE );
							InterpEd->SetAudioRealtimeOverride( FALSE );

							//make sure to turn off recording
							InterpEd->StopRecordingInterpValues();

							// Move to clicked on location
							InterpEd->SetInterpPosition(NewTime);

							// Act as if we grabbed the handle as well.
							bGrabbingHandle = TRUE;
						}
						else if(HitResult->IsA(HInterpEdNavigatorBackground::StaticGetType()))
						{
							// Clicked on the navigator background, so jump directly to the position under the
							// mouse cursor and wait for a drag
							const FLOAT JumpToTime = ((HitX - InterpEd->LabelWidth)/InterpEd->NavPixelsPerSecond);
							const FLOAT ViewWindow = (InterpEd->ViewEndTime - InterpEd->ViewStartTime);

							InterpEd->ViewStartTime = JumpToTime - (0.5f * ViewWindow);
							InterpEd->ViewEndTime = JumpToTime + (0.5f * ViewWindow);
							InterpEd->SyncCurveEdView();

							bNavigating = TRUE;
						}
						else if(HitResult->IsA(HInterpEdNavigator::StaticGetType()))
						{
							// Clicked on the navigator foreground, so just start the drag immediately without
							// jumping the timeline
							bNavigating = TRUE;
						}
						else if(HitResult->IsA(HInterpEdMarker::StaticGetType()))
						{
							InterpEd->GrabbedMarkerType = ((HInterpEdMarker*)HitResult)->Type;

							InterpEd->BeginMoveMarker();
							bGrabbingMarker = TRUE;
						}
						else if(HitResult->IsA(HInterpEdTab::StaticGetType()))
						{
							InterpEd->SetSelectedFilter(((HInterpEdTab*)HitResult)->Filter);

							Viewport->Invalidate();	
						}
						else if(HitResult->IsA(HInterpEdTrackDisableTrackBtn::StaticGetType()))
						{
							HInterpEdTrackDisableTrackBtn* TrackProxy = ((HInterpEdTrackDisableTrackBtn*)HitResult);

							if(TrackProxy->Group != NULL && TrackProxy->Track != NULL )
							{
								UInterpTrack* Track = TrackProxy->Track;

								InterpEd->InterpEdTrans->BeginSpecial( *LocalizeUnrealEd( "InterpEd_Undo_ToggleTrackEnabled" ) );

								//if only one per group is allowed to be active (And this is ABOUT to become active), ensure that no other is active
								if (Track->bOnePerGroup && (Track->IsDisabled() == TRUE))
								{
									InterpEd->DisableTracksOfClass(TrackProxy->Group, Track->GetClass());
								}

								Track->Modify();
								Track->EnableTrack( Track->IsDisabled() ? TRUE : FALSE, TRUE );

								InterpEd->InterpEdTrans->EndSpecial();

								// Update the preview and actor states
								InterpEd->Interp->RecaptureActorState();
								
							}
						}
						else if(HitResult->IsA(HInterpEdInputInterface::StaticGetType()))
						{
							HInterpEdInputInterface* Proxy = ((HInterpEdInputInterface*)HitResult);

							DragObject = Proxy->ClickedObject;
							DragData = Proxy->InputData;
							DragData.PixelsPerSec = InterpEd->PixelsPerSec;
							DragData.MouseStart = FIntPoint(HitX, HitY);
							DragData.bCtrlDown = bCtrlDown;
							DragData.bAltDown = bAltDown;
							DragData.bShiftDown = bShiftDown;
							Proxy->ClickedObject->BeginDrag(DragData);
						}
					}
					else
					{
						// The user clicked on empty space that doesn't have a hit proxy associated to it.
						bClickedTrackViewport = TRUE;
					}

					// When the user clicks on empty, non-hit proxy space, allow 
					// features such as box-selection and panning of the viewport. 
					if( bClickedTrackViewport )
					{
						// Enable box selection if CTRL + ALT held down.
						if(bCtrlDown && bAltDown)
						{
							BoxStartX = BoxEndX = HitX;
							BoxStartY = BoxEndY = HitY;

							bBoxSelecting = TRUE;
						}
						// The last option is to simply pan the viewport
						else
						{
							bPanning = TRUE;
						}
					}

					Viewport->LockMouseToWindow(TRUE);

					bMouseDown = TRUE;
					OldMouseX = HitX;
					OldMouseY = HitY;
					DistanceDragged = 0;
				}
			}
			break;
		case IE_DoubleClick:
			{
				if(HitResult)
				{
					if(HitResult->IsA(HInterpEdGroupTitle::StaticGetType()))
					{
						UInterpGroup* Group = ((HInterpTrackKeypointProxy*)HitResult)->Group;

						Group->bCollapsed = !Group->bCollapsed;

						// A group has been expanded or collapsed, so we need to update our scroll bar
						InterpEd->UpdateTrackWindowScrollBars();
					}
				}
			}
			break;
		case IE_Released:
			{
				if(bBoxSelecting)
				{
					const INT MinX = Min(BoxStartX, BoxEndX);
					const INT MinY = Min(BoxStartY, BoxEndY);
					const INT MaxX = Max(BoxStartX, BoxEndX);
					const INT MaxY = Max(BoxStartY, BoxEndY);
					const INT TestSizeX = MaxX - MinX + 1;
					const INT TestSizeY = MaxY - MinY + 1;

					// Find how much (in time) 1.5 pixels represents on the screen.
					const FLOAT PixelTime = 1.5f/InterpEd->PixelsPerSec;

					// We read back the hit proxy map for the required region.
					TArray<HHitProxy*> ProxyMap;
					Viewport->GetHitProxyMap((UINT)MinX, (UINT)MinY, (UINT)MaxX, (UINT)MaxY, ProxyMap);

					TArray<FInterpEdSelKey>	NewSelection;

					if( ProxyMap.Num() > 0 )
					{
						// Find any keypoint hit proxies in the region - add the keypoint to selection.
						for( INT Y = 0; Y < TestSizeY; Y++ )
						{
							for( INT X = 0; X < TestSizeX; X++ )
							{
								AddKeysFromHitProxy( ProxyMap( Y * TestSizeX + X ), NewSelection );
							}
						}
					}

					// If the SHIFT key is down, then the user wants to preserve 
					// the current selection in Matinee during box selection.
					if(!bShiftDown)
					{
						// NOTE: This will clear all keys
						InterpEd->DeselectAllTracks();
					}

					for(INT i=0; i<NewSelection.Num(); i++)
					{
						UInterpGroup* Group = NewSelection(i).Group;
						UInterpTrack* TrackToSelect = NewSelection(i).Track;
						InterpEd->SelectTrack( Group, TrackToSelect, FALSE );
						InterpEd->AddKeyToSelection( Group, TrackToSelect, NewSelection(i).KeyIndex, FALSE );
					}
				}
				else if(DragObject)
				{
					if(HitResult)
					{
						if(HitResult->IsA(HInterpEdInputInterface::StaticGetType()))
						{
							HInterpEdInputInterface* Proxy = ((HInterpEdInputInterface*)HitResult);
							
							//@todo: Do dropping.
						}
					}

					DragData.PixelsPerSec = InterpEd->PixelsPerSec;
					DragData.MouseCurrent = FIntPoint(HitX, HitY);
					DragObject->EndDrag(DragData);
					DragObject = NULL;
				}
				else if (!bTransactionBegun)
				{
					// If mouse didn't really move since last time, and we released over empty space, deselect everything.
					if( !HitResult || abs(PressedCoordinates.X - HitX) > 4 || abs(PressedCoordinates.Y - HitY) > 4 )
					{
						InterpEd->ClearKeySelection();
					}
					// Allow track selection if the user selects the space in the track viewport that is associated to a given track.
					else if(HitResult->IsA(HInterpEdTrackTimeline::StaticGetType()))
					{
						UInterpGroup* Group = ((HInterpEdTrackTimeline*)HitResult)->Group;
						UInterpTrack* TrackToSelect = ((HInterpEdTrackTimeline*)HitResult)->Track;

						if( bCtrlDown && !InterpEd->HasAGroupSelected() )
						{
							if( TrackToSelect->bIsSelected )
							{
								// Deselect a selected track when ctrl + clicking it. 
								InterpEd->DeselectTrack( Group, TrackToSelect );
							}
							else
							{
								// Otherwise, select the track, but don't deselect any other selected tracks.
								InterpEd->SelectTrack(  Group, TrackToSelect, FALSE);
							}
						}
						else
						{
							// Always clear the key selection when single-clicking on the track viewport background. 
							// If the user is CTRL-clicking, we don't want to clear the key selection of other tracks.
							InterpEd->ClearKeySelection();

							// Since ctrl is not down, just select this track only.
							InterpEd->SelectTrack( Group, TrackToSelect, TRUE);
						}
					}
					else if(bCtrlDown && HitResult->IsA(HInterpTrackKeypointProxy::StaticGetType()))
					{
						HInterpTrackKeypointProxy* KeyProxy = ((HInterpTrackKeypointProxy*)HitResult);
						UInterpGroup* Group = KeyProxy->Group;
						UInterpTrack* Track = KeyProxy->Track;
						const INT KeyIndex = KeyProxy->KeyIndex;
	
						const UBOOL bAlreadySelected = InterpEd->KeyIsInSelection(Group, Track, KeyIndex);
						if(bAlreadySelected)
						{
							InterpEd->RemoveKeyFromSelection(Group, Track, KeyIndex);
						}
						else
						{
							// NOTE: Do not clear previously-selected tracks because ctrl is down. 
							InterpEd->SelectTrack( Group, Track, FALSE );
							InterpEd->AddKeyToSelection(Group, Track, KeyIndex, !bShiftDown);
						}
					}
					else if( ( HitResult->IsA( HInterpTrackKeypointProxy::StaticGetType() ) ) )
					{
						TArray<FInterpEdSelKey>	NewSelection;
						AddKeysFromHitProxy( HitResult, NewSelection );

						for( INT Index = 0; Index < NewSelection.Num(); Index++ )
						{
							FInterpEdSelKey& SelKey = NewSelection( Index );

							UInterpGroup* Group = SelKey.Group;
							UInterpTrack* Track = SelKey.Track;
							const INT KeyIndex = SelKey.KeyIndex;

							if(!bCtrlDown)
							{
								// NOTE: Clear previously-selected tracks because ctrl is not down. 
								InterpEd->SelectTrack( Group, Track );
								InterpEd->ClearKeySelection();
								InterpEd->AddKeyToSelection(Group, Track, KeyIndex, !bShiftDown);
							}
						}
					}
					else if( HitResult->IsA( HInterpTrackSubGroupKeypointProxy::StaticGetType() ) )
					{
						TArray<FInterpEdSelKey>	NewSelection;
						AddKeysFromHitProxy( HitResult, NewSelection );

						if( !bCtrlDown )
						{
							InterpEd->DeselectAllTracks();
							InterpEd->ClearKeySelection();
						}

						for( INT Index = 0; Index < NewSelection.Num(); Index++ )
						{
							FInterpEdSelKey& SelKey = NewSelection( Index );

							UInterpGroup* Group = SelKey.Group;
							UInterpTrack* Track = SelKey.Track;
							const INT KeyIndex = SelKey.KeyIndex;

							if( !InterpEd->KeyIsInSelection( Group, Track, KeyIndex ) )
							{
								InterpEd->SelectTrack( Group, Track, FALSE );
								InterpEd->AddKeyToSelection( Group, Track, KeyIndex, !bShiftDown );
							}
						}

						if(InterpEd->Opt->SelectedKeys.Num() > 1)
						{
							InterpEd->Opt->bAdjustingGroupKeyframes = TRUE;
						}
					}
				}

				if(bTransactionBegun)
				{
					InterpEd->EndMoveSelectedKeys();
					bTransactionBegun = FALSE;
				}

				if(bGrabbingMarker)
				{
					InterpEd->EndMoveMarker();
					bGrabbingMarker = FALSE;
				}

				Viewport->LockMouseToWindow(FALSE);

				DistanceDragged = 0;

				PressedCoordinates = FVector2D(0.0f, 0.0f);

				bPanning = FALSE;
				bMouseDown = FALSE;
				bGrabbingHandle = FALSE;
				bNavigating = FALSE;
				bBoxSelecting = FALSE;
			}
			break;
		}
	}
	else if( Key == KEY_RightMouseButton )
	{
		switch( Event )
		{
		case IE_Pressed:
			{
				const INT HitX = Viewport->GetMouseX();
				const INT HitY = Viewport->GetMouseY();
				HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);

				if(HitResult)
				{
					// User right-click somewhere in the track editor
					wxMenu* Menu = InterpEd->CreateContextMenu( Viewport, HitResult );
					if(Menu)
					{
						// Redraw the viewport so the user can see which object was right clicked on
						Viewport->Draw();
						FlushRenderingCommands();

						FTrackPopupMenu tpm( InterpEd, Menu );
						tpm.Show();
						delete Menu;
					}
				}
			}
			break;

		case IE_Released:
			{
				
			}
			break;
		}
	}
	
	if(Event == IE_Pressed)
	{
		if(Key == KEY_MouseScrollDown)
		{
			InterpEd->ZoomView( InterpEditor_ZoomIncrement, InterpEd->bZoomToScrubPos );
		}
		else if(Key == KEY_MouseScrollUp)
		{
			InterpEd->ZoomView( 1.0f / InterpEditor_ZoomIncrement, InterpEd->bZoomToScrubPos );
		}

		// Handle hotkey bindings.
		UUnrealEdOptions* UnrealEdOptions = GUnrealEd->GetUnrealEdOptions();

		if(UnrealEdOptions)
		{
			FString Cmd = UnrealEdOptions->GetExecCommand(Key, bAltDown, bCtrlDown, bShiftDown, TEXT("Matinee"));

			if(Cmd.Len())
			{
				Exec(*Cmd);
			}
		}
	}

	// Handle viewport screenshot.
	InputTakeScreenshot( Viewport, Key, Event );

	return TRUE;
}

// X and Y here are the new screen position of the cursor.
void FInterpEdViewportClient::MouseMove(FViewport* Viewport, INT X, INT Y)
{
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);

	INT DeltaX = OldMouseX - X;
	INT DeltaY = OldMouseY - Y;

	if(bMouseDown)
	{
		DistanceDragged += ( Abs<INT>(DeltaX) + Abs<INT>(DeltaY) );

		if(DragObject != NULL)
		{
			DragData.PixelsPerSec = InterpEd->PixelsPerSec;
			DragData.MouseCurrent = FIntPoint(X, Y);
			DragObject->ObjectDragged(DragData);
		}
		else if(bGrabbingHandle)
		{
			FLOAT NewTime = InterpEd->ViewStartTime + ((X - InterpEd->LabelWidth) / InterpEd->PixelsPerSec);
			if( InterpEd->bSnapToFrames && InterpEd->bSnapTimeToFrames )
			{
				NewTime = InterpEd->SnapTimeToNearestFrame( NewTime );
			}

			InterpEd->SetInterpPosition(NewTime);
		}
		else if(bBoxSelecting)
		{
			BoxEndX = X;
			BoxEndY = Y;
		}
		else if( bCtrlDown && InterpEd->Opt->SelectedKeys.Num() > 0 )
		{
			if(DistanceDragged > 4)
			{
				if(!bTransactionBegun)
				{
					InterpEd->BeginMoveSelectedKeys();
					bTransactionBegun = TRUE;
				}

				FLOAT DeltaTime = -DeltaX / InterpEd->PixelsPerSec;
				InterpEd->MoveSelectedKeys(DeltaTime);
			}
		}
		else if(bNavigating)
		{
			FLOAT DeltaTime = -DeltaX / InterpEd->NavPixelsPerSecond;
			InterpEd->ViewStartTime += DeltaTime;
			InterpEd->ViewEndTime += DeltaTime;
			InterpEd->SyncCurveEdView();
		}
		else if(bGrabbingMarker)
		{
			FLOAT DeltaTime = -DeltaX / InterpEd->PixelsPerSec;
			InterpEd->UnsnappedMarkerPos += DeltaTime;

			if(InterpEd->GrabbedMarkerType == ISM_SeqEnd)
			{
				InterpEd->SetInterpEnd( InterpEd->SnapTime(InterpEd->UnsnappedMarkerPos, FALSE) );
			}
			else if(InterpEd->GrabbedMarkerType == ISM_LoopStart || InterpEd->GrabbedMarkerType == ISM_LoopEnd)
			{
				InterpEd->MoveLoopMarker( InterpEd->SnapTime(InterpEd->UnsnappedMarkerPos, FALSE), InterpEd->GrabbedMarkerType == ISM_LoopStart );
			}
		}
		else if(bPanning)
		{
			const UBOOL bInvertPanning = GEditorModeTools().GetInterpPanInvert();

			FLOAT DeltaTime = (bInvertPanning ? -DeltaX : DeltaX) / InterpEd->PixelsPerSec;
			InterpEd->ViewStartTime -= DeltaTime;
			InterpEd->ViewEndTime -= DeltaTime;

			// Handle vertical scrolling if the user moved the mouse up or down.
			// Vertical scrolling is handled by modifying the scroll bar attributes
			// because the scroll bar drives the vertical position of the viewport. 
			if( DeltaY != 0 )
			{
				// Account for the 'Drag Moves Canvas' option, which determines if panning 
				// is inverted, when figuring out the desired destination thumb position.
				const INT TargetThumbPosition = bInvertPanning ? ThumbPos_Vert - DeltaY : ThumbPos_Vert + DeltaY;

				// Figure out which window we are panning
				WxInterpEdVCHolder* WindowToPan = bIsDirectorTrackWindow ? InterpEd->DirectorTrackWindow : InterpEd->TrackWindow;

				// Determine the maximum scroll position in order to prevent the user from scrolling beyond the 
				// valid scroll range. The max scroll position is not equivalent to the max range of the scroll 
				// bar. We must subtract the size of the thumb because the thumb position is the top of the thumb.
				INT MaxThumbPosition = WindowToPan->ScrollBar_Vert->GetRange() - WindowToPan->ScrollBar_Vert->GetThumbSize();

				// Make sure the max thumb position is not negative. This is possible if the amount 
				// of scrollable space is less than the scroll bar (i.e. no scrolling possible). 
				MaxThumbPosition = Max<INT>( 0, MaxThumbPosition );

				// For some reason, the thumb position is always negated. So, instead 
				// of clamping from zero - max, we clamp from -max to zero.  
				ThumbPos_Vert = Clamp<INT>( TargetThumbPosition, -MaxThumbPosition, 0 );

				// Redraw the scroll bar so that it is at its new position. 
				WindowToPan->AdjustScrollBar();
			}

			InterpEd->SyncCurveEdView();
		}
	}

	OldMouseX = X;
	OldMouseY = Y;
}

UBOOL FInterpEdViewportClient::InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad)
{
	if ( Key == KEY_MouseX || Key == KEY_MouseY )
	{
		INT X = Viewport->GetMouseX();
		INT Y = Viewport->GetMouseY();
		MouseMove(Viewport, X, Y);
		return TRUE;
	}
	return FALSE;
}

EMouseCursor FInterpEdViewportClient::GetCursor(FViewport* Viewport,INT X,INT Y)
{
	EMouseCursor Result = MC_Cross;

	if(DragObject==NULL)
	{
		HHitProxy*	HitProxy = Viewport->GetHitProxy(X,Y);
		

		if(HitProxy)
		{
			Result = HitProxy->GetMouseCursor();
		}
	}
	else
	{
		Result = MC_NoChange;
	}

	return Result;
}

void FInterpEdViewportClient::Tick(FLOAT DeltaSeconds)
{
	// Only the main track window is allowed to tick the root object.  We never want the InterpEd object to be
	// ticked more than once per frame.
	if( !bIsDirectorTrackWindow )
	{
		InterpEd->TickInterp(DeltaSeconds);
	}

	// If curve editor is shown - sync us with it.
	if(InterpEd->CurveEd->IsShown())
	{
		InterpEd->ViewStartTime = InterpEd->CurveEd->StartIn;
		InterpEd->ViewEndTime = InterpEd->CurveEd->EndIn;
	}

	if(bNavigating || bPanning)
	{
		const INT ScrollBorderSize = 20;
		const FLOAT	ScrollBorderSpeed = 500.f;
		const INT PosX = Viewport->GetMouseX();
		const INT PosY = Viewport->GetMouseY();
		const INT SizeX = Viewport->GetSizeX();
		const INT SizeY = Viewport->GetSizeY();

		FLOAT DeltaTime = Clamp(DeltaSeconds, 0.01f, 1.0f);

		if(PosX < ScrollBorderSize)
		{
			ScrollAccum.X += (1.f - ((FLOAT)PosX/(FLOAT)ScrollBorderSize)) * ScrollBorderSpeed * DeltaTime;
		}
		else if(PosX > SizeX - ScrollBorderSize)
		{
			ScrollAccum.X -= ((FLOAT)(PosX - (SizeX - ScrollBorderSize))/(FLOAT)ScrollBorderSize) * ScrollBorderSpeed * DeltaTime;
		}
		else
		{
			ScrollAccum.X = 0.f;
		}

		// Apply integer part of ScrollAccum to the curve editor view position.
		const INT DeltaX = appFloor(ScrollAccum.X);
		ScrollAccum.X -= DeltaX;


		DeltaTime = -DeltaX / InterpEd->NavPixelsPerSecond;
		InterpEd->ViewStartTime += DeltaTime;
		InterpEd->ViewEndTime += DeltaTime;

		InterpEd->SyncCurveEdView();
	}

	Viewport->Invalidate();
}


void FInterpEdViewportClient::Serialize(FArchive& Ar) 
{ 
	Ar << Input; 

	// Drag object may be a instance of UObject, so serialize it if it is.
	if(DragObject && DragObject->GetUObject())
	{
		UObject* DragUObject = DragObject->GetUObject();
		Ar << DragUObject;
	}
}

/** Exec handler */
void FInterpEdViewportClient::Exec(const TCHAR* Cmd)
{
	const TCHAR* Str = Cmd;

	if(ParseCommand(&Str, TEXT("MATINEE")))
	{
		if(ParseCommand(&Str, TEXT("Undo")))
		{
			InterpEd->InterpEdUndo();
		}
		else if(ParseCommand(&Str, TEXT("Redo")))
		{
			InterpEd->InterpEdRedo();
		}
		else if(ParseCommand(&Str, TEXT("Cut")))
		{
			InterpEd->CopySelectedGroupOrTrack(TRUE);
		}
		else if(ParseCommand(&Str, TEXT("Copy")))
		{
			InterpEd->CopySelectedGroupOrTrack(FALSE);
		}
		else if(ParseCommand(&Str, TEXT("Paste")))
		{
			InterpEd->PasteSelectedGroupOrTrack();
		}
		else if(ParseCommand(&Str, TEXT("Play")))
		{
			InterpEd->StartPlaying( FALSE, TRUE );
		}
		else if(ParseCommand(&Str, TEXT("PlayReverse")))
		{
			InterpEd->StartPlaying( FALSE, FALSE );
		}
		else if(ParseCommand(&Str, TEXT("Stop")))
		{
			if(InterpEd->Interp->bIsPlaying)
			{
				InterpEd->StopPlaying();
			}
		}
		else if(ParseCommand(&Str, TEXT("Rewind")))
		{
			InterpEd->SetInterpPosition(0.f);
		}
		else if(ParseCommand(&Str, TEXT("TogglePlayPause")))
		{
			if(InterpEd->Interp->bIsPlaying)
			{
				InterpEd->StopPlaying();
			}
			else
			{
				// Start playback and retain whatever direction we were already playing
				InterpEd->StartPlaying( FALSE, TRUE );
			}
		}
		else if( ParseCommand( &Str, TEXT( "ZoomIn" ) ) )
		{
			const UBOOL bZoomToTimeCursorPos = TRUE;
			InterpEd->ZoomView( 1.0f / InterpEditor_ZoomIncrement, bZoomToTimeCursorPos );
		}
		else if( ParseCommand( &Str, TEXT( "ZoomOut" ) ) )
		{
			const UBOOL bZoomToTimeCursorPos = TRUE;
			InterpEd->ZoomView( InterpEditor_ZoomIncrement, bZoomToTimeCursorPos );
		}
		else if(ParseCommand(&Str, TEXT("DeleteSelection")))
		{
			InterpEd->DeleteSelection();
		}
		else if(ParseCommand(&Str, TEXT("MarkInSection")))
		{
			InterpEd->MoveLoopMarker(InterpEd->Interp->Position, TRUE);
		}
		else if(ParseCommand(&Str, TEXT("MarkOutSection")))
		{
			InterpEd->MoveLoopMarker(InterpEd->Interp->Position, FALSE);
		}
		else if(ParseCommand(&Str, TEXT("CropAnimationBeginning")))
		{
			InterpEd->CropAnimKey(TRUE);
		}
		else if(ParseCommand(&Str, TEXT("CropAnimationEnd")))
		{
			InterpEd->CropAnimKey(FALSE);
		}
		else if(ParseCommand(&Str, TEXT("IncrementPosition")))
		{
			InterpEd->IncrementSelection();
		}
		else if(ParseCommand(&Str, TEXT("DecrementPosition")))
		{
			InterpEd->DecrementSelection();
		}
		else if(ParseCommand(&Str, TEXT("MoveToNextKey")))
		{
			InterpEd->SelectNextKey();
		}
		else if(ParseCommand(&Str, TEXT("MoveToPrevKey")))
		{
			InterpEd->SelectPreviousKey();
		}
		else if(ParseCommand(&Str, TEXT("SplitAnimKey")))
		{
			InterpEd->SplitAnimKey();
		}
		else if(ParseCommand(&Str, TEXT("ToggleSnap")))
		{
			InterpEd->SetSnapEnabled(!InterpEd->bSnapEnabled);
		}
		else if(ParseCommand(&Str, TEXT("ToggleSnapTimeToFrames")))
		{
			InterpEd->SetSnapTimeToFrames(!InterpEd->bSnapTimeToFrames);
		}
		else if(ParseCommand(&Str, TEXT("ToggleFixedTimeStepPlayback")))
		{
			InterpEd->SetFixedTimeStepPlayback( !InterpEd->bFixedTimeStepPlayback );
		}
		else if(ParseCommand(&Str, TEXT("TogglePreferFrameNumbers")))
		{
			InterpEd->SetPreferFrameNumbers( !InterpEd->bPreferFrameNumbers );
		}
		else if(ParseCommand(&Str, TEXT("ToggleShowTimeCursorPosForAllKeys")))
		{
			InterpEd->SetShowTimeCursorPosForAllKeys( !InterpEd->bShowTimeCursorPosForAllKeys );
		}
		else if(ParseCommand(&Str, TEXT("MoveActiveUp")))
		{
			InterpEd->MoveActiveUp();
		}
		else if(ParseCommand(&Str, TEXT("MoveActiveDown")))
		{
			InterpEd->MoveActiveDown();
		}
		else if(ParseCommand(&Str, TEXT("AddKey")))
		{
			InterpEd->AddKey();
		}
		else if(ParseCommand(&Str, TEXT("DuplicateSelectedKeys")) )
		{
			InterpEd->DuplicateSelectedKeys();
		}
		else if(ParseCommand(&Str, TEXT("ViewFitSequence")) )
		{
			InterpEd->ViewFitSequence();
		}
		else if(ParseCommand(&Str, TEXT("ViewFitToSelected")) )
		{
			InterpEd->ViewFitToSelected();
		}
		else if(ParseCommand(&Str, TEXT("ViewFitLoop")) )
		{
			InterpEd->ViewFitLoop();
		}
		else if(ParseCommand(&Str, TEXT("ViewFitLoopSequence")) )
		{
			InterpEd->ViewFitLoopSequence();
		}
		else if(ParseCommand(&Str, TEXT("ViewEndOfTrack")) )
		{
			InterpEd->ViewEndOfTrack();
		}
		else if(ParseCommand(&Str, TEXT("ChangeKeyInterpModeAUTO")) )
		{
			InterpEd->ChangeKeyInterpMode(CIM_CurveAuto);
		}
		else if(ParseCommand(&Str, TEXT("ChangeKeyInterpModeAUTOCLAMPED")) )
		{
			InterpEd->ChangeKeyInterpMode(CIM_CurveAutoClamped);
		}
		else if(ParseCommand(&Str, TEXT("ChangeKeyInterpModeUSER")) )
		{
			InterpEd->ChangeKeyInterpMode(CIM_CurveUser);
		}
		else if(ParseCommand(&Str, TEXT("ChangeKeyInterpModeBREAK")) )
		{
			InterpEd->ChangeKeyInterpMode(CIM_CurveBreak);
		}
		else if(ParseCommand(&Str, TEXT("ChangeKeyInterpModeLINEAR")) )
		{
			InterpEd->ChangeKeyInterpMode(CIM_Linear);
		}
		else if(ParseCommand(&Str, TEXT("ChangeKeyInterpModeCONSTANT")) )
		{
			InterpEd->ChangeKeyInterpMode(CIM_Constant);
		}
	}
}

/*-----------------------------------------------------------------------------
 WxInterpEdVCHolder
 -----------------------------------------------------------------------------*/


BEGIN_EVENT_TABLE( WxInterpEdVCHolder, wxWindow )
	EVT_SIZE( WxInterpEdVCHolder::OnSize )
END_EVENT_TABLE()

WxInterpEdVCHolder::WxInterpEdVCHolder( wxWindow* InParent, wxWindowID InID, WxInterpEd* InInterpEd )
: wxWindow( InParent, InID )
{
	SetMinSize(wxSize(2, 2));

	// Create renderer viewport.
	InterpEdVC = new FInterpEdViewportClient( InInterpEd );
	InterpEdVC->Viewport = GEngine->Client->CreateWindowChildViewport(InterpEdVC, (HWND)GetHandle());
	InterpEdVC->Viewport->CaptureJoystickInput(FALSE);

	// Create the vertical scroll bar.  We want this on the LEFT side, so the tracks line up in Matinee
	ScrollBar_Vert = new wxScrollBar(this, IDM_INTERP_VERT_SCROLL_BAR, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL);

	// Setup the initial metrics for the scroll bar
	AdjustScrollBar();
}

WxInterpEdVCHolder::~WxInterpEdVCHolder()
{
	DestroyViewport();
}

/**
 * Destroys the viewport held by this viewport holder, disassociating it from the engine, etc.  Rentrant.
 */
void WxInterpEdVCHolder::DestroyViewport()
{
	if ( InterpEdVC )
	{
		GEngine->Client->CloseViewport(InterpEdVC->Viewport);
		InterpEdVC->Viewport = NULL;
		delete InterpEdVC;
		InterpEdVC = NULL;
	}
}



/**
 * Updates the scroll bar for the current state of the window's size and content layout.  This should be called
 *  when either the window size changes or the vertical size of the content contained in the window changes.
 */
void WxInterpEdVCHolder::AdjustScrollBar()
{
	if( InterpEdVC != NULL && ScrollBar_Vert != NULL )
	{
		// Grab the height of the client window
		wxRect rc = GetClientRect();
		const UINT ViewportHeight = rc.GetHeight();

		// Compute scroll bar layout metrics
		UINT ContentHeight = InterpEdVC->ComputeGroupListContentHeight();
		UINT ContentBoxHeight =	InterpEdVC->ComputeGroupListBoxHeight( ViewportHeight );


		// The current scroll bar position
		const INT ScrollBarPos = -InterpEdVC->ThumbPos_Vert;

		// The thumb size is the number of 'scrollbar units' currently visible
		const UINT ScrollBarThumbSize = ContentBoxHeight;

		// The size of a 'scrollbar page'.  This is how much to scroll when paging up and down.
		const UINT ScrollBarPageSize = ScrollBarThumbSize;


		// Configure the scroll bar's position and size
		wxRect rcSBV = ScrollBar_Vert->GetClientRect();
        ScrollBar_Vert->SetSize( 0, 0, rcSBV.GetWidth(), InterpEdVC->bWantTimeline ? rc.GetHeight() - 64 : rc.GetHeight() );

		// Configure the scroll bar layout metrics
		ScrollBar_Vert->SetScrollbar(
			ScrollBarPos,         // Position
			ScrollBarThumbSize,   // Thumb size
			ContentHeight,        // Range
			ScrollBarPageSize );  // Page size
	}
}


void WxInterpEdVCHolder::OnSize( wxSizeEvent& In )
{
	// Update the window's content layout
	UpdateWindowLayout();
}


/**
 * Updates layout of the track editor window's content layout.  This is usually called in response to a window size change
 */
void WxInterpEdVCHolder::UpdateWindowLayout()
{
	if( InterpEdVC != NULL )
	{
		// The track window size has changed, so update our scroll bar
		AdjustScrollBar();

		wxRect rc = GetClientRect();

		// Make sure the track window's group list is positioned to the right of the scroll bar
		if( ScrollBar_Vert != NULL )
		{
			INT ScrollBarWidth = ScrollBar_Vert->GetClientRect().GetWidth();
			rc.x += ScrollBarWidth;
			rc.width -= ScrollBarWidth;
		}
		::MoveWindow( (HWND)InterpEdVC->Viewport->GetWindow(), rc.x, rc.y, rc.GetWidth(), rc.GetHeight(), 1 );
	}
}



/*-----------------------------------------------------------------------------
 WxInterpEd
 -----------------------------------------------------------------------------*/


UBOOL				WxInterpEd::bInterpTrackClassesInitialized = FALSE;
TArray<UClass*>		WxInterpEd::InterpTrackClasses;

// On init, find all track classes. Will use later on to generate menus.
void WxInterpEd::InitInterpTrackClasses()
{
	if(bInterpTrackClassesInitialized)
		return;

	// Construct list of non-abstract gameplay sequence object classes.
	for(TObjectIterator<UClass> It; It; ++It)
	{
		if( It->IsChildOf(UInterpTrack::StaticClass()) && !(It->ClassFlags & CLASS_Abstract) )
		{
			InterpTrackClasses.AddItem(*It);
		}
	}

	bInterpTrackClassesInitialized = TRUE;
}

void WxInterpEd::SetAudioRealtimeOverride( UBOOL bAudioIsRealtime, UBOOL bSaveExisting ) const
{
	for(INT i=0; i<GApp->EditorFrame->ViewportConfigData->GetViewportCount(); i++)
	{
		WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->AccessViewport(i).ViewportWindow;
		if(LevelVC)
		{
			// Turn off realtime when exiting.
			if(LevelVC->ViewportType == LVT_Perspective && LevelVC->AllowMatineePreview() )
			{				
				LevelVC->OverrideRealtimeAudio( TRUE );
				LevelVC->SetAudioRealtime( bAudioIsRealtime, bSaveExisting );
			}
		}
	}
}

void WxInterpEd::RestoreAudioRealtimeOverride() const
{
	for(INT i=0; i<GApp->EditorFrame->ViewportConfigData->GetViewportCount(); i++)
	{
		WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->AccessViewport(i).ViewportWindow;
		if(LevelVC)
		{
			// Turn off realtime when exiting.
			if(LevelVC->ViewportType == LVT_Perspective && LevelVC->AllowMatineePreview() )
			{				
				LevelVC->OverrideRealtimeAudio( FALSE );
				LevelVC->RestoreAudioRealtime();
			}
		}
	}
}
BEGIN_EVENT_TABLE( WxInterpEd, WxTrackableFrame )
	EVT_SIZE( WxInterpEd::OnSize )
	EVT_CLOSE( WxInterpEd::OnClose )
	EVT_MENU( IDM_INTERP_FILE_EXPORT, WxInterpEd::OnMenuExport )
	EVT_MENU( IDM_INTERP_FILE_IMPORT, WxInterpEd::OnMenuImport )
	EVT_MENU( IDM_INTERP_ExportSoundCueInfo, WxInterpEd::OnExportSoundCueInfoCommand )
	EVT_MENU( IDM_INTERP_ExportAnimInfo, WxInterpEd::OnExportAnimationInfoCommand )
	EVT_MENU( IDM_INTERP_FILE_EXPORT_BAKE_TRANSFORMS, WxInterpEd::OnToggleBakeTransforms )
	EVT_UPDATE_UI( IDM_INTERP_FILE_EXPORT_BAKE_TRANSFORMS, WxInterpEd::OnToggleBakeTransforms_UpdateUI )
	EVT_MENU_RANGE( IDM_INTERP_NEW_TRACK_START, IDM_INTERP_NEW_TRACK_END, WxInterpEd::OnContextNewTrack )
	EVT_MENU_RANGE( IDM_INTERP_KEYMODE_LINEAR, IDM_INTERP_KEYMODE_CONSTANT, WxInterpEd::OnContextKeyInterpMode )
	EVT_MENU( IDM_INTERP_EVENTKEY_RENAME, WxInterpEd::OnContextRenameEventKey )
	EVT_MENU( IDM_INTERP_NOTIFYKEY_EDIT, WxInterpEd::OnContextEditNotifyKey )
	EVT_MENU( IDM_INTERP_KEY_SETTIME, WxInterpEd::OnContextSetKeyTime )
	EVT_MENU( IDM_INTERP_KEY_SETVALUE, WxInterpEd::OnContextSetValue )
	EVT_MENU( IDM_INTERP_KEY_SETCOLOR, WxInterpEd::OnContextSetColor )
	EVT_MENU( IDM_INTERP_KEY_SETBOOL, WxInterpEd::OnContextSetBool )
	EVT_MENU( IDM_INTERP_ANIMKEY_LOOP, WxInterpEd::OnSetAnimKeyLooping )
	EVT_MENU( IDM_INTERP_ANIMKEY_NOLOOP, WxInterpEd::OnSetAnimKeyLooping )
	EVT_MENU( IDM_INTERP_ANIMKEY_SETSTARTOFFSET, WxInterpEd::OnSetAnimOffset )
	EVT_MENU( IDM_INTERP_ANIMKEY_SETENDOFFSET, WxInterpEd::OnSetAnimOffset )
	EVT_MENU( IDM_INTERP_ANIMKEY_SETPLAYRATE, WxInterpEd::OnSetAnimPlayRate )
	EVT_MENU( IDM_INTERP_ANIMKEY_TOGGLEREVERSE, WxInterpEd::OnToggleReverseAnim )
	EVT_UPDATE_UI( IDM_INTERP_ANIMKEY_TOGGLEREVERSE, WxInterpEd::OnToggleReverseAnim_UpdateUI )

	EVT_MENU( IDM_INTERP_CAMERA_ANIM_EXPORT, WxInterpEd::OnContextSaveAsCameraAnimation )

	EVT_MENU( IDM_INTERP_SoundKey_SetVolume, WxInterpEd::OnSetSoundVolume )
	EVT_MENU( IDM_INTERP_SoundKey_SetPitch, WxInterpEd::OnSetSoundPitch )
	EVT_MENU( IDM_INTERP_KeyContext_SyncGenericBrowserToSoundCue, WxInterpEd::OnKeyContext_SyncGenericBrowserToSoundCue )
	EVT_MENU( IDM_INTERP_KeyContext_SetMasterVolume, WxInterpEd::OnKeyContext_SetMasterVolume )
	EVT_MENU( IDM_INTERP_KeyContext_SetMasterPitch, WxInterpEd::OnKeyContext_SetMasterPitch )
	EVT_MENU( IDM_INTERP_ParticleReplayKeyContext_SetClipIDNumber, WxInterpEd::OnParticleReplayKeyContext_SetClipIDNumber )
	EVT_MENU( IDM_INTERP_ParticleReplayKeyContext_SetDuration, WxInterpEd::OnParticleReplayKeyContext_SetDuration )
	EVT_MENU( IDM_INTERP_DIRKEY_SETTRANSITIONTIME, WxInterpEd::OnContextDirKeyTransitionTime )
	EVT_MENU( IDM_INTERP_DIRKEY_RENAMECAMERASHOT, WxInterpEd::OnContextDirKeyRenameCameraShot )
	EVT_MENU( IDM_INTERP_TOGGLEKEY_FLIP, WxInterpEd::OnFlipToggleKey )
	EVT_MENU( IDM_INTERP_KeyContext_SetCondition_Always, WxInterpEd::OnKeyContext_SetCondition )
	EVT_MENU( IDM_INTERP_KeyContext_SetCondition_GoreEnabled, WxInterpEd::OnKeyContext_SetCondition )
	EVT_MENU( IDM_INTERP_KeyContext_SetCondition_GoreDisabled, WxInterpEd::OnKeyContext_SetCondition )

	EVT_MENU( IDM_INTERP_DeleteSelectedKeys, WxInterpEd::OnDeleteSelectedKeys )
	EVT_MENU( IDM_INTERP_GROUP_RENAME, WxInterpEd::OnContextGroupRename )
	EVT_MENU( IDM_INTERP_GROUP_DUPLICATE, WxInterpEd::OnContextNewGroup )
	EVT_MENU( IDM_INTERP_GROUP_DELETE, WxInterpEd::OnContextGroupDelete )
	EVT_MENU( IDM_INTERP_GROUP_CREATETAB, WxInterpEd::OnContextGroupCreateTab )
	EVT_MENU( IDM_INTERP_GROUP_DELETETAB, WxInterpEd::OnContextDeleteGroupTab )
	EVT_MENU( IDM_INTERP_GROUP_REMOVEFROMTAB, WxInterpEd::OnContextGroupRemoveFromTab )
	EVT_MENU_RANGE( IDM_INTERP_GROUP_SENDTOTAB_START, IDM_INTERP_GROUP_SENDTOTAB_END, WxInterpEd::OnContextGroupSendToTab )
	EVT_MENU_RANGE( IDM_INTERP_GROUP_MoveActiveGroupToFolder_Start, IDM_INTERP_GROUP_MoveActiveGroupToFolder_End, WxInterpEd::OnContextGroupChangeGroupFolder )
	EVT_MENU_RANGE( IDM_INTERP_GROUP_MoveGroupToActiveFolder_Start, IDM_INTERP_GROUP_MoveGroupToActiveFolder_End, WxInterpEd::OnContextGroupChangeGroupFolder )
	EVT_MENU( IDM_INTERP_GROUP_RemoveFromGroupFolder, WxInterpEd::OnContextGroupChangeGroupFolder )
	EVT_MENU( IDM_INTERP_GROUP_ExportAnimTrackFBX, WxInterpEd::OnContextGroupExportAnimFBX )
	
	EVT_MENU( IDM_INTERP_TRACK_RENAME, WxInterpEd::OnContextTrackRename )
	EVT_MENU( IDM_INTERP_TRACK_DELETE, WxInterpEd::OnContextTrackDelete )
	EVT_MENU( IDM_INTERP_TOGGLE_CURVEEDITOR, WxInterpEd::OnToggleCurveEd )

	EVT_MENU_RANGE( IDM_INTERP_TRACK_FRAME_WORLD, IDM_INTERP_TRACK_FRAME_RELINITIAL, WxInterpEd::OnContextTrackChangeFrame )
	EVT_MENU( IDM_INTERP_TRACK_SPLIT_TRANS_AND_ROT, WxInterpEd::OnSplitTranslationAndRotation )
	EVT_MENU( IDM_INTERP_TRACK_NORMALIZE_VELOCITY, WxInterpEd::NormalizeVelocity )
	EVT_MENU( IDM_INTERP_TRACK_Show3DTrajectory, WxInterpEd::OnContextTrackShow3DTrajectory )
#if WITH_FBX
	EVT_MENU( IDM_INTERP_TRACK_ExportAnimTrackFBX, WxInterpEd::OnContextTrackExportAnimFBX )
#endif
	EVT_MENU( IDM_INTERP_VIEW_ShowAll3DTrajectories, WxInterpEd::OnViewShowOrHideAll3DTrajectories )
	EVT_MENU( IDM_INTERP_VIEW_HideAll3DTrajectories, WxInterpEd::OnViewShowOrHideAll3DTrajectories )
	EVT_MENU( IDM_INTERP_ParticleReplayTrackContext_StartRecording, WxInterpEd::OnParticleReplayTrackContext_ToggleCapture )
	EVT_MENU( IDM_INTERP_NotifyTrackContext_SetParentNodeName, WxInterpEd::OnNotifyTrackContext_SetParentNodeName )
	EVT_MENU( IDM_INTERP_ParticleReplayTrackContext_StopRecording, WxInterpEd::OnParticleReplayTrackContext_ToggleCapture )
	EVT_MENU( IDM_INTERP_NEW_FOLDER, WxInterpEd::OnContextNewGroup )
	EVT_MENU( IDM_INTERP_NEW_EMPTY_GROUP, WxInterpEd::OnContextNewGroup )
	EVT_MENU( IDM_INTERP_NEW_CAMERA_GROUP, WxInterpEd::OnContextNewGroup )
	EVT_MENU( IDM_INTERP_NEW_PARTICLE_GROUP, WxInterpEd::OnContextNewGroup )
	EVT_MENU( IDM_INTERP_NEW_SKELETAL_MESH_GROUP, WxInterpEd::OnContextNewGroup )
	EVT_MENU( IDM_INTERP_NEW_AI_GROUP, WxInterpEd::OnContextNewGroup )
	EVT_MENU( IDM_INTERP_NEW_LIGHTING_GROUP, WxInterpEd::OnContextNewGroup )
	EVT_MENU( IDM_INTERP_NEW_DIRECTOR_GROUP, WxInterpEd::OnContextNewGroup )
	EVT_MENU( IDM_INTERP_ADDKEY, WxInterpEd::OnMenuAddKey )
	EVT_COMBOBOX( IDM_INTERP_InitialInterpMode_ComboBox, WxInterpEd::OnChangeInitialInterpMode )
	EVT_MENU( IDM_INTERP_PLAY, WxInterpEd::OnMenuPlay )
	EVT_MENU( IDM_INTERP_PlayReverse, WxInterpEd::OnMenuPlay )
	EVT_MENU( IDM_INTERP_CreateMovie, WxInterpEd::OnMenuCreateMovie )
	EVT_MENU( IDM_INTERP_PLAYLOOPSECTION, WxInterpEd::OnMenuPlay )
	EVT_MENU( IDM_INTERP_STOP, WxInterpEd::OnMenuStop )
	EVT_MENU_RANGE( IDM_INTERP_SPEED_1, IDM_INTERP_SPEED_100, WxInterpEd::OnChangePlaySpeed )
	EVT_MENU( IDM_INTERP_ToggleGorePreview, WxInterpEd::OnToggleGorePreview )
	EVT_UPDATE_UI( IDM_INTERP_ToggleGorePreview, WxInterpEd::OnToggleGorePreview_UpdateUI )
	EVT_MENU( IDM_INTERP_CreateCameraActorAtCurrentCameraLocation, WxInterpEd::OnCreateCameraActorAtCurrentCameraLocation )
	
	EVT_MENU( IDM_INTERP_LaunchRecordWindow, WxInterpEd::OnLaunchRecordingViewport )
	EVT_MENU( IDM_INTERP_DirectorWindow, WxInterpEd::OnLaunchDirectorWindow )

	EVT_MENU( IDM_OPEN_BINDKEYS_DIALOG, WxInterpEd::OnOpenBindKeysDialog )

	EVT_MENU( IDM_INTERP_EDIT_UNDO, WxInterpEd::OnMenuUndo )
	EVT_MENU( IDM_INTERP_EDIT_REDO, WxInterpEd::OnMenuRedo )
	EVT_MENU( IDM_INTERP_EDIT_CUT, WxInterpEd::OnMenuCut )
	EVT_MENU( IDM_INTERP_EDIT_COPY, WxInterpEd::OnMenuCopy )
	EVT_MENU( IDM_INTERP_EDIT_PASTE, WxInterpEd::OnMenuPaste )

	EVT_UPDATE_UI ( IDM_INTERP_EDIT_UNDO, WxInterpEd::OnMenuEdit_UpdateUI )
	EVT_UPDATE_UI ( IDM_INTERP_EDIT_REDO, WxInterpEd::OnMenuEdit_UpdateUI )
	EVT_UPDATE_UI ( IDM_INTERP_EDIT_CUT, WxInterpEd::OnMenuEdit_UpdateUI )
	EVT_UPDATE_UI ( IDM_INTERP_EDIT_COPY, WxInterpEd::OnMenuEdit_UpdateUI )
	EVT_UPDATE_UI ( IDM_INTERP_EDIT_PASTE, WxInterpEd::OnMenuEdit_UpdateUI )

	EVT_MENU( IDM_INTERP_MOVEKEY_SETLOOKUP, WxInterpEd::OnSetMoveKeyLookupGroup )
	EVT_MENU( IDM_INTERP_MOVEKEY_CLEARLOOKUP, WxInterpEd::OnClearMoveKeyLookupGroup )

	EVT_MENU( IDM_INTERP_TOGGLE_SNAP, WxInterpEd::OnToggleSnap )
	EVT_UPDATE_UI( IDM_INTERP_TOGGLE_SNAP, WxInterpEd::OnToggleSnap_UpdateUI )
	EVT_MENU( IDM_INTERP_TOGGLE_SNAP_TIME_TO_FRAMES, WxInterpEd::OnToggleSnapTimeToFrames )
	EVT_UPDATE_UI( IDM_INTERP_TOGGLE_SNAP_TIME_TO_FRAMES, WxInterpEd::OnToggleSnapTimeToFrames_UpdateUI )
	EVT_MENU( IDM_INTERP_FixedTimeStepPlayback, WxInterpEd::OnFixedTimeStepPlaybackCommand )
	EVT_UPDATE_UI( IDM_INTERP_FixedTimeStepPlayback, WxInterpEd::OnFixedTimeStepPlaybackCommand_UpdateUI )
	EVT_MENU( IDM_INTERP_PreferFrameNumbers, WxInterpEd::OnPreferFrameNumbersCommand )
	EVT_UPDATE_UI( IDM_INTERP_PreferFrameNumbers, WxInterpEd::OnPreferFrameNumbersCommand_UpdateUI )
	EVT_MENU( IDM_INTERP_ShowTimeCursorPosForAllKeys, WxInterpEd::OnShowTimeCursorPosForAllKeysCommand )
	EVT_UPDATE_UI( IDM_INTERP_ShowTimeCursorPosForAllKeys, WxInterpEd::OnShowTimeCursorPosForAllKeysCommand_UpdateUI )
	EVT_MENU( IDM_INTERP_VIEW_FITSEQUENCE, WxInterpEd::OnViewFitSequence )
	EVT_MENU( IDM_INTERP_VIEW_FitViewToSelected, WxInterpEd::OnViewFitToSelected )
	EVT_MENU( IDM_INTERP_VIEW_FITLOOP, WxInterpEd::OnViewFitLoop )
	EVT_MENU( IDM_INTERP_VIEW_FITLOOPSEQUENCE, WxInterpEd::OnViewFitLoopSequence )
	EVT_MENU( IDM_INTERP_VIEW_ENDOFSEQUENCE, WxInterpEd::OnViewEndOfTrack )
	EVT_COMBOBOX( IDM_INTERP_SNAPCOMBO, WxInterpEd::OnChangeSnapSize )
	EVT_MENU( IDM_INTERP_EDIT_INSERTSPACE, WxInterpEd::OnMenuInsertSpace )
	EVT_MENU( IDM_INTERP_EDIT_STRETCHSECTION, WxInterpEd::OnMenuStretchSection )
	EVT_MENU( IDM_INTERP_EDIT_STRETCHSELECTEDKEYFRAMES, WxInterpEd::OnMenuStretchSelectedKeyframes )
	EVT_MENU( IDM_INTERP_EDIT_DELETESECTION, WxInterpEd::OnMenuDeleteSection )
	EVT_MENU( IDM_INTERP_EDIT_SELECTINSECTION, WxInterpEd::OnMenuSelectInSection )
	EVT_MENU( IDM_INTERP_EDIT_DUPLICATEKEYS, WxInterpEd::OnMenuDuplicateSelectedKeys )
	EVT_MENU( IDM_INTERP_EDIT_SAVEPATHTIME, WxInterpEd::OnSavePathTime )
	EVT_MENU( IDM_INTERP_EDIT_JUMPTOPATHTIME, WxInterpEd::OnJumpToPathTime )
	EVT_MENU( IDM_INTERP_EDIT_REDUCEKEYS, WxInterpEd::OnMenuReduceKeys )
	EVT_MENU( IDM_INTERP_VIEW_Draw3DTrajectories, WxInterpEd::OnViewHide3DTracks )
	EVT_MENU( IDM_INTERP_VIEW_ZoomToTimeCursorPosition, WxInterpEd::OnViewZoomToScrubPos )
	EVT_MENU( IDM_INTERP_VIEW_ViewportFrameStats, WxInterpEd::OnToggleViewportFrameStats )
	EVT_MENU( IDM_INTERP_VIEW_EnableEditingGrid, WxInterpEd::OnEnableEditingGrid )
	EVT_UPDATE_UI( IDM_INTERP_VIEW_EnableEditingGrid, WxInterpEd::OnEnableEditingGridUpdateUI )
	EVT_MENU_RANGE( IDM_INTERP_VIEW_EditingGrid_Start, IDM_INTERP_VIEW_EditingGrid_End, WxInterpEd::OnSetEditingGrid )
	EVT_UPDATE_UI_RANGE( IDM_INTERP_VIEW_EditingGrid_Start, IDM_INTERP_VIEW_EditingGrid_End, WxInterpEd::OnEditingGridUpdateUI )
	EVT_MENU( IDM_INTERP_VIEW_EditingCrosshair, WxInterpEd::OnToggleEditingCrosshair )

	EVT_MENU( IDM_INTERP_ExpandAllGroups, WxInterpEd::OnExpandAllGroups )
	EVT_MENU( IDM_INTERP_CollapseAllGroups, WxInterpEd::OnCollapseAllGroups )

	EVT_MENU( IDM_INTERP_INVERT_PAN, WxInterpEd::OnToggleInvertPan )

	EVT_MENU( IDM_INTERP_Marker_MoveToCurrentPosition, WxInterpEd::OnContextMoveMarkerToCurrentPosition )
	EVT_MENU( IDM_INTERP_Marker_MoveToBeginning, WxInterpEd::OnContextMoveMarkerToBeginning )
	EVT_MENU( IDM_INTERP_Marker_MoveToEnd, WxInterpEd::OnContextMoveMarkerToEnd )
	EVT_MENU( IDM_INTERP_Marker_MoveToEndOfSelectedTrack, WxInterpEd::OnContextMoveMarkerToEndOfSelectedTrack )
	EVT_MENU( IDM_INTERP_Marker_MoveToEndOfLongestTrack, WxInterpEd::OnContextMoveMarkerToEndOfLongestTrack )

	EVT_MENU( IDM_INTERP_ToggleAllowKeyframeBarSelection, WxInterpEd::OnToggleKeyframeBarSelection )
	EVT_MENU( IDM_INTERP_ToggleAllowKeyframeTextSelection, WxInterpEd::OnToggleKeyframeTextSelection )
	EVT_UPDATE_UI( IDM_INTERP_ToggleAllowKeyframeBarSelection, WxInterpEd::OnToggleKeyframeBarSelection_UpdateUI )
	EVT_UPDATE_UI( IDM_INTERP_ToggleAllowKeyframeTextSelection, WxInterpEd::OnToggleKeyframeTextSelection_UpdateUI )

	EVT_SPLITTER_SASH_POS_CHANGED( IDM_INTERP_GRAPH_SPLITTER, WxInterpEd::OnGraphSplitChangePos )
	EVT_SCROLL( WxInterpEd::OnScroll )
END_EVENT_TABLE()


/** Should NOT open an InterpEd unless InInterp has a valid InterpData attached! */
WxInterpEd::WxInterpEd( wxWindow* InParent, wxWindowID InID, USeqAct_Interp* InInterp )
	:	WxTrackableFrame( InParent, InID, *LocalizeUnrealEd("Matinee"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR )
	,	FDockingParent(this)
	,	bClosed( FALSE )
	,   PropertyWindow( NULL )
	,   CurveEd( NULL )
	,	TrackWindow(NULL)
	,	DirectorTrackWindow(NULL)
	,	CurveEdToggleMenuItem( NULL )
	,	ToolBar( NULL )
	,	MenuBar( NULL )
	,	bIsInitialized( FALSE )
	,	bViewportFrameStatsEnabled( TRUE )
	,	bEditingCrosshairEnabled( FALSE )
	,	bEditingGridEnabled( FALSE )
	,	bBakeTransforms( FALSE )
	,	bAllowKeyframeBarSelection( FALSE )
	,	bAllowKeyframeTextSelection( FALSE )
	,	EditingGridSize(0)
	,	RecordMenuSelection(MatineeConstants::RECORD_MENU_RECORD_MODE)
	,	bDisplayRecordingMenu(TRUE)
	,	RecordingState(MatineeConstants::RECORDING_COMPLETE)
	,	RecordMode(MatineeConstants::RECORD_MODE_NEW_CAMERA)
	,	RecordRollSmoothingSamples(5)
	,	RecordPitchSmoothingSamples(5)
	,	RecordCameraMovementScheme(MatineeConstants::CAMERA_SCHEME_FREE_CAM)
	,	RecordingStateStartTime(0)
{
	// Make sure we have a list of available track classes
	WxInterpEd::InitInterpTrackClasses();

	// NOTE: This should match the curve editor's label width!
	LabelWidth = 200;

	// 3D tracks should be visible by default
	bHide3DTrackView = FALSE;
	GConfig->GetBool( TEXT("Matinee"), TEXT("Hide3DTracks"), bHide3DTrackView, GEditorUserSettingsIni );

	// Zoom to scrub position defaults to off.  We want zoom to cursor position by default.
	bZoomToScrubPos = FALSE;
	GConfig->GetBool( TEXT("Matinee"), TEXT("ZoomToScrubPos"), bZoomToScrubPos, GEditorUserSettingsIni );

	// Setup 'viewport frame stats' preference
	bViewportFrameStatsEnabled = TRUE;
	GConfig->GetBool( TEXT("Matinee"), TEXT("ViewportFrameStats"), bViewportFrameStatsEnabled, GEditorUserSettingsIni );

	// Get the editing grid size from user settings
	EditingGridSize = 1;
	GConfig->GetInt( TEXT("Matinee"), TEXT("EditingGridSize"), EditingGridSize, GEditorUserSettingsIni );

	// Look to see if the crosshair should be enabled
	// Disabled by default
	bEditingCrosshairEnabled = FALSE;
	GConfig->GetBool( TEXT("Matinee"), TEXT("EditingCrosshair"), bEditingCrosshairEnabled, GEditorUserSettingsIni );

	// Look to see if the editing grid should be enabled
	bEditingGridEnabled = FALSE;
	GConfig->GetBool( TEXT("Matinee"), TEXT("EnableEditingGrid"), bEditingGridEnabled, GEditorUserSettingsIni );

	// Setup "allow keyframe bar selection" preference
	GConfig->GetBool( TEXT("Matinee"), TEXT("AllowKeyframeBarSelection"), bAllowKeyframeBarSelection, GEditorUserSettingsIni ); 

	// Setup "allow keyframe text selection" preference
	GConfig->GetBool( TEXT("Matinee"), TEXT("AllowKeyframeTextSelection"), bAllowKeyframeTextSelection, GEditorUserSettingsIni );

	// We want to update the Attached array of each Actor, so that we can move attached things then their base moves.
	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		if(Actor && !Actor->bDeleteMe)
		{
			Actor->EditorUpdateBase();
		}
	}


	// Create options object.
	Opt = ConstructObject<UInterpEdOptions>( UInterpEdOptions::StaticClass(), INVALID_OBJECT, NAME_None, RF_Transactional );
	check(Opt);

	// Swap out regular UTransactor for our special one
	GEditor->ResetTransaction( *LocalizeUnrealEd("OpenMatinee") );

	NormalTransactor = GEditor->Trans;
	InterpEdTrans = new UInterpEdTransBuffer( 8*1024*1024 );
	GEditor->Trans = InterpEdTrans;

	// Set up pointers to interp objects
	Interp = InInterp;

	// Do all group/track instancing and variable hook-up.
	Interp->InitInterp();

	// Flag this action as 'being edited'
	Interp->bIsBeingEdited = TRUE;


	// Always start out with gore preview turned on in the editor!
	Interp->bShouldShowGore = TRUE;


	// Should always find some data.
	check(Interp->InterpData);
	IData = Interp->InterpData;


	// Repair any folder/group hierarchy problems in the data set
	RepairHierarchyProblems();


	PixelsPerSec = 1.f;
	TrackViewSizeX = 0;
	NavPixelsPerSecond = 1.0f;

	// Set initial zoom range
	ViewStartTime = 0.f;
	ViewEndTime = IData->InterpLength;

	bDrawSnappingLine = FALSE;
	SnappingLinePosition = 0.f;
	UnsnappedMarkerPos = 0.f;


	// Set the default filter for the data
	if(IData->DefaultFilters.Num())
	{
		SetSelectedFilter(IData->DefaultFilters(0));
	}
	else
	{
		SetSelectedFilter(NULL);
	}

	// Slight hack to ensure interpolation data is transactional.
	Interp->SetFlags( RF_Transactional );
	IData->SetFlags( RF_Transactional );
	for(INT i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups(i);
		Group->SetFlags( RF_Transactional );

		for(INT j=0; j<Group->InterpTracks.Num(); j++)
		{
			Group->InterpTracks(j)->SetFlags( RF_Transactional );
		}
	}


	FString ContentWarnings;

	// For each track let it save the state of the object its going to work on before being changed at all by Matinee.
	for(INT i=0; i<Interp->GroupInst.Num(); i++)
	{
		UInterpGroupInst* GrInst = Interp->GroupInst(i);
		GrInst->SaveGroupActorState();

		AActor* GroupActor = GrInst->GetGroupActor();
		if ( GroupActor )
		{
			// Save this actor's transformations if we need to (along with its children)
			Interp->ConditionallySaveActorState( GrInst, GroupActor );

			// Check for bStatic actors that have dynamic tracks associated with them and report a warning to the user
			if ( GroupActor->IsStatic() )
			{
				UBOOL bHasTrack = FALSE;
				FString TrackNames;

				for(INT TrackIdx=0; TrackIdx<GrInst->Group->InterpTracks.Num(); TrackIdx++)
				{
					if( GrInst->Group->InterpTracks(TrackIdx)->AllowStaticActors()==FALSE )
					{
						bHasTrack = TRUE;

						if( TrackNames.Len() > 0 )
						{
							TrackNames += ", ";
						}
						TrackNames += GrInst->Group->InterpTracks(TrackIdx)->GetClass()->GetDescription();
					}
				}

				if(bHasTrack)
				{
					// Warn if any groups with dynamic tracks are trying to act on bStatic actors!

					// Add to list of warnings of this type
					FString WarningString = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "GroupOnStaticActor_F" ),
						*TrackNames,
						*GrInst->Group->GroupName.ToString(), 
						*GroupActor->GetName() ) );

					ContentWarnings += WarningString;
				}
			}


			// Also, we'll check for actors configured to use a non-interpolating physics mode (such as PHYS_None.)  If
			// the actor's group has tracks, then PHYS_None is going to prevent the object's properties from being
			// animated in game, although it may appear to animate in UnrealEd.  We'll report a warning to the user
			// about these actors.
			if ( GroupActor->Physics != PHYS_Interpolating )
			{
				UBOOL bHasPhysicsRelatedTrack = FALSE;
				FString PhysicsRelatedTrackNames;

				for( INT TrackIdx = 0; TrackIdx < GrInst->Group->InterpTracks.Num(); ++TrackIdx )
				{
					const UInterpTrack* CurTrack = GrInst->Group->InterpTracks( TrackIdx );

					// Is this is a 'movement' track?  If so, then we'll consider it worthy of our test
					if( CurTrack->IsA( UInterpTrackMove::StaticClass() ) )
					{
						bHasPhysicsRelatedTrack = TRUE;

						if( PhysicsRelatedTrackNames.Len() > 0 )
						{
							PhysicsRelatedTrackNames += ", ";
						}
						PhysicsRelatedTrackNames += CurTrack->GetClass()->GetDescription();
					}
				}

				// If we have at least one track, then we can assume that Matinee will be manipulating this object,
				// and that the physics mode should probably be set to Phys_Interpolating.
				if( bHasPhysicsRelatedTrack )
				{
					// Tracks are bound to an actor that is not configured to use interpolation physics!

					// Add to list of warnings of this type
					FString WarningString = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "GroupOnNonInterpActor_F" ),
						*PhysicsRelatedTrackNames,
						*GrInst->Group->GroupName.ToString(), 
						*GroupActor->GetName() ) );

					ContentWarnings += WarningString;
				}
			}


			// Check for toggle tracks bound to non-toggleable light sources
			ALight* LightActor = Cast< ALight>( GroupActor );
			if( LightActor != NULL && !LightActor->IsToggleable() )
			{
				UBOOL bHasTrack = FALSE;
				FString TrackNames;

				for( INT TrackIdx = 0; TrackIdx < GrInst->Group->InterpTracks.Num(); ++TrackIdx )
				{
					if( GrInst->Group->InterpTracks( TrackIdx )->IsA( UInterpTrackToggle::StaticClass() ) )
					{
						bHasTrack = TRUE;

						if( TrackNames.Len() > 0 )
						{
							TrackNames += ", ";
						}
						TrackNames += GrInst->Group->InterpTracks( TrackIdx )->GetClass()->GetDescription();
					}
				}

				if( bHasTrack )
				{
					// Warn if any groups with toggle tracks are trying to act on non-toggleable light sources!

					// Add to list of warnings of this type
					FString WarningString = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "InterpEd_ToggleTrackOnNonToggleableLight_F" ),
						*TrackNames,
						*GrInst->Group->GroupName.ToString(), 
						*GroupActor->GetName() ) );

					ContentWarnings += WarningString;
				}
			}
		}
	}


	// Is "force start pos" enabled?  If so, check for some common problems with use of that
	if( Interp->bForceStartPos && Interp->ForceStartPosition > 0.0f )
	{
		for( INT CurGroupIndex = 0; CurGroupIndex < Interp->InterpData->InterpGroups.Num(); ++CurGroupIndex )
		{
			const UInterpGroup* CurGroup = Interp->InterpData->InterpGroups( CurGroupIndex );

			for( INT CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
			{
				const UInterpTrack* CurTrack = CurGroup->InterpTracks( CurTrackIndex );


				UBOOL bNeedWarning = FALSE;


				// @todo: Abstract these checks!  Should be accessor check in UInterpTrack!

				// @todo: These checks don't involve actors or group instances, so we should move them to
				//     the Map Check phase instead of Matinee startup!


				// Toggle tracks don't play nice with bForceStartPos since they currently cannot 'fast forward',
				// except in certain cases
				const UInterpTrackToggle* ToggleTrack = ConstCast< UInterpTrackToggle >( CurTrack );
				if( ToggleTrack != NULL )
				{
					for( INT CurKeyIndex = 0; CurKeyIndex < ToggleTrack->ToggleTrack.Num(); ++CurKeyIndex )
					{
						const FToggleTrackKey& CurKey = ToggleTrack->ToggleTrack( CurKeyIndex );

						// Trigger events will be skipped entirely when jumping forward
						if( !ToggleTrack->bFireEventsWhenJumpingForwards ||
							CurKey.ToggleAction == ETTA_Trigger )
						{
							// Is this key's time within the range that we'll be skipping over due to the Force Start
							// Position being set to a later time, we'll warn the user about that!
							if( CurKey.Time < Interp->ForceStartPosition )
							{
								// One warning per track is plenty!
								bNeedWarning = TRUE;
								break;
							}
						}
					}
				}


				// Visibility tracks don't play nice with bForceStartPos since they currently cannot 'fast forward'
				const UInterpTrackVisibility* VisibilityTrack = ConstCast< UInterpTrackVisibility >( CurTrack );
				if( VisibilityTrack != NULL && !VisibilityTrack->bFireEventsWhenJumpingForwards )
				{
					for( INT CurKeyIndex = 0; CurKeyIndex < VisibilityTrack->VisibilityTrack.Num(); ++CurKeyIndex )
					{
						const FVisibilityTrackKey& CurKey = VisibilityTrack->VisibilityTrack( CurKeyIndex );

						// Is this key's time within the range that we'll be skipping over due to the Force Start
						// Position being set to a later time, we'll warn the user about that!
						if( CurKey.Time < Interp->ForceStartPosition )
						{
							// One warning per track is plenty!
							bNeedWarning = TRUE;
							break;
						}
					}
				}


				// Sound tracks don't play nice with bForceStartPos since we can't start playing from the middle
				// of an audio clip (not supported, yet)
				const UInterpTrackSound* SoundTrack = ConstCast< UInterpTrackSound >( CurTrack );
				if( SoundTrack != NULL )
				{
					for( INT CurKeyIndex = 0; CurKeyIndex < SoundTrack->Sounds.Num(); ++CurKeyIndex )
					{
						const FSoundTrackKey& CurKey = SoundTrack->Sounds( CurKeyIndex );

						// Is this key's time within the range that we'll be skipping over due to the Force Start
						// Position being set to a later time, we'll warn the user about that!
						if( CurKey.Time < Interp->ForceStartPosition )
						{
							// One warning per track is plenty!
							bNeedWarning = TRUE;
							break;
						}
					}
				}


				// Event tracks are only OK if bFireEventsWhenJumpingForwards is also set, since that will go
				// back and fire off events between 0 and the ForceStartPosition
				const UInterpTrackEvent* EventTrack = ConstCast< UInterpTrackEvent >( CurTrack );
				if( EventTrack != NULL && ( EventTrack->bFireEventsWhenJumpingForwards == FALSE ) )
				{
					for( INT CurKeyIndex = 0; CurKeyIndex < EventTrack->EventTrack.Num(); ++CurKeyIndex )
					{
						const FEventTrackKey& CurKey = EventTrack->EventTrack( CurKeyIndex );

						// Is this key's time within the range that we'll be skipping over due to the Force Start
						// Position being set to a later time, we'll warn the user about that!
						if( CurKey.Time < Interp->ForceStartPosition )
						{
							// One warning per track is plenty!
							bNeedWarning = TRUE;
							break;
						}
					}
				}


				// FaceFX tracks don't play nice with bForceStartPos since they currently cannot 'fast forward'
				const UInterpTrackFaceFX* FaceFXTrack = ConstCast< UInterpTrackFaceFX >( CurTrack );
				if( FaceFXTrack != NULL )
				{
					for( INT CurKeyIndex = 0; CurKeyIndex < FaceFXTrack->FaceFXSeqs.Num(); ++CurKeyIndex )
					{
						const FFaceFXTrackKey& CurKey = FaceFXTrack->FaceFXSeqs( CurKeyIndex );

						// Is this key's time within the range that we'll be skipping over due to the Force Start
						// Position being set to a later time, we'll warn the user about that!
						if( CurKey.StartTime < Interp->ForceStartPosition )
						{
							// One warning per track is plenty!
							bNeedWarning = TRUE;
							break;
						}
					}
				}


				if( bNeedWarning )
				{
					FString WarningString =
						FString::Printf( LocalizeSecure( LocalizeUnrealEd( "InterpEd_TrackKeyAffectedByForceStartPosition_F" ),
							*CurTrack->TrackTitle,						// Friendly track type title
							*CurGroup->GroupName.ToString(),			// Group name
							Interp->ForceStartPosition ) );				// Force start pos value

					ContentWarnings += WarningString;
				}
			}
		}
	}


	// Did we have any warning messages to display?
	if( ContentWarnings.Len() > 0 )
	{
		// Tracks are bound to an actor that is not configured to use interpolation physics!
		appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd( "InterpEdWarningList_F" ), *ContentWarnings ) );
	}


	// Set position to the start of the interpolation.
	// Will position objects as the first frame of the sequence.
	Interp->UpdateInterp(0.f, TRUE);

	CamViewGroup = NULL;

	bLoopingSection = FALSE;
	bDragging3DHandle = FALSE;

	PlaybackSpeed = 1.0f;
	PlaybackStartRealTime = 0.0;
	NumContinuousFixedTimeStepFrames = 0;


	// Update cam frustum colours.
	UpdateCamColours();

	// Setup property window
	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create( this, this );

	
	// Override level viewport audio settings for viewports previewing matinee.
	// Store the original state so we can restore it after matinee closes
	SetAudioRealtimeOverride( FALSE, TRUE );

	// Setup track windows
	{
		GraphSplitterWnd = new wxSplitterWindow(this, IDM_INTERP_GRAPH_SPLITTER, wxDefaultPosition, wxSize(100, 100), wxSP_3DBORDER|wxSP_FULLSASH );
		GraphSplitterWnd->SetMinimumPaneSize(104);

		// Setup track windows (separated by a splitter)
		TrackWindow = new WxInterpEdVCHolder( GraphSplitterWnd, -1, this );
		DirectorTrackWindow = new WxInterpEdVCHolder( GraphSplitterWnd, -1, this );

		// Load a default splitter position for the curve editor.
		const UBOOL bSuccess = GConfig->GetInt(TEXT("Matinee"), TEXT("SplitterPos"), GraphSplitPos, GEditorUserSettingsIni);
		if(bSuccess == FALSE)
		{
			GraphSplitPos = 104;
		}

		// Start off with the window unsplit, showing only the regular track window
		GraphSplitterWnd->Initialize( TrackWindow );
		TrackWindow->Show( TRUE );
		DirectorTrackWindow->Show( FALSE );

		// Setup track window defaults
		TrackWindow->InterpEdVC->bIsDirectorTrackWindow = FALSE;
		TrackWindow->InterpEdVC->bWantFilterTabs = TRUE;
		TrackWindow->InterpEdVC->bWantTimeline = TRUE;
		DirectorTrackWindow->InterpEdVC->bIsDirectorTrackWindow = TRUE;
	}


	// Create new curve editor setup if not already done
	if(!IData->CurveEdSetup)
	{
		IData->CurveEdSetup = ConstructObject<UInterpCurveEdSetup>( UInterpCurveEdSetup::StaticClass(), IData, NAME_None, RF_NotForClient | RF_NotForServer );
	}

	// Create graph editor to work on InterpData's CurveEd setup.
	CurveEd = new WxCurveEditor( this, -1, IData->CurveEdSetup );

	// Register this window with the Curve editor so we will be notified of various things.
	CurveEd->SetNotifyObject(this);

	// Set graph view to match track view.
	SyncCurveEdView();

	PosMarkerColor = PositionMarkerLineColor;
	RegionFillColor = LoopRegionFillColor;

	CurveEd->SetEndMarker(TRUE, IData->InterpLength);
	CurveEd->SetPositionMarker(TRUE, 0.f, PosMarkerColor);
	CurveEd->SetRegionMarker(TRUE, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);


	// Setup docked windows
	const wxString CurveEdWindowTitle( *LocalizeUnrealEd( "InterpEdCurveEditor" ) );
	{
		AddDockingWindow( CurveEd, FDockingParent::DH_Top, CurveEdWindowTitle.c_str(), NULL, wxSize(400,110) );
		AddDockingWindow(PropertyWindow, FDockingParent::DH_Bottom, *LocalizeUnrealEd("Properties"));

		SetDockHostSize( FDockingParent::DH_Left, 500 );

		AddDockingWindow( GraphSplitterWnd, FDockingParent::DH_None, NULL );

		// Load and apply docked window layout
		LoadDockingLayout();
	}

	// Create toolbar
	ToolBar = new WxInterpEdToolBar( this, -1 );
	SetToolBar( ToolBar );

	// Initialise snap settings.
	bSnapToKeys = FALSE;
	bSnapEnabled = FALSE;
	bSnapToFrames = FALSE;	
	bSnapTimeToFrames = FALSE;
	bFixedTimeStepPlayback = FALSE;
	bPreferFrameNumbers = TRUE;
	bShowTimeCursorPosForAllKeys = FALSE;

	// Load fixed time step setting
	GConfig->GetBool( TEXT("Matinee"), TEXT("FixedTimeStepPlayback"), bFixedTimeStepPlayback, GEditorUserSettingsIni );

	// Load 'prefer frame numbers' setting
	GConfig->GetBool( TEXT("Matinee"), TEXT("PreferFrameNumbers"), bPreferFrameNumbers, GEditorUserSettingsIni );

	// Load 'show time cursor pos for all keys' setting
	GConfig->GetBool( TEXT("Matinee"), TEXT("ShowTimeCursorPosForAllKeys"), bShowTimeCursorPosForAllKeys, GEditorUserSettingsIni );

	// Restore selected snap mode from INI.
	GConfig->GetBool( TEXT("Matinee"), TEXT("SnapEnabled"), bSnapEnabled, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("Matinee"), TEXT("SnapTimeToFrames"), bSnapTimeToFrames, GEditorUserSettingsIni );
	INT SelectedSnapMode = 3; // default 0.5 sec
	GConfig->GetInt(TEXT("Matinee"), TEXT("SelectedSnapMode"), SelectedSnapMode, GEditorUserSettingsIni );
	ToolBar->SnapCombo->SetSelection( SelectedSnapMode );
	wxCommandEvent In;
	In.SetInt(SelectedSnapMode);
	OnChangeSnapSize(In);

	// Update snap button & synchronize with curve editor
	SetSnapEnabled( bSnapEnabled );
	SetSnapTimeToFrames( bSnapTimeToFrames );
	SetFixedTimeStepPlayback( bFixedTimeStepPlayback );
	SetPreferFrameNumbers( bPreferFrameNumbers );
	SetShowTimeCursorPosForAllKeys( bShowTimeCursorPosForAllKeys );


	// We always default to Curve (Auto/Clamped) when we have no other settings
	InitialInterpMode = CIM_CurveAutoClamped;

	// Restore user's "initial curve interpolation mode" setting from their preferences file
	{
		// NOTE: InitialInterpMode now has a '2' suffix after a version bump to change the default
		INT DesiredInitialInterpMode = ( INT )InitialInterpMode;
		GConfig->GetInt( TEXT( "Matinee" ), TEXT( "InitialInterpMode2" ), DesiredInitialInterpMode, GEditorUserSettingsIni );

		// Update combo box
		ToolBar->InitialInterpModeComboBox->Select( DesiredInitialInterpMode );

		// Fire off a callback event for the combo box so that everything is properly refreshed
		wxCommandEvent In;
		In.SetInt( DesiredInitialInterpMode );
		OnChangeInitialInterpMode( In );
	}


	CurveEdToggleMenuItem = NULL;
	{
		// Create menu bar and 'Window' menu
		MenuBar = new WxInterpEdMenuBar(this);
		wxMenu* WindowMenu = AppendWindowMenu( MenuBar );
		SetMenuBar( MenuBar );

		// SetMenuBar should have caused our dockable WINDOWS to be added as checkable menu items to the Window menu,
		// so we'll go ahead and grab a pointer to the Curve Editor toggle menu item now.
		INT CurveEdToggleMenuItemIndex = WindowMenu->FindItem( CurveEdWindowTitle );
		CurveEdToggleMenuItem = WindowMenu->FindItem( CurveEdToggleMenuItemIndex );

		// Update button status
		bool bCurveEditorVisible = TRUE;		
		if( CurveEdToggleMenuItem != NULL )
		{
			bCurveEditorVisible = CurveEdToggleMenuItem->IsChecked();
		}
		ToolBar->ToggleTool( IDM_INTERP_TOGGLE_CURVEEDITOR, bCurveEditorVisible );
		MenuBar->ViewMenu->Check( IDM_INTERP_TOGGLE_CURVEEDITOR, bCurveEditorVisible );
	}

	// Will look at current selection to set active track
	ActorSelectionChange();

	// Load gradient texture for bars
	BarGradText = LoadObject<UTexture2D>(NULL, TEXT("EditorMaterials.MatineeGreyGrad"), NULL, LOAD_None, NULL);

	// If there is a Director group in this data, default to locking the camera to it.
	UInterpGroupDirector* DirGroup = IData->FindDirectorGroup();
	if(DirGroup)
	{
		LockCamToGroup(DirGroup);
	}

	for(INT i=0; i<GApp->EditorFrame->ViewportConfigData->GetViewportCount(); i++)
	{
		WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->AccessViewport(i).ViewportWindow;
		if(LevelVC)
		{
			// If there is a director group, set the perspective viewports to realtime automatically.
			if(LevelVC->ViewportType == LVT_Perspective && LevelVC->AllowMatineePreview() )
			{				
				LevelVC->SetRealtime(TRUE);
			}

			// Turn on 'show camera frustums' flag
			LevelVC->ShowFlags |= SHOW_CamFrustums;
		}
	}

	// Refresh any properties windows that are open, as the settings have been cached
	GCallbackEvent->Send( CALLBACK_RefreshPropertyWindows );

	// Update UI to reflect any change in realtime status
	GCallbackEvent->Send( CALLBACK_UpdateUI );

	// Load the desired window position from .ini file
	FWindowUtil::LoadPosSize(TEXT("Matinee"), this, -1, -1, 885, 800);


	// OK, we're now initialized!
	bIsInitialized = TRUE;


	// Make sure any particle replay tracks are filled in with the correct state
	UpdateParticleReplayTracks();

	// Update visibility of director track window
	UpdateDirectorTrackWindowVisibility();
	
	// Now that we've filled in the track window's contents, reconfigure our scroll bar
	UpdateTrackWindowScrollBars();

	// register for any actor move change
	GCallbackEvent->Register(CALLBACK_OnActorMoved,this);
}

WxInterpEd::~WxInterpEd()
{
	// Viewport config data may be null if we are shutting down matinee as a result of closing the editor
	if( GApp->EditorFrame->ViewportConfigData )
	{
		for(INT i=0; i<GApp->EditorFrame->ViewportConfigData->GetViewportCount(); i++)
		{
			WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->AccessViewport(i).ViewportWindow;
			if(LevelVC && LevelVC->ViewportType == LVT_Perspective && LevelVC->AllowMatineePreview() )
			{
				RestoreLevelViewport(LevelVC);
			}
		}
	}

#if WITH_MANAGED_CODE
	UnBindColorPickers(this);
#endif
}

/**
 * This function is called when the window has been selected from within the ctrl + tab dialog.
 */
void WxInterpEd::OnSelected()
{
	Raise();
}

void WxInterpEd::Serialize(FArchive& Ar)
{
	Ar << Interp;
	Ar << IData;
	Ar << NormalTransactor;
	Ar << Opt;

	// Check for non-NULL, as these references will be cleared in OnClose.
	if ( CurveEd )
	{
		CurveEd->CurveEdVC->Serialize(Ar);
	}
	if ( TrackWindow != NULL && TrackWindow->InterpEdVC != NULL )
	{
		TrackWindow->InterpEdVC->Serialize(Ar);
	}
	if ( DirectorTrackWindow != NULL && DirectorTrackWindow->InterpEdVC != NULL )
	{
		DirectorTrackWindow->InterpEdVC->Serialize(Ar);
	}
}


/** 
 * Starts playing the current sequence. 
 * @param bPlayLoop		Whether or not we should play the looping section.
 * @param bPlayForward	TRUE if we should play forwards, or FALSE for reverse
 */
void WxInterpEd::StartPlaying( UBOOL bPlayLoop, UBOOL bPlayForward )
{
	// Allow audio to play in realtime
	SetAudioRealtimeOverride( TRUE );

	//make sure to turn off recording
	StopRecordingInterpValues();

	// Were we already in the middle of playback?
	const UBOOL bWasAlreadyPlaying = Interp->bIsPlaying;

	Opt->bAdjustingKeyframe = FALSE;
	Opt->bAdjustingGroupKeyframes = FALSE;

	bLoopingSection = bPlayLoop;
	//if looping or the marker is already at the end of the section.
	if( bLoopingSection )
	{
		// If looping - jump to start of looping section.
		SetInterpPosition( IData->EdSectionStart );
	}

	// Start playing if we need to
	if( !bWasAlreadyPlaying )
	{
		// If 'snap time to frames' or 'fixed time step playback' is turned on, we'll make sure that we
		// start playback exactly on the closest frame
		if( bSnapToFrames && ( bSnapTimeToFrames || bFixedTimeStepPlayback ) )
		{
			SetInterpPosition( SnapTimeToNearestFrame( Interp->Position ) );
		}

		// Start playing
		Interp->bIsPlaying = TRUE;
		Interp->bReversePlayback = !bPlayForward;

		// Remember the real-time that we started playing the sequence
		PlaybackStartRealTime = appSeconds();
		NumContinuousFixedTimeStepFrames = 0;

		// Switch the Matinee windows to real-time so the track editor and curve editor update during playback
		TrackWindow->InterpEdVC->SetRealtime( TRUE );
		if( DirectorTrackWindow->IsShown() )
		{
			DirectorTrackWindow->InterpEdVC->SetRealtime( TRUE );
		}
	}
	else
	{
		// Switch playback directions if we need to
		if( Interp->bReversePlayback == bPlayForward )
		{
			Interp->ChangeDirection();

			// Reset our playback start time so fixed time step playback can gate frame rate properly
			PlaybackStartRealTime = appSeconds();
			NumContinuousFixedTimeStepFrames = 0;
		}
	}

	// Make sure fixed time step mode is set correctly based on whether we're currently 'playing' or not
	UpdateFixedTimeStepPlayback();
}

/** Stops playing the current sequence. */
void WxInterpEd::StopPlaying()
{
	// Stop audio from playing in realtime
	SetAudioRealtimeOverride( FALSE );

	//make sure to turn off recording
	StopRecordingInterpValues();

	// If already stopped, pressing stop again winds you back to the beginning.
	if(!Interp->bIsPlaying)
	{
		SetInterpPosition(0.f);
		return;
	}

	// Iterate over each group/track giving it a chance to stop things.
	for(INT i=0; i<Interp->GroupInst.Num(); i++)
	{
		UInterpGroupInst* GrInst = Interp->GroupInst(i);
		UInterpGroup* Group = GrInst->Group;

		check(Group->InterpTracks.Num() == GrInst->TrackInst.Num());
		for(INT j=0; j<Group->InterpTracks.Num(); j++)
		{
			UInterpTrack* Track = Group->InterpTracks(j);
			UInterpTrackInst* TrInst = GrInst->TrackInst(j);

			Track->PreviewStopPlayback(TrInst);
		}
	}

	// Set flag to indicate stopped
	Interp->bIsPlaying = FALSE;

	// Stop viewport being realtime
	TrackWindow->InterpEdVC->SetRealtime( FALSE );
	DirectorTrackWindow->InterpEdVC->SetRealtime( FALSE );

	// If the 'snap time to frames' option is enabled, we'll need to snap the time cursor position to
	// the nearest frame
	if( bSnapToFrames && bSnapTimeToFrames )
	{
		SetInterpPosition( SnapTimeToNearestFrame( Interp->Position ) );
	}

	// Make sure fixed time step mode is set correctly based on whether we're currently 'playing' or not
	UpdateFixedTimeStepPlayback();
}

/** Starts recording the current sequence */
void WxInterpEd::StartRecordingMovie()
{
	// Figure out which console support container represents the PC platform
	INT PCSupportContainerIndex = INDEX_NONE;
	const INT ConsoleCount = FConsoleSupportContainer::GetConsoleSupportContainer()->GetNumConsoleSupports();
	for( INT CurConsoleIndex = 0; CurConsoleIndex < ConsoleCount; ++CurConsoleIndex )
	{
		FConsoleSupport* Console = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport( CurConsoleIndex );
		if( Console != NULL && appStricmp( Console->GetPlatformName(), CONSOLESUPPORT_NAME_PC ) == 0 )
		{
			PCSupportContainerIndex = CurConsoleIndex;
			break;
		}
	}

	if (PCSupportContainerIndex != INDEX_NONE)
	{
		GUnrealEd->PlayMap(NULL, NULL, PCSupportContainerIndex, -1, FALSE, TRUE);
	}
}


/** Creates a popup context menu based on the item under the mouse cursor.
* @param	Viewport	FViewport for the FInterpEdViewportClient.
* @param	HitResult	HHitProxy returned by FViewport::GetHitProxy( ).
* @return	A new wxMenu with context-appropriate menu options or NULL if there are no appropriate menu options.
*/
wxMenu	*WxInterpEd::CreateContextMenu( FViewport *Viewport, const HHitProxy *HitResult )
{
	wxMenu	*Menu = NULL;

	if(HitResult->IsA(HInterpEdTrackBkg::StaticGetType()))
	{
		DeselectAll();				

		Menu = new WxMBInterpEdBkgMenu( this );
	}
	else if(HitResult->IsA(HInterpEdGroupTitle::StaticGetType()))
	{
		UInterpGroup* Group = ((HInterpEdGroupTitle*)HitResult)->Group;

		if( !IsGroupSelected(Group) )
		{
			SelectGroup(Group);
		}

		Menu = new WxMBInterpEdGroupMenu( this );		
	}
	else if(HitResult->IsA(HInterpEdTrackTitle::StaticGetType()))
	{
		HInterpEdTrackTitle* TrackProxy = ((HInterpEdTrackTitle*)HitResult);
		UInterpGroup* Group = TrackProxy->Group;
		UInterpTrack* TrackToSelect = TrackProxy->Track;

		check( TrackToSelect );

		if( !TrackToSelect->bIsSelected )
		{
			SelectTrack( Group, TrackToSelect );
		}

		// Dont allow subtracks to have a menu as this could cause the ability to copy/paste subtracks which would be bad
		if( TrackToSelect->GetOuter()->IsA( UInterpGroup::StaticClass() ) )
		{
			Menu = new WxMBInterpEdTrackMenu( this );
		}
	}
	else if(HitResult->IsA(HInterpTrackKeypointProxy::StaticGetType()))
	{
		HInterpTrackKeypointProxy* KeyProxy = ((HInterpTrackKeypointProxy*)HitResult);
		UInterpGroup* Group = KeyProxy->Group;
		UInterpTrack* Track = KeyProxy->Track ;
		const INT KeyIndex = KeyProxy->KeyIndex;

		const UBOOL bAlreadySelected = KeyIsInSelection(Group, Track, KeyIndex);
		if(bAlreadySelected)
		{
			Menu = new WxMBInterpEdKeyMenu( this );
		}
	}
	else if(HitResult->IsA(HInterpEdTab::StaticGetType()))
	{
		UInterpFilter* Filter = ((HInterpEdTab*)HitResult)->Filter;

		SetSelectedFilter(Filter);
		Viewport->Invalidate();

		Menu = new WxMBInterpEdTabMenu( this );
	}
	else if(HitResult->IsA(HInterpEdGroupCollapseBtn::StaticGetType()))
	{
		// Use right-clicked on the 'Expand/Collapse' track editor widget for a group
		Menu = new WxMBInterpEdCollapseExpandMenu( this );
	}
	else if(HitResult->IsA(HInterpEdMarker::StaticGetType()))
	{
		GrabbedMarkerType = ((HInterpEdMarker*)HitResult)->Type;

		// Don't create a context menu for the sequence 
		// start marker because it should not be moved.
		if( GrabbedMarkerType != ISM_SeqStart )
		{
			Menu = new WxMBInterpEdMarkerMenu( this, GrabbedMarkerType );
		}
	}

	return Menu;
}

/**Preps Matinee to record/stop-recording realtime camera movement*/
void WxInterpEd::ToggleRecordInterpValues(void)
{
	//if we're already sampling, just stop sampling
	if (RecordingState != MatineeConstants::RECORDING_COMPLETE)
	{
		StopRecordingInterpValues();
	}
	else
	{
		RecordingState = MatineeConstants::RECORDING_GET_READY_PAUSE;
		RecordingStateStartTime = appSeconds();

		InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("RecordTracks") );

#if WITH_MANAGED_CODE
		MatineeWindows::FocusRecordWindow();
#endif //WITH_MANAGED_CODE

		// Stop time if it's playing.
		Interp->Stop();
		// Move to proper start time
		SetInterpPosition(GetRecordingStartTime());
	}
}

/**Helper function to properly shut down matinee recording*/
void WxInterpEd::StopRecordingInterpValues(void)
{
	if (RecordingState != MatineeConstants::RECORDING_COMPLETE)
	{
		//STOP SAMPLING!!!
		RecordingState = MatineeConstants::RECORDING_COMPLETE;

		for (INT i = 0; i < RecordingTracks.Num(); ++i)
		{
			RecordingTracks(i)->bIsRecording = FALSE;
		}

		//Clear recording tracks
		RecordingTracks.Empty();

		InterpEdTrans->EndSpecial();

		// Stop time if it's playing.
		Interp->Stop();
		// Move to proper start time
		SetInterpPosition(GetRecordingStartTime());
	}
}

/**
 * Increments or decrements the currently selected recording menu item
 * @param bInNext - TRUE if going forward in the menu system, FALSE if going backward
 */
void WxInterpEd::ChangeRecordingMenu(const UBOOL bInNext)
{
	RecordMenuSelection += (bInNext ? 1 : -1);
	if (RecordMenuSelection < 0)
	{
		RecordMenuSelection = MatineeConstants::NUM_RECORD_MENU_ITEMS-1;
	} 
	else if (RecordMenuSelection == MatineeConstants::NUM_RECORD_MENU_ITEMS)
	{
		RecordMenuSelection = 0;
	}
}

/**
 * Increases or decreases the recording menu value
 * @param bInIncrease - TRUE if increasing the value, FALSE if decreasing the value
 */
void WxInterpEd::ChangeRecordingMenuValue(FEditorLevelViewportClient* InClient, const UBOOL bInIncrease)
{
	check(InClient);
	FEditorCameraController* CameraController = InClient->GetCameraController();
	check(CameraController);
	FCameraControllerConfig CameraConfig = CameraController->GetConfig(); 

	FLOAT DecreaseMultiplier = .99f;
	FLOAT IncreaseMultiplier = 1.0f / DecreaseMultiplier;

	switch (RecordMenuSelection)
	{
		case MatineeConstants::RECORD_MENU_RECORD_MODE:
			RecordMode += (bInIncrease ? 1 : -1);
			if (RecordMode < 0)
			{
				RecordMode = MatineeConstants::NUM_RECORD_MODES-1;
			} 
			else if (RecordMode == MatineeConstants::NUM_RECORD_MODES)
			{
				RecordMode = 0;
			}
			break;
		case MatineeConstants::RECORD_MENU_TRANSLATION_SPEED:
			CameraConfig.TranslationMultiplier *= (bInIncrease ? IncreaseMultiplier : DecreaseMultiplier);
			break;
		case MatineeConstants::RECORD_MENU_ROTATION_SPEED:
			CameraConfig.RotationMultiplier *= (bInIncrease ? IncreaseMultiplier : DecreaseMultiplier);
			break;
		case MatineeConstants::RECORD_MENU_ZOOM_SPEED:
			CameraConfig.ZoomMultiplier*= (bInIncrease ? IncreaseMultiplier : DecreaseMultiplier);
			break;
		case MatineeConstants::RECORD_MENU_TRIM:
			CameraConfig.PitchTrim += (bInIncrease ? 0.2f : -0.2f );
			break;
		case MatineeConstants::RECORD_MENU_INVERT_X_AXIS:
			CameraConfig.bInvertX = !CameraConfig.bInvertX;
			break;
		case MatineeConstants::RECORD_MENU_INVERT_Y_AXIS:
			CameraConfig.bInvertY = !CameraConfig.bInvertY;
			break;
		case MatineeConstants::RECORD_MENU_ROLL_SMOOTHING:
			RecordRollSmoothingSamples += (bInIncrease ? 1 : -1);
			if (RecordRollSmoothingSamples < 1)
			{
				RecordRollSmoothingSamples = MatineeConstants::MaxSmoothingSamples-1;
			} 
			else if (RecordRollSmoothingSamples == MatineeConstants::MaxSmoothingSamples)
			{
				RecordRollSmoothingSamples = 1;
			}
			break;
		case MatineeConstants::RECORD_MENU_PITCH_SMOOTHING:
			RecordPitchSmoothingSamples += (bInIncrease ? 1 : -1);
			if (RecordPitchSmoothingSamples < 1)
			{
				RecordPitchSmoothingSamples = MatineeConstants::MaxSmoothingSamples-1;
			} 
			else if (RecordPitchSmoothingSamples == MatineeConstants::MaxSmoothingSamples)
			{
				RecordPitchSmoothingSamples = 1;
			}
			break;
		case MatineeConstants::RECORD_MENU_CAMERA_MOVEMENT_SCHEME:
			RecordCameraMovementScheme += (bInIncrease ? 1 : -1);
			if (RecordCameraMovementScheme < 0)
			{
				RecordCameraMovementScheme = MatineeConstants::NUM_CAMERA_SCHEMES-1;
			} 
			else if (RecordCameraMovementScheme == MatineeConstants::NUM_CAMERA_SCHEMES)
			{
				RecordCameraMovementScheme = 0;
			}
			break;
		case MatineeConstants::RECORD_MENU_ZOOM_DISTANCE:
			{
				FEditorLevelViewportClient* LevelVC = GetRecordingViewport();
				if (LevelVC)
				{
					LevelVC->ViewFOV += (bInIncrease ? 5.0f : -5.0f);
				}
			}
			break;
	}

	SaveRecordingSettings(CameraConfig);

	CameraController->SetConfig(CameraConfig); 
}

/**
 * Resets the recording menu value to the default
 */
void WxInterpEd::ResetRecordingMenuValue(FEditorLevelViewportClient* InClient)
{
	check(InClient);
	FEditorCameraController* CameraController = InClient->GetCameraController();
	check(CameraController);
	FCameraControllerConfig CameraConfig = CameraController->GetConfig(); 

	switch (RecordMenuSelection)
	{
		case MatineeConstants::RECORD_MENU_RECORD_MODE:
			RecordMode = 0;
			break;
		case MatineeConstants::RECORD_MENU_TRANSLATION_SPEED:
			CameraConfig.TranslationMultiplier = 1.0f;
			break;
		case MatineeConstants::RECORD_MENU_ROTATION_SPEED:
			CameraConfig.RotationMultiplier = 1.0f;
			break;
		case MatineeConstants::RECORD_MENU_ZOOM_SPEED:
			CameraConfig.ZoomMultiplier = 1.0f;
			break;
		case MatineeConstants::RECORD_MENU_TRIM:
			CameraConfig.PitchTrim = 0.0f;
			break;
		case MatineeConstants::RECORD_MENU_INVERT_X_AXIS:
			CameraConfig.bInvertX = FALSE;
			break;
		case MatineeConstants::RECORD_MENU_INVERT_Y_AXIS:
			CameraConfig.bInvertY = FALSE;
			break;
		case MatineeConstants::RECORD_MENU_ROLL_SMOOTHING:
			RecordRollSmoothingSamples = 1;
			break;
		case MatineeConstants::RECORD_MENU_PITCH_SMOOTHING:
			RecordPitchSmoothingSamples = 1;
			break;
		case MatineeConstants::RECORD_MENU_CAMERA_MOVEMENT_SCHEME:
			RecordCameraMovementScheme = MatineeConstants::CAMERA_SCHEME_FREE_CAM;
			break;
		case MatineeConstants::RECORD_MENU_ZOOM_DISTANCE:
			{
				FEditorLevelViewportClient* LevelVC = GetRecordingViewport();
				if (LevelVC)
				{
					LevelVC->ViewFOV = 90.0f;
				}
			}
			break;
	}

	SaveRecordingSettings(CameraConfig);

	CameraController->SetConfig(CameraConfig); 
}

/**
 * Determines whether only the first click event is allowed or all repeat events are allowed
 * @return - TRUE, if the value should change multiple times.  FALSE, if the user should have to release and reclick
 */
UBOOL WxInterpEd::IsRecordMenuChangeAllowedRepeat (void) const 
{
	UBOOL bAllowRepeat = TRUE;
	switch (RecordMenuSelection)
	{
		case MatineeConstants::RECORD_MENU_RECORD_MODE:
		case MatineeConstants::RECORD_MENU_INVERT_X_AXIS:
		case MatineeConstants::RECORD_MENU_INVERT_Y_AXIS:
		case MatineeConstants::RECORD_MENU_ROLL_SMOOTHING:
		case MatineeConstants::RECORD_MENU_PITCH_SMOOTHING:
		case MatineeConstants::RECORD_MENU_CAMERA_MOVEMENT_SCHEME:
		case MatineeConstants::RECORD_MENU_ZOOM_DISTANCE:
			bAllowRepeat = FALSE;
			break;
		default:
			break;
	}

	return bAllowRepeat;
}


/** Sets the record mode for matinee */
void WxInterpEd::SetRecordMode(const UINT InNewMode)
{
	check (IsWithin<UINT>(InNewMode, 0, MatineeConstants::NUM_RECORD_MODES));
	RecordMode = InNewMode;
}


/**If TRUE, real time camera recording mode has been enabled*/
UBOOL WxInterpEd::IsRecordingInterpValues (void) const
{
	return (RecordingState != MatineeConstants::RECORDING_COMPLETE);
}

/** Returns The time that sampling should start at */
const DOUBLE WxInterpEd::GetRecordingStartTime (void) const
{
	if (IData->EdSectionStart == IData->EdSectionEnd)
	{
		return 0.0;
	}
	return (IData->EdSectionStart);
}

/** Returns The time that sampling should end at */
const DOUBLE WxInterpEd::GetRecordingEndTime (void) const
{
	if (IData->EdSectionStart == IData->EdSectionEnd)
	{
		return IData->InterpLength;
	}
	return (IData->EdSectionEnd);
}

/** Save record settings for next run */
void WxInterpEd::SaveRecordingSettings(const FCameraControllerConfig& InCameraConfig)
{
	GConfig->SetInt(TEXT("InterpEd.Recording"), TEXT("Mode"), RecordMode, GEditorUserSettingsIni);

	GConfig->SetFloat(TEXT("InterpEd.Recording"), TEXT("TranslationSpeed"), InCameraConfig.TranslationMultiplier, GEditorUserSettingsIni);
	GConfig->SetFloat(TEXT("InterpEd.Recording"), TEXT("RotationSpeed"), InCameraConfig.RotationMultiplier, GEditorUserSettingsIni);
	GConfig->SetFloat(TEXT("InterpEd.Recording"), TEXT("ZoomSpeed"), InCameraConfig.ZoomMultiplier, GEditorUserSettingsIni);

	GConfig->SetBool(TEXT("InterpEd.Recording"), TEXT("InvertX"), InCameraConfig.bInvertX, GEditorUserSettingsIni);
	GConfig->SetBool(TEXT("InterpEd.Recording"), TEXT("InvertY"), InCameraConfig.bInvertY, GEditorUserSettingsIni);
	
	GConfig->SetInt(TEXT("InterpEd.Recording"), TEXT("RollSamples"), RecordRollSmoothingSamples, GEditorUserSettingsIni);
	GConfig->SetInt(TEXT("InterpEd.Recording"), TEXT("PitchSamples"), RecordPitchSmoothingSamples, GEditorUserSettingsIni);

	GConfig->SetInt(TEXT("InterpEd.Recording"), TEXT("CameraMovement"), RecordCameraMovementScheme, GEditorUserSettingsIni);

	FEditorLevelViewportClient* LevelVC = GetRecordingViewport();
	if (LevelVC)
	{
		GConfig->SetFloat(TEXT("InterpEd.Recording"), TEXT("ZoomDistance"), LevelVC->ViewFOV, GEditorUserSettingsIni);
	}
}

/** Load record settings for next run */
void WxInterpEd::LoadRecordingSettings(FCameraControllerConfig& InCameraConfig)
{
	GConfig->GetInt(TEXT("InterpEd.Recording"), TEXT("Mode"), RecordMode, GEditorUserSettingsIni);

	GConfig->GetFloat(TEXT("InterpEd.Recording"), TEXT("TranslationSpeed"), InCameraConfig.TranslationMultiplier, GEditorUserSettingsIni);
	GConfig->GetFloat(TEXT("InterpEd.Recording"), TEXT("RotationSpeed"), InCameraConfig.RotationMultiplier, GEditorUserSettingsIni);
	GConfig->GetFloat(TEXT("InterpEd.Recording"), TEXT("ZoomSpeed"), InCameraConfig.ZoomMultiplier, GEditorUserSettingsIni);

	GConfig->GetBool(TEXT("InterpEd.Recording"), TEXT("InvertX"), InCameraConfig.bInvertX, GEditorUserSettingsIni);
	GConfig->GetBool(TEXT("InterpEd.Recording"), TEXT("InvertY"), InCameraConfig.bInvertY, GEditorUserSettingsIni);

	GConfig->GetInt(TEXT("InterpEd.Recording"), TEXT("RollSamples"), RecordRollSmoothingSamples, GEditorUserSettingsIni);
	GConfig->GetInt(TEXT("InterpEd.Recording"), TEXT("PitchSamples"), RecordPitchSmoothingSamples, GEditorUserSettingsIni);

	GConfig->GetInt(TEXT("InterpEd.Recording"), TEXT("CameraMovement"), RecordCameraMovementScheme, GEditorUserSettingsIni);

	FEditorLevelViewportClient* LevelVC = GetRecordingViewport();
	if (LevelVC)
	{
		GConfig->GetFloat(TEXT("InterpEd.Recording"), TEXT("ZoomDistance"), LevelVC->ViewFOV, GEditorUserSettingsIni);
	}
}

/**
 * Access function to appropriate camera actor
 * @param InCameraIndex - The index of the camera actor to return
 * 
 */
ACameraActor* WxInterpEd::GetCameraActor(const INT InCameraIndex)
{
	//quick early out
	if (InCameraIndex >= 0)
	{
		INT CurrentCameraIndex = 0;
		for( TArray<UInterpGroupInst*>::TIterator GroupInstIt(Interp->GroupInst); GroupInstIt; ++GroupInstIt )
		{
			UInterpGroupInst* Inst = *GroupInstIt;
			AActor* TempActor = Inst->GetGroupActor();
			if (TempActor)
			{
				ACameraActor* TempCameraActor = Cast<ACameraActor>(TempActor);
				if (TempCameraActor != NULL)
				{
					if (CurrentCameraIndex == InCameraIndex)
					{
						return TempCameraActor;
					}
					CurrentCameraIndex++;
				}
			}
		}
	}

	return NULL;
}

/**
 * Access function to return the number of used camera actors
 */
INT WxInterpEd::GetNumCameraActors(void) const
{
	INT CameraCount = 0;

	for( TArray<UInterpGroupInst*>::TIterator GroupInstIt(Interp->GroupInst); GroupInstIt; ++GroupInstIt )
	{
		UInterpGroupInst* Inst = *GroupInstIt;
		AActor* TempActor = Inst->GetGroupActor();
		if (Cast<ACameraActor>(TempActor))
		{
			CameraCount++;
		}
	}
	return CameraCount;
}

/**
 * Adds extra non-wx viewport
 * @param InViewport - The viewport to update
 * @param InActor - The actor the viewport is supposed to follow
 */
void WxInterpEd::AddExtraViewport(FEditorLevelViewportClient* InViewport, AActor* InActor)
{
	ExtraViewports.Set(InViewport, InActor);
}



void WxInterpEd::OnSize( wxSizeEvent& In )
{
	In.Skip();
}

void WxInterpEd::OnClose( wxCloseEvent& In )
{
	// Unregister call back events
	GCallbackEvent->UnregisterAll( this );

#if WITH_MANAGED_CODE
	MatineeWindows::CloseDirectorWindow(this);
	MatineeWindows::CloseRecordWindow(this);
#endif
	
	// Restore the perspective viewport audio settings when matinee closes.
	RestoreAudioRealtimeOverride();

	// Re-instate regular transactor
	check( GEditor->Trans == InterpEdTrans );
	check( NormalTransactor->IsA( UTransBuffer::StaticClass() ) );

	GEditor->ResetTransaction( *LocalizeUnrealEd("ExitMatinee") );
	GEditor->Trans = NormalTransactor;

	// Detach editor camera from any group and clear any previewing stuff.
	LockCamToGroup(NULL);

	// Restore the saved state of any actors we were previewing interpolation on.
	for(INT i=0; i<Interp->GroupInst.Num(); i++)
	{
		// if AI Group, I need to de-select previewpawn before it gets destroyed
		// I can't do this in uninterpolation.cpp due to Editor/Engine independency
		if ( Interp->GroupInst(i)->IsA(UInterpGroupInstAI::StaticClass()) )
		{
			AActor * GrActor = Interp->GroupInst(i)->GetGroupActor();
			
			if ( GrActor && GrActor->IsSelected())
			{
				GEditor->GetSelectedActors()->Deselect(GrActor);
			}

			// if I don't update property window, it will keep the actor pointer even after actor is deleted
			GUnrealEd->UpdatePropertyWindows();
		}

		// Restore Actors to the state they were in when we opened Matinee.
		Interp->GroupInst(i)->RestoreGroupActorState();

		// Call TermTrackInst, but don't actually delete them. Leave them for GC.
		// Because we don't delete groups/tracks so undo works, we could end up deleting the Outer of a valid object.
		Interp->GroupInst(i)->TermGroupInst(FALSE);

		// Set any manipulated cameras back to default frustum colours.
		ACameraActor* Cam = Cast<ACameraActor>(Interp->GroupInst(i)->GroupActor);
		if(Cam && Cam->DrawFrustum)
		{
			ACameraActor* DefCam = (ACameraActor*)(Cam->GetClass()->GetDefaultActor());
				Cam->DrawFrustum->FrustumColor = DefCam->DrawFrustum->FrustumColor;
			}
	}

	// Restore the bHidden state of all actors with visibility tracks
	Interp->RestoreActorVisibilities();

	// Movement tracks no longer save/restore relative actor positions.  Instead, the SeqAct_interp
	// stores actor positions/orientations so they can be precisely restored on Matinee close.
	// Note that this must happen before Interp's GroupInst array is emptied.
	Interp->RestoreActorTransforms();

	DeselectAllGroups(FALSE);
	DeselectAllTracks(FALSE);

	Interp->GroupInst.Empty();
	Interp->InterpData = NULL;

	// Indicate action is no longer being edited.
	Interp->bIsBeingEdited = FALSE;

	// Reset interpolation to the beginning when quitting.
	Interp->bIsPlaying = FALSE;
	Interp->Position = 0.f;

	Opt->bAdjustingKeyframe = FALSE;
	Opt->bAdjustingGroupKeyframes = FALSE;

	// When they close the window - change the mode away from InterpEdit.
	if( GEditorModeTools().IsModeActive( EM_InterpEdit ) )
	{
		FEdModeInterpEdit* InterpEditMode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );

		// Only change mode if this window closing wasn't instigated by someone changing mode!
		if( !InterpEditMode->bLeavingMode )
		{
			InterpEditMode->InterpEd = NULL;
			GEditorModeTools().DeactivateMode( EM_InterpEdit );
		}
	}

	// Undo any weird settings to editor level viewports.
	for(INT i=0; i<GApp->EditorFrame->ViewportConfigData->GetViewportCount(); i++)
	{
		WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->AccessViewport(i).ViewportWindow;
		if(LevelVC)
		{
			// Turn off realtime when exiting.
			if(LevelVC->ViewportType == LVT_Perspective && LevelVC->AllowMatineePreview() )
			{				
				LevelVC->SetRealtime(FALSE);
			}

			// Turn off 'show camera frustums' flag.
			LevelVC->ShowFlags &= ~SHOW_CamFrustums;
		}
	}

	// Un-highlight selected track.
	if( HasATrackSelected() )
	{
		for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
		{
			IData->CurveEdSetup->ChangeCurveColor(*TrackIt, TrackIt.GetGroup()->GroupColor);
		}
	}


	// Make sure benchmarking mode is disabled (we may have turned it on for 'fixed time step playback')
	GIsBenchmarking = FALSE;

	// Refresh any properties windows that are open, as the settings have been reverted
	GCallbackEvent->Send( CALLBACK_RefreshPropertyWindows );

	// Update UI to reflect any change in realtime status
	GCallbackEvent->Send( CALLBACK_UpdateUI );

	// Redraw viewport as well, to show reset state of stuff.
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );

	// Save window position/size
	FWindowUtil::SavePosSize(TEXT("Matinee"), this);

	// Save the default splitter position for the curve editor.
	GConfig->SetInt(TEXT("Matinee"), TEXT("SplitterPos"), GraphSplitPos, GEditorUserSettingsIni);

	// Save docking layout.
	SaveDockingLayout();


	// Make sure our dock windows are unbound since we're about to destroy them
	UnbindDockingWindow( PropertyWindow );
	UnbindDockingWindow( CurveEd );

	// Kill our dockable windows, so that we don't end up trying to draw it's (possibly floating) viewport
	// again after the InterpEd is gone
	PropertyWindow->Destroy();
	PropertyWindow = NULL;
	CurveEd->Destroy();
	CurveEd = NULL;

	// Clear references to serialized members so they won't be serialized in the time
	// between the window closing and wx actually deleting it.
	bClosed = TRUE;
	Interp = NULL;
	IData = NULL;
	NormalTransactor = NULL;
	Opt = NULL;
	CurveEd = NULL;

	// Destroy the viewport, disassociating it from the engine, etc.
	TrackWindow->DestroyViewport();
	TrackWindow = NULL;
	DirectorTrackWindow->DestroyViewport();
	DirectorTrackWindow = NULL;

	// Destroy the window.
	this->Destroy();
}


/** Handle scrolling */
void WxInterpEd::OnScroll(wxScrollEvent& In)
{
	wxScrollBar* InScrollBar = wxDynamicCast(In.GetEventObject(), wxScrollBar);
	if (InScrollBar && DirectorTrackWindow != NULL && TrackWindow != NULL ) 
	{
		// Figure out which scroll bar has changed
		UBOOL bIsDirectorTrackWindow = ( InScrollBar == DirectorTrackWindow->ScrollBar_Vert );

		if( bIsDirectorTrackWindow )
		{
			DirectorTrackWindow->InterpEdVC->ThumbPos_Vert = -In.GetPosition();

			if( DirectorTrackWindow->IsShown() )
			{
				if (DirectorTrackWindow->InterpEdVC->Viewport)
				{
					// Force it to draw so the view change is seen
					DirectorTrackWindow->InterpEdVC->Viewport->Invalidate();
					DirectorTrackWindow->InterpEdVC->Viewport->Draw();
				}
			}
		}
		else
		{
			TrackWindow->InterpEdVC->ThumbPos_Vert = -In.GetPosition();

			if (TrackWindow->InterpEdVC->Viewport)
			{
				// Force it to draw so the view change is seen
				TrackWindow->InterpEdVC->Viewport->Invalidate();
				TrackWindow->InterpEdVC->Viewport->Draw();
			}
		}
	}
}

void WxInterpEd::DrawTracks3D(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	for(INT i=0; i<Interp->GroupInst.Num(); i++)
	{
		UInterpGroupInst* GrInst = Interp->GroupInst(i);
		check( GrInst->Group );
		check( GrInst->TrackInst.Num() == GrInst->Group->InterpTracks.Num() );

		// In 3D viewports, Don't draw path if we are locking the camera to this group.
		//if( !(View->Family->ShowFlags & SHOW_Orthographic) && (Group == CamViewGroup) )
		//	continue;

		for(INT j=0; j<GrInst->TrackInst.Num(); j++)
		{
			UInterpTrackInst* TrInst = GrInst->TrackInst(j);
			UInterpTrack* Track = GrInst->Group->InterpTracks(j);

			//don't draw disabled tracks
			if ( Track->IsDisabled() )
			{
				continue;
			}

			UBOOL bTrackSelected = Track->bIsSelected;
			FColor TrackColor = bTrackSelected ? Track3DSelectedColor : GrInst->Group->GroupColor;

			Track->Render3DTrack( TrInst, View, PDI, j, TrackColor, Opt->SelectedKeys);
		}
	}
}

/** 
 * Draws a line with a 1 pixel dark border around it
 * 
 * @param Canvas	The canvas to draw on
 * @param Start		The start of the line
 * @param End		The end of the line
 * @param bVertical	TRUE if the line is vertical, FALSE if horizontal
 */
static void DrawShadowedLine( FCanvas* Canvas, const FVector2D& Start, const FVector2D& End, UBOOL bVertical )
{
	// This method uses DrawTile instead of DrawLine because DrawLine does not support alpha.
	if( bVertical )
	{
		DrawTile( Canvas, Start.X-1.0f, Start.Y, 1, Start.Y+End.Y-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(0.0,0.0f,0.0f,0.50f) );
		DrawTile( Canvas, Start.X, Start.Y, 1, Start.Y+End.Y, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(1.0,1.0f,1.0f,0.75f) );
		DrawTile( Canvas, Start.X+1.0f, Start.Y, 1, Start.Y+End.Y+1.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(0.0,0.0f,0.0f,0.50f) );
	}
	else
	{
		DrawTile( Canvas, Start.X, Start.Y-1.0f, Start.X+End.X-1.0f, 1, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(0.0,0.0f,0.0f,0.50f) );
		DrawTile( Canvas, Start.X, Start.Y, Start.X+End.X, 1, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(1.0,1.0f,1.0f,.75f) );
		DrawTile( Canvas, Start.X, Start.Y+1.0f, Start.X+End.X+1.0f, 1,  0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(0.0,0.0f,0.0f,0.50f) );
	}
}


/** 
 * Draws a line with alpha
 * 
 * @param Canvas	The canvas to draw on
 * @param Start		The start of the line
 * @param End		The end of the line
 * @param Alpha		The Alpha value to use
 * @param bVertical	TRUE if the line is vertical, FALSE if horizontal
 */
static void DrawTransparentLine( FCanvas* Canvas, const FVector2D& Start, const FVector2D& End, FLOAT Alpha, UBOOL bVertical )
{
	// This method uses DrawTile instead of DrawLine because DrawLine does not support alpha.
	if( bVertical )
	{
		DrawTile( Canvas, Start.X, Start.Y, 1, Start.Y+End.Y, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(1.0,1.0f,1.0f,Alpha) );
	}
	else
	{
		DrawTile( Canvas, Start.X, Start.Y, Start.X+End.X, 1, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(1.0,1.0f,1.0f,Alpha) );
	}
}

void WxInterpEd::DrawModeHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas)
{
	//do not render the matinee hud for "director windows"
	if (ExtraViewports.HasKey(ViewportClient))
	{
		return;
	}

	if( ViewportClient->AllowMatineePreview() )
	{
		// Get the size of the viewport
		const INT SizeX = Viewport->GetSizeX();
		const INT SizeY = Viewport->GetSizeY();

		if( IsEditingGridEnabled() )
		{
			// The main lines are rule of thirds lines so there should be 2 horizontal and vertical lines
			const UINT NumLines = 2;

			// Calculate the step size 
			const FLOAT InvSize = 1.0f/3.0f;
			const FLOAT StepX = SizeX * InvSize;
			const FLOAT StepY = SizeY * InvSize;

			// Draw each line 
			for( UINT Step = 1; Step <= NumLines; ++Step )
			{
				DrawShadowedLine( Canvas, FVector2D( StepX*Step, 0.0f ), FVector2D(StepX*Step, SizeY), TRUE );
				DrawShadowedLine( Canvas, FVector2D( 0.0f, StepY*Step), FVector2D(SizeX, StepY*Step), FALSE );
			}

			// Get the number of sub grid lines that should be drawn
			const INT EditingGridSize = GetEditingGridSize();
		
			// Do nothing if the user doesnt want to draw any lines
			if( EditingGridSize > 1 )
			{
				// The size of each rule of thirds block
				const FVector2D BlockSize(StepX,StepY);

				// The number of sub lines to draw is the number of lines in each block times the number of horizontal and vertical blocks
				const UINT NumRowsAndColumns = 6;
				UINT NumLines = NumRowsAndColumns*(EditingGridSize - 1);

				// Calculate the step size for each sub grid line 
				FLOAT InvSize = 1.0f/EditingGridSize;
				const FLOAT SubStepX = BlockSize.X * InvSize;
				const FLOAT SubStepY = BlockSize.Y * InvSize;

				// Draw each line
				for( UINT Step = 1; Step <= NumLines; ++Step )
				{
					DrawTransparentLine( Canvas, FVector2D( SubStepX*Step, 0 ), FVector2D(SubStepX*Step, SizeY ), .15f, TRUE );
					DrawTransparentLine( Canvas, FVector2D( 0, SubStepY*Step), FVector2D(SizeX, SubStepY*Step), .15f, FALSE );
				}
			}
		}

		if( IsEditingCrosshairEnabled() )
		{
			// Get the center point for the crosshair, accounting for half pixel offset
			FLOAT CenterX = SizeX / 2.0f + .5f;
			FLOAT CenterY = SizeY / 2.0f + .5f;
			
			FVector2D Center(CenterX,CenterY);

			// Draw the line a line in X and Y extending out from the center.
			DrawLine2D( Canvas, Center+FVector2D(-10.0f,0.0f), Center+FVector2D(10.0f,0.0f), FLinearColor(1.0f,0.0f,0.0f) );
			DrawLine2D( Canvas, Center+FVector2D(0.0f,-10.0f), Center+FVector2D(0.0f,10.0f), FLinearColor(1.0f,0.0f,0.0f) );
		}

		// If 'frame stats' are turned on and this viewport is configured for Matinee preview, then draw some text
		if( IsViewportFrameStatsEnabled() )
		{
			INT XL, YL;
			INT YPos = 3;
			INT XPos = 5;

			// Title
			{
				FString StatsString = *LocalizeUnrealEd( "Matinee" );
				DrawShadowedString( Canvas, XPos, YPos, *StatsString, GEngine->LargeFont, FLinearColor::White );
				StringSize( GEngine->LargeFont, XL, YL, *StatsString );
				XPos += XL;
				XPos += 32;
			}

			// Viewport resolution
			{
				FString StatsString = FString::Printf( TEXT("%dx%d"), SizeX, SizeY );
				DrawShadowedString( Canvas, XPos, YPos, *StatsString, GEngine->TinyFont, FLinearColor( 0, 1, 1 ) );
				StringSize( GEngine->TinyFont, XL, YL, *StatsString );
				YPos += YL;
			}

			// Frame counts
			{
				FString StatsString =
					FString::Printf(
						TEXT("%3.1f / %3.1f %s"),
						(1.0 / SnapAmount) * Interp->Position,
						(1.0 / SnapAmount) * IData->InterpLength,
						*LocalizeUnrealEd("InterpEd_TimelineInfo_Frames") );
				DrawShadowedString( Canvas, XPos, YPos, *StatsString, GEngine->TinyFont, FLinearColor( 0, 1, 0 ) );
				StringSize( GEngine->TinyFont, XL, YL, *StatsString );
				YPos += YL;
			}

			// SMTPE-style timecode
			if( bSnapToFrames )
			{
				FString StatsString = MakeTimecodeString( Interp->Position );
				DrawShadowedString( Canvas, XPos, YPos, *StatsString, GEngine->TinyFont, FLinearColor( 1, 1, 0 ) );
				StringSize( GEngine->TinyFont, XL, YL, *StatsString );
				YPos += YL;
			}
		}

		// Draw subtitles (toggle is handled internally)
		FVector2D MinPos(0.f, 0.f);
		FVector2D MaxPos(1.f, .9f);
		FIntRect SubtitleRegion(appTrunc(SizeX * MinPos.X), appTrunc(SizeY * MinPos.Y), appTrunc(SizeX * MaxPos.X), appTrunc(SizeY * MaxPos.Y));
		FSubtitleManager::GetSubtitleManager()->DisplaySubtitles( Canvas, SubtitleRegion );
	}
    // Camera Shot Names
    {
        TArray<UInterpTrack*> Results;
        UInterpGroupDirector* DirGroup = IData->FindDirectorGroup();
        if (DirGroup)
        {
            IData->FindTracksByClass( UInterpTrackDirector::StaticClass(), Results );
            for (INT i=0; i < Results.Num(); i++)
            {
                if (!Results(i)->IsDisabled()) 
                {
                    FString Name = Cast<UInterpTrackDirector>(Results(i))->GetViewedCameraShotName(Interp->Position);
                    if (Name != TEXT(""))
                    {
                        FString ShotNameString = FString::Printf(TEXT("[%s]"),*Name);
                        INT XL, YL;
                        StringSize( GEngine->LargeFont, XL, YL, *ShotNameString );
                        //to get text in the correct spot, we need to get its actual scaled size in the viewport
                        FLOAT FontScale = GEngine->LargeFont->GetScalingFactor(Viewport->GetSizeY());
                        XL *= FontScale;
                        YL *= FontScale;
                        INT LeftXPos = 10;
                        INT RightXPos = Viewport->GetSizeX() - (XL + 10);
                        INT BottomYPos = Viewport->GetSizeY() - (YL + 10);
                        DrawShadowedString( Canvas, RightXPos, BottomYPos, *ShotNameString, GEngine->LargeFont, FLinearColor::White );
                        
                        FString CinemaNameString = FString::Printf(TEXT("[%s]"),*DirGroup->GroupName.ToString());
                        DrawShadowedString( Canvas, LeftXPos, BottomYPos, *CinemaNameString, GEngine->LargeFont, FLinearColor::White );
                    }
                }
            }
        }
    }

	// Show a notification if we are adjusting a particular keyframe.
	if(Opt->bAdjustingKeyframe)
	{
		check(Opt->SelectedKeys.Num() == 1);

		FInterpEdSelKey& rSelKey = Opt->SelectedKeys(0);
		FString KeyTitle = FString::Printf( TEXT("%s%d"), ( rSelKey.Track ? *rSelKey.Track->TrackTitle : TEXT( "?" ) ), rSelKey.KeyIndex );
		FString AdjustNotify = FString::Printf( LocalizeSecure(LocalizeUnrealEd("AdjustKey_F"), *KeyTitle) );

		INT XL, YL;
		StringSize(GEngine->LargeFont, XL, YL, *AdjustNotify);
		DrawShadowedString(Canvas, 5, Viewport->GetSizeY() - (3 + YL) , *AdjustNotify, GEngine->LargeFont, FLinearColor( 1, 0, 0 ) );
	}
	else if(Opt->bAdjustingGroupKeyframes)
	{
		check(Opt->SelectedKeys.Num() > 1);

		// Make a list of all the unique subgroups within the selection, cache for fast lookup
		TArray<FString> UniqueSubGroupNames;
		TArray<FString> KeySubGroupNames;
		TArray<FString> KeyTitles;
		for( INT iSelectedKey = 0; iSelectedKey < Opt->SelectedKeys.Num(); iSelectedKey++ )
		{
			FInterpEdSelKey& rSelKey = Opt->SelectedKeys(iSelectedKey);
			FString SubGroupName = rSelKey.GetOwningTrackSubGroupName();
			UniqueSubGroupNames.AddUniqueItem( SubGroupName );
			KeySubGroupNames.AddItem( SubGroupName );
			FString KeyTitle = FString::Printf( TEXT("%s%d"), ( rSelKey.Track ? *rSelKey.Track->TrackTitle : TEXT( "?" ) ), rSelKey.KeyIndex );
			KeyTitles.AddItem( KeyTitle );
		}

		// Order the string in the format subgroup[tracktrack] subgroup[track]
		FString AdjustNotify( "AdjustKeys_F " );
		for( INT iUSubGroupName = 0; iUSubGroupName < UniqueSubGroupNames.Num(); iUSubGroupName++ )
		{
			FString& rUniqueSubGroupName = UniqueSubGroupNames(iUSubGroupName);
			AdjustNotify += rUniqueSubGroupName;
			AdjustNotify += TEXT( "[" );
			for( INT iKSubGroupName = 0; iKSubGroupName < KeySubGroupNames.Num(); iKSubGroupName++ )
			{
				FString& KeySubGroupName = KeySubGroupNames( iKSubGroupName );
				if ( rUniqueSubGroupName == KeySubGroupName )
				{
					AdjustNotify += KeyTitles( iKSubGroupName );
				}
			}
			AdjustNotify += TEXT( "] " );
		}

		INT XL, YL;
		StringSize(GEngine->LargeFont, XL, YL, *AdjustNotify);
		DrawShadowedString(Canvas, 5, Viewport->GetSizeY() - (3 + YL) , *AdjustNotify, GEngine->LargeFont, FLinearColor( 1, 0, 0 ) );
	}

	//Draw menu for real time track value recording
	if (ViewportClient->IsMatineeRecordingWindow() && bDisplayRecordingMenu)
	{
		//reset x position to left aligned
		INT XL, YL;
		INT XPos = 5;
		INT ValueXPos = 450;
		INT YPos = 25;
		FLinearColor ActiveMenuColor (1.0f, 1.0f, 0.0f);
		FLinearColor NormalMenuColor (1.0f, 1.0f, 1.0f);

		//if we're not actively recording
		if (RecordingState == MatineeConstants::RECORDING_COMPLETE)
		{
			//display record menu item
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::RECORD_MENU_RECORD_MODE) ? ActiveMenuColor : NormalMenuColor;

			FString RecordTracksString = LocalizeUnrealEd("InterpEd_RecordMenu_RecordMode");
			StringSize(GEngine->LargeFont, XL, YL, *RecordTracksString);
			DrawShadowedString(Canvas, XPos, YPos , *RecordTracksString, GEngine->LargeFont, DisplayColor );

			switch (RecordMode)
			{
				case MatineeConstants::RECORD_MODE_NEW_CAMERA :
					RecordTracksString = LocalizeUnrealEd("InterpEd_RecordMode_NewCameraMode");
					break;
				case MatineeConstants::RECORD_MODE_NEW_CAMERA_ATTACHED:
					RecordTracksString = LocalizeUnrealEd("InterpEd_RecordMode_NewCameraAttachedMode");
					break;
				case MatineeConstants::RECORD_MODE_DUPLICATE_TRACKS:
					RecordTracksString = LocalizeUnrealEd("InterpEd_RecordMode_DuplicateTracksMode");
					break;
				case MatineeConstants::RECORD_MODE_REPLACE_TRACKS:
					RecordTracksString = LocalizeUnrealEd("InterpEd_RecordMode_ReplaceTracksMode");
					break;
				default:
					break;
			}
			StringSize(GEngine->LargeFont, XL, YL, *RecordTracksString);
			DrawShadowedString(Canvas, ValueXPos, YPos , *RecordTracksString, GEngine->LargeFont, DisplayColor );

			YPos += YL;
		}
		else
		{
			//Time since we began recording
			DOUBLE CurrentTime = appSeconds();
			DOUBLE TimeSinceStateStart = (CurrentTime - RecordingStateStartTime);
			DOUBLE SelectedRegionDuration = GetRecordingEndTime() - GetRecordingStartTime();

			FLinearColor DisplayColor(1.0, 1.0f, 0.0f);

			//draw recording state
			FString RecordingStateString;
			switch (RecordingState)
			{
				case MatineeConstants::RECORDING_GET_READY_PAUSE:
					RecordingStateString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("InterpEd_RecordingStateGetReadyPause"), MatineeConstants::CountdownDurationInSeconds - TimeSinceStateStart) );
					break;
				case MatineeConstants::RECORDING_ACTIVE:
					RecordingStateString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("InterpEd_RecordingStateActive"), Interp->Position - GetRecordingStartTime(), SelectedRegionDuration) );
					DisplayColor = FLinearColor(1.0f, 0.0f, 0.0f);
					break;
			}
			StringSize(GEngine->LargeFont, XL, YL, *RecordingStateString);
			DrawShadowedString(Canvas, XPos, YPos , *RecordingStateString, GEngine->LargeFont, DisplayColor );
			YPos += YL;
		}

		FEditorCameraController* CameraController = ViewportClient->GetCameraController();
		check(CameraController);
		FCameraControllerConfig& CameraConfig = CameraController->GetConfig();

		//display translation speed adjustment factor
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::RECORD_MENU_TRANSLATION_SPEED) ? ActiveMenuColor : NormalMenuColor;

			FString TranslationSpeedString = LocalizeUnrealEd("InterpEd_RecordMenu_TranslationSpeedMultiplier");
			StringSize(GEngine->LargeFont, XL, YL, *TranslationSpeedString);
			DrawShadowedString(Canvas, XPos, YPos , *TranslationSpeedString, GEngine->LargeFont, DisplayColor );

			TranslationSpeedString = FString::Printf(TEXT("%f"), CameraConfig.TranslationMultiplier);
			StringSize(GEngine->LargeFont, XL, YL, *TranslationSpeedString);
			DrawShadowedString(Canvas, ValueXPos, YPos , *TranslationSpeedString, GEngine->LargeFont, DisplayColor );

			YPos += YL;
		}

		//display rotational speed adjustment factor
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::RECORD_MENU_ROTATION_SPEED) ? ActiveMenuColor : NormalMenuColor;

			FString RotationSpeedString = LocalizeUnrealEd("InterpEd_RecordMenu_RotationSpeedMultiplier");
			StringSize(GEngine->LargeFont, XL, YL, *RotationSpeedString);
			DrawShadowedString(Canvas, XPos, YPos , *RotationSpeedString, GEngine->LargeFont, DisplayColor );

			RotationSpeedString = FString::Printf(TEXT("%f"), CameraConfig.RotationMultiplier);
			StringSize(GEngine->LargeFont, XL, YL, *RotationSpeedString);
			DrawShadowedString(Canvas, ValueXPos, YPos , *RotationSpeedString, GEngine->LargeFont, DisplayColor );

			YPos += YL;
		}

		//display zoom speed adjustment factor
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::RECORD_MENU_ZOOM_SPEED) ? ActiveMenuColor : NormalMenuColor;

			FString ZoomSpeedString = LocalizeUnrealEd("InterpEd_RecordMenu_ZoomSpeedMultiplier");
			StringSize(GEngine->LargeFont, XL, YL, *ZoomSpeedString );
			DrawShadowedString(Canvas, XPos, YPos , *ZoomSpeedString , GEngine->LargeFont, DisplayColor );

			ZoomSpeedString = FString::Printf(TEXT("%f"), CameraConfig.ZoomMultiplier);
			StringSize(GEngine->LargeFont, XL, YL, *ZoomSpeedString );
			DrawShadowedString(Canvas, ValueXPos, YPos , *ZoomSpeedString , GEngine->LargeFont, DisplayColor );

			YPos += YL;
		}

		//Trim
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::RECORD_MENU_TRIM) ? ActiveMenuColor : NormalMenuColor;

			FString TrimString = LocalizeUnrealEd("InterpEd_RecordMenu_Trim");
			StringSize(GEngine->LargeFont, XL, YL, *TrimString );
			DrawShadowedString(Canvas, XPos, YPos , *TrimString , GEngine->LargeFont, DisplayColor );

			TrimString = FString::Printf(TEXT("%f"), CameraConfig.PitchTrim);
			StringSize(GEngine->LargeFont, XL, YL, *TrimString );
			DrawShadowedString(Canvas, ValueXPos, YPos , *TrimString , GEngine->LargeFont, DisplayColor );

			YPos += YL;
		}

		//Display Invert Mouse X & Mouse Y settings
		for (INT i = 0; i < 2; ++i)
		{
			INT SettingToCheck = (i==0) ? MatineeConstants::RECORD_MENU_INVERT_X_AXIS : MatineeConstants::RECORD_MENU_INVERT_Y_AXIS;
			FString InvertString = (i==0) ? LocalizeUnrealEd("InterpEd_RecordMenu_InvertXAxis") : LocalizeUnrealEd("InterpEd_RecordMenu_InvertYAxis");
			UBOOL InvertValue = (i==0) ? CameraConfig.bInvertX : CameraConfig.bInvertY;
			FString InvertValueString = (InvertValue ? LocalizeUnrealEd("Yes") : LocalizeUnrealEd("No"));

			FLinearColor DisplayColor = (RecordMenuSelection == SettingToCheck) ? ActiveMenuColor : NormalMenuColor;

			StringSize(GEngine->LargeFont, XL, YL, *InvertString );
			DrawShadowedString(Canvas, XPos, YPos , *InvertString , GEngine->LargeFont, DisplayColor );

			StringSize(GEngine->LargeFont, XL, YL, *InvertValueString );
			DrawShadowedString(Canvas, ValueXPos, YPos , *InvertValueString , GEngine->LargeFont, DisplayColor );

			YPos += YL;
		}

		//display roll smoothing
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::RECORD_MENU_ROLL_SMOOTHING) ? ActiveMenuColor : NormalMenuColor;

			FString RollSmoothingString = LocalizeUnrealEd("InterpEd_RecordMenu_RollSmoothing");
			StringSize(GEngine->LargeFont, XL, YL, *RollSmoothingString);
			DrawShadowedString(Canvas, XPos, YPos , *RollSmoothingString, GEngine->LargeFont, DisplayColor );

			
			FString RollSmoothingStateString = FString::Printf(TEXT("%d"), RecordRollSmoothingSamples);
			StringSize(GEngine->LargeFont, XL, YL, *RollSmoothingStateString);
			DrawShadowedString(Canvas, ValueXPos, YPos , *RollSmoothingStateString, GEngine->LargeFont, DisplayColor );

			YPos += YL;
		}

		//display roll smoothing
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::RECORD_MENU_PITCH_SMOOTHING) ? ActiveMenuColor : NormalMenuColor;

			FString PitchSmoothingString = LocalizeUnrealEd("InterpEd_RecordMenu_PitchSmoothing");
			StringSize(GEngine->LargeFont, XL, YL, *PitchSmoothingString);
			DrawShadowedString(Canvas, XPos, YPos , *PitchSmoothingString, GEngine->LargeFont, DisplayColor );


			FString PitchSmoothingStateString = FString::Printf(TEXT("%d"), RecordPitchSmoothingSamples);
			StringSize(GEngine->LargeFont, XL, YL, *PitchSmoothingStateString);
			DrawShadowedString(Canvas, ValueXPos, YPos , *PitchSmoothingStateString, GEngine->LargeFont, DisplayColor );

			YPos += YL;
		}

		//display roll smoothing
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::RECORD_MENU_CAMERA_MOVEMENT_SCHEME) ? ActiveMenuColor : NormalMenuColor;

			FString CameraMovmentString = LocalizeUnrealEd("InterpEd_RecordMenu_CameraMovementScheme");
			StringSize(GEngine->LargeFont, XL, YL, *CameraMovmentString);
			DrawShadowedString(Canvas, XPos, YPos , *CameraMovmentString, GEngine->LargeFont, DisplayColor );

			FString CameraMovmentStateString;
			switch (RecordCameraMovementScheme)
			{
			case MatineeConstants::CAMERA_SCHEME_FREE_CAM:
				CameraMovmentStateString = LocalizeUnrealEd("InterpEd_RecordMenu_CameraMovementScheme_FreeCam");
				break;
			case MatineeConstants::CAMERA_SCHEME_PLANAR_CAM:
				CameraMovmentStateString = LocalizeUnrealEd("InterpEd_RecordMenu_CameraMovementScheme_PlanarCam");
				break;
			}
			StringSize(GEngine->LargeFont, XL, YL, *CameraMovmentStateString);
			DrawShadowedString(Canvas, ValueXPos, YPos , *CameraMovmentStateString, GEngine->LargeFont, DisplayColor );

			YPos += YL;
		}

		//give some space before giving the live stats
		YPos += 20;
		//display current zoom distance
		{
			FLinearColor DisplayColor = (RecordMenuSelection == MatineeConstants::RECORD_MENU_ZOOM_DISTANCE) ? ActiveMenuColor : NormalMenuColor;

			FString ZoomDistanceString = LocalizeUnrealEd("InterpEd_RecordMenu_ZoomDistance");
			StringSize(GEngine->LargeFont, XL, YL, *ZoomDistanceString );
			DrawShadowedString(Canvas, XPos, YPos , *ZoomDistanceString , GEngine->LargeFont, DisplayColor );

			ZoomDistanceString = FString::Printf(TEXT("%f"), ViewportClient->ViewFOV);
			StringSize(GEngine->LargeFont, XL, YL, *ZoomDistanceString );
			DrawShadowedString(Canvas, ValueXPos, YPos , *ZoomDistanceString , GEngine->LargeFont, DisplayColor );

			YPos += YL;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////
// Properties window NotifyHook stuff

void WxInterpEd::NotifyDestroy( void* Src )
{

}

void WxInterpEd::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{

}

void WxInterpEd::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	CurveEd->CurveChanged();

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();

	// If we are changing the properties of a Group, propagate changes to the GroupAnimSets array to the Actors being controlled by this group.
	for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		UInterpGroup* CurrentSelectedGroup = *GroupIt;

		if( CurrentSelectedGroup->HasAnimControlTrack() )
		{
			for( TArray<UInterpGroupInst*>::TIterator GroupInstIt(Interp->GroupInst); GroupInstIt; ++GroupInstIt )
			{
				UInterpGroupInst* Inst = *GroupInstIt;

				if( CurrentSelectedGroup == Inst->Group  )
				{
					AActor* Actor = Inst->GetGroupActor();
					if(Actor)
					{
						Actor->PreviewBeginAnimControl(CurrentSelectedGroup);
					}
				}
			}

			// Update to current position - so changes in AnimSets take affect now.
			RefreshInterpPosition();
		}
	}
}

void WxInterpEd::NotifyExec( void* Src, const TCHAR* Cmd )
{
	
}

////////////////////////////////
// FCallbackEventDevice interface

void WxInterpEd::Send(ECallbackEventType Event, UObject* InObject)
{
	if ( Interp == NULL )
	{
		return;
	}

	// need to modify relative location when stage marker location has been changed
	// so we're listening to it and if found same actor, refresh it
	if( Event == CALLBACK_OnActorMoved )
	{
		// If we are changing the properties of a Group, propagate changes to the GroupAnimSets array to the Actors being controlled by this group.
		for( INT I=0; I<Interp->GroupInst.Num(); ++I )
		{
			UInterpGroupInstAI* AIGroupInst = Cast<UInterpGroupInstAI>(Interp->GroupInst(I));
			// if it hasn't been deleted, while closing, they're deleted 
			if ( AIGroupInst && AIGroupInst->AIGroup && AIGroupInst->AIGroup->HasMoveTrack()==FALSE )
			{
				if ( AIGroupInst->StageMarkActor == InObject )
				{
					// need to pause sequence if playing
					if ( Interp->bIsPlaying ) 
					{
						StopPlaying();
					}
					// refresh Stage Marker location
					AIGroupInst->DestroyPreviewPawn();
					AIGroupInst->CreatePreviewPawn();

					// rewind 
					SetInterpPosition(0.f);
				}
				// if user is modifying preview pawn and the position is 0.f and if not playing
				else if ( AIGroupInst->StageMarkActor && AIGroupInst->PreviewPawn == InObject && Interp->Position == 0.f && Interp->bIsPlaying == FALSE )
				{
					// then change stage marker location to preview pawn location
					FVector NewLoc = AIGroupInst->PreviewPawn->Location;

					if (AIGroupInst->PreviewPawn && AIGroupInst->PreviewPawn->CylinderComponent)
					{
						NewLoc.Z -= AIGroupInst->PreviewPawn->CylinderComponent->CollisionHeight;
					}

					AIGroupInst->StageMarkActor->SetLocation(NewLoc);
					AIGroupInst->StageMarkActor->SetRotation(AIGroupInst->PreviewPawn->Rotation);

					// rewind 
					SetInterpPosition(0.f);
				}


			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////
// Curve editor notify stuff

/** Implement Curve Editor notify interface, so we can back up state before changes and support Undo. */
void WxInterpEd::PreEditCurve(TArray<UObject*> CurvesAboutToChange)
{
	InterpEdTrans->BeginSpecial(*LocalizeUnrealEd("CurveEdit"));

	// Call Modify on all tracks with keys selected
	for(INT i=0; i<CurvesAboutToChange.Num(); i++)
	{
		// If this keypoint is from an InterpTrack, call Modify on it to back up its state.
		UInterpTrack* Track = Cast<UInterpTrack>( CurvesAboutToChange(i) );
		if(Track)
		{
			Track->Modify();
		}
	}
}

void WxInterpEd::PostEditCurve()
{
	InterpEdTrans->EndSpecial();
}

void WxInterpEd::MovedKey()
{
	// Update interpolation to the current position - but thing may have changed due to fiddling on the curve display.
	RefreshInterpPosition();
}

void WxInterpEd::DesireUndo()
{
	InterpEdUndo();
}

void WxInterpEd::DesireRedo()
{
	InterpEdRedo();
}


/**
 * FCurveEdNotifyInterface: Called by the Curve Editor when a Curve Label is clicked on
 *
 * @param	CurveObject	The curve object whose label was clicked on
 */
void WxInterpEd::OnCurveLabelClicked( UObject* CurveObject )
{
	check( CurveObject != NULL );

	// Is this curve an interp track?
	UInterpTrack* Track = Cast<UInterpTrack>( CurveObject );
	if( Track != NULL )
	{
		// Select the track!
		SelectTrack( Track->GetOwningGroup(), Track );
		ClearKeySelection();
	}
}



/**
 * Either shows or hides the director track window by splitting/unsplitting the parent window
 */
void WxInterpEd::UpdateDirectorTrackWindowVisibility()
{
	// Do we have a director group?  If so, then the director track window will be implicitly visible!
	UInterpGroupDirector* DirGroup = IData->FindDirectorGroup();
	UBOOL bWantDirectorTrackWindow = ( DirGroup != NULL );

	// Show director track window by splitting the window
	if( bWantDirectorTrackWindow && !GraphSplitterWnd->IsSplit() )
	{
		GraphSplitterWnd->SplitHorizontally( DirectorTrackWindow, TrackWindow, GraphSplitPos );
		DirectorTrackWindow->Show( TRUE );

		// When both windows are visible, we'll draw the tabs in the director track window and the timeline in the
		// regular track window
		DirectorTrackWindow->InterpEdVC->bWantFilterTabs = TRUE;
		DirectorTrackWindow->InterpEdVC->bWantTimeline = FALSE;
		TrackWindow->InterpEdVC->bWantFilterTabs = FALSE;
		TrackWindow->InterpEdVC->bWantTimeline = TRUE;
	}
	// Hide the director track window
	else if( !bWantDirectorTrackWindow && GraphSplitterWnd->IsSplit() )
	{
		GraphSplitterWnd->Unsplit( DirectorTrackWindow );
		DirectorTrackWindow->Show( FALSE );

		// When only the regular track window is visible, we'll draw both the tabs and timeline in that window!
		TrackWindow->InterpEdVC->bWantFilterTabs = TRUE;
		TrackWindow->InterpEdVC->bWantTimeline = TRUE;
	}
}

/**
 * Updates the contents of the property window based on which groups or tracks are selected if any. 
 */
void WxInterpEd::UpdatePropertyWindow()
{
	if( HasATrackSelected() )
	{
		TArray<UInterpTrack*> SelectedTracks;
		GetSelectedTracks(SelectedTracks);

		// We are guaranteed at least one selected track.
		check( SelectedTracks.Num() );

		// Set the property window to the selected track.
		PropertyWindow->SetObjectArray(SelectedTracks, EPropertyWindowFlags::Sorted );
	}
	else if( HasAGroupSelected() )
	{
		TArray<UInterpGroup*> SelectedGroups;
		GetSelectedGroups(SelectedGroups);

		// We are guaranteed at least one selected group.
		check( SelectedGroups.Num() );

		// Set the property window to the selected group.
		PropertyWindow->SetObjectArray(SelectedGroups, EPropertyWindowFlags::Sorted );
	}
	else
	{
		// Set the property window to nothing
		PropertyWindow->SetObject(NULL, EPropertyWindowFlags::NoFlags );
	}
}



/**
 * Locates the specified group's parent group folder, if it has one
 *
 * @param ChildGroup The group who's parent we should search for
 *
 * @return Returns the parent group pointer or NULL if one wasn't found
 */
UInterpGroup* WxInterpEd::FindParentGroupFolder( UInterpGroup* ChildGroup ) const
{
	// Does this group even have a parent?
	if( ChildGroup->bIsParented )
	{
		check( !ChildGroup->bIsFolder );

		// Find the child group list index
		INT ChildGroupIndex = -1;
		if( IData->InterpGroups.FindItem( ChildGroup, ChildGroupIndex ) )
		{
			// Iterate backwards in the group list starting at the child group index, looking for its parent
			for( INT CurGroupIndex = ChildGroupIndex - 1; CurGroupIndex >= 0; --CurGroupIndex )
			{
				UInterpGroup* CurGroup = IData->InterpGroups( CurGroupIndex );

				// Just skip the director group if we find it; it's not allowed to be a parent
				if( !CurGroup->IsA( UInterpGroupDirector::StaticClass() ) )
				{
					// Is the current group a top level folder?
					if( !CurGroup->bIsParented )
					{
						check( CurGroup->bIsFolder );

						// Found it!
						return CurGroup;
					}
				}
			}
		}
	}

	// Not found
	return NULL;
}



/**
 * Counts the number of children that the specified group folder has
 *
 * @param GroupFolder The group who's children we should count
 *
 * @return Returns the number of child groups
 */
INT WxInterpEd::CountGroupFolderChildren( UInterpGroup* const GroupFolder ) const
{
	INT ChildCount = 0;

	// Child groups currently don't support containing their own children
	if( GroupFolder->bIsFolder && !GroupFolder->bIsParented )
	{
		INT StartIndex = IData->InterpGroups.FindItemIndex( GroupFolder ) + 1;
		for( INT CurGroupIndex = StartIndex; CurGroupIndex < IData->InterpGroups.Num(); ++CurGroupIndex	)
		{
			UInterpGroup* CurGroup = IData->InterpGroups( CurGroupIndex );

			// Children always appear sequentially after their parent in the array, so if we find an unparented item, then 
			// we know we've reached the last child
			if( CurGroup->bIsParented )
			{
				// Found a child!
				++ChildCount;
			}
			else
			{
				// No more children
				break;
			}
		}
	}

	return ChildCount;
}

/**
* @param	InGroup	The group to check if its a parent or has a parent. 
* @return	A structure containing information about the given group's parent relationship.
*/
FInterpGroupParentInfo WxInterpEd::GetParentInfo( UInterpGroup* InGroup ) const
{
	check(InGroup);

	FInterpGroupParentInfo Info(InGroup);

	Info.Parent			= FindParentGroupFolder(InGroup);
	Info.GroupIndex		= IData->InterpGroups.FindItemIndex(InGroup);
	Info.bHasChildren	= CountGroupFolderChildren(InGroup);

	return Info;
}

/**
 * Determines if the child candidate can be parented (or re-parented) by the parent candiddate.
 *
 * @param	ChildCandidate	The group that desires to become the child to the parent candidate.
 * @param	ParentCandidate	The group that, if a folder, desires to parent the child candidate.
 *
 * @return	TRUE if the parent candidate can parent the child candidate. 
 */
UBOOL WxInterpEd::CanReparent( const FInterpGroupParentInfo& ChildCandidate, const FInterpGroupParentInfo& ParentCandidate ) const
{
	// Can re-parent if both groups are the same!
	if( ParentCandidate.Group == ChildCandidate.Group )
	{
		return FALSE;
	}

	const UClass* DirectorClass = UInterpGroupDirector::StaticClass();

	// Neither group can be a director
	if( ParentCandidate.Group->IsA(DirectorClass) || ChildCandidate.Group->IsA(DirectorClass) )
	{
		return FALSE;
	}

	// We can't allow the user to re-parent groups that already have children, 
	// since we currently don't support multi-level nesting.
	if( ChildCandidate.IsAParent() )
	{
		return FALSE;
	}

	// The group candidate can't be a folder because we don't support folders 
	// parenting folders. This is similar to the multi-level parent nesting. 
	if( ChildCandidate.Group->bIsFolder )
	{
		return FALSE;
	}

	// The folder candidate must be a folder, obviously.
	if( !ParentCandidate.Group->bIsFolder )
	{
		return FALSE;
	}

	// The parent candidate can't already be a parent to the child.
	if( ChildCandidate.IsParent(ParentCandidate) )
	{
		return FALSE;
	}

	// At this point we verified the folder candidate is actually a folder. 
	check( !ParentCandidate.HasAParent() );

	return TRUE;
}


/**
 * Fixes up any problems in the folder/group hierarchy caused by bad parenting in previous builds
 */
void WxInterpEd::RepairHierarchyProblems()
{
	UBOOL bAnyRepairsMade = FALSE;

	UBOOL bPreviousGroupWasFolder = FALSE;
	UBOOL bPreviousGroupWasParented = FALSE;

	for( INT CurGroupIndex = 0; CurGroupIndex < IData->InterpGroups.Num(); ++CurGroupIndex )
	{
		UInterpGroup* CurGroup = IData->InterpGroups( CurGroupIndex );
		if( CurGroup != NULL )
		{
			if( CurGroup->bIsFolder )
			{
				// This is a folder group.
				
				// Folders are never allowed to be parented
				if( CurGroup->bIsParented )
				{
					// Repair parenting problem
					CurGroup->bIsParented = FALSE;
					bAnyRepairsMade = TRUE;
				}
			}
			else if( CurGroup->bIsParented )
			{
				// This group is parented to a folder
				
				// Make sure the previous group in the list was either a folder OR a parented group
				if( !bPreviousGroupWasFolder && !bPreviousGroupWasParented )
				{
					// Uh oh, the current group thinks its parented but the previous item is not a folder
					// or another parented group.  This means the current group thinks its parented to
					// another root group.  No good!  We'll unparent the group to fix this.
					CurGroup->bIsParented = FALSE;
					bAnyRepairsMade = TRUE;
				}
			}

			// If this is a 'director group', its never allowed to be parented (or act as a folder)
			if( CurGroup->IsA( UInterpGroupDirector::StaticClass() ) )
			{
				if( CurGroup->bIsParented )
				{
					// Director groups cannot be parented
					CurGroup->bIsParented = FALSE;
					bAnyRepairsMade = TRUE;
				}

				if( CurGroup->bIsFolder )
				{
					// Director groups cannot act as a folder
					CurGroup->bIsFolder = FALSE;
					bAnyRepairsMade = TRUE;
				}
			}

			// Keep track of this group's status for the next iteration's tests
			bPreviousGroupWasFolder	 = CurGroup->bIsFolder;
			bPreviousGroupWasParented = CurGroup->bIsParented;
		}
		else
		{
			// Bad group pointer, so remove this element from the list
			IData->InterpGroups.Remove( 0 );
			--CurGroupIndex;
			bAnyRepairsMade = TRUE;
		}
	}


	if( bAnyRepairsMade )
	{
		// Dirty the package so that editor changes will be saved
		IData->MarkPackageDirty();

		// Notify the user
		appMsgf( AMT_OK, *LocalizeUnrealEd( "InterpEd_HierachyRepairsNotification" ) );
	}
}



///////////////////////////////////////////////////////////////////////////////////////
// FDockingParent Interface

/**
 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
 *  @return A string representing a name to use for this docking parent.
 */
const TCHAR* WxInterpEd::GetDockingParentName() const
{
	return TEXT("Matinee");
}

/**
 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
 */
const INT WxInterpEd::GetDockingParentVersion() const
{
	// NOTE: Version 0 supported a dockable Property window
	// NOTE: Version 1 added support for a dockable Curve Editor window
	return 1;
}

/**
 * Helper function to refresh the Kismet window associated with the editor's USeqAct_Interp
 */
void WxInterpEd::RefreshAssociatedKismetWindow() const
{
	check( Interp );

	// Find the sequence of Interp so we can find the matching kismet window
	USequence* RootSeq = Cast<USequence>( Interp->GetOuter() );
	if ( !RootSeq )
	{
		RootSeq = Interp->ParentSequence;
	}
	check( RootSeq );

	// Search all the Kismet windows for a sequence match
	for( INT WindowIndex = 0; WindowIndex < GApp->KismetWindows.Num(); ++WindowIndex )
	{
		WxKismet* KismetWindow = GApp->KismetWindows( WindowIndex );
		if ( KismetWindow->Sequence == RootSeq )
		{
			KismetWindow->RefreshViewport();
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////
// WxCameraAnimEd editor

WxCameraAnimEd::WxCameraAnimEd( wxWindow* InParent, wxWindowID InID, class USeqAct_Interp* InInterp )
:	WxInterpEd(InParent, InID, InInterp)
{
	SetTitle(*LocalizeUnrealEd("MatineeCamAnimMode"));

	PreviewPawns.Empty();
	// initialize preview stuff
	InitializePreviewSets();
}

void WxCameraAnimEd::InitializePreviewSets()
{
#if WITH_EDITORONLY_DATA
	// before tearing down, store data we need in the camera anim
	if ( Interp && Interp->InterpData && (Interp->InterpData->InterpGroups.Num() > 0) )
	{
		UCameraAnim* const CamAnim = Cast<UCameraAnim>(Interp->InterpData->InterpGroups(0)->GetOuter());
		if (CamAnim)
		{
			// link back to camra, so it can update back after it's done
			CamAnim->CameraInterpGroup->CameraAnimInst = CamAnim;

			UClass * DefaultPreviewPawnClass=NULL;
			// if no preview pawn class is set, get default one
			FString PreviewPawnName = GConfig->GetStr(TEXT("CameraPreview"), TEXT("DefaultPreviewPawnClassName"), GEditorIni);
			if ( PreviewPawnName!= TEXT("") )
			{
				DefaultPreviewPawnClass = LoadObject<UClass>(NULL, *PreviewPawnName, NULL, LOAD_None, NULL);
			}
			else
			{
				debugf(NAME_Warning, TEXT("Matinee Preview Default Mesh is missing."));
				return;
			}

			// create preview pawn in the location
			if (CamAnim->CameraInterpGroup->Target.PawnClass==NULL)
			{
				CamAnim->CameraInterpGroup->Target.PawnClass=DefaultPreviewPawnClass;
			}

// 			if (CamAnim->CameraInterpGroup->InteractionTarget.PawnClass==NULL)
// 			{
// 				CamAnim->CameraInterpGroup->InteractionTarget.PawnClass=DefaultPreviewPawnClass;
// 			}

			CamAnim->PreviewInterpGroup = CreateInterpGroup(CamAnim->CameraInterpGroup->Target);;
// 			if (CamAnim->CameraInterpGroup->EnableInteractionTarget)
// 			{
// 				CreateInterpGroup(CamAnim->CameraInterpGroup->InteractionTarget);
// 			}
		}
	}
#endif
}

UInterpGroup*	WxCameraAnimEd::CreateInterpGroup(FCameraPreviewInfo& PreviewInfo)
{
	PreviewInfo.PawnInst = CreatePreviewPawn(PreviewInfo.PawnClass, PreviewInfo.Location, PreviewInfo.Rotation);
	PreviewPawns.AddItem(PreviewInfo.PawnInst);

	if (PreviewInfo.PawnInst && PreviewInfo.PawnInst->Mesh && PreviewInfo.PawnInst->Mesh->AnimSets.Num() > 0)
	{
		// create InterpGroup so that we can play animation to this pawn
		UInterpGroup* NewGroup = ConstructObject<UInterpGroup>( UInterpGroup::StaticClass(), IData, NAME_None, RF_Transient );
		NewGroup->GroupName=FName(TEXT("Preview Pawn"));
		NewGroup->EnsureUniqueName();
		NewGroup->GroupAnimSets.Append(PreviewInfo.PawnInst->Mesh->AnimSets);
		if ( PreviewInfo.PreviewAnimSets.Num() > 0 )
		{
			NewGroup->GroupAnimSets.Append(PreviewInfo.PreviewAnimSets);
		}

		IData->InterpGroups.AddItem(NewGroup);

		// now add group inst
		UInterpGroupInst* NewGroupInst = ConstructObject<UInterpGroupInst>( UInterpGroupInst::StaticClass(), Interp, NAME_None, RF_Transient );
		// Initialise group instance, saving ref to actor it works on.
		NewGroupInst->InitGroupInst(NewGroup, PreviewInfo.PawnInst);
		const INT NewGroupInstIndex = Interp->GroupInst.AddItem(NewGroupInst);

		//Link group with actor
		Interp->InitGroupActorForGroup(NewGroup, PreviewInfo.PawnInst);

		// Now time to add AnimTrack so that we can play animation
		INT AnimTrackIndex = INDEX_NONE;
		// add anim track but do not use addtotrack function that does too many things 
		// Construct track and track instance objects.
		UInterpTrackAnimControl* AnimTrack = ConstructObject<UInterpTrackAnimControl>( UInterpTrackAnimControl::StaticClass(), NewGroup, NAME_None, RF_Transient );
		check(AnimTrack);

		NewGroup->InterpTracks.AddItem(AnimTrack);
		check (AnimTrack);
		// use config anim slot
		AnimTrack->SlotName = FName(*GConfig->GetStr(TEXT("MatineePreview"), TEXT("DefaultAnimSlotName"), GEditorIni));
		UInterpTrackInst* NewTrackInst = ConstructObject<UInterpTrackInst>( AnimTrack->TrackInstClass, NewGroupInst, NAME_None, RF_Transient );

		NewGroupInst->TrackInst.AddItem(NewTrackInst);

		// Initialize track, giving selected object.
		NewTrackInst->InitTrackInst(AnimTrack);

		// Save state into new track before doing anything else (because we didn't do it on ed mode change).
		NewTrackInst->SaveActorState(AnimTrack);
		check (NewGroupInst->TrackInst.Num() > 0);

		// add default anim curve weights to be 1
		INT KeyIndex = AnimTrack->CreateNewKey(0.0f);
		AnimTrack->SetKeyOut(0, KeyIndex, 1.0f);

		if (PreviewInfo.AnimSeqName!=NAME_None)
		{
			KeyIndex = AnimTrack->AddKeyframe(0.0f, NewGroupInst->TrackInst(0), CIM_Linear);
			FAnimControlTrackKey& NewSeqKey = AnimTrack->AnimSeqs( KeyIndex );
			NewSeqKey.AnimSeqName = PreviewInfo.AnimSeqName;	
		}

		PreviewInfo.PawnInst->PreviewBeginAnimControl( NewGroup );
		PreviewPawns.AddItem(PreviewInfo.PawnInst);

		return NewGroup;
	}

	return NULL;
}
APawn * WxCameraAnimEd::CreatePreviewPawn(UClass * PreviewPawnClass, const FVector & Loc, const FRotator & Rot)
{
	APawn* PawnActor = Cast<APawn>(GWorld->SpawnActor(PreviewPawnClass, NAME_None, Loc, Rot, NULL, TRUE));

	// if failed error
	PawnActor->SetFlags(RF_Transient);

	return PawnActor;
}

WxCameraAnimEd::~WxCameraAnimEd()
{
	for (INT I=0; I<PreviewPawns.Num(); ++I)
	{
		GWorld->DestroyActor(PreviewPawns(I));
	}
}

/** Creates a popup context menu based on the item under the mouse cursor.
* @param	Viewport	FViewport for the FInterpEdViewportClient.
* @param	HitResult	HHitProxy returned by FViewport::GetHitProxy( ).
* @return	A new wxMenu with context-appropriate menu options or NULL if there are no appropriate menu options.
*/
wxMenu	*WxCameraAnimEd::CreateContextMenu( FViewport *Viewport, const HHitProxy *HitResult )
{
	wxMenu	*Menu = NULL;

	if(HitResult->IsA(HInterpEdTrackBkg::StaticGetType()))
	{
		// no menu, explicitly ignore this case
	}
	else if(HitResult->IsA(HInterpEdGroupTitle::StaticGetType()))
	{
		UInterpGroup* Group = ((HInterpEdGroupTitle*)HitResult)->Group;

		if( !IsGroupSelected(Group) )
		{
			SelectGroup(Group);
		}

		Menu = new WxMBCameraAnimEdGroupMenu( this );		
	}
	else 
	{
		// let our parent handle it
		Menu = WxInterpEd::CreateContextMenu(Viewport, HitResult);
	}

	return Menu;
}

void WxCameraAnimEd::OnClose(wxCloseEvent& In)
{
	TArray<UObject**> ObjectVars;
	Interp->GetObjectVars(ObjectVars);

	// before tearing down, store data we need in the camera anim
	if ( Interp && Interp->InterpData && (Interp->InterpData->InterpGroups.Num() > 0) )
	{
		UCameraAnim* const CamAnim = Cast<UCameraAnim>(Interp->InterpData->InterpGroups(0)->GetOuter());
		if (CamAnim)
		{
			// Fill in AnimLength parameter, in case it changed during editing
			if (CamAnim->AnimLength != Interp->InterpData->InterpLength)
			{
				CamAnim->AnimLength = Interp->InterpData->InterpLength;
				CamAnim->MarkPackageDirty(TRUE);
			}

			// find the temp camera actor and fill in other base data
			for (INT Idx=0; Idx<ObjectVars.Num(); ++Idx)
			{
				ACameraActor* const CamActor = Cast<ACameraActor>(*(ObjectVars(Idx)));
				if (CamActor)
				{
					CamAnim->BaseFOV = CamActor->FOVAngle;
					CamAnim->BasePPSettings = CamActor->CamOverridePostProcess;
					CamAnim->BasePPSettingsAlpha = CamActor->CamOverridePostProcessAlpha;
					CamAnim->MarkPackageDirty(TRUE);
					break;
				}
			}
		}
	}

	// delete the attached objects.  this should take care of the temp camera actor.
	for (INT Idx=0; Idx<ObjectVars.Num(); ++Idx)
	{
		AActor* Actor = Cast<AActor>(*(ObjectVars(Idx)));

		// prevents a NULL in the selection
		if (Actor->IsSelected())
		{
			GEditor->GetSelectedActors()->Deselect(Actor);
		}

		GWorld->DestroyActor(Actor);
	}

	// need to destroy all of the temp kismet stuff
	TArray<USequenceObject*> SeqsToDelete;
	{
		SeqsToDelete.AddItem(Interp);

		if (Interp->InterpData)
		{
			// delete everything linked to the temp interp.  this should take care of
			// both the interpdata and the cameraactor's seqvar_object
			for (INT Idx=0; Idx<Interp->VariableLinks.Num(); ++Idx)
			{
				FSeqVarLink* Link = &Interp->VariableLinks(Idx);

				for (INT VarIdx=0; VarIdx<Link->LinkedVariables.Num(); ++VarIdx)
				{
					SeqsToDelete.AddItem(Link->LinkedVariables(VarIdx));
				}
			}
		}
	}
	USequence* RootSeq = Interp->GetRootSequence();
	RootSeq->RemoveObjects(SeqsToDelete);

	// update all kismet property windows, so we don't have a property window with a dangling 
	// ref to the seq objects we just deleted
	for(INT i=0; i<GApp->KismetWindows.Num(); i++)
	{
		WxKismet* const KismetWindow = GApp->KismetWindows(i);
		for (INT SeqIdx=0; SeqIdx<SeqsToDelete.Num(); SeqIdx++)
		{
			KismetWindow->RemoveFromSelection(SeqsToDelete(SeqIdx));
		}
		KismetWindow->UpdatePropertyWindow();
	}

	// make sure destroyed actors get flushed asap
	GWorld->GetWorldInfo()->ForceGarbageCollection();

	// update the content browser to reflect any changes we've made
	GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI ) );
	
	// clean up any open property windows, in case one of them points to something we just deleted
	GUnrealEd->UpdatePropertyWindows();

	WxInterpEd::OnClose(In);
}




