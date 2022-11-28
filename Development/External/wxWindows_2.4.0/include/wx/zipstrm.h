/////////////////////////////////////////////////////////////////////////////
// Name:        zipstream.h
// Purpose:     wxZipInputStream for reading files from ZIP archive
// Author:      Vaclav Slavik
// Copyright:   (c) 1999 Vaclav Slavik
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __ZIPSTREAM_H__
#define __ZIPSTREAM_H__

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "zipstrm.h"
#endif

#include "wx/defs.h"

#if wxUSE_STREAMS && wxUSE_ZIPSTREAM && wxUSE_ZLIB 

#include "wx/stream.h"

//--------------------------------------------------------------------------------
// wxZipInputStream
//                  This class is input stream from ZIP archive. The archive
//                  must be local file (accessible via FILE*)
//--------------------------------------------------------------------------------


class WXDLLEXPORT wxZipInputStream : public wxInputStream
{
public:
    wxZipInputStream(const wxString& archive, const wxString& file);
            // archive is name of .zip archive, file is name of file to be extracted.
            // Remember that archive must be local file accesible via fopen, fread functions!
    ~wxZipInputStream();

    virtual size_t GetSize() const {return m_Size;}
    virtual bool Eof() const;

protected:
    virtual size_t OnSysRead(void *buffer, size_t bufsize);
    virtual off_t OnSysSeek(off_t seek, wxSeekMode mode);
    virtual off_t OnSysTell() const {return m_Pos;}

private:
    size_t m_Size;
    off_t m_Pos;

    // this void* is handle of archive . I'm sorry it is void and not proper
    // type but I don't want to make unzip.h header public.
    void *m_Archive;
};


#endif 
   // wxUSE_STREAMS && wxUSE_ZIPSTREAM && wxUSE_ZLIB 

#endif 
   // __ZIPSTREAM_H__
