/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "RemoteControlActorsPage.h"
#include "RemoteControlGame.h"

#include <stack>

namespace
{
	/** Tree icons. */
	enum
	{
		TREE_ICON_FOLDER_CLOSED=0,
		TREE_ICON_FOLDER_OPEN=1,
		TREE_ICON_PAWN=2,
		TREE_ICON_DEBUG_OBJECT=3,
		TREE_ICON_SUBOBJECT=4,
		TREE_ICON_DEBUG_SUBOBJECT=5
	};

	/**
	 * Actor tree data class for our actor tree control.
	 */
	class ActorTreeItemData : public wxTreeItemData
	{
	public:
		ActorTreeItemData (const wxString &ClassName, const wxString &ActorName)
			: mClassName(ClassName)
			, mActorName(ActorName)
		{
		}

		const wxString &GetClassName() const
		{
			return mClassName;
		}
		const wxString &GetActorName() const
		{
			return mActorName;
		}

	private:
		wxString mClassName;
		wxString mActorName;
	};

	/**
	 * Actor tree data class for our debug tree control.
	 */
	class DebugTreeItemData : public wxTreeItemData
	{
	public:
		DebugTreeItemData (const wxString &ActorName, const wxString &OptionName)
			: mActorName(ActorName),
			  mOptionName(OptionName)
		{
		}

		const wxString &GetActorName() const
		{
			return mActorName;
		}
		const wxString &GetOptionName() const
		{
			return mOptionName;
		}

	private:
		wxString mActorName;
		wxString mOptionName;
	};

	/**
	 * Override base wxTreeCtrl behavior to make sorting case-insensitive.
	 */
	class ActorTreeCtrl : public wxTreeCtrl
	{
	public:
		ActorTreeCtrl(wxWindow *parent
						, wxWindowID id
						, const wxPoint &pos = wxDefaultPosition
						, const wxSize &size = wxDefaultSize
						, long style = wxTR_HAS_BUTTONS
						, const wxValidator &validator = wxDefaultValidator
						, const wxString &name = TEXT("treeCtrl"))
						: wxTreeCtrl(parent, id, pos, size, style, validator, name)
		{
		}

		virtual int OnCompareItems(const wxTreeItemId &item1, const wxTreeItemId &item2)
		{
			wxString text1 = GetItemText(item1);
			wxString text2 = GetItemText(item2);

			return text1.CmpNoCase(text2);
		}
	private:
	};
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxRemoteControlActorsPage
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(WxRemoteControlActorsPage, WxRemoteControlPage)
	EVT_TOOL(ID_REMOTECONTROL_ACTORSPAGE_DYNAMIC_TOGGLE, WxRemoteControlActorsPage::OnToggleDynamicActors)
	EVT_TOOL(ID_REMOTECONTROL_ACTORSPAGE_DEBUG_SUBOBJECTS_TOGGLE, WxRemoteControlActorsPage::OnToggleDebugSubobjects)
	EVT_TOOL(ID_REMOTECONTROL_ACTORSPAGE_AUTOEXPAND_TREE, WxRemoteControlActorsPage::OnToggleDebugSubobjects)
	EVT_TOOL(ID_REMOTECONTROL_ACTORSPAGE_EDIT_ACTOR, WxRemoteControlActorsPage::OnEditActor)
	EVT_TOOL(ID_REMOTECONTROL_ACTORSPAGE_EDIT_ACTOR_TRACE, WxRemoteControlActorsPage::OnEditActorTrace)
	EVT_TREE_SEL_CHANGED(ID_REMOTECONTROL_ACTORSPAGE_TREE, WxRemoteControlActorsPage::OnTreeSelChanged)
	EVT_TREE_ITEM_ACTIVATED(ID_REMOTECONTROL_ACTORSPAGE_TREE, WxRemoteControlActorsPage::OnTreeDoubleClick)
END_EVENT_TABLE()

WxRemoteControlActorsPage::WxRemoteControlActorsPage(FRemoteControlGame *InGame, wxNotebook *pNotebook)
	:	WxRemoteControlPage(InGame)
	,	bDynamicActors(TRUE)
	,	bDebugSubobjects(FALSE)
	,	bAutoExpandActorTree(FALSE)
	,	bBuildTreeFlag(FALSE)
{
	Create(pNotebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL|wxNO_BORDER|wxCLIP_CHILDREN);

	wxBoxSizer *pMainSizer = new wxBoxSizer(wxVERTICAL);

	ToolBar = new WxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_FLAT | wxTB_HORIZONTAL);
	
	ToolBar->SetToolBitmapSize(wxSize(16,16));

	// Temporary variable for loading bitmaps.
	WxBitmap TempBitmap;

	// "Show Dynamic Actors" toggle.
	LoadMaskedBitmap( TEXT("RCDynamicActorsToggle"), TempBitmap );
	ToolBar->AddCheckTool(ID_REMOTECONTROL_ACTORSPAGE_DYNAMIC_TOGGLE
						  , TEXT("Dynamic actors")
						  , TempBitmap
						  , wxNullBitmap
						  , TEXT("Display of dynamic actors only"));
	ToolBar->AddSeparator();

	// Refresh button.
	LoadMaskedBitmap( TEXT("RCRefresh"), TempBitmap );
	ToolBar->AddTool(ID_REMOTECONTROL_REFRESH_PAGE
					 , TempBitmap
					 , TEXT("Refresh the actor list"));
	ToolBar->AddSeparator();

	// Properties button.
	LoadMaskedBitmap( TEXT("RCEditActor"), TempBitmap );
	ToolBar->AddTool(ID_REMOTECONTROL_ACTORSPAGE_EDIT_ACTOR
					 , TempBitmap
					 , TEXT("Show the selected actor's properties"));
	ToolBar->AddSeparator();

	// "Show Debuggable Sub-object" button.
	LoadMaskedBitmap( TEXT("RCDebugObject"), TempBitmap );
	ToolBar->AddCheckTool(ID_REMOTECONTROL_ACTORSPAGE_DEBUG_SUBOBJECTS_TOGGLE,
							TEXT("Show debuggable sub-objects"),
					        TempBitmap,
							wxNullBitmap,
					        TEXT("Show sub-objects with debug options"));
	ToolBar->AddSeparator();

	// "Auto-expand actor tree" button.
	LoadMaskedBitmap( TEXT("RCAutoExpandActors"), TempBitmap );
	ToolBar->AddCheckTool(ID_REMOTECONTROL_ACTORSPAGE_AUTOEXPAND_TREE,
							TEXT("Show debuggable sub-objects"),
							TempBitmap,
							wxNullBitmap,
							TEXT("Auto-expand actor tree"));
	ToolBar->AddSeparator();

	// "Editactor trace" button.
	LoadMaskedBitmap( TEXT("RCEditActorTrace"), TempBitmap );
	ToolBar->AddTool(ID_REMOTECONTROL_ACTORSPAGE_EDIT_ACTOR_TRACE
					, TempBitmap
					, TEXT("Show properties for the actor under the crosshairs"));

	// Finalize the toolbar.
	ToolBar->Realize();

	ToolBar->ToggleTool( ID_REMOTECONTROL_ACTORSPAGE_DYNAMIC_TOGGLE, bDynamicActors == TRUE );
	ToolBar->ToggleTool( ID_REMOTECONTROL_ACTORSPAGE_DEBUG_SUBOBJECTS_TOGGLE, bDebugSubobjects == TRUE );
	ToolBar->ToggleTool( ID_REMOTECONTROL_ACTORSPAGE_AUTOEXPAND_TREE, bAutoExpandActorTree == TRUE );

	// Add the toolbar to the sizer.
	pMainSizer->Add(ToolBar, 0, wxALL|wxEXPAND|wxALIGN_LEFT);
	pMainSizer->AddSpacer(2);

	// if you change this order, be sure to change the enum above
	TreeImages = new wxImageList(16,16);
	// TREE_ICON_FOLDER_CLOSED
	TreeImages->Add(WxBitmap("FolderClosed"), wxColor(192, 192, 192));
	// TREE_ICON_FOLDER_OPEN
	TreeImages->Add(WxBitmap("FolderOpen"), wxColor(192, 192, 192));
	// TREE_ICON_PAWN
	TreeImages->Add(WxBitmap("RCPawn"), wxColor(0, 128, 128));
	// TREE_ICON_DEBUG_OBJECT
	TreeImages->Add(WxBitmap("RCDebugObject"), wxColor(0, 128, 128));
	// TREE_ICON_SUBOBJECT
	TreeImages->Add(WxBitmap("RCSubObject"), wxColor(0, 128, 128));
	// TREE_ICON_DEBUG_SUBOBJECT
	TreeImages->Add(WxBitmap("RCDebugSubObject"), wxColor(0, 128, 128));

	wxPoint p = wxDefaultPosition;
	TreeCtrl = new wxTreeCtrl(this
							  , ID_REMOTECONTROL_ACTORSPAGE_TREE
							  , p
							  , wxDefaultSize
							  , wxTR_HAS_BUTTONS|wxTR_FULL_ROW_HIGHLIGHT|wxTR_LINES_AT_ROOT|wxTR_HIDE_ROOT );
	TreeCtrl->AssignImageList(TreeImages);

	pMainSizer->Add(TreeCtrl, 1, wxALL|wxEXPAND);

	SetSizer(pMainSizer);
	pMainSizer->Fit(this);
	pMainSizer->SetSizeHints(this);
	UpdateToolUI();

	// Build the actor tree.
	BuildActorTree();
}

/**
 * Refreshes the page.
 */
void WxRemoteControlActorsPage::RefreshPage(UBOOL bForce)
{
	if (bBuildTreeFlag || bForce)
	{
		BuildActorTree();
	}
}

/**
 * Builds the actor tree based on the current settings.
 */
void WxRemoteControlActorsPage::BuildActorTree()
{
	// Set cursor to hourglass
	wxSetCursor(*wxHOURGLASS_CURSOR);

	StringItemMap ClassItemMap;
	StringItemMap ActorItemMap;
	StringSubObjectItemMap SubObjectItemMap;
	SubObjectItemMap.Empty();

	INT actorsAdded = 0;
	INT actorsOld=0;
	UBOOL bForceByClass = FALSE;

	// Remember the selection.
	FString SelectedItemName;
	UBOOL bHadSelectedItem = FALSE;
	{
		const wxTreeItemId SelectedItem( TreeCtrl->GetSelection() );
		if( SelectedItem.IsOk() )
		{
			SelectedItemName = TreeCtrl->GetItemText(SelectedItem).GetData();
			bHadSelectedItem = TRUE;
		}
	}
	TreeCtrl->Freeze();

	TreeCtrl->DeleteAllItems();

	wxTreeItemId rootId = TreeCtrl->AddRoot(TEXT("Actors"));
	
	TArray<FRemoteControlGame::ActorDescription> Actors;

	GetGame()->GetActorList(Actors, bDynamicActors);

	while(actorsAdded < Actors.Num())
	{
		for(INT i = 0; i < Actors.Num(); ++i)
		{
			FRemoteControlGame::ActorDescription &ad = Actors(i);

			if(NULL == ActorItemMap.Find(ad.ActorName))
			{
				wxTreeItemId parentItemId;
				// handle an actor with an owner
				if(ad.OwnerName != TEXT(""))
				{
					// owner
					// see if owner has already been added
					wxTreeItemId *pFindId = ActorItemMap.Find(ad.OwnerName);
					if(NULL == pFindId)
					{
						// nope, if we're forcing adding by class
						if(bForceByClass)
						{
							parentItemId = AddClassItem(ClassItemMap, ad.ClassName);
						}
						else
						{
							// try it the other time around
							continue;
						}
					}
					else
					{
						// yes, it has been found
						parentItemId = *pFindId;
					}
				}
				else
				{
					parentItemId = AddClassItem(ClassItemMap, ad.ClassName);
				}

				FString ActorBaseName = FFilename(ad.ActorName).GetExtension();
				wxTreeItemId item = TreeCtrl->AppendItem(parentItemId, *ActorBaseName, TREE_ICON_PAWN, TREE_ICON_PAWN);
				actorsAdded++;
				ActorItemMap.Set(*ad.ActorName, item);
				TreeCtrl->SetItemData(item, new ActorTreeItemData(*ad.ClassName, *ad.ActorName));

				if (bDebugSubobjects)
				{
					BuildSubObjectTree(item, ad.ActorName, SubObjectItemMap);
				}
			}
		}
		if(actorsAdded == actorsOld)
		{
			// Fix infinite loop when we've got actors with the same name
			if(!bForceByClass)
			{
				// hmm, we're in an infinite loop. just force adding things by class
				bForceByClass = TRUE;
			}
			else
			{
				// ok, we already tried forcing things by class, this means we probably have actors with the same name.
				// This can happen in the multilevel case (wonderful), so just bail
				break;
			}
		}
		actorsOld = actorsAdded;
	}

	// Recursively sort and expand everything.
	TArray<wxTreeItemId> ItemStack;
	ItemStack.AddItem( rootId );

	while( ItemStack.Num() > 0 )
	{
		const INT StackTopIndex = ItemStack.Num() - 1;
		const wxTreeItemId curId = ItemStack( StackTopIndex );
		ItemStack.Remove( StackTopIndex ); // pop the stack.

		TreeCtrl->SortChildren(curId);

		// Trying to expand the root will cause an assertion.
		if( bAutoExpandActorTree && curId != rootId )
		{
			TreeCtrl->Expand(curId);
		}

		wxTreeItemIdValue cookie;
		// Push children onto the stack.
		for(wxTreeItemId child = TreeCtrl->GetFirstChild(curId, cookie);
			child.IsOk();
			child = TreeCtrl->GetNextChild(curId, cookie))
		{
			ItemStack.AddItem( child );
		}
	}

	// Re-apply selection.
	if( bHadSelectedItem )//&& SelectedItemName != TEXT("") )
	{
		// First try actors.
		wxTreeItemId *FindTreeId;
		FindTreeId = ActorItemMap.Find(*SelectedItemName);
		if( !FindTreeId )
		{
			// Now classes.
			FindTreeId = ClassItemMap.Find(*SelectedItemName);
		}
		if( FindTreeId )
		{
			TreeCtrl->SelectItem(*FindTreeId);
			TreeCtrl->EnsureVisible(*FindTreeId);
		}
	}
	TreeCtrl->Thaw();
	GetGame()->RefreshActorPropertyWindowList();

	bBuildTreeFlag = FALSE;

	// Set cursor back to normal
	wxSetCursor(wxNullCursor);
}

/**
 * Refresh helper -- adds a class folder if one doesn't already exist.
 */
wxTreeItemId WxRemoteControlActorsPage::AddClassItem(TMap<FString, wxTreeItemId> &ClassItemMap, const FString &ClassName)
{
	// no owner, add under top-level classes
	wxTreeItemId *FindId = ClassItemMap.Find(ClassName);
	wxTreeItemId ParentItemId;

	if( FindId )
	{
		ParentItemId = *FindId;
	}
	else
	{
		// Add item.
		ParentItemId = TreeCtrl->AppendItem(TreeCtrl->GetRootItem()
											  , *ClassName
											  , TREE_ICON_FOLDER_CLOSED
											  , TREE_ICON_FOLDER_OPEN);
		ClassItemMap.Set(*ClassName, ParentItemId);
	}

	return ParentItemId;
}

/**
 * Set the enabled/disabled status on the toolbar.
 */
void WxRemoteControlActorsPage::UpdateToolUI()
{
	UBOOL bEnable = FALSE;
	if (GWorld == NULL || GWorld->GetNetMode() == NM_Standalone)
	{
		const wxTreeItemId SelectedItem = TreeCtrl->GetSelection();
		if( SelectedItem.IsOk() )
		{
			if( TreeCtrl->GetItemData(SelectedItem) )
			{
				bEnable = TRUE;
			}
		}
	}
	ToolBar->EnableTool( ID_REMOTECONTROL_ACTORSPAGE_EDIT_ACTOR, bEnable == TRUE );
}

/**
 * Called when user presses dynamic actors toggle.
 */
void WxRemoteControlActorsPage::OnToggleDynamicActors(wxCommandEvent &In)
{
	bDynamicActors = !bDynamicActors;
	bBuildTreeFlag = TRUE;
	RefreshPage();
}

/**
 * Called when user presses "debug sub-objects" toggle.
 */
void WxRemoteControlActorsPage::OnToggleDebugSubobjects(wxCommandEvent &In)
{
	bDebugSubobjects = !bDebugSubobjects;
	bBuildTreeFlag = TRUE;
	RefreshPage();
}

/**
 * Called when user presses "auto-expand actor tree" toggle.
 */
void WxRemoteControlActorsPage::OnAutoExpandActorTree(wxCommandEvent &In)
{
	bAutoExpandActorTree = !bAutoExpandActorTree;
	bBuildTreeFlag = TRUE;
	RefreshPage();
}

/**
 * Called when user presses "Actor properties" button.
 */
void WxRemoteControlActorsPage::OnEditActor(wxCommandEvent &In)
{
	if (GWorld == NULL || GWorld->GetNetMode() == NM_Standalone)
	{
		const wxTreeItemId SelectedItem = TreeCtrl->GetSelection();
		if( SelectedItem.IsOk() )
		{
			ActorTreeItemData *TreeData = (ActorTreeItemData *)TreeCtrl->GetItemData(SelectedItem);
			if( TreeData )
			{
				GetGame()->ShowEditActor( TreeData->GetActorName().GetData() );
			}
		}
	}
}

/**
 * Called when user presses "Trace actor properties" button.
 */
void WxRemoteControlActorsPage::OnEditActorTrace(wxCommandEvent &In)
{
	if (GWorld != NULL && GWorld->GetNetMode() == NM_Standalone)
	{
		AActor* HitActor = NULL;

		ULocalPlayer* LocalPlayer = GetGame()->GetLocalPlayer();
		if ( LocalPlayer )
		{
			AActor* Player = LocalPlayer->Actor;
			APlayerController* PC = Cast<APlayerController>( Player );
			if ( PC )
			{
				// Do a trace in the player's facing direction and edit anything that's hit.
				FVector PlayerLocation;
				FRotator PlayerRotation;
				PC->eventGetPlayerViewPoint(PlayerLocation, PlayerRotation);
				FCheckResult Hit(1.0f);
				// Prevent the trace intersecting with the player's pawn.
				if( PC->Pawn )
				{
					Player = PC->Pawn;
				}
				GWorld->SingleLineCheck(Hit, Player, PlayerLocation + PlayerRotation.Vector() * 10000, PlayerLocation, TRACE_SingleResult | TRACE_Actors);
				HitActor = Hit.Actor;
			}
		}

		if ( HitActor )
		{
			// Display a property window for the hit actor.
			GetGame()->ShowEditActor( *HitActor->GetPathName() );
		}
	}
}

/**
 * Called when the tree selection changes.
 */
void WxRemoteControlActorsPage::OnTreeSelChanged(wxTreeEvent &In)
{
	UpdateToolUI();
	const wxTreeItemId SelectedItem = TreeCtrl->GetSelection();

	UBOOL bIsActor = FALSE;
	if(SelectedItem.IsOk())
	{
		ActorTreeItemData *TreeData = (ActorTreeItemData *)TreeCtrl->GetItemData(SelectedItem);
		if( TreeData )
		{
			GetGame()->SetSelectedActor(TreeData->GetClassName().GetData(),
										TreeData->GetActorName().GetData());
			bIsActor = TRUE;
		}
	}
	
	if(!bIsActor)
	{
		GetGame()->SetSelectedActor(TEXT("Actor"), TEXT(""));
	}
}

/**
 * Called when the user double clicks an item.
 */
void WxRemoteControlActorsPage::OnTreeDoubleClick(wxTreeEvent &In)
{
	wxTreeItemId item = TreeCtrl->GetSelection();
	if(item.IsOk())
	{
		ActorTreeItemData *pTreeData = (ActorTreeItemData *)TreeCtrl->GetItemData(item);
		if(NULL != pTreeData)
		{
			GetGame()->ShowEditActor( pTreeData->GetActorName().GetData() );
		}
	}
}

/**
 * Builds a tree of sub-objects that have debug options.
 */
void WxRemoteControlActorsPage::BuildSubObjectTree(const wxTreeItemId& ParentID,
											     const FString& Start,
											     StringSubObjectItemMap& SubObjectMap)
{
	//Make sure root actor is in the map
	if(!SubObjectMap.Find(*Start))
	{
		SubObjectItem ActorItem;
		ActorItem.ActorID  = ParentID;
		ActorItem.ObjectID = ParentID;
		SubObjectMap.Set(*Start, ActorItem);
	}
	BuildSubObjectTreeHelper(ParentID, Start, SubObjectMap, ParentID, TRUE);
}

/**
 * Does the heavy lifting involved in building a debug option sub-object tree.
 */
UBOOL WxRemoteControlActorsPage::BuildSubObjectTreeHelper(const wxTreeItemId& ParentID,
												      const FString& Current,
													  StringSubObjectItemMap& ObjectMap,
													  const wxTreeItemId& ParentActorID,
													  UBOOL IsRoot)
{
	UBOOL DisplayFlag      = FALSE;
	UBOOL DebugOptionsFlag = FALSE;
	wxTreeItemId invalidId;
	SubObjectItem invalidSubObjectID(invalidId, invalidId);
	SubObjectItem* PreviouslySeenID = ObjectMap.Find(Current);

	// if at the root of tree, 
	// or current sub-object not encountered before, 
	// or sub-object has been seen before and was valid...
	if (IsRoot || (!PreviouslySeenID) || (PreviouslySeenID->ObjectID != invalidId))
	{
		wxTreeItemId CurrentID = ParentID;

		// if object has debug options, then make sure it is in the tree
		if (!IsRoot)
		{
			// tentatively add an item to the tree
			CurrentID = TreeCtrl->AppendItem(ParentID, *Current);
			TreeCtrl->SetItemData(CurrentID, new ActorTreeItemData(*GetGame()->GetObjectClass(Current),
				                    *Current));

			if ((PreviouslySeenID != NULL) && (PreviouslySeenID->ObjectID != invalidId))
			{
				// Determine if the current occurrence of this object is closer to an actor than the last occurrence
				// if so, then delete the old branch and build the new one.
				INT previousDistance = GetDistanceToChild(PreviouslySeenID->ActorID, PreviouslySeenID->ObjectID);
				INT currentDistance  = GetDistanceToChild(ParentActorID, CurrentID);
				if (previousDistance > currentDistance)
				{
					DeleteBranch(PreviouslySeenID->ObjectID, ObjectMap);
				}
				else
				{
					TreeCtrl->Delete(CurrentID);
					return FALSE;
				}
			}
			SubObjectItem newSubObjectItem;
			newSubObjectItem.ActorID  = ParentActorID;
			newSubObjectItem.ObjectID = CurrentID;

			ObjectMap.Set(*Current, newSubObjectItem);
		}

		TArray<FString> PropertyList;
		GetGame()->GetPropertyList(Current, PropertyList);
		for(TArray<FString>::TIterator PropertyIt(PropertyList); PropertyIt; ++PropertyIt)
		{
			FString &PropertyName = *PropertyIt;
			if (GetGame()->IsArrayProperty(Current, PropertyName))
			{
				TArray<FString> ArrayObjectList;
				GetGame()->GetArrayObjectList(Current, PropertyName, ArrayObjectList);
				for (TArray<FString>::TIterator ArrayObjectIt(ArrayObjectList); ArrayObjectIt; ++ArrayObjectIt)
				{
					FString PropertyObjectName;
					FString &ArrayItemObjectName = *ArrayObjectIt;
					if (!GetGame()->IsAActor(ArrayItemObjectName))
					{
						DisplayFlag |= BuildSubObjectTreeHelper(CurrentID,
																ArrayItemObjectName,
																ObjectMap,
																ParentActorID,
																FALSE);
					}
				}
			}
			else if (GetGame()->IsObjectProperty(Current, PropertyName))
			{
				FString PropertyObjectName;
				if (GetGame()->GetObjectFromProperty(Current, PropertyName, PropertyObjectName)
					&& !GetGame()->IsAActor(PropertyObjectName))
				{
					DisplayFlag |= BuildSubObjectTreeHelper(CurrentID,
					                                        PropertyObjectName,
															ObjectMap,
															ParentActorID,
															FALSE);
				}
			}
		}

		if (!IsRoot)
		{
			if (!(DisplayFlag || DebugOptionsFlag))
			{
				// remove item from tree since it has no debug options and has no children with debug options
				TreeCtrl->Delete(CurrentID);
				ObjectMap.Set(*Current, invalidSubObjectID);
			}
			else
			{
				// set image of the item to show whether it has debug options or not
				if ((DisplayFlag) && (!DebugOptionsFlag))
				{
					TreeCtrl->SetItemImage(CurrentID, TREE_ICON_SUBOBJECT);
				}
				else
				{
					TreeCtrl->SetItemImage(CurrentID, TREE_ICON_DEBUG_SUBOBJECT);
				}
			}
		}
	}

	return DisplayFlag || DebugOptionsFlag;
}

/**
 * Returns the number of tree nodes from an Ancestor node to a Child node.
 * Returns -1 if Child not related to Ancestor.
 * Ancestor should NOT be set to Root node.
 */
INT WxRemoteControlActorsPage::GetDistanceToChild(const wxTreeItemId& AncestorID, wxTreeItemId& ChildID)
{
	INT i;
	wxTreeItemId CurrentAncestorID = ChildID;

	for (i = 0; (AncestorID != CurrentAncestorID) && (CurrentAncestorID != TreeCtrl->GetRootItem()); ++i)
	{
		CurrentAncestorID = TreeCtrl->GetItemParent(CurrentAncestorID);
	}

	if (CurrentAncestorID != TreeCtrl->GetRootItem())
	{
		return i;
	}
	else
	{
		return -1;
	}
}

/**
 * Delete the current branch and prune unneeded sub-objects upwards from delete point.
 */
void WxRemoteControlActorsPage::DeleteBranch(wxTreeItemId Node, StringSubObjectItemMap& ObjectMap)
{
	TArray<wxTreeItemId> ItemStack;
	ItemStack.AddItem( Node );

	// used for upwards tree pruning
	wxTreeItemId CurrentNode = TreeCtrl->GetItemParent(Node);

	while( ItemStack.Num() > 0 )
	{
		wxTreeItemIdValue cookie;

		const INT StackTopIndex = ItemStack.Num() - 1;
		wxTreeItemId curId = ItemStack( StackTopIndex );

		wxTreeItemId child = TreeCtrl->GetFirstChild(curId, cookie);
	
		// Pop the node if no children exist.
		if(!child.IsOk())
		{
			ActorTreeItemData* NodeData = (ActorTreeItemData*)(TreeCtrl->GetItemData(curId));
			check(NodeData);
			ObjectMap.Remove(FString(NodeData->GetActorName()));
			TreeCtrl->Delete(curId);
			// Pop.
			ItemStack.Remove( StackTopIndex );
		}
		else
		{
			// Push children onto the stack.
			for(;child.IsOk(); child = TreeCtrl->GetNextChild(curId, cookie))
			{
				ItemStack.AddItem( child );
			}
		}
	}		

	// now climb back up the tree deleting nodes until a
	// node with multiple children/Actor Node/Debuggable Node is found

	ActorTreeItemData* CurrentData = (ActorTreeItemData*)(TreeCtrl->GetItemData(CurrentNode));
	check(CurrentData);
	FString CurrentNodeName(CurrentData->GetActorName());
	wxTreeItemId ParentNode = TreeCtrl->GetItemParent(CurrentNode);

	while(!GetGame()->IsAActor(CurrentNodeName)
		  && !(TreeCtrl->GetChildrenCount(CurrentNode) > 0))
	{
		ObjectMap.Remove(FString(CurrentData->GetActorName()));
		TreeCtrl->Delete(CurrentNode);
		CurrentNode = ParentNode;
		ParentNode  = TreeCtrl->GetItemParent(CurrentNode);
		if(ParentNode == TreeCtrl->GetRootItem())
		{
			break;
		}
		CurrentData = (ActorTreeItemData*)(TreeCtrl->GetItemData(CurrentNode));
		check(CurrentData);
		CurrentNodeName.Printf(TEXT("%s"), CurrentData->GetActorName().c_str());
	}
}
