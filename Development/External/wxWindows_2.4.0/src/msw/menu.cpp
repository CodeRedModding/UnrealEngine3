/////////////////////////////////////////////////////////////////////////////
// Name:        menu.cpp
// Purpose:     wxMenu, wxMenuBar, wxMenuItem
// Author:      Julian Smart
// Modified by: Vadim Zeitlin
// Created:     04/01/98
// RCS-ID:      $Id: menu.cpp,v 1.71.2.1 2002/12/09 09:29:00 JS Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "menu.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_MENUS

#ifndef WX_PRECOMP
    #include "wx/frame.h"
    #include "wx/menu.h"
    #include "wx/utils.h"
    #include "wx/intl.h"
    #include "wx/log.h"
#endif

#if wxUSE_OWNER_DRAWN
    #include "wx/ownerdrw.h"
#endif

#include "wx/msw/private.h"

// other standard headers
#include <string.h>

// ----------------------------------------------------------------------------
// global variables
// ----------------------------------------------------------------------------

extern wxMenu *wxCurrentPopupMenu;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// the (popup) menu title has this special id
static const int idMenuTitle = -2;

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

// make the given menu item default
static void SetDefaultMenuItem(HMENU hmenu, UINT id)
{
    MENUITEMINFO mii;
    wxZeroMemory(mii);
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STATE;
    mii.fState = MFS_DEFAULT;

    if ( !::SetMenuItemInfo(hmenu, id, FALSE, &mii) )
    {
        wxLogLastError(wxT("SetMenuItemInfo"));
    }
}

// ============================================================================
// implementation
// ============================================================================

IMPLEMENT_DYNAMIC_CLASS(wxMenu, wxEvtHandler)
IMPLEMENT_DYNAMIC_CLASS(wxMenuBar, wxWindow)

// ---------------------------------------------------------------------------
// wxMenu construction, adding and removing menu items
// ---------------------------------------------------------------------------

// Construct a menu with optional title (then use append)
void wxMenu::Init()
{
    m_doBreak = FALSE;
    m_startRadioGroup = -1;

    // create the menu
    m_hMenu = (WXHMENU)CreatePopupMenu();
    if ( !m_hMenu )
    {
        wxLogLastError(wxT("CreatePopupMenu"));
    }

    // if we have a title, insert it in the beginning of the menu
    if ( !!m_title )
    {
        Append(idMenuTitle, m_title);
        AppendSeparator();
    }
}

// The wxWindow destructor will take care of deleting the submenus.
wxMenu::~wxMenu()
{
    // we should free Windows resources only if Windows doesn't do it for us
    // which happens if we're attached to a menubar or a submenu of another
    // menu
    if ( !IsAttached() && !GetParent() )
    {
        if ( !::DestroyMenu(GetHmenu()) )
        {
            wxLogLastError(wxT("DestroyMenu"));
        }
    }

#if wxUSE_ACCEL
    // delete accels
    WX_CLEAR_ARRAY(m_accels);
#endif // wxUSE_ACCEL
}

void wxMenu::Break()
{
    // this will take effect during the next call to Append()
    m_doBreak = TRUE;
}

void wxMenu::Attach(wxMenuBarBase *menubar)
{
    wxMenuBase::Attach(menubar);

    EndRadioGroup();
}

#if wxUSE_ACCEL

int wxMenu::FindAccel(int id) const
{
    size_t n, count = m_accels.GetCount();
    for ( n = 0; n < count; n++ )
    {
        if ( m_accels[n]->m_command == id )
            return n;
    }

    return wxNOT_FOUND;
}

void wxMenu::UpdateAccel(wxMenuItem *item)
{
    if ( item->IsSubMenu() )
    {
        wxMenu *submenu = item->GetSubMenu();
        wxMenuItemList::Node *node = submenu->GetMenuItems().GetFirst();
        while ( node )
        {
            UpdateAccel(node->GetData());

            node = node->GetNext();
        }
    }
    else if ( !item->IsSeparator() )
    {
        // find the (new) accel for this item
        wxAcceleratorEntry *accel = wxGetAccelFromString(item->GetText());
        if ( accel )
            accel->m_command = item->GetId();

        // find the old one
        int n = FindAccel(item->GetId());
        if ( n == wxNOT_FOUND )
        {
            // no old, add new if any
            if ( accel )
                m_accels.Add(accel);
            else
                return;     // skipping RebuildAccelTable() below
        }
        else
        {
            // replace old with new or just remove the old one if no new
            delete m_accels[n];
            if ( accel )
                m_accels[n] = accel;
            else
                m_accels.RemoveAt(n);
        }

        if ( IsAttached() )
        {
            m_menuBar->RebuildAccelTable();
        }
    }
    //else: it is a separator, they can't have accels, nothing to do
}

#endif // wxUSE_ACCEL

// append a new item or submenu to the menu
bool wxMenu::DoInsertOrAppend(wxMenuItem *pItem, size_t pos)
{
#if wxUSE_ACCEL
    UpdateAccel(pItem);
#endif // wxUSE_ACCEL

    UINT flags = 0;

    // if "Break" has just been called, insert a menu break before this item
    // (and don't forget to reset the flag)
    if ( m_doBreak ) {
        flags |= MF_MENUBREAK;
        m_doBreak = FALSE;
    }

    if ( pItem->IsSeparator() ) {
        flags |= MF_SEPARATOR;
    }

    // id is the numeric id for normal menu items and HMENU for submenus as
    // required by ::AppendMenu() API
    UINT id;
    wxMenu *submenu = pItem->GetSubMenu();
    if ( submenu != NULL ) {
        wxASSERT_MSG( submenu->GetHMenu(), wxT("invalid submenu") );

        submenu->SetParent(this);

        id = (UINT)submenu->GetHMenu();

        flags |= MF_POPUP;
    }
    else {
        id = pItem->GetId();
    }

    LPCTSTR pData;

#if wxUSE_OWNER_DRAWN
    if ( pItem->IsOwnerDrawn() ) {  // want to get {Measure|Draw}Item messages?
        // item draws itself, pass pointer to it in data parameter
        flags |= MF_OWNERDRAW;
        pData = (LPCTSTR)pItem;
    }
    else
#endif
    {
        // menu is just a normal string (passed in data parameter)
        flags |= MF_STRING;

        pData = (wxChar*)pItem->GetText().c_str();
    }

    BOOL ok;
    if ( pos == (size_t)-1 )
    {
        ok = ::AppendMenu(GetHmenu(), flags, id, pData);
    }
    else
    {
        ok = ::InsertMenu(GetHmenu(), pos, flags | MF_BYPOSITION, id, pData);
    }

    if ( !ok )
    {
        wxLogLastError(wxT("Insert or AppendMenu"));

        return FALSE;
    }

    // if we just appended the title, highlight it
#ifdef __WIN32__
    if ( (int)id == idMenuTitle )
    {
        // visually select the menu title
        SetDefaultMenuItem(GetHmenu(), id);
    }
#endif // __WIN32__

    // if we're already attached to the menubar, we must update it
    if ( IsAttached() && m_menuBar->IsAttached() )
    {
        m_menuBar->Refresh();
    }

    return TRUE;
}

void wxMenu::EndRadioGroup()
{
    // we're not inside a radio group any longer
    m_startRadioGroup = -1;
}

bool wxMenu::DoAppend(wxMenuItem *item)
{
    wxCHECK_MSG( item, FALSE, _T("NULL item in wxMenu::DoAppend") );

    bool check = FALSE;

    if ( item->GetKind() == wxITEM_RADIO )
    {
        int count = GetMenuItemCount();

        if ( m_startRadioGroup == -1 )
        {
            // start a new radio group
            m_startRadioGroup = count;

            // for now it has just one element
            item->SetAsRadioGroupStart();
            item->SetRadioGroupEnd(m_startRadioGroup);

            // ensure that we have a checked item in the radio group
            check = TRUE;
        }
        else // extend the current radio group
        {
            // we need to update its end item
            item->SetRadioGroupStart(m_startRadioGroup);
            wxMenuItemList::Node *node = GetMenuItems().Item(m_startRadioGroup);

            if ( node )
            {
                node->GetData()->SetRadioGroupEnd(count);
            }
            else
            {
                wxFAIL_MSG( _T("where is the radio group start item?") );
            }
        }
    }
    else // not a radio item
    {
        EndRadioGroup();
    }

    if ( !wxMenuBase::DoAppend(item) || !DoInsertOrAppend(item) )
    {
        return FALSE;
    }

    if ( check )
    {
        // check the item initially
        item->Check(TRUE);
    }

    return TRUE;
}

bool wxMenu::DoInsert(size_t pos, wxMenuItem *item)
{
    return wxMenuBase::DoInsert(pos, item) && DoInsertOrAppend(item, pos);
}

wxMenuItem *wxMenu::DoRemove(wxMenuItem *item)
{
    // we need to find the items position in the child list
    size_t pos;
    wxMenuItemList::Node *node = GetMenuItems().GetFirst();
    for ( pos = 0; node; pos++ )
    {
        if ( node->GetData() == item )
            break;

        node = node->GetNext();
    }

    // DoRemove() (unlike Remove) can only be called for existing item!
    wxCHECK_MSG( node, NULL, wxT("bug in wxMenu::Remove logic") );

#if wxUSE_ACCEL
    // remove the corresponding accel from the accel table
    int n = FindAccel(item->GetId());
    if ( n != wxNOT_FOUND )
    {
        delete m_accels[n];

        m_accels.RemoveAt(n);
    }
    //else: this item doesn't have an accel, nothing to do
#endif // wxUSE_ACCEL

    // remove the item from the menu
    if ( !::RemoveMenu(GetHmenu(), (UINT)pos, MF_BYPOSITION) )
    {
        wxLogLastError(wxT("RemoveMenu"));
    }

    if ( IsAttached() && m_menuBar->IsAttached() )
    {
        // otherwise, the chane won't be visible
        m_menuBar->Refresh();
    }

    // and from internal data structures
    return wxMenuBase::DoRemove(item);
}

// ---------------------------------------------------------------------------
// accelerator helpers
// ---------------------------------------------------------------------------

#if wxUSE_ACCEL

// create the wxAcceleratorEntries for our accels and put them into provided
// array - return the number of accels we have
size_t wxMenu::CopyAccels(wxAcceleratorEntry *accels) const
{
    size_t count = GetAccelCount();
    for ( size_t n = 0; n < count; n++ )
    {
        *accels++ = *m_accels[n];
    }

    return count;
}

#endif // wxUSE_ACCEL

// ---------------------------------------------------------------------------
// set wxMenu title
// ---------------------------------------------------------------------------

void wxMenu::SetTitle(const wxString& label)
{
    bool hasNoTitle = m_title.IsEmpty();
    m_title = label;

    HMENU hMenu = GetHmenu();

    if ( hasNoTitle )
    {
        if ( !label.IsEmpty() )
        {
            if ( !::InsertMenu(hMenu, 0u, MF_BYPOSITION | MF_STRING,
                               (unsigned)idMenuTitle, m_title) ||
                 !::InsertMenu(hMenu, 1u, MF_BYPOSITION, (unsigned)-1, NULL) )
            {
                wxLogLastError(wxT("InsertMenu"));
            }
        }
    }
    else
    {
        if ( label.IsEmpty() )
        {
            // remove the title and the separator after it
            if ( !RemoveMenu(hMenu, 0, MF_BYPOSITION) ||
                 !RemoveMenu(hMenu, 0, MF_BYPOSITION) )
            {
                wxLogLastError(wxT("RemoveMenu"));
            }
        }
        else
        {
            // modify the title
            if ( !ModifyMenu(hMenu, 0u,
                             MF_BYPOSITION | MF_STRING,
                             (unsigned)idMenuTitle, m_title) )
            {
                wxLogLastError(wxT("ModifyMenu"));
            }
        }
    }

#ifdef __WIN32__
    // put the title string in bold face
    if ( !m_title.IsEmpty() )
    {
        SetDefaultMenuItem(GetHmenu(), (UINT)idMenuTitle);
    }
#endif // Win32
}

// ---------------------------------------------------------------------------
// event processing
// ---------------------------------------------------------------------------

bool wxMenu::MSWCommand(WXUINT WXUNUSED(param), WXWORD id)
{
    // ignore commands from the menu title

    // NB: VC++ generates wrong assembler for `if ( id != idMenuTitle )'!!
    if ( id != (WXWORD)idMenuTitle )
    {
        // VZ: previosuly, the command int was set to id too which was quite
        //     useless anyhow (as it could be retrieved using GetId()) and
        //     uncompatible with wxGTK, so now we use the command int instead
        //     to pass the checked status
        SendEvent(id, ::GetMenuState(GetHmenu(), id, MF_BYCOMMAND) & MF_CHECKED);
    }

    return TRUE;
}

// ---------------------------------------------------------------------------
// other
// ---------------------------------------------------------------------------

wxWindow *wxMenu::GetWindow() const
{
    if ( m_invokingWindow != NULL )
        return m_invokingWindow;
    else if ( m_menuBar != NULL)
        return m_menuBar->GetFrame();

    return NULL;
}

// ---------------------------------------------------------------------------
// Menu Bar
// ---------------------------------------------------------------------------

void wxMenuBar::Init()
{
    m_eventHandler = this;
    m_hMenu = 0;
}

wxMenuBar::wxMenuBar()
{
    Init();
}

wxMenuBar::wxMenuBar( long WXUNUSED(style) )
{
    Init();
}

wxMenuBar::wxMenuBar(int count, wxMenu *menus[], const wxString titles[])
{
    Init();

    m_titles.Alloc(count);

    for ( int i = 0; i < count; i++ )
    {
        m_menus.Append(menus[i]);
        m_titles.Add(titles[i]);

        menus[i]->Attach(this);
    }
}

wxMenuBar::~wxMenuBar()
{
    // we should free Windows resources only if Windows doesn't do it for us
    // which happens if we're attached to a frame
    if (m_hMenu && !IsAttached())
    {
        ::DestroyMenu((HMENU)m_hMenu);
        m_hMenu = (WXHMENU)NULL;
    }
}

// ---------------------------------------------------------------------------
// wxMenuBar helpers
// ---------------------------------------------------------------------------

void wxMenuBar::Refresh()
{
    wxCHECK_RET( IsAttached(), wxT("can't refresh unattached menubar") );

    DrawMenuBar(GetHwndOf(GetFrame()));
}

WXHMENU wxMenuBar::Create()
{
    if ( m_hMenu != 0 )
        return m_hMenu;

    m_hMenu = (WXHMENU)::CreateMenu();

    if ( !m_hMenu )
    {
        wxLogLastError(wxT("CreateMenu"));
    }
    else
    {
        size_t count = GetMenuCount();
        for ( size_t i = 0; i < count; i++ )
        {
            if ( !::AppendMenu((HMENU)m_hMenu, MF_POPUP | MF_STRING,
                               (UINT)m_menus[i]->GetHMenu(),
                               m_titles[i]) )
            {
                wxLogLastError(wxT("AppendMenu"));
            }
        }
    }

    return m_hMenu;
}

// ---------------------------------------------------------------------------
// wxMenuBar functions to work with the top level submenus
// ---------------------------------------------------------------------------

// NB: we don't support owner drawn top level items for now, if we do these
//     functions would have to be changed to use wxMenuItem as well

void wxMenuBar::EnableTop(size_t pos, bool enable)
{
    wxCHECK_RET( IsAttached(), wxT("doesn't work with unattached menubars") );

    int flag = enable ? MF_ENABLED : MF_GRAYED;

    EnableMenuItem((HMENU)m_hMenu, pos, MF_BYPOSITION | flag);

    Refresh();
}

void wxMenuBar::SetLabelTop(size_t pos, const wxString& label)
{
    wxCHECK_RET( pos < GetMenuCount(), wxT("invalid menu index") );

    m_titles[pos] = label;

    if ( !IsAttached() )
    {
        return;
    }
    //else: have to modify the existing menu

    UINT id;
    UINT flagsOld = ::GetMenuState((HMENU)m_hMenu, pos, MF_BYPOSITION);
    if ( flagsOld == 0xFFFFFFFF )
    {
        wxLogLastError(wxT("GetMenuState"));

        return;
    }

    if ( flagsOld & MF_POPUP )
    {
        // HIBYTE contains the number of items in the submenu in this case
        flagsOld &= 0xff;
        id = (UINT)::GetSubMenu((HMENU)m_hMenu, pos);
    }
    else
    {
        id = pos;
    }

    if ( ::ModifyMenu(GetHmenu(), pos, MF_BYPOSITION | MF_STRING | flagsOld,
                      id, label) == (int)0xFFFFFFFF )
    {
        wxLogLastError(wxT("ModifyMenu"));
    }

    Refresh();
}

wxString wxMenuBar::GetLabelTop(size_t pos) const
{
    wxCHECK_MSG( pos < GetMenuCount(), wxEmptyString,
                 wxT("invalid menu index in wxMenuBar::GetLabelTop") );

    return m_titles[pos];
}

// ---------------------------------------------------------------------------
// wxMenuBar construction
// ---------------------------------------------------------------------------

wxMenu *wxMenuBar::Replace(size_t pos, wxMenu *menu, const wxString& title)
{
    wxMenu *menuOld = wxMenuBarBase::Replace(pos, menu, title);
    if ( !menuOld )
        return NULL;

    m_titles[pos] = title;

    if ( IsAttached() )
    {
        // can't use ModifyMenu() because it deletes the submenu it replaces
        if ( !::RemoveMenu(GetHmenu(), (UINT)pos, MF_BYPOSITION) )
        {
            wxLogLastError(wxT("RemoveMenu"));
        }

        if ( !::InsertMenu(GetHmenu(), (UINT)pos,
                           MF_BYPOSITION | MF_POPUP | MF_STRING,
                           (UINT)GetHmenuOf(menu), title) )
        {
            wxLogLastError(wxT("InsertMenu"));
        }

#if wxUSE_ACCEL
        if ( menuOld->HasAccels() || menu->HasAccels() )
        {
            // need to rebuild accell table
            RebuildAccelTable();
        }
#endif // wxUSE_ACCEL

        Refresh();
    }

    return menuOld;
}

bool wxMenuBar::Insert(size_t pos, wxMenu *menu, const wxString& title)
{
    if ( !wxMenuBarBase::Insert(pos, menu, title) )
        return FALSE;

    m_titles.Insert(title, pos);

    if ( IsAttached() )
    {
        if ( !::InsertMenu(GetHmenu(), pos,
                           MF_BYPOSITION | MF_POPUP | MF_STRING,
                           (UINT)GetHmenuOf(menu), title) )
        {
            wxLogLastError(wxT("InsertMenu"));
        }

#if wxUSE_ACCEL
        if ( menu->HasAccels() )
        {
            // need to rebuild accell table
            RebuildAccelTable();
        }
#endif // wxUSE_ACCEL

        Refresh();
    }

    return TRUE;
}

bool wxMenuBar::Append(wxMenu *menu, const wxString& title)
{
    WXHMENU submenu = menu ? menu->GetHMenu() : 0;
    wxCHECK_MSG( submenu, FALSE, wxT("can't append invalid menu to menubar") );

    if ( !wxMenuBarBase::Append(menu, title) )
        return FALSE;

    m_titles.Add(title);

    if ( IsAttached() )
    {
        if ( !::AppendMenu(GetHmenu(), MF_POPUP | MF_STRING,
                           (UINT)submenu, title) )
        {
            wxLogLastError(wxT("AppendMenu"));
        }

#if wxUSE_ACCEL
        if ( menu->HasAccels() )
        {
            // need to rebuild accell table
            RebuildAccelTable();
        }
#endif // wxUSE_ACCEL

        Refresh();
    }

    return TRUE;
}

wxMenu *wxMenuBar::Remove(size_t pos)
{
    wxMenu *menu = wxMenuBarBase::Remove(pos);
    if ( !menu )
        return NULL;

    if ( IsAttached() )
    {
        if ( !::RemoveMenu(GetHmenu(), (UINT)pos, MF_BYPOSITION) )
        {
            wxLogLastError(wxT("RemoveMenu"));
        }

#if wxUSE_ACCEL
        if ( menu->HasAccels() )
        {
            // need to rebuild accell table
            RebuildAccelTable();
        }
#endif // wxUSE_ACCEL

        Refresh();
    }

    m_titles.Remove(pos);

    return menu;
}

#if wxUSE_ACCEL

void wxMenuBar::RebuildAccelTable()
{
    // merge the accelerators of all menus into one accel table
    size_t nAccelCount = 0;
    size_t i, count = GetMenuCount();
    for ( i = 0; i < count; i++ )
    {
        nAccelCount += m_menus[i]->GetAccelCount();
    }

    if ( nAccelCount )
    {
        wxAcceleratorEntry *accelEntries = new wxAcceleratorEntry[nAccelCount];

        nAccelCount = 0;
        for ( i = 0; i < count; i++ )
        {
            nAccelCount += m_menus[i]->CopyAccels(&accelEntries[nAccelCount]);
        }

        m_accelTable = wxAcceleratorTable(nAccelCount, accelEntries);

        delete [] accelEntries;
    }
}

#endif // wxUSE_ACCEL

void wxMenuBar::Attach(wxFrame *frame)
{
    wxMenuBarBase::Attach(frame);

#if wxUSE_ACCEL
    RebuildAccelTable();
#endif // wxUSE_ACCEL
}

void wxMenuBar::Detach()
{
    wxMenuBarBase::Detach();
}

#endif // wxUSE_MENUS
