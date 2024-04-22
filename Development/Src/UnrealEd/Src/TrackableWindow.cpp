/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "ResourceIDs.h"
#include "TrackableWindow.h"

TArray<FTrackableEntry> WxTrackableWindowBase::TrackableWindows;
WxCtrlTabDialog* WxTrackableWindowBase::WindowDialog = NULL;

WxTrackableWindowBase::~WxTrackableWindowBase()
{

}

/**
 * Registers a window with the global list of trackable windows.
 *
 * @param	Window		The window to be added to the global list of trackable windows.
 */
void WxTrackableWindowBase::RegisterWindow(const FTrackableEntry& Window)
{
	TrackableWindows.AddUniqueItem(Window);
}

/**
 * Unregisters a window with the global list of trackable windows.
 *
 * @param	Window		The window to be removed from the global list of trackable windows.
 */
void WxTrackableWindowBase::UnRegisterWindow(const FTrackableEntry& Window)
{
	TrackableWindows.RemoveItem(Window);
}

/**
 * Gets the global list of all trackable windows.
 *
 * @param	Windows	A reference to a TArray<FTrackableEntry> that will retrieve the global list of trackable windows.
 */
void WxTrackableWindowBase::GetTrackableWindows(TArray<FTrackableEntry> &Windows)
{
	for(INT WindowIndex = 0; WindowIndex < TrackableWindows.Num(); ++WindowIndex)
	{
		Windows.AddItem(TrackableWindows(WindowIndex));
	}
}

/**
 * Makes a window the first entry in the global list of trackable windows.
 *
 * @param	Window		The window to make the first entry in the global list of trackable windows.
 */
void WxTrackableWindowBase::MakeFirstEntry(const FTrackableEntry& Window)
{
	TrackableWindows.RemoveItem(Window);
	TrackableWindows.InsertItem(Window, 0);
}

/**
 * Handles a ctrl + tab event.
 *
 * @param	Parent			The parent window for the ctrl + tab dialog box. Usually this is the editor frame.
 * @param	bIsShiftDown	TRUE if the shift key is currently being held down.
 */
void WxTrackableWindowBase::HandleCtrlTab(wxWindow* Parent, UBOOL bIsShiftDown)
{
	if(WindowDialog == NULL)
	{
		WindowDialog = new WxCtrlTabDialog(Parent, bIsShiftDown);
		WindowDialog->ShowModal();

		WxTrackableWindowBase *SelectedWindow = WindowDialog->GetSelection();

		if(SelectedWindow)
		{
			SelectedWindow->OnSelected();
		}

		WindowDialog = NULL;
	}
	else
	{
		if(bIsShiftDown)
		{
			WindowDialog->DecrementSelection();
		}
		else
		{
			WindowDialog->IncrementSelection();
		}

		WindowDialog->Refresh();
	}
}

/**
* Is the CtrlTab dialog active
*
* @return	TRUE if the ctrl+tab dialog is active
*/
UBOOL WxTrackableWindowBase::IsCtrlTabActive()
{
	return ( WindowDialog != NULL );
}

/**
* Called when the window has been selected in the ctrl + tab dialog box.
*/
void WxTrackableWindowBase::OnSelected()
{

}

IMPLEMENT_DYNAMIC_CLASS(WxTrackableWindow, wxWindow);

BEGIN_EVENT_TABLE(WxTrackableWindow, wxWindow)
EVT_SET_FOCUS(WxTrackableWindow::OnSetFocus)
END_EVENT_TABLE()

WxTrackableWindow::WxTrackableWindow()
{
	WxTrackableWindowBase::RegisterWindow(FTrackableEntry(this, this));
}

WxTrackableWindow::WxTrackableWindow(wxWindow *Parent, wxWindowID ID, const wxPoint& Pos, const wxSize& Size, INT Style, const wxString& Name)
	: wxWindow(Parent, ID, Pos, Size, Style, Name)
{
	WxTrackableWindowBase::RegisterWindow(FTrackableEntry(this, this));
}

WxTrackableWindow::~WxTrackableWindow()
{
	WxTrackableWindowBase::UnRegisterWindow(FTrackableEntry(this, this));
}

/**
 * Event handler for the EVT_SET_FOCUS event.
 *
 * @param	Event		Information about the event.
 */
void WxTrackableWindow::OnSetFocus(wxFocusEvent& Event)
{
	WxTrackableWindowBase::MakeFirstEntry(FTrackableEntry(this, this));
	Event.Skip();
}



IMPLEMENT_DYNAMIC_CLASS(WxTrackableFrame, wxFrame);

BEGIN_EVENT_TABLE(WxTrackableFrame, wxFrame)
EVT_ACTIVATE(WxTrackableFrame::OnActivate)
END_EVENT_TABLE()

WxTrackableFrame::WxTrackableFrame()
{
	WxTrackableWindowBase::RegisterWindow(FTrackableEntry(this, this));
}

WxTrackableFrame::WxTrackableFrame(wxWindow *Parent, wxWindowID ID, const wxString& Title, const wxPoint& Pos, const wxSize& Size, INT Style, const wxString& Name)
	: wxFrame(Parent, ID, Title, Pos, Size, Style, Name)
{
	WxTrackableWindowBase::RegisterWindow(FTrackableEntry(this, this));
}

WxTrackableFrame::~WxTrackableFrame()
{
	WxTrackableWindowBase::UnRegisterWindow(FTrackableEntry(this, this));
}

/**
 * Event handler for the EVT_ACTIVATE event.
 *
 * @param	Event		Information about the event.
 */
void WxTrackableFrame::OnActivate(wxActivateEvent& Event)
{
	if(Event.GetActive())
	{
		WxTrackableWindowBase::MakeFirstEntry(FTrackableEntry(this, this));
	}

	Event.Skip();
}


IMPLEMENT_DYNAMIC_CLASS(WxTrackableDialog, wxDialog);

BEGIN_EVENT_TABLE(WxTrackableDialog, wxDialog)
EVT_ACTIVATE(WxTrackableDialog::OnActivate)
END_EVENT_TABLE()

WxTrackableDialog::WxTrackableDialog()
{
	WxTrackableWindowBase::RegisterWindow(FTrackableEntry(this, this));
}

WxTrackableDialog::WxTrackableDialog(wxWindow *Parent, wxWindowID ID, const wxString& Title, const wxPoint& Pos, const wxSize& Size, INT Style, const wxString& Name)
	: wxDialog(Parent, ID, Title, Pos, Size, Style, Name)
{
	WxTrackableWindowBase::RegisterWindow(FTrackableEntry(this, this));
}

WxTrackableDialog::~WxTrackableDialog()
{
	WxTrackableWindowBase::UnRegisterWindow(FTrackableEntry(this, this));
}

/**
 * This function is called when the WxTrackableDialog has been selected from within the ctrl + tab dialog.
 */
void WxTrackableDialog::OnSelected()
{
	Raise();
}

/**
 * Event handler for the EVT_ACTIVATE event.
 *
 * @param	Event		Information about the event.
 */
void WxTrackableDialog::OnActivate(wxActivateEvent& Event)
{
	if(Event.GetActive())
	{
		WxTrackableWindowBase::MakeFirstEntry(FTrackableEntry(this, this));
	}
	
	Event.Skip();
}

BEGIN_EVENT_TABLE(WxCtrlTabDialog, wxDialog)
EVT_PAINT(WxCtrlTabDialog::OnPaint)
EVT_KEY_UP(WxCtrlTabDialog::OnKeyUp)
EVT_KEY_DOWN(WxCtrlTabDialog::OnKeyDown)
EVT_MOTION(WxCtrlTabDialog::OnMouseMove)
EVT_LEFT_DOWN(WxCtrlTabDialog::OnLeftMouseDown)
EVT_LEFT_UP(WxCtrlTabDialog::OnLeftMouseUp)
END_EVENT_TABLE()

/** The size of the apdding around the edges of the ctrl + tab dialog box. */
const INT WxCtrlTabDialog::PADDING = 10;

/** The size of the space between the left and right columns. */
const INT WxCtrlTabDialog::CENTER_SPACE_SIZE = 40;

/** The padding to the height of each trackable window entry. */
const INT WxCtrlTabDialog::ENTRY_HEIGHT_PAD = 4;

WxCtrlTabDialog::WxCtrlTabDialog(wxWindow *Parent, UBOOL bIsShiftDown)
	: wxDialog(Parent, -1, TEXT(""), wxPoint(100, 100), wxSize(1, 1), wxSIMPLE_BORDER | wxFRAME_NO_TASKBAR | wxWANTS_CHARS), bHasBeenSized(FALSE), CurrentlySelectedIndex(0)
{
	WxTrackableWindowBase::GetTrackableWindows(TrackableWindows);

	if(TrackableWindows.Num() == 0)
	{
		this->Destroy();
	}
	else
	{
		if(bIsShiftDown)
		{
			CurrentlySelectedIndex = TrackableWindows.Num() > 1 ? TrackableWindows.Num() - 1 : 0;
		}
		else
		{
			CurrentlySelectedIndex = TrackableWindows.Num() > 1 ? 1 : 0;
		}
	}
}

/**
 * Event handler for the EVT_KEY_UP event.
 *
 * @param	Event		Information about the event.
 */
void WxCtrlTabDialog::OnKeyUp(wxKeyEvent& Event)
{
	if(!Event.ControlDown())
	{
		this->Destroy();
	}
}

/**
 * Event handler for the EVT_KEY_DOWN event.
 *
 * @param	Event		Information about the event.
 */
void WxCtrlTabDialog::OnKeyDown(wxKeyEvent& Event)
{
	if(Event.GetKeyCode() == WXK_TAB)
	{
		if(Event.ShiftDown())
		{
			DecrementSelection();
		}
		else
		{
			IncrementSelection();
		}

		Refresh();
	}
}

/**
 * Event handler for the EVT_KILL_FOCUS event.
 *
 * @param	Event		Information about the event.
 */
void WxCtrlTabDialog::OnLostFocus(wxFocusEvent& Event)
{
	this->Destroy();
}

/**
 * Event handler for the EVT_PAINT event.
 *
 * @param	Event		Information about the event.
 */
void WxCtrlTabDialog::OnPaint(wxPaintEvent &Event)
{
	wxBufferedPaintDC DC(this);

	const wxRect ClientRect = GetClientRect();

	// clear the background
	DC.SetBackground(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE), wxSOLID));
	DC.Clear();

	// Get the bounding rectangles for all entries
	SelectionRects.Empty();
	GetEntryBoundingRects(SelectionRects, DC);

	// Resize the window if it hasn't been sized yet, this can only be done in OnPaint() because that's the only time you can have a valid DC without wxWidgets complaining :(
	if(!bHasBeenSized)
	{
		bHasBeenSized = TRUE;

		wxWindow *Parent = GetParent();

		const INT Width = CalculateWindowWidth(SelectionRects);
		const INT Height = CalculateWindowHeight(SelectionRects);

		// If the ctrl + tab dialog has a parent then center it within the parent's client rectangle.
		if(Parent)
		{
			wxPoint Pt = Parent->GetPosition();
			const wxRect ParentRect = Parent->GetClientRect();
			SetSize(Pt.x + (ParentRect.width / 2 - Width / 2), Pt.y + (ParentRect.height / 2  - Height / 2), Width, Height);
		}
		else
		{
			SetSize(100, 100, Width, Height);
		}

		// redraw!
		Refresh();
		
		// Don't propagate the event and immediately return
		Event.Skip(false);
		return;
	}

	const INT LongestWidthLeft = GetLongestWidth(SelectionRects, WC_Left);
	const INT LongestWidthRight = GetLongestWidth(SelectionRects, WC_Right);
	const INT RightColX = LongestWidthLeft + CENTER_SPACE_SIZE;
	
	INT LeftColSize = TrackableWindows.Num() / 2;
	// If there is an odd # of items we want the extra item to appear in the left column so increment by 1
	if(TrackableWindows.Num() & 1)
	{
		++LeftColSize;
	}

	INT OffsetY = PADDING;
	INT WindowIndex = 0;

	// Draw left column
	for(; WindowIndex < LeftColSize; OffsetY += SelectionRects(WindowIndex).GetHeight() + ENTRY_HEIGHT_PAD, ++WindowIndex)
	{
		if(CurrentlySelectedIndex == WindowIndex)
		{
			DrawSelectedBackground(wxRect(PADDING, OffsetY, SelectionRects(WindowIndex).GetWidth(), SelectionRects(WindowIndex).GetHeight()), DC);
		}
		DC.DrawText(GetDisplayString(TrackableWindows(WindowIndex).Window), PADDING, OffsetY);
	}

	OffsetY = PADDING;

	// Draw right column
	for(; WindowIndex < TrackableWindows.Num(); OffsetY += SelectionRects(WindowIndex).GetHeight() + ENTRY_HEIGHT_PAD, ++WindowIndex)
	{
		if(CurrentlySelectedIndex == WindowIndex)
		{
			DrawSelectedBackground(wxRect(RightColX, OffsetY, SelectionRects(WindowIndex).GetWidth(), SelectionRects(WindowIndex).GetHeight()), DC);
		}
		DC.DrawText(GetDisplayString(TrackableWindows(WindowIndex).Window), RightColX, OffsetY);
	}

	Event.Skip(false);
}

/**
 * Gets the bounding rectangles for all of the trackable window entries.
 *
 * @param	Rects	The list to be filled with the bounding rectangles.
 * @param	DC		The device context to use for drawing. Needed to measure the size of the entries.
 */
void WxCtrlTabDialog::GetEntryBoundingRects(TArray<wxRect> &OutRects, wxDC &DC)
{
	wxCoord X = 0;
	wxCoord Y = 0;

	for(INT WindowIndex = 0; WindowIndex < TrackableWindows.Num(); ++WindowIndex)
	{
		DC.GetTextExtent(GetDisplayString(TrackableWindows(WindowIndex).Window), &X, &Y);
		OutRects.AddItem(wxRect(0, 0, X, Y));
	}
}

/**
 * Gets the width of the longest trackable window entry.
 *
 * @param	Rects			The list of bounding rectangles to check.
 * @param	ColumnFlags		Flags specifying which columns to check for the longest width.
 */
INT WxCtrlTabDialog::GetLongestWidth(const TArray<wxRect> &Rects, const EWindowColumn ColumnFlags) const
{
	INT Width = 0;
	INT RectIndex = 0;
	INT MaxRects = Rects.Num();

	if((ColumnFlags & WC_Both) != WC_Both && ColumnFlags != WC_None)
	{
		if(ColumnFlags & WC_Left)
		{
			MaxRects /= 2;

			if(Rects.Num() & 1)
			{
				++MaxRects;
			}
		}
		else if(ColumnFlags & WC_Right)
		{
			RectIndex = Rects.Num() / 2;

			if(Rects.Num() & 1)
			{
				++RectIndex;
			}
		}
	}

	for(; RectIndex < MaxRects; ++RectIndex)
	{
		Width = Width < Rects(RectIndex).GetWidth() ? Rects(RectIndex).GetWidth() : Width;
	}

	return Width;
}

/**
 * Gets the largest height of the left and right columns.
 *
 * @param	Rects	The list of bounding rectangles to use for calculating the height.
 */
INT WxCtrlTabDialog::GetTotalEntryHeight(const TArray<wxRect> &Rects) const
{
	INT Height = 0;
	for(INT RectIndex = 0; RectIndex < Rects.Num(); RectIndex += 2)
	{
		Height += Rects(RectIndex).GetHeight() + ENTRY_HEIGHT_PAD;
	}

	// Get rid of the trailing height padding
	Height -= ENTRY_HEIGHT_PAD;

	return Height;
}

/**
 * Gets the height of the ctrl + tab dialog box.
 *
 * @param	Rects	The list of bounding rectangles to use for calculating the height.
 */
INT WxCtrlTabDialog::CalculateWindowHeight(const TArray<wxRect> &Rects) const
{
	return GetTotalEntryHeight(Rects) + PADDING + PADDING;
}

/**
 * Gets the width of the ctrl + tab dialog box.
 *
 * @param	Rects	The list of bounding rectangles to use for calculating the width.
 */
INT WxCtrlTabDialog::CalculateWindowWidth(const TArray<wxRect> &Rects) const
{
	return GetLongestWidth(Rects, WC_Left) + GetLongestWidth(Rects, WC_Right) + PADDING * 2 + CENTER_SPACE_SIZE;
}

/**
 * Draws the background for a selected entry.
 *
 * @param	Rect	The rectangle area to draw the background in.
 * @param	DC		The device context to use for drawing.
 */
void WxCtrlTabDialog::DrawSelectedBackground(wxRect Rect, wxDC &DC) const
{
	wxBrush OldBrush = DC.GetBrush();
	wxPen OldPen = DC.GetPen();

	Rect.x -= 4;
	Rect.width += 4 + PADDING;
	Rect.y -= ENTRY_HEIGHT_PAD / 2;
	Rect.height += ENTRY_HEIGHT_PAD / 2 + 2;

	DC.SetBrush(wxBrush(wxColour(193, 210, 238))); // light blue interior
	DC.SetPen(wxPen(wxColour(49, 106, 197))); // with a dark blue border

	DC.DrawRectangle(Rect);

	DC.SetBrush(OldBrush);
	DC.SetPen(OldPen);
}

/**
 * Changes the currently selected trackable window by moving the selection forward.
 */
void WxCtrlTabDialog::IncrementSelection()
{
	++CurrentlySelectedIndex;

	if(CurrentlySelectedIndex >= TrackableWindows.Num())
	{
		CurrentlySelectedIndex = 0;
	}
}

/**
 * Changes the currently selected trackable window by moving the selection backward.
 */
void WxCtrlTabDialog::DecrementSelection()
{
	--CurrentlySelectedIndex;

	if(CurrentlySelectedIndex < 0)
	{
		CurrentlySelectedIndex = TrackableWindows.Num() - 1;
	}
}

/**
 * Returns the selected trackable window or NULL if there wasn't one.
 */
WxTrackableWindowBase* WxCtrlTabDialog::GetSelection()
{
	if(CurrentlySelectedIndex >= 0 && CurrentlySelectedIndex < TrackableWindows.Num())
	{
		return TrackableWindows(CurrentlySelectedIndex).TrackableWindow;
	}

	return NULL;
}

/**
 * Returns the string to display for a trackable window.
 *
 * @param	Window		The window to get the display string from.
 */
const wxString WxCtrlTabDialog::GetDisplayString(const wxWindow* Window) const
{
	return Window->GetLabel();
}

/**
 * Event handler for the EVT_MOUSE_MOVE event.
 *
 * @param	Event		Information about the event.
 */
void WxCtrlTabDialog::OnMouseMove(wxMouseEvent& Event)
{
	INT CurSelection = GetSelectionFromPoint(Event.GetPosition());

	if(CurSelection >= 0)
	{
		this->SetCursor(wxCURSOR_HAND);
	}
	else
	{
		this->SetCursor(wxNullCursor);
	}
}

/**
 * Returns the index of the item containing the supplied point.
 *
 * @param	Point	The point to check for containment.
 */
INT WxCtrlTabDialog::GetSelectionFromPoint(const wxPoint &Point) const
{
	const INT LongestWidthLeft = GetLongestWidth(SelectionRects, WC_Left);
	const INT LongestWidthRight = GetLongestWidth(SelectionRects, WC_Right);
	const INT RightColX = LongestWidthLeft + CENTER_SPACE_SIZE;

	INT LeftColSize = TrackableWindows.Num() / 2;
	if(TrackableWindows.Num() & 1)
	{
		++LeftColSize;
	}

	INT OffsetY = PADDING;
	INT WindowIndex = 0;

	// Check the left column
	for(; WindowIndex < LeftColSize; OffsetY += SelectionRects(WindowIndex).GetHeight() + ENTRY_HEIGHT_PAD, ++WindowIndex)
	{
		wxRect LeftCol(PADDING, OffsetY, SelectionRects(WindowIndex).GetWidth(), SelectionRects(WindowIndex).GetHeight());

		if(LeftCol.Contains(Point))
		{
			return WindowIndex;
		}
	}

	OffsetY = PADDING;

	// Check the right column
	for(; WindowIndex < TrackableWindows.Num(); OffsetY += SelectionRects(WindowIndex).GetHeight() + ENTRY_HEIGHT_PAD, ++WindowIndex)
	{
		wxRect RightCol(RightColX, OffsetY, SelectionRects(WindowIndex).GetWidth(), SelectionRects(WindowIndex).GetHeight());

		if(RightCol.Contains(Point))
		{
			return WindowIndex;
		}
	}

	// Did not click on an item so return -1
	return -1;
}

/**
 * Event handler for the EVT_LEFT_DOWN event.
 *
 * @param	Event		Information about the event.
 */
void WxCtrlTabDialog::OnLeftMouseDown(wxMouseEvent& Event)
{
	INT CurSelection = GetSelectionFromPoint(Event.GetPosition());

	if(CurSelection > -1)
	{
		CurrentlySelectedIndex = CurSelection;
		Refresh();
	}
}

/**
 * Event handler for the EVT_LEFT_UP event.
 *
 * @param	Event		Information about the event.
 */
void WxCtrlTabDialog::OnLeftMouseUp(wxMouseEvent& Event)
{
	INT CurSelection = GetSelectionFromPoint(Event.GetPosition());

	if(CurSelection == CurrentlySelectedIndex)
	{
		Destroy();
	}
}
