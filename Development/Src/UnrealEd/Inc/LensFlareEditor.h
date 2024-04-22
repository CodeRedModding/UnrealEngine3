/*=============================================================================
	LensFlareEditor.h: LensFlare editor
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LENSFLAREEDITOR_H__
#define __LENSFLAREEDITOR_H__

#include "UnrealEd.h"
#include "CurveEd.h"

/**
 *	HitProxy for the lens flare element in the ElementEd window.
 */
struct HLensFlareElementProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HLensFlareElementProxy,HHitProxy);

	/** The LensFlare this hit proxy represents */
	class ULensFlare* LensFlare;
	/** 
	 *	The element in the LensFlare for this hit proxy.
	 *	-1 indicates the SourceElement.
	 */
	INT ElementIndex;

	HLensFlareElementProxy(class ULensFlare* InLensFlare, INT InElementIndex) :
		HHitProxy(HPP_UI),
		LensFlare(InLensFlare),
		ElementIndex(InElementIndex)
	{}
};

/**
 *	HitProxy for the enable/disable button of an element in the ElementEd window.
 */
struct HLensFlareElementEnableProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HLensFlareElementEnableProxy,HHitProxy);

	/** The LensFlare this hit proxy represents */
	class ULensFlare* LensFlare;
	/** 
	 *	The element in the LensFlare for this hit proxy.
	 *	-1 indicates the SourceElement.
	 */
	INT ElementIndex;

	HLensFlareElementEnableProxy(class ULensFlare* InLensFlare, INT InElementIndex) :
		HHitProxy(HPP_UI),
		LensFlare(InLensFlare),
		ElementIndex(InElementIndex)
	{}
};

/**
 *	HitProxy for the color button of an element in the ElementEd window.
 */
struct HLensFlareElementColorButtonProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HLensFlareElementColorButtonProxy,HHitProxy);

	/** The LensFlare this hit proxy represents */
	class ULensFlare* LensFlare;
	/** 
	 *	The element in the LensFlare for this hit proxy.
	 *	-1 indicates the SourceElement.
	 */
	INT ElementIndex;

	HLensFlareElementColorButtonProxy(class ULensFlare* InLensFlare, INT InElementIndex) :
		HHitProxy(HPP_UI),
		LensFlare(InLensFlare),
		ElementIndex(InElementIndex)
	{}
};

/*-----------------------------------------------------------------------------
	FLensFlarePreviewViewportClient
	The EditorLevelViewportClient-derived class for the preview window of the editor.
-----------------------------------------------------------------------------*/
class FLensFlarePreviewViewportClient : public FEditorLevelViewportClient
{
public:
	/** Pointer to the 'parent' editor */
	class WxLensFlareEditor*	LensFlareEditor;

	/** The scene used to render the preview */
	FPreviewScene				PreviewScene;
	/** DrawHelper used for rendering the grid, etc. */
	FEditorCommonDrawHelper		DrawHelper;

	FLOAT						TimeScale;
	FLOAT						TotalTime;

	FRotator					PreviewAngle;
	FLOAT						PreviewDistance;

	UBOOL						bDrawOriginAxes;
	UBOOL						bDrawSystemDistance;
	UBOOL						bWireframe;
	UBOOL						bBounds;

	/** The background color to render in the preview viewport */
	FColor						BackgroundColor;

	/** Internal variable indicating a screen shot should be captured */
	UBOOL						bCaptureScreenShot;

	/** Show post-process flags	*/
	INT							ShowPPFlags;

	// Constructor
	FLensFlarePreviewViewportClient(class WxLensFlareEditor* InLensFlareEditor);

	// FEditorLevelViewportClient interface
	virtual FSceneInterface* GetScene() { return PreviewScene.GetScene(); }
	virtual FLinearColor GetBackgroundColor();
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas);
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI);
	virtual void DrawTools(const FSceneView* View,FPrimitiveDrawInterface* PDI){}

	/**
	 * Configures the specified FSceneView object with the view and projection matrices for this viewport.
	 * @param	View		The view to be configured.  Must be valid.
	 * @return	A pointer to the view within the view family which represents the viewport's primary view.
	 */
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily);

	/**
	 *	Input handlers
	 */
	virtual UBOOL InputKey(FViewport* Viewport,INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f,UBOOL bGamepad=FALSE);
	virtual void MouseMove(FViewport* Viewport, INT X, INT Y);
	virtual UBOOL InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad=FALSE);

	virtual void Serialize(FArchive& Ar) 
	{ 
		Ar << Input;
		Ar << PreviewScene; 
	}

	// FLensFlarePreviewViewportClient interface
	void SetPreviewCamera(const FRotator& PreviewAngle, FLOAT PreviewDistance);
	void UpdateLighting();

	void SetupPostProcessChain();
};

/*-----------------------------------------------------------------------------
	WxLensFlareEditorPreview
	The wxWindow for holding the preview viewport client, FLensFlarePreviewViewportClient.
-----------------------------------------------------------------------------*/
class WxLensFlareEditorPreview : public wxWindow
{
public:
	/** The viewport client */
	FLensFlarePreviewViewportClient* LensFlareEditorPreviewVC;

	// Constructor/destructor
	WxLensFlareEditorPreview( wxWindow* InParent, wxWindowID InID, class WxLensFlareEditor* InLensFlareEditor );
	~WxLensFlareEditorPreview();

	/**
	 *	OnSize handler
	 *
	 *	@param	In		The size event passed in when the size changes
	 */
	void OnSize( wxSizeEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	FLensFlareElementEdViewportClient
	The EditorLevelViewportClient-derived class for the element editing window of the editor.
-----------------------------------------------------------------------------*/
class FLensFlareElementEdViewportClient : public FEditorLevelViewportClient
{
public:
	/** The 'parent' editor that holds this viewport */
	class WxLensFlareEditor*	LensFlareEditor;

	/** Top-left corner of dragged module relative to mouse cursor. */
	FIntPoint					MouseHoldOffset; 
	/** Location of cursor when mouse was pressed. */
	FIntPoint					MousePressPosition;
	/** TRUE if the mouse is dragging (the left button was clicked and held down) */
	UBOOL						bMouseDragging;
	/** TRUE if the mouse button is down */
	UBOOL						bMouseDown;
	/** TRUE if the canvas is being panned */
	UBOOL						bPanning;

	/** The origin of the canvas w.r.t. the viewport window */
	FIntPoint					Origin2D;
	/** The previous mouse position */
	INT							OldMouseX, OldMouseY;

	/** Icon 'indices' */
	enum Icons
	{
		LFEDITOR_Icon_Module_Enabled = 0,
		LFEDITOR_Icon_Module_Disabled,
		LFEDITOR_Icon_COUNT
	};

protected:
	/** The icons used to draw buttons, etc. */
	UTexture2D*		IconTex[LFEDITOR_Icon_COUNT];
	/** The background image to draw when an element is disabled */
	UTexture2D*		TexElementDisabledBackground;

public:
	// Constructor/destructor
	FLensFlareElementEdViewportClient( class WxLensFlareEditor* InLensFlareEditor );
	~FLensFlareElementEdViewportClient();

	virtual void Serialize(FArchive& Ar) { Ar << Input; }

	// FEditorLevelViewportClient interface
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas);

	// Input handlers
	virtual UBOOL InputKey(FViewport* Viewport,INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f,UBOOL bGamepad=FALSE);
	virtual void MouseMove(FViewport* Viewport, INT X, INT Y);
	virtual UBOOL InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad=FALSE);

	// FLensFlareEmitterEdViewportClient interface
    void DrawLensFlareElement(INT Index, INT XPos, ULensFlare* LensFlare, FViewport* Viewport, FCanvas* Canvas);
    void DrawEnableButton(INT Index, INT XPos, ULensFlare* LensFlare, UBOOL bHitTesting, FViewport* Viewport, FCanvas* Canvas);
	void DrawColorButton(INT Index, INT XPos, ULensFlare* LensFlare, UBOOL bHitTesting, FViewport* Viewport, FCanvas* Canvas);

	virtual void SetCanvas(INT X, INT Y);
	virtual void PanCanvas(INT DeltaX, INT DeltaY);

	FMaterialRenderProxy*	GetIcon(Icons eIcon);
	FMaterialRenderProxy*	GetModuleDisabledBackground();
	FTexture*				GetIconTexture(Icons eIcon);
	FTexture*				GetTextureDisabledBackground();

protected:
	void CreateIconMaterials();
};

/*-----------------------------------------------------------------------------
	WxLensFlareElementEd
	The wxWindow holder for the FLensFlareElementEdViewportClient.
-----------------------------------------------------------------------------*/
class WxLensFlareElementEd : public wxWindow
{
public:
	/** The viewport client held by this window */
	FLensFlareElementEdViewportClient*	ElementEdVC;

	/** Scrollbars and supporting values */
	wxScrollBar*				ScrollBar_Horz;
	wxScrollBar*				ScrollBar_Vert;
	INT							ThumbPos_Horz;
	INT							ThumbPos_Vert;

	// Constructor/destructor
	WxLensFlareElementEd( wxWindow* InParent, wxWindowID InID, class WxLensFlareEditor* InLensFlareEditor );
	~WxLensFlareElementEd();

	/** Handler for resizing the window */
	void OnSize( wxSizeEvent& In );
	/** Helper function for dealing with the scroll bars */
	void UpdateScrollBar(INT Horz, INT Vert);
	/** Handler for scrolling the window */
	void OnScroll(wxScrollEvent& In);
	/** Handler for scrolling the window using the mouse wheel */
	void OnMouseWheel(wxMouseEvent& In);

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxLensFlareEditorMenuBar
	The menu bar for the editor window.
-----------------------------------------------------------------------------*/
class WxLensFlareEditorMenuBar : public wxMenuBar
{
public:
	wxMenu	*EditMenu, *ViewMenu;

	WxLensFlareEditorMenuBar(WxLensFlareEditor* InLensFlareEditor);
	~WxLensFlareEditorMenuBar();
};

/*-----------------------------------------------------------------------------
	WxLensFlareToolBar
	The tool bar for the editor window
-----------------------------------------------------------------------------*/
class WxLensFlareEditorToolBar : public WxToolBar
{
public:
	WxLensFlareEditorToolBar( wxWindow* InParent, wxWindowID InID );
	~WxLensFlareEditorToolBar();

	WxLensFlareEditor*	LensFlareEditor;

	WxMaskedBitmap		ResetInLevelB;
	WxMaskedBitmap		SaveCamB, ResetSystemB, OrbitModeB;
	WxMaskedBitmap		WireframeB;
	WxMaskedBitmap		BoundsB;
	WxMaskedBitmap		PostProcessB;
	WxMaskedBitmap		ToggleGridB;
	WxMaskedBitmap		RealtimeB;
	WxMaskedBitmap		SyncGenericBrowserB;
	UBOOL				bRealtime;
	WxMaskedBitmap		BackgroundColorB;
	WxMaskedBitmap		UndoB;
	WxMaskedBitmap		RedoB;
    
	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	FLensFlareEditorNotifyHook
	Notify hook for the editor to catch when it is being closed.
-----------------------------------------------------------------------------*/
class FLensFlareEditorNotifyHook : public FNotifyHook
{
public:
	virtual void NotifyDestroy(void* Src);

	WxLensFlareEditor*	LensFlareEditor;
	void*				WindowOfInterest;
};

/*-----------------------------------------------------------------------------
	WxLensFlareEditor
	The actual editor itself.
-----------------------------------------------------------------------------*/
class WxLensFlareEditor : public wxFrame, public FNotifyHook, public FSerializableObject, FDockingParent, FTickableObject, FCurveEdNotifyInterface
{
public:
	FLensFlareEditorNotifyHook			PropWindowNotifyHook;

	WxLensFlareEditorMenuBar*			MenuBar;
	WxLensFlareEditorToolBar*			ToolBar;

	WxPropertyWindowHost*				PropertyWindow;
	FLensFlarePreviewViewportClient*	PreviewVC;
	FLensFlareElementEdViewportClient*	ElementEdVC;
	WxLensFlareElementEd*				LensFlareElementEdWindow;
	class WxCurveEditor*				CurveEd;

	FLOAT								SimSpeed;

	class ULensFlare*					LensFlare;
	class ULensFlareComponent*			LensFlareComp;

	// Resources for previewing system
	class ULensFlarePreviewComponent*	LensFlarePrevComp;

	/** Used to display a single element in the property window. */
	class ULensFlareEditorPropertyWrapper* LensFlareElementWrapper;

	INT									SelectedElementIndex;
	struct FLensFlareElement*			SelectedElement;

	// Whether to reset the simulation once it has completed a run and all particles have died.
	UBOOL					bResetOnFinish;
	UBOOL					bPendingReset;
	DOUBLE					ResetTime;
	UBOOL					bOrbitMode;
	UBOOL					bWireframe;
	UBOOL					bBounds;
	
	UObject*				CurveToReplace;
	TArray<UObject*>		ChangingCurves;

	ULensFlareEditorOptions*	EditorOptions;

	/** The post-process effect to apply to the preview window */
    FString					DefaultPostProcessName;
    UPostProcessChain*		DefaultPostProcess;

	WxLensFlareEditor( wxWindow* InParent, wxWindowID InID, class ULensFlare* InLensFlare );
	~WxLensFlareEditor();

	// wxFrame interface
	virtual wxToolBar* OnCreateToolBar(long style, wxWindowID id, const wxString& name);

	// FSerializableObject interface
	void Serialize(FArchive& Ar);

	// FTickableObject interface
	/**
	 * Pure virtual that must be overloaded by the inheriting class. It will
	 * be called from within UnLevTick.cpp after ticking all actors.
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	virtual void Tick( FLOAT DeltaTime );

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It is
	 * used to determine whether an object is ready to be ticked. This is 
	 * required for example for all UObject derived classes as they might be
	 * loaded async and therefore won't be ready immediately.
	 *
	 * @return	TRUE if class is ready to be ticked, FALSE otherwise.
	 */
	virtual UBOOL IsTickable() const
	{
		return TRUE;
	}

	/**
	 * Used to determine if an object should be ticked when the game is paused.
	 * Defaults to false, as that mimics old behavior.
	 *
	 * @return TRUE if it should be ticked when paused, FALSE otherwise
	 */
	virtual UBOOL IsTickableWhenPaused() const
	{
		return TRUE;
	}

	/**
	 * Used to determine whether the object should be ticked in the editor.  Defaults to FALSE since
	 * that is the previous behavior.
	 *
	 * @return	TRUE if this tickable object can be ticked in the editor
	 */
	virtual UBOOL IsTickableInEditor() const
	{
		return TRUE;
	}

	// FCurveEdNotifyInterface
	/**
	 *	PreEditCurve
	 *	Called by the curve editor when N curves are about to change
	 *
	 *	@param	CurvesAboutToChange		An array of the curves about to change
	 */
	virtual void PreEditCurve(TArray<UObject*> CurvesAboutToChange);
	/**
	 *	PostEditCurve
	 *	Called by the curve editor when the edit has completed.
	 */
	virtual void PostEditCurve();
	/**
	 *	MovedKey
	 *	Called by the curve editor when a key has been moved.
	 */
	virtual void MovedKey();
	/**
	 *	DesireUndo
	 *	Called by the curve editor when an Undo is requested.
	 */
	virtual void DesireUndo();
	/**
	 *	DesireRedo
	 *	Called by the curve editor when an Redo is requested.
	 */
	virtual void DesireRedo();

	// Menu callbacks
	void OnSize( wxSizeEvent& In );

	void OnSelectLensFlare( wxCommandEvent& In );
	void OnAddCurve( wxCommandEvent& In );
	void OnElementAdd( wxCommandEvent& In );
	void OnElementAddBefore( wxCommandEvent& In );
	void OnElementAddAfter( wxCommandEvent& In );
	void OnDuplicateElement( wxCommandEvent& In );
	void OnDeleteElement( wxCommandEvent& In );
	void OnEnableElement( wxCommandEvent& In );
	void OnDisableElement( wxCommandEvent& In );
	void OnResetElement( wxCommandEvent& In );

	void OnResetInLevel( wxCommandEvent& In );
	void OnSaveCam( wxCommandEvent& In );
	void OnSyncGenericBrowser( wxCommandEvent& In );
	void OnSavePackage(wxCommandEvent& In);
	void OnOrbitMode(wxCommandEvent& In);
	void OnWireframe(wxCommandEvent& In);
	void OnBounds(wxCommandEvent& In);
	void OnPostProcess(wxCommandEvent& In);
	void OnToggleGrid(wxCommandEvent& In);
	void OnViewAxes( wxCommandEvent& In );
	void OnShowPPBloom( wxCommandEvent& In );
	void OnShowPPDOF( wxCommandEvent& In );
	void OnShowPPMotionBlur( wxCommandEvent& In );
	void OnShowPPVolumeMaterial( wxCommandEvent& In );

	void OnRealtime(wxCommandEvent& In);

	void OnBackgroundColor(wxCommandEvent& In);

	void OnUndo(wxCommandEvent& In);
	void OnRedo(wxCommandEvent& In);

	// Utils
	void CreateNewElement(INT ElementIndex);
	void SetSelectedElement(INT NewSelectedElement);
	void DeleteSelectedElement();
	void EnableSelectedElement();
	void DisableSelectedElement();
	void ResetSelectedElement();
	void MoveSelectedElement(INT MoveAmount);

	void SetSelectedInCurveEditor();

	// FNotify interface
	void NotifyDestroy( void* Src );
	void NotifyPreChange( void* Src, FEditPropertyChain* PropertyChain );
	void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );
	void NotifyPostChange( void* Src, FEditPropertyChain* PropertyChain );
	void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );
	void NotifyExec( void* Src, const TCHAR* Cmd );

	// Undo/Redo support
	bool BeginTransaction(const TCHAR* pcTransaction);
	bool EndTransaction(const TCHAR* pcTransaction);
	bool TransactionInProgress();

	void ModifyLensFlare();

	void LensFlareEditorUndo();
	void LensFlareEditorRedo();
	void LensFlareEditorTouch();

	// PostProces
	/**
	 *	Update the post process chain according to the show options
	 */
	void UpdatePostProcessChain();

	/** Reset the lens flare in the level */
	void ResetLensFlareInLevel();

protected:
	/**
	 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
	 *  @return A string representing a name to use for this docking parent.
	 */
	virtual const TCHAR* GetDockingParentName() const;

	/**
	 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
	 */
	virtual const INT GetDockingParentVersion() const;

	bool	bTransactionInProgress;
	FString	kTransactionName;

public:
	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxMBLensFlareEditor
-----------------------------------------------------------------------------*/

class WxMBLensFlareEditor : public wxMenu
{
public:
	WxMBLensFlareEditor(WxLensFlareEditor* InLensFlareEditor);
	~WxMBLensFlareEditor();

protected:
	wxMenu* LensFlareMenu;
	wxMenu* CurveMenu;
};

#endif // __LENSFLAREEDITOR_H__
