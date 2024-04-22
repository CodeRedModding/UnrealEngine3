/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgGenericStringCombo.h"

BEGIN_EVENT_TABLE(WxDlgGenericStringCombo, WxDlgGenericComboEntry)
	EVT_BUTTON( wxID_OK, WxDlgGenericStringCombo::OnOK )
END_EVENT_TABLE()

/**
 * @param	bDisableCancelButton	[opt] If TRUE, disable (grey out) the dialog's 'Cancel' button.  Default is FALSE.
 * @param	bReadOnlyComboBox		[opt] If TRUE, the dialog box will be read only.  Default is TRUE.
 */
WxDlgGenericStringCombo::WxDlgGenericStringCombo( UBOOL bDisableCancelButton, UBOOL bReadOnlyComboBox )
: WxDlgGenericComboEntry(bDisableCancelButton, bReadOnlyComboBox, TEXT("ID_GENERICSTRINGCOMBO_DLG"))
{
	StringEntry = (wxTextCtrl*)FindWindow( XRCID( "IDEC_STRINGENTRY" ) );
	check( StringEntry != NULL );
	StringCaption = (wxStaticText*)FindWindow( XRCID( "IDEC_STRINGCAPTION" ) );
	check( StringCaption != NULL );

	ADDEVENTHANDLER( XRCID("IDEC_STRINGENTRY"), wxEVT_COMMAND_TEXT_ENTER, &WxDlgGenericStringCombo::OnOK );
	FWindowUtil::LoadPosSize( TEXT("DlgGenericStringCombo"), this );
}
WxDlgGenericStringCombo::~WxDlgGenericStringCombo()
{
	FWindowUtil::SavePosSize( TEXT("DlgGenericStringCombo"), this );
}

/**
 * Programatically sets the editbox text to the specified string.
 */
void WxDlgGenericStringCombo::SetEditboxText( const TCHAR* EditboxString )
{
	if ( StringEntry != NULL && EditboxString != NULL )
	{
		StringEntry->SetValue( EditboxString );
	}
}

/**
 * Assigns a WxNameTextValidator to the editbox or clears an existing WxNameTextValidator
 *
 * @param	a bitmask of ENameValidationType members indicating which characters are valid
 */
void WxDlgGenericStringCombo::SetEditboxValidatorMask( DWORD ValidationMask )
{
	if ( ValidationMask == 0 )
	{
		StringEntry->SetValidator(wxDefaultValidator);
	}
	else
	{
		StringEntry->SetValidator(WxNameTextValidator(&EnteredString, ValidationMask));
	}
}

/**
 * Displays the dialog.
 *
 * @param	DialogTitle		The text to be displayed in the dialog's title bar.
 * @param	EditboxCaption	The text to use for the editbox caption.
 * @param	EditboxValue	The text to use as the initial value for the editbox
 * @param	ComboCaption	A caption appearing to the left of the combo box.
 * @param	ComboOptions	An array of strings to appear in the combo box.  Must contain at least one element.
 * @param	SelectedItem	Index of the item to be selected when the window first appears, in [0,ComboOptions.Num()-1].
 * @param	bLocalize		If TRUE, localize the window.  Set to FALSE if InDialogTitle and InComboCaption are already localized.
 * @return					wxID_OK if the user selected the "Ok" button.
 */
int WxDlgGenericStringCombo::ShowModal(const TCHAR* DialogTitle, const TCHAR* EditboxCaption, const TCHAR* EditboxValue, const TCHAR* ComboCaption, const TArray<FString>& ComboOptions, INT SelectedItem, UBOOL bLocalize)
{
	if ( StringCaption != NULL && EditboxCaption != NULL )
	{
		StringCaption->SetLabel(EditboxCaption);
	}
	SetEditboxText(EditboxValue);

	return WxDlgGenericComboEntry::ShowModal(DialogTitle, ComboCaption, ComboOptions, SelectedItem, bLocalize);
}

/**
 * Responds to the user selecting the "OK" button.
 *
 * @return					wxID_OK if the user selected the "Ok" button.
 */
void WxDlgGenericStringCombo::OnOK(wxCommandEvent& In)
{
	EnteredString = StringEntry->GetValue();
	WxDlgGenericComboEntry::OnOK(In);
}
