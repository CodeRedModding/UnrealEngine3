/*=============================================================================
	Kismet.h: Gameplay sequence editing
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/


#include "UnrealEd.h"


/*-----------------------------------------------------------------------------
	WxKismetToolBar
-----------------------------------------------------------------------------*/

class WxKismetToolBar : public wxToolBar
{
public:
	WxKismetToolBar( wxWindow* InParent, wxWindowID InID );
	~WxKismetToolBar();

	WxMaskedBitmap ParentSequenceB, RenameSequenceB, HideB, ShowB, CurvesB, SearchB, ZoomToFitB;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxKismetStatusBar
-----------------------------------------------------------------------------*/

class WxKismetStatusBar : public wxStatusBar
{
public:
	WxKismetStatusBar( wxWindow* InParent, wxWindowID InID);
	~WxKismetStatusBar();

	void SetMouseOverObject( class USequenceObject* SeqObj );
};


/*-----------------------------------------------------------------------------
	WxKismetSearch
-----------------------------------------------------------------------------*/

enum EKismetSearchType
{
	KST_NameComment = 0,
	KST_ObjName = 1,
	KST_VarName = 2,
	KST_VarNameUses = 3
};

class WxKismetSearch : public wxFrame
{
public:
	WxKismetSearch( WxKismet* InKismet, wxWindowID InID );
	~WxKismetSearch();

	void OnClose( wxCloseEvent& In );

	wxListBox*		ResultList;
	wxTextCtrl*		NameEntry;
	wxComboBox*		SearchTypeCombo;
	wxButton*		SearchButton;

	WxKismet*		Kismet;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxKismet
-----------------------------------------------------------------------------*/

class WxKismetTreeLeafData : public wxTreeItemData
{
public:
	WxKismetTreeLeafData() { check(0);	}	// Wrong ctor
	WxKismetTreeLeafData( USequence* InSeq )
	{
		Data = InSeq;
	}
	~WxKismetTreeLeafData() {}

	USequence* Data;
};

//FIXME: change this to a configurable value maybe?
#define KISMET_GRIDSIZE			8

class WxKismet : public WxLinkedObjEd
{
public:
	WxKismet( wxWindow* InParent, wxWindowID InID, class USequence* InSequence );
	~WxKismet();


	// LinkedObjEditor interface

	virtual void OpenNewObjectMenu();
	virtual void OpenObjectOptionsMenu();
	virtual void OpenConnectorOptionsMenu();
	virtual void DoubleClickedObject(UObject* Obj);
	virtual UBOOL ClickOnBackground();
	virtual void DrawLinkedObjects(FRenderInterface* RI);
	virtual void UpdatePropertyWindow();

	virtual void EmptySelection();
	virtual void AddToSelection( UObject* Obj );
	virtual void RemoveFromSelection( UObject* Obj );
	virtual UBOOL IsInSelection( UObject* Obj );
	virtual UBOOL HaveObjectsSelected();

	virtual void SetSelectedConnector( UObject* ConnObj, INT ConnType, INT ConnIndex );
	virtual FIntPoint GetSelectedConnLocation(FRenderInterface* RI);
	virtual INT GetSelectedConnectorType();
	virtual UBOOL ShouldHighlightConnector(UObject* Obj, INT ConnType, INT ConnIndex);
	virtual FColor GetMakingLinkColor();

	// Make a connection between selected connector and an object or another connector.
	virtual void MakeConnectionToConnector( UObject* EndConnObj, INT EndConnType, INT EndConnIndex );
	virtual void MakeConnectionToObject( UObject* EndObj );

	virtual void MoveSelectedObjects( INT DeltaX, INT DeltaY );
	virtual void PositionSelectedObjects();
	virtual void EdHandleKeyInput(FChildViewport* Viewport, FName Key, EInputEvent Event);
	virtual void OnMouseOver(UObject* Obj);
	virtual void ViewPosChanged();
	virtual void SpecialDrag( INT DeltaX, INT DeltaY, INT SpecialIndex );

	virtual void BeginTransactionOnSelected();
	virtual void EndTransactionOnSelected();


	// Static, initialiasation stuff
	static void InitSeqObjClasses();
	static void OpenKismet(UUnrealEdEngine* Editor);
	static void CloseAllKismetWindows();
	static void FindActorReferences(AActor* FindActor);

	void ChangeActiveSequence(USequence *newSeq, UBOOL bNotifyTree=true);
	void CenterViewOnSeqObj(USequenceObject* ViewObj);
	void ZoomSelection();
	void ZoomAll();
	void ZoomToBox(const FIntRect& Box);

	// Sequence Tools

	void OpenMatinee(class USeqAct_Interp* Interp);
	void DeleteSelected();
	void MakeLogicConnection(class USequenceOp* OutOp, INT OutConnIndex, class USequenceOp* InOp, INT InConnIndex);
	void MakeVariableConnection(class USequenceOp* Op, INT OpConnIndex, class USequenceVariable* Var);
	void NewSequenceObject(UClass* NewSeqObjClass, INT NewPosX, INT NewPosY);
	void SingleTickSequence();

	void KismetUndo();
	void KismetRedo();
	void KismetCopy();
	void KismetPaste(UBOOL bAtMousePos=false);

	void RebuildTreeControl();
	void BuildSelectedActorLists();
	FIntRect CalcBoundingBoxOfSelected();
	FIntRect CalcBoundingBoxOfAll();
	void DoSearch(const TCHAR* InText, EKismetSearchType Type, UBOOL bJumpToFirstResult);

	static UBOOL FindOutputLinkTo(const class USequence *sequence, const class USequenceOp *targetOp, const INT inputIdx, TArray<class USequenceOp*> &outputOps, TArray<INT> &outputIndices);
	static void HideConnectors(USequence *Sequence,TArray<USequenceObject*> &objList);


	// Context menu functions.

	void OnContextNewScriptingObject( wxCommandEvent& In );
	void OnContextNewScriptingObjVarContext( wxCommandEvent& In );
	void OnContextNewScriptingEventContext( wxCommandEvent& In );
	void OnContextNewVarConnector( wxCommandEvent& In );
	void OnContextCreateLinkedVariable( wxCommandEvent& In );
	void OnContextClearVariable( wxCommandEvent& In );
	void OnContextDelVarConnector( wxCommandEvent& In );
	void OnContextMoveUpVarConnector( wxCommandEvent& In );
	void OnContextMoveDownVarConnector( wxCommandEvent& In );
	void OnContextDelSeqObject( wxCommandEvent& In );
	void OnContextBreakLink( wxCommandEvent& In );
	void OnContextSelectEventActors( wxCommandEvent& In );
	void OnContextFireEvent( wxCommandEvent& In );
	void OnContextOpenInterpEdit( wxCommandEvent& In );
	void OnContextHideConnectors( wxCommandEvent& In );
	void OnContextShowConnectors( wxCommandEvent& In );
	void OnContextFindNamedVarUses( wxCommandEvent& In );
	void OnContextFindNamedVarDefs( wxCommandEvent& In );
	void OnContextCreateSequence( wxCommandEvent& In );
	void OnContextExportSequence( wxCommandEvent& In );
	void OnContextImportSequence( wxCommandEvent& In );
	void OnContextPasteHere( wxCommandEvent& In );
	void OnContextLinkEvent( wxCommandEvent& In );
	void OnContextLinkObject( wxCommandEvent& In );
	void OnContextRenameSequence( wxCommandEvent &In );	

	void OnButtonParentSequence( wxCommandEvent &In );	
	void OnButtonRenameSequence( wxCommandEvent &In );	
	void OnButtonHideConnectors( wxCommandEvent &In );
	void OnButtonShowConnectors( wxCommandEvent &In );
	void OnButtonShowCurves( wxCommandEvent &In );
	void OnButtonZoomToFit( wxCommandEvent &In );

	void OnTreeItemSelected( wxTreeEvent &In );
	void OnTreeExpanding( wxTreeEvent &In );
	void OnTreeCollapsed( wxTreeEvent &In );

	void OnOpenSearch( wxCommandEvent &In );
	void OnDoSearch( wxCommandEvent &In );
	void OnSearchResultChanged( wxCommandEvent &In );

	WxKismetToolBar*		ToolBar;
	WxKismetStatusBar*	StatusBar;
	WxKismetSearch*		SearchWindow;

	// Sequence currently being edited
	class USequence* Sequence;

	// Previously edited sequence. Used for Ctrl-Tab.
	class USequence* OldSequence;

	// Currently selected SequenceObjects
	TArray<class USequenceObject*> SelectedSeqObjs;

	// Selected Connector
	class USequenceOp* ConnSeqOp;
	INT ConnType;
	INT ConnIndex;

	INT PasteCount;

	// Allows us to find the TreeItemId for a particular sequence.
	TMap<USequence*,wxTreeItemId> TreeMap;

	// When creating a new event, here are the options. Generated by WxMBKismetNewObject each time its brought up.
	TArray<AActor*>		NewObjActors;
	TArray<UClass*>		NewEventClasses;
	UBOOL				bAttachVarsToConnector;

	// Static list of all SequenceObject classes.
	static TArray<UClass*>	SeqObjClasses;
	static UBOOL			bSeqObjClassesInitialized;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxMBKismetNewObject
-----------------------------------------------------------------------------*/

class WxMBKismetNewObject : public wxMenu
{
public:
	WxMBKismetNewObject(WxKismet* SeqEditor);
	~WxMBKismetNewObject();

	wxMenu *ActionMenu, *VariableMenu, *ConditionMenu, *EventMenu, *ContextEventMenu;
};

/*-----------------------------------------------------------------------------
	WxMBKismetConnectorOptions
-----------------------------------------------------------------------------*/

class WxMBKismetConnectorOptions : public wxMenu
{
public:
	wxMenu *BreakLinkMenu;
	WxMBKismetConnectorOptions(WxKismet* SeqEditor);
	~WxMBKismetConnectorOptions();
};

/*-----------------------------------------------------------------------------
	WxMBKismetObjectOptions
-----------------------------------------------------------------------------*/

class WxMBKismetObjectOptions : public wxMenu
{
public:
	WxMBKismetObjectOptions(WxKismet* SeqEditor);
	~WxMBKismetObjectOptions();

	wxMenu *NewVariableMenu;
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
