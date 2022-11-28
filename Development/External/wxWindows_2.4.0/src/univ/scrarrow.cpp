///////////////////////////////////////////////////////////////////////////////
// Name:        src/univ/scrarrow.cpp
// Purpose:     wxScrollArrows class implementation
// Author:      Vadim Zeitlin
// Modified by:
// Created:     22.01.01
// RCS-ID:      $Id: scrarrow.cpp,v 1.8 2001/12/09 02:21:30 VS Exp $
// Copyright:   (c) 2001 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "univscrarrow.h"
#endif

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
#endif

#include "wx/univ/scrtimer.h"
#include "wx/univ/scrarrow.h"

#include "wx/univ/renderer.h"
#include "wx/univ/inphand.h"
#include "wx/univ/theme.h"

// ----------------------------------------------------------------------------
// wxScrollArrowCaptureData: contains the data used while the arrow is being
// pressed by the user
// ----------------------------------------------------------------------------

struct wxScrollArrowCaptureData
{
    wxScrollArrowCaptureData()
    {
        m_arrowPressed = wxScrollArrows::Arrow_None;
        m_window = NULL;
        m_btnCapture = -1;
        m_timerScroll = NULL;
    }

    ~wxScrollArrowCaptureData()
    {
        if ( m_window )
            m_window->ReleaseMouse();

        delete m_timerScroll;
    }

    // the arrow being held pressed (may be Arrow_None)
    wxScrollArrows::Arrow m_arrowPressed;

    // the mouse button which started the capture (-1 if none)
    int m_btnCapture;

    // the window which has captured the mouse
    wxWindow *m_window;

    // the timer for generating the scroll events
    wxScrollTimer *m_timerScroll;
};

// ----------------------------------------------------------------------------
// wxScrollArrowTimer: a wxScrollTimer which calls OnArrow()
// ----------------------------------------------------------------------------

class wxScrollArrowTimer : public wxScrollTimer
{
public:
    wxScrollArrowTimer(wxControlWithArrows *control,
                       wxScrollArrows::Arrow arrow)
    {
        m_control = control;
        m_arrow = arrow;

        StartAutoScroll();
    }

protected:
    virtual bool DoNotify()
    {
        return m_control->OnArrow(m_arrow);
    }

    wxControlWithArrows *m_control;
    wxScrollArrows::Arrow m_arrow;
};

// ============================================================================
// implementation of wxScrollArrows
// ============================================================================

// ----------------------------------------------------------------------------
// con/destruction
// ----------------------------------------------------------------------------

wxScrollArrows::wxScrollArrows(wxControlWithArrows *control)
{
    m_control = control;
    m_captureData = NULL;
}

wxScrollArrows::~wxScrollArrows()
{
    // it should have been destroyed
    wxASSERT_MSG( !m_captureData, _T("memory leak in wxScrollArrows") );
}

// ----------------------------------------------------------------------------
// drawing
// ----------------------------------------------------------------------------

void wxScrollArrows::DrawArrow(Arrow arrow,
                               wxDC& dc,
                               const wxRect& rect,
                               bool scrollbarLike) const
{
    static const wxDirection arrowDirs[2][Arrow_Max] =
    {
        { wxLEFT, wxRIGHT },
        { wxUP,   wxDOWN  }
    };

    if ( scrollbarLike )
        m_control->GetRenderer()->DrawScrollbarArrow(
            dc,
            arrowDirs[m_control->IsVertical()][arrow],
            rect,
            m_control->GetArrowState(arrow));
    else
        m_control->GetRenderer()->DrawArrow(
            dc,
            arrowDirs[m_control->IsVertical()][arrow],
            rect,
            m_control->GetArrowState(arrow));
}

// ----------------------------------------------------------------------------
// input handling
// ----------------------------------------------------------------------------

void wxScrollArrows::UpdateCurrentFlag(Arrow arrow, Arrow arrowCur) const
{
    m_control->SetArrowFlag(arrow, wxCONTROL_CURRENT, arrow == arrowCur);
}

bool wxScrollArrows::HandleMouseMove(const wxMouseEvent& event) const
{
    Arrow arrow;
    if ( event.Leaving() )
    {
        // no arrow has mouse if it left the window completely
        arrow = Arrow_None;
    }
    else // Moving() or Entering(), treat them the same here
    {
        arrow = m_control->HitTest(event.GetPosition());
    }

    if ( m_captureData )
    {
        // the mouse is captured, we may want to pause scrolling if it goes
        // outside the arrow or to resume it if we had paused it before
        wxTimer *timer = m_captureData->m_timerScroll;
        if ( !timer->IsRunning() )
        {
            // timer is paused
            if ( arrow == m_captureData->m_arrowPressed )
            {
                // resume now
                m_control->SetArrowFlag(arrow, wxCONTROL_PRESSED, TRUE);
                timer->Start();

                return TRUE;
            }
        }
        else if ( 1 ) //FIXME: m_control->ShouldPauseScrolling() )
        {
            // we may want to stop it
            if ( arrow != m_captureData->m_arrowPressed )
            {
                // stop as the mouse left the arrow
                m_control->SetArrowFlag(m_captureData->m_arrowPressed,
                                        wxCONTROL_PRESSED, FALSE);
                timer->Stop();

                return TRUE;
            }
        }

        return FALSE;
    }

    // reset the wxCONTROL_CURRENT flag for the arrows which don't have the
    // mouse and set it for the one which has
    UpdateCurrentFlag(Arrow_First, arrow);
    UpdateCurrentFlag(Arrow_Second, arrow);

    // return TRUE if it was really an event for an arrow
    return !event.Leaving() && arrow != Arrow_None;
}

bool wxScrollArrows::HandleMouse(const wxMouseEvent& event) const
{
    int btn = event.GetButton();
    if ( btn == -1 )
    {
        // we only care about button press/release events
        return FALSE;
    }

    if ( event.ButtonDown() || event.ButtonDClick() )
    {
        if ( !m_captureData )
        {
            Arrow arrow = m_control->HitTest(event.GetPosition());
            if ( arrow == Arrow_None )
            {
                // mouse pressed over something else
                return FALSE;
            }

            if ( m_control->GetArrowState(arrow) & wxCONTROL_DISABLED )
            {
                // don't allow to press disabled arrows
                return TRUE;
            }

            wxConstCast(this, wxScrollArrows)->m_captureData =
                new wxScrollArrowCaptureData;
            m_captureData->m_arrowPressed = arrow;
            m_captureData->m_btnCapture = btn;
            m_captureData->m_window = m_control->GetWindow();
            m_captureData->m_window->CaptureMouse();

            // start scrolling
            m_captureData->m_timerScroll =
                new wxScrollArrowTimer(m_control, arrow);

            m_control->SetArrowFlag(arrow, wxCONTROL_PRESSED, TRUE);
        }
        //else: mouse already captured, nothing to do
    }
    // release mouse if the *same* button went up
    else if ( m_captureData && (btn == m_captureData->m_btnCapture) )
    {
        Arrow arrow = m_captureData->m_arrowPressed;

        delete m_captureData;
        wxConstCast(this, wxScrollArrows)->m_captureData = NULL;

        m_control->SetArrowFlag(arrow, wxCONTROL_PRESSED, FALSE);
    }
    else
    {
        // we don't process this
        return FALSE;
    }

    return TRUE;
}

