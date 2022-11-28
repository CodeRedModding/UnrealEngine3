/////////////////////////////////////////////////////////////////////////////
// Name:        generic/statusbr.cpp
// Purpose:     wxStatusBarGeneric class implementation
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: statusbr.cpp,v 1.47.2.3 2002/11/22 07:13:39 GD Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "statusbr.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#if wxUSE_STATUSBAR

#ifndef WX_PRECOMP
#include "wx/setup.h"
#include "wx/frame.h"
#include "wx/settings.h"
#include "wx/dcclient.h"
#endif

#ifdef __WXGTK20__
#include "wx/gtk/private.h"
#include "wx/gtk/win_gtk.h"
#endif

#include "wx/statusbr.h"

// we only have to do it here when we use wxStatusBarGeneric in addition to the
// standard wxStatusBar class, if wxStatusBarGeneric is the same as
// wxStatusBar, then the corresponding IMPLEMENT_DYNAMIC_CLASS is already in
// common/statbar.cpp
#if defined(__WXMAC__) || \
    (defined(wxUSE_NATIVE_STATUSBAR) && wxUSE_NATIVE_STATUSBAR)
    #include "wx/generic/statusbr.h"

    IMPLEMENT_DYNAMIC_CLASS(wxStatusBarGeneric, wxWindow)
#endif // wxUSE_NATIVE_STATUSBAR

BEGIN_EVENT_TABLE(wxStatusBarGeneric, wxWindow)
    EVT_PAINT(wxStatusBarGeneric::OnPaint)
    EVT_LEFT_DOWN(wxStatusBarGeneric::OnLeftDown)
    EVT_RIGHT_DOWN(wxStatusBarGeneric::OnRightDown)
    EVT_SYS_COLOUR_CHANGED(wxStatusBarGeneric::OnSysColourChanged)
END_EVENT_TABLE()

// Default status border dimensions
#define         wxTHICK_LINE_BORDER 2
#define         wxTHICK_LINE_WIDTH  1

void wxStatusBarGeneric::Init()
{
  m_borderX = wxTHICK_LINE_BORDER;
  m_borderY = wxTHICK_LINE_BORDER;
}

wxStatusBarGeneric::~wxStatusBarGeneric()
{
    // VZ: what is this for? please comment...
#ifdef __WXMSW__
    SetFont(wxNullFont);
#endif // MSW
}

bool wxStatusBarGeneric::Create(wxWindow *parent,
                                wxWindowID id,
                                long style,
                                const wxString& name)
{
  if ( !wxWindow::Create(parent, id,
                         wxDefaultPosition, wxDefaultSize,
                         style | wxTAB_TRAVERSAL, name) )
      return FALSE;

  // The status bar should have a themed background
  SetThemeEnabled( TRUE );

  // Don't wish this to be found as a child
#ifndef __WXMAC__
  parent->GetChildren().DeleteObject(this);
#endif
  InitColours();

  SetFont(m_defaultStatusBarFont);

  // Set the height according to the font and the border size
  wxClientDC dc(this);
  dc.SetFont(GetFont());

  wxCoord y;
  dc.GetTextExtent(_T("X"), NULL, &y );

  int height = (int)( (11*y)/10 + 2*GetBorderY());

  SetSize(-1, -1, -1, height);

  return TRUE;
}

void wxStatusBarGeneric::SetFieldsCount(int number, const int *widths)
{
    wxASSERT_MSG( number >= 0, _T("negative number of fields in wxStatusBar?") );

    int i;
    for(i = m_nFields; i < number; ++i)
        m_statusStrings.Add( wxEmptyString );

    for (i = m_nFields - 1; i >= number; --i)
        m_statusStrings.RemoveAt(i);

    m_nFields = number;

    wxASSERT_MSG( m_nFields == (int)m_statusStrings.GetCount(),
                  _T("This really should never happen, can we do away with m_nFields here?") );

    SetStatusWidths(number, widths);
}

void wxStatusBarGeneric::SetStatusText(const wxString& text, int number)
{
    wxCHECK_RET( (number >= 0) && (number < m_nFields),
                 _T("invalid status bar field index") );

    m_statusStrings[number] = text;

    wxRect rect;
    GetFieldRect(number, rect);

    Refresh( TRUE, &rect );
}

wxString wxStatusBarGeneric::GetStatusText(int n) const
{
    wxCHECK_MSG( (n >= 0) && (n < m_nFields), wxEmptyString,
                 _T("invalid status bar field index") );

    return m_statusStrings[n];
}

void wxStatusBarGeneric::SetStatusWidths(int n, const int widths_field[])
{
    // only set status widths, when n == number of statuswindows
    wxCHECK_RET( n == m_nFields, _T("status bar field count mismatch") );

    // delete the old widths in any case - this function may be used to reset
    // the widths to the default (all equal)
    // MBN: this is incompatible with at least wxMSW and wxMAC and not
    //      documented, but let's keep it for now
    ReinitWidths();

    // forget the old cached pixel widths
    m_widthsAbs.Empty();

    if ( !widths_field )
    {
        // not an error, see the comment above
        Refresh();
        return;
    }

    wxStatusBarBase::SetStatusWidths(n, widths_field);
}

void wxStatusBarGeneric::OnPaint(wxPaintEvent& WXUNUSED(event) )
{
    wxPaintDC dc(this);

#ifdef __WXGTK20__
    // Draw grip first
    if (HasFlag( wxST_SIZEGRIP ))
    {
        int width, height;
        GetClientSize(&width, &height);
        
        gtk_paint_resize_grip( m_widget->style,
                               GTK_PIZZA(m_wxwindow)->bin_window,
                               (GtkStateType) GTK_WIDGET_STATE (m_widget),
                               NULL,
                               m_widget,
                               "statusbar",
                               GDK_WINDOW_EDGE_SOUTH_EAST,
                               width-height-2, 1, height-2, height-3 );
        
    }
#endif

  if (GetFont().Ok())
        dc.SetFont(GetFont());
    
  dc.SetBackgroundMode(wxTRANSPARENT);

#ifdef __WXPM__
  wxColour vColor;

  vColor = wxSystemSettings::GetColour(wxSYS_COLOUR_MENUBAR);
  ::WinFillRect(dc.m_hPS, &dc.m_vRclPaint, vColor.GetPixel());
#endif

  for (int i = 0; i < m_nFields; i ++)
        DrawField(dc, i);
}

void wxStatusBarGeneric::DrawFieldText(wxDC& dc, int i)
{
  int leftMargin = 2;

  wxRect rect;
  GetFieldRect(i, rect);

  wxString text(GetStatusText(i));

  long x, y;

  dc.GetTextExtent(text, &x, &y);

  int xpos = rect.x + leftMargin;
  int ypos = (int) (((rect.height - y) / 2 ) + rect.y + 0.5) ;

#if defined( __WXGTK__ ) || defined(__WXMAC__)
  xpos++;
  ypos++;
#endif

  dc.SetClippingRegion(rect.x, rect.y, rect.width, rect.height);

  dc.DrawText(text, xpos, ypos);

  dc.DestroyClippingRegion();
}

void wxStatusBarGeneric::DrawField(wxDC& dc, int i)
{
    wxRect rect;
    GetFieldRect(i, rect);

    // Draw border
    // Have grey background, plus 3-d border -
    // One black rectangle.
    // Inside this, left and top sides - dark grey. Bottom and right -
    // white.

    dc.SetPen(m_hilightPen);

#ifndef __WXPM__

    // Right and bottom white lines
    dc.DrawLine(rect.x + rect.width, rect.y,
                rect.x + rect.width, rect.y + rect.height);
    dc.DrawLine(rect.x + rect.width, rect.y + rect.height,
                rect.x, rect.y + rect.height);

    dc.SetPen(m_mediumShadowPen);

    // Left and top grey lines
    dc.DrawLine(rect.x, rect.y + rect.height,
           rect.x, rect.y);
    dc.DrawLine(rect.x, rect.y,
        rect.x + rect.width, rect.y);
#else

    dc.DrawLine(rect.x + rect.width, rect.height + 2,
                rect.x, rect.height + 2);
    dc.DrawLine(rect.x + rect.width, rect.y,
                rect.x + rect.width, rect.y + rect.height);

    dc.SetPen(m_mediumShadowPen);
    dc.DrawLine(rect.x, rect.y,
                rect.x + rect.width, rect.y);
    dc.DrawLine(rect.x, rect.y + rect.height,
                rect.x, rect.y);

#endif

    DrawFieldText(dc, i);
}

  // Get the position and size of the field's internal bounding rectangle
bool wxStatusBarGeneric::GetFieldRect(int n, wxRect& rect) const
{
    wxCHECK_MSG( (n >= 0) && (n < m_nFields), FALSE,
                 _T("invalid status bar field index") );

    // FIXME: workarounds for OS/2 bugs have nothing to do here (VZ)
    int width, height;
#ifdef __WXPM__
    GetSize(&width, &height);
#else
    GetClientSize(&width, &height);
#endif

    // we cache m_widthsAbs between calls and recompute it if client
    // width has changed (or when it is initially empty)
    if ( m_widthsAbs.IsEmpty() || (m_lastClientWidth != width) )
    {
        wxConstCast(this, wxStatusBarGeneric)->
            m_widthsAbs = CalculateAbsWidths(width);
        // remember last width for which we have recomputed the widths in pixels
        wxConstCast(this, wxStatusBarGeneric)->
            m_lastClientWidth = width;
    }

    rect.x = 0;
    for ( int i = 0; i < n; i++ )
    {
        rect.x += m_widthsAbs[i];
    }

    rect.x += m_borderX;
    rect.y = m_borderY;

    rect.width = m_widthsAbs[n] - 2*m_borderX;
    rect.height = height - 2*m_borderY;

    return TRUE;
}

// Initialize colours
void wxStatusBarGeneric::InitColours()
{
    // Shadow colours
#if defined(__WIN95__)
    wxColour mediumShadowColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW));
    m_mediumShadowPen = wxPen(mediumShadowColour, 1, wxSOLID);

    wxColour hilightColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DHILIGHT));
    m_hilightPen = wxPen(hilightColour, 1, wxSOLID);
#elif defined(__WXPM__)
    m_mediumShadowPen = wxPen(wxColour(127, 127, 127), 1, wxSOLID);
    m_hilightPen = wxPen("WHITE", 1, wxSOLID);

    wxColour                        vColour;

    vColour.Set(wxString("LIGHT GREY"));
    SetBackgroundColour(vColour);
    vColour.Set(wxString("BLACK"));
    SetForegroundColour(vColour);
    m_defaultStatusBarFont = *wxSMALL_FONT;
#else
    m_mediumShadowPen = wxPen("GREY", 1, wxSOLID);
    m_hilightPen = wxPen("WHITE", 1, wxSOLID);
#endif

#ifndef __WXPM__
    m_defaultStatusBarFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));
#endif
}

// Responds to colour changes, and passes event on to children.
void wxStatusBarGeneric::OnSysColourChanged(wxSysColourChangedEvent& event)
{
    InitColours();
    Refresh();

    // Propagate the event to the non-top-level children
    wxWindow::OnSysColourChanged(event);
}

void wxStatusBarGeneric::SetMinHeight(int height)
{
    // check that this min height is not less than minimal height for the
    // current font
    wxClientDC dc(this);
    wxCoord y;
    dc.GetTextExtent( wxT("X"), NULL, &y );

    if ( height > (11*y)/10 )
    {
        SetSize(-1, -1, -1, height + 2*m_borderY);
    }
}

void wxStatusBarGeneric::OnLeftDown(wxMouseEvent& event)
{
#ifdef __WXGTK20__
    int width, height;
    GetClientSize(&width, &height);
    
    if (HasFlag( wxST_SIZEGRIP ) && (event.GetX() > width-height))
    {
        GtkWidget *ancestor = gtk_widget_get_toplevel( m_widget );

        if (!GTK_IS_WINDOW (ancestor))
            return;

        GdkWindow *source = GTK_PIZZA(m_wxwindow)->bin_window;

        int org_x = 0;
        int org_y = 0;
        gdk_window_get_origin( source, &org_x, &org_y );

        gtk_window_begin_resize_drag (GTK_WINDOW (ancestor),
                                  GDK_WINDOW_EDGE_SOUTH_EAST,
                                  1,
                                  org_x + event.GetX(), 
                                  org_y + event.GetY(), 
                                  0);
    }
    else
    {
        event.Skip( TRUE );
    }
#else
    event.Skip( TRUE );
#endif
}

void wxStatusBarGeneric::OnRightDown(wxMouseEvent& event)
{
#ifdef __WXGTK20__
    int width, height;
    GetClientSize(&width, &height);
    
    if (HasFlag( wxST_SIZEGRIP ) && (event.GetX() > width-height))
    {
        GtkWidget *ancestor = gtk_widget_get_toplevel( m_widget );

        if (!GTK_IS_WINDOW (ancestor))
            return;

        GdkWindow *source = GTK_PIZZA(m_wxwindow)->bin_window;

        int org_x = 0;
        int org_y = 0;
        gdk_window_get_origin( source, &org_x, &org_y );
    
        gtk_window_begin_move_drag (GTK_WINDOW (ancestor),
                                2,
                                org_x + event.GetX(), 
                                org_y + event.GetY(), 
                                0);
    }
    else
    {
        event.Skip( TRUE );
    }
#else
    event.Skip( TRUE );
#endif
}

#endif // wxUSE_STATUSBAR

