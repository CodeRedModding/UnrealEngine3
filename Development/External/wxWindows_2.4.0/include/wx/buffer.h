///////////////////////////////////////////////////////////////////////////////
// Name:        wx/buffer.h
// Purpose:     auto buffer classes: buffers which automatically free memory
// Author:      Vadim Zeitlin
// Modified by:
// Created:     12.04.99
// RCS-ID:      $Id: buffer.h,v 1.23.2.4 2002/11/30 22:22:25 VZ Exp $
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

// these classes are for private use only for now, they're not documented

#ifndef _WX_BUFFER_H
#define _WX_BUFFER_H

#include "wx/wxchar.h"

#include <string.h> // strdup

// ----------------------------------------------------------------------------
// Special classes for (wide) character strings: they use malloc/free instead
// of new/delete
// ----------------------------------------------------------------------------

#define DEFINE_BUFFER(classname, chartype, strdupfunc)                      \
class classname                                                             \
{                                                                           \
public:                                                                     \
    classname(const chartype *str)                                          \
        : m_str(str ? strdupfunc(str) : NULL)                               \
    {                                                                       \
    }                                                                       \
                                                                            \
    classname(size_t len)                                                   \
        : m_str((chartype *)malloc((len + 1)*sizeof(chartype)))             \
    {                                                                       \
        m_str[len] = (chartype)0;                                           \
    }                                                                       \
                                                                            \
    /* no need to check for NULL, free() does it */                         \
    ~classname() { free(m_str); }                                           \
                                                                            \
    /*                                                                      \
        WARNING:                                                            \
                                                                            \
        the copy ctor and assignment operators change the passed in object  \
        even although it is declared as "const", so:                        \
                                                                            \
        a) it shouldn't be really const                                     \
        b) you shouldn't use it afterwards (or know that it was reset)      \
                                                                            \
        This is very ugly but is unfortunately needed to make the normal use\
        of classname buffer objects possible and is very similar to what    \
        std::auto_ptr<> does (as if it were an excuse...)                   \
    */                                                                      \
                                                                            \
    /*                                                                      \
       because of the remark above, release() is declared const even if it  \
       isn't really const                                                   \
     */                                                                     \
    chartype *release() const                                               \
    {                                                                       \
        chartype *p = m_str;                                                \
        ((classname *)this)->m_str = NULL;                                  \
        return p;                                                           \
    }                                                                       \
                                                                            \
    classname(const classname& src)                                         \
        : m_str(src.release())                                              \
    {                                                                       \
    }                                                                       \
                                                                            \
    classname& operator=(const chartype *str)                               \
    {                                                                       \
        free(m_str);                                                        \
        m_str = str ? strdupfunc(str) : NULL;                               \
        return *this;                                                       \
    }                                                                       \
                                                                            \
    classname& operator=(const classname& src)                              \
    {                                                                       \
        free(m_str);                                                        \
        m_str = src.release();                                              \
                                                                            \
        return *this;                                                       \
    }                                                                       \
                                                                            \
    chartype *data() { return m_str; }                                      \
    const chartype *data() const { return m_str; }                          \
    operator const chartype *() const { return m_str; }                     \
    chartype operator[](size_t n) const { return m_str[n]; }                \
                                                                            \
private:                                                                    \
    chartype *m_str;                                                        \
}

DEFINE_BUFFER(wxCharBuffer, char, strdup);

#if wxUSE_WCHAR_T

inline wchar_t *wxWcsdupReplacement(const wchar_t *wcs)
{
    const size_t siz = (wxWcslen(wcs) + 1)*sizeof(wchar_t);
    wchar_t *wcsCopy = (wchar_t *)malloc(siz);
    memcpy(wcsCopy, wcs, siz);
    return wcsCopy;
}

DEFINE_BUFFER(wxWCharBuffer, wchar_t, wxWcsdupReplacement);

#endif // wxUSE_WCHAR_T

#undef DEFINE_BUFFER

#if wxUSE_UNICODE
    #define wxMB2WXbuf wxWCharBuffer
    #define wxWX2MBbuf wxCharBuffer
    #define wxWC2WXbuf wxChar*
    #define wxWX2WCbuf wxChar*
#else // ANSI
    #define wxMB2WXbuf wxChar*
    #define wxWX2MBbuf wxChar*
    #define wxWC2WXbuf wxCharBuffer
    #define wxWX2WCbuf wxWCharBuffer
#endif // Unicode/ANSI

// ----------------------------------------------------------------------------
// A class for holding growable data buffers (not necessarily strings)
// ----------------------------------------------------------------------------

// This class manages the actual data buffer pointer and is ref-counted.
class wxMemoryBufferData
{
public:
    // the initial size and also the size added by ResizeIfNeeded()
    enum { BLOCK_SIZE = 1024 };

    friend class wxMemoryBuffer;

    // everyting is private as it can only be used by wxMemoryBuffer
private:
    wxMemoryBufferData(size_t size = wxMemoryBufferData::BLOCK_SIZE)
        : m_data(size ? malloc(size) : NULL), m_size(size), m_len(0), m_ref(0)
    {
    }
    ~wxMemoryBufferData() { free(m_data); }


    void ResizeIfNeeded(size_t newSize)
    {
        if (newSize > m_size)
        {
            void *dataOld = m_data;
            m_data = realloc(m_data, newSize + wxMemoryBufferData::BLOCK_SIZE);
            if ( !m_data )
            {
                free(dataOld);
            }

            m_size = newSize + wxMemoryBufferData::BLOCK_SIZE;
        }
    }

    void IncRef() { m_ref += 1; }
    void DecRef()
    {
        m_ref -= 1;
        if (m_ref == 0)  // are there no more references?
            delete this;
    }


    // the buffer containing the data
    void  *m_data;

    // the size of the buffer
    size_t m_size;

    // the amount of data currently in the buffer
    size_t m_len;

    // the reference count
    size_t m_ref;
};


class wxMemoryBuffer
{
public:
    // ctor and dtor
    wxMemoryBuffer(size_t size = wxMemoryBufferData::BLOCK_SIZE)
    {
        m_bufdata = new wxMemoryBufferData(size);
        m_bufdata->IncRef();
    }

    ~wxMemoryBuffer() { m_bufdata->DecRef(); }


    // copy and assignment
    wxMemoryBuffer(const wxMemoryBuffer& src)
        : m_bufdata(src.m_bufdata)
    {
        m_bufdata->IncRef();
    }

    wxMemoryBuffer& operator=(const wxMemoryBuffer& src)
    {
        m_bufdata->DecRef();
        m_bufdata = src.m_bufdata;
        m_bufdata->IncRef();
        return *this;
    }


    // Accessors
    void  *GetData() const    { return m_bufdata->m_data; }
    size_t GetBufSize() const { return m_bufdata->m_size; }
    size_t GetDataLen() const { return m_bufdata->m_len; }

    void   SetBufSize(size_t size) { m_bufdata->ResizeIfNeeded(size); }
    void   SetDataLen(size_t len)
    {
        wxASSERT(len <= m_bufdata->m_size);
        m_bufdata->m_len = len;
    }

    // Ensure the buffer is big enough and return a pointer to it
    void *GetWriteBuf(size_t sizeNeeded)
    {
        m_bufdata->ResizeIfNeeded(sizeNeeded);
        return m_bufdata->m_data;
    }

    // Update the length after the write
    void  UngetWriteBuf(size_t sizeUsed) { SetDataLen(sizeUsed); }

    // Like the above, but appends to the buffer
    void *GetAppendBuf(size_t sizeNeeded)
    {
        m_bufdata->ResizeIfNeeded(m_bufdata->m_len + sizeNeeded);
        return (char*)m_bufdata->m_data + m_bufdata->m_len;
    }

    // Update the length after the append
    void  UngetAppendBuf(size_t sizeUsed)
    {
        SetDataLen(m_bufdata->m_len + sizeUsed);
    }

    // Other ways to append to the buffer
    void  AppendByte(char data)
    {
        wxCHECK_RET( m_bufdata->m_data, _T("invalid wxMemoryBuffer") );

        m_bufdata->ResizeIfNeeded(m_bufdata->m_len + 1);
        *(((char*)m_bufdata->m_data) + m_bufdata->m_len) = data;
        m_bufdata->m_len += 1;
    }

    void  AppendData(void* data, size_t len)
    {
        memcpy(GetAppendBuf(len), data, len);
        UngetAppendBuf(len);
    }

    operator const char *() const { return (const char*)GetData(); }

private:
    wxMemoryBufferData*  m_bufdata;
};

// ----------------------------------------------------------------------------
// template class for any kind of data
// ----------------------------------------------------------------------------

// TODO

#endif // _WX_BUFFER_H
