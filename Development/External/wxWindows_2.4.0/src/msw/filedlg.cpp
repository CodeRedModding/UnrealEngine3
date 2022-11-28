/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/filedlg.cpp
// Purpose:     wxFileDialog
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: filedlg.cpp,v 1.46.2.3 2002/10/05 20:20:57 VZ Exp $
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
    #pragma implementation "filedlg.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_FILEDLG

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/msgdlg.h"
    #include "wx/dialog.h"
    #include "wx/filedlg.h"
    #include "wx/filefn.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/app.h"
#endif

#include "wx/msw/private.h"

#if !defined(__WIN32__) || defined(__SALFORDC__) || defined(__WXWINE__)
    #include <commdlg.h>
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "wx/tokenzr.h"

#ifndef OFN_EXPLORER
    #define OFN_EXPLORER 0x00080000
#endif

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#ifdef __WIN32__
# define wxMAXPATH   65534
#else
# define wxMAXPATH   1024
#endif

# define wxMAXFILE   1024

# define wxMAXEXT    5

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

IMPLEMENT_CLASS(wxFileDialog, wxDialog)

// ----------------------------------------------------------------------------
// global functions
// ----------------------------------------------------------------------------

wxString wxFileSelector(const wxChar *title,
                        const wxChar *defaultDir,
                        const wxChar *defaultFileName,
                        const wxChar *defaultExtension,
                        const wxChar *filter,
                        int flags,
                        wxWindow *parent,
                        int x, int y)
{
    // In the original implementation, defaultExtension is passed to the
    // lpstrDefExt member of OPENFILENAME. This extension, if non-NULL, is
    // appended to the filename if the user fails to type an extension. The new
    // implementation (taken from wxFileSelectorEx) appends the extension
    // automatically, by looking at the filter specification. In fact this
    // should be better than the native Microsoft implementation because
    // Windows only allows *one* default extension, whereas here we do the
    // right thing depending on the filter the user has chosen.

    // If there's a default extension specified but no filter, we create a
    // suitable filter.

    wxString filter2;
    if ( defaultExtension && !filter )
        filter2 = wxString(wxT("*.")) + defaultExtension;
    else if ( filter )
        filter2 = filter;

    wxString defaultDirString;
    if (defaultDir)
        defaultDirString = defaultDir;

    wxString defaultFilenameString;
    if (defaultFileName)
        defaultFilenameString = defaultFileName;

    wxFileDialog fileDialog(parent, title, defaultDirString,
                            defaultFilenameString, filter2,
                            flags, wxPoint(x, y));
    if( wxStrlen(defaultExtension) != 0 )
    {
        int filterFind = 0,
            filterIndex = 0;

        for( unsigned int i = 0; i < filter2.Len(); i++ )
        {
            if( filter2.GetChar(i) == wxT('|') )
            {
                // save the start index of the new filter
                unsigned int is = i++;

                // find the end of the filter
                for( ; i < filter2.Len(); i++ )
                {
                    if(filter2[i] == wxT('|'))
                        break;
                }

                if( i-is-1 > 0 && is+1 < filter2.Len() )
                {
                    if( filter2.Mid(is+1,i-is-1).Contains(defaultExtension) )
                    {
                        filterFind = filterIndex;
                        break;
                    }
                }

                filterIndex++;
            }
        }

        fileDialog.SetFilterIndex(filterFind);
    }

    wxString filename;
    if ( fileDialog.ShowModal() == wxID_OK )
    {
        filename = fileDialog.GetPath();
    }

    return filename;
}


wxString wxFileSelectorEx(const wxChar *title,
                       const wxChar *defaultDir,
                       const wxChar *defaultFileName,
                       int* defaultFilterIndex,
                       const wxChar *filter,
                       int       flags,
                       wxWindow* parent,
                       int       x,
                       int       y)

{
    wxFileDialog fileDialog(parent,
                            title ? title : wxT(""),
                            defaultDir ? defaultDir : wxT(""),
                            defaultFileName ? defaultFileName : wxT(""),
                            filter ? filter : wxT(""),
                            flags, wxPoint(x, y));

    wxString filename;
    if ( fileDialog.ShowModal() == wxID_OK )
    {
        if ( defaultFilterIndex )
            *defaultFilterIndex = fileDialog.GetFilterIndex();

        filename = fileDialog.GetPath();
    }

    return filename;
}

wxFileDialog::wxFileDialog(wxWindow *parent,
                           const wxString& message,
                           const wxString& defaultDir,
                           const wxString& defaultFileName,
                           const wxString& wildCard,
                           long style,
                           const wxPoint& WXUNUSED(pos))
{
    m_message = message;
    m_dialogStyle = style;
    if ( ( m_dialogStyle & wxMULTIPLE ) && ( m_dialogStyle & wxSAVE ) )
        m_dialogStyle &= ~wxMULTIPLE;
    m_parent = parent;
    m_path = wxT("");
    m_fileName = defaultFileName;
    m_dir = defaultDir;
    m_wildCard = wildCard;
    m_filterIndex = 0;
}

void wxFileDialog::GetPaths(wxArrayString& paths) const
{
    paths.Empty();

    wxString dir(m_dir);
    if ( m_dir.Last() != _T('\\') )
        dir += _T('\\');

    size_t count = m_fileNames.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        paths.Add(dir + m_fileNames[n]);
    }
}

void wxFileDialog::SetPath(const wxString& path)
{
    wxString ext;
    wxSplitPath(path, &m_dir, &m_fileName, &ext);
    if ( !ext.empty() )
        m_fileName << _T('.') << ext;
}

int wxFileDialog::ShowModal()
{
    HWND hWnd = 0;
    if (m_parent) hWnd = (HWND) m_parent->GetHWND();
    if (!hWnd && wxTheApp->GetTopWindow())
        hWnd = (HWND) wxTheApp->GetTopWindow()->GetHWND();

    static wxChar fileNameBuffer [ wxMAXPATH ];           // the file-name
    wxChar        titleBuffer    [ wxMAXFILE+1+wxMAXEXT ];  // the file-name, without path

    *fileNameBuffer = wxT('\0');
    *titleBuffer    = wxT('\0');

    long msw_flags = 0;
    if ( (m_dialogStyle & wxHIDE_READONLY) || (m_dialogStyle & wxSAVE) )
        msw_flags |= OFN_HIDEREADONLY;
    if ( m_dialogStyle & wxFILE_MUST_EXIST )
        msw_flags |= OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (m_dialogStyle & wxMULTIPLE )
    {
        // OFN_EXPLORER must always be specified with OFN_ALLOWMULTISELECT
        msw_flags |= OFN_EXPLORER | OFN_ALLOWMULTISELECT;
    }

    // if wxCHANGE_DIR flag is not given we shouldn't change the CWD which the
    // standard dialog does by default
    if ( !(m_dialogStyle & wxCHANGE_DIR) )
    {
        msw_flags |= OFN_NOCHANGEDIR;
    }
/* chris elliott for some reason this does not work usefully if no extension
   is given, as it test for junk instead of junk.ext
    if ( m_dialogStyle & wxOVERWRITE_PROMPT )
    {
        msw_flags |= OFN_OVERWRITEPROMPT;
    }
*/
    OPENFILENAME of;
    wxZeroMemory(of);

    // the OPENFILENAME struct has been extended in newer version of
    // comcdlg32.dll, but as we don't use the extended fields anyhow, set
    // the struct size to the old value - otherwise, the programs compiled
    // with new headers will not work with the old libraries
#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0500)
    of.lStructSize       = sizeof(OPENFILENAME) -
                           (sizeof(void *) + 2*sizeof(DWORD));
#else // old headers
    of.lStructSize       = sizeof(OPENFILENAME);
#endif

    of.hwndOwner         = hWnd;
    of.lpstrTitle        = WXSTRINGCAST m_message;
    of.lpstrFileTitle    = titleBuffer;
    of.nMaxFileTitle     = wxMAXFILE + 1 + wxMAXEXT;    // Windows 3.0 and 3.1

    // Convert forward slashes to backslashes (file selector doesn't like
    // forward slashes) and also squeeze multiple consecutive slashes into one
    // as it doesn't like two backslashes in a row neither

    wxString 	dir;
    size_t 	i, len = m_dir.length();
    dir.reserve(len);
    for ( i = 0; i < len; i++ )
    {
        wxChar ch = m_dir[i];
        switch ( ch )
        {
            case _T('/'):
                // convert to backslash
                ch = _T('\\');

                // fall through

            case _T('\\'):
                while ( i < len - 1 )
                {
                    wxChar chNext = m_dir[i + 1];
                    if ( chNext != _T('\\') && chNext != _T('/') )
                        break;

                    // ignore the next one, unless it is at the start of a UNC path
                    if (i > 0)
                        i++;
                    else
                        break;    
                }
                // fall through

            default:
                // normal char
                dir += ch;
        }
    }

    of.lpstrInitialDir   = dir.c_str();

    of.Flags             = msw_flags;


    //=== Like Alejandro Sierra's wildcard modification >>===================
    /*
       In wxFileSelector you can put, instead of a single wild_card,
       pairs of strings separated by '|'.
       The first string is a description, and the
       second is the wild card. You can put any number of pairs.

       eg.  "description1 (*.ex1)|*.ex1|description2 (*.ex2)|*.ex2"

       If you put a single wild card, it works as before the modification.
     */
    //=======================================================================

    wxString theFilter;
    if ( wxStrlen(m_wildCard) == 0 )
        theFilter = wxString(wxT("*.*"));
    else
        theFilter = m_wildCard ;
    wxString filterBuffer;

    if ( !wxStrchr( theFilter, wxT('|') ) ) {    // only one filter ==> default text
        filterBuffer.Printf(_("Files (%s)|%s"),
                            theFilter.c_str(), theFilter.c_str());
    }
    else {                                // more then one filter
        filterBuffer = theFilter;

    }

    filterBuffer += wxT("|");
    // Replace | with \0
    for (i = 0; i < filterBuffer.Len(); i++ ) {
        if ( filterBuffer.GetChar(i) == wxT('|') ) {
            filterBuffer[i] = wxT('\0');
        }
    }

    of.lpstrFilter  = (LPTSTR)(const wxChar *)filterBuffer;
    of.nFilterIndex = m_filterIndex + 1;

    //=== Setting defaultFileName >>=========================================

    wxStrncpy( fileNameBuffer, (const wxChar *)m_fileName, wxMAXPATH-1 );
    fileNameBuffer[ wxMAXPATH-1 ] = wxT('\0');

    of.lpstrFile = fileNameBuffer;  // holds returned filename
    of.nMaxFile  = wxMAXPATH;

    //== Execute FileDialog >>=================================================

    bool success = (m_dialogStyle & wxSAVE ? GetSaveFileName(&of)
                                           : GetOpenFileName(&of)) != 0;

    DWORD errCode = CommDlgExtendedError();

#ifdef __WIN32__
    if (!success && (errCode == CDERR_STRUCTSIZE))
    {
        // The struct size has changed so try a smaller or bigger size

        int oldStructSize = of.lStructSize;
        of.lStructSize       = oldStructSize - (sizeof(void *) + 2*sizeof(DWORD));
        success = (m_dialogStyle & wxSAVE) ? (GetSaveFileName(&of) != 0)
                                            : (GetOpenFileName(&of) != 0);
        errCode = CommDlgExtendedError();

        if (!success && (errCode == CDERR_STRUCTSIZE))
        {
            of.lStructSize       = oldStructSize + (sizeof(void *) + 2*sizeof(DWORD));
            success = (m_dialogStyle & wxSAVE) ? (GetSaveFileName(&of) != 0)
                                            : (GetOpenFileName(&of) != 0);
        }
    }
#endif // __WIN32__

    if ( success )
    {
        m_fileNames.Empty();

        if ( ( m_dialogStyle & wxMULTIPLE ) &&
#if defined(OFN_EXPLORER)
             ( fileNameBuffer[of.nFileOffset-1] == wxT('\0') ) )
#else
             ( fileNameBuffer[of.nFileOffset-1] == wxT(' ') ) )
#endif // OFN_EXPLORER
        {
#if defined(OFN_EXPLORER)
            m_dir = fileNameBuffer;
            i = of.nFileOffset;
            m_fileName = &fileNameBuffer[i];
            m_fileNames.Add(m_fileName);
            i += m_fileName.Len() + 1;

            while (fileNameBuffer[i] != wxT('\0'))
            {
                m_fileNames.Add(&fileNameBuffer[i]);
                i += wxStrlen(&fileNameBuffer[i]) + 1;
            }
#else
            wxStringTokenizer toke(fileNameBuffer, _T(" \t\r\n"));
            m_dir = toke.GetNextToken();
            m_fileName = toke.GetNextToken();
            m_fileNames.Add(m_fileName);

            while (toke.HasMoreTokens())
                m_fileNames.Add(toke.GetNextToken());
#endif // OFN_EXPLORER

            wxString dir(m_dir);
            if ( m_dir.Last() != _T('\\') )
                dir += _T('\\');

            m_fileNames.Sort();
            m_path = dir + m_fileName;
        }
        else
        {
            const wxChar* extension = NULL;

            //=== Adding the correct extension >>=================================

            m_filterIndex = (int)of.nFilterIndex - 1;

            if ( !of.nFileExtension || 
                 (of.nFileExtension && fileNameBuffer[of.nFileExtension] == wxT('\0')) )
            {
                // User has typed a filename without an extension:

                // A filename can end in a "." here ("abc."), this means it
                // does not have an extension. Because later on a "." with
                // the default extension is appended we remove the "." if
                // filename ends with one (We don't want files called
                // "abc..ext")
                int idx = wxStrlen(fileNameBuffer) - 1;
                if ( fileNameBuffer[idx] == wxT('.') )
                {
                    fileNameBuffer[idx] = wxT('\0');
                }

                int   maxFilter = (int)(of.nFilterIndex*2L-1L);
                extension = filterBuffer;

                for( int i = 0; i < maxFilter; i++ ) {          // get extension
                    extension = extension + wxStrlen( extension ) +1;
                }

                extension = wxStrrchr( extension, wxT('.') );
                if (  extension                                 // != "blabla"
                        && !wxStrrchr( extension, wxT('*') )       // != "blabla.*"
                        && !wxStrrchr( extension, wxT('?') )       // != "blabla.?"
                        && extension[1]                           // != "blabla."
                        && extension[1] != wxT(' ') )              // != "blabla. "
                {
                    // now concat extension to the fileName:
                    m_fileName = wxString(fileNameBuffer) + extension;

                    int len = wxStrlen( fileNameBuffer );
                    wxStrncpy( fileNameBuffer + len, extension, wxMAXPATH - len );
                    fileNameBuffer[ wxMAXPATH -1 ] = wxT('\0');
                }
            }

            m_path = fileNameBuffer;
            m_fileName = wxFileNameFromPath(fileNameBuffer);
            m_fileNames.Add(m_fileName);
            m_dir = wxPathOnly(fileNameBuffer);
        }
        //=== Simulating the wxOVERWRITE_PROMPT >>============================
        //should we also test for file save style ??
        if ( (m_dialogStyle & wxOVERWRITE_PROMPT) &&
             ::wxFileExists( fileNameBuffer ) )
             {
             wxString messageText;
             messageText.Printf(_("File '%s' already exists.\nDo you want to replace it?"), fileNameBuffer);
             if ( wxMessageBox(messageText, wxT("Save File As"), wxYES_NO | wxICON_EXCLAMATION	 ) != wxYES )
             {
                 success = FALSE;
             }            
        }
    }
    else
    {
        // common dialog failed - why?
#ifdef __WXDEBUG__
        DWORD dwErr = CommDlgExtendedError();
        if ( dwErr != 0 )
        {
            // this msg is only for developers
            wxLogError(wxT("Common dialog failed with error code %0lx."),
                       dwErr);
        }
        //else: it was just cancelled
#endif
    }

    return success ? wxID_OK : wxID_CANCEL;

}

// Generic file load/save dialog (for internal use only)
static
wxString wxDefaultFileSelector(bool load,
                               const wxChar *what,
                               const wxChar *extension,
                               const wxChar *default_name,
                               wxWindow *parent)
{
    wxString prompt;
    wxString str;
    if (load)
        str = _("Load %s file");
    else
        str = _("Save %s file");
    prompt.Printf(str, what);

    const wxChar *ext = extension;
    if (*ext == wxT('.'))
        ext++;

    wxString wild;
    wild.Printf(wxT("*.%s"), ext);

    return wxFileSelector(prompt, NULL, default_name, ext, wild,
                          load ? wxOPEN : wxSAVE, parent);
}

// Generic file load dialog
WXDLLEXPORT wxString wxLoadFileSelector(const wxChar *what,
                                        const wxChar *extension,
                                        const wxChar *default_name,
                                        wxWindow *parent)
{
    return wxDefaultFileSelector(TRUE, what, extension, default_name, parent);
}

// Generic file save dialog
WXDLLEXPORT wxString wxSaveFileSelector(const wxChar *what,
                                        const wxChar *extension,
                                        const wxChar *default_name,
                                        wxWindow *parent)
{
    return wxDefaultFileSelector(FALSE, what, extension, default_name, parent);
}

#endif // wxUSE_FILEDLG

