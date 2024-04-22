/*=============================================================================
	UnLinkedObjEditor.h: Base class for boxes-and-lines editing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNLINKEDOBJEDITOR_H__
#define __UNLINKEDOBJEDITOR_H__

#include "TrackableWindow.h"

/**
 * Provides notifications of messages sent from a linked object editor workspace.
 */
class FLinkedObjEdNotifyInterface
{
public:
	/* =======================================
		Input methods and notifications
	   =====================================*/

	/**
	 * Called when the user right-clicks on an empty region of the viewport
	 */
	virtual void OpenNewObjectMenu() {}

	/**
	 * Called when the user right-clicks on an object in the viewport
	 */
	virtual void OpenObjectOptionsMenu() {}

	virtual void ClickedLine(struct FLinkedObjectConnector &Src, struct FLinkedObjectConnector &Dest) {}
	virtual void DoubleClickedLine(struct FLinkedObjectConnector &Src, struct FLinkedObjectConnector &Dest) {}

	/**
	 * Called when the user double-clicks an object in the viewport
	 *
	 * @param	Obj		the object that was double-clicked on
	 */
	virtual void DoubleClickedObject(UObject* Obj) {}

	/**
	 * Called whent he use double-clicks an object connector.
	 */
	virtual void DoubleClickedConnector(struct FLinkedObjectConnector& Connector) {}
																			   
	/**
	 * Called when the user left-clicks on an empty region of the viewport
	 *
	 * @return	TRUE to deselect all selected objects
	 */
	virtual UBOOL ClickOnBackground() {return TRUE;}

	/**
	 *	Called when the user left-clicks on a connector.
	 *
	 *	@return TRUE to deselect all selected objects
	 */
	virtual UBOOL ClickOnConnector(UObject* InObj, INT InConnType, INT InConnIndex) {return TRUE;}

	/**
	 * Called when the user performs a draw operation while objects are selected.
	 *
	 * @param	DeltaX	the X value to move the objects
	 * @param	DeltaY	the Y value to move the objects
	 */
	virtual void MoveSelectedObjects( INT DeltaX, INT DeltaY ) {}

	/**
	 * Hook for child classes to perform special logic for key presses.  Called for all key events, event
	 * those which cause other functions to be called (such as ClickOnBackground, etc.)
	 */
	virtual void EdHandleKeyInput(FViewport* Viewport, FName Key, EInputEvent Event)=0;

	/**
	 * Called once when the user mouses over an new object.  Child classes should implement
	 * this function if they need to perform any special logic when an object is moused over
	 *
	 * @param	Obj		the object that is currently under the mouse cursor
	 */
	virtual void OnMouseOver(UObject* Obj) {}

	/**
	 * Called when the user clicks something in the workspace that uses a special hit proxy (HLinkedObjProxySpecial),
	 * Hook for preventing AddToSelection/SetSelectedConnector from being called for the clicked element.
	 *
	 * @return	FALSE to prevent any selection methods from being called for the clicked element
	 */
	virtual UBOOL SpecialClick( INT NewX, INT NewY, INT SpecialIndex, FViewport* Viewport, UObject* ProxyObj ) { return 0; }

	/**
	 * Called when the user drags an object that uses a special hit proxy (HLinkedObjProxySpecial)
	 * (corresponding method for non-special objects is MoveSelectedObjects)
	 */
	virtual void SpecialDrag( INT DeltaX, INT DeltaY, INT NewX, INT NewY, INT SpecialIndex ) {}

	/* =======================================
		General methods and notifications
	   =====================================*/

	/**
	 * Child classes should implement this function to render the objects that are currently visible.
	 */
	virtual void DrawObjects(FViewport* Viewport, FCanvas* Canvas)=0;

	/**
	 * Called when the viewable region is changed, such as when the user pans or zooms the workspace.
	 */
	virtual void ViewPosChanged() {}

	/**
	 * Called when something happens in the linked object drawing that invalidates the property window's
	 * current values, such as selecting a new object.
	 * Child classes should implement this function to update any property windows which are being displayed.
	 */
	virtual void UpdatePropertyWindow() {}

	/**
	 * Called when one or more objects changed
	 */
	virtual void NotifyObjectsChanged() {}

	/**
	 * Called once the user begins a drag operation.  Child classes should implement this method
	 * if they the position of the selected objects is part of the object's state (and thus, should
	 * be recorded into the transaction buffer)
	 */
	virtual void BeginTransactionOnSelected() {}

	/**
	 * Called when the user releases the mouse button while a drag operation is active.
	 * Child classes should implement this only if they also implement BeginTransactionOnSelected.
	 */
	virtual void EndTransactionOnSelected() {}

	/* =======================================
		Selection methods and notifications
	   =====================================*/

	/**
	 * Empty the list of selected objects.
	 */
	virtual void EmptySelection() {}

	/**
	 * Add the specified object to the list of selected objects
	 */
	virtual void AddToSelection( UObject* Obj ) {}

	/**
	 * Remove the specified object from the list of selected objects.
	 */
	virtual void RemoveFromSelection( UObject* Obj ) {}

	/**
	 * Checks whether the specified object is currently selected.
	 * Child classes should implement this to check for their specific
	 * object class.
	 *
	 * @return	TRUE if the specified object is currently selected
	 */
	virtual UBOOL IsInSelection( UObject* Obj ) const { return FALSE; }

	/**
	 * Returns the number of selected objects
	 */
	virtual INT GetNumSelected() const { return 0; }

	/**
	 * Checks whether we have any objects selected.
	 */
	UBOOL HaveObjectsSelected() const { return GetNumSelected() > 0; }

	/**
	 * Called once the mouse is released after moving objects.
	 * Snaps the selected objects to the grid.
	 */
	virtual void PositionSelectedObjects() {}

	/**
	 * Called when the user right-clicks on an object connector in the viewport.
	 */
	virtual void OpenConnectorOptionsMenu() {}

	// Selection-related methods

	/**
	 * Called when the user clicks on an unselected link connector.
	 *
	 * @param	Connector	the link connector that was clicked on
	 */
	virtual void SetSelectedConnector( struct FLinkedObjectConnector& Connector ) {}

	/**
	 * Gets the position of the selected connector.
	 *
	 * @return	an FIntPoint that represents the position of the selected connector, or (0,0)
	 *			if no connectors are currently selected
	 */
	virtual FIntPoint GetSelectedConnLocation(FCanvas* Canvas) { return FIntPoint(0,0); }

	/**
	 * Adjusts the postion of the selected connector based on the Delta position passed in.
	 * Currently only variable, event, and output connectors can be moved. 
	 * 
	 * @param DeltaX	The amount to move the connector in X
	 * @param DeltaY	The amount to move the connector in Y	
	 */
	virtual void MoveSelectedConnLocation( INT DeltaX, INT DeltaY ) {}

	/**
	 * Sets the member variable on the selected connector struct so we can perform different calculations in the draw code
	 * 
	 * @param bMoving	True if the connector is moving
	 */
	virtual void SetSelectedConnectorMoving( UBOOL bMoving ) {}
	/**
	 * Gets the EConnectorHitProxyType for the currently selected connector
	 *
	 * @return	the type for the currently selected connector, or 0 if no connector is selected
	 */
	virtual INT GetSelectedConnectorType() { return 0; }

	/**
	 * Called when the user mouses over a linked object connector.
	 * Checks whether the specified connector should be highlighted (i.e. whether this connector is valid for linking)
	 */
	virtual UBOOL ShouldHighlightConnector(struct FLinkedObjectConnector& Connector) { return TRUE; }

	/**
	 * Gets the color to use for drawing the pending link connection.  Only called when the user
	 * is connecting a link to a link connector.
	 */
	virtual FColor GetMakingLinkColor() { return FColor(0,0,0); }

	/**
	 * Called when the user releases the mouse over a link connector during a link connection operation.
	 * Make a connection between selected connector and an object or another connector.
	 *
	 * @param	Connector	the connector corresponding to the endpoint of the new link
	 */
	virtual void MakeConnectionToConnector( struct FLinkedObjectConnector& Connector ) {}

	/**
	 * Called when the user releases the mouse over an object during a link connection operation.
	 * Makes a connection between the current selected connector and the specified object.
	 *
	 * @param	Obj		the object target corresponding to the endpoint of the new link connection
	 */
	virtual void MakeConnectionToObject( UObject* Obj ) {}

	/**
	 * Called when the user releases the mouse over a link connector and is holding the ALT key.
	 * Commonly used as a shortcut to breaking connections.
	 *
	 * @param	Connector	The connector that was ALT+clicked upon.
	 */
	virtual void AltClickConnector(struct FLinkedObjectConnector& Connector) {}

	/* =============
	    Bookmarks
	   ===========*/
	/**
	 * Called when the user attempts to set a bookmark via CTRL + # key.
	 *
	 * @param	InIndex		Index of the bookmark to set
	 */
	virtual void SetBookmark( UINT InIndex ) {}

	/**
	 * Called when the user attempts to check a bookmark.
	 *
	 * @param	InIndex		Index of the bookmark to check
	 */
	virtual UBOOL CheckBookmark( UINT InIndex ) { return FALSE; }

	/**
	 * Called when the user attempts to jump to a bookmark via a # key.
	 *
	 * @param	InIndex		Index of the bookmark to jump to
	 */
	virtual void JumpToBookmark( UINT InIndex ) {}

	/* ================================================
	    Navigation history methods and notifications
	   ==============================================*/

	/**
	 * Sets the user's navigation history back one entry, if possible.
	 */
	virtual void NavigationHistoryBack() {}

	/**
	 * Sets the user's navigation history forward one entry, if possible.
	 */
	virtual void NavigationHistoryForward() {}

	/**
	 * Jumps the user's navigation history to the entry at the specified index, if possible.
	 *
	 * @param	InIndex	Index of history entry to jump to
	 */
	virtual void NavigationHistoryJump( INT InIndex ) {}

	/**
	 * Add a new history data item to the user's navigation history, storing the current state
	 *
	 * @param	InHistoryString		The string that identifies the history data operation and will display in a navigation menu (CANNOT be empty)
	 */
	virtual void AddNewNavigationHistoryDataItem( FString InHistoryString ) {}

	/**
	 * Update the current history data item of the user's navigation history with any desired changes that have occurred since it was first added,
	 * such as camera updates, etc.
	 */
	virtual void UpdateCurrentNavigationHistoryData() {}

protected:
	/**
	 * Process a specified history data object by responding to its contents accordingly
	 *
	 * @param	InData	History data to process
	 *
	 * @return	TRUE if the navigation history data was successfully processed; FALSE otherwise
	 */
	virtual UBOOL ProcessNavigationHistoryData( const struct FLinkedObjEdNavigationHistoryData* InData ) { return FALSE; }

};

class FLinkedObjViewportClient : public FEditorLevelViewportClient
{
public:
	FLinkedObjViewportClient( FLinkedObjEdNotifyInterface* InEdInterface );

	virtual void Draw(FViewport* Viewport,FCanvas* Canvas);
	virtual UBOOL InputKey(FViewport* Viewport,INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f,UBOOL bGamepad=FALSE);
	virtual UBOOL InputAxis(FViewport* Viewport,INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime, UBOOL bGamepad=FALSE);
	virtual void MouseMove(FViewport* Viewport, INT X, INT Y);
	virtual void CapturedMouseMove(FViewport* InViewport, INT InMouseX, INT InMouseY);
	virtual void Tick(FLOAT DeltaSeconds);

	virtual EMouseCursor GetCursor(FViewport* Viewport,INT X,INT Y);

	/**
	 * Sets the cursor to be visible or not.  Meant to be called as the mouse moves around in "move canvas" mode (not just on button clicks)
	 */
	UBOOL UpdateCursorVisibility (void);
	/**
	 * Given that we're in "move canvas" mode, set the snap back visible mouse position to clamp to the viewport
	 */
	void UpdateMousePosition(void);
	/** Determines if the cursor should presently be visible
	 * @return - TRUE if the cursor should remain visible
	 */
	UBOOL ShouldCursorBeVisible (void);

	/** 
	 * See if cursor is in 'scroll' region around the edge, and if so, scroll the view automatically. 
	 * Returns the distance that the view was moved.
	 */
	FIntPoint DoScrollBorder(FLOAT DeltaSeconds);

	/**
	 * Sets whether or not the viewport should be invalidated in Tick().
	 */
	void SetRedrawInTick(UBOOL bInAlwaysDrawInTick);

	FLinkedObjEdNotifyInterface* EdInterface;

	FIntPoint Origin2D;

	/**
	 * If set non-zero, the viewport will automatically pan to DesiredOrigin2D before resetting to 0.
	 */
	FLOAT DesiredPanTime;
	FIntPoint DesiredOrigin2D;

	FLOAT Zoom2D;
	FLOAT MinZoom2D, MaxZoom2D;
	INT NewX, NewY; // Location for creating new object

	INT OldMouseX, OldMouseY;
	FVector2D BoxOrigin2D;
	INT BoxStartX, BoxStartY;
	INT BoxEndX, BoxEndY;
	INT DistanceDragged;
	FLOAT DeltaXFraction;	// Fractional delta X, which can add up when zoomed in
	FLOAT DeltaYFraction;	// Fractional delta Y, which can add up when zoomed in

	FVector2D ScrollAccum;

	UBOOL bTransactionBegun;
	UBOOL bMouseDown;

	UBOOL bMakingLine;
	
	/** Whether or not we are moving a ocnnector */
	UBOOL bMovingConnector;

	UBOOL bBoxSelecting;
	UBOOL bSpecialDrag;
	UBOOL bAllowScroll;

protected:

	/** Handle mouse over events */
	void OnMouseOver( INT X, INT Y );

	/** If TRUE, invalidate the viewport in Tick().  Used for mouseover support, etc. */
	UBOOL bAlwaysDrawInTick;

public:
	// For mouse over stuff.
	UObject* MouseOverObject;
	INT MouseOverConnType;
	INT MouseOverConnIndex;
	DOUBLE MouseOverTime;
	INT ToolTipDelayMS;

	INT SpecialIndex;
	
	/** Updates to the latest real time if the viewport real time flag is set. Useful when pausing material expressions*/
	FLOAT CachedRealTime;
};

/*-----------------------------------------------------------------------------
	WxLinkedObjVCHolder
-----------------------------------------------------------------------------*/

class WxLinkedObjVCHolder : public wxWindow
{
public:
	WxLinkedObjVCHolder( wxWindow* InParent, wxWindowID InID, FLinkedObjEdNotifyInterface* InEdInterface );
	~WxLinkedObjVCHolder();

	void OnSize( wxSizeEvent& In );

	FLinkedObjViewportClient* LinkedObjVC;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	FLinkedObjEdNavigationHistoryData
-----------------------------------------------------------------------------*/

/**
 * Structure designed for use within the linked object editor navigation history system. 
 * Stores information including the string to display in the navigation menu, as well as
 * the zoom/position for a particular history event.
 */
struct FLinkedObjEdNavigationHistoryData
{
	/** String to display in the navigation menu; CANNOT be empty */
	FString HistoryString;

	/** Zoom of the camera */
	FLOAT HistoryCameraZoom2D;

	/** X-position of the camera */
	INT HistoryCameraXPosition;

	/** Y-position of the camera */
	INT HistoryCameraYPosition;

	/**
	 * Construct a FLinkedObjEdNavigationHistoryData object
	 *
	 * @param	InHistoryString	String to set HistoryString to; Will display in the navigation menu; CANNOT be empty
	 */
	FLinkedObjEdNavigationHistoryData( FString InHistoryString );

	/**
	 * Destroy a FLinkedObjEdNavigationHistoryData object; Virtual as this serves as a base class
	 */
	virtual ~FLinkedObjEdNavigationHistoryData() {}

	/**
	 * Convenience function to quickly set the zoom and position values of the struct
	 *
	 * @param	InXPos		Value to set HistoryXPos to; Represents X-position of the camera at the time of the history event
	 * @param	InYPos		Value to set HistoryYPos to; Represents Y-position of the camera at the time of the history event
	 * @param	InZoom2D	Value to set HistoryZoom2D to; Represents zoom of the camera at the time of the history event
	 */
	void SetPositionAndZoomData( INT InXPos, INT InYPos, FLOAT InZoom2D );
};

/*-----------------------------------------------------------------------------
	FLinkedObjEdNavigationHistory
-----------------------------------------------------------------------------*/

/**
 * Class designed to store the user's navigation history while using a linked object
 * editor. The editor can use the interface to add new navigation data to the class,
 * as well as navigate through previously-saved data in a manner similar to a web
 * browser's history. Data added to the class is maintained internally and deleted
 * when it is no longer required. Outside code should *NOT* maintain references to
 * history data after inserting/processing it.
 */
class FLinkedObjEdNavigationHistory
{
public:

	/**
	 * Construct a FLinkedObjEdNavigationHistory object
	 */
	FLinkedObjEdNavigationHistory();

	/**
	 * Destroy a FLinkedObjEdNavigationHistory object
	 */
	~FLinkedObjEdNavigationHistory();

	/**
	 * Attach back, forward, and drop-down buttons to the specified toolbar
	 *
	 * @param	InParentToolBar			Toolbar to attach buttons to
	 * @param	bInsertSeparatorBefore	If TRUE, a separator will be placed on the toolbar in a position prior to the new buttons being added
	 * @param	bInsertSeparatorAfter	If TRUE, a separator will be placed on the toolbar in a position after the new buttons being added
	 * @param	InPosition				If specified, the position that the buttons will start being inserted at on the toolbar; If negative or not-specified,
	 *									the buttons will be placed at the end of the toolbar
	 */
	void AttachToToolBar( wxToolBar* InParentToolBar, UBOOL bInsertSeparatorBefore = FALSE, UBOOL bInsertSeparatorAfter = FALSE, INT InPosition = -1 );

	/**
	 * Add a new history data object to the navigation history at the current index, and make it the current item within the system. If new history 
	 * data is added while the user is not currently at the last history item, all history items following the insertion point are discarded, just
	 * as occurs with a web browser. If new history data is added while the system is currently at its maximum number of entries, the oldest entry is
	 * discarded first to make room. Once data is added to the class, it will be maintained internally and deleted when it is no longer required or
	 * upon destruction. Outside code should *NOT* maintain references to history data for this reason.
	 *
	 * @param	InData	History data object to add to the navigation history
	 */
	void AddHistoryNavigationData( FLinkedObjEdNavigationHistoryData* InData );

	/**
	 * Make the history item that precedes the current history item the new current history item if possible, and return a pointer to it.
	 *
	 * @return	Pointer to the current history item after the back operation has occurred; NULL if there are no navigation history items or
	 *			if the user is already at the first entry in the navigation history
	 */
	FLinkedObjEdNavigationHistoryData* Back();

	/**
	 * Make the history item that follows the current history item the new current history item if possible, and return a pointer to it.
	 *
	 * @return	Pointer to the current history item after the forward operation has occurred; NULL if there are no navigation history items
	 *			or if the user is already at the last entry in the navigation history
	 */
	FLinkedObjEdNavigationHistoryData* Forward();

	/**
	 * Make the history item specified by the provided index the current history item if possible, and return a pointer to it.
	 *
	 * @return	Pointer to the current history item after the jump operation has occurred; NULL if there are no navigation history items
	 */
	FLinkedObjEdNavigationHistoryData* JumpTo( INT InIndex );

	/**
	 * Clear all history items from the navigation history data
	 */
	void ClearHistory();

	/**
	 * Const accessor to the current navigation history data item
	 *
	 * @return	The current navigation history data item, if one exists; NULL otherwise
	 */
	const FLinkedObjEdNavigationHistoryData* GetCurrentNavigationHistoryData() const;

	/**
	 * Non-const accessor to the current navigation history data item
	 *
	 * @return	The current navigation history data item, if one exists; NULL otherwise
	 */
	FLinkedObjEdNavigationHistoryData* GetCurrentNavigationHistoryData();

	/**
	 * Forcibly remove the specified history item from the navigation system and set the current history item to the last item before a
	 * navigation operation if the removed item was the current item. This is used if an editor identifies a data item it has processed
	 * as containing invalid data (data refers to objects that were deleted, etc.). The editor can then remove the data from the navigation
	 * system, as it is no longer relevant.
	 *
	 * @param	DataToRemove	History object to remove from the navigation system
	 * @param	bWarnUser		If TRUE, a message box pops up informing the user that the data has been forcibly removed
	 */
	void ForceRemoveNavigationHistoryData( FLinkedObjEdNavigationHistoryData* DataToRemove, UBOOL bWarnUser );

	/**
	 * Update the back/forward buttons to be disabled/enabled, as appropriate. This method is usually forwarded a wxUpdateUIEvent
	 * from elsewhere, as the class is not an actual wxWidget-derived object and does not itself receive events automatically. 
	 *
	 * @param	InEvent	Update UI event specifying which button ID to update
	 */
	void UpdateUI( wxUpdateUIEvent& InEvent );

	wxMenu* HistoryMenu;						/** Menu of navigation history items displayed when clicking the HistoryListButton */

private:

	/** Maximum number of history entries to allow */
	static const INT MAX_HISTORY_ENTRIES = IDM_LinkedObjNavHistory_Item_Index_End - IDM_LinkedObjNavHistory_Item_Index_Start;

	WxBitmap BackB;								/** Bitmap for the back button */
	WxBitmap ForwardB;							/** Bitmap for the forward button */
	WxBitmap DownArrowB;						/** Bitmap for the pull-down button */
	WxMenuButton HistoryListButton;				/** Pull-down button, displays a menu of navigation history items */

	TArray<FLinkedObjEdNavigationHistoryData*> HistoryNavigationData;	/** Array of stored history navigation data */

	UBOOL bAttachedToToolBar;					/** Whether the class has been attached to a toolbar or not */
	INT CurNavHistoryIndex;						/** Index of the current history item in the array */
	INT NavHistoryIndexBeforeLastNavCommand;	/** Index of the current history item before the last navigation command; Used to restore back to a safe point */

	/**
	 * Copy constructor; Intentionally made private and left unimplemented to prevent use
	 */
	FLinkedObjEdNavigationHistory( const FLinkedObjEdNavigationHistory& );

	/**
	 * Assignment operator; Intentionally made private and left unimplemented to prevent use
	 */
	FLinkedObjEdNavigationHistory& operator=( const FLinkedObjEdNavigationHistory& );

	/**
	 * Internally update the menu that displays the navigation history strings
	 */
	void UpdateMenu();
};
/*-----------------------------------------------------------------------------
	WxLinkedObjEd
-----------------------------------------------------------------------------*/

class WxLinkedObjEd : public WxTrackableFrame, public FNotifyHook, public FLinkedObjEdNotifyInterface, public FSerializableObject, public FDockingParent
{
public:
	/** Default constructor for two-step dynamic window creation */
	WxLinkedObjEd( wxWindow* InParent, wxWindowID InID, const TCHAR* InWinName );
	virtual ~WxLinkedObjEd();

	/**
	 * This function is called when the window has been selected from within the ctrl + tab dialog.
	 */
	virtual void OnSelected();

    /**
     * Creates the controls for this window.  If bTreeControl is TRUE, the bottom portion of
	 * the screen is split with a property window on the left and a tree control on the right.
	 * If bTreeControl is FALSE, the bottom portion of the screen will contain only a property window.
	 *
	 * @param	bTreeControl	If TRUE, create a property window and tree control; If FALSE, create just a property window.
     */
	virtual void CreateControls( UBOOL bTreeControl );
	virtual void InitEditor()=0;

	/** 
	 * Saves Window Properties
	 */ 
	void SaveProperties();

	/**
	 * Loads Window Properties
	 */
	void LoadProperties();

	/**
	 * @return Returns the name of the inherited class, so we can generate .ini entries for all LinkedObjEd instances.
	 */
	virtual const TCHAR* GetConfigName() const = 0;

	void OnSize( wxSizeEvent& In );
	void RefreshViewport();

	// FLinkedObjEdNotifyInterface interface
	virtual void DrawObjects(FViewport* Viewport, FCanvas* Canvas);

	/**
	 * Sets the user's navigation history back one entry, if possible (and processes it).
	 */
	virtual void NavigationHistoryBack();

	/**
	 * Sets the user's navigation history forward one entry, if possible (and processes it).
	 */
	virtual void NavigationHistoryForward();

	/**
	 * Jumps the user's navigation history to the entry at the specified index, if possible (and processes it).
	 *
	 * @param	InIndex	Index of history entry to jump to
	 */
	virtual void NavigationHistoryJump( INT InIndex );

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

	/**
	 * Creates the tree control for this linked object editor.  Only called if TRUE is specified for bTreeControl
	 * in the constructor.
	 *
	 * @param	TreeParent	the window that should be the parent for the tree control
	 */
	virtual void CreateTreeControl( wxWindow* TreeParent );


	// FNotifyHook interface

	/**
	 * Called when the property window is destroyed.
	 *
	 * @param	Src		a pointer to the property window being destroyed
	 */
	virtual void NotifyDestroy( void* Src );

	/**
	 * Called when a property value has been changed, before the new value has been applied.
	 *
	 * @param	Src						a pointer the property window containing the property that was changed
	 * @param	PropertyAboutToChange	a pointer to the UProperty that is being changed
	 */
	virtual void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );

	/**
	 * Called when a property value has been changed, after the new value has been applied.
	 *
	 * @param	Src						a pointer the property window containing the property that was changed
	 * @param	PropertyThatChanged		a pointer to the UProperty that has been changed
	 */
	virtual void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );

	/**
	 * Called when the the propert window wants to send an exec command.  Seems to be currently unused.
	 */
	virtual void NotifyExec( void* Src, const TCHAR* Cmd );

	/**
	 * Used to serialize any UObjects contained that need to be to kept around.
	 *
	 * @param Ar The archive to serialize with
	 */
	virtual void Serialize(FArchive& Ar);

	WxPropertyWindowHost* PropertyWindow;

	/** Container window for the 'graph' viewport.  This is usually a dockable window that wraps LinkedObjVC. */
	WxLinkedObjVCHolder* GraphWindow;

	FLinkedObjViewportClient* LinkedObjVC;

	wxTreeCtrl* TreeControl;
	wxImageList* TreeImages;
	UTexture2D*	BackgroundTexture;
	FString WinNameString;

protected:
	FLinkedObjEdNavigationHistory NavHistory;	/** Stores the user's navigation history */

	/**
	 * Process a specified history data object by responding to its contents accordingly (here by adjusting the camera
	 * to the specified zoom and position)
	 *
	 * @param	InData	History data to process
	 *
	 * @return	TRUE if the navigation history data was successfully processed; FALSE otherwise
	 */
	virtual UBOOL ProcessNavigationHistoryData( const FLinkedObjEdNavigationHistoryData* InData );

	/**
	 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
	 *  @return A string representing a name to use for this docking parent.
	 */
	virtual const TCHAR* GetDockingParentName() const;

	/**
	 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
	 */
	virtual const INT GetDockingParentVersion() const;

private:

	/**
	 * Function called in response to the navigation history back button being pressed
	 *
	 * @param	InEvent	Event generated by wxWidgets in response to the back button being pressed
	 */
	void OnHistoryBackButton( wxCommandEvent& InEvent );

	/**
	* Function called in response to the navigation history forward button being pressed
	*
	* @param	InEvent	Event generated by wxWidgets in response to the forward button being pressed
	*/
	void OnHistoryForwardButton( wxCommandEvent& InEvent );

	/**
	* Function called in response to a navigation history pull-down menu item being selected
	*
	* @param	InEvent	Event generated by wxWidgets in response to a pull-down menu item being selected
	*/
	void OnHistoryPulldownMenu( wxCommandEvent& InEvent );

	/**
	* Function called in response to the wxWidgets update UI event for the back/forward buttons
	*
	* @param	InEvent	Event generated by wxWidgets to update the UI
	*/
	void OnUpdateUIForHistory( wxUpdateUIEvent& InEvent );

	// Note: The following macro changes the access specifier to protected
	DECLARE_EVENT_TABLE()
};

#endif	// __UNLINKEDOBJEDITOR_H__
