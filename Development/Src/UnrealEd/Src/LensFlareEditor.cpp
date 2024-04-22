/*=============================================================================
	LensFlareEditor.cpp: LensFlare editor
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"

#include "CurveEd.h"
//#include "EngineLensFlareClasses.h"
#include "LensFlare.h"
#include "EngineMaterialClasses.h"
#include "PropertyWindow.h"
#include "LensFlareEditor.h"

//IMPLEMENT_CLASS(ULensFlareEditorElementStub);
IMPLEMENT_CLASS(ULensFlareEditorOptions);
IMPLEMENT_CLASS(ULensFlarePreviewComponent);
IMPLEMENT_CLASS(ULensFlareEditorPropertyWrapper);

#define INDEX_LENSFLARE		-2
/*-----------------------------------------------------------------------------
	ULensFlareEditorPropertyWrapper
-----------------------------------------------------------------------------*/
void ULensFlareEditorPropertyWrapper::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);
}

void ULensFlareEditorPropertyWrapper::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/*-----------------------------------------------------------------------------
	FLensFlareEditorNotifyHook
-----------------------------------------------------------------------------*/
void FLensFlareEditorNotifyHook::NotifyDestroy(void* Src)
{
	if (WindowOfInterest == Src)
	{
	}
}

/*-----------------------------------------------------------------------------
	WxLensFlareEditor
-----------------------------------------------------------------------------*/


// Undo/Redo support
bool WxLensFlareEditor::BeginTransaction(const TCHAR* pcTransaction)
{
	if (TransactionInProgress())
	{
		FString kError(*LocalizeUnrealEd("Error_FailedToBeginTransaction"));
		kError += kTransactionName;
		check(!*kError);
		return FALSE;
	}

	GEditor->BeginTransaction(pcTransaction);
	kTransactionName = FString(pcTransaction);
	bTransactionInProgress = TRUE;

	ModifyLensFlare();

	return TRUE;
}

bool WxLensFlareEditor::EndTransaction(const TCHAR* pcTransaction)
{
	if (!TransactionInProgress())
	{
		FString kError(*LocalizeUnrealEd("Error_FailedToEndTransaction"));
		kError += kTransactionName;
		check(!*kError);
		return FALSE;
	}

	if (appStrcmp(*kTransactionName, pcTransaction) != 0)
	{
		return FALSE;
	}

	GEditor->EndTransaction();

	kTransactionName = TEXT("");
	bTransactionInProgress = FALSE;

	return TRUE;
}

bool WxLensFlareEditor::TransactionInProgress()
{
	return bTransactionInProgress;
}

void WxLensFlareEditor::ModifyLensFlare()
{
	if (LensFlare)
	{
		LensFlare->Modify();
	}

	if (LensFlareComp)
	{
		LensFlareComp->Modify();
	}
}

void WxLensFlareEditor::LensFlareEditorUndo()
{
	FlushRenderingCommands();

	GEditor->UndoTransaction();
	LensFlareEditorTouch();

	ResetLensFlareInLevel();
}

void WxLensFlareEditor::LensFlareEditorRedo()
{
	FlushRenderingCommands();

	GEditor->RedoTransaction();
	LensFlareEditorTouch();

	ResetLensFlareInLevel();
}

void WxLensFlareEditor::LensFlareEditorTouch()
{
	// 'Refresh' the viewport
	if (LensFlareComp)
	{
		LensFlareComp->BeginDeferredReattach();
	}
	ElementEdVC->Viewport->Invalidate();
}

void WxLensFlareEditor::ResetLensFlareInLevel()
{
	FlushRenderingCommands();

	// Reset all the templates on actors in the level
	for (TObjectIterator<ULensFlareComponent> It;It;++It)
	{
		if (It->Template == LensFlare)
		{
			ULensFlareComponent* LocalLFComp = *It;

			if (LocalLFComp->Template == LensFlare)
			{
				UBOOL bForceSet = TRUE;
				//force template to update
				LocalLFComp->SetTemplate(LensFlare, bForceSet);
			}
		}
	}
	//make sure non-realtime viewport redraw
	GEditor->RedrawLevelEditingViewports();
}

// PostProces
/**
 *	Update the post process chain according to the show options
 */
void WxLensFlareEditor::UpdatePostProcessChain()
{
}

BEGIN_EVENT_TABLE(WxLensFlareEditor, wxFrame)
	EVT_SIZE(WxLensFlareEditor::OnSize)
	EVT_MENU( IDM_MENU_LENSFLAREEDITOR_SELECT_LENSFLARE, WxLensFlareEditor::OnSelectLensFlare )
	EVT_MENU( IDM_LENSFLAREEDITOR_CURVE_SCREENPERCENTAGEMAP, WxLensFlareEditor::OnAddCurve )
	EVT_MENU( IDM_LENSFLAREEDITOR_CURVE_MATERIALINDEX, WxLensFlareEditor::OnAddCurve )
	EVT_MENU( IDM_LENSFLAREEDITOR_CURVE_SCALING, WxLensFlareEditor::OnAddCurve )
	EVT_MENU( IDM_LENSFLAREEDITOR_CURVE_AXISSCALING, WxLensFlareEditor::OnAddCurve )
	EVT_MENU( IDM_LENSFLAREEDITOR_CURVE_ROTATION, WxLensFlareEditor::OnAddCurve )
	EVT_MENU( IDM_LENSFLAREEDITOR_CURVE_COLOR, WxLensFlareEditor::OnAddCurve )
	EVT_MENU( IDM_LENSFLAREEDITOR_CURVE_ALPHA, WxLensFlareEditor::OnAddCurve )
	EVT_MENU( IDM_LENSFLAREEDITOR_CURVE_OFFSET, WxLensFlareEditor::OnAddCurve )
	EVT_MENU( IDM_LENSFLAREEDITOR_CURVE_DISTMAP_SCALE, WxLensFlareEditor::OnAddCurve )
	EVT_MENU( IDM_LENSFLAREEDITOR_CURVE_DISTMAP_COLOR, WxLensFlareEditor::OnAddCurve )
	EVT_MENU( IDM_LENSFLAREEDITOR_CURVE_DISTMAP_ALPHA, WxLensFlareEditor::OnAddCurve )
	EVT_MENU( IDM_LENSFLAREEDITOR_ELEMENT_DUPLICATE, WxLensFlareEditor::OnDuplicateElement )
	EVT_MENU( IDM_LENSFLAREEDITOR_ELEMENT_ADD, WxLensFlareEditor::OnElementAdd )
	EVT_MENU( IDM_LENSFLAREEDITOR_ELEMENT_ADD_BEFORE, WxLensFlareEditor::OnElementAddBefore )
	EVT_MENU( IDM_LENSFLAREEDITOR_ELEMENT_ADD_AFTER, WxLensFlareEditor::OnElementAddAfter )
	EVT_MENU( IDM_LENSFLAREEDITOR_RESET_ELEMENT, WxLensFlareEditor::OnResetElement )
	EVT_MENU( IDM_LENSFLAREEDITOR_DELETE_ELEMENT, WxLensFlareEditor::OnDeleteElement )
	EVT_MENU( IDM_LENSFLAREEDITOR_RESETINLEVEL, WxLensFlareEditor::OnResetInLevel )
	EVT_MENU( IDM_LENSFLAREEDITOR_SAVECAM, WxLensFlareEditor::OnSaveCam )
	EVT_MENU( IDM_LENSFLAREEDITOR_SYNCGENERICBROWSER, WxLensFlareEditor::OnSyncGenericBrowser )
	EVT_TOOL( IDM_LENSFLAREEDITOR_ORBITMODE, WxLensFlareEditor::OnOrbitMode )
	EVT_TOOL(IDM_LENSFLAREEDITOR_WIREFRAME, WxLensFlareEditor::OnWireframe)
	EVT_TOOL(IDM_LENSFLAREEDITOR_BOUNDS, WxLensFlareEditor::OnBounds)
	EVT_TOOL(IDM_LENSFLAREEDITOR_POSTPROCESS, WxLensFlareEditor::OnPostProcess)
	EVT_TOOL(IDM_LENSFLAREEDITOR_TOGGLEGRID, WxLensFlareEditor::OnToggleGrid)
	EVT_TOOL(IDM_LENSFLAREEDITOR_REALTIME, WxLensFlareEditor::OnRealtime)
	EVT_TOOL(IDM_LENSFLAREEDITOR_BACKGROUND_COLOR, WxLensFlareEditor::OnBackgroundColor)
	EVT_TOOL(IDM_LENSFLAREEDITOR_UNDO, WxLensFlareEditor::OnUndo)
	EVT_TOOL(IDM_LENSFLAREEDITOR_REDO, WxLensFlareEditor::OnRedo)
	EVT_MENU( IDM_LENSFLAREEDITOR_VIEW_AXES, WxLensFlareEditor::OnViewAxes )
	EVT_MENU(IDM_LENSFLAREEDITOR_SAVE_PACKAGE, WxLensFlareEditor::OnSavePackage)
	EVT_MENU(IDM_LENSFLAREEDITOR_SHOWPP_BLOOM, WxLensFlareEditor::OnShowPPBloom)
	EVT_MENU(IDM_LENSFLAREEDITOR_SHOWPP_DOF, WxLensFlareEditor::OnShowPPDOF)
	EVT_MENU(IDM_LENSFLAREEDITOR_SHOWPP_MOTIONBLUR, WxLensFlareEditor::OnShowPPMotionBlur)
	EVT_MENU(IDM_LENSFLAREEDITOR_SHOWPP_PPVOLUME, WxLensFlareEditor::OnShowPPVolumeMaterial)
END_EVENT_TABLE()


#define LENSFLAREEDITOR_NUM_SASHES		4

WxLensFlareEditor::WxLensFlareEditor(wxWindow* InParent, wxWindowID InID, ULensFlare* InLensFlare) : 
	wxFrame(InParent, InID, TEXT(""), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR, 
		InLensFlare ? *(InLensFlare->GetPathName()) : TEXT("EMPTY")),
	FDockingParent(this),
	MenuBar(NULL),
	ToolBar(NULL),
	PreviewVC(NULL),
	LensFlareElementWrapper(NULL)
{
	DefaultPostProcessName = TEXT("");
    DefaultPostProcess = NULL;

	EditorOptions = ConstructObject<ULensFlareEditorOptions>(ULensFlareEditorOptions::StaticClass());
	check(EditorOptions);

	LensFlareElementWrapper = ConstructObject<ULensFlareEditorPropertyWrapper>(ULensFlareEditorPropertyWrapper::StaticClass());
	check(LensFlareElementWrapper);

	// Load the desired window position from .ini file
	FWindowUtil::LoadPosSize(TEXT("LensFlareEditorEditor"), this, 256, 256, 1024, 768);
	
	// Set up pointers to interp objects
	LensFlare = InLensFlare;

	// Set up for undo/redo!
	LensFlare->SetFlags(RF_Transactional);

	// Nothing selected initially
	SelectedElementIndex = INDEX_LENSFLARE;

	CurveToReplace = NULL;

	bOrbitMode = TRUE;
	bWireframe = FALSE;
	bBounds = FALSE;

	bTransactionInProgress = FALSE;

	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create(this, this);

	// Create particle system preview
	WxLensFlareEditorPreview* PreviewWindow = new WxLensFlareEditorPreview( this, -1, this );
	PreviewVC = PreviewWindow->LensFlareEditorPreviewVC;
//	PreviewVC->SetPreviewCamera(LensFlare->ThumbnailAngle, LensFlare->ThumbnailDistance);
	PreviewVC->SetViewLocationForOrbiting( FVector(0.f,0.f,0.f) );
	if (EditorOptions->bShowGrid == TRUE)
	{
		PreviewVC->ShowFlags |= SHOW_Grid;
	}
	else
	{
		PreviewVC->ShowFlags &= ~SHOW_Grid;
	}

	UpdatePostProcessChain();

	// Create new curve editor setup if not already done
	if (!LensFlare->CurveEdSetup)
	{
		LensFlare->CurveEdSetup = ConstructObject<UInterpCurveEdSetup>( 
			UInterpCurveEdSetup::StaticClass(), LensFlare, NAME_None, 
			RF_NotForClient | RF_NotForServer | RF_Transactional );
	}

	// Create graph editor to work on systems CurveEd setup.
	CurveEd = new WxCurveEditor( this, -1, LensFlare->CurveEdSetup );
	// Register this window with the Curve editor so we will be notified of various things.
	CurveEd->SetNotifyObject(this);

	// Create emitter editor
	LensFlareElementEdWindow = new WxLensFlareElementEd(this, -1, this);
	ElementEdVC = LensFlareElementEdWindow->ElementEdVC;

	// Create Docking Windows
	{
		AddDockingWindow(PropertyWindow, FDockingParent::DH_Bottom, *FString::Printf( LocalizeSecure(LocalizeUnrealEd(TEXT("PropertiesCaption_F")), *LensFlare->GetName()) ), *LocalizeUnrealEd(TEXT("Properties")) );
		AddDockingWindow(CurveEd, FDockingParent::DH_Bottom, *FString::Printf( LocalizeSecure(LocalizeUnrealEd(TEXT("CurveEditorCaption_F")), *LensFlare->GetName()) ), *LocalizeUnrealEd(TEXT("CurveEditor")) );
		AddDockingWindow(PreviewWindow, FDockingParent::DH_Left, *FString::Printf( LocalizeSecure(LocalizeUnrealEd(TEXT("PreviewCaption_F")), *LensFlare->GetName()) ), *LocalizeUnrealEd(TEXT("Preview")) );
		
		SetDockHostSize(FDockingParent::DH_Left, 500);

		AddDockingWindow( LensFlareElementEdWindow, FDockingParent::DH_None, NULL );

		// Try to load a existing layout for the docking windows.
		LoadDockingLayout();
	}

	// Create menu bar
	MenuBar = new WxLensFlareEditorMenuBar(this);
	AppendWindowMenu(MenuBar);
	SetMenuBar( MenuBar );

	// Create tool bar
	ToolBar	= NULL;
	ToolBar = new WxLensFlareEditorToolBar( this, -1 );
	SetToolBar( ToolBar );

	// Set window title to particle system we are editing.
	SetTitle( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("LensFlareEditorCaption_F"), *LensFlare->GetName()) ) );

	// Set the lensflare component to use the LensFlare we are editing.
	LensFlareComp->SetTemplate(LensFlare);

	SetSelectedElement(-1);

	PreviewVC->BackgroundColor = EditorOptions->LFED_BackgroundColor;
}

WxLensFlareEditor::~WxLensFlareEditor()
{
	GEditor->ResetTransaction(TEXT("QuitLensFlareEditor"));

	// Save the desired window position to the .ini file
	FWindowUtil::SavePosSize(TEXT("LensFlareEditorEditor"), this);
	
	SaveDockingLayout();

#if WITH_MANAGED_CODE
	UnBindColorPickers(this);
#endif
	// Destroy preview viewport before we destroy the level.
	GEngine->Client->CloseViewport(PreviewVC->Viewport);
	PreviewVC->Viewport = NULL;

	delete PreviewVC;
	PreviewVC = NULL;

	delete PropertyWindow;
}

wxToolBar* WxLensFlareEditor::OnCreateToolBar(long style, wxWindowID id, const wxString& name)
{
	if (name == TEXT("LensFlareEditor"))
	{
		return new WxLensFlareEditorToolBar(this, -1);
	}

	wxToolBar* ReturnToolBar = OnCreateToolBar(style, id, name);
	return ReturnToolBar;
}

void WxLensFlareEditor::Serialize(FArchive& Ar)
{
	PreviewVC->Serialize(Ar);

	Ar << EditorOptions;
//	Ar << SelectedElementPropertyStub;
	Ar << LensFlareElementWrapper;
}

/**
 * Pure virtual that must be overloaded by the inheriting class. It will
 * be called from within UnLevTick.cpp after ticking all actors.
 *
 * @param DeltaTime	Game time passed since the last call.
 */
void WxLensFlareEditor::Tick( FLOAT DeltaTime )
{
}

// FCurveEdNotifyInterface
/**
 *	PreEditCurve
 *	Called by the curve editor when N curves are about to change
 *
 *	@param	CurvesAboutToChange		An array of the curves about to change
 */
void WxLensFlareEditor::PreEditCurve(TArray<UObject*> CurvesAboutToChange)
{
	debugf(TEXT("LensFlareEditor: PreEditCurve - %2d curves"), CurvesAboutToChange.Num());

	BeginTransaction(*LocalizeUnrealEd("CurveEdit"));
	ModifyLensFlare();

	// Call Modify on all tracks with keys selected
	for (INT i = 0; i < CurvesAboutToChange.Num(); i++)
	{
		// If this keypoint is from a distribution, call Modify on it to back up its state.
		UDistributionFloat* DistFloat = Cast<UDistributionFloat>(CurvesAboutToChange(i));
		if (DistFloat)
		{
			DistFloat->Modify();
		}
		UDistributionVector* DistVector = Cast<UDistributionVector>(CurvesAboutToChange(i));
		if (DistVector)
		{
			DistVector->Modify();
		}
	}

	// Store off the changing curves...
	ChangingCurves.Empty();
	ChangingCurves = CurvesAboutToChange;
}

/**
 *	PostEditCurve
 *	Called by the curve editor when the edit has completed.
 */
void WxLensFlareEditor::PostEditCurve()
{
	debugf(TEXT("LensFlareEditor: PostEditCurve"));

	UBOOL bRebuildPropWindow = FALSE;
	// Find the curves that changed and update the property window...
	for (INT CurveIndex = 0; CurveIndex < ChangingCurves.Num(); CurveIndex++)
	{
		UObject* CheckCurve = ChangingCurves(CurveIndex);
		if (CheckCurve)
		{
			// We don't need to worry about the screenpercentagemap as there is no dupe object for it...

			// Find the element that owns it...
			for (INT CheckElementIndex = -1; CheckElementIndex < LensFlare->Reflections.Num(); CheckElementIndex++)
			{
				if (CheckElementIndex != SelectedElementIndex) 
				{
					continue;
				}

				FLensFlareElement* DstElement = &(LensFlareElementWrapper->Element);
				FLensFlareElement* Element = NULL;
				if (CheckElementIndex == -1)
				{
					Element = &LensFlare->SourceElement;
				}
				else
				{
					Element = &LensFlare->Reflections(CheckElementIndex);
				}

				if (Element)
				{
					if (Element->LFMaterialIndex.Distribution == CheckCurve)
					{
						DstElement->DuplicateDistribution_Float(Element->LFMaterialIndex, LensFlare, DstElement->LFMaterialIndex);
						bRebuildPropWindow = TRUE;
					}
					if (Element->Scaling.Distribution == CheckCurve)
					{
						DstElement->DuplicateDistribution_Float(Element->Scaling, LensFlare, DstElement->Scaling);
						bRebuildPropWindow = TRUE;
					}
					if (Element->AxisScaling.Distribution == CheckCurve)
					{
						DstElement->DuplicateDistribution_Vector(Element->AxisScaling, LensFlare, DstElement->AxisScaling);
						bRebuildPropWindow = TRUE;
					}
					if (Element->Rotation.Distribution == CheckCurve)
					{
						DstElement->DuplicateDistribution_Float(Element->Rotation, LensFlare, DstElement->Rotation);
						bRebuildPropWindow = TRUE;
					}
					if (Element->Color.Distribution == CheckCurve)
					{
						DstElement->DuplicateDistribution_Vector(Element->Color, LensFlare, DstElement->Color);
						bRebuildPropWindow = TRUE;
					}
					if (Element->Alpha.Distribution == CheckCurve)
					{
						DstElement->DuplicateDistribution_Float(Element->Alpha, LensFlare, DstElement->Alpha);
						bRebuildPropWindow = TRUE;
					}
					if (Element->Offset.Distribution == CheckCurve)
					{
						DstElement->DuplicateDistribution_Vector(Element->Offset, LensFlare, DstElement->Offset);
						bRebuildPropWindow = TRUE;
					}
					if (Element->DistMap_Scale.Distribution == CheckCurve)
					{
						DstElement->DuplicateDistribution_Vector(Element->DistMap_Scale, LensFlare, DstElement->DistMap_Scale);
						bRebuildPropWindow = TRUE;
					}
					if (Element->DistMap_Color.Distribution == CheckCurve)
					{
						DstElement->DuplicateDistribution_Vector(Element->DistMap_Color, LensFlare, DstElement->DistMap_Color);
						bRebuildPropWindow = TRUE;
					}
					if (Element->DistMap_Alpha.Distribution == CheckCurve)
					{
						DstElement->DuplicateDistribution_Float(Element->DistMap_Alpha, LensFlare, DstElement->DistMap_Alpha);
						bRebuildPropWindow = TRUE;
					}
				}
			}
		}
	}
	
	EndTransaction(*LocalizeUnrealEd("CurveEdit"));

	if (bRebuildPropWindow == TRUE)
	{
		PropertyWindow->Rebuild();
		LensFlareEditorTouch();
		ResetLensFlareInLevel();
	}
}

/**
 *	MovedKey
 *	Called by the curve editor when a key has been moved.
 */
void WxLensFlareEditor::MovedKey()
{
}

/**
 *	DesireUndo
 *	Called by the curve editor when an Undo is requested.
 */
void WxLensFlareEditor::DesireUndo()
{
	debugf(TEXT("LensFlareEditor: DesireUndo"));
	LensFlareEditorUndo();
}

/**
 *	DesireRedo
 *	Called by the curve editor when an Redo is requested.
 */
void WxLensFlareEditor::DesireRedo()
{
	debugf(TEXT("LensFlareEditor: DesireRedo"));
	LensFlareEditorRedo();
}

void WxLensFlareEditor::OnSize( wxSizeEvent& In )
{
	In.Skip();
	Refresh();
}

///////////////////////////////////////////////////////////////////////////////////////
// Menu Callbacks

void WxLensFlareEditor::OnDuplicateElement( wxCommandEvent& In )
{
	BeginTransaction(TEXT("ElementDuplicate"));
	LensFlare->PreEditChange(NULL);
	LensFlareComp->PreEditChange(NULL);

	// Insert the value at the current index
	INT NewIndex = LensFlare->Reflections.AddZeroed();
	FLensFlareElement* NewElement = &(LensFlare->Reflections(NewIndex));

	FLensFlareElement* CopySource = NULL;
	if (SelectedElementIndex == -1)
	{
		CopySource = &(LensFlare->SourceElement);
	}
	else
	{
		CopySource = &(LensFlare->Reflections(SelectedElementIndex));
	}

	NewElement->DuplicateFromSource(*CopySource, LensFlare);

	LensFlare->PostEditChange();
	LensFlare->MarkPackageDirty();

	EndTransaction(TEXT("ElementDuplicate"));

	ResetLensFlareInLevel();

	SetSelectedElement(NewIndex);

	// Refresh viewport
	ElementEdVC->Viewport->Invalidate();
}

void WxLensFlareEditor::OnDeleteElement(wxCommandEvent& In)
{
	DeleteSelectedElement();
}

void WxLensFlareEditor::OnSelectLensFlare( wxCommandEvent& In )
{
	SelectedElementIndex = INDEX_LENSFLARE;
	SelectedElement = NULL;
	PropertyWindow->SetObject(LensFlare, EPropertyWindowFlags::ShouldShowCategories);
}

void WxLensFlareEditor::OnAddCurve( wxCommandEvent& In )
{
	if (LensFlare == NULL)
	{
		return;
	}

	FString CurveName;
	switch (In.GetId())
	{
	case IDM_LENSFLAREEDITOR_CURVE_SCREENPERCENTAGEMAP:	CurveName = FString(TEXT("ScreenPercentageMap"));	break;
	case IDM_LENSFLAREEDITOR_CURVE_MATERIALINDEX:		CurveName = FString(TEXT("LFMaterialIndex"));		break;
	case IDM_LENSFLAREEDITOR_CURVE_SCALING:				CurveName = FString(TEXT("Scaling"));				break;
	case IDM_LENSFLAREEDITOR_CURVE_AXISSCALING:			CurveName = FString(TEXT("AxisScaling"));			break;
	case IDM_LENSFLAREEDITOR_CURVE_ROTATION:			CurveName = FString(TEXT("Rotation"));				break;
	case IDM_LENSFLAREEDITOR_CURVE_COLOR:				CurveName = FString(TEXT("Color"));					break;
	case IDM_LENSFLAREEDITOR_CURVE_ALPHA:				CurveName = FString(TEXT("Alpha"));					break;
	case IDM_LENSFLAREEDITOR_CURVE_OFFSET:				CurveName = FString(TEXT("Offset"));				break;
	case IDM_LENSFLAREEDITOR_CURVE_DISTMAP_SCALE:		CurveName = FString(TEXT("DistMap_Scale"));			break;
	case IDM_LENSFLAREEDITOR_CURVE_DISTMAP_COLOR:		CurveName = FString(TEXT("DistMap_Color"));			break;
	case IDM_LENSFLAREEDITOR_CURVE_DISTMAP_ALPHA:		CurveName = FString(TEXT("DistMap_Alpha"));			break;
	}

	if (In.GetId() == IDM_LENSFLAREEDITOR_CURVE_SCREENPERCENTAGEMAP)
	{
		LensFlare->AddElementCurveToEditor(SelectedElementIndex, CurveName, LensFlare->CurveEdSetup);
	}
	else
	{
		LensFlare->AddElementCurveToEditor(SelectedElementIndex, CurveName, LensFlare->CurveEdSetup);
/***
		if (LensFlareElementWrapper != NULL)
		{
			UObject* Curve = LensFlareElementWrapper->Element.GetCurve(CurveName);
			if (Curve)
			{
				LensFlare->CurveEdSetup->AddCurveToCurrentTab(Curve, CurveName, FColor(255,0,0), TRUE, TRUE);
			}
		}
***/
	}
	SetSelectedInCurveEditor();
	CurveEd->CurveEdVC->Viewport->Invalidate();
}

void WxLensFlareEditor::OnElementAdd( wxCommandEvent& In )
{
	BeginTransaction(TEXT("ElementAdd"));
	LensFlare->PreEditChange(NULL);
	LensFlareComp->PreEditChange(NULL);

	// Insert the value at the current index
	INT NewIndex = LensFlare->Reflections.AddZeroed();
	LensFlare->InitializeElement(NewIndex);

	LensFlareComp->PostEditChange();
	LensFlare->PostEditChange();
	LensFlare->MarkPackageDirty();

	EndTransaction(TEXT("ElementAdd"));

	ResetLensFlareInLevel();

	SetSelectedElement(NewIndex);

	// Refresh viewport
	ElementEdVC->Viewport->Invalidate();
}

void WxLensFlareEditor::OnElementAddBefore( wxCommandEvent& In )
{
	if (SelectedElementIndex == -1)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("Error_LensFlareEditor_CantAddBeforeSource"));
		return;
	}

	BeginTransaction(TEXT("ElementAddBefore"));
	LensFlare->PreEditChange(NULL);
	LensFlareComp->PreEditChange(NULL);

	// Insert the value at the current index
	INT NewIndex = LensFlare->Reflections.AddZeroed();
	LensFlare->InitializeElement(NewIndex);

	FLensFlareElement* SelectedElement = &(LensFlare->Reflections(SelectedElementIndex));

	// Set the ray distance to 1/2 between the neighboring elements
	FLensFlareElement* CheckElement;
	FLOAT RightDistance = SelectedElement->RayDistance;
	FLOAT LeftDistance = RightDistance;
	FLOAT CurrDiff = 0.0f;

	if (LensFlare->SourceElement.RayDistance < RightDistance)
	{
		LeftDistance = LensFlare->SourceElement.RayDistance;
		CurrDiff = LeftDistance - RightDistance;
	}

	UBOOL bFound = FALSE;

	for (INT ElementIndex = 0; ElementIndex < LensFlare->Reflections.Num(); ElementIndex++)
	{
		CheckElement = &(LensFlare->Reflections(ElementIndex));
		if (CheckElement->RayDistance > RightDistance)
		{
			// Skip it
			continue;
		}

		if (CheckElement == SelectedElement)
		{
			continue;
		}

		if ((CheckElement->RayDistance - RightDistance) > CurrDiff)
		{
			LeftDistance = CheckElement->RayDistance;
			CurrDiff = LeftDistance - RightDistance;
			bFound = TRUE;
		}
	}

	if (!bFound)
	{
		CurrDiff = RightDistance;
		if (CurrDiff > 0.0f)
		{
			CurrDiff = -CurrDiff;
		}
	}

	FLensFlareElement* NewElement = &(LensFlare->Reflections(NewIndex));
	NewElement->RayDistance = RightDistance + (CurrDiff / 2.0f);

	LensFlareComp->PostEditChange();
	LensFlare->PostEditChange();
	LensFlare->MarkPackageDirty();

	EndTransaction(TEXT("ElementAddBefore"));

	SetSelectedElement(NewIndex);

	ResetLensFlareInLevel();

	// Refresh viewport
	ElementEdVC->Viewport->Invalidate();
}

void WxLensFlareEditor::OnElementAddAfter( wxCommandEvent& In )
{
	BeginTransaction(TEXT("ElementAddAfter"));
	LensFlare->PreEditChange(NULL);
	LensFlareComp->PreEditChange(NULL);

	// Insert the value at the current index
	INT NewIndex = LensFlare->Reflections.AddZeroed();
	LensFlare->InitializeElement(NewIndex);

	FLensFlareElement* NewElement = &(LensFlare->Reflections(NewIndex));
	FLensFlareElement* SelectedElement;

	if (SelectedElementIndex == -1)
	{
		SelectedElement = &(LensFlare->SourceElement);
	}
	else
	{
		SelectedElement = &(LensFlare->Reflections(SelectedElementIndex));
	}

	// Set the ray distance to 1/2 between the neighboring elements
	FLensFlareElement* CheckElement;
	FLOAT LeftDistance = SelectedElement->RayDistance;
	FLOAT RightDistance = LeftDistance;
	FLOAT CurrDiff = 2.0f;

	if (LensFlare->SourceElement.RayDistance > LeftDistance)
	{
		RightDistance = LensFlare->SourceElement.RayDistance;
		CurrDiff = RightDistance - LeftDistance;
	}

	UBOOL bFound = FALSE;

	for (INT ElementIndex = 0; ElementIndex < LensFlare->Reflections.Num(); ElementIndex++)
	{
		CheckElement = &(LensFlare->Reflections(ElementIndex));
		if (CheckElement->RayDistance < LeftDistance)
		{
			// Skip it
			continue;
		}

		if ((CheckElement == SelectedElement) ||
			(CheckElement == NewElement))
		{
			continue;
		}

		if ((CheckElement->RayDistance - LeftDistance) < CurrDiff)
		{
			RightDistance = CheckElement->RayDistance;
			CurrDiff = RightDistance - LeftDistance;
			bFound = TRUE;
		}
	}

	if (!bFound)
	{
		CurrDiff = LeftDistance;
		if (CurrDiff < 0.0f)
		{
			CurrDiff = -CurrDiff;
		}
	}

	NewElement->RayDistance = LeftDistance + (CurrDiff / 2.0f);

	LensFlareComp->PostEditChange();
	LensFlare->PostEditChange();
	LensFlare->MarkPackageDirty();

	EndTransaction(TEXT("ElementAddAfter"));

	SetSelectedElement(NewIndex);

	ResetLensFlareInLevel();

	// Refresh viewport
	ElementEdVC->Viewport->Invalidate();
}

void WxLensFlareEditor::OnEnableElement(wxCommandEvent& In)
{
	EnableSelectedElement();
}

void WxLensFlareEditor::OnDisableElement(wxCommandEvent& In)
{
	DisableSelectedElement();
}

void WxLensFlareEditor::OnResetElement(wxCommandEvent& In)
{
	ResetSelectedElement();
}

void WxLensFlareEditor::OnResetInLevel(wxCommandEvent& In)
{
	ResetLensFlareInLevel();
}

void WxLensFlareEditor::OnSaveCam(wxCommandEvent& In)
{
//	PartSys->ThumbnailAngle = PreviewVC->PreviewAngle;
//	PartSys->ThumbnailDistance = PreviewVC->PreviewDistance;
//	PartSys->PreviewComponent = NULL;

	PreviewVC->bCaptureScreenShot = TRUE;
}

void WxLensFlareEditor::OnSyncGenericBrowser( wxCommandEvent& In )
{
	// Sync the LensFlare in the generic browser
	if (LensFlare != NULL)
	{
		TArray<UObject*> Objects;
		Objects.AddItem(LensFlare);
		GApp->EditorFrame->SyncBrowserToObjects(Objects);
	}
}

void WxLensFlareEditor::OnSavePackage(wxCommandEvent& In)
{
	debugf(TEXT("SAVE PACKAGE"));
	if (!LensFlare)
	{
		appMsgf(AMT_OK, TEXT("No lens flare active..."));
		return;
	}

	UPackage* Package = Cast<UPackage>(LensFlare->GetOutermost());
	if (Package)
	{
		debugf(TEXT("Have a package!"));

		FString FileTypes( TEXT("All Files|*.*") );
		
		for (INT i=0; i<GSys->Extensions.Num(); i++)
		{
			FileTypes += FString::Printf( TEXT("|(*.%s)|*.%s"), *GSys->Extensions(i), *GSys->Extensions(i) );
		}

		if (FindObject<UWorld>(Package, TEXT("TheWorld")))
		{
			appMsgf(AMT_OK, *LocalizeUnrealEd("Error_CantSaveMapViaLensFlareEditor"), *Package->GetName());
		}
		else
		{
			FString ExistingFile, File, Directory;
			FString PackageName = Package->GetName();

			if (GPackageFileCache->FindPackageFile( *PackageName, NULL, ExistingFile ))
			{
				FString Filename, Extension;
				GPackageFileCache->SplitPath( *ExistingFile, Directory, Filename, Extension );
				File = FString::Printf( TEXT("%s.%s"), *Filename, *Extension );
			}
			else
			{
				Directory = TEXT("");
				File = FString::Printf( TEXT("%s.upk"), *PackageName );
			}

			WxFileDialog SaveFileDialog( this, 
				*LocalizeUnrealEd("SavePackage"), 
				*Directory,
				*File,
				*FileTypes,
				wxSAVE,
				wxDefaultPosition);

			FString SaveFileName;

			if ( SaveFileDialog.ShowModal() == wxID_OK )
			{
				SaveFileName = FString( SaveFileDialog.GetPath() );

				// If the supplied file name is missing an extension then give it the default package
				// file extension.
				if( SaveFileName.Len() > 0 && FFilename( SaveFileName ).GetExtension().Len() == 0 )
				{
					SaveFileName += TEXT( ".upk" );
				}						

				if ( GFileManager->IsReadOnly( *SaveFileName ) || !GUnrealEd->Exec( *FString::Printf(TEXT("OBJ SAVEPACKAGE PACKAGE=\"%s\" FILE=\"%s\""), *PackageName, *SaveFileName) ) )
				{
					appMsgf( AMT_OK, *LocalizeUnrealEd("Error_CouldntSavePackage") );
				}
			}
		}

		if (LensFlare)
		{
			LensFlare->PostEditChange();
		}
	}
}

void WxLensFlareEditor::OnOrbitMode(wxCommandEvent& In)
{
	bOrbitMode = In.IsChecked();

	//@todo. actually handle this...
	if (bOrbitMode)
	{
		PreviewVC->SetPreviewCamera(PreviewVC->PreviewAngle, PreviewVC->PreviewDistance);
	}
}

void WxLensFlareEditor::OnWireframe(wxCommandEvent& In)
{
	bWireframe = In.IsChecked();
	PreviewVC->bWireframe = bWireframe;
}

void WxLensFlareEditor::OnBounds(wxCommandEvent& In)
{
	bBounds = In.IsChecked();
	PreviewVC->bBounds = bBounds;
}

/**
 *	Handler for turning post processing on and off.
 *
 *	@param	In	wxCommandEvent
 */
void WxLensFlareEditor::OnPostProcess(wxCommandEvent& In)
{
}

/**
 *	Handler for turning the grid on and off.
 *
 *	@param	In	wxCommandEvent
 */
void WxLensFlareEditor::OnToggleGrid(wxCommandEvent& In)
{
	bool bShowGrid = In.IsChecked();

	if (PreviewVC)
	{
		// Toggle the grid and worldbox.
		EditorOptions->bShowGrid = bShowGrid;
		EditorOptions->SaveConfig();
		PreviewVC->DrawHelper.bDrawGrid = bShowGrid;
		if (bShowGrid)
		{
			PreviewVC->ShowFlags |= SHOW_Grid;
		}
		else
		{
			PreviewVC->ShowFlags &= ~SHOW_Grid;
		}
	}
}

void WxLensFlareEditor::OnViewAxes(wxCommandEvent& In)
{
	PreviewVC->bDrawOriginAxes = In.IsChecked();
}

void WxLensFlareEditor::OnShowPPBloom( wxCommandEvent& In )
{
}

void WxLensFlareEditor::OnShowPPDOF( wxCommandEvent& In )
{
}

void WxLensFlareEditor::OnShowPPMotionBlur( wxCommandEvent& In )
{
}

void WxLensFlareEditor::OnShowPPVolumeMaterial( wxCommandEvent& In )
{
}

void WxLensFlareEditor::OnRealtime(wxCommandEvent& In)
{
	PreviewVC->SetRealtime(In.IsChecked());
}

void WxLensFlareEditor::OnBackgroundColor(wxCommandEvent& In)
{
	FPickColorStruct PickColorStruct;
	PickColorStruct.RefreshWindows.AddItem(this);
	PickColorStruct.DWORDColorArray.AddItem(&(PreviewVC->BackgroundColor));
	PickColorStruct.FLOATColorArray.AddItem(&(EditorOptions->LFED_BackgroundColor));

	PickColor(PickColorStruct);
}

void WxLensFlareEditor::OnUndo(wxCommandEvent& In)
{
	LensFlareEditorUndo();
}

void WxLensFlareEditor::OnRedo(wxCommandEvent& In)
{
	LensFlareEditorRedo();
}

///////////////////////////////////////////////////////////////////////////////////////
// Properties window NotifyHook stuff

void WxLensFlareEditor::NotifyDestroy( void* Src )
{

}

static UDistributionFloat* PreviousScreenPercentage;
static FLensFlareElement PreviousSource;
static TArray<FLensFlareElement> PreviousReflections;
static FLensFlareElement PreviousElement;
static INT PreviousElementIndex = INDEX_LENSFLARE;

void WxLensFlareEditor::NotifyPreChange( void* Src, FEditPropertyChain* PropertyChain )
{
	BeginTransaction(TEXT("LensFlareEditorPropertyChange"));

	PreviousScreenPercentage = LensFlare->ScreenPercentageMap.Distribution;
	if (SelectedElement)
	{
		PreviousElementIndex = SelectedElementIndex;
//		PreviousElement.DuplicateFromSource(LensFlareElementWrapper->Element, UObject::GetTransientPackage());
		PreviousElement = LensFlareElementWrapper->Element;
	}
	else
	{
		PreviousElementIndex = INDEX_LENSFLARE;
	}
}

void WxLensFlareEditor::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{
#if 0
	BeginTransaction(TEXT("LensFlareEditorPropertyChange"));

	PreviousScreenPercentage = LensFlare->ScreenPercentageMap.Distribution;
	if (SelectedElement)
	{
		PreviousElementIndex = SelectedElementIndex;
		PreviousElement.DuplicateFromSource(LensFlareElementWrapper->Element, UObject::GetTransientPackage());
	}
	else
	{
		PreviousElementIndex = INDEX_LENSFLARE;
	}
#endif
}

void WxLensFlareEditor::NotifyPostChange( void* Src, FEditPropertyChain* PropertyChain )
{
	// Determine if the type of a distribution was changed, or if the value of a distribution itself changed
	UBOOL bDistributionChanged = FALSE;
	UBOOL bDistributionValueChanged = FALSE;
	FString PropertyName;

	FEditPropertyChain::TDoubleLinkedListNode* PropertyNode = PropertyChain->GetHead();
	while (PropertyNode)
	{
		UStructProperty* StructProp = Cast<UStructProperty>(PropertyNode->GetValue());
		UComponentProperty* CompProp = Cast<UComponentProperty>(PropertyNode->GetValue());
		UObjectProperty* ObjProp = Cast<UObjectProperty>(PropertyNode->GetValue());
		
		if (CompProp && (CompProp->GetName() == TEXT("Distribution")))
		{
			// The previous node will be the name of the distribution...
			if (PropertyNode->GetPrevNode() != NULL)
			{
				UStructProperty* PrevStructProp = Cast<UStructProperty>(PropertyNode->GetPrevNode()->GetValue());
				if (PrevStructProp)
				{
					PropertyName = PrevStructProp->GetName();
				}
			}

			if (PropertyNode->GetNextNode() == NULL)
			{
				// The actual value of the distribution didn't change, so assume it is the type...
				bDistributionChanged = TRUE;
			}
			else
			{
				bDistributionValueChanged = TRUE;
			}

			PropertyNode = NULL;
		}
		else
		{
			PropertyNode = PropertyNode->GetNextNode();
		}
	}

	UObject* OldCurve = NULL;
	UObject* NewCurve = NULL;
	
	// Update the ScreenPercentageMap is the type changed...
	// Otherwise changes to the value of it will be the actual source so we don't need to do anything special.
	// This is not the case for distributions associated with individual elements, which are handled below.
	if (PreviousScreenPercentage != LensFlare->ScreenPercentageMap.Distribution)
	{
		OldCurve = PreviousScreenPercentage;
		NewCurve = LensFlare->ScreenPercentageMap.Distribution;
		if (NewCurve)
		{
			LensFlare->CurveEdSetup->ReplaceCurve(OldCurve, NewCurve);
			CurveEd->CurveChanged();
		}
	}

	// If there is a selected element, update it as well
	if (SelectedElement && (PreviousElementIndex != INDEX_LENSFLARE))
	{
		FLensFlareElement* PrevElement = &PreviousElement;
		FLensFlareElement* Element = &(LensFlareElementWrapper->Element);

		if ((PrevElement != NULL) || (Element != NULL))
		{
			if (bDistributionValueChanged == TRUE)
			{
				debugf(TEXT("*** Distribution value has changed: %s"), *PropertyName);
			}
			else if (bDistributionChanged == TRUE)
			{
				debugf(TEXT("*** Distribution type has changed : %s"), *PropertyName);
			}

			if (PrevElement->ElementName != Element->ElementName)
			{
				SelectedElement->ElementName = Element->ElementName;
			}

			if (PrevElement->RayDistance != Element->RayDistance)
			{
				SelectedElement->RayDistance = Element->RayDistance;
			}

			if (PrevElement->bIsEnabled != Element->bIsEnabled)
			{
				SelectedElement->bIsEnabled = Element->bIsEnabled;
			}

			if (PrevElement->bUseSourceDistance != Element->bUseSourceDistance)
			{
				SelectedElement->bUseSourceDistance = Element->bUseSourceDistance;
			}

			if (PrevElement->bNormalizeRadialDistance != Element->bNormalizeRadialDistance)
			{
				SelectedElement->bNormalizeRadialDistance = Element->bNormalizeRadialDistance;
			}

			if (PrevElement->bModulateColorBySource != Element->bModulateColorBySource)
			{
				SelectedElement->bModulateColorBySource = Element->bModulateColorBySource;
			}

			if (PrevElement->Size != Element->Size)
			{
				SelectedElement->Size = Element->Size;
			}

			if (PrevElement->bOrientTowardsSource != Element->bOrientTowardsSource)
			{
				SelectedElement->bOrientTowardsSource = Element->bOrientTowardsSource;
			}

			SelectedElement->LFMaterials.Empty();
			for (INT MatIndex = 0; MatIndex < Element->LFMaterials.Num(); MatIndex++)
			{
				SelectedElement->LFMaterials.AddItem(Element->LFMaterials(MatIndex));
			}

			if (bDistributionChanged == TRUE)
			{
				// Check each distribution...
				if (PrevElement->LFMaterialIndex.Distribution != Element->LFMaterialIndex.Distribution)
				{
//					OldCurve = PrevElement->LFMaterialIndex.Distribution;
					OldCurve = SelectedElement->LFMaterialIndex.Distribution;
					SelectedElement->DuplicateDistribution_Float(Element->LFMaterialIndex, LensFlare, SelectedElement->LFMaterialIndex);
//					NewCurve = Element->LFMaterialIndex.Distribution;
					NewCurve = SelectedElement->LFMaterialIndex.Distribution;
				}
				else
				if (PrevElement->Scaling.Distribution != Element->Scaling.Distribution)
				{
//					OldCurve = PrevElement->Scaling.Distribution;
					OldCurve = SelectedElement->Scaling.Distribution;
					SelectedElement->DuplicateDistribution_Float(Element->Scaling, LensFlare, SelectedElement->Scaling);
//					NewCurve = Element->Scaling.Distribution;
					NewCurve = SelectedElement->Scaling.Distribution;
				}
				else
				if (PrevElement->AxisScaling.Distribution != Element->AxisScaling.Distribution)
				{
//					OldCurve = PrevElement->AxisScaling.Distribution;
					OldCurve = SelectedElement->AxisScaling.Distribution;
					SelectedElement->DuplicateDistribution_Vector(Element->AxisScaling, LensFlare, SelectedElement->AxisScaling);
//					NewCurve = Element->AxisScaling.Distribution;
					NewCurve = SelectedElement->AxisScaling.Distribution;
				}
				else
				if (PrevElement->Rotation.Distribution != Element->Rotation.Distribution)
				{
//					OldCurve = PrevElement->Rotation.Distribution;
					OldCurve = SelectedElement->Rotation.Distribution;
					SelectedElement->DuplicateDistribution_Float(Element->Rotation, LensFlare, SelectedElement->Rotation);
//					NewCurve = Element->Rotation.Distribution;
					NewCurve = SelectedElement->Rotation.Distribution;
				}
				else
				if (PrevElement->Color.Distribution != Element->Color.Distribution)
				{
//					OldCurve = PrevElement->Color.Distribution;
					OldCurve = SelectedElement->Color.Distribution;
					SelectedElement->DuplicateDistribution_Vector(Element->Color, LensFlare, SelectedElement->Color);
//					NewCurve = Element->Color.Distribution;
					NewCurve = SelectedElement->Color.Distribution;
				}
				else
				if (PrevElement->Alpha.Distribution != Element->Alpha.Distribution)
				{
//					OldCurve = PrevElement->Alpha.Distribution;
					OldCurve = SelectedElement->Alpha.Distribution;
					SelectedElement->DuplicateDistribution_Float(Element->Alpha, LensFlare, SelectedElement->Alpha);
//					NewCurve = Element->Alpha.Distribution;
					NewCurve = SelectedElement->Alpha.Distribution;
				}
				else
				if (PrevElement->Offset.Distribution != Element->Offset.Distribution)
				{
//					OldCurve = PrevElement->Offset.Distribution;
					OldCurve = SelectedElement->Offset.Distribution;
					SelectedElement->DuplicateDistribution_Vector(Element->Offset, LensFlare, SelectedElement->Offset);
//					NewCurve = Element->Offset.Distribution;
					NewCurve = SelectedElement->Offset.Distribution;
				}
				else
				if (PrevElement->DistMap_Scale.Distribution != Element->DistMap_Scale.Distribution)
				{
//					OldCurve = PrevElement->DistMap_Scale.Distribution;
					OldCurve = SelectedElement->DistMap_Scale.Distribution;
					SelectedElement->DuplicateDistribution_Vector(Element->DistMap_Scale, LensFlare, SelectedElement->DistMap_Scale);
//					NewCurve = Element->DistMap_Scale.Distribution;
					NewCurve = SelectedElement->DistMap_Scale.Distribution;
				}
				else
				if (PrevElement->DistMap_Color.Distribution != Element->DistMap_Color.Distribution)
				{
//					OldCurve = PrevElement->DistMap_Color.Distribution;
					OldCurve = SelectedElement->DistMap_Color.Distribution;
					SelectedElement->DuplicateDistribution_Vector(Element->DistMap_Color, LensFlare, SelectedElement->DistMap_Color);
//					NewCurve = Element->DistMap_Color.Distribution;
					NewCurve = SelectedElement->DistMap_Color.Distribution;
				}
				else
				if (PrevElement->DistMap_Alpha.Distribution != Element->DistMap_Alpha.Distribution)
				{
//					OldCurve = PrevElement->DistMap_Alpha.Distribution;
					OldCurve = SelectedElement->DistMap_Alpha.Distribution;
					SelectedElement->DuplicateDistribution_Float(Element->DistMap_Alpha, LensFlare, SelectedElement->DistMap_Alpha);
//					NewCurve = Element->DistMap_Alpha.Distribution;
					NewCurve = SelectedElement->DistMap_Alpha.Distribution;
				}

				if (NewCurve)
				{
					LensFlare->CurveEdSetup->ReplaceCurve(OldCurve, NewCurve);
					CurveEd->CurveChanged();
				}
			}
			
			if (bDistributionValueChanged == TRUE)
			{
				if (PropertyName == TEXT("LFMaterialIndex"))
				{
					OldCurve = SelectedElement->LFMaterialIndex.Distribution;
					SelectedElement->DuplicateDistribution_Float(Element->LFMaterialIndex, LensFlare, SelectedElement->LFMaterialIndex);
					NewCurve = SelectedElement->LFMaterialIndex.Distribution;
				}
				else if (PropertyName == TEXT("Scaling"))
				{
					OldCurve = SelectedElement->Scaling.Distribution;
					SelectedElement->DuplicateDistribution_Float(Element->Scaling, LensFlare, SelectedElement->Scaling);
					NewCurve = SelectedElement->Scaling.Distribution;
				}
				else if (PropertyName == TEXT("AxisScaling"))
				{
					OldCurve = SelectedElement->AxisScaling.Distribution;
					SelectedElement->DuplicateDistribution_Vector(Element->AxisScaling, LensFlare, SelectedElement->AxisScaling);
					NewCurve = SelectedElement->AxisScaling.Distribution;
				}
				else if (PropertyName == TEXT("Rotation"))
				{
					OldCurve = SelectedElement->Rotation.Distribution;
					SelectedElement->DuplicateDistribution_Float(Element->Rotation, LensFlare, SelectedElement->Rotation);
					NewCurve = SelectedElement->Rotation.Distribution;
				}
				else if (PropertyName == TEXT("Color"))
				{
					OldCurve = SelectedElement->Color.Distribution;
					SelectedElement->DuplicateDistribution_Vector(Element->Color, LensFlare, SelectedElement->Color);
					NewCurve = SelectedElement->Color.Distribution;
				}
				else if (PropertyName == TEXT("Alpha"))
				{
					OldCurve = SelectedElement->Alpha.Distribution;
					SelectedElement->DuplicateDistribution_Float(Element->Alpha, LensFlare, SelectedElement->Alpha);
					NewCurve = SelectedElement->Alpha.Distribution;
				}
				else if (PropertyName == TEXT("Offset"))
				{
					OldCurve = SelectedElement->Offset.Distribution;
					SelectedElement->DuplicateDistribution_Vector(Element->Offset, LensFlare, SelectedElement->Offset);
					NewCurve = SelectedElement->Offset.Distribution;
				}
				else if (PropertyName == TEXT("DistMap_Scale"))
				{
					OldCurve = SelectedElement->DistMap_Scale.Distribution;
					SelectedElement->DuplicateDistribution_Vector(Element->DistMap_Scale, LensFlare, SelectedElement->DistMap_Scale);
					NewCurve = SelectedElement->DistMap_Scale.Distribution;
				}
				else if (PropertyName == TEXT("DistMap_Color"))
				{
					OldCurve = SelectedElement->DistMap_Color.Distribution;
					SelectedElement->DuplicateDistribution_Vector(Element->DistMap_Color, LensFlare, SelectedElement->DistMap_Color);
					NewCurve = SelectedElement->DistMap_Color.Distribution;
				}
				else if (PropertyName == TEXT("DistMap_Alpha"))
				{
					OldCurve = SelectedElement->DistMap_Alpha.Distribution;
					SelectedElement->DuplicateDistribution_Float(Element->DistMap_Alpha, LensFlare, SelectedElement->DistMap_Alpha);
					NewCurve = SelectedElement->DistMap_Alpha.Distribution;
				}

				if (NewCurve)
				{
					LensFlare->CurveEdSetup->ReplaceCurve(OldCurve, NewCurve);
					CurveEd->CurveChanged();
				}
			}
		}
	}

	if (LensFlare)
	{
		LensFlare->PostEditChange();
		TArray<FLensFlareElementCurvePair> Curves;
		if (SelectedElementIndex == -1)
		{
			LensFlare->GetCurveObjects(Curves);
		}
		if (SelectedElement && LensFlareElementWrapper)
		{
			SelectedElement->GetCurveObjects(Curves);
//			LensFlareElementWrapper->Element.GetCurveObjects(Curves);
		}

		for (INT i=0; i<Curves.Num(); i++)
		{
			UDistributionFloat* DistF = Cast<UDistributionFloat>(Curves(i).CurveObject);
			UDistributionVector* DistV = Cast<UDistributionVector>(Curves(i).CurveObject);
			if (DistF)
				DistF->bIsDirty = TRUE;
			if (DistV)
				DistV->bIsDirty = TRUE;

			CurveEd->EdSetup->ChangeCurveColor(Curves(i).CurveObject, 
				FLinearColor(0.2f,0.2f,0.2f) * SelectedElementIndex);
				//SelectedElement->ModuleEditorColor);
			CurveEd->CurveChanged();
		}
	}

	LensFlare->ThumbnailImageOutOfDate = TRUE;

	check(TransactionInProgress());
	EndTransaction(TEXT("LensFlareEditorPropertyChange"));

	PreviousElementIndex = INDEX_LENSFLARE;

	LensFlareEditorTouch();
	ResetLensFlareInLevel();

	ElementEdVC->Viewport->Invalidate();
}

void WxLensFlareEditor::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	check(0 && TEXT("What are you doing in here??"));
	UObject* OldCurve = NULL;
	UObject* NewCurve = NULL;
	if (PreviousScreenPercentage != LensFlare->ScreenPercentageMap.Distribution)
	{
		OldCurve = PreviousScreenPercentage;
		NewCurve = LensFlare->ScreenPercentageMap.Distribution;
		if (NewCurve)
		{
			LensFlare->CurveEdSetup->ReplaceCurve(OldCurve, NewCurve);
			CurveEd->CurveChanged();
		}
	}

	// If there is a selected element, update it as well
	if (SelectedElement && (PreviousElementIndex != INDEX_LENSFLARE))
	{
		FLensFlareElement* PrevElement = &PreviousElement;
		FLensFlareElement* Element = &(LensFlareElementWrapper->Element);

		if ((PrevElement != NULL) || (Element != NULL))
		{
			if (PrevElement->ElementName != Element->ElementName)
			{
				SelectedElement->ElementName = Element->ElementName;
			}

			if (PrevElement->RayDistance != Element->RayDistance)
			{
				SelectedElement->RayDistance = Element->RayDistance;
			}

			if (PrevElement->bIsEnabled != Element->bIsEnabled)
			{
				SelectedElement->bIsEnabled = Element->bIsEnabled;
			}

			if (PrevElement->bUseSourceDistance != Element->bUseSourceDistance)
			{
				SelectedElement->bUseSourceDistance = Element->bUseSourceDistance;
			}

			if (PrevElement->bNormalizeRadialDistance != Element->bNormalizeRadialDistance)
			{
				SelectedElement->bNormalizeRadialDistance = Element->bNormalizeRadialDistance;
			}

			if (PrevElement->bModulateColorBySource != Element->bModulateColorBySource)
			{
				SelectedElement->bModulateColorBySource = Element->bModulateColorBySource;
			}

			if (PrevElement->Size != Element->Size)
			{
				SelectedElement->Size = Element->Size;
			}

			SelectedElement->LFMaterials.Empty();
			for (INT MatIndex = 0; MatIndex < Element->LFMaterials.Num(); MatIndex++)
			{
				SelectedElement->LFMaterials.AddItem(Element->LFMaterials(MatIndex));
			}

			// Check each distribution...
			if (PrevElement->LFMaterialIndex.Distribution != Element->LFMaterialIndex.Distribution)
			{
				OldCurve = SelectedElement->LFMaterialIndex.Distribution;
				SelectedElement->DuplicateDistribution_Float(Element->LFMaterialIndex, LensFlare, SelectedElement->LFMaterialIndex);
				NewCurve = SelectedElement->LFMaterialIndex.Distribution;
			}
			else
			if (PrevElement->Scaling.Distribution != Element->Scaling.Distribution)
			{
				OldCurve = SelectedElement->Scaling.Distribution;
				SelectedElement->DuplicateDistribution_Float(Element->Scaling, LensFlare, SelectedElement->Scaling);
				NewCurve = SelectedElement->Scaling.Distribution;
			}
			else
			if (PrevElement->AxisScaling.Distribution != Element->AxisScaling.Distribution)
			{
				OldCurve = SelectedElement->AxisScaling.Distribution;
				SelectedElement->DuplicateDistribution_Vector(Element->AxisScaling, LensFlare, SelectedElement->AxisScaling);
				NewCurve = SelectedElement->AxisScaling.Distribution;
			}
			else
			if (PrevElement->Rotation.Distribution != Element->Rotation.Distribution)
			{
				OldCurve = SelectedElement->Rotation.Distribution;
				SelectedElement->DuplicateDistribution_Float(Element->Rotation, LensFlare, SelectedElement->Rotation);
				NewCurve = SelectedElement->Rotation.Distribution;
			}
			else
			if (PrevElement->Color.Distribution != Element->Color.Distribution)
			{
				OldCurve = SelectedElement->Color.Distribution;
				SelectedElement->DuplicateDistribution_Vector(Element->Color, LensFlare, SelectedElement->Color);
				NewCurve = SelectedElement->Color.Distribution;
			}
			else
			if (PrevElement->Alpha.Distribution != Element->Alpha.Distribution)
			{
				OldCurve = SelectedElement->Alpha.Distribution;
				SelectedElement->DuplicateDistribution_Float(Element->Alpha, LensFlare, SelectedElement->Alpha);
				NewCurve = SelectedElement->Alpha.Distribution;
			}
			else
			if (PrevElement->Offset.Distribution != Element->Offset.Distribution)
			{
				OldCurve = SelectedElement->Offset.Distribution;
				SelectedElement->DuplicateDistribution_Vector(Element->Offset, LensFlare, SelectedElement->Offset);
				NewCurve = SelectedElement->Offset.Distribution;
			}
			else
			if (PrevElement->DistMap_Scale.Distribution != Element->DistMap_Scale.Distribution)
			{
				OldCurve = SelectedElement->DistMap_Scale.Distribution;
				SelectedElement->DuplicateDistribution_Vector(Element->DistMap_Scale, LensFlare, SelectedElement->DistMap_Scale);
				NewCurve = SelectedElement->DistMap_Scale.Distribution;
			}
			else
			if (PrevElement->DistMap_Color.Distribution != Element->DistMap_Color.Distribution)
			{
				OldCurve = SelectedElement->DistMap_Color.Distribution;
				SelectedElement->DuplicateDistribution_Vector(Element->DistMap_Color, LensFlare, SelectedElement->DistMap_Color);
				NewCurve = SelectedElement->DistMap_Color.Distribution;
			}
			else
			if (PrevElement->DistMap_Alpha.Distribution != Element->DistMap_Alpha.Distribution)
			{
				OldCurve = SelectedElement->DistMap_Alpha.Distribution;
				SelectedElement->DuplicateDistribution_Float(Element->DistMap_Alpha, LensFlare, SelectedElement->DistMap_Alpha);
				NewCurve = SelectedElement->DistMap_Alpha.Distribution;
			}

			if (NewCurve)
			{
				LensFlare->CurveEdSetup->ReplaceCurve(OldCurve, NewCurve);
				CurveEd->CurveChanged();
			}
		}
	}

	if (LensFlare)
	{
		LensFlare->PostEditChange();
		if (SelectedElement)
		{
			TArray<FLensFlareElementCurvePair> Curves;
			SelectedElement->GetCurveObjects(Curves);
			if (SelectedElementIndex == -1)
			{
				LensFlare->GetCurveObjects(Curves);
			}
			for (INT i=0; i<Curves.Num(); i++)
			{
				UDistributionFloat* DistF = Cast<UDistributionFloat>(Curves(i).CurveObject);
				UDistributionVector* DistV = Cast<UDistributionVector>(Curves(i).CurveObject);
				if (DistF)
					DistF->bIsDirty = TRUE;
				if (DistV)
					DistV->bIsDirty = TRUE;

				CurveEd->EdSetup->ChangeCurveColor(Curves(i).CurveObject, 
					FLinearColor(0.2f,0.2f,0.2f) * SelectedElementIndex);
					//SelectedElement->ModuleEditorColor);
				CurveEd->CurveChanged();
			}
		}
	}

	LensFlare->ThumbnailImageOutOfDate = TRUE;

	check(TransactionInProgress());
	EndTransaction(TEXT("LensFlareEditorPropertyChange"));

	PreviousElementIndex = INDEX_LENSFLARE;

	LensFlareEditorTouch();
	ResetLensFlareInLevel();

	ElementEdVC->Viewport->Invalidate();
}

void WxLensFlareEditor::NotifyExec( void* Src, const TCHAR* Cmd )
{
	GUnrealEd->NotifyExec(Src, Cmd);
}

///////////////////////////////////////////////////////////////////////////////////////
// Utils
void WxLensFlareEditor::CreateNewElement(INT ElementIndex)
{
	if (LensFlare == NULL)
	{
		return;
	}

	BeginTransaction(TEXT("CreateNewElement"));

	LensFlare->PreEditChange(NULL);
	LensFlareComp->PreEditChange(NULL);

	// Construct it and insert it into the array

	LensFlareComp->PostEditChange();
	LensFlare->PostEditChange();

	LensFlare->MarkPackageDirty();

	EndTransaction(TEXT("CreateNewElement"));

	ResetLensFlareInLevel();

	// Refresh viewport
	ElementEdVC->Viewport->Invalidate();
}

void WxLensFlareEditor::SetSelectedElement(INT NewSelectedElement)
{
	if ((LensFlare == NULL) || 
		(NewSelectedElement == SelectedElementIndex) ||
		(NewSelectedElement < -1)
		)
	{
		return;
	}

	if (NewSelectedElement == -1)
	{
		// Set the 'source' element as selected.
		SelectedElementIndex = NewSelectedElement;
		SelectedElement = &(LensFlare->SourceElement);
	}
	else
	if (NewSelectedElement < LensFlare->Reflections.Num())
	{
		SelectedElementIndex = NewSelectedElement;
		SelectedElement = &(LensFlare->Reflections(SelectedElementIndex));
	}
	else
	{
		// Outside of the available element range... get out.
		return;
	}

	PropertyWindow->SetObject(NULL, EPropertyWindowFlags::ShouldShowCategories);
	LensFlareElementWrapper->Element.DuplicateFromSource(*SelectedElement, LensFlareElementWrapper);
//	LensFlareElementWrapper->Element = SelectedElement;
	LensFlareElementWrapper->ElementIndex = SelectedElementIndex;
	LensFlareElementWrapper->SourceLensFlare = LensFlare;
	PropertyWindow->SetObject(LensFlareElementWrapper, EPropertyWindowFlags::ShouldShowCategories);

	SetSelectedInCurveEditor();
	ElementEdVC->Viewport->Invalidate();
}

void WxLensFlareEditor::DeleteSelectedElement()
{
	if ((LensFlare == NULL) || (SelectedElement == NULL))
	{
		return;
	}

	if (SelectedElementIndex == -1)
	{
		// Not allowed to delete the source of the lens flare.
		return;
	}

	BeginTransaction(TEXT("DeleteSelectedElement"));

	LensFlare->PreEditChange(NULL);
	LensFlareComp->PreEditChange(NULL);

	LensFlare->RemoveElementCurvesFromEditor(SelectedElementIndex, CurveEd->EdSetup);
	LensFlare->Reflections.Remove(SelectedElementIndex);

	LensFlareComp->PostEditChange();
	LensFlare->PostEditChange();

	SetSelectedElement(-1);

	LensFlare->MarkPackageDirty();

	EndTransaction(TEXT("DeleteSelectedElement"));

	ResetLensFlareInLevel();

	ElementEdVC->Viewport->Invalidate();
}

void WxLensFlareEditor::EnableSelectedElement()
{
	if ((LensFlare == NULL) || (SelectedElement == NULL))
	{
		return;
	}

	BeginTransaction(TEXT("EnableSelectedElement"));

	LensFlare->PreEditChange(NULL);
	LensFlareComp->PreEditChange(NULL);

	SelectedElement->bIsEnabled = TRUE;

	LensFlareComp->PostEditChange();
	LensFlare->PostEditChange();
	LensFlare->MarkPackageDirty();

	EndTransaction(TEXT("EnableSelectedElement"));

	ResetLensFlareInLevel();

	ElementEdVC->Viewport->Invalidate();
}

void WxLensFlareEditor::DisableSelectedElement()
{
	if ((LensFlare == NULL) || (SelectedElement == NULL))
	{
		return;
	}

	BeginTransaction(TEXT("DisableSelectedElement"));

	LensFlare->PreEditChange(NULL);
	LensFlareComp->PreEditChange(NULL);

	SelectedElement->bIsEnabled = FALSE;

	LensFlareComp->PostEditChange();
	LensFlare->PostEditChange();


	LensFlare->MarkPackageDirty();

	EndTransaction(TEXT("DisableSelectedElement"));

	ResetLensFlareInLevel();

	ElementEdVC->Viewport->Invalidate();
}

void WxLensFlareEditor::ResetSelectedElement()
{
}

void WxLensFlareEditor::MoveSelectedElement(INT MoveAmount)
{
	if ((LensFlare == NULL) || (SelectedElement == NULL))
	{
		return;
	}

	if (SelectedElementIndex == -1)
	{
		// Not allowed to move the source of the lens flare.
		return;
	}
	if (SelectedElementIndex >= LensFlare->Reflections.Num())
	{
		// This should not happen!
		SelectedElementIndex = -1;
		return;
	}

	BeginTransaction(TEXT("MoveSelectedElement"));

	INT CurrentElementIndex = SelectedElementIndex;
	INT NewElementIndex = Clamp<INT>(CurrentElementIndex + MoveAmount, 0, LensFlare->Reflections.Num() - 1);

	if (NewElementIndex != CurrentElementIndex)
	{
		LensFlare->PreEditChange(NULL);
		LensFlareComp->PreEditChange(NULL);

		FLensFlareElement MovingElement = LensFlare->Reflections(SelectedElementIndex);
//		LensFlareElement
//		LensFlare->Reflections.RemoveItem(*SelectedElement);
//		LensFlare->Reflections.InsertZeroed(NewElementIndex);
//		LensFlare->Reflections(NewElementIndex) = *SelectedElement;

		LensFlareComp->PostEditChange();
		LensFlare->PostEditChange();

		ElementEdVC->Viewport->Invalidate();
	}

	LensFlare->MarkPackageDirty();

	EndTransaction(TEXT("MoveSelectedElement"));
}

void WxLensFlareEditor::SetSelectedInCurveEditor()
{
	CurveEd->ClearAllSelectedCurves();
	TArray<FLensFlareElementCurvePair> Curves;
	if (SelectedElementIndex == -1)
	{
		LensFlare->GetCurveObjects(Curves);
	}
	if (SelectedElement)
	{
		SelectedElement->GetCurveObjects(Curves);
	}
/***	
	if (LensFlareElementWrapper && (LensFlareElementWrapper->ElementIndex == SelectedElementIndex))
	{
		LensFlareElementWrapper->Element.GetCurveObjects(Curves);
	}
***/
	for (INT CurveIndex = 0; CurveIndex < Curves.Num(); CurveIndex++)
	{
		UObject* Distribution = Curves(CurveIndex).CurveObject;
		if (Distribution)
		{
			CurveEd->SetCurveSelected(Distribution, TRUE);
		}
	}

	CurveEd->CurveEdVC->Viewport->Invalidate();
}

/**
 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
 *  @return A string representing a name to use for this docking parent.
 */
const TCHAR* WxLensFlareEditor::GetDockingParentName() const
{
	return TEXT("LensFlareEditor");
}

/**
 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
 */
const INT WxLensFlareEditor::GetDockingParentVersion() const
{
	return 0;
}


//
// ULensFlareEditorOptions
// 
