/////////////////////////////////////////////////////////////////////////////
// Name:        xh_dlg.h
// Purpose:     XML resource handler for dialogs
// Author:      Vaclav Slavik
// Created:     2000/03/05
// RCS-ID:      $Id: xh_dlg.h,v 1.2 2002/09/07 12:10:21 GD Exp $
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_DLG_H_
#define _WX_XH_DLG_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "xh_dlg.h"
#endif

#include "wx/xrc/xmlres.h"

class WXXMLDLLEXPORT wxDialogXmlHandler : public wxXmlResourceHandler
{
public:
    wxDialogXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);
};


#endif // _WX_XH_DLG_H_
