/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_MobileInput extends SeqEvent_MobileZoneBase
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
var float XAxisValue;
var float YAxisValue;

var float CenterX;
var float CenterY;
var float CurrentX;
var float CurrentY;


defaultproperties
{
	ObjName="Mobile Input Access"
	ObjCategory="Input"

	OutputLinks(0)=(LinkDesc="Input Active")
	OutputLinks(1)=(LinkDesc="Input Inactive")
	VariableLinks(0)=(ExpectedType=class'SeqVar_Float',LinkDesc="X-Axis",bWriteable=false,PropertyName=XAxisValue)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="Y-Axis",bWriteable=false,PropertyName=YAxisValue)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Float',LinkDesc="Center.X",bWriteable=false,PropertyName=CenterX)
	VariableLinks(3)=(ExpectedType=class'SeqVar_Float',LinkDesc="Center.Y",bWriteable=false,PropertyName=CenterY)
	VariableLinks(4)=(ExpectedType=class'SeqVar_Float',LinkDesc="Current.X",bWriteable=false,PropertyName=CurrentX)
	VariableLinks(5)=(ExpectedType=class'SeqVar_Float',LinkDesc="Current.Y",bWriteable=false,PropertyName=CurrentY)

}