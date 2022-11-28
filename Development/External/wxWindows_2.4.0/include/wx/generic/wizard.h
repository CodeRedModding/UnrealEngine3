///////////////////////////////////////////////////////////////////////////////
// Name:        generic/wizard.h
// Purpose:     declaration of generic wxWizard class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     28.09.99
// RCS-ID:      $Id: wizard.h,v 1.11 2002/08/31 11:29:12 GD Exp $
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// wxWizard
// ----------------------------------------------------------------------------

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "wizardg.h"
#endif

class WXDLLEXPORT wxButton;
class WXDLLEXPORT wxStaticBitmap;
class WXDLLEXPORT wxWizardEvent;

class WXDLLEXPORT wxWizard : public wxWizardBase
{
public:
    // ctor
    wxWizard() { Init(); }
    wxWizard(wxWindow *parent,
             int id = -1,
             const wxString& title = wxEmptyString,
             const wxBitmap& bitmap = wxNullBitmap,
             const wxPoint& pos = wxDefaultPosition)
    {
        Init();
        Create(parent, id, title, bitmap, pos);
    }
    bool Create(wxWindow *parent,
             int id = -1,
             const wxString& title = wxEmptyString,
             const wxBitmap& bitmap = wxNullBitmap,
             const wxPoint& pos = wxDefaultPosition);
    void Init();

    // implement base class pure virtuals
    virtual bool RunWizard(wxWizardPage *firstPage);
    virtual wxWizardPage *GetCurrentPage() const;
    virtual void SetPageSize(const wxSize& size);
    virtual wxSize GetPageSize() const;
    virtual void FitToPage(const wxWizardPage *firstPage);

    // implementation only from now on
    // -------------------------------

    // is the wizard running?
    bool IsRunning() const { return m_page != NULL; }

    // show the prev/next page, but call TransferDataFromWindow on the current
    // page first and return FALSE without changing the page if
    // TransferDataFromWindow() returns FALSE - otherwise, returns TRUE
    bool ShowPage(wxWizardPage *page, bool goingForward = TRUE);

    // do fill the dialog with controls
    // this is app-overridable to, for example, set help and tooltip text
    virtual void DoCreateControls();

private:
    // was the dialog really created?
    bool WasCreated() const { return m_btnPrev != NULL; }

    // event handlers
    void OnCancel(wxCommandEvent& event);
    void OnBackOrNext(wxCommandEvent& event);
    void OnHelp(wxCommandEvent& event);

    void OnWizEvent(wxWizardEvent& event);

    // the page size requested by user
    wxSize m_sizePage;

    // the dialog position from the ctor
    wxPoint m_posWizard;

    // wizard dimensions
    int          m_x, m_y;      // the origin for the pages
    int          m_width,       // the size of the page itself
                 m_height;      // (total width is m_width + m_x)

    // wizard state
    wxWizardPage *m_page;       // the current page or NULL
    wxBitmap      m_bitmap;     // the default bitmap to show

    // wizard controls
    wxButton    *m_btnPrev,     // the "<Back" button
                *m_btnNext;     // the "Next>" or "Finish" button
    wxStaticBitmap *m_statbmp;  // the control for the bitmap

    DECLARE_DYNAMIC_CLASS(wxWizard)
    DECLARE_EVENT_TABLE()
};

