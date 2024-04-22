/*=============================================================================
	Kismet.h: Gameplay sequence editing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __KISMET_H__
#define __KISMET_H__

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"

class WxKismetDebugger;

/*-----------------------------------------------------------------------------
	WxKismetToolBar
-----------------------------------------------------------------------------*/

class WxKismetToolBar : public WxToolBar
{
public:
	WxKismetToolBar( wxWindow* InParent, wxWindowID InID );
	~WxKismetToolBar();

	WxBitmap ParentSequenceB;
	WxMaskedBitmap RenameSequenceB, HideB, ShowB, CurvesB, SearchB, CreateSeqObjB, ZoomToFitB, UpdateB, OpenB, ClearBreakPointsB;

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

/* ==========================================================================================================
	WxSequenceTreeCtrl
========================================================================================================== */
/**
 * This tree control displays the list of sequences opened for edit
 */ 
class WxSequenceTreeCtrl : public WxTreeCtrl, public FSerializableObject
{
	DECLARE_DYNAMIC_CLASS(WxSequenceTreeCtrl)

public:
	/** Default constructor for use in two-stage dynamic window creation */
	WxSequenceTreeCtrl();
	virtual ~WxSequenceTreeCtrl();

	/**
	 * Initialize this control when using two-stage dynamic window creation.  Must be the first function called after creation.
	 *
	 * @param	InParent	the window that opened this dialog
	 * @param	InID		the ID to use for this dialog
	 * @param	InEditor	pointer to the editor window that contains this control
	 * @param   InStyle		Style of this tree control.
	 */
	void Create( wxWindow* InParent, wxWindowID InID, class WxKismet* InEditor, LONG InStyle = wxTR_HAS_BUTTONS );
private:
	using WxTreeCtrl::Create;		// Hide parent implementation

	/**
	 * Used to restore the expanded state of the tree items when the tree is refreshed
	 */
	void FindExpandedNodes(wxTreeItemId NodeItem, TArray<FString>& NodeList, UBOOL GenerateList);
	
public:

	/**
	 * Repopulates the tree control with the sequences of all levels in the world.
	 */
	virtual void RefreshTree();

	/**
	 * Deletes all tree items from the list and clears the Sequence-ItemId map.
	 */
	void ClearTree();

	/**
	 * Recursively adds all subsequences of the specified sequence to the tree control.
	 */
	virtual void AddChildren( USequence* ParentSeq, wxTreeItemId ParentId );

	/**
	 * De/selects the tree item corresponding to the specified object
	 *
	 * @param	SeqObj		the sequence object to select in the tree control
	 * @param	bSelect		whether to select or deselect the sequence
	 *
	 * @param	True if the specified sequence object was found
	 */
	virtual UBOOL SelectObject( USequenceObject* SeqObj, UBOOL bSelect=TRUE );

	/** FSerializableObject interface */
	virtual void Serialize( FArchive& Ar );

protected:
	/** the kismet editor window that contains this tree control */
	class WxKismet* KismetEditor;

	/** maps the sequence objects to their corresponding tree id */
	TMap<USequenceObject*, wxTreeItemId> TreeMap;

	/**
	 * Adds the root sequence of the specified level to the sequence tree, if a sequence exists for the specified level.
	 *
	 * @param	Level		The level whose root sequence should be added.  Can be NULL.
	 * @param	RootId		The ID of the tree's root node.
	 */
	void AddLevelToTree(ULevel* Level, wxTreeItemId RootId);

private:
	/** Hide this constructor */
	WxSequenceTreeCtrl( wxWindow* parent, wxWindowID id, wxMenu* InMenu ) {}
};


/*-----------------------------------------------------------------------------
	WxKismetSearch
-----------------------------------------------------------------------------*/

enum EKismetSearchType
{
	KST_NameComment = 0,
	KST_ObjName = 1,
	KST_VarName = 2,
	KST_VarNameUses = 3,
	KST_RemoteEvents = 4,
	KST_ReferencesRemoteEvent = 5,
	KST_ObjectType = 6,
	KST_ObjProperties = 7,

	MAX_KST_TYPES
};

enum EKismetSearchScope
{
	KSS_CurrentLevel = 0,
	KSS_CurrentSequence = 1,
	KSS_AllLevels = 2,

	MAX_KSS_TYPES
};

class WxKismetSearch : public wxDialog
{
public:
	WxKismetSearch( WxKismet* InKismet, wxWindowID InID );

	/**
	 * Loads prior search settings from the editor user settings ini.
	 */
	void LoadSearchSettings();

	/**
	 * Saves prior search settings to the editor user settings ini.
	 */
	void SaveSearchSettings();

	/**
	 * Clears the results list, clearing references to the previous results.
	 */
	void ClearResultsList();

	/**
	 * Appends a results to the results list.
	 *
	 * @param	ResultString	The string to display in the results list.
	 * @param	SequenceObj		The sequence object associated with the search result.
	 */
	void AppendResult(const TCHAR* ResultString, USequenceObject* SequenceObject);

	UBOOL SetResultListSelection(INT Index);

	/**
	 * @return		The selected search result sequence, or NULL if none selected.
	 */
	USequenceObject* GetSelectedResult() const;

	/**
	 * @return		The number of results in the results list.
	 */
	INT GetNumResults() const;

	/**
	 * @return		The current search string.
	 */
	FString GetSearchString() const;

	/**
	 * Sets the search string field.
	 *
	 * @param	SearchString	The new search string.
	 */
	void SetSearchString(const TCHAR* SearchString);

	/**
	 * @return		The selected search type setting.
	 */
	INT GetSearchType() const;

	/**
	 * Sets the search type.
	 */
	void SetSearchType(EKismetSearchType SearchType);

	/**
	 * @return		The selected search scope setting.
	 */
	INT GetSearchScope() const;

	/**
	 * Sets the search scope.
	 */
	void SetSearchScope(EKismetSearchScope SearchScope);

	/**
	 * Returns the currently selected object class in the search dialog, if any
	 *
	 * @return	The object class type currently selected in the search dialog; NULL if no type is currently selected
	 */
	UClass* GetSearchObjectClass() const;

	/**
	 * Sets the currently selected object class in the search dialog, if possible
	 *
	 * @param	SelectedClass	Class to attempt to set the search dialog to; 
	 *							No change is made if the passed in class is not currently an option in the combo box
	 */
	void SetSearchObjectClass( const UClass* SelectedClass );

	/**
	 * Sets the currently selected object class in the search dialog, if possible
	 *
	 * @param	SelectedClassName	Name of the class to attempt to set the search dialog to; 
	 *								No change is made if the passed in class name is not currently an option in the combo box
	 */
	void SetSearchObjectClass( const FString& SelectedClassName );

	void UpdateSearchWindowResults(TArray<USequenceObject*> &Results);

	/**
	 * Called when user clicks on a search result. Changes active Sequence
	 * and moves the view to center on the selected SequenceObject.
	 */
	void OnSearchResultChanged(wxCommandEvent &In);

private:
	wxListBox*		ResultList;
	wxTextCtrl*		NameEntry;
	WxComboBox*		ObjectTypeCombo;
	wxComboBox*		SearchTypeCombo;
	wxComboBox*		SearchScopeCombo;
	wxButton*		SearchButton;

	WxKismet*		Kismet;

	/** Internal helper method to keep the object type combo box up-to-date based on the current search scope */
	void UpdateObjectTypeControl();

	/**
	 * Internal helper method to extract all of the sequence object classes represented within a particular sequence
	 *
	 * @param	CurSequence	Sequence to extract all sequence object classes from
	 * @param	OutClasses	Array to populate with extracted object classes
	 * @param	bRecursive	If TRUE, the method will be recursively called on any sub-sequences found while searching
	 */
	void GetClassesFromSequence( const USequence* CurSequence, TArray<UClass*>& OutClasses, UBOOL bRecursive );

	/** 
	 * Internal helper method to update which control should display next to the "Search For" label. For any search type except "object type,"
	 * the search dialog should show a text box to allow the user to type in their search query. For "object type" searches, a combo box should
	 * be displayed, auto-populated with all of the valid types to search for.
	 */
	void UpdateSearchForControls();

	void OnCancel( wxCommandEvent& In ) { wxCloseEvent Close; OnClose(Close); }
	void OnClose(wxCloseEvent& In);

	/**
	 * Called in response to the user changing the search type combo box
	 *
	 * @param	In	Event generated by wxWidgets when the user changes the combo box selection
	 */
	void OnSearchTypeChange( wxCommandEvent& In );

	/**
	 * Called in response to the user changing the search scope combo box
	 *
	 * @param	In	Event generated by wxWidgets when the user changes the combo box selection
	 */
	void OnSearchScopeChange( wxCommandEvent& In );

	/** 
	 * Handler for pressing Search button (or pressing Enter on entry box) on the Kismet Search tool.
	 * Searches all sequences and puts results into the ResultList list box.
	 */
	void OnDoSearch( wxCommandEvent &In );

	DECLARE_EVENT_TABLE()
};


class WxKismetClassSearch : public wxDialog
{
public:
	enum EKismetClassSearchFields
	{
		EKCSF_Name,		// Search by name
		EKCSF_Type,		// Search by type
		EKCSF_Category,	// Search by category
	};

	class FKismetClassSearchOptions
	{
	public:
		FKismetClassSearchOptions()
			: Column( EKCSF_Name )
			, bSortAscending( TRUE )
		{}

		/** The column currently being used for sorting. */
		EKismetClassSearchFields Column;

		/** Denotes ascending/descending sort order. */
		UBOOL bSortAscending;
	};

	WxKismetClassSearch( WxKismet* InKismet, wxWindowID InID );

	/**
	 * Clears the results list, clearing references to the previous results.
	 */
	void ClearResultsList();

	/**
	 * Appends a results to the results list.
	 *
	 * @param	ResultString			The string to display in the results list.
	 * @param	ResultType				The type of the sequence object (Action, Event, etc.)
	 * @param	ResultCategory			The category of the sequence object (Actor, Physics, etc.)
	 * @param	SequenceObjectClass		The sequence object class associated with the search result.
	 */
	void AppendResult(const TCHAR* ResultString, const TCHAR* ResultType, const TCHAR* ResultCategory, UClass* SequenceObjectClass);

	/**
	 * Selects the item in the results list
	 * 
	 * @param	Index					The index into the result list
	 *
	 * @return	UBOOL					True if sucessful
	 */
	UBOOL SetResultListSelection(INT Index);

	void ItemSelected(wxListEvent& event);

	void OnColumnClicked(wxListEvent& In);

	/**
	 * Sets the selects items in the results list to bold
	 */
	void UpdateItemsFont();

	/**
	 * Gets the search string name for a given object type
	 * 
	 * @param	InObject				The object to get the string of
	 *
	 * @return	FString					The string to use for the object
	 */
	static FString GetKismetObjectTypeString( const USequenceObject* InObject );

	/**
	 * Gets the search string name for a given class type
	 * 
	 * @param	InClass					The class to get the string of
	 * @param	SearchField				The type of search, name/type/category
	 *
	 * @return	FString					The string to use for the class
	 */
	static FString GetKismetClassSearchString( UClass* InClass, EKismetClassSearchFields SearchField );

	UBOOL OnKeyDown( wxKeyEvent &In );

	/**
	 * @return		The selected search result sequence, or NULL if none selected.
	 */
	UClass* GetSelectedResult() const;

	/**
	 * @return		The current search string.
	 */
	FString GetSearchString() const;

	/**
	 * Sets the search string field.
	 *
	 * @param	SearchString	The new search string.
	 */
	void SetSearchString(const TCHAR* SearchString);

private:
	FKismetClassSearchOptions	SearchOptions;

	wxListCtrl*					ResultList;
	wxTextCtrl*					NameEntry;
	wxButton*					CreateButton;

	WxKismet*					Kismet;

	void OnCancel( wxCommandEvent& In ) { wxCloseEvent Close; OnClose(Close); }
	void OnClose(wxCloseEvent& In);
	void DoClose();

	/**
	 * Called when user selects a sequence object class to add
	 */
	void OnSequenceClassSelected( wxCommandEvent &In );

	/**
	 * Called when user activates a sequence object class to add
	 */
	void OnSequenceClassActivated( wxListEvent &In );

	/**
	 * Adds a new class of the type selected in the results list
	 */
	void PlaceSelectedClass();

	/** 
	 * Handler for pressing Search button (or pressing Enter on entry box) on the Kismet Class Search tool.
	 * Searches all sequence object classes and puts results into the ResultList list box.
	 */
	void OnDoClassSearch( wxCommandEvent &In );
	void DoClassSearch();

	DECLARE_EVENT_TABLE()
};



class WxKismetUpdate : public wxDialog
{
public:
	WxKismetUpdate( WxKismet* InKismet, wxWindowID InID );
	~WxKismetUpdate();

	void OnCancel( wxCommandEvent& In ) { wxCloseEvent Close; OnClose(Close); }
	void OnClose( wxCloseEvent& In );

	wxListBox*		UpdateList;
	wxButton		*UpdateButton, *UpdateAllButton;

	WxKismet*		Kismet;

	virtual void BuildUpdateList();
	void OnUpdateListChanged( wxCommandEvent &In );

	void OnContextUpdate( wxCommandEvent &In );
	void OnContextUpdateAll( wxCommandEvent &In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	FKismetNavigationHistoryData
-----------------------------------------------------------------------------*/

/**
 * Structure designed for use within the kismet editor navigation history system. 
 * Inherits from the base FLinkedObjEdNavigationHistoryData structure and stores additional
 * information relevant specifically to Kismet, including an array of any sequence objects
 * that are important to the history event, as well as the the current sequence when the 
 * history event was generated.
 */
struct FKismetNavigationHistoryData : public FLinkedObjEdNavigationHistoryData, public FSerializableObject
{
	TArray<USequenceObject*> HistoryRelevantSequenceObjects;	/** Array of sequence objects relevant to the history event; Can be empty */
	USequence* HistorySequence;									/** Current sequence at the time of the history event; *CANNOT* be NULL */

	/**
	 * Construct a FKismetNavigationHistoryData object 
	 *
	 * @param	InHistorySequence	The current Kismet sequence when this object is created; *Required* to properly handle sequence switches; *CANNOT* be NULL
	 * @param	InHistoryString		String to display in the navigation menu; *CANNOT* be empty
	 */
	FKismetNavigationHistoryData( USequence* InHistorySequence, const FString& InHistoryString );

	/**
	 * Destroy a FKismetNavigationHistoryData object
	 */
	~FKismetNavigationHistoryData();

	/**
	 * Convenience function for adding relevant sequence objects to the data's array
	 *
	 * @param	InSeqObj	Sequence object to add to the data's array
	 */
	void AddHistoryRelevantSequenceObject( USequenceObject* InSeqObj );

	/**
	 * Serialize UObject references within the struct so if the objects are deleted from
	 * elsewhere the struct will be updated with NULL pointers
	 *
	 * @param	Ar	The archive to serialize with
	 */
	virtual void Serialize( FArchive& Ar );
	
};

/*-----------------------------------------------------------------------------
	WxKismet
-----------------------------------------------------------------------------*/

struct FCopyConnInfo
{
	class USequenceObject*	SeqObject;
	INT						ConnIndex;

	FCopyConnInfo()
	: SeqObject(NULL)
	{}

	FCopyConnInfo(USequenceObject* InSeqObject, INT InConnIndex) 
	: SeqObject(InSeqObject)
	, ConnIndex(InConnIndex)
	{}

	friend FArchive& operator<<(FArchive &Ar, FCopyConnInfo &Info);
};

struct FExposeVarLinkInfo
{
	// for EExposeType::Property
	class UProperty *Property;
	class UClass *VariableClass;
	// for EExposeType::HiddenLink
	INT LinkIdx;

	enum EExposeType
	{
		TYPE_Unknown = 0,
		TYPE_Property = 1,			// expose a property in a new link
		TYPE_HiddenLink = 2,		// expose a hidden link
        TYPE_GFxArrayElement = 3,
	};
	// type of expose this is
	EExposeType Type;

	FExposeVarLinkInfo()
	: Property(NULL)
	, VariableClass(NULL)
	, LinkIdx(-1)
	, Type(TYPE_Unknown)
	{}

	FExposeVarLinkInfo(class UProperty *InProperty, class UClass *InVariableClass)
	: Property(InProperty)
	, VariableClass(InVariableClass)
	, LinkIdx(-1)
	, Type(TYPE_Property)
	{}

	FExposeVarLinkInfo(INT InLinkIdx)
	: Property(NULL)
	, VariableClass(NULL)
	, LinkIdx(InLinkIdx)
	, Type(TYPE_HiddenLink)
	{}

	friend FArchive& operator<<(FArchive &Ar, FExposeVarLinkInfo &Info);
};

//FIXME: change this to a configurable value maybe?
#define KISMET_GRIDSIZE			8

class WxKismet : public WxLinkedObjEd, public FCallbackEventDevice
{
public:
	WxKismet( wxWindow* InParent, wxWindowID InID, class USequence* InSequence, const TCHAR* Title = TEXT("Kismet") );
	virtual ~WxKismet();

	/**
	 * Populates a provided TArray with all of the actors referenced by the provided sequence objects
	 *
	 * @param	OutReferencedActors	Array to populate with actors referenced by the provided sequence objects
	 * @param	InObjectsToCheck	Provided array of sequence objects to check for actor references
	 * @param	bRecursive			If TRUE, will check all sequence objects contained within any USequences found in the provided array
	 */
	static void GetReferencedActors( TArray<AActor*>& OutReferencedActors, const TArray<USequenceObject*>& InObjectsToCheck, UBOOL bRecursive );

	/**
	 * Selects any actors in the editor that are referenced by the provided sequence objects
	 *
	 * @param	ObjectsToCheck		Array of sequence objects to check for actor references
	 * @param	bRecursive			If TRUE, will check all sequence objects contained within any USequences found in the provided array
	 * @param	bDeselectAllPrior	If TRUE, will deselect all currently selected actors in the editor before selecting referenced actors
	 */
	static void SelectReferencedActors( const TArray<USequenceObject*>& ObjectsToCheck, UBOOL bRecursive, UBOOL bDeselectAllPrior );

	// Initalization / Accessors
	virtual void		InitEditor();
	virtual void		CreateControls( UBOOL bTreeControl );
	virtual USequence*	GetRootSequence();
//	virtual void		SetRootSequence( USequence* Seq );
	virtual void		InitSeqObjClasses();

	/**
	 * @return Returns the name of the inherited class, so we can generate .ini entries for all LinkedObjEd instances.
	 */
	virtual const TCHAR* GetConfigName() const
	{
		return TEXT("Kismet");
	}

	// LinkedObjEditor interface

	/**
	 * Creates the tree control for this linked object editor.  Only called if TRUE is specified for bTreeControl
	 * in the constructor.
	 *
	 * @param	TreeParent	the window that should be the parent for the tree control
	 */
	virtual void CreateTreeControl( wxWindow* TreeParent );

	virtual void OpenNewObjectMenu();
	virtual void OpenObjectOptionsMenu();
	virtual void OpenConnectorOptionsMenu();
	virtual void ClickedLine(FLinkedObjectConnector &Src, FLinkedObjectConnector &Dest);
	virtual void DoubleClickedLine(FLinkedObjectConnector &Src, FLinkedObjectConnector &Dest);
	virtual void DoubleClickedObject(UObject* Obj);
	virtual void DoubleClickedConnector(FLinkedObjectConnector& Connector);
	virtual UBOOL ClickOnBackground();
	virtual UBOOL ClickOnConnector(UObject* InObj, INT InConnType, INT InConnIndex);
	virtual void DrawObjects(FViewport* Viewport, FCanvas* Canvas);
	virtual void UpdatePropertyWindow();

	virtual void EmptySelection();
	virtual void AddToSelection( UObject* Obj );
	virtual void RemoveFromSelection( UObject* Obj );
	virtual UBOOL IsInSelection( UObject* Obj ) const;
	virtual INT GetNumSelected() const;

	virtual void SetSelectedConnector( FLinkedObjectConnector& Connector );
	virtual FIntPoint GetSelectedConnLocation(FCanvas* Canvas);

	/**
	 * Adjusts the postion of the selected connector based on the Delta position passed in.
	 * Currently only variable, event, and output connectors can be moved. 
	 * 
	 * @param DeltaX	The amount to move the connector in X
	 * @param DeltaY	The amount to move the connector in Y	
	 */
	virtual void MoveSelectedConnLocation(INT DeltaX, INT DeltaY);
	/**
	 * Sets the member variable on the selected connector struct so we can perform different calculations in the draw code
	 * 
	 * @param bMoving	True if the connector is moving
	 */
	virtual void SetSelectedConnectorMoving( UBOOL bMoving );
	virtual INT GetSelectedConnectorType();
	virtual UBOOL ShouldHighlightConnector(FLinkedObjectConnector& Connector);
	virtual FColor GetMakingLinkColor();

	// Make a connection between selected connector and an object or another connector.
	virtual void MakeConnectionToConnector( FLinkedObjectConnector& Connector );
	virtual void MakeConnectionToObject( UObject* EndObj );

	/**
	 * Called when the user releases the mouse over a link connector and is holding the ALT key.
	 * Commonly used as a shortcut to breaking connections.
	 *
	 * @param	Connector	The connector that was ALT+clicked upon.
	 */
	virtual void AltClickConnector(FLinkedObjectConnector& Connector);

	virtual void MoveSelectedObjects( INT DeltaX, INT DeltaY );
	virtual void PositionSelectedObjects();
	virtual void EdHandleKeyInput(FViewport* Viewport, FName Key, EInputEvent Event);
	virtual void OnMouseOver(UObject* Obj);
	virtual void ViewPosChanged();
	virtual void SpecialDrag( INT DeltaX, INT DeltaY, INT NewX, INT NewY, INT SpecialIndex );

	virtual void BeginTransactionOnSelected();
	virtual void EndTransactionOnSelected();

	virtual void NotifyObjectsChanged();

	/**
	 * Called when the user attempts to set a bookmark via CTRL + # key. Saves the current sequence + camera zoom/position
	 * to be recalled later.
	 *
	 * @param	InIndex		Index of the bookmark to set
	 */
	virtual void SetBookmark( UINT InIndex );

	/**
	 * Called when the user attempts to check to see if a bookmark exists
	 *
	 * @param	InIndex		Index of the bookmark to set
	 */
	virtual UBOOL CheckBookmark( UINT InIndex );

	/**
	 * Called when the user attempts to jump to a bookmark via a # key. Recalls the saved data, if possible, and jumps the screen to it.
	 *
	 * @param	InIndex		Index of the bookmark to jump to
	 */
	virtual void JumpToBookmark( UINT InIndex );

	/**
	 * Add a new history data item to the user's navigation history, storing the current state
	 *
	 * @param	InHistoryString		The string that identifies the history data operation and will display in a navigation menu (CANNOT be empty)
	 */
	virtual void AddNewNavigationHistoryDataItem( FString InHistoryString );

	/**
	 * Update the current history data item of the user's navigation history with any desired changes that have occurred since it was first added,
	 * such as camera updates, etc.
	 */
	virtual void UpdateCurrentNavigationHistoryData();

	// Static, initialization stuff

	/** 
	 * Opens a kismet window to edit the specified sequence.  If InSequence is NULL, the kismet
	 * of the current level will be opened, or created if it does not yet exist.
	 *
	 * @param	InSequence			The sequence to open.  If NULL, open the current level's root sequence.
	 * @param	bCreateNewWindow	If TRUE, open the sequence in a new kismet window.
	 * @param	WindowParent		When a window is being created, use this as its parent.
	 */
	static void OpenKismet(USequence* InSequence, UBOOL bCreateNewWindow, class wxWindow* ParentWindow);

	/**
	 * Opens a new Kismet debugger window, or, if one already exists, set focus to it
	 *
	 * @param	InSequence				The sequence to open.  If NULL, open the current level's root sequence.
	 * @param	WindowParent			When a window is being created, use this as its parent.
	 * @param	SequenceName			The name of the sequence op to be centered upon.
	 * @param	bShouldSetButtonState	Determines whether, or not, the state of the debugger buttons should be modified
	 */
	static void OpenKismetDebugger(class USequence* InSequence, wxWindow* InParentWindow, const TCHAR* SequenceName, UBOOL bShouldSetButtonState);

	/**
	 * Returns the Kismet debugging window
	 */
	static WxKismetDebugger* FindKismetDebuggerWindow();

	/**
	 * Closes Kismet debugger windows
	 */
	static void CloseAllKismetDebuggerWindows();

	/**
	 * If any breakpoints have been modified in the debugger while running PIE, this should return true
	 */
	static UBOOL HasBeenMarkedDirty();

	/**
	 * Adds a new breakpoint to the breakpoint queue
	 */
	static UBOOL AddSequenceBreakpointToQueue(const TCHAR* SequenceName);

	static void SetVisibilityOnAllKismetWindows(UBOOL bIsVisible);
	static void CloseAllKismetWindows();
	static void EnsureNoKismetEditingSequence(USequence* InSeq);

	/** 
	 *	Searches the current levels Kismet sequences to find any references to supplied Actor.
	 *	Opens a Kismet window if none are currently open, or re-uses first one. Opens SearchWindow as well if not currently open.
	 *
	 *	@param FindActor	Actor to find references to.
	 */
	static void FindActorReferences(AActor* FindActor);

	static void OpenSequenceInKismet(USequence* InSeq, wxWindow* ParentWindow);

	/**
	 * Activates the passed in sequence for editing.
	 *
	 * @param	NewSeq						The new sequence to edit
	 * @param	bNotifyTree					If TRUE, update/refresh the tree control
	 * @param	bShouldGenerateNavHistory	If TRUE, a new navigation history data object will be added to the kismet's nav. history signifying
	 *										the sequence change
	 */
	virtual void ChangeActiveSequence(USequence *NewSeq, UBOOL bNotifyTree = TRUE, UBOOL bShouldGenerateNavHistory = TRUE );

	/** 
	 * Switch Kismet to the Sequence containing the supplied object and move view to center on it.
	 *
	 * @param	ViewObj	Sequence object which the view should center upon
	 */
	void CenterViewOnSeqObj(USequenceObject* ViewObj, UBOOL bShouldSelectObject = TRUE);
	void ZoomSelection();
	void ZoomAll();

	void ZoomToBox(const FIntRect& Box);

	// Sequence Tools

	static void OpenMatinee(class USeqAct_Interp* Interp);
	void DeleteSelected(UBOOL bSilent=FALSE);
	void MakeLogicConnection(class USequenceOp* OutOp, INT OutConnIndex, class USequenceOp* InOp, INT InConnIndex);
	void MakeVariableConnection(class USequenceOp* Op, INT OpConnIndex, class USequenceVariable* Var);
	void MakeEventConnection(class USequenceOp* Op, int OpConnIndex, class USequenceEvent* Event );
	USequenceObject* NewSequenceObject(UClass* NewSeqObjClass, INT NewPosX, INT NewPosY, UBOOL bTransient=FALSE);
	virtual USequenceObject* NewShorcutObject();
	void SingleTickSequence();


	void KismetUndo();
	void KismetRedo();
	void KismetCopy();
	void KismetPaste(UBOOL bAtMousePos=false);
	void ClearCopiedConnections();

	void SelectAll();
	void SelectAllMatching();

	void RebuildTreeControl();
	void BuildBookmarkMenu(wxMenu* Menu, const UBOOL bRebuild);
	void BuildSelectedActorLists();
	FIntRect CalcBoundingBoxOfSelected();
	FIntRect CalcBoundingBoxOfAll();

	/**
	 * Calculate the combined bounding box of the provided sequence objects.
	 * Does not produce sensible result if no objects are provided in the array.
	 *
	 * @param	InSequenceObjects	Objects to calculate the bounding box of
	 *
	 * @return	Rectangle representing the bounding box of the provided objects
	 */
	FIntRect CalcBoundingBoxOfSequenceObjects(const TArray<USequenceObject*>& InSequenceObjects);

	/** 
	*	Run a silent search now. 
	*	This will do a search without opening any search windows
	*
	* @param Results - the array to fill with search results
	* @param SearchString - the substring to search for
	* @param Type - indicates which fields to search
	* @param Scope - the scope of the search, which defaults to the current sequence
	* @param SearchClass - the object class to search for, if any
	*/
	void DoSilentSearch(TArray<USequenceObject*> &Results, const FString SearchString, EKismetSearchType Type, EKismetSearchScope Scope = KSS_CurrentSequence, UClass* SearchClass = NULL);
	void DoSearch(const TCHAR* InText, EKismetSearchType Type, UBOOL bJumpToFirstResult, EKismetSearchScope Scope = KSS_CurrentSequence, UClass* SearchClass = NULL);
	void DoClassSearch(TArray<UClass*> &Results, const FString SearchString);

	static UBOOL FindOutputLinkTo(const class USequence *sequence, const class USequenceOp *targetOp, const INT inputIdx, TArray<class USequenceOp*> &outputOps, TArray<INT> &outputIndices);
	static void ShowConnectors(TArray<USequenceObject*> &objList);
	static void HideConnectors(USequence *Sequence,TArray<USequenceObject*> &objList);

	/**
	 * Attempts to cast this window as a debugger window
	 */
	WxKismetDebugger* GetDebuggerWindow();

	/** 
	 * Iterates through all selected objects and sets their breakpoint to true
	 */
	void SetBreakpoints( TArray<USequenceObject*> &ObjectList, UBOOL bBreakpointOn );

	/** 
	 * Sets the focus in the viewport window
	 */
	void SetWindowFocus();

	// Context menu functions.

	void OnContextUpdateAction( wxCommandEvent& In );
	void OnContextNewScriptingObject( wxCommandEvent& In );
	virtual void OnContextNewScriptingObjVarContext( wxCommandEvent& In );
	void OnContextNewScriptingEventContext( wxCommandEvent& In );
	void OnContextCreateLinkedVariable( wxCommandEvent& In );
	void OnContextCreateLinkedEvent( wxCommandEvent& In );
	void OnContextClearVariable( wxCommandEvent& In );
	void OnContextDelSeqObject( wxCommandEvent& In );
	void OnContextBreakLink( wxCommandEvent& In );
	void OnContextToggleLink( wxCommandEvent& In );
	void OnContextToggleLinkPIE( wxCommandEvent& In );
	void OnContextSelectEventActors( wxCommandEvent& In );
	void OnContextFireEvent( wxCommandEvent& In );
	void OnContextOpenInterpEdit( wxCommandEvent& In );
	void OnContextSwitchAdd( wxCommandEvent& In );
	void OnConextSwitchRemove( wxCommandEvent& In );
	void OnContextBreakAllOpLinks( wxCommandEvent& In );
	void OnSelectDownsteamNodes( wxCommandEvent& In );
	void OnSelectUpsteamNodes( wxCommandEvent& In );
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
	void OnContextInsertIntoObjectList( wxCommandEvent& In );
	void OnContextRenameSequence( wxCommandEvent &In );	
	void OnContextCopyConnections( wxCommandEvent &In );	
	void OnContextPasteConnections( wxCommandEvent &In );	
	void OnContextExposeVariable( wxCommandEvent &In );
	void OnContextRemoveVariable( wxCommandEvent &In );
	void OnContextExposeOutput( wxCommandEvent &In );
	void OnContextRemoveOutput( wxCommandEvent &In );
	void OnContextSetOutputDelay( wxCommandEvent &In );
	void OnContextSetInputDelay( wxCommandEvent &In );
	void OnContextSelectAssigned( wxCommandEvent &In );
	void OnContextSearchRemoteEvents( wxCommandEvent &In );
	void OnContextApplyCommentStyle( wxCommandEvent &In );
	void OnContextCommentToFront( wxCommandEvent &In );
	void OnContextCommentToBack( wxCommandEvent &In );
	void OnContextSetBreakpoint( wxCommandEvent &In );
	void OnContextClearBreakpoint( wxCommandEvent &In );
	void OnContextAddExistingNamedVar( wxCommandEvent& In );
	void OnContextPlaySoundsOrMusicTrack( wxCommandEvent& In );
	void OnContextStopMusicTrack( wxCommandEvent& In );
	void OnContextSetBookmark( wxCommandEvent& In );
	void OnContextJumpToBookmark( wxCommandEvent& In );

	/**
	 * Called when the user picks the "Select Referenced Actors of Selected" context menu option
	 *
	 * @param	In	Event generated by wxWidgets when the menu option is selected
	 */
	void OnContextSelectReferencedActorsOfSelected( wxCommandEvent& In );

	/**
	 * Called when the user picks the "Find All Uses of Type: ___" context menu option
	 *
	 * @param	In	Event generated by wxWidgets when the menu option is selected
	 */
	void OnContextFindObjectsOfSameClass( wxCommandEvent& In );
	
	/**
	 * Called when the user picks the "Select All Matching" context menu option
	 *
	 * @param	In	Event generated by wxWidgets when the menu option is selected
	 */
	void OnContextSelectAllMatching( wxCommandEvent& In );

	void OnButtonParentSequence( wxCommandEvent &In );	
	void OnButtonRenameSequence( wxCommandEvent &In );	
	void OnButtonHideConnectors( wxCommandEvent &In );
	void OnButtonShowConnectors( wxCommandEvent &In );
	void OnButtonZoomToFit( wxCommandEvent &In );

	void OnTreeItemSelected( wxTreeEvent &In );
	void OnTreeExpanding( wxTreeEvent &In );
	void OnTreeCollapsed( wxTreeEvent &In );

	/**
	 * Toggles the "Search for SequencesObjects" tool.
	 */
	void OnOpenSearch( wxCommandEvent &In );

	void OnOpenClassSearch( wxCommandEvent &In );
	void DoOpenClassSearch();

	void OnOpenUpdate( wxCommandEvent &In );

	/**
	 * Open the current level's root sequence in a new window.
	 */
	void OnOpenNewWindow( wxCommandEvent &In );

	/**
	 * Clear out all Kismet debugger breakpoints
	 */
	void OnClearBreakpoints( wxCommandEvent &In );

	/**
	 * Toggles realtime debugging of kismet sequence objects.
	 */
	void OnRealtimeDebuggingPause( wxCommandEvent &In );
	void OnRealtimeDebuggingContinue( wxCommandEvent &In );
	void OnRealtimeDebuggingStep( wxCommandEvent &In );
	void OnRealtimeDebuggingNext( wxCommandEvent &In );
	void OnRealtimeDebuggingNextRightClick( wxCommandEvent &In );
	void OnRealtimeDebuggingCallstackNodeSelected ( wxCommandEvent &In );
	void OnRealtimeDebuggingCallstackCopy ( wxCommandEvent &In );

	void OnClose( wxCloseEvent& In );
	void OnSize(wxSizeEvent& In);

	//FCallbackEventDevice interface
	virtual void Send( ECallbackEventType InType );

	WxKismetToolBar*		ToolBar;
	WxKismetStatusBar*		StatusBar;
	WxKismetSearch*			SearchWindow;
	WxKismetClassSearch*	ClassSearchWindow;

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
	INT DuplicationCount;

	// Used for copying/pasting connections.
	INT CopyConnType;
	TArray<FCopyConnInfo>	CopyConnInfo;

	// When creating a new event, here are the options. Generated by WxMBKismetNewObject each time its brought up.
	TArray<AActor*>		NewObjActors;
	TArray<UClass*>		NewEventClasses;
	UBOOL				bAttachVarsToConnector;

	// List of all SequenceObject classes.
	TArray<UClass*>		SeqObjClasses;

	/** List of properties that can be exposed for the currently selected action */
	TMap<INT,FExposeVarLinkInfo> OpExposeVarLinkMap;

	/** Flag that gets set if the window is a realtime debugger window */
	UBOOL bIsDebuggerWindow;

	virtual void Serialize(FArchive& Ar);

	// A map of the named variables in the persistant map, to allow our event handler, ::OnContextAddExistingNamedVar,
	// to deal with easily adding a "Named Variable" of that type
	TMap<UINT, USequenceVariable*> NamedVariablesEventMap;

protected:

	/**
	 * Returns whether the specified class can be displayed in the list of ops which are available to be placed.
	 *
	 * @param	SequenceClass	a child class of USequenceObject
	 *
	 * @return	TRUE if the specified class should be available in the context menus for adding sequence ops
	 */
	virtual UBOOL IsValidSequenceClass( UClass* SequenceClass ) const;

	/**
	 * Process a specified history data object by responding to its contents accordingly 
	 * (Here, by changing active sequences if required, and then responding appropriately to relevant sequence 
	 * objects; due to how Kismet operates, the passed in data *MUST* be an instance of the FLinkedObjEdNavigationHistoryData-derived
	 * struct, FKismetNavigationHistoryData, which contains a full path to the history data's sequence)
	 *
	 * @param	InData	History data to process
	 *
	 * @return	TRUE if the navigation history data was successfully processed; FALSE otherwise
	 */
	virtual UBOOL ProcessNavigationHistoryData( const FLinkedObjEdNavigationHistoryData* InData );

private:
	
	/**
	 * Helper function which generates a string to be used in the navigation history
	 * system depending on the current state of the kismet editor
	 *
	 * @return Navigation history string based on the current state of the editor
	 */
	virtual FString GenerateNavigationHistoryString() const;

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

	void AppendBookmarkMenu(WxKismet* SeqEditor);

	wxMenu *ActionMenu, *VariableMenu, *ConditionMenu, *EventMenu, *ContextEventMenu, *ExistingNamedVariablesInPMapMenu, *ExistingNamedVariablesInOtherLevelsMenu;
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

#endif	// __KISMET_H__
