///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/dircmn.cpp
// Purpose:     wxDir methods common to all implementations
// Author:      Vadim Zeitlin
// Modified by:
// Created:     19.05.01
// RCS-ID:      $Id: dircmn.cpp,v 1.6 2002/07/17 16:58:05 MBN Exp $
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// License:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

/* this is done in platform-specific files
#ifdef __GNUG__
    #pragma implementation "dir.h"
#endif
*/

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/filefn.h"
#endif //WX_PRECOMP

#include "wx/dir.h"

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxDir::HasFiles() and HasSubDirs()
// ----------------------------------------------------------------------------

// dumb generic implementation

bool wxDir::HasFiles(const wxString& spec)
{
    wxString s;
    return GetFirst(&s, spec, wxDIR_FILES | wxDIR_HIDDEN);
}

// we have a (much) faster version for Unix
#if (defined(__CYGWIN__) && defined(__WINDOWS__)) || !defined(__UNIX_LIKE__) || defined(__WXMAC__)

bool wxDir::HasSubDirs(const wxString& spec)
{
    wxString s;
    return GetFirst(&s, spec, wxDIR_DIRS | wxDIR_HIDDEN);
}

#endif // !Unix

// ----------------------------------------------------------------------------
// wxDir::Traverse()
// ----------------------------------------------------------------------------

size_t wxDir::Traverse(wxDirTraverser& sink,
                       const wxString& filespec,
                       int flags) const
{
    wxCHECK_MSG( IsOpened(), (size_t)-1,
                 _T("dir must be opened before traversing it") );

    // the total number of files found
    size_t nFiles = 0;

    // the name of this dir with path delimiter at the end
    wxString prefix = GetName();
    prefix += wxFILE_SEP_PATH;

    // first, recurse into subdirs
    if ( flags & wxDIR_DIRS )
    {
        wxString dirname;
        bool cont = GetFirst(&dirname, _T(""), wxDIR_DIRS | wxDIR_HIDDEN);
        while ( cont )
        {
            wxDirTraverseResult res = sink.OnDir(prefix + dirname);

            if ( res == wxDIR_STOP )
                break;

            if ( res == wxDIR_CONTINUE )
            {
                wxDir subdir(prefix + dirname);
                if ( subdir.IsOpened() )
                {
                    nFiles += subdir.Traverse(sink, filespec, flags);
                }
            }
            else
            {
                wxASSERT_MSG( res == wxDIR_IGNORE,
                              _T("unexpected OnDir() return value") );
            }

            cont = GetNext(&dirname);
        }
    }

    // now enum our own files
    if ( flags & wxDIR_FILES )
    {
        flags &= ~wxDIR_DIRS;

        wxString filename;
        bool cont = GetFirst(&filename, filespec, flags);
        while ( cont )
        {
            wxDirTraverseResult res = sink.OnFile(prefix + filename);
            if ( res == wxDIR_STOP )
                break;

            wxASSERT_MSG( res == wxDIR_CONTINUE,
                          _T("unexpected OnFile() return value") );

            nFiles++;

            cont = GetNext(&filename);
        }
    }

    return nFiles;
}

// ----------------------------------------------------------------------------
// wxDir::GetAllFiles()
// ----------------------------------------------------------------------------

class wxDirTraverserSimple : public wxDirTraverser
{
public:
    wxDirTraverserSimple(wxArrayString& files) : m_files(files) { }

    virtual wxDirTraverseResult OnFile(const wxString& filename)
    {
        m_files.Add(filename);
        return wxDIR_CONTINUE;
    }

    virtual wxDirTraverseResult OnDir(const wxString& WXUNUSED(dirname))
    {
        return wxDIR_CONTINUE;
    }

private:
    wxArrayString& m_files;
};

/* static */
size_t wxDir::GetAllFiles(const wxString& dirname,
                          wxArrayString *files,
                          const wxString& filespec,
                          int flags)
{
    wxCHECK_MSG( files, (size_t)-1, _T("NULL pointer in wxDir::GetAllFiles") );

    size_t nFiles = 0;

    wxDir dir(dirname);
    if ( dir.IsOpened() )
    {
        wxDirTraverserSimple traverser(*files);

        nFiles += dir.Traverse(traverser, filespec, flags);
    }

    return nFiles;
}

