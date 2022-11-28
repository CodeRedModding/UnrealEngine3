///////////////////////////////////////////////////////////////////////////////
// Name:        wx/snglinst.h
// Purpose:     wxSingleInstanceChecker can be used to restrict the number of
//              simultaneously running copies of a program to one
// Author:      Vadim Zeitlin
// Modified by:
// Created:     08.06.01
// RCS-ID:      $Id: snglinst.h,v 1.4 2002/08/31 11:29:11 GD Exp $
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SNGLINST_H_
#define _WX_SNGLINST_H_

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "snglinst.h"
#endif

#if wxUSE_SNGLINST_CHECKER

// ----------------------------------------------------------------------------
// wxSingleInstanceChecker
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxSingleInstanceChecker
{
public:
    // default ctor, use Create() after it
    wxSingleInstanceChecker() { Init(); }

    // like Create() but no error checking (dangerous!)
    wxSingleInstanceChecker(const wxString& name,
                            const wxString& path = wxEmptyString)
    {
        Init();
        Create(name, path);
    }

    // name must be given and be as unique as possible, it is used as the mutex
    // name under Win32 and the lock file name under Unix -
    // wxTheApp->GetAppName() may be a good value for this parameter
    //
    // path is optional and is ignored under Win32 and used as the directory to
    // create the lock file in under Unix (default is wxGetHomeDir())
    //
    // returns FALSE if initialization failed, it doesn't mean that another
    // instance is running - use IsAnotherRunning() to check it
    bool Create(const wxString& name, const wxString& path = wxEmptyString);

    // is another copy of this program already running?
    bool IsAnotherRunning() const;

    // dtor is not virtual, this class is not meant to be used polymorphically
    ~wxSingleInstanceChecker();

private:
    // common part of all ctors
    void Init() { m_impl = NULL; }

    // the implementation details (platform specific)
    class WXDLLEXPORT wxSingleInstanceCheckerImpl *m_impl;
};

#endif // wxUSE_SNGLINST_CHECKER

#endif // _WX_SNGLINST_H_
