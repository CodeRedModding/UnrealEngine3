/*=============================================================================
	AnimSetViewerMenus.cpp: AnimSet viewer menus
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EngineAnimClasses.h"
#include "AnimSetViewer.h"

///////////////////// MENU HANDLERS

void WxAnimSetViewer::OnSkelMeshComboChanged(wxCommandEvent& In)
{
	INT SelIndex = SkelMeshCombo->GetSelection();
	if(SelIndex == -1)
		return; // Should not be possible to select no skeletal mesh!

	USkeletalMesh* NewSelectedSkelMesh = (USkeletalMesh*)SkelMeshCombo->GetClientData(SelIndex);
	SetSelectedSkelMesh( NewSelectedSkelMesh );
}

void WxAnimSetViewer::OnAnimSetComboChanged(wxCommandEvent& In)
{
	INT SelIndex = AnimSetCombo->GetSelection();
	if(SelIndex == -1)
		return;

	UAnimSet* NewSelectedAnimSet = (UAnimSet*)AnimSetCombo->GetClientData(SelIndex);
	SetSelectedAnimSet( NewSelectedAnimSet );
}

void WxAnimSetViewer::OnAnimSeqListChanged(wxCommandEvent& In)
{
	INT SelIndex = AnimSeqList->GetSelection();
	if(SelIndex == -1)
		return;

	UAnimSequence* NewSelectedAnimSeq = (UAnimSequence*)AnimSeqList->GetClientData(SelIndex);
	SetSelectedAnimSequence(NewSelectedAnimSeq);
}

// Create a new AnimSet, defaulting to in the package of the selected SkeletalMesh...
void WxAnimSetViewer::OnNewAnimSet(wxCommandEvent& In)
{
	CreateNewAnimSet();
}

void WxAnimSetViewer::OnImportPSA(wxCommandEvent& In)
{
	ImportPSA();
}

void WxAnimSetViewer::OnTimeScrub(wxScrollEvent& In)
{
	INT NewIntPosition = In.GetPosition();
	FLOAT NewPosition = (FLOAT)NewIntPosition/(FLOAT)ASV_SCRUBRANGE;

	if(SelectedAnimSeq)
	{
		check(SelectedAnimSeq->SequenceName == PreviewAnimNode->AnimSeqName);

		PreviewAnimNode->SetPosition( NewPosition * SelectedAnimSeq->SequenceLength );

		// Update position display on status bar
		StatusBar->UpdateStatusBar(this);
	}
}

void WxAnimSetViewer::OnViewBones(wxCommandEvent& In)
{
	PreviewSkelComp->bDisplayBones = In.IsChecked();
}

void WxAnimSetViewer::OnViewBoneNames(wxCommandEvent& In)
{
	ASVPreviewVC->bShowBoneNames = In.IsChecked();
}

void WxAnimSetViewer::OnViewFloor(wxCommandEvent& In)
{
	ASVPreviewVC->bShowFloor = In.IsChecked();
}

void WxAnimSetViewer::OnViewRefPose(wxCommandEvent& In)
{
	PreviewSkelComp->bForceRefpose = In.IsChecked();
}

void WxAnimSetViewer::OnViewWireframe(wxCommandEvent& In)
{
	if( In.IsChecked() )
	{
		ASVPreviewVC->ViewMode = SVM_Wireframe;
	}
	else
	{
		ASVPreviewVC->ViewMode = SVM_Lit;

	}
}

void WxAnimSetViewer::OnViewGrid( wxCommandEvent& In )
{
	if( In.IsChecked() )
	{
		ASVPreviewVC->ShowFlags |= SHOW_Grid;
	}
	else
	{
		ASVPreviewVC->ShowFlags &= ~SHOW_Grid;

	}
}

void WxAnimSetViewer::OnLoopAnim(wxCommandEvent& In)
{
	PreviewAnimNode->bLooping = !(PreviewAnimNode->bLooping);

	RefreshPlaybackUI();
}

void WxAnimSetViewer::OnPlayAnim(wxCommandEvent& In)
{
	if(!PreviewAnimNode->bPlaying)
	{
		PreviewAnimNode->PlayAnim(PreviewAnimNode->bLooping, 1.f);
	}
	else
	{
		PreviewAnimNode->StopAnim();
	}

	RefreshPlaybackUI();
}

void WxAnimSetViewer::OnEmptySet(wxCommandEvent& In)
{
	EmptySelectedSet();
}

void WxAnimSetViewer::OnRenameSequence(wxCommandEvent& In)
{
	RenameSelectedSeq();
}

void WxAnimSetViewer::OnDeleteSequence(wxCommandEvent& In)
{
	DeleteSelectedSequence();
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
	UBOOL bCropFromStart = false;
	if(In.GetId() == IDM_ANIMSET_SEQDELBEFORE)
		bCropFromStart = true;

	SequenceCrop(bCropFromStart);
}

void WxAnimSetViewer::OnNotifySort(wxCommandEvent& In)
{
	if(SelectedAnimSeq)
	{
		SelectedAnimSeq->SortNotifies();
	}

	AnimSeqProps->SetObject(NULL, true, false, true);
	AnimSeqProps->SetObject( SelectedAnimSeq, true, false, true );
}


void WxAnimSetViewer::OnCopySequenceName( wxCommandEvent& In )
{
	if(SelectedAnimSeq)
	{
		appClipboardCopy( *(SelectedAnimSeq->SequenceName) );
	}
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
				BoneNames += FString::Printf( TEXT("%3d: %s%s\r\n"), i, *LeadingSpace, *(MeshBone.Name) );
			}
			else
			{
				BoneNames += FString::Printf( TEXT("%3d: %s%s (P: %d)\r\n"), i, *LeadingSpace, *(MeshBone.Name), MeshBone.ParentIndex );
			}
		}
	}

	appClipboardCopy( *BoneNames );
}
	


/*-----------------------------------------------------------------------------
	WxASVToolBar
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxASVToolBar, wxToolBar )
END_EVENT_TABLE()

WxASVToolBar::WxASVToolBar( wxWindow* InParent, wxWindowID InID )
: wxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_3DBUTTONS )
{
	Realize();
}

WxASVToolBar::~WxASVToolBar()
{
}

/*-----------------------------------------------------------------------------
	WxASVMenuBar
-----------------------------------------------------------------------------*/

WxASVMenuBar::WxASVMenuBar()
{
	// File Menu
	FileMenu = new wxMenu();
	Append( FileMenu, TEXT("&File") );

	FileMenu->Append( IDM_ANIMSET_NEWANISMET, TEXT("&New AnimSet"), TEXT("") );
	FileMenu->AppendSeparator();
	FileMenu->Append( IDM_ANIMSET_IMPORTPSA, TEXT("&Import PSA"), TEXT("") );


	// View Menu
	ViewMenu = new wxMenu();
	Append( ViewMenu, TEXT("&View") );

	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWBONES, TEXT("Show &Skeleton"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWBONENAMES, TEXT("Show &Bone Names"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWREFPOSE, TEXT("Show &Reference Pose"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWWIREFRAME, TEXT("Show &Wireframe"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWFLOOR, TEXT("Show &Floor"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_ANIMSET_VIEWGRID, TEXT("Show &Grid"), TEXT("") );
	ViewMenu->Check( IDM_ANIMSET_VIEWGRID, true );


	// Mesh Menu
	MeshMenu = new wxMenu();
	Append( MeshMenu, TEXT("&Mesh") );

	MeshMenu->Append( IDM_ANIMSET_COPYMESHBONES, TEXT("&Copy Bone Names To Clipboard"), TEXT("") );


	// AnimSet Menu
	AnimSetMenu = new wxMenu();
	Append( AnimSetMenu, TEXT("AnimSe&t") );

	AnimSetMenu->Append( IDM_ANIMSET_EMPTYSET, TEXT("&Reset AnimSet"), TEXT("") );


	// AnimSeq Menu
	AnimSeqMenu = new wxMenu();
	Append( AnimSeqMenu, TEXT("AnimSe&quence") );

	AnimSeqMenu->Append( IDM_ANIMSET_RENAMESEQ, TEXT("&Rename Sequence"), TEXT("") );
	AnimSeqMenu->Append( IDM_ANIMSET_DELETESEQ, TEXT("&Delete Sequence"), TEXT("") );
	AnimSeqMenu->Append( IDM_ANIMSET_COPYSEQNAME, TEXT("&Copy Sequence Name To Clipboard"), TEXT("") );
	AnimSeqMenu->AppendSeparator();
	AnimSeqMenu->Append( IDM_ANIMSET_SEQAPPLYROT, TEXT("&Apply Rotation"), TEXT("") );
	AnimSeqMenu->Append( IDM_ANIMSET_SEQREZERO, TEXT("Re&Zero On Current Position"), TEXT("") );
	AnimSeqMenu->AppendSeparator();
	AnimSeqMenu->Append( IDM_ANIMSET_SEQDELBEFORE, TEXT("Remove &Before Current Position"), TEXT("") );
	AnimSeqMenu->Append( IDM_ANIMSET_SEQDELAFTER, TEXT("Remove A&fter Current Position"), TEXT("") );

	// Notify Menu

	NotifyMenu = new wxMenu();
	Append( NotifyMenu, TEXT("&Notifies") );

	//NotifyMenu->Append( IDM_ANIMSET_NOTIFYNEW, TEXT("&New Notify"), TEXT("") );
	NotifyMenu->Append( IDM_ANIMSET_NOTIFYSORT, TEXT("&Sort Notifies"), TEXT("") );
}

WxASVMenuBar::~WxASVMenuBar()
{

}

/*-----------------------------------------------------------------------------
	WxASVStatusBar.
-----------------------------------------------------------------------------*/


WxASVStatusBar::WxASVStatusBar( wxWindow* InParent, wxWindowID InID )
: wxStatusBar( InParent, InID )
{
	INT Widths[4] = {-3, -3, -3, 150};

	SetFieldsCount(4, Widths);
}

WxASVStatusBar::~WxASVStatusBar()
{

}

void WxASVStatusBar::UpdateStatusBar( WxAnimSetViewer* AnimSetViewer )
{
	// SkeletalMesh status text
	FString MeshStatus( TEXT("Mesh: None") );
	if(AnimSetViewer->SelectedSkelMesh && AnimSetViewer->SelectedSkelMesh->LODModels.Num() > 0)
	{
		AnimSetViewer->SelectedSkelMesh->LODModels(0).Faces.Load();
		INT NumTris = AnimSetViewer->SelectedSkelMesh->LODModels(0).Faces.Num();

		INT NumBones = AnimSetViewer->SelectedSkelMesh->RefSkeleton.Num();

		MeshStatus = FString::Printf( TEXT("Mesh: %s  Tris: %d  Bones: %d"), AnimSetViewer->SelectedSkelMesh->GetName(), NumTris, NumBones );
	}
	SetStatusText( *MeshStatus, 0 );

	// AnimSet status text
	FString AnimSetStatus( TEXT("AnimSet: None") );
	if(AnimSetViewer->SelectedAnimSet)
	{
		FLOAT SetMem = ((FLOAT)AnimSetViewer->SelectedAnimSet->GetAnimSetMemory())/(1024.f * 1024.f);

		AnimSetStatus = FString::Printf( TEXT("AnimSet: %s   Mem: %7.2fMB"), AnimSetViewer->SelectedAnimSet->GetName(), SetMem );
	}
	SetStatusText( *AnimSetStatus, 1 );

	// AnimSet status text
	FString AnimSeqStatus( TEXT("AnimSeq: None") );
	if(AnimSetViewer->SelectedAnimSeq)
	{
		FLOAT SeqMem = ((FLOAT)AnimSetViewer->SelectedAnimSeq->GetAnimSequenceMemory())/1024.f;

		FLOAT SeqLength = AnimSetViewer->SelectedAnimSeq->SequenceLength;

		AnimSeqStatus = FString::Printf( TEXT("AnimSeq: %s   Len: %4.2fs   Mem: %7.2fKB"), *AnimSetViewer->SelectedAnimSeq->SequenceName, SeqLength, SeqMem );
	}
	SetStatusText( *AnimSeqStatus, 2 );

	// Preview status
	FString PreviewStatus = FString::Printf( TEXT("Preview Pos: %4.2fs"), AnimSetViewer->PreviewAnimNode->CurrentTime );
	SetStatusText( *PreviewStatus, 3 );
}

/*-----------------------------------------------------------------------------
	WxDlgRotateAnimSeq.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgRotateAnimSeq, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgRotateAnimSeq::OnOK )
END_EVENT_TABLE()

WxDlgRotateAnimSeq::WxDlgRotateAnimSeq()
: wxDialog( )
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_ROTATEANIMSEQ") );

	DegreesEntry = (wxTextCtrl*)FindWindow( XRCID( "IDEC_ANGLEENTRY" ) );
	DegreesEntry->SetValue( TEXT("0.0") );

	AxisCombo = (wxComboBox*)FindWindow( XRCID( "IDEC_AXISCOMBO" ) );
	AxisCombo->Append( TEXT("X Axis") );
	AxisCombo->Append( TEXT("Y Axis") );
	AxisCombo->Append( TEXT("Z Axis") );
	AxisCombo->SetSelection(2);

	Degrees = 0.f;
	Axis = AXIS_X;
}

WxDlgRotateAnimSeq::~WxDlgRotateAnimSeq()
{
}

int WxDlgRotateAnimSeq::ShowModal()
{
	return wxDialog::ShowModal();
}

void WxDlgRotateAnimSeq::OnOK( wxCommandEvent& In )
{
	double DegreesNum;
	UBOOL bIsNumber = DegreesEntry->GetValue().ToDouble(&DegreesNum);
	if(bIsNumber)
	{
		Degrees = DegreesNum;
	}

	switch( AxisCombo->GetSelection() )
	{
		case 0:
			Axis = AXIS_X;
			break;

		case 1:
			Axis = AXIS_Y;
			break;

		case 2:
			Axis = AXIS_Z;
			break;
	}

	wxDialog::OnOK( In );
}