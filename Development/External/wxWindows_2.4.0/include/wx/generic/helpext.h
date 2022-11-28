/*-*- c++ -*-********************************************************
 * helpext.h - an external help controller for wxWindows             *
 *                                                                  *
 * (C) 1998 by Karsten Ball�der (Ballueder@usa.net)                 *
 *                                                                  *
 * $Id: helpext.h,v 1.11 2002/08/31 11:29:12 GD Exp $
 *******************************************************************/

#ifndef __WX_HELPEXT_H_
#define __WX_HELPEXT_H_

#if wxUSE_HELP

#if defined(__GNUG__) && !defined(__APPLE__)
#   pragma interface "wxexthlp.h"
#endif

#include "wx/generic/helphtml.h"

#ifndef WXEXTHELP_DEFAULTBROWSER
/// Default browser name.
#   define WXEXTHELP_DEFAULTBROWSER _T("netscape")
/// Is default browse a variant of netscape?
#   define WXEXTHELP_DEFAULTBROWSER_IS_NETSCAPE TRUE
#endif

/**
   This class implements help via an external browser.
   It requires the name of a directory containing the documentation
   and a file mapping numerical Section numbers to relative URLS.

   The map file contains two or three fields per line:
   numeric_id  relative_URL  [; comment/documentation]

   The numeric_id is the id used to look up the entry in
   DisplaySection()/DisplayBlock(). The relative_URL is a filename of
   an html file, relative to the help directory. The optional
   comment/documentation field (after a ';') is used for keyword
   searches, so some meaningful text here does not hurt.
   If the documentation itself contains a ';', only the part before
   that will be displayed in the listbox, but all of it used for search.

   Lines starting with ';' will be ignored.
*/

class WXDLLEXPORT wxExtHelpController : public wxHTMLHelpControllerBase
{      
DECLARE_CLASS(wxExtHelpController)
   public:
   wxExtHelpController(void);

   /** Tell it which browser to use.
       The Netscape support will check whether Netscape is already
       running (by looking at the .netscape/lock file in the user's
       home directory) and tell it to load the page into the existing
       window. 
       @param browsername The command to call a browser/html viewer.
       @param isNetscape Set this to TRUE if the browser is some variant of Netscape.
   */
   // Obsolete form
   void SetBrowser(wxString const & browsername = WXEXTHELP_DEFAULTBROWSER,
                   bool isNetscape = WXEXTHELP_DEFAULTBROWSER_IS_NETSCAPE);

  // Set viewer: new name for SetBrowser
  virtual void SetViewer(const wxString& viewer = WXEXTHELP_DEFAULTBROWSER, long flags = wxHELP_NETSCAPE);

 private:
   /// How to call the html viewer.
   wxString         m_BrowserName;
   /// Is the viewer a variant of netscape?
   bool             m_BrowserIsNetscape;
   /// Call the browser using a relative URL.
   virtual bool DisplayHelp(const wxString&);
};

#endif // wxUSE_HELP

#endif // __WX_HELPEXT_H_
