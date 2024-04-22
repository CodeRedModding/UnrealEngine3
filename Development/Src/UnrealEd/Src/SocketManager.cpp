/*=============================================================================
	SocketManager.h: AnimSet viewer's socket manager.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "TrackableWindow.h"
#include "SocketManager.h"
#include "AnimSetViewer.h"
#include "PropertyWindow.h"

/** Border width around the socket manager controls. */
static INT SocketManager_ControlBorder = 4;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxSocketManagerToolBar
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

WxSocketManagerToolBar::WxSocketManagerToolBar( wxWindow* InParent, wxWindowID InID )
	:	WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
{
	TranslateB.Load(TEXT("ASV_Translate"));
	RotateB.Load(TEXT("ASV_Rotate"));
	ClearPreviewB.Load(TEXT("ASV_ClearPreview"));
	CopyB.Load(TEXT("Cut"));
	PasteB.Load(TEXT("Paste"));

	SetToolBitmapSize( wxSize( 16, 16 ) );

	AddSeparator();
	AddRadioTool(IDM_ANIMSET_SOCKET_TRANSLATE, *LocalizeUnrealEd("TranslateSocket"), TranslateB, wxNullBitmap, *LocalizeUnrealEd("TranslateSocket"));
	AddRadioTool(IDM_ANIMSET_SOCKET_ROTATE, *LocalizeUnrealEd("RotateSocket"), RotateB, wxNullBitmap, *LocalizeUnrealEd("RotateSocket"));
	AddSeparator();
	AddTool(IDM_ANIMSET_CLEARPREVIEWS, *LocalizeUnrealEd("ClearPreviews"), ClearPreviewB, *LocalizeUnrealEd("ClearPreviews"));
	AddSeparator();
	AddTool(IDM_ANIMSET_COPYSOCKETS, *LocalizeUnrealEd("CopySockets"), CopyB, *LocalizeUnrealEd("CopySockets"));
	AddTool(IDM_ANIMSET_PASTESOCKETS, *LocalizeUnrealEd("PasteSockets"), PasteB, *LocalizeUnrealEd("PasteSockets"));

	ToggleTool(IDM_ANIMSET_SOCKET_TRANSLATE, true);

	Realize();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxSocketManager
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE( WxSocketManager, WxTrackableFrame )
	EVT_CLOSE( WxSocketManager::OnClose )
	EVT_TEXT_ENTER( ID_SM_WSRotXEntry, WxSocketManager::OnRotEntry )
	EVT_TEXT_ENTER( ID_SM_WSRotYEntry, WxSocketManager::OnRotEntry )
	EVT_TEXT_ENTER( ID_SM_WSRotZEntry, WxSocketManager::OnRotEntry )
END_EVENT_TABLE()

WxSocketManager::WxSocketManager(WxAnimSetViewer* InASV, wxWindowID InID)
	:	WxTrackableFrame( InASV, InID, *LocalizeUnrealEd("SocketManager"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR )
{
	AnimSetViewer = InASV;

	MoveMode = SMM_Translate;

	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));

	wxBoxSizer* TopSizerH = new wxBoxSizer( wxHORIZONTAL );
	this->SetSizer(TopSizerH);
	this->SetAutoLayout(true);

	wxBoxSizer* LeftSizerV = new wxBoxSizer( wxVERTICAL );
	TopSizerH->Add( LeftSizerV, 0, wxGROW | wxALL, SocketManager_ControlBorder );

	wxStaticText* ListLabel = new wxStaticText( this, -1, *LocalizeUnrealEd("Sockets") );
	LeftSizerV->Add( ListLabel, 0, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, SocketManager_ControlBorder );

	SocketList = new wxListBox( this, IDM_ANIMSET_SOCKETLIST, wxDefaultPosition, wxSize(200, -1), 0, NULL, wxLB_SINGLE|wxLB_SORT );
	LeftSizerV->Add( SocketList, 1, wxGROW | wxALL, SocketManager_ControlBorder );

	wxBoxSizer* ButtonSizerH = new wxBoxSizer( wxHORIZONTAL );
	LeftSizerV->Add( ButtonSizerH, 0, wxGROW | wxALL, SocketManager_ControlBorder );

	NewSocketButton = new wxButton( this, IDM_ANIMSET_NEWSOCKET, *LocalizeUnrealEd("NewSocket"), wxDefaultPosition, wxSize(-1, 30) );
	ButtonSizerH->Add( NewSocketButton, 1, wxGROW | wxRIGHT, SocketManager_ControlBorder );

	DeleteSocketButton = new wxButton( this, IDM_ANIMSET_DELSOCKET, *LocalizeUnrealEd("DeleteSocket"), wxDefaultPosition, wxSize(-1, 30) );
	ButtonSizerH->Add( DeleteSocketButton, 1, wxGROW | wxLEFT, SocketManager_ControlBorder );

	// World Space Rotation
	wxStaticBox* itemStaticBoxSizer16Static = new wxStaticBox(this, wxID_ANY, TEXT("World Space Rotation"));
	wxStaticBoxSizer* itemStaticBoxSizer16 = new wxStaticBoxSizer(itemStaticBoxSizer16Static, wxVERTICAL);
	LeftSizerV->Add(itemStaticBoxSizer16, 1, wxEXPAND|wxALL, 5);

	wxFlexGridSizer* itemFlexGridSizer17 = new wxFlexGridSizer(3, 2, 0, 0);
	itemFlexGridSizer17->AddGrowableCol(1);
	itemStaticBoxSizer16->Add(itemFlexGridSizer17, 0, wxEXPAND|wxALL, 5);

	// Rot X ENTRY
	wxStaticText* itemStaticText18 = new wxStaticText( this, wxID_STATIC, TEXT("Pitch"), wxDefaultPosition, wxDefaultSize, 0 );
	itemFlexGridSizer17->Add(itemStaticText18, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

	WSRotXEntry = new wxTextCtrl( this, ID_SM_WSRotXEntry, TEXT(""), wxDefaultPosition, wxDefaultSize, 0 );
	itemFlexGridSizer17->Add(WSRotXEntry, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// Rot Y ENTRY
	wxStaticText* itemStaticText20 = new wxStaticText( this, wxID_STATIC, TEXT("Yaw"), wxDefaultPosition, wxDefaultSize, 0 );
	itemFlexGridSizer17->Add(itemStaticText20, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

	WSRotYEntry = new wxTextCtrl( this, ID_SM_WSRotYEntry, TEXT(""), wxDefaultPosition, wxDefaultSize, 0 );
	itemFlexGridSizer17->Add(WSRotYEntry, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// Rot Z ENTRY
	wxStaticText* itemStaticText22 = new wxStaticText( this, wxID_STATIC, TEXT("Roll"), wxDefaultPosition, wxDefaultSize, 0 );
	itemFlexGridSizer17->Add(itemStaticText22, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

	WSRotZEntry = new wxTextCtrl( this, ID_SM_WSRotZEntry, TEXT(""), wxDefaultPosition, wxDefaultSize, 0 );
	itemFlexGridSizer17->Add(WSRotZEntry, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);
	// End

	SocketProps = new WxPropertyWindowHost;
	SocketProps->Create( this, this );
	TopSizerH->Add( SocketProps, 1, wxGROW | wxALL, SocketManager_ControlBorder );

	ToolBar = new WxSocketManagerToolBar( this, -1 );
	SetToolBar( ToolBar );

	FWindowUtil::LoadPosSize( TEXT("ASVSocketMgr"), this, 100, 100, 640, 300 );
}

void WxSocketManager::OnClose(wxCloseEvent& In)
{
	FWindowUtil::SavePosSize( TEXT("ASVSocketMgr"), this );	

	check(AnimSetViewer->SocketMgr == this);
	AnimSetViewer->SocketMgr = NULL;
	AnimSetViewer->SelectedSocket = NULL;

	AnimSetViewer->ToolBar->ToggleTool(IDM_ANIMSET_OPENSOCKETMGR, false);

	this->Destroy();
}


void WxSocketManager::OnRotEntry(wxCommandEvent& In)
{
	// Grab the text and process it.
	ProcessTextEntry();

	// Select the text in the box so we can just type again.
	INT Id = In.GetId();
	if( Id == ID_SM_WSRotXEntry )
	{
		WSRotXEntry->SetSelection(-1,-1);
	}
	else if( Id == ID_SM_WSRotYEntry )
	{
		WSRotYEntry->SetSelection(-1,-1);
	}
	else if( Id == ID_SM_WSRotZEntry )
	{
		WSRotZEntry->SetSelection(-1,-1);
	}
}

void WxSocketManager::ProcessTextEntry()
{
	if( !AnimSetViewer || !AnimSetViewer->SelectedSocket || !AnimSetViewer->PreviewSkelComp )
	{
		return;
	}

	// Get the value from each edit box.
	FVector NewVal;
	double NewNum;
	UBOOL bIsNumber = WSRotXEntry->GetValue().ToDouble(&NewNum);
	if( !bIsNumber )
	{
		NewNum = 0.f;
	}
	NewVal.X = NewNum;

	bIsNumber = WSRotYEntry->GetValue().ToDouble(&NewNum);
	if( !bIsNumber )
	{
		NewNum = 0.f;
	}
	NewVal.Y = NewNum;

	bIsNumber = WSRotZEntry->GetValue().ToDouble(&NewNum);
	if( !bIsNumber )
	{
		NewNum = 0.f;
	}
	NewVal.Z = NewNum;

	// Convert from degrees to Quaterion
	FQuat NewQuat = FQuat::MakeFromEuler(NewVal);

	FQuat RelativeRotQuat	= AnimSetViewer->SelectedSocket->RelativeRotation.Quaternion();
	FMatrix SocketMatrix;
	AnimSetViewer->SelectedSocket->GetSocketMatrix(SocketMatrix, AnimSetViewer->PreviewSkelComp);
	FQuat WorldRotQuat		= FQuat(SocketMatrix);
	FQuat ParentQuat		= WorldRotQuat * -RelativeRotQuat;
	FQuat NewRelQuat		= NewQuat * -ParentQuat;

	AnimSetViewer->SelectedSocket->RelativeRotation = FQuatRotationTranslationMatrix(NewRelQuat, FVector(0.f)).Rotator();
}

void WxSocketManager::UpdateTextEntry()
{
	if( AnimSetViewer && AnimSetViewer->SelectedSocket && AnimSetViewer->PreviewSkelComp )
	{
		FMatrix SocketMatrix;
		AnimSetViewer->SelectedSocket->GetSocketMatrix(SocketMatrix, AnimSetViewer->PreviewSkelComp);
		FQuat WorldRotQuat	= FQuat(SocketMatrix);
		FVector CurrentVal	= WorldRotQuat.Euler();

		WSRotXEntry->SetValue( *FString::Printf(TEXT("%3.2f"), CurrentVal.X) );
		WSRotYEntry->SetValue( *FString::Printf(TEXT("%3.2f"), CurrentVal.Y) );
		WSRotZEntry->SetValue( *FString::Printf(TEXT("%3.2f"), CurrentVal.Z) );
	}
}

/**
 * Calls the UpdateSocketPreviewComps function if we have change the preview Skel/Static Mesh.
 */
void WxSocketManager::NotifyPostChange(void* Src, UProperty* PropertyThatChanged)
{
	if( PropertyThatChanged
			&& (PropertyThatChanged->GetFName() == FName(TEXT("PreviewSkelMesh"))
			||  PropertyThatChanged->GetFName() == FName(TEXT("PreviewStaticMesh"))
			||  PropertyThatChanged->GetFName() == FName(TEXT("PreviewParticleSystem")) ))
	{
		AnimSetViewer->RecreateSocketPreviews();
	}
	else
	{
		AnimSetViewer->UpdateSocketPreviews();
	}
}

void WxSocketManager::NotifyExec(void* Src, const TCHAR* Cmd)
{
	GUnrealEd->NotifyExec(Src, Cmd);
}

/**
* Called when the window has been selected in the ctrl + tab dialog box.
*/
void WxSocketManager::OnSelected()
{
	Raise();
}
