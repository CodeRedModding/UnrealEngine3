/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "UnrealEd.h"
#include "DlgReferenceTree.h"
#include "UnObjectTools.h"
#include "AssetSelection.h"

FArchiveGenerateReferenceGraph::FArchiveGenerateReferenceGraph( FReferenceGraph& OutGraph ) 
	: CurrentObject(NULL),
	  ObjectGraph(OutGraph)
{

	ArIsObjectReferenceCollector = TRUE;
	ArIgnoreOuterRef = TRUE;

	// Iterate over each object..
	for( FObjectIterator It; It; ++It )
	{
		UObject* Object	= *It;

		// Skip transient and those about to be deleted
		if( !Object->HasAnyFlags( RF_Transient | RF_PendingKill ) )
		{
			// only serialize non actors objects which have not been visited.
			// actors are skipped because we have don't need them to show the reference tree
			// @todo, may need to serialize them later for full reference graph.
			if( !VisitedObjects.Find( Object ) && !Object->IsA( AActor::StaticClass() ) )
			{
				// Set the current object to the one we are about to serialize
				CurrentObject = Object;
				// This object has been visited.  Any serializations after this should skip this object
				VisitedObjects.Add( Object );
				Object->Serialize( *this );
			}
		}
	}
}

FArchive& FArchiveGenerateReferenceGraph::operator<<( UObject*& Object )
{
	// Only look at objects which are valid
	UBOOL bValidObject = 
		Object &&	// Object should not be NULL
		!Object->HasAnyFlags( RF_Transient | RF_PendingKill ) && // Should not be transient or pending kill
		Object->GetClass() != UClass::StaticClass(); // skip UClasses

	if( bValidObject )
	{
		// Determine if a node for the referenced object has already been created
		FReferenceGraphNode* ReferencedNode = ObjectGraph.FindRef( Object );
		if ( ReferencedNode == NULL )
		{
			// If no node has been created, create one now
			ReferencedNode = ObjectGraph.Set( Object, new FReferenceGraphNode( Object ) );
		}

		// Find a node for the referencer object.  CurrentObject references Object
		FReferenceGraphNode* ReferencerNode = ObjectGraph.FindRef( CurrentObject );
		if( ReferencerNode == NULL )
		{
			// If node node has been created, create one now
			ReferencerNode = ObjectGraph.Set( CurrentObject, new FReferenceGraphNode( CurrentObject ) );
		}

		// Ignore self referencing objects
		if( Object != CurrentObject )
		{
			// Add a new link from the node to what references it.  
			// Links represent references to the object contained in ReferencedNode
			ReferencedNode->Links.Add( ReferencerNode );
		}
		
		if( !VisitedObjects.Find( Object ) && !Object->IsA( AActor::StaticClass() ) )
		{
			// If this object hasnt been visited and is not an actor, serialize it

			// Store the current object for when we return from serialization 
			UObject* PrevObject = CurrentObject;
			// Set the new current object
			CurrentObject = Object;
			// This object has now been visited
			VisitedObjects.Add( Object );
			// Serialize
			Object->Serialize( *this );
			// Restore the current object
			CurrentObject = PrevObject;
		}
	}

	return *this;
}

/**
 * A Context menu class for the reference tree
 */
class WxReferenceTreeContextMenu : public wxMenu
{
public:
	WxReferenceTreeContextMenu( UObject* ObjectForMenu )
	{
		if( ObjectForMenu->IsA( AActor::StaticClass() ) )
		{
			// The object is an actor, generate actor specific menu items
			Append( ID_REFERENCE_TREE_CONTEXT_MENU_SELECT, *LocalizeUnrealEd( "ReferenceTreeDialog_SelectActor" ), TEXT("") );
			Append( ID_REFERENCE_TREE_CONTEXT_MENU_VIEW_PROPERTIES, *LocalizeUnrealEd( "ReferenceTreeDialog_ViewProps" ), TEXT("") );
		}
		else
		{
			// The object is not an actor, and is a browsable object in the content browser, generate specific menu items for it.
			Append( ID_REFERENCE_TREE_CONTEXT_MENU_SELECT, *LocalizeUnrealEd( "ReferenceTreeDialog_ShowInCB" ), TEXT("") );
			Append( ID_REFERENCE_TREE_CONTEXT_MENU_OPEN_EDITOR, *LocalizeUnrealEd( "ReferenceTreeDialog_OpenEditor" ), TEXT("") );
		}
	}
};

/**
 * A menu bar for the reference tree dialog
 */
class WxReferenceTreeDialogMenuBar : public wxMenuBar
{
public:
	WxReferenceTreeDialogMenuBar()
	{
		// View  menu
		wxMenu* ViewMenu = new wxMenu();
		ViewMenu->Append( ID_REFERENCE_TREE_MAIN_MENU_REBUILD, *LocalizeUnrealEd("ReferenceTreeDialog_RebuildTree"), TEXT("") );
		ViewMenu->Append( ID_REFERENCE_TREE_MAIN_MENU_EXPAND_ALL, *LocalizeUnrealEd("ReferenceTreeDialog_ExpandAll"), TEXT("") );
		ViewMenu->Append( ID_REFERENCE_TREE_MAIN_MENU_COLLAPSE_ALL, *LocalizeUnrealEd("ReferenceTreeDialog_CollapseAll"), TEXT("") );
		Append( ViewMenu, *LocalizeUnrealEd( "ReferenceTreeDialog_ViewMenu" ) );

		// Options menu
		wxMenu* OptionsMenu = new wxMenu();
		OptionsMenu->Append( ID_REFERENCE_TREE_MAIN_MENU_SHOW_SCRIPT, *LocalizeUnrealEd( "ReferenceTreeDialog_ShowScriptRefs" ) );
		Append( OptionsMenu, *LocalizeUnrealEd( "ReferenceTreeDialog_OptionsMenu" ) );
	
	}
};

/** 
 * A drop target class for the reference tree dialog
 * Enables content browser objects to be dropped from the content browser to the dialog to show their ref tree.
 */
class WxReferenceTreeDialogDropTarget : public wxTextDropTarget
{
public:
	WxReferenceTreeDialogDropTarget( WxReferenceTreeDialog* InOwner )
		: OwnerDialog( InOwner )
	{

	}

	/** Called when text is dropped onto the dialog */
	virtual bool OnDropText( wxCoord X, wxCoord Y, const wxString& Data )
	{
		// Get the dropped text
		FString DroppedString = Data.c_str();

		const TCHAR AssetDelimiter[] = { AssetMarshalDefs::AssetDelimiter, TEXT('\0') };

		bool bSucceeded = false;

		// Find the UObject represented by the text which was dropped.
		TArray<FString> DroppedAssetStrings;
		DroppedString.ParseIntoArray(&DroppedAssetStrings, AssetDelimiter, TRUE);
		if( DroppedAssetStrings.Num() > 0 )
		{
			// Only take the first object since the dialog currently shows
			// one object at a time.
			FSelectedAssetInfo Info( DroppedAssetStrings(0) );

			if( Info.Object )
			{
				// Dropped text was valid, repopulate the tree
				OwnerDialog->PopulateTree( Info.Object );
				bSucceeded = true;
			}
		}

		return bSucceeded;
	}

private:
	/** Pointer to the dialog which owns this drop target. */
	WxReferenceTreeDialog* OwnerDialog;
};

WxReferenceTreeDialog* WxReferenceTreeDialog::Instance;

WxReferenceTreeDialog::WxReferenceTreeDialog( const TMap<UClass*, TArray<UGenericBrowserType*> >& InBrowsableTypes )
	: wxFrame( GApp->EditorFrame, wxID_ANY, *LocalizeUnrealEd( "ReferenceTreeDialog_Title" ), wxDefaultPosition, wxDefaultSize, wxFRAME_FLOAT_ON_PARENT|wxDEFAULT_FRAME_STYLE & ~( wxRESIZE_BOX | wxMAXIMIZE_BOX ) ),
	  BrowsableObjectTypes( InBrowsableTypes ),
	  bShowScriptRefs( FALSE )
{
	// Create tree panel
	wxPanel* TreePanel = new wxPanel(this);
	
	// Add a menu bar
	SetMenuBar( new WxReferenceTreeDialogMenuBar );

	// Create tree sizer and reference tree
	wxBoxSizer* TreeSizer = new wxBoxSizer(wxHORIZONTAL);
	TreePanel->SetSizer(TreeSizer);

	ReferenceTree = new WxTreeCtrl;
	ReferenceTree->Create( TreePanel, ID_REFERENCE_TREE, NULL, wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_ROW_LINES | wxTR_SINGLE );

	TreeSizer->Add( ReferenceTree, 1, wxEXPAND|wxALL, 5 );
	TreeSizer->SetMinSize( wxSize(300,400) );

	// Create main sizer for the dialog
	wxBoxSizer* DialogSizer = new wxBoxSizer(wxVERTICAL);
	DialogSizer->Add( TreePanel, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT, 0 );

	SetSizerAndFit( DialogSizer );
	Center();

	// We want to recieve callbacks when a map change happens.  The reference tree becomes invalid when map changes occur
	GCallbackEvent->Register( CALLBACK_MapChange, this );
	
	// Enable dropping onto the reference tree.
	ReferenceTree->SetDropTarget( new WxReferenceTreeDialogDropTarget(this) );
}


WxReferenceTreeDialog::~WxReferenceTreeDialog()
{
	// Destropy graph and dialog
	DestroyGraphAndTree();
	Destroy();
	// Unregister all callbacks
	GCallbackEvent->UnregisterAll( this );
}

/** FCallbackEventDevice interface */
void WxReferenceTreeDialog::Send( ECallbackEventType InType, DWORD Flag )
{
	if( InType == CALLBACK_MapChange && MapChangeEventFlags::WorldTornDown)
	{
		// If a map is changing and the world was torn down, destroy the graph
		DestroyGraphAndTree();
	}
}

/** Refreshes the tree control for the current root object. */
void WxReferenceTreeDialog::RefreshTree()
{
	if( ReferenceTree->GetCount() > 0 )
	{
		// Get the root item so we know what what object we show show references to
		wxTreeItemId RootId = ReferenceTree->GetRootItem();

		// Get the root node.  It must exist if there are items in the tree.
		WxReferenceTreeNode* Node = (WxReferenceTreeNode*)ReferenceTree->GetItemData( RootId );
		// Root node must exist.
		check( Node && Node->Object );

		// Repopulate the tree
		PopulateTree( Node->Object );
	}
}

/** Destroys the graph and tree */
void WxReferenceTreeDialog::DestroyGraphAndTree()
{
	// Remove all items from the tree
	ReferenceTree->DeleteAllItems();

	// Delete every node in the graph.
	for( FReferenceGraph::TIterator It(ReferenceGraph); It; ++It )
	{
		delete It.Value();
		It.Value() = NULL;
	}

	// Empty graph
	ReferenceGraph.Empty();
}

/** Populates the tree, for a specific root object */
void WxReferenceTreeDialog::PopulateTree( UObject* RootObject )
{
	if( RootObject == NULL )
		return;

	if( ReferenceGraph.Num() == 0 )
	{
		// If the reference graph has no nodes in it, generate the tree now
		FArchiveGenerateReferenceGraph GenerateReferenceGraph( ReferenceGraph );
	}

	
	// Delete all items in the tree control
	ReferenceTree->DeleteAllItems();
	
	// Add a root node tree item
	WxReferenceTreeNode* RootNode = new WxReferenceTreeNode( RootObject );
	const wxTreeItemId RootItemId = ReferenceTree->AddRoot( *RootNode->GetNodeText(), -1, -1, RootNode );

	const FReferenceGraphNode* RootGraphNode = ReferenceGraph.FindRef( RootObject );
	
	UBOOL bIsReferenced = FALSE;
	if( RootGraphNode )
	{
		// For each node that references the root node, recurse over its links to generate the tree.
		for( TSet<FReferenceGraphNode*>::TConstIterator It(RootGraphNode->Links); It; ++It )
		{
			FReferenceGraphNode* Link = *It;
			UObject* Reference = Link->Object;
			if( ( bShowScriptRefs || !Reference->HasAnyFlags( RF_ClassDefaultObject ) ) && ( Reference->IsA( UActorComponent::StaticClass() ) || ObjectTools::IsObjectBrowsable( Reference, &BrowsableObjectTypes ) ) && !Reference->HasAnyFlags( RF_Transient ) )
			{
				bIsReferenced = TRUE;
				// Skip default objects unless we are showing script references and transient objects.
				// Populate links to browsable objects and actor components (we will actually display the actor or script reference for components)
				PopulateTreeRecursive( *Link, RootItemId );
			}
		}
	}

	if( !bIsReferenced )
	{
		ReferenceTree->AppendItem( RootItemId, *LocalizeUnrealEd( "ReferenceTreeDialog_NoRefs" ), -1, -1, NULL );
	}

	// Expand all tree nodes and ensure the root item is visible
	ReferenceTree->ExpandAll();
	ReferenceTree->EnsureVisible( RootItemId );
}

/** Helper function for recursively generating the reference tree */ 
UBOOL WxReferenceTreeDialog::PopulateTreeRecursive( FReferenceGraphNode& Node, wxTreeItemId ParentId )
{
	// Prevent circular references.  This node has now been visited for this path.
	Node.bVisited = TRUE;

	UBOOL bNodesWereAdded = FALSE;

	UObject* ObjectToDisplay = Node.GetObjectToDisplay( bShowScriptRefs );

	if( ObjectToDisplay )
	{
		// Make a tree node for this object. If the object is a component, display the components outer instead.
		WxReferenceTreeNode* TreeNode = new WxReferenceTreeNode( ObjectToDisplay );
		// Add the new node to our tree.
		const wxTreeItemId TreeItemId = ReferenceTree->AppendItem( ParentId, *TreeNode->GetNodeText(), -1, -1, TreeNode );

		// We just added a node. Inform the parent.
		bNodesWereAdded = TRUE;

		UINT NumChildrenAdded = 0;
		// @todo: Move to INI or menu option?
		const UINT MaxChildrenPerNodeToDisplay = 50;

		// Iterate over all this nodes links and add them to the tree
		for( TSet<FReferenceGraphNode*>::TConstIterator It(Node.Links); It; ++It )
		{
			if( NumChildrenAdded == MaxChildrenPerNodeToDisplay )
			{
				// The tree is getting too large to be usable
				// We will display a node saying how many other nodes there are that cant be displayed
				// @todo: provide the ability to expand this node and populate the tree with the skipped nodes.
				ReferenceTree->AppendItem( TreeItemId, *FString::Printf( TEXT("%d others..."), Node.Links.Num() - NumChildrenAdded ), -1, -1, NULL );
				// stop populating
				break;
			}

			FReferenceGraphNode* Link = *It;
			UObject* Object = Link->Object;

			// Only recurse into unvisited nodes which are components or are visible in the content browser.  
			// Components are acceptable so their actor references can be added to the tree.
			UBOOL bObjectIsValid = !Object->HasAnyFlags( RF_Transient ) &&
								   ( Object->IsA( UActorComponent::StaticClass() ) || // Allow actor components to pass so that their actors can be displayed
									 Object->IsA( UPolys::StaticClass() ) || // Allow polys to pass so BSP can be displayed
									 ObjectTools::IsObjectBrowsable( Object, &BrowsableObjectTypes ) ); // Allow all browsable objects through

			if( Link->bVisited == FALSE && bObjectIsValid )
			{
				if( PopulateTreeRecursive( *Link, TreeItemId ) )
				{
					++NumChildrenAdded;
				}
			}
		}
	}

	// We can safely visit this node again, all of its links have been visited.
	// Any other way this node is visited represents a new path.
	Node.bVisited = FALSE;

	return bNodesWereAdded;
}

/** Shows the passed in object in the content browser (if browsable) or the level (if actor) */
void WxReferenceTreeDialog::SelectObjectInEditor( UObject* ObjectToSelect )
{
	AActor* Actor = Cast<AActor>( ObjectToSelect );
	if( Actor ) 
	{
		// Do not attempt to select script based objects
		if( !Actor->HasAnyFlags(RF_ClassDefaultObject) )
		{
			// Select and focus in on the actor
			GEditor->SelectNone( FALSE, TRUE );
			GEditor->SelectActor( Actor, TRUE, NULL, TRUE, TRUE );
			GEditor->MoveViewportCamerasToActor( *Actor, TRUE );
		}
	}
	else
	{
		// Show the object in the content browser.
		GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_SyncAssetView, ObjectToSelect ) );
	}
}

/** Asserts if called, this is not a modal dialog */
INT WxReferenceTreeDialog::ShowModal()
{
	checkf(0, TEXT("WxReferenceTreeDialog is a modeless dialog") );
	return 0;
}

/** Called when the dialog is closed. */
void WxReferenceTreeDialog::OnClose( wxCloseEvent& In )
{
	// Destroy the graph, dialog, and instance.
	DestroyGraphAndTree();
	Destroy();
	Instance = NULL;
}

/** Called when a tree item is double clicked. */
void WxReferenceTreeDialog::OnTreeItemDoubleClicked( wxTreeEvent& In )
{
	// Show the object in the editor.  I.E show the object in the level if its an actor, or content browser otherwise.
	WxReferenceTreeNode* Node = (WxReferenceTreeNode*)ReferenceTree->GetItemData( In.GetItem() );
	if( Node && Node->Object )
	{
		SelectObjectInEditor( Node->Object );
	}
}

/** Called when a tooltip needs to be displayed for a tree item. */
void WxReferenceTreeDialog::OnTreeItemGetToolTip( wxTreeEvent& In )
{
	WxReferenceTreeNode* Node = (WxReferenceTreeNode*)ReferenceTree->GetItemData( In.GetItem() );
	if( Node && Node->Object )
	{
		// The tooltip is the full name of the object
		In.SetToolTip( *Node->Object->GetFullName() );
	}
}

/** Called when a context menu needs to be displayed for a tree item. */
void WxReferenceTreeDialog::OnTreeItemShowContextMenu( wxTreeEvent& In )
{
	WxReferenceTreeNode* Node = (WxReferenceTreeNode*)ReferenceTree->GetItemData( In.GetItem() );

	// Do not show context menus for script references
	if( Node && Node->Object && !Node->Object->HasAnyFlags( RF_ClassDefaultObject ) )
	{
		WxReferenceTreeContextMenu ContextMenu( Node->Object );

		FTrackPopupMenu tpm( this, &ContextMenu );
		tpm.Show();
	}
}

/** Called when the select object menu option is chosen. */
void WxReferenceTreeDialog::OnMenuSelectObject( wxCommandEvent& In )
{
	// Should only be one selected item for now
	wxArrayTreeItemIds SelectedIds;
	ReferenceTree->GetSelections( SelectedIds );
	check( SelectedIds.GetCount() == 1 );

	WxReferenceTreeNode* Node = (WxReferenceTreeNode*)ReferenceTree->GetItemData( SelectedIds[0] );
	if( Node && Node->Object )
	{
		// Show the object in the editor
		SelectObjectInEditor( Node->Object );
	}
}

/** Called when the view properties menu option is chosen. */
void WxReferenceTreeDialog::OnMenuViewProperties( wxCommandEvent& In )
{
	wxArrayTreeItemIds SelectedIds;
	ReferenceTree->GetSelections( SelectedIds );
	check( SelectedIds.GetCount() == 1 );

	WxReferenceTreeNode* Node = (WxReferenceTreeNode*)ReferenceTree->GetItemData( SelectedIds[0] );
	if( Node && Node->Object )
	{
		// Show the property windows and create one if necessary
		GUnrealEd->ShowActorProperties();

		// Show the property window for the actor
		TArray<UObject*> Objects;
		Objects.AddItem( Node->Object );
		GUnrealEd->UpdatePropertyWindowFromActorList( Objects );
	}
}

/** Called when the show object in editor menu option is chosen. */
void WxReferenceTreeDialog::OnMenuShowEditor( wxCommandEvent& In )
{
	wxArrayTreeItemIds SelectedIds;
	ReferenceTree->GetSelections( SelectedIds );
	check( SelectedIds.GetCount() == 1 );

	WxReferenceTreeNode* Node = (WxReferenceTreeNode*)ReferenceTree->GetItemData( SelectedIds[0] );
	if( Node && Node->Object )
	{
		// Show the editor for this object
		GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_FocusBrowser | CBR_ActivateObject, Node->Object ) );
	}
}

/** Called when the rebuild menu option is chosen. */
void WxReferenceTreeDialog::OnMenuRebuild( wxCommandEvent& In )
{
	if( ReferenceTree->GetCount() > 0 )
	{
		wxTreeItemId RootId = ReferenceTree->GetRootItem();

		// Get the root node.  It must exist if there are items in the tree.
		WxReferenceTreeNode* Node = (WxReferenceTreeNode*)ReferenceTree->GetItemData( RootId );
		check( Node && Node->Object );

		UObject* RootObject = Node->Object;
	
		DestroyGraphAndTree();
		PopulateTree( RootObject );
	}
}

/** Called when the show script references menu option is chosen. */
void WxReferenceTreeDialog::OnMenuToggleScript( wxCommandEvent& In )
{
	bShowScriptRefs = !bShowScriptRefs;
	RefreshTree();
}

/** Called when the Expand All menu option is chosen. */
void WxReferenceTreeDialog::ExpandAll( wxCommandEvent& In )
{
	ReferenceTree->ExpandAll();
}

/** Called when the Collapse All menu option is chosen. */
void WxReferenceTreeDialog::CollapseAll( wxCommandEvent& In )
{
	ReferenceTree->CollapseAll();
}

/** Called when a UI update is needed. */
void WxReferenceTreeDialog::ShowScriptUpdateUI( wxUpdateUIEvent& In )
{
	if( bShowScriptRefs )
	{
		In.Check( TRUE );
	}
	else
	{
		In.Check( FALSE );
	}
}

BEGIN_EVENT_TABLE( WxReferenceTreeDialog, wxFrame )
	EVT_CLOSE( WxReferenceTreeDialog::OnClose )
	EVT_TREE_ITEM_ACTIVATED( ID_REFERENCE_TREE, WxReferenceTreeDialog::OnTreeItemDoubleClicked )
	EVT_TREE_ITEM_GETTOOLTIP( ID_REFERENCE_TREE, WxReferenceTreeDialog::OnTreeItemGetToolTip )
	EVT_TREE_ITEM_MENU( ID_REFERENCE_TREE, WxReferenceTreeDialog::OnTreeItemShowContextMenu )
	EVT_MENU( ID_REFERENCE_TREE_CONTEXT_MENU_SELECT, WxReferenceTreeDialog::OnMenuSelectObject )
	EVT_MENU( ID_REFERENCE_TREE_CONTEXT_MENU_VIEW_PROPERTIES, WxReferenceTreeDialog::OnMenuViewProperties )
	EVT_MENU( ID_REFERENCE_TREE_CONTEXT_MENU_OPEN_EDITOR, WxReferenceTreeDialog::OnMenuShowEditor )
	EVT_MENU( ID_REFERENCE_TREE_MAIN_MENU_REBUILD, WxReferenceTreeDialog::OnMenuRebuild )
	EVT_MENU( ID_REFERENCE_TREE_MAIN_MENU_SHOW_SCRIPT, WxReferenceTreeDialog::OnMenuToggleScript )
	EVT_MENU( ID_REFERENCE_TREE_MAIN_MENU_EXPAND_ALL, WxReferenceTreeDialog::ExpandAll )
	EVT_MENU( ID_REFERENCE_TREE_MAIN_MENU_COLLAPSE_ALL, WxReferenceTreeDialog::CollapseAll )
	EVT_UPDATE_UI( ID_REFERENCE_TREE_MAIN_MENU_SHOW_SCRIPT, WxReferenceTreeDialog::ShowScriptUpdateUI )
END_EVENT_TABLE()
