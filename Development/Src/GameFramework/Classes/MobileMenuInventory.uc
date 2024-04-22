
/**
* MobileMenuInventory
* A container of objects that can be scrolled through.  
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/
class MobileMenuInventory extends MobileMenuObject;

struct RenderElementInfo
{
	var bool bIsDragItem;
	var int	Index;
};

struct DragElementInfo
{
	var bool		bIsDragging;
	var int			IndexFrom;
	var bool		bIsOver;
	var int			IndexOver;
	var bool		bCanDropInOver;
	var Vector2D	OrigTouch;
	var Vector2D	CurTouch;
	var ETouchType	EventType;
};

/** Locations to put items */
var array<MobileMenuElement>	Slots;

/** What items are in inventory - the index matches with what slot it is in */
var array<MobileMenuElement>	Items;

/** Extra amount of distance on each side to detect touch */
var float						SideLeewayPercent;

/** Info about the cur element being rendered. */
var RenderElementInfo			CurrentElement;

/** Data about what is being drug by the finger - if any */
var DragElementInfo				Drag;

/** How this was scaled if any other things are added */
var Vector2D					ScaleSize;

/** If set to false, user should call RenderDragItem() after scene is drawn.*/
var bool						bRenderDragItem;

/* Rather than inheriting, a class can use delegates to allow an item in a slot and be updated when it happens */
delegate OnUpdateItemInSlot(MobileMenuInventory FromInv, int SlotIndex);
delegate bool DoCanPutItemInSlot(MobileMenuInventory FromInv, MobileMenuElement Item, MobileMenuElement ToSlot, int ToIdx, int FromIdx);
delegate OnUpdateDrag(out const DragElementInfo Before, out const DragElementInfo After);

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
	local MobileMenuElement Element;

	// Save before they are modified in InitMenuObject.
	ScaleSize.X = Width;
	ScaleSize.Y = Height;

	Super.InitMenuObject(PlayerInput, Scene, ScreenWidth, ScreenHeight, bIsFirstInitialization);

	// Now see how they were scaled and scale all Slot locations the same way.
	ScaleSize.X = Width/ScaleSize.X;
	ScaleSize.Y = Height/ScaleSize.Y;

	foreach Slots(Element)
	{
		ScaleSlot(Element);
	}
	foreach Items(Element)
	{
		ScaleSlot(Element);
	}
	Items.Length = Slots.Length;
}

/** Add slots to the inventory system - return index */
function int AddSlot(MobileMenuElement Slot)
{
	if (Slot != none)
	{
		Slots.AddItem(Slot);
		if (bHasBeenInitialized)
		{
			ScaleSlot(Slot);
		}
		return Slots.Length - 1;
	}
	return -1;
}

/** Scale slot in same way this object was scaled - use same scaling this MobileMenuObject uses */
private function ScaleSlot(MobileMenuElement Slot)
{
	Slot.VpPos.X *= ScaleSize.X;
	Slot.VpPos.Y *= ScaleSize.Y;
	Slot.VpSize.X *= ScaleSize.X;
	Slot.VpSize.Y *= ScaleSize.Y;
}

/** Inheriting class will need to override this function if there are restrictions on what slot
 * an element can be placed.
 * Owning class can set the DoCanPutItemInSlot().
 *
 * @param Item - Item that wants to go into slot
 * @param ToSlot - Slot to receive element
 * @param ToIdx - Index of Slot - where we want to put element.
 * @param FromIdx - Index of slot element is currently in - if any
 */
function bool CanPutItemInSlot(MobileMenuElement Item, MobileMenuElement ToSlot, int ToIdx, int FromIdx=-1)
{
	if ((Item == none) || (FromIdx == ToIdx) || (ToIdx < 0))
		return false;

	if (DoCanPutItemInSlot != none)
	{
		return DoCanPutItemInSlot(self, Item, ToSlot, ToIdx, FromIdx);
	}
	return true;
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
	local DragElementInfo OrigDrag;

	OrigDrag = Drag;
	Drag.EventType = EventType;

	TouchX -= Left;
	TouchY -= Top;

	Drag.CurTouch.X = TouchX;
	Drag.CurTouch.Y = TouchY;

	switch (EventType)
	{
	case Touch_Began:
		InitDragAt(TouchX, TouchY);
		if (OnUpdateDrag != none)
			OnUpdateDrag(OrigDrag, Drag);
		return true;
	case Touch_Moved:
	case Touch_Stationary:
		if (!Drag.bIsDragging)
		{
			InitDragAt(TouchX, TouchY);
		}
		else
		{
			Drag.IndexOver = FindSlotIndexAt(TouchX, TouchY);
			Drag.bIsOver = Drag.IndexOver >= 0;
		}
		Drag.bCanDropInOver = Drag.bIsOver && CanPutItemInSlot(Items[Drag.IndexFrom], Slots[Drag.IndexOver], Drag.IndexOver, Drag.IndexFrom);
		if (OnUpdateDrag != none)
			OnUpdateDrag(OrigDrag, Drag);
		return true;
	case Touch_Ended:
		if (Drag.bIsDragging)
		{
			// If we were not over something, try one last time,
			// but do not remove it because sometimes when people lift they move too much.
			if (!Drag.bIsOver)
			{
				Drag.IndexOver = FindSlotIndexAt(TouchX, TouchY);
				Drag.bIsOver = Drag.IndexOver >= 0;
			}
			Drag.bCanDropInOver = Drag.bIsOver && CanPutItemInSlot(Items[Drag.IndexFrom], Slots[Drag.IndexOver], Drag.IndexOver, Drag.IndexFrom);
			if (Drag.bCanDropInOver)
			{
				SwapItemsInSlots(Drag.IndexOver, Drag.IndexFrom);
			}
		}
		break;
	case Touch_Cancelled:
		break;

	}
	// Only come here where we are done.
	Drag.bIsDragging = false;
	if (OnUpdateDrag != none)
		OnUpdateDrag(OrigDrag, Drag);

	// Don't change until after the OnUpdateDrag() because it will want to know last state.
	Drag.bCanDropInOver = false;
	Drag.bIsOver = false;
	return true;
}

function bool SwapItemsInSlots(int Slot0, int Slot1)
{
	local MobileMenuElement Element0, Element1;

	Element0 = Items[Slot0];
	Element1 = Items[Slot1];

	if ((Element0 == none) || CanPutItemInSlot(Element0, Slots[Slot1], Slot1, Slot0))
	{
		if ((Element1 == none) || CanPutItemInSlot(Element1, Slots[Slot0], Slot0, Slot1))
		{
			Items[Slot0] = Element1;
			Items[Slot1] = Element0;
			UpdateItemInSlot(Slot0);
			UpdateItemInSlot(Slot1);
			return true;
		}
	}
	return false;
}

function MobileMenuElement AddItemToSlot(MobileMenuElement Element, int ToSlot)
{
	local MobileMenuElement PrevElement;

	if (CanPutItemInSlot(Element, Slots[ToSlot], ToSlot))
	{
		PrevElement = Items[ToSlot];
		Items[ToSlot] = Element;
		UpdateItemInSlot(ToSlot);
		return PrevElement;
	}
	return none;
}

protected function UpdateItemInSlot(int InSlot)
{
	local MobileMenuElement Element, Slot;

	Element = Items[InSlot];
	if (Element != none)
	{
		Slot = Slots[InSlot];
		Element.VpPos = Slot.VpPos;
		Element.VpSize = Slot.VpSize;
	}
	if (OnUpdateItemInSlot != none)
	{
		OnUpdateItemInSlot(self, InSlot);
	}
}

function InitDragAt(int TouchX, int TouchY)
{
	Drag.IndexFrom = FindSlotIndexAt(TouchX, TouchY);
	Drag.bIsDragging = (Drag.IndexFrom >= 0) && (Items[Drag.IndexFrom] != none);
	Drag.IndexOver = Drag.IndexFrom;
	Drag.bIsOver = (Drag.IndexFrom >= 0);
	Drag.bCanDropInOver = false;
	Drag.OrigTouch.X = TouchX;
	Drag.OrigTouch.Y = TouchY;
}

function int FindSlotIndexAt(float X, float Y)
{
	local MobileMenuElement Element;
	local float ExtraX, ExtraY;
	local int Idx;

	Idx = -1;
	foreach Slots(Element)
	{
		Idx++;
		if (Element.bIsActive)
		{
			ExtraX = Element.VpSize.X * SideLeewayPercent;
			ExtraY = Element.VpSize.Y * SideLeewayPercent;

			if (X < (Element.VpPos.X - ExtraX))
				continue;
			if (Y < (Element.VpPos.Y - ExtraY))
				continue;
			if (X > ((Element.VpPos.X + Element.VpSize.X) + ExtraX))
				continue;
			if (Y > ((Element.VpPos.Y + Element.VpSize.Y) + ExtraY))
				continue;

			return Idx;
		}
	}
	return -1;
}

function int GetIndexOfItem(MobileMenuElement Item)
{
	return Items.Find(Item);
}

/**
 * Render the widget
 *
 * @param Canvas - the canvas object for drawing
 */
function RenderObject(canvas Canvas, float DeltaTime)
{
	local MobileMenuElement Element;
	local float OrgX, OrgY;

	OrgX = Canvas.OrgX;
	OrgY = Canvas.OrgY;

	CurrentElement.bIsDragItem = false;
	CurrentElement.Index = 0;
	foreach Slots(Element)
	{
		if (Element.bIsVisible)
		{
			Canvas.SetOrigin(Left + Element.VpPos.X, Top + Element.VpPos.Y);
			Element.RenderElement(self, Canvas, DeltaTime, Opacity);
		}
		CurrentElement.Index++;
	}

	// ForEach does not work because it does not return null slots, therefore CurrentElement.Index cannot be calculated.
	for (CurrentElement.Index = 0; CurrentElement.Index < Items.Length; CurrentElement.Index++)
	{
		Element = Items[CurrentElement.Index];
		if (Element != none && Element.bIsVisible)
		{
			Canvas.SetOrigin(Left + Element.VpPos.X, Top + Element.VpPos.Y);
			Element.RenderElement(self, Canvas, DeltaTime, Opacity);
		}
	}

	// Restore to not mess up next scene.
	Canvas.OrgX  = OrgX;
	Canvas.OrgY  = OrgY;

	if (bRenderDragItem)
		RenderDragItem(Canvas, DeltaTime);
}

function RenderDragItem(canvas Canvas, float DeltaTime)
{
	local MobileMenuElement Element;
	local float OrgX, OrgY;

	if (Drag.bIsDragging)
	{
		OrgX = Canvas.OrgX;
		OrgY = Canvas.OrgY;

		CurrentElement.bIsDragItem = true;
		CurrentElement.Index = Drag.IndexFrom;
		Element = Items[Drag.IndexFrom];
		Canvas.SetOrigin(Left + Drag.CurTouch.X, Top + Drag.CurTouch.Y);
		Element.RenderElement(self, Canvas, DeltaTime, Opacity);

		// Restore to not mess up next scene.
		Canvas.OrgX  = OrgX;
		Canvas.OrgY  = OrgY;
	}
}

defaultproperties
{
	bIsActive=true
	bRenderDragItem=true
	SideLeewayPercent=0.1f
}

