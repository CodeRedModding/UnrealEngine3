/////////////////////////////////////////////////////////////////////////////
// Name:        xh_sttxt.h
// Purpose:     XML resource handler for wxStaticBitmap
// Author:      Bob Mitchell
// Created:     2000/03/21
// RCS-ID:      $Id: xh_sttxt.h,v 1.2 2002/09/07 12:10:21 GD Exp $
// Copyright:   (c) 2000 Bob Mitchell
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_STTXT_H_
#define _WX_XH_STTXT_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "xh_sttxt.h"
#endif

#include "wx/xrc/xmlres.h"


class WXXMLDLLEXPORT wxStaticTextXmlHandler : public wxXmlResourceHandler
{
public:
    wxStaticTextXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);
};


#endif // _WX_XH_STBMP_H_
