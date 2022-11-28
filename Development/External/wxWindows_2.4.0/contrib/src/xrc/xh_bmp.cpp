/////////////////////////////////////////////////////////////////////////////
// Name:        xh_bmp.cpp
// Purpose:     XRC resource for wxBitmap and wxIcon
// Author:      Vaclav Slavik
// Created:     2000/09/09
// RCS-ID:      $Id: xh_bmp.cpp,v 1.3 2001/12/29 16:14:04 VS Exp $
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
 
#ifdef __GNUG__
#pragma implementation "xh_bmp.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/xrc/xh_bmp.h"
#include "wx/bitmap.h"


wxBitmapXmlHandler::wxBitmapXmlHandler() 
: wxXmlResourceHandler() 
{
}

wxObject *wxBitmapXmlHandler::DoCreateResource()
{ 
    return new wxBitmap(GetBitmap(wxT("")));
}

bool wxBitmapXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("wxBitmap"));
}

wxIconXmlHandler::wxIconXmlHandler() 
: wxXmlResourceHandler() 
{
}

wxObject *wxIconXmlHandler::DoCreateResource()
{ 
    return new wxIcon(GetIcon(wxT("")));
}

bool wxIconXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("wxIcon"));
}
