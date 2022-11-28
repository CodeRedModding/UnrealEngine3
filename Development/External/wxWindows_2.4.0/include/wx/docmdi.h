/////////////////////////////////////////////////////////////////////////////
// Name:        docmdi.h
// Purpose:     Frame classes for MDI document/view applications
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: docmdi.h,v 1.9.2.1 2002/10/29 21:47:14 RR Exp $
// Copyright:   (c)
// Licence:   	wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DOCMDI_H_
#define _WX_DOCMDI_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "docmdi.h"
#endif

#include "wx/defs.h"

#if wxUSE_MDI_ARCHITECTURE && wxUSE_DOC_VIEW_ARCHITECTURE

#include "wx/docview.h"
#include "wx/mdi.h"

/*
 * Use this instead of wxMDIParentFrame
 */

class WXDLLEXPORT wxDocMDIParentFrame: public wxMDIParentFrame
{
public:
    wxDocMDIParentFrame(wxDocManager *manager, wxFrame *parent, wxWindowID id,
        const wxString& title, const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize, long style = wxDEFAULT_FRAME_STYLE, const wxString& name = wxT("frame"));

    // Extend event processing to search the document manager's event table
    virtual bool ProcessEvent(wxEvent& event);

    wxDocManager *GetDocumentManager(void) const { return m_docManager; }

    void OnExit(wxCommandEvent& event);
    void OnMRUFile(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);

protected:
    wxDocManager *m_docManager;

private:
    DECLARE_CLASS(wxDocMDIParentFrame)
    DECLARE_EVENT_TABLE()
};

/*
 * Use this instead of wxMDIChildFrame
 */

class WXDLLEXPORT wxDocMDIChildFrame: public wxMDIChildFrame
{
public:
    wxDocMDIChildFrame(wxDocument *doc, wxView *view, wxMDIParentFrame *frame, wxWindowID id,
        const wxString& title, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
        long type = wxDEFAULT_FRAME_STYLE, const wxString& name = wxT("frame"));
    ~wxDocMDIChildFrame();

    // Extend event processing to search the view's event table
    virtual bool ProcessEvent(wxEvent& event);

    void OnActivate(wxActivateEvent& event);
    void OnCloseWindow(wxCloseEvent& event);

    inline wxDocument *GetDocument() const { return m_childDocument; }
    inline wxView *GetView(void) const { return m_childView; }
    inline void SetDocument(wxDocument *doc) { m_childDocument = doc; }
    inline void SetView(wxView *view) { m_childView = view; }
    bool Destroy() { m_childView = (wxView *)NULL; return wxMDIChildFrame::Destroy(); }
    
protected:
    wxDocument*       m_childDocument;
    wxView*           m_childView;

private:
    DECLARE_EVENT_TABLE()
    DECLARE_CLASS(wxDocMDIChildFrame)
};

#endif
    // wxUSE_MDI_ARCHITECTURE

#endif
    // _WX_DOCMDI_H_
