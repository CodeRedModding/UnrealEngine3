/*=============================================================================
	AnimAimOffsetEditor.cpp: Special editor handling for AnimNodeAimOffset
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "EngineAnimClasses.h"
#include "UnLinkedObjDrawUtils.h"
#include "AnimTreeEditor.h"
#include "AnimAimOffsetEditor.h"
#include "DlgGenericComboEntry.h"
#include "FConfigCacheIni.h"

IMPLEMENT_CLASS(UAnimNodeEditInfo_AimOffset);

//////////////////////////////////////////////////////////////////////////
// UAnimNodeEditInfo_AimOffset
//////////////////////////////////////////////////////////////////////////

void UAnimNodeEditInfo_AimOffset::OnDoubleClickNode(UAnimNode* InNode, class WxAnimTreeEditor* InEditor)
{
	// Cast and remeber the node we are working on.
	EditNode = CastChecked<UAnimNodeAimOffset>(InNode);
	EditNode->bForceAimDir = TRUE;

	// If no window exists, create one now.
	if( EditWindow == NULL )
	{
		EditWindow = new WxAnimAimOffsetEditor(InEditor, -1, this);
		EditWindow->SetSize(610,480);
		EditWindow->Show();
	}
}

void UAnimNodeEditInfo_AimOffset::OnCloseAnimTreeEditor()
{
	if(EditWindow)
	{
		EditWindow->Close();
	}
}


UBOOL UAnimNodeEditInfo_AimOffset::ShouldDrawWidget()
{
	if( EditWindow && EditNode && EditNode->CurrentProfileIndex < EditNode->Profiles.Num() )		
	{
		FAimOffsetProfile& P = EditNode->Profiles( EditNode->CurrentProfileIndex );

		if( EditWindow->EditBoneIndex < static_cast<UINT>(P.AimComponents.Num()) )
		{
			return TRUE;
		}
	}

	return FALSE;
}

UBOOL UAnimNodeEditInfo_AimOffset::IsRotationWidget()
{
	if( EditWindow && EditWindow->MoveMode == AOE_Rotate )
	{
		return TRUE;
	}
	else 
	{
		return FALSE;
	}
}

FMatrix UAnimNodeEditInfo_AimOffset::GetWidgetTM()
{
	if( EditWindow && EditNode && EditNode->CurrentProfileIndex < EditNode->Profiles.Num() )
	{
		FAimOffsetProfile& P = EditNode->Profiles( EditNode->CurrentProfileIndex );

		if( EditWindow->EditBoneIndex < static_cast<UINT>(P.AimComponents.Num()) )
		{
			USkeletalMeshComponent* SkelComp = EditWindow->AnimTreeEditor->PreviewSkelComp;
			check(SkelComp);

			// Get the name of the bone that is selected.
			const FName EditBoneName = P.AimComponents(EditWindow->EditBoneIndex).BoneName;

			if( SkelComp && SkelComp->SkeletalMesh )
			{
				// Find its index in the current skeletal mesh
				const INT BoneIndex = SkelComp->SkeletalMesh->MatchRefBone(EditBoneName);

				if( BoneIndex != INDEX_NONE )
				{
					// Then make a transform for this bone.
					FMatrix WorldSpaceTM = SkelComp->GetBoneMatrix(BoneIndex);

					FMatrix WidgetTM;
					if(EditWindow->bWorldSpaceWidget)
					{
						WidgetTM = FMatrix::Identity;
					}
					else
					{
						WidgetTM = SkelComp->GetBoneMatrix(BoneIndex);
					}

					// But set origin of widet to be on the bone.
					WidgetTM.SetOrigin( WorldSpaceTM.GetOrigin() );
					return WidgetTM;
				}
			}
		}
	}

	// Shouldn't really get here...
	return FMatrix::Identity;
}


void UAnimNodeEditInfo_AimOffset::HandleWidgetDrag(const FQuat& DeltaQuat, const FVector& DeltaTranslate)
{
	if( EditWindow && EditNode && EditNode->CurrentProfileIndex < EditNode->Profiles.Num() )
	{
		FAimOffsetProfile& P = EditNode->Profiles( EditNode->CurrentProfileIndex );

		if( EditWindow->EditBoneIndex < static_cast<UINT>(P.AimComponents.Num()) )
		{
			USkeletalMeshComponent* SkelComp = EditWindow->AnimTreeEditor->PreviewSkelComp;
			check(SkelComp);

			// Make component rotation matrix (component -> world)
			FMatrix ComponentRot = SkelComp->LocalToWorld;
			ComponentRot.SetOrigin( FVector(0.f) );

			if( EditWindow->MoveMode == AOE_Rotate )
			{
				// Get current control quaternion.
				FQuat	CurrentQuat = EditNode->GetBoneAimQuaternion(EditWindow->EditBoneIndex, EditWindow->EditDir);;

				// Apply delta rotation.
				FQuatRotationTranslationMatrix DeltaTM( DeltaQuat, FVector(0,0,0) );
				FQuat NewQuat = FQuat(ComponentRot * DeltaTM * ComponentRot.Inverse()) * CurrentQuat;

				EditNode->SetBoneAimQuaternion(EditWindow->EditBoneIndex, EditWindow->EditDir, NewQuat);
			}
			else
			{
				FVector CurrentTrans = EditNode->GetBoneAimTranslation(EditWindow->EditBoneIndex, EditWindow->EditDir);
				FTranslationMatrix CurrentTM(CurrentTrans);

				FTranslationMatrix DeltaTM( DeltaTranslate );
				FMatrix NewTranslateTM = (ComponentRot * DeltaTM * ComponentRot.Inverse()) * CurrentTM;

				EditNode->SetBoneAimTranslation(EditWindow->EditBoneIndex, EditWindow->EditDir, NewTranslateTM.GetOrigin());
			}

			// Update the text entry boxes in the tool.
			EditWindow->UpdateTextEntry();
		}
	}
}

void UAnimNodeEditInfo_AimOffset::Draw3DInfo(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{

}

//////////////////////////////////////////////////////////////////////////
// WxAnimAimOffsetEditor
//////////////////////////////////////////////////////////////////////////


BEGIN_EVENT_TABLE( WxAnimAimOffsetEditor, WxTrackableFrame )
	EVT_CLOSE( WxAnimAimOffsetEditor::OnClose )
	//EVT_SET_FOCUS( WxAnimAimOffsetEditor::OnGetFocus )
	//EVT_KILL_FOCUS( WxAnimAimOffsetEditor::OnLoseFocus )
	EVT_TOGGLEBUTTON( ID_AOE_LEFTUP, WxAnimAimOffsetEditor::OnDirButton )
	EVT_TOGGLEBUTTON( ID_AOE_CENTERUP, WxAnimAimOffsetEditor::OnDirButton )
	EVT_TOGGLEBUTTON( ID_AOE_RIGHTUP, WxAnimAimOffsetEditor::OnDirButton )
	EVT_TOGGLEBUTTON( ID_AOE_LEFTCENTER, WxAnimAimOffsetEditor::OnDirButton )
	EVT_TOGGLEBUTTON( ID_AOE_CENTERCENTER, WxAnimAimOffsetEditor::OnDirButton )
	EVT_TOGGLEBUTTON( ID_AOE_RIGHTCENTER, WxAnimAimOffsetEditor::OnDirButton )
	EVT_TOGGLEBUTTON( ID_AOE_LEFTDOWN, WxAnimAimOffsetEditor::OnDirButton )
	EVT_TOGGLEBUTTON( ID_AOE_CENTERDOWN, WxAnimAimOffsetEditor::OnDirButton )
	EVT_TOGGLEBUTTON( ID_AOE_RIGHTDOWN, WxAnimAimOffsetEditor::OnDirButton )
	EVT_CHECKBOX( ID_AOE_FORCENODE, WxAnimAimOffsetEditor::OnForceNode )
	EVT_TEXT_ENTER( ID_AOE_XENTRY, WxAnimAimOffsetEditor::OnDirEntry )
	EVT_TEXT_ENTER( ID_AOE_YENTRY, WxAnimAimOffsetEditor::OnDirEntry )
	EVT_TEXT_ENTER( ID_AOE_ZENTRY, WxAnimAimOffsetEditor::OnDirEntry )
	EVT_CHECKBOX( ID_AOE_WORLDSPACEWIDGET, WxAnimAimOffsetEditor::OnWorldSpaceWidget )
	EVT_LISTBOX( ID_AOE_BONELIST, WxAnimAimOffsetEditor::OnSelectBone )
	EVT_BUTTON( ID_AOE_ADDBONE, WxAnimAimOffsetEditor::OnAddBone )
	EVT_BUTTON( ID_AOE_REMOVEBONE, WxAnimAimOffsetEditor::OnRemoveBone )
	EVT_TOOL( ID_AOE_ROTATE, WxAnimAimOffsetEditor::OnMoveMode )
	EVT_TOOL( ID_AOE_TRANSLATE, WxAnimAimOffsetEditor::OnMoveMode )
	EVT_TOOL( ID_AOE_LOAD, WxAnimAimOffsetEditor::OnLoadProfile )
	EVT_TOOL( ID_AOE_SAVE, WxAnimAimOffsetEditor::OnSaveProfile )
	EVT_COMBOBOX( ID_AOE_PROFILECOMBO, WxAnimAimOffsetEditor::OnChangeProfile )
	EVT_BUTTON( ID_AOE_PROFILENEW, WxAnimAimOffsetEditor::OnNewProfile )
	EVT_BUTTON( ID_AOE_PROFILEDELETE, WxAnimAimOffsetEditor::OnDelProfile )
END_EVENT_TABLE()

WxAnimAimOffsetEditor::WxAnimAimOffsetEditor( WxAnimTreeEditor* InEditor, wxWindowID InID, UAnimNodeEditInfo_AimOffset* InEditInfo )
: WxTrackableFrame( InEditor, InID, TEXT("AimOffset Editor"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR )
{
	AnimTreeEditor = InEditor;
	EditInfo = InEditInfo;
	check(EditInfo->EditNode);

	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));

	wxBoxSizer* itemBoxSizer2 = new wxBoxSizer(wxVERTICAL);
	this->SetSizer(itemBoxSizer2);
	this->SetAutoLayout(true);

	wxBoxSizer* itemBoxSizer3 = new wxBoxSizer(wxHORIZONTAL);
	itemBoxSizer2->Add(itemBoxSizer3, 1, wxGROW|wxALL, 5);

	wxBoxSizer* itemBoxSizer4 = new wxBoxSizer(wxVERTICAL);
	itemBoxSizer3->Add(itemBoxSizer4, 0, wxGROW|wxALL, 0);

	//////////////////////////////////////////////////////////////////////////
	// PROFILE COMBO
	wxStaticBox* itemStaticBoxSizer1Static = new wxStaticBox(this, wxID_ANY, TEXT("Profile"));
	wxStaticBoxSizer* itemStaticBoxSizer1 = new wxStaticBoxSizer(itemStaticBoxSizer1Static, wxVERTICAL);
	itemBoxSizer4->Add(itemStaticBoxSizer1, 0, wxEXPAND|wxALL, 5);

	wxBoxSizer* profileSizer = new wxBoxSizer(wxHORIZONTAL);
	itemStaticBoxSizer1->Add(profileSizer, 1, wxGROW|wxALL, 0);

	wxStaticText* profileStaticText = new wxStaticText( this, wxID_STATIC, TEXT("Profile:"), wxDefaultPosition, wxDefaultSize, 0 );
	profileSizer->Add(profileStaticText, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	ProfileCombo = new WxComboBox(this, ID_AOE_PROFILECOMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, NULL, wxCB_READONLY);
	profileSizer->Add(ProfileCombo, 1, wxGROW|wxALL, 5);

	wxButton* NewProfileButton = new wxButton( this, ID_AOE_PROFILENEW, TEXT("New"), wxDefaultPosition, wxDefaultSize, 0 );
	profileSizer->Add(NewProfileButton, 0, wxALL, 5 );

	wxButton* DelProfileButton = new wxButton( this, ID_AOE_PROFILEDELETE, TEXT("Delete"), wxDefaultPosition, wxDefaultSize, 0 );
	profileSizer->Add(DelProfileButton, 0, wxALL, 5 );

	//////////////////////////////////////////////////////////////////////////
	// AIM DIRECTION BUTTONS
	wxStaticBox* itemStaticBoxSizer5Static = new wxStaticBox(this, wxID_ANY, TEXT("Aim Direction"));
	wxStaticBoxSizer* itemStaticBoxSizer5 = new wxStaticBoxSizer(itemStaticBoxSizer5Static, wxVERTICAL);
	itemBoxSizer4->Add(itemStaticBoxSizer5, 0, wxEXPAND|wxALL, 5);

	wxGridSizer* itemGridSizer6 = new wxGridSizer(3, 3, 0, 0);
	itemStaticBoxSizer5->Add(itemGridSizer6, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

	LUButton = new wxToggleButton( this, ID_AOE_LEFTUP, TEXT("LeftUp"), wxDefaultPosition, wxDefaultSize, 0 );
	itemGridSizer6->Add(LUButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	CUButton = new wxToggleButton( this, ID_AOE_CENTERUP, TEXT("CenterUp"), wxDefaultPosition, wxDefaultSize, 0 );
	itemGridSizer6->Add(CUButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	RUButton = new wxToggleButton( this, ID_AOE_RIGHTUP, TEXT("RightUp"), wxDefaultPosition, wxDefaultSize, 0 );
	itemGridSizer6->Add(RUButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	LCButton = new wxToggleButton( this, ID_AOE_LEFTCENTER, TEXT("LeftCenter"), wxDefaultPosition, wxDefaultSize, 0 );
	itemGridSizer6->Add(LCButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	CCButton = new wxToggleButton( this, ID_AOE_CENTERCENTER, TEXT("CenterCenter"), wxDefaultPosition, wxDefaultSize, 0 );
	itemGridSizer6->Add(CCButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	RCButton = new wxToggleButton( this, ID_AOE_RIGHTCENTER, TEXT("RightCenter"), wxDefaultPosition, wxDefaultSize, 0 );
	itemGridSizer6->Add(RCButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	LDButton = new wxToggleButton( this, ID_AOE_LEFTDOWN, TEXT("LeftDown"), wxDefaultPosition, wxDefaultSize, 0 );
	itemGridSizer6->Add(LDButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	CDButton = new wxToggleButton( this, ID_AOE_CENTERDOWN, TEXT("CenterDown"), wxDefaultPosition, wxDefaultSize, 0 );
	itemGridSizer6->Add(CDButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	RDButton = new wxToggleButton( this, ID_AOE_RIGHTDOWN, TEXT("RightDown"), wxDefaultPosition, wxDefaultSize, 0 );
	itemGridSizer6->Add(RDButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	wxFlexGridSizer* itemFlexGridSizer16 = new wxFlexGridSizer(2, 2, 0, 0);
	itemFlexGridSizer16->AddGrowableCol(1);
	itemStaticBoxSizer5->Add(itemFlexGridSizer16, 0, wxGROW|wxALL, 5);

	wxStaticText* itemStaticText19 = new wxStaticText( this, wxID_STATIC, TEXT("Force Selected Aim:"), wxDefaultPosition, wxDefaultSize, 0 );
	itemFlexGridSizer16->Add(itemStaticText19, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

	ForceNodeCheck = new wxCheckBox( this, ID_AOE_FORCENODE, TEXT(""), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	ForceNodeCheck->SetValue(TRUE);
	itemFlexGridSizer16->Add(ForceNodeCheck, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL, 5);


	//////////////////////////////////////////////////////////////////////////
	// BONE ROTATION ENTRY

	wxStaticBox* itemStaticBoxSizer16Static = new wxStaticBox(this, wxID_ANY, TEXT("Bone Rotation"));
	wxStaticBoxSizer* itemStaticBoxSizer16 = new wxStaticBoxSizer(itemStaticBoxSizer16Static, wxVERTICAL);
	itemBoxSizer4->Add(itemStaticBoxSizer16, 1, wxEXPAND|wxALL, 5);

	wxFlexGridSizer* itemFlexGridSizer17 = new wxFlexGridSizer(3, 2, 0, 0);
	itemFlexGridSizer17->AddGrowableCol(1);
	itemStaticBoxSizer16->Add(itemFlexGridSizer17, 0, wxEXPAND|wxALL, 5);

	// X ENTRY
	wxStaticText* itemStaticText18 = new wxStaticText( this, wxID_STATIC, TEXT("X"), wxDefaultPosition, wxDefaultSize, 0 );
	itemFlexGridSizer17->Add(itemStaticText18, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

	XEntry = new wxTextCtrl( this, ID_AOE_XENTRY, TEXT(""), wxDefaultPosition, wxDefaultSize, 0 );
	itemFlexGridSizer17->Add(XEntry, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// Y ENTRY
	wxStaticText* itemStaticText20 = new wxStaticText( this, wxID_STATIC, TEXT("Y"), wxDefaultPosition, wxDefaultSize, 0 );
	itemFlexGridSizer17->Add(itemStaticText20, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

	YEntry = new wxTextCtrl( this, ID_AOE_YENTRY, TEXT(""), wxDefaultPosition, wxDefaultSize, 0 );
	itemFlexGridSizer17->Add(YEntry, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// Z ENTRY
	wxStaticText* itemStaticText22 = new wxStaticText( this, wxID_STATIC, TEXT("Z"), wxDefaultPosition, wxDefaultSize, 0 );
	itemFlexGridSizer17->Add(itemStaticText22, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

	ZEntry = new wxTextCtrl( this, ID_AOE_ZENTRY, TEXT(""), wxDefaultPosition, wxDefaultSize, 0 );
	itemFlexGridSizer17->Add(ZEntry, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// WIDGET SPACE
	wxStaticText* itemStaticText23 = new wxStaticText( this, wxID_STATIC, TEXT("World Space Widget"), wxDefaultPosition, wxDefaultSize, 0 );
	itemFlexGridSizer17->Add(itemStaticText23, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

	WorldSpaceWidgetCheck = new wxCheckBox( this, ID_AOE_WORLDSPACEWIDGET, TEXT(""), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	WorldSpaceWidgetCheck->SetValue(TRUE);
	itemFlexGridSizer17->Add(WorldSpaceWidgetCheck, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);


	//////////////////////////////////////////////////////////////////////////
	// BONE LIST

	wxStaticBox* itemStaticBoxSizer24Static = new wxStaticBox(this, wxID_ANY, TEXT("Bones"));
	wxStaticBoxSizer* itemStaticBoxSizer24 = new wxStaticBoxSizer(itemStaticBoxSizer24Static, wxVERTICAL);
	itemBoxSizer3->Add(itemStaticBoxSizer24, 1, wxGROW|wxALL, 5);

	wxString* itemListBox25Strings = NULL;
	BoneList = new wxListBox( this, ID_AOE_BONELIST, wxDefaultPosition, wxSize(200, -1), 0, itemListBox25Strings, wxLB_SINGLE );
	itemStaticBoxSizer24->Add(BoneList, 1, wxGROW|wxALL, 5);

	wxButton* itemButton26 = new wxButton( this, ID_AOE_ADDBONE, TEXT("Add Bone"), wxDefaultPosition, wxDefaultSize, 0 );
	itemStaticBoxSizer24->Add(itemButton26, 0, wxGROW|wxALL, 5);

	wxButton* itemButton27 = new wxButton( this, ID_AOE_REMOVEBONE, TEXT("Remove Bone"), wxDefaultPosition, wxDefaultSize, 0 );
	itemStaticBoxSizer24->Add(itemButton27, 0, wxGROW|wxALL, 5);

	// Toolbar
	ToolBar = new WxAnimAimOffsetToolBar( this, -1 );
	SetToolBar( ToolBar );

	MoveMode = AOE_Rotate;
	ToolBar->ToggleTool(ID_AOE_TRANSLATE, FALSE);
	ToolBar->ToggleTool(ID_AOE_ROTATE, TRUE);

	// Fill in combo and set to currently active profile.
	UpdateProfileCombo();
	ProfileCombo->SetSelection(EditInfo->EditNode->CurrentProfileIndex);

	// Init various UI bits
	UpdateBoneList();
	EditBoneIndex = 0;
	if(EditBoneIndex < BoneList->GetCount())
	{
		BoneList->SetSelection(0);
	}

	EditDir = ANIMAIM_CENTERCENTER;
	CCButton->SetValue(TRUE);

	bWorldSpaceWidget = TRUE;

	UpdateTextEntry();
}

WxAnimAimOffsetEditor::~WxAnimAimOffsetEditor()
{

}

/**
 * This function is called when the window has been selected from within the ctrl + tab dialog.
 */
void WxAnimAimOffsetEditor::OnSelected()
{
	Raise();
}

//////////////////////////////////////////////////////////////////////////
// Control handling

void WxAnimAimOffsetEditor::OnClose(wxCloseEvent& In)
{
	check(EditInfo->EditWindow == this);
	EditInfo->EditWindow = NULL;

	EditInfo->EditNode->bForceAimDir = FALSE;
	EditInfo->EditNode = NULL;

	EditInfo = NULL;

	this->Destroy();
}



void WxAnimAimOffsetEditor::OnDirButton( wxCommandEvent& In )
{
	// Change EditDir based on which button was pressed.
	INT Id = In.GetId();
	if(Id == ID_AOE_LEFTUP)
	{
		EditDir = ANIMAIM_LEFTUP;
	}
	else if(Id == ID_AOE_CENTERUP)
	{
		EditDir = ANIMAIM_CENTERUP;
	}
	else if(Id == ID_AOE_RIGHTUP)
	{
		EditDir = ANIMAIM_RIGHTUP;
	}
	else if(Id == ID_AOE_LEFTCENTER)
	{
		EditDir = ANIMAIM_LEFTCENTER;
	}
	else if(Id == ID_AOE_CENTERCENTER)
	{
		EditDir = ANIMAIM_CENTERCENTER;
	}
	else if(Id == ID_AOE_RIGHTCENTER)
	{
		EditDir = ANIMAIM_RIGHTCENTER;
	}
	else if(Id == ID_AOE_LEFTDOWN)
	{
		EditDir = ANIMAIM_LEFTDOWN;
	}
	else if(Id == ID_AOE_CENTERDOWN)
	{
		EditDir = ANIMAIM_CENTERDOWN;
	}
	else if(Id == ID_AOE_RIGHTDOWN)
	{
		EditDir = ANIMAIM_RIGHTDOWN;
	}
	else
	{
		assert(0);
	}

	// Pop up all the buttons apart from the current direction.
	UncheckAllDirButtonExcept(EditDir);

	// Force the AnimNode to point the way.
	EditInfo->EditNode->ForcedAimDir = EditDir;

	// Then update the text entry boxes for the current direction.
	UpdateTextEntry();
}

void WxAnimAimOffsetEditor::OnDirEntry( wxCommandEvent& In )
{
	// Grab the text and process it.
	ProcessTextEntry();

	// Select the text in the box so we can just type again.
	INT Id = In.GetId();
	if(Id == ID_AOE_XENTRY)
	{
		XEntry->SetSelection(-1,-1);
	}
	else if(Id == ID_AOE_YENTRY)
	{
		YEntry->SetSelection(-1,-1);
	}
	else if(Id == ID_AOE_ZENTRY)
	{
		ZEntry->SetSelection(-1,-1);
	}
}

#if 0 // These never seem to get called.. would be nice though!
void WxAnimAimOffsetEditor::OnGetFocus( wxFocusEvent& In )
{
	wxWindow* Ctrl = (wxWindow*)In.GetEventObject();

	// Select the text in the box so we can just type again.
	if(Ctrl == XEntry)
	{
		XEntry->SetSelection(-1,-1);
	}
	else if(Ctrl == YEntry)
	{
		YEntry->SetSelection(-1,-1);
	}
	else if(Ctrl == ZEntry)
	{
		ZEntry->SetSelection(-1,-1);
	}

	In.Skip();
}

void WxAnimAimOffsetEditor::OnLoseFocus( wxFocusEvent& In )
{
	wxWindow* Ctrl = (wxWindow*)In.GetEventObject();
	if(Ctrl == XEntry || Ctrl == YEntry || Ctrl == ZEntry)
	{
		ProcessTextEntry();
	}

	In.Skip();
}
#endif

void WxAnimAimOffsetEditor::OnWorldSpaceWidget( wxCommandEvent& In )
{
	bWorldSpaceWidget = In.IsChecked();
}

void WxAnimAimOffsetEditor::OnForceNode( wxCommandEvent& In )
{
	// Toggle the bForceAimDir flag, to choose whether to use the 'Aim' or force a particular pose.
	EditInfo->EditNode->bForceAimDir = In.IsChecked();
}

void WxAnimAimOffsetEditor::OnAddBone( wxCommandEvent& In )
{
	// If not a valid profile - do nothing.
	if(EditInfo->EditNode->CurrentProfileIndex >= EditInfo->EditNode->Profiles.Num())
	{
		return;
	}

	// Get the SkeletalMesh that we are operating on.
	USkeletalMesh* SkelMesh = AnimTreeEditor->PreviewSkelComp->SkeletalMesh;
	if( !SkelMesh )
	{
		return;
	}

	// Then make a list of all the bones in the skeleton, which we dont already have.
	TArray<FString> BoneNames;
	for(INT i=0; i<SkelMesh->RefSkeleton.Num(); i++)
	{
		FName BoneName = SkelMesh->RefSkeleton(i).Name;
		if( !EditInfo->EditNode->ContainsBone(BoneName) )
		{
			BoneNames.AddItem(BoneName.ToString());
		}
	}

	// Pop up a combo to let them choose the new bone to modify.
	WxDlgGenericComboEntry BoneDlg;
	if( BoneDlg.ShowModal( TEXT("NewBone"), TEXT("BoneName"), BoneNames, 0, TRUE ) == wxID_OK )
	{
		FName BoneName = FName( *BoneDlg.GetSelectedString() );
		INT BoneIndex = SkelMesh->MatchRefBone(BoneName);

		// If its a valid bone we want to add..
		if( BoneName != NAME_None && BoneIndex != INDEX_NONE )
		{
			INT InsertPos = INDEX_NONE;
			FAimOffsetProfile& P = EditInfo->EditNode->Profiles( EditInfo->EditNode->CurrentProfileIndex );

			// Iterate through current array, to find place to insert this new Bone so they stay in Bone index order.
			for(INT i=0; i<P.AimComponents.Num() && InsertPos == INDEX_NONE; i++)
			{
				FName	TestName	= P.AimComponents(i).BoneName;
				INT		TestIndex	= SkelMesh->MatchRefBone(TestName);

				if( TestIndex != INDEX_NONE && 
					TestIndex > BoneIndex )
				{
					InsertPos = i;
				}
			}

			// If we didn't find insert position - insert at end.
			// This also handles case of and empty BoneNames array.
			if( InsertPos == INDEX_NONE )
			{
				InsertPos = P.AimComponents.Num();
			}

			// If we found a valid insertion position.
			if( InsertPos != INDEX_NONE )
			{
				// Add a new component.
				P.AimComponents.InsertZeroed(InsertPos);

				// Set correct name and index.
				P.AimComponents(InsertPos).BoneName = BoneName;

				// Initialize Quaternions - InsertZeroed doesn't set them to Identity
				EditInfo->EditNode->SetBoneAimQuaternion(InsertPos, ANIMAIM_LEFTUP,			FQuat::Identity);
				EditInfo->EditNode->SetBoneAimQuaternion(InsertPos, ANIMAIM_CENTERUP,		FQuat::Identity);
				EditInfo->EditNode->SetBoneAimQuaternion(InsertPos, ANIMAIM_RIGHTUP,		FQuat::Identity);

				EditInfo->EditNode->SetBoneAimQuaternion(InsertPos, ANIMAIM_LEFTCENTER,		FQuat::Identity);
				EditInfo->EditNode->SetBoneAimQuaternion(InsertPos, ANIMAIM_CENTERCENTER,	FQuat::Identity);
				EditInfo->EditNode->SetBoneAimQuaternion(InsertPos, ANIMAIM_RIGHTCENTER,	FQuat::Identity);

				EditInfo->EditNode->SetBoneAimQuaternion(InsertPos, ANIMAIM_LEFTDOWN,		FQuat::Identity);
				EditInfo->EditNode->SetBoneAimQuaternion(InsertPos, ANIMAIM_CENTERDOWN,		FQuat::Identity);
				EditInfo->EditNode->SetBoneAimQuaternion(InsertPos, ANIMAIM_RIGHTDOWN,		FQuat::Identity);

				EditInfo->EditNode->UpdateListOfRequiredBones();

				// Now update the bone list
				UpdateBoneList();

				// Set the selection to the new one.
				EditBoneIndex = InsertPos;
				BoneList->SetSelection(InsertPos);
				UpdateTextEntry();
			}
		}
	}
}

void WxAnimAimOffsetEditor::OnRemoveBone( wxCommandEvent& In )
{
	// If not a valid profile - do nothing.
	if(EditInfo->EditNode->CurrentProfileIndex >= EditInfo->EditNode->Profiles.Num())
	{
		return;
	}

	FAimOffsetProfile& P = EditInfo->EditNode->Profiles( EditInfo->EditNode->CurrentProfileIndex );

	if( EditBoneIndex < static_cast<UINT>(P.AimComponents.Num()) )
	{
		// Remove AimComponent
		P.AimComponents.Remove(EditBoneIndex);
		EditInfo->EditNode->UpdateListOfRequiredBones();

		// Refresh bone list box.
		UpdateBoneList();

		// Update current selection.
		if( EditBoneIndex > 0 )
		{
			EditBoneIndex--;
		}

		if( EditBoneIndex < BoneList->GetCount() )
		{
			BoneList->SetSelection(EditBoneIndex);
		}

		UpdateTextEntry();
	}
}

void WxAnimAimOffsetEditor::OnSelectBone( wxCommandEvent& In )
{
	// If not a valid profile - do nothing.
	if(EditInfo->EditNode->CurrentProfileIndex >= EditInfo->EditNode->Profiles.Num())
	{
		return;
	}

	FAimOffsetProfile& P = EditInfo->EditNode->Profiles( EditInfo->EditNode->CurrentProfileIndex );

	// Set the internal bone index to the one from the list.
	EditBoneIndex = BoneList->GetSelection();
	check(EditBoneIndex >= 0);
	check(EditBoneIndex < static_cast<UINT>(P.AimComponents.Num()));

	// Update rotation entry boxes with rotation for this bone in current pose.
	UpdateTextEntry();
}

void WxAnimAimOffsetEditor::OnMoveMode( wxCommandEvent& In )
{
	if( In.GetId() == ID_AOE_ROTATE )
	{
		MoveMode = AOE_Rotate;
	}
	else if( In.GetId() == ID_AOE_TRANSLATE )
	{
		MoveMode = AOE_Translate;
	}

	// Update to show translation/rotation value.
	UpdateTextEntry();
}

void WxAnimAimOffsetEditor::OnLoadProfile( wxCommandEvent& In )
{
	WxFileDialog dlg( GApp->EditorFrame, *LocalizeUnrealEd("Open"), *GApp->LastDir[LD_GENERIC_SAVE], TEXT(""), TEXT("AimOffsetProfile files (*.aop)|*.aop|All files (*.*)|*.*"), wxOPEN | wxFILE_MUST_EXIST );
	
	if( dlg.ShowModal() != wxID_OK )
	{
		return;
	}

	FString FileName(dlg.GetPath());
	FConfigCacheIni* ConfigCache = static_cast<FConfigCacheIni*>(GConfig);

	// Load Existing file
	FConfigFile* LoadFile = ConfigCache->Find(*FileName, FALSE);

	// Abort is file couldn't be loaded
	if( !LoadFile )
	{
		appMsgf(AMT_OK, TEXT("Couldn't find file %s."), *FileName);
		return;
	}

	// Profile section
	const TCHAR*	ProfileSectionName = TEXT("Profile");

	// Get profile name we're importing
	FString ImportedProfileNameStr = GConfig->GetStr(ProfileSectionName, TEXT("ProfileName"), *FileName);

	// See if this profile exists
	FName ImportedProfileName(*ImportedProfileNameStr);

	INT ProfileIndex = -1;
	for(INT i=0; i<EditInfo->EditNode->Profiles.Num(); i++)
	{
		// We found a matching profile
		if( EditInfo->EditNode->Profiles(i).ProfileName == ImportedProfileName )
		{
			if( appMsgf(AMT_OKCancel, TEXT("Profile %s exists, do you want to overwrite it?"), *ImportedProfileNameStr) == 0 )
			{
				// User pressed cancel... so abort right here
				ConfigCache->UnloadFile(*FileName);
				delete LoadFile;
				return;
			}

			// Otherwise, use this profile
			ProfileIndex = i;
			break;
		}
	}

	// If we haven't found an existing profile, create a new one!
	if( ProfileIndex == -1 )
	{
		ProfileIndex = EditInfo->EditNode->Profiles.AddZeroed();
	}

	// We have our profile now... we can start filling it up!
	FAimOffsetProfile& P = EditInfo->EditNode->Profiles(ProfileIndex);

	// Profile Name
	P.ProfileName = ImportedProfileName;

	// Range values
	GConfig->GetFloat(ProfileSectionName, TEXT("HorizontalRange_X"),	P.HorizontalRange.X,	*FileName);
	GConfig->GetFloat(ProfileSectionName, TEXT("HorizontalRange_Y"),	P.HorizontalRange.Y,	*FileName);
	GConfig->GetFloat(ProfileSectionName, TEXT("VerticalRange_X"),		P.VerticalRange.X,		*FileName);
	GConfig->GetFloat(ProfileSectionName, TEXT("VerticalRange_Y"),		P.VerticalRange.Y,		*FileName);

	// Anim Names
	P.AnimName_LU = FName(*GConfig->GetStr(ProfileSectionName, TEXT("AnimName_LU"), *FileName));
	P.AnimName_LC = FName(*GConfig->GetStr(ProfileSectionName, TEXT("AnimName_LC"), *FileName));
	P.AnimName_LD = FName(*GConfig->GetStr(ProfileSectionName, TEXT("AnimName_LD"), *FileName));
	P.AnimName_CU = FName(*GConfig->GetStr(ProfileSectionName, TEXT("AnimName_CU"), *FileName));
	P.AnimName_CC = FName(*GConfig->GetStr(ProfileSectionName, TEXT("AnimName_CC"), *FileName));
	P.AnimName_CD = FName(*GConfig->GetStr(ProfileSectionName, TEXT("AnimName_CD"), *FileName));
	P.AnimName_RU = FName(*GConfig->GetStr(ProfileSectionName, TEXT("AnimName_RU"), *FileName));
	P.AnimName_RC = FName(*GConfig->GetStr(ProfileSectionName, TEXT("AnimName_RC"), *FileName));
	P.AnimName_RD = FName(*GConfig->GetStr(ProfileSectionName, TEXT("AnimName_RD"), *FileName));

	// AimComponents!

	// First empty them, in case we're overwriting an existing one, and bones don't match up.
	P.AimComponents.Empty();

	// Iterate through all section names
	FConfigFile::TIterator SectionIterator(*LoadFile);
	while( SectionIterator )
	{
		// if it's not the Profile header section, then fill in AimComponent!
		if( SectionIterator.Key() != FString(ProfileSectionName) )
		{
			INT AimCompIdx = P.AimComponents.AddZeroed();
			FAimComponent& AimComp = P.AimComponents(AimCompIdx);

			// Section Name
			const TCHAR* SectionName = *SectionIterator.Key();

			// Save BoneName, dupe from section, but just in case...
			AimComp.BoneName = FName(*GConfig->GetStr(SectionName, TEXT("BoneName"), *FileName));

			GConfig->GetFloat(SectionName, TEXT("LU_Quat_W"), AimComp.LU.Quaternion.W, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LU_Quat_X"), AimComp.LU.Quaternion.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LU_Quat_Y"), AimComp.LU.Quaternion.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LU_Quat_Z"), AimComp.LU.Quaternion.Z, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LU_Trans_X"), AimComp.LU.Translation.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LU_Trans_Y"), AimComp.LU.Translation.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LU_Trans_Z"), AimComp.LU.Translation.Z, *FileName);

			GConfig->GetFloat(SectionName, TEXT("LC_Quat_W"), AimComp.LC.Quaternion.W, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LC_Quat_X"), AimComp.LC.Quaternion.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LC_Quat_Y"), AimComp.LC.Quaternion.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LC_Quat_Z"), AimComp.LC.Quaternion.Z, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LC_Trans_X"), AimComp.LC.Translation.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LC_Trans_Y"), AimComp.LC.Translation.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LC_Trans_Z"), AimComp.LC.Translation.Z, *FileName);

			GConfig->GetFloat(SectionName, TEXT("LD_Quat_W"), AimComp.LD.Quaternion.W, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LD_Quat_X"), AimComp.LD.Quaternion.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LD_Quat_Y"), AimComp.LD.Quaternion.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LD_Quat_Z"), AimComp.LD.Quaternion.Z, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LD_Trans_X"), AimComp.LD.Translation.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LD_Trans_Y"), AimComp.LD.Translation.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("LD_Trans_Z"), AimComp.LD.Translation.Z, *FileName);


			GConfig->GetFloat(SectionName, TEXT("CU_Quat_W"), AimComp.CU.Quaternion.W, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CU_Quat_X"), AimComp.CU.Quaternion.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CU_Quat_Y"), AimComp.CU.Quaternion.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CU_Quat_Z"), AimComp.CU.Quaternion.Z, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CU_Trans_X"), AimComp.CU.Translation.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CU_Trans_Y"), AimComp.CU.Translation.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CU_Trans_Z"), AimComp.CU.Translation.Z, *FileName);

			GConfig->GetFloat(SectionName, TEXT("CC_Quat_W"), AimComp.CC.Quaternion.W, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CC_Quat_X"), AimComp.CC.Quaternion.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CC_Quat_Y"), AimComp.CC.Quaternion.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CC_Quat_Z"), AimComp.CC.Quaternion.Z, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CC_Trans_X"), AimComp.CC.Translation.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CC_Trans_Y"), AimComp.CC.Translation.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CC_Trans_Z"), AimComp.CC.Translation.Z, *FileName);

			GConfig->GetFloat(SectionName, TEXT("CD_Quat_W"), AimComp.CD.Quaternion.W, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CD_Quat_X"), AimComp.CD.Quaternion.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CD_Quat_Y"), AimComp.CD.Quaternion.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CD_Quat_Z"), AimComp.CD.Quaternion.Z, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CD_Trans_X"), AimComp.CD.Translation.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CD_Trans_Y"), AimComp.CD.Translation.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("CD_Trans_Z"), AimComp.CD.Translation.Z, *FileName);


			GConfig->GetFloat(SectionName, TEXT("RU_Quat_W"), AimComp.RU.Quaternion.W, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RU_Quat_X"), AimComp.RU.Quaternion.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RU_Quat_Y"), AimComp.RU.Quaternion.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RU_Quat_Z"), AimComp.RU.Quaternion.Z, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RU_Trans_X"), AimComp.RU.Translation.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RU_Trans_Y"), AimComp.RU.Translation.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RU_Trans_Z"), AimComp.RU.Translation.Z, *FileName);

			GConfig->GetFloat(SectionName, TEXT("RC_Quat_W"), AimComp.RC.Quaternion.W, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RC_Quat_X"), AimComp.RC.Quaternion.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RC_Quat_Y"), AimComp.RC.Quaternion.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RC_Quat_Z"), AimComp.RC.Quaternion.Z, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RC_Trans_X"), AimComp.RC.Translation.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RC_Trans_Y"), AimComp.RC.Translation.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RC_Trans_Z"), AimComp.RC.Translation.Z, *FileName);

			GConfig->GetFloat(SectionName, TEXT("RD_Quat_W"), AimComp.RD.Quaternion.W, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RD_Quat_X"), AimComp.RD.Quaternion.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RD_Quat_Y"), AimComp.RD.Quaternion.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RD_Quat_Z"), AimComp.RD.Quaternion.Z, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RD_Trans_X"), AimComp.RD.Translation.X, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RD_Trans_Y"), AimComp.RD.Translation.Y, *FileName);
			GConfig->GetFloat(SectionName, TEXT("RD_Trans_Z"), AimComp.RD.Translation.Z, *FileName);
		}
		++SectionIterator;
	}

	// We're done, finished! So unload file
	ConfigCache->UnloadFile(*FileName);

	// Update required bones array, to recache bone indices.
	EditInfo->EditNode->UpdateListOfRequiredBones();

	// Select newly laoded profile
	EditInfo->EditNode->SetActiveProfileByIndex(ProfileIndex);

	// Update bone list and text entry
	UpdateBoneList();
	EditBoneIndex = 0;
	if( EditBoneIndex < BoneList->GetCount() )
	{
		BoneList->SetSelection(0);
	}
	UpdateTextEntry();

	appMsgf(AMT_OK, TEXT("Profile %s successfully imported!"), *ImportedProfileNameStr);
}


void WxAnimAimOffsetEditor::OnSaveProfile( wxCommandEvent& In )
{
	// Current Profile
	FAimOffsetProfile& P = EditInfo->EditNode->Profiles(EditInfo->EditNode->CurrentProfileIndex);

	// Make up a default name
	FString DefaultFileName = EditInfo->EditNode->NodeName.ToString() + TEXT("_") + P.ProfileName.ToString();
	WxFileDialog dlg(GApp->EditorFrame, *LocalizeUnrealEd("Save"),  *GApp->LastDir[LD_GENERIC_SAVE], *DefaultFileName, TEXT("AimOffsetProfile files (*.aop)|*.aop|All files (*.*)|*.*"), wxSAVE | wxOVERWRITE_PROMPT );

	if( dlg.ShowModal() != wxID_OK )
	{
		return;
	}

	FString FileName(dlg.GetPath());
	FConfigCacheIni* ConfigCache = static_cast<FConfigCacheIni*>(GConfig);

	// See if file already exists, then load it. Otherwise create a new one
	FConfigFile* SaveFile = ConfigCache->Find(*FileName, TRUE);

	// Make sure we have a valid save file
	check(SaveFile);

	// Empty save in case we're overwriting an existing file.
	SaveFile->Empty();

	// Profile section
	const TCHAR*	ProfileSectionName = TEXT("Profile");

	GConfig->SetString(ProfileSectionName, TEXT("ProfileName"), *P.ProfileName.GetNameString(), *FileName);

	GConfig->SetFloat(ProfileSectionName, TEXT("HorizontalRange_X"), P.HorizontalRange.X, *FileName);
	GConfig->SetFloat(ProfileSectionName, TEXT("HorizontalRange_Y"), P.HorizontalRange.Y, *FileName);
	GConfig->SetFloat(ProfileSectionName, TEXT("VerticalRange_X"), P.VerticalRange.X, *FileName);
	GConfig->SetFloat(ProfileSectionName, TEXT("VerticalRange_Y"), P.VerticalRange.Y, *FileName);

	// Anim Names
	GConfig->SetString(ProfileSectionName, TEXT("AnimName_LU"), *P.AnimName_LU.GetNameString(), *FileName);
	GConfig->SetString(ProfileSectionName, TEXT("AnimName_LC"), *P.AnimName_LC.GetNameString(), *FileName);
	GConfig->SetString(ProfileSectionName, TEXT("AnimName_LD"), *P.AnimName_LD.GetNameString(), *FileName);
	GConfig->SetString(ProfileSectionName, TEXT("AnimName_CU"), *P.AnimName_CU.GetNameString(), *FileName);
	GConfig->SetString(ProfileSectionName, TEXT("AnimName_CC"), *P.AnimName_CC.GetNameString(), *FileName);
	GConfig->SetString(ProfileSectionName, TEXT("AnimName_CD"), *P.AnimName_CD.GetNameString(), *FileName);
	GConfig->SetString(ProfileSectionName, TEXT("AnimName_RU"), *P.AnimName_RU.GetNameString(), *FileName);
	GConfig->SetString(ProfileSectionName, TEXT("AnimName_RC"), *P.AnimName_RC.GetNameString(), *FileName);
	GConfig->SetString(ProfileSectionName, TEXT("AnimName_RD"), *P.AnimName_RD.GetNameString(), *FileName);

	// Per AimComponent section
	for(INT i=0; i<P.AimComponents.Num(); i++)
	{
		FAimComponent& AimComp = P.AimComponents(i);

		// Each section is a bone name
		const FString& SectionName = AimComp.BoneName.ToString();
		
		// Save BoneName, dupe from section, but just in case...
		GConfig->SetString(*SectionName, TEXT("BoneName"), *SectionName, *FileName);

		GConfig->SetFloat(*SectionName, TEXT("LU_Quat_W"), AimComp.LU.Quaternion.W, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LU_Quat_X"), AimComp.LU.Quaternion.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LU_Quat_Y"), AimComp.LU.Quaternion.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LU_Quat_Z"), AimComp.LU.Quaternion.Z, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LU_Trans_X"), AimComp.LU.Translation.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LU_Trans_Y"), AimComp.LU.Translation.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LU_Trans_Z"), AimComp.LU.Translation.Z, *FileName);

		GConfig->SetFloat(*SectionName, TEXT("LC_Quat_W"), AimComp.LC.Quaternion.W, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LC_Quat_X"), AimComp.LC.Quaternion.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LC_Quat_Y"), AimComp.LC.Quaternion.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LC_Quat_Z"), AimComp.LC.Quaternion.Z, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LC_Trans_X"), AimComp.LC.Translation.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LC_Trans_Y"), AimComp.LC.Translation.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LC_Trans_Z"), AimComp.LC.Translation.Z, *FileName);

		GConfig->SetFloat(*SectionName, TEXT("LD_Quat_W"), AimComp.LD.Quaternion.W, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LD_Quat_X"), AimComp.LD.Quaternion.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LD_Quat_Y"), AimComp.LD.Quaternion.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LD_Quat_Z"), AimComp.LD.Quaternion.Z, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LD_Trans_X"), AimComp.LD.Translation.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LD_Trans_Y"), AimComp.LD.Translation.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("LD_Trans_Z"), AimComp.LD.Translation.Z, *FileName);


		GConfig->SetFloat(*SectionName, TEXT("CU_Quat_W"), AimComp.CU.Quaternion.W, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CU_Quat_X"), AimComp.CU.Quaternion.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CU_Quat_Y"), AimComp.CU.Quaternion.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CU_Quat_Z"), AimComp.CU.Quaternion.Z, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CU_Trans_X"), AimComp.CU.Translation.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CU_Trans_Y"), AimComp.CU.Translation.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CU_Trans_Z"), AimComp.CU.Translation.Z, *FileName);

		GConfig->SetFloat(*SectionName, TEXT("CC_Quat_W"), AimComp.CC.Quaternion.W, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CC_Quat_X"), AimComp.CC.Quaternion.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CC_Quat_Y"), AimComp.CC.Quaternion.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CC_Quat_Z"), AimComp.CC.Quaternion.Z, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CC_Trans_X"), AimComp.CC.Translation.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CC_Trans_Y"), AimComp.CC.Translation.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CC_Trans_Z"), AimComp.CC.Translation.Z, *FileName);

		GConfig->SetFloat(*SectionName, TEXT("CD_Quat_W"), AimComp.CD.Quaternion.W, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CD_Quat_X"), AimComp.CD.Quaternion.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CD_Quat_Y"), AimComp.CD.Quaternion.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CD_Quat_Z"), AimComp.CD.Quaternion.Z, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CD_Trans_X"), AimComp.CD.Translation.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CD_Trans_Y"), AimComp.CD.Translation.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("CD_Trans_Z"), AimComp.CD.Translation.Z, *FileName);


		GConfig->SetFloat(*SectionName, TEXT("RU_Quat_W"), AimComp.RU.Quaternion.W, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RU_Quat_X"), AimComp.RU.Quaternion.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RU_Quat_Y"), AimComp.RU.Quaternion.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RU_Quat_Z"), AimComp.RU.Quaternion.Z, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RU_Trans_X"), AimComp.RU.Translation.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RU_Trans_Y"), AimComp.RU.Translation.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RU_Trans_Z"), AimComp.RU.Translation.Z, *FileName);

		GConfig->SetFloat(*SectionName, TEXT("RC_Quat_W"), AimComp.RC.Quaternion.W, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RC_Quat_X"), AimComp.RC.Quaternion.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RC_Quat_Y"), AimComp.RC.Quaternion.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RC_Quat_Z"), AimComp.RC.Quaternion.Z, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RC_Trans_X"), AimComp.RC.Translation.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RC_Trans_Y"), AimComp.RC.Translation.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RC_Trans_Z"), AimComp.RC.Translation.Z, *FileName);

		GConfig->SetFloat(*SectionName, TEXT("RD_Quat_W"), AimComp.RD.Quaternion.W, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RD_Quat_X"), AimComp.RD.Quaternion.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RD_Quat_Y"), AimComp.RD.Quaternion.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RD_Quat_Z"), AimComp.RD.Quaternion.Z, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RD_Trans_X"), AimComp.RD.Translation.X, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RD_Trans_Y"), AimComp.RD.Translation.Y, *FileName);
		GConfig->SetFloat(*SectionName, TEXT("RD_Trans_Z"), AimComp.RD.Translation.Z, *FileName);
	}

	// Save everything to disk
	ConfigCache->Flush(FALSE, *FileName);

	// Unload file
	ConfigCache->UnloadFile(*FileName);
}

void WxAnimAimOffsetEditor::OnChangeProfile( wxCommandEvent& In )
{
	INT NewProfile = In.GetSelection();

	EditInfo->EditNode->SetActiveProfileByIndex(NewProfile);

	// Update bone list and text entry
	UpdateBoneList();
	EditBoneIndex = 0;
	if(EditBoneIndex < BoneList->GetCount())
	{
		BoneList->SetSelection(0);
	}
	UpdateTextEntry();
}

void WxAnimAimOffsetEditor::OnNewProfile( wxCommandEvent& In )
{
	// Pop up dialog to ask for a profile name.
	WxDlgGenericStringEntry dlg;
	INT Result = dlg.ShowModal( TEXT("AOE_EnterNewProfileName"), TEXT("AOE_ProfileName"), TEXT("") );
	if( Result == wxID_OK )
	{
		FName NewProfileName = FName(*dlg.GetEnteredString());

		// Check new name is not empty
		if(NewProfileName == NAME_None)
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("AOE_NoProfileName") );
			return;
		}

		// Check name is not a duplicate
		for(INT i=0; i<EditInfo->EditNode->Profiles.Num(); i++)
		{
			if(EditInfo->EditNode->Profiles(i).ProfileName == NewProfileName)
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("AOE_DuplicateProfileName") );
				return;
			}
		}

		// Add new profile to the array (at the end)
		INT NewProfileIndex = EditInfo->EditNode->Profiles.AddZeroed();
		FAimOffsetProfile& P = EditInfo->EditNode->Profiles(NewProfileIndex);

		// Init new profile
		P.ProfileName = NewProfileName;
		P.HorizontalRange = FVector2D(-1,1);
		P.VerticalRange = FVector2D(-1,1);

		// Switch to this profile.
		EditInfo->EditNode->SetActiveProfileByIndex(NewProfileIndex);

		// Reset precalc data
		EditInfo->EditNode->AimCpntIndexLUT.Empty();
		EditInfo->EditNode->UpdateListOfRequiredBones();

		UpdateProfileCombo();
		ProfileCombo->SetSelection(NewProfileIndex);

		// Update bone list and text entry
		UpdateBoneList();
		EditBoneIndex = 0;
		if(EditBoneIndex < BoneList->GetCount())
		{
			BoneList->SetSelection(0);
		}
		UpdateTextEntry();
	}
}

void WxAnimAimOffsetEditor::OnDelProfile( wxCommandEvent& In )
{
	// Check current profile is within range
	if( EditInfo->EditNode->CurrentProfileIndex >= EditInfo->EditNode->Profiles.Num() )
	{
		return;
	}

	// Confirm they want to delete profile.
	FAimOffsetProfile& P = EditInfo->EditNode->Profiles(EditInfo->EditNode->CurrentProfileIndex);
	UBOOL bDoIt = appMsgf( AMT_YesNo, LocalizeSecure(LocalizeUnrealEd("AOE_DelAreYouSure"), *P.ProfileName.ToString()) );
	if(bDoIt)
	{
		// Remove profile from array
		EditInfo->EditNode->Profiles.Remove( EditInfo->EditNode->CurrentProfileIndex );

		// Update UI and aim node.
		UpdateProfileCombo();

		if(EditInfo->EditNode->Profiles.Num() > 0)
		{
			ProfileCombo->SetSelection(0);
			EditInfo->EditNode->SetActiveProfileByIndex(0);
		}
		else
		{
			EditInfo->EditNode->CurrentProfileIndex = 0;
		}

		// Update bone list and text entry
		UpdateBoneList();
		EditBoneIndex = 0;
		if(EditBoneIndex < BoneList->GetCount())
		{
			BoneList->SetSelection(0);
		}
		UpdateTextEntry();
	}
}

//////////////////////////////////////////////////////////////////////////
// Utils

/** Grab text from boxes and apply to aim pose. */
void WxAnimAimOffsetEditor::ProcessTextEntry()
{
	FVector NewVal;

	// Get the value from each edit box.
	double NewNum;
	UBOOL bIsNumber = XEntry->GetValue().ToDouble(&NewNum);
	if(!bIsNumber)
		NewNum = 0.f;
	NewVal.X = NewNum;

	bIsNumber = YEntry->GetValue().ToDouble(&NewNum);
	if(!bIsNumber)
		NewNum = 0.f;
	NewVal.Y = NewNum;

	bIsNumber = ZEntry->GetValue().ToDouble(&NewNum);
	if(!bIsNumber)
		NewNum = 0.f;
	NewVal.Z = NewNum;

	if( MoveMode == AOE_Rotate )
	{
		// Convert from degrees to Quaterion
		FQuat NewQuat = FQuat::MakeFromEuler(NewVal);

		// And set for current bone/pose.
		EditInfo->EditNode->SetBoneAimQuaternion(EditBoneIndex, EditDir, NewQuat);
	}
	else
	{
		// Set translation for current bone/pose
		EditInfo->EditNode->SetBoneAimTranslation(EditBoneIndex, EditDir, NewVal);
	}
}


void WxAnimAimOffsetEditor::UpdateBoneList()
{
	BoneList->Clear();

	USkeletalMeshComponent* SkelComp = AnimTreeEditor->PreviewSkelComp;

	if( !SkelComp || !SkelComp->SkeletalMesh )
	{
		return;
	}

	if(EditInfo->EditNode->CurrentProfileIndex < EditInfo->EditNode->Profiles.Num())
	{
		FAimOffsetProfile& P = EditInfo->EditNode->Profiles( EditInfo->EditNode->CurrentProfileIndex );

		for(INT i=0; i<P.AimComponents.Num(); i++)
		{
			FName	BoneName	= P.AimComponents(i).BoneName;
			INT		BoneIndex	= SkelComp->SkeletalMesh->MatchRefBone(BoneName);
			BoneList->Append(*FString::Printf(TEXT("(%d) %s"), BoneIndex, *BoneName.ToString()));
		}
	}
}

void WxAnimAimOffsetEditor::UpdateTextEntry()
{
	FVector CurrentVal;

	if( MoveMode == AOE_Rotate )
	{
		FQuat CurrentQuat = EditInfo->EditNode->GetBoneAimQuaternion(EditBoneIndex, EditDir);
		CurrentVal = CurrentQuat.Euler();
	}
	else
	{
		CurrentVal = EditInfo->EditNode->GetBoneAimTranslation(EditBoneIndex, EditDir);
	}

	XEntry->SetValue( *FString::Printf(TEXT("%3.2f"), CurrentVal.X) );
	YEntry->SetValue( *FString::Printf(TEXT("%3.2f"), CurrentVal.Y) );
	ZEntry->SetValue( *FString::Printf(TEXT("%3.2f"), CurrentVal.Z) );
}

void WxAnimAimOffsetEditor::UncheckAllDirButtonExcept(EAnimAimDir InDir)
{
	if(InDir != ANIMAIM_LEFTUP)
		LUButton->SetValue(FALSE);

	if(InDir != ANIMAIM_CENTERUP)
		CUButton->SetValue(FALSE);

	if(InDir != ANIMAIM_RIGHTUP)
		RUButton->SetValue(FALSE);

	if(InDir != ANIMAIM_LEFTCENTER)
		LCButton->SetValue(FALSE);

	if(InDir != ANIMAIM_CENTERCENTER)
		CCButton->SetValue(FALSE);

	if(InDir != ANIMAIM_RIGHTCENTER)
		RCButton->SetValue(FALSE);

	if(InDir != ANIMAIM_LEFTDOWN)
		LDButton->SetValue(FALSE);

	if(InDir != ANIMAIM_CENTERDOWN)
		CDButton->SetValue(FALSE);

	if(InDir != ANIMAIM_RIGHTDOWN)
		RDButton->SetValue(FALSE);
}

/** 
 *	Clear and re-fill the profile list combo. 
 *	Will no preserve current selection - you need to do that after this call.
 */
void WxAnimAimOffsetEditor::UpdateProfileCombo()
{
	ProfileCombo->Freeze();
	ProfileCombo->Clear();

	check(EditInfo);
	check(EditInfo->EditNode);

	// Iterate over each profile, adding its name to the combo.
	for(INT i=0; i<EditInfo->EditNode->Profiles.Num(); i++)
	{
		FAimOffsetProfile& P = EditInfo->EditNode->Profiles(i);
		ProfileCombo->Append( *P.ProfileName.ToString() );
	}

	ProfileCombo->Thaw();
}

//////////////////////////////////////////////////////////////////////////
// TOOLBAR

BEGIN_EVENT_TABLE( WxAnimAimOffsetToolBar, WxToolBar )
END_EVENT_TABLE()

WxAnimAimOffsetToolBar::WxAnimAimOffsetToolBar( wxWindow* InParent, wxWindowID InID )
: WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
{
	TranslateB.Load(TEXT("ASV_Translate"));
	RotateB.Load(TEXT("ASV_Rotate"));
	BM_AOE_LOAD.Load(TEXT("Open"));
	BM_AOE_SAVE.Load(TEXT("Save"));

	SetToolBitmapSize( wxSize( 16, 16 ) );

	AddSeparator();
	AddRadioTool(ID_AOE_TRANSLATE, TEXT("TranslateSocket"), TranslateB, wxNullBitmap, *LocalizeUnrealEd("TranslateSocket") );
	AddRadioTool(ID_AOE_ROTATE, TEXT("RotateSocket"), RotateB, wxNullBitmap, *LocalizeUnrealEd("RotateSocket") );

	AddSeparator();
	AddTool(ID_AOE_LOAD, BM_AOE_LOAD, *LocalizeUnrealEd("Open"));
	AddTool(ID_AOE_SAVE, BM_AOE_SAVE, *LocalizeUnrealEd("AOE_SAVE"));

	Realize();
}

WxAnimAimOffsetToolBar::~WxAnimAimOffsetToolBar()
{
}
