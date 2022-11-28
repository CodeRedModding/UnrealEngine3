/////////////////////////////////////////////////////////////////////////////
// Name:        dirdlg.cpp
// Purpose:     wxDirDialog
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: dirdlg.cpp,v 1.24 2002/06/08 15:11:49 VZ Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "dirdlg.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_DIRDLG

#if defined(__WIN95__) && !defined(__GNUWIN32_OLD__) && wxUSE_OLE

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/dialog.h"
    #include "wx/dirdlg.h"
    #include "wx/log.h"
    #include "wx/app.h"     // for GetComCtl32Version()
#endif

#include "wx/msw/private.h"

#include <shlobj.h> // Win95 shell

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#ifndef MAX_PATH
    #define MAX_PATH 4096      // be generous
#endif

// ----------------------------------------------------------------------------
// wxWindows macros
// ----------------------------------------------------------------------------

IMPLEMENT_CLASS(wxDirDialog, wxDialog)

// ----------------------------------------------------------------------------
// private functions prototypes
// ----------------------------------------------------------------------------

// free the parameter
static void ItemListFree(LPITEMIDLIST pidl);

// the callback proc for the dir dlg
static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp,
                                       LPARAM pData);


// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxDirDialog
// ----------------------------------------------------------------------------

wxDirDialog::wxDirDialog(wxWindow *parent,
                         const wxString& message,
                         const wxString& defaultPath,
                         long style,
                         const wxPoint& WXUNUSED(pos),
                         const wxSize& WXUNUSED(size),
                         const wxString& WXUNUSED(name))
{
    m_message = message;
    m_parent = parent;

    SetStyle(style);
    SetPath(defaultPath);
}

void wxDirDialog::SetPath(const wxString& path)
{
    m_path = path;

    // SHBrowseForFolder doesn't like '/'s nor the trailing backslashes
    m_path.Replace(_T("/"), _T("\\"));
    if ( !m_path.empty() )
    {
        while ( *(m_path.end() - 1) == _T('\\') )
        {
            m_path.erase(m_path.length() - 1);
        }

        // but the root drive should have a trailing slash (again, this is just
        // the way the native dialog works)
        if ( *(m_path.end() - 1) == _T(':') )
        {
            m_path += _T('\\');
        }
    }
}

#ifndef BIF_NEWDIALOGSTYLE
#define BIF_NEWDIALOGSTYLE 0x0040
#endif

int wxDirDialog::ShowModal()
{
    wxWindow *parent = GetParent();

    BROWSEINFO bi;
    bi.hwndOwner      = parent ? GetHwndOf(parent) : NULL;
    bi.pidlRoot       = NULL;
    bi.pszDisplayName = NULL;
    bi.lpszTitle      = m_message.c_str();
    bi.ulFlags        = BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT;
    bi.lpfn           = BrowseCallbackProc;
    bi.lParam         = (LPARAM)m_path.c_str();    // param for the callback

    if ((GetStyle() & wxDD_NEW_DIR_BUTTON) &&
        (wxApp::GetComCtl32Version() >= 500))
    {
        bi.ulFlags |= BIF_NEWDIALOGSTYLE;
    }

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);

    if ( bi.pidlRoot )
    {
        ItemListFree((LPITEMIDLIST)bi.pidlRoot);
    }

    if ( !pidl )
    {
        // Cancel button pressed
        return wxID_CANCEL;
    }

    BOOL ok = SHGetPathFromIDList(pidl, m_path.GetWriteBuf(MAX_PATH));
    m_path.UngetWriteBuf();

    ItemListFree(pidl);

    if ( !ok )
    {
        wxLogLastError(wxT("SHGetPathFromIDList"));

        return wxID_CANCEL;
    }

    return wxID_OK;
}

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

static int CALLBACK
BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
    switch(uMsg)
    {
        case BFFM_INITIALIZED:
            // sent immediately after initialisation and so we may set the
            // initial selection here
            //
            // wParam = TRUE => lParam is a string and not a PIDL
            SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
            break;

        case BFFM_SELCHANGED:
            {
                // Set the status window to the currently selected path.
                TCHAR szDir[MAX_PATH];
                if ( SHGetPathFromIDList((LPITEMIDLIST)lp, szDir) )
                {
                    wxString strDir(szDir);
                    int maxChars = 40; // Have to truncate string else it displays incorrectly
                    if (strDir.Len() > (size_t) (maxChars - 3))
                    {
                        strDir = strDir.Right(maxChars - 3);
                        strDir = wxString(wxT("...")) + strDir;
                    }
                    SendMessage(hwnd, BFFM_SETSTATUSTEXT, 0, (LPARAM) (const wxChar*) strDir);
                }
            }
            break;

        //case BFFM_VALIDATEFAILED: -- might be used to provide custom message
        //                             if the user types in invalid dir name
    }

    return 0;
}


static void ItemListFree(LPITEMIDLIST pidl)
{
    if ( pidl )
    {
        LPMALLOC pMalloc;
        SHGetMalloc(&pMalloc);
        if ( pMalloc )
        {
            pMalloc->Free(pidl);
            pMalloc->Release();
        }
        else
        {
            wxLogLastError(wxT("SHGetMalloc"));
        }
    }
}

#else
    #include "../generic/dirdlgg.cpp"
#endif // compiler/platform on which the code here compiles

#endif // wxUSE_DIRDLG
