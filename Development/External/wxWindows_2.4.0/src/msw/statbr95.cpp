///////////////////////////////////////////////////////////////////////////////
// Name:        msw/statbr95.cpp
// Purpose:     native implementation of wxStatusBar
// Author:      Vadim Zeitlin
// Modified by:
// Created:     04.04.98
// RCS-ID:      $Id: statbr95.cpp,v 1.44 2002/01/28 01:38:39 VZ Exp $
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "statbr95.h"
#endif

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
  #pragma hdrstop
#endif

#ifndef WX_PRECOMP
  #include "wx/setup.h"
  #include "wx/frame.h"
  #include "wx/settings.h"
  #include "wx/dcclient.h"
#endif

#if defined(__WIN95__) && wxUSE_NATIVE_STATUSBAR

#include "wx/intl.h"
#include "wx/log.h"
#include "wx/statusbr.h"

#include "wx/msw/private.h"
#include <windowsx.h>

#if defined(__WIN95__) && !((defined(__GNUWIN32_OLD__) || defined(__TWIN32__)) && !defined(__CYGWIN10__))
    #include <commctrl.h>
#endif

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

// windowsx.h and commctrl.h don't define those, so we do it here
#define StatusBar_SetParts(h, n, w) SendMessage(h, SB_SETPARTS, (WPARAM)n, (LPARAM)w)
#define StatusBar_SetText(h, n, t)  SendMessage(h, SB_SETTEXT, (WPARAM)n, (LPARAM)(LPCTSTR)t)
#define StatusBar_GetTextLen(h, n)  LOWORD(SendMessage(h, SB_GETTEXTLENGTH, (WPARAM)n, 0))
#define StatusBar_GetText(h, n, s)  LOWORD(SendMessage(h, SB_GETTEXT, (WPARAM)n, (LPARAM)(LPTSTR)s))

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxStatusBar95 class
// ----------------------------------------------------------------------------

wxStatusBar95::wxStatusBar95()
{
    SetParent(NULL);
    m_hWnd = 0;
    m_windowId = 0;
}

bool wxStatusBar95::Create(wxWindow *parent,
                           wxWindowID id,
                           long style,
                           const wxString& name)
{
    wxCHECK_MSG( parent, FALSE, wxT("status bar must have a parent") );

    SetName(name);
    SetWindowStyleFlag(style);
    SetParent(parent);

    parent->AddChild(this);

    m_windowId = id == -1 ? NewControlId() : id;

    DWORD wstyle = WS_CHILD | WS_VISIBLE;

    if ( style & wxCLIP_SIBLINGS )
        wstyle |= WS_CLIPSIBLINGS;

    // setting SBARS_SIZEGRIP is perfectly useless: it's always on by default
    // (at least in the version of comctl32.dll I'm using), and the only way to
    // turn it off is to use CCS_TOP style - as we position the status bar
    // manually anyhow (see DoMoveWindow), use CCS_TOP style if wxST_SIZEGRIP
    // is not given
    if ( !(style & wxST_SIZEGRIP) )
    {
        wstyle |= CCS_TOP;
    }
    else
    {
        // may be some versions of comctl32.dll do need it - anyhow, it won't
        // do any harm
        wstyle |= SBARS_SIZEGRIP;
    }

    m_hWnd = (WXHWND)CreateStatusWindow(wstyle,
                                        wxEmptyString,
                                        GetHwndOf(parent),
                                        m_windowId);
    if ( m_hWnd == 0 )
    {
        wxLogSysError(_("Failed to create a status bar."));

        return FALSE;
    }

    SetFieldsCount(1);
    SubclassWin(m_hWnd);

    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_MENUBAR));

    return TRUE;
}

wxStatusBar95::~wxStatusBar95()
{
}

void wxStatusBar95::SetFieldsCount(int nFields, const int *widths)
{
    // this is a Windows limitation
    wxASSERT_MSG( (nFields > 0) && (nFields < 255), _T("too many fields") );

    wxStatusBarBase::SetFieldsCount(nFields, widths);

    SetFieldsWidth();
}

void wxStatusBar95::SetStatusWidths(int n, const int widths[])
{
    wxStatusBarBase::SetStatusWidths(n, widths);

    SetFieldsWidth();
}

void wxStatusBar95::SetFieldsWidth()
{
    if ( !m_nFields )
        return;

    int aBorders[3];
    SendMessage(GetHwnd(), SB_GETBORDERS, 0, (LPARAM)aBorders);

    int extraWidth = aBorders[2]; // space between fields

    wxArrayInt widthsAbs =
        CalculateAbsWidths(GetClientSize().x - extraWidth*(m_nFields - 1));

    int *pWidths = new int[m_nFields];

    int nCurPos = 0;
    for ( int i = 0; i < m_nFields; i++ ) {
        nCurPos += widthsAbs[i] + extraWidth;
        pWidths[i] = nCurPos;
    }

    if ( !StatusBar_SetParts(GetHwnd(), m_nFields, pWidths) ) {
        wxLogLastError(wxT("StatusBar_SetParts"));
    }

    delete [] pWidths;
}

void wxStatusBar95::SetStatusText(const wxString& strText, int nField)
{
    wxCHECK_RET( (nField >= 0) && (nField < m_nFields),
                 _T("invalid statusbar field index") );

    if ( !StatusBar_SetText(GetHwnd(), nField, strText) )
    {
        wxLogLastError(wxT("StatusBar_SetText"));
    }
}

wxString wxStatusBar95::GetStatusText(int nField) const
{
    wxCHECK_MSG( (nField >= 0) && (nField < m_nFields), wxEmptyString,
                 _T("invalid statusbar field index") );

    wxString str;
    int len = StatusBar_GetTextLen(GetHwnd(), nField);
    if ( len > 0 )
    {
        StatusBar_GetText(GetHwnd(), nField, str.GetWriteBuf(len));
        str.UngetWriteBuf();
    }

    return str;
}

int wxStatusBar95::GetBorderX() const
{
    int aBorders[3];
    SendMessage(GetHwnd(), SB_GETBORDERS, 0, (LPARAM)aBorders);

    return aBorders[0];
}

int wxStatusBar95::GetBorderY() const
{
    int aBorders[3];
    SendMessage(GetHwnd(), SB_GETBORDERS, 0, (LPARAM)aBorders);

    return aBorders[1];
}

void wxStatusBar95::SetMinHeight(int height)
{
    SendMessage(GetHwnd(), SB_SETMINHEIGHT, height + 2*GetBorderY(), 0);

    // have to send a (dummy) WM_SIZE to redraw it now
    SendMessage(GetHwnd(), WM_SIZE, 0, 0);
}

bool wxStatusBar95::GetFieldRect(int i, wxRect& rect) const
{
    wxCHECK_MSG( (i >= 0) && (i < m_nFields), FALSE,
                 _T("invalid statusbar field index") );

    RECT r;
    if ( !::SendMessage(GetHwnd(), SB_GETRECT, i, (LPARAM)&r) )
    {
        wxLogLastError(wxT("SendMessage(SB_GETRECT)"));
    }

    wxCopyRECTToRect(r, rect);

    return TRUE;
}

void wxStatusBar95::DoMoveWindow(int x, int y, int width, int height)
{
    // the status bar wnd proc must be forwarded the WM_SIZE message whenever
    // the stat bar position/size is changed because it normally positions the
    // control itself along bottom or top side of the parent window - failing
    // to do so will result in nasty visual effects
    FORWARD_WM_SIZE(GetHwnd(), SIZE_RESTORED, x, y, SendMessage);

    // but now, when the standard status bar wnd proc did all it wanted to do,
    // move the status bar to its correct location - usually this call may be
    // omitted because for normal status bars (positioned along the bottom
    // edge) the position is already set correctly, but if the user wants to
    // position them in some exotic location, this is really needed
    wxWindow::DoMoveWindow(x, y, width, height);

    // adjust fields widths to the new size
    SetFieldsWidth();

    // we have to trigger wxSizeEvent if there are children window in status 
    // bar because GetFieldRect returned incorrect (not updated) values up to
    // here, which almost certainly resulted in incorrectly redrawn statusbar
    if ( m_children.GetCount() > 0 )
    {
        wxSizeEvent event(GetClientSize(), m_windowId);
        event.SetEventObject(this);
        GetEventHandler()->ProcessEvent(event);
    }
}

#endif // __WIN95__ && wxUSE_NATIVE_STATUSBAR

