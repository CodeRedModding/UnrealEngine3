/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "PropertyWindow.h"

extern class WxDlgAddSpecial* GDlgAddSpecial;

/*-----------------------------------------------------------------------------
	WxDlgBrushBuilder.
-----------------------------------------------------------------------------*/

class WxDlgBrushBuilder : 
	public wxDialog,
	// The interface for receiving notifications
	public FCallbackEventDevice

{
public:
	WxDlgBrushBuilder()
	{
		const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_BRUSH_BUILDER") );
		check( bSuccess );

		wxWindow* win = (wxWindow*)FindWindow( XRCID( "ID_PROPERTY_WINDOW" ) );
		check( win );

		//Must be done before property window creation, otherwise search text control gets localized incorrectly
		FLocalizeWindow( this );

		// Attach our property window to the placeholder window
		wxRect rc = win->GetRect();
		rc.SetX( 0 );
		rc.SetY( 0 );

		const INT PropertyWindowID = -1;
		const UBOOL bShowPropertyWindowTools = FALSE;

		PropertyWindow = new WxPropertyWindowHost;
		PropertyWindow->Create(win, GUnrealEd, PropertyWindowID, bShowPropertyWindowTools);
		PropertyWindow->SetSize( rc );

		// For Brush building dialogs, we want the ENTER key to build the brush, so we disallow the property
		// window from intercepting the ENTER key itself
		PropertyWindow->SetFlags( EPropertyWindowFlags::AllowEnterKeyToApplyChanges, FALSE );

		FWindowUtil::LoadPosSize( TEXT("DlgBrushBuilder"), this );

		GCallbackEvent->Register(CALLBACK_ObjectPropertyChanged,this);
	}

	~WxDlgBrushBuilder()
	{
		GCallbackEvent->Unregister(CALLBACK_ObjectPropertyChanged,this);

		FWindowUtil::SavePosSize( TEXT("DlgBrushBuilder"), this );
		delete PropertyWindow;
	}

	UBOOL Show( UBrushBuilder* InBrushBuilder, UBOOL bInShow = TRUE )
	{
		BrushBuilder = InBrushBuilder;
		PropertyWindow->SetObject( BrushBuilder, EPropertyWindowFlags::NoFlags );
		SetTitle( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("BrushBuilderCaption"), *LocalizeUnrealEd( *BrushBuilder->ToolTip )) ) );

		const bool bShouldShow = (bInShow == TRUE);
		return wxDialog::Show( bShouldShow );
	}
private:
	using wxWindow::Show;		// Hide parent implementation
public:


	/**
	 * Routes the event to the appropriate handlers
	 *
	 * @param InObject the relevant object for this event
	 */
	void Send(ECallbackEventType InType, UObject* InObject)
	{
		if (InObject == BrushBuilder)
		{
			switch( InType )
			{
				case CALLBACK_ObjectPropertyChanged:
					BrushBuilder->eventBuild();
					GEditorModeTools().MapChangeNotify();
					break;
			}
		}
	}

private:
	UBrushBuilder* BrushBuilder;
	WxPropertyWindowHost* PropertyWindow;

	void OnOK( wxCommandEvent& In )
	{
		PropertyWindow->FinalizeValues();
		BrushBuilder->eventBuild();
		GEditorModeTools().MapChangeNotify();
	}

	void OnCancel( wxCommandEvent& In )
	{
		Destroy();
	}

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(WxDlgBrushBuilder, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgBrushBuilder::OnOK )
	EVT_BUTTON( wxID_CANCEL, WxDlgBrushBuilder::OnCancel )
END_EVENT_TABLE()

/*-----------------------------------------------------------------------------
	WxButtonGroupButton.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxButtonGroupButton, wxEvtHandler )
END_EVENT_TABLE()

WxButtonGroupButton::WxButtonGroupButton()
{
	check(0);		// wrong ctor
}

WxButtonGroupButton::WxButtonGroupButton( wxWindow* parent, wxWindowID id, WxBitmap& InBitmap, EButtonType InButtonType, UClass* InClass, INT InStartMenuID )
	: wxEvtHandler()
{
	Button = new WxBitmapButton( parent, id, InBitmap );
	ButtonType = InButtonType;
	Class = InClass;
	StartMenuID = InStartMenuID;
	BrushBuilder = NULL;
}

WxButtonGroupButton::WxButtonGroupButton( wxWindow* parent, wxWindowID id, WxBitmap& InBitmapOff, WxBitmap& InBitmapOn, EButtonType InButtonType, UClass* InClass, INT InStartMenuID )
	: wxEvtHandler()
{
	Button = new WxBitmapCheckButton( parent, parent, id, &InBitmapOff, &InBitmapOn );
	ButtonType = InButtonType;
	Class = InClass;
	StartMenuID = InStartMenuID;
	BrushBuilder = NULL;
}

WxButtonGroupButton::~WxButtonGroupButton()
{
}

/** 
 * Serializes the BrushBuilder reference so it doesn't get garbage collected.
 *
 * @param Ar	FArchive to serialize with
 */
void WxButtonGroupButton::Serialize(FArchive& Ar)
{
	Ar << BrushBuilder;
}

/*-----------------------------------------------------------------------------
	WxButtonGroup.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxButtonGroup, wxWindow )
	EVT_SIZE( WxButtonGroup::OnSize )
	EVT_RIGHT_DOWN( WxButtonGroup::OnRightButtonDown )
	EVT_COMMAND_RANGE( IDM_VolumeClasses_START, IDM_VolumeClasses_END, wxEVT_COMMAND_MENU_SELECTED, WxButtonGroup::OnAddVolumeClass )
	EVT_BUTTON( IDM_DEFAULT, WxButtonGroup::OnModeCamera )
	EVT_BUTTON( IDM_GEOMETRY, WxButtonGroup::OnModeGeometry )
	EVT_BUTTON( IDM_STATICMESH, WxButtonGroup::OnModeStaticMesh )
	EVT_BUTTON( IDM_TERRAIN, WxButtonGroup::OnModeTerrain )
	EVT_BUTTON( IDM_TEXTURE, WxButtonGroup::OnModeTexture )
	EVT_BUTTON( IDM_COVEREDIT, WxButtonGroup::OnModeCoverEdit )
	EVT_BUTTON( IDM_EditorMode_MeshPaint, WxButtonGroup::OnModeMeshPaint )
	EVT_BUTTON( IDM_LANDSCAPE, WxButtonGroup::OnModeLandscape )
	EVT_BUTTON( IDM_FOLIAGE, WxButtonGroup::OnModeFoliage )
	EVT_BUTTON( IDM_BRUSH_ADD, WxButtonGroup::OnBrushAdd )
	EVT_BUTTON( IDM_BRUSH_SUBTRACT, WxButtonGroup::OnBrushSubtract )
	EVT_BUTTON( IDM_BRUSH_INTERSECT, WxButtonGroup::OnBrushIntersect )
	EVT_BUTTON( IDM_BRUSH_DEINTERSECT, WxButtonGroup::OnBrushDeintersect )
	EVT_BUTTON( IDM_BRUSH_ADD_SPECIAL, WxButtonGroup::OnAddSpecial )
	EVT_BUTTON( IDM_BRUSH_ADD_VOLUME, WxButtonGroup::OnAddVolume )
	EVT_BUTTON( IDM_SELECT_SHOW, WxButtonGroup::OnSelectShow )
	EVT_BUTTON( IDM_SELECT_HIDE, WxButtonGroup::OnSelectHide )
	EVT_BUTTON( IDM_SELECT_INVERT, WxButtonGroup::OnSelectInvert )
	EVT_BUTTON( IDM_SHOW_ALL, WxButtonGroup::OnShowAll )
	EVT_BUTTON( IDM_GO_TO_ACTOR, WxButtonGroup::GoToActor )
	EVT_BUTTON( IDM_GO_TO_BUILDERBRUSH, WxButtonGroup::GoToBuilderBrush )
	EVT_COMMAND_RANGE( IDM_BrushBuilder_START, IDM_BrushBuilder_END, wxEVT_COMMAND_BUTTON_CLICKED, WxButtonGroup::OnBrushBuilder )
	EVT_PAINT( WxButtonGroup::OnPaint )
END_EVENT_TABLE()

WxButtonGroup::WxButtonGroup( wxWindow* InParent )
	: wxWindow( InParent, -1 )
{
	bExpanded = 1;
}

WxButtonGroup::~WxButtonGroup()
{
}

WxButtonGroupButton* WxButtonGroup::AddButton( INT InID, FString InBitmapFilename, FString InToolTip, EButtonType InButtonType, UClass* InClass, INT InStartMenuID )
{
	WxBitmap* bmp = new WxBitmap( InBitmapFilename );
	WxButtonGroupButton* bb = new WxButtonGroupButton( this, InID, *bmp, InButtonType, InClass, InStartMenuID );
	Buttons.AddItem( bb );
	bb->Button->SetToolTip( *InToolTip );

	return bb;
}

WxButtonGroupButton* WxButtonGroup::AddButtonChecked( INT InID, FString InBitmapFilename, FString InToolTip, EButtonType InButtonType, UClass* InClass, INT InStartMenuID )
{
	WxBitmap* bmp = new WxBitmap( InBitmapFilename);
	WxBitmap* bmpHi = new WxBitmap( InBitmapFilename);
	WxButtonGroupButton* bb = new WxButtonGroupButton( this, InID, *bmp, *bmpHi, InButtonType, InClass, InStartMenuID );
	Buttons.AddItem( bb );
	bb->Button->SetToolTip( *InToolTip );

	return bb;
}

void WxButtonGroup::OnSize( wxSizeEvent& InEvent )
{
	wxRect rc = GetClientRect();

	INT XPos = 0;
	INT YPos = WxButtonGroup::TITLEBAR_H;

	for( INT x = 0 ; x < Buttons.Num() ; ++x )
	{
		wxBitmapButton* bmb = Buttons(x)->Button;

		bmb->SetSize( XPos,YPos, WxButtonGroup::BUTTON_SZ,WxButtonGroup::BUTTON_SZ );

		XPos += WxButtonGroup::BUTTON_SZ;
		if( XPos > WxButtonGroup::BUTTON_SZ )
		{
			XPos = 0;
			YPos += WxButtonGroup::BUTTON_SZ;
		}
	}
}

INT WxButtonGroup::GetHeight()
{
	INT NumButtons = Buttons.Num();

	if( !NumButtons || !bExpanded )
	{
		return WxButtonGroup::TITLEBAR_H;
	}

	int ExpandedHeight = WxButtonGroup::TITLEBAR_H + (WxButtonGroup::BUTTON_SZ + (((NumButtons-1)/2)*WxButtonGroup::BUTTON_SZ));

	return ExpandedHeight;
}

void WxButtonGroup::CreateAddVolumeMenu()
{
    wxMenu Menu;
    PopupMenuClasses.Empty();

    // Get a sorted array of Volume Classes then add each item in the array to the menu.
    TArray< UClass* > VolumeClasses;

    GApp->EditorFrame->GetSortedVolumeClasses( &VolumeClasses );


    INT ID = IDM_VolumeClasses_START;

    for( INT VolumeIdx = 0; VolumeIdx < VolumeClasses.Num(); VolumeIdx++ )
    {
        PopupMenuClasses.AddItem( VolumeClasses( VolumeIdx ) );
        Menu.Insert( 0, ID, *VolumeClasses( VolumeIdx )->GetName(), TEXT(""), 0 );

        ID++;
    }

    FTrackPopupMenu tpm( this, &Menu );
    tpm.Show();
}

void WxButtonGroup::OnModeCamera( wxCommandEvent& In )			{	GUnrealEd->Exec(TEXT("MODE CAMERAMOVE")); UpdateUI(); }
void WxButtonGroup::OnModeGeometry( wxCommandEvent& In )		{	GUnrealEd->Exec(TEXT("MODE GEOMETRY")); UpdateUI(); }
void WxButtonGroup::OnModeStaticMesh( wxCommandEvent& In )		{	GUnrealEd->Exec(TEXT("MODE STATICMESH")); UpdateUI(); }
void WxButtonGroup::OnModeTexture( wxCommandEvent& In )			{	GUnrealEd->Exec(TEXT("MODE TEXTURE")); UpdateUI(); }
void WxButtonGroup::OnModeTerrain( wxCommandEvent& In )			{	GUnrealEd->Exec(TEXT("MODE TERRAINEDIT")); UpdateUI(); }
void WxButtonGroup::OnModeCoverEdit( wxCommandEvent& In )		{	GUnrealEd->Exec(TEXT("MODE COVEREDIT")); UpdateUI(); }
void WxButtonGroup::OnModeMeshPaint( wxCommandEvent& In )		{	GUnrealEd->Exec(TEXT("MODE MESHPAINT")); UpdateUI(); }
void WxButtonGroup::OnModeLandscape( wxCommandEvent& In )		{	GUnrealEd->Exec(TEXT("MODE LANDSCAPE")); UpdateUI(); }
void WxButtonGroup::OnModeFoliage( wxCommandEvent& In )			{	GUnrealEd->Exec(TEXT("MODE FOLIAGE")); UpdateUI(); }
void WxButtonGroup::OnBrushAdd( wxCommandEvent& In )			{	GUnrealEd->Exec(TEXT("BRUSH ADD"));	}
void WxButtonGroup::OnBrushSubtract( wxCommandEvent& In )		{	GUnrealEd->Exec(TEXT("BRUSH SUBTRACT"));	}
void WxButtonGroup::OnBrushIntersect( wxCommandEvent& In )		{	GUnrealEd->Exec(TEXT("BRUSH FROM INTERSECTION"));	}
void WxButtonGroup::OnBrushDeintersect( wxCommandEvent& In )	{	GUnrealEd->Exec(TEXT("BRUSH FROM DEINTERSECTION"));	}
void WxButtonGroup::OnAddSpecial( wxCommandEvent& In )			{	GDlgAddSpecial->Show(); }
void WxButtonGroup::OnAddVolume( wxCommandEvent& In )			{	CreateAddVolumeMenu(); }
void WxButtonGroup::OnSelectShow( wxCommandEvent& In )			{	GUnrealEd->Exec(TEXT("ACTOR HIDE UNSELECTED"));	}
void WxButtonGroup::OnSelectHide( wxCommandEvent& In )			{	GUnrealEd->Exec(TEXT("ACTOR HIDE SELECTED"));	}
void WxButtonGroup::OnSelectInvert( wxCommandEvent& In )		{	GUnrealEd->Exec(TEXT("ACTOR SELECT INVERT"));	}
void WxButtonGroup::OnShowAll( wxCommandEvent& In )				{	GUnrealEd->Exec(TEXT("ACTOR UNHIDE ALL"));	}
void WxButtonGroup::GoToActor( wxCommandEvent& In )
{
	const UBOOL bShiftDown = ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0;
	if( bShiftDown )
	{
		GUnrealEd->Exec( TEXT( "CAMERA ALIGN ACTIVEVIEWPORTONLY" ) );
	}
	else
	{
		GUnrealEd->Exec( TEXT( "CAMERA ALIGN" ) );
	}
}
void WxButtonGroup::GoToBuilderBrush( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("SELECT BUILDERBRUSH") );
	GoToActor( In );
}

void WxButtonGroup::OnRightButtonDown( wxMouseEvent& In )
{
	WxButtonGroupButton* Button = FindButtonFromId( In.GetId() );
	if( !Button )	return;

	if( Button->ButtonType == BUTTONTYPE_ClassMenu )
	{
        CreateAddVolumeMenu();
	}

	if( Button->ButtonType == BUTTONTYPE_BrushBuilder )
	{
		WxDlgBrushBuilder* dlg = new WxDlgBrushBuilder;
		dlg->Show( Button->BrushBuilder );
	}
}

void WxButtonGroup::OnAddVolumeClass( wxCommandEvent& In )
{
	WxButtonGroupButton* Button = FindButtonFromId( In.GetId() );
	if( !Button )	return;

	INT VolumeIdx = In.GetId() - IDM_VolumeClasses_START;
	UClass* Class = PopupMenuClasses(VolumeIdx);

	GUnrealEd->Exec( *FString::Printf( TEXT("BRUSH ADDVOLUME CLASS=%s"), *Class->GetName() ) );

	// A new volume actor was added, update the volumes visibility.
	// This volume should be hidden if the user doesnt have this type of volume visible.
	GUnrealEd->UpdateVolumeActorVisibility( Class );
}

WxButtonGroupButton* WxButtonGroup::FindButtonFromId( INT InID )
{
	if( InID >= IDM_VolumeClasses_START && InID <= IDM_VolumeClasses_END )
		InID = IDM_BRUSH_ADD_VOLUME;

	for( INT x = 0 ; x < Buttons.Num() ; ++x )
	{
		if( Buttons(x)->Button->GetId() == InID )
			return Buttons(x);
	}

	return NULL;
}

void WxButtonGroup::OnPaint( wxPaintEvent& In )
{
	wxPaintDC dc(this);
	wxRect rc = GetClientRect();
	rc.height = WxButtonGroup::TITLEBAR_H;

	dc.SetBrush( *wxTRANSPARENT_BRUSH );
	dc.SetPen( *wxGREY_PEN );
	dc.SetFont( wxFont(8.f, wxDEFAULT, wxNORMAL, wxNORMAL, FALSE, _("Lucida Sans Unicode")) );

	dc.DrawLine(rc.x+1, rc.y+1, rc.width-1, rc.y+1);
	wxSize size = dc.GetMultiLineTextExtent(GetLabel());
	dc.DrawText(GetLabel(), rc.x+(rc.width/2)-(size.GetWidth()/2)-1,rc.y+6);
}

void WxButtonGroup::OnBrushBuilder( wxCommandEvent& In )
{
	INT idx = In.GetId() - IDM_BrushBuilder_START;
	UBrushBuilder* bb = Buttons(idx)->BrushBuilder;

	bb->eventBuild();
	GEditorModeTools().MapChangeNotify();
}

void WxButtonGroup::UpdateUI()
{
	for( INT x = 0 ; x < Buttons.Num() ; ++x )
	{
		WxButtonGroupButton* bgb = Buttons(x);

		UBOOL bIsCurrentState = FALSE;
		switch( bgb->Button->GetId() )
		{
			case IDM_DEFAULT:
				bIsCurrentState = (GEditorModeTools().IsModeActive(EM_Default));
				break;
			case IDM_GEOMETRY:
				bIsCurrentState = (GEditorModeTools().IsModeActive(EM_Geometry));
				break;
			case IDM_STATICMESH:
				bIsCurrentState = (GEditorModeTools().IsModeActive(EM_StaticMesh));
				break;
			case IDM_TERRAIN:
				bIsCurrentState = (GEditorModeTools().IsModeActive(EM_TerrainEdit));
				break;
			case IDM_TEXTURE:
				bIsCurrentState = (GEditorModeTools().IsModeActive(EM_Texture));
				break;
			case IDM_COVEREDIT:
				bIsCurrentState = (GEditorModeTools().IsModeActive(EM_CoverEdit));
				break;
			case IDM_EditorMode_MeshPaint:
				bIsCurrentState = (GEditorModeTools().IsModeActive(EM_MeshPaint));
				break;
			case IDM_LANDSCAPE:
				bIsCurrentState = (GEditorModeTools().IsModeActive(EM_Landscape));
				break;
			case IDM_FOLIAGE:
				bIsCurrentState = (GEditorModeTools().IsModeActive(EM_Foliage));
				break;
		}
		WxBitmapCheckButton* CheckButton = wxDynamicCast(bgb->Button,WxBitmapCheckButton);
		if (CheckButton)
		{
			CheckButton->SetCurrentState( bIsCurrentState );
			wxColour BackgroundColor = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
			if (bIsCurrentState)
			{
				BackgroundColor = wxColor(140, 190, 190);
			}
			CheckButton->SetBackgroundColour(BackgroundColor);
		}
	}
}

/*-----------------------------------------------------------------------------
	WxButtonBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxButtonBar, wxPanel )
	EVT_SIZE( WxButtonBar::OnSize )
END_EVENT_TABLE()

WxButtonBar::WxButtonBar()
{
}

WxButtonBar::~WxButtonBar()
{
}

void WxButtonBar::UpdateUI()
{
	for( INT x = 0 ; x < ButtonGroups.Num() ; ++x )
		ButtonGroups(x)->UpdateUI();
}

UBOOL WxButtonBar::Create( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style, const FString& name )
{
	UBOOL bRet = wxPanel::Create( parent, id, pos, size, style, *name );

	WxButtonGroup* bg;

	// Modes

	bg = new WxButtonGroup( (wxWindow*)this );
	ButtonGroups.AddItem( bg );
	bg->SetLabel(*LocalizeUnrealEd("ModeButtonGroupLabel"));
	bg->AddButtonChecked( IDM_DEFAULT, TEXT("Btn_Camera.png"), *LocalizeUnrealEd("CameraMode") );
	bg->AddButtonChecked( IDM_GEOMETRY, TEXT("Btn_Geometry.png"), *LocalizeUnrealEd("GeometryMode") );
	bg->AddButtonChecked( IDM_TERRAIN, TEXT("Btn_TerrainMode.png"), *LocalizeUnrealEd("TerrainEditingMode") );
	bg->AddButtonChecked( IDM_TEXTURE, TEXT("Btn_TextureMode.png"), *LocalizeUnrealEd("TextureAlignmentMode") );
	bg->AddButtonChecked( IDM_EditorMode_MeshPaint, TEXT("Btn_MeshPaintMode.png"), *LocalizeUnrealEd("EditorModeButtonBar_MeshPaint") );
	bg->AddButtonChecked( IDM_STATICMESH, TEXT("Btn_StaticMeshMode.png"), *LocalizeUnrealEd("StaticMeshMode") );
	bg->AddButtonChecked( IDM_LANDSCAPE, TEXT("Btn_LandscapeMode.png"), *LocalizeUnrealEd("LandscapeMode") );
	bg->AddButtonChecked( IDM_FOLIAGE, TEXT("Btn_FoliageMode.png"), *LocalizeUnrealEd("FoliageMode") );

	bg = new WxButtonGroup( (wxWindow*)this );
	ButtonGroups.AddItem( bg );
	bg->SetLabel(*LocalizeUnrealEd("BrushButtonGroupLabel"));

	INT ID = IDM_BrushBuilder_START;

	for( TObjectIterator<UClass> ItC; ItC; ++ItC )
		if( ItC->IsChildOf(UBrushBuilder::StaticClass()) && !(ItC->ClassFlags&CLASS_Abstract) )
		{
			UBrushBuilder* ubb = ConstructObject<UBrushBuilder>( *ItC );
			if( ubb )
			{
				FString TempFileName = ubb->BitmapFilename + TEXT(".png");
				WxButtonGroupButton* bgb = bg->AddButton( ID, TempFileName, *LocalizeUnrealEd( *ubb->ToolTip ), BUTTONTYPE_BrushBuilder, *ItC );
				bgb->BrushBuilder = ubb;
				ID++;
			}
		}

	bg = new WxButtonGroup( (wxWindow*)this );
	ButtonGroups.AddItem( bg );
	bg->SetLabel(*LocalizeUnrealEd("CSGButtonGroupLabel"));
	bg->AddButton( IDM_BRUSH_ADD, TEXT("Btn_Add.png"), *LocalizeUnrealEd("CSGAdd") );
	bg->AddButton( IDM_BRUSH_SUBTRACT, TEXT("Btn_Subtract.png"), *LocalizeUnrealEd("CSGSubtract") );
	bg->AddButton( IDM_BRUSH_INTERSECT, TEXT("Btn_Intersect.png"), *LocalizeUnrealEd("CSGIntersect") );
	bg->AddButton( IDM_BRUSH_DEINTERSECT, TEXT("Btn_Deintersect.png"), *LocalizeUnrealEd("CSGDeintersect") );

	bg = new WxButtonGroup( (wxWindow*)this );
	ButtonGroups.AddItem( bg );
	bg->SetLabel(*LocalizeUnrealEd("VolumeButtonGroupLabel"));
	bg->AddButton( IDM_BRUSH_ADD_SPECIAL, TEXT("Btn_AddSpecial.png"), *LocalizeUnrealEd("AddSpecialBrush") );
	bg->AddButton( IDM_BRUSH_ADD_VOLUME, TEXT("Btn_AddVolume.png"), *LocalizeUnrealEd("AddVolume"), BUTTONTYPE_ClassMenu, AVolume::StaticClass(), IDM_VolumeClasses_START );

	bg = new WxButtonGroup( (wxWindow*)this );
	ButtonGroups.AddItem( bg );
	bg->SetLabel(*LocalizeUnrealEd("ShowHideButtonGroupLabel"));
	bg->AddButton( IDM_SELECT_SHOW, TEXT("Btn_ShowSelected.png"), *LocalizeUnrealEd("ShowSelectedOnly") );
	bg->AddButton( IDM_SELECT_HIDE, TEXT("Btn_HideSelected.png"), *LocalizeUnrealEd("HideSelected") );
	bg->AddButton( IDM_SELECT_INVERT, TEXT("Btn_InvertSelection.png"), *LocalizeUnrealEd("InvertSelections") );
	bg->AddButton( IDM_SHOW_ALL, TEXT("Btn_ShowAll.png"), *LocalizeUnrealEd("ShowAll") );

	bg = new WxButtonGroup( (wxWindow*)this );
	ButtonGroups.AddItem( bg );
	bg->SetLabel(*LocalizeUnrealEd("GoToActorButtonGroupLabel"));
	bg->AddButton( IDM_GO_TO_ACTOR, TEXT("Btn_GoToActor.png"), *LocalizeUnrealEd("GoToActor") );
	bg->AddButton( IDM_GO_TO_BUILDERBRUSH, TEXT("Btn_GoToBuilderBrush.png"), *LocalizeUnrealEd("GoToBuilderBrush") );

	return bRet;
}

void WxButtonBar::OnSize( wxSizeEvent& InEvent )
{
	PositionChildControls();
}

void WxButtonBar::PositionChildControls()
{
	wxRect rc = GetClientRect();

	INT Top = 0;

	// Size button groups to fit inside

	for( INT x = 0 ; x < ButtonGroups.Num() ; ++x )
	{
		WxButtonGroup* bg = ButtonGroups(x);

		INT YSz = bg->GetHeight();

		bg->SetSize( 0,Top, rc.GetWidth(),YSz );
		bg->Refresh();

		Top += YSz;
	}
}

