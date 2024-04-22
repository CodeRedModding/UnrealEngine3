/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_MobileMotion extends SeqEvent_MobileBase
	native;

cpptext
{
	/**
	 * Called each frame.  
	 * @param Originator is a reference to the PC that caused the input
	 * @param OriginatorInput is a reference to the mobile player input assoicated with this object
	 */
	void Update(APlayerController* Originator, UMobilePlayerInput* OriginatorInput);
}

var float Roll;
var float Pitch;
var float Yaw;


var float DeltaRoll;
var float DeltaPitch;
var float DeltaYaw;

/**
 * Return the version number for this class.  Child classes should increment this method by calling Super then adding
 * a individual class version to the result.  When a class is first created, the number should be 0; each time one of the
 * link arrays is modified (VariableLinks, OutputLinks, InputLinks, etc.), the number that is added to the result of
 * Super.GetObjClassVersion() should be incremented by 1.
 *
 * @return	the version number for this specific class.
 */
static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="Mobile Motion Access [Old]"
	ObjCategory="Input"

	OutputLinks(0)=(LinkDesc="Input Active")
	VariableLinks(0)=(ExpectedType=class'SeqVar_Float',LinkDesc="Pitch",bWriteable=false,PropertyName=Pitch)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="Yaw",bWriteable=false,PropertyName=Yaw)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Float',LinkDesc="Roll",bWriteable=false,PropertyName=Roll)
	VariableLinks(3)=(ExpectedType=class'SeqVar_Float',LinkDesc="Delta Pitch",bWriteable=false,PropertyName=DeltaPitch)
	VariableLinks(4)=(ExpectedType=class'SeqVar_Float',LinkDesc="Delta Yaw",bWriteable=false,PropertyName=DeltaYaw)
	VariableLinks(5)=(ExpectedType=class'SeqVar_Float',LinkDesc="Delta Roll",bWriteable=false,PropertyName=DeltaRoll)

}