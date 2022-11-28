/////////////////////////////////////////////////////////////////////////////
// Name:        dcclient.h
// Purpose:     wxClientDC class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: dcclient.h,v 1.14 2002/02/23 21:32:35 VZ Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DCCLIENT_H_
#define _WX_DCCLIENT_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma interface "dcclient.h"
#endif

#include "wx/dc.h"
#include "wx/dynarray.h"

// ----------------------------------------------------------------------------
// array types
// ----------------------------------------------------------------------------

// this one if used by wxPaintDC only
struct WXDLLEXPORT wxPaintDCInfo;

WX_DECLARE_EXPORTED_OBJARRAY(wxPaintDCInfo, wxArrayDCInfo);

// ----------------------------------------------------------------------------
// DC classes
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxWindowDC : public wxDC
{
public:
    // default ctor
    wxWindowDC();

    // Create a DC corresponding to the whole window
    wxWindowDC(wxWindow *win);

protected:
    // intiialize the newly created DC
    void InitDC();

    // override some base class virtuals
    virtual void DoGetSize(int *width, int *height) const;

private:
    DECLARE_DYNAMIC_CLASS(wxWindowDC)
};

class WXDLLEXPORT wxClientDC : public wxWindowDC
{
public:
    // default ctor
    wxClientDC();

    // Create a DC corresponding to the client area of the window
    wxClientDC(wxWindow *win);

    virtual ~wxClientDC();

protected:
    void InitDC();

    // override some base class virtuals
    virtual void DoGetSize(int *width, int *height) const;

private:
    DECLARE_DYNAMIC_CLASS(wxClientDC)
};

class WXDLLEXPORT wxPaintDC : public wxClientDC
{
public:
    wxPaintDC();

    // Create a DC corresponding for painting the window in OnPaint()
    wxPaintDC(wxWindow *win);

    virtual ~wxPaintDC();

    // find the entry for this DC in the cache (keyed by the window)
    static WXHDC FindDCInCache(wxWindow* win);

protected:
    static wxArrayDCInfo ms_cache;

    // find the entry for this DC in the cache (keyed by the window)
    wxPaintDCInfo *FindInCache(size_t *index = NULL) const;

private:
    DECLARE_DYNAMIC_CLASS(wxPaintDC)
};

#endif
    // _WX_DCCLIENT_H_
