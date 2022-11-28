/////////////////////////////////////////////////////////////////////////////
// Name:        xh_stbox.cpp
// Purpose:     XRC resource for wxStaticBox
// Author:      Brian Gavin
// Created:     2000/09/09
// RCS-ID:      $Id: xh_stbox.cpp,v 1.4 2001/12/29 16:14:04 VS Exp $
// Copyright:   (c) 2000 Brian Gavin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
 
#ifdef __GNUG__
#pragma implementation "xh_stbox.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/xrc/xh_stbox.h"
#include "wx/statbox.h"

wxStaticBoxXmlHandler::wxStaticBoxXmlHandler() 
: wxXmlResourceHandler() 
{
    AddWindowStyles();
}

wxObject *wxStaticBoxXmlHandler::DoCreateResource()
{ 
    XRC_MAKE_INSTANCE(box, wxStaticBox)

    box->Create(m_parentAsWindow,
                GetID(),
                GetText(wxT("label")),
                GetPosition(), GetSize(),
                GetStyle(),
                GetName());

    SetupWindow(box);
    
    return box;
}

bool wxStaticBoxXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("wxStaticBox"));
}
