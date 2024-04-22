/**
* MobileMenuImage
* This is a simple image.
*
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class MobileMenuImage extends MobileMenuObject
	native;

/** Holds the texture to display */
var Texture2D Image;

/** Determines how the image is displayed */
enum MenuImageDrawStyle
{
	IDS_Normal,
	IDS_Stretched,
	IDS_Tile,
};

var MenuImageDrawStyle ImageDrawStyle;

/** Holds the texture UVs.  Note, after InitMenuObject(), these will hold the values to use regardless of the bUseCustomUVs flag */
var UVCoords ImageUVs;

/** Holds the color override for the image */
var LinearColor ImageColor;

/**
 * Render the widget
 *
 * @param Canvas - the canvas object for drawing
 */

function RenderObject(canvas Canvas, float DeltaTime)
{
	local float W, H, U, V, UL, VL;
	local LinearColor DrawColor;
	
	// Set the position
	SetCanvasPos(Canvas);

	// Calculate the default set of rendering params
	if (ImageUVs.bCustomCoords)
	{
		U = ImageUVs.U;
		V = ImageUVs.V;
		UL = ImageUVs.UL;
		VL = ImageUVs.VL;
	}
	else
	{
		U = 0;
		V = 0;
		UL = Image.SizeX;
		VL = Image.SizeY;
	}


	// Determine how we render the image.

	switch (ImageDrawStyle)
	{
		case IDS_Normal: // Clip it
			W = Width > UL ? UL : Width;
			H = Height > VL ? VL : Height;
			UL = W;
			VL = H;
			break;

		case IDS_Stretched:	// Stretch it

			W = Width;
			H = Height;
			break;

		case IDS_Tile: // Tile it
			
			W = Width;
			H = Height;
			UL = W;
			VL = H;
			break;
	}

	DrawColor = ImageColor;
	DrawColor.A *= Opacity * OwnerScene.Opacity;

	Canvas.DrawTile(Image, W,H, U, V, UL, VL, DrawColor);
}


defaultproperties
{
	ImageColor=(r=1.0,g=1.0,b=1.0,a=1.0)
}


