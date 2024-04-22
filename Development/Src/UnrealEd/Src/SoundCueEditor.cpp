/*=============================================================================
	SoundCueEditor.cpp: SoundCue editing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "EngineSoundClasses.h"
#include "UnLinkedObjDrawUtils.h"
#include "SoundCueEditor.h"
#include "PropertyWindow.h"

#include "ScopedTransaction.h"

#include "UnPackageTools.h"

#include "EngineAudioDeviceClasses.h"

#include "BusyCursor.h"
#include "PackageHelperFunctions.h"

IMPLEMENT_CLASS( USoundNodeHelper )


/**
 * Utility that can be used to check for loops in the sound node tree. 
 *
 * @param	Start		The sound node to search through its children.
 *
 * @param	Destination	The sound node to search for in the parent node's children.
 * 
 * @return	TRUE if the destination sound node is a child node of the start node; FALSE, otherwise.
 */
static UBOOL CheckSoundNodeReachability( USoundNode* Start, USoundNode* Destination )
{
	check(Start && Destination);

	// Grab all child nodes. We can't just test the destination because 
	// the loop could happen from any additional child nodes. 
	TArray<USoundNode*> Nodes;
	Start->GetAllNodes(Nodes);

	// If our test destination is in that set, return true.
	return Nodes.ContainsItem(Destination);
}

///////////////////////////////////////////////////////////////////////////////
//
// WxSoundCueEditorEditMenu
//
///////////////////////////////////////////////////////////////////////////////

class WxSoundCueEditorEditMenu : public wxMenuBar
{
public:
	WxSoundCueEditorEditMenu()
	{
		wxMenu* EditMenu = new wxMenu();
		EditMenu->Append( IDM_SOUNDCUE_CUT, *LocalizeUnrealEd("Cut"));
		EditMenu->Append( IDM_SOUNDCUE_COPY, *LocalizeUnrealEd("Copy"));
		EditMenu->Append( IDM_SOUNDCUE_PASTE, *LocalizeUnrealEd("Paste"));

		Append( EditMenu, *LocalizeUnrealEd("Edit"));
	}
};

void WxSoundCueEditor::OpenNewObjectMenu()
{
	WxMBSoundCueEdNewNode menu( this );
	FTrackPopupMenu tpm( this, &menu );
	tpm.Show();
}

void WxSoundCueEditor::OpenObjectOptionsMenu()
{
	WxMBSoundCueEdNodeOptions menu( this );
	FTrackPopupMenu tpm( this, &menu );
	tpm.Show();
}

void WxSoundCueEditor::OpenConnectorOptionsMenu()
{
	WxMBSoundCueEdConnectorOptions menu( this );
	FTrackPopupMenu tpm( this, &menu );
	tpm.Show();
}

void WxSoundCueEditor::DrawObjects(FViewport* Viewport, FCanvas* Canvas)
{
	WxLinkedObjEd::DrawObjects( Viewport, Canvas );
	SoundCue->DrawCue(Canvas, SelectedNodes);
}

void WxSoundCueEditor::UpdatePropertyWindow()
{
	if( SelectedNodes.Num() )
	{
		PropertyWindow->SetObjectArray( SelectedNodes, EPropertyWindowFlags::ShouldShowCategories  );
	}
	else
	{
		PropertyWindow->SetObject( SoundCue, EPropertyWindowFlags::Sorted );
	}

	PropertyWindow->ExpandAllItems();
}

void WxSoundCueEditor::EmptySelection()
{
	SelectedNodes.Empty();
}

void WxSoundCueEditor::AddToSelection( UObject* Obj )
{
	check( Obj->IsA( USoundNode::StaticClass() ) );

	if( SelectedNodes.ContainsItem( ( USoundNode *)Obj ) )
	{
		return;
	}

	SelectedNodes.AddItem( (USoundNode*)Obj );
}

UBOOL WxSoundCueEditor::IsInSelection( UObject* Obj ) const
{
	check( Obj->IsA( USoundNode::StaticClass() ) );

	return SelectedNodes.ContainsItem( ( USoundNode * )Obj );
}

INT WxSoundCueEditor::GetNumSelected() const
{
	return SelectedNodes.Num();
}

void WxSoundCueEditor::SetSelectedConnector( FLinkedObjectConnector& Connector )
{
	check( Connector.ConnObj->IsA( USoundCue::StaticClass() ) || Connector.ConnObj->IsA( USoundNode::StaticClass() ) );

	ConnObj = Connector.ConnObj;
	ConnType = Connector.ConnType;
	ConnIndex = Connector.ConnIndex;
}

FIntPoint WxSoundCueEditor::GetSelectedConnLocation( FCanvas* Canvas )
{
	// If no ConnNode, return origin. This works in the case of connecting a node to the 'root'.
	if( !ConnObj )
	{
		return FIntPoint( 0, 0 );
	}

	// Special case of connection from 'root' connector.
	if( ConnObj == SoundCue )
	{
		return FIntPoint( 0, 0 );
	}

	USoundNode* ConnNode = CastChecked<USoundNode>( ConnObj );

	FSoundNodeEditorData* ConnNodeEdData = SoundCue->EditorData.Find( ConnNode );
	check( ConnNodeEdData );

	return ConnNode->GetConnectionLocation( Canvas, ConnType, ConnIndex, *ConnNodeEdData );
}

void WxSoundCueEditor::MakeConnectionToConnector( FLinkedObjectConnector& Connector )
{
	// Avoid connections to yourself.
	if( Connector.ConnObj == ConnObj )
	{
		return;
	}

	const FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT("SoundCueEditorMakeConnectionToConnector") ) );

	// Handle special case of connecting a node to the 'root'.
	if( Connector.ConnObj == SoundCue )
	{
		if( ConnType == LOC_OUTPUT )
		{
			check( Connector.ConnType == LOC_INPUT );
			check( Connector.ConnIndex == 0 );

			USoundNode* ConnNode = CastChecked<USoundNode>( ConnObj );
			SoundCue->Modify();
			SoundCue->FirstNode = ConnNode;

			SoundCue->PostEditChange();
			NotifyObjectsChanged();
		}
		return;
	}
	else if( ConnObj == SoundCue )
	{
		if( Connector.ConnType == LOC_OUTPUT )
		{
			check( ConnType == LOC_INPUT );
			check( ConnIndex == 0 );

			USoundNode* EndConnNode = CastChecked<USoundNode>( Connector.ConnObj );
			SoundCue->Modify();
			SoundCue->FirstNode = EndConnNode;

			SoundCue->PostEditChange();
			NotifyObjectsChanged();
		}
		return;
	}

	// Normal case - connecting an input of one node to the output of another.
	if( ConnType == LOC_INPUT && Connector.ConnType == LOC_OUTPUT )
	{
		check( Connector.ConnIndex == 0 );

		USoundNode* ConnNode = CastChecked<USoundNode>( ConnObj );
		USoundNode* EndConnNode = CastChecked<USoundNode>( Connector.ConnObj );

		// Check to make sure the user isn't creating a loop somewhere in the sound node tree. Loops in 
		// the sound node tree will cause infinite loops in the code, which of course crash the engine.
		const UBOOL bWillCauseLoop = CheckSoundNodeReachability( EndConnNode, ConnNode );

		if( !bWillCauseLoop )
		{
			ConnectNodes( ConnNode, ConnIndex, EndConnNode );
		}
		else
		{
			appMsgf(AMT_OK, *LocalizeUnrealEd("Error_PBLoopDetected") );
		}
	}
	else if( ConnType == LOC_OUTPUT && Connector.ConnType == LOC_INPUT )
	{
		check( ConnIndex == 0 );

		USoundNode* ConnNode = CastChecked<USoundNode>( ConnObj );
		USoundNode* EndConnNode = CastChecked<USoundNode>( Connector.ConnObj );

		// Check to make sure the user isn't creating a loop somewhere in the sound node tree. Loops in 
		// the sound node tree will cause infinite loops in the code, which of course crash the engine.
		const UBOOL bWillCauseLoop = CheckSoundNodeReachability( ConnNode, EndConnNode );

		if( !bWillCauseLoop )
		{
			ConnectNodes( EndConnNode, Connector.ConnIndex, ConnNode );
		}
		else
		{
			appMsgf(AMT_OK, *LocalizeUnrealEd("Error_PBLoopDetected") );
		}
	}
}

void WxSoundCueEditor::MakeConnectionToObject( UObject* EndObj )
{
}

/**
 * Called when the user releases the mouse over a link connector and is holding the ALT key.
 * Commonly used as a shortcut to breaking connections.
 *
 * @param	Connector	The connector that was ALT+clicked upon.
 */
void WxSoundCueEditor::AltClickConnector( FLinkedObjectConnector& Connector )
{
	SetSelectedConnector(Connector);

	wxCommandEvent DummyEvent;
	DummyEvent.SetClientData(&Connector); //this works because OnContextBreakLink() is called immediately
	OnContextBreakLink( DummyEvent );

	NotifyObjectsChanged();
}

void WxSoundCueEditor::MoveSelectedObjects( INT DeltaX, INT DeltaY )
{
	for( INT i = 0; i < SelectedNodes.Num(); i++ )
	{
		USoundNode* Node = SelectedNodes( i );
		FSoundNodeEditorData* EdData = SoundCue->EditorData.Find( Node );
		check(EdData);

		EdData->NodePosX += DeltaX;
		EdData->NodePosY += DeltaY;
	}
	
	NotifyObjectsChanged();
}

// Retrieves a reference to the copy paste buffer.  Creates it if it doesnt exist.
static USoundCue& GetCopyPasteBuffer()
{
	if( !GUnrealEd->SoundCueCopyPasteBuffer )
	{
		GUnrealEd->SoundCueCopyPasteBuffer = ConstructObject<USoundCue>( USoundCue::StaticClass() );
	}

	check( GUnrealEd->SoundCueCopyPasteBuffer );
	return *( GUnrealEd->SoundCueCopyPasteBuffer );
}

/**
 * Copies SoundNodes to a SoundCue, duplicating each node (unless its a SoundNodeWave)
 *
 * @param	SrcData		The source data to copy, this takes a TMap of the sound nodes and their position in the SoundCueEditor.
 * @param	ToSoundCue	The SoundCue to copy the data to.	
 */
static void CopySoundNodes( WxSoundCueEditor& InSoundCueEditor, const TMap< USoundNode*,FSoundNodeEditorData >& SrcData, USoundCue& ToSoundCue, const UBOOL bInSelectNodes)
{
	// A mapping of source to destination nodes to fix references later.
	TMap<USoundNode*,USoundNode*> SrcToDestMap;

	// All the new nodes we created
	TArray< USoundNode *> NewNodes;
	
	// Copy the Source Data
	for( TMap<USoundNode*, FSoundNodeEditorData>::TConstIterator It( SrcData ); It; ++It )
	{
		// The original node.
		USoundNode* SrcNode = It.Key();
	
		// The copied node.
		USoundNode* NewNode = NULL;

		// Special case handling is needed for SoundNodeWaves
		UBOOL bSrcIsASoundNodeWave = SrcNode->IsA( USoundNodeWave::StaticClass() );

		if( bSrcIsASoundNodeWave && ToSoundCue.EditorData.Find(SrcNode) )
		{
			// This SoundCue already has this SoundNodeWave and it cant exist twice.
			appMsgf( AMT_OK, *LocalizeUnrealEd( "Error_OnlyAddSoundNodeWaveOnce" ) );
			continue;
		}
		else if( bSrcIsASoundNodeWave )
		{
			// The source is a sound node wave but we arent copying it to a buffer that already has the same one.
			// If the source node is a SoundNodeWave do not call StaticDuplicate, keep the old reference.  
			NewNode = SrcNode;
		}
		else
		{
			// Duplicate the node
			NewNode = Cast<USoundNode>(UObject::StaticDuplicateObject( SrcNode, SrcNode, &ToSoundCue, NULL, RF_Transactional ) );
		}

		// NewNode must exist
		check(NewNode);

		SrcToDestMap.Set( SrcNode, NewNode );

		// Use the SoundNodeEditor data of the original node we copied + an offset so nodes don't overlap completely.
		// This is so we can have a suitable place to position the node when we paste.
		const INT NodeOffset = 15;
		FSoundNodeEditorData NewEditorData = It.Value();
		NewEditorData.NodePosX += NodeOffset;
		NewEditorData.NodePosY += NodeOffset;

		ToSoundCue.EditorData.Set( NewNode, NewEditorData ) ;

		if (bInSelectNodes)
		{
			InSoundCueEditor.AddToSelection(NewNode);
		}

		NewNodes.AddItem(NewNode);
	}

	// Fix references to the newly created nodes
	for( INT NewNodeIdx = 0; NewNodeIdx < NewNodes.Num(); ++NewNodeIdx )
	{
		USoundNode* NewNode = NewNodes(NewNodeIdx);

		// Iterate through the ChildNodes of each copied node
		const TArray< USoundNode* > ChildNodes = NewNode->ChildNodes;
		for( INT ChildNodeIdx = 0; ChildNodeIdx < ChildNodes.Num(); ++ChildNodeIdx )
		{
			// Find the original child node and replace it with the copied child node (if it exists)
			USoundNode** Node =  SrcToDestMap.Find( ChildNodes(ChildNodeIdx) );
			if( Node )
			{
				NewNode->ChildNodes( ChildNodeIdx ) = *Node;
			}
			else
			{
				NewNode->ChildNodes( ChildNodeIdx ) = NULL;
			}

		}
	}
}

void WxSoundCueEditor::EdHandleKeyInput( FViewport* Viewport, FName Key, EInputEvent Event )
{
	const UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	if( Event == IE_Pressed )
	{
		if( Key == KEY_Delete )
		{
			DeleteSelectedNodes();
		}

		if( bCtrlDown )
		{
			// Ctrl-C was pressed.  Initiate a copy of sound nodes.
			if( Key == KEY_C )
			{
				Copy();
			}
			else if( Key == KEY_X)
			{
				Cut();
			}
			else if( Key == KEY_V)
			{
				Paste();
			}
			else if( Key == KEY_Y )
			{
				Redo();
			}
			else if( Key == KEY_Z )
			{
				Undo();
			}
		}
			
	}
}

void WxSoundCueEditor::ConnectNodes( USoundNode* ParentNode, INT ChildIndex, USoundNode* ChildNode )
{
	check( ChildIndex >= 0 && ChildIndex < ParentNode->ChildNodes.Num() );

	ParentNode->SetFlags( RF_Transactional );
	ParentNode->Modify();
	ParentNode->ChildNodes( ChildIndex ) = ChildNode;

	ParentNode->PostEditChange();
	NotifyObjectsChanged();

	UpdatePropertyWindow();
	RefreshViewport();
}

void WxSoundCueEditor::DeleteSelectedNodes()
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT("SoundCueEditorDeleteSelectedNode") ) );
	for( INT i = 0; i < SelectedNodes.Num(); i++ )
	{
		USoundNode* DelNode = SelectedNodes( i );

		// We look through all nodes to see if there is a node that still references the one we want to delete.
		// If so, break the link.
		for( TMap<USoundNode*, FSoundNodeEditorData>::TIterator It( SoundCue->EditorData ); It; ++It )
		{
			USoundNode* ChkNode = It.Key();

			if( ChkNode == NULL )
			{
				continue;
			}

			for( INT ChildIdx = 0; ChildIdx < ChkNode->ChildNodes.Num(); ChildIdx++ )
			{
				if( ChkNode->ChildNodes( ChildIdx ) == DelNode )
				{
					ChkNode->SetFlags( RF_Transactional );
					ChkNode->Modify();
					ChkNode->ChildNodes( ChildIdx ) = NULL;
					ChkNode->PostEditChange();
				}
			}
		}

		SoundCue->Modify();

		// Also check the 'first node' pointer
		if( SoundCue->FirstNode == DelNode )
		{
			SoundCue->FirstNode = NULL;
		}

		// Remove this node from the SoundCue's map of all SoundNodes
		check( SoundCue->EditorData.Find( DelNode ) );
		SoundCue->EditorData.Remove( DelNode );
		NotifyObjectsChanged();
	}

	SelectedNodes.Empty();

	UpdatePropertyWindow();
	RefreshViewport();
}

/**
* Uses the global Undo transactor to redo changes, update viewports etc.
*/
void WxSoundCueEditor::Undo()
{
	EmptySelection();
	GEditor->UndoTransaction();

	UpdatePropertyWindow();
	RefreshViewport();
}

/**
* Uses the global Redo transactor to undo changes, update viewports etc.
*/
void WxSoundCueEditor::Redo()
{
	EmptySelection();
	GEditor->RedoTransaction();

	UpdatePropertyWindow();
	RefreshViewport();

	NotifyObjectsChanged();
	UpdatePropertyWindow();
	RefreshViewport();
	
}

/** 
 * Copy the current selection
 */
void WxSoundCueEditor::Copy()
{
	// Empty all previous copy data.  We are starting a new copy
	GetCopyPasteBuffer().EditorData.Empty();

	TMap< USoundNode*, FSoundNodeEditorData > DataToCopy;

	// Iterate through all selected nodes and copy them to a map with their SoundNodeEditorData. 
	// We need the editor data to have a suitable place to position nodes when pasted.
	for( INT SelectedIdx = 0; SelectedIdx < SelectedNodes.Num(); ++SelectedIdx )
	{
		USoundNode* SelectedNode = SelectedNodes(SelectedIdx);
		DataToCopy.Set( SelectedNode, *(SoundCue->EditorData.Find(SelectedNode)) );
	}

	// Copy the currently selected nodes into the CopyPasteBuffer for pasting later.
	UBOOL bSelectNodes = FALSE;
	CopySoundNodes(*this, DataToCopy, GetCopyPasteBuffer(), bSelectNodes);
}

/** 
 * Cut the current selection (just a copy and a delete)
 */
void WxSoundCueEditor::Cut()
{
	Copy();
	DeleteSelectedNodes();
}

/** 
 * Paste the copied selection
 */
void WxSoundCueEditor::Paste()
{
	// Begin a paste operation
	if( GetCopyPasteBuffer().EditorData.Num() > 0 )
	{
		EmptySelection();

		{
			// Undo/Redo support
			const FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT("SoundCueEditorPaste") ) );
			SoundCue->Modify();

			// Paste Nodes into the SoundCue modified by this editor
			UBOOL bSelectNodes = TRUE;
			CopySoundNodes(*this, GetCopyPasteBuffer().EditorData, *SoundCue, bSelectNodes);
		}

		SoundCue->PostEditChange();
		SoundCue->MarkPackageDirty();
		UpdatePropertyWindow();
		RefreshViewport();
	}
}

/*-----------------------------------------------------------------------------
	WxSoundCueEditor
-----------------------------------------------------------------------------*/

UBOOL				WxSoundCueEditor::bSoundNodeClassesInitialized = false;
TArray<UClass *>	WxSoundCueEditor::SoundNodeClasses;


BEGIN_EVENT_TABLE( WxSoundCueEditor, wxFrame )
	EVT_MENU_RANGE( IDM_SOUNDCUE_NEW_NODE_START, IDM_SOUNDCUE_NEW_NODE_END, WxSoundCueEditor::OnContextNewSoundNode )
	EVT_MENU( IDM_SOUNDCUE_NEW_WAVE, WxSoundCueEditor::OnContextNewWave )
	EVT_MENU( IDM_SOUNDCUE_NEW_RANDOM, WxSoundCueEditor::OnContextNewRandom )
	EVT_MENU( IDM_SOUNDCUE_ADD_INPUT, WxSoundCueEditor::OnContextAddInput )
	EVT_MENU( IDM_SOUNDCUE_DELETE_INPUT, WxSoundCueEditor::OnContextDeleteInput )
	EVT_MENU( IDM_SOUNDCUE_DELETE_NODE, WxSoundCueEditor::OnContextDeleteNode )
	EVT_MENU( IDM_SOUNDCUE_PLAY_NODE, WxSoundCueEditor::OnContextPlaySoundNode )
	EVT_MENU( IDM_SOUNDCUE_SYNC_BROWSER, WxSoundCueEditor::OnContextSyncInBrowser )
	EVT_MENU( IDM_SOUNDCUE_PLAY_CUE, WxSoundCueEditor::OnContextPlaySoundCue )
	EVT_MENU( IDM_SOUNDCUE_STOP_PLAYING, WxSoundCueEditor::OnContextStopPlaying )
	EVT_MENU( IDM_SOUNDCUE_BREAK_LINK, WxSoundCueEditor::OnContextBreakLink )
	EVT_MENU( IDM_SOUNDCUE_CUT, WxSoundCueEditor::OnMenuCut)
	EVT_MENU( IDM_SOUNDCUE_COPY, WxSoundCueEditor::OnMenuCopy)
	EVT_MENU( IDM_SOUNDCUE_PASTE, WxSoundCueEditor::OnMenuPaste)
	EVT_SIZE( WxSoundCueEditor::OnSize )
END_EVENT_TABLE()

IMPLEMENT_COMPARE_POINTER( UClass, SoundCueEditor, { return appStricmp( *A->GetName(), *B->GetName() ); } )

// Static functions that fills in array of all available USoundNode classes and sorts them alphabetically.
void WxSoundCueEditor::InitSoundNodeClasses()
{
	if( bSoundNodeClassesInitialized )
	{
		return;
	}

	// Construct list of non-abstract gameplay sequence object classes.
	for( TObjectIterator<UClass> It; It; ++It )
	{
		if( It->IsChildOf( USoundNode::StaticClass() ) 
			&& !( It->ClassFlags & CLASS_Abstract ) 
			&& !It->IsChildOf( USoundNodeWave::StaticClass() ) 
			&& !It->IsChildOf( USoundNodeAmbient::StaticClass() ) 
			&& !It->IsChildOf( UForcedLoopSoundNode::StaticClass() ) )
		{
			SoundNodeClasses.AddItem(*It);
		}
	}

	Sort<USE_COMPARE_POINTER( UClass, SoundCueEditor )>( &SoundNodeClasses( 0 ), SoundNodeClasses.Num() );

	bSoundNodeClassesInitialized = true;
}

WxSoundCueEditor::WxSoundCueEditor( wxWindow* InParent, wxWindowID InID, USoundCue* InSoundCue )
	: WxLinkedObjEd( InParent, InID, TEXT( "SoundCueEditor" ) )
{
	SoundCue = InSoundCue;

	// Set up undo/redo
	SoundCue->SetFlags(RF_Transactional);

	// Set the sound cue editor window title to include the sound cue being edited.
	SetTitle( *FString::Printf( LocalizeSecure(LocalizeUnrealEd( "SoundCueEditorCaption_F" ), *InSoundCue->GetPathName()) ) );
}

WxSoundCueEditor::~WxSoundCueEditor()
{
	SaveProperties();
}

void WxSoundCueEditor::InitEditor()
{
	CreateControls( FALSE );

	LoadProperties();

	InitSoundNodeClasses();

	// Shift origin so origin is roughly in the middle. Would be nice to use Viewport size, but doesn't seem initialised here...
	LinkedObjVC->Origin2D.X = 150;
	LinkedObjVC->Origin2D.Y = 300;

	BackgroundTexture = LoadObject<UTexture2D>( NULL, TEXT( "EditorMaterials.SoundCueBackground" ), NULL, LOAD_None, NULL );

	ConnObj = NULL;
}

/**
 * Creates the controls for this window
 */
void WxSoundCueEditor::CreateControls( UBOOL bTreeControl )
{
	WxLinkedObjEd::CreateControls( bTreeControl );

	if( PropertyWindow != NULL )
	{
		SetDockingWindowTitle( PropertyWindow, *FString::Printf( LocalizeSecure(LocalizeUnrealEd( "PropertiesCaption_F" ), *SoundCue->GetPathName()) ) );
	}

	MenuBar = new WxSoundCueEditorEditMenu();
	AppendWindowMenu(MenuBar);
	SetMenuBar(MenuBar);

	ToolBar = new WxSoundCueEdToolBar( this, -1 );
	SetToolBar( ToolBar );
}

/**
 * Used to serialize any UObjects contained that need to be to kept around.
 *
 * @param Ar The archive to serialize with
 */
void WxSoundCueEditor::Serialize( FArchive& Ar )
{
	WxLinkedObjEd::Serialize( Ar );
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << SoundCue << SelectedNodes << ConnObj << SoundNodeClasses;
	}
}

#define NODE_SPACING	70

static void CreateWaveContainers( TArray<USoundNodeWave*>& OutWavs, USoundCue* SoundCue, INT X, INT Y )
{
	TArray<USoundNodeWave*> SelectedWavs;
	GEditor->GetSelectedObjects()->GetSelectedObjects<USoundNodeWave>( SelectedWavs );

	Y -= ( SelectedWavs.Num() - 1 ) * NODE_SPACING / 2;

	for( INT i = 0; i < SelectedWavs.Num(); i++ )
	{
		USoundNodeWave* NewWave = SelectedWavs( i );
		if( NewWave )
		{
			// We can only have a particular SoundNodeWave in the SoundCue once.
			bool bDuplicate = false;

			for( TMap<USoundNode*, FSoundNodeEditorData>::TIterator It( SoundCue->EditorData ); It; ++It )
			{
				USoundNodeWave* Wave = Cast<USoundNodeWave>( It.Key() );
				if( Wave )
				{
					if( Wave->GetFName() == NewWave->GetFName() )
					{
						appMsgf( AMT_OK, *LocalizeUnrealEd( "Error_OnlyAddSoundNodeWaveOnce" ) );
						bDuplicate = true;
						break;
					}
				}
			}

			if( bDuplicate )
			{
				continue;
			}

			// Create new editor data struct and add to map in SoundCue.
			FSoundNodeEditorData NewEdData;
			appMemset( &NewEdData, 0, sizeof( FSoundNodeEditorData ) );

			NewEdData.NodePosX = X;
			NewEdData.NodePosY = Y + ( NODE_SPACING * i );

			SoundCue->EditorData.Set( NewWave, NewEdData );
			OutWavs.AddItem( NewWave );
		}
	}
}

void WxSoundCueEditor::OnContextNewSoundNode( wxCommandEvent& In )
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT("SoundCueEditorNewSoundNode") ) );
	SoundCue->Modify();

	INT NewNodeIndex = In.GetId() - IDM_SOUNDCUE_NEW_NODE_START;	
	check( NewNodeIndex >= 0 && NewNodeIndex < SoundNodeClasses.Num() );

	UClass* NewNodeClass = SoundNodeClasses( NewNodeIndex );
	check( NewNodeClass->IsChildOf( USoundNode::StaticClass() ) );

	USoundNode* NewNode = ConstructObject<USoundNode>( NewNodeClass, SoundCue, NAME_None );

    // If this node allows >0 children but by default has zero - create a connector for starters
	if( ( NewNode->GetMaxChildNodes() > 0 || NewNode->GetMaxChildNodes() == -1 ) && NewNode->ChildNodes.Num() == 0 )
	{
		NewNode->CreateStartingConnectors();
	}

	// Create new editor data struct and add to map in SoundCue.
	FSoundNodeEditorData NewEdData;
	appMemset( &NewEdData, 0, sizeof( FSoundNodeEditorData ) );

	NewEdData.NodePosX = ( LinkedObjVC->NewX - LinkedObjVC->Origin2D.X ) / LinkedObjVC->Zoom2D;
	NewEdData.NodePosY = ( LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y ) / LinkedObjVC->Zoom2D;

	SoundCue->EditorData.Set( NewNode, NewEdData );

	NotifyObjectsChanged();
	RefreshViewport();
}

void WxSoundCueEditor::OnContextNewWave( wxCommandEvent& In )
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT("SoundCueEditorNewWave") ) );
	SoundCue->Modify();

	INT iX = ( ( LinkedObjVC->NewX - LinkedObjVC->Origin2D.X ) / LinkedObjVC->Zoom2D );
	INT iY = ( ( LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y ) / LinkedObjVC->Zoom2D );

	TArray<USoundNodeWave*> CreatedWavs;
	CreateWaveContainers( CreatedWavs, SoundCue, iX, iY );

	NotifyObjectsChanged();
	RefreshViewport();
}

void WxSoundCueEditor::OnContextNewRandom( wxCommandEvent& In )
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT("SoundCueEditorNewRandom") ) );
	SoundCue->Modify();

	USoundNodeRandom* NewNode = ConstructObject<USoundNodeRandom>( USoundNodeRandom::StaticClass(), SoundCue, NAME_None );

	// Create new editor data struct and add to map in SoundCue.
	FSoundNodeEditorData NewEdData;
	appMemset( &NewEdData, 0, sizeof( FSoundNodeEditorData ) );

	NewEdData.NodePosX = ( LinkedObjVC->NewX - LinkedObjVC->Origin2D.X ) / LinkedObjVC->Zoom2D;
	NewEdData.NodePosY = ( LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y ) / LinkedObjVC->Zoom2D;

	SoundCue->EditorData.Set( NewNode, NewEdData );

	INT iX = NewEdData.NodePosX + 100;
	INT iY = NewEdData.NodePosY;

	TArray<USoundNodeWave*> CreatedWavs;
	CreateWaveContainers( CreatedWavs, SoundCue, iX, iY );

	if( CreatedWavs.Num() > 0 )
	{
		NewNode->ChildNodes.Empty();
		NewNode->Weights.Empty();
		NewNode->ChildNodes.AddZeroed( CreatedWavs.Num() );
		NewNode->Weights.AddZeroed( CreatedWavs.Num() );
		for( INT i = 0; i < CreatedWavs.Num(); i++ )
		{				
			NewNode->ChildNodes( i ) = CreatedWavs( i );
			NewNode->Weights( i ) = 1.0f;
		}
	}
	else
	{
		NewNode->CreateStartingConnectors();
	}

	NotifyObjectsChanged();
	RefreshViewport();
}

void WxSoundCueEditor::OnContextAddInput( wxCommandEvent& In )
{
	INT NumSelected = SelectedNodes.Num();
	if( NumSelected != 1 )
	{
		return;
	}

	USoundNode* Node = SelectedNodes( 0 );

	if( Node->GetMaxChildNodes() == -1 || ( Node->ChildNodes.Num() < Node->GetMaxChildNodes() ) )
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT("SoundCueEditorAddInput") ) );
		Node->SetFlags( RF_Transactional );
		Node->Modify();

		Node->InsertChildNode( Node->ChildNodes.Num() );

		Node->PostEditChange();
		NotifyObjectsChanged();

		UpdatePropertyWindow();
		RefreshViewport();
	}
}

void WxSoundCueEditor::OnContextDeleteInput( wxCommandEvent& In )
{
	check( ConnType == LOC_INPUT );

	// Can't delete root input!
	if( ConnObj == SoundCue )
	{
		return;
	}

	USoundNode* ConnNode = CastChecked<USoundNode>( ConnObj );
	check( ConnIndex >= 0 && ConnIndex < ConnNode->ChildNodes.Num() );

	ConnNode->RemoveChildNode( ConnIndex );

	ConnNode->PostEditChange();
	NotifyObjectsChanged();

	UpdatePropertyWindow();
	RefreshViewport();
}

void WxSoundCueEditor::OnContextDeleteNode( wxCommandEvent& In )
{
	DeleteSelectedNodes();
}

void WxSoundCueEditor::OnContextPlaySoundNode( wxCommandEvent& In )
{
	if( SelectedNodes.Num() != 1 )
	{
		return;
	}

	UAudioComponent* AudioComponent = GEditor->GetPreviewAudioComponent( NULL, SelectedNodes( 0 ) );
	if( AudioComponent )
	{
		AudioComponent->Stop();

		AudioComponent->SoundCue->ValidateData();

		AudioComponent->bUseOwnerLocation = FALSE;
		AudioComponent->bAutoDestroy = FALSE;
		AudioComponent->Location = FVector( 0.0f, 0.0f, 0.0f );
		AudioComponent->bIsUISound = TRUE;
		AudioComponent->bAllowSpatialization = FALSE;
		AudioComponent->bReverb = FALSE;
		AudioComponent->bCenterChannelOnly = FALSE;

		AudioComponent->Play();	
	}
}

void WxSoundCueEditor::OnContextSyncInBrowser( wxCommandEvent& In )
{
	FCallbackEventParameters Parms( NULL, CALLBACK_RefreshContentBrowser, CBR_SyncAssetView | CBR_FocusBrowser );
	for( INT ObjIndex = 0; ObjIndex < SelectedNodes.Num(); ObjIndex++ )
	{
		Parms.EventObject = SelectedNodes( ObjIndex );
		GCallbackEvent->Send( Parms );
	}
}

void WxSoundCueEditor::OnContextPlaySoundCue( wxCommandEvent& In )
{
	UAudioComponent* AudioComponent = GEditor->GetPreviewAudioComponent( SoundCue, NULL );
	if( AudioComponent )
	{
		AudioComponent->Stop();

		AudioComponent->SoundCue->ValidateData();

		AudioComponent->bUseOwnerLocation = FALSE;
		AudioComponent->bAutoDestroy = FALSE;
		AudioComponent->Location = FVector( 0.0f, 0.0f, 0.0f );
		AudioComponent->bIsUISound = TRUE;
		AudioComponent->bAllowSpatialization = FALSE;
		AudioComponent->bReverb = FALSE;
		AudioComponent->bCenterChannelOnly = FALSE;

		AudioComponent->Play();	
	}
}

void WxSoundCueEditor::OnContextStopPlaying( wxCommandEvent& In )
{
	UAudioComponent* AudioComponent = GEditor->GetPreviewAudioComponent( NULL, NULL );
	if( AudioComponent )
	{
		AudioComponent->Stop();
	}
}

void WxSoundCueEditor::OnContextBreakLink( wxCommandEvent& In )
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT("SoundCueEditorBreakLink") ) );
	SoundCue->Modify();
	if( ConnObj == SoundCue )
	{
		SoundCue->FirstNode = NULL;
		SoundCue->PostEditChange();
	}
	else
	{
		USoundNode* ConnNode = CastChecked<USoundNode>( ConnObj );

		if(ConnType == LOC_INPUT)
		{
			ConnNode->SetFlags( RF_Transactional );
			ConnNode->Modify();

			check( ConnIndex >= 0 && ConnIndex < ConnNode->ChildNodes.Num() );
			ConnNode->ChildNodes( ConnIndex ) = NULL;

			ConnNode->PostEditChange();
		}
		else if(ConnType == LOC_OUTPUT)
		{
			if(ConnNode == SoundCue->FirstNode)
			{
				SoundCue->FirstNode = NULL;
				SoundCue->PostEditChange();
			}
			else
			{
				TArray<USoundNode*> NodeCollection;
				SoundCue->EditorData.GenerateKeyArray(NodeCollection);

				UBOOL bFound = FALSE;
				for(int NodeIndex = 0; NodeIndex < NodeCollection.Num() && !bFound; ++NodeIndex)
				{
					USoundNode *CurrentNode = NodeCollection(NodeIndex);
					for(int ChildIndex = 0; ChildIndex < CurrentNode->ChildNodes.Num(); ++ChildIndex)
					{
						if(CurrentNode->ChildNodes(ChildIndex) == ConnNode)
						{
							bFound = TRUE;
							CurrentNode->SetFlags( RF_Transactional );
							CurrentNode->Modify();
							CurrentNode->ChildNodes(ChildIndex) = NULL;
							CurrentNode->PostEditChange();
							break;
						}
					}
				}
			}
		}
	}

	UpdatePropertyWindow();
	RefreshViewport();
}

void WxSoundCueEditor::OnSize( wxSizeEvent& In )
{
	RefreshViewport();
	const wxRect rc = GetClientRect();
	::MoveWindow( ( HWND )LinkedObjVC->Viewport->GetWindow(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1 );
	In.Skip();
}

void WxSoundCueEditor::OnMenuCut( wxCommandEvent& In)
{
	Cut();
}

void WxSoundCueEditor::OnMenuCopy( wxCommandEvent& In)
{
	Copy();
}

void WxSoundCueEditor::OnMenuPaste( wxCommandEvent& In)
{
	Paste();
}

/**
 * Sets all Sounds in the SelectedPackages to the SoundClass specified by SoundClassID
 * 
 * @param SelectedPackages	Packages to search inside for SoundCues.
 * @param SoundClassID		The ID of the sound class to set all sounds to.
 */
void WxSoundCueEditor::BatchProcessSoundClass( const TArray<UPackage*>& SelectedPackages, INT SoundClassID )
{
	// Disallow export if any packages are cooked.
	if ( PackageTools::DisallowOperationOnCookedPackages( SelectedPackages ) )
	{
		return;
	}

	FName SoundClassName = NAME_None;

	UAudioDevice* AudioDevice = GEditor && GEditor->Client ? GEditor->Client->GetAudioDevice() : NULL;
	if( AudioDevice )
	{
		SoundClassName = AudioDevice->GetSoundClass( SoundClassID );
	}

	// Iterate over all sound cues...
	for( TObjectIterator<USoundCue> It; It; ++It )
	{
		USoundCue*	SoundCue				= *It;
		UBOOL		bIsInSelectedPackage	= FALSE;

		// ... and check whether it belongs to any of the selected packages.
		for( INT PackageIndex=0; PackageIndex<SelectedPackages.Num(); PackageIndex++ )
		{
			UPackage* Package = SelectedPackages( PackageIndex );
			if( SoundCue->IsIn( Package ) )
			{
				bIsInSelectedPackage = TRUE;
				break;
			}
		}

		// Set the group to the specified one if we're part of a selected package.
		if( bIsInSelectedPackage )
		{
			SoundCue->SoundClass = SoundClassName;
			// Mark the package dirty so it can be saved later.
			SoundCue->Modify();
		}
	}

}

// Helper struct for clustering sounds
struct FWaveList
{
public:
	TArray< USoundNodeWave* > WaveList;
};

// Creates a substring delimited by a digit.
static inline FString GetSoundNameSubString(const TCHAR* InStr)
{
	const FString String( InStr );
	INT LeftMostDigit = -1;
	for ( INT i = String.Len()-1 ; i >= 0 && appIsDigit( String[i] ) ; --i )
	{
		LeftMostDigit = i;
	}

	FString SubString;
	if ( LeftMostDigit > 0 )
	{
		// At least one non-digit char was found.
		SubString = String.Left( LeftMostDigit );
	}
	return SubString;
}


/**
 * Clusters sounds into a cue through a random node, based on wave name, and optionally an attenuation node.
 * 
 * @param SelectedPackages			Packages to search inside for SoundNodeWaves 
 * @param bIncludeAttenuationNode	If true an attenuation node will be created in the SoundCues
 */
void WxSoundCueEditor::BatchProcessClusterSounds( const TArray< UPackage*>& SelectedPackages, UBOOL bIncludeAttenuationNode )
{
	// Disallow export if any packages are cooked.
	if (PackageTools::DisallowOperationOnCookedPackages(SelectedPackages))
	{
		return;
	}


	// WaveLists(i) is the list of waves in package SelectedPackages(i).
	TArray< FWaveList > WaveLists;
	WaveLists.AddZeroed( SelectedPackages.Num() );

	// Iterate over all waves and sort by package.
	INT NumWaves = 0;
	for( TObjectIterator<USoundNodeWave> It ; It ; ++It )
	{
		for( INT PackageIndex = 0 ; PackageIndex < SelectedPackages.Num() ; ++PackageIndex )
		{
			if( It->IsIn( SelectedPackages(PackageIndex) ) )
			{
				WaveLists(PackageIndex).WaveList.AddItem( *It );
				++NumWaves;
				break;
			}
		}
	}

	// Abort if no waves were found or the user aborts.
	if ( NumWaves == 0 || !appMsgf( AMT_YesNo, *FString::Printf( LocalizeSecure(LocalizeUnrealEd(TEXT("Prompt_AboutToCluserSoundsQ")), NumWaves) ) ) )
	{
		return;
	}

	// Prompt the user for a target group name.
	const FString GroupName;
	WxDlgGenericStringEntry dlg;
	const INT Result = dlg.ShowModal( TEXT("NewGroupName"), TEXT("NewGroupName"), TEXT("") );
	if( Result != wxID_OK )
	{
		return;		
	}

	const FScopedBusyCursor BusyCursor;

	FString NewGroupName = dlg.GetEnteredString();
	// Dont allow spaces in group names.
	NewGroupName = NewGroupName.Replace(TEXT(" "),TEXT("_"));

	INT NumCuesCreated = 0;
	TArray<FString> CuesNotCreated;

	for( INT PackageIndex = 0 ; PackageIndex < SelectedPackages.Num() ; ++PackageIndex )
	{
		// Get the wave list for the current package.
		const TArray<USoundNodeWave*>& WaveList	= WaveLists(PackageIndex).WaveList;
		if ( WaveList.Num() == 0 )
		{
			continue;
		}

		// Create a package name.
		UPackage* SelectedPackage = SelectedPackages(PackageIndex);

		// The outermost selected package should always be the root of this sound cue.
		FString PkgName = SelectedPackage->GetOutermost()->GetName();

		// Check for a valid group name and that the group name is not the same as the package name 
		// We shouldn't allow groups to exist with the same name as its outer.
		if( NewGroupName.Len() > 0 && NewGroupName != SelectedPackage->GetName() )
		{
			// Add a new group to the outermost package.  (OutermostPackage.NewGroupName)
			PkgName += FString::Printf( TEXT(".%s"), *NewGroupName );
		}
		else if( SelectedPackage->GetOuter() != NULL )
		{
			// The user did not select a top level package.  Create cues in the selected package
			PkgName += FString::Printf( TEXT(".%s"), *SelectedPackage->GetName() );
		}

		// Create a map of common name prefixes to waves.
		TMap< FString, TArray<USoundNodeWave*> > StringToWaveMap;
		for ( INT WaveIndex = 0 ; WaveIndex < WaveList.Num() ; ++WaveIndex )
		{
			USoundNodeWave* Wave = WaveList(WaveIndex);
			const FString SubString = GetSoundNameSubString( *Wave->GetName() );

			// No suitable substring can be generated if the substrings length is 0.
			if ( SubString.Len() > 0 )
			{	
				TArray<USoundNodeWave*>* List = StringToWaveMap.Find( SubString );
				if ( List )
				{
					List->AddItem( Wave );
				}
				else
				{
					TArray<USoundNodeWave*> NewList;
					NewList.AddItem( Wave );
					StringToWaveMap.Set( *SubString, NewList );
				}
			}
		}

		// Iterate over name prefixes and create a cue for each, with all nodes sharing
		// that name connected to a SoundNodeRandom in that cue.
		for ( TMap< FString, TArray<USoundNodeWave*> >::TConstIterator It( StringToWaveMap ) ; It ; ++It )
		{
			const TArray<USoundNodeWave*>& List		= It.Value();
			if ( List.Num() < 2 )
			{
				// must have at least 2 waves.
				continue;
			}
			const FString& SubString				= It.Key();
			const FString SoundCueName				= FString::Printf( TEXT("%s_Cue"), *SubString );

			// Check if a new cue with this name can be created;
			UObject* ExistingPackage				= UObject::FindPackage( NULL, *PkgName );
			FString Reason;
			if ( ExistingPackage && !FIsUniqueObjectName( *SoundCueName, ExistingPackage, Reason ) )
			{
				CuesNotCreated.AddItem( FString::Printf( TEXT("%s.%s"), *PkgName, *SoundCueName ) );
				continue;
			}

			// Create sound cue.
			USoundCue* SoundCue = ConstructObject<USoundCue>( USoundCue::StaticClass(), UObject::CreatePackage( NULL, *PkgName ), *SoundCueName, RF_Public|RF_Standalone );


			// Mark the package dirty since we added a new object.  
			SoundCue->Modify();

			++NumCuesCreated;

			debugf(TEXT("SoundCue %s created in Package: %s, Group %s, "), *SoundCue->GetName(), *SoundCue->GetOutermost()->GetName(), *SoundCue->GetOuter()->GetName() );

			// Create a random node.
			USoundNodeRandom* RandomNode = ConstructObject<USoundNodeRandom>( USoundNodeRandom::StaticClass(), SoundCue, NAME_None );

			// If this node allows >0 children but by default has zero - create a connector for starters
			if( (RandomNode->GetMaxChildNodes() > 0 || RandomNode->GetMaxChildNodes() == -1) && RandomNode->ChildNodes.Num() == 0 )
			{
				RandomNode->CreateStartingConnectors();
			}

			// Create new editor data struct and add to map in SoundCue.
			FSoundNodeEditorData RandomEdData;
			appMemset(&RandomEdData, 0, sizeof(FSoundNodeEditorData));
			RandomEdData.NodePosX = 200;
			RandomEdData.NodePosY = -150;
			SoundCue->EditorData.Set(RandomNode, RandomEdData);

			// Connect the waves.
			for ( INT WaveIndex = 0 ; WaveIndex < List.Num() ; ++WaveIndex )
			{
				USoundNodeWave* Wave = List(WaveIndex);

				// Create new editor data struct and add to map in SoundCue.
				FSoundNodeEditorData WaveEdData;
				appMemset(&WaveEdData, 0, sizeof(FSoundNodeEditorData));
				WaveEdData.NodePosX = 350;
				WaveEdData.NodePosY = -200 + WaveIndex * 75;
				SoundCue->EditorData.Set( Wave, WaveEdData );

				// Link the random node to the wave.
				while ( WaveIndex >= RandomNode->ChildNodes.Num() )
				{
					RandomNode->InsertChildNode( RandomNode->ChildNodes.Num() );
				}
				RandomNode->ChildNodes(WaveIndex) = Wave;
			}

			if ( bIncludeAttenuationNode )
			{
				USoundNode* AttenuationNode = ConstructObject<USoundNode>( USoundNodeAttenuation::StaticClass(), SoundCue, NAME_None );

				// If this node allows >0 children but by default has zero - create a connector for starters
				if( (AttenuationNode->GetMaxChildNodes() > 0 || AttenuationNode->GetMaxChildNodes() == -1) && AttenuationNode->ChildNodes.Num() == 0 )
				{
					AttenuationNode->CreateStartingConnectors();
				}

				// Create new editor data struct and add to map in SoundCue.
				FSoundNodeEditorData AttenuationEdData;
				appMemset(&AttenuationEdData, 0, sizeof(FSoundNodeEditorData));
				AttenuationEdData.NodePosX = 50;
				AttenuationEdData.NodePosY = -150;
				SoundCue->EditorData.Set(AttenuationNode, AttenuationEdData);

				// Link the attenuation node to the wave.
				AttenuationNode->ChildNodes(0) = RandomNode;

				// Link the attenuation node to root.
				SoundCue->FirstNode = AttenuationNode;
			}
			else
			{
				// Link the wave to root.
				SoundCue->FirstNode = RandomNode;
			}
		}
	}

	if( NumCuesCreated > 0 )
	{
		appMsgf( AMT_OK, *FString::Printf( LocalizeSecure(LocalizeUnrealEd(TEXT("CreatedNCues")), NumCuesCreated) ) );
	}
	else if( CuesNotCreated.Num() == 0 )
	{
		// No cues could be created and we don't have any cues we tried to create.  This is probably because we couldn't find any suitable waves to cluster.
		appMsgf( AMT_OK, *LocalizeUnrealEd("CouldNotCreateAnyCues") );
	}

	if( CuesNotCreated.Num() > 0 )
	{
		// Generate a list of cues that could not be created because their names are in use
		FString CuesNotCreatedMsg = LocalizeUnrealEd( TEXT("CouldNotCreateTheFollowingCuesNamesInUse") );
		for ( INT i = 0 ; i < CuesNotCreated.Num() ; ++i )
		{
			CuesNotCreatedMsg += FString::Printf(LINE_TERMINATOR TEXT("   %s"), *CuesNotCreated(i) );
		}

		appMsgf( AMT_OK, *CuesNotCreatedMsg );
	}
	
}

/** Helper struct for use in inserting mature sound nodes into a cue */
struct FBatchProcessMatureNodeHelper
{
	/** The cue to insert nodes into */
	USoundCue* OwningCue;
	/** The list of all sound node waves that are known to exist (for searching for non-mature versions of sounds) */
	TArray<FString> AllWaveNames;
	/** All the sound node waves in the owning cue */
	TArray<USoundNodeWave*> AllWavesInCue;
};

/**
 * @return	TRUE if the given sound cue can have a concatenator radio node inserted; FALSE, otherwise.
 */
static UBOOL IsEligibleForRadioChirp( USoundCue* ChirpCandidate )
{
	check( ChirpCandidate );

	UBOOL bIsEligible = FALSE;

	static TArray<FString> ChirpSoundClasses;

	// Load up the the names of the sound classes that can be used with radio chirps from the INI. 
	if( ChirpSoundClasses.Num() == 0 )
	{
		GConfig->GetSingleLineArray(TEXT("SoundSettings"), TEXT("ChirpSoundClasses"), ChirpSoundClasses, GEditorUserSettingsIni);
	}

	// A given sound cue is eligible if its sound class is set to a chirp sound class. 
	for( INT SoundClassIndex = 0; SoundClassIndex < ChirpSoundClasses.Num(); ++SoundClassIndex )
	{
		if( *ChirpSoundClasses(SoundClassIndex) == ChirpCandidate->SoundClass )
		{
			bIsEligible = TRUE;
			break;
		}
	}

	return bIsEligible;
}

/**
 * Inserts a concatenator radio node (for chirps) in the sound cue before 
 * any wave nodes by recursively searching through all the children.
 *
 * @param	OwningCue	The sound cue that owns all the sound nodes referenced here.
 * @param	ParentNode	The node to parse its children for wave nodes. 
 * 
 * @return	TRUE if a concatenator radio node was added to the cue; FALSE, otherwise. 
 */
UBOOL RecursiveInsertChirpNode( USoundCue* OwningCue, USoundNode* ParentNode )
{
	UBOOL bChirpInserted = FALSE;

	if( OwningCue && ParentNode )
	{
		for( INT ChildIndex = 0; ChildIndex < ParentNode->ChildNodes.Num(); ChildIndex++ )
		{
			USoundNode* ChildNode = ParentNode->ChildNodes(ChildIndex);

			if( ChildNode )
			{
				// No need to add the concatenator radio node if it already exists in this tree.
				if( ChildNode->IsA( USoundNodeConcatenatorRadio::StaticClass() ) )
				{
					bChirpInserted = FALSE;
				}
				// Create a concatenator radio node when we find a wave without a radio node in-between.
				else if( ChildNode->IsA( USoundNodeWave::StaticClass() ) )
				{
					USoundNodeConcatenatorRadio* Concatenator = ConstructObject<USoundNodeConcatenatorRadio>( USoundNodeConcatenatorRadio::StaticClass(), OwningCue, NAME_None );

					if( Concatenator )
					{
						// Create editor data for the new concatenator 
						// radio node to see in the sound cue editor.
						FSoundNodeEditorData ConcatenatorEdData;
						appMemset(&ConcatenatorEdData, 0, sizeof(FSoundNodeEditorData));
						ConcatenatorEdData.NodePosX = 200;
						ConcatenatorEdData.NodePosY = -150;

						const FSoundNodeEditorData* WaveEditorData = OwningCue->EditorData.Find(ChildNode);

						// Attempt to use the editor position of the sound wave node so 
						// that we can position the concatenator node right next to it. 
						if( WaveEditorData )
						{
							ConcatenatorEdData.NodePosX = WaveEditorData->NodePosX - 75;
							ConcatenatorEdData.NodePosY = WaveEditorData->NodePosY - 50;
						}

						OwningCue->EditorData.Set(Concatenator, ConcatenatorEdData);

						// Insert the node between the parent node and the sound wave.
						Concatenator->ChildNodes.AddItem(ChildNode);
						ParentNode->ChildNodes(ChildIndex) = Concatenator;
						bChirpInserted = TRUE;
					}
				}
				// Keep traversing the tree of children until we reach the end.
				else
				{
					bChirpInserted = RecursiveInsertChirpNode( OwningCue, ChildNode );
				}
			}
		}
	}

	return bChirpInserted;
}

/**
 * Sets up all sound cues in the given packages to play a radio chirp if possible. 
 * All sound cues must have an attenuation node and wave nodes to be eligible. 
 *
 * @param	SelectedPackages	The set of packages containing sound cues to add chirps. 
 */
void WxSoundCueEditor::BatchProcessInsertRadioChirp( const TArray< UPackage*>& SelectedPackages )
{
	// Iterate over all sound cues...
	for( TObjectIterator<USoundCue> It; It; ++It )
	{
		USoundCue*	SoundCue				= *It;
		UBOOL		bIsInSelectedPackage	= FALSE;

		// We only care about sound cue in the selected packages. 
		for( INT PackageIndex = 0; PackageIndex < SelectedPackages.Num(); PackageIndex++ )
		{
			UPackage* Package = SelectedPackages( PackageIndex );
			if( SoundCue->IsIn( Package ) )
			{
				bIsInSelectedPackage = TRUE;
				break;
			}
		}

		// Make sure the sound cue is a dialog class before attempting to apply radio chirps. 
		if( bIsInSelectedPackage && SoundCue->FirstNode && IsEligibleForRadioChirp(SoundCue) )
		{
			UBOOL bCueEdited = FALSE;

			TArray<USoundNodeAttenuation*> AttenuationNodes;
			SoundCue->RecursiveFindAttenuation( SoundCue->FirstNode, AttenuationNodes );

			// Concatenation radio nodes must have an attenuation node as a parent to 
			// add chirps. Thus, ignore any parent nodes to the attenuation nodes. 
			for( INT AttenuationIndex = 0; AttenuationIndex < AttenuationNodes.Num(); AttenuationIndex++ )
			{
				// Add the concatenation radio node by traversing the sound node 
				// children to place the node before any sound wave nodes. 
				bCueEdited |= RecursiveInsertChirpNode( SoundCue, AttenuationNodes(AttenuationIndex) );
			}

			// If a node was added, mark the package dirty.
			if( bCueEdited )
			{
				SoundCue->Modify();
			}
		}
	}
}

void WxSoundCueEditor::BatchProcessInsertRadioChirp( const TArray< UObject*>& SelectedObjects )
{
	// Iterate over all sound cues...
	for( INT ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		USoundCue* SoundCue = Cast<USoundCue>( SelectedObjects(ObjectIndex) );

		// Make sure the sound cue is a dialog class before attempting to apply radio chirps. 
		if( SoundCue && SoundCue->FirstNode && IsEligibleForRadioChirp(SoundCue) )
		{
			UBOOL bCueEdited = FALSE;

			TArray<USoundNodeAttenuation*> AttenuationNodes;
			SoundCue->RecursiveFindAttenuation( SoundCue->FirstNode, AttenuationNodes );

			// Concatenation radio nodes must have an attenuation node as a parent to 
			// add chirps. Thus, ignore any parent nodes to the attenuation nodes. 
			for( INT AttenuationIndex = 0; AttenuationIndex < AttenuationNodes.Num(); AttenuationIndex++ )
			{
				// Add the concatenation radio node by traversing the sound node 
				// children to place the node before any sound wave nodes. 
				bCueEdited |= RecursiveInsertChirpNode( SoundCue, AttenuationNodes(AttenuationIndex) );
			}

			// If a node was added, mark the package dirty.
			if( bCueEdited )
			{
				SoundCue->Modify();
			}
		}
	}

}

USoundNodeWave* FindNonMatureSoundNodeWave( const FString& NonMatureWaveName, TArray<FString>& AllSoundNodeWaves )
{
	// Create a list of material classes we should find.
	for( INT AssetIndex = 0; AssetIndex < AllSoundNodeWaves.Num(); ++AssetIndex )
	{
		const FString& AssetNameWithClass = AllSoundNodeWaves( AssetIndex );
		INT SpacePos = AssetNameWithClass.InStr( TEXT(" ") );
		FString AssetName = AssetNameWithClass.Right( AssetNameWithClass.Len() - (SpacePos + 1) );

		if( AssetName.InStr( NonMatureWaveName, TRUE, TRUE ) != INDEX_NONE )
		{
			// Attempt to find the object
			UObject* Object = UObject::StaticFindObject( USoundNodeWave::StaticClass(), ANY_PACKAGE, *AssetName, FALSE );
			if( !Object )
			{
				// It was not found so it must be loaded.
				Object = UObject::StaticLoadObject( USoundNodeWave::StaticClass(), NULL, *AssetName, NULL, LOAD_NoRedirects, NULL );
			}
			return CastChecked<USoundNodeWave>( Object );
		}
	}

	return NULL;
}

static UBOOL RecursiveInsertMatureSoundNode( FBatchProcessMatureNodeHelper& InHelper, USoundNode* ParentNode, TArray<FString>& OutErrors )
{
	UBOOL bNodeInserted = FALSE;

	if( InHelper.OwningCue && ParentNode )
	{
		for( INT ChildIndex = 0; ChildIndex < ParentNode->ChildNodes.Num(); ChildIndex++ )
		{
			USoundNode* ChildNode = ParentNode->ChildNodes(ChildIndex);

			if( ChildNode )
			{
				// No need to add the node if it already exists in this tree.
				if( ChildNode->IsA( USoundNodeMature::StaticClass() ) )
				{
					bNodeInserted = FALSE;
				}
				// Create a mature sound node when we find a wave without a radio node in-between.
				else if( ChildNode->IsA( USoundNodeWave::StaticClass() ) )
				{
					USoundNodeWave* FoundNonMatureWave = NULL;

					FString NonMatureWaveName = ChildNode->GetName()+TEXT("_Bleep");

					// Search all the waves in the cue to see if the non mature version is already in the cue.
					for( INT WaveIndex = 0; WaveIndex < InHelper.AllWavesInCue.Num(); ++WaveIndex )
					{
						if( InHelper.AllWavesInCue(WaveIndex)->GetName() == NonMatureWaveName )
						{
							FoundNonMatureWave = InHelper.AllWavesInCue(WaveIndex);
							break;
						}
					}

					if( !FoundNonMatureWave )
					{
						// If the non mature wave was not found in the cue, attempt to find it in a package
						FoundNonMatureWave = FindNonMatureSoundNodeWave( NonMatureWaveName, InHelper.AllWaveNames );
					}

					if( FoundNonMatureWave )
					{
						USoundNodeMature* MatureNode = ConstructObject<USoundNodeMature>( USoundNodeMature::StaticClass(), InHelper.OwningCue, NAME_None );
						check(MatureNode);

						// Create editor data for the new node 
						FSoundNodeEditorData MatureNodeEdData;
						appMemset(&MatureNodeEdData, 0, sizeof(FSoundNodeEditorData));
						MatureNodeEdData.NodePosX = 200;
						MatureNodeEdData.NodePosY = -150;

						const FSoundNodeEditorData* WaveEditorData = InHelper.OwningCue->EditorData.Find(ChildNode);

						// Attempt to use the editor position of the sound wave node so 
						// that we can position the node right next to it. 
						if( WaveEditorData )
						{
							MatureNodeEdData.NodePosX = WaveEditorData->NodePosX - 200;
							MatureNodeEdData.NodePosY = WaveEditorData->NodePosY;
						}

						InHelper.OwningCue->EditorData.Set(MatureNode, MatureNodeEdData);


						FSoundNodeEditorData NewEdData;
						appMemset( &NewEdData, 0, sizeof( FSoundNodeEditorData ) );

						NewEdData.NodePosX = WaveEditorData->NodePosX;
						NewEdData.NodePosY = WaveEditorData->NodePosY + 100;

						InHelper.OwningCue->EditorData.Set( FoundNonMatureWave, NewEdData );

						// Insert the node between the parent node and the sound wave.
						MatureNode->ChildNodes.AddItem(ChildNode);
						ParentNode->ChildNodes(ChildIndex) = MatureNode;
					
						USoundNodeWave* WaveNode = CastChecked<USoundNodeWave>( ChildNode );
						WaveNode->bMature = TRUE;	

						MatureNode->ChildNodes.AddItem( FoundNonMatureWave );

						bNodeInserted = TRUE;
					}
					else
					{
						OutErrors.AddItem( FString::Printf(TEXT("No mature node inserted in '%s': Could not find sound wave '%s' to insert"), *InHelper.OwningCue->GetName(), *NonMatureWaveName ) );
					}
				}
				else
				{
					// Keep traversing the tree of children until we reach the end.
					bNodeInserted = RecursiveInsertMatureSoundNode( InHelper, ChildNode, OutErrors );
				}
			}
		}
	}

	return bNodeInserted;
}

void RecursiveAddChildSoundClasses( USoundClass* ParentClass, TSet<FName>& OutSoundClasses )
{
	UAudioDevice* AudioDevice = GEditor->Client->GetAudioDevice();
	check(AudioDevice);

	for( INT SoundClassIndex = 0; SoundClassIndex < ParentClass->ChildClassNames.Num(); ++SoundClassIndex )
	{
		USoundClass* SoundClass = AudioDevice->GetSoundClass(  ParentClass->ChildClassNames(SoundClassIndex) );
		OutSoundClasses.Add( SoundClass->GetFName() );

		RecursiveAddChildSoundClasses( SoundClass, OutSoundClasses );
	}
}


static UBOOL IsEligableForMatureSoundNode( USoundCue* MatureDialogCandidate )
{
	check( MatureDialogCandidate );

	UAudioDevice* AudioDevice = GEditor->Client->GetAudioDevice();
	check(AudioDevice);

	static TSet<FName> EligableSoundClasses;

	// Load  the the names of the sound classes that can be used with mature nodes from the INI. 
	if( EligableSoundClasses.Num() == 0 )
	{
		TArray<FString> MatureNodeSoundClasses;
		GConfig->GetSingleLineArray(TEXT("SoundSettings"), TEXT("BatchProcessMatureNodeSoundClasses"), MatureNodeSoundClasses, GEditorUserSettingsIni);
		if( MatureNodeSoundClasses.Num() == 0 )
		{
			// If nothing was found in the ini, assume dialog and chatter sound classes only
			MatureNodeSoundClasses.AddItem( TEXT("Dialog") );
			MatureNodeSoundClasses.AddItem( TEXT("Chatter") );
		}

		// All subclasses of the ones specified in the INI are also valid.
		for( INT SoundClassIndex = 0; SoundClassIndex < MatureNodeSoundClasses.Num(); ++SoundClassIndex )
		{
			USoundClass* SoundClass = AudioDevice->GetSoundClass( FName( *MatureNodeSoundClasses(SoundClassIndex) ) );
			EligableSoundClasses.Add( SoundClass->GetFName() );

			RecursiveAddChildSoundClasses( SoundClass, EligableSoundClasses );	
		}
	}

	// A given sound cue is eligible if its sound class is set to a chirp sound class. 
	return EligableSoundClasses.Contains( MatureDialogCandidate->SoundClass );
}


void WxSoundCueEditor::BatchProcessInsertMatureNode( const TArray< UObject*>& SelectedObjects )
{
	TArray<FString> Errors;

	FGADHelper* GadHelper = new FGADHelper();
	if( GadHelper->Initialize() )
	{
		FBatchProcessMatureNodeHelper BatchHelper;

		TArray<FString> ClassTags;
		ClassTags.AddItem( TEXT("[ObjectType]SoundNodeWave") );

		TArray<FString> AllWaveNames;
		// Query the database for objects
		GadHelper->QueryAssetsWithAllTags( ClassTags, AllWaveNames );

		for( INT AssetIndex = 0; AssetIndex < AllWaveNames.Num(); ++AssetIndex )
		{
			if( AllWaveNames(AssetIndex).InStr(TEXT("_Bleep"), TRUE, TRUE ) != INDEX_NONE )
			{
				BatchHelper.AllWaveNames.AddItem( AllWaveNames(AssetIndex) );
			}
		}

		// Iterate over all sound cues...
		for( INT ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
		{
			USoundCue* SoundCue = Cast<USoundCue>( SelectedObjects(ObjectIndex) );

			// Make sure the sound cue is a dialog class before attempting to apply radio chirps. 
			if( SoundCue && SoundCue->FirstNode && IsEligableForMatureSoundNode(SoundCue) )
			{
				UBOOL bCueEdited = FALSE;

				BatchHelper.OwningCue = SoundCue;
				BatchHelper.AllWavesInCue.Reset();

				// Find all the waves in this cue
				TArray<USoundNodeWave*> SoundNodeWaves;
				SoundCue->RecursiveFindNode<USoundNodeWave>( SoundCue->FirstNode, BatchHelper.AllWavesInCue );

				TArray<USoundNodeAttenuation*> AttenuationNodes;
				SoundCue->RecursiveFindAttenuation( SoundCue->FirstNode, AttenuationNodes );

				// Nodes must have an attenuation node as a parent to.  Ignore any parent nodes to the attenuation nodes. 
				for( INT AttenuationIndex = 0; AttenuationIndex < AttenuationNodes.Num(); AttenuationIndex++ )
				{
					// Add the node by traversing the sound node 
					// children to place the node before any sound wave nodes. 
					bCueEdited |= RecursiveInsertMatureSoundNode( BatchHelper, AttenuationNodes(AttenuationIndex), Errors );
				}

				// If a node was added, mark the package dirty.
				if( bCueEdited )
				{
					SoundCue->Modify();
				}
			}
		}

		// Print out any errors
		if( Errors.Num() > 0 )
		{
			for( INT ErrorIndex = 0; ErrorIndex < Errors.Num(); ++ErrorIndex )
			{
				GWarn->Logf( NAME_Warning, *Errors(ErrorIndex) );
			}

			appMsgf( AMT_OK, TEXT("Errors were generated during batch processing.  See Log for details.") );
		}
	}
	delete GadHelper;
}

/**
 * One or more objects changed, so mark the package as dirty
 */
void WxSoundCueEditor::NotifyObjectsChanged()
{
	SoundCue->MarkPackageDirty();
}

/*-----------------------------------------------------------------------------
	WxSoundCueEdToolBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxSoundCueEdToolBar, WxToolBar )
END_EVENT_TABLE()

WxSoundCueEdToolBar::WxSoundCueEdToolBar( wxWindow* InParent, wxWindowID InID )
: WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
{
	PlayCueB.Load( TEXT( "SCE_PlayCue" ) );
	PlayNodeB.Load( TEXT( "SCE_PlayNode" ) );
	StopB.Load( TEXT( "SCE_Stop" ) );
	SetToolBitmapSize( wxSize( 18, 18 ) );

	AddTool( IDM_SOUNDCUE_STOP_PLAYING, StopB, *LocalizeUnrealEd( "Stop" ) );
	AddTool( IDM_SOUNDCUE_PLAY_NODE, PlayNodeB, *LocalizeUnrealEd( "PlaySelectedNode" ) );
	AddTool( IDM_SOUNDCUE_PLAY_CUE, PlayCueB, *LocalizeUnrealEd( "PlaySoundCue" ) );

	Realize();
}

WxSoundCueEdToolBar::~WxSoundCueEdToolBar()
{
}

/*-----------------------------------------------------------------------------
	WxMBSoundCueEdNewNode.
-----------------------------------------------------------------------------*/

WxMBSoundCueEdNewNode::WxMBSoundCueEdNewNode( WxSoundCueEditor* CueEditor )
{
	// Ensure that all selected assets are loaded
	// @todo CB: Ideally we could just peek at the names/type instead of force-loading here
	GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );

	Append( IDM_SOUNDCUE_PASTE, *LocalizeUnrealEd("Paste"), TEXT(""));
	AppendSeparator();

	USoundNodeWave* SelectedWave = GEditor->GetSelectedObjects()->GetTop<USoundNodeWave>();
	if( SelectedWave )
	{
		Append( IDM_SOUNDCUE_NEW_WAVE, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "SoundNodeWave_F" ), *SelectedWave->GetName() ) ), TEXT( "" ) );
		Append( IDM_SOUNDCUE_NEW_RANDOM, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "SoundNodeRandom_F" ), *SelectedWave->GetName() ) ), TEXT( "" ) );
		AppendSeparator();
	}

	for( INT i = 0; i < CueEditor->SoundNodeClasses.Num(); i++)
	{
		Append( IDM_SOUNDCUE_NEW_NODE_START + i, *CueEditor->SoundNodeClasses(i)->GetDescription(), TEXT( "" ) );
	}
}

WxMBSoundCueEdNewNode::~WxMBSoundCueEdNewNode()
{
}

/*-----------------------------------------------------------------------------
	WxMBSoundCueEdNodeOptions.
-----------------------------------------------------------------------------*/

WxMBSoundCueEdNodeOptions::WxMBSoundCueEdNodeOptions( WxSoundCueEditor* CueEditor )
{
	INT NumSelected = CueEditor->SelectedNodes.Num();

	Append( IDM_SOUNDCUE_COPY, *LocalizeUnrealEd("Copy"), TEXT( "" ));
	Append( IDM_SOUNDCUE_CUT, *LocalizeUnrealEd("Cut"), TEXT(""));
	AppendSeparator();

	if( NumSelected == 1 )
	{
		// See if we adding another input would exceed max child nodes.
		USoundNode* Node = CueEditor->SelectedNodes( 0 );
		if( Node->GetMaxChildNodes() == -1 || ( Node->ChildNodes.Num() < Node->GetMaxChildNodes() ) )
		{
			Append( IDM_SOUNDCUE_ADD_INPUT, *LocalizeUnrealEd( "AddInput" ), TEXT( "" ) );
		}
	}

	Append( IDM_SOUNDCUE_DELETE_NODE, *LocalizeUnrealEd( "DeleteSelectedNodes" ), TEXT( "" ) );

	if( NumSelected == 1 )
	{
		Append( IDM_SOUNDCUE_PLAY_NODE, *LocalizeUnrealEd( "PlaySelectedNode" ), TEXT( "" ) );
	}

	for( INT Index = 0; Index < CueEditor->SelectedNodes.Num(); Index++ )
	{
		if( Cast<USoundNodeWave>( CueEditor->SelectedNodes( Index ) ) )
		{
			Append( IDM_SOUNDCUE_SYNC_BROWSER, *LocalizeUnrealEd( "SyncNodeInBrowser" ), TEXT( "" ) );
			break;
		}
	}
}

WxMBSoundCueEdNodeOptions::~WxMBSoundCueEdNodeOptions()
{
}

/*-----------------------------------------------------------------------------
	WxMBSoundCueEdConnectorOptions.
-----------------------------------------------------------------------------*/

WxMBSoundCueEdConnectorOptions::WxMBSoundCueEdConnectorOptions( WxSoundCueEditor* CueEditor )
{
	// Only display the 'Break Link' option if there is a link to break!
	UBOOL bHasConnection = false;
	if( CueEditor->ConnType == LOC_INPUT )
	{
		if( CueEditor->ConnObj == CueEditor->SoundCue )
		{
			if( CueEditor->SoundCue->FirstNode )
			{
				bHasConnection = true;
			}
		}
		else
		{
			USoundNode* ConnNode = CastChecked<USoundNode>( CueEditor->ConnObj );

			if( ConnNode->ChildNodes( CueEditor->ConnIndex ) )
			{
				bHasConnection = true;
			}
		}
	}
	else if(CueEditor->ConnType == LOC_OUTPUT && Cast<USoundNode>(CueEditor->ConnObj))
	{
		bHasConnection = true;
	}

	if( bHasConnection )
	{
		Append( IDM_SOUNDCUE_BREAK_LINK, *LocalizeUnrealEd( "BreakLink" ), TEXT( "" ) );
	}

	// If on an input that can be deleted, show option
	if( CueEditor->ConnType == LOC_INPUT && CueEditor->ConnObj != CueEditor->SoundCue )
	{
		Append( IDM_SOUNDCUE_DELETE_INPUT, *LocalizeUnrealEd( "DeleteInput" ), TEXT( "" ) );
	}
}

WxMBSoundCueEdConnectorOptions::~WxMBSoundCueEdConnectorOptions()
{
}

