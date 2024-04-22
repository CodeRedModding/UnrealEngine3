/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineAnimClasses.h"
#include "ScopedTransaction.h"
#include "UnLinkedObjEditor.h"
#include "UnLinkedObjDrawUtils.h"
#include "AttachmentEditor.h"

BEGIN_EVENT_TABLE(WxAttachmentEditor,WxBrowser)
	EVT_SIZE(WxAttachmentEditor::OnSize)
	EVT_BUTTON( IDM_ATTACHEDIT_ADDACTOR, WxAttachmentEditor::OnAddSelectedActor)
	EVT_BUTTON( IDM_ATTACHEDIT_CLEAR, WxAttachmentEditor::OnClearActors)
	EVT_BUTTON( IDM_ATTACHEDIT_ATTACH, WxAttachmentEditor::OnAttachActors)
	EVT_BUTTON( IDM_ATTACHEDIT_REFRESH, WxAttachmentEditor::OnRefreshGraph)
	EVT_BUTTON( IDM_ATTACHEDIT_AUTOARRANGE, WxAttachmentEditor::OnAutoArrange)
	EVT_MENU( IDM_ATTACHEDIT_BREAKTOPARENT, WxAttachmentEditor::OnBreakParent)
	EVT_MENU( IDM_ATTACHEDIT_REMOVEGRAPH, WxAttachmentEditor::OnRemoveGraph)
	EVT_MENU( IDM_ATTACHEDIT_ADDACTOR, WxAttachmentEditor::OnAddSelectedActor)
	EVT_MENU( IDM_ATTACHEDIT_CLEAR, WxAttachmentEditor::OnClearActors)
	EVT_MENU( IDM_ATTACHEDIT_ATTACH, WxAttachmentEditor::OnAttachActors)
	EVT_MENU( IDM_ATTACHEDIT_REFRESH, WxAttachmentEditor::OnRefreshGraph)
	EVT_MENU( IDM_ATTACHEDIT_BREAKALLANDREMOVE, WxAttachmentEditor::OnDetachAllSelectedAndRemove)
	EVT_MENU( IDM_ATTACHEDIT_SELECTDOWNSTREAMNODES, WxAttachmentEditor::OnSelectDownsteamNodes)
	EVT_MENU( IDM_ATTACHEDIT_SELECTUPSTREAMNODES, WxAttachmentEditor::OnSelectUpsteamNodes)
END_EVENT_TABLE()

/** Simple class to act as a context menu when the user right-clicks on the attachment editor's background */
class WxAttachEdBackgroundOptions : public wxMenu
{
public:
	/** Construct a WxAttachEdBackgroundOptions object */
	WxAttachEdBackgroundOptions( WxAttachmentEditor* AttachEd )
	{
		// If the user has at least one actor selected in the editor, present the option to add the selected actors to
		// the attachment editor
		if ( GEditor->GetSelectedActorCount() > 0 )
		{
			Append( IDM_ATTACHEDIT_ADDACTOR, *LocalizeUnrealEd("AttachEdAddActor") );
		}

		// If the attachment editor has any drawing info for any actors, present the option to clear them away
		if ( AttachEd->DrawInfos.Num() > 0 )
		{
			Append( IDM_ATTACHEDIT_CLEAR, *LocalizeUnrealEd("AttachEdClear") );
		}

		// If there is more than one actor selected in the attachment editor, present the option to attach them together
		// (NOTE: This is basically the same as checking GEditor->GetSelectedActorCount() for now, but if the AttachEd implementation
		// changes, this is the appropriate function that should be called)
		if ( AttachEd->GetNumSelected() > 1 )
		{
			Append( IDM_ATTACHEDIT_ATTACH, *LocalizeUnrealEd("AttachEdAttach") );
		}

		// Present the option to refresh the attachment editor view
		Append( IDM_ATTACHEDIT_REFRESH, *LocalizeUnrealEd("AttachEdRefresh") );
	}
};

/** 
 * A small menu class which generates a menu of bone and socket names from a skeletal mesh and 
 * presents that menu to a user 
 */
class WxAttachEdSkeletalMeshAttachOptions : public wxMenu
{
public:
	WxAttachEdSkeletalMeshAttachOptions( const ASkeletalMeshActor* InSkelMesh ) : SelectedName( NAME_None )
	{
		const TArray<USkeletalMeshSocket*>& Sockets = InSkelMesh->SkeletalMeshComponent->SkeletalMesh->Sockets;
		const TArray<FMeshBone>& RefSkeleton = InSkelMesh->SkeletalMeshComponent->SkeletalMesh->RefSkeleton;

		// The start menu ID
		UINT MenuId = IDM_ATTACHEDIT_SKELMESH_MENU_START;

		// We already know how many menu items there will be
		MenuNames.Reserve( Sockets.Num() + RefSkeleton.Num() + 1 );

		// Attach to actor should be the first menu item.  
		// This menu item lets the user attach directly to the skel mesh instead of a bone or socket
		Append( MenuId, *LocalizeUnrealEd("AttachEdAttachToActor") );
		// We will identify "attach to actor" by a null name value
		MenuNames.AddItem( NAME_None );

		// Populate the socket menu
		wxMenu* SocketMenu = new wxMenu();
		for( INT SocketIdx = 0; SocketIdx < Sockets.Num(); ++SocketIdx )
		{
			SocketMenu->Append( ++MenuId, *Sockets( SocketIdx )->SocketName.ToString() ); 
			MenuNames.AddItem( Sockets( SocketIdx )->SocketName );
		}
		// Attach the socket menu to the main menu
		Append( wxID_ANY, *LocalizeUnrealEd("AttachEdSkelMeshMenuSockets"), SocketMenu );

		// Populate the bone menu 
		wxMenu* BoneMenu = new wxMenu();
		for( INT BoneIdx = 0; BoneIdx < RefSkeleton.Num(); ++BoneIdx )
		{
			BoneMenu->Append( ++MenuId, *RefSkeleton( BoneIdx ).Name.ToString() );
			MenuNames.AddItem( RefSkeleton( BoneIdx ).Name );
		}
		// Attach the bone menu to the main menu
		Append( wxID_ANY, *LocalizeUnrealEd("AttachEdSkelMeshMenuBones"), BoneMenu );
	}

	// Returns the bone/socket name the user selected from the menu
	const FName& GetSelectedName() const { return SelectedName; }
private:
	/** Stores a list of bone/socket names so we can look them up by their menu id */
	TArray<FName> MenuNames;
	/** Stores the user selected bone/socket name */
	FName SelectedName;

	/** Called when a menu item is selected */
	void OnSelectMenuItem( wxCommandEvent& In )
	{
		UINT StringIndex = In.GetId() - IDM_ATTACHEDIT_SKELMESH_MENU_START;
		SelectedName = MenuNames( StringIndex );
	}

	DECLARE_EVENT_TABLE();
};

BEGIN_EVENT_TABLE( WxAttachEdSkeletalMeshAttachOptions, wxMenu )
	EVT_MENU_RANGE( IDM_ATTACHEDIT_SKELMESH_MENU_START, IDM_ATTACHEDIT_SKELMESH_MENU_END, WxAttachEdSkeletalMeshAttachOptions::OnSelectMenuItem )
END_EVENT_TABLE()

class WxAttachEdNodeOptions : public wxMenu
{
public:
	WxAttachEdNodeOptions( WxAttachmentEditor* AttachEd )
	{
		Append( IDM_ATTACHEDIT_BREAKTOPARENT, *LocalizeUnrealEd("AttachEdBreakParent"), TEXT("") );
		Append( IDM_ATTACHEDIT_REMOVEGRAPH, *LocalizeUnrealEd("AttachEdRemoveGraph"), TEXT("") );
		Append( IDM_ATTACHEDIT_BREAKALLANDREMOVE, *LocalizeUnrealEd("AttachEdBreakAndRemove"), TEXT("") );
		AppendSeparator();
		Append( IDM_ATTACHEDIT_SELECTDOWNSTREAMNODES, *LocalizeUnrealEd("SelectDownstream"), TEXT("") );
		Append( IDM_ATTACHEDIT_SELECTUPSTREAMNODES, *LocalizeUnrealEd("SelectUpstream"), TEXT("") );
	}

	~WxAttachEdNodeOptions()
	{

	}
};

/*
 * The menu for the attachment editor
 */
class WxMBAttachmentEditor : public wxMenuBar
{
public:
	WxMBAttachmentEditor()
	{
		WxBrowser::AddDockingMenu( this );
	}
};

WxAttachmentEditor::WxAttachmentEditor()
{
	GraphWindow = NULL;
	LinkedObjVC = NULL;
	BigPanel = NULL;
	BackgroundTexture = NULL;
	ToolBar = NULL;

	GCallbackEvent->Register(CALLBACK_SelChange, this);
	GCallbackEvent->Register(CALLBACK_Undo, this);
	GCallbackEvent->Register(CALLBACK_ActorPropertiesChange, this);
	GCallbackEvent->Register(CALLBACK_MapChange, this);

}

WxAttachmentEditor::~WxAttachmentEditor()
{

}

void WxAttachmentEditor::Create(INT DockID, const TCHAR* FriendlyName, wxWindow* Parent)
{
	// Let our base class start up the windows
	WxBrowser::Create(DockID, FriendlyName, Parent);	

	BackgroundTexture = LoadObject<UTexture2D>(NULL, TEXT("EditorMaterials.Att_EdBackground"), NULL, LOAD_None, NULL);


	BigPanel = new wxPanel(this);
	wxBoxSizer* VertSizer = new wxBoxSizer(wxVERTICAL);
	{
		ToolBar = new WxAttachmentEditorToolBar(BigPanel, -1);

		VertSizer->Add(ToolBar, 0, wxGROW | wxALL, 0);
	}
	{
		GraphWindow = new WxLinkedObjVCHolder( BigPanel, -1, this );
		LinkedObjVC = GraphWindow->LinkedObjVC;

		VertSizer->Add(GraphWindow, 1, wxGROW | wxALL, 0);
	}
	BigPanel->SetSizer(VertSizer);

	MenuBar = new WxMBAttachmentEditor();
}

void WxAttachmentEditor::Update()
{
	RefreshGraph();
}

void WxAttachmentEditor::Activated()
{
	// Let the super class do it's thing
	WxBrowser::Activated();

	Update();
}

void WxAttachmentEditor::Serialize(FArchive& Ar)
{
	// Need to call Serialize(Ar) on super class in case we ever move inheritance from FSerializeObject up the chain.
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << BackgroundTexture;

		// Serialize all Actors, just in case
		TArray<AActor*> AllActors;
		for(INT i=0; i<DrawInfos.Num(); i++)
		{
			if(DrawInfos(i))
			{
				DrawInfos(i)->GetAllActors(AllActors);
			}
		}

		for(INT ActorIdx=0; ActorIdx<AllActors.Num(); ActorIdx++)
		{
			Ar << AllActors(ActorIdx);
		}
		
		// Serialize just the actors.  
		TArray< AActor*> ActorKeys;
		ActorToDrawPosMap.GenerateKeyArray( ActorKeys );
		Ar << ActorKeys;

		// Clear out the ref to the selected connector object. 
		SelectedConnectorObj = NULL;
	}
}

/** Called when the user right-clicks in the editor background; shows a context menu */
void WxAttachmentEditor::OpenNewObjectMenu()
{
	// Display the background options context menu
	WxAttachEdBackgroundOptions Menu( this );
	FTrackPopupMenu Tpm( this, &Menu );
	Tpm.Show();
}

void WxAttachmentEditor::OpenObjectOptionsMenu()
{
	WxAttachEdNodeOptions menu( this );
	FTrackPopupMenu tpm( this, &menu );
	tpm.Show();
}

void WxAttachmentEditor::AltClickConnector( struct FLinkedObjectConnector& Connector )
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("BreakParentLink")) );

	// The actor to break defauts to the actor pointed to by the connector we clicked on.
	AActor* ActorToBreak = Cast<AActor>(SelectedConnectorObj);

	// If the connector is an output connector, we need to find the actor connected to it
	if( ActorToBreak && SelectedConnectorType == LOC_OUTPUT)
	{
		// Find the actor connected to this link;
		// First start by finding the draw info of the connector that was clicked
		FAttachEdDrawInfo* FoundInfo = FindDrawInfoForActor( *ActorToBreak );
		
		// Something bad happened if we clicked on a connector but there is no draw info for it
		check(FoundInfo);
		
		// Next, from the found draw info, get the child corresponding to the selected connector index
		// The actor associated with this child info is the actor whose connection we need to break
		ActorToBreak = FoundInfo->ChildInfos( SelectedConnectorIndex )->Actor;
		
	}

	// Break the link
	if( ActorToBreak && !ActorToBreak->bDeleteMe )
	{
		if ( ActorToBreak->GetBase() != NULL )
		{
			ActorToBreak->Modify();
			ActorToBreak->SetBase(NULL);
		}

		// Refresh the graph view
		RefreshGraph();
		// Redraw the level editing viewports to update attachment visuals
		GUnrealEd->RedrawLevelEditingViewports();
	}
}

void WxAttachmentEditor::ClickedLine( struct FLinkedObjectConnector &Src, struct FLinkedObjectConnector &Dest )
{
	UBOOL bAltDown = (LinkedObjVC->Viewport->KeyState(KEY_LeftAlt) || LinkedObjVC->Viewport->KeyState(KEY_RightAlt));

	if( bAltDown )
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("BreakParentLink")) );
		
		// Remove the base of the source actor which represents a child of the dest actor
		AActor* SelectedActor = Cast<AActor>( Src.ConnObj );
		if( SelectedActor && !SelectedActor->bDeleteMe )
		{
			SelectedActor->Modify();
			SelectedActor->SetBase(NULL);
		}

		// Refresh the graph view
		RefreshGraph();
		// Redraw the level editing viewports to update attachment visuals
		GUnrealEd->RedrawLevelEditingViewports();
	}
	
}

void WxAttachmentEditor::SetSelectedConnector( struct FLinkedObjectConnector& Connector )
{
	// Remember the selected connector info
	SelectedConnectorObj = Connector.ConnObj;
	SelectedConnectorType = Connector.ConnType;
	SelectedConnectorIndex = Connector.ConnIndex;
}

void WxAttachmentEditor::MoveSelectedObjects( INT DeltaX, INT DeltaY )
{
	// Iterate over each selected actor
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* SelectedActor = Cast<AActor>(*It);
		if(SelectedActor)
		{
			// Find the draw info for the selected actor
			FAttachEdDrawInfo* Info = FindDrawInfoForActor( *SelectedActor );
			if( Info )
			{
				// Set the draw info to a custom position
				Info->SetUserDrawPos( Info->DrawX + DeltaX, Info->DrawY + DeltaY );
			}
		}
	}
}

/**
 * Gets the position of the selected connector.
 *
 * @return	an FIntPoint that represents the position of the selected connector, or (0,0)
 *			if no connectors are currently selected
 */
FIntPoint WxAttachmentEditor::GetSelectedConnLocation( FCanvas* Canvas )
{
	FIntPoint ConnLocation(0,0);
	AActor* Actor = Cast<AActor>(SelectedConnectorObj);
	if( Actor )
	{
		// Find the draw info for the actor to determine the connector position
		FAttachEdDrawInfo* FoundInfo = FindDrawInfoForActor( *Actor );
		if( SelectedConnectorType == LOC_OUTPUT )
		{
			// If the connector type is an output link we have the position stored.
			const FIntPoint& ConnPoint = FoundInfo->OutputPositions( SelectedConnectorIndex );
			// Increase the location in X by the width of a connector so no lines are drawn directly on the connector
			ConnLocation.X = ConnPoint.X + LO_CONNECTOR_WIDTH;
			ConnLocation.Y = ConnPoint.Y;
		}
		else
		{
			// If the connector type is an input link, there is only one connector position and we can use the draw position of the draw info as X
			ConnLocation = FIntPoint( FoundInfo->DrawX - LO_CONNECTOR_WIDTH, FoundInfo->InputY );
		}
	}
	return ConnLocation;
}

/**
 * Called when the user releases the mouse over a link connector during a link connection operation.
 * Make a connection between selected connector and an object or another connector.
 *
 * @param	Connector	the connector corresponding to the endpoint of the new link
 */
void WxAttachmentEditor::MakeConnectionToConnector( struct FLinkedObjectConnector& Connector )
{
	MakeConnectionToObject( Connector.ConnObj );
}

/**
 * Called when the user releases the mouse over an object during a link connection operation.
 * Makes a connection between the current selected connector and the specified object.
 *
 * @param	Obj		the object target corresponding to the endpoint of the new link connection
 */
void WxAttachmentEditor::MakeConnectionToObject( UObject* Obj )
{
	AActor* BaseActor = NULL;
	AActor* ActorToBase = NULL;
	if( SelectedConnectorType == LOC_OUTPUT)
	{
		// If the connector we dragged a line from is an output connector
		// then the base actor is the actor the selected connector references
		// and the actor to base is the actor referenced by the connector we dropped the link on.
		BaseActor = CastChecked<AActor>( SelectedConnectorObj );
		ActorToBase = CastChecked<AActor>( Obj );
	}
	else
	{
		// If the connector we dragged a line from is an input connector
		// then the base actor is the actor referenced by the connector we dropped the link on
		// and the actor to base is the actor the selected connector references.
		BaseActor = CastChecked<AActor>( Obj );
		ActorToBase = CastChecked<AActor>( SelectedConnectorObj );
	}

	// Do not attempt to base actors to themselves or to the same base
	if( ActorToBase != BaseActor && ActorToBase->Base != BaseActor )
	{
		ActorToBase->Modify();
		ASkeletalMeshActor* SkelActor = Cast<ASkeletalMeshActor>( BaseActor );
		if( SkelActor )
		{
			// If a skeletal mesh actor is being attached to prompt the user to select a bone or socket name to attach to
			AttachToSkeletalMesh( SkelActor, ActorToBase );
		}
		else
		{
			ActorToBase->SetBase( BaseActor );
		}

		RefreshGraph();
	}
}

/**
 * Attaches an actor to a skeletal mesh, prompting the user to select a socket or bone to attach to.
 * 
 * @param SkelMeshActor	The skeletal mesh actor to attach to
 * @param ActorToAttach The actor being attached to the skeletal mesh
 */
void WxAttachmentEditor::AttachToSkeletalMesh( ASkeletalMeshActor* SkelMeshActor, AActor* ActorToAttach )
{
	// Show a menu of bone and socket names
	WxAttachEdSkeletalMeshAttachOptions Menu( SkelMeshActor );
	FTrackPopupMenu Tpm( this, &Menu );
	Tpm.Show();

	// Get the bone/socket name the user chose
	const FName& SelectedName = Menu.GetSelectedName();

	if( SelectedName != NAME_None )
	{
		// The user chose a name, set the base with the approprate bone name and skeletal mesh component
		ActorToAttach->SetBase( SkelMeshActor, FVector(0,0,1), 1, SkelMeshActor->SkeletalMeshComponent, SelectedName );
	}
	else
	{
		// The user chose to attach directly to the skeletal mesh
		ActorToAttach->SetBase( SkelMeshActor );
	}
}

/**
 * Called when an object in the attachment editor is double clicked
 */
void WxAttachmentEditor::DoubleClickedObject( UObject *Obj )
{
	// The clicked object must be an actor in the attachment editor
	AActor* Actor = CastChecked<AActor>( Obj );

	// Move the selected viewports camera directly in front of the actor.
	GEditor->MoveViewportCamerasToActor( *Actor, TRUE );

}

/**
 * Callback interface; Check for map change callbacks and clear out the attachment editor
 *
 * @param	InType	Callback type
 * @param	Flag	Flag associated with the provided callback
 */
void WxAttachmentEditor::Send( ECallbackEventType InType, DWORD Flag )
{
	// If a map change is occurring, clear out the attachment editor
	if ( InType == CALLBACK_MapChange && ( Flag != MapChangeEventFlags::Default && Flag != MapChangeEventFlags::MapRebuild ) )
	{
		ClearActors();
	}
}

/** Called when editor is resized */
void WxAttachmentEditor::OnSize( wxSizeEvent& In )
{
	if ( LinkedObjVC && LinkedObjVC->Viewport )
	{
		LinkedObjVC->Viewport->Invalidate();
	}

	if(BigPanel)
	{
		BigPanel->SetSize( GetClientRect() );
	}
}

/** Add a particular actor's graph to the editor (if its not already there) */
void WxAttachmentEditor::AddActorToEditor(AActor* Actor)
{
	// Find the root of this actors attachment graph
	AActor* BaseMost = Actor->GetBaseMost();
	// If its not already added, make a new graph for it
	if(!BaseMost->bDeleteMe && !IsActorAdded(BaseMost))
	{
		FAttachEdDrawInfo* NewInfo = new FAttachEdDrawInfo();
		NewInfo->Actor = BaseMost;
		DrawInfos.AddItem(NewInfo);

		// Determine if the actor had a previous position.
		FIntPoint* DrawPos = ActorToDrawPosMap.Find( BaseMost );
		if( DrawPos )
		{
			// If the actor did have a previous position, restore it now.
			NewInfo->SetUserDrawPos( DrawPos->X, DrawPos->Y );
		}

		// Make cloud of actors that share same base-most as this one (to speed up the exact parent stuff)
		TArray<AActor*> ChildActors;
		for (FActorIterator It; It; ++It)
		{
			AActor* TestActor = *It;
			if(!TestActor->bDeleteMe && (TestActor->GetBaseMost() == BaseMost))
			{
				ChildActors.AddItem(TestActor);
			}
		}

		// Build out the graph information from the 'cloud' of child actors
		NewInfo->AddChildActors(ChildActors, &ActorToDrawPosMap );
	}

	RecalcDrawPositions();
}

/** Attempt to add all selected actors, along with everything they're attached to, to the attachment editor */
void WxAttachmentEditor::AddSelectedToEditor()
{
	// Iterate over each selected actor
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* SelectedActor = Cast<AActor>(*It);
		if(SelectedActor)
		{
			AddActorToEditor(SelectedActor);
		}
	}

	RefreshViewport();
}

/** Handler for 'add actor' button. Adds selected Actor(s), along with everything they are attached to, into the editor */
void WxAttachmentEditor::OnAddSelectedActor( wxCommandEvent& In)
{
	AddSelectedToEditor();
}

/** Handler for 'clear' button. Remove all actors from attachment editor window */
void WxAttachmentEditor::OnClearActors( wxCommandEvent& In)
{
	ClearActors();
}

/** Handler for 'attach actors' button. Attaches selected actors together. */
void WxAttachmentEditor::OnAttachActors( wxCommandEvent& In)
{
	AttachSelected();
}

/** Handler for 'refresh' button. Regenerates the graph view */
void WxAttachmentEditor::OnRefreshGraph( wxCommandEvent& In)
{
	RefreshGraph();
}


/** Handle for 'auto arrange' button. Auto arranges graph nodes in the editor window */
void WxAttachmentEditor::OnAutoArrange( wxCommandEvent& In )
{
	// Pass true to clear all draw info positions.
	RefreshGraph( TRUE );
}

/** Handler for 'break parent link' menu option. */
void WxAttachmentEditor::OnBreakParent( wxCommandEvent& In)
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("BreakParentLink")) );

	// Iterate over each selected actors
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* SelectedActor = Cast<AActor>(*It);
		if(SelectedActor && !SelectedActor->bDeleteMe)
		{
			SelectedActor->Modify();
			SelectedActor->SetBase(NULL);
		}
	}

	// Refresh the graph view
	RefreshGraph();
	// Redraw the level editing viewports to update attachment visuals
	GUnrealEd->RedrawLevelEditingViewports();
}

/** Handler for 'Remove attachment graph' menu option.*/
void WxAttachmentEditor::OnRemoveGraph( wxCommandEvent& In )
{
	// A list of infos to remove
	TArray<FAttachEdDrawInfo*> InfosToRemove;

	// Iterate over each selected actors
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		// Find the root base of the selected actor
		AActor* SelectedActor = CastChecked<AActor>(*It);
		AActor* BaseMost = SelectedActor->GetBaseMost();
		// Find the roots draw info.  No recursion necessary since its a root, it will be in the DrawInfos array
		for( INT InfoIdx =0; InfoIdx < DrawInfos.Num(); ++InfoIdx )
		{
			FAttachEdDrawInfo* DrawInfo = DrawInfos( InfoIdx );
			if( DrawInfo->Actor == BaseMost )
			{
				// This draw info belongs to the root so it needs to be removed
				InfosToRemove.AddUniqueItem( DrawInfo );
			}
		}
	}

	for( INT InfoIdx = 0; InfoIdx < InfosToRemove.Num(); ++InfoIdx )
	{
		FAttachEdDrawInfo* DrawInfo = InfosToRemove(InfoIdx);
		// Remove the draw info 
		DrawInfos.RemoveItem( DrawInfo );
		// delete it to remove the children
		delete DrawInfo;
	}

	RefreshViewport();
}

/**
 * Handler for 'Break All Attachments of Selected and Remove' menu option.
 *
 * @param	In	Event generated by wxWidgets when the menu option is selected
 */
void WxAttachmentEditor::OnDetachAllSelectedAndRemove( wxCommandEvent& In )
{
	DetachAndRemoveSelected();
}

/** Handler for selecting downstream graph nodes */
void WxAttachmentEditor::OnSelectDownsteamNodes( wxCommandEvent& In )
{
	TArray<AActor*> ActorsToEvaluate;	// Graph nodes that need to be traced downstream
	TArray<AActor*> ActorsEvalated;		// Keep track of evaluated graph nodes so we don't re-evaluate them
	TArray<AActor*> ActorsToSelect;		// Downstream graph nodes that will end up being selected

	// Add currently selected graph nodes to the "need to be traced downstream" list
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* SelectedActor = CastChecked<AActor>(*It);
		ActorsToEvaluate.AddItem(SelectedActor);
		ActorsToSelect.AddItem(SelectedActor);
	}

	// Generate a list of downstream nodes
	while (ActorsToEvaluate.Num() > 0)
	{
		AActor* CurrentActor = ActorsToEvaluate.Last();
		if (CurrentActor)
		{
			for (FActorIterator It; It; ++It)
			{
				AActor* TestActor = *It;
				if(TestActor && !TestActor->bDeleteMe && (TestActor->GetBase() == CurrentActor))
				{
					INT index = -1;
					ActorsEvalated.FindItem(TestActor, index);

					if (index < 0)
					{
						// This node is a downstream node (so, we'll need to select it) 
						// and it's children need to be traced as well
						ActorsToSelect.AddItem(TestActor);
						ActorsToEvaluate.AddItem(TestActor);
					}
				}
			}
		}

		// This graph node has now been examined
		ActorsEvalated.AddItem(CurrentActor);
		ActorsToEvaluate.RemoveItem(CurrentActor);
	}

	// Select all downstream nodes
	if (ActorsToSelect.Num() > 0)
	{
		EmptySelection();

		for (INT Idx = ActorsToSelect.Num() - 1; Idx >= 0; --Idx)
		{
			AddToSelection(ActorsToSelect(Idx));
		}

		RefreshViewport();
	}
}

/** Handler for selecting upstream graph nodes */
void WxAttachmentEditor::OnSelectUpsteamNodes( wxCommandEvent& In )
{
	TArray<AActor*> ActorsToEvaluate;	// Graph nodes that need to be traced upstream
	TArray<AActor*> ActorsEvalated;		// Keep track of evaluated graph nodes so we don't re-evaluate them
	TArray<AActor*> ActorsToSelect;		// Upstream graph nodes that will end up being selected

	// Add currently selected graph nodes to the "need to be traced upstream" list
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* SelectedActor = CastChecked<AActor>(*It);
		ActorsToEvaluate.AddItem(SelectedActor);
	}

	// Generate a list of upstream nodes
	while (ActorsToEvaluate.Num() > 0)
	{
		AActor* CurrentActor = ActorsToEvaluate.Last();
		if (CurrentActor)
		{
			for (FActorIterator It; It; ++It)
			{
				AActor* TestActor = *It;
				if(TestActor && !TestActor->bDeleteMe && (CurrentActor->GetBase() == TestActor))
				{
					INT index = -1;
					ActorsEvalated.FindItem(TestActor, index);

					if (index < 0)
					{
						// This node is a upstream node (so, we'll need to select it) 
						// and it's children need to be traced as well
						ActorsToSelect.AddItem(TestActor);
						ActorsToEvaluate.AddItem(TestActor);
					}
				}
			}
		}

		// This graph node has now been examined
		ActorsEvalated.AddItem(CurrentActor);
		ActorsToEvaluate.RemoveItem(CurrentActor);
	}

	// Select all upstream nodes
	if (ActorsToSelect.Num() > 0)
	{
		for ( TArray<AActor*>::TConstIterator ActorIter(ActorsToSelect); ActorIter; ++ActorIter )
		{
			AddToSelection(*ActorIter);
		}

		RefreshViewport();
	}
}

/** Redraw the attachment viewport */
void WxAttachmentEditor::RefreshViewport()
{
	LinkedObjVC->Viewport->Invalidate();
}

/** Update all position information of nodes */
void WxAttachmentEditor::RecalcDrawPositions()
{
	// First we let everything calc the height of the children, to work out the total height of all stuff
	INT TotalHeight = 0;
	for(INT i=0; i<DrawInfos.Num(); i++)
	{
		if(DrawInfos(i))
		{
			TotalHeight += DrawInfos(i)->CalcTotalHeight();
		}
	}

	// Now we can calculate where each node should be
	INT CurrentYPos = 200;
	for(INT i=0; i<DrawInfos.Num(); i++)
	{
		if(DrawInfos(i))
		{
			CurrentYPos += (0.5f * DrawInfos(i)->TotalHeight);

			DrawInfos(i)->CalcPosition(0, CurrentYPos);

			CurrentYPos += (0.5f * DrawInfos(i)->TotalHeight);

			// Bit of extra space between each 'tree'
			CurrentYPos += 25;
		}
	}
}

/** See if an actor is already added to the editor */
UBOOL WxAttachmentEditor::IsActorAdded(AActor* Actor)
{
	if(Actor)
	{
		AActor* BaseMost = Actor->GetBaseMost();
		for(INT i=0; i<DrawInfos.Num(); i++)
		{
			if( DrawInfos(i)->Actor == BaseMost )
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}


/** Attach the selected actors together */
void WxAttachmentEditor::AttachSelected()
{
	// First try to add the selected actors to the attachment editor in case they aren't already present
	AddSelectedToEditor();

	// Now attach the selected actors together
	GUnrealEd->AttachSelectedActors();

	RefreshGraph();
}

/** Clears all of the actors from the attachment editor */
void WxAttachmentEditor::ClearActors()
{
	// Delete all of the allocated draw info structs
	for ( TArray<FAttachEdDrawInfo*>::TIterator DrawInfoIter( DrawInfos ); DrawInfoIter; ++DrawInfoIter )
	{
		FAttachEdDrawInfo* CurDrawInfo = *DrawInfoIter;
		delete CurDrawInfo;
		CurDrawInfo = NULL;
	}
	DrawInfos.Empty();

	RefreshViewport();
}

/**
 * Remove the selected actors' graphs from the editor, breaking their attachments in the process
 */
void WxAttachmentEditor::DetachAndRemoveSelected()
{
	// Iterate over every selected actor
	for ( FSelectionIterator SelectedActorIter( GEditor->GetSelectedActorIterator() ); SelectedActorIter; ++SelectedActorIter )
	{
		AActor* CurActor = Cast<AActor>( *SelectedActorIter );
		
		// Check if the actor is in the attachment editor or not
		if ( IsActorAdded( CurActor ) && !CurActor->bDeleteMe )
		{
			// Ensure we have draw info for the actor
			FAttachEdDrawInfo* FoundInfoForActor = NULL;
			TArray<FAttachEdDrawInfo*>* FoundInfoArray = NULL;
			FindDrawInfoForActor( *CurActor, FoundInfoForActor, FoundInfoArray );
			check( FoundInfoForActor && FoundInfoArray );

			// Break all of the attachments from the actor
			BreakAllAttachmentsForInfo( *FoundInfoForActor, FALSE );

			// Remove the found info from its corresponding array and free the memory
			// allocated to it
			FoundInfoArray->RemoveItem( FoundInfoForActor );
			delete FoundInfoForActor;
			FoundInfoForActor = NULL;
		}
	}
	// Refresh the graph to reflect any changes
	RefreshGraph();
}

/**
 * Break all of the attachments for the actor of the provided info, setting its base to NULL, and un-attaching anything based on the actor
 *
 * @param	ActorDrawInfo		Info containing the actor to break all attachments for
 * @param	bShouldRefreshGraph	Optional parameter, if TRUE, will refresh the graph after breaking attachments
 */
void WxAttachmentEditor::BreakAllAttachmentsForInfo( FAttachEdDrawInfo& ActorDrawInfo, UBOOL bShouldRefreshGraph /*= TRUE*/ )
{
	if ( ActorDrawInfo.Actor )
	{
		// First, make sure the actor is no longer based on another actor
		ActorDrawInfo.Actor->SetBase( NULL );

		// Next, set each child actor's base to NULL as well, as they will no longer be based upon the provided actor
		for ( TArray<FAttachEdDrawInfo*>::TIterator ChildIter( ActorDrawInfo.ChildInfos ); ChildIter; ++ChildIter )
		{
			FAttachEdDrawInfo* CurDrawInfo = *ChildIter;
			if ( CurDrawInfo && CurDrawInfo->Actor )
			{
				CurDrawInfo->Actor->SetBase( NULL );
			}
		}

		// Append each of the child infos to the main draw infos array, as they should still be part of the attachment editor,
		// just not attached to anything
		DrawInfos.Append( ActorDrawInfo.ChildInfos );

		// Empty the child info array of the actor; no deletion needs to occur because the pointers have been transferred over to
		// the main draw infos array
		ActorDrawInfo.ChildInfos.Empty();

		// Refresh the graph if desired
		if ( bShouldRefreshGraph )
		{
			RefreshGraph();
		}
	}
}

/** 
 * Gets all actors and their draw info positions in the attachment editor
 * 
 * @param Infos	The infos to search.  Will search all child infos of the passed in infos.
 * @param OutActorToDrawPos	A mapping of Actors to their draw info positions.
 */
static void GetAllActorsAndPositions( const TArray<FAttachEdDrawInfo*>& Infos, TMap<AActor*, FIntPoint>& OutActorToDrawPos )
{
	for(INT i=0; i<Infos.Num(); i++)
	{
		OutActorToDrawPos.Set( Infos(i)->Actor, FIntPoint( Infos(i)->DrawX, Infos(i)->DrawY ) );
		// Recurse through children
		GetAllActorsAndPositions( Infos(i)->ChildInfos, OutActorToDrawPos );
	}
}

/** 
 * Regenerates attachment graph, without changing the set visible
 *
 * @param bResetDrawPositions	If true, reset the user draw positions of all nodes in the graph.  This will auto arrange all nodes
 */
void WxAttachmentEditor::RefreshGraph( UBOOL bResetDrawPositions )
{
	TArray<AActor*> AllActors;

	ActorToDrawPosMap.Empty();
	if( !bResetDrawPositions )
	{
		// If we are not resetting draw positions, store the positions now since we are going to clear everything
		GetAllActorsAndPositions( DrawInfos, ActorToDrawPosMap );
	}
	else
	{
		// Grab all actors shown in the editor
		for(INT i=0; i<DrawInfos.Num(); i++)
		{
			if(DrawInfos(i))
			{
				DrawInfos(i)->GetAllActors(AllActors);
			}
		}
	}

	// Rebuild the editor view
	ClearActors();

	if( !bResetDrawPositions )
	{
		// Use the map as a source of actors
		for( TMap<AActor*, FIntPoint>::TConstIterator It( ActorToDrawPosMap ); It; ++It )
		{
			AddActorToEditor( It.Key() );
		}
	}
	else
	{
		for(INT ActorIdx=0; ActorIdx<AllActors.Num(); ActorIdx++)
		{
			AddActorToEditor( AllActors(ActorIdx) );
		}
	}

	RefreshViewport();
}

/** Called to draw all objects */
void WxAttachmentEditor::DrawObjects(FViewport* Viewport, FCanvas* Canvas)
{
	// draw the background texture if specified
	if (BackgroundTexture != NULL)
	{
		Canvas->PushAbsoluteTransform(FMatrix::Identity);

		Clear(Canvas, FColor(161,161,161) );

		const INT ViewWidth = GraphWindow->GetSize().x;
		const INT ViewHeight = GraphWindow->GetSize().y;

		// draw the texture to the side, stretched vertically
		DrawTile(Canvas, ViewWidth - BackgroundTexture->SizeX, 0,
			BackgroundTexture->SizeX, ViewHeight,
			0.f, 0.f,
			1.f, 1.f,
			FLinearColor::White,
			BackgroundTexture->Resource );

		// stretch the left part of the texture to fill the remaining gap
		if (ViewWidth > BackgroundTexture->SizeX)
		{
			DrawTile(Canvas, 0, 0,
				ViewWidth - BackgroundTexture->SizeX, ViewHeight,
				0.f, 0.f,
				0.1f, 0.1f,
				FLinearColor::White,
				BackgroundTexture->Resource );
		}

		Canvas->PopTransform();
	}

	// Starting from root of each attachment tree, draw
	for(INT i=0; i<DrawInfos.Num(); i++)
	{
		if(DrawInfos(i))
		{
			DrawInfos(i)->DrawInfo(Canvas, TRUE);
		}
	}
}

void WxAttachmentEditor::EdHandleKeyInput(FViewport* Viewport, FName Key, EInputEvent Event)
{
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	UBOOL bAltDown = Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt);

	if( Event == IE_Pressed )
	{
		if( bAltDown && Key == KEY_B )
		{
			AttachSelected();
		}
		else if ( Key == KEY_Delete )
		{
			DetachAndRemoveSelected();
		}
		else if( bAltDown && Key == KEY_A )
		{
			AddSelectedToEditor();
		}
		else if( bAltDown && Key == KEY_C )
		{
			ClearActors();
		}	
		else if( bCtrlDown && Key == KEY_Right )
		{
			// Iterate over each selected actor, and select all actors based to it
			for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ); It; ++It )
			{
				AActor* SelectedActor = Cast<AActor>(*It);
				if( SelectedActor )
				{
					SelectAllChildren( *SelectedActor );
				}
			}
			RefreshViewport();
		}
	}
}

/**
 * Selects all children of the passed in actor (Actors based to it)
 *
 * @param InActor	The actor which may have other actors based to it.
 */
void WxAttachmentEditor::SelectAllChildren( AActor& InActor )
{
	// Find the draw info of the selected actor
	FAttachEdDrawInfo* ActorDrawInfo = FindDrawInfoForActor( InActor );
	if( ActorDrawInfo )
	{
		// If the draw info has been found, get all actors all actors referenced by the draw info and select them
		TArray<AActor*> AllActors;
		ActorDrawInfo->GetAllActors( AllActors );
		for( INT ActorIdx = 0; ActorIdx < AllActors.Num(); ++ActorIdx )
		{
			AActor* Actor = AllActors( ActorIdx );
			if( !Actor->IsSelected() )
			{
				// Only select actors which are not already selected
				GEditor->SelectActor( Actor, TRUE, NULL, TRUE, TRUE );
			}
		}
	}
}

void WxAttachmentEditor::EmptySelection()
{
	GUnrealEd->SelectNone( TRUE, TRUE );
	RefreshViewport();
}

void WxAttachmentEditor::AddToSelection( UObject* Obj )
{
	AActor* Actor = Cast<AActor>(Obj);
	if(Actor)
	{
		GUnrealEd->SelectActor( Actor, TRUE, NULL, TRUE, TRUE );
		RefreshViewport();
	}
}

void WxAttachmentEditor::RemoveFromSelection( UObject* Obj )
{
	AActor* Actor = Cast<AActor>(Obj);
	if(Actor)
	{
		GUnrealEd->SelectActor( Actor, FALSE, NULL, TRUE, TRUE );
		RefreshViewport();
	}
}

UBOOL WxAttachmentEditor::IsInSelection( UObject* Obj ) const
{
	AActor* Actor = Cast<AActor>(Obj);
	if(Actor)
	{
		return Actor->IsSelected();
	}
	return FALSE;
}


INT WxAttachmentEditor::GetNumSelected() const
{
	return GEditor->GetSelectedActorCount();
}

/**
 * Finds the corresponding draw info for an actor.
 * 
 * @param	ActorToFind		The actor whose info should be found
 * @return	A draw info struct, or null if the a node representing the actor is not being drawn in the attachment editor
 */
FAttachEdDrawInfo* WxAttachmentEditor::FindDrawInfoForActor( const AActor& ActorToFind )
{
	FAttachEdDrawInfo* FoundInfo = NULL;
	TArray<FAttachEdDrawInfo*>* FoundInfoArray = NULL;
	FindDrawInfoForActor( ActorToFind, FoundInfo, FoundInfoArray );
	return FoundInfo;
}

/**
 * Finds corresponding draw info for an actor, if any, as well as the draw info array that draw info is located in.
 *
 * @param	ActorToFind			The actor whose info should be found
 * @param	OutFoundInfo		Pointer which is set to the draw info corresponding to the actor, if found; NULL otherwise
 * @param	OutFoundInfoArray	Pointer which is set to the draw info array containing the found draw info, if found; NULL otherwise
 */
void WxAttachmentEditor::FindDrawInfoForActor( const AActor& ActorToFind, FAttachEdDrawInfo*& OutFoundInfo, TArray<FAttachEdDrawInfo*>*& OutFoundInfoArray )
{
	// Null out the pointers, so that they are both NULL if the draw info can't be found for the provided actor
	OutFoundInfo = NULL;
	OutFoundInfoArray = NULL;

	const AActor* CurActor = &ActorToFind;

	// Store all of the bases of the actor in a stack. They will be used to identify the path of draw infos required to reach
	// where the actor's draw info should be, allowing the algorithm to not have to search every single draw info in the editor.
	TArray<AActor*> BasesOfActor;
	while ( CurActor->Base )
	{
		BasesOfActor.Push( CurActor->Base );
		CurActor = CurActor->Base;
	}

	// Start the search at the main/root-level array of draw infos; if the actor has any bases, the basemost should be present
	// in the root-level of draw infos
	TArray<FAttachEdDrawInfo*>* CurInfoArray = &DrawInfos;
	while ( BasesOfActor.Num() > 0 )
	{
		AActor* CurBase = BasesOfActor.Pop();
		UBOOL bFoundCurBase = FALSE;

		// Loop until the current base from the actor base stack is found
		for ( TArray<FAttachEdDrawInfo*>::TConstIterator DrawInfoIter( *CurInfoArray ); DrawInfoIter; ++DrawInfoIter )
		{
			FAttachEdDrawInfo* CurDrawInfo = *DrawInfoIter;

			// If the current base's draw info was found, advance our search into its child draw infos
			if ( CurDrawInfo && CurDrawInfo->Actor == CurBase )
			{
				CurInfoArray = &( CurDrawInfo->ChildInfos );
				bFoundCurBase = TRUE;
				break;
			}
		}

		// If the current base wasn't found, the actor isn't going to be found either, so exit out immediately
		if ( !bFoundCurBase )
		{
			return;
		}
	}

	// With all the bases searched and accounted for, the search should now be properly in the correct draw info array
	// for the actor's draw info
	for ( TArray<FAttachEdDrawInfo*>::TConstIterator DrawInfoIter( *CurInfoArray ); DrawInfoIter; ++DrawInfoIter )
	{
		FAttachEdDrawInfo* CurDrawInfo = *DrawInfoIter;
		
		// If the actor's draw info was found, set the passed in pointers to point to the proper draw info and draw info array
		if ( CurDrawInfo && CurDrawInfo->Actor == &ActorToFind )
		{
			OutFoundInfo = CurDrawInfo;
			OutFoundInfoArray = CurInfoArray;
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FAttachEdDrawInfo

/** Empty/free all child info's owned by this one */
void FAttachEdDrawInfo::ReleaseChildInfos()
{
	for(INT i=0; i<ChildInfos.Num(); i++)
	{
		if(ChildInfos(i))
		{
			delete ChildInfos(i);
			ChildInfos(i) = NULL;
		}
	}
	ChildInfos.Empty();
}

/** Calculate the total height of this node, plus all child nodes */
INT FAttachEdDrawInfo::CalcTotalHeight()
{
	TotalHeight = 0;

	for(INT i=0; i<ChildInfos.Num(); i++)
	{
		if( ChildInfos(i) )
		{
			ChildInfos(i)->CalcTotalHeight();

			TotalHeight += ChildInfos(i)->TotalHeight;
		}
	}

	// Can be no smaller than the node itself
	FLinkedObjDrawInfo ObjInfo = MakeLinkObjDrawInfo(TRUE);
	const FIntPoint LogicSize = FLinkedObjDrawUtils::GetLogicConnectorsSize(ObjInfo);
	NodeHeight = 23 + (7*2) + LogicSize.Y;

	TotalHeight = Max<INT>(NodeHeight, TotalHeight);

	return TotalHeight;
}

/** Update the internal position of this node, and all children */
void FAttachEdDrawInfo::CalcPosition(INT InDepth, FLOAT YPos)
{
	Depth = InDepth;

	// This is the calculated x and y position of this connector as if it were auto arranged
	// We store this value so that child connectors can position themselves based on this connector
	// However if the user moved this connector, its draw position will not use these values
	const INT CalcDrawX = 200 + (200 * Depth);
	const INT CalcDrawY = YPos;

	if( !bUserMoved )
	{
		// The user did not move this connector so the draw position should be auto calculated
		DrawX = CalcDrawX;
		DrawY = CalcDrawY;
	}

	INT YOffset = -0.5f * TotalHeight;

	for(INT i=0; i<ChildInfos.Num(); i++)
	{
		if( ChildInfos(i) )
		{
			YOffset += (0.5f * ChildInfos(i)->TotalHeight);

			ChildInfos(i)->CalcPosition(Depth + 1, CalcDrawY + YOffset);

			YOffset += (0.5f * ChildInfos(i)->TotalHeight);
		}
	}
}

/** Generate a FLinkedObjDrawInfo struct, based on the contents of this node */
FLinkedObjDrawInfo FAttachEdDrawInfo::MakeLinkObjDrawInfo(UBOOL bDrawInput)
{
	// Fill in information to draw this node
	FLinkedObjDrawInfo ObjInfo;

	if(bDrawInput)
	{
		ObjInfo.Inputs.AddItem( FLinkedObjConnInfo(TEXT(" "), FColor(0,0,0)) );
	}

	for(INT i=0; i<ChildInfos.Num(); i++)
	{
		AActor* Child = ChildInfos(i)->Actor;
		
		// See if this child is attached to a bone and if it is, draw the bone name by the connector
		const FName& BoneName = Child->BaseBoneName;
		ObjInfo.Outputs.AddItem( FLinkedObjConnInfo( BoneName != NAME_None ? *BoneName.ToString() : TEXT("") , FColor(0,0,0)) );
	}

	// Always add an extra unused connector so it can be used as an additional attachment point
	ObjInfo.Outputs.AddItem( FLinkedObjConnInfo(TEXT(""), FColor(0,0,0) ) );

	// 'object' for this node is the actor
	ObjInfo.ObjObject = Actor;

	return ObjInfo;
}

/** Draw this node onto the canvas, and then draw all child nodes */
void FAttachEdDrawInfo::DrawInfo(FCanvas* Canvas, UBOOL bDrawInput)
{
	// First draw all child infos (ensures input location is up to date)
	for(INT i=0; i<ChildInfos.Num(); i++)
	{
		if(ChildInfos(i))
		{
			ChildInfos(i)->DrawInfo(Canvas, TRUE);
		}
	}

	FLinkedObjDrawInfo ObjInfo = MakeLinkObjDrawInfo(bDrawInput);


	if( OutputPositions.Num() != ObjInfo.Outputs.Num() )
	{
		// The number of outputs changed, make sure the stored output position array matches
		OutputPositions.Reset();
		OutputPositions.AddZeroed( ObjInfo.Outputs.Num() );
	}

	// Get desired selection state
	TArray<AActor*> NewChildren;
	AActor* NewBase = GUnrealEd->GetDesiredAttachmentState(NewChildren);

	// Default border comment is the empty string
	FString BorderComment;

	// Default border color is black
	FColor BorderColor(0,0,0);

	// Font color
	const FColor FontColor = FColor( 255, 255, 128 );

	// Will be a child - color is yellow
	if(NewChildren.ContainsItem(Actor))
	{
		BorderColor = FColor(255,255,0);
	}
	// Will be new base - color is orange
	else if(NewBase == Actor)
	{
		BorderColor = FColor(255,143,0);

		// Have the border comment indicate that this draw info is the "base to be" if a new connection occurs
		BorderComment = LocalizeUnrealEd("AttachEdBaseToBe");
	}

	// Draw the actual node
	FString NodeName = Actor->GetName();
	FIntPoint DrawPos = FIntPoint(DrawX, DrawY - (0.5f*NodeHeight));
	FLinkedObjDrawUtils::DrawLinkedObj( Canvas, ObjInfo, *NodeName, *BorderComment, FontColor, BorderColor, FColor(124,63,168), DrawPos);

	// Get how wide this node is, needed to find where outputs are
	const FIntPoint TitleSize = FLinkedObjDrawUtils::GetTitleBarSize(Canvas, *NodeName);
	const FIntPoint LogicSize = FLinkedObjDrawUtils::GetLogicConnectorsSize(ObjInfo);
	INT MaxWidth = Max(TitleSize.X, LogicSize.X);

	// Save Y location of our single input
	if(ObjInfo.InputY.Num() > 0)
	{
		InputY = ObjInfo.InputY(0);
	}
	else
	{
		InputY = 0;
	}

	// There should always be as many outputs as child infos plus one more for an additional spot to attach things
	check(ObjInfo.OutputY.Num() == ChildInfos.Num() + 1);
	INT ChildIdx = 0;
	for( ChildIdx; ChildIdx < ChildInfos.Num(); ++ChildIdx )
	{
		if(ChildInfos(ChildIdx))
		{
			FIntPoint Start(DrawX + MaxWidth + LO_CONNECTOR_WIDTH, ObjInfo.OutputY(ChildIdx));
			FIntPoint End(ChildInfos(ChildIdx)->DrawX, ChildInfos(ChildIdx)->InputY);

			// Store the position of this connector
			OutputPositions(ChildIdx) = Start;

			const FLOAT Tension = Abs<INT>(Start.X - End.X);
			
			if( Canvas->IsHitTesting() )
			{
				// Set up a hit proxy for the line we are about to draw so we can detect if it was clicked
				Canvas->SetHitProxy( new HLinkedObjLineProxy( ChildInfos(ChildIdx)->Actor, ChildIdx, this->Actor, this->Depth ) );
			}

			FLinkedObjDrawUtils::DrawSpline(Canvas, End, -Tension * FVector2D(1,0), Start, -Tension * FVector2D(1,0), FColor(0,0,0), TRUE);
			
			if( Canvas->IsHitTesting() )
			{	
				Canvas->SetHitProxy( NULL );
			}
		}
	}

	// The connector doesnt represent a child but a place to add a new child later.
	OutputPositions(ChildIdx) = FIntPoint( DrawX + MaxWidth, ObjInfo.OutputY(ChildIdx) );
}

/** Sets a custom user draw position for this info */
void FAttachEdDrawInfo::SetUserDrawPos( INT InX, INT InY )
{
	// If this function is being called then the user moved this info.
	bUserMoved = TRUE;
	DrawX = InX;
	DrawY = InY;
}

/** 
 *	See if any actors in ChildActors are a direct child of the Actor of this DrawInfo. If so, create a ChildInfo entry 
 *	For each newly created ChildIndo entry, call this function on it in turn, to build out the whole tree
 *  
 *	@param ChildActors			 	The actors to check for direct children
 *  @param ActorToUserDrawPosMap	Optional mapping of actors to their position in the graph.  
 *									This is used to preserve moved draw infos in the editor when they are destroyed during a refresh
 */
void FAttachEdDrawInfo::AddChildActors(const TArray<AActor*>& ChildActors, const TMap<AActor*, FIntPoint>* ActorToUserDrawPosMap )
{
	ReleaseChildInfos();

	for(INT ActorIdx=0; ActorIdx<ChildActors.Num(); ActorIdx++)
	{
		if(ChildActors(ActorIdx)->Base == Actor)
		{
			FAttachEdDrawInfo* NewInfo = new FAttachEdDrawInfo();
			NewInfo->Actor = ChildActors(ActorIdx);
			ChildInfos.AddItem(NewInfo);

			if( ActorToUserDrawPosMap )
			{
				// Attempt to find a previous position for this draw info
				const FIntPoint* DrawPos = ActorToUserDrawPosMap->Find( ChildActors(ActorIdx) );
				if( DrawPos )
				{
					NewInfo->SetUserDrawPos( DrawPos->X, DrawPos->Y );
				}
			}
		}
	}

	for(INT i=0; i<ChildInfos.Num(); i++)
	{
		if(ChildInfos(i))
		{
			ChildInfos(i)->AddChildActors( ChildActors, ActorToUserDrawPosMap );
		}
	}
}

/** Get all actors ref'd by this node, and its children */
void FAttachEdDrawInfo::GetAllActors(TArray<AActor*>& OutActors)
{
	// Add actor in this node
	OutActors.AddUniqueItem(Actor);

	// Get actors from all child nodes
	for(INT i=0; i<ChildInfos.Num(); i++)
	{
		if(ChildInfos(i))
		{
			ChildInfos(i)->GetAllActors(OutActors);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// WxFacadeToolBar

/** Constructor for 'toolbar', loads bitmaps, creates buttons etc. */
WxAttachmentEditorToolBar::WxAttachmentEditorToolBar( wxWindow* InParent, wxWindowID InID )
: wxPanel( InParent, InID )
{	
	wxBoxSizer* ToolBarSizer = new wxBoxSizer(wxHORIZONTAL);
	{
		wxSize ButtonSize;
		ButtonSize.Set(18, 18);

		// Add Actor button
		AddActorB.Load( TEXT("AED_AddActor") );
		AddActorButton = new WxBitmapCheckButton(this, this, IDM_ATTACHEDIT_ADDACTOR, &AddActorB, &AddActorB);
		AddActorButton->SetSize(ButtonSize);
		AddActorButton->SetToolTip(*LocalizeUnrealEd("AttachEdAddActor"));
		ToolBarSizer->Add(AddActorButton, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 0);

		// Clear button
		ClearB.Load( TEXT("AED_Clear") );
		ClearButton = new WxBitmapCheckButton(this, this, IDM_ATTACHEDIT_CLEAR, &ClearB, &ClearB);
		ClearButton->SetSize(ButtonSize);
		ClearButton->SetToolTip(*LocalizeUnrealEd("AttachEdClear"));
		ToolBarSizer->Add(ClearButton, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 0);

		// Attach button
		AttachB.Load( TEXT("AED_AttachActors") );
		AttachButton = new WxBitmapCheckButton(this, this, IDM_ATTACHEDIT_ATTACH, &AttachB, &AttachB);
		AttachButton->SetSize(ButtonSize);
		AttachButton->SetToolTip(*LocalizeUnrealEd("AttachEdAttach"));
		ToolBarSizer->Add(AttachButton, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 0);

		// Refresh button
		RefreshB.Load( TEXT("AED_Refresh") );
		RefreshButton = new WxBitmapCheckButton(this, this, IDM_ATTACHEDIT_REFRESH, &RefreshB, &RefreshB);
		RefreshButton->SetSize(ButtonSize);
		RefreshButton->SetToolTip(*LocalizeUnrealEd("AttachEdRefresh"));
		ToolBarSizer->Add(RefreshButton, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 0);

		AutoArrangeB.Load( TEXT("AED_AutoArrange") );
		AutoArrangeButton = new WxBitmapCheckButton(this, this, IDM_ATTACHEDIT_AUTOARRANGE, &AutoArrangeB, &AutoArrangeB );
		AutoArrangeButton->SetSize(ButtonSize);
		AutoArrangeButton->SetToolTip(*LocalizeUnrealEd("AttachEdAutoArrange"));
		ToolBarSizer->Add(AutoArrangeButton, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 0);
	}
	SetSizer(ToolBarSizer);
}

/** Destructor */
WxAttachmentEditorToolBar::~WxAttachmentEditorToolBar()
{
}
