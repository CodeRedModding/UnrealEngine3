/**
* MobileMenuBar
* A container of items that can be selected on a bar - think letters on side of contact list.
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class MobileMenuBar extends MobileMenuObject;

/** Vertical or horizontal list supported */
var(DefaultInit) bool bIsVertical;

/** Current selected index */
var(DefaultInit) int SelectedIndex;

/** First item to draw */
var int				FirstItem;

/** List of items */
var array<MobileMenuBarItem>	Items;

/** Do we need to update before rendering */
var bool bDirty;

/**
* InitMenuObject - Virtual override from base to init object.
*
* @param PlayerInput - A pointer to the MobilePlayerInput object that owns the UI system
* @param Scene - The scene this object is in
* @param ScreenWidth - The Width of the Screen
* @param ScreenHeight - The Height of the Screen
*/
function InitMenuObject(MobilePlayerInput PlayerInput, MobileMenuScene Scene, int ScreenWidth, int ScreenHeight, bool bIsFirstInitialization)
{
	Super.InitMenuObject(PlayerInput, Scene, ScreenWidth, ScreenHeight, bIsFirstInitialization);
	UpdateItemViewports();
}

function AddItem(MobileMenuBarItem Item, Int Index=-1)
{
	if (Index < 0)
	{
		Index = Items.length + (Index + 1);
	}
	Items.InsertItem(Index, Item);
	bDirty = true;
}

function int Num()
{
	return Items.length;
}

function MobileMenuBarItem GetSelected()
{
	if ((SelectedIndex >= 0) && (SelectedIndex < Items.length))
	{
		return Items[SelectedIndex];
	}
	return none;
}

/**
* This event is called when a "touch" event is detected on the object.
* If false is returned (unhanded) event will be passed to scene.
*
* @param EventType - type of event
* @param TouchX - The X location of the touch event
* @param TouchY - The Y location of the touch event
* @param ObjectOver - The Object that mouse is over (NOTE: May be NULL or another object!)
*/
event bool OnTouch(ETouchType EventType, float TouchX, float TouchY, MobileMenuObject ObjectOver, float DeltaTime)
{
	if (bIsVertical)
	{
		TouchY -= Top;
		for (SelectedIndex = FirstItem; SelectedIndex < Items.Length-1; SelectedIndex++)
		{
			TouchY -= Items[SelectedIndex].Height;
			if (TouchY <= 0)
				break;
		}
	}
	else
	{
		TouchX -= Left;
		for (SelectedIndex = 0; SelectedIndex < Items.Length-1; SelectedIndex++)
		{
			TouchX -= Items[SelectedIndex].Width;
			if (TouchX <= 0)
				break;
		}
	}

	return true;
}

/**
 * Render the widget
 *
 * @param Canvas - the canvas object for drawing
 */
function RenderObject(canvas Canvas, float DeltaTime)
{
	local float OrgX, OrgY;
	local int CurIndex;

	if (bDirty)
	{
		UpdateItemViewports();
	}

	OrgX = Canvas.OrgX;
	OrgY = Canvas.OrgY;
	Canvas.SetOrigin(Left, Top);

	// Now render up to (not including) selected, then backwards to and including selected.
	// This is so if we render larger that our VP, the selected on will be top.
	for (CurIndex = 0; CurIndex < Items.Length; CurIndex++)
	{
		RenderItem(Canvas, DeltaTime, CurIndex);
	}

	// Restore to not mess up next scene.
	Canvas.OrgX  = OrgX;
	Canvas.OrgY  = OrgY;
}

/**
* Inheriting class can overload each item itself and not bother with the MobileMenuBarItem doing the rendering 
*
* @param Canvas - the canvas object for drawing
* @param DeltaTime - How much time as passed.
* @param Index - Index of the item.
*/
function RenderItem(Canvas Canvas, float DeltaTime, int ItemIndex)
{
	Items[ItemIndex].RenderItem(self, Canvas, DeltaTime);
}

function SetFirstItem(int First)
{
	FirstItem = First;
	bDirty = true;
}

/** Recalculate size and position of each sub item */
function UpdateItemViewports()
{
	local MobileMenuBarItem Item;
	local float Pos;
	local int CurIndex;

	Pos = 0;
	Width = 0;
	Height = 0;

	if (bIsVertical)
	{
		for (CurIndex = FirstItem; CurIndex < Items.Length; CurIndex++)
		{
			Item = Items[CurIndex];
			Width = FMax(Width, Item.Width);
			Height += Item.Height;
		}
		for (CurIndex = FirstItem; CurIndex < Items.Length; CurIndex++)
		{
			Item = Items[CurIndex];
			Item.VpPos.X = 0;
			Item.VpPos.Y = Pos;
			Pos += Item.Height;
		}
	}
	else
	{
		for (CurIndex = FirstItem; CurIndex < Items.Length; CurIndex++)
		{
			Item = Items[CurIndex];
			Width += Item.Width;
			Height = FMax(Height, Item.Height);
		}
		for (CurIndex = FirstItem; CurIndex < Items.Length; CurIndex++)
		{
			Item = Items[CurIndex];
			Item.VpPos.X = Pos;
			Item.VpPos.Y = 0;
			Pos += Item.Width;
		}
	}

	bDirty = false;
}

defaultproperties
{
	bIsActive=true
	bIsVertical=true
	SelectedIndex=0
}

