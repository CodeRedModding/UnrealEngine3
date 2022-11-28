///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/popupwin.h
// Purpose:     wxPopupWindow class for wxMSW
// Author:      Vadim Zeitlin
// Modified by:
// Created:     06.01.01
// RCS-ID:      $Id: popupwin.h,v 1.9 2002/06/05 23:29:59 VZ Exp $
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_POPUPWIN_H_
#define _WX_MSW_POPUPWIN_H_

#ifdef __GNUG__
    #pragma interface "popup.h"
#endif

// ----------------------------------------------------------------------------
// wxPopupWindow
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxPopupWindow : public wxPopupWindowBase
{
public:
    wxPopupWindow() { }

    wxPopupWindow(wxWindow *parent, int flags = wxBORDER_NONE)
        { (void)Create(parent, flags); }

    bool Create(wxWindow *parent, int flags = wxBORDER_NONE);

protected:
    // popups handle the position like wxTopLevelWindow, not wxWindow
    virtual void DoGetPosition(int *x, int *y) const;

    // return the style to be used for the popup windows
    virtual WXDWORD MSWGetStyle(long flags, WXDWORD *exstyle) const;

    // get the HWND to be used as parent of this window with CreateWindow()
    virtual WXHWND MSWGetParent() const;

    DECLARE_DYNAMIC_CLASS(wxPopupWindow)
};

#endif // _WX_MSW_POPUPWIN_H_

