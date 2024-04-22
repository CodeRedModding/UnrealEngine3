/**
* MobileMenuLabel
* This is a simple label.  NOTE this label does not support
* word wrap or any additional functionality.  It just renders text
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class MobileMenuLabel extends MobileMenuObject;
	
/** Holds the caption for this label */
var string Caption;

/** Holds the font that will be used to draw the text */
var font TextFont;

/** Hold the color that the font will be displayed in */
var color TextColor;

/** Holds the color of the font when pressed */
var color TouchedColor;

/** Holds the X scaling factor for the label */
var float TextXScale;

/** Holds the Y scaling factor for the label */
var float TextYScale;

/** If true, we will calculate the actual render bounds,etc upon draw */
var bool bAutoSize;

/**
 * Render the widget
 *
 * @param Canvas - the canvas object for drawing
 */

function RenderObject(canvas Canvas, float DeltaTime)
{
	local float CX,CY;
	local float TX, TY;

	CX = Canvas.ClipX;
	CY = Canvas.ClipY;

	Canvas.Font = TextFont;

	if (bAutoSize)
	{
		Canvas.TextSize(Caption, TX, TY);
		Width = TX * TextXScale;
		Height = TY * TextYScale;
	}

	Canvas.DrawColor = bIsTouched ? TouchedColor : TextColor;
	Canvas.DrawColor.A *= Opacity * OwnerScene.Opacity;
	SetCanvasPos(Canvas);
	Canvas.ClipX = Canvas.CurX + Width;
	Canvas.ClipY = Canvas.CurY + Height;
	Canvas.DrawText(Caption,,TextXScale, TextYScale);

	Canvas.ClipX = CX;
	Canvas.ClipY = CY;
}

defaultproperties
{
	TextXScale=1.0
	TextYScale=1.0
	bAutoSize=true
}

