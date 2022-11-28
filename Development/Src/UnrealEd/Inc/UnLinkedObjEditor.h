/*=============================================================================
	UnLinkedObjEditor.h: Base class for boxes-and-lines editing
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	FLinkedObjViewportClient
-----------------------------------------------------------------------------*/

class FLinkedObjViewportClient : public FEditorLevelViewportClient
{
public:
	FLinkedObjViewportClient( class WxLinkedObjEd* InObjEditor );
	~FLinkedObjViewportClient();

	virtual void Draw(FChildViewport* Viewport,FRenderInterface* RI);
	virtual void InputKey(FChildViewport* Viewport,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f);
	virtual void MouseMove(FChildViewport* Viewport, INT X, INT Y);
	virtual void Tick(FLOAT DeltaSeconds);

	virtual ULevel* GetLevel() { return NULL; }

	virtual EMouseCursor GetCursor(FChildViewport* Viewport,INT X,INT Y);

	FIntPoint DoScrollBorder(FLOAT DeltaSeconds);


	class WxLinkedObjEd* ObjEditor;

	FIntPoint Origin2D;
	FLOAT Zoom2D;
	FLOAT MinZoom2D, MaxZoom2D;
	INT NewX, NewY; // Location for creating new object

	INT OldMouseX, OldMouseY;
	INT BoxStartX, BoxStartY;
	INT BoxEndX, BoxEndY;
	INT DistanceDragged;

	FVector2D ScrollAccum;

	UBOOL bTransactionBegun;
	UBOOL bMouseDown;

	UBOOL bMakingLine;
	UBOOL bBoxSelecting;
	UBOOL bSpecialDrag;
	UBOOL bAllowScroll;

	// For mouse over stuff.
	UObject* MouseOverObject;
	INT MouseOverConnType;
	INT MouseOverConnIndex;
	DOUBLE MouseOverTime;

	INT SpecialIndex;
};

/*-----------------------------------------------------------------------------
	WxLinkedObjVCHolder
-----------------------------------------------------------------------------*/

class WxLinkedObjVCHolder : public wxWindow
{
public:
	WxLinkedObjVCHolder( wxWindow* InParent, wxWindowID InID, class WxLinkedObjEd* InObjEditor );
	~WxLinkedObjVCHolder();

	void OnSize( wxSizeEvent& In );

	FLinkedObjViewportClient* LinkedObjVC;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxLinkedObjEd
-----------------------------------------------------------------------------*/

class WxLinkedObjEd : public wxFrame, public FNotifyHook
{
public:
	WxLinkedObjEd( wxWindow* InParent, wxWindowID InID, const TCHAR* InWinName, UBOOL bTreeControl=false );
	~WxLinkedObjEd();

	void OnSize( wxSizeEvent& In );

	// LinkedObjEditor interface

	virtual void OpenNewObjectMenu() {}
	virtual void OpenObjectOptionsMenu() {}
	virtual void OpenConnectorOptionsMenu() {}
	virtual void DoubleClickedObject(UObject* Obj) {}
	virtual UBOOL ClickOnBackground() {return true;}
	virtual void DrawLinkedObjects(FRenderInterface* RI);
	virtual void UpdatePropertyWindow() {}

	virtual void EmptySelection() {}
	virtual void AddToSelection( UObject* Obj ) {}
	virtual void RemoveFromSelection( UObject* Obj ) {}	
	virtual UBOOL IsInSelection( UObject* Obj ) { return false; }
	virtual UBOOL HaveObjectsSelected() { return false; }

	virtual void SetSelectedConnector( UObject* ConnObj, INT ConnType, INT ConnIndex ) {}
	virtual FIntPoint GetSelectedConnLocation(FRenderInterface* RI) { return FIntPoint(0,0); }
	virtual INT GetSelectedConnectorType() { return 0; }
	virtual UBOOL ShouldHighlightConnector(UObject* Obj, INT ConnType, INT ConnIndex) { return true; }
	virtual FColor GetMakingLinkColor() { return FColor(0,0,0); }

	// Make a connection between selected connector and an object or another connector.
	virtual void MakeConnectionToConnector( UObject* ConnObj, INT ConnType, INT ConnIndex ) {}
	virtual void MakeConnectionToObject( UObject* Obj ) {}

	virtual void MoveSelectedObjects( INT DeltaX, INT DeltaY ) {}
	virtual void PositionSelectedObjects() {}
	virtual void EdHandleKeyInput(FChildViewport* Viewport, FName Key, EInputEvent Event) {}
	virtual void OnMouseOver(UObject* Obj) {}
	virtual void ViewPosChanged() {}
	virtual void SpecialDrag( INT DeltaX, INT DeltaY, INT SpecialIndex ) {}

	virtual void BeginTransactionOnSelected() {}
	virtual void EndTransactionOnSelected() {}

	void RefreshViewport();

	// FNotify interface

	virtual void NotifyDestroy( void* Src );
	virtual void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );
	virtual void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );
	virtual void NotifyExec( void* Src, const TCHAR* Cmd );

	wxSplitterWindow* SplitterWnd;
	wxSplitterWindow* TreeSplitterWnd;
	WxPropertyWindow* PropertyWindow;
	FLinkedObjViewportClient* LinkedObjVC;
	wxTreeCtrl* TreeControl;
	wxImageList* TreeImages;

	UTexture2D*	BackgroundTexture;
	FString WinNameString;
	UBOOL bDrawCurves;

	DECLARE_EVENT_TABLE()
};