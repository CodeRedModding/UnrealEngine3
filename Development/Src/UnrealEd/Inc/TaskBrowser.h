/*=============================================================================
	TaskBrowser.h: Browser window for working with a task database
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if _MSC_VER
#pragma once
#endif

#ifndef __TaskBrowser_h__
#define __TaskBrowser_h__


#include "TaskDatabaseDefs.h"
#include "TaskDataManager.h"



/**
 * Task browser
 */
class WxTaskBrowser :
	public WxBrowser,
	public FTaskDataGUIInterface
{

	DECLARE_DYNAMIC_CLASS( WxTaskBrowser );

public:

	/** WxTaskBrowser constructor */
	WxTaskBrowser();

	/** WxTaskBrowser destructor */
	virtual ~WxTaskBrowser();


	/**
	 * Forwards the call to our base class to create the window relationship.
	 * Creates any internally used windows after that
	 *
	 * @param	DockID			The unique id to associate with this dockable window
	 * @param	FriendlyName	The friendly name to assign to this window
	 * @param	Parent			The parent of this window (should be a Notebook)
	 */
	virtual void Create( INT DockID, const TCHAR* FriendlyName, wxWindow* Parent );

	/** Tells the browser manager whether or not this browser can be cloned */
	virtual UBOOL IsClonable()
	{
		// Only allow one instance of a task browser window.  We only want one database connection open at a time!
		return FALSE;
	}

	/** Tells the browser to update itself */
	virtual void Update();

	/** Returns the key to use when looking up values */
	virtual const TCHAR* GetLocalizationKey() const
	{
		return TEXT( "TaskBrowser_Caption" );
	}


protected:

	/**
	 * Refresh all or part of the user interface
	 *
	 * @param	Options		Bitfield that describes which parts of the GUI to refresh at minimum
	 */
	void RefreshGUI( const ETaskBrowserGUIRefreshOptions::Type Options );

	/** Generates a list of currently selected task numbers */
	void QuerySelectedTaskNumbers( TArray< UINT >& OutSelectedTaskNumbers, UBOOL bOnlyOpenTasks );

	/** Static: Wx callback to sort task list items */
	static int wxCALLBACK TaskListItemSortCallback( UPTRINT InItem1, UPTRINT InItem2, UPTRINT InSortData );

	/** Handler for EVT_SIZE events. */
	void OnSize( wxSizeEvent& In );

	/** Called when the 'Mark as Fixed' button is clicked */
	void OnFixButtonClicked( wxCommandEvent& In );

	/** Called when the refresh button is clicked */
	void OnRefreshButtonClicked( wxCommandEvent& In );

	/** Called when a new filter is selected in the choice box */
	void OnDBFilterChoiceSelected( wxCommandEvent& In );

	/** Called when a display filter check box is clicked */
	void OnDisplayFilterCheckBoxClicked( wxCommandEvent& In );

	/** Called when a column button is clicked on the task list */
	void OnTaskListColumnButtonClicked( wxListEvent& In );

	/** Called when an item in the task list is selected */
	void OnTaskListItemSelected( wxListEvent& In );

	/** Called when an item in the task list is double-clicked */
	void OnTaskListItemActivated( wxListEvent& In );

	/** Called when the 'Connect' button is clicked */
	void OnConnectButtonClicked( wxCommandEvent& In );

	/** Called when the 'Settings' button is clicked */
	void OnSettingsButtonClicked( wxCommandEvent& In );

	/** Called when a text URL is clicked on in a text control */
	void OnURLLaunched( wxTextUrlEvent& In );

	/** FTaskDataGUIInterface Callback: Called when the GUI should be refreshed */
	virtual void Callback_RefreshGUI( const ETaskBrowserGUIRefreshOptions::Type Options );


	// WxWidgets event table
	DECLARE_EVENT_TABLE();


protected:

	/** Task data manager object */
	FTaskDataManager* TaskDataManager;

	/** Child window object, loaded from .xrc data */
	wxPanel* TaskBrowserPanel;

	/** Splitter window that divides the task list (top) from the task description (bottom) */
	wxSplitterWindow* SplitterWindow;

	/** List control used for displaying the list of tasks */
	wxListCtrl* TaskListControl;

	/** Text control for displaying the description of the selected task */
	wxTextCtrl* TaskDescriptionControl;

	/** Cached array of filter names that always matches the visible GUI */
	TArray< FString > FilterNames;

	/** Cached array of task entries that always matches the visible contents of task list control */
	TArray< FTaskDatabaseEntry > TaskEntries;

	/** The column currently being used for sorting */
	INT TaskListSortColumn;

	/** Denotes ascending/descending sort order */
	UBOOL bTaskListSortAscending;

};




#endif // __TaskBrowser_h__


