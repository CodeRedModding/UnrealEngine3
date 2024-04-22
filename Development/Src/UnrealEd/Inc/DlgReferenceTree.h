/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGREFERENCETREE_H__
#define __DLGREFERENCETREE_H__

/**
 * A struct representing a node in the reference graph
 */
struct FReferenceGraphNode 
{
	/** The object this node represents */
	UObject* Object;
	/** Links from this nodes object.  Each link represents a reference to the object. */
	TSet<FReferenceGraphNode*> Links;
	/** If the node has been visited while populating the reference tree.  This prevents circular references. */
	UBOOL bVisited;

	FReferenceGraphNode( UObject* InObject )
		: Object( InObject ),
		  bVisited(FALSE)
	{
	}

	/** 
	 * Returns the object that should be displayed on the graph
	 *
	 * @param bShowScriptReferences		If true we should return script objects.
	 */
	UObject* GetObjectToDisplay( UBOOL bShowScriptReferences )
	{
		UObject* ObjectToDisplay = NULL;
		// Check to see if the object in this node is a component.  If it is try to display the actor that uses it.
		UActorComponent* Comp = Cast<UActorComponent>( Object );
		if( Comp )
		{
			if( Comp->GetOwner() )
			{
				// Use the components owner if it has one.
				ObjectToDisplay = Comp->GetOwner();
			}
			else if( Comp->GetOuter() && Comp->GetOuter()->IsA( AActor::StaticClass() ) )
			{
				// Use the components outer if it is an actor
				ObjectToDisplay = Comp->GetOuter();
			}
		}
		else if( Object->IsA( UPolys::StaticClass() ) )
		{
			// Special case handling for bsp.
			// Outer chain: Polys->UModel->ABrush
			UObject* PossibleModel = Object->GetOuter();
			if( PossibleModel && PossibleModel->IsA( UModel::StaticClass() ))
			{
				UObject* PossibleBrush = PossibleModel->GetOuter();

				if(PossibleBrush && PossibleBrush->IsA( ABrush::StaticClass() ) )
				{
					ObjectToDisplay = PossibleBrush;
				}
			}
		}
		else
		{
			ObjectToDisplay = Object;
		}

		if( ObjectToDisplay && ObjectToDisplay->HasAnyFlags( RF_ClassDefaultObject ) && !bShowScriptReferences )
		{
			// Don't return class default objects if we aren't showing script references
			ObjectToDisplay = NULL;
		}

		return ObjectToDisplay;
	}
};

typedef TMap<UObject*,FReferenceGraphNode*> FReferenceGraph;

/**
 * An archive for creating a reference graph of all UObjects
 */
class FArchiveGenerateReferenceGraph : public FArchive
{
public:
	FArchiveGenerateReferenceGraph( FReferenceGraph& OutGraph );
	FArchive& operator<<( UObject*& Object );
private:
	/** The object currently being serialized. */
	UObject* CurrentObject;
	/** The set of visited objects so we dont serialize something twice. */
	TSet< UObject* > VisitedObjects;
	/** Reference to the graph we are creating. */
	FReferenceGraph& ObjectGraph;
};

/**
 * Represents each node in a wxTreeCtrl
 */
class WxReferenceTreeNode : public wxTreeItemData, public FSerializableObject
{
public:
	/** The object this node represents. */
	UObject* Object;

	WxReferenceTreeNode( UObject* InObject )
		: Object(InObject)
	{

	}

	/** Returns the text to display for this node in the tree control. */
	FString GetNodeText() const
	{			
		return FString::Printf( TEXT("%s(%s)"), *Object->GetClass()->GetName(), *Object->GetName() );
	}

	/** FSerializableObject interface */
	void Serialize( FArchive& Ar )
	{
		if( !Ar.IsSaving() && !Ar.IsLoading() )
		{
			Ar << Object;
		}
	}
};

/** 
 * A dialog for displaying the reference tree of a specific object.
 */
class WxReferenceTreeDialog : public wxFrame, public FCallbackEventDevice
{
public:
	~WxReferenceTreeDialog();

	/** Refreshes the tree control for the current root object. */
	void RefreshTree();

	/** Populates the tree, for a specific root object */
	void PopulateTree( UObject* RootObject );

	/** 
	 * Shows the reference tree dialog, creating it and generating the graph if it doesnt exist. 
	 *
	 * @param InObjectToShow	The object to to gather references to (the root of the tree).
	 * @param InBrowsableTypes	The browsable object type map.  The tree only shows browsable types or actors so we need this to check if a type is browsable.		
	 */
	static void ShowReferenceTree( UObject* InObjectToShow, const TMap<UClass*, TArray<UGenericBrowserType*> >& InBrowsableTypes )
	{
		if( !Instance )
		{
			// There is no instance of the dialog yet so create it.
			Instance = new WxReferenceTreeDialog( InBrowsableTypes );
		}
		
		// Populate the tree and show the dialog
		Instance->PopulateTree( InObjectToShow );
		Instance->Show();
		// Make sure the dialog has focus and is the top most window
		Instance->Raise();
	}

private:
	/** The reference graph for all UObjects. */
	FReferenceGraph ReferenceGraph;
	/** Singleton instance since this dialog is modeless.  We do not create more than one.*/
	static WxReferenceTreeDialog* Instance;
	/** TreeCtrl pointer */
	WxTreeCtrl* ReferenceTree;
	/** A map of all classes to their browser type.  For detecting browsable objects. */
	const TMap< UClass*, TArray< UGenericBrowserType* > >& BrowsableObjectTypes;
	/** If the tree should show script references. */
	UBOOL bShowScriptRefs;
private:
	/** FCallbackEventDevice */
	virtual void Send( ECallbackEventType InType, DWORD Flag );

	/** Asserts if called, this is not a modal dialog */
	virtual INT ShowModal();

	/** Destroys the graph and tree */
	void DestroyGraphAndTree();

	/** Helper function for recursively generating the reference tree */ 
	UBOOL PopulateTreeRecursive( FReferenceGraphNode& Node, wxTreeItemId ParentId );

	/** Shows the passed in object in the content browser (if browsable) or the level (if actor) */
	void SelectObjectInEditor( UObject* ObjectToSelect );

	/** Called when the dialog is closed. */
	void OnClose( wxCloseEvent& In );

	/** Called when a tree item is double clicked. */
	void OnTreeItemDoubleClicked( wxTreeEvent& In );

	/** Called when a tooltip needs to be displayed for a tree item. */
	void OnTreeItemGetToolTip( wxTreeEvent& In );

	/** Called when a context menu needs to be displayed for a tree item. */
	void OnTreeItemShowContextMenu( wxTreeEvent& In );

	/** Called when the select object menu option is chosen. */
	void OnMenuSelectObject( wxCommandEvent& In );

	/** Called when the view properties menu option is chosen. */
	void OnMenuViewProperties( wxCommandEvent& In );

	/** Called when the show object in editor menu option is chosen. */
	void OnMenuShowEditor( wxCommandEvent& In );

	/** Called when the refresh menu option is chosen. */
	void OnMenuRebuild( wxCommandEvent& In );

	/** Called when the show script references menu option is chosen. */
	void OnMenuToggleScript( wxCommandEvent& In );

	/** Called when the Expand All menu option is chosen. */
	void ExpandAll( wxCommandEvent& In );

	/** Called when the Collapse All menu option is chosen. */
	void CollapseAll( wxCommandEvent& In );

	/** Called when a UI update is needed. */
	void ShowScriptUpdateUI( wxUpdateUIEvent& In );

	/** Constructor.  private so instances cannot be created. Use static interface instead. */
	WxReferenceTreeDialog( const TMap<UClass*, TArray<UGenericBrowserType*> >& InBrowsableTypes );

	DECLARE_EVENT_TABLE()

};

#endif