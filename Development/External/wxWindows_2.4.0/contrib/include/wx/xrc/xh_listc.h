/////////////////////////////////////////////////////////////////////////////
// Name:        xh_listc.h
// Purpose:     XML resource handler for wxCalendarCtrl
// Author:      Brian Gavin
// Created:     2000/09/09
// RCS-ID:      $Id: xh_listc.h,v 1.2 2002/09/07 12:10:21 GD Exp $
// Copyright:   (c) 2000 Brian Gavin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_LISTC_H_
#define _WX_XH_LISTC_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "xh_listc.h"
#endif

#include "wx/xrc/xmlres.h"

class WXXMLDLLEXPORT wxListCtrlXmlHandler : public wxXmlResourceHandler
{
public:
    wxListCtrlXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);
};


#endif // _WX_XH_LISTC_H_
