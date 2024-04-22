/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGGENERICCOMBOENTRY_H__
#define __DLGGENERICCOMBOENTRY_H__

/**
 * A workhorse "OK/Cancel" dialog containing a combo box populated with strings.
 */
class WxDlgGenericComboEntry : public wxDialog
{
public:
	/**
	 * @param	bDisableCancelButton	[opt] If TRUE, disable (grey out) the dialog's 'Cancel' button.  Default is FALSE.
	 * @param	bReadOnlyComboBox		[opt] If TRUE, the dialog box will be read only.  Default is TRUE.
	 * @param	DialogIdString			allows derived classes to use a different xml resource for this dialog.
	 */
	WxDlgGenericComboEntry( UBOOL bDisableCancelButton=FALSE, UBOOL bReadOnlyComboBox=TRUE, const TCHAR* DialogIdString=NULL );

	/**
	* @return		The string the user either entered or selected from the list
	*/

	FString WxDlgGenericComboEntry::GetComboBoxString();

	/**
	 * @return		The contents of the selected item.
	 */
	const FString& GetSelectedString() const
	{
		return SelectedString;
	}

	/**
	* @return		The index of the selected item.
	*/
	const INT GetSelectedIndex() const
	{
		return SelectedIndex;
	}

	/**
	 * @return		A handle to the dialog's combo box.
	 */
	const wxComboBox& GetComboBox() const
	{
		return *ComboBox;
	}

	/**
	 * @param	DialogTitle		The text to be displayed in the dialog's title bar.
	 * @param	ComboCaption	A caption appearing to the left of the combo box.
	 * @param	bLocalize		If TRUE, localize the window.  Set to FALSE if InDialogTitle and InComboCaption are already localized.
	 */
	void SetTitleAndCaption(const TCHAR* DialogTitle, const TCHAR* ComboCaption, UBOOL bLocalize);

	/**
	 * @param	ComboOptions	An array of strings to appear in the combo box.  Must contain at least one element.
	 */
	void PopulateComboBox(const TArray<FString>& ComboOptions);

	/**
	 * @param	SelectedItem	Index of the item to be selected when the window first appears, in [0,ComboOptions.Num()-1].
	 */
	void SetSelection(INT SelectedItem);

	/**
	 * @param	SelectedItem	Contents of the item to be selected.  Does nothing if the item couldn't be found.
	 */
	void SetSelection(const TCHAR* SelectedItem);

	/**
	 * Displays the dialog.
	 *
	 * @param	DialogTitle		The text to be displayed in the dialog's title bar.
	 * @param	ComboCaption	A caption appearing to the left of the combo box.
	 * @param	ComboOptions	An array of strings to appear in the combo box.   Must contain at least one element.
	 * @param	SelectedItem	Index of the item to be selected when the window first appears, in [0,ComboOptions.Num()-1].
	 * @param	bLocalize		If TRUE, localize the window.  Set to FALSE if InDialogTitle and InComboCaption are already localized.
	 * @return					wxID_OK if the user selected the "Ok" button.
	 */
private:
	using wxDialog::ShowModal;		// Hide parent implementation
public:
	int ShowModal(const TCHAR* DialogTitle, const TCHAR* ComboCaption, const TArray<FString>& ComboOptions, INT SelectedItem, UBOOL bLocalize);

	/**
	 * Displays the dialog.
	 *
	 * @return					wxID_OK if the user selected the "Ok" button.
	 */
	int ShowModal();

protected:
	/** The index of the selected entry in the combo box when SetSelection(...) was called or "OK" was pressed. */
	INT				SelectedIndex;

	/** An image of the last string selected in the combo box when SetSelection(...) was called or "OK" was pressed. */
	FString			SelectedString;

	/** The combo box appearing in the dialog. */
	wxComboBox*		ComboBox;

	/** The text caption appearing beside the caption dialog. */
	wxStaticText*	ComboTextCaption;

	/**
	 * Updates the seleted string with the current combo box selection.
	 */
	void UpdateSelectedString();

	/**
	 * If TRUE, the combobox on this dialog is in "read only" mode, meaning the user has to pick from the preset list of items.
	 */
	UBOOL			bIsReadOnly;

	/**
	 * Responds to the user selecting the "OK" button.
	 */
	void OnOK(wxCommandEvent& In);

	DECLARE_EVENT_TABLE()
};

#endif // __DLGGENERICCOMBOENTRY_H__
