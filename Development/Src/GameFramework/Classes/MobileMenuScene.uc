/**
* MobileMenuScene
* This is the base class for the mobile menu system
*
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class MobileMenuScene extends object
	native;

var (UI) string MenuName;
var (UI) instanced array<MobileMenuObject> MenuObjects;

/** Allows for a single font for all buttons in a scene */
var (UI) font SceneCaptionFont;

/** A reference to the input owner */
var (UI) MobilePlayerInput InputOwner;

var (UI) bool bSceneDoesNotRequireInput;

/** Which touchpad this menu will respond to */
var (UI) byte TouchpadIndex;

/** Positions and sizing */
var (Positions) float Left;
var (Positions) float Top;
var (Positions) float Width;
var (Positions) float Height;

/** Initial location/size, used for resizing the menu */
var float InitialLeft;
var float InitialTop;
var float InitialWidth;
var float InitialHeight;

var (Options) bool bRelativeLeft;
var (Options) bool bRelativeTop;
var (Options) bool bRelativeWidth;
var (Options) bool bRelativeHeight;

var (Options) bool bApplyGlobalScaleLeft;
var (Options) bool bApplyGlobalScaleTop;
var (Options) bool bApplyGlobalScaleWidth;
var (Options) bool bApplyGlobalScaleHeight;

/** This is the scale factor you are authoring for. 2.0 is useful for Retina display resolution (960x640), 1.0 for iPads and older iPhones */
var (Options) float AuthoredGlobalScale;

/** The general opacity of the scene */
var (Options) float Opacity;

/** Holds a reference to the sound to play when a touch occurs in the mobile menu system */
var (Sounds) SoundCue UITouchSound;

/** Holds a reference to the sound to play when a touch occurs in the mobile menu system */
var (Sounds) SoundCue UIUnTouchSound;



cpptext
{

	/**
	 * Performs a hit test against all of the objects in the object stack.  
	 *
	 * @param TouchX - The X location of the touch event
	 * @param TouchY - The Y location of the touch event
	 * @param Returns the object that hits.                                                                     
	 */
	virtual UMobileMenuObject* HitTest(FLOAT TouchX, FLOAT TouchY);
}

/** Native functions to get the global scale to apply to UI elements that desire such */
native static final function float GetGlobalScaleX();
native static final function float GetGlobalScaleY();


/**
 * Script events that allows for menu setup.  It's called at the beginning of the native InitMenuScene.  Nothing is set at this point and
 * allows the scene to override default settings
 * @param PlayerInput - A pointer to the MobilePlayerInput object that owns the UI scene
 * @param ScreenWidth - The Width of the Screen
 * @param ScreenHeight - The Height of the Screen
 * @param bIsFirstInitialization - If True, this is the first time the menu is being initialized. If False, it's a result of the screen being resized
 */

event InitMenuScene(MobilePlayerInput PlayerInput, int ScreenWidth, int ScreenHeight, bool bIsFirstInitialization)
{
	local int i,X,Y,W,H;

	//`log("### InitMenuScene"@ MenuName @ PlayerInput @ ScreenWidth @ ScreenHeight @ MenuObjects.Length);

	SceneCaptionFont = GetSceneFont();

	InputOwner = PlayerInput;

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

	X = (bRelativeLeft) ? ScreenWidth * Left : Left;
	Y = (bRelativeTop) ? ScreenHeight * Top : Top;
	W = (bRelativeWidth) ? ScreenWidth * Width : Width;
	H = (bRelativeHeight) ? ScreenHeight * Height : Height;

	if (bApplyGlobalScaleLeft)
	{
		X *= static.GetGlobalScaleX() / AuthoredGlobalScale;
	}
	if (bApplyGlobalScaleTop)
	{
		Y *= static.GetGlobalScaleY() / AuthoredGlobalScale;
	}
	if (bApplyGlobalScaleWidth)
	{
		W *= static.GetGlobalScaleX() / AuthoredGlobalScale;
	}
	if (bApplyGlobalScaleHeight)
	{
		H *= static.GetGlobalScaleY() / AuthoredGlobalScale;
	}

	// We now have the zone positions converted in to actual screen positions.
	// If the zone position is negative, it's right/bottom justified so handle it and store the final values back

	Left = X >= 0 ? X : X + ScreenWidth;
	Top = Y >= 0 ? Y : Y + ScreenHeight;
	Width = W >= 0 ? W : W + ScreenWidth;
	Height = H >= 0 ? H : H + ScreenHeight;

	for (i=0;i<MenuObjects.Length;i++)
	{
		MenuObjects[i].InitMenuObject(InputOwner,self,ScreenWidth,ScreenHeight,bIsFirstInitialization);
	}
}


/**
 * Override this function to change the font used by this Scene
 *
 * @return the Font to use for this scene
 */
function Font GetSceneFont()
{
	return class'Engine'.Static.GetSmallFont();
}


/**
 * Render the scene
 *
 * @param Canvas - the canvas object for drawing
 */
function RenderScene(Canvas Canvas,float RenderDelta)
{
	local MobileMenuObject MenuObject;

	foreach MenuObjects(MenuObject)
	{
		if (MenuObject.bTellSceneBeforeRendering)
		{
			PreRenderMenuObject(MenuObject, Canvas, RenderDelta);
		}
		if (!MenuObject.bIsHidden)
		{
			MenuObject.RenderObject(Canvas, RenderDelta);
		}
	}
}

/** Allow user to render between layers. */
function PreRenderMenuObject(MobileMenuObject MenuObject, Canvas Canvas,float RenderDelta)
{
}

/**
 * This event is called when a touch event is detected on an object if control did not handle it.
 *
 * @param Sender - The Object that swallowed the touch
 * @param EventType - type of event
 * @param TouchX - The X location of the touch event
 * @param TouchY - The Y location of the touch event
 * @param Sender.bIsTouched - Is touch currently over the object
 */
event OnTouch(MobileMenuObject Sender, ETouchType EventType, float TouchX, float TouchY)
{
}

/**
 * Allows the scene to manage touches that aren't sent to any given control.
 * This will pass indicate if the touch was inside its bounds or not.
 */
event bool OnSceneTouch(ETouchType EventType, float TouchX, float TouchY, bool bInside)
{
	// If inside, return true to swallow input.
	return false;
}

/**
 * Opened will be called after the scene is opened and initialized.
 * 
 * @param Mode Optional string to pass to the scene for however it wants to use it
 */
function Opened(string Mode) {}

/**
 * Called when this menu is the topmost menu (ie, when opened or when one of top was closed)
 */
function MadeTopMenu() {}

/**
 * Closing will be called before the closing process really begins.  Return false if
 * you wish to override the closing process.

 * @returns true if we can close
 */
function bool Closing()
{
	return true;
}

/**
 * Closed will be called when the closing process is done and the scene has been removed from the stack
 */
function Closed() 
{
	CleanUpScene();
}

native function CleanUpScene();

/**
 * Search the menu stack for a object                                                                     
 *
 * @param Tag - The name of the object to find.
 * @returns the object
 */
function MobileMenuObject FindMenuObject(string Tag)
{
	local int idx;
	for (idx=0;idx<MenuObjects.Length;idx++)
	{
		if (Caps(MenuObjects[idx].Tag) == Caps(Tag))
		{
			return MenuObjects[idx];
		}
	}

	return none;
}

/**
 * Allows menus to handle exec commands 
 *
 * @Param Command - The command to handle
 * @returns true if handled
 */
function bool MobileMenuCommand(string Command)
{
	return false;
}

defaultproperties
{
	AuthoredGlobalScale=2.0
	Opacity=1.0
	bSceneDoesNotRequireInput=false;
}

