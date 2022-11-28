///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/ole/dataobj2.h
// Purpose:     second part of platform specific wxDataObject header -
//              declarations of predefined wxDataObjectSimple-derived classes
// Author:      Vadim Zeitlin
// Modified by:
// Created:     21.10.99
// RCS-ID:      $Id: dataobj2.h,v 1.4 2001/10/26 02:11:22 RD Exp $
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_OLE_DATAOBJ2_H
#define _WX_MSW_OLE_DATAOBJ2_H

// ----------------------------------------------------------------------------
// wxBitmapDataObject is a specialization of wxDataObject for bitmap data
//
// NB: in fact, under MSW we support CF_DIB (and not CF_BITMAP) clipboard
//     format and we also provide wxBitmapDataObject2 for CF_BITMAP (which is
//     rarely used). This is ugly, but I haven't found a solution for it yet.
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxBitmapDataObject : public wxBitmapDataObjectBase
{
public:
    // ctors
    wxBitmapDataObject(const wxBitmap& bitmap = wxNullBitmap)
        : wxBitmapDataObjectBase(bitmap)
        {
            SetFormat(wxDF_DIB);

            m_data = NULL;
        }

    // implement base class pure virtuals
    virtual size_t GetDataSize() const;
    virtual bool GetDataHere(void *buf) const;
    virtual bool SetData(size_t len, const void *buf);

private:
    // the DIB data
    void /* BITMAPINFO */ *m_data;
};

// ----------------------------------------------------------------------------
// wxBitmapDataObject2 - a data object for CF_BITMAP
//
// FIXME did I already mention it was ugly?
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxBitmapDataObject2 : public wxBitmapDataObjectBase
{
public:
    // ctors
    wxBitmapDataObject2(const wxBitmap& bitmap = wxNullBitmap)
        : wxBitmapDataObjectBase(bitmap)
        {
        }

    // implement base class pure virtuals
    virtual size_t GetDataSize() const;
    virtual bool GetDataHere(void *buf) const;
    virtual bool SetData(size_t len, const void *buf);
};

// ----------------------------------------------------------------------------
// wxFileDataObject - data object for CF_HDROP
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxFileDataObject : public wxFileDataObjectBase
{
public:
    // implement base class pure virtuals
    virtual bool SetData(size_t len, const void *buf);
    virtual size_t GetDataSize() const;
    virtual bool GetDataHere(void *pData) const;
    virtual void AddFile(const wxString& file);
};

// ----------------------------------------------------------------------------
// wxURLDataObject: data object for URLs
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxURLDataObject : public wxDataObjectComposite
{
public:
    wxURLDataObject();

    // return the URL as string
    wxString GetURL() const;

    // Set a string as the URL in the data object
    void SetURL(const wxString& url);

    // override to set m_textFormat
    virtual bool SetData(const wxDataFormat& format,
                         size_t len,
                         const void *buf);

private:
    // last data object we got data in
    wxDataObjectSimple *m_dataObjectLast;
};

#endif // _WX_MSW_OLE_DATAOBJ2_H
