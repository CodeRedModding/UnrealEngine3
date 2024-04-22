/**
* MobileMenuObject
* This is the base of all Mobile UI Menu Widgets
*
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class MobileMenuObject extends object
	native;

struct native UVCoords
{
	var() bool bCustomCoords;

	/** The UV coords. */
	var() float U;
	var() float V;
	var() float UL;
	var() float VL;
};

/** If true, the object has been initialized to the screen size (note, it will not work if the screen size changes) */
var transient bool bHasBeenInitialized;

/** The left position of the menu. */
var float Left;

/** The top position of the menu. */
var float Top;

/** The width of the menu. */
var float Width;

/** The height of the menu */
var float Height;

/** Initial location/size, used for resizing the menu */
var float InitialLeft;
var float InitialTop;
var float InitialWidth;
var float InitialHeight;


/** If any of the bRelativeXXXX vars are set, then the value will be considered a percentage of the viewport */
var bool bRelativeLeft;
var bool bRelativeTop;
var bool bRelativeWidth;
var bool bRelativeHeight;

/** If any of these are set, then the Global scsale will be applied */
var bool bApplyGlobalScaleLeft;
var bool bApplyGlobalScaleTop;
var bool bApplyGlobalScaleWidth;
var bool bApplyGlobalScaleHeight;

/** This is the scale factor you are authoring for. 2.0 is useful for Retina display resolution (960x640), 1.0 for iPads and older iPhones */
var (Bounds) float AuthoredGlobalScale;

/** The Leeway values all you to subtle adjust the hitbox for an object.*/
var float TopLeeway;
var float BottomLeeway;
var float LeftLeeway;
var float RightLeeway;

var bool bHeightRelativeToWidth;

/** The XOffset and YOffset can be used to shift the position of the widget within it's bounds. */
var float XOffset;
var float YOffset;

/** Unlike Left/Top/Width/Height the XOffset and YOffsets are assumed to be a percentage of the bounds.  If you
    wish to use actual offsets. set one of the variables below */

var bool bXOffsetIsActual;
var bool bYOffsetIsActual;

/** Holds the tag of this widget */
var string Tag;

/** If true, this control is considered to be active and accepts taps */
var bool bIsActive;

/** If true, this control is hidden and will not be rendered */
var bool bIsHidden;

/** If true, this control is being touched/pressed */
var bool bIsTouched;

/** If true, this control is highlighted (like a radio button) */
var bool bIsHighlighted;

/** A reference to the input owner */
var MobilePlayerInput InputOwner;

/** Holds the opacity of an object */
var float Opacity;

/** The scene this object is in */
var MobileMenuScene OwnerScene;

/** You can set RelativeToTag to the Tag of an object, 
    and this Left,Top is an offset to the Left,Top of RelativeTo*/
var String RelativeToTag;
var MobileMenuObject RelativeTo;

/** Tell scene before we are rendering this Object - Allows rendering between menu objects */
var bool bTellSceneBeforeRendering;

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
	return false;
}

/*
* Figure out real position of object taking in consideration OwnerScnen and RelativeTO
* @param PosX - True X Position
* @param PosY - True Y Position
*/
event GetRealPosition(out float PosX, out float PosY)
{
	if (RelativeTo == none)
	{
		PosX = OwnerScene.Left + Left;
		PosY = OwnerScene.Top + Top;
	}
	else
	{
		RelativeTo.GetRealPosition(PosX, PosY);
		PosX += Left;
		PosY += Top;
	}
}

/**
 * InitMenuObject - Perform any initialization here
 *
 * @param PlayerInput - A pointer to the MobilePlayerInput object that owns the UI system
 * @param Scene - The scene this object is in
 * @param ScreenWidth - The Width of the Screen
 * @param ScreenHeight - The Height of the Screen
 * @param bIsFirstInitialization - If True, this is the first time the menu is being initialized. If False, it's a result of the screen being resized
 */
function InitMenuObject(MobilePlayerInput PlayerInput, MobileMenuScene Scene, int ScreenWidth, int ScreenHeight, bool bIsFirstInitialization)
{
	local int X,Y,W,H,oX,oY,RelativeIdx;
	// First out the bounds.

	InputOwner = PlayerInput;
	OwnerScene = Scene; 

	if (Len(RelativeToTag) > 0)
	{
		RelativeIdx = int(RelativeToTag);
		if (String(RelativeIdx) != RelativeToTag)
		{
			RelativeTo = Scene.FindMenuObject(RelativeToTag);
		}
		else
		{
			RelativeIdx += Scene.MenuObjects.find(self);
			RelativeTo = Scene.MenuObjects[RelativeIdx];
		}
	}

	// don't reinitialize the view coords
	if (!bHasBeenInitialized || !bIsFirstInitialization)
	{
		if (bIsFirstInitialization)
		{
			InitialTop = Top;
			InitialLeft = Left;
			InitialWidth = Width;
			InitialHeight = Height;
		}
		else
		{
			Top = InitialTop;
			Left = InitialLeft;
			Width = InitialWidth;
			Height = InitialHeight;
		}

		X = bRelativeLeft ? Scene.Width * Left : Left;
		Y = bRelativeTop ? Scene.Height * Top : Top;
		W = bRelativeWidth ? Scene.Width * Width : Width;

		if (bHeightRelativeToWidth)
		{
			H = W * Height;
		}
		else
		{
			H = bRelativeHeight ? Scene.Height * Height : Height;
		}

		if (bApplyGlobalScaleLeft)
		{
			X *= Scene.static.GetGlobalScaleX() / AuthoredGlobalScale;
		}
		if (bApplyGlobalScaleTop)
		{
			Y *= Scene.static.GetGlobalScaleY() / AuthoredGlobalScale;
		}
		if (bApplyGlobalScaleWidth)
		{
			W *= Scene.static.GetGlobalScaleX() / AuthoredGlobalScale;
		}
		if (bApplyGlobalScaleHeight)
		{
			H *= Scene.static.GetGlobalScaleY() / AuthoredGlobalScale;
		}

		if (RelativeTo == none)
		{
			if (X<0) X = Scene.Width + X;
			if (Y<0) Y = Scene.Height + Y;
			if (W<0) W = Scene.Width + W;
			if (H<0) H = Scene.Height + H;
		}

		// Copy them back in to place

		Left = X;
		Top = Y;
		Width = W;
		Height = H;

		// Now we figure out the render bounds. To figure out the render bounds, we need the 
		// position + offsets.

		oX = bXOffsetIsActual ? XOffset : Width * XOffset;
		oY = bYOffsetIsActual ? YOffset : Height * YOffset;

		// Calculate the actual render bounds based on the data

		Left -= oX;
		Top -= oY;

		//`log("  InitMenuObject::"$Tag@"["$ScreenWidth@ScreenHeight$"] ["@Left@Top@Width@Height$"]"@OwnerScene@Scene);
	}

	// mark as initialized
	bHasBeenInitialized = TRUE;
}

/** 
 * Call Canvas.SetPos based on Left,Top and taking into consideration the OwnerScene and RelativeTo Object.
 * @param OffsetX - Optional additional Offset to apply
 * @param OffsetX - Optional additional Offset to apply
 */
function SetCanvasPos(Canvas Canvas, optional float OffsetX = 0, optional float OffsetY = 0)
{
	local float PosX, PosY;

	GetRealPosition(PosX, PosY);
	Canvas.SetPos(PosX + OffsetX, PosY + OffsetY);
}

/**
 * Render the widget
 *
 * @param Canvas - the canvas object for drawing
 */

function RenderObject(canvas Canvas, float DeltaTime)
{
//	`log("Object " $ Class $ "." $ Name $ "needs to have a RenderObject function");
}

defaultproperties
{
	Opacity=1.0
	AuthoredGlobalScale=2.0
	bTellSceneBeforeRendering=false
}
