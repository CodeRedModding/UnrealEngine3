/////////////////////////////////////////////////////////////////////////////
// Name:        gauge95.h
// Purpose:     wxGauge95 class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: gauge95.h,v 1.10 2001/06/26 20:59:07 VZ Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _GAUGE95_H_
#define _GAUGE95_H_

#ifdef __GNUG__
#pragma interface "gauge95.h"
#endif

#if wxUSE_SLIDER

#include "wx/control.h"

WXDLLEXPORT_DATA(extern const wxChar*) wxGaugeNameStr;

// Group box
class WXDLLEXPORT wxGauge95 : public wxControl
{
    DECLARE_DYNAMIC_CLASS(wxGauge95)

public:
    wxGauge95(void) { m_rangeMax = 0; m_gaugePos = 0; }

    wxGauge95(wxWindow *parent, wxWindowID id,
            int range,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxGA_HORIZONTAL,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxGaugeNameStr)
    {
        Create(parent, id, range, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
            int range,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxGA_HORIZONTAL,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxGaugeNameStr);

    void SetShadowWidth(int w);
    void SetBezelFace(int w);
    void SetRange(int r);
    void SetValue(int pos);

    int GetShadowWidth(void) const ;
    int GetBezelFace(void) const ;
    int GetRange(void) const ;
    int GetValue(void) const ;

    bool SetForegroundColour(const wxColour& col);
    bool SetBackgroundColour(const wxColour& col);

    // overriden base class virtuals
    virtual bool AcceptsFocus() const { return FALSE; }

    // Backward compatibility
#if WXWIN_COMPATIBILITY
    void SetButtonColour(const wxColour& col) { SetForegroundColour(col); }
#endif

    virtual void Command(wxCommandEvent& WXUNUSED(event)) {} ;

protected:
    int      m_rangeMax;
    int      m_gaugePos;
};

#endif // wxUSE_GAUGE

#endif
    // _GAUGEMSW_H_
