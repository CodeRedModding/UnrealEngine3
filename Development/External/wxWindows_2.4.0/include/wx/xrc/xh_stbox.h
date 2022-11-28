/////////////////////////////////////////////////////////////////////////////
// Name:        xh_stbox.h
// Purpose:     XML resource handler for wxStaticBox
// Author:      Brian Gavin
// Created:     2000/09/00
// RCS-ID:      $Id: xh_stbox.h,v 1.2 2002/09/07 12:10:21 GD Exp $
// Copyright:   (c) 2000 Brian Gavin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_STBOX_H_
#define _WX_XH_STBOX_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "xh_stbox.h"
#endif

#include "wx/xrc/xmlres.h"


class WXXMLDLLEXPORT wxStaticBoxXmlHandler : public wxXmlResourceHandler
{
public:
    wxStaticBoxXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);
};


#endif // _WX_XH_STBOX_H_
