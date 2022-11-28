/////////////////////////////////////////////////////////////////////////////
// Name:        helpbase.h
// Purpose:     Help system base classes
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: helpbase.h,v 1.22.2.1 2002/10/29 21:47:16 RR Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:   	wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_HELPBASEH__
#define _WX_HELPBASEH__

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "helpbase.h"
#endif

#include "wx/defs.h"

#if wxUSE_HELP

#include "wx/object.h"
#include "wx/string.h"
#include "wx/gdicmn.h"
#include "wx/frame.h"

// Flags for SetViewer
#define wxHELP_NETSCAPE     1

// Defines the API for help controllers
class WXDLLEXPORT wxHelpControllerBase: public wxObject
{
public:
    inline wxHelpControllerBase() {}
    inline ~wxHelpControllerBase() {}

    // Must call this to set the filename and server name.
    // server is only required when implementing TCP/IP-based
    // help controllers.
    virtual bool Initialize(const wxString& WXUNUSED(file), int WXUNUSED(server) ) { return FALSE; }
    virtual bool Initialize(const wxString& WXUNUSED(file)) { return FALSE; }

    // Set viewer: only relevant to some kinds of controller
    virtual void SetViewer(const wxString& WXUNUSED(viewer), long WXUNUSED(flags) = 0) {}

    // If file is "", reloads file given  in Initialize
    virtual bool LoadFile(const wxString& file = wxT("")) = 0;

    // Displays the contents
    virtual bool DisplayContents(void) = 0;

    // Display the given section
    virtual bool DisplaySection(int sectionNo) = 0;

    // Display the section using a context id
    virtual bool DisplayContextPopup(int WXUNUSED(contextId)) { return FALSE; };

    // Display the text in a popup, if possible
    virtual bool DisplayTextPopup(const wxString& WXUNUSED(text), const wxPoint& WXUNUSED(pos)) { return FALSE; }

    // By default, uses KeywordSection to display a topic. Implementations
    // may override this for more specific behaviour.
    virtual bool DisplaySection(const wxString& section) { return KeywordSearch(section); }
    virtual bool DisplayBlock(long blockNo) = 0;
    virtual bool KeywordSearch(const wxString& k) = 0;
    /// Allows one to override the default settings for the help frame.
    virtual void SetFrameParameters(const wxString& WXUNUSED(title),
        const wxSize& WXUNUSED(size),
        const wxPoint& WXUNUSED(pos) = wxDefaultPosition,
        bool WXUNUSED(newFrameEachTime) = FALSE)
    {
        // does nothing by default
    }
    /// Obtains the latest settings used by the help frame and the help
    /// frame.
    virtual wxFrame *GetFrameParameters(wxSize *WXUNUSED(size) = NULL,
        wxPoint *WXUNUSED(pos) = NULL,
        bool *WXUNUSED(newFrameEachTime) = NULL)
    {
        return (wxFrame*) NULL; // does nothing by default
    }

    virtual bool Quit() = 0;
    virtual void OnQuit() {}
    
private:
    DECLARE_CLASS(wxHelpControllerBase)
};

#endif // wxUSE_HELP

#endif
// _WX_HELPBASEH__
