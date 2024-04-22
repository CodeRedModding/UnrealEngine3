/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "LevelUtils.h"
#include "EnginePrefabClasses.h"
#include "SourceControl.h"

enum
{
	DRAWSCALE_NONE,
	DRAWSCALE,
	DRAWSCALE_X,
	DRAWSCALE_Y,
	DRAWSCALE_Z,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxScaleTextCtrl
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void SendDrawScaleToSelectedActors(WxScaleTextCtrl& DrawScaleTextCtrl, INT DrawScaleIndex);

class WxScaleTextCtrl : public wxTextCtrl
{
public:
	WxScaleTextCtrl()
		:	wxTextCtrl(),
		DrawScaleIndex( DRAWSCALE_NONE ),
		Next( NULL ),
		Prev( NULL )
	{}

	WxScaleTextCtrl(wxWindow* parent, wxWindowID id, const wxString& value, const wxPoint& pos, const wxSize& size, long style)
		:	wxTextCtrl( parent, id, value, pos, size, style ),
		DrawScaleIndex( DRAWSCALE_NONE ),
		Next( NULL ),
		Prev( NULL )
	{}

	void Link(INT InDrawScaleIndex, WxScaleTextCtrl* InNext, WxScaleTextCtrl* InPrev)
	{
		DrawScaleIndex = InDrawScaleIndex;
		Next = InNext;
		Prev = InPrev;
	}

private:
	INT DrawScaleIndex;
	WxScaleTextCtrl* Next;
	WxScaleTextCtrl* Prev;

	void OnChar(wxKeyEvent& In)
	{
		switch( In.GetKeyCode() )
		{
		case WXK_UP:
			SendDrawScaleToSelectedActors( *this, DrawScaleIndex );
			check( Prev );
			Prev->SetFocus();
			Next->SetSelection(-1,-1);
			break;
		case WXK_DOWN:
			SendDrawScaleToSelectedActors( *this, DrawScaleIndex );
			check( Next );
			Next->SetFocus();
			Next->SetSelection(-1,-1);
			break;
		default:
			In.Skip();
			break;
		};
	}

	void OnKillFocus(wxFocusEvent& In)
	{
		// Send text when the field loses focus.
		SendDrawScaleToSelectedActors( *this, DrawScaleIndex );
	}

	DECLARE_EVENT_TABLE();
};

BEGIN_EVENT_TABLE(WxScaleTextCtrl, wxTextCtrl)
	EVT_CHAR( WxScaleTextCtrl::OnChar )
	EVT_KILL_FOCUS( WxScaleTextCtrl::OnKillFocus )
END_EVENT_TABLE()

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Static drawscale functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline UBOOL NearlyEqual(FLOAT A, FLOAT B, FLOAT ErrTolerance = KINDA_SMALL_NUMBER)
{
	return Abs( A - B ) < ErrTolerance;
}

static UBOOL GetDrawScaleFromSelectedActors(FLOAT& DrawScale,
											FVector& DrawScale3D,
											UBOOL MultipleDrawScaleValues[4])
{
	for( INT i = 0 ; i < 4 ; ++i )
	{
		MultipleDrawScaleValues[i] = FALSE;
	}

	UBOOL bFoundActor = FALSE;

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if ( !bFoundActor )
		{
			// This is the first actor we've encountered; copy off values.
			bFoundActor = TRUE;
			DrawScale = Actor->DrawScale;
			DrawScale3D = Actor->DrawScale3D;
		}
		else
		{
			if ( !MultipleDrawScaleValues[0] && !NearlyEqual( Actor->DrawScale, DrawScale ) )
			{
				MultipleDrawScaleValues[0] = TRUE;
			}

			for ( INT i = 0 ; i < 3 ; ++i )
			{
				if ( !MultipleDrawScaleValues[i+1] && !NearlyEqual( Actor->DrawScale3D[i], DrawScale3D[i] ) )
				{
					MultipleDrawScaleValues[i+1] = TRUE;
				}
			}

			// Once we've found that all values differ, we can halt.
			if ( MultipleDrawScaleValues[0] && MultipleDrawScaleValues[1] && MultipleDrawScaleValues[2] && MultipleDrawScaleValues[3] )
			{
				break;
			}
		}
	}

	return bFoundActor;
}

static void SendDrawScaleToSelectedActors(WxScaleTextCtrl& DrawScaleTextCtrl, INT DrawScaleIndex)
{
	double DoubleDrawScale;
	const UBOOL bIsNumber = DrawScaleTextCtrl.GetValue().ToDouble( &DoubleDrawScale );

	if( bIsNumber )
	{
		if (GEditor)
		{
			GEditor->BeginTransaction(*LocalizeUnrealEd("EditProperties"));
		}

		const FLOAT DrawScale = static_cast<FLOAT>( DoubleDrawScale );

		// Fires CALLBACK_LevelDirtied when falling out of scope.
		FScopedLevelDirtied		LevelDirtyCallback;

		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if (!Actor->IsPendingKill() && Actor != GWorld->GetBrush())
			{
				UProperty* SetProperty = NULL;
				FLOAT* SetPropertyFloatPtr = NULL;

				FVector DeltaScaleVector(0.0f, 0.0f, 0.0f);

				switch( DrawScaleIndex )
				{
					case DRAWSCALE:
						if ( !NearlyEqual(Actor->DrawScale, DrawScale) )
						{
							SetProperty = FindField<UProperty>(Actor->GetClass(), TEXT("DrawScale"));
							SetPropertyFloatPtr = &Actor->DrawScale;
							DeltaScaleVector = FVector(DrawScale) - Actor->DrawScale3D;
						}
						break;
					case DRAWSCALE_X:
						if ( !NearlyEqual(Actor->DrawScale3D.X, DrawScale) )
						{
							SetProperty = FindField<UProperty>(Actor->GetClass(), TEXT("DrawScale3D"));
							SetPropertyFloatPtr = &Actor->DrawScale3D.X;
							DeltaScaleVector = FVector(DrawScale - Actor->DrawScale3D.X, 0.0f, 0.0f);
						}
						break;
					case DRAWSCALE_Y:
						if ( !NearlyEqual(Actor->DrawScale3D.Y, DrawScale) )
						{
							SetProperty = FindField<UProperty>(Actor->GetClass(), TEXT("DrawScale3D"));
							SetPropertyFloatPtr = &Actor->DrawScale3D.Y;
							DeltaScaleVector = FVector(0.0f, DrawScale - Actor->DrawScale3D.Y, 0.0f);
						}
						break;
					case DRAWSCALE_Z:
						if ( !NearlyEqual(Actor->DrawScale3D.Z, DrawScale) )
						{
							SetProperty = FindField<UProperty>(Actor->GetClass(), TEXT("DrawScale3D"));
							SetPropertyFloatPtr = &Actor->DrawScale3D.Z;
							DeltaScaleVector = FVector(0.0f, 0.0f, DrawScale - Actor->DrawScale3D.Z);
						}
						break;
					default:
						check( 0 );
						break;
				}

				if (NULL != SetProperty && NULL != SetPropertyFloatPtr)
				{
					// tell the system that the property is about to change
					Actor->PreEditChange(SetProperty);

					// offset scaling based on editor pivot point for actors within a locked group
					AGroupActor* GroupActor = AGroupActor::GetParentForActor(Actor);
					if( GEditor->bGroupingActive && GroupActor != NULL && GroupActor->IsLocked() && Actor->DrawScale3D.GetAbsMax() > 0.0f )
					{
						DeltaScaleVector /= Actor->DrawScale3D;

						GEditor->ApplyDeltaToActor(Actor,
							TRUE,
							NULL,
							NULL,
							&DeltaScaleVector,
							FALSE,
							FALSE,
							FALSE);
					}
					else
					{
						// Otherwise just set actual value of the float on the property normally
						*SetPropertyFloatPtr = DrawScale;

						// tell the system that the property was changed
						FPropertyChangedEvent PropChangedEvent(SetProperty, FALSE, EPropertyChangeType::ValueSet);
						Actor->PostEditChangeProperty(PropChangedEvent);
					}				

					LevelDirtyCallback.Request();
				}
				
			}
		}

		if (GEditor)
		{
			GEditor->EndTransaction();
		}

		if ( LevelDirtyCallback.HasRequests() )
		{
			GCallbackEvent->Send(CALLBACK_ActorPropertiesChange);
			GCallbackEvent->Send(CALLBACK_RedrawAllViewports);
		}

		DrawScaleTextCtrl.SetSelection(-1,-1);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum
{
	FIELD_ExecCombo,
	FIELD_SourceControl,
	FIELD_Lighting,
	FIELD_Paths,
	FIELD_PackageCheckout,
	FIELD_Status,
	FIELD_ActorName,
	FIELD_MouseWorldspacePosition,
	FIELD_DrawScale,
	FIELD_DrawScaleX,
	FIELD_DrawScaleY,
	FIELD_DrawScaleZ,
	FIELD_Drag_Grid,
	FIELD_Rotation_Grid,
	FIELD_Scale_Grid,
	FIELD_Autosave,
	FIELD_Buffer,
};

BEGIN_EVENT_TABLE(WxStatusBarStandard, WxStatusBar)
	EVT_SIZE(OnSize)
	EVT_COMMAND( IDCB_DRAG_GRID_TOGGLE, wxEVT_COMMAND_CHECKBOX_CLICKED, WxStatusBarStandard::OnDragGridToggleClick )
	EVT_COMMAND( IDCB_ROTATION_GRID_TOGGLE, wxEVT_COMMAND_CHECKBOX_CLICKED, WxStatusBarStandard::OnRotationGridToggleClick )
	EVT_COMMAND( IDCB_SCALE_GRID_TOGGLE, wxEVT_COMMAND_CHECKBOX_CLICKED, WxStatusBarStandard::OnScaleGridToggleClick )
	EVT_COMMAND( IDCB_AUTOSAVE_TOGGLE, wxEVT_COMMAND_CHECKBOX_CLICKED, WxStatusBarStandard::OnAutosaveToggleClick )
	EVT_TEXT_ENTER( ID_EXEC_COMMAND, WxStatusBarStandard::OnExecComboEnter )
	EVT_TEXT_ENTER( IDTB_DRAWSCALE, WxStatusBarStandard::OnDrawScale )
	EVT_TEXT_ENTER( IDTB_DRAWSCALEX, WxStatusBarStandard::OnDrawScaleX )
	EVT_TEXT_ENTER( IDTB_DRAWSCALEY, WxStatusBarStandard::OnDrawScaleY )
	EVT_TEXT_ENTER( IDTB_DRAWSCALEZ, WxStatusBarStandard::OnDrawScaleZ )
	EVT_COMBOBOX( ID_EXEC_COMMAND, WxStatusBarStandard::OnExecComboSelChange )
	EVT_BUTTON( IDSB_SCCStatus, WxStatusBarStandard::OnClickSCCStatusButton )
	EVT_BUTTON( IDSB_LightingStatus, WxStatusBarStandard::OnClickLightingStatusButton )
	EVT_BUTTON( IDSB_PathsStatus, WxStatusBarStandard::OnClickPathsStatusButton )
	EVT_BUTTON( IDSB_PackageCheckoutStatus, WxStatusBarStandard::OnClickPackageCheckoutStatusButton )
	EVT_UPDATE_UI( IDSB_AUTOSAVE, WxStatusBarStandard::OnUpdateUI )
	EVT_UPDATE_UI( IDSB_SCCStatus, WxStatusBarStandard::OnUpdateUI )
	EVT_UPDATE_UI( IDSB_LightingStatus, WxStatusBarStandard::OnUpdateUI )
	EVT_UPDATE_UI( IDSB_PackageCheckoutStatus, WxStatusBarStandard::OnUpdateUI )
	EVT_UPDATE_UI( IDSB_PathsStatus, WxStatusBarStandard::OnUpdateUI )
END_EVENT_TABLE()

WxStatusBarStandard::WxStatusBarStandard()
	: WxStatusBar(),
	ExecCombo( NULL ),
	DragGridCB( NULL ),
	RotationGridCB( NULL ),
	ScaleGridCB( NULL ),
	AutoSaveCB( NULL ),
	DragGridSB( NULL ),
	RotationGridSB( NULL ),
	DragGridST( NULL ),
	RotationGridST( NULL ),
	ScaleGridST( NULL ),
	ActorNameST( NULL ),
	DragGridMB( NULL ),
	RotationGridMB( NULL ),
	ScaleGridMB( NULL ),
	AutoSaveMB( NULL ),
	DrawScaleTextCtrl( NULL ),
	DrawScaleXTextCtrl( NULL ),
	DrawScaleYTextCtrl( NULL ),
	DrawScaleZTextCtrl( NULL ),
	AutoSaveSB( NULL ),
	SCCStatusBCB( NULL ),
	LightingStatusBCB( NULL )
{
}

void WxStatusBarStandard::SetUp()
{
	//////////////////////
	// Bitmaps

	wxImage TempDragGrid;
	TempDragGrid.LoadFile( *FString::Printf( TEXT("%swxres\\DragGrid.bmp"), *GetEditorResourcesDir() ), wxBITMAP_TYPE_BMP );
	DragGridB = wxBitmap(TempDragGrid);
	DragGridB.SetMask( new wxMask( DragGridB, wxColor(192,192,192) ) );

	wxImage TempRotationGrid;
	TempRotationGrid.LoadFile( *FString::Printf( TEXT("%swxres\\RotationGrid.bmp"), *GetEditorResourcesDir() ), wxBITMAP_TYPE_BMP );
	RotationGridB = wxBitmap(TempRotationGrid);
	RotationGridB.SetMask( new wxMask( RotationGridB, wxColor(192,192,192) ) );

	SCCEnabledB.Load( TEXT("SourceControlEnabled.png") );
	SCCDisabledB.Load( TEXT("SourceControlDisabled.png") );
	SCCConnectionProblemB.Load( TEXT("SourceControlConnectionProblem.png") );
	LightingDirtyB.Load( TEXT("LightingDirty.png") );
	LightingNotDirtyB.Load( TEXT("LightingNotDirty.png") );
	PathsDirtyB.Load( TEXT("PathsDirty.png") );
	PathsNotDirtyB.Load( TEXT("PathsNotDirty.png") );

	PackagesNeedCheckout.Load( TEXT("PackagesNeedCheckout.png") );
	PackagesDontNeedCheckout.Load( TEXT("PackagesDontNeedCheckout.png") );
	PackagesNeedCheckoutBCB = new WxBitmapStateButton( this, this, IDSB_PackageCheckoutStatus, wxDefaultPosition, wxDefaultSize, FALSE  ); 
	PackagesNeedCheckoutBCB->AddState( PCS_CheckoutNeeded, &PackagesNeedCheckout );
	PackagesNeedCheckoutBCB->AddState( PCS_CheckoutNotNeeded, &PackagesDontNeedCheckout );
	PackagesNeedCheckoutBCB->SetCurrentState( PCS_CheckoutNotNeeded );
	PackagesNeedCheckoutBCB->SetToolTip( *LocalizeUnrealEd( TEXT("StatusBar_PackageCheckoutNotNeeded") ) );
	//////////////////////
	// Controls

	wxString dummy[1];
	ExecCombo = new WxComboBox( this, ID_EXEC_COMMAND, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, dummy, wxCB_DROPDOWN );

	// Set up the source control status button
	SCCStatusBCB = new WxBitmapStateButton( this, this, IDSB_SCCStatus, wxDefaultPosition, wxDefaultSize, FALSE );
	SCCStatusBCB->AddState( SCCS_Enabled, &SCCEnabledB );
	SCCStatusBCB->AddState( SCCS_Disabled, &SCCDisabledB );
	SCCStatusBCB->AddState( SCCS_ConnectionProblems, &SCCConnectionProblemB );
	SCCStatusBCB->SetCurrentState( SCCS_Disabled );
	SCCStatusBCB->SetToolTip( *LocalizeUnrealEd( TEXT("SourceControlDisabledTooltip") ) );
	
	// Set up the lighting status button
	LightingStatusBCB = new WxBitmapStateButton( this, this, IDSB_LightingStatus, wxDefaultPosition, wxDefaultSize, FALSE );
	LightingStatusBCB->AddState( LS_NotDirty, &LightingNotDirtyB );
	LightingStatusBCB->AddState( LS_Dirty, &LightingDirtyB );
	LightingStatusBCB->SetCurrentState( LS_NotDirty );
	LightingStatusBCB->SetToolTip( *LocalizeUnrealEd( TEXT("LevelLightingNotDirtyTooltip") ) );

	// Set up the paths status button
	PathsStatusBCB = new WxBitmapStateButton( this, this, IDSB_PathsStatus, wxDefaultPosition, wxDefaultSize, FALSE );
	PathsStatusBCB->AddState( PS_Dirty, &PathsDirtyB );
	PathsStatusBCB->AddState( PS_NotDirty, &PathsNotDirtyB );
	PathsStatusBCB->SetCurrentState( PS_NotDirty );
	PathsStatusBCB->SetToolTip( *LocalizeUnrealEd( TEXT("PathsNotDirtyTooltip") ) );

	DragGridST = new wxStaticText( this, IDST_DRAG_GRID, TEXT("XXXX"), wxDefaultPosition, wxSize(-1,-1), wxST_NO_AUTORESIZE );
	RotationGridST = new wxStaticText( this, IDST_ROTATION_GRID, TEXT("XXXX"), wxDefaultPosition, wxSize(-1,-1), wxST_NO_AUTORESIZE );
	ScaleGridST = new wxStaticText( this, wxID_ANY, TEXT("XXXXX"), wxDefaultPosition, wxSize(-1,-1), wxST_NO_AUTORESIZE );
	
	DragGridCB = new wxCheckBox( this, IDCB_DRAG_GRID_TOGGLE, TEXT("") );
	RotationGridCB = new wxCheckBox( this, IDCB_ROTATION_GRID_TOGGLE, TEXT("") );
	ScaleGridCB = new wxCheckBox( this, IDCB_SCALE_GRID_TOGGLE, TEXT("") );
	AutoSaveCB = new wxCheckBox( this, IDCB_AUTOSAVE_TOGGLE, TEXT("") );
	
	DragGridSB = new wxStaticBitmap( this, IDSB_DRAG_GRID, DragGridB );
	RotationGridSB = new wxStaticBitmap( this, IDSB_ROTATION_GRID, RotationGridB );
	
	DragGridMB = new WxMenuButton( this, IDPB_DRAG_GRID, &GApp->EditorFrame->GetDownArrowB(), (wxMenu*) GApp->EditorFrame->GetDragGridMenu() );
	RotationGridMB = new WxMenuButton( this, IDPB_ROTATION_GRID, &GApp->EditorFrame->GetDownArrowB(), (wxMenu*) GApp->EditorFrame->GetRotationGridMenu() );
	AutoSaveMB = new WxMenuButton( this, IDPB_AUTOSAVE_INTERVAL, &GApp->EditorFrame->GetDownArrowB(), (wxMenu*) GApp->EditorFrame->GetAutoSaveOptionsMenu() );

	ScaleGridMB = new WxMenuButton( this, IDPB_SCALE_GRID, &GApp->EditorFrame->GetDownArrowB(), (wxMenu*) GApp->EditorFrame->GetScaleGridMenu() );
	
	ActorNameST = new wxStaticText( this, wxID_ANY, TEXT(""));

	DrawScaleTextCtrl = new WxScaleTextCtrl( this, IDTB_DRAWSCALE, TEXT("DrawScaleTextCtrl"), wxDefaultPosition, wxSize( -1, -1 ), wxST_NO_AUTORESIZE );//wxTE_PROCESS_ENTER
	DrawScaleXTextCtrl = new WxScaleTextCtrl( this, IDTB_DRAWSCALEX, TEXT("DrawScaleXTextCtrl"), wxDefaultPosition, wxSize( -1, -1 ), wxST_NO_AUTORESIZE );//wxTE_PROCESS_ENTER
	DrawScaleYTextCtrl = new WxScaleTextCtrl( this, IDTB_DRAWSCALEY, TEXT("DrawScaleYTextCtrl"), wxDefaultPosition, wxSize( -1, -1 ), wxST_NO_AUTORESIZE );//wxTE_PROCESS_ENTER
	DrawScaleZTextCtrl = new WxScaleTextCtrl( this, IDTB_DRAWSCALEZ, TEXT("DrawScaleZTextCtrl"), wxDefaultPosition, wxSize( -1, -1 ), wxST_NO_AUTORESIZE );//wxTE_PROCESS_ENTER
	
	DrawScaleTextCtrl->Link( DRAWSCALE, DrawScaleXTextCtrl, DrawScaleZTextCtrl );
	DrawScaleXTextCtrl->Link( DRAWSCALE_X, DrawScaleYTextCtrl, DrawScaleTextCtrl );
	DrawScaleYTextCtrl->Link( DRAWSCALE_Y, DrawScaleZTextCtrl, DrawScaleXTextCtrl );
	DrawScaleZTextCtrl->Link( DRAWSCALE_Z, DrawScaleTextCtrl, DrawScaleYTextCtrl );

	

	DragGridCB->SetToolTip( *LocalizeUnrealEd("ToolTip_16") );
	RotationGridCB->SetToolTip( *LocalizeUnrealEd("ToolTip_17") );
	ScaleGridCB->SetToolTip( *LocalizeUnrealEd("ToolTip_SnapScaling") );
	AutoSaveCB->SetToolTip( *LocalizeUnrealEd("ToolTip_Autosave") );
	AutoSaveMB->SetToolTip( *LocalizeUnrealEd("ToolTip_AutosaveMenu") );
	DragGridMB->SetToolTip( *LocalizeUnrealEd("ToolTip_18") );
	RotationGridMB->SetToolTip( *LocalizeUnrealEd("ToolTip_19") );
	DrawScaleTextCtrl->SetToolTip( *LocalizeUnrealEd("ToolTip_DrawScale") );
	DrawScaleXTextCtrl->SetToolTip( *LocalizeUnrealEd("ToolTip_DrawScale3DX") );
	DrawScaleYTextCtrl->SetToolTip( *LocalizeUnrealEd("ToolTip_DrawScale3DY") );
	DrawScaleZTextCtrl->SetToolTip( *LocalizeUnrealEd("ToolTip_DrawScale3DZ") );

	//////////////////////
	// Now that we have the controls created, figure out how large each pane should be.

	DragTextWidth = DragGridST->GetRect().GetWidth();
	RotationTextWidth = RotationGridST->GetRect().GetWidth();
	ScaleTextWidth = ScaleGridST->GetRect().GetWidth();

	// Create autosave bitmaps
	AutosaveEnabled = WxBitmap(TEXT("Autosave_Enabled"));
	AutosaveDisabled = WxBitmap(TEXT("Autosave_Disabled"));
	AutosaveSoon = WxBitmap(TEXT("Autosave_Soon"));

	AutoSaveSB = new wxStaticBitmap(this, IDSB_AUTOSAVE, AutosaveEnabled);
	AutoSaveSB->SetSize(wxSize(16,16));

	UpdateStatusWidths();

	//////////////////////
	// Update with initial values and resize everything.

	UpdateUI();

	RefreshPositionAndSize();
}

void WxStatusBarStandard::UpdateStatusWidths()
{
	const INT ScaleWidths = 60;
	const INT ExecComboPane = -1;
	const INT SourceControlPane = 30;
	const INT LightingPane = 30;
	const INT PathsPane = 30;
	const INT PackageCheckoutPane = 30;
	const INT ActorNamePaneWidth = ActorNameST->GetSize().x;
	const INT ActorNamePane = ActorNamePaneWidth ? ActorNamePaneWidth + 4 : -1;
	const INT MousePosPane = -1;
	const INT DragGridPane = 2+ DragGridB.GetWidth() +2+ DragTextWidth +2+ DragGridCB->GetRect().GetWidth() +2+ DragGridMB->GetRect().GetWidth() +1;
	const INT RotationGridPane = 2+ RotationGridB.GetWidth() +2+ RotationTextWidth +2+ RotationGridCB->GetRect().GetWidth() +2+ RotationGridMB->GetRect().GetWidth() +3;
	const INT ScaleGridPane = 2+ ScaleTextWidth +2+ ScaleGridCB->GetRect().GetWidth() +2+ ScaleGridMB->GetRect().GetWidth() +3;
	const INT AutosavePane = 62;
	const INT BufferPane = 20;
	const INT Widths[] = { ExecComboPane, SourceControlPane, LightingPane, PathsPane, PackageCheckoutPane, ActorNamePane, ActorNamePane, MousePosPane, ScaleWidths, ScaleWidths, ScaleWidths, ScaleWidths, DragGridPane, RotationGridPane, ScaleGridPane, AutosavePane, BufferPane };
	SetFieldsCount( sizeof(Widths)/sizeof(INT), Widths );
}

void WxStatusBarStandard::UpdateUI()
{
	if( !DragGridCB )
	{
		return;
	}

	DragGridCB->SetValue( GEditor->Constraints.GridEnabled );
	RotationGridCB->SetValue( GEditor->Constraints.RotGridEnabled );
	ScaleGridCB->SetValue( GEditor->Constraints.SnapScaleEnabled );

	DragGridST->SetLabel( *FString::Printf( TEXT("%g"), GEditor->Constraints.GetGridSize() ) );

	// For integral rotation snap amounts, we'll just display the number.  For snap amounts with fractions,
	// we'll display a '~' character next to the rounded integer.
	const FLOAT RotationSnapDegrees = GEditor->Constraints.RotGridSize.Pitch / (16384.0f / 90.0f );
	const UBOOL bIsExactValue = ( appFractional( RotationSnapDegrees ) < 0.001f );
	const INT RoundedRotationSnapDegrees = appRound( RotationSnapDegrees );
	RotationGridST->SetLabel(
		*FString::Printf( TEXT("%s%i"),
						  bIsExactValue ? TEXT( "" ) : TEXT( "~" ),
						  RoundedRotationSnapDegrees ) );

	ScaleGridST->SetLabel( *FString::Printf( TEXT("%i%c"), GEditor->Constraints.ScaleGridSize, '%' ));

	FLOAT DrawScale = 1.0f;
	FVector DrawScale3D( 1.0f, 1.0f, 1.0f );
	UBOOL bMultipleDrawScaleValues[4];
	FString NewLabels[ 4 ];
	if ( GetDrawScaleFromSelectedActors( DrawScale, DrawScale3D, bMultipleDrawScaleValues ) )
	{
		NewLabels[ 0 ] = bMultipleDrawScaleValues[0] ? LocalizeUnrealEd("Multiple") : FString::Printf( TEXT("%.4f"), DrawScale );
		NewLabels[ 1 ] = bMultipleDrawScaleValues[1] ? LocalizeUnrealEd("Multiple") : FString::Printf( TEXT("%.4f"), DrawScale3D.X );
		NewLabels[ 2 ] = bMultipleDrawScaleValues[2] ? LocalizeUnrealEd("Multiple") : FString::Printf( TEXT("%.4f"), DrawScale3D.Y );
		NewLabels[ 3 ] = bMultipleDrawScaleValues[3] ? LocalizeUnrealEd("Multiple") : FString::Printf( TEXT("%.4f"), DrawScale3D.Z );
	}
	else
	{
		const FString LocalizedNone( LocalizeUnrealEd("None") );
		NewLabels[ 0 ] = LocalizedNone;
		NewLabels[ 1 ] = LocalizedNone;
		NewLabels[ 2 ] = LocalizedNone;
		NewLabels[ 3 ] = LocalizedNone;
	}

	// If text was selected in the edit box then make sure to reselect it after we update the label
	{
		UBOOL bNeedsReselect = ( DrawScaleTextCtrl->GetStringSelection().length() > 0 );
		DrawScaleTextCtrl->SetLabel( *NewLabels[ 0 ] );
		if( bNeedsReselect )
		{
			DrawScaleTextCtrl->SetSelection( -1, -1 );
		}
	}

	if( appStrcmp( *NewLabels[ 1 ], DrawScaleXTextCtrl->GetLabel() ) != 0 )
	{			 
		UBOOL bNeedsReselect = ( DrawScaleXTextCtrl->GetStringSelection().length() > 0 );
		DrawScaleXTextCtrl->SetLabel( *NewLabels[ 1 ] );
		if( bNeedsReselect )
		{
			DrawScaleXTextCtrl->SetSelection( -1, -1 );
		}
	}
	if( appStrcmp( *NewLabels[ 2 ], DrawScaleYTextCtrl->GetLabel() ) != 0 )
	{
		UBOOL bNeedsReselect = ( DrawScaleYTextCtrl->GetStringSelection().length() > 0 );
		DrawScaleYTextCtrl->SetLabel( *NewLabels[ 2 ] );
		if( bNeedsReselect )
		{
			DrawScaleYTextCtrl->SetSelection( -1, -1 );
		}
	}
	if( appStrcmp( *NewLabels[ 3 ], DrawScaleZTextCtrl->GetLabel() ) != 0 )
	{
		UBOOL bNeedsReselect = ( DrawScaleZTextCtrl->GetStringSelection().length() > 0 );
		DrawScaleZTextCtrl->SetLabel( *NewLabels[ 3 ] );
		if( bNeedsReselect )
		{
			DrawScaleZTextCtrl->SetSelection( -1, -1 );
		}
	}
	

	// Update Actor Name Static Text.
	USelection* SelectedActors = GEditor->GetSelectedActors();
	const INT NumActors = SelectedActors->Num();
	
	if(NumActors == 0)
	{
		ActorNameST->SetLabel(TEXT(""));
	}
	else if(NumActors == 1)
	{
		AActor* Actor					= SelectedActors->GetTop<AActor>();
		if ( Actor )
		{
			ULevel* Level					= Actor->GetLevel();
			ULevelStreaming* StreamingLevel	= FLevelUtils::FindStreamingLevel( Level ); 

			const FString LevelName( StreamingLevel ? StreamingLevel->PackageName.ToString() : LocalizeUnrealEd( "PersistentLevel" ) );
			const FString LabelString( FString::Printf(LocalizeSecure(LocalizeUnrealEd("ActorSelected"), *LevelName, *Actor->GetName(),
				Actor->GetActorMetrics(METRICS_TRIS), Actor->GetActorMetrics(METRICS_VERTS), Actor->GetActorMetrics(METRICS_SECTIONS)) ));

			ActorNameST->SetLabel( *LabelString );
			ActorNameST->SetToolTip( *LabelString );
		}
		else
		{
			ActorNameST->SetLabel(TEXT(""));
		}
	}
	else
	{
		wxString ActorNameString;
		wxString ActorToolTip;
		
		// The actual number of actors displayed in the status bar may be different from the number of selected actors
		// if prefab instances are involved (as prefabs should count as one object)
		INT NumActorsToDisplay = 0;
		
		// Loop through all actors and see if they are the same type, if they are then display a more 
		// specific string using their class name.
		UBOOL bActorsHaveSameClass = TRUE;
		FString ActorClassString;

		INT TotalVertices(0), TotalTris(0), TotalSections(0);

		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* NthActor = static_cast<AActor*>( *It );
			checkSlow( NthActor->IsA(AActor::StaticClass()) );

			// If the actor is in a prefab then we don't want to consider it for the actor count or
			// for the same class check (we also have to check if the actor is itself a prefab instance
			// because IsInPrefabInstance() will return TRUE for an actor which is actually a prefab instance
			// itself)
			if ( !NthActor->IsInPrefabInstance() || NthActor->IsA( APrefabInstance::StaticClass() ) )
			{
				++NumActorsToDisplay;

				TotalVertices += NthActor->GetActorMetrics(METRICS_VERTS);
				TotalTris += NthActor->GetActorMetrics(METRICS_TRIS);
				TotalSections += NthActor->GetActorMetrics(METRICS_SECTIONS);

				if ( ActorClassString.Len() == 0 )
				{
					ActorClassString = NthActor->GetClass()->GetName();
					ActorToolTip = *NthActor->GetName();
				}
				else if ( bActorsHaveSameClass )
				{	
					const INT StrCompare = appStricmp( *ActorClassString, *NthActor->GetClass()->GetName() );

					if( StrCompare != 0 )
					{
						bActorsHaveSameClass = FALSE;
						continue;
					}

					ActorToolTip += TEXT(", ");
					ActorToolTip += *NthActor->GetName();
				}
			}
		}


		if( bActorsHaveSameClass && ActorClassString.Len() )
		{
			ActorNameString.Printf(LocalizeSecure(LocalizeUnrealEd("ActorsClassSelected"), NumActorsToDisplay, *ActorClassString, TotalTris, TotalVertices, TotalSections));
		}
		else
		{
			ActorNameString.Printf(LocalizeSecure(LocalizeUnrealEd("ActorsSelected"), NumActorsToDisplay, TotalTris, TotalVertices, TotalSections));
		}

		ActorNameST->SetToolTip(ActorToolTip);
		ActorNameST->SetLabel(ActorNameString);

		UpdateStatusWidths();
		RefreshPositionAndSize();
	}
}


/** Update the position and size of the status bar */
void WxStatusBarStandard::RefreshPositionAndSize()
{
	wxSizeEvent DummyEvent;
	OnSize( DummyEvent );
}


/**
* Overload for the default SetStatusText function.  This overload maps
* any text set to field 0, to field 5 because the command combo box takes up
* field 0.
*/
void WxStatusBarStandard::SetStatusText(const wxString& InText, INT InFieldIdx )
{
	if(InFieldIdx == 0)
	{
		// Shouldn't be necessary due to the SetStatusBarPane call in EditorFrame, but just to be safe
		InFieldIdx = 5;
	}

	WxStatusBar::SetStatusText(InText, InFieldIdx);
}

void WxStatusBarStandard::OnDragGridToggleClick( wxCommandEvent& InEvent )
{
	GEditor->Constraints.GridEnabled = !GEditor->Constraints.GridEnabled;
}

void WxStatusBarStandard::OnRotationGridToggleClick( wxCommandEvent& InEvent )
{
	GEditor->Constraints.RotGridEnabled = !GEditor->Constraints.RotGridEnabled;
}

void WxStatusBarStandard::OnScaleGridToggleClick( wxCommandEvent& InEvent )
{
	GEditor->Constraints.SnapScaleEnabled = !GEditor->Constraints.SnapScaleEnabled;
}

void WxStatusBarStandard::OnAutosaveToggleClick( wxCommandEvent& InEvent )
{
	GEditor->AccessUserSettings().bAutoSaveEnable = !GEditor->GetUserSettings().bAutoSaveEnable;

	//reset the counter if autosave has been enabled
	if (GEditor->GetUserSettings().bAutoSaveEnable) 
	{
		GUnrealEd->AutosaveCount = 0;
	}

	GEditor->SaveUserSettings();
}

void WxStatusBarStandard::OnSize( wxSizeEvent& InEvent )
{
	wxRect rect;

	//////////////////////
	// Exec combo

	GetFieldRect( FIELD_ExecCombo, rect );
	ExecCombo->SetSize( rect.x+2, rect.y+2, rect.GetWidth()-4, rect.GetHeight()-4 );

	//////////////////////
	// Source control status
	if ( SCCStatusBCB )
	{
		GetFieldRect( FIELD_SourceControl, rect );
		SCCStatusBCB->SetSize( rect.x, rect.y, rect.GetWidth(), rect.GetHeight() );
	}

	//////////////////////
	// Lighting status
	if ( LightingStatusBCB )
	{
		GetFieldRect( FIELD_Lighting, rect );
		LightingStatusBCB->SetSize( rect.x, rect.y, rect.GetWidth(), rect.GetHeight() );
	}

	//////////////////////
	// Paths status
	if ( PathsStatusBCB )
	{
		GetFieldRect( FIELD_Paths, rect );
		PathsStatusBCB->SetSize( rect.x, rect.y, rect.GetWidth(), rect.GetHeight() );
	}

	//////////////////////
	// Package checkout status
	if( PackagesNeedCheckoutBCB )
	{
		GetFieldRect( FIELD_PackageCheckout, rect );
		PackagesNeedCheckoutBCB->SetSize( rect.x, rect.y, rect.GetWidth(), rect.GetHeight() );
	}

	//////////////////////
	// DrawScale text controls.

	if( DrawScaleTextCtrl )
	{
		GetFieldRect( FIELD_DrawScale, rect );
		DrawScaleTextCtrl->SetSize( rect.x, rect.y, rect.GetWidth(), rect.GetHeight() );
	}
	if( DrawScaleXTextCtrl )
	{
		GetFieldRect( FIELD_DrawScaleX, rect );
		DrawScaleXTextCtrl->SetSize( rect.x, rect.y, rect.GetWidth(), rect.GetHeight() );
	}
	if( DrawScaleYTextCtrl )
	{
		GetFieldRect( FIELD_DrawScaleY, rect );
		DrawScaleYTextCtrl->SetSize( rect.x, rect.y, rect.GetWidth(), rect.GetHeight() );
	}
	if( DrawScaleZTextCtrl )
	{
		GetFieldRect( FIELD_DrawScaleZ, rect );
		DrawScaleZTextCtrl->SetSize( rect.x, rect.y, rect.GetWidth(), rect.GetHeight() );
	}

	//////////////////////
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

	//////////////////////
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

	//////////////////////
	// Scale grid
	if(ScaleGridST)
	{
		GetFieldRect( FIELD_Scale_Grid, rect );

		INT Left = rect.x + 2;
		ScaleGridST->SetSize( Left, rect.y+2, ScaleTextWidth, rect.height-4 );
		Left += ScaleTextWidth + 2;
		ScaleGridCB->SetSize( Left, rect.y+2, -1, rect.height-4 );
		Left += ScaleGridCB->GetSize().GetWidth() + 2;
		ScaleGridMB->SetSize( Left, rect.y+1, 13, rect.height-2 );
	}

	//////////////////////
	// Selected Actors
	if(ActorNameST)
	{
		GetFieldRect( FIELD_ActorName, rect );

		wxSize LocalSize = ActorNameST->GetSize();
		ActorNameST->SetSize(rect.x + 2, rect.GetY() + (rect.height - LocalSize.GetHeight()) / 2, LocalSize.GetWidth(), LocalSize.GetHeight());
	}

	INT Left = 0;

	// Autosave Static Bitmap
	if(AutoSaveSB)
	{
		GetFieldRect( FIELD_Autosave, rect );
		Left = rect.x + 2;
		AutoSaveSB->SetSize(Left, 1+rect.GetY() + (rect.height - 16) / 2, 16, rect.height-4);
		Left += AutoSaveSB->GetSize().GetWidth() + 12;
	}

	// Autosave Check Box
	if(AutoSaveCB)
	{
		AutoSaveCB->SetSize(Left, 1+rect.GetY() + (rect.height - 16) / 2, 16, rect.height-4);
		Left += AutoSaveCB->GetSize().GetWidth();
	}

	// Autosave Menu Bar
	if (AutoSaveMB) 
	{
		AutoSaveMB->SetSize(Left, 1+rect.GetY() + (rect.height - 16) / 2, 13, rect.height-2);
	}
}

void WxStatusBarStandard::OnDrawScale( wxCommandEvent& In )
{
	SendDrawScaleToSelectedActors( *DrawScaleTextCtrl, DRAWSCALE );
}

void WxStatusBarStandard::OnDrawScaleX( wxCommandEvent& In )
{
	SendDrawScaleToSelectedActors( *DrawScaleXTextCtrl, DRAWSCALE_X );
}

void WxStatusBarStandard::OnDrawScaleY( wxCommandEvent& In )
{
	SendDrawScaleToSelectedActors( *DrawScaleYTextCtrl, DRAWSCALE_Y );
}

void WxStatusBarStandard::OnDrawScaleZ( wxCommandEvent& In )
{
	SendDrawScaleToSelectedActors( *DrawScaleZTextCtrl, DRAWSCALE_Z );
}

void WxStatusBarStandard::OnExecComboEnter( wxCommandEvent& In )
{
	const FString exec = (const TCHAR*)ExecCombo->GetValue();
	GEngine->Exec( *exec );

	// Clear the text after the user pressed enter
	ExecCombo->Clear();

	AddToExecCombo( exec );
}

void WxStatusBarStandard::OnExecComboSelChange( wxCommandEvent& In )
{
	const FString exec = (const TCHAR*)ExecCombo->GetString( ExecCombo->GetSelection() );
	GEngine->Exec( *exec );
	ExecCombo->Clear();
}

void WxStatusBarStandard::AddToExecCombo( const FString& InExec )
{
	// If the string isn't already in the combo box list, add it.
	if( ExecCombo->FindString( *InExec ) == -1 )
	{
		ExecCombo->Append( *InExec );
	}
}

/**
 * Sets the mouse worldspace position text field to the text passed in.
 *
 * @param StatusText	 String to use as the new text for the worldspace position field.
 */
void WxStatusBarStandard::SetMouseWorldspacePositionText( const TCHAR* StatusText )
{
	SetStatusText( StatusText, FIELD_MouseWorldspacePosition );
}

/**
 * Called by wxWidgets when the user clicks the SCC status button
 *
 * @param	In	Event automatically generated by wxWidgets when the user clicks the SCC status button
 */
void WxStatusBarStandard::OnClickSCCStatusButton( wxCommandEvent& In )
{
	// If source control is disabled, wipe out the user's SCC settings (so the connection dialog can auto-acquire them),
	// and attempt to enable/connect to source control
	if ( SCCStatusBCB->GetCurrentState()->ID == SCCS_Disabled )
	{
		// Preserve the user's ExceptOnWarning preference (the other settings will be auto-acquired)
		UBOOL bPreservedExceptOnWarningSetting = TRUE;
		GConfig->GetBool( TEXT("SourceControl"), TEXT("ExceptOnWarning"), bPreservedExceptOnWarningSetting, GEditorUserSettingsIni );
		
		// Reset the user's source control settings, forcibly setting disabled to FALSE, and restoring the ExceptOnWarning preference
		GConfig->EmptySection( TEXT("SourceControl"), GEditorUserSettingsIni );
		GConfig->SetBool( TEXT("SourceControl"), TEXT("Disabled"), FALSE, GEditorUserSettingsIni );
		GConfig->SetBool( TEXT("SourceControl"), TEXT("ExceptOnWarning"), bPreservedExceptOnWarningSetting, GEditorUserSettingsIni );
		
#if HAVE_SCC
		// Attempt to connect to source control
		FSourceControl::Init();

		// If we were able to successfully connect, the content browser needs to refresh its SCC state
		if ( FSourceControl::IsEnabled() )
		{
			GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateSCCState ) );
		}
#endif //HAVE_SCC
	}
}

/**
 * Called by wxWidgets when the user clicks the lighting status button
 *
 * @param	In	Event automatically generated by wxWidgets when the user clicks the lighting status button
 */
void WxStatusBarStandard::OnClickLightingStatusButton( wxCommandEvent& In )
{
	// If the lighting is dirty, clicking the icon should trigger a lighting build
	if ( LightingStatusBCB->GetCurrentState()->ID == LS_Dirty )
	{
		GUnrealEd->Exec( TEXT("BUILDLIGHTING") );
	}
}

/**
 * Called by wxWidgets when the user clicks the paths status button
 *
 * @param	In	Event automatically generated by wxWidgets when the user clicks the paths status button
 */
void WxStatusBarStandard::OnClickPathsStatusButton( wxCommandEvent& In )
{
	// If paths are dirty, clicking the icon should trigger a path build
	if ( PathsStatusBCB->GetCurrentState()->ID == PS_Dirty )
	{
		GUnrealEd->Exec( TEXT("BUILDPATHS") );
	}
}

/**
 * Called by wxWidgets when the user clicks the package checkout status button
 *
 * @param 	In	Event automatically generated by wxWidgets when the user clicks the package checkout status button
 */
void WxStatusBarStandard::OnClickPackageCheckoutStatusButton( wxCommandEvent& In )
{
	// If packages need to be checked out, prompt the user
	if ( PackagesNeedCheckoutBCB->GetCurrentState()->ID == PCS_CheckoutNeeded )
	{
		GUnrealEd->PromptToCheckoutModifiedPackages( TRUE );
	}
}

/**
 * Called automatically by wxWidgets to update the UI
 *
 * @param	Event	Event automatically generated by wxWidgets to update the UI
 */
void WxStatusBarStandard::OnUpdateUI( wxUpdateUIEvent &Event )
{
	// Update autosave image
	static INT AutosaveSeconds = -1;
	const UBOOL bCanAutosave = GEditor->GetUserSettings().bAutoSaveEnable;

	AutoSaveCB->SetValue(GEditor->GetUserSettings().bAutoSaveEnable);

	if(bCanAutosave)
	{
		const UBOOL bAutosaveSoon = GUnrealEd->AutoSaveSoon();

		if(bAutosaveSoon)
		{
			if(AutoSaveSB->GetBitmap().GetHandle() != AutosaveSoon.GetHandle())
			{
				AutoSaveSB->SetBitmap(AutosaveSoon);
			}

			INT NewSeconds = GUnrealEd->GetTimeTillAutosave();
			if(AutosaveSeconds != NewSeconds)
			{
				AutosaveSeconds = NewSeconds;
				INT Sec = AutosaveSeconds % 60;

				AutoSaveSB->SetToolTip(*FString::Printf(LocalizeSecure(LocalizeUnrealEd("Autosave_Soon"), Sec)));
			}
		}
		else
		{
			if(AutoSaveSB->GetBitmap().GetHandle() != AutosaveEnabled.GetHandle())
			{
				AutoSaveSB->SetBitmap(AutosaveEnabled);
			}

			INT NewSeconds = GUnrealEd->GetTimeTillAutosave();
			if(AutosaveSeconds != NewSeconds)
			{
				AutosaveSeconds = NewSeconds;

				INT Min = AutosaveSeconds / 60;
				INT Sec = AutosaveSeconds % 60;

				AutoSaveSB->SetToolTip(*FString::Printf(LocalizeSecure(LocalizeUnrealEd("Autosave_Enabled"), Min,Sec)));
			}
		}
	}
	else
	{
		if(AutoSaveSB->GetBitmap().GetHandle() != AutosaveDisabled.GetHandle())
		{
			AutoSaveSB->SetBitmap(AutosaveDisabled);
			AutoSaveSB->SetToolTip(*LocalizeUnrealEd("Autosave_Disabled"));
		}
	}


	// Does lighting currently need to be rebuilt for any levels in world?  If so then we'll
	// display an indicator on the status bar
	UBOOL bAnyLevelsDirty = FALSE;
	if( GWorld )
	{
		bAnyLevelsDirty = GWorld->GetWorldInfo()->bMapNeedsLightingFullyRebuilt;
		for( INT CurLevelIndex = 0; CurLevelIndex < GWorld->Levels.Num() && !bAnyLevelsDirty; ++CurLevelIndex )
		{
			const ULevel* CurLevel = GWorld->Levels( CurLevelIndex );
			if ( CurLevel && CurLevel->bGeometryDirtyForLighting )
			{
				bAnyLevelsDirty = TRUE;
				break;
			}
		}
	}

	// Determine if the status of the lighting is different than what the UI currently displays;
	// if so, update the icon and tooltip appropriately
	const INT LightingID = bAnyLevelsDirty ? LS_Dirty : LS_NotDirty;
	if ( LightingStatusBCB->GetCurrentState()->ID != LightingID )
	{
		LightingStatusBCB->SetCurrentState( LightingID );
		if ( LightingID == LS_Dirty )
		{
			LightingStatusBCB->SetToolTip( *LocalizeUnrealEd( TEXT("LevelLightingDirtyTooltip") ) );
		}
		else
		{
			LightingStatusBCB->SetToolTip( *LocalizeUnrealEd( TEXT("LevelLightingNotDirtyTooltip") ) );
		}
	}

	// Determine if the status of the paths is different than what the UI currently displays;
	// if so, update the icon and tooltip appropriately
	const INT PathStatusID = GWorld && GWorld->GetWorldInfo()->bPathsRebuilt ? PS_NotDirty : PS_Dirty;
	if ( PathsStatusBCB->GetCurrentState()->ID != PathStatusID )
	{
		PathsStatusBCB->SetCurrentState( PathStatusID );
		PathsStatusBCB->SetToolTip( PathStatusID == PS_Dirty ? *LocalizeUnrealEd( TEXT("PathsDirtyTooltip") ) : *LocalizeUnrealEd( TEXT("PathsNotDirtyTooltip") ) );
	}


	// Display the current level and current level grid volume in the status bar
	if( GWorld != NULL && GWorld->CurrentLevel != NULL )
	{
		FString CurrentLevelName = GWorld->CurrentLevel->GetOutermost()->GetName();
		if( GWorld->CurrentLevel == GWorld->PersistentLevel )
		{
			CurrentLevelName = *LocalizeUnrealEd( "PersistentLevel" );
		}

		if( GWorld->CurrentLevelGridVolume != NULL )
		{
			SetStatusText( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MainStatusBar_CurrentLevelGridVolumeF" ), *GWorld->CurrentLevelGridVolume->GetLevelGridVolumeName(), *CurrentLevelName ) ), FIELD_Status );
		}
		else
		{
			SetStatusText( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MainStatusBar_CurrentLevelF" ), *CurrentLevelName ) ), FIELD_Status );
		}
	}
	else
	{
		// No current level
		SetStatusText( TEXT( "" ), FIELD_Status );
	}


#if HAVE_SCC
	// Determine if the status of the source control connection is different than what the UI currently displays;
	// if so, update icon and tooltip appropriately
	const INT SourceControlID = FSourceControl::IsEnabled() ? ( FSourceControl::IsServerAvailable() ? SCCS_Enabled : SCCS_ConnectionProblems ) : SCCS_Disabled;
	if ( SCCStatusBCB->GetCurrentState()->ID != SourceControlID )
	{
		SCCStatusBCB->SetCurrentState( SourceControlID );
		if ( SourceControlID == SCCS_Enabled )
		{
			SCCStatusBCB->SetToolTip( *LocalizeUnrealEd( TEXT("SourceControlEnabledTooltip") ) );
		}
		else if ( SourceControlID == SCCS_Disabled )
		{
			SCCStatusBCB->SetToolTip( *LocalizeUnrealEd( TEXT("SourceControlDisabledTooltip") ) );
		}
		else
		{
			SCCStatusBCB->SetToolTip( *LocalizeUnrealEd( TEXT("SourceControlConnectionProblemsTooltip") ) );
		}
	}

	if( FSourceControl::IsEnabled() )
	{
		// Check to see if any dirty packages need to be checked out.
		// If there are dirty packages, swap the bitmap and tooltip.
		const UBOOL bPackagesNeedCheckout = GUnrealEd->DoDirtyPackagesNeedCheckout();
		if( bPackagesNeedCheckout )
		{
			if( PackagesNeedCheckoutBCB->GetCurrentState()->ID != PCS_CheckoutNeeded )
			{
				PackagesNeedCheckoutBCB->SetCurrentState( PCS_CheckoutNeeded );
				PackagesNeedCheckoutBCB->SetToolTip( *LocalizeUnrealEd( TEXT("StatusBar_PackageCheckoutNeeded") ) );
			}
		}
		else
		{
			if( PackagesNeedCheckoutBCB->GetCurrentState()->ID != PCS_CheckoutNotNeeded )
			{
				PackagesNeedCheckoutBCB->SetCurrentState( PCS_CheckoutNotNeeded );
				PackagesNeedCheckoutBCB->SetToolTip( *LocalizeUnrealEd( TEXT("StatusBar_PackageCheckoutNotNeeded") ) );
			}
		}
	}

#endif // #if HAVE_SCC

}
