/////////////////////////////////////////////////////////////////////////////
// Name:        xh_stbmp.cpp
// Purpose:     XRC resource for wxStaticBitmap
// Author:      Vaclav Slavik
// Created:     2000/04/22
// RCS-ID:      $Id: xh_stbmp.cpp,v 1.5 2002/08/20 22:28:15 VS Exp $
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
 
#ifdef __GNUG__
#pragma implementation "xh_stbmp.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/xrc/xh_stbmp.h"
#include "wx/statbmp.h"

wxStaticBitmapXmlHandler::wxStaticBitmapXmlHandler() 
: wxXmlResourceHandler() 
{
    AddWindowStyles();
}

wxObject *wxStaticBitmapXmlHandler::DoCreateResource()
{ 
    XRC_MAKE_INSTANCE(bmp, wxStaticBitmap)

    bmp->Create(m_parentAsWindow,
                GetID(),
                GetBitmap(wxT("bitmap"), wxART_OTHER, GetSize()),
                GetPosition(), GetSize(),
                GetStyle(),
                GetName());

    SetupWindow(bmp);
    
    return bmp;
}

bool wxStaticBitmapXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("wxStaticBitmap"));
}
