///////////////////////////////////////////////////////////////////////////////
// Name:        univ/theme.cpp
// Purpose:     implementation of wxTheme
// Author:      Vadim Zeitlin
// Modified by:
// Created:     06.08.00
// RCS-ID:      $Id: theme.cpp,v 1.9 2002/06/14 14:17:29 CE Exp $
// Copyright:   (c) 2000 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "theme.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
#endif // WX_PRECOMP

#include "wx/univ/renderer.h"
#include "wx/univ/inphand.h"
#include "wx/univ/theme.h"

// ============================================================================
// implementation
// ============================================================================

wxThemeInfo *wxTheme::ms_allThemes = (wxThemeInfo *)NULL;
wxTheme *wxTheme::ms_theme = (wxTheme *)NULL;

// ----------------------------------------------------------------------------
// "dynamic" theme creation
// ----------------------------------------------------------------------------

wxThemeInfo::wxThemeInfo(Constructor c,
                         const wxChar *n,
                         const wxChar *d)
           : name(n), desc(d), ctor(c)
{
    // insert us (in the head of) the linked list
    next = wxTheme::ms_allThemes;
    wxTheme::ms_allThemes = this;
}

/* static */ wxTheme *wxTheme::Create(const wxString& name)
{
    // find the theme in the list by name
    wxThemeInfo *info = ms_allThemes;
    while ( info )
    {
        if ( name.CmpNoCase(info->name) == 0 )
        {
            return info->ctor();
        }

        info = info->next;
    }

    return (wxTheme *)NULL;
}

// ----------------------------------------------------------------------------
// the default theme (called by wxApp::OnInitGui)
// ----------------------------------------------------------------------------

/* static */ bool wxTheme::CreateDefault()
{
    if ( ms_theme )
    {
        // we already have a theme
        return TRUE;
    }

    wxString nameDefTheme;

    // use the environment variable first
    const wxChar *p = wxGetenv(_T("WXTHEME"));
    if ( p )
    {
        nameDefTheme = p;
    }
    else // use native theme by default
    {
        #if defined(__WXGTK__)
            nameDefTheme = _T("gtk");
        #elif defined(__WXX11__)
            nameDefTheme = _T("win32");
        #else
            nameDefTheme = _T("win32");
        #endif
    }

    ms_theme = Create(nameDefTheme);

    // fallback to the first one in the list
    if ( !ms_theme && ms_allThemes )
    {
        ms_theme = ms_allThemes->ctor();
    }

    // abort if still nothing
    if ( !ms_theme )
    {
        wxLogError(_("Failed to initialize GUI: no built-in themes found."));

        return FALSE;
    }

    return TRUE;
}

/* static */ wxTheme *wxTheme::Set(wxTheme *theme)
{
    wxTheme *themeOld = ms_theme;
    ms_theme = theme;
    return themeOld;
}

// ----------------------------------------------------------------------------
// assorted trivial dtors
// ----------------------------------------------------------------------------

wxTheme::~wxTheme()
{
}

