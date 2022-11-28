/////////////////////////////////////////////////////////////////////////////
// Name:        project.h
// Purpose:     Studio project classes
// Author:      Julian Smart
// Modified by:
// Created:     27/7/98
// RCS-ID:      $Id: project.h,v 1.1 2000/03/03 11:24:43 JS Exp $
// Copyright:   (c) Julian Smart
// Licence:
/////////////////////////////////////////////////////////////////////////////

#ifndef _STUDIO_PROJECT_H_
#define _STUDIO_PROJECT_H_

#include <wx/treectrl.h>
#include <wx/imaglist.h>

/*
 * This is the project tree control.
 */

class csProjectTreeCtrl: public wxTreeCtrl
{

DECLARE_CLASS(csProjectTreeCtrl)
public:

    csProjectTreeCtrl(wxWindow *parent, const wxWindowID id, const wxPoint& pos, const wxSize& size,
       long style = wxTR_HAS_BUTTONS|wxTR_LINES_AT_ROOT);

    ~csProjectTreeCtrl();

    wxImageList& GetImageList() const { return (wxImageList&) m_imageList; }
protected:
    wxImageList     m_imageList;

DECLARE_EVENT_TABLE()
};

#endif
  // _STUDIO_PROJECT_H_

