/////////////////////////////////////////////////////////////////////////////
// Name:        proplist.cpp
// Purpose:     Property list classes
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: proplist.cpp,v 1.30.2.1 2002/10/29 21:47:58 RR Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "proplist.h"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_PROPSHEET

#ifndef WX_PRECOMP
    #include "wx/window.h"
    #include "wx/font.h"
    #include "wx/button.h"
    #include "wx/bmpbuttn.h"
    #include "wx/textctrl.h"
    #include "wx/listbox.h"
    #include "wx/settings.h"
    #include "wx/msgdlg.h"
    #include "wx/filedlg.h"
#endif

#include "wx/sizer.h"
#include "wx/module.h"
#include "wx/intl.h"
#include "wx/artprov.h"

#include "wx/colordlg.h"
#include "wx/proplist.h"

#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// ----------------------------------------------------------------------------
// Property text edit control
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxPropertyTextEdit, wxTextCtrl)

wxPropertyTextEdit::wxPropertyTextEdit(wxPropertyListView *v, wxWindow *parent,
    const wxWindowID id, const wxString& value,
    const wxPoint& pos, const wxSize& size,
    long style, const wxString& name):
 wxTextCtrl(parent, id, value, pos, size, style, wxDefaultValidator, name)
{
  m_view = v;
}

void wxPropertyTextEdit::OnSetFocus()
{
}

void wxPropertyTextEdit::OnKillFocus()
{
}

// ----------------------------------------------------------------------------
// Property list view
// ----------------------------------------------------------------------------

bool wxPropertyListView::sm_dialogCancelled = FALSE;

IMPLEMENT_DYNAMIC_CLASS(wxPropertyListView, wxPropertyView)

BEGIN_EVENT_TABLE(wxPropertyListView, wxPropertyView)
    EVT_BUTTON(wxID_OK,                 wxPropertyListView::OnOk)
    EVT_BUTTON(wxID_CANCEL,             wxPropertyListView::OnCancel)
    EVT_BUTTON(wxID_HELP,               wxPropertyListView::OnHelp)
    EVT_BUTTON(wxID_PROP_CROSS,         wxPropertyListView::OnCross)
    EVT_BUTTON(wxID_PROP_CHECK,         wxPropertyListView::OnCheck)
    EVT_BUTTON(wxID_PROP_EDIT,          wxPropertyListView::OnEdit)
    EVT_TEXT_ENTER(wxID_PROP_TEXT,      wxPropertyListView::OnText)
    EVT_LISTBOX(wxID_PROP_SELECT,       wxPropertyListView::OnPropertySelect)
    EVT_COMMAND(wxID_PROP_SELECT, wxEVT_COMMAND_LISTBOX_DOUBLECLICKED,
                                        wxPropertyListView::OnPropertyDoubleClick)
    EVT_LISTBOX(wxID_PROP_VALUE_SELECT, wxPropertyListView::OnValueListSelect)
END_EVENT_TABLE()

wxPropertyListView::wxPropertyListView(wxPanel *propPanel, long flags):wxPropertyView(flags)
{
  m_propertyScrollingList = NULL;
  m_valueList = NULL;
  m_valueText = NULL;
  m_editButton = NULL;
  m_confirmButton = NULL;
  m_cancelButton = NULL;
  m_propertyWindow = propPanel;
  m_managedWindow = NULL;

  m_windowCloseButton = NULL;
  m_windowCancelButton = NULL;
  m_windowHelpButton = NULL;

  m_detailedEditing = FALSE;
}

wxPropertyListView::~wxPropertyListView()
{
}

void wxPropertyListView::ShowView(wxPropertySheet *ps, wxPanel *panel)
{
  m_propertySheet = ps;

  AssociatePanel(panel);
  CreateControls();

  UpdatePropertyList();
  panel->Layout();
}

// Update this view of the viewed object, called e.g. by
// the object itself.
bool wxPropertyListView::OnUpdateView()
{
  return TRUE;
}

bool wxPropertyListView::UpdatePropertyList(bool clearEditArea)
{
  if (!m_propertyScrollingList || !m_propertySheet)
    return FALSE;

  m_propertyScrollingList->Clear();
  if (clearEditArea)
  {
    m_valueList->Clear();
    m_valueText->SetValue( wxT("") );
  }
  wxNode *node = m_propertySheet->GetProperties().First();

  // Should sort them... later...
  while (node)
  {
    wxProperty *property = (wxProperty *)node->Data();
    wxString stringValueRepr(property->GetValue().GetStringRepresentation());
    wxString paddedString(MakeNameValueString(property->GetName(), stringValueRepr));
    m_propertyScrollingList->Append(paddedString.GetData(), (void *)property);
    node = node->Next();
  }
  return TRUE;
}

bool wxPropertyListView::UpdatePropertyDisplayInList(wxProperty *property)
{
  if (!m_propertyScrollingList || !m_propertySheet)
    return FALSE;

#ifdef __WXMSW__
  int currentlySelected = m_propertyScrollingList->GetSelection();
#endif
// #ifdef __WXMSW__
  wxString stringValueRepr(property->GetValue().GetStringRepresentation());
  wxString paddedString(MakeNameValueString(property->GetName(), stringValueRepr));
  int sel = FindListIndexForProperty(property);

  if (sel > -1)
  {
    // Don't update the listbox unnecessarily because it can cause
    // ugly flashing.

    if (paddedString != m_propertyScrollingList->GetString(sel))
      m_propertyScrollingList->SetString(sel, paddedString.GetData());
  }
//#else
//  UpdatePropertyList(FALSE);
//#endif

  // TODO: why is this necessary?
#ifdef __WXMSW__
  if (currentlySelected > -1)
    m_propertyScrollingList->SetSelection(currentlySelected);
#endif

  return TRUE;
}

// Find the wxListBox index corresponding to this property
int wxPropertyListView::FindListIndexForProperty(wxProperty *property)
{
  int n = m_propertyScrollingList->GetCount();
  for (int i = 0; i < n; i++)
  {
    if (property == (wxProperty *)m_propertyScrollingList->wxListBox::GetClientData(i))
      return i;
  }
  return -1;
}

wxString wxPropertyListView::MakeNameValueString(wxString name, wxString value)
{
  wxString theString(name);

  int nameWidth = 25;
  int padWith = nameWidth - theString.Length();
  if (padWith < 0)
    padWith = 0;

  if (GetFlags() & wxPROP_SHOWVALUES)
  {
    // Want to pad with spaces
    theString.Append( wxT(' '), padWith);
    theString += value;
  }

  return theString;
}

// Select and show string representation in validator the given
// property. NULL resets to show no property.
bool wxPropertyListView::ShowProperty(wxProperty *property, bool select)
{
  if (m_currentProperty)
  {
    EndShowingProperty(m_currentProperty);
    m_currentProperty = NULL;
  }

  m_valueList->Clear();
  m_valueText->SetValue( wxT("") );

  if (property)
  {
    m_currentProperty = property;
    BeginShowingProperty(property);
  }
  if (select)
  {
    int sel = FindListIndexForProperty(property);
    if (sel > -1)
      m_propertyScrollingList->SetSelection(sel);
  }
  return TRUE;
}

// Find appropriate validator and load property into value controls
bool wxPropertyListView::BeginShowingProperty(wxProperty *property)
{
  m_currentValidator = FindPropertyValidator(property);
  if (!m_currentValidator)
    return FALSE;

  if (!m_currentValidator->IsKindOf(CLASSINFO(wxPropertyListValidator)))
    return FALSE;

  wxPropertyListValidator *listValidator = (wxPropertyListValidator *)m_currentValidator;

  listValidator->OnPrepareControls(property, this, m_propertyWindow);
  DisplayProperty(property);
  return TRUE;
}

// Find appropriate validator and unload property from value controls
bool wxPropertyListView::EndShowingProperty(wxProperty *property)
{
  if (!m_currentValidator)
    return FALSE;

  RetrieveProperty(property);

  if (!m_currentValidator->IsKindOf(CLASSINFO(wxPropertyListValidator)))
    return FALSE;

  wxPropertyListValidator *listValidator = (wxPropertyListValidator *)m_currentValidator;

  listValidator->OnClearControls(property, this, m_propertyWindow);
  if (m_detailedEditing)
  {
    listValidator->OnClearDetailControls(property, this, m_propertyWindow);
    m_detailedEditing = FALSE;
  }
  return TRUE;
}

void wxPropertyListView::BeginDetailedEditing()
{
  if (!m_currentValidator)
    return;
  if (!m_currentProperty)
    return;
  if (m_detailedEditing)
    return;
  if (!m_currentValidator->IsKindOf(CLASSINFO(wxPropertyListValidator)))
    return;
  if (!m_currentProperty->IsEnabled())
    return;

  wxPropertyListValidator *listValidator = (wxPropertyListValidator *)m_currentValidator;

  if (listValidator->OnPrepareDetailControls(m_currentProperty, this, m_propertyWindow))
    m_detailedEditing = TRUE;
}

void wxPropertyListView::EndDetailedEditing()
{
  if (!m_currentValidator)
    return;
  if (!m_currentProperty)
    return;

  RetrieveProperty(m_currentProperty);

  if (!m_currentValidator->IsKindOf(CLASSINFO(wxPropertyListValidator)))
    return;

  wxPropertyListValidator *listValidator = (wxPropertyListValidator *)m_currentValidator;

  if (m_detailedEditing)
  {
    listValidator->OnClearDetailControls(m_currentProperty, this, m_propertyWindow);
    m_detailedEditing = FALSE;
  }
}

bool wxPropertyListView::DisplayProperty(wxProperty *property)
{
  if (!m_currentValidator)
    return FALSE;

  if (((m_currentValidator->GetFlags() & wxPROP_ALLOW_TEXT_EDITING) == 0) || !property->IsEnabled())
    m_valueText->SetEditable(FALSE);
  else
    m_valueText->SetEditable(TRUE);

  if (!m_currentValidator->IsKindOf(CLASSINFO(wxPropertyListValidator)))
    return FALSE;

  wxPropertyListValidator *listValidator = (wxPropertyListValidator *)m_currentValidator;

  listValidator->OnDisplayValue(property, this, m_propertyWindow);
  return TRUE;
}

bool wxPropertyListView::RetrieveProperty(wxProperty *property)
{
  if (!m_currentValidator)
    return FALSE;
  if (!property->IsEnabled())
    return FALSE;

  if (!m_currentValidator->IsKindOf(CLASSINFO(wxPropertyListValidator)))
    return FALSE;

  wxPropertyListValidator *listValidator = (wxPropertyListValidator *)m_currentValidator;

  if (listValidator->OnCheckValue(property, this, m_propertyWindow))
  {
    if (listValidator->OnRetrieveValue(property, this, m_propertyWindow))
    {
      UpdatePropertyDisplayInList(property);
      OnPropertyChanged(property);
    }
  }
  else
  {
    // Revert to old value
    listValidator->OnDisplayValue(property, this, m_propertyWindow);
  }
  return TRUE;
}


bool wxPropertyListView::EditProperty(wxProperty *WXUNUSED(property))
{
  return TRUE;
}

// Called by the listbox callback
void wxPropertyListView::OnPropertySelect(wxCommandEvent& WXUNUSED(event))
{
  int sel = m_propertyScrollingList->GetSelection();
  if (sel > -1)
  {
    wxProperty *newSel = (wxProperty *)m_propertyScrollingList->wxListBox::GetClientData(sel);
    if (newSel && newSel != m_currentProperty)
    {
      ShowProperty(newSel, FALSE);
    }
  }
}

bool wxPropertyListView::CreateControls()
{
    wxPanel *panel = (wxPanel *)m_propertyWindow;

    wxSize largeButtonSize( 70, 25 );
    wxSize smallButtonSize( 23, 23 );

    if (m_valueText)
        return TRUE;

    if (!panel)
        return FALSE;

    wxFont guiFont = wxSystemSettings::GetSystemFont(wxSYS_DEFAULT_GUI_FONT);

#ifdef __WXMSW__
    wxFont *boringFont =
        wxTheFontList->FindOrCreateFont(guiFont.GetPointSize(), wxMODERN,
                                        wxNORMAL, wxNORMAL, FALSE, _T("Courier New"));
#else
    wxFont *boringFont = wxTheFontList->FindOrCreateFont(guiFont.GetPointSize(), wxTELETYPE, wxNORMAL, wxNORMAL);
#endif

    // May need to be changed in future to eliminate clashes with app.
    // WHAT WAS THIS FOR?
//  panel->SetClientData((char *)this);

    wxBoxSizer *mainsizer = new wxBoxSizer( wxVERTICAL );

    // top row with optional buttons and input line

    wxBoxSizer *topsizer = new wxBoxSizer( wxHORIZONTAL );
    int buttonborder = 3;

    if (m_buttonFlags & wxPROP_BUTTON_CHECK_CROSS)
    {
        wxBitmap tickBitmap = wxArtProvider::GetBitmap(wxART_TICK_MARK);
        wxBitmap crossBitmap = wxArtProvider::GetBitmap(wxART_CROSS_MARK);

        if ( tickBitmap.Ok() && crossBitmap.Ok() )
        {
            m_confirmButton = new wxBitmapButton(panel, wxID_PROP_CHECK, tickBitmap, wxPoint(-1, -1), smallButtonSize );
            m_cancelButton = new wxBitmapButton(panel, wxID_PROP_CROSS, crossBitmap, wxPoint(-1, -1), smallButtonSize );
        }
        else
        {
            m_confirmButton = new wxButton(panel, wxID_PROP_CHECK, _T(":-)"), wxPoint(-1, -1), smallButtonSize );
            m_cancelButton = new wxButton(panel, wxID_PROP_CROSS, _T("X"), wxPoint(-1, -1), smallButtonSize );
        }

        topsizer->Add( m_confirmButton, 0, wxLEFT|wxTOP|wxBOTTOM | wxEXPAND, buttonborder );
        topsizer->Add( m_cancelButton, 0, wxLEFT|wxTOP|wxBOTTOM | wxEXPAND, buttonborder );
    }

    m_valueText = new wxPropertyTextEdit(this, panel, wxID_PROP_TEXT, _T(""),
       wxPoint(-1, -1), wxSize(-1, smallButtonSize.y), wxPROCESS_ENTER);
    m_valueText->Enable(FALSE);
    topsizer->Add( m_valueText, 1, wxALL | wxEXPAND, buttonborder );

    if (m_buttonFlags & wxPROP_PULLDOWN)
    {
        m_editButton = new wxButton(panel, wxID_PROP_EDIT, _T("..."),  wxPoint(-1, -1), smallButtonSize);
        m_editButton->Enable(FALSE);
        topsizer->Add( m_editButton, 0, wxRIGHT|wxTOP|wxBOTTOM | wxEXPAND, buttonborder );
    }

    mainsizer->Add( topsizer, 0, wxEXPAND );

    // middle section with two list boxes

    m_middleSizer = new wxBoxSizer( wxVERTICAL );

    m_valueList = new wxListBox(panel, wxID_PROP_VALUE_SELECT, wxPoint(-1, -1), wxSize(-1, 60));
    m_valueList->Show(FALSE);

    m_propertyScrollingList = new wxListBox(panel, wxID_PROP_SELECT, wxPoint(-1, -1), wxSize(100, 100));
    m_propertyScrollingList->SetFont(* boringFont);
    m_middleSizer->Add( m_propertyScrollingList, 1, wxALL|wxEXPAND, buttonborder );

    mainsizer->Add( m_middleSizer, 1, wxEXPAND );

    // bottom row with buttons

    if ((m_buttonFlags & wxPROP_BUTTON_OK) ||
        (m_buttonFlags & wxPROP_BUTTON_CLOSE) ||
        (m_buttonFlags & wxPROP_BUTTON_CANCEL) ||
        (m_buttonFlags & wxPROP_BUTTON_HELP))
    {
        wxBoxSizer *bottomsizer = new wxBoxSizer( wxHORIZONTAL );
        buttonborder = 5;

        if (m_buttonFlags & wxPROP_BUTTON_OK)
        {
            m_windowCloseButton = new wxButton(panel, wxID_OK, _("OK"), wxPoint(-1, -1), largeButtonSize );
            m_windowCloseButton->SetDefault();
            m_windowCloseButton->SetFocus();
            bottomsizer->Add( m_windowCloseButton, 0, wxALL, buttonborder );
        }
        else if (m_buttonFlags & wxPROP_BUTTON_CLOSE)
        {
            m_windowCloseButton = new wxButton(panel, wxID_OK, _("Close"), wxPoint(-1, -1), largeButtonSize );
            bottomsizer->Add( m_windowCloseButton, 0, wxALL, buttonborder );
        }
        if (m_buttonFlags & wxPROP_BUTTON_CANCEL)
        {
            m_windowCancelButton = new wxButton(panel, wxID_CANCEL, _("Cancel"), wxPoint(-1, -1), largeButtonSize );
            bottomsizer->Add( m_windowCancelButton, 0, wxALL, buttonborder );
        }
        if (m_buttonFlags & wxPROP_BUTTON_HELP)
        {
            m_windowHelpButton = new wxButton(panel, wxID_HELP, _("Help"), wxPoint(-1, -1), largeButtonSize );
            bottomsizer->Add( m_windowHelpButton, 0, wxALL, buttonborder );
        }

        mainsizer->Add( bottomsizer, 0, wxALIGN_RIGHT | wxEXPAND );
    }

    panel->SetSizer( mainsizer );

    return TRUE;
}

void wxPropertyListView::ShowTextControl(bool show)
{
  if (m_valueText)
    m_valueText->Show(show);
}

void wxPropertyListView::ShowListBoxControl(bool show)
{
    if (!m_valueList) return;

    m_valueList->Show(show);

    if (m_buttonFlags & wxPROP_DYNAMIC_VALUE_FIELD)
    {
        if (show)
            m_middleSizer->Prepend( m_valueList, 0, wxTOP|wxLEFT|wxRIGHT | wxEXPAND, 3 );
        else
            m_middleSizer->Remove( 0 );

        m_propertyWindow->Layout();
    }
}

void wxPropertyListView::EnableCheck(bool show)
{
  if (m_confirmButton)
    m_confirmButton->Enable(show);
}

void wxPropertyListView::EnableCross(bool show)
{
  if (m_cancelButton)
    m_cancelButton->Enable(show);
}

bool wxPropertyListView::OnClose()
{
  // Retrieve the value if any
  wxCommandEvent event;
  OnCheck(event);

  delete this;
  return TRUE;
}

void wxPropertyListView::OnValueListSelect(wxCommandEvent& WXUNUSED(event))
{
  if (m_currentProperty && m_currentValidator)
  {
    if (!m_currentValidator->IsKindOf(CLASSINFO(wxPropertyListValidator)))
      return;

    wxPropertyListValidator *listValidator = (wxPropertyListValidator *)m_currentValidator;

    listValidator->OnValueListSelect(m_currentProperty, this, m_propertyWindow);
  }
}

void wxPropertyListView::OnOk(wxCommandEvent& event)
{
  // Retrieve the value if any
  OnCheck(event);

  m_managedWindow->Close(TRUE);
  sm_dialogCancelled = FALSE;
}

void wxPropertyListView::OnCancel(wxCommandEvent& WXUNUSED(event))
{
//  SetReturnCode(wxID_CANCEL);
  m_managedWindow->Close(TRUE);
  sm_dialogCancelled = TRUE;
}

void wxPropertyListView::OnHelp(wxCommandEvent& WXUNUSED(event))
{
}

void wxPropertyListView::OnCheck(wxCommandEvent& WXUNUSED(event))
{
  if (m_currentProperty)
  {
    RetrieveProperty(m_currentProperty);
  }
}

void wxPropertyListView::OnCross(wxCommandEvent& WXUNUSED(event))
{
  if (m_currentProperty && m_currentValidator)
  {
    if (!m_currentValidator->IsKindOf(CLASSINFO(wxPropertyListValidator)))
      return;

    wxPropertyListValidator *listValidator = (wxPropertyListValidator *)m_currentValidator;

    // Revert to old value
    listValidator->OnDisplayValue(m_currentProperty, this, m_propertyWindow);
  }
}

void wxPropertyListView::OnPropertyDoubleClick(wxCommandEvent& WXUNUSED(event))
{
  if (m_currentProperty && m_currentValidator)
  {
    if (!m_currentValidator->IsKindOf(CLASSINFO(wxPropertyListValidator)))
      return;

    wxPropertyListValidator *listValidator = (wxPropertyListValidator *)m_currentValidator;

    // Revert to old value
    listValidator->OnDoubleClick(m_currentProperty, this, m_propertyWindow);
  }
}

void wxPropertyListView::OnEdit(wxCommandEvent& WXUNUSED(event))
{
  if (m_currentProperty && m_currentValidator)
  {
    if (!m_currentValidator->IsKindOf(CLASSINFO(wxPropertyListValidator)))
      return;

    wxPropertyListValidator *listValidator = (wxPropertyListValidator *)m_currentValidator;

    listValidator->OnEdit(m_currentProperty, this, m_propertyWindow);
  }
}

void wxPropertyListView::OnText(wxCommandEvent& event)
{
  if (event.GetEventType() == wxEVT_COMMAND_TEXT_ENTER)
  {
    OnCheck(event);
  }
}

// ----------------------------------------------------------------------------
// Property dialog box
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxPropertyListDialog, wxDialog)

BEGIN_EVENT_TABLE(wxPropertyListDialog, wxDialog)
    EVT_BUTTON(wxID_CANCEL,                wxPropertyListDialog::OnCancel)
    EVT_CLOSE(wxPropertyListDialog::OnCloseWindow)
END_EVENT_TABLE()

wxPropertyListDialog::wxPropertyListDialog(wxPropertyListView *v, wxWindow *parent,
    const wxString& title, const wxPoint& pos,
    const wxSize& size, long style, const wxString& name):
     wxDialog(parent, -1, title, pos, size, style, name)
{
  m_view = v;
  m_view->AssociatePanel( ((wxPanel*)this) );
  m_view->SetManagedWindow(this);
  SetAutoLayout(TRUE);
}

void wxPropertyListDialog::OnCloseWindow(wxCloseEvent& event)
{
  if (m_view)
  {
    SetReturnCode(wxID_CANCEL);
    m_view->OnClose();
    m_view = NULL;
    this->Destroy();
  }
  else
  {
    event.Veto();
  }
}

void wxPropertyListDialog::OnCancel(wxCommandEvent& WXUNUSED(event))
{
    SetReturnCode(wxID_CANCEL);
    this->Close();
}

void wxPropertyListDialog::OnDefaultAction(wxControl *WXUNUSED(item))
{
/*
  if (item == m_view->GetPropertyScrollingList())
    view->OnDoubleClick();
*/
}

// Extend event processing to search the view's event table
bool wxPropertyListDialog::ProcessEvent(wxEvent& event)
{
    if ( !m_view || ! m_view->ProcessEvent(event) )
        return wxEvtHandler::ProcessEvent(event);
    else
        return TRUE;
}

// ----------------------------------------------------------------------------
// Property panel
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxPropertyListPanel, wxPanel)

BEGIN_EVENT_TABLE(wxPropertyListPanel, wxPanel)
    EVT_SIZE(wxPropertyListPanel::OnSize)
END_EVENT_TABLE()

wxPropertyListPanel::~wxPropertyListPanel()
{
}

void wxPropertyListPanel::OnDefaultAction(wxControl *WXUNUSED(item))
{
/*
  if (item == view->GetPropertyScrollingList())
    view->OnDoubleClick();
*/
}

// Extend event processing to search the view's event table
bool wxPropertyListPanel::ProcessEvent(wxEvent& event)
{
    if ( !m_view || ! m_view->ProcessEvent(event) )
        return wxEvtHandler::ProcessEvent(event);
    else
        return TRUE;
}

void wxPropertyListPanel::OnSize(wxSizeEvent& WXUNUSED(event))
{
    Layout();
}

// ----------------------------------------------------------------------------
// Property frame
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxPropertyListFrame, wxFrame)

BEGIN_EVENT_TABLE(wxPropertyListFrame, wxFrame)
    EVT_CLOSE(wxPropertyListFrame::OnCloseWindow)
END_EVENT_TABLE()

void wxPropertyListFrame::OnCloseWindow(wxCloseEvent& event)
{
  if (m_view)
  {
    if (m_propertyPanel)
        m_propertyPanel->SetView(NULL);
    m_view->OnClose();
    m_view = NULL;
    this->Destroy();
  }
  else
  {
    event.Veto();
  }
}

wxPropertyListPanel *wxPropertyListFrame::OnCreatePanel(wxFrame *parent, wxPropertyListView *v)
{
  return new wxPropertyListPanel(v, parent);
}

bool wxPropertyListFrame::Initialize()
{
  m_propertyPanel = OnCreatePanel(this, m_view);
  if (m_propertyPanel)
  {
    m_view->AssociatePanel(m_propertyPanel);
    m_view->SetManagedWindow(this);
    m_propertyPanel->SetAutoLayout(TRUE);
    return TRUE;
  }
  else
    return FALSE;
}

// ----------------------------------------------------------------------------
// Property list specific validator
// ----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxPropertyListValidator, wxPropertyValidator)

bool wxPropertyListValidator::OnSelect(bool select, wxProperty *property, wxPropertyListView *view, wxWindow *parentWindow)
{
//  view->GetValueText()->Show(TRUE);
  if (select)
    OnDisplayValue(property, view, parentWindow);

  return TRUE;
}

bool wxPropertyListValidator::OnValueListSelect(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  wxString s(view->GetValueList()->GetStringSelection());
  if (s != wxT(""))
  {
    view->GetValueText()->SetValue(s);
    view->RetrieveProperty(property);
  }
  return TRUE;
}

bool wxPropertyListValidator::OnDisplayValue(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
//  view->GetValueText()->Show(TRUE);
  wxString str(property->GetValue().GetStringRepresentation());

  view->GetValueText()->SetValue(str);
  return TRUE;
}

// Called when TICK is pressed or focus is lost or view wants to update
// the property list.
// Does the transferance from the property editing area to the property itself
bool wxPropertyListValidator::OnRetrieveValue(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!view->GetValueText())
    return FALSE;
  return FALSE;
}

void wxPropertyListValidator::OnEdit(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (view->GetDetailedEditing())
    view->EndDetailedEditing();
  else
    view->BeginDetailedEditing();
}

bool wxPropertyListValidator::OnClearControls(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (view->GetConfirmButton())
    view->GetConfirmButton()->Enable(FALSE);
  if (view->GetCancelButton())
    view->GetCancelButton()->Enable(FALSE);
  if (view->GetEditButton())
    view->GetEditButton()->Enable(FALSE);
  return TRUE;
}

// ----------------------------------------------------------------------------
// Default validators
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxRealListValidator, wxPropertyListValidator)

///
/// Real number validator
///
bool wxRealListValidator::OnCheckValue(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *parentWindow)
{
  if (m_realMin == 0.0 && m_realMax == 0.0)
    return TRUE;

  if (!view->GetValueText())
    return FALSE;
  wxString value(view->GetValueText()->GetValue());

  float val = 0.0;
  if (!StringToFloat(WXSTRINGCAST value, &val))
  {
    wxChar buf[200];
    wxSprintf(buf, wxT("Value %s is not a valid real number!"), value.GetData());
    wxMessageBox(buf, wxT("Property value error"), wxOK | wxICON_EXCLAMATION, parentWindow);
    return FALSE;
  }

  if (val < m_realMin || val > m_realMax)
  {
    wxChar buf[200];
    wxSprintf(buf, wxT("Value must be a real number between %.2f and %.2f!"), m_realMin, m_realMax);
    wxMessageBox(buf, wxT("Property value error"), wxOK | wxICON_EXCLAMATION, parentWindow);
    return FALSE;
  }
  return TRUE;
}

// Called when TICK is pressed or focus is lost or view wants to update
// the property list.
// Does the transferance from the property editing area to the property itself
bool wxRealListValidator::OnRetrieveValue(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!view->GetValueText())
    return FALSE;

  if (wxStrlen(view->GetValueText()->GetValue()) == 0)
    return FALSE;

  wxString value(view->GetValueText()->GetValue());
  float f = (float)wxAtof(value.GetData());
  property->GetValue() = f;
  return TRUE;
}

bool wxRealListValidator::OnPrepareControls(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (view->GetConfirmButton())
    view->GetConfirmButton()->Enable(TRUE);
  if (view->GetCancelButton())
    view->GetCancelButton()->Enable(TRUE);
  if (view->GetEditButton())
    view->GetEditButton()->Enable(FALSE);
  if (view->GetValueText())
    view->GetValueText()->Enable(TRUE);
  return TRUE;
}

///
/// Integer validator
///
IMPLEMENT_DYNAMIC_CLASS(wxIntegerListValidator, wxPropertyListValidator)

bool wxIntegerListValidator::OnCheckValue(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *parentWindow)
{
  if (m_integerMin == 0 && m_integerMax == 0)
    return TRUE;

  if (!view->GetValueText())
    return FALSE;
  wxString value(view->GetValueText()->GetValue());

  long val = 0;
  if (!StringToLong(WXSTRINGCAST value, &val))
  {
    wxChar buf[200];
    wxSprintf(buf, wxT("Value %s is not a valid integer!"), value.GetData());
    wxMessageBox(buf, wxT("Property value error"), wxOK | wxICON_EXCLAMATION, parentWindow);
    return FALSE;
  }
  if (val < m_integerMin || val > m_integerMax)
  {
    wxChar buf[200];
    wxSprintf(buf, wxT("Value must be an integer between %ld and %ld!"), m_integerMin, m_integerMax);
    wxMessageBox(buf, wxT("Property value error"), wxOK | wxICON_EXCLAMATION, parentWindow);
    return FALSE;
  }
  return TRUE;
}

// Called when TICK is pressed or focus is lost or view wants to update
// the property list.
// Does the transferance from the property editing area to the property itself
bool wxIntegerListValidator::OnRetrieveValue(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!view->GetValueText())
    return FALSE;

  if (wxStrlen(view->GetValueText()->GetValue()) == 0)
    return FALSE;

  wxString value(view->GetValueText()->GetValue());
  long val = (long)wxAtoi(value.GetData());
  property->GetValue() = (long)val;
  return TRUE;
}

bool wxIntegerListValidator::OnPrepareControls(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (view->GetConfirmButton())
    view->GetConfirmButton()->Enable(TRUE);
  if (view->GetCancelButton())
    view->GetCancelButton()->Enable(TRUE);
  if (view->GetEditButton())
    view->GetEditButton()->Enable(FALSE);
  if (view->GetValueText())
    view->GetValueText()->Enable(TRUE);
  return TRUE;
}

///
/// boolean validator
///
IMPLEMENT_DYNAMIC_CLASS(wxBoolListValidator, wxPropertyListValidator)

bool wxBoolListValidator::OnCheckValue(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *parentWindow)
{
  if (!view->GetValueText())
    return FALSE;
  wxString value(view->GetValueText()->GetValue());
  if (value != wxT("True") && value != wxT("False"))
  {
    wxMessageBox(wxT("Value must be True or False!"), wxT("Property value error"), wxOK | wxICON_EXCLAMATION, parentWindow);
    return FALSE;
  }
  return TRUE;
}

// Called when TICK is pressed or focus is lost or view wants to update
// the property list.
// Does the transferance from the property editing area to the property itself
bool wxBoolListValidator::OnRetrieveValue(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!view->GetValueText())
    return FALSE;

  if (wxStrlen(view->GetValueText()->GetValue()) == 0)
    return FALSE;

  wxString value(view->GetValueText()->GetValue());
  bool boolValue = FALSE;
  if (value == wxT("True"))
    boolValue = TRUE;
  else
    boolValue = FALSE;
  property->GetValue() = (bool)boolValue;
  return TRUE;
}

bool wxBoolListValidator::OnDisplayValue(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!view->GetValueText())
    return FALSE;
  wxString str(property->GetValue().GetStringRepresentation());

  view->GetValueText()->SetValue(str);

  if (view->GetValueList()->IsShown())
  {
    view->GetValueList()->SetStringSelection(str);
  }
  return TRUE;
}

bool wxBoolListValidator::OnPrepareControls(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (view->GetConfirmButton())
    view->GetConfirmButton()->Enable(FALSE);
  if (view->GetCancelButton())
    view->GetCancelButton()->Enable(FALSE);
  if (view->GetEditButton())
    view->GetEditButton()->Enable(TRUE);
  if (view->GetValueText())
    view->GetValueText()->Enable(FALSE);
  return TRUE;
}

bool wxBoolListValidator::OnPrepareDetailControls(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (view->GetValueList())
  {
    view->ShowListBoxControl(TRUE);
    view->GetValueList()->Enable(TRUE);

    view->GetValueList()->Append(wxT("True"));
    view->GetValueList()->Append(wxT("False"));
    wxChar *currentString = copystring(view->GetValueText()->GetValue());
    view->GetValueList()->SetStringSelection(currentString);
    delete[] currentString;
  }
  return TRUE;
}

bool wxBoolListValidator::OnClearDetailControls(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (view->GetValueList())
  {
    view->GetValueList()->Clear();
    view->ShowListBoxControl(FALSE);
    view->GetValueList()->Enable(FALSE);
  }
  return TRUE;
}

// Called when the property is double clicked. Extra functionality can be provided,
// cycling through possible values.
bool wxBoolListValidator::OnDoubleClick(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!view->GetValueText())
    return FALSE;
  if (property->GetValue().BoolValue())
    property->GetValue() = (bool)FALSE;
  else
    property->GetValue() = (bool)TRUE;
  view->DisplayProperty(property);
  view->UpdatePropertyDisplayInList(property);
  view->OnPropertyChanged(property);
  return TRUE;
}

///
/// String validator
///
IMPLEMENT_DYNAMIC_CLASS(wxStringListValidator, wxPropertyListValidator)

wxStringListValidator::wxStringListValidator(wxStringList *list, long flags):
  wxPropertyListValidator(flags)
{
  m_strings = list;
  // If no constraint, we just allow the string to be edited.
  if (!m_strings && ((m_validatorFlags & wxPROP_ALLOW_TEXT_EDITING) == 0))
    m_validatorFlags |= wxPROP_ALLOW_TEXT_EDITING;
}

bool wxStringListValidator::OnCheckValue(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *parentWindow)
{
  if (!m_strings)
    return TRUE;

  if (!view->GetValueText())
    return FALSE;
  wxString value(view->GetValueText()->GetValue());

  if (!m_strings->Member(value.GetData()))
  {
    wxString str( wxT("Value ") );
    str += value.GetData();
    str += wxT(" is not valid.");
    wxMessageBox( str.GetData(), wxT("Property value error"), wxOK | wxICON_EXCLAMATION, parentWindow);
    return FALSE;
  }
  return TRUE;
}

// Called when TICK is pressed or focus is lost or view wants to update
// the property list.
// Does the transferance from the property editing area to the property itself
bool wxStringListValidator::OnRetrieveValue(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!view->GetValueText())
    return FALSE;
  wxString value(view->GetValueText()->GetValue());
  property->GetValue() = value ;
  return TRUE;
}

// Called when TICK is pressed or focus is lost or view wants to update
// the property list.
// Does the transferance from the property editing area to the property itself
bool wxStringListValidator::OnDisplayValue(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!view->GetValueText())
    return FALSE;
  wxString str(property->GetValue().GetStringRepresentation());
  view->GetValueText()->SetValue(str);
  if (m_strings && view->GetValueList() && view->GetValueList()->IsShown() && view->GetValueList()->GetCount() > 0)
  {
    view->GetValueList()->SetStringSelection(str);
  }
  return TRUE;
}

bool wxStringListValidator::OnPrepareControls(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  // Unconstrained
  if (!m_strings)
  {
    if (view->GetEditButton())
      view->GetEditButton()->Enable(FALSE);
    if (view->GetConfirmButton())
      view->GetConfirmButton()->Enable(TRUE);
    if (view->GetCancelButton())
      view->GetCancelButton()->Enable(TRUE);
    if (view->GetValueText())
      view->GetValueText()->Enable(TRUE);
    return TRUE;
  }

  // Constrained
  if (view->GetValueText())
    view->GetValueText()->Enable(FALSE);

  if (view->GetEditButton())
    view->GetEditButton()->Enable(TRUE);

  if (view->GetConfirmButton())
    view->GetConfirmButton()->Enable(FALSE);
  if (view->GetCancelButton())
    view->GetCancelButton()->Enable(FALSE);
  return TRUE;
}

bool wxStringListValidator::OnPrepareDetailControls(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (view->GetValueList())
  {
    view->ShowListBoxControl(TRUE);
    view->GetValueList()->Enable(TRUE);
    wxNode *node = m_strings->First();
    while (node)
    {
      wxChar *s = (wxChar *)node->Data();
      view->GetValueList()->Append(s);
      node = node->Next();
    }
    wxChar *currentString = property->GetValue().StringValue();
    view->GetValueList()->SetStringSelection(currentString);
  }
  return TRUE;
}

bool wxStringListValidator::OnClearDetailControls(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!m_strings)
  {
    return TRUE;
  }

  if (view->GetValueList())
  {
    view->GetValueList()->Clear();
    view->ShowListBoxControl(FALSE);
    view->GetValueList()->Enable(FALSE);
  }
  return TRUE;
}

// Called when the property is double clicked. Extra functionality can be provided,
// cycling through possible values.
bool wxStringListValidator::OnDoubleClick(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!view->GetValueText())
    return FALSE;
  if (!m_strings)
    return FALSE;

  wxNode *node = m_strings->First();
  wxChar *currentString = property->GetValue().StringValue();
  while (node)
  {
    wxChar *s = (wxChar *)node->Data();
    if (wxStrcmp(s, currentString) == 0)
    {
      wxChar *nextString = NULL;
      if (node->Next())
        nextString = (wxChar *)node->Next()->Data();
      else
        nextString = (wxChar *)m_strings->First()->Data();
      property->GetValue() = wxString(nextString);
      view->DisplayProperty(property);
      view->UpdatePropertyDisplayInList(property);
      view->OnPropertyChanged(property);
      return TRUE;
    }
    else node = node->Next();
  }
  return TRUE;
}

///
/// Filename validator
///
IMPLEMENT_DYNAMIC_CLASS(wxFilenameListValidator, wxPropertyListValidator)

wxFilenameListValidator::wxFilenameListValidator(wxString message , wxString wildcard, long flags):
  wxPropertyListValidator(flags), m_filenameWildCard(wildcard), m_filenameMessage(message)
{
}

wxFilenameListValidator::~wxFilenameListValidator()
{
}

bool wxFilenameListValidator::OnCheckValue(wxProperty *WXUNUSED(property), wxPropertyListView *WXUNUSED(view), wxWindow *WXUNUSED(parentWindow))
{
  return TRUE;
}

// Called when TICK is pressed or focus is lost or view wants to update
// the property list.
// Does the transferance from the property editing area to the property itself
bool wxFilenameListValidator::OnRetrieveValue(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!view->GetValueText())
    return FALSE;
  wxString value(view->GetValueText()->GetValue());
  property->GetValue() = value ;
  return TRUE;
}

// Called when TICK is pressed or focus is lost or view wants to update
// the property list.
// Does the transferance from the property editing area to the property itself
bool wxFilenameListValidator::OnDisplayValue(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!view->GetValueText())
    return FALSE;
  wxString str(property->GetValue().GetStringRepresentation());
  view->GetValueText()->SetValue(str);
  return TRUE;
}

// Called when the property is double clicked. Extra functionality can be provided,
// cycling through possible values.
bool wxFilenameListValidator::OnDoubleClick(wxProperty *property, wxPropertyListView *view, wxWindow *parentWindow)
{
  if (!view->GetValueText())
    return FALSE;
  OnEdit(property, view, parentWindow);
  return TRUE;
}

bool wxFilenameListValidator::OnPrepareControls(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (view->GetConfirmButton())
    view->GetConfirmButton()->Enable(TRUE);
  if (view->GetCancelButton())
    view->GetCancelButton()->Enable(TRUE);
  if (view->GetEditButton())
    view->GetEditButton()->Enable(TRUE);
  if (view->GetValueText())
    view->GetValueText()->Enable((GetFlags() & wxPROP_ALLOW_TEXT_EDITING) == wxPROP_ALLOW_TEXT_EDITING);
  return TRUE;
}

void wxFilenameListValidator::OnEdit(wxProperty *property, wxPropertyListView *view, wxWindow *parentWindow)
{
  if (!view->GetValueText())
    return;

  wxString s = wxFileSelector(
     m_filenameMessage.GetData(),
     wxPathOnly(property->GetValue().StringValue()),
     wxFileNameFromPath(property->GetValue().StringValue()),
     NULL,
     m_filenameWildCard.GetData(),
     0,
     parentWindow);
  if (s != wxT(""))
  {
    property->GetValue() = s;
    view->DisplayProperty(property);
    view->UpdatePropertyDisplayInList(property);
    view->OnPropertyChanged(property);
  }
}

///
/// Colour validator
///
IMPLEMENT_DYNAMIC_CLASS(wxColourListValidator, wxPropertyListValidator)

wxColourListValidator::wxColourListValidator(long flags):
  wxPropertyListValidator(flags)
{
}

wxColourListValidator::~wxColourListValidator()
{
}

bool wxColourListValidator::OnCheckValue(wxProperty *WXUNUSED(property), wxPropertyListView *WXUNUSED(view), wxWindow *WXUNUSED(parentWindow))
{
  return TRUE;
}

// Called when TICK is pressed or focus is lost or view wants to update
// the property list.
// Does the transferance from the property editing area to the property itself
bool wxColourListValidator::OnRetrieveValue(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!view->GetValueText())
    return FALSE;
  wxString value(view->GetValueText()->GetValue());

  property->GetValue() = value ;
  return TRUE;
}

// Called when TICK is pressed or focus is lost or view wants to update
// the property list.
// Does the transferance from the property editing area to the property itself
bool wxColourListValidator::OnDisplayValue(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!view->GetValueText())
    return FALSE;
  wxString str(property->GetValue().GetStringRepresentation());
  view->GetValueText()->SetValue(str);
  return TRUE;
}

// Called when the property is double clicked. Extra functionality can be provided,
// cycling through possible values.
bool wxColourListValidator::OnDoubleClick(wxProperty *property, wxPropertyListView *view, wxWindow *parentWindow)
{
  if (!view->GetValueText())
    return FALSE;
  OnEdit(property, view, parentWindow);
  return TRUE;
}

bool wxColourListValidator::OnPrepareControls(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (view->GetConfirmButton())
    view->GetConfirmButton()->Enable(TRUE);
  if (view->GetCancelButton())
    view->GetCancelButton()->Enable(TRUE);
  if (view->GetEditButton())
    view->GetEditButton()->Enable(TRUE);
  if (view->GetValueText())
    view->GetValueText()->Enable((GetFlags() & wxPROP_ALLOW_TEXT_EDITING) == wxPROP_ALLOW_TEXT_EDITING);
  return TRUE;
}

void wxColourListValidator::OnEdit(wxProperty *property, wxPropertyListView *view, wxWindow *parentWindow)
{
  if (!view->GetValueText())
    return;

  wxChar *s = property->GetValue().StringValue();
  int r = 0;
  int g = 0;
  int b = 0;
  if (s)
  {
    r = wxHexToDec(s);
    g = wxHexToDec(s+2);
    b = wxHexToDec(s+4);
  }

  wxColour col(r,g,b);

  wxColourData data;
  data.SetChooseFull(TRUE);
  data.SetColour(col);

  for (int i = 0; i < 16; i++)
  {
    wxColour colour(i*16, i*16, i*16);
    data.SetCustomColour(i, colour);
  }

  wxColourDialog dialog(parentWindow, &data);
  if (dialog.ShowModal() != wxID_CANCEL)
  {
    wxColourData retData = dialog.GetColourData();
    col = retData.GetColour();

    wxChar buf[7];
    wxDecToHex(col.Red(), buf);
    wxDecToHex(col.Green(), buf+2);
    wxDecToHex(col.Blue(), buf+4);

    property->GetValue() = wxString(buf);
    view->DisplayProperty(property);
    view->UpdatePropertyDisplayInList(property);
    view->OnPropertyChanged(property);
  }
}

///
/// List of strings validator. For this we need more user interface than
/// we get with a property list; so create a new dialog for editing the list.
///
IMPLEMENT_DYNAMIC_CLASS(wxListOfStringsListValidator, wxPropertyListValidator)

wxListOfStringsListValidator::wxListOfStringsListValidator(long flags):
  wxPropertyListValidator(flags)
{
}

bool wxListOfStringsListValidator::OnCheckValue(wxProperty *WXUNUSED(property), wxPropertyListView *WXUNUSED(view), wxWindow *WXUNUSED(parentWindow))
{
  // No constraints for an arbitrary, user-editable list of strings.
  return TRUE;
}

// Called when TICK is pressed or focus is lost or view wants to update
// the property list.
// Does the transferance from the property editing area to the property itself.
// In this case, the user cannot directly edit the string list.
bool wxListOfStringsListValidator::OnRetrieveValue(wxProperty *WXUNUSED(property), wxPropertyListView *WXUNUSED(view), wxWindow *WXUNUSED(parentWindow))
{
  return TRUE;
}

bool wxListOfStringsListValidator::OnDisplayValue(wxProperty *property, wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (!view->GetValueText())
    return FALSE;
  wxString str(property->GetValue().GetStringRepresentation());
  view->GetValueText()->SetValue(str);
  return TRUE;
}

bool wxListOfStringsListValidator::OnPrepareControls(wxProperty *WXUNUSED(property), wxPropertyListView *view, wxWindow *WXUNUSED(parentWindow))
{
  if (view->GetEditButton())
    view->GetEditButton()->Enable(TRUE);
  if (view->GetValueText())
    view->GetValueText()->Enable(FALSE);

  if (view->GetConfirmButton())
    view->GetConfirmButton()->Enable(FALSE);
  if (view->GetCancelButton())
    view->GetCancelButton()->Enable(FALSE);
  return TRUE;
}

// Called when the property is double clicked. Extra functionality can be provided,
// cycling through possible values.
bool wxListOfStringsListValidator::OnDoubleClick(wxProperty *property, wxPropertyListView *view, wxWindow *parentWindow)
{
  OnEdit(property, view, parentWindow);
  return TRUE;
}

void wxListOfStringsListValidator::OnEdit(wxProperty *property, wxPropertyListView *view, wxWindow *parentWindow)
{
  // Convert property value to a list of strings for editing
  wxStringList *stringList = new wxStringList;

  wxPropertyValue *expr = property->GetValue().GetFirst();
  while (expr)
  {
    wxChar *s = expr->StringValue();
    if (s)
      stringList->Add(s);
    expr = expr->GetNext();
  }

  wxString title(wxT("Editing "));
  title += property->GetName();

  if (EditStringList(parentWindow, stringList, title.GetData()))
  {
    wxPropertyValue& oldValue = property->GetValue();
    oldValue.ClearList();
    wxNode *node = stringList->First();
    while (node)
    {
      wxChar *s = (wxChar *)node->Data();
      oldValue.Append(new wxPropertyValue(s));

      node = node->Next();
    }

    view->DisplayProperty(property);
    view->UpdatePropertyDisplayInList(property);
    view->OnPropertyChanged(property);
  }
  delete stringList;
}

class wxPropertyStringListEditorDialog: public wxDialog
{
  public:
    wxPropertyStringListEditorDialog(wxWindow *parent, const wxString& title,
        const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
          long windowStyle = wxDEFAULT_DIALOG_STYLE, const wxString& name = wxT("stringEditorDialogBox")):
               wxDialog(parent, -1, title, pos, size, windowStyle, name)
    {
      m_stringList = NULL;
      m_stringText = NULL;
      m_listBox = NULL;
      sm_dialogCancelled = FALSE;
      m_currentSelection = -1;
    }
    ~wxPropertyStringListEditorDialog(void) {}
    void OnCloseWindow(wxCloseEvent& event);
    void SaveCurrentSelection(void);
    void ShowCurrentSelection(void);

    void OnOK(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    void OnAdd(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);
    void OnStrings(wxCommandEvent& event);
    void OnText(wxCommandEvent& event);

public:
    wxStringList*       m_stringList;
    wxListBox*          m_listBox;
    wxTextCtrl*         m_stringText;
    static bool         sm_dialogCancelled;
    int                 m_currentSelection;
DECLARE_EVENT_TABLE()
};

#define    wxID_PROP_SL_ADD            3000
#define    wxID_PROP_SL_DELETE            3001
#define    wxID_PROP_SL_STRINGS        3002
#define    wxID_PROP_SL_TEXT            3003

BEGIN_EVENT_TABLE(wxPropertyStringListEditorDialog, wxDialog)
    EVT_BUTTON(wxID_OK,                 wxPropertyStringListEditorDialog::OnOK)
    EVT_BUTTON(wxID_CANCEL,                wxPropertyStringListEditorDialog::OnCancel)
    EVT_BUTTON(wxID_PROP_SL_ADD,        wxPropertyStringListEditorDialog::OnAdd)
    EVT_BUTTON(wxID_PROP_SL_DELETE,        wxPropertyStringListEditorDialog::OnDelete)
    EVT_LISTBOX(wxID_PROP_SL_STRINGS,    wxPropertyStringListEditorDialog::OnStrings)
    EVT_TEXT_ENTER(wxID_PROP_SL_TEXT,            wxPropertyStringListEditorDialog::OnText)
    EVT_CLOSE(wxPropertyStringListEditorDialog::OnCloseWindow)
END_EVENT_TABLE()

class wxPropertyStringListEditorText: public wxTextCtrl
{
 public:
  wxPropertyStringListEditorText(wxWindow *parent, wxWindowID id, const wxString& val,
      const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
    long windowStyle = 0, const wxString& name = wxT("text")):
     wxTextCtrl(parent, id, val, pos, size, windowStyle, wxDefaultValidator, name)
  {
  }
  void OnKillFocus()
  {
    wxPropertyStringListEditorDialog *dialog = (wxPropertyStringListEditorDialog *)GetParent();
    dialog->SaveCurrentSelection();
  }
};

bool wxPropertyStringListEditorDialog::sm_dialogCancelled = FALSE;

// Edit the string list.
bool wxListOfStringsListValidator::EditStringList(wxWindow *parent, wxStringList *stringList, const wxChar *title)
{
  int largeButtonWidth = 60;
  int largeButtonHeight = 25;

  wxBeginBusyCursor();
  wxPropertyStringListEditorDialog *dialog = new wxPropertyStringListEditorDialog(parent,
      title, wxPoint(10, 10), wxSize(400, 400), wxDEFAULT_DIALOG_STYLE|wxDIALOG_MODAL);

  dialog->m_stringList = stringList;

  dialog->m_listBox = new wxListBox(dialog, wxID_PROP_SL_STRINGS,
    wxPoint(-1, -1), wxSize(-1, -1), 0, NULL, wxLB_SINGLE);

  dialog->m_stringText = new wxPropertyStringListEditorText(dialog,
  wxID_PROP_SL_TEXT, wxT(""), wxPoint(5, 240),
       wxSize(300, -1), wxPROCESS_ENTER);
  dialog->m_stringText->Enable(FALSE);

  wxButton *addButton = new wxButton(dialog, wxID_PROP_SL_ADD, wxT("Add"), wxPoint(-1, -1), wxSize(largeButtonWidth, largeButtonHeight));
  wxButton *deleteButton = new wxButton(dialog, wxID_PROP_SL_DELETE, wxT("Delete"), wxPoint(-1, -1), wxSize(largeButtonWidth, largeButtonHeight));
  wxButton *cancelButton = new wxButton(dialog, wxID_CANCEL, wxT("Cancel"), wxPoint(-1, -1), wxSize(largeButtonWidth, largeButtonHeight));
  wxButton *okButton = new wxButton(dialog, wxID_OK, wxT("OK"), wxPoint(-1, -1), wxSize(largeButtonWidth, largeButtonHeight));

#ifndef __WXGTK__
  okButton->SetDefault();
#endif

  wxLayoutConstraints *c = new wxLayoutConstraints;

  c->top.SameAs     (dialog, wxTop, 2);
  c->left.SameAs    (dialog, wxLeft, 2);
  c->right.SameAs   (dialog, wxRight, 2);
  c->bottom.SameAs  (dialog->m_stringText, wxTop, 2);
  dialog->m_listBox->SetConstraints(c);

  c = new wxLayoutConstraints;
  c->left.SameAs    (dialog, wxLeft, 2);
  c->right.SameAs   (dialog, wxRight, 2);
  c->bottom.SameAs  (addButton, wxTop, 2);
  c->height.AsIs();
  dialog->m_stringText->SetConstraints(c);

  c = new wxLayoutConstraints;
  c->bottom.SameAs  (dialog, wxBottom, 2);
  c->left.SameAs    (dialog, wxLeft, 2);
  c->width.AsIs();
  c->height.AsIs();
  addButton->SetConstraints(c);

  c = new wxLayoutConstraints;
  c->bottom.SameAs  (dialog, wxBottom, 2);
  c->left.SameAs    (addButton, wxRight, 2);
  c->width.AsIs();
  c->height.AsIs();
  deleteButton->SetConstraints(c);

  c = new wxLayoutConstraints;
  c->bottom.SameAs  (dialog, wxBottom, 2);
  c->right.SameAs   (dialog, wxRight, 2);
  c->width.AsIs();
  c->height.AsIs();
  cancelButton->SetConstraints(c);

  c = new wxLayoutConstraints;
  c->bottom.SameAs  (dialog, wxBottom, 2);
  c->right.SameAs   (cancelButton, wxLeft, 2);
  c->width.AsIs();
  c->height.AsIs();
  okButton->SetConstraints(c);

  wxNode *node = stringList->First();
  while (node)
  {
    wxChar *str = (wxChar *)node->Data();
    // Save node as client data for each listbox item
    dialog->m_listBox->Append(str, (wxChar *)node);
    node = node->Next();
  }

  dialog->SetClientSize(310, 305);
  dialog->Layout();

  dialog->Centre(wxBOTH);
  wxEndBusyCursor();
  if (dialog->ShowModal() == wxID_CANCEL)
    return FALSE;
  else
    return TRUE;
}

/*
 * String list editor callbacks
 *
 */

void wxPropertyStringListEditorDialog::OnStrings(wxCommandEvent& WXUNUSED(event))
{
  int sel = m_listBox->GetSelection();
  if (sel > -1)
  {
    m_currentSelection = sel;

    ShowCurrentSelection();
  }
}

void wxPropertyStringListEditorDialog::OnDelete(wxCommandEvent& WXUNUSED(event))
{
  int sel = m_listBox->GetSelection();
  if (sel == -1)
    return;

  wxNode *node = (wxNode *)m_listBox->wxListBox::GetClientData(sel);
  if (!node)
    return;

  m_listBox->Delete(sel);
  delete[] (wxChar *)node->Data();
  delete node;
  m_currentSelection = -1;
  m_stringText->SetValue(_T(""));
}

void wxPropertyStringListEditorDialog::OnAdd(wxCommandEvent& WXUNUSED(event))
{
  SaveCurrentSelection();

  wxString initialText;
  wxNode *node = m_stringList->Add(initialText);
  m_listBox->Append(initialText, (void *)node);
  m_currentSelection = m_stringList->Number() - 1;
  m_listBox->SetSelection(m_currentSelection);
  ShowCurrentSelection();
  m_stringText->SetFocus();
}

void wxPropertyStringListEditorDialog::OnOK(wxCommandEvent& WXUNUSED(event))
{
  SaveCurrentSelection();
  EndModal(wxID_OK);
  // Close(TRUE);
  this->Destroy();
}

void wxPropertyStringListEditorDialog::OnCancel(wxCommandEvent& WXUNUSED(event))
{
  sm_dialogCancelled = TRUE;
  EndModal(wxID_CANCEL);
//  Close(TRUE);
  this->Destroy();
}

void wxPropertyStringListEditorDialog::OnText(wxCommandEvent& event)
{
  if (event.GetEventType() == wxEVT_COMMAND_TEXT_ENTER)
  {
    SaveCurrentSelection();
  }
}

void
wxPropertyStringListEditorDialog::OnCloseWindow(wxCloseEvent& WXUNUSED(event))
{
  SaveCurrentSelection();

  Destroy();
}

void wxPropertyStringListEditorDialog::SaveCurrentSelection()
{
  if (m_currentSelection == -1)
    return;

  wxNode *node = (wxNode *)m_listBox->wxListBox::GetClientData(m_currentSelection);
  if (!node)
    return;

  wxString txt(m_stringText->GetValue());
  if (node->Data())
    delete[] (wxChar *)node->Data();
  node->SetData((wxObject *)wxStrdup(txt));

  m_listBox->SetString(m_currentSelection, (wxChar *)node->Data());
}

void wxPropertyStringListEditorDialog::ShowCurrentSelection()
{
  if (m_currentSelection == -1)
  {
    m_stringText->SetValue(wxT(""));
    return;
  }
  wxNode *node = (wxNode *)m_listBox->wxListBox::GetClientData(m_currentSelection);
  wxChar *txt = (wxChar *)node->Data();
  m_stringText->SetValue(txt);
  m_stringText->Enable(TRUE);
}


#endif // wxUSE_PROPSHEET
