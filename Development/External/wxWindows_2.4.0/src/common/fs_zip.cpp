/////////////////////////////////////////////////////////////////////////////
// Name:        fs_zip.cpp
// Purpose:     ZIP file system
// Author:      Vaclav Slavik
// Copyright:   (c) 1999 Vaclav Slavik
// CVS-ID:      $Id: fs_zip.cpp,v 1.18.2.5 2002/12/16 00:16:05 VS Exp $
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////



#ifdef __GNUG__
#pragma implementation "fs_zip.h"
#endif

#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#if wxUSE_FILESYSTEM && wxUSE_FS_ZIP && wxUSE_ZIPSTREAM

#ifndef WXPRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
#endif

#include "wx/hash.h"
#include "wx/filesys.h"
#include "wx/zipstrm.h"
#include "wx/fs_zip.h"

/* Not the right solution (paths in makefiles) but... */
#ifdef __BORLANDC__
#include "../common/unzip.h"
#else
#include "unzip.h"
#endif


//--------------------------------------------------------------------------------
// wxZipFSHandler
//--------------------------------------------------------------------------------



wxZipFSHandler::wxZipFSHandler() : wxFileSystemHandler()
{
    m_Archive = NULL;
    m_ZipFile = m_Pattern = m_BaseDir = wxEmptyString;
    m_AllowDirs = m_AllowFiles = TRUE;
    m_DirsFound = NULL;
}



wxZipFSHandler::~wxZipFSHandler()
{
    if (m_Archive)
        unzClose((unzFile)m_Archive);
    if (m_DirsFound)
        delete m_DirsFound;
}



bool wxZipFSHandler::CanOpen(const wxString& location)
{
    wxString p = GetProtocol(location);
    return (p == wxT("zip")) &&
           (GetProtocol(GetLeftLocation(location)) == wxT("file"));
}




wxFSFile* wxZipFSHandler::OpenFile(wxFileSystem& WXUNUSED(fs), const wxString& location)
{
    wxString right = GetRightLocation(location);
    wxString left = GetLeftLocation(location);
    wxInputStream *s;

    if (GetProtocol(left) != wxT("file"))
    {
        wxLogError(_("ZIP handler currently supports only local files!"));
        return NULL;
    }

    if (right.GetChar(0) == wxT('/')) right = right.Mid(1);

    wxFileName leftFilename = wxFileSystem::URLToFileName(left);

    s = new wxZipInputStream(leftFilename.GetFullPath(), right);
    if (s && s->IsOk() )
    {
        return new wxFSFile(s,
                            left + wxT("#zip:") + right,
                            GetMimeTypeFromExt(location),
                            GetAnchor(location),
                            wxDateTime(wxFileModificationTime(left)));
    }

    delete s;
    return NULL;
}



wxString wxZipFSHandler::FindFirst(const wxString& spec, int flags)
{
    wxString right = GetRightLocation(spec);
    wxString left = GetLeftLocation(spec);

    if (right.Last() == wxT('/')) right.RemoveLast();

    if (m_Archive)
    {
        unzClose((unzFile)m_Archive);
        m_Archive = NULL;
    }

    if (GetProtocol(left) != wxT("file"))
    {
        wxLogError(_("ZIP handler currently supports only local files!"));
        return wxEmptyString;
    }

    switch (flags)
    {
        case wxFILE:
            m_AllowDirs = FALSE, m_AllowFiles = TRUE; break;
        case wxDIR:
            m_AllowDirs = TRUE, m_AllowFiles = FALSE; break;
        default:
            m_AllowDirs = m_AllowFiles = TRUE; break;
    }

    m_ZipFile = left;
    wxString nativename = wxFileSystem::URLToFileName(m_ZipFile).GetFullPath();
    m_Archive = (void*) unzOpen(nativename.mb_str());
    m_Pattern = right.AfterLast(wxT('/'));
    m_BaseDir = right.BeforeLast(wxT('/'));

    if (m_Archive)
    {
        if (unzGoToFirstFile((unzFile)m_Archive) != UNZ_OK)
        {
            unzClose((unzFile)m_Archive);
            m_Archive = NULL;
        }
        else
        {
            if (m_AllowDirs)
            {
                delete m_DirsFound;
                m_DirsFound = new wxHashTableLong();
            }
            return DoFind();
        }
    }
    return wxEmptyString;
}



wxString wxZipFSHandler::FindNext()
{
    if (!m_Archive) return wxEmptyString;
    return DoFind();
}



wxString wxZipFSHandler::DoFind()
{
    static char namebuf[1024]; // char, not wxChar!
    char *c;
    wxString namestr, dir, filename;
    wxString match = wxEmptyString;

    while (match == wxEmptyString)
    {
        unzGetCurrentFileInfo((unzFile)m_Archive, NULL, namebuf, 1024, NULL, 0, NULL, 0);
        for (c = namebuf; *c; c++) if (*c == '\\') *c = '/';
        namestr = wxString::FromAscii( namebuf );    // TODO what encoding does ZIP use?

        if (m_AllowDirs)
        {
            dir = namestr.BeforeLast(wxT('/'));
            while (!dir.IsEmpty())
            {
                long key = 0;
                for (size_t i = 0; i < dir.Length(); i++) key += (wxUChar)dir[i];
                if (m_DirsFound->Get(key) == wxNOT_FOUND)
                {
                    m_DirsFound->Put(key, 1);
                    filename = dir.AfterLast(wxT('/'));
                    dir = dir.BeforeLast(wxT('/'));
                    if (!filename.IsEmpty() && m_BaseDir == dir &&
                                wxMatchWild(m_Pattern, filename, FALSE))
                        match = m_ZipFile + wxT("#zip:") + dir + wxT("/") + filename;
                }
                else
                    break; // already tranversed
            }
        }

        filename = namestr.AfterLast(wxT('/'));
        dir = namestr.BeforeLast(wxT('/'));
        if (m_AllowFiles && !filename.IsEmpty() && m_BaseDir == dir &&
                            wxMatchWild(m_Pattern, filename, FALSE))
            match = m_ZipFile + wxT("#zip:") + namestr;

        if (unzGoToNextFile((unzFile)m_Archive) != UNZ_OK)
        {
            unzClose((unzFile)m_Archive);
            m_Archive = NULL;
            break;
        }
    }

    return match;
}



#endif
      //wxUSE_FILESYSTEM && wxUSE_FS_ZIP && wxUSE_ZIPSTREAM
