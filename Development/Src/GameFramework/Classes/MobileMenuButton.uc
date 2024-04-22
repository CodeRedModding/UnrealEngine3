/**
* MobileMenuButton
* This is a simple button.  It's an image with 2 states
*
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class MobileMenuButton extends MobileMenuObject;

/** The 2 images that make up the button.  [0] = the untouched, [1] = touched */
var Texture2D Images[2];

/** The UV Coordinates for the images.  [0] = the untouched, [1] = touched */
var UVCoords ImagesUVs[2];

/** Holds the color override for the image */
var LinearColor ImageColor;

/** Localizable caption for the button */
var string Caption;

/** Holds the color for the caption */
var LinearColor CaptionColor;

function InitMenuObject(MobilePlayerInput PlayerInput, MobileMenuScene Scene, int ScreenWidth, int ScreenHeight, bool bIsFirstInitialization)
{
	local int i;
	Super.InitMenuObject(PlayerInput, Scene, ScreenWidth, ScreenHeight,bIsFirstInitialization);

	for (i=0;i<2;i++)
	{
		if (!ImagesUVs[i].bCustomCoords && Images[i] != none)
		{
			ImagesUVs[i].U = 0.0f;
			ImagesUVs[i].V = 0.0f;
			ImagesUVs[i].UL = Images[i].SizeX;
			ImagesUVs[i].VL = Images[i].SizeY;
		}
	}
}


/**
 * Render the widget
 *
 * @param Canvas - the canvas object for drawing
 */

function RenderObject(canvas Canvas, float DeltaTime)
{
	local int Idx;
	local LinearColor DrawColor;


	Idx = (bIsTouched || bIsHighlighted) ? 1 : 0;
	SetCanvasPos(Canvas);
	Drawcolor = ImageColor;
	Drawcolor.A *= Opacity * OwnerScene.Opacity;
	Canvas.DrawTile(Images[Idx], Width, Height,ImagesUVs[Idx].U, ImagesUVs[Idx].V, ImagesUVs[Idx].UL, ImagesUVs[Idx].VL, DrawColor);

	RenderCaption(Canvas);
}

/**
 * Render the optional caption on top of the widget
 *
 * @param Canvas - the canvas object for drawing
 */
function RenderCaption(canvas Canvas)
{
	local float UL,VL;

	if (Caption != "")
	{
		Canvas.Font = OwnerScene.SceneCaptionFont;
		Canvas.TextSize(Caption,UL,VL);

		SetCanvasPos(Canvas, (Width / 2) - (UL/2), (Height /2) - (VL/2));

		Canvas.DrawColor.R = byte(CaptionColor.R * 255.0);
		Canvas.DrawColor.G = byte(CaptionColor.G * 255.0);
		Canvas.DrawColor.B = byte(CaptionColor.B * 255.0);
		Canvas.DrawColor.A = byte(CaptionColor.A * 255.0);

		Canvas.DrawText(Caption);
	}
}

defaultproperties
{
	ImageColor=(r=1.0,g=1.0,b=1.0,a=1.0)
	CaptionColor=(r=0.0,g=0.0,b=0.0,a=1.0)
	bIsActive=true;
}

