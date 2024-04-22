/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "DlgGenericComboEntry.h"

#ifndef __DLGGENERICSTRINGCOMBO_H__
#define __DLGGENERICSTRINGCOMBO_H__

/**
 * Simple dialog which combines an editbox and a combo populated with strings.
 */
class WxDlgGenericStringCombo : public WxDlgGenericComboEntry
{
public:
	/**
	 * @param	bDisableCancelButton	specify TRUE to have the dialog's Cancel button greyed out (disabled)
	 * @param	bReadOnlyComboBox		specify TRUE to indicate that the user should only be able to choose from existing options in the combobox
	 *									(doesn't affect the editbox)
	 */
	WxDlgGenericStringCombo( UBOOL bDisableCancelButton=FALSE, UBOOL bReadOnlyComboBox=TRUE );

	/** Destructor */
	~WxDlgGenericStringCombo();

	/**
	 * @return	the current value of the edtibox
	 */
	FString GetEditBoxString() const
	{
		return EnteredString.c_str();
	}
	/**
	 * @return	a reference to the dialog's string editbox.
	 */
	wxTextCtrl& GetEditBoxControl() const
	{
		return *StringEntry;
	}
	/**
	 * @return	a reference to the dialog editbox's caption
	 */
	wxStaticText& GetEditBoxCaptionControl() const
	{
		return *StringCaption;
	}

	/**
	 * Programatically sets the editbox text to the specified string.
	 */
	void SetEditboxText( const TCHAR* EditboxString );

	/**
	 * Assigns a WxNameTextValidator to the editbox or clears an existing WxNameTextValidator
	 *
	 * @param	a bitmask of ENameValidationType members indicating which characters are valid
	 */
 	void SetEditboxValidatorMask( DWORD ValidationMask );

	/**
	 * Displays the dialog.
	 *
	 * @param	DialogTitle		The text to be displayed in the dialog's title bar.
	 * @param	EditboxCaption	The text to use for the editbox caption.
	 * @param	EditboxValue	The text to use as the initial value for the editbox
	 * @param	ComboCaption	A caption appearing to the left of the combo box.
	 * @param	ComboOptions	An array of strings to appear in the combo box.   Must contain at least one element.
	 * @param	SelectedItem	Index of the item to be selected when the window first appears, in [0,ComboOptions.Num()-1].
	 * @param	bLocalize		If TRUE, localize the window.  Set to FALSE if InDialogTitle and InComboCaption are already localized.
	 * @return					wxID_OK if the user selected the "Ok" button.
	 */
	int ShowModal(const TCHAR* DialogTitle, const TCHAR* EditboxCaption, const TCHAR* EditboxValue, const TCHAR* ComboCaption, const TArray<FString>& ComboOptions, INT SelectedItem, UBOOL bLocalize);
private:
	using WxDlgGenericComboEntry::ShowModal;		// Hide parent implementation
public:

protected:
	/** The string entered into the editbox */
	wxString		EnteredString;

	/** the editbox for the string */
	wxTextCtrl*		StringEntry;

	/** the caption for the editbox */
	wxStaticText*	StringCaption;

private:
	/**
	 * Responds to the user selecting the "OK" button.
	 */
	void OnOK(wxCommandEvent& In);

	DECLARE_EVENT_TABLE()
};

#endif // __DLGGENERICSTRINGCOMBO_H__
