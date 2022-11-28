/////////////////////////////////////////////////////////////////////////////
// Name:        image.h
// Purpose:     wxImage class
// Author:      Robert Roebling
// RCS-ID:      $Id: image.h,v 1.73.2.5 2002/12/07 02:30:32 VZ Exp $
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_IMAGE_H_
#define _WX_IMAGE_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "image.h"
#endif

#include "wx/setup.h"
#include "wx/object.h"
#include "wx/string.h"
#include "wx/gdicmn.h"
#include "wx/bitmap.h"
#include "wx/hashmap.h"

#if wxUSE_STREAMS
#  include "wx/stream.h"
#endif

#if wxUSE_IMAGE

#define wxIMAGE_OPTION_FILENAME wxString(_T("FileName"))

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class WXDLLEXPORT wxImageHandler;
class WXDLLEXPORT wxImage;

//-----------------------------------------------------------------------------
// wxImageHandler
//-----------------------------------------------------------------------------

class WXDLLEXPORT wxImageHandler: public wxObject
{
public:
    wxImageHandler()
        : m_name(wxT("")), m_extension(wxT("")), m_mime(), m_type(0)
        { }

#if wxUSE_STREAMS
    virtual bool LoadFile( wxImage *image, wxInputStream& stream, bool verbose=TRUE, int index=-1 );
    virtual bool SaveFile( wxImage *image, wxOutputStream& stream, bool verbose=TRUE );

    virtual int GetImageCount( wxInputStream& stream );

    bool CanRead( wxInputStream& stream ) { return CallDoCanRead(stream); }
    bool CanRead( const wxString& name );
#endif // wxUSE_STREAMS

    void SetName(const wxString& name) { m_name = name; }
    void SetExtension(const wxString& ext) { m_extension = ext; }
    void SetType(long type) { m_type = type; }
    void SetMimeType(const wxString& type) { m_mime = type; }
    wxString GetName() const { return m_name; }
    wxString GetExtension() const { return m_extension; }
    long GetType() const { return m_type; }
    wxString GetMimeType() const { return m_mime; }

protected:
#if wxUSE_STREAMS
    virtual bool DoCanRead( wxInputStream& stream ) = 0;

    // save the stream position, call DoCanRead() and restore the position
    bool CallDoCanRead(wxInputStream& stream);
#endif // wxUSE_STREAMS

    wxString  m_name;
    wxString  m_extension;
    wxString  m_mime;
    long      m_type;

private:
    DECLARE_CLASS(wxImageHandler)
};

//-----------------------------------------------------------------------------
// wxImageHistogram
//-----------------------------------------------------------------------------

class WXDLLEXPORT wxImageHistogramEntry
{
public:
    wxImageHistogramEntry() : index(0), value(0) {}
    unsigned long index;
    unsigned long value;
};

WX_DECLARE_EXPORTED_HASH_MAP(unsigned long, wxImageHistogramEntry,
                             wxIntegerHash, wxIntegerEqual,
                             wxImageHistogram);

//-----------------------------------------------------------------------------
// wxImage
//-----------------------------------------------------------------------------

class WXDLLEXPORT wxImage: public wxObject
{
public:
    wxImage();
    wxImage( int width, int height );
    wxImage( int width, int height, unsigned char* data, bool static_data = FALSE );
    wxImage( const wxString& name, long type = wxBITMAP_TYPE_ANY, int index = -1 );
    wxImage( const wxString& name, const wxString& mimetype, int index = -1 );

#if wxUSE_STREAMS
    wxImage( wxInputStream& stream, long type = wxBITMAP_TYPE_ANY, int index = -1 );
    wxImage( wxInputStream& stream, const wxString& mimetype, int index = -1 );
#endif // wxUSE_STREAMS

    wxImage( const wxImage& image );
    wxImage( const wxImage* image );

#if WXWIN_COMPATIBILITY_2_2 && wxUSE_GUI
    // conversion to/from wxBitmap (deprecated, use wxBitmap's methods instead):
    wxImage( const wxBitmap &bitmap );
    wxBitmap ConvertToBitmap() const;
#ifdef __WXGTK__
    wxBitmap ConvertToMonoBitmap( unsigned char red, unsigned char green, unsigned char blue ) const;
#endif
#endif

    void Create( int width, int height );
    void Create( int width, int height, unsigned char* data, bool static_data = FALSE );
    void Destroy();

    // creates an identical copy of the image (the = operator
    // just raises the ref count)
    wxImage Copy() const;

    // return the new image with size width*height
    wxImage GetSubImage( const wxRect& ) const;

    // pastes image into this instance and takes care of
    // the mask colour and out of bounds problems
    void Paste( const wxImage &image, int x, int y );

    // return the new image with size width*height
    wxImage Scale( int width, int height ) const;

    // rescales the image in place
    wxImage& Rescale( int width, int height ) { return *this = Scale(width, height); }

    // Rotates the image about the given point, 'angle' radians.
    // Returns the rotated image, leaving this image intact.
    wxImage Rotate(double angle, const wxPoint & centre_of_rotation,
                   bool interpolating = TRUE, wxPoint * offset_after_rotation = (wxPoint*) NULL) const;

    wxImage Rotate90( bool clockwise = TRUE ) const;
    wxImage Mirror( bool horizontally = TRUE ) const;

    // replace one colour with another
    void Replace( unsigned char r1, unsigned char g1, unsigned char b1,
                  unsigned char r2, unsigned char g2, unsigned char b2 );

    // convert to monochrome image (<r,g,b> will be replaced by white, everything else by black)
    wxImage ConvertToMono( unsigned char r, unsigned char g, unsigned char b ) const;

    // these routines are slow but safe
    void SetRGB( int x, int y, unsigned char r, unsigned char g, unsigned char b );
    unsigned char GetRed( int x, int y ) const;
    unsigned char GetGreen( int x, int y ) const;
    unsigned char GetBlue( int x, int y ) const;

    // find first colour that is not used in the image and has higher
    // RGB values than <startR,startG,startB>
    bool FindFirstUnusedColour( unsigned char *r, unsigned char *g, unsigned char *b,
                                unsigned char startR = 1, unsigned char startG = 0,
                                unsigned char startB = 0 ) const;
    // Set image's mask to the area of 'mask' that has <r,g,b> colour
    bool SetMaskFromImage(const wxImage & mask,
                          unsigned char mr, unsigned char mg, unsigned char mb);

    static bool CanRead( const wxString& name );
    static int GetImageCount( const wxString& name, long type = wxBITMAP_TYPE_ANY );
    virtual bool LoadFile( const wxString& name, long type = wxBITMAP_TYPE_ANY, int index = -1 );
    virtual bool LoadFile( const wxString& name, const wxString& mimetype, int index = -1 );

#if wxUSE_STREAMS
    static bool CanRead( wxInputStream& stream );
    static int GetImageCount( wxInputStream& stream, long type = wxBITMAP_TYPE_ANY );
    virtual bool LoadFile( wxInputStream& stream, long type = wxBITMAP_TYPE_ANY, int index = -1 );
    virtual bool LoadFile( wxInputStream& stream, const wxString& mimetype, int index = -1 );
#endif

    virtual bool SaveFile( const wxString& name ) const;
    virtual bool SaveFile( const wxString& name, int type ) const;
    virtual bool SaveFile( const wxString& name, const wxString& mimetype ) const;

#if wxUSE_STREAMS
    virtual bool SaveFile( wxOutputStream& stream, int type ) const;
    virtual bool SaveFile( wxOutputStream& stream, const wxString& mimetype ) const;
#endif

    bool Ok() const;
    int GetWidth() const;
    int GetHeight() const;

    char unsigned *GetData() const;
    void SetData( char unsigned *data );
    void SetData( char unsigned *data, int new_width, int new_height );

    // Mask functions
    void SetMaskColour( unsigned char r, unsigned char g, unsigned char b );
    unsigned char GetMaskRed() const;
    unsigned char GetMaskGreen() const;
    unsigned char GetMaskBlue() const;
    void SetMask( bool mask = TRUE );
    bool HasMask() const;

#if wxUSE_PALETTE
    // Palette functions
    bool HasPalette() const;
    const wxPalette& GetPalette() const;
    void SetPalette(const wxPalette& palette);
#endif // wxUSE_PALETTE

    // Option functions (arbitrary name/value mapping)
    void SetOption(const wxString& name, const wxString& value);
    void SetOption(const wxString& name, int value);
    wxString GetOption(const wxString& name) const;
    int GetOptionInt(const wxString& name) const;
    bool HasOption(const wxString& name) const;

    unsigned long CountColours( unsigned long stopafter = (unsigned long) -1 ) const;

    // Computes the histogram of the image and fills a hash table, indexed
    // with integer keys built as 0xRRGGBB, containing wxImageHistogramEntry
    // objects. Each of them contains an 'index' (useful to build a palette 
    // with the image colours) and a 'value', which is the number of pixels 
    // in the image with that colour.
    // Returned value: # of entries in the histogram
    unsigned long ComputeHistogram( wxImageHistogram &h ) const;

    wxImage& operator = (const wxImage& image)
    {
        if ( (*this) != image )
            Ref(image);
        return *this;
    }

    bool operator == (const wxImage& image) const
        { return m_refData == image.m_refData; }
    bool operator != (const wxImage& image) const
        { return m_refData != image.m_refData; }

    static wxList& GetHandlers() { return sm_handlers; }
    static void AddHandler( wxImageHandler *handler );
    static void InsertHandler( wxImageHandler *handler );
    static bool RemoveHandler( const wxString& name );
    static wxImageHandler *FindHandler( const wxString& name );
    static wxImageHandler *FindHandler( const wxString& extension, long imageType );
    static wxImageHandler *FindHandler( long imageType );
    static wxImageHandler *FindHandlerMime( const wxString& mimetype );

    static void CleanUpHandlers();
    static void InitStandardHandlers();

protected:
    static wxList   sm_handlers;

private:
    friend class WXDLLEXPORT wxImageHandler;

    DECLARE_DYNAMIC_CLASS(wxImage)
};


extern void WXDLLEXPORT wxInitAllImageHandlers();

WXDLLEXPORT_DATA(extern wxImage)    wxNullImage;

//-----------------------------------------------------------------------------
// wxImage handlers
//-----------------------------------------------------------------------------

#include "wx/imagbmp.h"
#include "wx/imagpng.h"
#include "wx/imaggif.h"
#include "wx/imagpcx.h"
#include "wx/imagjpeg.h"
#include "wx/imagtiff.h"
#include "wx/imagpnm.h"
#include "wx/imagxpm.h"
#include "wx/imagiff.h"

#endif // wxUSE_IMAGE

#endif
  // _WX_IMAGE_H_

