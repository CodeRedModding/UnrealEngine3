/*=============================================================================
	UnSkeletalRender.cpp: Skeletal mesh skinning/rendering code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "EnginePhysicsClasses.h"
#include "UnSkeletalRender.h"

/*-----------------------------------------------------------------------------
Globals
-----------------------------------------------------------------------------*/

// smallest blend weight for morph targets
const FLOAT MinMorphBlendWeight = 0.01f;
// largest blend weight for morph targets
const FLOAT MaxMorphBlendWeight = 5.0f;

/*-----------------------------------------------------------------------------
FSkeletalMeshObject
-----------------------------------------------------------------------------*/

/** 
*	Given a set of views, update the MinDesiredLODLevel member to indicate the minimum (ie best) LOD we would like to use to render this mesh. 
*	This is called from the rendering thread (PreRender) so be very careful what you read/write to.
*	If this is the first render for the frame, will just set MinDesiredLODLevel - otherwise will set it to min of current MinDesiredLODLevel and calculated value.
*/
void FSkeletalMeshObject::UpdateMinDesiredLODLevel(const FSceneView* View, const FBoxSphereBounds& Bounds, INT FrameNumber)
{
	const FVector4 ScreenPosition( View->WorldToScreen(Bounds.Origin) );
	const FLOAT ScreenRadius = Max((FLOAT)View->SizeX / 2.0f * View->ProjectionMatrix.M[0][0],
		(FLOAT)View->SizeY / 2.0f * View->ProjectionMatrix.M[1][1]) * Bounds.SphereRadius / Max(ScreenPosition.W,1.0f);
	const FLOAT LODFactor = ScreenRadius / 320.0f;

	check( SkeletalMesh->LODInfo.Num() == SkeletalMesh->LODModels.Num() );

	// Need the current LOD
	const INT CurrentLODLevel = GetLOD();
	const FLOAT HysteresisOffset = 0.f;

	INT NewLODLevel = 0;
	// Iterate from worst to best LOD
	for(INT LODLevel = SkeletalMesh->LODModels.Num()-1; LODLevel > 0; LODLevel--) 
	{
		// Get DistanceFactor for this LOD
		FLOAT LODDistanceFactor = SkeletalMesh->LODInfo(LODLevel).DisplayFactor;

		// If we are considering shifting to a better (lower) LOD, bias with hysteresis.
		if(LODLevel  <= CurrentLODLevel)
		{
			LODDistanceFactor += SkeletalMesh->LODInfo(LODLevel).LODHysteresis;
		}

		// If have passed this boundary, use this LOD
		if(LODDistanceFactor > LODFactor)
		{
			NewLODLevel = LODLevel;
			break;
		}
	}

	// Different path for first-time vs subsequent-times in this function (ie splitscreen)
	if(FrameNumber != LastFrameNumber)
	{
		// Copy last frames value to the version that will be read by game thread
		MaxDistanceFactor = WorkingMaxDistanceFactor;
		MinDesiredLODLevel = WorkingMinDesiredLODLevel;
		LastFrameNumber = FrameNumber;

		WorkingMaxDistanceFactor = LODFactor;
		WorkingMinDesiredLODLevel = NewLODLevel;
	}
	else
	{
		WorkingMaxDistanceFactor = ::Max(WorkingMaxDistanceFactor, LODFactor);
		WorkingMinDesiredLODLevel = ::Min(WorkingMinDesiredLODLevel, NewLODLevel);
	}
}

/**
 * List of chunks to be rendered based on instance weight usage. Full swap of weights will render with its own chunks.
 * @return Chunks to iterate over for rendering
 */
const TArray<FSkelMeshChunk>& FSkeletalMeshObject::GetRenderChunks(INT InLODIndex) const
{
	const FStaticLODModel& LOD = SkeletalMesh->LODModels(InLODIndex);
	const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo(InLODIndex);
	const UBOOL bUseInstance = MeshLODInfo.bUseInstancedVertexInfluences && 
		MeshLODInfo.InstanceWeightUsage == IWU_FullSwap && 
		LOD.VertexInfluences.IsValidIndex(MeshLODInfo.InstanceWeightIdx) &&
		LOD.VertexInfluences(MeshLODInfo.InstanceWeightIdx).Chunks.Num() > 0;
	return bUseInstance ? LOD.VertexInfluences(MeshLODInfo.InstanceWeightIdx).Chunks : LOD.Chunks;
}

/**
 * Update the hidden material section flags for an LOD entry
 *
 * @param InLODIndex - LOD entry to update hidden material flags for
 * @param HiddenMaterials - array of hidden material sections
 */
void FSkeletalMeshObject::SetHiddenMaterials(INT InLODIndex,const TArray<UBOOL>& HiddenMaterials)
{
	check(LODInfo.IsValidIndex(InLODIndex));
	LODInfo(InLODIndex).HiddenMaterials = HiddenMaterials;		
}

/**
 * Determine if the material section entry for an LOD is hidden or not
 *
 * @param InLODIndex - LOD entry to get hidden material flags for
 * @param MaterialIdx - index of the material section to check
 */
UBOOL FSkeletalMeshObject::IsMaterialHidden(INT InLODIndex,INT MaterialIdx) const
{
	check(LODInfo.IsValidIndex(InLODIndex));
	return LODInfo(InLODIndex).HiddenMaterials.IsValidIndex(MaterialIdx) && LODInfo(InLODIndex).HiddenMaterials(MaterialIdx);
}
/**
 * Initialize the array of LODInfo based on the settings of the current skel mesh component
 */
void FSkeletalMeshObject::InitLODInfos(const USkeletalMeshComponent* SkelComponent)
{
	LODInfo.Empty(SkeletalMesh->LODInfo.Num());
	for (INT Idx=0; Idx < SkeletalMesh->LODInfo.Num(); Idx++)
	{
		FSkelMeshObjectLODInfo& MeshLODInfo = *new(LODInfo) FSkelMeshObjectLODInfo();
		if (SkelComponent->LODInfo.IsValidIndex(Idx))
		{
			const FSkelMeshComponentLODInfo &Info = SkelComponent->LODInfo(Idx);

			MeshLODInfo.HiddenMaterials = Info.HiddenMaterials;
			MeshLODInfo.InstanceWeightIdx = Info.InstanceWeightIdx;
			MeshLODInfo.InstanceWeightUsage = (EInstanceWeightUsage)Info.InstanceWeightUsage;

			// force toggle instance weight usage before skeletal mesh gets reinitialized
			MeshLODInfo.bUseInstancedVertexInfluences = Info.bAlwaysUseInstanceWeights && !GSystemSettings.bDisableSkeletalInstanceWeights;
		}		
	}
}

/*-----------------------------------------------------------------------------
Global functions
-----------------------------------------------------------------------------*/

/**
 * Utility function that fills in the array of ref-pose to local-space matrices using 
 * the mesh component's updated space bases
 * @param	ReferenceToLocal - matrices to update
 * @param	SkeletalMeshComponent - mesh primitive with updated bone matrices
 * @param	LODIndex - each LOD has its own mapping of bones to update
 * @param	ExtraRequiredBoneIndices - any extra bones apart from those active in the LOD that we'd like to update
 */
void UpdateRefToLocalMatrices( TArray<FBoneAtom>& ReferenceToLocal, const USkeletalMeshComponent* SkeletalMeshComponent, INT LODIndex, const TArray<WORD>* ExtraRequiredBoneIndices )
{
	const USkeletalMesh* const ThisMesh = SkeletalMeshComponent->SkeletalMesh;
	const USkeletalMeshComponent* const ParentComp = SkeletalMeshComponent->ParentAnimComponent;
	const FStaticLODModel& LOD = ThisMesh->LODModels(LODIndex);

	if(ReferenceToLocal.Num() != ThisMesh->RefBasesInvMatrix.Num())
	{
		ReferenceToLocal.Empty(ThisMesh->RefBasesInvMatrix.Num());
		ReferenceToLocal.Add(ThisMesh->RefBasesInvMatrix.Num());
	}

	const UBOOL bIsParentValid = ParentComp && SkeletalMeshComponent->ParentBoneMap.Num() == ThisMesh->RefSkeleton.Num();
	const TArray<WORD>* RequiredBoneSets[3] = { &LOD.ActiveBoneIndices, ExtraRequiredBoneIndices, NULL };

	// Handle case of using ParentAnimComponent for SpaceBases.
	for( INT RequiredBoneSetIndex = 0; RequiredBoneSets[RequiredBoneSetIndex]!=NULL; RequiredBoneSetIndex++ )
	{
		const TArray<WORD>& RequiredBoneIndices = *RequiredBoneSets[RequiredBoneSetIndex];

		// Get the index of the bone in this skeleton, and loop up in table to find index in parent component mesh.
		for(INT BoneIndex = 0;BoneIndex < RequiredBoneIndices.Num();BoneIndex++)
		{
			const INT ThisBoneIndex = RequiredBoneIndices(BoneIndex);

			if ( ThisMesh->RefBasesInvMatrix.IsValidIndex(ThisBoneIndex) )
			{
				FBoneAtom const *ParentMatrix = NULL;

				if( bIsParentValid)
				{
					// If valid, use matrix from parent component.
					const INT ParentBoneIndex = SkeletalMeshComponent->ParentBoneMap(ThisBoneIndex);
					if ( ParentComp->SpaceBases.IsValidIndex(ParentBoneIndex) )
					{
						ParentMatrix = &ParentComp->SpaceBases(ParentBoneIndex);
					}
				}
				else
				{
					// If we can't find this bone in the parent, we just use the reference pose.
					ParentMatrix = &SkeletalMeshComponent->SpaceBases(ThisBoneIndex);
				}

				if ( ParentMatrix )
				{
					ReferenceToLocal(ThisBoneIndex) = ThisMesh->RefBasesInvMatrix(ThisBoneIndex) * *ParentMatrix;
					checkSlow( ParentMatrix->IsRotationNormalized() );
					checkSlow( ThisMesh->RefBasesInvMatrix(ThisBoneIndex).IsRotationNormalized() );
					checkSlow( ReferenceToLocal(ThisBoneIndex).IsRotationNormalized() );
				}
				else
				{
					// On the off chance the parent matrix isn't valid, revert to identity.
					ReferenceToLocal(ThisBoneIndex) = FBoneAtom::Identity;
				}
			}
			else
			{
				// In this case we basically want Reference->Reference ie identity.
				ReferenceToLocal(ThisBoneIndex) = FBoneAtom::Identity;
			}
		}
	}
}

/**
 * Utility function that calculates the local-space origin and bone direction vectors for the
 * current pose for any TRISORT_CustomLeftRight sections.
 * @param	OutVectors - origin and direction vectors to update
 * @param	SkeletalMeshComponent - mesh primitive with updated bone matrices
 * @param	LODIndex - current LOD
 */
void UpdateCustomLeftRightVectors( TArray<FTwoVectors>& OutVectors, const USkeletalMeshComponent* SkeletalMeshComponent, INT LODIndex )
{
	const USkeletalMesh* const ThisMesh = SkeletalMeshComponent->SkeletalMesh;
	const USkeletalMeshComponent* const ParentComp = SkeletalMeshComponent->ParentAnimComponent;
	const FStaticLODModel& LOD = ThisMesh->LODModels(LODIndex);
	const FSkeletalMeshLODInfo& LODInfo = ThisMesh->LODInfo(LODIndex);

	if(OutVectors.Num() != LODInfo.TriangleSortSettings.Num())
	{
		OutVectors.Empty(LODInfo.TriangleSortSettings.Num());
		OutVectors.Add(LODInfo.TriangleSortSettings.Num());
	}

	const FVector AxisDirections[] = { FVector(1.f,0.f,0.f), FVector(0.f,1.f,0.f), FVector(0.f,0.f,1.f) };

	for ( INT SectionIndex = 0 ; SectionIndex < LOD.Sections.Num() ; ++SectionIndex )
	{
		if( LOD.Sections(SectionIndex).TriangleSorting == TRISORT_CustomLeftRight )
		{
			FName CustomLeftRightBoneName = LODInfo.TriangleSortSettings(SectionIndex).CustomLeftRightBoneName;
			if( CustomLeftRightBoneName == NAME_None )
			{
				OutVectors(SectionIndex).v1 = FVector(0,0,0);
				OutVectors(SectionIndex).v2 = AxisDirections[LODInfo.TriangleSortSettings(SectionIndex).CustomLeftRightAxis];
			}
			else
			{
				INT SpaceBasesBoneIndex = ThisMesh->MatchRefBone(CustomLeftRightBoneName);
				const USkeletalMeshComponent* SpaceBasesComp = SkeletalMeshComponent;
				
				// Handle case of using ParentAnimComponent for SpaceBases.
				if( ParentComp && SkeletalMeshComponent->ParentBoneMap.Num() == ThisMesh->RefSkeleton.Num() && SpaceBasesBoneIndex != INDEX_NONE )
				{
					// If valid, use matrix from parent component.
					SpaceBasesBoneIndex = SkeletalMeshComponent->ParentBoneMap(SpaceBasesBoneIndex);
					SpaceBasesComp = ParentComp;
				}

				if ( SpaceBasesComp->SpaceBases.IsValidIndex(SpaceBasesBoneIndex) )
				{
					const FMatrix BoneMatrix = SpaceBasesComp->SpaceBases(SpaceBasesBoneIndex).ToMatrix();
					OutVectors(SectionIndex).v1 = BoneMatrix.GetOrigin();
					OutVectors(SectionIndex).v2 = BoneMatrix.GetAxis(LODInfo.TriangleSortSettings(SectionIndex).CustomLeftRightAxis);
				}
				else
				{
					OutVectors(SectionIndex).v1 = FVector(0,0,0);
					OutVectors(SectionIndex).v2 = AxisDirections[LODInfo.TriangleSortSettings(SectionIndex).CustomLeftRightAxis];
				}
			}
		}
	}
}
