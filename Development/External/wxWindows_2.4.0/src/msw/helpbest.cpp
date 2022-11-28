/////////////////////////////////////////////////////////////////////////////
// Name:        helpbest.cpp
// Purpose:     Tries to load MS HTML Help, falls back to wxHTML upon failure
// Author:      Mattia Barbon
// Modified by:
// Created:     02/04/2001
// RCS-ID:      $Id: helpbest.cpp,v 1.6.2.1 2002/11/09 00:20:15 VS Exp $
// Copyright:   (c) Mattia Barbon
// Licence:   	wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "helpbest.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/defs.h"
#endif

#include "wx/filefn.h"
#include "wx/log.h"

#if wxUSE_HELP && wxUSE_MS_HTML_HELP && defined(__WIN95__) && wxUSE_WXHTML_HELP
#include "wx/msw/helpchm.h"
#include "wx/html/helpctrl.h"
#include "wx/msw/helpbest.h"

IMPLEMENT_DYNAMIC_CLASS( wxBestHelpController, wxHelpControllerBase )

bool wxBestHelpController::Initialize( const wxString& filename )
{
    // try wxCHMHelpController
    wxCHMHelpController* chm = new wxCHMHelpController;

    m_helpControllerType = wxUseChmHelp;
    // do not warn upon failure
    wxLogNull dontWarnOnFailure;

    if( chm->Initialize( GetValidFilename( filename ) ) )
    {
        m_helpController = chm;
        return TRUE;
    }

    // failed
    delete chm;

    // try wxHtmlHelpController
    wxHtmlHelpController* html = new wxHtmlHelpController;

    m_helpControllerType = wxUseHtmlHelp;
    if( html->Initialize( GetValidFilename( filename ) ) )
    {
        m_helpController = html;
        return TRUE;
    }

    // failed
    delete html;

    return FALSE;
}

wxString wxBestHelpController::GetValidFilename( const wxString& filename ) const
{
    wxString tmp = filename;
    ::wxStripExtension( tmp );

    switch( m_helpControllerType )
    {
        case wxUseChmHelp:
            if( ::wxFileExists( tmp + wxT(".chm") ) )
                return tmp + wxT(".chm");

            return filename;

        case wxUseHtmlHelp:
            if( ::wxFileExists( tmp + wxT(".htb") ) )
                return tmp + wxT(".htb");
            if( ::wxFileExists( tmp + wxT(".zip") ) )
                return tmp + wxT(".zip");
            if( ::wxFileExists( tmp + wxT(".hhp") ) )
                return tmp + wxT(".hhp");

            return filename;

        default:
            // we CAN'T get here
            wxFAIL_MSG( wxT("wxBestHelpController: Must call Initialize, first!") );
    }

    return wxEmptyString;
}

#endif
    // wxUSE_HELP && wxUSE_MS_HTML_HELP && defined(__WIN95__) && wxUSE_WXHTML_HELP
