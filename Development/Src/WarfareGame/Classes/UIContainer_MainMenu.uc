class UIContainer_MainMenu extends UIContainer;

struct MenuEntry
{
	var UIElement_Label Label;
	var Name ElementName;
	var string Command;
};
var array<MenuEntry> Entries;

var UIContainer FocusContainer;
var UIElement Focus;

function bool InputKey(Name Key, EInputEvent Event, float AmountDepressed)
{
	if (Event == IE_Pressed)
	{
		if (Key == 'Down' ||
			Key == 'XboxTypeS_DPad_Down')
		{
			SelectEntry(FocusContainer.GetNextFocus(Focus));
		}
		else
		if (Key == 'Up' ||
			Key == 'XboxTypeS_DPad_Up')
		{
			SelectEntry(FocusContainer.GetPrevFocus(Focus));
		}
		else
		if (Key == 'A' ||
			Key == 'Enter' ||
			Key == 'XboxTypeS_A' ||
			Key == 'XboxTypeS_Start')
		{
			ActivateEntry();
		}
	}
	return (Key != 'Enter');
}

function SelectEntry(UIElement newFocus)
{
	if (Focus != None)
	{
		Focus.bDrawBorder = false;
	}
	Focus = newFocus;
	if (Focus != None)
	{
		Focus.bDrawBorder = true;
	}
}

function ActivateEntry()
{
	local int idx;
	local WarPC wfPC;
	wfPC = GetPlayer();
	for (idx = 0; idx < Entries.Length; idx++)
	{
		if (Entries[idx].Label == Focus)
		{
			wfPC.ConsoleCommand(Entries[idx].Command);
			return;
		}
	}
}

function Activated()
{
	local WarPC wfPC;
	local int idx;
	wfPC = GetPlayer();
	wfPC.myHUD.bShowHUD = false;
	wfPC.bMovementInputEnabled = false;
	FocusContainer = UIContainer(DynamicLoadObject("WarInterface.MainMenu",class'UIContainer'));
	Elements[0] = FocusContainer;
	for (idx = 0; idx < Entries.Length; idx++)
	{
		Entries[idx].Label = UIElement_Label(FindElement(Entries[idx].ElementName));
	}
	SelectEntry(FocusContainer.GetNextFocus());
}

function Deactivated()
{
	local WarPC wfPC;
	wfPC = GetPlayer();
	wfPC.myHUD.bShowHUD = true;
	wfPC.bMovementInputEnabled = true;
}

function WarPC GetPlayer()
{
	return WarPC(Console.Player);
}

defaultproperties
{
	bHandleInput=true
	Entries(0)=(ElementName=AdamsHouse,Command="OPEN SP_AdamsHouse1")
	Entries(1)=(ElementName=EBA,Command="OPEN SP_EBA_G")
	Entries(2)=(ElementName=HOS,Command="OPEN SP_HOS")
	Entries(3)=(ElementName=LedgeTest,Command="OPEN LedgeTest")
	Entries(4)=(ElementName=WretchTest,Command="OPEN WretchTest")
	Entries(5)=(ElementName=Exit,Command="EXIT")
}
