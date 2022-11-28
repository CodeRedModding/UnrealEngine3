class UIContainer extends UIElement
	native;

cpptext
{
	virtual void DrawElement(UCanvas *inCanvas);
	UUIElement* FindElement(FName searchName);

	// for generic browser
	void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	INT GetThumbnailLabels( TArray<FString>* InLabels );
};

/** List of all elements for this container */
var() export editinline array<UIElement> Elements;

/** Does this container handle input? */
var() bool bHandleInput;

/** Transient copy of the Console set when active */
var transient Console Console;

/**
 * Handles drawing the contents of this container.
 */
native function Draw(Canvas Canvas);

/**
 * Searches for a named element in all nested containers.
 */
native function UIElement FindElement(Name searchName);

/**
 * Called when this container becomes the active one.
 */
function Activated();

/**
 * Called when this container is no longer active.
 */
function Deactivated();

/**
 * Called upon input if bHandleInput is true.
 * 
 * @return	true to indicate the input was handled
 */
function bool InputKey(Name Key, EInputEvent Event, float AmountDepressed)
{
	local bool bHandled;
	local int idx;
	local UIContainer container;
	for (idx = 0; idx < Elements.Length && !bHandled; idx++)
	{
		container = UIContainer(Elements[idx]);
		if (container != None &&
			container.bHandleInput)
		{
			bHandled = UIContainer(Elements[idx]).InputKey(Key,Event,AmountDepressed);
		}
	}
	return bHandled;
}

function UIElement GetNextFocus(optional UIElement inFocus)
{
	local int idx;
	local int currentId;
	local UIElement bestElement;
	if (inFocus != None)
	{
		currentId = inFocus.FocusId;
	}
	else
	{
		currentId = -1;
	}
	for (idx = 0; idx < Elements.Length; idx++)
	{
		if (Elements[idx].bAcceptsFocus &&
			Elements[idx].FocusId > currentId)
		{
			if (bestElement == None ||
				Elements[idx].FocusId < bestElement.FocusId)
			{
				bestElement = Elements[idx];
			}
		}
	}
	if (bestElement == None)
	{
		bestElement = inFocus;
	}
	return bestElement;
}

function UIElement GetPrevFocus(optional UIElement inFocus)
{
	local int idx;
	local int currentId;
	local UIElement bestElement;
	if (inFocus != None)
	{
		currentId = inFocus.FocusId;
	}
	else
	{
		currentId = 999;
	}
	for (idx = 0; idx < Elements.Length; idx++)
	{
		if (Elements[idx].bAcceptsFocus &&
			Elements[idx].FocusId < currentId)
		{
			if (bestElement == None ||
				Elements[idx].FocusId > bestElement.FocusId)
			{
				bestElement = Elements[idx];
			}
		}
	}
	if (bestElement == None)
	{
		bestElement = inFocus;
	}
	return bestElement;
}

defaultproperties
{
	bHandleInput = false;
	DrawW=640
	DrawH=480
	bSizeDrag=true
}
