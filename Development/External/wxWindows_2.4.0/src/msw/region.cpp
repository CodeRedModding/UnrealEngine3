/////////////////////////////////////////////////////////////////////////////
// Name:      msw/region.cpp
// Purpose:   wxRegion implementation using Win32 API
// Author:    Markus Holzem, Vadim Zeitlin
// Modified by:
// Created:   Fri Oct 24 10:46:34 MET 1997
// RCS-ID:    $Id: region.cpp,v 1.21.2.2 2002/09/24 09:29:31 JS Exp $
// Copyright: (c) 1997-2002 wxWindows team
// Licence:   wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "region.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#include "wx/region.h"
#include "wx/gdicmn.h"

#include "wx/msw/private.h"

IMPLEMENT_DYNAMIC_CLASS(wxRegion, wxGDIObject)
IMPLEMENT_DYNAMIC_CLASS(wxRegionIterator, wxObject)

// ----------------------------------------------------------------------------
// wxRegionRefData implementation
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxRegionRefData : public wxGDIRefData
{
public:
    wxRegionRefData()
    {
        m_region = 0;
    }

    wxRegionRefData(const wxRegionRefData& data)
    {
#if defined(__WIN32__) && !defined(__WXMICROWIN__)
        DWORD noBytes = ::GetRegionData(data.m_region, 0, NULL);
        RGNDATA *rgnData = (RGNDATA*) new char[noBytes];
        ::GetRegionData(data.m_region, noBytes, rgnData);
        m_region = ::ExtCreateRegion(NULL, noBytes, rgnData);
        delete[] (char*) rgnData;
#else
        RECT rect;
        ::GetRgnBox(data.m_region, &rect);
        m_region = ::CreateRectRgnIndirect(&rect);
#endif
    }

    virtual ~wxRegionRefData()
    {
        ::DeleteObject(m_region);
        m_region = 0;
    }

    HRGN m_region;
};

#define M_REGION (((wxRegionRefData*)m_refData)->m_region)
#define M_REGION_OF(rgn) (((wxRegionRefData*)(rgn.m_refData))->m_region)

// ============================================================================
// wxRegion implementation
// ============================================================================

// ----------------------------------------------------------------------------
// ctors and dtor
// ----------------------------------------------------------------------------

wxRegion::wxRegion()
{
    m_refData = (wxRegionRefData *)NULL;
}

wxRegion::wxRegion(WXHRGN hRegion)
{
    m_refData = new wxRegionRefData;
    M_REGION = (HRGN) hRegion;
}

wxRegion::wxRegion(wxCoord x, wxCoord y, wxCoord w, wxCoord h)
{
    m_refData = new wxRegionRefData;
    M_REGION = ::CreateRectRgn(x, y, x + w, y + h);
}

wxRegion::wxRegion(const wxPoint& topLeft, const wxPoint& bottomRight)
{
    m_refData = new wxRegionRefData;
    M_REGION = ::CreateRectRgn(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);
}

wxRegion::wxRegion(const wxRect& rect)
{
    m_refData = new wxRegionRefData;
    M_REGION = ::CreateRectRgn(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);
}

wxRegion::wxRegion(size_t n, const wxPoint *points, int fillStyle)
{
#ifdef __WXMICROWIN__
    m_refData = NULL;
    M_REGION = NULL;
#else
    m_refData = new wxRegionRefData;
    M_REGION = ::CreatePolygonRgn
               (
                    (POINT*)points,
                    n,
                    fillStyle == wxODDEVEN_RULE ? ALTERNATE : WINDING
               );
#endif
}

wxRegion::~wxRegion()
{
    // m_refData unrefed in ~wxObject
}

wxObjectRefData *wxRegion::CreateRefData() const
{
    return new wxRegionRefData;
}

wxObjectRefData *wxRegion::CloneRefData(const wxObjectRefData *data) const
{
    return new wxRegionRefData(*(wxRegionRefData *)data);
}

// ----------------------------------------------------------------------------
// wxRegion operations
// ----------------------------------------------------------------------------

// Clear current region
void wxRegion::Clear()
{
    UnRef();
}

bool wxRegion::Offset(wxCoord x, wxCoord y)
{
    wxCHECK_MSG( M_REGION, FALSE, _T("invalid wxRegion") );

    if ( !x && !y )
    {
        // nothing to do
        return TRUE;
    }

    AllocExclusive();

    if ( ::OffsetRgn(GetHrgn(), x, y) == ERROR )
    {
        wxLogLastError(_T("OffsetRgn"));

        return FALSE;
    }

    return TRUE;
}

// combine another region with this one
bool wxRegion::Combine(const wxRegion& rgn, wxRegionOp op)
{
    // we can't use the API functions if we don't have a valid region handle
    if ( !m_refData )
    {
        // combining with an empty/invalid region works differently depending
        // on the operation
        switch ( op )
        {
            case wxRGN_COPY:
            case wxRGN_OR:
            case wxRGN_XOR:
                *this = rgn;
                break;

            default:
                wxFAIL_MSG( _T("unknown region operation") );
                // fall through

            case wxRGN_AND:
            case wxRGN_DIFF:
                // leave empty/invalid
                return FALSE;
        }
    }
    else // we have a valid region
    {
        AllocExclusive();

        int mode;
        switch ( op )
        {
            case wxRGN_AND:
                mode = RGN_AND;
                break;

            case wxRGN_OR:
                mode = RGN_OR;
                break;

            case wxRGN_XOR:
                mode = RGN_XOR;
                break;

            case wxRGN_DIFF:
                mode = RGN_DIFF;
                break;

            default:
                wxFAIL_MSG( _T("unknown region operation") );
                // fall through

            case wxRGN_COPY:
                mode = RGN_COPY;
                break;
        }

        if ( ::CombineRgn(M_REGION, M_REGION, M_REGION_OF(rgn), mode) == ERROR )
        {
            wxLogLastError(_T("CombineRgn"));

            return FALSE;
        }
    }

    return TRUE;
}

// Combine rectangle (x, y, w, h) with this.
bool wxRegion::Combine(wxCoord x, wxCoord y,
                       wxCoord width, wxCoord height,
                       wxRegionOp op)
{
    return Combine(wxRegion(x, y, width, height), op);
}

bool wxRegion::Combine(const wxRect& rect, wxRegionOp op)
{
    return Combine(rect.GetLeft(), rect.GetTop(),
                   rect.GetWidth(), rect.GetHeight(), op);
}

// ----------------------------------------------------------------------------
// wxRegion bounding box
// ----------------------------------------------------------------------------

// Outer bounds of region
void wxRegion::GetBox(wxCoord& x, wxCoord& y, wxCoord&w, wxCoord &h) const
{
    if (m_refData)
    {
        RECT rect;
        ::GetRgnBox(M_REGION, & rect);
        x = rect.left;
        y = rect.top;
        w = rect.right - rect.left;
        h = rect.bottom - rect.top;
    }
    else
    {
        x = y = w = h = 0;
    }
}

wxRect wxRegion::GetBox() const
{
    wxCoord x, y, w, h;
    GetBox(x, y, w, h);
    return wxRect(x, y, w, h);
}

// Is region empty?
bool wxRegion::Empty() const
{
    wxCoord x, y, w, h;
    GetBox(x, y, w, h);

    return (w == 0) && (h == 0);
}

// ----------------------------------------------------------------------------
// wxRegion hit testing
// ----------------------------------------------------------------------------

// Does the region contain the point (x,y)?
wxRegionContain wxRegion::Contains(wxCoord x, wxCoord y) const
{
    if (!m_refData)
        return wxOutRegion;

    return ::PtInRegion(M_REGION, (int) x, (int) y) ? wxInRegion : wxOutRegion;
}

// Does the region contain the point pt?
wxRegionContain wxRegion::Contains(const wxPoint& pt) const
{
    return Contains(pt.x, pt.y);
}

// Does the region contain the rectangle (x, y, w, h)?
wxRegionContain wxRegion::Contains(wxCoord x, wxCoord y,
                                   wxCoord w, wxCoord h) const
{
    if (!m_refData)
        return wxOutRegion;

    RECT rect;
    rect.left = x;
    rect.top = y;
    rect.right = x + w;
    rect.bottom = y + h;

    return ::RectInRegion(M_REGION, &rect) ? wxInRegion : wxOutRegion;
}

// Does the region contain the rectangle rect
wxRegionContain wxRegion::Contains(const wxRect& rect) const
{
    return Contains(rect.x, rect.y, rect.width, rect.height);
}

// Get internal region handle
WXHRGN wxRegion::GetHRGN() const
{
    return (WXHRGN)(m_refData ? M_REGION : 0);
}

// ============================================================================
// wxRegionIterator implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxRegionIterator ctors/dtor
// ----------------------------------------------------------------------------

void wxRegionIterator::Init()
{
    m_current =
    m_numRects = 0;

    m_rects = NULL;
}

wxRegionIterator::~wxRegionIterator()
{
    delete [] m_rects;
}

// Initialize iterator for region
wxRegionIterator::wxRegionIterator(const wxRegion& region)
{
    m_rects = NULL;

    Reset(region);
}

wxRegionIterator& wxRegionIterator::operator=(const wxRegionIterator& ri)
{
    delete [] m_rects;

    m_current = ri.m_current;
    m_numRects = ri.m_numRects;
    if ( m_numRects )
    {
        m_rects = new wxRect[m_numRects];
        for ( long n = 0; n < m_numRects; n++ )
            m_rects[n] = ri.m_rects[n];
    }
    else
    {
        m_rects = NULL;
    }

    return *this;
}

// ----------------------------------------------------------------------------
// wxRegionIterator operations
// ----------------------------------------------------------------------------

// Reset iterator for a new region.
void wxRegionIterator::Reset(const wxRegion& region)
{
    m_current = 0;
    m_region = region;

    if (m_rects)
    {
        delete[] m_rects;

        m_rects = NULL;
    }

    if (m_region.Empty())
        m_numRects = 0;
    else
    {
#if defined(__WIN32__)
        DWORD noBytes = ::GetRegionData(((wxRegionRefData*)region.m_refData)->m_region, 0, NULL);
        RGNDATA *rgnData = (RGNDATA*) new char[noBytes];
        ::GetRegionData(((wxRegionRefData*)region.m_refData)->m_region, noBytes, rgnData);

        RGNDATAHEADER* header = (RGNDATAHEADER*) rgnData;

        m_rects = new wxRect[header->nCount];

        RECT* rect = (RECT*) ((char*)rgnData + sizeof(RGNDATAHEADER));
        size_t i;
        for (i = 0; i < header->nCount; i++)
        {
            m_rects[i] = wxRect(rect->left, rect->top,
                                 rect->right - rect->left, rect->bottom - rect->top);
            rect ++; // Advances pointer by sizeof(RECT)
        }

        m_numRects = header->nCount;

        delete[] (char*) rgnData;
#else // Win16
        RECT rect;
        ::GetRgnBox(((wxRegionRefData*)region.m_refData)->m_region, &rect);
        m_rects = new wxRect[1];
        m_rects[0].x = rect.left;
        m_rects[0].y = rect.top;
        m_rects[0].width = rect.right - rect.left;
        m_rects[0].height = rect.bottom - rect.top;

        m_numRects = 1;
#endif // Win32/16
    }
}

wxRegionIterator& wxRegionIterator::operator++()
{
    if (m_current < m_numRects)
        ++m_current;

    return *this;
}

wxRegionIterator wxRegionIterator::operator ++ (int)
{
    wxRegionIterator tmp = *this;
    if (m_current < m_numRects)
        ++m_current;

    return tmp;
}

// ----------------------------------------------------------------------------
// wxRegionIterator accessors
// ----------------------------------------------------------------------------

wxCoord wxRegionIterator::GetX() const
{
    wxCHECK_MSG( m_current < m_numRects, 0, _T("invalid wxRegionIterator") );

    return m_rects[m_current].x;
}

wxCoord wxRegionIterator::GetY() const
{
    wxCHECK_MSG( m_current < m_numRects, 0, _T("invalid wxRegionIterator") );

    return m_rects[m_current].y;
}

wxCoord wxRegionIterator::GetW() const
{
    wxCHECK_MSG( m_current < m_numRects, 0, _T("invalid wxRegionIterator") );

    return m_rects[m_current].width;
}

wxCoord wxRegionIterator::GetH() const
{
    wxCHECK_MSG( m_current < m_numRects, 0, _T("invalid wxRegionIterator") );

    return m_rects[m_current].height;
}

