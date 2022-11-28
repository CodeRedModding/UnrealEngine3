/*=============================================================================
	UnSkeletalComponent.cpp: Actor component implementation.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
		* Vectorized versions of CacheVertices by Daniel Vogel
		* Optimizations by Bruce Dawson
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "UnSkeletalRender.h"

IMPLEMENT_CLASS(USkeletalMeshComponent);
IMPLEMENT_CLASS(ASkeletalMeshActor);


/*-----------------------------------------------------------------------------
	USkeletalMeshComponent.
-----------------------------------------------------------------------------*/

void USkeletalMeshComponent::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
}

//
//	USkeletalMeshComponent::Destroy
//

void USkeletalMeshComponent::Destroy()
{
	TermArticulated();

	DeleteAnimTree();

	Super::Destroy();
}

void USkeletalMeshComponent::DeleteAnimTree()
{
	// Destroy the tree, including all children.
	if(Animations)
	{
		// Set Animations to NULL now to avoid re-entrance.
		UAnimNode* TreeToDestroy = Animations;
		Animations = NULL;

		TreeToDestroy->DestroyChildren();

		TreeToDestroy->ConditionalDestroy();
	}
}

//
//	USkeletalMeshComponent::Created
//

void USkeletalMeshComponent::Created()
{
	Super::Created();

	// If there is no anim tree for this SkeletalMeshComponent, but we do have a template
	if(!Animations && AnimTreeTemplate)
	{
		Animations = ConstructObject<UAnimTree>(UAnimTree::StaticClass(), this, NAME_None, 0, AnimTreeTemplate);
	}

	if(Animations)
	{
		Animations->InitAnim(this, NULL);
	}

	MeshObject = new FSkeletalMeshObject(this);
	UpdateSpaceBases();
	Update();
}

//
//	USkeletalMeshComponent::Update
//

void USkeletalMeshComponent::Update()
{
	Super::Update();

	for(UINT AttachmentIndex = 0;AttachmentIndex < (UINT)Attachments.Num();AttachmentIndex++)
	{
		FAttachment&	Attachment = Attachments(AttachmentIndex);
		INT				BoneIndex = MatchRefBone(Attachments(AttachmentIndex).BoneName);

		if(Attachment.Component && BoneIndex != INDEX_NONE)
		{
			Attachment.Component->SetParentToWorld( 
				(Attachment.RelativeScale != FVector(0,0,0) ? FScaleMatrix(Attachment.RelativeScale) : FMatrix::Identity) *
				FMatrix(Attachment.RelativeLocation,Attachment.RelativeRotation) *
				SpaceBases(BoneIndex) * 
				LocalToWorld 
				);

			if(!Attachment.Component->Initialized)
			{
				Attachment.Component->Scene = Scene;
				Attachment.Component->Owner = Owner;
				if(Attachment.Component->IsValidComponent())
					Attachment.Component->Created();
			}
			else
			{
				Attachment.Component->Update();
			}
		}
	}

	if(Owner)
	{
		for(INT i=0; i<Owner->Attached.Num(); i++)
		{
			AActor* Other = Owner->Attached(i);
			if(Other && Other->BaseSkelComponent == this)
			{
				INT BoneIndex = MatchRefBone(Other->BaseBoneName);
				if(BoneIndex != INDEX_NONE)
				{
					FMatrix BaseTM = GetBoneMatrix(BoneIndex);
					BaseTM.RemoveScaling();

					FMatrix HardRelMatrix(Other->RelativeLocation, Other->RelativeRotation);

					const FMatrix& NewWorldTM = HardRelMatrix * BaseTM;

					FVector NewWorldPos = NewWorldTM.GetOrigin();
					FRotator NewWorldRot = NewWorldTM.Rotator();

					FCheckResult Hit(1.f);
					Other->GetLevel()->MoveActor( Other, NewWorldPos - Other->Location, NewWorldRot, Hit, 0, 0, 1 );
				}
				else
				{
					debugf( TEXT("USkeletalMeshComponent::Update : BaseBoneName (%s) not found!"), *(Other->BaseBoneName) );
				}
				
			}
		}
	}

	MeshObject->UpdateTransforms();
}

//
//	USkeletalMeshComponent::Destroyed
//

void USkeletalMeshComponent::Destroyed()
{
	Super::Destroyed();

	for(UINT AttachmentIndex = 0;AttachmentIndex < (UINT)Attachments.Num();AttachmentIndex++)
	{
		if(Attachments(AttachmentIndex).Component)
		{
			if(Attachments(AttachmentIndex).Component->Initialized)
			{
				Attachments(AttachmentIndex).Component->Destroyed();
			}

			Attachments(AttachmentIndex).Component->Scene = NULL;
			Attachments(AttachmentIndex).Component->Owner = NULL;
		}
	}

	delete MeshObject;
	MeshObject = NULL;
}

//
//	USkeletalMeshComponent::Tick
//

void USkeletalMeshComponent::Tick(FLOAT DeltaTime)
{
	// Tick all animation channels in our anim nodes tree.		
	//   Formerly UpdateAnimation(DeltaTime)'s functionality.
	if( MeshObject && Animations )
	{		
		check(Animations->SkelComponent == this);
		Animations->TickAnim(DeltaTime, 1.f);
	}

	UpdateSpaceBases(); // Update the mesh-space bone transforms held in SpaceBases array from animation data.

	Update();

	for(UINT AttachmentIndex = 0;AttachmentIndex < (UINT)Attachments.Num();AttachmentIndex++)
	{
		if(Attachments(AttachmentIndex).Component && Attachments(AttachmentIndex).Component->Initialized)
		{
			Attachments(AttachmentIndex).Component->Tick(DeltaTime);
		}
	}
}

//
//	USkeletalMeshComponent::Precache
//

void USkeletalMeshComponent::Precache(FPrecacheInterface* P)
{
	for(UINT LodIndex = 0;LodIndex < (UINT)SkeletalMesh->LODModels.Num();LodIndex++)
	{
		for(UINT SectionIndex = 0;SectionIndex < (UINT)SkeletalMesh->LODModels(LodIndex).Sections.Num();SectionIndex++)
		{
			FSkelMeshSection&	Section = SkeletalMesh->LODModels(LodIndex).Sections(SectionIndex);
			P->CacheResource(&MeshObject->LODs(LodIndex).VertexFactory);
			P->CacheResource(&SkeletalMesh->LODModels(LodIndex).IndexBuffer);
			if( GetMaterial(Section.MaterialIndex) )
				P->CacheResource(GetMaterial(Section.MaterialIndex)->GetMaterialInterface(0));
		}
	}
}

//
//	USkeletalMeshComponent::GetMaterial
//

UMaterialInstance* USkeletalMeshComponent::GetMaterial(INT MaterialIndex) const
{
	if(MaterialIndex < Materials.Num() && Materials(MaterialIndex))
		return Materials(MaterialIndex);
	else if(SkeletalMesh && MaterialIndex < SkeletalMesh->Materials.Num() && SkeletalMesh->Materials(MaterialIndex))
		return SkeletalMesh->Materials(MaterialIndex);
	else
		return GEngine->DefaultMaterial;
}

//
//	USkeletalMeshComponent::GetLODLevel
//

INT USkeletalMeshComponent::GetLODLevel(const FSceneContext& Context) const
{
	if(ForcedLodModel == 0)
	{
		return 0; //@todo skeletal mesh LOD: support for more than one LOD level.
	}
	else 
	{
		return Clamp<INT>(ForcedLodModel-1, 0, SkeletalMesh->LODModels.Num() - 1);
	}

}

//
//	USkeletalMeshComponent::AttachComponent
//

void USkeletalMeshComponent::AttachComponent(UActorComponent* Component,FName BoneName,FVector RelativeLocation,FRotator RelativeRotation,FVector RelativeScale)
{
	if(Component->Initialized)
	{
		debugf(
			TEXT("AttachComponent called with a component that's already been initialized by something else. Actor is %s, mesh component is %s, attach component is %s"),
			Owner ? *Owner->GetPathName() : TEXT("None"),
			*GetPathName(),
			*Component->GetPathName()
			);
		return;
	}

	{
		FComponentRecreateContext	RecreateContext(this);
		new(Attachments) FAttachment(Component,BoneName,RelativeLocation,RelativeRotation,RelativeScale);
	}
}

//
//	USkeletalMeshComponent::DetachComponent
//

void USkeletalMeshComponent::DetachComponent(UActorComponent* Component)
{
	for(UINT AttachmentIndex = 0;AttachmentIndex < (UINT)Attachments.Num();AttachmentIndex++)
	{
		if(Attachments(AttachmentIndex).Component == Component)
		{
			FComponentRecreateContext	RecreateContext(this);
			Attachments.Remove(AttachmentIndex--);
			break;
		}
	}
}

//
//	USkeletalMeshComponent::SetParentToWorld
//

void USkeletalMeshComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	if(SkeletalMesh)
	{
		Super::SetParentToWorld((bForceRawOffset ? FMatrix::Identity : FTranslationMatrix( SkeletalMesh->Origin )) * FRotationMatrix(SkeletalMesh->RotOrigin) * ParentToWorld);
	}
	else
	{
		Super::SetParentToWorld(ParentToWorld);
	}
}


UBOOL USkeletalMeshComponent::Visible(FSceneView* View)
{
	if( !(View->ShowFlags & SHOW_SkeletalMeshes) )
	{
		return false;
	}

	return Super::Visible(View);
}


/**
 * Update the SpaceBases array of component-space bone transforms.
 * This will evaulate any animation blend tree if present (or use the reference pose if not).
 * It will then blend in any physics information from a PhysicsAssetInstance based on the PhysicsWeight value.
 * Then evaluates any root bone options to apply translation to the owning Actor if desired.
 * Finally it composes all the relative transforms to calculate component-space transforms for each bone. 
 * Applying per-bone controllers is done as part of mutiplying the relative transforms down the heirarchy.
 */
void USkeletalMeshComponent::UpdateSpaceBases()
{
	FCycleCounterSection CycleCounter(GEngineStats.SkeletalAnimTime);

	// Can't do anything without a SkeletalMesh
	if(!SkeletalMesh)
	{
		return;
	}

	// Allocate transforms if not present.
	if( SpaceBases.Num() != SkeletalMesh->RefSkeleton.Num() )
	{
		SpaceBases.Empty();
		SpaceBases.Add( SkeletalMesh->RefSkeleton.Num() );
	}

	// Do nothing more if no bones in skeleton.
	if(SpaceBases.Num() == 0)
	{
		return;
	}

	// Update bones transform from animations (if present)

	TArray<FBoneAtom> Transforms;
	Transforms.Add( SpaceBases.Num() );

	if(Animations && !bForceRefpose)
	{
		Animations->GetBoneAtoms(Transforms);
	}
	else
	{
		UAnimNode::FillWithRefPose(Transforms, SkeletalMesh->RefSkeleton);
	}


	// We now have all the animations blended together and final relative transforms for each bone.
	// If we don't have or want any physics, we do nothing.

	if(PhysicsAssetInstance && PhysicsWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		TArray<FBoneAtom> PhysTransforms;
		PhysTransforms.Add( SpaceBases.Num() );

		GetPhysicsBoneAtoms(PhysTransforms);

		for(INT i=0; i<SpaceBases.Num(); i++)
		{
			Transforms(i).Blend( Transforms(i), PhysTransforms(i), PhysicsWeight );
		}
	}

	// Root bone options

	// We do no delta movement on the first time through - need an old root transform to compare to.
	if(bOldRootInitialized)
	{
		FVector DeltaTrans = LocalToWorld.TransformFVector( Transforms(0).Translation ) - OldRootLocation;

		if(RootBoneOption[0] != RBA_Translate)
		{
			DeltaTrans.X = 0.f;
		}

		if(RootBoneOption[1] != RBA_Translate)
		{
			DeltaTrans.Y = 0.f;
		}

		if(RootBoneOption[2] != RBA_Translate)
		{
			DeltaTrans.Z = 0.f;
		}

		if(!DeltaTrans.IsZero() && Owner)
		{
			FCheckResult Hit(1.f);
			Owner->GetLevel()->MoveActor( Owner, DeltaTrans, Owner->Rotation, Hit );
		}
	}

	OldRootLocation = LocalToWorld.TransformFVector( Transforms(0).Translation );
	bOldRootInitialized = true;

	// For each axis - if we are locking the origin of the skeleton to the origin of the SkeletalMeshComponet, 
	// we set the elements of the root translation to zero accordingly.
	if( (RootBoneOption[0] != RBA_Default) || (RootBoneOption[1] != RBA_Default) || (RootBoneOption[2] != RBA_Default) )
	{
		FVector WorldTranslation = LocalToWorld.TransformNormal( Transforms(0).Translation );

		if(RootBoneOption[0] != RBA_Default)
		{
			WorldTranslation.X = 0.f;
		}

		if(RootBoneOption[1] != RBA_Default)
		{
			WorldTranslation.Y = 0.f;
		}

		if(RootBoneOption[2] != RBA_Default)
		{
			WorldTranslation.Z = 0.f;
		}

		Transforms(0).Translation = LocalToWorld.InverseTransformNormal(WorldTranslation);
	}

	// If desired, discard the root rotation as well.
	if(bDiscardRootRotation)
	{
		Transforms(0).Rotation = FQuat::Identity;
	}

	// Finally multiply matrices to transform all bones heirarchly into component space.

	UAnimNode::ComposeSkeleton( Transforms, SkeletalMesh->RefSkeleton, SpaceBases, this );
}

//
//	USkeletalMeshComponent::GetLayerMask
//

DWORD USkeletalMeshComponent::GetLayerMask(const FSceneContext& Context) const
{
	check(IsValidComponent());
	DWORD	LayerMask = 0;
	for(INT MaterialIndex = 0;MaterialIndex < SkeletalMesh->Materials.Num();MaterialIndex++)
		LayerMask |= GetMaterial(MaterialIndex)->GetLayerMask();
	if(((Context.View->ShowFlags & SHOW_Bounds) && (!GIsEditor || !Owner || GSelectionTools.IsSelected( Owner ) )) || bDisplayBones)
		LayerMask |= PLM_Foreground;
	return LayerMask;
}



//
//	USkeletalMeshComponent::UpdateBounds
//

void USkeletalMeshComponent::UpdateBounds()
{
	if(PhysicsAsset)
		Bounds = FBoxSphereBounds(PhysicsAsset->CalcAABB(this));
	else if(SkeletalMesh)
		Bounds = SkeletalMesh->Bounds.TransformBy(LocalToWorld);
	else
		Super::UpdateBounds();
}

FMatrix USkeletalMeshComponent::GetBoneMatrix( DWORD BoneIdx )
{
	if( SpaceBases.Num() && BoneIdx < (DWORD)SpaceBases.Num() )
		return SpaceBases(BoneIdx) * LocalToWorld;
	else
		return FMatrix::Identity;

}

//
//	USkeletalMeshComponent::SetBoneRotation
//
void USkeletalMeshComponent::SetBoneRotation( FName BoneName, const FRotator& BoneRotation, BYTE BoneSpace )
{
	INT CtrlIndex = INDEX_NONE;

	// Look up if a controller is already set up for that bone
	if ( BoneRotationControls.Num() > 0 )
	{
		for(INT c=0; c<BoneRotationControls.Num(); c++)
		{
			if ( BoneRotationControls(c).BoneName == BoneName )
			{
				CtrlIndex = c;
				break;
			}
		}
	}

	// If no controller is found, add a new one
	if ( CtrlIndex == INDEX_NONE )
	{
		INT BoneIndex = MatchRefBone(BoneName);
		if ( BoneIndex == INDEX_NONE )
		{
			debugf( TEXT("USkeletalMeshComponent::SetBoneRotation : Could not find bone: %s"), **BoneName );
			return;
		}
		CtrlIndex = BoneRotationControls.Add();
		BoneRotationControls(CtrlIndex).BoneName	= BoneName;
		BoneRotationControls(CtrlIndex).BoneIndex	= BoneIndex;
	}

	// Update Bone Rotation Controller
	BoneRotationControls(CtrlIndex).BoneRotation	= BoneRotation;
	BoneRotationControls(CtrlIndex).BoneSpace		= BoneSpace;
}

//
//	USkeletalMeshComponent::SetBoneTranslation
//
void USkeletalMeshComponent::SetBoneTranslation(FName BoneName, const FVector& BoneTranslation)
{
	INT CtrlIndex = INDEX_NONE;

	// Look up if a controller is already set up for that bone
	if ( BoneTranslationControls.Num() > 0 )
	{
		for(INT c=0; c<BoneTranslationControls.Num(); c++)
		{
			if ( BoneTranslationControls(c).BoneName == BoneName )
			{
				CtrlIndex = c;
				break;
			}
		}
	}

	// If no controller is found, add a new one
	if ( CtrlIndex == INDEX_NONE )
	{
		INT BoneIndex = MatchRefBone(BoneName);
		if ( BoneIndex == INDEX_NONE )
		{
			debugf( TEXT("USkeletalMeshComponent::SetBoneTranslation : Could not find bone: %s"), *BoneName );
			return;
		}
		CtrlIndex = BoneTranslationControls.Add();
		BoneTranslationControls(CtrlIndex).BoneName	= BoneName;
		BoneTranslationControls(CtrlIndex).BoneIndex = BoneIndex;
	}

	// Update Bone Translation Controller
	BoneTranslationControls(CtrlIndex).BoneTranslation	= BoneTranslation;
}


/**
 * Find the index of bone by name. Looks in the current SkeletalMesh being used by this SkeletalMeshComponent.
 * 
 * @param BoneName Name of bone to look up
 * 
 * @return Index of the named bone in the current SkeletalMesh. Will return INDEX_NONE if bone not found.
 *
 * @see USkeletalMesh::MatchRefBone.
 */
INT USkeletalMeshComponent::MatchRefBone( FName BoneName)
{
	INT BoneIndex = INDEX_NONE;
	if ( BoneName != NAME_None && SkeletalMesh )
	{
		BoneIndex = SkeletalMesh->MatchRefBone( BoneName );
	}

	return BoneIndex;
}

/**
 * Sets the SkeletalMesh used by this SkeletalMeshComponent.
 * 
 * @param InSkelMesh New SkeletalMesh to use for SkeletalMeshComponent.
 */
void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkelMesh)
{	
	FComponentRecreateContext	RecreateContext(this);

	FMatrix OldSkelMatrix;
	if(SkeletalMesh)
		OldSkelMatrix =  ( bForceRawOffset ?  FMatrix::Identity : FTranslationMatrix( SkeletalMesh->Origin ) ) * FRotationMatrix(SkeletalMesh->RotOrigin);
	else
		OldSkelMatrix = FMatrix::Identity;

	SkeletalMesh = InSkelMesh;

	// Reset the animation stuff when changing mesh.
	SpaceBases.Empty();	

	if(Animations)
	{
		Animations->InitAnim(this,NULL);
	}

	// We want to just replace the bit of the transform due to the actual skeletal mesh.
	FMatrix OldOffset = OldSkelMatrix.Inverse() * LocalToWorld;

	if(SkeletalMesh)
		LocalToWorld = ( bForceRawOffset ? FMatrix::Identity : FTranslationMatrix(SkeletalMesh->Origin) )  * FRotationMatrix(SkeletalMesh->RotOrigin) * OldOffset;
	else
		LocalToWorld = OldOffset;
}

/**
 * Find a named AnimSequence from the AnimSets array in the SkeletalMeshComponent. 
 * This searches array from end to start, so specific sequence can be replaced by putting a set containing a sequence with the same name later in the array.
 * 
 * @param AnimSeqName Name of AnimSequence to look for.
 * 
 * @return Pointer to found AnimSequence. Returns NULL if could not find sequence with that name.
 */
UAnimSequence* USkeletalMeshComponent::FindAnimSequence(FName AnimSeqName)
{
	if(AnimSeqName == NAME_None)
		return NULL;

	// Work from last element in list backwards, so you can replace a specific sequence by adding a set later in the array.
	for(INT i=AnimSets.Num()-1; i>=0; i--)
	{
		if( AnimSets(i) )
		{
			UAnimSequence* FoundSeq = AnimSets(i)->FindAnimSequence(AnimSeqName);
			if(FoundSeq)
				return FoundSeq;
		}
	}

	return NULL;
}

/**
 *	Set the AnimTree, residing in a package, to use as the template for this SkeletalMeshComponent.
 *	The AnimTree that is passed in is copied and assigned to the Animations pointer in the SkeletalMeshComponent.
 *	NB. This will destroy the currently instanced AnimTree, so it important you don't have any references to it or nodes within it!
 */
void USkeletalMeshComponent::SetAnimTreeTemplate(UAnimTree* NewTemplate)
{
	if(!NewTemplate)
	{
		return;
	}

	// If there is a tree instanced at the moment - destroy it.
	if(Animations)
	{
		Animations->DestroyChildren();

		delete Animations;
		Animations = NULL;
	}

	// Copy template and assign to Animations pointer.
	Animations = ConstructObject<UAnimTree>(UAnimTree::StaticClass(), this, NAME_None, 0, NewTemplate);

	if(Animations)
	{
		// If successful, initialise the new tree.
		Animations->InitAnim(this, NULL);

		// Remember the new template
		AnimTreeTemplate = NewTemplate;
	}
	else
	{
		debugf( TEXT("Failed to instance AnimTree Template: %s"), NewTemplate->GetName() );
		AnimTreeTemplate = NULL;
	}
}

///////////////////////////////////////////////////////
// Script function implementations


/** @see USkeletalMeshComponent::FindAnimSequence */
void USkeletalMeshComponent::execFindAnimSequence( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(AnimSeqName);
	P_FINISH;

	*(UAnimSequence**)Result = FindAnimSequence( AnimSeqName );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execFindAnimSequence);



void USkeletalMeshComponent::execAttachComponent(FFrame& Stack,RESULT_DECL)
{
	P_GET_OBJECT(UActorComponent,Component);
	P_GET_NAME(BoneName);
	P_GET_VECTOR_OPTX(RelativeLocation,FVector(0,0,0));
	P_GET_ROTATOR_OPTX(RelativeRotation,FRotator(0,0,0));
	P_GET_VECTOR_OPTX(RelativeScale,FVector(1,1,1));
	P_FINISH;

	AttachComponent(Component,BoneName,RelativeLocation,RelativeRotation,RelativeScale);

}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execAttachComponent);

void USkeletalMeshComponent::execDetachComponent(FFrame& Stack,RESULT_DECL)
{
	P_GET_OBJECT(UActorComponent,Component);
	P_FINISH;

	DetachComponent(Component);

}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execDetachComponent);

void USkeletalMeshComponent::execSetSkeletalMesh( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(USkeletalMesh, NewMesh);
	P_FINISH;	

	SetSkeletalMesh( NewMesh ); 
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execSetSkeletalMesh);

//
//	USkeletalMeshComponent::execSetBoneRotation
//

void USkeletalMeshComponent::execSetBoneRotation( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(BoneName);
	P_GET_ROTATOR(BoneRotation);
	P_GET_BYTE_OPTX(BoneSpace,0);
	P_FINISH;

	SetBoneRotation( BoneName, BoneRotation, BoneSpace );
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execSetBoneRotation);

void USkeletalMeshComponent::execClearBoneRotations(FFrame &Stack, RESULT_DECL)
{
	P_FINISH;
	// empty the current list of bone rotations
	BoneRotationControls.Empty();
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execClearBoneRotations);

//
//	USkeletalMeshComponent::execDrawAnimDebug
//

void USkeletalMeshComponent::execDrawAnimDebug( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UCanvas, Canvas);
	P_FINISH;

	// Start drawing the animation tree from the top of the screen.
	if(Animations)
	{
		Animations->CalcDebugHeight();
		Animations->DrawDebug(Canvas->RenderInterface, 0.0f, Canvas->ClipY * 0.5f, 16.0f, Canvas->ClipY * 0.5f, 1.f);
	}

}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execDrawAnimDebug);


//
//	USkeletalMeshComponent::execGetBoneQuaternion
//

void USkeletalMeshComponent::execGetBoneQuaternion( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(BoneName);
	P_FINISH;

	INT BoneIndex = MatchRefBone(BoneName);
	if(BoneIndex == INDEX_NONE)
	{
		debugf( TEXT("USkeletalMeshComponent::execGetBoneQuaternion : Could not find bone: %s"), *BoneName );
		*(FQuat*)Result = FQuat::Identity;
		return;
	}

	FMatrix BoneMatrix = GetBoneMatrix(BoneIndex);
	BoneMatrix.RemoveScaling();

	*(FQuat*)Result = FQuat( BoneMatrix );

}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execGetBoneQuaternion);


//
//	USkeletalMeshComponent::execGetBoneLocation
//

void USkeletalMeshComponent::execGetBoneLocation( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(BoneName);
	P_FINISH;

	INT BoneIndex = MatchRefBone(BoneName);
	if(BoneIndex == INDEX_NONE)
	{
		debugf( TEXT("USkeletalMeshComponent::execGetBoneLocation : Could not find bone: %s"), *BoneName );
		*(FVector*)Result = FVector(0,0,0);
		return;
	}

	FMatrix BoneMatrix = GetBoneMatrix(BoneIndex);
	*(FVector*)Result = BoneMatrix.GetOrigin();

}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execGetBoneLocation);

/** Script-exposed SetAnimTreeTemplate function. */
void USkeletalMeshComponent::execSetAnimTreeTemplate( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UAnimTree, NewTemplate);
	P_FINISH;

	SetAnimTreeTemplate(NewTemplate);
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execSetAnimTreeTemplate);

