/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __BUILDINGSTATSBROWSER_H__
#define __BUILDINGSTATSBROWSER_H__


/**
 * Building stats browser class.
 */
class WxBuildingStatsBrowser : public WxBrowser
{
	DECLARE_DYNAMIC_CLASS(WxBuildingStatsBrowser);

protected:
	/** List control used for displaying stats. */
	WxListView*		ListControl;
	/** Array of column headers, used during CSV export. */
	TArray<FString> ColumnHeaders;

public:
	/** Current sort order (-1 or 1) */
	static INT CurrentSortOrder[BSBC_MAX];
	/** Primary index/ column to sort by */
	static INT PrimarySortIndex;
	/** Secondary index/ column to sort by */
	static INT SecondarySortIndex;

	/**
	 * Forwards the call to our base class to create the window relationship.
	 * Creates any internally used windows after that
	 *
	 * @param DockID the unique id to associate with this dockable window
	 * @param FriendlyName the friendly name to assign to this window
	 * @param Parent the parent of this window (should be a Notebook)
	 */
	virtual void Create(INT DockID,const TCHAR* FriendlyName,wxWindow* Parent);

	/**
	 * Called when the browser is getting activated (becoming the visible
	 * window in it's dockable frame).
	 */
	void Activated(void);

	/**
	 * Tells the browser to update itself
	 */
	void Update(void);

	/**
	 * Returns the key to use when looking up values
	 */
	virtual const TCHAR* GetLocalizationKey(void) const
	{
		return TEXT("BuildingStatsBrowser");
	}

protected:

	/**
	 * Dumps current stats to CVS file.
	 *
	 * @param NumRows	Number of rows to dump
	 */
	void DumpToCSV( INT NumRows );

	/**
	 * Inserts a column into the control.
	 *
	 * @param	ColumnId		Id of the column to insert
	 * @param	ColumnHeader	Header/ description of the column.
	 * @param	Format			The alignment of the column text.
	 */
	void InsertColumn( EBuildingStatsBrowserColumns ColumnId, const TCHAR* ColumnHeader, int Format = wxLIST_FORMAT_LEFT );

	/**
	 * Updates the primitives list with new data
	 *
	 * @param bResizeColumns	Whether or not to resize the columns after updating data.
	 */
	void UpdateList(UBOOL bResizeColumns=TRUE);

	/**
	 * Handler for EVT_SIZE events.
	 *
	 * @param In the command that was sent
	 */
	void OnSize( wxSizeEvent& In );

	/**
	 * Handler for column click events
	 *
	 * @param In the command that was sent
	 */
	void OnColumnClick( wxListEvent& In );

	/**
	 * Handler for column right click events
	 *
	 * @param In the command that was sent
	 */
	void OnColumnRightClick( wxListEvent& In );

	/**
	 * Handler for item activation (double click) event
	 *
	 * @param In the command that was sent
	 */
	void OnItemActivated( wxListEvent& In );

	/**
	 * Sets auto column width. Needs to be called after resizing as well.
	 */
	void SetAutoColumnWidth();


	DECLARE_EVENT_TABLE();
};

#endif // __BUILDINGSTATSBROWSER_H__
