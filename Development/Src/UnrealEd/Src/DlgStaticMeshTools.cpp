/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "PropertyWindow.h"
#include "DlgStaticMeshTools.h"

IMPLEMENT_CLASS(UStaticMeshMode_Options)

BEGIN_EVENT_TABLE(WxDlgStaticMeshTools, wxDialog)
	EVT_CLOSE(WxDlgStaticMeshTools::OnClose)
END_EVENT_TABLE()

/**
* UnrealEd dialog with various static mesh mode related tools on it.
*/
WxDlgStaticMeshTools::WxDlgStaticMeshTools(wxWindow* InParent) : 
	wxDialog(InParent, wxID_ANY, TEXT("DlgStaticMeshToolsTitle"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxRESIZE_BORDER | wxCLOSE_BOX | wxSYSTEM_MENU )
{
	FEdModeStaticMesh* mode = (FEdModeStaticMesh*)GEditorModeTools().GetActiveMode( EM_StaticMesh );
	FModeTool* tool = mode->GetCurrentTool();
	
	// If this dialog is being summoned static mesh mode must be active
	check( mode && tool );

	wxBoxSizer* VerticalSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(VerticalSizer);

	//Must occur before property window is created, otherwise search field will get localized incorrectly
	FLocalizeWindow( this );

	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create( this, NULL );
	PropertyWindow->SetFlags( EPropertyWindowFlags::SupportsCustomControls, TRUE );
	VerticalSizer->Add(PropertyWindow, 1, wxGROW|wxALL, 5);

	SetSizer(VerticalSizer);
	
	PropertyWindow->SetObject( ((FModeTool_StaticMesh*)tool)->StaticMeshModeOptions, EPropertyWindowFlags::ShouldShowCategories );
	PropertyWindow->ExpandAllItems();

	FWindowUtil::LoadPosSize( TEXT("DlgStaticMeshTools"), this );

	VerticalSizer->Fit(this);

	// Increase the width of the window a little to allow property visibility of the property window

	INT w, h;
	GetSize( &w, &h );
	SetSize( w + 192, h + 256 );

	Show ( FALSE );
}

WxDlgStaticMeshTools::~WxDlgStaticMeshTools()
{
	FWindowUtil::SavePosSize( TEXT("DlgStaticMeshTools"), this );
}

void WxDlgStaticMeshTools::OnClose( wxCloseEvent& In )
{
	GEditorModeTools().DeactivateMode( EM_StaticMesh );
}