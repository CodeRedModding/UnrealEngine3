/////////////////////////////////////////////////////////////////////////////
// Name:        msgdlgg.h
// Purpose:     Generic wxMessageDialog
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: msgdlgg.h,v 1.13 2002/08/31 11:29:12 GD Exp $
// Copyright:   (c)
// Licence:   	wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __MSGDLGH_G__
#define __MSGDLGH_G__

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "msgdlgg.h"
#endif

#include "wx/setup.h"
#include "wx/dialog.h"

// type is an 'or' (|) of wxOK, wxCANCEL, wxYES_NO
// Returns wxYES/NO/OK/CANCEL

WXDLLEXPORT_DATA(extern const wxChar*) wxMessageBoxCaptionStr;

class WXDLLEXPORT wxGenericMessageDialog: public wxDialog
{
DECLARE_DYNAMIC_CLASS(wxGenericMessageDialog)

public:
    wxGenericMessageDialog(wxWindow *parent, const wxString& message,
        const wxString& caption = wxMessageBoxCaptionStr,
        long style = wxOK|wxCENTRE, const wxPoint& pos = wxDefaultPosition);

    void OnYes(wxCommandEvent& event);
    void OnNo(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);

private:
    long m_dialogStyle;

    DECLARE_EVENT_TABLE()
};

#if (!defined( __WXMSW__ ) && !defined( __WXMAC__) && !defined(__WXPM__)) || defined(__WXUNIVERSAL__)
#define wxMessageDialog wxGenericMessageDialog
#endif

#endif
	// __MSGDLGH_G__
