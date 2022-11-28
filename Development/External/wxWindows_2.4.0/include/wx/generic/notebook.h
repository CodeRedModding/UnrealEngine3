/////////////////////////////////////////////////////////////////////////////
// Name:        notebook.h
// Purpose:     wxNotebook class (a.k.a. property sheet, tabbed dialog)
// Author:      Julian Smart
// Modified by:
// RCS-ID:      $Id: notebook.h,v 1.7 2002/08/31 11:29:12 GD Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_NOTEBOOK_H_
#define _WX_NOTEBOOK_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "notebook.h"
#endif

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------
#include "wx/dynarray.h"
#include "wx/event.h"
#include "wx/control.h"
#include "wx/generic/tabg.h"

// ----------------------------------------------------------------------------
// types
// ----------------------------------------------------------------------------

// fwd declarations
class WXDLLEXPORT wxImageList;
class WXDLLEXPORT wxWindow;

// Already defined in wx/notebook.h
#if 0
// array of notebook pages
typedef wxWindow wxNotebookPage;  // so far, any window can be a page
WX_DEFINE_ARRAY(wxNotebookPage *, wxArrayPages);
#endif

// ----------------------------------------------------------------------------
// wxNotebook
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxNotebook;

// This reuses wxTabView to draw the tabs.
class WXDLLEXPORT wxNotebookTabView: public wxTabView
{
DECLARE_DYNAMIC_CLASS(wxNotebookTabView)
public:
  wxNotebookTabView(wxNotebook* notebook, long style = wxTAB_STYLE_DRAW_BOX | wxTAB_STYLE_COLOUR_INTERIOR);
  ~wxNotebookTabView(void);

  // Called when a tab is activated
  virtual void OnTabActivate(int activateId, int deactivateId);
  // Allows vetoing
  virtual bool OnTabPreActivate(int activateId, int deactivateId);

protected:
   wxNotebook*      m_notebook;
};

class wxNotebook : public wxNotebookBase
{
public:
  // ctors
  // -----
    // default for dynamic class
  wxNotebook();
    // the same arguments as for wxControl (@@@ any special styles?)
  wxNotebook(wxWindow *parent,
             wxWindowID id,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = 0,
             const wxString& name = "notebook");
    // Create() function
  bool Create(wxWindow *parent,
              wxWindowID id,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = 0,
              const wxString& name = "notebook");
    // dtor
  ~wxNotebook();

  // accessors
  // ---------
  // Find the position of the wxNotebookPage, -1 if not found.
  int FindPagePosition(wxNotebookPage* page) const;

    // set the currently selected page, return the index of the previously
    // selected one (or -1 on error)
    // NB: this function will _not_ generate wxEVT_NOTEBOOK_PAGE_xxx events
  int SetSelection(int nPage);
    // cycle thru the tabs
  //  void AdvanceSelection(bool bForward = TRUE);
    // get the currently selected page
  int GetSelection() const { return m_nSelection; }

    // set/get the title of a page
  bool SetPageText(int nPage, const wxString& strText);
  wxString GetPageText(int nPage) const;

  // get the number of rows for a control with wxNB_MULTILINE style (not all
  // versions support it - they will always return 1 then)
  virtual int GetRowCount() const ;

    // sets/returns item's image index in the current image list
  int  GetPageImage(int nPage) const;
  bool SetPageImage(int nPage, int nImage);

  // control the appearance of the notebook pages
    // set the size (the same for all pages)
  void SetPageSize(const wxSize& size);
    // set the padding between tabs (in pixels)
  void SetPadding(const wxSize& padding);

    // Sets the size of the tabs (assumes all tabs are the same size)
  void SetTabSize(const wxSize& sz);

  // operations
  // ----------
    // remove one page from the notebook, and delete the page.
  bool DeletePage(int nPage);
  bool DeletePage(wxNotebookPage* page);
    // remove one page from the notebook, without deleting the page.
  bool RemovePage(int nPage);
  bool RemovePage(wxNotebookPage* page);
    // remove all pages
  bool DeleteAllPages();
    // the same as AddPage(), but adds it at the specified position
  bool InsertPage(int nPage,
                  wxNotebookPage *pPage,
                  const wxString& strText,
                  bool bSelect = FALSE,
                  int imageId = -1);

  // callbacks
  // ---------
  void OnSize(wxSizeEvent& event);
  void OnIdle(wxIdleEvent& event);
  void OnSelChange(wxNotebookEvent& event);
  void OnSetFocus(wxFocusEvent& event);
  void OnNavigationKey(wxNavigationKeyEvent& event);

  // base class virtuals
  // -------------------
  virtual void Command(wxCommandEvent& event);
  virtual void SetConstraintSizes(bool recurse = TRUE);
  virtual bool DoPhase(int nPhase);

  // Implementation

  // wxNotebook on Motif uses a generic wxTabView to implement itself.
  wxTabView *GetTabView() const { return m_tabView; }
  void SetTabView(wxTabView *v) { m_tabView = v; }

  void OnMouseEvent(wxMouseEvent& event);
  void OnPaint(wxPaintEvent& event);

  virtual wxRect GetAvailableClientSize();

  // Implementation: calculate the layout of the view rect
  // and resize the children if required
  bool RefreshLayout(bool force = TRUE);

protected:
  // common part of all ctors
  void Init();

  // helper functions
  void ChangePage(int nOldSel, int nSel); // change pages

#if 0
  wxImageList  *m_pImageList; // we can have an associated image list
  wxArrayPages  m_aPages;     // array of pages
#endif
  
  int m_nSelection;           // the current selection (-1 if none)

  wxTabView*   m_tabView;

  DECLARE_DYNAMIC_CLASS(wxNotebook)
  DECLARE_EVENT_TABLE()
};

#endif // _WX_NOTEBOOK_H_
