/////////////////////////////////////////////////////////////////////////////
// Name:        editlbox.h
// Purpose:     ListBox with editable items
// Author:      Vaclav Slavik
// RCS-ID:      $Id: editlbox.h,v 1.7 2002/09/07 12:10:20 GD Exp $
// Copyright:   (c) Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


#ifndef __WX_EDITLBOX_H__
#define __WX_EDITLBOX_H__

#if defined(__GNUG__) && !defined(__APPLE__)
	#pragma interface "editlbox.h"
#endif

#include "wx/panel.h"

#ifdef GIZMOISDLL
#define GIZMODLLEXPORT WXDLLEXPORT
#else
#define GIZMODLLEXPORT
#endif


class WXDLLEXPORT wxBitmapButton;
class WXDLLEXPORT wxListCtrl;
class WXDLLEXPORT wxListEvent;

#define wxEL_ALLOW_NEW          0x0100
#define wxEL_ALLOW_EDIT         0x0200
#define wxEL_ALLOW_DELETE       0x0400

// This class provides a composite control that lets the
// user easily enter list of strings

class GIZMODLLEXPORT wxEditableListBox : public wxPanel
{
	DECLARE_CLASS(wxEditableListBox);

public:
    wxEditableListBox(wxWindow *parent, wxWindowID id,
                      const wxString& label,
                      const wxPoint& pos = wxDefaultPosition,
                      const wxSize& size = wxDefaultSize,
                      long style = wxEL_ALLOW_NEW | wxEL_ALLOW_EDIT | wxEL_ALLOW_DELETE,
                      const wxString& name = wxT("editableListBox"));

    void SetStrings(const wxArrayString& strings);
    void GetStrings(wxArrayString& strings);

    wxListCtrl* GetListCtrl()       { return m_listCtrl; }
    wxBitmapButton* GetDelButton()  { return m_bDel; }
    wxBitmapButton* GetNewButton()  { return m_bNew; }
    wxBitmapButton* GetUpButton()   { return m_bUp; }
    wxBitmapButton* GetDownButton() { return m_bDown; }
    wxBitmapButton* GetEditButton() { return m_bEdit; }

protected:
    wxBitmapButton *m_bDel, *m_bNew, *m_bUp, *m_bDown, *m_bEdit;
    wxListCtrl *m_listCtrl;
    int m_selection;
    long m_style;

    void OnItemSelected(wxListEvent& event);
    void OnEndLabelEdit(wxListEvent& event);
    void OnNewItem(wxCommandEvent& event);
    void OnDelItem(wxCommandEvent& event);
    void OnEditItem(wxCommandEvent& event);
    void OnUpItem(wxCommandEvent& event);
    void OnDownItem(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};

#endif
