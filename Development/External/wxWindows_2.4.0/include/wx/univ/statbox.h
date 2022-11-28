//////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/statbox.h
// Purpose:     wxStaticBox declaration
// Author:      Vadim Zeitlin
// Modified by:
// Created:     15.08.00
// RCS-ID:      $Id: statbox.h,v 1.7 2002/04/14 14:42:42 RR Exp $
// Copyright:   (c) 2000 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_STATBOX_H_
#define _WX_UNIV_STATBOX_H_

#ifdef __GNUG__
    #pragma interface "univstatbox.h"
#endif

class WXDLLEXPORT wxStaticBox : public wxStaticBoxBase
{
public:
    wxStaticBox() { }

    wxStaticBox(wxWindow *parent,
                const wxString& label,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize)
    {
        Create(parent, -1, label, pos, size);
    }

    wxStaticBox(wxWindow *parent, wxWindowID id,
                const wxString& label,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxStaticBoxNameStr)
    {
        Create(parent, id, label, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& label,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxStaticBoxNameStr);

    // the origin of the static box is inside the border and under the label:
    // take account of this
    virtual wxPoint GetBoxAreaOrigin() const;

    virtual bool HasTransparentBackground() { return TRUE; }
    
protected:
    // draw the control
    virtual void DoDraw(wxControlRenderer *renderer);

    // get the size of the border
    wxRect GetBorderGeometry() const;

    // returning TRUE from here ensures that we act as a container window for
    // our children
    virtual bool IsStaticBox() const { return TRUE; }

private:
    DECLARE_DYNAMIC_CLASS(wxStaticBox)
};

#endif // _WX_UNIV_STATBOX_H_
