/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/brush.h
// Purpose:     wxBrush class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: brush.h,v 1.9.2.1 2002/09/21 23:01:24 VZ Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_BRUSH_H_
#define _WX_BRUSH_H_

#ifdef __GNUG__
    #pragma interface "brush.h"
#endif

#include "wx/gdicmn.h"
#include "wx/gdiobj.h"
#include "wx/bitmap.h"

class WXDLLEXPORT wxBrush;

// ----------------------------------------------------------------------------
// wxBrush
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxBrush : public wxGDIObject
{
public:
    wxBrush();
    wxBrush(const wxColour& col, int style);
    wxBrush(const wxBitmap& stipple);
    wxBrush(const wxBrush& brush) { Ref(brush); }
    virtual ~wxBrush();

    virtual void SetColour(const wxColour& col);
    virtual void SetColour(unsigned char r, unsigned char g, unsigned char b);
    virtual void SetStyle(int style);
    virtual void SetStipple(const wxBitmap& stipple);

    wxBrush& operator=(const wxBrush& brush);
    bool operator==(const wxBrush& brush) const;
    bool operator!=(const wxBrush& brush) const { return !(*this == brush); }

    wxColour GetColour() const;
    int GetStyle() const;
    wxBitmap *GetStipple() const;

    bool Ok() const { return m_refData != NULL; }

    // return the HBRUSH for this brush
    virtual WXHANDLE GetResourceHandle() const;

protected:
    virtual wxObjectRefData *CreateRefData() const;
    virtual wxObjectRefData *CloneRefData(const wxObjectRefData *data) const;

private:
    DECLARE_DYNAMIC_CLASS(wxBrush)
};

#endif
    // _WX_BRUSH_H_
