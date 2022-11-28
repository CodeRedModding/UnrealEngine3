///////////////////////////////////////////////////////////////////////////////
// Name:        common/dobjcmn.cpp
// Purpose:     implementation of data object methods common to all platforms
// Author:      Vadim Zeitlin, Robert Roebling
// Modified by:
// Created:     19.10.99
// RCS-ID:      $Id: dobjcmn.cpp,v 1.20 2002/08/15 20:47:04 RR Exp $
// Copyright:   (c) wxWindows Team
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "dataobjbase.h"
#endif

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_DATAOBJ

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/debug.h"
#endif // WX_PRECOMP

#include "wx/dataobj.h"

// ----------------------------------------------------------------------------
// lists
// ----------------------------------------------------------------------------

#include "wx/listimpl.cpp"

WX_DEFINE_LIST(wxSimpleDataObjectList);

// ----------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------

static wxDataFormat dataFormatInvalid;
WXDLLEXPORT const wxDataFormat& wxFormatInvalid = dataFormatInvalid;

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxDataObjectBase
// ----------------------------------------------------------------------------

wxDataObjectBase::~wxDataObjectBase()
{
}

bool wxDataObjectBase::IsSupported(const wxDataFormat& format,
                                   Direction dir) const
{
    size_t nFormatCount = GetFormatCount(dir);
    if ( nFormatCount == 1 )
    {
        return format == GetPreferredFormat(dir);
    }
    else
    {
        wxDataFormat *formats = new wxDataFormat[nFormatCount];
        GetAllFormats(formats, dir);

        size_t n;
        for ( n = 0; n < nFormatCount; n++ )
        {
            if ( formats[n] == format )
                break;
        }

        delete [] formats;

        // found?
        return n < nFormatCount;
    }
}

// ----------------------------------------------------------------------------
// wxDataObjectComposite
// ----------------------------------------------------------------------------

wxDataObjectComposite::wxDataObjectComposite()
{
    m_preferred = 0;

    m_dataObjects.DeleteContents(TRUE);
}

wxDataObjectSimple *
wxDataObjectComposite::GetObject(const wxDataFormat& format) const
{
    wxSimpleDataObjectList::Node *node = m_dataObjects.GetFirst();
    while ( node )
    {
        wxDataObjectSimple *dataObj = node->GetData();

        if ( dataObj->GetFormat() == format )
        {
            return dataObj;
        }

        node = node->GetNext();
    }

    return (wxDataObjectSimple *)NULL;
}

void wxDataObjectComposite::Add(wxDataObjectSimple *dataObject, bool preferred)
{
    if ( preferred )
        m_preferred = m_dataObjects.GetCount();

    m_dataObjects.Append( dataObject );
}

wxDataFormat
wxDataObjectComposite::GetPreferredFormat(Direction WXUNUSED(dir)) const
{
    wxSimpleDataObjectList::Node *node = m_dataObjects.Item( m_preferred );

    wxCHECK_MSG( node, wxFormatInvalid, wxT("no preferred format") );

    wxDataObjectSimple* dataObj = node->GetData();

    return dataObj->GetFormat();
}

#if defined(__WXMSW__)

size_t wxDataObjectComposite::GetBufferOffset( const wxDataFormat& format )
{
    wxDataObjectSimple *dataObj = GetObject(format);

    wxCHECK_MSG( dataObj, 0,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->GetBufferOffset( format );
}

const void* wxDataObjectComposite::GetSizeFromBuffer( const void* buffer,
                                                      size_t* size,
                                                      const wxDataFormat& format )
{
    wxDataObjectSimple *dataObj = GetObject(format);

    wxCHECK_MSG( dataObj, NULL,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->GetSizeFromBuffer( buffer, size, format );
}

void* wxDataObjectComposite::SetSizeInBuffer( void* buffer, size_t size,
                                              const wxDataFormat& format )
{
    wxDataObjectSimple *dataObj = GetObject(format);

    wxCHECK_MSG( dataObj, NULL,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->SetSizeInBuffer( buffer, size, format );
}

#endif

size_t wxDataObjectComposite::GetFormatCount(Direction WXUNUSED(dir)) const
{
    // TODO what about the Get/Set only formats?
    return m_dataObjects.GetCount();
}

void wxDataObjectComposite::GetAllFormats(wxDataFormat *formats,
                                          Direction WXUNUSED(dir)) const
{
    size_t n = 0;
    wxSimpleDataObjectList::Node *node;
    for ( node = m_dataObjects.GetFirst(); node; node = node->GetNext() )
    {
        // TODO if ( !outputOnlyToo ) && this one counts ...
        formats[n++] = node->GetData()->GetFormat();
    }
}

size_t wxDataObjectComposite::GetDataSize(const wxDataFormat& format) const
{
    wxDataObjectSimple *dataObj = GetObject(format);

    wxCHECK_MSG( dataObj, 0,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->GetDataSize();
}

bool wxDataObjectComposite::GetDataHere(const wxDataFormat& format,
                                        void *buf) const
{
    wxDataObjectSimple *dataObj = GetObject(format);

    wxCHECK_MSG( dataObj, FALSE,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->GetDataHere(buf);
}

bool wxDataObjectComposite::SetData(const wxDataFormat& format,
                                    size_t len,
                                    const void *buf)
{
    wxDataObjectSimple *dataObj = GetObject(format);

    wxCHECK_MSG( dataObj, FALSE,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->SetData(len, buf);
}

// ----------------------------------------------------------------------------
// wxTextDataObject
// ----------------------------------------------------------------------------

size_t wxTextDataObject::GetDataSize() const
{
#if defined(__WXGTK20__) && wxUSE_UNICODE
    // Use UTF8 not UCS4
    wxCharBuffer buffer = wxConvUTF8.cWX2MB( GetText().c_str() );
    return strlen( (const char*) buffer ) + 1;
#else
    return GetTextLength() * sizeof(wxChar);
#endif
}

bool wxTextDataObject::GetDataHere(void *buf) const
{
#if defined(__WXGTK20__) && wxUSE_UNICODE
    // Use UTF8 not UCS4
    wxCharBuffer buffer = wxConvUTF8.cWX2MB( GetText().c_str() );
    strcpy( (char*) buf, (const char*) buffer );
#else
    wxStrcpy((wxChar *)buf, GetText().c_str());
#endif

    return TRUE;
}

bool wxTextDataObject::SetData(size_t WXUNUSED(len), const void *buf)
{
#if defined(__WXGTK20__) && wxUSE_UNICODE
    // Use UTF8 not UCS4
    SetText( wxConvUTF8.cMB2WX( (const char*) buf ) );
#else
    SetText(wxString((const wxChar *)buf));
#endif

    return TRUE;
}

// ----------------------------------------------------------------------------
// wxFileDataObjectBase
// ----------------------------------------------------------------------------

// VZ: I don't need this in MSW finally, so if it is needed in wxGTK, it should
//     be moved to gtk/dataobj.cpp
#if 0

wxString wxFileDataObjectBase::GetFilenames() const
{
    wxString str;
    size_t count = m_filenames.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        str << m_filenames[n] << wxT('\0');
    }

    return str;
}

void wxFileDataObjectBase::SetFilenames(const wxChar* filenames)
{
    m_filenames.Empty();

    wxString current;
    for ( const wxChar *pc = filenames; ; pc++ )
    {
        if ( *pc )
        {
            current += *pc;
        }
        else
        {
            if ( !current )
            {
                // 2 consecutive NULs - this is the end of the string
                break;
            }

            m_filenames.Add(current);
            current.Empty();
        }
    }
}

#endif // 0

// ----------------------------------------------------------------------------
// wxCustomDataObject
// ----------------------------------------------------------------------------

wxCustomDataObject::wxCustomDataObject(const wxDataFormat& format)
    : wxDataObjectSimple(format)
{
    m_data = (void *)NULL;
}

wxCustomDataObject::~wxCustomDataObject()
{
    Free();
}

void wxCustomDataObject::TakeData(size_t size, void *data)
{
    Free();

    m_size = size;
    m_data = data;
}

void *wxCustomDataObject::Alloc(size_t size)
{
    return (void *)new char[size];
}

void wxCustomDataObject::Free()
{
    delete [] (char *)m_data;
    m_size = 0;
    m_data = (void *)NULL;
}

size_t wxCustomDataObject::GetDataSize() const
{
    return GetSize();
}

bool wxCustomDataObject::GetDataHere(void *buf) const
{
    void *data = GetData();
    if ( !data )
        return FALSE;

    memcpy(buf, data, GetSize());

    return TRUE;
}

bool wxCustomDataObject::SetData(size_t size, const void *buf)
{
    Free();

    m_data = Alloc(size);
    if ( !m_data )
        return FALSE;

    memcpy(m_data, buf, m_size = size);

    return TRUE;
}

// ============================================================================
// some common dnd related code
// ============================================================================

#if wxUSE_DRAG_AND_DROP

#include "wx/dnd.h"

// ----------------------------------------------------------------------------
// wxTextDropTarget
// ----------------------------------------------------------------------------

// NB: we can't use "new" in ctor initializer lists because this provokes an
//     internal compiler error with VC++ 5.0 (hey, even gcc compiles this!),
//     so use SetDataObject() instead

wxTextDropTarget::wxTextDropTarget()
{
    SetDataObject(new wxTextDataObject);
}

wxDragResult wxTextDropTarget::OnData(wxCoord x, wxCoord y, wxDragResult def)
{
    if ( !GetData() )
        return wxDragNone;

    wxTextDataObject *dobj = (wxTextDataObject *)m_dataObject;
    return OnDropText(x, y, dobj->GetText()) ? def : wxDragNone;
}

// ----------------------------------------------------------------------------
// wxFileDropTarget
// ----------------------------------------------------------------------------

wxFileDropTarget::wxFileDropTarget()
{
    SetDataObject(new wxFileDataObject);
}

wxDragResult wxFileDropTarget::OnData(wxCoord x, wxCoord y, wxDragResult def)
{
    if ( !GetData() )
        return wxDragNone;

    wxFileDataObject *dobj = (wxFileDataObject *)m_dataObject;
    return OnDropFiles(x, y, dobj->GetFilenames()) ? def : wxDragNone;
}

#endif // wxUSE_DRAG_AND_DROP

#endif // wxUSE_DATAOBJ
