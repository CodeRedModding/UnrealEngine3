/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_MobileObjectPicker extends SeqEvent_MobileRawInput
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

/** How far should this object track out to hit something */
var(mobile) float TraceDistance;

/** Should we check on touch/move as well */
var(mobile) bool bCheckonTouch;

var vector FinalTouchLocation;
var vector FinalTouchNormal;
var object FinalTouchObject;

/** List of objects that we are looking for touches on */
var() array<Object> Targets;

defaultproperties
{
	ObjName="Mobile Object Picker"
	ObjCategory="Input"
	MaxTriggerCount=0
	TraceDistance=20480
	OutputLinks.Empty
	OutputLinks(0)=(LinkDesc="Success")
	OutputLinks(1)=(LinkDesc="Fail")
	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Target",PropertyName=Targets)
}