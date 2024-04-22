/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_MobileSwipe extends SeqEvent_MobileRawInput
	native;

cpptext
{
	/**
	 * Handle a touch event coming from the device. 
	 *
	 * @param Originator		is a reference to the PC that caused the input
	 * @param Handle			the id of the touch
	 * @param Type				What type of event is this
	 * @param TouchpadIndex		The touchpad this touch came from
	 * @param TouchLocation		Where the touch occurred
	 * @param DeviceTimestamp	Input event timestamp from the device
	 */
	void InputTouch(APlayerController* Originator, UINT Handle, UINT TouchpadIndex, BYTE Type, FVector2D TouchLocation, DOUBLE DeviceTimestamp);
}


/** How much tolerance should we give the weak axis in order to consider it a swipe */
var (swipe) float Tolerance;

/** How far does the touch need to travel in order to be consider a swipe */
var (swipe) float MinDistance;

var vector2D InitialTouch;

/** A list of actors that were passed over when the swipe occured */
var (Results) array<actor> TouchedActors;

/** How far should this object track out to hit something */
var(mobile) float TraceDistance;



defaultproperties
{

	ObjName="Mobile Simple Swipes"
	ObjCategory="Input"
	MaxTriggerCount=0
	TraceDistance=20480
	Tolerance=100.0
	MinDistance=5.0
	OutputLinks.Empty
	OutputLinks(0)=(LinkDesc="Swipe Left")
	OutputLinks(1)=(LinkDesc="Swipe Right")
	OutputLinks(2)=(LinkDesc="Swipe Up")
	OutputLinks(3)=(LinkDesc="Swipe Down")

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_ObjectList',LinkDesc="Touched Actors List",bWriteable=true)
}