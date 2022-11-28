/////////////////////////////////////////////////////////////////////////////
// Name:        textdlgg.cpp
// Purpose:     wxTextEntryDialog
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: textdlgg.cpp,v 1.20 2002/06/23 13:51:32 JS Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "textdlgg.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TEXTDLG

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/dialog.h"
    #include "wx/button.h"
    #include "wx/stattext.h"
    #include "wx/textctrl.h"
    #include "wx/intl.h"
    #include "wx/sizer.h"
#endif

#if wxUSE_STATLINE
    #include "wx/statline.h"
#endif

#include "wx/generic/textdlgg.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

static const int wxID_TEXT = 3000;

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxTextEntryDialog
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxTextEntryDialog, wxDialog)
    EVT_BUTTON(wxID_OK, wxTextEntryDialog::OnOK)
END_EVENT_TABLE()

IMPLEMENT_CLASS(wxTextEntryDialog, wxDialog)

wxTextEntryDialog::wxTextEntryDialog(wxWindow *parent,
                                     const wxString& message,
                                     const wxString& caption,
                                     const wxString& value,
                                     long style,
                                     const wxPoint& pos)
                 : wxDialog(parent, -1, caption, pos, wxDefaultSize,
                            wxDEFAULT_DIALOG_STYLE | wxDIALOG_MODAL),
                   m_value(value)
{
    m_dialogStyle = style;
    m_value = value;

    wxBeginBusyCursor();

    wxBoxSizer *topsizer = new wxBoxSizer( wxVERTICAL );

    // 1) text message
    topsizer->Add( CreateTextSizer( message ), 0, wxALL, 10 );

    // 2) text ctrl
    m_textctrl = new wxTextCtrl(this, wxID_TEXT, value,
                                wxDefaultPosition, wxSize(300, -1),
                                style & ~wxTextEntryDialogStyle);
    topsizer->Add( m_textctrl, 1, wxEXPAND | wxLEFT|wxRIGHT, 15 );

#if wxUSE_VALIDATORS
    wxTextValidator validator( wxFILTER_NONE, &m_value );
    m_textctrl->SetValidator( validator );
#endif
  // wxUSE_VALIDATORS

#if wxUSE_STATLINE
    // 3) static line
    topsizer->Add( new wxStaticLine( this, -1 ), 0, wxEXPAND | wxLEFT|wxRIGHT|wxTOP, 10 );
#endif

    // 4) buttons
    topsizer->Add( CreateButtonSizer( style ), 0, wxCENTRE | wxALL, 10 );

    SetAutoLayout( TRUE );
    SetSizer( topsizer );

    topsizer->SetSizeHints( this );
    topsizer->Fit( this );

    Centre( wxBOTH );

    m_textctrl->SetFocus();

    wxEndBusyCursor();
}

void wxTextEntryDialog::OnOK(wxCommandEvent& WXUNUSED(event) )
{
#if wxUSE_VALIDATORS
    if( Validate() && TransferDataFromWindow() ) 
    {
        EndModal( wxID_OK );
    }
#else
    m_value = m_textctrl->GetValue();

    EndModal(wxID_OK);
#endif
  // wxUSE_VALIDATORS
}

void wxTextEntryDialog::SetValue(const wxString& val)
{
    m_value = val;

    m_textctrl->SetValue(val);
}

#if wxUSE_VALIDATORS
void wxTextEntryDialog::SetTextValidator( long style )
{
    wxTextValidator validator( style, &m_value );
    m_textctrl->SetValidator( validator );
}

void wxTextEntryDialog::SetTextValidator( wxTextValidator& validator )
{
    m_textctrl->SetValidator( validator );
}

#endif
  // wxUSE_VALIDATORS

#endif // wxUSE_TEXTDLG
