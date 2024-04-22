/**
* MobileMenuListItem
* Interface for any object that is stored in a MobileMenuList.
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class MobileMenuListItem extends MobileMenuElement;

/** User sets these values and the MobileMenuElement.VpPos and MobileMenuElement.VpSize will be set when (if) rendered */
var float Width;
var float Height;


/* Render a visible element of the container.
*
* @Param List - List that is rendering item.
* @param Canvas - Render system.
* @Param DeltaTime - Time since last render.
*/
function RenderItem(MobileMenuList List, Canvas Canvas, float DeltaTime);


defaultproperties
{
	bIsVisible=true
}

