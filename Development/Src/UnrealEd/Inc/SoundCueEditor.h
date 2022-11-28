/*=============================================================================
	SoundCueEditor.h: SoundCue editing
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	WxSoundCueEdToolBar
-----------------------------------------------------------------------------*/

class WxSoundCueEdToolBar : public wxToolBar
{
public:
	WxSoundCueEdToolBar( wxWindow* InParent, wxWindowID InID );
	~WxSoundCueEdToolBar();

	WxMaskedBitmap PlayCueB, PlayNodeB, StopB;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxSoundCueEditor
-----------------------------------------------------------------------------*/

class WxSoundCueEditor : public WxLinkedObjEd
{
public:
	WxSoundCueEditor( wxWindow* InParent, wxWindowID InID, class USoundCue* InSoundCue );
	~WxSoundCueEditor();

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


	// Static, initialiasation stuff
	static void InitSoundNodeClasses();

	// Utils
	void ConnectNodes(USoundNode* ParentNode, INT ChildIndex, USoundNode* ChildNode);
	void DeleteSelectedNodes();

	// Context Menu handlers
	void OnContextNewSoundNode( wxCommandEvent& In );
	void OnContextNewWave( wxCommandEvent& In );
	void OnContextAddInput( wxCommandEvent& In );
	void OnContextDeleteInput( wxCommandEvent& In );
	void OnContextDeleteNode( wxCommandEvent& In );
	void OnContextPlaySoundNode( wxCommandEvent& In );
	void OnContextPlaySoundCue( wxCommandEvent& In );
	void OnContextStopPlaying( wxCommandEvent& In );
	void OnContextBreakLink( wxCommandEvent& In );

	WxSoundCueEdToolBar* ToolBar;

	// SoundCue currently being edited
	class USoundCue* SoundCue;

	// Currently selected USoundNodes
	TArray<class USoundNode*> SelectedNodes;

	// Selected Connector
	UObject* ConnObj; // This is usually a SoundNode, but might be the SoundCue to indicate the 'root' connector.
	INT ConnType;
	INT ConnIndex;

	// Used for previewing sounds inside the editor
	UAudioComponent* PreviewAudioComponent;
	class USoundCue* PreviewSoundCue;

	// Static list of all SequenceObject classes.
	static TArray<UClass*>	SoundNodeClasses;
	static UBOOL			bSoundNodeClassesInitialized;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxMBSoundCueEdNewNode
-----------------------------------------------------------------------------*/

class WxMBSoundCueEdNewNode : public wxMenu
{
public:
	WxMBSoundCueEdNewNode(WxSoundCueEditor* CueEditor);
	~WxMBSoundCueEdNewNode();
};

/*-----------------------------------------------------------------------------
	WxMBSoundCueEdNodeOptions
-----------------------------------------------------------------------------*/

class WxMBSoundCueEdNodeOptions : public wxMenu
{
public:
	WxMBSoundCueEdNodeOptions(WxSoundCueEditor* CueEditor);
	~WxMBSoundCueEdNodeOptions();
};

/*-----------------------------------------------------------------------------
	WxMBSoundCueEdConnectorOptions
-----------------------------------------------------------------------------*/

class WxMBSoundCueEdConnectorOptions : public wxMenu
{
public:
	WxMBSoundCueEdConnectorOptions(WxSoundCueEditor* CueEditor);
	~WxMBSoundCueEdConnectorOptions();
};



/*-----------------------------------------------------------------------------
The End.
-----------------------------------------------------------------------------*/
