/*=============================================================================
UnPhysAnim.cpp: Code for supporting animation/physics blending
Copyright 2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "EngineAnimClasses.h"

// Temporary workspace for caching world-space matrices.
struct FAssetWorldBoneTM
{
	FMatrix	TM; // Should never contain scaling.
	INT		UpdateNum; // If this equals PhysAssetUpdateNum, then the matrix is up to date.
};

static INT PhysAssetUpdateNum = 0;
static TArray<FAssetWorldBoneTM> WorldBoneTMs;

// Use reference pose to calculate world-space position of this bone without physics now.
static void UpdateWorldBoneTM(INT BoneIndex, USkeletalMeshComponent* skelComp, FLOAT Scale)
{
	// If its already up to date - do nothing
	if(	WorldBoneTMs(BoneIndex).UpdateNum == PhysAssetUpdateNum )
		return;

	FMatrix ParentTM, RelTM;
	if(BoneIndex == 0)
	{
		// If this is the root bone, we use the mesh component LocalToWorld as the parent transform.
		ParentTM = skelComp->LocalToWorld;
		ParentTM.RemoveScaling();
	}
	else
	{
		// If not root, use our cached world-space bone transforms.
		INT ParentIndex = skelComp->SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;
		UpdateWorldBoneTM(ParentIndex, skelComp, Scale);
		ParentTM = WorldBoneTMs(ParentIndex).TM;
	}

	RelTM = skelComp->SkeletalMesh->GetRefPoseMatrix(BoneIndex);
	RelTM.ScaleTranslation( FVector(Scale) );

	WorldBoneTMs(BoneIndex).TM = RelTM * ParentTM;
	WorldBoneTMs(BoneIndex).UpdateNum = PhysAssetUpdateNum;

}

// Calculates the local-space transforms needed by the animation system from the world-space transforms we get from the physics.
// If the parent of a physics bone is not physical, we work out its world space position by walking up the heirarchy until we reach 
// another physics bone (or the root) and then construct back down using the ref pose.

void USkeletalMeshComponent::GetPhysicsBoneAtoms( TArray<FBoneAtom>& Atoms )
{
	check(PhysicsAssetInstance);

	FVector Scale3D = Owner->DrawScale * Owner->DrawScale3D;
	if( !Scale3D.IsUniform() )
	{
		debugf( TEXT("UAnimNodePhysAsset::AddAnimationData : Non-uniform scale factor.") );
		UAnimNode::FillWithRefPose( Atoms, SkeletalMesh->RefSkeleton );
		return;
	}

	FLOAT Scale = Scale3D.X;
	FLOAT RecipScale = 1.0f/Scale;

	// If there isn't one - we just use the base-class to put in ref pose.
	if(!PhysicsAssetInstance)
	{
		UAnimNode::FillWithRefPose( Atoms, SkeletalMesh->RefSkeleton );
		return;
	}

	check( PhysicsAsset );

	// Make sure scratch space is big enough.
	if(WorldBoneTMs.Num() < Atoms.Num())
		WorldBoneTMs.AddZeroed( Atoms.Num() - WorldBoneTMs.Num() );
	check( WorldBoneTMs.Num() >= Atoms.Num() );

	PhysAssetUpdateNum++;

	// For each bone - see if we need to provide some data for it.
	for(INT i=0; i<Atoms.Num(); i++)
	{
		//FLOAT BoneWeight = Weights.GetBoneWeight(i);

		//if( BoneWeight > ZERO_ANIMWEIGHT_THRESH )
		{
			FBoneAtom ThisAtom;

			// See if this is a physics bone..
			INT BodyInstIndex = PhysicsAsset->FindBodyIndex( SkeletalMesh->RefSkeleton(i).Name );

			// If so - get its world space matrix and its parents world space matrix and calc relative atom.
			if(BodyInstIndex != INDEX_NONE)
			{	
				URB_BodyInstance* bi = PhysicsAssetInstance->Bodies(BodyInstIndex);
				FMatrix BoneTM = bi->GetUnrealWorldTM();

				// Store this world-space transform in cache.
				WorldBoneTMs(i).TM = BoneTM;
				WorldBoneTMs(i).UpdateNum = PhysAssetUpdateNum;

				// Find this bones parent matrix.
				FMatrix ParentTM;
				if(i == 0)
				{
					// If root bone is physical - use skeletal LocalToWorld as parent.
					ParentTM = LocalToWorld;
					ParentTM.RemoveScaling();
				}
				else
				{
					// If not root, get parent TM from cache (making sure its up-to-date).
					INT ParentIndex = SkeletalMesh->RefSkeleton(i).ParentIndex;
					UpdateWorldBoneTM(ParentIndex, this, Scale);
					ParentTM = WorldBoneTMs(ParentIndex).TM;
				}

				// Then calc rel TM and convert to atom.
				FMatrix RelTM = BoneTM * ParentTM.Inverse();
				FQuat RelRot(RelTM);
				FVector RelPos = RecipScale * RelTM.GetOrigin();

				ThisAtom = FBoneAtom( RelRot, RelPos, FVector (1.0f) );
			}
			else // No physics - just use reference pose.
			{
				ThisAtom = FBoneAtom( SkeletalMesh->RefSkeleton(i).BonePos.Orientation, SkeletalMesh->RefSkeleton(i).BonePos.Position, FVector(1.0f) );
			}

			Atoms(i) = ThisAtom;
		}
	}

}