/////////////////////////////////////////////////////////////////////////////
// Name:        private.h
// Purpose:     Private declarations: as this header is only included by
//              wxWindows itself, it may contain identifiers which don't start
//              with "wx".
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: private.h,v 1.77 2002/09/02 00:32:30 VZ Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_H_
#define _WX_PRIVATE_H_

#ifndef STRICT
    #define STRICT 1
#endif

#include <windows.h>

#ifdef __WXMICROWIN__
    // Extra prototypes and symbols not defined by MicroWindows
    #include "wx/msw/microwin.h"
#endif

// undefine conflicting symbols which were defined in windows.h
#include "wx/msw/winundef.h"

// Include fixes for MSLU:
#include "wx/msw/mslu.h"

#include "wx/log.h"

class WXDLLEXPORT wxFont;
class WXDLLEXPORT wxWindow;

// ---------------------------------------------------------------------------
// private constants
// ---------------------------------------------------------------------------

// Conversion
static const double METRIC_CONVERSION_CONSTANT = 0.0393700787;

// Scaling factors for various unit conversions
static const double mm2inches = (METRIC_CONVERSION_CONSTANT);
static const double inches2mm = (1/METRIC_CONVERSION_CONSTANT);

static const double mm2twips = (METRIC_CONVERSION_CONSTANT*1440);
static const double twips2mm = (1/(METRIC_CONVERSION_CONSTANT*1440));

static const double mm2pt = (METRIC_CONVERSION_CONSTANT*72);
static const double pt2mm = (1/(METRIC_CONVERSION_CONSTANT*72));

// ---------------------------------------------------------------------------
// standard icons from the resources
// ---------------------------------------------------------------------------

#if wxUSE_GUI

WXDLLEXPORT_DATA(extern HICON) wxSTD_FRAME_ICON;
WXDLLEXPORT_DATA(extern HICON) wxSTD_MDIPARENTFRAME_ICON;
WXDLLEXPORT_DATA(extern HICON) wxSTD_MDICHILDFRAME_ICON;
WXDLLEXPORT_DATA(extern HICON) wxDEFAULT_FRAME_ICON;
WXDLLEXPORT_DATA(extern HICON) wxDEFAULT_MDIPARENTFRAME_ICON;
WXDLLEXPORT_DATA(extern HICON) wxDEFAULT_MDICHILDFRAME_ICON;
WXDLLEXPORT_DATA(extern HFONT) wxSTATUS_LINE_FONT;

#endif // wxUSE_GUI

// ---------------------------------------------------------------------------
// define things missing from some compilers' headers
// ---------------------------------------------------------------------------

#if defined(__GNUWIN32__) && !wxUSE_NORLANDER_HEADERS
#ifndef ZeroMemory
    inline void ZeroMemory(void *buf, size_t len) { memset(buf, 0, len); }
#endif
#endif // old mingw32

// this defines a CASTWNDPROC macro which casts a pointer to the type of a
// window proc
#if defined(STRICT) || defined(__GNUC__)
    typedef WNDPROC WndProcCast;
#else
    typedef FARPROC WndProcCast;
#endif

#define CASTWNDPROC (WndProcCast)

// ---------------------------------------------------------------------------
// some stuff for old Windows versions (FIXME: what does it do here??)
// ---------------------------------------------------------------------------

#if !defined(APIENTRY)  // NT defines APIENTRY, 3.x not
    #define APIENTRY FAR PASCAL
#endif

#ifdef __WIN32__
    #define _EXPORT
#else
    #define _EXPORT _export
#endif

#ifndef __WIN32__
    typedef signed short int SHORT;
#endif

#if !defined(__WIN32__)  // 3.x uses FARPROC for dialogs
#ifndef STRICT
    #define DLGPROC FARPROC
#endif
#endif

#if wxUSE_PENWIN
    WXDLLEXPORT void wxRegisterPenWin();
    WXDLLEXPORT void wxCleanUpPenWin();
    WXDLLEXPORT void wxEnablePenAppHooks (bool hook);
#endif // wxUSE_PENWIN

#if wxUSE_ITSY_BITSY
    #define IBS_HORZCAPTION    0x4000L
    #define IBS_VERTCAPTION    0x8000L

    UINT    WINAPI ibGetCaptionSize( HWND hWnd  ) ;
    UINT    WINAPI ibSetCaptionSize( HWND hWnd, UINT nSize ) ;
    LRESULT WINAPI ibDefWindowProc( HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam ) ;
    VOID    WINAPI ibAdjustWindowRect( HWND hWnd, LPRECT lprc ) ;
#endif // wxUSE_ITSY_BITSY

#if wxUSE_CTL3D
    #include "wx/msw/ctl3d/ctl3d.h"
#endif // wxUSE_CTL3D

/*
 * Decide what window classes we're going to use
 * for this combination of CTl3D/FAFA settings
 */

#define STATIC_CLASS     wxT("STATIC")
#define STATIC_FLAGS     (SS_LEFT|WS_CHILD|WS_VISIBLE)
#define CHECK_CLASS      wxT("BUTTON")
#define CHECK_FLAGS      (BS_AUTOCHECKBOX|WS_TABSTOP|WS_CHILD)
#define CHECK_IS_FAFA   FALSE
#define RADIO_CLASS      wxT("BUTTON")
#define RADIO_FLAGS      (BS_AUTORADIOBUTTON|WS_CHILD|WS_VISIBLE)
#define RADIO_SIZE       20
#define RADIO_IS_FAFA   FALSE
#define PURE_WINDOWS
#define GROUP_CLASS      wxT("BUTTON")
#define GROUP_FLAGS      (BS_GROUPBOX|WS_CHILD|WS_VISIBLE)

/*
#define BITCHECK_FLAGS   (FB_BITMAP|FC_BUTTONDRAW|FC_DEFAULT|WS_VISIBLE)
#define BITRADIO_FLAGS   (FC_BUTTONDRAW|FB_BITMAP|FC_RADIO|WS_CHILD|WS_VISIBLE)
*/

// ---------------------------------------------------------------------------
// misc macros
// ---------------------------------------------------------------------------

#define MEANING_CHARACTER '0'
#define DEFAULT_ITEM_WIDTH  100
#define DEFAULT_ITEM_HEIGHT 80

// Scale font to get edit control height
//#define EDIT_HEIGHT_FROM_CHAR_HEIGHT(cy)    (3*(cy)/2)
#define EDIT_HEIGHT_FROM_CHAR_HEIGHT(cy)    (cy+8)

// Generic subclass proc, for panel item moving/sizing and intercept
// EDIT control VK_RETURN messages
extern LONG APIENTRY _EXPORT
  wxSubclassedGenericControlProc(WXHWND hWnd, WXUINT message, WXWPARAM wParam, WXLPARAM lParam);

// ---------------------------------------------------------------------------
// constants which might miss from some compilers' headers
// ---------------------------------------------------------------------------

#if !defined(__WIN32__) && !defined(WS_EX_CLIENTEDGE)
    #define WS_EX_CLIENTEDGE 0
#endif

#if defined(__WIN32__) && !defined(WS_EX_CLIENTEDGE)
    #define WS_EX_CLIENTEDGE 0x00000200L
#endif

#ifndef ENDSESSION_LOGOFF
    #define ENDSESSION_LOGOFF    0x80000000
#endif

// ---------------------------------------------------------------------------
// useful macros and functions
// ---------------------------------------------------------------------------

// a wrapper macro for ZeroMemory()
#if defined(__WIN32__) && !defined(__WXMICROWIN__)
#define wxZeroMemory(obj)   ::ZeroMemory(&obj, sizeof(obj))
#else
#define wxZeroMemory(obj)   memset((void*) & obj, 0, sizeof(obj))
#endif

#if wxUSE_GUI

#include <wx/gdicmn.h>

// make conversion from wxColour and COLORREF a bit less painful
inline COLORREF wxColourToRGB(const wxColour& c)
{
    return RGB(c.Red(), c.Green(), c.Blue());
}

inline COLORREF wxColourToPalRGB(const wxColour& c)
{
    return PALETTERGB(c.Red(), c.Green(), c.Blue());
}

inline wxColour wxRGBToColour(COLORREF rgb)
{
    return wxColour(GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
}

inline void wxRGBToColour(wxColour& c, COLORREF rgb)
{
    c.Set(GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
}

// get the standard colour map for some standard colours - see comment in this
// function to understand why is it needed and when should it be used
//
// it returns a wxCOLORMAP (can't use COLORMAP itself here as comctl32.dll
// might be not included/available) array of size wxSTD_COLOUR_MAX
//
// NB: if you change these colours, update wxBITMAP_STD_COLOURS in the
//     resources as well: it must have the same number of pixels!
enum wxSTD_COLOUR
{
    wxSTD_COL_BTNTEXT,
    wxSTD_COL_BTNSHADOW,
    wxSTD_COL_BTNFACE,
    wxSTD_COL_BTNHIGHLIGHT,
    wxSTD_COL_MAX,
};

struct WXDLLEXPORT wxCOLORMAP
{
    COLORREF from, to;
};

// this function is implemented in src/msw/window.cpp
extern wxCOLORMAP *wxGetStdColourMap();

// copy Windows RECT to our wxRect
inline void wxCopyRECTToRect(const RECT& r, wxRect& rect)
{
    rect.y = r.top;
    rect.x = r.left;
    rect.width = r.right - r.left;
    rect.height = r.bottom - r.top;
}

// translations between HIMETRIC units (which OLE likes) and pixels (which are
// liked by all the others) - implemented in msw/utilsexc.cpp
extern void HIMETRICToPixel(LONG *x, LONG *y);
extern void PixelToHIMETRIC(LONG *x, LONG *y);

// Windows convention of the mask is opposed to the wxWindows one, so we need
// to invert the mask each time we pass one/get one to/from Windows
extern HBITMAP wxInvertMask(HBITMAP hbmpMask, int w = 0, int h = 0);

// get (x, y) from DWORD - notice that HI/LOWORD can *not* be used because they
// will fail on system with multiple monitors where the coords may be negative
//
// these macros are standard now (Win98) but some older headers don't have them
#ifndef GET_X_LPARAM
    #define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
    #define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif // GET_X_LPARAM

// get the current state of SHIFT/CTRL keys
inline bool wxIsShiftDown()
{
    return (::GetKeyState(VK_SHIFT) & 0x100) != 0;
}

inline bool wxIsCtrlDown()
{
    return (::GetKeyState(VK_CONTROL) & 0x100) != 0;
}

// wrapper around GetWindowRect() and GetClientRect() APIs doing error checking
// for Win32
inline RECT wxGetWindowRect(HWND hwnd)
{
    RECT rect;
#ifdef __WIN16__
    ::GetWindowRect(hwnd, &rect);
#else // Win32
    if ( !::GetWindowRect(hwnd, &rect) )
    {
        wxLogLastError(_T("GetWindowRect"));
    }
#endif // Win16/32

    return rect;
}

inline RECT wxGetClientRect(HWND hwnd)
{
    RECT rect;
#ifdef __WIN16__
    ::GetClientRect(hwnd, &rect);
#else // Win32
    if ( !::GetClientRect(hwnd, &rect) )
    {
        wxLogLastError(_T("GetClientRect"));
    }
#endif // Win16/32

    return rect;
}

// ---------------------------------------------------------------------------
// small helper classes
// ---------------------------------------------------------------------------

// create an instance of this class and use it as the HDC for screen, will
// automatically release the DC going out of scope
class ScreenHDC
{
public:
    ScreenHDC() { m_hdc = ::GetDC(NULL);    }
   ~ScreenHDC() { ::ReleaseDC(NULL, m_hdc); }

    operator HDC() const { return m_hdc; }

private:
    HDC m_hdc;
};

// the same as ScreenHDC but for memory DCs: creates the HDC in ctor and
// destroys it in dtor
class MemoryHDC
{
public:
    MemoryHDC() { m_hdc = ::CreateCompatibleDC(NULL); }
   ~MemoryHDC() { ::DeleteObject(m_hdc);              }

    operator HDC() const { return m_hdc; }

private:
    HDC m_hdc;
};

// a class which selects a GDI object into a DC in its ctor and deselects in
// dtor
class SelectInHDC
{
public:
    SelectInHDC(HDC hdc, HGDIOBJ hgdiobj) : m_hdc(hdc)
        { m_hgdiobj = ::SelectObject(hdc, hgdiobj); }

   ~SelectInHDC() { ::SelectObject(m_hdc, m_hgdiobj); }

   // return true if the object was successfully selected
   operator bool() const { return m_hgdiobj != 0; }

private:
   HDC m_hdc;
   HGDIOBJ m_hgdiobj;
};

// ---------------------------------------------------------------------------
// macros to make casting between WXFOO and FOO a bit easier: the GetFoo()
// returns Foo cast to the Windows type for oruselves, while GetFooOf() takes
// an argument which should be a pointer or reference to the object of the
// corresponding class (this depends on the macro)
// ---------------------------------------------------------------------------

#define GetHwnd()               ((HWND)GetHWND())
#define GetHwndOf(win)          ((HWND)((win)->GetHWND()))
// old name
#define GetWinHwnd              GetHwndOf

#define GetHdc()                ((HDC)GetHDC())
#define GetHdcOf(dc)            ((HDC)(dc).GetHDC())

#define GetHbitmap()            ((HBITMAP)GetHBITMAP())
#define GetHbitmapOf(bmp)       ((HBITMAP)(bmp).GetHBITMAP())

#define GetHicon()              ((HICON)GetHICON())
#define GetHiconOf(icon)        ((HICON)(icon).GetHICON())

#define GetHaccel()             ((HACCEL)GetHACCEL())
#define GetHaccelOf(table)      ((HACCEL)((table).GetHACCEL()))

#define GetHmenu()              ((HMENU)GetHMenu())
#define GetHmenuOf(menu)        ((HMENU)menu->GetHMenu())

#define GetHcursor()            ((HCURSOR)GetHCURSOR())
#define GetHcursorOf(cursor)    ((HCURSOR)(cursor).GetHCURSOR())

#define GetHfont()              ((HFONT)GetHFONT())
#define GetHfontOf(font)        ((HFONT)(font).GetHFONT())

#define GetHpalette()           ((HPALETTE)GetHPALETTE())
#define GetHpaletteOf(pal)      ((HPALETTE)(pal).GetHPALETTE())

#define GetHrgn()               ((HRGN)GetHRGN())
#define GetHrgnOf(rgn)          ((HRGN)(rgn).GetHRGN())

#endif // wxUSE_GUI

// ---------------------------------------------------------------------------
// global data
// ---------------------------------------------------------------------------

WXDLLEXPORT_DATA(extern wxChar*) wxBuffer;

WXDLLEXPORT_DATA(extern HINSTANCE) wxhInstance;

// ---------------------------------------------------------------------------
// global functions
// ---------------------------------------------------------------------------

extern "C"
{
    WXDLLEXPORT HINSTANCE wxGetInstance();
}

WXDLLEXPORT void wxSetInstance(HINSTANCE hInst);

#if wxUSE_GUI

// cursor stuff
extern HCURSOR wxGetCurrentBusyCursor();    // from msw/utils.cpp
extern const wxCursor *wxGetGlobalCursor(); // from msw/cursor.cpp

WXDLLEXPORT void wxGetCharSize(WXHWND wnd, int *x, int *y, const wxFont *the_font);
WXDLLEXPORT void wxFillLogFont(LOGFONT *logFont, const wxFont *font);
WXDLLEXPORT wxFont wxCreateFontFromLogFont(const LOGFONT *logFont);
WXDLLEXPORT wxFontEncoding wxGetFontEncFromCharSet(int charset);

WXDLLEXPORT void wxSliderEvent(WXHWND control, WXWORD wParam, WXWORD pos);
WXDLLEXPORT void wxScrollBarEvent(WXHWND hbar, WXWORD wParam, WXWORD pos);

// Find maximum size of window/rectangle
WXDLLEXPORT extern void wxFindMaxSize(WXHWND hwnd, RECT *rect);

// Safely get the window text (i.e. without using fixed size buffer)
WXDLLEXPORT extern wxString wxGetWindowText(WXHWND hWnd);

// get the window class name
WXDLLEXPORT extern wxString wxGetWindowClass(WXHWND hWnd);

// get the window id (should be unsigned, hence this is not wxWindowID which
// is, for mainly historical reasons, signed)
WXDLLEXPORT extern WXWORD wxGetWindowId(WXHWND hWnd);

// check if hWnd's WNDPROC is wndProc. Return true if yes, false if they are
// different
WXDLLEXPORT extern bool wxCheckWindowWndProc(WXHWND hWnd, WXFARPROC wndProc);

// Does this window style specify any border?
inline bool wxStyleHasBorder(long style)
{
    return (style & (wxSIMPLE_BORDER | wxRAISED_BORDER |
                     wxSUNKEN_BORDER | wxDOUBLE_BORDER)) != 0;
}

// ----------------------------------------------------------------------------
// functions mapping HWND to wxWindow
// ----------------------------------------------------------------------------

// this function simply checks whether the given hWnd corresponds to a wxWindow
// and returns either that window if it does or NULL otherwise
WXDLLEXPORT extern wxWindow* wxFindWinFromHandle(WXHWND hWnd);

// find the window for HWND which is part of some wxWindow, i.e. unlike
// wxFindWinFromHandle() above it will also work for "sub controls" of a
// wxWindow.
//
// returns the wxWindow corresponding to the given HWND or NULL.
WXDLLEXPORT extern wxWindow *wxGetWindowFromHWND(WXHWND hwnd);


// Get the size of an icon
WXDLLEXPORT extern wxSize wxGetHiconSize(HICON hicon);

#endif // wxUSE_GUI

#endif
    // _WX_PRIVATE_H_
