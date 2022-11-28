/////////////////////////////////////////////////////////////////////////////
// Name:        imagpnm.h
// Purpose:     wxImage PNM handler
// Author:      Sylvain Bougnoux
// RCS-ID:      $Id: imagpnm.h,v 1.3.2.1 2002/10/23 17:32:03 RR Exp $
// Copyright:   (c) Sylvain Bougnoux
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_IMAGPNM_H_
#define _WX_IMAGPNM_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "imagpnm.h"
#endif

#include "wx/image.h"

//-----------------------------------------------------------------------------
// wxPNMHandler
//-----------------------------------------------------------------------------

#if wxUSE_PNM
class WXDLLEXPORT wxPNMHandler : public wxImageHandler
{
public:
    inline wxPNMHandler()
    {
        m_name = wxT("PNM file");
        m_extension = wxT("pnm");
        m_type = wxBITMAP_TYPE_PNM;
        m_mime = wxT("image/pnm");
    }

#if wxUSE_STREAMS
    virtual bool LoadFile( wxImage *image, wxInputStream& stream, bool verbose=TRUE, int index=-1 );
    virtual bool SaveFile( wxImage *image, wxOutputStream& stream, bool verbose=TRUE );
    virtual bool DoCanRead( wxInputStream& stream );
#endif

private:
    DECLARE_DYNAMIC_CLASS(wxPNMHandler)
};
#endif


#endif
  // _WX_IMAGPNM_H_

