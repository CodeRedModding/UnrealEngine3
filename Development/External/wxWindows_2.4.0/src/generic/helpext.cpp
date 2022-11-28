/////////////////////////////////////////////////////////////////////////////
// Name:        helpext.cpp
// Purpose:     an external help controller for wxWindows
// Author:      Karsten Ballueder
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: helpext.cpp,v 1.23.2.1 2002/10/29 21:47:53 RR Exp $
// Copyright:   (c) Karsten Ballueder
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#   pragma implementation "wxexthlp.h"
#endif

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_HELP

#ifndef WX_PRECOMP
    #include "wx/setup.h"
    #include "wx/string.h"
    #include "wx/utils.h"
    #include "wx/list.h"
    #include "wx/intl.h"
#endif

#include "wx/helpbase.h"
#include "wx/generic/helpext.h"

#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

#if !defined(__WINDOWS__) && !defined(__OS2__)
    #include   <unistd.h>
#endif

#ifdef __WXMSW__
#include <windows.h>
#endif

IMPLEMENT_CLASS(wxExtHelpController, wxHTMLHelpControllerBase)

/// Name of environment variable to set help browser.
#define   WXEXTHELP_ENVVAR_BROWSER   wxT("WX_HELPBROWSER")
/// Is browser a netscape browser?
#define   WXEXTHELP_ENVVAR_BROWSERISNETSCAPE wxT("WX_HELPBROWSER_NS")

/**
   This class implements help via an external browser.
   It requires the name of a directory containing the documentation
   and a file mapping numerical Section numbers to relative URLS.
*/

wxExtHelpController::wxExtHelpController(void)
{
   m_BrowserName = WXEXTHELP_DEFAULTBROWSER;
   m_BrowserIsNetscape = WXEXTHELP_DEFAULTBROWSER_IS_NETSCAPE;

   wxChar *browser = wxGetenv(WXEXTHELP_ENVVAR_BROWSER);
   if(browser)
   {
      m_BrowserName = browser;
      browser = wxGetenv(WXEXTHELP_ENVVAR_BROWSERISNETSCAPE);
      m_BrowserIsNetscape = browser && (wxAtoi(browser) != 0);
   }
}


void wxExtHelpController::SetBrowser(wxString const & browsername, bool isNetscape)
{
   m_BrowserName = browsername;
   m_BrowserIsNetscape = isNetscape;
}

// Set viewer: new, generic name for SetBrowser
void wxExtHelpController::SetViewer(const wxString& viewer, long flags)
{
    SetBrowser(viewer, ((flags & wxHELP_NETSCAPE) == wxHELP_NETSCAPE));
}

bool
wxExtHelpController::DisplayHelp(const wxString &relativeURL)
{
   wxBusyCursor b; // display a busy cursor


#if defined(__WXMSW__)
   wxString url;
   url << m_MapFile << '\\' << relativeURL.BeforeFirst('#');
   bool bOk = (int)ShellExecute(NULL, wxT("open"), url.c_str(),
                                NULL, NULL, SW_SHOWNORMAL ) > 32;
   if ( !bOk )
   {
      wxLogSysError(_("Cannot open URL '%s'"), relativeURL.c_str());
      return false;
   }
   else
      return true;

#elif  defined(__WXPM__)

   wxString url;
   url << m_MapFile << '\\' << relativeURL.BeforeFirst('#');
//   will have to fix for OS/2, later.....DW
//   bool bOk = (int)ShellExecute(NULL, "open", url,
//                                NULL, NULL, SW_SHOWNORMAL ) > 32;
//   if ( !bOk )
//   {
//      wxLogSysError(_("Cannot open URL '%s'"), relativeURL.c_str());
//      return false;
//   }
//   else
      return TRUE;

#elif defined(__DOS__)

   wxString command;
   command = m_BrowserName;
   command << wxT(" file://")
           << m_MapFile << WXEXTHELP_SEPARATOR << relativeURL;
   return wxExecute(command) != 0;

#else // UNIX
   wxString command;

#ifndef __EMX__
   if(m_BrowserIsNetscape) // try re-loading first
   {
      wxString lockfile;
      wxGetHomeDir(&lockfile);
#ifdef __VMS__
      lockfile << WXEXTHELP_SEPARATOR << wxT(".netscape]lock.");
      struct stat statbuf;
      if(stat(lockfile.fn_str(), &statbuf) == 0)
#else
      lockfile << WXEXTHELP_SEPARATOR << wxT(".netscape/lock");
      struct stat statbuf;
      if(lstat(lockfile.fn_str(), &statbuf) == 0)
      // cannot use wxFileExists, because it's a link pointing to a
      // non-existing location      if(wxFileExists(lockfile))
#endif
      {
         long success;
         command << m_BrowserName << wxT(" -remote openURL(")
                 << wxT("file://") << m_MapFile
                 << WXEXTHELP_SEPARATOR << relativeURL << wxT(")");
         success = wxExecute(command);
         if(success != 0 ) // returns PID on success
            return TRUE;
      }
   }
#endif
   command = m_BrowserName;
   command << wxT(" file://")
           << m_MapFile << WXEXTHELP_SEPARATOR << relativeURL;
   return wxExecute(command) != 0;
#endif
}

#endif // wxUSE_HELP

