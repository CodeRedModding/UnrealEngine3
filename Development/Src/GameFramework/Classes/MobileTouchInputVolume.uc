/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MobileTouchInputVolume extends Volume
	implements(TouchableElement3D)
	placeable;

var bool bEnabled;

simulated function OnToggle( SeqAct_Toggle inAction )
{
	if (inAction.InputLinks[0].bHasImpulse)
	{
		bEnabled = true;
	}
	else if (inAction.InputLinks[1].bHasImpulse)
	{
		bEnabled = false;
	}
	else if (inAction.InputLinks[2].bHasImpulse)
	{
		bEnabled = !bEnabled;
	}

	Super.OnToggle(inAction);
}

/** Handle being clicked by the user */
function HandleClick()
{
	if( bEnabled )
	{
		TriggerEventClass( class'SeqEvent_MobileTouchInputVolume', self, 1);
	}
}

/** Handle being double clicked by the user */
function HandleDoubleClick()
{
	if( bEnabled )
	{   
		TriggerEventClass( class'SeqEvent_MobileTouchInputVolume', self, 2);
	}
}


/** Handle a touch moving over this object, and not necessarily tapping or releasing on it */
function HandleDragOver()
{
	if( bEnabled )
	{   
		TriggerEventClass( class'SeqEvent_MobileTouchInputVolume', self, 0);
	}
}

defaultproperties
{
	bBlockActors=false
	bWorldGeometry=false
	bStatic=false

	bEnabled=true
	bCollideActors=True

	SupportedEvents.Empty
	SupportedEvents(0)=class'SeqEvent_MobileTouchInputVolume'
}
