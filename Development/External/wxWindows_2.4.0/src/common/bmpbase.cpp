/////////////////////////////////////////////////////////////////////////////
// Name:        bitmap.cpp
// Purpose:     wxBitmapBase
// Author:      VaclavSlavik
// Created:     2001/04/11
// RCS-ID:      $Id: bmpbase.cpp,v 1.5 2002/02/05 00:14:48 VZ Exp $
// Copyright:   (c) 2001, Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "bitmapbase.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if defined(__WXMGL__) || defined(__WXMAC__)

#include "wx/wx.h"
#include "wx/setup.h"
#include "wx/utils.h"
#include "wx/palette.h"
#include "wx/bitmap.h"
#include "wx/icon.h"
#include "wx/log.h"
#include "wx/image.h"
#include "wx/module.h"

IMPLEMENT_ABSTRACT_CLASS(wxBitmapBase, wxGDIObject)
IMPLEMENT_ABSTRACT_CLASS(wxBitmapHandlerBase,wxObject)

wxList wxBitmapBase::sm_handlers;

void wxBitmapBase::AddHandler(wxBitmapHandlerBase *handler)
{
    sm_handlers.Append(handler);
}

void wxBitmapBase::InsertHandler(wxBitmapHandlerBase *handler)
{
    sm_handlers.Insert(handler);
}

bool wxBitmapBase::RemoveHandler(const wxString& name)
{
    wxBitmapHandler *handler = FindHandler(name);
    if ( handler )
    {
        sm_handlers.DeleteObject(handler);
        return TRUE;
    }
    else
        return FALSE;
}

wxBitmapHandler *wxBitmapBase::FindHandler(const wxString& name)
{
    wxNode *node = sm_handlers.First();
    while ( node )
    {
        wxBitmapHandler *handler = (wxBitmapHandler *)node->Data();
        if ( handler->GetName() == name )
            return handler;
        node = node->Next();
    }
    return NULL;
}

wxBitmapHandler *wxBitmapBase::FindHandler(const wxString& extension, wxBitmapType bitmapType)
{
    wxNode *node = sm_handlers.First();
    while ( node )
    {
        wxBitmapHandler *handler = (wxBitmapHandler *)node->Data();
        if ( handler->GetExtension() == extension &&
                    (bitmapType == -1 || handler->GetType() == bitmapType) )
            return handler;
        node = node->Next();
    }
    return NULL;
}

wxBitmapHandler *wxBitmapBase::FindHandler(wxBitmapType bitmapType)
{
    wxNode *node = sm_handlers.First();
    while ( node )
    {
        wxBitmapHandler *handler = (wxBitmapHandler *)node->Data();
        if (handler->GetType() == bitmapType)
            return handler;
        node = node->Next();
    }
    return NULL;
}

void wxBitmapBase::CleanUpHandlers()
{
    wxNode *node = sm_handlers.First();
    while ( node )
    {
        wxBitmapHandler *handler = (wxBitmapHandler *)node->Data();
        wxNode *next = node->Next();
        delete handler;
        delete node;
        node = next;
    }
}

class wxBitmapBaseModule: public wxModule
{
DECLARE_DYNAMIC_CLASS(wxBitmapBaseModule)
public:
    wxBitmapBaseModule() {}
    bool OnInit() { wxBitmap::InitStandardHandlers(); return TRUE; };
    void OnExit() { wxBitmap::CleanUpHandlers(); };
};

IMPLEMENT_DYNAMIC_CLASS(wxBitmapBaseModule, wxModule)

#endif // defined(__WXMGL__) || defined(__WXMAC__)

