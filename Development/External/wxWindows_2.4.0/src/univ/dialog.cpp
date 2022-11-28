/////////////////////////////////////////////////////////////////////////////
// Name:        src/univ/dialog.cpp
// Author:      Robert Roebling, Vaclav Slavik
// Id:          $Id: dialog.cpp,v 1.8 2002/03/10 09:25:35 JS Exp $
// Copyright:   (c) 2001 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "univdialog.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/dialog.h"
    #include "wx/utils.h"
    #include "wx/app.h"
#endif

#include "wx/evtloop.h"

//-----------------------------------------------------------------------------
// wxDialog
//-----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxDialog,wxDialogBase)
    EVT_BUTTON  (wxID_OK,       wxDialog::OnOK)
    EVT_BUTTON  (wxID_CANCEL,   wxDialog::OnCancel)
    EVT_BUTTON  (wxID_APPLY,    wxDialog::OnApply)
    EVT_CLOSE   (wxDialog::OnCloseWindow)
END_EVENT_TABLE()

IMPLEMENT_DYNAMIC_CLASS(wxDialog,wxTopLevelWindow)

void wxDialog::Init()
{
    m_returnCode = 0;
    m_windowDisabler = NULL;
    m_eventLoop = NULL;
    m_isShowingModal = FALSE;
}

wxDialog::~wxDialog()
{
    delete m_eventLoop;
}

bool wxDialog::Create(wxWindow *parent,
                      wxWindowID id, const wxString &title,
                      const wxPoint &pos, const wxSize &size,
                      long style, const wxString &name)
{
    SetExtraStyle(GetExtraStyle() | wxTOPLEVEL_EX_DIALOG);

    // all dialogs should have tab traversal enabled
    style |= wxTAB_TRAVERSAL;

    return wxTopLevelWindow::Create(parent, id, title, pos, size, style, name);
}

void wxDialog::OnApply(wxCommandEvent &WXUNUSED(event))
{
    if ( Validate() ) 
        TransferDataFromWindow();
}

void wxDialog::OnCancel(wxCommandEvent &WXUNUSED(event))
{
    if ( IsModal() )
    {
        EndModal(wxID_CANCEL);
    }
    else
    {
        SetReturnCode(wxID_CANCEL);
        Show(FALSE);
    }
}

void wxDialog::OnOK(wxCommandEvent &WXUNUSED(event))
{
    if ( Validate() && TransferDataFromWindow() )
    {
        if ( IsModal() )
        {
            EndModal(wxID_OK);
        }
        else
        {
            SetReturnCode(wxID_OK);
            Show(FALSE);
        }
    }
}

void wxDialog::OnCloseWindow(wxCloseEvent& WXUNUSED(event))
{
    // We'll send a Cancel message by default,
    // which may close the dialog.
    // Check for looping if the Cancel event handler calls Close().

    // Note that if a cancel button and handler aren't present in the dialog,
    // nothing will happen when you close the dialog via the window manager, or
    // via Close().
    // We wouldn't want to destroy the dialog by default, since the dialog may have been
    // created on the stack.
    // However, this does mean that calling dialog->Close() won't delete the dialog
    // unless the handler for wxID_CANCEL does so. So use Destroy() if you want to be
    // sure to destroy the dialog.
    // The default OnCancel (above) simply ends a modal dialog, and hides a modeless dialog.

    static wxList s_closing;

    if (s_closing.Member(this))
        return;   // no loops

    s_closing.Append(this);

    wxCommandEvent cancelEvent(wxEVT_COMMAND_BUTTON_CLICKED, wxID_CANCEL);
    cancelEvent.SetEventObject(this);
    GetEventHandler()->ProcessEvent(cancelEvent);
    s_closing.DeleteObject(this);
}

bool wxDialog::Show(bool show)
{
    if ( !show )
    {
        // if we had disabled other app windows, reenable them back now because
        // if they stay disabled Windows will activate another window (one
        // which is enabled, anyhow) and we will lose activation
        if ( m_windowDisabler )
        {
            delete m_windowDisabler;
            m_windowDisabler = NULL;
        }

        if ( IsModal() )
            EndModal(wxID_CANCEL);
    }

    bool ret = wxDialogBase::Show(show);

    if ( show ) 
        InitDialog();

    return ret;
}

bool wxDialog::IsModal() const
{
    return m_isShowingModal;
}

void wxDialog::SetModal(bool WXUNUSED(flag))
{
    wxFAIL_MSG( wxT("wxDialog:SetModal obsolete now") );
}

int wxDialog::ShowModal()
{
    if ( IsModal() )
    {
       wxFAIL_MSG( wxT("wxDialog:ShowModal called twice") );
       return GetReturnCode();
    }

    // use the apps top level window as parent if none given unless explicitly
    // forbidden
    if ( !GetParent() && !(GetWindowStyleFlag() & wxDIALOG_NO_PARENT) )
    {
        wxWindow *parent = wxTheApp->GetTopWindow();
        if ( parent && parent != this )
        {
            m_parent = parent;
        }
    }

    Show(TRUE);

    m_isShowingModal = TRUE;

    wxASSERT_MSG( !m_windowDisabler, _T("disabling windows twice?") );

#if defined(__WXGTK__) || defined(__WXMGL__)
    wxBusyCursorSuspender suspender;
    // FIXME (FIXME_MGL) - make sure busy cursor disappears under MSW too
#endif

    m_windowDisabler = new wxWindowDisabler(this);
    if ( !m_eventLoop )
        m_eventLoop = new wxEventLoop;

    m_eventLoop->Run();

    return GetReturnCode();
}

void wxDialog::EndModal(int retCode)
{
    wxASSERT_MSG( m_eventLoop, _T("wxDialog is not modal") );

    SetReturnCode(retCode);

    if ( !IsModal() )
    {
        wxFAIL_MSG( wxT("wxDialog:EndModal called twice") );
        return;
    }

    m_isShowingModal = FALSE;
    
    m_eventLoop->Exit();

    Show(FALSE);
}
