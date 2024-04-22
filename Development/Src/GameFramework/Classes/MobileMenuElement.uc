/**
* MobileMenuElement
* Interface for any object that may be stored in some sort of MobileMenu container (List, Inventory)
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class MobileMenuElement extends Object;

/* Location of element in container */
var Vector2D VpPos;
var Vector2D VpSize;

/** Will RenderElement be called for this object? */
var bool bIsVisible;

/** Will be tested to see if finger is over */
var bool bIsActive;

/* Allow item to use touch instead of scrolling.  If Item returns true on first touch, it will receive
 * all input until released.
 * @param EventType - type of event
 * @param TouchX - The X location of the touch event
 * @param TouchY - The Y location of the touch event
 * @param DeltaTime - Time since last touch
 */
function bool OnTouch(ETouchType EventType, float TouchX, float TouchY, float DeltaTime)
{
	return false;
}

/* Render a visible element of the container.
*
* @Param List - List that is rendering item.
* @param Canvas - Render system.
* @Param DeltaTime - Time since last render.
*/
function RenderElement(MobileMenuObject Owner, Canvas Canvas, float DeltaTime, float Opacity);

defaultproperties
{
	bIsVisible=true
	bIsActive=true
}

