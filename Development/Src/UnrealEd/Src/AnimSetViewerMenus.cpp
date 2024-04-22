/*=============================================================================
	AnimSetViewerMenus.cpp: AnimSet viewer menus
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineAnimClasses.h"
#include "AnimSetViewer.h"
#include "PropertyWindow.h"
#include "DlgGenericComboEntry.h"
#include "DlgRename.h"
#include "BusyCursor.h"
#include "SoftBodyEditor.h"
#include "NvApexManager.h"
#include "NvApexCommands.h"
#include "PackageHelperFunctions.h"

#if WITH_SIMPLYGON
#include "DlgSimplifyLOD.h"
#include "SkeletalMeshSimplificationWindow.h"
#include "SimplygonMeshUtilities.h"
#endif // #if WITH_SIMPLYGON

#include <strsafe.h>

const char * gDebugCommands[IDM_ANIMSET_LAST-IDM_ANIMSET_PHYSX_CLEAR_ALL] =
{
	"PHYSX_CLEAR_ALL",
	"WORLD_AXES",
	"BODY_AXES",
	"BODY_MASS_AXES",
	"BODY_LIN_VELOCITY",
	"BODY_ANG_VELOCITY",
	"BODY_JOINT_GROUPS",
	"JOINT_LOCAL_AXES",
	"JOINT_WORLD_AXES",
	"JOINT_LIMITS",
	"CONTACT_POINT",
	"CONTACT_NORMAL",
	"CONTACT_ERROR",
	"CONTACT_FORCE",
	"ACTOR_AXES",
	"COLLISION_AABBS",
	"COLLISION_SHAPES",
	"COLLISION_AXES",
	"COLLISION_COMPOUNDS",
	"COLLISION_VNORMALS",
	"COLLISION_FNORMALS",
	"COLLISION_EDGES",
	"COLLISION_SPHERES",
	"COLLISION_STATIC",
	"COLLISION_DYNAMIC",
	"COLLISION_FREE",
	"COLLISION_CCD",
	"COLLISION_SKELETONS",
	"FLUID_EMITTERS",
	"FLUID_POSITION",
	"FLUID_VELOCITY",
	"FLUID_KERNEL_RADIUS",
	"FLUID_BOUNDS",
	"FLUID_PACKETS",
	"FLUID_MOTION_LIMIT",
	"FLUID_DYN_COLLISION",
	"FLUID_STC_COLLISION",
	"FLUID_MESH_PACKETS",
	"FLUID_DRAINS",
	"FLUID_PACKET_DATA",
	"CLOTH_MESH",
	"CLOTH_COLLISIONS",
	"CLOTH_SELFCOLLISIONS",
	"CLOTH_WORKPACKETS",
	"CLOTH_SLEEP",
	"CLOTH_SLEEP_VERTEX",
	"CLOTH_TEARABLE_VERTICES",
	"CLOTH_TEARING",
	"CLOTH_ATTACHMENT",
	"CLOTH_VALIDBOUNDS",
	"SOFTBODY_MESH",
	"SOFTBODY_COLLISIONS",
	"SOFTBODY_WORKPACKETS",
	"SOFTBODY_SLEEP",
	"SOFTBODY_SLEEP_VERTEX",
	"SOFTBODY_TEARABLE_VERTICES",
	"SOFTBODY_TEARING",
	"SOFTBODY_ATTACHMENT",
	"SOFTBODY_VALIDBOUNDS",
#if WITH_APEX
	"APEX_CLEAR_ALL",
	"VISUALIZE_CLOTHING_SKINNED_POSITIONS",
	"VISUALIZE_CLOTHING_BACKSTOP",
	"VISUALIZE_CLOTHING_MAX_DISTANCE",
	"VISUALIZE_CLOTHING_MAX_DISTANCE_IN",
	"VISUALIZE_CLOTHING_BAD_SKIN_MAP",
	"VISUALIZE_CLOTHING_SKIN_MAP",
	"VISUALIZE_CLOTHING_RENDER_NORMALS",
	"VISUALIZE_CLOTHING_RENDER_TANGENTS",
	"VISUALIZE_CLOTHING_PHYSICS_MESH_WIRE",
	"VISUALIZE_CLOTHING_PHYSICS_MESH_SOLID",
	"VISUALIZE_CLOTHING_PHYSICS_MESH_NORMALS",
	"VISUALIZE_CLOTHING_SKELETON",
	"VISUALIZE_CLOTHING_BONE_FRAMES",
	"VISUALIZE_CLOTHING_BONE_NAMES",
	"VISUALIZE_CLOTHING_VELOCITIES",
	"VISUALIZE_CLOTHING_GRAPHICAL_VERTEX_BONES",
	"VISUALIZE_CLOTHING_PHYSICAL_VERTEX_BONES",
	"VISUALIZE_CLOTHING_BACKSTOP_UMBRELLAS",
	"VISUALIZE_CLOTHING_COLLISION_VOLUMES_WIRE",
	"VISUALIZE_CLOTHING_STRETCHING_V_PHASES",
	"VISUALIZE_CLOTHING_STRETCHING_H_PHASES",
	"VISUALIZE_CLOTHING_BENDING_PHASES",
	"VISUALIZE_CLOTHING_SHEARING_PHASES",
	"VISUALIZE_CLOTHING_ZEROSTRETCH_PHASES",
	"VISUALIZE_CLOTHING_SEMIIMPLICIT_PHASES",
	"VISUALIZE_CLOTHING_GAUSSSEIDEL_PHASES",
	"VISUALIZE_CLOTHING_VIRTUAL_PARTICLES",
	"VISUALIZE_CLOTHING_STIFFNESS_SCALING",
	"VISUALIZE_CLOTHING_SHOW_LOCAL_SPACE",
	"VISUALIZE_CLOTHING_SHOW_GLOBAL_POSE",
	"VISUALIZE_CLOTHING_RECOMPUTE_SUBMESHES",
	"VISUALIZE_CLOTHING_RECOMPUTE_VERTICES",

	"VISUALIZE_DESTRUCTIBLE_BOUNDS",
	"VISUALIZE_DESTRUCTIBLE_SUPPORT",
	"VISUALIZE_DESTRUCTIBLE_ACTOR_POSE",
	"VISUALIZE_DESTRUCTIBLE_FRAGMENT_ACTOR_POSE",
	"VISUALIZE_DESTRUCTIBLE_ACTOR_NAME",
	"LOD_DISTANCE_DESTRUCTIBLE_ACTOR_POSE",
	"LOD_DISTANCE_DESTRUCTIBLE_ACTOR_NAME",
	"LOD_DISTANCE_DESTRUCTIBLE_FRAGMENT_ACTOR_POSE",
	"RENDERNORMALS",
	"RENDERTANGENTS",
	"RENDERBITANGENTS",
	"VISUALIZE_LOD_BENEFITS",
	"LOD_DISTANCE_SCALE",
#if WITH_APEX_PARTICLES
	"VISUALIZE_TOTAL_INJECTED_AABB",
	"VISUALIZE_APEX_EMITTER_ACTOR_POSE",
	"THRESHOLD_DISTANCE_APEX_EMITTER_ACTOR_POSE",
	"VISUALIZE_APEX_EMITTER_ACTOR_NAME",
	"THRESHOLD_DISTANCE_APEX_EMITTER_ACTOR_NAME",
	"VISUALIZE_GROUND_EMITTER_SPHERE",
	"VISUALIZE_GROUND_EMITTER_GRID",
	"VISUALIZE_GROUND_EMITTER_RAYCAST",
	"VISUALIZE_GROUND_EMITTER_ACTOR_POSE",
	"VISUALIZE_GROUND_EMITTER_ACTOR_NAME",
	"VISUALIZE_IMPACT_EMITTER_RAYCAST",
	"VISUALIZE_IMPACT_EMITTER_ACTOR_NAME",
	"THRESHOLD_DISTANCE_IMPACT_EMITTER_ACTOR_NAME",

	"VISUALIZE_IOFX_BOUNDING_BOX",
	"APEX_VISUALIZE_IOFX_ACTOR_NAME",
#endif

#endif
};

///////////////////// MENU HANDLERS

/**
 *	Called when a SIZE event occurs on the window
 *
 *	@param	In		The size event information
 */
void WxAnimSetViewer::OnSize( wxSizeEvent& In )
{
	In.Skip();
	Refresh();
	UpdatePreviewWindow();
}

/**
 *	Called when a UNDO event occurs on the window
 *
 *	@param	In		The event information
 */
void WxAnimSetViewer::OnMenuEditUndo( wxCommandEvent& In )
{
	FASVViewportClient* ViewportClient = GetPreviewVC();
	ensure(ViewportClient);

	if (ViewportClient)
	{
		UndoTransaction();
		if (PreviewSkelComp != NULL)
		{
			PreviewSkelComp->BeginDeferredReattach();
		}

		if (PreviewSkelCompRaw != NULL)
		{
			PreviewSkelCompRaw->BeginDeferredReattach();
		}

		ViewportClient->Viewport->Invalidate();
	}
}

/**
 *	Called when a REDO event occurs on the window
 *
 *	@param	In		The event information
 */
void WxAnimSetViewer::OnMenuEditRedo( wxCommandEvent& In )
{
	FASVViewportClient* ViewportClient = GetPreviewVC();
	ensure(ViewportClient);

	if (ViewportClient)
	{
		RedoTransaction();
		if (PreviewSkelComp != NULL)
		{
			PreviewSkelComp->BeginDeferredReattach();
		}

	if (PreviewSkelCompRaw != NULL)
		{
			PreviewSkelCompRaw->BeginDeferredReattach();
		}

		ViewportClient->Viewport->Invalidate();
	}
}

void WxAnimSetViewer::OnSkelMeshComboChanged(wxCommandEvent& In)
{
	const INT SelIndex = SkelMeshCombo->GetSelection();
	// Should not be possible to select no skeletal mesh!
	if( SelIndex != -1 )
	{
		USkeletalMesh* NewSelectedSkelMesh = (USkeletalMesh*)SkelMeshCombo->GetClientData(SelIndex);
		SetSelectedSkelMesh( NewSelectedSkelMesh, TRUE );
	}
}

void WxAnimSetViewer::OnAuxSkelMeshComboChanged(wxCommandEvent& In)
{
	USkeletalMeshComponent* AuxComp = NULL;
	USkeletalMesh* NewMesh = NULL;

	if(In.GetId() == IDM_ANIMSET_SKELMESHAUX1COMBO)
	{
		AuxComp = PreviewSkelCompAux1;

		if(SkelMeshAux1Combo->GetSelection() != -1)
		{
			NewMesh = (USkeletalMesh*)SkelMeshAux1Combo->GetClientData( SkelMeshAux1Combo->GetSelection() );
		}
	}
	else if(In.GetId() == IDM_ANIMSET_SKELMESHAUX2COMBO)
	{
		AuxComp = PreviewSkelCompAux2;

		if(SkelMeshAux2Combo->GetSelection() != -1)
		{
			NewMesh = (USkeletalMesh*)SkelMeshAux2Combo->GetClientData( SkelMeshAux2Combo->GetSelection() );
		}
	}
	else if(In.GetId() == IDM_ANIMSET_SKELMESHAUX3COMBO)
	{
		AuxComp = PreviewSkelCompAux3;

		if(SkelMeshAux3Combo->GetSelection() != -1)
		{
			NewMesh = (USkeletalMesh*)SkelMeshAux3Combo->GetClientData( SkelMeshAux3Combo->GetSelection() );
		}
	}

	if(AuxComp)
	{
		FASVViewportClient* ASVPreviewVC = GetPreviewVC();
		check( ASVPreviewVC );

		AuxComp->SetSkeletalMesh(NewMesh);
		AuxComp->UpdateParentBoneMap();
		ASVPreviewVC->PreviewScene.AddComponent(AuxComp,FMatrix::Identity);
	}
}

void WxAnimSetViewer::OnAnimSetComboChanged(wxCommandEvent& In)
{
	const INT SelIndex = AnimSetCombo->GetSelection();
	if( SelIndex != -1)
	{
		UAnimSet* NewSelectedAnimSet = (UAnimSet*)AnimSetCombo->GetClientData(SelIndex);
		SetSelectedAnimSet( NewSelectedAnimSet, TRUE );
	}
}

void WxAnimSetViewer::OnSkelMeshUse( wxCommandEvent& In )
{
	GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);

	USkeletalMesh* SelectedSkelMesh = GEditor->GetSelectedObjects()->GetTop<USkeletalMesh>();
	if(SelectedSkelMesh)
	{
		SetSelectedSkelMesh(SelectedSkelMesh, TRUE);
	}
}

void WxAnimSetViewer::OnAnimSetUse( wxCommandEvent& In )
{
	if( bSearchAllAnimSequences )
	{
		// If the user is searching all AnimSets, then the AnimSet combo 
		// was disabled. Enable it so the user can interact with it. 
		AnimSetCombo->Enable();

		// Disable 'Search All' mode since we are filtering by a specific AnimSet. 
		bSearchAllAnimSequences = FALSE;
	}

	GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);
	UAnimSet* SelectedAnimSet = GEditor->GetSelectedObjects()->GetTop<UAnimSet>();
	if(SelectedAnimSet)
	{
		SetSelectedAnimSet(SelectedAnimSet, TRUE);
	}
}

/**
 * Event handler that enables or disables showing all anim sequences. 
 *
 * @param	In	Event triggered by wxWidgets when the "Show All Sequences" button is pushed. 
 */
void WxAnimSetViewer::OnAnimSeqSearchTypeChange( wxCommandEvent& In )
{
	const UBOOL bAllSequencesWereShown = bSearchAllAnimSequences;
	bSearchAllAnimSequences = !bSearchAllAnimSequences;

	// When the user first turns on the option to view all anim sequences, we have 
	// to load all anim sets into memory. Ask the user if they are OK with that. 
	if( bSearchAllAnimSequences && bPromptUserToLoadAllAnimSets )
	{
		WxChoiceDialog LoadOptionDialog(
			*LocalizeUnrealEd( TEXT("AnimSetViewer_LoadAllAnimSetsPrompt") ),
			*LocalizeUnrealEd( TEXT("AnimSetViewer_ShowAllSequences") ),
			WxChoiceDialogBase::Choice( AMT_OK, *LocalizeUnrealEd( TEXT("AnimSetViewer_LoadAllAnimSets") ), WxChoiceDialogBase::DCT_DefaultAffirmative ),
			WxChoiceDialogBase::Choice( AMT_OKCancel, *LocalizeUnrealEd( TEXT("AnimSetViewer_UseLoadedAnimSetsOnly") ), WxChoiceDialogBase::DCT_DefaultCancel )
			);

		LoadOptionDialog.ShowModal();
		
		// We should respect the user's decision for the duration that this editor is open. 
		bPromptUserToLoadAllAnimSets = FALSE;

		// If the user wants to load all anim sets, use the GAD to figure out the paths of all anim sets. 
		if( LoadOptionDialog.GetChoice().ReturnCode == AMT_OK )
		{
			GWarn->BeginSlowTask( *LocalizeUnrealEd( TEXT("AnimSetViewer_LoadingAllAnimSets") ), TRUE);

			FGADHelper* GADHelper = new FGADHelper();
			if( GADHelper->Initialize() )
			{
				TArray<FString> AnimSetDBEntries;
				TArray<FString> AnimSetTags;

				// The GAD appends system tag type to the asset's class name when storing the asset in the GAD. We need to replicate this 
				AnimSetTags.AddItem( TEXT("[ObjectType]AnimSet") );

				// Gather all AnimSets that are in the GAD.
				GADHelper->QueryAssetsWithAllTags( AnimSetTags, AnimSetDBEntries );

				for( INT AnimSetNameIndex = 0; AnimSetNameIndex < AnimSetDBEntries.Num(); ++AnimSetNameIndex )
				{
					const FString& AssetNameWithClass = AnimSetDBEntries( AnimSetNameIndex );

					// Extract the class name, which is appended to the front. 
					// Otherwise, the load object will fail. 
					INT SpacePos = AssetNameWithClass.InStr( TEXT(" ") );
					FString AssetName = AssetNameWithClass.Right( AssetNameWithClass.Len() - (SpacePos + 1) );

					// Attempt to find the object
					UObject* Object = UObject::StaticFindObject( UAnimSet::StaticClass(), ANY_PACKAGE, *AssetName, FALSE );
					if( !Object )
					{
						// Since we couldn't find it, try to load it.
						Object = UObject::StaticLoadObject( UAnimSet::StaticClass(), NULL, *AssetName, NULL, LOAD_NoRedirects, NULL );
					}
				}
			}

			delete GADHelper;
			GWarn->EndSlowTask();
		}
	}

	// Only update the GUI if the search all feature was toggled. 
	if( bAllSequencesWereShown != bSearchAllAnimSequences )
	{
		// Change the state of the AnimSet combo box to give the user some feedback on 
		// whether the list of sequences are filter by the anim set or all are visible. 
		AnimSetCombo->Enable( FALSE == bSearchAllAnimSequences );

		// Reselect the anim set and sequence that were selected because the entries may have been added or 
		// removed to the anim set combo box or anim sequence list box. Thus, we must update the selections.
		UAnimSequence* PrevSelectedSeq = SelectedAnimSeq;
		SetSelectedAnimSet( SelectedAnimSet, TRUE );
		SetSelectedAnimSequence(PrevSelectedSeq);
	}
}

/**
 * Called when the user changes the text in the anim. sequence filter text control
 *
 * @param	In	Event generated by wxWidgets when the user changes the text in the filter control
 */
void WxAnimSetViewer::OnAnimSeqFilterTextChanged( wxCommandEvent& In )
{
	// Reset the timer if it's already running
	if ( AnimSeqFilterTimer.IsRunning() )
	{
		AnimSeqFilterTimer.Stop();
	}
	const INT TimerDelay = 500;
	AnimSeqFilterTimer.Start( TimerDelay, wxTIMER_ONE_SHOT );
}

/**
 * Called when the user presses the Enter key inside the anim. sequence filter text control; highlights all of the text
 *
 * @param	In	Event generated by wxWidgets when the user presses enter in the filter control
 */
void WxAnimSetViewer::OnAnimSeqFilterEnterPressed( wxCommandEvent& In )
{
	AnimSeqFilter->SelectAll();
}

/**
 * Called when the user clicks the clear anim. seq. search filter button
 *
 * @param	In	Event generated by wxWidgets when the user clicks the button to clear the anim. seq. search filter
 */
void WxAnimSetViewer::OnClearAnimSeqFilter( wxCommandEvent& In )
{
	// If there is text in the filter, clear it
	if ( AnimSeqFilter->GetValue().Len() > 0 )
	{
		AnimSeqFilter->SetValue( TEXT("") );
	}
}

/**
 * Called by wxWidgets to update the UI for the clear anim. seq. search filter button
 *
 * @param	In	Event generated by wxWidgets to update the UI
 */
void WxAnimSetViewer::OnClearAnimSeqFilter_UpdateUI( wxUpdateUIEvent& In )
{
	// Disable the clear button if the filter is empty
	In.Enable( AnimSeqFilter->GetValue().Len() > 0 );
}

/**
 * Called when the timer for updating the anim. seq. filter expires
 *
 * @param	In	Event generated by wxWidgets when the timer expires
 */
void WxAnimSetViewer::OnAnimSeqFilterTimer( wxTimerEvent& In )
{
	// Update the anim. sequence list with the new filter string
	UpdateAnimSeqList();

	// See if the previously selected anim. sequence, if any, is still within the
	// the anim. sequence list box. If it is, select it again. If not, don't select
	// anything.
	UBOOL bSelAnimSeqPresent = FALSE;
	if ( SelectedAnimSeq )
	{
		for ( UINT AnimSeqIndex = 0;  AnimSeqIndex < AnimSeqList->GetCount(); ++AnimSeqIndex )
		{
			if( AnimSeqList->GetClientData( AnimSeqIndex ) == SelectedAnimSeq )
			{
				bSelAnimSeqPresent = TRUE;
				break;
			}
		}	
	}
	SetSelectedAnimSequence( bSelAnimSeqPresent ? SelectedAnimSeq : NULL );
}

void WxAnimSetViewer::OnAuxSkelMeshUse( wxCommandEvent& In )
{
	GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);

	USkeletalMesh* SelectedSkelMesh = GEditor->GetSelectedObjects()->GetTop<USkeletalMesh>();
	if(SelectedSkelMesh)
	{
		USkeletalMeshComponent* AuxComp = NULL;
		wxComboBox* AuxCombo = NULL;

		if(In.GetId() == IDM_ANIMSET_SKELMESH_AUX1USE)
		{
			AuxComp = PreviewSkelCompAux1;
			AuxCombo = SkelMeshAux1Combo;
		}
		else if(In.GetId() == IDM_ANIMSET_SKELMESH_AUX2USE)
		{
			AuxComp = PreviewSkelCompAux2;
			AuxCombo = SkelMeshAux2Combo;
		}
		else if(In.GetId() == IDM_ANIMSET_SKELMESH_AUX3USE)
		{
			AuxComp = PreviewSkelCompAux3;
			AuxCombo = SkelMeshAux3Combo;
		}

		if(AuxComp && AuxCombo)
		{
			// Set the skeletal mesh component to the new mesh.
			AuxComp->SetSkeletalMesh(SelectedSkelMesh);
			AuxComp->UpdateParentBoneMap();

			// Update combo box to point at new selected mesh.

			// First, refresh the contents, in case we have loaded a package since opening the AnimSetViewer.
			AuxCombo->Freeze();
			AuxCombo->Clear();
			AuxCombo->Append( *LocalizeUnrealEd("-None-"), (void*)NULL );
			for(TObjectIterator<USkeletalMesh> It; It; ++It)
			{
				USkeletalMesh* ItSkelMesh = *It;
				AuxCombo->Append( *ItSkelMesh->GetName(), ItSkelMesh );
			}
			AuxCombo->Thaw();

			// Then look for the skeletal mesh.
			for(UINT i=0; i<AuxCombo->GetCount(); i++)
			{
				if(AuxCombo->GetClientData(i) == SelectedSkelMesh)
				{
					AuxCombo->SetSelection(i);
				}
			}
		}
	}
}

void WxAnimSetViewer::OnAnimSeqListChanged(wxCommandEvent& In)
{
	const INT SelIndex = In.GetInt();// AnimSeqList->GetSelection();
	if( SelIndex != -1 )
	{
		UAnimSequence* NewSelectedAnimSeq = (UAnimSequence*)AnimSeqList->GetClientData(SelIndex);

		// When all sequences are shown, the user may select a sequence in the list contained in a different anim set. In that case, we must update the selected anim set. 
		if( bSearchAllAnimSequences && (SelectedAnimSet != NewSelectedAnimSeq->GetOuter()) )
		{
			UAnimSet* AnimSetOwner = CastChecked<UAnimSet>(NewSelectedAnimSeq->GetOuter());
			const UBOOL bUpdateSelectedAnimSeq = FALSE;
			SetSelectedAnimSet( AnimSetOwner, TRUE, bUpdateSelectedAnimSeq );
		}

		SetSelectedAnimSequence(NewSelectedAnimSeq);
	}
}

void WxAnimSetViewer::OnAnimSeqListRightClick(wxMouseEvent& In)
{
	if (MenuBar && MenuBar->AnimSeqMenu)
	{
		FTrackPopupMenu TrackPopupMenu(this, MenuBar->AnimSeqMenu);
		TrackPopupMenu.Show();
	}
}

/** Create a new AnimSet, defaulting to in the package of the selected SkeletalMesh. */
void WxAnimSetViewer::OnNewAnimSet(wxCommandEvent& In)
{
	CreateNewAnimSet();
}

void WxAnimSetViewer::OnImportPSA(wxCommandEvent& In)
{
	ImportPSA();
}

#if WITH_FBX
void WxAnimSetViewer::OnImportFbxAnim(wxCommandEvent& In)
{
	ImportFbxAnim();
}

void WxAnimSetViewer::OnExportFbxAnim(wxCommandEvent& In)
{
	ExportFbxAnim();
}
#endif // WITH_FBX

void WxAnimSetViewer::OnImportMeshLOD( wxCommandEvent& In )
{
	ImportMeshLOD();
}

void WxAnimSetViewer::OnImportMeshWeights( wxCommandEvent& In )
{
	ImportMeshWeights();
	FillSkeletonTree();
}

#if WITH_SIMPLYGON
void WxAnimSetViewer::OnGenerateLOD( wxCommandEvent& In )
{
	if ( SelectedSkelMesh )
	{
		// Combo options to choose target LOD
		const INT MaxAllowedLOD = 3;
		TArray<FString> LODStrings;
		INT CurrentMaxLOD = SelectedSkelMesh->LODModels.Num() - 1;
		INT NewMaxLOD = Min( CurrentMaxLOD + 1, MaxAllowedLOD );
		LODStrings.AddZeroed( NewMaxLOD );
		for( INT LODIndex = 0; LODIndex < NewMaxLOD; ++LODIndex )
		{
			if ( LODIndex + 1 > CurrentMaxLOD )
			{
				LODStrings( LODIndex ) = LocalizeUnrealEd( "StaticMeshEditor_AddNewLOD" ) + FString::Printf( TEXT(" %d"), LODIndex + 1 );
			}
			else
			{
				LODStrings( LODIndex ) = LocalizeUnrealEd( "StaticMeshEditor_ReplaceLOD" ) + FString::Printf( TEXT(" %d"), LODIndex + 1 );
			}
		}

		WxDlgGenericComboEntry ChooseLODDlg;	
		if( ChooseLODDlg.ShowModal( TEXT("ChooseLODLevel"), TEXT("LODLevel:"), LODStrings, 0, TRUE ) == wxID_OK )
		{
			const INT DesiredLOD = ChooseLODDlg.GetComboBox().GetSelection()+1;
			check( DesiredLOD >= 1 );

			WxDlgSimplifyLOD SimplifyLODDialog( this, *LocalizeUnrealEd( TEXT("GenerateLOD") ) );
			if( SimplifyLODDialog.ShowModal() == wxID_OK )
			{
				FSkeletalMeshOptimizationSettings OptimizationSettings;
				const FLOAT ViewDistance = SimplifyLODDialog.GetViewDistance();
				const FLOAT MaxDeviation = SimplifyLODDialog.GetMaxDeviation();
				OptimizationSettings.MaxDeviationPercentage = MaxDeviation / SelectedSkelMesh->Bounds.SphereRadius;
				GWarn->BeginSlowTask( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MeshSimp_GeneratingLOD_F" ), DesiredLOD, *SelectedSkelMesh->GetName() ) ), TRUE );
				if ( SimplygonMeshUtilities::OptimizeSkeletalMesh( SelectedSkelMesh, DesiredLOD, OptimizationSettings ) )
				{
					check( SelectedSkelMesh->LODInfo.Num() >= 2 );
					if ( SelectedSkelMesh->LODInfo( DesiredLOD ).DisplayFactor == 0.0f )
					{
						SelectedSkelMesh->LODInfo( DesiredLOD ).DisplayFactor = 2.0f * SelectedSkelMesh->Bounds.SphereRadius / ViewDistance;
					}
					SelectedSkelMesh->MarkPackageDirty();
				}
				else
				{
					// Simplification failed! Warn the user.
					appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MeshSimp_GenerateLODFailed_F" ), *SelectedSkelMesh->GetName() ) ) );
				}
				GWarn->EndSlowTask();
				UpdateForceLODMenu();
				UpdateStatusBar();
				SimplificationWindow->UpdateControls();
			}
		}
	}
}
#endif // #if WITH_SIMPLYGON

// Toggle Mesh Weights depending on boolean
void WxAnimSetViewer::ToggleMeshWeights(UBOOL bEnable)
{
	if( PreviewSkelComp &&
		PreviewSkelComp->SkeletalMesh )
	{
		bPreviewInstanceWeights = bEnable;
		for (INT LODIdx=0;LODIdx<PreviewSkelComp->LODInfo.Num();LODIdx++)
		{
			  const FSkelMeshComponentLODInfo& MeshLODInfo = PreviewSkelComp->LODInfo(LODIdx);
			  
			  if( bPreviewInstanceWeights )
			  {
				  // enable usage of instanced weights
				  PreviewSkelComp->ToggleInstanceVertexWeights(TRUE, LODIdx);
				  if (MeshLODInfo.InstanceWeightUsage == IWU_PartialSwap)
				  {
					  // use all ref skeleton bones so that all vertices use the instanced weights
					  TArray<FBonePair> BonePairs;
					  BonePairs.Empty(PreviewSkelComp->SkeletalMesh->RefSkeleton.Num());
					  BonePairs.Add(PreviewSkelComp->SkeletalMesh->RefSkeleton.Num());
					  for (INT i = 0; i < PreviewSkelComp->SkeletalMesh->RefSkeleton.Num(); i++)
					  {
						  FBonePair& BonePair = BonePairs(i);
						  BonePair.Bones[0] = PreviewSkelComp->SkeletalMesh->RefSkeleton(i).Name;
						  INT ParentIdx = PreviewSkelComp->SkeletalMesh->RefSkeleton(i).ParentIndex;
						  if (ParentIdx != INDEX_NONE)
						  {
							  BonePair.Bones[1] = PreviewSkelComp->SkeletalMesh->RefSkeleton(ParentIdx).Name;
						  }
						  else
						  {
							  BonePair.Bones[1] = NAME_None;
						  }
					  }

					  PreviewSkelComp->UpdateInstanceVertexWeightBones(BonePairs);
				  }
			  }
			  else
			  {
				  // disable usage of instanced weights (respect Gore Mode flag)
				  PreviewSkelComp->ToggleInstanceVertexWeights(PreviewSkelComp->bDrawBoneInfluences, LODIdx);
			  }
		}

		// toggle label
		MenuBar->Check(IDM_ANIMSET_TOGGLEMESHWEIGHTS, bPreviewInstanceWeights == TRUE);
	}

	// Update the weights
	FillSkeletonTree();
}

// Event handler for toggle mesh weights
void WxAnimSetViewer::OnToggleMeshWeights( wxCommandEvent& In )
{
	ToggleMeshWeights(!bPreviewInstanceWeights);
	UpdateAltBoneWeightingMenu();
}

/** Enable/disable showing all vertices (vs just selected bone) shown while editing bone weights */
void WxAnimSetViewer::OnToggleShowAllMeshVerts( wxCommandEvent& In )
{
	bDrawAllBoneInfluenceVertices = !bDrawAllBoneInfluenceVertices;
	PreviewSkelComp->BeginDeferredReattach();
	// toggle label
	MenuBar->Check(IDM_ANIMSET_BONEEDITING_SHOWALLMESHVERTS, bDrawAllBoneInfluenceVertices ? true : false);
}

//////////////////////////////////////////////////////////////////////////
// MORPH TARGETS

void WxAnimSetViewer::OnNewMorphTargetSet( wxCommandEvent& In )
{
	if(!SelectedSkelMesh)
	{
		return;
	}

	FString Package = TEXT("");
	FString Group = TEXT("");

	// Use the selected skeletal mesh to find the 'default' package for the new MorphTargetSet.
	// Bit yucky this...
	check( SelectedSkelMesh->GetOuter() );

	// If there are 2 levels above this mesh - top is packages, then group
	if( SelectedSkelMesh->GetOuter()->GetOuter() )
	{
		Group = SelectedSkelMesh->GetOuter()->GetFullGroupName(0);
		Package = SelectedSkelMesh->GetOuter()->GetOuter()->GetName();
	}
	else // Otherwise, just a package.
	{
		Group = TEXT("");
		Package = SelectedSkelMesh->GetOuter()->GetName();
	}

	WxDlgRename RenameDialog;
	RenameDialog.SetTitle( *LocalizeUnrealEd("NewMorphTargetSet") );
	if( RenameDialog.ShowModal( Package, Group, TEXT("NewMorphTargetSet") ) == wxID_OK )
	{
		if( RenameDialog.GetNewName().Len() == 0 || RenameDialog.GetNewPackage().Len() == 0 )
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Error_MustSpecifyNewMorphSetName") );
			return;
		}

		UPackage* Pkg = GEngine->CreatePackage(NULL,*RenameDialog.GetNewPackage());
		if( RenameDialog.GetNewGroup().Len() )
		{
			Pkg = GEngine->CreatePackage(Pkg,*RenameDialog.GetNewGroup());
		}

		UMorphTargetSet* NewMorphSet = ConstructObject<UMorphTargetSet>( UMorphTargetSet::StaticClass(), Pkg, FName( *RenameDialog.GetNewName() ), RF_Public|RF_Standalone );

		// Remember this as the base mesh that the morph target will modify
		NewMorphSet->BaseSkelMesh = SelectedSkelMesh;

		// Will update MorphTargetSet list, which will include new AnimSet.
		SetSelectedSkelMesh(SelectedSkelMesh, false);
		SetSelectedMorphSet(NewMorphSet, false);

		SelectedMorphSet->MarkPackageDirty();
	}
}

void WxAnimSetViewer::OnImportMorphTarget( wxCommandEvent& In )
{
	ImportMorphTarget(FALSE, FALSE);
}

void WxAnimSetViewer::OnImportMorphTargetLOD( wxCommandEvent& In )
{
	ImportMorphTarget(TRUE, FALSE);
}

void WxAnimSetViewer::OnImportMorphTargets( wxCommandEvent& In )
{
	ImportMorphTarget(FALSE, TRUE);
}

void WxAnimSetViewer::OnImportMorphTargetsLOD( wxCommandEvent& In )
{
	ImportMorphTarget(TRUE, TRUE);
}

void WxAnimSetViewer::OnMorphSetComboChanged( wxCommandEvent& In )
{
	const INT SelIndex = MorphSetCombo->GetSelection();
	if( SelIndex != -1 )
	{
		UMorphTargetSet* NewSelectedMorphSet = (UMorphTargetSet*)MorphSetCombo->GetClientData(SelIndex);
		SetSelectedMorphSet( NewSelectedMorphSet, true );
	}
}

void WxAnimSetViewer::OnMorphSetUse( wxCommandEvent& In )
{
	GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);

	UMorphTargetSet* SelectedMorphSet = GEditor->GetSelectedObjects()->GetTop<UMorphTargetSet>();
	if(SelectedMorphSet)
	{
		SetSelectedMorphSet(SelectedMorphSet, true);
	}
}
/*
 * Rename morph target [index] to new name
 */
void WxAnimSetViewer::OnMorphTargetTextChanged( wxCommandEvent& In )
{
	// Rename [index] to new name
	RenameMorphTarget(In.GetId() - IDM_ANIMSET_MORPHTARGETS_START, In.GetString().c_str());
}

/*
* Update morph target [index]'s weight to input
*/
void WxAnimSetViewer::OnMorphTargetWeightChanged( wxCommandEvent& In )
{
	INT Index = In.GetId() - IDM_ANIMSET_MORPHTARGETWEIGHTS_START;
	
	UpdateMorphTargetWeight(In.GetId() - IDM_ANIMSET_MORPHTARGETWEIGHTS_START, In.GetInt()/100.f);
}

/*
* Update selected morph targets
*/
void WxAnimSetViewer::OnSelectMorphTarget( wxCommandEvent& In )
{
	UpdateSelectedMorphTargets();
}

/*
* Reset all morph targets weight to 0
*/
void WxAnimSetViewer::OnResetMorphTargetPreview( wxCommandEvent& In )
{
	MorphTargetPanel->ResetAllWeights();
	ResetMorphTargets();
}

/*
* Delete selected morph targets
*/
void WxAnimSetViewer::OnDeleteMorphTarget( wxCommandEvent& In )
{
	DeleteSelectedMorphTarget();
}

/*
* Select/Deselect all morph targets 
*/
void WxAnimSetViewer::OnSelectAllMorphTargets( wxCommandEvent& In )
{
	MorphTargetPanel->SelectAll(In.IsChecked());
	UpdateSelectedMorphTargets();
}

/*
* Remap vertices of the selected morph targets
*/
void WxAnimSetViewer::OnUpdateMorphTarget( wxCommandEvent& In )
{
	RemapVerticesSelectedMorphTarget();
}
//
//////////////////////////////////////////////////////////////////////////

void WxAnimSetViewer::OnTimeScrub(wxScrollEvent& In)
{
	if(SelectedAnimSeq)
	{
		check(SelectedAnimSeq->SequenceName == PreviewAnimNode->AnimSeqName && SelectedAnimSeq->SequenceName == PreviewAnimNodeRaw->AnimSeqName);
		const INT NewIntPosition = In.GetPosition();
		const FLOAT NewPosition = (FLOAT)NewIntPosition/(FLOAT)ASV_SCRUBRANGE;
		PreviewAnimNode->SetPosition( NewPosition * SelectedAnimSeq->SequenceLength, false );
		PreviewAnimNodeRaw->SetPosition( NewPosition * SelectedAnimSeq->SequenceLength, false );
	}
}

void WxAnimSetViewer::OnViewBones(wxCommandEvent& In)
{
	PreviewSkelComp->bDisplayBones = In.IsChecked();
	PreviewSkelComp->BeginDeferredReattach();
	MenuBar->Check( IDM_ANIMSET_VIEWBONES, PreviewSkelComp->bDisplayBones == TRUE );
	ToolBar->ToggleTool( IDM_ANIMSET_VIEWBONES, PreviewSkelComp->bDisplayBones == TRUE );
}

void WxAnimSetViewer::OnShowRawAnimation(wxCommandEvent& In)
{
	PreviewSkelCompRaw->bDisplayBones = In.IsChecked();
	PreviewSkelCompRaw->BeginDeferredReattach();
	MenuBar->Check( IDM_ANIMSET_ShowRawAnimation, PreviewSkelCompRaw->bDisplayBones == TRUE );
	ToolBar->ToggleTool( IDM_ANIMSET_ShowRawAnimation, PreviewSkelCompRaw->bDisplayBones == TRUE );
}

void WxAnimSetViewer::OnViewBoneNames(wxCommandEvent& In)
{
	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check( ASVPreviewVC );

	ASVPreviewVC->bShowBoneNames = In.IsChecked();
	MenuBar->Check( IDM_ANIMSET_VIEWBONENAMES, ASVPreviewVC->bShowBoneNames == TRUE );
	ToolBar->ToggleTool( IDM_ANIMSET_VIEWBONENAMES, ASVPreviewVC->bShowBoneNames == TRUE );
}

void WxAnimSetViewer::OnViewFloor(wxCommandEvent& In)
{
	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check( ASVPreviewVC );

	ASVPreviewVC->bShowFloor = In.IsChecked();

	UpdateFloorComponent();
}

void WxAnimSetViewer::OnViewMorphKeys(wxCommandEvent& In)
{
	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check( ASVPreviewVC );

	ASVPreviewVC->bShowMorphKeys = In.IsChecked();
	MenuBar->Check( IDM_ANIMSET_VIEWMORPHKEYS, ASVPreviewVC->bShowMorphKeys == TRUE );
	ToolBar->ToggleTool( IDM_ANIMSET_VIEWMORPHKEYS, ASVPreviewVC->bShowMorphKeys == TRUE );
}

void WxAnimSetViewer::OnViewRefPose(wxCommandEvent& In)
{
	PreviewSkelComp->bForceRefpose = In.IsChecked();
	PreviewSkelCompRaw->bForceRefpose = In.IsChecked();
	MenuBar->Check( IDM_ANIMSET_VIEWREFPOSE, PreviewSkelComp->bForceRefpose == TRUE );
	ToolBar->ToggleTool( IDM_ANIMSET_VIEWREFPOSE, PreviewSkelComp->bForceRefpose == TRUE );
}

void WxAnimSetViewer::OnViewMirror(wxCommandEvent& In)
{
	PreviewAnimMirror->bEnableMirroring = In.IsChecked();
	MenuBar->Check( IDM_ANIMSET_VIEWMIRROR, PreviewAnimMirror->bEnableMirroring );
	ToolBar->ToggleTool( IDM_ANIMSET_VIEWMIRROR, PreviewAnimMirror->bEnableMirroring );
}

void WxAnimSetViewer::OnViewBounds(wxCommandEvent& In)
{
	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check( ASVPreviewVC );

	if( In.IsChecked() )
	{
		ASVPreviewVC->ShowFlags |= SHOW_Bounds;
	}
	else
	{
		ASVPreviewVC->ShowFlags &= ~SHOW_Bounds;
	}
}

void WxAnimSetViewer::OnViewCollision(wxCommandEvent& In)
{
	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check( ASVPreviewVC );

	if( In.IsChecked() )
	{
		ASVPreviewVC->ShowFlags |= SHOW_Collision;
	}
	else
	{
		ASVPreviewVC->ShowFlags &= ~SHOW_Collision;
	}
}

void WxAnimSetViewer::OnViewSoftBodyTetra(wxCommandEvent& In)
{
	check( PreviewSkelComp );

	PreviewSkelComp->bShowSoftBodyTetra = In.IsChecked();
	PreviewSkelComp->BeginDeferredReattach();
	MenuBar->Check(IDM_ANIMSET_VIEWSOFTBODYTETRA, PreviewSkelComp->bShowSoftBodyTetra == TRUE);
}

void WxAnimSetViewer::OnViewClothMoveDistScale(wxCommandEvent& In)
{
	bShowClothMovementScale = In.IsChecked();
	MenuBar->Check(IDM_ANIMSET_VIEWCLOTHMOVEDISTSCALE, bShowClothMovementScale == TRUE);
}

// Alternative Bone Weighting Edit mode
void WxAnimSetViewer::OnEditAltBoneWeightingMode(wxCommandEvent& In)
{									  
	check (PreviewSkelComp);

	EnableAltBoneWeighting(In.IsChecked()? TRUE : FALSE);
}

// Update Alternative Bone Weighting menu
void WxAnimSetViewer::UpdateAltBoneWeightingMenu()
{
	check (PreviewSkelComp);

	if ( PreviewSkelComp->SkeletalMesh )
	{
		INT LOD = ::Clamp(PreviewSkelComp->PredictedLODLevel, PreviewSkelComp->MinLodModel, PreviewSkelComp->SkeletalMesh->LODModels.Num()-1);
		const FStaticLODModel & LODModel = PreviewSkelComp->SkeletalMesh->LODModels(LOD);
		
		UBOOL bEnableAltWeightMode = FALSE;
		if (LODModel.VertexInfluences.Num() > 0)
		{
		   const FSkeletalMeshVertexInfluences& VertexInfluences = LODModel.VertexInfluences(0);
		   bEnableAltWeightMode = VertexInfluences.Usage == IWU_PartialSwap;
		}
		
		MenuBar->Enable(IDM_ANIMSET_VIEWALTBONEWEIGHTINGMODE, bEnableAltWeightMode ? true : false);
		ToolBar->EnableTool(IDM_ANIMSET_VIEWALTBONEWEIGHTINGMODE, bEnableAltWeightMode ? true : false);		
		MenuBar->EnableAltBoneWeightingMenu(PreviewSkelComp->bDrawBoneInfluences ? TRUE:FALSE, PreviewSkelComp);		
	}
}

// Enable Alternative Bone weighting mode
void WxAnimSetViewer::EnableAltBoneWeighting(UBOOL bEnable)
{
	check( PreviewSkelComp );

	// first make enable/disable bones
	PreviewSkelComp->bDrawBoneInfluences = bEnable;

	// if Toggle is on, turn that off first
	if ( bPreviewInstanceWeights )
	{
		ToggleMeshWeights(FALSE);
	}

	for (INT LODIdx = 0; LODIdx<PreviewSkelComp->LODInfo.Num(); LODIdx++)
	{
		// Enable/Disable Alternative Bone weighting
		PreviewSkelComp->EnableAltBoneWeighting(bEnable, LODIdx);

		// enable usage of instanced weights (being mindful of the ToggleMeshWeights flag)
		PreviewSkelComp->ToggleInstanceVertexWeights( PreviewSkelComp->bDrawBoneInfluences, LODIdx );
	}

	// update existing selection if applicable
	if (PreviewSkelComp->bDrawBoneInfluences)
	{
		InitBoneWeightInfluenceData(PreviewSkelComp);
		UpdateInfluenceWeights(PreviewSkelComp->BonesOfInterest);
	}

	PreviewSkelComp->BeginDeferredReattach();

	// update interface
	MenuBar->Check(IDM_ANIMSET_VIEWALTBONEWEIGHTINGMODE, PreviewSkelComp->bDrawBoneInfluences == TRUE);

	// enable alt bone weighting menu
	MenuBar->EnableAltBoneWeightingMenu(bEnable, PreviewSkelComp);
	// lock LOD menu
	UpdateForceLODMenu();
}

// when section is selected
void WxAnimSetViewer::OnSectionSelected(wxCommandEvent& In)
{
	SelectSection(In.GetId()- IDM_ANIMSET_VERTEX_SECTION_0);
}

void WxAnimSetViewer::SelectSection(INT MaterialID)
{
	// radio button doesn't work, so I'm forcing it to clear and select only one
	for (INT MenuID=IDM_ANIMSET_VERTEX_SECTION_0; MenuID<=IDM_ANIMSET_VERTEX_SECTION_4; ++MenuID)
	{
		MenuBar->SectionSubMenu->Check(MenuID, false);
	}

	// select the one that's valid and selected
	VertexInfo.SelectedMaterialIndex = MaterialID;
	MenuBar->SectionSubMenu->Check(IDM_ANIMSET_VERTEX_SECTION_0+MaterialID, true);

	// clear all variables
	VertexInfo.SelectedVertexIndex = 0;
	VertexInfo.SelectedVertexPosition = FVector(0.f);
	VertexInfo.BoneIndices.Empty();
	VertexInfo.BoneWeights.Empty();
}

// view vertex mode
void WxAnimSetViewer::OnViewVertexMode(wxCommandEvent& In)
{									  
	if ( In.GetId() == IDM_ANIMSET_VERTEX_SHOWTANGENT_AS_VECTOR )
	{
		VertexInfo.bShowTangent = In.IsChecked()? TRUE: FALSE;
	}
	else if ( In.GetId() == IDM_ANIMSET_VERTEX_SHOWNORMAL_AS_VECTOR )
	{
		VertexInfo.bShowNormal = In.IsChecked()? TRUE: FALSE;
	}
	else
	{
		VertexInfo.ColorOption = 0;
		if (In.GetId() == IDM_ANIMSET_VERTEX_SHOWTANGENT_AS_TEXTURE)
		{
			if (In.IsChecked())
			{
				VertexInfo.ColorOption = 1;

				// clear normal and mirror
				MenuBar->NormalSubMenu->Check(IDM_ANIMSET_VERTEX_SHOWNORMAL_AS_TEXTURE, false);
				MenuBar->ViewMenu->Check(IDM_ANIMSET_VERTEX_SHOWMIRROR, false);
			}
		}
		else if (In.GetId() == IDM_ANIMSET_VERTEX_SHOWNORMAL_AS_TEXTURE)
		{
			if (In.IsChecked())
			{
				VertexInfo.ColorOption = 2;
				// clear normal and mirror
				MenuBar->TangentSubMenu->Check(IDM_ANIMSET_VERTEX_SHOWTANGENT_AS_TEXTURE, false);
				MenuBar->ViewMenu->Check(IDM_ANIMSET_VERTEX_SHOWMIRROR, false);
			}
		}
		else if (In.GetId() == IDM_ANIMSET_VERTEX_SHOWMIRROR)
		{
			if (In.IsChecked())
			{
				VertexInfo.ColorOption = 3;
				// clear normal and mirror
				MenuBar->TangentSubMenu->Check(IDM_ANIMSET_VERTEX_SHOWTANGENT_AS_TEXTURE, false);
				MenuBar->NormalSubMenu->Check(IDM_ANIMSET_VERTEX_SHOWNORMAL_AS_TEXTURE, false);
			}
		}

		UpdateVertexMode();
	}
}

// update LOD menu based on PreviewSkelComp->ForcedLODModel
// and enable/disable
void WxAnimSetViewer::UpdateForceLODMenu()
{
	if ( PreviewSkelComp )
	{
		// update to forced LOD
		// so that at least users can tell it's locked to the LOD
		PreviewSkelCompRaw->ForcedLodModel = PreviewSkelComp->ForcedLodModel;

		INT MenuID = IDM_ANIMSET_LOD_AUTO+PreviewSkelComp->ForcedLodModel;

		// check it
		MenuBar->Check( MenuID, true );
		ToolBar->ToggleTool( MenuID, true );

		// update LODbuttons
		UpdateForceLODButtons();
	}
}

/** Handler for forcing the rendering of the preview model to use a particular LOD. */
void WxAnimSetViewer::OnForceLODLevel( wxCommandEvent& In)
{
	if(In.GetId() == IDM_ANIMSET_LOD_AUTO)
	{
		PreviewSkelComp->ForcedLodModel = 0;
		PreviewSkelCompRaw->ForcedLodModel = 0;
	}
	else if(In.GetId() == IDM_ANIMSET_LOD_BASE)
	{
		PreviewSkelComp->ForcedLodModel = 1;
		PreviewSkelCompRaw->ForcedLodModel = 1;
	}
	else if(In.GetId() == IDM_ANIMSET_LOD_1)
	{
		PreviewSkelComp->ForcedLodModel = 2;
		PreviewSkelCompRaw->ForcedLodModel = 2;
	}
	else if(In.GetId() == IDM_ANIMSET_LOD_2)
	{
		PreviewSkelComp->ForcedLodModel = 3;
		PreviewSkelCompRaw->ForcedLodModel = 3;
	}
	else if(In.GetId() == IDM_ANIMSET_LOD_3)
	{
		PreviewSkelComp->ForcedLodModel = 4;
		PreviewSkelCompRaw->ForcedLodModel = 4;
	}

	MenuBar->Check( In.GetId(), true );
	ToolBar->ToggleTool( In.GetId(), true );

	// whenever LOD has changed, update Alternate Bone weighting menu
	UpdateAltBoneWeightingMenu();

	// Refresh the toolbar chunks and sections controls
	UpdateChunkPreview();
	UpdateSectionPreview();

#if WITH_SIMPLYGON
	SimplificationWindow->UpdateControls();
#endif // #if WITH_SIMPLYGON
}

/** Handler for removing a particular LOD from the SkeletalMesh. */
void WxAnimSetViewer::OnRemoveLOD(wxCommandEvent &In)
{
	if( SelectedSkelMesh->LODModels.Num() == 1 )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("NoLODToRemove") );
		return;
	}

	// Now display combo to choose which LOD to remove.
	TArray<FString> LODStrings;
	LODStrings.AddZeroed( SelectedSkelMesh->LODModels.Num()-1 );
	for(INT i=0; i<SelectedSkelMesh->LODModels.Num()-1; i++)
	{
		LODStrings(i) = FString::Printf( TEXT("%d"), i+1 );
	}

	// pop up dialog
	WxDlgGenericComboEntry dlg;
	if( dlg.ShowModal( TEXT("ChooseLODLevel"), TEXT("LODLevel:"), LODStrings, 0, TRUE ) == wxID_OK )
	{
		check( SelectedSkelMesh->LODInfo.Num() == SelectedSkelMesh->LODModels.Num() );

		// If its a valid LOD, kill it.
		INT DesiredLOD = dlg.GetComboBox().GetSelection() + 1;
		if( DesiredLOD > 0 && DesiredLOD < SelectedSkelMesh->LODModels.Num() )
		{
			//We'll be modifying the skel mesh data so reattach
			FComponentReattachContext PSCContext(PreviewSkelComp);
			FComponentReattachContext PSCRawContext(PreviewSkelCompRaw);

			// Release rendering resources before deleting LOD
			SelectedSkelMesh->LODModels(DesiredLOD).ReleaseResources();

			// Block until this is done
			FlushRenderingCommands();

			SelectedSkelMesh->LODModels.Remove(DesiredLOD);
			SelectedSkelMesh->LODInfo.Remove(DesiredLOD);
			SelectedSkelMesh->InitResources();

			// Set the forced LOD to Auto.
			PreviewSkelComp->ForcedLodModel = 0;
			PreviewSkelCompRaw->ForcedLodModel = 0;
			MenuBar->Check( IDM_ANIMSET_LOD_AUTO, true );
			ToolBar->ToggleTool( IDM_ANIMSET_LOD_AUTO, true );

			UpdateForceLODButtons();

			// Mark things for saving.
			SelectedSkelMesh->MarkPackageDirty();
		}
	}
}

void WxAnimSetViewer::OnViewChunk( wxCommandEvent& In )
{
	INT SelectedChunk = In.GetInt() - 1;
	PreviewChunk(SelectedChunk);
}

void WxAnimSetViewer::OnViewSection( wxCommandEvent& In )
{
	INT SelectedSection = In.GetInt() - 1;
	PreviewSection(SelectedSection);
}

void WxAnimSetViewer::OnSpeed(wxCommandEvent& In)
{
	const INT Id = In.GetId();
	switch (Id)
	{
	case IDM_ANIMSET_SPEED_1:
		PlaybackSpeed = 0.01f;
		break;
	case IDM_ANIMSET_SPEED_10:
		PlaybackSpeed = 0.1f;
		break;
	case IDM_ANIMSET_SPEED_25:
		PlaybackSpeed = 0.25f;
		break;
	case IDM_ANIMSET_SPEED_50:
		PlaybackSpeed = 0.5f;
		break;
	case IDM_ANIMSET_SPEED_100:
	default:
		PlaybackSpeed = 1.0f;
		break;
	};
}

void WxAnimSetViewer::OnViewWireframe(wxCommandEvent& In)
{
	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check( ASVPreviewVC );

	// Cycle through viewmodes.
	WireframeCycleCounter++;
	if( WireframeCycleCounter > 2 )
	{
		WireframeCycleCounter = 0;
	}

	switch( WireframeCycleCounter )
	{
		case 0 : // show mesh lit
			ASVPreviewVC->ShowFlags &= ~SHOW_ViewMode_Mask;
			ASVPreviewVC->ShowFlags |= SHOW_ViewMode_Lit;
			PreviewSkelComp->bDrawMesh = TRUE;
			break;
		case 1 : // show mesh wireframe
			ASVPreviewVC->ShowFlags &= ~SHOW_ViewMode_Mask;
			ASVPreviewVC->ShowFlags |= SHOW_ViewMode_Wireframe;
			PreviewSkelComp->bDrawMesh = TRUE;
			break;
		case 2 : // hide mesh
			ASVPreviewVC->ShowFlags &= ~SHOW_ViewMode_Mask;
			ASVPreviewVC->ShowFlags |= SHOW_ViewMode_Lit;
			PreviewSkelComp->bDrawMesh = FALSE;
			break;
	}

	// Toggle socket visibility as well
	for(INT i=0; i<SocketPreviews.Num(); i++)
	{
		UPrimitiveComponent* PrimComp = SocketPreviews(i).PreviewComp;
		if( PrimComp )
		{
			PrimComp->SetHiddenEditor(!PreviewSkelComp->bDrawMesh);
		}
	}

	MenuBar->Check( IDM_ANIMSET_VIEWWIREFRAME, (WireframeCycleCounter != 0) );
	ToolBar->ToggleTool( IDM_ANIMSET_VIEWWIREFRAME, (WireframeCycleCounter != 0) );
}

void WxAnimSetViewer::OnViewAdditiveBase(wxCommandEvent& In)
{
	bShowAdditiveBase = In.IsChecked();
	MenuBar->Check( IDM_ANIMSET_VIEWADDITIVEBASE, In.IsChecked() );
	ToolBar->ToggleTool( IDM_ANIMSET_VIEWADDITIVEBASE, In.IsChecked() );
}

void WxAnimSetViewer::OnViewGrid( wxCommandEvent& In )
{
	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check( ASVPreviewVC );

	if( In.IsChecked() )
	{
		ASVPreviewVC->ShowFlags |= SHOW_Grid;
	}
	else
	{
		ASVPreviewVC->ShowFlags &= ~SHOW_Grid;
	}
}

void WxAnimSetViewer::OnViewSockets( wxCommandEvent& In )
{
	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check( ASVPreviewVC );

	ASVPreviewVC->bShowSockets = In.IsChecked();
}

void WxAnimSetViewer::OnLoopAnim(wxCommandEvent& In)
{
	PreviewAnimNode->bLooping = !(PreviewAnimNode->bLooping);
	PreviewAnimNodeRaw->bLooping = !(PreviewAnimNodeRaw->bLooping);

	RefreshPlaybackUI();
}

void WxAnimSetViewer::OnPlayAnim(wxCommandEvent& In)
{
	if(!PreviewAnimNode->bPlaying)
	{
		PreviewAnimNode->PlayAnim(PreviewAnimNode->bLooping, 1.f);
		PreviewAnimNodeRaw->PlayAnim(PreviewAnimNodeRaw->bLooping, 1.f);
	}
	else
	{
		PreviewAnimNode->StopAnim();
		PreviewAnimNodeRaw->StopAnim();
	}

	RefreshPlaybackUI();
}

void WxAnimSetViewer::OnEmptySet(wxCommandEvent& In)
{
	EmptySelectedSet();
}

void WxAnimSetViewer::OnShowUVSet( wxCommandEvent& In )
{
	const INT NewUVSet = In.GetId() - IDM_ANIMSET_SHOW_UV_START;
	NewUVSet != UVSetToDisplay ? UVSetToDisplay = NewUVSet : UVSetToDisplay = -1;
}

void WxAnimSetViewer::OnShowUVSet_UpdateUI( wxUpdateUIEvent& In )
{
	if( UVSetToDisplay == In.GetId() - IDM_ANIMSET_SHOW_UV_START )
	{
		In.Check( TRUE );
	}
	else
	{
		In.Check( FALSE );
	}
}

void WxAnimSetViewer::OnDeleteTrack(wxCommandEvent& In)
{
	DeleteTrackFromSelectedSet();
}

void WxAnimSetViewer::OnDeleteMorphTrack(wxCommandEvent& In)
{
	DeleteMorphTrackFromSelectedSet();
}

void WxAnimSetViewer::OnCopyTranslationBoneNames(wxCommandEvent& In)
{
	CopyTranslationBoneNamesToAnimSet();
}

void WxAnimSetViewer::OnAnalyzeAnimSet(wxCommandEvent& In)
{
	AnalyzeAnimSet();
}

void WxAnimSetViewer::OnRenameSequence(wxCommandEvent& In)
{
	RenameSelectedSeq();
}

void WxAnimSetViewer::OnRemovePrefixFromSequences(wxCommandEvent& In)
{
	RemovePrefixFromSequences();
}

void WxAnimSetViewer::OnDeleteSequence(wxCommandEvent& In)
{
	DeleteSelectedSequence();
}

void WxAnimSetViewer::OnCopySequence(wxCommandEvent& In)
{
	CopySelectedSequence();
}

void WxAnimSetViewer::OnMoveSequence(wxCommandEvent& In)
{
	MoveSelectedSequence();
}

void WxAnimSetViewer::OnMakeSequencesAdditive(wxCommandEvent& In)
{
	MakeSelectedSequencesAdditive();
}

void WxAnimSetViewer::OnRebuildAdditiveAnimation(wxCommandEvent& In)
{
	RebuildAdditiveAnimation();
}

void WxAnimSetViewer::OnAddAdditiveAnimationToSelectedSequence(wxCommandEvent& In)
{
	AddAdditiveAnimationToSelectedSequence( (In.GetId() == IDM_ANIMSET_SUBTRACTADDITIVETOSEQ) );
}

void WxAnimSetViewer::OnSequenceApplyRotation(wxCommandEvent& In)
{
	SequenceApplyRotation();
}

void WxAnimSetViewer::OnSequenceReZero(wxCommandEvent& In)
{
	SequenceReZeroToCurrent();
}

void WxAnimSetViewer::OnSequenceCrop(wxCommandEvent& In)
{
	UBOOL bCropFromStart = FALSE;
	if(In.GetId() == IDM_ANIMSET_SEQDELBEFORE)
	{
		bCropFromStart = TRUE;
	}

	SequenceCrop(bCropFromStart);
}

/** Copies anim notifies from the selected sequence to a user-named destination sequence. */
void WxAnimSetViewer::OnNotifyCopy( wxCommandEvent& In )
{
	if( SelectedAnimSeq )
	{
		check( SelectedAnimSet );

		// If the source sequence doesn't contain any notifies, abort.
		UAnimSequence* SrcSeq = SelectedAnimSeq;
		if ( SrcSeq->Notifies.Num() == 0 )
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("SelectedSeqHasNoNotifies") );
			return;
		}

		// Prompt the user for the name of the destination sequence.
		WxDlgGenericStringEntry dlg;
		const INT Result = dlg.ShowModal( TEXT("CopyNotifies"), TEXT("DestAnimSequenceName"), *SelectedAnimSeq->SequenceName.ToString() );
		if( Result == wxID_OK )
		{
			// Find the named sequence.
			UAnimSequence* DestSeq = NULL;
			{
				FString EnteredString = dlg.GetEnteredString();

				// parse off part before the dot and see if it's an AnimSet name
				FString AnimSetName = TEXT("");
				FString AnimName = TEXT("");
				
				if (EnteredString.Split(TEXT("."), &AnimSetName, &AnimName) == 0)
				{
					// split failed, probably because there's no dot, which is ok.  AnimName should be the whole string.
					DestSeq = SelectedAnimSet->FindAnimSequence( *EnteredString );
				}
				else
				{
					// try to find the user-specified AnimSet
					UAnimSet* DestAnimSet = FindObject<UAnimSet>(ANY_PACKAGE, *AnimSetName);
					if (DestAnimSet)
					{
						DestSeq = DestAnimSet->FindAnimSequence(*AnimName);
					}
					else
					{
						// error, couldn't find specified AnimSet
						appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("AnimSetNotFound"), *AnimSetName) );
						return;
					}
				}
			}

			// If the named dest sequence couldn't be found, abort.
			if ( !DestSeq )
			{
				appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("DestSeqNotFoundF"), *dlg.GetEnteredString()) );
				return;
			}

			// If the source and dest sequences are the same, abort.
			if (SrcSeq == DestSeq)
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("DestAndSrcSeqsMatch") );
				return;
			}

			// Copy notifies
			UAnimSequence::CopyNotifies(SrcSeq, DestSeq);

			// Make sure UI shows the destination anim info.
			SetSelectedAnimSequence( DestSeq );

			// Select the sequence properties page.
			PropNotebook->SetSelection(2);

			// Make sure its up to date.
			AnimSeqProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );
			AnimSeqProps->SetObject( DestSeq, EPropertyWindowFlags::ShouldShowCategories);

// 			// Select the new entires in the property window.
// 			for ( INT NewNotify = 0 ; NewNotify < NewNotifyIndices.Num() ; ++NewNotify )
// 			{
// 				AnimSeqProps->ExpandItem( TEXT("Notifies"), NewNotifyIndices(NewNotify) );
// 			}
		}
	}
}

void WxAnimSetViewer::OnNotifySort(wxCommandEvent& In)
{
	AnimSeqProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );

	if(SelectedAnimSeq)
	{
		SelectedAnimSeq->SortNotifies();
		SelectedAnimSeq->MarkPackageDirty();
		AnimSeqProps->SetObject( SelectedAnimSeq, EPropertyWindowFlags::ShouldShowCategories );
	}
}
void WxAnimSetViewer::OnNotifiesRemove( wxCommandEvent& In )
{
	// Build list of animations to delete
	TArray<class UAnimSequence*> Sequences;

	for (UINT i=0; i<AnimSeqList->GetCount(); i++)
	{
		UAnimSequence* AnimSeq = (UAnimSequence*)AnimSeqList->GetClientData(i);
		if( AnimSeq )
		{
			check( AnimSeq->GetOuter() == SelectedAnimSet );
			check( SelectedAnimSet->Sequences.ContainsItem(AnimSeq) );

			// Checks out, so add item...
			Sequences.AddItem(AnimSeq);
		}
	}

	// If no sequences found, just abort
	if( Sequences.Num() == 0 )
	{
		return;
	}

	AnimSeqProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );
	// Now go through all sequences and delete!
	INT LastIndex = INDEX_NONE;
	for(INT i=0; i<Sequences.Num(); i++)
	{
		UAnimSequence* AnimSeq = Sequences(i);
		if( AnimSeq && AnimSeq->Notifies.Num())
		{
			AnimSeq->Notifies.Empty();
		}
	}
	AnimSeqProps->SetObject( SelectedAnimSeq, EPropertyWindowFlags::ShouldShowCategories );
	SelectedAnimSeq->MarkPackageDirty();	
}
/** Move all anim notifies by a value */
void WxAnimSetViewer::OnNotifyShift( wxCommandEvent& In )
{
	if( SelectedAnimSeq )
	{
		check( SelectedAnimSet );

		// If the source sequence doesn't contain any notifies, abort.
		UAnimSequence* SrcSeq = SelectedAnimSeq;
		if ( SrcSeq->Notifies.Num() == 0 )
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("SelectedSeqHasNoNotifies") );
			return;
		}

		// Prompt the user for the name of the destination sequence.
		WxDlgGenericStringEntry dlg;
		FString StartStr("0.0");
		const INT Result = dlg.ShowModal( TEXT("ShiftNotifies"), TEXT("TimeShift"), *StartStr );
		if( Result == wxID_OK )
		{
			FString EnteredString = dlg.GetEnteredString();
			FLOAT fShift = appAtof(*EnteredString);

			if(Abs(fShift) <= SelectedAnimSeq->SequenceLength)
			{
				//shift all by given amount
				for(INT i = 0; i < SelectedAnimSeq->Notifies.Num(); ++i)
				{
					SelectedAnimSeq->Notifies(i).Time += fShift;
					//clamp if needed
					if(SelectedAnimSeq->Notifies(i).Time < 0.0f)
						SelectedAnimSeq->Notifies(i).Time = 0.0f;
					if(SelectedAnimSeq->Notifies(i).Time > SelectedAnimSeq->SequenceLength)
						SelectedAnimSeq->Notifies(i).Time = SelectedAnimSeq->SequenceLength;
				}

				SelectedAnimSeq->MarkPackageDirty();

				// Make sure its up to date
				AnimSeqProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );
				AnimSeqProps->SetObject( SelectedAnimSeq, EPropertyWindowFlags::ShouldShowCategories );
			}
			else
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("SelectedSeqHasNoNotifies") );
				return;
			}
		}
	}
}

void WxAnimSetViewer::OnNewNotify( wxCommandEvent& In )
{
	if( SelectedAnimSeq )
	{
		// Create notify if one specified in ini
		UClass* NotifyClass = NULL;
		UObject* NotifyArchetype = NULL;
		const TCHAR* IniPath = NULL;
		FLOAT Duration = 0.0f;
		FLOAT ForcedStart = 0.0f;
		if(In.GetId() == IDM_ANIMSET_NOTIFYCUSTOM1)
		{
			IniPath = TEXT("editor-ini:AnimSetViewer.AnimNotifyCustom1");
			GConfig->GetFloat( TEXT("AnimSetViewer"), TEXT("AnimNotifyCustom1Duration"), Duration, GEditorIni );
			GConfig->GetFloat( TEXT("AnimSetViewer"), TEXT("AnimNotifyCustom1FixedStart"), ForcedStart, GEditorIni );
		}
		else if(In.GetId() == IDM_ANIMSET_NOTIFYCUSTOM2)
		{
			IniPath = TEXT("editor-ini:AnimSetViewer.AnimNotifyCustom2");
			GConfig->GetFloat( TEXT("AnimSetViewer"), TEXT("AnimNotifyCustom2Duration"), Duration, GEditorIni );
			GConfig->GetFloat( TEXT("AnimSetViewer"), TEXT("AnimNotifyCustom2FixedStart"), ForcedStart, GEditorIni );
		}
		//default case (first one, let's use it anyway)
		else
		{
			IniPath = TEXT("editor-ini:AnimSetViewer.AnimNotifyCustom0");
			GConfig->GetFloat( TEXT("AnimSetViewer"), TEXT("AnimNotifyCustom0Duration"), Duration, GEditorIni );
			GConfig->GetFloat( TEXT("AnimSetViewer"), TEXT("AnimNotifyCustom0FixedStart"), ForcedStart, GEditorIni );
		}

		const FLOAT NewTime = ::Clamp(ForcedStart > 0.0f ? ForcedStart : PreviewAnimNode->CurrentTime, 0.f, SelectedAnimSeq->SequenceLength);
		INT NewNotifyIndex = 0;
		while( NewNotifyIndex < SelectedAnimSeq->Notifies.Num() && 
			SelectedAnimSeq->Notifies(NewNotifyIndex).Time <= NewTime )
		{
			NewNotifyIndex++;
		}

		SelectedAnimSeq->Notifies.InsertZeroed(NewNotifyIndex);
		SelectedAnimSeq->Notifies(NewNotifyIndex).Time = NewTime;

		//if the duration is negative that means move the notify back and put the end sequence at NewTime
		if(Duration < 0.0f)
		{
			SelectedAnimSeq->Notifies(NewNotifyIndex).Time = Max(NewTime + Duration, 0.0f);
			SelectedAnimSeq->Notifies(NewNotifyIndex).Duration = -Duration;
		}
		else
			SelectedAnimSeq->Notifies(NewNotifyIndex).Duration = Duration;

		//if we have a path, try to load the notify
		if(IniPath)
		{
			NotifyClass = UObject::StaticLoadClass( UAnimNotify::StaticClass(), NULL, IniPath, NULL, LOAD_None, NULL );
			if(!NotifyClass)
			{
				//try loading as an archetype
				NotifyArchetype = UObject::StaticLoadObject( UAnimNotify::StaticClass(), NULL, IniPath, NULL, LOAD_None, NULL );
			}
			if( NotifyClass && NotifyClass->IsChildOf(UAnimNotify::StaticClass()) )
			{
				//load as a class
				SelectedAnimSeq->Notifies(NewNotifyIndex).Notify = ConstructObject<UAnimNotify>( NotifyClass, SelectedAnimSeq );
			}
			else if( NotifyArchetype )
			{
				if( NotifyArchetype->IsTemplate() && NotifyArchetype->GetClass() && NotifyArchetype->GetClass()->IsChildOf(UAnimNotify::StaticClass()) )
				{
					//handle archetype
					SelectedAnimSeq->Notifies(NewNotifyIndex).Notify = ConstructObject<UAnimNotify>( NotifyArchetype->GetClass(), SelectedAnimSeq, NAME_None, 0, NotifyArchetype);
				}
			}
		}

		AddedNotify( NewNotifyIndex );
	}
}

void WxAnimSetViewer::AddedNotify( const INT iIndex )
{
	if ( SelectedAnimSeq )
	{
		// Mark the package as dirty
		SelectedAnimSeq->MarkPackageDirty();

		// Select the sequence properties page.
		PropNotebook->SetSelection(2);

		// Make sure its up to date
		AnimSeqProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );
		AnimSeqProps->SetObject( SelectedAnimSeq, EPropertyWindowFlags::ShouldShowCategories );

		// Select the new entry
		AnimSeqProps->ExpandItem( FString(TEXT("Notifies")), iIndex );

		//expand the new notify
		if(SelectedAnimSeq->Notifies(iIndex).Notify)
		{
			AnimSeqProps->ExpandItem( FString(TEXT("Notify")), iIndex );
		}
	}
}

void WxAnimSetViewer::OnAllParticleNotifies( wxCommandEvent& In )
{
	UBOOL bEnable = (In.GetId() == IDM_ANIMSET_NOTIFY_ENABLEALLPSYS);
	for (UINT AnimSeqIndex = 0; AnimSeqIndex < AnimSeqList->GetCount(); AnimSeqIndex++)
	{
		UAnimSequence* AnimSeq = Cast<UAnimSequence>((UAnimSequence*)(AnimSeqList->GetClientData(AnimSeqIndex)));
		if (AnimSeq)
		{
			for (INT NotifyIndex = 0; NotifyIndex < AnimSeq->Notifies.Num(); NotifyIndex++)
			{
				FAnimNotifyEvent& Notify = AnimSeq->Notifies(NotifyIndex);
				UAnimNotify_PlayParticleEffect* NotifyPPE = Cast<UAnimNotify_PlayParticleEffect>(Notify.Notify);
				if (NotifyPPE)
				{
					NotifyPPE->bPreview = bEnable;
				}

				UAnimNotify_Trails* NotifyTrails = Cast<UAnimNotify_Trails>(Notify.Notify);
				if (NotifyTrails)
				{
					NotifyTrails->bPreview = bEnable;
				}
			}
		}
	}
}

void WxAnimSetViewer::OnRefreshAllNotifierData( wxCommandEvent& In )
{
	UEditorEngine::UpdateAnimSetNotifiers(SelectedAnimSet, PreviewAnimNode);
}

void WxAnimSetViewer::OnCopySequenceName( wxCommandEvent& In )
{
	if(SelectedAnimSeq)
	{
		appClipboardCopy(*SelectedAnimSeq->SequenceName.ToString());
	}
}


void WxAnimSetViewer::OnCopySequenceNameList( wxCommandEvent& In )
{
	FString AnimSeqNameListStr;

	if ( SelectedAnimSet == NULL )
	{
		return;
	}

	for(INT i=0; i<SelectedAnimSet->Sequences.Num(); i++)
	{
		UAnimSequence* Seq = SelectedAnimSet->Sequences(i);

		AnimSeqNameListStr += FString::Printf( TEXT("%s\r\n"), *Seq->SequenceName.ToString());
	}

	appClipboardCopy( *AnimSeqNameListStr );
}


void WxAnimSetViewer::OnCopyMeshBoneNames( wxCommandEvent& In )
{
	FString BoneNames;

	if(SelectedSkelMesh)
	{
		for(INT i=0; i<SelectedSkelMesh->RefSkeleton.Num(); i++)
		{
			FMeshBone& MeshBone = SelectedSkelMesh->RefSkeleton(i);

			INT Depth = 0;
			INT TmpBoneIndex = i;
			while( TmpBoneIndex != 0 )
			{
				TmpBoneIndex = SelectedSkelMesh->RefSkeleton(TmpBoneIndex).ParentIndex;
				Depth++;
			}

			FString LeadingSpace;
			for(INT j=0; j<Depth; j++)
			{
				LeadingSpace += TEXT(" ");
			}
	
			if( i==0 )
			{
				BoneNames += FString::Printf( TEXT("%3d: %s%s\r\n"), i, *LeadingSpace, *MeshBone.Name.ToString());
			}
			else
			{
				BoneNames += FString::Printf( TEXT("%3d: %s%s (ParentBoneID: %d)\r\n"), i, *LeadingSpace, *MeshBone.Name.ToString(), MeshBone.ParentIndex );
			}
		}
	}

	appClipboardCopy( *BoneNames );
}

/**
 * Called when the user selects the "Copy Weighted Bone Names to Clipboard" menu option
 *
 * @param	In	Event automatically generated by wxWidgets whenever the user selects the menu option
 */
void WxAnimSetViewer::OnCopyWeightedMeshBoneNames( wxCommandEvent& In )
{
	FString BoneNames;

	if ( SelectedSkelMesh && PreviewSkelComp )
	{
		// Determine the correct LOD model to use
		const INT LODIndex = ::Clamp( PreviewSkelComp->PredictedLODLevel, 0, SelectedSkelMesh->LODModels.Num() - 1 );
		const FStaticLODModel& CurLODModel = SelectedSkelMesh->LODModels( LODIndex );

		// Iterate over each mesh chunk in the current LOD model, tracking the indices of all of the bones in each
		// chunk's bone map
		TSet<WORD> WeightedBoneIndices;
		for ( INT ChunkIndex = 0; ChunkIndex < CurLODModel.Chunks.Num(); ++ChunkIndex )
		{
			const FSkelMeshChunk& CurChunk = CurLODModel.Chunks(ChunkIndex);
			for ( INT BoneMapIndex = 0; BoneMapIndex < CurChunk.BoneMap.Num(); ++BoneMapIndex )
			{
				WeightedBoneIndices.Add( CurChunk.BoneMap(BoneMapIndex) );
			}
		}

		// Iterate over each bone in the ref. skeleton, seeing if the index appeared in one of the LOD model's mesh chunks
		for( INT BoneIndex = 0; BoneIndex < SelectedSkelMesh->RefSkeleton.Num(); ++BoneIndex )
		{
			if ( WeightedBoneIndices.Contains( BoneIndex ) )
			{
				const FMeshBone& CurBone = SelectedSkelMesh->RefSkeleton(BoneIndex);
				check( SelectedSkelMesh->RefSkeleton.IsValidIndex( CurBone.ParentIndex ) );

				const FMeshBone& CurParentBone = SelectedSkelMesh->RefSkeleton(CurBone.ParentIndex);

				// Format the bone information to display the bone index and name, as well as the index and name of its parent bone, if applicable
				BoneNames += FString::Printf( TEXT("%3d: %s"), BoneIndex,  *( CurBone.Name.ToString() ) );
				if ( BoneIndex != 0 )
				{
					BoneNames += FString::Printf( TEXT(" (ParentBoneID: %d, ParentBoneName: %s)"), CurBone.ParentIndex, *( CurParentBone.Name.ToString() ) );
				}
				BoneNames += TEXT("\r\n");
			}
		}
	}

	// Copy any of the relevant bone information to the clipboard
	appClipboardCopy( *BoneNames );
}

/**
 * Fixes up names by removing tabs and replacing spaces with underscores.
 *
 * @param	InOldName	The name to fixup.
 * @param	OutResult	[out] The fixed up name.
 * @return				TRUE if the name required fixup, FALSE otherwise.
 */
static UBOOL FixupName(const TCHAR* InOldName, FString& OutResult)
{
	// Replace spaces with underscores.
	OutResult = FString( InOldName ).Replace( TEXT(" "), TEXT("_") );

	// Remove tabs by recursively splitting the string at a tab and merging halves.
	FString LeftStr;
	FString RightStr;

	while ( TRUE )
	{
		const UBOOL bSplit = OutResult.Split( TEXT("\t"), &LeftStr, &RightStr );
		if ( !bSplit )
		{
			break;
		}
		OutResult = LeftStr + RightStr;
	}

	const UBOOL bNewNameDiffersFromOld = ( OutResult != InOldName );
	return bNewNameDiffersFromOld;
}

void WxAnimSetViewer::OnFixupMeshBoneNames( wxCommandEvent& In )
{
	if( !SelectedSkelMesh )
	{
		return;
	}

	// Fix up bone names for the reference skeleton.
	TArray<FMeshBone>& RefSkeleton = SelectedSkelMesh->RefSkeleton;

	TArray<FName> OldNames;
	TArray<FName> NewNames;

	for( INT BoneIndex = 0 ; BoneIndex < RefSkeleton.Num() ; ++BoneIndex )
	{
		FMeshBone& MeshBone = RefSkeleton( BoneIndex );
	
		FString FixedName;
		const UBOOL bNeededFixup = FixupName( *MeshBone.Name.ToString(), FixedName );
		if ( bNeededFixup )
		{
			// Point the skeleton to the new bone name.
			const FName NewName( *FixedName );
			MeshBone.Name = NewName;

			// Store off the old and new bone names for animset fixup.
			NewNames.AddItem( NewName );
			OldNames.AddItem( MeshBone.Name );

			debugf(TEXT("Fixed \"%s\" --> \"%s\""), *MeshBone.Name.ToString(), *NewName.ToString() );
		}
	}

	check( OldNames.Num() == NewNames.Num() );

	// Go back over the list of names that were changed and update the selected animset.
	if( SelectedAnimSet && OldNames.Num() > 0 )
	{
		// Flag for tracking whether or not the anim set referred to bones that were fixed up.
		UBOOL bFixedUpAnimSet = FALSE;

		for ( INT TrackIndex = 0 ; TrackIndex < SelectedAnimSet->TrackBoneNames.Num() ; ++TrackIndex )
		{
			const FName& TrackName = SelectedAnimSet->TrackBoneNames( TrackIndex );

			// See if this track was fixed up.
			for ( INT NameIndex = 0 ; NameIndex < OldNames.Num() ; ++NameIndex )
			{
				const FName& OldName = OldNames( NameIndex );

				if ( OldName == TrackName )
				{
					// Point the track to the new bone name.
					const FName& NewName = NewNames( NameIndex );
					SelectedAnimSet->TrackBoneNames( TrackIndex ) = NewName;

					bFixedUpAnimSet = TRUE;

					break;
				}
			}
		}

		// Mark things for saving.
		SelectedSkelMesh->MarkPackageDirty();

		// Mark the anim set dirty if it refered to bones that were fixed up.
		if ( bFixedUpAnimSet )
		{
			SelectedAnimSet->MarkPackageDirty();
		}
	}
}

void WxAnimSetViewer::OnSwapLODSections(wxCommandEvent& In)
{
	if(!SelectedSkelMesh || PreviewSkelComp->ForcedLodModel == 0)
	{
		return;
	}

	{
		FComponentReattachContext ReattachContext(PreviewSkelComp);
		SelectedSkelMesh->ReleaseResources();
		SelectedSkelMesh->ReleaseResourcesFence.Wait();

		INT LODIndex = PreviewSkelComp->ForcedLodModel - 1;
		FStaticLODModel& LODModel = SelectedSkelMesh->LODModels(LODIndex);
		if(LODModel.Sections.Num() == 2)
		{
			FSkelMeshSection TempSection = LODModel.Sections(0);
			LODModel.Sections(0) = LODModel.Sections(1);
			LODModel.Sections(0).MaterialIndex = 0;
			LODModel.Sections(1) = TempSection;
			LODModel.Sections(1).MaterialIndex = 1;
		}

		SelectedSkelMesh->InitResources();
	}
}

/** Utility for shrinking the Materials array in the SkeletalMesh, removed any duplicates. */
void WxAnimSetViewer::OnMergeMaterials( wxCommandEvent& In )
{
	if(!SelectedSkelMesh)
	{
		return;
	}

	TArray<UMaterialInterface*>	NewMaterialArray;

	TArray<INT> OldToNewMaterialMap;
	OldToNewMaterialMap.Add( SelectedSkelMesh->Materials.Num() );

	for(INT i=0; i<SelectedSkelMesh->Materials.Num(); i++)
	{
		UMaterialInterface* Inst = SelectedSkelMesh->Materials(i);

		INT NewIndex = NewMaterialArray.FindItemIndex(Inst);
		if(NewIndex == INDEX_NONE)
		{
			NewIndex = NewMaterialArray.AddItem(Inst);
			OldToNewMaterialMap(i) = NewIndex;
		}
		else
		{
			OldToNewMaterialMap(i) = NewIndex;
		}
	}

	// Assign new materials array to SkeletalMesh
	SelectedSkelMesh->Materials = NewMaterialArray;

	// For the base mesh - remap the sections.
	for(INT i=0; i<SelectedSkelMesh->LODModels(0).Sections.Num(); i++)
	{
		FSkelMeshSection& Section = SelectedSkelMesh->LODModels(0).Sections(i);
		Section.MaterialIndex = OldToNewMaterialMap( Section.MaterialIndex );
	}

	// Now remap each LOD's LODMaterialMap to take this into account.
	for(INT i=0; i<SelectedSkelMesh->LODInfo.Num(); i++)
	{
		FSkeletalMeshLODInfo& LODInfo = SelectedSkelMesh->LODInfo(i);
		for(INT j=0; j<LODInfo.LODMaterialMap.Num(); j++)
		{
			LODInfo.LODMaterialMap(j) = OldToNewMaterialMap( LODInfo.LODMaterialMap(j) );
		}
	}

	// Display success box.
	appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("MergeMaterialSuccess"), OldToNewMaterialMap.Num(), SelectedSkelMesh->Materials.Num()) );
}


static FLOAT DistFromPlane(FVector& Point, BYTE Axis)
{
	if(Axis == AXIS_X)
	{
		return Point.X;
	}
	else if(Axis == AXIS_Y)
	{
		return Point.Y;
	}
	else if(Axis == AXIS_Z)
	{
		return Point.Z;
	}
	else
	{
		return 0.f;
	}
}

static void MirrorPointByPlane(FVector& Point, BYTE Axis)
{
	if(Axis == AXIS_X)
	{
		Point.X *= -1.f;
	}
	else if(Axis == AXIS_Y)
	{
		Point.Y *= -1.f;
	}
	else if(Axis == AXIS_Z)
	{
		Point.Z *= -1.f;
	}
}

static const FLOAT NoSwapThreshold = 0.2f;
static const FLOAT MatchBoneThreshold = 0.1f;


/** Utility tool for constructing the mirror table automatically. */
void WxAnimSetViewer::OnAutoMirrorTable( wxCommandEvent& In )
{
	if( SelectedSkelMesh->SkelMirrorAxis != AXIS_X &&
		SelectedSkelMesh->SkelMirrorAxis != AXIS_Y &&
		SelectedSkelMesh->SkelMirrorAxis != AXIS_Z )
	{
		return;
	}

	TArray<BYTE> NewMirrorTable;
	NewMirrorTable.Add(SelectedSkelMesh->RefSkeleton.Num());

	// First we build all the mesh-space reference pose transforms.
	TArray<FMatrix> BoneTM;
	BoneTM.Add(SelectedSkelMesh->RefSkeleton.Num());

	for(INT i=0; i<SelectedSkelMesh->RefSkeleton.Num(); i++)
	{
		BoneTM(i) = FQuatRotationTranslationMatrix( SelectedSkelMesh->RefSkeleton(i).BonePos.Orientation, SelectedSkelMesh->RefSkeleton(i).BonePos.Position );

		if(i>0)
		{
			INT ParentIndex = SelectedSkelMesh->RefSkeleton(i).ParentIndex;
			BoneTM(i) = BoneTM(i) * BoneTM(ParentIndex);
		}

		// Init table to 255, so we can see where we are missing bones later on.
		NewMirrorTable(i) = 255;
	}

	// Then we go looking for pairs of bones.
	for(INT i=0; i<SelectedSkelMesh->RefSkeleton.Num(); i++)
	{
		// First, find distance of point from mirror plane.
		FVector BonePos = BoneTM(i).GetOrigin();
		FLOAT DistFromMirrorPlane = DistFromPlane(BonePos, SelectedSkelMesh->SkelMirrorAxis);

		// If its suitable small (eg. for spine) we don't look for its mirror twin.
		if(Abs(DistFromMirrorPlane) > NoSwapThreshold)
		{
			// Only search for bones on positive side of plane.
			if(DistFromMirrorPlane > 0.f)
			{
				INT ClosestBoneIndex = INDEX_NONE;
				FLOAT ClosestDist = 10000000000.f;

				// Mirror point, to compare distances against.
				MirrorPointByPlane(BonePos, SelectedSkelMesh->SkelMirrorAxis);

				for(INT j=0; j<SelectedSkelMesh->RefSkeleton.Num(); j++)
				{
					FVector TestPos = BoneTM(j).GetOrigin();
					FLOAT TestDistFromPlane = DistFromPlane(TestPos, SelectedSkelMesh->SkelMirrorAxis);

					// If point is on negative side of plane, test against it.
					if(TestDistFromPlane < 0.f)
					{
						// If this is closer than best match so far, remember it.
						FLOAT DistToBone = (BonePos - TestPos).Size();
						if(DistToBone < ClosestDist)
						{
							ClosestBoneIndex = j;
							ClosestDist = DistToBone;
						}
					}
				}

				// If below our match threshold..
				if(ClosestDist < MatchBoneThreshold)
				{
					UBOOL bDoAssignment = true;

					// If one of these slots is already being used, fail.
					if(NewMirrorTable(i) != 255)
					{
						//appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("AutoMirrorMultiMatch"), *(SelectedSkelMesh->RefSkeleton(i).Name)) );
						NewMirrorTable(i) = 255;
						bDoAssignment = false;
					}

					if( NewMirrorTable(ClosestBoneIndex) != 255)
					{
						//appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("AutoMirrorMultiMatch"), *(SelectedSkelMesh->RefSkeleton(ClosestBoneIndex).Name)) );
						NewMirrorTable(ClosestBoneIndex) = 255;
						bDoAssignment = false;
					}

					// If not, set these into the mirror table.
					if(bDoAssignment)
					{
						NewMirrorTable(i) = ClosestBoneIndex;
						NewMirrorTable(ClosestBoneIndex) = i;
					}
				}
				else
				{					
					//appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("AutoMirrorNoMatch"), *(SelectedSkelMesh->RefSkeleton(i).Name)) );
					debugf( TEXT("NoMatch: %d:%s (closest %d:%s %f)"), 
						i, *(SelectedSkelMesh->RefSkeleton(i).Name.ToString()), 
						ClosestBoneIndex, *(SelectedSkelMesh->RefSkeleton(ClosestBoneIndex).Name.ToString()), ClosestDist );
				}
			}
		}
	}

	// Look for bones that are not present.
	INT ProblemCount = 0;
	for(INT i=0; i<NewMirrorTable.Num(); i++)
	{
		if(NewMirrorTable(i) == 255)
		{
			ProblemCount++;
			NewMirrorTable(i) = i;
		}
	}

	// Clear old mapping table and assign new one.
	SelectedSkelMesh->SkelMirrorTable.Empty();
	SelectedSkelMesh->SkelMirrorTable.AddZeroed(NewMirrorTable.Num());
	for(INT i=0; i<NewMirrorTable.Num(); i++)
	{
		SelectedSkelMesh->SkelMirrorTable(i).SourceIndex = NewMirrorTable(i);
	}

	SelectedSkelMesh->MarkPackageDirty();

	appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("AutoMirrorProblemCount"), ProblemCount) );
}

/** Check the current mirror table is ok */
void WxAnimSetViewer::OnCheckMirrorTable( wxCommandEvent& In )
{
	FString ProblemBones;
	UBOOL bIsOK = SelectedSkelMesh->MirrorTableIsGood(ProblemBones);
	if(!bIsOK)
	{
		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("MirrorTableBad"), *ProblemBones) );
	}
	else
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("MirrorTableOK") );
	}
}

/**
 * Copy Mirror table to clipboard
 */
void WxAnimSetViewer::OnCopyMirrorTable( wxCommandEvent& In )
{
	FString	Table;
	UEnum*	AxisEnum = FindObject<UEnum>(UObject::StaticClass(), TEXT("EAxis"));

	if( SelectedSkelMesh )
	{
		for(INT i=0; i<SelectedSkelMesh->SkelMirrorTable.Num(); i++)
		{
			FMeshBone& MeshBone = SelectedSkelMesh->RefSkeleton(i);

			Table += FString::Printf( TEXT("%3d: %d %s \r\n"), i, SelectedSkelMesh->SkelMirrorTable(i).SourceIndex, *AxisEnum->GetEnum(SelectedSkelMesh->SkelMirrorTable(i).BoneFlipAxis).ToString() );
		}
	}

	appClipboardCopy( *Table );
}

/** Copy the mirror table from the selected SkeletalMesh in the browser to the current mesh in the AnimSet Viewer */
void WxAnimSetViewer::OnCopyMirroTableFromMesh( wxCommandEvent& In )
{
	// Get selected mesh, and check we have one!
	GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );
	USkeletalMesh* SrcMesh = GEditor->GetSelectedObjects()->GetTop<USkeletalMesh>();
	if(!SrcMesh)
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("NoSourceSkelMeshSelected") );
		return;
	}

	// Check source mesh has a mirror table
	if(SrcMesh->SkelMirrorTable.Num() == 0)
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("SrcMeshNoMirrorTable") );
		return;
	}

	// Let the user confirm they want to do this - will clobber existing table in this mesh.
	UBOOL bConfirm = appMsgf( AMT_YesNo, *LocalizeUnrealEd("SureCopyMirrorTable") );
	if(!bConfirm)
	{
		return;
	}

	// Do the copy...
	SelectedSkelMesh->CopyMirrorTableFrom(SrcMesh);

	SelectedSkelMesh->MarkPackageDirty();

	// Run a check, just to be sure, and let user fix up and existing problems.
	FString ProblemBones;
	UBOOL bIsOK = SelectedSkelMesh->MirrorTableIsGood(ProblemBones);
	if(!bIsOK)
	{
		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("MirrorTableBad"), *ProblemBones) );
	}
}

void WxAnimSetViewer::OnUpdateBounds( wxCommandEvent& In )
{
	UpdateMeshBounds();
}

void WxAnimSetViewer::OnToggleClothSim( wxCommandEvent& In )
{
	if(In.IsChecked())
	{
		PreviewSkelComp->InitClothSim(RBPhysScene);
		PreviewSkelComp->bEnableClothSimulation = TRUE;
	}
	else
	{
		PreviewSkelComp->TermClothSim(RBPhysScene);
		PreviewSkelComp->bEnableClothSimulation = FALSE;
	}
}
void WxAnimSetViewer::OnSoftBodyGenerate( wxCommandEvent& In )
{
	if(PreviewSkelComp->SkeletalMesh)
	{
		PreviewSkelComp->TermSoftBodySim(NULL);
		FlushRenderingCommands();
#if WITH_NOVODEX
		PreviewSkelComp->SkeletalMesh->ClearSoftBodyMeshCache();
		BuildSoftBodyMapping(*PreviewSkelComp->SkeletalMesh);	
#endif	//#if WITH_NOVODEX
		PreviewSkelComp->InitSoftBodySimBuffers();
		if(PreviewSkelComp->bEnableSoftBodySimulation)
		{
			PreviewSkelComp->InitSoftBodySim(RBPhysScene, TRUE);
		}
	}
}

void WxAnimSetViewer::OnSoftBodyToggleSim( wxCommandEvent& In )
{
	if(In.IsChecked())
	{
		PreviewSkelComp->InitSoftBodySim(RBPhysScene, TRUE);
		PreviewSkelComp->bEnableSoftBodySimulation = TRUE;
		// TODO: Is there a reason to simulate the "Raw" mesh as well?
	}
	else
	{
		PreviewSkelComp->TermSoftBodySim(NULL);
		PreviewSkelComp->bEnableSoftBodySimulation = FALSE;
		// Reset mesh to position based on original tetra-mesh.
		PreviewSkelComp->InitSoftBodySimBuffers();
	}
}

void WxAnimSetViewer::OnToggleTriangleSortMode( wxCommandEvent& In )
{
	GetPreviewVC()->bTriangleSortMode = In.IsChecked();

	WxAnimSetViewer* ASV = GetPreviewVC()->AnimSetViewer;
	if( In.IsChecked() )
	{
		// Check if we have a TRISORT_Custom section.
		UBOOL bFoundCustomLeftRight=FALSE;
		for( INT LodIndex=0;LodIndex<ASV->SelectedSkelMesh->LODModels.Num();LodIndex++ )
		{
			FStaticLODModel& LODModel = ASV->SelectedSkelMesh->LODModels(LodIndex);
			for( INT SectionIndex=0;SectionIndex < LODModel.Sections.Num();SectionIndex++ )
			{
				FSkelMeshSection& Section = LODModel.Sections(SectionIndex);
				if( Section.TriangleSorting == TRISORT_CustomLeftRight )
				{
					bFoundCustomLeftRight = TRUE;
					break;
				}
			}
			if( bFoundCustomLeftRight )
			{
				ToolBar->EnableTool(IDM_ANIMSET_TRIANGLESORTMODELR,TRUE);
				ASV->PreviewSkelComp->CustomSortAlternateIndexMode = ToolBar->GetToolState(IDM_ANIMSET_TRIANGLESORTMODELR) ? CSAIM_Right : CSAIM_Left;
				break;
			}
		}
	}
	else
	{
		ToolBar->EnableTool(IDM_ANIMSET_TRIANGLESORTMODELR,FALSE);
		ASV->PreviewSkelComp->CustomSortAlternateIndexMode = CSAIM_Auto;
	}

	// Invalidate hit proxies as we may have changed the set of indices to use.
	GetPreviewVC()->Viewport->InvalidateHitProxy();
	ASV->UpdateSkelComponents();
}

void WxAnimSetViewer::OnToggleTriangleSortModeLR( wxCommandEvent& In )
{
	ToolBar->SetToolNormalBitmap( IDM_ANIMSET_TRIANGLESORTMODELR, In.IsChecked() ? ToolBar->TriangleSortModeR : ToolBar->TriangleSortModeL );
	GetPreviewVC()->AnimSetViewer->PreviewSkelComp->CustomSortAlternateIndexMode = In.IsChecked() ? CSAIM_Right : CSAIM_Left;

	// Invalidate hit proxies as we may have changed the set of indices to use.
	GetPreviewVC()->Viewport->InvalidateHitProxy();
	GetPreviewVC()->AnimSetViewer->UpdateSkelComponents();
}


/*-----------------------------------------------------------------------------
	WxASVToolBar
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxASVToolBar, WxToolBar )
END_EVENT_TABLE()

WxASVToolBar::WxASVToolBar( wxWindow* InParent, wxWindowID InID )
: WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
{
	SocketMgrB.Load(TEXT("ASV_SocketMgr"));
	ShowBonesB.Load(TEXT("AnimTree_ShowBones"));
	ShowRawAnimationB.Load(TEXT("ASV_ShowRawAnimation"));
	ShowCompressedAnimationB.Load(TEXT("ASV_ShowCompressedAnimation"));
	ShowBoneNamesB.Load(TEXT("AnimTree_ShowBoneNames"));
	ShowWireframeB.Load(TEXT("AnimTree_ShowWireframe"));
	ShowRefPoseB.Load(TEXT("ASV_ShowRefPose"));
	ShowMirrorB.Load(TEXT("ASV_ShowMirror"));

	NewNotifyB.Load(TEXT("ASV_NewNotify"));
	ClothB.Load(TEXT("ASV_Cloth"));
	SoftBodyGenerateB.Load(TEXT("ASV_SoftBodyGenerate"));
	SoftBodyToggleSimB.Load(TEXT("ASV_SoftBodyToggleSim"));

	LODAutoB.Load(TEXT("AnimTree_LOD_Auto"));
	LODBaseB.Load(TEXT("AnimTree_LOD_Base"));
	LOD1B.Load(TEXT("AnimTree_LOD_1"));
	LOD2B.Load(TEXT("AnimTree_LOD_2"));
	LOD3B.Load(TEXT("AnimTree_LOD_3"));

	Speed1B.Load(TEXT("ASV_Speed_1"));
	Speed10B.Load(TEXT("ASV_Speed_10"));
	Speed25B.Load(TEXT("ASV_Speed_25"));
	Speed50B.Load(TEXT("ASV_Speed_50"));
	Speed100B.Load(TEXT("ASV_Speed_100"));

	GoreMode.Load(TEXT("ASV_ShowGoreMode"));

	TangentMode.Load(TEXT("ASV_ShowTangents"));
	NormalMode.Load(TEXT("ASV_ShowNormals"));

	TriangleSortMode.Load(TEXT("ASV_TriangleSortMode"));
	TriangleSortModeL.Load(TEXT("ASV_TriangleSortModeL"));
	TriangleSortModeR.Load(TEXT("ASV_TriangleSortModeR"));

	SetToolBitmapSize( wxSize( 16, 16 ) );

	wxStaticText* LODLabel = new wxStaticText(this, -1 ,*LocalizeUnrealEd("LODLabel") );
	wxStaticText* ChunksLabel = new wxStaticText(this, -1 ,*LocalizeUnrealEd("AnimSetViewer_ChunksLabel") );
	wxStaticText* SectionsLabel = new wxStaticText(this, -1 ,*LocalizeUnrealEd("AnimSetViewer_SectionsLabel") );

	AddSeparator();
	AddCheckTool(IDM_ANIMSET_VIEWBONES, *LocalizeUnrealEd("ToolTip_3"), ShowBonesB, wxNullBitmap, *LocalizeUnrealEd("ToolTip_3") );
	AddCheckTool(IDM_ANIMSET_VIEWADDITIVEBASE, *LocalizeUnrealEd("ShowAdditiveBase"), ShowBonesB, wxNullBitmap, *LocalizeUnrealEd("ShowAdditiveBase") );
	AddCheckTool(IDM_ANIMSET_VIEWBONENAMES, *LocalizeUnrealEd("ToolTip_4"), ShowBoneNamesB, wxNullBitmap, *LocalizeUnrealEd("ToolTip_4") );
	AddCheckTool(IDM_ANIMSET_VIEWWIREFRAME, *LocalizeUnrealEd("ToolTip_5"), ShowWireframeB, wxNullBitmap, *LocalizeUnrealEd("ToolTip_5") );
	AddCheckTool(IDM_ANIMSET_VIEWREFPOSE, *LocalizeUnrealEd("ShowReferencePose"), ShowRefPoseB, wxNullBitmap, *LocalizeUnrealEd("ShowReferencePose") );
	AddCheckTool(IDM_ANIMSET_VIEWMIRROR, *LocalizeUnrealEd("ShowMirror"), ShowMirrorB, wxNullBitmap, *LocalizeUnrealEd("ShowMirror") );
	AddSeparator();
	AddCheckTool(IDM_ANIMSET_OPENSOCKETMGR, *LocalizeUnrealEd("ToolTip_6"), SocketMgrB, wxNullBitmap, *LocalizeUnrealEd("ToolTip_6") );
	AddSeparator();
	
	//check to see if the user has any custom notifies added (class or archetype)
	UClass* pClass = UObject::StaticLoadClass( UAnimNotify::StaticClass(), NULL, TEXT("editor-ini:AnimSetViewer.AnimNotifyCustom0"), NULL, LOAD_None, NULL );
	if(pClass && pClass->IsChildOf(UAnimNotify::StaticClass()))
		AddTool(IDM_ANIMSET_NOTIFYNEW, NewNotifyB, *pClass->GetName() );
	else
	{
		UObject* pObj = UObject::StaticLoadObject( UAnimNotify::StaticClass(), NULL, TEXT("editor-ini:AnimSetViewer.AnimNotifyCustom0"), NULL, LOAD_None, NULL );
		if( pObj && pObj->IsTemplate() && pObj->GetClass() && pObj->GetClass()->IsChildOf(UAnimNotify::StaticClass()) )
			AddTool(IDM_ANIMSET_NOTIFYNEW, NewNotifyB, *pObj->GetName() );
		else	//default case just add the normal new notify button
			AddTool(IDM_ANIMSET_NOTIFYNEW, NewNotifyB, *LocalizeUnrealEd("NewNotify") );
	}
	AddSeparator();

	//allow up to two more extra custom buttons
	//TODO, allow this to support more than three, generalize with arrays
	pClass = UObject::StaticLoadClass( UAnimNotify::StaticClass(), NULL, TEXT("editor-ini:AnimSetViewer.AnimNotifyCustom1"), NULL, LOAD_None, NULL );
	if(pClass && pClass->IsChildOf(UAnimNotify::StaticClass()))
	{
		AddTool(IDM_ANIMSET_NOTIFYCUSTOM1, NewNotifyB, *pClass->GetName() );
		AddSeparator();
	}
	else
	{
		UObject* pObj = UObject::StaticLoadObject( UAnimNotify::StaticClass(), NULL, TEXT("editor-ini:AnimSetViewer.AnimNotifyCustom1"), NULL, LOAD_None, NULL );
		if( pObj && pObj->IsTemplate() && pObj->GetClass() && pObj->GetClass()->IsChildOf(UAnimNotify::StaticClass()) )
		{
			AddTool(IDM_ANIMSET_NOTIFYCUSTOM1, NewNotifyB, *pObj->GetName() );
			AddSeparator();
		}

		//by default don't add another if this isn't set
	}

	pClass = UObject::StaticLoadClass( UAnimNotify::StaticClass(), NULL, TEXT("editor-ini:AnimSetViewer.AnimNotifyCustom2"), NULL, LOAD_None, NULL );
	if(pClass && pClass->IsChildOf(UAnimNotify::StaticClass()))
	{
		AddTool(IDM_ANIMSET_NOTIFYCUSTOM2, NewNotifyB, *pClass->GetName() );
		AddSeparator();
	}
	else
	{
		UObject* pObj = UObject::StaticLoadObject( UAnimNotify::StaticClass(), NULL, TEXT("editor-ini:AnimSetViewer.AnimNotifyCustom2"), NULL, LOAD_None, NULL );
		if( pObj && pObj->IsTemplate() && pObj->GetClass() && pObj->GetClass()->IsChildOf(UAnimNotify::StaticClass()) )
		{
			AddTool(IDM_ANIMSET_NOTIFYCUSTOM2, NewNotifyB, *pObj->GetName() );
			AddSeparator();

		}

		//by default don't add another if this isn't set
	}	

	AddCheckTool(IDM_ANIMSET_TOGGLECLOTH, *LocalizeUnrealEd("ToggleCloth"), ClothB, wxNullBitmap, *LocalizeUnrealEd("ToggleCloth") );
	AddTool(IDM_ANIMSET_SOFTBODYGENERATE, *LocalizeUnrealEd("SoftBodyGenerate"), SoftBodyGenerateB, *LocalizeUnrealEd("SoftBodyGenerate") );
	AddCheckTool(IDM_ANIMSET_SOFTBODYTOGGLESIM, *LocalizeUnrealEd("SoftBodyToggleSim"), SoftBodyToggleSimB, wxNullBitmap, *LocalizeUnrealEd("SoftBodyToggleSim") );
	AddSeparator();
	AddCheckTool(IDM_ANIMSET_ShowRawAnimation, *LocalizeUnrealEd("ShowUncompressedAnimation"), ShowRawAnimationB, wxNullBitmap, *LocalizeUnrealEd("ShowUncompressedAnimation") );
	AddCheckTool(IDM_ANIMSET_OpenAnimationCompressionDlg, *LocalizeUnrealEd("AnimationCompressionSettings"), ShowCompressedAnimationB, wxNullBitmap, *LocalizeUnrealEd("AnimationCompressionSettings") );
	AddSeparator();
	AddControl(LODLabel);
	AddRadioTool(IDM_ANIMSET_LOD_AUTO, *LocalizeUnrealEd("SetLODAuto"), LODAutoB, wxNullBitmap, *LocalizeUnrealEd("SetLODAuto") );
	AddRadioTool(IDM_ANIMSET_LOD_BASE, *LocalizeUnrealEd("ForceLODBaseMesh"), LODBaseB, wxNullBitmap, *LocalizeUnrealEd("ForceLODBaseMesh") );
	AddRadioTool(IDM_ANIMSET_LOD_1, *LocalizeUnrealEd("ForceLOD1"), LOD1B, wxNullBitmap, *LocalizeUnrealEd("ForceLOD1") );
	AddRadioTool(IDM_ANIMSET_LOD_2, *LocalizeUnrealEd("ForceLOD2"), LOD2B, wxNullBitmap, *LocalizeUnrealEd("ForceLOD2") );
	AddRadioTool(IDM_ANIMSET_LOD_3, *LocalizeUnrealEd("ForceLOD3"), LOD3B, wxNullBitmap, *LocalizeUnrealEd("ForceLOD3") );
	ToggleTool(IDM_ANIMSET_LOD_AUTO, true);
	AddSeparator();
	AddControl(ChunksLabel);
	ChunkComboBox = new WxComboBox( this, IDM_ANIMSET_CHUNKS, TEXT(""), wxDefaultPosition, wxSize( 40, -1 ), 0, NULL, wxCB_READONLY );
	AddControl( ChunkComboBox );
	AddControl(SectionsLabel);
	SectionComboBox = new WxComboBox( this, IDM_ANIMSET_SECTIONS, TEXT(""), wxDefaultPosition, wxSize( 40, -1 ), 0, NULL, wxCB_READONLY );
	AddControl( SectionComboBox );
	AddSeparator();
	AddRadioTool(IDM_ANIMSET_SPEED_100,	*LocalizeUnrealEd("FullSpeed"), Speed100B, wxNullBitmap, *LocalizeUnrealEd("FullSpeed") );
	AddRadioTool(IDM_ANIMSET_SPEED_50,	*LocalizeUnrealEd("50Speed"), Speed50B, wxNullBitmap, *LocalizeUnrealEd("50Speed") );
	AddRadioTool(IDM_ANIMSET_SPEED_25,	*LocalizeUnrealEd("25Speed"), Speed25B, wxNullBitmap, *LocalizeUnrealEd("25Speed") );
	AddRadioTool(IDM_ANIMSET_SPEED_10,	*LocalizeUnrealEd("10Speed"), Speed10B, wxNullBitmap, *LocalizeUnrealEd("10Speed") );
	AddRadioTool(IDM_ANIMSET_SPEED_1,	*LocalizeUnrealEd("1Speed"), Speed1B, wxNullBitmap, *LocalizeUnrealEd("1Speed") );
	ToggleTool(IDM_ANIMSET_SPEED_100, true);
	AddSeparator();
	ProgressiveDrawingSlider = new wxSlider( this, IDM_ANIMSET_PROGRESSIVESLIDER, 10000, 0, 10000 );
	AddControl(ProgressiveDrawingSlider);
	AddSeparator();
	AddCheckTool(IDM_ANIMSET_VIEWALTBONEWEIGHTINGMODE, *LocalizeUnrealEd("ShowAltBoneWeightingEdit"), GoreMode, wxNullBitmap, *LocalizeUnrealEd("ShowAltBoneWeightingEdit") );
	AddSeparator();
	//wxStaticText* VertexInfo = new wxStaticText(this, -1 ,*LocalizeUnrealEd("ShowVertexInfo") );
	//AddControl(VertexInfo);
	//ChunkList = new wxComboBox( this, IDM_ANIMSET_SELECTCHUNKINDEX, TEXT(""), wxDefaultPosition, wxSize(30, 10), 0, NULL, wxCB_READONLY );
	//AddControl(ChunkList);
	//AddCheckTool(IDM_ANIMSET_VERTEX_SHOWTANGENT, *LocalizeUnrealEd("ShowTangent"), TangentMode, wxNullBitmap, *LocalizeUnrealEd("ShowTangent") );
	//AddCheckTool(IDM_ANIMSET_VERTEX_SHOWNORMAL, *LocalizeUnrealEd("ShowNormal"), NormalMode, wxNullBitmap, *LocalizeUnrealEd("ShowNormal") );
	//AddSeparator();
	
	//ColorOption = new wxComboBox( this, IDM_ANIMSET_VERTEX_COLOROPTION, TEXT(""), wxDefaultPosition, wxSize(50, 10), 0, NULL, wxCB_READONLY );
	//ColorOption->Append(TEXT("None"));
	//ColorOption->Append(TEXT("Tangent"));
	//ColorOption->Append(TEXT("Normal"));
	//ColorOption->Append(TEXT("Mirror"));
	//AddControl(ColorOption);

	AddCheckTool(IDM_ANIMSET_TRIANGLESORTMODE, *LocalizeUnrealEd("TriangleSortMode"), TriangleSortMode, wxNullBitmap, *LocalizeUnrealEd("TriangleSortMode") );
	AddCheckTool(IDM_ANIMSET_TRIANGLESORTMODELR, *LocalizeUnrealEd("TriangleSortModeLR"), TriangleSortModeL, wxNullBitmap, *LocalizeUnrealEd("TriangleSortModeLR") );
	EnableTool(IDM_ANIMSET_TRIANGLESORTMODELR,FALSE);

	AddSeparator();
	ActiveListenBitmap.Load(TEXT("VisibilityEyeOn.png"));
	AddCheckTool(IDM_ANIMSET_ACTIVELY_LISTEN_FOR_FILE_CHANGES, *LocalizeUnrealEd("AnimSetEditor_ActiveListen"), ActiveListenBitmap, wxNullBitmap, *LocalizeUnrealEd("AnimSetEditor_ActiveListen") );
	ToggleTool(IDM_ANIMSET_ACTIVELY_LISTEN_FOR_FILE_CHANGES, GEditor->AccessUserSettings().bAutoReimportAnimSets);

	AddSeparator();
	FOVResetBitmap.Load(TEXT("FOVReset.png"));
	AddTool(IDM_ANIMSET_FOVRESET, FOVResetBitmap, *LocalizeUnrealEd("FOVReset") );
	FOVSlider = new wxSlider( this, IDM_ANIMSET_FOVSLIDER, ( GEditor ? ((INT)GEditor->FOVAngle) : 90 ), 5, 170, wxDefaultPosition, wxSize( 165, -1 ) );	// min/max limits enforced by unreal
	AddControl(FOVSlider);

	Realize();
}

/*-----------------------------------------------------------------------------
	WxASVMenuBar
-----------------------------------------------------------------------------*/

WxASVMenuBar::WxASVMenuBar()
{
	// File Menu
	FileMenu = new wxMenu();
	Append( FileMenu, *LocalizeUnrealEd("File") );

	FileMenu->Append( IDM_ANIMSET_IMPORTMESHLOD, *LocalizeUnrealEd("ImportMeshLOD"), TEXT("") );
	FileMenu->AppendSeparator();
	FileMenu->Append( IDM_ANIMSET_NEWANISMET, *LocalizeUnrealEd("NewAnimSet"), TEXT("") );
#if WITH_ACTORX
	FileMenu->Append( IDM_ANIMSET_IMPORTPSA, *LocalizeUnrealEd("ImportPSA"), TEXT("") );
#endif
#if WITH_FBX
	FileMenu->Append( IDM_ANIMSET_IMPORTFBXANIM, *LocalizeUnrealEd("ImportFbxAnim"), TEXT("") );
	FileMenu->Append( IDM_ANIMSET_EXPORTFBXANIM, *LocalizeUnrealEd("ExportFbxAnim"), TEXT("") );
#endif // WITH_FBX
	FileMenu->AppendSeparator();
	FileMenu->Append( IDM_ANIMSET_NEWMORPHSET, *LocalizeUnrealEd("NewMorphTargetSet"), TEXT("") );
	FileMenu->Append( IDM_ANIMSET_IMPORTMORPHTARGET, *LocalizeUnrealEd("ImportMorphTarget"), TEXT("") );
	FileMenu->Append( IDM_ANIMSET_IMPORTMORPHTARGETS, *LocalizeUnrealEd("ImportMorphTargets"), TEXT("") );
	FileMenu->Append( IDM_ANIMSET_IMPORTMORPHTARGET_LOD, *LocalizeUnrealEd("ImportMorphTargetLOD"), TEXT("") );
	FileMenu->Append( IDM_ANIMSET_IMPORTMORPHTARGETS_LOD, *LocalizeUnrealEd("ImportMorphTargetsLOD"), TEXT("") );

	// Edit Menu
	EditMenu = new wxMenu();
	Append( EditMenu, *LocalizeUnrealEd("Edit") );
	EditMenu->Append( IDM_ANIMSET_UNDO, *LocalizeUnrealEd("Undo"), *LocalizeUnrealEd("ToolTip_86") );
	EditMenu->Append( IDM_ANIMSET_REDO, *LocalizeUnrealEd("Redo"), *LocalizeUnrealEd("ToolTip_87") );

	// View Menu
	ViewMenu = new wxMenu();
	Append( ViewMenu, *LocalizeUnrealEd("View") );

	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWBONES, *LocalizeUnrealEd("ShowSkeleton"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWADDITIVEBASE, *LocalizeUnrealEd("ShowAdditiveBase"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWBONENAMES, *LocalizeUnrealEd("ShowBoneNames"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWREFPOSE, *LocalizeUnrealEd("ShowReferencePose"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWWIREFRAME, *LocalizeUnrealEd("ToolTip_5"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWMIRROR, *LocalizeUnrealEd("ShowMirror"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWFLOOR, *LocalizeUnrealEd("ShowFloor"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWGRID, *LocalizeUnrealEd("ShowGrid"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWSOCKETS, *LocalizeUnrealEd("ShowSockets"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWBOUNDS, *LocalizeUnrealEd("ShowBounds"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWCOLLISION, *LocalizeUnrealEd("ShowCollision"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_ShowRawAnimation, *LocalizeUnrealEd("ShowUncompressedAnimation"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWSOFTBODYTETRA, *LocalizeUnrealEd("ShowSoftBodyTetra"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWMORPHKEYS, *LocalizeUnrealEd("ShowMorphKeys"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWALTBONEWEIGHTINGMODE, *LocalizeUnrealEd("ShowAltBoneWeightingEdit"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWCLOTHMOVEDISTSCALE, *LocalizeUnrealEd("ShowClothMovementScale"), TEXT("") );


	// Tangent/Normal/Mirror stuff
	ViewMenu->AppendSeparator();
	TangentSubMenu = new wxMenu();
	ViewMenu->Append(-1, *LocalizeUnrealEd("ShowTangent"), TangentSubMenu );
	TangentSubMenu->AppendCheckItem (IDM_ANIMSET_VERTEX_SHOWTANGENT_AS_VECTOR, *LocalizeUnrealEd("AsVector"));
	TangentSubMenu->AppendCheckItem (IDM_ANIMSET_VERTEX_SHOWTANGENT_AS_TEXTURE, *LocalizeUnrealEd("AsTexture"));

	NormalSubMenu = new wxMenu();
	ViewMenu->Append(-1, *LocalizeUnrealEd("ShowNormal"), NormalSubMenu );
	NormalSubMenu->AppendCheckItem (IDM_ANIMSET_VERTEX_SHOWNORMAL_AS_VECTOR, *LocalizeUnrealEd("AsVector"));
	NormalSubMenu->AppendCheckItem (IDM_ANIMSET_VERTEX_SHOWNORMAL_AS_TEXTURE, *LocalizeUnrealEd("AsTexture"));
	
	ViewMenu->AppendCheckItem(IDM_ANIMSET_VERTEX_SHOWMIRROR, *LocalizeUnrealEd("ShowMirror") );

	SectionSubMenu = new wxMenu();
	ViewMenu->Append(-1, *LocalizeUnrealEd("ShowSection"), SectionSubMenu );
	SectionSubMenu->AppendCheckItem (IDM_ANIMSET_VERTEX_SECTION_0, TEXT("Material Section 0"));
	SectionSubMenu->AppendCheckItem (IDM_ANIMSET_VERTEX_SECTION_1, TEXT("Material Section 1"));
	SectionSubMenu->AppendCheckItem (IDM_ANIMSET_VERTEX_SECTION_2, TEXT("Material Section 2"));
	SectionSubMenu->AppendCheckItem (IDM_ANIMSET_VERTEX_SECTION_3, TEXT("Material Section 3"));
	SectionSubMenu->AppendCheckItem (IDM_ANIMSET_VERTEX_SECTION_4, TEXT("Material Section 4"));

	// Add a menu for displaying UV sets.
	UVSetSubMenu = new wxMenu();
	ViewMenu->Append(-1, *LocalizeUnrealEd("AnimSetViewer_ShowUVs"), UVSetSubMenu );
	const UINT NumUVSetsToShow = IDM_ANIMSET_SHOW_UV_END - IDM_ANIMSET_SHOW_UV_START;
	for( INT TexCoordIdx = 0; TexCoordIdx < NumUVSetsToShow; ++TexCoordIdx )
	{
		UVSetSubMenu->AppendCheckItem( IDM_ANIMSET_SHOW_UV_START+TexCoordIdx, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "AnimSetViewer_ShowUVIndex" ), TexCoordIdx ) ) );
	}

	ViewMenu->Check( IDM_ANIMSET_VIEWSOFTBODYTETRA, true );
	ViewMenu->Check( IDM_ANIMSET_VIEWGRID, true );

	// PhysX debug visualization menu

	GEngine->Exec(TEXT("nxvis PHYSX_CLEAR_ALL"));

	PhysXMenu = new wxMenu();
	Append( PhysXMenu, TEXT("PhysX Debug") );

	PhysXMenu->Append( IDM_ANIMSET_PHYSX_CLEAR_ALL, TEXT("PHYSX CLEAR ALL"), TEXT("") );
	PhysX_Body = new wxMenu();
	PhysXMenu->Append(-1, TEXT("PhysX Body / Actor"), PhysX_Body );
	for (INT i=IDM_ANIMSET_PHYSX_DEBUG_WORLD_AXES; i<=IDM_ANIMSET_PHYSX_DEBUG_ACTOR_AXES; i++)
	{
		PhysX_Body->AppendCheckItem(i,ANSI_TO_TCHAR( gDebugCommands[i-IDM_ANIMSET_PHYSX_CLEAR_ALL] ), TEXT("") );
	}

	PhysX_Joint = new wxMenu();
	PhysXMenu->Append(-1, TEXT("PhysX Joints"), PhysX_Joint );
	for (INT i=IDM_ANIMSET_PHYSX_DEBUG_JOINT_LOCAL_AXES; i<= IDM_ANIMSET_PHYSX_DEBUG_JOINT_LIMITS; i++)
	{
		PhysX_Joint->AppendCheckItem(i,ANSI_TO_TCHAR( gDebugCommands[i-IDM_ANIMSET_PHYSX_CLEAR_ALL] ), TEXT("") );
	}

	PhysX_Contact = new wxMenu();
	PhysXMenu->Append(-1, TEXT("PhysX Contacts"), PhysX_Contact );
	for (INT i=IDM_ANIMSET_PHYSX_DEBUG_CONTACT_POINT; i<=IDM_ANIMSET_PHYSX_DEBUG_CONTACT_FORCE; i++)
	{
		PhysX_Contact->AppendCheckItem(i,ANSI_TO_TCHAR( gDebugCommands[i-IDM_ANIMSET_PHYSX_CLEAR_ALL] ), TEXT("") );
	}


	PhysX_Collision = new wxMenu();
	PhysXMenu->Append(-1, TEXT("PhysX Collision"), PhysX_Collision );
	for (INT i=IDM_ANIMSET_PHYSX_DEBUG_COLLISION_AABBS; i<=IDM_ANIMSET_PHYSX_DEBUG_COLLISION_SKELETONS; i++)
	{
		PhysX_Collision->AppendCheckItem(i,ANSI_TO_TCHAR( gDebugCommands[i-IDM_ANIMSET_PHYSX_CLEAR_ALL] ), TEXT("") );
	}

	PhysX_Fluid = new wxMenu();
	PhysXMenu->Append(-1, TEXT("PhysX Fluid"), PhysX_Fluid );
	for (INT i=IDM_ANIMSET_PHYSX_DEBUG_FLUID_EMITTERS; i<=IDM_ANIMSET_PHYSX_DEBUG_FLUID_PACKET_DATA; i++)
	{
		PhysX_Fluid->AppendCheckItem(i,ANSI_TO_TCHAR( gDebugCommands[i-IDM_ANIMSET_PHYSX_CLEAR_ALL] ), TEXT("") );
	}


	PhysX_Cloth = new wxMenu();
	PhysXMenu->Append(-1, TEXT("PhysX Cloth"), PhysX_Cloth );
	for (INT i=IDM_ANIMSET_PHYSX_DEBUG_CLOTH_MESH; i<=IDM_ANIMSET_PHYSX_DEBUG_CLOTH_VALIDBOUNDS; i++)
	{
		PhysX_Cloth->AppendCheckItem(i,ANSI_TO_TCHAR( gDebugCommands[i-IDM_ANIMSET_PHYSX_CLEAR_ALL] ), TEXT("") );
	}

	PhysX_SoftBody = new wxMenu();
	PhysXMenu->Append(-1, TEXT("PhysX Soft Body"), PhysX_SoftBody );
	for (INT i=IDM_ANIMSET_PHYSX_DEBUG_SOFTBODY_MESH; i<=IDM_ANIMSET_PHYSX_DEBUG_SOFTBODY_VALIDBOUNDS; i++)
	{
		PhysX_SoftBody->AppendCheckItem(i,ANSI_TO_TCHAR( gDebugCommands[i-IDM_ANIMSET_PHYSX_CLEAR_ALL] ), TEXT("") );
	}
#if WITH_APEX
	GEngine->Exec(TEXT("apexvis APEX_CLEAR_ALL"));


	PhysXMenu->AppendSeparator();
	PhysXMenu->Append( IDM_ANIMSET_APEX_CLEAR_ALL, TEXT("APEX CLEAR ALL"), TEXT("") );


	Apex_Clothing = new wxMenu();
	PhysXMenu->Append(-1, TEXT("APEX Clothing"), Apex_Clothing );
	char* moduleName = "Clothing";
	UINT NumApexDebugVisualizationNames = GApexCommands->GetNumDebugVisualizationNames(moduleName);
	UINT commandsIndex = IDM_ANIMSET_APEX_DBG_CLOTHING_BEGIN - IDM_ANIMSET_PHYSX_CLEAR_ALL;
	for (UINT i = 0; i < NumApexDebugVisualizationNames; ++i)
	{
		FString name;
		GApexCommands->GetDebugVisualizationNamePretty(name, "Clothing", i);
		Apex_Clothing->AppendCheckItem(IDM_ANIMSET_APEX_DBG_CLOTHING_BEGIN + i, *name, TEXT("") );
	}


	Apex_Destructible = new wxMenu();
	PhysXMenu->Append(-1, TEXT("APEX Destructible"), Apex_Destructible );
	moduleName = "Destructible";
	NumApexDebugVisualizationNames = GApexCommands->GetNumDebugVisualizationNames(moduleName);
	commandsIndex = IDM_ANIMSET_APEX_DBG_DESTRUCTIBLES_BEGIN - IDM_ANIMSET_PHYSX_CLEAR_ALL;
	for (UINT i = 0; i < NumApexDebugVisualizationNames; ++i)
	{
		FString name;
		GApexCommands->GetDebugVisualizationNamePretty(name, moduleName, i);
		Apex_Destructible->AppendCheckItem(IDM_ANIMSET_APEX_DBG_DESTRUCTIBLES_BEGIN + i, *name, TEXT("") );
	}

	Apex_Misc = new wxMenu();
	PhysXMenu->Append(-1, TEXT("APEX Misc"), Apex_Misc );
	moduleName = NULL;
	NumApexDebugVisualizationNames = GApexCommands->GetNumDebugVisualizationNames(moduleName);
	commandsIndex = IDM_ANIMSET_APEX_DBG_MISC_BEGIN - IDM_ANIMSET_PHYSX_CLEAR_ALL;
	for (UINT i = 0; i < NumApexDebugVisualizationNames; ++i)
	{
		FString name;
		GApexCommands->GetDebugVisualizationNamePretty(name, moduleName, i);
		Apex_Misc->AppendCheckItem(IDM_ANIMSET_APEX_DBG_MISC_BEGIN + i, *name, TEXT("") );
	}
#if WITH_APEX_PARTICLES
	Apex_Emitter = new wxMenu();
	PhysXMenu->Append(-1, TEXT("APEX Emitter"), Apex_Emitter );
	moduleName = "Emitter";
	NumApexDebugVisualizationNames = GApexCommands->GetNumDebugVisualizationNames(moduleName);
	commandsIndex = IDM_ANIMSET_APEX_DBG_EMITTERS_BEGIN - IDM_ANIMSET_PHYSX_CLEAR_ALL;
	for (UINT i = 0; i < NumApexDebugVisualizationNames; ++i)
	{
		FString name;
		GApexCommands->GetDebugVisualizationNamePretty(name, moduleName, i);
		Apex_Emitter->AppendCheckItem(IDM_ANIMSET_APEX_DBG_EMITTERS_BEGIN + i, *name, TEXT("") );
	}

	Apex_Iofx = new wxMenu();
	PhysXMenu->Append(-1, TEXT("APEX IFOX"), Apex_Iofx );
	moduleName = "Iofx";
	NumApexDebugVisualizationNames = GApexCommands->GetNumDebugVisualizationNames(moduleName);
	commandsIndex = IDM_ANIMSET_APEX_DBG_IOFX_BEGIN - IDM_ANIMSET_PHYSX_CLEAR_ALL;
	for (UINT i = 0; i < NumApexDebugVisualizationNames; ++i)
	{
		FString name;
		GApexCommands->GetDebugVisualizationNamePretty(name, moduleName, i);
		Apex_Iofx->AppendCheckItem(IDM_ANIMSET_APEX_DBG_IOFX_BEGIN + i, *name, TEXT("") );
	}
#endif // WITH_PARTICLES

#endif // WITH_APEX

	// Mesh Menu
	MeshMenu = new wxMenu();
	Append( MeshMenu, *LocalizeUnrealEd("Mesh") );

	MeshMenu->AppendRadioItem( IDM_ANIMSET_LOD_AUTO, *LocalizeUnrealEd("SetLODAuto"), TEXT("") );
	MeshMenu->AppendRadioItem( IDM_ANIMSET_LOD_BASE, *LocalizeUnrealEd("ForceLODBaseMesh"), TEXT("") );
	MeshMenu->AppendRadioItem( IDM_ANIMSET_LOD_1, *LocalizeUnrealEd("ForceLOD1"), TEXT("") );
	MeshMenu->AppendRadioItem( IDM_ANIMSET_LOD_2, *LocalizeUnrealEd("ForceLOD2"), TEXT("") );
	MeshMenu->AppendRadioItem( IDM_ANIMSET_LOD_3, *LocalizeUnrealEd("ForceLOD3"), TEXT("") );
	MeshMenu->Check( IDM_ANIMSET_LOD_AUTO, true);
	MeshMenu->AppendSeparator();
#if WITH_SIMPLYGON
	MeshMenu->Append( IDM_ANIMSET_GENERATELOD, *LocalizeUnrealEd("GenerateLOD") );
#endif // #if WITH_SIMPLYGON
	MeshMenu->Append( IDM_ANIMSET_REMOVELOD, *LocalizeUnrealEd("RemoveLOD"), TEXT("") );
	MeshMenu->AppendSeparator();
	MeshMenu->Append( IDM_ANIMSET_AUTOMIRRORTABLE, *LocalizeUnrealEd("AutoMirrorTable"), TEXT("") );
	MeshMenu->Append( IDM_ANIMSET_CHECKMIRRORTABLE, *LocalizeUnrealEd("CheckMirrorTable"), TEXT("") );
	MeshMenu->Append( IDM_ANIMSET_COPYMIRRORTABLE, *LocalizeUnrealEd("CopyMirrorTableToClipboard"), TEXT("") );
	MeshMenu->Append( IDM_ANIMSET_COPYMIRRORTABLEFROM, *LocalizeUnrealEd("CopyMirrorTableFromMesh"), TEXT("") );
	MeshMenu->AppendSeparator();
	MeshMenu->Append( IDM_ANIMSET_UPDATEBOUNDS, *LocalizeUnrealEd("UpdateBounds"), TEXT("") );
	MeshMenu->Append( IDM_ANIMSET_COPYMESHBONES, *LocalizeUnrealEd("CopyBoneNamesToClipboard"), TEXT("") );
	MeshMenu->Append( IDM_ANIMSET_COPYWEIGHTEDMESHBONES, *LocalizeUnrealEd("CopyWeightedBoneNamesToClipboard"), TEXT("") );
	MeshMenu->Append( IDM_ANIMSET_MERGEMATERIALS, *LocalizeUnrealEd("MergeMeshMaterials"), TEXT("") );
	MeshMenu->AppendSeparator();
	MeshMenu->Append( IDM_ANIMSET_FIXUPMESHBONES, *LocalizeUnrealEd("FixupBoneNames"), TEXT("") );
	MeshMenu->Append( IDM_ANIMSET_SWAPLODSECTIONS, *LocalizeUnrealEd("SwapLODSections"), TEXT("") );
	MeshMenu->AppendSeparator();
	// Socket Manager
	MeshMenu->Append( IDM_ANIMSET_OPENSOCKETMGR, *LocalizeUnrealEd("ToolTip_6"), TEXT("") );

	// AnimSet Menu
	AnimSetMenu = new wxMenu();
	Append( AnimSetMenu, *LocalizeUnrealEd("AnimSet") );

	AnimSetMenu->Append( IDM_ANIMSET_EMPTYSET, *LocalizeUnrealEd("ResetAnimSet"), TEXT("") );
	AnimSetMenu->AppendSeparator();
	AnimSetMenu->Append( IDM_ANIMSET_DELETETRACK, *LocalizeUnrealEd("DeleteTrack"), TEXT("") );
	AnimSetMenu->AppendSeparator();
	AnimSetMenu->Append( IDM_ANIMSET_DELETEMORPHTRACK, *LocalizeUnrealEd("AnimSetViewer_DeleteMorphTracksTitle"), TEXT("") );
	AnimSetMenu->AppendSeparator();
	AnimSetMenu->Append( IDM_ANIMSET_COPYTRANSLATIONBONENAMES, *LocalizeUnrealEd("CopyTranslationBoneNames"), TEXT("") );
	AnimSetMenu->AppendSeparator();
	AnimSetMenu->Append( IDM_ANIMSET_ANALYZEANIMSET, *LocalizeUnrealEd("AnalyzeAnimSet"), TEXT("") );
	

	// AnimSeq Menu
	AnimSeqMenu = new wxMenu();
	Append( AnimSeqMenu, *LocalizeUnrealEd("AnimSequence") );

	AnimSeqMenu->Append( IDM_ANIMSET_RENAMESEQ, *LocalizeUnrealEd("RenameSequence"), TEXT("") );
	AnimSeqMenu->Append( IDM_ANIMSET_REMOVE_PREFIX, *LocalizeUnrealEd("RemovePrefixFromSelectedSequences"), TEXT("") );
	AnimSeqMenu->Append( IDM_ANIMSET_DELETESEQ, *LocalizeUnrealEd("DeleteSequence"), TEXT("") );
	AnimSeqMenu->Append( IDM_ANIMSET_COPYSEQ,  *LocalizeUnrealEd("CopySequence"), TEXT("") );
	AnimSeqMenu->Append( IDM_ANIMSET_MOVESEQ,  *LocalizeUnrealEd("MoveSequence"), TEXT("") );

	AnimSeqMenu->AppendSeparator();
	AnimSeqMenu->Append( IDM_ANIMSET_MAKESEQADDITIVE,  *LocalizeUnrealEd("MakeSequencesAdditive"), TEXT("") );
	AnimSeqMenu->Append( IDM_ANIMSET_REBUILDADDITIVE,  *LocalizeUnrealEd("RebuildAdditiveAnimation"), TEXT("") );
	AnimSeqMenu->Append( IDM_ANIMSET_ADDADDITIVETOSEQ, *LocalizeUnrealEd("AddAdditiveToSequence"), TEXT("") );
	AnimSeqMenu->Append( IDM_ANIMSET_SUBTRACTADDITIVETOSEQ, *LocalizeUnrealEd("SubtractAdditiveToSequence"), TEXT("") );

	AnimSeqMenu->AppendSeparator();
	AnimSeqMenu->Append( IDM_ANIMSET_COPYSEQNAME, *LocalizeUnrealEd("CopySequenceName"), TEXT("") );
	AnimSeqMenu->Append( IDM_ANIMSET_COPYSEQNAMELIST, *LocalizeUnrealEd("CopySequenceNameList"), TEXT("") );
	
	AnimSeqMenu->AppendSeparator();
	AnimSeqMenu->Append( IDM_ANIMSET_SEQAPPLYROT, *LocalizeUnrealEd("ApplyRotation"), TEXT("") );
	AnimSeqMenu->Append( IDM_ANIMSET_SEQREZERO, *LocalizeUnrealEd("ReZeroCurrentPosition"), TEXT("") );

	AnimSeqMenu->AppendSeparator();
	AnimSeqMenu->Append( IDM_ANIMSET_SEQDELBEFORE, *LocalizeUnrealEd("RemoveBeforeCurrentPosition"), TEXT("") );
	AnimSeqMenu->Append( IDM_ANIMSET_SEQDELAFTER, *LocalizeUnrealEd("RemoveAfterCurrentPosition"), TEXT("") );

	// Notify Menu

	NotifyMenu = new wxMenu();
	Append( NotifyMenu, *LocalizeUnrealEd("Notifies") );

	NotifyMenu->Append( IDM_ANIMSET_NOTIFYNEW, *LocalizeUnrealEd("NewNotify"), TEXT("") );
	NotifyMenu->Append( IDM_ANIMSET_NOTIFYCOPY, *LocalizeUnrealEd("CopyNotifies"), TEXT("") );
	NotifyMenu->Append( IDM_ANIMSET_NOTIFYSORT, *LocalizeUnrealEd("SortNotifies"), TEXT("") );
	NotifyMenu->Append( IDM_ANIMSET_NOTIFYSHIFT, *LocalizeUnrealEd("ShiftNotifies"), TEXT("") );
	NotifyMenu->Append( IDM_ANIMSET_NOTIFYREMOVE, *LocalizeUnrealEd("RemoveAllNotifies"), TEXT("") );
	NotifyMenu->AppendSeparator();
	NotifyMenu->Append( IDM_ANIMSET_NOTIFY_ENABLEALLPSYS, *LocalizeUnrealEd("NotifyEnableAllPSys"), TEXT("") );
	NotifyMenu->Append( IDM_ANIMSET_NOTIFY_DISABLEALLPSYS, *LocalizeUnrealEd("NotifyDisableAllPSys"), TEXT("") );
	NotifyMenu->Append( IDM_ANIMSET_NOTIFY_REFRESH_ALL_DATA, *LocalizeUnrealEd("NotifyRefreshAllData"), *LocalizeUnrealEd("NotifyRefreshAllDataHelp"));

	// MorphSet Menu
	MorphSetMenu = new wxMenu();
	Append( MorphSetMenu, *LocalizeUnrealEd("MorphSet") );

	MorphSetMenu->Append( IDM_ANIMSET_UPDATE_MORPHTARGET, *LocalizeUnrealEd("UpdateMorphTarget"), TEXT("") );
	
	// Morph Menu
	MorphTargetMenu = new wxMenu();
	Append( MorphTargetMenu, *LocalizeUnrealEd("MorphTarget") );

	MorphTargetMenu->Append( IDM_ANIMSET_DELETE_MORPH, *LocalizeUnrealEd("DeleteMorph"), TEXT("") );

	// AnimationCompresion Menu
	AnimationCompressionMenu = new wxMenu();
	Append( AnimationCompressionMenu, *LocalizeUnrealEd("AnimationCompression") );

	AnimationCompressionMenu->AppendCheckItem( IDM_ANIMSET_OpenAnimationCompressionDlg, *LocalizeUnrealEd("AnimationCompressionSettings"), TEXT("") );

	// Alt. Bone Weight Track editing
	AltBoneWeightingMenu = new wxMenu();
	Append( AltBoneWeightingMenu, *LocalizeUnrealEd("AltBoneWeightingMenu") );

	AltBoneWeightingMenu->Append( IDM_ANIMSET_IMPORTMESHWEIGHTS, *LocalizeUnrealEd("ImportMeshWeights"), TEXT("") );
	AltBoneWeightingMenu->AppendCheckItem( IDM_ANIMSET_TOGGLEMESHWEIGHTS, *LocalizeUnrealEd("ToggleMeshWeights"), TEXT("") );
	AltBoneWeightingMenu->AppendSeparator();
	AltBoneWeightingMenu->AppendCheckItem( IDM_ANIMSET_BONEEDITING_SHOWALLMESHVERTS, *LocalizeUnrealEd("ShowAllMeshVerts"), TEXT("") );
	AltBoneWeightingMenu->Check(IDM_ANIMSET_BONEEDITING_SHOWALLMESHVERTS, true);
	AltBoneWeightingMenu->Append( IDM_ANIMSET_SKELETONTREE_RESETBONEBREAKS, *LocalizeUnrealEd("ResetBoneBreaks"), TEXT("") );

}

// Enable/Disable Alt Bone Weighting Menu
// While AltBoneWeighting Mode, you can't toggle mesh weight on/off
void WxASVMenuBar::EnableAltBoneWeightingMenu(UBOOL bEnable, const USkeletalMeshComponent* PreviewSkelComp)
{
	if (PreviewSkelComp != NULL && 
		PreviewSkelComp->SkeletalMesh != NULL)
	{
		INT LOD = ::Clamp(PreviewSkelComp->PredictedLODLevel, PreviewSkelComp->MinLodModel, PreviewSkelComp->SkeletalMesh->LODModels.Num()-1);
		const FStaticLODModel & LODModel = PreviewSkelComp->SkeletalMesh->LODModels(LOD);
		AltBoneWeightingMenu->Enable(IDM_ANIMSET_TOGGLEMESHWEIGHTS, !bEnable && LODModel.VertexInfluences.Num() > 0);
		AltBoneWeightingMenu->Enable(IDM_ANIMSET_BONEEDITING_SHOWALLMESHVERTS, bEnable ? true : false);
		AltBoneWeightingMenu->Enable(IDM_ANIMSET_SKELETONTREE_RESETBONEBREAKS, bEnable ? true : false);
	}	
}
/*-----------------------------------------------------------------------------
	WxASVStatusBar.
-----------------------------------------------------------------------------*/


WxASVStatusBar::WxASVStatusBar( wxWindow* InParent, wxWindowID InID )
	:	wxStatusBar( InParent, InID )
{
	INT Widths[3] = {-3, -3, -3};

	SetFieldsCount(3, Widths);
}

/**
 * @return		The size of the raw animation data in the specified animset.
 */
static INT GetRawSize(const UAnimSet& AnimSet)
{
	INT ResourceSize = 0;
	for( INT SeqIndex = 0 ; SeqIndex < AnimSet.Sequences.Num() ; ++SeqIndex )
	{
		ResourceSize += AnimSet.Sequences(SeqIndex)->GetApproxRawSize();
	}
	return ResourceSize;
}

/**
 * @return		The size of the compressed animation data in the specified animset.
 */
static INT GetCompressedSize(const UAnimSet& AnimSet)
{
	INT ResourceSize = 0;
	for( INT SeqIndex = 0 ; SeqIndex < AnimSet.Sequences.Num() ; ++SeqIndex )
	{
		ResourceSize += AnimSet.Sequences(SeqIndex)->GetApproxCompressedSize();
	}
	return ResourceSize;
}

void WxASVStatusBar::UpdateStatusBar(WxAnimSetViewer* AnimSetViewer)
{
	// SkeletalMesh status text
	FString MeshStatus( *LocalizeUnrealEd("MeshNone") );
	if(AnimSetViewer->SelectedSkelMesh && AnimSetViewer->SelectedSkelMesh->LODModels.Num() > 0)
	{
		const INT NumTris = AnimSetViewer->SelectedSkelMesh->LODModels(0).GetTotalFaces();
		const INT NumBones = AnimSetViewer->SelectedSkelMesh->RefSkeleton.Num();

		MeshStatus = FString::Printf( LocalizeSecure(LocalizeUnrealEd("MeshTrisBones_F"), *AnimSetViewer->SelectedSkelMesh->GetName(), NumTris, NumBones) );
	}
	SetStatusText( *MeshStatus, 0 );

	// AnimSet status text
	FString AnimSetStatus( *LocalizeUnrealEd("AnimSetNone") );
	if(AnimSetViewer->SelectedAnimSet)
	{
		const INT RawSize				= GetRawSize( *AnimSetViewer->SelectedAnimSet );
		const INT CompressedSize		= GetCompressedSize( *AnimSetViewer->SelectedAnimSet );
		const FLOAT RawSizeMB			= static_cast<FLOAT>( RawSize ) / (1024.f * 1024.f);
		const FLOAT CompressedSizeMB	= static_cast<FLOAT>( CompressedSize ) / (1024.f * 1024.f);

		AnimSetStatus = FString::Printf( LocalizeSecure(LocalizeUnrealEd("AnimSet_F"), *AnimSetViewer->SelectedAnimSet->GetName(), CompressedSizeMB, RawSizeMB) );
	}
	SetStatusText( *AnimSetStatus, 1 );

	// AnimSeq status text
	FString AnimSeqStatus( *LocalizeUnrealEd("AnimSeqNone") );
	if(AnimSetViewer->SelectedAnimSeq)
	{
		const INT RawSize				= AnimSetViewer->SelectedAnimSeq->GetApproxRawSize();
		const INT CompressedSize		= AnimSetViewer->SelectedAnimSeq->GetApproxCompressedSize();
		const FLOAT RawSizeKB			= static_cast<FLOAT>( RawSize ) / 1024.f;
		const FLOAT CompressedSizeKB	= static_cast<FLOAT>( CompressedSize ) / 1024.f;
		const FLOAT SeqLength			= AnimSetViewer->SelectedAnimSeq->SequenceLength;

		AnimSeqStatus = FString::Printf( LocalizeSecure(LocalizeUnrealEd("AnimSeq_F"), *AnimSetViewer->SelectedAnimSeq->SequenceName.ToString(), SeqLength, CompressedSizeKB, RawSizeKB) );
	}
	SetStatusText( *AnimSeqStatus, 2 );
}

/*-----------------------------------------------------------------------------
	WxSkeletonTreePopUpMenu
-----------------------------------------------------------------------------*/

WxSkeletonTreePopUpMenu::WxSkeletonTreePopUpMenu(WxAnimSetViewer* AnimSetViewer)
{
	check(AnimSetViewer);

	Append( IDM_ANIMSET_SKELETONTREE_SHOWBONE, *LocalizeUnrealEd("ShowBones"), TEXT("") );
	Append( IDM_ANIMSET_SKELETONTREE_HIDEBONE, *LocalizeUnrealEd("HideBones"), TEXT("") );
	Append( IDM_ANIMSET_SKELETONTREE_SETBONECOLOR, *LocalizeUnrealEd("SetBonesColor"), TEXT("") );
	AppendSeparator();
	Append( IDM_ANIMSET_SKELETONTREE_SHOWCHILDBONE, *LocalizeUnrealEd("ShowChildBones"), TEXT("") );
	Append( IDM_ANIMSET_SKELETONTREE_HIDECHILDBONE, *LocalizeUnrealEd("HideChildBones"), TEXT("") );
	Append( IDM_ANIMSET_SKELETONTREE_SETCHILDBONECOLOR, *LocalizeUnrealEd("SetChildBonesColor"), TEXT("") );
	AppendSeparator();
	Append( IDM_ANIMSET_SKELETONTREE_COPYBONENAME, *LocalizeUnrealEd("CopyBoneNameToClipBoard"), TEXT("") );
	AppendSeparator();
	// File Menu
	wxMenu * SubMenu = new wxMenu();
	Append(-1, *LocalizeUnrealEd("CalculateBoneBreaks"), SubMenu );
	SubMenu->Append( IDM_ANIMSET_SKELETONTREE_CALCULATEBONEBREAKS, *LocalizeUnrealEd("CalculateBoneBreaks_1"), TEXT("") );
	SubMenu->Append( IDM_ANIMSET_SKELETONTREE_CALCULATEBONEBREAKS_AUTODETECT, *LocalizeUnrealEd("CalculateBoneBreaks_2"), TEXT("") );
	SubMenu->Append( IDM_ANIMSET_SKELETONTREE_CALCULATEBONEBREAKS_RIGIDPREFERRED, *LocalizeUnrealEd("CalculateBoneBreaks_3"), TEXT("") );
	Append( IDM_ANIMSET_SKELETONTREE_DELETEBONEBREAK, *LocalizeUnrealEd("DeleteBoneBreaks"), TEXT("") );
	Append( IDM_ANIMSET_SKELETONTREE_RESETBONEBREAKS, *LocalizeUnrealEd("ResetBoneBreaks"), TEXT("") );
	if (!AnimSetViewer->PreviewSkelComp || AnimSetViewer->PreviewSkelComp->bDrawBoneInfluences==FALSE)
	{
		SubMenu->Enable( IDM_ANIMSET_SKELETONTREE_CALCULATEBONEBREAKS, false );
		SubMenu->Enable( IDM_ANIMSET_SKELETONTREE_CALCULATEBONEBREAKS_AUTODETECT, false );
		SubMenu->Enable( IDM_ANIMSET_SKELETONTREE_CALCULATEBONEBREAKS_RIGIDPREFERRED, false );
		Enable( IDM_ANIMSET_SKELETONTREE_DELETEBONEBREAK, false );
		Enable( IDM_ANIMSET_SKELETONTREE_RESETBONEBREAKS, false );
	}

	AppendCheckItem( IDM_ANIMSET_SKELETONTREE_SHOWBLENDWEIGHTS, *LocalizeUnrealEd("RenderBlendWeights"), TEXT("") );
	if (AnimSetViewer->PreviewSkelComp)
	{
		Check( IDM_ANIMSET_SKELETONTREE_SHOWBLENDWEIGHTS, AnimSetViewer->PreviewSkelComp->ColorRenderMode==ESCRM_BoneWeights ? true : false);
	}
	else
	{
		Check( IDM_ANIMSET_SKELETONTREE_SHOWBLENDWEIGHTS, false);
	}
}

WxSkeletonTreePopUpMenu::~WxSkeletonTreePopUpMenu()
{
}

/*-----------------------------------------------------------------------------
	WxAddAdditiveAnimDialog
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxAddAdditiveAnimDialog, wxDialog)
	EVT_BUTTON( wxID_OK, WxAddAdditiveAnimDialog::OnOK )
	EVT_LISTBOX( IDM_ANIMSET_ADDADDITIVE_BUILDMETHOD_LISTBOX, WxAddAdditiveAnimDialog::OnBuildMethodChange )
END_EVENT_TABLE()

WxAddAdditiveAnimDialog::WxAddAdditiveAnimDialog(const wxString& StringValue, const wxString& WindowStringValue, wxWindow *parent, WxAnimSetViewer* InAnimSetViewer)
{
	if( !wxDialog::Create(parent, wxID_ANY, WindowStringValue, wxDefaultPosition, wxDefaultSize, wxCHOICEDLG_STYLE) )
	{
		return;
	}

	// Keep a pointer to the AnimSetViewer
	AnimSetViewer = InAnimSetViewer;

	wxBoxSizer *topsizer = new wxBoxSizer( wxVERTICAL );

    // text message
    topsizer->Add(CreateTextSizer(*LocalizeUnrealEd("AAD_DestinationAnimationName")), wxSizerFlags().Expand().TripleBorder());

	// Destination Animation Name Edit Box
	AnimNameCtrl = new wxTextCtrl(this, IDM_ANIMSET_ADDADDITIVE_TEXTCTRL, StringValue, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, *LocalizeUnrealEd("AAD_DestinationAnimationName") );
	topsizer->Add(AnimNameCtrl, wxSizerFlags().Expand().TripleBorder(wxLEFT | wxRIGHT));

    // text message
    topsizer->Add(CreateTextSizer(*LocalizeUnrealEd("AACT_SelectBuildMethod")), wxSizerFlags().Expand().TripleBorder());

	// Method Selection
	wxString* Choices = new wxString[EAA_MAX];
	Choices[EAA_ScaleAdditiveToSource] = *LocalizeUnrealEd("AAD_ScaleAdditiveToSource");
	Choices[EAA_ScaleSourceToAdditive] = *LocalizeUnrealEd("AAD_ScaleSourceToAdditive");
	BuildMethodCtrl = new wxListBox(this, IDM_ANIMSET_ADDADDITIVE_BUILDMETHOD_LISTBOX, wxDefaultPosition, wxSize(300, 50), EAA_MAX, Choices, wxLB_SINGLE | wxLB_ALWAYS_SB);

	topsizer->Add(BuildMethodCtrl, wxSizerFlags().Expand().TripleBorder(wxLEFT | wxRIGHT));

	// text message
    topsizer->Add(CreateTextSizer(*LocalizeUnrealEd("AAD_SelectAdditiveAnimation")), wxSizerFlags().Expand().TripleBorder());

	// List of animations
	AnimListCtrl = new wxListBox(this, IDM_ANIMSET_ADDADDITIVE_LISTBOX, wxDefaultPosition, wxSize(300, 200), 0, NULL, wxLB_SINGLE | wxLB_ALWAYS_SB | wxLB_SORT);
	topsizer->Add(AnimListCtrl, wxSizerFlags().Expand().Proportion(1).TripleBorder(wxLEFT | wxRIGHT));

    // Buttons 
    wxSizer *buttonSizer = CreateSeparatedButtonSizer(wxCHOICEDLG_STYLE & ButtonSizerFlags);
    if( buttonSizer )
    {
        topsizer->Add(buttonSizer, wxSizerFlags().Expand().DoubleBorder());
    }

    SetSizer( topsizer );

    topsizer->SetSizeHints( this );
    topsizer->Fit( this );

    if( wxCHOICEDLG_STYLE & wxCENTRE )
	{
        Centre(wxBOTH);
	}

	UpdateAnimationList();

	// Select Scaled animation by default.
	SelectedBuildMethod = EAA_ScaleSourceToAdditive;
	BuildMethodCtrl->SetSelection(EAA_ScaleSourceToAdditive);
	UpdateAnimationList();
	BuildMethodCtrl->SetFocus();
}

void WxAddAdditiveAnimDialog::OnOK( wxCommandEvent& In )
{
	// When OK is pressed, just close dialog.
	EndModal(wxID_OK);
}

/** Update AnimList Listbox */
void WxAddAdditiveAnimDialog::UpdateAnimationList()
{
	AnimListCtrl->Freeze();
	AnimListCtrl->Clear();

	// If RefPose is selected, no need to choose an animation
	if( AnimSetViewer && AnimSetViewer->SelectedAnimSet )
	{
		UAnimSet* SelectedAnimSet = AnimSetViewer->SelectedAnimSet;
		for(INT i=0; i<SelectedAnimSet->Sequences.Num(); i++)
		{		
			UAnimSequence* Seq = SelectedAnimSet->Sequences(i);

			if( Seq && Seq->bIsAdditive )
			{
				FString SeqString = FString::Printf( TEXT("%s"), *Seq->SequenceName.ToString());
				AnimListCtrl->Append( *SeqString, Seq );
			}
		}
	}

	AnimListCtrl->Thaw();
}

/** Return the selected additive animation */
UAnimSequence* WxAddAdditiveAnimDialog::GetSelectedAnimation()
{
	if( AnimSetViewer && AnimSetViewer->SelectedAnimSet)
	{
		const FName RefSeqName = FName( AnimListCtrl->GetStringSelection() );
		UAnimSequence* RefAnimSeq = AnimSetViewer->SelectedAnimSet->FindAnimSequence(RefSeqName);
		return RefAnimSeq;
	}

	return NULL;
}

/** Returns DestinationAnimationName */
FName WxAddAdditiveAnimDialog::GetDestinationAnimationName()
{
	return FName(AnimNameCtrl->GetValue());
}

/** Return Build Method to use to create additive animation */
EAddAdditive WxAddAdditiveAnimDialog::GetBuildMethod()
{
	return SelectedBuildMethod;
}

/** Helper function */
static INT GetWxListSelectedItem(wxListBoxBase& ListBox)
{
	wxArrayInt Selection;
	ListBox.GetSelections(Selection);
	
	if( Selection.Count() > 0 )
	{
		return Selection.Item(0);
	}

	return INDEX_NONE;
}

/** Event called when BuildMethod selection changes in ListBox */
void WxAddAdditiveAnimDialog::OnBuildMethodChange( wxCommandEvent& In )
{
	SelectedBuildMethod = (EAddAdditive)GetWxListSelectedItem(*BuildMethodCtrl);
}

/*-----------------------------------------------------------------------------
	WxConvertToAdditiveDialog
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxConvertToAdditiveDialog, wxDialog)
	EVT_BUTTON( wxID_OK, WxConvertToAdditiveDialog::OnOK )
	EVT_LISTBOX( IDM_ANIMSET_MAKEADDITIVE_LB1, WxConvertToAdditiveDialog::OnBuildMethodChange )
END_EVENT_TABLE()

WxConvertToAdditiveDialog::WxConvertToAdditiveDialog(wxWindow *parent, WxAnimSetViewer* InAnimSetViewer)
{
	if( !wxDialog::Create(parent, wxID_ANY, *LocalizeUnrealEd("AACT_CreateAdditiveAnimation"), wxDefaultPosition, wxDefaultSize, wxCHOICEDLG_STYLE) )
	{
		return;
	}

	// Keep a pointer to the AnimSetViewer
	AnimSetViewer = InAnimSetViewer;

	wxBoxSizer *topsizer = new wxBoxSizer( wxVERTICAL );

    // text message
    topsizer->Add(CreateTextSizer(*LocalizeUnrealEd("AACT_SelectBuildMethod")), wxSizerFlags().Expand().TripleBorder());

	// Method Selection
	wxString* Choices = new wxString[CTA_MAX];
	Choices[CTA_RefPose] = *LocalizeUnrealEd("AACT_ReferencePose");
	Choices[CTA_AnimFirstFrame] = *LocalizeUnrealEd("AACT_AnimationFirstFrame");
	Choices[CTA_AnimScaled] = *LocalizeUnrealEd("AACT_AnimationScaled");
	BuildMethod = new wxListBox(this, IDM_ANIMSET_MAKEADDITIVE_LB1, wxDefaultPosition, wxSize(300, 50), CTA_MAX, Choices, wxLB_SINGLE | wxLB_ALWAYS_SB);

	topsizer->Add(BuildMethod, wxSizerFlags().Expand().TripleBorder(wxLEFT | wxRIGHT));

	// text message
    topsizer->Add(CreateTextSizer(*LocalizeUnrealEd("AACT_PickBaseAnimation")), wxSizerFlags().Expand().TripleBorder());

	// List of animations
	AnimList = new wxListBox(this, IDM_ANIMSET_MAKEADDITIVE_LB2, wxDefaultPosition, wxSize(300, 200), 0, NULL, wxLB_SINGLE | wxLB_ALWAYS_SB | wxLB_SORT);

	topsizer->Add(AnimList, wxSizerFlags().Expand().Proportion(1).TripleBorder(wxLEFT | wxRIGHT));

	// Checkbox
	LoopingAnimCB = new wxCheckBox(this, IDM_ANIMSET_MAKEADDITIVE_LOOPINGCB, *LocalizeUnrealEd("AACT_LoopingAnim"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	topsizer->Add(LoopingAnimCB, wxSizerFlags().Expand().TripleBorder());

    // Buttons 
    wxSizer *buttonSizer = CreateSeparatedButtonSizer(wxCHOICEDLG_STYLE & ButtonSizerFlags);
    if( buttonSizer )
    {
        topsizer->Add(buttonSizer, wxSizerFlags().Expand().DoubleBorder());
    }

    SetSizer( topsizer );

    topsizer->SetSizeHints( this );
    topsizer->Fit( this );

    if( wxCHOICEDLG_STYLE & wxCENTRE )
	{
        Centre(wxBOTH);
	}

	// Select Scaled animation by default.
	SelectedBuildMethod = CTA_AnimScaled;
	BuildMethod->SetSelection(CTA_AnimScaled);
	UpdateAnimationList();
	BuildMethod->SetFocus();
}

void WxConvertToAdditiveDialog::OnOK( wxCommandEvent& In )
{
	// When OK is pressed, just close dialog.
	EndModal(wxID_OK);
}

/** Return the selected animation to use as a base */
UAnimSequence* WxConvertToAdditiveDialog::GetSelectedAnimation()
{
	if( AnimSetViewer && AnimSetViewer->SelectedAnimSet )
	{
		const FName RefSeqName = FName( AnimList->GetStringSelection() );
		UAnimSequence* RefAnimSeq = AnimSetViewer->SelectedAnimSet->FindAnimSequence(RefSeqName);
		return RefAnimSeq;
	}

	return NULL;
}

/** Return Build Method to use to create additive animation */
EConvertToAdditive WxConvertToAdditiveDialog::GetBuildMethod()
{
	return SelectedBuildMethod;
}

/** Event called when BuildMethod selection changes in ListBox */
void WxConvertToAdditiveDialog::OnBuildMethodChange( wxCommandEvent& In )
{
	SelectedBuildMethod = (EConvertToAdditive)GetWxListSelectedItem(*BuildMethod);
	// Disable selection if ref pose build mode is selected.
	AnimList->Enable( SelectedBuildMethod != CTA_RefPose );
	// LoopingAnim doesn't make sense w/ RefPose and AnimFirstFrame
	LoopingAnimCB->Enable( SelectedBuildMethod != CTA_RefPose && SelectedBuildMethod != CTA_AnimFirstFrame );
}

/** Return Build Method to use to create additive animation */
UBOOL WxConvertToAdditiveDialog::GetLoopingAnim()
{
	return LoopingAnimCB->GetValue();
}

/** Update AnimList Listbox */
void WxConvertToAdditiveDialog::UpdateAnimationList()
{
	AnimList->Freeze();
	AnimList->Clear();

	// If RefPose is selected, no need to choose an animation
	if( AnimSetViewer && AnimSetViewer->SelectedAnimSet )
	{
		UAnimSet* SelectedAnimSet = AnimSetViewer->SelectedAnimSet;
		for(INT i=0; i<SelectedAnimSet->Sequences.Num(); i++)
		{
			UAnimSequence* Seq = SelectedAnimSet->Sequences(i);

			FString SeqString = FString::Printf( TEXT("%s"), *Seq->SequenceName.ToString());
			AnimList->Append( *SeqString, Seq );
		}
	}

	AnimList->Thaw();
}


void WxAnimSetViewer::OnPhysXDebug(wxCommandEvent& In)
{
	if ( In.GetId() == IDM_ANIMSET_PHYSX_CLEAR_ALL )
	{
		GEngine->Exec(TEXT("nxvis PHYSX_CLEAR_ALL"));
    	for (INT i=IDM_ANIMSET_PHYSX_DEBUG_WORLD_AXES; i<=IDM_ANIMSET_PHYSX_DEBUG_ACTOR_AXES; i++)
    	{
    		MenuBar->PhysX_Body->Check(i, false );
    	}
    	for (INT i=IDM_ANIMSET_PHYSX_DEBUG_JOINT_LOCAL_AXES; i<= IDM_ANIMSET_PHYSX_DEBUG_JOINT_LIMITS; i++)
    	{
    		MenuBar->PhysX_Joint->Check(i,false );
    	}
    	for (INT i=IDM_ANIMSET_PHYSX_DEBUG_CONTACT_POINT; i<=IDM_ANIMSET_PHYSX_DEBUG_CONTACT_FORCE; i++)
    	{
    		MenuBar->PhysX_Contact->Check(i,false);
    	}
    	for (INT i=IDM_ANIMSET_PHYSX_DEBUG_COLLISION_AABBS; i<=IDM_ANIMSET_PHYSX_DEBUG_COLLISION_SKELETONS; i++)
    	{
    		MenuBar->PhysX_Collision->Check(i,false );
    	}
    	for (INT i=IDM_ANIMSET_PHYSX_DEBUG_FLUID_EMITTERS; i<=IDM_ANIMSET_PHYSX_DEBUG_FLUID_PACKET_DATA; i++)
    	{
    		MenuBar->PhysX_Fluid->Check(i,false);
    	}
    	for (INT i=IDM_ANIMSET_PHYSX_DEBUG_CLOTH_MESH; i<=IDM_ANIMSET_PHYSX_DEBUG_CLOTH_VALIDBOUNDS; i++)
    	{
    		MenuBar->PhysX_Cloth->Check(i,false);
    	}
    	for (INT i=IDM_ANIMSET_PHYSX_DEBUG_SOFTBODY_MESH; i<=IDM_ANIMSET_PHYSX_DEBUG_SOFTBODY_VALIDBOUNDS; i++)
    	{
    		MenuBar->PhysX_SoftBody->Check(i,false);
    	}
	}
	else
	{
		char scratch[512];
		StringCbPrintfA(scratch,512,"nxvis %s %s", gDebugCommands[ In.GetId() - IDM_ANIMSET_PHYSX_CLEAR_ALL], In.IsChecked() ? "true" : "false" );
		GEngine->Exec( ANSI_TO_TCHAR(scratch));
	}
}

#if WITH_APEX
void WxAnimSetViewer::OnApexDebug(wxCommandEvent& In)
{
	if ( In.GetId() == IDM_ANIMSET_APEX_CLEAR_ALL )
	{
		GEngine->Exec(TEXT("apexvis APEX_CLEAR_ALL"));
		for (UINT i=IDM_ANIMSET_APEX_CLEAR_ALL+1; i<IDM_ANIMSET_LAST; i++)
		{
			if( i >= IDM_ANIMSET_APEX_DBG_CLOTHING_BEGIN && i < IDM_ANIMSET_APEX_DBG_CLOTHING_BEGIN+GApexCommands->GetNumDebugVisualizationNames("Clothing") )
			{
				MenuBar->Apex_Clothing->Check(i,false);
			}
			else if( i >= IDM_ANIMSET_APEX_DBG_DESTRUCTIBLES_BEGIN && i < IDM_ANIMSET_APEX_DBG_DESTRUCTIBLES_BEGIN+GApexCommands->GetNumDebugVisualizationNames("Destructible") )
			{
				MenuBar->Apex_Destructible->Check(i,false);
			}
			else if( i >= IDM_ANIMSET_APEX_DBG_MISC_BEGIN && i < IDM_ANIMSET_APEX_DBG_MISC_BEGIN+GApexCommands->GetNumDebugVisualizationNames(NULL) )
			{
				MenuBar->Apex_Misc->Check(i,false);
			}
#if WITH_APEX_PARTICLES
			else if( i >= IDM_ANIMSET_APEX_DBG_EMITTERS_BEGIN && i < IDM_ANIMSET_APEX_DBG_EMITTERS_BEGIN+GApexCommands->GetNumDebugVisualizationNames("Emitter") )
			{
				MenuBar->Apex_Emitter->Check(i,false);
			}
			else if( i >= IDM_ANIMSET_APEX_DBG_IOFX_BEGIN && i < IDM_ANIMSET_APEX_DBG_IOFX_BEGIN+GApexCommands->GetNumDebugVisualizationNames("Clothing") )
			{
				MenuBar->Apex_Iofx->Check(i,false);
			}
#endif
		}
	}
	else if (In.GetId() >= IDM_ANIMSET_APEX_DBG_BEGIN && In.GetId() < IDM_ANIMSET_APEX_DBG_END)
	{
		const char* commandName = NULL;
		INT id = In.GetId();
		if (id >= IDM_ANIMSET_APEX_DBG_CLOTHING_BEGIN && id <= IDM_ANIMSET_APEX_DBG_CLOTHING_END)
		{
			commandName = GApexCommands->GetDebugVisualizationName("Clothing", id - IDM_ANIMSET_APEX_DBG_CLOTHING_BEGIN);
		}
		else if( id >= IDM_ANIMSET_APEX_DBG_DESTRUCTIBLES_BEGIN && id <= IDM_ANIMSET_APEX_DBG_DESTRUCTIBLES_END )
		{
			commandName = GApexCommands->GetDebugVisualizationName("Destructible", id - IDM_ANIMSET_APEX_DBG_DESTRUCTIBLES_BEGIN);
		}
		else if( id >= IDM_ANIMSET_APEX_DBG_MISC_BEGIN && id <= IDM_ANIMSET_APEX_DBG_MISC_END)
		{
			commandName = GApexCommands->GetDebugVisualizationName(NULL, id - IDM_ANIMSET_APEX_DBG_MISC_BEGIN);
		}
#if WITH_APEX_PARTICLES
		else if( id >= IDM_ANIMSET_APEX_DBG_EMITTERS_BEGIN && id <= IDM_ANIMSET_APEX_DBG_EMITTERS_END )
		{
			commandName = GApexCommands->GetDebugVisualizationName("Emitter", id - IDM_ANIMSET_APEX_DBG_EMITTERS_BEGIN);
		}
		else if( id >= IDM_ANIMSET_APEX_DBG_IOFX_BEGIN && id <= IDM_ANIMSET_APEX_DBG_IOFX_END )
		{
			commandName = GApexCommands->GetDebugVisualizationName("Iofx", id - IDM_ANIMSET_APEX_DBG_IOFX_BEGIN);
		}
#endif
		char scratch[512];
		StringCbPrintfA(scratch,512,"%s %s %s","apexvis", commandName, In.IsChecked() ? "true" : "false" );
		GEngine->Exec( ANSI_TO_TCHAR(scratch));
	}
}
#endif
