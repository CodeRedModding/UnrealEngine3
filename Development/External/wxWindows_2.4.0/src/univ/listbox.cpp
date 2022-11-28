/////////////////////////////////////////////////////////////////////////////
// Name:        univ/listbox.cpp
// Purpose:     wxListBox implementation
// Author:      Vadim Zeitlin
// Modified by:
// Created:     30.08.00
// RCS-ID:      $Id: listbox.cpp,v 1.12.2.1 2002/09/21 20:31:32 VZ Exp $
// Copyright:   (c) 2000 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "univlistbox.h"
#endif

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_LISTBOX

#ifndef WX_PRECOMP
    #include "wx/log.h"

    #include "wx/dcclient.h"
    #include "wx/listbox.h"
    #include "wx/validate.h"
#endif

#include "wx/univ/renderer.h"
#include "wx/univ/inphand.h"
#include "wx/univ/theme.h"

// ============================================================================
// implementation of wxListBox
// ============================================================================

IMPLEMENT_DYNAMIC_CLASS(wxListBox, wxControl)

BEGIN_EVENT_TABLE(wxListBox, wxListBoxBase)
    EVT_SIZE(wxListBox::OnSize)

    EVT_IDLE(wxListBox::OnIdle)
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
// construction
// ----------------------------------------------------------------------------

void wxListBox::Init()
{
    // will be calculated later when needed
    m_lineHeight = 0;
    m_itemsPerPage = 0;
    m_maxWidth = 0;
    m_scrollRangeY = 0;
    m_maxWidthItem = -1;

    // no items hence no current item
    m_current = -1;
    m_selAnchor = -1;
    m_currentChanged = FALSE;

    // no need to update anything initially
    m_updateCount = 0;

    // no scrollbars to show nor update
    m_updateScrollbarX =
    m_showScrollbarX =
    m_updateScrollbarY =
    m_showScrollbarY = FALSE;
}

bool wxListBox::Create(wxWindow *parent,
                       wxWindowID id,
                       const wxPoint &pos,
                       const wxSize &size,
                       int n,
                       const wxString choices[],
                       long style,
                       const wxValidator& validator,
                       const wxString &name)
{
    // for compatibility accept both the new and old styles - they mean the
    // same thing for us
    if ( style & wxLB_ALWAYS_SB )
        style |= wxALWAYS_SHOW_SB;

    // if we don't have neither multiple nor extended flag, we must have the
    // single selection listbox
    if ( !(style & (wxLB_MULTIPLE | wxLB_EXTENDED)) )
        style |= wxLB_SINGLE;

#if wxUSE_TWO_WINDOWS
    style |=  wxVSCROLL|wxHSCROLL;
    if ((style & wxBORDER_MASK) == 0)
        style |= wxBORDER_SUNKEN;
#endif

    if ( !wxControl::Create(parent, id, pos, size, style, 
                            wxDefaultValidator, name) )
        return FALSE;

    SetWindow(this);

    if ( style & wxLB_SORT )
        m_strings = wxArrayString(TRUE /* auto sort */);

    Set(n, choices);

    SetBestSize(size);

    CreateInputHandler(wxINP_HANDLER_LISTBOX);

    return TRUE;
}

wxListBox::~wxListBox()
{
}

// ----------------------------------------------------------------------------
// adding/inserting strings
// ----------------------------------------------------------------------------

int wxListBox::DoAppend(const wxString& item)
{
    size_t index = m_strings.Add(item);
    m_itemsClientData.Insert(NULL, index);

    m_updateScrollbarY = TRUE;

    if ( HasHorzScrollbar() )
    {
        // has the max width increased?
        wxCoord width;
        GetTextExtent(item, &width, NULL);
        if ( width > m_maxWidth )
        {
            m_maxWidth = width;
            m_maxWidthItem = index;
            m_updateScrollbarX = TRUE;
        }
    }

    RefreshFromItemToEnd(index);

    return index;
}

void wxListBox::DoInsertItems(const wxArrayString& items, int pos)
{
    // the position of the item being added to a sorted listbox can't be
    // specified
    wxCHECK_RET( !IsSorted(), _T("can't insert items into sorted listbox") );

    size_t count = items.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        m_strings.Insert(items[n], pos + n);
        m_itemsClientData.Insert(NULL, pos + n);
    }

    // the number of items has changed so we might have to show the scrollbar
    m_updateScrollbarY = TRUE;

    // the max width also might have changed - just recalculate it instead of
    // keeping track of it here, this is probably more efficient for a typical
    // use pattern
    RefreshHorzScrollbar();

    // note that we have to refresh all the items after the ones we inserted,
    // not just these items
    RefreshFromItemToEnd(pos);
}

void wxListBox::DoSetItems(const wxArrayString& items, void **clientData)
{
    DoClear();

    size_t count = items.GetCount();
    if ( !count )
        return;

    m_strings.Alloc(count);
    m_itemsClientData.Alloc(count);
    for ( size_t n = 0; n < count; n++ )
    {
        size_t index = m_strings.Add(items[n]);
        m_itemsClientData.Insert(clientData ? clientData[n] : NULL, index);
    }

    m_updateScrollbarY = TRUE;

    RefreshAll();
}

void wxListBox::SetString(int n, const wxString& s)
{
    if ( HasHorzScrollbar() )
    {
        // we need to update m_maxWidth as changing the string may cause the
        // horz scrollbar [dis]appear
        wxCoord width;
        m_strings[n] = s;
        GetTextExtent(s, &width, NULL);

        // it might have increased if the new string is long
        if ( width > m_maxWidth )
        {
            m_maxWidth = width;
            m_maxWidthItem = n;
            m_updateScrollbarX = TRUE;
        }
        // or also decreased if the old string was the longest one
        else if ( n == m_maxWidthItem )
        {
            RefreshHorzScrollbar();
        }
    }
    else // no horz scrollbar
    {
        m_strings[n] = s;
    }

    RefreshItem(n);
}

// ----------------------------------------------------------------------------
// removing strings
// ----------------------------------------------------------------------------

void wxListBox::DoClear()
{
    m_strings.Clear();

    if ( HasClientObjectData() )
    {
        size_t count = m_itemsClientData.GetCount();
        for ( size_t n = 0; n < count; n++ )
        {
            delete (wxClientData *) m_itemsClientData[n];
        }
    }

    m_itemsClientData.Clear();
    m_selections.Clear();

    m_current = -1;
}

void wxListBox::Clear()
{
    DoClear();

    m_updateScrollbarY = TRUE;

    RefreshHorzScrollbar();

    RefreshAll();
}

void wxListBox::Delete(int n)
{
    wxCHECK_RET( n < GetCount(), _T("invalid index in wxListBox::Delete") );

    // do it before removing the index as otherwise the last item will not be
    // refreshed (as GetCount() will be decremented)
    RefreshFromItemToEnd(n);

    m_strings.RemoveAt(n);

    if ( HasClientObjectData() )
    {
        delete (wxClientData *)m_itemsClientData[n];
    }

    m_itemsClientData.RemoveAt(n);

    // when the item disappears we must not keep using its index
    if ( n == m_current )
    {
        m_current = -1;
    }
    else if ( n < m_current )
    {
        m_current--;
    }
    //else: current item may stay

    // update the selections array: the indices of all seletected items after
    // the one being deleted must change and the item itselfm ust be removed
    int index = wxNOT_FOUND;
    size_t count = m_selections.GetCount();
    for ( size_t item = 0; item < count; item++ )
    {
        if ( m_selections[item] == n )
        {
            // remember to delete it later
            index = item;
        }
        else if ( m_selections[item] > n )
        {
            // to account for the index shift
            m_selections[item]--;
        }
        //else: nothing changed for this one
    }

    if ( index != wxNOT_FOUND )
    {
        m_selections.RemoveAt(index);
    }

    // the number of items has changed, hence the scrollbar may disappear
    m_updateScrollbarY = TRUE;

    // finally, if the longest item was deleted the scrollbar may disappear
    if ( n == m_maxWidthItem )
    {
        RefreshHorzScrollbar();
    }
}

// ----------------------------------------------------------------------------
// client data handling
// ----------------------------------------------------------------------------

void wxListBox::DoSetItemClientData(int n, void* clientData)
{
    m_itemsClientData[n] = clientData;
}

void *wxListBox::DoGetItemClientData(int n) const
{
    return m_itemsClientData[n];
}

void wxListBox::DoSetItemClientObject(int n, wxClientData* clientData)
{
    m_itemsClientData[n] = clientData;
}

wxClientData* wxListBox::DoGetItemClientObject(int n) const
{
    return (wxClientData *)m_itemsClientData[n];
}

// ----------------------------------------------------------------------------
// selection
// ----------------------------------------------------------------------------

void wxListBox::SetSelection(int n, bool select)
{
    if ( select )
    {
        if ( m_selections.Index(n) == wxNOT_FOUND )
        {
            if ( !HasMultipleSelection() )
            {
                // selecting an item in a single selection listbox deselects
                // all the others
                DeselectAll();
            }

            m_selections.Add(n);

            RefreshItem(n);
        }
        //else: already selected
    }
    else // unselect
    {
        int index = m_selections.Index(n);
        if ( index != wxNOT_FOUND )
        {
            m_selections.RemoveAt(index);

            RefreshItem(n);
        }
        //else: not selected
    }

    // sanity check: a single selection listbox can't have more than one item
    // selected
    wxASSERT_MSG( HasMultipleSelection() || (m_selections.GetCount() < 2),
                  _T("multiple selected items in single selection lbox?") );

    if ( select )
    {
        // the newly selected item becomes the current one
        SetCurrentItem(n);
    }
}

int wxListBox::GetSelection() const
{
    wxCHECK_MSG( !HasMultipleSelection(), -1,
                 _T("use wxListBox::GetSelections for ths listbox") );

    return m_selections.IsEmpty() ? -1 : m_selections[0];
}

int wxCMPFUNC_CONV wxCompareInts(int *n, int *m)
{
    return *n - *m;
}

int wxListBox::GetSelections(wxArrayInt& selections) const
{
    // always return sorted array to the user
    selections = m_selections;
    size_t count = m_selections.GetCount();

    // don't call sort on an empty array
    if ( count )
    {
        selections.Sort(wxCompareInts);
    }

    return count;
}

// ----------------------------------------------------------------------------
// refresh logic: we use delayed refreshing which allows to avoid multiple
// refreshes (and hence flicker) in case when several listbox items are
// added/deleted/changed subsequently
// ----------------------------------------------------------------------------

void wxListBox::RefreshFromItemToEnd(int from)
{
    RefreshItems(from, GetCount() - from);
}

void wxListBox::RefreshItems(int from, int count)
{
    switch ( m_updateCount )
    {
        case 0:
            m_updateFrom = from;
            m_updateCount = count;
            break;

        case -1:
            // we refresh everything anyhow
            break;

        default:
            // add these items to the others which we have to refresh
            if ( m_updateFrom < from )
            {
                count += from - m_updateFrom;
                if ( m_updateCount < count )
                    m_updateCount = count;
            }
            else // m_updateFrom >= from
            {
                int updateLast = wxMax(m_updateFrom + m_updateCount,
                                       from + count);
                m_updateFrom = from;
                m_updateCount = updateLast - m_updateFrom;
            }
    }
}

void wxListBox::RefreshItem(int n)
{
    switch ( m_updateCount )
    {
        case 0:
            // refresh this item only
            m_updateFrom = n;
            m_updateCount = 1;
            break;

        case -1:
            // we refresh everything anyhow
            break;

        default:
            // add this item to the others which we have to refresh
            if ( m_updateFrom < n )
            {
                if ( m_updateCount < n - m_updateFrom + 1 )
                    m_updateCount = n - m_updateFrom + 1;
            }
            else // n <= m_updateFrom
            {
                m_updateCount += m_updateFrom - n;
                m_updateFrom = n;
            }
    }
}

void wxListBox::RefreshAll()
{
    m_updateCount = -1;
}

void wxListBox::RefreshHorzScrollbar()
{
    m_maxWidth = 0; // recalculate it
    m_updateScrollbarX = TRUE;
}

void wxListBox::UpdateScrollbars()
{
    wxSize size = GetClientSize();

    // is our height enough to show all items?
    int nLines = GetCount();
    wxCoord lineHeight = GetLineHeight();
    bool showScrollbarY = nLines*lineHeight > size.y;

    // check the width too if required
    wxCoord charWidth, maxWidth;
    bool showScrollbarX;
    if ( HasHorzScrollbar() )
    {
        charWidth = GetCharWidth();
        maxWidth = GetMaxWidth();
        showScrollbarX = maxWidth > size.x;
    }
    else // never show it
    {
        charWidth = maxWidth = 0;
        showScrollbarX = FALSE;
    }

    // what should be the scrollbar range now?
    int scrollRangeX = showScrollbarX
                        ? (maxWidth + charWidth - 1) / charWidth + 2 // FIXME
                        : 0;
    int scrollRangeY = showScrollbarY
                        ? nLines +
                            (size.y % lineHeight + lineHeight - 1) / lineHeight
                        : 0;

    // reset scrollbars if something changed: either the visibility status
    // or the range of a scrollbar which is shown
    if ( (showScrollbarY != m_showScrollbarY) ||
         (showScrollbarX != m_showScrollbarX) ||
         (showScrollbarY && (scrollRangeY != m_scrollRangeY)) ||
         (showScrollbarX && (scrollRangeX != m_scrollRangeX)) )
    {
        int x, y;
        GetViewStart(&x, &y);
        SetScrollbars(charWidth, lineHeight,
                      scrollRangeX, scrollRangeY,
                      x, y);

        m_showScrollbarX = showScrollbarX;
        m_showScrollbarY = showScrollbarY;

        m_scrollRangeX = scrollRangeX;
        m_scrollRangeY = scrollRangeY;
    }
}

void wxListBox::UpdateItems()
{
    // only refresh the items which must be refreshed
    if ( m_updateCount == -1 )
    {
        // refresh all
        wxLogTrace(_T("listbox"), _T("Refreshing all"));

        Refresh();
    }
    else
    {
        wxSize size = GetClientSize();
        wxRect rect;
        rect.width = size.x;
        rect.height = size.y;
        rect.y += m_updateFrom*GetLineHeight();
        rect.height = m_updateCount*GetLineHeight();

        // we don't need to calculate x position as we always refresh the
        // entire line(s)
        CalcScrolledPosition(0, rect.y, NULL, &rect.y);

        wxLogTrace(_T("listbox"), _T("Refreshing items %d..%d (%d-%d)"),
                   m_updateFrom, m_updateFrom + m_updateCount - 1,
                   rect.GetTop(), rect.GetBottom());

        Refresh(TRUE, &rect);
    }
}

void wxListBox::OnIdle(wxIdleEvent& event)
{
    if ( m_updateScrollbarY || m_updateScrollbarX )
    {
        UpdateScrollbars();

        m_updateScrollbarX =
        m_updateScrollbarY = FALSE;
    }

    if ( m_currentChanged )
    {
        DoEnsureVisible(m_current);

        m_currentChanged = FALSE;
    }

    if ( m_updateCount )
    {
        UpdateItems();

        m_updateCount = 0;
    }

    event.Skip();
}

// ----------------------------------------------------------------------------
// drawing
// ----------------------------------------------------------------------------

wxBorder wxListBox::GetDefaultBorder() const
{
    return wxBORDER_SUNKEN;
}

void wxListBox::DoDraw(wxControlRenderer *renderer)
{
    // adjust the DC to account for scrolling
    wxDC& dc = renderer->GetDC();
    PrepareDC(dc);
    dc.SetFont(GetFont());

    // get the update rect
    wxRect rectUpdate = GetUpdateClientRect();

    int yTop, yBottom;
    CalcUnscrolledPosition(0, rectUpdate.GetTop(), NULL, &yTop);
    CalcUnscrolledPosition(0, rectUpdate.GetBottom(), NULL, &yBottom);

    // get the items which must be redrawn
    wxCoord lineHeight = GetLineHeight();
    size_t itemFirst = yTop / lineHeight,
           itemLast = (yBottom + lineHeight - 1) / lineHeight,
           itemMax = m_strings.GetCount();

    if ( itemFirst >= itemMax )
        return;

    if ( itemLast > itemMax )
        itemLast = itemMax;

    // do draw them
    wxLogTrace(_T("listbox"), _T("Repainting items %d..%d"),
               itemFirst, itemLast);

    DoDrawRange(renderer, itemFirst, itemLast);
}

void wxListBox::DoDrawRange(wxControlRenderer *renderer,
                            int itemFirst, int itemLast)
{
    renderer->DrawItems(this, itemFirst, itemLast);
}

// ----------------------------------------------------------------------------
// size calculations
// ----------------------------------------------------------------------------

bool wxListBox::SetFont(const wxFont& font)
{
    if ( !wxControl::SetFont(font) )
        return FALSE;

    CalcItemsPerPage();

    RefreshAll();

    return TRUE;
}

void wxListBox::CalcItemsPerPage()
{
    m_lineHeight = GetRenderer()->GetListboxItemHeight(GetCharHeight());
    m_itemsPerPage = GetClientSize().y / m_lineHeight;
}

int wxListBox::GetItemsPerPage() const
{
    if ( !m_itemsPerPage )
    {
        wxConstCast(this, wxListBox)->CalcItemsPerPage();
    }

    return m_itemsPerPage;
}

wxCoord wxListBox::GetLineHeight() const
{
    if ( !m_lineHeight )
    {
        wxConstCast(this, wxListBox)->CalcItemsPerPage();
    }

    return m_lineHeight;
}

wxCoord wxListBox::GetMaxWidth() const
{
    if ( m_maxWidth == 0 )
    {
        wxListBox *self = wxConstCast(this, wxListBox);
        wxCoord width;
        size_t count = m_strings.GetCount();
        for ( size_t n = 0; n < count; n++ )
        {
            GetTextExtent(m_strings[n], &width, NULL);
            if ( width > m_maxWidth )
            {
                self->m_maxWidth = width;
                self->m_maxWidthItem = n;
            }
        }
    }

    return m_maxWidth;
}

void wxListBox::OnSize(wxSizeEvent& event)
{
    // recalculate the number of items per page
    CalcItemsPerPage();

    // the scrollbars might [dis]appear
    m_updateScrollbarX =
    m_updateScrollbarY = TRUE;

    event.Skip();
}

void wxListBox::DoSetFirstItem(int n)
{
    SetCurrentItem(n);
}

void wxListBox::DoSetSize(int x, int y,
                          int width, int height,
                          int sizeFlags)
{
    if ( GetWindowStyle() & wxLB_INT_HEIGHT )
    {
        // we must round up the height to an entire number of rows

        // the client area must contain an int number of rows, so take borders
        // into account
        wxRect rectBorders = GetRenderer()->GetBorderDimensions(GetBorder());
        wxCoord hBorders = rectBorders.y + rectBorders.height;

        wxCoord hLine = GetLineHeight();
        height = ((height - hBorders + hLine - 1) / hLine)*hLine + hBorders;
    }

    wxListBoxBase::DoSetSize(x, y, width, height);
}

wxSize wxListBox::DoGetBestClientSize() const
{
    wxCoord width = 0,
            height = 0;

    size_t count = m_strings.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        wxCoord w,h;
        GetTextExtent(m_strings[n], &w, &h);

        if ( w > width )
            width = w;
        if ( h > height )
            height = h;
    }

    // if the listbox is empty, still give it some non zero (even if
    // arbitrary) size - otherwise, leave small margin around the strings
    if ( !width )
        width = 100;
    else
        width += 3*GetCharWidth();

    if ( !height )
        height = GetCharHeight();

    // we need the height of the entire listbox, not just of one line
    height *= wxMax(count, 7);

    return wxSize(width, height);
}

// ----------------------------------------------------------------------------
// listbox actions
// ----------------------------------------------------------------------------

bool wxListBox::SendEvent(wxEventType type, int item)
{
    wxCommandEvent event(type, m_windowId);
    event.SetEventObject(this);

    // use the current item by default
    if ( item == -1 )
    {
        item = m_current;
    }

    // client data and string parameters only make sense if we have an item
    if ( item != -1 )
    {
        if ( HasClientObjectData() )
            event.SetClientObject(GetClientObject(item));
        else if ( HasClientUntypedData() )
            event.SetClientData(GetClientData(item));

        event.SetString(GetString(item));
    }

    event.m_commandInt = item;

    return GetEventHandler()->ProcessEvent(event);
}

void wxListBox::SetCurrentItem(int n)
{
    if ( n != m_current )
    {
        if ( m_current != -1 )
            RefreshItem(m_current);

        m_current = n;

        if ( m_current != -1 )
        {
            m_currentChanged = TRUE;

            RefreshItem(m_current);
        }
    }
    //else: nothing to do
}

bool wxListBox::FindItem(const wxString& prefix, bool strictlyAfter)
{
    int count = GetCount();
    if ( !count )
    {
        // empty listbox, we can't find anything in it
        return FALSE;
    }

    // start either from the current item or from the next one if strictlyAfter
    // is true
    int first;
    if ( strictlyAfter )
    {
        // the following line will set first correctly to 0 if there is no
        // selection (m_current == -1)
        first = m_current == count - 1 ? 0 : m_current + 1;
    }
    else // start with the current
    {
        first = m_current == -1 ? 0 : m_current;
    }

    int last = first == 0 ? count - 1 : first - 1;

    // if this is not true we'd never exit from the loop below!
    wxASSERT_MSG( first < count && last < count, _T("logic error") );

    // precompute it outside the loop
    size_t len = prefix.length();

    // loop over all items in the listbox
    for ( int item = first; item != last; item < count - 1 ? item++ : item = 0 )
    {
        if ( wxStrnicmp(m_strings[item], prefix, len) == 0 )
        {
            SetCurrentItem(item);

            if ( !(GetWindowStyle() & wxLB_MULTIPLE) )
            {
                DeselectAll(item);
                SelectAndNotify(item);

                if ( GetWindowStyle() & wxLB_EXTENDED )
                    AnchorSelection(item);
            }

            return TRUE;
        }
    }

    // nothing found
    return FALSE;
}

void wxListBox::EnsureVisible(int n)
{
    if ( m_updateScrollbarY )
    {
        UpdateScrollbars();

        m_updateScrollbarX =
        m_updateScrollbarY = FALSE;
    }

    DoEnsureVisible(n);
}

void wxListBox::DoEnsureVisible(int n)
{
    if ( !m_showScrollbarY )
    {
        // nothing to do - everything is shown anyhow
        return;
    }

    int first;
    GetViewStart(0, &first);
    if ( first > n )
    {
        // we need to scroll upwards, so make the current item appear on top
        // of the shown range
        Scroll(0, n);
    }
    else
    {
        int last = first + GetClientSize().y / GetLineHeight() - 1;
        if ( last < n )
        {
            // scroll down: the current item appears at the bottom of the
            // range
            Scroll(0, n - (last - first));
        }
    }
}

void wxListBox::ChangeCurrent(int diff)
{
    int current = m_current == -1 ? 0 : m_current;

    current += diff;

    int last = GetCount() - 1;
    if ( current < 0 )
        current = 0;
    else if ( current > last )
        current = last;

    SetCurrentItem(current);
}

void wxListBox::ExtendSelection(int itemTo)
{
    // if we don't have the explicit values for selection start/end, make them
    // up
    if ( m_selAnchor == -1 )
        m_selAnchor = m_current;

    if ( itemTo == -1 )
        itemTo = m_current;

    // swap the start/end of selection range if necessary
    int itemFrom = m_selAnchor;
    if ( itemFrom > itemTo )
    {
        int itemTmp = itemFrom;
        itemFrom = itemTo;
        itemTo = itemTmp;
    }

    // the selection should now include all items in the range between the
    // anchor and the specified item and only them

    int n;
    for ( n = 0; n < itemFrom; n++ )
    {
        Deselect(n);
    }

    for ( ; n <= itemTo; n++ )
    {
        SetSelection(n);
    }

    int count = GetCount();
    for ( ; n < count; n++ )
    {
        Deselect(n);
    }
}

void wxListBox::DoSelect(int item, bool sel)
{
    if ( item != -1 )
    {
        // go to this item first
        SetCurrentItem(item);
    }

    // the current item is the one we want to change: either it was just
    // changed above to be the same as item or item == -1 in which we case we
    // are supposed to use the current one anyhow
    if ( m_current != -1 )
    {
        // [de]select it
        SetSelection(m_current, sel);
    }
}

void wxListBox::SelectAndNotify(int item)
{
    DoSelect(item);

    SendEvent(wxEVT_COMMAND_LISTBOX_SELECTED);
}

void wxListBox::Activate(int item)
{
    if ( item != -1 )
        SetCurrentItem(item);
    else
        item = m_current;

    if ( !(GetWindowStyle() & wxLB_MULTIPLE) )
    {
        DeselectAll(item);
    }

    if ( item != -1 )
    {
        DoSelect(item);

        SendEvent(wxEVT_COMMAND_LISTBOX_DOUBLECLICKED);
    }
}

// ----------------------------------------------------------------------------
// input handling
// ----------------------------------------------------------------------------

/*
   The numArg here is the listbox item index while the strArg is used
   differently for the different actions:

   a) for wxACTION_LISTBOX_FIND it has the natural meaning: this is the string
      to find

   b) for wxACTION_LISTBOX_SELECT and wxACTION_LISTBOX_EXTENDSEL it is used
      to decide if the listbox should send the notification event (it is empty)
      or not (it is not): this allows us to reuse the same action for when the
      user is dragging the mouse when it has been released although in the
      first case no notification is sent while in the second it is sent.
 */
bool wxListBox::PerformAction(const wxControlAction& action,
                              long numArg,
                              const wxString& strArg)
{
    int item = (int)numArg;

    if ( action == wxACTION_LISTBOX_SETFOCUS )
    {
        SetCurrentItem(item);
    }
    else if ( action == wxACTION_LISTBOX_ACTIVATE )
    {
        Activate(item);
    }
    else if ( action == wxACTION_LISTBOX_TOGGLE )
    {
        if ( item == -1 )
            item = m_current;

        if ( IsSelected(item) )
            DoUnselect(item);
        else
            SelectAndNotify(item);
    }
    else if ( action == wxACTION_LISTBOX_SELECT )
    {
        DeselectAll(item);

        if ( strArg.empty() )
            SelectAndNotify(item);
        else
            DoSelect(item);
    }
    else if ( action == wxACTION_LISTBOX_SELECTADD )
        DoSelect(item);
    else if ( action == wxACTION_LISTBOX_UNSELECT )
        DoUnselect(item);
    else if ( action == wxACTION_LISTBOX_MOVEDOWN )
        ChangeCurrent(1);
    else if ( action == wxACTION_LISTBOX_MOVEUP )
        ChangeCurrent(-1);
    else if ( action == wxACTION_LISTBOX_PAGEDOWN )
        ChangeCurrent(GetItemsPerPage());
    else if ( action == wxACTION_LISTBOX_PAGEUP )
        ChangeCurrent(-GetItemsPerPage());
    else if ( action == wxACTION_LISTBOX_START )
        SetCurrentItem(0);
    else if ( action == wxACTION_LISTBOX_END )
        SetCurrentItem(GetCount() - 1);
    else if ( action == wxACTION_LISTBOX_UNSELECTALL )
        DeselectAll(item);
    else if ( action == wxACTION_LISTBOX_EXTENDSEL )
        ExtendSelection(item);
    else if ( action == wxACTION_LISTBOX_FIND )
        FindNextItem(strArg);
    else if ( action == wxACTION_LISTBOX_ANCHOR )
        AnchorSelection(item == -1 ? m_current : item);
    else if ( action == wxACTION_LISTBOX_SELECTALL ||
              action == wxACTION_LISTBOX_SELTOGGLE )
        wxFAIL_MSG(_T("unimplemented yet"));
    else
        return wxControl::PerformAction(action, numArg, strArg);

    return TRUE;
}

// ============================================================================
// implementation of wxStdListboxInputHandler
// ============================================================================

wxStdListboxInputHandler::wxStdListboxInputHandler(wxInputHandler *handler,
                                                   bool toggleOnPressAlways)
                        : wxStdInputHandler(handler)
{
    m_btnCapture = 0;
    m_toggleOnPressAlways = toggleOnPressAlways;
    m_actionMouse = wxACTION_NONE;
    m_trackMouseOutside = TRUE;
}

int wxStdListboxInputHandler::HitTest(const wxListBox *lbox,
                                      const wxMouseEvent& event)
{
    int item = HitTestUnsafe(lbox, event);

    return FixItemIndex(lbox, item);
}

int wxStdListboxInputHandler::HitTestUnsafe(const wxListBox *lbox,
                                            const wxMouseEvent& event)
{
    wxPoint pt = event.GetPosition();
    pt -= lbox->GetClientAreaOrigin();
    int y;
    lbox->CalcUnscrolledPosition(0, pt.y, NULL, &y);
    return y / lbox->GetLineHeight();
}

int wxStdListboxInputHandler::FixItemIndex(const wxListBox *lbox,
                                           int item)
{
    if ( item < 0 )
    {
        // mouse is above the first item
        item = 0;
    }
    else if ( item >= lbox->GetCount() )
    {
        // mouse is below the last item
        item = lbox->GetCount() - 1;
    }

    return item;
}

bool wxStdListboxInputHandler::IsValidIndex(const wxListBox *lbox, int item)
{
    return item >= 0 && item < lbox->GetCount();
}

wxControlAction
wxStdListboxInputHandler::SetupCapture(wxListBox *lbox,
                                       const wxMouseEvent& event,
                                       int item)
{
    // we currently only allow selecting with the left mouse button, if we
    // do need to allow using other buttons too we might use the code
    // inside #if 0
#if 0
    m_btnCapture = event.LeftDown()
                    ? 1
                    : event.RightDown()
                        ? 3
                        : 2;
#else
    m_btnCapture = 1;
#endif // 0/1

    wxControlAction action;
    if ( lbox->HasMultipleSelection() )
    {
        if ( lbox->GetWindowStyle() & wxLB_MULTIPLE )
        {
            if ( m_toggleOnPressAlways )
            {
                // toggle the item right now
                action = wxACTION_LISTBOX_TOGGLE;
            }
            //else: later

            m_actionMouse = wxACTION_LISTBOX_SETFOCUS;
        }
        else // wxLB_EXTENDED listbox
        {
            // simple click in an extended sel listbox clears the old
            // selection and adds the clicked item to it then, ctrl-click
            // toggles an item to it and shift-click adds a range between
            // the old selection anchor and the clicked item
            if ( event.ControlDown() )
            {
                lbox->PerformAction(wxACTION_LISTBOX_ANCHOR, item);

                action = wxACTION_LISTBOX_TOGGLE;
            }
            else if ( event.ShiftDown() )
            {
                action = wxACTION_LISTBOX_EXTENDSEL;
            }
            else // simple click
            {
                lbox->PerformAction(wxACTION_LISTBOX_ANCHOR, item);

                action = wxACTION_LISTBOX_SELECT;
            }

            m_actionMouse = wxACTION_LISTBOX_EXTENDSEL;
        }
    }
    else // single selection
    {
        m_actionMouse =
        action = wxACTION_LISTBOX_SELECT;
    }

    // by default we always do track it
    m_trackMouseOutside = TRUE;

    return action;
}

bool wxStdListboxInputHandler::HandleKey(wxInputConsumer *consumer,
                                         const wxKeyEvent& event,
                                         bool pressed)
{
    // we're only interested in the key press events
    if ( pressed && !event.AltDown() )
    {
        bool isMoveCmd = TRUE;
        int style = consumer->GetInputWindow()->GetWindowStyle();

        wxControlAction action;
        wxString strArg;

        int keycode = event.GetKeyCode();
        switch ( keycode )
        {
            // movement
            case WXK_UP:
                action = wxACTION_LISTBOX_MOVEUP;
                break;

            case WXK_DOWN:
                action = wxACTION_LISTBOX_MOVEDOWN;
                break;

            case WXK_PAGEUP:

            case WXK_PRIOR:
                action = wxACTION_LISTBOX_PAGEUP;
                break;

            case WXK_PAGEDOWN:

            case WXK_NEXT:
                action = wxACTION_LISTBOX_PAGEDOWN;
                break;

            case WXK_HOME:
                action = wxACTION_LISTBOX_START;
                break;

            case WXK_END:
                action = wxACTION_LISTBOX_END;
                break;

            // selection
            case WXK_SPACE:
                if ( style & wxLB_MULTIPLE )
                {
                    action = wxACTION_LISTBOX_TOGGLE;
                    isMoveCmd = FALSE;
                }
                break;

            case WXK_RETURN:
                action = wxACTION_LISTBOX_ACTIVATE;
                isMoveCmd = FALSE;
                break;

            default:
                if ( (keycode < 255) && wxIsalnum(keycode) )
                {
                    action = wxACTION_LISTBOX_FIND;
                    strArg = (wxChar)keycode;
                }
        }

        if ( !!action )
        {
            consumer->PerformAction(action, -1, strArg);

            if ( isMoveCmd )
            {
                if ( style & wxLB_SINGLE )
                {
                    // the current item is always the one selected
                    consumer->PerformAction(wxACTION_LISTBOX_SELECT);
                }
                else if ( style & wxLB_EXTENDED )
                {
                    if ( event.ShiftDown() )
                        consumer->PerformAction(wxACTION_LISTBOX_EXTENDSEL);
                    else
                    {
                        // select the item and make it the new selection anchor
                        consumer->PerformAction(wxACTION_LISTBOX_SELECT);
                        consumer->PerformAction(wxACTION_LISTBOX_ANCHOR);
                    }
                }
                //else: nothing to do for multiple selection listboxes
            }

            return TRUE;
        }
    }

    return wxStdInputHandler::HandleKey(consumer, event, pressed);
}

bool wxStdListboxInputHandler::HandleMouse(wxInputConsumer *consumer,
                                           const wxMouseEvent& event)
{
    wxListBox *lbox = wxStaticCast(consumer->GetInputWindow(), wxListBox);
    int item = HitTest(lbox, event);
    wxControlAction action;

    // when the left mouse button is pressed, capture the mouse and track the
    // item under mouse (if the mouse leaves the window, we will still be
    // getting the mouse move messages generated by wxScrollWindow)
    if ( event.LeftDown() )
    {
        // capture the mouse to track the selected item
        lbox->CaptureMouse();

        action = SetupCapture(lbox, event, item);
    }
    else if ( m_btnCapture && event.ButtonUp(m_btnCapture) )
    {
        // when the left mouse button is released, release the mouse too
        wxWindow *winCapture = wxWindow::GetCapture();
        if ( winCapture )
        {
            winCapture->ReleaseMouse();
            m_btnCapture = 0;

            action = m_actionMouse;
        }
        //else: the mouse wasn't presed over the listbox, only released here
    }
    else if ( event.LeftDClick() )
    {
        action = wxACTION_LISTBOX_ACTIVATE;
    }

    if ( !!action )
    {
        lbox->PerformAction(action, item);

        return TRUE;
    }

    return wxStdInputHandler::HandleMouse(consumer, event);
}

bool wxStdListboxInputHandler::HandleMouseMove(wxInputConsumer *consumer,
                                               const wxMouseEvent& event)
{
    wxWindow *winCapture = wxWindow::GetCapture();
    if ( winCapture && (event.GetEventObject() == winCapture) )
    {
        wxListBox *lbox = wxStaticCast(consumer->GetInputWindow(), wxListBox);

        if ( !m_btnCapture || !m_trackMouseOutside )
        {
            // someone captured the mouse for us (we always set m_btnCapture
            // when we do it ourselves): in this case we only react to
            // the mouse messages when they happen inside the listbox
            if ( lbox->HitTest(event.GetPosition()) != wxHT_WINDOW_INSIDE )
                return FALSE;
        }

        int item = HitTest(lbox, event);
        if ( !m_btnCapture )
        {
            // now that we have the mouse inside the listbox, do capture it
            // normally - but ensure that we will still ignore the outside
            // events
            SetupCapture(lbox, event, item);

            m_trackMouseOutside = FALSE;
        }

        if ( IsValidIndex(lbox, item) )
        {
            // pass something into strArg to tell the listbox that it shouldn't
            // send the notification message: see PerformAction() above
            lbox->PerformAction(m_actionMouse, item, _T("no"));
        }
        // else: don't pass invalid index to the listbox
    }
    else // we don't have capture any more
    {
        if ( m_btnCapture )
        {
            // if we lost capture unexpectedly (someone else took the capture
            // from us), return to a consistent state
            m_btnCapture = 0;
        }
    }

    return wxStdInputHandler::HandleMouseMove(consumer, event);
}

#endif // wxUSE_LISTBOX
