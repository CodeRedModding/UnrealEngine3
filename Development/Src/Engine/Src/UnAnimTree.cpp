/*=============================================================================
	UnAnimTree.cpp: Blend tree implementation.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"

IMPLEMENT_CLASS(UAnimNode);

IMPLEMENT_CLASS(UAnimNodeBlendBase);
IMPLEMENT_CLASS(UAnimNodeBlend);
IMPLEMENT_CLASS(UAnimNodeCrossfader);
IMPLEMENT_CLASS(UAnimNodeBlendPerBone);
IMPLEMENT_CLASS(UAnimNodeBlendList);
IMPLEMENT_CLASS(UAnimNodeBlendDirectional);
IMPLEMENT_CLASS(UAnimNodeBlendByPosture);
IMPLEMENT_CLASS(UAnimNodeBlendBySpeed);

IMPLEMENT_CLASS(UAnimTree);

static const FLOAT	AnimDebugHozSpace = 40.0f;
static const FLOAT  AnimDebugVertSpace = 20.0f;

static const FColor FullWeightColor(255,255,255,255);
static const FColor ZeroWeightColor(0,0,255,255);
static const FColor InactiveColor(128,128,128,255);


/**
 * Used when drawing in-game animation tree.
 * 
 * @param Weight Weight of Atom between 0.0 and 1.0
 * 
 * @return Color to draw atom.
 */
FColor AnimDebugColorForWeight(FLOAT Weight)
{
	FColor out;

	if(Weight < ZERO_ANIMWEIGHT_THRESH)
		out = InactiveColor;
	else
	{
		out.R = ZeroWeightColor.R + (BYTE)(Weight * (FLOAT)(FullWeightColor.R - ZeroWeightColor.R));
		out.G = ZeroWeightColor.G + (BYTE)(Weight * (FLOAT)(FullWeightColor.G - ZeroWeightColor.G));
		out.B = ZeroWeightColor.B + (BYTE)(Weight * (FLOAT)(FullWeightColor.B - ZeroWeightColor.B));
		out.A = 128;
	}

	return out;
}

///////////////////////////////////////
//////////// FBoneAtom ////////////////
///////////////////////////////////////

/** Set this atom to the weighted blend of the supplied two atoms. */
void FBoneAtom::Blend(const FBoneAtom& Atom1, const FBoneAtom& Atom2, FLOAT Alpha)
{
	Translation = Lerp(Atom1.Translation, Atom2.Translation, Alpha);
	Rotation = SlerpQuat(Atom1.Rotation, Atom2.Rotation, Alpha);
	Scale = Lerp(Atom1.Scale, Atom2.Scale, Alpha);
}

/** Print the contents of this BoneAtom to the log. */
void FBoneAtom::DebugPrint()
{
	debugf(TEXT("Rotation: %f %f %f %f"), Rotation.X, Rotation.Y, Rotation.Z, Rotation.W);
	debugf(TEXT("Translation: %f %f %f"), Translation.X, Translation.Y, Translation.Z);
	debugf(TEXT("Scale: %f %f %f"), Scale.X, Scale.Y, Scale.Z);
}

/** Convert this Atom to a transformation matrix. */
static void AtomToTransform(FMatrix& OutMatrix, FBoneAtom& InAtom)
{
	OutMatrix = FMatrix(InAtom.Translation, InAtom.Rotation);
}

///////////////////////////////////////
//////////// UAnimNode ////////////////
///////////////////////////////////////

/** 
 *	During garbage collection - we ensure the tree is cleaned up from the SkeletalMeshComponent.
 *	This way, the first AnimNode to be destroyed cleans up the whole tree.
 */
void UAnimNode::Destroy()
{
	Super::Destroy();

	if(SkelComponent && GIsGarbageCollecting)
	{
		SkelComponent->DeleteAnimTree();
	}
}

/**
 * Called when a node's weight within its parent blend goes above 0.0.
 */
void UAnimNode::OnBecomeActive()
{
	eventOnBecomeActive();
}

/**
 * Called when a node's weight within its parent blend hits 0.0.
 */
void UAnimNode::OnCeaseActive()
{
	eventOnCeaseActive();
}

/**
 * Called when a node's parent becomes active
 */
void UAnimNode::OnParentBecomeActive()
{
}

/**
 * Called when a node's parent ceases activity
 */
void UAnimNode::OnParentCeaseActive()
{
}


/**
 * Take an array of relative bone atoms (translation vector, rotation quaternion and scale vector) and turn into array of component-space bone transformation matrices.
 * It will work down heirarchy multiplying the component-space transform of the parent by the relative transform of the child.
 * This code also applies any per-bone rotators etc. as part of the composition process
 *
 * @param LocalTransforms Relative bone atom ie. transform of child bone relative to parent bone.
 * @param RefSkel Reference skeleton used to find parent of each bone. Must be same size as Local Transforms.
 * @param MeshTransforms Output set of component-space bone transformation matrices. Must be same size as Local Transforms.
 * @param meshComp SkeletalMeshComponent containing bone rotation controllers etc. to apply.
 */
void UAnimNode::ComposeSkeleton( TArray<FBoneAtom>& LocalTransforms, TArray<FMeshBone>& RefSkel, TArray<FMatrix>& MeshTransforms, USkeletalMeshComponent* meshComp )
{
	check( RefSkel.Num() == MeshTransforms.Num() );

	for(INT i=0; i<MeshTransforms.Num(); i++)
	{
		AtomToTransform( MeshTransforms(i), LocalTransforms(i) );

		if(i>0)
		{
			INT Parent = RefSkel(i).ParentIndex;
			MeshTransforms(i) = MeshTransforms(i) * MeshTransforms(Parent);
		}

		// Bone Rotation Controllers
		if ( meshComp && !meshComp->bIgnoreControllers)
		{
			for(INT c=0; c<meshComp->BoneRotationControls.Num(); c++)
			{
				if ( meshComp->BoneRotationControls(c).BoneIndex == i )
				{
					FRotator NewBoneRotation = meshComp->BoneRotationControls(c).BoneRotation;

					//debugf( TEXT("UAnimNode::ComposeSkeleton - bone:%s, BoneSpace:%i"), *(meshComp->BoneRotationControls(c).BoneName), meshComp->BoneRotationControls(c).BoneSpace );

					if ( meshComp->BoneRotationControls(c).BoneSpace == 0 ) // Relative Bone Rotation (Component Space)
					{					
						MeshTransforms(i) = FRotationMatrix(NewBoneRotation) * MeshTransforms(i);
					}
					else if ( meshComp->BoneRotationControls(c).BoneSpace == 1 ) // Relative Bone Rotation (Actor Space)
					{
						// Set up space conversion matrices
						FMatrix LocalToWorldRot = meshComp->LocalToWorld;
						LocalToWorldRot.SetOrigin( FVector(0.f) );
						LocalToWorldRot.RemoveScaling();

						FMatrix WorldToLocalRot = LocalToWorldRot.Inverse();
						
						// Original Bone Info
						FMatrix BoneRot = MeshTransforms(i);
						BoneRot.SetOrigin(0);
						FVector BoneLoc = MeshTransforms(i).GetOrigin();

						// Compose New Bone Info
						FMatrix NewBoneMat = LocalToWorldRot * BoneRot;				// convert bone rot to world
						NewBoneMat = FRotationMatrix(NewBoneRotation) * NewBoneMat;	// apply actor space relative rotation
						NewBoneMat = WorldToLocalRot * NewBoneMat;					// convert back to mesh space

						MeshTransforms(i) = NewBoneMat;
						MeshTransforms(i).SetOrigin( BoneLoc );
					}
					else if ( meshComp->BoneRotationControls(c).BoneSpace == 2 ) // Lock Bone to World Rotation
					{
						// Set up space conversion matrices
						FMatrix LocalToWorldRot = meshComp->LocalToWorld;
						LocalToWorldRot.SetOrigin( FVector(0.f) );
						LocalToWorldRot.RemoveScaling();

						FMatrix WorldToLocalRot = LocalToWorldRot.Inverse();

						// Original Bone Info
						FMatrix BoneRot = MeshTransforms(i);
						BoneRot.SetOrigin(0);
						FVector BoneLoc = MeshTransforms(i).GetOrigin();
	
						// Compose New Bone Info
						FMatrix NewBoneMat = WorldToLocalRot * FRotationMatrix(NewBoneRotation);

						MeshTransforms(i) = NewBoneMat;
						MeshTransforms(i).SetOrigin( BoneLoc );
					}

					break;
				}
			}

			for(INT c=0; c<meshComp->BoneTranslationControls.Num(); c++)
			{
				if( meshComp->BoneTranslationControls(c).BoneIndex == i )
				{
					FVector NewBoneTranslation = meshComp->BoneTranslationControls(c).BoneTranslation;

					MeshTransforms(i) = MeshTransforms(i) * FTranslationMatrix(NewBoneTranslation);
				}
			}
		}

	}
}

/**
 * Do any initialisation. Should not reset any state of the animation tree so should be safe to call multiple times.
 * However, the SkeletalMesh may have changed on the SkeletalMeshComponent, so should update any data that relates to that.
 * 
 * @param meshComp SkeletalMeshComponent to which this node of the tree belongs.
 * @param Parent Parent blend node (will be NULL for root note).
 */
void UAnimNode::InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent )
{
	ParentNode = Parent;
	SkelComponent = meshComp;

	eventOnInit();
}

/**
 * Generate a set of relative transform atoms based on the reference skeleton of a skeletal mesh.
 * 
 * @param Atoms Output array of relative bone transforms. Must be the same length as RefSkel when calling function.
 * @param RefSkel Input reference skeleton to create atoms from.
 */
void UAnimNode::FillWithRefPose( TArray<FBoneAtom>& Atoms, TArray<struct FMeshBone>& RefSkel )
{
	check( Atoms.Num() == RefSkel.Num() );

	for(INT i=0; i<Atoms.Num(); i++)
	{
		Atoms(i) = FBoneAtom( RefSkel(i).BonePos.Orientation, RefSkel(i).BonePos.Position, FVector( 1.0f, 1.0f, 1.0f ) );		
	}
}

/**
 * Get the set of bone 'atoms' (ie. transform of bone relative to parent bone) generated by the blend subtree starting at this node.
 * 
 * @param Atoms Output array of bone transforms. Must be correct size when calling function - that is one entry for each bone. Contents will be erased by function though.
 */
void UAnimNode::GetBoneAtoms( TArray<FBoneAtom>& Atoms )
{
	INT NumAtoms = SkelComponent->SkeletalMesh->RefSkeleton.Num();
	check(NumAtoms == Atoms.Num());

	FillWithRefPose(Atoms, SkelComponent->SkeletalMesh->RefSkeleton);
}

/**
 * Find all AnimNodes including and below this one in the tree. Results are arranged so that parents are before children in the array.
 * 
 * @param Nodes Output array of AnimNode pointers.
 */
void UAnimNode::GetNodes( TArray<UAnimNode*>& Nodes )
{
	Nodes.AddItem(this);
}

/**
 * For debugging. Draw this node onto the screen for viewing the animation tree in-game.
 * 
 * @param RI 2D RenderInterface to draw node onto.
 * @param ParentPosX X Position of parent node. In pixels.
 * @param ParentPosY Y Position of parent node. In pixels.
 * @param PosX X Position to draw node at. In pixels.
 * @param PosY Y Position to draw node at. In pixels.
 * @param Weight Global weight of node.
 */
void UAnimNode::DrawDebug(FRenderInterface* RI, FLOAT ParentPosX, FLOAT ParentPosY, FLOAT PosX, FLOAT PosY, FLOAT Weight)
{
	FColor NodeColor = AnimDebugColorForWeight(Weight);

	// Draw icon
	RI->DrawTile(PosX, PosY - 8, 16, 16, 0.f, 0.f, 1.f, 1.f, NodeColor, DebugIcon);

	// Draw line to parent
	RI->DrawLine2D(ParentPosX, ParentPosY+1, PosX, PosY+1, FColor(0,0,0,255));
	RI->DrawLine2D(ParentPosX, ParentPosY, PosX, PosY, NodeColor);
}

/**
 * Calculate the vertical height to allow for this node and children when drawing the debug anim tree in-game
 * 
 * @return On screen height (in pixels).
 */
FLOAT UAnimNode::CalcDebugHeight()
{
	DebugHeight = AnimDebugVertSpace;
	return DebugHeight;
}

/** Find a node whose NodeName matches InNodeName. Will search this node and all below it. */
UAnimNode* UAnimNode::FindAnimNode(FName InNodeName)
{
	TArray<UAnimNode*> Nodes;
	this->GetNodes( Nodes );

	for(INT i=0; i<Nodes.Num(); i++)
	{
		if( Nodes(i)->NodeName == InNodeName )
		{
			return Nodes(i);
		}
	}

	return NULL;
}

/** Script exposed implementation of FindAnimNode. */
void UAnimNode::execFindAnimNode( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(InNodeName);
	P_FINISH;

	UAnimNode* FoundNode = FindAnimNode(InNodeName);

	*(UAnimNode**)Result = FoundNode;
}

///////////////////////////////////////
///////// UAnimNodeBlendBase //////////
///////////////////////////////////////

/**
 * Do any initialisation. Should not reset any state of the animation tree so should be safe to call multiple times.
 * For a blend, will call InitAnim on any child nodes.
 * 
 * @param meshComp SkeletalMeshComponent to which this node of the tree belongs.
 * @param Parent Parent blend node (will be NULL for root note).
 */
void UAnimNodeBlendBase::InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent )
{
	Super::InitAnim(meshComp, Parent);

	for(INT i=0; i<Children.Num(); i++)
	{
		if( Children(i).Anim )
		{
			Children(i).Anim->InitAnim( meshComp, this );
		}
	}
}

/**
 * Do any time-dependent work in the blend, changing blend weights etc.
 * 
 * @param DeltaSeconds Size of timestep we are advancing animation tree by. In seconds.
 */
void UAnimNodeBlendBase::TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight )
{
	check(!ParentNode || ParentNode->SkelComponent == SkelComponent);

	for(INT i=0; i<Children.Num(); i++)
	{
		if( Children(i).Anim )
		{
			Children(i).Anim->TickAnim( DeltaSeconds, TotalWeight * Children(i).Weight );

			// Call blend in/out notifications.
			if((Children(i).Weight > ZERO_ANIMWEIGHT_THRESH) && !Children(i).bActive)
			{	
				Children(i).Anim->OnBecomeActive();
				Children(i).bActive = true;
			}
			else if((Children(i).Weight < ZERO_ANIMWEIGHT_THRESH) && Children(i).bActive)
			{
				Children(i).Anim->OnCeaseActive();
				Children(i).bActive = false;
			}
		}
	}
}

/**
 * Make sure to pass the OnParentBecomeActive event to all children
 */
void UAnimNodeBlendBase::OnBecomeActive()
{
	for (INT i=0; i<Children.Num(); i++)
	{	
		if( Children(i).Anim )
		{
			Children(i).Anim->OnParentBecomeActive();
		}
	}
}

/**
 * Make sure to pass the OnParentCeaseActive event to all children
 */
void UAnimNodeBlendBase::OnCeaseActive()
{
	for (INT i=0; i<Children.Num(); i++)
	{
		if( Children(i).Anim )
		{
			Children(i).Anim->OnParentCeaseActive();
		}
	}
}

/**
 * Find all AnimNodes including and below this one in the tree. Results are arranged so that parents are before children in the array.
 * For a blend node, will recursively call GetNodes on all child nodes.
 * 
 * @param Nodes Output array of AnimNode pointers.
 */
void UAnimNodeBlendBase::GetNodes( TArray<UAnimNode*>& Nodes )
{
	Super::GetNodes(Nodes);

	for(INT i=0; i<Children.Num(); i++)
	{
		if( Children(i).Anim )
		{
			Children(i).Anim->GetNodes( Nodes );
		}
	}
}

/**
 * Blends together the Children AnimNodes of this blend based on the Weight in each element of the Children array.
 * Instead of using SLERPs, the blend is done by taking a weighted sum of each atom, and renormalising the quaternion part at the end.
 * This allows n-way blends, and makes the code much faster, though the angular velocity will not be constant across the blend.
 *
 * @todo	Great scope for optimisation here (not querying children with a weight of zero!), but waiting until we know these scheme works.
 * 
 * @param	Atoms - Output array of relative bone transforms.
 */
void UAnimNodeBlendBase::GetBoneAtoms( TArray<FBoneAtom>& Atoms )
{
	// Handle case of a blend with no children.
	if( Children.Num() == 0 )
	{
		FillWithRefPose(Atoms, SkelComponent->SkeletalMesh->RefSkeleton);
		return;
	}

	// Check all children weights sum to 1.0
	if( Abs(GetChildWeightTotal() - 1.f) > ZERO_ANIMWEIGHT_THRESH )
	{
		debugf( TEXT("WARNING: AnimNodeBlendBase (%s) has Children weights which do not sum to 1.0."), GetName() );

		FLOAT TotalWeight = 0.f;
		for(INT i=0; i<Children.Num(); i++)
		{
			debugf( TEXT("Child: %d Weight: %f"), i, Children(i).Weight );

			TotalWeight += Children(i).Weight;
		}

		debugf( TEXT("Total Weight: %f"), TotalWeight );
		//@todo - adjust first node weight to 

		FillWithRefPose(Atoms, SkelComponent->SkeletalMesh->RefSkeleton);
		return;
	}

	INT NumAtoms = SkelComponent->SkeletalMesh->RefSkeleton.Num();
	check( NumAtoms == Atoms.Num() );

	// Find index of the last child with a non-zero weight.
	INT LastChildIndex = INDEX_NONE;
	for(INT i=0; i<Children.Num(); i++)
	{
		if( Children(i).Weight > ZERO_ANIMWEIGHT_THRESH )
		{
			LastChildIndex = i;
		}
	}
	check(LastChildIndex != INDEX_NONE);


	// We don't allocate this array until we need it.
	TArray<FBoneAtom> ChildAtoms;
	UBOOL bNoChildrenYet = true;

	// Iterate over each child getting its atoms, scaling them and adding them to output (Atoms array)
	for(INT i=0; i<=LastChildIndex; i++)
	{
		// If this is the only child with any weight, pass Atoms array into it directly.
		if( Children(i).Weight > (1.f - ZERO_ANIMWEIGHT_THRESH) )
		{
			if( Children(i).Anim )
			{
				Children(i).Anim->GetBoneAtoms(Atoms);
			}
			else
			{
				FillWithRefPose(Atoms, SkelComponent->SkeletalMesh->RefSkeleton);
			}

			return;
		}
		// If this child has non-zero weight, blend it into accumulator.
		else if( Children(i).Weight > ZERO_ANIMWEIGHT_THRESH )
		{
			// Do need to request atoms, so allocate array here.
			if( ChildAtoms.Num() == 0 )
			{
				ChildAtoms.Add(NumAtoms);
			}

			// Get bone atoms from child node (if no child - use ref pose).
			if(Children(i).Anim)
			{
				Children(i).Anim->GetBoneAtoms(ChildAtoms);
			}
			else
			{
				FillWithRefPose(ChildAtoms, SkelComponent->SkeletalMesh->RefSkeleton);
			}

			for(INT j=0; j<NumAtoms; j++)
			{
				// To ensure the 'shortest route', we make sure the dot product between the accumulator and the incoming child atom is positive.
				if( (Atoms(j).Rotation | ChildAtoms(j).Rotation) < 0.f )
				{
					ChildAtoms(j).Rotation = ChildAtoms(j).Rotation * -1.f;
				}

				// We just write the first childrens atoms into the output array. Avoids zero-ing it out.
				if(bNoChildrenYet)
				{
					Atoms(j) = ChildAtoms(j) * Children(i).Weight;
				}
				else
				{
					Atoms(j) += ChildAtoms(j) * Children(i).Weight;
				}

				// If last child - normalize the rotaion quaternion now.
				if(i == LastChildIndex)
				{
					Atoms(j).Rotation.Normalize();
				}
			}

			bNoChildrenYet = false;
		}
	}
}

/**
 * Draw node onto screen for in-game debugging. For blends, will call DrawDebug recursively on children.
 * 
 * @see UAnimNode::DrawDebug
 */
void UAnimNodeBlendBase::DrawDebug(FRenderInterface* RI, FLOAT ParentPosX, FLOAT ParentPosY, FLOAT PosX, FLOAT PosY, FLOAT Weight)
{
	Super::DrawDebug(RI, ParentPosX, ParentPosY, PosX, PosY, Weight);

	FLOAT CurrPos = -0.5f * DebugHeight;

	for(INT i=0; i<Children.Num(); i++)
	{
		if( Children(i).Anim )
		{
			CurrPos += 0.5f * Children(i).Anim->DebugHeight;
			Children(i).Anim->DrawDebug( RI, PosX + 16, PosY, PosX + AnimDebugHozSpace, PosY + CurrPos, Weight * Children(i).Weight );
			CurrPos += 0.5f * Children(i).Anim->DebugHeight;
		}
	}	
}

/** @see UAnimNode::CalcDebugHeight */
FLOAT UAnimNodeBlendBase::CalcDebugHeight()
{
	DebugHeight = 0;

	for(INT i=0; i<Children.Num(); i++)
	{
		if( Children(i).Anim )
		{
			DebugHeight += Children(i).Anim->CalcDebugHeight();
		}
	}

	DebugHeight = Max<FLOAT>(DebugHeight, AnimDebugVertSpace);

	return DebugHeight;
}

/** Delete all children AnimNode Objects. */
void UAnimNodeBlendBase::DestroyChildren()
{
	for(INT i=0; i<Children.Num(); i++)
	{
		if(Children(i).Anim)
		{
			// Destroy any children of this node.
			Children(i).Anim->DestroyChildren();

			// Then destroy the node itself. 
			// We call ConditionalDestroy as this doesn't actually free the child now, and is safe to call twice.
			Children(i).Anim->ConditionalDestroy();

			Children(i).Anim = NULL;
		}
	}

	Children.Empty();
}


/**
 * Find the current weight of a particular child node of this blend.
 * 
 * @param Child Child to look for in blend.
 * 
 * @return Current weight of child. Between 0.0 and 1.0. Will return 0.0 if child not found.
 */
FLOAT UAnimNodeBlendBase::GetChildWeight(UAnimNode* Child)
{
	for(INT i=0; i<Children.Num(); i++)
	{
		if(Child == Children(i).Anim)
		{
			return Children(i).Weight;
		}
	}

	return 0.f;
}


/**
 * For debugging.
 * 
 * @return Sum weight of all children weights. Should always be 1.0
 */
FLOAT UAnimNodeBlendBase::GetChildWeightTotal()
{
	FLOAT TotalWeight = 0.f;

	for(INT i=0; i<Children.Num(); i++)
	{
		TotalWeight += Children(i).Weight;
	}

	return TotalWeight;
}

/**
 * Make sure to relay OnChildAnimEnd to Parent AnimNodeBlendBase as long as it exists 
 */ 
void UAnimNodeBlendBase::OnChildAnimEnd(UAnimNodeSequence* Child) 
{ 
	if( ParentNode != NULL )  
	{ 
		ParentNode->OnChildAnimEnd( Child ); 
	} 
} 



///////////////////////////////////////
//////////// UAnimNodeBlend ///////////
///////////////////////////////////////

/** @see UAnimNode::TickAnim */
void UAnimNodeBlend::TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight )
{
	check( Children.Num() == 2 );

	FLOAT BlendDelta = Child2WeightTarget - Child2Weight; // Amount we want to change Child2Weight by.

	if( Abs(BlendDelta) > KINDA_SMALL_NUMBER && BlendTimeToGo > KINDA_SMALL_NUMBER )
	{
		if(BlendTimeToGo < DeltaSeconds)
		{
			BlendTimeToGo = DeltaSeconds; // So we don't overshoot.
		}

		Child2Weight += (BlendDelta / BlendTimeToGo) * DeltaSeconds;
		BlendTimeToGo -= DeltaSeconds;
	}

	// debugf(TEXT("Blender: %s (%x) Child2Weight: %f BlendTimeToGo: %f"), *GetPathName(), (INT)this, Child2BoneWeights.ChannelWeight, BlendTimeToGo);

	Children(0).Weight = 1.f - Child2Weight;
	Children(1).Weight = Child2Weight;

	// Tick children
	Super::TickAnim( DeltaSeconds, TotalWeight );
}

/** @see UAnimNodeBlend::SetBlendTarget. */
void UAnimNodeBlend::execSetBlendTarget( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(BlendTarget);
	P_GET_FLOAT(BlendTime);
	P_FINISH;

	SetBlendTarget(BlendTarget, BlendTime);

}

/**
 * Set desired balance of this blend.
 * 
 * @param BlendTarget Target amount of weight to put on Children(1) (second child). Between 0.0 and 1.0. 1.0 means take all animation from second child.
 * @param BlendTime How long to take to get to BlendTarget.
 */
void UAnimNodeBlend::SetBlendTarget( FLOAT BlendTarget, FLOAT BlendTime )
{
	Child2WeightTarget = BlendTarget;
	
	// If we want this weight NOW - update weights straight away (dont wait for TickAnim).
	if(BlendTime == 0.0f)
	{
		Child2Weight = BlendTarget;
	}

	BlendTimeToGo = BlendTime;
}

///////////////////////////////////////
///////////// AnimNodeCrossfader //////
///////////////////////////////////////

/** @see UAnimNode::InitAnim */
void UAnimNodeCrossfader::InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent )
{
	Super::InitAnim( meshComp, Parent );
	
	UAnimNodeSequence	*ActiveChild = GetActiveChild();
	if ( GetActiveChild()->AnimSeqName == NAME_None )
	{
		SetAnim( DefaultAnimSeqName );
	}
}

/** @see UAnimNode::TickAnim */
void UAnimNodeCrossfader::TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight )
{
	if ( !bDontBlendOutOneShot && PendingBlendOutTimeOneShot > 0.f )
	{
		UAnimNodeSequence	*ActiveChild	= GetActiveChild();
		FLOAT				fCountDown		= ActiveChild->AnimSeq->SequenceLength - ActiveChild->CurrentTime;
		
		// if playing a one shot anim, and it's time to blend back to previous animation, do so.
		if ( fCountDown <= PendingBlendOutTimeOneShot )
		{
			SetBlendTarget( 1.f - Child2WeightTarget, PendingBlendOutTimeOneShot );
			PendingBlendOutTimeOneShot = 0.f;
		}
	}

	// Tick children
	Super::TickAnim( DeltaSeconds, TotalWeight );
}

/** @see AnimCrossfader::GetActiveAnimSeq */
void UAnimNodeCrossfader::execGetActiveAnimSeq( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(FName*)Result = GetActiveChild()->AnimSeqName;
}

/** @see AnimCrossfader::GetActiveChild */
void UAnimNodeCrossfader::execGetActiveChild( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(UAnimNodeSequence**)Result = GetActiveChild();
}

/**
 * Get active AnimNodeSequence child. To access animation properties and control functions.
 *
 * @return	AnimNodeSequence currently playing.
 */
UAnimNodeSequence *UAnimNodeCrossfader::GetActiveChild()
{
	return CastChecked<UAnimNodeSequence>(Child2WeightTarget == 0.f ? Children(0).Anim : Children(1).Anim);
}

/** @see AnimNodeCrossFader::PlayOneShotAnim */
void UAnimNodeCrossfader::execPlayOneShotAnim( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(AnimSeqName);
	P_GET_FLOAT_OPTX(BlendInTime,0.f); 
	P_GET_FLOAT_OPTX(BlendOutTime,0.f);
	P_GET_UBOOL_OPTX(bDontBlendOut,false);
	P_GET_FLOAT_OPTX(Rate,1.f);
	P_FINISH;

	FLOAT				BlendTarget;
	UAnimNodeSequence	*Child;

	Child		= CastChecked<UAnimNodeSequence>(Child2WeightTarget == 0.f ? Children(1).Anim : Children(0).Anim);
	BlendTarget = Child2WeightTarget == 0.f ? 1.f : 0.f;

	bDontBlendOutOneShot = bDontBlendOut;
	PendingBlendOutTimeOneShot = BlendOutTime;

	Child->SetAnim(AnimSeqName);
	Child->PlayAnim(false, Rate);
	SetBlendTarget( BlendTarget, BlendInTime );
}

/** @see AnimNodeCrossFader::BlendToLoopingAnim */
void UAnimNodeCrossfader::execBlendToLoopingAnim( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(AnimSeqName);
	P_GET_FLOAT_OPTX(BlendInTime,0.f);
	P_GET_FLOAT_OPTX(Rate,1.f);
	P_FINISH;

	FLOAT				BlendTarget;
	UAnimNodeSequence	*Child;

	Child		= CastChecked<UAnimNodeSequence>(Child2WeightTarget == 0.f ? Children(1).Anim : Children(0).Anim);
	BlendTarget = Child2WeightTarget == 0.f ? 1.f : 0.f;

	// One shot variables..
	bDontBlendOutOneShot		= true;
	PendingBlendOutTimeOneShot	= 0.f;

	Child->SetAnim(AnimSeqName);
	Child->PlayAnim(true, Rate);
	SetBlendTarget( BlendTarget, BlendInTime );
}

///////////////////////////////////////
/////////// AnimNodeBlendPerBone //////
///////////////////////////////////////

/** @see UAnimNode::InitAnim */
void UAnimNodeBlendPerBone::InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent )
{
	Super::InitAnim(meshComp, Parent);

	if(InitChild2StartBone != NAME_None)
	{
		SetChild2StartBone( InitChild2StartBone, InitPerBoneIncrease );
	}
}

/** . Starting from the named bone,  */


/**
 * Utility for creating the Child2PerBoneWeight array. Walks down the heirarchy increasing the weight by PerBoneIncrease each step.
 * The per-bone weight is multiplied by the overall blend weight to calculate how much animation data to pull from each child.
 * 
 * @param StartBoneName Start blending in animation from Children(1) (ie second child) from this bone down.
 * @param PerBoneIncrease Amount to increase bone weight by for each bone down heirarchy.
 */
void UAnimNodeBlendPerBone::SetChild2StartBone( FName StartBoneName, FLOAT PerBoneIncrease )
{
	if(StartBoneName == NAME_None)
	{
		Child2PerBoneWeight.Empty();
	}
	else
	{
		INT StartBoneIndex = SkelComponent->MatchRefBone(StartBoneName);
		if(StartBoneIndex == INDEX_NONE)
		{
			debugf( TEXT("UAnimNodeBlendPerBone::SetChild2StartBone : StartBoneName (%s) not found."), *StartBoneName );
			return;
		}

		TArray<FMeshBone>& RefSkel = SkelComponent->SkeletalMesh->RefSkeleton;

		Child2PerBoneWeight.Empty();
		Child2PerBoneWeight.AddZeroed( RefSkel.Num() );


		check(PerBoneIncrease >= 0.0f && PerBoneIncrease <= 1.0f); // rather aggressive...

		// Allocate bone weights by walking heirarchy, increasing bone weight for bones below the start bone.

		Child2PerBoneWeight(StartBoneIndex) = PerBoneIncrease;

		for(INT i=0; i<Child2PerBoneWeight.Num(); i++)
		{
			if(i != StartBoneIndex)
			{
				FLOAT ParentWeight = Child2PerBoneWeight( RefSkel(i).ParentIndex );
				FLOAT BoneWeight = ( ParentWeight == 0.0f ) ? 0.0f: Min(ParentWeight + PerBoneIncrease, 1.0f);

				Child2PerBoneWeight(i) = BoneWeight;
			}
		}

#if 0
		for(INT i=0; i<RefSkel.Num(); i++)
		{
			debugf( TEXT("(%d) %s: %f"),  i, *RefSkel(i).Name, Child2PerBoneWeight(i) );
		}
#endif
	}
}

/** @see UAnimNodeBlendPerBone::SetChild2StartBone */
void UAnimNodeBlendPerBone::execSetChild2StartBone( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(StartBoneName);
	P_GET_FLOAT_OPTX(PerBoneIncrease, 1.0f);
	P_FINISH;
	
	SetChild2StartBone(StartBoneName, PerBoneIncrease);
}

/** @see UAnimNode::GetBoneAtoms. */
void UAnimNodeBlendPerBone::GetBoneAtoms(TArray<FBoneAtom>& Atoms)
{
	check(Children.Num() == 2);	

	// If per-bone weight array is empty we behave just like a regular 2-way blend.
	if(Child2PerBoneWeight.Num() == 0)
	{
		Super::GetBoneAtoms(Atoms);
		return;
	}

	// Check all children weights sum to 1.0
	if( Abs(GetChildWeightTotal() - 1.f) > ZERO_ANIMWEIGHT_THRESH )
	{
		debugf( TEXT("WARNING: AnimNodeBlendBase (%s) has Children weights which do not sum to 1.0.") );

		FLOAT TotalWeight = 0.f;
		for(INT i=0; i<Children.Num(); i++)
		{
			debugf( TEXT("Child: %d Weight: %f"), i, Children(i).Weight );

			TotalWeight += Children(i).Weight;
		}

		debugf( TEXT("Total Weigh: %f"), TotalWeight );

		FillWithRefPose(Atoms, SkelComponent->SkeletalMesh->RefSkeleton);
		return;
	}

	// If drawing all from Children(0), no need to look at Children(1). Pass Atoms array directly to Children(0).
	if( Children(0).Weight > (1.f - ZERO_ANIMWEIGHT_THRESH) )
	{
		if( Children(0).Anim )
		{
			Children(0).Anim->GetBoneAtoms(Atoms);
		}
		else
		{
			FillWithRefPose(Atoms, SkelComponent->SkeletalMesh->RefSkeleton);
		}

		return;
	}

	INT NumAtoms = SkelComponent->SkeletalMesh->RefSkeleton.Num();
	check( NumAtoms == Atoms.Num() );

	TArray<FBoneAtom> Child1Atoms, Child2Atoms;

	// Get bone atoms from each child (if no child - use ref pose).
	Child1Atoms.Add(NumAtoms);
	if(Children(0).Anim)
	{
		Children(0).Anim->GetBoneAtoms(Child1Atoms);
	}
	else
	{
		FillWithRefPose(Child1Atoms, SkelComponent->SkeletalMesh->RefSkeleton);
	}

	Child2Atoms.Add(NumAtoms);
	if(Children(1).Anim)
	{
		Children(1).Anim->GetBoneAtoms(Child2Atoms);
	}
	else
	{
		FillWithRefPose(Child2Atoms, SkelComponent->SkeletalMesh->RefSkeleton);
	}

	for(INT i=0; i<NumAtoms; i++)
	{
		// As in the usual GetBoneAtoms, we ensure the dot product between blended atoms rotation is positive to get 'shortest route'.
		if( (Child1Atoms(i).Rotation | Child2Atoms(i).Rotation) < 0.f )
		{
			Child2Atoms(i).Rotation = Child2Atoms(i).Rotation * -1.f;
		}

		FLOAT Child2BoneWeight = Children(1).Weight * Child2PerBoneWeight(i);

		Atoms(i) = (Child1Atoms(i) * (1.f - Child2BoneWeight)) + (Child2Atoms(i) * (Child2BoneWeight));
		Atoms(i).Rotation.Normalize();
	}
}

///////////////////////////////////////
/////// AnimNodeBlendDirectional //////
///////////////////////////////////////

static inline float FindDeltaAngle(FLOAT a1, FLOAT a2)
{
	FLOAT delta = a2 - a1;

	if(delta > PI)
		delta = delta - (PI * 2.0f);
	else if(delta < -PI)
		delta = delta + (PI * 2.0f);

	return delta;
}

static inline float UnwindHeading(FLOAT a)
{
	while(a > PI)
		a -= ((FLOAT)PI * 2.0f);

	while(a < -PI)
		a += ((FLOAT)PI * 2.0f);

	return a;
}


/**
 * Updates weight of the 4 directional animation children by looking at the current velocity and heading of actor.
 * 
 * @see UAnimNode::TickAnim
 */
void UAnimNodeBlendDirectional::TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight )
{
	check(Children.Num() == 4);

	// Calculate blend weights based on player velocity.
	
	AActor* actor = SkelComponent->Owner; // Get the actor to use for acceleration/look direction etc.

	FVector AccelDir = actor->Acceleration;
	AccelDir.Z = 0.0f;

	// If we are not actually moving, don't update the animation mix.
	if( !AccelDir.IsNearlyZero() )
	{
		AccelDir = AccelDir.SafeNormal();

		FVector LookDir = actor->Rotation.Vector();
		LookDir.Z = 0.f;
		LookDir = LookDir.SafeNormal();

		FVector LeftDir = LookDir ^ FVector(0.f,0.f,1.f);
		LeftDir = LeftDir.SafeNormal();

		FLOAT ForwardPct = (LookDir | AccelDir);
		FLOAT LeftPct = (LeftDir | AccelDir);

		FLOAT TargetDirAngle = appAcos( Clamp<FLOAT>(ForwardPct, -1.f, 1.f) );
		if(LeftPct > 0.f)
			TargetDirAngle *= -1.f;

		// Move DirAngle towards TargetDirAngle as fast as DirRadsPerSecond allows
		FLOAT DeltaDir = FindDeltaAngle(DirAngle, TargetDirAngle);
		FLOAT MaxDelta = DeltaSeconds * DirDegreesPerSecond * (PI/180.f);
		DeltaDir = Clamp<FLOAT>(DeltaDir, -MaxDelta, MaxDelta);
		DirAngle = UnwindHeading( DirAngle + DeltaDir );


		// Work out child weights based on the direction we are moving.

		if(DirAngle < -0.5f*PI) // Back and left
		{
			Children(2).Weight = (DirAngle/(0.5f*PI)) + 2.f;
			Children(3).Weight = 0.f;

			Children(0).Weight = 0.f;
			Children(1).Weight = 1.f - Children(2).Weight;
		}
		else if(DirAngle < 0.f) // Forward and left
		{
			Children(2).Weight = -DirAngle/(0.5f*PI);
			Children(3).Weight = 0.f;

			Children(0).Weight = 1.f - Children(2).Weight;
			Children(1).Weight = 0.f;
		}
		else if(DirAngle < 0.5f*PI) // Forward and right
		{
			Children(2).Weight = 0.f;
			Children(3).Weight = DirAngle/(0.5f*PI);

			Children(0).Weight = 1.f - Children(3).Weight;
			Children(1).Weight = 0.f;
		}
		else // Back and right
		{
			Children(2).Weight = 0.f;
			Children(3).Weight = (-DirAngle/(0.5f*PI)) + 2.f;

			Children(0).Weight = 0.f;
			Children(1).Weight = 1.f - Children(3).Weight;
		}

		//debugf( TEXT("DirAngle: %f LatWeight: %f"), DirAngle, LatWeight );
	}
	else
	{
		Children(2).Weight = 0.f;
		Children(3).Weight = 0.f;

		Children(0).Weight = 1.f;
		Children(1).Weight = 0.f;

		DirAngle = 0.f;
	}

	// TODO: This node could also 'lock' animation rate/phase together for its children (for cadence matching)? 
	// Should this be a general blender property?
	
	// Tick children
	Super::TickAnim(DeltaSeconds, TotalWeight);
}


///////////////////////////////////////
/////////// AnimNodeBlendList /////////
///////////////////////////////////////

/**
 * Will ensure TargetWeight array is right size. If it has to resize it, will set Chidlren(0) to have a target of 1.0.
 * Also, if all Children weights are zero, will set Children(0) as the active child.
 * 
 * @see UAnimNode::InitAnim
 */
void UAnimNodeBlendList::InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent )
{
	Super::InitAnim(meshComp, Parent);

	if( TargetWeight.Num() != Children.Num() )
	{
		TargetWeight.Empty();
		TargetWeight.AddZeroed( Children.Num() );

		if(TargetWeight.Num() > 0)
		{
			TargetWeight(0) = 1.f;
		}
	}

	// If all child weights are zero - set the first one to the active child.
	FLOAT ChildWeightSum = GetChildWeightTotal();
	if( ChildWeightSum < ZERO_ANIMWEIGHT_THRESH )
	{
		SetActiveChild(ActiveChildIndex, 0.f);
	}
}

/** @see UAnimNode::TickAnim */
void UAnimNodeBlendList::TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight )
{
	check(Children.Num() == TargetWeight.Num());

	// Do nothing if BlendTimeToGo is zero.
	if(BlendTimeToGo > KINDA_SMALL_NUMBER)
	{
		if(BlendTimeToGo < DeltaSeconds)
		{
			BlendTimeToGo = DeltaSeconds; // So we don't overshoot.
		}

		for(INT i=0; i<Children.Num(); i++)
		{
			FLOAT BlendDelta = TargetWeight(i) - Children(i).Weight; // Amount left to blend.

			Children(i).Weight += (BlendDelta / BlendTimeToGo) * DeltaSeconds;
		}

		BlendTimeToGo -= DeltaSeconds;
	}
	else
	{
		BlendTimeToGo = 0.f;
	}

	// Tick child nodes
	Super::TickAnim(DeltaSeconds, TotalWeight);
}

/**
 * The the child to blend up to full Weight (1.0)
 * 
 * @param ChildIndex Index of child node to ramp in. If outside range of children, will set child 0 to active.
 * @param BlendTime How long for child to take to reach weight of 1.0.
 */
void UAnimNodeBlendList::SetActiveChild( INT ChildIndex, FLOAT BlendTime )
{
	check(Children.Num() == TargetWeight.Num());
	
	if( ChildIndex < 0 || ChildIndex >= Children.Num() )
	{
		debugf( TEXT("UAnimNodeBlendList::SetActiveChild : %s ChildIndex (%d) outside number of Children (%d)."), GetName(), ChildIndex, Children.Num() );
		ChildIndex = 0;
	}

	for(INT i=0; i<Children.Num(); i++)
	{
		if(i == ChildIndex)
		{
			TargetWeight(i) = 1.0f;

			// If we basically want this straight away - dont wait until a tick to update weights.
			if(BlendTime == 0.0f)
			{
				Children(i).Weight = 1.0f;
			}
		}
		else
		{
			TargetWeight(i) = 0.0f;

			if(BlendTime == 0.0f)
			{
				Children(i).Weight = 0.0f;
			}
		}
	}

	BlendTimeToGo = BlendTime;
	ActiveChildIndex = ChildIndex;
}

/** @see SetActiveChild. */
void UAnimNodeBlendList::execSetActiveChild( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(ChildIndex);
	P_GET_FLOAT(BlendTime);
	P_FINISH;

	SetActiveChild(ChildIndex, BlendTime);
}

/**
 * Blend animations based on an Owner's posture.
 *
 * @param DeltaSeconds	Time since last tick in seconds.
 */
void UAnimNodeBlendByPosture::TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight )
{
	APawn* Owner = SkelComponent ? Cast<APawn>(SkelComponent->Owner) : NULL;

	if ( Owner )
	{
		if ( Owner->bIsCrouched )
		{
			if ( ActiveChildIndex != 1 )
			{
				SetActiveChild( 1, 0.1f );
			}
		}
		else if ( ActiveChildIndex != 0 )
		{
			SetActiveChild( 0 , 0.1f );
		}
	}

	Super::TickAnim(DeltaSeconds, TotalWeight);
}

/**
 * Resets the last channel on becoming active.	
 */
void UAnimNodeBlendBySpeed::OnBecomeActive()
{
	Super::OnBecomeActive();
	LastChannel = -1;
}

/**
 * Blend animations based on an Owner's velocity.
 *
 * @param DeltaSeconds	Time since last tick in seconds.
 */
void UAnimNodeBlendBySpeed::TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight )
{
	AActor* Owner					= SkelComponent ? SkelComponent->Owner : NULL;
	INT		NumChannels				= Children.Num();
	UBOOL	SufficientChannels		= NumChannels >= 2,
			SufficientConstraints	= Constraints.Num() >= NumChannels;

	if( Owner && SufficientChannels && SufficientConstraints )
	{
		INT		TargetChannel	= 0;
		FLOAT	Speed			= Owner->Velocity.Size();
		LastSpeed				= Speed;

		// Find appropriate channel for current speed with "Constraints" containing an upper speed bound.
		while( (TargetChannel < NumChannels-1) && (Speed > Constraints(TargetChannel)) )
		{
			TargetChannel++;
		}

		// See if we need to blend down.
		if( TargetChannel > 0 )
		{
			FLOAT SpeedRatio = (Speed - Constraints(TargetChannel-1)) / (Constraints(TargetChannel) - Constraints(TargetChannel-1));
			if( SpeedRatio <= BlendDownPerc )
			{
				TargetChannel--;
			}
		}

		if( TargetChannel != LastChannel )
		{
			if( TargetChannel < LastChannel )
			{
				SetActiveChild( TargetChannel, BlendDownTime );
			}
			else
			{
				SetActiveChild( TargetChannel, BlendUpTime );
			}
			LastChannel = TargetChannel;
		}
	}
	else if( !SufficientChannels )
	{
		debugf(TEXT("UAnimNodeBlendBySpeed::TickAnim - Need at least two children"));
	}
	else if( !SufficientConstraints )
	{
		debugf(TEXT("UAnimNodeBlendBySpeed::TickAnim - Number of constraints (%i) is lower than number of children! (%i)"), Constraints.Num(), NumChannels);
	}
	
	Super::TickAnim(DeltaSeconds, TotalWeight);				
}

///////////////////////////////////////
//////////// UAnimTree ////////////////
///////////////////////////////////////



///////////////////////////////////////
// Thumbnail support

void UAnimTree::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	UTexture2D* Icon = LoadObject<UTexture2D>(NULL, TEXT("EngineMaterials.UnrealEdIcon_AnimTree"), NULL, LOAD_NoFail, NULL);
	InRI->DrawTile( InX, InY, Icon->SizeX*InZoom, Icon->SizeY*InZoom, 0.0f,	0.0f, 1.0f, 1.0f, FLinearColor::White, Icon );
}

FThumbnailDesc UAnimTree::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	UTexture2D* Icon = LoadObject<UTexture2D>(NULL, TEXT("EngineMaterials.UnrealEdIcon_AnimTree"), NULL, LOAD_NoFail, NULL);
	FThumbnailDesc	ThumbnailDesc;
	ThumbnailDesc.Width	= InFixedSz ? InFixedSz : Icon->SizeX*InZoom;
	ThumbnailDesc.Height = InFixedSz ? InFixedSz : Icon->SizeY*InZoom;
	return ThumbnailDesc;
}

INT UAnimTree::GetThumbnailLabels( TArray<FString>* InLabels )
{
	TArray<UAnimNode*> Nodes;
	GetNodes(Nodes);

	InLabels->Empty();
	new( *InLabels )FString( GetName() );
	new( *InLabels )FString( FString::Printf(TEXT("%d Nodes"), Nodes.Num()) );
	return InLabels->Num();
}