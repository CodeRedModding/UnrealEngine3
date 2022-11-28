class UIContainer_Loadout extends UIContainer;

var UIContainer FocusContainer, ColumnA, ColumnB;
var UIElement FocusElement;

/** Current cash label */
var UIElement_Label CashLabel;

/** Primary/Secondary weapon labels */
var UIElement_Label PriLabel, SecLabel;

/** Various info labels */
var UIElement_Label PurchaseLabel, EquipPriLabel, EquipSecLabel;

var color FocusColor, BorderColor, SelectedColor, UnselectedColor;

/** Weapon selection info */
struct WeaponInfo
{
	var UIElement_Icon IconElement;
	var UIElement_Label LabelElement;
	var class<WarWeapon> WeaponClass;
	var int WeaponCost;
	var bool bSelected;
};
var array<WeaponInfo> Weapons;
var int WeaponIdx;

/** Squad member selection info */
struct SquadInfo
{
	var UIElement_Icon IconElement;
	var string MemberName;
	var bool bSelected;
};
var array<SquadInfo> Squad;

function bool InputKey(Name Key, EInputEvent Event, float AmountDepressed)
{
	local WarPC wfPC;
	wfPC = GetPlayer();
	if (Event == IE_Pressed)
	{
		if (Key == 'Left' ||
			Key == 'XboxTypeS_DPad_Left')
		{
			if (FocusContainer == ColumnB)
			{
				SetFocusContainer(ColumnA);
			}
		}
		else
		if (Key == 'Right' ||
			Key == 'XboxTypeS_DPad_Right')
		{
			if (FocusContainer == ColumnA)
			{
				SetFocusContainer(ColumnB);
			}
		}
		else
		if (Key == 'Down' ||
			Key == 'XboxTypeS_DPad_Down')
		{
			SetFocusElement(FocusContainer.GetNextFocus(FocusElement));
		}
		else
		if (Key == 'Up' ||
			Key == 'XboxTypeS_DPad_Up')
		{
			SetFocusElement(FocusContainer.GetPrevFocus(FocusElement));
		}
		else
		if (Key == 'A' ||
			Key == 'XboxTypeS_A')
		{
			BuySelected();
		}
		else
		if (Key == 'X' ||
			Key == 'XboxTypeS_X')
		{
			if (wfPC.Pawn.FindInventoryType(Weapons[WeaponIdx].WeaponClass) != None)
			{
				wfPC.PrimaryWeaponClass = Weapons[WeaponIdx].WeaponClass;
				if (wfPC.PrimaryWeaponClass == wfPC.SecondaryWeaponClass)
				{
					wfPC.SecondaryWeaponClass = None;
					SecLabel.bEnabled = false;
				}
			}
		}
		else
		if (Key == 'Y' ||
			Key == 'XboxTypeS_Y')
		{
			if (wfPC.Pawn.FindInventoryType(Weapons[WeaponIdx].WeaponClass) != None)
			{
				wfPC.SecondaryWeaponClass = Weapons[WeaponIdx].WeaponClass;
				if (wfPC.SecondaryWeaponClass == wfPC.PrimaryWeaponClass)
				{
					wfPC.PrimaryWeaponClass = None;
					PriLabel.bEnabled = false;
				}
			}
		}
		else
		if (Key == 'B' ||
			Key == 'XboxTypeS_B' ||
			Key == 'Escape')
		{
			WarConsole(Console).DeactivateUIContainer(self);
		}
		else
		if (Key == 'Plus' ||
			Key == 'Equals')
		{
			GetPlayer().Cash += 100;
		}
		else
		if (Key == 'Minus' ||
			Key == 'Underscore')
		{
			GetPlayer().Cash -= 100;
		}
		UpdateWeaponList();
		UpdateCashLabel();
	}
	return (Key != 'Enter');
}

function SetFocusContainer(UIContainer inFocusContainer)
{
	if (FocusContainer != None)
	{
		FocusContainer.BorderColor = BorderColor;
		FocusContainer.BorderSize -= 4;
		SetFocusElement();
	}
	FocusContainer = inFocusContainer;
	if (inFocusContainer != None)
	{
		FocusContainer.BorderColor = FocusColor;
		FocusContainer.BorderSize += 4;
		SetFocusElement(FocusContainer.GetNextFocus());
	}
}

function SetFocusElement(optional UIElement inFocusElement)
{
	local int idx;
	local WarPC wfPC;
	wfPC = GetPlayer();
	FocusElement = inFocusElement;
	if (FocusContainer == ColumnA)
	{
		for (idx = 0; idx < Squad.Length; idx++)
		{
			if (Squad[idx].IconElement == FocusElement)
			{
				Squad[idx].IconElement.DrawColor = SelectedColor;
				Squad[idx].bSelected = true;
			}
			else
			{
				Squad[idx].IconElement.DrawColor = UnselectedColor;
				Squad[idx].bSelected = false;
			}
		}
	}
	else
	if (FocusContainer == ColumnB)
	{
		UpdateWeaponList();
	}
}

function UpdateWeaponList()
{
	local int idx;
	local WarPC wfPC;
	if (Console == None)
	{
		return;
	}
	wfPC = GetPlayer();
	// highlight the selected
	PurchaseLabel.DrawColor = MakeColor(45,45,45,255);
	EquipPriLabel.DrawColor = MakeColor(45,45,45,255);
	EquipSecLabel.DrawColor = MakeColor(45,45,45,255);
	for (idx = 0; idx < Weapons.Length; idx++)
	{
		if (Weapons[idx].IconElement == FocusElement)
		{
			if (wfPC.Pawn.FindInventoryType(Weapons[idx].WeaponClass) != None)
			{
				Weapons[idx].IconElement.DrawColor = SelectedColor;
				if (Weapons[idx].WeaponClass != wfPC.PrimaryWeaponClass)
				{
					EquipPriLabel.DrawColor = MakeColor(255,255,255,255);
				}
				if (Weapons[idx].WeaponClass != wfPC.SecondaryWeaponClass)
				{
					EquipSecLabel.DrawColor = MakeColor(255,255,255,255);
				}
			}
			else
			if (wfPC.Cash >= Weapons[idx].WeaponCost)
			{
				PurchaseLabel.DrawColor = MakeColor(255,255,255,255);
				Weapons[idx].IconElement.DrawColor = SelectedColor;
			}
			else
			{
				Weapons[idx].IconElement.DrawColor = MakeColor(255,0,0,255);
			}
			Weapons[idx].bSelected = true;
			WeaponIdx = idx;
		}
		else
		{
			Weapons[idx].IconElement.DrawColor = UnselectedColor;
			Weapons[idx].bSelected = false;
		}
		// anchor the primary/secondary labels
		if (wfPC.PrimaryWeaponClass == Weapons[idx].WeaponClass)
		{
			PriLabel.bEnabled = true;
			PriLabel.ParentElement = Weapons[idx].IconElement;
		}
		else
		if (wfPC.SecondaryWeaponClass == Weapons[idx].WeaponClass)
		{
			SecLabel.bEnabled = true;
			SecLabel.ParentElement = Weapons[idx].IconElement;
		}
		if (wfPC.Pawn.FindInventoryType(Weapons[idx].WeaponClass) != None)
		{
			Weapons[idx].LabelElement.DrawLabel = Weapons[idx].WeaponClass.default.ItemName;
		}
		else
		{
			Weapons[idx].LabelElement.DrawLabel = "$"$Weapons[idx].WeaponCost@"-"@Weapons[idx].WeaponClass.default.ItemName;
		}
	}
}

function BuySelected()
{
	local WarPC wfPC;
	local int idx;
	wfPC = GetPlayer();
	for (idx = 0; idx < Weapons.Length; idx++)
	{
		if (Weapons[idx].bSelected)
		{
			if (wfPC.Pawn.FindInventoryType(Weapons[idx].WeaponClass) == None &&
				Weapons[idx].WeaponCost <= wfPC.Cash)
			{
				wfPC.AddCash(-Weapons[idx].WeaponCost);
				wfPC.Pawn.CreateInventory(Weapons[idx].WeaponClass);
				UpdateCashLabel();
				break;
			}
		}
	}
	UpdateWeaponList();
}

function WarPC GetPlayer()
{
	return WarPC(Console.Player);
}

function Activated()
{
	local WarPC wfPC;
	local int idx;
	wfPC = GetPlayer();
	wfPC.myHUD.bShowHUD = false;
	wfPC.bMovementInputEnabled = false;
	// load main interface container
	Elements[0] = UIContainer(DynamicLoadObject("WarInterface.Loadout",class'UIContainer'));
	PurchaseLabel = UIElement_Label(FindElement('Purchase_Label'));
	EquipPriLabel = UIElement_Label(FindElement('EquipPri_Label'));
	EquipSecLabel = UIElement_Label(FindElement('EquipSec_Label'));
	// grab column elements
	ColumnA = UIContainer(FindElement('Loadout_Column_A'));
	// build the list of squad elements
	for (idx = 0; idx < Squad.Length; idx++)
	{
		Squad[idx].IconElement = UIElement_Icon(ColumnA.FindElement(Name("Squad_Icon_"$idx+1)));
	}
	ColumnB = UIContainer(FindElement('Loadout_Column_B'));
	// build the list of weapon elements
	for (idx = 0; idx < Weapons.Length; idx++)
	{
		Weapons[idx].IconElement = UIElement_Icon(ColumnB.FindElement(Name("Weap_Icon_"$idx+1)));
		Weapons[idx].LabelElement = UIElement_Label(ColumnB.FindElement(Name("Weap_Label_"$idx+1)));
		if (wfPC.Pawn.FindInventoryType(Weapons[idx].WeaponClass) != None)
		{
			Weapons[idx].LabelElement.DrawLabel = Weapons[idx].WeaponClass.default.ItemName;
		}
		else
		{
			Weapons[idx].LabelElement.DrawLabel = "$"$Weapons[idx].WeaponCost@"-"@Weapons[idx].WeaponClass.default.ItemName;
		}
	}
	CashLabel = UIElement_Label(ColumnB.FindElement('Cash_Label'));
	PriLabel = UIElement_Label(ColumnB.FindElement('Primary_Label'));
	SecLabel = UIElement_Label(ColumnB.FindElement('Secondary_Label'));
	UpdateCashLabel();
	// set initial focus
	SetFocusContainer(ColumnA);
	SetFocusContainer(ColumnB);
}

function UpdateCashLabel()
{
	CashLabel.DrawLabel = "COMBAT CASH: $"$GetPlayer().Cash;
}

function Deactivated()
{
	local WarPC wfPC;
	wfPC = GetPlayer();
	wfPC.myHUD.bShowHUD = true;
	wfPC.bMovementInputEnabled = true;
}

defaultproperties
{
	bHandleInput=true
	FocusColor=(G=255,A=255)
	BorderColor=(A=255)
	SelectedColor=(R=255,G=255,B=255,A=255)
	UnselectedColor=(R=128,G=128,B=128,A=128)
	Weapons(0)=(WeaponClass=class'Weap_AssaultRifle',WeaponCost=150)
	Weapons(1)=(WeaponClass=class'Weap_MX8SnubPistol',WeaponCost=75)
	Weapons(2)=(WeaponClass=class'Weap_ScorchRifle',WeaponCost=250)
	Weapons(3)=(WeaponClass=class'Weap_SledgeCannon',WeaponCost=450)
	Weapons(4)=(WeaponClass=class'Weap_RPG',WeaponCost=675)

	Squad(0)=(MemberName="Marcus")
	Squad(1)=(MemberName="Dom")
	Squad(2)=(MemberName="Frankie")
	Squad(3)=(MemberName="Bob")
}
