/////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/spinbutt.h
// Purpose:     wxSpinButton class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: spinbutt.h,v 1.16 2001/07/20 11:59:46 VZ Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SPINBUTT_H_
#define _WX_SPINBUTT_H_

#ifdef __GNUG__
    #pragma interface "spinbutt.h"
#endif

#include "wx/control.h"
#include "wx/event.h"

class WXDLLEXPORT wxSpinButton : public wxSpinButtonBase
{
public:
    // construction
    wxSpinButton() { }

    wxSpinButton(wxWindow *parent,
                 wxWindowID id = -1,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxSP_VERTICAL | wxSP_ARROW_KEYS,
                 const wxString& name = wxSPIN_BUTTON_NAME)
    {
        Create(parent, id, pos, size, style, name);
    }

    virtual ~wxSpinButton();

    bool Create(wxWindow *parent,
                wxWindowID id = -1,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSP_VERTICAL | wxSP_ARROW_KEYS,
                const wxString& name = wxSPIN_BUTTON_NAME);


    // accessors
    virtual int GetValue() const;
    virtual void SetValue(int val);
    virtual void SetRange(int minVal, int maxVal);

    // implementation
    virtual bool MSWCommand(WXUINT param, WXWORD id);
    virtual bool MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result);
    virtual bool MSWOnScroll(int orientation, WXWORD wParam,
                             WXWORD pos, WXHWND control);

    // a wxSpinButton can't do anything useful with focus, only wxSpinCtrl can
    virtual bool AcceptsFocus() const { return FALSE; }

protected:
   virtual wxSize DoGetBestSize() const;

private:
    DECLARE_DYNAMIC_CLASS(wxSpinButton)
};

#endif
    // _WX_SPINBUTT_H_
