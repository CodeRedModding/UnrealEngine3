/////////////////////////////////////////////////////////////////////////////
// Name:        wx/event.h
// Purpose:     Event classes
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: event.h,v 1.158.2.1 2002/10/30 19:55:06 RR Exp $
// Copyright:   (c) wxWindows team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_EVENTH__
#define _WX_EVENTH__

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "event.h"
#endif

#include "wx/defs.h"
#include "wx/object.h"
#include "wx/clntdata.h"

#if wxUSE_GUI
    #include "wx/gdicmn.h"
    #include "wx/cursor.h"
#endif

#include "wx/thread.h"

// ----------------------------------------------------------------------------
// forward declarations
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxList;

#if wxUSE_GUI
    class WXDLLEXPORT wxDC;
    class WXDLLEXPORT wxMenu;
    class WXDLLEXPORT wxWindow;
#endif // wxUSE_GUI

// ----------------------------------------------------------------------------
// Event types
// ----------------------------------------------------------------------------

typedef int wxEventType;

// in previous versions of wxWindows the event types used to be constants
// which created difficulties with custom/user event types definition
//
// starting from wxWindows 2.4 the event types are now dynamically assigned
// using wxNewEventType() which solves this problem, however at price of
// several incompatibilities:
//
//  a) event table macros declaration changed, it now uses wxEventTableEntry
//     ctor instead of initialisation from an agregate - the macro
//     DECLARE_EVENT_TABLE_ENTRY may be used to write code which can compile
//     with all versions of wxWindows
//
//  b) event types can't be used as switch() cases as they're not really
//     constant any more - there is no magic solution here, you just have to
//     change the switch()es to if()s
//
// if these are real problems for you, define WXWIN_COMPATIBILITY_EVENT_TYPES
// to get 100% old behaviour, however you won't be able to use the libraries
// using the new dynamic event type allocation in such case, so avoid it if
// possible.

#if WXWIN_COMPATIBILITY_EVENT_TYPES

#define DECLARE_EVENT_TABLE_ENTRY(type, id, idLast, fn, obj) \
    { type, id, idLast, fn, obj }

#define BEGIN_DECLARE_EVENT_TYPES() enum {
#define END_DECLARE_EVENT_TYPES() };
#define DECLARE_EVENT_TYPE(name, value) name = wxEVT_FIRST + value,
#define DECLARE_LOCAL_EVENT_TYPE(name, value) name = wxEVT_USER_FIRST + value,
#define DEFINE_EVENT_TYPE(name)
#define DEFINE_LOCAL_EVENT_TYPE(name)


#else // !WXWIN_COMPATIBILITY_EVENT_TYPES

#define DECLARE_EVENT_TABLE_ENTRY(type, id, idLast, fn, obj) \
    wxEventTableEntry(type, id, idLast, fn, obj)

#define BEGIN_DECLARE_EVENT_TYPES()
#define END_DECLARE_EVENT_TYPES()
#define DECLARE_EVENT_TYPE(name, value) \
    extern const wxEventType WXDLLEXPORT name;
#define DECLARE_LOCAL_EVENT_TYPE(name, value) extern const wxEventType name;
#define DECLARE_EXPORTED_LOCAL_EVENT_TYPE(usergoo, name, value) extern const wxEventType usergoo name;
#define DEFINE_EVENT_TYPE(name) const wxEventType name = wxNewEventType();
#define DEFINE_LOCAL_EVENT_TYPE(name) const wxEventType name = wxNewEventType();

// generate a new unique event type
extern WXDLLEXPORT wxEventType wxNewEventType();

#endif // WXWIN_COMPATIBILITY_EVENT_TYPES/!WXWIN_COMPATIBILITY_EVENT_TYPES

BEGIN_DECLARE_EVENT_TYPES()

#if WXWIN_COMPATIBILITY_EVENT_TYPES
    wxEVT_NULL = 0,
    wxEVT_FIRST = 10000,
    wxEVT_USER_FIRST = wxEVT_FIRST + 2000,
#else // !WXWIN_COMPATIBILITY_EVENT_TYPES
    // it is important to still have these as constants to avoid
    // initialization order related problems
    DECLARE_EVENT_TYPE(wxEVT_NULL, 0)
    const wxEventType wxEVT_FIRST = 10000;
    const wxEventType wxEVT_USER_FIRST = wxEVT_FIRST + 2000;
#endif // WXWIN_COMPATIBILITY_EVENT_TYPES/!WXWIN_COMPATIBILITY_EVENT_TYPES

    DECLARE_EVENT_TYPE(wxEVT_COMMAND_BUTTON_CLICKED, 1)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_CHECKBOX_CLICKED, 2)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_CHOICE_SELECTED, 3)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_LISTBOX_SELECTED, 4)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_LISTBOX_DOUBLECLICKED, 5)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_CHECKLISTBOX_TOGGLED, 6)
    // now they are in wx/textctrl.h
#if WXWIN_COMPATIBILITY_EVENT_TYPES
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_TEXT_UPDATED, 7)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_TEXT_ENTER, 8)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_TEXT_URL, 13)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_TEXT_MAXLEN, 14)
#endif // WXWIN_COMPATIBILITY_EVENT_TYPES
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_MENU_SELECTED, 9)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_SLIDER_UPDATED, 10)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_RADIOBOX_SELECTED, 11)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_RADIOBUTTON_SELECTED, 12)

    // wxEVT_COMMAND_SCROLLBAR_UPDATED is now obsolete since we use
    // wxEVT_SCROLL... events

    DECLARE_EVENT_TYPE(wxEVT_COMMAND_SCROLLBAR_UPDATED, 13)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_VLBOX_SELECTED, 14)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_COMBOBOX_SELECTED, 15)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_TOOL_RCLICKED, 16)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_TOOL_ENTER, 17)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_SPINCTRL_UPDATED, 18)

        // Sockets and timers send events, too
    DECLARE_EVENT_TYPE(wxEVT_SOCKET, 50)
    DECLARE_EVENT_TYPE(wxEVT_TIMER , 80)

        // Mouse event types
    DECLARE_EVENT_TYPE(wxEVT_LEFT_DOWN, 100)
    DECLARE_EVENT_TYPE(wxEVT_LEFT_UP, 101)
    DECLARE_EVENT_TYPE(wxEVT_MIDDLE_DOWN, 102)
    DECLARE_EVENT_TYPE(wxEVT_MIDDLE_UP, 103)
    DECLARE_EVENT_TYPE(wxEVT_RIGHT_DOWN, 104)
    DECLARE_EVENT_TYPE(wxEVT_RIGHT_UP, 105)
    DECLARE_EVENT_TYPE(wxEVT_MOTION, 106)
    DECLARE_EVENT_TYPE(wxEVT_ENTER_WINDOW, 107)
    DECLARE_EVENT_TYPE(wxEVT_LEAVE_WINDOW, 108)
    DECLARE_EVENT_TYPE(wxEVT_LEFT_DCLICK, 109)
    DECLARE_EVENT_TYPE(wxEVT_MIDDLE_DCLICK, 110)
    DECLARE_EVENT_TYPE(wxEVT_RIGHT_DCLICK, 111)
    DECLARE_EVENT_TYPE(wxEVT_SET_FOCUS, 112)
    DECLARE_EVENT_TYPE(wxEVT_KILL_FOCUS, 113)
    DECLARE_EVENT_TYPE(wxEVT_CHILD_FOCUS, 114)
    DECLARE_EVENT_TYPE(wxEVT_MOUSEWHEEL, 115)

        // Non-client mouse events
    DECLARE_EVENT_TYPE(wxEVT_NC_LEFT_DOWN, 200)
    DECLARE_EVENT_TYPE(wxEVT_NC_LEFT_UP, 201)
    DECLARE_EVENT_TYPE(wxEVT_NC_MIDDLE_DOWN, 202)
    DECLARE_EVENT_TYPE(wxEVT_NC_MIDDLE_UP, 203)
    DECLARE_EVENT_TYPE(wxEVT_NC_RIGHT_DOWN, 204)
    DECLARE_EVENT_TYPE(wxEVT_NC_RIGHT_UP, 205)
    DECLARE_EVENT_TYPE(wxEVT_NC_MOTION, 206)
    DECLARE_EVENT_TYPE(wxEVT_NC_ENTER_WINDOW, 207)
    DECLARE_EVENT_TYPE(wxEVT_NC_LEAVE_WINDOW, 208)
    DECLARE_EVENT_TYPE(wxEVT_NC_LEFT_DCLICK, 209)
    DECLARE_EVENT_TYPE(wxEVT_NC_MIDDLE_DCLICK, 210)
    DECLARE_EVENT_TYPE(wxEVT_NC_RIGHT_DCLICK, 211)

        // Character input event type
    DECLARE_EVENT_TYPE(wxEVT_CHAR, 212)
    DECLARE_EVENT_TYPE(wxEVT_CHAR_HOOK, 213)
    DECLARE_EVENT_TYPE(wxEVT_NAVIGATION_KEY, 214)
    DECLARE_EVENT_TYPE(wxEVT_KEY_DOWN, 215)
    DECLARE_EVENT_TYPE(wxEVT_KEY_UP, 216)

        // Set cursor event
    DECLARE_EVENT_TYPE(wxEVT_SET_CURSOR, 230)

        // wxScrollBar and wxSlider event identifiers
    DECLARE_EVENT_TYPE(wxEVT_SCROLL_TOP, 300)
    DECLARE_EVENT_TYPE(wxEVT_SCROLL_BOTTOM, 301)
    DECLARE_EVENT_TYPE(wxEVT_SCROLL_LINEUP, 302)
    DECLARE_EVENT_TYPE(wxEVT_SCROLL_LINEDOWN, 303)
    DECLARE_EVENT_TYPE(wxEVT_SCROLL_PAGEUP, 304)
    DECLARE_EVENT_TYPE(wxEVT_SCROLL_PAGEDOWN, 305)
    DECLARE_EVENT_TYPE(wxEVT_SCROLL_THUMBTRACK, 306)
    DECLARE_EVENT_TYPE(wxEVT_SCROLL_THUMBRELEASE, 307)
    DECLARE_EVENT_TYPE(wxEVT_SCROLL_ENDSCROLL, 308)

        // Scroll events from wxWindow
    DECLARE_EVENT_TYPE(wxEVT_SCROLLWIN_TOP, 320)
    DECLARE_EVENT_TYPE(wxEVT_SCROLLWIN_BOTTOM, 321)
    DECLARE_EVENT_TYPE(wxEVT_SCROLLWIN_LINEUP, 322)
    DECLARE_EVENT_TYPE(wxEVT_SCROLLWIN_LINEDOWN, 323)
    DECLARE_EVENT_TYPE(wxEVT_SCROLLWIN_PAGEUP, 324)
    DECLARE_EVENT_TYPE(wxEVT_SCROLLWIN_PAGEDOWN, 325)
    DECLARE_EVENT_TYPE(wxEVT_SCROLLWIN_THUMBTRACK, 326)
    DECLARE_EVENT_TYPE(wxEVT_SCROLLWIN_THUMBRELEASE, 327)

        // System events
    DECLARE_EVENT_TYPE(wxEVT_SIZE, 400)
    DECLARE_EVENT_TYPE(wxEVT_MOVE, 401)
    DECLARE_EVENT_TYPE(wxEVT_CLOSE_WINDOW, 402)
    DECLARE_EVENT_TYPE(wxEVT_END_SESSION, 403)
    DECLARE_EVENT_TYPE(wxEVT_QUERY_END_SESSION, 404)
    DECLARE_EVENT_TYPE(wxEVT_ACTIVATE_APP, 405)
    DECLARE_EVENT_TYPE(wxEVT_POWER, 406)
    DECLARE_EVENT_TYPE(wxEVT_ACTIVATE, 409)
    DECLARE_EVENT_TYPE(wxEVT_CREATE, 410)
    DECLARE_EVENT_TYPE(wxEVT_DESTROY, 411)
    DECLARE_EVENT_TYPE(wxEVT_SHOW, 412)
    DECLARE_EVENT_TYPE(wxEVT_ICONIZE, 413)
    DECLARE_EVENT_TYPE(wxEVT_MAXIMIZE, 414)
    DECLARE_EVENT_TYPE(wxEVT_MOUSE_CAPTURE_CHANGED, 415)
    DECLARE_EVENT_TYPE(wxEVT_PAINT, 416)
    DECLARE_EVENT_TYPE(wxEVT_ERASE_BACKGROUND, 417)
    DECLARE_EVENT_TYPE(wxEVT_NC_PAINT, 418)
    DECLARE_EVENT_TYPE(wxEVT_PAINT_ICON, 419)
    DECLARE_EVENT_TYPE(wxEVT_MENU_OPEN, 420)
    DECLARE_EVENT_TYPE(wxEVT_MENU_CLOSE, 421)
    DECLARE_EVENT_TYPE(wxEVT_MENU_HIGHLIGHT, 422)
    // DECLARE_EVENT_TYPE(wxEVT_POPUP_MENU_INIT, 423) -- free slot
    DECLARE_EVENT_TYPE(wxEVT_CONTEXT_MENU, 424)
    DECLARE_EVENT_TYPE(wxEVT_SYS_COLOUR_CHANGED, 425)
    DECLARE_EVENT_TYPE(wxEVT_DISPLAY_CHANGED, 426)
    DECLARE_EVENT_TYPE(wxEVT_SETTING_CHANGED, 427)
    DECLARE_EVENT_TYPE(wxEVT_QUERY_NEW_PALETTE, 428)
    DECLARE_EVENT_TYPE(wxEVT_PALETTE_CHANGED, 429)
    DECLARE_EVENT_TYPE(wxEVT_JOY_BUTTON_DOWN, 430)
    DECLARE_EVENT_TYPE(wxEVT_JOY_BUTTON_UP, 431)
    DECLARE_EVENT_TYPE(wxEVT_JOY_MOVE, 432)
    DECLARE_EVENT_TYPE(wxEVT_JOY_ZMOVE, 433)
    DECLARE_EVENT_TYPE(wxEVT_DROP_FILES, 434)
    DECLARE_EVENT_TYPE(wxEVT_DRAW_ITEM, 435)
    DECLARE_EVENT_TYPE(wxEVT_MEASURE_ITEM, 436)
    DECLARE_EVENT_TYPE(wxEVT_COMPARE_ITEM, 437)
    DECLARE_EVENT_TYPE(wxEVT_INIT_DIALOG, 438)
    DECLARE_EVENT_TYPE(wxEVT_IDLE, 439)
    DECLARE_EVENT_TYPE(wxEVT_UPDATE_UI, 440)

        // Generic command events
        // Note: a click is a higher-level event than button down/up
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_LEFT_CLICK, 500)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_LEFT_DCLICK, 501)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_RIGHT_CLICK, 502)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_RIGHT_DCLICK, 503)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_SET_FOCUS, 504)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_KILL_FOCUS, 505)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_ENTER, 506)

        // Help events
    DECLARE_EVENT_TYPE(wxEVT_HELP, 1050)
    DECLARE_EVENT_TYPE(wxEVT_DETAILED_HELP, 1051)

END_DECLARE_EVENT_TYPES()

// these 2 events are the same
#define wxEVT_COMMAND_TOOL_CLICKED wxEVT_COMMAND_MENU_SELECTED

// ----------------------------------------------------------------------------
// Compatibility
// ----------------------------------------------------------------------------

// this event is also used by wxComboBox and wxSpinCtrl which don't include
// wx/textctrl.h in all ports [yet], so declare it here as well
//
// still, any new code using it should include wx/textctrl.h explicitly
#if !WXWIN_COMPATIBILITY_EVENT_TYPES
    extern const wxEventType WXDLLEXPORT wxEVT_COMMAND_TEXT_UPDATED;
#endif

#if WXWIN_COMPATIBILITY

#define wxEVENT_TYPE_BUTTON_COMMAND             wxEVT_COMMAND_BUTTON_CLICKED
#define wxEVENT_TYPE_CHECKBOX_COMMAND           wxEVT_COMMAND_CHECKBOX_CLICKED
#define wxEVENT_TYPE_CHOICE_COMMAND             wxEVT_COMMAND_CHOICE_SELECTED
#define wxEVENT_TYPE_LISTBOX_COMMAND            wxEVT_COMMAND_LISTBOX_SELECTED
#define wxEVENT_TYPE_LISTBOX_DCLICK_COMMAND     wxEVT_COMMAND_LISTBOX_DOUBLECLICKED
#define wxEVENT_TYPE_TEXT_COMMAND               wxEVT_COMMAND_TEXT_UPDATED
#define wxEVENT_TYPE_MULTITEXT_COMMAND          wxEVT_COMMAND_TEXT_UPDATED
#define wxEVENT_TYPE_MENU_COMMAND               wxEVT_COMMAND_MENU_SELECTED
#define wxEVENT_TYPE_SLIDER_COMMAND             wxEVT_COMMAND_SLIDER_UPDATED
#define wxEVENT_TYPE_RADIOBOX_COMMAND           wxEVT_COMMAND_RADIOBOX_SELECTED
#define wxEVENT_TYPE_RADIOBUTTON_COMMAND        wxEVT_COMMAND_RADIOBUTTON_SELECTED
#define wxEVENT_TYPE_TEXT_ENTER_COMMAND         wxEVT_COMMAND_TEXT_ENTER
#define wxEVENT_TYPE_SET_FOCUS                  wxEVT_SET_FOCUS
#define wxEVENT_TYPE_KILL_FOCUS                 wxEVT_KILL_FOCUS
#define wxEVENT_TYPE_SCROLLBAR_COMMAND          wxEVT_COMMAND_SCROLLBAR_UPDATED
#define wxEVENT_TYPE_VIRT_LISTBOX_COMMAND       wxEVT_COMMAND_VLBOX_SELECTED
#define wxEVENT_TYPE_COMBOBOX_COMMAND           wxEVT_COMMAND_COMBOBOX_SELECTED

#define wxEVENT_TYPE_LEFT_DOWN                  wxEVT_LEFT_DOWN
#define wxEVENT_TYPE_LEFT_UP                    wxEVT_LEFT_UP
#define wxEVENT_TYPE_MIDDLE_DOWN                wxEVT_MIDDLE_DOWN
#define wxEVENT_TYPE_MIDDLE_UP                  wxEVT_MIDDLE_UP
#define wxEVENT_TYPE_RIGHT_DOWN                 wxEVT_RIGHT_DOWN
#define wxEVENT_TYPE_RIGHT_UP                   wxEVT_RIGHT_UP
#define wxEVENT_TYPE_MOTION                     wxEVT_MOTION
#define wxEVENT_TYPE_ENTER_WINDOW               wxEVT_ENTER_WINDOW
#define wxEVENT_TYPE_LEAVE_WINDOW               wxEVT_LEAVE_WINDOW
#define wxEVENT_TYPE_LEFT_DCLICK                wxEVT_LEFT_DCLICK
#define wxEVENT_TYPE_MIDDLE_DCLICK              wxEVT_MIDDLE_DCLICK
#define wxEVENT_TYPE_RIGHT_DCLICK               wxEVT_RIGHT_DCLICK
#define wxEVENT_TYPE_CHAR                       wxEVT_CHAR
#define wxEVENT_TYPE_SCROLL_TOP                 wxEVT_SCROLL_TOP
#define wxEVENT_TYPE_SCROLL_BOTTOM              wxEVT_SCROLL_BOTTOM
#define wxEVENT_TYPE_SCROLL_LINEUP              wxEVT_SCROLL_LINEUP
#define wxEVENT_TYPE_SCROLL_LINEDOWN            wxEVT_SCROLL_LINEDOWN
#define wxEVENT_TYPE_SCROLL_PAGEUP              wxEVT_SCROLL_PAGEUP
#define wxEVENT_TYPE_SCROLL_PAGEDOWN            wxEVT_SCROLL_PAGEDOWN
#define wxEVENT_TYPE_SCROLL_THUMBTRACK          wxEVT_SCROLL_THUMBTRACK
#define wxEVENT_TYPE_SCROLL_ENDSCROLL           wxEVT_SCROLL_ENDSCROLL

#endif // WXWIN_COMPATIBILITY

/*
 * wxWindows events, covering all interesting things that might happen
 * (button clicking, resizing, setting text in widgets, etc.).
 *
 * For each completely new event type, derive a new event class.
 * An event CLASS represents a C++ class defining a range of similar event TYPES;
 * examples are canvas events, panel item command events.
 * An event TYPE is a unique identifier for a particular system event,
 * such as a button press or a listbox deselection.
 *
 */

class WXDLLEXPORT wxEvent : public wxObject
{
private:
    wxEvent& operator=(const wxEvent&);

protected:
    wxEvent(const wxEvent&);                   // for implementing Clone()

public:
    wxEvent(int id = 0, wxEventType commandType = wxEVT_NULL );

    void SetEventType(wxEventType typ) { m_eventType = typ; }
    wxEventType GetEventType() const { return m_eventType; }
    wxObject *GetEventObject() const { return m_eventObject; }
    void SetEventObject(wxObject *obj) { m_eventObject = obj; }
    long GetTimestamp() const { return m_timeStamp; }
    void SetTimestamp(long ts = 0) { m_timeStamp = ts; }
    int GetId() const { return m_id; }
    void SetId(int Id) { m_id = Id; }

    // Can instruct event processor that we wish to ignore this event
    // (treat as if the event table entry had not been found): this must be done
    // to allow the event processing by the base classes (calling event.Skip()
    // is the analog of calling the base class verstion of a virtual function)
    void Skip(bool skip = TRUE) { m_skipped = skip; }
    bool GetSkipped() const { return m_skipped; };

    // Implementation only: this test is explicitlty anti OO and this functions
    // exists only for optimization purposes.
    bool IsCommandEvent() const { return m_isCommandEvent; }

    // this function is used to create a copy of the event polymorphically and
    // all derived classes must implement it because otherwise wxPostEvent()
    // for them wouldn't work (it needs to do a copy of the event)
    virtual wxEvent *Clone() const = 0;

public:
    wxObject*         m_eventObject;
    wxEventType       m_eventType;
    long              m_timeStamp;
    int               m_id;
    wxObject*         m_callbackUserData;
    bool              m_skipped;
    bool              m_isCommandEvent;

private:
    DECLARE_ABSTRACT_CLASS(wxEvent)
};

#if wxUSE_GUI

// Item or menu event class
/*
 wxEVT_COMMAND_BUTTON_CLICKED
 wxEVT_COMMAND_CHECKBOX_CLICKED
 wxEVT_COMMAND_CHOICE_SELECTED
 wxEVT_COMMAND_LISTBOX_SELECTED
 wxEVT_COMMAND_LISTBOX_DOUBLECLICKED
 wxEVT_COMMAND_TEXT_UPDATED
 wxEVT_COMMAND_TEXT_ENTER
 wxEVT_COMMAND_MENU_SELECTED
 wxEVT_COMMAND_SLIDER_UPDATED
 wxEVT_COMMAND_RADIOBOX_SELECTED
 wxEVT_COMMAND_RADIOBUTTON_SELECTED
 wxEVT_COMMAND_SCROLLBAR_UPDATED
 wxEVT_COMMAND_VLBOX_SELECTED
 wxEVT_COMMAND_COMBOBOX_SELECTED
 wxEVT_COMMAND_TOGGLEBUTTON_CLICKED
*/

class WXDLLEXPORT wxCommandEvent : public wxEvent
{
private:
    wxCommandEvent& operator=(const wxCommandEvent& event);

public:
    wxCommandEvent(wxEventType commandType = wxEVT_NULL, int id = 0);

    wxCommandEvent(const wxCommandEvent& event)
        : wxEvent(event),
          m_commandString(event.m_commandString),
          m_commandInt(event.m_commandInt),
          m_extraLong(event.m_extraLong),
          m_clientData(event.m_clientData),
          m_clientObject(event.m_clientObject)
        { }

    // Set/Get client data from controls
    void SetClientData(void* clientData) { m_clientData = clientData; }
    void *GetClientData() const { return m_clientData; }

    // Set/Get client object from controls
    void SetClientObject(wxClientData* clientObject) { m_clientObject = clientObject; }
    void *GetClientObject() const { return m_clientObject; }

    // Get listbox selection if single-choice
    int GetSelection() const { return m_commandInt; }

    // Set/Get listbox/choice selection string
    void SetString(const wxString& s) { m_commandString = s; }
    wxString GetString() const { return m_commandString; }

    // Get checkbox value
    bool IsChecked() const { return m_commandInt != 0; }

    // TRUE if the listbox event was a selection.
    bool IsSelection() const { return (m_extraLong != 0); }

    void SetExtraLong(long extraLong) { m_extraLong = extraLong; }
    long GetExtraLong() const { return m_extraLong ; }

    void SetInt(int i) { m_commandInt = i; }
    long GetInt() const { return m_commandInt ; }

    virtual wxEvent *Clone() const { return new wxCommandEvent(*this); }

#if WXWIN_COMPATIBILITY_2
    bool Checked() const { return IsChecked(); }
#endif // WXWIN_COMPATIBILITY_2

public:
    wxString          m_commandString; // String event argument
    int               m_commandInt;
    long              m_extraLong;     // Additional information (e.g. select/deselect)
    void*             m_clientData;    // Arbitrary client data
    wxClientData*     m_clientObject;  // Arbitrary client object

private:
    DECLARE_DYNAMIC_CLASS(wxCommandEvent)
};

// this class adds a possibility to react (from the user) code to a control
// notification: allow or veto the operation being reported.
class WXDLLEXPORT wxNotifyEvent  : public wxCommandEvent
{
public:
    wxNotifyEvent(wxEventType commandType = wxEVT_NULL, int id = 0)
        : wxCommandEvent(commandType, id)
        { m_bAllow = TRUE; }

    wxNotifyEvent(const wxNotifyEvent& event)
        : wxCommandEvent(event)
        { m_bAllow = event.m_bAllow; }

    // veto the operation (usually it's allowed by default)
    void Veto() { m_bAllow = FALSE; }

    // allow the operation if it was disabled by default
    void Allow() { m_bAllow = TRUE; }

    // for implementation code only: is the operation allowed?
    bool IsAllowed() const { return m_bAllow; }

    virtual wxEvent *Clone() const { return new wxNotifyEvent(*this); }

private:
    bool m_bAllow;

private:
    DECLARE_DYNAMIC_CLASS(wxNotifyEvent)
};

// Scroll event class, derived form wxCommandEvent. wxScrollEvents are
// sent by wxSlider and wxScrollBar.
/*
 wxEVT_SCROLL_TOP
 wxEVT_SCROLL_BOTTOM
 wxEVT_SCROLL_LINEUP
 wxEVT_SCROLL_LINEDOWN
 wxEVT_SCROLL_PAGEUP
 wxEVT_SCROLL_PAGEDOWN
 wxEVT_SCROLL_THUMBTRACK
 wxEVT_SCROLL_THUMBRELEASE
 wxEVT_SCROLL_ENDSCROLL
*/

class WXDLLEXPORT wxScrollEvent : public wxCommandEvent
{
public:
    wxScrollEvent(wxEventType commandType = wxEVT_NULL,
                  int id = 0, int pos = 0, int orient = 0);

    int GetOrientation() const { return (int) m_extraLong ; }
    int GetPosition() const { return m_commandInt ; }
    void SetOrientation(int orient) { m_extraLong = (long) orient; }
    void SetPosition(int pos) { m_commandInt = pos; }

    virtual wxEvent *Clone() const { return new wxScrollEvent(*this); }

private:
    DECLARE_DYNAMIC_CLASS(wxScrollEvent)
};

// ScrollWin event class, derived fom wxEvent. wxScrollWinEvents
// are sent by wxWindow.
/*
 wxEVT_SCROLLWIN_TOP
 wxEVT_SCROLLWIN_BOTTOM
 wxEVT_SCROLLWIN_LINEUP
 wxEVT_SCROLLWIN_LINEDOWN
 wxEVT_SCROLLWIN_PAGEUP
 wxEVT_SCROLLWIN_PAGEDOWN
 wxEVT_SCROLLWIN_THUMBTRACK
 wxEVT_SCROLLWIN_THUMBRELEASE
*/

class WXDLLEXPORT wxScrollWinEvent : public wxEvent
{
public:
    wxScrollWinEvent(wxEventType commandType = wxEVT_NULL,
                     int pos = 0, int orient = 0);
	wxScrollWinEvent(const wxScrollWinEvent & event) : wxEvent(event)
		{	m_commandInt = event.m_commandInt;
			m_extraLong = event.m_extraLong;	}

    int GetOrientation() const { return (int) m_extraLong ; }
    int GetPosition() const { return m_commandInt ; }
    void SetOrientation(int orient) { m_extraLong = (long) orient; }
    void SetPosition(int pos) { m_commandInt = pos; }

    virtual wxEvent *Clone() const { return new wxScrollWinEvent(*this); }

public:
    int               m_commandInt;
    long              m_extraLong;

private:
    DECLARE_DYNAMIC_CLASS(wxScrollWinEvent)
};

// Mouse event class

/*
 wxEVT_LEFT_DOWN
 wxEVT_LEFT_UP
 wxEVT_MIDDLE_DOWN
 wxEVT_MIDDLE_UP
 wxEVT_RIGHT_DOWN
 wxEVT_RIGHT_UP
 wxEVT_MOTION
 wxEVT_ENTER_WINDOW
 wxEVT_LEAVE_WINDOW
 wxEVT_LEFT_DCLICK
 wxEVT_MIDDLE_DCLICK
 wxEVT_RIGHT_DCLICK
 wxEVT_NC_LEFT_DOWN
 wxEVT_NC_LEFT_UP,
 wxEVT_NC_MIDDLE_DOWN,
 wxEVT_NC_MIDDLE_UP,
 wxEVT_NC_RIGHT_DOWN,
 wxEVT_NC_RIGHT_UP,
 wxEVT_NC_MOTION,
 wxEVT_NC_ENTER_WINDOW,
 wxEVT_NC_LEAVE_WINDOW,
 wxEVT_NC_LEFT_DCLICK,
 wxEVT_NC_MIDDLE_DCLICK,
 wxEVT_NC_RIGHT_DCLICK,
*/

// the symbolic names for the mouse buttons
enum
{
    wxMOUSE_BTN_ANY     = -1,
    wxMOUSE_BTN_NONE    = -1,
    wxMOUSE_BTN_LEFT    = 0,
    wxMOUSE_BTN_MIDDLE  = 1,
    wxMOUSE_BTN_RIGHT   = 2
};

class WXDLLEXPORT wxMouseEvent : public wxEvent
{
public:
    wxMouseEvent(wxEventType mouseType = wxEVT_NULL);
    wxMouseEvent(const wxMouseEvent& event)	: wxEvent(event)
		{ Assign(event); }

    // Was it a button event? (*doesn't* mean: is any button *down*?)
    bool IsButton() const { return Button(wxMOUSE_BTN_ANY); }

    // Was it a down event from button 1, 2 or 3 or any?
    bool ButtonDown(int but = wxMOUSE_BTN_ANY) const;

    // Was it a dclick event from button 1, 2 or 3 or any?
    bool ButtonDClick(int but = wxMOUSE_BTN_ANY) const;

    // Was it a up event from button 1, 2 or 3 or any?
    bool ButtonUp(int but = wxMOUSE_BTN_ANY) const;

    // Was the given button 1,2,3 or any changing state?
    bool Button(int but) const;

    // Was the given button 1,2,3 or any in Down state?
    bool ButtonIsDown(int but) const;

    // Get the button which is changing state (wxMOUSE_BTN_NONE if none)
    int GetButton() const;

    // Find state of shift/control keys
    bool ControlDown() const { return m_controlDown; }
    bool MetaDown() const { return m_metaDown; }
    bool AltDown() const { return m_altDown; }
    bool ShiftDown() const { return m_shiftDown; }

    // Find which event was just generated
    bool LeftDown() const { return (m_eventType == wxEVT_LEFT_DOWN); }
    bool MiddleDown() const { return (m_eventType == wxEVT_MIDDLE_DOWN); }
    bool RightDown() const { return (m_eventType == wxEVT_RIGHT_DOWN); }

    bool LeftUp() const { return (m_eventType == wxEVT_LEFT_UP); }
    bool MiddleUp() const { return (m_eventType == wxEVT_MIDDLE_UP); }
    bool RightUp() const { return (m_eventType == wxEVT_RIGHT_UP); }

    bool LeftDClick() const { return (m_eventType == wxEVT_LEFT_DCLICK); }
    bool MiddleDClick() const { return (m_eventType == wxEVT_MIDDLE_DCLICK); }
    bool RightDClick() const { return (m_eventType == wxEVT_RIGHT_DCLICK); }

    // Find the current state of the mouse buttons (regardless
    // of current event type)
    bool LeftIsDown() const { return m_leftDown; }
    bool MiddleIsDown() const { return m_middleDown; }
    bool RightIsDown() const { return m_rightDown; }

    // True if a button is down and the mouse is moving
    bool Dragging() const
    {
        return ((m_eventType == wxEVT_MOTION) &&
                (LeftIsDown() || MiddleIsDown() || RightIsDown()));
    }

    // True if the mouse is moving, and no button is down
    bool Moving() const { return (m_eventType == wxEVT_MOTION); }

    // True if the mouse is just entering the window
    bool Entering() const { return (m_eventType == wxEVT_ENTER_WINDOW); }

    // True if the mouse is just leaving the window
    bool Leaving() const { return (m_eventType == wxEVT_LEAVE_WINDOW); }

    // Find the position of the event
    void GetPosition(wxCoord *xpos, wxCoord *ypos) const
    {
        if (xpos)
            *xpos = m_x;
        if (ypos)
            *ypos = m_y;
    }

#ifndef __WIN16__
    void GetPosition(long *xpos, long *ypos) const
    {
        if (xpos)
            *xpos = (long)m_x;
        if (ypos)
            *ypos = (long)m_y;
    }
#endif

    // Find the position of the event
    wxPoint GetPosition() const { return wxPoint(m_x, m_y); }

    // Find the logical position of the event given the DC
    wxPoint GetLogicalPosition(const wxDC& dc) const ;

    // Compatibility
#if WXWIN_COMPATIBILITY
    void Position(long *xpos, long *ypos) const
    {
        if (xpos)
            *xpos = (long)m_x;
        if (ypos)
            *ypos = (long)m_y;
    }

    void Position(float *xpos, float *ypos) const
    {
        *xpos = (float) m_x; *ypos = (float) m_y;
    }
#endif // WXWIN_COMPATIBILITY

    // Get X position
    wxCoord GetX() const { return m_x; }

    // Get Y position
    wxCoord GetY() const { return m_y; }

    // Get wheel rotation, positive or negative indicates direction of
    // rotation.  Current devices all send an event when rotation is equal to
    // +/-WheelDelta, but this allows for finer resolution devices to be
    // created in the future.  Because of this you shouldn't assume that one
    // event is equal to 1 line or whatever, but you should be able to either
    // do partial line scrolling or wait until +/-WheelDelta rotation values
    // have been accumulated before scrolling.
    int GetWheelRotation() const { return m_wheelRotation; }

    // Get wheel delta, normally 120.  This is the threshold for action to be
    // taken, and one such action (for example, scrolling one increment)
    // should occur for each delta.
    int GetWheelDelta() const { return m_wheelDelta; }

    // Returns the configured number of lines (or whatever) to be scrolled per
    // wheel action.  Defaults to one.
    int GetLinesPerAction() const { return m_linesPerAction; }

    // Is the system set to do page scrolling?
    bool IsPageScroll() const { return ((unsigned int)m_linesPerAction == UINT_MAX); }

    virtual wxEvent *Clone() const { return new wxMouseEvent(*this); }

    wxMouseEvent& operator=(const wxMouseEvent& event) { Assign(event); return *this; }

public:
    wxCoord m_x, m_y;

    bool          m_leftDown;
    bool          m_middleDown;
    bool          m_rightDown;

    bool          m_controlDown;
    bool          m_shiftDown;
    bool          m_altDown;
    bool          m_metaDown;

    int           m_wheelRotation;
    int           m_wheelDelta;
    int           m_linesPerAction;

protected:
    void Assign(const wxMouseEvent& evt);

private:
    DECLARE_DYNAMIC_CLASS(wxMouseEvent)
};

// Cursor set event

/*
   wxEVT_SET_CURSOR
 */

class WXDLLEXPORT wxSetCursorEvent : public wxEvent
{
public:
    wxSetCursorEvent(wxCoord x = 0, wxCoord y = 0)
        : wxEvent(0, wxEVT_SET_CURSOR),
          m_x(x), m_y(y), m_cursor()
        { }

	wxSetCursorEvent(const wxSetCursorEvent & event) : wxEvent(event)
		{	m_x = event.m_x;
			m_y = event.m_y;
			m_cursor = event.m_cursor;	}

    wxCoord GetX() const { return m_x; }
    wxCoord GetY() const { return m_y; }

    void SetCursor(const wxCursor& cursor) { m_cursor = cursor; }
    const wxCursor& GetCursor() const { return m_cursor; }
    bool HasCursor() const { return m_cursor.Ok(); }

    virtual wxEvent *Clone() const { return new wxSetCursorEvent(*this); }

private:
    wxCoord  m_x, m_y;
    wxCursor m_cursor;

private:
    DECLARE_DYNAMIC_CLASS(wxSetCursorEvent)
};

// Keyboard input event class

/*
 wxEVT_CHAR
 wxEVT_CHAR_HOOK
 wxEVT_KEY_DOWN
 wxEVT_KEY_UP
 */

class WXDLLEXPORT wxKeyEvent : public wxEvent
{
public:
    wxKeyEvent(wxEventType keyType = wxEVT_NULL);
	wxKeyEvent(const wxKeyEvent& evt);

    // Find state of shift/control keys
    bool ControlDown() const { return m_controlDown; }
    bool MetaDown() const { return m_metaDown; }
    bool AltDown() const { return m_altDown; }
    bool ShiftDown() const { return m_shiftDown; }

    // exclude MetaDown() from HasModifiers() because NumLock under X is often
    // configured as mod2 modifier, yet the key events even when it is pressed
    // should be processed normally, not like Ctrl- or Alt-key
    bool HasModifiers() const { return ControlDown() || AltDown(); }

    // get the key code: an ASCII7 char or an element of wxKeyCode enum
    int GetKeyCode() const { return (int)m_keyCode; }

    // get the raw key code (platform-dependent)
    wxUint32 GetRawKeyCode() const { return m_rawCode; }

    // get the raw key flags (platform-dependent)
    wxUint32 GetRawKeyFlags() const { return m_rawFlags; }

    // Find the position of the event
    void GetPosition(wxCoord *xpos, wxCoord *ypos) const
    {
        if (xpos) *xpos = m_x;
        if (ypos) *ypos = m_y;
    }

#ifndef __WIN16__
    void GetPosition(long *xpos, long *ypos) const
    {
        if (xpos) *xpos = (long)m_x;
        if (ypos) *ypos = (long)m_y;
    }
#endif

    wxPoint GetPosition() const
        { return wxPoint(m_x, m_y); }

    // Get X position
    wxCoord GetX() const { return m_x; }

    // Get Y position
    wxCoord GetY() const { return m_y; }

    // deprecated
    long KeyCode() const { return m_keyCode; }

    virtual wxEvent *Clone() const { return new wxKeyEvent(*this); }

    // we do need to copy wxKeyEvent sometimes (in wxTreeCtrl code, for
    // example)
    wxKeyEvent& operator=(const wxKeyEvent& evt)
    {
        m_x = evt.m_x;
        m_y = evt.m_y;

        m_keyCode = evt.m_keyCode;

        m_controlDown = evt.m_controlDown;
        m_shiftDown = evt.m_shiftDown;
        m_altDown = evt.m_altDown;
        m_metaDown = evt.m_metaDown;
        m_scanCode = evt.m_scanCode;
        m_rawCode = evt.m_rawCode;
        m_rawFlags = evt.m_rawFlags;

        return *this;
    }

public:
    wxCoord       m_x, m_y;

    long          m_keyCode;

    bool          m_controlDown;
    bool          m_shiftDown;
    bool          m_altDown;
    bool          m_metaDown;
    bool          m_scanCode;

#if wxUSE_UNICODE
    // This contains the full Unicode character
    // in a character events in Unicode mode
    wxChar        m_uniChar;
#endif

    // these fields contain the platform-specific information about
    // key that was pressed
    wxUint32      m_rawCode;
    wxUint32      m_rawFlags;

private:
    DECLARE_DYNAMIC_CLASS(wxKeyEvent)
};

// Size event class
/*
 wxEVT_SIZE
 */

class WXDLLEXPORT wxSizeEvent : public wxEvent
{
public:
    wxSizeEvent() : wxEvent(0, wxEVT_SIZE)
        { }
    wxSizeEvent(const wxSize& sz, int id = 0)
        : wxEvent(id, wxEVT_SIZE),
          m_size(sz)
        { }
    wxSizeEvent(const wxSizeEvent & event)
		: wxEvent(event),
		  m_size(event.m_size)
		{ }

    wxSize GetSize() const { return m_size; }

    virtual wxEvent *Clone() const { return new wxSizeEvent(*this); }

public:
    wxSize m_size;

private:
    DECLARE_DYNAMIC_CLASS(wxSizeEvent)
};

// Move event class

/*
 wxEVT_MOVE
 */

class WXDLLEXPORT wxMoveEvent : public wxEvent
{
public:
    wxMoveEvent()
        : wxEvent(0, wxEVT_MOVE)
        { }
    wxMoveEvent(const wxPoint& pos, int id = 0)
        : wxEvent(id, wxEVT_MOVE),
          m_pos(pos)
        { }
    wxMoveEvent(const wxMoveEvent& event)
        : wxEvent(event),
		  m_pos(event.m_pos)
	{ }

    wxPoint GetPosition() const { return m_pos; }

    virtual wxEvent *Clone() const { return new wxMoveEvent(*this); }

    wxPoint m_pos;

private:
    DECLARE_DYNAMIC_CLASS(wxMoveEvent)
};

// Paint event class
/*
 wxEVT_PAINT
 wxEVT_NC_PAINT
 wxEVT_PAINT_ICON
 */

#if defined(__WXDEBUG__) && (defined(__WXMSW__) || defined(__WXPM__))
    // see comments in src/msw|os2/dcclient.cpp where g_isPainting is defined
    extern WXDLLEXPORT int g_isPainting;
#endif // debug

class WXDLLEXPORT wxPaintEvent : public wxEvent
{
public:
    wxPaintEvent(int Id = 0)
        : wxEvent(Id, wxEVT_PAINT)
    {
#if defined(__WXDEBUG__) && (defined(__WXMSW__) || defined(__WXPM__))
        // set the internal flag for the duration of processing of WM_PAINT
        g_isPainting++;
#endif // debug
    }

#if defined(__WXDEBUG__) && (defined(__WXMSW__) || defined(__WXPM__))
    ~wxPaintEvent()
    {
        g_isPainting--;
    }
#endif // debug

    virtual wxEvent *Clone() const { return new wxPaintEvent(*this); }

private:
    DECLARE_DYNAMIC_CLASS(wxPaintEvent)
};

class WXDLLEXPORT wxNcPaintEvent : public wxEvent
{
public:
    wxNcPaintEvent(int id = 0)
        : wxEvent(id, wxEVT_NC_PAINT)
        { }

    virtual wxEvent *Clone() const { return new wxNcPaintEvent(*this); }

private:
    DECLARE_DYNAMIC_CLASS(wxNcPaintEvent)
};

// Erase background event class
/*
 wxEVT_ERASE_BACKGROUND
 */

class WXDLLEXPORT wxEraseEvent : public wxEvent
{
private:
    wxEraseEvent& operator=(const wxEraseEvent& event);

public:
    wxEraseEvent(int Id = 0, wxDC *dc = (wxDC *) NULL)
        : wxEvent(Id, wxEVT_ERASE_BACKGROUND),
          m_dc(dc)
        { }

    wxEraseEvent(const wxEraseEvent& event)
        : wxEvent(event),
          m_dc(event.m_dc)
        { }

    wxDC *GetDC() const { return m_dc; }

    virtual wxEvent *Clone() const { return new wxEraseEvent(*this); }

    wxDC *m_dc;

private:
    DECLARE_DYNAMIC_CLASS(wxEraseEvent)
};

// Focus event class
/*
 wxEVT_SET_FOCUS
 wxEVT_KILL_FOCUS
 */

class WXDLLEXPORT wxFocusEvent : public wxEvent
{
private:
    wxFocusEvent& operator=(const wxFocusEvent& event);

public:
    wxFocusEvent(wxEventType type = wxEVT_NULL, int id = 0)
        : wxEvent(id, type)
        { m_win = NULL; }

    wxFocusEvent(const wxFocusEvent& event)
        : wxEvent(event)
        { m_win = event.m_win; }

    // The window associated with this event is the window which had focus
    // before for SET event and the window which will have focus for the KILL
    // one. NB: it may be NULL in both cases!
    wxWindow *GetWindow() const { return m_win; }
    void SetWindow(wxWindow *win) { m_win = win; }

    virtual wxEvent *Clone() const { return new wxFocusEvent(*this); }

private:
    wxWindow *m_win;

private:
    DECLARE_DYNAMIC_CLASS(wxFocusEvent)
};

// wxChildFocusEvent notifies the parent that a child has got the focus: unlike
// wxFocusEvent it is propgated upwards the window chain
class WXDLLEXPORT wxChildFocusEvent : public wxCommandEvent
{
public:
    wxChildFocusEvent(wxWindow *win = NULL);

    wxWindow *GetWindow() const { return (wxWindow *)GetEventObject(); }

    virtual wxEvent *Clone() const { return new wxChildFocusEvent(*this); }

private:
    DECLARE_DYNAMIC_CLASS(wxChildFocusEvent)
};

// Activate event class
/*
 wxEVT_ACTIVATE
 wxEVT_ACTIVATE_APP
 */

class WXDLLEXPORT wxActivateEvent : public wxEvent
{
public:
    wxActivateEvent(wxEventType type = wxEVT_NULL, bool active = TRUE, int Id = 0)
        : wxEvent(Id, type)
        { m_active = active; }
    wxActivateEvent(const wxActivateEvent& event)
        : wxEvent(event)
	{ m_active = event.m_active; }

    bool GetActive() const { return m_active; }

    virtual wxEvent *Clone() const { return new wxActivateEvent(*this); }

private:
    bool m_active;

private:
    DECLARE_DYNAMIC_CLASS(wxActivateEvent)
};

// InitDialog event class
/*
 wxEVT_INIT_DIALOG
 */

class WXDLLEXPORT wxInitDialogEvent : public wxEvent
{
public:
    wxInitDialogEvent(int Id = 0)
        : wxEvent(Id, wxEVT_INIT_DIALOG)
        { }

    virtual wxEvent *Clone() const { return new wxInitDialogEvent(*this); }

private:
    DECLARE_DYNAMIC_CLASS(wxInitDialogEvent)
};

// Miscellaneous menu event class
/*
 wxEVT_MENU_OPEN,
 wxEVT_MENU_CLOSE,
 wxEVT_MENU_HIGHLIGHT,
*/

class WXDLLEXPORT wxMenuEvent : public wxEvent
{
public:
    wxMenuEvent(wxEventType type = wxEVT_NULL, int id = 0)
        : wxEvent(id, type)
        { m_menuId = id; }
    wxMenuEvent(const wxMenuEvent & event)
        : wxEvent(event)
	{ m_menuId = event.m_menuId; }

    // only for wxEVT_MENU_HIGHLIGHT
    int GetMenuId() const { return m_menuId; }

    // only for wxEVT_MENU_OPEN/CLOSE
    bool IsPopup() const { return m_menuId == -1; }

    virtual wxEvent *Clone() const { return new wxMenuEvent(*this); }

private:
    int m_menuId;

    DECLARE_DYNAMIC_CLASS(wxMenuEvent)
};

// Window close or session close event class
/*
 wxEVT_CLOSE_WINDOW,
 wxEVT_END_SESSION,
 wxEVT_QUERY_END_SESSION
 */

class WXDLLEXPORT wxCloseEvent : public wxEvent
{
public:
    wxCloseEvent(wxEventType type = wxEVT_NULL, int id = 0)
        : wxEvent(id, type),
          m_loggingOff(TRUE),
          m_veto(FALSE),      // should be FALSE by default
          m_canVeto(TRUE)
    {
#if WXWIN_COMPATIBILITY
        m_force = FALSE;
#endif // WXWIN_COMPATIBILITY
    }
    wxCloseEvent(const wxCloseEvent & event)
        : wxEvent(event),
		m_loggingOff(event.m_loggingOff),
		m_veto(event.m_veto),
		m_canVeto(event.m_canVeto)
    {
#if WXWIN_COMPATIBILITY
        m_force = event.m_force;
#endif // WXWIN_COMPATIBILITY
	}

    void SetLoggingOff(bool logOff) { m_loggingOff = logOff; }
    bool GetLoggingOff() const { return m_loggingOff; }

    void Veto(bool veto = TRUE)
    {
        // GetVeto() will return FALSE anyhow...
        wxCHECK_RET( m_canVeto,
                     wxT("call to Veto() ignored (can't veto this event)") );

        m_veto = veto;
    }
    void SetCanVeto(bool canVeto) { m_canVeto = canVeto; }
    // No more asserts here please, the one you put here was wrong.
    bool CanVeto() const { return m_canVeto; }
    bool GetVeto() const { return m_canVeto && m_veto; }

#if WXWIN_COMPATIBILITY
    // This is probably obsolete now, since we use CanVeto instead, in
    // both OnCloseWindow and OnQueryEndSession.
    // m_force == ! m_canVeto i.e., can't veto means we must force it to close.
    void SetForce(bool force) { m_force = force; }
    bool GetForce() const { return m_force; }
#endif

    virtual wxEvent *Clone() const { return new wxCloseEvent(*this); }

protected:
    bool m_loggingOff;
    bool m_veto, m_canVeto;

#if WXWIN_COMPATIBILITY
    bool m_force;
#endif

private:
    DECLARE_DYNAMIC_CLASS(wxCloseEvent)

};

/*
 wxEVT_SHOW
 */

class WXDLLEXPORT wxShowEvent : public wxEvent
{
public:
    wxShowEvent(int id = 0, bool show = FALSE)
        : wxEvent(id, wxEVT_SHOW)
        { m_show = show; }
    wxShowEvent(const wxShowEvent & event)
        : wxEvent(event)
	{ m_show = event.m_show; }

    void SetShow(bool show) { m_show = show; }
    bool GetShow() const { return m_show; }

    virtual wxEvent *Clone() const { return new wxShowEvent(*this); }

protected:
    bool m_show;

private:
    DECLARE_DYNAMIC_CLASS(wxShowEvent)
};

/*
 wxEVT_ICONIZE
 */

class WXDLLEXPORT wxIconizeEvent : public wxEvent
{
public:
    wxIconizeEvent(int id = 0, bool iconized = TRUE)
        : wxEvent(id, wxEVT_ICONIZE)
        { m_iconized = iconized; }
    wxIconizeEvent(const wxIconizeEvent & event)
        : wxEvent(event)
	{ m_iconized = event.m_iconized; }

    // return true if the frame was iconized, false if restored
    bool Iconized() const { return m_iconized; }

    virtual wxEvent *Clone() const { return new wxIconizeEvent(*this); }

protected:
    bool m_iconized;

private:
    DECLARE_DYNAMIC_CLASS(wxIconizeEvent)
};
/*
 wxEVT_MAXIMIZE
 */

class WXDLLEXPORT wxMaximizeEvent : public wxEvent
{
public:
    wxMaximizeEvent(int id = 0)
        : wxEvent(id, wxEVT_MAXIMIZE)
        { }

    virtual wxEvent *Clone() const { return new wxMaximizeEvent(*this); }

private:
    DECLARE_DYNAMIC_CLASS(wxMaximizeEvent)
};

// Joystick event class
/*
 wxEVT_JOY_BUTTON_DOWN,
 wxEVT_JOY_BUTTON_UP,
 wxEVT_JOY_MOVE,
 wxEVT_JOY_ZMOVE
*/

// Which joystick? Same as Windows ids so no conversion necessary.
enum
{
    wxJOYSTICK1,
    wxJOYSTICK2
};

// Which button is down?
enum
{
    wxJOY_BUTTON_ANY = -1,
    wxJOY_BUTTON1    = 1,
    wxJOY_BUTTON2    = 2,
    wxJOY_BUTTON3    = 4,
    wxJOY_BUTTON4    = 8
};

class WXDLLEXPORT wxJoystickEvent : public wxEvent
{
public:
    wxPoint   m_pos;
    int       m_zPosition;
    int       m_buttonChange;   // Which button changed?
    int       m_buttonState;    // Which buttons are down?
    int       m_joyStick;       // Which joystick?

    wxJoystickEvent(wxEventType type = wxEVT_NULL,
                    int state = 0,
                    int joystick = wxJOYSTICK1,
                    int change = 0)
        : wxEvent(0, type),
          m_pos(0, 0),
          m_zPosition(0),
          m_buttonChange(change),
          m_buttonState(state),
          m_joyStick(joystick)
    {
    }
    wxJoystickEvent(const wxJoystickEvent & event)
		: wxEvent(event),
		  m_pos(event.m_pos),
		  m_zPosition(event.m_zPosition),
		  m_buttonChange(event.m_buttonChange),
		  m_buttonState(event.m_buttonState),
		  m_joyStick(event.m_joyStick)
    { }

    wxPoint GetPosition() const { return m_pos; }
    int GetZPosition() const { return m_zPosition; }
    int GetButtonState() const { return m_buttonState; }
    int GetButtonChange() const { return m_buttonChange; }
    int GetJoystick() const { return m_joyStick; }

    void SetJoystick(int stick) { m_joyStick = stick; }
    void SetButtonState(int state) { m_buttonState = state; }
    void SetButtonChange(int change) { m_buttonChange = change; }
    void SetPosition(const wxPoint& pos) { m_pos = pos; }
    void SetZPosition(int zPos) { m_zPosition = zPos; }

    // Was it a button event? (*doesn't* mean: is any button *down*?)
    bool IsButton() const { return ((GetEventType() == wxEVT_JOY_BUTTON_DOWN) ||
            (GetEventType() == wxEVT_JOY_BUTTON_UP)); }

    // Was it a move event?
    bool IsMove() const { return (GetEventType() == wxEVT_JOY_MOVE) ; }

    // Was it a zmove event?
    bool IsZMove() const { return (GetEventType() == wxEVT_JOY_ZMOVE) ; }

    // Was it a down event from button 1, 2, 3, 4 or any?
    bool ButtonDown(int but = wxJOY_BUTTON_ANY) const
    { return ((GetEventType() == wxEVT_JOY_BUTTON_DOWN) &&
            ((but == wxJOY_BUTTON_ANY) || (but == m_buttonChange))); }

    // Was it a up event from button 1, 2, 3 or any?
    bool ButtonUp(int but = wxJOY_BUTTON_ANY) const
    { return ((GetEventType() == wxEVT_JOY_BUTTON_UP) &&
            ((but == wxJOY_BUTTON_ANY) || (but == m_buttonChange))); }

    // Was the given button 1,2,3,4 or any in Down state?
    bool ButtonIsDown(int but =  wxJOY_BUTTON_ANY) const
    { return (((but == wxJOY_BUTTON_ANY) && (m_buttonState != 0)) ||
            ((m_buttonState & but) == but)); }

    virtual wxEvent *Clone() const { return new wxJoystickEvent(*this); }

private:
    DECLARE_DYNAMIC_CLASS(wxJoystickEvent)
};

// Drop files event class
/*
 wxEVT_DROP_FILES
 */

class WXDLLEXPORT wxDropFilesEvent : public wxEvent
{
private:
    wxDropFilesEvent& operator=(const wxDropFilesEvent& event);

public:
    int       m_noFiles;
    wxPoint   m_pos;
    wxString* m_files;

    wxDropFilesEvent(wxEventType type = wxEVT_NULL,
                     int noFiles = 0,
                     wxString *files = (wxString *) NULL)
        : wxEvent(0, type),
          m_noFiles(noFiles),
          m_pos(),
          m_files(files)
        { }

    // we need a copy ctor to avoid deleting m_files pointer twice
    wxDropFilesEvent(const wxDropFilesEvent& other)
        : wxEvent(other),
          m_noFiles(other.m_noFiles),
          m_pos(other.m_pos),
          m_files(NULL)
    {
        m_files = new wxString[m_noFiles];
        for ( int n = 0; n < m_noFiles; n++ )
        {
            m_files[n] = other.m_files[n];
        }
    }

    virtual ~wxDropFilesEvent()
    {
        delete [] m_files;
    }

    wxPoint GetPosition() const { return m_pos; }
    int GetNumberOfFiles() const { return m_noFiles; }
    wxString *GetFiles() const { return m_files; }

    virtual wxEvent *Clone() const { return new wxDropFilesEvent(*this); }

private:
    DECLARE_DYNAMIC_CLASS(wxDropFilesEvent)
};

// Update UI event
/*
 wxEVT_UPDATE_UI
 */

class WXDLLEXPORT wxUpdateUIEvent : public wxCommandEvent
{
public:
    wxUpdateUIEvent(wxWindowID commandId = 0)
        : wxCommandEvent(wxEVT_UPDATE_UI, commandId)
    {
        m_checked =
        m_enabled =
        m_setEnabled =
        m_setText =
        m_setChecked = FALSE;
    }
    wxUpdateUIEvent(const wxUpdateUIEvent & event)
        : wxCommandEvent(event),
		  m_checked(event.m_checked),
		  m_enabled(event.m_enabled),
		  m_setEnabled(event.m_setEnabled),
		  m_setText(event.m_setText),
		  m_setChecked(event.m_setChecked),
		  m_text(event.m_text)
	{ }

    bool GetChecked() const { return m_checked; }
    bool GetEnabled() const { return m_enabled; }
    wxString GetText() const { return m_text; }
    bool GetSetText() const { return m_setText; }
    bool GetSetChecked() const { return m_setChecked; }
    bool GetSetEnabled() const { return m_setEnabled; }

    void Check(bool check) { m_checked = check; m_setChecked = TRUE; }
    void Enable(bool enable) { m_enabled = enable; m_setEnabled = TRUE; }
    void SetText(const wxString& text) { m_text = text; m_setText = TRUE; }

    virtual wxEvent *Clone() const { return new wxUpdateUIEvent(*this); }

protected:
    bool          m_checked;
    bool          m_enabled;
    bool          m_setEnabled;
    bool          m_setText;
    bool          m_setChecked;
    wxString      m_text;

private:
    DECLARE_DYNAMIC_CLASS(wxUpdateUIEvent)
};

/*
 wxEVT_SYS_COLOUR_CHANGED
 */

// TODO: shouldn't all events record the window ID?
class WXDLLEXPORT wxSysColourChangedEvent : public wxEvent
{
public:
    wxSysColourChangedEvent()
        : wxEvent(0, wxEVT_SYS_COLOUR_CHANGED)
        { }

    virtual wxEvent *Clone() const { return new wxSysColourChangedEvent(*this); }

private:
    DECLARE_DYNAMIC_CLASS(wxSysColourChangedEvent)
};

/*
 wxEVT_MOUSE_CAPTURE_CHANGED
 The window losing the capture receives this message
 (even if it released the capture itself).
 */

class WXDLLEXPORT wxMouseCaptureChangedEvent : public wxEvent
{
private:
    wxMouseCaptureChangedEvent operator=(const wxMouseCaptureChangedEvent& event);

public:
    wxMouseCaptureChangedEvent(wxWindowID id = 0, wxWindow* gainedCapture = NULL)
        : wxEvent(id, wxEVT_MOUSE_CAPTURE_CHANGED),
          m_gainedCapture(gainedCapture)
        { }

    wxMouseCaptureChangedEvent(const wxMouseCaptureChangedEvent& event)
        : wxEvent(event),
          m_gainedCapture(event.m_gainedCapture)
        { }

    virtual wxEvent *Clone() const { return new wxMouseCaptureChangedEvent(*this); }

    wxWindow* GetCapturedWindow() const { return m_gainedCapture; };

private:
    wxWindow* m_gainedCapture;
    DECLARE_DYNAMIC_CLASS(wxMouseCaptureChangedEvent)
};

/*
 wxEVT_DISPLAY_CHANGED
 */
class WXDLLEXPORT wxDisplayChangedEvent : public wxEvent
{
private:
    DECLARE_DYNAMIC_CLASS(wxDisplayChangedEvent)

public:
    wxDisplayChangedEvent()
        : wxEvent(0, wxEVT_DISPLAY_CHANGED)
        { }

    virtual wxEvent *Clone() const { return new wxDisplayChangedEvent(*this); }
};

/*
 wxEVT_PALETTE_CHANGED
 */

class WXDLLEXPORT wxPaletteChangedEvent : public wxEvent
{
private:
    wxPaletteChangedEvent& operator=(const wxPaletteChangedEvent& event);

public:
    wxPaletteChangedEvent(wxWindowID id = 0)
        : wxEvent(id, wxEVT_PALETTE_CHANGED),
          m_changedWindow((wxWindow *) NULL)
        { }

    wxPaletteChangedEvent(const wxPaletteChangedEvent& event)
        : wxEvent(event),
          m_changedWindow(event.m_changedWindow)
        { }

    void SetChangedWindow(wxWindow* win) { m_changedWindow = win; }
    wxWindow* GetChangedWindow() const { return m_changedWindow; }

    virtual wxEvent *Clone() const { return new wxPaletteChangedEvent(*this); }

protected:
    wxWindow*     m_changedWindow;

private:
    DECLARE_DYNAMIC_CLASS(wxPaletteChangedEvent)
};

/*
 wxEVT_QUERY_NEW_PALETTE
 Indicates the window is getting keyboard focus and should re-do its palette.
 */

class WXDLLEXPORT wxQueryNewPaletteEvent : public wxEvent
{
public:
    wxQueryNewPaletteEvent(wxWindowID id = 0)
        : wxEvent(id, wxEVT_QUERY_NEW_PALETTE),
          m_paletteRealized(FALSE)
        { }
    wxQueryNewPaletteEvent(const wxQueryNewPaletteEvent & event)
        : wxEvent(event),
		m_paletteRealized(event.m_paletteRealized)
	{ }

    // App sets this if it changes the palette.
    void SetPaletteRealized(bool realized) { m_paletteRealized = realized; }
    bool GetPaletteRealized() const { return m_paletteRealized; }

    virtual wxEvent *Clone() const { return new wxQueryNewPaletteEvent(*this); }

protected:
    bool m_paletteRealized;

private:
    DECLARE_DYNAMIC_CLASS(wxQueryNewPaletteEvent)
};

/*
 Event generated by dialog navigation keys
 wxEVT_NAVIGATION_KEY
 */
// NB: don't derive from command event to avoid being propagated to the parent
class WXDLLEXPORT wxNavigationKeyEvent : public wxEvent
{
private:
    wxNavigationKeyEvent& operator=(const wxNavigationKeyEvent& event);

public:
    wxNavigationKeyEvent()
        : wxEvent(0, wxEVT_NAVIGATION_KEY),
          m_flags(IsForward | Propagate),    // defaults are for TAB
          m_focus((wxWindow *)NULL)
        { }

    wxNavigationKeyEvent(const wxNavigationKeyEvent& event)
        : wxEvent(event),
          m_flags(event.m_flags),
          m_focus(event.m_focus)
        { }

    // direction: forward (true) or backward (false)
    bool GetDirection() const
        { return (m_flags & IsForward) != 0; }
    void SetDirection(bool bForward)
        { if ( bForward ) m_flags |= IsForward; else m_flags &= ~IsForward; }

    // it may be a window change event (MDI, notebook pages...) or a control
    // change event
    bool IsWindowChange() const
        { return (m_flags & WinChange) != 0; }
    void SetWindowChange(bool bIs)
        { if ( bIs ) m_flags |= WinChange; else m_flags &= ~WinChange; }

    // some navigation events are meant to be propagated upwards (Windows
    // convention is to do this for TAB events) while others should always
    // cycle inside the panel/radiobox/whatever we're current inside
    bool ShouldPropagate() const
        { return (m_flags & Propagate) != 0; }
    void SetPropagate(bool bDoIt)
        { if ( bDoIt ) m_flags |= Propagate; else m_flags &= ~Propagate; }

    // the child which has the focus currently (may be NULL - use
    // wxWindow::FindFocus then)
    wxWindow* GetCurrentFocus() const { return m_focus; }
    void SetCurrentFocus(wxWindow *win) { m_focus = win; }

    virtual wxEvent *Clone() const { return new wxNavigationKeyEvent(*this); }

private:
    enum
    {
        IsForward = 0x0001,
        WinChange = 0x0002,
        Propagate = 0x0004
    };

    long m_flags;
    wxWindow *m_focus;

private:
    DECLARE_DYNAMIC_CLASS(wxNavigationKeyEvent)
};

// Window creation/destruction events: the first is sent as soon as window is
// created (i.e. the underlying GUI object exists), but when the C++ object is
// fully initialized (so virtual functions may be called). The second,
// wxEVT_DESTROY, is sent right before the window is destroyed - again, it's
// still safe to call virtual functions at this moment
/*
 wxEVT_CREATE
 wxEVT_DESTROY
 */

class WXDLLEXPORT wxWindowCreateEvent : public wxCommandEvent
{
public:
    wxWindowCreateEvent(wxWindow *win = NULL);

    wxWindow *GetWindow() const { return (wxWindow *)GetEventObject(); }

    virtual wxEvent *Clone() const { return new wxWindowCreateEvent(*this); }

private:
    DECLARE_DYNAMIC_CLASS(wxWindowCreateEvent)
};

class WXDLLEXPORT wxWindowDestroyEvent : public wxCommandEvent
{
public:
    wxWindowDestroyEvent(wxWindow *win = NULL);

    wxWindow *GetWindow() const { return (wxWindow *)GetEventObject(); }

    virtual wxEvent *Clone() const { return new wxWindowDestroyEvent(*this); }

private:
    DECLARE_DYNAMIC_CLASS(wxWindowDestroyEvent)
};

// A help event is sent when the user clicks on a window in context-help mode.
/*
 wxEVT_HELP
 wxEVT_DETAILED_HELP
*/

class WXDLLEXPORT wxHelpEvent : public wxCommandEvent
{
public:
    wxHelpEvent(wxEventType type = wxEVT_NULL,
                wxWindowID id = 0,
                const wxPoint& pt = wxDefaultPosition)
        : wxCommandEvent(type, id),
          m_pos(pt), m_target(), m_link()
    { }
    wxHelpEvent(const wxHelpEvent & event)
        : wxCommandEvent(event),
		  m_pos(event.m_pos),
		  m_target(event.m_target),
		  m_link(event.m_link)
    { }

    // Position of event (in screen coordinates)
    const wxPoint& GetPosition() const { return m_pos; }
    void SetPosition(const wxPoint& pos) { m_pos = pos; }

    // Optional link to further help
    const wxString& GetLink() const { return m_link; }
    void SetLink(const wxString& link) { m_link = link; }

    // Optional target to display help in. E.g. a window specification
    const wxString& GetTarget() const { return m_target; }
    void SetTarget(const wxString& target) { m_target = target; }

    virtual wxEvent *Clone() const { return new wxHelpEvent(*this); }

protected:
    wxPoint   m_pos;
    wxString  m_target;
    wxString  m_link;

private:
    DECLARE_DYNAMIC_CLASS(wxHelpEvent)
};

// A Context event is sent when the user right clicks on a window or
// presses Shift-F10
// NOTE : Under windows this is a repackaged WM_CONTETXMENU message
//        Under other systems it may have to be generated from a right click event
/*
 wxEVT_CONTEXT_MENU
*/

class WXDLLEXPORT wxContextMenuEvent : public wxCommandEvent
{
public:
    wxContextMenuEvent(wxEventType type = wxEVT_NULL,
                       wxWindowID id = 0,
                       const wxPoint& pt = wxDefaultPosition)
        : wxCommandEvent(type, id),
          m_pos(pt)
    { }
    wxContextMenuEvent(const wxContextMenuEvent & event)
        : wxCommandEvent(event),
		m_pos(event.m_pos)
    { }

    // Position of event (in screen coordinates)
    const wxPoint& GetPosition() const { return m_pos; }
    void SetPosition(const wxPoint& pos) { m_pos = pos; }

    virtual wxEvent *Clone() const { return new wxContextMenuEvent(*this); }

protected:
    wxPoint   m_pos;

private:
    DECLARE_DYNAMIC_CLASS(wxContextMenuEvent)
};

// Idle event
/*
 wxEVT_IDLE
 */

class WXDLLEXPORT wxIdleEvent : public wxEvent
{
public:
    wxIdleEvent()
        : wxEvent(0, wxEVT_IDLE),
          m_requestMore(FALSE)
        { }
    wxIdleEvent(const wxIdleEvent & event)
        : wxEvent(event),
		m_requestMore(event.m_requestMore)
	{ }

    void RequestMore(bool needMore = TRUE) { m_requestMore = needMore; }
    bool MoreRequested() const { return m_requestMore; }

    virtual wxEvent *Clone() const { return new wxIdleEvent(*this); }

protected:
    bool m_requestMore;

private:
    DECLARE_DYNAMIC_CLASS(wxIdleEvent)
};

#endif // wxUSE_GUI

/* TODO
 wxEVT_POWER,
 wxEVT_MOUSE_CAPTURE_CHANGED,
 wxEVT_SETTING_CHANGED, // WM_WININICHANGE (NT) / WM_SETTINGCHANGE (Win95)
// wxEVT_FONT_CHANGED,  // WM_FONTCHANGE: roll into wxEVT_SETTING_CHANGED, but remember to propagate
                        // wxEVT_FONT_CHANGED to all other windows (maybe).
 wxEVT_DRAW_ITEM, // Leave these three as virtual functions in wxControl?? Platform-specific.
 wxEVT_MEASURE_ITEM,
 wxEVT_COMPARE_ITEM
*/


// ============================================================================
// event handler and related classes
// ============================================================================

typedef void (wxObject::*wxObjectEventFunction)(wxEvent&);

// we can't have ctors nor base struct in backwards compatibility mode or
// otherwise we won't be able to initialize the objects with an agregate, so
// we have to keep both versions
#if WXWIN_COMPATIBILITY_EVENT_TYPES

struct WXDLLEXPORT wxEventTableEntry
{
    // For some reason, this can't be wxEventType, or VC++ complains.
    int m_eventType;            // main event type
    int m_id;                   // control/menu/toolbar id
    int m_lastId;               // used for ranges of ids
    wxObjectEventFunction m_fn; // function to call: not wxEventFunction,
                                // because of dependency problems

    wxObject* m_callbackUserData;
};

#else // !WXWIN_COMPATIBILITY_EVENT_TYPES

// struct containing the members common to static and dynamic event tables
// entries
struct WXDLLEXPORT wxEventTableEntryBase
{
private:
    wxEventTableEntryBase& operator=(const wxEventTableEntryBase& event);

public:
    wxEventTableEntryBase(int id, int idLast,
                          wxObjectEventFunction fn, wxObject *data)
        : m_id(id),
          m_lastId(idLast),
          m_fn(fn),
          m_callbackUserData(data)
    { }

    wxEventTableEntryBase(const wxEventTableEntryBase& event)
        : m_id(event.m_id),
          m_lastId(event.m_lastId),
          m_fn(event.m_fn),
          m_callbackUserData(event.m_callbackUserData)
    { }

    // the range of ids for this entry: if m_lastId == wxID_ANY, the range
    // consists only of m_id, otherwise it is m_id..m_lastId inclusive
    int m_id,
        m_lastId;

    // function to call: not wxEventFunction, because of dependency problems
    wxObjectEventFunction m_fn;

    // arbitrary user data asosciated with the callback
    wxObject* m_callbackUserData;
};

// an entry from a static event table
struct WXDLLEXPORT wxEventTableEntry : public wxEventTableEntryBase
{
    wxEventTableEntry(const int& evType, int id, int idLast,
                      wxObjectEventFunction fn, wxObject *data)
        : wxEventTableEntryBase(id, idLast, fn, data),
        m_eventType(evType)
    { }

    // the reference to event type: this allows us to not care about the
    // (undefined) order in which the event table entries and the event types
    // are initialized: initially the value of this reference might be
    // invalid, but by the time it is used for the first time, all global
    // objects will have been initialized (including the event type constants)
    // and so it will have the correct value when it is needed
    const int& m_eventType;
};

// an entry used in dynamic event table managed by wxEvtHandler::Connect()
struct WXDLLEXPORT wxDynamicEventTableEntry : public wxEventTableEntryBase
{
    wxDynamicEventTableEntry(int evType, int id, int idLast,
                             wxObjectEventFunction fn, wxObject *data)
        : wxEventTableEntryBase(id, idLast, fn, data),
          m_eventType(evType)
    { }

    // not a reference here as we can't keep a reference to a temporary int
    // created to wrap the constant value typically passed to Connect() - nor
    // do we need it
    int m_eventType;
};

#endif // !WXWIN_COMPATIBILITY_EVENT_TYPES

// ----------------------------------------------------------------------------
// wxEventTable: an array of event entries terminated with {0, 0, 0, 0, 0}
// ----------------------------------------------------------------------------
struct WXDLLEXPORT wxEventTable
{
    const wxEventTable *baseTable;    // base event table (next in chain)
    const wxEventTableEntry *entries; // bottom of entry array
};

// ----------------------------------------------------------------------------
// wxEvtHandler: the base class for all objects handling wxWindows events
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxEvtHandler : public wxObject
{
public:
    wxEvtHandler();
    virtual ~wxEvtHandler();

    wxEvtHandler *GetNextHandler() const { return m_nextHandler; }
    wxEvtHandler *GetPreviousHandler() const { return m_previousHandler; }
    void SetNextHandler(wxEvtHandler *handler) { m_nextHandler = handler; }
    void SetPreviousHandler(wxEvtHandler *handler) { m_previousHandler = handler; }

    void SetEvtHandlerEnabled(bool enabled) { m_enabled = enabled; }
    bool GetEvtHandlerEnabled() const { return m_enabled; }

    // process an event right now
    virtual bool ProcessEvent(wxEvent& event);

    // add an event to be processed later
    void AddPendingEvent(wxEvent& event);

    // process all pending events
    void ProcessPendingEvents();

    // add a
#if wxUSE_THREADS
    bool ProcessThreadEvent(wxEvent& event);
#endif

    // Dynamic association of a member function handler with the event handler,
    // id and event type
    void Connect( int id, int lastId, int eventType,
                  wxObjectEventFunction func,
                  wxObject *userData = (wxObject *) NULL );

    // Convenience function: take just one id
    void Connect( int id, int eventType,
                  wxObjectEventFunction func,
                  wxObject *userData = (wxObject *) NULL )
        { Connect(id, wxID_ANY, eventType, func, userData); }

    bool Disconnect( int id, int lastId, wxEventType eventType,
                  wxObjectEventFunction func = NULL,
                  wxObject *userData = (wxObject *) NULL );

    // Convenience function: take just one id
    bool Disconnect( int id, wxEventType eventType = wxEVT_NULL,
                  wxObjectEventFunction func = NULL,
                  wxObject *userData = (wxObject *) NULL )
        { return Disconnect(id, wxID_ANY, eventType, func, userData); }


    // User data can be associated with each wxEvtHandler
    void SetClientObject( wxClientData *data ) { DoSetClientObject(data); }
    wxClientData *GetClientObject() const { return DoGetClientObject(); }

    void SetClientData( void *data ) { DoSetClientData(data); }
    void *GetClientData() const { return DoGetClientData(); }


    // implementation from now on
    virtual bool SearchEventTable(wxEventTable& table, wxEvent& event);
    bool SearchDynamicEventTable( wxEvent& event );

#if wxUSE_THREADS
    void ClearEventLocker()
   {
   };
#endif

    // old stuff

#if WXWIN_COMPATIBILITY_2
    virtual void OnCommand(wxWindow& WXUNUSED(win),
                           wxCommandEvent& WXUNUSED(event))
    {
        wxFAIL_MSG(wxT("shouldn't be called any more"));
    }

    // Called if child control has no callback function
    virtual long Default()
        { return GetNextHandler() ? GetNextHandler()->Default() : 0; };
#endif // WXWIN_COMPATIBILITY_2

#if WXWIN_COMPATIBILITY
    virtual bool OnClose();
#endif

private:
    static const wxEventTableEntry sm_eventTableEntries[];

protected:
    static const wxEventTable sm_eventTable;

    virtual const wxEventTable *GetEventTable() const;

    wxEvtHandler*       m_nextHandler;
    wxEvtHandler*       m_previousHandler;
    wxList*             m_dynamicEvents;
    wxList*             m_pendingEvents;

#if wxUSE_THREADS
    wxCriticalSection   m_eventsLocker;
#endif

    // optimization: instead of using costly IsKindOf() to decide whether we're
    // a window (which is true in 99% of cases), use this flag
    bool                m_isWindow;

    // Is event handler enabled?
    bool                m_enabled;


    // The user data: either an object which will be deleted by the container
    // when it's deleted or some raw pointer which we do nothing with - only
    // one type of data can be used with the given window (i.e. you cannot set
    // the void data and then associate the container with wxClientData or vice
    // versa)
    union
    {
        wxClientData *m_clientObject;
        void         *m_clientData;
    };

    // what kind of data do we have?
    wxClientDataType m_clientDataType;

    // client data accessors
    virtual void DoSetClientObject( wxClientData *data );
    virtual wxClientData *DoGetClientObject() const;

    virtual void DoSetClientData( void *data );
    virtual void *DoGetClientData() const;

private:
    DECLARE_NO_COPY_CLASS(wxEvtHandler)
    DECLARE_DYNAMIC_CLASS(wxEvtHandler)
};

typedef void (wxEvtHandler::*wxEventFunction)(wxEvent&);
#if wxUSE_GUI
typedef void (wxEvtHandler::*wxCommandEventFunction)(wxCommandEvent&);
typedef void (wxEvtHandler::*wxScrollEventFunction)(wxScrollEvent&);
typedef void (wxEvtHandler::*wxScrollWinEventFunction)(wxScrollWinEvent&);
typedef void (wxEvtHandler::*wxSizeEventFunction)(wxSizeEvent&);
typedef void (wxEvtHandler::*wxMoveEventFunction)(wxMoveEvent&);
typedef void (wxEvtHandler::*wxPaintEventFunction)(wxPaintEvent&);
typedef void (wxEvtHandler::*wxEraseEventFunction)(wxEraseEvent&);
typedef void (wxEvtHandler::*wxMouseEventFunction)(wxMouseEvent&);
typedef void (wxEvtHandler::*wxCharEventFunction)(wxKeyEvent&);
typedef void (wxEvtHandler::*wxFocusEventFunction)(wxFocusEvent&);
typedef void (wxEvtHandler::*wxChildFocusEventFunction)(wxChildFocusEvent&);
typedef void (wxEvtHandler::*wxActivateEventFunction)(wxActivateEvent&);
typedef void (wxEvtHandler::*wxMenuEventFunction)(wxMenuEvent&);
typedef void (wxEvtHandler::*wxJoystickEventFunction)(wxJoystickEvent&);
typedef void (wxEvtHandler::*wxDropFilesEventFunction)(wxDropFilesEvent&);
typedef void (wxEvtHandler::*wxInitDialogEventFunction)(wxInitDialogEvent&);
typedef void (wxEvtHandler::*wxSysColourChangedFunction)(wxSysColourChangedEvent&);
typedef void (wxEvtHandler::*wxDisplayChangedFunction)(wxDisplayChangedEvent&);
typedef void (wxEvtHandler::*wxUpdateUIEventFunction)(wxUpdateUIEvent&);
typedef void (wxEvtHandler::*wxIdleEventFunction)(wxIdleEvent&);
typedef void (wxEvtHandler::*wxCloseEventFunction)(wxCloseEvent&);
typedef void (wxEvtHandler::*wxShowEventFunction)(wxShowEvent&);
typedef void (wxEvtHandler::*wxIconizeEventFunction)(wxIconizeEvent&);
typedef void (wxEvtHandler::*wxMaximizeEventFunction)(wxMaximizeEvent&);
typedef void (wxEvtHandler::*wxNavigationKeyEventFunction)(wxNavigationKeyEvent&);
typedef void (wxEvtHandler::*wxPaletteChangedEventFunction)(wxPaletteChangedEvent&);
typedef void (wxEvtHandler::*wxQueryNewPaletteEventFunction)(wxQueryNewPaletteEvent&);
typedef void (wxEvtHandler::*wxWindowCreateEventFunction)(wxWindowCreateEvent&);
typedef void (wxEvtHandler::*wxWindowDestroyEventFunction)(wxWindowDestroyEvent&);
typedef void (wxEvtHandler::*wxSetCursorEventFunction)(wxSetCursorEvent&);
typedef void (wxEvtHandler::*wxNotifyEventFunction)(wxNotifyEvent&);
typedef void (wxEvtHandler::*wxHelpEventFunction)(wxHelpEvent&);
typedef void (wxEvtHandler::*wxContextMenuEventFunction)(wxContextMenuEvent&);
typedef void (wxEvtHandler::*wxMouseCaptureChangedEventFunction)(wxMouseCaptureChangedEvent&);
#endif // wxUSE_GUI

// N.B. In GNU-WIN32, you *have* to take the address of a member function
// (use &) or the compiler crashes...

#define DECLARE_EVENT_TABLE() \
    private: \
        static const wxEventTableEntry sm_eventTableEntries[]; \
    protected: \
        static const wxEventTable        sm_eventTable; \
        virtual const wxEventTable*        GetEventTable() const;

#define BEGIN_EVENT_TABLE(theClass, baseClass) \
    const wxEventTable *theClass::GetEventTable() const \
        { return &theClass::sm_eventTable; } \
    const wxEventTable theClass::sm_eventTable = \
        { &baseClass::sm_eventTable, &theClass::sm_eventTableEntries[0] }; \
    const wxEventTableEntry theClass::sm_eventTableEntries[] = { \

#define END_EVENT_TABLE() DECLARE_EVENT_TABLE_ENTRY( wxEVT_NULL, 0, 0, 0, 0 ) };

/*
 * Event table macros
 */

// Generic events
#define EVT_CUSTOM(event, id, func) DECLARE_EVENT_TABLE_ENTRY( event, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) & func, (wxObject *) NULL ),
#define EVT_CUSTOM_RANGE(event, id1, id2, func) DECLARE_EVENT_TABLE_ENTRY( event, id1, id2, (wxObjectEventFunction) (wxEventFunction) & func, (wxObject *) NULL ),

// Miscellaneous
#define EVT_SIZE(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SIZE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxSizeEventFunction) & func, (wxObject *) NULL ),
#define EVT_MOVE(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_MOVE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMoveEventFunction) & func, (wxObject *) NULL ),
#define EVT_CLOSE(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_CLOSE_WINDOW, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCloseEventFunction) & func, (wxObject *) NULL ),
#define EVT_END_SESSION(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_END_SESSION, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCloseEventFunction) & func, (wxObject *) NULL ),
#define EVT_QUERY_END_SESSION(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_QUERY_END_SESSION, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCloseEventFunction) & func, (wxObject *) NULL ),
#define EVT_PAINT(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_PAINT, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxPaintEventFunction) & func, (wxObject *) NULL ),
#define EVT_NC_PAINT(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_NC_PAINT, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxPaintEventFunction) & func, (wxObject *) NULL ),
#define EVT_ERASE_BACKGROUND(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_ERASE_BACKGROUND, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxEraseEventFunction) & func, (wxObject *) NULL ),
#define EVT_CHAR(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_CHAR, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCharEventFunction) & func, (wxObject *) NULL ),
#define EVT_KEY_DOWN(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_KEY_DOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCharEventFunction) & func, (wxObject *) NULL ),
#define EVT_KEY_UP(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_KEY_UP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCharEventFunction) & func, (wxObject *) NULL ),
#define EVT_CHAR_HOOK(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_CHAR_HOOK, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCharEventFunction) & func, NULL ),
#define EVT_MENU_OPEN(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_MENU_OPEN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMenuEventFunction) & func, (wxObject *) NULL ),
#define EVT_MENU_CLOSE(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_MENU_CLOSE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMenuEventFunction) & func, (wxObject *) NULL ),
#define EVT_MENU_HIGHLIGHT(id, func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_MENU_HIGHLIGHT, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMenuEventFunction) & func, (wxObject *) NULL ),
#define EVT_MENU_HIGHLIGHT_ALL(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_MENU_HIGHLIGHT, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMenuEventFunction) & func, (wxObject *) NULL ),
#define EVT_SET_FOCUS(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SET_FOCUS, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxFocusEventFunction) & func, (wxObject *) NULL ),
#define EVT_KILL_FOCUS(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_KILL_FOCUS, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxFocusEventFunction) & func, (wxObject *) NULL ),
#define EVT_CHILD_FOCUS(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_CHILD_FOCUS, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) (wxChildFocusEventFunction) & func, (wxObject *) NULL ),
#define EVT_ACTIVATE(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_ACTIVATE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxActivateEventFunction) & func, (wxObject *) NULL ),
#define EVT_ACTIVATE_APP(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_ACTIVATE_APP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxActivateEventFunction) & func, (wxObject *) NULL ),
#define EVT_END_SESSION(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_END_SESSION, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCloseEventFunction) & func, (wxObject *) NULL ),
#define EVT_QUERY_END_SESSION(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_QUERY_END_SESSION, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCloseEventFunction) & func, (wxObject *) NULL ),
#define EVT_DROP_FILES(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_DROP_FILES, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxDropFilesEventFunction) & func, (wxObject *) NULL ),
#define EVT_INIT_DIALOG(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_INIT_DIALOG, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxInitDialogEventFunction) & func, (wxObject *) NULL ),
#define EVT_SYS_COLOUR_CHANGED(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SYS_COLOUR_CHANGED, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxSysColourChangedFunction) & func, (wxObject *) NULL ),
#define EVT_DISPLAY_CHANGED(func)  DECLARE_EVENT_TABLE_ENTRY( wxEVT_DISPLAY_CHANGED, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxDisplayChangedFunction) & func, (wxObject *) NULL ),
#define EVT_SHOW(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SHOW, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxShowEventFunction) & func, (wxObject *) NULL ),
#define EVT_MAXIMIZE(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_MAXIMIZE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMaximizeEventFunction) & func, (wxObject *) NULL ),
#define EVT_ICONIZE(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_ICONIZE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxIconizeEventFunction) & func, (wxObject *) NULL ),
#define EVT_NAVIGATION_KEY(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_NAVIGATION_KEY, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) (wxNavigationKeyEventFunction) & func, (wxObject *) NULL ),
#define EVT_PALETTE_CHANGED(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_PALETTE_CHANGED, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxPaletteChangedEventFunction) & func, (wxObject *) NULL ),
#define EVT_QUERY_NEW_PALETTE(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_QUERY_NEW_PALETTE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxQueryNewPaletteEventFunction) & func, (wxObject *) NULL ),
#define EVT_WINDOW_CREATE(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_CREATE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxWindowCreateEventFunction) & func, (wxObject *) NULL ),
#define EVT_WINDOW_DESTROY(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_DESTROY, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxWindowDestroyEventFunction) & func, (wxObject *) NULL ),
#define EVT_SET_CURSOR(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SET_CURSOR, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxSetCursorEventFunction) & func, (wxObject *) NULL ),
#define EVT_MOUSE_CAPTURE_CHANGED(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_MOUSE_CAPTURE_CHANGED, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseCaptureChangedEventFunction) & func, (wxObject *) NULL ),

// Mouse events
#define EVT_LEFT_DOWN(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_LEFT_DOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),
#define EVT_LEFT_UP(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_LEFT_UP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),
#define EVT_MIDDLE_DOWN(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_MIDDLE_DOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),
#define EVT_MIDDLE_UP(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_MIDDLE_UP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),
#define EVT_RIGHT_DOWN(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_RIGHT_DOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),
#define EVT_RIGHT_UP(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_RIGHT_UP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),
#define EVT_MOTION(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_MOTION, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),
#define EVT_LEFT_DCLICK(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_LEFT_DCLICK, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),
#define EVT_MIDDLE_DCLICK(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_MIDDLE_DCLICK, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),
#define EVT_RIGHT_DCLICK(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_RIGHT_DCLICK, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),
#define EVT_LEAVE_WINDOW(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_LEAVE_WINDOW, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),
#define EVT_ENTER_WINDOW(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_ENTER_WINDOW, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),
#define EVT_MOUSEWHEEL(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_MOUSEWHEEL, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),

// All mouse events
#define EVT_MOUSE_EVENTS(func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_LEFT_DOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_LEFT_UP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_MIDDLE_DOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_MIDDLE_UP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_RIGHT_DOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_RIGHT_UP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_MOTION, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_LEFT_DCLICK, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_MIDDLE_DCLICK, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_RIGHT_DCLICK, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_ENTER_WINDOW, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_LEAVE_WINDOW, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_MOUSEWHEEL, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxMouseEventFunction) & func, (wxObject *) NULL ),

// EVT_COMMAND
#define EVT_COMMAND(id, event, fn)  DECLARE_EVENT_TABLE_ENTRY( event, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_COMMAND_RANGE(id1, id2, event, fn)  DECLARE_EVENT_TABLE_ENTRY( event, id1, id2, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),

// Scrolling from wxWindow (sent to wxScrolledWindow)
#define EVT_SCROLLWIN(func) \
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_TOP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_BOTTOM, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_LINEUP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_LINEDOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_PAGEUP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_PAGEDOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_THUMBTRACK, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_THUMBRELEASE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),

#define EVT_SCROLLWIN_TOP(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_TOP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLLWIN_BOTTOM(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_BOTTOM, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLLWIN_LINEUP(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_LINEUP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLLWIN_LINEDOWN(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_LINEDOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLLWIN_PAGEUP(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_PAGEUP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLLWIN_PAGEDOWN(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_PAGEDOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLLWIN_THUMBTRACK(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_THUMBTRACK, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLLWIN_THUMBRELEASE(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLLWIN_THUMBRELEASE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollWinEventFunction) & func, (wxObject *) NULL ),

// Scrolling from wxSlider and wxScrollBar
#define EVT_SCROLL(func) \
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_TOP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_BOTTOM, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_LINEUP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_LINEDOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_PAGEUP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_PAGEDOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_THUMBTRACK, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_THUMBRELEASE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ), \
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_ENDSCROLL, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),

#define EVT_SCROLL_TOP(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_TOP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLL_BOTTOM(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_BOTTOM, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLL_LINEUP(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_LINEUP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLL_LINEDOWN(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_LINEDOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLL_PAGEUP(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_PAGEUP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLL_PAGEDOWN(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_PAGEDOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLL_THUMBTRACK(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_THUMBTRACK, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLL_THUMBRELEASE(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_THUMBRELEASE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_SCROLL_ENDSCROLL(func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_ENDSCROLL, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),

// Scrolling from wxSlider and wxScrollBar, with an id
#define EVT_COMMAND_SCROLL(id, func) \
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_TOP, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_BOTTOM, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_LINEUP, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_LINEDOWN, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_PAGEUP, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_PAGEDOWN, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_THUMBTRACK, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),\
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_THUMBRELEASE, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ), \
  DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_ENDSCROLL, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),

#define EVT_COMMAND_SCROLL_TOP(id, func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_TOP, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_COMMAND_SCROLL_BOTTOM(id, func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_BOTTOM, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_COMMAND_SCROLL_LINEUP(id, func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_LINEUP, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_COMMAND_SCROLL_LINEDOWN(id, func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_LINEDOWN, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_COMMAND_SCROLL_PAGEUP(id, func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_PAGEUP, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_COMMAND_SCROLL_PAGEDOWN(id, func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_PAGEDOWN, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_COMMAND_SCROLL_THUMBTRACK(id, func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_THUMBTRACK, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_COMMAND_SCROLL_THUMBRELEASE(id, func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_THUMBRELEASE, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),
#define EVT_COMMAND_SCROLL_ENDSCROLL(id, func) DECLARE_EVENT_TABLE_ENTRY( wxEVT_SCROLL_ENDSCROLL, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxScrollEventFunction) & func, (wxObject *) NULL ),

// Convenience macros for commonly-used commands
#define EVT_BUTTON(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_BUTTON_CLICKED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_CHECKBOX(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_CHECKBOX_CLICKED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_CHOICE(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_CHOICE_SELECTED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_LISTBOX(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_LISTBOX_SELECTED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_LISTBOX_DCLICK(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_LISTBOX_DOUBLECLICKED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_MENU(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_MENU_SELECTED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_MENU_RANGE(id1, id2, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_MENU_SELECTED, id1, id2, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_SLIDER(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_SLIDER_UPDATED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_RADIOBOX(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_RADIOBOX_SELECTED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_RADIOBUTTON(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_RADIOBUTTON_SELECTED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
// EVT_SCROLLBAR is now obsolete since we use EVT_COMMAND_SCROLL... events
#define EVT_SCROLLBAR(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_SCROLLBAR_UPDATED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_VLBOX(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_VLBOX_SELECTED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_COMBOBOX(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_COMBOBOX_SELECTED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_TOOL(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_TOOL_CLICKED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_TOOL_RANGE(id1, id2, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_TOOL_CLICKED, id1, id2, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_TOOL_RCLICKED(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_TOOL_RCLICKED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_TOOL_RCLICKED_RANGE(id1, id2, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_TOOL_RCLICKED, id1, id2, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_TOOL_ENTER(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_TOOL_ENTER, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_CHECKLISTBOX(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_CHECKLISTBOX_TOGGLED, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),

// Generic command events
#define EVT_COMMAND_LEFT_CLICK(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_LEFT_CLICK, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_COMMAND_LEFT_DCLICK(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_LEFT_DCLICK, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_COMMAND_RIGHT_CLICK(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_RIGHT_CLICK, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_COMMAND_RIGHT_DCLICK(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_RIGHT_DCLICK, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_COMMAND_SET_FOCUS(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_SET_FOCUS, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_COMMAND_KILL_FOCUS(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_KILL_FOCUS, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),
#define EVT_COMMAND_ENTER(id, fn) DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_ENTER, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) & fn, (wxObject *) NULL ),

// Joystick events

#define EVT_JOY_BUTTON_DOWN(func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_JOY_BUTTON_DOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxJoystickEventFunction) & func, (wxObject *) NULL ),
#define EVT_JOY_BUTTON_UP(func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_JOY_BUTTON_UP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxJoystickEventFunction) & func, (wxObject *) NULL ),
#define EVT_JOY_MOVE(func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_JOY_MOVE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxJoystickEventFunction) & func, (wxObject *) NULL ),
#define EVT_JOY_ZMOVE(func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_JOY_ZMOVE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxJoystickEventFunction) & func, (wxObject *) NULL ),

// These are obsolete, see _BUTTON_ events
#define EVT_JOY_DOWN(func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_JOY_BUTTON_DOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxJoystickEventFunction) & func, (wxObject *) NULL ),
#define EVT_JOY_UP(func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_JOY_BUTTON_UP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxJoystickEventFunction) & func, (wxObject *) NULL ),

// All joystick events
#define EVT_JOYSTICK_EVENTS(func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_JOY_BUTTON_DOWN, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxJoystickEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_JOY_BUTTON_UP, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxJoystickEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_JOY_MOVE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxJoystickEventFunction) & func, (wxObject *) NULL ),\
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_JOY_ZMOVE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxJoystickEventFunction) & func, (wxObject *) NULL ),

// Idle event
#define EVT_IDLE(func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_IDLE, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxIdleEventFunction) & func, (wxObject *) NULL ),

// Update UI event
#define EVT_UPDATE_UI(id, func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_UPDATE_UI, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxUpdateUIEventFunction) & func, (wxObject *) NULL ),
#define EVT_UPDATE_UI_RANGE(id1, id2, func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_UPDATE_UI, id1, id2, (wxObjectEventFunction)(wxEventFunction)(wxUpdateUIEventFunction)&func, (wxObject *) NULL ),

// Help events
#define EVT_HELP(id, func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_HELP, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) (wxHelpEventFunction) & func, (wxObject *) NULL ),

#define EVT_HELP_RANGE(id1, id2, func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_HELP, id1, id2, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) (wxHelpEventFunction) & func, (wxObject *) NULL ),

#define EVT_DETAILED_HELP(id, func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_DETAILED_HELP, id, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) (wxHelpEventFunction) & func, (wxObject *) NULL ),

#define EVT_DETAILED_HELP_RANGE(id1, id2, func) \
 DECLARE_EVENT_TABLE_ENTRY( wxEVT_DETAILED_HELP, id1, id2, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) (wxHelpEventFunction) & func, (wxObject *) NULL ),

// Context Menu Events
#define EVT_CONTEXT_MENU(func) \
 DECLARE_EVENT_TABLE_ENTRY(wxEVT_CONTEXT_MENU, wxID_ANY, wxID_ANY, (wxObjectEventFunction) (wxEventFunction) (wxContextMenuEventFunction) & func, (wxObject *) NULL ),

// ----------------------------------------------------------------------------
// Global data
// ----------------------------------------------------------------------------

// for pending event processing - notice that there is intentionally no
// WXDLLEXPORT here
extern wxList *wxPendingEvents;
#if wxUSE_THREADS
    extern wxCriticalSection *wxPendingEventsLocker;
#endif

// ----------------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------------

#if wxUSE_GUI

// Find a window with the focus, that is also a descendant of the given window.
// This is used to determine the window to initially send commands to.
wxWindow* wxFindFocusDescendant(wxWindow* ancestor);

#endif // wxUSE_GUI

#endif
        // _WX_EVENTH__
