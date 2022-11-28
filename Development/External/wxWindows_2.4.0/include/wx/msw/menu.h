/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/menu.h
// Purpose:     wxMenu, wxMenuBar classes
// Author:      Julian Smart
// Modified by: Vadim Zeitlin (wxMenuItem is now in separate file)
// Created:     01/02/97
// RCS-ID:      $Id: menu.h,v 1.36 2002/03/21 02:35:08 VZ Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MENU_H_
#define _WX_MENU_H_

#ifdef __GNUG__
    #pragma interface "menu.h"
#endif

#if wxUSE_ACCEL
    #include "wx/accel.h"
    #include "wx/dynarray.h"

    WX_DEFINE_EXPORTED_ARRAY(wxAcceleratorEntry *, wxAcceleratorArray);
#endif // wxUSE_ACCEL

class WXDLLEXPORT wxFrame;

// ----------------------------------------------------------------------------
// Menu
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxMenu : public wxMenuBase
{
public:
    // ctors & dtor
    wxMenu(const wxString& title, long style = 0)
        : wxMenuBase(title, style) { Init(); }

    wxMenu(long style = 0) : wxMenuBase(style) { Init(); }

    virtual ~wxMenu();

    // implement base class virtuals
    virtual bool DoAppend(wxMenuItem *item);
    virtual bool DoInsert(size_t pos, wxMenuItem *item);
    virtual wxMenuItem *DoRemove(wxMenuItem *item);

    virtual void Break();

    virtual void SetTitle(const wxString& title);

    // deprecated functions
#if wxUSE_MENU_CALLBACK
    wxMenu(const wxString& title, const wxFunction func)
        : wxMenuBase(title)
    {
        Init();

        Callback(func);
    }
#endif // wxUSE_MENU_CALLBACK

    // implementation only from now on
    // -------------------------------

    virtual void Attach(wxMenuBarBase *menubar);

    bool MSWCommand(WXUINT param, WXWORD id);

    // semi-private accessors
        // get the window which contains this menu
    wxWindow *GetWindow() const;
        // get the menu handle
    WXHMENU GetHMenu() const { return m_hMenu; }

#if wxUSE_ACCEL
    // called by wxMenuBar to build its accel table from the accels of all menus
    bool HasAccels() const { return !m_accels.IsEmpty(); }
    size_t GetAccelCount() const { return m_accels.GetCount(); }
    size_t CopyAccels(wxAcceleratorEntry *accels) const;

    // called by wxMenuItem when its accels changes
    void UpdateAccel(wxMenuItem *item);

    // helper used by wxMenu itself (returns the index in m_accels)
    int FindAccel(int id) const;
#endif // wxUSE_ACCEL

private:
    // common part of all ctors
    void Init();

    // common part of Append/Insert (behaves as Append is pos == (size_t)-1)
    bool DoInsertOrAppend(wxMenuItem *item, size_t pos = (size_t)-1);

    // terminate the current radio group, if any
    void EndRadioGroup();

    // if TRUE, insert a breal before appending the next item
    bool m_doBreak;

    // the position of the first item in the current radio group or -1
    int m_startRadioGroup;

    // the menu handle of this menu
    WXHMENU m_hMenu;

#if wxUSE_ACCEL
    // the accelerators for our menu items
    wxAcceleratorArray m_accels;
#endif // wxUSE_ACCEL

    DECLARE_DYNAMIC_CLASS(wxMenu)
};

// ----------------------------------------------------------------------------
// Menu Bar (a la Windows)
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxMenuBar : public wxMenuBarBase
{
public:
    // ctors & dtor
        // default constructor
    wxMenuBar();
        // unused under MSW
    wxMenuBar(long style);
        // menubar takes ownership of the menus arrays but copies the titles
    wxMenuBar(int n, wxMenu *menus[], const wxString titles[]);
    virtual ~wxMenuBar();

    // menubar construction
    virtual bool Append( wxMenu *menu, const wxString &title );
    virtual bool Insert(size_t pos, wxMenu *menu, const wxString& title);
    virtual wxMenu *Replace(size_t pos, wxMenu *menu, const wxString& title);
    virtual wxMenu *Remove(size_t pos);

    virtual void EnableTop( size_t pos, bool flag );
    virtual void SetLabelTop( size_t pos, const wxString& label );
    virtual wxString GetLabelTop( size_t pos ) const;

    // compatibility: these functions are deprecated
#if WXWIN_COMPATIBILITY
    void SetEventHandler(wxEvtHandler *handler) { m_eventHandler = handler; }
    wxEvtHandler *GetEventHandler() { return m_eventHandler; }

    bool Enabled(int id) const { return IsEnabled(id); }
    bool Checked(int id) const { return IsChecked(id); }
#endif // WXWIN_COMPATIBILITY

    // implementation from now on
    WXHMENU Create();
    virtual void Detach();
    virtual void Attach(wxFrame *frame);

#if wxUSE_ACCEL
    // get the accel table for all the menus
    const wxAcceleratorTable& GetAccelTable() const { return m_accelTable; }

    // update the accel table (must be called after adding/deletign a menu)
    void RebuildAccelTable();
#endif // wxUSE_ACCEL

        // get the menu handle
    WXHMENU GetHMenu() const { return m_hMenu; }

    // if the menubar is modified, the display is not updated automatically,
    // call this function to update it (m_menuBarFrame should be !NULL)
    void Refresh();

protected:
    // common part of all ctors
    void Init();

#if WXWIN_COMPATIBILITY
    wxEvtHandler *m_eventHandler;
#endif // WXWIN_COMPATIBILITY

    wxArrayString m_titles;

    WXHMENU       m_hMenu;

#if wxUSE_ACCEL
    // the accelerator table for all accelerators in all our menus
    wxAcceleratorTable m_accelTable;
#endif // wxUSE_ACCEL

private:
    DECLARE_DYNAMIC_CLASS(wxMenuBar)
};

#endif // _WX_MENU_H_
