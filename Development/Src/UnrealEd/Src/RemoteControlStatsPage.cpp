/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "RemoteControlStatsPage.h"
#include "RemoteControlGame.h"

/** Enum indicating which type of object is being wrapped */
enum EStatTreeItemType
{
	STIT_Group,
	STIT_Stat
};

#if STATS

/** Wrapper object around the tree item's stats data */
class WxStatsTreeItem :
	public wxTreeItemData
{
	/** Hide this ctor*/
	WxStatsTreeItem() :
		ItemType(STIT_Group),
		Group(NULL),
		Stat(NULL)
	{
	}

	/** Indicates the type of item being held */
	const EStatTreeItemType ItemType;
	/** Pointer to the group if it's this type */
	FStatGroup* Group;
	/** Pointer to the stat if it's this type */
	FStatCommonData* Stat;

public:
	/**
	 * Sets the wrapper values
	 *
	 * @param InItemType the type of item being held
	 * @param InGroup the group pointer if this is a group wrapper
	 * @param InStat the stat pointer if this is a stat wrapper
	 */
	WxStatsTreeItem(EStatTreeItemType InItemType,FStatGroup* InGroup,
		FStatCommonData* InStat) :
		ItemType(InItemType),
		Group(InGroup),
		Stat(InStat)
	{
		check((ItemType == STIT_Group && Group!= NULL) ||
			(ItemType == STIT_Stat && Stat != NULL));
	}

	/** Does nothing as we allocate no state */
	virtual ~WxStatsTreeItem(void)
	{
	}

	/** Returns the text for this item */
	const TCHAR* GetText(void) const
	{
		if (ItemType == STIT_Group)
		{
			return Group->Desc;
		}
		else
		{
			return Stat->CounterName;
		}
	}

	/** Determines if the item is checked or not */
	UBOOL IsChecked(void) const
	{
		if (ItemType == STIT_Group)
		{
			return Group->bShowGroup;
		}
		else
		{
			return Stat->bShowStat;
		}
	}

	/**
	 * Toggles the checked status for the item that is wrapped
	 *
	 * @return the new checked state
	 */
	UBOOL ToggleCheck(void)
	{
		if (ItemType == STIT_Group)
		{
			Group->bShowGroup ^= TRUE;
			// Notify the stat manager of the change
			if (Group->bShowGroup == TRUE)
			{
				GStatManager.IncrementNumRendered();
			}
			else
			{
				GStatManager.DecrementNumRendered();
			}
			return Group->bShowGroup;
		}
		else
		{
			Stat->bShowStat ^= TRUE;
			return Stat->bShowStat;
		}
	}
};

#endif

/**
 * Class that handles displaying stats in the tree view
 */
class WxStatTreeCtrl :
	public wxTreeCtrl
{
	/** The list of images to use */
	wxImageList ImageList;

	/**
	 * Handles the user clicking on the tree item. If it's within the check
	 * box, it changes the state of the object and updates the UI
	 *
	 * @param In the event that needs processing
	 */
	void OnMouseEvent(wxMouseEvent &In)
	{
		if (In.LeftDown())
		{
#if STATS
			INT HitFlags = 0;
			// Figure out what was hit
			wxTreeItemId ItemId = HitTest(In.GetPosition(), HitFlags);
			// If the user has hit the check box
			if (HitFlags & wxTREE_HITTEST_ONITEMICON)
			{
				// Get the item for the specified id
				WxStatsTreeItem* Item = (WxStatsTreeItem*)GetItemData(ItemId);
				if (Item != NULL)
				{
					// Toggle checked state
					UBOOL bIsChecked = Item->ToggleCheck();
					// Update image list setting
					SetItemImage(ItemId,(INT)bIsChecked);
				}
				else
				{
					In.Skip();
				}
			}
			else
#endif
			{
				In.Skip();
			}
		}
		else
		{
			In.Skip();
		}
	}

	/** Builds the tree data from the stats data */
	void InitTree(void)
	{
		DeleteAllItems();

		// Create a root node for the groups to be children of
		wxTreeItemId GroupsId = AddRoot(TEXT("Groups"),-1);

#if STATS
		// Iterate through all groups
		for (FStatManager::FStatGroupIterator GroupIt = GStatManager.GetGroupIterator(); GroupIt; ++GroupIt)
		{
			WxStatsTreeItem* GroupItem = new WxStatsTreeItem(STIT_Group,*GroupIt,NULL);
			// Create a new tree item for this group
			wxTreeItemId GroupId = AppendItem(GroupsId,GroupItem->GetText(),
				(INT)GroupItem->IsChecked());
			// Associate the stats tree item data with the id
			SetItemData(GroupId,GroupItem);
			// Add each stat in the group
			for (FStatGroup::FStatIterator StatIt(*GroupIt); StatIt; ++StatIt)
			{
				// Create the wrapper
				WxStatsTreeItem* StatItem = new WxStatsTreeItem(STIT_Stat,NULL,*StatIt);
				// Now append the item to the group
				wxTreeItemId StatId = AppendItem(GroupId,StatItem->GetText(),
					(INT)StatItem->IsChecked());
				// Associate the data with the item
				SetItemData(StatId,StatItem);
			}
		}
#endif
		// Expand the root so all groups are shown
		Expand(GroupsId);
	}

	DECLARE_EVENT_TABLE();

	/**
	 * Recursively updates the children of the node passed in
	 */
	void UpdateCheckedState(const wxTreeItemId& Id)
	{
#if STATS
		WxStatsTreeItem* Item = (WxStatsTreeItem*)GetItemData(Id);
		if (Item != NULL)
		{
			SetItemImage(Id,(INT)Item->IsChecked());
		}
		wxTreeItemIdValue Cookie;
		// Tell each child to update
		for (wxTreeItemId ChildId = GetFirstChild(Id,Cookie); ChildId.IsOk();
			ChildId = GetNextChild(Id,Cookie))
		{
			UpdateCheckedState(ChildId);
		}
#endif // STATS
	}

public:
	/** Constructs the tree control and builds the image list */
	WxStatTreeCtrl(wxWindow *InParent,wxWindowID InID,
        const wxPoint& InPos = wxDefaultPosition,
		const wxSize& InSize = wxDefaultSize,long InStyle = wxTR_HAS_BUTTONS,
		const wxValidator& InValidator = wxDefaultValidator,
		const wxString& InName = TEXT("WxStatTreeCtrl")) :
		wxTreeCtrl(InParent, InID, InPos, InSize, InStyle, InValidator, InName),
		ImageList(16,16,TRUE)
	{
		ImageList.Add(WxBitmap(TEXT("CheckBox_Off")), wxColor(192,192,192));
		ImageList.Add(WxBitmap(TEXT("CheckBox_On")), wxColor(192,192,192));

		SetImageList(&ImageList);

		InitTree();
	}

	/** For each item in the tree it refreshes the checked state */
	void UpdateCheckedState(void)
	{
		UpdateCheckedState(GetRootItem());
	}
};

BEGIN_EVENT_TABLE(WxStatTreeCtrl, wxTreeCtrl)
	EVT_LEFT_DOWN(WxStatTreeCtrl::OnMouseEvent)
END_EVENT_TABLE()

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxRemoteControlStatsPage
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(WxRemoteControlStatsPage, WxRemoteControlPage)
END_EVENT_TABLE()

WxRemoteControlStatsPage::WxRemoteControlStatsPage(FRemoteControlGame *InGame, wxNotebook *InNotebook)
	:	WxRemoteControlPage(InGame)
{
	Create(InNotebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL|wxNO_BORDER|wxCLIP_CHILDREN);

	wxBoxSizer *NewMainSizer = new wxBoxSizer(wxVERTICAL);

	StatTreeCtrl = new WxStatTreeCtrl(this,ID_REMOTECONTROL_STATSPAGE_LIST);
	NewMainSizer->Add(StatTreeCtrl, 1, wxALL|wxEXPAND);

	SetSizer(NewMainSizer);
	NewMainSizer->Fit(this);
	NewMainSizer->SetSizeHints(this);
}

/**
 * Return's the page's title, displayed on the notebook tab.
 */
const TCHAR *WxRemoteControlStatsPage::GetPageTitle() const
{
	return TEXT("Stats");
}

/**
 * Refreshes page contents.
 */
void WxRemoteControlStatsPage::RefreshPage(UBOOL bForce)
{
	StatTreeCtrl->Freeze();
	StatTreeCtrl->UpdateCheckedState();
	StatTreeCtrl->Thaw();
}
