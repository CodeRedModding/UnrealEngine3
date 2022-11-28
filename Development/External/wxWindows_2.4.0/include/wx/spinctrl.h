/////////////////////////////////////////////////////////////////////////////
// Name:        spinctrl.h
// Purpose:     wxSpinCtrlBase class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     22.07.99
// RCS-ID:      $Id: spinctrl.h,v 1.13 2002/09/01 20:24:25 VZ Exp $
// Copyright:   (c) Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SPINCTRL_H_
#define _WX_SPINCTRL_H_

#include "wx/spinbutt.h"        // should make wxSpinEvent visible to the app

// ----------------------------------------------------------------------------
// a spin ctrl is a text control with a spin button which is usually used to
// prompt the user for a numeric input
// ----------------------------------------------------------------------------

/* there is no generic base class for this control because it's imlpemented
   very differently under MSW and other platforms

class WXDLLEXPORT wxSpinCtrlBase : public wxControl
{
public:
    wxSpinCtrlBase() { Init(); }

    // accessors
    virtual int GetValue() const = 0;
    virtual int GetMin() const { return m_min; }
    virtual int GetMax() const { return m_max; }

    // operations
    virtual void SetValue(const wxString& value) = 0;
    virtual void SetValue(int val) = 0;
    virtual void SetRange(int minVal, int maxVal) = 0;

    // as the wxTextCtrl method
    virtual void SetSelection(long from, long to) = 0;

protected:
    // initialize m_min/max with the default values
    void Init() { m_min = 0; m_max = 100; }

    int   m_min;
    int   m_max;
};
*/

// ----------------------------------------------------------------------------
// include the platform-dependent class implementation
// ----------------------------------------------------------------------------

#if defined(__WXUNIVERSAL__)
    #include "wx/generic/spinctlg.h"
#elif defined(__WXMSW__) && defined(__WIN32__)
    #include "wx/msw/spinctrl.h"
#elif defined(__WXPM__)
    #include "wx/os2/spinctrl.h"
#elif defined(__WXGTK__)
    #include "wx/gtk/spinctrl.h"
#elif defined(__WXMOTIF__)
    #include "wx/generic/spinctlg.h"
#elif defined(__WXMAC__)
    #include "wx/mac/spinctrl.h"
#else // Win16 || !Win
    #include "wx/generic/spinctlg.h"
#endif // platform

#define EVT_SPINCTRL(id, fn) \
    DECLARE_EVENT_TABLE_ENTRY( wxEVT_COMMAND_SPINCTRL_UPDATED, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxSpinEventFunction) & fn, (wxObject *) NULL ),

#endif // _WX_SPINCTRL_H_

