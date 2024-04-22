/*=============================================================================
	KismetDebugger.h: Kismet Debugger window
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved
=============================================================================*/

#ifndef __KISMETDEBUGGER_H__
#define __KISMETDEBUGGER_H__

#include "Kismet.h"
#include "UnrealEd.h"

enum EKismetDebuggerNextType
{
	KDNT_NextAny				= 0,
	KDNT_NextBreakpointChain	= 1,
	KDNT_NextSelected			= 2
};

class WxKismetDebuggerCallstack : public wxPanel
{
public:
	DECLARE_DYNAMIC_CLASS(WxKismetDebuggerCallstack);
	//WxKismetDebuggerCallstack( WxKismetDebugger* InDebugger, wxWindowID InID );
	virtual ~WxKismetDebuggerCallstack();

	/**
	 * Initialize the callstack panel.  Must be the first function called after creation.
	 *
	 * @param	InParent	the window that opened this dialog
	 */
	virtual void Create( wxWindow* InParent );

	/**
	 * Clears the callstack list
	 */
	void ClearCallstack();

	/**
	 * Returns true if callstack has nodes
	 */
	UBOOL CallstackHasNodes() const;

	/**
	 * Appends a node to the nodes list
	 *
	 * @param SequenceOp A reference to the sequenceop to jump to that node when selected
	 */
	UBOOL AppendNode(USequenceOp* SequenceOp);

	/**
	 * Sets the selection to the passed in index
	 */
	UBOOL SetNodeListSelection(INT Index);

	/**
	 * Sets the selection to the passed in SequenceObject if it exists
	 *
	 * @param SequenceObj the Object to search for in the listbox
	 */
	UBOOL SetNodeListSelectionObject(USequenceObject* SequenceObj);

	/**
	 * Gets the selected SequenceOp
	 *
	 * @return Reference to the selected sequenceop
	 */
	USequenceOp* GetSelectedNode() const;

	/**
	 * Copy the callstack to the clipboard
	 */
	void CopyCallstack() const;

	wxListBox* GetNodeList() const;
private:

	wxListBox* NodeList;
	
	WxKismetDebugger* KismetDebugger;

	/**
	 * Callback for when the editor closes, clear the callstack to ensure all data is cleaned up properly
	 */
	void OnClose(wxCloseEvent& In);

	DECLARE_EVENT_TABLE()
};

class WxKismetDebugger: public WxKismet 
{
public:
	WxKismetDebugger( wxWindow* InParent, wxWindowID InID, class USequence* InSequence, const TCHAR* Title = TEXT("KismetDebugger") );
	virtual ~WxKismetDebugger();

	virtual void CreateControls( UBOOL bTreeControl );

	// Override certain interaction functions in WxKismet to make it "read-only"
	virtual void OpenNewObjectMenu();
	virtual void OpenObjectOptionsMenu();
	virtual void OpenConnectorOptionsMenu() {};
	virtual void ClickedLine(FLinkedObjectConnector &Src, FLinkedObjectConnector &Dest) {};
	virtual void DoubleClickedLine(FLinkedObjectConnector &Src, FLinkedObjectConnector &Dest) {};
	virtual void DoubleClickedObject(UObject* Obj) {};
	virtual void DoubleClickedConnector(FLinkedObjectConnector& Connector) {};
	virtual UBOOL ClickOnBackground() {return TRUE;};
	virtual UBOOL ClickOnConnector(UObject* InObj, INT InConnType, INT InConnIndex) {return TRUE;};
	virtual void AltClickConnector(FLinkedObjectConnector& Connector) {};
	virtual void MoveSelectedObjects( INT DeltaX, INT DeltaY ) {};
	virtual void PositionSelectedObjects() {};
	virtual void EdHandleKeyInput(FViewport* Viewport, FName Key, EInputEvent Event);
	virtual void SpecialDrag( INT DeltaX, INT DeltaY, INT NewX, INT NewY, INT SpecialIndex ) {};
	virtual void BeginTransactionOnSelected() {};
	virtual void EndTransactionOnSelected() {};

	// Debugging flow control methods
	void RealtimeDebuggingPause();
	void RealtimeDebuggingContinue();
	void RealtimeDebuggingStep();
	void RealtimeDebuggingNext();

	// "Run to next" methods
	void CheckForHiddenBreakPoint();
	UBOOL SetHiddenBreakpoints();
	void ClearAllHiddenBreakpoints();

	UBOOL bHasBeenMarkedDirty;
	TArray<FString> DebuggerBreakpointQueue;
	USequenceOp* CurrentSeqOp;
	UBOOL bAreHiddenBreakpointsSet;
	UBOOL bIsRunToNextMode;
	INT NextType;

	WxKismetDebuggerCallstack*	Callstack;

	DECLARE_EVENT_TABLE()
};

class WxKismetDebuggerToolBar : public WxKismetToolBar
{
public:
	WxKismetDebuggerToolBar( wxWindow* InParent, wxWindowID InID );
	~WxKismetDebuggerToolBar();

	WxMaskedBitmap PauseB, ContinueB, NextB, StepB;

	void UpdateDebuggerButtonState();

	DECLARE_EVENT_TABLE()
};

class WxMBKismetDebuggerBasicOptions : public wxMenu
{
public:
	WxMBKismetDebuggerBasicOptions(WxKismet* SeqEditor);
	~WxMBKismetDebuggerBasicOptions();
};

class WxMBKismetDebuggerObjectOptions : public wxMenu
{
public:
	WxMBKismetDebuggerObjectOptions(WxKismet* SeqEditor);
	~WxMBKismetDebuggerObjectOptions();
};

class WxKismetDebuggerNextToolBarButtonRightClick : public wxMenu
{
public:
	WxKismetDebuggerNextToolBarButtonRightClick(WxKismetDebugger* Window);
	~WxKismetDebuggerNextToolBarButtonRightClick();

	void OnNextTypeButton( wxCommandEvent& In );

private:
	WxKismetDebugger* DebuggerWindow;
};


#endif // __KISMETDEBUGGER_H__