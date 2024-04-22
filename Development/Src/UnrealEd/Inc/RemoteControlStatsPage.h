/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __REMOTECONTROLSTATSPAGE_H__
#define __REMOTECONTROLSTATSPAGE_H__

#include "RemoteControlPage.h"

/**
 * Standard RemoteControl stats page.
 */
class WxRemoteControlStatsPage : public WxRemoteControlPage
{
public:
	WxRemoteControlStatsPage(FRemoteControlGame *Game, wxNotebook *Notebook);

	/**
	 * Return's the page's title, displayed on the notebook tab.
	 */
	virtual const TCHAR *GetPageTitle() const;

	/**
	 * Refreshes page contents.
	 */
	virtual void RefreshPage(UBOOL bForce = FALSE);

private:
	/** The instance of the tree control for displaying stats */
	class WxStatTreeCtrl* StatTreeCtrl;

	DECLARE_EVENT_TABLE()
};

#endif // __REMOTECONTROLSTATSPAGE_H__
