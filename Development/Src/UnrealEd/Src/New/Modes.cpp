
#include "UnrealEd.h"
#include "UnTerrain.h"

/*-----------------------------------------------------------------------------
	WxModeBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxModeBar, wxPanel )
END_EVENT_TABLE()

WxModeBar::WxModeBar( wxWindow* InParent, wxWindowID InID )
	: wxPanel( InParent, InID )
{
	SavedHeight = -1;
}

WxModeBar::WxModeBar( wxWindow* InParent, wxWindowID InID, FString InPanelName )
	: wxPanel( InParent, InID )
{
	SavedHeight = -1;

	Panel = (wxPanel*)wxXmlResource::Get()->LoadPanel( this, *InPanelName );
	Panel->Fit();
	SetSize( InParent->GetRect().GetWidth(), Panel->GetRect().GetHeight() );
}

WxModeBar::~WxModeBar()
{
}

void WxModeBar::Refresh()
{
	LoadData();
}

/*-----------------------------------------------------------------------------
	WxModeBarDefault.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxModeBarDefault, WxModeBar )
END_EVENT_TABLE()

WxModeBarDefault::WxModeBarDefault( wxWindow* InParent, wxWindowID InID )
	: WxModeBar( InParent, InID, TEXT("ID_PANEL_MODEBARDEFAULT") )
{
	LoadData();
	Show(0);
}

WxModeBarDefault::~WxModeBarDefault()
{
}

/*-----------------------------------------------------------------------------
	WxModeBarTerrainEdit.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxModeBarTerrainEdit, WxModeBar )
END_EVENT_TABLE()

WxModeBarTerrainEdit::WxModeBarTerrainEdit( wxWindow* InParent, wxWindowID InID )
	: WxModeBar( InParent, InID, TEXT("ID_PANEL_MODEBARTERRAINEDIT") )
{
	ADDEVENTHANDLER( XRCID("IDCB_TOOLS"), wxEVT_COMMAND_COMBOBOX_SELECTED, &WxModeBarTerrainEdit::OnToolsSelChange );
	ADDEVENTHANDLER( XRCID("IDCK_PERTOOL"), wxEVT_UPDATE_UI, &WxModeBarTerrainEdit::UI_PerToolSettings );
	ADDEVENTHANDLER( XRCID("IDCK_PERTOOL"), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxModeBarTerrainEdit::SaveData );
	ADDEVENTHANDLER( XRCID("IDEC_RADIUS_MIN"), wxEVT_COMMAND_TEXT_UPDATED, &WxModeBarTerrainEdit::OnUpdateRadiusMin );
	ADDEVENTHANDLER( XRCID("IDEC_RADIUS_MAX"), wxEVT_COMMAND_TEXT_UPDATED, &WxModeBarTerrainEdit::OnUpdateRadiusMax );
	ADDEVENTHANDLER( XRCID("IDEC_RADIUS_MIN"), wxEVT_COMMAND_TEXT_ENTER, &WxModeBarTerrainEdit::SaveData );
	ADDEVENTHANDLER( XRCID("IDEC_RADIUS_MAX"), wxEVT_COMMAND_TEXT_ENTER, &WxModeBarTerrainEdit::SaveData );
	ADDEVENTHANDLER( XRCID("IDCB_STRENGTH"), wxEVT_COMMAND_TEXT_UPDATED, &WxModeBarTerrainEdit::SaveData );
	ADDEVENTHANDLER( XRCID("IDCB_STRENGTH"), wxEVT_COMMAND_COMBOBOX_SELECTED, &WxModeBarTerrainEdit::SaveData );
	ADDEVENTHANDLER( XRCID("IDCB_LAYER"), wxEVT_COMMAND_COMBOBOX_SELECTED, &WxModeBarTerrainEdit::SaveData );
	ADDEVENTHANDLER( XRCID("IDCB_TERRAIN"), wxEVT_COMMAND_COMBOBOX_SELECTED, &WxModeBarTerrainEdit::OnTerrainSelChange );

	// Init controls

	FEdMode* mode = GEditorModeTools.FindMode( EM_TerrainEdit );
	wxComboBox* cb = (wxComboBox*)FindWindow( XRCID( "IDCB_TOOLS" ) );
	check(cb);
	for( INT x = 0 ; x < mode->Tools.Num() ; ++x )
	{
		cb->Append( *mode->Tools(x)->GetName(), mode->Tools(x) );
	}
	cb->Select( 0 );
	
	cb = (wxComboBox*)FindWindow( XRCID( "IDCB_STRENGTH" ) );
	check(cb);
	cb->Append( TEXT("5") );
	cb->Append( TEXT("15") );
	cb->Append( TEXT("25") );
	cb->Append( TEXT("50") );
	cb->Append( TEXT("75") );
	cb->Append( TEXT("100") );
	cb->Append( TEXT("200") );

	Show(0);
}

WxModeBarTerrainEdit::~WxModeBarTerrainEdit()
{
}

void WxModeBarTerrainEdit::OnToolsSelChange( wxCommandEvent& In )
{
	wxComboBox* cb = (wxComboBox*)FindWindow( XRCID( "IDCB_TOOLS" ) );
	GEditorModeTools.GetCurrentMode()->SetCurrentTool( (FModeTool*)cb->GetClientData( cb->GetSelection() ) );

	LoadData();
}

void WxModeBarTerrainEdit::SaveData( wxCommandEvent& In )
{
	SaveData();
}

void WxModeBarTerrainEdit::OnUpdateRadiusMin( wxCommandEvent& In )
{
	((FEdModeTerrainEditing*)GEditorModeTools.FindMode( EM_TerrainEdit ))->bPerToolSettings = ((wxCheckBox*)FindWindow( XRCID( "IDCK_PERTOOL"  ) ))->GetValue();

	FTerrainToolSettings* settings = (FTerrainToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();

	settings->RadiusMin = appAtoi( ((wxTextCtrl*)FindWindow( XRCID( "IDEC_RADIUS_MIN" ) ))->GetValue().c_str() );
}

void WxModeBarTerrainEdit::OnUpdateRadiusMax( wxCommandEvent& In )
{
	((FEdModeTerrainEditing*)GEditorModeTools.FindMode( EM_TerrainEdit ))->bPerToolSettings = ((wxCheckBox*)FindWindow( XRCID( "IDCK_PERTOOL"  ) ))->GetValue();

	FTerrainToolSettings* settings = (FTerrainToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();

	settings->RadiusMax = appAtoi( ((wxTextCtrl*)FindWindow( XRCID( "IDEC_RADIUS_MAX" ) ))->GetValue().c_str() );
}

enum ELayerType
{
	LT_HeightMap = 0,
	LT_Layer = 1,
	LT_DecoLayer = 2
};

union FTerrainLayerId
{
	void*			Id;
	struct
	{
		BITFIELD	Type : 2,
					Index : 30;
	};
};

void WxModeBarTerrainEdit::SaveData()
{
	((FEdModeTerrainEditing*)GEditorModeTools.FindMode( EM_TerrainEdit ))->bPerToolSettings = ((wxCheckBox*)FindWindow( XRCID( "IDCK_PERTOOL" ) ))->GetValue();

	FTerrainToolSettings* settings = (FTerrainToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();

	// Misc

	settings->RadiusMin = appAtoi( ((wxTextCtrl*)FindWindow( XRCID( "IDEC_RADIUS_MIN" ) ))->GetValue().c_str() );
	settings->RadiusMax = appAtoi( ((wxTextCtrl*)FindWindow( XRCID( "IDEC_RADIUS_MAX" ) ))->GetValue().c_str() );
	settings->Strength = appAtof( ((wxComboBox*)FindWindow( XRCID( "IDCB_STRENGTH" ) ))->GetValue().c_str() );

	// Layers

	FTerrainLayerId	LayerClientData;

	LayerClientData.Id = ((wxComboBox*)FindWindow( XRCID( "IDCB_LAYER" ) ))->GetClientData( ((wxComboBox*)FindWindow( XRCID( "IDCB_LAYER" ) ))->GetSelection() );

	if(LayerClientData.Type == LT_HeightMap)
		settings->LayerIndex = INDEX_NONE;
	else if(LayerClientData.Type == LT_Layer)
	{
		settings->DecoLayer = 0;
		settings->LayerIndex = LayerClientData.Index;
	}
	else
	{
		settings->DecoLayer = 1;
		settings->LayerIndex = LayerClientData.Index;
	}
}

void WxModeBarTerrainEdit::LoadData()
{
	FTerrainToolSettings* settings = (FTerrainToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();

	// Misc

	((wxTextCtrl*)FindWindow( XRCID( "IDEC_RADIUS_MIN" ) ) )->SetValue( *FString::Printf( TEXT("%d"), settings->RadiusMin ) );
	((wxTextCtrl*)FindWindow( XRCID( "IDEC_RADIUS_MAX" ) ) )->SetValue( *FString::Printf( TEXT("%d"), settings->RadiusMax ) );
	((wxComboBox*)FindWindow( XRCID( "IDCB_STRENGTH" ) ) )->SetValue( *FString::Printf( TEXT("%1.2f"), settings->Strength ) );

    ((wxCheckBox*)FindWindow( XRCID( "IDCK_PERTOOL" ) ) )->SetValue( ( ( FEdModeTerrainEditing*)GEditorModeTools.FindMode( EM_TerrainEdit ) )->bPerToolSettings ? true : false );

	// Terrains

	wxComboBox* Wk = (wxComboBox*)FindWindow( XRCID( "IDCB_TERRAIN" ) );
	INT save = Wk->GetSelection();
	if( save == -1 )	save = 0;

	Wk->Clear();
	for( int i = 0 ; i < GEditor->Level->Actors.Num() ; i++ )
	{
		ATerrain* TN = Cast<ATerrain>(GEditor->Level->Actors(i));
		if( TN )
			Wk->Append( TN->GetName(), TN );
	}
	Wk->SetSelection(save);

    wxCommandEvent eventCommand;
	OnTerrainSelChange( eventCommand );
}

// Load the layer combobox based on the currently selected terraininfo

void WxModeBarTerrainEdit::RefreshLayers()
{
	FEdModeTerrainEditing* mode = (FEdModeTerrainEditing*)GEditorModeTools.FindMode( EM_TerrainEdit );
	wxComboBox* cb = (wxComboBox*)FindWindow( XRCID( "IDCB_LAYER" ) );

	cb->Clear();
	cb->Append( TEXT("Heightmap"), (void*)NULL );
	cb->Append( TEXT("---") );

	if( mode->CurrentTerrain )
	{
		for( INT x = 1 ; x < mode->CurrentTerrain->Layers.Num() ; ++x )
		{
			FTerrainLayerId	LayerClientData;
			LayerClientData.Type = LT_Layer;
			LayerClientData.Index = x;
			cb->Append( *mode->CurrentTerrain->Layers(x).Name, LayerClientData.Id );
		}

		for( INT x = 0 ; x < mode->CurrentTerrain->DecoLayers.Num() ; ++x )
		{
			FTerrainLayerId	LayerClientData;
			LayerClientData.Type = LT_DecoLayer;
			LayerClientData.Index = x;
			cb->Append( *mode->CurrentTerrain->DecoLayers(x).Name, LayerClientData.Id );
		}
	}
}

void WxModeBarTerrainEdit::OnTerrainSelChange( wxCommandEvent& In )
{
	FEdModeTerrainEditing* mode = (FEdModeTerrainEditing*)GEditorModeTools.FindMode( EM_TerrainEdit );
	wxComboBox* cb = (wxComboBox*)FindWindow( XRCID( "IDCB_TERRAIN" ) );

	if( cb->GetSelection() != -1 )
		mode->CurrentTerrain = (ATerrain*)cb->GetClientData( cb->GetSelection() );

	RefreshLayers();

	// Select the heightmap

	((wxComboBox*)FindWindow( XRCID( "IDCB_LAYER" ) ) )->SetSelection( 0 );
}

void WxModeBarTerrainEdit::UI_PerToolSettings( wxUpdateUIEvent& In )	{	In.Check( ((FEdModeTerrainEditing*)GEditorModeTools.FindMode( EM_TerrainEdit ))->bPerToolSettings ? true : false );	}

/*-----------------------------------------------------------------------------
	WxModeBarGeometry.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxModeBarGeometry, WxModeBar )
	EVT_TOOL( ID_TOGGLE_WINDOW, WxModeBarGeometry::OnToggleWindow )
	EVT_TOOL( ID_GEOM_OBJECT, WxModeBarGeometry::OnSelObject )
	EVT_TOOL( ID_GEOM_POLY, WxModeBarGeometry::OnSelPoly )
	EVT_TOOL( ID_GEOM_EDGE, WxModeBarGeometry::OnSelEdge )
	EVT_TOOL( ID_GEOM_VERTEX, WxModeBarGeometry::OnSelVertex )
	EVT_UPDATE_UI( ID_TOGGLE_WINDOW, WxModeBarGeometry::UI_ToggleWindow )
	EVT_UPDATE_UI( ID_GEOM_OBJECT, WxModeBarGeometry::UI_SelObject )
	EVT_UPDATE_UI( ID_GEOM_POLY, WxModeBarGeometry::UI_SelPoly )
	EVT_UPDATE_UI( ID_GEOM_EDGE, WxModeBarGeometry::UI_SelEdge )
	EVT_UPDATE_UI( ID_GEOM_VERTEX, WxModeBarGeometry::UI_SelVertex )
END_EVENT_TABLE()

WxModeBarGeometry::WxModeBarGeometry( wxWindow* InParent, wxWindowID InID )
	: WxModeBar( InParent, InID, TEXT("ID_PANEL_MODEBARGEOMETRY") )
{
	wxWindow* win = FindWindow( XRCID( "IDSC_SEL_TOOLBAR" ) );
	ToolBar = new WxModeGeometrySelBar( win, -1 );

	ShowNormalsCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_SHOW_NORMALS" ) );
	AffectWidgetOnlyCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_WIDGET_ONLY" ) );
	UseSoftSelectionCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_SOFT_SELECTION" ) );
	SoftSelectionLabel = (wxStaticText*)FindWindow( XRCID( "IDSC_SS_RADIUS" ) );
	SoftSelectionRadiusText = (wxTextCtrl*)FindWindow( XRCID( "IDEC_SS_RADIUS" ) );

	ADDEVENTHANDLER( XRCID("IDCK_SHOW_NORMALS"), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxModeBarGeometry::OnShowNormals );
	ADDEVENTHANDLER( XRCID("IDCK_WIDGET_ONLY"), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxModeBarGeometry::OnAffectWidgetOnly );
	ADDEVENTHANDLER( XRCID("IDCK_SOFT_SELECTION"), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxModeBarGeometry::OnUseSoftSelection );
	ADDEVENTHANDLER( XRCID("IDEC_SS_RADIUS"), wxEVT_COMMAND_TEXT_UPDATED, &WxModeBarGeometry::OnSoftSelectionRadius );
	ADDEVENTHANDLER( XRCID("IDCK_SHOW_NORMALS"), wxEVT_UPDATE_UI, &WxModeBarGeometry::UI_ShowNormals );
	ADDEVENTHANDLER( XRCID("IDCK_WIDGET_ONLY"), wxEVT_UPDATE_UI, &WxModeBarGeometry::UI_AffectWidgetOnly );
	ADDEVENTHANDLER( XRCID("IDCK_SOFT_SELECTION"), wxEVT_UPDATE_UI, &WxModeBarGeometry::UI_UseSoftSelection );
	ADDEVENTHANDLER( XRCID("IDEC_SS_RADIUS"), wxEVT_UPDATE_UI, &WxModeBarGeometry::UI_SoftSelectionRadius );
	ADDEVENTHANDLER( XRCID("IDSC_SS_RADIUS"), wxEVT_UPDATE_UI, &WxModeBarGeometry::UI_SoftSelectionLabel );

	LoadData();
	Show(0);
}

WxModeBarGeometry::~WxModeBarGeometry()
{
}

void WxModeBarGeometry::SaveData()
{
}

void WxModeBarGeometry::LoadData()
{
}

void WxModeBarGeometry::OnToggleWindow( wxCommandEvent& In )
{
	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	settings->bShowModifierWindow = In.IsChecked();

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();
    mode->ModifierWindow->Show( settings->bShowModifierWindow ? true : false );

	GCallback->Send( CALLBACK_RedrawAllViewports );
}

void WxModeBarGeometry::OnSelObject( wxCommandEvent& In )
{
	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	settings->SelectionType = GST_Object;
	GCallback->Send( CALLBACK_RedrawAllViewports );
}

void WxModeBarGeometry::OnSelPoly( wxCommandEvent& In )
{
	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	settings->SelectionType = GST_Poly;
	GCallback->Send( CALLBACK_RedrawAllViewports );
}

void WxModeBarGeometry::OnSelEdge( wxCommandEvent& In )
{
	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	settings->SelectionType = GST_Edge;
	GCallback->Send( CALLBACK_RedrawAllViewports );
}

void WxModeBarGeometry::OnSelVertex( wxCommandEvent& In )
{
	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	settings->SelectionType = GST_Vertex;
	GCallback->Send( CALLBACK_RedrawAllViewports );
}

void WxModeBarGeometry::UI_ToggleWindow( wxUpdateUIEvent& In )
{
	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	if( settings )
	{
        In.Check( settings->bShowModifierWindow ? true : false );
	}
}

void WxModeBarGeometry::UI_SelObject( wxUpdateUIEvent& In )
{
	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	if( settings )
	{
		In.Check( settings->SelectionType == GST_Object );
	}
}

void WxModeBarGeometry::UI_SelPoly( wxUpdateUIEvent& In )
{
	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	if( settings )
	{
		In.Check( settings->SelectionType == GST_Poly );
	}
}

void WxModeBarGeometry::UI_SelEdge( wxUpdateUIEvent& In )
{
	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	if( settings )
	{
		In.Check( settings->SelectionType == GST_Edge );
	}
}

void WxModeBarGeometry::UI_SelVertex( wxUpdateUIEvent& In )
{
	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	if( settings )
	{
		In.Check( settings->SelectionType == GST_Vertex );
	}
}

void WxModeBarGeometry::OnShowNormals( wxCommandEvent& In )
{
	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	settings->bShowNormals = In.IsChecked();
	GCallback->Send( CALLBACK_RedrawAllViewports );
}

void WxModeBarGeometry::OnAffectWidgetOnly( wxCommandEvent& In )
{
	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	settings->bAffectWidgetOnly = In.IsChecked();
	GCallback->Send( CALLBACK_RedrawAllViewports );
}

void WxModeBarGeometry::OnUseSoftSelection( wxCommandEvent& In )
{
	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	settings->bUseSoftSelection = In.IsChecked();

	if( settings->bUseSoftSelection )
	{
		GEditorModeTools.GetCurrentMode()->SoftSelect();
	}
	else
	{
		GEditorModeTools.GetCurrentMode()->SoftSelectClear();
	}

	GCallback->Send( CALLBACK_RedrawAllViewports );
}

void WxModeBarGeometry::OnSoftSelectionRadius( wxCommandEvent& In )
{
	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	settings->SoftSelectionRadius = appAtoi( In.GetString().c_str() );
	GEditorModeTools.GetCurrentMode()->SoftSelect();
	GCallback->Send( CALLBACK_RedrawAllViewports );
}

void WxModeBarGeometry::UI_ShowNormals( wxUpdateUIEvent& In )
{
	if( GEditorModeTools.GetCurrentModeID() != EM_Geometry )
		return;

	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
    In.Check( settings->bShowNormals ? true : false );
}

void WxModeBarGeometry::UI_AffectWidgetOnly( wxUpdateUIEvent& In )
{
	if( GEditorModeTools.GetCurrentModeID() != EM_Geometry )
		return;

	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
    In.Check( settings->bAffectWidgetOnly ? true : false );
}

void WxModeBarGeometry::UI_UseSoftSelection( wxUpdateUIEvent& In )
{
	if( GEditorModeTools.GetCurrentModeID() != EM_Geometry )
		return;

	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
    In.Check( settings->bUseSoftSelection ? true : false );
}

void WxModeBarGeometry::UI_SoftSelectionRadius( wxUpdateUIEvent& In )
{
	if( GEditorModeTools.GetCurrentModeID() != EM_Geometry )
		return;

	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();

	In.SetText( appItoa( settings->SoftSelectionRadius ) );
    In.Enable( settings->bUseSoftSelection ? true : false );
}

void WxModeBarGeometry::UI_SoftSelectionLabel( wxUpdateUIEvent& In )
{
	if( GEditorModeTools.GetCurrentModeID() != EM_Geometry )
		return;

	FGeometryToolSettings* settings = (FGeometryToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
    In.Enable( settings->bUseSoftSelection ? true : false );
}

/*-----------------------------------------------------------------------------
	WxModeBarTexture.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxModeBarTexture, WxModeBar )
END_EVENT_TABLE()

WxModeBarTexture::WxModeBarTexture( wxWindow* InParent, wxWindowID InID )
	: WxModeBar( InParent, InID, TEXT("ID_PANEL_MODEBARTEXTURE") )
{
	LoadData();
	Show(0);
}

WxModeBarTexture::~WxModeBarTexture()
{
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
