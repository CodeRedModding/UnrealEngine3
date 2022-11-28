/////////////////////////////////////////////////////////////////////////////
// Name:        splash.h
// Purpose:     Splash screen class
// Author:      Julian Smart
// Modified by:
// Created:     28/6/2000
// RCS-ID:      $Id: splash.h,v 1.5 2002/08/31 11:29:12 GD Exp $
// Copyright:   (c) Julian Smart
// Licence:
/////////////////////////////////////////////////////////////////////////////

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "splash.h"
#endif

#ifndef _WX_SPLASH_H_
#define _WX_SPLASH_H_

#ifndef WX_PRECOMP
#include "wx/bitmap.h"
#include "wx/timer.h"
#endif

#include "wx/frame.h"


/*
 * A window for displaying a splash screen
 */

#define wxSPLASH_CENTRE_ON_PARENT   0x01
#define wxSPLASH_CENTRE_ON_SCREEN   0x02
#define wxSPLASH_NO_CENTRE          0x00
#define wxSPLASH_TIMEOUT            0x04
#define wxSPLASH_NO_TIMEOUT         0x00

class WXDLLEXPORT wxSplashScreenWindow;

/*
 * wxSplashScreen
 */

class WXDLLEXPORT wxSplashScreen: public wxFrame
{
public:
    // for RTTI macros only
    wxSplashScreen() {};
    wxSplashScreen(const wxBitmap& bitmap, long splashStyle, int milliseconds,
                   wxWindow* parent, wxWindowID id,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = wxSIMPLE_BORDER|wxFRAME_NO_TASKBAR|wxSTAY_ON_TOP);
    ~wxSplashScreen();

    void OnCloseWindow(wxCloseEvent& event);
    void OnNotify(wxTimerEvent& event);

    long GetSplashStyle() const { return m_splashStyle; }
    wxSplashScreenWindow* GetSplashWindow() const { return m_window; }
    int GetTimeout() const { return m_milliseconds; }

protected:
    wxSplashScreenWindow*   m_window;
    long                    m_splashStyle;
    int                     m_milliseconds;
    wxTimer                 m_timer;

DECLARE_DYNAMIC_CLASS(wxSplashScreen)
DECLARE_EVENT_TABLE()
};

/*
 * wxSplashScreenWindow
 */

class WXDLLEXPORT wxSplashScreenWindow: public wxWindow
{
public:
    wxSplashScreenWindow(const wxBitmap& bitmap, wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxNO_BORDER);

    void OnPaint(wxPaintEvent& event);
    void OnEraseBackground(wxEraseEvent& event);
    void OnMouseEvent(wxMouseEvent& event);
    void OnChar(wxKeyEvent& event);

    void SetBitmap(const wxBitmap& bitmap) { m_bitmap = bitmap; }
    wxBitmap& GetBitmap() { return m_bitmap; }

protected:
    wxBitmap    m_bitmap;

DECLARE_EVENT_TABLE()
};


#endif
    // _WX_SPLASH_H_
