/////////////////////////////////////////////////////////////////////////////
// Name:        helpbest.h
// Purpose:     Tries to load MS HTML Help, falls back to wxHTML upon failure
// Author:      Mattia Barbon
// Modified by:
// Created:     02/04/2001
// RCS-ID:      $Id: helpbest.h,v 1.2 2002/03/09 21:55:39 VZ Exp $
// Copyright:   (c) Mattia Barbon
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_HELPBEST_H_
#define _WX_HELPBEST_H_

#ifdef __GNUG__
#pragma interface "helpbest.h"
#endif

#if wxUSE_HELP && wxUSE_MS_HTML_HELP && defined(__WIN95__) && wxUSE_WXHTML_HELP

#include "wx/helpbase.h"

class WXDLLEXPORT wxBestHelpController: public wxHelpControllerBase
{
public:
    wxBestHelpController()
        : m_helpControllerType( wxUseNone ),
          m_helpController( NULL )
    {
    }

    virtual ~wxBestHelpController() { delete m_helpController; }

    // Must call this to set the filename
    virtual bool Initialize(const wxString& file);

    // If file is "", reloads file given in Initialize
    virtual bool LoadFile(const wxString& file = wxEmptyString)
    {
        return m_helpController->LoadFile( GetValidFilename( file ) );
    }

    virtual bool DisplayContents()
    {
        return m_helpController->DisplayContents();
    }

    virtual bool DisplaySection(int sectionNo)
    {
        return m_helpController->DisplaySection( sectionNo );
    }

    virtual bool DisplaySection(const wxString& section)
    {
        return m_helpController->DisplaySection( section );
    }

    virtual bool DisplayBlock(long blockNo)
    {
        return m_helpController->DisplayBlock( blockNo );
    }

    virtual bool DisplayContextPopup(int contextId)
    {
        return m_helpController->DisplayContextPopup( contextId );
    }

    virtual bool DisplayTextPopup(const wxString& text, const wxPoint& pos)
    {
        return m_helpController->DisplayTextPopup( text, pos );
    }

    virtual bool KeywordSearch(const wxString& k)
    {
        return m_helpController->KeywordSearch( k );
    }

    virtual bool Quit()
    {
        return m_helpController->Quit();
    }

    // Allows one to override the default settings for the help frame.
    virtual void SetFrameParameters(const wxString& title,
                                    const wxSize& size,
                                    const wxPoint& pos = wxDefaultPosition,
                                    bool newFrameEachTime = FALSE)
    {
        m_helpController->SetFrameParameters( title, size, pos,
                                              newFrameEachTime );
    }

    // Obtains the latest settings used by the help frame and the help frame.
    virtual wxFrame *GetFrameParameters(wxSize *size = NULL,
                                        wxPoint *pos = NULL,
                                        bool *newFrameEachTime = NULL)
    {
        return m_helpController->GetFrameParameters( size, pos,
                                                     newFrameEachTime );
    }

protected:
    // Append/change extension if necessary.
    wxString GetValidFilename(const wxString& file) const;

protected:
    enum HelpControllerType { wxUseNone, wxUseHtmlHelp, wxUseChmHelp };

    HelpControllerType m_helpControllerType;
    wxHelpControllerBase* m_helpController;

    DECLARE_DYNAMIC_CLASS(wxBestHelpController)
};

#endif // wxUSE_HELP && wxUSE_MS_HTML_HELP && defined(__WIN95__) && wxUSE_WXHTML_HELP

#endif
    // _WX_HELPBEST_H_
