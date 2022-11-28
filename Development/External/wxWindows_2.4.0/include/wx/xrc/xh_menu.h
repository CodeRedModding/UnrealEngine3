/////////////////////////////////////////////////////////////////////////////
// Name:        xh_menu.h
// Purpose:     XML resource handler for menus/menubars
// Author:      Vaclav Slavik
// Created:     2000/03/05
// RCS-ID:      $Id: xh_menu.h,v 1.2 2002/09/07 12:10:21 GD Exp $
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_MENU_H_
#define _WX_XH_MENU_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "xh_menu.h"
#endif

#include "wx/xrc/xmlres.h"

class WXXMLDLLEXPORT wxMenuXmlHandler : public wxXmlResourceHandler
{
public:
    wxMenuXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);

private:
    bool m_insideMenu;
};

class WXXMLDLLEXPORT wxMenuBarXmlHandler : public wxXmlResourceHandler
{
    public:
        wxMenuBarXmlHandler();
        virtual wxObject *DoCreateResource();
        virtual bool CanHandle(wxXmlNode *node);
};


#endif // _WX_XH_MENU_H_
