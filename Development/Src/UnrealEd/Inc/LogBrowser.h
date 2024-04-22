/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LOGBROWSER_H__
#define __LOGBROWSER_H__

class WxLogBrowser : public WxBrowser
{
	DECLARE_DYNAMIC_CLASS(WxBrowser);

public:
	WxLogBrowser(void);

	/**
	 * Forwards the call to our base class to create the window relationship.
	 * Creates any internally used windows after that
	 *
	 * @param DockID the unique id to associate with this dockable window
	 * @param FriendlyName the friendly name to assign to this window
	 * @param Parent the parent of this window (should be a Notebook)
	 */
	virtual void Create(INT DockID,const TCHAR* FriendlyName,wxWindow* Parent);

	/**
	 * Returns the key to use when looking up values
	 */
	virtual const TCHAR* GetLocalizationKey(void) const
	{
		return TEXT("LogBrowser");
	}

protected:
	class WxLogWindow* LogWindow;

private:
	////////////////////
	// Wx events.

	void OnSize( wxSizeEvent& In );

	DECLARE_EVENT_TABLE();
};

#endif // __LOGBROWSER_H__
