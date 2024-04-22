/**
* MobileMenuBarItem
* Interface for any object that is stored in a MobileMenuBar.
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class MobileMenuBarItem extends Object;

var int	Width;
var int Height;

// Set before being rendered - if was not rendered, then it is invalid.
var Vector2D VpPos;

/* Render a visible item in the bar (only called if visible)
 *
 * @Param Bar - Bar that is rendering item.
 * @param Canvas - Render system.
 * @Param DeltaTime - Time since last render.
*/
function RenderItem(MobileMenuBar Bar, Canvas Canvas, float DeltaTime)
{
}

defaultproperties
{
}

