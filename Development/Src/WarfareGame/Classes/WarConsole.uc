class WarConsole extends Console;

/** Active UI container that should be rendered and given input */
var transient UIContainer ActiveUIContainer;

event PostRender(Canvas Canvas)
{
	if (ActiveUIContainer != None)
	{
		ActiveUIContainer.Draw(Canvas);
	}
	Super.PostRender(Canvas);
}

event bool InputKey( name Key, EInputEvent Event, float AmountDepressed )
{
	local bool bHandled;
	if (ActiveUIContainer != None &&
		ActiveUIContainer.bHandleInput)
	{
		bHandled = ActiveUIContainer.InputKey(Key, Event, AmountDepressed);
	}
	if (!bHandled)
	{
		bHandled = Super.InputKey(Key,Event,AmountDepressed);
	}
	return bHandled;
}

/**
 * 
 */
function ActivateUIContainer(UIContainer inContainer)
{
	// deactivate the current container if applicable
	if (ActiveUIContainer != None)
	{
		DeactivateUIContainer(ActiveUIContainer);
	}
	// activate the new one
	if (inContainer != None)
	{
		inContainer.Console = self;
		inContainer.Activated();
		ActiveUIContainer = inContainer;
	}
}

/**
 * 
 */
function DeactivateUIContainer(UIContainer inContainer)
{
	if (inContainer != None)
	{
		inContainer.Deactivated();
		inContainer.Console = None;
		if (inContainer == ActiveUIContainer)
		{
			ActiveUIContainer = None;
		}
	}
}

/*
exec function Type()
{
	if (ActiveUIContainer == None ||
		!ActiveUIContainer.bHandleInput)
	{
		Super.Type();
	}
}
*/
