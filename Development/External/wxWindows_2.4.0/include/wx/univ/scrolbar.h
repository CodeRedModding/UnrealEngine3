///////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/scrolbar.h
// Purpose:     wxScrollBar for wxUniversal
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.08.00
// RCS-ID:      $Id: scrolbar.h,v 1.10 2002/05/16 10:45:48 VZ Exp $
// Copyright:   (c) 2000 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_SCROLBAR_H_
#define _WX_UNIV_SCROLBAR_H_

#ifdef __GNUG__
    #pragma interface "univscrolbar.h"
#endif

class WXDLLEXPORT wxScrollTimer;

#include "wx/univ/scrarrow.h"

// ----------------------------------------------------------------------------
// the actions supported by this control
// ----------------------------------------------------------------------------

// scroll the bar
#define wxACTION_SCROLL_START       _T("start")     // to the beginning
#define wxACTION_SCROLL_END         _T("end")       // to the end
#define wxACTION_SCROLL_LINE_UP     _T("lineup")    // one line up/left
#define wxACTION_SCROLL_PAGE_UP     _T("pageup")    // one page up/left
#define wxACTION_SCROLL_LINE_DOWN   _T("linedown")  // one line down/right
#define wxACTION_SCROLL_PAGE_DOWN   _T("pagedown")  // one page down/right

// the scrollbar thumb may be dragged
#define wxACTION_SCROLL_THUMB_DRAG      _T("thumbdrag")
#define wxACTION_SCROLL_THUMB_MOVE      _T("thumbmove")
#define wxACTION_SCROLL_THUMB_RELEASE   _T("thumbrelease")

// ----------------------------------------------------------------------------
// wxScrollBar
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxScrollBar : public wxScrollBarBase,
                                public wxControlWithArrows
{
public:
    // scrollbar elements: they correspond to wxHT_SCROLLBAR_XXX constants but
    // start from 0 which allows to use them as array indices
    enum Element
    {
        Element_Arrow_Line_1,
        Element_Arrow_Line_2,
        Element_Arrow_Page_1,
        Element_Arrow_Page_2,
        Element_Thumb,
        Element_Bar_1,
        Element_Bar_2,
        Element_Max
    };

    wxScrollBar();
    wxScrollBar(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSB_HORIZONTAL,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxScrollBarNameStr);

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSB_HORIZONTAL,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxScrollBarNameStr);

    virtual ~wxScrollBar();

    // implement base class pure virtuals
    virtual int GetThumbPosition() const;
    virtual int GetThumbSize() const;
    virtual int GetPageSize() const;
    virtual int GetRange() const;

    virtual void SetThumbPosition(int thumbPos);
    virtual void SetScrollbar(int position, int thumbSize,
                              int range, int pageSize,
                              bool refresh = TRUE);

    // wxScrollBar actions
    void ScrollToStart();
    void ScrollToEnd();
    bool ScrollLines(int nLines);
    bool ScrollPages(int nPages);

    virtual bool PerformAction(const wxControlAction& action,
                               long numArg = 0,
                               const wxString& strArg = wxEmptyString);

    // The scrollbars around a normal window should not
    // receive the focus.
    virtual bool AcceptsFocus() const;

    // wxScrollBar sub elements state (combination of wxCONTROL_XXX)
    void SetState(Element which, int flags);
    int GetState(Element which) const;

    // implement wxControlWithArrows methods
    virtual wxRenderer *GetRenderer() const { return m_renderer; }
    virtual wxWindow *GetWindow() { return this; }
    virtual bool IsVertical() const { return wxScrollBarBase::IsVertical(); }
    virtual int GetArrowState(wxScrollArrows::Arrow arrow) const;
    virtual void SetArrowFlag(wxScrollArrows::Arrow arrow, int flag, bool set);
    virtual bool OnArrow(wxScrollArrows::Arrow arrow);
    virtual wxScrollArrows::Arrow HitTest(const wxPoint& pt) const;

    // for wxControlRenderer::DrawScrollbar() only
    const wxScrollArrows& GetArrows() const { return m_arrows; }

protected:
    virtual wxSize DoGetBestClientSize() const;
    virtual void DoDraw(wxControlRenderer *renderer);
    virtual wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }

    // event handlers
    void OnIdle(wxIdleEvent& event);

    // forces update of thumb's visual appearence (does nothing if m_dirty=FALSE)
    void UpdateThumb();

    // SetThumbPosition() helper
    void DoSetThumb(int thumbPos);

    // common part of all ctors
    void Init();

    // is this scrollbar attached to a window or a standalone control?
    bool IsStandalone() const;

private:
    // total range of the scrollbar in logical units
    int m_range;

    // the current and previous (after last refresh - this is used for
    // repainting optimisation) size of the thumb in logical units (from 0 to
    // m_range) and its position (from 0 to m_range - m_thumbSize)
    int m_thumbSize,
        m_thumbPos,
        m_thumbPosOld;

    // the page size, i.e. the number of lines by which to scroll when page
    // up/down action is performed
    int m_pageSize;

    // the state of the sub elements
    int m_elementsState[Element_Max];

    // the dirty flag: if set, scrollbar must be updated
    bool m_dirty;

    // the object handling the arrows
    wxScrollArrows m_arrows;

    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxScrollBar)
};

// ----------------------------------------------------------------------------
// common scrollbar input handler: it manages clicks on the standard scrollbar
// elements (line arrows, bar, thumb)
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxStdScrollBarInputHandler : public wxStdInputHandler
{
public:
    // constructor takes a renderer (used for scrollbar hit testing) and the
    // base handler to which all unhandled events are forwarded
    wxStdScrollBarInputHandler(wxRenderer *renderer,
                               wxInputHandler *inphand);

    virtual bool HandleKey(wxInputConsumer *consumer,
                           const wxKeyEvent& event,
                           bool pressed);
    virtual bool HandleMouse(wxInputConsumer *consumer,
                             const wxMouseEvent& event);
    virtual bool HandleMouseMove(wxInputConsumer *consumer, const wxMouseEvent& event);

    virtual ~wxStdScrollBarInputHandler();

    // this method is called by wxScrollBarTimer only and may be overridden
    //
    // return TRUE to continue scrolling, FALSE to stop the timer
    virtual bool OnScrollTimer(wxScrollBar *scrollbar,
                               const wxControlAction& action);

protected:
    // the methods which must be overridden in the derived class

    // return TRUE if the mouse button can be used to activate scrollbar, FALSE
    // if not (only left mouse button can do it under Windows, any button under
    // GTK+)
    virtual bool IsAllowedButton(int button) = 0;

    // set or clear the specified flag on the scrollbar element corresponding
    // to m_htLast
    void SetElementState(wxScrollBar *scrollbar, int flag, bool doIt);

    // [un]highlight the scrollbar element corresponding to m_htLast
    virtual void Highlight(wxScrollBar *scrollbar, bool doIt)
        { SetElementState(scrollbar, wxCONTROL_CURRENT, doIt); }

    // [un]press the scrollbar element corresponding to m_htLast
    virtual void Press(wxScrollBar *scrollbar, bool doIt)
        { SetElementState(scrollbar, wxCONTROL_PRESSED, doIt); }

    // stop scrolling because we reached the end point
    void StopScrolling(wxScrollBar *scrollbar);

    // get the mouse coordinates in the scrollbar direction from the event
    wxCoord GetMouseCoord(const wxScrollBar *scrollbar,
                          const wxMouseEvent& event) const;

    // generate a "thumb move" action for this mouse event
    void HandleThumbMove(wxScrollBar *scrollbar, const wxMouseEvent& event);

    // the window (scrollbar) which has capture or NULL and the flag telling if
    // the mouse is inside the element which captured it or not
    wxWindow *m_winCapture;
    bool      m_winHasMouse;
    int       m_btnCapture;  // the mouse button which has captured mouse

    // the position where we started scrolling by page
    wxPoint m_ptStartScrolling;

    // one of wxHT_SCROLLBAR_XXX value: where has the mouse been last time?
    wxHitTest m_htLast;

    // the renderer (we use it only for hit testing)
    wxRenderer *m_renderer;

    // the offset of the top/left of the scrollbar relative to the mouse to
    // keep during the thumb drag
    int m_ofsMouse;

    // the timer for generating scroll events when the mouse stays pressed on
    // a scrollbar
    wxScrollTimer *m_timerScroll;
};

#endif // _WX_UNIV_SCROLBAR_H_

