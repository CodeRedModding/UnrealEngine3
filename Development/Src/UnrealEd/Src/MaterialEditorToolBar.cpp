/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "MaterialEditorToolBar.h"

//////////////////////////////////////////////////////////////////////////
//	WxMaterialEditorToolBarBase
//////////////////////////////////////////////////////////////////////////
/**
* The base material editor toolbar, loads all of the common bitmaps shared between editors.
*/
WxMaterialEditorToolBarBase::WxMaterialEditorToolBarBase(wxWindow* InParent, wxWindowID InID)
:	WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
{
	// Load bitmaps.
	BackgroundB.Load( TEXT("Background") );
	CylinderB.Load( TEXT("ME_Cylinder") );
	CubeB.Load( TEXT("ME_Cube") );
	SphereB.Load( TEXT("ME_Sphere") );
	PlaneB.Load( TEXT("ME_Plane") );
	RealTimeB.Load( TEXT("Realtime") );
	SyncGenericBrowserB.Load(TEXT("MatEdit_Prop_Browse"));
	CleanB.Load( TEXT("MaterialEditor_Clean") );
	UseButtonB.Load( TEXT("MaterialEditor_Use") );
	ToggleGridB.Load( TEXT("MaterialEditor_ToggleGrid") );
	ShowAllParametersB.Load(TEXT("MaterialInstanceEditor_ShowAllParams"));

	SetToolBitmapSize( wxSize( 18, 18 ) );
}


void WxMaterialEditorToolBarBase::SetShowGrid(UBOOL bValue)
{
	ToggleTool( ID_MATERIALEDITOR_TOGGLEGRID, bValue == TRUE );
}

void WxMaterialEditorToolBarBase::SetShowBackground(UBOOL bValue)
{
	//ToggleTool( IDM_SHOW_BACKGROUND, bValue == TRUE );
}

void WxMaterialEditorToolBarBase::SetRealtimeMaterialPreview(UBOOL bValue)
{
	ToggleTool( ID_MATERIALEDITOR_REALTIME_PREVIEW, bValue == TRUE );
}

//////////////////////////////////////////////////////////////////////////
//	WxMaterialEditorToolBar
//////////////////////////////////////////////////////////////////////////

/**
 * The toolbar appearing along the top of the material editor.
 */
WxMaterialEditorToolBar::WxMaterialEditorToolBar(wxWindow* InParent, wxWindowID InID)
	:	WxMaterialEditorToolBarBase( InParent, InID )
{
	// Load bitmaps.
	RealTimePreviewsB.Load( TEXT("MaterialEditor_RealtimePreviews") );
	HomeB.Load( TEXT("Home") );
	HideConnectorsB.Load( TEXT("MaterialEditor_HideConnectors") );
	ShowConnectorsB.Load( TEXT("MaterialEditor_ShowConnectors") );
	ApplyB.Load( TEXT("MaterialEditor_Apply") );
	ApplyDisabledB.Load( TEXT("MaterialEditor_ApplyDisabled") );
	FlattenB.Load( TEXT("MaterialEditor_OpenFallbackDisabled") );
	ToggleMaterialStatsB.Load( TEXT("MaterialEditor_ToggleStats") );
	ViewSourceB.Load( TEXT("MaterialEditor_ViewSource") );

	// Add toolbar buttons.
	ApplyButton = new WxBitmapButton(this, ID_MATERIALEDITOR_APPLY, ApplyB, wxDefaultPosition, wxSize(35,21));
	ApplyButton->SetBitmapDisabled(ApplyDisabledB);
	ApplyButton->SetLabel(*LocalizeUnrealEd("Apply"));
	ApplyButton->SetToolTip(*LocalizeUnrealEd("ToolTip_MaterialEditorApply"));
	AddControl(ApplyButton);
	AddSeparator();
	AddTool( ID_GO_HOME, TEXT(""), HomeB, *LocalizeUnrealEd("ToolTip_56") );
	AddSeparator();
	//AddCheckTool( IDM_SHOW_BACKGROUND, TEXT(""), BackgroundB, BackgroundB, *LocalizeUnrealEd("ToolTip_51") );
	AddCheckTool( ID_MATERIALEDITOR_TOGGLEGRID, TEXT(""), ToggleGridB, ToggleGridB, *LocalizeUnrealEd("ToolTip_ToggleGrid") );
	AddSeparator();
	AddCheckTool( ID_PRIMTYPE_CYLINDER, TEXT(""), CylinderB, CylinderB, *LocalizeUnrealEd("ToolTip_53") );
	AddCheckTool( ID_PRIMTYPE_CUBE, TEXT(""), CubeB, CubeB, *LocalizeUnrealEd("ToolTip_54") );
	AddCheckTool( ID_PRIMTYPE_SPHERE, TEXT(""), SphereB, SphereB, *LocalizeUnrealEd("ToolTip_55") );
	AddCheckTool( ID_PRIMTYPE_PLANE, TEXT(""), PlaneB, PlaneB, *LocalizeUnrealEd(TEXT("ToolTip_MaterialEditor_Toolbar_Plane")) );
	AddSeparator();
	AddTool( ID_MATERIALEDITOR_SYNCGENERICBROWSER, TEXT(""), SyncGenericBrowserB, *LocalizeUnrealEd("SyncContentBrowser"));
	AddSeparator();
	AddTool( ID_MATERIALEDITOR_SET_PREVIEW_MESH_FROM_SELECTION, TEXT(""), UseButtonB, *LocalizeUnrealEd("ToolTip_UseSelectedStaticMeshInGB") );
	AddTool( ID_MATERIALEDITOR_CLEAN_UNUSED_EXPRESSIONS, TEXT(""), CleanB, *LocalizeUnrealEd("ToolTip_CleanUnusedExpressions") );
	AddSeparator();
	AddCheckTool( ID_MATERIALEDITOR_SHOWHIDE_CONNECTORS, TEXT(""), HideConnectorsB, HideConnectorsB, *LocalizeUnrealEd("ShowHideUnusedConnectors") );
	AddSeparator();
	AddCheckTool( ID_MATERIALEDITOR_REALTIME_PREVIEW, TEXT(""), RealTimeB, RealTimeB, *LocalizeUnrealEd("ToolTip_RealtimeMaterialPreview") );
	AddCheckTool( ID_MATERIALEDITOR_REALTIME_EXPRESSIONS, TEXT(""), RealTimeB, RealTimeB, *LocalizeUnrealEd("ToolTip_RealtimeMaterialExpressions") );
	AddCheckTool( ID_MATERIALEDITOR_ALWAYS_REFRESH_ALL_PREVIEWS, TEXT(""), RealTimePreviewsB, RealTimePreviewsB, *LocalizeUnrealEd("ToolTip_RealtimeExpressionPreview") );

	AddSeparator();
	AddCheckTool(ID_MATERIALEDITOR_TOGGLESTATS, TEXT(""), ToggleMaterialStatsB, ToggleMaterialStatsB, *LocalizeUnrealEd("ToolTip_MaterialEditorToggleStats"));
	AddSeparator();
	AddCheckTool(ID_MATERIALEDITOR_VIEWSOURCE, TEXT(""), ViewSourceB, ViewSourceB, *LocalizeUnrealEd("ToolTip_MaterialEditorViewSource"));
	AddSeparator();

	SearchControl.Create(this, ID_MATERIALEDITOR_SEARCH);
	AddControl(&SearchControl);
	SearchControl.SetSize(220, 30);
	AddSeparator();

	//request flattening of a texture
	AddTool(ID_MATERIALEDITOR_FLATTEN, *LocalizeUnrealEd("Flatten"), FlattenB, FlattenB, wxITEM_NORMAL, *LocalizeUnrealEd("ToolTip_MaterialEditorFlatten"));

	Realize();
}

void WxMaterialEditorToolBar::SetHideConnectors(UBOOL bValue)
{
	ToggleTool( ID_MATERIALEDITOR_SHOWHIDE_CONNECTORS, bValue == TRUE );
}


void WxMaterialEditorToolBar::SetRealtimeExpressionPreview(UBOOL bValue)
{
	ToggleTool( ID_MATERIALEDITOR_REALTIME_EXPRESSIONS, bValue == TRUE );
}

void WxMaterialEditorToolBar::SetAlwaysRefreshAllPreviews(UBOOL bValue)
{
	ToggleTool( ID_MATERIALEDITOR_ALWAYS_REFRESH_ALL_PREVIEWS, bValue == TRUE );
}

//////////////////////////////////////////////////////////////////////////
//	WxMaterialInstanceConstantEditorToolBar
//////////////////////////////////////////////////////////////////////////
WxMaterialInstanceConstantEditorToolBar::WxMaterialInstanceConstantEditorToolBar(wxWindow* InParent, wxWindowID InID) : 
WxMaterialEditorToolBarBase(InParent, InID)
{
	// Add toolbar buttons.
	//AddCheckTool( IDM_SHOW_BACKGROUND, TEXT(""), BackgroundB, BackgroundB, *LocalizeUnrealEd("ToolTip_51") );
	AddCheckTool( ID_MATERIALEDITOR_TOGGLEGRID, TEXT(""), ToggleGridB, ToggleGridB, *LocalizeUnrealEd("ToolTip_ToggleGrid") );
	AddSeparator();
	AddCheckTool( ID_PRIMTYPE_CYLINDER, TEXT(""), CylinderB, CylinderB, *LocalizeUnrealEd("ToolTip_53") );
	AddCheckTool( ID_PRIMTYPE_CUBE, TEXT(""), CubeB, CubeB, *LocalizeUnrealEd("ToolTip_54") );
	AddCheckTool( ID_PRIMTYPE_SPHERE, TEXT(""), SphereB, SphereB, *LocalizeUnrealEd("ToolTip_55") );
	AddCheckTool( ID_PRIMTYPE_PLANE, TEXT(""), PlaneB, PlaneB, *LocalizeUnrealEd(TEXT("ToolTip_MaterialEditor_Toolbar_Plane")) );
	AddSeparator();
	AddTool( ID_MATERIALEDITOR_SYNCGENERICBROWSER, TEXT(""), SyncGenericBrowserB, *LocalizeUnrealEd("SyncContentBrowser"));
	AddSeparator();
	AddTool( ID_MATERIALEDITOR_SET_PREVIEW_MESH_FROM_SELECTION, TEXT(""), UseButtonB, *LocalizeUnrealEd("ToolTip_UseSelectedStaticMeshInGB") );
	AddSeparator();
	AddCheckTool( ID_MATERIALEDITOR_REALTIME_PREVIEW, TEXT(""), RealTimeB, RealTimeB, *LocalizeUnrealEd("ToolTip_RealtimeMaterialPreview") );
	AddSeparator();
	AddCheckTool(ID_MATERIALINSTANCE_CONSTANT_EDITOR_SHOWALLPARAMETERS, TEXT(""), ShowAllParametersB, ShowAllParametersB, *LocalizeUnrealEd("ToolTip_ShowMaterialParams"));
	Realize();
}
//////////////////////////////////////////////////////////////////////////
//	WxMaterialInstanceTimeVaryingEditorToolBar
//////////////////////////////////////////////////////////////////////////
WxMaterialInstanceTimeVaryingEditorToolBar::WxMaterialInstanceTimeVaryingEditorToolBar(wxWindow* InParent, wxWindowID InID) : 
WxMaterialEditorToolBarBase(InParent, InID)
{
	// Add toolbar buttons.
	//AddCheckTool( IDM_SHOW_BACKGROUND, TEXT(""), BackgroundB, BackgroundB, *LocalizeUnrealEd("ToolTip_51") );
	AddCheckTool( ID_MATERIALEDITOR_TOGGLEGRID, TEXT(""), ToggleGridB, ToggleGridB, *LocalizeUnrealEd("ToolTip_ToggleGrid") );
	AddSeparator();
	AddCheckTool( ID_PRIMTYPE_CYLINDER, TEXT(""), CylinderB, CylinderB, *LocalizeUnrealEd("ToolTip_53") );
	AddCheckTool( ID_PRIMTYPE_CUBE, TEXT(""), CubeB, CubeB, *LocalizeUnrealEd("ToolTip_54") );
	AddCheckTool( ID_PRIMTYPE_SPHERE, TEXT(""), SphereB, SphereB, *LocalizeUnrealEd("ToolTip_55") );
	AddCheckTool( ID_PRIMTYPE_PLANE, TEXT(""), PlaneB, PlaneB, *LocalizeUnrealEd(TEXT("ToolTip_MaterialEditor_Toolbar_Plane")) );
	AddSeparator();
	AddTool( ID_MATERIALEDITOR_SYNCGENERICBROWSER, TEXT(""), SyncGenericBrowserB, *LocalizeUnrealEd("SyncContentBrowser"));
	AddSeparator();
	AddTool( ID_MATERIALEDITOR_SET_PREVIEW_MESH_FROM_SELECTION, TEXT(""), UseButtonB, *LocalizeUnrealEd("ToolTip_UseSelectedStaticMeshInGB") );
	AddSeparator();
	AddCheckTool( ID_MATERIALEDITOR_REALTIME_PREVIEW, TEXT(""), RealTimeB, RealTimeB, *LocalizeUnrealEd("ToolTip_RealtimeMaterialPreview") );
	AddSeparator();
	AddCheckTool(ID_MATERIALINSTANCE_CONSTANT_EDITOR_SHOWALLPARAMETERS, TEXT(""), ShowAllParametersB, ShowAllParametersB, *LocalizeUnrealEd("ToolTip_ShowMaterialParams"));

	Realize();
}
