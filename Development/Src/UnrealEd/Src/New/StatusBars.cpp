
#include "UnrealEd.h"

/*------------------------------------------------------------------------------
    Standard
------------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxStatusBarStandard, WxStatusBar)
	EVT_SIZE(OnSize)
	EVT_COMMAND( IDCB_DRAG_GRID_TOGGLE, wxEVT_COMMAND_CHECKBOX_CLICKED, WxStatusBarStandard::OnDragGridToggleClick )
	EVT_COMMAND( IDCB_ROTATION_GRID_TOGGLE, wxEVT_COMMAND_CHECKBOX_CLICKED, WxStatusBarStandard::OnRotationGridToggleClick )
	EVT_TEXT_ENTER( ID_EXEC_COMMAND, WxStatusBarStandard::OnExecComboEnter )
	EVT_COMBOBOX( ID_EXEC_COMMAND, WxStatusBarStandard::OnExecComboSelChange )
END_EVENT_TABLE()

WxStatusBarStandard::WxStatusBarStandard()
	: WxStatusBar()
{
	ExecCombo = NULL;
	DragGridCB = RotationGridCB = NULL;
	DragGridSB = RotationGridSB = NULL;
	DragGridMB = RotationGridMB = NULL;
	DragGridST = RotationGridST = NULL;
}

WxStatusBarStandard::~WxStatusBarStandard()
{
}

void WxStatusBarStandard::SetUp()
{
	// Bitmaps

	DragGridB.Load( TEXT("DragGrid") );
	RotationGridB.Load( TEXT("RotationGrid") );

	// Controls

	wxString dummy[1];
	ExecCombo = new wxComboBox( this, ID_EXEC_COMMAND, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, dummy, wxCB_DROPDOWN );
	DragGridST = new wxStaticText( this, IDST_DRAG_GRID, TEXT("XXXX"), wxDefaultPosition, wxSize(-1,-1), wxST_NO_AUTORESIZE );
	RotationGridST = new wxStaticText( this, IDST_ROTATION_GRID, TEXT("XXXXX"), wxDefaultPosition, wxSize(-1,-1), wxST_NO_AUTORESIZE );
	DragGridCB = new wxCheckBox( this, IDCB_DRAG_GRID_TOGGLE, TEXT("") );
	RotationGridCB = new wxCheckBox( this, IDCB_ROTATION_GRID_TOGGLE, TEXT("") );
	DragGridSB = new wxStaticBitmap( this, IDSB_DRAG_GRID, DragGridB );
	RotationGridSB = new wxStaticBitmap( this, IDSB_ROTATION_GRID, RotationGridB );
	DragGridMB = new WxMenuButton( this, IDPB_DRAG_GRID, &GApp->EditorFrame->DownArrowB, GApp->EditorFrame->DragGridMenu );
	RotationGridMB = new WxMenuButton( this, IDPB_ROTATION_GRID, &GApp->EditorFrame->DownArrowB, GApp->EditorFrame->RotationGridMenu );

	DragGridCB->SetToolTip( TEXT("Toggle Drag Grid") );
	RotationGridCB->SetToolTip( TEXT("Toggle Rotation Grid") );
	DragGridMB->SetToolTip( TEXT("Change Drag Grid") );
	RotationGridMB->SetToolTip( TEXT("Change Rotation Grid") );

	// Now that we have the controls created, figure out how large each pane should be.

	DragTextWidth = DragGridST->GetRect().GetWidth();
	RotationTextWidth = RotationGridST->GetRect().GetWidth();
	INT ExecComboPane = 350;
	INT DragGridPane = 2+ DragGridB.GetWidth() +2+ DragTextWidth +2+ DragGridCB->GetRect().GetWidth() +2+ DragGridMB->GetRect().GetWidth() +1;
	INT RotationGridPane = 2+ RotationGridB.GetWidth() +2+ RotationTextWidth +2+ RotationGridCB->GetRect().GetWidth() +2+ RotationGridMB->GetRect().GetWidth() +3;
	INT Widths[] = { ExecComboPane, -1, DragGridPane, RotationGridPane };
	SetFieldsCount( sizeof(Widths)/sizeof(INT), Widths );

	// Update with initial values and resize everything.

	UpdateUI();

    wxSizeEvent eventSize;
	OnSize( eventSize );
}

void WxStatusBarStandard::UpdateUI()
{
	if( !DragGridCB ) return;

	DragGridCB->SetValue( GEditor->Constraints.GridEnabled );
	RotationGridCB->SetValue( GEditor->Constraints.RotGridEnabled );

	DragGridST->SetLabel( *FString::Printf( TEXT("%g"), GEditor->Constraints.GetGridSize() ) );

	RotationGridST->SetLabel( *FString::Printf( TEXT("%d"), appFloor( GEditor->Constraints.RotGridSize.Pitch / (16384.0 / 90.0 ) ) ) );
}

void WxStatusBarStandard::OnDragGridToggleClick( wxCommandEvent& InEvent )
{
	GEditor->Constraints.GridEnabled = !GEditor->Constraints.GridEnabled;
}

void WxStatusBarStandard::OnRotationGridToggleClick( wxCommandEvent& InEvent )
{
	GEditor->Constraints.RotGridEnabled = !GEditor->Constraints.RotGridEnabled;
}

void WxStatusBarStandard::OnSize( wxSizeEvent& InEvent )
{
	wxRect rect;

	// Exec combo

	GetFieldRect( FIELD_ExecCombo, rect );
	ExecCombo->SetSize( rect.x+2, rect.y+2, rect.GetWidth()-4, rect.GetHeight()-4 );

	// Drag grid

	if( DragGridSB )
	{
		GetFieldRect( FIELD_Drag_Grid, rect );

		INT Left = rect.x + 2;
		DragGridSB->SetSize( Left, rect.y+2, DragGridB.GetWidth(), rect.height-4 );
		Left += DragGridB.GetWidth() + 2;
		DragGridST->SetSize( Left, rect.y+2, DragTextWidth, rect.height-4 );
		Left += DragTextWidth + 2;
		DragGridCB->SetSize( Left, rect.y+2, -1, rect.height-4 );
		Left += DragGridCB->GetSize().GetWidth() + 2;
		DragGridMB->SetSize( Left, rect.y+1, 13, rect.height-2 );
	}

	// Rotation grid

	if( RotationGridSB )
	{
		GetFieldRect( FIELD_Rotation_Grid, rect );

		INT Left = rect.x + 2;
		RotationGridSB->SetSize( Left, rect.y+2, RotationGridB.GetWidth(), rect.height-4 );
		Left += RotationGridB.GetWidth() + 2;
		RotationGridST->SetSize( Left, rect.y+2, RotationTextWidth, rect.height-4 );
		Left += RotationTextWidth + 2;
		RotationGridCB->SetSize( Left, rect.y+2, -1, rect.height-4 );
		Left += RotationGridCB->GetSize().GetWidth() + 2;
		RotationGridMB->SetSize( Left, rect.y+1, 13, rect.height-2 );
	}

}

void WxStatusBarStandard::OnExecComboEnter( wxCommandEvent& In )
{
	FString exec = ExecCombo->GetValue();
	GEngine->Exec( *exec );

	AddToExecCombo( *exec );
}

void WxStatusBarStandard::OnExecComboSelChange( wxCommandEvent& In )
{
	FString exec = ExecCombo->GetString( ExecCombo->GetSelection() );
	GEngine->Exec( *exec );
}

void WxStatusBarStandard::AddToExecCombo( FString InExec )
{
	// If the string isn't already in the combo box list, add it.

	if( ExecCombo->FindString( *InExec ) == -1 )
	{
		ExecCombo->Append( *InExec );
	}
}

/*------------------------------------------------------------------------------
    Progress
------------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxStatusBarProgress, WxStatusBar)
	EVT_SIZE(WxStatusBarProgress::OnSize)
END_EVENT_TABLE()

WxStatusBarProgress::WxStatusBarProgress()
	: WxStatusBar()
{
}

WxStatusBarProgress::~WxStatusBarProgress()
{
}

void WxStatusBarProgress::SetUp()
{
	Gauge = new wxGauge( this, ID_PROGRESS_BAR, 100 );

	INT Widths[] = { -1, -3 };
	SetFieldsCount( sizeof(Widths)/sizeof(INT), Widths );

    wxSizeEvent     eventSize;
	OnSize( eventSize );
}

void WxStatusBarProgress::UpdateUI()
{
}

void WxStatusBarProgress::OnSize( wxSizeEvent& InEvent )
{
	wxRect rect;
	GetFieldRect( FIELD_Progress, rect );
	Gauge->SetSize( rect.x + 2, rect.y + 2, rect.width - 4, rect.height - 4 );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
