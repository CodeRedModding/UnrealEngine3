/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_MobileRawInput extends SequenceEvent
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
	virtual void InputTouch(APlayerController* Originator, UINT Handle, UINT TouchpadIndex, BYTE Type, FVector2D TouchLocation, DOUBLE DeviceTimestamp);
}

/** Holds the index in to the multi-touch array that we wish to manage. */
var(Mobile) int TouchIndex;
var(Mobile) int TouchpadIndex;

var float TouchLocationX;
var float TouchLocationY;
var float TimeStamp;

/**
 * Whenever a SeqEvent_MobileBase sequence is created, it needs to find the PlayerInput that is assoicated with it and 
 * add it'self to the list of Kismet sequences looking for input 
 */
event RegisterEvent()
{
	local WorldInfo WI;
	local GamePlayerController GPC;
	local MobilePlayerInput MPI;

	// Use the WorldInfo to find the current local player.  TODO: Add support for specifying which Player to use via Kismet 
	WI = class'WorldInfo'.static.GetWorldInfo();
	if (WI != none)
	{
		foreach WI.LocalPlayerControllers(class'GamePlayerController', GPC)
		{
			MPI = MobilePlayerInput(GPC.PlayerInput);
			if (MPI != none)
			{
				MPI.AddKismetRawInputEventHandler(self);
				break;
			}
		}
	}
}



defaultproperties
{

	ObjName="Mobile Raw Input Access [Old]"
	ObjCategory="Input"
	MaxTriggerCount=0
	OutputLinks(0)=(LinkDesc="Touch Begin")
	OutputLinks(1)=(LinkDesc="Touch Update")
	OutputLinks(2)=(LinkDesc="Touch End")
	OutputLinks(3)=(LinkDesc="Touch Cancel")
	VariableLinks(0)=(ExpectedType=class'SeqVar_Float',LinkDesc="Touch Location X",bWriteable=false,PropertyName=TouchLocationX)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="Touch Location Y",bWriteable=false,PropertyName=TouchLocationY)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Float',LinkDesc="Timestamp",bWriteable=false,PropertyName=Timestamp)
}