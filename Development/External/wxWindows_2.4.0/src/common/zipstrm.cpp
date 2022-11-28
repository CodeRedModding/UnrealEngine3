/////////////////////////////////////////////////////////////////////////////
// Name:        zipstream.cpp
// Purpose:     input stream for ZIP archive access
// Author:      Vaclav Slavik
// Copyright:   (c) 1999 Vaclav Slavik
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "zipstrm.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
  #pragma hdrstop
#endif

#if wxUSE_STREAMS && wxUSE_ZIPSTREAM && wxUSE_ZLIB

#include "wx/log.h"
#include "wx/intl.h"
#include "wx/stream.h"
#include "wx/wfstream.h"
#include "wx/zipstrm.h"
#include "wx/utils.h"

/* Not the right solution (paths in makefiles) but... */
#ifdef __BORLANDC__
#include "../common/unzip.h"
#else
#include "unzip.h"
#endif


wxZipInputStream::wxZipInputStream(const wxString& archive, const wxString& file) : wxInputStream()
{
    unz_file_info zinfo;

    m_Pos = 0;
    m_Size = 0;
    m_Archive = (void*) unzOpen(archive.mb_str());
    if (m_Archive == NULL)
    {
        m_lasterror = wxSTREAM_READ_ERROR;
        return;
    }
    if (unzLocateFile((unzFile)m_Archive, file.mb_str(), 0) != UNZ_OK)
    {
        m_lasterror = wxSTREAM_READ_ERROR;
        return;
    }

    unzGetCurrentFileInfo((unzFile)m_Archive, &zinfo, (char*) NULL, 0, (void*) NULL, 0, (char*) NULL, 0);

    if (unzOpenCurrentFile((unzFile)m_Archive) != UNZ_OK)
    {
        m_lasterror = wxSTREAM_READ_ERROR;
        return;
    }
    m_Size = (size_t)zinfo.uncompressed_size;
}



wxZipInputStream::~wxZipInputStream()
{
    if (m_Archive)
    {
        if (m_Size != 0)
            unzCloseCurrentFile((unzFile)m_Archive);
        unzClose((unzFile)m_Archive);
    }
}

bool wxZipInputStream::Eof() const
{
    wxASSERT_MSG( m_Pos <= (off_t)m_Size,
                  _T("wxZipInputStream: invalid current position") );

    return m_Pos >= (off_t)m_Size;
}


size_t wxZipInputStream::OnSysRead(void *buffer, size_t bufsize)
{
    wxASSERT_MSG( m_Pos <= (off_t)m_Size,
                  _T("wxZipInputStream: invalid current position") );

    if ( m_Pos >= (off_t)m_Size )
    {
        m_lasterror = wxSTREAM_EOF;
        return 0;
    }

    if (m_Pos + bufsize > m_Size)
        bufsize = m_Size - m_Pos;

    unzReadCurrentFile((unzFile)m_Archive, buffer, bufsize);
    m_Pos += bufsize;

    return bufsize;
}



off_t wxZipInputStream::OnSysSeek(off_t seek, wxSeekMode mode)
{
    // NB: since ZIP files don't natively support seeking, we have to 
    //     implement a brute force workaround -- reading all the data
    //     between current and the new position (or between beginning of 
    //     the file and new position...)

    off_t nextpos;

    switch ( mode )
    {
        case wxFromCurrent : nextpos = seek + m_Pos; break;
        case wxFromStart : nextpos = seek; break;
        case wxFromEnd : nextpos = m_Size - 1 + seek; break;
        default : nextpos = m_Pos; break; /* just to fool compiler, never happens */
    }

    size_t toskip = 0;
    if ( nextpos > m_Pos )
    {
        toskip = nextpos - m_Pos;
    }
    else
    {
        unzCloseCurrentFile((unzFile)m_Archive);
        if (unzOpenCurrentFile((unzFile)m_Archive) != UNZ_OK)
        {
            m_lasterror = wxSTREAM_READ_ERROR;
            return m_Pos;
        }
        toskip = nextpos;
    }
        
    if ( toskip > 0 )
    {
        const size_t BUFSIZE = 4096;
        size_t sz;
        char buffer[BUFSIZE];
        while ( toskip > 0 )
        {
            sz = wxMin(toskip, BUFSIZE);
            unzReadCurrentFile((unzFile)m_Archive, buffer, sz);
            toskip -= sz;
        }
    }

    m_Pos = nextpos;
    return m_Pos;
}

#endif
  // wxUSE_STREAMS && wxUSE_ZIPSTREAM && wxUSE_ZLIB
