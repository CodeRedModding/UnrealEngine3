/////////////////////////////////////////////////////////////////////////////
// Name:        msw/button.cpp
// Purpose:     wxButton
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: button.cpp,v 1.56 2002/08/23 16:00:32 VZ Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "button.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_BUTTON

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/button.h"
    #include "wx/brush.h"
    #include "wx/panel.h"
    #include "wx/bmpbuttn.h"
    #include "wx/settings.h"
    #include "wx/dcscreen.h"
#endif

#include "wx/msw/private.h"

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxButton, wxControl)

// this macro tries to adjust the default button height to a reasonable value
// using the char height as the base
#define BUTTON_HEIGHT_FROM_CHAR_HEIGHT(cy) (11*EDIT_HEIGHT_FROM_CHAR_HEIGHT(cy)/10)

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// creation/destruction
// ----------------------------------------------------------------------------

bool wxButton::Create(wxWindow *parent,
                      wxWindowID id,
                      const wxString& label,
                      const wxPoint& pos,
                      const wxSize& size,
                      long style,
                      const wxValidator& validator,
                      const wxString& name)
{
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return FALSE;

    WXDWORD exstyle;
    WXDWORD msStyle = MSWGetStyle(style, &exstyle);

#ifdef __WIN32__
    // if the label contains several lines we must explicitly tell the button
    // about it or it wouldn't draw it correctly ("\n"s would just appear as
    // black boxes)
    //
    // NB: we do it here and not in MSWGetStyle() because we need the label
    //     value and m_label is not set yet when MSWGetStyle() is called;
    //     besides changing BS_MULTILINE during run-time is pointless anyhow
    if ( label.find(_T('\n')) != wxString::npos )
    {
        msStyle |= BS_MULTILINE;
    }
#endif // __WIN32__

    return MSWCreateControl(_T("BUTTON"), msStyle, pos, size, label, exstyle);
}

wxButton::~wxButton()
{
}

// ----------------------------------------------------------------------------
// flags
// ----------------------------------------------------------------------------

WXDWORD wxButton::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    // buttons never have an external border, they draw their own one
    WXDWORD msStyle = wxControl::MSWGetStyle
                      (
                        (style & ~wxBORDER_MASK) | wxBORDER_NONE, exstyle
                      );

    // we must use WS_CLIPSIBLINGS with the buttons or they would draw over
    // each other in any resizeable dialog which has more than one button in
    // the bottom
    msStyle |= WS_CLIPSIBLINGS;

#ifdef __WIN32__
    // don't use "else if" here: weird as it is, but you may combine wxBU_LEFT
    // and wxBU_RIGHT to get BS_CENTER!
    if ( style & wxBU_LEFT )
        msStyle |= BS_LEFT;
    if ( style & wxBU_RIGHT )
        msStyle |= BS_RIGHT;
    if ( style & wxBU_TOP )
        msStyle |= BS_TOP;
    if ( style & wxBU_BOTTOM )
        msStyle |= BS_BOTTOM;
#endif // __WIN32__

    return msStyle;
}

// ----------------------------------------------------------------------------
// size management including autosizing
// ----------------------------------------------------------------------------

wxSize wxButton::DoGetBestSize() const
{
    int wBtn;
    GetTextExtent(wxGetWindowText(GetHWND()), &wBtn, NULL);

    int wChar, hChar;
    wxGetCharSize(GetHWND(), &wChar, &hChar, &GetFont());

    // add a margin -- the button is wider than just its label
    wBtn += 3*wChar;

    // the button height is proportional to the height of the font used
    int hBtn = BUTTON_HEIGHT_FROM_CHAR_HEIGHT(hChar);

    // all buttons have at least the standard size unless the user explicitly
    // wants them to be of smaller size and used wxBU_EXACTFIT style when
    // creating the button
    if ( !HasFlag(wxBU_EXACTFIT) )
    {
        wxSize sz = GetDefaultSize();
        if (wBtn > sz.x)
            sz.x = wBtn;
        if (hBtn > sz.y)
            sz.y = hBtn;

        return sz;
    }

    return wxSize(wBtn, hBtn);
}

/* static */
wxSize wxButtonBase::GetDefaultSize()
{
    static wxSize s_sizeBtn;

    if ( s_sizeBtn.x == 0 )
    {
        wxScreenDC dc;
        dc.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));

        // the size of a standard button in the dialog units is 50x14,
        // translate this to pixels
        // NB1: the multipliers come from the Windows convention
        // NB2: the extra +1/+2 were needed to get the size be the same as the
        //      size of the buttons in the standard dialog - I don't know how
        //      this happens, but on my system this size is 75x23 in pixels and
        //      23*8 isn't even divisible by 14... Would be nice to understand
        //      why these constants are needed though!
        s_sizeBtn.x = (50 * (dc.GetCharWidth() + 1))/4;
        s_sizeBtn.y = ((14 * dc.GetCharHeight()) + 2)/8;
    }

    return s_sizeBtn;
}

// ----------------------------------------------------------------------------
// default button handling
// ----------------------------------------------------------------------------

/*
   "Everything you ever wanted to know about the default buttons" or "Why do we
   have to do all this?"

   In MSW the default button should be activated when the user presses Enter
   and the current control doesn't process Enter itself somehow. This is
   handled by ::DefWindowProc() (or maybe ::DefDialogProc()) using DM_SETDEFID
   Another aspect of "defaultness" is that the default button has different
   appearance: this is due to BS_DEFPUSHBUTTON style which is completely
   separate from DM_SETDEFID stuff (!). Also note that BS_DEFPUSHBUTTON should
   be unset if our parent window is not active so it should be unset whenever
   we lose activation and set back when we regain it.

   Final complication is that when a button is active, it should be the default
   one, i.e. pressing Enter on a button always activates it and not another
   one.

   We handle this by maintaining a permanent and a temporary default items in
   wxControlContainer (both may be NULL). When a button becomes the current
   control (i.e. gets focus) it sets itself as the temporary default which
   ensures that it has the right appearance and that Enter will be redirected
   to it. When the button loses focus, it unsets the temporary default and so
   the default item will be the permanent default -- that is the default button
   if any had been set or none otherwise, which is just what we want.

   NB: all this is quite complicated by now and the worst is that normally
       it shouldn't be necessary at all as for the normal Windows programs
       DefWindowProc() and IsDialogMessage() take care of all this
       automatically -- however in wxWindows programs this doesn't work for
       nested hierarchies (i.e. a notebook inside a notebook) for unknown
       reason and so we have to reproduce all this code ourselves. It would be
       very nice if we could avoid doing it.
 */

// set this button as the (permanently) default one in its panel
void wxButton::SetDefault()
{
    wxWindow *parent = GetParent();

    wxCHECK_RET( parent, _T("button without parent?") );

    // set this one as the default button both for wxWindows ...
    wxWindow *winOldDefault = parent->SetDefaultItem(this);

    // ... and Windows
    SetDefaultStyle(wxDynamicCast(winOldDefault, wxButton), FALSE);
    SetDefaultStyle(this, TRUE);
}

// set this button as being currently default
void wxButton::SetTmpDefault()
{
    wxWindow *parent = GetParent();

    wxCHECK_RET( parent, _T("button without parent?") );

    wxWindow *winOldDefault = parent->GetDefaultItem();
    parent->SetTmpDefaultItem(this);

    SetDefaultStyle(wxDynamicCast(winOldDefault, wxButton), FALSE);
    SetDefaultStyle(this, TRUE);
}

// unset this button as currently default, it may still stay permanent default
void wxButton::UnsetTmpDefault()
{
    wxWindow *parent = GetParent();

    wxCHECK_RET( parent, _T("button without parent?") );

    parent->SetTmpDefaultItem(NULL);

    wxWindow *winOldDefault = parent->GetDefaultItem();

    SetDefaultStyle(this, FALSE);
    SetDefaultStyle(wxDynamicCast(winOldDefault, wxButton), TRUE);
}

/* static */
void
wxButton::SetDefaultStyle(wxButton *btn, bool on)
{
    // we may be called with NULL pointer -- simpler to do the check here than
    // in the caller which does wxDynamicCast()
    if ( !btn )
        return;

    // first, let DefDlgProc() know about the new default button
    if ( on )
    {
        // we shouldn't set BS_DEFPUSHBUTTON for any button if we don't have
        // focus at all any more
        if ( !wxTheApp->IsActive() )
            return;

        // look for a panel-like window
        wxWindow *win = btn->GetParent();
        while ( win && !win->HasFlag(wxTAB_TRAVERSAL) )
            win = win->GetParent();

        if ( win )
        {
            ::SendMessage(GetHwndOf(win), DM_SETDEFID, btn->GetId(), 0L);

            // sending DM_SETDEFID also changes the button style to
            // BS_DEFPUSHBUTTON so there is nothing more to do
        }
    }

    // then also change the style as needed
    long style = ::GetWindowLong(GetHwndOf(btn), GWL_STYLE);
    if ( !(style & BS_DEFPUSHBUTTON) == on )
    {
        // don't do it with the owner drawn buttons because it will
        // reset BS_OWNERDRAW style bit too (as BS_OWNERDRAW &
        // BS_DEFPUSHBUTTON != 0)!
        if ( (style & BS_OWNERDRAW) != BS_OWNERDRAW )
        {
            ::SendMessage(GetHwndOf(btn), BM_SETSTYLE,
                          on ? style | BS_DEFPUSHBUTTON
                             : style & ~BS_DEFPUSHBUTTON,
                          1L /* redraw */);
        }
        else // owner drawn
        {
            // redraw the button - it will notice itself that it's
            // [not] the default one [any longer]
            btn->Refresh();
        }
    }
    //else: already has correct style
}

// ----------------------------------------------------------------------------
// helpers
// ----------------------------------------------------------------------------

bool wxButton::SendClickEvent()
{
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, GetId());
    event.SetEventObject(this);

    return ProcessCommand(event);
}

void wxButton::Command(wxCommandEvent & event)
{
    ProcessCommand(event);
}

// ----------------------------------------------------------------------------
// event/message handlers
// ----------------------------------------------------------------------------

bool wxButton::MSWCommand(WXUINT param, WXWORD WXUNUSED(id))
{
    bool processed = FALSE;
    switch ( param )
    {
        case 1:                     // message came from an accelerator
        case BN_CLICKED:            // normal buttons send this
        case BN_DOUBLECLICKED:      // owner-drawn ones also send this
            processed = SendClickEvent();
            break;
    }

    return processed;
}

long wxButton::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    // when we receive focus, we want to temporary become the default button in
    // our parent panel so that pressing "Enter" would activate us -- and when
    // losing it we should restore the previous default button as well
    if ( nMsg == WM_SETFOCUS )
    {
        SetTmpDefault();

        // let the default processing take place too
    }
    else if ( nMsg == WM_KILLFOCUS )
    {
        UnsetTmpDefault();
    }
    else if ( nMsg == WM_LBUTTONDBLCLK )
    {
        // emulate a click event to force an owner-drawn button to change its
        // appearance - without this, it won't do it
        (void)wxControl::MSWWindowProc(WM_LBUTTONDOWN, wParam, lParam);

        // and continue with processing the message normally as well
    }

    // let the base class do all real processing
    return wxControl::MSWWindowProc(nMsg, wParam, lParam);
}

// ----------------------------------------------------------------------------
// owner-drawn buttons support
// ----------------------------------------------------------------------------

#ifdef __WIN32__

// drawing helpers

static void DrawButtonText(HDC hdc,
                           RECT *pRect,
                           const wxString& text,
                           COLORREF col)
{
    COLORREF colOld = SetTextColor(hdc, col);
    int modeOld = SetBkMode(hdc, TRANSPARENT);

    DrawText(hdc, text, text.length(), pRect,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetBkMode(hdc, modeOld);
    SetTextColor(hdc, colOld);
}

static void DrawRect(HDC hdc, const RECT& r)
{
    MoveToEx(hdc, r.left, r.top, NULL);
    LineTo(hdc, r.right, r.top);
    LineTo(hdc, r.right, r.bottom);
    LineTo(hdc, r.left, r.bottom);
    LineTo(hdc, r.left, r.top);
}

void wxButton::MakeOwnerDrawn()
{
    long style = GetWindowLong(GetHwnd(), GWL_STYLE);
    if ( (style & BS_OWNERDRAW) != BS_OWNERDRAW )
    {
        // make it so
        style |= BS_OWNERDRAW;
        SetWindowLong(GetHwnd(), GWL_STYLE, style);
    }
}

bool wxButton::SetBackgroundColour(const wxColour &colour)
{
    if ( !wxControl::SetBackgroundColour(colour) )
    {
        // nothing to do
        return FALSE;
    }

    MakeOwnerDrawn();

    Refresh();

    return TRUE;
}

bool wxButton::SetForegroundColour(const wxColour &colour)
{
    if ( !wxControl::SetForegroundColour(colour) )
    {
        // nothing to do
        return FALSE;
    }

    MakeOwnerDrawn();

    Refresh();

    return TRUE;
}

/*
   The button frame looks like this normally:

   WWWWWWWWWWWWWWWWWWB
   WHHHHHHHHHHHHHHHHGB  W = white       (HILIGHT)
   WH               GB  H = light grey  (LIGHT)
   WH               GB  G = dark grey   (SHADOW)
   WH               GB  B = black       (DKSHADOW)
   WH               GB
   WGGGGGGGGGGGGGGGGGB
   BBBBBBBBBBBBBBBBBBB

   When the button is selected, the button becomes like this (the total button
   size doesn't change):

   BBBBBBBBBBBBBBBBBBB
   BWWWWWWWWWWWWWWWWBB
   BWHHHHHHHHHHHHHHGBB
   BWH             GBB
   BWH             GBB
   BWGGGGGGGGGGGGGGGBB
   BBBBBBBBBBBBBBBBBBB
   BBBBBBBBBBBBBBBBBBB

   When the button is pushed (while selected) it is like:

   BBBBBBBBBBBBBBBBBBB
   BGGGGGGGGGGGGGGGGGB
   BG               GB
   BG               GB
   BG               GB
   BG               GB
   BGGGGGGGGGGGGGGGGGB
   BBBBBBBBBBBBBBBBBBB
*/

static void DrawButtonFrame(HDC hdc, const RECT& rectBtn,
                            bool selected, bool pushed)
{
    RECT r;
    CopyRect(&r, &rectBtn);

    HPEN hpenBlack   = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DDKSHADOW)),
         hpenGrey    = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW)),
         hpenLightGr = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DLIGHT)),
         hpenWhite   = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHILIGHT));

    HPEN hpenOld = (HPEN)SelectObject(hdc, hpenBlack);

    r.right--;
    r.bottom--;

    if ( pushed )
    {
        DrawRect(hdc, r);

        (void)SelectObject(hdc, hpenGrey);
        InflateRect(&r, -1, -1);

        DrawRect(hdc, r);
    }
    else // !pushed
    {
        if ( selected )
        {
            DrawRect(hdc, r);

            InflateRect(&r, -1, -1);
        }

        MoveToEx(hdc, r.left, r.bottom, NULL);
        LineTo(hdc, r.right, r.bottom);
        LineTo(hdc, r.right, r.top - 1);

        (void)SelectObject(hdc, hpenWhite);
        MoveToEx(hdc, r.left, r.bottom - 1, NULL);
        LineTo(hdc, r.left, r.top);
        LineTo(hdc, r.right, r.top);

        (void)SelectObject(hdc, hpenLightGr);
        MoveToEx(hdc, r.left + 1, r.bottom - 2, NULL);
        LineTo(hdc, r.left + 1, r.top + 1);
        LineTo(hdc, r.right - 1, r.top + 1);

        (void)SelectObject(hdc, hpenGrey);
        MoveToEx(hdc, r.left + 1, r.bottom - 1, NULL);
        LineTo(hdc, r.right - 1, r.bottom - 1);
        LineTo(hdc, r.right - 1, r.top);
    }

    (void)SelectObject(hdc, hpenOld);
    DeleteObject(hpenWhite);
    DeleteObject(hpenLightGr);
    DeleteObject(hpenGrey);
    DeleteObject(hpenBlack);
}

bool wxButton::MSWOnDraw(WXDRAWITEMSTRUCT *wxdis)
{
    LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)wxdis;

    RECT rectBtn;
    CopyRect(&rectBtn, &lpDIS->rcItem);

    COLORREF colBg = wxColourToRGB(GetBackgroundColour()),
             colFg = wxColourToRGB(GetForegroundColour());

    HDC hdc = lpDIS->hDC;
    UINT state = lpDIS->itemState;

    // first, draw the background
    HBRUSH hbrushBackground = ::CreateSolidBrush(colBg);

    FillRect(hdc, &rectBtn, hbrushBackground);

    // draw the border for the current state
    bool selected = (state & ODS_SELECTED) != 0;
    if ( !selected )
    {
        wxPanel *panel = wxDynamicCast(GetParent(), wxPanel);
        if ( panel )
        {
            selected = panel->GetDefaultItem() == this;
        }
    }
    bool pushed = (SendMessage(GetHwnd(), BM_GETSTATE, 0, 0) & BST_PUSHED) != 0;

    DrawButtonFrame(hdc, rectBtn, selected, pushed);

    // draw the focus rect if needed
    if ( state & ODS_FOCUS )
    {
        RECT rectFocus;
        CopyRect(&rectFocus, &rectBtn);

        // I don't know where does this constant come from, but this is how
        // Windows draws them
        InflateRect(&rectFocus, -4, -4);

        DrawFocusRect(hdc, &rectFocus);
    }

    if ( pushed )
    {
        // the label is shifted by 1 pixel to create "pushed" effect
        OffsetRect(&rectBtn, 1, 1);
    }

    DrawButtonText(hdc, &rectBtn, GetLabel(),
                   state & ODS_DISABLED ? GetSysColor(COLOR_GRAYTEXT)
                                        : colFg);

    ::DeleteObject(hbrushBackground);

    return TRUE;
}

#endif // __WIN32__

#endif // wxUSE_BUTTON

