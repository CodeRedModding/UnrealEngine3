/////////////////////////////////////////////////////////////////////////////
// Name:        imagpcx.h
// Purpose:     wxImage PCX handler
// Author:      Guillermo Rodriguez Garcia <guille@iies.es>
// RCS-ID:      $Id: imagpcx.h,v 1.3.2.1 2002/10/23 17:32:03 RR Exp $
// Copyright:   (c) 1999 Guillermo Rodriguez Garcia
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_IMAGPCX_H_
#define _WX_IMAGPCX_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "imagpcx.h"
#endif

#include "wx/image.h"


//-----------------------------------------------------------------------------
// wxPCXHandler
//-----------------------------------------------------------------------------

#if wxUSE_PCX
class WXDLLEXPORT wxPCXHandler : public wxImageHandler
{
public:
    inline wxPCXHandler()
    {
        m_name = wxT("PCX file");
        m_extension = wxT("pcx");
        m_type = wxBITMAP_TYPE_PCX;
        m_mime = wxT("image/pcx");
    }

#if wxUSE_STREAMS
    virtual bool LoadFile( wxImage *image, wxInputStream& stream, bool verbose=TRUE, int index=-1 );
    virtual bool SaveFile( wxImage *image, wxOutputStream& stream, bool verbose=TRUE );
    virtual bool DoCanRead( wxInputStream& stream );
#endif // wxUSE_STREAMS

private:
    DECLARE_DYNAMIC_CLASS(wxPCXHandler)
};
#endif // wxUSE_PCX


#endif
  // _WX_IMAGPCX_H_

