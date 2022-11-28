/*=============================================================================
	PhATTool.cpp: Physics Asset Tool general tools/modal stuff
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EnginePhysicsClasses.h"
#include "..\..\Launch\Resources\resource.h"

static const FLOAT	TranslateSpeed  = 0.25f;
static const FLOAT  RotateSpeed     = 1.0f * ( static_cast<FLOAT>( PI ) / 180.0f );

static const FLOAT	DefaultPrimSize = 15.0f;
static const FLOAT	MinPrimSize     = 0.5f;

static const FLOAT	SimGrabCheckDistance        = 5000.0f;
static const FLOAT	SimHoldDistanceChangeDelta  = 20.0f;
static const FLOAT	SimMinHoldDistance          = 10.0f;
static const FLOAT  SimGrabMoveSpeed            = 1.0f;

static const FLOAT	DuplicateXOffset            = 10.0f;

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// WPhAT //////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void WPhAT::RecalcEntireAsset( void )
{
	UBOOL bDoRecalc = appMsgf(1, TEXT("This will completely replace the current asset.\nAre you sure?"));
	if(!bDoRecalc)
		return;

	// Then calculate a new one.

	WxDlgNewPhysicsAsset dlg;
	if( dlg.ShowModal( FString(TEXT("")), FString(PhysicsAsset->GetName()), NULL, true ) == wxID_OK )
	{
		// Deselect everything.
		SetSelectedBody(INDEX_NONE, KPT_Unknown, INDEX_NONE);
		SetSelectedConstraint(INDEX_NONE);	

		// Empty current asset data.
		PhysicsAsset->BodySetup.Empty();
		PhysicsAsset->ConstraintSetup.Empty();
		PhysicsAsset->DefaultInstance->Bodies.Empty();
		PhysicsAsset->DefaultInstance->Constraints.Empty();

		PhysicsAsset->CreateFromSkeletalMesh(EditorSkelMesh, dlg.Params);

		PhATViewportClient->Viewport->Invalidate();
	}

}

void WPhAT::ResetBoneCollision(INT BodyIndex)
{
	if(BodyIndex == INDEX_NONE)
		return;

	UBOOL bDoRecalc = appMsgf(1, TEXT("This will completely replace the current bone collision.\nAre you sure?"));
	if(!bDoRecalc)
		return;

	WxDlgNewPhysicsAsset dlg;
	if( dlg.ShowModal( FString(TEXT("")), FString(PhysicsAsset->GetName()), NULL, true ) != wxID_OK )
		return;

	URB_BodySetup* bs = PhysicsAsset->BodySetup(BodyIndex);
	check(bs);

	INT BoneIndex = EditorSkelMesh->MatchRefBone(bs->BoneName);
	check(BoneIndex != INDEX_NONE);

	if(dlg.Params.VertWeight == EVW_DominantWeight)
		PhysicsAsset->CreateCollisionFromBone(bs, EditorSkelMesh, BoneIndex, dlg.Params, DominantWeightBoneInfos);
	else
		PhysicsAsset->CreateCollisionFromBone(bs, EditorSkelMesh, BoneIndex, dlg.Params, AnyWeightBoneInfos);

	SetSelectedBodyAnyPrim(BodyIndex);

	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::SetSelectedBodyAnyPrim(INT BodyIndex)
{
	if(BodyIndex == INDEX_NONE)
	{
		SetSelectedBody(INDEX_NONE, KPT_Unknown, INDEX_NONE);
		return;
	}
	
	URB_BodySetup* bs = PhysicsAsset->BodySetup(BodyIndex);
	check(bs);

	if(bs->AggGeom.SphereElems.Num() > 0)
		SetSelectedBody(BodyIndex, KPT_Sphere, 0);
	else if(bs->AggGeom.BoxElems.Num() > 0)
		SetSelectedBody(BodyIndex, KPT_Box, 0);
	else if(bs->AggGeom.SphylElems.Num() > 0)
		SetSelectedBody(BodyIndex, KPT_Sphyl, 0);
	else if(bs->AggGeom.ConvexElems.Num() > 0)
		SetSelectedBody(BodyIndex, KPT_Convex, 0);
	else
		appErrorf(TEXT("Body Setup with No Primitives!")); 

}


void WPhAT::SetSelectedBody(INT BodyIndex, EKCollisionPrimitiveType PrimitiveType, INT PrimitiveIndex)
{
	SelectedBodyIndex = BodyIndex;
	SelectedPrimitiveType = PrimitiveType;
	SelectedPrimitiveIndex = PrimitiveIndex;	

	if(SelectedBodyIndex == INDEX_NONE)
	{
		// No bone selected
		PropertyWindow->Root.SetObjects((UObject**)&EditorSimOptions, 1);

		for( INT i=0; i<PropertyWindow->List.GetCount(); i++ )
		{
			FTreeItem* Item = PropertyWindow->GetListItem(i);
			if(Item->Expandable && !Item->Expanded && (Item->GetCaption() == TEXT("KMeshProps") || Item->GetCaption() == TEXT("RB_BodySetup")) )
				Item->Expand();
		}
	}
	else
	{
		check( SelectedBodyIndex >= 0 && SelectedBodyIndex < PhysicsAsset->BodySetup.Num() );

		// Set properties dialog to display selected bone info.
		PropertyWindow->Root.SetObjects((UObject**)&PhysicsAsset->BodySetup(SelectedBodyIndex), 1);

		for( INT i=0; i<PropertyWindow->List.GetCount(); i++ )
		{
			FTreeItem* Item = PropertyWindow->GetListItem(i);
			if(Item->Expandable && !Item->Expanded && (Item->GetCaption() == TEXT("KMeshProps") || Item->GetCaption() == TEXT("RB_BodySetup")) )
				Item->Expand();
		}
	}

	NextSelectEvent = PNS_Normal;
	UpdateControlledBones();
	UpdateNoCollisionBodies();
	UpdateToolBarStatus();
	PhATViewportClient->Viewport->Invalidate();

}

// Fill in array of graphics bones currently being moved by selected physics body.
void WPhAT::UpdateControlledBones()
{
	ControlledBones.Empty();

	// Not bone selected.
	if(SelectedBodyIndex == INDEX_NONE)
		return;

	for(INT i=0; i<EditorSkelMesh->RefSkeleton.Num(); i++)
	{
		INT ControllerBodyIndex = PhysicsAsset->FindControllingBodyIndex(EditorSkelMesh, i);
		if(ControllerBodyIndex == SelectedBodyIndex)
			ControlledBones.AddItem(i);
	}

}

// Update NoCollisionBodies array with indices of all bodies that have no collision with the selected one.
void WPhAT::UpdateNoCollisionBodies()
{
	NoCollisionBodies.Empty();

	// Query disable table with selected body and every other body.
	for(INT i=0; i<PhysicsAsset->BodySetup.Num(); i++)
	{
		// Add any bodies with bNoCollision
		if( PhysicsAsset->BodySetup(i)->bNoCollision )
		{
			NoCollisionBodies.AddItem(i);
		}
		else if(SelectedBodyIndex != INDEX_NONE && i != SelectedBodyIndex)
		{
			// Add this body if it has disabled collision with selected.
			QWORD Key = RigidBodyIndicesToKey(i, SelectedBodyIndex);

			if( PhysicsAsset->BodySetup(SelectedBodyIndex)->bNoCollision ||
				PhysicsAsset->DefaultInstance->CollisionDisableTable.Find(Key) )
				NoCollisionBodies.AddItem(i);
		}
	}

}

void WPhAT::SetSelectedConstraint(INT ConstraintIndex)
{
	SelectedConstraintIndex = ConstraintIndex;

	if(SelectedConstraintIndex == INDEX_NONE)
	{
		PropertyWindow->Root.SetObjects((UObject**)&EditorSimOptions, 1);
	}
	else
	{
		check( SelectedConstraintIndex >= 0 && SelectedConstraintIndex < PhysicsAsset->ConstraintSetup.Num() );
		check( PhysicsAsset->ConstraintSetup.Num() == PhysicsAsset->DefaultInstance->Constraints.Num() );

		PropertyWindow->Root.SetObjects((UObject**)&PhysicsAsset->ConstraintSetup(SelectedConstraintIndex), 1);

		for( INT i=0; i<PropertyWindow->List.GetCount(); i++ )
		{
			FTreeItem* Item = PropertyWindow->GetListItem(i);
			if(Item->Expandable && !Item->Expanded && (Item->GetCaption() == TEXT("Linear") || Item->GetCaption() == TEXT("Angular")) )
				Item->Expand();
		}
	}	

	NextSelectEvent = PNS_Normal;
	UpdateToolBarStatus();
	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::DisableCollisionWithNextSelect()
{
	if(EditingMode != PEM_BodyEdit || SelectedBodyIndex == INDEX_NONE)
		return;

	NextSelectEvent = PNS_DisableCollision;
	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::EnableCollisionWithNextSelect()
{
	if(EditingMode != PEM_BodyEdit || SelectedBodyIndex == INDEX_NONE)
		return;

	NextSelectEvent = PNS_EnableCollision;
	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::CopyPropertiesToNextSelect()
{
	if(EditingMode == PEM_ConstraintEdit && SelectedConstraintIndex == INDEX_NONE)
		return;

	if(EditingMode == PEM_BodyEdit && SelectedBodyIndex == INDEX_NONE)
		return;

	NextSelectEvent = PNS_CopyProperties;
	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::WeldBodyToNextSelect()
{
	if(bRunningSimulation || EditingMode != PEM_BodyEdit || SelectedBodyIndex == INDEX_NONE)
		return;

	NextSelectEvent = PNS_WeldBodies;
	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::SetCollisionBetween(INT Body1Index, INT Body2Index, UBOOL bEnableCollision)
{
	if(Body1Index != INDEX_NONE && Body2Index != INDEX_NONE && Body1Index != Body2Index)
	{
		URB_BodyInstance* bi1 = PhysicsAsset->DefaultInstance->Bodies(Body1Index);
		URB_BodyInstance* bi2 = PhysicsAsset->DefaultInstance->Bodies(Body2Index);

		if(bEnableCollision)
			PhysicsAsset->DefaultInstance->EnableCollision(bi1, bi2);
		else
			PhysicsAsset->DefaultInstance->DisableCollision(bi1, bi2);

		UpdateNoCollisionBodies();
	}

	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::CopyBodyProperties(INT ToBodyIndex, INT FromBodyIndex)
{
	if(ToBodyIndex != INDEX_NONE && FromBodyIndex != INDEX_NONE && ToBodyIndex != FromBodyIndex)
	{
		URB_BodySetup* tbs = PhysicsAsset->BodySetup(ToBodyIndex);
		URB_BodySetup* fbs = PhysicsAsset->BodySetup(FromBodyIndex);

		tbs->CopyBodyPropertiesFrom(fbs);
	}

	SetSelectedBodyAnyPrim(ToBodyIndex);

	PhATViewportClient->Viewport->Invalidate();

}

// Supplied body must be a direct child of a bone controlled by selected body.
void WPhAT::WeldBodyToSelected(INT AddBodyIndex)
{
	if(AddBodyIndex == INDEX_NONE || SelectedBodyIndex == INDEX_NONE || AddBodyIndex == SelectedBodyIndex)
	{
		PhATViewportClient->Viewport->Invalidate();
		return;
	}

	FName SelectedBoneName = PhysicsAsset->BodySetup(SelectedBodyIndex)->BoneName;
	INT SelectedBoneIndex = EditorSkelMesh->MatchRefBone(SelectedBoneName);
	check(SelectedBoneIndex != INDEX_NONE);

	FName AddBoneName = PhysicsAsset->BodySetup(AddBodyIndex)->BoneName;
	INT AddBoneIndex = EditorSkelMesh->MatchRefBone(AddBoneName);
	check(AddBoneIndex != INDEX_NONE);

	INT AddBoneParentIndex = EditorSkelMesh->RefSkeleton(AddBoneIndex).ParentIndex;
	INT SelectedBoneParentIndex = EditorSkelMesh->RefSkeleton(SelectedBoneIndex).ParentIndex;

	INT ChildBodyIndex = INDEX_NONE, ParentBodyIndex = INDEX_NONE;
	FName ParentBoneName;

	if( PhysicsAsset->FindControllingBodyIndex(EditorSkelMesh, AddBoneParentIndex) == SelectedBodyIndex )
	{
		ChildBodyIndex = AddBodyIndex;
		ParentBodyIndex = SelectedBodyIndex;
		ParentBoneName = SelectedBoneName;
	}
	else if( PhysicsAsset->FindControllingBodyIndex(EditorSkelMesh, SelectedBoneParentIndex) == AddBodyIndex )
	{
		ChildBodyIndex = SelectedBodyIndex;
		ParentBodyIndex = AddBodyIndex;
		ParentBoneName = AddBoneName;
	}
	else
	{
		appMsgf(0, TEXT("You can only weld parent/child pairs."));
		return;
	}

	check(ChildBodyIndex != INDEX_NONE);
	check(ParentBodyIndex != INDEX_NONE);

	//UBOOL bDoWeld = appMsgf(1,  *FString::Printf(TEXT("Are you sure you want to weld '%s' to '%s' ?"), *AddBoneName, *SelectedBoneName) );
	UBOOL bDoWeld = true;
	if(bDoWeld)
	{
		// Call 'Modify' on all things that will be affected by the welding..
		GEditor->Trans->Begin( TEXT("Weld Bodies") );

		// .. the asset itself..
		PhysicsAsset->Modify();
		PhysicsAsset->DefaultInstance->Modify();

		// .. the parent and child bodies..
		PhysicsAsset->BodySetup(ParentBodyIndex)->Modify();
		PhysicsAsset->DefaultInstance->Bodies(ParentBodyIndex)->Modify();
		PhysicsAsset->BodySetup(ChildBodyIndex)->Modify();
		PhysicsAsset->DefaultInstance->Bodies(ChildBodyIndex)->Modify();

		// .. and any constraints of the 'child' body..
		TArray<INT>	Constraints;
		PhysicsAsset->BodyFindConstraints(ChildBodyIndex, Constraints);

		for(INT i=0; i<Constraints.Num(); i++)
		{
			INT ConstraintIndex = Constraints(i);
			PhysicsAsset->ConstraintSetup(ConstraintIndex)->Modify();
			PhysicsAsset->DefaultInstance->Constraints(ConstraintIndex)->Modify();
		}

		// Do the actual welding
		PhysicsAsset->WeldBodies(ParentBodyIndex, ChildBodyIndex, EditorSkelComp);

		// End the transaction.
		GEditor->Trans->End();

		// Body index may have changed, so we re-find it.
		INT NewSelectedIndex = PhysicsAsset->FindBodyIndex(ParentBoneName);
		SetSelectedBody(NewSelectedIndex, SelectedPrimitiveType, SelectedPrimitiveIndex); // This redraws the viewport as well...

		// Just to be safe - deselect any selected constraints
		SetSelectedConstraint(INDEX_NONE);
	}
	
}

void WPhAT::MakeNewBodyFromNextSelect()
{
	if(bRunningSimulation || EditingMode != PEM_BodyEdit)
		return;

	NextSelectEvent = PNS_MakeNewBody;
	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::MakeNewBody(INT NewBoneIndex)
{
	FName NewBoneName = EditorSkelMesh->RefSkeleton(NewBoneIndex).Name;

	// If this body is already physical - do nothing.
	if( PhysicsAsset->FindBodyIndex(NewBoneName) != INDEX_NONE )
		return;

	// Check that this bone has no physics children.
	for(INT i=0; i<EditorSkelMesh->RefSkeleton.Num(); i++)
	{
		if( EditorSkelMesh->BoneIsChildOf(i, NewBoneIndex) )
		{
			INT BodyIndex = PhysicsAsset->FindBodyIndex( EditorSkelMesh->RefSkeleton(i).Name );
			if(BodyIndex != INDEX_NONE)
			{
				appMsgf(0, TEXT("You cannot make a new bone physical if it has physical children."));
				return;
			}
		}
	}

	// Find body that currently controls this bone.
	INT ParentBodyIndex = PhysicsAsset->FindControllingBodyIndex(EditorSkelMesh, NewBoneIndex);

	// Create the physics body.
	INT NewBodyIndex = PhysicsAsset->CreateNewBody( NewBoneName );
	URB_BodySetup* bs = PhysicsAsset->BodySetup( NewBodyIndex );
	check(bs->BoneName == NewBoneName);

	WxDlgNewPhysicsAsset dlg;
	if( dlg.ShowModal( FString(TEXT("")), FString(PhysicsAsset->GetName()), NULL, true ) != wxID_OK )
		return;

	// Create a new physics body for this bone.
	if(dlg.Params.VertWeight == EVW_DominantWeight)
		PhysicsAsset->CreateCollisionFromBone(bs, EditorSkelMesh, NewBoneIndex, dlg.Params, DominantWeightBoneInfos);
	else
		PhysicsAsset->CreateCollisionFromBone(bs, EditorSkelMesh, NewBoneIndex, dlg.Params, AnyWeightBoneInfos);

	// If we have a physics parent, create a joint to it.
	if(ParentBodyIndex != INDEX_NONE)
	{
		INT NewConstraintIndex = PhysicsAsset->CreateNewConstraint( NewBoneName );
		URB_ConstraintSetup* cs = PhysicsAsset->ConstraintSetup( NewConstraintIndex );

		FName ParentBoneName = PhysicsAsset->BodySetup(ParentBodyIndex)->BoneName;
		INT ParentBoneIndex = EditorSkelMesh->MatchRefBone(ParentBoneName);
		check(ParentBoneIndex != INDEX_NONE);

		// Transform of child from parent is just child ref-pose entry.
		FMatrix NewBoneTM = EditorSkelComp->GetBoneMatrix(NewBoneIndex);
		NewBoneTM.RemoveScaling();

		FMatrix ParentBoneTM = EditorSkelComp->GetBoneMatrix(ParentBoneIndex);
		ParentBoneTM.RemoveScaling();

		FMatrix RelTM = NewBoneTM * ParentBoneTM.Inverse();

		// Place joint at origin of child
		cs->ConstraintBone1 = NewBoneName;
		cs->Pos1 = FVector(0,0,0);
		cs->PriAxis1 = FVector(1,0,0);
		cs->SecAxis1 = FVector(0,1,0);

		cs->ConstraintBone2 = ParentBoneName;
		cs->Pos2 = RelTM.GetOrigin() * U2PScale;
		cs->PriAxis2 = RelTM.GetAxis(0);
		cs->SecAxis2 = RelTM.GetAxis(1);

		// Disable collision between constrained bodies by default.
		URB_BodyInstance* bodyInstance = PhysicsAsset->DefaultInstance->Bodies(NewBodyIndex);
		URB_BodyInstance* parentInstance = PhysicsAsset->DefaultInstance->Bodies(ParentBodyIndex);

		PhysicsAsset->DefaultInstance->DisableCollision(bodyInstance, parentInstance);
	}

	SetSelectedBodyAnyPrim(NewBodyIndex);

}

void WPhAT::SetAssetPhysicalMaterial()
{
	WxDlgSetAssetPhysMaterial dlg;
	if( dlg.ShowModal() != wxID_OK )
		return;

	check(dlg.PhysMaterialClass);

	for(INT i=0; i<PhysicsAsset->BodySetup.Num(); i++)
	{
		URB_BodySetup* bs = PhysicsAsset->BodySetup(i);
		bs->PhysicalMaterial = dlg.PhysMaterialClass;
	}
}

static void CycleMatrixRows(FMatrix* TM)
{
	FLOAT Tmp[3];

	Tmp[0]		= TM->M[0][0];		Tmp[1]	= TM->M[0][1];		Tmp[2]	= TM->M[0][2];
	TM->M[0][0] = TM->M[1][0];	TM->M[0][1] = TM->M[1][1];	TM->M[0][2] = TM->M[1][2];
	TM->M[1][0] = TM->M[2][0];	TM->M[1][1] = TM->M[2][1];	TM->M[1][2] = TM->M[2][2];
	TM->M[2][0] = Tmp[0];		TM->M[2][1] = Tmp[1];		TM->M[2][2] = Tmp[2];
}

// Keep con frame 1 fixed, and update con frame 0 so supplied rel TM is maintained.
void WPhAT::SetSelectedConstraintRelTM(const FMatrix& RelTM)
{
	FMatrix WParentFrame = GetSelectedConstraintWorldTM(1);
	FMatrix WNewChildFrame = RelTM * WParentFrame;

	URB_ConstraintSetup* cs = PhysicsAsset->ConstraintSetup(SelectedConstraintIndex);

	FVector Scale3D = EditorActor->DrawScale3D * EditorActor->DrawScale;
	check( Scale3D.IsUniform() );
	FVector InvScale3D( 1.0f/Scale3D.X );

	// Get child bone transform
	INT BoneIndex = EditorSkelMesh->MatchRefBone( cs->ConstraintBone1 );
	check(BoneIndex != INDEX_NONE);

	FMatrix BoneTM = EditorSkelComp->GetBoneMatrix(BoneIndex);
	BoneTM.RemoveScaling();
	BoneTM.ScaleTranslation(InvScale3D); // Remove any asset scaling here.

	cs->SetRefFrameMatrix(0, WNewChildFrame * BoneTM.Inverse() );
}

void WPhAT::CycleSelectedConstraintOrientation()
{
	if(EditingMode != PEM_ConstraintEdit || SelectedConstraintIndex == INDEX_NONE)
		return;

	URB_ConstraintSetup* cs = PhysicsAsset->ConstraintSetup(SelectedConstraintIndex);
	FMatrix ConMatrix = cs->GetRefFrameMatrix(1);

	FMatrix WParentFrame = GetSelectedConstraintWorldTM(1);
	FMatrix WChildFrame = GetSelectedConstraintWorldTM(0);
	FMatrix RelTM = WChildFrame * WParentFrame.Inverse();

	CycleMatrixRows(&ConMatrix);
	cs->SetRefFrameMatrix(1, ConMatrix);

	SetSelectedConstraintRelTM(RelTM);

	PhATViewportClient->Viewport->Invalidate();
}

FMatrix WPhAT::GetSelectedConstraintWorldTM(INT BodyIndex)
{
	if(SelectedConstraintIndex == INDEX_NONE)
		return FMatrix::Identity;

	URB_ConstraintSetup* cs = PhysicsAsset->ConstraintSetup(SelectedConstraintIndex);

	FVector Scale3D = EditorActor->DrawScale3D * EditorActor->DrawScale;
	check( Scale3D.IsUniform() );
	FVector InvScale3D( 1.0f/Scale3D.X );

	FMatrix Frame = cs->GetRefFrameMatrix(BodyIndex);

	INT BoneIndex;
	if(BodyIndex == 0)
		BoneIndex = EditorSkelMesh->MatchRefBone( cs->ConstraintBone1 );
	else
		BoneIndex = EditorSkelMesh->MatchRefBone( cs->ConstraintBone2 );
	check(BoneIndex != INDEX_NONE);

	FMatrix BoneTM = EditorSkelComp->GetBoneMatrix(BoneIndex);
	BoneTM.RemoveScaling();
	BoneTM.ScaleTranslation(InvScale3D); // Remove any asset scaling here.

	return Frame * BoneTM;
}

// This leaves the joint ref frames unchanged, but copies limit info/type.
void WPhAT::CopyConstraintProperties(INT ToConstraintIndex, INT FromConstraintIndex)
{
	if(ToConstraintIndex == INDEX_NONE || FromConstraintIndex == INDEX_NONE)
		return;

	URB_ConstraintSetup* tcs = PhysicsAsset->ConstraintSetup(ToConstraintIndex);
	URB_ConstraintSetup* fcs = PhysicsAsset->ConstraintSetup(FromConstraintIndex);

	tcs->CopyConstraintParamsFrom(fcs);

	SetSelectedConstraint(ToConstraintIndex);

	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::Undo()
{
	if(bRunningSimulation)
		return;

	GEditor->Trans->Undo();
	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::Redo()
{
	if(bRunningSimulation)
		return;

	GEditor->Trans->Redo();
	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::ToggleSelectionLock()
{
	if(bRunningSimulation)
		return;

	bSelectionLock = !bSelectionLock;
	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::ToggleSnap()
{
	if(bRunningSimulation)
		return;

	bSnap = !bSnap;

	if(bSnap)
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_SNAP, (LPARAM) MAKELONG(1, 0));
	else
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_SNAP, (LPARAM) MAKELONG(0, 0));

	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::ToggleEditingMode()
{
	if(bRunningSimulation)
		return;

	if(EditingMode == PEM_ConstraintEdit)
	{
		EditingMode = PEM_BodyEdit;
		SetSelectedBody(SelectedBodyIndex, SelectedPrimitiveType, SelectedPrimitiveIndex); // Forces properties panel to update...
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_EDITMODE, (LPARAM) MAKELONG(0, 0));
	}
	else
	{
		EditingMode = PEM_ConstraintEdit;
		SetSelectedConstraint(SelectedConstraintIndex);
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_EDITMODE, (LPARAM) MAKELONG(1, 0));

		// Scale isn't valid for constraints!
		if(MovementMode == PMM_Scale)
			SetMovementMode(PMM_Translate);
	}

	NextSelectEvent = PNS_Normal;
	UpdateToolBarStatus();
	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::SetMovementMode(EPhATMovementMode NewMovementMode)
{
	if(bRunningSimulation)
		return;

	MovementMode = NewMovementMode;

	if(MovementMode == PMM_Rotate)
	{
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_ROTATE, (LPARAM) MAKELONG(1, 0));
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_TRANSLATE, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_SCALE, (LPARAM) MAKELONG(0, 0));
	}
	else if(MovementMode == PMM_Translate)
	{
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_ROTATE, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_TRANSLATE, (LPARAM) MAKELONG(1, 0));
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_SCALE, (LPARAM) MAKELONG(0, 0));
	}
	else if(MovementMode == PMM_Scale)
	{
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_ROTATE, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_TRANSLATE, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_SCALE, (LPARAM) MAKELONG(1, 0));
	}

	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::ToggleMovementSpace()
{
	if(MovementSpace == PMS_Local)
	{
		MovementSpace = PMS_World;
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_MOVESPACE, (LPARAM) MAKELONG(7, 0));

	}
	else
	{
		MovementSpace = PMS_Local;
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_MOVESPACE, (LPARAM) MAKELONG(8, 0));
	}

	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::UpdateToolBarStatus()
{	
	if(bRunningSimulation) // Disable everything.
	{
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ROTATE, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_TRANSLATE, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_MOVESPACE, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_COPYPROPERTIES, (LPARAM) MAKELONG(0, 0));

		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_SCALE, (LPARAM) MAKELONG(0, 0));

		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DISABLECOLLISION, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ENABLECOLLISION, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_WELDBODIES, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDNEWBODY, (LPARAM) MAKELONG(0, 0));

		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDSPHERE, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDSPHYL, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDBOX, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DUPLICATEPRIM, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DELETEPRIM, (LPARAM) MAKELONG(0, 0));

		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_RESETCONFRAME, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDBS, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDHINGE, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDPRISMATIC, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDSKELETAL, (LPARAM) MAKELONG(0, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DELETECONSTRAINT, (LPARAM) MAKELONG(0, 0));
	}
	else // Enable stuff for current editing mode.
	{
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ROTATE, (LPARAM) MAKELONG(1, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_TRANSLATE, (LPARAM) MAKELONG(1, 0));
		SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_MOVESPACE, (LPARAM) MAKELONG(1, 0));

		if(EditingMode == PEM_BodyEdit) //// BODY MODE ////
		{
			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_SCALE, (LPARAM) MAKELONG(1, 0));

			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDNEWBODY, (LPARAM) MAKELONG(1, 0));

			if(SelectedBodyIndex != INDEX_NONE)
			{
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_COPYPROPERTIES, (LPARAM) MAKELONG(1, 0));

				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DISABLECOLLISION, (LPARAM) MAKELONG(1, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ENABLECOLLISION, (LPARAM) MAKELONG(1, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_WELDBODIES, (LPARAM) MAKELONG(1, 0));

				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDSPHERE, (LPARAM) MAKELONG(1, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDSPHYL, (LPARAM) MAKELONG(1, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDBOX, (LPARAM) MAKELONG(1, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DUPLICATEPRIM, (LPARAM) MAKELONG(1, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DELETEPRIM, (LPARAM) MAKELONG(1, 0));
			}
			else
			{
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_COPYPROPERTIES, (LPARAM) MAKELONG(0, 0));

				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DISABLECOLLISION, (LPARAM) MAKELONG(0, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ENABLECOLLISION, (LPARAM) MAKELONG(0, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_WELDBODIES, (LPARAM) MAKELONG(0, 0));

				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDSPHERE, (LPARAM) MAKELONG(0, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDSPHYL, (LPARAM) MAKELONG(0, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDBOX, (LPARAM) MAKELONG(0, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DUPLICATEPRIM, (LPARAM) MAKELONG(0, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DELETEPRIM, (LPARAM) MAKELONG(0, 0));
			}

			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_RESETCONFRAME, (LPARAM) MAKELONG(0, 0));
			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDBS, (LPARAM) MAKELONG(0, 0));
			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDHINGE, (LPARAM) MAKELONG(0, 0));
			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDPRISMATIC, (LPARAM) MAKELONG(0, 0));
			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDSKELETAL, (LPARAM) MAKELONG(0, 0));
			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DELETECONSTRAINT, (LPARAM) MAKELONG(0, 0));
		}
		else //// CONSTRAINT MODE ////
		{
			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_SCALE, (LPARAM) MAKELONG(0, 0));

			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DISABLECOLLISION, (LPARAM) MAKELONG(0, 0));
			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ENABLECOLLISION, (LPARAM) MAKELONG(0, 0));
			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_WELDBODIES, (LPARAM) MAKELONG(0, 0));
			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDNEWBODY, (LPARAM) MAKELONG(0, 0));

			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDSPHERE, (LPARAM) MAKELONG(0, 0));
			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDSPHYL, (LPARAM) MAKELONG(0, 0));
			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDBOX, (LPARAM) MAKELONG(0, 0));
			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DUPLICATEPRIM, (LPARAM) MAKELONG(0, 0));
			SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DELETEPRIM, (LPARAM) MAKELONG(0, 0));	

			if(SelectedConstraintIndex != INDEX_NONE)
			{
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_COPYPROPERTIES, (LPARAM) MAKELONG(1, 0));

				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_RESETCONFRAME, (LPARAM) MAKELONG(1, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDBS, (LPARAM) MAKELONG(1, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDHINGE, (LPARAM) MAKELONG(1, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDPRISMATIC, (LPARAM) MAKELONG(1, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDSKELETAL, (LPARAM) MAKELONG(1, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DELETECONSTRAINT, (LPARAM) MAKELONG(1, 0));
			}
			else
			{
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_COPYPROPERTIES, (LPARAM) MAKELONG(0, 0));

				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_RESETCONFRAME, (LPARAM) MAKELONG(0, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDBS, (LPARAM) MAKELONG(0, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDHINGE, (LPARAM) MAKELONG(0, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDPRISMATIC, (LPARAM) MAKELONG(0, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_ADDSKELETAL, (LPARAM) MAKELONG(0, 0));
				SendMessageX( hWndToolBar, TB_ENABLEBUTTON,  IDMN_PHAT_DELETECONSTRAINT, (LPARAM) MAKELONG(0, 0));
			}
		}
	}

	// Update view mode icons.
	EPhATRenderMode MeshViewMode = GetCurrentMeshViewMode();
	if(MeshViewMode == PRM_None)
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_MESHVIEWMODE, (LPARAM) MAKELONG(19, 0));
	else if(MeshViewMode == PRM_Wireframe)
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_MESHVIEWMODE, (LPARAM) MAKELONG(20, 0));
	else if(MeshViewMode == PRM_Solid)
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_MESHVIEWMODE, (LPARAM) MAKELONG(21, 0));

	EPhATRenderMode CollisionViewMode = GetCurrentCollisionViewMode();
	if(CollisionViewMode == PRM_None)
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_COLLISIONVIEWMODE, (LPARAM) MAKELONG(16, 0));
	else if(CollisionViewMode == PRM_Wireframe)
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_COLLISIONVIEWMODE, (LPARAM) MAKELONG(17, 0));
	else if(CollisionViewMode == PRM_Solid)
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_COLLISIONVIEWMODE, (LPARAM) MAKELONG(18, 0));

	EPhATConstraintViewMode ConstraintViewMode = GetCurrentConstraintViewMode();
	if(ConstraintViewMode == PCV_None)
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_CONSTRAINTVIEWMODE, (LPARAM) MAKELONG(28, 0));
	else if(ConstraintViewMode == PCV_AllPositions)
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_CONSTRAINTVIEWMODE, (LPARAM) MAKELONG(29, 0));
	else if(ConstraintViewMode == PCV_AllLimits)
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_CONSTRAINTVIEWMODE, (LPARAM) MAKELONG(30, 0));

}	



void WPhAT::StartManipulating(EAxis Axis, const FViewportClick& Click, const FMatrix& WorldToCamera)
{
	check(!bManipulating);

	//debugf(TEXT("START: %d"), (INT)Axis);

	ManipulateAxis = Axis;

	if(EditingMode == PEM_BodyEdit)
	{
		check(SelectedBodyIndex != INDEX_NONE);
		GEditor->Trans->Begin( TEXT("Move Element") );
		PhysicsAsset->BodySetup(SelectedBodyIndex)->Modify();
	}
	else
	{
		check(SelectedConstraintIndex != INDEX_NONE);
		GEditor->Trans->Begin( TEXT("Move Constraint") );
		PhysicsAsset->ConstraintSetup(SelectedConstraintIndex)->Modify();

		FMatrix WParentFrame = GetSelectedConstraintWorldTM(1);
		FMatrix WChildFrame = GetSelectedConstraintWorldTM(0);
		StartManRelConTM = WChildFrame * WParentFrame.Inverse();

		StartManParentConTM = PhysicsAsset->ConstraintSetup(SelectedConstraintIndex)->GetRefFrameMatrix(1);
		StartManChildConTM = PhysicsAsset->ConstraintSetup(SelectedConstraintIndex)->GetRefFrameMatrix(0);
	}

	FVector Scale3D = EditorActor->DrawScale3D * EditorActor->DrawScale;

	FMatrix InvWidgetTM = WidgetTM.Inverse(); // WidgetTM is set inside WPhAT::DrawCurrentWidget.

	// First - get the manipulation axis in world space.
	FVector WorldManDir;
	if(MovementSpace == PMS_World)
	{
		if(ManipulateAxis == AXIS_X)
			WorldManDir = FVector(1,0,0);
		else if(ManipulateAxis == AXIS_Y)
			WorldManDir = FVector(0,1,0);
		else
			WorldManDir = FVector(0,0,1);
	}
	else
	{
		if(ManipulateAxis == AXIS_X)
			WorldManDir = WidgetTM.GetAxis(0);
		else if(ManipulateAxis == AXIS_Y)
			WorldManDir = WidgetTM.GetAxis(1);
		else
			WorldManDir = WidgetTM.GetAxis(2);
	}

	// Then transform to get the ELEMENT SPACE direction vector that we are manipulating
	ManipulateDir = InvWidgetTM.TransformNormal(WorldManDir);

	// Then work out the screen-space vector that you need to drag along.
	FVector WorldDragDir(0,0,0);

	if(MovementMode == PMM_Rotate)
	{
		if( Abs(Click.Direction | WorldManDir) > KINDA_SMALL_NUMBER ) // If click direction and circle plane are parallel.. can't resolve.
		{
			// First, find actual position we clicking on the circle in world space.
			FVector ClickPosition = FLinePlaneIntersection( Click.Origin, Click.Origin + Click.Direction, WidgetTM.GetOrigin(), WorldManDir );

			// Then find Radial direction vector (from center to widget to clicked position).
			FVector RadialDir = ( ClickPosition - WidgetTM.GetOrigin() );
			RadialDir.Normalize();

			// Then tangent in plane is just the cross product. Should always be unit length again because RadialDir and WorlManDir should be orthogonal.
			WorldDragDir = RadialDir ^ WorldManDir;
		}
	}
	else
	{
		// Easy - drag direction is just the world-space manipulation direction.
		WorldDragDir = WorldManDir;
	}

	// Transform world-space drag dir to screen space.
	FVector ScreenDir = WorldToCamera.TransformNormal(WorldDragDir);
	ScreenDir.Z = 0.0f;

	if( ScreenDir.IsZero() )
	{
		DragDirX = 0.0f;
		DragDirY = 0.0f;
	}
	else
	{
		ScreenDir.Normalize();
		DragDirX = ScreenDir.X;
		DragDirY = ScreenDir.Y;
	}

	ManipulateMatrix = FMatrix::Identity;
	ManipulateTranslation = 0.f;
	ManipulateRotation = 0.f;
	DragX = 0.0f;
	DragY = 0.0f;
	CurrentScale = 0.0f;
	bManipulating = true;

	PhATViewportClient->Viewport->Invalidate();
}

void WPhAT::UpdateManipulation(FLOAT DeltaX, FLOAT DeltaY, UBOOL bCtrlDown)
{
	// DragX/Y is total offset from start of drag.
	DragX += DeltaX;
	DragY += DeltaY;

	//debugf(TEXT("UPDATE: %f %f"), DragX, DragY);

	// Update ManipulateMatrix using DragX, DragY, MovementMode & MovementSpace

	FLOAT DragMag = (DragX * DragDirX) + (DragY * DragDirY);

	if(MovementMode == PMM_Translate)
	{
		ManipulateTranslation = TranslateSpeed * DragMag;

		if(bSnap)
			ManipulateTranslation = EditorSimOptions->LinearSnap * appFloor(ManipulateTranslation/EditorSimOptions->LinearSnap);
		else
			ManipulateTranslation = TranslateSpeed * appFloor(ManipulateTranslation/TranslateSpeed);

		ManipulateMatrix = FTranslationMatrix(ManipulateDir * ManipulateTranslation);
	}
	else if(MovementMode == PMM_Rotate)
	{
		ManipulateRotation = -RotateSpeed * DragMag;

		if(bSnap)
		{
			FLOAT AngSnapRadians = EditorSimOptions->AngularSnap * (PI/180.f);
			ManipulateRotation = AngSnapRadians * appFloor(ManipulateRotation/AngSnapRadians);
		}
		else
			ManipulateRotation = RotateSpeed * appFloor(ManipulateRotation/RotateSpeed);

		FQuat RotQuat(ManipulateDir, ManipulateRotation);
		ManipulateMatrix = FMatrix(FVector(0.f), RotQuat);
	}
	else if(MovementMode == PMM_Scale && EditingMode == PEM_BodyEdit) // Scaling only valid for bodies.
	{
		FLOAT DeltaMag = ((DeltaX * DragDirX) + (DeltaY * DragDirY)) * TranslateSpeed;
		CurrentScale += DeltaMag;

		FLOAT ApplyScale;
		if(bSnap)
		{
			ApplyScale = 0.0f;

			while(CurrentScale > EditorSimOptions->LinearSnap)
			{
				ApplyScale += EditorSimOptions->LinearSnap;
				CurrentScale -= EditorSimOptions->LinearSnap;
			}

			while(CurrentScale < -EditorSimOptions->LinearSnap)
			{
				ApplyScale -= EditorSimOptions->LinearSnap;
				CurrentScale += EditorSimOptions->LinearSnap;
			}
		}
		else
		{
			ApplyScale = DeltaMag;
		}


		FVector DeltaSize;
		if(bCtrlDown)
		{
			DeltaSize = FVector(ApplyScale);
		}
		else
		{
			if(ManipulateAxis == AXIS_X)
				DeltaSize = FVector(ApplyScale, 0, 0);
			else if(ManipulateAxis == AXIS_Y)
				DeltaSize = FVector(0, ApplyScale, 0);
			else if(ManipulateAxis == AXIS_Z)
				DeltaSize = FVector(0, 0, ApplyScale);
		}

		ModifyPrimitiveSize(SelectedBodyIndex, SelectedPrimitiveType, SelectedPrimitiveIndex, DeltaSize);
	}

	if(EditingMode == PEM_ConstraintEdit)
	{
		URB_ConstraintSetup* cs = PhysicsAsset->ConstraintSetup(SelectedConstraintIndex);

		cs->SetRefFrameMatrix(1, ManipulateMatrix * StartManParentConTM);

		if(bCtrlDown)
			SetSelectedConstraintRelTM( StartManRelConTM );
		else
			cs->SetRefFrameMatrix(0, StartManChildConTM);
	}
}

void WPhAT::EndManipulating()
{
	if(bManipulating)
	{
		//debugf(TEXT("END"));

		bManipulating = false;

		if(EditingMode == PEM_BodyEdit)
		{
			// Collapse ManipulateMatrix into the current element transform.
			check(SelectedBodyIndex != INDEX_NONE);

			FKAggregateGeom* AggGeom = &PhysicsAsset->BodySetup(SelectedBodyIndex)->AggGeom;

			if(SelectedPrimitiveType == KPT_Sphere)
				AggGeom->SphereElems(SelectedPrimitiveIndex).TM = ManipulateMatrix * AggGeom->SphereElems(SelectedPrimitiveIndex).TM;
			else if(SelectedPrimitiveType == KPT_Box)
				AggGeom->BoxElems(SelectedPrimitiveIndex).TM = ManipulateMatrix * AggGeom->BoxElems(SelectedPrimitiveIndex).TM;
			else if(SelectedPrimitiveType == KPT_Sphyl)
				AggGeom->SphylElems(SelectedPrimitiveIndex).TM = ManipulateMatrix * AggGeom->SphylElems(SelectedPrimitiveIndex).TM;
		}

		GEditor->Trans->End();

		PhATViewportClient->Viewport->Invalidate();
	}
}

// Mouse forces

void WPhAT::SimMousePress(FChildViewport* Viewport, UBOOL bConstrainRotation)
{
	FSceneView	View;
	PhATViewportClient->CalcSceneView(&View);

	FViewportClick Click( &View, PhATViewportClient, NAME_None, IE_Released, Viewport->GetMouseX(), Viewport->GetMouseY() );

	FCheckResult Result(1.f);
	UBOOL bHit = !PhysicsAsset->LineCheck( Result, EditorSkelComp, Click.Origin,Click.Origin + Click.Direction * SimGrabCheckDistance, FVector(0) );

	if(bHit)
	{
		bManipulating = true;
		DragX = 0.0f;
		DragY = 0.0f;
		SimGrabPush = 0.0f;

		check(Result.Item != INDEX_NONE);
		FName BoneName = PhysicsAsset->BodySetup(Result.Item)->BoneName;
		URB_Handle* Handle = EditorActor->MouseHandle;

		// Update mouse force properties from sim options.
		Handle->LinearDamping = EditorSimOptions->MouseLinearDamping;
		Handle->LinearStiffness = EditorSimOptions->MouseLinearStiffness;
		Handle->LinearMaxDistance = EditorSimOptions->MouseLinearMaxDistance;
		Handle->AngularDamping = EditorSimOptions->MouseAngularDamping;
		Handle->AngularStiffness = EditorSimOptions->MouseAngularStiffness;

		// Create handle to object.
		Handle->GrabComponent(EditorSkelComp, BoneName, Result.Location, bConstrainRotation);
		
		FMatrix	InvViewMatrix = View.ViewMatrix.Inverse();

		SimGrabMinPush = SimMinHoldDistance - (Result.Time * SimGrabCheckDistance);

		SimGrabLocation = Result.Location;
		SimGrabX = InvViewMatrix.GetAxis(0).SafeNormal();
		SimGrabY = InvViewMatrix.GetAxis(1).SafeNormal();
		SimGrabZ = InvViewMatrix.GetAxis(2).SafeNormal();
	}
}

void WPhAT::SimMouseMove(FLOAT DeltaX, FLOAT DeltaY)
{
	DragX += DeltaX;
	DragY += DeltaY;

	URB_Handle* Handle = EditorActor->MouseHandle;

	if(!Handle->GrabbedComponent)
		return;

	check(Handle->GrabbedComponent->Owner == EditorActor);

	FVector NewLocation = SimGrabLocation + (SimGrabPush * SimGrabZ) + (DragX * SimGrabMoveSpeed * SimGrabX) + (DragY * SimGrabMoveSpeed * SimGrabY);

	Handle->SetLocation(NewLocation);
	Handle->GrabbedComponent->WakeRigidBody(Handle->GrabbedBoneName);
}

void WPhAT::SimMouseRelease()
{
	bManipulating = false;

	URB_Handle* Handle = EditorActor->MouseHandle;

	if(!Handle->GrabbedComponent)
		return;

	check(Handle->GrabbedComponent->Owner == EditorActor);

	Handle->GrabbedComponent->WakeRigidBody(Handle->GrabbedBoneName);
	Handle->ReleaseComponent();
}

void WPhAT::SimMouseWheelUp()
{
	URB_Handle* Handle = EditorActor->MouseHandle;
	if(!Handle->GrabbedComponent)
		return;

	SimGrabPush += SimHoldDistanceChangeDelta;

	SimMouseMove(0.0f, 0.0f);
}

void WPhAT::SimMouseWheelDown()
{
	URB_Handle* Handle = EditorActor->MouseHandle;
	if(!Handle->GrabbedComponent)
		return;

	SimGrabPush -= SimHoldDistanceChangeDelta;
	SimGrabPush = Max(SimGrabMinPush, SimGrabPush); 

	SimMouseMove(0.0f, 0.0f);
}

// Simulation

void WPhAT::ToggleSimulation()
{
	bRunningSimulation = !bRunningSimulation;

	if(bRunningSimulation)
	{
		// START RUNNING SIM

		NextSelectEvent = PNS_Normal;

		// Disable all the editing buttons
		UpdateToolBarStatus();

		// Change the button icon to a stop icon
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_RUNSIM, (LPARAM) MAKELONG(4, 0));

		// Flush geometry cache inside the asset (dont want to use cached version of old geometry!)
		PhysicsAsset->ClearShapeCaches();

		// Start up the physics asset for the actor.
		check(EditorSkelComp == EditorActor->CollisionComponent);
		EditorSkelComp->InitArticulated();
		EditorActor->setPhysics(PHYS_Articulated);
		EditorSkelComp->WakeRigidBody();

		// Set the properties window to point at the simulation options object.
		PropertyWindow->Root.SetObjects((UObject**)&EditorSimOptions, 1);

		PhATViewportClient->Realtime = 1;
	}
	else
	{
		// STOP RUNNING SIM

		PhATViewportClient->Realtime = 0;

		// Turn off/remove the physics instance for this thing, and move back to start location.
		EditorActor->setPhysics(PHYS_None);
		EditorSkelComp->TermArticulated();


		FCheckResult Hit(0);
		EditorLevel->FarMoveActor(EditorActor, EditorActorStartLocation, 0, 0, 0);
		EditorLevel->MoveActor(EditorActor, FVector(0.f), FRotator(0,0,0), Hit);

		// Force an update of the skeletal mesh to get it back to ref pose
		EditorSkelComp->UpdateSpaceBases();
		EditorSkelComp->Update();
		PhATViewportClient->Viewport->Invalidate();

		// Enable all the buttons again
		UpdateToolBarStatus();

		// Change the button icon to a play icon again
		SendMessageX( hWndToolBar, TB_CHANGEBITMAP,  IDMN_PHAT_RUNSIM, (LPARAM) MAKELONG(3, 0));

		// Put properties window back to selected.
		if(EditingMode == PEM_BodyEdit)
			SetSelectedBody(SelectedBodyIndex, SelectedPrimitiveType, SelectedPrimitiveIndex);
		else
			SetSelectedConstraint(SelectedConstraintIndex);
	}
}

void WPhAT::TickSimulation(FLOAT DeltaSeconds)
{
	check(bRunningSimulation);

	FLOAT TimeScale = Clamp<FLOAT>(EditorSimOptions->SimSpeed, 0.1f, 2.0f);
	EditorLevel->Tick(LEVELTICK_All, TimeScale * DeltaSeconds);
}

// Create/Delete Primitives

void WPhAT::AddNewPrimitive(EKCollisionPrimitiveType InPrimitiveType, UBOOL bCopySelected)
{
	if(SelectedBodyIndex == INDEX_NONE)
		return;

	URB_BodySetup* bs = PhysicsAsset->BodySetup(SelectedBodyIndex);

	GEditor->Trans->Begin( TEXT("Add New Primitive") );
	bs->Modify();

	EKCollisionPrimitiveType PrimitiveType;
	if(bCopySelected)
		PrimitiveType = SelectedPrimitiveType;
	else
		PrimitiveType = InPrimitiveType;


	INT NewPrimIndex = 0;
	if(PrimitiveType == KPT_Sphere)
	{
		NewPrimIndex = bs->AggGeom.SphereElems.AddZeroed();
		FKSphereElem* se = &bs->AggGeom.SphereElems(NewPrimIndex);

		if(!bCopySelected)
		{
			se->TM = FMatrix::Identity;

			se->Radius = DefaultPrimSize;
		}
		else
		{
			se->TM = bs->AggGeom.SphereElems(SelectedPrimitiveIndex).TM;
			se->TM.M[3][0] += DuplicateXOffset;

			se->Radius = bs->AggGeom.SphereElems(SelectedPrimitiveIndex).Radius;
		}
	}
	else if(PrimitiveType == KPT_Box)
	{
		NewPrimIndex = bs->AggGeom.BoxElems.AddZeroed();
		FKBoxElem* be = &bs->AggGeom.BoxElems(NewPrimIndex);

		if(!bCopySelected)
		{
			be->TM = FMatrix::Identity;

			be->X = 0.5f * DefaultPrimSize;
			be->Y = 0.5f * DefaultPrimSize;
			be->Z = 0.5f * DefaultPrimSize;
		}
		else
		{
			be->TM = bs->AggGeom.BoxElems(SelectedPrimitiveIndex).TM;
			be->TM.M[3][0] += DuplicateXOffset;

			be->X = bs->AggGeom.BoxElems(SelectedPrimitiveIndex).X;
			be->Y = bs->AggGeom.BoxElems(SelectedPrimitiveIndex).Y;
			be->Z = bs->AggGeom.BoxElems(SelectedPrimitiveIndex).Z;
		}
	}
	else if(PrimitiveType == KPT_Sphyl)
	{
		NewPrimIndex = bs->AggGeom.SphylElems.AddZeroed();
		FKSphylElem* se = &bs->AggGeom.SphylElems(NewPrimIndex);

		if(!bCopySelected)
		{
			se->TM = FMatrix::Identity;

			se->Length = DefaultPrimSize;
			se->Radius = DefaultPrimSize;
		}
		else
		{
			se->TM = bs->AggGeom.SphylElems(SelectedPrimitiveIndex).TM;
			se->TM.M[3][0] += DuplicateXOffset;

			se->Length = bs->AggGeom.SphylElems(SelectedPrimitiveIndex).Length;
			se->Radius = bs->AggGeom.SphylElems(SelectedPrimitiveIndex).Radius;
		}
	}

	GEditor->Trans->End();

	// Select the new primitive. Will call UpdateViewport.
	SetSelectedBody(SelectedBodyIndex, PrimitiveType, NewPrimIndex);


	SelectedPrimitiveType = PrimitiveType;
	SelectedPrimitiveIndex = NewPrimIndex;

	PhATViewportClient->Viewport->Invalidate();
}

void WPhAT::DeleteCurrentPrim()
{
	if(bRunningSimulation)
		return;

	if(SelectedBodyIndex == INDEX_NONE)
		return;

	URB_BodySetup* bs = PhysicsAsset->BodySetup(SelectedBodyIndex);

	// If this bone has no more geometry - remove it totally.
	if( bs->AggGeom.GetElementCount() == 1 )
	{
		UBOOL bDoDelete = true;
		
		if(EditorSimOptions->bPromptOnBoneDelete)
			bDoDelete = appMsgf(1, TEXT("This will completely remove this bone, and any constraints to it.\nAre you sure you want to do this?"));

		if(!bDoDelete)
			return;

		// We are about the delete an entire body, so we start the transaction and call Modify of affected parts.
		GEditor->Trans->Begin( TEXT("Delete Body") );

		// The physics asset and default instance..
		PhysicsAsset->Modify();
		PhysicsAsset->DefaultInstance->Modify();

		// .. the body..
		PhysicsAsset->BodySetup(SelectedBodyIndex)->Modify();
		PhysicsAsset->DefaultInstance->Bodies(SelectedBodyIndex)->Modify();		

		// .. and any constraints to the body.
		TArray<INT>	Constraints;
		PhysicsAsset->BodyFindConstraints(SelectedBodyIndex, Constraints);

		for(INT i=0; i<Constraints.Num(); i++)
		{
			INT ConstraintIndex = Constraints(i);
			PhysicsAsset->ConstraintSetup(ConstraintIndex)->Modify();
			PhysicsAsset->DefaultInstance->Constraints(ConstraintIndex)->Modify();
		}

		// Now actually destroy body. This will destroy any constraints associated with the body as well.
		PhysicsAsset->DestroyBody(SelectedBodyIndex);

		GEditor->Trans->End();

		// Select nothing.
		SetSelectedBody(INDEX_NONE, KPT_Unknown, INDEX_NONE);

		return;
	}

	GEditor->Trans->Begin( TEXT("Delete Primitive") );
	bs->Modify();

	if(SelectedPrimitiveType == KPT_Sphere)
		bs->AggGeom.SphereElems.Remove(SelectedPrimitiveIndex);
	else if(SelectedPrimitiveType == KPT_Box)
		bs->AggGeom.BoxElems.Remove(SelectedPrimitiveIndex);
	else if(SelectedPrimitiveType == KPT_Sphyl)
		bs->AggGeom.SphylElems.Remove(SelectedPrimitiveIndex);
	else if(SelectedPrimitiveType == KPT_Convex)
		bs->AggGeom.ConvexElems.Remove(SelectedPrimitiveIndex);

	GEditor->Trans->End();

	SetSelectedBodyAnyPrim(SelectedBodyIndex); // Will call UpdateViewport
}

void WPhAT::ModifyPrimitiveSize(INT BodyIndex, EKCollisionPrimitiveType PrimType, INT PrimIndex, FVector DeltaSize)
{
	check(SelectedBodyIndex != INDEX_NONE);

	FKAggregateGeom* AggGeom = &PhysicsAsset->BodySetup(SelectedBodyIndex)->AggGeom;

	if(PrimType == KPT_Sphere)
	{
		// Find element with largest magnitude, btu preserve sign.
		FLOAT DeltaRadius = DeltaSize.X;
		if( Abs(DeltaSize.Y) > Abs(DeltaRadius) )
			DeltaRadius = DeltaSize.Y;
		else if( Abs(DeltaSize.Z) > Abs(DeltaRadius) )
			DeltaRadius = DeltaSize.Z;

		AggGeom->SphereElems(PrimIndex).Radius = Max( AggGeom->SphereElems(PrimIndex).Radius + DeltaRadius, MinPrimSize );
	}
	else if(PrimType == KPT_Box)
	{
		// Sizes are lengths, so we double the delta to get similar increase in size.
		AggGeom->BoxElems(PrimIndex).X = Max( AggGeom->BoxElems(PrimIndex).X + 2*DeltaSize.X, MinPrimSize );
		AggGeom->BoxElems(PrimIndex).Y = Max( AggGeom->BoxElems(PrimIndex).Y + 2*DeltaSize.Y, MinPrimSize );
		AggGeom->BoxElems(PrimIndex).Z = Max( AggGeom->BoxElems(PrimIndex).Z + 2*DeltaSize.Z, MinPrimSize );

	}
	else if(PrimType == KPT_Sphyl)
	{
		FLOAT DeltaRadius = DeltaSize.X;
		if( Abs(DeltaSize.Y) > Abs(DeltaRadius) )
			DeltaRadius = DeltaSize.Y;

		FLOAT DeltaHeight = DeltaSize.Z;

		AggGeom->SphylElems(PrimIndex).Radius = Max( AggGeom->SphylElems(PrimIndex).Radius + DeltaRadius, MinPrimSize );
		AggGeom->SphylElems(PrimIndex).Length = Max( AggGeom->SphylElems(PrimIndex).Length + 2*DeltaHeight, MinPrimSize );
	}
	else if(PrimType == KPT_Convex)
	{
		// Figure out a scaling factor...
	}
}

// BoneTM is assumed to have NO SCALING in it!
FMatrix WPhAT::GetPrimitiveMatrix(FMatrix& BoneTM, INT BodyIndex, EKCollisionPrimitiveType PrimType, INT PrimIndex, FLOAT Scale)
{
	URB_BodySetup* bs = PhysicsAsset->BodySetup(BodyIndex);
	FVector Scale3D(Scale);

	FMatrix ManTM = FMatrix::Identity;
	if( bManipulating && 
		!bRunningSimulation &&
		(EditingMode == PEM_BodyEdit) && 
		(BodyIndex == SelectedBodyIndex) && 
		(PrimType == SelectedPrimitiveType) && 
		(PrimIndex == SelectedPrimitiveIndex) )
		ManTM = ManipulateMatrix;

	if(PrimType == KPT_Sphere)
	{
		FMatrix PrimTM = ManTM * bs->AggGeom.SphereElems(PrimIndex).TM;
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if(PrimType == KPT_Box)
	{
		FMatrix PrimTM = ManTM * bs->AggGeom.BoxElems(PrimIndex).TM;
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if(PrimType == KPT_Sphyl)
	{
		FMatrix PrimTM = ManTM * bs->AggGeom.SphylElems(PrimIndex).TM;
		PrimTM.ScaleTranslation(Scale3D);
		return PrimTM * BoneTM;
	}
	else if(PrimType == KPT_Convex)
	{
		return ManTM * BoneTM;
	}

	// Should never reach here
	check(0);
	return FMatrix::Identity;
}

FMatrix WPhAT::GetConstraintMatrix(INT ConstraintIndex, INT BodyIndex, FLOAT Scale)
{
	check(BodyIndex == 0 || BodyIndex == 1);

	URB_ConstraintSetup* cs = PhysicsAsset->ConstraintSetup(ConstraintIndex);
	FVector Scale3D(Scale);

	INT BoneIndex;
	FMatrix LFrame;
	if(BodyIndex == 0)
	{
		BoneIndex = EditorSkelMesh->MatchRefBone( cs->ConstraintBone1 );
		LFrame = cs->GetRefFrameMatrix(0);
	}
	else
	{
		BoneIndex = EditorSkelMesh->MatchRefBone( cs->ConstraintBone2 );
		LFrame = cs->GetRefFrameMatrix(1);
	}

	check(BoneIndex != INDEX_NONE);

	FMatrix BoneTM = EditorSkelComp->GetBoneMatrix(BoneIndex);
	BoneTM.RemoveScaling();

	LFrame.ScaleTranslation(Scale3D);

	return LFrame * BoneTM;
}

void WPhAT::CreateOrConvertConstraint(URB_ConstraintSetup* NewSetup)
{
	if(EditingMode != PEM_ConstraintEdit)
		return;

	if(SelectedConstraintIndex != INDEX_NONE)
	{
		// TODO- warning dialog box "About to over-write settings"

		URB_ConstraintSetup* cs = PhysicsAsset->ConstraintSetup( SelectedConstraintIndex );
		cs->CopyConstraintParamsFrom( NewSetup );
	}
	else
	{
		// TODO - making brand new constraints...
	}

	SetSelectedConstraint(SelectedConstraintIndex); // Push new properties into property window.
	PhATViewportClient->Viewport->Invalidate();
}

void WPhAT::DeleteCurrentConstraint()
{
	if(EditingMode != PEM_ConstraintEdit || SelectedConstraintIndex == INDEX_NONE)
		return;

	PhysicsAsset->DestroyConstraint(SelectedConstraintIndex);
	SetSelectedConstraint(INDEX_NONE);

	PhATViewportClient->Viewport->Invalidate();
}