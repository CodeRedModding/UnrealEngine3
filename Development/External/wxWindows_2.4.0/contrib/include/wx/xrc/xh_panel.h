/////////////////////////////////////////////////////////////////////////////
// Name:        xh_panel.h
// Purpose:     XML resource handler for panels
// Author:      Vaclav Slavik
// Created:     2000/03/05
// RCS-ID:      $Id: xh_panel.h,v 1.2 2002/09/07 12:10:21 GD Exp $
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_PANEL_H_
#define _WX_XH_PANEL_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "xh_panel.h"
#endif

#include "wx/xrc/xmlres.h"

class WXXMLDLLEXPORT wxPanelXmlHandler : public wxXmlResourceHandler
{
public:
    wxPanelXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);
};


#endif // _WX_XH_PANEL_H_
