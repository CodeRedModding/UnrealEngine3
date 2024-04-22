/*=============================================================================
	DlgInterpEditorKeyReduction.h: Interpolation key reduction param dialog.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
	Written by Feeling Software inc.
=============================================================================*/

#ifndef __DLGINTERPEDITORKEYREDUCTION_H_
#define __DLGINTERPEDITORKEYREDUCTION_H_

/**
 * Dialog that requests the key reduction parameters from the user.
 * These parameters are tolerance and the reduction interval.
 */
class WxInterpEditorKeyReduction : public wxDialog
{
public:
	WxInterpEditorKeyReduction(wxWindow* InParent, FLOAT IntervalMinimum, FLOAT IntervalMaximum);
	~WxInterpEditorKeyReduction();

	// The return values of the dialog.
	FLOAT Tolerance;
	UBOOL FullInterval;
	FLOAT IntervalStart;
	FLOAT IntervalEnd;

private:
	// The dialog controls.
	wxStaticText* ToleranceLabel;
	wxStaticText* FullIntervalLabel;
	wxStaticText* IntervalStartLabel;
	wxStaticText* IntervalEndLabel;
	wxSpinCtrl* ToleranceControl;
	wxCheckBox* FullIntervalCheckBox;
	wxTextCtrl* IntervalStartControl;
	wxTextCtrl* IntervalEndControl;
	wxButton* OkayButton;
	wxButton* CancelButton;

	// The dialog control event-handlers.
	void OnOkay(wxCommandEvent& In);
	void OnCancel(wxCommandEvent& In);

	DECLARE_EVENT_TABLE()
};

#endif // __DLGINTERPEDITORKEYREDUCTION_H_

