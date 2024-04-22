/*=============================================================================
	DlgTransform.h: UnrealEd dialog for transforming actors.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGTRANSFORM_H__
#define __DLGTRANSFORM_H__

/** Dialog for transforming level-placed actors. */
class WxDlgTransform : public wxDialog
{
public:
	WxDlgTransform(wxWindow* InParent);
	virtual ~WxDlgTransform();

	/** Values representing translation, rotation or scaling to the transform dialog. */
	enum ETransformMode
	{
		TM_Translate,
		TM_Rotate,
		TM_Scale
	};

	/** Sets whether the dialog applies translation, rotation or scaling. */
	void SetTransformMode(ETransformMode InTransformMode);

	/** @returns		The current transform mode. */
	ETransformMode GetTransformMode() const
	{
		return TransformMode;
	}

private:
	/** Indicates whether the dialog is applying translation, rotation or scaling. */
	ETransformMode TransformMode;

	/** TRUE if the transformation is applied as a delta. */
	UBOOL bIsDelta;

	wxStaticText* XLabel;
	wxStaticText* YLabel;
	wxStaticText* ZLabel;
	wxTextCtrl* XEdit;
	wxTextCtrl* YEdit;
	wxTextCtrl* ZEdit;
	wxButton* ApplyButton;
	wxCheckBox* DeltaCheck;

	DECLARE_EVENT_TABLE();

	/** Updates text labels based on the current transform mode. */
	void UpdateLabels();

	/** Applies the transform to selected actors */
	void ApplyTransform();

	/** Called when the 'Apply' button is clicked; applies transform to selected actors. */
	void OnApplyTransform(wxCommandEvent& In);

	/** Called when the 'Delta' checkbox is clicked; toggles between absolute and relative transformation. */
	void OnDeltaCheck(wxCommandEvent& In);
};

#endif // __DLGTRANSFORM_H__
