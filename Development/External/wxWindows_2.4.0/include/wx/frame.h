/////////////////////////////////////////////////////////////////////////////
// Name:        wx/frame.h
// Purpose:     wxFrame class interface
// Author:      Vadim Zeitlin
// Modified by:
// Created:     15.11.99
// RCS-ID:      $Id: frame.h,v 1.28.2.1 2002/10/02 23:33:01 VZ Exp $
// Copyright:   (c) wxWindows team
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FRAME_H_BASE_
#define _WX_FRAME_H_BASE_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "framebase.h"
#endif

#include "wx/toplevel.h"      // the base class

// the default names for various classs
WXDLLEXPORT_DATA(extern const wxChar*) wxFrameNameStr;
WXDLLEXPORT_DATA(extern const wxChar*) wxStatusLineNameStr;
WXDLLEXPORT_DATA(extern const wxChar*) wxToolBarNameStr;

class WXDLLEXPORT wxFrame;
class WXDLLEXPORT wxMenuBar;
class WXDLLEXPORT wxStatusBar;
class WXDLLEXPORT wxToolBar;

// ----------------------------------------------------------------------------
// wxFrame is a top-level window with optional menubar, statusbar and toolbar
//
// For each of *bars, a frame may have several of them, but only one is
// managed by the frame, i.e. resized/moved when the frame is and whose size
// is accounted for in client size calculations - all others should be taken
// care of manually. The CreateXXXBar() functions create this, main, XXXBar,
// but the actual creation is done in OnCreateXXXBar() functions which may be
// overridden to create custom objects instead of standard ones when
// CreateXXXBar() is called.
// ----------------------------------------------------------------------------

// FIXME - temporary hack in absence of wxTLW !!
#ifndef wxTopLevelWindowNative
class WXDLLEXPORT wxFrameBase : public wxTopLevelWindowBase
#else
class WXDLLEXPORT wxFrameBase : public wxTopLevelWindow
#endif
{
public:
    // construction
    wxFrameBase();
    virtual ~wxFrameBase();

    wxFrame *New(wxWindow *parent,
                 wxWindowID id,
                 const wxString& title,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxDEFAULT_FRAME_STYLE,
                 const wxString& name = wxFrameNameStr);

    // frame state
    // -----------

    // get the origin of the client area (which may be different from (0, 0)
    // if the frame has a toolbar) in client coordinates
    virtual wxPoint GetClientAreaOrigin() const;

    // sends a size event to the window using its current size -- this has an
    // effect of refreshing the window layout
    //
    // currently it is only implemented under MSW but is declared here to make
    // it possible to call it in portable code without using #ifdef's
    virtual void SendSizeEvent() { }

    // menu bar functions
    // ------------------

#if wxUSE_MENUS
    virtual void SetMenuBar(wxMenuBar *menubar);
    virtual wxMenuBar *GetMenuBar() const { return m_frameMenuBar; }
#endif // wxUSE_MENUS

#ifdef WXWIN_COMPATIBILITY_2_2
    // call this to simulate a menu command
    bool Command(int id) { return ProcessCommand(id); }
#endif // WXWIN_COMPATIBILITY_2_2

    // process menu command: returns TRUE if processed
    bool ProcessCommand(int id);

    // status bar functions
    // --------------------
#if wxUSE_STATUSBAR
    // create the main status bar by calling OnCreateStatusBar()
    virtual wxStatusBar* CreateStatusBar(int number = 1,
                                         long style = wxST_SIZEGRIP,
                                         wxWindowID id = 0,
                                         const wxString& name =
                                            wxStatusLineNameStr);
    // return a new status bar
    virtual wxStatusBar *OnCreateStatusBar(int number,
                                           long style,
                                           wxWindowID id,
                                           const wxString& name);
    // get the main status bar
    virtual wxStatusBar *GetStatusBar() const { return m_frameStatusBar; }

    // sets the main status bar
    void SetStatusBar(wxStatusBar *statBar) { m_frameStatusBar = statBar; }

    // forward these to status bar
    virtual void SetStatusText(const wxString &text, int number = 0);
    virtual void SetStatusWidths(int n, const int widths_field[]);
    void PushStatusText(const wxString &text, int number = 0);
    void PopStatusText(int number = 0);

    // set the status bar pane the help will be shown in
    void SetStatusBarPane(int n) { m_statusBarPane = n; }
    int GetStatusBarPane() const { return m_statusBarPane; }
#endif // wxUSE_STATUSBAR

    // toolbar functions
    // -----------------
#if wxUSE_TOOLBAR
    // create main toolbar bycalling OnCreateToolBar()
    virtual wxToolBar* CreateToolBar(long style = wxNO_BORDER|wxTB_HORIZONTAL,
                                     wxWindowID id = -1,
                                     const wxString& name = wxToolBarNameStr);
    // return a new toolbar
    virtual wxToolBar *OnCreateToolBar(long style,
                                       wxWindowID id,
                                       const wxString& name );

    // get/set the main toolbar
    virtual wxToolBar *GetToolBar() const { return m_frameToolBar; }
    virtual void SetToolBar(wxToolBar *toolbar) { m_frameToolBar = toolbar; }
#endif // wxUSE_TOOLBAR

    // implementation only from now on
    // -------------------------------

    // event handlers
    void OnIdle(wxIdleEvent& event);
    void OnMenuHighlight(wxMenuEvent& event);

#if wxUSE_MENUS
    // send wxUpdateUIEvents for all menu items (called from OnIdle())
    void DoMenuUpdates();
    void DoMenuUpdates(wxMenu* menu, wxWindow* focusWin);
#endif // wxUSE_MENUS

    // if there is no real wxTopLevelWindow on this platform we have to define
    // some wxTopLevelWindowBase pure virtual functions here to avoid breaking
    // old ports (wxMotif) which don't define them in wxFrame
#ifndef wxTopLevelWindowNative
    virtual bool ShowFullScreen(bool WXUNUSED(show),
                                long WXUNUSED(style) = wxFULLSCREEN_ALL)
        { return FALSE; }
    virtual bool IsFullScreen() const
        { return FALSE; }
#endif // no wxTopLevelWindowNative

    // show help text (typically in the statusbar); show is FALSE
    // if you are hiding the help, TRUE otherwise
    virtual void DoGiveHelp(const wxString& text, bool show);

protected:
    // the frame main menu/status/tool bars
    // ------------------------------------

    // this (non virtual!) function should be called from dtor to delete the
    // main menubar, statusbar and toolbar (if any)
    void DeleteAllBars();

    // test whether this window makes part of the frame
    virtual bool IsOneOfBars(const wxWindow *win) const;

#if wxUSE_MENUS
    // override to update menu bar position when the frame size changes
    virtual void PositionMenuBar() { }

    // override to do something special when the menu bar is being removed
    // from the frame
    virtual void DetachMenuBar();

    // override to do something special when the menu bar is attached to the
    // frame
    virtual void AttachMenuBar(wxMenuBar *menubar);

    wxMenuBar *m_frameMenuBar;
#endif // wxUSE_MENUS

#if wxUSE_STATUSBAR
    // override to update status bar position (or anything else) when
    // something changes
    virtual void PositionStatusBar() { }

    // show the help string for this menu item in the given status bar: the
    // status bar pointer can be NULL; return TRUE if help was shown
    bool ShowMenuHelp(wxStatusBar *statbar, int id);

    wxStatusBar *m_frameStatusBar;
#endif // wxUSE_STATUSBAR


    int m_statusBarPane;

#if wxUSE_TOOLBAR
    // override to update status bar position (or anything else) when
    // something changes
    virtual void PositionToolBar() { }

    wxToolBar *m_frameToolBar;
#endif // wxUSE_TOOLBAR

    DECLARE_EVENT_TABLE()
};

// include the real class declaration
#if defined(__WXUNIVERSAL__) // && !defined(__WXMICROWIN__)
    #include "wx/univ/frame.h"
#else // !__WXUNIVERSAL__
    #if defined(__WXMSW__)
        #include "wx/msw/frame.h"
    #elif defined(__WXGTK__)
        #include "wx/gtk/frame.h"
    #elif defined(__WXMOTIF__)
        #include "wx/motif/frame.h"
    #elif defined(__WXMAC__)
        #include "wx/mac/frame.h"
    #elif defined(__WXPM__)
        #include "wx/os2/frame.h"
    #endif
#endif

#endif
    // _WX_FRAME_H_BASE_
