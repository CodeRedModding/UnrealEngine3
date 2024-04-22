/*=============================================================================
	InterpEditorTools.cpp: Interpolation editing support tools
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineAnimClasses.h"
#include "UnLinkedObjDrawUtils.h"
#include "InterpEditor.h"
#include "PropertyWindow.h"
#include "DlgGenericComboEntry.h"
#include "LevelViewportToolbar.h"

static const FColor ActiveCamColor(255, 255, 0);
static const FColor SelectedCurveColor(255, 255, 0);
static const INT	DuplicateKeyOffset(10);
static const INT	KeySnapPixels(5);

///// UTILS

void WxInterpEd::TickInterp(FLOAT DeltaTime)
{
	static UBOOL bWasPlayingLastTick = FALSE;
	
	// Don't tick if a windows close request was issued.
	if( !bClosed && Interp->bIsPlaying )
	{
		// When in 'fixed time step' playback, we may need to constrain the frame rate (by sleeping!)
		ConstrainFixedTimeStepFrameRate();

		// Make sure particle replay tracks have up-to-date editor-only transient state
		UpdateParticleReplayTracks();

		// Modify playback rate by desired speed.
		FLOAT TimeDilation = GWorld->GetWorldInfo()->TimeDilation;
		Interp->StepInterp(DeltaTime * PlaybackSpeed * TimeDilation, TRUE);
		
		// If we are looping the selected section, when we pass the end, place it back to the beginning 
		if(bLoopingSection)
		{
			if(Interp->Position >= IData->EdSectionEnd)
			{
				Interp->UpdateInterp(IData->EdSectionStart, TRUE, TRUE);
				Interp->Play();
			}
		}

		UpdateCameraToGroup(TRUE);
		UpdateCamColours();
		CurveEd->SetPositionMarker(TRUE, Interp->Position, PosMarkerColor );
	}
	else
	{
		UpdateCameraToGroup(FALSE);
	}

	if( bWasPlayingLastTick && !Interp->bIsPlaying )
	{
		// If the interp was playing last tick but is now no longer playing turn off audio.
		SetAudioRealtimeOverride( FALSE );
	}

	bWasPlayingLastTick = Interp->bIsPlaying;

	// Make sure fixed time step mode is set correctly based on whether we're currently 'playing' or not
	// We need to do this here because interp sequences can stop without us ever telling them to (and
	// we won't find out about it!)
	UpdateFixedTimeStepPlayback();

	/**Capture key frames and increment the state of recording*/
	UpdateCameraRecording();
}

void  WxInterpEd::UpdateCameraRecording (void)
{
	//if we're recording a real-time camera playback, capture camera frame
	if (RecordingState != MatineeConstants::RECORDING_COMPLETE)
	{
		DOUBLE CurrentTime = appSeconds();
		DOUBLE TimeSinceStateStart = (CurrentTime - RecordingStateStartTime);
		
		switch (RecordingState)
		{
			case MatineeConstants::RECORDING_GET_READY_PAUSE:
				//if time to begin recording
				if (TimeSinceStateStart >= MatineeConstants::CountdownDurationInSeconds)
				{
					//Set the new start time
					RecordingStateStartTime = CurrentTime;
					//change state
					RecordingState = MatineeConstants::RECORDING_ACTIVE;

					//Clear all tracks that think they are recording
					RecordingTracks.Empty();

					//turn off looping!
					bLoopingSection = FALSE;

					// Start Time moving, MUST be done done before set position, as Play rewinds time
					Interp->Play();

					// Move to proper start time
					SetInterpPosition(GetRecordingStartTime());


					//if we're in camera duplication mode
					if ((RecordMode == MatineeConstants::RECORD_MODE_NEW_CAMERA) || (RecordMode == MatineeConstants::RECORD_MODE_NEW_CAMERA_ATTACHED))
					{
						//add new camera
						FEditorLevelViewportClient* LevelVC = GetRecordingViewport();
						if (!LevelVC)
						{
							StopRecordingInterpValues();
							return;
						}

						AActor* ActorToUseForBase = NULL;
						if ((RecordMode == MatineeConstants::RECORD_MODE_NEW_CAMERA_ATTACHED) && (GEditor->GetSelectedActorCount()==1))
						{
							USelection& SelectedActors = *GEditor->GetSelectedActors();
							ActorToUseForBase = CastChecked< AActor >( SelectedActors( 0 ) );
						}

						ACameraActor* NewCam = Cast<ACameraActor>(GEditor->AddActor( ACameraActor::StaticClass(), LevelVC->ViewLocation ));
						NewCam->Rotation				= LevelVC->ViewRotation;
						NewCam->bConstrainAspectRatio	= LevelVC->bConstrainAspectRatio;
						NewCam->AspectRatio				= LevelVC->AspectRatio;
						NewCam->FOVAngle				= LevelVC->ViewFOV;
						//in case we wanted to attach this new camera to an object
						NewCam->Base = ActorToUseForBase;

						//make new group for the camera
						UInterpGroup* NewGroup = ConstructObject<UInterpGroup>( UInterpGroup::StaticClass(), IData, NAME_None, RF_Transactional );
						FString NewGroupName = FString::Printf( LocalizeSecure(LocalizeUnrealEd("CaptureGroup"), *LocalizeUnrealEd("InterpEd_RecordMode_CameraGroupName")) );
						NewGroup->GroupName = FName(*NewGroupName);
						NewGroup->EnsureUniqueName();
						//add new camera group to matinee
						IData->InterpGroups.AddItem(NewGroup);

						//add group instance for camera
						UInterpGroupInst* NewGroupInst = ConstructObject<UInterpGroupInst>( UInterpGroupInst::StaticClass(), Interp, NAME_None, RF_Transactional );
						// Initialise group instance, saving ref to actor it works on.
						NewGroupInst->InitGroupInst(NewGroup, NewCam);
						const INT NewGroupInstIndex = Interp->GroupInst.AddItem(NewGroupInst);

						//Link group with actor
						Interp->InitGroupActorForGroup(NewGroup, NewCam);

						//unselect all, so we can select the newly added tracks
						DeselectAll();

						//Add new tracks to the camera group
						INT MovementTrackIndex = INDEX_NONE;
						UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(AddTrackToGroup( NewGroup, UInterpTrackMove::StaticClass(), NULL, FALSE, MovementTrackIndex, FALSE ));
						check(MoveTrack);
						MoveTrack->MoveFrame = IMF_World;

						//add fov track
						SetTrackAddPropName( FName( TEXT( "FOVAngle" ) ) );
						INT FOVTrackIndex = INDEX_NONE;
						UInterpTrack* FOVTrack = AddTrackToGroup( NewGroup, UInterpTrackFloatProp::StaticClass(), NULL, FALSE, FOVTrackIndex, FALSE );

						//set this group as the preview group
						const UBOOL bResetViewports = FALSE;
						LockCamToGroup(NewGroup, bResetViewports);

						//Select camera tracks
						SelectTrack( NewGroup, MoveTrack , FALSE);
						SelectTrack( NewGroup, FOVTrack, FALSE);

						RecordingTracks.AddItem(MoveTrack);
						RecordingTracks.AddItem(FOVTrack);
					}
					else if ((RecordMode == MatineeConstants::RECORD_MODE_DUPLICATE_TRACKS) && (HasATrackSelected()))
					{
						//duplicate all selected tracks in their respective groups, and clear them
						const UBOOL bDeleteSelectedTracks = FALSE;
						DuplicateSelectedTracksForRecording(bDeleteSelectedTracks);
					} 
					else if ((RecordMode == MatineeConstants::RECORD_MODE_REPLACE_TRACKS) && (HasATrackSelected()))
					{
						const UBOOL bDeleteSelectedTracks = TRUE;
						DuplicateSelectedTracksForRecording(bDeleteSelectedTracks);
					}
					else
					{
						//failed to be in a valid recording state (no track selected, and duplicate or replace)
						StopRecordingInterpValues();
						return;
					}
					
					for (INT i = 0; i < RecordingTracks.Num(); ++i)
					{
						RecordingTracks(i)->bIsRecording = TRUE;
					}

					//Sample state at "Start Time"
					RecordKeys();

					//Save the parent offsets for next frame
					SaveRecordingParentOffsets();
				}
				break;
			case MatineeConstants::RECORDING_ACTIVE:
				{
					//Adjust current state by user input
					AdjustRecordingTracksByInput();

					//apply movement of any parent object to the child object as well (since that movement is no longer processed when recording)
					ApplyRecordingParentOffsets();

					//Sample state at "Start Time"
					RecordKeys();

					//update the parent offsets for next frame
					SaveRecordingParentOffsets();

					//see if we're done recording (accounting for slow mo)
					if (Interp->Position >= GetRecordingEndTime())
					{
						//Set the new start time
						RecordingStateStartTime = CurrentTime;
						//change state
						StopRecordingInterpValues();

						// Stop time if it's playing.
						Interp->Stop();
						// Move to proper start time
						SetInterpPosition(GetRecordingStartTime());
					}
				}
				break;
			default:
				//invalid state
				break;
		}
	}
}



/** Constrains the maximum frame rate to the fixed time step rate when playing back in that mode */
void WxInterpEd::ConstrainFixedTimeStepFrameRate()
{
	// Don't allow the fixed time step playback to run faster than real-time
	if( bSnapToFrames && bFixedTimeStepPlayback )
	{
		// NOTE: Its important that PlaybackStartRealTime and NumContinuousFixedTimeStepFrames are reset
		//    when anything timing-related changes, like GFixedDeltaTime or playback direction.

		DOUBLE CurRealTime = appSeconds();

		// Minor hack to handle changes to world TimeDilation.  We reset our frame rate gate state
		// when we detect a change to time dilation.
		static FLOAT s_LastTimeDilation = GWorld->GetWorldInfo()->TimeDilation;
		if( s_LastTimeDilation != GWorld->GetWorldInfo()->TimeDilation )
		{
			// Looks like time dilation has changed!
			NumContinuousFixedTimeStepFrames = 0;
			PlaybackStartRealTime = CurRealTime;

			s_LastTimeDilation = GWorld->GetWorldInfo()->TimeDilation;
		}

		// How long should have it taken to get to the current frame?
		const DOUBLE ExpectedPlaybackTime =
			NumContinuousFixedTimeStepFrames * GFixedDeltaTime * PlaybackSpeed;

		// How long has it been (in real-time) since we started playback?
		DOUBLE RealTimeSincePlaybackStarted = CurRealTime - PlaybackStartRealTime;

		// If we're way ahead of schedule (more than 5 ms), then we'll perform a long sleep
		FLOAT WaitTime = ExpectedPlaybackTime - RealTimeSincePlaybackStarted;
		if( WaitTime > 5 / 1000.0f )
		{
			appSleep( WaitTime - 3 / 1000.0f );

			// Update timing info after our little snooze
			CurRealTime = appSeconds();
			RealTimeSincePlaybackStarted = CurRealTime - PlaybackStartRealTime;
			WaitTime = ExpectedPlaybackTime - RealTimeSincePlaybackStarted;
		}

		while( RealTimeSincePlaybackStarted < ExpectedPlaybackTime )
		{
			// OK, we're running ahead of schedule so we need to wait a bit before the next frame
			appSleep( 0.0f );

			// Check the time again
			CurRealTime = appSeconds();
			RealTimeSincePlaybackStarted = CurRealTime - PlaybackStartRealTime;
			WaitTime = ExpectedPlaybackTime - RealTimeSincePlaybackStarted;
		}

		// Increment number of continuous fixed time step frames
		++NumContinuousFixedTimeStepFrames;
	}
}

/**
 * Updates the initial rotation-translation matrix for the 
 * given track's inst if the track is a movement track.
 *
 * @param	OwningGroup	The group that owns the track to update. 
 * @param	TrackIndex	The index of the track to update its inst.
 */
void WxInterpEd::UpdateInitialTransformForMoveTrack( UInterpGroup* OwningGroup, UInterpTrackMove* InMoveTrack )
{
	UInterpGroupInst* GroupInst = Interp->FindFirstGroupInst(OwningGroup);
	check(GroupInst);
	
	// Get the index of the passed in track so we can find the track instance.
	const INT TrackIndex = OwningGroup->InterpTracks.FindItemIndex( InMoveTrack );

	UInterpTrackInst* TrackInst = GroupInst->TrackInst(TrackIndex);
	check(TrackInst);

	if( TrackInst->IsA( UInterpTrackInstMove::StaticClass() ) )
	{
		CastChecked<UInterpTrackInstMove>(TrackInst)->CalcInitialTransform( OwningGroup->InterpTracks(TrackIndex), TRUE );
	}
}


static UBOOL bIgnoreActorSelection = FALSE;

void WxInterpEd::SetSelectedFilter(class UInterpFilter* InFilter)
{
	if ( IData->SelectedFilter != InFilter )
	{
		IData->SelectedFilter = InFilter;

		if(InFilter != NULL)
		{
			// Start by hiding all groups and tracks
			for(INT GroupIdx=0; GroupIdx<IData->InterpGroups.Num(); GroupIdx++)
			{
				UInterpGroup* CurGroup = IData->InterpGroups( GroupIdx );
				CurGroup->bVisible = FALSE;

				for( INT CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
				{
					UInterpTrack* CurTrack = CurGroup->InterpTracks( CurTrackIndex );
					CurTrack->bVisible = FALSE;
				}
			}


			// Apply the filter.  This will mark certain groups and tracks as visible.
			InFilter->FilterData( Interp );


			// Make sure folders that are parents to visible groups are ALSO visible!
			for(INT GroupIdx=0; GroupIdx<IData->InterpGroups.Num(); GroupIdx++)
			{
				UInterpGroup* CurGroup = IData->InterpGroups( GroupIdx );
				if( CurGroup->bVisible )
				{
					// Make sure my parent folder group is also visible!
					if( CurGroup->bIsParented )
					{
						UInterpGroup* ParentFolderGroup = FindParentGroupFolder( CurGroup );
						if( ParentFolderGroup != NULL )
						{
							ParentFolderGroup->bVisible = TRUE;
						}
					}
				}
			}
		}
		else
		{
			// No filter, so show all groups and tracks
			for(INT GroupIdx=0; GroupIdx<IData->InterpGroups.Num(); GroupIdx++)
			{
				UInterpGroup* CurGroup = IData->InterpGroups( GroupIdx );
				CurGroup->bVisible = TRUE;

				// Hide tracks
				for( INT CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
				{
					UInterpTrack* CurTrack = CurGroup->InterpTracks( CurTrackIndex );
					CurTrack->bVisible = TRUE;
				}

			}
		}
		

		// The selected group filter may have changed which directly affects the vertical size of the content
		// in the track window, so we'll need to update our scroll bars.
		UpdateTrackWindowScrollBars();

		// Update scroll position
		for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
		{
			if( (*GroupIt)->bVisible )
			{
				ScrollToGroup( *GroupIt );

				// Immediately break because we want to scroll only 
				// to the first selected group that's visible.
				break;
			}
		}
	}
}

/**
 * @return	TRUE if there is at least one selected group. FALSE, otherwise.
 */
UBOOL WxInterpEd::HasAGroupSelected() const
{
	UBOOL bHasAGroupSelected = FALSE;

	for( FSelectedGroupConstIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		// If we reach here, then we have at least one 
		// group selected because the iterator is valid.
		bHasAGroupSelected = TRUE;
		break;
	}

	return bHasAGroupSelected;
}

/**
* @param	GroupClass	The class type of interp group.
* @return	TRUE if there is at least one selected group. FALSE, otherwise.
*/
UBOOL WxInterpEd::HasAGroupSelected( const UClass* GroupClass ) const
{
	// If the user didn't pass in a UInterpGroup derived class, then 
	// they probably made a typo or are calling the wrong function.
	check( GroupClass->IsChildOf(UInterpGroup::StaticClass()) );

	UBOOL bHasAGroupSelected = FALSE;

	for( FSelectedGroupConstIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		if( (*GroupIt)->IsA(GroupClass) )
		{
			bHasAGroupSelected = TRUE;
			break;
		}
	}

	return bHasAGroupSelected;
}

/**
 * @return	TRUE if there is at least one track in the Matinee; FALSE, otherwise. 
 */
UBOOL WxInterpEd::HasATrack() const
{
	UBOOL bHasATrack = FALSE;

	FAllTracksConstIterator AllTracks(IData->InterpGroups);

	// Upon construction, the track iterator will automatically iterate until reaching the first 
	// interp track. If the track iterator is valid, then we have at least one track in the Matinee. 
	if( AllTracks )
	{
		bHasATrack = TRUE;
	}

	return bHasATrack;
}

/**
 * @return	TRUE if there is at least one selected track. FALSE, otherwise.
 */
UBOOL WxInterpEd::HasATrackSelected() const
{
	UBOOL bHasASelectedTrack = FALSE;

	for( FSelectedTrackConstIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		bHasASelectedTrack = TRUE;
		break;
	}

	return bHasASelectedTrack;
}

/**
 * @param	TrackClass	The type of interp track. 
 * @return	TRUE if there is at least one selected track of the given class type. FALSE, otherwise.
 */
UBOOL WxInterpEd::HasATrackSelected( const UClass* TrackClass ) const
{
	// If the user didn't pass in a UInterpTrack derived class, then 
	// they probably made a typo or are calling the wrong function.
	check( TrackClass->IsChildOf(UInterpTrack::StaticClass()) );

	UBOOL bHasASelectedTrack = FALSE;

	for( FSelectedTrackConstIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		if( (*TrackIt)->IsA(TrackClass) )
		{
			bHasASelectedTrack = TRUE;
			break;
		}
	}

	return bHasASelectedTrack;
}

/**
 * @param	OwningGroup	Interp group to check for selected tracks.
 * @return	TRUE if at least one interp track selected owned by the given group; FALSE, otherwise.
 */
UBOOL WxInterpEd::HasATrackSelected( const UInterpGroup* OwningGroup ) const
{
	UBOOL bHasASelectedTrack = FALSE;

	for( TArray<UInterpTrack*>::TConstIterator TrackIt(OwningGroup->InterpTracks); TrackIt; ++TrackIt )
	{
		if( (*TrackIt)->bIsSelected == TRUE )
		{
			bHasASelectedTrack = TRUE;
			break;
		}
	}

	return bHasASelectedTrack;
}

/**
 * @return	TRUE if at least one folder is selected; FALSE, otherwise.
 */
UBOOL WxInterpEd::HasAFolderSelected() const
{
	UBOOL bHasAFolderSelected = FALSE;

	for( FSelectedGroupConstIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		if( (*GroupIt)->bIsFolder )
		{
			bHasAFolderSelected = TRUE;
			break;
		}
	}

	return bHasAFolderSelected;
}

/**
 * @return	TRUE if every single selected group is a folder. 
 */
UBOOL WxInterpEd::AreAllSelectedGroupsFolders() const
{
	// Set return value based on whether a group is selected or not because in the event 
	// that there are no selected groups, then the internals of the loop will never 
	// evaluate. If no groups are selected, then no folders are selected.
	UBOOL bAllFolders = HasAGroupSelected();

	for( FSelectedGroupConstIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		if( (*GroupIt)->bIsFolder == FALSE )
		{
			bAllFolders = FALSE;
			break;
		}
	}

	return bAllFolders;
}

/**
 * @return	TRUE if every single selected group is parented.
 */
UBOOL WxInterpEd::AreAllSelectedGroupsParented() const
{
	// Assume true until we find the first group to not be parented.
	UBOOL bAllGroupsAreParented = TRUE;

	for( FSelectedGroupConstIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		if( !(*GroupIt)->bIsParented )
		{
			// We found a group that is not parented. 
			// We can exit the loop now. 
			bAllGroupsAreParented = FALSE;
			break;
		}
	}

	return bAllGroupsAreParented;
}

/**
 * @param	TrackClass	The class to check against each selected track.
 * @return	TRUE if every single selected track is of the given UClass; FALSE, otherwise.
 */
UBOOL WxInterpEd::AreAllSelectedTracksOfClass( const UClass* TrackClass ) const
{
	UBOOL bResult = TRUE;

	for( FSelectedTrackConstIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		if( !(*TrackIt)->IsA(TrackClass) )
		{
			bResult = FALSE;
			break;
		}
	}

	return bResult;
}

/**
 * @param	OwningGroup	The group to check against each selected track.
 * @return	TRUE if every single selected track is of owned by the given group; FALSE, otherwise.
 */
UBOOL WxInterpEd::AreAllSelectedTracksFromGroup( const UInterpGroup* OwningGroup ) const
{
	UBOOL bResult = TRUE;

	for( FSelectedTrackConstIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		if( !(TrackIt.GetGroup() == OwningGroup) )
		{
			bResult = FALSE;
			break;
		}
	}

	return bResult;
}

/**
 * @return	The number of the selected groups.
 */
INT WxInterpEd::GetSelectedGroupCount() const
{
	INT SelectedGroupCount = 0;

	for( FSelectedGroupConstIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		SelectedGroupCount++;
	}

	return SelectedGroupCount;
}

/**
 * @return	The number of selected tracks. 
 */
INT WxInterpEd::GetSelectedTrackCount() const
{
	INT SelectedTracksTotal = 0;

	for( FSelectedTrackConstIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		SelectedTracksTotal++;
	}

	return SelectedTracksTotal;
}

/**
 * Utility function for gathering all the selected tracks into a TArray.
 *
 * @param	OutSelectedTracks	[out] An array of all interp tracks that are currently-selected.
 */
void WxInterpEd::GetSelectedTracks( TArray<UInterpTrack*>& OutTracks )
{
	// Make sure there aren't any existing items in the array in case they are non-selected tracks.
	OutTracks.Empty();

	for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		OutTracks.AddItem(*TrackIt);
	}
}

/**
 * Utility function for gathering all the selected groups into a TArray.
 *
 * @param	OutSelectedGroups	[out] An array of all interp groups that are currently-selected.
 */
void WxInterpEd::GetSelectedGroups( TArray<UInterpGroup*>& OutSelectedGroups )
{
	// Make sure there aren't any existing items in the array in case they are non-selected groups.
	OutSelectedGroups.Empty();
	
	for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		OutSelectedGroups.AddItem(*GroupIt);
	}
}

/**
 * Selects the interp track at the given index in the given group's array of interp tracks.
 * If the track is already selected, this function will do nothing. 
 *
 * @param	OwningGroup				The group that stores the interp track to be selected. Cannot be NULL.
 * @param	TrackToSelect			The interp track to select. Cannot be NULL
 * @param	bDeselectPreviousTracks	If TRUE, then all previously-selected tracks will be deselected. Defaults to TRUE.
 */
void WxInterpEd::SelectTrack( UInterpGroup* OwningGroup, UInterpTrack* TrackToSelect, UBOOL bDeselectPreviousTracks /*= TRUE*/ )
{
	check( OwningGroup && TrackToSelect );

	const UBOOL bTrackAlreadySelected = TrackToSelect->bIsSelected;
	const UBOOL bWantsOtherTracksDeselected = ( bDeselectPreviousTracks && ( GetSelectedTrackCount() > 1 ) );

	// Early out if we already have the track selected or if there are multiple 
	// tracks and the user does not want all but the given track selected.
	if( bTrackAlreadySelected && !bWantsOtherTracksDeselected )
	{
		return;
	}

	// By default, the previously-selected tracks should be deselected. However, the client 
	// code has the option of not deselecting, especially when multi-selecting tracks.
	if( bDeselectPreviousTracks )
	{
		DeselectAllTracks();
	}

	// By selecting a track, we must deselect all selected groups.
	// We can only have one or the other. 
	DeselectAllGroups(FALSE);

	// If this track has an Actor, select it (if not already).
	SelectGroupActor( OwningGroup );

	UBOOL bFoundExistingGroup = FALSE;

	// Select the track
	TrackToSelect->bIsSelected = TRUE;

	// Update the property window to reflect the properties of the selected track.
	UpdatePropertyWindow();

	// Highlight the selected curve.
	IData->CurveEdSetup->ChangeCurveColor(TrackToSelect, SelectedCurveColor);
	CurveEd->UpdateDisplay();
}


/**
 * Selects the given group.
 *
 * @param	GroupToSelect			The desired group to select. Cannot be NULL.
 * @param	bDeselectPreviousGroups	If TRUE, then all previously-selected groups will be deselected. Defaults to TRUE.
 */
void WxInterpEd::SelectGroup( UInterpGroup* GroupToSelect, UBOOL bDeselectPreviousGroups /*= TRUE*/ )
{
	// Must be a valid interp group.
	check( GroupToSelect );

	// First, deselect the previously-selected groups by default. The client code has 
	// the option to prevent this, especially for case such as multi-group select.
	if( bDeselectPreviousGroups )
	{
		DeselectAllGroups();
	}

	// By selecting a group, we must deselect any selected tracks.
	DeselectAllTracks(FALSE);

	// Select the group now!
	GroupToSelect->bIsSelected = TRUE;

	// Also select the actor associated to the selected interp group.
	SelectGroupActor(GroupToSelect);

	// Update the property window according to the new selection.
	UpdatePropertyWindow();

	// Dirty the display
	InvalidateTrackWindowViewports();
}

/**
 * Deselects the interp track store in the given group at the given array index.
 *
 * @param	OwningGroup				The group that stores the interp track to be deselected. Cannot be NULL.
 * @param	TrackToDeselect			The track to deslect. Cannot be NULL
 * @param	bUpdateVisuals			If TRUE, then all affected visual components related to track selections will be updated. Defaults to TRUE.
 */
void WxInterpEd::DeselectTrack( UInterpGroup* OwningGroup, UInterpTrack* TrackToDeselect, UBOOL bUpdateVisuals /*= TRUE*/  )
{
	check( OwningGroup && TrackToDeselect );

	const UBOOL bAllTracksFromSameGroup = AreAllSelectedTracksFromGroup(OwningGroup);
	const UBOOL bHasMultipleTracksSelected = (GetSelectedTrackCount() > 1);


	TrackToDeselect->bIsSelected = FALSE;

	// The client code has the option of opting out of updating the 
	// visual components that are affected by selecting tracks. 
	if( bUpdateVisuals )
	{
		// Update the curve corresponding to this track
		IData->CurveEdSetup->ChangeCurveColor( TrackToDeselect, OwningGroup->GroupColor );
		CurveEd->UpdateDisplay();

		// Deselect an associated actor.
		if( bHasMultipleTracksSelected )
		{
			// Deselect only the specific associated actor if there 
			// are multiple tracks selected over multiple groups.
			if( !bAllTracksFromSameGroup )
			{
				DeselectGroupActor(OwningGroup);
			}

			// If there are multiple tracks selected from the same 
			// group, then we don't want to deselect an actor. 
		}
		// Otherwise, there is only one track selected, so deselect that. 
		else
		{
			GUnrealEd->SelectNone( TRUE, TRUE );
		}

		// Update the property window to reflect the properties of the selected track.
		UpdatePropertyWindow();
	}


	// Clear any keys related to this track.
	ClearKeySelectionForTrack( OwningGroup, TrackToDeselect, FALSE );

	// Always invalidate track windows
	InvalidateTrackWindowViewports();
}

/**
 * Deselects every selected track. 
 *
 * @param	bUpdateVisuals	If TRUE, then all affected visual components related to track selections will be updated. Defaults to TRUE.
 */
void WxInterpEd::DeselectAllTracks( UBOOL bUpdateVisuals /*= TRUE*/ )
{
	// Deselect all selected tracks and remove their matching curves.
	for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		UInterpTrack* CurrentTrack = *TrackIt;
		IData->CurveEdSetup->ChangeCurveColor(CurrentTrack, TrackIt.GetGroup()->GroupColor);
		CurrentTrack->bIsSelected = FALSE;
	}

	// The client code has the option of opting out of updating the 
	// visual components that are affected by selecting tracks. 
	if( bUpdateVisuals )
	{
		// Update the curve editor to reflect the curve color change
		CurveEd->UpdateDisplay();

		// Make sure there are no selected actors because selecting 
		// tracks in Matinee will select the associated actor.
		GUnrealEd->SelectNone( TRUE, TRUE );

		// Make sure there is nothing selected in the property 
		// window or in the level editing viewports.
		UpdatePropertyWindow();
	}

	// Make sure all keys are cleared!
	ClearKeySelection();
}

/**
 * Deselects the given group.
 *
 * @param	GroupToDeselect	The desired group to deselect.
 * @param	bUpdateVisuals	If TRUE, then all affected visual components related to group selections will be updated. Defaults to TRUE.
 */
void WxInterpEd::DeselectGroup( UInterpGroup* GroupToDeselect, UBOOL bUpdateVisuals /*= TRUE*/ )
{
	GroupToDeselect->bIsSelected = FALSE;

	// Deselect the actor associated to this interp group as well.
	DeselectGroupActor(GroupToDeselect);

	// The client code has the option of opting out of updating the 
	// visual components that are affected by selecting groups. 
	if( bUpdateVisuals )
	{
		// Make sure there is nothing selected in the property window
		UpdatePropertyWindow();

		// Request an update of the track windows
		InvalidateTrackWindowViewports();
	}
}

/**
 * Deselects all selected groups.
 *
 * @param	bUpdateVisuals	If TRUE, then all affected visual components related to group selections will be updated. Defaults to TRUE.
 */
void WxInterpEd::DeselectAllGroups( UBOOL bUpdateVisuals /*= TRUE*/ )
{
	for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		(*GroupIt)->bIsSelected = FALSE;
	}

	// The client code has the option of opting out of updating the 
	// visual components that are affected by selecting groups. 
	if( bUpdateVisuals )
	{
		// Make sure there are no selected actors because selecting 
		// groups in Matinee will select the associated actor.
		GUnrealEd->SelectNone( TRUE, TRUE );

		// Update the property window to reflect the group deselection
		UpdatePropertyWindow();

		// Request an update of the track windows
		InvalidateTrackWindowViewports();
	}
}

/**
 * Deselects all selected groups or selected tracks. 
 *
 * @param	bUpdateVisuals	If TRUE, then all affected visual components related to group selections will be updated. Defaults to TRUE.
 */
void WxInterpEd::DeselectAll( UBOOL bUpdateVisuals /*= TRUE*/ )
{
	// We either have one-to-many groups selected or one-to-many tracks selected. 
	// So, we need to check which one it is. 
	if( HasAGroupSelected() )
	{
		DeselectAllGroups(bUpdateVisuals);
	}
	else if( HasATrackSelected() )
	{
		DeselectAllTracks(bUpdateVisuals);
	}
}

void WxInterpEd::ClearKeySelection()
{
	Opt->SelectedKeys.Empty();
	Opt->bAdjustingKeyframe = FALSE;
	Opt->bAdjustingGroupKeyframes = FALSE;

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();
}

/**
 * Clears all selected key of a given track.
 *
 * @param	OwningGroup			The group that owns the track containing the keys. 
 * @param	Track				The track holding the keys to clear. 
 * @param	bInvalidateDisplay	Sets the Matinee track viewport to refresh (Defaults to TRUE).
 */
void WxInterpEd::ClearKeySelectionForTrack( UInterpGroup* OwningGroup, UInterpTrack* Track, UBOOL bInvalidateDisplay )
{
	for( INT SelectedKeyIndex = 0; SelectedKeyIndex < Opt->SelectedKeys.Num(); SelectedKeyIndex++ )
	{
		// Remove key selections only for keys matching the given group and track index.
		if( (Opt->SelectedKeys(SelectedKeyIndex).Group == OwningGroup) && (Opt->SelectedKeys(SelectedKeyIndex).Track == Track) )
		{
			Opt->SelectedKeys.Remove(SelectedKeyIndex--);
		}
	}

	// If there are no more keys selected, then the user is not adjusting keyframes anymore.
	Opt->bAdjustingKeyframe = (Opt->SelectedKeys.Num() == 1);
	Opt->bAdjustingGroupKeyframes = (Opt->SelectedKeys.Num() > 1);

	// Dirty the track window viewports
	if( bInvalidateDisplay )
	{
		InvalidateTrackWindowViewports();
	}
}

void WxInterpEd::AddKeyToSelection(UInterpGroup* InGroup, UInterpTrack* InTrack, INT InKeyIndex, UBOOL bAutoWind)
{
	check(InGroup);
	check(InTrack);

	check( InKeyIndex >= 0 && InKeyIndex < InTrack->GetNumKeyframes() );

	// If the sequence is currently playing, stop it before selecting the key.
	// This check is necessary because calling StopPlaying if playback is stopped will zero
	// the playback position, which we don't want to do.
	if ( Interp->bIsPlaying )
	{
		StopPlaying();
	}

	// If key is not already selected, add to selection set.
	if( !KeyIsInSelection(InGroup, InTrack, InKeyIndex) )
	{
		// Add to array of selected keys.
		Opt->SelectedKeys.AddItem( FInterpEdSelKey(InGroup, InTrack, InKeyIndex) );
	}

	// If this is the first and only keyframe selected, make track active and wind to it.
	if(Opt->SelectedKeys.Num() == 1 && bAutoWind)
	{
		FLOAT KeyTime = InTrack->GetKeyframeTime(InKeyIndex);
		SetInterpPosition(KeyTime);

		// When jumping to keyframe, update the pivot so the widget is in the right place.
		UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(InGroup);
		if(GrInst)
		{
			AActor * GrActor = GrInst->GetGroupActor();
			if (GrActor)
			{
				GEditor->SetPivot( GrActor->Location, FALSE, TRUE );
			}
		}

		Opt->bAdjustingKeyframe = TRUE;
	}

	if(Opt->SelectedKeys.Num() != 1)
	{
		Opt->bAdjustingKeyframe = FALSE;
	}

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();
}

void WxInterpEd::RemoveKeyFromSelection(UInterpGroup* InGroup, UInterpTrack* InTrack, INT InKeyIndex)
{
	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		if( Opt->SelectedKeys(i).Group == InGroup && 
			Opt->SelectedKeys(i).Track == InTrack && 
			Opt->SelectedKeys(i).KeyIndex == InKeyIndex )
		{
			Opt->SelectedKeys.Remove(i);

			// If there are no more keys selected, then the user is not adjusting keyframes anymore.
			Opt->bAdjustingKeyframe = (Opt->SelectedKeys.Num() == 1);
			Opt->bAdjustingGroupKeyframes = (Opt->SelectedKeys.Num() > 1);

			// Dirty the track window viewports
			InvalidateTrackWindowViewports();

			return;
		}
	}
}

UBOOL WxInterpEd::KeyIsInSelection(UInterpGroup* InGroup, UInterpTrack* InTrack, INT InKeyIndex)
{
	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		if( Opt->SelectedKeys(i).Group == InGroup && 
			Opt->SelectedKeys(i).Track == InTrack && 
			Opt->SelectedKeys(i).KeyIndex == InKeyIndex )
			return TRUE;
	}

	return FALSE;
}

/** Clear selection and then select all keys within the gree loop-section. */
void WxInterpEd::SelectKeysInLoopSection()
{
	ClearKeySelection();

	// Add keys that are within current section to selection
	for(INT i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups(i);
		for(INT j=0; j<Group->InterpTracks.Num(); j++)
		{
			UInterpTrack* Track = Group->InterpTracks(j);
			Track->Modify();

			for(INT k=0; k<Track->GetNumKeyframes(); k++)
			{
				// Add keys in section to selection for deletion.
				FLOAT KeyTime = Track->GetKeyframeTime(k);
				if(KeyTime >= IData->EdSectionStart && KeyTime <= IData->EdSectionEnd)
				{
					// Add to selection for deletion.
					AddKeyToSelection(Group, Track, k, FALSE);
				}
			}
		}
	}
}

/** Calculate the start and end of the range of the selected keys. */
void WxInterpEd::CalcSelectedKeyRange(FLOAT& OutStartTime, FLOAT& OutEndTime)
{
	if(Opt->SelectedKeys.Num() == 0)
	{
		OutStartTime = 0.f;
		OutEndTime = 0.f;
	}
	else
	{
		OutStartTime = BIG_NUMBER;
		OutEndTime = -BIG_NUMBER;

		for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
		{
			UInterpTrack* Track = Opt->SelectedKeys(i).Track;
			FLOAT KeyTime = Track->GetKeyframeTime( Opt->SelectedKeys(i).KeyIndex );

			OutStartTime = ::Min(KeyTime, OutStartTime);
			OutEndTime = ::Max(KeyTime, OutEndTime);
		}
	}
}

//Deletes keys if they are selected, otherwise will deleted selected tracks or groups
void WxInterpEd::DeleteSelection (void)
{
	if (Opt->SelectedKeys.Num() > 0)
	{
		DeleteSelectedKeys(TRUE);
	}
	else if (GetSelectedTrackCount() > 0)
	{
		DeleteSelectedTracks();
	}
	else if (GetSelectedGroupCount())
	{
		DeleteSelectedGroups();
	}
}


void WxInterpEd::DeleteSelectedKeys(UBOOL bDoTransaction)
{
	if(bDoTransaction)
	{
		InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("DeleteSelectedKeys") );
		Interp->Modify();
		Opt->Modify();
	}

	TArray<UInterpTrack*> ModifiedTracks;

	UBOOL bRemovedEventKeys = FALSE;
	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(i);
		UInterpTrack* Track = SelKey.Track;

		check(Track);
		check(SelKey.KeyIndex >= 0 && SelKey.KeyIndex < Track->GetNumKeyframes());

		if(bDoTransaction)
		{
			// If not already done so, call Modify on this track now.
			if( !ModifiedTracks.ContainsItem(Track) )
			{
				Track->Modify();
				ModifiedTracks.AddItem(Track);
			}
		}

		// If this is an event key - we update the connectors later.
		if(Track->IsA(UInterpTrackEvent::StaticClass()))
		{
			bRemovedEventKeys = TRUE;
		}
			
		Track->RemoveKeyframe(SelKey.KeyIndex);

		// If any other keys in the selection are on the same track but after the one we just deleted, decrement the index to correct it.
		for(INT j=0; j<Opt->SelectedKeys.Num(); j++)
		{
			if( Opt->SelectedKeys(j).Group == SelKey.Group &&
				Opt->SelectedKeys(j).Track == SelKey.Track &&
				Opt->SelectedKeys(j).KeyIndex > SelKey.KeyIndex &&
				j != i)
			{
				Opt->SelectedKeys(j).KeyIndex--;
			}
		}

		const UBOOL bRemovedFirstKey = (0 == SelKey.KeyIndex);

		if( bRemovedFirstKey  )
		{
			// Determine if a movement track or a subtrack of a movement track was modified.
			UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>( SelKey.Track );
			if( !MoveTrack )
			{
				UInterpTrackMoveAxis* MoveSubTrack = Cast<UInterpTrackMoveAxis>( SelKey.Track );
				if( MoveSubTrack )
				{
					MoveTrack = CastChecked<UInterpTrackMove>( MoveSubTrack->GetOuter() );
				}
			}

			if( MoveTrack )
			{
				// When the first key is deleted, we must update the initial transform for a RelativeToInitial 
				// move track because the transforms are not referring the new first key. 
				UpdateInitialTransformForMoveTrack( SelKey.Group, MoveTrack);
			}
		}
	}

	// If we removed some event keys - ensure all Matinee actions are up to date.
	if(bRemovedEventKeys)
	{
		UpdateMatineeActionConnectors();
	}

	// Update positions at current time, in case removal of the key changed things.
	RefreshInterpPosition();

	// Select no keyframe.
	ClearKeySelection();

	if(bDoTransaction)
	{
		InterpEdTrans->EndSpecial();
	}
}

void WxInterpEd::DuplicateSelectedKeys()
{
	InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("DuplicateSelectedKeys") );
	Interp->Modify();
	Opt->Modify();

	TArray<UInterpTrack*> ModifiedTracks;

	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(i);
		UInterpTrack* Track = SelKey.Track;

		check(Track);
		check(SelKey.KeyIndex >= 0 && SelKey.KeyIndex < Track->GetNumKeyframes());

		// If not already done so, call Modify on this track now.
		if( !ModifiedTracks.ContainsItem(Track) )
		{
			Track->Modify();
			ModifiedTracks.AddItem(Track);
		}
		
		FLOAT CurrentKeyTime = Track->GetKeyframeTime(SelKey.KeyIndex);
		FLOAT NewKeyTime = CurrentKeyTime + (FLOAT)DuplicateKeyOffset/PixelsPerSec;

		INT DupKeyIndex = Track->DuplicateKeyframe(SelKey.KeyIndex, NewKeyTime);

		// Change selection to select the new keyframe instead.
		SelKey.KeyIndex = DupKeyIndex;

		// If any other keys in the selection are on the same track but after the new key, increase the index to correct it.
		for(INT j=0; j<Opt->SelectedKeys.Num(); j++)
		{
			if( Opt->SelectedKeys(j).Group == SelKey.Group &&
				Opt->SelectedKeys(j).Track == SelKey.Track &&
				Opt->SelectedKeys(j).KeyIndex >= DupKeyIndex &&
				j != i)
			{
				Opt->SelectedKeys(j).KeyIndex++;
			}
		}
	}

	InterpEdTrans->EndSpecial();
}

/** Adjust the view so the entire sequence fits into the viewport. */
void WxInterpEd::ViewFitSequence()
{
	ViewStartTime = 0.f;
	ViewEndTime = IData->InterpLength;

	SyncCurveEdView();
}



/** Adjust the view so the selected keys fit into the viewport. */
void WxInterpEd::ViewFitToSelected()
{
	if( Opt->SelectedKeys.Num() > 0 )
	{
		FLOAT NewStartTime = BIG_NUMBER;
		FLOAT NewEndTime = -BIG_NUMBER;

		for( INT CurKeyIndex = 0; CurKeyIndex < Opt->SelectedKeys.Num(); ++CurKeyIndex )
		{
			FInterpEdSelKey& CurSelKey = Opt->SelectedKeys( CurKeyIndex );
			
			UInterpTrack* Track = CurSelKey.Track;
			check( Track != NULL );
			check( CurSelKey.KeyIndex >= 0 && CurSelKey.KeyIndex < Track->GetNumKeyframes() );

			NewStartTime = Min( Track->GetKeyframeTime( CurSelKey.KeyIndex ), NewStartTime );
			NewEndTime = Max( Track->GetKeyframeTime( CurSelKey.KeyIndex ), NewEndTime );
		}

		// Clamp the minimum size
		if( NewStartTime - NewEndTime < 0.001f )
		{
			NewStartTime -= 0.005f;
			NewEndTime += 0.005f;
		}

		ViewStartTime = NewStartTime;
		ViewEndTime = NewEndTime;

		SyncCurveEdView();
	}
}



/** Adjust the view so the looped section fits into the viewport. */
void WxInterpEd::ViewFitLoop()
{
	// Do nothing if loop section is too small!
	FLOAT LoopRange = IData->EdSectionEnd - IData->EdSectionStart;
	if(LoopRange > 0.01f)
	{
		ViewStartTime = IData->EdSectionStart;
		ViewEndTime = IData->EdSectionEnd;

		SyncCurveEdView();
	}
}

/** Adjust the view so the looped section fits into the entire sequence. */
void WxInterpEd::ViewFitLoopSequence()
{
	// Adjust the looped section
	IData->EdSectionStart = 0.0f;
	IData->EdSectionEnd = IData->InterpLength;

	// Adjust the view
	ViewStartTime = IData->EdSectionStart;
	ViewEndTime = IData->EdSectionEnd;

	SyncCurveEdView();
}

/** Move the view to the end of the currently selected track(s). */
void WxInterpEd::ViewEndOfTrack()
{
	FLOAT NewEndTime = 0.0f;
	
	if( GetSelectedTrackCount() > 0 )
	{
		FSelectedTrackIterator TrackIt(GetSelectedTrackIterator());
		for( TrackIt; TrackIt; ++TrackIt )
		{
			UInterpTrack* Track = *TrackIt;
			if (Track->GetTrackEndTime() > NewEndTime)
			{
				NewEndTime = Track->GetTrackEndTime();
			}
		}
	}
	else // If no track is selected, move to the end of the sequence
	{
		NewEndTime = IData->InterpLength;
	}
	
	ViewStartTime = NewEndTime - (ViewEndTime - ViewStartTime);
	ViewEndTime = NewEndTime;

	SyncCurveEdView();
}

/** Adjust the view by the defined range. */
void WxInterpEd::ViewFit(FLOAT StartTime, FLOAT EndTime)
{
	ViewStartTime = StartTime;
	ViewEndTime = EndTime;

	SyncCurveEdView();
}

/** Iterate over keys changing their interpolation mode and adjusting tangents appropriately. */
void WxInterpEd::ChangeKeyInterpMode(EInterpCurveMode NewInterpMode/*=CIM_Unknown*/)
{
	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(i);
		UInterpTrack* Track = SelKey.Track;

		UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(Track);
		if(MoveTrack)
		{
			MoveTrack->PosTrack.Points(SelKey.KeyIndex).InterpMode = NewInterpMode;
			MoveTrack->EulerTrack.Points(SelKey.KeyIndex).InterpMode = NewInterpMode;

			MoveTrack->PosTrack.AutoSetTangents(MoveTrack->LinCurveTension);
			MoveTrack->EulerTrack.AutoSetTangents(MoveTrack->AngCurveTension);
		}

		UInterpTrackFloatBase* FloatTrack = Cast<UInterpTrackFloatBase>(Track);
		if(FloatTrack)
		{
			FloatTrack->FloatTrack.Points(SelKey.KeyIndex).InterpMode = NewInterpMode;

			FloatTrack->FloatTrack.AutoSetTangents(FloatTrack->CurveTension);
		}

		UInterpTrackVectorBase* VectorTrack = Cast<UInterpTrackVectorBase>(Track);
		if(VectorTrack)
		{
			VectorTrack->VectorTrack.Points(SelKey.KeyIndex).InterpMode = NewInterpMode;

			VectorTrack->VectorTrack.AutoSetTangents(VectorTrack->CurveTension);
		}

		UInterpTrackLinearColorBase* LinearColorTrack = Cast<UInterpTrackLinearColorBase>(Track);
		if(LinearColorTrack)
		{
			LinearColorTrack->LinearColorTrack.Points(SelKey.KeyIndex).InterpMode = NewInterpMode;

			LinearColorTrack->LinearColorTrack.AutoSetTangents(LinearColorTrack->CurveTension);
		}
	}

	CurveEd->UpdateDisplay();
}

/** Increments the cursor or selected keys by 1 interval amount, as defined by the toolbar combo. */
void WxInterpEd::IncrementSelection()
{
	UBOOL bMoveMarker = TRUE;

	if(Opt->SelectedKeys.Num())
	{
		BeginMoveSelectedKeys();
		{
			MoveSelectedKeys(SnapAmount);
		}
		EndMoveSelectedKeys();
		bMoveMarker = FALSE;
	}


	// Move the interp marker if there are no keys selected.
	if(bMoveMarker)
	{
		FLOAT StartTime = Interp->Position;
		if( bSnapToFrames && bSnapTimeToFrames )
		{
			StartTime = SnapTimeToNearestFrame( Interp->Position );
		}

		SetInterpPosition( StartTime + SnapAmount );
	}
}

/** Decrements the cursor or selected keys by 1 interval amount, as defined by the toolbar combo. */
void WxInterpEd::DecrementSelection()
{
	UBOOL bMoveMarker = TRUE;

	if(Opt->SelectedKeys.Num())
	{
		BeginMoveSelectedKeys();
		{
			MoveSelectedKeys(-SnapAmount);
		}
		EndMoveSelectedKeys();
		bMoveMarker = FALSE;
	}

	// Move the interp marker if there are no keys selected.
	if(bMoveMarker)
	{
		FLOAT StartTime = Interp->Position;
		if( bSnapToFrames && bSnapTimeToFrames )
		{
			StartTime = SnapTimeToNearestFrame( Interp->Position );
		}

		SetInterpPosition( StartTime - SnapAmount );
	}
}

void WxInterpEd::SelectNextKey()
{
	// Keyframe operations can only happen when only one track is selected
	if( GetSelectedTrackCount() == 1 )
	{
		FSelectedTrackIterator TrackIt(GetSelectedTrackIterator());
		UInterpTrack* Track = *TrackIt;

		INT NumKeys = Track->GetNumKeyframes();

		if(NumKeys)
		{
			INT i;
			for(i=0; i < NumKeys-1 && Track->GetKeyframeTime(i) < (Interp->Position + KINDA_SMALL_NUMBER); i++);
	
			ClearKeySelection();
			AddKeyToSelection(TrackIt.GetGroup(), Track, i, TRUE);
		}
	}
}

void WxInterpEd::SelectPreviousKey()
{
	// Keyframe operations can only happen when only one track is selected
	if( GetSelectedTrackCount() == 1 )
	{
		FSelectedTrackIterator TrackIt(GetSelectedTrackIterator());
		UInterpTrack* Track = *TrackIt;

		INT NumKeys = Track->GetNumKeyframes();

		if(NumKeys)
		{
			INT i;
			for(i=NumKeys-1; i > 0 && Track->GetKeyframeTime(i) > (Interp->Position - KINDA_SMALL_NUMBER); i--);

			ClearKeySelection();
			AddKeyToSelection(TrackIt.GetGroup(), Track, i, TRUE);
		}
	}
}

/** Turns snap on and off in Matinee. Updates state of snap button as well. */
void WxInterpEd::SetSnapEnabled(UBOOL bInSnapEnabled)
{
	bSnapEnabled = bInSnapEnabled;

	if(bSnapToKeys)
	{
		CurveEd->SetInSnap(FALSE, SnapAmount, bSnapToFrames);
	}
	else
	{
		CurveEd->SetInSnap(bSnapEnabled, SnapAmount, bSnapToFrames);
	}

	// Update button status.
	ToolBar->ToggleTool(IDM_INTERP_TOGGLE_SNAP, bSnapEnabled == TRUE);
	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("SnapEnabled"), bSnapEnabled, GEditorUserSettingsIni );
}


/** Toggles snapping the current timeline position to 'frames' in Matinee. */
void WxInterpEd::SetSnapTimeToFrames( UBOOL bInValue )
{
	bSnapTimeToFrames = bInValue;

	// Update button status.
	ToolBar->ToggleTool( IDM_INTERP_TOGGLE_SNAP_TIME_TO_FRAMES, ( bSnapToFrames && bSnapTimeToFrames ) );
	ToolBar->EnableTool( IDM_INTERP_TOGGLE_SNAP_TIME_TO_FRAMES, bSnapToFrames == TRUE );
	
	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("SnapTimeToFrames"), bSnapTimeToFrames, GEditorUserSettingsIni );

	// Go ahead and apply the change right now if we need to
	if( IsInitialized() && bSnapToFrames && bSnapTimeToFrames )
	{
		SetInterpPosition( SnapTimeToNearestFrame( Interp->Position ) );
	}
}



/** Toggles fixed time step mode */
void WxInterpEd::SetFixedTimeStepPlayback( UBOOL bInValue )
{
	bFixedTimeStepPlayback = bInValue;

	// Update button status.
	ToolBar->ToggleTool( IDM_INTERP_FixedTimeStepPlayback, ( bSnapToFrames && bFixedTimeStepPlayback ) );
	ToolBar->EnableTool( IDM_INTERP_FixedTimeStepPlayback, bSnapToFrames == TRUE );
	
	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("FixedTimeStepPlayback"), bFixedTimeStepPlayback, GEditorUserSettingsIni );

	// Update fixed time step state
	UpdateFixedTimeStepPlayback();
}



/** Updates 'fixed time step' mode based on current playback state and user preferences */
void WxInterpEd::UpdateFixedTimeStepPlayback()
{
	// Turn on 'benchmarking' mode if we're using a fixed time step
	GIsBenchmarking = Interp->bIsPlaying && bSnapToFrames && bFixedTimeStepPlayback;

	// Set the time interval between fixed ticks
	GFixedDeltaTime = SnapAmount;
}



/** Toggles 'prefer frame numbers' setting */
void WxInterpEd::SetPreferFrameNumbers( UBOOL bInValue )
{
	bPreferFrameNumbers = bInValue;

	// Update button status.
	ToolBar->ToggleTool( IDM_INTERP_PreferFrameNumbers, ( bSnapToFrames && bPreferFrameNumbers ) );
	ToolBar->EnableTool( IDM_INTERP_PreferFrameNumbers, bSnapToFrames == TRUE );
	
	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("PreferFrameNumbers"), bPreferFrameNumbers, GEditorUserSettingsIni );
}



/** Toggles 'show time cursor pos for all keys' setting */
void WxInterpEd::SetShowTimeCursorPosForAllKeys( UBOOL bInValue )
{
	bShowTimeCursorPosForAllKeys = bInValue;

	// Update button status.
	ToolBar->ToggleTool( IDM_INTERP_ShowTimeCursorPosForAllKeys, bShowTimeCursorPosForAllKeys ? TRUE : FALSE );
	ToolBar->EnableTool( IDM_INTERP_ShowTimeCursorPosForAllKeys, TRUE );
	
	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("ShowTimeCursorPosForAllKeys"), bShowTimeCursorPosForAllKeys, GEditorUserSettingsIni );
}



/** Snaps the specified time value to the closest frame */
FLOAT WxInterpEd::SnapTimeToNearestFrame( FLOAT InTime ) const
{
	// Compute the new time value by rounding
	const INT InterpPositionInFrames = appRound( InTime / SnapAmount );
	const FLOAT NewTime = InterpPositionInFrames * SnapAmount;

	return NewTime;
}



/** Take the InTime and snap it to the current SnapAmount. Does nothing if bSnapEnabled is FALSE */
FLOAT WxInterpEd::SnapTime(FLOAT InTime, UBOOL bIgnoreSelectedKeys)
{
	if(bSnapEnabled)
	{
		if(bSnapToKeys)
		{
			// Iterate over all tracks finding the closest snap position to the supplied time.

			UBOOL bFoundSnap = FALSE;
			FLOAT BestSnapPos = 0.f;
			FLOAT BestSnapDist = BIG_NUMBER;
			for(INT i=0; i<IData->InterpGroups.Num(); i++)
			{
				UInterpGroup* Group = IData->InterpGroups(i);
				for(INT j=0; j<Group->InterpTracks.Num(); j++)
				{
					UInterpTrack* Track = Group->InterpTracks(j);

					// If we are ignoring selected keys - build an array of the indices of selected keys on this track.
					TArray<INT> IgnoreKeys;
					if(bIgnoreSelectedKeys)
					{
						for(INT SelIndex=0; SelIndex<Opt->SelectedKeys.Num(); SelIndex++)
						{
							if( Opt->SelectedKeys(SelIndex).Group == Group && 
								Opt->SelectedKeys(SelIndex).Track == Track )
							{
								IgnoreKeys.AddUniqueItem( Opt->SelectedKeys(SelIndex).KeyIndex );
							}
						}
					}

					FLOAT OutPos = 0.f;
					UBOOL bTrackSnap = Track->GetClosestSnapPosition(InTime, IgnoreKeys, OutPos);
					if(bTrackSnap) // If we found a snap location
					{
						// See if its closer than the closest location so far.
						FLOAT SnapDist = Abs(InTime - OutPos);
						if(SnapDist < BestSnapDist)
						{
							BestSnapPos = OutPos;
							BestSnapDist = SnapDist;
							bFoundSnap = TRUE;
						}
					}
				}
			}

			// Find how close we have to get to snap, in 'time' instead of pixels.
			FLOAT SnapTolerance = (FLOAT)KeySnapPixels/(FLOAT)PixelsPerSec;

			// If we are close enough to snap position - do it.
			if(bFoundSnap && (BestSnapDist < SnapTolerance))
			{
				bDrawSnappingLine = TRUE;
				SnappingLinePosition = BestSnapPos;

				return BestSnapPos;
			}
			else
			{
				bDrawSnappingLine = FALSE;

				return InTime;
			}
		}
		else
		{	
			// Don't draw snapping line when just snapping to grid.
			bDrawSnappingLine = FALSE;

			return SnapTimeToNearestFrame( InTime );
		}
	}
	else
	{
		bDrawSnappingLine = FALSE;

		return InTime;
	}
}

void WxInterpEd::BeginMoveMarker()
{
	if(GrabbedMarkerType == ISM_SeqEnd)
	{
		UnsnappedMarkerPos = IData->InterpLength;
		InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("MoveEndMarker") );
		IData->Modify();
	}
	else if(GrabbedMarkerType == ISM_LoopStart)
	{
		UnsnappedMarkerPos = IData->EdSectionStart;
		InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("MoveLoopStartMarker") );
		IData->Modify();
	}
	else if(GrabbedMarkerType == ISM_LoopEnd)
	{
		UnsnappedMarkerPos = IData->EdSectionEnd;
		InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("MoveLoopEndMarker") );
		IData->Modify();
	}
}

void WxInterpEd::EndMoveMarker()
{
	if(	GrabbedMarkerType == ISM_SeqEnd || 
		GrabbedMarkerType == ISM_LoopStart || 
		GrabbedMarkerType == ISM_LoopEnd)
	{
		InterpEdTrans->EndSpecial();
	}
}

void WxInterpEd::SetInterpEnd(FLOAT NewInterpLength)
{
	// Ensure non-negative end time.
	IData->InterpLength = ::Max(NewInterpLength, 0.f);
	
	CurveEd->SetEndMarker(TRUE, IData->InterpLength);

	// Ensure the current position is always inside the valid sequence area.
	if(Interp->Position > IData->InterpLength)
	{
		SetInterpPosition(IData->InterpLength);
	}

	// Ensure loop points are inside sequence.
	IData->EdSectionStart = ::Clamp(IData->EdSectionStart, 0.f, IData->InterpLength);
	IData->EdSectionEnd = ::Clamp(IData->EdSectionEnd, 0.f, IData->InterpLength);
	CurveEd->SetRegionMarker(TRUE, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);

	// Refresh the associated kismet window to display the length change
	RefreshAssociatedKismetWindow();
}

void WxInterpEd::MoveLoopMarker(FLOAT NewMarkerPos, UBOOL bIsStart)
{
	if(bIsStart)
	{
		IData->EdSectionStart = NewMarkerPos;
		IData->EdSectionEnd = ::Max(IData->EdSectionStart, IData->EdSectionEnd);				
	}
	else
	{
		IData->EdSectionEnd = NewMarkerPos;
		IData->EdSectionStart = ::Min(IData->EdSectionStart, IData->EdSectionEnd);
	}

	// Ensure loop points are inside sequence.
	IData->EdSectionStart = ::Clamp(IData->EdSectionStart, 0.f, IData->InterpLength);
	IData->EdSectionEnd = ::Clamp(IData->EdSectionEnd, 0.f, IData->InterpLength);

	CurveEd->SetRegionMarker(TRUE, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);
}

void WxInterpEd::BeginMoveSelectedKeys()
{
	InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("MoveSelectedKeys") );
	Opt->Modify();

	TArray<UInterpTrack*> ModifiedTracks;
	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(i);

		UInterpTrack* Track = SelKey.Track;
		check(Track);

		// If not already done so, call Modify on this track now.
		if( !ModifiedTracks.ContainsItem(Track) )
		{
			Track->Modify();
			ModifiedTracks.AddItem(Track);
		}

		SelKey.UnsnappedPosition = Track->GetKeyframeTime(SelKey.KeyIndex);
	}

	// When moving a key in time, turn off 'recording', so we dont end up assigning an objects location at one time to a key at another time.
	Opt->bAdjustingKeyframe = FALSE;
	Opt->bAdjustingGroupKeyframes = FALSE;
}

void WxInterpEd::EndMoveSelectedKeys()
{
	InterpEdTrans->EndSpecial();
}

void WxInterpEd::MoveSelectedKeys(FLOAT DeltaTime)
{
	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(i);

		UInterpTrack* Track = SelKey.Track;
		check(Track);

		SelKey.UnsnappedPosition += DeltaTime;
		FLOAT NewTime = SnapTime(SelKey.UnsnappedPosition, TRUE);

		// Do nothing if already at target time.
		if( Track->GetKeyframeTime(SelKey.KeyIndex) != NewTime )
		{
			INT OldKeyIndex = SelKey.KeyIndex;
			INT NewKeyIndex = Track->SetKeyframeTime(SelKey.KeyIndex, NewTime);
			SelKey.KeyIndex = NewKeyIndex;

			// There are two scenarios when the first key was changed when moving keys: 
			// (1) user moved the first key past the second key.
			// (2) user moved the a non-first key before the first key. 
			const UBOOL bFirstKeyChangedPositions = (OldKeyIndex != NewKeyIndex) && (0 == NewKeyIndex || 0 == OldKeyIndex);

			if( bFirstKeyChangedPositions )
			{
				// Determine if a movement track or a subtrack of a movement track was modified.
				UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>( SelKey.Track );
				if( !MoveTrack )
				{
					UInterpTrackMoveAxis* MoveSubTrack = Cast<UInterpTrackMoveAxis>( SelKey.Track );
					if( MoveSubTrack )
					{
						MoveTrack = CastChecked<UInterpTrackMove>( MoveSubTrack->GetOuter() );
					}
				}

				if( MoveTrack )
				{
					// We must update the initial transform for a RelativeToInitial move track if the first
					// key changed array positions because the track will be using the old transformation. 
					UpdateInitialTransformForMoveTrack( SelKey.Group, MoveTrack);
				}
			}

			// If the key changed index we need to search for any other selected keys on this track that may need their index adjusted because of this change.
			INT KeyMove = NewKeyIndex - OldKeyIndex;
			if(KeyMove > 0)
			{
				for(INT j=0; j<Opt->SelectedKeys.Num(); j++)
				{
					if( j == i ) // Don't look at one we just changed.
						continue;

					FInterpEdSelKey& TestKey = Opt->SelectedKeys(j);
					if( TestKey.Track == SelKey.Track && 
						TestKey.Group == SelKey.Group &&
						TestKey.KeyIndex > OldKeyIndex && 
						TestKey.KeyIndex <= NewKeyIndex)
					{
						TestKey.KeyIndex--;
					}
				}
			}
			else if(KeyMove < 0)
			{
				for(INT j=0; j<Opt->SelectedKeys.Num(); j++)
				{
					if( j == i )
						continue;

					FInterpEdSelKey& TestKey = Opt->SelectedKeys(j);
					if( TestKey.Track == SelKey.Track && 
						TestKey.Group == SelKey.Group &&
						TestKey.KeyIndex < OldKeyIndex && 
						TestKey.KeyIndex >= NewKeyIndex)
					{
						TestKey.KeyIndex++;
					}
				}
			}
		}

	} // FOR each selected key

	// Update positions at current time but with new keyframe times.
	RefreshInterpPosition();

	CurveEd->UpdateDisplay();
}


void WxInterpEd::BeginDrag3DHandle(UInterpGroup* Group, INT TrackIndex)
{
	if(TrackIndex < 0 || TrackIndex >= Group->InterpTracks.Num())
	{
		return;
	}

	UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>( Group->InterpTracks(TrackIndex) );
	if(MoveTrack)
	{
		InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("Drag3DTrajectoryHandle") );
		MoveTrack->Modify();
		bDragging3DHandle = TRUE;
	}
}

void WxInterpEd::Move3DHandle(UInterpGroup* Group, INT TrackIndex, INT KeyIndex, UBOOL bArriving, const FVector& Delta)
{
	if(!bDragging3DHandle)
	{
		return;
	}

	if(TrackIndex < 0 || TrackIndex >= Group->InterpTracks.Num())
	{
		return;
	}

	UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>( Group->InterpTracks(TrackIndex) );
	if(MoveTrack)
	{
		if(KeyIndex < 0 || KeyIndex >= MoveTrack->PosTrack.Points.Num())
		{
			return;
		}

		UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(Group);
		check(GrInst);
		check(GrInst->TrackInst.Num() == Group->InterpTracks.Num());
		UInterpTrackInstMove* MoveInst = CastChecked<UInterpTrackInstMove>( GrInst->TrackInst(TrackIndex) );

		FMatrix InvRefTM = MoveTrack->GetMoveRefFrame( MoveInst ).Inverse();
		FVector LocalDelta = InvRefTM.TransformNormal(Delta);

		BYTE InterpMode = MoveTrack->PosTrack.Points(KeyIndex).InterpMode;

		if(bArriving)
		{
			MoveTrack->PosTrack.Points(KeyIndex).ArriveTangent -= LocalDelta;

			// If keeping tangents smooth, update the LeaveTangent
			if(InterpMode != CIM_CurveBreak)
			{
				MoveTrack->PosTrack.Points(KeyIndex).LeaveTangent = MoveTrack->PosTrack.Points(KeyIndex).ArriveTangent;
			}
		}
		else
		{
			MoveTrack->PosTrack.Points(KeyIndex).LeaveTangent += LocalDelta;

			// If keeping tangents smooth, update the ArriveTangent
			if(InterpMode != CIM_CurveBreak)
			{
				MoveTrack->PosTrack.Points(KeyIndex).ArriveTangent = MoveTrack->PosTrack.Points(KeyIndex).LeaveTangent;
			}
		}

		// If adjusting an 'Auto' keypoint, switch it to 'User'
		if(InterpMode == CIM_CurveAuto || InterpMode == CIM_CurveAutoClamped)
		{
			MoveTrack->PosTrack.Points(KeyIndex).InterpMode = CIM_CurveUser;
			MoveTrack->EulerTrack.Points(KeyIndex).InterpMode = CIM_CurveUser;
		}

		// Update the curve editor to see curves change.
		CurveEd->UpdateDisplay();
	}
}

void WxInterpEd::EndDrag3DHandle()
{
	if(bDragging3DHandle)
	{
		InterpEdTrans->EndSpecial();
	}
}

void WxInterpEd::MoveInitialPosition(const FVector& Delta, const FRotator& DeltaRot)
{
	// If no movement track selected, do nothing. 
	if( !HasATrackSelected( UInterpTrackMove::StaticClass() ) )
	{
		return;
	}

	FRotationTranslationMatrix RotMatrix(DeltaRot, FVector(0));
	FTranslationMatrix TransMatrix(Delta);

	// Iterate only through selected movement tracks because those are the only relevant tracks. 
 	for( TTrackClassTypeIterator<UInterpTrackMove> MoveTrackIter(GetSelectedTrackIterator<UInterpTrackMove>()); MoveTrackIter; ++MoveTrackIter  )
	{
		// To move the initial position, we have to track down the interp 
		// track instance corresponding to the selected movement track. 

		UInterpGroupInst* GroupInst = Interp->FindFirstGroupInst( MoveTrackIter.GetGroup() );

		// Look for an instance of a movement track
		for( INT TrackInstIndex = 0; TrackInstIndex < GroupInst->TrackInst.Num(); TrackInstIndex++ )
		{
			UInterpTrackInstMove* MoveInst = Cast<UInterpTrackInstMove>(GroupInst->TrackInst(TrackInstIndex));

			if(MoveInst)
			{
				// Apply to reference frame of movement track.

				// Translate to origin and rotate, then apply translation.
				FVector InitialOrigin = MoveInst->InitialTM.GetOrigin();
				MoveInst->InitialTM.SetOrigin(FVector(0));
				MoveInst->InitialTM = MoveInst->InitialTM * RotMatrix;
				MoveInst->InitialTM.SetOrigin(InitialOrigin);
				MoveInst->InitialTM = MoveInst->InitialTM * TransMatrix;

				FMatrix ResetTM = FRotationTranslationMatrix(MoveInst->ResetRotation, MoveInst->ResetLocation);

				// Apply to reset information as well.

				FVector ResetOrigin = ResetTM.GetOrigin();
				ResetTM.SetOrigin(FVector(0));
				ResetTM = ResetTM * RotMatrix;
				ResetTM.SetOrigin(ResetOrigin);
				ResetTM = ResetTM * TransMatrix;

				MoveInst->ResetLocation = ResetTM.GetOrigin();
				MoveInst->ResetRotation = ResetTM.Rotator();
			}
		}
	}

	RefreshInterpPosition();

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();
}

// Small struct to help keep track of selected tracks
struct FSelectedTrackData
{
	UInterpTrack* Track;
	INT SelectedIndex;
};

/**
 * Adds a keyframe to the selected track.
 *
 * There must be one and only one track selected for a keyframe to be added.
 */
void WxInterpEd::AddKey()
{
	// To add keyframes easier, if a group is selected with only one 
	// track, select the track so the keyframe can be placed. 
	if( GetSelectedGroupCount() == 1 )
	{
		UInterpGroup* SelectedGroup = *GetSelectedGroupIterator();

		if( SelectedGroup->InterpTracks.Num() == 1 )
		{
			// Note: We shouldn't have to deselect currently 
			// selected tracks because a group is selected. 
			const UBOOL bDeselectPreviousTracks = FALSE;
			const INT FirstTrackIndex = 0;

			SelectTrack( SelectedGroup, SelectedGroup->InterpTracks(FirstTrackIndex), bDeselectPreviousTracks );
		}
	}

	if( !HasATrackSelected() )
	{
		appMsgf(AMT_OK,*LocalizeUnrealEd("NoActiveTrack"));
		return;
	}

	// Array of tracks that were selected
	TArray<FSelectedTrackData> TracksToAddKeys;

	if( GetSelectedTrackCount() > 1 )
	{
		// Populate the list of tracks that we need to add keys to.
		FSelectedTrackIterator TrackIt(GetSelectedTrackIterator());
		for( TrackIt; TrackIt; ++TrackIt )
		{
			// Only allow keys to be added to multiple tracks at once if they are subtracks of a movement track.
			if( (*TrackIt)->IsA( UInterpTrackMoveAxis::StaticClass() ) ) 
			{
				FSelectedTrackData Data;
				Data.Track = *TrackIt;
				Data.SelectedIndex = TrackIt.GetTrackIndex();
				TracksToAddKeys.AddItem( Data );
			}
			else
			{
				TracksToAddKeys.Empty();
				break;
			}
		}

		if( TracksToAddKeys.Num() == 0 )
		{
			appMsgf(AMT_OK,*LocalizeUnrealEd("InterpEd_Track_TooManySelected"));
		}
	}
	else
	{
		FSelectedTrackIterator TrackIt(GetSelectedTrackIterator());

		//  There is only one track selected.
		FSelectedTrackData Data;
		Data.Track = *TrackIt;
		Data.SelectedIndex = TrackIt.GetTrackIndex();
		TracksToAddKeys.AddItem( Data );
	}
	
	// A mapping of tracks to indices where keys were added
	TMap<UInterpTrack*, INT> TrackToNewKeyIndexMap;
	
	if( TracksToAddKeys.Num() > 0 )
	{
		// Add keys to all tracks in the array
		for( INT TrackIndex = 0; TrackIndex < TracksToAddKeys.Num(); ++TrackIndex )
		{
			UInterpTrack* Track = TracksToAddKeys( TrackIndex ).Track;
			INT SelectedTrackIndex = TracksToAddKeys( TrackIndex ).SelectedIndex;

			UInterpTrackInst* TrInst = NULL;
			UInterpGroup* Group = Cast<UInterpGroup>(Track->GetOuter());
			if( Group )
			{
				UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(Group);
				check(GrInst);

				TrInst = GrInst->TrackInst( SelectedTrackIndex );
				check(TrInst);
			}
			else
			{
				// The track is a subtrack, get the tracks group from its parent track.
				UInterpTrack* ParentTrack = CastChecked<UInterpTrack>( Track->GetOuter() );

				Group = CastChecked<UInterpGroup>( ParentTrack->GetOuter() );
				INT ParentTrackIndex = Group->InterpTracks.FindItemIndex( ParentTrack );

				UInterpGroupInst* GrInst = Interp->FindFirstGroupInst( Group );
				check(GrInst);

				TrInst = GrInst->TrackInst( ParentTrackIndex );
				check(TrInst);
			}

			UInterpTrackHelper* TrackHelper = NULL;
			UClass* Class = LoadObject<UClass>( NULL, *Track->GetEdHelperClassName(), NULL, LOAD_None, NULL );
			if ( Class != NULL )
			{
				TrackHelper = CastChecked<UInterpTrackHelper>(Class->GetDefaultObject());
			}

			FLOAT fKeyTime = SnapTime( Interp->Position, FALSE );
			if ( (TrackHelper == NULL) || !TrackHelper->PreCreateKeyframe(Track, fKeyTime) )
			{
				return;
			}

			InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("AddKey") );
			Track->Modify();
			Opt->Modify();

			if( Track->SubTracks.Num() > 0 )
			{
				// Add a keyframe to each subtrack.  We have to do this manually here because we need to know the indices where keyframes were added.
				for( INT SubTrackIndex = 0; SubTrackIndex < Track->SubTracks.Num(); ++SubTrackIndex )
				{
					UInterpTrack* SubTrack = Track->SubTracks( SubTrackIndex );
					SubTrack->Modify();
					// Add key at current time, snapped to the grid if its on.
					INT NewKeyIndex = Track->AddChildKeyframe( SubTrack, fKeyTime, TrInst, InitialInterpMode );
					if( NewKeyIndex != INDEX_NONE)
					{
						TrackToNewKeyIndexMap.Set( SubTrack, NewKeyIndex );

						const UBOOL bAddedKeyToBeginning = (0 == NewKeyIndex);

						if( bAddedKeyToBeginning && TrInst->IsA( UInterpTrackInstMove::StaticClass() ) )
						{
							// We must update the initial transform for a RelativeToInitial move track if the user adds a 
							// key before the first key because the track is using the old transformation at this point. 
							CastChecked<UInterpTrackInstMove>(TrInst)->CalcInitialTransform( Track, TRUE );
						}
					}
					else
					{
						appMsgf( AMT_OK, *LocalizeUnrealEd("NothingToKeyframe") );
						// Dont bother with other tracks
						break;
					}
				}
			}
			else
			{
				// Add key at current time, snapped to the grid if its on.
				INT NewKeyIndex = Track->AddKeyframe( fKeyTime, TrInst, InitialInterpMode );
				if( NewKeyIndex != INDEX_NONE)
				{
					TrackHelper->PostCreateKeyframe( Track, NewKeyIndex );

					const UBOOL bAddedKeyToBeginning = (0 == NewKeyIndex);

					if( bAddedKeyToBeginning && TrInst->IsA( UInterpTrackInstMove::StaticClass() ) )
					{
						// We must update the initial transform for a RelativeToInitial move track if the user adds a 
						// key before the first key because the track is using the old transformation at this point. 
						CastChecked<UInterpTrackInstMove>(TrInst)->CalcInitialTransform( Track->IsA(UInterpTrackMoveAxis::StaticClass()) ? CastChecked<UInterpTrackMove>(Track->GetOuter()) : Track, TRUE );
					}
				}

				// If we failed to add the keyframe - bail out now.
				if(NewKeyIndex == INDEX_NONE)
				{
					appMsgf( AMT_OK, *LocalizeUnrealEd("NothingToKeyframe") );
				}
				else
				{
					TrackToNewKeyIndexMap.Set( Track, NewKeyIndex );
				}
			}

			InterpEdTrans->EndSpecial();

		}


		if( TrackToNewKeyIndexMap.Num() > 0 )
		{
			// Select the newly added keyframes.
			ClearKeySelection();;

			for( TMap<UInterpTrack*, INT>::TIterator It(TrackToNewKeyIndexMap); It; ++It)
			{
				UInterpTrack* Track = It.Key();
				INT NewKeyIndex = It.Value();

				AddKeyToSelection(Track->GetOwningGroup(), Track, NewKeyIndex, TRUE); // Probably don't need to auto-wind - should already be there!
			}

			// Update to current time, in case new key affects state of scene.
			RefreshInterpPosition();

		}
		
		// Dirty the track window viewports
		InvalidateTrackWindowViewports();
	}

}

/** 
 * Call utility to split an animation in the selected AnimControl track. 
 *
 * Only one interp track can be selected and it must be a anim control track for the function.
 */
void WxInterpEd::SplitAnimKey()
{
	// Only one track can be selected at a time when dealing with keyframes.
	// Also, there must be an anim control track selected.
	if( GetSelectedTrackCount() != 1 || !HasATrackSelected(UInterpTrackAnimControl::StaticClass()) )
	{
		return;
	}

	// Split keys only for anim tracks
	TTrackClassTypeIterator<UInterpTrackAnimControl> AnimTrackIt(GetSelectedTrackIterator<UInterpTrackAnimControl>());
	UInterpTrackAnimControl* AnimTrack = *AnimTrackIt;

	// Call split utility.
	INT NewKeyIndex = AnimTrack->SplitKeyAtPosition(Interp->Position);

	// If we created a new key - select it by default.
	if(NewKeyIndex != INDEX_NONE)
	{
		ClearKeySelection();
		AddKeyToSelection(AnimTrackIt.GetGroup(), AnimTrack, NewKeyIndex, FALSE);
	}
}

/**
 * Copies the currently selected track.
 *
 * @param bCut	Whether or not we should cut instead of simply copying the track.
 */
void WxInterpEd::CopySelectedGroupOrTrack(UBOOL bCut)
{
	const UBOOL bHasAGroupSelected = HasAGroupSelected();
	const UBOOL bHasATrackSelected = HasATrackSelected();

	if( !bHasAGroupSelected && !bHasATrackSelected )
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("InterpEd_Copy_NeedToSelectGroup"));
		return;
	}

	// Sanity check. There should only be only tracks 
	// selected or only groups selected. Not both!
	check( bHasAGroupSelected ^ bHasATrackSelected );

	// Make sure to clear the buffer before adding to it again. 
	GUnrealEd->MatineeCopyPasteBuffer.Empty();

	// If no tracks are selected, copy the group.
	if( bHasAGroupSelected )
	{
		// Add all the selected groups to the copy-paste buffer
		for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
		{
			UObject* CopiedObject = (UObject*)UObject::StaticDuplicateObject(*GroupIt, *GroupIt, 
			UObject::GetTransientPackage(), NULL);
			 GUnrealEd->MatineeCopyPasteBuffer.AddItem(CopiedObject);
		}

		// Delete the active group if we are doing a cut operation.
		if(bCut)
		{
			InterpEdTrans->BeginSpecial(  *LocalizeUnrealEd(TEXT("InterpEd_Cut_SelectedTrackOrGroup")) );
			{
				DeleteSelectedGroups();
			}
			InterpEdTrans->EndSpecial();
		}
	}
	else
	{
		TArray<UInterpTrack*> DuplicatedTracks;

		for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
		{
			UInterpTrack* Track = *TrackIt;
			// Only allow base tracks to be copied.  Subtracks should never be copied because this could result in subtracks being pasted where they dont belong (like directly in groups).
			if( Track->GetOuter()->IsA( UInterpGroup::StaticClass() ) )
			{
				UObject* CopiedObject = (UObject*)UObject::StaticDuplicateObject(Track, Track, 
					UObject::GetTransientPackage(), NULL);
				GUnrealEd->MatineeCopyPasteBuffer.AddItem(CopiedObject);
				DuplicatedTracks.AddItem( Track );
			}
		}

		// Delete the originating track if we are cutting.
		if( bCut && DuplicatedTracks.Num() > 0 )
		{
			// Deselect All tracks and only select the tracks that were valid to copy
			DeselectAllTracks();

			for( INT TrackIndex = 0; TrackIndex < DuplicatedTracks.Num(); ++TrackIndex )
			{
				UInterpTrack* Track = DuplicatedTracks( TrackIndex );
				SelectTrack( Track->GetOwningGroup(), Track, FALSE );
			}

			InterpEdTrans->BeginSpecial(  *LocalizeUnrealEd(TEXT("InterpEd_Cut_SelectedTrackOrGroup")) );
			DeleteSelectedTracks();
			InterpEdTrans->EndSpecial();
		}
	}
}

/**
 * Pastes the previously copied track.
 */
void WxInterpEd::PasteSelectedGroupOrTrack()
{
	// See if we are pasting a track or group.
	if(GUnrealEd->MatineeCopyPasteBuffer.Num())
	{
		// Variables only used when pasting tracks.
		UInterpGroup* GroupToPasteTracks = NULL;
		TArray<UInterpTrack*> TracksToSelect;

		for( TArray<UObject*>::TIterator BufferIt(GUnrealEd->MatineeCopyPasteBuffer); BufferIt; ++BufferIt )
		{
			UObject* CurrentObject = *BufferIt;

			if( CurrentObject->IsA(UInterpGroup::StaticClass()) )
			{
				DuplicateGroup(CastChecked<UInterpGroup>(CurrentObject));
			}
			else if( CurrentObject->IsA(UInterpTrack::StaticClass()) )
			{	
				INT GroupsSelectedCount = GetSelectedGroupCount();

				if( GroupsSelectedCount < 1 )
				{
					appMsgf(AMT_OK, *LocalizeUnrealEd("InterpEd_Paste_NeedToSelectGroup"));
				}
				else if( GroupsSelectedCount > 1 )
				{
					appMsgf(AMT_OK, *LocalizeUnrealEd("InterpEd_Paste_OneGroup"));
				}
				else
				{
					UInterpTrack* TrackToPaste = CastChecked<UInterpTrack>(CurrentObject);

					InterpEdTrans->BeginSpecial(  *LocalizeUnrealEd(TEXT("InterpEd_Paste_SelectedTrackOrGroup")) );
					{
						if( NULL == GroupToPasteTracks )
						{
							GroupToPasteTracks = *GetSelectedGroupIterator();
						}

						GroupToPasteTracks->Modify();

						// Defer selection of the pasted track so the group is not deselected, 
						// which would cause all other tracks to fail when pasting. 
						const UBOOL bSelectTrack = FALSE;
						UInterpTrack* PastedTrack = AddTrackToSelectedGroup(TrackToPaste->GetClass(), TrackToPaste, bSelectTrack);

						// Save off the created track so we can select it later. 
						if( NULL != PastedTrack )
						{
							TracksToSelect.AddItem(PastedTrack);
						}
					}
					InterpEdTrans->EndSpecial();
				}
			}
		}

		// If we pasted tracks to a group, then we still need to select them.
		if( NULL != GroupToPasteTracks )
		{
			// Don't deselect previous tracks because (1) if a group was selected, then no other tracks 
			// were selected and (2) we don't want to deselect tracks we just selected in the loop.
			const UBOOL bDeselectPreviousTracks = FALSE;
			for( INT TrackIndex = 0; TrackIndex < TracksToSelect.Num(); ++TrackIndex )
			{
				SelectTrack(GroupToPasteTracks, TracksToSelect(TrackIndex), bDeselectPreviousTracks);
			}
		}
	}
}

/**
 * @return Whether or not we can paste a track/group.
 */
UBOOL WxInterpEd::CanPasteGroupOrTrack()
{
	UBOOL bResult = FALSE;

	// Make sure we at least have something in the buffer.
	if( GUnrealEd->MatineeCopyPasteBuffer.Num() )
	{
		// We don't currently support pasting on multiple groups or tracks.
		// So, we have have to make sure we have one group or one track selected.
		const UBOOL bCanPasteOnGroup = (GetSelectedGroupCount() < 2);
		const UBOOL bCanPasteOnTrack = (GetSelectedTrackCount() == 1);

		// Copy-paste can only happen if only one group OR only one track is selected.
		// We cannot paste if there is one track and one group selected.
		if( bCanPasteOnGroup ^ bCanPasteOnTrack )
		{
			bResult = TRUE;

			// Can we paste on top of a group?
			if( bCanPasteOnGroup )
			{
				for( TArray<UObject*>::TIterator BufferIt(GUnrealEd->MatineeCopyPasteBuffer); BufferIt; ++BufferIt )
				{
					const UBOOL bIsAGroup = (*BufferIt)->IsA(UInterpGroup::StaticClass());
					const UBOOL bIsATrack = (*BufferIt)->IsA(UInterpTrack::StaticClass());

					// We can paste groups or tracks on top of selected groups. If there 
					// is one object in the buffer that isn't either, then we can't paste.
					if( !bIsAGroup && !bIsATrack )
					{
						bResult = FALSE;
						break;
					}
				}
			}
			// Can we paste on top of a track?
			else
			{
				for( TArray<UObject*>::TIterator BufferIt(GUnrealEd->MatineeCopyPasteBuffer); BufferIt; ++BufferIt )
				{
					// We can only paste tracks on top of tracks. If there exists any other 
					// objects in the buffer that aren't tracks, then we can't paste.
					if( !(*BufferIt)->IsA(UInterpTrack::StaticClass()) )
					{
						bResult = FALSE;
						break;
					}
				}
			}
		}
	}

	return bResult;
}




/**
 * Adds a new track to the specified group.
 *
 * @param Group The group to add a track to
 * @param TrackClass The class of track object we are going to add.
 * @param TrackToCopy A optional track to copy instead of instantiating a new one.
 * @param bAllowPrompts TRUE if we should allow a dialog to be summoned to ask for initial information
 * @param OutNewTrackIndex [Out] The index of the newly created track in its parent group
 * @param bSelectTrack TRUE if we should select the track after adding it
 *
 * @return Returns newly created track (or NULL if failed)
 */
UInterpTrack* WxInterpEd::AddTrackToGroup( UInterpGroup* Group, UClass* TrackClass, UInterpTrack* TrackToCopy, UBOOL bAllowPrompts, INT& OutNewTrackIndex, UBOOL bSelectTrack /*= TRUE*/ )
{
	OutNewTrackIndex = INDEX_NONE;

	if( Group == NULL )
	{
		return NULL;
	}

	UInterpGroupInst* GrInst = Interp->FindFirstGroupInst( Group );
	check(GrInst);

	UInterpTrack* TrackDef = TrackClass->GetDefaultObject<UInterpTrack>();

	UInterpTrackHelper* TrackHelper = NULL;
	UBOOL bCopyingTrack = (TrackToCopy!=NULL);
	UClass	*Class = LoadObject<UClass>( NULL, *TrackDef->GetEdHelperClassName(), NULL, LOAD_None, NULL );
	if ( Class != NULL )
	{
		TrackHelper = CastChecked<UInterpTrackHelper>(Class->GetDefaultObject());
	}

	if ( (TrackHelper == NULL) || !TrackHelper->PreCreateTrack( Group, TrackDef, bCopyingTrack, bAllowPrompts ) )
	{
		return NULL;
	}

	Group->Modify();

	// Construct track and track instance objects.
	UInterpTrack* NewTrack = NULL;
	if(TrackToCopy)
	{
		NewTrack = Cast<UInterpTrack>(UObject::StaticDuplicateObject(TrackToCopy, TrackToCopy, Group, NULL ));
	}
	else
	{
		NewTrack = ConstructObject<UInterpTrack>( TrackClass, Group, NAME_None, RF_Transactional );
	}

	check(NewTrack);

	OutNewTrackIndex = Group->InterpTracks.AddItem(NewTrack);

	check( NewTrack->TrackInstClass );
	check( NewTrack->TrackInstClass->IsChildOf(UInterpTrackInst::StaticClass()) );

	TrackHelper->PostCreateTrack( NewTrack, bCopyingTrack, OutNewTrackIndex );

	if(bCopyingTrack == FALSE)
	{
		NewTrack->SetTrackToSensibleDefault();
	}

	NewTrack->Modify();

	// We need to create a InterpTrackInst in each instance of the active group (the one we are adding the track to).
	for(INT i=0; i<Interp->GroupInst.Num(); i++)
	{
		UInterpGroupInst* GrInst = Interp->GroupInst(i);
		if(GrInst->Group == Group)
		{
			GrInst->Modify();

			UInterpTrackInst* NewTrackInst = ConstructObject<UInterpTrackInst>( NewTrack->TrackInstClass, GrInst, NAME_None, RF_Transactional );

			const INT NewInstIndex = GrInst->TrackInst.AddItem(NewTrackInst);
			check(NewInstIndex == OutNewTrackIndex);

			// Initialize track, giving selected object.
			NewTrackInst->InitTrackInst(NewTrack);

			// Save state into new track before doing anything else (because we didn't do it on ed mode change).
			NewTrackInst->SaveActorState(NewTrack);
			NewTrackInst->Modify();
		}
	}

	if(bCopyingTrack == FALSE)
	{
		// Bit of a hack here, but useful. Whenever you put down a movement track, add a key straight away at the start.
		// Should be ok to add at the start, because it should not be having its location (or relative location) changed in any way already as we scrub.
		UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(NewTrack);
		if(MoveTrack)
		{
			UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(Group);
			UInterpTrackInst* TrInst = GrInst->TrackInst(OutNewTrackIndex);
			MoveTrack->AddKeyframe( 0.0f, TrInst, InitialInterpMode );
		}

		if ( Group->IsA(UInterpGroupAI::StaticClass()) )
		{
			// same kind of hack here since I need group information to do this
			// I can't do it in PostCreateTrack which could have been perfect
			// also I couldn't do it in InittrackInst because this has to be only done once when created
			UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(NewTrack);
			if(AnimTrack)
			{
				// default slot name if not exists
				if ( AnimTrack->SlotName == NAME_None )
				{
					AnimTrack->SlotName = FName(*GConfig->GetStr(TEXT("MatineePreview"), TEXT("DefaultAnimSlotName"), GEditorIni));
				}

				// add default anim curve weights to be 1
				INT KeyIndex = AnimTrack->CreateNewKey(0.0f);
				AnimTrack->SetKeyOut(0, KeyIndex, 1.0f);
			}
		}
	}

	if ( bSelectTrack )
	{
		SelectTrack( Group, NewTrack );
	}
	return NewTrack;
}

/**
 * Adds a new track to the selected group.
 *
 * @param TrackClass		The class of the track we are adding.
 * @param TrackToCopy		A optional track to copy instead of instantiating a new one.  If NULL, a new track will be instantiated.
 * @param bSelectTrack		If TRUE, select the track after adding it
 *
 * @return	The newly-created track if created; NULL, otherwise. 
 */
UInterpTrack* WxInterpEd::AddTrackToSelectedGroup(UClass* TrackClass, UInterpTrack* TrackToCopy, UBOOL bSelectTrack /* = TRUE */)
{
	// In order to add a track to a group, there can only be one group selected.
	check( GetSelectedGroupCount() == 1 );
	UInterpGroup* Group = *GetSelectedGroupIterator();

	return AddTrackToGroupAndRefresh(Group, *LocalizeUnrealEd("NewTrack"), TrackClass, TrackToCopy, bSelectTrack);
}

/**
 * Adds a new track to a group and appropriately updates/refreshes the editor
 *
 * @param Group				The group to add this track to
 * @param NewTrackName		The default name of the new track to add
 * @param TrackClass		The class of the track we are adding.
 * @param TrackToCopy		A optional track to copy instead of instantiating a new one.  If NULL, a new track will be instantiated.
 * @param bSelectTrack		If TRUE, select the track after adding it
 * return					New interp track that was created
 */
UInterpTrack* WxInterpEd::AddTrackToGroupAndRefresh(UInterpGroup* Group, const FString& NewTrackName, UClass* TrackClass, UInterpTrack* TrackToCopy, UBOOL bSelectTrack /* = TRUE */)
{
	UInterpTrack* TrackDef = TrackClass->GetDefaultObject<UInterpTrack>();

	// If bOnePerGrouop - check we don't already have a track of this type in the group.
	if(TrackDef->bOnePerGroup)
	{
		DisableTracksOfClass(Group, TrackClass);
	}

	// Warn when creating dynamic track on a static actor, warn and offer to bail out.
	if(TrackDef->AllowStaticActors()==FALSE)
	{
		UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(Group);
		check(GrInst);

		AActor* GrActor = GrInst->GetGroupActor();
		if(GrActor && GrActor->IsStatic())
		{
			const UBOOL bConfirm = appMsgf(AMT_YesNo, *LocalizeUnrealEd("WarnNewMoveTrackOnStatic"));
			if(!bConfirm)
			{
				return NULL;
			}
		}
	}

	InterpEdTrans->BeginSpecial( *NewTrackName );

	// Add the track!
	INT NewTrackIndex = INDEX_NONE;
	UInterpTrack* ReturnTrack = AddTrackToGroup( Group, TrackClass, TrackToCopy, TRUE, NewTrackIndex, bSelectTrack );
	if (ReturnTrack)
	{
		ReturnTrack->EnableTrack( TRUE );
		// Now ensure all Matinee actions (including 'Interp') are updated with a new connector to match the data.
		FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
		if ( mode != NULL && mode->InterpEd != NULL )
		{
			mode->InterpEd->UpdateMatineeActionConnectors( );	
		}
	}

	InterpEdTrans->EndSpecial();


	if( NewTrackIndex != INDEX_NONE )
	{
		// Make sure particle replay tracks have up-to-date editor-only transient state
		UpdateParticleReplayTracks();

		// A new track may have been added, so we'll need to update the scroll bar
		UpdateTrackWindowScrollBars();

		// Update graphics to show new track!
		InvalidateTrackWindowViewports();

		// If we added a movement track to this group, we'll need to make sure that the actor's transformations are captured
		// so that we can restore them later.
		Interp->RecaptureActorState();
	}

	return ReturnTrack;
}

/** 
 * Deletes the currently active track. 
 */
void WxInterpEd::DeleteSelectedTracks()
{
	// This function should only be called if there is at least one selected track.
	check( HasATrackSelected() );

	InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("TrackDelete") );
	Interp->Modify();
	IData->Modify();

	// Deselect everything.
	ClearKeySelection();

	for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		UInterpTrack* ActiveTrack = *TrackIt;

		// Only allow base tracks to be deleted,  Subtracks will be deleted by their parent
		if( ActiveTrack->GetOuter()->IsA( UInterpGroup::StaticClass() ) )
		{
			UInterpGroup* Group = TrackIt.GetGroup();
			Group->Modify();
			ActiveTrack->Modify();

			const INT TrackIndex = TrackIt.GetTrackIndex();

			for(INT i=0; i<Interp->GroupInst.Num(); i++)
			{
				UInterpGroupInst* GrInst = Interp->GroupInst(i); 
				if( GrInst->Group == Group )
				{
					UInterpTrack* Track = GrInst->Group->InterpTracks(TrackIndex);
					UInterpTrackInst* TrInst = GrInst->TrackInst(TrackIndex);

					GrInst->Modify();
					TrInst->Modify();

					// Before deleting this track - find each instance of it and restore state.
					TrInst->RestoreActorState( Track );

					// Clean up the track instance
					TrInst->TermTrackInst( Track );

					// Disable any post processing effects that were happening
					AActor* GroupActor = GrInst->GetGroupActor();
					if(GroupActor)
					{
						Track->DisableCameraPostProcessFlags( GroupActor );
					}

					GrInst->TrackInst.Remove(TrackIndex);
				}
			}

			// Remove from the Curve editor, if its there.
			IData->CurveEdSetup->RemoveCurve(ActiveTrack);
			// Remove any subtrack curves if the parent is being removed
			for( INT SubTrackIndex = 0; SubTrackIndex < ActiveTrack->SubTracks.Num(); ++SubTrackIndex )
			{
				IData->CurveEdSetup->RemoveCurve( ActiveTrack->SubTracks( SubTrackIndex ) );
			}

			// If this is an Event track, update the event outputs of any actions using this MatineeData.
			if(ActiveTrack->IsA(UInterpTrackEvent::StaticClass()))
			{
				UpdateMatineeActionConnectors();
			}

			// Finally, remove the track completely. 
			// WARNING: Do not deference or use this iterator after the call to RemoveCurrent()!
			TrackIt.RemoveCurrent();

			// NOW Select the group
			SelectGroup( Group );
		}
	}

	InterpEdTrans->EndSpecial();

	// Also remove the variable connector that corresponded to this group in all SeqAct_Interps in the level.
	UpdateMatineeActionConnectors();

	// Update the curve editor
	CurveEd->CurveChanged();

	// A track may have been deleted, so we'll need to update our track window scroll bar
	UpdateTrackWindowScrollBars();

	// Update the property window to reflect the change in selection.
	UpdatePropertyWindow();

	Interp->RecaptureActorState();
}

/** 
 * Deletes all selected groups.
 */
void WxInterpEd::DeleteSelectedGroups()
{
	// There must be one group selected to use this funciton.
	check( HasAGroupSelected() );

	InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("GroupDelete") );
	Interp->Modify();
	IData->Modify();

	// Deselect everything.
	ClearKeySelection();

	for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		UInterpGroup* GroupToDelete = *GroupIt;

		// Mark InterpGroup and all InterpTracks as Modified.
		GroupToDelete->Modify();
		for(INT j=0; j<GroupToDelete->InterpTracks.Num(); j++)
		{
			GroupToDelete->InterpTracks(j)->Modify();

			// Remove from the Curve editor, if its there.
			IData->CurveEdSetup->RemoveCurve( GroupToDelete->InterpTracks(j) );
		}


		UBOOL bAIGroup =GroupToDelete->IsA(UInterpGroupAI::StaticClass());
		// First, destroy any instances of this group.
		INT i=0;
		while( i<Interp->GroupInst.Num() )
		{
			UInterpGroupInst* GrInst = Interp->GroupInst(i);
			if(GrInst->Group == GroupToDelete)
			{
				if (bAIGroup)
				{
					UInterpGroupInstAI * AIGroupInst = CastChecked<UInterpGroupInstAI>(GrInst);
					if (AIGroupInst->PreviewPawn!=NULL)
					{
						GWorld->DestroyActor(AIGroupInst->PreviewPawn);
					}
				}
				// Mark InterpGroupInst and all InterpTrackInsts as Modified.
				GrInst->Modify();
				for(INT j=0; j<GrInst->TrackInst.Num(); j++)
				{
					GrInst->TrackInst(j)->Modify();
				}

				// Restore all state in this group before exiting
				GrInst->RestoreGroupActorState();

				// Clean up GroupInst
				GrInst->TermGroupInst(FALSE);
				// Don't actually delete the TrackInsts - but we do want to call TermTrackInst on them.

				// Remove from SeqAct_Interps list of GroupInsts
				Interp->GroupInst.Remove(i);
			}
			else
			{
				i++;
			}
		}

		// We're being deleted, so we need to unparent any child groups
		// @todo: Should we support optionally deleting all sub-groups when deleting the parent?
		for( INT CurGroupIndex = IData->InterpGroups.FindItemIndex( GroupToDelete ) + 1; CurGroupIndex < IData->InterpGroups.Num(); ++CurGroupIndex )
		{
			UInterpGroup* CurGroup = IData->InterpGroups( CurGroupIndex );
			if( CurGroup->bIsParented )
			{
				CurGroup->Modify();

				// Unparent this child
				CurGroup->bIsParented = FALSE;
			}
			else
			{
				// We've reached a root object, so we're done processing children.  Bail!
				break;
			}
		}

		// Prevent group from being selected as well as any tracks associated to the group.
		// WARNING: Do not deference or use this iterator after the call to RemoveCurrent()!
		GroupIt.RemoveCurrent();
	}

	// Tell curve editor stuff might have changed.
	CurveEd->CurveChanged();

	// Make sure the director track window is only visible if we have a director group!
	UpdateDirectorTrackWindowVisibility();

	// A group may have been deleted, so we'll need to update our track window scroll bar
	UpdateTrackWindowScrollBars();

	// Deselect everything.
	ClearKeySelection();

	// Also remove the variable connector that corresponded to this group in all SeqAct_Interps in the level.
	UpdateMatineeActionConnectors();	

	InterpEdTrans->EndSpecial();

	// Stop having the camera locked to this group if it currently is.
	if( CamViewGroup && IsGroupSelected(CamViewGroup) )
	{
		LockCamToGroup(NULL);
	}

	// Update the property window to reflect the change in selection.
	UpdatePropertyWindow();

	// Reimage actor world locations.  This must happen after the group was removed.
	Interp->RecaptureActorState();
}

/**
 * Disables all tracks of a class type in this group
 * @param Group - group in which to disable tracks of TrackClass type
 * @param TrackClass - Type of track to disable
 */
void WxInterpEd::DisableTracksOfClass(UInterpGroup* Group, UClass* TrackClass)
{
	for( INT TrackIndex = 0; TrackIndex < Group->InterpTracks.Num(); TrackIndex++ )
	{
		if( Group->InterpTracks(TrackIndex)->GetClass() == TrackClass )
		{
			Group->InterpTracks(TrackIndex)->EnableTrack( FALSE );
		}
	}
}

/**
 * Selects the group actor associated to the given interp group. 
 *
 * @param	AssociatedGroup	The group corresponding to the referenced actor. 
 */
void WxInterpEd::SelectGroupActor( UInterpGroup* AssociatedGroup )
{
	UInterpGroupInst* GroupInstance = Interp->FindFirstGroupInst(AssociatedGroup);

	if( GroupInstance != NULL )
	{
		check( GroupInstance->TrackInst.Num() == AssociatedGroup->InterpTracks.Num() );

		AActor* Actor = GroupInstance->GetGroupActor();

		if( Actor && !Actor->IsSelected() )
		{
			bIgnoreActorSelection = TRUE;
			GUnrealEd->SelectActor( Actor, TRUE, NULL, TRUE );
			bIgnoreActorSelection = FALSE;
		}
	}
}

/**
 * Deselects the group actor associated to the given interp group. 
 *
 * @param	AssociatedGroup	The group corresponding to the referenced actor to deselect. 
 */
void WxInterpEd::DeselectGroupActor( UInterpGroup* AssociatedGroup )
{
	UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(AssociatedGroup);

	if( GrInst != NULL )
	{
		check( GrInst->TrackInst.Num() == AssociatedGroup->InterpTracks.Num() );

		AActor* Actor = GrInst->GroupActor;

		if( Actor && Actor->IsSelected() )
		{
			GUnrealEd->SelectActor( Actor, FALSE, NULL, TRUE );
		}
	}
}

/**
 * Duplicates the specified group
 *
 * @param GroupToDuplicate		Group we are going to duplicate.
 */
void WxCameraAnimEd::DuplicateGroup(UInterpGroup* GroupToDuplicate)
{
	// Can't duplicate a group into a camera anim, just throw error message and ignore
	appMsgf(AMT_OK, *LocalizeUnrealEd("UnableToPasteGroupIntoCameraAnim"));
}


/**
 * Duplicates the specified group
 *
 * @param GroupToDuplicate		Group we are going to duplicate.
 */
void WxInterpEd::DuplicateGroup(UInterpGroup* GroupToDuplicate)
{
	if(GroupToDuplicate==NULL)
	{
		return;
	}

	FName NewGroupName;

	// See if we are duplicating a director group.
	UBOOL bDirGroup = GroupToDuplicate->IsA(UInterpGroupDirector::StaticClass());

	// If we are a director group, make sure we don't have a director group yet in our interp data.
	if(bDirGroup)
	{
		 UInterpGroupDirector* DirGroup = IData->FindDirectorGroup();

		 if(DirGroup)
		 {
			appMsgf(AMT_OK, *LocalizeUnrealEd("UnableToPasteOnlyOneDirectorGroup"));
			return;
		 }
	}
	else
	{
		FString GroupName = GroupToDuplicate->GroupName.ToString();

		FString DialogTitle = GroupToDuplicate->bIsFolder ? TEXT("NewFolderName") : TEXT("NewGroupName");

		// Otherwise, pop up dialog to enter name.
		WxDlgGenericStringEntry dlg;
		const INT Result = dlg.ShowModal( *DialogTitle, *DialogTitle, *GroupName );
		if( Result != wxID_OK )
		{
			return;
		}

		FString TempString = dlg.GetEnteredString();
		TempString = TempString.Replace(TEXT(" "),TEXT("_"));
		NewGroupName = FName( *TempString );
	}

	

	// Begin undo transaction
	InterpEdTrans->BeginSpecial( TEXT("NewGroup") );
	{
		Interp->Modify();
		IData->Modify();

		// Create new InterpGroup.
		UInterpGroup* NewGroup = NULL;

		NewGroup = (UInterpGroup*)UObject::StaticDuplicateObject( GroupToDuplicate, GroupToDuplicate, IData, TEXT("None"), RF_Transactional );

		if(!bDirGroup)
		{
			NewGroup->GroupName = NewGroupName;
		}
		IData->InterpGroups.AddItem(NewGroup);


		// All groups must have a unique name.
		NewGroup->EnsureUniqueName();

		// Randomly generate a group colour for the new group.
		NewGroup->GroupColor = FColor::MakeRandomColor();
		NewGroup->Modify();

		// Pasted groups are always unparented.  If we wanted to support pasting a group into a folder, we'd
		// need to be sure to insert the new group in the appropriate place in the group list.
		NewGroup->bIsParented = FALSE;

		// Create new InterpGroupInst
		UInterpGroupInst* NewGroupInst = NULL;

		if(bDirGroup)
		{
			NewGroupInst = ConstructObject<UInterpGroupInstDirector>( UInterpGroupInstDirector::StaticClass(), Interp, NAME_None, RF_Transactional );
		}
		else
		{
			NewGroupInst = ConstructObject<UInterpGroupInst>( UInterpGroupInst::StaticClass(), Interp, NAME_None, RF_Transactional );
		}

		// Initialise group instance, saving ref to actor it works on.
		NewGroupInst->InitGroupInst(NewGroup, NULL);

		const INT NewGroupInstIndex = Interp->GroupInst.AddItem(NewGroupInst);

		NewGroupInst->Modify();


		// If a director group, create a director track for it now.
		if(bDirGroup)
		{
			UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(NewGroup);
			check(DirGroup);

			// See if the director group has a director track yet, if not make one and make the corresponding track inst as well.
			UInterpTrackDirector* NewDirTrack = DirGroup->GetDirectorTrack();
			
			if(NewDirTrack==NULL)
			{
				NewDirTrack = ConstructObject<UInterpTrackDirector>( UInterpTrackDirector::StaticClass(), NewGroup, NAME_None, RF_Transactional );
				NewGroup->InterpTracks.AddItem(NewDirTrack);

				UInterpTrackInst* NewDirTrackInst = ConstructObject<UInterpTrackInstDirector>( UInterpTrackInstDirector::StaticClass(), NewGroupInst, NAME_None, RF_Transactional );
				NewGroupInst->TrackInst.AddItem(NewDirTrackInst);

				NewDirTrackInst->InitTrackInst(NewDirTrack);
				NewDirTrackInst->SaveActorState(NewDirTrack);

				// Save for undo then redo.
				NewDirTrackInst->Modify();
				NewDirTrack->Modify();
			}
		}
		else if( !NewGroup->bIsFolder )
		{	
			// Create a new variable connector on all Matinee's using this data.
			UpdateMatineeActionConnectors();

			// Find the newly created connector on this SeqAct_Interp. Should always have one now!
			const INT NewLinkIndex = Interp->FindConnectorIndex(NewGroup->GroupName.ToString(), LOC_VARIABLE );
			check(NewLinkIndex != INDEX_NONE);
			FSeqVarLink* NewLink = &(Interp->VariableLinks(NewLinkIndex));

			// Find the sequence that this SeqAct_Interp is in.
			USequence* Seq = Cast<USequence>( Interp->GetOuter() );
			check(Seq);
		}

		// Select the group we just duplicated
		SelectGroup(NewGroup);
	}
	InterpEdTrans->EndSpecial();

	// Make sure the director track window is only visible if we have a director group!
	UpdateDirectorTrackWindowVisibility();

	// A new group may have been added (via duplication), so we'll need to update our scroll bar
	UpdateTrackWindowScrollBars();

	// Update graphics to show new group.
	InvalidateTrackWindowViewports();

	// If adding a camera- make sure its frustum colour is updated.
	UpdateCamColours();

	// Reimage actor world locations.  This must happen after the group was created.
	Interp->RecaptureActorState();
}

/**
 * Duplicates selected tracks in their respective groups and clears them to begin real time recording, and selects them 
 * @param bInDeleteSelectedTracks - TRUE if the currently selected tracks should be destroyed when recording begins
 */

void WxInterpEd::DuplicateSelectedTracksForRecording (const UBOOL bInDeleteSelectedTracks)
{
	TArray <UInterpGroup*> OwnerGroups;
	TArray <INT> RecordTrackIndex;
	TArray <UInterpTrack*> OldSelectedTracks;
	for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		UInterpTrack* TrackToCopy = *TrackIt;
		check(TrackToCopy);
		//make sure we support this type of track for duplication
		if ((Cast<UInterpTrackMove>(TrackToCopy)==NULL) && (Cast<UInterpTrackFloatProp>(TrackToCopy)==NULL))
		{
			//not supporting this track type for now
			continue;
		}
		
		OldSelectedTracks.AddItem(TrackToCopy);

		UInterpGroup* OwnerGroup = TrackIt.GetGroup();

		FString NewTrackName = FString::Printf( LocalizeSecure(LocalizeUnrealEd("CaptureTrack"), *TrackToCopy->GetEdHelperClassName()) );
		UInterpTrack* NewTrack = AddTrackToGroupAndRefresh(OwnerGroup, NewTrackName, TrackToCopy->GetClass(), TrackToCopy, FALSE);
		if (NewTrack)
		{
			RecordingTracks.AddItem(NewTrack);
			OwnerGroups.AddItem(OwnerGroup);
			RecordTrackIndex.AddItem(OwnerGroup->InterpTracks.FindItemIndex(NewTrack));
			
			//guard around movement tracks being relative
			INT FinalIndex = 0;
			UInterpTrackMove* MovementTrack = Cast<UInterpTrackMove>(NewTrack);
			if(MovementTrack && (MovementTrack->MoveFrame == IMF_RelativeToInitial))
			{
				FinalIndex = 1;
			}

			//remove all keys
			for (INT KeyFrameIndex = NewTrack->GetNumKeyframes()-1; KeyFrameIndex >= FinalIndex; --KeyFrameIndex)
			{
				NewTrack->RemoveKeyframe(KeyFrameIndex);
			}

			// remove all subtrack keys.  We cant do this inside the parent tracks remove keyframe as the keyframe index does not
			// necessarily represent a valid index in a subtrack.
			for( INT SubTrackIndex = 0; SubTrackIndex < NewTrack->SubTracks.Num(); ++SubTrackIndex )
			{
				UInterpTrack* SubTrack = NewTrack->SubTracks( SubTrackIndex );
				for (INT KeyFrameIndex = SubTrack->GetNumKeyframes()-1; KeyFrameIndex >= FinalIndex; --KeyFrameIndex)
				{
					SubTrack->RemoveKeyframe(KeyFrameIndex);
				}
			}
			NewTrack->TrackTitle = NewTrackName;
		}
	}

	if (bInDeleteSelectedTracks)
	{
		DeleteSelectedTracks();
	}

	//empty selection
	DeselectAllTracks(FALSE);
	DeselectAllGroups(FALSE);
	
	//add all copied tracks to selection
	const UBOOL bDeselectOtherTracks = FALSE;
	UBOOL bNewGroupSelected = FALSE;
	for (INT i = 0; i < RecordingTracks.Num(); ++i)
	{
		UInterpTrack* TrackToSelect = RecordingTracks(i);
		UInterpGroup* OwnerGroup = OwnerGroups(i);

		UInterpTrackMove* TrackToSelectAsMoveTrack = Cast<UInterpTrackMove>(TrackToSelect);

		if (!bNewGroupSelected && OwnerGroup && (TrackToSelectAsMoveTrack!= NULL))
		{
			//set this group as the preview group
			LockCamToGroup(OwnerGroup);
			bNewGroupSelected = TRUE;
		}

		SelectTrack( OwnerGroup, TrackToSelect, bDeselectOtherTracks);
	}

	// Update the property window to reflect the group deselection
	UpdatePropertyWindow();
	// Request an update of the track windows
	InvalidateTrackWindowViewports();
}

/** Polls state of controller (Game Caster, Game Pad) and adjust each track by that input*/
void WxInterpEd::AdjustRecordingTracksByInput(void)
{
	//poll controller for input

	//send input to each selected track
	for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		//UInterpTrack* TrackToAdjust = *TrackIt;
	}
}

/**Used during recording to capture a key frame at the current position of the timeline*/
void WxInterpEd::RecordKeys(void)
{
	for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		UInterpTrack* TrackToSample = *TrackIt;
		UInterpGroup* ParentGroup = TrackIt.GetGroup();

		UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(ParentGroup);
		check(GrInst);
		UInterpTrackInst* TrInst = GrInst->TrackInst(TrackIt.GetTrackIndex());
		check(TrInst);

		UInterpTrackHelper* TrackHelper = NULL;
		UClass* Class = LoadObject<UClass>( NULL, *TrackToSample->GetEdHelperClassName(), NULL, LOAD_None, NULL );
		if ( Class != NULL )
		{
			TrackHelper = CastChecked<UInterpTrackHelper>(Class->GetDefaultObject());
		}

		FLOAT	fKeyTime = SnapTime( Interp->Position, FALSE );
		if ( (TrackHelper == NULL) || !TrackHelper->PreCreateKeyframe(TrackToSample, fKeyTime) )
		{
			continue;
		}

		TrackToSample->Modify();

		// Add key at current time, snapped to the grid if its on.
		INT NewKeyIndex = TrackToSample->AddKeyframe( Interp->Position, TrInst, InitialInterpMode );
		TrackToSample->UpdateKeyframe( NewKeyIndex, TrInst);
	}

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();
}


/**Store off parent positions so we can apply the parents delta of movement to the child*/
void WxInterpEd::SaveRecordingParentOffsets(void)
{
	RecordingParentOffsets.Empty();
	if (RecordMode == MatineeConstants::RECORD_MODE_NEW_CAMERA_ATTACHED)
	{
		for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
		{
			UInterpTrack* TrackToSample = *TrackIt;
			UInterpGroup* ParentGroup = TrackIt.GetGroup();

			UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(ParentGroup);
			check(GrInst);
			UInterpTrackInst* TrInst = GrInst->TrackInst(TrackIt.GetTrackIndex());
			check(TrInst);

			//get the actor that is currently recording
			AActor* Actor = TrInst->GetGroupActor();
			if(!Actor)
			{
				return;
			}

			//get the parent actor
			AActor* ParentActor = Actor->Base;
			if (ParentActor)
			{
				//save the offsets
				RecordingParentOffsets.Set(ParentActor, ParentActor->Location);
			}
		}
	}
}

/**Apply the movement of the parent to child during recording*/
void WxInterpEd::ApplyRecordingParentOffsets(void)
{
	if (RecordMode == MatineeConstants::RECORD_MODE_NEW_CAMERA_ATTACHED)
	{
		//list of unique actors to apply parent transforms to
		TArray<AActor*> RecordingActorsWithParents;
		for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
		{
			UInterpTrack* TrackToSample = *TrackIt;
			UInterpGroup* ParentGroup = TrackIt.GetGroup();

			UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(ParentGroup);
			check(GrInst);

			//get the actor that is currently recording
			AActor* Actor = GrInst->GetGroupActor();
			if(!Actor)
			{
				return;
			}

			//get the parent actor
			AActor* ParentActor = Actor->Base;
			if (ParentActor)
			{
				//keep a list of actors to apply offsets to.
				RecordingActorsWithParents.AddUniqueItem(Actor);
			}
		}

		//now apply parent offsets to list
		for (INT i = 0; i < RecordingActorsWithParents.Num(); ++i)
		{
			AActor* Actor = RecordingActorsWithParents(i);
			check(Actor);
			
			//get parent actor
			AActor* ParentActor = Actor->Base;
			check(ParentActor);
			
			//get the old position out of the map
			FVector OldOffset = RecordingParentOffsets.FindRef(ParentActor);

			//find the delta of the parent actor
			FVector Delta = ParentActor->Location - OldOffset;

			//apply the delta to the actor we're recoding
			Actor->Location += Delta;

			//we have to move the level viewport as well.
			ACameraActor* CameraActor = Cast<ACameraActor>(Actor);
			if (CameraActor)
			{
				//add new camera
				FEditorLevelViewportClient* LevelVC = GetRecordingViewport();
				if (LevelVC)
				{
					LevelVC->ViewLocation += Delta;
				}
			}
		}
	}
}

/**
 * Returns the custom recording viewport if it has been created yet
 * @return - NULL, if no record viewport exists yet, or the current recording viewport
 */
FEditorLevelViewportClient* WxInterpEd::GetRecordingViewport(void)
{
	//see if we already have a custom recording frame...if so, just use that
	// Move any perspective viewports to coincide with moved actor.
	for(INT i=0; i<GApp->EditorFrame->ViewportConfigData->GetViewportCount(); i++)
	{
		WxLevelViewportWindow* TempViewport = GApp->EditorFrame->ViewportConfigData->AccessViewport(i).ViewportWindow;
		if (TempViewport && TempViewport->IsMatineeRecordingWindow())
		{
			return TempViewport;
		}
	}
	for(TMap<FEditorLevelViewportClient*, AActor*>::TIterator ViewportIt(ExtraViewports);ViewportIt;++ViewportIt)
	{
		FEditorLevelViewportClient* TempViewport = ViewportIt.Key();
		if (TempViewport && TempViewport->IsMatineeRecordingWindow())
		{
			return TempViewport;
		}
	}
	return NULL;
}

/** Call utility to crop the current key in the selected track. */
void WxInterpEd::CropAnimKey(UBOOL bCropBeginning)
{
	// Check we have a group and track selected
	if( HasATrackSelected() )
	{
		return;
	}

	// Check an AnimControlTrack is selected to avoid messing with the transaction system preemptively. 
	if( HasATrackSelected(UInterpTrackAnimControl::StaticClass()) )
	{
		InterpEdTrans->BeginSpecial( *LocalizeUnrealEd(TEXT("CropAnimationKey")) );
		{
			for( TTrackClassTypeIterator<UInterpTrackAnimControl> AnimTrackIt(GetSelectedTrackIterator<UInterpTrackAnimControl>()); AnimTrackIt; ++AnimTrackIt )
			{
				UInterpTrackAnimControl* AnimTrack = *AnimTrackIt;

				// Call crop utility.
				AnimTrack->Modify();
				AnimTrack->CropKeyAtPosition(Interp->Position, bCropBeginning);
			}
		}
		InterpEdTrans->EndSpecial();
	}
}


/** Go over all Matinee Action (SeqAct_Interp) making sure that their connectors (variables for group and outputs for event tracks) are up to date. */
void WxInterpEd::UpdateMatineeActionConnectors()
{
	USequence* RootSeq = Interp->GetRootSequence();
	check(RootSeq);
	RootSeq->UpdateInterpActionConnectors();
}


/** Jump the position of the interpolation to the current time, updating Actors. */
void WxInterpEd::SetInterpPosition( FLOAT NewPosition )
{
	UBOOL bTimeChanged = (NewPosition != Interp->Position);

	// Make sure particle replay tracks have up-to-date editor-only transient state
	UpdateParticleReplayTracks();

	// Move preview position in interpolation to where we want it, and update any properties
	Interp->UpdateInterp( NewPosition, TRUE, bTimeChanged );

	// When playing/scrubbing, we release the current keyframe from editing
	if(bTimeChanged)
	{
		Opt->bAdjustingKeyframe = FALSE;
		Opt->bAdjustingGroupKeyframes = FALSE;
	}

	// If we are locking the camera to a group, update it here
	UpdateCameraToGroup(TRUE);

	// Set the camera frustum colours to show which is being viewed.
	UpdateCamColours();

	// Redraw viewport.
	InvalidateTrackWindowViewports();

	// Update the position of the marker in the curve view.
	CurveEd->SetPositionMarker( TRUE, Interp->Position, PosMarkerColor );
}



/** Make sure particle replay tracks have up-to-date editor-only transient state */
void WxInterpEd::UpdateParticleReplayTracks()
{
	for( INT CurGroupIndex = 0; CurGroupIndex < Interp->InterpData->InterpGroups.Num(); ++CurGroupIndex )
	{
		UInterpGroup* CurGroup = Interp->InterpData->InterpGroups( CurGroupIndex );
		if( CurGroup != NULL )
		{
			for( INT CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
			{
				UInterpTrack* CurTrack = CurGroup->InterpTracks( CurTrackIndex );
				if( CurTrack != NULL )
				{
					UInterpTrackParticleReplay* ParticleReplayTrack = Cast< UInterpTrackParticleReplay >( CurTrack );
					if( ParticleReplayTrack != NULL )
					{
						// Copy time step
						ParticleReplayTrack->FixedTimeStep = SnapAmount;
					}
				}
			}
		}
	}
}



/** Refresh the Matinee position marker and viewport state */
void WxInterpEd::RefreshInterpPosition()
{
	// check to see if InterpData exists. Otherwise it crashes in UpdateParticleReplayTracks
	if (Interp->InterpData)
	{
		SetInterpPosition( Interp->Position );
	}
}



/** Ensure the curve editor is synchronised with the track editor. */
void WxInterpEd::SyncCurveEdView()
{
	CurveEd->StartIn = ViewStartTime;
	CurveEd->EndIn = ViewEndTime;
	CurveEd->CurveEdVC->Viewport->Invalidate();
}

/** Add the property being controlled by this track to the graph editor. */
void WxInterpEd::AddTrackToCurveEd( FString GroupName, FColor GroupColor, UInterpTrack* InTrack, UBOOL bShouldShowTrack )
{
	FString TrackTitle = InTrack->TrackTitle;

	// Slight hack for movement tracks.  Subtracks that translate are prepended with a T and subtracks that rotate are prepended with an R
	// We do this to conserve space on the curve title bar.
	UInterpTrackMoveAxis* MoveAxisTrack = Cast<UInterpTrackMoveAxis>(InTrack);
	if( MoveAxisTrack )
	{
		BYTE MoveAxis = MoveAxisTrack->MoveAxis;
		if( MoveAxis == AXIS_TranslationX || MoveAxis == AXIS_TranslationY || MoveAxis == AXIS_TranslationZ )
		{
			TrackTitle = FString(TEXT("T")) + TrackTitle;
		}
		else
		{
			TrackTitle = FString(TEXT("R")) + TrackTitle;
		}
	}
	FString CurveName = GroupName + FString(TEXT("_")) + TrackTitle;

	// Toggle whether this curve is edited in the Curve editor.
	if( !bShouldShowTrack )
	{
		IData->CurveEdSetup->RemoveCurve(InTrack);
	}
	else
	{
		FColor CurveColor = GroupColor;

		// If we are adding selected curve - highlight it.
		if( InTrack->bIsSelected )
		{
			CurveColor = SelectedCurveColor;
		}

		// Add track to the curve editor.
		UBOOL bColorTrack = FALSE;

		if( InTrack->IsA(UInterpTrackColorProp::StaticClass() ) )
		{
			bColorTrack = TRUE;
		}

		IData->CurveEdSetup->AddCurveToCurrentTab(InTrack, CurveName, CurveColor, bColorTrack, bColorTrack);
	}

	CurveEd->CurveChanged();
}


/** 
 *	Get the actor that the camera should currently be viewed through.
 *	We look here to see if the viewed group has a Director Track, and if so, return that Group.
 */
AActor* WxInterpEd::GetViewedActor()
{
	if( CamViewGroup != NULL )
	{
		UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(CamViewGroup);
		if(DirGroup)
		{
			return Interp->FindViewedActor();
		}
		else
		{
			UInterpGroupInst* GroupInst = Interp->FindFirstGroupInst(CamViewGroup);
			if( GroupInst != NULL )
			{
				return GroupInst->GetGroupActor();
			}
		}
	}

	return NULL;
}

/** Can input NULL to unlock camera from all group. */
void WxInterpEd::LockCamToGroup(class UInterpGroup* InGroup, const UBOOL bResetViewports)
{
	// If different from current locked group - release current.
	if(CamViewGroup && (CamViewGroup != InGroup))
	{
		// Re-show the actor (if present)
		//UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(ActiveGroup);
		//check(GrInst);
		//if(GrInst->GroupActor)
		//	GrInst->GroupActor->bHiddenEd = FALSE;

		// Reset viewports (clear roll etc).  But not when recording
		if (bResetViewports)
		{
			for(INT i=0; i<GApp->EditorFrame->ViewportConfigData->GetViewportCount(); i++)
			{
				WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->AccessViewport(i).ViewportWindow;
				if(LevelVC && LevelVC->ViewportType == LVT_Perspective && LevelVC->AllowMatineePreview() )
				{
					LevelVC->ViewRotation.Roll = 0;
					LevelVC->bConstrainAspectRatio = FALSE;
					LevelVC->OverridePostProcessSettingsAlpha = 0.f;
					LevelVC->ViewFOV = GEditor->FOVAngle;
					LevelVC->bEnableFading = FALSE;
					LevelVC->bEnableColorScaling = FALSE;
				}
			}
		}

		CamViewGroup = NULL;
	}

	// If non-null new group - switch to it now.
	if(InGroup)
	{
		// Hide the actor when viewing through it.
		//UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(InGroup);
		//check(GrInst);
		//GrInst->GroupActor->bHiddenEd = TRUE;

		CamViewGroup = InGroup;

		// Move camera to track now.
		UpdateCameraToGroup(TRUE);
	}
}

/** Update the colours of any CameraActors we are manipulating to match their group colours, and indicate which is 'active'. */
void WxInterpEd::UpdateCamColours()
{
	AActor* ViewedActor = Interp->FindViewedActor();

	for(INT i=0; i<Interp->GroupInst.Num(); i++)
	{
		ACameraActor* Cam = Cast<ACameraActor>(Interp->GroupInst(i)->GetGroupActor());
		if(Cam && Cam->DrawFrustum)
			{
				if(Interp->GroupInst(i)->GetGroupActor() == ViewedActor)
				{
					Cam->DrawFrustum->FrustumColor = ActiveCamColor;
				}
				else
				{
					Cam->DrawFrustum->FrustumColor = Interp->GroupInst(i)->Group->GroupColor;
				}
			}
	}
}

/** 
 *	If we are viewing through a particular group - move the camera to correspond. 
 */
void WxInterpEd::UpdateCameraToGroup(const UBOOL bInUpdateStandardViewports)
{
	UBOOL bEnableColorScaling = FALSE;
	FVector ColorScale(1.f,1.f,1.f);
	FRenderingPerformanceOverrides RenderingOverrides(E_ForceInit);

	// If viewing through the director group, see if we have a fade track, and if so see how much fading we should do.
	FLOAT FadeAmount = 0.f;
	if(CamViewGroup)
	{
		UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(CamViewGroup);
		if(DirGroup)
		{
			UInterpTrackDirector* DirTrack = DirGroup->GetDirectorTrack();
			if (DirTrack && DirTrack->CutTrack.Num() > 0)
			{
				RenderingOverrides = Interp->RenderingOverrides;
			}

			UInterpTrackFade* FadeTrack = DirGroup->GetFadeTrack();
			if(FadeTrack && !FadeTrack->IsDisabled())
			{
				FadeAmount = FadeTrack->GetFadeAmountAtTime(Interp->Position);
			}

			// Set TimeDilation in the LevelInfo based on what the Slomo track says (if there is one).
			UInterpTrackSlomo* SlomoTrack = DirGroup->GetSlomoTrack();
			if(SlomoTrack && !SlomoTrack->IsDisabled() && Interp->bIsPlaying)
			{
				GWorld->GetWorldInfo()->TimeDilation = SlomoTrack->GetSlomoFactorAtTime(Interp->Position);
			}
			else
			{
				// Set to normal time (not necessarily the value of TimeDilation when MatineeEd was opened but the original will be restored on exit)
				GWorld->GetWorldInfo()->TimeDilation = 1.0f;
			}

			UInterpTrackColorScale* ColorTrack = DirGroup->GetColorScaleTrack();
			if(ColorTrack && !ColorTrack->IsDisabled())
			{
				bEnableColorScaling = TRUE;
				ColorScale = ColorTrack->GetColorScaleAtTime(Interp->Position);
			}
		}
	}

	AActor* DefaultViewedActor = GetViewedActor();

	if (bInUpdateStandardViewports)
	{
		// Move any perspective viewports to coincide with moved actor.
		for(INT i=0; i<GApp->EditorFrame->ViewportConfigData->GetViewportCount(); i++)
		{
			WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->AccessViewport(i).ViewportWindow;
			if(LevelVC && LevelVC->ViewportType == LVT_Perspective && LevelVC->AllowMatineePreview() )
			{
				UpdateLevelViewport(DefaultViewedActor, LevelVC, RenderingOverrides, FadeAmount, ColorScale, bEnableColorScaling);
			}
		}
	}
	//Now update all the other viewports that aren't wx viewport (given by the director window)
	for(TMap<FEditorLevelViewportClient*, AActor*>::TIterator ViewportIt(ExtraViewports);ViewportIt;++ViewportIt)
	{
		FEditorLevelViewportClient* LevelVC = ViewportIt.Key();
		if(LevelVC && LevelVC->ViewportType == LVT_Perspective && LevelVC->AllowMatineePreview() )
		{
			AActor* ViewedActor = ViewportIt.Value() ? ViewportIt.Value() : DefaultViewedActor;

			UpdateLevelViewport(ViewedActor, LevelVC, RenderingOverrides, FadeAmount, ColorScale, bEnableColorScaling);
		}
	}
}

/**
 * Updates a viewport from a given actor
 * @param InActor - The actor to track the viewport to
 * @param InViewportClient - The viewport to update
 * @param InFadeAmount - Fade amount for camera
 * @param InColorScale - Color scale for render
 * @param bInEnableColorScaling - whether to use color scaling or not
 */
void WxInterpEd::UpdateLevelViewport(AActor* InActor, FEditorLevelViewportClient* InViewportClient, const FRenderingPerformanceOverrides& RenderingOverrides, const FLOAT InFadeAmount, const FVector& InColorScale, const UBOOL bInEnableColorScaling )
{
	//if we're recording matinee and this is the proper recording window.  Do NOT update the viewport (it's being controlled by input)
	if ((RecordingState!=MatineeConstants::RECORDING_COMPLETE) && InViewportClient->IsMatineeRecordingWindow())
	{
		//if this actor happens to be a camera, let's copy the appropriate viewport settings back to the camera
		ACameraActor* CameraActor = Cast<ACameraActor>(InActor);
		if (CameraActor)
		{
			CameraActor->FOVAngle = InViewportClient->ViewFOV;
			CameraActor->AspectRatio = InViewportClient->AspectRatio;
			CameraActor->Location = InViewportClient->ViewLocation;
			CameraActor->Rotation = InViewportClient->ViewRotation;
		}
		return;
	}

	InViewportClient->RenderingOverrides = RenderingOverrides;

	ACameraActor* Cam = Cast<ACameraActor>(InActor);
	if (InActor)
	{
		InViewportClient->ViewLocation = InActor->Location;
		InViewportClient->ViewRotation = InActor->Rotation;				

		InViewportClient->FadeAmount = InFadeAmount;
		InViewportClient->bEnableFading = TRUE;

		InViewportClient->bEnableColorScaling = bInEnableColorScaling;
		InViewportClient->ColorScale = InColorScale;
	}
	else
	{
		InViewportClient->OverridePostProcessSettingsAlpha = 0.f;

		InViewportClient->bConstrainAspectRatio = FALSE;
		InViewportClient->ViewFOV = GEditor->FOVAngle;

		InViewportClient->FadeAmount = InFadeAmount;
		InViewportClient->bEnableFading = TRUE;
	}

	// If viewing through a camera - enforce aspect ratio and PP settings of camera.
	if(Cam)
	{
		InViewportClient->OverridePostProcessSettingsAlpha = Cam->CamOverridePostProcessAlpha;
		InViewportClient->OverrideProcessSettings = Cam->CamOverridePostProcess;

		InViewportClient->bConstrainAspectRatio = Cam->bConstrainAspectRatio;
		// If the Camera's aspect ratio is zero, put a more reasonable default here - this at least stops it from crashing
		// nb. the AspectRatio will be reported as a Map Check Warning
		if( Cam->AspectRatio == 0 )
		{
			InViewportClient->AspectRatio = 1.7f;
		}
		else
		{
			InViewportClient->AspectRatio = Cam->AspectRatio;
		}

		//if this isn't the recording viewport OR (it is the recording viewport and it's playing or we're scrubbing the timeline
		if ((InViewportClient != GetRecordingViewport()) || (Interp && ( Interp->bIsPlaying || (TrackWindow && TrackWindow->InterpEdVC->IsGrabbingHandle() ) ) ))
		{
			//don't stop the camera from zooming when not playing back
			InViewportClient->ViewFOV = Cam->FOVAngle;

			// If there are selected actors, invalidate the viewports hit proxies, otherwise they won't be selectable afterwards
			if ( InViewportClient->Viewport && GEditor->GetSelectedActorCount() > 0 )
			{
				InViewportClient->Viewport->InvalidateHitProxy();
			}
		}
	}
}

/** Restores a viewport's settings that were overridden by UpdateLevelViewport, where necessary. */
void WxInterpEd::RestoreLevelViewport(FEditorLevelViewportClient* InViewportClient)
{
	InViewportClient->RenderingOverrides = FRenderingPerformanceOverrides(E_ForceInit);
}

// Notification from the EdMode that a perspective camera has moves. 
// If we are locking the camera to a particular actor - we update its location to match.
void WxInterpEd::CamMoved(const FVector& NewCamLocation, const FRotator& NewCamRotation)
{
	// If cam not locked to something, do nothing.
	AActor* ViewedActor = GetViewedActor();
	if(ViewedActor)
	{
		// Update actors location/rotation from camera
		ViewedActor->Location = NewCamLocation;
		ViewedActor->Rotation = NewCamRotation;

		ViewedActor->InvalidateLightingCache();
		ViewedActor->ForceUpdateComponents();

		// In case we were modifying a keyframe for this actor.
		ActorModified();
	}
}


void WxInterpEd::ActorModified()
{
	// We only see if we need to update a track if we have a keyframe selected.
	if(Opt->bAdjustingKeyframe || Opt->bAdjustingGroupKeyframes)
	{
		check(Opt->SelectedKeys.Num() > 0);

		// For sanitys sake, make sure all these keys are part of the same group
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
		for( INT iSelectedKey = 1; iSelectedKey < Opt->SelectedKeys.Num(); iSelectedKey++ )
		{
			FInterpEdSelKey& rSelKey = Opt->SelectedKeys(iSelectedKey);
			if ( rSelKey.Group != SelKey.Group)
			{
				return;
			}
		}

		// Find the actor controlled by the selected group.
		UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(SelKey.Group);
		if( GrInst == NULL || GrInst->GetGroupActor() == NULL )
		{
			return;
		}
  
		// See if this is one of the actors that was just moved.
		UBOOL bTrackActorModified = FALSE;

		TArray<UObject*> NewObjects;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if ( Actor == GrInst->GetGroupActor() )
			{
				bTrackActorModified = TRUE;
				break;
	    }
		}
        
    // If so, update the selected keyframe on the selected track to reflect its new position.
    if(bTrackActorModified)
    {
      InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("UpdateKeyframe") );

			for( INT iSelectedKey = 0; iSelectedKey < Opt->SelectedKeys.Num(); iSelectedKey++ )
			{
				FInterpEdSelKey& rSelKey = Opt->SelectedKeys(iSelectedKey);
				rSelKey.Track->Modify();
	    
				UInterpTrack* Parent = Cast<UInterpTrack>( rSelKey.Track->GetOuter() );
				if( Parent )
				{
					// This track is a subtrack of some other track.  
					// Get the parent track index and let the parent update the childs keyframe 
					UInterpTrackInst* TrInst = GrInst->TrackInst( rSelKey.Group->InterpTracks.FindItemIndex( Parent ) );
					Parent->UpdateChildKeyframe( rSelKey.Track, rSelKey.KeyIndex, TrInst );
				}
				else
				{
					// This track is a normal track parented to a group
					UInterpTrackInst* TrInst = GrInst->TrackInst( rSelKey.Group->InterpTracks.FindItemIndex( rSelKey.Track ) );
					rSelKey.Track->UpdateKeyframe( rSelKey.KeyIndex, TrInst );
				}
			}

      InterpEdTrans->EndSpecial();
    }
	}

	// This might have been a camera propety - update cameras.
	UpdateCameraToGroup(TRUE);
}

void WxInterpEd::ActorSelectionChange()
{
	// Ignore this selection notification if desired.
	if(bIgnoreActorSelection)
	{
		return;
	}

	// When an actor selection changed and the interp groups associated to the selected actors does NOT match 
	// the selected interp groups (or tracks if only interp tracks are selected), that means the user selected 
	// an actor in the level editing viewport and we need to synchronize the selection in Matinee. 

	TArray<UInterpGroup*> ActorGroups;

	// First, gather all the interp groups associated to the selected actors 
	// so that we can compare them to selected groups or tracks in Matinee. 
	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		UInterpGroupInst* GroupInstance = Interp->FindGroupInst(Actor);

		if(GroupInstance)
		{
			check(GroupInstance->Group);
			ActorGroups.AddUniqueItem(GroupInstance->Group);
		}
	}

	if( ActorGroups.Num() > 0 )
	{
		// There are actors referenced in the opened Matinee. Now, figure 
		// out if selected actors matches the selection in Matinee.

		UBOOL bSelectionIsOutOfSync = FALSE;

		// If all selected groups or tracks match up to the selected actors, then the user 
		// selected a group or track in Matinee. In this case, we are in sync with Matinee. 

		if( HasATrackSelected() )
		{
			for( TArray<UInterpGroup*>::TIterator GroupIter(ActorGroups); GroupIter; ++GroupIter )
			{
				if( !HasATrackSelected(*GroupIter) )
				{
					// NOTE: Since one selected actor did not have a selected track, we will clear the track selection in favor 
					// of selecting the groups instead. We do this because we don't know if the actor without the selected
					// track even has an interp track. It is guaranteed, however, to have an interp group associated to it. 
					bSelectionIsOutOfSync = TRUE;
					break;
				}
			}
		}
		else
		{
			for( TArray<UInterpGroup*>::TIterator GroupIter(ActorGroups); GroupIter; ++GroupIter )
			{
				if( !IsGroupSelected(*GroupIter) )
				{
					bSelectionIsOutOfSync = TRUE;
					break;
				}
			}
		}

		// The selected actors don't match up to the selection state in Matinee!
		if( bSelectionIsOutOfSync )
		{
			// Clear out all selections because the user might have deselected something, which means 
			// it wouldn't be in the array of interp groups that was gathered in this function.
			DeselectAll(FALSE);

			for( TArray<UInterpGroup*>::TIterator GroupIter(ActorGroups); GroupIter; ++GroupIter )
			{
				SelectGroup(*GroupIter, FALSE);
			}
		}
	}
	// If there are no interp groups associated to the selected 
	// actors, then clear out any existing Matinee selections.
	else
	{
		DeselectAll();
	}
}

UBOOL WxInterpEd::ProcessKeyPress(FName Key, UBOOL bCtrlDown, UBOOL bAltDown)
{
	return FALSE;
}



/**
 * Zooms the curve editor and track editor in or out by the specified amount
 *
 * @param ZoomAmount			Amount to zoom in or out
 * @param bZoomToTimeCursorPos	True if we should zoom to the time cursor position, otherwise mouse cursor position
 */
void WxInterpEd::ZoomView( FLOAT ZoomAmount, UBOOL bZoomToTimeCursorPos )
{
	// Proportion of interp we are currently viewing
	const FLOAT OldTimeRange = ViewEndTime - ViewStartTime;
	FLOAT CurrentZoomFactor = OldTimeRange / TrackViewSizeX;

	FLOAT NewZoomFactor = Clamp<FLOAT>(CurrentZoomFactor * ZoomAmount, 0.0003f, 1.0f);
	FLOAT NewTimeRange = NewZoomFactor * TrackViewSizeX;

	// zoom into scrub position
	if(bZoomToScrubPos)
	{
		FLOAT ViewMidTime = Interp->Position;
		ViewStartTime = ViewMidTime - 0.5*NewTimeRange;
		ViewEndTime = ViewMidTime + 0.5*NewTimeRange;
	}
	else
	{
		UBOOL bZoomedToCursorPos = FALSE;
		
		if( TrackWindow != NULL && IsMouseInWindow() )
		{
			// Figure out where the mouse cursor is over the Matinee track editor timeline
			wxPoint ScreenMousePos = wxGetMousePosition();
			wxPoint ClientMousePos =  TrackWindow->ScreenToClient( ScreenMousePos );
			INT ViewportClientAreaX = ClientMousePos.x;
			INT MouseXOverTimeline = ClientMousePos.x - LabelWidth;

  			if( MouseXOverTimeline >= 0 && MouseXOverTimeline < TrackViewSizeX )
			{
				// zoom into the mouse cursor's position over the view
				const FLOAT CursorPosInTime = ViewStartTime + ( MouseXOverTimeline / PixelsPerSec );
				const FLOAT CursorPosScalar = ( CursorPosInTime - ViewStartTime ) / OldTimeRange;

				ViewStartTime = CursorPosInTime - CursorPosScalar * NewTimeRange;
				ViewEndTime = CursorPosInTime + ( 1.0f - CursorPosScalar ) * NewTimeRange;

				bZoomedToCursorPos = TRUE;
			}
		}
		
		
		// We'll only zoom to the middle if we weren't already able to zoom to the cursor position.  Useful
		// if the mouse is outside of the window but the window still has focus for the zoom event
		if( !bZoomedToCursorPos )
		{
			// zoom into middle of view
			FLOAT ViewMidTime = ViewStartTime + 0.5f*(ViewEndTime - ViewStartTime);
			ViewStartTime = ViewMidTime - 0.5*NewTimeRange;
			ViewEndTime = ViewMidTime + 0.5*NewTimeRange;
		}
	}

	SyncCurveEdView();
}

struct TopLevelGroupInfo
{
	/** Index in original list */
	INT GroupIndex;

	/** Number of children */
	INT ChildCount;
};

void WxInterpEd::MoveActiveBy(INT MoveBy)
{
	const UBOOL bOnlyOneGroupSelected = (GetSelectedGroupCount() == 1);
	const UBOOL bOnlyOneTrackSelected = (GetSelectedTrackCount() == 1);

	// Only one group or one track can be selected for this operation
	if( !(bOnlyOneGroupSelected ^ bOnlyOneTrackSelected) )
	{
		return;
	}

	// We only support moving 1 unit in either direction
	check( Abs( MoveBy ) == 1 );
	
	InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("InterpEd_Move_SelectedTrackOrGroup") );

	// If no track selected, move group
	if( bOnlyOneGroupSelected )
	{
		UInterpGroup* SelectedGroup = *(GetSelectedGroupIterator());
		INT SelectedGroupIndex = IData->InterpGroups.FindItemIndex(SelectedGroup);

		// Is this a root group or a child group?  We'll only allow navigation through groups within the current scope.
		const UBOOL bIsChildGroup = SelectedGroup->bIsParented;

		// If we're moving a child group, then don't allow it to move outside of it's current folder's sub-group list
		if( bIsChildGroup )
		{
			INT TargetGroupIndex = SelectedGroupIndex + MoveBy;

			if( TargetGroupIndex >= 0 && TargetGroupIndex < IData->InterpGroups.Num() )
			{
				UInterpGroup* GroupToCheck = IData->InterpGroups( TargetGroupIndex );
				if( !GroupToCheck->bIsParented )
				{
					// Uh oh, we've reached the end of our parent group's list.  We'll deny movement.
					TargetGroupIndex = SelectedGroupIndex;
				}
			}

			if(TargetGroupIndex != SelectedGroupIndex && TargetGroupIndex >= 0 && TargetGroupIndex < IData->InterpGroups.Num())
			{
				IData->Modify();

				UInterpGroup* TempGroup = IData->InterpGroups(TargetGroupIndex);
				IData->InterpGroups(TargetGroupIndex) = IData->InterpGroups(SelectedGroupIndex);
				IData->InterpGroups(SelectedGroupIndex) = TempGroup;
			}
		}
		else
		{
			// We're moving a root group.  This is a bit tricky.  Our (single level) 'heirarchy' of groups is really just
			// a flat list of elements with a bool that indicates whether the element is a child of the previous non-child
			// element, so we need to be careful to skip over all child groups when reordering things.

			// Also, we'll also skip over the director group if we find one, since those will always appear immutable to the
			// user in the GUI.  The director group draws at the top of the group list and never appears underneath
			// another group or track, so we don't want to consider it when rearranging groups through the UI.

			// Digest information about the group list
			TArray< TopLevelGroupInfo > TopLevelGroups;
			INT SelectedGroupTLIndex = INDEX_NONE;
			{
				INT LastParentListIndex = INDEX_NONE;
				for( INT CurGroupIndex = 0; CurGroupIndex < IData->InterpGroups.Num(); ++CurGroupIndex )
				{
					UInterpGroup* CurGroup = IData->InterpGroups( CurGroupIndex );

					if( CurGroup->bIsParented )
					{
						// Add a new child to the last top level group
						check( LastParentListIndex != INDEX_NONE );
						++TopLevelGroups( LastParentListIndex ).ChildCount;
					}
					else
					{
						// A new top level group!
						TopLevelGroupInfo NewTopLevelGroup;
						NewTopLevelGroup.GroupIndex = CurGroupIndex;

						// Start at zero; we'll count these as we go along
						NewTopLevelGroup.ChildCount = 0;

						LastParentListIndex = TopLevelGroups.AddItem( NewTopLevelGroup );

						// If this is the active group, then keep track of that
						if( CurGroup == SelectedGroup )
						{
							SelectedGroupTLIndex = LastParentListIndex;
						}
					}
				}
			}

			// Make sure we found ourselves in the list
			check( SelectedGroupTLIndex != INDEX_NONE );



			// Determine our top-level list target
			INT TargetTLIndex = SelectedGroupTLIndex + MoveBy;
			if( TargetTLIndex >= 0 && TargetTLIndex < TopLevelGroups.Num() )
			{
				// Skip over director groups if we need to
				if( IData->InterpGroups( TopLevelGroups( TargetTLIndex ).GroupIndex )->IsA( UInterpGroupDirector::StaticClass() ) )
				{
					TargetTLIndex += MoveBy;
				}
			}

			// Make sure we're still in range
			if( TargetTLIndex >= 0 && TargetTLIndex < TopLevelGroups.Num() )
			{
				// Compute the list index that we'll be 'inserting before'
				INT InsertBeforeTLIndex = TargetTLIndex;
				if( MoveBy > 0 )
				{
					++InsertBeforeTLIndex;
				}

				// Compute our list destination
				INT TargetGroupIndex;
				if( InsertBeforeTLIndex < TopLevelGroups.Num() )
				{
					// Grab the top-level target group
					UInterpGroup* TLTargetGroup = IData->InterpGroups( TopLevelGroups( InsertBeforeTLIndex ).GroupIndex );

					// Setup 'insert' target group index
					TargetGroupIndex = TopLevelGroups( InsertBeforeTLIndex ).GroupIndex;
				}
				else
				{
					// We need to be at the very end of the list!
					TargetGroupIndex = IData->InterpGroups.Num();
				}


				// OK, time to move!
				const INT NumChildGroups = CountGroupFolderChildren( SelectedGroup );
				const INT NumGroupsToMove = NumChildGroups + 1;


				// We're about to modify stuff 
				IData->Modify();


				// Remove source groups from master list
				TArray< UInterpGroup* > GroupsToMove;
				for( INT GroupToMoveIndex = 0; GroupToMoveIndex < NumGroupsToMove; ++GroupToMoveIndex )
				{
					GroupsToMove.AddItem( IData->InterpGroups( SelectedGroupIndex ) );
					IData->InterpGroups.Remove( SelectedGroupIndex );

					// Adjust our target index for removed groups
					if( TargetGroupIndex >= SelectedGroupIndex )
					{
						--TargetGroupIndex;
					}
				};


				// Reinsert source groups at destination index
				for( INT GroupToMoveIndex = 0; GroupToMoveIndex < NumGroupsToMove; ++GroupToMoveIndex )
				{
					INT DestGroupIndex = TargetGroupIndex + GroupToMoveIndex;
					IData->InterpGroups.InsertItem( GroupsToMove( GroupToMoveIndex ), DestGroupIndex );
				};

			}
			else
			{
				// Out of range, we can't move any further
			}
		}
	}
	// If a track is selected, move it instead.
	else
	{
		FSelectedTrackIterator TrackIt(GetSelectedTrackIterator());
		UInterpGroup* Group = TrackIt.GetGroup();
		INT TrackIndex = TrackIt.GetTrackIndex();

		// Move the track itself.
		INT TargetTrackIndex = TrackIndex + MoveBy;

		Group->Modify();

		if(TargetTrackIndex >= 0 && TargetTrackIndex < Group->InterpTracks.Num())
		{
			UInterpTrack* TempTrack = Group->InterpTracks(TargetTrackIndex);
			Group->InterpTracks(TargetTrackIndex) = Group->InterpTracks(TrackIndex);
			Group->InterpTracks(TrackIndex) = TempTrack;

			// Now move any track instances inside their group instance.
			for(INT i=0; i<Interp->GroupInst.Num(); i++)
			{
				UInterpGroupInst* GrInst = Interp->GroupInst(i);
				if(GrInst->Group == Group)
				{
					check(GrInst->TrackInst.Num() == Group->InterpTracks.Num());

					GrInst->Modify();

					UInterpTrackInst* TempTrInst = GrInst->TrackInst(TargetTrackIndex);
					GrInst->TrackInst(TargetTrackIndex) = GrInst->TrackInst(TrackIndex);
					GrInst->TrackInst(TrackIndex) = TempTrInst;
				}
			}

			// Update selection to keep same track selected.
			TrackIt.MoveIteratorBy(MoveBy);

			// Selection stores keys by track index - safest to invalidate here.
			ClearKeySelection();
		}
	}

	InterpEdTrans->EndSpecial();

	UInterpGroup* Group = NULL;
	INT LabelTop = 0;
	INT LabelBottom = 0;

	if( HasATrackSelected() )
	{
		FSelectedTrackIterator TrackIter = GetSelectedTrackIterator();
		Group = TrackIter.GetGroup();
		GetTrackLabelPositions( Group, TrackIter.GetTrackIndex(), LabelTop, LabelBottom );
	}
	else
	{
		FSelectedGroupIterator GroupIter = GetSelectedGroupIterator();
		Group = *GroupIter;
		GetGroupLabelPosition( Group, LabelTop, LabelBottom );

	}

	// Attempt to autoscroll when the user moves a track or group label out of view. 
	if( Group != NULL )
	{
		// Figure out which window we are panning
		WxInterpEdVCHolder* CurrentWindow = Group->IsA(UInterpGroupDirector::StaticClass()) ? DirectorTrackWindow : TrackWindow;

		const INT ThumbSize = CurrentWindow->ScrollBar_Vert->GetThumbSize();
		const INT ThumbTop = CurrentWindow->ScrollBar_Vert->GetThumbPosition();
		const INT ThumbBottom = ThumbTop + ThumbSize;

		// Start the scrollbar at the current location. If the 
		// selected track title is visible, nothing will be scrolled. 
		INT NewScrollPosition = ThumbTop;

		// If the user moved the track title up and it's not viewable anymore,
		// move the scrollbar up so that the selected track is visible. 
		if( MoveBy < 0 && (LabelTop - ThumbTop) < 0 )
		{
			NewScrollPosition += (LabelTop - ThumbTop);
		}
		// If the user moved the track title down and it's not viewable anymore,
		// move the scrollbar down so that the selected track is visible. 
		else if( MoveBy > 0 && ThumbBottom < LabelBottom )
		{
			NewScrollPosition += (LabelBottom - ThumbBottom);
		}

		CurrentWindow->SetThumbPosition(NewScrollPosition);
		CurrentWindow->AdjustScrollBar();
	}

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();
}

void WxInterpEd::MoveActiveUp()
{
	MoveActiveBy(-1);
}

void WxInterpEd::MoveActiveDown()
{
	MoveActiveBy(+1);
}

void WxInterpEd::InterpEdUndo()
{
	GEditor->Trans->Undo();
	UpdateMatineeActionConnectors();
	CurveEd->SetRegionMarker(TRUE, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);
	CurveEd->SetEndMarker(TRUE, IData->InterpLength);
	Opt->bAdjustingKeyframe = FALSE;
	Opt->bAdjustingGroupKeyframes = FALSE;

	// Make sure the director track window is only visible if we have a director group!
	UpdateDirectorTrackWindowVisibility();

	// A new group may have been added (via duplication), so we'll need to update our scroll bar
	UpdateTrackWindowScrollBars();

	// Make sure that the viewports get updated after the Undo operation
	InvalidateTrackWindowViewports();

	if(Interp != NULL)
	{
		Interp->RecaptureActorState();
	}
}

void WxInterpEd::InterpEdRedo()
{
	GEditor->Trans->Redo();
	UpdateMatineeActionConnectors();
	CurveEd->SetRegionMarker(TRUE, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);
	CurveEd->SetEndMarker(TRUE, IData->InterpLength);
	Opt->bAdjustingKeyframe = FALSE;
	Opt->bAdjustingGroupKeyframes = FALSE;

	// Make sure the director track window is only visible if we have a director group!
	UpdateDirectorTrackWindowVisibility();

	// A new group may have been added (via duplication), so we'll need to update our scroll bar
	UpdateTrackWindowScrollBars();

	// Make sure that the viewports get updated after the Undo operation
	InvalidateTrackWindowViewports();

	if(Interp != NULL)
	{
		Interp->RecaptureActorState();
	}
}

/*******************************************************************************
* InterpTrack methods used only in the editor.
*******************************************************************************/

// Common FName used just for storing name information while adding Keyframes to tracks.
static FName		KeyframeAddDataName = NAME_None;
static USoundCue	*KeyframeAddSoundCue = NULL;
static UClass		*KeyframeNotifyClass = NULL;
static FName		TrackAddPropName = NAME_None;
static FName		AnimSlotName = NAME_None;
static FString		FaceFXGroupName = FString(TEXT(""));
static FString		FaceFXAnimName = FString(TEXT(""));


/**
 * Sets the global property name to use for newly created property tracks
 *
 * @param NewName The property name
 */
void WxInterpEd::SetTrackAddPropName( const FName NewName )
{
	TrackAddPropName = NewName;
}


IMPLEMENT_CLASS(UInterpTrackHelper);

/**
 * @param  Track	The track to get the actor for.
 * @return Returns the actor for the group's track if one exists, NULL otherwise.
 */
AActor* UInterpTrackHelper::GetGroupActor(const UInterpTrack* Track) const
{
	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
	check(mode != NULL);

	WxInterpEd	*InterpEd = mode->InterpEd;
	check(InterpEd != NULL);

	UInterpGroupInst* GrInst = NULL;

	// Traverse through the selected tracks in hopes of finding the associated group. 
	for( FSelectedTrackIterator TrackIt(InterpEd->GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		if( (*TrackIt) == Track )
		{
			GrInst = InterpEd->Interp->FindFirstGroupInst(TrackIt.GetGroup());
			break;
		}
	}
	
	return ( GrInst != NULL ) ? GrInst->GetGroupActor() : NULL;
}


/** Checks track-dependent criteria prior to adding a new track.
 * Responsible for any message-boxes or dialogs for selecting track-specific parameters.
 * Called on default object.
 *
 * @param Group The group that this track is being added to
 * @param	Trackdef Pointer to default object for this UInterpTrackClass.
 * @param	bDuplicatingTrack Whether we are duplicating this track or creating a new one from scratch.
 * @param bAllowPrompts When TRUE, we'll prompt for more information from the user with a dialog box if we need to
 * @return Returns TRUE if this track can be created and FALSE if some criteria is not met (i.e. A named property is already controlled for this group).
 */
UBOOL UInterpTrackAnimControlHelper::PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, UBOOL bDuplicatingTrack, UBOOL bAllowPrompts ) const
{
	// For AnimControl tracks - pop up a dialog to choose slot name.
	AnimSlotName = NAME_None;

	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
	check(mode != NULL);

	WxInterpEd	*InterpEd = mode->InterpEd;
	check(InterpEd != NULL);

	UInterpGroupInst* GrInst = InterpEd->Interp->FindFirstGroupInst(Group);
	check(GrInst);

	AActor* Actor = GrInst->GetGroupActor();
	if ( Actor != NULL )
	{
		// If this is the first AnimControlTrack, then init anim control now.
		// We need that before calling GetAnimControlSlotDesc
		if( !Group->HasAnimControlTrack() )
		{
			Actor->PreviewBeginAnimControl( Group );
		}

		if( bAllowPrompts )
		{
			TArray<FAnimSlotDesc> SlotDescs;
			Actor->GetAnimControlSlotDesc(SlotDescs);

			// If we get no information - just allow it to be created with empty slot.
			if( SlotDescs.Num() == 0 )
			{
				return TRUE;
			}

			// Build combo to let you pick a slot. Don't put any names in that have already used all their channels. */

			TArray<FString> SlotStrings;
			for(INT i=0; i<SlotDescs.Num(); i++)
			{
				INT ChannelsUsed = GrInst->Group->GetAnimTracksUsingSlot( SlotDescs(i).SlotName );
				if(ChannelsUsed < SlotDescs(i).NumChannels)
				{
					SlotStrings.AddItem(*SlotDescs(i).SlotName.ToString());
				}
			}

			// If no slots free - we fail to create track.
			if(SlotStrings.Num() == 0)
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("Error_NoAnimChannelsLeft") );
				return FALSE;
			}

			WxDlgGenericComboEntry dlg;
			if( dlg.ShowModal( TEXT("ChooseAnimSlot"), TEXT("SlotName"), SlotStrings, 0, TRUE ) == wxID_OK )
			{
				AnimSlotName = FName( *dlg.GetSelectedString() );
				if ( AnimSlotName != NAME_None )
				{
					return TRUE;
				}
			}
		}
		else
		{
			// Prompts aren't allowed, so just succeed with defaults
			return TRUE;
		}
	}

	return FALSE;
}

void  UInterpTrackAnimControlHelper::PostCreateTrack( UInterpTrack *Track, UBOOL bDuplicatingTrack, INT TrackIndex ) const
{
	UInterpTrackAnimControl* AnimTrack = CastChecked<UInterpTrackAnimControl>(Track);
	AnimTrack->SlotName = AnimSlotName;

	// When you change the SlotName, change the TrackTitle to reflect that.
	UInterpTrackAnimControl* DefAnimTrack = CastChecked<UInterpTrackAnimControl>(AnimTrack->GetClass()->GetDefaultObject());
	FString DefaultTrackTitle = DefAnimTrack->TrackTitle;

	if(AnimTrack->SlotName == NAME_None)
	{
		AnimTrack->TrackTitle = DefaultTrackTitle;
	}
	else
	{
		AnimTrack->TrackTitle = FString::Printf( TEXT("%s:%s"), *DefaultTrackTitle, *AnimTrack->SlotName.ToString() );
	}
}


/** Checks track-dependent criteria prior to adding a new keyframe.
* Responsible for any message-boxes or dialogs for selecting key-specific parameters.
* Optionally creates/references a key-specific data object to be used in PostCreateKeyframe.
*
* @param	Track	Pointer to the currently selected track.
* @param	KeyTime	The time that this Key becomes active.
* @return	Returns TRUE if this key can be created and FALSE if some criteria is not met (i.e. No related item selected in browser).
*/
UBOOL UInterpTrackAnimControlHelper::PreCreateKeyframe( UInterpTrack *Track, FLOAT fTime ) const
{
	KeyframeAddDataName = NAME_None;
	UInterpTrackAnimControl	*AnimTrack = CastChecked<UInterpTrackAnimControl>(Track);
	UInterpGroup* Group = CastChecked<UInterpGroup>(Track->GetOuter());

	if ( Group->GroupAnimSets.Num() > 0 )
	{
		// Make array of AnimSequence names.
		TArray<FString> AnimNames;
		for ( INT i = 0; i < Group->GroupAnimSets.Num(); i++ )
		{
			UAnimSet *Set = Group->GroupAnimSets(i);
			if ( Set )
			{
				for ( INT j = 0; j < Set->Sequences.Num(); j++ )
				{
					AnimNames.AddUniqueItem(Set->Sequences(j)->SequenceName.ToString());
				}
			}
		}

		// If we couldn't find any AnimSequence names, don't bother continuing.
		if ( AnimNames.Num() > 0 )
		{
			// Show the dialog.
			WxDlgGenericComboEntry	dlg;
			const INT Result = dlg.ShowModal( TEXT("NewSeqKey"), TEXT("SeqKeyName"), AnimNames, 0, TRUE );
			if ( Result == wxID_OK )
			{
				KeyframeAddDataName = FName( *dlg.GetSelectedString() );
				return TRUE;
			}
		}
		else
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("NoAnimSeqsFound") );
		}
	}

	return FALSE;
}

/** Uses the key-specific data object from PreCreateKeyframe to initialize the newly added key.
* @param	Track		Pointer to the currently selected track.
* @param	KeyIndex	The index of the keyframe that as just added.  This is the index returned by AddKeyframe.
*/
void  UInterpTrackAnimControlHelper::PostCreateKeyframe( UInterpTrack *Track, INT KeyIndex ) const
{
	UInterpTrackAnimControl	*AnimTrack = CastChecked<UInterpTrackAnimControl>(Track);
	FAnimControlTrackKey& NewSeqKey = AnimTrack->AnimSeqs( KeyIndex );
	NewSeqKey.AnimSeqName = KeyframeAddDataName;
	KeyframeAddDataName = NAME_None;
}

IMPLEMENT_CLASS(UInterpTrackAnimControlHelper);

/** Checks track-dependent criteria prior to adding a new keyframe.
* Responsible for any message-boxes or dialogs for selecting key-specific parameters.
* Optionally creates/references a key-specific data object to be used in PostCreateKeyframe.
*
* @param	Track	Pointer to the currently selected track.
* @param	KeyTime	The time that this Key becomes active.
* @return	Returns TRUE if this key can be created and FALSE if some criteria is not met (i.e. No related item selected in browser).
*/
UBOOL UInterpTrackDirectorHelper::PreCreateKeyframe( UInterpTrack *Track, FLOAT KeyTime ) const
{
	// If adding a cut, bring up combo to let user choose group to cut to.
	KeyframeAddDataName = NAME_None;

	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
	check(mode != NULL);
	check(mode->InterpEd != NULL);

	if ( (mode != NULL) && (mode->InterpEd != NULL) )
	{
		// Make array of group names
		TArray<FString> GroupNames;
		for ( INT i = 0; i < mode->InterpEd->IData->InterpGroups.Num(); i++ )
		{
			// Skip folder groups
			if( !mode->InterpEd->IData->InterpGroups(i)->bIsFolder)
			{
				GroupNames.AddItem( *(mode->InterpEd->IData->InterpGroups(i)->GroupName.ToString()) );
			}
		}

		WxDlgGenericComboEntry	dlg;
		const INT	Result = dlg.ShowModal( TEXT("NewCut"), TEXT("CutToGroup"), GroupNames, 0, TRUE );
		if ( Result == wxID_OK )
		{
			KeyframeAddDataName = FName( *dlg.GetSelectedString() );
			return TRUE;
		}
	}
	else
	{
	}

	return FALSE;
}

/** Uses the key-specific data object from PreCreateKeyframe to initialize the newly added key.
*
* @param	Track	Pointer to the currently selected track.
* @param KeyIndex	The index of the keyframe that as just added.  This is the index returned by AddKeyframe.
*/
void  UInterpTrackDirectorHelper::PostCreateKeyframe( UInterpTrack *Track, INT KeyIndex ) const
{
	UInterpTrackDirector	*DirectorTrack = CastChecked<UInterpTrackDirector>(Track);
	FDirectorTrackCut& NewDirCut = DirectorTrack->CutTrack( KeyIndex );
	NewDirCut.TargetCamGroup = KeyframeAddDataName;
	KeyframeAddDataName = NAME_None;
}

IMPLEMENT_CLASS(UInterpTrackDirectorHelper);

/** Checks track-dependent criteria prior to adding a new keyframe.
* Responsible for any message-boxes or dialogs for selecting key-specific parameters.
* Optionally creates/references a key-specific data object to be used in PostCreateKeyframe.
*
* @param	Track	Pointer to the currently selected track.
* @param	KeyTime	The time that this Key becomes active.
* @return	Returns TRUE if this key can be created and FALSE if some criteria is not met (i.e. No related item selected in browser).
*/
UBOOL UInterpTrackEventHelper::PreCreateKeyframe( UInterpTrack *Track, FLOAT KeyTime ) const
{
	KeyframeAddDataName = NAME_None;

	// Prompt user for name of new event.
	WxDlgGenericStringEntry	dlg;
	const INT Result = dlg.ShowModal( TEXT("NewEventKey"), TEXT("NewEventName"), TEXT("Event0") );
	if( Result == wxID_OK )
	{
		FString TempString = dlg.GetEnteredString();
		TempString = TempString.Replace(TEXT(" "),TEXT("_"));
		KeyframeAddDataName = FName( *TempString );
		return TRUE;
	}

	return FALSE;
}

/** Uses the key-specific data object from PreCreateKeyframe to initialize the newly added key.
*
* @param	Track		Pointer to the currently selected track.
* @param	KeyIndex	The index of the keyframe that as just added.  This is the index returned by AddKeyframe.
*/
void  UInterpTrackEventHelper::PostCreateKeyframe( UInterpTrack *Track, INT KeyIndex ) const
{
	UInterpTrackEvent	*EventTrack = CastChecked<UInterpTrackEvent>(Track);
	FEventTrackKey& NewEventKey = EventTrack->EventTrack( KeyIndex );
	NewEventKey.EventName = KeyframeAddDataName;

	// Now ensure all Matinee actions (including 'Interp') are updated with a new connector to match the data.
	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
	check(mode != NULL);
	check(mode->InterpEd != NULL);

	if ( (mode != NULL) && (mode->InterpEd != NULL) )
	{
		mode->InterpEd->UpdateMatineeActionConnectors( );	
	}
	else
	{
	}

	KeyframeAddDataName = NAME_None;
}

IMPLEMENT_CLASS(UInterpTrackEventHelper);

/** Checks track-dependent criteria prior to adding a new track.
* Responsible for any message-boxes or dialogs for selecting track-specific parameters.
* Called on default object.
*
* @param Group The group that this track is being added to
* @param Trackdef Pointer to default object for this UInterpTrackClass.
* @param bDuplicatingTrack Whether we are duplicating this track or creating a new one from scratch.
* @param bAllowPrompts When TRUE, we'll prompt for more information from the user with a dialog box if we need to
* @return Returns true if this track can be created and false if some criteria is not met (i.e. A named property is already controlled for this group).
*/
UBOOL UInterpTrackNotifyHelper::PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, UBOOL bDuplicatingTrack, UBOOL bAllowPrompts ) const
{
	// For Notify tracks - pop up a dialog to choose slot name.
	AnimSlotName = NAME_None;

	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
	check(mode != NULL);

	WxInterpEd* InterpEd = mode->InterpEd;
	check(InterpEd != NULL);

	UInterpGroupInst* GrInst = InterpEd->Interp->FindFirstGroupInst(Group);
	check(GrInst);

	AActor* Actor = GrInst->GetGroupActor();

	if (Actor != NULL)
	{
		// If the group doesn't have an AnimControlTrack, then init anim control now
		// We need that before calling GetAnimControlSlotDesc
		if(!Group->HasAnimControlTrack())
		{
			Actor->PreviewBeginAnimControl(Group);
		}

		if(bAllowPrompts)
		{
			TArray<FAnimSlotDesc> SlotDescs;
			Actor->GetAnimControlSlotDesc(SlotDescs);

			// If we get no information - just allow it to be created with empty slot
			if(SlotDescs.Num() == 0)
			{
				return TRUE;
			}

			// Build array of slot names. The slot name is used for the track's ParentNodeName property.
			TArray<FString> SlotStrings;

			for(INT i=0; i<SlotDescs.Num(); i++)
			{
				SlotStrings.AddItem(*SlotDescs(i).SlotName.ToString());
			}

			// If no slots free fail to create track
			if(SlotStrings.Num() == 0)
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("Error_NoAnimChannelsLeft") );
				return FALSE;
			}

			// Pop up a combo box to allow selection of a slot name to use for the ParentNodeName property
			WxDlgGenericComboEntry dlg;
			if(dlg.ShowModal( TEXT("ChooseParentNode"), TEXT("ParentNode"), SlotStrings, 0, TRUE ) == wxID_OK)
			{
				AnimSlotName = FName(*dlg.GetSelectedString());
				return AnimSlotName != NAME_None;
			}
		}
		else
		{
			// Prompts aren't allowed, so just succeed with defaults
			return TRUE;
		}
	}

	return FALSE;
}

/** Uses the track-specific data object from PreCreateTrack to initialize the newly added Track.
* @param Track				Pointer to the track that was just created.
* @param bDuplicatingTrack	Whether we are duplicating this track or creating a new one from scratch.
* @param TrackIndex			The index of the Track that as just added.  This is the index returned by InterpTracks.AddItem.
*/
void UInterpTrackNotifyHelper::PostCreateTrack( UInterpTrack *Track, UBOOL bDuplicatingTrack, INT TrackIndex ) const
{
	UInterpTrackNotify* NotifyTrack = CastChecked<UInterpTrackNotify>(Track);
	NotifyTrack->ParentNodeName = AnimSlotName;

	// When you change the ParentNodeName, change the TrackTitle to reflect that.
	UInterpTrackNotify* DefNotifyTrack = CastChecked<UInterpTrackNotify>(NotifyTrack->GetClass()->GetDefaultObject());
	FString DefaultTrackTitle = DefNotifyTrack->TrackTitle;

	if(AnimSlotName == NAME_None)
	{
		NotifyTrack->TrackTitle = DefaultTrackTitle;
	}
	else
	{
		NotifyTrack->TrackTitle = FString::Printf(TEXT("%s:%s"), *DefaultTrackTitle, *AnimSlotName.ToString());
	}
}

/** Checks track-dependent criteria prior to adding a new keyframe.
* Responsible for any message-boxes or dialogs for selecting key-specific parameters.
* Optionally creates/references a key-specific data object to be used in PostCreateKeyframe.
*
* @param	Track	Pointer to the currently selected track.
* @param	KeyTime	The time that this Key becomes active.
* @return	Returns TRUE if this key can be created and FALSE if some criteria is not met (i.e. No related item selected in browser).
*/
UBOOL UInterpTrackNotifyHelper::PreCreateKeyframe( UInterpTrack *Track, FLOAT KeyTime ) const
{
	KeyframeNotifyClass = NULL;

	TMap<FString, UClass*> NotifyClassMap;
	TArray<FString> NotifyStrings;

	// Build a list of all the possible AnimNotify classes
	for(TObjectIterator<UClass> It ; It ; ++It)
	{
		UClass* CheckClass = *It;
		UBOOL bChildOfObjectClass = CheckClass->IsChildOf(UAnimNotify::StaticClass());
		UBOOL bClassAllowed = !CheckClass->HasAnyClassFlags(CLASS_Hidden|CLASS_HideDropDown|CLASS_Deprecated|CLASS_Abstract);

		if (bChildOfObjectClass && bClassAllowed)
		{
			// Add the actual class object to the map referenced by the name of the class
			NotifyClassMap.Set(CheckClass->GetName(), CheckClass);

			// Add the name of the class to an array to populate the combo box
			NotifyStrings.AddItem(CheckClass->GetName());
		}
	}

	if (NotifyStrings.Num() > 0)
	{
		// Pop up a combo box to let them pick which class of AnimNotify to use
		WxDlgGenericComboEntry dlg;
		if(dlg.ShowModal(TEXT("ChooseNotifyType"), TEXT("NotifyType"), NotifyStrings, 0, TRUE) == wxID_OK)
		{
			KeyframeNotifyClass = *NotifyClassMap.Find(*dlg.GetSelectedString());
			return TRUE;
		}
	}

	return FALSE;
}

/** Uses the key-specific data object from PreCreateKeyframe to initialize the newly added key.
*
* @param	Track		Pointer to the currently selected track.
* @param	KeyIndex	The index of the keyframe that as just added.  This is the index returned by AddKeyframe.
*/
void UInterpTrackNotifyHelper::PostCreateKeyframe( UInterpTrack *Track, INT KeyIndex ) const
{
	if (KeyframeNotifyClass)
	{
		UInterpTrackNotify* NotifyTrack = CastChecked<UInterpTrackNotify>(Track);

		if(NotifyTrack)
		{
			FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
			check(mode != NULL);

			WxInterpEd* InterpEd = mode->InterpEd;
			check(InterpEd != NULL);

			// Get the newly created key and build a Notify using the track's OuterSequence as the Outer
			FNotifyTrackKey& NewNotifyKey = NotifyTrack->NotifyTrack(KeyIndex);
			NewNotifyKey.Notify = CastChecked<UAnimNotify>(GEngine->StaticConstructObject(KeyframeNotifyClass, NotifyTrack->OuterSequence));

			// Pop up a property window to edit the notify for this key
			WxPropertyWindowFrame* pw = new WxPropertyWindowFrame;
			pw->Create(GApp->EditorFrame, -1, InterpEd);
			pw->SetObject(NewNotifyKey.Notify, EPropertyWindowFlags::Sorted);

			// Tell this property window to execute ACTOR DESELECT when the escape key is pressed
			pw->SetFlags(EPropertyWindowFlags::ExecDeselectOnEscape, TRUE);

			pw->Show();
			pw->Raise();
		}
	}

	KeyframeNotifyClass = NULL;
}

IMPLEMENT_CLASS(UInterpTrackNotifyHelper);

/** Checks track-dependent criteria prior to adding a new keyframe.
* Responsible for any message-boxes or dialogs for selecting key-specific parameters.
* Optionally creates/references a key-specific data object to be used in PostCreateKeyframe.
*
* @param	Track	Pointer to the currently selected track.
* @param	KeyTime	The time that this Key becomes active.
* @return	Returns TRUE if this key can be created and FALSE if some criteria is not met (i.e. No related item selected in browser).
*/
UBOOL UInterpTrackSoundHelper::PreCreateKeyframe( UInterpTrack *Track, FLOAT KeyTime ) const
{
	GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );
	KeyframeAddSoundCue = GEditor->GetSelectedObjects()->GetTop<USoundCue>();
	if ( KeyframeAddSoundCue )
	{
		return TRUE;
	}

	appMsgf( AMT_OK, *LocalizeUnrealEd("NoSoundCueSelected") );
	return FALSE;
}

/** Uses the key-specific data object from PreCreateKeyframe to initialize the newly added key.
*
* @param	Track		Pointer to the currently selected track.
* @param	KeyIndex	The index of the keyframe that as just added.  This is the index returned by AddKeyframe.
*/
void  UInterpTrackSoundHelper::PostCreateKeyframe( UInterpTrack *Track, INT KeyIndex ) const
{
	UInterpTrackSound	*SoundTrack = CastChecked<UInterpTrackSound>(Track);

	// Assign the chosen SoundCue to the new key.
	FSoundTrackKey& NewSoundKey = SoundTrack->Sounds( KeyIndex );
	NewSoundKey.Sound = KeyframeAddSoundCue;
	KeyframeAddSoundCue = NULL;
}

IMPLEMENT_CLASS(UInterpTrackSoundHelper);

/** Checks track-dependent criteria prior to adding a new track.
 * Responsible for any message-boxes or dialogs for selecting track-specific parameters.
 * Called on default object.
 *
 * @param Group The group that this track is being added to
 * @param	Trackdef Pointer to default object for this UInterpTrackClass.
 * @param	bDuplicatingTrack Whether we are duplicating this track or creating a new one from scratch.
 * @param bAllowPrompts When TRUE, we'll prompt for more information from the user with a dialog box if we need to
 * @return Returns TRUE if this track can be created and FALSE if some criteria is not met (i.e. A named property is already controlled for this group).
 */
UBOOL UInterpTrackFloatPropHelper::PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, UBOOL bDuplicatingTrack, UBOOL bAllowPrompts ) const
{
	if( bAllowPrompts && bDuplicatingTrack == FALSE )
	{
		// For Property tracks - pop up a dialog to choose property name.
		TrackAddPropName = NAME_None;

		FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
		check(mode != NULL);

		WxInterpEd	*InterpEd = mode->InterpEd;
		check(InterpEd != NULL);

		UInterpGroupInst* GrInst = InterpEd->Interp->FindFirstGroupInst(Group);
		check(GrInst);

		AActor* Actor = GrInst->GetGroupActor();
		if ( Actor != NULL )
		{
			TArray<FName> PropNames;
			Actor->GetInterpFloatPropertyNames(PropNames);

			TArray<FString> PropStrings;
			PropStrings.AddZeroed( PropNames.Num() );
			for(INT i=0; i<PropNames.Num(); i++)
			{
				PropStrings(i) = PropNames(i).ToString();
			}

			WxDlgGenericComboEntry dlg;
			if( dlg.ShowModal( TEXT("ChooseProperty"), TEXT("PropertyName"), PropStrings, 0, TRUE ) == wxID_OK )
			{
				TrackAddPropName = FName( *dlg.GetSelectedString() );
				if ( TrackAddPropName != NAME_None )
				{
					// Check we don't already have a track controlling this property.
					for(INT i=0; i<Group->InterpTracks.Num(); i++)
					{
						FName PropertyName;
						UInterpTrack* InterpTrack = Group->InterpTracks(i);
						if ( InterpTrack && InterpTrack->GetPropertyName( PropertyName ) && PropertyName == TrackAddPropName )
						{
							appMsgf(AMT_OK, *LocalizeUnrealEd("Error_PropertyAlreadyControlled"));
							return FALSE;
						}
					}

					return TRUE;
				}
			}
		}

		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

/** Uses the track-specific data object from PreCreateTrack to initialize the newly added Track.
 * @param Track				Pointer to the track that was just created.
 * @param bDuplicatingTrack	Whether we are duplicating this track or creating a new one from scratch.
 * @param TrackIndex			The index of the Track that as just added.  This is the index returned by InterpTracks.AddItem.
 */
void  UInterpTrackFloatPropHelper::PostCreateTrack( UInterpTrack *Track, UBOOL bDuplicatingTrack, INT TrackIndex ) const
{
	if(bDuplicatingTrack == FALSE)
	{
		UInterpTrackFloatProp	*PropTrack = CastChecked<UInterpTrackFloatProp>(Track);

		// Set track title to property name (cut off component name if there is one).
		FString PropString = TrackAddPropName.ToString();
		INT PeriodPos = PropString.InStr(TEXT("."));
		if(PeriodPos != INDEX_NONE)
		{
			PropString = PropString.Mid(PeriodPos+1);
		}

		PropTrack->PropertyName = TrackAddPropName;
		PropTrack->TrackTitle = *PropString;

		TrackAddPropName = NAME_None;
	}
}

IMPLEMENT_CLASS(UInterpTrackFloatPropHelper);



/*-----------------------------------------------------------------------------
	UInterpTrackBoolPropHelper
-----------------------------------------------------------------------------*/

/** 
 * Checks track-dependent criteria prior to adding a new track.
 * Responsible for any message-boxes or dialogs for selecting track-specific parameters.
 * Called on default object.
 *
 * @param   Group               The group that this track is being added to
 * @param	Trackdef            Pointer to default object for this UInterpTrackClass.
 * @param	bDuplicatingTrack   Whether we are duplicating this track or creating a new one from scratch.
 * @param   bAllowPrompts	    When TRUE, we'll prompt for more information from the user with a dialog box if we need to.
 * 
 * @return  TRUE if this track can be created; FALSE if some criteria is not met (i.e. A named property is already controlled for this group).
 */
UBOOL UInterpTrackBoolPropHelper::PreCreateTrack( UInterpGroup* Group, const UInterpTrack* TrackDef, UBOOL bDuplicatingTrack, UBOOL bAllowPrompts ) const
{
	if( bAllowPrompts && bDuplicatingTrack == FALSE )
	{
		// For Property tracks - pop up a dialog to choose property name.
		TrackAddPropName = NAME_None;

		FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
		check(mode != NULL);

		WxInterpEd	*InterpEd = mode->InterpEd;
		check(InterpEd != NULL);

		UInterpGroupInst* GrInst = InterpEd->Interp->FindFirstGroupInst(Group);
		check(GrInst);

		AActor* Actor = GrInst->GetGroupActor();
		if ( Actor != NULL )
		{
			TArray<FName> PropNames;
			Actor->GetInterpBoolPropertyNames(PropNames);

			TArray<FString> PropStrings;
			PropStrings.AddZeroed( PropNames.Num() );
			for(INT i=0; i<PropNames.Num(); i++)
			{
				PropStrings(i) = PropNames(i).ToString();
			}

			WxDlgGenericComboEntry dlg;
			if( dlg.ShowModal( TEXT("ChooseProperty"), TEXT("PropertyName"), PropStrings, 0, TRUE ) == wxID_OK )
			{
				TrackAddPropName = FName( *dlg.GetSelectedString() );
				if ( TrackAddPropName != NAME_None )
				{
					// Check we don't already have a track controlling this property.
					for(INT i=0; i<Group->InterpTracks.Num(); i++)
					{
						FName PropertyName;
						UInterpTrack* InterpTrack = Group->InterpTracks(i);
						if ( InterpTrack && InterpTrack->GetPropertyName( PropertyName ) && PropertyName == TrackAddPropName )
						{
							appMsgf(AMT_OK, *LocalizeUnrealEd("Error_PropertyAlreadyControlled"));
							return FALSE;
						}
					}

					return TRUE;
				}
			}
		}

		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

/**
 * Uses the track-specific data object from PreCreateTrack to initialize the newly added Track.
 * 
 * @param Track				Pointer to the track that was just created.
 * @param bDuplicatingTrack	Whether we are duplicating this track or creating a new one from scratch.
 * @param TrackIndex		The index of the Track that as just added.  This is the index returned by InterpTracks.AddItem.
 */
void UInterpTrackBoolPropHelper::PostCreateTrack( UInterpTrack* Track, UBOOL bDuplicatingTrack, INT TrackIndex ) const
{
	if(bDuplicatingTrack == FALSE)
	{
		UInterpTrackBoolProp* PropTrack = CastChecked<UInterpTrackBoolProp>(Track);

		// Set track title to property name (cut off component name if there is one).
		FString PropString = TrackAddPropName.ToString();
		INT PeriodPos = PropString.InStr(TEXT("."));
		if(PeriodPos != INDEX_NONE)
		{
			PropString = PropString.Mid(PeriodPos+1);
		}

		PropTrack->PropertyName = TrackAddPropName;
		PropTrack->TrackTitle = *PropString;

		TrackAddPropName = NAME_None;
	}
}

IMPLEMENT_CLASS(UInterpTrackBoolPropHelper);

//////////////////////////////////////////////////////////////////////////
// UInterpTrackToggleHelper
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_CLASS(UInterpTrackToggleHelper);

/** 
 * Checks track-dependent criteria prior to adding a new keyframe.
 * Responsible for any message-boxes or dialogs for selecting key-specific parameters.
 * Optionally creates/references a key-specific data object to be used in PostCreateKeyframe.
 *
 * @param Track		Pointer to the currently selected track.
 * @param KeyTime	The time that this Key becomes active.
 * @return			Returns TRUE if this key can be created and FALSE if some 
 *					criteria is not met (i.e. No related item selected in browser).
 */
UBOOL UInterpTrackToggleHelper::PreCreateKeyframe( UInterpTrack *Track, FLOAT KeyTime ) const
{
	UBOOL bResult = FALSE;

	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
	check(mode != NULL);

	WxInterpEd	*InterpEd = mode->InterpEd;
	check(InterpEd != NULL);

	TArray<FString> PropStrings;
	PropStrings.AddZeroed( 3 );
	PropStrings(0) = TEXT("Trigger");
	PropStrings(1) = TEXT("On");
	PropStrings(2) = TEXT("Off");

	WxDlgGenericComboEntry dlg;
	if( dlg.ShowModal( TEXT("ChooseToggleAction"), TEXT("ToggleAction"), PropStrings, 0, TRUE ) == wxID_OK )
	{
		KeyframeAddDataName = FName(*dlg.GetSelectedString());
		bResult = TRUE;
	}

	return bResult;
}

/**
 * Uses the key-specific data object from PreCreateKeyframe to initialize the newly added key.
 *
 * @param Track		Pointer to the currently selected track.
 * @param KeyIndex	The index of the keyframe that as just added.  This is the index returned by AddKeyframe.
 */
void  UInterpTrackToggleHelper::PostCreateKeyframe( UInterpTrack *Track, INT KeyIndex ) const
{
	UInterpTrackToggle* ToggleTrack = CastChecked<UInterpTrackToggle>(Track);

	FToggleTrackKey& NewToggleKey = ToggleTrack->ToggleTrack(KeyIndex);
	if (KeyframeAddDataName == FName(TEXT("On")))
	{
		NewToggleKey.ToggleAction = ETTA_On;
	}
	else
	if (KeyframeAddDataName == FName(TEXT("Trigger")))
	{
		NewToggleKey.ToggleAction = ETTA_Trigger;
	}
	else
	{
		NewToggleKey.ToggleAction = ETTA_Off;
	}

	KeyframeAddDataName = NAME_None;
}

//////////////////////////////////////////////////////////////////////////
// UInterpTrackVectorPropHelper
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_CLASS(UInterpTrackVectorPropHelper);

UBOOL UInterpTrackVectorPropHelper::ChooseProperty(TArray<FName> &PropNames) const
{
	UBOOL bResult = FALSE;

	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
	check(mode != NULL);

	WxInterpEd	*InterpEd = mode->InterpEd;
	check(InterpEd != NULL);
	check( InterpEd->GetSelectedGroupCount() == 1 );

	UInterpGroup* Group = *(InterpEd->GetSelectedGroupIterator());

	TArray<FString> PropStrings;
	PropStrings.AddZeroed( PropNames.Num() );
	for(INT i=0; i<PropNames.Num(); i++)
	{
		PropStrings(i) = PropNames(i).ToString();
	}

	WxDlgGenericComboEntry dlg;
	if( dlg.ShowModal( TEXT("ChooseProperty"), TEXT("PropertyName"), PropStrings, 0, TRUE ) == wxID_OK )
	{
		TrackAddPropName = FName( *dlg.GetSelectedString() );
		if ( TrackAddPropName != NAME_None )
		{
			bResult = TRUE;

			// Check we don't already have a track controlling this property.
			for(INT i=0; i<Group->InterpTracks.Num(); i++)
			{
				FName PropertyName;
				UInterpTrack* InterpTrack = Group->InterpTracks(i);
				if ( InterpTrack && InterpTrack->GetPropertyName( PropertyName ) && PropertyName == TrackAddPropName )
				{
					appMsgf(AMT_OK, *LocalizeUnrealEd("Error_PropertyAlreadyControlled"));
					return FALSE;
				}
			}
		}
	}

	return bResult;
}

/** Checks track-dependent criteria prior to adding a new track.
 * Responsible for any message-boxes or dialogs for selecting track-specific parameters.
 * Called on default object.
 *
 * @param Group The group that this track is being added to
 * @param	Trackdef Pointer to default object for this UInterpTrackClass.
 * @param	bDuplicatingTrack Whether we are duplicating this track or creating a new one from scratch.
 * @param bAllowPrompts When TRUE, we'll prompt for more information from the user with a dialog box if we need to
 * @return Returns TRUE if this track can be created and FALSE if some criteria is not met (i.e. A named property is already controlled for this group).
 */
UBOOL UInterpTrackVectorPropHelper::PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, UBOOL bDuplicatingTrack, UBOOL bAllowPrompts ) const
{
	UBOOL bResult = TRUE;

	if( bAllowPrompts && bDuplicatingTrack == FALSE )
	{
		bResult = FALSE;

		// For Property tracks - pop up a dialog to choose property name.
		TrackAddPropName = NAME_None;

		FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
		check(mode != NULL);

		WxInterpEd	*InterpEd = mode->InterpEd;
		check(InterpEd != NULL);

		UInterpGroupInst* GrInst = InterpEd->Interp->FindFirstGroupInst(Group);
		check(GrInst);

		AActor* Actor = GrInst->GetGroupActor();
		if ( Actor != NULL )
		{
			TArray<FName> PropNames;
			Actor->GetInterpVectorPropertyNames(PropNames);
			bResult = ChooseProperty(PropNames);
		}
	}

	return bResult;
}

/** Uses the track-specific data object from PreCreateTrack to initialize the newly added Track.
 * @param Track				Pointer to the track that was just created.
 * @param bDuplicatingTrack	Whether we are duplicating this track or creating a new one from scratch.
 * @param TrackIndex			The index of the Track that as just added.  This is the index returned by InterpTracks.AddItem.
 */
void  UInterpTrackVectorPropHelper::PostCreateTrack( UInterpTrack *Track, UBOOL bDuplicatingTrack, INT TrackIndex ) const
{
	if(bDuplicatingTrack == FALSE)
	{
		UInterpTrackVectorProp	*PropTrack = CastChecked<UInterpTrackVectorProp>(Track);

		// Set track title to property name (cut off component name if there is one).
		FString PropString = TrackAddPropName.ToString();
		INT PeriodPos = PropString.InStr(TEXT("."));
		if(PeriodPos != INDEX_NONE)
		{
			PropString = PropString.Mid(PeriodPos+1);
		}

		PropTrack->PropertyName = TrackAddPropName;
		PropTrack->TrackTitle = *PropString;
		
		TrackAddPropName = NAME_None;
	}
}

//////////////////////////////////////////////////////////////////////////
// UInterpTrackColorPropHelper
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_CLASS(UInterpTrackColorPropHelper);

/** Checks track-dependent criteria prior to adding a new track.
 * Responsible for any message-boxes or dialogs for selecting track-specific parameters.
 * Called on default object.
 *
 * @param Group The group that this track is being added to
 * @param	Trackdef Pointer to default object for this UInterpTrackClass.
 * @param	bDuplicatingTrack Whether we are duplicating this track or creating a new one from scratch.
 * @param bAllowPrompts When TRUE, we'll prompt for more information from the user with a dialog box if we need to
 * @return Returns TRUE if this track can be created and FALSE if some criteria is not met (i.e. A named property is already controlled for this group).
 */
UBOOL UInterpTrackColorPropHelper::PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, UBOOL bDuplicatingTrack, UBOOL bAllowPrompts ) const
{
	UBOOL bResult = TRUE;

	if( bAllowPrompts && bDuplicatingTrack == FALSE )
	{
		bResult = FALSE;

		// For Property tracks - pop up a dialog to choose property name.
		TrackAddPropName = NAME_None;

		FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
		check(mode != NULL);

		WxInterpEd	*InterpEd = mode->InterpEd;
		check(InterpEd != NULL);

		UInterpGroupInst* GrInst = InterpEd->Interp->FindFirstGroupInst(Group);
		check(GrInst);

		AActor* Actor = GrInst->GetGroupActor();
		if ( Actor != NULL )
		{
			TArray<FName> PropNames;
			Actor->GetInterpColorPropertyNames(PropNames);
			bResult = ChooseProperty(PropNames);
		}
	}

	return bResult;
}

/** Uses the track-specific data object from PreCreateTrack to initialize the newly added Track.
 * @param Track				Pointer to the track that was just created.
 * @param bDuplicatingTrack	Whether we are duplicating this track or creating a new one from scratch.
 * @param TrackIndex			The index of the Track that as just added.  This is the index returned by InterpTracks.AddItem.
 */
void  UInterpTrackColorPropHelper::PostCreateTrack( UInterpTrack *Track, UBOOL bDuplicatingTrack, INT TrackIndex ) const
{
	if(bDuplicatingTrack == FALSE)
	{
		UInterpTrackColorProp	*PropTrack = CastChecked<UInterpTrackColorProp>(Track);

		// Set track title to property name (cut off component name if there is one).
		FString PropString = TrackAddPropName.ToString();
		INT PeriodPos = PropString.InStr(TEXT("."));
		if(PeriodPos != INDEX_NONE)
		{
			PropString = PropString.Mid(PeriodPos+1);
		}

		PropTrack->PropertyName = TrackAddPropName;
		PropTrack->TrackTitle = *PropString;
		
		TrackAddPropName = NAME_None;
	}
}



//////////////////////////////////////////////////////////////////////////
// UInterpTrackColorPropHelper
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_CLASS(UInterpTrackLinearColorPropHelper);

/** Checks track-dependent criteria prior to adding a new track.
 * Responsible for any message-boxes or dialogs for selecting track-specific parameters.
 * Called on default object.
 *
 * @param Group The group that this track is being added to
 * @param	Trackdef Pointer to default object for this UInterpTrackClass.
 * @param	bDuplicatingTrack Whether we are duplicating this track or creating a new one from scratch.
 * @param bAllowPrompts When TRUE, we'll prompt for more information from the user with a dialog box if we need to
 * @return Returns TRUE if this track can be created and FALSE if some criteria is not met (i.e. A named property is already controlled for this group).
 */
UBOOL UInterpTrackLinearColorPropHelper::PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, UBOOL bDuplicatingTrack, UBOOL bAllowPrompts ) const
{
	UBOOL bResult = TRUE;

	if( bAllowPrompts && bDuplicatingTrack == FALSE )
	{
		bResult = FALSE;

		// For Property tracks - pop up a dialog to choose property name.
		TrackAddPropName = NAME_None;

		FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
		check(mode != NULL);

		WxInterpEd	*InterpEd = mode->InterpEd;
		check(InterpEd != NULL);

		UInterpGroupInst* GrInst = InterpEd->Interp->FindFirstGroupInst(Group);
		check(GrInst);

		AActor* Actor = GrInst->GetGroupActor();
		if ( Actor != NULL )
		{
			TArray<FName> PropNames;
			Actor->GetInterpLinearColorPropertyNames(PropNames);
			bResult = ChooseProperty(PropNames);
		}
	}

	return bResult;
}

/** Uses the track-specific data object from PreCreateTrack to initialize the newly added Track.
 * @param Track				Pointer to the track that was just created.
 * @param bDuplicatingTrack	Whether we are duplicating this track or creating a new one from scratch.
 * @param TrackIndex			The index of the Track that as just added.  This is the index returned by InterpTracks.AddItem.
 */
void  UInterpTrackLinearColorPropHelper::PostCreateTrack( UInterpTrack *Track, UBOOL bDuplicatingTrack, INT TrackIndex ) const
{
	if(bDuplicatingTrack == FALSE)
	{
		UInterpTrackLinearColorProp	*PropTrack = CastChecked<UInterpTrackLinearColorProp>(Track);

		// Set track title to property name (cut off component name if there is one).
		FString PropString = TrackAddPropName.ToString();
		INT PeriodPos = PropString.InStr(TEXT("."));
		if(PeriodPos != INDEX_NONE)
		{
			PropString = PropString.Mid(PeriodPos+1);
		}

		PropTrack->PropertyName = TrackAddPropName;
		PropTrack->TrackTitle = *PropString;

		TrackAddPropName = NAME_None;
	}
}



//////////////////////////////////////////////////////////////////////////
// UInterpTrackFaceFXHelper
//////////////////////////////////////////////////////////////////////////

/** Offer choice of FaceFX sequences. */
UBOOL UInterpTrackFaceFXHelper::PreCreateKeyframe( UInterpTrack *Track, FLOAT fTime ) const
{
	FaceFXGroupName = FString(TEXT(""));
	FaceFXAnimName = FString(TEXT(""));

	// Get the Actor for the active Group - we need this to get a list of FaceFX animations to choose from.
	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
	check(mode != NULL);

	WxInterpEd	*InterpEd = mode->InterpEd;
	check(InterpEd != NULL);

	check( InterpEd->GetSelectedTrackCount() == 1 );
	UInterpGroupInst* GrInst = InterpEd->Interp->FindFirstGroupInst( InterpEd->GetSelectedTrackIterator().GetGroup() );
	check(GrInst);

	AActor* Actor = GrInst->GetGroupActor();

	// If no Actor, warn and fail
	if(!Actor)
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_NoActorForFaceFXTrack") );
		return FALSE;
	}

	UInterpTrackFaceFX* FaceFXTrack = CastChecked<UInterpTrackFaceFX>(Track);
	if(!FaceFXTrack->CachedActorFXAsset)
	{
		//appMsgf( AMT_OK, *LocalizeUnrealEd("Error_NoActorForFaceFXTrack") ); // @todo - warning here
		return FALSE;
	}

	// Get array of sequence names. Will be in form 'GroupName.SequenceName'
	TArray<FString> SeqNames;
	FaceFXTrack->CachedActorFXAsset->GetSequenceNames(TRUE, SeqNames);

	// Get from each mounted AnimSet.
	for(INT i=0; i<FaceFXTrack->FaceFXAnimSets.Num(); i++)
	{
		UFaceFXAnimSet* Set = FaceFXTrack->FaceFXAnimSets(i);
		if(Set)
		{
			Set->GetSequenceNames(SeqNames);
		}
	}

	// If we got none, warn and fail.
	if(SeqNames.Num() == 0)
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_NoFaceFXSequencesFound") );
		return FALSE;
	}

	// Show a dialog to let user pick sequence.
	WxDlgGenericComboEntry dlg;
	const INT Result = dlg.ShowModal( TEXT("SelectFaceFXAnim"), TEXT("FaceFXAnim"), SeqNames, 0, TRUE );
	if(Result == wxID_OK)
	{
		// Get full name
		FString SelectedFullName = dlg.GetSelectedString();

		// Split it on the dot
		FString GroupName, SeqName;
		if( SelectedFullName.Split(TEXT("."), &GroupName, &SeqName) )
		{
			FaceFXGroupName = GroupName;
			FaceFXAnimName = SeqName;

			return TRUE;
		}
	}

	return FALSE;
}

/** Set the group/sequence name we chose in the newly created key. */
void  UInterpTrackFaceFXHelper::PostCreateKeyframe( UInterpTrack *Track, INT KeyIndex ) const
{
	UInterpTrackFaceFX* FaceFXTrack = CastChecked<UInterpTrackFaceFX>(Track);
	FFaceFXTrackKey& NewSeqKey = FaceFXTrack->FaceFXSeqs( KeyIndex );
	NewSeqKey.FaceFXGroupName = FaceFXGroupName;
	NewSeqKey.FaceFXSeqName = FaceFXAnimName;
}

IMPLEMENT_CLASS(UInterpTrackFaceFXHelper);



//////////////////////////////////////////////////////////////////////////
// UInterpTrackVisibilityHelper
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_CLASS(UInterpTrackVisibilityHelper);

/** 
 * Checks track-dependent criteria prior to adding a new keyframe.
 * Responsible for any message-boxes or dialogs for selecting key-specific parameters.
 * Optionally creates/references a key-specific data object to be used in PostCreateKeyframe.
 *
 * @param Track		Pointer to the currently selected track.
 * @param KeyTime	The time that this Key becomes active.
 * @return			Returns TRUE if this key can be created and FALSE if some 
 *					criteria is not met (i.e. No related item selected in browser).
 */
UBOOL UInterpTrackVisibilityHelper::PreCreateKeyframe( UInterpTrack *Track, FLOAT KeyTime ) const
{
	UBOOL bResult = FALSE;

	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
	check(mode != NULL);

	WxInterpEd	*InterpEd = mode->InterpEd;
	check(InterpEd != NULL);

	TArray<FString> PropStrings;
	PropStrings.AddZeroed( 3 );
	PropStrings(0) = TEXT("Show");
	PropStrings(1) = TEXT("Hide");
	PropStrings(2) = TEXT("Toggle");

	WxDlgGenericComboEntry dlg;
	if( dlg.ShowModal( TEXT("InterpEd_VisibilityActionDialogTitle"), TEXT("InterpEd_VisibilityActionDialogText"), PropStrings, 0, TRUE ) == wxID_OK )
	{
		KeyframeAddDataName = FName(*dlg.GetSelectedString());
		bResult = TRUE;
	}

	return bResult;
}

/**
 * Uses the key-specific data object from PreCreateKeyframe to initialize the newly added key.
 *
 * @param Track		Pointer to the currently selected track.
 * @param KeyIndex	The index of the keyframe that as just added.  This is the index returned by AddKeyframe.
 */
void  UInterpTrackVisibilityHelper::PostCreateKeyframe( UInterpTrack *Track, INT KeyIndex ) const
{
	UInterpTrackVisibility* VisibilityTrack = CastChecked<UInterpTrackVisibility>(Track);

	FVisibilityTrackKey& NewVisibilityKey = VisibilityTrack->VisibilityTrack(KeyIndex);

	if (KeyframeAddDataName == FName(TEXT("Show")))
	{
		NewVisibilityKey.Action = EVTA_Show;
	}
	else
	if (KeyframeAddDataName == FName(TEXT("Toggle")))
	{
		NewVisibilityKey.Action = EVTA_Toggle;
	}
	else	// "Hide"
	{
		NewVisibilityKey.Action = EVTA_Hide;
	}


	// Default to Always firing this event.  The user can change it later by right clicking on the
	// track keys in the editor.
	NewVisibilityKey.ActiveCondition = EVTC_Always;

	KeyframeAddDataName = NAME_None;
}




//////////////////////////////////////////////////////////////////////////
// UInterpTrackHeadTracking
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_CLASS(UInterpTrackHeadTrackingHelper);

/** 
 * Checks track-dependent criteria prior to adding a new keyframe.
 * Responsible for any message-boxes or dialogs for selecting key-specific parameters.
 * Optionally creates/references a key-specific data object to be used in PostCreateKeyframe.
 *
 * @param Track		Pointer to the currently selected track.
 * @param KeyTime	The time that this Key becomes active.
 * @return			Returns TRUE if this key can be created and FALSE if some 
 *					criteria is not met (i.e. No related item selected in browser).
 */
UBOOL UInterpTrackHeadTrackingHelper::PreCreateKeyframe( UInterpTrack *Track, FLOAT KeyTime ) const
{
	UBOOL bResult = FALSE;

	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
	check(mode != NULL);

	WxInterpEd	*InterpEd = mode->InterpEd;
	check(InterpEd != NULL);

	TArray<FString> PropStrings;
	PropStrings.AddZeroed( 2 );
	PropStrings(0) = TEXT("Disable");
	PropStrings(1) = TEXT("Enable");

	WxDlgGenericComboEntry dlg;
	if( dlg.ShowModal( TEXT("InterpEd_HeadTrackingActionDialogTitle"), TEXT("InterpEd_HeadTrackingActionDialogText"), PropStrings, 0, TRUE ) == wxID_OK )
	{
		KeyframeAddDataName = FName(*dlg.GetSelectedString());
		bResult = TRUE;
	}

	return bResult;
}

/**
 * Uses the key-specific data object from PreCreateKeyframe to initialize the newly added key.
 *
 * @param Track		Pointer to the currently selected track.
 * @param KeyIndex	The index of the keyframe that as just added.  This is the index returned by AddKeyframe.
 */
void  UInterpTrackHeadTrackingHelper::PostCreateKeyframe( UInterpTrack *Track, INT KeyIndex ) const
{
	UInterpTrackHeadTracking* HeadTrackingTrack = CastChecked<UInterpTrackHeadTracking>(Track);

	FHeadTrackingKey& NewHeadTrackingKey = HeadTrackingTrack->HeadTrackingTrack(KeyIndex);

	if (KeyframeAddDataName == FName(TEXT("Enable")))
	{
		NewHeadTrackingKey.Action = EHTA_EnableHeadTracking;
	}
	else
	{
		NewHeadTrackingKey.Action = EHTA_DisableHeadTracking;
	}

	KeyframeAddDataName = NAME_None;
}

//////////////////////////////////////////////////////////////////////////
// UInterpTrackParticleReplayHelper
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_CLASS(UInterpTrackParticleReplayHelper);

/** 
 * Checks track-dependent criteria prior to adding a new keyframe.
 * Responsible for any message-boxes or dialogs for selecting key-specific parameters.
 * Optionally creates/references a key-specific data object to be used in PostCreateKeyframe.
 *
 * @param Track		Pointer to the currently selected track.
 * @param KeyTime	The time that this Key becomes active.
 * @return			Returns TRUE if this key can be created and FALSE if some 
 *					criteria is not met (i.e. No related item selected in browser).
 */
UBOOL UInterpTrackParticleReplayHelper::PreCreateKeyframe( UInterpTrack *Track, FLOAT KeyTime ) const
{
	// We don't currently need to do anything here

	// @todo: It would be nice to pop up a dialog where the user can select a clip ID number
	//        from a list of replay clips that exist in emitter actor.

	return TRUE;
	UBOOL bResult = TRUE;
}

/**
 * Uses the key-specific data object from PreCreateKeyframe to initialize the newly added key.
 *
 * @param Track		Pointer to the currently selected track.
 * @param KeyIndex	The index of the keyframe that as just added.  This is the index returned by AddKeyframe.
 */
void  UInterpTrackParticleReplayHelper::PostCreateKeyframe( UInterpTrack *Track, INT KeyIndex ) const
{
	// We don't currently need to do anything here
}
