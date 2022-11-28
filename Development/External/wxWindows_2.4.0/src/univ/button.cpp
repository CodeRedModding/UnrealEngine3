/////////////////////////////////////////////////////////////////////////////
// Name:        univ/button.cpp
// Purpose:     wxButton
// Author:      Vadim Zeitlin
// Modified by:
// Created:     14.08.00
// RCS-ID:      $Id: button.cpp,v 1.15 2002/05/19 22:44:27 VS Exp $
// Copyright:   (c) 2000 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "univbutton.h"
#endif

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_BUTTON

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
    #include "wx/dcscreen.h"
    #include "wx/button.h"
    #include "wx/validate.h"
    #include "wx/settings.h"
#endif

#include "wx/univ/renderer.h"
#include "wx/univ/inphand.h"
#include "wx/univ/theme.h"
#include "wx/univ/colschem.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// default margins around the image
static const wxCoord DEFAULT_BTN_MARGIN_X = 0;  // We should give space for the border, at least.
static const wxCoord DEFAULT_BTN_MARGIN_Y = 0;

// ============================================================================
// implementation
// ============================================================================

IMPLEMENT_DYNAMIC_CLASS(wxButton, wxControl)

// ----------------------------------------------------------------------------
// creation
// ----------------------------------------------------------------------------

void wxButton::Init()
{
    m_isPressed =
    m_isDefault = FALSE;
}

bool wxButton::Create(wxWindow *parent,
                      wxWindowID id,
                      const wxBitmap& bitmap,
                      const wxString &label,
                      const wxPoint &pos,
                      const wxSize &size,
                      long style,
                      const wxValidator& validator,
                      const wxString &name)
{
    // center label by default
    if ( !(style & wxALIGN_MASK) )
    {
        style |= wxALIGN_CENTRE_HORIZONTAL | wxALIGN_CENTRE_VERTICAL;
    }

    if ( !wxControl::Create(parent, id, pos, size, style, wxDefaultValidator, name) )
        return FALSE;

    SetLabel(label);
    SetImageLabel(bitmap);
    // SetBestSize(size); -- called by SetImageLabel()

    CreateInputHandler(wxINP_HANDLER_BUTTON);

    return TRUE;
}

wxButton::~wxButton()
{
}

// ----------------------------------------------------------------------------
// size management
// ----------------------------------------------------------------------------

/* static */
wxSize wxButtonBase::GetDefaultSize()
{
    static wxSize s_sizeBtn;

    if ( s_sizeBtn.x == 0 )
    {
        wxScreenDC dc;

        // this corresponds more or less to wxMSW standard in Win32 theme (see
        // wxWin32Renderer::AdjustSize())
        s_sizeBtn.x = 8*dc.GetCharWidth();
        s_sizeBtn.y = (11*dc.GetCharHeight())/10 + 2;
    }

    return s_sizeBtn;
}

wxSize wxButton::DoGetBestClientSize() const
{
    wxClientDC dc(wxConstCast(this, wxButton));
    wxCoord width, height;
    dc.GetMultiLineTextExtent(GetLabel(), &width, &height);

    if ( m_bitmap.Ok() )
    {
        // allocate extra space for the bitmap
        wxCoord heightBmp = m_bitmap.GetHeight() + 2*m_marginBmpY;
        if ( height < heightBmp )
            height = heightBmp;

        width += m_bitmap.GetWidth() + 2*m_marginBmpX;
    }

    // for compatibility with other ports, the buttons default size is never
    // less than the standard one, but not when display not PDAs.
    if (wxSystemSettings::GetScreenType() > wxSYS_SCREEN_PDA)
    {
        if ( !(GetWindowStyle() & wxBU_EXACTFIT) )
        {
            wxSize szDef = GetDefaultSize();
            if ( width < szDef.x )
                width = szDef.x;
        }
    }

    return wxSize(width, height);
}

// ----------------------------------------------------------------------------
// drawing
// ----------------------------------------------------------------------------

void wxButton::DoDraw(wxControlRenderer *renderer)
{
    if ( !(GetWindowStyle() & wxBORDER_NONE) )
    {
        renderer->DrawButtonBorder();
    }

    renderer->DrawLabel(m_bitmap, m_marginBmpX, m_marginBmpY);
}

bool wxButton::DoDrawBackground(wxDC& dc)
{
    wxRect rect;
    wxSize size = GetSize();
    rect.width = size.x;
    rect.height = size.y;
    
    if ( GetBackgroundBitmap().Ok() )
    {
        // get the bitmap and the flags
        int alignment;
        wxStretch stretch;
        wxBitmap bmp = GetBackgroundBitmap(&alignment, &stretch);
        wxControlRenderer::DrawBitmap(dc, bmp, rect, alignment, stretch);
    }
    else
    {
        m_renderer->DrawButtonSurface(dc, wxTHEME_BG_COLOUR(this),
                                      rect, GetStateFlags());
    }

    return TRUE;
}

// ----------------------------------------------------------------------------
// input processing
// ----------------------------------------------------------------------------

void wxButton::Press()
{
    if ( !m_isPressed )
    {
        m_isPressed = TRUE;

        Refresh();
    }
}

void wxButton::Release()
{
    if ( m_isPressed )
    {
        m_isPressed = FALSE;

        Refresh();
    }
}

void wxButton::Toggle()
{
    if ( m_isPressed )
        Release();
    else
        Press();

    if ( !m_isPressed )
    {
        // releasing button after it had been pressed generates a click event
        Click();
    }
}

void wxButton::Click()
{
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, GetId());
    InitCommandEvent(event);
    Command(event);
}

bool wxButton::PerformAction(const wxControlAction& action,
                             long numArg,
                             const wxString& strArg)
{
    if ( action == wxACTION_BUTTON_TOGGLE )
        Toggle();
    else if ( action == wxACTION_BUTTON_CLICK )
        Click();
    else if ( action == wxACTION_BUTTON_PRESS )
        Press();
    else if ( action == wxACTION_BUTTON_RELEASE )
        Release();
    else
        return wxControl::PerformAction(action, numArg, strArg);

    return TRUE;
}

// ----------------------------------------------------------------------------
// misc
// ----------------------------------------------------------------------------

void wxButton::SetImageLabel(const wxBitmap& bitmap)
{
    m_bitmap = bitmap;

    SetImageMargins(DEFAULT_BTN_MARGIN_X, DEFAULT_BTN_MARGIN_Y);
}

void wxButton::SetImageMargins(wxCoord x, wxCoord y)
{
    m_marginBmpX = x + 2;
    m_marginBmpY = y + 2;
    
    SetBestSize(wxDefaultSize);
}

void wxButton::SetDefault()
{
    m_isDefault = TRUE;
}

// ============================================================================
// wxStdButtonInputHandler
// ============================================================================

wxStdButtonInputHandler::wxStdButtonInputHandler(wxInputHandler *handler)
                       : wxStdInputHandler(handler)
{
    m_winCapture = NULL;
    m_winHasMouse = FALSE;
}

bool wxStdButtonInputHandler::HandleKey(wxInputConsumer *consumer,
                                        const wxKeyEvent& event,
                                        bool pressed)
{
    int keycode = event.GetKeyCode();
    if ( keycode == WXK_SPACE || keycode == WXK_RETURN )
    {
        consumer->PerformAction(wxACTION_BUTTON_TOGGLE);

        return TRUE;
    }

    return wxStdInputHandler::HandleKey(consumer, event, pressed);
}

bool wxStdButtonInputHandler::HandleMouse(wxInputConsumer *consumer,
                                          const wxMouseEvent& event)
{
    // the button has 2 states: pressed and normal with the following
    // transitions between them:
    //
    //      normal -> left down -> capture mouse and go to pressed state
    //      pressed -> left up inside -> generate click -> go to normal
    //                         outside ------------------>
    //
    // the other mouse buttons are ignored
    if ( event.Button(1) )
    {
        if ( event.LeftDown() || event.LeftDClick() )
        {
            m_winCapture = consumer->GetInputWindow();
            m_winCapture->CaptureMouse();
            m_winHasMouse = TRUE;

            consumer->PerformAction(wxACTION_BUTTON_PRESS);

            return TRUE;
        }
        else if ( event.LeftUp() )
        {
            if ( m_winCapture )
            {
                m_winCapture->ReleaseMouse();
                m_winCapture = NULL;
            }

            if ( m_winHasMouse )
            {
                // this will generate a click event
                consumer->PerformAction(wxACTION_BUTTON_TOGGLE);

                return TRUE;
            }
            //else: the mouse was released outside the window, this doesn't
            //      count as a click
        }
        //else: don't do anything special about the double click
    }

    return wxStdInputHandler::HandleMouse(consumer, event);
}

bool wxStdButtonInputHandler::HandleMouseMove(wxInputConsumer *consumer,
                                              const wxMouseEvent& event)
{
    // we only have to do something when the mouse leaves/enters the pressed
    // button and don't care about the other ones
    if ( event.GetEventObject() == m_winCapture )
    {
        // leaving the button should remove its pressed state
        if ( event.Leaving() )
        {
            // remember that the mouse is now outside
            m_winHasMouse = FALSE;

            // we do have a pressed button, so release it
            consumer->GetInputWindow()->SetCurrent(FALSE);
            consumer->PerformAction(wxACTION_BUTTON_RELEASE);

            return TRUE;
        }
        // and entering it back should make it pressed again if it had been
        // pressed
        else if ( event.Entering() )
        {
            // the mouse is (back) inside the button
            m_winHasMouse = TRUE;

            // we did have a pressed button which we released when leaving the
            // window, press it again
            consumer->GetInputWindow()->SetCurrent(TRUE);
            consumer->PerformAction(wxACTION_BUTTON_PRESS);

            return TRUE;
        }
    }

    return wxStdInputHandler::HandleMouseMove(consumer, event);
}

bool wxStdButtonInputHandler::HandleFocus(wxInputConsumer *consumer,
                                          const wxFocusEvent& event)
{
    // buttons change appearance when they get/lose focus, so return TRUE to
    // refresh
    return TRUE;
}

bool wxStdButtonInputHandler::HandleActivation(wxInputConsumer *consumer,
                                               bool activated)
{
    // the default button changes appearance when the app is [de]activated, so
    // return TRUE to refresh
    return wxStaticCast(consumer->GetInputWindow(), wxButton)->IsDefault();
}

#endif // wxUSE_BUTTON

