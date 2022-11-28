/*=============================================================================
	AnimTreeEditor.h: AnimTree editing
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	WxAnimTreeEditor
-----------------------------------------------------------------------------*/

class WxAnimTreeEditor : public WxLinkedObjEd
{
public:
	// SoundCue currently being edited
	class UAnimTree* AnimTree;

	// All nodes in current tree (excluding the AnimTree itself).
	TArray<UAnimNode*> TreeNodes;


	// Currently selected UAnimNodes
	TArray<class UAnimNode*> SelectedNodes;

	// Selected Connector
	UObject* ConnObj;
	INT ConnType;
	INT ConnIndex;

	// Static list of all SequenceObject classes.
	static TArray<UClass*>	AnimNodeClasses;
	static UBOOL			bAnimNodeClassesInitialized;


	WxAnimTreeEditor( wxWindow* InParent, wxWindowID InID, class UAnimTree* InAnimTree );
	~WxAnimTreeEditor();

	// LinkedObjEditor interface

	virtual void OpenNewObjectMenu();
	virtual void OpenObjectOptionsMenu();
	virtual void OpenConnectorOptionsMenu();
	virtual void DrawLinkedObjects(FRenderInterface* RI);
	virtual void UpdatePropertyWindow();

	virtual void EmptySelection();
	virtual void AddToSelection( UObject* Obj );
	virtual UBOOL IsInSelection( UObject* Obj );
	virtual UBOOL HaveObjectsSelected();

	virtual void SetSelectedConnector( UObject* ConnObj, INT ConnType, INT ConnIndex );
	virtual FIntPoint GetSelectedConnLocation(FRenderInterface* RI);

	// Make a connection between selected connector and an object or another connector.
	virtual void MakeConnectionToConnector( UObject* EndConnObj, INT EndConnType, INT EndConnIndex );
	virtual void MakeConnectionToObject( UObject* EndObj );

	virtual void MoveSelectedObjects( INT DeltaX, INT DeltaY );
	virtual void EdHandleKeyInput(FChildViewport* Viewport, FName Key, EInputEvent Event);

	// Menu handlers
	void OnNewAnimNode( wxCommandEvent& In );
	void OnBreakLink( wxCommandEvent& In );
	void OnAddInput( wxCommandEvent& In );
	void OnRemoveInput( wxCommandEvent& In );
	void OnDeleteNodes( wxCommandEvent& In );

	// Utils
	void DeleteSelectedNodes();

	// Static, initialiasation stuff
	static void InitAnimNodeClasses();

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxMBAnimTreeEdNewNode
-----------------------------------------------------------------------------*/

class WxMBAnimTreeEdNewNode : public wxMenu
{
public:
	WxMBAnimTreeEdNewNode(WxAnimTreeEditor* AnimTreeEd);
	~WxMBAnimTreeEdNewNode();
};

/*-----------------------------------------------------------------------------
	WxMBAnimTreeEdNodeOptions
-----------------------------------------------------------------------------*/

class WxMBAnimTreeEdNodeOptions : public wxMenu
{
public:
	WxMBAnimTreeEdNodeOptions(WxAnimTreeEditor* AnimTreeEd);
	~WxMBAnimTreeEdNodeOptions();
};

/*-----------------------------------------------------------------------------
	WxMBAnimTreeEdConnectorOptions
-----------------------------------------------------------------------------*/

class WxMBAnimTreeEdConnectorOptions : public wxMenu
{
public:
	WxMBAnimTreeEdConnectorOptions(WxAnimTreeEditor* AnimTreeEd);
	~WxMBAnimTreeEdConnectorOptions();
};



/*-----------------------------------------------------------------------------
The End.
-----------------------------------------------------------------------------*/
