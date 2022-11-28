///////////////////////////////////////////////////////////////////////////////
// Name:        msw/snglinst.cpp
// Purpose:     implements wxSingleInstanceChecker class for Win32 using
//              named mutexes
// Author:      Vadim Zeitlin
// Modified by:
// Created:     08.06.01
// RCS-ID:      $Id: snglinst.cpp,v 1.4 2002/07/21 12:41:38 VZ Exp $
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// License:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "snglinst.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_SNGLINST_CHECKER && defined(__WIN32__)

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/log.h"
#endif //WX_PRECOMP

#include "wx/snglinst.h"

#include "wx/msw/private.h"

// ----------------------------------------------------------------------------
// wxSingleInstanceCheckerImpl: the real implementation class
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxSingleInstanceCheckerImpl
{
public:
    wxSingleInstanceCheckerImpl()
    {
        // we don't care about m_wasOpened, it can't be accessed before being
        // initialized
        m_hMutex = NULL;
    }

    bool Create(const wxString& name)
    {
        m_hMutex = ::CreateMutex(NULL, FALSE, name);
        if ( !m_hMutex )
        {
            wxLogLastError(_T("CreateMutex"));

            return FALSE;
        }

        // mutex was either created or opened - see what really happened
        m_wasOpened = ::GetLastError() == ERROR_ALREADY_EXISTS;

        return TRUE;
    }

    bool WasOpened() const
    {
        wxCHECK_MSG( m_hMutex, FALSE,
                     _T("can't be called if mutex creation failed") );

        return m_wasOpened;
    }

    ~wxSingleInstanceCheckerImpl()
    {
        if ( m_hMutex )
        {
            if ( !::CloseHandle(m_hMutex) )
            {
                wxLogLastError(_T("CloseHandle(mutex)"));
            }
        }
    }

private:
    // the result of the CreateMutex() call
    bool m_wasOpened;

    // the mutex handle, may be NULL
    HANDLE m_hMutex;
};

// ============================================================================
// wxSingleInstanceChecker implementation
// ============================================================================

bool wxSingleInstanceChecker::Create(const wxString& name,
                                     const wxString& WXUNUSED(path))
{
    wxASSERT_MSG( !m_impl,
                  _T("calling wxSingleInstanceChecker::Create() twice?") );

    // creating unnamed mutex doesn't have the same semantics!
    wxASSERT_MSG( !name.empty(), _T("mutex name can't be empty") );

    m_impl = new wxSingleInstanceCheckerImpl;

    return m_impl->Create(name);
}

bool wxSingleInstanceChecker::IsAnotherRunning() const
{
    wxCHECK_MSG( m_impl, FALSE, _T("must call Create() first") );

    // if the mutex had been opened, another instance is running - otherwise we
    // would have created it
    return m_impl->WasOpened();
}

wxSingleInstanceChecker::~wxSingleInstanceChecker()
{
    delete m_impl;
}

#endif // wxUSE_SNGLINST_CHECKER
