///////////////////////////////////////////////////////////////////////////////
// Name:        common/menucmn.cpp
// Purpose:     wxMenu and wxMenuBar methods common to all ports
// Author:      Vadim Zeitlin
// Modified by:
// Created:     26.10.99
// RCS-ID:      $Id: menucmn.cpp,v 1.18.2.2 2002/11/19 18:54:46 RD Exp $
// Copyright:   (c) wxWindows team
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "menubase.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_MENUS

#include <ctype.h>

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/menu.h"
#endif

// ----------------------------------------------------------------------------
// template lists
// ----------------------------------------------------------------------------

#include "wx/listimpl.cpp"

WX_DEFINE_LIST(wxMenuList);
WX_DEFINE_LIST(wxMenuItemList);

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxMenuItem
// ----------------------------------------------------------------------------

wxMenuItemBase::wxMenuItemBase(wxMenu *parentMenu,
                               int id,
                               const wxString& text,
                               const wxString& help,
                               wxItemKind kind,
                               wxMenu *subMenu)
              : m_text(text),
                m_help(help)
{
    wxASSERT_MSG( parentMenu != NULL, wxT("menuitem should have a menu") );

    m_parentMenu  = parentMenu;
    m_subMenu     = subMenu;
    m_isEnabled   = TRUE;
    m_isChecked   = FALSE;
    m_id          = id;
    m_kind        = kind;
}

wxMenuItemBase::~wxMenuItemBase()
{
    delete m_subMenu;
}

#if wxUSE_ACCEL

// return wxAcceleratorEntry for the given menu string or NULL if none
// specified
wxAcceleratorEntry *wxGetAccelFromString(const wxString& label)
{
    // check for accelerators: they are given after '\t'
    int posTab = label.Find(wxT('\t'));
    if ( posTab != wxNOT_FOUND ) {
        // parse the accelerator string
        int keyCode = 0;
        int accelFlags = wxACCEL_NORMAL;
        wxString current;
        for ( size_t n = (size_t)posTab + 1; n < label.Len(); n++ ) {
            if ( (label[n] == '+') || (label[n] == '-') ) {
                if ( current == _("ctrl") )
                    accelFlags |= wxACCEL_CTRL;
                else if ( current == _("alt") )
                    accelFlags |= wxACCEL_ALT;
                else if ( current == _("shift") )
                    accelFlags |= wxACCEL_SHIFT;
                else {
                    // we may have "Ctrl-+", for example, but we still want to
                    // catch typos like "Crtl-A" so only give the warning if we
                    // have something before the current '+' or '-', else take
                    // it as a literal symbol
                    if ( current.empty() )
                    {
                        current += label[n];

                        // skip clearing it below
                        continue;
                    }
                    else
                    {
                        wxLogDebug(wxT("Unknown accel modifier: '%s'"),
                                   current.c_str());
                    }
                }

                current.clear();
            }
            else {
                current += wxTolower(label[n]);
            }
        }

        if ( current.IsEmpty() ) {
            wxLogDebug(wxT("No accel key found, accel string ignored."));
        }
        else {
            if ( current.Len() == 1 ) {
                // it's a letter
                keyCode = wxToupper(current[0U]);
            }
            else {
                // is it a function key?
                if ( current[0U] == 'f' && isdigit(current[1U]) &&
                     (current.Len() == 2 ||
                     (current.Len() == 3 && isdigit(current[2U]))) ) {
                    int n;
                    wxSscanf(current.c_str() + 1, wxT("%d"), &n);

                    keyCode = WXK_F1 + n - 1;
                }
                else {
                    // several special cases
                    current.MakeUpper();
                    if ( current == wxT("DEL") ) {
                        keyCode = WXK_DELETE;
                    }
                    else if ( current == wxT("DELETE") ) {
                        keyCode = WXK_DELETE;
                    }
                    else if ( current == wxT("INS") ) {
                        keyCode = WXK_INSERT;
                    }
                    else if ( current == wxT("INSERT") ) {
                        keyCode = WXK_INSERT;
                    }
                    else if ( current == wxT("ENTER") || current == wxT("RETURN") ) {
                        keyCode = WXK_RETURN;
                    }
                    else if ( current == wxT("PGUP") ) {
                        keyCode = WXK_PRIOR;
                    }
                    else if ( current == wxT("PGDN") ) {
                        keyCode = WXK_NEXT;
                    }
                    else if ( current == wxT("LEFT") ) {
                        keyCode = WXK_LEFT;
                    }
                    else if ( current == wxT("RIGHT") ) {
                        keyCode = WXK_RIGHT;
                    }
                    else if ( current == wxT("UP") ) {
                        keyCode = WXK_UP;
                    }
                    else if ( current == wxT("DOWN") ) {
                        keyCode = WXK_DOWN;
                    }
                    else if ( current == wxT("HOME") ) {
                        keyCode = WXK_HOME;
                    }
                    else if ( current == wxT("END") ) {
                        keyCode = WXK_END;
                    }
                    else if ( current == wxT("SPACE") ) {
                        keyCode = WXK_SPACE;
                    }
                    else if ( current == wxT("TAB") ) {
                        keyCode = WXK_TAB;
                    }
                    else
                    {
                        wxLogDebug(wxT("Unrecognized accel key '%s', accel string ignored."),
                                   current.c_str());
                    }
                }
            }
        }

        if ( keyCode ) {
            // we do have something
            return new wxAcceleratorEntry(accelFlags, keyCode);
        }
    }

    return (wxAcceleratorEntry *)NULL;
}

wxAcceleratorEntry *wxMenuItemBase::GetAccel() const
{
    return wxGetAccelFromString(GetText());
}

void wxMenuItemBase::SetAccel(wxAcceleratorEntry *accel)
{
    wxString text = m_text.BeforeFirst(wxT('\t'));
    if ( accel )
    {
        text += wxT('\t');

        int flags = accel->GetFlags();
        if ( flags & wxACCEL_ALT )
            text += wxT("Alt-");
        if ( flags & wxACCEL_CTRL )
            text += wxT("Ctrl-");
        if ( flags & wxACCEL_SHIFT )
            text += wxT("Shift-");

        int code = accel->GetKeyCode();
        switch ( code )
        {
            case WXK_F1:
            case WXK_F2:
            case WXK_F3:
            case WXK_F4:
            case WXK_F5:
            case WXK_F6:
            case WXK_F7:
            case WXK_F8:
            case WXK_F9:
            case WXK_F10:
            case WXK_F11:
            case WXK_F12:
                text << wxT('F') << code - WXK_F1 + 1;
                break;

            // if there are any other keys wxGetAccelFromString() may return,
            // we should process them here

            default:
                if ( wxIsalnum(code) )
                {
                    text << (wxChar)code;

                    break;
                }

                wxFAIL_MSG( wxT("unknown keyboard accel") );
        }
    }

    SetText(text);
}

#endif // wxUSE_ACCEL

// ----------------------------------------------------------------------------
// wxMenu ctor and dtor
// ----------------------------------------------------------------------------

void wxMenuBase::Init(long style)
{
    m_items.DeleteContents(TRUE);

    m_menuBar = (wxMenuBar *)NULL;
    m_menuParent = (wxMenu *)NULL;

    m_invokingWindow = (wxWindow *)NULL;
    m_style = style;
    m_clientData = (void *)NULL;
    m_eventHandler = this;

#if wxUSE_MENU_CALLBACK
    m_callback = (wxFunction) NULL;
#endif // wxUSE_MENU_CALLBACK
}

wxMenuBase::~wxMenuBase()
{
    // nothing to do, wxMenuItemList dtor will delete the menu items.

    // Actually, in GTK, the submenus have to get deleted first.
}

// ----------------------------------------------------------------------------
// wxMenu item adding/removing
// ----------------------------------------------------------------------------

void wxMenuBase::AddSubMenu(wxMenu *submenu)
{
    wxCHECK_RET( submenu, _T("can't add a NULL submenu") );

    if ( m_menuBar )
    {
        submenu->Attach(m_menuBar);
    }

    submenu->SetParent((wxMenu *)this);
}

bool wxMenuBase::DoAppend(wxMenuItem *item)
{
    wxCHECK_MSG( item, FALSE, wxT("invalid item in wxMenu::Append()") );

    m_items.Append(item);
    if ( item->IsSubMenu() )
    {
        AddSubMenu(item->GetSubMenu());
    }

    return TRUE;
}

bool wxMenuBase::Insert(size_t pos, wxMenuItem *item)
{
    wxCHECK_MSG( item, FALSE, wxT("invalid item in wxMenu::Insert") );

    if ( pos == GetMenuItemCount() )
    {
        return DoAppend(item);
    }
    else
    {
        wxCHECK_MSG( pos < GetMenuItemCount(), FALSE,
                     wxT("invalid index in wxMenu::Insert") );

        return DoInsert(pos, item);
    }
}

bool wxMenuBase::DoInsert(size_t pos, wxMenuItem *item)
{
    wxCHECK_MSG( item, FALSE, wxT("invalid item in wxMenu::Insert()") );

    wxMenuItemList::Node *node = m_items.Item(pos);
    wxCHECK_MSG( node, FALSE, wxT("invalid index in wxMenu::Insert()") );

    m_items.Insert(node, item);
    if ( item->IsSubMenu() )
    {
        AddSubMenu(item->GetSubMenu());
    }

    return TRUE;
}

wxMenuItem *wxMenuBase::Remove(wxMenuItem *item)
{
    wxCHECK_MSG( item, NULL, wxT("invalid item in wxMenu::Remove") );

    return DoRemove(item);
}

wxMenuItem *wxMenuBase::DoRemove(wxMenuItem *item)
{
    wxMenuItemList::Node *node = m_items.Find(item);

    // if we get here, the item is valid or one of Remove() functions is broken
    wxCHECK_MSG( node, NULL, wxT("bug in wxMenu::Remove logic") );

    // we detach the item, but we do delete the list node (i.e. don't call
    // DetachNode() here!)
    node->SetData((wxMenuItem *)NULL);  // to prevent it from deleting the item
    m_items.DeleteNode(node);

    // item isn't attached to anything any more
    wxMenu *submenu = item->GetSubMenu();
    if ( submenu )
    {
        submenu->SetParent((wxMenu *)NULL);
    }

    return item;
}

bool wxMenuBase::Delete(wxMenuItem *item)
{
    wxCHECK_MSG( item, FALSE, wxT("invalid item in wxMenu::Delete") );

    return DoDelete(item);
}

bool wxMenuBase::DoDelete(wxMenuItem *item)
{
    wxMenuItem *item2 = DoRemove(item);
    wxCHECK_MSG( item2, FALSE, wxT("failed to delete menu item") );

    // don't delete the submenu
    item2->SetSubMenu((wxMenu *)NULL);

    delete item2;

    return TRUE;
}

bool wxMenuBase::Destroy(wxMenuItem *item)
{
    wxCHECK_MSG( item, FALSE, wxT("invalid item in wxMenu::Destroy") );

    return DoDestroy(item);
}

bool wxMenuBase::DoDestroy(wxMenuItem *item)
{
    wxMenuItem *item2 = DoRemove(item);
    wxCHECK_MSG( item2, FALSE, wxT("failed to delete menu item") );

    delete item2;

    return TRUE;
}

// ----------------------------------------------------------------------------
// wxMenu searching for items
// ----------------------------------------------------------------------------

// Finds the item id matching the given string, -1 if not found.
int wxMenuBase::FindItem(const wxString& text) const
{
    wxString label = wxMenuItem::GetLabelFromText(text);
    for ( wxMenuItemList::Node *node = m_items.GetFirst();
          node;
          node = node->GetNext() )
    {
        wxMenuItem *item = node->GetData();
        if ( item->IsSubMenu() )
        {
            int rc = item->GetSubMenu()->FindItem(label);
            if ( rc != wxNOT_FOUND )
                return rc;
        }

        // we execute this code for submenus as well to alllow finding them by
        // name just like the ordinary items
        if ( !item->IsSeparator() )
        {
            if ( item->GetLabel() == label )
                return item->GetId();
        }
    }

    return wxNOT_FOUND;
}

// recursive search for item by id
wxMenuItem *wxMenuBase::FindItem(int itemId, wxMenu **itemMenu) const
{
    if ( itemMenu )
        *itemMenu = NULL;

    wxMenuItem *item = NULL;
    for ( wxMenuItemList::Node *node = m_items.GetFirst();
          node && !item;
          node = node->GetNext() )
    {
        item = node->GetData();

        if ( item->GetId() == itemId )
        {
            if ( itemMenu )
                *itemMenu = (wxMenu *)this;
        }
        else if ( item->IsSubMenu() )
        {
            item = item->GetSubMenu()->FindItem(itemId, itemMenu);
        }
        else
        {
            // don't exit the loop
            item = NULL;
        }
    }

    return item;
}

// non recursive search
wxMenuItem *wxMenuBase::FindChildItem(int id, size_t *ppos) const
{
    wxMenuItem *item = (wxMenuItem *)NULL;
    wxMenuItemList::Node *node = GetMenuItems().GetFirst();

    size_t pos;
    for ( pos = 0; node; pos++ )
    {
        if ( node->GetData()->GetId() == id )
        {
            item = node->GetData();

            break;
        }

        node = node->GetNext();
    }

    if ( ppos )
    {
        *ppos = item ? pos : (size_t)wxNOT_FOUND;
    }

    return item;
}

// ----------------------------------------------------------------------------
// wxMenu helpers used by derived classes
// ----------------------------------------------------------------------------

// Update a menu and all submenus recursively. source is the object that has
// the update event handlers defined for it. If NULL, the menu or associated
// window will be used.
void wxMenuBase::UpdateUI(wxEvtHandler* source)
{
    if ( !source && GetInvokingWindow() )
        source = GetInvokingWindow()->GetEventHandler();
    if ( !source )
        source = GetEventHandler();
    if ( !source )
        source = this;

    wxMenuItemList::Node* node = GetMenuItems().GetFirst();
    while ( node )
    {
        wxMenuItem* item = node->GetData();
        if ( !item->IsSeparator() )
        {
            wxWindowID id = item->GetId();
            wxUpdateUIEvent event(id);
            event.SetEventObject( source );

            if ( source->ProcessEvent(event) )
            {
                // if anything changed, update the chanegd attribute
                if (event.GetSetText())
                    SetLabel(id, event.GetText());
                if (event.GetSetChecked())
                    Check(id, event.GetChecked());
                if (event.GetSetEnabled())
                    Enable(id, event.GetEnabled());
            }

            // recurse to the submenus
            if ( item->GetSubMenu() )
                item->GetSubMenu()->UpdateUI(source);
        }
        //else: item is a separator (which don't process update UI events)

        node = node->GetNext();
    }
}

bool wxMenuBase::SendEvent(int id, int checked)
{
    wxCommandEvent event(wxEVT_COMMAND_MENU_SELECTED, id);
    event.SetEventObject(this);
    event.SetInt(checked);

    bool processed = FALSE;

#if wxUSE_MENU_CALLBACK
    // Try a callback
    if (m_callback)
    {
        (void)(*(m_callback))(*this, event);
        processed = TRUE;
    }
#endif // wxUSE_MENU_CALLBACK

    // Try the menu's event handler
    if ( !processed )
    {
        wxEvtHandler *handler = GetEventHandler();
        if ( handler )
            processed = handler->ProcessEvent(event);
    }

    // Try the window the menu was popped up from (and up through the
    // hierarchy)
    if ( !processed )
    {
        const wxMenuBase *menu = this;
        while ( menu )
        {
            wxWindow *win = menu->GetInvokingWindow();
            if ( win )
            {
                processed = win->GetEventHandler()->ProcessEvent(event);
                break;
            }

            menu = menu->GetParent();
        }
    }

    return processed;
}

// ----------------------------------------------------------------------------
// wxMenu attaching/detaching to/from menu bar
// ----------------------------------------------------------------------------

void wxMenuBase::Attach(wxMenuBarBase *menubar)
{
    // use Detach() instead!
    wxASSERT_MSG( menubar, _T("menu can't be attached to NULL menubar") );

    // use IsAttached() to prevent this from happening
    wxASSERT_MSG( !m_menuBar, _T("attaching menu twice?") );

    m_menuBar = (wxMenuBar *)menubar;
}

void wxMenuBase::Detach()
{
    // use IsAttached() to prevent this from happening
    wxASSERT_MSG( m_menuBar, _T("detaching unattached menu?") );

    m_menuBar = NULL;
}

// ----------------------------------------------------------------------------
// wxMenu functions forwarded to wxMenuItem
// ----------------------------------------------------------------------------

void wxMenuBase::Enable( int id, bool enable )
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_RET( item, wxT("wxMenu::Enable: no such item") );

    item->Enable(enable);
}

bool wxMenuBase::IsEnabled( int id ) const
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_MSG( item, FALSE, wxT("wxMenu::IsEnabled: no such item") );

    return item->IsEnabled();
}

void wxMenuBase::Check( int id, bool enable )
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_RET( item, wxT("wxMenu::Check: no such item") );

    item->Check(enable);
}

bool wxMenuBase::IsChecked( int id ) const
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_MSG( item, FALSE, wxT("wxMenu::IsChecked: no such item") );

    return item->IsChecked();
}

void wxMenuBase::SetLabel( int id, const wxString &label )
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_RET( item, wxT("wxMenu::SetLabel: no such item") );

    item->SetText(label);
}

wxString wxMenuBase::GetLabel( int id ) const
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_MSG( item, wxT(""), wxT("wxMenu::GetLabel: no such item") );

    return item->GetText();
}

void wxMenuBase::SetHelpString( int id, const wxString& helpString )
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_RET( item, wxT("wxMenu::SetHelpString: no such item") );

    item->SetHelp( helpString );
}

wxString wxMenuBase::GetHelpString( int id ) const
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_MSG( item, wxT(""), wxT("wxMenu::GetHelpString: no such item") );

    return item->GetHelp();
}

// ----------------------------------------------------------------------------
// wxMenuBarBase ctor and dtor
// ----------------------------------------------------------------------------

wxMenuBarBase::wxMenuBarBase()
{
    // we own the menus when we get them
    m_menus.DeleteContents(TRUE);

    // not attached yet
    m_menuBarFrame = NULL;
}

wxMenuBarBase::~wxMenuBarBase()
{
    // nothing to do, the list will delete the menus because of the call to
    // DeleteContents() above
}

// ----------------------------------------------------------------------------
// wxMenuBar item access: the base class versions manage m_menus list, the
// derived class should reflect the changes in the real menubar
// ----------------------------------------------------------------------------

wxMenu *wxMenuBarBase::GetMenu(size_t pos) const
{
    wxMenuList::Node *node = m_menus.Item(pos);
    wxCHECK_MSG( node, NULL, wxT("bad index in wxMenuBar::GetMenu()") );

    return node->GetData();
}

bool wxMenuBarBase::Append(wxMenu *menu, const wxString& WXUNUSED(title))
{
    wxCHECK_MSG( menu, FALSE, wxT("can't append NULL menu") );

    m_menus.Append(menu);
    menu->Attach(this);

    return TRUE;
}

bool wxMenuBarBase::Insert(size_t pos, wxMenu *menu,
                           const wxString& title)
{
    if ( pos == m_menus.GetCount() )
    {
        return wxMenuBarBase::Append(menu, title);
    }
    else // not at the end
    {
        wxCHECK_MSG( menu, FALSE, wxT("can't insert NULL menu") );

        wxMenuList::Node *node = m_menus.Item(pos);
        wxCHECK_MSG( node, FALSE, wxT("bad index in wxMenuBar::Insert()") );

        m_menus.Insert(node, menu);
        menu->Attach(this);

        return TRUE;
    }
}

wxMenu *wxMenuBarBase::Replace(size_t pos, wxMenu *menu,
                               const wxString& WXUNUSED(title))
{
    wxCHECK_MSG( menu, NULL, wxT("can't insert NULL menu") );

    wxMenuList::Node *node = m_menus.Item(pos);
    wxCHECK_MSG( node, NULL, wxT("bad index in wxMenuBar::Replace()") );

    wxMenu *menuOld = node->GetData();
    node->SetData(menu);

    menu->Attach(this);
    menuOld->Detach();

    return menuOld;
}

wxMenu *wxMenuBarBase::Remove(size_t pos)
{
    wxMenuList::Node *node = m_menus.Item(pos);
    wxCHECK_MSG( node, NULL, wxT("bad index in wxMenuBar::Remove()") );

    node = m_menus.DetachNode(node);
    wxCHECK( node, NULL );  // unexpected
    wxMenu *menu = node->GetData();
    menu->Detach();

    delete node;

    return menu;
}

int wxMenuBarBase::FindMenu(const wxString& title) const
{
    wxString label = wxMenuItem::GetLabelFromText(title);

    size_t count = GetMenuCount();
    for ( size_t i = 0; i < count; i++ )
    {
        wxString title2 = GetLabelTop(i);
        if ( (title2 == title) ||
             (wxMenuItem::GetLabelFromText(title2) == label) )
        {
            // found
            return (int)i;
        }
    }

    return wxNOT_FOUND;

}

// ----------------------------------------------------------------------------
// wxMenuBar attaching/detaching to/from the frame
// ----------------------------------------------------------------------------

void wxMenuBarBase::Attach(wxFrame *frame)
{
    wxASSERT_MSG( !IsAttached(), wxT("menubar already attached!") );

    m_menuBarFrame = frame;
}

void wxMenuBarBase::Detach()
{
    wxASSERT_MSG( IsAttached(), wxT("detaching unattached menubar") );

    m_menuBarFrame = NULL;
}

// ----------------------------------------------------------------------------
// wxMenuBar searching for items
// ----------------------------------------------------------------------------

wxMenuItem *wxMenuBarBase::FindItem(int id, wxMenu **menu) const
{
    if ( menu )
        *menu = NULL;

    wxMenuItem *item = NULL;
    size_t count = GetMenuCount();
    for ( size_t i = 0; !item && (i < count); i++ )
    {
        item = m_menus[i]->FindItem(id, menu);
    }

    return item;
}

int wxMenuBarBase::FindMenuItem(const wxString& menu, const wxString& item) const
{
    wxString label = wxMenuItem::GetLabelFromText(menu);

    int i = 0;
    wxMenuList::Node *node;
    for ( node = m_menus.GetFirst(); node; node = node->GetNext(), i++ )
    {
        if ( label == wxMenuItem::GetLabelFromText(GetLabelTop(i)) )
            return node->GetData()->FindItem(item);
    }

    return wxNOT_FOUND;
}

// ---------------------------------------------------------------------------
// wxMenuBar functions forwarded to wxMenuItem
// ---------------------------------------------------------------------------

void wxMenuBarBase::Enable(int id, bool enable)
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_RET( item, wxT("attempt to enable an item which doesn't exist") );

    item->Enable(enable);
}

void wxMenuBarBase::Check(int id, bool check)
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_RET( item, wxT("attempt to check an item which doesn't exist") );
    wxCHECK_RET( item->IsCheckable(), wxT("attempt to check an uncheckable item") );

    item->Check(check);
}

bool wxMenuBarBase::IsChecked(int id) const
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_MSG( item, FALSE, wxT("wxMenuBar::IsChecked(): no such item") );

    return item->IsChecked();
}

bool wxMenuBarBase::IsEnabled(int id) const
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_MSG( item, FALSE, wxT("wxMenuBar::IsEnabled(): no such item") );

    return item->IsEnabled();
}

void wxMenuBarBase::SetLabel(int id, const wxString& label)
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_RET( item, wxT("wxMenuBar::SetLabel(): no such item") );

    item->SetText(label);
}

wxString wxMenuBarBase::GetLabel(int id) const
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_MSG( item, wxEmptyString,
                 wxT("wxMenuBar::GetLabel(): no such item") );

    return item->GetText();
}

void wxMenuBarBase::SetHelpString(int id, const wxString& helpString)
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_RET( item, wxT("wxMenuBar::SetHelpString(): no such item") );

    item->SetHelp(helpString);
}

wxString wxMenuBarBase::GetHelpString(int id) const
{
    wxMenuItem *item = FindItem(id);

    wxCHECK_MSG( item, wxEmptyString,
                 wxT("wxMenuBar::GetHelpString(): no such item") );

    return item->GetHelp();
}

#endif // wxUSE_MENUS
