/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_MobileLook extends SeqEvent_MobileZoneBase
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


/** Holds the current axis values for the device */
var float Yaw;
var float StickStrength;
var vector RotationVector;

defaultproperties
{
	ObjName="Mobile Look"
	ObjCategory="Input"

	OutputLinks(0)=(LinkDesc="Input Active")
	OutputLinks(1)=(LinkDesc="Input Inactive")
	VariableLinks(0)=(ExpectedType=class'SeqVar_Float',LinkDesc="Yaw",bWriteable=false,PropertyName=Yaw)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="Strength",bWriteable=false,PropertyName=StickStrength)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Rotation Vector",bWriteable=false,PropertyName=RotationVector)
}