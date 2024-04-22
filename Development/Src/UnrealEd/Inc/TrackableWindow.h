/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __TRACKABLEWINDOW_H__
#define __TRACKABLEWINDOW_H__

class WxTrackableWindowBase;
class WxCtrlTabDialog;

/**
 * This structure represents an entry in a list of trackable windows.
 */
struct FTrackableEntry
{
	/** Pointer to a wxWindow */
	wxWindow* Window;
	/** Pointer to the WxTrackableWindowBase that owns the wxWindow */
	WxTrackableWindowBase* TrackableWindow;

	FTrackableEntry(){}
	FTrackableEntry(wxWindow* Win, WxTrackableWindowBase* WinTrackable) : Window(Win), TrackableWindow(WinTrackable){}

	UBOOL operator==(const FTrackableEntry& Entry) const
	{
		return Window == Entry.Window && TrackableWindow == Entry.TrackableWindow;
	}
};

/**
 * The base class for any window that is to be tracked for ctrl + tab functionality
 */
class WxTrackableWindowBase
{
public:
	virtual ~WxTrackableWindowBase();

	/**
	 * Called when the window has been selected in the ctrl + tab dialog box.
	 */
	virtual void OnSelected();

	/**
	 * Gets the global list of all trackable windows.
	 *
	 * @param	Windows	A reference to a TArray<FTrackableEntry> that will retrieve the global list of trackable windows.
	 */
	static void GetTrackableWindows(TArray<FTrackableEntry> &Windows);
	
	/**
	 * Handles a ctrl + tab event.
	 *
	 * @param	Parent			The parent window for the ctrl + tab dialog box. Usually this is the editor frame.
	 * @param	bIsShiftDown	TRUE if the shift key is currently being held down.
	 */
	static void HandleCtrlTab(wxWindow* Parent, UBOOL bIsShiftDown);

	/**
	* Is the CtrlTab dialog active
	*
	* @return	TRUE if the ctrl+tab dialog is active
	*/
	static UBOOL IsCtrlTabActive();

protected:
	/**
	 * Registers a window with the global list of trackable windows.
	 *
	 * @param	Window		The window to be added to the global list of trackable windows.
	 */
	static void RegisterWindow(const FTrackableEntry& Window);
	
	/**
	 * Unregisters a window with the global list of trackable windows.
	 *
	 * @param	Window		The window to be removed from the global list of trackable windows.
	 */
	static void UnRegisterWindow(const FTrackableEntry& Window);
	
	/**
	 * Makes a window the first entry in the global list of trackable windows.
	 *
	 * @param	Window		The window to make the first entry in the global list of trackable windows.
	 */
	static void MakeFirstEntry(const FTrackableEntry& Window);

private:
	/** The global list of trackable windows. */
	static TArray<FTrackableEntry> TrackableWindows;
	/** The global ctrl + tab dialog instance. */
	static WxCtrlTabDialog* WindowDialog;
};

/**
 * Overrides wxWindow to make it trackable.
 */
class WxTrackableWindow : public wxWindow, public WxTrackableWindowBase
{
public:
	DECLARE_DYNAMIC_CLASS(WxTrackableWindow);

	WxTrackableWindow();
	WxTrackableWindow(wxWindow *Parent, wxWindowID ID, const wxPoint& Pos = wxDefaultPosition, const wxSize& Size = wxDefaultSize, INT Style = 0, const wxString& Name = wxPanelNameStr);
	virtual ~WxTrackableWindow();

	/**
	 * Event handler for the EVT_SET_FOCUS event.
	 *
	 * @param	Event		Information about the event.
	 */
	void OnSetFocus(wxFocusEvent& Event);

	DECLARE_EVENT_TABLE();
};

/**
 * Overrides wxFrame to make it trackable.
 */
class WxTrackableFrame : public wxFrame, public WxTrackableWindowBase
{
public:
	DECLARE_DYNAMIC_CLASS(WxTrackableFrame);

	WxTrackableFrame();
	WxTrackableFrame(wxWindow *Parent, wxWindowID ID, const wxString& Title, const wxPoint& Pos = wxDefaultPosition, const wxSize& Size = wxDefaultSize, INT Style = wxDEFAULT_FRAME_STYLE, const wxString& Name = wxFrameNameStr);
	virtual ~WxTrackableFrame();

	/**
	 * Event handler for the EVT_ACTIVATE event.
	 *
	 * @param	Event		Information about the event.
	 */
	void OnActivate(wxActivateEvent& Event);

	DECLARE_EVENT_TABLE();
};

/**
 * Overrides wxDialog to make it trackable.
 */
class WxTrackableDialog : public wxDialog, public WxTrackableWindowBase
{
public:
	DECLARE_DYNAMIC_CLASS(WxTrackableDialog);

	WxTrackableDialog();
	WxTrackableDialog(wxWindow *Parent, wxWindowID ID, const wxString& Title, const wxPoint& Pos = wxDefaultPosition, const wxSize& Size = wxDefaultSize, INT Style = wxDEFAULT_DIALOG_STYLE, const wxString& Name = wxDialogNameStr);
	virtual ~WxTrackableDialog();

	/**
	 * This function is called when the WxTrackableDialog has been selected from within the ctrl + tab dialog.
	 */
	virtual void OnSelected();

	/**
	 * Event handler for the EVT_ACTIVATE event.
	 *
	 * @param	Event		Information about the event.
	 */
	void OnActivate(wxActivateEvent& Event);

	DECLARE_EVENT_TABLE();
};

/**
 * The ctrl + tab dialog box that displays a list of all trackable windows.
 */
class WxCtrlTabDialog : public wxDialog
{
public:
	WxCtrlTabDialog(wxWindow *Parent, UBOOL bIsShiftDown);

	/**
	 * Returns the selected trackable window or NULL if there wasn't one.
	 */
	WxTrackableWindowBase* GetSelection();

	/**
	 * Changes the currently selected trackable window by moving the selection forward.
	 */
	void IncrementSelection();

	/**
	 * Changes the currently selected trackable window by moving the selection backward.
	 */
	void DecrementSelection();


	/**
	 * Event handler for the EVT_PAINT event.
	 *
	 * @param	Event		Information about the event.
	 */
	void OnPaint(wxPaintEvent& Event);

	/**
	 * Event handler for the EVT_KEY_UP event.
	 *
	 * @param	Event		Information about the event.
	 */
	void OnKeyUp(wxKeyEvent& Event);

	/**
	 * Event handler for the EVT_KILL_FOCUS event.
	 *
	 * @param	Event		Information about the event.
	 */
	void OnLostFocus(wxFocusEvent& Event);

	/**
	 * Event handler for the EVT_KEY_DOWN event.
	 *
	 * @param	Event		Information about the event.
	 */
	void OnKeyDown(wxKeyEvent& Event);
	
	/**
	 * Event handler for the EVT_MOUSE_MOVE event.
	 *
	 * @param	Event		Information about the event.
	 */
	void OnMouseMove(wxMouseEvent& Event);

	/**
	 * Event handler for the EVT_LEFT_DOWN event.
	 *
	 * @param	Event		Information about the event.
	 */
	void OnLeftMouseDown(wxMouseEvent& Event);

	/**
	 * Event handler for the EVT_LEFT_UP event.
	 *
	 * @param	Event		Information about the event.
	 */
	void OnLeftMouseUp(wxMouseEvent& Event);

	DECLARE_EVENT_TABLE();

protected:
	/** Flags for controlling which collumn to get a dimension for. */
	enum EWindowColumn
	{
		WC_None = 0,
		WC_Left = 1,
		WC_Right = 1 << 1,
		WC_Both = WC_Left | WC_Right,
	};

	/** The size of the apdding around the edges of the ctrl + tab dialog box. */
	static const INT PADDING;
	/** The size of the space between the left and right columns. */
	static const INT CENTER_SPACE_SIZE;
	/** The padding to the height of each trackable window entry. */
	static const INT ENTRY_HEIGHT_PAD;

	/** The list of trackable windows to be displayed. */
	TArray<FTrackableEntry> TrackableWindows;

	/**
	 * Draws the background for a selected entry.
	 *
	 * @param	Rect	The rectangle area to draw the background in.
	 * @param	DC		The device context to use for drawing.
	 */
	void DrawSelectedBackground(wxRect Rect, wxDC &DC) const;
	
	/**
	 * Gets the bounding rectangles for all of the trackable window entries.
	 *
	 * @param	Rects	The list to be filled with the bounding rectangles.
	 * @param	DC		The device context to use for drawing. Needed to measure the size of the entries.
	 */
	void GetEntryBoundingRects(TArray<wxRect> &OutRects, wxDC &DC);

	/**
	 * Gets the width of the longest trackable window entry.
	 *
	 * @param	Rects			The list of bounding rectangles to check.
	 * @param	ColumnFlags		Flags specifying which columns to check for the longest width.
	 */
	INT GetLongestWidth(const TArray<wxRect> &Rects, const EWindowColumn ColumnFlags = WC_None) const;

	/**
	 * Gets the largest height of the left and right columns.
	 *
	 * @param	Rects	The list of bounding rectangles to use for calculating the height.
	 */
	INT GetTotalEntryHeight(const TArray<wxRect> &Rects) const;
	
	/**
	 * Gets the height of the ctrl + tab dialog box.
	 *
	 * @param	Rects	The list of bounding rectangles to use for calculating the height.
	 */
	INT CalculateWindowHeight(const TArray<wxRect> &Rects) const;

	/**
	 * Gets the width of the ctrl + tab dialog box.
	 *
	 * @param	Rects	The list of bounding rectangles to use for calculating the width.
	 */
	INT CalculateWindowWidth(const TArray<wxRect> &Rects) const;

	/**
	 * Returns the index of the item containing the supplied point.
	 *
	 * @param	Point	The point to check for containment.
	 */
	INT GetSelectionFromPoint(const wxPoint &Point) const;

private:
	/** Initialized to FALSE until the first EVT_PAINT event which causes the window to be resized and this variable set to TRUE. */
	UBOOL bHasBeenSized;
	/** The index of the currently selected trackable window. */
	INT CurrentlySelectedIndex;
	/** The list of bounding rectangles for all of the trackable windows. */
	TArray<wxRect> SelectionRects;

	/**
	 * Returns the string to display for a trackable window.
	 *
	 * @param	Window		The window to get the display string from.
	 */
	const wxString GetDisplayString(const wxWindow* Window) const;
};

#endif //__TRACKABLEWINDOW_H__
