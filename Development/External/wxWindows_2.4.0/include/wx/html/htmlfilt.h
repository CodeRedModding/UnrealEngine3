/////////////////////////////////////////////////////////////////////////////
// Name:        htmlfilt.h
// Purpose:     filters
// Author:      Vaclav Slavik
// RCS-ID:      $Id: htmlfilt.h,v 1.4.2.1 2002/11/10 23:58:51 VS Exp $
// Copyright:   (c) 1999 Vaclav Slavik
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////


#ifndef _WX_HTMLFILT_H_
#define _WX_HTMLFILT_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "htmlfilt.h"
#endif

#include "wx/defs.h"

#if wxUSE_HTML

#include "wx/filesys.h"


//--------------------------------------------------------------------------------
// wxHtmlFilter
//                  This class is input filter. It can "translate" files
//                  in non-HTML format to HTML format
//                  interface to access certain
//                  kinds of files (HTPP, FTP, local, tar.gz etc..)
//--------------------------------------------------------------------------------

class WXDLLEXPORT wxHtmlFilter : public wxObject
{
    DECLARE_ABSTRACT_CLASS(wxHtmlFilter)

public:
    wxHtmlFilter() : wxObject() {}
    virtual ~wxHtmlFilter() {}

    // returns TRUE if this filter is able to open&read given file
    virtual bool CanRead(const wxFSFile& file) const = 0;

    // Reads given file and returns HTML document.
    // Returns empty string if opening failed
    virtual wxString ReadFile(const wxFSFile& file) const = 0;
};



//--------------------------------------------------------------------------------
// wxHtmlFilterPlainText
//                  This filter is used as default filter if no other can
//                  be used (= uknown type of file). It is used by
//                  wxHtmlWindow itself.
//--------------------------------------------------------------------------------


class WXDLLEXPORT wxHtmlFilterPlainText : public wxHtmlFilter
{
    DECLARE_DYNAMIC_CLASS(wxHtmlFilterPlainText)

public:
    virtual bool CanRead(const wxFSFile& file) const;
    virtual wxString ReadFile(const wxFSFile& file) const;
};

//--------------------------------------------------------------------------------
// wxHtmlFilterHTML
//          filter for text/html
//--------------------------------------------------------------------------------

class wxHtmlFilterHTML : public wxHtmlFilter
{
    DECLARE_DYNAMIC_CLASS(wxHtmlFilterHTML)

    public:
        virtual bool CanRead(const wxFSFile& file) const;
        virtual wxString ReadFile(const wxFSFile& file) const;
};



#endif // wxUSE_HTML

#endif // _WX_HTMLFILT_H_

