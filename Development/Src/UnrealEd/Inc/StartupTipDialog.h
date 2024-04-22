/*=============================================================================
StartupTipDialog.h: Startup Tip dialog window
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __StartupTipDialog_H__
#define __StartupTipDialog_H__

/** Provides tips to the startup tip dialog. */
class WxLocalizedTipProvider
{
public:
	WxLocalizedTipProvider();
	wxString GetTip(INT &TipNumber);
private:
	class FConfigSection* TipSection;

	/** Cached filename to use to load the tips from */
	FString TipFilename;
};


/** Startup tip dialog, displays a random tip at startup. */
class WxStartupTipDialog : public wxDialog
{
public:
	WxStartupTipDialog(wxWindow* Parent);
	virtual ~WxStartupTipDialog();

	/**
	* @return Whether or not the application should display this dialog at startup.
	*/
	UBOOL GetShowAtStartup() const
	{
		return CheckStartupShow->GetValue();
	}

private:

	/** Saves options for the tip box. */
	void SaveOptions() const;

	/** Loads options for the tip box. */
	void LoadOptions();

	/** Updates the currently displayed tip with a new randomized tip. */
	void UpdateTip();

	/** Updates the tip text with a new tip. */
	void OnNextTip(wxCommandEvent &Event);

	/** Launch Browser with URL specified*/
	void LaunchURL(wxTextUrlEvent&Event);

	/** wxWidgets tip provider interface to get tips for this dialog. */
	WxLocalizedTipProvider TipProvider;

	/** Button to generate a new tip. */
	wxButton* NextTip;

	/** Text control to display tips in. */
	wxTextCtrl* TipText;

	/** Checkbox to determine whether or not we show at startup. */
	wxCheckBox* CheckStartupShow;

	/**State of mouse click over URL*/
	UBOOL bIsMouseDownOverURL;

	/** Index of the current tip. */
	INT CurrentTip;

	DECLARE_EVENT_TABLE()
};


#endif
