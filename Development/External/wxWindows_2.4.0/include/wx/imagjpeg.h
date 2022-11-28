/////////////////////////////////////////////////////////////////////////////
// Name:        imagjpeg.h
// Purpose:     wxImage JPEG handler
// Author:      Vaclav Slavik
// RCS-ID:      $Id: imagjpeg.h,v 1.3.2.1 2002/10/23 17:32:02 RR Exp $
// Copyright:   (c) Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_IMAGJPEG_H_
#define _WX_IMAGJPEG_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "imagjpeg.h"
#endif

#include "wx/image.h"

//-----------------------------------------------------------------------------
// wxJPEGHandler
//-----------------------------------------------------------------------------

#if wxUSE_LIBJPEG
class WXDLLEXPORT wxJPEGHandler: public wxImageHandler
{
public:
    inline wxJPEGHandler()
    {
        m_name = wxT("JPEG file");
        m_extension = wxT("jpg");
        m_type = wxBITMAP_TYPE_JPEG;
        m_mime = wxT("image/jpeg");
    }

#if wxUSE_STREAMS
    virtual bool LoadFile( wxImage *image, wxInputStream& stream, bool verbose=TRUE, int index=-1 );
    virtual bool SaveFile( wxImage *image, wxOutputStream& stream, bool verbose=TRUE );
    virtual bool DoCanRead( wxInputStream& stream );
#endif

private:
    DECLARE_DYNAMIC_CLASS(wxJPEGHandler)
};
#endif



#endif
  // _WX_IMAGJPEG_H_

