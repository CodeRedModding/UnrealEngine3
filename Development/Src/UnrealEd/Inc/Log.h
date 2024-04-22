/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Log.h: Logging windows for displaying engine log info
 */

#ifndef __LOG_H__
#define __LOG_H__

/**
 * This version of the container cell only keeps a specified number of cells
 * in it's list.
 */
class WxBoundedHtmlContainerCell : public wxHtmlContainerCell
{
	/**
	 * The number of cells to have in the list before popping off stale ones
	 */
	DWORD MaxCellCount;
	/**
	 * The current size of the cell list
	 */
	DWORD CurrentCellCount;

public:
	/**
	 * Constructor. Sets the max cell count to the specified value
	 *
	 * @param InMaxCellCount the max cells to have before popping them off
	 */
	WxBoundedHtmlContainerCell(DWORD InMaxCellCount,wxHtmlContainerCell* Parent) :
		MaxCellCount(InMaxCellCount), CurrentCellCount(0),
		wxHtmlContainerCell(Parent)
	{
	}

	/**
	 * Handles adding a cell (or string of cells) to the list. If the additions
	 * go beyond the maximum specified, the oldest ones are removed
	 *
	 * @param Cell the new set of cells to append to our container
	 */
    void AppendCell(wxHtmlCell* Cell);

	/**
	 * Safely removes the specified cell.
	 *
	 * @param	CellToRemove	the first cell to remove
	 * @param	StopCell		the first cell in CellToRemove's linked list of cells which should not be removed.  if not specified, only one cell will be removed
	 *
	 * @return	the number of cells that were actually removed.
	 */
	INT RemoveCell( wxHtmlCell* CellToRemove, wxHtmlCell* StopCell=NULL );
};

/**
 * This window class handles displaying the last N number of HTML lines, where
 * N is configurable.
 */
class WxBoundedHtmlWindow : public wxHtmlWindow
{
protected:
	/**
	 * The container that only holds N number of cells before purging the oldest
	 */
	WxBoundedHtmlContainerCell* ContainerCell;

public:
	/**
	 * Constructor that creates the bounded cell container and puts that in
	 * the html window's cell list
	 *
	 * @param InMaxCellsToDisplay the number of lines to display
	 * @param InParent the parent of this window
	 * @param InID the ID to assign to the new window
	 */
	WxBoundedHtmlWindow(DWORD InMaxCellsToDisplay,wxWindow* InParent,
		wxWindowID InID);
};

/*-----------------------------------------------------------------------------
	WxHtmlWindow
-----------------------------------------------------------------------------*/

class WxHtmlWindow : public WxBoundedHtmlWindow
{
public:
	WxHtmlWindow( DWORD InMaxCellsToDisplay, wxWindow* InParent, wxWindowID InID = -1 );

	void AppendLine( const FString& InLine );

protected:
	void ScrollToBottom();
	void OnSize( wxSizeEvent& In );

	DECLARE_EVENT_TABLE();
};

class WxRichTextEditControl : public wxRichTextCtrl
{
	DECLARE_DYNAMIC_CLASS(WxRichTextEditControl)

public:
	WxRichTextEditControl();
	WxRichTextEditControl( wxWindow* InParent, wxWindowID InID = -1, long Style=wxVSCROLL|wxHSCROLL|wxBORDER_NONE|wxWANTS_CHARS|wxRE_MULTILINE );

	/** Sets the maximum number of lines that are allowed in this control */
	void SetMaxLines( INT InMaxLines );

	/**
	 * Adds a line of text to the end of the control.
	 */
	void AppendLine( const FString& InLine );

	/**
	 * Removes a line of text from the control.
	 */
	void RemoveLine( INT LineNumber );

	/**
	 * Removes all lines which bring the line count above the configured max lines...
	 */
	void RemoveLineOverflow();

	// intercept WM_GETDLGCODE
	virtual WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);


	class FScopedBatchLogHelper
	{
		WxRichTextEditControl* TextControl;

	public:
		FScopedBatchLogHelper( WxRichTextEditControl* InTextCtrl )
		: TextControl(InTextCtrl)
		{
			checkSlow(TextControl);
			TextControl->bFreezeDeferredLayouts = TRUE;
		}

		~FScopedBatchLogHelper()
		{
			TextControl->bFreezeDeferredLayouts = FALSE;
			TextControl->AttemptDeferredLayout();
		}
	};

protected:
	/**
	 * Wrapper for refreshing the contents of the control, provided enough time has passed.
	 *
	 * @return	TRUE if a layout was performed.
	 */
	UBOOL AttemptDeferredLayout();

private:
	/** Maximum number of lines to allow in this control */
	INT MaxLines;

	/** TRUE if we've added text since the last time LayoutContent was called */
	UBOOL bLayoutNeeded;

	/** TRUE to prevent AttemptDeferredLayout from unfreezing the control */
	UBOOL bFreezeDeferredLayouts;

	/** Wx Event Handlers */
	void OnIdle( wxIdleEvent& event );

	void OnContextMenu(wxContextMenuEvent& event);
	void OnClear(wxCommandEvent& event);
	void OnUpdateClear(wxUpdateUIEvent& event);

	DECLARE_EVENT_TABLE();
};

/*-----------------------------------------------------------------------------
	WxLogWindow
-----------------------------------------------------------------------------*/

class WxLogWindow : public wxPanel, public FOutputDevice
{
public:
	DECLARE_DYNAMIC_CLASS(WxLogWindow);

	WxLogWindow()
	{}
	WxLogWindow( wxWindow* InParent );
	virtual ~WxLogWindow();

	wxPanel* Panel;
	WxHtmlWindow* HTMLWindow; 
	WxRichTextEditControl*		RichTextWindow;
	wxComboBox* CommandCombo;

	virtual void Serialize( const TCHAR* V, EName Event );
	void ExecCommand();

	void OnSize( wxSizeEvent& In );
	void OnExecCommand( wxCommandEvent& In );

	DECLARE_EVENT_TABLE();
};

#endif // __LOG_H__
