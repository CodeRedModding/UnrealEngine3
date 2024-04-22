/*=============================================================================
	DlgBuildProgress.h: UnrealEd dialog for displaying map build progress and cancelling builds.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGBUILDPROGRESS_H__
#define __DLGBUILDPROGRESS_H__

/**
  * UnrealEd dialog for displaying map build progress and cancelling builds.
  */
class WxDlgBuildProgress : public wxDialog
{
public:
	WxDlgBuildProgress( wxWindow* InParent );
	virtual ~WxDlgBuildProgress();

	/** The type of build that is occurring. */
	enum EBuildType
	{
		/** Do not know what is being built... */
		BUILDTYPE_Unknown,
		/** Geometry is being built. */
		BUILDTYPE_Geometry,
		/** Lighting is being built. */
		BUILDTYPE_Lighting,
		/** Paths are being built. */
		BUILDTYPE_Paths,
		/** Fluid surfaces are being build. */
		BUILDTYPE_Fluids
	};

	/** The various issues that can occur. */
	enum EBuildIssueType
	{
		/** A critical error has occurred. */
		BUILDISSUE_CriticalError,
		/** An error has occurred. */
		BUILDISSUE_Error,
		/** A warning has occurred. */
		BUILDISSUE_Warning
	};

	/**
	 *	Displays the build progress dialog.
	 *	@param	InBuildType		The build that is occurring.
	 */
	void ShowDialog(EBuildType InBuildType);
	
	/**
	 *	Sets the current build type.
	 *	@param	InBuildType		The build that is occurring.
	 */
	void SetBuildType(EBuildType InBuildType);
	
	/**
	 * Updates the label displaying the current time.
	 */
	void UpdateTime();
	
	/**
	 * Sets the text that describes what part of the build we are currently on.
	 *
	 * @param StatusText	Text to set the status label to.
	 */
	void SetBuildStatusText( const TCHAR* StatusText );

	/**
	 * Sets the build progress bar percentage.
	 *
	 *	@param ProgressNumerator		Numerator for the progress meter (its current value).
	 *	@param ProgressDenominitator	Denominiator for the progress meter (its range).
	 */
	void SetBuildProgressPercent( INT ProgressNumerator, INT ProgressDenominator );

	/**
	 * Records the application time in seconds; used in display of elapsed build time.
	 */
	void MarkBuildStartTime();

	/**
	 * Assembles a string containing the elapsed build time.
	 */
	FString BuildElapsedTimeString() const;

private:
	/**
	 * Callback for the Stop Build button, stops the current build.
	 */
	void OnStopBuild( wxCommandEvent& In );

	wxBoxSizer* VerticalSizer;

	/** Progress bar that shows how much of the build has finished. */
	wxGauge*		BuildProgress;

	/** Displays the elapsed time for the build */
	wxStaticText*	BuildStatusTime;

	/** Displays some status info about the build. */
	wxStaticText*	BuildStatusText;

	/** The build details button */
	wxStaticBoxSizer* DetailsSizer;

	/** The stop build button */
	wxButton*		StopBuildButton;

	/** Application time in seconds at which the build began. */
	DOUBLE			BuildStartTime;

	/** The type of build that is currently occurring. */
	EBuildType		BuildType;

	/** The warning/error/critical error counts. */
	INT				WarningCount;
	INT				ErrorCount;
	INT				CriticalErrorCount;

	DECLARE_EVENT_TABLE()
};

#endif // __DLGBUILDPROGRESS_H__
