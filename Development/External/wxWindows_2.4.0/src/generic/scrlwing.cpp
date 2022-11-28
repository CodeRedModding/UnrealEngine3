/////////////////////////////////////////////////////////////////////////////
// Name:        generic/scrolwin.cpp
// Purpose:     wxGenericScrolledWindow implementation
// Author:      Julian Smart
// Modified by: Vadim Zeitlin on 31.08.00: wxScrollHelper allows to implement.
//              Ron Lee on 10.4.02:  virtual size / auto scrollbars et al.
// Created:     01/02/97
// RCS-ID:      $Id: scrlwing.cpp,v 1.28.2.5 2003/01/05 20:27:25 JS Exp $
// Copyright:   (c) wxWindows team
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "genscrolwin.h"
#endif

#ifdef __VMS
#define XtDisplay XTDISPLAY
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if !defined(__WXGTK__) || defined(__WXUNIVERSAL__)

#include "wx/utils.h"
#include "wx/dcclient.h"

#include "wx/scrolwin.h"
#include "wx/panel.h"
#include "wx/timer.h"
#include "wx/sizer.h"

#ifdef __WXMSW__
    #include <windows.h> // for DLGC_WANTARROWS
#endif

#ifdef __WXMOTIF__
// For wxRETAINED implementation
#ifdef __VMS__ //VMS's Xm.h is not (yet) compatible with C++
               //This code switches off the compiler warnings
# pragma message disable nosimpint
#endif
#include <Xm/Xm.h>
#ifdef __VMS__
# pragma message enable nosimpint
#endif
#endif

IMPLEMENT_CLASS(wxScrolledWindow, wxGenericScrolledWindow)

// ----------------------------------------------------------------------------
// wxScrollHelperEvtHandler: intercept the events from the window and forward
// them to wxScrollHelper
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxScrollHelperEvtHandler : public wxEvtHandler
{
public:
    wxScrollHelperEvtHandler(wxScrollHelper *scrollHelper)
    {
        m_scrollHelper = scrollHelper;
    }

    virtual bool ProcessEvent(wxEvent& event);

    void ResetDrawnFlag() { m_hasDrawnWindow = FALSE; }

private:
    wxScrollHelper *m_scrollHelper;

    bool m_hasDrawnWindow;
};

// ----------------------------------------------------------------------------
// wxAutoScrollTimer: the timer used to generate a stream of scroll events when
// a captured mouse is held outside the window
// ----------------------------------------------------------------------------

class wxAutoScrollTimer : public wxTimer
{
public:
    wxAutoScrollTimer(wxWindow *winToScroll, wxScrollHelper *scroll,
                      wxEventType eventTypeToSend,
                      int pos, int orient);

    virtual void Notify();

private:
    wxWindow *m_win;
    wxScrollHelper *m_scrollHelper;
    wxEventType m_eventType;
    int m_pos,
        m_orient;
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxAutoScrollTimer
// ----------------------------------------------------------------------------

wxAutoScrollTimer::wxAutoScrollTimer(wxWindow *winToScroll,
                                     wxScrollHelper *scroll,
                                     wxEventType eventTypeToSend,
                                     int pos, int orient)
{
    m_win = winToScroll;
    m_scrollHelper = scroll;
    m_eventType = eventTypeToSend;
    m_pos = pos;
    m_orient = orient;
}

void wxAutoScrollTimer::Notify()
{
    // only do all this as long as the window is capturing the mouse
    if ( wxWindow::GetCapture() != m_win )
    {
        Stop();
    }
    else // we still capture the mouse, continue generating events
    {
        // first scroll the window if we are allowed to do it
        wxScrollWinEvent event1(m_eventType, m_pos, m_orient);
        event1.SetEventObject(m_win);
        if ( m_scrollHelper->SendAutoScrollEvents(event1) &&
                m_win->GetEventHandler()->ProcessEvent(event1) )
        {
            // and then send a pseudo mouse-move event to refresh the selection
            wxMouseEvent event2(wxEVT_MOTION);
            wxGetMousePosition(&event2.m_x, &event2.m_y);

            // the mouse event coordinates should be client, not screen as
            // returned by wxGetMousePosition
            wxWindow *parentTop = m_win;
            while ( parentTop->GetParent() )
                parentTop = parentTop->GetParent();
            wxPoint ptOrig = parentTop->GetPosition();
            event2.m_x -= ptOrig.x;
            event2.m_y -= ptOrig.y;

            event2.SetEventObject(m_win);

            // FIXME: we don't fill in the other members - ok?

            m_win->GetEventHandler()->ProcessEvent(event2);
        }
        else // can't scroll further, stop
        {
            Stop();
        }
    }
}

// ----------------------------------------------------------------------------
// wxScrollHelperEvtHandler
// ----------------------------------------------------------------------------

bool wxScrollHelperEvtHandler::ProcessEvent(wxEvent& event)
{
    wxEventType evType = event.GetEventType();

    // the explanation of wxEVT_PAINT processing hack: for historic reasons
    // there are 2 ways to process this event in classes deriving from
    // wxScrolledWindow. The user code may
    //
    //  1. override wxScrolledWindow::OnDraw(dc)
    //  2. define its own OnPaint() handler
    //
    // In addition, in wxUniversal wxWindow defines OnPaint() itself and
    // always processes the draw event, so we can't just try the window
    // OnPaint() first and call our HandleOnPaint() if it doesn't process it
    // (the latter would never be called in wxUniversal).
    //
    // So the solution is to have a flag telling us whether the user code drew
    // anything in the window. We set it to true here but reset it to false in
    // wxScrolledWindow::OnPaint() handler (which wouldn't be called if the
    // user code defined OnPaint() in the derived class)
    m_hasDrawnWindow = TRUE;

    // pass it on to the real handler
    bool processed = wxEvtHandler::ProcessEvent(event);

    // always process the size events ourselves, even if the user code handles
    // them as well, as we need to AdjustScrollbars()
    //
    // NB: it is important to do it after processing the event in the normal
    //     way as HandleOnSize() may generate a wxEVT_SIZE itself if the
    //     scrollbar[s] (dis)appear and it should be seen by the user code
    //     after this one
    if ( evType == wxEVT_SIZE )
    {
        m_scrollHelper->HandleOnSize((wxSizeEvent &)event);

        return TRUE;
    }

    if ( processed )
    {
        // normally, nothing more to do here - except if it was a paint event
        // which wasn't really processed, then we'll try to call our
        // OnDraw() below (from HandleOnPaint)
        if ( m_hasDrawnWindow )
        {
            return TRUE;
        }
    }

    // reset the skipped flag to FALSE as it might have been set to TRUE in
    // ProcessEvent() above
    event.Skip(FALSE);

    if ( evType == wxEVT_PAINT )
    {
        m_scrollHelper->HandleOnPaint((wxPaintEvent &)event);
        return TRUE;
    }

    if ( evType == wxEVT_SCROLLWIN_TOP ||
         evType == wxEVT_SCROLLWIN_BOTTOM ||
         evType == wxEVT_SCROLLWIN_LINEUP ||
         evType == wxEVT_SCROLLWIN_LINEDOWN ||
         evType == wxEVT_SCROLLWIN_PAGEUP ||
         evType == wxEVT_SCROLLWIN_PAGEDOWN ||
         evType == wxEVT_SCROLLWIN_THUMBTRACK ||
         evType == wxEVT_SCROLLWIN_THUMBRELEASE )
    {
            m_scrollHelper->HandleOnScroll((wxScrollWinEvent &)event);
            return !event.GetSkipped();
    }

    if ( evType == wxEVT_ENTER_WINDOW )
    {
        m_scrollHelper->HandleOnMouseEnter((wxMouseEvent &)event);
    }
    else if ( evType == wxEVT_LEAVE_WINDOW )
    {
        m_scrollHelper->HandleOnMouseLeave((wxMouseEvent &)event);
    }
#if wxUSE_MOUSEWHEEL
    else if ( evType == wxEVT_MOUSEWHEEL )
    {
        m_scrollHelper->HandleOnMouseWheel((wxMouseEvent &)event);
    }
#endif // wxUSE_MOUSEWHEEL
    else if ( evType == wxEVT_CHAR )
    {
        m_scrollHelper->HandleOnChar((wxKeyEvent &)event);
        return !event.GetSkipped();
    }

    return FALSE;
}

// ----------------------------------------------------------------------------
// wxScrollHelper construction
// ----------------------------------------------------------------------------

wxScrollHelper::wxScrollHelper(wxWindow *win)
{
    m_xScrollPixelsPerLine =
    m_yScrollPixelsPerLine =
    m_xScrollPosition =
    m_yScrollPosition =
    m_xScrollLines =
    m_yScrollLines =
    m_xScrollLinesPerPage =
    m_yScrollLinesPerPage = 0;

    m_xScrollingEnabled =
    m_yScrollingEnabled = TRUE;

    m_scaleX =
    m_scaleY = 1.0;
#if wxUSE_MOUSEWHEEL
    m_wheelRotation = 0;
#endif

    m_win =
    m_targetWindow = (wxWindow *)NULL;

    m_timerAutoScroll = (wxTimer *)NULL;

    m_handler = NULL;

    if ( win )
        SetWindow(win);
}

wxScrollHelper::~wxScrollHelper()
{
    StopAutoScrolling();

    DeleteEvtHandler();
}

// ----------------------------------------------------------------------------
// setting scrolling parameters
// ----------------------------------------------------------------------------

void wxScrollHelper::SetScrollbars(int pixelsPerUnitX,
                                   int pixelsPerUnitY,
                                   int noUnitsX,
                                   int noUnitsY,
                                   int xPos,
                                   int yPos,
                                   bool noRefresh)
{
    int xpos, ypos;

    CalcUnscrolledPosition(xPos, yPos, &xpos, &ypos);
    bool do_refresh =
    (
      (noUnitsX != 0 && m_xScrollLines == 0) ||
      (noUnitsX < m_xScrollLines && xpos > pixelsPerUnitX * noUnitsX) ||

      (noUnitsY != 0 && m_yScrollLines == 0) ||
      (noUnitsY < m_yScrollLines && ypos > pixelsPerUnitY * noUnitsY) ||
      (xPos != m_xScrollPosition) ||
      (yPos != m_yScrollPosition)
    );

    m_xScrollPixelsPerLine = pixelsPerUnitX;
    m_yScrollPixelsPerLine = pixelsPerUnitY;
    m_xScrollPosition = xPos;
    m_yScrollPosition = yPos;

    // For better backward compatibility we set persisting limits
    // here not just the size.  It makes SetScrollbars 'sticky'
    // emulating the old non-autoscroll behaviour.

    wxSize sz = m_targetWindow->GetClientSize();
#if 1
    int x = wxMax(noUnitsX * pixelsPerUnitX, sz.x);
    int y = wxMax(noUnitsY * pixelsPerUnitY, sz.y);
#else
    int x = noUnitsX * pixelsPerUnitX;
    int y = noUnitsY * pixelsPerUnitY;
#endif
    m_targetWindow->SetVirtualSizeHints( x, y );

    // The above should arguably be deprecated, this however we still need.

    m_targetWindow->SetVirtualSize( x, y );

    if (do_refresh && !noRefresh)
        m_targetWindow->Refresh(TRUE, GetRect());

    // TODO: check if we can use AdjustScrollbars always.
#ifdef __WXUNIVERSAL__
    AdjustScrollbars();
#else    
    // This is also done by AdjustScrollbars, above
#ifdef __WXMAC__
    m_targetWindow->MacUpdateImmediately() ;
#endif
#endif
}

// ----------------------------------------------------------------------------
// [target] window handling
// ----------------------------------------------------------------------------

void wxScrollHelper::DeleteEvtHandler()
{
    // search for m_handler in the handler list
    if ( m_win && m_handler )
    {
        if ( m_win->RemoveEventHandler(m_handler) )
        {
            delete m_handler;
        }
        //else: something is very wrong, so better [maybe] leak memory than
        //      risk a crash because of double deletion

        m_handler = NULL;
    }
}

void wxScrollHelper::SetWindow(wxWindow *win)
{
    wxCHECK_RET( win, _T("wxScrollHelper needs a window to scroll") );

    m_win = win;

    // by default, the associated window is also the target window
    DoSetTargetWindow(win);
}

void wxScrollHelper::DoSetTargetWindow(wxWindow *target)
{
    m_targetWindow = target;

    // install the event handler which will intercept the events we're
    // interested in (but only do it for our real window, not the target window
    // which we scroll - we don't need to hijack its events)
    if ( m_targetWindow == m_win )
    {
        // if we already have a handler, delete it first
        DeleteEvtHandler();

        m_handler = new wxScrollHelperEvtHandler(this);
        m_targetWindow->PushEventHandler(m_handler);
    }
}

void wxScrollHelper::SetTargetWindow(wxWindow *target)
{
    wxCHECK_RET( target, wxT("target window must not be NULL") );

    if ( target == m_targetWindow )
        return;

    DoSetTargetWindow(target);
}

wxWindow *wxScrollHelper::GetTargetWindow() const
{
    return m_targetWindow;
}

// ----------------------------------------------------------------------------
// scrolling implementation itself
// ----------------------------------------------------------------------------

void wxScrollHelper::HandleOnScroll(wxScrollWinEvent& event)
{
    int nScrollInc = CalcScrollInc(event);
    if ( nScrollInc == 0 )
    {
        // can't scroll further
        event.Skip();

        return;
    }

    int orient = event.GetOrientation();
    if (orient == wxHORIZONTAL)
    {
        m_xScrollPosition += nScrollInc;
        m_win->SetScrollPos(wxHORIZONTAL, m_xScrollPosition);
    }
    else
    {
        m_yScrollPosition += nScrollInc;
        m_win->SetScrollPos(wxVERTICAL, m_yScrollPosition);
    }

    bool needsRefresh = FALSE;
    int dx = 0,
        dy = 0;
    if (orient == wxHORIZONTAL)
    {
       if ( m_xScrollingEnabled )
       {
           dx = -m_xScrollPixelsPerLine * nScrollInc;
       }
       else
       {
           needsRefresh = TRUE;
       }
    }
    else
    {
        if ( m_yScrollingEnabled )
        {
            dy = -m_yScrollPixelsPerLine * nScrollInc;
        }
        else
        {
            needsRefresh = TRUE;
        }
    }

    if ( needsRefresh )
    {
        m_targetWindow->Refresh(TRUE, GetRect());
    }
    else
    {
        m_targetWindow->ScrollWindow(dx, dy, GetRect());
    }

#ifdef __WXMAC__
    m_targetWindow->MacUpdateImmediately() ;
#endif
}

int wxScrollHelper::CalcScrollInc(wxScrollWinEvent& event)
{
    int pos = event.GetPosition();
    int orient = event.GetOrientation();

    int nScrollInc = 0;
    if (event.GetEventType() == wxEVT_SCROLLWIN_TOP)
    {
            if (orient == wxHORIZONTAL)
                nScrollInc = - m_xScrollPosition;
            else
                nScrollInc = - m_yScrollPosition;
    } else
    if (event.GetEventType() == wxEVT_SCROLLWIN_BOTTOM)
    {
            if (orient == wxHORIZONTAL)
                nScrollInc = m_xScrollLines - m_xScrollPosition;
            else
                nScrollInc = m_yScrollLines - m_yScrollPosition;
    } else
    if (event.GetEventType() == wxEVT_SCROLLWIN_LINEUP)
    {
            nScrollInc = -1;
    } else
    if (event.GetEventType() == wxEVT_SCROLLWIN_LINEDOWN)
    {
            nScrollInc = 1;
    } else
    if (event.GetEventType() == wxEVT_SCROLLWIN_PAGEUP)
    {
            if (orient == wxHORIZONTAL)
                nScrollInc = -GetScrollPageSize(wxHORIZONTAL);
            else
                nScrollInc = -GetScrollPageSize(wxVERTICAL);
    } else
    if (event.GetEventType() == wxEVT_SCROLLWIN_PAGEDOWN)
    {
            if (orient == wxHORIZONTAL)
                nScrollInc = GetScrollPageSize(wxHORIZONTAL);
            else
                nScrollInc = GetScrollPageSize(wxVERTICAL);
    } else
    if ((event.GetEventType() == wxEVT_SCROLLWIN_THUMBTRACK) ||
        (event.GetEventType() == wxEVT_SCROLLWIN_THUMBRELEASE))
    {
            if (orient == wxHORIZONTAL)
                nScrollInc = pos - m_xScrollPosition;
            else
                nScrollInc = pos - m_yScrollPosition;
    }

    if (orient == wxHORIZONTAL)
    {
        if (m_xScrollPixelsPerLine > 0)
        {
            int w, h;
            GetTargetSize(&w, &h);

            int nMaxWidth = m_xScrollLines*m_xScrollPixelsPerLine;
            int noPositions = (int) ( ((nMaxWidth - w)/(double)m_xScrollPixelsPerLine) + 0.5 );
            if (noPositions < 0)
                noPositions = 0;

            if ( (m_xScrollPosition + nScrollInc) < 0 )
                nScrollInc = -m_xScrollPosition; // As -ve as we can go
            else if ( (m_xScrollPosition + nScrollInc) > noPositions )
                nScrollInc = noPositions - m_xScrollPosition; // As +ve as we can go
        }
        else
            m_targetWindow->Refresh(TRUE, GetRect());
    }
    else
    {
        if (m_yScrollPixelsPerLine > 0)
        {
            int w, h;
            GetTargetSize(&w, &h);

            int nMaxHeight = m_yScrollLines*m_yScrollPixelsPerLine;
            int noPositions = (int) ( ((nMaxHeight - h)/(double)m_yScrollPixelsPerLine) + 0.5 );
            if (noPositions < 0)
                noPositions = 0;

            if ( (m_yScrollPosition + nScrollInc) < 0 )
                nScrollInc = -m_yScrollPosition; // As -ve as we can go
            else if ( (m_yScrollPosition + nScrollInc) > noPositions )
                nScrollInc = noPositions - m_yScrollPosition; // As +ve as we can go
        }
        else
            m_targetWindow->Refresh(TRUE, GetRect());
    }

    return nScrollInc;
}

// Adjust the scrollbars - new version.
void wxScrollHelper::AdjustScrollbars()
{
#ifdef __WXMAC__
    m_targetWindow->MacUpdateImmediately();
#endif

    int w = 0, h = 0;
    int oldw, oldh;

    int oldXScroll = m_xScrollPosition;
    int oldYScroll = m_yScrollPosition;

    do {
        GetTargetSize(&w, 0);

        if (m_xScrollPixelsPerLine == 0)
        {
            m_xScrollLines = 0;
            m_xScrollPosition = 0;
            m_win->SetScrollbar (wxHORIZONTAL, 0, 0, 0, FALSE);
        }
        else
        {
            m_xScrollLines = m_targetWindow->GetVirtualSize().GetWidth() / m_xScrollPixelsPerLine;

            // Calculate page size i.e. number of scroll units you get on the
            // current client window
            int noPagePositions = (int) ( (w/(double)m_xScrollPixelsPerLine) + 0.5 );
            if (noPagePositions < 1) noPagePositions = 1;
            if ( noPagePositions > m_xScrollLines )
                noPagePositions = m_xScrollLines;

            // Correct position if greater than extent of canvas minus
            // the visible portion of it or if below zero
            m_xScrollPosition = wxMin( m_xScrollLines - noPagePositions, m_xScrollPosition);
            m_xScrollPosition = wxMax( 0, m_xScrollPosition );

            m_win->SetScrollbar(wxHORIZONTAL, m_xScrollPosition, noPagePositions, m_xScrollLines);
            // The amount by which we scroll when paging
            SetScrollPageSize(wxHORIZONTAL, noPagePositions);
        }

        GetTargetSize(0, &h);

        if (m_yScrollPixelsPerLine == 0)
        {
            m_yScrollLines = 0;
            m_yScrollPosition = 0;
            m_win->SetScrollbar (wxVERTICAL, 0, 0, 0, FALSE);
        }
        else
        {
            m_yScrollLines = m_targetWindow->GetVirtualSize().GetHeight() / m_yScrollPixelsPerLine;

            // Calculate page size i.e. number of scroll units you get on the
            // current client window
            int noPagePositions = (int) ( (h/(double)m_yScrollPixelsPerLine) + 0.5 );
            if (noPagePositions < 1) noPagePositions = 1;
            if ( noPagePositions > m_yScrollLines )
                noPagePositions = m_yScrollLines;

            // Correct position if greater than extent of canvas minus
            // the visible portion of it or if below zero
            m_yScrollPosition = wxMin( m_yScrollLines - noPagePositions, m_yScrollPosition );
            m_yScrollPosition = wxMax( 0, m_yScrollPosition );

            m_win->SetScrollbar(wxVERTICAL, m_yScrollPosition, noPagePositions, m_yScrollLines);
            // The amount by which we scroll when paging
            SetScrollPageSize(wxVERTICAL, noPagePositions);
        }

        // If a scrollbar (dis)appeared as a result of this, adjust them again.

        oldw = w;
        oldh = h;

        GetTargetSize( &w, &h );
    } while ( w != oldw && h != oldh );

#ifdef __WXMOTIF__
    // Sorry, some Motif-specific code to implement a backing pixmap
    // for the wxRETAINED style. Implementing a backing store can't
    // be entirely generic because it relies on the wxWindowDC implementation
    // to duplicate X drawing calls for the backing pixmap.

    if ( m_targetWindow->GetWindowStyle() & wxRETAINED )
    {
        Display* dpy = XtDisplay((Widget)m_targetWindow->GetMainWidget());

        int totalPixelWidth = m_xScrollLines * m_xScrollPixelsPerLine;
        int totalPixelHeight = m_yScrollLines * m_yScrollPixelsPerLine;
        if (m_targetWindow->GetBackingPixmap() &&
           !((m_targetWindow->GetPixmapWidth() == totalPixelWidth) &&
             (m_targetWindow->GetPixmapHeight() == totalPixelHeight)))
        {
            XFreePixmap (dpy, (Pixmap) m_targetWindow->GetBackingPixmap());
            m_targetWindow->SetBackingPixmap((WXPixmap) 0);
        }

        if (!m_targetWindow->GetBackingPixmap() &&
           (m_xScrollLines != 0) && (m_yScrollLines != 0))
        {
            int depth = wxDisplayDepth();
            m_targetWindow->SetPixmapWidth(totalPixelWidth);
            m_targetWindow->SetPixmapHeight(totalPixelHeight);
            m_targetWindow->SetBackingPixmap((WXPixmap) XCreatePixmap (dpy, RootWindow (dpy, DefaultScreen (dpy)),
              m_targetWindow->GetPixmapWidth(), m_targetWindow->GetPixmapHeight(), depth));
        }

    }
#endif // Motif

    if (oldXScroll != m_xScrollPosition)
    {
       if (m_xScrollingEnabled)
            m_targetWindow->ScrollWindow( m_xScrollPixelsPerLine * (oldXScroll - m_xScrollPosition), 0,
                                          GetRect() );
       else
            m_targetWindow->Refresh(TRUE, GetRect());
    }

    if (oldYScroll != m_yScrollPosition)
    {
        if (m_yScrollingEnabled)
            m_targetWindow->ScrollWindow( 0, m_yScrollPixelsPerLine * (oldYScroll-m_yScrollPosition),
                                          GetRect() );
        else
            m_targetWindow->Refresh(TRUE, GetRect());
    }

#ifdef __WXMAC__
    m_targetWindow->MacUpdateImmediately();
#endif
}

void wxScrollHelper::DoPrepareDC(wxDC& dc)
{
    wxPoint pt = dc.GetDeviceOrigin();
    dc.SetDeviceOrigin( pt.x - m_xScrollPosition * m_xScrollPixelsPerLine,
                        pt.y - m_yScrollPosition * m_yScrollPixelsPerLine );
    dc.SetUserScale( m_scaleX, m_scaleY );
}

void wxScrollHelper::SetScrollRate( int xstep, int ystep )
{
    int old_x = m_xScrollPixelsPerLine * m_xScrollPosition;
    int old_y = m_yScrollPixelsPerLine * m_yScrollPosition;

    m_xScrollPixelsPerLine = xstep;
    m_yScrollPixelsPerLine = ystep;

    int new_x = m_xScrollPixelsPerLine * m_xScrollPosition;
    int new_y = m_yScrollPixelsPerLine * m_yScrollPosition;

    m_win->SetScrollPos( wxHORIZONTAL, m_xScrollPosition );
    m_win->SetScrollPos( wxVERTICAL, m_yScrollPosition );
    m_targetWindow->ScrollWindow( old_x - new_x, old_y - new_y );

    AdjustScrollbars();
}

void wxScrollHelper::GetScrollPixelsPerUnit (int *x_unit, int *y_unit) const
{
    if ( x_unit )
        *x_unit = m_xScrollPixelsPerLine;
    if ( y_unit )
        *y_unit = m_yScrollPixelsPerLine;
}

int wxScrollHelper::GetScrollPageSize(int orient) const
{
    if ( orient == wxHORIZONTAL )
        return m_xScrollLinesPerPage;
    else
        return m_yScrollLinesPerPage;
}

void wxScrollHelper::SetScrollPageSize(int orient, int pageSize)
{
    if ( orient == wxHORIZONTAL )
        m_xScrollLinesPerPage = pageSize;
    else
        m_yScrollLinesPerPage = pageSize;
}

/*
 * Scroll to given position (scroll position, not pixel position)
 */
void wxScrollHelper::Scroll( int x_pos, int y_pos )
{
    if (!m_targetWindow)
        return;

    if (((x_pos == -1) || (x_pos == m_xScrollPosition)) &&
        ((y_pos == -1) || (y_pos == m_yScrollPosition))) return;

#ifdef __WXMAC__
    m_targetWindow->MacUpdateImmediately();
#endif

    int w, h;
    GetTargetSize(&w, &h);

    if ((x_pos != -1) && (m_xScrollPixelsPerLine))
    {
        int old_x = m_xScrollPosition;
        m_xScrollPosition = x_pos;

        // Calculate page size i.e. number of scroll units you get on the
        // current client window
        int noPagePositions = (int) ( (w/(double)m_xScrollPixelsPerLine) + 0.5 );
        if (noPagePositions < 1) noPagePositions = 1;

        // Correct position if greater than extent of canvas minus
        // the visible portion of it or if below zero
        m_xScrollPosition = wxMin( m_xScrollLines-noPagePositions, m_xScrollPosition );
        m_xScrollPosition = wxMax( 0, m_xScrollPosition );

        if (old_x != m_xScrollPosition) {
            m_win->SetScrollPos( wxHORIZONTAL, m_xScrollPosition );
            m_targetWindow->ScrollWindow( (old_x-m_xScrollPosition)*m_xScrollPixelsPerLine, 0,
                                          GetRect() );
        }
    }
    if ((y_pos != -1) && (m_yScrollPixelsPerLine))
    {
        int old_y = m_yScrollPosition;
        m_yScrollPosition = y_pos;

        // Calculate page size i.e. number of scroll units you get on the
        // current client window
        int noPagePositions = (int) ( (h/(double)m_yScrollPixelsPerLine) + 0.5 );
        if (noPagePositions < 1) noPagePositions = 1;

        // Correct position if greater than extent of canvas minus
        // the visible portion of it or if below zero
        m_yScrollPosition = wxMin( m_yScrollLines-noPagePositions, m_yScrollPosition );
        m_yScrollPosition = wxMax( 0, m_yScrollPosition );

        if (old_y != m_yScrollPosition) {
            m_win->SetScrollPos( wxVERTICAL, m_yScrollPosition );
            m_targetWindow->ScrollWindow( 0, (old_y-m_yScrollPosition)*m_yScrollPixelsPerLine,
                                          GetRect() );
        }
    }

#ifdef __WXMAC__
    m_targetWindow->MacUpdateImmediately();
#endif

}

void wxScrollHelper::EnableScrolling (bool x_scroll, bool y_scroll)
{
    m_xScrollingEnabled = x_scroll;
    m_yScrollingEnabled = y_scroll;
}

// Where the current view starts from
void wxScrollHelper::GetViewStart (int *x, int *y) const
{
    if ( x )
        *x = m_xScrollPosition;
    if ( y )
        *y = m_yScrollPosition;
}

void wxScrollHelper::DoCalcScrolledPosition(int x, int y, int *xx, int *yy) const
{
    if ( xx )
        *xx = x - m_xScrollPosition * m_xScrollPixelsPerLine;
    if ( yy )
        *yy = y - m_yScrollPosition * m_yScrollPixelsPerLine;
}

void wxScrollHelper::DoCalcUnscrolledPosition(int x, int y, int *xx, int *yy) const
{
    if ( xx )
        *xx = x + m_xScrollPosition * m_xScrollPixelsPerLine;
    if ( yy )
        *yy = y + m_yScrollPosition * m_yScrollPixelsPerLine;
}

// ----------------------------------------------------------------------------
// event handlers
// ----------------------------------------------------------------------------

// Default OnSize resets scrollbars, if any
void wxScrollHelper::HandleOnSize(wxSizeEvent& WXUNUSED(event))
{
    if( m_win->GetAutoLayout() )
    {
        if ( m_targetWindow != m_win )
            m_targetWindow->FitInside();

        m_win->FitInside();

#if wxUSE_CONSTRAINTS
        m_win->Layout();
#endif
    }
    else
        AdjustScrollbars();
}

// This calls OnDraw, having adjusted the origin according to the current
// scroll position
void wxScrollHelper::HandleOnPaint(wxPaintEvent& WXUNUSED(event))
{
    // don't use m_targetWindow here, this is always called for ourselves
    wxPaintDC dc(m_win);
    DoPrepareDC(dc);

    OnDraw(dc);
}

// kbd handling: notice that we use OnChar() and not OnKeyDown() for
// compatibility here - if we used OnKeyDown(), the programs which process
// arrows themselves in their OnChar() would never get the message and like
// this they always have the priority
void wxScrollHelper::HandleOnChar(wxKeyEvent& event)
{
    int stx, sty,       // view origin
        szx, szy,       // view size (total)
        clix, cliy;     // view size (on screen)

    GetViewStart(&stx, &sty);
    GetTargetSize(&clix, &cliy);
    m_targetWindow->GetVirtualSize(&szx, &szy);

    if( m_xScrollPixelsPerLine )
    {
        clix /= m_xScrollPixelsPerLine;
        szx /= m_xScrollPixelsPerLine;
    }
    else
    {
        clix = 0;
        szx = -1;
    }
    if( m_yScrollPixelsPerLine )
    {
        cliy /= m_yScrollPixelsPerLine;
        szy /= m_yScrollPixelsPerLine;
    }
    else
    {
        cliy = 0;
        szy = -1;
    }

    int xScrollOld = m_xScrollPosition,
        yScrollOld = m_yScrollPosition;

    int dsty;
    switch ( event.KeyCode() )
    {
        case WXK_PAGEUP:
        case WXK_PRIOR:
            dsty = sty - (5 * cliy / 6);
            Scroll(-1, (dsty == -1) ? 0 : dsty);
            break;

        case WXK_PAGEDOWN:
        case WXK_NEXT:
            Scroll(-1, sty + (5 * cliy / 6));
            break;

        case WXK_HOME:
            Scroll(0, event.ControlDown() ? 0 : -1);
            break;

        case WXK_END:
            Scroll(szx - clix, event.ControlDown() ? szy - cliy : -1);
            break;

        case WXK_UP:
            Scroll(-1, sty - 1);
            break;

        case WXK_DOWN:
            Scroll(-1, sty + 1);
            break;

        case WXK_LEFT:
            Scroll(stx - 1, -1);
            break;

        case WXK_RIGHT:
            Scroll(stx + 1, -1);
            break;

        default:
            // not for us
            event.Skip();
    }

    if ( m_xScrollPosition != xScrollOld )
    {
        wxScrollWinEvent event(wxEVT_SCROLLWIN_THUMBTRACK, m_xScrollPosition,
                               wxHORIZONTAL);
        event.SetEventObject(m_win);
        m_win->GetEventHandler()->ProcessEvent(event);
    }

    if ( m_yScrollPosition != yScrollOld )
    {
        wxScrollWinEvent event(wxEVT_SCROLLWIN_THUMBTRACK, m_yScrollPosition,
                               wxVERTICAL);
        event.SetEventObject(m_win);
        m_win->GetEventHandler()->ProcessEvent(event);
    }
}

// ----------------------------------------------------------------------------
// autoscroll stuff: these functions deal with sending fake scroll events when
// a captured mouse is being held outside the window
// ----------------------------------------------------------------------------

bool wxScrollHelper::SendAutoScrollEvents(wxScrollWinEvent& event) const
{
    // only send the event if the window is scrollable in this direction
    wxWindow *win = (wxWindow *)event.GetEventObject();
    return win->HasScrollbar(event.GetOrientation());
}

void wxScrollHelper::StopAutoScrolling()
{
    if ( m_timerAutoScroll )
    {
        delete m_timerAutoScroll;
        m_timerAutoScroll = (wxTimer *)NULL;
    }
}

void wxScrollHelper::HandleOnMouseEnter(wxMouseEvent& event)
{
    StopAutoScrolling();

    event.Skip();
}

void wxScrollHelper::HandleOnMouseLeave(wxMouseEvent& event)
{
    // don't prevent the usual processing of the event from taking place
    event.Skip();

    // when a captured mouse leave a scrolled window we start generate
    // scrolling events to allow, for example, extending selection beyond the
    // visible area in some controls
    if ( wxWindow::GetCapture() == m_targetWindow )
    {
        // where is the mouse leaving?
        int pos, orient;
        wxPoint pt = event.GetPosition();
        if ( pt.x < 0 )
        {
            orient = wxHORIZONTAL;
            pos = 0;
        }
        else if ( pt.y < 0 )
        {
            orient = wxVERTICAL;
            pos = 0;
        }
        else // we're lower or to the right of the window
        {
            wxSize size = m_targetWindow->GetClientSize();
            if ( pt.x > size.x )
            {
                orient = wxHORIZONTAL;
                pos = m_xScrollLines;
            }
            else if ( pt.y > size.y )
            {
                orient = wxVERTICAL;
                pos = m_yScrollLines;
            }
            else // this should be impossible
            {
                // but seems to happen sometimes under wxMSW - maybe it's a bug
                // there but for now just ignore it

                //wxFAIL_MSG( _T("can't understand where has mouse gone") );

                return;
            }
        }

        // only start the auto scroll timer if the window can be scrolled in
        // this direction
        if ( !m_targetWindow->HasScrollbar(orient) )
            return;

        delete m_timerAutoScroll;
        m_timerAutoScroll = new wxAutoScrollTimer
                                (
                                    m_targetWindow, this,
                                    pos == 0 ? wxEVT_SCROLLWIN_LINEUP
                                             : wxEVT_SCROLLWIN_LINEDOWN,
                                    pos,
                                    orient
                                );
        m_timerAutoScroll->Start(50); // FIXME: make configurable
    }
}

#if wxUSE_MOUSEWHEEL

void wxScrollHelper::HandleOnMouseWheel(wxMouseEvent& event)
{
    m_wheelRotation += event.GetWheelRotation();
    int lines = m_wheelRotation / event.GetWheelDelta();
    m_wheelRotation -= lines * event.GetWheelDelta();

    if (lines != 0)
    {

        wxScrollWinEvent newEvent;

        newEvent.SetPosition(0);
        newEvent.SetOrientation(wxVERTICAL);
        newEvent.m_eventObject = m_win;

        if (event.IsPageScroll())
        {
            if (lines > 0)
                newEvent.m_eventType = wxEVT_SCROLLWIN_PAGEUP;
            else
                newEvent.m_eventType = wxEVT_SCROLLWIN_PAGEDOWN;

            m_win->GetEventHandler()->ProcessEvent(newEvent);
        }
        else
        {
            lines *= event.GetLinesPerAction();
            if (lines > 0)
                newEvent.m_eventType = wxEVT_SCROLLWIN_LINEUP;
            else
                newEvent.m_eventType = wxEVT_SCROLLWIN_LINEDOWN;

            int times = abs(lines);
            for (; times > 0; times--)
                m_win->GetEventHandler()->ProcessEvent(newEvent);
        }
    }
}

#endif // wxUSE_MOUSEWHEEL

// ----------------------------------------------------------------------------
// wxGenericScrolledWindow implementation
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxGenericScrolledWindow, wxPanel)

BEGIN_EVENT_TABLE(wxGenericScrolledWindow, wxPanel)
    EVT_PAINT(wxGenericScrolledWindow::OnPaint)
END_EVENT_TABLE()

bool wxGenericScrolledWindow::Create(wxWindow *parent,
                              wxWindowID id,
                              const wxPoint& pos,
                              const wxSize& size,
                              long style,
                              const wxString& name)
{
    m_targetWindow = this;

    bool ok = wxPanel::Create(parent, id, pos, size, style, name);

    return ok;
}

wxGenericScrolledWindow::~wxGenericScrolledWindow()
{
}

bool wxGenericScrolledWindow::Layout()
{
    if (GetSizer() && m_targetWindow == this)
    {
        // If we're the scroll target, take into account the
        // virtual size and scrolled position of the window.

        int x, y, w, h;
        CalcScrolledPosition(0,0, &x,&y);
        GetVirtualSize(&w, &h);
        GetSizer()->SetDimension(x, y, w, h);
        return TRUE;
    }

    // fall back to default for LayoutConstraints
    return wxPanel::Layout();
}

void wxGenericScrolledWindow::DoSetVirtualSize(int x, int y)
{
    wxPanel::DoSetVirtualSize( x, y );
    AdjustScrollbars();

#if wxUSE_CONSTRAINTS
    if (GetAutoLayout())
        Layout();
#endif
}

void wxGenericScrolledWindow::OnPaint(wxPaintEvent& event)
{
    // the user code didn't really draw the window if we got here, so set this
    // flag to try to call OnDraw() later
    m_handler->ResetDrawnFlag();

    event.Skip();
}

#ifdef __WXMSW__
long
wxGenericScrolledWindow::MSWWindowProc(WXUINT nMsg,
                                       WXWPARAM wParam,
                                       WXLPARAM lParam)
{
    long rc = wxPanel::MSWWindowProc(nMsg, wParam, lParam);

    // we need to process arrows ourselves for scrolling
    if ( nMsg == WM_GETDLGCODE )
    {
        rc |= DLGC_WANTARROWS;
    }

    return rc;
}

#endif // __WXMSW__

#if WXWIN_COMPATIBILITY

void wxGenericScrolledWindow::GetScrollUnitsPerPage (int *x_page, int *y_page) const
{
      *x_page = GetScrollPageSize(wxHORIZONTAL);
      *y_page = GetScrollPageSize(wxVERTICAL);
}

void wxGenericScrolledWindow::CalcUnscrolledPosition(int x, int y, float *xx, float *yy) const
{
    if ( xx )
        *xx = (float)(x + m_xScrollPosition * m_xScrollPixelsPerLine);
    if ( yy )
        *yy = (float)(y + m_yScrollPosition * m_yScrollPixelsPerLine);
}

#endif // WXWIN_COMPATIBILITY

#endif // !wxGTK

// vi:sts=4:sw=4:et
