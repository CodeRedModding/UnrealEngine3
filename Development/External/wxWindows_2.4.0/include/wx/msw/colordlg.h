/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/colordlg.h
// Purpose:     wxColourDialog class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: colordlg.h,v 1.5 2002/05/09 22:31:44 VZ Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COLORDLG_H_
#define _WX_COLORDLG_H_

#ifdef __GNUG__
#pragma interface "colordlg.h"
#endif

#include "wx/setup.h"
#include "wx/dialog.h"
#include "wx/cmndata.h"

// ----------------------------------------------------------------------------
// wxColourDialog: dialog for choosing a colours
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxColourDialog : public wxDialog
{
public:
    wxColourDialog();
    wxColourDialog(wxWindow *parent, wxColourData *data = NULL);

    bool Create(wxWindow *parent, wxColourData *data = NULL);

    wxColourData& GetColourData() { return m_colourData; }

    // override some base class virtuals
    virtual void SetTitle(const wxString& title);
    virtual wxString GetTitle() const;

    virtual int ShowModal();

    virtual void DoGetPosition( int *x, int *y ) const;

protected:
    virtual void DoGetSize(int *width, int *height) const;
    virtual void DoGetClientSize(int *width, int *height) const;
    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO);

    wxColourData        m_colourData;
    wxString            m_title;

    wxPoint             m_pos;

    DECLARE_DYNAMIC_CLASS(wxColourDialog)
};

#endif
    // _WX_COLORDLG_H_
