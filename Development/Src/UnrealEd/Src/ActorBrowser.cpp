/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EnginePrefabClasses.h"
#include "UnClassTree.h"
#include "ActorBrowser.h"

class WxActorBrowserTreeData : public wxTreeItemData
{
public:
	WxActorBrowserTreeData( const INT InIndex) { ClassIndex = InIndex; }

	INT ClassIndex;
};


/**
 * Derived WxTreeCtrl with custom save and restore expansion state
 */

IMPLEMENT_DYNAMIC_CLASS(WxActorBrowserTreeCtrl,WxTreeCtrl);

/** 
 * Loops through all of the elements of the tree and adds selected items to the selection set,
 * and expanded items to the expanded set.
 */
void WxActorBrowserTreeCtrl::SaveSelectionExpansionState()
{
	ActorBrowserSavedSelections.Empty();
	ActorBrowserSavedExpansions.Empty();

	// Recursively traverse and tree and store selection states based on the client data.
	wxTreeItemId Root = GetRootItem();

	if(Root.IsOk())
	{
		SaveSelectionExpansionStateRecurse(Root);
	}
}

/** 
 * Loops through all of the elements of the tree and sees if the client data of the item is in the 
 * selection or expansion set, and modifies the item accordingly.
 */
void WxActorBrowserTreeCtrl::RestoreSelectionExpansionState()
{
	// Recursively traverse and tree and restore selection states based on the client data.
	Freeze();
	{
		wxTreeItemId Root = GetRootItem();

		if(Root.IsOk())
		{
			RestoreSelectionExpansionStateRecurse(Root);
		}
	}
	Thaw();

	ActorBrowserSavedSelections.Empty();
	ActorBrowserSavedExpansions.Empty();
}


/** 
 * Recursion function that loops through all of the elements of the tree item provided and saves their select/expand state. 
 * 
 * @param Item Item to use for the root of this recursion.
 */
void WxActorBrowserTreeCtrl::SaveSelectionExpansionStateRecurse(wxTreeItemId& Item)
{
	// Expand and select this item
	WxActorBrowserTreeData* ActorBrowserTreeItem = static_cast<WxActorBrowserTreeData*>(GetItemData(Item));

	const UBOOL bIsRoot = (Item == GetRootItem());
	const UBOOL bVirtualRoot = ((GetWindowStyle() & wxTR_HIDE_ROOT) == wxTR_HIDE_ROOT);
	const UBOOL bProcessItem = (bIsRoot == FALSE) || (bVirtualRoot == FALSE);

	if( bProcessItem )
	{
		if(ActorBrowserTreeItem != NULL)
		{
			INT ClassIndex = ActorBrowserTreeItem->ClassIndex;
			if(IsSelected(Item))
			{
				ActorBrowserSavedSelections.Set(ClassIndex, ClassIndex);
			}
			if(IsExpanded(Item))
			{
				ActorBrowserSavedExpansions.Set(ClassIndex, ClassIndex);
			}
		}
	}

	// Loop through all of the item's children and store their state.
	wxTreeItemIdValue Cookie;
	wxTreeItemId ChildItem =GetFirstChild( Item, Cookie );

	while(ChildItem.IsOk())
	{
		SaveSelectionExpansionStateRecurse(ChildItem);

		ChildItem = GetNextChild(Item, Cookie);
	}
}


/** 
 * Recursion function that loops through all of the elements of the tree item provided and restores their select/expand state. 
 * 
 * @param Item Item to use for the root of this recursion.
 */
void WxActorBrowserTreeCtrl::RestoreSelectionExpansionStateRecurse(wxTreeItemId& Item)
{
	// Expand and select this item
	WxActorBrowserTreeData* ActorBrowserTreeItem = static_cast<WxActorBrowserTreeData*>(GetItemData(Item));

	const UBOOL bIsRoot = (Item == GetRootItem());
	const UBOOL bVirtualRoot = ((GetWindowStyle() & wxTR_HIDE_ROOT) == wxTR_HIDE_ROOT);
	const UBOOL bProcessItem = (bIsRoot == FALSE) || (bVirtualRoot == FALSE);

	if( bProcessItem )
	{
		if(ActorBrowserTreeItem != NULL)
		{
			INT ClassIndex = ActorBrowserTreeItem->ClassIndex;
			const UBOOL bItemSelected = ActorBrowserSavedSelections.Find(ClassIndex) != NULL;
			const UBOOL bItemExpanded = ActorBrowserSavedExpansions.Find(ClassIndex) != NULL;
			if(bItemSelected == TRUE)
			{
				SelectItem(Item);
			}
			if(bItemExpanded == TRUE)
			{
				Expand(Item);
			}
		}
	}

	// Loop through all of the item's children and select/expand them.
	wxTreeItemIdValue Cookie;
	wxTreeItemId ChildItem =GetFirstChild( Item, Cookie );

	while(ChildItem.IsOk())
	{
		RestoreSelectionExpansionStateRecurse(ChildItem);

		ChildItem = GetNextChild(Item, Cookie);
	}
}

/**
 * The menu for the actor browser.
 */
class WxMBActorBrowser : public wxMenuBar
{
public:
	WxMBActorBrowser()
	{
		// File menu
		wxMenu* FileMenu = new wxMenu();

		FileMenu->Append( IDMN_AB_FileOpen, *LocalizeUnrealEd("OpenE"), TEXT("") );
		FileMenu->Append( IDMN_AB_EXPORT_ALL, *LocalizeUnrealEd("ExportAllScripts"), TEXT("") );

		Append( FileMenu, *LocalizeUnrealEd("File") );

		WxBrowser::AddDockingMenu( this );
	}
};

BEGIN_EVENT_TABLE( WxActorBrowser, WxBrowser )
	EVT_MENU( IDMN_AB_FileOpen, WxActorBrowser::OnFileOpen )
	EVT_MENU( IDMN_AB_EXPORT_ALL, WxActorBrowser::OnFileExportAll )
	EVT_MENU( IDMN_ActorBrowser_CreateArchetype, WxActorBrowser::OnCreateArchetype )
	EVT_TIMER( IDMN_ActorBrowser_SearchTimer, WxActorBrowser::OnSearchTimer )
END_EVENT_TABLE()

/** @return			A new class tree. */
static FClassTree* CreateClassTree()
{
	FClassTree* ClassTree = new FClassTree( UObject::StaticClass() );
	check( ClassTree );
	for( TObjectIterator<UClass> It ; It ; ++It )
	{
		UClass* CurClass = *It;
		ClassTree->AddClass( CurClass );
	}
	return ClassTree;
}

WxActorBrowser::WxActorBrowser()
	:	bUpdateOnActivated( FALSE )
{
	bIsRepopulating	= FALSE;

	// Register the ActorBrowser event
	GCallbackEvent->Register(CALLBACK_RefreshEditor_ActorBrowser,this);

	RightClickedClass = NULL;
}

WxActorBrowser::~WxActorBrowser()
{
}

/**
 * Forwards the call to our base class to create the window relationship.
 * Creates any internally used windows after that
 *
 * @param DockID the unique id to associate with this dockable window
 * @param FriendlyName the friendly name to assign to this window
 * @param Parent the parent of this window (should be a Notebook)
 */
void WxActorBrowser::Create(INT DockID,const TCHAR* FriendlyName,wxWindow* Parent)
{
	WxBrowser::Create(DockID,FriendlyName,Parent);

	Panel = (wxPanel*)wxXmlResource::Get()->LoadPanel( this, TEXT("ID_PANEL_ACTORCLASSBROWSER") );
	check( Panel );
	Panel->Fit();

	TreeCtrl = wxDynamicCast( FindWindow( XRCID( "IDTC_CLASSES" ) ), WxActorBrowserTreeCtrl );
	check( TreeCtrl != NULL );
	ActorAsParentCheck = wxDynamicCast( FindWindow( XRCID( "IDCK_USE_ACTOR_AS_PARENT" ) ), wxCheckBox );
	check( ActorAsParentCheck != NULL );
	PlaceableCheck = wxDynamicCast( FindWindow( XRCID( "IDCK_PLACEABLE_CLASSES_ONLY" ) ), wxCheckBox );
	check( PlaceableCheck != NULL );
	ShowCategoriesCheck = wxDynamicCast( FindWindow( XRCID( "IDCK_SHOW_CATEGORIES" ) ), wxCheckBox );
	check( ShowCategoriesCheck != NULL );
	FullPathStatic = wxDynamicCast( FindWindow( XRCID( "IDST_FULLPATH" ) ), wxStaticText );
	check( FullPathStatic != NULL );
	SearchTextCtrl = wxDynamicCast( FindWindow( XRCID( "IDTC_SEARCH_FILTER") ), wxTextCtrl );
	check( SearchTextCtrl != NULL );
	ClearSearchButton = wxDynamicCast( FindWindow( XRCID( "IDB_CLEAR_SEARCH_FILTER" ) ), wxBitmapButton );
	check( ClearSearchButton != NULL );
	ClearB.Load( "Cancel.png" );
	ClearDisabledB.Load( "Cancel_Disabled.png" );
	ClearSearchButton->SetBitmapLabel( ClearB );
	ClearSearchButton->SetBitmapDisabled( ClearDisabledB );
	ClearSearchButton->SetToolTip( *LocalizeUnrealEd("ClearQ") );

	MenuBar = new WxMBActorBrowser();

	bForceActorClassesUpdate = TRUE;
	Update();

	ADDEVENTHANDLER( XRCID("IDTC_CLASSES"), wxEVT_COMMAND_TREE_ITEM_EXPANDING, &WxActorBrowser::OnItemExpanding );
	ADDEVENTHANDLER( XRCID("IDTC_CLASSES"), wxEVT_COMMAND_TREE_SEL_CHANGED, &WxActorBrowser::OnSelChanged );
	ADDEVENTHANDLER( XRCID("IDTC_CLASSES"), wxEVT_COMMAND_TREE_ITEM_RIGHT_CLICK, &WxActorBrowser::OnItemRightClicked );
	ADDEVENTHANDLER( XRCID("IDTC_CLASSES"), wxEVT_COMMAND_TREE_BEGIN_DRAG, &WxActorBrowser::OnBeginDrag );
	ADDEVENTHANDLER( XRCID("IDCK_USE_ACTOR_AS_PARENT"), wxEVT_COMMAND_CHECKBOX_CLICKED , &WxActorBrowser::OnUseActorAsParent );
	ADDEVENTHANDLER( XRCID("IDCK_PLACEABLE_CLASSES_ONLY"), wxEVT_COMMAND_CHECKBOX_CLICKED , &WxActorBrowser::OnPlaceableClassesOnly );
	ADDEVENTHANDLER( XRCID("IDCK_SHOW_CATEGORIES"), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxActorBrowser::OnShowCategories );
	ADDEVENTHANDLER( XRCID("IDTC_SEARCH_FILTER"), wxEVT_COMMAND_TEXT_UPDATED, &WxActorBrowser::OnSearchTextUpdated );
	ADDEVENTHANDLER( XRCID("IDB_CLEAR_SEARCH_FILTER"), wxEVT_COMMAND_BUTTON_CLICKED, &WxActorBrowser::OnClearSearchTextClicked );
	ADDEVENTHANDLER( XRCID("IDTC_SEARCH_FILTER"), wxEVT_COMMAND_TEXT_ENTER, &WxActorBrowser::OnSearchTextEnter );
	ADDEVENTHANDLER( XRCID("IDB_CLEAR_SEARCH_FILTER"), wxEVT_UPDATE_UI, &WxActorBrowser::UpdateUI_ClearSearchTextButton );

	FLocalizeWindow( this );
	SetLabel(FriendlyName);

	SearchTimer.SetOwner( this, IDMN_ActorBrowser_SearchTimer );

#if UDK
	// In UDK show categories option is the default
	ShowCategoriesCheck->SetValue(TRUE);
#endif
}

/*
template< class Comparator >
void GetChildClasses( TArray<UClass*>& ChildClasses, const Comparator& Mask, UBOOL bRecurse=FALSE ) const

*/
void WxActorBrowser::Serialize( FArchive& Ar )
{
	if ( Ar.IsObjectReferenceCollector() && TreeCtrl != NULL )
	{
		FEditorClassHierarchy* EditorClassHierarchy = GEditor->EditorClassHierarchy;
		check(EditorClassHierarchy);

		TArray<UClass*> AllClasses;
		const INT StartNode = 0;
		EditorClassHierarchy->GetAllClasses(StartNode, AllClasses);

		for ( INT ClassIdx = 0; ClassIdx < AllClasses.Num(); ClassIdx++ )
		{
			UClass* Cls = AllClasses(ClassIdx);
			Ar << Cls;
		}
	}
}

void WxActorBrowser::Send(ECallbackEventType InType)
{
	if(InType == CALLBACK_RefreshEditor_ActorBrowser || InType == CALLBACK_RefreshEditor_AllBrowsers)
	{
		Update();
	}
}

void WxActorBrowser::Send(ECallbackEventType InType, UObject* InObject)
{
	if(InType == CALLBACK_RefreshEditor_ActorBrowser || InType == CALLBACK_RefreshEditor_AllBrowsers)
	{
		debugf(TEXT("No arguments passed to this callback!"));
	}
}

void WxActorBrowser::Update()
{
	BeginUpdate();

	UBOOL bUpdate = IsShownOnScreen();
	if( bForceActorClassesUpdate )
	{
		bUpdate = TRUE;
	}
	if ( bUpdate )
	{
		RepopulateClassTree();

		// RepopulateClassTree relies on this bool being set after it's called, in order to determine if the list of expanded items were
		// already stored due to Update() being called when the actor browser wasn't visible (thus bUpdateOnActivated was set to TRUE)
		bUpdateOnActivated = FALSE;

		bForceActorClassesUpdate = FALSE;
	}
	else
	{
		bUpdateOnActivated = TRUE;
	}

	EndUpdate();
}

void WxActorBrowser::Activated()
{
	WxBrowser::Activated();
	if ( bUpdateOnActivated )
	{
		Update();
	}
}

/** Adds any children with no class groups to the passed in Parent 
 * 
 * @param ClassIndex			The parent class index to get children from
 * @param ParentId				The wxWidgets tree ID of the parent class
 * @param ClassesAlreadyAdded	The classes that have already been added to the tree
 */
void WxActorBrowser::AddUngroupedChildrenInternal( INT ClassIndex, const wxTreeItemId& ParentId, TSet<INT>& ClassIndicesAlreadyAdded )
{
	FEditorClassHierarchy* EditorClassHierarchy = GEditor->EditorClassHierarchy;
	
	// Get a list of children
	TArray<INT> ChildrenStack;
	EditorClassHierarchy->GetChildrenOfClass( ClassIndex, ChildrenStack );

	// Examine each child to see if it should be added to the tree
	while( ChildrenStack.Num() > 0 )
	{
		const INT CurIndexToCheck = ChildrenStack.Pop();
		
		// Skip any children that have already been added
		if( ClassIndicesAlreadyAdded.Contains( CurIndexToCheck ) )
		{
			continue;
		}

		const UBOOL bIsPlaceable = EditorClassHierarchy->IsPlaceable(CurIndexToCheck );
		const UBOOL bIsAbstract = EditorClassHierarchy->IsAbstract(CurIndexToCheck);

		// Get any class groups that the children should be added to.
		TArray<FString> ClassGroups;
		EditorClassHierarchy->GetClassGroupNames( CurIndexToCheck, ClassGroups );
		
		// Since we are adding ungrouped children, skip any that have class groups
		if( ClassGroups.Num() == 0 )
		{
			// If the class is valid to be shown add a wxTreeItem for it.
			if( !IsIndexFiltered( CurIndexToCheck ) && !bIsAbstract && bIsPlaceable )
			{
				const FString& ClassName = EditorClassHierarchy->GetClassName( CurIndexToCheck );
				wxTreeItemId ClassId = TreeCtrl->AppendItem( ParentId, *ClassName, -1, -1, new WxActorBrowserTreeData(CurIndexToCheck) );

				// We have now added this class to the tree
				ClassIndicesAlreadyAdded.Add( CurIndexToCheck );

				// Recurse over any children that the current class may have
				AddUngroupedChildrenInternal( CurIndexToCheck, ClassId, ClassIndicesAlreadyAdded );
			}
			else
			{
				// The current class is not valid but its children may be so recurse over all children
				AddUngroupedChildrenInternal( CurIndexToCheck, ParentId, ClassIndicesAlreadyAdded );
			}
		}
	}

	// Expand any children that should be.
	if ( ClassIndicesToExpand.Contains( ClassIndex ) )
	{
		TreeCtrl->Expand( ParentId );
	}
}

/** Adds any children with no class groups to the passed in Parent 
 * 
 * @param ClassIndex			The parent class index to get children from
 * @param ParentId				The wxWidgets tree ID of the parent class
 * @param ClassesAlreadyAdded	The classes that have already been added to the tree
 */
UINT WxActorBrowser::AddUngroupedChildren( INT ClassIndex, const wxTreeItemId& ParentId, TSet<INT>& ClassIndicesAlreadyAdded )
{
	AddUngroupedChildrenInternal( ClassIndex, ParentId, ClassIndicesAlreadyAdded );
 	return TreeCtrl->GetChildrenCount( ParentId, TRUE );
}

/** A small helper for keeping track of group entries in the tree */
struct ClassGroupEntry
{
	wxTreeItemId TreeId;
	UINT NumClassChildren;

	ClassGroupEntry() : NumClassChildren(0)
	{
	}
};

/** Builds a category based treeview starting at the passed in index */
void WxActorBrowser::BuildClassGroups( INT RootClassIndex )
{
	// A mapping of group names to their tree entries
	TMap<FString, ClassGroupEntry> GroupNameToEntryMap;

	// A set of all classes that have already been added to the tree
	TSet<INT> ClassIndicesAlreadyAdded;

	FEditorClassHierarchy* EditorClassHierarchy = GEditor->EditorClassHierarchy;

	// The current list of classes we are checking
	TArray<INT> IndicesToCheckStack;

	// Get all children of the root class
	EditorClassHierarchy->GetChildrenOfClass( RootClassIndex, IndicesToCheckStack );

	// Ensure there are no preexisting items in tree
	TreeCtrl->DeleteAllItems();

	FString ClassName = EditorClassHierarchy->GetClassName(RootClassIndex);

	// In category mode the root of the tree should be a node called categories
	wxTreeItemId RootId = TreeCtrl->AddRoot( TEXT("Categories") );

	// Make a temporary root entry that will be used as the starting parent when examining category relationships
	ClassGroupEntry RootEntry;
	RootEntry.NumClassChildren = 0;
	RootEntry.TreeId = RootId;

	// The category name for any class that isnt part of a group. 
	const FString UncategorizedName( TEXT("Uncategorized") );

	// Iterate over every class and examine where it should be placed in the tree (if at all)
	while ( IndicesToCheckStack.Num() > 0 )
	{
		const INT CurIndexToCheck = IndicesToCheckStack.Pop();
		
		const UBOOL bIsPlaceable = EditorClassHierarchy->IsPlaceable(CurIndexToCheck );
		const UBOOL bIsAbstract = EditorClassHierarchy->IsAbstract(CurIndexToCheck);

		// Get the list of group names for this class.
		TArray<FString> ClassGroups;
		EditorClassHierarchy->GetClassGroupNames( CurIndexToCheck, ClassGroups );

		const FString& ClassName = EditorClassHierarchy->GetClassName( CurIndexToCheck );
		if( ClassGroups.Num() >  0 )
		{
			//Each new category's parent is the root entry
			ClassGroupEntry* ParentEntry = &RootEntry;
			
			// The class belongs to at least one group, iterate over every group and create a tree item for them.
			// We also determine where in the tree the current class should be located
			for( INT GroupIndex = 0; GroupIndex < ClassGroups.Num(); ++GroupIndex )
			{	
				// See if we already created a group entry with the same name. Classes can belong to the same group 
				// even if they have no relationship to each other
				ClassGroupEntry* Entry = GroupNameToEntryMap.Find( ClassGroups( GroupIndex ) );
				if( !Entry )
				{
					// An entry for the current group was not found.  Add a new one now
					ClassGroupEntry NewEntry;
					NewEntry.TreeId = TreeCtrl->AppendItem( ParentEntry->TreeId, *ClassGroups(GroupIndex), -1, -1, NULL );
					TreeCtrl->SetItemBold( NewEntry.TreeId );

					// If this is the last group in the current list of groups attempt to add the current class under the last group
					// Classes can have nested groups but should only be added under the childmost group
					if( GroupIndex == ClassGroups.Num()-1 )
					{
						if( !IsIndexFiltered( CurIndexToCheck ) && bIsPlaceable && !bIsAbstract )
						{
							// Only add the class if its not filtered for any reason
							wxTreeItemId ClassId = TreeCtrl->AppendItem( NewEntry.TreeId , *ClassName, -1, -1, new WxActorBrowserTreeData(CurIndexToCheck) );
							// The class has now been added to the tree
							ClassIndicesAlreadyAdded.Add( CurIndexToCheck );

							// The current group entry has had one child added to it
							++NewEntry.NumClassChildren;
							// Add any children that do not belong to a group.  All children of a class that do not explicitly specify a group are parented under their normal class parent
							NewEntry.NumClassChildren += AddUngroupedChildren( CurIndexToCheck, ClassId, ClassIndicesAlreadyAdded );
						}
						else
						{
							// The current class is filtered out but we still need to examine children.  
							// Add any children that do not belong to a group.  All children of a class that do not explicitly specify a group are parented under their normal class parent
							// and in this case since the parent is filtered out, we add it ungrouped children directly to the group
							NewEntry.NumClassChildren += AddUngroupedChildren( CurIndexToCheck, NewEntry.TreeId , ClassIndicesAlreadyAdded );
						}
					}

					// Parent entries number of class children are the summation of all child entry number of class children.
					ParentEntry->NumClassChildren += NewEntry.NumClassChildren;
					// Add a new entry into the map and set it as the new parent entry
					ParentEntry = &GroupNameToEntryMap.Set( ClassGroups(GroupIndex), NewEntry );
				}
				else
				{
					// An entry for the current group was already found

					// If this is the last group in the current list of groups attempt to add the current class under the last group
					// Classes can have nested groups but should only be added under the childmost group
					if( GroupIndex == ClassGroups.Num() - 1 )
					{
						if( !IsIndexFiltered( CurIndexToCheck ) && bIsPlaceable && !bIsAbstract )
						{
							// Only add the class if its not filtered for any reason
							wxTreeItemId ClassId = TreeCtrl->AppendItem( Entry->TreeId, *ClassName, -1, -1, new WxActorBrowserTreeData(CurIndexToCheck) );
							// The class has now been added to the tree
							ClassIndicesAlreadyAdded.Add( CurIndexToCheck );
							
							// The current group entry has had one child added to it
							++(Entry->NumClassChildren);

							// Add any children that do not belong to a group.  All children of a class that do not explicitly specify a group are parented under their normal class parent
							Entry->NumClassChildren += AddUngroupedChildren( CurIndexToCheck, ClassId, ClassIndicesAlreadyAdded );
						}
						else
						{
							// The current class is filtered out but we still need to examine children.  
							// Add any children that do not belong to a group.  All children of a class that do not explicitly specify a group are parented under their normal class parent
							// and in this case since the parent is filtered out, we add it ungrouped children directly to the group
							Entry->NumClassChildren += AddUngroupedChildren( CurIndexToCheck, Entry->TreeId, ClassIndicesAlreadyAdded );
						}
					}
				
					// Parent entries number of class children are the summation of all child entry number of class children.
					ParentEntry->NumClassChildren += Entry->NumClassChildren;
					// Set the current entry as the new parent entry
					ParentEntry = Entry;
				}
			}
		}
		else
		{
			// The current class has no group.  Add it to the uncategorized group unless its already been added by a parent class that does have a group
 			if( !ClassIndicesAlreadyAdded.Contains( CurIndexToCheck ) )
 			{
				// Find the uncategorized entry and create one if it doesnt exist.
				ClassGroupEntry* UncategorizedEntry = GroupNameToEntryMap.Find( UncategorizedName );
				if( !UncategorizedEntry )
				{
					ClassGroupEntry NewEntry;
					// An entry for this group does not exist. Create it now
					NewEntry.TreeId  = TreeCtrl->AppendItem( RootId, *UncategorizedName, -1, -1, NULL );
					NewEntry.NumClassChildren = 0;
					TreeCtrl->SetItemBold( NewEntry.TreeId );
					UncategorizedEntry = &GroupNameToEntryMap.Set( UncategorizedName, NewEntry );
				}

				if( !IsIndexFiltered( CurIndexToCheck ) && bIsPlaceable && !bIsAbstract )
				{
					// Only add the class if its not filtered for any reason
					wxTreeItemId ClassId = TreeCtrl->AppendItem( UncategorizedEntry->TreeId, *ClassName, -1, -1, new WxActorBrowserTreeData(CurIndexToCheck) );
					ClassIndicesAlreadyAdded.Add( CurIndexToCheck );
					// Add any children that do not belong to a group.  All children of a class that do not explicitly specify a group are parented under their normal class parent
					UncategorizedEntry->NumClassChildren += AddUngroupedChildren( CurIndexToCheck, ClassId, ClassIndicesAlreadyAdded );
				}
				else
				{
					// Add any children that do not belong to a group.  All children of a class that do not explicitly specify a group are parented under their normal class parent
					UncategorizedEntry->NumClassChildren += AddUngroupedChildren( CurIndexToCheck, UncategorizedEntry->TreeId, ClassIndicesAlreadyAdded );
				}
 			}
		}

		// Push all of the child indices of the current index onto the stack for processing
		// unless they have already been added elseware in the tree
		TArray<INT> ChildIndices;
		EditorClassHierarchy->GetChildrenOfClass( CurIndexToCheck, ChildIndices );
		for( INT ChildIndex = 0; ChildIndex < ChildIndices.Num(); ++ChildIndex )
		{
			if( !ClassIndicesAlreadyAdded.Contains( ChildIndices( ChildIndex ) ) )
			{
				IndicesToCheckStack.AddItem( ChildIndices( ChildIndex ) );
			}
		}
		
	}

	// Iterate through all groups to see if they have any children.  A group can have no children
	// if all the children were filtered out for some reason (like not passing a search)
	for( TMap<FString,ClassGroupEntry>::TConstIterator It(GroupNameToEntryMap); It; ++It )
	{
		const ClassGroupEntry& Entry = It.Value();
		if( Entry.NumClassChildren == 0 && Entry.TreeId.IsOk() )
		{
			// The group has no children, delete it
			TreeCtrl->Delete( Entry.TreeId );
		}
		else
		{
			// The grop has children.  Sort them now
			TreeCtrl->SortChildren( Entry.TreeId );
		}
	}

	// Sort all categories
	TreeCtrl->SortChildren( RootId );

	
	if( SearchTextCtrl->GetValue().Len() > 0 )
	{
		// Expand all nodes if we are searching
		TreeCtrl->ExpandAll();
	}
	else
	{
		// Find the "Common" entry and expand it by default.  These are popular classes 
		ClassGroupEntry* CommonEntry = GroupNameToEntryMap.Find( TEXT("Common") );
		if( CommonEntry && SearchTextCtrl->GetValue().Len() == 0 )
		{
			TreeCtrl->Expand( CommonEntry->TreeId );
		}

		// Expand the root node to show all categories
		TreeCtrl->Expand( RootId );
	}
}	

void WxActorBrowser::RepopulateClassTree()
{
	// fix for a recursion issue - FilterClasses(), below, ends up triggering a CALLBACK_RefreshEditor_ActorBrowser which ends up causing this function to be called again...
	// this causes a wxWidgets crash due to multiple calls to AddRoot() -following- DeleteAllItems .....
	if( bIsRepopulating )
	{
		return;
	}
	bIsRepopulating	= TRUE;

	TreeCtrl->Freeze();
	TreeCtrl->SaveSelectionExpansionState();

	TreeCtrl->DeleteAllItems();

	FEditorClassHierarchy* EditorClassHierarchy = GEditor->EditorClassHierarchy;
	check(EditorClassHierarchy);
	if (!EditorClassHierarchy->WasLoadedSuccessfully())
	{
		checkf(FALSE, TEXT("Couldn't find Manifest.txt, it should have been placed in the Scripts folder when compiling scripts!"));
	}

	INT RootClassIndex;
	if( ActorAsParentCheck->IsChecked() )
	{
		//find actor
		RootClassIndex = EditorClassHierarchy->Find(TEXT("Actor"));
	}
	else
	{
		//find object
		RootClassIndex = EditorClassHierarchy->Find(TEXT("Object"));
	}

	// Filter out the classes with any of the currently applied filters
	FilterClasses( RootClassIndex );


	TreeCtrl->Enable( true );
	if( ShowCategoriesCheck->IsChecked() && ClassIndicesToShow.Num() > 0 )
	{
		// We are in category mode and not all items were filtered out.  Build a category based tree
		BuildClassGroups( RootClassIndex );
	}
	else if( !ShowCategoriesCheck->IsChecked() && !IsIndexFiltered( RootClassIndex ) )
	{
		// Show the normal class hierarchy
		// If the root isn't filtered out, add it and its immediate children to the tree
		FString ClassName = EditorClassHierarchy->GetClassName(RootClassIndex);
		TreeCtrl->AddRoot( *ClassName, -1, -1, new WxActorBrowserTreeData( RootClassIndex ) );

		AddChildren( TreeCtrl->GetRootItem() );
		TreeCtrl->Expand( TreeCtrl->GetRootItem() );
	}
	else
	{
		// If the root is filtered out or there are no class indices to show, no matches passed the filters at all! Display a no results message and disable the
		// tree so the user doesn't interact with it and cause a crash on this special "NULL" node
		TreeCtrl->AddRoot( *LocalizeUnrealEd("NoResults") );
		TreeCtrl->Enable( false );
	}


	TreeCtrl->RestoreSelectionExpansionState();
	TreeCtrl->Thaw();

	bIsRepopulating = FALSE;
}

void WxActorBrowser::AddChildren( wxTreeItemId InID )
{
	INT ClassIndex = ((WxActorBrowserTreeData*)(TreeCtrl->GetItemData( InID )))->ClassIndex;

	FEditorClassHierarchy* EditorClassHierarchy = GEditor->EditorClassHierarchy;
	check(EditorClassHierarchy);

	// Get a list of all direct descendants of this class.
	TArray<INT> ChildClassIndexArray;
	EditorClassHierarchy->GetChildrenOfClass(ClassIndex, ChildClassIndexArray);

	for ( INT i= 0 ; i < ChildClassIndexArray.Num() ; ++i )
	{
		INT ChildClassIndex = ChildClassIndexArray(i);

		// If the child has been filtered out by the filters, ignore it
		if ( IsIndexFiltered( ChildClassIndex ) )
		{
			continue;
		}

		const UBOOL bIsPlaceable = EditorClassHierarchy->IsPlaceable(ChildClassIndex );
		const UBOOL bIsAbstract = EditorClassHierarchy->IsAbstract(ChildClassIndex);
		const UBOOL bIsBrush = EditorClassHierarchy->IsBrush(ChildClassIndex);

		FString ChildClassName = EditorClassHierarchy->GetClassName(ChildClassIndex);

		if (ChildClassName.StartsWith(TEXT("SubstanceAir")))
		{
			ChildClassName = ChildClassName.Replace(TEXT("SubstanceAir"),TEXT("Substance"));
		}

		const wxTreeItemId Wk = TreeCtrl->AppendItem( InID, *ChildClassName, -1, -1, new WxActorBrowserTreeData( ChildClassIndex ) );

		// Mark placeable classes in bold.
		if( bIsPlaceable && !bIsAbstract && !bIsBrush)
		{
			TreeCtrl->SetItemBold( Wk );
		}
		else
		{
			TreeCtrl->SetItemTextColour( Wk, wxColour(96,96,96) );
		}

		// Forcibly set whether the item has children or not instead of adding all the children to the tree at once. Children will be physically added
		// as needed when an expansion occurs. Even though filtering requires that all of the children classes have been "processed," it's still better performance
		// to delay the creation of the children wx tree nodes until they're needed.
		TreeCtrl->SetItemHasChildren( Wk, IndexHasFilterPassingChildren( ChildClassIndex ) == TRUE );

		// If the item should be auto-expanded (to show something that matches a filter, for instance) then forcibly expand it now
		if ( ClassIndicesToExpand.Contains( ChildClassIndex ) )
		{
			TreeCtrl->Expand( Wk );
		}
	}

	TreeCtrl->SortChildren( InID );
}

/**
 * Helper method to filter the class tree by the various filters that might be in effect
 *
 * @param	InRootIndex	Index of the class to use as the tree's root
 */
void WxActorBrowser::FilterClasses( INT InRootIndex )
{
	// Reset the class indices that pass the filter
	ClassIndicesToShow.Empty( ClassIndicesToShow.Num() );
	ClassIndicesToExpand.Empty( ClassIndicesToExpand.Num() );

	// Only bother with filtering logic if any of the filtering controls are active
	if ( IsFiltering() )
	{
		const FString FilterString = SearchTextCtrl->GetValue().c_str();
		FEditorClassHierarchy* ClassHierarchy = GEditor->EditorClassHierarchy;
		
		// Avoid recursion by tracking which class indices have yet to be checked against the filters
		TArray<INT> IndicesToCheckStack;
		IndicesToCheckStack.Push( InRootIndex );

		// Iterate until all class indices starting from the specified root have been checked against the filter
		while ( IndicesToCheckStack.Num() > 0 )
		{
			const INT CurIndexToCheck = IndicesToCheckStack.Pop();
			const FString CurClassName = ClassHierarchy->GetClassName( CurIndexToCheck );

			const UBOOL bPassesTextFilter = PassesTextFilter( CurIndexToCheck, FilterString );
			const UBOOL bExpandIndex = bPassesTextFilter && FilterString.Len() > 0;
			const UBOOL bPassesAllFilters = bPassesTextFilter &&
											PassesPlaceableFilter( CurIndexToCheck ) && 
											PassesBrushFilter( CurIndexToCheck );
			
			// If the current class index passes all of the filters, add it to the set of filter-passing indices, as well as all of the indices of
			// its parent classes (or else it wouldn't be able to be shown in the tree!)
		
			if ( bPassesAllFilters )
			{
				ClassIndicesToShow.Add( CurIndexToCheck );

				// If we are showing categories we don't do this as we dont need to show parenting relationships in category mode.
				if ( !ShowCategoriesCheck->IsChecked() && CurIndexToCheck != InRootIndex )
				{
					INT ParentIndex = CurIndexToCheck;
					while ( ParentIndex != InRootIndex )
					{
						ParentIndex = ClassHierarchy->GetParentIndex( ParentIndex );
						ClassIndicesToShow.Add( ParentIndex );

						// If the class should be guaranteed to be shown (expanded), then mark each of its parents to be
						// auto-expanded
						if ( bExpandIndex )
						{
							ClassIndicesToExpand.Add( ParentIndex );
						}
					}
				}
			}

			// Push all of the child indices of the current index onto the stack for processing
			TArray<INT> ChildIndices;
			ClassHierarchy->GetChildrenOfClass( CurIndexToCheck, ChildIndices );
			IndicesToCheckStack.Append( ChildIndices );
		}
	}
}

/**
 * Returns TRUE if any filtering is in effect, FALSE otherwise
 *
 * @return	TRUE if any filtering is in effect; FALSE otherwise
 */
UBOOL WxActorBrowser::IsFiltering() const
{
	return ( SearchTextCtrl->GetValue().Len() > 0 || PlaceableCheck->IsChecked() );
}

/**
 * Returns TRUE if the provided class index is filtered out under the current filters
 *
 * @param	InClassIndex	Class index to check
 *
 * @return	TRUE if the provided class index represents a class that is filtered out under the current filters
 */
UBOOL WxActorBrowser::IsIndexFiltered( INT InClassIndex ) const
{
	return ( IsFiltering() && !ClassIndicesToShow.Contains( InClassIndex ) );
}

/**
 * Returns TRUE if the provided class index has children which pass all current filters
 *
 * @param	InClassIndex	Class index to check
 *
 * @return	TRUE if the class represented by the provided class index has at least one child that passes
 *			all filters currently in effect; FALSE if the class has no children that pass the filters
 */
UBOOL WxActorBrowser::IndexHasFilterPassingChildren( INT InClassIndex ) const
{
	UBOOL bHasUnfilteredChildren = FALSE;
	TArray<INT> ChildIndices;
	GEditor->EditorClassHierarchy->GetChildrenOfClass( InClassIndex, ChildIndices );

	for ( INT ChildIndex = 0; ChildIndex < ChildIndices.Num(); ++ChildIndex )
	{
		if ( !IsIndexFiltered( ChildIndices(ChildIndex) ) )
		{
			bHasUnfilteredChildren = TRUE;
			break;
		}
	}
	return bHasUnfilteredChildren;
}

/**
 * Returns TRUE if the provided class index passes the provided text filter (or the filter is empty)
 *
 * @param	InClassIndex	Class index to check
 * @param	TextFilter		Text filter to check the class name against
 *
 * @return	TRUE if the class represented by the provided class index passed the provided text filter
 */
UBOOL WxActorBrowser::PassesTextFilter( INT InClassIndex, const FString& TextFilter ) const
{
	const FString ClassName = GEditor->EditorClassHierarchy->GetClassName( InClassIndex );
	return ( TextFilter.Len() == 0 || ClassName.InStr( TextFilter, FALSE, TRUE ) != INDEX_NONE );
}

/**
 * Returns TRUE if the provided class index passes the placeable filter (or the placeable filter is disabled)
 *
 * @param	InClassIndex	Class index to check
 *
 * @return	TRUE if the class represented by the provided class index passed the placeable filter; FALSE if it did not
 */
UBOOL WxActorBrowser::PassesPlaceableFilter( INT InClassIndex ) const
{
	return ( !PlaceableCheck->IsChecked() || GEditor->EditorClassHierarchy->IsPlaceable( InClassIndex ) );
}

/**
 * Returns TRUE if the provided class index is not a brush
 *
 * @param	InClassIndex	Class index to check
 *
 * @return	TRUE if the class represented by the provided class index is not a brush
 */
UBOOL WxActorBrowser::PassesBrushFilter( INT InClassIndex ) const
{
	return !PlaceableCheck->IsChecked() || !GEditor->EditorClassHierarchy->IsBrush(InClassIndex);
}

void WxActorBrowser::OnFileOpen( wxCommandEvent& In )
{
	WxFileDialog OpenFileDialog( this, 
		*LocalizeUnrealEd("OpenPackage"), 
		*appScriptOutputDir(),
		TEXT(""),
		TEXT("Class Packages (*.u)|*.u|All Files|*.*\0\0"),
		wxOPEN | wxFILE_MUST_EXIST | wxMULTIPLE,
		wxDefaultPosition);

	if( OpenFileDialog.ShowModal() == wxID_OK )
	{
		wxArrayString	OpenFilePaths;
		OpenFileDialog.GetPaths(OpenFilePaths);

		for(UINT FileIndex = 0;FileIndex < OpenFilePaths.Count();FileIndex++)
		{
			UObject::LoadPackage( NULL, *FString(OpenFilePaths[FileIndex]), 0 );
		}
	}

	GCallbackEvent->Send( CALLBACK_RefreshEditor_AllBrowsers );
	Update();
}

void WxActorBrowser::OnFileExportAll( wxCommandEvent& In )		
{
	if( ::MessageBox( (HWND)GetHandle(), *LocalizeUnrealEd("Prompt_18"), *LocalizeUnrealEd("Prompt_19"), MB_YESNO) == IDYES)
	{
		GUnrealEd->Exec( TEXT("CLASS SPEW") );
	}
}

void WxActorBrowser::OnItemExpanding( wxCommandEvent& In )
{
	// In category mode all classes are populated from the start by necessity
	if( !ShowCategoriesCheck->IsChecked() )
	{
		wxTreeEvent* TreeEvent = static_cast<wxTreeEvent*>(&In);
		if( TreeCtrl->GetItemData(TreeEvent->GetItem()) )
		{
			TreeCtrl->DeleteChildren( TreeEvent->GetItem() );
			AddChildren( TreeEvent->GetItem() );
		}
	}
}

void WxActorBrowser::OnSelChanged( wxCommandEvent& In )
{
	wxTreeEvent* TreeEvent = static_cast<wxTreeEvent*>(&In);
	wxTreeItemId SelectedItem  = TreeEvent->GetItem();

	FEditorClassHierarchy* EditorClassHierarchy = GEditor->EditorClassHierarchy;
	check(EditorClassHierarchy);

	if( SelectedItem.IsOk() )
	{	
		WxActorBrowserTreeData* TreeItemData = ((WxActorBrowserTreeData*)(TreeCtrl->GetItemData( SelectedItem )));
		if( TreeItemData )
		{
			INT ClassIndex = TreeItemData->ClassIndex;
			UClass* CurrentClass = EditorClassHierarchy->GetClass(ClassIndex);
			GUnrealEd->SetCurrentClass( CurrentClass );
		}
	}

	UClass* SelectedClass = GEditor->GetSelectedObjects()->GetTop<UClass>();
	if( SelectedClass != NULL )
	{
		FullPathStatic->SetLabel( *SelectedClass->GetPathName() );
	}
}

/** @return		TRUE if it's possible to create an archetype with the specified class. */
static UBOOL CanCreateArchetypeOfClass(const UClass* Class)
{
	const UBOOL bDeprecated = Class->ClassFlags & CLASS_Deprecated;
	const UBOOL bAbstract = Class->ClassFlags & CLASS_Abstract;
	
	const UBOOL bCanCreateArchetype =
		!bDeprecated &&
		!bAbstract && 
		!Class->IsChildOf(UUIRoot::StaticClass());	// no ui stuff

	return bCanCreateArchetype;
}

void WxActorBrowser::OnItemRightClicked( wxCommandEvent& In )
{
	wxTreeEvent* TreeEvent = static_cast<wxTreeEvent*>(&In);
	wxTreeItemId SelectedItem  = TreeEvent->GetItem();

	FEditorClassHierarchy* EditorClassHierarchy = GEditor->EditorClassHierarchy;
	check(EditorClassHierarchy);

	if( SelectedItem.IsOk() )
	{	
		WxActorBrowserTreeData* TreeItemData = ((WxActorBrowserTreeData*)(TreeCtrl->GetItemData( SelectedItem )));
		if( TreeItemData )
		{
			INT ClassIndex = TreeItemData->ClassIndex;
			RightClickedClass = EditorClassHierarchy->GetClass(ClassIndex);

			// Determine if this is a class for which we can create archetypes.
			const UBOOL bCanCreateArchetype = CanCreateArchetypeOfClass( RightClickedClass );

			class WxMBActorBrowserContext : public wxMenu
			{
			public:
				WxMBActorBrowserContext(UBOOL bEnableCreateArchetypeOption)
				{
					if ( bEnableCreateArchetypeOption )
					{
						Append(IDMN_ActorBrowser_CreateArchetype,*LocalizeUnrealEd("Archetype_Create"),TEXT(""));
					}
				};
			} Menu( bCanCreateArchetype);

			FTrackPopupMenu tpm( this, &Menu );
			tpm.Show();
		}
	}
}

/** Creates an archteype from the actor class selected in the Actor Browser. */
void WxActorBrowser::OnCreateArchetype(wxCommandEvent& In )
{
	if( RightClickedClass && CanCreateArchetypeOfClass(RightClickedClass) )
	{
		FString ArchetypeName, PackageName, GroupName;

		if (RightClickedClass->IsChildOf( AActor::StaticClass() ) )
		{
			// actor archetype
			AActor* const Actor =
				GWorld->SpawnActor(
					RightClickedClass,
					NAME_None,
					FVector( 0.0f, 0.0f, 0.0f ),
					FRotator( 0, 0, 0 ),
					NULL,
					TRUE );		// Disallow placement failure due to collisions?
			if( Actor != NULL )
			{
				GUnrealEd->Archetype_CreateFromObject( Actor, ArchetypeName, PackageName, GroupName );
				GWorld->EditorDestroyActor( Actor, FALSE );
			}
		}
		else
		{
			// creating archetype of a UObject
			// ok to leave this dangle, garbage collection will clean it up
			UObject* const Object = UObject::StaticConstructObject(RightClickedClass);
			GUnrealEd->Archetype_CreateFromObject( Object, ArchetypeName, PackageName, GroupName );
		}
		UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
	}
}



void WxActorBrowser::OnUseActorAsParent( wxCommandEvent& In )
{
	RepopulateClassTree();
}

void WxActorBrowser::OnPlaceableClassesOnly( wxCommandEvent& In )
{
	RepopulateClassTree();
}

void WxActorBrowser::OnShowCategories( wxCommandEvent& In )
{
	PlaceableCheck->Enable( !In.IsChecked() );
	RepopulateClassTree();
}

/** Called when the search timer goes off */
void WxActorBrowser::OnSearchTimer( wxTimerEvent& In )
{
	RepopulateClassTree();
}

/** Called whenever the search text is changed; kicks off the search timer */
void WxActorBrowser::OnSearchTextUpdated( wxCommandEvent& In )
{
	// Reset the timer if it's already running
	if ( SearchTimer.IsRunning() )
	{
		SearchTimer.Stop();
	}
	const INT TimerDelay = 500;
	SearchTimer.Start( TimerDelay, wxTIMER_ONE_SHOT );
}

/** Called when the user begins to drag in the actor tree */
void WxActorBrowser::OnBeginDrag( wxCommandEvent& In )
{	
	// Get the currently selected item from the tree control
	wxTreeItemId SelectedItem = TreeCtrl->GetSelection();
	if ( SelectedItem.IsOk() )
	{
		WxActorBrowserTreeData* TreeItemData = ((WxActorBrowserTreeData*)(TreeCtrl->GetItemData( SelectedItem )));

		if( TreeItemData )
		{
			// Get the UClass that was selected.
			INT ClassIndex = TreeItemData->ClassIndex;
			UClass* CurrentClass = GEditor->EditorClassHierarchy->GetClass(ClassIndex);
			if( !CurrentClass->HasAnyClassFlags( CLASS_Abstract ) && CurrentClass->HasAnyClassFlags( CLASS_Placeable ) && CurrentClass->IsChildOf( AActor::StaticClass() ) && !CurrentClass->IsChildOf( ABrush::StaticClass() ) )
			{
				// Only begin drag and drop operations on non-abstract classes that are placeable and inherit from an actor.  
				// We also do not allow volumes to be added via a drag and drop because they require a brush shape that cannot be determined here.

				// Format text data object that drop targets will parse.
				FString DataStr = *FString::Printf( TEXT("%s,%s"), *CurrentClass->GetName(), *CurrentClass->GetPathName() );
				wxTextDataObject Data( *DataStr );
				wxDropSource DropSource(this);
				DropSource.SetData( Data );
				// Begin the drag and drop operation.
				DropSource.DoDragDrop( wxDrag_CopyOnly );
			}
		}
	}
}
/** Called whenever the "clear" button is clicked */
void WxActorBrowser::OnClearSearchTextClicked( wxCommandEvent& In )
{
	if ( SearchTextCtrl->GetValue().Len() > 0 )
	{
		SearchTextCtrl->SetValue( TEXT("") );
	}
}

/** Called whenever the user presses the enter key in the search text box */
void WxActorBrowser::OnSearchTextEnter( wxCommandEvent& In )
{
	// Select all of the text in the box if the user pressed enter
	SearchTextCtrl->SelectAll();
}

/** Called to update the UI for the clear search text button */
void WxActorBrowser::UpdateUI_ClearSearchTextButton( wxCommandEvent& In )
{
	wxUpdateUIEvent& UpdateUIEvent = static_cast<wxUpdateUIEvent&>( In );
	UpdateUIEvent.Enable( SearchTextCtrl->GetValue().Len() > 0 );
}

