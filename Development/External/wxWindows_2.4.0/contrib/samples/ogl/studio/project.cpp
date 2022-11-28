/////////////////////////////////////////////////////////////////////////////
// Name:        project.cpp
// Purpose:     Studio project classes
// Author:      Julian Smart
// Modified by:
// Created:     27/7/98
// RCS-ID:      $Id: project.cpp,v 1.2 2002/01/08 23:27:53 VS Exp $
// Copyright:   (c) Julian Smart
// Licence:
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#include "wx/mdi.h"
#endif

#include "wx/laywin.h"
#include "studio.h"
#include "project.h"

IMPLEMENT_CLASS(csProjectTreeCtrl, wxTreeCtrl)

BEGIN_EVENT_TABLE(csProjectTreeCtrl, wxTreeCtrl)
END_EVENT_TABLE()

// Define my frame constructor
csProjectTreeCtrl::csProjectTreeCtrl(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size,
	long style):

  wxTreeCtrl(parent, id, pos, size, style),
  m_imageList(16, 16)
{
    m_imageList.Add(wxIcon("folder1"));
    m_imageList.Add(wxIcon("file1"));

    SetImageList(& m_imageList);
}

csProjectTreeCtrl::~csProjectTreeCtrl()
{
    SetImageList(NULL);
}

// Create the project window
bool csApp::CreateProjectWindow(wxFrame *parent)
{
#if 0
    // Create a layout window
    wxSashLayoutWindow* win = new wxSashLayoutWindow(parent, ID_LAYOUT_WINDOW_PROJECT, wxDefaultPosition, wxSize(200, 30), wxNO_BORDER|wxSW_3D|wxCLIP_CHILDREN);
    win->SetDefaultSize(wxSize(150, 10000));
    win->SetOrientation(wxLAYOUT_VERTICAL);
    win->SetAlignment(wxLAYOUT_LEFT);
    win->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));
    win->SetSashVisible(wxSASH_RIGHT, TRUE);
    win->SetExtraBorderSize(5);

    m_projectSashWindow = win;

    m_projectTreeCtrl = new csProjectTreeCtrl(win, ID_WINDOW_PROJECT_TREE, wxDefaultPosition,
        wxDefaultSize, wxTR_HAS_BUTTONS|wxTR_LINES_AT_ROOT|wxDOUBLE_BORDER);

    // For now, hide the window
    m_projectSashWindow->Show(FALSE);
#endif

    return TRUE;
}

// Fill out the project tree control
void csApp::FillProjectTreeCtrl()
{
#if 0
    csProjectTreeCtrl& tree = *GetProjectTreeCtrl();

    // Dummy data for now
    long level0 = tree.InsertItem(0, "Applications", 0, 0);
    long level1 = tree.InsertItem(level0, "Projects", 0, 0);
    tree.InsertItem(level1, "project1", 1, 1);
    tree.InsertItem(level1, "project2", 1, 1);
#endif
}

