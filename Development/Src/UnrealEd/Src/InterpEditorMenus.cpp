/*=============================================================================
	InterpEditorMenus.cpp: Interpolation editing menus
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineAnimClasses.h"
#include "InterpEditor.h"
#include "UnLinkedObjDrawUtils.h"
#include "PropertyWindow.h"
#include "DlgGenericComboEntry.h"
#include "ScopedTransaction.h"
#include "UnPackageTools.h"
#include "Kismet.h"
#include "InterpEditor.h"
#include "LevelViewportToolBar.h"

#if WITH_MANAGED_CODE
#include "MatineeDirectorWindowShared.h"
#include "MatineeRecordWindowShared.h"
#endif //WITH_MANAGED_CODE

#if WITH_FBX
#include "UnFbxExporter.h"
#include "UnFbxImporter.h"
#endif
namespace InterpEditorUtils
{
	/**
	 * Utility function for retrieving a new name from the user.
	 *
	 * @note	Any spaces in the name will be converted to underscores. 
	 *
	 * @param	InDialogTitle			The title of dialog.
	 * @param	InDialogCaption			The caption that displayed next to the text box.
	 * @param	InDefaultText			The default name to put in the text box when first showing the dialog.
	 * @param	OutName					The resulting name. It can be invalid if the name wasn't successfully retrieved from the user. 
	 *
	 * @return	TRUE if the dialog was able to successfully get a name from the user.
	 */
	UBOOL GetNewName( const FString& InDialogTitle, const FString& InDialogCaption, const FString& InDefaultText, FName& OutName, const FString* InOriginalName = NULL )
	{
		UBOOL bRetrievedName = FALSE;

		// If the calling code provides an original name, we don't want 
		// to localize the dialog so we can put the name in the title.
		const UBOOL bLocalizeDialog = InOriginalName ? FALSE : TRUE;

		// Decide what the title will be. If we were given the original name of the 
		// group, we want to put that in the dialog's title to help out the user.
		FString Title = InOriginalName ? FString::Printf( TEXT("%s - %s"), *InDialogTitle, *(*InOriginalName) ) : InDialogTitle;

		// Pop up dialog to enter name.
		WxDlgGenericStringEntry dlg( bLocalizeDialog );
		const INT Result = dlg.ShowModal( *Title, *InDialogCaption, *InDefaultText );

		// If the user didn't cancel, then we can process the resultant name
		if( Result == wxID_OK )
		{
			// Make sure there are no spaces!
			FString EnteredString = dlg.GetEnteredString();
			EnteredString = EnteredString.Replace(TEXT(" "),TEXT("_"));

			OutName = FName( *EnteredString );
			bRetrievedName = TRUE;
		}

		return bRetrievedName;
	}
}

///// MENU CALLBACKS

// Add a new keyframe on the selected track 
void WxInterpEd::OnMenuAddKey( wxCommandEvent& In )
{
	AddKey();
}

void WxInterpEd::OnContextNewGroup( wxCommandEvent& In )
{
	INT Id = In.GetId();

	const UBOOL bIsNewFolder = ( Id == IDM_INTERP_NEW_FOLDER );
	const UBOOL bDirGroup = ( Id == IDM_INTERP_NEW_DIRECTOR_GROUP );
	const UBOOL bDuplicateGroup = ( Id == IDM_INTERP_GROUP_DUPLICATE );
	const UBOOL bAIGroup = ( Id == IDM_INTERP_NEW_AI_GROUP );
	const UBOOL bLightingGroup = ( Id == IDM_INTERP_NEW_LIGHTING_GROUP );

	if(bDuplicateGroup && !HasAGroupSelected())
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("InterpEd_Duplicate_NoGroup") );
		return;
	}

	// This is temporary - need a unified way to associate tracks with components/actors etc... hmm..
	AActor* GroupActor = NULL;

	if(!bIsNewFolder && !bDirGroup && !bDuplicateGroup)
	{
		// find if they have any other actor they want
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			GroupActor = Actor;
			break;
		}

		// ignore any other actor unless it's pawn
		if (bAIGroup && GroupActor)
		{
			if (!GroupActor->IsA(APawn::StaticClass()))
			{
				GroupActor = NULL;
			}
		}
		else if (bLightingGroup && GroupActor)
		{
			if (!GroupActor->IsA(ALight::StaticClass()))
			{
				GroupActor = NULL;
			}
		}

		if(GroupActor)
		{
			// Check that the Outermost of both the Matinee action and the Actor are the same
			// We can't create a group for an Actor that is not in the same level as the sequence
			UObject* SequenceOutermost = Interp->GetOutermost();
			UObject* ActorOutermost = GroupActor->GetOutermost();
			if(ActorOutermost != SequenceOutermost)
			{
				appMsgf(AMT_OK, *LocalizeUnrealEd("Error_ActorNotInSequenceLevel"));
				return;
			}

			// Check we do not already have a group acting on this actor.
			for(INT i=0; i<Interp->GroupInst.Num(); i++)
			{
				if( GroupActor == Interp->GroupInst(i)->GetGroupActor() )
				{
					appMsgf(AMT_OK, *LocalizeUnrealEd("Error_GroupAlreadyAssocWithActor"));
					return;
				}
			}
		}
	}

	NewGroup( Id, GroupActor );
}

void WxInterpEd::NewGroup( INT Id, AActor* GroupActor )
{
	// Find out if we want to make a 'Director' group.
	const UBOOL bIsNewFolder = ( Id == IDM_INTERP_NEW_FOLDER );
	const UBOOL bDirGroup = ( Id == IDM_INTERP_NEW_DIRECTOR_GROUP );
	const UBOOL bDuplicateGroup = ( Id == IDM_INTERP_GROUP_DUPLICATE );
	const UBOOL bAIGroup = ( Id == IDM_INTERP_NEW_AI_GROUP );

	FName NewGroupName;
	TMap<UInterpGroup*, FName> DuplicateGroupToNameMap;
	// If not a director group - ask for a name.
	if(!bDirGroup)
	{
		FString DialogName;
		FString DefaultNewGroupName;
		switch( Id )
		{
			case IDM_INTERP_NEW_CAMERA_GROUP:
				DialogName = TEXT( "NewGroupName" );
				DefaultNewGroupName = TEXT( "NewCameraGroup" );
				break;

			case IDM_INTERP_NEW_PARTICLE_GROUP:
				DialogName = TEXT( "NewGroupName" );
				DefaultNewGroupName = TEXT( "NewParticleGroup" );
				break;

			case IDM_INTERP_NEW_SKELETAL_MESH_GROUP:
				DialogName = TEXT( "NewGroupName" );
				DefaultNewGroupName = TEXT( "NewSkeletalMeshGroup" );
				break;

			case IDM_INTERP_NEW_AI_GROUP:
				DialogName = TEXT( "NewGroupName" );
				DefaultNewGroupName = TEXT( "NewAIGroup" );
				break;

			case IDM_INTERP_NEW_LIGHTING_GROUP:
				DialogName = TEXT( "NewGroupName" );
				DefaultNewGroupName = TEXT( "NewLightingGroup" );
				break;

			case IDM_INTERP_NEW_FOLDER:
				DialogName = TEXT( "NewFolderName" );
				DefaultNewGroupName = TEXT( "NewFolder" );
				break;

			case IDM_INTERP_GROUP_DUPLICATE:
				// When duplicating, we use unlocalized text 
				// at the moment. So, the spaces are needed.
				DialogName = TEXT( "New Group Name" );
				DefaultNewGroupName = TEXT( "New Group" );
				break;

			default:
				DialogName = TEXT( "NewGroupName" );
				DefaultNewGroupName = TEXT( "NewGroup" );
				break;
		}

		// If duplicating, we need to prompt for each selected group.
		if( bDuplicateGroup )
		{
			for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
			{
				FName NewName;
				const FString GroupName = (*GroupIt)->GroupName.ToString();

				// Only duplicate the ones that the user hasn't cancelled
				if( InterpEditorUtils::GetNewName( DialogName, DialogName, GroupName, NewName, &GroupName ) )
				{
					DuplicateGroupToNameMap.Set( *GroupIt, NewName );
				}
			}

			if( DuplicateGroupToNameMap.Num() == 0 )
			{
				// If the user cancelled on each duplication, then we have to early-out of this function.
				return;
			}
		}
		// else, we just need to prompt once. 
		else
		{
			if( !InterpEditorUtils::GetNewName( DialogName, DialogName, DefaultNewGroupName, NewGroupName ) )
			{
				// If the user cancelled, then we have to early-out of this function.
				return;
			}
		}
	}

	// Create new InterpGroup.
	TArray<UInterpGroup*> NewGroups;
	// if AIGroup, ask for Stage Mark Group
	FName AIStageMarkGroup = NAME_None;
	if ( bAIGroup )
	{
		if (Interp)
		{
			// if would like to create stage mark group for this AIGroup
			if ( appMsgf(AMT_YesNo, *LocalizeUnrealEd("CreateValidStageMarkGroup")) )
			{
				// create empty group
				FString StageMarkNewGroupName = NewGroupName.GetNameString() + TEXT("_StageMark");
				UInterpGroup* NewGroup = ConstructObject<UInterpGroup>( UInterpGroup::StaticClass(), IData, NAME_None, RF_Transactional );
				NewGroup->GroupName = FName(*StageMarkNewGroupName);
				NewGroups.AddItem(NewGroup);
				AIStageMarkGroup = FName(*StageMarkNewGroupName);
			}
			else
			{
				// Make array of group names
				TArray<FString> GroupNames;
				for ( INT GroupIdx = 0; GroupIdx < Interp->GroupInst.Num(); GroupIdx++ )
				{
					// if GroupActor exists
					if( Interp->GroupInst( GroupIdx )->Group && !Interp->GroupInst( GroupIdx )->Group->bIsFolder )
					{
						GroupNames.AddItem( *(Interp->GroupInst( GroupIdx )->Group->GroupName.ToString()) );
					}
				}

				// valid group actors
				if ( GroupNames.Num() > 0 )
				{
					WxDlgGenericComboEntry	dlg;
					const INT	Result = dlg.ShowModal( TEXT("SelectStageMarkGroup"), TEXT("SelectStageMarkGroupToBasedOn"), GroupNames, 0, TRUE );
					if ( Result == wxID_OK )
					{
						AIStageMarkGroup = FName( *dlg.GetSelectedString() );
					}
				}
			}
		}

		// if no name for stage mark group, just return
		if ( AIStageMarkGroup == NAME_None)
		{
			return;
		}
	}
	
	// Begin undo transaction
	InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("NewGroup") );
	Interp->Modify();
	IData->Modify();

	if(bDirGroup)
	{
		UInterpGroup* NewDirector = ConstructObject<UInterpGroupDirector>( UInterpGroupDirector::StaticClass(), IData, NAME_None, RF_Transactional );
		NewGroups.AddItem(NewDirector);
		LockCamToGroup(NewDirector);
	}
	else if (bAIGroup)
	{
		UInterpGroupAI * NewAIGroup = ConstructObject<UInterpGroupAI>( UInterpGroupAI::StaticClass(), IData, NAME_None, RF_Transactional );
		NewAIGroup->StageMarkGroup = AIStageMarkGroup;
		NewAIGroup->GroupName = NewGroupName;
		NewGroups.AddItem(NewAIGroup);		
	}	
	else if(bDuplicateGroup)
	{
		// There should not be a director selected because there can only be one!
		check( !HasAGroupSelected(UInterpGroupDirector::StaticClass()) );

		// Duplicate each selected group.
		for( TMap<UInterpGroup*,FName>::TIterator GroupIt(DuplicateGroupToNameMap); GroupIt; ++GroupIt )
		{
			UInterpGroup* DupGroup = (UInterpGroup*)UObject::StaticDuplicateObject( GroupIt.Key(), GroupIt.Key(), IData, TEXT("None"), RF_Transactional );
			DupGroup->GroupName = GroupIt.Value();
			NewGroups.AddItem(DupGroup);
		}
	}
	else
	{
		UInterpGroup* NewGroup = ConstructObject<UInterpGroup>( UInterpGroup::StaticClass(), IData, NAME_None, RF_Transactional );
		NewGroup->GroupName = NewGroupName;
		NewGroups.AddItem(NewGroup);
	}

	IData->InterpGroups.Append(NewGroups);

	// Deselect any previous group so that we are only selecting the duplicated groups.
	DeselectAllGroups(FALSE);

	for( TArray<UInterpGroup*>::TIterator NewGroupIt(NewGroups); NewGroupIt; ++NewGroupIt )
	{
		UInterpGroup* NewGroup = *NewGroupIt;
		// since now it's multiple groups, it should find what is current iterator of new groups
		UBOOL bAIGroupIter = NewGroup->IsA(UInterpGroupAI::StaticClass());

		// All groups must have a unique name.
		NewGroup->EnsureUniqueName();

		// Randomly generate a group colour for the new group.
		NewGroup->GroupColor = FColor::MakeRandomColor();

		// Set whether this is a folder or not
		NewGroup->bIsFolder = bIsNewFolder;

		NewGroup->Modify();

		SelectGroup(NewGroup, FALSE);

		// Folders don't need a group instance
		UInterpGroupInst* NewGroupInst = NULL;
		USeqVar_Character* AIVarObj = NULL;
		if( !bIsNewFolder )
		{
			// Create new InterpGroupInst
			if(bDirGroup)
			{
				NewGroupInst = ConstructObject<UInterpGroupInstDirector>( UInterpGroupInstDirector::StaticClass(), Interp, NAME_None, RF_Transactional );
				NewGroupInst->InitGroupInst(NewGroup, NULL);
			}
			else if (bAIGroupIter)
			{
				// find kismet window this belongs to 
				USequence* RootSeq = Cast<USequence>(Interp->GetOuter());
				if (RootSeq == NULL)
				{
					RootSeq = Interp->ParentSequence;
				}
				check(RootSeq);

				// go through find the right sequence and find what is selected
				for(INT i=0; i<GApp->KismetWindows.Num(); i++)
				{
					WxKismet* const KismetWindow = GApp->KismetWindows(i);
					// Find if I belong to this Kismet window
					if ( KismetWindow->Sequence == RootSeq )
					{
						// go through selected objects
						if (KismetWindow->SelectedSeqObjs.Num() > 0)
						{
							AIVarObj = Cast<USeqVar_Character>(KismetWindow->SelectedSeqObjs(0));
							break;					
						}
					}
				}

				UInterpGroupInstAI * NewGroupInstAI = ConstructObject<UInterpGroupInstAI>( UInterpGroupInstAI::StaticClass(), Interp, NAME_None, RF_Transactional );
				NewGroupInstAI->UpdatePreviewPawnFromSeqVarCharacter(NewGroup, AIVarObj);
				NewGroupInstAI->InitGroupInst(NewGroup, NULL);
				NewGroupInst = NewGroupInstAI;
			}
			else
			{
				NewGroupInst = ConstructObject<UInterpGroupInst>( UInterpGroupInst::StaticClass(), Interp, NAME_None, RF_Transactional );
				// Initialise group instance, saving ref to actor it works on.
				NewGroupInst->InitGroupInst(NewGroup, GroupActor);
			}

			const INT NewGroupInstIndex = Interp->GroupInst.AddItem(NewGroupInst);

			NewGroupInst->Modify();
		}


		// Don't need to save state here - no tracks!

		// If a director group, create a director track for it now.
		if(bDirGroup)
		{
			UInterpTrack* NewDirTrack = ConstructObject<UInterpTrackDirector>( UInterpTrackDirector::StaticClass(), NewGroup, NAME_None, RF_Transactional );
			const INT TrackIndex = NewGroup->InterpTracks.AddItem(NewDirTrack);

			UInterpTrackInst* NewDirTrackInst = ConstructObject<UInterpTrackInstDirector>( UInterpTrackInstDirector::StaticClass(), NewGroupInst, NAME_None, RF_Transactional );
			NewGroupInst->TrackInst.AddItem(NewDirTrackInst);

			NewDirTrackInst->InitTrackInst(NewDirTrack);
			NewDirTrackInst->SaveActorState(NewDirTrack);

			// Save for undo then redo.
			NewDirTrack->Modify();
			NewDirTrackInst->Modify();

			SelectTrack( NewGroup, NewDirTrack );
		}
		// If regular track, create a new object variable connector, and variable containing selected actor if there is one.
		else
		{
			if (bAIGroupIter && AIVarObj)
			{
				// if AIVarObj exists - if not, it could be still Pawn
				Interp->InitSeqObjectForGroup(NewGroup, AIVarObj);
			}
			// Folder's don't need to be bound to actors
			else if( !bIsNewFolder )
			{
				Interp->InitGroupActorForGroup(NewGroup, GroupActor);
			}

			// For Camera or Skeletal Mesh groups, add a Movement track
			// FIXME: this doesn't work like this anymore
			// if you'd like to create multiple groups at once
			if( Id == IDM_INTERP_NEW_CAMERA_GROUP ||
				Id == IDM_INTERP_NEW_SKELETAL_MESH_GROUP ||
				bAIGroupIter )
			{
				INT NewTrackIndex = INDEX_NONE;
				AddTrackToGroup( NewGroup, UInterpTrackMove::StaticClass(), NULL, FALSE, NewTrackIndex );
			}

			// For Camera groups, add a Float Property track for FOV
			if( Id == IDM_INTERP_NEW_CAMERA_GROUP )
			{
				// Set the property name for the new track.  This is a global that will be used when setting everything up.
				SetTrackAddPropName( FName( TEXT( "FOVAngle" ) ) );

				INT NewTrackIndex = INDEX_NONE;
				UInterpTrack* NewTrack = AddTrackToGroup( NewGroup, UInterpTrackFloatProp::StaticClass(), NULL, FALSE, NewTrackIndex );
			}

			// For Lighting groups, add a Movement, Brightness, Light Color, and Radius Property track
			if( Id == IDM_INTERP_NEW_LIGHTING_GROUP )
			{
				UInterpTrack* NewMovTrack = ConstructObject<UInterpTrackMove>( UInterpTrackMove::StaticClass(), NewGroup, NAME_None, RF_Transactional );
				const INT TrackIndex = NewGroup->InterpTracks.AddItem(NewMovTrack);

				UInterpTrackInst* NewMovTrackInst = ConstructObject<UInterpTrackInstMove>( UInterpTrackInstMove::StaticClass(), NewGroupInst, NAME_None, RF_Transactional );
				NewGroupInst->TrackInst.AddItem(NewMovTrackInst);

				NewMovTrackInst->InitTrackInst(NewMovTrack);
				NewMovTrackInst->SaveActorState(NewMovTrack);

				// Save for undo then redo.
				NewMovTrack->Modify();
				NewMovTrackInst->Modify();

				INT NewTrackIndex = INDEX_NONE;

				// Set the property name for the new track.  Since this is a global we need to add the track after calling this and then
				// set the next prop name.
				SetTrackAddPropName( FName( TEXT( "Brightness" ) ) );
				UInterpTrack* NewTrackBrightness = AddTrackToGroup( NewGroup, UInterpTrackFloatProp::StaticClass(), NULL, FALSE, NewTrackIndex );

				SetTrackAddPropName( FName( TEXT( "LightColor" ) ) );
				UInterpTrack* NewTrackLightColor = AddTrackToGroup( NewGroup, UInterpTrackColorProp::StaticClass(), NULL, FALSE, NewTrackIndex );

				SetTrackAddPropName( FName( TEXT( "Radius" ) ) );
				UInterpTrack* NewTrackRadius = AddTrackToGroup( NewGroup, UInterpTrackFloatProp::StaticClass(), NULL, FALSE, NewTrackIndex );
			}

			// For Skeletal Mesh groups, add an Anim track
			if( Id == IDM_INTERP_NEW_SKELETAL_MESH_GROUP ||
				bAIGroupIter )
			{
				INT NewTrackIndex = INDEX_NONE;
				AddTrackToGroup( NewGroup, UInterpTrackAnimControl::StaticClass(), NULL, FALSE, NewTrackIndex );
			}

			// For Particle groups, add a Toggle track
			if( Id == IDM_INTERP_NEW_PARTICLE_GROUP )
			{
				INT NewTrackIndex = INDEX_NONE;
				AddTrackToGroup( NewGroup, UInterpTrackToggle::StaticClass(), NULL, FALSE, NewTrackIndex );
			}
		}


		// If we have a custom filter tab currently selected, then add the new group to that filter tab
		{
			UInterpFilter_Custom* CustomFilter = Cast< UInterpFilter_Custom >( IData->SelectedFilter );
			if( CustomFilter != NULL && IData->InterpFilters.ContainsItem( CustomFilter ) )
			{
				check( !CustomFilter->GroupsToInclude.ContainsItem( NewGroup ) );

				// Add the new group to the custom filter tab!
				CustomFilter->GroupsToInclude.AddItem( NewGroup );
			}
		}
	}


	InterpEdTrans->EndSpecial();

	// Make sure particle replay tracks have up-to-date editor-only transient state
	UpdateParticleReplayTracks();

	// Make sure the director track window is only visible if we have a director group!
	UpdateDirectorTrackWindowVisibility();

	// A new group or track may have been added, so we'll update the group list scroll bar
	UpdateTrackWindowScrollBars();

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();

	// If adding a camera- make sure its frustum colour is updated.
	UpdateCamColours();

	// Reimage actor world locations.  This must happen after the group was created.
	Interp->RecaptureActorState();

	// Refresh the associated kismet window to show the new group
	RefreshAssociatedKismetWindow();
}



void WxInterpEd::OnContextNewTrack( wxCommandEvent& In )
{
	// You can only add a new track if only one group is selected
	if( GetSelectedGroupCount() != 1 )
	{
		return;
	}

	// Find the class of the new track we want to add.
	const INT NewTrackClassIndex = In.GetId() - IDM_INTERP_NEW_TRACK_START;
	check( NewTrackClassIndex >= 0 && NewTrackClassIndex < InterpTrackClasses.Num() );

	UClass* NewInterpTrackClass = InterpTrackClasses(NewTrackClassIndex);
	check( NewInterpTrackClass->IsChildOf(UInterpTrack::StaticClass()) );

	AddTrackToSelectedGroup(NewInterpTrackClass, NULL);
}



/**
 * Called when the user selects the 'Expand All Groups' option from a menu.  Expands every group such that the
 * entire hierarchy of groups and tracks are displayed.
 */
void WxInterpEd::OnExpandAllGroups( wxCommandEvent& In )
{
	const UBOOL bWantExpand = TRUE;
	ExpandOrCollapseAllVisibleGroups( bWantExpand );
}



/**
 * Called when the user selects the 'Collapse All Groups' option from a menu.  Collapses every group in the group
 * list such that no tracks are displayed.
 */
void WxInterpEd::OnCollapseAllGroups( wxCommandEvent& In )
{
	const UBOOL bWantExpand = FALSE;
	ExpandOrCollapseAllVisibleGroups( bWantExpand );
}



/**
 * Expands or collapses all visible groups in the track editor
 *
 * @param bExpand TRUE to expand all groups, or FALSE to collapse them all
 */
void WxInterpEd::ExpandOrCollapseAllVisibleGroups( const UBOOL bExpand )
{
	// We'll keep track of whether or not something changes
	UBOOL bAnythingChanged = FALSE;

	// Iterate over each group
	for( INT CurGroupIndex = 0; CurGroupIndex < IData->InterpGroups.Num(); ++CurGroupIndex	)
	{
		UInterpGroup* CurGroup = IData->InterpGroups( CurGroupIndex );
		assert( CurGroup != NULL );
															 
		// Only expand/collapse visible groups
		const UBOOL bIsCollapsing = !bExpand;
		if( CurGroup->bVisible )
		{
			if( CurGroup->bCollapsed != bIsCollapsing )
			{
				// Expand or collapse this group!
				CurGroup->bCollapsed = bIsCollapsing;
			}
		}
	}

	if( bAnythingChanged )
	{
		// @todo: Should we re-scroll to the currently selected group if needed?

		// At least one group has been expanded or collapsed, so we need to update our scroll bar
		UpdateTrackWindowScrollBars();
	}
}


void WxInterpEd::OnMenuPlay( wxCommandEvent& In )
{
	const UBOOL bShouldLoop = ( In.GetId() == IDM_INTERP_PLAYLOOPSECTION );
	const UBOOL bPlayForward = ( In.GetId() != IDM_INTERP_PlayReverse );
	StartPlaying( bShouldLoop, bPlayForward );
}

/**
 * A set of parameters specifying how movie capture is configured.
 */
class FCreateMovieOptions
{
public:
	FCreateMovieOptions()
	:	CloseEditor(FALSE),
	    CaptureResolutionIndex(0),
		CaptureTypeIndex(0),
		CaptureResolutionFPS(30),
		Compress(FALSE),
		CinematicMode(TRUE),
		DisableMovement(TRUE),
		DisableTurning(TRUE),
		HidePlayer(TRUE),
		DisableInput(TRUE),
		HideHUD(TRUE)
	{}

	/** Whether to close the editor or not			*/
	UBOOL					CloseEditor;
	/** The capture resolution index to use			*/
	INT						CaptureResolutionIndex;
	/** The capture FPS								*/
	INT						CaptureResolutionFPS;
	/** The capture type							*/
	INT						CaptureTypeIndex;
	/** Whether to compress or not					*/
	UBOOL					Compress;	
	/** Whether to turn on cinematic mode			*/
	UBOOL					CinematicMode;
	/** Whether to disable movement					*/
	UBOOL					DisableMovement;
	/** Whether to disable turning					*/
	UBOOL					DisableTurning;
	/** Whether to hide the player					*/
	UBOOL					HidePlayer;
	/** Whether to disable input					*/
	UBOOL					DisableInput;
	/** Whether to hide the HUD						*/
	UBOOL					HideHUD;
};

/**
 * Dialog window for matinee movie capture. Values are read from the ini
 * and stored again if the user presses OK.
 */
class WxInterpEditorRecordMovie : public wxDialog
{
public:
	/**
	 * Default constructor, initializing dialog.
	 */
	WxInterpEditorRecordMovie();

	/** Checkbox for close editor setting */
	wxCheckBox*	CloseEditorCheckBox;
	
	/** Checkbox for compression */
	wxCheckBox*	CompressionCheckBox;
		
	/** Checkbox for capture resolution. */
	wxStaticText* CaptureResolutionLabel;
	wxComboBox* CaptureResolutionComboBox;

	/** Text for FPS */
	wxTextCtrl		*FPSEntry;
	wxStaticText	*FPSLabel;

	/** Radio buttons for capture type */
	wxRadioButton* AVIRadioButton;
	wxRadioButton* SSRadioButton;

	/** Checkboxes for cinematic mode */
	wxCheckBox*	CinematicModeCheckBox;
	wxCheckBox*	DisableMovementCheckBox;
	wxCheckBox*	DisableTurningCheckBox;
	wxCheckBox*	HidePlayerCheckBox;
	wxCheckBox*	DisableInputCheckBox;
	wxCheckBox*	HideHudCheckBox;

	/**
	 * Shows modal dialog populating default Options by reading from ini.
	 * The code will store the newly set options back to the ini if the 
	 * user presses OK.
	 *
	 * @param	Options		movie capture options to set.
	 */
	UBOOL ShowModal( FCreateMovieOptions& Options );
private:
	using wxDialog::ShowModal;		// Hide parent implementation
public:

	/**
	 * Route OK to wxDialog.
	 */
	void OnOK( wxCommandEvent& In ) 
	{
		wxDialog::AcceptAndClose();
	}

	void OnChangeType( wxCommandEvent& In )
	{
		//CompressionCheckBox->Enable(In.GetId() == ID_CAPTURE_AVI);
	}

	void OnChangeCinematicMode( wxCommandEvent& In )
	{
		bool isChecked = CinematicModeCheckBox->IsChecked();
		DisableMovementCheckBox->Enable(isChecked);
		DisableTurningCheckBox->Enable(isChecked);
		HidePlayerCheckBox->Enable(isChecked);
		DisableInputCheckBox->Enable(isChecked);
		HideHudCheckBox->Enable(isChecked);
	}

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(WxInterpEditorRecordMovie, wxDialog)
EVT_BUTTON( wxID_OK, WxInterpEditorRecordMovie::OnOK )
EVT_RADIOBUTTON(ID_CAPTURE_AVI, WxInterpEditorRecordMovie::OnChangeType)
EVT_RADIOBUTTON(ID_CAPTURE_SS, WxInterpEditorRecordMovie::OnChangeType)
EVT_CHECKBOX(ID_CAPTURE_CINEMATIC_MODE, WxInterpEditorRecordMovie::OnChangeCinematicMode)
END_EVENT_TABLE()

/**
 * Default constructor, creating dialog.
 */
WxInterpEditorRecordMovie::WxInterpEditorRecordMovie()
{
	SetExtraStyle(GetExtraStyle()|wxWS_EX_BLOCK_EVENTS);
	wxDialog::Create( GApp->EditorFrame, wxID_ANY, *LocalizeUnrealEd(TEXT("CO_CaptureOptions")), wxDefaultPosition, wxDefaultSize, wxCAPTION|wxDIALOG_MODAL|wxTAB_TRAVERSAL );

	wxBoxSizer* BoxSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(BoxSizer);

	FString MatineeName = TEXT("SeqAct_Interp");
	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
	if ( mode != NULL && mode->InterpEd != NULL )
	{
		MatineeName = mode->InterpEd->Interp->GetName();
	}	
	const FString StaticBoxTitle = FString::Printf( TEXT("%s %s"), *MatineeName, *LocalizeUnrealEd(TEXT("CO_CaptureOptions")) );
	
	wxStaticBox* StaticBoxSizerStatic = new wxStaticBox(this, wxID_ANY, *StaticBoxTitle);
	wxStaticBoxSizer* StaticBoxSizer = new wxStaticBoxSizer(StaticBoxSizerStatic, wxVERTICAL);
	BoxSizer->Add(StaticBoxSizer, 1, wxGROW|wxALL, 5);

	// Capture type radio buttons
	wxStaticBox* StaticBoxSizerStatic2 = new wxStaticBox(this, wxID_ANY, *LocalizeUnrealEd(TEXT("CO_CaptureType")));
	wxStaticBoxSizer* StaticBoxSizer2 = new wxStaticBoxSizer(StaticBoxSizerStatic2, wxVERTICAL);
	StaticBoxSizer->Add(StaticBoxSizer2, 0, wxGROW|wxALL, 5);	
	AVIRadioButton = new wxRadioButton();
	AVIRadioButton->Create( this, ID_CAPTURE_AVI, *LocalizeUnrealEd(TEXT("CO_AVI")), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	StaticBoxSizer2->Add(AVIRadioButton, 0, wxGROW|wxALL, 5);
	SSRadioButton = new wxRadioButton();
	SSRadioButton->Create( this, ID_CAPTURE_SS, *LocalizeUnrealEd(TEXT("CO_ScreenShots")), wxDefaultPosition, wxDefaultSize);
	StaticBoxSizer2->Add(SSRadioButton, 0, wxGROW|wxALL, 5);

	// Capture Resolution
	CaptureResolutionLabel = new wxStaticText(this, -1, *LocalizeUnrealEd("CaptureResolution"));
	StaticBoxSizer->Add(CaptureResolutionLabel, 0, wxGROW|wxALL, 1);
	CaptureResolutionComboBox = new wxComboBox(this, wxID_ANY, *LocalizeUnrealEd("CaptureResolution"), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY );
	StaticBoxSizer->Add( CaptureResolutionComboBox, 0, wxGROW|wxALL, 5 );
	CaptureResolutionComboBox->Freeze();
	CaptureResolutionComboBox->Append( TEXT("320 x 240"), (void*)NULL );
	CaptureResolutionComboBox->Append( TEXT("640 x 480"), (void*)NULL );
	CaptureResolutionComboBox->Append( TEXT("1280 x 720"), (void*)NULL );
	CaptureResolutionComboBox->Append( TEXT("1920 x 1080"), (void*)NULL );
	CaptureResolutionComboBox->Thaw();

	// FPS
	FPSLabel = new wxStaticText(this, -1, *LocalizeUnrealEd("CO_FPS"));
	StaticBoxSizer->Add(FPSLabel, 0, wxGROW|wxALL, 1);
	FPSEntry = new wxTextCtrl( this, wxID_ANY, *TEXT("30"), wxDefaultPosition, wxDefaultSize, 0, wxTextValidator(wxFILTER_NUMERIC) );
	StaticBoxSizer->Add(FPSEntry, 0, wxGROW|wxALL, 5);

	// Compression?
	//CompressionCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd(TEXT("CO_Compress")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	//CompressionCheckBox->SetValue(FALSE);
	//StaticBoxSizer->Add(CompressionCheckBox, 0, wxGROW|wxALL, 5);
	
	// Close editor?
	CloseEditorCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd(TEXT("CO_CloseEditor")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	CloseEditorCheckBox->SetValue(FALSE);
	StaticBoxSizer->Add(CloseEditorCheckBox, 0, wxGROW|wxALL, 5);

	// Cinematic Mode
	wxStaticBox* StaticBoxSizerStatic3 = new wxStaticBox(this, wxID_ANY, *LocalizeUnrealEd(TEXT("CO_CinematicModeTitle")));
	wxStaticBoxSizer* StaticBoxSizer3 = new wxStaticBoxSizer(StaticBoxSizerStatic3, wxVERTICAL);
	StaticBoxSizer->Add(StaticBoxSizer3, 0, wxGROW|wxALL, 5);	
	CinematicModeCheckBox = new wxCheckBox( this, ID_CAPTURE_CINEMATIC_MODE, *LocalizeUnrealEd(TEXT("CO_CinematicMode")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	CinematicModeCheckBox->SetValue(TRUE);
	StaticBoxSizer3->Add(CinematicModeCheckBox, 0, wxGROW|wxALL, 5);
	wxStaticBox* StaticBoxSizerStatic4 = new wxStaticBox(this, wxID_ANY, *LocalizeUnrealEd(TEXT("CO_CinematicModeSettings")));
	wxStaticBoxSizer* StaticBoxSizer4 = new wxStaticBoxSizer(StaticBoxSizerStatic4, wxVERTICAL);
	StaticBoxSizer3->Add(StaticBoxSizer4, 0, wxGROW|wxALL, 5);	
	DisableMovementCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd(TEXT("CO_CinematicModeDisableMovement")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	DisableMovementCheckBox->SetValue(TRUE);
	StaticBoxSizer4->Add(DisableMovementCheckBox, 0, wxGROW|wxALL, 5);
	DisableTurningCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd(TEXT("CO_CinematicModeDisableTurning")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	DisableTurningCheckBox->SetValue(TRUE);
	StaticBoxSizer4->Add(DisableTurningCheckBox, 0, wxGROW|wxALL, 5);
	HidePlayerCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd(TEXT("CO_CinematicModeHidePlayer")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	HidePlayerCheckBox->SetValue(TRUE);
	StaticBoxSizer4->Add(HidePlayerCheckBox, 0, wxGROW|wxALL, 5);
	DisableInputCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd(TEXT("CO_CinematicModeDisableInput")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	DisableInputCheckBox->SetValue(TRUE);
	StaticBoxSizer4->Add(DisableInputCheckBox, 0, wxGROW|wxALL, 5);
	HideHudCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd(TEXT("CO_CinematicModeHideHud")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	HideHudCheckBox->SetValue(TRUE);
	StaticBoxSizer4->Add(HideHudCheckBox, 0, wxGROW|wxALL, 5);

	// Buttons
	wxBoxSizer* ButtonBoxSizer = new wxBoxSizer(wxHORIZONTAL);
	BoxSizer->Add(ButtonBoxSizer, 0, wxGROW|wxALL, 5);
	wxButton* OKButton = new wxButton( this, wxID_OK, *LocalizeUnrealEd(TEXT("&OK")) );
	OKButton->SetDefault();
	OKButton->SetFocus();
	ButtonBoxSizer->Add(OKButton, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	wxButton* CancelButton = new wxButton( this, wxID_CANCEL, *LocalizeUnrealEd(TEXT("&Cancel")) );
	ButtonBoxSizer->Add(CancelButton, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// Fit the dialog to the window contents
	BoxSizer->Fit( this );
}

/**
 * Shows modal dialog populating default Options by reading from ini. The code will store the newly set options
 * back to the ini if the user presses OK.
 *
 * @param	Options		Lighting rebuild options to set.
 * @return				TRUE if the lighting rebuild should go ahead.
 */
UBOOL WxInterpEditorRecordMovie::ShowModal( FCreateMovieOptions& Options )
{
	// Retrieve settings from ini.
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("CloseEditor"), Options.CloseEditor, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("Compress"), Options.Compress, GEditorUserSettingsIni );
	GConfig->GetInt(  TEXT("MatineeCreateMovieOptions"), TEXT("CaptureResolutionFPS"), Options.CaptureResolutionFPS, GEditorUserSettingsIni );
	GConfig->GetInt(  TEXT("MatineeCreateMovieOptions"), TEXT("CaptureResolutionIndex"), Options.CaptureResolutionIndex, GEditorUserSettingsIni );
	GConfig->GetInt(  TEXT("MatineeCreateMovieOptions"), TEXT("CaptureTypeIndex"), Options.CaptureTypeIndex, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("CinematicMode"), Options.CinematicMode, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("DisableMovement"), Options.DisableMovement, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("DisableTurning"), Options.DisableTurning, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("HidePlayer"), Options.HidePlayer, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("DisableInput"), Options.DisableInput, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("HideHUD"), Options.HideHUD, GEditorUserSettingsIni );
	
	INT MaxEntries = CaptureResolutionComboBox->GetCount();
	Options.CaptureResolutionIndex = Clamp<INT>(Options.CaptureResolutionIndex, 0, MaxEntries - 1);
	
	CaptureResolutionComboBox->SetSelection(Options.CaptureResolutionIndex);
	CloseEditorCheckBox->SetValue( Options.CloseEditor == TRUE );
	//CompressionCheckBox->SetValue( Options.bCompress == TRUE );
	FString FPSString = FString::Printf(TEXT("%d"), Options.CaptureResolutionFPS);
	FPSEntry->SetValue(*FPSString);

	AVIRadioButton->SetValue(Options.CaptureTypeIndex == 0);
	SSRadioButton->SetValue(Options.CaptureTypeIndex == 1);

	CinematicModeCheckBox->SetValue( Options.CinematicMode == TRUE );
	DisableMovementCheckBox->SetValue( Options.DisableMovement == TRUE );
	DisableTurningCheckBox->SetValue( Options.DisableTurning == TRUE );
	HidePlayerCheckBox->SetValue( Options.HidePlayer == TRUE );
	DisableInputCheckBox->SetValue( Options.DisableInput == TRUE );
	HideHudCheckBox->SetValue( Options.HideHUD == TRUE );

	DisableMovementCheckBox->Enable(Options.CinematicMode == TRUE);
	DisableTurningCheckBox->Enable(Options.CinematicMode == TRUE);
	HidePlayerCheckBox->Enable(Options.CinematicMode == TRUE);
	DisableInputCheckBox->Enable(Options.CinematicMode == TRUE);
	HideHudCheckBox->Enable(Options.CinematicMode == TRUE);

	//CompressionCheckBox->Enable(Options.CaptureTypeIndex == 0);

	UBOOL bProceedWithBuild = TRUE;
	
	FString levelNames;
	AWorldInfo* WorldInfo = GWorld ? GWorld->GetWorldInfo() : NULL;
	if (WorldInfo)
	{
		for( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
		{
			ULevelStreaming* StreamingLevel	= WorldInfo->StreamingLevels(LevelIndex);
			if (StreamingLevel && StreamingLevel->bIsVisible)
			{
				levelNames += StreamingLevel->PackageName.ToString() + "|";
			}
		}
	}

	// Run dialog...
	if( wxDialog::ShowModal() == wxID_OK )
	{
		// ... and save options if user pressed okay.
		Options.CloseEditor = CloseEditorCheckBox->GetValue();
		Options.Compress = FALSE;//CompressionCheckBox->GetValue();
		Options.CaptureResolutionFPS = appAtof(FPSEntry->GetValue());
		Options.CaptureResolutionIndex = CaptureResolutionComboBox->GetCurrentSelection();
		if (AVIRadioButton->GetValue())
		{
			Options.CaptureTypeIndex = 0;
		}
		else
		{
			Options.CaptureTypeIndex = 1;
		}
		Options.CinematicMode = CinematicModeCheckBox->GetValue();
		Options.DisableMovement = DisableMovementCheckBox->GetValue();
		Options.DisableTurning = DisableTurningCheckBox->GetValue();
		Options.HidePlayer = HidePlayerCheckBox->GetValue();
		Options.DisableInput = DisableInputCheckBox->GetValue();
		Options.HideHUD = HideHudCheckBox->GetValue();
		
		GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("CloseEditor"), Options.CloseEditor, GEditorUserSettingsIni );
		GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("Compress"), Options.Compress, GEditorUserSettingsIni );
		GConfig->SetInt(  TEXT("MatineeCreateMovieOptions"), TEXT("CaptureResolutionFPS"), Options.CaptureResolutionFPS, GEditorUserSettingsIni );
		GConfig->SetInt(  TEXT("MatineeCreateMovieOptions"), TEXT("CaptureResolutionIndex"), Options.CaptureResolutionIndex, GEditorUserSettingsIni );
		GConfig->SetInt(  TEXT("MatineeCreateMovieOptions"), TEXT("CaptureTypeIndex"), Options.CaptureTypeIndex, GEditorUserSettingsIni );
		GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("CinematicMode"), Options.CinematicMode, GEditorUserSettingsIni );
		GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("DisableMovement"), Options.DisableMovement, GEditorUserSettingsIni );
		GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("DisableTurning"), Options.DisableTurning, GEditorUserSettingsIni );
		GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("HidePlayer"), Options.HidePlayer, GEditorUserSettingsIni );
		GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("DisableInput"), Options.DisableInput, GEditorUserSettingsIni );
		GConfig->SetBool( TEXT("MatineeCreateMovieOptions"), TEXT("HideHUD"), Options.HideHUD, GEditorUserSettingsIni );
		GConfig->Flush( FALSE, GEditorUserSettingsIni );
		
		FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );
		if ( mode != NULL && mode->InterpEd != NULL )
		{
			// Store the options for the capture of the Matinee
			GEngine->MatineeCaptureName = mode->InterpEd->Interp->GetName();
			GEngine->MatineePackageCaptureName = mode->InterpEd->Interp->ParentSequence->GetOutermost()->GetName();
			GEngine->VisibleLevelsForMatineeCapture = levelNames;

			GUnrealEd->MatineeCaptureFPS = Options.CaptureResolutionFPS;
			GUnrealEd->bCompressMatineeCapture = Options.Compress;

			FString ResX = TEXT("");
			FString ResY = TEXT("");
			FString ResString(CaptureResolutionComboBox->GetValue());

			TArray<FString> Res;
			if( ResString.ParseIntoArray( &Res, TEXT(" x "), 0 ) == 2 )
			{
				GUnrealEd->MatineeCaptureResolutionX = appAtof(Res(0).GetCharArray().GetData());
				GUnrealEd->MatineeCaptureResolutionY = appAtof(Res(1).GetCharArray().GetData());
			}
			GUnrealEd->MatineeCaptureType = Options.CaptureTypeIndex;
			mode->InterpEd->StartRecordingMovie();
			if (Options.CloseEditor)
			{			
				WxEditorFrame* Frame = static_cast<WxEditorFrame*>(GApp->EditorFrame);
				Frame->Close();
			}
		}		
	}
	else
	{
		bProceedWithBuild = FALSE;
	}

	return bProceedWithBuild;
}

void WxInterpEd::OnMenuCreateMovie(  wxCommandEvent& In )
{
	WxInterpEditorRecordMovie CreateMovieOptionsDialog;
	FCreateMovieOptions CreateMovieOptions;
	CreateMovieOptionsDialog.ShowModal( CreateMovieOptions );
}

void WxInterpEd::OnMenuStop( wxCommandEvent& In )
{
	StopPlaying();
}

void WxInterpEd::OnOpenBindKeysDialog(wxCommandEvent &In)
{
	WxDlgBindHotkeys* BindHotkeys = GApp->GetDlgBindHotkeys();
	check(BindHotkeys);
	BindHotkeys->Show(TRUE);
	BindHotkeys->SetFocus();
}

void WxInterpEd::OnChangePlaySpeed( wxCommandEvent& In )
{
	PlaybackSpeed = 1.f;

	switch( In.GetId() )
	{
	case IDM_INTERP_SPEED_1:
		PlaybackSpeed = 0.01f;
		break;
	case IDM_INTERP_SPEED_10:
		PlaybackSpeed = 0.1f;
		break;
	case IDM_INTERP_SPEED_25:
		PlaybackSpeed = 0.25f;
		break;
	case IDM_INTERP_SPEED_50:
		PlaybackSpeed = 0.5f;
		break;
	case IDM_INTERP_SPEED_100:
		PlaybackSpeed = 1.0f;
		break;
	}

	// Playback speed changed, so reset our playback start time so fixed time step playback can
	// gate frame rate properly
	PlaybackStartRealTime = appSeconds();
	NumContinuousFixedTimeStepFrames = 0;
}

void WxInterpEd::StretchSection( UBOOL bUseSelectedOnly )
{
	// Edit section markers should always be within sequence...
	FLOAT SectionStart = IData->EdSectionStart;
	FLOAT SectionEnd = IData->EdSectionEnd;

	if( bUseSelectedOnly )
	{
		// reverse the section start/end - good way to initialise the data to be written over
		SectionStart = IData->EdSectionEnd;
		SectionEnd = IData->EdSectionStart;

		if( !Opt->SelectedKeys.Num() )
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd( "InterpEd_NoKeyframesSelected" ) );
		}

		for( INT iKey = 0; iKey < Opt->SelectedKeys.Num(); iKey++ )
		{
			FInterpEdSelKey& SelKey = Opt->SelectedKeys( iKey );

			UInterpTrack* Track = SelKey.Track;
			FLOAT CurrentKeyTime = Track->GetKeyframeTime(SelKey.KeyIndex);
			if( CurrentKeyTime < SectionStart )
			{
				SectionStart = CurrentKeyTime;
			}
			if( CurrentKeyTime > SectionEnd )
			{
				SectionEnd = CurrentKeyTime;
			}
		}
	}

	const FLOAT CurrentSectionLength = SectionEnd - SectionStart;
	if(CurrentSectionLength < 0.01f)
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_HighlightNonZeroLength") );
		return;
	}

	const FString CurrentLengthStr = FString::Printf( TEXT("%3.3f"), CurrentSectionLength );

	// Display dialog and let user enter new length for this section.
	WxDlgGenericStringEntry dlg;
	const INT Result = dlg.ShowModal( TEXT("StretchSection"), TEXT("NewLength"), *CurrentLengthStr);
	if( Result != wxID_OK )
		return;

	double dNewSectionLength;
	const UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToDouble(&dNewSectionLength);
	if(!bIsNumber)
		return;

	const FLOAT NewSectionLength = (FLOAT)dNewSectionLength;
	if(NewSectionLength <= 0.f)
		return;

	InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("StretchSection") );

	IData->Modify();
	Opt->Modify();

	const FLOAT LengthDiff = NewSectionLength - CurrentSectionLength;
	const FLOAT StretchRatio = NewSectionLength/CurrentSectionLength;

	// Iterate over all tracks.
	for(INT i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups(i);
		for(INT j=0; j<Group->InterpTracks.Num(); j++)
		{
			UInterpTrack* Track = Group->InterpTracks(j);

			Track->Modify();

			for(INT k=0; k<Track->GetNumKeyframes(); k++)
			{
				const FLOAT KeyTime = Track->GetKeyframeTime(k);

				// Key is before start of stretched section
				if(KeyTime < SectionStart)
				{
					// Leave key as it is
				}
				// Key is in section being stretched
				else if(KeyTime < SectionEnd)
				{
					// Calculate new key time.
					const FLOAT FromSectionStart = KeyTime - SectionStart;
					const FLOAT NewKeyTime = SectionStart + (StretchRatio * FromSectionStart);

					Track->SetKeyframeTime(k, NewKeyTime, FALSE);
				}
				// Key is after stretched section
				else
				{
					// Move it on by the increase in sequence length.
					Track->SetKeyframeTime(k, KeyTime + LengthDiff, FALSE);
				}
			}
		}
	}

	// Move the end of the interpolation to account for changing the length of this section.
	SetInterpEnd(IData->InterpLength + LengthDiff);

	// Move end marker of section to new, stretched position.
	MoveLoopMarker( IData->EdSectionEnd + LengthDiff, FALSE );

	InterpEdTrans->EndSpecial();
}

void WxInterpEd::OnMenuStretchSection(wxCommandEvent& In)
{
	StretchSection( FALSE );
}

void WxInterpEd::OnMenuStretchSelectedKeyframes(wxCommandEvent& In)
{
	StretchSection( TRUE );
}

/** Remove the currernt section, reducing the length of the sequence and moving any keys after the section earlier in time. */
void WxInterpEd::OnMenuDeleteSection(wxCommandEvent& In)
{
	const FLOAT CurrentSectionLength = IData->EdSectionEnd - IData->EdSectionStart;
	if(CurrentSectionLength < 0.01f)
		return;

	InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("DeleteSection") );

	IData->Modify();
	Opt->Modify();

	// Add keys that are within current section to selection
	SelectKeysInLoopSection();

	// Delete current selection
	DeleteSelectedKeys(FALSE);

	// Then move any keys after the current section back by the length of the section.
	for(INT i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups(i);
		for(INT j=0; j<Group->InterpTracks.Num(); j++)
		{
			UInterpTrack* Track = Group->InterpTracks(j);
			Track->Modify();

			for(INT k=0; k<Track->GetNumKeyframes(); k++)
			{
				// Move keys after section backwards by length of the section
				FLOAT KeyTime = Track->GetKeyframeTime(k);
				if(KeyTime > IData->EdSectionEnd)
				{
					// Add to selection for deletion.
					Track->SetKeyframeTime(k, KeyTime - CurrentSectionLength, FALSE);
				}
			}
		}
	}

	// Move the end of the interpolation to account for changing the length of this section.
	SetInterpEnd(IData->InterpLength - CurrentSectionLength);

	// Move section end marker on top of section start marker (section has vanished).
	MoveLoopMarker( IData->EdSectionStart, FALSE );

	InterpEdTrans->EndSpecial();
}

/** Insert an amount of space (specified by user in dialog) at the current position in the sequence. */
void WxInterpEd::OnMenuInsertSpace( wxCommandEvent& In )
{
	WxDlgGenericStringEntry dlg;
	INT Result = dlg.ShowModal( TEXT("InsertEmptySpace"), TEXT("Seconds"), TEXT("1.0"));
	if( Result != wxID_OK )
		return;

	double dAddTime;
	UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToDouble(&dAddTime);
	if(!bIsNumber)
		return;

	FLOAT AddTime = (FLOAT)dAddTime;

	// Ignore if adding a negative amount of time!
	if(AddTime <= 0.f)
		return;

	InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("InsertSpace") );

	IData->Modify();
	Opt->Modify();

	// Move the end of the interpolation on by the amount we are adding.
	SetInterpEnd(IData->InterpLength + AddTime);

	// Iterate over all tracks.
	for(INT i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups(i);
		for(INT j=0; j<Group->InterpTracks.Num(); j++)
		{
			UInterpTrack* Track = Group->InterpTracks(j);

			Track->Modify();

			for(INT k=0; k<Track->GetNumKeyframes(); k++)
			{
				FLOAT KeyTime = Track->GetKeyframeTime(k);
				if(KeyTime > Interp->Position)
				{
					Track->SetKeyframeTime(k, KeyTime + AddTime, FALSE);
				}
			}
		}
	}

	InterpEdTrans->EndSpecial();
}

void WxInterpEd::OnMenuSelectInSection(wxCommandEvent& In)
{
	SelectKeysInLoopSection();
}

void WxInterpEd::OnMenuDuplicateSelectedKeys(wxCommandEvent& In)
{
	DuplicateSelectedKeys();
}

void WxInterpEd::OnSavePathTime( wxCommandEvent& In )
{
	IData->PathBuildTime = Interp->Position;
}

void WxInterpEd::OnJumpToPathTime( wxCommandEvent& In )
{
	SetInterpPosition(IData->PathBuildTime);
}

void WxInterpEd::OnViewHide3DTracks( wxCommandEvent& In )
{
	bHide3DTrackView = !bHide3DTrackView;
	MenuBar->ViewMenu->Check( IDM_INTERP_VIEW_Draw3DTrajectories, bHide3DTrackView == FALSE );

	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("Hide3DTracks"), bHide3DTrackView, GEditorUserSettingsIni );
}

void WxInterpEd::OnViewZoomToScrubPos( wxCommandEvent& In )
{
	bZoomToScrubPos = !bZoomToScrubPos;
	MenuBar->ViewMenu->Check( IDM_INTERP_VIEW_ZoomToTimeCursorPosition, bZoomToScrubPos == TRUE );

	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("ZoomToScrubPos"), bZoomToScrubPos, GEditorUserSettingsIni );
}

void WxInterpEd::OnEnableEditingGrid( wxCommandEvent& In )
{
	bEditingGridEnabled = !bEditingGridEnabled;
	GConfig->SetBool( TEXT("Matinee"), TEXT("EnableEditingGrid"), bEditingGridEnabled, GEditorUserSettingsIni );
}

void WxInterpEd::OnEnableEditingGridUpdateUI( wxUpdateUIEvent& In )
{
	In.Check(bEditingGridEnabled==TRUE);
}

void WxInterpEd::OnSetEditingGrid( wxCommandEvent& In )
{
	const INT Id = In.GetId();

	EditingGridSize = (In.GetId() - IDM_INTERP_VIEW_EditingGrid_Start)+1;

	GConfig->SetInt( TEXT("Matinee"), TEXT("EditingGridSize"), EditingGridSize, GEditorUserSettingsIni );
}

void WxInterpEd::OnEditingGridUpdateUI( wxUpdateUIEvent& In )
{
	if( (In.GetId()-IDM_INTERP_VIEW_EditingGrid_Start)+1 == EditingGridSize )
	{
		In.Check(TRUE);
	}
	else
	{
		In.Check(FALSE);
	}
}
void WxInterpEd::OnToggleEditingCrosshair( wxCommandEvent& In )
{
	bEditingCrosshairEnabled = !bEditingCrosshairEnabled;
	MenuBar->ViewMenu->Check( IDM_INTERP_VIEW_EditingCrosshair, bEditingCrosshairEnabled == TRUE );

	GConfig->SetBool( TEXT("Matinee"), TEXT("EditingCrosshair"), bEditingCrosshairEnabled, GEditorUserSettingsIni );
}

void WxInterpEd::OnToggleViewportFrameStats( wxCommandEvent& In )
{
	bViewportFrameStatsEnabled = !bViewportFrameStatsEnabled;
	MenuBar->ViewMenu->Check( IDM_INTERP_VIEW_ViewportFrameStats, bViewportFrameStatsEnabled == TRUE );

	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("ViewportFrameStats"), bViewportFrameStatsEnabled, GEditorUserSettingsIni );
}


/** Called when the "Toggle Gore Preview" button is pressed */
void WxInterpEd::OnToggleGorePreview( wxCommandEvent& In )
{
	Interp->bShouldShowGore = !Interp->bShouldShowGore;
}


/** Called when the "Toggle Gore Preview" UI should be updated */
void WxInterpEd::OnToggleGorePreview_UpdateUI( wxUpdateUIEvent& In )
{
	In.Check( Interp->bShouldShowGore == TRUE );
}

/** Called when the "Create Camera Actor at Current Camera Location" button is pressed */
void WxInterpEd::OnCreateCameraActorAtCurrentCameraLocation( wxCommandEvent& In )
{
	ACameraActor* NewCamera = Cast<ACameraActor>( GWorld->SpawnActor( ACameraActor::StaticClass() ) );

	// find the first perspective viewport - if one exists
	FEditorLevelViewportClient* ViewportClient = NULL;
	for( INT iView = 0; iView < GEditor->ViewportClients.Num(); iView++ )
	{
		ViewportClient = GEditor->ViewportClients( iView );
		if( ViewportClient->IsPerspective() )
		{
			break;
		}
	}

	if( ViewportClient )
	{
		NewCamera->SetLocation( ViewportClient->ViewLocation );
		NewCamera->SetRotation( ViewportClient->ViewRotation );
	}

	NewGroup( IDM_INTERP_NEW_CAMERA_GROUP, NewCamera );

	GEditor->RedrawAllViewports();
}

#define WITH_WPF_RECORDING_WINDOW 0
/** Called when the "Launch Custom Preview Viewport" is pressed */
void WxInterpEd::OnLaunchRecordingViewport ( wxCommandEvent& In )
{
	//if it's ok to make the new viewport and we don't already have a dedicated viewport
	if ( GApp && GApp->EditorFrame && GApp->EditorFrame->ViewportConfigData)
	{
#if WITH_MANAGED_CODE && WITH_WPF_RECORDING_WINDOW
		MatineeWindows::LaunchRecordWindow(this);
#else
		FViewportConfig_Data *ViewportConfig = GApp->EditorFrame->ViewportConfigData;

		// Create the new floating viewport
		FFloatingViewportParams ViewportParams;
		ViewportParams.ParentWxWindow = this;
		ViewportParams.ViewportType = ELevelViewportType::LVT_Perspective;
		ViewportParams.ShowFlags = SHOW_DefaultGame;
		ViewportParams.Width = 200;
		ViewportParams.Height = 200;
		ViewportParams.Title = LocalizeUnrealEd( "InterpEd_RecordingWindowTitle" );

		INT NewViewportIndex = INDEX_NONE;
		UBOOL bDisablePlayInViewport = TRUE;
		UBOOL bResultValue = ViewportConfig->OpenNewFloatingViewport(ViewportParams, NewViewportIndex, bDisablePlayInViewport );

		if( bResultValue )
		{
			// OK, now copy various settings from our viewport into the newly created viewport
			FVCD_Viewport& NewViewport = ViewportConfig->AccessViewport( NewViewportIndex );
			WxLevelViewportWindow* NewViewportWin = NewViewport.ViewportWindow;
			WxFloatingViewportFrame* NewViewportFrame = NewViewport.FloatingViewportFrame;
			if((NewViewportWin != NULL) && (NewViewportFrame != NULL))
			{
				const bool bMaximize = TRUE;
				NewViewportFrame->Maximize(bMaximize);
				
				NewViewportWin->SetRealtime(TRUE);
				NewViewportWin->SetAllowMatineePreview(TRUE);
				NewViewportWin->Viewport->CaptureJoystickInput(TRUE);
				NewViewportWin->SetMatineeRecordingWindow(this);

				WxLevelViewportToolBar* Toolbar = NewViewportWin->ToolBar;
				if (Toolbar)
				{
					Toolbar->AppendMatineeRecordOptions(this);
				}
			}
		}
		else
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd( "OpenNewFloatingViewport_Error" ) );
		}
#endif
	}
}

/** Called when the "Launch the Director Window" button is called*/
void WxInterpEd::OnLaunchDirectorWindow (wxCommandEvent& In)
{
#if WITH_MANAGED_CODE
	MatineeWindows::LaunchDirectorWindow(this);
#endif
}

void WxInterpEd::OnContextTrackRename( wxCommandEvent& In )
{
	if( !HasATrackSelected() )
	{
		return;
	}

	for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		UInterpTrack* Track = *TrackIt;

		FName NewName;
		if( InterpEditorUtils::GetNewName( TEXT("Rename Track"), TEXT("New Track Name"), Track->TrackTitle, NewName, &Track->TrackTitle ) )
		{
			Track->TrackTitle = NewName.ToString();

			// In case this track is being displayed on the curve editor, update its name there too.
			FString CurveName = FString::Printf( TEXT("%s_%s"), *(TrackIt.GetGroup()->GroupName.ToString()), *Track->TrackTitle);
			IData->CurveEdSetup->ChangeCurveName(Track, CurveName);
			CurveEd->CurveChanged();
		}
	}
}

void WxInterpEd::OnContextTrackDelete( wxCommandEvent& In )
{
	//stop recording
	StopRecordingInterpValues();

	DeleteSelectedTracks();
}

void WxInterpEd::OnContextTrackChangeFrame( wxCommandEvent& In  )
{
	check( HasATrackSelected() );

	TTrackClassTypeIterator<UInterpTrackMove> MoveTrackIt(GetSelectedTrackIterator<UInterpTrackMove>());
	for( ; MoveTrackIt; ++MoveTrackIt )
	{
		UInterpTrackMove* MoveTrack = *MoveTrackIt;

		// Find the frame we want to convert to.
		INT Id = In.GetId();
		BYTE DesiredFrame = 0;
		if(Id == IDM_INTERP_TRACK_FRAME_WORLD)
		{
			DesiredFrame = IMF_World;
		}
		else if(Id == IDM_INTERP_TRACK_FRAME_RELINITIAL)
		{
			DesiredFrame = IMF_RelativeToInitial;
		}
		else
		{
			check(0);
		}

		// Do nothing if already in desired frame
		if(DesiredFrame == MoveTrack->MoveFrame)
		{
			return;
		}

		// Find the first instance of this group. This is the one we are going to use to store the curve relative to.
		UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(MoveTrackIt.GetGroup());
		check(GrInst);

		AActor* Actor = GrInst->GetGroupActor();
		if(!Actor)
		{
			appMsgf(AMT_OK, *LocalizeUnrealEd("Error_NoActorForThisGroup"));
			return;
		}

		// Get instance of movement track, for the initial TM.
		UInterpTrackInstMove* MoveTrackInst = CastChecked<UInterpTrackInstMove>( GrInst->TrackInst(MoveTrackIt.GetTrackIndex()) );

		// Find the frame to convert key-frame from.
		FMatrix FromFrameTM = MoveTrack->GetMoveRefFrame(MoveTrackInst);

		// Find the frame to convert the key-frame into.
		AActor* BaseActor = Actor->GetBase();

		FMatrix BaseTM = FMatrix::Identity;
		if(BaseActor)
		{
			BaseTM = FRotationTranslationMatrix( BaseActor->Rotation, BaseActor->Location );
		}

		FMatrix ToFrameTM = FMatrix::Identity;
		if( DesiredFrame == IMF_World )
		{
			if(BaseActor)
			{
				ToFrameTM = BaseTM;
			}
			else
			{
				ToFrameTM = FMatrix::Identity;
			}
		}
		else if( DesiredFrame == IMF_RelativeToInitial )
		{
			if(BaseActor)
			{
				ToFrameTM = MoveTrackInst->InitialTM * BaseTM;
			}
			else
			{
				ToFrameTM = MoveTrackInst->InitialTM;
			}
		}
		FMatrix InvToFrameTM = ToFrameTM.Inverse();


		// Iterate over each keyframe. Convert key into world reference frame, then into new desired reference frame.
		check( MoveTrack->PosTrack.Points.Num() == MoveTrack->EulerTrack.Points.Num() );
		for(INT i=0; i<MoveTrack->PosTrack.Points.Num(); i++)
		{
			FQuat KeyQuat = FQuat::MakeFromEuler( MoveTrack->EulerTrack.Points(i).OutVal );
			FQuatRotationTranslationMatrix KeyTM( KeyQuat, MoveTrack->PosTrack.Points(i).OutVal );

			FMatrix WorldKeyTM = KeyTM * FromFrameTM;

			FVector WorldArriveTan = FromFrameTM.TransformNormal( MoveTrack->PosTrack.Points(i).ArriveTangent );
			FVector WorldLeaveTan = FromFrameTM.TransformNormal( MoveTrack->PosTrack.Points(i).LeaveTangent );

			FMatrix RelKeyTM = WorldKeyTM * InvToFrameTM;

			MoveTrack->PosTrack.Points(i).OutVal = RelKeyTM.GetOrigin();
			MoveTrack->PosTrack.Points(i).ArriveTangent = ToFrameTM.InverseTransformNormal( WorldArriveTan );
			MoveTrack->PosTrack.Points(i).LeaveTangent = ToFrameTM.InverseTransformNormal( WorldLeaveTan );

			MoveTrack->EulerTrack.Points(i).OutVal = FQuat(RelKeyTM).Euler();
		}

		MoveTrack->MoveFrame = DesiredFrame;

		//PropertyWindow->Refresh(); // Don't know why this doesn't work...

		PropertyWindow->SetObject(NULL, EPropertyWindowFlags::Sorted);
		PropertyWindow->SetObject(MoveTrack, EPropertyWindowFlags::Sorted);
	}

	// We changed the interp mode, so dirty the Matinee sequence
	Interp->MarkPackageDirty();
}



/**
 * Toggles visibility of the trajectory for the selected movement track
 */
void WxInterpEd::OnContextTrackShow3DTrajectory( wxCommandEvent& In )
{
	check( HasATrackSelected() );

	// Check to make sure there is a movement track in list
	// before attempting to start the transaction system.
	if( HasATrackSelected( UInterpTrackMove::StaticClass() ) )
	{
		InterpEdTrans->BeginSpecial( *LocalizeUnrealEd( "InterpEd_Undo_ToggleTrajectory" ) );

		TTrackClassTypeIterator<UInterpTrackMove> MoveTrackIt(GetSelectedTrackIterator<UInterpTrackMove>());
		for( ; MoveTrackIt; ++MoveTrackIt )
		{
			// Grab the movement track for the selected group
			UInterpTrackMove* MoveTrack = *MoveTrackIt;
			MoveTrack->Modify();

			MoveTrack->bHide3DTrack = !MoveTrack->bHide3DTrack;
		}

		InterpEdTrans->EndSpecial();
	}
}


#if WITH_FBX
/**
 * Exports the animations in the selected track to FBX
 */
void WxInterpEd::OnContextTrackExportAnimFBX( wxCommandEvent& In )
{
	// Check to make sure there is an animation track in list
	// before attempting to start the transaction system.
	if( HasATrackSelected( UInterpTrackAnimControl::StaticClass() ) )
	{
		TArray<UInterpTrack*> SelectedTracks;
		GetSelectedTracks(SelectedTracks);

		check( SelectedTracks.Num() == 1 );

		UInterpTrack* SelectedTrack = SelectedTracks(0);

		// Make sure the track is an animation track
		UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(SelectedTrack);

		// Find the skeletal mesh for this anim track
		USkeletalMesh* SkelMesh = NULL;
		
		// Get the owning group of the track
		UInterpGroup* Group = CastChecked<UInterpGroup>( SelectedTrack->GetOuter() ) ;

		// Get the first group instance for this track.  In the case of animations there is only one instance usually
		UInterpGroupInst* GroupInst = Interp->FindFirstGroupInst( Group );

		// Get the actor for this group.
		ASkeletalMeshActor* SkelMeshActor = Cast<ASkeletalMeshActor>( GroupInst->GroupActor );

		// Someone could have hooked up an invalid actor.  In that case do nothing
		if( SkelMeshActor )
		{
			SkelMesh = SkelMeshActor->SkeletalMeshComponent->SkeletalMesh;
		}
		

		// If this is a valid anim track and it has a valid skeletal mesh
		if(AnimTrack && SkelMesh)
		{
			if( Interp != NULL )
			{
				WxFileDialog ExportFileDialog(this, 
					*LocalizeUnrealEd("ExportMatineeAnimTrack"), 
					*(GApp->LastDir[LD_GENERIC_EXPORT]), 
					TEXT(""), 
					TEXT("FBX document|*.fbx"),
					wxSAVE | wxOVERWRITE_PROMPT, 
					wxDefaultPosition);

				// Show dialog and execute the import if the user did not cancel out
				if( ExportFileDialog.ShowModal() == wxID_OK )
				{
					// Get the filename from dialog
					wxString ExportFilename = ExportFileDialog.GetPath();
					FFilename FileName = ExportFilename.c_str();
					GApp->LastDir[LD_GENERIC_EXPORT] = FileName.GetPath(); // Save path as default for next time.

					UnFbx::CFbxExporter* Exporter = UnFbx::CFbxExporter::GetInstance();

					// Export the Matinee information to a COLLADA document.
					Exporter->CreateDocument();
					Exporter->SetTrasformBaking(bBakeTransforms);

					// Export the anim sequences
					TArray<UAnimSequence*> AnimSequences;
					for(INT TrackKeyIndex = 0; TrackKeyIndex < AnimTrack->AnimSeqs.Num(); ++TrackKeyIndex)
					{
						UAnimSequence* AnimSeq = AnimTrack->FindAnimSequenceFromName(AnimTrack->AnimSeqs(TrackKeyIndex).AnimSeqName);
						
						if( AnimSeq )
						{
							AnimSequences.Push( AnimSeq );
						}
						else
						{
							warnf( TEXT("Warning: Animation %s not found when exporting %s"), *AnimTrack->AnimSeqs(TrackKeyIndex).AnimSeqName.ToString(), *AnimTrack->GetName() );
						}
					}

					FString ExportName = Group->GroupName.ToString() + TEXT("_") + AnimTrack->GetName();
					Exporter->ExportAnimSequencesAsSingle( SkelMesh, SkelMeshActor, ExportName, AnimSequences, AnimTrack->AnimSeqs );

					// Save to disk
					Exporter->WriteToFile( ExportFilename.c_str() );
				}
			}
		}
	}
}
#endif


/**
 * Shows or hides all movement track trajectories in the Matinee sequence
 */
void WxInterpEd::OnViewShowOrHideAll3DTrajectories( wxCommandEvent& In )
{
	// Are we showing or hiding track trajectories?
	const UBOOL bShouldHideTrajectories = ( In.GetId() == IDM_INTERP_VIEW_HideAll3DTrajectories );

	UBOOL bAnyTracksModified = FALSE;

	// Iterate over each group
	for( INT CurGroupIndex = 0; CurGroupIndex < IData->InterpGroups.Num(); ++CurGroupIndex	)
	{
		UInterpGroup* CurGroup = IData->InterpGroups( CurGroupIndex );
		assert( CurGroup != NULL );

		// Iterate over tracks in this group
		for( INT CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
		{
			UInterpTrack* CurTrack = CurGroup->InterpTracks( CurTrackIndex );
			assert( CurTrack != NULL );

			// Is this a movement track?  Only movement tracks have trajectories
			UInterpTrackMove* MovementTrack = Cast<UInterpTrackMove>( CurTrack );
			if( MovementTrack != NULL )
			{
				if( bShouldHideTrajectories != MovementTrack->bHide3DTrack )
				{
					// Begin our undo transaction if we haven't started on already
					if( !bAnyTracksModified )
					{
						InterpEdTrans->BeginSpecial( *LocalizeUnrealEd( "InterpEd_Undo_ShowOrHideAllTrajectories" ) );
						bAnyTracksModified = TRUE;
					}

					// Show or hide the trajectory for this movement track
					MovementTrack->Modify();
					MovementTrack->bHide3DTrack = bShouldHideTrajectories;
				}
			}
		}
	}

	// End our undo transaction, but only if we actually modified something
	if( bAnyTracksModified )
	{
		InterpEdTrans->EndSpecial();
	}
}



/** Toggles 'capture mode' for particle replay tracks */
void WxInterpEd::OnParticleReplayTrackContext_ToggleCapture( wxCommandEvent& In )
{
	check( HasATrackSelected() );

	const UBOOL bEnableCapture = ( In.GetId() == IDM_INTERP_ParticleReplayTrackContext_StartRecording );

	TTrackClassTypeIterator<UInterpTrackParticleReplay> ReplayTrackIt(GetSelectedTrackIterator<UInterpTrackParticleReplay>());
	for( ; ReplayTrackIt; ++ReplayTrackIt )
	{
		UInterpTrackParticleReplay* ParticleReplayTrack = *ReplayTrackIt;

		// Toggle capture mode
		ParticleReplayTrack->bIsCapturingReplay = bEnableCapture;

		// Dirty the track window viewports
		InvalidateTrackWindowViewports();
	}
}

/** Pops up a combo box of the pawn's slot nodes to set the track's ParentNodeName property */
void WxInterpEd::OnNotifyTrackContext_SetParentNodeName( wxCommandEvent& In )
{
	TTrackClassTypeIterator<UInterpTrackNotify> NotifyTrackIt(GetSelectedTrackIterator<UInterpTrackNotify>());
	for(; NotifyTrackIt; ++NotifyTrackIt)
	{
		UInterpTrackNotify* NotifyTrack = *NotifyTrackIt;

		if(NotifyTrack)
		{
			UInterpGroup* Group = NotifyTrack->GetOwningGroup();
			UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(Group);
			check(GrInst);

			AActor* Actor = GrInst->GetGroupActor();

			if (Actor != NULL)
			{
				TArray<FAnimSlotDesc> SlotDescs;
				Actor->PreviewBeginAnimControl(Group);
				Actor->GetAnimControlSlotDesc(SlotDescs);

				// Warn if no slots found and fail to change ParentNodeName
				if(SlotDescs.Num() == 0)
				{
					appMsgf(AMT_OK, *LocalizeUnrealEd("Error_NoParentNode"));
					return;
				}

				// Build array of slot names. The slot name is used for the track's ParentNodeName property.
				TArray<FString> SlotStrings;

				for(INT i=0; i<SlotDescs.Num(); i++)
				{
					SlotStrings.AddItem(*SlotDescs(i).SlotName.ToString());
				}

				// If no slots free fail to change ParentNodeName
				if(SlotStrings.Num() == 0)
				{
					appMsgf(AMT_OK, *LocalizeUnrealEd("Error_NoAnimChannelsLeft"));
					return;
				}

				// Pop up a combo box to allow selection of a slot name to use for the ParentNodeName property
				WxDlgGenericComboEntry dlg;
				INT SelectedIndex = SlotStrings.FindItemIndex(NotifyTrack->Node->ParentNodes(0)->NodeName.ToString());
				if(dlg.ShowModal(TEXT("ChooseParentNode"), TEXT("ParentNode"), SlotStrings, SelectedIndex >= 0 ? SelectedIndex : 0, TRUE) == wxID_OK)
				{
					NotifyTrack->ParentNodeName = FName(*dlg.GetSelectedString());
					NotifyTrack->Node->ParentNodes(0)->NodeName = FName(*dlg.GetSelectedString());

					// When you change the ParentNodeName, change the TrackTitle to reflect that.
					UInterpTrackNotify* DefNotifyTrack = CastChecked<UInterpTrackNotify>(NotifyTrack->GetClass()->GetDefaultObject());
					FString DefaultTrackTitle = DefNotifyTrack->TrackTitle;

					if(NotifyTrack->ParentNodeName == NAME_None)
					{
						NotifyTrack->TrackTitle = DefaultTrackTitle;
					}
					else
					{
						NotifyTrack->TrackTitle = FString::Printf(TEXT("%s:%s"), *DefaultTrackTitle, *NotifyTrack->ParentNodeName.ToString());
					}

					NotifyTrack->MarkPackageDirty();
				}
			}
		}
	}
}

void WxInterpEd::OnContextGroupRename( wxCommandEvent& In )
{
	if( !HasAGroupSelected() )
	{
		return;
	}

	for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		UInterpGroup* GroupToRename = *GroupIt;
		const FString OriginalName = GroupToRename->GroupName.ToString();
		FName NewName;
		UBOOL bValidName = FALSE;
		UBOOL bUserCancelled = FALSE;

		while( !bValidName && !bUserCancelled )
		{
			FString DialogName = GroupToRename->bIsFolder ? TEXT( "Rename Folder" ) : TEXT( "Rename Group" );
			FString PromptName = GroupToRename->bIsFolder ? TEXT( "New Folder Name" ) : TEXT( "New Group Name" );

			if( InterpEditorUtils::GetNewName( DialogName, PromptName, OriginalName, NewName, &OriginalName ) )
			{
				bValidName = TRUE;

				// Check this name does not already exist.
				for(INT i=0; i<IData->InterpGroups.Num() && bValidName; i++)
				{
					if(IData->InterpGroups(i)->GroupName == NewName)
					{
						bValidName = FALSE;
					}
				}

				if(!bValidName)
				{
					appMsgf( AMT_OK, *LocalizeUnrealEd("Error_NameAlreadyExists") );
				}
			}
			// The user cancelled on the dialog, so, exit the while loop.
			else
			{
				bUserCancelled = TRUE;
			}
		}

		// If the user cancelled on renaming in the dialog for this 
		// selected group, move on to the next selected group.
		if( bUserCancelled )
		{
			continue;
		}

		// We also need to change the name of the variable connector on all SeqAct_Interps in this level using this InterpData
		USequence* RootSeq = Interp->GetRootSequence();
		check(RootSeq);

		TArray<USequenceObject*> MatineeActions;
		RootSeq->FindSeqObjectsByClass( USeqAct_Interp::StaticClass(), MatineeActions );

		for(INT i=0; i<MatineeActions.Num(); i++)
		{
			USeqAct_Interp* TestAction = CastChecked<USeqAct_Interp>( MatineeActions(i) );
			check(TestAction);

			UInterpData* TestData = TestAction->FindInterpDataFromVariable();
			if(TestData && TestData == IData)
			{
				INT VarIndex = TestAction->FindConnectorIndex( GroupToRename->GroupName.ToString(), LOC_VARIABLE );
				if(VarIndex != INDEX_NONE && VarIndex >= 1) // Ensure variable index is not the reserved first one.
				{
					TestAction->VariableLinks(VarIndex).LinkDesc = NewName.ToString();
					TestAction->SetPendingConnectorRecalc(); 
				}
			}
		}

		// Update any camera cuts to point to new group name
		UInterpGroupDirector* DirGroup = IData->FindDirectorGroup();
		if(DirGroup)
		{
			UInterpTrackDirector* DirTrack = DirGroup->GetDirectorTrack();
			if(DirTrack)
			{
				for(INT i=0; i<DirTrack->CutTrack.Num(); i++)
				{
					FDirectorTrackCut& Cut = DirTrack->CutTrack(i);
					if(Cut.TargetCamGroup == GroupToRename->GroupName)
					{
						Cut.TargetCamGroup = NewName;
					}	
				}
			}
		}

		// Change the name of the InterpGroup.
		GroupToRename->GroupName = NewName;
	}

	// Refresh the associated Kismet window to update with the new name
	RefreshAssociatedKismetWindow();
}

void WxInterpEd::OnContextGroupDelete( wxCommandEvent& In )
{
	//stop recording
	StopRecordingInterpValues();

	// Must have at least one group selected for this function.
	check( HasAGroupSelected() );

	for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		UInterpGroup* Group = *GroupIt;

		if( Group->bIsFolder )
		{
			// Check we REALLY want to do this.
			UBOOL bDoDestroy = appMsgf(AMT_YesNo, LocalizeSecure(LocalizeUnrealEd( "InterpEd_DeleteSelectedFolder" ), *Group->GroupName.ToString() ));

			// The user backed out of deleting this group. 
			// Deselect the group so it doesn't get deleted.
			if(!bDoDestroy)
			{
				DeselectGroup(Group, FALSE);
			}
		}
	}

	DeleteSelectedGroups();

	// Refresh the associated kismet window to show the group deletion
	RefreshAssociatedKismetWindow();
}

/** Prompts the user for a name for a new filter and creates a custom filter. */
void WxInterpEd::OnContextGroupCreateTab( wxCommandEvent& In )
{
	// Display dialog and let user enter new time.
	FString TabName;

	WxDlgGenericStringEntry Dlg;
	const INT Result = Dlg.ShowModal( TEXT("CreateGroupTab_Title"), TEXT("CreateGroupTab_Caption"), TEXT(""));
	if( Result == wxID_OK )
	{
		// Create a new tab.
		if( HasAGroupSelected() )
		{
			UInterpFilter_Custom* Filter = ConstructObject<UInterpFilter_Custom>(UInterpFilter_Custom::StaticClass(), IData, NAME_None, RF_Transactional);

			if(Dlg.GetEnteredString().Len())
			{
				Filter->Caption = Dlg.GetEnteredString();
			}
			else
			{
				Filter->Caption = Filter->GetName();
			}
	
			TArray<UInterpGroup*> SelectedGroups;
			GetSelectedGroups(SelectedGroups);
			Filter->GroupsToInclude.Append(SelectedGroups);

			IData->InterpFilters.AddItem(Filter);
		}
	}
}

/** Sends the selected group to the tab the user specified.  */
void WxInterpEd::OnContextGroupSendToTab( wxCommandEvent& In )
{
	INT TabIndex = (In.GetId() - IDM_INTERP_GROUP_SENDTOTAB_START);

	if(TabIndex >=0 && TabIndex < IData->InterpFilters.Num())
	{
		// Make sure the active group isnt already in the filter's set of groups.
		UInterpFilter_Custom* Filter = Cast<UInterpFilter_Custom>(IData->InterpFilters(TabIndex));

		if(Filter != NULL)
		{
			for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
			{
				// Only add move the selected group to the tab if it's not already in the tab.
				if( !Filter->GroupsToInclude.ContainsItem(*GroupIt) )
				{
					Filter->GroupsToInclude.AddItem(*GroupIt);
				}
			}
		}
	}
}

/** Removes the group from the current tab.  */
void WxInterpEd::OnContextGroupRemoveFromTab( wxCommandEvent& In )
{
	// Make sure the active group exists in the selected filter and that the selected filter isn't a default filter.
	UInterpFilter_Custom* Filter = Cast<UInterpFilter_Custom>(IData->SelectedFilter);

	UBOOL bInvalidateViewports = FALSE;

	if(Filter != NULL && IData->InterpFilters.ContainsItem(Filter) == TRUE)
	{
		for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
		{
			UInterpGroup* Group = *GroupIt;

			if( Filter->GroupsToInclude.ContainsItem(Group) )
			{
				Filter->GroupsToInclude.RemoveItem(Group);
				Group->bVisible = FALSE;

				bInvalidateViewports = TRUE;
			}
		}

		if( bInvalidateViewports )
		{
			// Dirty the track window viewports
			InvalidateTrackWindowViewports();
		}
	}
}

/** Exports all the animations in the group as a single FBX file.  */
void WxInterpEd::OnContextGroupExportAnimFBX( wxCommandEvent& In )
{
#if WITH_FBX
	// Check to make sure there is an animation track in list
	// before attempting to start the transaction system.
	if( HasAGroupSelected() )
	{
		TArray<UInterpGroup*> SelectedGroups;
		GetSelectedGroups(SelectedGroups);

		if( SelectedGroups.Num() == 1 )
		{
			UInterpGroup* SelectedGroup = SelectedGroups(0);

			// Only export this group if it has at least one animation track
			if( SelectedGroup->HasAnimControlTrack() )
			{
				// Find the skeletal mesh for this group
				USkeletalMeshComponent* SkelMeshComponent = NULL;
				{
					// Get the first group instance for this group.  In the case of animations there is usually only one instance
					UInterpGroupInst* GroupInst = Interp->FindFirstGroupInst( SelectedGroup );

					// Get the actor for this group.
					ASkeletalMeshActor* SkelMeshActor = Cast<ASkeletalMeshActor>( GroupInst->GroupActor );

					// Someone could have hooked up an invalid actor.  In that case do nothing
					if( SkelMeshActor )
					{
						SkelMeshComponent = SkelMeshActor->SkeletalMeshComponent;
					}
				}

				// If this is a valid skeletal mesh
				if(SkelMeshComponent)
				{
					if( Interp != NULL )
					{
						WxFileDialog ExportFileDialog(this, 
							*LocalizeUnrealEd("ExportMatineeAnimTrack"), 
							*(GApp->LastDir[LD_GENERIC_EXPORT]), 
							TEXT(""), 
							TEXT("FBX document|*.fbx"),
							wxSAVE | wxOVERWRITE_PROMPT, 
							wxDefaultPosition);

						// Show dialog and execute the import if the user did not cancel out
						if( ExportFileDialog.ShowModal() == wxID_OK )
						{
							// Get the filename from dialog
							wxString ExportFilename = ExportFileDialog.GetPath();
							FFilename FileName = ExportFilename.c_str();
							GApp->LastDir[LD_GENERIC_EXPORT] = FileName.GetPath(); // Save path as default for next time.

							UnFbx::CFbxExporter* Exporter = UnFbx::CFbxExporter::GetInstance();

							// Export the Matinee information to an FBX document.
							Exporter->CreateDocument();
							Exporter->SetTrasformBaking(bBakeTransforms);

							// Export the animation sequences in the group by sampling the skeletal mesh over the
							// duration of the matinee sequence
							Exporter->ExportMatineeGroup(Interp, SkelMeshComponent);

							// Save to disk
							Exporter->WriteToFile( ExportFilename.c_str() );
						}
					}
				}
			}
		}
	}
#endif // WITH_FBX
}

/** Deletes the currently selected group tab.  */
void WxInterpEd::OnContextDeleteGroupTab( wxCommandEvent& In )
{
	// Make sure the active group exists in the selected filter and that the selected filter isn't a default filter.
	UInterpFilter_Custom* Filter = Cast<UInterpFilter_Custom>(IData->SelectedFilter);

	if(Filter != NULL)
	{
		IData->InterpFilters.RemoveItem(Filter);

		// Set the selected filter back to the all filter.
		if(IData->DefaultFilters.Num())
		{
			SetSelectedFilter(IData->DefaultFilters(0));
		}
		else
		{
			SetSelectedFilter(NULL);
		}
	}
}



/** Called when the user selects to move a group to another group folder */
void WxInterpEd::OnContextGroupChangeGroupFolder( wxCommandEvent& In )
{
	// To invoke this command, there must be at least one group selected.
	check( HasAGroupSelected() );

	// Figure out if we're moving the active group to a new group, or if we simply want to unparent it
	const UBOOL bIsParenting = ( In.GetId() != IDM_INTERP_GROUP_RemoveFromGroupFolder );

	// Figure out which direction we're moving things: A group to the selected folder?  Or, the selected group
	// to a folder?
	UBOOL bIsMovingSelectedGroupToFolder = FALSE;
	UBOOL bIsMovingGroupToSelectedFolder = FALSE;

	if( bIsParenting )
	{
		bIsMovingSelectedGroupToFolder =
			( In.GetId() >= IDM_INTERP_GROUP_MoveActiveGroupToFolder_Start && In.GetId() <= IDM_INTERP_GROUP_MoveActiveGroupToFolder_End );
		bIsMovingGroupToSelectedFolder = !bIsMovingSelectedGroupToFolder;
	}

	// Store the source group to the destination group index.
	TMap<UInterpGroup*, UInterpGroup*> SourceGroupToDestGroup;

	for( FSelectedGroupIterator GroupIter(GetSelectedGroupIterator()); GroupIter; ++GroupIter )
	{
		UInterpGroup* SelectedGroup = *GroupIter;

		// Make sure we're dealing with a valid group index
		INT MenuGroupIndex = INDEX_NONE;

		if( bIsParenting )
		{
			MenuGroupIndex =
				bIsMovingSelectedGroupToFolder ?
					( In.GetId() - IDM_INTERP_GROUP_MoveActiveGroupToFolder_Start ) :
					( In.GetId() - IDM_INTERP_GROUP_MoveGroupToActiveFolder_Start );
		}
		else
		{
			// If we're unparenting, then use ourselves as the destination index
			MenuGroupIndex = GroupIter.GetGroupIndex();

			// Make sure we're not already in the desired state; this would be a UI error
			check( SelectedGroup->bIsParented );
		}

		UBOOL bIsValidGroupIndex = IData->InterpGroups.IsValidIndex( MenuGroupIndex );
		check( bIsValidGroupIndex );
		if( !bIsValidGroupIndex )
		{
			continue;
		}


		// Figure out what our source and destination groups are for this operation
		if( !bIsParenting || bIsMovingSelectedGroupToFolder )
		{
			// We're moving the selected group to a group, or unparenting a group
			SourceGroupToDestGroup.Set( SelectedGroup, IData->InterpGroups( MenuGroupIndex ) );
		}
		else
		{
			// We're moving a group to our selected group
			SourceGroupToDestGroup.Set( IData->InterpGroups(MenuGroupIndex), IData->InterpGroups( GroupIter.GetGroupIndex() ));
		}
	}



	// OK, to pull this off we need to do two things.  First, we need to relocate the source group such that
	// it's at the bottom of the destination group's children in our list.  Then, we'll need to mark the
	// group as 'parented'!

	// We're about to modify stuff!
	InterpEdTrans->BeginSpecial( *LocalizeUnrealEd( "InterpEd_ChangeGroupFolder" ) );
	Interp->Modify();
	IData->Modify();

	for( TMap<UInterpGroup*,UInterpGroup*>::TIterator SrcToDestMap(SourceGroupToDestGroup); SrcToDestMap; ++SrcToDestMap )
	{
		UInterpGroup* SourceGroup = SrcToDestMap.Key();
		UInterpGroup* DestGroup = SrcToDestMap.Value();

		//if they are not the same and they are both folders, skip.  No folder on folder operations
		if (SourceGroup->bIsFolder && DestGroup->bIsFolder)
		{
			continue;
		}

		// First, remove ourselves from the group list
		{
			INT SourceGroupIndex = IData->InterpGroups.FindItemIndex( SourceGroup );
			IData->InterpGroups.Remove( SourceGroupIndex );
		}
		INT DestGroupIndex = IData->InterpGroups.FindItemIndex( DestGroup );

		INT TargetGroupIndex = DestGroupIndex + 1;
		for( INT OtherGroupIndex = TargetGroupIndex; OtherGroupIndex < IData->InterpGroups.Num(); ++OtherGroupIndex )
		{
			UInterpGroup* OtherGroup = IData->InterpGroups( OtherGroupIndex );

			// Is this group parented?
			if( OtherGroup->bIsParented )
			{
				// OK, this is a child group of the destination group.  We want to append our new group to the end of
				// the destination group's list of children, so we'll just keep on iterating.
				++TargetGroupIndex;
			}
			else
			{
				// This group isn't the destination group or a child of the destination group.  We now have the index
				// we're looking for!
				break;
			}
		}

		// OK, now we know where we need to place the source group to in the list.  Let's do it!
		IData->InterpGroups.InsertItem( SourceGroup, TargetGroupIndex );

		// OK, now mark the group as parented!  Note that if we're relocating a group from one folder to another, it
		// may already be tagged as parented.
		if( SourceGroup->bIsParented != bIsParenting )
		{
			SourceGroup->Modify();
			SourceGroup->bIsParented = bIsParenting;
		}
	}

	// Complete undo state
	InterpEdTrans->EndSpecial();

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();
}



// Iterate over keys changing their interpolation mode and adjusting tangents appropriately.
void WxInterpEd::OnContextKeyInterpMode( wxCommandEvent& In )
{

	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(i);
		UInterpTrack* Track = SelKey.Track;

		if(In.GetId() == IDM_INTERP_KEYMODE_LINEAR)
		{
			Track->SetKeyInterpMode( SelKey.KeyIndex, CIM_Linear );
		}
		else if(In.GetId() == IDM_INTERP_KEYMODE_CURVE_AUTO)
		{
			Track->SetKeyInterpMode( SelKey.KeyIndex, CIM_CurveAuto );
		}
		else if(In.GetId() == IDM_INTERP_KEYMODE_CURVE_AUTO_CLAMPED)
		{
			Track->SetKeyInterpMode( SelKey.KeyIndex, CIM_CurveAutoClamped );
		}
		else if(In.GetId() == IDM_INTERP_KEYMODE_CURVEBREAK)
		{
			Track->SetKeyInterpMode( SelKey.KeyIndex, CIM_CurveBreak );
		}
		else if(In.GetId() == IDM_INTERP_KEYMODE_CONSTANT)
		{
			Track->SetKeyInterpMode( SelKey.KeyIndex, CIM_Constant );
		}
	}

	CurveEd->UpdateDisplay();
}

/** Pops up menu and lets you set the time for the selected key. */
void WxInterpEd::OnContextSetKeyTime( wxCommandEvent& In )
{
	// Only works if one key is selected.
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	// Get the time the selected key is currently at.
	FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
	UInterpTrack* Track = SelKey.Track;

	FLOAT CurrentKeyTime = Track->GetKeyframeTime(SelKey.KeyIndex);
	const FString CurrentTimeStr = FString::Printf( TEXT("%3.3f"), CurrentKeyTime );

	// Display dialog and let user enter new time.
	WxDlgGenericStringEntry dlg;
	const INT Result = dlg.ShowModal( TEXT("NewKeyTime"), TEXT("NewTime"), *CurrentTimeStr);
	if( Result != wxID_OK )
		return;

	double dNewTime;
	const UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToDouble(&dNewTime);
	if(!bIsNumber)
		return;

	const FLOAT NewKeyTime = (FLOAT)dNewTime;

	// Save off the original key index to check if a movement 
	// track needs its initial transform updated. 
	const INT OldKeyIndex = SelKey.KeyIndex;

	// Move the key. Also update selected to reflect new key index.
	SelKey.KeyIndex = Track->SetKeyframeTime( SelKey.KeyIndex, NewKeyTime );

	// There are two scenarios when the first key was changed using "Set Time": 
	// (1) user invoked "Set Time" on the first key with a time value that exceeds the second key's time.
	// (2) user invoked "Set Time" on a non-first key with a time value less than or equal to the first key's time. 
	const UBOOL bFirstKeyChangedPositions = (OldKeyIndex != SelKey.KeyIndex) && (0 == SelKey.KeyIndex || 0 == OldKeyIndex);

	if( bFirstKeyChangedPositions )
	{
		// Determine if we have a moment track.
		UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>( SelKey.Track );
		if( !MoveTrack )
		{
			UInterpTrackMoveAxis* MoveSubTrack = Cast<UInterpTrackMoveAxis>( SelKey.Track );
			if( MoveSubTrack )
			{
				// If we have a moment subtrack then get its parent move track
				MoveTrack = CastChecked<UInterpTrackMove>( MoveSubTrack->GetOuter() );
			}
		}

		if( MoveTrack )
		{
			// We must update the initial transform for a RelativeToInitial move track if the first
			// key changed array positions because the track will reference the old transformation. 
			UpdateInitialTransformForMoveTrack( SelKey.Group, MoveTrack );
		}
	}

	// Update positions at current time but with new keyframe times.
	RefreshInterpPosition();
	CurveEd->UpdateDisplay();
}

/** Pops up a menu and lets you set the value for the selected key. Not all track types are supported. */
void WxInterpEd::OnContextSetValue( wxCommandEvent& In )
{
	// Only works if one key is selected.
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	// Get the time the selected key is currently at.
	FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
	UInterpTrack* Track = SelKey.Track;

	// If its a float track - pop up text entry dialog.
	UInterpTrackFloatBase* FloatTrack = Cast<UInterpTrackFloatBase>(Track);
	if(FloatTrack)
	{
		// Get current float value of the key
		FLOAT CurrentKeyVal = FloatTrack->FloatTrack.Points(SelKey.KeyIndex).OutVal;
		const FString CurrentValStr = FString::Printf( TEXT("%f"), CurrentKeyVal );

		// Display dialog and let user enter new value.
		WxDlgGenericStringEntry dlg;
		const INT Result = dlg.ShowModal( TEXT("NewKeyValue"), TEXT("NewValue"), *CurrentValStr);
		if( Result != wxID_OK )
			return;

		double dNewVal;
		const UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToDouble(&dNewVal);
		if(!bIsNumber)
			return;

		// Set new value, and update tangents.
		const FLOAT NewVal = (FLOAT)dNewVal;
		FloatTrack->FloatTrack.Points(SelKey.KeyIndex).OutVal = NewVal;
		FloatTrack->FloatTrack.AutoSetTangents(FloatTrack->CurveTension);
	}

	// Update positions at current time but with new keyframe times.
	RefreshInterpPosition();
	CurveEd->UpdateDisplay();
}


/** Pops up a menu and lets you set the color for the selected key. Not all track types are supported. */
void WxInterpEd::OnContextSetColor( wxCommandEvent& In )
{
	// Only works if one key is selected.
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	// Get the time the selected key is currently at.
	FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
	UInterpTrack* Track = SelKey.Track;

	// If its a color prop track - pop up color dialog.
	UInterpTrackColorProp* ColorPropTrack = Cast<UInterpTrackColorProp>(Track);
	if(ColorPropTrack)
	{
		FVector CurrentColorVector = ColorPropTrack->VectorTrack.Points(SelKey.KeyIndex).OutVal;
		FColor CurrentColor(FLinearColor(CurrentColorVector.X, CurrentColorVector.Y, CurrentColorVector.Z));

		// Get the current color and show a color picker dialog.
		FPickColorStruct PickColorStruct;
		PickColorStruct.RefreshWindows.AddItem(this);
		PickColorStruct.DWORDColorArray.AddItem(&CurrentColor);
		PickColorStruct.bModal = TRUE;

		if (PickColor(PickColorStruct) == ColorPickerConstants::ColorAccepted)
		{
			// The user chose a color so set the keyframe color to the color they picked.
			FLinearColor VectorColor(CurrentColor);

			ColorPropTrack->VectorTrack.Points(SelKey.KeyIndex).OutVal = FVector(VectorColor.R, VectorColor.G, VectorColor.B);
			ColorPropTrack->VectorTrack.AutoSetTangents(ColorPropTrack->CurveTension);
		}
	}

	// We also support linear color tracks!
	UInterpTrackLinearColorProp* LinearColorPropTrack = Cast<UInterpTrackLinearColorProp>(Track);
	if(LinearColorPropTrack)
	{
		// Get the current color and show a color picker dialog.
		FPickColorStruct PickColorStruct;
		PickColorStruct.RefreshWindows.AddItem(this);
		PickColorStruct.FLOATColorArray.AddItem(&(LinearColorPropTrack->LinearColorTrack.Points(SelKey.KeyIndex).OutVal));
		PickColorStruct.bModal = TRUE;

		if (PickColor(PickColorStruct) == ColorPickerConstants::ColorAccepted)
		{
			LinearColorPropTrack->LinearColorTrack.AutoSetTangents(LinearColorPropTrack->CurveTension);
		}
	}


	// Update positions at current time but with new keyframe times.
	RefreshInterpPosition();
	CurveEd->UpdateDisplay();
}

/**
 * Flips the value of the selected key for a boolean property.
 *
 * @note	Assumes that the user was only given the option of flipping the 
 *			value in the context menu (i.e. TRUE -> FALSE or FALSE -> TRUE).
 *
 * @param	In	The wxWidgets event sent when a user selects the "Set to True" or "Set to False" context menu option.
 */
void WxInterpEd::OnContextSetBool( wxCommandEvent& In )
{
	// Only works if one key is selected.
	if( Opt->SelectedKeys.Num() != 1 )
	{
		return;
	}

	// Get the time the selected key is currently at.
	FInterpEdSelKey& SelectedKey = Opt->SelectedKeys(0);
	UInterpTrack* Track = SelectedKey.Track;

	// Can't flip the boolean unless the owning track is a boolean property track. 
	if( Track->IsA(UInterpTrackBoolProp::StaticClass()) )
	{
		UInterpTrackBoolProp* BoolPropTrack = CastChecked<UInterpTrackBoolProp>(Track);
		FBoolTrackKey& Key = BoolPropTrack->BoolTrack(SelectedKey.KeyIndex);

		// Flip the value.
		Key.Value = !Key.Value;
	}
}


/** Pops up menu and lets the user set a group to use to lookup transform info for a movement keyframe. */
void WxInterpEd::OnSetMoveKeyLookupGroup( wxCommandEvent& In )
{
	// Only works if one key is selected.
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	// Get the time the selected key is currently at.
	FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
	UInterpTrack* Track = SelKey.Track;

	// Only perform work if we are on a movement track.
	UInterpTrackMoveAxis* MoveTrackAxis = NULL;
	UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(Track);
	if(!MoveTrack)
	{
		MoveTrackAxis = Cast<UInterpTrackMoveAxis>( Track );
	}

	if( MoveTrack || MoveTrackAxis )
	{
		// Make array of group names
		TArray<FString> GroupNames;
		for ( INT GroupIdx = 0; GroupIdx < IData->InterpGroups.Num(); GroupIdx++ )
		{
			// Skip folder groups
			if( !IData->InterpGroups( GroupIdx )->bIsFolder )
			{
				if(IData->InterpGroups(GroupIdx) != SelKey.Group)
				{
					GroupNames.AddItem( *(IData->InterpGroups(GroupIdx)->GroupName.ToString()) );
				}
			}
		}

		WxDlgGenericComboEntry	dlg;
		const INT	Result = dlg.ShowModal( TEXT("SelectGroup"), TEXT("SelectGroupToLookupDataFrom"), GroupNames, 0, TRUE );
		if ( Result == wxID_OK )
		{
			FName KeyframeLookupGroup = FName( *dlg.GetSelectedString() );
			if( MoveTrack )
			{
				MoveTrack->SetLookupKeyGroupName(SelKey.KeyIndex, KeyframeLookupGroup);
			}
			else
			{
				MoveTrackAxis->SetLookupKeyGroupName( SelKey.KeyIndex, KeyframeLookupGroup );
			}
		}
	}
}

/** Clears the lookup group for a currently selected movement key. */
void WxInterpEd::OnClearMoveKeyLookupGroup( wxCommandEvent& In )
{
	// Only works if one key is selected.
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	// Get the time the selected key is currently at.
	FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
	UInterpTrack* Track = SelKey.Track;

	// Only perform work if we are on a movement track.
	UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(Track);
	if(MoveTrack)
	{
		MoveTrack->ClearLookupKeyGroupName(SelKey.KeyIndex);
	}
	else
	{
		UInterpTrackMoveAxis* MoveTrackAxis = Cast<UInterpTrackMoveAxis>(Track);
		if( MoveTrackAxis )
		{
			MoveTrackAxis->ClearLookupKeyGroupName( SelKey.KeyIndex );
		}
	}
}


/** Rename an event. Handle removing/adding connectors as appropriate. */
void WxInterpEd::OnContextRenameEventKey(wxCommandEvent& In)
{
	// Only works if one Event key is selected.
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	// Find the EventNames of selected key
	FName EventNameToChange;
	FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
	UInterpTrack* Track = SelKey.Track;
	UInterpTrackEvent* EventTrack = Cast<UInterpTrackEvent>(Track);
	if(EventTrack)
	{
		EventNameToChange = EventTrack->EventTrack(SelKey.KeyIndex).EventName; 
	}
	else
	{
		return;
	}

	// Pop up dialog to ask for new name.
	WxDlgGenericStringEntry dlg;
	INT Result = dlg.ShowModal( TEXT("EnterNewEventName"), TEXT("NewEventName"), *EventNameToChange.ToString() );
	if( Result != wxID_OK )
		return;		

	FString TempString = dlg.GetEnteredString();
	TempString = TempString.Replace(TEXT(" "),TEXT("_"));
	FName NewEventName = FName( *TempString );

	// If this Event name is already in use- disallow it
	TArray<FName> CurrentEventNames;
	IData->GetAllEventNames(CurrentEventNames);
	if( CurrentEventNames.ContainsItem(NewEventName) )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_EventNameInUse") );
		return;
	}
	
	// Then go through all keys, changing those with this name to the new one.
	for(INT i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups(i);
		for(INT j=0; j<Group->InterpTracks.Num(); j++)
		{
			UInterpTrackEvent* EventTrack = Cast<UInterpTrackEvent>( Group->InterpTracks(j) );
			if(EventTrack)
			{
				for(INT k=0; k<EventTrack->EventTrack.Num(); k++)
				{
					if(EventTrack->EventTrack(k).EventName == EventNameToChange)
					{
						EventTrack->EventTrack(k).EventName = NewEventName;
					}	
				}
			}			
		}
	}

	// We also need to change the name of the output connector on all SeqAct_Interps using this InterpData
	USequence* RootSeq = Interp->GetRootSequence();
	check(RootSeq);

	TArray<USequenceObject*> MatineeActions;
	RootSeq->FindSeqObjectsByClass( USeqAct_Interp::StaticClass(), MatineeActions );

	for(INT i=0; i<MatineeActions.Num(); i++)
	{
		USeqAct_Interp* TestAction = CastChecked<USeqAct_Interp>( MatineeActions(i) );
		check(TestAction);
	
		UInterpData* TestData = TestAction->FindInterpDataFromVariable();
		if(TestData && TestData == IData)
		{
			INT OutputIndex = TestAction->FindConnectorIndex( EventNameToChange.ToString(), LOC_OUTPUT );
			if(OutputIndex != INDEX_NONE && OutputIndex >= 2) // Ensure Output index is not one of the reserved first 2.
			{
				TestAction->OutputLinks(OutputIndex).LinkDesc = NewEventName.ToString();
			}
		}
	}
}

/** Pops up a property window to edit the notify for this key */
void WxInterpEd::OnContextEditNotifyKey( wxCommandEvent& In )
{
	// Only works if one Notify key is selected.
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	// Find the Notify of selected key
	FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
	UInterpTrackNotify* NotifyTrack = Cast<UInterpTrackNotify>(SelKey.Track);

	if(NotifyTrack)
	{
		// Pop up a property window to edit the selected Notify
		WxPropertyWindowFrame* pw = new WxPropertyWindowFrame;
		pw->Create( GApp->EditorFrame, -1, this );
		pw->SetObject(NotifyTrack->NotifyTrack(SelKey.KeyIndex).Notify, EPropertyWindowFlags::Sorted);

		// Tell this property window to execute ACTOR DESELECT when the escape key is pressed
		pw->SetFlags(EPropertyWindowFlags::ExecDeselectOnEscape, TRUE);

		pw->Show();
		pw->Raise();
	}
}

void WxInterpEd::OnSetAnimKeyLooping( wxCommandEvent& In )
{
	UBOOL bNewLooping = (In.GetId() == IDM_INTERP_ANIMKEY_LOOP);

	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(i);
		UInterpTrack* Track = SelKey.Track;
		UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Track);
		if(AnimTrack)
		{
			AnimTrack->AnimSeqs(SelKey.KeyIndex).bLooping = bNewLooping;
		}
	}
}

void WxInterpEd::OnSetAnimOffset( wxCommandEvent& In )
{
	UBOOL bEndOffset = (In.GetId() == IDM_INTERP_ANIMKEY_SETENDOFFSET);

	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
	UInterpTrack* Track = SelKey.Track;
	UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Track);
	if(!AnimTrack)
	{
		return;
	}

	FLOAT CurrentOffset = 0.f;
	if(bEndOffset)
	{
		CurrentOffset = AnimTrack->AnimSeqs(SelKey.KeyIndex).AnimEndOffset;
	}
	else
	{
		CurrentOffset = AnimTrack->AnimSeqs(SelKey.KeyIndex).AnimStartOffset;
	}

	const FString CurrentOffsetStr = FString::Printf( TEXT("%3.3f"), CurrentOffset );

	// Display dialog and let user enter new offset.
	WxDlgGenericStringEntry dlg;
	const INT Result = dlg.ShowModal( TEXT("NewAnimOffset"), TEXT("NewOffset"), *CurrentOffsetStr);
	if( Result != wxID_OK )
		return;

	double dNewOffset;
	const UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToDouble(&dNewOffset);
	if(!bIsNumber)
		return;

	const FLOAT NewOffset = ::Max( (FLOAT)dNewOffset, 0.f );

	if(bEndOffset)
	{
		AnimTrack->AnimSeqs(SelKey.KeyIndex).AnimEndOffset = NewOffset;
	}
	else
	{
		AnimTrack->AnimSeqs(SelKey.KeyIndex).AnimStartOffset = NewOffset;
	}


	// Update stuff in case doing this has changed it.
	RefreshInterpPosition();
}

void WxInterpEd::OnSetAnimPlayRate( wxCommandEvent& In )
{
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
	UInterpTrack* Track = SelKey.Track;
	UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Track);
	if(!AnimTrack)
	{
		return;
	}

	FLOAT CurrentRate = AnimTrack->AnimSeqs(SelKey.KeyIndex).AnimPlayRate;
	const FString CurrentRateStr = FString::Printf( TEXT("%3.3f"), CurrentRate );

	// Display dialog and let user enter new rate.
	WxDlgGenericStringEntry dlg;
	const INT Result = dlg.ShowModal( TEXT("NewAnimRate"), TEXT("PlayRate"), *CurrentRateStr);
	if( Result != wxID_OK )
		return;

	double dNewRate;
	const UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToDouble(&dNewRate);
	if(!bIsNumber)
		return;

	const FLOAT NewRate = ::Clamp( (FLOAT)dNewRate, 0.01f, 100.f );

	AnimTrack->AnimSeqs(SelKey.KeyIndex).AnimPlayRate = NewRate;

	// Update stuff in case doing this has changed it.
	RefreshInterpPosition();
}

/** Handler for the toggle animation reverse menu item. */
void WxInterpEd::OnToggleReverseAnim( wxCommandEvent& In )
{
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
	UInterpTrack* Track = SelKey.Track;
	UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Track);
	if(!AnimTrack)
	{
		return;
	}

	AnimTrack->AnimSeqs(SelKey.KeyIndex).bReverse = !AnimTrack->AnimSeqs(SelKey.KeyIndex).bReverse;
}

/** Handler for UI update requests for the toggle anim reverse menu item. */
void WxInterpEd::OnToggleReverseAnim_UpdateUI( wxUpdateUIEvent& In )
{
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
	UInterpTrack* Track = SelKey.Track;
	UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Track);
	if(!AnimTrack)
	{
		return;
	}

	In.Check(AnimTrack->AnimSeqs(SelKey.KeyIndex).bReverse==TRUE);
}

/** Handler for the save as camera animation menu item. */
void WxInterpEd::OnContextSaveAsCameraAnimation( wxCommandEvent& In )
{
	// There must be one and only one selected group to save a camera animation out. 
	check( GetSelectedGroupCount() == 1 );

	WxDlgPackageGroupName dlg;
	dlg.SetTitle( *LocalizeUnrealEd("ExportCameraAnim") );
	FString Reason;

//@todo cb: GB conversion
	FString PackageName, GroupName, ObjName;
	{
		UPackage* SelectedPkg = NULL;
		UPackage* SelectedGrp = NULL;

		PackageName = SelectedPkg ? SelectedPkg->GetName() : TEXT("");
		GroupName = SelectedGrp ? SelectedGrp->GetName() : TEXT("");

		GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );
		UObject* const SelectedCamAnim = GEditor->GetSelectedObjects()->GetTop<UCameraAnim>();
		ObjName = SelectedCamAnim ? *SelectedCamAnim->GetName() : TEXT("");
	}

	if( dlg.ShowModal( PackageName, GroupName, ObjName ) == wxID_OK )
	{
		FString Pkg;
		if( dlg.GetGroup().Len() > 0 )
		{
			Pkg = FString::Printf(TEXT("%s.%s"), *dlg.GetPackage(), *dlg.GetGroup());
		}
		else
		{
			Pkg = FString::Printf(TEXT("%s"), *dlg.GetPackage());
		}
		UObject* ExistingPackage = UObject::FindPackage(NULL, *Pkg);
		if( ExistingPackage == NULL )
		{
			// Create the package
			ExistingPackage = UObject::CreatePackage(NULL,*(GroupName != TEXT("") ? (PackageName+TEXT(".")+GroupName) : PackageName));
		}

		// Make sure packages objects are duplicated into are fully loaded.
		TArray<UPackage*> TopLevelPackages;
		if( ExistingPackage )
		{
			TopLevelPackages.AddItem( ExistingPackage->GetOutermost() );
		}

		if(!dlg.GetPackage().Len() || !dlg.GetObjectName().Len())
		{
			appMsgf(AMT_OK,*LocalizeUnrealEd("Error_InvalidInput"));
		}
// 		else if( !FIsValidObjectName( *dlg.GetObjectName(), Reason ))
// 		{
// 			appMsgf( AMT_OK, *Reason );
// 		}
		else
		{
			UBOOL bNewObject = FALSE, bSavedSuccessfully = FALSE;

			UObject* ExistingObject = GEditor->StaticFindObject(UCameraAnim::StaticClass(), ExistingPackage, *dlg.GetObjectName(), TRUE);

			if (ExistingObject == NULL)
			{
				// attempting to create a new object, need to handle fully loading
				if( PackageTools::HandleFullyLoadingPackages( TopLevelPackages, TEXT("ExportCameraAnim") ) )
				{
					// make sure name of new object is unique
					if (ExistingPackage && !FIsUniqueObjectName(*dlg.GetObjectName(), ExistingPackage, Reason))
					{
						appMsgf(AMT_OK, *Reason);
					}
					else
					{
						// create it, then copy params into it
						ExistingObject = GEditor->StaticConstructObject(UCameraAnim::StaticClass(), ExistingPackage, *dlg.GetObjectName(), RF_Public|RF_Standalone);
						bNewObject = TRUE;
					}
				}
			}

			if (ExistingObject)
			{
				// copy params into it
				UCameraAnim* CamAnim = Cast<UCameraAnim>(ExistingObject);
				// Create the camera animation from the first selected group
				// because there should only be one selected group. 
				if (CamAnim->CreateFromInterpGroup(*GetSelectedGroupIterator(), Interp))
				{
					bSavedSuccessfully = TRUE;
					CamAnim->MarkPackageDirty();
				}
			}

			if (bNewObject)
			{
				if (!bSavedSuccessfully)
				{
					// delete the new object
					ExistingObject->MarkPendingKill();
				}
			}
		}
	}
}

/**
 * Calculates The timeline position of the longest track, which includes 
 *			the duration of any assets, such as: sounds or animations.
 *
 * @note	Use the template parameter to define which tracks to consider (all, selected only, etc).
 * 
 * @return	The timeline position of the longest track.
 */
template <class TrackFilterType>
FLOAT WxInterpEd::GetLongestTrackTime() const
{
	FLOAT LongestTrackTime = 0.0f;

	// Iterate through each group to find the longest track time.
	for( TInterpTrackIterator<TrackFilterType> TrackIter(IData->InterpGroups); TrackIter; ++TrackIter )
	{
		FLOAT TrackEndTime = (*TrackIter)->GetTrackEndTime();

		// Update the longest track time. 
		if( TrackEndTime > LongestTrackTime )
		{
			LongestTrackTime = TrackEndTime;
		}
	}

	return LongestTrackTime;
}

/**
 * Moves the marker the user grabbed to the given time on the timeline. 
 *
 * @param	MarkerType	The marker to move.
 * @param	InterpTime	The position on the timeline to move the marker. 
 */
void WxInterpEd::MoveGrabbedMarker( FLOAT InterpTime )
{
	const UBOOL bIgnoreSelectedKeys = FALSE;
	const UBOOL bIsLoopStartMarker = (GrabbedMarkerType == ISM_LoopStart);

	switch( GrabbedMarkerType )
	{
	case ISM_LoopStart:
		MoveLoopMarker( SnapTime(InterpTime, bIgnoreSelectedKeys), bIsLoopStartMarker );
		break;

	case ISM_LoopEnd:
		MoveLoopMarker( SnapTime(InterpTime, bIgnoreSelectedKeys), bIsLoopStartMarker );
		break;

	case ISM_SeqEnd:
		SetInterpEnd( SnapTime(InterpTime, bIgnoreSelectedKeys) );
		break;

	// Intentionally ignoring ISM_SeqStart because 
	// the sequence start must always be zero. 

	default:
		break;
	}
}

/**
 * Handler to move the grabbed marker to the current timeline position.
 *
 * @param	In	The event sent by wxWidgets.
 */
void WxInterpEd::OnContextMoveMarkerToCurrentPosition( wxCommandEvent& In )
{
	MoveGrabbedMarker(Interp->Position);
}

/**
 * Handler to move the clicked-marker to the beginning of the sequence.
 *
 * @param	In	The event sent by wxWidgets.
 */
void WxInterpEd::OnContextMoveMarkerToBeginning( wxCommandEvent& In )
{
	MoveGrabbedMarker(0.0f);
}

/**
 * Handler to move the clicked-marker to the end of the sequence.
 *
 * @param	In	The event sent by wxWidgets.
 */
void WxInterpEd::OnContextMoveMarkerToEnd( wxCommandEvent& In )
{
	MoveGrabbedMarker(IData->InterpLength);
}

/**
 * Handler to move the clicked-marker to the end of the longest track.
 *
 * @param	In	The event sent by wxWidgets.
 */
void WxInterpEd::OnContextMoveMarkerToEndOfLongestTrack( wxCommandEvent& In )
{
	MoveGrabbedMarker( GetLongestTrackTime<FAllTrackFilter>() );
}

/**
 * Handler to move the clicked-marker to the end of the selected track.
 *
 * @param	In	The event sent by wxWidgets.
 */
void WxInterpEd::OnContextMoveMarkerToEndOfSelectedTrack( wxCommandEvent& In )
{
	MoveGrabbedMarker( GetLongestTrackTime<FSelectedTrackFilter>() );
}

/**
 * Called when the user toggles the preference for allowing clicks on keyframe "bars" to cause a selection
 *
 * @param	In	Event generated by wxWidgets when the user toggles the preference
 */
void WxInterpEd::OnToggleKeyframeBarSelection( wxCommandEvent& In )
{
	bAllowKeyframeBarSelection = !bAllowKeyframeBarSelection;
	GConfig->SetBool( TEXT("Matinee"), TEXT("AllowKeyframeBarSelection"), bAllowKeyframeBarSelection, GEditorUserSettingsIni );
}

/**
 * Called automatically by wxWidgets to update the UI for the keyframe bar selection option
 *
 * @param	In	Event generated by wxWidgets to update the UI	
 */
void WxInterpEd::OnToggleKeyframeBarSelection_UpdateUI( wxUpdateUIEvent& In )
{
	In.Check( bAllowKeyframeBarSelection == TRUE );
}

/**
 * Called when the user toggles the preference for allowing clicks on keyframe text to cause a selection
 *
 * @param	In	Event generated by wxWidgets when the user toggles the preference
 */
void WxInterpEd::OnToggleKeyframeTextSelection( wxCommandEvent& In )
{
	bAllowKeyframeTextSelection = !bAllowKeyframeTextSelection;
	GConfig->SetBool( TEXT("Matinee"), TEXT("AllowKeyframeTextSelection"), bAllowKeyframeTextSelection, GEditorUserSettingsIni );
}

/**
 * Called automatically by wxWidgets to update the UI for the keyframe text selection option
 *
 * @param	In	Event generated by wxWidgets to update the UI
 */
void WxInterpEd::OnToggleKeyframeTextSelection_UpdateUI( wxUpdateUIEvent& In )
{
	In.Check( bAllowKeyframeTextSelection == TRUE );
}

/**
 * Prompts the user to edit volumes for the selected sound keys.
 */
void WxInterpEd::OnSetSoundVolume(wxCommandEvent& In)
{
	TArray<INT> SoundTrackKeyIndices;
	UBOOL bFoundVolume = FALSE;
	UBOOL bKeysDiffer = FALSE;
	FLOAT Volume = 1.0f;

	// Make a list of all keys and what their volumes are.
	for( INT i = 0 ; i < Opt->SelectedKeys.Num() ; ++i )
	{
		const FInterpEdSelKey& SelKey		= Opt->SelectedKeys(i);
		UInterpTrack* Track					= SelKey.Track;
		UInterpTrackSound* SoundTrack		= Cast<UInterpTrackSound>( Track );

		if( SoundTrack )
		{
			SoundTrackKeyIndices.AddItem(i);
			const FSoundTrackKey& SoundTrackKey	= SoundTrack->Sounds(SelKey.KeyIndex);
			if ( !bFoundVolume )
			{
				bFoundVolume = TRUE;
				Volume = SoundTrackKey.Volume;
			}
			else
			{
				if ( Abs(Volume-SoundTrackKey.Volume) > KINDA_SMALL_NUMBER )
				{
					bKeysDiffer = TRUE;
				}
			}
		}
	}

	if ( SoundTrackKeyIndices.Num() )
	{
		// Display dialog and let user enter new rate.
		const FString VolumeStr( FString::Printf( TEXT("%2.2f"), bKeysDiffer ? 1.f : Volume ) );
		WxDlgGenericStringEntry dlg;
		const INT Result = dlg.ShowModal( TEXT("SetSoundVolume"), TEXT("Volume"), *VolumeStr );
		if( Result == wxID_OK )
		{
			double NewVolume;
			const UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToDouble( &NewVolume );
			if( bIsNumber )
			{
				const FLOAT ClampedNewVolume = ::Clamp( (FLOAT)NewVolume, 0.f, 100.f );
				for ( INT i = 0 ; i < SoundTrackKeyIndices.Num() ; ++i )
				{
					const INT Index						= SoundTrackKeyIndices(i);
					const FInterpEdSelKey& SelKey		= Opt->SelectedKeys(Index);
					UInterpTrack* Track					= SelKey.Track;
					UInterpTrackSound* SoundTrack		= CastChecked<UInterpTrackSound>( Track );
					FSoundTrackKey& SoundTrackKey		= SoundTrack->Sounds(SelKey.KeyIndex);
					SoundTrackKey.Volume				= ClampedNewVolume;
				}
			}
		}

		Interp->MarkPackageDirty();

		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}

/**
 * Prompts the user to edit pitches for the selected sound keys.
 */
void WxInterpEd::OnSetSoundPitch(wxCommandEvent& In)
{
	TArray<INT> SoundTrackKeyIndices;
	UBOOL bFoundPitch = FALSE;
	UBOOL bKeysDiffer = FALSE;
	FLOAT Pitch = 1.0f;

	// Make a list of all keys and what their pitches are.
	for( INT i = 0 ; i < Opt->SelectedKeys.Num() ; ++i )
	{
		const FInterpEdSelKey& SelKey		= Opt->SelectedKeys(i);
		UInterpTrack* Track					= SelKey.Track;
		UInterpTrackSound* SoundTrack		= Cast<UInterpTrackSound>( Track );

		if( SoundTrack )
		{
			SoundTrackKeyIndices.AddItem(i);
			const FSoundTrackKey& SoundTrackKey	= SoundTrack->Sounds(SelKey.KeyIndex);
			if ( !bFoundPitch )
			{
				bFoundPitch = TRUE;
				Pitch = SoundTrackKey.Pitch;
			}
			else
			{
				if ( Abs(Pitch-SoundTrackKey.Pitch) > KINDA_SMALL_NUMBER )
				{
					bKeysDiffer = TRUE;
				}
			}
		}
	}

	if ( SoundTrackKeyIndices.Num() )
	{
		// Display dialog and let user enter new rate.
		const FString PitchStr( FString::Printf( TEXT("%2.2f"), bKeysDiffer ? 1.f : Pitch ) );
		WxDlgGenericStringEntry dlg;
		const INT Result = dlg.ShowModal( TEXT("SetSoundPitch"), TEXT("Pitch"), *PitchStr );
		if( Result == wxID_OK )
		{
			double NewPitch;
			const UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToDouble( &NewPitch );
			if( bIsNumber )
			{
				const FLOAT ClampedNewPitch = ::Clamp( (FLOAT)NewPitch, 0.f, 100.f );
				for ( INT i = 0 ; i < SoundTrackKeyIndices.Num() ; ++i )
				{
					const INT Index						= SoundTrackKeyIndices(i);
					const FInterpEdSelKey& SelKey		= Opt->SelectedKeys(Index);
					UInterpTrack* Track					= SelKey.Track;
					UInterpTrackSound* SoundTrack		= CastChecked<UInterpTrackSound>( Track );
					FSoundTrackKey& SoundTrackKey		= SoundTrack->Sounds(SelKey.KeyIndex);
					SoundTrackKey.Pitch					= ClampedNewPitch;
				}
			}
		}

		Interp->MarkPackageDirty();

		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}



/** Syncs the generic browser to the currently selected sound track key */
void WxInterpEd::OnKeyContext_SyncGenericBrowserToSoundCue( wxCommandEvent& In )
{
	if( Opt->SelectedKeys.Num() > 0 )
	{
		// Does this key have a sound cue set?
		FInterpEdSelKey& SelKey = Opt->SelectedKeys( 0 );
		UInterpTrackSound* SoundTrack = Cast<UInterpTrackSound>( SelKey.Track );
		USoundCue* KeySoundCue = SoundTrack->Sounds( SelKey.KeyIndex ).Sound;
		if( KeySoundCue != NULL )
		{
			TArray< UObject* > Objects;
			Objects.AddItem( KeySoundCue );

			// Sync the generic/content browser!
			GApp->EditorFrame->SyncBrowserToObjects(Objects);
		}
	}
}



/** Called when the user wants to set the master volume on Audio Master track keys */
void WxInterpEd::OnKeyContext_SetMasterVolume( wxCommandEvent& In )
{
	TArray<INT> SoundTrackKeyIndices;
	UBOOL bFoundVolume = FALSE;
	UBOOL bKeysDiffer = FALSE;
	FLOAT Volume = 1.0f;

	// Make a list of all keys and what their volumes are.
	for( INT i = 0 ; i < Opt->SelectedKeys.Num() ; ++i )
	{
		const FInterpEdSelKey& SelKey		= Opt->SelectedKeys(i);
		UInterpTrack* Track					= SelKey.Track;
		UInterpTrackAudioMaster* AudioMasterTrack = Cast<UInterpTrackAudioMaster>( Track );

		if( AudioMasterTrack != NULL )
		{
			// SubIndex 0 = Volume
			const FLOAT CurKeyVolume = AudioMasterTrack->GetKeyOut( 0, SelKey.KeyIndex );

			SoundTrackKeyIndices.AddItem(i);
			if ( !bFoundVolume )
			{
				bFoundVolume = TRUE;
				Volume = CurKeyVolume;
			}
			else
			{
				if ( Abs(Volume-CurKeyVolume) > KINDA_SMALL_NUMBER )
				{
					bKeysDiffer = TRUE;
				}
			}
		}
	}

	if ( SoundTrackKeyIndices.Num() )
	{
		// Display dialog and let user enter new rate.
		const FString VolumeStr( FString::Printf( TEXT("%2.2f"), bKeysDiffer ? 1.f : Volume ) );
		WxDlgGenericStringEntry dlg;
		const INT Result = dlg.ShowModal( TEXT("SetSoundVolume"), TEXT("Volume"), *VolumeStr );
		if( Result == wxID_OK )
		{
			double NewVolume;
			const UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToDouble( &NewVolume );
			if( bIsNumber )
			{
				const FLOAT ClampedNewVolume = ::Clamp( (FLOAT)NewVolume, 0.f, 100.f );
				for ( INT i = 0 ; i < SoundTrackKeyIndices.Num() ; ++i )
				{
					const INT Index						= SoundTrackKeyIndices(i);
					const FInterpEdSelKey& SelKey		= Opt->SelectedKeys(Index);
					UInterpTrack* Track					= SelKey.Track;
					UInterpTrackAudioMaster* AudioMasterTrack = Cast<UInterpTrackAudioMaster>( Track );

					// SubIndex 0 = Volume
					AudioMasterTrack->SetKeyOut( 0, SelKey.KeyIndex, ClampedNewVolume );
				}
			}
		}

		Interp->MarkPackageDirty();

		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}



/** Called when the user wants to set the master pitch on Audio Master track keys */
void WxInterpEd::OnKeyContext_SetMasterPitch( wxCommandEvent& In )
{
	TArray<INT> SoundTrackKeyIndices;
	UBOOL bFoundPitch = FALSE;
	UBOOL bKeysDiffer = FALSE;
	FLOAT Pitch = 1.0f;

	// Make a list of all keys and what their pitches are.
	for( INT i = 0 ; i < Opt->SelectedKeys.Num() ; ++i )
	{
		const FInterpEdSelKey& SelKey		= Opt->SelectedKeys(i);
		UInterpTrack* Track					= SelKey.Track;
		UInterpTrackAudioMaster* AudioMasterTrack = Cast<UInterpTrackAudioMaster>( Track );

		if( AudioMasterTrack != NULL )
		{
			// SubIndex 1 = Pitch
			const FLOAT CurKeyPitch = AudioMasterTrack->GetKeyOut( 1, SelKey.KeyIndex );

			SoundTrackKeyIndices.AddItem(i);
			if ( !bFoundPitch )
			{
				bFoundPitch = TRUE;
				Pitch = CurKeyPitch;
			}
			else
			{
				if ( Abs(Pitch-CurKeyPitch) > KINDA_SMALL_NUMBER )
				{
					bKeysDiffer = TRUE;
				}
			}
		}
	}

	if ( SoundTrackKeyIndices.Num() )
	{
		// Display dialog and let user enter new rate.
		const FString PitchStr( FString::Printf( TEXT("%2.2f"), bKeysDiffer ? 1.f : Pitch ) );
		WxDlgGenericStringEntry dlg;
		const INT Result = dlg.ShowModal( TEXT("SetSoundPitch"), TEXT("Pitch"), *PitchStr );
		if( Result == wxID_OK )
		{
			double NewPitch;
			const UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToDouble( &NewPitch );
			if( bIsNumber )
			{
				const FLOAT ClampedNewPitch = ::Clamp( (FLOAT)NewPitch, 0.f, 100.f );
				for ( INT i = 0 ; i < SoundTrackKeyIndices.Num() ; ++i )
				{
					const INT Index						= SoundTrackKeyIndices(i);
					const FInterpEdSelKey& SelKey		= Opt->SelectedKeys(Index);
					UInterpTrack* Track					= SelKey.Track;
					UInterpTrackAudioMaster* AudioMasterTrack = Cast<UInterpTrackAudioMaster>( Track );

					// SubIndex 1 = Pitch
					AudioMasterTrack->SetKeyOut( 1, SelKey.KeyIndex, ClampedNewPitch );
				}
			}
		}

		Interp->MarkPackageDirty();

		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}



/** Called when the user wants to set the clip ID number for Particle Replay track keys */
void WxInterpEd::OnParticleReplayKeyContext_SetClipIDNumber( wxCommandEvent& In )
{
	if( Opt->SelectedKeys.Num() > 0 )
	{
		const FInterpEdSelKey& FirstSelectedKey = Opt->SelectedKeys( 0 );

		// We only support operating on one key at a time, we'll use the first selected key.
		UInterpTrackParticleReplay* ParticleReplayTrack =
			Cast< UInterpTrackParticleReplay >( FirstSelectedKey.Track );
		if( ParticleReplayTrack != NULL )
		{
			FParticleReplayTrackKey& ParticleReplayKey =
				ParticleReplayTrack->TrackKeys( FirstSelectedKey.KeyIndex );

			WxDlgGenericStringEntry StringEntryDialog;
			const INT DlgResult =
				StringEntryDialog.ShowModal(
					TEXT( "InterpEd_SetParticleReplayKeyClipIDNumber_DialogTitle" ),	// Title
					TEXT( "InterpEd_SetParticleReplayKeyClipIDNumber_DialogCaption" ),	// Caption
					*appItoa( ParticleReplayKey.ClipIDNumber ) );						// Initial value

			if( DlgResult == wxID_OK )
			{
				long NewClipIDNumber;	// 'long', for WxWidgets
				const UBOOL bIsNumber = StringEntryDialog.GetStringEntry().GetValue().ToLong( &NewClipIDNumber );
				if( bIsNumber )
				{
					// Store the new value!
					ParticleReplayKey.ClipIDNumber = NewClipIDNumber;

					// Mark the package as dirty
					Interp->MarkPackageDirty();

					// Refresh Matinee
					RefreshInterpPosition();
				}
			}
		}
	}
}



/** Called when the user wants to set the duration of Particle Replay track keys */
void WxInterpEd::OnParticleReplayKeyContext_SetDuration( wxCommandEvent& In )
{
	if( Opt->SelectedKeys.Num() > 0 )
	{
		const FInterpEdSelKey& FirstSelectedKey = Opt->SelectedKeys( 0 );

		// We only support operating on one key at a time, we'll use the first selected key.
		UInterpTrackParticleReplay* ParticleReplayTrack =
			Cast< UInterpTrackParticleReplay >( FirstSelectedKey.Track );
		if( ParticleReplayTrack != NULL )
		{
			FParticleReplayTrackKey& ParticleReplayKey =
				ParticleReplayTrack->TrackKeys( FirstSelectedKey.KeyIndex );

			WxDlgGenericStringEntry StringEntryDialog;
			const INT DlgResult =
				StringEntryDialog.ShowModal(
					TEXT( "InterpEd_SetParticleReplayKeyDuration_DialogTitle" ),	// Title
					TEXT( "InterpEd_SetParticleReplayKeyDuration_DialogCaption" ),	// Caption
					*FString::Printf( TEXT( "%2.2f" ), ParticleReplayKey.Duration ) );			// Initial value

			if( DlgResult == wxID_OK )
			{
				double NewDuration;		// 'double', for WxWidgets
				const UBOOL bIsNumber = StringEntryDialog.GetStringEntry().GetValue().ToDouble( &NewDuration );
				if( bIsNumber )
				{
					// Store the new value!
					ParticleReplayKey.Duration = ( FLOAT )NewDuration;

					// Mark the package as dirty
					Interp->MarkPackageDirty();

					// Refresh Matinee
					RefreshInterpPosition();
				}
			}
		}
	}
}


	
/** Called to delete the currently selected keys */
void WxInterpEd::OnDeleteSelectedKeys( wxCommandEvent& In )
{
	const UBOOL bWantTransactions = TRUE;
	DeleteSelectedKeys( bWantTransactions );
}



void WxInterpEd::OnContextDirKeyTransitionTime( wxCommandEvent& In )
{
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
	UInterpTrack* Track = SelKey.Track;
	UInterpTrackDirector* DirTrack = Cast<UInterpTrackDirector>(Track);
	if(!DirTrack)
	{
		return;
	}

	FLOAT CurrentTime = DirTrack->CutTrack(SelKey.KeyIndex).TransitionTime;
	const FString CurrentTimeStr = FString::Printf( TEXT("%3.3f"), CurrentTime );

	// Display dialog and let user enter new time.
	WxDlgGenericStringEntry dlg;
	const INT Result = dlg.ShowModal( TEXT("NewTransitionTime"), TEXT("Time"), *CurrentTimeStr);
	if( Result != wxID_OK )
		return;

	double dNewTime;
	const UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToDouble(&dNewTime);
	if(!bIsNumber)
		return;

	const FLOAT NewTime = (FLOAT)dNewTime;

	DirTrack->CutTrack(SelKey.KeyIndex).TransitionTime = NewTime;

	// Update stuff in case doing this has changed it.
	RefreshInterpPosition();
}

void WxInterpEd::OnContextDirKeyRenameCameraShot( wxCommandEvent& In )
{
    if(Opt->SelectedKeys.Num() != 1)
    {
        return;
    }

    FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
    UInterpTrack* Track = SelKey.Track;
    UInterpTrackDirector* DirTrack = Cast<UInterpTrackDirector>(Track);
    if(!DirTrack)
    {
        return;
    }

    
    FString CurrentShot = FString::Printf(TEXT("%d"),DirTrack->CutTrack(SelKey.KeyIndex).ShotNumber);

    // Display dialog and let user enter new time.
    WxDlgGenericStringEntry dlg;
    const INT Result = dlg.ShowModal( TEXT("RenameCameraShotBox"), TEXT("SetNewCameraShotNumber"), *CurrentShot);
    if( Result != wxID_OK )
        return;
    
    ULONG NewShot = 0;    
    const UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToULong(&NewShot);
    if(!bIsNumber)
        return;
    
    DirTrack->CutTrack(SelKey.KeyIndex).ShotNumber = NewShot;
}

void WxInterpEd::OnFlipToggleKey(wxCommandEvent& In)
{
	for (INT KeyIndex = 0; KeyIndex < Opt->SelectedKeys.Num(); KeyIndex++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(KeyIndex);
		UInterpTrack* Track = SelKey.Track;

		UInterpTrackToggle* ToggleTrack = Cast<UInterpTrackToggle>(Track);
		if (ToggleTrack)
		{
			FToggleTrackKey& ToggleKey = ToggleTrack->ToggleTrack(SelKey.KeyIndex);
			ToggleKey.ToggleAction = (ToggleKey.ToggleAction == ETTA_Off) ? ETTA_On : ETTA_Off;
			Track->MarkPackageDirty();
		}

		UInterpTrackVisibility* VisibilityTrack = Cast<UInterpTrackVisibility>(Track);
		if (VisibilityTrack)
		{
			FVisibilityTrackKey& VisibilityKey = VisibilityTrack->VisibilityTrack(SelKey.KeyIndex);
			VisibilityKey.Action = (VisibilityKey.Action == EVTA_Hide) ? EVTA_Show : EVTA_Hide;
			Track->MarkPackageDirty();
		}
	}
}



/** Called when a new key condition is selected in a track keyframe context menu */
void WxInterpEd::OnKeyContext_SetCondition( wxCommandEvent& In )
{
	for (INT KeyIndex = 0; KeyIndex < Opt->SelectedKeys.Num(); KeyIndex++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(KeyIndex);
		UInterpTrack* Track = SelKey.Track;

		UInterpTrackVisibility* VisibilityTrack = Cast<UInterpTrackVisibility>(Track);
		if (VisibilityTrack)
		{
			FVisibilityTrackKey& VisibilityKey = VisibilityTrack->VisibilityTrack(SelKey.KeyIndex);

			switch( In.GetId() )
			{
				case IDM_INTERP_KeyContext_SetCondition_Always:
					VisibilityKey.ActiveCondition = EVTC_Always;
					break;

				case IDM_INTERP_KeyContext_SetCondition_GoreEnabled:
					VisibilityKey.ActiveCondition = EVTC_GoreEnabled;
					break;

				case IDM_INTERP_KeyContext_SetCondition_GoreDisabled:
					VisibilityKey.ActiveCondition = EVTC_GoreDisabled;
					break;
			}

			Track->MarkPackageDirty();
		}
	}
}



void WxInterpEd::OnMenuUndo(wxCommandEvent& In)
{
	InterpEdUndo();
}

void WxInterpEd::OnMenuRedo(wxCommandEvent& In)
{
	InterpEdRedo();
}

/** Menu handler for cut operations. */
void WxInterpEd::OnMenuCut( wxCommandEvent& In )
{
	CopySelectedGroupOrTrack(TRUE);
}

/** Menu handler for copy operations. */
void WxInterpEd::OnMenuCopy( wxCommandEvent& In )
{
	CopySelectedGroupOrTrack(FALSE);
}

/** Menu handler for paste operations. */
void WxInterpEd::OnMenuPaste( wxCommandEvent& In )
{
	PasteSelectedGroupOrTrack();
}

/** Update UI handler for edit menu items. */
void WxInterpEd::OnMenuEdit_UpdateUI( wxUpdateUIEvent& In )
{
	switch(In.GetId())
	{
	case IDM_INTERP_EDIT_UNDO:
		if(InterpEdTrans->CanUndo())
		{
			FString Label = FString::Printf(TEXT("%s %s"), *LocalizeUnrealEd("Undo"), *InterpEdTrans->GetUndoDesc());
			In.SetText(*Label);
			In.Enable(TRUE);
		}
		else
		{
			In.Enable(FALSE);
		}
		break;
	case IDM_INTERP_EDIT_REDO:
		if(InterpEdTrans->CanRedo())
		{
			FString Label = FString::Printf(TEXT("%s %s"), *LocalizeUnrealEd("Redo"), *InterpEdTrans->GetRedoDesc());
			In.SetText(*Label);
			In.Enable(TRUE);
		}
		else
		{
			In.Enable(FALSE);
		}
		break;
	case IDM_INTERP_EDIT_PASTE:
		{
			UBOOL bCanPaste = CanPasteGroupOrTrack();
			In.Enable(bCanPaste==TRUE);
		}
		break;
	}
}

void WxInterpEd::OnMenuImport( wxCommandEvent& )
{
#if WITH_FBX
	if( Interp != NULL )
	{
		WxFileDialog ImportFileDialog(this,
							*LocalizeUnrealEd("ImportMatineeSequence"),
							*(GApp->LastDir[LD_GENERIC_IMPORT]),
							TEXT(""),
							TEXT("FBX document|*.fbx|All document|*.*"),
							wxOPEN | wxFILE_MUST_EXIST,
							wxDefaultPosition);

		// Show dialog and execute the import if the user did not cancel out
		if( ImportFileDialog.ShowModal() == wxID_OK )
		{
			// Get the filename from dialog
			wxString ImportFilename = ImportFileDialog.GetPath();
			FFilename FileName = ImportFilename.c_str();
			GApp->LastDir[LD_GENERIC_IMPORT] = FileName.GetPath(); // Save path as default for next time.
		
			const FString FileExtension = FileName.GetExtension();
			const UBOOL bIsFBX = appStricmp(*FileExtension, TEXT("FBX")) == 0;
		
			if (bIsFBX)
			{
				// Import the Matinee information from the FBX document.
				UnFbx::CFbxImporter* FbxImporter = UnFbx::CFbxImporter::GetInstance();
				if (FbxImporter->ImportFromFile(ImportFilename.c_str()))
				{
					FbxImporter->SetProcessUnknownCameras(FALSE);
					
					if ( FbxImporter->HasUnknownCameras( Interp ) )
					{
						// Ask the user whether to create any missing cameras.
						int LResult = wxMessageBox(*LocalizeUnrealEd("ImportMatineeSequence_MissingCameras"), *LocalizeUnrealEd("ImportMatineeSequence"), wxICON_QUESTION | wxYES_NO | wxCENTRE);
						FbxImporter->SetProcessUnknownCameras(LResult == wxYES);
					}

					// Re-create the Matinee sequence.
					FbxImporter->ImportMatineeSequence(Interp);

					// We have modified the sequence, so update its UI.
					NotifyPostChange(NULL, NULL);
				}
				FbxImporter->ReleaseScene();
			}
			else
			{
				// Invalid filename 
			}
		}
	}
#else
	appMsgf( AMT_OK, TEXT( "FBX support must be enabled to import Matinee sequences. See the WITH_FBX build definition." ) );
#endif
}

void WxInterpEd::OnMenuExport( wxCommandEvent& In )
{
#if WITH_FBX
	if( Interp != NULL )
	{
		WxFileDialog ExportFileDialog(this, 
			*LocalizeUnrealEd("ExportMatineeSequence"), 
			*(GApp->LastDir[LD_GENERIC_EXPORT]), 
			TEXT(""), 
			TEXT("FBX document|*.fbx"),
			wxSAVE | wxOVERWRITE_PROMPT, 
			wxDefaultPosition);

		// Show dialog and execute the import if the user did not cancel out
		if( ExportFileDialog.ShowModal() == wxID_OK )
		{
			// Get the filename from dialog
			wxString ExportFilename = ExportFileDialog.GetPath();
			FFilename FileName = ExportFilename.c_str();
			GApp->LastDir[LD_GENERIC_EXPORT] = FileName.GetPath(); // Save path as default for next time.

			const FString FileExtension = FileName.GetExtension();
			const UBOOL bIsFBX = appStricmp(*FileExtension, TEXT("FBX")) == 0;
			
			MatineeExporter* Exporter = NULL;

			if (bIsFBX)
			{
				Exporter = UnFbx::CFbxExporter::GetInstance();

				// Export the Matinee information to an FBX file
				Exporter->CreateDocument();
				Exporter->SetTrasformBaking(bBakeTransforms);

				// Export the persistent level and all of it's actors
				Exporter->ExportLevelMesh(GWorld->PersistentLevel, Interp );

				// Export streaming levels and actors
				for( INT CurLevelIndex = 0; CurLevelIndex < GWorld->Levels.Num(); ++CurLevelIndex )
				{
					ULevel* CurLevel = GWorld->Levels( CurLevelIndex );
					if( CurLevel != NULL && CurLevel != GWorld->PersistentLevel )
					{
						Exporter->ExportLevelMesh( CurLevel, Interp );
					}
				}

				// Export Matinee
				Exporter->ExportMatinee( Interp );

				// Save to disk
				Exporter->WriteToFile( ExportFilename.c_str() );
			}
			else
			{
				// Invalid file
			}
			
		}
	}
#else
	appMsgf( AMT_OK, TEXT( "FBX support must be enabled to export Matinee sequences. See the WITH_FBX build definition." ) );
#endif
}



void WxInterpEd::OnExportSoundCueInfoCommand( wxCommandEvent& )
{
	if( Interp == NULL )
	{
		return;
	}


	WxFileDialog ExportFileDialog(
		this,
		*LocalizeUnrealEd( "InterpEd_ExportSoundCueInfoDialogTitle" ),
		*(GApp->LastDir[LD_GENERIC_EXPORT]),
		TEXT( "" ),
		TEXT( "CSV file|*.csv" ),
		wxSAVE | wxOVERWRITE_PROMPT,
		wxDefaultPosition );

	// Show dialog and execute the import if the user did not cancel out
	if( ExportFileDialog.ShowModal() == wxID_OK )
	{
		// Get the filename from dialog
		wxString ExportFilename = ExportFileDialog.GetPath();
		FFilename FileName = ExportFilename.c_str();

		// Save path as default for next time.
		GApp->LastDir[ LD_GENERIC_EXPORT ] = FileName.GetPath();

		
		FArchive* CSVFile = GFileManager->CreateFileWriter( *FileName );
		if( CSVFile != NULL )
		{
			// Write header
			{
				FString TextLine( TEXT( "Group,Track,SoundCue,Time,Frame,Anim,AnimTime,AnimFrame" ) LINE_TERMINATOR );
				CSVFile->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() );
			}

			for( INT CurGroupIndex = 0; CurGroupIndex < Interp->InterpData->InterpGroups.Num(); ++CurGroupIndex )
			{
				const UInterpGroup* CurGroup = Interp->InterpData->InterpGroups( CurGroupIndex );
				if( CurGroup != NULL )
				{
					for( INT CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
					{
						const UInterpTrack* CurTrack = CurGroup->InterpTracks( CurTrackIndex );
						if( CurTrack != NULL )
						{
							const UInterpTrackSound* SoundTrack = ConstCast< UInterpTrackSound >( CurTrack );
							if( SoundTrack != NULL )
							{
								for( INT CurSoundIndex = 0; CurSoundIndex < SoundTrack->Sounds.Num(); ++CurSoundIndex )
								{
									const FSoundTrackKey& CurSound = SoundTrack->Sounds( CurSoundIndex );
									if( CurSound.Sound != NULL )
									{
										FString FoundAnimName;
										FLOAT FoundAnimTime = 0.0f;

										// Search for an animation track in this group that overlaps this sound's start time
										for( TArrayNoInit< UInterpTrack* >::TConstIterator TrackIter( CurGroup->InterpTracks ); TrackIter != NULL; ++TrackIter )
										{
											const UInterpTrackAnimControl* AnimTrack = ConstCast< UInterpTrackAnimControl >( *TrackIter );
											if( AnimTrack != NULL )
											{
												// Iterate over animations in this anim track
												for( TArrayNoInit< FAnimControlTrackKey >::TConstIterator AnimKeyIter( AnimTrack->AnimSeqs ); AnimKeyIter != NULL; ++AnimKeyIter )
												{
													const FAnimControlTrackKey& CurAnimKey = *AnimKeyIter;

													// Does this anim track overlap the sound's start time?
													if( CurSound.Time >= CurAnimKey.StartTime )
													{
														FoundAnimName = CurAnimKey.AnimSeqName.ToString();
														
														// Compute the time the sound exists at within this animation
														FoundAnimTime = ( CurSound.Time - CurAnimKey.StartTime ) + CurAnimKey.AnimStartOffset;

														// NOTE: The array is ordered, so we'll take the LAST anim we find that overlaps the sound!
													}
												}
											}
										}


										// Also store values as frame numbers instead of time values if a frame rate is selected
										const INT SoundFrameIndex = bSnapToFrames ? appTrunc( CurSound.Time / SnapAmount ) : 0;

										FString TextLine = FString::Printf(
											TEXT( "%s,%s,%s,%0.2f,%i" ),
											*CurGroup->GroupName.ToString(),
											*CurTrack->TrackTitle,
											*CurSound.Sound->GetName(),
											CurSound.Time,
											SoundFrameIndex );

										// Did we find an animation that overlaps this sound?  If so, we'll emit that info
										if( FoundAnimName.Len() > 0 )
										{
											// Also store values as frame numbers instead of time values if a frame rate is selected
											const INT AnimFrameIndex = bSnapToFrames ? appTrunc( FoundAnimTime / SnapAmount ) : 0;

											TextLine += FString::Printf(
												TEXT( ",%s,%.2f,%i" ),
												*FoundAnimName,
												FoundAnimTime,
												AnimFrameIndex );
										}

										TextLine += LINE_TERMINATOR;

										CSVFile->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() );
									}
								}
							}
						}
					}
				}
			}

			// Close and delete archive
			CSVFile->Close();
			delete CSVFile;
		}
		else
		{
			debugf( NAME_Warning, TEXT("Could not create CSV file %s for writing."), *FileName );
		}
	}
}

void WxInterpEd::OnExportAnimationInfoCommand( wxCommandEvent& )
{
	if( Interp == NULL )
	{
		return;
	}

	UInterpData* InterpData = Interp->InterpData;
			
	//Get Our File Name from the Obj Comment
	FString MatineeComment = TEXT("");
	FString FileName = TEXT("");
	
	MatineeComment = Interp->ObjComment;
	FileName = FString::Printf(TEXT("MatineeAnimInfo%s"),*MatineeComment);
	//remove whitespaces
	FileName = FileName.Replace(TEXT(" "),TEXT(""));

	WxFileDialog ExportFileDialog(
		this,
		*LocalizeUnrealEd( "InterpEd_ExportSoundCueInfoDialogTitle" ), //~D add a new file to INT
		*(GApp->LastDir[LD_GENERIC_EXPORT]),
		*FileName,
		TEXT( "Text file|*.txt" ),
		wxSAVE | wxOVERWRITE_PROMPT,
		wxDefaultPosition );

	// Show dialog and execute the import if the user did not cancel out
	if( ExportFileDialog.ShowModal() == wxID_OK )
	{
		// Get the filename from dialog
		wxString ExportFilename = ExportFileDialog.GetPath();
		FFilename FileName = ExportFilename.c_str();

		// Save path as default for next time.
		GApp->LastDir[ LD_GENERIC_EXPORT ] = FileName.GetPath();

		
		FArchive* File = GFileManager->CreateFileWriter( *FileName );
		if( File != NULL )
		{	
			FString TextLine;
			
			//Header w/Comment	
			TextLine = FString::Printf(TEXT("Matinee Animation Data Export%s"),LINE_TERMINATOR);
			TextLine += FString::Printf(TEXT("Comment: %s%s"),*MatineeComment,LINE_TERMINATOR);
			TextLine += LINE_TERMINATOR;
			File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() );	
			
			
			// Director Track Data...
			UInterpGroupDirector* DirGroup = Interp->FindDirectorGroup();
			UInterpTrackDirector* DirTrack = DirGroup ? DirGroup->GetDirectorTrack() : NULL;
			
			TextLine = FString::Printf(TEXT("Director:%s"),LINE_TERMINATOR);
			File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() );
			if ( DirTrack && DirTrack->CutTrack.Num() > 0 )
			{
				//Keys
				for( INT KeyFrameIndex=0; KeyFrameIndex < DirTrack->CutTrack.Num(); KeyFrameIndex++)
				{
					const FDirectorTrackCut& Cut = DirTrack->CutTrack(KeyFrameIndex);
					
					FLOAT Time = Cut.Time;
					FString TargetCamGroup = Cut.TargetCamGroup.ToString();
					
					FString ShotName = DirTrack->GetFormattedCameraShotName(KeyFrameIndex);

					TextLine = FString::Printf(TEXT("\tKeyFrame: %d,\tTime: %.2f,\tCameraGroup: %s,\tShot: %s%s"),KeyFrameIndex,Time,*TargetCamGroup,*ShotName,LINE_TERMINATOR);
					File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() ); 								
				}
			}
			else
			{
				TextLine = FString::Printf(TEXT("\t(No Director Track Data)%s"),LINE_TERMINATOR);
				File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() ); 			
			}
			
			//Anim Group/Track Data
			UBOOL bAnimDataFound = FALSE;
			if(InterpData)
			{
				//Groups
				for(INT GroupIndex=0; GroupIndex < InterpData->InterpGroups.Num(); GroupIndex++)
				{
					UInterpGroup* Group = InterpData->InterpGroups(GroupIndex);
					
					//check for any animation tracks...
					TArray<UInterpTrack*> AnimTracks; //= Cast<UInterpTrackAnimControl>(Track);
					Group->FindTracksByClass(UInterpTrackAnimControl::StaticClass(),AnimTracks);
					if (AnimTracks.Num() > 0)
					{
						TextLine = LINE_TERMINATOR;
						File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() ); 	
					
						FString GroupName = Group->GroupName.ToString();
						TextLine = FString::Printf(TEXT("Group: %s%s"),*GroupName,LINE_TERMINATOR);	
						File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() ); 	
						bAnimDataFound = TRUE;										
					}
					//Tracks
					for (INT TrackIndex=0; TrackIndex < AnimTracks.Num(); TrackIndex++)
					{		
						UInterpTrackAnimControl *Track = Cast<UInterpTrackAnimControl>(AnimTracks(TrackIndex));
						FString TrackName = Track->TrackTitle;
						TextLine = FString::Printf(TEXT("\tTrack: %s%s"),*TrackName,LINE_TERMINATOR);	
						File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() );
						
						//Keys 				
						for( INT KeyFrameIndex=0; KeyFrameIndex < Track->AnimSeqs.Num(); KeyFrameIndex++)
						{
							//animation controls					
							FLOAT Time = Track->GetKeyframeTime(KeyFrameIndex);
							FAnimControlTrackKey &Key = Track->AnimSeqs(KeyFrameIndex);
							FString AnimSeqName = Key.AnimSeqName.ToString();
							
							FLOAT AnimStartTime = Key.AnimStartOffset;
							UAnimSequence* Seq = Track->FindAnimSequenceFromName(Key.AnimSeqName);
							FLOAT AnimEndTime = (Seq) ? (Seq->SequenceLength - Key.AnimEndOffset) : 0.0f;
					
							FLOAT AnimPlayRate = Key.AnimPlayRate;
							UBOOL bLooping = Key.bLooping;
							UBOOL bReverse = Key.bReverse;
						
							TextLine = FString::Printf(
								TEXT("\t\tKeyFrame: %d,\tTime: %.2f,"),
								KeyFrameIndex,
								Time,
								LINE_TERMINATOR);
							//do a bit of formatting to clean up our file
							AnimSeqName += TEXT(",");
							AnimSeqName = AnimSeqName.RightPad(20);
							TextLine += FString::Printf(
								TEXT("\tSequence: %s\tAnimStart: %.2f,\tAnimEnd: %.2f,\tPlayRate: %.2f,\tLoop:%d, Reverse:%d%s"),
								*AnimSeqName,
								AnimStartTime,
								AnimEndTime,
								AnimPlayRate,
								bLooping,
								bReverse,
								LINE_TERMINATOR);
							File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() ); 					
						}
						
						TextLine = LINE_TERMINATOR;
						File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() ); 				
					}							
				}
			}
			
			if (!bAnimDataFound)
			{
				TextLine = LINE_TERMINATOR;
				File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() );
				TextLine = FString::Printf(TEXT("(No Animation Data)%s"),LINE_TERMINATOR);
				File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() ); 		
			}
									

			// Close and delete archive
			File->Close();
			delete File;
		}
	}
}

/**
 * Called when the user toggles the ability to export a key every frame. 
 *
 * @param	In	The wxWidgets event sent when the user clicks on the "Bake Transforms On Export" menu item.
 */
void WxInterpEd::OnToggleBakeTransforms(wxCommandEvent& In)
{
	bBakeTransforms = !bBakeTransforms;
}

/**
 * Updates the checked-menu item for baking transforms
 *
 * @param	In	The wxWidgets event sent when the user clicks on the "Bake Transforms On Export" menu item.
 */
void WxInterpEd::OnToggleBakeTransforms_UpdateUI( wxUpdateUIEvent& In )
{
	In.Check( bBakeTransforms == TRUE );
}

void WxInterpEd::OnMenuReduceKeys(wxCommandEvent& In)
{
	ReduceKeys();
}


void WxInterpEd::OnToggleCurveEd(wxCommandEvent& In)
{
	// Check to see if the Curve Editor is currently visible
	UBOOL bCurveEditorVisible = FALSE;

	FDockingParent::FDockWindowState CurveEdDockState;
	if( GetDockingWindowState( CurveEd, CurveEdDockState ) )
	{
		bCurveEditorVisible = CurveEdDockState.bIsVisible;
	}

	// Toggle the curve editor
	if( CurveEdToggleMenuItem != NULL )
	{
		CurveEdToggleMenuItem->Toggle();
	}
	bCurveEditorVisible = !bCurveEditorVisible;

	// Now actually show or hide the window
	ShowDockingWindow( CurveEd, bCurveEditorVisible );

	// Update button status.
	ToolBar->ToggleTool( IDM_INTERP_TOGGLE_CURVEEDITOR, bCurveEditorVisible ? TRUE : FALSE );
	MenuBar->ViewMenu->Check( IDM_INTERP_TOGGLE_CURVEEDITOR, bCurveEditorVisible ? TRUE : FALSE );

	// OK, we need to make sure our track window's scroll bar gets repositioned correctly if the docking
	// layout changes
	if( TrackWindow != NULL )
	{
		TrackWindow->UpdateWindowLayout();
	}
	if( DirectorTrackWindow != NULL )
	{
		DirectorTrackWindow->UpdateWindowLayout();
	}
}

/** Toggles interting of the panning the interp editor left and right */
void WxInterpEd::OnToggleInvertPan( wxCommandEvent& In )
{
	UBOOL bInvertPan = GEditorModeTools().GetInterpPanInvert();

	bInvertPan = !bInvertPan;

	MenuBar->ViewMenu->Check( IDM_INTERP_INVERT_PAN, bInvertPan ? TRUE : FALSE );
	GEditorModeTools().SetInterpPanInvert(bInvertPan);
}

/** Called when split translation and rotation is selected from a movement track context menu */
void WxInterpEd::OnSplitTranslationAndRotation( wxCommandEvent& In )
{
	check( HasATrackSelected() );

	ClearKeySelection();
	// Check to make sure there is a movement track in list
	// before attempting to start the transaction system.
	if( HasATrackSelected( UInterpTrackMove::StaticClass() ) )
	{
		InterpEdTrans->BeginSpecial( *LocalizeUnrealEd( "InterpEd_Undo_SplitTranslationAndRotation" ) );

		Interp->Modify();
		IData->Modify();

		TTrackClassTypeIterator<UInterpTrackMove> MoveTrackIt(GetSelectedTrackIterator<UInterpTrackMove>());
		for( ; MoveTrackIt; ++MoveTrackIt )
		{
			// Grab the movement track for the selected group
			UInterpTrackMove* MoveTrack = *MoveTrackIt;
			MoveTrack->Modify();
			// Remove from the Curve editor, if its there.
			IData->CurveEdSetup->RemoveCurve(MoveTrack);
			DeselectTrack( (UInterpGroup*)MoveTrack->GetOuter(), MoveTrack );
			MoveTrack->SplitTranslationAndRotation();

		}

		InterpEdTrans->EndSpecial();

		UpdateTrackWindowScrollBars();
	}
}


class WxInterpEditorNormalizeVelocity : public wxDialog
{
public:
	WxInterpEditorNormalizeVelocity(wxWindow* InParent, FLOAT IntervalMinimum, FLOAT IntervalMaximum);
	~WxInterpEditorNormalizeVelocity();

	// The return values of the dialog.
	FLOAT IntervalStart;
	FLOAT IntervalEnd;
	UBOOL bFullInterval;
private:
	// The dialog controls.
	wxStaticText* FullIntervalLabel;
	wxStaticText* IntervalStartLabel;
	wxStaticText* IntervalEndLabel;
	wxCheckBox* FullIntervalCheckBox;
	wxTextCtrl* IntervalStartControl;
	wxTextCtrl* IntervalEndControl;
	wxButton* OkayButton;
	wxButton* CancelButton;

	// The dialog control event-handlers.
	void OnOkay(wxCommandEvent& In);
	void OnCancel(wxCommandEvent& In);

	DECLARE_EVENT_TABLE()
};



BEGIN_EVENT_TABLE(WxInterpEditorNormalizeVelocity, wxDialog)
	EVT_BUTTON(wxID_OK, WxInterpEditorNormalizeVelocity::OnOkay)
	EVT_BUTTON(wxID_CANCEL, WxInterpEditorNormalizeVelocity::OnCancel)
END_EVENT_TABLE()


WxInterpEditorNormalizeVelocity::WxInterpEditorNormalizeVelocity(wxWindow* InParent, FLOAT IntervalMinimum, FLOAT IntervalMaximum) :
	wxDialog(InParent, wxID_ANY, TEXT("DlgInterpEditorNormalizeVelocityTitle"), wxDefaultPosition, wxSize(203, 138), wxCAPTION),
	bFullInterval(TRUE), 
	IntervalStart(IntervalMinimum), 
	IntervalEnd(IntervalMaximum),
	FullIntervalLabel(NULL), 
	IntervalStartLabel(NULL),
	IntervalEndLabel(NULL),
	FullIntervalCheckBox(NULL), 
	IntervalStartControl(NULL), 
	IntervalEndControl(NULL),
	OkayButton(NULL),
	CancelButton(NULL)
{
	static const UINT RowHeight = 18;

	IntervalStartLabel = new wxStaticText(this, wxID_ANY, TEXT("DlgInterpEditorIntervalStart"), wxPoint(3, 3), wxSize(138, RowHeight));
	IntervalEndLabel = new wxStaticText(this, wxID_ANY, TEXT("DlgInterpEditorIntervalEnd"), wxPoint(3, 5 + RowHeight), wxSize(138, RowHeight));
	FullIntervalLabel = new wxStaticText(this, wxID_ANY, TEXT("DlgInterpEditorFullInterval"), wxPoint(3, 7 + RowHeight * 2), wxSize(138, RowHeight));

	IntervalStartControl = new wxTextCtrl(this, wxID_ANY, TEXT(""), wxPoint(141, 5), wxSize(58, RowHeight), wxCHK_2STATE);
	IntervalEndControl = new wxTextCtrl(this, wxID_ANY, TEXT(""), wxPoint(141, 7 + RowHeight), wxSize(58, RowHeight));
	FullIntervalCheckBox = new wxCheckBox(this, wxID_ANY, TEXT(""), wxPoint(141, 9 + RowHeight * 2), wxSize(58, RowHeight));

	OkayButton = new wxButton(this, wxID_OK, TEXT("OK"), wxPoint(82, 13 + RowHeight * 4), wxSize(48, 25));
	CancelButton = new wxButton(this, wxID_CANCEL, TEXT("Cancel"), wxPoint(134, 13 + RowHeight * 4), wxSize(64, 25));

	FLocalizeWindow( this );
	FWindowUtil::LoadPosSize( TEXT("DlgInterpEditorNormalizeVelocityTitle"), this );
	Layout();
	OkayButton->SetDefault();
	SetDefaultItem(OkayButton);

	FullIntervalLabel->SetWindowStyle(wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
	IntervalStartLabel->SetWindowStyle(wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
	IntervalEndLabel->SetWindowStyle(wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);

	TCHAR StringValue[MAX_SPRINTF]=TEXT("");
	appSprintf(StringValue, TEXT("%.2f"), IntervalMinimum);
	IntervalStartControl->SetValue(StringValue);
	appSprintf(StringValue, TEXT("%.2f"), IntervalMaximum);
	IntervalEndControl->SetValue(StringValue);
	FullIntervalCheckBox->SetValue(true);
}

WxInterpEditorNormalizeVelocity::~WxInterpEditorNormalizeVelocity()
{
}

void WxInterpEditorNormalizeVelocity::OnOkay(wxCommandEvent& In)
{
	// Parse the dialog control values.
	bFullInterval = (UBOOL) FullIntervalCheckBox->GetValue();
	IntervalStart = appAtof(IntervalStartControl->GetValue().c_str());
	IntervalEnd = appAtof(IntervalEndControl->GetValue().c_str());

	wxDialog::EndDialog(TRUE);
}

void WxInterpEditorNormalizeVelocity::OnCancel(wxCommandEvent& In)
{
	wxDialog::EndDialog(FALSE);
}


/** 
 * Reparameterizes the curve in the passed in movement track in terms of arc length.  (For constant velocity)
 * 
 * @param	InMoveTrack	The move track containing curves to reparameterize
 * @param	StartTime	The start of the time range to reparameterize
 * @param	EndTime		The end of the time range to reparameterize
 * @param	OutReparameterizedCurve	The resulting reparameterized curve
 */
static FLOAT ReparameterizeCurve( const UInterpTrackMove* InMoveTrack, FLOAT StartTime, FLOAT EndTime, FInterpCurveFloat& OutReparameterizedCurve )
{
	//@todo Should really be adaptive
	const INT NumSteps = 500;

	// Clear out any existing points
	OutReparameterizedCurve.Reset();

	// This should only be called on split tracks
	check( InMoveTrack->SubTracks.Num() > 0 );

	// Get each curve
	const FInterpCurveFloat& XAxisCurve = CastChecked<UInterpTrackMoveAxis>( InMoveTrack->SubTracks(AXIS_TranslationX) )->FloatTrack;
	const FInterpCurveFloat& YAxisCurve = CastChecked<UInterpTrackMoveAxis>( InMoveTrack->SubTracks(AXIS_TranslationY) )->FloatTrack;
	const FInterpCurveFloat& ZAxisCurve = CastChecked<UInterpTrackMoveAxis>( InMoveTrack->SubTracks(AXIS_TranslationZ) )->FloatTrack;

	// Current time should start at the passed in start time
	FLOAT CurTime = StartTime;
	// Determine the amount of time to step
	const FLOAT Interval = (EndTime - CurTime)/((FLOAT)(NumSteps-1)); 

	// Add first entry, using first point on curve, total distance will be 0
	FVector StartPos = FVector(0.f);
	StartPos.X = XAxisCurve.Eval( CurTime, 0.f );
	StartPos.Y = YAxisCurve.Eval( CurTime, 0.f );
	StartPos.Z = ZAxisCurve.Eval( CurTime, 0.f );

	FLOAT TotalLen = 0.f;
	OutReparameterizedCurve.AddPoint( TotalLen, CurTime );

	// Increment time past the first entry
	CurTime += Interval;

	// Iterate over the curve
	for(INT i=1; i<NumSteps; i++)
	{
		// Determine the length of this segment.
		FVector NewPos = FVector(0.f);
		NewPos.X = XAxisCurve.Eval( CurTime, 0.f );
		NewPos.Y = YAxisCurve.Eval( CurTime, 0.f );
		NewPos.Z = ZAxisCurve.Eval( CurTime, 0.f );

		// Add the total length of this segment to the current total length.
		TotalLen += (NewPos - StartPos).Size();

		// Set up the start pos for the next segment to be the end of this segment
		StartPos = NewPos;

		// Add a new entry in the reparmeterized curve.
		OutReparameterizedCurve.AddPoint( TotalLen, CurTime );

		// Increment time
		CurTime += Interval;
	}

	return TotalLen;
}


/**
 * Removes keys from the specified move track if they are within the specified time range
 * 
 * @param Track	Track to remove keys from
 * @param StartTime The start of the time range.
 * @param End		The end of the time range
*/
static void ClearKeysInTimeRange( UInterpTrackMoveAxis* Track, FLOAT StartTime, FLOAT EndTime )
{
	for( INT KeyIndex = Track->FloatTrack.Points.Num() - 1; KeyIndex >= 0 ; --KeyIndex )
	{
		FLOAT KeyTime = Track->FloatTrack.Points( KeyIndex ).InVal;
		if( KeyTime >= StartTime && KeyTime <= EndTime )
		{
			// This point is in the time range, remove it.
			Track->FloatTrack.Points.Remove( KeyIndex );
			// Since there must be an equal number of lookup track keys we must remove the key from the lookup track at the same index.
			Track->LookupTrack.Points.Remove( KeyIndex );
		}
	}
}

/** Called when a user selects the normalize velocity option on a movement track */
void WxInterpEd::NormalizeVelocity( wxCommandEvent& In )
{
	check( HasATrackSelected( UInterpTrackMove::StaticClass() ) );

	UInterpTrackMove* MoveTrack = *GetSelectedTrackIterator<UInterpTrackMove>();

	if( MoveTrack->SubTracks.Num() > 0 )
	{
		// Find the group instance for this move track 
		UInterpGroup* Group = MoveTrack->GetOwningGroup();
		UInterpGroupInst* GrInst = Interp->FindFirstGroupInst( Group );
		check(GrInst);

		// Find the track instance which is needed to reduce keys
		UInterpTrackInst* TrackInst = NULL;
		INT Index = Group->InterpTracks.FindItemIndex( MoveTrack );
		check(Index != INDEX_NONE);

		TrackInst = GrInst->TrackInst( Index );
		check(TrackInst);

		// Get this movment track's subtracks.
		UInterpTrackMoveAxis* XAxisTrack = CastChecked<UInterpTrackMoveAxis>( MoveTrack->SubTracks(AXIS_TranslationX) );
		UInterpTrackMoveAxis* YAxisTrack = CastChecked<UInterpTrackMoveAxis>( MoveTrack->SubTracks(AXIS_TranslationY) );
		UInterpTrackMoveAxis* ZAxisTrack = CastChecked<UInterpTrackMoveAxis>( MoveTrack->SubTracks(AXIS_TranslationZ) );

		// Get each curve.
		FInterpCurveFloat& XAxisCurve = XAxisTrack->FloatTrack;
		FInterpCurveFloat& YAxisCurve = YAxisTrack->FloatTrack;
		FInterpCurveFloat& ZAxisCurve = ZAxisTrack->FloatTrack;
		
		// The start and end time of the segment we are modifying
		FLOAT SegmentStartTime = 0.0f;
		FLOAT SegmentEndTime = 0.0f;

		// The start and end time of the full track lenth
		FLOAT FullStartTime = 0.0f;
		FLOAT FullEndTime = 0.0f;

		// Get the full time range
		MoveTrack->GetTimeRange( FullStartTime, FullEndTime );

		// Prompt the user for the section we should normalize.
		WxInterpEditorNormalizeVelocity Dialog( this, FullStartTime, FullEndTime );
		INT Result = Dialog.ShowModal();
		if( Result == TRUE )
		{
			SegmentStartTime = Dialog.IntervalStart;
			SegmentEndTime = Dialog.IntervalEnd;
			
			// Make sure the user didnt enter any invalid values
			Clamp(SegmentStartTime, FullStartTime, FullEndTime );
			Clamp(SegmentEndTime, FullStartTime, FullEndTime );

			// If we have a valid start and end time, normalize the track
			if( SegmentStartTime != SegmentEndTime )
			{
				InterpEdTrans->BeginSpecial( TEXT("NormalizeVelocity") );

				FInterpCurveInitFloat ReparameterizedCurve;
				FLOAT TotalLen = ReparameterizeCurve( MoveTrack, FullStartTime, FullEndTime, ReparameterizedCurve );

				MoveTrack->Modify();
				XAxisTrack->Modify();
				YAxisTrack->Modify();
				ZAxisTrack->Modify();
			
				const FLOAT TotalTime = FullEndTime - FullStartTime;
				const INT NumSteps = appCeil(TotalTime/(1.0f/60.0f));
				const FLOAT Interval = (SegmentEndTime-SegmentStartTime)/NumSteps; 

				// An array of points that were created in order to normalize velocity
				TArray< FInterpCurvePoint<FVector> > CreatedPoints;

				FLOAT Time = SegmentStartTime;
				for( INT Step = 0; Step < NumSteps; ++Step )
				{
					// Determine how far along the curve we should be at the given time.
					FLOAT PctDone = Time/TotalTime;
					FLOAT TotalDistSoFar = TotalLen * PctDone;

					// Given the total distance along the curve that has been traversed so far, find the actual time where we should evaluate the original curve.
					FLOAT NewTime = ReparameterizedCurve.Eval( TotalDistSoFar, 0.f );

					// Evaluate the curve given the new time and create a new point
					FInterpCurvePoint<FVector> Point;
					Point.InVal = Time;
					Point.OutVal.X = XAxisCurve.Eval( NewTime, 0.0f );
					Point.OutVal.Y = YAxisCurve.Eval( NewTime, 0.0f );
					Point.OutVal.Z = ZAxisCurve.Eval( NewTime, 0.0f );
					Point.InterpMode = CIM_CurveAuto;
					Point.ArriveTangent = FVector(0.0f);
					Point.LeaveTangent = FVector(0.0f);

					CreatedPoints.AddItem( Point );

					// Increment time
					Time+=Interval;
				}

				// default name for lookup track keys
				FName DefaultName(NAME_None);

				// If we didnt start at the beginning add a key right before the modification.  This preserves the part we dont modify.
				if( SegmentStartTime > FullStartTime )
				{
					FLOAT KeyTime = SegmentStartTime - 0.01f;

					FInterpCurvePoint<FVector> PointToAdd;
					PointToAdd.InVal = KeyTime;
					PointToAdd.OutVal.X = XAxisCurve.Eval( KeyTime, 0.0f );
					PointToAdd.OutVal.Y = YAxisCurve.Eval( KeyTime, 0.0f );
					PointToAdd.OutVal.Z = ZAxisCurve.Eval( KeyTime, 0.0f );
					PointToAdd.ArriveTangent = FVector(0.0f);
					PointToAdd.LeaveTangent = PointToAdd.ArriveTangent;
					PointToAdd.InterpMode = CIM_CurveAuto;
	
					CreatedPoints.AddItem( PointToAdd );
				}

				// If we didnt stop at the end of the track add a key right after the modification.  This preserves the part we dont modify.
				if( SegmentEndTime < FullEndTime )
				{
					FLOAT KeyTime = SegmentEndTime + 0.01f;

					FInterpCurvePoint<FVector> PointToAdd;
					PointToAdd.InVal = KeyTime;
					PointToAdd.OutVal.X = XAxisCurve.Eval( KeyTime, 0.0f );
					PointToAdd.OutVal.Y = YAxisCurve.Eval( KeyTime, 0.0f );
					PointToAdd.OutVal.Z = ZAxisCurve.Eval( KeyTime, 0.0f );
					PointToAdd.ArriveTangent = FVector(0.0f);
					PointToAdd.LeaveTangent = PointToAdd.ArriveTangent;
					PointToAdd.InterpMode = CIM_CurveAuto;

					CreatedPoints.AddItem( PointToAdd );
				}

				// Empty all points in the time range for each axis curve.  Normalized velocity means the original points are now invalid.
				ClearKeysInTimeRange( XAxisTrack, SegmentStartTime, SegmentEndTime );
				ClearKeysInTimeRange( YAxisTrack, SegmentStartTime, SegmentEndTime );
				ClearKeysInTimeRange( ZAxisTrack, SegmentStartTime, SegmentEndTime );

				// Add each created point to each curve
				for( INT I = 0; I < CreatedPoints.Num(); ++I )
				{
					// Created points are vectors so we must split them into their individual components.
					const FInterpCurvePoint<FVector>& CreatedPoint = CreatedPoints(I);

					// X Axis
					{
						INT Index = XAxisCurve.AddPoint( CreatedPoint.InVal, CreatedPoint.OutVal.X );
						FInterpCurvePoint<FLOAT>& AddedPoint = XAxisCurve.Points( Index );

						AddedPoint.InterpMode = CreatedPoint.InterpMode;
						AddedPoint.ArriveTangent = CreatedPoint.ArriveTangent.X;
						AddedPoint.LeaveTangent = CreatedPoint.LeaveTangent.X;

						// Add a point to each lookup track since the curve and the lookup track must have an equal number of points.
						XAxisTrack->LookupTrack.AddPoint( CreatedPoint.InVal, DefaultName );
					}

					// Y Axis
					{
						INT Index = YAxisCurve.AddPoint( CreatedPoint.InVal, CreatedPoint.OutVal.Y );
						FInterpCurvePoint<FLOAT>& AddedPoint = YAxisCurve.Points( Index );

						AddedPoint.InterpMode = CreatedPoint.InterpMode;
						AddedPoint.ArriveTangent = CreatedPoint.ArriveTangent.Y;
						AddedPoint.LeaveTangent = CreatedPoint.LeaveTangent.Y;

						// Add a point to each lookup track since the curve and the lookup track must have an equal number of points.
						YAxisTrack->LookupTrack.AddPoint( CreatedPoint.InVal, DefaultName );
					}

					// Z Axis
					{
						INT Index = ZAxisCurve.AddPoint( CreatedPoint.InVal, CreatedPoint.OutVal.Z );
						FInterpCurvePoint<FLOAT>& AddedPoint = ZAxisCurve.Points( Index );

						AddedPoint.InterpMode = CreatedPoint.InterpMode;
						AddedPoint.ArriveTangent = CreatedPoint.ArriveTangent.Y;
						AddedPoint.LeaveTangent = CreatedPoint.LeaveTangent.Y;

						// Add a point to each lookup track since the curve and the lookup track must have an equal number of points.
						ZAxisTrack->LookupTrack.AddPoint( CreatedPoint.InVal, DefaultName );
					}
				}

				// Calculate tangents
				XAxisCurve.AutoSetTangents();
				YAxisCurve.AutoSetTangents();
				ZAxisCurve.AutoSetTangents();

				// Reduce the number of keys we created as there were probably too many.
				ReduceKeysForTrack( XAxisTrack, TrackInst, SegmentStartTime, SegmentEndTime, 1.0f );
				ReduceKeysForTrack( YAxisTrack, TrackInst, SegmentStartTime, SegmentEndTime, 1.0f );
				ReduceKeysForTrack( ZAxisTrack, TrackInst, SegmentStartTime, SegmentEndTime, 1.0f );

				InterpEdTrans->EndSpecial();
			}
		}
	}
}

/** Called when sash position changes so we can remember the sash position. */
void WxInterpEd::OnGraphSplitChangePos(wxSplitterEvent& In)
{
	GraphSplitPos = GraphSplitterWnd->GetSashPosition();
}


/**
 * Called when a docking window state has changed
 */
void WxInterpEd::OnWindowDockingLayoutChanged()
{
	// Check to see if the Curve Editor is currently visible
	UBOOL bCurveEditorVisible = FALSE;

	FDockingParent::FDockWindowState CurveEdDockState;
	if( GetDockingWindowState( CurveEd, CurveEdDockState ) )
	{
		bCurveEditorVisible = CurveEdDockState.bIsVisible;
	}

	// Update button status.
	if( ToolBar != NULL )
	{
		ToolBar->ToggleTool( IDM_INTERP_TOGGLE_CURVEEDITOR, bCurveEditorVisible ? TRUE : FALSE );
	}

	if( MenuBar != NULL )
	{
		MenuBar->ViewMenu->Check( IDM_INTERP_TOGGLE_CURVEEDITOR, bCurveEditorVisible ? TRUE : FALSE );
	}

	// OK, we need to make sure our track window's scroll bar gets repositioned correctly if the docking
	// layout changes
	if( TrackWindow != NULL )
	{
		TrackWindow->UpdateWindowLayout();
	}
	if( DirectorTrackWindow != NULL )
	{
		DirectorTrackWindow->UpdateWindowLayout();
	}
}



/** Turn keyframe snap on/off. */
void WxInterpEd::OnToggleSnap(wxCommandEvent& In)
{
	SetSnapEnabled( In.IsChecked() );
}



/** Updates UI state for 'snap keys' option */
void WxInterpEd::OnToggleSnap_UpdateUI( wxUpdateUIEvent& In )
{
	In.Check( bSnapEnabled ? TRUE : FALSE );
}	



/** Called when the 'snap time to frames' command is triggered from the GUI */
void WxInterpEd::OnToggleSnapTimeToFrames( wxCommandEvent& In )
{
	SetSnapTimeToFrames( In.IsChecked() );
}



/** Updates UI state for 'snap time to frames' option */
void WxInterpEd::OnToggleSnapTimeToFrames_UpdateUI( wxUpdateUIEvent& In )
{
	In.Enable( bSnapToFrames ? TRUE : FALSE );
	In.Check( bSnapToFrames && bSnapTimeToFrames );
}



/** Called when the 'fixed time step playback' command is triggered from the GUI */
void WxInterpEd::OnFixedTimeStepPlaybackCommand( wxCommandEvent& In )
{
	SetFixedTimeStepPlayback( In.IsChecked() );
}



/** Updates UI state for 'fixed time step playback' option */
void WxInterpEd::OnFixedTimeStepPlaybackCommand_UpdateUI( wxUpdateUIEvent& In )
{
	In.Enable( bSnapToFrames ? TRUE : FALSE );
	In.Check( bSnapToFrames && bFixedTimeStepPlayback );
}



/** Called when the 'prefer frame numbers' command is triggered from the GUI */
void WxInterpEd::OnPreferFrameNumbersCommand( wxCommandEvent& In )
{
	SetPreferFrameNumbers( In.IsChecked() );
}



/** Updates UI state for 'prefer frame numbers' option */
void WxInterpEd::OnPreferFrameNumbersCommand_UpdateUI( wxUpdateUIEvent& In )
{
	In.Enable( bSnapToFrames ? TRUE : FALSE );
	In.Check( bSnapToFrames && bPreferFrameNumbers );
}



/** Called when the 'show time cursor pos for all keys' command is triggered from the GUI */
void WxInterpEd::OnShowTimeCursorPosForAllKeysCommand( wxCommandEvent& In )
{
	SetShowTimeCursorPosForAllKeys( In.IsChecked() );
}



/** Updates UI state for 'show time cursor pos for all keys' option */
void WxInterpEd::OnShowTimeCursorPosForAllKeysCommand_UpdateUI( wxUpdateUIEvent& In )
{
	In.Enable( TRUE );
	In.Check( bShowTimeCursorPosForAllKeys ? TRUE : FALSE );
}



/** The snap resolution combo box was changed. */
void WxInterpEd::OnChangeSnapSize(wxCommandEvent& In)
{
	const INT NewSelection = In.GetInt();
	check(NewSelection >= 0 && NewSelection <= ARRAY_COUNT(InterpEdSnapSizes)+ARRAY_COUNT(InterpEdFPSSnapSizes));

	if(NewSelection == ARRAY_COUNT(InterpEdSnapSizes)+ARRAY_COUNT(InterpEdFPSSnapSizes))
	{
		bSnapToFrames = FALSE;
		bSnapToKeys = TRUE;
		SnapAmount = 1.0f / 30.0f;	// Shouldn't be used
		CurveEd->SetInSnap(FALSE, SnapAmount, bSnapToFrames);
	}
	else if(NewSelection<ARRAY_COUNT(InterpEdSnapSizes))	// see if they picked a second snap amount
	{
		bSnapToFrames = FALSE;
		bSnapToKeys = FALSE;
		SnapAmount = InterpEdSnapSizes[NewSelection];
		CurveEd->SetInSnap(bSnapEnabled, SnapAmount, bSnapToFrames);
	}
	else if(NewSelection<ARRAY_COUNT(InterpEdFPSSnapSizes)+ARRAY_COUNT(InterpEdSnapSizes))	// See if they picked a FPS snap amount.
	{
		bSnapToFrames = TRUE;
		bSnapToKeys = FALSE;
		SnapAmount = InterpEdFPSSnapSizes[NewSelection-ARRAY_COUNT(InterpEdSnapSizes)];
		CurveEd->SetInSnap(bSnapEnabled, SnapAmount, bSnapToFrames);
	}

	// Enable or disable the 'Snap Time To Frames' button based on whether or not we have a 'frame rate' selected
	ToolBar->EnableTool( IDM_INTERP_TOGGLE_SNAP_TIME_TO_FRAMES, bSnapToFrames ? TRUE : FALSE );

	// Save selected snap mode to INI.
	GConfig->SetInt(TEXT("Matinee"), TEXT("SelectedSnapMode"), NewSelection, GEditorUserSettingsIni );

	// Snap time to frames right now if we need to
	SetSnapTimeToFrames( bSnapTimeToFrames );

	// If 'fixed time step playback' is turned on, we also need to make sure the benchmarking time step
	// is set when this changes
	SetFixedTimeStepPlayback( bFixedTimeStepPlayback );

	// The 'prefer frame numbers' option requires bSnapToFrames to be enabled, so update it's state
	SetPreferFrameNumbers( bPreferFrameNumbers );

	// Make sure any particle replay tracks are filled in with the correct state
	UpdateParticleReplayTracks();
}



/**
 * Called when the initial curve interpolation mode for newly created keys is changed
 */
void WxInterpEd::OnChangeInitialInterpMode( wxCommandEvent& In )
{
	const INT NewSelection = In.GetInt();

	// Store new interp mode
	InitialInterpMode = ( EInterpCurveMode )NewSelection;
	check( InitialInterpMode >= 0 && InitialInterpMode < CIM_Unknown );

	// Save selected mode to user's preference file
	GConfig->SetInt( TEXT( "Matinee" ), TEXT( "InitialInterpMode2" ), NewSelection, GEditorUserSettingsIni );
}



/** Adjust the view so the entire sequence fits into the viewport. */
void WxInterpEd::OnViewFitSequence(wxCommandEvent& In)
{
	ViewFitSequence();
}

/** Adjust the view so the selected keys fit into the viewport. */
void WxInterpEd::OnViewFitToSelected(wxCommandEvent& In)
{
	ViewFitToSelected();
}

/** Adjust the view so the looped section fits into the viewport. */
void WxInterpEd::OnViewFitLoop(wxCommandEvent& In)
{
	ViewFitLoop();
}

/** Adjust the view so the looped section fits into the entire sequence. */
void WxInterpEd::OnViewFitLoopSequence(wxCommandEvent& In)
{
	ViewFitLoopSequence();
}

/** Move the view to the end of the currently selected track(s). */
void WxInterpEd::OnViewEndOfTrack( wxCommandEvent& In)
{
	ViewEndOfTrack();
}

/*-----------------------------------------------------------------------------
	WxInterpEdToolBar
-----------------------------------------------------------------------------*/

WxInterpEdToolBar::WxInterpEdToolBar( wxWindow* InParent, wxWindowID InID )
: WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
{
	AddB.Load( TEXT("MAT_AddKey") );
	PlayReverseB.Load( TEXT("MAT_PlayReverse") );
	CreateMovieB.Load( TEXT("MAT_CreateMovie") );
	PlayB.Load( TEXT("MAT_Play") );
	LoopSectionB.Load( TEXT("MAT_PlayLoopSection") );
	StopB.Load( TEXT("MAT_Stop") );
	UndoB.Load( TEXT("MAT_Undo") );
	RedoB.Load( TEXT("MAT_Redo") );
	CurveEdB.Load( TEXT("MAT_CurveEd") );
	SnapB.Load( TEXT("MAT_ToggleSnap") );
	FitSequenceB.Load( TEXT("MAT_FitSequence") );
	FitToSelectedB.Load( TEXT( "MAT_FitViewToSelected" ) );
	FitLoopB.Load( TEXT("MAT_FitLoop") );
	FitLoopSequenceB.Load( TEXT("MAT_FitLoopSequence") );
	EndOfTrackB.Load( TEXT("MAT_EndOfTrack") );

	Speed1B.Load(TEXT("CASC_Speed_1"));
	Speed10B.Load(TEXT("CASC_Speed_10"));
	Speed25B.Load(TEXT("CASC_Speed_25"));
	Speed50B.Load(TEXT("CASC_Speed_50"));
	Speed100B.Load(TEXT("CASC_Speed_100"));
	SnapTimeToFramesB.Load( TEXT( "MAT_SnapTimeToFrames" ) );
	FixedTimeStepPlaybackB.Load( TEXT( "MAT_FixedTimeStepPlayback" ) );
	GorePreviewB.Load( TEXT( "MAT_GorePreview" ) );
	CreateCameraActorAtCurrentCameraLocationB.Load( TEXT( "MAT_CreateCameraActorAtCurrentCameraLocation" ) );
	
	RecordModeViewportBitmap.Load(TEXT("VisibilityEyeOn.png"));

	SetToolBitmapSize( wxSize( 18, 18 ) );

	AddTool( IDM_INTERP_ADDKEY, AddB, *LocalizeUnrealEd("AddKey") );

	// Create combo box that allows the user to select the initial curve interpolation mode for newly created keys
	{
		InitialInterpModeComboBox = new WxComboBox( this, IDM_INTERP_InitialInterpMode_ComboBox, TEXT(""), wxDefaultPosition, wxSize(140, -1), 0, NULL, wxCB_READONLY );

		InitialInterpModeComboBox->SetToolTip( *LocalizeUnrealEd( "InterpEd_InitialInterpModeComboBox_Desc" ) );

		// NOTE: These must be in the same order as the definitions in UnMath.h for EInterpCurveMode
		InitialInterpModeComboBox->Append( *LocalizeUnrealEd( "Linear" ) );                   // CIM_Linear
		InitialInterpModeComboBox->Append( *LocalizeUnrealEd( "CurveAuto" ) );                // CIM_CurveAuto
		InitialInterpModeComboBox->Append( *LocalizeUnrealEd( "Constant" ) );                 // CIM_Constant
		InitialInterpModeComboBox->Append( *LocalizeUnrealEd( "CurveUser" ) );                // CIM_CurveUser
		InitialInterpModeComboBox->Append( *LocalizeUnrealEd( "CurveBreak" ) );               // CIM_CurveBreak
		InitialInterpModeComboBox->Append( *LocalizeUnrealEd( "CurveAutoClamped" ) );         // CIM_CurveAutoClamped

		AddControl( InitialInterpModeComboBox );
	}

	AddSeparator();

	AddTool( IDM_INTERP_PLAY, PlayB, *LocalizeUnrealEd("Play") );
	AddTool( IDM_INTERP_PLAYLOOPSECTION, LoopSectionB, *LocalizeUnrealEd("LoopSection") );
	AddTool( IDM_INTERP_STOP, StopB, *LocalizeUnrealEd("Stop") );
	AddTool( IDM_INTERP_PlayReverse, PlayReverseB, *LocalizeUnrealEd( "InterpEd_ToolBar_PlayReverse_Desc" ) );

	AddSeparator();

	AddTool( IDM_INTERP_CreateCameraActorAtCurrentCameraLocation, CreateCameraActorAtCurrentCameraLocationB, *LocalizeUnrealEd( "InterpEd_CreateCameraActorAtCurrentCameraLocation" ) );

	AddSeparator();

	AddRadioTool(IDM_INTERP_SPEED_100,	*LocalizeUnrealEd("FullSpeed"), Speed100B, wxNullBitmap, *LocalizeUnrealEd("FullSpeed") );
	AddRadioTool(IDM_INTERP_SPEED_50,	*LocalizeUnrealEd("50Speed"), Speed50B, wxNullBitmap, *LocalizeUnrealEd("50Speed") );
	AddRadioTool(IDM_INTERP_SPEED_25,	*LocalizeUnrealEd("25Speed"), Speed25B, wxNullBitmap, *LocalizeUnrealEd("25Speed") );
	AddRadioTool(IDM_INTERP_SPEED_10,	*LocalizeUnrealEd("10Speed"), Speed10B, wxNullBitmap, *LocalizeUnrealEd("10Speed") );
	AddRadioTool(IDM_INTERP_SPEED_1,	*LocalizeUnrealEd("1Speed"), Speed1B, wxNullBitmap, *LocalizeUnrealEd("1Speed") );
	ToggleTool(IDM_INTERP_SPEED_100, TRUE);

	AddSeparator();

	AddTool( IDM_INTERP_EDIT_UNDO, UndoB, *LocalizeUnrealEd("Undo") );
	AddTool( IDM_INTERP_EDIT_REDO, RedoB, *LocalizeUnrealEd("Redo") );

	AddSeparator();

	AddCheckTool( IDM_INTERP_TOGGLE_CURVEEDITOR, *LocalizeUnrealEd("ToggleCurveEditor"), CurveEdB, wxNullBitmap, *LocalizeUnrealEd("ToggleCurveEditor") );

	AddSeparator();

	AddCheckTool( IDM_INTERP_TOGGLE_SNAP, *LocalizeUnrealEd("ToggleSnap"), SnapB, wxNullBitmap, *LocalizeUnrealEd("ToggleSnap") );
	AddCheckTool( IDM_INTERP_TOGGLE_SNAP_TIME_TO_FRAMES, *LocalizeUnrealEd( "InterpEd_ToggleSnapTimeToFrames_Desc" ), SnapTimeToFramesB, wxNullBitmap, *LocalizeUnrealEd("InterpEd_ToggleSnapTimeToFrames_Desc") );
	AddCheckTool( IDM_INTERP_FixedTimeStepPlayback, *LocalizeUnrealEd( "InterpEd_FixedTimeStepPlayback_Desc" ), FixedTimeStepPlaybackB, wxNullBitmap, *LocalizeUnrealEd("InterpEd_FixedTimeStepPlayback_Desc") );
	
	// Create snap-size combo
	{
		SnapCombo = new WxComboBox( this, IDM_INTERP_SNAPCOMBO, TEXT(""), wxDefaultPosition, wxSize(110, -1), 0, NULL, wxCB_READONLY );

		SnapCombo->SetToolTip( *LocalizeUnrealEd( "InterpEd_SnapComboBox_Desc" ) );

		// Append Second Snap Times
		for(INT i=0; i<ARRAY_COUNT(InterpEdSnapSizes); i++)
		{
			FString SnapCaption = FString::Printf( TEXT("%1.2f"), InterpEdSnapSizes[i] );
			SnapCombo->Append( *SnapCaption );
		}

		// Append FPS Snap Times
		for(INT i=0; i<ARRAY_COUNT(InterpEdFPSSnapSizes); i++)
		{
			FString SnapCaption = LocalizeUnrealEd( InterpEdFPSSnapSizeLocNames[ i ] );
			SnapCombo->Append( *SnapCaption );
		}

		SnapCombo->Append( *LocalizeUnrealEd( TEXT("InterpEd_Snap_Keys") ) ); // Add option for snapping to other keys.
		SnapCombo->SetSelection(2);
	}

	AddControl(SnapCombo);

	AddSeparator();
	AddTool( IDM_INTERP_VIEW_FITSEQUENCE, FitSequenceB, *LocalizeUnrealEd("ViewFitSequence") );
	AddTool( IDM_INTERP_VIEW_FitViewToSelected, FitToSelectedB, *LocalizeUnrealEd("ViewFitToSelected") );
	AddTool( IDM_INTERP_VIEW_FITLOOP, FitLoopB, *LocalizeUnrealEd("ViewFitLoop") );
	AddTool( IDM_INTERP_VIEW_FITLOOPSEQUENCE, FitLoopSequenceB, *LocalizeUnrealEd("ViewFitLoopSequence") );
	
	AddSeparator();
	AddTool( IDM_INTERP_VIEW_ENDOFSEQUENCE, EndOfTrackB, *LocalizeUnrealEd("ViewEndOfTrack") );

	AddSeparator();
	AddCheckTool( IDM_INTERP_ToggleGorePreview, *LocalizeUnrealEd( "InterpEd_ToggleGorePreview"), GorePreviewB, wxNullBitmap, *LocalizeUnrealEd( "InterpEd_ToggleGorePreview" ) );

	AddSeparator();
	AddTool( IDM_INTERP_LaunchRecordWindow, RecordModeViewportBitmap, *LocalizeUnrealEd( "InterpEd_LaunchRecordingWindow_Tooltip" ) );
	AddSeparator();
	AddTool( IDM_INTERP_CreateMovie, CreateMovieB, *LocalizeUnrealEd( "InterpEd_ToolBar_CreateMovie_Desc" ) );

	//WxBitmap DirectorIcon;
	//DirectorIcon.Load( TEXT( "MainToolBar_UnrealMatinee" ) );
	//AddTool( IDM_INTERP_DirectorWindow, DirectorIcon, *LocalizeUnrealEd("InterpEd_LaunchDirectorWindow") );

	Realize();
}

WxInterpEdToolBar::~WxInterpEdToolBar()
{
}


/*-----------------------------------------------------------------------------
	WxInterpEdMenuBar
-----------------------------------------------------------------------------*/

WxInterpEdMenuBar::WxInterpEdMenuBar(WxInterpEd* InEditor)
{
	FileMenu = new wxMenu();
	Append( FileMenu, *LocalizeUnrealEd("File") );

	FileMenu->Append( IDM_INTERP_FILE_IMPORT, *LocalizeUnrealEd("InterpEd_FileMenu_Import"), TEXT("") );
	FileMenu->Append( IDM_INTERP_FILE_EXPORT, *LocalizeUnrealEd("InterpEd_FileMenu_ExportAll"), TEXT("") );
	FileMenu->Append( IDM_INTERP_ExportSoundCueInfo, *LocalizeUnrealEd( "InterpEd_FileMenu_ExportSoundCueInfo" ), TEXT("") );
	FileMenu->Append( IDM_INTERP_ExportAnimInfo, *LocalizeUnrealEd( "InterpEd_FileMenu_ExportAnimInfo" ), TEXT("") );
	FileMenu->AppendSeparator();
	FileMenu->AppendCheckItem( IDM_INTERP_FILE_EXPORT_BAKE_TRANSFORMS, *LocalizeUnrealEd("InterpEd_FileMenu_ExportBakeTransforms"), TEXT("") );

	EditMenu = new wxMenu();
	Append( EditMenu, *LocalizeUnrealEd("Edit") );

	EditMenu->Append( IDM_INTERP_EDIT_UNDO, *LocalizeUnrealEd("Undo"), TEXT("") );
	EditMenu->Append( IDM_INTERP_EDIT_REDO, *LocalizeUnrealEd("Redo"), TEXT("") );
	EditMenu->AppendSeparator();
	EditMenu->Append( IDM_INTERP_DeleteSelectedKeys, *LocalizeUnrealEd( "InterpEd_EditMenu_DeleteSelectedKeys" ), TEXT("") );
	EditMenu->Append( IDM_INTERP_EDIT_DUPLICATEKEYS, *LocalizeUnrealEd("DuplicateSelectedKeys"), TEXT("") );
	EditMenu->AppendSeparator();
	EditMenu->Append( IDM_INTERP_EDIT_INSERTSPACE, *LocalizeUnrealEd("InsertSpaceCurrent"), TEXT("") );
	EditMenu->Append( IDM_INTERP_EDIT_STRETCHSECTION, *LocalizeUnrealEd("StretchSection"), TEXT("") );
	EditMenu->Append( IDM_INTERP_EDIT_STRETCHSELECTEDKEYFRAMES, *LocalizeUnrealEd("StretchSelectedKeyframes"), TEXT("") );
	EditMenu->Append( IDM_INTERP_EDIT_DELETESECTION, *LocalizeUnrealEd("DeleteSection"), TEXT("") );
	EditMenu->Append( IDM_INTERP_EDIT_SELECTINSECTION, *LocalizeUnrealEd("SelectKeysSection"), TEXT("") );
	EditMenu->AppendSeparator();
	EditMenu->Append( IDM_INTERP_EDIT_REDUCEKEYS, *LocalizeUnrealEd("ReduceKeys"), TEXT("") );
	EditMenu->AppendSeparator();
	EditMenu->Append( IDM_INTERP_EDIT_SAVEPATHTIME, *LocalizeUnrealEd("SavePathBuildingPositions"), TEXT("") );
	EditMenu->Append( IDM_INTERP_EDIT_JUMPTOPATHTIME, *LocalizeUnrealEd("JumpPathBuildingPositions"), TEXT("") );
	EditMenu->AppendSeparator();
	EditMenu->Append( IDM_OPEN_BINDKEYS_DIALOG,	*LocalizeUnrealEd("BindEditorHotkeys"), TEXT("") );

	ViewMenu = new wxMenu();
	Append( ViewMenu, *LocalizeUnrealEd("View") );

	ViewMenu->AppendCheckItem( IDM_INTERP_VIEW_Draw3DTrajectories, *LocalizeUnrealEd("InterpEd_ViewMenu_Show3DTrajectories"), TEXT("") );
	bool bShowTrack = (InEditor->bHide3DTrackView == FALSE);
	ViewMenu->Check( IDM_INTERP_VIEW_Draw3DTrajectories, bShowTrack );
	ViewMenu->Append( IDM_INTERP_VIEW_ShowAll3DTrajectories, *LocalizeUnrealEd( "InterpEd_MovementTrackContext_ShowAll3DTracjectories" ), TEXT( "" ) );
	ViewMenu->Append( IDM_INTERP_VIEW_HideAll3DTrajectories, *LocalizeUnrealEd( "InterpEd_MovementTrackContext_HideAll3DTracjectories" ), TEXT( "" ) );
	ViewMenu->AppendSeparator();

	ViewMenu->AppendCheckItem( IDM_INTERP_TOGGLE_SNAP, *LocalizeUnrealEd( "InterpEd_ViewMenu_ToggleSnap" ), TEXT( "" ) );
	ViewMenu->AppendCheckItem( IDM_INTERP_TOGGLE_SNAP_TIME_TO_FRAMES, *LocalizeUnrealEd( "InterpEd_ViewMenu_SnapTimeToFrames" ), TEXT( "" ) );
	ViewMenu->AppendCheckItem( IDM_INTERP_FixedTimeStepPlayback, *LocalizeUnrealEd( "InterpEd_ViewMenu_FixedTimeStepPlayback" ), TEXT( "" ) );
	ViewMenu->AppendCheckItem( IDM_INTERP_PreferFrameNumbers, *LocalizeUnrealEd( "InterpEd_ViewMenu_PreferFrameNumbers" ), TEXT( "" ) );
	ViewMenu->AppendCheckItem( IDM_INTERP_ShowTimeCursorPosForAllKeys, *LocalizeUnrealEd( "InterpEd_ViewMenu_ShowTimeCursorPosForAllKeys" ), TEXT( "" ) );
	ViewMenu->AppendSeparator();

	ViewMenu->AppendCheckItem( IDM_INTERP_VIEW_ZoomToTimeCursorPosition, *LocalizeUnrealEd("InterpEd_ViewMenu_ZoomToTimeCursorPosition"), TEXT("") );
	bool bZoomToScub = (InEditor->bZoomToScrubPos == TRUE);
	ViewMenu->Check( IDM_INTERP_VIEW_ZoomToTimeCursorPosition, bZoomToScub );

	ViewMenu->AppendCheckItem( IDM_INTERP_VIEW_ViewportFrameStats, *LocalizeUnrealEd("InterpEd_ViewMenu_ViewportFrameStats"), TEXT("") );
	ViewMenu->Check( IDM_INTERP_VIEW_ViewportFrameStats, InEditor->IsViewportFrameStatsEnabled() == TRUE );

	ViewMenu->AppendCheckItem( IDM_INTERP_VIEW_EditingCrosshair, *LocalizeUnrealEd("InterpEd_ViewMenu_EditingCrosshair"), TEXT("") );
	ViewMenu->Check( IDM_INTERP_VIEW_EditingCrosshair, InEditor->IsEditingCrosshairEnabled() == TRUE );
	
	wxMenu* EditingGridMenu = new wxMenu();
	
	EditingGridMenu->AppendCheckItem( IDM_INTERP_VIEW_EnableEditingGrid, *LocalizeUnrealEd("InterpEd_ViewMenu_EnableEditingGrid") );
	EditingGridMenu->AppendSeparator();
	for( INT GridSize = 1; GridSize <= 16; ++GridSize )
	{
		FString MenuStr = FString::Printf(LocalizeSecure( LocalizeUnrealEd("InterpEd_EditingGridMenu_Size"), GridSize,GridSize ));
		EditingGridMenu->AppendCheckItem( IDM_INTERP_VIEW_EditingGrid_Start+(GridSize-1), *MenuStr, TEXT("") );
	}
	
	ViewMenu->AppendSubMenu( EditingGridMenu, *LocalizeUnrealEd("InterpEd_ViewMenu_EditingGrid"), TEXT("") );

	ViewMenu->AppendSeparator();
	ViewMenu->Append( IDM_INTERP_VIEW_FITSEQUENCE,	*LocalizeUnrealEd("ViewFitSequence"), TEXT(""));
	ViewMenu->Append( IDM_INTERP_VIEW_FitViewToSelected, *LocalizeUnrealEd( "ViewFitToSelected" ), TEXT( "" ) );
	ViewMenu->Append( IDM_INTERP_VIEW_FITLOOP,	*LocalizeUnrealEd("ViewFitLoop"), TEXT(""));
	ViewMenu->Append( IDM_INTERP_VIEW_FITLOOPSEQUENCE,	*LocalizeUnrealEd("ViewFitLoopSequence"), TEXT(""));
	
	ViewMenu->AppendSeparator();
	ViewMenu->Append( IDM_INTERP_VIEW_ENDOFSEQUENCE,	*LocalizeUnrealEd("ViewEndOfTrack"), TEXT(""));

	ViewMenu->AppendSeparator();
	ViewMenu->AppendCheckItem( IDM_INTERP_ToggleGorePreview, *LocalizeUnrealEd( "InterpEd_ToggleGorePreview"), TEXT( "" ) );

	ViewMenu->AppendSeparator();
	ViewMenu->AppendCheckItem( IDM_INTERP_TOGGLE_CURVEEDITOR,	*LocalizeUnrealEd("ToggleCurveEditor"), TEXT(""));

	ViewMenu->AppendSeparator();
	ViewMenu->AppendCheckItem( IDM_INTERP_INVERT_PAN, *LocalizeUnrealEd("TogglePanInvert"), TEXT(""));

	ViewMenu->AppendSeparator();
	ViewMenu->AppendCheckItem( IDM_INTERP_ToggleAllowKeyframeBarSelection, *LocalizeUnrealEd("InterpEd_AllowKeyframeBarSelection"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_INTERP_ToggleAllowKeyframeTextSelection, *LocalizeUnrealEd("InterpEd_AllowKeyframeTextSelection"), TEXT("") );

	// Check to see if the Curve Editor is currently visible
	UBOOL bCurveEditorVisible = FALSE;

	FDockingParent::FDockWindowState CurveEdDockState;
	if( InEditor->GetDockingWindowState( InEditor->CurveEd, CurveEdDockState ) )
	{
		bCurveEditorVisible = CurveEdDockState.bIsVisible;
	}

	UBOOL bInvertPan = GEditorModeTools().GetInterpPanInvert();

	// Update button state
	ViewMenu->Check( IDM_INTERP_TOGGLE_CURVEEDITOR, bCurveEditorVisible ? TRUE : FALSE );
	ViewMenu->Check( IDM_INTERP_INVERT_PAN, bInvertPan ? TRUE : FALSE );
}

WxInterpEdMenuBar::~WxInterpEdMenuBar()
{

}


/*-----------------------------------------------------------------------------
	WxMBInterpEdTabMenu
-----------------------------------------------------------------------------*/

WxMBInterpEdTabMenu::WxMBInterpEdTabMenu(WxInterpEd* InterpEd)
{
	UInterpFilter_Custom* Filter = Cast<UInterpFilter_Custom>(InterpEd->IData->SelectedFilter);
	if(Filter != NULL)
	{
		// make sure this isn't a default filter.
		if(InterpEd->IData->InterpFilters.ContainsItem(Filter))
		{
			Append(IDM_INTERP_GROUP_DELETETAB, *LocalizeUnrealEd("DeleteGroupTab"));
		}
	}
}

WxMBInterpEdTabMenu::~WxMBInterpEdTabMenu()
{

}


/*-----------------------------------------------------------------------------
	WxMBInterpEdGroupMenu
-----------------------------------------------------------------------------*/

WxMBInterpEdGroupMenu::WxMBInterpEdGroupMenu(WxInterpEd* InterpEd)
{
	// If no group is selected, then this menu should 
	// not have been created in the first place.
	check( InterpEd->HasAGroupSelected() );

	const INT SelectedGroupCount = InterpEd->GetSelectedGroupCount();
	const UBOOL bHasOneGroupSelected = (SelectedGroupCount == 1);

	// Certain menu options are only available if only one group is selected. 
	if( bHasOneGroupSelected )
	{
		UInterpGroup* SelectedGroup = *InterpEd->GetSelectedGroupIterator();

		const UBOOL bIsFolder = SelectedGroup->bIsFolder;
		const UBOOL bIsDirGroup = SelectedGroup->IsA(UInterpGroupDirector::StaticClass());

		// When we have only one group selected and it's not a folder, 
		// then we can create tracks on the selected group.
		if( !bIsFolder )
		{
			for(INT i=0; i<InterpEd->InterpTrackClasses.Num(); i++)
			{
				UInterpTrack* DefTrack = (UInterpTrack*)InterpEd->InterpTrackClasses(i)->GetDefaultObject();
				if( !DefTrack->bDirGroupOnly && !DefTrack->bSubTrackOnly )
				{
					FString NewTrackString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("AddNew_F"), *InterpEd->InterpTrackClasses(i)->GetDescription()) );
					Append( IDM_INTERP_NEW_TRACK_START+i, *NewTrackString, TEXT("") );
				}
			}

			AppendSeparator();
		}


		// Add Director-group specific tracks to separate menu underneath.
		if( bIsDirGroup )
		{
			for(INT i=0; i<InterpEd->InterpTrackClasses.Num(); i++)
			{
				UInterpTrack* DefTrack = (UInterpTrack*)InterpEd->InterpTrackClasses(i)->GetDefaultObject();
				if(DefTrack->bDirGroupOnly)
				{
					FString NewTrackString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("AddNew_F"), *InterpEd->InterpTrackClasses(i)->GetDescription()) );
					Append( IDM_INTERP_NEW_TRACK_START+i, *NewTrackString, TEXT("") );
				}
			}

			AppendSeparator();
		}

		// Add CameraAnim export option if appropriate
		if ( !bIsDirGroup && !bIsFolder )
		{
			UInterpGroupInst* GrInst = InterpEd->Interp->FindFirstGroupInst(SelectedGroup);
			check(GrInst);
			if (GrInst)
			{
				AActor* const GroupActor = GrInst->GetGroupActor();
				UBOOL bControllingACameraActor = GroupActor && GroupActor->IsA(ACameraActor::StaticClass());
				if (bControllingACameraActor)
				{
					// add strings to unrealed.int
					Append(IDM_INTERP_CAMERA_ANIM_EXPORT, *LocalizeUnrealEd("ExportCameraAnim"), *LocalizeUnrealEd("ExportCameraAnim_Desc"));
					AppendSeparator();
				}
			}
		}

		if( SelectedGroup->HasAnimControlTrack() )
		{
			// Add menu item to export group animations to fbx
			// Should be very similar to the anim control track right click menu 
			Append (IDM_INTERP_GROUP_ExportAnimTrackFBX, *LocalizeUnrealEd("ExportAnimTrack"), TEXT( "" ));
			AppendSeparator();
		}
	}

	const UBOOL bHasAFolderSelected = InterpEd->HasAFolderSelected();
	const UBOOL bHasADirectorSelected = InterpEd->HasAGroupSelected(UInterpGroupDirector::StaticClass());

	// Copy/Paste not supported on folders yet
	if( !bHasAFolderSelected )
	{
		Append (IDM_INTERP_EDIT_CUT, *LocalizeUnrealEd("CutGroup"), *LocalizeUnrealEd("InterpEd_Cut_GroupDesc"));
		Append (IDM_INTERP_EDIT_COPY, *LocalizeUnrealEd("CopyGroup"), *LocalizeUnrealEd("InterpEd_Copy_GroupDesc"));
		Append (IDM_INTERP_EDIT_PASTE, *LocalizeUnrealEd("PasteGroupOrTrack"), *LocalizeUnrealEd("InterpEd_Paste_GroupOrTrackDesc"));

		AppendSeparator();
	}

	FString RenameText = FString(TEXT("RenameGroup"));
	FString DeleteText = FString(TEXT("DeleteGroup"));

	if( bHasAFolderSelected )
	{
		if( InterpEd->AreAllSelectedGroupsFolders() )
		{
			RenameText = FString(TEXT("RenameFolder"));
			DeleteText = FString(TEXT("DeleteFolder"));
		}
		else
		{
			RenameText = FString(TEXT("RenameFolderAndGroup"));
			DeleteText = FString(TEXT("DeleteFolderAndGroup"));
		}
	}

	Append( IDM_INTERP_GROUP_RENAME, *LocalizeUnrealEd(*RenameText), TEXT("") );

	// Cannot duplicate Director groups or folders
	if( !bHasADirectorSelected && !bHasAFolderSelected )
	{
		Append( IDM_INTERP_GROUP_DUPLICATE, *LocalizeUnrealEd("DuplicateGroup"), TEXT("") );
	}

	Append( IDM_INTERP_GROUP_DELETE, *LocalizeUnrealEd(*DeleteText), TEXT("") );


	UBOOL bNeedSeparator = TRUE;

	// These variables will be allocated on demand, later
	wxMenu* PotentialParentFoldersMenu = NULL;
	wxMenu* PotentialChildGroupsMenu = NULL;

	// If only one group is selectd and that group is a folder, then 
	// we can setup a sub-menu to move selected groups to the folder.
	if( bHasOneGroupSelected && bHasAFolderSelected )
	{
		FInterpGroupParentInfo SelectedGroupInfo(*InterpEd->GetSelectedGroupIterator());

		// @todo: If more than 1000 groups exist in the data set, this limit will start to cause us problems
		const INT MaxAllowedGroupIndex =
			( IDM_INTERP_GROUP_MoveGroupToActiveFolder_End - IDM_INTERP_GROUP_MoveGroupToActiveFolder_Start );

		for( FGroupIterator GroupIter(InterpEd->GetGroupIterator()); GroupIter; ++GroupIter )
		{
			FInterpGroupParentInfo CurrentGroupInfo = InterpEd->GetParentInfo(*GroupIter);

			if( CurrentGroupInfo.GroupIndex > MaxAllowedGroupIndex )
			{
				// We've run out of space in the sub menu (no more resource IDs!).  Oh well, we won't display these items.
				// Since we are iterating incrementally, all groups after this can't be added either. So, break out of the loop.
				break;
			}

			// If the current group can be re-parented by the only selected group, then 
			// we can add an option to move the current group into the selected folder.
			if( InterpEd->CanReparent(CurrentGroupInfo, SelectedGroupInfo) )
			{
				// Construct on demand!
				if( PotentialChildGroupsMenu == NULL )
				{
					PotentialChildGroupsMenu = new wxMenu();
				}

				// OK, this is a candidate child group!
				PotentialChildGroupsMenu->Append( IDM_INTERP_GROUP_MoveGroupToActiveFolder_Start + CurrentGroupInfo.GroupIndex, *CurrentGroupInfo.Group->GroupName.GetNameString() );
			}
		}
	}
	// Else, we have may have multiple groups selected. Attempt to setup a 
	// sub-menu for moving the selected groups to all the potential folders.
	else
	{
		TArray<FInterpGroupParentInfo> MasterFolderArray;

		// @todo: If more than 1000 groups exist in the data set, this limit will start to cause us problems
		const INT MaxAllowedGroupIndex =
			( IDM_INTERP_GROUP_MoveActiveGroupToFolder_End - IDM_INTERP_GROUP_MoveActiveGroupToFolder_Start );

		for( FSelectedGroupIterator SelectedGroupIter(InterpEd->GetSelectedGroupIterator()); SelectedGroupIter; ++SelectedGroupIter )
		{
			FInterpGroupParentInfo SelectedGroupInfo = InterpEd->GetParentInfo(*SelectedGroupIter);

			// We have to compare the current selected group to each existing group to find all potential folders to move to.
			for( FGroupIterator GroupIter(InterpEd->GetGroupIterator()); GroupIter; ++GroupIter )
			{
				FInterpGroupParentInfo CurrentGroupInfo = InterpEd->GetParentInfo(*GroupIter);

				if( CurrentGroupInfo.GroupIndex > MaxAllowedGroupIndex )
				{
					// We've run out of space in the sub menu (no more resource IDs!).  Oh well, we won't display these items.
					// Since we are iterating incrementally, all groups after this can't be added either. So, break out of the loop.
					break;
				}

				// If we can re-parent the selected group to be parented by the current 
				// group, then the current group is a potential folder to move to.
				if( InterpEd->CanReparent(SelectedGroupInfo, CurrentGroupInfo) )
				{
					MasterFolderArray.AddUniqueItem( CurrentGroupInfo );
				}
			}
		}

		// If we have folders that all selected groups can move to, add a sub-menu for that!
		if( MasterFolderArray.Num() )
		{
			PotentialParentFoldersMenu = new wxMenu();

			// Add all the possible folders, that ALL selected groups could move to, to the menu.
			for( TArray<FInterpGroupParentInfo>::TIterator ParentIter(MasterFolderArray); ParentIter; ++ParentIter )
			{
				FInterpGroupParentInfo& CurrentParent = *ParentIter;
				PotentialParentFoldersMenu->Append( IDM_INTERP_GROUP_MoveActiveGroupToFolder_Start + CurrentParent.GroupIndex, *CurrentParent.Group->GroupName.GetNameString() );
			}
		}
	}

	UBOOL bAddedFolderMenuItem = FALSE;
	if( PotentialParentFoldersMenu != NULL )
	{
		if( bNeedSeparator )
		{
			AppendSeparator();
			bNeedSeparator = FALSE;
		}

		Append( IDM_INTERP_GROUP_MoveActiveGroupToFolder_SubMenu, *LocalizeUnrealEd( "InterpEd_MoveActiveGroupToFolder" ), PotentialParentFoldersMenu );
		bAddedFolderMenuItem = TRUE;
	}

	if( PotentialChildGroupsMenu != NULL )
	{
		if( bNeedSeparator )
		{
			AppendSeparator();
			bNeedSeparator = FALSE;
		}
		Append( IDM_INTERP_GROUP_MoveGroupToActiveFolder_SubMenu, *LocalizeUnrealEd( "InterpEd_MoveGroupToActiveFolder" ), PotentialChildGroupsMenu );
		bAddedFolderMenuItem = TRUE;
	}

	// If the group is parented, then add an option to remove it from the group folder its in
	if( InterpEd->AreAllSelectedGroupsParented() )
	{
		if( bNeedSeparator )
		{
			AppendSeparator();
			bNeedSeparator = FALSE;
		}
		Append( IDM_INTERP_GROUP_RemoveFromGroupFolder, *LocalizeUnrealEd( "InterpEd_RemoveFromGroupFolder" ) );
		bAddedFolderMenuItem = TRUE;
	}

	if( bAddedFolderMenuItem )
	{
		bNeedSeparator = TRUE;
	}


	if( !bHasAFolderSelected )
	{
		if( bNeedSeparator )
		{
			AppendSeparator();
			bNeedSeparator = FALSE;
		}

		// Add entries for creating and sending to tabs.
		Append(IDM_INTERP_GROUP_CREATETAB, *LocalizeUnrealEd("CreateNewGroupTab"));

		// See if the user can remove this group from the current tab.
		UInterpFilter* Filter = Cast<UInterpFilter_Custom>(InterpEd->IData->SelectedFilter);
		if(Filter != NULL && InterpEd->HasAGroupSelected() && InterpEd->IData->InterpFilters.ContainsItem(Filter))
		{
			Append(IDM_INTERP_GROUP_REMOVEFROMTAB, *LocalizeUnrealEd("RemoveFromGroupTab"));
		}

		if(InterpEd->Interp->InterpData->InterpFilters.Num())
		{
			wxMenu* TabMenu = new wxMenu();
			for(INT FilterIdx=0; FilterIdx<InterpEd->IData->InterpFilters.Num(); FilterIdx++)
			{
				UInterpFilter* Filter = InterpEd->IData->InterpFilters(FilterIdx);
				TabMenu->Append(IDM_INTERP_GROUP_SENDTOTAB_START+FilterIdx, *Filter->Caption);
			}

			Append(IDM_INTERP_GROUP_SENDTOTAB_SUBMENU, *LocalizeUnrealEd("SendToGroupTab"), TabMenu);
		}
	}

}

WxMBInterpEdGroupMenu::~WxMBInterpEdGroupMenu()
{

}

/*-----------------------------------------------------------------------------
	WxMBInterpEdTrackMenu
-----------------------------------------------------------------------------*/

WxMBInterpEdTrackMenu::WxMBInterpEdTrackMenu(WxInterpEd* InterpEd)
{
	// Must have a track selected to create this menu
	check( InterpEd->HasATrackSelected() );
	
	const UBOOL bOnlyOneTrackSelected = (InterpEd->GetSelectedTrackCount() == 1);

	UInterpTrack* Track = *InterpEd->GetSelectedTrackIterator();

	Append (IDM_INTERP_EDIT_CUT, *LocalizeUnrealEd("CutTrack"), *LocalizeUnrealEd("InterpEd_Cut_TrackDesc"));
	Append (IDM_INTERP_EDIT_COPY, *LocalizeUnrealEd("CopyTrack"), *LocalizeUnrealEd("InterpEd_Copy_TrackDesc"));
	Append (IDM_INTERP_EDIT_PASTE, *LocalizeUnrealEd("PasteGroupOrTrack"), *LocalizeUnrealEd("InterpEd_PasteGroupOrTrackDesc"));

	AppendSeparator();


	Append( IDM_INTERP_TRACK_RENAME, *LocalizeUnrealEd("InterpEd_Rename_Track"), TEXT("") );
	Append( IDM_INTERP_TRACK_DELETE, *LocalizeUnrealEd("DeleteTrack"), TEXT("") );

	

	// These menu commands are only accessible if only one track is selected.
	if( bOnlyOneTrackSelected )
	{
		if( Track->IsA(UInterpTrackAnimControl::StaticClass()) )
		{
			AppendSeparator();

			Append (IDM_INTERP_TRACK_ExportAnimTrackFBX, *LocalizeUnrealEd("ExportAnimTrack"), TEXT( "" ));
		}
		else if( Track->IsA(UInterpTrackMove::StaticClass()) )
		{
			UInterpTrackMove* MoveTrack = CastChecked<UInterpTrackMove>(Track);

			AppendSeparator();

			// Trajectory settings for movement tracks
			AppendCheckItem( IDM_INTERP_TRACK_Show3DTrajectory, *LocalizeUnrealEd( "InterpEd_MovementTrackContext_Show3DTracjectory" ), TEXT( "" ) );
			Check( IDM_INTERP_TRACK_Show3DTrajectory, MoveTrack->bHide3DTrack == FALSE );
			Append( IDM_INTERP_VIEW_ShowAll3DTrajectories, *LocalizeUnrealEd( "InterpEd_MovementTrackContext_ShowAll3DTracjectories" ), TEXT( "" ) );
			Append( IDM_INTERP_VIEW_HideAll3DTrajectories, *LocalizeUnrealEd( "InterpEd_MovementTrackContext_HideAll3DTracjectories" ), TEXT( "" ) );

			AppendSeparator();

			AppendCheckItem( IDM_INTERP_TRACK_FRAME_WORLD, *LocalizeUnrealEd("WorldFrame"), TEXT("") );
			AppendCheckItem( IDM_INTERP_TRACK_FRAME_RELINITIAL, *LocalizeUnrealEd("RelativeInitial"), TEXT("") );

			// Check the currently the selected movement frame
			if( MoveTrack->MoveFrame == IMF_World )
			{
				Check(IDM_INTERP_TRACK_FRAME_WORLD, TRUE);
			}
			else if( MoveTrack->MoveFrame == IMF_RelativeToInitial )
			{
				Check(IDM_INTERP_TRACK_FRAME_RELINITIAL, TRUE);
			}
			else
			{
				// Unhandled move frame type. 
				check(FALSE);
			}

			AppendSeparator();
			if( MoveTrack->SubTracks.Num() == 0 )
			{
				Append( IDM_INTERP_TRACK_SPLIT_TRANS_AND_ROT, *LocalizeUnrealEd("InterpEd_MovementTrackContext_SplitMovementTrack"), TEXT("") );
			}
			else
			{
				// Normalizing velocity is only possible for split tracks.
				Append( IDM_INTERP_TRACK_NORMALIZE_VELOCITY, *LocalizeUnrealEd("InterpEd_MovementTrackContext_NormalizeVelocity"), TEXT("") );
			}

		
		}
		// If this is a Particle Replay track, add buttons for toggling Capture Mode
		else if( Track->IsA(UInterpTrackParticleReplay::StaticClass()) )
		{
			UInterpTrackParticleReplay* ParticleTrack = CastChecked<UInterpTrackParticleReplay>(Track);

			AppendSeparator();

			if( ParticleTrack->bIsCapturingReplay )
			{
				AppendCheckItem( IDM_INTERP_ParticleReplayTrackContext_StopRecording, *LocalizeUnrealEd( "InterpEd_ParticleReplayTrackContext_StopRecording" ), TEXT( "" ) );
			}
			else
			{
				AppendCheckItem( IDM_INTERP_ParticleReplayTrackContext_StartRecording, *LocalizeUnrealEd( "InterpEd_ParticleReplayTrackContext_StartRecording" ), TEXT( "" ) );
			}
		}
		else if( Track->IsA(UInterpTrackNotify::StaticClass()) )
		{
			UInterpTrackNotify* NotifyTrack = CastChecked<UInterpTrackNotify>(Track);

			AppendSeparator();

			// Allow ability to change the slot node picked during track creation
			Append( IDM_INTERP_NotifyTrackContext_SetParentNodeName, *LocalizeUnrealEd("SetParentNodeName"), TEXT("") );
		}
	}
}

WxMBInterpEdTrackMenu::~WxMBInterpEdTrackMenu()
{

}

/*-----------------------------------------------------------------------------
	WxMBInterpEdBkgMenu
-----------------------------------------------------------------------------*/

WxMBInterpEdBkgMenu::WxMBInterpEdBkgMenu(WxInterpEd* InterpEd)
{
	Append (IDM_INTERP_EDIT_PASTE, *LocalizeUnrealEd("PasteGroup"), *LocalizeUnrealEd("InterpEd_Paste_GroupDesc"));

	AppendSeparator();

	Append( IDM_INTERP_NEW_FOLDER, *LocalizeUnrealEd("InterpEd_AddNewFolder"), TEXT("") );

	AppendSeparator();

	Append( IDM_INTERP_NEW_EMPTY_GROUP, *LocalizeUnrealEd("AddNewEmptyGroup"), TEXT("") );

	// Prefab group types
	Append( IDM_INTERP_NEW_CAMERA_GROUP, *LocalizeUnrealEd("AddNewCameraGroup"), TEXT("") );
	Append( IDM_INTERP_NEW_PARTICLE_GROUP, *LocalizeUnrealEd("AddNewParticleGroup"), TEXT("") );
	Append( IDM_INTERP_NEW_SKELETAL_MESH_GROUP, *LocalizeUnrealEd("AddNewSkeletalMeshGroup"), TEXT("") );
	Append( IDM_INTERP_NEW_LIGHTING_GROUP, *LocalizeUnrealEd("AddNewLightingGroup"), TEXT("") );
	Append( IDM_INTERP_NEW_AI_GROUP, *LocalizeUnrealEd("AddNewAIGroup"), TEXT("") );

	TArray<UInterpTrack*> Results;
	InterpEd->IData->FindTracksByClass( UInterpTrackDirector::StaticClass(), Results );
	if(Results.Num() == 0)
	{
		Append( IDM_INTERP_NEW_DIRECTOR_GROUP, *LocalizeUnrealEd("AddNewDirectorGroup"), TEXT("") );
	}
}

WxMBInterpEdBkgMenu::~WxMBInterpEdBkgMenu()
{

}

/*-----------------------------------------------------------------------------
	WxMBInterpEdKeyMenu
-----------------------------------------------------------------------------*/

WxMBInterpEdKeyMenu::WxMBInterpEdKeyMenu(WxInterpEd* InterpEd)
{
	UBOOL bHaveMoveKeys = FALSE;
	UBOOL bHaveFloatKeys = FALSE;
	UBOOL bHaveBoolKeys = FALSE;
	UBOOL bHaveVectorKeys = FALSE;
	UBOOL bHaveLinearColorKeys = FALSE;
	UBOOL bHaveColorKeys = FALSE;
	UBOOL bHaveEventKeys = FALSE;
	UBOOL bHaveNotifyKeys = FALSE;
	UBOOL bHaveAnimKeys = FALSE;
	UBOOL bHaveDirKeys = FALSE;
	UBOOL bAnimIsLooping = FALSE;
	UBOOL bHaveToggleKeys = FALSE;
	UBOOL bHaveVisibilityKeys = FALSE;
	UBOOL bHaveAudioMasterKeys = FALSE;
	UBOOL bHaveParticleReplayKeys = FALSE;

	// TRUE if at least one sound key is selected.
	UBOOL bHaveSoundKeys = FALSE;


	// Keep track of the conditions required for all selected visibility keys to fire
	UBOOL bAllKeyConditionsAreSetToAlways = TRUE;
	UBOOL bAllKeyConditionsAreGoreEnabled = TRUE;
	UBOOL bAllKeyConditionsAreGoreDisabled = TRUE;


	for(INT i=0; i<InterpEd->Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = InterpEd->Opt->SelectedKeys(i);
		UInterpTrack* Track = SelKey.Track;

		if( Track->IsA(UInterpTrackMove::StaticClass()) )
		{
			bHaveMoveKeys = TRUE;
		}
		else if( Track->IsA(UInterpTrackEvent::StaticClass()) )
		{
			bHaveEventKeys = TRUE;
		}
		else if( Track->IsA(UInterpTrackNotify::StaticClass()) )
		{
			bHaveNotifyKeys = TRUE;
		}
		else if( Track->IsA(UInterpTrackDirector::StaticClass()) )
		{
			bHaveDirKeys = TRUE;
		}
		else if( Track->IsA(UInterpTrackAnimControl::StaticClass()) )
		{
			bHaveAnimKeys = TRUE;

			UInterpTrackAnimControl* AnimTrack = (UInterpTrackAnimControl*)Track;
			bAnimIsLooping = AnimTrack->AnimSeqs(SelKey.KeyIndex).bLooping;
		}
		else if( Track->IsA(UInterpTrackFloatBase::StaticClass()) )
		{
			bHaveFloatKeys = TRUE;
		}
		else if( Track->IsA(UInterpTrackBoolProp::StaticClass()) )
		{
			bHaveBoolKeys = TRUE;
		}
		else if( Track->IsA(UInterpTrackColorProp::StaticClass()) )
		{
			bHaveColorKeys = TRUE;
		}
		else if( Track->IsA(UInterpTrackVectorBase::StaticClass()) )
		{
			bHaveVectorKeys = TRUE;
		}
		else if( Track->IsA(UInterpTrackLinearColorBase::StaticClass()) )
		{
			bHaveLinearColorKeys = TRUE;
		}

		if( Track->IsA(UInterpTrackSound::StaticClass()) )
		{
			bHaveSoundKeys = TRUE;
		}

		if( Track->IsA( UInterpTrackToggle::StaticClass() ) )
		{
			bHaveToggleKeys = TRUE;
		}

		if( Track->IsA( UInterpTrackVisibility::StaticClass() ) )
		{
			bHaveVisibilityKeys = TRUE;

			UInterpTrackVisibility* VisibilityTrack = CastChecked<UInterpTrackVisibility>(Track);
			FVisibilityTrackKey& VisibilityKey = VisibilityTrack->VisibilityTrack( SelKey.KeyIndex );

			if( VisibilityKey.ActiveCondition != EVTC_Always )
			{
				bAllKeyConditionsAreSetToAlways = FALSE;
			}

			if( VisibilityKey.ActiveCondition != EVTC_GoreEnabled )
			{
				bAllKeyConditionsAreGoreEnabled = FALSE;
			}

			if( VisibilityKey.ActiveCondition != EVTC_GoreDisabled )
			{
				bAllKeyConditionsAreGoreDisabled = FALSE;
			}
		}

		if( Track->IsA( UInterpTrackAudioMaster::StaticClass() ) )
		{
			bHaveAudioMasterKeys = TRUE;
		}

		if( Track->IsA( UInterpTrackParticleReplay::StaticClass() ) )
		{
			bHaveParticleReplayKeys = TRUE;
		}
	}

	if(bHaveMoveKeys || bHaveFloatKeys || bHaveVectorKeys || bHaveColorKeys || bHaveLinearColorKeys)
	{
		wxMenu* MoveMenu = new wxMenu();
		MoveMenu->Append( IDM_INTERP_KEYMODE_CURVE_AUTO, *LocalizeUnrealEd("CurveAuto"), TEXT("") );
		MoveMenu->Append( IDM_INTERP_KEYMODE_CURVE_AUTO_CLAMPED, *LocalizeUnrealEd("CurveAutoClamped"), TEXT("") );
		MoveMenu->Append( IDM_INTERP_KEYMODE_CURVEBREAK, *LocalizeUnrealEd("CurveBreak"), TEXT("") );
		MoveMenu->Append( IDM_INTERP_KEYMODE_LINEAR, *LocalizeUnrealEd("Linear"), TEXT("") );
		MoveMenu->Append( IDM_INTERP_KEYMODE_CONSTANT, *LocalizeUnrealEd("Constant"), TEXT("") );
		Append( IDM_INTERP_MOVEKEYMODEMENU, *LocalizeUnrealEd("InterpMode"), MoveMenu );
	}

	if(InterpEd->Opt->SelectedKeys.Num() == 1)
	{
		Append( IDM_INTERP_KEY_SETTIME, *LocalizeUnrealEd("SetTime"), TEXT("") );

		FInterpEdSelKey& SelKey = InterpEd->Opt->SelectedKeys(0);

		if(bHaveMoveKeys)
		{
			AppendSeparator();

			wxMenuItem* SetLookupSourceItem = Append(IDM_INTERP_MOVEKEY_SETLOOKUP, *LocalizeUnrealEd("GetPositionFromAnotherGroup"), TEXT(""));
			wxMenuItem* ClearLookupSourceItem = Append(IDM_INTERP_MOVEKEY_CLEARLOOKUP, *LocalizeUnrealEd("ClearGroupLookup"), TEXT(""));

			UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(SelKey.Track);

			if( MoveTrack )
			{
				FName GroupName = MoveTrack->GetLookupKeyGroupName(SelKey.KeyIndex);

				if(GroupName == NAME_None)
				{
					ClearLookupSourceItem->Enable(FALSE);
				}
				else
				{
					ClearLookupSourceItem->SetText(*FString::Printf(LocalizeSecure(LocalizeUnrealEd("ClearGroupLookup_F"), *GroupName.ToString())));
				}
			}
		}

		if(bHaveFloatKeys)
		{
			Append( IDM_INTERP_KEY_SETVALUE, *LocalizeUnrealEd("SetValue"), TEXT("") );
		}

		if(bHaveBoolKeys)
		{
			UInterpTrackBoolProp* BoolPropTrack = Cast<UInterpTrackBoolProp>(SelKey.Track);

			// If the boolean value is FALSE, the user only has the option to set it to TRUE. 
			if( BoolPropTrack->BoolTrack(SelKey.KeyIndex).Value == FALSE )
			{
				Append( IDM_INTERP_KEY_SETBOOL, *LocalizeUnrealEd("SetToTrue"), TEXT("") );
			}
			// Otherwise, the boolean value is TRUE, the user only has the option to set it to FALSE. 
			else
			{
				Append( IDM_INTERP_KEY_SETBOOL, *LocalizeUnrealEd("SetToFalse"), TEXT("") );
			}
		}

		if(bHaveColorKeys || bHaveLinearColorKeys)
		{
			Append( IDM_INTERP_KEY_SETCOLOR, *LocalizeUnrealEd("SetColor"), TEXT("") );
		}

		if(bHaveEventKeys)
		{
			Append( IDM_INTERP_EVENTKEY_RENAME, *LocalizeUnrealEd("RenameEvent"), TEXT("") );
		}

		if(bHaveNotifyKeys)
		{
			Append( IDM_INTERP_NOTIFYKEY_EDIT, *LocalizeUnrealEd("EditNotify"), TEXT("") );
		}

		if(bHaveDirKeys)
		{
			Append(IDM_INTERP_DIRKEY_SETTRANSITIONTIME, *LocalizeUnrealEd("SetTransitionTime"));
			Append(IDM_INTERP_DIRKEY_RENAMECAMERASHOT, *LocalizeUnrealEd("RenameCameraShot"));
		}

		if( bHaveAudioMasterKeys )
		{
			Append( IDM_INTERP_KeyContext_SetMasterVolume, *LocalizeUnrealEd( "InterpEd_KeyContext_SetMasterVolume" ) );
			Append( IDM_INTERP_KeyContext_SetMasterPitch, *LocalizeUnrealEd( "InterpEd_KeyContext_SetMasterPitch" ) );
		}
	}

	if( bHaveToggleKeys || bHaveVisibilityKeys )
	{
		Append(IDM_INTERP_TOGGLEKEY_FLIP, *LocalizeUnrealEd("FlipToggle"));
	}

	if( bHaveVisibilityKeys )
	{
		wxMenu* ConditionMenu = new wxMenu();

		// Key condition: Always
		wxMenuItem* AlwaysItem = ConditionMenu->AppendCheckItem( IDM_INTERP_KeyContext_SetCondition_Always, *LocalizeUnrealEd( "InterpEd_KeyContext_SetCondition_Always" ) );
		AlwaysItem->Check( bAllKeyConditionsAreSetToAlways ? TRUE : FALSE );

		// Key condition: Gore Enabled
		wxMenuItem* GoreEnabledItem = ConditionMenu->AppendCheckItem( IDM_INTERP_KeyContext_SetCondition_GoreEnabled, *LocalizeUnrealEd( "InterpEd_KeyContext_SetCondition_GoreEnabled" ) );
		GoreEnabledItem->Check( bAllKeyConditionsAreGoreEnabled ? TRUE : FALSE );

		// Key condition: Gore Disabled
		wxMenuItem* GoreDisabledItem = ConditionMenu->AppendCheckItem( IDM_INTERP_KeyContext_SetCondition_GoreDisabled, *LocalizeUnrealEd( "InterpEd_KeyContext_SetCondition_GoreDisabled" ) );
		GoreDisabledItem->Check( bAllKeyConditionsAreGoreDisabled ? TRUE : FALSE );

		Append( IDM_INTERP_KeyContext_ConditionMenu, *LocalizeUnrealEd( "InterpEd_KeyContext_ConditionMenu" ), ConditionMenu );
	}

	if(bHaveAnimKeys)
	{
		Append(IDM_INTERP_ANIMKEY_LOOP, *LocalizeUnrealEd("SetAnimLooping"));
		Append(IDM_INTERP_ANIMKEY_NOLOOP, *LocalizeUnrealEd("SetAnimNoLooping"));

		if(InterpEd->Opt->SelectedKeys.Num() == 1)
		{
			Append(IDM_INTERP_ANIMKEY_SETSTARTOFFSET,  *LocalizeUnrealEd("SetStartOffset"));
			Append(IDM_INTERP_ANIMKEY_SETENDOFFSET,  *LocalizeUnrealEd("SetEndOffset"));
			Append(IDM_INTERP_ANIMKEY_SETPLAYRATE,  *LocalizeUnrealEd("SetPlayRate"));
			AppendCheckItem(IDM_INTERP_ANIMKEY_TOGGLEREVERSE,  *LocalizeUnrealEd("Reverse"));
		}
	}

	if ( bHaveSoundKeys )
	{
		Append( IDM_INTERP_SoundKey_SetVolume, *LocalizeUnrealEd("SetSoundVolume") );
		Append( IDM_INTERP_SoundKey_SetPitch, *LocalizeUnrealEd("SetSoundPitch") );


		// Does this key have a sound cue set?
		FInterpEdSelKey& SelKey = InterpEd->Opt->SelectedKeys( 0 );
		UInterpTrackSound* SoundTrack = Cast<UInterpTrackSound>( SelKey.Track );
		USoundCue* KeySoundCue = SoundTrack->Sounds( SelKey.KeyIndex ).Sound;
		if( KeySoundCue != NULL )
		{
			AppendSeparator();

			Append( IDM_INTERP_KeyContext_SyncGenericBrowserToSoundCue,
					*FString::Printf( LocalizeSecure( LocalizeUnrealEd( "InterpEd_KeyContext_SyncGenericBrowserToSoundCue_F" ),
									*KeySoundCue->GetName() ) ) );
		}
	}


	if( bHaveParticleReplayKeys)
	{
		Append( IDM_INTERP_ParticleReplayKeyContext_SetClipIDNumber, *LocalizeUnrealEd( "InterpEd_ParticleReplayKeyContext_SetClipIDNumber" ) );
		Append( IDM_INTERP_ParticleReplayKeyContext_SetDuration, *LocalizeUnrealEd( "InterpEd_ParticleReplayKeyContext_SetDuration" ) );
	}


	if( InterpEd->Opt->SelectedKeys.Num() > 0 )
	{
		AppendSeparator();
		Append( IDM_INTERP_DeleteSelectedKeys, *LocalizeUnrealEd( "InterpEd_KeyContext_DeleteSelected" ) );
	}
}

WxMBInterpEdKeyMenu::~WxMBInterpEdKeyMenu()
{

}



/*-----------------------------------------------------------------------------
	WxMBInterpEdCollapseExpandMenu
-----------------------------------------------------------------------------*/

WxMBInterpEdCollapseExpandMenu::WxMBInterpEdCollapseExpandMenu( WxInterpEd* InterpEd )
{
	Append( IDM_INTERP_ExpandAllGroups, *LocalizeUnrealEd( "InterpEdExpandAllGroups" ), *LocalizeUnrealEd( "InterpEdExpandAllGroups_Desc" ) );
	Append( IDM_INTERP_CollapseAllGroups, *LocalizeUnrealEd( "InterpEdCollapseAllGroups" ), *LocalizeUnrealEd( "InterpEdCollapseAllGroups_Desc" ) );
}


WxMBInterpEdCollapseExpandMenu::~WxMBInterpEdCollapseExpandMenu()
{
}



/*-----------------------------------------------------------------------------
	WxMBCameraAnimEdGroupMenu
-----------------------------------------------------------------------------*/

WxMBCameraAnimEdGroupMenu::WxMBCameraAnimEdGroupMenu(WxCameraAnimEd* InterpEd)
{
	// This menu can only be invoked if only one group is selected.
	check( InterpEd->GetSelectedGroupCount() == 1 );

	for(INT i=0; i<InterpEd->InterpTrackClasses.Num(); i++)
	{
		UInterpTrack* DefTrack = (UInterpTrack*)InterpEd->InterpTrackClasses(i)->GetDefaultObject();
		if( !DefTrack->bDirGroupOnly && ( DefTrack->IsA( UInterpTrackMove::StaticClass() ) || 
			DefTrack->IsA( UInterpTrackFloatProp::StaticClass() ) ||
			DefTrack->IsA( UInterpTrackVectorProp::StaticClass() ) || 
			DefTrack->IsA( UInterpTrackColorProp::StaticClass() ) ) )
		{
			FString NewTrackString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("AddNew_F"), *InterpEd->InterpTrackClasses(i)->GetDescription()) );
			Append( IDM_INTERP_NEW_TRACK_START+i, *NewTrackString, TEXT("") );
		}
	}

	AppendSeparator();

	// Add CameraAnim export option if appropriate
	UInterpGroupInst* GrInst = InterpEd->Interp->FindFirstGroupInst(*InterpEd->GetSelectedGroupIterator());
	if (GrInst)
	{
		AActor* const GroupActor = GrInst->GetGroupActor();
		UBOOL bControllingACameraActor = GroupActor && GroupActor->IsA(ACameraActor::StaticClass());
		if (bControllingACameraActor)
		{
			// add strings to unrealed.int
			Append(IDM_INTERP_CAMERA_ANIM_EXPORT, *LocalizeUnrealEd("ExportCameraAnim"), *LocalizeUnrealEd("ExportCameraAnim_Desc"));
			AppendSeparator();
		}
	}

	const UBOOL bHasAFolderSelected = InterpEd->HasAFolderSelected();

	// Copy/Paste not supported on folders yet
	if( !bHasAFolderSelected )
	{
		UBOOL bNeedsSeparator = FALSE;
		const UBOOL bHasATrackSelected = InterpEd->HasATrackSelected();
		const UBOOL bCanPaste = InterpEd->CanPasteGroupOrTrack();

		if (bHasATrackSelected)
		{
			Append (IDM_INTERP_EDIT_CUT, *LocalizeUnrealEd("CutTrack"), *LocalizeUnrealEd("InterpEd_Cut_TrackDesc"));
			Append (IDM_INTERP_EDIT_COPY, *LocalizeUnrealEd("CopyTrack"), *LocalizeUnrealEd("InterpEd_Copy_TrackDesc"));
			bNeedsSeparator = TRUE;
		}
		if (bCanPaste)
		{
			Append (IDM_INTERP_EDIT_PASTE, *LocalizeUnrealEd("PasteGroupOrTrack"), *LocalizeUnrealEd("InterpEd_Paste_GroupOrTrackDesc"));
			bNeedsSeparator = TRUE;
		}

		if (bNeedsSeparator)
		{
			AppendSeparator();
		}
	}

	Append( IDM_INTERP_GROUP_RENAME, *LocalizeUnrealEd("RenameGroup"), TEXT("") );
}

WxMBCameraAnimEdGroupMenu::~WxMBCameraAnimEdGroupMenu()
{
}

/**
 * Default constructor. 
 * Create a context menu with menu items based on the type of marker clicked-on.
 *
 * @param	InterpEd	The interp editor.
 * @param	MarkerType	The type of marker right-clicked on.
 */
WxMBInterpEdMarkerMenu::WxMBInterpEdMarkerMenu( WxInterpEd* InterpEd, EInterpEdMarkerType MarkerType )
{
	// The sequence start marker should never move. 
	// Thus, this context menu doesn't support it. 
	check( MarkerType != ISM_SeqStart );

	// Move marker to beginning of sequence
	if( ISM_LoopStart == MarkerType )
	{
		Append( IDM_INTERP_Marker_MoveToBeginning, *LocalizeUnrealEd("InterpEd_MoveToBeginning"), TEXT("") );
	}

	// Only makes sense to move the loop marker to the sequence end point.
	// Moving the sequence end marker to the sequence end would not move it and
	// moving the loop start marker would cause the loop section to be zero. 
	if( ISM_LoopEnd == MarkerType )
	{
		Append( IDM_INTERP_Marker_MoveToEnd, *LocalizeUnrealEd("InterpEd_MoveToEnd"), TEXT("") );
	}

	// Doesn't make sense to move the start loop maker to the end of 
	// the longest track because the loop section would be zero. 
	const UBOOL bCanMoveMarkerToTrackEnd = ( ISM_SeqEnd == MarkerType || ISM_LoopEnd == MarkerType );

	// In order to move a marker to the end of a track, we must actually have a track.
	if( bCanMoveMarkerToTrackEnd && InterpEd->HasATrack() )
	{
		// The user always has the option of moving the marker to the end of 
		// the longest track if we have at least one track, selected or not.
		Append( IDM_INTERP_Marker_MoveToEndOfLongestTrack, *LocalizeUnrealEd("InterpEd_MoveToEndOfLongestTrack"), TEXT("") );

		// When one or more tracks are selected, the user has the option of moving the markers 
		// to the end of the longest selected track instead of the longest overall track. 
		if( InterpEd->HasATrackSelected() )
		{
			Append( IDM_INTERP_Marker_MoveToEndOfSelectedTrack, *LocalizeUnrealEd("InterpEd_MoveToEndOfSelectedTrack"), TEXT("") );
		}
	}

	// All non-sequence start markers can be moved to the current, timeline position.
	Append( IDM_INTERP_Marker_MoveToCurrentPosition, *LocalizeUnrealEd("InterpEd_MoveToCurrentPosition"), TEXT("") );
}

/**
 * Destructor.
 */
WxMBInterpEdMarkerMenu::~WxMBInterpEdMarkerMenu()
{
}


