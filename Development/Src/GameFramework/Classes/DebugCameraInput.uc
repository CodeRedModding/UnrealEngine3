//-----------------------------------------------------------
// * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//-----------------------------------------------------------
class DebugCameraInput extends PlayerInput;

/**
 * Process an input key event routed through unrealscript from another object. This method is assigned as the value for the
 * OnRecievedNativeInputKey delegate so that native input events are routed to this unrealscript function.
 *
 * @param	ControllerId	the controller that generated this input key event
 * @param	Key				the name of the key which an event occured for (KEY_Up, KEY_Down, etc.)
 * @param	EventType		the type of event which occured (pressed, released, etc.)
 * @param	AmountDepressed	for analog keys, the depression percent.
 *
 * @return	true to consume the key event, false to pass it on.
 */
function bool InputKey( int ControllerId, name Key, EInputEvent Event, float AmountDepressed = 1.f, bool bGamepad = FALSE )
{
	local PlayerController		PC;
	local DebugCameraController	DCC;

	foreach WorldInfo.AllControllers(class'PlayerController', PC)
	{
		if ( PC.bIsPlayer && PC.IsLocalPlayerController() )
		{
			DCC = DebugCameraController(PC);
			if( DCC!=none && DCC.OriginalControllerRef==none )
			{
				//dcc are disabled, so we are looking for normal player controller
				continue;
			}

			return DCC.NativeInputKey( ControllerId, Key, Event, AmountDepressed, bGamepad );
		}
	}

	return false;
}


defaultproperties
{
	OnReceivedNativeInputKey=InputKey
}
