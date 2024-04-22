/*=============================================================================
	DlgProgress.h: UnrealEd dialog for displaying progress for slow operations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGPROGRESS_H__
#define __DLGPROGRESS_H__

/**
  * UnrealEd dialog for displaying progress during slow operations.
  */
class WxDlgProgress : public wxDialog
{
public:
	WxDlgProgress( wxWindow* InParent, UBOOL AddCancelButton=FALSE );
	virtual ~WxDlgProgress();

	/**
	 * Displays the progress dialog.
	 */
	void ShowDialog();
	
	/**
	 * Updates the label displaying the current time.
	 */
	void UpdateTime();

	/**
	 * Sets the text that describes what part of the we are currently on.
	 *
	 * @param InStatusText	Text to set the status label to.
	 */
	void SetStatusText( const TCHAR* InStatusText );

	/**
	 * Sets the progress bar percentage.
	 *
	 *	@param ProgressNumerator		Numerator for the progress meter (its current value).
	 *	@param ProgressDenominitator	Denominator for the progress meter (its range).
	 */
	void SetProgressPercent( INT ProgressNumerator, INT ProgressDenominator );

	/**
	 * Records the application time in seconds; used in display of elapsed time.
	 */
	void MarkStartTime();

	/**
	 * Assembles a string containing the elapsed time.
	 */
	FString BuildElapsedTimeString() const;

	/**
	 * Sets the progress bar to be the top window without taking the focus. 
	 */
	void BringToFront();

	/** Did the user cancel? */
	UBOOL ReceivedUserCancel() { return bReceivedUserCancel; };

private:

	/** Progress bar that shows how much of the operation has finished. */
	wxGauge*		Progress;

	wxStaticText*	TimeText;

	/** Displays some status info about the operation. */
	wxStaticText*	StatusText;

	/** Application time in seconds at which the operation began. */
	DOUBLE			StartTime;

	/** Did the user cancel? */
	UBOOL			bReceivedUserCancel;

	/** Event for the cancel button */
	void OnClose(wxCommandEvent& In);

	DECLARE_EVENT_TABLE()
};

#endif // __DLGPROGRESS_H__
