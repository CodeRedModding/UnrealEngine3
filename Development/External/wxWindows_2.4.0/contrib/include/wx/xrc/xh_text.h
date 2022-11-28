/////////////////////////////////////////////////////////////////////////////
// Name:        xh_text.h
// Purpose:     XML resource handler for wxTextCtrl
// Author:      Aleksandras Gluchovas
// Created:     2000/03/21
// RCS-ID:      $Id: xh_text.h,v 1.2 2002/09/07 12:10:21 GD Exp $
// Copyright:   (c) 2000 Aleksandras Gluchovas
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_TEXT_H_
#define _WX_XH_TEXT_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "xh_text.h"
#endif

#include "wx/xrc/xmlres.h"


class WXXMLDLLEXPORT wxTextCtrlXmlHandler : public wxXmlResourceHandler
{
public:
    wxTextCtrlXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);
};


#endif // _WX_XH_TEXT_H_
