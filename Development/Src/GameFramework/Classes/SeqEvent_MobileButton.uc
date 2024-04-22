/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_MobileButton extends SeqEvent_MobileZoneBase
	native;

cpptext
{
	/**
	 * Called each frame.  
	 * @param Originator is a reference to the PC that caused the input
	 * @param OriginatorInput is a reference to the mobile player input assoicated with this object
	 * @param OriginatorZone is a reference to the zone that caused this update
	 */
	void UpdateZone(APlayerController* Originator, UMobilePlayerInput* OriginatorInput, UMobileInputZone* OriginatorZone);
}

/** TRUE if the zone was active last frame (for tracking edges) */
var bool bWasActiveLastFrame;

/** If TRUE, the Input Pressed output will only trigger when a touch first happens, not every frame */
var () bool bSendPressedOnlyOnTouchDown;

/** If TRUE, the Input Pressed output will only trigger when a touch ends, not every frame. MAKE SURE RETRIGGER DELAY IS 0!!! */
var () bool bSendPressedOnlyOnTouchUp;

defaultproperties
{
	ObjName="Mobile Button Access"
	ObjCategory="Input"

	OutputLinks(0)=(LinkDesc="Input Pressed")
	OutputLinks(1)=(LinkDesc="Input Not Pressed")
}