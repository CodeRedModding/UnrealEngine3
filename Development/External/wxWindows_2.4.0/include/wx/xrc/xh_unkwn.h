/////////////////////////////////////////////////////////////////////////////
// Name:        xh_unkwn.h
// Purpose:     XML resource handler for unkown widget
// Author:      Vaclav Slavik
// Created:     2000/03/05
// RCS-ID:      $Id: xh_unkwn.h,v 1.2 2002/09/07 12:10:21 GD Exp $
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_UNKWN_H_
#define _WX_XH_UNKWN_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "xh_unkwn.h"
#endif

#include "wx/xrc/xmlres.h"


class WXXMLDLLEXPORT wxUnknownWidgetXmlHandler : public wxXmlResourceHandler
{
public:
    wxUnknownWidgetXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);
};


#endif // _WX_XH_UNKWN_H_
