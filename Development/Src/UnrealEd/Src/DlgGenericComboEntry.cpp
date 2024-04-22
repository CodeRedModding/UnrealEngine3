/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgGenericComboEntry.h"

BEGIN_EVENT_TABLE(WxDlgGenericComboEntry, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgGenericComboEntry::OnOK )
END_EVENT_TABLE()

/**
 * @param	bDisableCancelButton	[opt] If TRUE, disable (grey out) the dialog's 'Cancel' button.  Default is FALSE.
 * @param	bReadOnlyComboBox		[opt] If TRUE, the dialog box will be read only.  Default is TRUE.
 */
WxDlgGenericComboEntry::WxDlgGenericComboEntry(UBOOL bDisableCancelButton, UBOOL bReadOnlyComboBox, const TCHAR* DialogIdString/*=NULL*/)
{
	FString ComboDlgID = TEXT("ID_GENERICCOMBOENTRY_DROPDOWN");
	if( bReadOnlyComboBox )
	{
		ComboDlgID = TEXT("ID_GENERICCOMBOENTRY");
	}

	if ( DialogIdString != NULL && *DialogIdString )
	{
		ComboDlgID = DialogIdString;
	}

	bIsReadOnly = bReadOnlyComboBox;

	const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, *ComboDlgID );
	check( bSuccess );

	ComboBox = (wxComboBox*)FindWindow( XRCID( "IDEC_COMBOENTRY" ) );
	check( ComboBox != NULL );

	ComboTextCaption = (wxStaticText*)FindWindow( XRCID( "IDEC_COMBOCAPTION" ) );
	check( ComboTextCaption != NULL );

	if ( bDisableCancelButton )
	{
		wxButton* CancelButton = (wxButton*)FindWindow( XRCID( "wxID_CANCEL" ) );
		check( CancelButton != NULL );
		CancelButton->Enable( false );
	}
}

/**
 * @param	DialogTitle		The text to be displayed in the dialog's title bar.
 * @param	ComboCaption	A caption appearing to the left of the combo box.
 * @param	bLocalize		If TRUE, localize the window.  Set to FALSE if InDialogTitle and InComboCaption are already localized.
 */
void WxDlgGenericComboEntry::SetTitleAndCaption(const TCHAR* DialogTitle, const TCHAR* ComboCaption, UBOOL bLocalize)
{
	// Set dialog title.
	SetTitle( DialogTitle );

	// Set the text label appearing next to the combo box.
	ComboTextCaption->SetLabel( ComboCaption );

	// Localize the window, if requested.
	if ( bLocalize )
	{
		FLocalizeWindow( this );
	}
}

/**
 * @param	ComboOptions	An array of strings to appear in the combo box.  Must contain at least one element.
 */
void WxDlgGenericComboEntry::PopulateComboBox(const TArray<FString>& ComboOptions)
{
	// Populate the combo box.
	ComboBox->Clear();
	for( INT OptionIndex = 0 ; OptionIndex < ComboOptions.Num() ; ++OptionIndex )
	{
		const FString& Option = ComboOptions( OptionIndex );
		ComboBox->Append( *Option );
	}
	UpdateSelectedString();
}

/**
 * @param	SelectedItem	Index of the item to be selected when the window first appears, in [0,ComboOptions.Num()-1].
 */
void WxDlgGenericComboEntry::SetSelection(INT SelectedItem)
{
	ComboBox->SetSelection( SelectedItem );
	UpdateSelectedString();
}

/**
* @param	SelectedItem	Contents of the item to be selected.  Does nothing if the item couldn't be found.
*/
void WxDlgGenericComboEntry::SetSelection(const TCHAR* SelectedItem)
{
	SelectedIndex = ComboBox->FindString( SelectedItem );
	if ( SelectedIndex >= 0 )
	{
		SetSelection( SelectedIndex );
	}
	UpdateSelectedString();
}

/**
* @return		The string the user either entered or selected from the list
*/
FString WxDlgGenericComboEntry::GetComboBoxString()
{
	return ComboBox->GetValue().c_str();
}

/**
 * Updates the selected string with the current combo box selection.
 */
void WxDlgGenericComboEntry::UpdateSelectedString()
{
	// Copy off the selected string.
	SelectedIndex = ComboBox->GetSelection();
	if ( SelectedIndex >= 0 )
	{
		SelectedString = ComboBox->GetString(SelectedIndex);
	}
	else
	{
		SelectedString.Empty();
	}
}

/**
 * Displays the dialog.
 *
 * @param	DialogTitle		The text to be displayed in the dialog's title bar.
 * @param	ComboCaption	A caption appearing to the left of the combo box.
 * @param	ComboOptions	An array of strings to appear in the combo box.  Must contain at least one element.
 * @param	SelectedItem	Index of the item to be selected when the window first appears, in [0,ComboOptions.Num()-1].
 * @param	bLocalize		If TRUE, localize the window.  Set to FALSE if InDialogTitle and InComboCaption are already localized.
 * @return					wxID_OK if the user selected the "Ok" button.
 */
int WxDlgGenericComboEntry::ShowModal(const TCHAR* DialogTitle, const TCHAR* ComboCaption, const TArray<FString>& ComboOptions, INT SelectedItem, UBOOL bLocalize)
{
	SetTitleAndCaption( DialogTitle, ComboCaption, bLocalize);
	PopulateComboBox( ComboOptions );
	SetSelection( SelectedItem );

	Layout();

	return ShowModal();
}

/**
 * Displays the dialog.
 */
int WxDlgGenericComboEntry::ShowModal()
{
	return wxDialog::ShowModal();
}

/**
 * Responds to the user selecting the "OK" button.
 *
 * @return					wxID_OK if the user selected the "Ok" button.
 */
void WxDlgGenericComboEntry::OnOK(wxCommandEvent& In)
{
	// Copy off the selected string.
	UpdateSelectedString();

	// Pass along the event.
	wxDialog::AcceptAndClose();
}
