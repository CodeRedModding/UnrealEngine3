/////////////////////////////////////////////////////////////////////////////
// Name:        univ/stattext.cpp
// Purpose:     wxStaticText
// Author:      Vadim Zeitlin
// Modified by:
// Created:     14.08.00
// RCS-ID:      $Id: stattext.cpp,v 1.8 2002/04/14 14:42:42 RR Exp $
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
    #pragma implementation "univstattext.h"
#endif

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_STATTEXT

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
    #include "wx/stattext.h"
    #include "wx/validate.h"
#endif

#include "wx/univ/renderer.h"
#include "wx/univ/theme.h"

// ============================================================================
// implementation
// ============================================================================

IMPLEMENT_ABSTRACT_CLASS(wxStaticText, wxControl)

// ----------------------------------------------------------------------------
// creation
// ----------------------------------------------------------------------------

bool wxStaticText::Create(wxWindow *parent,
                          wxWindowID id,
                          const wxString &label,
                          const wxPoint &pos,
                          const wxSize &size,
                          long style,
                          const wxString &name)
{
    if ( !wxControl::Create(parent, id, pos, size, style, wxDefaultValidator, name) )
        return FALSE;
        
    SetLabel(label);
    SetBestSize(size);

    return TRUE;
}

// ----------------------------------------------------------------------------
// size management
// ----------------------------------------------------------------------------

void wxStaticText::SetLabel(const wxString& label)
{
    wxControl::SetLabel(label);
}

wxSize wxStaticText::DoGetBestClientSize() const
{
    wxStaticText *self = wxConstCast(this, wxStaticText);
    wxClientDC dc(self);
    dc.SetFont(GetFont());
    wxCoord width, height;
    dc.GetMultiLineTextExtent(GetLabel(), &width, &height);

    return wxSize(width, height);
}

// ----------------------------------------------------------------------------
// drawing
// ----------------------------------------------------------------------------

void wxStaticText::DoDraw(wxControlRenderer *renderer)
{
    renderer->DrawLabel();
}

#endif // wxUSE_STATTEXT
