/*=============================================================================
DlgPropertyTextEditBox.cpp: Property text edit dialog
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "FConfigCacheIni.h"
#include "DlgPropertyTextEditBox.h"
#include "PropertyWindow.h"
#include "PropertyUtils.h"

/*-----------------------------------------------------------------------------
WxDlgPropertyTextEditBox
-----------------------------------------------------------------------------*/
BEGIN_EVENT_TABLE(WxDlgPropertyTextEditBox, wxDialog)
	EVT_BUTTON(IDB_APPLY, WxDlgPropertyTextEditBox::OnApply)
	EVT_BUTTON(IDB_OK, WxDlgPropertyTextEditBox::OnOK)
	EVT_CLOSE(WxDlgPropertyTextEditBox::OnClose)
	EVT_KILL_FOCUS(WxDlgPropertyTextEditBox::OnKillFocus)
END_EVENT_TABLE()


wxPoint WxDlgPropertyTextEditBox::DefaultPosition = wxDefaultPosition;
wxSize WxDlgPropertyTextEditBox::DefaultSize	  = wxDefaultSize;

WxDlgPropertyTextEditBox::WxDlgPropertyTextEditBox(class WxPropertyControl* InItem, class UPropertyInputTextEditBox* InPropertyInput, const TCHAR* Title, const TCHAR* Value)
:	wxDialog(NULL, wxID_ANY, Title, DefaultPosition, DefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxSTAY_ON_TOP )
,	Item(InItem)
,	PropertyInput(InPropertyInput)
{
	SetMinSize(wxSize(400,200));

	wxSizer* MainSizer = new wxBoxSizer(wxVERTICAL);
	{
		// Textboxes
		wxSizer* TextSizer = new wxBoxSizer(wxVERTICAL);
		{
			// Text Box
			TextCtrl = new wxTextCtrl(this, wxID_ANY, Value, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
			TextCtrl->SetEditable(true);
			TextSizer->Add(TextCtrl, 1, wxEXPAND | wxTOP, 5);
			TextCtrl->SetInsertionPointEnd();
		}
		MainSizer->Add(TextSizer, 1, wxEXPAND | wxALL, 5);

		wxSizer* ButtonSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			ApplyButton = new wxButton(this, IDB_APPLY, *LocalizeUnrealEd("&Apply"));
			ButtonSizer->Add(ApplyButton, 0, wxEXPAND | wxALIGN_RIGHT, 5);
			ButtonSizer->AddSpacer(5);
			OKButton = new wxButton(this, IDB_OK, *LocalizeUnrealEd("&OK"));
			ButtonSizer->Add(OKButton, 0, wxEXPAND | wxALIGN_RIGHT, 5);
		}
		MainSizer->Add(ButtonSizer, 0, wxALIGN_RIGHT, 5);
	}
	SetSizer(MainSizer);

	TextCtrl->Connect(TextCtrl->GetId(), wxEVT_KILL_FOCUS, wxFocusEventHandler(WxDlgPropertyTextEditBox::OnKillFocus));
}

WxDlgPropertyTextEditBox::~WxDlgPropertyTextEditBox()
{
	DefaultPosition = GetPosition();
	DefaultSize = GetSize();
}

void WxDlgPropertyTextEditBox::OnApply(wxCommandEvent& Event)
{
	SaveValue();
}

void WxDlgPropertyTextEditBox::OnOK(wxCommandEvent& Event)
{
	SaveValue();
	Hide();
}

void WxDlgPropertyTextEditBox::OnClose( wxCloseEvent& CloseEvent )
{
	SaveValue();
	Hide();
}

/**
 * Save the value out to the input.
 */
void WxDlgPropertyTextEditBox::SaveValue()
{
	PropertyInput->SendToObjects(Item);
}

/**
 *	Returns value of the text box
 */
FString WxDlgPropertyTextEditBox::GetValue()
{
	return (const TCHAR*)TextCtrl->GetValue();
}

/**
 *	Sets the value of the text box
 */
void WxDlgPropertyTextEditBox::SetValue( const TCHAR* Value )
{
	TextCtrl->SetValue( Value );
}

/**
*	Event to allow commit prior to destructor
*/
void WxDlgPropertyTextEditBox::OnKillFocus( wxFocusEvent& In )
{
	//NOTE: the "this" pointer is actually the text ctrl.  NOT the Dialog
	WxDlgPropertyTextEditBox* ParentDlg = wxDynamicCast(m_parent, WxDlgPropertyTextEditBox);
	check(ParentDlg);
	check(ParentDlg->PropertyInput);
	check(ParentDlg->Item);

	// Apply on close.
	ParentDlg->PropertyInput->SendToObjects(ParentDlg->Item);
}
