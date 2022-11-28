/////////////////////////////////////////////////////////////////////////////
// Name:        wx/statbmp.h
// Purpose:     wxStaticBitmap class interface
// Author:      Vadim Zeitlin
// Modified by:
// Created:     25.08.00
// RCS-ID:      $Id: statbmp.h,v 1.11 2002/08/31 11:29:11 GD Exp $
// Copyright:   (c) 2000 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STATBMP_H_BASE_
#define _WX_STATBMP_H_BASE_

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "statbmpbase.h"
#endif

#if wxUSE_STATBMP

#include "wx/control.h"
#include "wx/bitmap.h"

class WXDLLEXPORT wxIcon;
class WXDLLEXPORT wxBitmap;

WXDLLEXPORT_DATA(extern const wxChar*) wxStaticBitmapNameStr;

// a control showing an icon or a bitmap
class WXDLLEXPORT wxStaticBitmapBase : public wxControl
{
 public:
    virtual ~wxStaticBitmapBase();
    
    // our interface
    virtual void SetIcon(const wxIcon& icon) = 0;
    virtual void SetBitmap(const wxBitmap& bitmap) = 0;
    virtual wxBitmap GetBitmap() const = 0;

    // overriden base class virtuals
    virtual bool AcceptsFocus() const { return FALSE; }

protected:
    virtual wxSize DoGetBestClientSize() const;
};

#if defined(__WXUNIVERSAL__)
    #include "wx/univ/statbmp.h"
#elif defined(__WXMSW__)
    #include "wx/msw/statbmp.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/statbmp.h"
#elif defined(__WXGTK__)
    #include "wx/gtk/statbmp.h"
#elif defined(__WXMAC__)
    #include "wx/mac/statbmp.h"
#elif defined(__WXPM__)
    #include "wx/os2/statbmp.h"
#elif defined(__WXSTUBS__)
    #include "wx/stubs/statbmp.h"
#endif

#endif // wxUSE_STATBMP

#endif
    // _WX_STATBMP_H_BASE_
