/////////////////////////////////////////////////////////////////////////////
// Name:        src/univ/inpcons.cpp
// Purpose:     wxInputConsumer: mix-in class for input handling
// Author:      Vadim Zeitlin
// Modified by:
// Created:     14.08.00
// RCS-ID:      $Id: inpcons.cpp,v 1.1 2001/09/22 11:54:54 VS Exp $
// Copyright:   (c) 2000 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

#ifdef __GNUG__
    #pragma implementation "inpcons.h"
#endif

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/univ/renderer.h"
#include "wx/univ/inphand.h"
#include "wx/univ/theme.h"

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// focus/activation handling
// ----------------------------------------------------------------------------

void wxInputConsumer::OnFocus(wxFocusEvent& event)
{
    if ( m_inputHandler && m_inputHandler->HandleFocus(this, event) )
        GetInputWindow()->Refresh();
    else
        event.Skip();
}

void wxInputConsumer::OnActivate(wxActivateEvent& event)
{
    if ( m_inputHandler && m_inputHandler->HandleActivation(this, event.GetActive()) )
        GetInputWindow()->Refresh();
    else
        event.Skip();
}

// ----------------------------------------------------------------------------
// input processing
// ----------------------------------------------------------------------------

void wxInputConsumer::CreateInputHandler(const wxString& inphandler)
{
    m_inputHandler = wxTheme::Get()->GetInputHandler(inphandler);
}

void wxInputConsumer::OnKeyDown(wxKeyEvent& event)
{
    if ( !m_inputHandler || !m_inputHandler->HandleKey(this, event, TRUE) )
        event.Skip();
}

void wxInputConsumer::OnKeyUp(wxKeyEvent& event)
{
    if ( !m_inputHandler || !m_inputHandler->HandleKey(this, event, FALSE) )
        event.Skip();
}

void wxInputConsumer::OnMouse(wxMouseEvent& event)
{
    if ( m_inputHandler )
    {
        if ( event.Moving() || event.Entering() || event.Leaving() )
        {
            if ( m_inputHandler->HandleMouseMove(this, event) )
                return;
        }
        else // a click action
        {
            if ( m_inputHandler->HandleMouse(this, event) )
                return;
        }
    }

    event.Skip();
}

// ----------------------------------------------------------------------------
// the actions
// ----------------------------------------------------------------------------

bool wxInputConsumer::PerformAction(const wxControlAction& action,
                                    long numArg,
                                    const wxString& strArg)
{
    return FALSE;
}

