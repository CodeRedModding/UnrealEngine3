/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dialog.h
// Purpose:     wxDialogBase class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29.06.99
// RCS-ID:      $Id: dialog.h,v 1.20 2002/08/31 11:29:10 GD Exp $
// Copyright:   (c) Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DIALOG_H_BASE_
#define _WX_DIALOG_H_BASE_

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "dialogbase.h"
#endif

#include "wx/defs.h"
#include "wx/containr.h"
#include "wx/toplevel.h"

// FIXME - temporary hack in absence of wxTLW !!
#ifndef wxTopLevelWindowNative
#include "wx/panel.h"
class WXDLLEXPORT wxDialogBase : public wxPanel
#else
class WXDLLEXPORT wxDialogBase : public wxTopLevelWindow
#endif
{
public:
    wxDialogBase() { Init(); }
    virtual ~wxDialogBase() { }

    void Init();

    // the modal dialogs have a return code - usually the id of the last
    // pressed button
    void SetReturnCode(int returnCode) { m_returnCode = returnCode; }
    int GetReturnCode() const { return m_returnCode; }

#if wxUSE_STATTEXT && wxUSE_TEXTCTRL
    // splits text up at newlines and places the
    // lines into a vertical wxBoxSizer
    wxSizer *CreateTextSizer( const wxString &message );
#endif // wxUSE_STATTEXT && wxUSE_TEXTCTRL

#if wxUSE_BUTTON
    // places buttons into a horizontal wxBoxSizer
    wxSizer *CreateButtonSizer( long flags );
#endif // wxUSE_BUTTON

protected:
    // the return code from modal dialog
    int m_returnCode;

    // FIXME - temporary hack in absence of wxTLW !!
#ifdef wxTopLevelWindowNative
    DECLARE_EVENT_TABLE()
    WX_DECLARE_CONTROL_CONTAINER();
#endif
};


#if defined(__WXUNIVERSAL__) && !defined(__WXMICROWIN__)
    #include "wx/univ/dialog.h"
#else
    #if defined(__WXMSW__)
        #include "wx/msw/dialog.h"
    #elif defined(__WXMOTIF__)
        #include "wx/motif/dialog.h"
    #elif defined(__WXGTK__)
        #include "wx/gtk/dialog.h"
    #elif defined(__WXMAC__)
        #include "wx/mac/dialog.h"
    #elif defined(__WXPM__)
        #include "wx/os2/dialog.h"
    #elif defined(__WXSTUBS__)
        #include "wx/stubs/dialog.h"
    #endif
#endif

#endif
    // _WX_DIALOG_H_BASE_
