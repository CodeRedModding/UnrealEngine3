/////////////////////////////////////////////////////////////////////////////
// Name:        dc.cpp
// Purpose:     wxDC class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: dc.cpp,v 1.130 2002/08/30 20:34:25 JS Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "dc.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/window.h"
    #include "wx/dc.h"
    #include "wx/utils.h"
    #include "wx/dialog.h"
    #include "wx/app.h"
    #include "wx/bitmap.h"
    #include "wx/dcmemory.h"
    #include "wx/log.h"
    #include "wx/icon.h"
#endif

#include "wx/sysopt.h"
#include "wx/dcprint.h"
#include "wx/module.h"

#include <string.h>
#include <math.h>

#include "wx/msw/private.h" // needs to be before #include <commdlg.h>

#if wxUSE_COMMON_DIALOGS && !defined(__WXMICROWIN__)
    #include <commdlg.h>
#endif

#ifndef __WIN32__
    #include <print.h>
#endif

/* Quaternary raster codes */
#ifndef MAKEROP4
#define MAKEROP4(fore,back) (DWORD)((((back) << 8) & 0xFF000000) | (fore))
#endif

IMPLEMENT_ABSTRACT_CLASS(wxDC, wxDCBase)

// ---------------------------------------------------------------------------
// constants
// ---------------------------------------------------------------------------

static const int VIEWPORT_EXTENT = 1000;

static const int MM_POINTS = 9;
static const int MM_METRIC = 10;

// usually this is defined in math.h
#ifndef M_PI
    static const double M_PI = 3.14159265358979323846;
#endif // M_PI

// ROPs which don't have standard names (see "Ternary Raster Operations" in the
// MSDN docs for how this and other numbers in wxDC::Blit() are obtained)
#define DSTCOPY 0x00AA0029      // a.k.a. NOP operation

// ----------------------------------------------------------------------------
// macros for logical <-> device coords conversion
// ----------------------------------------------------------------------------

/*
   We currently let Windows do all the translations itself so these macros are
   not really needed (any more) but keep them to enhance readability of the
   code by allowing to see where are the logical and where are the device
   coordinates used.
 */

// logical to device
#define XLOG2DEV(x) (x)
#define YLOG2DEV(y) (y)

// device to logical
#define XDEV2LOG(x) (x)
#define YDEV2LOG(y) (y)

// ---------------------------------------------------------------------------
// private functions
// ---------------------------------------------------------------------------

// convert degrees to radians
static inline double DegToRad(double deg) { return (deg * M_PI) / 180.0; }

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// instead of duplicating the same code which sets and then restores text
// colours in each wxDC method working with wxSTIPPLE_MASK_OPAQUE brushes,
// encapsulate this in a small helper class

// wxColourChanger: changes the text colours in the ctor if required and
//                  restores them in the dtor
class wxColourChanger
{
public:
    wxColourChanger(wxDC& dc);
   ~wxColourChanger();

private:
    wxDC& m_dc;

    COLORREF m_colFgOld, m_colBgOld;

    bool m_changed;
};

// ===========================================================================
// implementation
// ===========================================================================

// ----------------------------------------------------------------------------
// wxColourChanger
// ----------------------------------------------------------------------------

wxColourChanger::wxColourChanger(wxDC& dc) : m_dc(dc)
{
    const wxBrush& brush = dc.GetBrush();
    if ( brush.Ok() && brush.GetStyle() == wxSTIPPLE_MASK_OPAQUE )
    {
        HDC hdc = GetHdcOf(dc);
        m_colFgOld = ::GetTextColor(hdc);
        m_colBgOld = ::GetBkColor(hdc);

        // note that Windows convention is opposite to wxWindows one, this is
        // why text colour becomes the background one and vice versa
        const wxColour& colFg = dc.GetTextForeground();
        if ( colFg.Ok() )
        {
            ::SetBkColor(hdc, colFg.GetPixel());
        }

        const wxColour& colBg = dc.GetTextBackground();
        if ( colBg.Ok() )
        {
            ::SetTextColor(hdc, colBg.GetPixel());
        }

        SetBkMode(hdc,
                  dc.GetBackgroundMode() == wxTRANSPARENT ? TRANSPARENT
                                                          : OPAQUE);

        // flag which telsl us to undo changes in the dtor
        m_changed = TRUE;
    }
    else
    {
        // nothing done, nothing to undo
        m_changed = FALSE;
    }
}

wxColourChanger::~wxColourChanger()
{
    if ( m_changed )
    {
        // restore the colours we changed
        HDC hdc = GetHdcOf(m_dc);

        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, m_colFgOld);
        ::SetBkColor(hdc, m_colBgOld);
    }
}

// ---------------------------------------------------------------------------
// wxDC
// ---------------------------------------------------------------------------

// Default constructor
wxDC::wxDC()
{
    m_canvas = NULL;

    m_oldBitmap = 0;
    m_oldPen = 0;
    m_oldBrush = 0;
    m_oldFont = 0;
#if wxUSE_PALETTE
    m_oldPalette = 0;
#endif // wxUSE_PALETTE

    m_bOwnsDC = FALSE;
    m_hDC = 0;
}

wxDC::~wxDC()
{
    if ( m_hDC != 0 )
    {
        SelectOldObjects(m_hDC);

        // if we own the HDC, we delete it, otherwise we just release it

        if ( m_bOwnsDC )
        {
            ::DeleteDC(GetHdc());
        }
        else // we don't own our HDC
        {
            if (m_canvas)
            {
                ::ReleaseDC(GetHwndOf(m_canvas), GetHdc());
            }
            else
            {
                // Must have been a wxScreenDC
                ::ReleaseDC((HWND) NULL, GetHdc());
            }
        }
    }
}

// This will select current objects out of the DC,
// which is what you have to do before deleting the
// DC.
void wxDC::SelectOldObjects(WXHDC dc)
{
    if (dc)
    {
        if (m_oldBitmap)
        {
            ::SelectObject((HDC) dc, (HBITMAP) m_oldBitmap);
            if (m_selectedBitmap.Ok())
            {
                m_selectedBitmap.SetSelectedInto(NULL);
            }
        }
        m_oldBitmap = 0;
        if (m_oldPen)
        {
            ::SelectObject((HDC) dc, (HPEN) m_oldPen);
        }
        m_oldPen = 0;
        if (m_oldBrush)
        {
            ::SelectObject((HDC) dc, (HBRUSH) m_oldBrush);
        }
        m_oldBrush = 0;
        if (m_oldFont)
        {
            ::SelectObject((HDC) dc, (HFONT) m_oldFont);
        }
        m_oldFont = 0;

#if wxUSE_PALETTE
        if (m_oldPalette)
        {
            ::SelectPalette((HDC) dc, (HPALETTE) m_oldPalette, FALSE);
        }
        m_oldPalette = 0;
#endif // wxUSE_PALETTE
    }

    m_brush = wxNullBrush;
    m_pen = wxNullPen;
#if wxUSE_PALETTE
    m_palette = wxNullPalette;
#endif // wxUSE_PALETTE
    m_font = wxNullFont;
    m_backgroundBrush = wxNullBrush;
    m_selectedBitmap = wxNullBitmap;
}

// ---------------------------------------------------------------------------
// clipping
// ---------------------------------------------------------------------------

void wxDC::UpdateClipBox()
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    RECT rect;
    ::GetClipBox(GetHdc(), &rect);

    m_clipX1 = (wxCoord) XDEV2LOG(rect.left);
    m_clipY1 = (wxCoord) YDEV2LOG(rect.top);
    m_clipX2 = (wxCoord) XDEV2LOG(rect.right);
    m_clipY2 = (wxCoord) YDEV2LOG(rect.bottom);
}

// common part of DoSetClippingRegion() and DoSetClippingRegionAsRegion()
void wxDC::SetClippingHrgn(WXHRGN hrgn)
{
    wxCHECK_RET( hrgn, wxT("invalid clipping region") );

#ifdef __WXMICROWIN__
    if (!GetHdc()) return;
#endif // __WXMICROWIN__

    // note that we combine the new clipping region with the existing one: this
    // is compatible with what the other ports do and is the documented
    // behaviour now (starting with 2.3.3)
#ifdef __WIN16__
    RECT rectClip;
    if ( !::GetClipBox(GetHdc(), &rectClip) )
        return;

    HRGN hrgnDest = ::CreateRectRgn(0, 0, 0, 0);
    HRGN hrgnClipOld = ::CreateRectRgn(rectClip.left, rectClip.top,
                                       rectClip.right, rectClip.bottom);

    if ( ::CombineRgn(hrgnDest, hrgnClipOld, (HRGN)hrgn, RGN_AND) != ERROR )
    {
        ::SelectClipRgn(GetHdc(), hrgnDest);
    }

    ::DeleteObject(hrgnClipOld);
    ::DeleteObject(hrgnDest);
#else // Win32
    if ( ::ExtSelectClipRgn(GetHdc(), (HRGN)hrgn, RGN_AND) == ERROR )
    {
        wxLogLastError(_T("ExtSelectClipRgn"));

        return;
    }
#endif // Win16/32

    m_clipping = TRUE;

    UpdateClipBox();
}

void wxDC::DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord w, wxCoord h)
{
    // the region coords are always the device ones, so do the translation
    // manually
    //
    // FIXME: possible +/-1 error here, to check!
    HRGN hrgn = ::CreateRectRgn(LogicalToDeviceX(x),
                                LogicalToDeviceY(y),
                                LogicalToDeviceX(x + w),
                                LogicalToDeviceY(y + h));
    if ( !hrgn )
    {
        wxLogLastError(_T("CreateRectRgn"));
    }
    else
    {
        SetClippingHrgn((WXHRGN)hrgn);

        ::DeleteObject(hrgn);
    }
}

void wxDC::DoSetClippingRegionAsRegion(const wxRegion& region)
{
    SetClippingHrgn(region.GetHRGN());
}

void wxDC::DestroyClippingRegion()
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    if (m_clipping && m_hDC)
    {
        // TODO: this should restore the previous clipping region,
        //       so that OnPaint processing works correctly, and the update
        //       clipping region doesn't get destroyed after the first
        //       DestroyClippingRegion.
        HRGN rgn = CreateRectRgn(0, 0, 32000, 32000);
        ::SelectClipRgn(GetHdc(), rgn);
        ::DeleteObject(rgn);
    }

    m_clipping = FALSE;
}

// ---------------------------------------------------------------------------
// query capabilities
// ---------------------------------------------------------------------------

bool wxDC::CanDrawBitmap() const
{
    return TRUE;
}

bool wxDC::CanGetTextExtent() const
{
#ifdef __WXMICROWIN__
    // TODO Extend MicroWindows' GetDeviceCaps function
    return TRUE;
#else
    // What sort of display is it?
    int technology = ::GetDeviceCaps(GetHdc(), TECHNOLOGY);

    return (technology == DT_RASDISPLAY) || (technology == DT_RASPRINTER);
#endif
}

int wxDC::GetDepth() const
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return 16;
#endif

    return (int)::GetDeviceCaps(GetHdc(), BITSPIXEL);
}

// ---------------------------------------------------------------------------
// drawing
// ---------------------------------------------------------------------------

void wxDC::Clear()
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    RECT rect;
    if ( m_canvas )
    {
        GetClientRect((HWND) m_canvas->GetHWND(), &rect);
    }
    else
    {
        // No, I think we should simply ignore this if printing on e.g.
        // a printer DC.
        // wxCHECK_RET( m_selectedBitmap.Ok(), wxT("this DC can't be cleared") );
        if (!m_selectedBitmap.Ok())
            return;

        rect.left = 0; rect.top = 0;
        rect.right = m_selectedBitmap.GetWidth();
        rect.bottom = m_selectedBitmap.GetHeight();
    }

    (void) ::SetMapMode(GetHdc(), MM_TEXT);

    DWORD colour = ::GetBkColor(GetHdc());
    HBRUSH brush = ::CreateSolidBrush(colour);
    ::FillRect(GetHdc(), &rect, brush);
    ::DeleteObject(brush);

    int width = DeviceToLogicalXRel(VIEWPORT_EXTENT)*m_signX,
        height = DeviceToLogicalYRel(VIEWPORT_EXTENT)*m_signY;

    ::SetMapMode(GetHdc(), MM_ANISOTROPIC);
    ::SetViewportExtEx(GetHdc(), VIEWPORT_EXTENT, VIEWPORT_EXTENT, NULL);
    ::SetWindowExtEx(GetHdc(), width, height, NULL);
    ::SetViewportOrgEx(GetHdc(), (int)m_deviceOriginX, (int)m_deviceOriginY, NULL);
    ::SetWindowOrgEx(GetHdc(), (int)m_logicalOriginX, (int)m_logicalOriginY, NULL);
}

bool wxDC::DoFloodFill(wxCoord x, wxCoord y, const wxColour& col, int style)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return FALSE;
#endif

    bool success = (0 != ::ExtFloodFill(GetHdc(), XLOG2DEV(x), YLOG2DEV(y),
                         col.GetPixel(),
                         style == wxFLOOD_SURFACE ? FLOODFILLSURFACE
                                                  : FLOODFILLBORDER) ) ;
    if (!success)
    {
        // quoting from the MSDN docs:
        //
        //      Following are some of the reasons this function might fail:
        //
        //      * The filling could not be completed.
        //      * The specified point has the boundary color specified by the
        //        crColor parameter (if FLOODFILLBORDER was requested).
        //      * The specified point does not have the color specified by
        //        crColor (if FLOODFILLSURFACE was requested)
        //      * The point is outside the clipping region that is, it is not
        //        visible on the device.
        //
        wxLogLastError(wxT("ExtFloodFill"));
    }

    CalcBoundingBox(x, y);
    
    return success;
}

bool wxDC::DoGetPixel(wxCoord x, wxCoord y, wxColour *col) const
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return FALSE;
#endif

    wxCHECK_MSG( col, FALSE, _T("NULL colour parameter in wxDC::GetPixel") );

    // get the color of the pixel
    COLORREF pixelcolor = ::GetPixel(GetHdc(), XLOG2DEV(x), YLOG2DEV(y));

    wxRGBToColour(*col, pixelcolor);

    return TRUE;
}

void wxDC::DoCrossHair(wxCoord x, wxCoord y)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    wxCoord x1 = x-VIEWPORT_EXTENT;
    wxCoord y1 = y-VIEWPORT_EXTENT;
    wxCoord x2 = x+VIEWPORT_EXTENT;
    wxCoord y2 = y+VIEWPORT_EXTENT;

    (void)MoveToEx(GetHdc(), XLOG2DEV(x1), YLOG2DEV(y), NULL);
    (void)LineTo(GetHdc(), XLOG2DEV(x2), YLOG2DEV(y));

    (void)MoveToEx(GetHdc(), XLOG2DEV(x), YLOG2DEV(y1), NULL);
    (void)LineTo(GetHdc(), XLOG2DEV(x), YLOG2DEV(y2));

    CalcBoundingBox(x1, y1);
    CalcBoundingBox(x2, y2);
}

void wxDC::DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    (void)MoveToEx(GetHdc(), XLOG2DEV(x1), YLOG2DEV(y1), NULL);
    (void)LineTo(GetHdc(), XLOG2DEV(x2), YLOG2DEV(y2));

    CalcBoundingBox(x1, y1);
    CalcBoundingBox(x2, y2);
}

// Draws an arc of a circle, centred on (xc, yc), with starting point (x1, y1)
// and ending at (x2, y2)
void wxDC::DoDrawArc(wxCoord x1, wxCoord y1,
                     wxCoord x2, wxCoord y2,
                     wxCoord xc, wxCoord yc)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    wxColourChanger cc(*this); // needed for wxSTIPPLE_MASK_OPAQUE handling

    double dx = xc - x1;
    double dy = yc - y1;
    double radius = (double)sqrt(dx*dx+dy*dy);
    wxCoord r = (wxCoord)radius;

    // treat the special case of full circle separately
    if ( x1 == x2 && y1 == y2 )
    {
        DrawEllipse(xc - r, yc - r, 2*r, 2*r);
        return;
    }

    wxCoord xx1 = XLOG2DEV(x1);
    wxCoord yy1 = YLOG2DEV(y1);
    wxCoord xx2 = XLOG2DEV(x2);
    wxCoord yy2 = YLOG2DEV(y2);
    wxCoord xxc = XLOG2DEV(xc);
    wxCoord yyc = YLOG2DEV(yc);
    wxCoord ray = (wxCoord) sqrt(double((xxc-xx1)*(xxc-xx1)+(yyc-yy1)*(yyc-yy1)));

    wxCoord xxx1 = (wxCoord) (xxc-ray);
    wxCoord yyy1 = (wxCoord) (yyc-ray);
    wxCoord xxx2 = (wxCoord) (xxc+ray);
    wxCoord yyy2 = (wxCoord) (yyc+ray);

    if ( m_brush.Ok() && m_brush.GetStyle() != wxTRANSPARENT )
    {
        // Have to add 1 to bottom-right corner of rectangle
        // to make semi-circles look right (crooked line otherwise).
        // Unfortunately this is not a reliable method, depends
        // on the size of shape.
        // TODO: figure out why this happens!
        Pie(GetHdc(),xxx1,yyy1,xxx2+1,yyy2+1, xx1,yy1,xx2,yy2);
    }
    else
    {
        Arc(GetHdc(),xxx1,yyy1,xxx2,yyy2, xx1,yy1,xx2,yy2);
    }

    CalcBoundingBox(xc - r, yc - r);
    CalcBoundingBox(xc + r, yc + r);
}

void wxDC::DoDrawCheckMark(wxCoord x1, wxCoord y1,
                           wxCoord width, wxCoord height)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    wxCoord x2 = x1 + width,
            y2 = y1 + height;

#if defined(__WIN32__) && !defined(__SC__) && !defined(__WXMICROWIN__)
    RECT rect;
    rect.left   = x1;
    rect.top    = y1;
    rect.right  = x2;
    rect.bottom = y2;

    DrawFrameControl(GetHdc(), &rect, DFC_MENU, DFCS_MENUCHECK);
#else // Win16
    // In WIN16, draw a cross
    HPEN blackPen = ::CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HPEN whiteBrush = (HPEN)::GetStockObject(WHITE_BRUSH);
    HPEN hPenOld = (HPEN)::SelectObject(GetHdc(), blackPen);
    HPEN hBrushOld = (HPEN)::SelectObject(GetHdc(), whiteBrush);
    ::SetROP2(GetHdc(), R2_COPYPEN);
    Rectangle(GetHdc(), x1, y1, x2, y2);
    MoveToEx(GetHdc(), x1, y1, NULL);
    LineTo(GetHdc(), x2, y2);
    MoveToEx(GetHdc(), x2, y1, NULL);
    LineTo(GetHdc(), x1, y2);
    ::SelectObject(GetHdc(), hPenOld);
    ::SelectObject(GetHdc(), hBrushOld);
    ::DeleteObject(blackPen);
#endif // Win32/16

    CalcBoundingBox(x1, y1);
    CalcBoundingBox(x2, y2);
}

void wxDC::DoDrawPoint(wxCoord x, wxCoord y)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    COLORREF color = 0x00ffffff;
    if (m_pen.Ok())
    {
        color = m_pen.GetColour().GetPixel();
    }

    SetPixel(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), color);

    CalcBoundingBox(x, y);
}

void wxDC::DoDrawPolygon(int n, wxPoint points[], wxCoord xoffset, wxCoord yoffset,int fillStyle)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    wxColourChanger cc(*this); // needed for wxSTIPPLE_MASK_OPAQUE handling

    // Do things less efficiently if we have offsets
    if (xoffset != 0 || yoffset != 0)
    {
        POINT *cpoints = new POINT[n];
        int i;
        for (i = 0; i < n; i++)
        {
            cpoints[i].x = (int)(points[i].x + xoffset);
            cpoints[i].y = (int)(points[i].y + yoffset);

            CalcBoundingBox(cpoints[i].x, cpoints[i].y);
        }
        int prev = SetPolyFillMode(GetHdc(),fillStyle==wxODDEVEN_RULE?ALTERNATE:WINDING);
        (void)Polygon(GetHdc(), cpoints, n);
        SetPolyFillMode(GetHdc(),prev);
        delete[] cpoints;
    }
    else
    {
        int i;
        for (i = 0; i < n; i++)
            CalcBoundingBox(points[i].x, points[i].y);

        int prev = SetPolyFillMode(GetHdc(),fillStyle==wxODDEVEN_RULE?ALTERNATE:WINDING);
        (void)Polygon(GetHdc(), (POINT*) points, n);
        SetPolyFillMode(GetHdc(),prev);
    }
}

void wxDC::DoDrawLines(int n, wxPoint points[], wxCoord xoffset, wxCoord yoffset)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    // Do things less efficiently if we have offsets
    if (xoffset != 0 || yoffset != 0)
    {
        POINT *cpoints = new POINT[n];
        int i;
        for (i = 0; i < n; i++)
        {
            cpoints[i].x = (int)(points[i].x + xoffset);
            cpoints[i].y = (int)(points[i].y + yoffset);

            CalcBoundingBox(cpoints[i].x, cpoints[i].y);
        }
        (void)Polyline(GetHdc(), cpoints, n);
        delete[] cpoints;
    }
    else
    {
        int i;
        for (i = 0; i < n; i++)
            CalcBoundingBox(points[i].x, points[i].y);

        (void)Polyline(GetHdc(), (POINT*) points, n);
    }
}

void wxDC::DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    wxColourChanger cc(*this); // needed for wxSTIPPLE_MASK_OPAQUE handling

    wxCoord x2 = x + width;
    wxCoord y2 = y + height;

    if ((m_logicalFunction == wxCOPY) && (m_pen.GetStyle() == wxTRANSPARENT))
    {
        RECT rect;
        rect.left = XLOG2DEV(x);
        rect.top = YLOG2DEV(y);
        rect.right = XLOG2DEV(x2);
        rect.bottom = YLOG2DEV(y2);
        (void)FillRect(GetHdc(), &rect, (HBRUSH)m_brush.GetResourceHandle() );
    }
    else
    {
        // Windows draws the filled rectangles without outline (i.e. drawn with a
        // transparent pen) one pixel smaller in both directions and we want them
        // to have the same size regardless of which pen is used - adjust

        // I wonder if this shouldn�t be done after the LOG2DEV() conversions. RR.
        if ( m_pen.GetStyle() == wxTRANSPARENT )
        {
            x2++;
            y2++;
        }

        (void)Rectangle(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), XLOG2DEV(x2), YLOG2DEV(y2));
    }


    CalcBoundingBox(x, y);
    CalcBoundingBox(x2, y2);
}

void wxDC::DoDrawRoundedRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height, double radius)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    wxColourChanger cc(*this); // needed for wxSTIPPLE_MASK_OPAQUE handling

    // Now, a negative radius value is interpreted to mean
    // 'the proportion of the smallest X or Y dimension'

    if (radius < 0.0)
    {
        double smallest = 0.0;
        if (width < height)
            smallest = width;
        else
            smallest = height;
        radius = (- radius * smallest);
    }

    wxCoord x2 = (x+width);
    wxCoord y2 = (y+height);

    // Windows draws the filled rectangles without outline (i.e. drawn with a
    // transparent pen) one pixel smaller in both directions and we want them
    // to have the same size regardless of which pen is used - adjust
    if ( m_pen.GetStyle() == wxTRANSPARENT )
    {
        x2++;
        y2++;
    }

    (void)RoundRect(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), XLOG2DEV(x2),
        YLOG2DEV(y2), (int) (2*XLOG2DEV(radius)), (int)( 2*YLOG2DEV(radius)));

    CalcBoundingBox(x, y);
    CalcBoundingBox(x2, y2);
}

void wxDC::DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    wxColourChanger cc(*this); // needed for wxSTIPPLE_MASK_OPAQUE handling

    wxCoord x2 = (x+width);
    wxCoord y2 = (y+height);

    (void)Ellipse(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), XLOG2DEV(x2), YLOG2DEV(y2));

    CalcBoundingBox(x, y);
    CalcBoundingBox(x2, y2);
}

// Chris Breeze 20/5/98: first implementation of DrawEllipticArc on Windows
void wxDC::DoDrawEllipticArc(wxCoord x,wxCoord y,wxCoord w,wxCoord h,double sa,double ea)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    wxColourChanger cc(*this); // needed for wxSTIPPLE_MASK_OPAQUE handling

    wxCoord x2 = x + w;
    wxCoord y2 = y + h;

    int rx1 = XLOG2DEV(x+w/2);
    int ry1 = YLOG2DEV(y+h/2);
    int rx2 = rx1;
    int ry2 = ry1;

    sa = DegToRad(sa);
    ea = DegToRad(ea);

    rx1 += (int)(100.0 * abs(w) * cos(sa));
    ry1 -= (int)(100.0 * abs(h) * m_signY * sin(sa));
    rx2 += (int)(100.0 * abs(w) * cos(ea));
    ry2 -= (int)(100.0 * abs(h) * m_signY * sin(ea));

    // draw pie with NULL_PEN first and then outline otherwise a line is
    // drawn from the start and end points to the centre
    HPEN hpenOld = (HPEN) ::SelectObject(GetHdc(), (HPEN) ::GetStockObject(NULL_PEN));
    if (m_signY > 0)
    {
        (void)Pie(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), XLOG2DEV(x2)+1, YLOG2DEV(y2)+1,
                  rx1, ry1, rx2, ry2);
    }
    else
    {
        (void)Pie(GetHdc(), XLOG2DEV(x), YLOG2DEV(y)-1, XLOG2DEV(x2)+1, YLOG2DEV(y2),
                  rx1, ry1-1, rx2, ry2-1);
    }

    ::SelectObject(GetHdc(), hpenOld);

    (void)Arc(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), XLOG2DEV(x2), YLOG2DEV(y2),
              rx1, ry1, rx2, ry2);

    CalcBoundingBox(x, y);
    CalcBoundingBox(x2, y2);
}

void wxDC::DoDrawIcon(const wxIcon& icon, wxCoord x, wxCoord y)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    wxCHECK_RET( icon.Ok(), wxT("invalid icon in DrawIcon") );

#ifdef __WIN32__
    ::DrawIconEx(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), GetHiconOf(icon), icon.GetWidth(), icon.GetHeight(), 0, NULL, DI_NORMAL);
#else
    ::DrawIcon(GetHdc(), XLOG2DEV(x), YLOG2DEV(y), GetHiconOf(icon));
#endif

    CalcBoundingBox(x, y);
    CalcBoundingBox(x + icon.GetWidth(), y + icon.GetHeight());
}

void wxDC::DoDrawBitmap( const wxBitmap &bmp, wxCoord x, wxCoord y, bool useMask )
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    wxCHECK_RET( bmp.Ok(), _T("invalid bitmap in wxDC::DrawBitmap") );

    int width = bmp.GetWidth(),
        height = bmp.GetHeight();

    HBITMAP hbmpMask = 0;

#if wxUSE_PALETTE
    HPALETTE oldPal = 0;
#endif // wxUSE_PALETTE

    if ( useMask )
    {
        wxMask *mask = bmp.GetMask();
        if ( mask )
            hbmpMask = (HBITMAP)mask->GetMaskBitmap();

        if ( !hbmpMask )
        {
            // don't give assert here because this would break existing
            // programs - just silently ignore useMask parameter
            useMask = FALSE;
        }
    }
    if ( useMask )
    {
#ifdef __WIN32__
        // use MaskBlt() with ROP which doesn't do anything to dst in the mask
        // points
        // On some systems, MaskBlt succeeds yet is much much slower
        // than the wxWindows fall-back implementation. So we need
        // to be able to switch this on and off at runtime.
        bool ok = FALSE;
#if wxUSE_SYSTEM_OPTIONS
        if (wxSystemOptions::GetOptionInt(wxT("no-maskblt")) == 0)
#endif
        {
            HDC cdc = GetHdc();
            HDC hdcMem = ::CreateCompatibleDC(GetHdc());
            HGDIOBJ hOldBitmap = ::SelectObject(hdcMem, GetHbitmapOf(bmp));
#if wxUSE_PALETTE
            wxPalette *pal = bmp.GetPalette();
            if ( pal && ::GetDeviceCaps(cdc,BITSPIXEL) <= 8 )
            {
                oldPal = ::SelectPalette(hdcMem, GetHpaletteOf(*pal), FALSE);
                ::RealizePalette(hdcMem);
            }
#endif // wxUSE_PALETTE

            ok = ::MaskBlt(cdc, x, y, width, height,
                            hdcMem, 0, 0,
                            hbmpMask, 0, 0,
                            MAKEROP4(SRCCOPY, DSTCOPY)) != 0;

#if wxUSE_PALETTE
            if (oldPal)
                ::SelectPalette(hdcMem, oldPal, FALSE);
#endif // wxUSE_PALETTE

            ::SelectObject(hdcMem, hOldBitmap);
            ::DeleteDC(hdcMem);
        }

        if ( !ok )
#endif // Win32
        {
            // Rather than reproduce wxDC::Blit, let's do it at the wxWin API
            // level
            wxMemoryDC memDC;
            memDC.SelectObject(bmp);

            Blit(x, y, width, height, &memDC, 0, 0, wxCOPY, useMask);

            memDC.SelectObject(wxNullBitmap);
        }
    }
    else // no mask, just use BitBlt()
    {
        HDC cdc = GetHdc();
        HDC memdc = ::CreateCompatibleDC( cdc );
        HBITMAP hbitmap = (HBITMAP) bmp.GetHBITMAP( );

        wxASSERT_MSG( hbitmap, wxT("bitmap is ok but HBITMAP is NULL?") );

        COLORREF old_textground = ::GetTextColor(GetHdc());
        COLORREF old_background = ::GetBkColor(GetHdc());
        if (m_textForegroundColour.Ok())
        {
            ::SetTextColor(GetHdc(), m_textForegroundColour.GetPixel() );
        }
        if (m_textBackgroundColour.Ok())
        {
            ::SetBkColor(GetHdc(), m_textBackgroundColour.GetPixel() );
        }

#if wxUSE_PALETTE
        wxPalette *pal = bmp.GetPalette();
        if ( pal && ::GetDeviceCaps(cdc,BITSPIXEL) <= 8 )
        {
            oldPal = ::SelectPalette(memdc, GetHpaletteOf(*pal), FALSE);
            ::RealizePalette(memdc);
        }
#endif // wxUSE_PALETTE

        HGDIOBJ hOldBitmap = ::SelectObject( memdc, hbitmap );
        ::BitBlt( cdc, x, y, width, height, memdc, 0, 0, SRCCOPY);

#if wxUSE_PALETTE
        if (oldPal)
            ::SelectPalette(memdc, oldPal, FALSE);
#endif // wxUSE_PALETTE

        ::SelectObject( memdc, hOldBitmap );
        ::DeleteDC( memdc );

        ::SetTextColor(GetHdc(), old_textground);
        ::SetBkColor(GetHdc(), old_background);
    }
}

void wxDC::DoDrawText(const wxString& text, wxCoord x, wxCoord y)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    DrawAnyText(text, x, y);

    // update the bounding box
    CalcBoundingBox(x, y);

    wxCoord w, h;
    GetTextExtent(text, &w, &h);
    CalcBoundingBox(x + w, y + h);
}

void wxDC::DrawAnyText(const wxString& text, wxCoord x, wxCoord y)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    // prepare for drawing the text
    if ( m_textForegroundColour.Ok() )
        SetTextColor(GetHdc(), m_textForegroundColour.GetPixel());

    DWORD old_background = 0;
    if ( m_textBackgroundColour.Ok() )
    {
        old_background = SetBkColor(GetHdc(), m_textBackgroundColour.GetPixel() );
    }

    SetBkMode(GetHdc(), m_backgroundMode == wxTRANSPARENT ? TRANSPARENT
                                                          : OPAQUE);

    if ( ::TextOut(GetHdc(), XLOG2DEV(x), YLOG2DEV(y),
                   text.c_str(), text.length()) == 0 )
    {
        wxLogLastError(wxT("TextOut"));
    }

    // restore the old parameters (text foreground colour may be left because
    // it never is set to anything else, but background should remain
    // transparent even if we just drew an opaque string)
    if ( m_textBackgroundColour.Ok() )
        (void)SetBkColor(GetHdc(), old_background);

    SetBkMode(GetHdc(), TRANSPARENT);
}

void wxDC::DoDrawRotatedText(const wxString& text,
                             wxCoord x, wxCoord y,
                             double angle)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    // we test that we have some font because otherwise we should still use the
    // "else" part below to avoid that DrawRotatedText(angle = 180) and
    // DrawRotatedText(angle = 0) use different fonts (we can't use the default
    // font for drawing rotated fonts unfortunately)
    if ( (angle == 0.0) && m_font.Ok() )
    {
        DoDrawText(text, x, y);
    }
#ifndef __WXMICROWIN__
    else
    {
        // NB: don't take DEFAULT_GUI_FONT (a.k.a. wxSYS_DEFAULT_GUI_FONT)
        //     because it's not TrueType and so can't have non zero
        //     orientation/escapement under Win9x
        wxFont font = m_font.Ok() ? m_font : *wxSWISS_FONT;
        HFONT hfont = (HFONT)font.GetResourceHandle();
        LOGFONT lf;
        if ( ::GetObject(hfont, sizeof(lf), &lf) == 0 )
        {
            wxLogLastError(wxT("GetObject(hfont)"));
        }

        // GDI wants the angle in tenth of degree
        long angle10 = (long)(angle * 10);
        lf.lfEscapement = angle10;
        lf. lfOrientation = angle10;

        hfont = ::CreateFontIndirect(&lf);
        if ( !hfont )
        {
            wxLogLastError(wxT("CreateFont"));
        }
        else
        {
            HFONT hfontOld = (HFONT)::SelectObject(GetHdc(), hfont);

            DrawAnyText(text, x, y);

            (void)::SelectObject(GetHdc(), hfontOld);
            (void)::DeleteObject(hfont);
        }

        // call the bounding box by adding all four vertices of the rectangle
        // containing the text to it (simpler and probably not slower than
        // determining which of them is really topmost/leftmost/...)
        wxCoord w, h;
        GetTextExtent(text, &w, &h);

        double rad = DegToRad(angle);

        // "upper left" and "upper right"
        CalcBoundingBox(x, y);
        CalcBoundingBox(x + wxCoord(w*cos(rad)), y - wxCoord(h*sin(rad)));

        // "bottom left" and "bottom right"
        x += (wxCoord)(h*sin(rad));
        y += (wxCoord)(h*cos(rad));
        CalcBoundingBox(x, y);
        CalcBoundingBox(x + wxCoord(h*sin(rad)), y + wxCoord(h*cos(rad)));
    }
#endif
}

// ---------------------------------------------------------------------------
// set GDI objects
// ---------------------------------------------------------------------------

#if wxUSE_PALETTE

void wxDC::DoSelectPalette(bool realize)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    // Set the old object temporarily, in case the assignment deletes an object
    // that's not yet selected out.
    if (m_oldPalette)
    {
        ::SelectPalette(GetHdc(), (HPALETTE) m_oldPalette, FALSE);
        m_oldPalette = 0;
    }

    if ( m_palette.Ok() )
    {
        HPALETTE oldPal = ::SelectPalette(GetHdc(),
                                          GetHpaletteOf(m_palette),
                                          FALSE);
        if (!m_oldPalette)
            m_oldPalette = (WXHPALETTE) oldPal;

        if (realize)
            ::RealizePalette(GetHdc());
    }
}

void wxDC::SetPalette(const wxPalette& palette)
{
    if ( palette.Ok() )
    {
        m_palette = palette;
        DoSelectPalette(TRUE);
    }
}

void wxDC::InitializePalette()
{
    if ( wxDisplayDepth() <= 8 )
    {
        // look for any window or parent that has a custom palette. If any has
        // one then we need to use it in drawing operations
        wxWindow *win = m_canvas->GetAncestorWithCustomPalette();

        m_hasCustomPalette = win && win->HasCustomPalette();
        if ( m_hasCustomPalette )
        {
            m_palette = win->GetPalette();

            // turn on MSW translation for this palette
            DoSelectPalette();
        }
    }
}

#endif // wxUSE_PALETTE

void wxDC::SetFont(const wxFont& the_font)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    // Set the old object temporarily, in case the assignment deletes an object
    // that's not yet selected out.
    if (m_oldFont)
    {
        ::SelectObject(GetHdc(), (HFONT) m_oldFont);
        m_oldFont = 0;
    }

    m_font = the_font;

    if (!the_font.Ok())
    {
        if (m_oldFont)
            ::SelectObject(GetHdc(), (HFONT) m_oldFont);
        m_oldFont = 0;
    }

    if (m_font.Ok() && m_font.GetResourceHandle())
    {
        HFONT f = (HFONT) ::SelectObject(GetHdc(), (HFONT) m_font.GetResourceHandle());
        if (f == (HFONT) NULL)
        {
            wxLogDebug(wxT("::SelectObject failed in wxDC::SetFont."));
        }
        if (!m_oldFont)
            m_oldFont = (WXHFONT) f;
    }
}

void wxDC::SetPen(const wxPen& pen)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    // Set the old object temporarily, in case the assignment deletes an object
    // that's not yet selected out.
    if (m_oldPen)
    {
        ::SelectObject(GetHdc(), (HPEN) m_oldPen);
        m_oldPen = 0;
    }

    m_pen = pen;

    if (!m_pen.Ok())
    {
        if (m_oldPen)
            ::SelectObject(GetHdc(), (HPEN) m_oldPen);
        m_oldPen = 0;
    }

    if (m_pen.Ok())
    {
        if (m_pen.GetResourceHandle())
        {
            HPEN p = (HPEN) ::SelectObject(GetHdc(), (HPEN)m_pen.GetResourceHandle());
            if (!m_oldPen)
                m_oldPen = (WXHPEN) p;
        }
    }
}

void wxDC::SetBrush(const wxBrush& brush)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    // Set the old object temporarily, in case the assignment deletes an object
    // that's not yet selected out.
    if (m_oldBrush)
    {
        ::SelectObject(GetHdc(), (HBRUSH) m_oldBrush);
        m_oldBrush = 0;
    }

    m_brush = brush;

    if (!m_brush.Ok())
    {
        if (m_oldBrush)
            ::SelectObject(GetHdc(), (HBRUSH) m_oldBrush);
        m_oldBrush = 0;
    }

    if (m_brush.Ok())
    {
        // to make sure the brush is alligned with the logical coordinates
        wxBitmap *stipple = m_brush.GetStipple();
        if ( stipple && stipple->Ok() )
        {
#ifdef __WIN32__
            ::SetBrushOrgEx(GetHdc(),
                            m_deviceOriginX % stipple->GetWidth(),
                            m_deviceOriginY % stipple->GetHeight(),
                            NULL);  // don't need previous brush origin
#else
            ::SetBrushOrg(GetHdc(),
                            m_deviceOriginX % stipple->GetWidth(),
                            m_deviceOriginY % stipple->GetHeight());
#endif
        }

        if ( m_brush.GetResourceHandle() )
        {
            HBRUSH b = 0;
            b = (HBRUSH) ::SelectObject(GetHdc(), (HBRUSH)m_brush.GetResourceHandle());
            if (!m_oldBrush)
                m_oldBrush = (WXHBRUSH) b;
        }
    }
}

void wxDC::SetBackground(const wxBrush& brush)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    m_backgroundBrush = brush;

    if (!m_backgroundBrush.Ok())
        return;

    if (m_canvas)
    {
        bool customColours = TRUE;
        // If we haven't specified wxUSER_COLOURS, don't allow the panel/dialog box to
        // change background colours from the control-panel specified colours.
        if (m_canvas->IsKindOf(CLASSINFO(wxWindow)) && ((m_canvas->GetWindowStyleFlag() & wxUSER_COLOURS) != wxUSER_COLOURS))
            customColours = FALSE;

        if (customColours)
        {
            if (m_backgroundBrush.GetStyle()==wxTRANSPARENT)
            {
                m_canvas->SetTransparent(TRUE);
            }
            else
            {
                // New behaviour, 10/2/99: setting the background brush of a DC
                // doesn't affect the window background colour. However,
                // I'm leaving in the transparency setting because it's needed by
                // various controls (e.g. wxStaticText) to determine whether to draw
                // transparently or not. TODO: maybe this should be a new function
                // wxWindow::SetTransparency(). Should that apply to the child itself, or the
                // parent?
                //        m_canvas->SetBackgroundColour(m_backgroundBrush.GetColour());
                m_canvas->SetTransparent(FALSE);
            }
        }
    }
    COLORREF new_color = m_backgroundBrush.GetColour().GetPixel();
    {
        (void)SetBkColor(GetHdc(), new_color);
    }
}

void wxDC::SetBackgroundMode(int mode)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    m_backgroundMode = mode;

    // SetBackgroundColour now only refers to text background
    // and m_backgroundMode is used there
}

void wxDC::SetLogicalFunction(int function)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    m_logicalFunction = function;

    SetRop(m_hDC);
}

void wxDC::SetRop(WXHDC dc)
{
    if ( !dc || m_logicalFunction < 0 )
        return;

    int rop;

    switch (m_logicalFunction)
    {
        case wxCLEAR:        rop = R2_BLACK;         break;
        case wxXOR:          rop = R2_XORPEN;        break;
        case wxINVERT:       rop = R2_NOT;           break;
        case wxOR_REVERSE:   rop = R2_MERGEPENNOT;   break;
        case wxAND_REVERSE:  rop = R2_MASKPENNOT;    break;
        case wxCOPY:         rop = R2_COPYPEN;       break;
        case wxAND:          rop = R2_MASKPEN;       break;
        case wxAND_INVERT:   rop = R2_MASKNOTPEN;    break;
        case wxNO_OP:        rop = R2_NOP;           break;
        case wxNOR:          rop = R2_NOTMERGEPEN;   break;
        case wxEQUIV:        rop = R2_NOTXORPEN;     break;
        case wxSRC_INVERT:   rop = R2_NOTCOPYPEN;    break;
        case wxOR_INVERT:    rop = R2_MERGENOTPEN;   break;
        case wxNAND:         rop = R2_NOTMASKPEN;    break;
        case wxOR:           rop = R2_MERGEPEN;      break;
        case wxSET:          rop = R2_WHITE;         break;

        default:
           wxFAIL_MSG( wxT("unsupported logical function") );
           return;
    }

    SetROP2(GetHdc(), rop);
}

bool wxDC::StartDoc(const wxString& WXUNUSED(message))
{
    // We might be previewing, so return TRUE to let it continue.
    return TRUE;
}

void wxDC::EndDoc()
{
}

void wxDC::StartPage()
{
}

void wxDC::EndPage()
{
}

// ---------------------------------------------------------------------------
// text metrics
// ---------------------------------------------------------------------------

wxCoord wxDC::GetCharHeight() const
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return 0;
#endif

    TEXTMETRIC lpTextMetric;

    GetTextMetrics(GetHdc(), &lpTextMetric);

    return lpTextMetric.tmHeight;
}

wxCoord wxDC::GetCharWidth() const
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return 0;
#endif

    TEXTMETRIC lpTextMetric;

    GetTextMetrics(GetHdc(), &lpTextMetric);

    return lpTextMetric.tmAveCharWidth;
}

void wxDC::DoGetTextExtent(const wxString& string, wxCoord *x, wxCoord *y,
                           wxCoord *descent, wxCoord *externalLeading,
                           wxFont *font) const
{
#ifdef __WXMICROWIN__
    if (!GetHDC())
    {
        if (x) *x = 0;
        if (y) *y = 0;
        if (descent) *descent = 0;
        if (externalLeading) *externalLeading = 0;
        return;
    }
#endif // __WXMICROWIN__

    HFONT hfontOld;
    if ( font )
    {
        wxASSERT_MSG( font->Ok(), _T("invalid font in wxDC::GetTextExtent") );

        hfontOld = (HFONT)::SelectObject(GetHdc(), GetHfontOf(*font));
    }
    else // don't change the font
    {
        hfontOld = 0;
    }

    SIZE sizeRect;
    TEXTMETRIC tm;

    GetTextExtentPoint(GetHdc(), string, string.length(), &sizeRect);
    GetTextMetrics(GetHdc(), &tm);

    if (x)
        *x = sizeRect.cx;
    if (y)
        *y = sizeRect.cy;
    if (descent)
        *descent = tm.tmDescent;
    if (externalLeading)
        *externalLeading = tm.tmExternalLeading;

    if ( hfontOld )
    {
        ::SelectObject(GetHdc(), hfontOld);
    }
}

void wxDC::SetMapMode(int mode)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    m_mappingMode = mode;

    if ( mode == wxMM_TEXT )
    {
        m_logicalScaleX =
        m_logicalScaleY = 1.0;
    }
    else // need to do some calculations
    {
        int pixel_width = ::GetDeviceCaps(GetHdc(), HORZRES),
            pixel_height = ::GetDeviceCaps(GetHdc(), VERTRES),
            mm_width = ::GetDeviceCaps(GetHdc(), HORZSIZE),
            mm_height = ::GetDeviceCaps(GetHdc(), VERTSIZE);

        if ( (mm_width == 0) || (mm_height == 0) )
        {
            // we can't calculate mm2pixels[XY] then!
            return;
        }

        double mm2pixelsX = pixel_width / mm_width,
               mm2pixelsY = pixel_height / mm_height;

        switch (mode)
        {
            case wxMM_TWIPS:
                m_logicalScaleX = twips2mm * mm2pixelsX;
                m_logicalScaleY = twips2mm * mm2pixelsY;
                break;

            case wxMM_POINTS:
                m_logicalScaleX = pt2mm * mm2pixelsX;
                m_logicalScaleY = pt2mm * mm2pixelsY;
                break;

            case wxMM_METRIC:
                m_logicalScaleX = mm2pixelsX;
                m_logicalScaleY = mm2pixelsY;
                break;

            case wxMM_LOMETRIC:
                m_logicalScaleX = mm2pixelsX / 10.0;
                m_logicalScaleY = mm2pixelsY / 10.0;
                break;

            default:
                wxFAIL_MSG( _T("unknown mapping mode in SetMapMode") );
        }
    }

    // VZ: it seems very wasteful to always use MM_ANISOTROPIC when in 99% of
    //     cases we could do with MM_TEXT and in the remaining 0.9% with
    //     MM_ISOTROPIC (TODO!)
    ::SetMapMode(GetHdc(), MM_ANISOTROPIC);

    int width = DeviceToLogicalXRel(VIEWPORT_EXTENT)*m_signX,
        height = DeviceToLogicalYRel(VIEWPORT_EXTENT)*m_signY;

    ::SetViewportExtEx(GetHdc(), VIEWPORT_EXTENT, VIEWPORT_EXTENT, NULL);
    ::SetWindowExtEx(GetHdc(), width, height, NULL);

    ::SetViewportOrgEx(GetHdc(), m_deviceOriginX, m_deviceOriginY, NULL);
    ::SetWindowOrgEx(GetHdc(), m_logicalOriginX, m_logicalOriginY, NULL);
}

void wxDC::SetUserScale(double x, double y)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    if ( x == m_userScaleX && y == m_userScaleY )
        return;

    m_userScaleX = x;
    m_userScaleY = y;

    SetMapMode(m_mappingMode);
}

void wxDC::SetAxisOrientation(bool xLeftRight, bool yBottomUp)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    int signX = xLeftRight ? 1 : -1,
        signY = yBottomUp ? -1 : 1;

    if ( signX != m_signX || signY != m_signY )
    {
        m_signX = signX;
        m_signY = signY;

        SetMapMode(m_mappingMode);
    }
}

void wxDC::SetSystemScale(double x, double y)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    if ( x == m_scaleX && y == m_scaleY )
        return;

    m_scaleX = x;
    m_scaleY = y;

    SetMapMode(m_mappingMode);
}

void wxDC::SetLogicalOrigin(wxCoord x, wxCoord y)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    if ( x == m_logicalOriginX && y == m_logicalOriginY )
        return;

    m_logicalOriginX = x;
    m_logicalOriginY = y;

    ::SetWindowOrgEx(GetHdc(), (int)m_logicalOriginX, (int)m_logicalOriginY, NULL);
}

void wxDC::SetDeviceOrigin(wxCoord x, wxCoord y)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    if ( x == m_deviceOriginX && y == m_deviceOriginY )
        return;

    m_deviceOriginX = x;
    m_deviceOriginY = y;

    ::SetViewportOrgEx(GetHdc(), (int)m_deviceOriginX, (int)m_deviceOriginY, NULL);
}

// ---------------------------------------------------------------------------
// coordinates transformations
// ---------------------------------------------------------------------------

wxCoord wxDCBase::DeviceToLogicalX(wxCoord x) const
{
    return DeviceToLogicalXRel(x - m_deviceOriginX)*m_signX + m_logicalOriginX;
}

wxCoord wxDCBase::DeviceToLogicalXRel(wxCoord x) const
{
    // axis orientation is not taken into account for conversion of a distance
    return (wxCoord)(x / (m_logicalScaleX*m_userScaleX*m_scaleX));
}

wxCoord wxDCBase::DeviceToLogicalY(wxCoord y) const
{
    return DeviceToLogicalYRel(y - m_deviceOriginY)*m_signY + m_logicalOriginY;
}

wxCoord wxDCBase::DeviceToLogicalYRel(wxCoord y) const
{
    // axis orientation is not taken into account for conversion of a distance
    return (wxCoord)( y / (m_logicalScaleY*m_userScaleY*m_scaleY));
}

wxCoord wxDCBase::LogicalToDeviceX(wxCoord x) const
{
    return LogicalToDeviceXRel(x - m_logicalOriginX)*m_signX + m_deviceOriginX;
}

wxCoord wxDCBase::LogicalToDeviceXRel(wxCoord x) const
{
    // axis orientation is not taken into account for conversion of a distance
    return (wxCoord) (x*m_logicalScaleX*m_userScaleX*m_scaleX);
}

wxCoord wxDCBase::LogicalToDeviceY(wxCoord y) const
{
    return LogicalToDeviceYRel(y - m_logicalOriginY)*m_signY + m_deviceOriginY;
}

wxCoord wxDCBase::LogicalToDeviceYRel(wxCoord y) const
{
    // axis orientation is not taken into account for conversion of a distance
    return (wxCoord) (y*m_logicalScaleY*m_userScaleY*m_scaleY);
}

// ---------------------------------------------------------------------------
// bit blit
// ---------------------------------------------------------------------------

bool wxDC::DoBlit(wxCoord xdest, wxCoord ydest,
                  wxCoord width, wxCoord height,
                  wxDC *source, wxCoord xsrc, wxCoord ysrc,
                  int rop, bool useMask,
                  wxCoord xsrcMask, wxCoord ysrcMask)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return FALSE;
#endif

    wxMask *mask = NULL;
    if ( useMask )
    {
        const wxBitmap& bmp = source->m_selectedBitmap;
        mask = bmp.GetMask();

        if ( !(bmp.Ok() && mask && mask->GetMaskBitmap()) )
        {
            // don't give assert here because this would break existing
            // programs - just silently ignore useMask parameter
            useMask = FALSE;
        }
    }

    if (xsrcMask == -1 && ysrcMask == -1)
    {
        xsrcMask = xsrc; ysrcMask = ysrc;
    }

    COLORREF old_textground = ::GetTextColor(GetHdc());
    COLORREF old_background = ::GetBkColor(GetHdc());
    if (m_textForegroundColour.Ok())
    {
        ::SetTextColor(GetHdc(), m_textForegroundColour.GetPixel() );
    }
    if (m_textBackgroundColour.Ok())
    {
        ::SetBkColor(GetHdc(), m_textBackgroundColour.GetPixel() );
    }

    DWORD dwRop = SRCCOPY;
    switch (rop)
    {
        case wxXOR:          dwRop = SRCINVERT;        break;
        case wxINVERT:       dwRop = DSTINVERT;        break;
        case wxOR_REVERSE:   dwRop = 0x00DD0228;       break;
        case wxAND_REVERSE:  dwRop = SRCERASE;         break;
        case wxCLEAR:        dwRop = BLACKNESS;        break;
        case wxSET:          dwRop = WHITENESS;        break;
        case wxOR_INVERT:    dwRop = MERGEPAINT;       break;
        case wxAND:          dwRop = SRCAND;           break;
        case wxOR:           dwRop = SRCPAINT;         break;
        case wxEQUIV:        dwRop = 0x00990066;       break;
        case wxNAND:         dwRop = 0x007700E6;       break;
        case wxAND_INVERT:   dwRop = 0x00220326;       break;
        case wxCOPY:         dwRop = SRCCOPY;          break;
        case wxNO_OP:        dwRop = DSTCOPY;          break;
        case wxSRC_INVERT:   dwRop = NOTSRCCOPY;       break;
        case wxNOR:          dwRop = NOTSRCCOPY;       break;
        default:
           wxFAIL_MSG( wxT("unsupported logical function") );
           return FALSE;
    }

    bool success = FALSE;

    if (useMask)
    {
#ifdef __WIN32__
        // we want the part of the image corresponding to the mask to be
        // transparent, so use "DSTCOPY" ROP for the mask points (the usual
        // meaning of fg and bg is inverted which corresponds to wxWin notion
        // of the mask which is also contrary to the Windows one)

        // On some systems, MaskBlt succeeds yet is much much slower
        // than the wxWindows fall-back implementation. So we need
        // to be able to switch this on and off at runtime.
#if wxUSE_SYSTEM_OPTIONS
        if (wxSystemOptions::GetOptionInt(wxT("no-maskblt")) == 0)
#endif
        {
           success = ::MaskBlt(GetHdc(), xdest, ydest, width, height,
                            GetHdcOf(*source), xsrc, ysrc,
                            (HBITMAP)mask->GetMaskBitmap(), xsrcMask, ysrcMask,
                            MAKEROP4(dwRop, DSTCOPY)) != 0;
        }

        if ( !success )
#endif // Win32
        {
            // Blit bitmap with mask
            HDC dc_mask ;
            HDC  dc_buffer ;
            HBITMAP buffer_bmap ;

#if wxUSE_DC_CACHEING
            // create a temp buffer bitmap and DCs to access it and the mask
            wxDCCacheEntry* dcCacheEntry1 = FindDCInCache(NULL, source->GetHDC());
            dc_mask = (HDC) dcCacheEntry1->m_dc;

            wxDCCacheEntry* dcCacheEntry2 = FindDCInCache(dcCacheEntry1, GetHDC());
            dc_buffer = (HDC) dcCacheEntry2->m_dc;

            wxDCCacheEntry* bitmapCacheEntry = FindBitmapInCache(GetHDC(),
                width, height);

            buffer_bmap = (HBITMAP) bitmapCacheEntry->m_bitmap;
#else // !wxUSE_DC_CACHEING
            // create a temp buffer bitmap and DCs to access it and the mask
            dc_mask = ::CreateCompatibleDC(GetHdcOf(*source));
            dc_buffer = ::CreateCompatibleDC(GetHdc());
            buffer_bmap = ::CreateCompatibleBitmap(GetHdc(), width, height);
#endif // wxUSE_DC_CACHEING/!wxUSE_DC_CACHEING
            HGDIOBJ hOldMaskBitmap = ::SelectObject(dc_mask, (HBITMAP) mask->GetMaskBitmap());
            HGDIOBJ hOldBufferBitmap = ::SelectObject(dc_buffer, buffer_bmap);

            // copy dest to buffer
            if ( !::BitBlt(dc_buffer, 0, 0, (int)width, (int)height,
                           GetHdc(), xdest, ydest, SRCCOPY) )
            {
                wxLogLastError(wxT("BitBlt"));
            }

            // copy src to buffer using selected raster op
            if ( !::BitBlt(dc_buffer, 0, 0, (int)width, (int)height,
                           GetHdcOf(*source), xsrc, ysrc, dwRop) )
            {
                wxLogLastError(wxT("BitBlt"));
            }

            // set masked area in buffer to BLACK (pixel value 0)
            COLORREF prevBkCol = ::SetBkColor(GetHdc(), RGB(255, 255, 255));
            COLORREF prevCol = ::SetTextColor(GetHdc(), RGB(0, 0, 0));
            if ( !::BitBlt(dc_buffer, 0, 0, (int)width, (int)height,
                           dc_mask, xsrcMask, ysrcMask, SRCAND) )
            {
                wxLogLastError(wxT("BitBlt"));
            }

            // set unmasked area in dest to BLACK
            ::SetBkColor(GetHdc(), RGB(0, 0, 0));
            ::SetTextColor(GetHdc(), RGB(255, 255, 255));
            if ( !::BitBlt(GetHdc(), xdest, ydest, (int)width, (int)height,
                           dc_mask, xsrcMask, ysrcMask, SRCAND) )
            {
                wxLogLastError(wxT("BitBlt"));
            }
            ::SetBkColor(GetHdc(), prevBkCol);   // restore colours to original values
            ::SetTextColor(GetHdc(), prevCol);

            // OR buffer to dest
            success = ::BitBlt(GetHdc(), xdest, ydest,
                               (int)width, (int)height,
                               dc_buffer, 0, 0, SRCPAINT) != 0;
            if ( !success )
            {
                wxLogLastError(wxT("BitBlt"));
            }

            // tidy up temporary DCs and bitmap
            ::SelectObject(dc_mask, hOldMaskBitmap);
            ::SelectObject(dc_buffer, hOldBufferBitmap);

#if !wxUSE_DC_CACHEING
            {
                ::DeleteDC(dc_mask);
                ::DeleteDC(dc_buffer);
                ::DeleteObject(buffer_bmap);
            }
#endif
        }
    }
    else // no mask, just BitBlt() it
    {
        success = ::BitBlt(GetHdc(), xdest, ydest,
                           (int)width, (int)height,
                           GetHdcOf(*source), xsrc, ysrc, dwRop) != 0;
        if ( !success )
        {
            wxLogLastError(wxT("BitBlt"));
        }
    }
    ::SetTextColor(GetHdc(), old_textground);
    ::SetBkColor(GetHdc(), old_background);

    return success;
}

void wxDC::DoGetSize(int *w, int *h) const
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    if ( w ) *w = ::GetDeviceCaps(GetHdc(), HORZRES);
    if ( h ) *h = ::GetDeviceCaps(GetHdc(), VERTRES);
}

void wxDC::DoGetSizeMM(int *w, int *h) const
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    // if we implement it in terms of DoGetSize() instead of directly using the
    // results returned by GetDeviceCaps(HORZ/VERTSIZE) as was done before, it
    // will also work for wxWindowDC and wxClientDC even though their size is
    // not the same as the total size of the screen
    int wPixels, hPixels;
    DoGetSize(&wPixels, &hPixels);

    if ( w )
    {
        int wTotal = ::GetDeviceCaps(GetHdc(), HORZRES);

        wxCHECK_RET( wTotal, _T("0 width device?") );

        *w = (wPixels * ::GetDeviceCaps(GetHdc(), HORZSIZE)) / wTotal;
    }

    if ( h )
    {
        int hTotal = ::GetDeviceCaps(GetHdc(), VERTRES);

        wxCHECK_RET( hTotal, _T("0 height device?") );

        *h = (hPixels * ::GetDeviceCaps(GetHdc(), VERTSIZE)) / hTotal;
    }
}

wxSize wxDC::GetPPI() const
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return wxSize();
#endif

    int x = ::GetDeviceCaps(GetHdc(), LOGPIXELSX);
    int y = ::GetDeviceCaps(GetHdc(), LOGPIXELSY);

    return wxSize(x, y);
}

// For use by wxWindows only, unless custom units are required.
void wxDC::SetLogicalScale(double x, double y)
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    m_logicalScaleX = x;
    m_logicalScaleY = y;
}

#if WXWIN_COMPATIBILITY
void wxDC::DoGetTextExtent(const wxString& string, float *x, float *y,
                         float *descent, float *externalLeading,
                         wxFont *theFont, bool use16bit) const
{
#ifdef __WXMICROWIN__
    if (!GetHDC()) return;
#endif

    wxCoord x1, y1, descent1, externalLeading1;
    GetTextExtent(string, & x1, & y1, & descent1, & externalLeading1, theFont, use16bit);
    *x = x1; *y = y1;
    if (descent)
        *descent = descent1;
    if (externalLeading)
        *externalLeading = externalLeading1;
}
#endif

#if wxUSE_DC_CACHEING

/*
 * This implementation is a bit ugly and uses the old-fashioned wxList class, so I will
 * improve it in due course, either using arrays, or simply storing pointers to one
 * entry for the bitmap, and two for the DCs. -- JACS
 */

wxList wxDC::sm_bitmapCache;
wxList wxDC::sm_dcCache;

wxDCCacheEntry::wxDCCacheEntry(WXHBITMAP hBitmap, int w, int h, int depth)
{
    m_bitmap = hBitmap;
    m_dc = 0;
    m_width = w;
    m_height = h;
    m_depth = depth;
}

wxDCCacheEntry::wxDCCacheEntry(WXHDC hDC, int depth)
{
    m_bitmap = 0;
    m_dc = hDC;
    m_width = 0;
    m_height = 0;
    m_depth = depth;
}

wxDCCacheEntry::~wxDCCacheEntry()
{
    if (m_bitmap)
        ::DeleteObject((HBITMAP) m_bitmap);
    if (m_dc)
        ::DeleteDC((HDC) m_dc);
}

wxDCCacheEntry* wxDC::FindBitmapInCache(WXHDC dc, int w, int h)
{
    int depth = ::GetDeviceCaps((HDC) dc, PLANES) * ::GetDeviceCaps((HDC) dc, BITSPIXEL);
    wxNode* node = sm_bitmapCache.First();
    while (node)
    {
        wxDCCacheEntry* entry = (wxDCCacheEntry*) node->Data();

        if (entry->m_depth == depth)
        {
            if (entry->m_width < w || entry->m_height < h)
            {
                ::DeleteObject((HBITMAP) entry->m_bitmap);
                entry->m_bitmap = (WXHBITMAP) ::CreateCompatibleBitmap((HDC) dc, w, h);
                if ( !entry->m_bitmap)
                {
                    wxLogLastError(wxT("CreateCompatibleBitmap"));
                }
                entry->m_width = w; entry->m_height = h;
                return entry;
            }
            return entry;
        }

        node = node->Next();
    }
    WXHBITMAP hBitmap = (WXHBITMAP) ::CreateCompatibleBitmap((HDC) dc, w, h);
    if ( !hBitmap)
    {
        wxLogLastError(wxT("CreateCompatibleBitmap"));
    }
    wxDCCacheEntry* entry = new wxDCCacheEntry(hBitmap, w, h, depth);
    AddToBitmapCache(entry);
    return entry;
}

wxDCCacheEntry* wxDC::FindDCInCache(wxDCCacheEntry* notThis, WXHDC dc)
{
    int depth = ::GetDeviceCaps((HDC) dc, PLANES) * ::GetDeviceCaps((HDC) dc, BITSPIXEL);
    wxNode* node = sm_dcCache.First();
    while (node)
    {
        wxDCCacheEntry* entry = (wxDCCacheEntry*) node->Data();

        // Don't return the same one as we already have
        if (!notThis || (notThis != entry))
        {
            if (entry->m_depth == depth)
            {
                return entry;
            }
        }

        node = node->Next();
    }
    WXHDC hDC = (WXHDC) ::CreateCompatibleDC((HDC) dc);
    if ( !hDC)
    {
        wxLogLastError(wxT("CreateCompatibleDC"));
    }
    wxDCCacheEntry* entry = new wxDCCacheEntry(hDC, depth);
    AddToDCCache(entry);
    return entry;
}

void wxDC::AddToBitmapCache(wxDCCacheEntry* entry)
{
    sm_bitmapCache.Append(entry);
}

void wxDC::AddToDCCache(wxDCCacheEntry* entry)
{
    sm_dcCache.Append(entry);
}

void wxDC::ClearCache()
{
    sm_dcCache.DeleteContents(TRUE);
    sm_dcCache.Clear();
    sm_dcCache.DeleteContents(FALSE);
    sm_bitmapCache.DeleteContents(TRUE);
    sm_bitmapCache.Clear();
    sm_bitmapCache.DeleteContents(FALSE);
}

// Clean up cache at app exit
class wxDCModule : public wxModule
{
public:
    virtual bool OnInit() { return TRUE; }
    virtual void OnExit() { wxDC::ClearCache(); }

private:
    DECLARE_DYNAMIC_CLASS(wxDCModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxDCModule, wxModule)

#endif
    // wxUSE_DC_CACHEING

