/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __REMOTECONTROLPAGE_H__
#define __REMOTECONTROLPAGE_H__

// Forward declarations.
class FRemoteControlGame;

/**
 * Baseclass for all RemoteControl pages.
 */
class WxRemoteControlPage : public wxPanel
{
public:
	explicit WxRemoteControlPage(FRemoteControlGame *InGame);
	virtual ~WxRemoteControlPage();

	/**
	 * Returns the page's title, displayed on the notebook tab.
	 */
	virtual const TCHAR *GetPageTitle() const=0;

	/**
	 * Refreshes page contents.
	 * @param bForce - forces an update, even if the page thinks it's already up to date
	 */
	virtual void RefreshPage(UBOOL bForce = FALSE) {}

protected:
	/**
	 * Helper function to load a masked bitmap.
	 * @param	InBitMapName	The bitmap filename, excluding extension.
	 * @param	OutBitmap		[out] Bitmap object receiving the loaded bitmap.
	 * @param	InMaskColor		[opt] The color to use as a transparency mask; default is (0,128,128).
	 */
	static void LoadMaskedBitmap(const TCHAR *BitmapName, WxBitmap& OutBitmap, const wxColor& MaskColor=wxColor(0,128,128));

	/**
	 * Returns the current FRemoteControlGame.
	 */
	FRemoteControlGame *GetGame() const;

	template <class T>
	void BindControl(T *&Control, int ID)
	{
		Control = static_cast<T *>(FindWindow(ID));
		check(NULL != Control);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////
	// Helper functions to go back and forth between wx and Unreal for boolean values.

	/** Initializes the specified boolean choice control. */
	static void SetupBooleanChoiceUI(wxChoice& ChoiceControl);

	/** Updates a boolean choice from an Unreal property. */
	void UpdateBooleanChoiceUI(wxChoice& ChoiceControl, const TCHAR *InClassName, const TCHAR *InPropertyName, const TCHAR *InObjectName=NULL);

	/** Sets a UBOOL property value from the UI value. */
	void SetBooleanPropertyFromChoiceUI(const wxChoice& pChoiceControl, const TCHAR *InClassName, const TCHAR *InPropertyName, const TCHAR *InObjectName=NULL);

	/////////////////////////////////////////////////////////////////////////////////////////////
	// Helper functions to go back and forth between wx and Unreal for resolution values.

	/** Initializes the specified control with the specified resolution. */
	static void SetupResolutionChoiceUI(wxChoice& ChoiceControl, INT MinPowerOf2, INT MaxPowerOf2);

	/** Updates a control containing a resolution from a resolution Unreal property. */
	void UpdateResolutionChoiceUI(wxChoice& ChoiceControl, const TCHAR *InClassName, const TCHAR *InPropertyName, const TCHAR *InObjectName=NULL);

	/** Sets a resolution property value from the UI value.	*/
	void SetResolutionPropertyFromChoiceUI(const wxChoice& ChoiceControl, const TCHAR *InClassName, const TCHAR *InPropertyName, const TCHAR *InObjectName=NULL);

private:
	FRemoteControlGame *Game;
};

#endif // __REMOTECONTROLPAGE_H__
