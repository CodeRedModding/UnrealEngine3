/*=============================================================================
	Facade.h: ProcBuilding Ruleset editing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FACADE_H__
#define __FACADE_H__

#include "UnrealEd.h"
#include "UnLinkedObjDrawUtils.h"

/** 
 * Helper class to allow the use of TTransArray for tracking the array of all rules to display
 * in Facade. While initially populated from the ruleset currently being edited, the user can
 * perform actions that manipulate the contents of the array which would not be trackable otherwise.
 */
class UFacadeHelper : public UObject
{
	DECLARE_CLASS_INTRINSIC(UFacadeHelper,UObject,CLASS_Transient,UnrealEd);
public:
	/** Default constructor */
	UFacadeHelper();

	/**
	 * Serialize this class' UObject references
	 *
	 * @param	Ar	Archive to serialize to
	 */
	virtual void Serialize( FArchive& Ar );

	/** All of the rules to be displayed within Facade */
	TTransArray<class UPBRuleNodeBase*> AllRules;
};

/*-----------------------------------------------------------------------------
	WxFacadeToolBar
-----------------------------------------------------------------------------*/

class WxFacadeToolBar : public WxToolBar
{
public:
	WxFacadeToolBar( wxWindow* InParent, wxWindowID InID );
	~WxFacadeToolBar();

	WxMaskedBitmap ReApplyB;
	WxBitmap UndoB, RedoB;
};

/*-----------------------------------------------------------------------------
	WxFacade
-----------------------------------------------------------------------------*/

class WxFacade : public WxLinkedObjEd, protected FLinkedObjectDrawHelper
{
public:
	DECLARE_DYNAMIC_CLASS(WxFacade);

	WxFacade();
	WxFacade( wxWindow* InParent, wxWindowID InID, class UProcBuildingRuleset* InRuleset);
	virtual ~WxFacade();

	/////// LINKEDOBJEDITOR INTERFACE
	virtual void InitEditor();

	/**
	* @return Returns the name of the inherited class, so we can generate .ini entries for all LinkedObjEd instances.
	*/
	virtual const TCHAR* GetConfigName() const
	{
		return TEXT( "Facade" );
	}

	virtual void CreateControls( UBOOL bTreeControl );

	virtual void OpenNewObjectMenu();
	virtual void OpenObjectOptionsMenu();
	virtual void OpenConnectorOptionsMenu();
	virtual void ClickedLine(FLinkedObjectConnector &Src, FLinkedObjectConnector &Dest);
	virtual void DoubleClickedObject(UObject* Obj);

	virtual void DrawObjects( FViewport* Viewport, FCanvas* Canvas );
	virtual void DrawThumbnail (UObject* PreviewObject,  TArray<UMaterialInterface*>& InMaterialOverrides, FViewport* Viewport, FCanvas* Canvas, const FIntRect& InRect);
	
	virtual void UpdatePropertyWindow();

	virtual void EmptySelection();
	virtual void AddToSelection( UObject* Obj );
	virtual void RemoveFromSelection( UObject* Obj );
	virtual UBOOL IsInSelection( UObject* Obj ) const;
	virtual INT GetNumSelected() const;

	virtual void SetSelectedConnector( FLinkedObjectConnector& Connector );
	virtual FIntPoint GetSelectedConnLocation( FCanvas* Canvas );
	virtual INT GetSelectedConnectorType();

	virtual void MakeConnectionToConnector( FLinkedObjectConnector& Connector );
	virtual void MakeConnectionToObject( UObject* EndObj );

	virtual void AltClickConnector( FLinkedObjectConnector& Connector );
	virtual UBOOL ClickOnBackground();

	virtual void MoveSelectedObjects( INT DeltaX, INT DeltaY );
	virtual void EdHandleKeyInput( FViewport* Viewport, FName Key, EInputEvent Event );
	virtual void SpecialDrag( INT DeltaX, INT DeltaY, INT NewX, INT NewY, INT SpecialIndex );

	virtual void NotifyObjectsChanged();

	////// FNOTIFYHOOK INTERFACE
	virtual void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );

	////// FACADE INTERFACE

	// Static, initialization stuff
	
	/** Static functions that fills in array of all available UPBRuleNode classes and sorts them alphabetically. */
	static void InitPBRuleClasses();

	// Utils
	
	/** Connect FromConnIndex output connector on FromNode to the input of ToNode */
	void ConnectRules( UPBRuleNodeBase* FromNode, INT FromConnIndex, UPBRuleNodeBase* ToNode );
	/** Delete all selected rule nodes - will clear connections to them */
	void DeleteSelectedRules();

	/** Select all rules after the supplied one */
	void SelectRulesAfterSelected();

	/** Util for breaking all links to a specified rule */
	void BreakLinksToRule(UPBRuleNodeBase* Rule, UBOOL bBreakNextRulesLinks = TRUE );

	/** Break link at a specific connector */
	void BreakConnection(UObject* BreakConnObj, INT BreakConnType, INT BreakConnIndex);

	/** Remesh all building actors in level using edited ruleset */
	void ReapplyRuleset();

	/** Create a new rule of the given class, at current cursor location */
	UPBRuleNodeBase* CreateNewRuleOfClass(UClass* NewRuleClass);

	/** Called when mouse is clicked on background, to see if we want to make a new 'shortcut' object (key+click) */
	UPBRuleNodeBase* NewShortcutObject();

	// Context Menu handlers
	void OnSelectDownsteamNodes( wxCommandEvent& In );
	void OnSelectUpsteamNodes( wxCommandEvent& In );
	void OnContextNewRule( wxCommandEvent& In );
	void OnContextBreakLink( wxCommandEvent& In );
	// Toolbar
	void OnReapplyRuleset( wxCommandEvent& In );

	/**
	 * Handle user selecting toolbar option to undo
	 *
	 * @param	In	Event automatically generated by wxWidgets when the user clicks the undo toolbar option
	 */
	void OnFacadeUndo( wxCommandEvent& In );

	/**
	 * Handle user selecting toolbar option to redo
	 *
	 * @param	In	Event automatically generated by wxWidgets when the user clicks the redo toolbar option
	 */
	void OnFacadeRedo( wxCommandEvent& In );

	/**
	 * Update the UI for the undo toolbar option
	 *
	 * @param	In	Event automatically generated by wxWidgets to update the UI
	 */
	void UpdateFacadeUndoUI( wxUpdateUIEvent& In );

	/**
	 * Update the UI for the redo toolbar option
	 *
	 * @param	In	Event automatically generated by wxWidgets to update the UI
	 */
	void UpdateFacadeRedoUI( wxUpdateUIEvent& In );

	virtual void Serialize( FArchive& Ar );

	WxFacadeToolBar* ToolBar;

	// Ruleset currently being edited
	class UProcBuildingRuleset* Ruleset;

	// Currently selected nodes
	TArray<class UPBRuleNodeBase*> SelectedRules;

	// Selected Connector
	UObject* ConnObj;
	INT ConnType;
	INT ConnIndex;

	// Static list of all SequenceObject classes.
	static TArray<UClass*>	PBRuleClasses;
	static UBOOL			bPBRuleClassesInitialized;

protected:

	void Copy();
	void Paste();

	// Undo/Redo Transaction Support

	/**
	 * Begin a transaction within Facade that should be tracked for undo/redo purposes.
	 *
	 * @param	SessionName	Name of this transaction session
	 */
	void BeginFacadeTransaction( const TCHAR* SessionName );

	/** End a transaction within Facade */
	void EndFacadeTransaction();

	/** Undo the last Facade transaction, if possible */
	void FacadeUndo();

	/** Redo the last Facade transaction, if possible */
	void FacadeRedo();

	INT PasteCount;
	INT DuplicationCount;
	
	/** Helper class to track all of the rules visible in Facade */
	UFacadeHelper* FacadeHelper;

	/** Transaction buffer for undo/redo support */
	class UTransBuffer* FacadeTrans;
	
	DECLARE_EVENT_TABLE()
};



#endif // __FACADE_H__
