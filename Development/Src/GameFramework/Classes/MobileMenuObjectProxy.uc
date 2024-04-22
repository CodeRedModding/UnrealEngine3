/**
* MobileMenuObjectProxy - 
* Allow another class to handle touches and draws.
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class MobileMenuObjectProxy extends MobileMenuObject;

delegate bool OnTouchEvent(MobileMenuObjectProxy Proxy, ETouchType EventType, float TouchX, float TouchY, MobileMenuObject ObjectOver, float DeltaTime);
delegate OnRenderObject(MobileMenuObjectProxy Proxy, canvas Canvas, float DeltaTime);

/**
* This event is called when a "touch" event is detected on the object.
* If false is returned (unhandled) event will be passed to scene.
*
* @param EventType - type of event
* @param TouchX - The X location of the touch event
* @param TouchY - The Y location of the touch event
* @param ObjectOver - The Object that mouse is over (NOTE: May be NULL or another object!)
* @param DeltaTime - Time since last update.
*/
event bool OnTouch(ETouchType EventType, float TouchX, float TouchY, MobileMenuObject ObjectOver, float DeltaTime)
{
	if (OnTouchEvent != none)
		return OnTouchEvent(self, EventType, TouchX, TouchY, ObjectOver, DeltaTime);
	return false;
}

/**
* Render the widget
*
* @param Canvas - the canvas object for drawing
*/
function RenderObject(canvas Canvas, float DeltaTime)
{
	if (OnRenderObject != none)
		OnRenderObject(self, Canvas, DeltaTime);
}

defaultproperties
{
}
