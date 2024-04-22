/*============================================================================
	DlgRotateAnimSequence.h: Dialog for rotating UAnimSequence objects.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGROTATEANIMSEQUENCE_H__
#define __DLGROTATEANIMSEQUENCE_H__

/**
 * Presents the user with options for specifying a rotation about an axis
 * to be applied to all tracks of an AnimSequence.
 */
class WxDlgRotateAnimSeq : public wxDialog
{
public:
	WxDlgRotateAnimSeq();

	FLOAT			Degrees;
	EAxis			Axis;

	int ShowModal();
	void OnOK(wxCommandEvent& In);

private:
	wxTextCtrl		*DegreesEntry;
	wxComboBox		*AxisCombo;

	DECLARE_EVENT_TABLE()
};

#endif // __DLGROTATEANIMSEQUENCE_H__
