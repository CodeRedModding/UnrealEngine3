/*=============================================================================
	CurveEd.h: FInterpCurve editor
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"

struct HCurveEdLabelProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HCurveEdLabelProxy,HHitProxy);

	INT CurveIndex;

	HCurveEdLabelProxy(INT InCurveIndex) :
		CurveIndex(InCurveIndex)
	{}
};

struct HCurveEdHideCurveProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HCurveEdHideCurveProxy,HHitProxy);
	
	INT CurveIndex;

	HCurveEdHideCurveProxy(INT InCurveIndex) :
		CurveIndex(InCurveIndex)
	{}
};

struct HCurveEdKeyProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HCurveEdKeyProxy,HHitProxy);

	INT CurveIndex;
	INT SubIndex;
	INT KeyIndex;

	HCurveEdKeyProxy(INT InCurveIndex, INT InSubIndex, INT InKeyIndex) :
		CurveIndex(InCurveIndex),
		SubIndex(InSubIndex),
		KeyIndex(InKeyIndex)
	{}
};

struct HCurveEdKeyHandleProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HCurveEdKeyHandleProxy,HHitProxy);

	INT CurveIndex;
	INT SubIndex;
	INT KeyIndex;
	UBOOL bArriving;

	HCurveEdKeyHandleProxy(INT InCurveIndex, INT InSubIndex, INT InKeyIndex, UBOOL bInArriving) :
		CurveIndex(InCurveIndex),
		SubIndex(InSubIndex),
		KeyIndex(InKeyIndex),
		bArriving(bInArriving)
	{}
};

struct HCurveEdLineProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HCurveEdLineProxy,HHitProxy);

	INT CurveIndex;
	INT SubIndex;

	HCurveEdLineProxy(INT InCurveIndex, INT InSubIndex) :
		CurveIndex(InCurveIndex),
		SubIndex(InSubIndex)
	{}
};

struct HCurveEdLabelBkgProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HCurveEdLabelBkgProxy,HHitProxy);
	HCurveEdLabelBkgProxy() {}
};

/*-----------------------------------------------------------------------------
	FCurveEdViewportClient
-----------------------------------------------------------------------------*/

class FCurveEdViewportClient : public FEditorLevelViewportClient
{
public:

	class WxCurveEditor* CurveEd;

	INT OldMouseX, OldMouseY;
	UBOOL bPanning;
	UBOOL bZooming;
	UBOOL bMouseDown;
	UBOOL bDraggingHandle;
	UBOOL bBegunMoving;
	UBOOL bBoxSelecting;
	UBOOL bKeyAdded;
	INT DistanceDragged;

	INT BoxStartX, BoxStartY;
	INT BoxEndX, BoxEndY;

	FCurveEdViewportClient(WxCurveEditor* InCurveEd);
	~FCurveEdViewportClient();

	virtual void Draw(FChildViewport* Viewport,FRenderInterface* RI);

	virtual ULevel* GetLevel() { return NULL; }

	virtual void InputKey(FChildViewport* Viewport,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f);
	virtual void MouseMove(FChildViewport* Viewport, INT X, INT Y);
	virtual void InputAxis(FChildViewport* Viewport, FName Key, FLOAT Delta, FLOAT DeltaTime);

	virtual void Serialize(FArchive& Ar) { Ar << Input; }
};

/*-----------------------------------------------------------------------------
	WxCurveEditor
-----------------------------------------------------------------------------*/

struct FCurveEdSelKey
{
	INT		CurveIndex;
	INT		SubIndex;
	INT		KeyIndex;
	FLOAT	UnsnappedIn;
	FLOAT	UnsnappedOut;

	FCurveEdSelKey(INT InCurveIndex, INT InSubIndex, INT InKeyIndex)
	{
		CurveIndex = InCurveIndex;
		SubIndex = InSubIndex;
		KeyIndex = InKeyIndex;
	}

	UBOOL operator==(const FCurveEdSelKey& Other) const
	{
		if( CurveIndex == Other.CurveIndex &&
			SubIndex == Other.SubIndex &&
			KeyIndex == Other.KeyIndex )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
};

enum ECurveEdMode
{
	CEM_Pan,
	CEM_Zoom
};

class FCurveEdNotifyInterface
{
public:
	virtual void PreEditCurve(TArray<UObject*> CurvesAboutToChange) {}
	virtual void PostEditCurve() {}

	virtual void MovedKey() {}

	virtual void DesireUndo() {}
	virtual void DesireRedo() {}
};

class WxCurveEditor : public wxWindow
{
public:
	FCurveEdViewportClient*		CurveEdVC;

	UInterpCurveEdSetup*		EdSetup;

	FLOAT StartIn, EndIn, StartOut, EndOut;
	FLOAT CurveViewX, CurveViewY;
	FLOAT PixelsPerIn, PixelsPerOut;

	INT MouseOverCurveIndex;
	INT MouseOverSubIndex;
	INT MouseOverKeyIndex;

	TArray<FCurveEdSelKey>		SelectedKeys;

	INT							HandleCurveIndex;
	INT							HandleSubIndex;
	INT							HandleKeyIndex;
	UBOOL						bHandleArriving;

	class WxCurveEdToolBar*		ToolBar;
	INT							RightClickCurveIndex;

	ECurveEdMode				EdMode;

	UBOOL bShowPositionMarker;
	FLOAT MarkerPosition;
	FColor MarkerColor;

	UBOOL bShowEndMarker;
	FLOAT EndMarkerPosition;

	UBOOL bShowRegionMarker;
	FLOAT RegionStart;
	FLOAT RegionEnd;
	FColor RegionFillColor;

	UBOOL bSnapEnabled;
	FLOAT InSnapAmount;

	FCurveEdNotifyInterface* NotifyObject;

public:
	WxCurveEditor( wxWindow* InParent, wxWindowID InID, class UInterpCurveEdSetup* InEdSetup );
	~WxCurveEditor();

	void CurveChanged();
	void UpdateDisplay();

	void SetPositionMarker(UBOOL bEnabled, FLOAT InPosition, const FColor& InMarkerColor);
	void SetEndMarker(UBOOL bEnabled, FLOAT InEndPosition);
	void SetRegionMarker(UBOOL bEnabled, FLOAT InRegionStart, FLOAT InRegionEnd, const FColor& InRegionFillColor);

	void SetInSnap(UBOOL bEnabled, FLOAT SnapAmount);

	void SetNotifyObject(FCurveEdNotifyInterface* NewNotifyObject);

private:

	void OnSize( wxSizeEvent& In );
	void OnFitHorz( wxCommandEvent& In );
	void OnFitVert( wxCommandEvent& In );
	void OnContextCurveRemove( wxCommandEvent& In );
	void OnChangeMode( wxCommandEvent& In );
	void OnSetKey( wxCommandEvent& In );
	void OnSetKeyColor( wxCommandEvent& In );
	void OnChangeInterpMode( wxCommandEvent& In );

	// --

	INT AddNewKeypoint( INT InCurveIndex, INT InSubIndex, const FIntPoint& ScreenPos );
	void SetCurveView(FLOAT StartIn, FLOAT EndIn, FLOAT StartOut, FLOAT EndOut);
	void MoveSelectedKeys( FLOAT DeltaIn, FLOAT DeltaOut );
	void MoveCurveHandle(const FVector2D& NewHandleVal);

	void AddKeyToSelection(INT InCurveIndex, INT InSubIndex, INT InKeyIndex);
	void RemoveKeyFromSelection(INT InCurveIndex, INT InSubIndex, INT InKeyIndex);
	void ClearKeySelection();
	UBOOL KeyIsInSelection(INT InCurveIndex, INT InSubIndex, INT InKeyIndex);

	FLOAT SnapIn(FLOAT InValue);

	void BeginMoveSelectedKeys();
	void EndMoveSelectedKeys();

	void DoubleClickedKey(INT InCurveIndex, INT InSubIndex, INT InKeyIndex);

	void ToggleCurveHidden(INT InCurveIndex);
	void UpdateInterpModeButtons();

	void DeleteSelectedKeys();

	FIntPoint CalcScreenPos(const FVector2D& Val);
	FVector2D CalcValuePoint(const FIntPoint& Pos);

	void DrawCurveEditor(FChildViewport* Viewport, FRenderInterface* RI);
	void DrawEntry(FChildViewport* Viewport, FRenderInterface* RI, const FCurveEdEntry& Entry, INT CurveIndex);
	void DrawGrid(FChildViewport* Viewport, FRenderInterface* RI);

	friend class FCurveEdViewportClient;
	friend class WxMBCurveKeyMenu;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxCurveEdToolBar
-----------------------------------------------------------------------------*/

class WxCurveEdToolBar : public wxToolBar
{
public:
	WxCurveEdToolBar( wxWindow* InParent, wxWindowID InID );
	~WxCurveEdToolBar();

	void SetCurveMode(EInterpCurveMode NewMode);
	void SetButtonsEnabled(UBOOL bEnabled);
	void SetEditMode(ECurveEdMode NewMode);

	WxMaskedBitmap FitHorzB, FitVertB, PanB, ZoomB, ModeAutoB, ModeUserB, ModeBreakB, ModeLinearB, ModeConstantB;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxMBCurveLabelMenu
-----------------------------------------------------------------------------*/

class WxMBCurveLabelMenu : public wxMenu
{
public:
	WxMBCurveLabelMenu(WxCurveEditor* CurveEd);
	~WxMBCurveLabelMenu();
};


/*-----------------------------------------------------------------------------
	WxMBCurveKeyMenu
-----------------------------------------------------------------------------*/

class WxMBCurveKeyMenu : public wxMenu
{
public:
	WxMBCurveKeyMenu(WxCurveEditor* CurveEd);
	~WxMBCurveKeyMenu();
};
