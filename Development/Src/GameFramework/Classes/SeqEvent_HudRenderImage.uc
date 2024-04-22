/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is the base class of all Mobile sequence events.  
 */
class SeqEvent_HudRenderImage extends SeqEvent_HudRender;

/** The color to modulate the text by */
var(HUD) LinearColor DisplayColor;

/** The Location to display the text at */
var(HUD) vector DisplayLocation;

/** The texture to display */
var(HUD) Texture2D DisplayTexture;

/** The Size of the image to display */
var(HUD) float XL, YL;

/** The UVs */
var(HUD) float U,V,UL,VL;

/** 
 * Perform the actual rendering
 */
function Render(Canvas TargetCanvas, Hud TargetHud)
{
	local float UsedX, UsedY, UsedXL, UsedYL;
	local float GlobalScaleX, GlobalScaleY;

	if (bIsActive)
	{
		PublishLinkedVariableValues();

		// cache the global scales
		GlobalScaleX = class'MobileMenuScene'.static.GetGlobalScaleX() / AuthoredGlobalScale;
		GlobalScaleY = class'MobileMenuScene'.static.GetGlobalScaleY() / AuthoredGlobalScale;

		// for floating point values, just multiply it by the canvas size
		// otherwise, apply GlobalScaleFactor, while undoing the scale factor they author at
		UsedX = (DisplayLocation.X < 1.0f) ? DisplayLocation.X * TargetCanvas.SizeX : (DisplayLocation.X * GlobalScaleX);
		UsedY = (DisplayLocation.Y < 1.0f) ? DisplayLocation.Y * TargetCanvas.SizeY : (DisplayLocation.Y * GlobalScaleY);
		UsedXL = (XL <= 1.0f) ? XL * TargetCanvas.SizeX : (XL * GlobalScaleX);
		UsedYL = (YL <= 1.0f) ? YL * TargetCanvas.SizeX : (YL * GlobalScaleY);
		TargetCanvas.SetPos(UsedX, UsedY);
		TargetCanvas.DrawTile(DisplayTexture, UsedXL, UsedYL, U, V, UL, VL, DisplayColor);
	}
}


defaultproperties
{
	ObjName="Draw Image"
	ObjCategory="HUD"

	VariableLinks(2)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Display Location",bWriteable=true,PropertyName=DisplayLocation,MaxVars=1)

}