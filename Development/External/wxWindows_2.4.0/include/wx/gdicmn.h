/////////////////////////////////////////////////////////////////////////////
// Name:        gdicmn.h
// Purpose:     Common GDI classes, types and declarations
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: gdicmn.h,v 1.60.2.2 2002/11/09 00:24:59 VS Exp $
// Copyright:   (c)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GDICMNH__
#define _WX_GDICMNH__

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "gdicmn.h"
#endif

#include "wx/object.h"
#include "wx/list.h"
#include "wx/hash.h"
#include "wx/string.h"
#include "wx/setup.h"
#include "wx/colour.h"
#include "wx/font.h"

// ---------------------------------------------------------------------------
// forward declarations
// ---------------------------------------------------------------------------

class WXDLLEXPORT wxBitmap;
class WXDLLEXPORT wxBrush;
class WXDLLEXPORT wxColour;
class WXDLLEXPORT wxCursor;
class WXDLLEXPORT wxFont;
class WXDLLEXPORT wxIcon;
class WXDLLEXPORT wxPalette;
class WXDLLEXPORT wxPen;
class WXDLLEXPORT wxRegion;
class WXDLLEXPORT wxString;

// ---------------------------------------------------------------------------
// constants
// ---------------------------------------------------------------------------

// Bitmap flags
enum wxBitmapType
{
    wxBITMAP_TYPE_INVALID,          // should be == 0 for compatibility!
    wxBITMAP_TYPE_BMP,
    wxBITMAP_TYPE_BMP_RESOURCE,
    wxBITMAP_TYPE_RESOURCE = wxBITMAP_TYPE_BMP_RESOURCE,
    wxBITMAP_TYPE_ICO,
    wxBITMAP_TYPE_ICO_RESOURCE,
    wxBITMAP_TYPE_CUR,
    wxBITMAP_TYPE_CUR_RESOURCE,
    wxBITMAP_TYPE_XBM,
    wxBITMAP_TYPE_XBM_DATA,
    wxBITMAP_TYPE_XPM,
    wxBITMAP_TYPE_XPM_DATA,
    wxBITMAP_TYPE_TIF,
    wxBITMAP_TYPE_TIF_RESOURCE,
    wxBITMAP_TYPE_GIF,
    wxBITMAP_TYPE_GIF_RESOURCE,
    wxBITMAP_TYPE_PNG,
    wxBITMAP_TYPE_PNG_RESOURCE,
    wxBITMAP_TYPE_JPEG,
    wxBITMAP_TYPE_JPEG_RESOURCE,
    wxBITMAP_TYPE_PNM,
    wxBITMAP_TYPE_PNM_RESOURCE,
    wxBITMAP_TYPE_PCX,
    wxBITMAP_TYPE_PCX_RESOURCE,
    wxBITMAP_TYPE_PICT,
    wxBITMAP_TYPE_PICT_RESOURCE,
    wxBITMAP_TYPE_ICON,
    wxBITMAP_TYPE_ICON_RESOURCE,
    wxBITMAP_TYPE_ANI,
    wxBITMAP_TYPE_IFF,
    wxBITMAP_TYPE_MACCURSOR,
    wxBITMAP_TYPE_MACCURSOR_RESOURCE,
    wxBITMAP_TYPE_ANY = 50
};

// Standard cursors
enum wxStockCursor
{
    wxCURSOR_NONE,          // should be 0
    wxCURSOR_ARROW,
    wxCURSOR_RIGHT_ARROW,
    wxCURSOR_BULLSEYE,
    wxCURSOR_CHAR,
    wxCURSOR_CROSS,
    wxCURSOR_HAND,
    wxCURSOR_IBEAM,
    wxCURSOR_LEFT_BUTTON,
    wxCURSOR_MAGNIFIER,
    wxCURSOR_MIDDLE_BUTTON,
    wxCURSOR_NO_ENTRY,
    wxCURSOR_PAINT_BRUSH,
    wxCURSOR_PENCIL,
    wxCURSOR_POINT_LEFT,
    wxCURSOR_POINT_RIGHT,
    wxCURSOR_QUESTION_ARROW,
    wxCURSOR_RIGHT_BUTTON,
    wxCURSOR_SIZENESW,
    wxCURSOR_SIZENS,
    wxCURSOR_SIZENWSE,
    wxCURSOR_SIZEWE,
    wxCURSOR_SIZING,
    wxCURSOR_SPRAYCAN,
    wxCURSOR_WAIT,
    wxCURSOR_WATCH,
    wxCURSOR_BLANK,
#ifdef __WXGTK__
    wxCURSOR_DEFAULT, // standard X11 cursor
#endif
#ifdef __X__
    // Not yet implemented for Windows
    wxCURSOR_CROSS_REVERSE,
    wxCURSOR_DOUBLE_ARROW,
    wxCURSOR_BASED_ARROW_UP,
    wxCURSOR_BASED_ARROW_DOWN,
#endif // X11

    wxCURSOR_ARROWWAIT,

    wxCURSOR_MAX
};

#ifndef __WXGTK__
    #define wxCURSOR_DEFAULT wxCURSOR_ARROW
#endif

// ---------------------------------------------------------------------------
// macros
// ---------------------------------------------------------------------------

/* Useful macro for creating icons portably, for example:

    wxIcon *icon = new wxICON(mondrian);

  expands into:

    wxIcon *icon = new wxIcon("mondrian");      // On wxMSW
    wxIcon *icon = new wxIcon(mondrian_xpm);    // On wxGTK
 */

#ifdef __WXMSW__
    // Load from a resource
    #define wxICON(X) wxIcon(wxT(#X))
#elif defined(__WXPM__)
    // Load from a resource
    #define wxICON(X) wxIcon(wxT(#X))
#elif defined(__WXMGL__)
    // Initialize from an included XPM
    #define wxICON(X) wxIcon( (const char**) X##_xpm )
#elif defined(__WXGTK__)
    // Initialize from an included XPM
    #define wxICON(X) wxIcon( (const char**) X##_xpm )
#elif defined(__WXMAC__)
    // Initialize from an included XPM
    #define wxICON(X) wxIcon( (const char**) X##_xpm )
#elif defined(__WXMOTIF__)
    // Initialize from an included XPM
    #define wxICON(X) wxIcon( X##_xpm )
#elif defined(__WXX11__)
    // Initialize from an included XPM
    #define wxICON(X) wxIcon( X##_xpm )
#else
    // This will usually mean something on any platform
    #define wxICON(X) wxIcon(wxT(#X))
#endif // platform

/* Another macro: this one is for portable creation of bitmaps. We assume that
   under Unix bitmaps live in XPMs and under Windows they're in ressources.
 */

#if defined(__WXMSW__) || defined(__WXPM__)
    #define wxBITMAP(name) wxBitmap(wxT(#name), wxBITMAP_TYPE_RESOURCE)
#elif defined(__WXGTK__) || defined(__WXMOTIF__) || defined(__WXX11__) || defined(__WXMAC__) || defined(__WXMGL__)
    // Initialize from an included XPM
    #define wxBITMAP(name) wxBitmap( (const char**) name##_xpm )
#else // other platforms
    #define wxBITMAP(name) wxBitmap(name##_xpm, wxBITMAP_TYPE_XPM)
#endif // platform

// ===========================================================================
// classes
// ===========================================================================

// ---------------------------------------------------------------------------
// wxSize
// ---------------------------------------------------------------------------

class WXDLLEXPORT wxSize
{
public:
    // members are public for compatibility (don't use them directly,
    // especially that there names were chosen very unfortunately - they should
    // have been called width and height)
    int x, y;

    // constructors
    wxSize() : x(0), y(0) { }
    wxSize(int xx, int yy) : x(xx), y(yy) { }

    // no copy ctor or assignment operator - the defaults are ok

    bool operator==(const wxSize& sz) const { return x == sz.x && y == sz.y; }
    bool operator!=(const wxSize& sz) const { return x != sz.x || y != sz.y; }

    // FIXME are these really useful? If they're, we should have += &c as well
    wxSize operator+(const wxSize& sz) { return wxSize(x + sz.x, y + sz.y); }
    wxSize operator-(const wxSize& sz) { return wxSize(x - sz.x, y - sz.y); }

    // accessors
    void Set(int xx, int yy) { x = xx; y = yy; }
    void SetWidth(int w) { x = w; }
    void SetHeight(int h) { y = h; }

    int GetWidth() const { return x; }
    int GetHeight() const { return y; }

    // compatibility
    int GetX() const { return x; }
    int GetY() const { return y; }
};

// ---------------------------------------------------------------------------
// Point classes: with real or integer coordinates
// ---------------------------------------------------------------------------

class WXDLLEXPORT wxRealPoint
{
public:
    double x;
    double y;

    wxRealPoint() : x(0.0), y(0.0) { }
    wxRealPoint(double xx, double yy) : x(xx), y(yy) { }

    wxRealPoint operator+(const wxRealPoint& pt) const { return wxRealPoint(x + pt.x, y + pt.y); }
    wxRealPoint operator-(const wxRealPoint& pt) const { return wxRealPoint(x - pt.x, y - pt.y); }

    bool operator==(const wxRealPoint& pt) const { return x == pt.x && y == pt.y; }
    bool operator!=(const wxRealPoint& pt) const { return x != pt.x || y != pt.y; }
};

class WXDLLEXPORT wxPoint
{
public:
    int x, y;

    wxPoint() : x(0), y(0) { }
    wxPoint(int xx, int yy) : x(xx), y(yy) { }

    // no copy ctor or assignment operator - the defaults are ok

    // comparison
    bool operator==(const wxPoint& p) const { return x == p.x && y == p.y; }
    bool operator!=(const wxPoint& p) const { return !(*this == p); }

    // arithmetic operations (component wise)
    wxPoint operator+(const wxPoint& p) const { return wxPoint(x + p.x, y + p.y); }
    wxPoint operator-(const wxPoint& p) const { return wxPoint(x - p.x, y - p.y); }

    wxPoint& operator+=(const wxPoint& p) { x += p.x; y += p.y; return *this; }
    wxPoint& operator-=(const wxPoint& p) { x -= p.x; y -= p.y; return *this; }
};

#if WXWIN_COMPATIBILITY
    #define wxIntPoint wxPoint
    #define wxRectangle wxRect
#endif // WXWIN_COMPATIBILITY

// ---------------------------------------------------------------------------
// wxRect
// ---------------------------------------------------------------------------

class WXDLLEXPORT wxRect
{
public:
    wxRect()
        : x(0), y(0), width(0), height(0)
        { }
    wxRect(int xx, int yy, int ww, int hh)
        : x(xx), y(yy), width(ww), height(hh)
        { }
    wxRect(const wxPoint& topLeft, const wxPoint& bottomRight);
    wxRect(const wxPoint& pos, const wxSize& size);

    // default copy ctor and assignment operators ok

    int GetX() const { return x; }
    void SetX(int xx) { x = xx; }

    int GetY() const { return y; }
    void SetY(int yy) { y = yy; }

    int GetWidth() const { return width; }
    void SetWidth(int w) { width = w; }

    int GetHeight() const { return height; }
    void SetHeight(int h) { height = h; }

    wxPoint GetPosition() const { return wxPoint(x, y); }
    void SetPosition( const wxPoint &p ) { x = p.x; y = p.y; }

    wxSize GetSize() const { return wxSize(width, height); }
    void SetSize( const wxSize &s ) { width = s.GetWidth(); height = s.GetHeight(); }

    int GetLeft()   const { return x; }
    int GetTop()    const { return y; }
    int GetBottom() const { return y + height - 1; }
    int GetRight()  const { return x + width - 1; }

    void SetLeft(int left) { x = left; }
    void SetRight(int right) { width = right - x + 1; }
    void SetTop(int top) { y = top; }
    void SetBottom(int bottom) { height = bottom - y + 1; }

    // operations with rect
    wxRect& Inflate(wxCoord dx, wxCoord dy);
    wxRect& Inflate(wxCoord d) { return Inflate(d, d); }
    wxRect Inflate(wxCoord dx, wxCoord dy) const
    {
        wxRect r = *this;
        r.Inflate(dx, dy);
        return r;
    }

    wxRect& Deflate(wxCoord dx, wxCoord dy) { return Inflate(-dx, -dy); }
    wxRect& Deflate(wxCoord d) { return Inflate(-d); }
    wxRect Deflate(wxCoord dx, wxCoord dy) const
    {
        wxRect r = *this;
        r.Deflate(dx, dy);
        return r;
    }

    void Offset(wxCoord dx, wxCoord dy) { x += dx; y += dy; }
    void Offset(const wxPoint& pt) { Offset(pt.x, pt.y); }

    wxRect& Intersect(const wxRect& rect);
    wxRect Intersect(const wxRect& rect) const
    {
        wxRect r = *this;
        r.Intersect(rect);
        return r;
    }

    wxRect operator+(const wxRect& rect) const;
    wxRect& operator+=(const wxRect& rect);

    // compare rectangles
    bool operator==(const wxRect& rect) const;
    bool operator!=(const wxRect& rect) const { return !(*this == rect); }

    // return TRUE if the point is (not strcitly) inside the rect
    bool Inside(int x, int y) const;
    bool Inside(const wxPoint& pt) const { return Inside(pt.x, pt.y); }

    // return TRUE if the rectangles have a non empty intersection
    bool Intersects(const wxRect& rect) const;

public:
    int x, y, width, height;
};

// ---------------------------------------------------------------------------
// Management of pens, brushes and fonts
// ---------------------------------------------------------------------------

typedef wxInt8 wxDash;

class WXDLLEXPORT wxPenList : public wxList
{
    DECLARE_DYNAMIC_CLASS(wxPenList)

public:
    wxPenList() { }
    ~wxPenList();

    void AddPen(wxPen *pen);
    void RemovePen(wxPen *pen);
    wxPen *FindOrCreatePen(const wxColour& colour, int width, int style);
};

class WXDLLEXPORT wxBrushList : public wxList
{
    DECLARE_DYNAMIC_CLASS(wxBrushList)

public:
    wxBrushList() { }
    ~wxBrushList();

    void AddBrush(wxBrush *brush);
    void RemoveBrush(wxBrush *brush);
    wxBrush *FindOrCreateBrush(const wxColour& colour, int style);
};

WXDLLEXPORT_DATA(extern const wxChar*) wxEmptyString;

class WXDLLEXPORT wxFontList : public wxList
{
    DECLARE_DYNAMIC_CLASS(wxFontList)

public:
    wxFontList() { }
    ~wxFontList();

    void AddFont(wxFont *font);
    void RemoveFont(wxFont *font);
    wxFont *FindOrCreateFont(int pointSize, int family, int style, int weight,
                             bool underline = FALSE,
                             const wxString& face = wxEmptyString,
                             wxFontEncoding encoding = wxFONTENCODING_DEFAULT);
};

class WXDLLEXPORT wxColourDatabase : public wxList
{
    DECLARE_CLASS(wxColourDatabase)

public:
    wxColourDatabase(int type);
    ~wxColourDatabase() ;

    // Not const because it may add a name to the database
    wxColour *FindColour(const wxString& colour) ;
    wxString FindName(const wxColour& colour) const;
    void Initialize();
#ifdef __WXPM__
    // PM keeps its own type of colour table
    long*                           m_palTable;
    size_t                          m_nSize;
#endif
};

class WXDLLEXPORT wxBitmapList : public wxList
{
    DECLARE_DYNAMIC_CLASS(wxBitmapList)

public:
    wxBitmapList();
    ~wxBitmapList();

    void AddBitmap(wxBitmap *bitmap);
    void RemoveBitmap(wxBitmap *bitmap);
};

class WXDLLEXPORT wxResourceCache: public wxList
{
public:
    wxResourceCache() { }
    wxResourceCache(const unsigned int keyType) : wxList(keyType) { }
    ~wxResourceCache();

private:
    DECLARE_DYNAMIC_CLASS(wxResourceCache)
};

// ---------------------------------------------------------------------------
// global variables
// ---------------------------------------------------------------------------

// Lists of GDI objects
WXDLLEXPORT_DATA(extern wxPenList*)   wxThePenList;
WXDLLEXPORT_DATA(extern wxBrushList*)   wxTheBrushList;
WXDLLEXPORT_DATA(extern wxFontList*)    wxTheFontList;
WXDLLEXPORT_DATA(extern wxBitmapList*)  wxTheBitmapList;

// Stock objects
WXDLLEXPORT_DATA(extern wxFont*)      wxNORMAL_FONT;
WXDLLEXPORT_DATA(extern wxFont*)      wxSMALL_FONT;
WXDLLEXPORT_DATA(extern wxFont*)      wxITALIC_FONT;
WXDLLEXPORT_DATA(extern wxFont*)      wxSWISS_FONT;

WXDLLEXPORT_DATA(extern wxPen*)      wxRED_PEN;
WXDLLEXPORT_DATA(extern wxPen*)      wxCYAN_PEN;
WXDLLEXPORT_DATA(extern wxPen*)      wxGREEN_PEN;
WXDLLEXPORT_DATA(extern wxPen*)      wxBLACK_PEN;
WXDLLEXPORT_DATA(extern wxPen*)      wxWHITE_PEN;
WXDLLEXPORT_DATA(extern wxPen*)      wxTRANSPARENT_PEN;
WXDLLEXPORT_DATA(extern wxPen*)      wxBLACK_DASHED_PEN;
WXDLLEXPORT_DATA(extern wxPen*)      wxGREY_PEN;
WXDLLEXPORT_DATA(extern wxPen*)      wxMEDIUM_GREY_PEN;
WXDLLEXPORT_DATA(extern wxPen*)      wxLIGHT_GREY_PEN;

WXDLLEXPORT_DATA(extern wxBrush*)    wxBLUE_BRUSH;
WXDLLEXPORT_DATA(extern wxBrush*)    wxGREEN_BRUSH;
WXDLLEXPORT_DATA(extern wxBrush*)    wxWHITE_BRUSH;
WXDLLEXPORT_DATA(extern wxBrush*)    wxBLACK_BRUSH;
WXDLLEXPORT_DATA(extern wxBrush*)    wxGREY_BRUSH;
WXDLLEXPORT_DATA(extern wxBrush*)    wxMEDIUM_GREY_BRUSH;
WXDLLEXPORT_DATA(extern wxBrush*)    wxLIGHT_GREY_BRUSH;
WXDLLEXPORT_DATA(extern wxBrush*)    wxTRANSPARENT_BRUSH;
WXDLLEXPORT_DATA(extern wxBrush*)    wxCYAN_BRUSH;
WXDLLEXPORT_DATA(extern wxBrush*)    wxRED_BRUSH;

WXDLLEXPORT_DATA(extern wxColour*)    wxBLACK;
WXDLLEXPORT_DATA(extern wxColour*)    wxWHITE;
WXDLLEXPORT_DATA(extern wxColour*)    wxRED;
WXDLLEXPORT_DATA(extern wxColour*)    wxBLUE;
WXDLLEXPORT_DATA(extern wxColour*)    wxGREEN;
WXDLLEXPORT_DATA(extern wxColour*)    wxCYAN;
WXDLLEXPORT_DATA(extern wxColour*)    wxLIGHT_GREY;

// 'Null' objects
WXDLLEXPORT_DATA(extern wxBitmap)     wxNullBitmap;
WXDLLEXPORT_DATA(extern wxIcon)       wxNullIcon;
WXDLLEXPORT_DATA(extern wxCursor)     wxNullCursor;
WXDLLEXPORT_DATA(extern wxPen)        wxNullPen;
WXDLLEXPORT_DATA(extern wxBrush)      wxNullBrush;
WXDLLEXPORT_DATA(extern wxPalette)     wxNullPalette;
WXDLLEXPORT_DATA(extern wxFont)       wxNullFont;
WXDLLEXPORT_DATA(extern wxColour)     wxNullColour;

// Stock cursors types
WXDLLEXPORT_DATA(extern wxCursor*)    wxSTANDARD_CURSOR;
WXDLLEXPORT_DATA(extern wxCursor*)    wxHOURGLASS_CURSOR;
WXDLLEXPORT_DATA(extern wxCursor*)    wxCROSS_CURSOR;

WXDLLEXPORT_DATA(extern wxColourDatabase*)  wxTheColourDatabase;

WXDLLEXPORT_DATA(extern const wxChar*) wxPanelNameStr;

WXDLLEXPORT_DATA(extern const wxSize) wxDefaultSize;
WXDLLEXPORT_DATA(extern const wxPoint) wxDefaultPosition;

// The list of objects which should be deleted
WXDLLEXPORT_DATA(extern wxList) wxPendingDelete;

// ---------------------------------------------------------------------------
// global functions
// ---------------------------------------------------------------------------

// resource management
extern void WXDLLEXPORT wxInitializeStockObjects();
extern void WXDLLEXPORT wxInitializeStockLists();
extern void WXDLLEXPORT wxDeleteStockObjects();
extern void WXDLLEXPORT wxDeleteStockLists();

// is the display colour (or monochrome)?
extern bool WXDLLEXPORT wxColourDisplay();

// Returns depth of screen
extern int WXDLLEXPORT wxDisplayDepth();
#define wxGetDisplayDepth wxDisplayDepth

// get the display size
extern void WXDLLEXPORT wxDisplaySize(int *width, int *height);
extern wxSize WXDLLEXPORT wxGetDisplaySize();
extern void WXDLLEXPORT wxDisplaySizeMM(int *width, int *height);
extern wxSize WXDLLEXPORT wxGetDisplaySizeMM();

// Get position and size of the display workarea
extern void WXDLLEXPORT wxClientDisplayRect(int *x, int *y, int *width, int *height);
extern wxRect WXDLLEXPORT wxGetClientDisplayRect();

// set global cursor
extern void WXDLLEXPORT wxSetCursor(const wxCursor& cursor);

#endif
    // _WX_GDICMNH__
