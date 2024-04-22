/*=============================================================================
	PhATTool.cpp: Physics Asset Tool general tools/modal stuff
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "PhAT.h"
#include "EnginePhysicsClasses.h"
#include "EngineAnimClasses.h"
#include "ScopedTransaction.h"
#include "PropertyWindow.h"
#include "..\..\Launch\Resources\resource.h"

static const FLOAT	PhAT_TranslateSpeed = 0.25f;
static const FLOAT  PhAT_RotateSpeed = static_cast<FLOAT>( 1.0*(PI/180.0) );

static const FLOAT	DefaultPrimSize = 15.0f;
static const FLOAT	MinPrimSize = 0.5f;

static const FLOAT	SimGrabCheckDistance = 5000.0f;
static const FLOAT	SimHoldDistanceChangeDelta = 20.0f;
static const FLOAT	SimMinHoldDistance = 10.0f;
static const FLOAT  SimGrabMoveSpeed = 1.0f;

static const FLOAT	DuplicateXOffset = 10.0f;

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// WPhAT //////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void WxPhAT::ChangeDefaultSkelMesh()
{
	// Don't let them do this while running sim!
	if(bRunningSimulation)
	{
		return;
	}

	// Get the currently selected SkeletalMesh. Fail if there ain't one.
	GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);

	USkeletalMesh* NewSkelMesh = GEditor->GetSelectedObjects()->GetTop<USkeletalMesh>();
	if(!NewSkelMesh)
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("NoSkelMeshSelected") );
		return;
	}

	// Confirm they want to do this.
	UBOOL bDoChange = appMsgf( AMT_YesNo, LocalizeSecure(LocalizeUnrealEd("SureChangeAssetSkelMesh"), *PhysicsAsset->GetName(), *NewSkelMesh->GetName()) );
	if(bDoChange)
	{
		// See if any bones are missing from the skeletal mesh we are trying to use
		// @todo Could do more here - check for bone lengths etc. Maybe modify asset?
		for(INT i=0; i<PhysicsAsset->BodySetup.Num(); i++)
		{
			FName BodyName = PhysicsAsset->BodySetup(i)->BoneName;
			INT BoneIndex = NewSkelMesh->MatchRefBone(BodyName);
			if(BoneIndex == INDEX_NONE)
			{
				appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("BoneMissingFromSkelMesh"), *BodyName.ToString()) );
				return;
			}
		}

		// We have all the bones - go ahead and make the change.
		PhysicsAsset->DefaultSkelMesh = NewSkelMesh;

		// Change preview
		EditorSkelMesh = NewSkelMesh;
		EditorSkelComp->SetSkeletalMesh(NewSkelMesh);

		// Update various infos based on the mesh
		EditorSkelMesh->CalcBoneVertInfos(DominantWeightBoneInfos, true);
		EditorSkelMesh->CalcBoneVertInfos(AnyWeightBoneInfos, false);
		FillTree();

		// Mark asset's package as dirty as its changed.
		PhysicsAsset->MarkPackageDirty();
	}
}

void WxPhAT::RecalcEntireAsset()
{
	if(bRunningSimulation)
	{
		return;
	}

	UBOOL bDoRecalc = appMsgf(AMT_YesNo, *LocalizeUnrealEd("Prompt_12"));
	if(!bDoRecalc)
		return;

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	// Then calculate a new one.

	WxDlgNewPhysicsAsset dlg;
	if( dlg.ShowModal( NULL, true ) == wxID_OK )
	{
		// Deselect everything.
		SetSelectedBody(INDEX_NONE, KPT_Unknown, INDEX_NONE);
		SetSelectedConstraint(INDEX_NONE);	

		// Empty current asset data.
		PhysicsAsset->BodySetup.Empty();
		PhysicsAsset->BodySetupIndexMap.Empty();
		PhysicsAsset->ConstraintSetup.Empty();
		PhysicsAsset->DefaultInstance->Bodies.Empty();
		PhysicsAsset->DefaultInstance->Constraints.Empty();

		PhysicsAsset->CreateFromSkeletalMesh(EditorSkelMesh, dlg.Params);

		FillTree();
		PhATViewportClient->Viewport->Invalidate();
	}

}

void WxPhAT::ResetBoneCollision(INT BodyIndex)
{
	if(bRunningSimulation || BodyIndex == INDEX_NONE)
		return;

	UBOOL bDoRecalc = appMsgf(AMT_YesNo, *LocalizeUnrealEd("Prompt_13"));
	if(!bDoRecalc)
		return;

	WxDlgNewPhysicsAsset dlg;
	if( dlg.ShowModal( NULL, true ) != wxID_OK )
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

void WxPhAT::SetSelectedBodyAnyPrim(INT BodyIndex)
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
		appErrorf(*LocalizeUnrealEd("Error_BodySetupWithNoPrimitives")); 

}


void WxPhAT::SetSelectedBody(INT BodyIndex, EKCollisionPrimitiveType PrimitiveType, INT PrimitiveIndex)
{
	SelectedBodyIndex = BodyIndex;
	SelectedPrimitiveType = PrimitiveType;
	SelectedPrimitiveIndex = PrimitiveIndex;	

	if(SelectedBodyIndex == INDEX_NONE)
	{
		// No bone selected
		//PropertyWindow->SetObject(PhysicsAsset, true, false, true);
		PropertyWindow->SetObject(EditorSimOptions, EPropertyWindowFlags::ShouldShowCategories);
	}
	else
	{
		check( SelectedBodyIndex >= 0 && SelectedBodyIndex < PhysicsAsset->BodySetup.Num() );

		// Set properties dialog to display selected bone (or bone instance) info.
		if(bShowInstanceProps)
		{
			PropertyWindow->SetObject(PhysicsAsset->DefaultInstance->Bodies(SelectedBodyIndex), EPropertyWindowFlags::ShouldShowCategories);
		}
		else
		{
			PropertyWindow->SetObject(PhysicsAsset->BodySetup(SelectedBodyIndex), EPropertyWindowFlags::ShouldShowCategories);
		}
	}

	NextSelectEvent =  ( NextSelectEvent == PNS_MakeNewBody ) ? PNS_MakeNewBody :PNS_Normal;
	UpdateControlledBones();
	UpdateNoCollisionBodies();
	UpdateToolBarStatus();
	PhATViewportClient->Viewport->Invalidate();

	// Update the tree control
	if( EditingMode == PEM_BodyEdit )
	{
		TreeCtrl->UnselectAll();

		if( SelectedBodyIndex != INDEX_NONE )
		{
			// todo: store reverse map to avoid linear search
			for( TMap<wxTreeItemId,FPhATTreeBoneItem>::TIterator TreeIt(TreeItemBodyIndexMap); TreeIt; ++TreeIt )
			{
				if( TreeIt.Value().BodyIndex == SelectedBodyIndex &&
					TreeIt.Value().PrimType  == SelectedPrimitiveType &&
					TreeIt.Value().PrimIndex == SelectedPrimitiveIndex )
				{
					TreeCtrl->SelectItem( TreeIt.Key() );
					break;
				}

			}
		}
	}
}

// Center the view on the selected bone/constraint.
void WxPhAT::CenterViewOnSelected()
{
	if(bRunningSimulation)
	{
		return;
	}

	if(EditingMode == PEM_BodyEdit)
	{
		if(SelectedBodyIndex != INDEX_NONE)
		{
			INT BoneIndex = EditorSkelComp->MatchRefBone( PhysicsAsset->BodySetup(SelectedBodyIndex)->BoneName );
			FMatrix BoneTM = EditorSkelComp->GetBoneMatrix(BoneIndex);
			PhATViewportClient->MayaLookAt = BoneTM.GetOrigin();
		}
	}
	else if(EditingMode == PEM_ConstraintEdit)
	{
		if(SelectedConstraintIndex != INDEX_NONE)
		{
			FMatrix ConstraintTM = GetConstraintMatrix(SelectedConstraintIndex, 1, 1.f);
			PhATViewportClient->MayaLookAt = ConstraintTM.GetOrigin();
		}
	}
}

// Fill in array of graphics bones currently being moved by selected physics body.
void WxPhAT::UpdateControlledBones()
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
void WxPhAT::UpdateNoCollisionBodies()
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
			FRigidBodyIndexPair Key(i, SelectedBodyIndex);

			if( PhysicsAsset->BodySetup(SelectedBodyIndex)->bNoCollision ||
				PhysicsAsset->DefaultInstance->CollisionDisableTable.Find(Key) )
				NoCollisionBodies.AddItem(i);
		}
	}
}

void WxPhAT::SetSelectedConstraint(INT ConstraintIndex)
{
	SelectedConstraintIndex = ConstraintIndex;

	if(SelectedConstraintIndex == INDEX_NONE)
	{
		//PropertyWindow->SetObject(PhysicsAsset, true, false, true);
		PropertyWindow->SetObject(EditorSimOptions, EPropertyWindowFlags::ShouldShowCategories);
	}
	else
	{
		check( SelectedConstraintIndex >= 0 && SelectedConstraintIndex < PhysicsAsset->ConstraintSetup.Num() );
		check( PhysicsAsset->ConstraintSetup.Num() == PhysicsAsset->DefaultInstance->Constraints.Num() );

		// Show instance or setup properties as desired.
		if(bShowInstanceProps)
		{
			PropertyWindow->SetObject(PhysicsAsset->DefaultInstance->Constraints(SelectedConstraintIndex), EPropertyWindowFlags::ShouldShowCategories);
		}
		else
		{
			PropertyWindow->SetObject(PhysicsAsset->ConstraintSetup(SelectedConstraintIndex), EPropertyWindowFlags::ShouldShowCategories);
		}
	}	

	NextSelectEvent = PNS_Normal;
	UpdateToolBarStatus();
	PhATViewportClient->Viewport->Invalidate();

	// Update the tree control
	if( EditingMode == PEM_ConstraintEdit )
	{
		TreeCtrl->UnselectAll();

		if( SelectedConstraintIndex != INDEX_NONE )
		{
			// todo: store reverse map to avoid linear search
			for( TMap<wxTreeItemId,INT>::TIterator TreeIt(TreeItemConstraintIndexMap); TreeIt; ++TreeIt )
			{
				if( TreeIt.Value() == SelectedConstraintIndex )
				{
					TreeCtrl->SelectItem( TreeIt.Key() );
					break;
				}

			}
		}
	}
}

void WxPhAT::DisableCollisionWithNextSelect()
{
	if(bRunningSimulation || EditingMode != PEM_BodyEdit || SelectedBodyIndex == INDEX_NONE)
		return;

	NextSelectEvent = PNS_DisableCollision;
	PhATViewportClient->Viewport->Invalidate();

}

void WxPhAT::EnableCollisionWithNextSelect()
{
	if(bRunningSimulation || EditingMode != PEM_BodyEdit || SelectedBodyIndex == INDEX_NONE)
		return;

	NextSelectEvent = PNS_EnableCollision;
	PhATViewportClient->Viewport->Invalidate();

}

void WxPhAT::CopyPropertiesToNextSelect()
{
	if(bRunningSimulation)
		return;

	if(EditingMode == PEM_ConstraintEdit && SelectedConstraintIndex == INDEX_NONE)
		return;

	if(EditingMode == PEM_BodyEdit && SelectedBodyIndex == INDEX_NONE)
		return;

	NextSelectEvent = PNS_CopyProperties;
	PhATViewportClient->Viewport->Invalidate();

}

void WxPhAT::WeldBodyToNextSelect()
{
	if(bRunningSimulation || EditingMode != PEM_BodyEdit || SelectedBodyIndex == INDEX_NONE)
		return;

	NextSelectEvent = PNS_WeldBodies;
	PhATViewportClient->Viewport->Invalidate();

}

void WxPhAT::SetCollisionBetween(INT Body1Index, INT Body2Index, UBOOL bEnableCollision)
{
	if(bRunningSimulation)
		return;

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

void WxPhAT::CopyBodyProperties(INT ToBodyIndex, INT FromBodyIndex)
{
	// Can't do this while simulating!
	if(bRunningSimulation)
	{
		return;
	}

	// Must have two valid bodies (which are different)
	if(ToBodyIndex == INDEX_NONE || FromBodyIndex == INDEX_NONE || ToBodyIndex == FromBodyIndex)
	{
		return;
	}

	// Copy setup/instance properties - based on what we are viewing.
	if(!bShowInstanceProps)
	{
		URB_BodySetup* tbs = PhysicsAsset->BodySetup(ToBodyIndex);
		URB_BodySetup* fbs = PhysicsAsset->BodySetup(FromBodyIndex);
		tbs->CopyBodyPropertiesFrom(fbs);

		SetSelectedBodyAnyPrim(ToBodyIndex);
	}
	else
	{
		URB_BodyInstance* tbi = PhysicsAsset->DefaultInstance->Bodies(ToBodyIndex);
		URB_BodyInstance* fbi = PhysicsAsset->DefaultInstance->Bodies(FromBodyIndex);
		tbi->CopyBodyInstancePropertiesFrom(fbi);
	}

	PhATViewportClient->Viewport->Invalidate();
}

// Supplied body must be a direct child of a bone controlled by selected body.
void WxPhAT::WeldBodyToSelected(INT AddBodyIndex)
{
	if(bRunningSimulation)
		return;


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
		appMsgf(AMT_OK, *LocalizeUnrealEd("Error_CanOnlyWeldParentChildPairs"));
		return;
	}

	check(ChildBodyIndex != INDEX_NONE);
	check(ParentBodyIndex != INDEX_NONE);

	//UBOOL bDoWeld = appMsgf(1,  *FString::Printf(LocalizeSecure(LocalizeUnrealEd("Prompt_14"), *AddBoneName, *SelectedBoneName)) );
	UBOOL bDoWeld = true;
	if(bDoWeld)
	{
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("WeldBodies") );

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
		}

		// update the tree
		FillTree();

		// Body index may have changed, so we re-find it.
		INT NewSelectedIndex = PhysicsAsset->FindBodyIndex(ParentBoneName);
		SetSelectedBody(NewSelectedIndex, SelectedPrimitiveType, SelectedPrimitiveIndex); // This redraws the viewport as well...

		// Just to be safe - deselect any selected constraints
		SetSelectedConstraint(INDEX_NONE);
	}
	
}

void WxPhAT::MakeNewBodyFromNextSelect()
{
	if( bRunningSimulation || EditingMode != PEM_BodyEdit )
	{
		return;
	}

	NextSelectEvent = PNS_MakeNewBody;

	PhATViewportClient->Viewport->Invalidate();

	FillTree();
	TreeCtrl->UnselectAll();
}

/**
 * Helper method to initialize a constraint setup between two bodies.
 *
 * @param	ConstraintSetup		Constraint setup to initialize
 * @param	ChildBodyIndex		Index of the child body in the physics asset body setup array
 * @param	ParentBodyIndex		Index of the parent body in the physics asset body setup array
 */
void WxPhAT::InitConstraintSetup( URB_ConstraintSetup* ConstraintSetup, INT ChildBodyIndex, INT ParentBodyIndex )
{
	check( ConstraintSetup );

	URB_BodySetup* ChildBodySetup = PhysicsAsset->BodySetup( ChildBodyIndex );
	URB_BodySetup* ParentBodySetup = PhysicsAsset->BodySetup( ParentBodyIndex );
	check( ChildBodySetup && ParentBodySetup );

	const INT ChildBoneIndex = EditorSkelMesh->MatchRefBone( ChildBodySetup->BoneName );
	const INT ParentBoneIndex = EditorSkelMesh->MatchRefBone( ParentBodySetup->BoneName );
	check( ChildBoneIndex != INDEX_NONE && ParentBoneIndex != INDEX_NONE );

	// Transform of child from parent is just child ref-pose entry.
	FMatrix ChildBoneTM = EditorSkelComp->GetBoneMatrix( ChildBoneIndex );
	ChildBoneTM.RemoveScaling();

	FMatrix ParentBoneTM = EditorSkelComp->GetBoneMatrix( ParentBoneIndex );
	ParentBoneTM.RemoveScaling();

	FMatrix RelTM = ChildBoneTM * ParentBoneTM.InverseSafe();

	// Place joint at origin of child
	ConstraintSetup->ConstraintBone1 = ChildBodySetup->BoneName;
	ConstraintSetup->Pos1 = FVector( 0, 0, 0 );
	ConstraintSetup->PriAxis1 = FVector( 1, 0, 0 );
	ConstraintSetup->SecAxis1 = FVector( 0, 1, 0 );

	ConstraintSetup->ConstraintBone2 = ParentBodySetup->BoneName;
	ConstraintSetup->Pos2 = RelTM.GetOrigin() * U2PScale;
	ConstraintSetup->PriAxis2 = RelTM.GetAxis( 0 );
	ConstraintSetup->SecAxis2 = RelTM.GetAxis( 1 );

	// Disable collision between constrained bodies by default.
	SetCollisionBetween( ChildBodyIndex, ParentBodyIndex, FALSE );
}

/** Creates a new collision body */
void WxPhAT::MakeNewBody(INT NewBoneIndex)
{
	FName NewBoneName = EditorSkelMesh->RefSkeleton(NewBoneIndex).Name;

	// If this body is already physical - do nothing.
	INT NewBodyIndex = PhysicsAsset->FindBodyIndex(NewBoneName);
	if(NewBodyIndex != INDEX_NONE)
	{
		SetSelectedBodyAnyPrim(NewBodyIndex);
		FillTree();
		return;
	}

	WxDlgNewPhysicsAsset AssetDlg;
	if( AssetDlg.ShowModal( NULL, true ) != wxID_OK )
	{
		FillTree();
		return;
	}

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	// Find body that currently controls this bone.
	INT ParentBodyIndex = PhysicsAsset->FindControllingBodyIndex(EditorSkelMesh, NewBoneIndex);

	// Create the physics body.
	NewBodyIndex = PhysicsAsset->CreateNewBody( NewBoneName );
	URB_BodySetup* bs = PhysicsAsset->BodySetup( NewBodyIndex );
	check(bs->BoneName == NewBoneName);

	// Create a new physics body for this bone.
	if(AssetDlg.Params.VertWeight == EVW_DominantWeight)
	{
		PhysicsAsset->CreateCollisionFromBone(bs, EditorSkelMesh, NewBoneIndex, AssetDlg.Params, DominantWeightBoneInfos);
	}
	else
	{
		PhysicsAsset->CreateCollisionFromBone(bs, EditorSkelMesh, NewBoneIndex, AssetDlg.Params, AnyWeightBoneInfos);
	}

	// Check if the bone of the new body has any physical children bones
	for( INT i = 0; i < EditorSkelMesh->RefSkeleton.Num(); ++i )
	{
		if( EditorSkelMesh->BoneIsChildOf( i, NewBoneIndex ) )
		{
			const INT ChildBodyIndex = PhysicsAsset->FindBodyIndex( EditorSkelMesh->RefSkeleton(i).Name );
			
			// If the child bone is physical, it may require fixing up in regards to constraints
			if( ChildBodyIndex != INDEX_NONE )
			{
				URB_BodySetup* ChildBody = PhysicsAsset->BodySetup( ChildBodyIndex );
				check( ChildBody );

				INT ConstraintIndex = PhysicsAsset->FindConstraintIndex( ChildBody->BoneName );
				
				// If the child body is not constrained already, create a new constraint between
				// the child body and the new body
				if ( ConstraintIndex == INDEX_NONE )
				{
					ConstraintIndex = PhysicsAsset->CreateNewConstraint( ChildBody->BoneName );
					check( ConstraintIndex != INDEX_NONE );
				}
				// If there's a pre-existing constraint, see if it needs to be fixed up
				else
				{
					URB_ConstraintSetup* ExistingConstraintSetup = PhysicsAsset->ConstraintSetup( ConstraintIndex );
					check( ExistingConstraintSetup );
					
					const INT ExistingConstraintBoneIndex = EditorSkelMesh->MatchRefBone( ExistingConstraintSetup->ConstraintBone2 );
					check( ExistingConstraintBoneIndex != INDEX_NONE );

					// If the constraint exists between two child bones, then no fix up is required
					if ( EditorSkelMesh->BoneIsChildOf( ExistingConstraintBoneIndex, NewBoneIndex ) )
					{
						continue;
					}
					
					// If the constraint isn't between two child bones, then it is between a physical bone higher in the bone
					// hierarchy than the new bone, so it needs to be fixed up by setting the constraint to point to the new bone
					// instead. Additionally, collision needs to be re-enabled between the child bone and the identified "grandparent"
					// bone.
					const INT ExistingConstraintBodyIndex = PhysicsAsset->FindBodyIndex( ExistingConstraintSetup->ConstraintBone2 );
					check( ExistingConstraintBodyIndex != INDEX_NONE );
					check( ExistingConstraintBodyIndex == ParentBodyIndex );

					SetCollisionBetween( ChildBodyIndex, ExistingConstraintBodyIndex, TRUE );
				}

				URB_ConstraintSetup* ChildConstraintSetup = PhysicsAsset->ConstraintSetup( ConstraintIndex );
				check( ChildConstraintSetup );
				InitConstraintSetup( ChildConstraintSetup, ChildBodyIndex, NewBodyIndex );
			}
		}
	}

	// If we have a physics parent, create a joint to it.
	if(ParentBodyIndex != INDEX_NONE)
	{
		const INT NewConstraintIndex = PhysicsAsset->CreateNewConstraint( NewBoneName );
		URB_ConstraintSetup* ConstraintSetup = PhysicsAsset->ConstraintSetup( NewConstraintIndex );
		check( ConstraintSetup );

		InitConstraintSetup( ConstraintSetup, NewBodyIndex, ParentBodyIndex );
	}

	// update the tree
	FillTree();

	SetSelectedBodyAnyPrim(NewBodyIndex);
}

/** Show the floating 'Sim Options' window. */
void WxPhAT::ShowSimOptionsWindow()
{
	if(!SimOptionsWindow)
	{
		SimOptionsWindow = new WxPropertyWindowFrame;
		SimOptionsWindow->Create( this, -1, this );
		SimOptionsWindow->SetSize( 64,64, 350,600 );
	}

	SimOptionsWindow->SetObject( EditorSimOptions, EPropertyWindowFlags::ShouldShowCategories );
	SimOptionsWindow->Show();
}

/** Toggle between setup properties and default per-instance properties, for the selected obejct. */
void WxPhAT::ToggleInstanceProperties()
{
	bShowInstanceProps = !bShowInstanceProps;
	ToolBar->ToggleTool( IDMN_PHAT_INSTANCEPROPS, bShowInstanceProps == TRUE );
	PhATViewportClient->Viewport->Invalidate();

	if(EditingMode == PEM_ConstraintEdit)
	{
		if(SelectedConstraintIndex != INDEX_NONE)
		{
			URB_ConstraintSetup* ConSetup = PhysicsAsset->ConstraintSetup(SelectedConstraintIndex);
			URB_ConstraintInstance* ConInstance = PhysicsAsset->DefaultInstance->Constraints(SelectedConstraintIndex);

			// Show the per-instance structure or shared setup structure as desired.
			if(bShowInstanceProps)
			{
				PropertyWindow->SetObject(ConInstance, EPropertyWindowFlags::ShouldShowCategories);
			}
			else
			{
				PropertyWindow->SetObject(ConSetup, EPropertyWindowFlags::ShouldShowCategories);
			}
		}
	}
	else if( EditingMode == PEM_BodyEdit )
	{
		if(SelectedBodyIndex != INDEX_NONE)
		{
			URB_BodySetup* BodySetup = PhysicsAsset->BodySetup(SelectedBodyIndex);
			URB_BodyInstance* BodyInstance = PhysicsAsset->DefaultInstance->Bodies(SelectedBodyIndex);

			// Set properties dialog to display selected bone (or bone instance) info.
			if(bShowInstanceProps)
			{
				PropertyWindow->SetObject(BodyInstance, EPropertyWindowFlags::ShouldShowCategories);
			}
			else
			{
				PropertyWindow->SetObject(BodySetup, EPropertyWindowFlags::ShouldShowCategories);
			}
		}
	}
}

void WxPhAT::SetAssetPhysicalMaterial()
{
	GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);
	UPhysicalMaterial* SelectedPhysMaterial = GEditor->GetSelectedObjects()->GetTop<UPhysicalMaterial>();

	if(SelectedPhysMaterial)
	{
		for(INT BodyIdx=0; BodyIdx<PhysicsAsset->BodySetup.Num(); BodyIdx++)
		{
			URB_BodySetup* BodySetup = PhysicsAsset->BodySetup(BodyIdx);
			BodySetup->PhysMaterial = SelectedPhysMaterial;
		}
	}
}

void WxPhAT::CopyJointSettingsToAll()
{
	// Don't do anything if running sim or not in constraint editing mode
	if(bRunningSimulation || EditingMode != PEM_ConstraintEdit || SelectedConstraintIndex == INDEX_NONE)
	{
		return;
	}

	// Quite a significant thing to do - so warn.
	const UBOOL bProceed = appMsgf( AMT_YesNo, *LocalizeUnrealEd("CopyToAllJointsWarning") );
	if(!bProceed)
	{
		return;
	}

	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("CopyJointSettingsToAll")) );

	// Iterate over all constraints
	for(INT i=0; i<PhysicsAsset->ConstraintSetup.Num(); i++)
	{
		// Don't copy to yourself!
		if(i != SelectedConstraintIndex)
		{
			CopyConstraintProperties(i, SelectedConstraintIndex);
		}
	}

	// Force redraw.
	PhATViewportClient->Viewport->Invalidate();
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
void WxPhAT::SetSelectedConstraintRelTM(const FMatrix& RelTM)
{
	FMatrix WParentFrame = GetSelectedConstraintWorldTM(1);
	FMatrix WNewChildFrame = RelTM * WParentFrame;

	URB_ConstraintSetup* cs = PhysicsAsset->ConstraintSetup(SelectedConstraintIndex);

	// Get child bone transform
	INT BoneIndex = EditorSkelMesh->MatchRefBone( cs->ConstraintBone1 );
	check(BoneIndex != INDEX_NONE);

	FMatrix BoneTM = EditorSkelComp->GetBoneMatrix(BoneIndex);
	BoneTM.RemoveScaling();

	cs->SetRefFrameMatrix(0, WNewChildFrame * BoneTM.InverseSafe() );
}

// Keep con frame 0 fixed, and update con frame 1
void WxPhAT::SnapConstraintToBone(INT ConstraintIndex, const FMatrix& WParentFrame)
{
	URB_ConstraintSetup* cs = PhysicsAsset->ConstraintSetup(ConstraintIndex);

	// Get child bone transform
	INT BoneIndex = EditorSkelMesh->MatchRefBone( cs->ConstraintBone1 );
	check(BoneIndex != INDEX_NONE);

	FMatrix BoneTM = EditorSkelComp->GetBoneMatrix(BoneIndex);

	FMatrix Con1Matrix = PhysicsAsset->ConstraintSetup(ConstraintIndex)->GetRefFrameMatrix(1);
	FMatrix Con0Matrix = PhysicsAsset->ConstraintSetup(ConstraintIndex)->GetRefFrameMatrix(0);

	cs->SetRefFrameMatrix(1, Con0Matrix * BoneTM * WParentFrame.InverseSafe() * Con1Matrix );

}

// Snap selected constraint to the bone
void WxPhAT::SnapSelectedConstraintToBone()
{
	FMatrix WParentFrame = GetSelectedConstraintWorldTM(1);
	SnapConstraintToBone(SelectedConstraintIndex, WParentFrame);
}

// Snap all constraints to the bone
void WxPhAT::SnapAllConstraintsToBone()
{
	for(INT i=0; i<PhysicsAsset->ConstraintSetup.Num(); i++)
	{
		FMatrix WParentFrame = GetConstraintMatrix(i, 1, 1.0f);
		SnapConstraintToBone(i, WParentFrame);
	}
}

void WxPhAT::CycleSelectedConstraintOrientation()
{
	if(EditingMode != PEM_ConstraintEdit || SelectedConstraintIndex == INDEX_NONE)
		return;

	URB_ConstraintSetup* cs = PhysicsAsset->ConstraintSetup(SelectedConstraintIndex);
	FMatrix ConMatrix = cs->GetRefFrameMatrix(1);

	FMatrix WParentFrame = GetSelectedConstraintWorldTM(1);
	FMatrix WChildFrame = GetSelectedConstraintWorldTM(0);
	FMatrix RelTM = WChildFrame * WParentFrame.InverseSafe();

	CycleMatrixRows(&ConMatrix);
	cs->SetRefFrameMatrix(1, ConMatrix);

	SetSelectedConstraintRelTM(RelTM);

	PhATViewportClient->Viewport->Invalidate();
}

FMatrix WxPhAT::GetSelectedConstraintWorldTM(INT BodyIndex)
{
	if(SelectedConstraintIndex == INDEX_NONE)
		return FMatrix::Identity;

	URB_ConstraintSetup* cs = PhysicsAsset->ConstraintSetup(SelectedConstraintIndex);

	FMatrix Frame = cs->GetRefFrameMatrix(BodyIndex);

	INT BoneIndex;
	if(BodyIndex == 0)
		BoneIndex = EditorSkelMesh->MatchRefBone( cs->ConstraintBone1 );
	else
		BoneIndex = EditorSkelMesh->MatchRefBone( cs->ConstraintBone2 );
	check(BoneIndex != INDEX_NONE);

	FMatrix BoneTM = EditorSkelComp->GetBoneMatrix(BoneIndex);
	BoneTM.RemoveScaling();

	return Frame * BoneTM;
}

// This leaves the joint ref frames unchanged, but copies limit info/type.
// Assumes that Begin has been called on undo buffer before calling this.
void WxPhAT::CopyConstraintProperties(INT ToConstraintIndex, INT FromConstraintIndex)
{
	if(ToConstraintIndex == INDEX_NONE || FromConstraintIndex == INDEX_NONE)
		return;

	// If we are showing instance properties - copy instance properties. If showing setup, just copy setup properties.
	if(!bShowInstanceProps)
	{
		URB_ConstraintSetup* tcs = PhysicsAsset->ConstraintSetup(ToConstraintIndex);
		URB_ConstraintSetup* fcs = PhysicsAsset->ConstraintSetup(FromConstraintIndex);

		tcs->Modify();
		tcs->CopyConstraintParamsFrom(fcs);
	}
	else
	{
		URB_ConstraintInstance* tci = PhysicsAsset->DefaultInstance->Constraints(ToConstraintIndex);
		URB_ConstraintInstance* fci = PhysicsAsset->DefaultInstance->Constraints(FromConstraintIndex);

		tci->Modify();
		tci->CopyInstanceParamsFrom(fci);
	}
}

void WxPhAT::Undo()
{
	if(bRunningSimulation)
		return;

	// Clear selection before we undo. We don't transact the editor itself - don't want to have something selected that is then removed.
	SetSelectedBody(INDEX_NONE, KPT_Unknown, INDEX_NONE);
	SetSelectedConstraint(INDEX_NONE);

	GEditor->UndoTransaction();
	PhATViewportClient->Viewport->Invalidate();
	FillTree();
}

void WxPhAT::Redo()
{
	if(bRunningSimulation)
		return;

	SetSelectedBody(INDEX_NONE, KPT_Unknown, INDEX_NONE);
	SetSelectedConstraint(INDEX_NONE);

	GEditor->RedoTransaction();
	PhATViewportClient->Viewport->Invalidate();
	FillTree();
}

void WxPhAT::ToggleSelectionLock()
{
	if(bRunningSimulation)
		return;

	bSelectionLock = !bSelectionLock;
	PhATViewportClient->Viewport->Invalidate();
}

void WxPhAT::ToggleSnap()
{
	if(bRunningSimulation)
		return;

	bSnap = !bSnap;
	ToolBar->ToggleTool( IDMN_PHAT_SNAP, bSnap == TRUE );
	PhATViewportClient->Viewport->Invalidate();
}

void WxPhAT::ToggleEditingMode()
{
	if(bRunningSimulation)
		return;

	if(EditingMode == PEM_ConstraintEdit)
	{
		EditingMode = PEM_BodyEdit;
		FillTree();
		SetSelectedBody(SelectedBodyIndex, SelectedPrimitiveType, SelectedPrimitiveIndex); // Forces properties panel to update...
		ToolBar->ModeButton->SetBitmapLabel(ToolBar->BodyModeB);
	}
	else
	{
		EditingMode = PEM_ConstraintEdit;
		FillTree();
		SetSelectedConstraint(SelectedConstraintIndex);
		ToolBar->ModeButton->SetBitmapLabel(ToolBar->ConstraintModeB);

		// Scale isn't valid for constraints!
		if(MovementMode == PMM_Scale)
			SetMovementMode(PMM_Translate);
	}

	NextSelectEvent = PNS_Normal;
	UpdateToolBarStatus();
	PhATViewportClient->Viewport->Invalidate();
}

void WxPhAT::SetMovementMode(EPhATMovementMode NewMovementMode)
{
	if(bRunningSimulation)
		return;

	MovementMode = NewMovementMode;

	if(MovementMode == PMM_Rotate)
	{
		ToolBar->ToggleTool(IDMN_PHAT_ROTATE, true);
		ToolBar->ToggleTool(IDMN_PHAT_TRANSLATE, false);
		ToolBar->ToggleTool(IDMN_PHAT_SCALE, false);
	}
	else if(MovementMode == PMM_Translate)
	{
		ToolBar->ToggleTool(IDMN_PHAT_ROTATE, false);
		ToolBar->ToggleTool(IDMN_PHAT_TRANSLATE, true);
		ToolBar->ToggleTool(IDMN_PHAT_SCALE, false);
	}
	else if(MovementMode == PMM_Scale)
	{
		ToolBar->ToggleTool(IDMN_PHAT_ROTATE, false);
		ToolBar->ToggleTool(IDMN_PHAT_TRANSLATE, false);
		ToolBar->ToggleTool(IDMN_PHAT_SCALE, true);
	}

	PhATViewportClient->Viewport->Invalidate();
}

void WxPhAT::CycleMovementMode()
{
	if(bRunningSimulation)
		return;

	if(MovementMode == PMM_Translate)
	{
		SetMovementMode(PMM_Rotate);
	}
	else if(MovementMode == PMM_Rotate)
	{
		SetMovementMode(PMM_Scale);
	}
	else if(MovementMode == PMM_Scale)
	{
		SetMovementMode(PMM_Translate);
	}
}

void WxPhAT::ToggleMovementSpace()
{
	if(MovementSpace == PMS_Local)
	{
		MovementSpace = PMS_World;
		ToolBar->SpaceButton->SetBitmapLabel(ToolBar->WorldSpaceB);
	}
	else
	{
		MovementSpace = PMS_Local;
		ToolBar->SpaceButton->SetBitmapLabel(ToolBar->LocalSpaceB);
	}

	PhATViewportClient->Viewport->Invalidate();
}

void WxPhAT::UpdateToolBarStatus()
{	
	if(bRunningSimulation) // Disable everything.
	{
		ToolBar->EnableTool( IDMN_PHAT_ROTATE, false );
		ToolBar->EnableTool( IDMN_PHAT_TRANSLATE, false );
		ToolBar->EnableTool( IDMN_PHAT_MOVESPACE, false );
		ToolBar->EnableTool( IDMN_PHAT_COPYPROPERTIES, false );
		
		ToolBar->EnableTool( IDMN_PHAT_SCALE, false );

		ToolBar->EnableTool( IDMN_PHAT_DISABLECOLLISION, false );
		ToolBar->EnableTool( IDMN_PHAT_ENABLECOLLISION, false );
		ToolBar->EnableTool( IDMN_PHAT_WELDBODIES, false );
		ToolBar->EnableTool( IDMN_PHAT_ADDNEWBODY, false );

		ToolBar->EnableTool( IDMN_PHAT_ADDSPHERE, false );
		ToolBar->EnableTool( IDMN_PHAT_ADDSPHYL, false );
		ToolBar->EnableTool( IDMN_PHAT_ADDBOX, false );
		ToolBar->EnableTool( IDMN_PHAT_DUPLICATEPRIM, false );
		ToolBar->EnableTool( IDMN_PHAT_DELETEPRIM, false );

		ToolBar->EnableTool( IDMN_PHAT_RESETCONFRAME, false );
		ToolBar->EnableTool( IDMN_PHAT_SNAPCONTOBONE, false );
		ToolBar->EnableTool( IDMN_PHAT_SNAPALLCONTOBONE, false );
		ToolBar->EnableTool( IDMN_PHAT_ADDBS, false );
		ToolBar->EnableTool( IDMN_PHAT_ADDHINGE, false );
		ToolBar->EnableTool( IDMN_PHAT_ADDPRISMATIC, false );
		ToolBar->EnableTool( IDMN_PHAT_ADDSKELETAL, false );
		ToolBar->EnableTool( IDMN_PHAT_DELETECONSTRAINT, false );
	}
	else // Enable stuff for current editing mode.
	{
		ToolBar->EnableTool( IDMN_PHAT_ROTATE, true );
		ToolBar->EnableTool( IDMN_PHAT_TRANSLATE, true );
		ToolBar->EnableTool( IDMN_PHAT_MOVESPACE, true );

		if(EditingMode == PEM_BodyEdit) //// BODY MODE ////
		{
			ToolBar->EnableTool( IDMN_PHAT_SCALE, true );

			ToolBar->EnableTool( IDMN_PHAT_ADDNEWBODY, true );

			if(SelectedBodyIndex != INDEX_NONE)
			{
				ToolBar->EnableTool( IDMN_PHAT_COPYPROPERTIES, true );

				ToolBar->EnableTool( IDMN_PHAT_DISABLECOLLISION, true );
				ToolBar->EnableTool( IDMN_PHAT_ENABLECOLLISION, true );
				ToolBar->EnableTool( IDMN_PHAT_WELDBODIES, true );

				ToolBar->EnableTool( IDMN_PHAT_ADDSPHERE, true );
				ToolBar->EnableTool( IDMN_PHAT_ADDSPHYL, true );
				ToolBar->EnableTool( IDMN_PHAT_ADDBOX, true );
				ToolBar->EnableTool( IDMN_PHAT_DUPLICATEPRIM, true );
				ToolBar->EnableTool( IDMN_PHAT_DELETEPRIM, true );
			}
			else
			{
				ToolBar->EnableTool( IDMN_PHAT_COPYPROPERTIES, false );

				ToolBar->EnableTool( IDMN_PHAT_DISABLECOLLISION, false );
				ToolBar->EnableTool( IDMN_PHAT_ENABLECOLLISION, false );
				ToolBar->EnableTool( IDMN_PHAT_WELDBODIES, false );

				ToolBar->EnableTool( IDMN_PHAT_ADDSPHERE, false );
				ToolBar->EnableTool( IDMN_PHAT_ADDSPHYL, false );
				ToolBar->EnableTool( IDMN_PHAT_ADDBOX, false );
				ToolBar->EnableTool( IDMN_PHAT_DUPLICATEPRIM, false );
				ToolBar->EnableTool( IDMN_PHAT_DELETEPRIM, false );
			}

			ToolBar->EnableTool( IDMN_PHAT_RESETCONFRAME, false );
			ToolBar->EnableTool( IDMN_PHAT_SNAPCONTOBONE, false );
			ToolBar->EnableTool( IDMN_PHAT_SNAPALLCONTOBONE, false );
			ToolBar->EnableTool( IDMN_PHAT_ADDBS, false );
			ToolBar->EnableTool( IDMN_PHAT_ADDHINGE, false );
			ToolBar->EnableTool( IDMN_PHAT_ADDPRISMATIC, false );
			ToolBar->EnableTool( IDMN_PHAT_ADDSKELETAL, false );
			ToolBar->EnableTool( IDMN_PHAT_DELETECONSTRAINT, false );
		}
		else //// CONSTRAINT MODE ////
		{
			ToolBar->EnableTool( IDMN_PHAT_SCALE, false );

			ToolBar->EnableTool( IDMN_PHAT_DISABLECOLLISION, false );
			ToolBar->EnableTool( IDMN_PHAT_ENABLECOLLISION, false );
			ToolBar->EnableTool( IDMN_PHAT_WELDBODIES, false );
			ToolBar->EnableTool( IDMN_PHAT_ADDNEWBODY, false );

			ToolBar->EnableTool( IDMN_PHAT_ADDSPHERE, false );
			ToolBar->EnableTool( IDMN_PHAT_ADDSPHYL, false );
			ToolBar->EnableTool( IDMN_PHAT_ADDBOX, false );
			ToolBar->EnableTool( IDMN_PHAT_DUPLICATEPRIM, false );
			ToolBar->EnableTool( IDMN_PHAT_DELETEPRIM, false );	
			ToolBar->EnableTool( IDMN_PHAT_SNAPALLCONTOBONE, true );

			if(SelectedConstraintIndex != INDEX_NONE)
			{
				ToolBar->EnableTool( IDMN_PHAT_COPYPROPERTIES, true );	

				ToolBar->EnableTool( IDMN_PHAT_RESETCONFRAME, true );	
				ToolBar->EnableTool( IDMN_PHAT_SNAPCONTOBONE, true );
				ToolBar->EnableTool( IDMN_PHAT_ADDBS, true );	
				ToolBar->EnableTool( IDMN_PHAT_ADDHINGE, true );	
				ToolBar->EnableTool( IDMN_PHAT_ADDPRISMATIC, true );	
				ToolBar->EnableTool( IDMN_PHAT_ADDSKELETAL, true );	
				ToolBar->EnableTool( IDMN_PHAT_DELETECONSTRAINT, true );	
			}
			else
			{
				ToolBar->EnableTool( IDMN_PHAT_COPYPROPERTIES, false );	

				ToolBar->EnableTool( IDMN_PHAT_RESETCONFRAME, false );	
				ToolBar->EnableTool( IDMN_PHAT_SNAPCONTOBONE, false );
				ToolBar->EnableTool( IDMN_PHAT_ADDBS, false );	
				ToolBar->EnableTool( IDMN_PHAT_ADDHINGE, false );	
				ToolBar->EnableTool( IDMN_PHAT_ADDPRISMATIC, false );	
				ToolBar->EnableTool( IDMN_PHAT_ADDSKELETAL, false );	
				ToolBar->EnableTool( IDMN_PHAT_DELETECONSTRAINT, false );	
			}
		}
	}

	// Update view mode icons.
	EPhATRenderMode MeshViewMode = GetCurrentMeshViewMode();
	if(MeshViewMode == PRM_None)
		ToolBar->MeshViewButton->SetBitmapLabel(ToolBar->HideMeshB);
	else if(MeshViewMode == PRM_Wireframe)
		ToolBar->MeshViewButton->SetBitmapLabel(ToolBar->WireMeshB);
	else if(MeshViewMode == PRM_Solid)
		ToolBar->MeshViewButton->SetBitmapLabel(ToolBar->ShowMeshB);

	EPhATRenderMode CollisionViewMode = GetCurrentCollisionViewMode();
	if(CollisionViewMode == PRM_None)
		ToolBar->CollisionViewButton->SetBitmapLabel(ToolBar->HideCollB);
	else if(CollisionViewMode == PRM_Wireframe)
		ToolBar->CollisionViewButton->SetBitmapLabel(ToolBar->WireCollB);
	else if(CollisionViewMode == PRM_Solid)
		ToolBar->CollisionViewButton->SetBitmapLabel(ToolBar->ShowCollB);

	EPhATConstraintViewMode ConstraintViewMode = GetCurrentConstraintViewMode();
	if(ConstraintViewMode == PCV_None)
		ToolBar->ConstraintViewButton->SetBitmapLabel(ToolBar->ConHideB);
	else if(ConstraintViewMode == PCV_AllPositions)
		ToolBar->ConstraintViewButton->SetBitmapLabel(ToolBar->ConPosB);
	else if(ConstraintViewMode == PCV_AllLimits)
		ToolBar->ConstraintViewButton->SetBitmapLabel(ToolBar->ConLimitB);

	ToolBar->Refresh();
}	

static wxTreeItemId PhAT_CreateTreeItemAndParents(INT BoneIndex, TArray<FMeshBone>& Skeleton, wxTreeCtrl* TreeCtrl, TMap<FName,wxTreeItemId>& BoneTreeItemMap)
{
	wxTreeItemId* existingItem = BoneTreeItemMap.Find( Skeleton(BoneIndex).Name );
	if( existingItem != NULL )
	{
		return *existingItem;
	}

	FMeshBone& Bone = Skeleton(BoneIndex);
	wxTreeItemId newItem;
	if( BoneIndex == 0 )
	{
		newItem = TreeCtrl->AddRoot( *Bone.Name.ToString() );
	}
	else
	{
		wxTreeItemId parentItem = PhAT_CreateTreeItemAndParents(Bone.ParentIndex, Skeleton, TreeCtrl, BoneTreeItemMap);
		newItem = TreeCtrl->AppendItem( parentItem, *Bone.Name.ToString() );
		TreeCtrl->Expand( parentItem );
	}    

	BoneTreeItemMap.Set(Bone.Name, newItem);

	return newItem;
}


/**
* Posts an event for regenerating the tree view data.
*/
void WxPhAT::FillTree()
{
	wxCommandEvent Event;
	Event.SetEventObject(this);
	Event.SetEventType(ID_PHAT_FILLTREE);
	GetEventHandler()->AddPendingEvent(Event);
}

/**
 * Event handler for regenerating the tree view data.
 *
 * @param	In	Information about the event.
 */
void WxPhAT::OnFillTree(wxCommandEvent &In)
{
	TreeCtrl->Freeze();
	{
		TreeCtrl->DeleteAllItems();

		TreeItemBoneIndexMap.Empty();
		TreeItemBodyIndexMap.Empty();
		TreeItemConstraintIndexMap.Empty();

		TMap<FName,wxTreeItemId>	BoneTreeItemMap;

		// if next event is selecting a bone to create a new body, Fill up tree with bone names
		if( NextSelectEvent == PNS_MakeNewBody )
		{
			// Fill Tree with all bones...
			for(INT BoneIndex=0; BoneIndex<EditorSkelMesh->RefSkeleton.Num(); BoneIndex++)
			{
				wxTreeItemId newItem = PhAT_CreateTreeItemAndParents(BoneIndex, EditorSkelMesh->RefSkeleton, TreeCtrl, BoneTreeItemMap);
				if ( PhysicsAsset->FindBodyIndex( EditorSkelMesh->RefSkeleton( BoneIndex ).Name ) == INDEX_NONE )
				{
					TreeCtrl->SetItemBold(newItem);
				}

				TreeItemBoneIndexMap.Set(newItem, BoneIndex);
			}
		}
		else if( EditingMode == PEM_BodyEdit )
		{
			// fill tree with bodies
			for(INT i=0; i<PhysicsAsset->BodySetup.Num(); i++)
			{
				INT BoneIndex = EditorSkelComp->MatchRefBone(PhysicsAsset->BodySetup(i)->BoneName);
				if(BoneIndex != INDEX_NONE)
				{
					wxTreeItemId newItem = PhAT_CreateTreeItemAndParents(BoneIndex, EditorSkelMesh->RefSkeleton, TreeCtrl, BoneTreeItemMap);

					FKAggregateGeom& AggGeom = PhysicsAsset->BodySetup(i)->AggGeom;

					if( AggGeom.SphereElems.Num()+AggGeom.BoxElems.Num()+AggGeom.SphylElems.Num()+AggGeom.ConvexElems.Num() == 1 )
					{
						// Case for only a single primitive
						TreeCtrl->SetItemBold( newItem );

						if( AggGeom.SphereElems.Num() == 1 )
							TreeItemBodyIndexMap.Set(newItem,FPhATTreeBoneItem(i, KPT_Sphere, 0));
						else
							if( AggGeom.BoxElems.Num() == 1 )
								TreeItemBodyIndexMap.Set(newItem,FPhATTreeBoneItem(i, KPT_Box, 0));
							else
								if( AggGeom.SphylElems.Num() == 1 )
									TreeItemBodyIndexMap.Set(newItem,FPhATTreeBoneItem(i, KPT_Sphyl, 0));
								else
									if( AggGeom.ConvexElems.Num() == 1 )
										TreeItemBodyIndexMap.Set(newItem,FPhATTreeBoneItem(i, KPT_Convex, 0));
					}
					else
					{
						// case for multiple primitives
						for(INT j=0; j<AggGeom.SphereElems.Num(); j++)
						{
							wxTreeItemId subItem = TreeCtrl->AppendItem( newItem, *FString::Printf(TEXT("Sphere %d"),j) );
							TreeCtrl->SetItemBold( subItem );
							TreeItemBodyIndexMap.Set(subItem,FPhATTreeBoneItem(i, KPT_Sphere, j));
						}

						for(INT j=0; j<AggGeom.BoxElems.Num(); j++)
						{
							wxTreeItemId subItem = TreeCtrl->AppendItem( newItem, *FString::Printf(TEXT("Box %d"),j) );
							TreeCtrl->SetItemBold( subItem );
							TreeItemBodyIndexMap.Set(subItem,FPhATTreeBoneItem(i, KPT_Box, j));
						}

						for(INT j=0; j<AggGeom.SphylElems.Num(); j++)
						{
							wxTreeItemId subItem = TreeCtrl->AppendItem( newItem, *FString::Printf(TEXT("Sphyl %d"),j) );
							TreeCtrl->SetItemBold( subItem );
							TreeItemBodyIndexMap.Set(subItem,FPhATTreeBoneItem(i, KPT_Sphyl, j));
						}

						for(INT j=0; j<AggGeom.ConvexElems.Num(); j++)
						{
							wxTreeItemId subItem = TreeCtrl->AppendItem( newItem, *FString::Printf(TEXT("Convex %d"),j) );
							TreeCtrl->SetItemBold( subItem );
							TreeItemBodyIndexMap.Set(subItem,FPhATTreeBoneItem(i, KPT_Convex, j));
						}
						TreeCtrl->Expand( newItem );
					}
				}
			}
		}
		else
		{
			// fill tree with constraints
			for(INT i=0; i<PhysicsAsset->ConstraintSetup.Num(); i++)
			{
				// try to find the 1st bone, 2nd finally the joint name in the hierarchy
				INT BoneIndex = EditorSkelComp->MatchRefBone( PhysicsAsset->ConstraintSetup(i)->ConstraintBone1 );
				if( BoneIndex == INDEX_NONE )
					BoneIndex = EditorSkelComp->MatchRefBone( PhysicsAsset->ConstraintSetup(i)->ConstraintBone2 );
				if( BoneIndex == INDEX_NONE )
					BoneIndex = EditorSkelComp->MatchRefBone( PhysicsAsset->ConstraintSetup(i)->JointName );
				if( BoneIndex == INDEX_NONE )
					continue;

				wxTreeItemId newItem = PhAT_CreateTreeItemAndParents( BoneIndex, EditorSkelMesh->RefSkeleton, TreeCtrl, BoneTreeItemMap );
				TreeCtrl->SetItemBold( newItem );
				TreeItemConstraintIndexMap.Set(newItem,i);
			}
		}
	}
	TreeCtrl->Thaw();
}

/** DblClick */
void WxPhAT::OnTreeItemDblClick(wxTreeEvent& InEvent)
{
	wxTreeItemId Item = InEvent.GetItem();

	// set the focus to the viewport and disable mouse capture
	PreviewWindow->SetFocus();
	::SetFocus( (HWND) PhATViewportClient->Viewport->GetWindow() );

	// If creating a new body
	if( NextSelectEvent == PNS_MakeNewBody )
	{
		const INT* BoneIdxPtr	= TreeItemBoneIndexMap.Find(Item);

		if( BoneIdxPtr )
		{
			const INT NewBoneIndex = *BoneIdxPtr;

			NextSelectEvent = PNS_Normal;

			//calls FillTree()
			MakeNewBody(NewBoneIndex);
		}
	}

	//HitNothing();
}


void WxPhAT::OnTreeSelChanged( wxTreeEvent& In )
{
	// prevent re-entrancy
	static UBOOL InsideSelChanged = FALSE;

	if( InsideSelChanged )
	{
		return;
	}

	wxTreeItemId Item = In.GetItem();

	InsideSelChanged = TRUE;

	// set the focus to the viewport and disable mouse capture
	PreviewWindow->SetFocus();
	::SetFocus( (HWND) PhATViewportClient->Viewport->GetWindow() );

	if( Item.IsOk() )
	{
		// see if it's a body
		FPhATTreeBoneItem* b = TreeItemBodyIndexMap.Find(Item);
		if( b )
		{
			HitBone( b->BodyIndex, b->PrimType, b->PrimIndex );
		}
		else if( SelectedBodyIndex != INDEX_NONE )
		{
			SetSelectedBody(INDEX_NONE, KPT_Unknown, INDEX_NONE);
		}

		// see if it's a constraint
		INT* c = TreeItemConstraintIndexMap.Find(Item);
		if( c )
		{
			HitConstraint( *c );
		}
		else if( SelectedConstraintIndex != INDEX_NONE )
		{
			SetSelectedConstraint(INDEX_NONE);
		}

		//HitNothing();
	}

	InsideSelChanged = FALSE;
}

/** Displays a context menu for the bone tree. */
void WxPhAT::OnTreeItemRightClick( wxTreeEvent& In )
{
	if(SelectedBodyIndex != INDEX_NONE && EditingMode == PEM_BodyEdit)
	{
		// Pop up menu, if we have a body selected.
		WxBodyContextMenu Menu( this );
		FTrackPopupMenu TrackMenu( this, &Menu );
		TrackMenu.Show();
	}
	else if(SelectedConstraintIndex != INDEX_NONE && EditingMode == PEM_ConstraintEdit)
	{
		// Pop up menu, if we have a constraint selected.
		WxConstraintContextMenu Menu( this );
		FTrackPopupMenu TrackMenu( this, &Menu );
		TrackMenu.Show();
	}
}

void WxPhAT::HitBone( INT BodyIndex, EKCollisionPrimitiveType PrimType, INT PrimIndex )
{
	if(EditingMode == PEM_BodyEdit)
	{
		if(NextSelectEvent == PNS_EnableCollision)
		{
			NextSelectEvent = PNS_Normal;
			SetCollisionBetween( SelectedBodyIndex, BodyIndex, true );
		}
		else if(NextSelectEvent == PNS_DisableCollision)
		{
			NextSelectEvent = PNS_Normal;
			SetCollisionBetween( SelectedBodyIndex, BodyIndex, false );
		}
		else if(NextSelectEvent == PNS_CopyProperties)
		{
			NextSelectEvent = PNS_Normal;
			CopyBodyProperties( BodyIndex, SelectedBodyIndex );
		}
		else if(NextSelectEvent == PNS_WeldBodies)
		{
			NextSelectEvent = PNS_Normal;
			WeldBodyToSelected( BodyIndex );
		}
		else if(!bSelectionLock)
		{
			SetSelectedBody( BodyIndex, PrimType, PrimIndex );
		}
	}
}

void WxPhAT::HitConstraint( INT ConstraintIndex )
{
	if(EditingMode == PEM_ConstraintEdit)
	{
		if(NextSelectEvent == PNS_CopyProperties)
		{
			NextSelectEvent = PNS_Normal;
			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("CopyConstraint")) );
				CopyConstraintProperties( ConstraintIndex, SelectedConstraintIndex );
			}
			SetSelectedConstraint(ConstraintIndex);
			PhATViewportClient->Viewport->Invalidate();
		}
		else if(!bSelectionLock)
		{
			SetSelectedConstraint( ConstraintIndex );
		}
	}
}

void WxPhAT::HitNothing()
{
	if( NextSelectEvent != PNS_Normal )
	{
		NextSelectEvent = PNS_Normal;
		PhATViewportClient->Viewport->Invalidate();
		FillTree();
	}
	else if(!bSelectionLock)
	{
		if(EditingMode == PEM_BodyEdit)
			SetSelectedBody( INDEX_NONE, KPT_Unknown, INDEX_NONE );	
		else
			SetSelectedConstraint( INDEX_NONE );
	}
}


void WxPhAT::StartManipulating(EAxis Axis, const FViewportClick& Click, const FMatrix& WorldToCamera)
{
	check(!bManipulating);

	//debugf(TEXT("START: %d"), (INT)Axis);

	ManipulateAxis = Axis;

	if(EditingMode == PEM_BodyEdit)
	{
		check(SelectedBodyIndex != INDEX_NONE);
		GEditor->BeginTransaction( *LocalizeUnrealEd("MoveElement") );
		PhysicsAsset->BodySetup(SelectedBodyIndex)->Modify();
	}
	else
	{
		check(SelectedConstraintIndex != INDEX_NONE);
		GEditor->BeginTransaction( *LocalizeUnrealEd("MoveConstraint") );
		PhysicsAsset->ConstraintSetup(SelectedConstraintIndex)->Modify();

		const FMatrix WParentFrame = GetSelectedConstraintWorldTM(1);
		const FMatrix WChildFrame = GetSelectedConstraintWorldTM(0);
		StartManRelConTM = WChildFrame * WParentFrame.InverseSafe();

		StartManParentConTM = PhysicsAsset->ConstraintSetup(SelectedConstraintIndex)->GetRefFrameMatrix(1);
		StartManChildConTM = PhysicsAsset->ConstraintSetup(SelectedConstraintIndex)->GetRefFrameMatrix(0);
	}

	FMatrix InvWidgetTM = WidgetTM.Inverse(); // WidgetTM is set inside WxPhAT::DrawCurrentWidget.

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
		if( Abs(Click.GetDirection() | WorldManDir) > KINDA_SMALL_NUMBER ) // If click direction and circle plane are parallel.. can't resolve.
		{
			// First, find actual position we clicking on the circle in world space.
			const FVector ClickPosition = FLinePlaneIntersection( Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection(), WidgetTM.GetOrigin(), WorldManDir );

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

void WxPhAT::UpdateManipulation(FLOAT DeltaX, FLOAT DeltaY, UBOOL bCtrlDown)
{
	// DragX/Y is total offset from start of drag.
	DragX += DeltaX;
	DragY += DeltaY;

	//debugf(TEXT("UPDATE: %f %f"), DragX, DragY);

	// Update ManipulateMatrix using DragX, DragY, MovementMode & MovementSpace

	FLOAT DragMag = (DragX * DragDirX) + (DragY * DragDirY);

	if(MovementMode == PMM_Translate)
	{
		ManipulateTranslation = PhAT_TranslateSpeed * DragMag;

		if(bSnap)
			ManipulateTranslation = EditorSimOptions->LinearSnap * appFloor(ManipulateTranslation/EditorSimOptions->LinearSnap);
		else
			ManipulateTranslation = PhAT_TranslateSpeed * appFloor(ManipulateTranslation/PhAT_TranslateSpeed);

		ManipulateMatrix = FTranslationMatrix(ManipulateDir * ManipulateTranslation);
	}
	else if(MovementMode == PMM_Rotate)
	{
		ManipulateRotation = -PhAT_RotateSpeed * DragMag;

		if(bSnap)
		{
			FLOAT AngSnapRadians = EditorSimOptions->AngularSnap * (PI/180.f);
			ManipulateRotation = AngSnapRadians * appFloor(ManipulateRotation/AngSnapRadians);
		}
		else
			ManipulateRotation = PhAT_RotateSpeed * appFloor(ManipulateRotation/PhAT_RotateSpeed);

		FQuat RotQuat(ManipulateDir, ManipulateRotation);
		ManipulateMatrix = FQuatRotationTranslationMatrix( RotQuat, FVector(0.f) );
	}
	else if(MovementMode == PMM_Scale && EditingMode == PEM_BodyEdit) // Scaling only valid for bodies.
	{
		FLOAT DeltaMag = ((DeltaX * DragDirX) + (DeltaY * DragDirY)) * PhAT_TranslateSpeed;
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

void WxPhAT::EndManipulating()
{
	if(bManipulating)
	{
		//debugf(TEXT("END"));

		bManipulating = FALSE;
		ManipulateAxis = AXIS_None;

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

		GEditor->EndTransaction();

		PhATViewportClient->Viewport->Invalidate();
	}
}

// Mouse forces

void WxPhAT::SimMousePress(FViewport* Viewport, UBOOL bConstrainRotation, FName Key)
{
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);

	FSceneViewFamilyContext ViewFamily(Viewport,PhATViewportClient->GetScene(),PhATViewportClient->ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds());
	FSceneView* View = PhATViewportClient->CalcSceneView(&ViewFamily);

	const FViewportClick Click( View, PhATViewportClient, NAME_None, IE_Released, Viewport->GetMouseX(), Viewport->GetMouseY() );

	FCheckResult Result(1.f);
	const UBOOL bHit = !PhysicsAsset->LineCheck( Result, EditorSkelComp, Click.GetOrigin(),Click.GetOrigin() + Click.GetDirection() * SimGrabCheckDistance, FVector(0), FALSE );

	if(bHit)
	{
		check(Result.Item != INDEX_NONE);
		FName BoneName = PhysicsAsset->BodySetup(Result.Item)->BoneName;

		// Right mouse is for dragging things around
		if(Key == KEY_RightMouseButton)
		{
			bManipulating = TRUE;
			DragX = 0.0f;
			DragY = 0.0f;
			SimGrabPush = 0.0f;

			// Update mouse force properties from sim options.
			MouseHandle->LinearDamping = EditorSimOptions->HandleLinearDamping;
			MouseHandle->LinearStiffness = EditorSimOptions->HandleLinearStiffness;
			MouseHandle->AngularDamping = EditorSimOptions->HandleAngularDamping;
			MouseHandle->AngularStiffness = EditorSimOptions->HandleAngularStiffness;

			// Create handle to object.
			MouseHandle->GrabComponent(EditorSkelComp, BoneName, Result.Location, bConstrainRotation);

			FMatrix	InvViewMatrix = View->ViewMatrix.Inverse();

			SimGrabMinPush = SimMinHoldDistance - (Result.Time * SimGrabCheckDistance);

			SimGrabLocation = Result.Location;
			SimGrabX = InvViewMatrix.GetAxis(0).SafeNormal();
			SimGrabY = InvViewMatrix.GetAxis(1).SafeNormal();
			SimGrabZ = InvViewMatrix.GetAxis(2).SafeNormal();
		}
		// Left mouse is for poking things
		else if(Key == KEY_LeftMouseButton)
		{
			check(EditorSkelComp->PhysicsAssetInstance);

			// Ensure that we are not fixed before adding an impulse.
			if(EditorSkelComp->bSkelCompFixed)
			{
				EditorSkelComp->SetComponentRBFixed(FALSE);
			}

			EditorSkelComp->AddImpulse( Click.GetDirection() * EditorSimOptions->PokeStrength, Result.Location, BoneName );

			LastPokeTime = TotalTickTime;
		}
	}
}

void WxPhAT::SimMouseMove(FLOAT DeltaX, FLOAT DeltaY)
{
	DragX += DeltaX;
	DragY += DeltaY;

	if(!MouseHandle->GrabbedComponent)
	{
		return;
	}

	FVector NewLocation = SimGrabLocation + (SimGrabPush * SimGrabZ) + (DragX * SimGrabMoveSpeed * SimGrabX) + (DragY * SimGrabMoveSpeed * SimGrabY);

	MouseHandle->SetLocation(NewLocation);
	MouseHandle->GrabbedComponent->WakeRigidBody(MouseHandle->GrabbedBoneName);
}

void WxPhAT::SimMouseRelease()
{
	bManipulating = FALSE;
	ManipulateAxis = AXIS_None;

	if(!MouseHandle->GrabbedComponent)
	{
		return;
	}

	MouseHandle->GrabbedComponent->WakeRigidBody(MouseHandle->GrabbedBoneName);
	MouseHandle->ReleaseComponent();
}

void WxPhAT::SimMouseWheelUp()
{
	if(!MouseHandle->GrabbedComponent)
	{
		return;
	}

	SimGrabPush += SimHoldDistanceChangeDelta;

	SimMouseMove(0.0f, 0.0f);
}

void WxPhAT::SimMouseWheelDown()
{
	if(!MouseHandle->GrabbedComponent)
	{
		return;
	}

	SimGrabPush -= SimHoldDistanceChangeDelta;
	SimGrabPush = Max(SimGrabMinPush, SimGrabPush); 

	SimMouseMove(0.0f, 0.0f);
}

// Simulation

void WxPhAT::ToggleSimulation()
{
	// don't start simulation if there are no bodies or if we are manipulating a body
	if( PhysicsAsset->BodySetup.Num() == 0 || bManipulating )
	{
		return;  
	}

	bRunningSimulation = !bRunningSimulation;

	if(bRunningSimulation)
	{
		NextSelectEvent = PNS_Normal;

		// Reset last poke time.
		LastPokeTime = -100000.f;

		// Disable all the editing buttons
		UpdateToolBarStatus();

		// Let you press the anim play button.
		ToolBar->AnimPlayButton->Enable(TRUE);
		ToolBar->AnimCombo->Enable(TRUE);

		// Reassign selected animation.
		AnimComboSelected();

		// Change the button icon to a stop icon
		ToolBar->SimButton->SetBitmapLabel(ToolBar->StopSimB);

		// Flush geometry cache inside the asset (don't want to use cached version of old geometry!)
		PhysicsAsset->ClearShapeCaches();

		// We should not already have an instance (destroyed when stopping sim).
		check(!EditorSkelComp->PhysicsAssetInstance);
		EditorSkelComp->PhysicsAssetInstance = ConstructObject<UPhysicsAssetInstance>(UPhysicsAssetInstance::StaticClass(), EditorSkelComp, NAME_None, RF_Public|RF_Transactional, PhysicsAsset->DefaultInstance);
		EditorSkelComp->PhysicsAssetInstance->CollisionDisableTable = PhysicsAsset->DefaultInstance->CollisionDisableTable;

		EditorSkelComp->PhysicsAssetInstance->InitInstance(EditorSkelComp, PhysicsAsset, FALSE, RBPhysScene);

		// Make it start simulating
		EditorSkelComp->WakeRigidBody();

		// Set the properties window to point at the simulation options object.
		//PropertyWindow->SetObject(PhysicsAsset, true, false, true);
		PropertyWindow->SetObject(EditorSimOptions, EPropertyWindowFlags::ShouldShowCategories);

		// empty the tree
		TreeCtrl->DeleteAllItems();
	}
	else
	{
		// Stop any animation and clear node when stopping simulation.
		SetAnimPlayback(FALSE);
		EditorSeqNode->SetAnim(NAME_None);

		// Turn off/remove the physics instance for this thing, and move back to start location.
		check(EditorSkelComp->PhysicsAssetInstance);
		UBOOL bTerminated = EditorSkelComp->PhysicsAssetInstance->TermInstance(RBPhysScene);
		check(bTerminated);
		EditorSkelComp->PhysicsAssetInstance = NULL;

		// Force an update of the skeletal mesh to get it back to ref pose
		EditorSkelComp->UpdateSkelPose();
		EditorSkelComp->ConditionalUpdateTransform();
		PhATViewportClient->Viewport->Invalidate();

		// Enable all the buttons again
		UpdateToolBarStatus();

		// Change the button icon to a play icon again
		ToolBar->SimButton->SetBitmapLabel(ToolBar->StartSimB);

		// Fill the tree
		FillTree();

		// Put properties window back to selected.
		if(EditingMode == PEM_BodyEdit)
			SetSelectedBody(SelectedBodyIndex, SelectedPrimitiveType, SelectedPrimitiveIndex);
		else
			SetSelectedConstraint(SelectedConstraintIndex);

		// Stop you from pressing the anim playback button
		ToolBar->AnimPlayButton->Enable(FALSE);
		ToolBar->AnimCombo->Enable(FALSE);
	}
}


void WxPhAT::ViewContactsToggle()
{
	// This moves the dependency on novodex files to the engine module

	// turn on / off the debug visualizations by calling the same NXVIS
	// params as this exec toggles the state
	GWorld->Exec( TEXT("NXVIS CONTACTPOINT") );
	GWorld->Exec( TEXT("NXVIS CONTACTNORMAL") );
	GWorld->Exec( TEXT("NXVIS CONTACTERROR") );
	GWorld->Exec( TEXT("NXVIS CONTACTFORCE") );

}



// Create/Delete Primitives

void WxPhAT::AddNewPrimitive(EKCollisionPrimitiveType InPrimitiveType, UBOOL bCopySelected)
{
	if(SelectedBodyIndex == INDEX_NONE)
		return;

	URB_BodySetup* bs = PhysicsAsset->BodySetup(SelectedBodyIndex);

	EKCollisionPrimitiveType PrimitiveType;
	if(bCopySelected)
	{
		PrimitiveType = SelectedPrimitiveType;
	}
	else
	{
		PrimitiveType = InPrimitiveType;
	}

	INT NewPrimIndex = 0;
	{
		// Make sure rendering is done - so we are not changing data being used by collision drawing.
		FlushRenderingCommands();

		const FScopedTransaction Transaction( *LocalizeUnrealEd("AddNewPrimitive") );
		bs->Modify();

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
	} // ScopedTransaction

	FillTree();

	// Select the new primitive. Will call UpdateViewport.
	SetSelectedBody(SelectedBodyIndex, PrimitiveType, NewPrimIndex);


	SelectedPrimitiveType = PrimitiveType;
	SelectedPrimitiveIndex = NewPrimIndex;

	PhATViewportClient->Viewport->Invalidate();
}

void WxPhAT::DeleteBody(INT DelBodyIndex)
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd("DeleteBody") );

	// The physics asset and default instance..
	PhysicsAsset->Modify();
	PhysicsAsset->DefaultInstance->Modify();

	// .. the body..
	PhysicsAsset->BodySetup(DelBodyIndex)->Modify();
	PhysicsAsset->DefaultInstance->Bodies(DelBodyIndex)->Modify();		

	// .. and any constraints to the body.
	TArray<INT>	Constraints;
	PhysicsAsset->BodyFindConstraints(DelBodyIndex, Constraints);

	for(INT i=0; i<Constraints.Num(); i++)
	{
		INT ConstraintIndex = Constraints(i);
		PhysicsAsset->ConstraintSetup(ConstraintIndex)->Modify();
		PhysicsAsset->DefaultInstance->Constraints(ConstraintIndex)->Modify();
	}

	// Now actually destroy body. This will destroy any constraints associated with the body as well.
	PhysicsAsset->DestroyBody(DelBodyIndex);

	// Select nothing.
	SetSelectedBody(INDEX_NONE, KPT_Unknown, INDEX_NONE);
	SetSelectedConstraint(INDEX_NONE);
	FillTree();
}

void WxPhAT::DeleteCurrentPrim()
{
	if(bRunningSimulation)
		return;

	if(SelectedBodyIndex == INDEX_NONE)
		return;

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	URB_BodySetup* bs = PhysicsAsset->BodySetup(SelectedBodyIndex);

	// If only one body, and this is the last element of this body - disallow deletion.
	if(PhysicsAsset->BodySetup.Num() == 1 && bs->AggGeom.GetElementCount() == 1)
	{
		appMsgf(AMT_OK, TEXT("%s"), *LocalizeUnrealEd("MustBeAtLeastOneCollisionBody"));
		return;
	}

	// If this bone has no more geometry - remove it totally.
	if( bs->AggGeom.GetElementCount() == 1 )
	{
		UBOOL bDoDelete = true;

		if(EditorSimOptions->bPromptOnBoneDelete)
		{
			bDoDelete = appMsgf(AMT_YesNo, TEXT("%s"), *LocalizeUnrealEd("Prompt_15"));

			if(!bDoDelete)
			{
				return;
			}
		}

		DeleteBody(SelectedBodyIndex);

		return;
	}

	const FScopedTransaction Transaction( *LocalizeUnrealEd("DeletePrimitive") );
	bs->Modify();

	if(SelectedPrimitiveType == KPT_Sphere)
	{
		bs->AggGeom.SphereElems.Remove(SelectedPrimitiveIndex);
	}
	else if(SelectedPrimitiveType == KPT_Box)
	{
		bs->AggGeom.BoxElems.Remove(SelectedPrimitiveIndex);
	}
	else if(SelectedPrimitiveType == KPT_Sphyl)
	{
		bs->AggGeom.SphylElems.Remove(SelectedPrimitiveIndex);
	}
	else if(SelectedPrimitiveType == KPT_Convex)
	{
		bs->AggGeom.ConvexElems.Remove(SelectedPrimitiveIndex);
	}

	FillTree();
	SetSelectedBodyAnyPrim(SelectedBodyIndex); // Will call UpdateViewport
}

void WxPhAT::ModifyPrimitiveSize(INT BodyIndex, EKCollisionPrimitiveType PrimType, INT PrimIndex, FVector DeltaSize)
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
FMatrix WxPhAT::GetPrimitiveMatrix(FMatrix& BoneTM, INT BodyIndex, EKCollisionPrimitiveType PrimType, INT PrimIndex, FLOAT Scale)
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

FMatrix WxPhAT::GetConstraintMatrix(INT ConstraintIndex, INT BodyIndex, FLOAT Scale)
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

	// If we couldn't find the bone - fall back to identity.
	if(BoneIndex == INDEX_NONE)
	{
		return FMatrix::Identity;
	}
	else
	{
		FMatrix BoneTM = EditorSkelComp->GetBoneMatrix(BoneIndex);
		BoneTM.RemoveScaling();

		LFrame.ScaleTranslation(Scale3D);

		return LFrame * BoneTM;
	}
}

void WxPhAT::CreateOrConvertConstraint(URB_ConstraintSetup* NewSetup)
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

    FillTree();
	SetSelectedConstraint(SelectedConstraintIndex); // Push new properties into property window.
	PhATViewportClient->Viewport->Invalidate();
}

void WxPhAT::DeleteCurrentConstraint()
{
	if(EditingMode != PEM_ConstraintEdit || SelectedConstraintIndex == INDEX_NONE)
		return;

	PhysicsAsset->DestroyConstraint(SelectedConstraintIndex);
    FillTree();
	SetSelectedConstraint(INDEX_NONE);

	PhATViewportClient->Viewport->Invalidate();
}

/** Utility for getting indices of all constraints below (and including) the one with the supplied name. */
void WxPhAT::GetConstraintIndicesBelow(TArray<INT>& OutConstraintIndices, FName InBoneName)
{
	INT BaseIndex = EditorSkelMesh->MatchRefBone(InBoneName);

	// Iterate over all other joints, looking for 'children' of this one
	for(INT i=0; i<PhysicsAsset->ConstraintSetup.Num(); i++)
	{
		URB_ConstraintSetup* CS = PhysicsAsset->ConstraintSetup(i);
		FName TestName = CS->JointName;
		INT TestIndex = EditorSkelMesh->MatchRefBone(TestName);

		// We want to return this constraint as well.
		if(TestIndex == BaseIndex || EditorSkelMesh->BoneIsChildOf(TestIndex, BaseIndex))
		{
			OutConstraintIndices.AddItem(i);
		}
	}
}

/** Toggle the swing/twist drive on the selected constraint. */
void WxPhAT::ToggleSelectedConstraintMotorised()
{
	if(SelectedConstraintIndex != INDEX_NONE)
	{
		URB_ConstraintInstance* CI = PhysicsAsset->DefaultInstance->Constraints(SelectedConstraintIndex);

		if(CI->bTwistPositionDrive && CI->bSwingPositionDrive)
		{
			CI->bTwistPositionDrive = FALSE;
			CI->bSwingPositionDrive = FALSE;
		}
		else
		{
			CI->bTwistPositionDrive = TRUE;
			CI->bSwingPositionDrive = TRUE;
		}
	}
}

/** Set twist/swing drive on the selected constraint, and all below if in the hierarchy. */
void WxPhAT::SetConstraintsBelowSelectedMotorised(UBOOL bMotorised)
{
	if(SelectedConstraintIndex != INDEX_NONE)
	{
		// Get the index of this constraint
		URB_ConstraintSetup* BaseSetup = PhysicsAsset->ConstraintSetup(SelectedConstraintIndex);

		TArray<INT> BelowConstraints;
		GetConstraintIndicesBelow(BelowConstraints, BaseSetup->JointName);

		for(INT i=0; i<BelowConstraints.Num(); i++)
		{
			INT ConIndex = BelowConstraints(i);
			URB_ConstraintInstance* CI = PhysicsAsset->DefaultInstance->Constraints(ConIndex);

			if(bMotorised)
			{
				CI->bTwistPositionDrive = TRUE;
				CI->bSwingPositionDrive = TRUE;
			}
			else
			{
				CI->bTwistPositionDrive = FALSE;
				CI->bSwingPositionDrive = FALSE;
			}
		}
	}
}

/** Toggle the bFixed flag on the selected body. */
void WxPhAT::ToggleSelectedBodyFixed()
{
	if(SelectedBodyIndex != INDEX_NONE)
	{
		URB_BodySetup* BS = PhysicsAsset->BodySetup(SelectedBodyIndex);

		BS->bFixed = !BS->bFixed;
	}
}

/** Set the bFixed flag on the selected body, and all below it in the hierarchy. */
void WxPhAT::SetBodiesBelowSelectedFixed(UBOOL bFix)
{
	if(SelectedBodyIndex != INDEX_NONE)
	{
		// Get the index of this body
		URB_BodySetup* BaseSetup = PhysicsAsset->BodySetup(SelectedBodyIndex);

		TArray<INT> BelowBodies;
		PhysicsAsset->GetBodyIndicesBelow(BelowBodies, BaseSetup->BoneName, EditorSkelMesh);

		for(INT i=0; i<BelowBodies.Num(); i++)
		{
			INT BodyIndex = BelowBodies(i);
			URB_BodySetup* BS = PhysicsAsset->BodySetup(BodyIndex);

			BS->bFixed = bFix;
		}
	}
}

/** Delete all the bodies below the selected one */
void WxPhAT::DeleteBodiesBelowSelected()
{
	if(SelectedBodyIndex != INDEX_NONE)
	{
		// Build a list of BodySetups below this one
		URB_BodySetup* BaseSetup = PhysicsAsset->BodySetup(SelectedBodyIndex);

		TArray<INT> BelowBodies;
		PhysicsAsset->GetBodyIndicesBelow(BelowBodies, BaseSetup->BoneName, EditorSkelMesh);

		TArray<URB_BodySetup*> BelowBodySetups;
		for(INT i=0; i<BelowBodies.Num(); i++)
		{
			INT BodyIndex = BelowBodies(i);
			URB_BodySetup* BS = PhysicsAsset->BodySetup(BodyIndex);

			BelowBodySetups.AddItem(BS);
		}

		// Now remove each one
		for(INT i=0; i<BelowBodySetups.Num(); i++)
		{
			URB_BodySetup* BS = BelowBodySetups(i);

			// DeleteBody function takes index, so we have to find that
			INT BodyIndex = PhysicsAsset->FindBodyIndex(BS->BoneName);
			if(BodyIndex != INDEX_NONE)
			{
				// Use PhAT function to delete action (so undo works etc)
				DeleteBody(BodyIndex);
			}
		}
	}
}


////////////////////// Animation //////////////////////

/** Called when someone changes the combo - get its new contents and set the animation on the mesh. */
void WxPhAT::AnimComboSelected()
{
	FString AnimName = (const TCHAR*)ToolBar->AnimCombo->GetStringSelection();

	if(AnimName == FString(TEXT("-None-")))
	{
		EditorSeqNode->SetAnim( NAME_None );
	}
	else
	{
		EditorSeqNode->SetAnim( FName( *AnimName ) );
	}
}

/** Toggle animation playing state */
void WxPhAT::ToggleAnimPlayback()
{
	if(EditorSeqNode->bPlaying)
	{
		SetAnimPlayback(FALSE);
	}
	else
	{
		SetAnimPlayback(TRUE);
	}
}

/** Turning animation playback on and off */
void WxPhAT::SetAnimPlayback(UBOOL bPlayAnim)
{
	// Only allow play animation if running sim.
	if(bPlayAnim && bRunningSimulation)
	{
		EditorSeqNode->PlayAnim(TRUE);
		ToolBar->AnimPlayButton->SetBitmapLabel(ToolBar->StopSimB);
	}
	else
	{
		EditorSeqNode->StopAnim();
		ToolBar->AnimPlayButton->SetBitmapLabel(ToolBar->StartSimB);
	}
}

/** Regenerate the contents of the animation combo. Doesn't actually change any animation state. */
void WxPhAT::UpdateAnimCombo()
{
	ToolBar->AnimCombo->Freeze();
	ToolBar->AnimCombo->Clear();

	// Put 'none' entry at the top.
	ToolBar->AnimCombo->Append( TEXT("-None-") );

	// If we have a set, iterate over it adding names to the combo box
	if(EditorSimOptions->PreviewAnimSet)
	{
		for(INT i=0; i<EditorSimOptions->PreviewAnimSet->Sequences.Num(); i++)
		{
			ToolBar->AnimCombo->Append( *(EditorSimOptions->PreviewAnimSet->Sequences(i)->SequenceName.ToString()) );
		}
	}

	// Select the top (none) entry
	ToolBar->AnimCombo->SetSelection(0);
	ToolBar->AnimCombo->Thaw();
}

/** Handle pressing the 'toggle animation skeleton' button, for drawing results of animation before physics. */
void WxPhAT::ToggleShowAnimSkel()
{
	bShowAnimSkel = !bShowAnimSkel;
	ToolBar->ToggleTool( IDMN_PHAT_SHOWANIMSKEL, bShowAnimSkel == TRUE );
	PhATViewportClient->Viewport->Invalidate();
}

/** Update physics/anim blend weight. */
void WxPhAT::UpdatePhysBlend()
{
	// Blend between all physics (just after poke) to all animation.
	if(EditorSimOptions->bBlendOnPoke)
	{
		FLOAT TimeSincePoke = TotalTickTime - LastPokeTime;

		// Within pause time - fully physics
		if(TimeSincePoke < EditorSimOptions->PokePauseTime)
		{
			EditorSkelComp->PhysicsWeight = 1.f;

			if(EditorSkelComp->bSkelCompFixed)
			{
				EditorSkelComp->SetComponentRBFixed(FALSE);
			}
		}
		// Blending between physics and animation
		else if(TimeSincePoke < (EditorSimOptions->PokePauseTime + EditorSimOptions->PokeBlendTime))
		{
			FLOAT Alpha = (TimeSincePoke - EditorSimOptions->PokePauseTime)/EditorSimOptions->PokeBlendTime;
			Alpha = ::Clamp(Alpha, 0.f, 1.f);
			EditorSkelComp->PhysicsWeight = 1.f - Alpha;

			if(EditorSkelComp->bSkelCompFixed)
			{
				EditorSkelComp->SetComponentRBFixed(FALSE);
			}
		}
		// All animation.
		else
		{
			EditorSkelComp->PhysicsWeight = 0.f;

			if(!EditorSkelComp->bSkelCompFixed)
			{
				EditorSkelComp->SetComponentRBFixed(TRUE);
			}
		}
	}
	// If not blending on poke, just update directly.
	else
	{
		EditorSkelComp->PhysicsWeight = EditorSimOptions->PhysicsBlend;
	}
}
