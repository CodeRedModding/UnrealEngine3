/////////////////////////////////////////////////////////////////////////////
// Name:        imagiff.h
// Purpose:     wxImage handler for Amiga IFF images
// Author:      Steffen Gutmann
// RCS-ID:      $Id: imagiff.h,v 1.3.2.1 2002/10/23 17:32:02 RR Exp $
// Copyright:   (c) Steffen Gutmann, 2002
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_IMAGE_IFF_H_
#define _WX_IMAGE_IFF_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "imagiff.h"
#endif

#include "wx/image.h"

//-----------------------------------------------------------------------------
// wxIFFHandler
//-----------------------------------------------------------------------------

#if wxUSE_IMAGE && wxUSE_IFF

class WXDLLEXPORT wxIFFHandler : public wxImageHandler
{
public:
    wxIFFHandler()
    {
        m_name = wxT("IFF file");
        m_extension = wxT("iff");
        m_type = wxBITMAP_TYPE_IFF;
        m_mime = wxT("image/x-iff");
    }

#if wxUSE_STREAMS
    virtual bool LoadFile(wxImage *image, wxInputStream& stream, bool verbose=TRUE, int index=-1);
    virtual bool SaveFile(wxImage *image, wxOutputStream& stream, bool verbose=TRUE);
    virtual bool DoCanRead(wxInputStream& stream);
#endif

private:
    DECLARE_DYNAMIC_CLASS(wxIFFHandler)
};

#endif // wxUSE_IMAGE && wxUSE_IFF

#endif // _WX_IMAGE_IFF_H_
