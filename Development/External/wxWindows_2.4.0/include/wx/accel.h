///////////////////////////////////////////////////////////////////////////////
// Name:        wx/accel.h
// Purpose:     wxAcceleratorEntry and wxAcceleratorTable classes
// Author:      Julian Smart, Robert Roebling, Vadim Zeitlin
// Modified by:
// Created:     31.05.01 (extracted from other files)
// RCS-ID:      $Id: accel.h,v 1.9 2002/05/07 20:40:04 GD Exp $
// Copyright:   (c) wxWindows team
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ACCEL_H_BASE_
#define _WX_ACCEL_H_BASE_

#include "wx/defs.h"

#if wxUSE_ACCEL

#include "wx/object.h"

class WXDLLEXPORT wxAcceleratorTable;
class WXDLLEXPORT wxMenuItem;
class WXDLLEXPORT wxKeyEvent;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// wxAcceleratorEntry flags
enum
{
    wxACCEL_NORMAL  = 0x0000,   // no modifiers
    wxACCEL_ALT     = 0x0001,   // hold Alt key down
    wxACCEL_CTRL    = 0x0002,   // hold Ctrl key down
    wxACCEL_SHIFT   = 0x0004    // hold Shift key down
};

// ----------------------------------------------------------------------------
// an entry in wxAcceleratorTable corresponds to one accelerator
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxAcceleratorEntry
{
public:
    wxAcceleratorEntry(int flags = 0, int keyCode = 0, int cmd = 0,
                       wxMenuItem *item = NULL)
        : m_flags(flags)
        , m_keyCode(keyCode)
        , m_command(cmd)
        , m_item(item)
        { }

    wxAcceleratorEntry(const wxAcceleratorEntry& entry)
        : m_flags(entry.m_flags)
        , m_keyCode(entry.m_keyCode)
        , m_command(entry.m_command)
        , m_item(entry.m_item)
        { }

    wxAcceleratorEntry& operator=(const wxAcceleratorEntry& entry)
    {
        Set(entry.m_flags, entry.m_keyCode, entry.m_command, entry.m_item);
        return *this;
    }
    
    void Set(int flags, int keyCode, int cmd, wxMenuItem *item = NULL)
    {
        m_flags = flags;
        m_keyCode = keyCode;
        m_command = cmd;
        m_item = item;
    }

    void SetMenuItem(wxMenuItem *item) { m_item = item; }

    int GetFlags() const { return m_flags; }
    int GetKeyCode() const { return m_keyCode; }
    int GetCommand() const { return m_command; }

    wxMenuItem *GetMenuItem() const { return m_item; }

    bool operator==(const wxAcceleratorEntry& entry) const
    {
        return m_flags == entry.m_flags &&
               m_keyCode == entry.m_keyCode &&
               m_command == entry.m_command &&
               m_item == entry.m_item;
    }

    bool operator!=(const wxAcceleratorEntry& entry) const
        { return !(*this == entry); }

#if defined(__WXMOTIF__)
    // Implementation use only
    bool MatchesEvent(const wxKeyEvent& event) const;
#endif
    
private:
    int m_flags;    // combination of wxACCEL_XXX constants
    int m_keyCode;  // ASCII or virtual keycode
    int m_command;  // Command id to generate

    // the menu item this entry corresponds to, may be NULL
    wxMenuItem *m_item;

    // for compatibility with old code, use accessors now!
    friend class WXDLLEXPORT wxMenu;
};

// ----------------------------------------------------------------------------
// include wxAcceleratorTable class declaration, it is only used by the library
// and so doesn't have any published user visible interface
// ----------------------------------------------------------------------------

#if defined(__WXUNIVERSAL__)
    #include "wx/generic/accel.h"
#elif defined(__WXMSW__)
    #include "wx/msw/accel.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/accel.h"
#elif defined(__WXGTK__)
    #include "wx/gtk/accel.h"
#elif defined(__WXMAC__)
    #include "wx/mac/accel.h"
#elif defined(__WXPM__)
    #include "wx/os2/accel.h"
#elif defined(__WXSTUBS__)
    #include "wx/stubs/accel.h"
#endif

WXDLLEXPORT_DATA(extern wxAcceleratorTable) wxNullAcceleratorTable;

#endif // wxUSE_ACCEL

#endif
    // _WX_ACCEL_H_BASE_
