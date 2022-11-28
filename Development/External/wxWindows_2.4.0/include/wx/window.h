///////////////////////////////////////////////////////////////////////////////
// Name:        wx/window.h
// Purpose:     wxWindowBase class - the interface of wxWindow
// Author:      Vadim Zeitlin
// Modified by: Ron Lee
// Created:     01/02/97
// RCS-ID:      $Id: window.h,v 1.94.2.4 2002/11/22 21:37:10 VZ Exp $
// Copyright:   (c) wxWindows team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WINDOW_H_BASE_
#define _WX_WINDOW_H_BASE_

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "windowbase.h"
#endif

// ----------------------------------------------------------------------------
// headers which we must include here
// ----------------------------------------------------------------------------

#include "wx/event.h"           // the base class

#include "wx/list.h"            // defines wxWindowList

#include "wx/cursor.h"          // we have member variables of these classes
#include "wx/font.h"            // so we can't do without them
#include "wx/colour.h"
#include "wx/region.h"
#include "wx/utils.h"

#include "wx/validate.h"        // for wxDefaultValidator (always include it)

#if wxUSE_PALETTE
    #include "wx/dcclient.h"
    #include "wx/palette.h"
#endif // wxUSE_PALETTE

#if wxUSE_ACCEL
    #include "wx/accel.h"
#endif // wxUSE_ACCEL

// when building wxUniv/Foo we don't want the code for native menu use to be
// compiled in - it should only be used when building real wxFoo
#ifdef __WXUNIVERSAL__
    #define wxUSE_MENUS_NATIVE 0
#else // __WXMSW__
    #define wxUSE_MENUS_NATIVE wxUSE_MENUS
#endif // __WXUNIVERSAL__/__WXMSW__

// ----------------------------------------------------------------------------
// forward declarations
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxCaret;
class WXDLLEXPORT wxControl;
class WXDLLEXPORT wxCursor;
class WXDLLEXPORT wxDC;
class WXDLLEXPORT wxDropTarget;
class WXDLLEXPORT wxItemResource;
class WXDLLEXPORT wxLayoutConstraints;
class WXDLLEXPORT wxResourceTable;
class WXDLLEXPORT wxSizer;
class WXDLLEXPORT wxToolTip;
class WXDLLEXPORT wxWindowBase;
class WXDLLEXPORT wxWindow;

// ----------------------------------------------------------------------------
// (pseudo)template list classes
// ----------------------------------------------------------------------------

WX_DECLARE_LIST_3(wxWindow, wxWindowBase, wxWindowList, wxWindowListNode, class WXDLLEXPORT);

// ----------------------------------------------------------------------------
// global variables
// ----------------------------------------------------------------------------

WXDLLEXPORT_DATA(extern wxWindowList) wxTopLevelWindows;

// ----------------------------------------------------------------------------
// wxWindowBase is the base class for all GUI controls/widgets, this is the public
// interface of this class.
//
// Event handler: windows have themselves as their event handlers by default,
// but their event handlers could be set to another object entirely. This
// separation can reduce the amount of derivation required, and allow
// alteration of a window's functionality (e.g. by a resource editor that
// temporarily switches event handlers).
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxWindowBase : public wxEvtHandler
{
public:
    // creating the window
    // -------------------

        // default ctor
    wxWindowBase() { InitBase(); }

        // pseudo ctor (can't be virtual, called from ctor)
    bool CreateBase(wxWindowBase *parent,
                    wxWindowID id,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    long style = 0,
                    const wxValidator& validator = wxDefaultValidator,
                    const wxString& name = wxPanelNameStr);

    virtual ~wxWindowBase();

#if wxUSE_WX_RESOURCES
    // these functions are implemented in resource.cpp and resourc2.cpp
    virtual bool LoadFromResource(wxWindow *parent,
                                  const wxString& resourceName,
                                  const wxResourceTable *table = (const wxResourceTable *) NULL);
    virtual wxControl *CreateItem(const wxItemResource* childResource,
                                  const wxItemResource* parentResource,
                                  const wxResourceTable *table = (const wxResourceTable *) NULL);
#endif // wxUSE_WX_RESOURCES

    // deleting the window
    // -------------------

        // ask the window to close itself, return TRUE if the event handler
        // honoured our request
    bool Close( bool force = FALSE );

        // the following functions delete the C++ objects (the window itself
        // or its children) as well as the GUI windows and normally should
        // never be used directly

        // delete window unconditionally (dangerous!), returns TRUE if ok
    virtual bool Destroy();
        // delete all children of this window, returns TRUE if ok
    bool DestroyChildren();

        // is the window being deleted?
    bool IsBeingDeleted() const { return m_isBeingDeleted; }

    // window attributes
    // -----------------

        // NB: in future versions of wxWindows Set/GetTitle() will only work
        //     with the top level windows (such as dialogs and frames) and
        //     Set/GetLabel() only with the other ones (i.e. all controls).

        // the title (or label, see below) of the window: the text which the
        // window shows
    virtual void SetTitle( const wxString& WXUNUSED(title) ) {}
    virtual wxString GetTitle() const { return wxEmptyString; }

        // label is just the same as the title (but for, e.g., buttons it
        // makes more sense to speak about labels)
    virtual void SetLabel(const wxString& label) { SetTitle(label); }
    virtual wxString GetLabel() const { return GetTitle(); }

        // the window name is used for ressource setting in X, it is not the
        // same as the window title/label
    virtual void SetName( const wxString &name ) { m_windowName = name; }
    virtual wxString GetName() const { return m_windowName; }

        // window id uniquely identifies the window among its siblings unless
        // it is -1 which means "don't care"
    void SetId( wxWindowID id ) { m_windowId = id; }
    wxWindowID GetId() const { return m_windowId; }

        // generate a control id for the controls which were not given one by
        // user
    static int NewControlId() { return --ms_lastControlId; }
        // get the id of the control following the one with the given
        // (autogenerated) id
    static int NextControlId(int id) { return id - 1; }
        // get the id of the control preceding the one with the given
        // (autogenerated) id
    static int PrevControlId(int id) { return id + 1; }

    // moving/resizing
    // ---------------

        // set the window size and/or position
    void SetSize( int x, int y, int width, int height,
                  int sizeFlags = wxSIZE_AUTO )
        {  DoSetSize(x, y, width, height, sizeFlags); }

    void SetSize( int width, int height )
        { DoSetSize( -1, -1, width, height, wxSIZE_USE_EXISTING ); }

    void SetSize( const wxSize& size )
        { SetSize( size.x, size.y); }

    void SetSize(const wxRect& rect, int sizeFlags = wxSIZE_AUTO)
        { DoSetSize(rect.x, rect.y, rect.width, rect.height, sizeFlags); }

    void Move(int x, int y, int flags = wxSIZE_USE_EXISTING)
        { DoSetSize(x, y, -1, -1, flags); }

    void Move(const wxPoint& pt, int flags = wxSIZE_USE_EXISTING)
        { Move(pt.x, pt.y, flags); }

        // Z-order
    virtual void Raise() = 0;
    virtual void Lower() = 0;

        // client size is the size of area available for subwindows
    void SetClientSize( int width, int height )
        { DoSetClientSize(width, height); }

    void SetClientSize( const wxSize& size )
        { DoSetClientSize(size.x, size.y); }

    void SetClientSize(const wxRect& rect)
        { SetClientSize( rect.width, rect.height ); }

        // get the window position and/or size (pointers may be NULL)
    void GetPosition( int *x, int *y ) const { DoGetPosition(x, y); }
    wxPoint GetPosition() const
    {
        int w, h;
        DoGetPosition(&w, &h);

        return wxPoint(w, h);
    }

    void GetSize( int *w, int *h ) const { DoGetSize(w, h); }
    wxSize GetSize() const
    {
        int w, h;
        DoGetSize(& w, & h);
        return wxSize(w, h);
    }

    wxRect GetRect() const
    {
        int x, y, w, h;
        GetPosition(& x, & y);
        GetSize(& w, & h);

        return wxRect(x, y, w, h);
    }

    void GetClientSize( int *w, int *h ) const { DoGetClientSize(w, h); }
    wxSize GetClientSize() const
    {
        int w, h;
        DoGetClientSize(& w, & h);

        return wxSize(w, h);
    }

        // get the origin of the client area of the window relative to the
        // window top left corner (the client area may be shifted because of
        // the borders, scrollbars, other decorations...)
    virtual wxPoint GetClientAreaOrigin() const;

        // get the client rectangle in window (i.e. client) coordinates
    wxRect GetClientRect() const
    {
        return wxRect(GetClientAreaOrigin(), GetClientSize());
    }

        // get the size best suited for the window (in fact, minimal
        // acceptable size using which it will still look "nice")
    wxSize GetBestSize() const { return DoGetBestSize(); }
    void GetBestSize(int *w, int *h) const
    {
        wxSize s = DoGetBestSize();
        if ( w )
            *w = s.x;
        if ( h )
            *h = s.y;
    }

        // There are times (and windows) where 'Best' size and 'Min' size
        // are vastly out of sync.  This should be remedied somehow, but in
        // the meantime, this method will return the larger of BestSize
        // (the window's smallest legible size), and any user specified
        // MinSize hint.
    wxSize GetAdjustedBestSize() const
    {
        wxSize  s( DoGetBestSize() );
        return wxSize( wxMax( s.x, GetMinWidth() ), wxMax( s.y, GetMinHeight() ) );
    }

        // the generic centre function - centers the window on parent by`
        // default or on screen if it doesn't have parent or
        // wxCENTER_ON_SCREEN flag is given
    void Centre( int direction = wxBOTH );
    void Center( int direction = wxBOTH ) { Centre(direction); }

        // centre on screen (only works for top level windows)
    void CentreOnScreen(int dir = wxBOTH) { Centre(dir | wxCENTER_ON_SCREEN); }
    void CenterOnScreen(int dir = wxBOTH) { CentreOnScreen(dir); }

        // centre with respect to the the parent window
    void CentreOnParent(int dir = wxBOTH) { Centre(dir | wxCENTER_FRAME); }
    void CenterOnParent(int dir = wxBOTH) { CentreOnParent(dir); }

        // set window size to wrap around its children
    virtual void Fit();

        // set virtual size to satisfy children
    virtual void FitInside();

        // set min/max size of the window
    virtual void SetSizeHints( int minW, int minH,
                               int maxW = -1, int maxH = -1,
                               int incW = -1, int incH = -1 );

    virtual void SetVirtualSizeHints( int minW, int minH,
                                      int maxW = -1, int maxH = -1 );

    virtual int GetMinWidth() const { return m_minWidth; }
    virtual int GetMinHeight() const { return m_minHeight; }
    int GetMaxWidth() const { return m_maxWidth; }
    int GetMaxHeight() const { return m_maxHeight; }

        // Override this method to control the values given to Sizers etc.
    virtual wxSize GetMaxSize() const { return wxSize( m_maxWidth, m_maxHeight ); }

        // Methods for accessing the virtual size of a window.  For most
        // windows this is just the client area of the window, but for
        // some like scrolled windows it is more or less independent of
        // the screen window size.  You may override the DoXXXVirtual
        // methods below for classes where that is is the case.

    void SetVirtualSize( const wxSize &size ) { DoSetVirtualSize( size.x, size.y ); }
    void SetVirtualSize( int x, int y ) { DoSetVirtualSize( x, y ); }

    wxSize GetVirtualSize() const { return DoGetVirtualSize(); }
    void GetVirtualSize( int *x, int *y ) const
    {
        wxSize s( DoGetVirtualSize() );

        if( x )
            *x = s.GetWidth();
        if( y )
            *y = s.GetHeight();
    }

        // Override these methods for windows that have a virtual size
        // independent of their client size.  eg. the virtual area of a
        // wxScrolledWindow.  Default is to alias VirtualSize to ClientSize.

    virtual void DoSetVirtualSize( int x, int y );
    virtual wxSize DoGetVirtualSize() const; // { return m_virtualSize; }

        // Return the largest of ClientSize and BestSize (as determined
        // by a sizer, interior children, or other means)

    virtual wxSize GetBestVirtualSize() const
    {
        wxSize  client( GetClientSize() );
        wxSize  best( GetBestSize() );

        return wxSize( wxMax( client.x, best.x ), wxMax( client.y, best.y ) );
    }

    // window state
    // ------------

        // returns TRUE if window was shown/hidden, FALSE if the nothing was
        // done (window was already shown/hidden)
    virtual bool Show( bool show = TRUE );
    bool Hide() { return Show(FALSE); }

        // returns TRUE if window was enabled/disabled, FALSE if nothing done
    virtual bool Enable( bool enable = TRUE );
    bool Disable() { return Enable(FALSE); }

    bool IsShown() const { return m_isShown; }
    bool IsEnabled() const { return m_isEnabled; }

        // get/set window style (setting style won't update the window and so
        // is only useful for internal usage)
    virtual void SetWindowStyleFlag( long style ) { m_windowStyle = style; }
    virtual long GetWindowStyleFlag() const { return m_windowStyle; }

        // just some (somewhat shorter) synonims
    void SetWindowStyle( long style ) { SetWindowStyleFlag(style); }
    long GetWindowStyle() const { return GetWindowStyleFlag(); }

    bool HasFlag(int flag) const { return (m_windowStyle & flag) != 0; }
    virtual bool IsRetained() const { return HasFlag(wxRETAINED); }

        // extra style: the less often used style bits which can't be set with
        // SetWindowStyleFlag()
    virtual void SetExtraStyle(long exStyle) { m_exStyle = exStyle; }
    long GetExtraStyle() const { return m_exStyle; }

        // make the window modal (all other windows unresponsive)
    virtual void MakeModal(bool modal = TRUE);

    virtual void SetThemeEnabled(bool enableTheme) { m_themeEnabled = enableTheme; }
    virtual bool GetThemeEnabled() const { return m_themeEnabled; }

    // focus and keyboard handling
    // ---------------------------

        // set focus to this window
    virtual void SetFocus() = 0;

        // set focus to this window as the result of a keyboard action
    virtual void SetFocusFromKbd() { SetFocus(); }

        // return the window which currently has the focus or NULL
    static wxWindow *FindFocus() /* = 0: implement in derived classes */;

        // can this window have focus?
    virtual bool AcceptsFocus() const { return IsShown() && IsEnabled(); }

        // can this window be given focus by keyboard navigation? if not, the
        // only way to give it focus (provided it accepts it at all) is to
        // click it
    virtual bool AcceptsFocusFromKeyboard() const { return AcceptsFocus(); }

        // NB: these methods really don't belong here but with the current
        //     class hierarchy there is no other place for them :-(

        // get the default child of this parent, i.e. the one which is
        // activated by pressing <Enter>
    virtual wxWindow *GetDefaultItem() const { return NULL; }

        // set this child as default, return the old default
    virtual wxWindow *SetDefaultItem(wxWindow * WXUNUSED(child))
        { return NULL; }

        // set this child as temporary default
    virtual void SetTmpDefaultItem(wxWindow * WXUNUSED(win)) { }

    // parent/children relations
    // -------------------------

        // get the list of children
    const wxWindowList& GetChildren() const { return m_children; }
    wxWindowList& GetChildren() { return m_children; }

        // get the parent or the parent of the parent
    wxWindow *GetParent() const { return m_parent; }
    inline wxWindow *GetGrandParent() const;

        // is this window a top level one?
    virtual bool IsTopLevel() const;

        // it doesn't really change parent, use ReParent() instead
    void SetParent( wxWindowBase *parent ) { m_parent = (wxWindow *)parent; }
        // change the real parent of this window, return TRUE if the parent
        // was changed, FALSE otherwise (error or newParent == oldParent)
    virtual bool Reparent( wxWindowBase *newParent );

        // implementation mostly
    virtual void AddChild( wxWindowBase *child );
    virtual void RemoveChild( wxWindowBase *child );

    // looking for windows
    // -------------------

        // find window among the descendants of this one either by id or by
        // name (return NULL if not found)
    wxWindow *FindWindow( long id );
    wxWindow *FindWindow( const wxString& name );

        // Find a window among any window (all return NULL if not found)
    static wxWindow *FindWindowById( long id, const wxWindow *parent = NULL );
    static wxWindow *FindWindowByName( const wxString& name,
                                       const wxWindow *parent = NULL );
    static wxWindow *FindWindowByLabel( const wxString& label,
                                        const wxWindow *parent = NULL );

    // event handler stuff
    // -------------------

        // get the current event handler
    wxEvtHandler *GetEventHandler() const { return m_eventHandler; }

        // replace the event handler (allows to completely subclass the
        // window)
    void SetEventHandler( wxEvtHandler *handler ) { m_eventHandler = handler; }

        // push/pop event handler: allows to chain a custom event handler to
        // alreasy existing ones
    void PushEventHandler( wxEvtHandler *handler );
    wxEvtHandler *PopEventHandler( bool deleteHandler = FALSE );

        // find the given handler in the event handler chain and remove (but
        // not delete) it from the event handler chain, return TRUE if it was
        // found and FALSE otherwise (this also results in an assert failure so
        // this function should only be called when the handler is supposed to
        // be there)
    bool RemoveEventHandler(wxEvtHandler *handler);

    // validators
    // ----------

#if wxUSE_VALIDATORS
        // a window may have an associated validator which is used to control
        // user input
    virtual void SetValidator( const wxValidator &validator );
    virtual wxValidator *GetValidator() { return m_windowValidator; }
#endif // wxUSE_VALIDATORS


    // dialog oriented functions
    // -------------------------

        // validate the correctness of input, return TRUE if ok
    virtual bool Validate();

        // transfer data between internal and GUI representations
    virtual bool TransferDataToWindow();
    virtual bool TransferDataFromWindow();

    virtual void InitDialog();

#if wxUSE_ACCEL
    // accelerators
    // ------------
    virtual void SetAcceleratorTable( const wxAcceleratorTable& accel )
        { m_acceleratorTable = accel; }
    wxAcceleratorTable *GetAcceleratorTable()
        { return &m_acceleratorTable; }
#endif // wxUSE_ACCEL

    // dialog units translations
    // -------------------------

    wxPoint ConvertPixelsToDialog( const wxPoint& pt );
    wxPoint ConvertDialogToPixels( const wxPoint& pt );
    wxSize ConvertPixelsToDialog( const wxSize& sz )
    {
        wxPoint pt(ConvertPixelsToDialog(wxPoint(sz.x, sz.y)));

        return wxSize(pt.x, pt.y);
    }

    wxSize ConvertDialogToPixels( const wxSize& sz )
    {
        wxPoint pt(ConvertDialogToPixels(wxPoint(sz.x, sz.y)));

        return wxSize(pt.x, pt.y);
    }

    // mouse functions
    // ---------------

        // move the mouse to the specified position
    virtual void WarpPointer(int x, int y) = 0;

        // start or end mouse capture, these functions maintain the stack of
        // windows having captured the mouse and after calling ReleaseMouse()
        // the mouse is not released but returns to the window which had had
        // captured it previously (if any)
    void CaptureMouse();
    void ReleaseMouse();

        // get the window which currently captures the mouse or NULL
    static wxWindow *GetCapture();

        // does this window have the capture?
    virtual bool HasCapture() const
        { return (wxWindow *)this == GetCapture(); }

    // painting the window
    // -------------------

        // mark the specified rectangle (or the whole window) as "dirty" so it
        // will be repainted
    virtual void Refresh( bool eraseBackground = TRUE,
                          const wxRect *rect = (const wxRect *) NULL ) = 0;

        // a less awkward wrapper for Refresh
    void RefreshRect(const wxRect& rect) { Refresh(TRUE, &rect); }

        // repaint all invalid areas of the window immediately
    virtual void Update() { }

        // clear the window entirely
    virtual void Clear() = 0;

        // freeze the window: don't redraw it until it is thawed
    virtual void Freeze() { }

        // thaw the window: redraw it after it had been frozen
    virtual void Thaw() { }

        // adjust DC for drawing on this window
    virtual void PrepareDC( wxDC & WXUNUSED(dc) ) { }

        // the update region of the window contains the areas which must be
        // repainted by the program
    const wxRegion& GetUpdateRegion() const { return m_updateRegion; }
    wxRegion& GetUpdateRegion() { return m_updateRegion; }

        // get the update rectangleregion bounding box in client coords
    wxRect GetUpdateClientRect() const;

        // these functions verify whether the given point/rectangle belongs to
        // (or at least intersects with) the update region
    bool IsExposed( int x, int y ) const;
    bool IsExposed( int x, int y, int w, int h ) const;

    bool IsExposed( const wxPoint& pt ) const
        { return IsExposed(pt.x, pt.y); }
    bool IsExposed( const wxRect& rect ) const
        { return IsExposed(rect.x, rect.y, rect.width, rect.height); }

    // colours, fonts and cursors
    // --------------------------

        // set/retrieve the window colours (system defaults are used by
        // default): Set functions return TRUE if colour was changed
    virtual bool SetBackgroundColour( const wxColour &colour );
    virtual bool SetForegroundColour( const wxColour &colour );

    wxColour GetBackgroundColour() const { return m_backgroundColour; }
    wxColour GetForegroundColour() const { return m_foregroundColour; }

        // set/retrieve the cursor for this window (SetCursor() returns TRUE
        // if the cursor was really changed)
    virtual bool SetCursor( const wxCursor &cursor );
    const wxCursor& GetCursor() const { return m_cursor; }
    wxCursor& GetCursor() { return m_cursor; }

        // set/retrieve the font for the window (SetFont() returns TRUE if the
        // font really changed)
    virtual bool SetFont( const wxFont &font ) = 0;
    const wxFont& GetFont() const { return m_font; }
    wxFont& GetFont() { return m_font; }

#if wxUSE_CARET
        // associate a caret with the window
    void SetCaret(wxCaret *caret);
        // get the current caret (may be NULL)
    wxCaret *GetCaret() const { return m_caret; }
#endif // wxUSE_CARET

        // get the (average) character size for the current font
    virtual int GetCharHeight() const = 0;
    virtual int GetCharWidth() const = 0;

        // get the width/height/... of the text using current or specified
        // font
    virtual void GetTextExtent(const wxString& string,
                               int *x, int *y,
                               int *descent = (int *) NULL,
                               int *externalLeading = (int *) NULL,
                               const wxFont *theFont = (const wxFont *) NULL)
                               const = 0;

    // client <-> screen coords
    // ------------------------

        // translate to/from screen/client coordinates (pointers may be NULL)
    void ClientToScreen( int *x, int *y ) const
        { DoClientToScreen(x, y); }
    void ScreenToClient( int *x, int *y ) const
        { DoScreenToClient(x, y); }

        // wxPoint interface to do the same thing
    wxPoint ClientToScreen(const wxPoint& pt) const
    {
        int x = pt.x, y = pt.y;
        DoClientToScreen(&x, &y);

        return wxPoint(x, y);
    }

    wxPoint ScreenToClient(const wxPoint& pt) const
    {
        int x = pt.x, y = pt.y;
        DoScreenToClient(&x, &y);

        return wxPoint(x, y);
    }

        // test where the given (in client coords) point lies
    wxHitTest HitTest(wxCoord x, wxCoord y) const
        { return DoHitTest(x, y); }

    wxHitTest HitTest(const wxPoint& pt) const
        { return DoHitTest(pt.x, pt.y); }

    // misc
    // ----

    // get the window border style: uses the current style and falls back to
    // the default style for this class otherwise (see GetDefaultBorder())
    wxBorder GetBorder() const;

    void UpdateWindowUI();

#if wxUSE_MENUS
    bool PopupMenu( wxMenu *menu, const wxPoint& pos )
        { return DoPopupMenu(menu, pos.x, pos.y); }
    bool PopupMenu( wxMenu *menu, int x, int y )
        { return DoPopupMenu(menu, x, y); }
#endif // wxUSE_MENUS

    // scrollbars
    // ----------

        // does the window have the scrollbar for this orientation?
    bool HasScrollbar(int orient) const
    {
        return (m_windowStyle &
                (orient == wxHORIZONTAL ? wxHSCROLL : wxVSCROLL)) != 0;
    }

        // configure the window scrollbars
    virtual void SetScrollbar( int orient,
                               int pos,
                               int thumbvisible,
                               int range,
                               bool refresh = TRUE ) = 0;
    virtual void SetScrollPos( int orient, int pos, bool refresh = TRUE ) = 0;
    virtual int GetScrollPos( int orient ) const = 0;
    virtual int GetScrollThumb( int orient ) const = 0;
    virtual int GetScrollRange( int orient ) const = 0;

        // scroll window to the specified position
    virtual void ScrollWindow( int dx, int dy,
                               const wxRect* rect = (wxRect *) NULL ) = 0;

        // scrolls window by line/page: note that not all controls support this
        //
        // return TRUE if the position changed, FALSE otherwise
    virtual bool ScrollLines(int WXUNUSED(lines)) { return FALSE; }
    virtual bool ScrollPages(int WXUNUSED(pages)) { return FALSE; }

        // convenient wrappers for ScrollLines/Pages
    bool LineUp() { return ScrollLines(-1); }
    bool LineDown() { return ScrollLines(1); }
    bool PageUp() { return ScrollPages(-1); }
    bool PageDown() { return ScrollPages(1); }

    // context-sensitive help
    // ----------------------

    // these are the convenience functions wrapping wxHelpProvider methods

#if wxUSE_HELP
        // associate this help text with this window
    void SetHelpText(const wxString& text);
        // associate this help text with all windows with the same id as this
        // one
    void SetHelpTextForId(const wxString& text);
        // get the help string associated with this window (may be empty)
    wxString GetHelpText() const;
#endif // wxUSE_HELP

    // tooltips
    // --------

#if wxUSE_TOOLTIPS
        // the easiest way to set a tooltip for a window is to use this method
    void SetToolTip( const wxString &tip );
        // attach a tooltip to the window
    void SetToolTip( wxToolTip *tip ) { DoSetToolTip(tip); }
        // get the associated tooltip or NULL if none
    wxToolTip* GetToolTip() const { return m_tooltip; }
#endif // wxUSE_TOOLTIPS

    // drag and drop
    // -------------
#if wxUSE_DRAG_AND_DROP
        // set/retrieve the drop target associated with this window (may be
        // NULL; it's owned by the window and will be deleted by it)
    virtual void SetDropTarget( wxDropTarget *dropTarget ) = 0;
    virtual wxDropTarget *GetDropTarget() const { return m_dropTarget; }
#endif // wxUSE_DRAG_AND_DROP

    // constraints and sizers
    // ----------------------
#if wxUSE_CONSTRAINTS
        // set the constraints for this window or retrieve them (may be NULL)
    void SetConstraints( wxLayoutConstraints *constraints );
    wxLayoutConstraints *GetConstraints() const { return m_constraints; }

        // implementation only
    void UnsetConstraints(wxLayoutConstraints *c);
    wxWindowList *GetConstraintsInvolvedIn() const
        { return m_constraintsInvolvedIn; }
    void AddConstraintReference(wxWindowBase *otherWin);
    void RemoveConstraintReference(wxWindowBase *otherWin);
    void DeleteRelatedConstraints();
    void ResetConstraints();

        // these methods may be overriden for special layout algorithms
    virtual void SetConstraintSizes(bool recurse = TRUE);
    virtual bool LayoutPhase1(int *noChanges);
    virtual bool LayoutPhase2(int *noChanges);
    virtual bool DoPhase(int phase);

        // these methods are virtual but normally won't be overridden
    virtual void SetSizeConstraint(int x, int y, int w, int h);
    virtual void MoveConstraint(int x, int y);
    virtual void GetSizeConstraint(int *w, int *h) const ;
    virtual void GetClientSizeConstraint(int *w, int *h) const ;
    virtual void GetPositionConstraint(int *x, int *y) const ;

#endif // wxUSE_CONSTRAINTS

        // when using constraints or sizers, it makes sense to update
        // children positions automatically whenever the window is resized
        // - this is done if autoLayout is on
    void SetAutoLayout( bool autoLayout ) { m_autoLayout = autoLayout; }
    bool GetAutoLayout() const { return m_autoLayout; }

        // lay out the window and its children
    virtual bool Layout();

        // sizers
    void SetSizer(wxSizer *sizer, bool deleteOld = TRUE );
    void SetSizerAndFit( wxSizer *sizer, bool deleteOld = TRUE );

    wxSizer *GetSizer() const { return m_windowSizer; }

    // Track if this window is a member of a sizer
    void SetContainingSizer(wxSizer* sizer) { m_containingSizer = sizer; }
    wxSizer *GetContainingSizer() const { return m_containingSizer; }

    // backward compatibility
    // ----------------------
#if WXWIN_COMPATIBILITY
    bool Enabled() const { return IsEnabled(); }

    void SetButtonFont(const wxFont& font) { SetFont(font); }
    void SetLabelFont(const wxFont& font) { SetFont(font); }
    wxFont& GetLabelFont() { return GetFont(); };
    wxFont& GetButtonFont() { return GetFont(); };
#endif // WXWIN_COMPATIBILITY

    // implementation
    // --------------

        // event handlers
    void OnSysColourChanged( wxSysColourChangedEvent& event );
    void OnInitDialog( wxInitDialogEvent &event );
    void OnMiddleClick( wxMouseEvent& event );
#if wxUSE_HELP
    void OnHelp(wxHelpEvent& event);
#endif // wxUSE_HELP

        // get the haqndle of the window for the underlying window system: this
        // is only used for wxWin itself or for user code which wants to call
        // platform-specific APIs
    virtual WXWidget GetHandle() const = 0;

#if wxUSE_PALETTE
        // Store the palette used by DCs in wxWindow so that the dcs can share
        // a palette. And we can respond to palette messages.
    wxPalette GetPalette() const { return m_palette; }

        // When palette is changed tell the DC to set the system palette to the
        // new one.
    void SetPalette(const wxPalette& pal);

        // return true if we have a specific palette
    bool HasCustomPalette() const { return m_hasCustomPalette; }

        // return the first parent window with a custom palette or NULL
    wxWindow *GetAncestorWithCustomPalette() const;
#endif // wxUSE_PALETTE

protected:
#if wxUSE_CONSTRAINTS
    // satisfy the constraints for the windows but don't set the window sizes
    void SatisfyConstraints();
#endif // wxUSE_CONSTRAINTS

    // the window id - a number which uniquely identifies a window among
    // its siblings unless it is -1
    wxWindowID           m_windowId;

    // the parent window of this window (or NULL) and the list of the children
    // of this window
    wxWindow            *m_parent;
    wxWindowList         m_children;

    // the minimal allowed size for the window (no minimal size if variable(s)
    // contain(s) -1)
    int                  m_minWidth,
                         m_minHeight,
                         m_maxWidth,
                         m_maxHeight;

    // event handler for this window: usually is just 'this' but may be
    // changed with SetEventHandler()
    wxEvtHandler        *m_eventHandler;

#if wxUSE_VALIDATORS
    // associated validator or NULL if none
    wxValidator         *m_windowValidator;
#endif // wxUSE_VALIDATORS

#if wxUSE_DRAG_AND_DROP
    wxDropTarget        *m_dropTarget;
#endif // wxUSE_DRAG_AND_DROP

    // visual window attributes
    wxCursor             m_cursor;
    wxFont               m_font;
    wxColour             m_backgroundColour, m_foregroundColour;

#if wxUSE_CARET
    wxCaret             *m_caret;
#endif // wxUSE_CARET

    // the region which should be repainted in response to paint event
    wxRegion             m_updateRegion;

#if wxUSE_ACCEL
    // the accelerator table for the window which translates key strokes into
    // command events
    wxAcceleratorTable   m_acceleratorTable;
#endif // wxUSE_ACCEL

    // the tooltip for this window (may be NULL)
#if wxUSE_TOOLTIPS
    wxToolTip           *m_tooltip;
#endif // wxUSE_TOOLTIPS

    // constraints and sizers
#if wxUSE_CONSTRAINTS
    // the constraints for this window or NULL
    wxLayoutConstraints *m_constraints;

    // constraints this window is involved in
    wxWindowList        *m_constraintsInvolvedIn;
#endif // wxUSE_CONSTRAINTS

    // this window's sizer
    wxSizer             *m_windowSizer;

    // The sizer this window is a member of, if any
    wxSizer             *m_containingSizer;

    // Layout() window automatically when its size changes?
    bool                 m_autoLayout:1;

    // window state
    bool                 m_isShown:1;
    bool                 m_isEnabled:1;
    bool                 m_isBeingDeleted:1;

    // was the window colours/font explicitly changed by user?
    bool                 m_hasBgCol:1;
    bool                 m_hasFgCol:1;
    bool                 m_hasFont:1;

    // window attributes
    long                 m_windowStyle,
                         m_exStyle;
    wxString             m_windowName;
    bool                 m_themeEnabled;

#if wxUSE_PALETTE
    wxPalette            m_palette;
    bool                 m_hasCustomPalette;
#endif // wxUSE_PALETTE

    // Virtual size (scrolling)
    wxSize                m_virtualSize;

    int                   m_minVirtualWidth;    // VirtualSizeHints
    int                   m_minVirtualHeight;
    int                   m_maxVirtualWidth;
    int                   m_maxVirtualHeight;

    // common part of all ctors: it is not virtual because it is called from
    // ctor
    void InitBase();

    // override this to change the default (i.e. used when no style is
    // specified) border for the window class
    virtual wxBorder GetDefaultBorder() const;

    // get the default size for the new window if no explicit size given
    // FIXME why 20 and not 30, 10 or ...?
    static int WidthDefault(int w) { return w == -1 ? 20 : w; }
    static int HeightDefault(int h) { return h == -1 ? 20 : h; }

    // set the best size for the control if the default size was given:
    // replaces the fields of size == -1 with the best values for them and
    // calls SetSize() if needed
    //
    // This function is rather unfortunately named..  it's really just a
    // smarter SetSize / convenience function for expanding wxDefaultSize.
    // Note that it does not influence the value returned by GetBestSize
    // at all.
    void SetBestSize(const wxSize& size)
    {
        // the size only needs to be changed if the current size is incomplete,
        // i.e. one of the components was specified as default -- so if both
        // were given, simply don't do anything
        if ( size.x == -1 || size.y == -1 )
        {
            wxSize sizeBest = DoGetBestSize();
            if ( size.x != -1 )
                sizeBest.x = size.x;
            if ( size.y != -1 )
                sizeBest.y = size.y;

            SetSize(sizeBest);
        }
    }

    // more pure virtual functions
    // ---------------------------

    // NB: we must have DoSomething() function when Something() is an overloaded
    //     method: indeed, we can't just have "virtual Something()" in case when
    //     the function is overloaded because then we'd have to make virtual all
    //     the variants (otherwise only the virtual function may be called on a
    //     pointer to derived class according to C++ rules) which is, in
    //     general, absolutely not needed. So instead we implement all
    //     overloaded Something()s in terms of DoSomething() which will be the
    //     only one to be virtual.

    // coordinates translation
    virtual void DoClientToScreen( int *x, int *y ) const = 0;
    virtual void DoScreenToClient( int *x, int *y ) const = 0;

    virtual wxHitTest DoHitTest(wxCoord x, wxCoord y) const;

    // capture/release the mouse, used by Capture/ReleaseMouse()
    virtual void DoCaptureMouse() = 0;
    virtual void DoReleaseMouse() = 0;

    // retrieve the position/size of the window
    virtual void DoGetPosition( int *x, int *y ) const = 0;
    virtual void DoGetSize( int *width, int *height ) const = 0;
    virtual void DoGetClientSize( int *width, int *height ) const = 0;

    // get the size which best suits the window: for a control, it would be
    // the minimal size which doesn't truncate the control, for a panel - the
    // same size as it would have after a call to Fit()
    virtual wxSize DoGetBestSize() const;

    // this is the virtual function to be overriden in any derived class which
    // wants to change how SetSize() or Move() works - it is called by all
    // versions of these functions in the base class
    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO) = 0;

    // same as DoSetSize() for the client size
    virtual void DoSetClientSize(int width, int height) = 0;

    // move the window to the specified location and resize it: this is called
    // from both DoSetSize() and DoSetClientSize() and would usually just
    // reposition this window except for composite controls which will want to
    // arrange themselves inside the given rectangle
    virtual void DoMoveWindow(int x, int y, int width, int height) = 0;

#if wxUSE_TOOLTIPS
    virtual void DoSetToolTip( wxToolTip *tip );
#endif // wxUSE_TOOLTIPS

#if wxUSE_MENUS
    virtual bool DoPopupMenu( wxMenu *menu, int x, int y ) = 0;
#endif // wxUSE_MENUS

    // Makes an adjustment to the window position (for example, a frame that has
    // a toolbar that it manages itself).
    virtual void AdjustForParentClientOrigin(int& x, int& y, int sizeFlags) const;


private:

    // contains the last id generated by NewControlId
    static int ms_lastControlId;

    // the stack of windows which have captured the mouse
    static struct WXDLLEXPORT wxWindowNext *ms_winCaptureNext;

    DECLARE_ABSTRACT_CLASS(wxWindowBase)
    DECLARE_NO_COPY_CLASS(wxWindowBase)
    DECLARE_EVENT_TABLE()
};

// ----------------------------------------------------------------------------
// now include the declaration of wxWindow class
// ----------------------------------------------------------------------------

// include the declaration of the platform-specific class
#if defined(__WXMSW__)
    #ifdef __WXUNIVERSAL__
        #define wxWindowNative wxWindowMSW
    #else // !wxUniv
        #define wxWindowMSW wxWindow
        #define sm_classwxWindowMSW sm_classwxWindow
    #endif // wxUniv/!wxUniv
    #include "wx/msw/window.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/window.h"
#elif defined(__WXGTK__)
    #ifdef __WXUNIVERSAL__
        #define wxWindowNative wxWindowGTK
    #else // !wxUniv
        #define wxWindowGTK wxWindow
        #define sm_classwxWindowGTK sm_classwxWindow
    #endif // wxUniv
    #include "wx/gtk/window.h"
#elif defined(__WXX11__)
    #ifdef __WXUNIVERSAL__
        #define wxWindowNative wxWindowX11
    #else // !wxUniv
        #define wxWindowX11 wxWindow
        #define sm_classwxWindowX11 sm_classwxWindow
    #endif // wxUniv
    #include "wx/x11/window.h"
#elif defined(__WXMGL__)
    #ifdef __WXUNIVERSAL__
        #define wxWindowNative wxWindowMGL
    #else // !wxUniv
        #define wxWindowMGL wxWindow
        #define sm_classwxWindowMGL sm_classwxWindow
    #endif // wxUniv
    #include "wx/mgl/window.h"
#elif defined(__WXMAC__)
    #ifdef __WXUNIVERSAL__
        #define wxWindowNative wxWindowMac
    #else // !wxUniv
        #define wxWindowMac wxWindow
        #define sm_classwxWindowMac sm_classwxWindow
    #endif // wxUniv
    #include "wx/mac/window.h"
#elif defined(__WXPM__)
    #ifdef __WXUNIVERSAL__
        #define wxWindowNative wxWindowOS2
    #else // !wxUniv
        #define wxWindowOS2 wxWindow
        #define sm_classwxWindowOS2 sm_classwxWindow
    #endif // wxUniv/!wxUniv
    #include "wx/os2/window.h"
#endif

// for wxUniversal, we now derive the real wxWindow from wxWindow<platform>,
// for the native ports we already have defined it above
#if defined(__WXUNIVERSAL__)
    #ifndef wxWindowNative
        #error "wxWindowNative must be defined above!"
    #endif

    #include "wx/univ/window.h"
#endif // wxUniv

// ----------------------------------------------------------------------------
// inline functions which couldn't be declared in the class body because of
// forward dependencies
// ----------------------------------------------------------------------------

inline wxWindow *wxWindowBase::GetGrandParent() const
{
    return m_parent ? m_parent->GetParent() : (wxWindow *)NULL;
}

// ----------------------------------------------------------------------------
// global functions
// ----------------------------------------------------------------------------

// Find the wxWindow at the current mouse position, also returning the mouse
// position.
WXDLLEXPORT extern wxWindow* wxFindWindowAtPointer(wxPoint& pt);

// Get the current mouse position.
WXDLLEXPORT extern wxPoint wxGetMousePosition();

// get the currently active window of this application or NULL
WXDLLEXPORT extern wxWindow *wxGetActiveWindow();

// get the (first) top level parent window
WXDLLEXPORT wxWindow* wxGetTopLevelParent(wxWindow *win);

// deprecated (doesn't start with 'wx' prefix), use wxWindow::NewControlId()
inline int NewControlId() { return wxWindowBase::NewControlId(); }

#endif
    // _WX_WINDOW_H_BASE_

