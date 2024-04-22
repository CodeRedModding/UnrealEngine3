/*=============================================================================
	DlgInterpEditorKeyReduction.cpp: Interpolation key reduction param dialog.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
	Written by Feeling Software inc.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgInterpEditorKeyReduction.h"

BEGIN_EVENT_TABLE(WxInterpEditorKeyReduction, wxDialog)
	EVT_BUTTON(wxID_OK, WxInterpEditorKeyReduction::OnOkay)
	EVT_BUTTON(wxID_CANCEL, WxInterpEditorKeyReduction::OnCancel)
END_EVENT_TABLE()


WxInterpEditorKeyReduction::WxInterpEditorKeyReduction(wxWindow* InParent, FLOAT IntervalMinimum, FLOAT IntervalMaximum) :
	wxDialog(InParent, wxID_ANY, TEXT("DlgInterpEditorKeyReductionTitle"), wxDefaultPosition, wxSize(203, 138), wxCAPTION),
	Tolerance(5.0f), FullInterval(TRUE), IntervalStart(IntervalMinimum), IntervalEnd(IntervalMaximum),
	ToleranceLabel(NULL), FullIntervalLabel(NULL), IntervalStartLabel(NULL), IntervalEndLabel(NULL),
	ToleranceControl(NULL), FullIntervalCheckBox(NULL), IntervalStartControl(NULL), IntervalEndControl(NULL),
	OkayButton(NULL), CancelButton(NULL)
{
	static const UINT RowHeight = 18;

	ToleranceLabel = new wxStaticText(this, wxID_ANY, TEXT("DlgInterpEditorKeyReductionTolerancePercent"), wxPoint(3, 3), wxSize(138, RowHeight));
	FullIntervalLabel = new wxStaticText(this, wxID_ANY, TEXT("DlgInterpEditorFullInterval"), wxPoint(3, 5 + RowHeight), wxSize(138, RowHeight));
	IntervalStartLabel = new wxStaticText(this, wxID_ANY, TEXT("DlgInterpEditorIntervalStart"), wxPoint(3, 7 + RowHeight * 2), wxSize(138, RowHeight));
	IntervalEndLabel = new wxStaticText(this, wxID_ANY, TEXT("DlgInterpEditorIntervalEnd"), wxPoint(3, 9 + RowHeight * 3), wxSize(138, RowHeight));
	
	ToleranceControl = new wxSpinCtrl(this, wxID_ANY, TEXT("5"), wxPoint(141, 3), wxSize(58, RowHeight), 0x100, 0, 100, 5);
	FullIntervalCheckBox = new wxCheckBox(this, wxID_ANY, TEXT(""), wxPoint(141, 5 + RowHeight), wxSize(58, RowHeight), wxCHK_2STATE);
	IntervalStartControl = new wxTextCtrl(this, wxID_ANY, TEXT(""), wxPoint(141, 7 + RowHeight * 2), wxSize(58, RowHeight));
	IntervalEndControl = new wxTextCtrl(this, wxID_ANY, TEXT(""), wxPoint(141, 9 + RowHeight * 3), wxSize(58, RowHeight));

	OkayButton = new wxButton(this, wxID_OK, TEXT("OK"), wxPoint(82, 13 + RowHeight * 4), wxSize(48, 25));
	CancelButton = new wxButton(this, wxID_CANCEL, TEXT("Cancel"), wxPoint(134, 13 + RowHeight * 4), wxSize(64, 25));

	FLocalizeWindow( this );
	FWindowUtil::LoadPosSize( TEXT("DlgInterpEditorKeyReductionTitle"), this );
	Layout();
	OkayButton->SetDefault();
	SetDefaultItem(OkayButton);

	ToleranceLabel->SetWindowStyle(wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
	FullIntervalLabel->SetWindowStyle(wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
	IntervalStartLabel->SetWindowStyle(wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
	IntervalEndLabel->SetWindowStyle(wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);

	TCHAR StringValue[MAX_SPRINTF]=TEXT("");
	appSprintf(StringValue, TEXT("%.2f"), IntervalMinimum);
	IntervalStartControl->SetValue(StringValue);
	appSprintf(StringValue, TEXT("%.2f"), IntervalMaximum);
	IntervalEndControl->SetValue(StringValue);
	FullIntervalCheckBox->SetValue(true);
}

WxInterpEditorKeyReduction::~WxInterpEditorKeyReduction()
{
}

void WxInterpEditorKeyReduction::OnOkay(wxCommandEvent& In)
{
	// Parse the dialog control values.
	Tolerance = (FLOAT) ToleranceControl->GetValue();
	FullInterval = (UBOOL) FullIntervalCheckBox->GetValue();
	IntervalStart = appAtof(IntervalStartControl->GetValue().c_str());
	IntervalEnd = appAtof(IntervalEndControl->GetValue().c_str());

	wxDialog::EndDialog(TRUE);
}

void WxInterpEditorKeyReduction::OnCancel(wxCommandEvent& In)
{
	wxDialog::EndDialog(FALSE);
}

