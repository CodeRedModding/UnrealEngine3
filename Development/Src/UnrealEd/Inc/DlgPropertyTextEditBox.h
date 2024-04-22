/*=============================================================================
DlgPropertyTextEditBox.h: Text edit box for editing string properties.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGPROPERTYTEXTEDITBOX_H__
#define __DLGPROPERTYTEXTEDITBOX_H__


/** Property text edit dialog */
class WxDlgPropertyTextEditBox : public wxDialog
{
public:
	WxDlgPropertyTextEditBox(class WxPropertyControl* InItem, class UPropertyInputTextEditBox* InPropertyInput, const TCHAR* Title, const TCHAR* Value);
	virtual ~WxDlgPropertyTextEditBox();

	// Get and set contents
	FString GetValue();
	void SetValue( const TCHAR* Value );

	// Save the value out to the input.
	void SaveValue();

	static wxPoint DefaultPosition;
	static wxSize DefaultSize;

private:
	class WxPropertyControl* Item;
	class UPropertyInputTextEditBox* PropertyInput;

	/** Controls */
	wxTextCtrl* TextCtrl;
	wxButton* ApplyButton;
	wxButton* OKButton;

	/** Events */
	void OnApply(wxCommandEvent &Event);
	void OnOK(wxCommandEvent &Event);
	void OnClose( wxCloseEvent& CloseEvent );
	void OnKillFocus ( wxFocusEvent& In );

	DECLARE_EVENT_TABLE()
};


#endif

