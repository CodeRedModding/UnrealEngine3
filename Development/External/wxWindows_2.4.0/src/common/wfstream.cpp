/////////////////////////////////////////////////////////////////////////////
// Name:        fstream.cpp
// Purpose:     "File stream" classes
// Author:      Julian Smart
// Modified by:
// Created:     11/07/98
// RCS-ID:      $Id: wfstream.cpp,v 1.15.2.1 2002/11/04 19:31:58 VZ Exp $
// Copyright:   (c) Guilhem Lavaux
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "wfstream.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
  #pragma hdrstop
#endif

#if wxUSE_STREAMS && wxUSE_FILE

#include <stdio.h>
#include "wx/stream.h"
#include "wx/wfstream.h"

// ----------------------------------------------------------------------------
// wxFileInputStream
// ----------------------------------------------------------------------------

wxFileInputStream::wxFileInputStream(const wxString& fileName)
  : wxInputStream()
{
    m_file = new wxFile(fileName, wxFile::read);
    m_file_destroy = TRUE;
}

wxFileInputStream::wxFileInputStream()
  : wxInputStream()
{
    m_file_destroy = FALSE;
    m_file = NULL;
}

wxFileInputStream::wxFileInputStream(wxFile& file)
{
    m_file = &file;
    m_file_destroy = FALSE;
}

wxFileInputStream::wxFileInputStream(int fd)
{
    m_file = new wxFile(fd);
    m_file_destroy = TRUE;
}

wxFileInputStream::~wxFileInputStream()
{
    if (m_file_destroy)
        delete m_file;
}

size_t wxFileInputStream::GetSize() const
{
    return m_file->Length();
}

size_t wxFileInputStream::OnSysRead(void *buffer, size_t size)
{
    off_t ret = m_file->Read(buffer, size);

    switch ( ret )
    {
        case 0:
            m_lasterror = wxSTREAM_EOF;
            break;

        case wxInvalidOffset:
            m_lasterror = wxSTREAM_READ_ERROR;
            ret = 0;
            break;

        default:
            m_lasterror = wxSTREAM_NO_ERROR;
    }

    return ret;
}

off_t wxFileInputStream::OnSysSeek(off_t pos, wxSeekMode mode)
{
    return m_file->Seek(pos, mode);
}

off_t wxFileInputStream::OnSysTell() const
{
    return m_file->Tell();
}

// ----------------------------------------------------------------------------
// wxFileOutputStream
// ----------------------------------------------------------------------------

wxFileOutputStream::wxFileOutputStream(const wxString& fileName)
{
    m_file = new wxFile(fileName, wxFile::write);
    m_file_destroy = TRUE;

    if (!m_file->IsOpened())
    {
        m_lasterror = wxSTREAM_WRITE_ERROR;
    }
    else
    {
        if (m_file->Error())
            m_lasterror = wxSTREAM_WRITE_ERROR;
    }
}

wxFileOutputStream::wxFileOutputStream(wxFile& file)
{
    m_file = &file;
    m_file_destroy = FALSE;
}

wxFileOutputStream::wxFileOutputStream()
                  : wxOutputStream()
{
    m_file_destroy = FALSE;
    m_file = NULL;
}

wxFileOutputStream::wxFileOutputStream(int fd)
{
    m_file = new wxFile(fd);
    m_file_destroy = TRUE;
}

wxFileOutputStream::~wxFileOutputStream()
{
    if (m_file_destroy)
    {
        Sync();
        delete m_file;
    }
}

size_t wxFileOutputStream::OnSysWrite(const void *buffer, size_t size)
{
    size_t ret = m_file->Write(buffer, size);

    m_lasterror = m_file->Error() ? wxSTREAM_WRITE_ERROR : wxSTREAM_NO_ERROR;

    return ret;
}

off_t wxFileOutputStream::OnSysTell() const
{
    return m_file->Tell();
}

off_t wxFileOutputStream::OnSysSeek(off_t pos, wxSeekMode mode)
{
    return m_file->Seek(pos, mode);
}

void wxFileOutputStream::Sync()
{
    wxOutputStream::Sync();
    m_file->Flush();
}

size_t wxFileOutputStream::GetSize() const
{
    return m_file->Length();
}

// ----------------------------------------------------------------------------
// wxFileStream
// ----------------------------------------------------------------------------

wxFileStream::wxFileStream(const wxString& fileName)
            : wxFileInputStream(fileName)
{
    wxFileOutputStream::m_file = wxFileInputStream::m_file;
}

// ----------------------------------------------------------------------------
// wxFFileInputStream
// ----------------------------------------------------------------------------

wxFFileInputStream::wxFFileInputStream(const wxString& fileName)
  : wxInputStream()
{
    m_file = new wxFFile(fileName, "rb");
    m_file_destroy = TRUE;
}

wxFFileInputStream::wxFFileInputStream()
  : wxInputStream()
{
    m_file_destroy = FALSE;
    m_file = NULL;
}

wxFFileInputStream::wxFFileInputStream(wxFFile& file)
{
    m_file = &file;
    m_file_destroy = FALSE;
}

wxFFileInputStream::wxFFileInputStream(FILE *file)
{
    m_file = new wxFFile(file);
    m_file_destroy = TRUE;
}

wxFFileInputStream::~wxFFileInputStream()
{
    if (m_file_destroy)
        delete m_file;
}

size_t wxFFileInputStream::GetSize() const
{
    return m_file->Length();
}

size_t wxFFileInputStream::OnSysRead(void *buffer, size_t size)
{
    off_t ret;

    ret = m_file->Read(buffer, size);

    if (m_file->Eof())
        m_lasterror = wxSTREAM_EOF;
    if (ret == wxInvalidOffset)
    {
        m_lasterror = wxSTREAM_READ_ERROR;
        ret = 0;
    }

    return ret;
}

off_t wxFFileInputStream::OnSysSeek(off_t pos, wxSeekMode mode)
{
    return ( m_file->Seek(pos, mode) ? pos : wxInvalidOffset );
}

off_t wxFFileInputStream::OnSysTell() const
{
    return m_file->Tell();
}

// ----------------------------------------------------------------------------
// wxFFileOutputStream
// ----------------------------------------------------------------------------

wxFFileOutputStream::wxFFileOutputStream(const wxString& fileName)
{
    m_file = new wxFFile(fileName, "w+b");
    m_file_destroy = TRUE;

    if (!m_file->IsOpened())
    {
        m_lasterror = wxSTREAM_WRITE_ERROR;
    }
    else
    {
        if (m_file->Error())
            m_lasterror = wxSTREAM_WRITE_ERROR;
    }
}

wxFFileOutputStream::wxFFileOutputStream(wxFFile& file)
{
    m_file = &file;
    m_file_destroy = FALSE;
}

wxFFileOutputStream::wxFFileOutputStream()
  : wxOutputStream()
{
    m_file_destroy = FALSE;
    m_file = NULL;
}

wxFFileOutputStream::wxFFileOutputStream(FILE *file)
{
    m_file = new wxFFile(file);
    m_file_destroy = TRUE;
}

wxFFileOutputStream::~wxFFileOutputStream()
{
    if (m_file_destroy)
    {
        Sync();
        delete m_file;
    }
}

size_t wxFFileOutputStream::OnSysWrite(const void *buffer, size_t size)
{
    size_t ret = m_file->Write(buffer, size);
    if (m_file->Error())
        m_lasterror = wxSTREAM_WRITE_ERROR;
    else
        m_lasterror = wxSTREAM_NO_ERROR;
    return ret;
}

off_t wxFFileOutputStream::OnSysTell() const
{
    return m_file->Tell();
}

off_t wxFFileOutputStream::OnSysSeek(off_t pos, wxSeekMode mode)
{
    return ( m_file->Seek(pos, mode) ? pos : wxInvalidOffset );
}

void wxFFileOutputStream::Sync()
{
    wxOutputStream::Sync();
    m_file->Flush();
}

size_t wxFFileOutputStream::GetSize() const
{
    return m_file->Length();
}

// ----------------------------------------------------------------------------
// wxFFileStream
// ----------------------------------------------------------------------------

wxFFileStream::wxFFileStream(const wxString& fileName)
             : wxFFileInputStream(fileName)
{
    wxFFileOutputStream::m_file = wxFFileInputStream::m_file;
}

#endif
  // wxUSE_STREAMS && wxUSE_FILE

