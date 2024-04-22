/**
* MobileMenuList
* A container of objects that can be scrolled through.  
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class MobileMenuList extends MobileMenuObject;

struct SelectedMenuItem
{
	/** Currently 'Selected' based off of position */
	var int	Index;

	/** The selected item may be not be right on the 'selected' location, this is its offset */
	var float Offset;

	/** Was the selected limited how far it dragged because it was end of the list **/
	var bool bEndOfList;
};

struct DragHistoryData
{
	var float TouchTime;
	var float TouchCoord;
};

const NumInDragHistory=4;
struct MenuListDragInfo
{
	/** Are we currently dragging?  If not, still may be used if ScrollSpeed != 0*/
	var bool				bIsDragging;

	/** Item that was initially pressed.  If !bIsDragging, then this item will use all input */
	var MobileMenuListItem	TouchedItem;

	/** Saved off to recalculate position */
	var SelectedMenuItem	OrigSelectedItem;

	/** Where did user start to drag at? */ 
	var Vector2D			StartTouch;

	/** Amount of time for the touch */
	var float				TouchTime;

	/** How far from orig position we are at */
	var float				ScrollAmount;

	/** Tracks if user moved up, then down - ScrollAmount might be 0, but AbsScrollAmount migh have a value*/
	var float				AbsScrollAmount;

	/** To smooth out the release velocity */
	var DragHistoryData		UpdateHistory[NumInDragHistory];

	/** Number of Update calls (not press or release) to index into History */
	var int					NumUpdates;

	/** See if the selected one has changed, if not, treat it as a touch when release */
	var bool				bHasSelectedChanged;
};

struct MenuListMovementInfo
{
	/* Are we automatically moving the list? */
	var bool			bIsMoving;

	/** Saved off to recalculate position */
	var SelectedMenuItem	OrigSelectedItem;

	/** How many pixels total to scroll */
	var float			FullMovement;

	/** Total time until it is done scrolling */
	var float			TotalTime;

	/** How much time we are at */
	var float			CurrentTime;
};


/** Vertical or horizontal list supported */
var(DefaultInit) bool bIsVerticalList;

/** On short list, might want to disable all scrolling */
var(DefaultInit) bool bDisableScrolling;

/** Offset from Top/Left of list that determines 'selected' item - init as percentage of Width/Height depending on bIsVerticalList */
var(DefaultInit) float SelectedOffset;

/** When user stops moving, should the closest item move to the selected position */
var(DefaultInit) bool bForceSelectedToLineup;

var array<MobileMenuListItem>	Items;

/** Current position of our list */
var SelectedMenuItem		SelectedItem;

/** User changing position of list */
var MenuListDragInfo	Drag;

/** Automatic movement of list (after user drag) */
var MenuListMovementInfo Movement;

/** How fast to Deaccelerate when user releases - cannot be 0*/
var float	Deacceleration;

/** How to we slow down while deaccelerate */
var float EaseOutExp;

/** Sometimes rendering item needs this */
var IntPoint ScreenSize;

/** When user taps on an options, should we scroll to it?  Typically, when there is no obvious 'selected', you don't want to */
var bool bTapToScrollToItem;

/** Behaves like a slot machine wheel, continually loops. */
var bool bLoops;

/** Index of first and last visible according to what just rendered. */
var int FirstVisible, LastVisible;

/** To not allow scrolling past end of lists */
var int NumShowEndOfList;

/** How much to decrease scroll when at the end of a list.  1.0 is none, 0.5 is half, */
var float EndOfListSupression;

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
	ScreenSize.X = ScreenWidth;
	ScreenSize.Y = ScreenHeight;
	Super.InitMenuObject(PlayerInput, Scene, ScreenWidth, ScreenHeight, bIsFirstInitialization);
	SelectedOffset *= (bIsVerticalList) ? Height : Width;
}

function AddItem(MobileMenuListItem Item, Int Index=-1)
{
	if (Index < 0)
	{
		Index = Items.length + (Index + 1);
	}
	Items.InsertItem(Index, Item);
}

function int Num()
{
	return Items.length;
}

function MobileMenuListItem GetSelected()
{
	local MobileMenuListItem Item;
	if ((SelectedItem.Index >= 0) && (SelectedItem.Index < Items.length))
	{
		Item = Items[SelectedItem.Index];
		if (Item != none && !Item.bIsVisible)
			Item = none;
		return Item;
	}
	return none;
}

/*
* If item is selected, 0 > RetValue >= 1.0.
* Useful for changing alpha or size of selected item.
*/
function float GetAmountSelected(MobileMenuListItem Item)
{
	local MobileMenuListItem Selected;
	local float Half;

	Selected = GetSelected();
	if (Item == Selected)
	{
		Half = (bIsVerticalList ? Item.Height : Item.Width) * 0.5f;
		return FMax(0.0001f, FMin(1.0f, 1.0f - (Abs(SelectedItem.Offset) / Half))); 
	}
	return 0.0f;
}

/**
 * Find the visible index of selected item.  In other words, number of
 * visible items before selected item.
 */
function int GetVisibleIndexOfSelected()
{
	local MobileMenuListItem Item, Selected;
	local int Index;

	Selected = GetSelected();
	Index = 0;
	foreach Items(Item)
	{
		if (Item == Selected)
		{
			return Index;
		}
		if (Item.bIsVisible)
		{
			Index++;
		}
	}
	return -1;
}

/**
  * Set the selected item to the visible item with VisibleIndex visible items before it
  */
function int SetSelectedToVisibleIndex(int VisibleIndex)
{
	local int Index;

	for (Index = 0; Index < Items.Length; Index++)
	{
		if (Items[Index].bIsVisible)
		{
			if (VisibleIndex <= 0)
			{
				SelectedItem.Index = Index;
				return Index;
			}
			VisibleIndex--;
		}
	}
	SelectedItem.Index = -1;
	return -1;
}

function int GetNumVisible()
{
	local int Index, Count;

	for (Index = 0; Index < Items.Length; Index++)
	{
		if (Items[Index].bIsVisible)
		{
			Count++;
		}
	}
	return Count;
}

// A little hack, forcing all.  Perhaps it should always be set to true,
// but this is nearly last bug before we ship.
function bool SetSelectedItem(int ItemIndex, bool bForceAll=false)
{
	if ((ItemIndex >= 0) && (ItemIndex < Items.Length))
	{
		if (Items[ItemIndex].bIsVisible)
		{
			SelectedItem.Index = ItemIndex;
			if (bForceAll)
			{
				Drag.OrigSelectedItem = SelectedItem;
				Movement.OrigSelectedItem = SelectedItem;
			}
			return true;
		}
	}
	return false;
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
	local float Velocity, SwipeDelta, FinalScrollDist, CalcScrollDist, SwipeTime;
	local MobileMenuListItem Selected;
	local int Index, Index0;
	local bool bUdpateTouchItem;

	TouchX -= Left;
	TouchY -= Top;

	Drag.TouchTime+= DeltaTime;
	//`log("EventType:" $ String(EventType) @ "Y:" $ TouchY @ "Time:" $ DeltaTime @ Drag.TouchTime);
	if (EventType == Touch_Began)
	{
		Movement.bIsMoving = false;
		Drag.bIsDragging = true;
		Drag.OrigSelectedItem = SelectedItem;
		Drag.StartTouch.X = TouchX;
		Drag.StartTouch.Y = TouchY;
		Drag.ScrollAmount = 0;
		Drag.AbsScrollAmount = 0;
		Drag.bHasSelectedChanged = false;
		Drag.TouchTime = 0;
		Drag.NumUpdates = 0;
		for (Index = 0; Index < NumInDragHistory; Index++)
		{
			Drag.UpdateHistory[Index].TouchTime = 0;
		}
		Drag.TouchedItem = GetItemClickPosition(TouchX, TouchY);
		if (Drag.TouchedItem != none)
		{
			Drag.bIsDragging = !Drag.TouchedItem.OnTouch(EventType, TouchX, TouchY, DeltaTime);
		}
	} 
	else if (!Drag.bIsDragging)
	{
		bUdpateTouchItem = true;
	}
	else if ((EventType == Touch_Ended) || (EventType == Touch_Cancelled))
	{
		bUdpateTouchItem = true;
		Drag.bIsDragging = false;
		Movement.bIsMoving = true;
		Movement.CurrentTime = 0;
		Movement.OrigSelectedItem = SelectedItem;

		if (!Drag.bHasSelectedChanged && (Drag.StartTouch.X == TouchX) && (Drag.StartTouch.Y == TouchY))
		{
			Selected = GetSelected();

			// Fix annoyance issue when you try to swipe, but do so very quick and therefore
			// it appears to only be a touch and it moves to the item you touched.
			if((Drag.TouchTime > 0.05f) && bTapToScrollToItem)
			{
				// Force to scroll to item selected.
				if (bIsVerticalList)
					FinalScrollDist = TouchY - (SelectedOffset + (Selected.Height/2));
				else
					FinalScrollDist = TouchX - (SelectedOffset + (Selected.Width/2));
			}
		}
		else if (Drag.NumUpdates >= 2)
		{
			Index =  (Drag.NumUpdates - 1) % NumInDragHistory;
			Index0 = (Drag.NumUpdates - Min(Drag.NumUpdates, NumInDragHistory)) % NumInDragHistory;
			SwipeDelta = -(Drag.UpdateHistory[Index].TouchCoord - Drag.UpdateHistory[Index0].TouchCoord);
			SwipeTime = Drag.UpdateHistory[Index].TouchTime - Drag.UpdateHistory[Index0].TouchTime;

			// Find the final velocity.
			Velocity = (SwipeTime > 0) ? (SwipeDelta / SwipeTime) : 0.0f;

			// Using acceleration formulas - find how far it should take to stop and how long that will take.
			FinalScrollDist = Square(Velocity) / (2.0 * Deacceleration);

			//`log("Delta:" $ SwipeDelta @ "Vel:" $ Velocity @ "Dist:" $ FinalScrollDist);
		}

		if (bDisableScrolling)
			FinalScrollDist = 0;

		// See how far we will really go since we want to lock in selected position (no adjust)
		if (SwipeDelta < 0)
			CalcScrollDist = CalculateSelectedItem(SelectedItem, -FinalScrollDist, true);
		else 
			CalcScrollDist = CalculateSelectedItem(SelectedItem, FinalScrollDist, true);

		// If we don't have to scroll to selected, then use our original scroll dist.  
		// We still have to call CalculateSelectedItem() because it will tell us if bEndOfList.
		// In that case, allow to auto scroll back to selected item.
		if (!bForceSelectedToLineup && !SelectedItem.bEndOfList)
		{
			if (SwipeDelta < 0)
				CalcScrollDist = -FinalScrollDist;
			else
				CalcScrollDist = FinalScrollDist;
		}

		// Restore...since we were looking into the future.
		SelectedItem = Movement.OrigSelectedItem; 

		// Given this desired distance, and a little algebra on D = (D/T)**2/(2A) -> T = sqrt(D /(2A))
		Movement.TotalTime = Sqrt(Abs(CalcScrollDist) / (2.0 * Deacceleration));
		Movement.FullMovement = CalcScrollDist;

		//`log("FinalDist:" $ CalcScrollDist @ "Time:" $ Movement.TotalTime);
	}
	else
	{
		Drag.UpdateHistory[Drag.NumUpdates % NumInDragHistory].TouchTime = Drag.TouchTime;
		Drag.UpdateHistory[Drag.NumUpdates % NumInDragHistory].TouchCoord = (bIsVerticalList) ? TouchY : TouchX;
		Drag.NumUpdates++;

		if (Drag.OrigSelectedItem.Index != SelectedItem.Index)
		{
			Drag.bHasSelectedChanged = true;
		}
		Drag.ScrollAmount = (bIsVerticalList) ? (Drag.StartTouch.Y - TouchY) : (Drag.StartTouch.X - TouchX);

		Index =  (Drag.NumUpdates - 1) % NumInDragHistory;
		Index0 = (Drag.NumUpdates - Min(Drag.NumUpdates, NumInDragHistory)) % NumInDragHistory;
		SwipeDelta = abs(Drag.UpdateHistory[Index].TouchCoord - Drag.UpdateHistory[Index0].TouchCoord);
		if (bDisableScrolling)
		{
			Drag.ScrollAmount = 0;
			SwipeDelta = 0;
		}

		Drag.AbsScrollAmount += SwipeDelta;
		//`log(Drag.AbsScrollAmount);
	}

	if (bUdpateTouchItem)
	{
		if (Drag.TouchedItem != none)
		{
			// If user has moved off of item, still update it, but indicate that we are no longer over it with -1.
			if (Drag.TouchedItem == GetItemClickPosition(TouchX, TouchY))
			{
				Drag.TouchedItem.OnTouch(EventType, TouchX, TouchY, DeltaTime);
			}
			else
			{
				Drag.TouchedItem.OnTouch(EventType, -1, -1, DeltaTime);
			}
		}
	}
	return true;
}


function MobileMenuListItem GetItemClickPosition(out float MouseX, out float MouseY)
{
	local int ScrollAmount, CurIndex, ScrollSize;
	local MobileMenuListItem Item;
	
	ScrollAmount = (bIsVerticalList) ? MouseY : MouseX;
	ScrollAmount -= SelectedOffset;

	// First attempt to scroll list up/left (if user swiped down/right)
	// ScrollSize needs to be size of SelectedIndex after loop...
	CurIndex = fMax(0, SelectedItem.Index);  // Avoid [] out of bounds.
	if (CurIndex >= Items.Length)
		return none;
	Item = Items[CurIndex];
	ScrollSize = ItemScrollSize(Item);
	while (ScrollAmount < 0)
	{
		if (CurIndex > 0)
			CurIndex--;
		else if (bLoops)
			CurIndex = Items.Length - 1;
		else
			break;
		Item = Items[CurIndex];
		if (Item.bIsVisible)
		{
			ScrollSize = ItemScrollSize(Item);
			ScrollAmount += ScrollSize;
		}
	}

	// Now see if we need to go other way (because user swiped up/left)
	while (ScrollAmount > ScrollSize)
	{
		if (CurIndex < (Items.length - 1)) 
			CurIndex++;
		else if (bLoops)
			CurIndex = 0;
		else
			break;
		Item = Items[CurIndex];
		if (Item.bIsVisible)
		{
			ScrollAmount -= ScrollSize;
			ScrollSize = ItemScrollSize(Item);
		}
	}

	if (bIsVerticalList)
	{
		MouseY = ScrollAmount;
		if (ScrollAmount < 0 || ScrollAmount > Item.Height)
		{
			Item = none;
		}
	}
	else
	{
		MouseX = ScrollAmount;
		if (ScrollAmount < 0 || ScrollAmount > Item.Width)
		{
			Item = none;
		}
	}

	return Item;
}

function float CalculateSelectedItem(out SelectedMenuItem Selected, float ScrollAmount, bool bForceZeroAdjustment)
{
	local float AdjustValue, ScrollSize, Scrolled, HalfScroll;
	local int CurIndex;
	local MobileMenuListItem Item;

	AdjustValue = Selected.Offset;

	// First scroll so that selected item is even.
	Scrolled = AdjustValue;
	ScrollAmount -= AdjustValue;

	// First attempt to scroll list up/left (if user swiped down/right)
	// ScrollSize needs to be size of SelectedIndex after loop...
	CurIndex = fMax(0, Selected.Index);  // Avoid [] out of bounds.
	if (CurIndex >= Items.Length)
		return 0;

	Item = Items[CurIndex];
	ScrollSize = ItemScrollSize(Item);
	Selected.bEndOfList = false;
	while (ScrollAmount < 0)
	{
		if (CurIndex > 0)
		{
			CurIndex--;
		}
		else if (bLoops)
		{
			CurIndex = Items.Length - 1;
		}
		else
		{
			// We are at top item - cause dragging to be less effective.
			ScrollAmount *= EndOfListSupression;
			Selected.bEndOfList = true;
			break;
		}
		Item = Items[CurIndex];
		if (Item.bIsVisible)
		{
			ScrollSize = ItemScrollSize(Item);
			ScrollAmount += ScrollSize;
			Scrolled -= ScrollSize;
			Selected.Index = CurIndex;
		}
	}

	// Now see if we need to go other way (because user swiped up/left or we went too far above)
	HalfScroll = (ScrollSize/2);
	while (ScrollAmount > HalfScroll)
	{
		if (CurIndex < (Items.length - (NumShowEndOfList + 1))) 
		{
			CurIndex++;
		}
		else if (bLoops)
		{
			CurIndex = 0;
		}
		else
		{
			// We are at bottom item - cause dragging to be less effective.
			// Need to take out the half scroll so it does not jump on transition from when
			// this code is not executed, and when it is.
			ScrollAmount -= HalfScroll;
			ScrollAmount *= EndOfListSupression;
			ScrollAmount += HalfScroll;
			Selected.bEndOfList = true;
			break;
		}
		Item = Items[CurIndex];
		if (Item.bIsVisible)
		{
			ScrollAmount -= ScrollSize;
			Scrolled += ScrollSize;
			Selected.Index = CurIndex;
			ScrollSize = ItemScrollSize(Item);
		}
	}

	if (bForceZeroAdjustment)
	{
		Selected.Offset = 0;
	}
	else
	{
		Selected.Offset = -ScrollAmount;
		Scrolled -= ScrollAmount;
	}

	return Scrolled;
}

function UpdateScroll(float DeltaTime)
{
	local float ScrollAmount;

	if (Drag.bIsDragging)
	{
		SelectedItem = Drag.OrigSelectedItem;
		ScrollAmount = Drag.ScrollAmount;
	}
	else if (Movement.bIsMoving)
	{
		SelectedItem = Movement.OrigSelectedItem;
		Movement.CurrentTime += DeltaTime;
		if (Movement.CurrentTime < Movement.TotalTime)
		{
			ScrollAmount = FInterpEaseOut(0, Movement.FullMovement,  Movement.CurrentTime/Movement.TotalTime, EaseOutExp);
			//`log(ScrollAmount $ "=" $ Movement.FullMovement @ "Fraction:" $ Movement.CurrentTime/Movement.TotalTime );
		}
		else
		{
			ScrollAmount = Movement.FullMovement;
			Movement.bIsMoving = false;
		}
	}
	else 
	{
		return;
	}

	CalculateSelectedItem(SelectedItem, ScrollAmount, false);
}

/**
 * Render the widget
 *
 * @param Canvas - the canvas object for drawing
 */
function RenderObject(canvas Canvas, float DeltaTime)
{
	local MobileMenuListItem Item;
	local float OrgX, OrgY; //, ClipX, ClipY;
	local int VpEnd, CurIndex, First, Last, SelectedIdx, NumItems, RealIndex;
	local Vector2D VpPos, VpSize;

	NumItems = Items.Length;
	if (NumItems == 0)
		return;

	UpdateScroll(DeltaTime);

	VpSize.X = Width;
	VpSize.Y = Height;

	// Find top displayed visible item.
	SelectedIdx = fMax(0, SelectedItem.Index); // Avoid [] out of bounds.

	// If we loop, then we add NumItems for 0 compares but always mod to get real index.
	if (bLoops)
		SelectedIdx += NumItems;
	First = SelectedIdx;

	if (bIsVerticalList)
	{
		VpPos.X = Left;
		VpPos.Y = Top + SelectedOffset + SelectedItem.Offset;
		VpEnd = Top + Height;
		while ((First > 0) && (VpPos.Y > Top))
		{
			First--;
			Item = Items[First % NumItems];
			if (Item.bIsVisible)
				VpPos.Y -= Item.Height;
		}
	}
	else
	{
		VpPos.X = Left + SelectedOffset + SelectedItem.Offset;
		VpPos.Y = Top;
		VpEnd = Left + Width;
		while ((First > 0) && (VpPos.X > Left))
		{
			First--;
			Item = Items[First % NumItems];
			if (Item.bIsVisible)
				VpPos.X -= Item.Width;
		}
	}

	// Make sure our First is actually visible.
	while ((First + 1) < NumItems)
	{
		Item = Items[First];
		if (Item.bIsVisible)
			break;
		First++;
	};

	
	// Calculate viewports for everyone
	Last = First;
	for (CurIndex = 0; CurIndex < NumItems; CurIndex++)
	{
		RealIndex = (bLoops) ? ((First + CurIndex) % NumItems) : (First + CurIndex);
		if (RealIndex >= NumItems)
		{
			break;
		}
		Item = Items[RealIndex];
		if (Item.bIsVisible)
		{
			Last = First + CurIndex;
			if (bIsVerticalList)
			{
				VpSize.Y = Item.Height;
				Item.VpPos = VpPos;
				Item.VpSize = VpSize;
				VpPos.Y += VpSize.Y;
				if (VpPos.Y >= VpEnd)
					break;
			}
			else
			{
				VpSize.X = Item.Width;
				Item.VpPos = VpPos;
				Item.VpSize = VpSize;
				VpPos.X += VpSize.X;
				if (VpPos.X >= VpEnd)
					break;
			}	
		}
	}

	OrgX = Canvas.OrgX;
	OrgY = Canvas.OrgY;
	// Does not good :(
	//ClipX = Canvas.ClipX;
	//ClipY = Canvas.ClipY;
	//Canvas.ClipX = Left + Width;
	//Canvas.ClipY = Top + Height;

	// Now render up to (not including) selected, then backwards to and including selected.
	// This is so if we render larger that our VP, the selected on will be top.
	// Normally these loop conditions do not kick it out, it is the check with RealIndex, the
	// conditions are just safety checks (like a small list)
	for (CurIndex = First; CurIndex < SelectedIdx; CurIndex++)
	{
		Item = Items[CurIndex % NumItems];
		if (Item.bIsVisible)
		{
			Canvas.SetOrigin(Item.VpPos.X, Item.VpPos.Y);
			Item.RenderItem(self, Canvas, DeltaTime);
		}
	}
	for (CurIndex = Last; CurIndex >= SelectedIdx; CurIndex--)
	{
		Item = Items[CurIndex % NumItems];
		if (Item.bIsVisible)
		{
			Canvas.SetOrigin(Item.VpPos.X, Item.VpPos.Y);
			Item.RenderItem(self, Canvas, DeltaTime);
		}
	}

	FirstVisible = First;
	LastVisible = Last;

	// Restore to not mess up next scene.
	Canvas.OrgX  = OrgX;
	Canvas.OrgY  = OrgY;
	//Canvas.ClipX = ClipX;
	//Canvas.ClipY = ClipY;
}

/** Amount of space item takes up in scroll direction */
function int ItemScrollSize(MobileMenuListItem Item)
{
	return (bIsVerticalList) ? Item.Height : Item.Width;
}

defaultproperties
{
	NumShowEndOfList=0
	bIsActive=true
	bIsVerticalList=true
	bTapToScrollToItem=true
	Deacceleration = 1500
	EaseOutExp=4.0
	EndOfListSupression=0.4f


	SelectedItem=(Index=0, Offset=0)
	SelectedOffset = 0
	bForceSelectedToLineup=true
}

