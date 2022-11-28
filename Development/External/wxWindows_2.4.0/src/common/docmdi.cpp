/////////////////////////////////////////////////////////////////////////////
// Name:        docmdi.cpp
// Purpose:     Frame classes for MDI document/view applications
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: docmdi.cpp,v 1.16 2002/07/22 14:46:41 DW Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "docmdi.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
  #pragma hdrstop
#endif

#if wxUSE_MDI_ARCHITECTURE && wxUSE_DOC_VIEW_ARCHITECTURE

#include "wx/docmdi.h"

/*
 * Docview MDI parent frame
 */

IMPLEMENT_CLASS(wxDocMDIParentFrame, wxMDIParentFrame)

BEGIN_EVENT_TABLE(wxDocMDIParentFrame, wxMDIParentFrame)
    EVT_MENU(wxID_EXIT, wxDocMDIParentFrame::OnExit)
    EVT_MENU_RANGE(wxID_FILE1, wxID_FILE9, wxDocMDIParentFrame::OnMRUFile)
    EVT_CLOSE(wxDocMDIParentFrame::OnCloseWindow)
END_EVENT_TABLE()

wxDocMDIParentFrame::wxDocMDIParentFrame(wxDocManager *manager, wxFrame *frame, wxWindowID id, const wxString& title,
  const wxPoint& pos, const wxSize& size, long style, const wxString& name):
  wxMDIParentFrame(frame, id, title, pos, size, style, name)
{
  m_docManager = manager;
}

void wxDocMDIParentFrame::OnExit(wxCommandEvent& WXUNUSED(event))
{
    Close();
}

void wxDocMDIParentFrame::OnMRUFile(wxCommandEvent& event)
{
      wxString f(m_docManager->GetHistoryFile(event.GetId() - wxID_FILE1));
      if (f != wxT(""))
        (void)m_docManager->CreateDocument(f, wxDOC_SILENT);
}

// Extend event processing to search the view's event table
bool wxDocMDIParentFrame::ProcessEvent(wxEvent& event)
{
    // Try the document manager, then do default processing
    if (!m_docManager || !m_docManager->ProcessEvent(event))
        return wxEvtHandler::ProcessEvent(event);
    else
        return TRUE;
}

void wxDocMDIParentFrame::OnCloseWindow(wxCloseEvent& event)
{
  if (m_docManager->Clear(!event.CanVeto()))
  {
    this->Destroy();
  }
  else
    event.Veto();
}


/*
 * Default document child frame for MDI children
 */

IMPLEMENT_CLASS(wxDocMDIChildFrame, wxMDIChildFrame)

BEGIN_EVENT_TABLE(wxDocMDIChildFrame, wxMDIChildFrame)
    EVT_ACTIVATE(wxDocMDIChildFrame::OnActivate)
    EVT_CLOSE(wxDocMDIChildFrame::OnCloseWindow)
END_EVENT_TABLE()

wxDocMDIChildFrame::wxDocMDIChildFrame(wxDocument *doc, wxView *view, wxMDIParentFrame *frame, wxWindowID  id,
  const wxString& title, const wxPoint& pos, const wxSize& size, long style, const wxString& name):
    wxMDIChildFrame(frame, id, title, pos, size, style, name)
{
  m_childDocument = doc;
  m_childView = view;
  if (view)
    view->SetFrame(this);
}

wxDocMDIChildFrame::~wxDocMDIChildFrame(void)
{
	m_childView = (wxView *) NULL;
}

// Extend event processing to search the view's event table
bool wxDocMDIChildFrame::ProcessEvent(wxEvent& event)
{
    static wxEvent *ActiveEvent = NULL;

    // Break recursion loops
    if (ActiveEvent == &event)
        return FALSE;

    ActiveEvent = &event;

    bool ret;
	if ( !m_childView || ! m_childView->ProcessEvent(event) )
    {
        // Only hand up to the parent if it's a menu command
        if (!event.IsKindOf(CLASSINFO(wxCommandEvent)) || !GetParent() || !GetParent()->ProcessEvent(event))
            ret = wxEvtHandler::ProcessEvent(event);
        else
            ret = TRUE;
    }
	else
        ret = TRUE;

    ActiveEvent = NULL;
    return ret;
}

void wxDocMDIChildFrame::OnActivate(wxActivateEvent& event)
{
  wxMDIChildFrame::OnActivate(event);

  if (event.GetActive() && m_childView)
    m_childView->Activate(event.GetActive());
}

void wxDocMDIChildFrame::OnCloseWindow(wxCloseEvent& event)
{
  // Close view but don't delete the frame while doing so!
  // ...since it will be deleted by wxWindows if we return TRUE.
  if (m_childView)
  {
    bool ans = FALSE;
    if (!event.CanVeto())
      ans = TRUE; // Must delete.
    else
      ans = m_childView->Close(FALSE); // FALSE means don't delete associated window

    if (ans)
    {
      m_childView->Activate(FALSE);
      delete m_childView;
      m_childView = (wxView *) NULL;
      m_childDocument = (wxDocument *) NULL;

      this->Destroy();
    }
    else
        event.Veto();
  }
  else
    event.Veto();
}

#endif
    // wxUSE_DOC_VIEW_ARCHITECTURE

