///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/listbox.cpp
// Purpose:     wxListBox
// Author:      Julian Smart
// Modified by: Vadim Zeitlin (owner drawn stuff)
// Created:
// RCS-ID:      $Id: listbox.cpp,v 1.68.2.5 2002/12/19 23:13:53 JS Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
    #pragma implementation "listbox.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_LISTBOX

#ifndef WX_PRECOMP
#include "wx/listbox.h"
#include "wx/settings.h"
#include "wx/brush.h"
#include "wx/font.h"
#include "wx/dc.h"
#include "wx/utils.h"
#endif

#include "wx/window.h"
#include "wx/msw/private.h"

#include <windowsx.h>

#ifdef __WXWINE__
  #if defined(GetWindowStyle)
    #undef GetWindowStyle
  #endif
#endif

#include "wx/dynarray.h"
#include "wx/log.h"

#if wxUSE_OWNER_DRAWN
    #include  "wx/ownerdrw.h"
#endif

#ifndef __TWIN32__
    #ifdef __GNUWIN32_OLD__
        #include "wx/msw/gnuwin32/extra.h"
    #endif
#endif

#ifdef __WXWINE__
  #ifndef ListBox_SetItemData
    #define ListBox_SetItemData(hwndCtl, index, data) \
      ((int)(DWORD)SendMessage((hwndCtl), LB_SETITEMDATA, (WPARAM)(int)(index), (LPARAM)(data)))
  #endif
  #ifndef ListBox_GetHorizontalExtent
    #define ListBox_GetHorizontalExtent(hwndCtl) \
      ((int)(DWORD)SendMessage((hwndCtl), LB_GETHORIZONTALEXTENT, 0L, 0L))
  #endif
  #ifndef ListBox_GetSelCount
    #define ListBox_GetSelCount(hwndCtl) \
      ((int)(DWORD)SendMessage((hwndCtl), LB_GETSELCOUNT, 0L, 0L))
  #endif
  #ifndef ListBox_GetSelItems
    #define ListBox_GetSelItems(hwndCtl, cItems, lpItems) \
      ((int)(DWORD)SendMessage((hwndCtl), LB_GETSELITEMS, (WPARAM)(int)(cItems), (LPARAM)(int *)(lpItems)))
  #endif
  #ifndef ListBox_GetTextLen
    #define ListBox_GetTextLen(hwndCtl, index) \
      ((int)(DWORD)SendMessage((hwndCtl), LB_GETTEXTLEN, (WPARAM)(int)(index), 0L))
  #endif
  #ifndef ListBox_GetText
    #define ListBox_GetText(hwndCtl, index, lpszBuffer) \
      ((int)(DWORD)SendMessage((hwndCtl), LB_GETTEXT, (WPARAM)(int)(index), (LPARAM)(LPCTSTR)(lpszBuffer)))
  #endif
#endif

    IMPLEMENT_DYNAMIC_CLASS(wxListBox, wxControl)

// ============================================================================
// list box item declaration and implementation
// ============================================================================

#if wxUSE_OWNER_DRAWN

class wxListBoxItem : public wxOwnerDrawn
{
public:
    wxListBoxItem(const wxString& str = wxEmptyString);
};

wxListBoxItem::wxListBoxItem(const wxString& str) : wxOwnerDrawn(str, FALSE)
{
    // no bitmaps/checkmarks
    SetMarginWidth(0);
}

wxOwnerDrawn *wxListBox::CreateLboxItem(size_t WXUNUSED(n))
{
    return new wxListBoxItem();
}

#endif  //USE_OWNER_DRAWN

// ============================================================================
// list box control implementation
// ============================================================================

// ----------------------------------------------------------------------------
// creation
// ----------------------------------------------------------------------------

// Listbox item
wxListBox::wxListBox()
{
    m_noItems = 0;
    m_selected = 0;
}

bool wxListBox::Create(wxWindow *parent,
                       wxWindowID id,
                       const wxPoint& pos,
                       const wxSize& size,
                       int n, const wxString choices[],
                       long style,
                       const wxValidator& validator,
                       const wxString& name)
{
    m_noItems = 0;
    m_hWnd = 0;
    m_selected = 0;

    SetName(name);
#if wxUSE_VALIDATORS
    SetValidator(validator);
#endif // wxUSE_VALIDATORS

    if (parent)
        parent->AddChild(this);

    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    SetForegroundColour(parent->GetForegroundColour());

    m_windowId = ( id == -1 ) ? (int)NewControlId() : id;

    int x = pos.x;
    int y = pos.y;
    int width = size.x;
    int height = size.y;
    m_windowStyle = style;

    DWORD wstyle = WS_VISIBLE | WS_VSCROLL | WS_TABSTOP |
                   LBS_NOTIFY | LBS_HASSTRINGS /* | WS_CLIPSIBLINGS */;

    wxASSERT_MSG( !(style & wxLB_MULTIPLE) || !(style & wxLB_EXTENDED),
                  _T("only one of listbox selection modes can be specified") );

    if ( (m_windowStyle & wxBORDER_MASK) == wxBORDER_DEFAULT )
        m_windowStyle |= wxBORDER_SUNKEN;

    if ( m_windowStyle & wxCLIP_SIBLINGS )
        wstyle |= WS_CLIPSIBLINGS;

    if (m_windowStyle & wxLB_MULTIPLE)
        wstyle |= LBS_MULTIPLESEL;
    else if (m_windowStyle & wxLB_EXTENDED)
        wstyle |= LBS_EXTENDEDSEL;

    if (m_windowStyle & wxLB_ALWAYS_SB)
        wstyle |= LBS_DISABLENOSCROLL;
    if (m_windowStyle & wxLB_HSCROLL)
        wstyle |= WS_HSCROLL;
    if (m_windowStyle & wxLB_SORT)
        wstyle |= LBS_SORT;

#if wxUSE_OWNER_DRAWN
    if ( m_windowStyle & wxLB_OWNERDRAW ) {
        // we don't support LBS_OWNERDRAWVARIABLE yet
        wstyle |= LBS_OWNERDRAWFIXED;
    }
#endif

    // Without this style, you get unexpected heights, so e.g. constraint layout
    // doesn't work properly
    wstyle |= LBS_NOINTEGRALHEIGHT;

    bool want3D;
    WXDWORD exStyle = Determine3DEffects(WS_EX_CLIENTEDGE, &want3D);

    // Even with extended styles, need to combine with WS_BORDER for them to
    // look right.
    if ( want3D || wxStyleHasBorder(m_windowStyle) )
    {
        wstyle |= WS_BORDER;
    }

    m_hWnd = (WXHWND)::CreateWindowEx(exStyle, wxT("LISTBOX"), NULL,
            wstyle | WS_CHILD,
            0, 0, 0, 0,
            (HWND)parent->GetHWND(), (HMENU)m_windowId,
            wxGetInstance(), NULL);

    wxCHECK_MSG( m_hWnd, FALSE, wxT("Failed to create listbox") );

#if wxUSE_CTL3D
    if (want3D)
    {
        Ctl3dSubclassCtl(GetHwnd());
        m_useCtl3D = TRUE;
    }
#endif

    // Subclass again to catch messages
    SubclassWin(m_hWnd);

    size_t ui;
    for (ui = 0; ui < (size_t)n; ui++) {
        Append(choices[ui]);
    }

    if ( (m_windowStyle & wxLB_MULTIPLE) == 0 )
        SendMessage(GetHwnd(), LB_SETCURSEL, 0, 0);

    SetFont(parent->GetFont());

    SetSize(x, y, width, height);

    return TRUE;
}

wxListBox::~wxListBox()
{
    Free();
}

void wxListBox::SetupColours()
{
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    SetForegroundColour(GetParent()->GetForegroundColour());
}

// ----------------------------------------------------------------------------
// implementation of wxListBoxBase methods
// ----------------------------------------------------------------------------

void wxListBox::DoSetFirstItem(int N)
{
    wxCHECK_RET( N >= 0 && N < m_noItems,
                 wxT("invalid index in wxListBox::SetFirstItem") );

    SendMessage(GetHwnd(), LB_SETTOPINDEX, (WPARAM)N, (LPARAM)0);
}

void wxListBox::Delete(int N)
{
    wxCHECK_RET( N >= 0 && N < m_noItems,
                 wxT("invalid index in wxListBox::Delete") );

    // for owner drawn objects, the data is used for storing wxOwnerDrawn
    // pointers and we shouldn't touch it
#if !wxUSE_OWNER_DRAWN
    if ( !(m_windowStyle & wxLB_OWNERDRAW) )
#endif // !wxUSE_OWNER_DRAWN
        if ( HasClientObjectData() )
        {
            delete GetClientObject(N);
        }

    SendMessage(GetHwnd(), LB_DELETESTRING, N, 0);
    m_noItems--;

    SetHorizontalExtent(wxEmptyString);
}

int wxListBox::DoAppend(const wxString& item)
{
    int index = ListBox_AddString(GetHwnd(), item);
    m_noItems++;

#if wxUSE_OWNER_DRAWN
    if ( m_windowStyle & wxLB_OWNERDRAW ) {
        wxOwnerDrawn *pNewItem = CreateLboxItem(index); // dummy argument
        pNewItem->SetName(item);
        m_aItems.Insert(pNewItem, index);
        ListBox_SetItemData(GetHwnd(), index, pNewItem);
        pNewItem->SetFont(GetFont());
    }
#endif // wxUSE_OWNER_DRAWN

    SetHorizontalExtent(item);

    return index;
}

void wxListBox::DoSetItems(const wxArrayString& choices, void** clientData)
{
    // avoid flicker - but don't need to do this for a hidden listbox
    bool hideAndShow = IsShown();
    if ( hideAndShow )
    {
        ShowWindow(GetHwnd(), SW_HIDE);
    }

    ListBox_ResetContent(GetHwnd());

    m_noItems = choices.GetCount();
    int i;
    for (i = 0; i < m_noItems; i++)
    {
        ListBox_AddString(GetHwnd(), choices[i]);
        if ( clientData )
        {
            SetClientData(i, clientData[i]);
        }
    }

#if wxUSE_OWNER_DRAWN
    if ( m_windowStyle & wxLB_OWNERDRAW ) {
        // first delete old items
        WX_CLEAR_ARRAY(m_aItems);

        // then create new ones
        for ( size_t ui = 0; ui < (size_t)m_noItems; ui++ ) {
            wxOwnerDrawn *pNewItem = CreateLboxItem(ui);
            pNewItem->SetName(choices[ui]);
            m_aItems.Add(pNewItem);
            ListBox_SetItemData(GetHwnd(), ui, pNewItem);
        }
    }
#endif // wxUSE_OWNER_DRAWN

    SetHorizontalExtent();

    if ( hideAndShow )
    {
        // show the listbox back if we hid it
        ShowWindow(GetHwnd(), SW_SHOW);
    }
}

int wxListBox::FindString(const wxString& s) const
{
    int pos = ListBox_FindStringExact(GetHwnd(), (WPARAM)-1, s);
    if (pos == LB_ERR)
        return wxNOT_FOUND;
    else
        return pos;
}

void wxListBox::Clear()
{
    Free();

    ListBox_ResetContent(GetHwnd());

    m_noItems = 0;
    SetHorizontalExtent();
}

void wxListBox::Free()
{
#if wxUSE_OWNER_DRAWN
    if ( m_windowStyle & wxLB_OWNERDRAW )
    {
        WX_CLEAR_ARRAY(m_aItems);
    }
    else
#endif // wxUSE_OWNER_DRAWN
    if ( HasClientObjectData() )
    {
        for ( size_t n = 0; n < (size_t)m_noItems; n++ )
        {
            delete GetClientObject(n);
        }
    }
}

void wxListBox::SetSelection(int N, bool select)
{
    wxCHECK_RET( N >= 0 && N < m_noItems,
                 wxT("invalid index in wxListBox::SetSelection") );

    if ( HasMultipleSelection() )
    {
        SendMessage(GetHwnd(), LB_SETSEL, select, N);
    }
    else
    {
        SendMessage(GetHwnd(), LB_SETCURSEL, select ? N : -1, 0);
    }
}

bool wxListBox::IsSelected(int N) const
{
    wxCHECK_MSG( N >= 0 && N < m_noItems, FALSE,
                 wxT("invalid index in wxListBox::Selected") );

    return SendMessage(GetHwnd(), LB_GETSEL, N, 0) == 0 ? FALSE : TRUE;
}

wxClientData* wxListBox::DoGetItemClientObject(int n) const
{
    return (wxClientData *)DoGetItemClientData(n);
}

void *wxListBox::DoGetItemClientData(int n) const
{
    wxCHECK_MSG( n >= 0 && n < m_noItems, NULL,
                 wxT("invalid index in wxListBox::GetClientData") );

    return (void *)SendMessage(GetHwnd(), LB_GETITEMDATA, n, 0);
}

void wxListBox::DoSetItemClientObject(int n, wxClientData* clientData)
{
    DoSetItemClientData(n, clientData);
}

void wxListBox::DoSetItemClientData(int n, void *clientData)
{
    wxCHECK_RET( n >= 0 && n < m_noItems,
                 wxT("invalid index in wxListBox::SetClientData") );

#if wxUSE_OWNER_DRAWN
    if ( m_windowStyle & wxLB_OWNERDRAW )
    {
        // client data must be pointer to wxOwnerDrawn, otherwise we would crash
        // in OnMeasure/OnDraw.
        wxFAIL_MSG(wxT("Can't use client data with owner-drawn listboxes"));
    }
#endif // wxUSE_OWNER_DRAWN

    if ( ListBox_SetItemData(GetHwnd(), n, clientData) == LB_ERR )
        wxLogDebug(wxT("LB_SETITEMDATA failed"));
}

// Return number of selections and an array of selected integers
int wxListBox::GetSelections(wxArrayInt& aSelections) const
{
    aSelections.Empty();

    if ( HasMultipleSelection() )
    {
        int countSel = ListBox_GetSelCount(GetHwnd());
        if ( countSel == LB_ERR )
        {
            wxLogDebug(_T("ListBox_GetSelCount failed"));
        }
        else if ( countSel != 0 )
        {
            int *selections = new int[countSel];

            if ( ListBox_GetSelItems(GetHwnd(),
                                     countSel, selections) == LB_ERR )
            {
                wxLogDebug(wxT("ListBox_GetSelItems failed"));
                countSel = -1;
            }
            else
            {
                aSelections.Alloc(countSel);
                for ( int n = 0; n < countSel; n++ )
                    aSelections.Add(selections[n]);
            }

            delete [] selections;
        }

        return countSel;
    }
    else  // single-selection listbox
    {
        if (ListBox_GetCurSel(GetHwnd()) > -1)
            aSelections.Add(ListBox_GetCurSel(GetHwnd()));

        return aSelections.Count();
    }
}

// Get single selection, for single choice list items
int wxListBox::GetSelection() const
{
    wxCHECK_MSG( !HasMultipleSelection(),
                 -1,
                 wxT("GetSelection() can't be used with multiple-selection listboxes, use GetSelections() instead.") );

    return ListBox_GetCurSel(GetHwnd());
}

// Find string for position
wxString wxListBox::GetString(int N) const
{
    wxCHECK_MSG( N >= 0 && N < m_noItems, wxEmptyString,
                 wxT("invalid index in wxListBox::GetClientData") );

    int len = ListBox_GetTextLen(GetHwnd(), N);

    // +1 for terminating NUL
    wxString result;
    ListBox_GetText(GetHwnd(), N, result.GetWriteBuf(len + 1));
    result.UngetWriteBuf();

    return result;
}

void
wxListBox::DoInsertItems(const wxArrayString& items, int pos)
{
    wxCHECK_RET( pos >= 0 && pos <= m_noItems,
                 wxT("invalid index in wxListBox::InsertItems") );

    int nItems = items.GetCount();
    for ( int i = 0; i < nItems; i++ )
    {
        int idx = ListBox_InsertString(GetHwnd(), i + pos, items[i]);

#if wxUSE_OWNER_DRAWN
        if ( m_windowStyle & wxLB_OWNERDRAW )
        {
            wxOwnerDrawn *pNewItem = CreateLboxItem(idx);
            pNewItem->SetName(items[i]);
            pNewItem->SetFont(GetFont());
            m_aItems.Insert(pNewItem, idx);

            ListBox_SetItemData(GetHwnd(), idx, pNewItem);
        }
#endif // wxUSE_OWNER_DRAWN
    }

    m_noItems += nItems;

    SetHorizontalExtent();
}

void wxListBox::SetString(int N, const wxString& s)
{
    wxCHECK_RET( N >= 0 && N < m_noItems,
                 wxT("invalid index in wxListBox::SetString") );

    // remember the state of the item
    bool wasSelected = IsSelected(N);

    void *oldData = NULL;
    wxClientData *oldObjData = NULL;
    if ( m_clientDataItemsType == wxClientData_Void )
        oldData = GetClientData(N);
    else if ( m_clientDataItemsType == wxClientData_Object )
        oldObjData = GetClientObject(N);

    // delete and recreate it
    SendMessage(GetHwnd(), LB_DELETESTRING, N, 0);

    int newN = N;
    if ( N == m_noItems - 1 )
        newN = -1;

    ListBox_InsertString(GetHwnd(), newN, s);

    // restore the client data
    if ( oldData )
        SetClientData(N, oldData);
    else if ( oldObjData )
        SetClientObject(N, oldObjData);

    // we may have lost the selection
    if ( wasSelected )
        Select(N);

#if wxUSE_OWNER_DRAWN
    if ( m_windowStyle & wxLB_OWNERDRAW )
    {
        // update item's text
        m_aItems[N]->SetName(s);

        // reassign the item's data
        ListBox_SetItemData(GetHwnd(), N, m_aItems[N]);
    }
#endif  //USE_OWNER_DRAWN
}

int wxListBox::GetCount() const
{
    return m_noItems;
}

// ----------------------------------------------------------------------------
// helpers
// ----------------------------------------------------------------------------

// Windows-specific code to set the horizontal extent of the listbox, if
// necessary. If s is non-NULL, it's used to calculate the horizontal extent.
// Otherwise, all strings are used.
void wxListBox::SetHorizontalExtent(const wxString& s)
{
    // Only necessary if we want a horizontal scrollbar
    if (!(m_windowStyle & wxHSCROLL))
        return;
    TEXTMETRIC lpTextMetric;

    if ( !s.IsEmpty() )
    {
        int existingExtent = (int)SendMessage(GetHwnd(), LB_GETHORIZONTALEXTENT, 0, 0L);
        HDC dc = GetWindowDC(GetHwnd());
        HFONT oldFont = 0;
        if (GetFont().Ok() && GetFont().GetResourceHandle())
            oldFont = (HFONT) ::SelectObject(dc, (HFONT) GetFont().GetResourceHandle());

        GetTextMetrics(dc, &lpTextMetric);
        SIZE extentXY;
        ::GetTextExtentPoint(dc, (LPTSTR) (const wxChar *)s, s.Length(), &extentXY);
        int extentX = (int)(extentXY.cx + lpTextMetric.tmAveCharWidth);

        if (oldFont)
            ::SelectObject(dc, oldFont);

        ReleaseDC(GetHwnd(), dc);
        if (extentX > existingExtent)
            SendMessage(GetHwnd(), LB_SETHORIZONTALEXTENT, LOWORD(extentX), 0L);
    }
    else
    {
        int largestExtent = 0;
        HDC dc = GetWindowDC(GetHwnd());
        HFONT oldFont = 0;
        if (GetFont().Ok() && GetFont().GetResourceHandle())
            oldFont = (HFONT) ::SelectObject(dc, (HFONT) GetFont().GetResourceHandle());

        GetTextMetrics(dc, &lpTextMetric);
        int i;
        for (i = 0; i < m_noItems; i++)
        {
            int len = (int)SendMessage(GetHwnd(), LB_GETTEXT, i, (LONG)wxBuffer);
            wxBuffer[len] = 0;
            SIZE extentXY;
            ::GetTextExtentPoint(dc, (LPTSTR)wxBuffer, len, &extentXY);
            int extentX = (int)(extentXY.cx + lpTextMetric.tmAveCharWidth);
            if (extentX > largestExtent)
                largestExtent = extentX;
        }
        if (oldFont)
            ::SelectObject(dc, oldFont);

        ReleaseDC(GetHwnd(), dc);
        SendMessage(GetHwnd(), LB_SETHORIZONTALEXTENT, LOWORD(largestExtent), 0L);
    }
}

wxSize wxListBox::DoGetBestSize() const
{
    // find the widest string
    int wLine;
    int wListbox = 0;
    for ( int i = 0; i < m_noItems; i++ )
    {
        wxString str(GetString(i));
        GetTextExtent(str, &wLine, NULL);
        if ( wLine > wListbox )
            wListbox = wLine;
    }

    // give it some reasonable default value if there are no strings in the
    // list
    if ( wListbox == 0 )
        wListbox = 100;

    // the listbox should be slightly larger than the widest string
    int cx, cy;
    wxGetCharSize(GetHWND(), &cx, &cy, &GetFont());

    wListbox += 3*cx;

    // don't make the listbox too tall (limit height to 10 items) but don't
    // make it too small neither
    int hListbox = EDIT_HEIGHT_FROM_CHAR_HEIGHT(cy)*
                    wxMin(wxMax(m_noItems, 3), 10);

    return wxSize(wListbox, hListbox);
}

// ----------------------------------------------------------------------------
// callbacks
// ----------------------------------------------------------------------------

bool wxListBox::MSWCommand(WXUINT param, WXWORD WXUNUSED(id))
{
    wxEventType evtType;
    if ( param == LBN_SELCHANGE )
    {
        evtType = wxEVT_COMMAND_LISTBOX_SELECTED;
    }
    else if ( param == LBN_DBLCLK )
    {
        evtType = wxEVT_COMMAND_LISTBOX_DOUBLECLICKED;
    }
    else
    {
        // some event we're not interested in
        return FALSE;
    }

    wxCommandEvent event(evtType, m_windowId);
    event.SetEventObject( this );

    wxArrayInt aSelections;
    int n, count = GetSelections(aSelections);
    if ( count > 0 )
    {
        n = aSelections[0];
        if ( HasClientObjectData() )
            event.SetClientObject( GetClientObject(n) );
        else if ( HasClientUntypedData() )
            event.SetClientData( GetClientData(n) );
        event.SetString( GetString(n) );
    }
    else
    {
        n = -1;
    }

    event.m_commandInt = n;

    return GetEventHandler()->ProcessEvent(event);
}

// ----------------------------------------------------------------------------
// wxCheckListBox support
// ----------------------------------------------------------------------------

#if wxUSE_OWNER_DRAWN

// drawing
// -------

// space beneath/above each row in pixels
// "standard" checklistbox use 1 here, some might prefer 2. 0 is ugly.
#define OWNER_DRAWN_LISTBOX_EXTRA_SPACE    (1)

// the height is the same for all items
// TODO should be changed for LBS_OWNERDRAWVARIABLE style listboxes

// NB: can't forward this to wxListBoxItem because LB_SETITEMDATA
//     message is not yet sent when we get here!
bool wxListBox::MSWOnMeasure(WXMEASUREITEMSTRUCT *item)
{
    // only owner-drawn control should receive this message
    wxCHECK( ((m_windowStyle & wxLB_OWNERDRAW) == wxLB_OWNERDRAW), FALSE );

    MEASUREITEMSTRUCT *pStruct = (MEASUREITEMSTRUCT *)item;

    HDC hdc = CreateIC(wxT("DISPLAY"), NULL, NULL, 0);

    wxDC dc;
    dc.SetHDC((WXHDC)hdc);
    dc.SetFont(wxSystemSettings::GetFont(wxSYS_ANSI_VAR_FONT));

    pStruct->itemHeight = dc.GetCharHeight() + 2*OWNER_DRAWN_LISTBOX_EXTRA_SPACE;
    pStruct->itemWidth  = dc.GetCharWidth();

    dc.SetHDC(0);

    DeleteDC(hdc);

    return TRUE;
}

// forward the message to the appropriate item
bool wxListBox::MSWOnDraw(WXDRAWITEMSTRUCT *item)
{
    // only owner-drawn control should receive this message
    wxCHECK( ((m_windowStyle & wxLB_OWNERDRAW) == wxLB_OWNERDRAW), FALSE );

    DRAWITEMSTRUCT *pStruct = (DRAWITEMSTRUCT *)item;
    UINT itemID = pStruct->itemID;

    // the item may be -1 for an empty listbox
    if ( itemID == (UINT)-1 )
        return FALSE;

    long data = ListBox_GetItemData(GetHwnd(), pStruct->itemID);

    wxCHECK( data && (data != LB_ERR), FALSE );

    wxListBoxItem *pItem = (wxListBoxItem *)data;

    wxDCTemp dc((WXHDC)pStruct->hDC);
    wxRect rect(wxPoint(pStruct->rcItem.left, pStruct->rcItem.top),
                wxPoint(pStruct->rcItem.right, pStruct->rcItem.bottom));

    return pItem->OnDrawItem(dc, rect,
                             (wxOwnerDrawn::wxODAction)pStruct->itemAction,
                             (wxOwnerDrawn::wxODStatus)pStruct->itemState);
}

#endif // wxUSE_OWNER_DRAWN

#endif // wxUSE_LISTBOX
