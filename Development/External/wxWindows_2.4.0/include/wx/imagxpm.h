/////////////////////////////////////////////////////////////////////////////
// Name:        imaggif.h
// Purpose:     wxImage XPM handler
// Author:      Vaclav Slavik
// RCS-ID:      $Id: imagxpm.h,v 1.4.2.1 2002/10/23 17:32:04 RR Exp $
// Copyright:   (c) 2001 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_IMAGXPM_H_
#define _WX_IMAGXPM_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "imagxpm.h"
#endif

#include "wx/image.h"


//-----------------------------------------------------------------------------
// wxXPMHandler
//-----------------------------------------------------------------------------

class WXDLLEXPORT wxXPMHandler : public wxImageHandler
{
public:
    inline wxXPMHandler()
    {
        m_name = wxT("XPM file");
        m_extension = wxT("xpm");
        m_type = wxBITMAP_TYPE_XPM;
        m_mime = wxT("image/xpm");
    }

#if wxUSE_STREAMS
    virtual bool LoadFile( wxImage *image, wxInputStream& stream, bool verbose=TRUE, int index=-1 );
    virtual bool SaveFile( wxImage *image, wxOutputStream& stream, bool verbose=TRUE );
    virtual bool DoCanRead( wxInputStream& stream );
#endif

private:
    DECLARE_DYNAMIC_CLASS(wxXPMHandler)
};


#endif
  // _WX_IMAGXPM_H_

