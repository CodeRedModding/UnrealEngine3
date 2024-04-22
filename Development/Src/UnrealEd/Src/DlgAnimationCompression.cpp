/*=============================================================================
	DlgAnimationCompression.cpp: AnimSet Viewer's animation compression dialog.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineAnimClasses.h"
#include "DlgAnimationCompression.h"
#include "AnimSetViewer.h"
#include "PropertyWindow.h"
#include "BusyCursor.h"

/** Border width around the socket manager controls. */
static INT AnimCompression_ControlBorder = 4;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxDlgAnimationCompression
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE( WxDlgAnimationCompression, wxFrame )
	EVT_CLOSE( WxDlgAnimationCompression::OnClose )
	EVT_LISTBOX( IDM_DlgAnimationCompression_AlgorithmList, WxDlgAnimationCompression::OnClickAlgorithm )
	EVT_BUTTON( IDM_DlgAnimationCompression_ApplyAlgorithmToSet, WxDlgAnimationCompression::OnApplyAlgorithmToSet )
	EVT_BUTTON( IDM_DlgAnimationCompression_ApplyAlgorithmToSequence, WxDlgAnimationCompression::OnApplyAlgorithmToSequence )
END_EVENT_TABLE()

WxDlgAnimationCompression::WxDlgAnimationCompression(WxAnimSetViewer* InASV, wxWindowID InID)
	:	wxFrame( InASV, InID, *LocalizeUnrealEd("AnimationCompression"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR )
	,	AnimSetViewer( InASV )
	,	SelectedAlgorithm( NULL )
{
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));

	wxBoxSizer* TopSizerH = new wxBoxSizer( wxHORIZONTAL );
	SetSizer(TopSizerH);
	SetAutoLayout(true);

	wxBoxSizer* LeftSizerV = new wxBoxSizer( wxVERTICAL );
	TopSizerH->Add( LeftSizerV, 0, wxGROW | wxALL, AnimCompression_ControlBorder );

	wxStaticText* ListLabel = new wxStaticText( this, -1, *LocalizeUnrealEd("CompressionAlgorithms") );
	LeftSizerV->Add( ListLabel, 0, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, AnimCompression_ControlBorder );

	AlgorithmList = new wxListBox( this, IDM_DlgAnimationCompression_AlgorithmList, wxDefaultPosition, wxSize(200, -1), 0, NULL, wxLB_SINGLE );
	LeftSizerV->Add( AlgorithmList, 1, wxGROW | wxALL, AnimCompression_ControlBorder );

	wxBoxSizer* ButtonSizerH = new wxBoxSizer( wxHORIZONTAL );
	LeftSizerV->Add( ButtonSizerH, 0, wxGROW | wxALL, AnimCompression_ControlBorder );

	ApplyToSetButton = new wxButton( this, IDM_DlgAnimationCompression_ApplyAlgorithmToSet, *LocalizeUnrealEd("ApplyToSet"), wxDefaultPosition, wxSize(-1, 30) );
	ButtonSizerH->Add( ApplyToSetButton, 1, wxGROW | wxRIGHT, AnimCompression_ControlBorder );

	ApplyToSequenceButton = new wxButton( this, IDM_DlgAnimationCompression_ApplyAlgorithmToSequence, *LocalizeUnrealEd("ApplyToSequence"), wxDefaultPosition, wxSize(-1, 30) );
	ButtonSizerH->Add( ApplyToSequenceButton, 1, wxGROW | wxLEFT, AnimCompression_ControlBorder );

	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create( this, NULL );
	TopSizerH->Add( PropertyWindow, 1, wxGROW | wxALL, AnimCompression_ControlBorder );

	FWindowUtil::LoadPosSize( TEXT("ASVAnimationCompressionDlg"), this, 100, 100, 640, 300 );

	// Enumerate animation compression algorithms.
	InitAnimCompressionAlgorithmClasses();

	// Populate the algorithm list with entries.
	UpdateAlgorithmList();
}

/**
 * Called when the window is closed.  Clears the AnimSet Viewer's reference to this dialog.
 */
void WxDlgAnimationCompression::OnClose(wxCloseEvent& In)
{
	FWindowUtil::SavePosSize( TEXT("ASVAnimationCompressionDlg"), this );	

	check( AnimSetViewer->DlgAnimationCompression == this );
	AnimSetViewer->DlgAnimationCompression = NULL;
	AnimSetViewer->ToolBar->ToggleTool( IDM_ANIMSET_OpenAnimationCompressionDlg, false );
	Destroy();
}

/** 
 * Called when an animation compression algorithm is selected from the list.
 * Sets the active algorithm and updates the property window with algorithm parameters.
 */
void WxDlgAnimationCompression::OnClickAlgorithm(wxCommandEvent& In)
{
	const INT AlgorithmIndex = AlgorithmList->GetSelection();		
	if ( AlgorithmIndex >= 0 )
	{
		SetSelectedAlgorithm( GUnrealEd->AnimationCompressionAlgorithms(AlgorithmIndex) );
	}
}

/**
 * Sets the active algorithm and updates the property window with algorithm parameters.
 */
void WxDlgAnimationCompression::SetSelectedAlgorithm(UAnimationCompressionAlgorithm* InAlgorithm)
{
	SelectedAlgorithm = InAlgorithm;
	PropertyWindow->SetObject( SelectedAlgorithm, EPropertyWindowFlags::NoFlags );
}

/** @return		TRUE if the specified object is in package which has not been cooked. */
static UBOOL InUncookedPackage(const UObject* Obj)
{
	if (GIsCooking)
	{
		if (Obj->HasAnyFlags(RF_MarkedByCooker))
		{
			return FALSE;
		}
		return TRUE;
	}

	const UPackage* Package = Obj->GetOutermost();
	if( Package->PackageFlags & PKG_Cooked )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
		return FALSE;
	}
	return TRUE;
}

/**
 * Applies the active algorithm to the selected set.
 */
void WxDlgAnimationCompression::OnApplyAlgorithmToSet(wxCommandEvent& In)
{
	if ( SelectedAlgorithm && AnimSetViewer->SelectedAnimSet )
	{
		const UBOOL bProceed =
			InUncookedPackage( AnimSetViewer->SelectedAnimSet )
			&& appMsgf( AMT_YesNo, LocalizeSecure(LocalizeUnrealEd(TEXT("AboutToCompressAnimations_F")), AnimSetViewer->SelectedAnimSet->Sequences.Num()) );
		if ( bProceed )
		{
			const FScopedBusyCursor BusyCursor;
			SelectedAlgorithm->Reduce( AnimSetViewer->SelectedAnimSet, AnimSetViewer->SelectedSkelMesh, TRUE );
			AnimSetViewer->UpdateStatusBar();
		}
	}
}

/**
 * Applies the active algorithm to the selected sequence.
 */
void WxDlgAnimationCompression::OnApplyAlgorithmToSequence(wxCommandEvent& In)
{
	if ( SelectedAlgorithm && AnimSetViewer->SelectedAnimSeq )
	{
		if ( InUncookedPackage( AnimSetViewer->SelectedAnimSeq ) )
		{
			const FScopedBusyCursor BusyCursor;
			SelectedAlgorithm->Reduce( AnimSetViewer->SelectedAnimSeq, AnimSetViewer->SelectedSkelMesh, TRUE );
			AnimSetViewer->UpdateStatusBar();
		}
	}
}

/**
 * Populates the algorithm list with the set of instanced algorithms.
 */
void WxDlgAnimationCompression::UpdateAlgorithmList()
{
	// Remember selection.
	const INT CurrentSelection = AlgorithmList->GetSelection();

	// Populate the list.
	AlgorithmList->Clear();
	for( INT i = 0 ; i < GUnrealEd->AnimationCompressionAlgorithms.Num() ; ++i )
	{
		UAnimationCompressionAlgorithm* Alg = GUnrealEd->AnimationCompressionAlgorithms(i);
		AlgorithmList->Append( *Alg->Description );
	}

	// Restore selection.
	if( CurrentSelection >= 0 && CurrentSelection < GUnrealEd->AnimationCompressionAlgorithms.Num() )
	{
		AlgorithmList->SetSelection(CurrentSelection);
	}
}

/**
 * Populates the AnimationCompressionAlgorithms list with instances of each UAnimationCompresionAlgorithm-derived class.
 */
void WxDlgAnimationCompression::InitAnimCompressionAlgorithmClasses()
{
	if ( GUnrealEd->AnimationCompressionAlgorithms.Num() == 0 )
	{
		for ( TObjectIterator<UClass> It ; It ; ++It )
		{
			UClass* Class = *It;
			if( !(Class->ClassFlags & CLASS_Abstract) && !(Class->ClassFlags & CLASS_Deprecated) )
			{
				if ( Class->IsChildOf(UAnimationCompressionAlgorithm::StaticClass()) )
				{
					UAnimationCompressionAlgorithm* NewAlgorithm = ConstructObject<UAnimationCompressionAlgorithm>( Class );
					GUnrealEd->AnimationCompressionAlgorithms.AddItem( NewAlgorithm );
				}
			}
		}
	}
}
