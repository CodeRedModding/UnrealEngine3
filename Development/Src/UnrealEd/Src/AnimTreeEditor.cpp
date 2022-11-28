/*=============================================================================
	AnimTreeEditor.cpp: SoundCue editing
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "EngineAnimClasses.h"
#include "UnLinkedObjDrawUtils.h"
#include "AnimTreeEditor.h"

void WxAnimTreeEditor::OpenNewObjectMenu()
{
	wxMenu* menu = new WxMBAnimTreeEdNewNode(this);

	FTrackPopupMenu tpm( this, menu );
	tpm.Show();
	delete menu;
}

void WxAnimTreeEditor::OpenObjectOptionsMenu()
{
	wxMenu* menu = new WxMBAnimTreeEdNodeOptions(this);

	FTrackPopupMenu tpm( this, menu );
	tpm.Show();
	delete menu;
}

void WxAnimTreeEditor::OpenConnectorOptionsMenu()
{
	wxMenu* menu = new WxMBAnimTreeEdConnectorOptions(this);

	FTrackPopupMenu tpm( this, menu );
	tpm.Show();
	delete menu;
}

void WxAnimTreeEditor::DrawLinkedObjects(FRenderInterface* RI)
{
	WxLinkedObjEd::DrawLinkedObjects(RI);

	for(INT i=0; i<TreeNodes.Num(); i++)
	{
		UBOOL bNodeSelected = SelectedNodes.ContainsItem( TreeNodes(i) );

		TreeNodes(i)->DrawAnimNode(RI, bNodeSelected);

	}
}

void WxAnimTreeEditor::UpdatePropertyWindow()
{
	PropertyWindow->SetObjectArray( (TArray<UObject*>*)&SelectedNodes, 1,1,0 );
}

void WxAnimTreeEditor::EmptySelection()
{
	SelectedNodes.Empty();
}

void WxAnimTreeEditor::AddToSelection( UObject* Obj )
{
	check( Obj->IsA(UAnimNode::StaticClass()) );

	if( SelectedNodes.ContainsItem( (UAnimNode*)Obj ) )
		return;

	SelectedNodes.AddItem( (UAnimNode*)Obj );
}

UBOOL WxAnimTreeEditor::IsInSelection( UObject* Obj )
{
	check( Obj->IsA(UAnimNode::StaticClass()) );

	return SelectedNodes.ContainsItem( (UAnimNode*)Obj );
}

UBOOL WxAnimTreeEditor::HaveObjectsSelected()
{
	return( SelectedNodes.Num() > 0 );
}

void WxAnimTreeEditor::SetSelectedConnector( UObject* InConnObj, INT InConnType, INT InConnIndex )
{
	check( InConnObj->IsA(UAnimNode::StaticClass()) );

	ConnObj = InConnObj;
	ConnType = InConnType;
	ConnIndex = InConnIndex;
}

FIntPoint WxAnimTreeEditor::GetSelectedConnLocation(FRenderInterface* RI)
{
	if(!ConnObj)
		return FIntPoint(0,0);

	UAnimNode* AnimNode = CastChecked<UAnimNode>(ConnObj);
	return AnimNode->GetConnectionLocation(ConnType, ConnIndex);
}

void WxAnimTreeEditor::MakeConnectionToConnector( UObject* EndConnObj, INT EndConnType, INT EndConnIndex )
{
	// Avoid connections to yourself.
	if(!EndConnObj || !ConnObj || EndConnObj == ConnObj)
		return;

	// Normal case - connecting an input of one node to the output of another.
	if(ConnType == LOC_INPUT && EndConnType == LOC_OUTPUT)
	{
		check( ConnIndex == 0 );

		UAnimNode* ChildNode = CastChecked<UAnimNode>(ConnObj);
		UAnimNodeBlendBase* ParentNode = CastChecked<UAnimNodeBlendBase>(EndConnObj); // Only blend nodes can have LOC_OUTPUT connectors

		check( EndConnIndex >= 0 && EndConnIndex < ParentNode->Children.Num() );
		ParentNode->Children(EndConnIndex).Anim = ChildNode;
	}
	else if(ConnType == LOC_OUTPUT && EndConnType == LOC_INPUT)
	{
		check( EndConnIndex == 0 );

		UAnimNodeBlendBase* ParentNode = CastChecked<UAnimNodeBlendBase>(ConnObj);
		UAnimNode* ChildNode = CastChecked<UAnimNode>(EndConnObj);
		
		check( ConnIndex >= 0 && ConnIndex < ParentNode->Children.Num() );
		ParentNode->Children(ConnIndex).Anim = ChildNode;
	}
}

void WxAnimTreeEditor::MakeConnectionToObject( UObject* EndObj )
{

}

void WxAnimTreeEditor::MoveSelectedObjects( INT DeltaX, INT DeltaY )
{
	for(INT i=0; i<SelectedNodes.Num(); i++)
	{
		UAnimNode* Node = SelectedNodes(i);
		
		Node->NodePosX += DeltaX;
		Node->NodePosY += DeltaY;
	}
}

void WxAnimTreeEditor::EdHandleKeyInput(FChildViewport* Viewport, FName Key, EInputEvent Event)
{
	if(Key == KEY_Delete)
	{
		DeleteSelectedNodes();
	}
}

/*-----------------------------------------------------------------------------
	WxAnimTreeEditor
-----------------------------------------------------------------------------*/

UBOOL				WxAnimTreeEditor::bAnimNodeClassesInitialized = false;
TArray<UClass*>		WxAnimTreeEditor::AnimNodeClasses;

BEGIN_EVENT_TABLE( WxAnimTreeEditor, wxFrame )
	EVT_MENU_RANGE( IDM_ANIMTREE_NEW_NODE_START, IDM_ANIMTREE_NEW_NODE_END, WxAnimTreeEditor::OnNewAnimNode )
	EVT_MENU( IDM_ANIMTREE_DELETE_NODE, WxAnimTreeEditor::OnDeleteNodes )
	EVT_MENU( IDM_ANIMTREE_BREAK_LINK, WxAnimTreeEditor::OnBreakLink )
	EVT_MENU( IDM_ANIMTREE_ADD_INPUT, WxAnimTreeEditor::OnAddInput )
	EVT_MENU( IDM_ANIMTREE_DELETE_INPUT, WxAnimTreeEditor::OnRemoveInput )
END_EVENT_TABLE()

IMPLEMENT_COMPARE_POINTER( UClass, AnimTreeEditor, { return appStricmp( A->GetName(), B->GetName() ); } )

// Static functions that fills in array of all available USoundNode classes and sorts them alphabetically.
void WxAnimTreeEditor::InitAnimNodeClasses()
{
	if(bAnimNodeClassesInitialized)
		return;

	// Construct list of non-abstract gameplay sequence object classes.
	for(TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;

		if( (Class->IsChildOf(UAnimNode::StaticClass())) && !(Class->ClassFlags & CLASS_Abstract) && !(Class->ClassFlags & CLASS_Hidden) && !(Class->IsChildOf(UAnimTree::StaticClass())) )
		{
			AnimNodeClasses.AddItem(Class);
		}
	}

	Sort<USE_COMPARE_POINTER(UClass,AnimTreeEditor)>( &AnimNodeClasses(0), AnimNodeClasses.Num() );

	bAnimNodeClassesInitialized = true;
}


WxAnimTreeEditor::WxAnimTreeEditor( wxWindow* InParent, wxWindowID InID, class UAnimTree* InAnimTree )
: WxLinkedObjEd( InParent, InID, TEXT("AnimTreeEditor") )
{
	AnimTree = InAnimTree;

	SetTitle( *FString::Printf( TEXT("AnimTree Editor: %s"), AnimTree->GetName() ) );

	InitAnimNodeClasses();

	// Shift origin so origin is roughly in the middle. Would be nice to use Viewport size, but doesnt seem initialised here...
	LinkedObjVC->Origin2D.X = 150;
	LinkedObjVC->Origin2D.Y = 300;

	BackgroundTexture = LoadObject<UTexture2D>(NULL, TEXT("EditorMaterials.SoundCueBackground"), NULL, LOAD_NoFail, NULL);

	// Build list of all AnimNodes in the tree.
	AnimTree->GetNodes(TreeNodes);
}

WxAnimTreeEditor::~WxAnimTreeEditor()
{

}

//////////////////////////////////////////////////////////////////
// Menu Handlers
//////////////////////////////////////////////////////////////////

void WxAnimTreeEditor::OnNewAnimNode(wxCommandEvent& In)
{
	INT NewNodeClassIndex = In.GetId() - IDM_ANIMTREE_NEW_NODE_START;
	check( NewNodeClassIndex >= 0 && NewNodeClassIndex < AnimNodeClasses.Num() );

	UClass* NewNodeClass = AnimNodeClasses(NewNodeClassIndex);
	check( NewNodeClass->IsChildOf(UAnimNode::StaticClass()) );

	UAnimNode* NewNode = ConstructObject<UAnimNode>( NewNodeClass, AnimTree, NAME_None);
	NewNode->NodePosX = LinkedObjVC->NewX - LinkedObjVC->Origin2D.X;
	NewNode->NodePosY = LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y;

	TreeNodes.AddItem(NewNode);

	RefreshViewport();
}

void WxAnimTreeEditor::OnBreakLink(wxCommandEvent& In)
{
	if(!ConnObj || ConnType != LOC_OUTPUT)
		return;

	// Only blend nodes have outputs..
	UAnimNodeBlendBase* BlendNode = CastChecked<UAnimNodeBlendBase>(ConnObj);

	check(ConnIndex >= 0 || ConnIndex < BlendNode->Children.Num() );

	BlendNode->Children(ConnIndex).Anim = NULL;

	RefreshViewport();
}

void WxAnimTreeEditor::OnAddInput(wxCommandEvent& In)
{
	INT NumSelected = SelectedNodes.Num();
	if(NumSelected == 1)
	{
		UAnimNodeBlendBase* BlendNode = Cast<UAnimNodeBlendBase>(SelectedNodes(0));
		if(BlendNode && !BlendNode->bFixNumChildren)
		{
			INT NewChildIndex = BlendNode->Children.AddZeroed();

			FString NewChildName = FString::Printf( TEXT("Child%d"), NewChildIndex + 1 );
			BlendNode->Children( NewChildIndex ).Name = FName( *NewChildName );
		}
	}

	RefreshViewport();
}

void WxAnimTreeEditor::OnRemoveInput(wxCommandEvent& In)
{
	if(!ConnObj || ConnType != LOC_OUTPUT)
		return;

	// Only blend nodes have outputs..
	UAnimNodeBlendBase* BlendNode = CastChecked<UAnimNodeBlendBase>(ConnObj);

	// Do nothing if not allowed to modify connectors
	if(BlendNode->bFixNumChildren)
		return;

	check(ConnIndex >= 0 || ConnIndex < BlendNode->Children.Num() );

	BlendNode->Children.Remove( ConnIndex );

	RefreshViewport();
}

void WxAnimTreeEditor::OnDeleteNodes(wxCommandEvent& In)
{
	DeleteSelectedNodes();
}

//////////////////////////////////////////////////////////////////
// Utils
//////////////////////////////////////////////////////////////////

void WxAnimTreeEditor::DeleteSelectedNodes()
{
	for(INT i=0; i<SelectedNodes.Num(); i++)
	{
		UAnimNode* DelNode = SelectedNodes(i);

		// Search all nodes for links to the one we are about to delete and zero them if so.
		for( INT j=0; j<TreeNodes.Num(); j++ )
		{
			// Only classes of UAnimNodeBlendBase contain links to other nodes.
			UAnimNodeBlendBase* ChkNode = Cast<UAnimNodeBlendBase>( TreeNodes(j) );
			if(ChkNode)
			{
				for(INT ChildIdx = 0; ChildIdx < ChkNode->Children.Num(); ChildIdx++)
				{
					if( ChkNode->Children(ChildIdx).Anim == DelNode)
					{
						ChkNode->Children(ChildIdx).Anim = NULL;
					}
				}
			}
		}

		// Remove this node from the SoundCue's map of all SoundNodes
		check( TreeNodes.ContainsItem(DelNode) );
		TreeNodes.RemoveItem(DelNode);
	}

	SelectedNodes.Empty();

	UpdatePropertyWindow();
	RefreshViewport();
}

/*-----------------------------------------------------------------------------
	WxMBAnimTreeEdNewNode.
-----------------------------------------------------------------------------*/

WxMBAnimTreeEdNewNode::WxMBAnimTreeEdNewNode(WxAnimTreeEditor* AnimTreeEd)
{
	INT SeqClassIndex = INDEX_NONE;

	for(INT i=0; i<AnimTreeEd->AnimNodeClasses.Num(); i++)
	{
		// Add AnimNodeSequence option at bottom of list because its special.
		if( AnimTreeEd->AnimNodeClasses(i) == UAnimNodeSequence::StaticClass() )
		{
			SeqClassIndex = i;
		}
		else
		{
			Append( IDM_ANIMTREE_NEW_NODE_START+i, *AnimTreeEd->AnimNodeClasses(i)->GetDescription(), TEXT("") );
		}
	}

	check( SeqClassIndex != INDEX_NONE );

	AppendSeparator();
	Append( IDM_ANIMTREE_NEW_NODE_START+SeqClassIndex, TEXT("AnimSequence Player"), TEXT("") );
}

WxMBAnimTreeEdNewNode::~WxMBAnimTreeEdNewNode()
{

}

/*-----------------------------------------------------------------------------
	EMBAnimTreeEdNodeOptions.
-----------------------------------------------------------------------------*/

WxMBAnimTreeEdNodeOptions::WxMBAnimTreeEdNodeOptions(WxAnimTreeEditor* AnimTreeEd)
{
	INT NumSelected = AnimTreeEd->SelectedNodes.Num();
	if(NumSelected == 1)
	{
		// See if we adding another input would exceed max child nodes.
		UAnimNodeBlendBase* BlendNode = Cast<UAnimNodeBlendBase>( AnimTreeEd->SelectedNodes(0) );
		if( BlendNode && !BlendNode->bFixNumChildren )
		{
			Append( IDM_ANIMTREE_ADD_INPUT, TEXT("Add Input"), TEXT("") );
		}
	}

	if( NumSelected == 1 )
	{
		Append( IDM_ANIMTREE_DELETE_NODE, TEXT("Delete Selected Node"), TEXT("") );
	}
	else
	{
		Append( IDM_ANIMTREE_DELETE_NODE, TEXT("Delete Selected Nodes"), TEXT("") );
	}
}

WxMBAnimTreeEdNodeOptions::~WxMBAnimTreeEdNodeOptions()
{

}

/*-----------------------------------------------------------------------------
	EMBAnimTreeEdConnectorOptions.
-----------------------------------------------------------------------------*/

WxMBAnimTreeEdConnectorOptions::WxMBAnimTreeEdConnectorOptions(WxAnimTreeEditor* AnimTreeEd)
{
	UAnimNodeBlendBase* BlendNode = Cast<UAnimNodeBlendBase>(AnimTreeEd->ConnObj);

	// Only display the 'Break Link' option if there is a link to break!
	UBOOL bHasConnection = false;
	if( AnimTreeEd->ConnType == LOC_OUTPUT )
	{
		if(BlendNode)
		{
			check( AnimTreeEd->ConnIndex >= 0 && AnimTreeEd->ConnIndex < BlendNode->Children.Num() );

			if( BlendNode->Children(AnimTreeEd->ConnIndex).Anim )
			{
				bHasConnection = true;
			}
		}
	}

	if(bHasConnection)
	{
		Append( IDM_ANIMTREE_BREAK_LINK, TEXT("Break Link"), TEXT("") );
	}

	// If on an input that can be deleted, show option
	if(BlendNode && AnimTreeEd->ConnType == LOC_OUTPUT && !BlendNode->bFixNumChildren)
	{
		Append( IDM_ANIMTREE_DELETE_INPUT, TEXT("Delete Input"), TEXT("") );
	}
}

WxMBAnimTreeEdConnectorOptions::~WxMBAnimTreeEdConnectorOptions()
{

}

/*-----------------------------------------------------------------------------
The End.
-----------------------------------------------------------------------------*/
