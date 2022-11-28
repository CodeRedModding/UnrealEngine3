/*=============================================================================
	SoundCueEditor.cpp: SoundCue editing
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "EngineSoundClasses.h"
#include "UnLinkedObjDrawUtils.h"
#include "SoundCueEditor.h"

void WxSoundCueEditor::OpenNewObjectMenu()
{
	wxMenu* menu = new WxMBSoundCueEdNewNode(this);

	FTrackPopupMenu tpm( this, menu );
	tpm.Show();
	delete menu;
}

void WxSoundCueEditor::OpenObjectOptionsMenu()
{
	wxMenu* menu = new WxMBSoundCueEdNodeOptions(this);

	FTrackPopupMenu tpm( this, menu );
	tpm.Show();
	delete menu;
}

void WxSoundCueEditor::OpenConnectorOptionsMenu()
{
	wxMenu* menu = new WxMBSoundCueEdConnectorOptions(this);

	FTrackPopupMenu tpm( this, menu );
	tpm.Show();
	delete menu;
}

void WxSoundCueEditor::DrawLinkedObjects(FRenderInterface* RI)
{
	WxLinkedObjEd::DrawLinkedObjects(RI);
	SoundCue->DrawCue(RI, SelectedNodes);
}

void WxSoundCueEditor::UpdatePropertyWindow()
{
	PropertyWindow->SetObjectArray( (TArray<UObject*>*)&SelectedNodes, 1,1,0 );
}

void WxSoundCueEditor::EmptySelection()
{
	SelectedNodes.Empty();
}

void WxSoundCueEditor::AddToSelection( UObject* Obj )
{
	check( Obj->IsA(USoundNode::StaticClass()) );

	if( SelectedNodes.ContainsItem( (USoundNode*)Obj ) )
		return;

	SelectedNodes.AddItem( (USoundNode*)Obj );
}

UBOOL WxSoundCueEditor::IsInSelection( UObject* Obj )
{
	check( Obj->IsA(USoundNode::StaticClass()) );

	return SelectedNodes.ContainsItem( (USoundNode*)Obj );
}

UBOOL WxSoundCueEditor::HaveObjectsSelected()
{
	return( SelectedNodes.Num() > 0 );
}

void WxSoundCueEditor::SetSelectedConnector( UObject* InConnObj, INT InConnType, INT InConnIndex )
{
	check( InConnObj->IsA(USoundCue::StaticClass()) || InConnObj->IsA(USoundNode::StaticClass()) );

	ConnObj = InConnObj;
	ConnType = InConnType;
	ConnIndex = InConnIndex;
}

FIntPoint WxSoundCueEditor::GetSelectedConnLocation(FRenderInterface* RI)
{
	// If no ConnNode, return origin. This works in the case of connecting a node to the 'root'.
	if(!ConnObj)
		return FIntPoint(0,0);

	// Special case of connection from 'root' connector.
	if(ConnObj == SoundCue)
		return FIntPoint(0,0);

	USoundNode* ConnNode = CastChecked<USoundNode>(ConnObj);

	FSoundNodeEditorData* ConnNodeEdData = SoundCue->EditorData.Find( ConnNode );
	check(ConnNodeEdData);

	return ConnNode->GetConnectionLocation(RI, ConnType, ConnIndex, *ConnNodeEdData);
}

void WxSoundCueEditor::MakeConnectionToConnector( UObject* EndConnObj, INT EndConnType, INT EndConnIndex )
{
	// Avoid connections to yourself.
	if(EndConnObj == ConnObj)
		return;

	// Handle special case of connecting a node to the 'root'.
	if(EndConnObj == SoundCue)
	{
		if(ConnType == LOC_OUTPUT)
		{
			check(EndConnType == LOC_INPUT);
			check(EndConnIndex == 0);

			USoundNode* ConnNode = CastChecked<USoundNode>(ConnObj);
			SoundCue->FirstNode = ConnNode;
		}
		return;
	}
	else if(ConnObj == SoundCue)
	{
		if(EndConnType == LOC_OUTPUT)
		{
			check(ConnType == LOC_INPUT);
			check(ConnIndex == 0);

			USoundNode* EndConnNode = CastChecked<USoundNode>(EndConnObj);
			SoundCue->FirstNode = EndConnNode;
		}
		return;
	}

	// Normal case - connecting an input of one node to the output of another.
	// JTODO: Loop detection!
	if(ConnType == LOC_INPUT && EndConnType == LOC_OUTPUT)
	{
		check( EndConnIndex == 0 );

		USoundNode* ConnNode = CastChecked<USoundNode>(ConnObj);
		USoundNode* EndConnNode = CastChecked<USoundNode>(EndConnObj);
		ConnectNodes(ConnNode, ConnIndex, EndConnNode);
	}
	else if(ConnType == LOC_OUTPUT && EndConnType == LOC_INPUT)
	{
		check( ConnIndex == 0 );

		USoundNode* ConnNode = CastChecked<USoundNode>(ConnObj);
		USoundNode* EndConnNode = CastChecked<USoundNode>(EndConnObj);
		ConnectNodes(EndConnNode, EndConnIndex, ConnNode);
	}
}

void WxSoundCueEditor::MakeConnectionToObject( UObject* EndObj )
{

}

void WxSoundCueEditor::MoveSelectedObjects( INT DeltaX, INT DeltaY )
{
	for(INT i=0; i<SelectedNodes.Num(); i++)
	{
		USoundNode* Node = SelectedNodes(i);
		FSoundNodeEditorData* EdData = SoundCue->EditorData.Find(Node);
		check(EdData);
		
		EdData->NodePosX += DeltaX;
		EdData->NodePosY += DeltaY;
	}
}

void WxSoundCueEditor::EdHandleKeyInput(FChildViewport* Viewport, FName Key, EInputEvent Event)
{
	if(Key == KEY_Delete)
	{
		DeleteSelectedNodes();
	}
}

void WxSoundCueEditor::ConnectNodes(USoundNode* ParentNode, INT ChildIndex, USoundNode* ChildNode)
{
	check( ChildIndex >= 0 && ChildIndex < ParentNode->ChildNodes.Num() );

	ParentNode->ChildNodes(ChildIndex) = ChildNode;

	ParentNode->PostEditChange(NULL);

	UpdatePropertyWindow();
	RefreshViewport();
}

void WxSoundCueEditor::DeleteSelectedNodes()
{
	for(INT i=0; i<SelectedNodes.Num(); i++)
	{
		USoundNode* DelNode = SelectedNodes(i);

		// We look through all nodes to see if there is a node that still references the one we want to delete.
		// If so, break the link.
		for( TMap<USoundNode*, FSoundNodeEditorData>::TIterator It(SoundCue->EditorData);It;++It )
		{
			USoundNode* ChkNode = It.Key();

			for(INT ChildIdx = 0; ChildIdx < ChkNode->ChildNodes.Num(); ChildIdx++)
			{
				if( ChkNode->ChildNodes(ChildIdx) == DelNode)
				{
					ChkNode->ChildNodes(ChildIdx) = NULL;
					ChkNode->PostEditChange(NULL);
				}
			}
		}

		// Also check the 'first node' pointer
		if(SoundCue->FirstNode == DelNode)
			SoundCue->FirstNode = NULL;

		// Remove this node from the SoundCue's map of all SoundNodes
		check( SoundCue->EditorData.Find(DelNode) );
		SoundCue->EditorData.Remove(DelNode);
	}

	SelectedNodes.Empty();

	UpdatePropertyWindow();
	RefreshViewport();
}

/*-----------------------------------------------------------------------------
	WxSoundCueEditor
-----------------------------------------------------------------------------*/

UBOOL				WxSoundCueEditor::bSoundNodeClassesInitialized = false;
TArray<UClass*>		WxSoundCueEditor::SoundNodeClasses;

BEGIN_EVENT_TABLE( WxSoundCueEditor, wxFrame )
	EVT_MENU_RANGE( IDM_SOUNDCUE_NEW_NODE_START, IDM_SOUNDCUE_NEW_NODE_END, WxSoundCueEditor::OnContextNewSoundNode )
	EVT_MENU( IDM_SOUNDCUE_NEW_WAVE, WxSoundCueEditor::OnContextNewWave )
	EVT_MENU( IDM_SOUNDCUE_ADD_INPUT, WxSoundCueEditor::OnContextAddInput )
	EVT_MENU( IDM_SOUNDCUE_DELETE_INPUT, WxSoundCueEditor::OnContextDeleteInput )
	EVT_MENU( IDM_SOUNDCUE_DELETE_NODE, WxSoundCueEditor::OnContextDeleteNode )
	EVT_MENU( IDM_SOUNDCUE_PLAY_NODE, WxSoundCueEditor::OnContextPlaySoundNode )
	EVT_MENU( IDM_SOUNDCUE_PLAY_CUE, WxSoundCueEditor::OnContextPlaySoundCue )
	EVT_MENU( IDM_SOUNDCUE_STOP_PLAYING, WxSoundCueEditor::OnContextStopPlaying )
	EVT_MENU( IDM_SOUNDCUE_BREAK_LINK, WxSoundCueEditor::OnContextBreakLink )
END_EVENT_TABLE()

IMPLEMENT_COMPARE_POINTER( UClass, SoundCueEditor, { return appStricmp( A->GetName(), B->GetName() ); } )

// Static functions that fills in array of all available USoundNode classes and sorts them alphabetically.
void WxSoundCueEditor::InitSoundNodeClasses()
{
	if(bSoundNodeClassesInitialized)
		return;

	// Construct list of non-abstract gameplay sequence object classes.
	for(TObjectIterator<UClass> It; It; ++It)
	{
		if( It->IsChildOf(USoundNode::StaticClass()) && !(It->ClassFlags & CLASS_Abstract) && !It->IsChildOf(USoundNodeWave::StaticClass()) )
		{
			SoundNodeClasses.AddItem(*It);
		}
	}

	Sort<USE_COMPARE_POINTER(UClass,SoundCueEditor)>( &SoundNodeClasses(0), SoundNodeClasses.Num() );

	bSoundNodeClassesInitialized = true;
}


WxSoundCueEditor::WxSoundCueEditor( wxWindow* InParent, wxWindowID InID, USoundCue* InSoundCue )
	: WxLinkedObjEd( InParent, InID, TEXT("SoundCueEditor") )
{
	SoundCue = InSoundCue;

	SetTitle( *FString::Printf( TEXT("SoundCue Editor: %s"), InSoundCue->GetName() ) );

	ToolBar = new WxSoundCueEdToolBar( this, -1 );
	SetToolBar( ToolBar );

	InitSoundNodeClasses();

	// Shift origin so origin is roughly in the middle. Would be nice to use Viewport size, but doesnt seem initialised here...
	LinkedObjVC->Origin2D.X = 150;
	LinkedObjVC->Origin2D.Y = 300;

	// Create a component/cue for previewing sounds
	PreviewSoundCue = ConstructObject<USoundCue>( USoundCue::StaticClass());
	PreviewAudioComponent = UAudioDevice::CreateComponent( SoundCue, GUnrealEd->Level, NULL, 0 );

	BackgroundTexture = LoadObject<UTexture2D>(NULL, TEXT("EditorMaterials.SoundCueBackground"), NULL, LOAD_NoFail, NULL);
}

WxSoundCueEditor::~WxSoundCueEditor()
{

}

void WxSoundCueEditor::OnContextNewSoundNode( wxCommandEvent& In )
{
	INT NewNodeIndex = In.GetId() - IDM_SOUNDCUE_NEW_NODE_START;	
	check( NewNodeIndex >= 0 && NewNodeIndex < SoundNodeClasses.Num() );

	UClass* NewNodeClass = SoundNodeClasses(NewNodeIndex);
	check( NewNodeClass->IsChildOf(USoundNode::StaticClass()) );

	USoundNode* NewNode = ConstructObject<USoundNode>( NewNodeClass, SoundCue, NAME_None);

    // If this node allows >0 children but by default has zero - create a connector for starters
	if( (NewNode->GetMaxChildNodes() > 0 || NewNode->GetMaxChildNodes() == -1) && NewNode->ChildNodes.Num() == 0 )
	{
		NewNode->InsertChildNode( NewNode->ChildNodes.Num() );
	}

	// Create new editor data struct and add to map in SoundCue.
	FSoundNodeEditorData NewEdData;
	appMemset(&NewEdData, 0, sizeof(FSoundNodeEditorData));

	NewEdData.NodePosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
	NewEdData.NodePosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;

	SoundCue->EditorData.Set(NewNode, NewEdData);

	RefreshViewport();
}

void WxSoundCueEditor::OnContextNewWave( wxCommandEvent& In )
{
	USoundNodeWave* NewWave = GSelectionTools.GetTop<USoundNodeWave>();
	if(!NewWave)
		return;

	// We can only have a particular SoundNodeWave in the SoundCue once.
	for( TMap<USoundNode*, FSoundNodeEditorData>::TIterator It(SoundCue->EditorData);It;++It )
	{
		USoundNode* ChkNode = It.Key();
		if(NewWave == ChkNode)
		{
			appMsgf(0, TEXT("Can only add a particular SoundNodeWave to a SoundCue once.") );
			return;
		}
	}

	// Create new editor data struct and add to map in SoundCue.
	FSoundNodeEditorData NewEdData;
	appMemset(&NewEdData, 0, sizeof(FSoundNodeEditorData));

	NewEdData.NodePosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
	NewEdData.NodePosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;

	SoundCue->EditorData.Set(NewWave, NewEdData);

	RefreshViewport();
}

void WxSoundCueEditor::OnContextAddInput( wxCommandEvent& In )
{
	INT NumSelected = SelectedNodes.Num();
	if(NumSelected != 1)
		return;

	USoundNode* Node = SelectedNodes(0);

	if( Node->GetMaxChildNodes() == -1 || (Node->ChildNodes.Num() < Node->GetMaxChildNodes()) )
	{
		Node->InsertChildNode( Node->ChildNodes.Num() );
	}

	Node->PostEditChange(NULL);

	UpdatePropertyWindow();
	RefreshViewport();
}

void WxSoundCueEditor::OnContextDeleteInput( wxCommandEvent& In )
{
	check(ConnType == LOC_INPUT);

	// Can't delete root input!
	if(ConnObj == SoundCue)
		return;

	USoundNode* ConnNode = CastChecked<USoundNode>(ConnObj);
	check( ConnIndex >= 0 && ConnIndex < ConnNode->ChildNodes.Num() );

	ConnNode->RemoveChildNode( ConnIndex );

	ConnNode->PostEditChange(NULL);

	UpdatePropertyWindow();
	RefreshViewport();
}

void WxSoundCueEditor::OnContextDeleteNode( wxCommandEvent& In )
{
	DeleteSelectedNodes();
}

void WxSoundCueEditor::OnContextPlaySoundNode( wxCommandEvent& In )
{
	if(SelectedNodes.Num() != 1)
		return;

	USoundNode* PreviewNode = SelectedNodes(0);

	PreviewAudioComponent->Stop();

	PreviewSoundCue->FirstNode						= PreviewNode;

	PreviewAudioComponent->SoundCue					= PreviewSoundCue;
	PreviewAudioComponent->bUseOwnerLocation		= 0;
	PreviewAudioComponent->bAutoDestroy				= 0;
	PreviewAudioComponent->Location					= FVector(0,0,0);
	PreviewAudioComponent->bNonRealtime				= 1;
	PreviewAudioComponent->bAllowSpatialization		= 0;

	PreviewAudioComponent->Play();	
}

void WxSoundCueEditor::OnContextPlaySoundCue( wxCommandEvent& In )
{
	PreviewAudioComponent->Stop();

	PreviewAudioComponent->SoundCue				= SoundCue;
	PreviewAudioComponent->bUseOwnerLocation	= 0;
	PreviewAudioComponent->bAutoDestroy			= 0;
	PreviewAudioComponent->Location				= FVector(0,0,0);
	PreviewAudioComponent->bNonRealtime			= 1;
	PreviewAudioComponent->bAllowSpatialization	= 0;

	PreviewAudioComponent->Play();	
}

void WxSoundCueEditor::OnContextStopPlaying( wxCommandEvent& In )
{
	PreviewAudioComponent->Stop();

	PreviewAudioComponent->SoundCue = NULL;
	PreviewAudioComponent->WaveInstances.Empty();
	PreviewAudioComponent->SoundNodeData.Empty();
	PreviewAudioComponent->SoundNodeWaveMap.Empty();
	PreviewAudioComponent->CurrentNotifyFinished = NULL;
}

void WxSoundCueEditor::OnContextBreakLink( wxCommandEvent& In )
{
	if( ConnObj == SoundCue )
	{
		SoundCue->FirstNode = NULL;
	}
	else
	{
		USoundNode* ConnNode = CastChecked<USoundNode>(ConnObj);

		check( ConnIndex >= 0 && ConnIndex < ConnNode->ChildNodes.Num() );
		ConnNode->ChildNodes(ConnIndex) = NULL;

		ConnNode->PostEditChange(NULL);
	}

	UpdatePropertyWindow();
	RefreshViewport();
}

/*-----------------------------------------------------------------------------
	WxSoundCueEdToolBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxSoundCueEdToolBar, wxToolBar )
END_EVENT_TABLE()

WxSoundCueEdToolBar::WxSoundCueEdToolBar( wxWindow* InParent, wxWindowID InID )
: wxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_3DBUTTONS )
{
	PlayCueB.Load( TEXT("SCE_PlayCue") );
	PlayNodeB.Load( TEXT("SCE_PlayNode") );
	StopB.Load( TEXT("SCE_Stop") );
	SetToolBitmapSize( wxSize( 18, 18 ) );

	AddTool( IDM_SOUNDCUE_STOP_PLAYING, StopB, TEXT("Stop") );
	AddTool( IDM_SOUNDCUE_PLAY_NODE, PlayNodeB, TEXT("Play Selected Node") );
	AddTool( IDM_SOUNDCUE_PLAY_CUE, PlayCueB, TEXT("Play SoundCue") );

	Realize();
}

WxSoundCueEdToolBar::~WxSoundCueEdToolBar()
{
}

/*-----------------------------------------------------------------------------
	WxMBSoundCueEdNewNode.
-----------------------------------------------------------------------------*/

WxMBSoundCueEdNewNode::WxMBSoundCueEdNewNode(WxSoundCueEditor* CueEditor)
{
	USoundNodeWave* SelectedWave = GSelectionTools.GetTop<USoundNodeWave>();
	if(SelectedWave)
	{
		Append( IDM_SOUNDCUE_NEW_WAVE, *FString::Printf( TEXT("SoundNodeWave: %s"), *SelectedWave->GetFName() ), TEXT("") );
		AppendSeparator();
	}

	for(INT i=0; i<CueEditor->SoundNodeClasses.Num(); i++)
	{
		Append( IDM_SOUNDCUE_NEW_NODE_START+i, *CueEditor->SoundNodeClasses(i)->GetDescription(), TEXT("") );
	}
}

WxMBSoundCueEdNewNode::~WxMBSoundCueEdNewNode()
{

}

/*-----------------------------------------------------------------------------
	WxMBSoundCueEdNodeOptions.
-----------------------------------------------------------------------------*/

WxMBSoundCueEdNodeOptions::WxMBSoundCueEdNodeOptions(WxSoundCueEditor* CueEditor)
{
	INT NumSelected = CueEditor->SelectedNodes.Num();

	if(NumSelected == 1)
	{
		// See if we adding another input would exceed max child nodes.
		USoundNode* Node = CueEditor->SelectedNodes(0);
		if( Node->GetMaxChildNodes() == -1 || (Node->ChildNodes.Num() < Node->GetMaxChildNodes()) )
			Append( IDM_SOUNDCUE_ADD_INPUT, TEXT("Add Input"), TEXT("") );
	}

	Append( IDM_SOUNDCUE_DELETE_NODE, TEXT("Delete Selected Node(s)"), TEXT("") );

	if(NumSelected == 1)
		Append( IDM_SOUNDCUE_PLAY_NODE, TEXT("Play Selected Node"), TEXT("") );
}

WxMBSoundCueEdNodeOptions::~WxMBSoundCueEdNodeOptions()
{

}

/*-----------------------------------------------------------------------------
	WxMBSoundCueEdConnectorOptions.
-----------------------------------------------------------------------------*/

WxMBSoundCueEdConnectorOptions::WxMBSoundCueEdConnectorOptions(WxSoundCueEditor* CueEditor)
{
	// Only display the 'Break Link' option if there is a link to break!
	UBOOL bHasConnection = false;
	if( CueEditor->ConnType == LOC_INPUT )
	{
		if( CueEditor->ConnObj == CueEditor->SoundCue )
		{
			if( CueEditor->SoundCue->FirstNode )
				bHasConnection = true;
		}
		else
		{
			USoundNode* ConnNode = CastChecked<USoundNode>(CueEditor->ConnObj);

			if( ConnNode->ChildNodes(CueEditor->ConnIndex) )
				bHasConnection = true;
		}
	}

	if(bHasConnection)
		Append( IDM_SOUNDCUE_BREAK_LINK, TEXT("Break Link"), TEXT("") );

	// If on an input that can be deleted, show option
	if(CueEditor->ConnType == LOC_INPUT && CueEditor->ConnObj != CueEditor->SoundCue)
		Append( IDM_SOUNDCUE_DELETE_INPUT, TEXT("Delete Input"), TEXT("") );
}

WxMBSoundCueEdConnectorOptions::~WxMBSoundCueEdConnectorOptions()
{

}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
