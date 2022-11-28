/*=============================================================================
	UIEditor.h
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#ifndef _XBOX

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"

class UUIContainerFactory : public UFactory
{
	DECLARE_CLASS(UUIContainerFactory,UFactory,0,WarfareGame);
	void StaticConstructor();
	UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn);
};

/*-----------------------------------------------------------------------------
	EUIEdToolBar
-----------------------------------------------------------------------------*/

class EUIEdToolBar : public wxToolBar
{
public:
	EUIEdToolBar( wxWindow* InParent, wxWindowID InID );
	~EUIEdToolBar();

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxUIEditor
-----------------------------------------------------------------------------*/

class WxUIEditor : public WxLinkedObjEd
{
public:
	WxUIEditor( wxWindow* InParent, wxWindowID InID, class UUIContainer *editContainer = NULL );
	~WxUIEditor();

	/** Current container we are editing */
	class UUIContainer *Container;

	/** Current width/height of our screen */
	FLOAT Width, Height;

	/** Should the grid be drawn? */
	UBOOL bShowGrid;

	/** Should movement/scaling be snapped to GridSize? */
	UBOOL bSnapToGridXY;
	UBOOL bSnapToGridWH;

	/** Draw element names? */
	UBOOL bShowNames;

	/** Current grid size */
	FLOAT GridSize;

	/** List of currently selected elements in the container */
	struct FSelectedElement
	{
		class UUIElement* Element;
		INT ElementX, ElementY, ElementW, ElementH;
	};
	TArray<FSelectedElement> SelectedElements;

	// LinkedObjEditor interface

	virtual void OpenNewObjectMenu();
	virtual void OpenObjectOptionsMenu();
	virtual void DrawLinkedObjects(FRenderInterface* RI);
	virtual void UpdatePropertyWindow();

	virtual void EmptySelection();
	virtual void AddToSelection( UObject* Obj );
	virtual UBOOL IsInSelection( UObject* Obj );
	virtual UBOOL HaveObjectsSelected();

	virtual void MoveSelectedObjects( INT DeltaX, INT DeltaY );
	virtual void EdHandleKeyInput(FChildViewport* Viewport, FName Key, EInputEvent Event);	
	virtual void OnMouseOver(UObject* Obj);
	virtual void SpecialDrag( INT DeltaX, INT DeltaY, INT SpecialIndex );

	virtual void SnapSelected();

	virtual void OnContextNewObject(wxCommandEvent& In);
	virtual void OnContextMoveObjectLevel(wxCommandEvent& In);
	virtual void OnContextGridSize(wxCommandEvent& In);
	virtual void OnContextToggleSnap(wxCommandEvent& In);
	virtual void OnContextRename(wxCommandEvent& In);
	virtual void OnContextToggleNames(wxCommandEvent& In);

	EUIEdToolBar* ToolBar;


	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	EUIEdNewObjectMenu
-----------------------------------------------------------------------------*/

class EUIEdNewObjectMenu : public wxMenu
{
public:
	EUIEdNewObjectMenu(WxUIEditor* UIEditor);
	~EUIEdNewObjectMenu();
};

/*-----------------------------------------------------------------------------
	EUIEdObjectOptionsMenu
-----------------------------------------------------------------------------*/

class EUIEdObjectOptionsMenu : public wxMenu
{
public:
	EUIEdObjectOptionsMenu(WxUIEditor* UIEditor);
	~EUIEdObjectOptionsMenu();
};


/*-----------------------------------------------------------------------------
The End.
-----------------------------------------------------------------------------*/
#endif
