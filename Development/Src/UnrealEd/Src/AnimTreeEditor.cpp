/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "EngineAnimClasses.h"
#include "UnLinkedObjDrawUtils.h"
#include "AnimTreeEditor.h"
#include "PropertyWindow.h"
#include "DlgGenericComboEntry.h"
#include "Factories.h"
#include "ScopedObjectStateChange.h"


IMPLEMENT_CLASS(UAnimTreeEdSkelComponent);
IMPLEMENT_CLASS(UAnimNodeEditInfo);

IMPLEMENT_DYNAMIC_CLASS(WxAnimTreeEditor, WxTrackableFrame);

static INT	DuplicateOffset = 30;

/*-----------------------------------------------------------------------------
	WxMBAnimTreeEdNewNode
-----------------------------------------------------------------------------*/
class WxMBAnimTreeEdNewNode : public wxMenu
{
	typedef TMap<UClass*,wxMenu*>	TClassToMenuMap;
	typedef TMap<FString,wxMenu*>	TStringToMenuMap;
	
	wxMenu * AnimMenu;
	wxMenu * SkelControlMenu;
	wxMenu * MorphMenu;
	wxMenu * AnimSequenceMenu;

	UBOOL AddToMenu(const TClassToMenuMap& MenuMap, const TMap<wxMenu*,TStringToMenuMap*>& SubmenuMap, UObject * Object, UINT IDMIdx)
	{
		// first look up the menu type
		wxMenu* Menu = NULL;
		for (TClassToMenuMap::TConstIterator It(MenuMap); It; ++It)
		{
			UClass *TestClass = It.Key();
			if (Object->GetClass()->IsChildOf(TestClass))
			{
				Menu = It.Value();
				break;
			}
		}

		if (Menu != NULL)
		{
			FString CategoryName, ObjName;

			// check for a category
			if (GetCategoryDescription(Object,CategoryName,ObjName))
			{
				// look up the submenu map
				TStringToMenuMap* Submenus = SubmenuMap.FindRef(Menu);
				if (Submenus != NULL)
				{
					wxMenu* Submenu = Submenus->FindRef(CategoryName);
					if (Submenu == NULL)
					{
						// create a new submenu for the category
						Submenu = new wxMenu();
						// add it to the map
						Submenus->Set(*CategoryName,Submenu);
						// and add it to the parent menu
						Menu->Append(IDM_ANIMTREE_NEW_NODE_CATEGORY_START+(Submenus->Num()),*CategoryName, Submenu);
					}
					// add the object to the submenu
					Submenu->Append(IDMIdx,*ObjName,TEXT(""));
					return TRUE;
				}
			}
		}

		return FALSE;
	}
public:
	WxMBAnimTreeEdNewNode(WxAnimTreeEditor* AnimTreeEd)
	{
		// create the top level menus
		AnimMenu = new wxMenu();
		SkelControlMenu = new wxMenu();
		MorphMenu = new wxMenu();
		AnimSequenceMenu = new wxMenu();

		// set up map of sequence object classes to their respective menus
		TClassToMenuMap MenuMap;
		// add sequence first so that sequence is checked before animnode
		MenuMap.Set(UAnimNodeSequence::StaticClass(),AnimSequenceMenu);
		MenuMap.Set(UAnimNode::StaticClass(),AnimMenu);
		MenuMap.Set(USkelControlBase::StaticClass(),SkelControlMenu);
		MenuMap.Set(UMorphNodeBase::StaticClass(),MorphMenu);

		// list of categories for all the submenus
		TStringToMenuMap AnimSubmenus;
		TStringToMenuMap SkelControlSubmenus;
		TStringToMenuMap MorphSubmenus;
		TStringToMenuMap AnimSequenceSubmenu;

		// map all the submenus to their parent menu
		TMap<wxMenu*,TStringToMenuMap*> SubmenuMap;
		SubmenuMap.Set(AnimMenu,&AnimSubmenus);
		SubmenuMap.Set(SkelControlMenu,&SkelControlSubmenus);
		SubmenuMap.Set(MorphMenu,&MorphSubmenus);
		SubmenuMap.Set(AnimSequenceMenu,&AnimSequenceSubmenu);

		Append(IDM_ANIMTREE_NEW_ANIM, *LocalizeUnrealEd("NewAnimNode"), AnimMenu);
		Append(IDM_ANIMTREE_NEW_CONTROL, *LocalizeUnrealEd("NewControlBase"), SkelControlMenu);
		Append(IDM_ANIMTREE_NEW_MORPH, *LocalizeUnrealEd("NewMorphNode"), MorphMenu);
		AppendSeparator();
		Append( IDM_ANIMTREE_NEW_ANIMSEQ, *LocalizeUnrealEd("NewAnimSequence"), AnimSequenceMenu );

		AppendSeparator();
		Append( IDM_ANIMTREE_NEW_COMMENT, *LocalizeUnrealEd("NewComment"), TEXT("") );

		AppendSeparator();
		if (AnimTreeEd->GetNumSelected() > 0)
		{
			Append( IDM_ANIMTREE_EDIT_COPY, *LocalizeUnrealEd("AnimTreeCopy"), TEXT("")  );
			Append( IDM_ANIMTREE_EDIT_DUPLICATE, *LocalizeUnrealEd("AnimTreeDuplicate"), TEXT("")  );
		}

		Append( IDM_ANIMTREE_EDIT_PASTE, *LocalizeUnrealEd("AnimTreePaste"), TEXT("")  );
		
		// these are the ones added at the end
		TMap<INT, FString> Items1, Items2;
		// Do not add separator if no category is added yet.
		UBOOL	bAddSeparator1=FALSE, bAddSeparator2=FALSE;

		///////// Add animation classes.
		// Items1 : AnimNode
		// Items2 : AnimNodeSequences
		for(INT i=0; i<AnimTreeEd->AnimNodeClasses.Num(); i++)
		{
			UClass * AnimClass = AnimTreeEd->AnimNodeClasses(i);
			if ( AddToMenu(MenuMap, SubmenuMap, AnimClass->GetDefaultObject(), IDM_ANIMTREE_NEW_ANIM_START+i)==FALSE )
			{
				// Extra Items that don't belong to category, attach later. 
				if ( AnimClass->IsChildOf(UAnimNodeSequence::StaticClass()) )
				{
					// anim sequence
					Items2.Set(IDM_ANIMTREE_NEW_ANIM_START+i, AnimClass->GetDescription());
				}
				else
				{
					// all others
					Items1.Set(IDM_ANIMTREE_NEW_ANIM_START+i, AnimClass->GetDescription());
				}
			}
			else
			{
				if ( AnimClass->IsChildOf(UAnimNodeSequence::StaticClass()) )
				{
					bAddSeparator2 = TRUE;
				}
				else
				{
					bAddSeparator1 = TRUE;
				}
			}
		}

		// Extra Items that don't belong to category, attach later. 
		if (bAddSeparator1)
		{
			AnimMenu->AppendSeparator();
		}

		for (TMap<INT, FString>::TConstIterator Iter(Items1); Iter; ++Iter)
		{
			AnimMenu->Append(Iter.Key(), *Iter.Value(), TEXT(""));
		}

		if (bAddSeparator2)
		{
			AnimSequenceMenu->AppendSeparator();
		}

		for (TMap<INT, FString>::TConstIterator Iter(Items2); Iter; ++Iter)
		{
			AnimSequenceMenu->Append(Iter.Key(), *Iter.Value(), TEXT(""));
		}

		Items1.Empty(); 
		Items2.Empty();
		bAddSeparator1 = bAddSeparator2 = FALSE;

		///////// Add morph target classes.
		for(INT i=0; i<AnimTreeEd->MorphNodeClasses.Num(); i++)
		{
			if ( AddToMenu(MenuMap, SubmenuMap, AnimTreeEd->MorphNodeClasses(i)->GetDefaultObject(), IDM_ANIMTREE_NEW_MORPH_START+i)==FALSE )
			{
				Items1.Set(IDM_ANIMTREE_NEW_MORPH_START+i, AnimTreeEd->MorphNodeClasses(i)->GetDescription());
			}
			else
			{
				bAddSeparator1 = TRUE;
			}
		}

		// Extra Items that don't belong to category, attach later. 
		if ( bAddSeparator1 )
		{
			MorphMenu->AppendSeparator();
		}

		for (TMap<INT, FString>::TConstIterator Iter(Items1); Iter; ++Iter)
		{
			MorphMenu->Append(Iter.Key(), *Iter.Value(), TEXT(""));
		}

		Items1.Empty();
		bAddSeparator1 = FALSE;

		////////// Add SkelControl classes
		for(INT i=0; i<AnimTreeEd->SkelControlClasses.Num(); i++)
		{
			if ( AddToMenu(MenuMap, SubmenuMap, AnimTreeEd->SkelControlClasses(i)->GetDefaultObject(), IDM_ANIMTREE_NEW_CONTROL_START+i)==FALSE )
			{
				Items1.Set(IDM_ANIMTREE_NEW_CONTROL_START+i, AnimTreeEd->SkelControlClasses(i)->GetDescription());
			}
			else
			{
				bAddSeparator1 = TRUE;
			}
		}

		// Extra Items that don't belong to category, attach later. 
		if (bAddSeparator1)
		{
			SkelControlMenu->AppendSeparator();
		}

		for (TMap<INT, FString>::TConstIterator Iter(Items1); Iter; ++Iter)
		{
			SkelControlMenu->Append(Iter.Key(), *Iter.Value(), TEXT(""));
		}
	}

	/**
	* Looks for a category in the op's name, returns TRUE if one is found.
	*/
	static UBOOL GetCategoryDescription(UObject *Obj, FString &CategoryName, FString &ObjName)
	{
		if (Obj)
		{
			ObjName = Obj->GetClass()->GetDescription();

			UAnimNode * AnimNode = Cast<UAnimNode>(Obj);

			if (AnimNode)
			{
				if (AnimNode->CategoryDesc.Len() > 0)
				{
					CategoryName = AnimNode->CategoryDesc;
					return TRUE;
				}

				return FALSE;
			}

			USkelControlBase * SkelControl = Cast<USkelControlBase>(Obj);

			if (SkelControl)
			{
				if (SkelControl->CategoryDesc.Len() > 0)
				{
					CategoryName = SkelControl->CategoryDesc;
					return TRUE;
				}

				return FALSE;
			}

			UMorphNodeBase* MorphNode= Cast<UMorphNodeBase>(Obj);

			if (MorphNode)
			{
				if (MorphNode->CategoryDesc.Len() > 0)
				{
					CategoryName = MorphNode->CategoryDesc;
					return TRUE;
				}
				
				return FALSE;
			}
		}

		return FALSE;
	}
};

/*-----------------------------------------------------------------------------
	WxMBAnimTreeEdNodeOptions
-----------------------------------------------------------------------------*/

class WxMBAnimTreeEdNodeOptions : public wxMenu
{
public:
	WxMBAnimTreeEdNodeOptions(WxAnimTreeEditor* AnimTreeEd)
	{
		UBOOL bAddSeparator=FALSE;
		INT NumSelected = AnimTreeEd->GetNumSelectedByClass<UAnimNode>();
		if(NumSelected == 1)
		{
			UAnimNode * AnimNode = AnimTreeEd->GetFirstSelectedNodeByClass<UAnimNode>();

			// See if we adding another input would exceed max child nodes.
			UAnimNodeBlendBase* BlendNode = Cast<UAnimNodeBlendBase>( AnimNode );
			if( BlendNode && !BlendNode->bFixNumChildren )
			{
				Append( IDM_ANIMTREE_ADD_INPUT, *LocalizeUnrealEd("AddInput"), TEXT("") );
				bAddSeparator = TRUE;
			}

			if( AnimNode->IsA(UAnimTree::StaticClass()) )
			{
				Append( IDM_ANIMTREE_ADD_CONTROLHEAD, *LocalizeUnrealEd("AddSkelControlChain") );
				bAddSeparator = TRUE;
			}

			if ( bAddSeparator )
			{
				AppendSeparator();
			}
		}
		else if (AnimTreeEd->SelectedNodes.Num() == 1)
		{
			// Add 'apply style' menu
			UAnimNodeFrame *SelectedComment = Cast<UAnimNodeFrame>(AnimTreeEd->SelectedNodes(0));
			if(SelectedComment)
			{
				Append(IDM_ANIMTREE_COMMENT_TO_FRONT, *LocalizeUnrealEd("CommentToFront"), TEXT(""));
				Append(IDM_ANIMTREE_COMMENT_TO_BACK, *LocalizeUnrealEd("CommentToBack"), TEXT(""));

				AppendSeparator();
			}
		}

		Append( IDM_ANIMTREE_NEW_COMMENT, *LocalizeUnrealEd("NewComment"), TEXT("") );
		AppendSeparator();

		Append( IDM_ANIMTREE_BREAK_ALL_LINKS, *LocalizeUnrealEd("BreakAllLinks"), TEXT("") );
		AppendSeparator();

		if( AnimTreeEd->GetNumSelected() == 1 )
		{
			Append( IDM_ANIMTREE_DELETE_NODE, *LocalizeUnrealEd("DeleteSelectedItem"), TEXT("") );
		}
		else
		{
			Append( IDM_ANIMTREE_DELETE_NODE, *LocalizeUnrealEd("DeleteSelectedItems"), TEXT("") );
		}

		AppendSeparator();
		if (AnimTreeEd->GetNumSelected() > 0)
		{
			Append( IDM_ANIMTREE_EDIT_COPY, *LocalizeUnrealEd("AnimTreeCopy"), TEXT("")  );
			Append( IDM_ANIMTREE_EDIT_DUPLICATE, *LocalizeUnrealEd("AnimTreeDuplicate"), TEXT("")  );
		}

		Append( IDM_ANIMTREE_EDIT_PASTE, *LocalizeUnrealEd("AnimTreePaste"), TEXT("")  );
	}
};

/*-----------------------------------------------------------------------------
	WxMBAnimTreeEdConnectorOptions
-----------------------------------------------------------------------------*/

class WxMBAnimTreeEdConnectorOptions : public wxMenu
{
public:
	WxMBAnimTreeEdConnectorOptions(WxAnimTreeEditor* AnimTreeEd)
	{
		UAnimTree* Tree = Cast<UAnimTree>(AnimTreeEd->ConnObj);
		UAnimNodeBlendBase* BlendNode = Cast<UAnimNodeBlendBase>(AnimTreeEd->ConnObj);
		USkelControlBase* SkelControl = Cast<USkelControlBase>(AnimTreeEd->ConnObj);
		UMorphNodeWeightBase* MorphNode = Cast<UMorphNodeWeightBase>(AnimTreeEd->ConnObj);

		// Only display the 'Break Link' option if there is a link to break!
		UBOOL bHasNodeConnection = false;
		UBOOL bHasControlConnection = false;
		UBOOL bHasMorphConnection = false;
		if( AnimTreeEd->ConnType == LOC_OUTPUT )
		{
			if(Tree)
			{
				// Animation
				if(AnimTreeEd->ConnIndex == 0)
				{
					if( Tree->Children(0).Anim )
					{
						bHasNodeConnection = true;
					}
				}
				// Morph
				else if(AnimTreeEd->ConnIndex == 1)
				{
					if( Tree->RootMorphNodes.Num() > 0 )
					{
						bHasMorphConnection = true;
					}
				}
				// Controls
				else
				{
					if( Tree->SkelControlLists(AnimTreeEd->ConnIndex-2).ControlHead )
					{
						bHasControlConnection = true;
					}
				}
			}
			else if(BlendNode)
			{		
				if( BlendNode->Children(AnimTreeEd->ConnIndex).Anim )
				{
					bHasNodeConnection = true;
				}
			}
			else if(SkelControl)
			{
				if(SkelControl->NextControl)
				{
					bHasControlConnection = true;
				}
			}
			else if(MorphNode)
			{
				if(MorphNode->NodeConns(AnimTreeEd->ConnIndex).ChildNodes.Num() > 0)
				{
					bHasMorphConnection = true;
				}
			}
		}

		if(bHasNodeConnection || bHasControlConnection || bHasMorphConnection)
		{
			Append( IDM_ANIMTREE_BREAK_LINK, *LocalizeUnrealEd("BreakLink"), TEXT("") );
		}

		// If on an input that can be deleted, show option
		if( BlendNode && !Tree &&
			AnimTreeEd->ConnType == LOC_OUTPUT )
		{
			Append( IDM_ANIMTREE_NAME_INPUT, *LocalizeUnrealEd("NameInput"), TEXT("") );

			if( !BlendNode->bFixNumChildren )
			{
				if(bHasNodeConnection || bHasControlConnection)
				{
					AppendSeparator();
				}

				Append( IDM_ANIMTREE_DELETE_INPUT,	*LocalizeUnrealEd("DeleteInput"),	TEXT("") );
			}
		}

		if( Tree && AnimTreeEd->ConnIndex > 0)
		{
			if(bHasNodeConnection || bHasControlConnection)
			{
				AppendSeparator();
			}

			Append( IDM_ANIMTREE_CHANGEBONE_CONTROLHEAD,  *LocalizeUnrealEd("ChangeBone") );
			Append( IDM_ANIMTREE_DELETE_CONTROLHEAD, *LocalizeUnrealEd("DeleteSkelControlChain") );
		}
	}
};

void WxAnimTreeEditor::OpenNewObjectMenu()
{
	WxMBAnimTreeEdNewNode menu( this );
	FTrackPopupMenu tpm( this, &menu );
	tpm.Show();
}

void WxAnimTreeEditor::OpenObjectOptionsMenu()
{
	WxMBAnimTreeEdNodeOptions menu( this );
	FTrackPopupMenu tpm( this, &menu );
	tpm.Show();
}

void WxAnimTreeEditor::OpenConnectorOptionsMenu()
{
	WxMBAnimTreeEdConnectorOptions menu( this );
	FTrackPopupMenu tpm( this, &menu );
	tpm.Show();
}

void WxAnimTreeEditor::DoubleClickedObject(UObject* Obj)
{
	UAnimNode* ClickedNode = Cast<UAnimNode>(Obj);
	if(ClickedNode)
	{
		UAnimNodeEditInfo* EditInfo = FindAnimNodeEditInfo(ClickedNode);
		if(EditInfo)
		{
			EditInfo->OnDoubleClickNode(ClickedNode, this);
		}
	}
	else if ( Obj->IsA(UAnimNodeFrame::StaticClass()) )
	{
		UAnimNodeFrame *Comment = (UAnimNodeFrame*)(Obj);
		WxDlgGenericStringWrappedEntry dlg;
		INT Result = dlg.ShowModal( TEXT("EditComment"), TEXT("CommentText"), *Comment->ObjComment );
		if (Result == wxID_OK)
		{
			Comment->ObjComment = dlg.GetEnteredString();
		}
	}
}

void WxAnimTreeEditor::DrawObjects(FViewport* Viewport, FCanvas* Canvas)
{
	if (BackgroundTexture != NULL)
	{
		Clear(Canvas, FColor(161,161,161) );

		Canvas->PushAbsoluteTransform(FMatrix::Identity);

		const INT ViewWidth = LinkedObjVC->Viewport->GetSizeX();
		const INT ViewHeight = LinkedObjVC->Viewport->GetSizeY();

		// draw the texture to the side, stretched vertically
		FLinkedObjDrawUtils::DrawTile( Canvas, ViewWidth - BackgroundTexture->SizeX, 0,
			BackgroundTexture->SizeX, ViewHeight,
			0.f, 0.f,
			1.f, 1.f,
			FLinearColor::White,
			BackgroundTexture->Resource );

		// stretch the left part of the texture to fill the remaining gap
		if (ViewWidth > BackgroundTexture->SizeX)
		{
			FLinkedObjDrawUtils::DrawTile( Canvas, 0, 0,
				ViewWidth - BackgroundTexture->SizeX, ViewHeight,
				0.f, 0.f,
				0.1f, 0.1f,
				FLinearColor::White,
				BackgroundTexture->Resource );
		}

		Canvas->PopTransform();
	}

	if (AnimTree)
	{
		for(INT i=0; i<AnimTree->AnimNodeFrames.Num(); i++)
		{
			AnimTree->AnimNodeFrames(i)->DrawNode( Canvas, SelectedNodes, bShowNodeWeights );
		}
	}

	{
		const DOUBLE StartTime = appSeconds();
		for(INT i=0; i<TreeNodes.Num(); i++)
		{
			UAnimObject* DrawNode = TreeNodes(i);
			// AnimNodeFrame should only be edited in editor. 
			// if this somehow enters to TreeNodes. Please remove it. 
			check(!DrawNode->IsA(UAnimNodeFrame::StaticClass()));
			DrawNode->DrawNode( Canvas, SelectedNodes, bShowNodeWeights );

			// If this is the 'viewed' node - draw icon above it.
			if(DrawNode == PreviewSkelComp->Animations)
			{
				FLinkedObjDrawUtils::DrawTile( Canvas, DrawNode->NodePosX,		DrawNode->NodePosY - 3 - 8, 10,	10, 0.f, 0.f, 1.f, 1.f, FColor(0,0,0) );
				FLinkedObjDrawUtils::DrawTile( Canvas, DrawNode->NodePosX + 1,	DrawNode->NodePosY - 2 - 8, 8,	8,	0.f, 0.f, 1.f, 1.f, FColor(255,215,0) );
			}
		}
	//debugf( TEXT("AnimTree::DrawObjects -- AnimNode(%lf msecs)"), 1000.0*(appSeconds() - StartTime) );
	}

}

void WxAnimTreeEditor::UpdatePropertyWindow()
{
	// If we have both controls and nodes selected - just show nodes in property window...
	//if we delete a pin or unselect a node, setobjectarray with an empty array will invalidate the property window
	//if(SelectedNodes.Num() > 0)
	{
		PropertyWindow->SetObjectArray( SelectedNodes, EPropertyWindowFlags::ShouldShowCategories  );
	}

}

void WxAnimTreeEditor::EmptySelection()
{
	SelectedNodes.Empty();

	// When empty, I'll just leave current one
	// this is called too many time, I can't keep track
}

void WxAnimTreeEditor::AddToSelection( UObject* Obj )
{
	check (Obj->IsA(UAnimObject::StaticClass()));

	SelectedNodes.AddUniqueItem( (UAnimObject*)Obj );

	// When added or removed, only find, otherwise, just update
	FindCurrentlySelectedAnimSequence();
}

void WxAnimTreeEditor::RemoveFromSelection( UObject* Obj )
{
	check (Obj->IsA(UAnimObject::StaticClass()));
	SelectedNodes.RemoveItem( (UAnimObject*)Obj );
	
	// When added or removed, only find, otherwise, just update
	// Remove might leave only one animnodesequence to be active
	FindCurrentlySelectedAnimSequence();
}

UBOOL WxAnimTreeEditor::IsInSelection( UObject* Obj ) const
{
	check (Obj->IsA(UAnimObject::StaticClass()));
	return SelectedNodes.ContainsItem( (UAnimObject*)Obj );

}

INT WxAnimTreeEditor::GetNumSelected() const
{
	return SelectedNodes.Num();
}

void WxAnimTreeEditor::SetSelectedConnector( FLinkedObjectConnector& Connector )
{
	ConnObj = Connector.ConnObj;
	ConnType = Connector.ConnType;
	ConnIndex = Connector.ConnIndex;
}

FIntPoint WxAnimTreeEditor::GetSelectedConnLocation(FCanvas* Canvas)
{
	if(ConnObj)
	{
		UAnimNode* AnimNode = Cast<UAnimNode>(ConnObj);
		if(AnimNode)
		{
			return AnimNode->GetConnectionLocation(ConnType, ConnIndex);
		}

		USkelControlBase* Control = Cast<USkelControlBase>(ConnObj);
		if(Control)
		{
			return Control->GetConnectionLocation(ConnType);
		}

		UMorphNodeBase* MorphNode = Cast<UMorphNodeBase>(ConnObj);
		if(MorphNode)
		{
			return MorphNode->GetConnectionLocation(ConnType, ConnIndex);
		}
	}

	return FIntPoint(0,0);
}

INT WxAnimTreeEditor::GetSelectedConnectorType()
{
	return ConnType;
}

FColor WxAnimTreeEditor::GetMakingLinkColor()
{
	if( ConnObj->IsA(USkelControlBase::StaticClass()) || (ConnObj->IsA(UAnimTree::StaticClass()) && ConnIndex > 1) )
	{
		return FColor(50,100,50);
	}
	else if( ConnObj->IsA(UMorphNodeBase::StaticClass()) || (ConnObj->IsA(UAnimTree::StaticClass()) && ConnIndex == 1) )
	{
		return FColor(50,50,100);
	}
	else if( ConnObj->IsA(UMorphNodeBase::StaticClass()) )
	{
		return FColor(100,50,50);
	}
	else
	{
		return FColor(0,0,0);
	}
}

static UBOOL CheckAnimNodeReachability(UAnimNode* Start, UAnimNode* TestDest)
{
	UAnimNodeBlendBase* StartBlend = Cast<UAnimNodeBlendBase>(Start);
	if(StartBlend)
	{
		// If we are starting from a blend - walk from this node and find all nodes we reach.
		TArray<UAnimNode*> Nodes;
		StartBlend->GetNodes(Nodes);

		// If our test destination is in that set, return true.
		return Nodes.ContainsItem(TestDest);
	}
	// If start is not a blend - no way we can reach anywhere.
	else
	{
		return false;
	}
}

static UBOOL CheckMorphNodeReachability(UMorphNodeBase* Start, UMorphNodeBase* TestDest)
{
	UMorphNodeWeightBase* StartWeight = Cast<UMorphNodeWeightBase>(Start);
	if(StartWeight)
	{
		// If we are starting from a blend - walk from this node and find all nodes we reach.
		TArray<UMorphNodeBase*> Nodes;
		StartWeight->GetNodes(Nodes);

		// If our test destination is in that set, return true.
		return Nodes.ContainsItem(TestDest);
	}
	// If start is not a blend - no way we can reach anywhere.
	else
	{
		return false;
	}
}

void WxAnimTreeEditor::MakeConnectionToConnector( FLinkedObjectConnector& Connector )
{
	// Avoid connections to yourself.
	if(!Connector.ConnObj || !ConnObj || Connector.ConnObj == ConnObj)
	{
		return;
	}

	UAnimNode* EndConnNode = Cast<UAnimNode>(Connector.ConnObj);
	USkelControlBase* EndConnControl = Cast<USkelControlBase>(Connector.ConnObj);
	UMorphNodeBase* EndConnMorph = Cast<UMorphNodeBase>(Connector.ConnObj);
	check(EndConnNode || EndConnControl || EndConnMorph);

	UAnimNode* ConnNode = Cast<UAnimNode>(ConnObj);
	USkelControlBase* ConnControl = Cast<USkelControlBase>(ConnObj);
	UMorphNodeBase* ConnMorph = Cast<UMorphNodeBase>(ConnObj);
	check(ConnNode || ConnControl || ConnMorph);

	// Control to...
	if(ConnControl)
	{
		// Control to control
		if(EndConnControl)
		{
			check(ConnIndex == 0 && Connector.ConnIndex == 0);

			// Determine which is the 'parent', which contains references to the child.
			if(ConnType == LOC_INPUT && Connector.ConnType == LOC_OUTPUT)
			{
				EndConnControl->NextControl = ConnControl;
			}
			else if(ConnType == LOC_OUTPUT && Connector.ConnType == LOC_INPUT)
			{
				ConnControl->NextControl = EndConnControl;
			}
		}
		// Control to Node. Node must be AnimTree. Make sure we are not connecting to Anim or Morph input.
		else if(EndConnNode && Connector.ConnIndex > 1)
		{
			check( ConnIndex == 0 );

			UAnimTree* Tree = Cast<UAnimTree>(EndConnNode);
			if(Tree)
			{
				Tree->SkelControlLists(Connector.ConnIndex-2).ControlHead = ConnControl;
			}
		}
	}
	// Morph to...
	else if(ConnMorph)
	{
		// Morph to Morph
		if(EndConnMorph)
		{
			// Determine which is the 'parent', to which you want to set a references to the child.
			UMorphNodeWeightBase* ParentNode = NULL;
			UMorphNodeBase* ChildNode = NULL;
			INT ChildIndex = INDEX_NONE;

			if(ConnType == LOC_INPUT && Connector.ConnType == LOC_OUTPUT)
			{
				ParentNode = CastChecked<UMorphNodeWeightBase>(EndConnMorph); // Only weight nodes can have LOC_OUTPUT connectors
				ChildIndex = Connector.ConnIndex;
				ChildNode = ConnMorph;
			}
			else if(ConnType == LOC_OUTPUT && Connector.ConnType == LOC_INPUT)
			{
				ParentNode = CastChecked<UMorphNodeWeightBase>(ConnMorph);
				ChildIndex = ConnIndex;
				ChildNode = EndConnMorph;
			}

			if(ParentNode)
			{
				// See if there is already a route from the child to the parent. If so - disallow connection.
				UBOOL bReachable = CheckMorphNodeReachability(ChildNode, ParentNode);
				if(bReachable)
				{
					appMsgf(AMT_OK, *LocalizeUnrealEd("Error_AnimTreeLoopDetected") );
				}
				else
				{
					check(ChildIndex < ParentNode->NodeConns.Num());
					ParentNode->NodeConns(ChildIndex).ChildNodes.AddUniqueItem(ChildNode);
				}
			}
		}
		// Morph to anim node. Node must be AnimTree.
		else if(EndConnNode)
		{
			check( ConnIndex == 0 );

			UAnimTree* Tree = Cast<UAnimTree>(EndConnNode);
			if(Tree)
			{
				Tree->RootMorphNodes.AddUniqueItem(ConnMorph);
			}
		}
	}
	// AnimNode to...
	else if(ConnNode)
	{
		// AnimNode to AnimNode
		if(EndConnNode)
		{
			// Determine which is the 'parent', to which you want to set a references to the child.
			UAnimNodeBlendBase* ParentNode = NULL;
			UAnimNode* ChildNode = NULL;
			INT ChildIndex = INDEX_NONE;

			if(ConnType == LOC_INPUT && Connector.ConnType == LOC_OUTPUT)
			{
				ParentNode = CastChecked<UAnimNodeBlendBase>(EndConnNode); // Only blend nodes can have LOC_OUTPUT connectors
				ChildIndex = Connector.ConnIndex;
				ChildNode = ConnNode;
			}
			else if(ConnType == LOC_OUTPUT && Connector.ConnType == LOC_INPUT)
			{
				ParentNode = CastChecked<UAnimNodeBlendBase>(ConnNode);
				ChildIndex = ConnIndex;
				ChildNode = EndConnNode;
			}

			if(ParentNode)
			{
				// See if there is already a route from the child to the parent. If so - disallow connection.
				UBOOL bReachable = CheckAnimNodeReachability(ChildNode, ParentNode);
				if(bReachable)
				{
					appMsgf(AMT_OK, *LocalizeUnrealEd("Error_AnimTreeLoopDetected") );
				}
				else
				{
					// If ParentNode is a tree, made sure child index is 0
					UAnimTree* Tree = Cast<UAnimTree>(ParentNode);
					if(Tree && ChildIndex != 0)
					{
						appMsgf(AMT_OK, *LocalizeUnrealEd("Error_AnimTreeInvalidNodeIndex") );
					}
					else
					{
						ParentNode->Children(ChildIndex).Anim = ChildNode;
						ParentNode->OnChildAnimChange(ChildIndex);
					}
				}
			}
		}
		// AnimNode to Control. AnimNode must be AnimTree.
		else if(EndConnControl)
		{
			check(Connector.ConnIndex == 0);

			UAnimTree* Tree = Cast<UAnimTree>(ConnNode);
			if(Tree && ConnIndex > 1)
			{
				Tree->SkelControlLists(ConnIndex-2).ControlHead = EndConnControl;
			}
		}
		// AnimNode to Morph. AnimNode must be AnimTree.
		else if(EndConnMorph)
		{
			check(Connector.ConnIndex == 0);
			check(ConnIndex == 1);

			UAnimTree* Tree = Cast<UAnimTree>(ConnNode);
			if(Tree)
			{
				Tree->RootMorphNodes.AddUniqueItem(EndConnMorph);
			}
		}
	}

	// Reinitialise the animation tree.
	ReInitAnimTree();

	// Mark the AnimTree's package as dirty.
	AnimTree->MarkPackageDirty();
}

void WxAnimTreeEditor::MakeConnectionToObject( UObject* EndObj )
{

}

/**
 * Called when the user releases the mouse over a link connector and is holding the ALT key.
 * Commonly used as a shortcut to breaking connections.
 *
 * @param	Connector	The connector that was ALT+clicked upon.
 */
void WxAnimTreeEditor::AltClickConnector(FLinkedObjectConnector& Connector)
{
	wxCommandEvent DummyEvent;
	OnBreakLink( DummyEvent );
}

void WxAnimTreeEditor::MoveSelectedObjects( INT DeltaX, INT DeltaY )
{
	for(INT i=0; i<SelectedNodes.Num(); i++)
	{
		UAnimObject* Node = SelectedNodes(i);
		
		Node->NodePosX += DeltaX;
		Node->NodePosY += DeltaY;
	}


	// Mark the AnimTree's package as dirty.
	AnimTree->MarkPackageDirty();
}

void WxAnimTreeEditor::EdHandleKeyInput(FViewport* Viewport, FName Key, EInputEvent Event)
{
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);

	if(Event == IE_Pressed)
	{
		if(Key == KEY_Delete)
		{
			DeleteSelectedObjects();
		}
		else if(Key == KEY_W && bCtrlDown)
		{
			DuplicateSelectedObjects();
		}
		else if(Key == KEY_SpaceBar)
		{
			wxCommandEvent DummyEvent;
			OnPreviewSelectedNode( DummyEvent );
		}
		else if( bCtrlDown && Key == KEY_C )
		{
			Copy();
		}
		else if( bCtrlDown && Key == KEY_V )
		{
			Paste();
		}
		else if( bCtrlDown && Key == KEY_X )
		{
			Copy();
			DeleteSelectedObjects();
		}
	}
}

/** The little button for doing automatic blending in/out of controls uses special index 1, so we look for it being clicked here. */
UBOOL WxAnimTreeEditor::SpecialClick( INT NewX, INT NewY, INT SpecialIndex, FViewport* Viewport, UObject* ProxyObj )
{
	UBOOL bResult = 0;

	// Handle clicking on slider region jumping straight to that value.
	if(SpecialIndex == 0)
	{
		SpecialDrag(0, 0, NewX, NewY, SpecialIndex);
	}
	// Handle clicking on control button
	else if( SpecialIndex == 1 )
	{
		// if no animnode is selected
		if( GetNumSelectedByClass<UAnimNode>() == 0 )
		{
			UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);

			// If holding control (for multiple controls) or only have one selected control - toggle activate
			if( bCtrlDown || GetNumSelectedByClass<USkelControlBase>() == 1 )
			{
				for( INT Idx = 0; Idx < SelectedNodes.Num(); Idx++ )
				{
					USkelControlBase* Control = SelectedNodes(Idx)->GetSkelControlBase();

					if ( Control )
					{
						// We basically use the mid-point as the deciding line between blending up or blending down when we press the button.
						if( Control->ControlStrength > 0.5f )
						{
							Control->SetSkelControlActive( FALSE );
						}
						else
						{
							Control->SetSkelControlActive( TRUE );
						}

						bResult = 1;
					}
				}
			}
		}
	}

	return bResult;
}

/** Handler for dragging a 'special' item in the LinkedObjEditor. Index 0 is for slider controls, and thats what we are handling here. */
void WxAnimTreeEditor::SpecialDrag( INT DeltaX, INT DeltaY, INT NewX, INT NewY, INT SpecialIndex )
{
	// Only works if only 1 thing is selected.
	if(GetNumSelected() != 1)
	{
		return;
	}

	UAnimNode* Node = SelectedNodes(0)->GetAnimNode();
	USkelControlBase * Control = SelectedNodes(0)->GetSkelControlBase();
	UMorphNodeBase * MorphNode = SelectedNodes(0)->GetMorphNodeBase();

	// Anim node slider
	if( Node )
	{
		INT SliderWidth, SliderHeight;

		if(ST_2D == Node->GetSliderType(SpecialIndex))
		{
			SliderWidth = LO_SLIDER_HANDLE_HEIGHT;
			SliderHeight = LO_SLIDER_HANDLE_HEIGHT;
		}
		else
		{
			check(ST_1D == Node->GetSliderType(SpecialIndex));
			SliderWidth = LO_SLIDER_HANDLE_WIDTH;
			SliderHeight = LO_SLIDER_HANDLE_HEIGHT;
		}

		INT SliderRangeX = (Node->DrawWidth - 4 - SliderWidth);
		INT SlideStartX = Node->NodePosX + (2 + (0.5f * SliderWidth));
		INT SlideEndX = Node->NodePosX + Node->DrawWidth - (2 + (0.5f * SliderWidth));

		FLOAT HandleValX = (FLOAT)(NewX - SlideStartX)/(FLOAT)SliderRangeX;

		Node->HandleSliderMove(SpecialIndex, 0, ::Clamp(HandleValX, 0.f, 1.f) );

		if(ST_2D == Node->GetSliderType(SpecialIndex))
		{
			INT SliderPosY = Node->NodePosY + Node->DrawHeight;

			// compute slider starting position
			for(INT i = 0; i < SpecialIndex; ++i)
			{
				if(ST_1D == Node->GetSliderType(i))
				{
					SliderPosY += FLinkedObjDrawUtils::ComputeSliderHeight(Node->DrawWidth);
				}
				else
				{
					check(ST_2D == Node->GetSliderType(i));
					SliderPosY += FLinkedObjDrawUtils::Compute2DSliderHeight(Node->DrawWidth);
				}
			}

			INT TotalSliderHeight = FLinkedObjDrawUtils::Compute2DSliderHeight(Node->DrawWidth);
			INT SliderRangeY = (TotalSliderHeight - 4 - SliderHeight);
			INT SlideStartY = SliderPosY + (2 + (0.5f * SliderHeight));
			INT SlideEndY = SliderPosY + TotalSliderHeight - (2 + (0.5f * SliderHeight));

			FLOAT HandleValY = (FLOAT)(NewY - SlideStartY)/(FLOAT)SliderRangeY;

			Node->HandleSliderMove(SpecialIndex, 1, ::Clamp(HandleValY, 0.f, 1.f) );
		}
	}
	// SkelControl slider
	else if( Control )
	{
		if( SpecialIndex == 0 )
		{
			INT SliderRange = (Control->DrawWidth - 4 - LO_SLIDER_HANDLE_WIDTH - 15);
			INT SlideStart = Control->NodePosX + (2 + (0.5f * LO_SLIDER_HANDLE_WIDTH));
			INT SlideEnd = Control->NodePosX + Control->DrawWidth - (2 + (0.5f * LO_SLIDER_HANDLE_WIDTH)) - 15;
			FLOAT HandleVal = (FLOAT)(NewX - SlideStart)/(FLOAT)SliderRange;
			Control->HandleControlSliderMove( ::Clamp(HandleVal, 0.f, 1.f) );
		}
	}
	// MorphNode slider
	else if( MorphNode )
	{
		if( SpecialIndex == 0 )
		{
			INT SliderRange = (MorphNode->DrawWidth - 4 - LO_SLIDER_HANDLE_WIDTH);
			INT SlideStart = MorphNode->NodePosX + (2 + (0.5f * LO_SLIDER_HANDLE_WIDTH));
			INT SlideEnd = MorphNode->NodePosX + MorphNode->DrawWidth - (2 + (0.5f * LO_SLIDER_HANDLE_WIDTH));
			FLOAT HandleVal = (FLOAT)(NewX - SlideStart)/(FLOAT)SliderRange;
			MorphNode->HandleSliderMove( ::Clamp(HandleVal, 0.f, 1.f) );
		}
	}
	else // resize frame
	{
		UAnimNodeFrame* AnimFrame = Cast<UAnimNodeFrame>(SelectedNodes(0));
		if(AnimFrame)
		{
			if(SpecialIndex == 1)
			{
				// Apply dragging to 
				AnimFrame->SizeX += DeltaX;
				AnimFrame->SizeX = ::Max<INT>(AnimFrame->SizeX, 64);

				AnimFrame->SizeY += DeltaY;
				AnimFrame->SizeY = ::Max<INT>(AnimFrame->SizeY, 64);
			}
		}
	}
}

/** Export selected sequence objects to text and puts into Windows clipboard. */
void WxAnimTreeEditor::Copy()
{
	// Never allow you to duplicate the AnimTree itself!
	SelectedNodes.RemoveItem(AnimTree);

	// Iterate over all objects making sure they import/export flags are unset. 
	// These are used for ensuring we export each object only once etc.
	for( FObjectIterator It; It; ++It )
	{
		It->ClearFlags( RF_TagImp | RF_TagExp );
	}

	FStringOutputDevice Ar;
	const FExportObjectInnerContext Context;
	for(INT i=0; i<SelectedNodes.Num(); i++)
	{
		UExporter::ExportToOutputDevice( &Context, SelectedNodes(i), NULL, Ar, TEXT("copy"), 0, PPF_ExportsNotFullyQualified );
	}

	appClipboardCopy( *Ar );

	PasteCount = 0;
}

/** 
*	Take contents of windows clipboard and use USequenceFactory to create new SequenceObjects (possibly new subsequences as well). 
*	If bAtMousePos is TRUE, it will move the objects so the top-left corner of their bounding box is at the current mouse position (NewX/NewY in LinkedObjVC)
*/
void WxAnimTreeEditor::Paste()
{
	INT PasteOffset = 30;

	PasteCount++;

	// Get pasted text.
	FString PasteString = appClipboardPaste();
	const TCHAR* Paste = *PasteString;

	ULinkedObjectFactory* Factory = new ULinkedObjectFactory;
	Factory->AllowedCreateClass = UAnimObject::StaticClass();
	Factory->FactoryCreateText( NULL, AnimTree, NAME_None, 0, NULL, TEXT("paste"), Paste, Paste+appStrlen(Paste), GWarn );

	// Select the newly pasted stuff, and offset a bit.
	EmptySelection();

	for ( USelection::TObjectIterator It( GEditor->GetSelectedObjects()->ObjectItor() ) ; It ; ++It )
	{
		if ((*It)->IsA(UAnimObject::StaticClass()))
		{
			AddToSelection(*It);
		}
	}

	// If we want to paste the copied objects at the current mouse position (NewX/NewY in LinkedObjVC) and we actually pasted something...
	if(SelectedNodes.Num() > 0)
	{
		// Find where we want the top-left corner of selection to be.
		INT DesPosX, DesPosY;
		DesPosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
		DesPosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;
		// By default offset pasted objects by the default value.
		// This is a fallback case for SequenceClasses that have a DrawHeight or DrawWidth of 0.
		INT DeltaX = PasteCount * PasteOffset;
		INT DeltaY = PasteCount * PasteOffset;

		// Apply to all selected objects.
		for(INT i=0; i<SelectedNodes.Num(); i++)
		{
			// if pasting to the same tree 
			if (SelectedNodes(i)->GetOuter() == AnimTree)
			{
				SelectedNodes(i)->NodePosX += DeltaX;
				SelectedNodes(i)->NodePosY += DeltaY;
			}
			else
			{
				// otherwise, paste to center of screen
				SelectedNodes(i)->NodePosX = DesPosX;
				SelectedNodes(i)->NodePosY = DesPosY;
			}

			if (SelectedNodes(i)->IsA(UAnimNodeFrame::StaticClass()))
			{
				AnimTree->AnimNodeFrames.AddItem(CastChecked<UAnimNodeFrame>(SelectedNodes(i)));
			}
			else
			{
				TreeNodes.AddItem(SelectedNodes(i));
			}
	
			// clean up
			SelectedNodes(i)->OnPaste();
		}
	}
	
	AnimTree->MarkPackageDirty();

	// Update property window to reflect new selection.
	UpdatePropertyWindow();

	RefreshViewport();
	NotifyObjectsChanged();
}

/*-----------------------------------------------------------------------------
	WxAnimTreeEditorToolBar.
-----------------------------------------------------------------------------*/

WxAnimTreeEditorToolBar::WxAnimTreeEditorToolBar( wxWindow* InParent, wxWindowID InID )
	:	WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
{
	// create the return to parent sequence button
	TickTreeB.Load(TEXT("AnimTree_TickTree"));
	PreviewNodeB.Load(TEXT("AnimTree_PrevNode"));
	ShowNodeWeightB.Load(TEXT("AnimTree_ShowNodeWeight"));
	ShowBonesB.Load(TEXT("AnimTree_ShowBones"));
	ShowBoneNamesB.Load(TEXT("AnimTree_ShowBoneNames"));
	ShowWireframeB.Load(TEXT("AnimTree_ShowWireframe"));
	ShowFloorB.Load(TEXT("AnimTree_ShowFloor"));
	CurvesB.Load(TEXT("KIS_DrawCurves"));
	AddMaskedBitmap.Load(TEXT("CASC_Cross"));

	SetToolBitmapSize( wxSize( 16, 16 ) );

	AddSeparator();
	AddCheckTool(IDM_ANIMTREE_TOGGLETICKTREE, *LocalizeUnrealEd("PauseAnimTree"), TickTreeB, wxNullBitmap, *LocalizeUnrealEd("PauseAnimTree"));
	AddSeparator();
	AddTool(IDM_ANIMTREE_PREVIEWSELECTEDNODE, PreviewNodeB, *LocalizeUnrealEd("PreviewSelectedNode"));
	AddCheckTool(IDM_ANIMTREE_SHOWNODEWEIGHT, *LocalizeUnrealEd("ShowNodeWeight"), ShowNodeWeightB, wxNullBitmap, *LocalizeUnrealEd("ShowNodeWeight"));
	AddSeparator();
	AddCheckTool(IDM_ANIMTREE_SHOWHIERARCHY, *LocalizeUnrealEd("ShowSkeleton"), ShowBonesB, wxNullBitmap, *LocalizeUnrealEd("ShowSkeleton"));
	AddCheckTool(IDM_ANIMTREE_SHOWBONENAMES, *LocalizeUnrealEd("ShowBoneNames"), ShowBoneNamesB, wxNullBitmap, *LocalizeUnrealEd("ShowBoneNames"));
	AddCheckTool(IDM_ANIMTREE_SHOWWIREFRAME, *LocalizeUnrealEd("ShowWireframe"), ShowWireframeB, wxNullBitmap, *LocalizeUnrealEd("ShowWireframe"));
	AddCheckTool(IDM_ANIMTREE_SHOWFLOOR, *LocalizeUnrealEd("ShowFloor"), ShowFloorB, wxNullBitmap, *LocalizeUnrealEd("ShowFloor"));

	AddSeparator();
	PreviewMeshListCombo = new WxComboBox(this, IDM_ANIMTREE_COMBO_SKELMESHLIST, TEXT(""), wxDefaultPosition, wxSize(150, -1), 0, NULL, wxCB_READONLY);
	AddControl(PreviewMeshListCombo);
	AddCheckTool(IDM_ANIMTREE_ADDENTRY_SKELMESHLIST, *LocalizeUnrealEd("CreateNewEntryFromCurrentSelection"), AddMaskedBitmap, wxNullBitmap, *LocalizeUnrealEd("CreateNewEntryFromCurrentSelection"));

	AddSeparator();
	PreviewAnimSetListCombo = new WxComboBox(this, IDM_ANIMTREE_COMBO_ANIMSETLIST, TEXT(""), wxDefaultPosition, wxSize(150, -1), 0, NULL, wxCB_READONLY);
	AddControl(PreviewAnimSetListCombo);
	AddCheckTool(IDM_ANIMTREE_ADDENTRY_ANIMSETLIST, *LocalizeUnrealEd("CreateNewEntryFromCurrentSelection"), AddMaskedBitmap, wxNullBitmap, *LocalizeUnrealEd("CreateNewEntryFromCurrentSelection"));

	AddSeparator();
	PreviewSocketListCombo = new WxComboBox(this, IDM_ANIMTREE_COMBO_SOCKETLIST, TEXT(""), wxDefaultPosition, wxSize(150, -1), 0, NULL, wxCB_READONLY);
	AddControl(PreviewSocketListCombo);
	AddCheckTool(IDM_ANIMTREE_ADDENTRY_SOCKETLIST, *LocalizeUnrealEd("CreateNewEntryFromCurrentSelection"), AddMaskedBitmap, wxNullBitmap, *LocalizeUnrealEd("CreateNewEntryFromCurrentSelection"));

	// Add Preview Animset/AnimSequence Combo
	AddSeparator();
	// Add text and combo
 	AddControl( new wxStaticText( this, -1, *LocalizeUnrealEd("AnimTreeEdPreviewAnimSet"), wxDefaultPosition, wxSize(100, -1) ) );
	PreviewAnimSetCombo = new WxComboBox(this, IDM_ANIMTREE_COMBO_ANIMSET, TEXT(""), wxDefaultPosition, wxSize(200, -1), 0, NULL, wxCB_READONLY);
	AddControl(PreviewAnimSetCombo);
	// Add text and combo
	PreviewAnimSequenceCombo = new WxComboBox(this, IDM_ANIMTREE_COMBO_ANIMSEQUENCE, TEXT("") , wxDefaultPosition, wxSize(250, -1), 0, NULL, wxCB_READONLY);
	AddControl(PreviewAnimSequenceCombo);

	// Add Preview Animset/AnimSequence Combo
	AddSeparator();
	// Add the item 
	// Add slider 0 <slider> 1
	const wxString Min(TEXT("0")), Max(TEXT("1")), PreviewRate(TEXT("Preview Rate :"));
	AddControl(new wxStaticText( this, -1, PreviewRate));
	AddControl(new wxStaticText( this, -1, Min));
	RateSlideBar = new wxSlider( this, IDM_ANIMTREE_PREVIEW_RATESLIDE, 100, 0, 100, wxDefaultPosition, wxSize(150, 20));
	AddControl(RateSlideBar);
	AddControl(new wxStaticText( this, -1, Max));

	// Add Preview Animset/AnimSequence Combo
	AddSeparator();
	Realize();
}

/*-----------------------------------------------------------------------------
	WxAnimTreeEditor
-----------------------------------------------------------------------------*/

/** return how many times the class was derived */
static INT CountDerivations(const UClass* cl)
{
	INT derivations = 0;
	while (cl != NULL)
	{
		++derivations;
		cl = cl->GetSuperClass();
	}
	return derivations;
}

/** compare two UAnimNodeEditInfo by how often their AnimNodeClass is derived (most derived will be in front) */
IMPLEMENT_COMPARE_POINTER(UAnimNodeEditInfo, AnimTreeEditor, \
{ \
	const INT derivationsA = CountDerivations(A->AnimNodeClass); \
	const INT derivationsB = CountDerivations(B->AnimNodeClass); \
	return (derivationsA < derivationsB) ? +1 \
	: ((derivationsA > derivationsB) ? -1 : 0); \
} )


/** this function will sort the given array of anim node edit infos by the derivation of their AnimNodeClass, so that most derived will be in front */
void SortAnimNodeEditInfosByDerivation(TArray<class UAnimNodeEditInfo*>& AnimNodeEditInfos)
{
	Sort<USE_COMPARE_POINTER(UAnimNodeEditInfo,
		AnimTreeEditor)>( AnimNodeEditInfos.GetTypedData(),
		AnimNodeEditInfos.Num() );
}


UBOOL				WxAnimTreeEditor::bAnimClassesInitialized = false;
TArray<UClass*>		WxAnimTreeEditor::AnimNodeClasses;
TArray<UClass*>		WxAnimTreeEditor::SkelControlClasses;
TArray<UClass*>		WxAnimTreeEditor::MorphNodeClasses;

BEGIN_EVENT_TABLE( WxAnimTreeEditor, WxTrackableFrame )
	EVT_CLOSE( WxAnimTreeEditor::OnClose )
	EVT_MENU_RANGE( IDM_ANIMTREE_NEW_ANIM_START, IDM_ANIMTREE_NEW_ANIM_END, WxAnimTreeEditor::OnNewAnimNode )
	EVT_MENU_RANGE( IDM_ANIMTREE_NEW_CONTROL_START, IDM_ANIMTREE_NEW_CONTROL_END, WxAnimTreeEditor::OnNewSkelControl )
	EVT_MENU_RANGE( IDM_ANIMTREE_NEW_MORPH_START, IDM_ANIMTREE_NEW_MORPH_END, WxAnimTreeEditor::OnNewMorphNode )
	EVT_MENU( IDM_ANIMTREE_DELETE_NODE, WxAnimTreeEditor::OnDeleteObjects )
	EVT_MENU( IDM_ANIMTREE_COMMENT_TO_FRONT, WxAnimTreeEditor::OnContextCommentToFront )
	EVT_MENU( IDM_ANIMTREE_COMMENT_TO_BACK, WxAnimTreeEditor::OnContextCommentToBack )
	EVT_MENU( IDM_ANIMTREE_BREAK_LINK, WxAnimTreeEditor::OnBreakLink )
	EVT_MENU( IDM_ANIMTREE_BREAK_ALL_LINKS, WxAnimTreeEditor::OnBreakAllLinks )
	EVT_MENU( IDM_ANIMTREE_ADD_INPUT, WxAnimTreeEditor::OnAddInput )
	EVT_MENU( IDM_ANIMTREE_DELETE_INPUT, WxAnimTreeEditor::OnRemoveInput )
	EVT_MENU( IDM_ANIMTREE_NAME_INPUT, WxAnimTreeEditor::OnNameInput )
	EVT_MENU( IDM_ANIMTREE_ADD_CONTROLHEAD, WxAnimTreeEditor::OnAddControlHead )
	EVT_MENU( IDM_ANIMTREE_DELETE_CONTROLHEAD, WxAnimTreeEditor::OnRemoveControlHead )
	EVT_MENU( IDM_ANIMTREE_CHANGEBONE_CONTROLHEAD,WxAnimTreeEditor::OnChangeBoneControlHead )
	EVT_TOOL( IDM_ANIMTREE_TOGGLETICKTREE, WxAnimTreeEditor::OnToggleTickAnimTree )
	EVT_TOOL( IDM_ANIMTREE_PREVIEWSELECTEDNODE, WxAnimTreeEditor::OnPreviewSelectedNode )
	EVT_TOOL( IDM_ANIMTREE_SHOWNODEWEIGHT, WxAnimTreeEditor::OnShowNodeWeights )
	EVT_TOOL( IDM_ANIMTREE_SHOWHIERARCHY, WxAnimTreeEditor::OnShowSkeleton )
	EVT_TOOL( IDM_ANIMTREE_SHOWBONENAMES, WxAnimTreeEditor::OnShowBoneNames )
	EVT_TOOL( IDM_ANIMTREE_SHOWWIREFRAME, WxAnimTreeEditor::OnShowWireframe )
	EVT_TOOL( IDM_ANIMTREE_SHOWFLOOR, WxAnimTreeEditor::OnShowFloor )
	EVT_TOOL( IDM_ANIMTREE_EDIT_COPY, WxAnimTreeEditor::OnCopy)
	EVT_TOOL( IDM_ANIMTREE_NEW_COMMENT, WxAnimTreeEditor::OnNewComment)
	EVT_TOOL( IDM_ANIMTREE_EDIT_DUPLICATE, WxAnimTreeEditor::OnDuplicate)
	EVT_TOOL( IDM_ANIMTREE_EDIT_PASTE, WxAnimTreeEditor::OnPaste)
	EVT_TOOL( IDM_ANIMTREE_ADDENTRY_SKELMESHLIST, WxAnimTreeEditor::OnAddNewEntryPreviewSkelMesh)
	EVT_TOOL( IDM_ANIMTREE_ADDENTRY_ANIMSETLIST, WxAnimTreeEditor::OnAddNewEntryPreviewAnimSet)
	EVT_TOOL( IDM_ANIMTREE_ADDENTRY_SOCKETLIST, WxAnimTreeEditor::OnAddNewEntryPreviewSocket)
	EVT_COMBOBOX( IDM_ANIMTREE_COMBO_ANIMSETLIST, WxAnimTreeEditor::OnPreviewAnimSetListCombo )
	EVT_COMBOBOX( IDM_ANIMTREE_COMBO_SOCKETLIST, WxAnimTreeEditor::OnPreviewSocketListCombo )
	EVT_COMBOBOX( IDM_ANIMTREE_COMBO_SKELMESHLIST, WxAnimTreeEditor::OnPreviewMeshListCombo )
	EVT_COMBOBOX( IDM_ANIMTREE_COMBO_ANIMSET, WxAnimTreeEditor::OnPreviewAnimSetCombo )
	EVT_COMBOBOX( IDM_ANIMTREE_COMBO_ANIMSEQUENCE, WxAnimTreeEditor::OnPreviewAnimSequenceCombo )
	EVT_COMMAND_SCROLL( IDM_ANIMTREE_PREVIEW_RATESLIDE, WxAnimTreeEditor::OnPreviewRateChanged )
END_EVENT_TABLE()

IMPLEMENT_COMPARE_POINTER( UClass, AnimTreeEditor, { return appStricmp( *A->GetName(), *B->GetName() ); } )

// Static functions that fills in array of all available USoundNode classes and sorts them alphabetically.
void WxAnimTreeEditor::InitAnimClasses()
{
	if(bAnimClassesInitialized)
		return;

	// Construct list of non-abstract gameplay sequence object classes.
	for(TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;

		if( !(Class->ClassFlags & CLASS_Abstract) && !(Class->ClassFlags & CLASS_Hidden) && !(Class->ClassFlags & CLASS_Deprecated) )
		{
			if( (Class->IsChildOf(UAnimNode::StaticClass())) && !(Class->IsChildOf(UAnimTree::StaticClass())) )
			{
				AnimNodeClasses.AddItem(Class);
			}
			else if( Class->IsChildOf(USkelControlBase::StaticClass()) )
			{
				SkelControlClasses.AddItem(Class);
			}
			else if( Class->IsChildOf(UMorphNodeBase::StaticClass()) )
			{
				MorphNodeClasses.AddItem(Class);
			}
		}
	}

	Sort<USE_COMPARE_POINTER(UClass,AnimTreeEditor)>( &AnimNodeClasses(0), AnimNodeClasses.Num() );

	bAnimClassesInitialized = true;
}


WxAnimTreeEditor::WxAnimTreeEditor() 
: FDockingParent(this)
{

}


WxAnimTreeEditor::WxAnimTreeEditor( wxWindow* InParent, wxWindowID InID, class UAnimTree* InAnimTree )
: WxTrackableFrame( InParent, InID, TEXT(""), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR ),
  FDockingParent(this)
{
	bShowSkeleton = FALSE;
	bShowBoneNames = FALSE;
	bShowWireframe = FALSE;
	bShowFloor = FALSE;
	bShowNodeWeights = FALSE;
	bTickAnimTree = TRUE;
	bEditorClosing = FALSE;

	AnimTree = InAnimTree;
	AnimTree->bBeingEdited = TRUE;

	// Set the anim tree editor window title to include the anim tree being edited.
	SetTitle( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("AnimTreeEditor_F"), *AnimTree->GetPathName()) ) );

	// Initialise list of AnimNode classes.
	InitAnimClasses();

	// Create property window
	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create( this, this );

	// Create linked-object tree window
	WxLinkedObjVCHolder* TreeWin = new WxLinkedObjVCHolder( this, -1, this );
	LinkedObjVC = TreeWin->LinkedObjVC;

	// Use default WorldInfo to define the gravity and stepping params.
#if WITH_APEX
	AWorldInfo* Info = (AWorldInfo*)(AWorldInfo::StaticClass()->GetDefaultObject());
	check(Info);
	FVector Gravity(0, 0, Info->DefaultGravityZ);
	RBPhysScene = CreateRBPhysScene( Gravity );
#endif

	// Create 3D preview window
	WxAnimTreePreview* PreviewWin = new WxAnimTreePreview( this, -1, this, AnimTree->PreviewCamPos, AnimTree->PreviewCamRot );
	PreviewVC = PreviewWin->AnimTreePreviewVC;

	SetSize(1024, 768);

	// Load the desired window position from .ini file
	FWindowUtil::LoadPosSize(TEXT("AnimTreeEditor"), this, 256, 256, 1024, 768);

	// Load the preview scene
	PreviewVC->PreviewScene.LoadSettings(TEXT("AnimTreeEditor"));

	// Add docking windows.
	{
		AddDockingWindow( PreviewWin, FDockingParent::DH_Left, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("PreviewCaption_F"), *AnimTree->GetPathName())), *LocalizeUnrealEd("Preview") );
		AddDockingWindow( PropertyWindow, FDockingParent::DH_Bottom, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("PropertiesCaption_F"), *AnimTree->GetPathName())), *LocalizeUnrealEd("Properties") );
		AddDockingWindow( TreeWin, FDockingParent::DH_None, NULL );

		// Try to load a existing layout for the docking windows.
		LoadDockingLayout();
	}

	wxMenuBar* MenuBar = new wxMenuBar;
	AppendWindowMenu(MenuBar);
	SetMenuBar(MenuBar);

	ToolBar = new WxAnimTreeEditorToolBar( this, -1 );
	SetToolBar(ToolBar);

	// Shift origin so origin is roughly in the middle. Would be nice to use Viewport size, but doesnt seem initialised here...
	LinkedObjVC->Origin2D.X = 150;
	LinkedObjVC->Origin2D.Y = 300;

	BackgroundTexture = LoadObject<UTexture2D>(NULL, TEXT("EditorMaterials.AnimTreeBackGround"), NULL, LOAD_None, NULL);

	// Build list of all AnimNodes in the tree.
	TArray<class UAnimNode*> AnimNodes;
	TArray<class USkelControlBase*> SkelControls;
	TArray<class UMorphNodeBase*> MorphNodes;
	AnimTree->GetNodes(AnimNodes, TRUE);
	AnimTree->GetSkelControls(SkelControls);
	AnimTree->GetMorphNodes(MorphNodes);

	TreeNodes.Empty();

	AppendToTreeNode<UAnimNode>(AnimNodes);
	AppendToTreeNode<USkelControlBase>(SkelControls);
	AppendToTreeNode<UMorphNodeBase>(MorphNodes);

	// Check for nodes that have been deprecated. Warn user if any has been found!
	{
		UBOOL bFoundDeprecatedNode = FALSE;

		for(INT i=0; i<TreeNodes.Num(); i++)
		{
			if( TreeNodes(i)->GetClass()->ClassFlags & CLASS_Deprecated )
			{
				bFoundDeprecatedNode = TRUE;
				break;
			}
		}


		// If we've found a deprecated node, warn the user that he should be careful when saving the tree
		if( bFoundDeprecatedNode )
		{
			appMsgf(AMT_OK, TEXT("This AnimTree contains DEPRECATED nodes.\nSaving the Tree will set these references to NULL!!\nMake sure you update or remove these nodes before saving.\nThey are shown with a bright RED background."));
		}
	}

	// Clean up node tick tag to make sure it's start from fresh 
	// when this gets opened multiple times, the nodeticktag is on memory
	// so it skips initializing (building tick array )
	// in particular this was problem when no skeletalmesh exists, causing no tick
	// next time opening will have initialized tick tag
	{
		for(INT i=0; i<AnimNodes.Num(); i++)
		{
			// clear node tick tag to make sure it's rebuilt clearly
			AnimNodes(i)->NodeTickTag = 0;
		}
	}

	// Initialize the preview SkeletalMeshComponent.
	PreviewSkelComp->Animations = AnimTree;
	UpdatePreviewMesh();
	UpdatePreviewAnimSet();

	// make sure SocketComponent is not initialized with garbage.
	SocketComponent = NULL;
	UpdatePreviewSocket();

	// Fill up combos
	UpdatePreviewMeshListCombo();
	UpdatePreviewAnimSetListCombo();
	UpdatePreviewSocketListCombo();

	// Refresh animset combo 
	RefreshPreviewAnimSetCombo();

	// Instance one of each AnimNodeEditInfos. Allows AnimNodes to have specific editor functionality.
	for(TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if( !(Class->ClassFlags & CLASS_Abstract) && !(Class->ClassFlags & CLASS_Hidden) )
		{
			if( Class->IsChildOf(UAnimNodeEditInfo::StaticClass()) )
			{
				UAnimNodeEditInfo* NewEditInfo = ConstructObject<UAnimNodeEditInfo>(Class);
				AnimNodeEditInfos.AddItem(NewEditInfo);
			}
		}
	}

	SortAnimNodeEditInfosByDerivation(AnimNodeEditInfos);
	PasteCount = 0;
}

WxAnimTreeEditor::~WxAnimTreeEditor()
{
	SaveDockingLayout();
	
	// Save the preview scene
	check(PreviewVC);
	PreviewVC->PreviewScene.SaveSettings(TEXT("AnimTreeEditor"));

	// Save the desired window position to the .ini file
	FWindowUtil::SavePosSize(TEXT("AnimTreeEditor"), this);

	// Do this here as well, as OnClose doesn't get called when closing the editor.
	PreviewSkelComp->Animations = NULL;
	for(INT i=0; i<TreeNodes.Num(); i++)
	{
		TreeNodes(i)->SkelComponent = NULL;
	}
	
	// Clean up apex clothing 
#if WITH_APEX
	if ( PreviewSkelComp )
	{
		PreviewSkelComp->ReleaseApexClothing();
		PreviewSkelComp->AnimSets.Empty();
	}
	DestroyRBPhysScene(RBPhysScene);
#endif
}

/**
 * This function is called when the window has been selected from within the ctrl + tab dialog.
 */
void WxAnimTreeEditor::OnSelected()
{
	Raise();
}

void WxAnimTreeEditor::OnClose( wxCloseEvent& In )
{
	// check if there is disconnected nodes, if so ask if they'd like to close or not
	// if we can veto, and if the number doesn't match
	if ( In.CanVeto() && PreviewSkelComp && AnimTree )
	{
		TArray<USkelControlBase*> Controls;
		TArray<UMorphNodeBase*> MorphNodes;

		AnimTree->GetSkelControls(Controls);
		AnimTree->GetMorphNodes(MorphNodes);

		// if treenodes that animtree viewer has does not match with what skelcomponent has
		UBOOL bMissingNodes = TreeNodes.Num()!=(PreviewSkelComp->AnimTickArray.Num() + Controls.Num() + MorphNodes.Num());

		//debugf(TEXT("#editor (%d), #animticks(%d), #skelcontro(%d), #morph target(%d)"), TreeNodes.Num(), PreviewSkelComp->AnimTickArray.Num(),Controls.Num(),MorphNodes.Num());
		if ( bMissingNodes && appMsgf(AMT_YesNo, *LocalizeUnrealEd("ConfirmClosingWithMissingNodes"))==0 )
		{
			// if user doesn't want to close, veto it. 
			In.Veto();
			return;
		}
	}

	AnimTree->bBeingEdited = false;

	// Call OnCloseAnimTreeEditor on all EditInfos
	for(INT i=0; i<AnimNodeEditInfos.Num(); i++)
	{
		AnimNodeEditInfos(i)->OnCloseAnimTreeEditor();
	}

	// Detach AnimTree from preview Component so it doesn't get deleted when preview SkeletalMeshComponent does, 
	// and there are no pointers to component we are about to destroy.
	PreviewSkelComp->Animations = NULL;

	for(INT i=0; i<TreeNodes.Num(); i++)
	{
		TreeNodes(i)->SkelComponent = NULL;
	}


	// Remember the preview viewport camera position/rotation.
	AnimTree->PreviewCamPos = PreviewVC->ViewLocation;
	AnimTree->PreviewCamRot = PreviewVC->ViewRotation;

	// Remember the floor position/rotation
	AnimTree->PreviewFloorPos = FloorComp->LocalToWorld.GetOrigin();
	FRotator FloorRot = FloorComp->LocalToWorld.Rotator();
	AnimTree->PreviewFloorYaw = FloorRot.Yaw;

	ClearSocketPreview();

	bEditorClosing = true;

	this->Destroy();
}

void WxAnimTreeEditor::Serialize(FArchive& Ar)
{
	PreviewVC->Serialize(Ar);

	// Make sure we don't garbage collect nodes/control that are not currently linked into the tree.
	if(!Ar.IsLoading() && !Ar.IsSaving())
	{
		for(INT i=0; i<TreeNodes.Num(); i++)
		{
			Ar << TreeNodes(i);
		}


		for(INT i=0; i<AnimNodeEditInfos.Num(); i++)
		{
			Ar << AnimNodeEditInfos(i);
		}
	}
}

/**
 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
 *  @return A string representing a name to use for this docking parent.
 */
const TCHAR* WxAnimTreeEditor::GetDockingParentName() const
{
	return TEXT("AnimTreeEditor");
}

/**
 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
 */
const INT WxAnimTreeEditor::GetDockingParentVersion() const
{
	return 0;
}

//////////////////////////////////////////////////////////////////
// Menu Handlers
//////////////////////////////////////////////////////////////////

void WxAnimTreeEditor::OnNewAnimNode(wxCommandEvent& In)
{
	INT NewNodeClassIndex = In.GetId() - IDM_ANIMTREE_NEW_ANIM_START;
	check( NewNodeClassIndex >= 0 && NewNodeClassIndex < AnimNodeClasses.Num() );

	UClass* NewNodeClass = AnimNodeClasses(NewNodeClassIndex);
	check( NewNodeClass->IsChildOf(UAnimNode::StaticClass()) );

	UAnimNode* NewNode = ConstructObject<UAnimNode>( NewNodeClass, AnimTree, NAME_None);

	NewNode->NodePosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
	NewNode->NodePosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;

	TreeNodes.AddItem(NewNode);

	RefreshViewport();
}

void WxAnimTreeEditor::OnNewSkelControl(wxCommandEvent& In)
{
	INT NewControlClassIndex = In.GetId() - IDM_ANIMTREE_NEW_CONTROL_START;
	check( NewControlClassIndex >= 0 && NewControlClassIndex < SkelControlClasses.Num() );

	UClass* NewControlClass = SkelControlClasses(NewControlClassIndex);
	check( NewControlClass->IsChildOf(USkelControlBase::StaticClass()) );

	USkelControlBase* NewControl = ConstructObject<USkelControlBase>( NewControlClass, AnimTree, NAME_None);
	NewControl->NodePosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
	NewControl->NodePosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;

	TreeNodes.AddItem(NewControl);

	RefreshViewport();
}

void WxAnimTreeEditor::OnNewMorphNode( wxCommandEvent& In )
{
	INT NewNodeClassIndex = In.GetId() - IDM_ANIMTREE_NEW_MORPH_START;
	check( NewNodeClassIndex >= 0 && NewNodeClassIndex < MorphNodeClasses.Num() );

	UClass* NewNodeClass = MorphNodeClasses(NewNodeClassIndex);
	check( NewNodeClass->IsChildOf(UMorphNodeBase::StaticClass()) );

	UMorphNodeBase* NewNode = ConstructObject<UMorphNodeBase>( NewNodeClass, AnimTree, NAME_None);

	NewNode->NodePosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
	NewNode->NodePosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;

	TreeNodes.AddItem(NewNode);

	RefreshViewport();
}


void WxAnimTreeEditor::OnBreakLink(wxCommandEvent& In)
{
	UAnimTree* Tree = Cast<UAnimTree>(ConnObj);
	UAnimNodeBlendBase* BlendNode = Cast<UAnimNodeBlendBase>(ConnObj);
	USkelControlBase* SkelControl = Cast<USkelControlBase>(ConnObj);
	UMorphNodeWeightBase* MorphNode = Cast<UMorphNodeWeightBase>(ConnObj);

	if( ConnType == LOC_OUTPUT )
	{
		if(Tree)
		{
			if(ConnIndex == 0)
			{
				Tree->Children(0).Anim = NULL;
				Tree->OnChildAnimChange(0);
			}
			else if(ConnIndex == 1)
			{
				// @todo - Let you select WHICH connection to break
				Tree->RootMorphNodes.Empty();
			}
			else
			{
				Tree->SkelControlLists(ConnIndex-2).ControlHead = NULL;
			}
		}
		else if(BlendNode)
		{		
			BlendNode->Children(ConnIndex).Anim = NULL;
			BlendNode->OnChildAnimChange(ConnIndex);
		}
		else if(SkelControl)
		{
			SkelControl->NextControl = NULL;
		}
		else if(MorphNode)
		{
			// @todo - Let you select WHICH connection to break
			MorphNode->NodeConns(ConnIndex).ChildNodes.Empty();
		}
	}

	ReInitAnimTree();
	RefreshViewport();

	AnimTree->MarkPackageDirty();
}

/** Break all links going to or from the selected node(s). */
void WxAnimTreeEditor::OnBreakAllLinks(wxCommandEvent& In)
{
	if (appMsgf(AMT_YesNo, *LocalizeUnrealEd("ConfirmBreakAllLinks")))
	{
		for(UAnimNode * AnimNode=GetFirstSelectedNodeByClass<UAnimNode>(); AnimNode!=NULL; AnimNode=GetNextSelectedNodeByClass<UAnimNode>(AnimNode))
		{
			UAnimTree* Tree = Cast<UAnimTree>(AnimNode);
			UAnimNode* Node = Cast<UAnimNodeBlendBase>(AnimNode);

			if(Tree)
			{
				Tree->Children(0).Anim = NULL;
				Tree->OnChildAnimChange(0);

				for(INT j=0; j<Tree->SkelControlLists.Num(); j++)
				{
					Tree->SkelControlLists(j).ControlHead = NULL;
				}
			}
			else if(Node)
			{		
				UAnimNodeBlendBase* BlendNode = Cast<UAnimNodeBlendBase>(Node);
				if(BlendNode)
				{
					for(INT j=0; j<BlendNode->Children.Num(); j++)
					{
						BlendNode->Children(j).Anim = NULL;
						BlendNode->OnChildAnimChange(j);
					}
				}

				BreakLinksToNode(Node);
			}
		}
	}
}

void WxAnimTreeEditor::OnAddInput(wxCommandEvent& In)
{
	const INT NumSelected = GetNumSelectedByClass<UAnimNode>();
	if(NumSelected == 1)
	{
		UAnimNodeBlendBase* BlendNode = Cast<UAnimNodeBlendBase>(GetFirstSelectedNodeByClass<UAnimNode>());
		if(BlendNode && !BlendNode->bFixNumChildren)
		{
			const INT NewChildIndex = BlendNode->Children.AddZeroed();
			BlendNode->OnAddChild(NewChildIndex);
		}
	}

	ReInitAnimTree();
	RefreshViewport();
	UpdatePropertyWindow();

	AnimTree->MarkPackageDirty();
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

	BlendNode->Children.Remove(ConnIndex);
	BlendNode->OnRemoveChild(ConnIndex);

	ReInitAnimTree();
	RefreshViewport();
	UpdatePropertyWindow();

	AnimTree->MarkPackageDirty();
}

void WxAnimTreeEditor::OnNameInput(wxCommandEvent& In)
{
	if( !ConnObj || ConnType != LOC_OUTPUT )
	{
		return;
	}

	// Only blend nodes have outputs...
	UAnimNodeBlendBase* BlendNode = CastChecked<UAnimNodeBlendBase>(ConnObj);
	check(ConnIndex >= 0 || ConnIndex < BlendNode->Children.Num() );

	// Prompt the user for the name of the input connector
	WxDlgGenericStringEntry dlg;
	if( dlg.ShowModal( TEXT("Rename"), TEXT("Rename"), TEXT("") ) == wxID_OK )
	{
		FString	newName = dlg.GetEnteredString();
		BlendNode->Children(ConnIndex).Name = FName( *newName, FNAME_Add );

		ReInitAnimTree();
		RefreshViewport();
		UpdatePropertyWindow();

		AnimTree->MarkPackageDirty();
	}
}

void WxAnimTreeEditor::OnAddControlHead(wxCommandEvent& In)
{
	// If we have only the AnimTree selected, and we have a preview mesh...
	if( GetFirstSelectedNodeByClass<UAnimNode>() == AnimTree &&
		GetNumSelectedByClass<UAnimNode>() == 1 && 
		PreviewSkelComp->SkeletalMesh )
	{
		// Make list of all bone names in preview skeleton, and let user pick which one to attach controllers to.
		TArray<FString> BoneNames;
		for(INT i=0; i<PreviewSkelComp->SkeletalMesh->RefSkeleton.Num(); i++)
		{
			FName BoneName = PreviewSkelComp->SkeletalMesh->RefSkeleton(i).Name;

			// Check there is not already a chain for this bone...
			UBOOL bAlreadyHasChain = false;
			for(INT j=0; j<AnimTree->SkelControlLists.Num() && !bAlreadyHasChain; j++)
			{
				if(AnimTree->SkelControlLists(j).BoneName == BoneName)
				{
					bAlreadyHasChain = true;
				}
			}

			// If not - add to combo box.
			if(!bAlreadyHasChain)
			{
				BoneNames.AddZeroed();
				BoneNames(BoneNames.Num()-1) = BoneName.ToString();
			}
		}

		// Display dialog and let user pick which bone to create a SkelControl Chain for.
		WxDlgGenericComboEntry dlg;
		if( dlg.ShowModal( TEXT("NewSkelControlChain"), TEXT("BoneName"), BoneNames, 0, TRUE ) == wxID_OK )
		{
			FName BoneName = FName( *dlg.GetSelectedString() );
			if(BoneName != NAME_None)
			{
				FSkelControlListHead ListHead;
				ListHead.BoneName = BoneName;
				ListHead.ControlHead = NULL;

				AnimTree->SkelControlLists.AddItem(ListHead);

				// Re-init the SkelControls list.
				PreviewSkelComp->InitSkelControls();
				RefreshViewport();
				AnimTree->MarkPackageDirty();
			}
		}
	}
}

void WxAnimTreeEditor::OnRemoveControlHead(wxCommandEvent& In)
{
	if( ConnObj == AnimTree )
	{
		check(ConnIndex > 0);
		AnimTree->SkelControlLists.Remove( ConnIndex-2 );

		// Update control table.
		PreviewSkelComp->InitSkelControls();
		RefreshViewport();
		AnimTree->MarkPackageDirty();
	}
}

void WxAnimTreeEditor::OnChangeBoneControlHead(wxCommandEvent& In)
{
	// If we have a preview mesh...
	if( PreviewSkelComp->SkeletalMesh )
	{
		// Make list of all bone names in preview skeleton, and let user pick which
		// one to attach controllers to.
		TArray<FString> BoneNames;
		for(INT i=0; i<PreviewSkelComp->SkeletalMesh->RefSkeleton.Num(); i++)
		{
			FName BoneName = PreviewSkelComp->SkeletalMesh->RefSkeleton(i).Name;

			// Check there is not already a chain for this bone...
			UBOOL bAlreadyHasChain = FALSE;

			for(INT j=0; j<AnimTree->SkelControlLists.Num() && !bAlreadyHasChain; j++)
			{
				if( AnimTree->SkelControlLists(j).BoneName == BoneName )
				{
					bAlreadyHasChain = TRUE;
				}
			}

			// If not - add to combo box.
			if( !bAlreadyHasChain )
			{
				BoneNames.AddZeroed();
				BoneNames(BoneNames.Num()-1) = BoneName.ToString();
			}
		}

		// Display dialog and let user pick which bone shall replace the
		// current bone in the SkelControl Chain.
		WxDlgGenericComboEntry dlg;
		if( dlg.ShowModal( TEXT("NewSkelControlChain"), TEXT("BoneName"), BoneNames, 0, TRUE ) == wxID_OK )
		{
			FName BoneName = FName( *dlg.GetSelectedString() );
			if( BoneName != NAME_None )
			{
				check(ConnIndex > 0);
				FSkelControlListHead& ListHead = AnimTree->SkelControlLists(ConnIndex-2);
				ListHead.BoneName = BoneName;

				// Update control table.
				PreviewSkelComp->InitSkelControls();
				RefreshViewport();
				AnimTree->MarkPackageDirty();
			}
		}

	}
}

void WxAnimTreeEditor::OnDeleteObjects(wxCommandEvent& In)
{
	DeleteSelectedObjects();
}

void WxAnimTreeEditor::OnToggleTickAnimTree( wxCommandEvent& In )
{
	bTickAnimTree = !bTickAnimTree;
}

void WxAnimTreeEditor::OnPreviewSelectedNode( wxCommandEvent& In )
{
	if(GetNumSelectedByClass<UAnimNode>() == 1)
	{
		SetPreviewNode( GetFirstSelectedNodeByClass<UAnimNode>() );
	}
}

/** Toggle showing total weights numerically above each  */
void WxAnimTreeEditor::OnShowNodeWeights( wxCommandEvent& In )
{
	bShowNodeWeights = !bShowNodeWeights;
}

/** Toggle drawing of skeleton. */
void WxAnimTreeEditor::OnShowSkeleton( wxCommandEvent& In )
{
	bShowSkeleton = !bShowSkeleton;
}

/** Toggle drawing of bone names. */
void WxAnimTreeEditor::OnShowBoneNames( wxCommandEvent& In )
{
	bShowBoneNames = !bShowBoneNames;
}

/** Toggle drawing skeletal mesh in wireframe. */
void WxAnimTreeEditor::OnShowWireframe( wxCommandEvent& In )
{
	bShowWireframe = !bShowWireframe;
}

/** Toggle drawing the floor (and allowing collision with it). */
void WxAnimTreeEditor::OnShowFloor( wxCommandEvent& In )
{
	bShowFloor = !bShowFloor;
}

/** Copy the selected nodes **/
void WxAnimTreeEditor::OnCopy(wxCommandEvent &In)
{
	if ( GetNumSelected() > 0 )
	{
		Copy();
	}
}

/** Duplicate the selected nodes - This duplicates currently selected nodes (without clipboard)**/
void WxAnimTreeEditor::OnDuplicate(wxCommandEvent &In)
{
	if ( GetNumSelected() > 0 )
	{
		DuplicateSelectedObjects();
	}
}

/** Paste from clipboard **/
void WxAnimTreeEditor::OnPaste(wxCommandEvent &In)
{
	Paste();
}

//////////////////////////////////////////////////////////////////
// Utils
//////////////////////////////////////////////////////////////////

void WxAnimTreeEditor::RefreshViewport()
{
	LinkedObjVC->Viewport->Invalidate();
}

/** Clear all references from any node to the given UAnimNode. */
void WxAnimTreeEditor::BreakLinksToNode(UAnimNode* InNode)
{
	for(INT i=0; i<TreeNodes.Num(); i++)
	{
		UAnimNodeBlendBase* BlendNode = Cast<UAnimNodeBlendBase>(TreeNodes(i));
		if(BlendNode)
		{
			for(INT j=0; j<BlendNode->Children.Num(); j++)
			{
				if(BlendNode->Children(j).Anim == InNode)
				{
					BlendNode->Children(j).Anim = NULL;
					BlendNode->OnChildAnimChange(j);
				}
			}
		}
	}
}

/** Clear all references from any SkelControl (or the root AnimTree) to the given USkelControlBase. */
void WxAnimTreeEditor::BreakLinksToControl(USkelControlBase* InControl)
{
	// Clear any references in the AnimTree to this SkelControl.
	for( INT i=0; i<AnimTree->SkelControlLists.Num(); i++)
	{
		if(AnimTree->SkelControlLists(i).ControlHead == InControl)
		{
			AnimTree->SkelControlLists(i).ControlHead = NULL;
		}
	}

	// Clear any references to this SkelControl from other SkelControls.
	for(INT i=0; i<TreeNodes.Num(); i++)
	{
		if ( TreeNodes(i)->GetSkelControlBase() )
		{
			if(TreeNodes(i)->GetSkelControlBase()->NextControl == InControl)
			{
				TreeNodes(i)->GetSkelControlBase()->NextControl = NULL;
			}
		}
	}
}

/** Clear all references from any MorphNode (or the root AnimTree) to the given UMorphNodeBase. */
void WxAnimTreeEditor::BreakLinksToMorphNode(UMorphNodeBase* InNode)
{
	// Clear any references in the AnimTree to this node.
	AnimTree->RootMorphNodes.RemoveItem(InNode);

	// Clear any references to this MorphNode from other MorphNode.
	for(INT i=0; i<TreeNodes.Num(); i++)
	{
		if ( TreeNodes(i)->GetMorphNodeBase() )
		{
			// Only MorphNodeWeightBase classes keep references.
			UMorphNodeWeightBase* WeightNode = Cast<UMorphNodeWeightBase>( TreeNodes(i)->GetMorphNodeBase() );
			if(WeightNode)
			{
				// Iterate over each connector
				for(INT j=0; j<WeightNode->NodeConns.Num(); j++)
				{
					// Remove from array connector struct.
					FMorphNodeConn& Conn = WeightNode->NodeConns(j);
					Conn.ChildNodes.RemoveItem(InNode);
				}
			}
		}
	}
}

void WxAnimTreeEditor::DeleteSelectedObjects()
{
	// DElete selected AnimNodes
	for(INT i=0; i<SelectedNodes.Num(); i++)
	{
		UAnimObject * NodeToDelete = SelectedNodes(i);

		// Don't allow AnimTree to be deleted.
		if(NodeToDelete!= AnimTree)
		{
			if (NodeToDelete->IsA(UAnimNodeFrame::StaticClass()))
			{
				AnimTree->AnimNodeFrames.RemoveItem(CastChecked<UAnimNodeFrame>(NodeToDelete));
			}
			else 
			{
				if (NodeToDelete->GetAnimNode())
				{
					BreakLinksToNode(NodeToDelete->GetAnimNode());
				}
				else if (NodeToDelete->GetSkelControlBase())
				{
					BreakLinksToControl(NodeToDelete->GetSkelControlBase());
				}
				else
				{
					BreakLinksToMorphNode(NodeToDelete->GetMorphNodeBase());
				}

				// Clear reference to preview SkeletalMeshComponent.
				NodeToDelete->SkelComponent = NULL;

				// Remove this node from the list of all Nodes (whether in the tree or not).
				check( TreeNodes.ContainsItem(NodeToDelete) );
				TreeNodes.RemoveItem(NodeToDelete);
			}
		}
	}

	SelectedNodes.Empty();

	UpdatePropertyWindow();
	ReInitAnimTree();
	RefreshViewport();
	AnimTree->MarkPackageDirty();
}

void WxAnimTreeEditor::DuplicateSelectedObjects()
{
	// Never allow you to duplicate the AnimTree itself!
	SelectedNodes.RemoveItem(AnimTree);

	// Duplicate the AnimNodes	
	TArray<UAnimNode*> SrcNodes, DestNodes; // Array of newly created nodes.
	TMap<UAnimNode*,UAnimNode*> SrcToDestNodeMap; // Mapping table from src node to newly created node.
	if ( GetSelectedNodeByClass<UAnimNode>(SrcNodes) > 0)
	{
		UAnimTree::CopyAnimNodes(SrcNodes, AnimTree, DestNodes, SrcToDestNodeMap);
	}

	// Duplicate the SkelControls
	TArray<USkelControlBase*> SrcControls, DestControls; // Array of new skel controls.
	TMap<USkelControlBase*, USkelControlBase*> SrcToDestControlMap; // Map from src control to newly created one.
	if ( GetSelectedNodeByClass<USkelControlBase>(SrcControls) > 0)
	{
		UAnimTree::CopySkelControls(SrcControls, AnimTree, DestControls, SrcToDestControlMap);
	}

	// Duplicate the MorphNodes
	TArray<UMorphNodeBase*> SrcMorphNodes, DestMorphNodes; // Array of new morph nodes.
	TMap<UMorphNodeBase*, UMorphNodeBase*> SrcToDestMorphNodeMap; // Map from src node to newly created one.
	if ( GetSelectedNodeByClass<UMorphNodeBase>(SrcMorphNodes) > 0)
	{
		UAnimTree::CopyMorphNodes(SrcMorphNodes, AnimTree, DestMorphNodes, SrcToDestMorphNodeMap);
	}

	// We set the selection to being the newly created nodes.
	EmptySelection();

	// Offset the newly created nodes.
	for(INT i=0; i < DestNodes.Num(); i++)
	{
		UAnimNode* Node = DestNodes(i);
		Node->NodePosX += DuplicateOffset;
		Node->NodePosY += DuplicateOffset;

		TreeNodes.AddItem(Node);
		AddToSelection(Node);
	}


	// Offset the newly created controls.
	for(INT i=0; i < DestControls.Num(); i++)
	{
		USkelControlBase* Control = DestControls(i);
		Control->NodePosX += DuplicateOffset;
		Control->NodePosY += DuplicateOffset;
	
		TreeNodes.AddItem(Control);
		AddToSelection(Control);
	}

	// Offset the newly created morph nodes.
	for(INT i=0; i < DestMorphNodes.Num(); i++)
	{
		UMorphNodeBase* MorphNode = DestMorphNodes(i);
		MorphNode->NodePosX += DuplicateOffset;
		MorphNode->NodePosY += DuplicateOffset;

		TreeNodes.AddItem(MorphNode);
		AddToSelection(MorphNode);
	}

	// Update property window so it's shows the properties of our new nodes, not the ones we duplicated from
	UpdatePropertyWindow();
}

/** 
 *	Update the SkelComponent of all nodes. Will set to NULL any that are no longer in the actual tree.
 *	Also prevents you from viewing a node that is no longer in the tree.
 */
void WxAnimTreeEditor::ReInitAnimTree()
{
	PreviewSkelComp->InitAnimTree();

	// If we are previewing a node which is no longer in the tree, reset to root.
	if( PreviewSkelComp->Animations->SkelComponent == NULL )
	{
		PreviewSkelComp->Animations = AnimTree;
	}
}

UAnimNodeEditInfo* WxAnimTreeEditor::FindAnimNodeEditInfo(UAnimNode* InNode)
{
	for(INT i=0; i<AnimNodeEditInfos.Num(); i++)
	{
		UAnimNodeEditInfo* EditInfo = AnimNodeEditInfos(i);
		if( EditInfo->AnimNodeClass && InNode->IsA(EditInfo->AnimNodeClass) )
		{
			return EditInfo;
		}
	}

	return NULL;
}


/** Update anything (eg the skeletal mesh) in the preview window. */
void WxAnimTreeEditor::TickPreview(FLOAT DeltaSeconds)
{
	const UBOOL bAttached = PreviewSkelComp->IsAttached();
	if( !bEditorClosing && bAttached )
	{
		// Take into account preview play rate
		const FLOAT UpdateDeltaSeconds = DeltaSeconds * Clamp<FLOAT>(AnimTree->PreviewPlayRate, 0.1f, 10.f);

		// Used to ensure nodes/controls are not ticked multiple times if there are multiple routes to one (ie they are shared).
		PreviewSkelComp->TickTag++;

		// Don't tick the anim tree if animation is paused
		if( bTickAnimTree )
		{
			PreviewSkelComp->TickAnimNodes(UpdateDeltaSeconds);
		}

		// update the instanced influence weights if needed
		for (INT LODIdx=0; LODIdx<PreviewSkelComp->LODInfo.Num(); LODIdx++)
		{
			if( PreviewSkelComp->LODInfo(LODIdx).bNeedsInstanceWeightUpdate )
			{
				PreviewSkelComp->UpdateInstanceVertexWeights(LODIdx);
			}
		}

		// Do keep ticking controls though.
		PreviewSkelComp->TickSkelControls(UpdateDeltaSeconds);

		// Update the bones and skeletal mesh.
		PreviewSkelComp->UpdateSkelPose();

#if WITH_APEX
		// Apply wind forces.
		PreviewSkelComp->UpdateClothWindForces(DeltaSeconds);

		PreviewSkelComp->TickApexClothing(DeltaSeconds);

		// Update any cloth attachment
		PreviewSkelComp->UpdateFixedClothVerts();

		// Run physics
		TickRBPhysScene(RBPhysScene, DeltaSeconds);
		WaitRBPhysScene(RBPhysScene);

		DeferredRBResourceCleanup(RBPhysScene);
#endif

		PreviewSkelComp->ConditionalUpdateTransform();
		RefreshViewport();
	}
}


/** Change the node that we are previewing in the 3D window. */
void WxAnimTreeEditor::SetPreviewNode(class UAnimNode* NewPreviewNode)
{
	// First we want to check that this node is in the 'main' tree - connected to the AnimTree root.
	TArray<UAnimNode*> TestNodes;
	AnimTree->GetNodesByClass(TestNodes, UAnimNode::StaticClass());

	if( TestNodes.ContainsItem(NewPreviewNode) )
	{
		PreviewSkelComp->Animations = NewPreviewNode;		
	}
}


///////////////////////////////////////////////////////////////////////////////////////
// Properties window NotifyHook stuff

void WxAnimTreeEditor::NotifyDestroy( void* Src )
{

}

void WxAnimTreeEditor::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{
	GEditor->BeginTransaction( *LocalizeUnrealEd("EditLinkedObj") );

	for ( WxPropertyWindow::TObjectIterator Itor( PropertyWindow->ObjectIterator() ) ; Itor ; ++Itor )
	{
		(*Itor)->Modify();
	}
}

void WxAnimTreeEditor::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	GEditor->EndTransaction();

	// If we are editing the AnimTree (root) and know what changed.
	if( PropertyThatChanged && (SelectedNodes.Num() == 0 || SelectedNodes.ContainsItem(AnimTree)) )
	{
		UBOOL bWasDisplayName = (PropertyThatChanged->GetFName() == FName(TEXT("DisplayName")));

		// If it was the SkeletalMesh, update the preview SkeletalMeshComponent
		if( PropertyThatChanged->GetFName() == FName(TEXT("PreviewMeshList"))
			|| PropertyThatChanged->GetFName() == FName(TEXT("PreviewSkelMesh"))
			|| PropertyThatChanged->GetFName() == FName(TEXT("PreviewMorphSets"))
			|| bWasDisplayName )
		{
			UpdatePreviewMeshListCombo();
			UpdatePreviewMesh();
		}

		// If it was the PreviewAnimSets array, update the preview SkelMeshComponent and re-init tree (to pick up new AnimSequences).
		if( PropertyThatChanged->GetFName() == FName(TEXT("PreviewAnimSetList")) 
			|| PropertyThatChanged->GetFName() == FName(TEXT("PreviewAnimSets"))
			|| bWasDisplayName )
		{
			UpdatePreviewAnimSetListCombo();
			UpdatePreviewAnimSet();
			RefreshPreviewAnimSetCombo();
		}

		// Sockets
		if( PropertyThatChanged->GetFName() == FName(TEXT("PreviewSocketList")) 
			|| PropertyThatChanged->GetFName() == FName(TEXT("SocketName"))
			|| PropertyThatChanged->GetFName() == FName(TEXT("PreviewSkelMesh"))
			|| PropertyThatChanged->GetFName() == FName(TEXT("PreviewStaticMesh"))
			|| bWasDisplayName )
		{
			UpdatePreviewSocketListCombo();
			UpdatePreviewSocket();
		}
	}

	RefreshViewport();
	AnimTree->MarkPackageDirty();
}

void WxAnimTreeEditor::NotifyExec( void* Src, const TCHAR* Cmd )
{
	GUnrealEd->NotifyExec(Src, Cmd);
}


void WxAnimTreeEditor::ClearSocketPreview()
{
	if( PreviewSkelComp && SocketComponent )
	{
		PreviewSkelComp->DetachComponent(SocketComponent);

		// delete is evil... and I trusted James ;)
		//delete SocketComponent;
		SocketComponent = NULL;
	}
}


/** Update the Skeletal and StaticMesh Components used to preview attachments in the editor. */
void WxAnimTreeEditor::RecreateSocketPreview()
{
	ClearSocketPreview();

	if( AnimTree && PreviewSkelComp && PreviewSkelComp->SkeletalMesh 
		&& AnimTree->PreviewSocketIndex < AnimTree->PreviewSocketList.Num()
		) 
	{
		FPreviewSocketStruct& SocketPreview = AnimTree->PreviewSocketList(AnimTree->PreviewSocketIndex);
		if( PreviewSkelComp->SkeletalMesh->FindSocket(SocketPreview.SocketName) )
		{
			if( SocketPreview.PreviewSkelMesh )
			{
				// Create SkeletalMeshComponent and fill in mesh and scene.
				USkeletalMeshComponent* NewSkelComp = ConstructObject<USkeletalMeshComponent>(USkeletalMeshComponent::StaticClass());
				NewSkelComp->SetSkeletalMesh(SocketPreview.PreviewSkelMesh);

				// Attach component to this socket.
				PreviewSkelComp->AttachComponentToSocket(NewSkelComp, SocketPreview.SocketName);

				// And keep track of it.
				SocketComponent = NewSkelComp;
			}

			if( SocketPreview.PreviewStaticMesh )
			{
				// Create StaticMeshComponent and fill in mesh and scene.
				UStaticMeshComponent* NewMeshComp = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());
				NewMeshComp->SetStaticMesh(SocketPreview.PreviewStaticMesh);

				// Attach component to this socket.
				PreviewSkelComp->AttachComponentToSocket(NewMeshComp, SocketPreview.SocketName);

				// And keep track of it
				SocketComponent = NewMeshComp;
			}
		}
	}
}

void WxAnimTreeEditor::OnPreviewMeshListCombo( wxCommandEvent &In )
{
	AnimTree->PreviewMeshIndex = ToolBar->PreviewMeshListCombo->GetCurrentSelection();
	UpdatePreviewMesh();
}

void WxAnimTreeEditor::OnPreviewAnimSetListCombo( wxCommandEvent &In )
{
	AnimTree->PreviewAnimSetListIndex = ToolBar->PreviewAnimSetListCombo->GetCurrentSelection();
	UpdatePreviewAnimSet();
}

void WxAnimTreeEditor::OnPreviewSocketListCombo( wxCommandEvent &In )
{
	AnimTree->PreviewSocketIndex = ToolBar->PreviewSocketListCombo->GetCurrentSelection();
	UpdatePreviewSocket();
}

/**
 * Notification of AnimSet Combo change
 */
void WxAnimTreeEditor::OnPreviewAnimSetCombo( wxCommandEvent &In )
{
	AnimTree->PreviewAnimSetIndex = ToolBar->PreviewAnimSetCombo->GetCurrentSelection();
	// when animset is changed, refresh sequence list
	RefreshPreviewAnimSequenceCombo();
}

/**
 * Notification of AnimSequence Combo change
 */
void WxAnimTreeEditor::OnPreviewAnimSequenceCombo( wxCommandEvent &In )
{
	// when anim sequence is changed, replace sequence name of current selected node with this new name
	ReplaceSequenceName( FName(ToolBar->PreviewAnimSequenceCombo->GetValue(), FNAME_Find) );
}

void WxAnimTreeEditor::OnPreviewRateChanged( wxScrollEvent& In )
{
	if (AnimTree)
	{
		AnimTree->PreviewPlayRate = ToolBar->RateSlideBar->GetValue()/100.f;
	}
}

/** 
* Refresh AnimSet Combo List (called init or post edit of preview animsets)
* Triggers UpdatePreviewAnimSequenceCombo
*/
void WxAnimTreeEditor::RefreshPreviewAnimSetCombo()
{
	ToolBar->PreviewAnimSetCombo->Freeze();
	ToolBar->PreviewAnimSetCombo->Clear();

	// Refresh PreviewAnimSet List
	if ( AnimTree->PreviewAnimSetList.Num() > 0 )
	{
		const TArray<UAnimSet*>& AnimSets = AnimTree->PreviewAnimSetList(0).PreviewAnimSets;
		
		for(INT Index=0; Index<AnimSets.Num(); Index++)
		{
			ToolBar->PreviewAnimSetCombo->Append( *AnimSets(Index)->GetName() ) ;
		}

		if( AnimTree->PreviewAnimSetIndex >= AnimSets.Num() )
		{
			AnimTree->PreviewAnimSetIndex = 0;
		}
	}

	ToolBar->PreviewAnimSetCombo->SetSelection(AnimTree->PreviewAnimSetIndex);
	ToolBar->PreviewAnimSetCombo->Thaw();

	RefreshPreviewAnimSequenceCombo();
}

/**
* Refresh AnimSequence List whenever Animset Changes 
*/
void WxAnimTreeEditor::RefreshPreviewAnimSequenceCombo()
{
	ToolBar->PreviewAnimSequenceCombo->Freeze();
	ToolBar->PreviewAnimSequenceCombo->Clear();

	// Add default one
	ToolBar->PreviewAnimSequenceCombo->Append(TEXT(""));// empty one

	// find what's selected
	if ( AnimTree->PreviewAnimSetList.Num() > 0 )
	{
		const TArray<UAnimSet*>& AnimSets = AnimTree->PreviewAnimSetList(0).PreviewAnimSets;
		if ( AnimTree->PreviewAnimSetIndex >= 0 && AnimTree->PreviewAnimSetIndex < AnimSets.Num() )
		{
			if ( AnimSets(AnimTree->PreviewAnimSetIndex) )
			{
				const TArray<UAnimSequence*>& AnimSequences = AnimSets(AnimTree->PreviewAnimSetIndex)->Sequences;

				for(INT Index=0; Index<AnimSequences.Num(); Index++)
				{
					ToolBar->PreviewAnimSequenceCombo->Append( *AnimSequences(Index)->SequenceName.ToString() ) ;
				}
			}
		}
	}

	ToolBar->PreviewAnimSequenceCombo->SetSelection(0);
	ToolBar->PreviewAnimSequenceCombo->Thaw();

	UpdateCurrentlySelectedAnimSequence();
}

/**
* Find Currently Selected  sequence name
* for AnimSequenceCombo - if AnimNodeSequence is selected
* This is triggered by add/remove selection code
* Can't just refresh animset combo - that can create cycling
*/
void WxAnimTreeEditor::FindCurrentlySelectedAnimSequence()
{
	UpdateCurrentlySelectedAnimSequence();

	// did not find it if selection == 0
	if ( ToolBar->PreviewAnimSequenceCombo->GetSelection() == 0 )
	{
		// refresh animset list and attempt to find it
		UAnimNodeSequence * SelectedAnimNodeSequence = GetCurrentlySelectedAnimNodeSequence();
		if ( SelectedAnimNodeSequence && SelectedAnimNodeSequence->AnimSeq )
		{
			UAnimSet* AnimSet = SelectedAnimNodeSequence->AnimSeq->GetAnimSet();
			if ( AnimSet )
			{
				INT OldSelection = ToolBar->PreviewAnimSetCombo->GetSelection();

				// Selection has been changed
				ToolBar->PreviewAnimSetCombo->SetValue(*AnimSet->GetName());
				INT NewSelection  = ToolBar->PreviewAnimSetCombo->GetSelection();
				if ( OldSelection != NewSelection )
				{
					AnimTree->PreviewAnimSetIndex = NewSelection;
					RefreshPreviewAnimSequenceCombo();
				}
			}
		}
	}
}
/**
* Update/Set Currently Selected  sequence name
* for AnimSequenceCombo - if AnimNodeSequence is selected
*
* @param: SequenceName - Sequence name to select
*/
void WxAnimTreeEditor::UpdateCurrentlySelectedAnimSequence()
{
	UAnimNodeSequence * SelectedAnimNodeSequence = GetCurrentlySelectedAnimNodeSequence();
	// Clear the value, so if not found, it will stay as cleared
	ToolBar->PreviewAnimSequenceCombo->SetSelection(0);

	debugf(TEXT("Refreshing currently selected anim sequence"));
	if ( SelectedAnimNodeSequence )
	{
		ToolBar->PreviewAnimSequenceCombo->SetValue(*SelectedAnimNodeSequence->AnimSeqName.GetNameString());
	}
}

/**
* Change sequence name
* if AnimNodeSequence is selected
*
* @param: NewSequenceName - Sequence name to change
*/
void WxAnimTreeEditor::ReplaceSequenceName(const FName NewSequenceName)
{
	if ( NewSequenceName != NAME_None )
	{
		UAnimNodeSequence * SelectedAnimNodeSequence = GetCurrentlySelectedAnimNodeSequence();
		if ( SelectedAnimNodeSequence )
		{
			SelectedAnimNodeSequence->SetAnim(NewSequenceName);

			// update property window for it to refresh
			UpdatePropertyWindow();
		}
	}
}

/** 
* This is used by previewing anim node sequence
* This only returns if only one AnimNodeSequence is selected
* if multiple, this is not going to return first selected on
* Use GetFirstSelectedNodeByClass<UAnimNodeSequence>() 
* if you need to get first selected animnodeseuqence instead 
*/
UAnimNodeSequence * WxAnimTreeEditor::GetCurrentlySelectedAnimNodeSequence()
{
	if ( GetNumSelected() == 1 ) 
	{
		return GetFirstSelectedNodeByClass<UAnimNodeSequence>();
	}

	return NULL;
}

void WxAnimTreeEditor::UpdatePreviewMeshListCombo()
{
	ToolBar->PreviewMeshListCombo->Freeze();
	ToolBar->PreviewMeshListCombo->Clear();

	for(INT i=0; i<AnimTree->PreviewMeshList.Num(); i++)
	{
		ToolBar->PreviewMeshListCombo->Append( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("AnimTreeEdComboMesh_F"), *(AnimTree->PreviewMeshList(i).DisplayName.ToString()) ) ) );
	}

	if( AnimTree->PreviewMeshIndex >= AnimTree->PreviewMeshList.Num() )
	{
		AnimTree->PreviewMeshIndex = 0;
	}
	ToolBar->PreviewMeshListCombo->SetSelection(AnimTree->PreviewMeshIndex);
	ToolBar->PreviewMeshListCombo->Thaw();

	// update preview animset combo
	UpdatePreviewAnimSetListCombo();
}

void WxAnimTreeEditor::UpdatePreviewAnimSetListCombo()
{
	ToolBar->PreviewAnimSetListCombo->Freeze();
	ToolBar->PreviewAnimSetListCombo->Clear();

	for(INT i=0; i<AnimTree->PreviewAnimSetList.Num(); i++)
	{
		ToolBar->PreviewAnimSetListCombo->Append( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("AnimTreeEdComboAnimSet_F"), *(AnimTree->PreviewAnimSetList(i).DisplayName.ToString()) ) ) );
	}

	if( AnimTree->PreviewAnimSetListIndex >= AnimTree->PreviewAnimSetList.Num() )
	{
		AnimTree->PreviewAnimSetListIndex = 0;
	}
	ToolBar->PreviewAnimSetListCombo->SetSelection(AnimTree->PreviewAnimSetListIndex);
	ToolBar->PreviewAnimSetListCombo->Thaw();
}

void WxAnimTreeEditor::UpdatePreviewSocketListCombo()
{
	ToolBar->PreviewSocketListCombo->Freeze();
	ToolBar->PreviewSocketListCombo->Clear();

	for(INT i=0; i<AnimTree->PreviewSocketList.Num(); i++)
	{
		ToolBar->PreviewSocketListCombo->Append( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("AnimTreeEdComboSocket_F"), *(AnimTree->PreviewSocketList(i).DisplayName.ToString()) ) ) );
	}

	if( AnimTree->PreviewSocketIndex >= AnimTree->PreviewSocketList.Num() )
	{
		AnimTree->PreviewSocketIndex = 0;
	}
	ToolBar->PreviewSocketListCombo->SetSelection(AnimTree->PreviewSocketIndex);
	ToolBar->PreviewSocketListCombo->Thaw();
}


void WxAnimTreeEditor::UpdatePreviewMesh()
{
	if( AnimTree->PreviewMeshIndex < AnimTree->PreviewMeshList.Num() )
	{
		PreviewSkelComp->SetSkeletalMesh(AnimTree->PreviewMeshList(AnimTree->PreviewMeshIndex).PreviewSkelMesh);
		PreviewSkelComp->MorphSets = AnimTree->PreviewMeshList(AnimTree->PreviewMeshIndex).PreviewMorphSets;
	}
	else
	{
		AnimTree->PreviewMeshIndex = 0;
		PreviewSkelComp->SetSkeletalMesh(NULL);
		PreviewSkelComp->MorphSets.Empty();
	}
	PreviewSkelComp->MorphTargetsQueried.Empty();
	PreviewSkelComp->InitAnimTree(TRUE);
	PreviewSkelComp->InitMorphTargets();
	AnimTree->InitTreeMorphNodes(PreviewSkelComp);
	UpdatePreviewSocket();

#if WITH_APEX
	PreviewSkelComp->ReleaseApexClothing();
	PreviewSkelComp->InitApexClothing(RBPhysScene);
#endif

	debugf(TEXT("UpdatePreviewMesh: %d, %s"), AnimTree->PreviewMeshIndex, PreviewSkelComp->SkeletalMesh ? *PreviewSkelComp->SkeletalMesh->GetFName().ToString() : TEXT("None") );
}

void WxAnimTreeEditor::UpdatePreviewAnimSet()
{
	if( AnimTree->PreviewAnimSetListIndex < AnimTree->PreviewAnimSetList.Num() )
	{
		PreviewSkelComp->AnimSets = AnimTree->PreviewAnimSetList(AnimTree->PreviewAnimSetListIndex).PreviewAnimSets;
	}
	else
	{
		AnimTree->PreviewAnimSetListIndex = 0;
		PreviewSkelComp->AnimSets.Empty();
	}

	debugf(TEXT("UpdatePreviewAnimSet: %d"), AnimTree->PreviewAnimSetListIndex);
	PreviewSkelComp->UpdateAnimations();
}

void WxAnimTreeEditor::UpdatePreviewSocket()
{
	RecreateSocketPreview();
}


void WxAnimTreeEditor::OnAddNewEntryPreviewSkelMesh( wxCommandEvent& In )
{
	// If our selection is not valid, just create and empty and entry, and that's it
	if( AnimTree->PreviewMeshIndex >= AnimTree->PreviewMeshList.Num() )
	{
		AnimTree->PreviewMeshList.AddZeroed(1);
		return;
	}

	// Create empty entry
	AnimTree->PreviewMeshList.AddZeroed(1);

	// Copy current selection to new entry
	AnimTree->PreviewMeshList(AnimTree->PreviewMeshList.Num() - 1) = AnimTree->PreviewMeshList(AnimTree->PreviewMeshIndex);
}

void WxAnimTreeEditor::OnAddNewEntryPreviewAnimSet( wxCommandEvent& In )
{
	// If our selection is not valid, just create and empty and entry, and that's it
	if( AnimTree->PreviewAnimSetListIndex >= AnimTree->PreviewAnimSetList.Num() )
	{
		AnimTree->PreviewAnimSetList.AddZeroed(1);
		return;
	}

	// Create empty entry
	AnimTree->PreviewAnimSetList.AddZeroed(1);

	// Copy current selection to new entry
	AnimTree->PreviewAnimSetList(AnimTree->PreviewAnimSetList.Num() - 1) = AnimTree->PreviewAnimSetList(AnimTree->PreviewAnimSetListIndex);

	// when new animset is added, refresh animset combo
	RefreshPreviewAnimSetCombo();
}

void WxAnimTreeEditor::OnAddNewEntryPreviewSocket( wxCommandEvent& In )
{
	// If our selection is not valid, just create and empty and entry, and that's it
	if( AnimTree->PreviewSocketIndex >= AnimTree->PreviewSocketList.Num() )
	{
		AnimTree->PreviewSocketList.AddZeroed(1);
		return;
	}

	// Create empty entry
	AnimTree->PreviewSocketList.AddZeroed(1);

	// Copy current selection to new entry
	AnimTree->PreviewSocketList(AnimTree->PreviewSocketList.Num() - 1) = AnimTree->PreviewSocketList(AnimTree->PreviewSocketIndex);
}


//////////////////////////////////////////////////////////////////////////
// Comment related
//////////////////////////////////////////////////////////////////////////

/** 
 *	Calculate the bounding box that encompasses the selected SequenceObjects. 
 *	Does not produce sensible result if nothing is selected. 
 */
FIntRect WxAnimTreeEditor::CalcBoundingBoxOfSelected()
{
	if (SelectedNodes.Num() > 0)
	{
		return CalcBoundingBoxOfSequenceObjects( SelectedNodes );
	}

	FIntRect Result(0, 0, 0, 0);
	Result.Min.X = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
	Result.Min.Y = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;
	Result.Max.X = Result.Min.X + 128;
	Result.Max.Y = Result.Min.Y + 64;

	return Result;
}

/**
 * Calculate the combined bounding box of the provided sequence objects.
 * Does not produce sensible result if no objects are provided in the array.
 *
 * @param	InSequenceObjects	Objects to calculate the bounding box of
 *
 * @return	Rectangle representing the bounding box of the provided objects
 */
FIntRect WxAnimTreeEditor::CalcBoundingBoxOfSequenceObjects(const TArray<UAnimObject*>& InAnimObjects)
{
	FIntRect Result(0, 0, 0, 0);
	UBOOL bResultValid = FALSE;

	for ( TArray<UAnimObject*>::TConstIterator ObjIter( InAnimObjects ); ObjIter; ++ObjIter )
	{
		UAnimObject* CurObj = *ObjIter;
		FIntRect ObjBox = CurObj->GetObjBoundingBox();

		if( bResultValid )
		{
			Result.Min.X = ::Min(Result.Min.X, ObjBox.Min.X);
			Result.Min.Y = ::Min(Result.Min.Y, ObjBox.Min.Y);

			Result.Max.X = ::Max(Result.Max.X, ObjBox.Max.X);
			Result.Max.Y = ::Max(Result.Max.Y, ObjBox.Max.Y);
		}
		else
		{
			Result = ObjBox;
			bResultValid = TRUE;
		}
	}

	return Result;
}

void WxAnimTreeEditor::OnNewComment(wxCommandEvent &In)
{
	// create new comment
	FString CommentText;
	WxDlgGenericStringEntry dlg;
	static INT	FitCommentToSelectedBorder = 30;
	INT Result = dlg.ShowModal( TEXT("NewComment"), TEXT("CommentText"), TEXT("Comment") );
	if (Result != wxID_OK)
	{
		return;
	}
	else
	{
		CommentText = dlg.GetEnteredString();
	}

	UAnimNodeFrame* NewFrame = ConstructObject<UAnimNodeFrame>( UAnimNodeFrame::StaticClass(), AnimTree, NAME_None, RF_Transactional);
	if (NewFrame)
	{
		AnimTree->AnimNodeFrames.AddItem(NewFrame);

		NewFrame->ObjComment = CommentText;

		FIntRect SelectedRect = CalcBoundingBoxOfSelected();
		NewFrame->NodePosX = SelectedRect.Min.X - (FitCommentToSelectedBorder);
		NewFrame->NodePosY = SelectedRect.Min.Y - (FitCommentToSelectedBorder);
		//NewFrame->SnapPosition(KISMET_GRIDSIZE, MaxSequenceSize);
		NewFrame->SizeX = (SelectedRect.Max.X - SelectedRect.Min.X) + (2 * (FitCommentToSelectedBorder));
		NewFrame->SizeY = (SelectedRect.Max.Y - SelectedRect.Min.Y) + (2 * (FitCommentToSelectedBorder));
		NewFrame->bDrawBox = TRUE;
	}
}


/** Move selected comment to front (last in array). */
void WxAnimTreeEditor::OnContextCommentToFront( wxCommandEvent &In )
{
	if(SelectedNodes.Num() > 0)
	{
		UAnimNodeFrame* Comment = Cast<UAnimNodeFrame>(SelectedNodes(0));
		if(Comment)
		{
			FScopedObjectStateChange Notifier(Comment);

			// Remove from array and add at end.
			AnimTree->AnimNodeFrames.RemoveItem(Comment);
			AnimTree->AnimNodeFrames.AddItem(Comment);
		}
	}
}

/** Move selected comment to back (first in array). */
void WxAnimTreeEditor::OnContextCommentToBack( wxCommandEvent &In )
{
	if(SelectedNodes.Num() > 0)
	{
		UAnimNodeFrame* Comment = Cast<UAnimNodeFrame>(SelectedNodes(0));
		if(Comment)
		{
			FScopedObjectStateChange Notifier(AnimTree);

			// Remove from array and add at end.
			AnimTree->AnimNodeFrames.RemoveItem(Comment);
			AnimTree->AnimNodeFrames.InsertItem(Comment, 0);
		}
	}
}

