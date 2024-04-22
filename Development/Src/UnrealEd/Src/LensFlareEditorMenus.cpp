/*=============================================================================
	LensFlareEditorMenus.cpp: LensFlare editor menus
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "UnrealEd.h"
//#include "EngineLensFlareClasses.h"
#include "LensFlare.h"
#include "LensFlareEditor.h"

/*-----------------------------------------------------------------------------
	WxLensFlareEditorMenuBar
-----------------------------------------------------------------------------*/

WxLensFlareEditorMenuBar::WxLensFlareEditorMenuBar(WxLensFlareEditor* InLensFlareEditor)
{
	EditMenu = new wxMenu();
	Append( EditMenu, *LocalizeUnrealEd("Edit") );

	EditMenu->Append(IDM_LENSFLAREEDITOR_SAVE_PACKAGE, *LocalizeUnrealEd("LensFlareEditor_SavePackage"),	TEXT("") );

	ViewMenu = new wxMenu();
	Append( ViewMenu, *LocalizeUnrealEd("View") );

	ViewMenu->AppendCheckItem( IDM_LENSFLAREEDITOR_VIEW_AXES, *LocalizeUnrealEd("ViewOriginAxes"), TEXT("") );
	ViewMenu->AppendSeparator();
	ViewMenu->Append( IDM_LENSFLAREEDITOR_SAVECAM, *LocalizeUnrealEd("SaveCamPosition"), TEXT("") );
	ViewMenu->AppendSeparator();
}

WxLensFlareEditorMenuBar::~WxLensFlareEditorMenuBar()
{

}

/*-----------------------------------------------------------------------------
	WxMBLensFlareEditor
-----------------------------------------------------------------------------*/

WxMBLensFlareEditor::WxMBLensFlareEditor(WxLensFlareEditor* InLensFlareEditor)
{
	Append(IDM_MENU_LENSFLAREEDITOR_SELECT_LENSFLARE, *LocalizeUnrealEd("LensFlareEditor_SelectLensFlare"), TEXT(""));
	AppendSeparator();
	Append(IDM_LENSFLAREEDITOR_ENABLE_ELEMENT,	*LocalizeUnrealEd("LensFlareEditor_EnableElement"),		TEXT(""));
	Append(IDM_LENSFLAREEDITOR_DISABLE_ELEMENT,	*LocalizeUnrealEd("LensFlareEditor_DisableElement"),	TEXT(""));
	AppendSeparator();

	CurveMenu = new wxMenu();
	if (InLensFlareEditor->SelectedElementIndex == -1)
	{
		CurveMenu->Append(IDM_LENSFLAREEDITOR_CURVE_SCREENPERCENTAGEMAP, *LocalizeUnrealEd("AddCurve_ScreenPercentageMap"), TEXT(""));
	}
	CurveMenu->Append(IDM_LENSFLAREEDITOR_CURVE_MATERIALINDEX, *LocalizeUnrealEd("AddCurve_MaterialIndex"), TEXT(""));
	CurveMenu->Append(IDM_LENSFLAREEDITOR_CURVE_SCALING, *LocalizeUnrealEd("AddCurve_Scaling"), TEXT(""));
	CurveMenu->Append(IDM_LENSFLAREEDITOR_CURVE_AXISSCALING, *LocalizeUnrealEd("AddCurve_AxisScaling"), TEXT(""));
	CurveMenu->Append(IDM_LENSFLAREEDITOR_CURVE_ROTATION, *LocalizeUnrealEd("AddCurve_Rotation"), TEXT(""));
	CurveMenu->Append(IDM_LENSFLAREEDITOR_CURVE_COLOR, *LocalizeUnrealEd("AddCurve_Color"), TEXT(""));
	CurveMenu->Append(IDM_LENSFLAREEDITOR_CURVE_ALPHA, *LocalizeUnrealEd("AddCurve_Alpha"), TEXT(""));
	CurveMenu->Append(IDM_LENSFLAREEDITOR_CURVE_OFFSET, *LocalizeUnrealEd("AddCurve_Offset"), TEXT(""));
	CurveMenu->Append(IDM_LENSFLAREEDITOR_CURVE_DISTMAP_SCALE, *LocalizeUnrealEd("AddCurve_DistMap_Scale"), TEXT(""));
	CurveMenu->Append(IDM_LENSFLAREEDITOR_CURVE_DISTMAP_COLOR, *LocalizeUnrealEd("AddCurve_DistMap_Color"), TEXT(""));
	CurveMenu->Append(IDM_LENSFLAREEDITOR_CURVE_DISTMAP_ALPHA, *LocalizeUnrealEd("AddCurve_DistMap_Alpha"), TEXT(""));
	Append(IDMENU_LENSFLAREEDITOR_POPUP_CURVES, *LocalizeUnrealEd("CurvesMenu"), CurveMenu);
	AppendSeparator();
	Append(IDM_LENSFLAREEDITOR_ELEMENT_DUPLICATE,	*LocalizeUnrealEd("LensFlareEditor_ElementDuplicate"),		TEXT(""));
	Append(IDM_LENSFLAREEDITOR_ELEMENT_ADD,			*LocalizeUnrealEd("LensFlareEditor_ElementAdd"),			TEXT(""));
	Append(IDM_LENSFLAREEDITOR_ELEMENT_ADD_BEFORE,	*LocalizeUnrealEd("LensFlareEditor_ElementAddBefore"),		TEXT(""));
	Append(IDM_LENSFLAREEDITOR_ELEMENT_ADD_AFTER,	*LocalizeUnrealEd("LensFlareEditor_ElementAddAfter"),		TEXT(""));
	AppendSeparator();
	Append(IDM_LENSFLAREEDITOR_DELETE_ELEMENT,	*LocalizeUnrealEd("LensFlareEditor_DeleteElement"),		TEXT(""));
	Append(IDM_LENSFLAREEDITOR_RESET_ELEMENT,	*LocalizeUnrealEd("LensFlareEditor_ResetElement"),		TEXT(""));
}

WxMBLensFlareEditor::~WxMBLensFlareEditor()
{

}

/*-----------------------------------------------------------------------------
	WxLensFlareEditorPostProcessMenu
-----------------------------------------------------------------------------*/
/***
WxLensFlareEditorPostProcessMenu::WxLensFlareEditorPostProcessMenu(WxLensFlareEditor* Cascade)
{
	ShowPPFlagData.AddItem(
		FCascShowPPFlagData(
			IDM_CASC_SHOWPP_BLOOM, 
			*LocalizeUnrealEd("Cascade_ShowBloom"), 
			CASC_SHOW_BLOOM
			)
		);
	ShowPPFlagData.AddItem(
		FCascShowPPFlagData(
			IDM_CASC_SHOWPP_DOF, 
			*LocalizeUnrealEd("Cascade_ShowDOF"), 
			CASC_SHOW_DOF
			)
		);
	ShowPPFlagData.AddItem(
		FCascShowPPFlagData(
			IDM_CASC_SHOWPP_MOTIONBLUR, 
			*LocalizeUnrealEd("Cascade_ShowMotionBlur"), 
			CASC_SHOW_MOTIONBLUR
			)
		);
	ShowPPFlagData.AddItem(
		FCascShowPPFlagData(
			IDM_CASC_SHOWPP_PPVOLUME, 
			*LocalizeUnrealEd("Cascade_ShowPPVolumeMaterial"), 
			CASC_SHOW_PPVOLUME
			)
		);

	for (INT i = 0; i < ShowPPFlagData.Num(); ++i)
	{
		const FCascShowPPFlagData& ShowFlagData = ShowPPFlagData(i);

		AppendCheckItem(ShowFlagData.ID, *ShowFlagData.Name);
		if (Cascade->PreviewVC)
		{
			if ((Cascade->PreviewVC->ShowPPFlags & ShowFlagData.Mask) != 0)
			{
				Check(ShowFlagData.ID, TRUE);
			}
			else
			{
				Check(ShowFlagData.ID, FALSE);
			}
		}
		else
		{
			Check(ShowFlagData.ID, FALSE);
		}
	}
}

WxLensFlareEditorPostProcessMenu::~WxLensFlareEditorPostProcessMenu()
{
}
***/

/*-----------------------------------------------------------------------------
	WxLensFlareEditorToolBar
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxLensFlareEditorToolBar, WxToolBar )
END_EVENT_TABLE()

WxLensFlareEditorToolBar::WxLensFlareEditorToolBar( wxWindow* InParent, wxWindowID InID ) :
	WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
{
	LensFlareEditor	= (WxLensFlareEditor*)InParent;

	ResetInLevelB.Load( TEXT("CASC_ResetInLevel") );
	SaveCamB.Load( TEXT("CASC_SaveCam") );
	OrbitModeB.Load(TEXT("CASC_OrbitMode"));
	WireframeB.Load(TEXT("CASC_Wireframe"));
	BoundsB.Load(TEXT("CASC_Bounds"));
	PostProcessB.Load(TEXT("CASC_PostProcess"));
	ToggleGridB.Load(TEXT("CASC_ToggleGrid"));

	BackgroundColorB.Load(TEXT("CASC_BackColor"));

	UndoB.Load(TEXT("CASC_Undo"));
	RedoB.Load(TEXT("CASC_Redo"));
    
	RealtimeB.Load(TEXT("CASC_Realtime"));

	SyncGenericBrowserB.Load(TEXT("CASC_Prop_Browse"));

	SetToolBitmapSize( wxSize( 18, 18 ) );

	AddTool( IDM_LENSFLAREEDITOR_RESETINLEVEL, ResetInLevelB, *LocalizeUnrealEd("ResetInLevel") );

	AddSeparator();

	AddTool( IDM_LENSFLAREEDITOR_SAVECAM, SaveCamB, *LocalizeUnrealEd("SaveCameraPosition") );

	AddSeparator();

	AddTool(IDM_LENSFLAREEDITOR_SYNCGENERICBROWSER, SyncGenericBrowserB, *LocalizeUnrealEd("SyncContentBrowser"));

	AddSeparator();
	AddCheckTool(IDM_LENSFLAREEDITOR_ORBITMODE, *LocalizeUnrealEd("ToggleOrbitMode"), OrbitModeB, wxNullBitmap, *LocalizeUnrealEd("ToggleOrbitMode"));
	ToggleTool(IDM_LENSFLAREEDITOR_ORBITMODE, TRUE);
	AddCheckTool(IDM_LENSFLAREEDITOR_WIREFRAME, *LocalizeUnrealEd("ToggleWireframe"), WireframeB, wxNullBitmap, *LocalizeUnrealEd("ToggleWireframe"));
	ToggleTool(IDM_LENSFLAREEDITOR_WIREFRAME, FALSE);
	AddCheckTool(IDM_LENSFLAREEDITOR_BOUNDS, *LocalizeUnrealEd("ToggleBounds"), BoundsB, wxNullBitmap, *LocalizeUnrealEd("ToggleBounds"));
	ToggleTool(IDM_LENSFLAREEDITOR_BOUNDS, FALSE);
	AddTool(IDM_LENSFLAREEDITOR_POSTPROCESS, PostProcessB, *LocalizeUnrealEd("TogglePostProcess"));
	AddCheckTool(IDM_LENSFLAREEDITOR_TOGGLEGRID, *LocalizeUnrealEd("Casc_ToggleGrid"), ToggleGridB, wxNullBitmap, *LocalizeUnrealEd("Casc_ToggleGrid"));
	ToggleTool(IDM_LENSFLAREEDITOR_TOGGLEGRID, TRUE);

	AddSeparator();

	AddCheckTool(IDM_LENSFLAREEDITOR_REALTIME, *LocalizeUnrealEd("ToggleRealtime"), RealtimeB, wxNullBitmap, *LocalizeUnrealEd("ToggleRealtime"));
	ToggleTool(IDM_LENSFLAREEDITOR_REALTIME, TRUE);
	bRealtime	= TRUE;

	AddSeparator();

	AddTool(IDM_LENSFLAREEDITOR_BACKGROUND_COLOR, BackgroundColorB, *LocalizeUnrealEd("BackgroundColor"));

	AddSeparator();

	AddTool(IDM_LENSFLAREEDITOR_UNDO, UndoB, *LocalizeUnrealEd("Undo"));
	AddTool(IDM_LENSFLAREEDITOR_REDO, RedoB, *LocalizeUnrealEd("Redo"));

	AddSeparator();

	Realize();
/***
	if (Cascade && Cascade->EditorOptions)
	{
		if (Cascade->EditorOptions->bShowGrid == TRUE)
		{
			ToggleTool(IDM_LENSFLAREEDITOR_TOGGLEGRID, TRUE);
		}
		else
		{
			ToggleTool(IDM_LENSFLAREEDITOR_TOGGLEGRID, FALSE);
		}
	}
***/
}

WxLensFlareEditorToolBar::~WxLensFlareEditorToolBar()
{
}
