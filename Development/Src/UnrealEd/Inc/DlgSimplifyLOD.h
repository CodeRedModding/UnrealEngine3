/*=============================================================================
	DlgSimplifyLOD.h: Dialog for mesh LOD simplification.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DlgSimplifyLOD_h__
#define __DlgSimplifyLOD_h__

/**
 * WxDlgSimplifyLOD - Helps the user determine a quality setting based on desired view distance
 * and the amount of error allowed.
 */
class WxDlgSimplifyLOD : public wxDialog
{
public:
	WxDlgSimplifyLOD( wxWindow* InParent, const TCHAR* DialogTitle );

	/**
	 * Shows the dialog box and waits for the user to respond.
	 * @returns wxID_OK or wxID_CANCEL depending on which button the user dismissed the dialog with.
	 */
	int ShowModal();

	/** 
	 * Computes the desired quality based on the user's input.
	 * @returns the quality to give the desired at error at the specified view distance.
	 */
	FLOAT GetMaxDeviation() const;

	/**
	 * Returns the view distance selected by the user.
	 */
	FLOAT GetViewDistance() const;

private:
	wxSlider* ViewDistanceSlider;
	wxSlider* PixelErrorSlider;

	using wxDialog::ShowModal; // Hide parent implementation

	DECLARE_EVENT_TABLE()
};

#endif // #ifndef __DlgSimplifyLOD_h__
