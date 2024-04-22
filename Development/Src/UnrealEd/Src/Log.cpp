/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Log.h"

/**
 * Handles adding a cell (or string of cells) to the list. If the additions
 * go beyond the maximum specified, the oldest ones are removed
 *
 * @param Cell the new set of cells to append to our container
 */
void WxBoundedHtmlContainerCell::AppendCell(wxHtmlCell* Cell)
{
	check(Cell && "Can't add a NULL cell");

	// Init our vars that will track how much is being added
	DWORD NumCellsAdded;

	// Figure out how many cells are being added
	wxHtmlCell* tmpCell;
	for ( tmpCell = Cell, NumCellsAdded = 1; tmpCell->GetNext() != NULL; tmpCell = tmpCell->GetNext(), NumCellsAdded++ );

	// If we are attempting to add more than we can hold, delete the first ones
	tmpCell = Cell;
	while ( Cell && NumCellsAdded >= MaxCellCount )
	{
		// Snip it off
		Cell = Cell->GetNext();
		// And keep track of our count
		NumCellsAdded--;
	}

	if ( tmpCell != Cell )
	{
		RemoveCell(tmpCell, Cell);
	}

	// If the addition will cross our max, delete cells until we are valid
	tmpCell = GetFirstChild();
	while (tmpCell && (NumCellsAdded + CurrentCellCount) >= MaxCellCount)
	{
		// Snip it off
		tmpCell = tmpCell->GetNext();
		// And keep track of our count
		CurrentCellCount--;
	}

	if ( tmpCell != GetFirstChild() )
	{
		RemoveCell(GetFirstChild(), tmpCell);
	}

	// Update our count and tell the cell it is owned
	CurrentCellCount += NumCellsAdded;
	InsertCell(Cell);
}


/**
 * Safely removes one or more cells from this container.
 *
 * @param	CellToRemove	the first cell to remove
 * @param	StopCell		the first cell in CellToRemove's linked list of cells which should not be removed.  if not specified, only one cell will be removed
 *
 * @return	the number of cells that were actually removed.
 */
INT WxBoundedHtmlContainerCell::RemoveCell( wxHtmlCell* CellToRemove, wxHtmlCell* StopCell/*=NULL*/ )
{
	INT Result=0;
	if ( CellToRemove != NULL )
	{
		if ( StopCell == NULL )
		{
			StopCell = CellToRemove->GetNext();
		}
		if ( CellToRemove == GetFirstChild() )
		{
			m_Cells = StopCell;
		}
		if ( CellToRemove == GetLastChild() )
		{
			m_LastCell = NULL;;
		}

		while ( CellToRemove != StopCell && CellToRemove != NULL )
		{
			// Grab the head of the list
			wxHtmlCell* tmp = CellToRemove;

			// Move to the next one in the list
			CellToRemove = CellToRemove->GetNext();

			// Snip it off
			tmp->SetNext(NULL);
			// Now free it
			delete tmp;

			Result++;
		}
	}

	return Result;
}

/**
 * Constructor that creates the bounded cell container and puts that in
 * the html window's cell list
 *
 * @param InMaxCellsToDisplay the number of lines to display
 * @param InParent the parent of this window
 * @param InID the ID to assign to the new window
 */
WxBoundedHtmlWindow::WxBoundedHtmlWindow(DWORD InMaxCellsToDisplay,
	wxWindow* InParent,wxWindowID InID) : wxHtmlWindow(InParent,InID)
{
	// Remove any existing container
	if (m_Cell != NULL)
	{
		delete m_Cell;
		m_Cell = NULL;
	}
	// Create the container with the specified maximum 
	ContainerCell = new WxBoundedHtmlContainerCell(InMaxCellsToDisplay,NULL);
	check(ContainerCell != NULL);
	// Replace their container with this one
	m_Cell = ContainerCell;
}

/*-----------------------------------------------------------------------------
	WxHtmlWindow
-----------------------------------------------------------------------------*/
BEGIN_EVENT_TABLE( WxHtmlWindow, WxBoundedHtmlWindow )
	EVT_SIZE( WxHtmlWindow::OnSize )
END_EVENT_TABLE()

WxHtmlWindow::WxHtmlWindow(DWORD InMaxCellsToDisplay,wxWindow* InParent,wxWindowID InID)
: WxBoundedHtmlWindow( InMaxCellsToDisplay, InParent, InID )
{
}

void WxHtmlWindow::AppendLine( const FString& InLine )
{
	{
		wxClientDC dc(this);
		dc.SetMapMode(wxMM_TEXT);
		m_Parser->SetDC(&dc);

		wxHtmlContainerCell* c2 = (wxHtmlContainerCell*)m_Parser->Parse(*InLine);
		// Add to the bounded list so that old lines can be culled
		ContainerCell->AppendCell(c2);
	}
	CreateLayout();
	ScrollToBottom();
	Refresh();
}

inline void WxHtmlWindow::ScrollToBottom()
{
	INT x, y;
	GetVirtualSize( &x, &y );
	Scroll(0, y);
}

void WxHtmlWindow::OnSize( wxSizeEvent& In )
{
	// parent implementation of OnSize calls CreateLayout(), which will clear the current scrollbar positions.  so before we call the parent version
	// save the current scrollbar positions so we can restore them afterwards.
	INT CurrentScrollX=0, CurrentScrollY=0;
	GetViewStart(&CurrentScrollX, &CurrentScrollY);

	WxBoundedHtmlWindow::OnSize(In);

	// wxScrollingPanel sets Skip(true) on the event which triggers another [redundant] call to the base version of OnSize
	// so here we unset the skip flag
	In.Skip(false);

	// finally restore the original scroll positions.
	Scroll(CurrentScrollX, CurrentScrollY);
}

/*-----------------------------------------------------------------------------
	WxRichTextEditControl
-----------------------------------------------------------------------------*/
#define LOG_WINDOW_TIMER_ID		100

IMPLEMENT_DYNAMIC_CLASS(WxRichTextEditControl, wxRichTextCtrl);

BEGIN_EVENT_TABLE( WxRichTextEditControl, wxRichTextCtrl )
	EVT_CONTEXT_MENU(WxRichTextEditControl::OnContextMenu)
	EVT_MENU(wxID_CLEAR, WxRichTextEditControl::OnClear)
	EVT_UPDATE_UI(wxID_CLEAR, WxRichTextEditControl::OnUpdateClear)
	EVT_IDLE(WxRichTextEditControl::OnIdle)
END_EVENT_TABLE()

WxRichTextEditControl::WxRichTextEditControl()
: MaxLines(0), bLayoutNeeded(FALSE), bFreezeDeferredLayouts(FALSE)
{
}

WxRichTextEditControl::WxRichTextEditControl( wxWindow* InParent, wxWindowID InID /* = -1 */, long Style )
: wxRichTextCtrl(InParent, InID, wxEmptyString, wxDefaultPosition, wxSize(200, 200), Style)
, MaxLines(0), bLayoutNeeded(FALSE), bFreezeDeferredLayouts(FALSE)
{
}

WXLRESULT WxRichTextEditControl::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    // we must handle clipboard events before calling MSWWindowProc, otherwise
    // the event would be handled twice if there's a handler for it in user
    // code:
#if WIN32
    switch ( nMsg )
    {
        case WM_CUT:
        case WM_COPY:
        case WM_PASTE:
            if ( HandleClipboardEvent(nMsg) )
                return 0;
            break;
    }

    WXLRESULT lRc = wxTextCtrlBase::MSWWindowProc(nMsg, wParam, lParam);

    switch ( nMsg )
    {
        case WM_GETDLGCODE:
            {
                // we always want the chars and the arrows: the arrows for
                // navigation and the chars because we want Ctrl-C to work even
                // in a read only control
                long lDlgCode = DLGC_WANTCHARS | DLGC_WANTARROWS;

                if ( IsEditable() )
                {
                    // we may have several different cases:
                    // 1. normal: both TAB and ENTER are used for navigation
                    // 2. ctrl wants TAB for itself: ENTER is used to pass to
                    //    the next control in the dialog
                    // 3. ctrl wants ENTER for itself: TAB is used for dialog
                    //    navigation
                    // 4. ctrl wants both TAB and ENTER: Ctrl-ENTER is used to
                    //    go to the next control (we need some way to do it)

                    // multiline controls should always get ENTER for themselves
                    if ( HasFlag(wxTE_PROCESS_ENTER) || HasFlag(wxRE_MULTILINE) )
                        lDlgCode |= DLGC_WANTMESSAGE;

                    if ( HasFlag(wxTE_PROCESS_TAB) )
                        lDlgCode |= DLGC_WANTTAB;

                    lRc |= lDlgCode;
                }
                else // !editable
                {
                    // NB: use "=", not "|=" as the base class version returns
                    //     the same flags is this state as usual (i.e.
                    //     including DLGC_WANTMESSAGE). This is strange (how
                    //     does it work in the native Win32 apps?) but for now
                    //     live with it.
                    lRc = lDlgCode;
                }
            }
            break;
	}

	return lRc;
#else
	return 0;
#endif
}

/** Sets the maximum number of lines that are allowed in this control */
void WxRichTextEditControl::SetMaxLines( INT InMaxLines )
{
	if ( MaxLines != InMaxLines )
	{
		MaxLines = InMaxLines;
// 		if ( MaxLines > 0 )
// 		{
// 			while ( GetNumberOfLines() > MaxLines )
// 			{
// 				RemoveLine(0);
// 			}
// 		}
	}
}

void WxRichTextEditControl::AppendLine( const FString& InLine )
{
	//@fixme ronp - for now disable logging when playing PIE
	if ( !GIsPlayInEditorWorld )
	{
		if ( !bLayoutNeeded )
		{
			bLayoutNeeded = TRUE;
			if ( !IsFrozen() )
			{
				Freeze();
			}
		}
		const bool bShouldAutoScroll = GetCaretPosition() == GetLastPosition() - 1;

	// must use a while loop in-case the user included \n in the string itself
// 	if ( MaxLines > 0 && )
// 	{
// 		while ( GetNumberOfLines() > MaxLines )
// 		{
// 			RemoveLine(0);
// 		}
// 	}

		const long CaretPosition = GetCaretPosition();
		AppendText(*(InLine + LINE_TERMINATOR));

		if ( bShouldAutoScroll )
		{
			SetCaretPosition(GetLastPosition() - 1, IsCaretAtLineStart());
			ShowPosition(GetCaretPosition());
		}
		else
		{
			SetCaretPosition(CaretPosition, IsCaretAtLineStart());
		}
	}
}

/** Removes the line at the specified line number */
void WxRichTextEditControl::RemoveLine( INT LineNumber )
{
	if ( !bLayoutNeeded )
	{
		bLayoutNeeded = TRUE;
		if ( !IsFrozen() )
		{
			Freeze();
		}
	}

	wxRichTextBuffer* buffer = &GetBuffer();
	wxRichTextParagraph* LineToRemove = buffer->GetParagraphAtLine(LineNumber);
	if ( LineToRemove )
	{
		wxRichTextAction* action = new wxRichTextAction(NULL, _("Delete"), wxRICHTEXT_DELETE, buffer, this);

		action->SetPosition(GetCaretPosition());
		action->SetRange(LineToRemove->GetRange());

		long nextCaretPos = GetCaretPosition();
		if ( nextCaretPos >= LineToRemove->GetRange().GetStart() )
		{
			nextCaretPos += LineToRemove->GetRange().GetLength();
		}
		buffer->SubmitAction(action);

		SetCaretPosition(nextCaretPos);
	}
}

/**
 * Removes all lines which bring the line count above the configured max lines...
 */
void WxRichTextEditControl::RemoveLineOverflow()
{
	INT ExtraLines = GetNumberOfLines() - MaxLines;
	if ( MaxLines > 0 && ExtraLines > 0 )
	{
		wxRichTextBuffer* buffer = &GetBuffer();

		wxRichTextParagraph* FirstLineToRemove = buffer->GetParagraphAtLine(0);
		wxRichTextParagraph* LastLineToRemove = buffer->GetParagraphAtLine(ExtraLines - 1);
		if ( FirstLineToRemove != NULL && LastLineToRemove != NULL )
		{
			wxRichTextRange RangeToRemove(FirstLineToRemove->GetRange().GetStart(), LastLineToRemove->GetRange().GetEnd());

			wxRichTextAction* action = new wxRichTextAction(NULL, _("Delete"), wxRICHTEXT_DELETE, buffer, this);

			action->SetPosition(GetCaretPosition());
			action->SetRange(RangeToRemove);

			long nextCaretPos = GetCaretPosition();
			if ( nextCaretPos >= RangeToRemove.GetStart() )
			{
				nextCaretPos = Min(nextCaretPos + RangeToRemove.GetLength(), GetLastPosition() - 1);
			}
			buffer->SubmitAction(action);
			SetInsertionPoint(nextCaretPos);
		}
	}
}

/**
 * Wrapper for refreshing the contents of the control, provided enough time has passed.
 *
 * @return	TRUE if a layout was performed.
 */
UBOOL WxRichTextEditControl::AttemptDeferredLayout()
{
	UBOOL bResult = FALSE;

	if ( bLayoutNeeded && IsShownOnScreen())
	{
		if ( !bFreezeDeferredLayouts && GetClientSize().x > 0 )
		{
			const bool bAutoScroll = GetCaretPosition() == GetLastPosition() - 1;
			RemoveLineOverflow();

			bLayoutNeeded = FALSE;
			bResult = TRUE;

			if ( IsFrozen() )
			{
				Thaw();
				if ( bAutoScroll || GetCaretPosition() == GetLastPosition() - 1 )
				{
					SetCaretPosition(GetLastPosition() - 1, IsCaretAtLineStart());
					ShowPosition(GetCaretPosition());
				}
			}
			else
			{
				if ( GetBuffer().GetDirty() )
				{
					LayoutContent();
				}
				else
				{
					SetupScrollbars();
				}

				Refresh(false);
			}
		}
	}

	return bResult;
}

/** Event handler for timer callbacks */
void WxRichTextEditControl::OnIdle( wxIdleEvent& event )
{
	AttemptDeferredLayout();
}

void WxRichTextEditControl::OnContextMenu(wxContextMenuEvent& event)
{
	if (event.GetEventObject() != this)
	{
		event.Skip();
		return;
	}

	wxMenu ContextMenu;

	ContextMenu.Append(wxID_COPY, _("&Copy"));
	ContextMenu.AppendSeparator();
	ContextMenu.Append(wxID_SELECTALL, _("Select &All"));
	ContextMenu.Append(wxID_CLEAR, _("Cl&ear"));

	FTrackPopupMenu tpm(this, &ContextMenu);
	tpm.Show();
}

void WxRichTextEditControl::OnClear(wxCommandEvent& WXUNUSED(event))
{
	Clear();
}

void WxRichTextEditControl::OnUpdateClear(wxUpdateUIEvent& event)
{
	event.Enable( GetNumberOfLines() > 0 );
}

/*-----------------------------------------------------------------------------
	WxLogWindow
-----------------------------------------------------------------------------*/

#define USING_HTML_CTRL 0

IMPLEMENT_DYNAMIC_CLASS(WxLogWindow,wxPanel);

BEGIN_EVENT_TABLE( WxLogWindow, wxPanel )
	EVT_SIZE( WxLogWindow::OnSize )
	EVT_TEXT_ENTER( XRCID("IDCB_COMMAND"), WxLogWindow::OnExecCommand )
END_EVENT_TABLE()

WxLogWindow::WxLogWindow( wxWindow* InParent )
	: wxPanel( InParent, -1 )
{
	Panel = (wxPanel*)wxXmlResource::Get()->LoadPanel( this, TEXT("ID_LOGWINDOW") );
	check( Panel != NULL );
	Panel->Fit();

	GLog->AddOutputDevice( this );

	INT MaxLogLines = 100;
	// Get the number of lines of log history from the editor ini
	GConfig->GetInt(TEXT("LogWindow"),TEXT("MaxNumberOfLogLines"),MaxLogLines,
		GEditorIni);

#if USING_HTML_CTRL

	RichTextWindow = NULL;
	HTMLWindow = new WxHtmlWindow( (DWORD)MaxLogLines, Panel, ID_LOG );

	wxXmlResource::Get()->AttachUnknownControl( wxT("ID_LOG"), HTMLWindow );
	HTMLWindow->SetWindowStyleFlag( wxSUNKEN_BORDER );

	HTMLWindow->SetWindowStyle( HTMLWindow->GetWindowStyle() );

#else
	HTMLWindow = NULL;
	RichTextWindow = wxDynamicCast( FindWindow( XRCID( "ID_LOG" ) ), WxRichTextEditControl );
	check(RichTextWindow);

	// don't need undo history
	RichTextWindow->BeginSuppressUndo();
	RichTextWindow->SetMaxLines(MaxLogLines);
#endif

	CommandCombo = wxDynamicCast( FindWindow( XRCID( "IDCB_COMMAND" ) ), wxComboBox );
	check( CommandCombo != NULL );

	CommandCombo->SetWindowStyle(CommandCombo->GetWindowStyle() | wxTE_PROCESS_ENTER);
}

WxLogWindow::~WxLogWindow()
{
	GLog->RemoveOutputDevice( this );
}

void WxLogWindow::Serialize( const TCHAR* V, EName Event )
{
	if( Event == NAME_Title )
	{
		SetLabel( V );
		return;
	}
	else if ( Event == NAME_Color )
	{
		// Skip color events...
	}
	else if( IsShown() )
	{
		// Create a formatted string based on the event type
#if USING_HTML_CTRL
		FString Wk;
		FString FormattedText = V;

		FormattedText.ReplaceInline(TEXT("<"), TEXT("&lt;"));
		FormattedText.ReplaceInline(TEXT(">"), TEXT("&gt;"));
		FormattedText.ReplaceInline(TEXT("\n"), TEXT("<br>"));
		FormattedText.ReplaceInline(TEXT("\t"), TEXT("&nbsp;&nbsp;&nbsp;&nbsp;"));
		FormattedText.ReplaceInline(TEXT(" "), TEXT("&nbsp;"));

		switch( Event )
		{
			case NAME_Cmd:
				Wk += TEXT("<B>");
				break;

			case NAME_Error:
				Wk += TEXT("<B>");
				Wk += TEXT("<FONT COLOR=\"#FF0000\">");
				break;

			case NAME_Warning:
				Wk += TEXT("<FONT COLOR=\"#FF0000\">");
				break;
		}

		Wk += FName(Event).ToString() + TEXT(": ") + FormattedText;
		switch( Event )
		{
			case NAME_Cmd:
				Wk += TEXT("</B>");
				break;

			case NAME_Error:
				Wk += TEXT("</FONT>");
				Wk += TEXT("</B>");
				break;

			case NAME_Warning:
				Wk += TEXT("</FONT>");
				break;
		}

		//Wk += TEXT("<BR>");

		HTMLWindow->AppendLine( Wk );
#else
		switch( Event )
		{
		case NAME_Cmd:
			RichTextWindow->BeginBold();
			break;

		case NAME_Error:
			RichTextWindow->BeginBold();
			RichTextWindow->BeginTextColour(wxColour(255, 0, 0));
			break;

		case NAME_Warning:
			RichTextWindow->BeginTextColour(wxColour(255, 0, 0));
			break;
		}

		RichTextWindow->AppendLine( FName(Event).ToString() + TEXT(": ") + V );

		switch( Event )
		{
		case NAME_Cmd:
			RichTextWindow->EndBold();
			break;

		case NAME_Error:
			RichTextWindow->EndTextColour();
			RichTextWindow->EndBold();
			break;

		case NAME_Warning:
			RichTextWindow->EndTextColour();
			break;
		}
#endif
	}
}

void WxLogWindow::OnSize( wxSizeEvent& In )
{
	if( Panel )
	{
		const wxPoint pt = GetClientAreaOrigin();
		wxRect rc = GetClientRect();
		rc.y -= pt.y;

		Panel->SetSize( rc );
	}
}

void WxLogWindow::OnExecCommand( wxCommandEvent& In )
{
	ExecCommand(); 
}

/**
 * Takes the text in the combobox and execs it.
 */

void WxLogWindow::ExecCommand()
{
	const wxString& String = CommandCombo->GetValue();
	const TCHAR* Cmd = String;

	if( Cmd && appStrlen(Cmd) )
	{
		// Add the command to the combobox if it isn't already there

		if( CommandCombo->FindString( Cmd ) == -1 )
		{
			CommandCombo->Append( Cmd );
		}

		// Send the exec to the engine

		GEditor->Exec( Cmd );
	}
}
