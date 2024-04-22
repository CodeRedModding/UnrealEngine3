/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "PropertyWindow.h"
#include "DlgGeometryTools.h"

BEGIN_EVENT_TABLE(WxDlgGeometryTools, wxDialog)
	EVT_COMMAND_RANGE( ID_GEOMMODIFIER_START, ID_GEOMMODIFIER_END, wxEVT_COMMAND_RADIOBUTTON_SELECTED, WxDlgGeometryTools::OnModifierSelected )
	EVT_COMMAND_RANGE( ID_GEOMMODIFIER_START, ID_GEOMMODIFIER_END, wxEVT_COMMAND_BUTTON_CLICKED , WxDlgGeometryTools::OnModifierClicked )
	EVT_UPDATE_UI_RANGE( ID_GEOMMODIFIER_START, ID_GEOMMODIFIER_END, WxDlgGeometryTools::OnUpdateUI )
	EVT_BUTTON( ID_GEOMMODIFIER_ACTION, WxDlgGeometryTools::OnActionClicked )
	EVT_CLOSE( WxDlgGeometryTools::OnClose )
END_EVENT_TABLE()

/**
* UnrealEd dialog with various geometry mode related tools on it.
*/
WxDlgGeometryTools::WxDlgGeometryTools(wxWindow* InParent) : 
	wxDialog(InParent, wxID_ANY, TEXT("DlgGeometryToolsTitle"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxRESIZE_BORDER | wxCLOSE_BOX | wxSYSTEM_MENU )
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode( EM_Geometry );
	FModeTool* tool = mode->GetCurrentTool();

	// If this dialog is being summoned geometry mode must be active
	check( mode && tool );

	FModeTool_GeometryModify* mtgm = (FModeTool_GeometryModify*)tool;
	INT id = ID_GEOMMODIFIER_START;
	for( FModeTool_GeometryModify::TModifierConstIterator Itor( mtgm->ModifierConstIterator() ) ; Itor ; ++Itor )
	{
		const UGeomModifier* modifier = *Itor;
		if( modifier->bPushButton )
		{
			PushButtons.AddItem( new wxButton( this, id, *modifier->GetModifierDescription(), wxDefaultPosition, wxDefaultSize, 0 ) );
		}
		else
		{
			RadioButtons.AddItem( new wxRadioButton( this, id, *modifier->GetModifierDescription(), wxDefaultPosition, wxDefaultSize, 0 ) );
		}

		id++;
	}

	MainVerticalSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(MainVerticalSizer);

	wxStaticBox* itemStaticBoxSizer3Static = new wxStaticBox(this, wxID_ANY, TEXT("Modifiers"));
	wxStaticBoxSizer* itemStaticBoxSizer3 = new wxStaticBoxSizer(itemStaticBoxSizer3Static, wxHORIZONTAL);
	MainVerticalSizer->Add(itemStaticBoxSizer3, 0, wxGROW|wxALL, 5);

	wxBoxSizer* itemBoxSizer4 = new wxBoxSizer(wxVERTICAL);
	itemStaticBoxSizer3->Add(itemBoxSizer4, 1, wxGROW|wxALL, 5);

	wxGridSizer* itemGridSizer5 = new wxGridSizer(2, 2, 0, 0);
	itemBoxSizer4->Add(itemGridSizer5, 1, wxGROW|wxALL, 5);

	for( INT x = 0 ; x < RadioButtons.Num() ; ++x )
	{
		RadioButtons(x)->Show();
		itemGridSizer5->Add( RadioButtons(x), 0, wxALIGN_CENTER_HORIZONTAL|wxGROW|wxALL, 5);
	}

	wxButton* itemButton9 = new wxButton( this, ID_GEOMMODIFIER_ACTION, TEXT("Apply"), wxDefaultPosition, wxDefaultSize, 0 );
	itemBoxSizer4->Add(itemButton9, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

	wxStaticBox* itemStaticBoxSizer8Static = new wxStaticBox(this, wxID_ANY, TEXT("Properties"));
	PropertyGroupSizer = new wxStaticBoxSizer(itemStaticBoxSizer8Static, wxHORIZONTAL);
	PropertyGroupSizer->SetMinSize(235, 140);
	MainVerticalSizer->Add(PropertyGroupSizer, 1, wxGROW|wxALL, 5);

	//Must occur before property window is created, otherwise search field will get localized incorrectly
	FLocalizeWindow( this );

	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create( this, NULL, -1, FALSE );
	PropertyWindow->SetFlags( EPropertyWindowFlags::SupportsCustomControls, TRUE );
	PropertyGroupSizer->Add(PropertyWindow, 1, wxGROW|wxALL, 5);

	wxStaticBox* itemStaticBoxSizer10Static = new wxStaticBox(this, wxID_ANY, TEXT("Modifiers"));
	wxStaticBoxSizer* itemStaticBoxSizer10 = new wxStaticBoxSizer(itemStaticBoxSizer10Static, wxHORIZONTAL);
	MainVerticalSizer->Add(itemStaticBoxSizer10, 0, wxGROW|wxALL, 5);

	wxGridSizer* itemGridSizer11 = new wxGridSizer(2, 2, 0, 0);
	itemStaticBoxSizer10->Add(itemGridSizer11, 1, wxALIGN_TOP|wxALL, 5);

	for( INT x = 0 ; x < PushButtons.Num() ; ++x )
	{
		PushButtons(x)->Show();
		itemGridSizer11->Add( PushButtons(x), 0, wxALIGN_CENTER_HORIZONTAL|wxGROW|wxALL, 5);
	}

	SetSizer(MainVerticalSizer);
	
	// The first button/modifier is assumed to be "Edit"

	RadioButtons(0)->SetValue( 1 );
	FModeTool_GeometryModify::TModifierIterator Itor2( mtgm->ModifierIterator() );
	MainVerticalSizer->Hide(PropertyGroupSizer);  // properties not shown for Edit mode
	IsShowingProperties = FALSE;
	mtgm->SetCurrentModifier( *Itor2 );

	FWindowUtil::LoadPosSize( TEXT("DlgGeometryTools"), this );

	MainVerticalSizer->Fit(this);

	Show ( FALSE );
}

WxDlgGeometryTools::~WxDlgGeometryTools()
{
	FWindowUtil::SavePosSize( TEXT("DlgGeometryTools"), this );
}

void WxDlgGeometryTools::OnModifierSelected( wxCommandEvent& In )
{
	// Geometry mode must be active
	check( GEditorModeTools().IsModeActive( EM_Geometry ) );

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode( EM_Geometry );
	FModeTool_GeometryModify* tool = (FModeTool_GeometryModify*)mode->GetCurrentTool();

	const INT idx = In.GetId() - ID_GEOMMODIFIER_START;

	PropertyWindow->SetObject( tool->GetModifier(idx), EPropertyWindowFlags::Sorted );

	// Hide properties in Edit mode (index 0)
	UBOOL ShowProperties = 0 != idx;
	if (ShowProperties != IsShowingProperties)
	{
		if (ShowProperties)
		{
			MainVerticalSizer->Show(PropertyGroupSizer);
		}
		else
		{
			MainVerticalSizer->Hide(PropertyGroupSizer);
		}
		MainVerticalSizer->Layout();
		MainVerticalSizer->Fit(this);
	}
	IsShowingProperties = ShowProperties;

	tool->SetCurrentModifier( tool->GetModifier(idx) );
}

void WxDlgGeometryTools::OnModifierClicked( wxCommandEvent& In )
{
	check( GEditorModeTools().IsModeActive( EM_Geometry ) );

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode( EM_Geometry );
	FModeTool_GeometryModify* tool = (FModeTool_GeometryModify*)mode->GetCurrentTool();

	const INT idx = In.GetId() - ID_GEOMMODIFIER_START;

	tool->GetModifier(idx)->Apply();
}

void WxDlgGeometryTools::OnUpdateUI( wxUpdateUIEvent& In )
{
	if ( GEditorModeTools().IsModeActive( EM_Geometry ) )
	{
		FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode( EM_Geometry );
		FModeTool_GeometryModify* tool = (FModeTool_GeometryModify*)mode->GetCurrentTool();
 		const INT idx = In.GetId() - ID_GEOMMODIFIER_START;
		UGeomModifier* modifier = tool->GetModifier(idx);

		// Enable/Disable modifier buttons based on the modifier supporting the current selection type.

		if( modifier->Supports() )
		{
			In.Enable( 1 );
		}
		else
		{
			In.Enable( 0 );

			// If this selection type doesn't support this modifier, and this modifier is currently
			// selected, select the first modifier in the list instead.  The first modifier is always
			// "Edit" and it supports everything by design.

			if( tool->GetCurrentModifier() == modifier )
			{
				FModeTool_GeometryModify::TModifierIterator Itor( tool->ModifierIterator() );
				tool->SetCurrentModifier( *Itor );
				PropertyWindow->SetObject( *Itor, EPropertyWindowFlags::Sorted );

				In.Check( tool->GetCurrentModifier() == modifier );
			}
		}
	}
}

/**
* Takes any input the user has made to the Keyboard section of the current modifier and causes the modifier activate.
*/
void WxDlgGeometryTools::OnActionClicked( wxCommandEvent& In )
{
	check( GEditorModeTools().IsModeActive( EM_Geometry ) );

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode( EM_Geometry );
	FModeTool_GeometryModify* tool = (FModeTool_GeometryModify*)mode->GetCurrentTool();

	tool->GetCurrentModifier()->Apply();
}

void WxDlgGeometryTools::OnClose( wxCloseEvent& In )
{
	check( GEditorModeTools().IsModeActive( EM_Geometry ) );
	GEditorModeTools().DeactivateMode( EM_Geometry );
}
