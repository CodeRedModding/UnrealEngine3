/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This object is responsible for the display and callbacks associated
 * with handling ingame advertisements
 */
class InGameAdManager extends PlatformInterfaceBase
	native(PlatformInterface);

cpptext
{
	/**
	 * Called by platform when the user clicks on the ad banner. Will pause the game before
	 * calling the delegates
	 */
	void OnUserClickedBanner();

	/**
	 * Called by platform when an opened ad is closed. Will unpause the game before
	 * calling the delegates
	 */
	void OnUserClosedAd();
}


enum EAdManagerDelegate
{
	AMD_ClickedBanner,
	AMD_UserClosedAd,
};

/** If true, the game will pause when the user clicks on the ad, which could take over the screen */
var bool bShouldPauseWhileAdOpen;


/**
 * Perform any needed initialization
 */
native event Init();

/**
 * Allows the platform to put up an advertisement on top of the viewport. Note that 
 * this will not resize the viewport, simply cover up a portion of it.
 *
 * @param bShowOnBottomOfScreen If TRUE, advertisement will be shown on the bottom, otherwise, the top
 */
native function ShowBanner(bool bShowBottomOfScreen);

/**
 * Hides the advertisement banner shown with ShowInGameAdvertisementBanner. If the ad is currently open
 * (ie, the user is interacting with the ad), the ad will be forcibly closed (see ForceCloseInGameAdvertisement)
 */
native function HideBanner();

/**
 * If the game absolutely must close an opened (clicked on) advertisement, call this function.
 * This may lead to loss of revenue, so don't do it unnecessarily.
 */
native function ForceCloseAd();



/**
 * Sets the value of bShouldPauseWhileAdOpen
 */
function SetPauseWhileAdOpen(bool bShouldPause)
{
	bShouldPauseWhileAdOpen = bShouldPause;
}

defaultproperties
{
	bShouldPauseWhileAdOpen=true
}