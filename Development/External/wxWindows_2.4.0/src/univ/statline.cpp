/////////////////////////////////////////////////////////////////////////////
// Name:        univ/statline.cpp
// Purpose:     wxStaticLine implementation
// Author:      Vadim Zeitlin
// Modified by:
// Created:     25.08.00
// RCS-ID:      $Id: statline.cpp,v 1.5 2001/07/04 18:07:15 VZ Exp $
// Copyright:   (c) 2000 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "univstatline.h"
#endif

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_STATLINE

#ifndef WX_PRECOMP
    #include "wx/dc.h"
    #include "wx/validate.h"
#endif

#include "wx/statline.h"

#include "wx/univ/renderer.h"

// ============================================================================
// implementation
// ============================================================================

IMPLEMENT_DYNAMIC_CLASS(wxStaticLine, wxControl)

// ----------------------------------------------------------------------------
// wxStaticLine
// ----------------------------------------------------------------------------

bool wxStaticLine::Create(wxWindow *parent,
                          wxWindowID id,
                          const wxPoint &pos,
                          const wxSize &size,
                          long style,
                          const wxString &name)
{
    if ( !wxControl::Create(parent, id, pos, size, style, wxDefaultValidator, name) )
        return FALSE;

    wxSize sizeReal = AdjustSize(size);
    if ( sizeReal != size )
        SetSize(sizeReal);

    return TRUE;
}

void wxStaticLine::DoDraw(wxControlRenderer *renderer)
{
    // we never have a border, so don't call the base class version whcih draws
    // it
    wxSize sz = GetSize();
    wxCoord x2, y2;
    if ( IsVertical() )
    {
        x2 = 0;
        y2 = sz.y;
    }
    else // horizontal
    {
        x2 = sz.x;
        y2 = 0;
    }

    renderer->DrawLine(0, 0, x2, y2);
}

#endif // wxUSE_STATLINE

