/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/slider95.h
// Purpose:     wxSlider95 class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: slider95.h,v 1.11 2002/06/20 12:52:06 VZ Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _SLIDER95_H_
#define _SLIDER95_H_

#ifdef __GNUG__
#pragma interface "slider95.h"
#endif

// Slider
class WXDLLEXPORT wxSlider95 : public wxSliderBase
{
public:
    wxSlider95();

    wxSlider95(wxWindow *parent, wxWindowID id,
            int value, int minValue, int maxValue,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxSL_HORIZONTAL,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxSliderNameStr)
    {
        Create(parent, id, value, minValue, maxValue, pos, size, style, validator, name);
    }

    ~wxSlider95();

    bool Create(wxWindow *parent, wxWindowID id,
            int value, int minValue, int maxValue,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxSL_HORIZONTAL,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxSliderNameStr);

    virtual int GetValue() const;
    virtual void SetValue(int);

    void GetSize(int *w, int *h) const;

    void GetPosition(int *x, int *y) const;

    bool Show(bool show = TRUE);

    void SetRange(int minValue, int maxValue);

    int GetMin() const { return m_rangeMin; }
    int GetMax() const { return m_rangeMax; }

    // For trackbars only
    void SetTickFreq(int n, int pos);
    int GetTickFreq() const { return m_tickFreq; }
    void SetPageSize(int pageSize);
    int GetPageSize() const;
    void ClearSel();
    void ClearTicks();
    void SetLineSize(int lineSize);
    int GetLineSize() const;
    int GetSelEnd() const;
    int GetSelStart() const;
    void SetSelection(int minPos, int maxPos);
    void SetThumbLength(int len);
    int GetThumbLength() const;
    void SetTick(int tickPos);

    // IMPLEMENTATION
    WXHWND GetStaticMin() const { return m_staticMin; }
    WXHWND GetStaticMax() const { return m_staticMax; }
    WXHWND GetEditValue() const { return m_staticValue; }
    virtual bool ContainsHWND(WXHWND hWnd) const;

    void Command(wxCommandEvent& event);
    virtual WXHBRUSH OnCtlColor(WXHDC pDC, WXHWND pWnd, WXUINT nCtlColor,
            WXUINT message, WXWPARAM wParam, WXLPARAM lParam);
    virtual bool MSWOnScroll(int orientation, WXWORD wParam,
                             WXWORD pos, WXHWND control);

protected:
    WXHWND        m_staticMin;
    WXHWND        m_staticMax;
    WXHWND        m_staticValue;
    int           m_rangeMin;
    int           m_rangeMax;
    int           m_pageSize;
    int           m_lineSize;
    int           m_tickFreq;

    virtual void DoGetSize(int *width, int *height) const;

    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO);

    DECLARE_DYNAMIC_CLASS(wxSlider95)
};

#endif
    // _SLIDER95_H_
