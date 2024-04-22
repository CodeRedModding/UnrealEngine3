/*=============================================================================
	UnSkeletalMeshCollision.cpp: Skeletal mesh collision code
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

static UBOOL LineBoxIntersect
(
	 const FVector&	BoxCenter,
	 const FVector& BoxExtent,
	 const FVector&	LineStart,
	 const FVector&	LineDir,
	 const FVector&	OneOverLineDir,
	 FCheckResult& Result
 )
{
	FLOAT tf, tb;
	FLOAT tnear = -BIG_NUMBER;
	FVector Normal(0,0,1);
	FLOAT tfar = BIG_NUMBER;

	FVector LocalStart = LineStart - BoxCenter;

	// X //
	// First - see if ray is parallel to slab.
	if(LineDir.X != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.X * OneOverLineDir.X) - BoxExtent.X * Abs(OneOverLineDir.X);
		tb = - (LocalStart.X * OneOverLineDir.X) + BoxExtent.X * Abs(OneOverLineDir.X);

		if(tf > tnear)
		{
			tnear = tf;
			Normal = (LineDir.X > 0.f) ? FVector(-1,0,0) : FVector(1,0,0);
		}

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return 0;
	}
	else
	{
		// If it is parallel, early return if start is outside slab.
		if(!(Abs(LocalStart.X) <= BoxExtent.X))
			return 0;
	}

	// Y //
	if(LineDir.Y != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.Y * OneOverLineDir.Y) - BoxExtent.Y * Abs(OneOverLineDir.Y);
		tb = - (LocalStart.Y * OneOverLineDir.Y) + BoxExtent.Y * Abs(OneOverLineDir.Y);


		if(tf > tnear)
		{
			tnear = tf;
			Normal = (LineDir.Y > 0.f) ? FVector(0,-1,0) : FVector(0,1,0);
		}

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return 0;
	}
	else
	{
		if(!(Abs(LocalStart.Y) <= BoxExtent.Y))
			return 0;
	}

	// Z //
	if(LineDir.Z != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.Z * OneOverLineDir.Z) - BoxExtent.Z * Abs(OneOverLineDir.Z);
		tb = - (LocalStart.Z * OneOverLineDir.Z) + BoxExtent.Z * Abs(OneOverLineDir.Z);

		if(tf > tnear)
		{
			tnear = tf;
			Normal = (LineDir.Z > 0.f) ? FVector(0,0,-1) : FVector(0,0,1);
		}

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return 0;
	}
	else
	{
		if(!(Abs(LocalStart.Z) <= BoxExtent.Z))
			return 0;
	}

	if(tnear > 1.f || tnear < 0.f)
	{
		return 0;
	}

	Result.Time = tnear;
	Result.Location = LineStart + (tnear * LineDir);
	Result.Normal = Normal;

	// we hit!
	return 1;
}

UBOOL USkeletalMeshComponent::LineCheck(
                FCheckResult &Result,
                const FVector& End,
                const FVector& Start,
                const FVector& Extent,
				DWORD TraceFlags)
{
	UBOOL Retval = TRUE;

	// Line check always fail if no SkeletalMesh.
	if(!SkeletalMesh)
	{
		return Retval;
	}

	// Special case for line checks just against the bounds.
	if(bEnableLineCheckWithBounds)
	{
		FVector LineDir = End - Start;
		FVector OneOverLineDir;
		OneOverLineDir.X = Square(LineDir.X) > Square(DELTA) ? 1.0f / LineDir.X : 0.0f;
		OneOverLineDir.Y = Square(LineDir.Y) > Square(DELTA) ? 1.0f / LineDir.Y : 0.0f;
		OneOverLineDir.Z = Square(LineDir.Z) > Square(DELTA) ? 1.0f / LineDir.Z : 0.0f;

		FCheckResult Hit(1.f);
		FVector CollisionExtent = LineCheckBoundsScale * Bounds.BoxExtent;
		FLOAT ZOffset = (Bounds.BoxExtent.Z - CollisionExtent.Z); // Offset box down so bottom stays in same place
		UBOOL bHit = LineBoxIntersect(Bounds.Origin - (FVector(0,0,1) * ZOffset), CollisionExtent + Extent, Start, LineDir, OneOverLineDir, Hit);
		if(bHit)
		{
			Result = Hit;
			Result.Actor = Owner;
			Result.Component = this;	
			Result.BoneName = SkeletalMesh->RefSkeleton(0).Name;
			Retval = FALSE;
		}

		return Retval;
	}

	// Normal code path testing against PhysicsAsset
	UBOOL bZeroExtent = Extent.IsZero();
	UBOOL bWantSimpleCheck = (SkeletalMesh->bUseSimpleBoxCollision && !bZeroExtent) || (SkeletalMesh->bUseSimpleLineCollision && bZeroExtent);

	UBOOL bTestPhysAssetPerPolyShapes = FALSE;

	// If we want to use per-poly collision information (works for specified rigid parts).
	// If no bones are specified for per-poly, always fall back to using simplified collision.
	if(!bWantSimpleCheck || ((TraceFlags & TRACE_ComplexCollision) && SkeletalMesh->PerPolyCollisionBones.Num() > 0))
	{
		check(SkeletalMesh->PerPolyCollisionBones.Num() == SkeletalMesh->PerPolyBoneKDOPs.Num());

		UBOOL bHit = FALSE;
		Result.Time = 1.f;
		for(INT i=0; i<SkeletalMesh->PerPolyBoneKDOPs.Num(); i++)
		{
			FName BoneName = SkeletalMesh->PerPolyCollisionBones(i);
			INT BoneIndex = SkeletalMesh->MatchRefBone(BoneName);
			if(BoneIndex != INDEX_NONE)
			{
				FSkelMeshCollisionDataProvider Provider(this, SkeletalMesh, BoneIndex, i);
				FCheckResult TempResult(1.f);
				UBOOL bTraceHit = FALSE;

				if(bZeroExtent)
				{
					TkDOPLineCollisionCheck<FSkelMeshCollisionDataProvider,WORD,TSkeletalKDOPTree> kDOPCheck(Start,End,TraceFlags,Provider,&TempResult);
					// Do the line check
					bTraceHit = SkeletalMesh->PerPolyBoneKDOPs(i).KDOPTree.LineCheck(kDOPCheck);
					if(bTraceHit)
					{
						TempResult.Normal = kDOPCheck.GetHitNormal();
					}
				}
				else
				{
					TkDOPBoxCollisionCheck<FSkelMeshCollisionDataProvider,WORD,TSkeletalKDOPTree> kDOPCheck(Start,End,Extent,TraceFlags,Provider,&TempResult);
					// Do the swept-box check
					bTraceHit = SkeletalMesh->PerPolyBoneKDOPs(i).KDOPTree.BoxCheck(kDOPCheck);
					if(bTraceHit)
					{
						TempResult.Normal = kDOPCheck.GetHitNormal();
					}
				}

				// If the result is closer than our best so far, keep this result
				if(TempResult.Time < Result.Time)
				{
					Result = TempResult;
					Result.BoneName = BoneName;
					bHit = TRUE;
				}
			} 
		}

		// If we got at least one hit, fill in the other details and return FALSE (
		if(bHit)
		{
			//FVector TrueLocation = Start + (End - Start) * Result.Time;

			Result.Actor = Owner;
			Result.Component = this;
			if (TraceFlags & TRACE_Accurate)
			{
				Result.Time = Clamp(Result.Time,0.0f,1.0f);
			}
			else
			{
				Result.Time = Clamp(Result.Time - Clamp(0.1f,0.1f / (End - Start).Size(),4.0f / (End - Start).Size()),0.0f,1.0f);
			}
			Result.Location = Start + (End - Start) * Result.Time;
			Retval = FALSE;

			//GWorld->PersistentLineBatcher->DrawLine(Start, TrueLocation, FColor(0,255,255), SDPG_World);
			//GWorld->PersistentLineBatcher->DrawLine(FVector(0,0,0), TrueLocation, FColor(255,0,0), SDPG_World);
		}
		else
		{
			Retval = TRUE;
		}

		bTestPhysAssetPerPolyShapes = TRUE;
	}

	// The PhysicsAsset provides the simplified collision geometry for a skeletal mesh
	if( PhysicsAsset != NULL )
	{
		FCheckResult PhysAssetResult(1.f);
		UBOOL bPhysAssetRetval = PhysicsAsset->LineCheck( PhysAssetResult, this, Start, End, Extent, bTestPhysAssetPerPolyShapes );

		// If we hit..
		if(!bPhysAssetRetval)
		{
			if(!bZeroExtent)
			{
				if (TraceFlags & TRACE_Accurate)
				{
					Result.Time = Clamp(PhysAssetResult.Time,0.0f,1.0f);
				}
				else
				{
					// If we hit and it's an extent trace (eg player movement) - pull back the hit location in the same way as static meshes.
					PhysAssetResult.Time = Clamp(PhysAssetResult.Time - Clamp(0.1f,0.1f / (End - Start).Size(),4.0f / (End - Start).Size()),0.0f,1.0f);
				}
				PhysAssetResult.Location = Start + (End - Start) * PhysAssetResult.Time;
			}

			// If there was no per-poly hit, or the PhysAsset hit was closer, use that
			if(Retval || PhysAssetResult.Time < Result.Time)
			{
				Result = PhysAssetResult;
				Retval = FALSE;				
			}
		}
	}

#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	if(ClothSim && bEnableClothSimulation && SkeletalMesh && SkeletalMesh->bEnableClothLineChecks && (TraceFlags & TRACE_ComplexCollision))
	{
		FCheckResult TempResult;

		if(!ClothLineCheck(this, TempResult, End, Start, Extent, TraceFlags))
		{//hit
			if( Retval || (TempResult.Time < Result.Time) )
			{
				Result = TempResult;
				Retval = FALSE;
			}
		}
	}
#endif

#if WITH_NOVODEX && !NX_DISABLE_SOFTBODY
	if(SoftBodySim && bEnableSoftBodySimulation && SkeletalMesh && SkeletalMesh->bEnableSoftBodyLineChecks && (TraceFlags & TRACE_ComplexCollision))
	{
		FCheckResult TempResult;

		if(!SoftBodyLineCheck(this, TempResult, End, Start, Extent, TraceFlags))
		{// hit
			if( Retval || (TempResult.Time < Result.Time) )
			{
				Result = TempResult;
				Retval = FALSE;
			}
		}
	}
#endif

	return Retval;
}


UBOOL USkeletalMeshComponent::PointCheck(FCheckResult& Result, const FVector& Location, const FVector& Extent, DWORD TraceFlags)
{
	UBOOL bHit = FALSE;

	if(PhysicsAsset)
	{
		bHit = !PhysicsAsset->PointCheck( Result, this, Location, Extent );
	}

#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	if(!bHit)
	{
		if(ClothSim && bEnableClothSimulation && SkeletalMesh && SkeletalMesh->bEnableClothLineChecks)
		{
			bHit = !ClothPointCheck(Result, this, Location, Extent);
		}
	}
#endif

	return !bHit;
}

/** Utility to find which bone in ParentBones is the 'best' (ie closest) parent to BoneName. */
INT FindBestParent(FName BoneName, TArray<FName>& ParentBones, USkeletalMesh* SkelMesh)
{
	// If bone is in parent array, ignore it
	if(ParentBones.ContainsItem(BoneName))
	{
		return INDEX_NONE;
	}

	// Find the index of the child bone
	INT BoneIndex = SkelMesh->MatchRefBone(BoneName);
	if(BoneIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	// Walk up from that bone, seeing if a parent is in the ParentBones array, and returning that Parent bones index when found.
	INT TestIndex = SkelMesh->RefSkeleton(BoneIndex).ParentIndex;
	while(1)
	{
		FName TestName = SkelMesh->RefSkeleton(TestIndex).Name;
		if( ParentBones.ContainsItem(TestName) )
		{
			return TestIndex;
		}

		if( TestIndex == 0 )
		{
			return INDEX_NONE;
		}
		TestIndex = SkelMesh->RefSkeleton(TestIndex).ParentIndex;
	}
}

/** Re-generate the per-poly collision data in the PerPolyBoneKDOPs array, based on names in the PerPolyCollisionBones array. */
void USkeletalMesh::UpdatePerPolyKDOPs()
{
	if( GetOutermost()->PackageFlags & PKG_Cooked )
	{
		// Can't re-generate this for cooked content because the chunk rigid and soft verts are cleared.
		return;
	}

	if (LODModels(0).MultiSizeIndexContainer.GetDataTypeSize() != sizeof(WORD))
	{
		// We don't currently support KDOPs for meshes using 32 bit indices
		return;
	}

	// Empty and resize array of kdops.
	PerPolyBoneKDOPs.Empty();
	PerPolyBoneKDOPs.AddZeroed(PerPolyCollisionBones.Num());

	// Build vertex buffer with _all_ verts in skeletal mesh.
	FStaticLODModel* LODModel = &LODModels(0);
	TArray<FVector>	AllVerts;
	for(INT ChunkIndex = 0;ChunkIndex < LODModel->Chunks.Num();ChunkIndex++)
	{
		FSkelMeshChunk& Chunk = LODModel->Chunks(ChunkIndex);

		if(Chunk.GetNumRigidVertices() > 0)
		{
			for(INT VertIdx=0; VertIdx<Chunk.GetNumRigidVertices(); VertIdx++)
			{
				AllVerts.AddItem( LODModel->VertexBufferGPUSkin.GetVertexPosition(Chunk.GetRigidVertexBufferIndex()+VertIdx));
			}
		}

		if(Chunk.GetNumSoftVertices() > 0)
		{
			for(INT VertIdx=0; VertIdx<Chunk.GetNumSoftVertices(); VertIdx++)
			{
				AllVerts.AddItem( LODModel->VertexBufferGPUSkin.GetVertexPosition(Chunk.GetSoftVertexBufferIndex()+VertIdx));
			}
		}
	}

	// Iterate over each bone we want collision for.
	if(PerPolyCollisionBones.Num() > 0)
	{
		debugf(TEXT("Building per-poly collision kDOP trees for '%s'"), *GetPathName());
	}

	for(INT i=0; i<PerPolyCollisionBones.Num(); i++)
	{
		FPerPolyBoneCollisionData& Data = PerPolyBoneKDOPs(i);

		INT BoneIndex = MatchRefBone(PerPolyCollisionBones(i));
		if(BoneIndex != INDEX_NONE)
		{
			// Make a list of all bones that are 'relevant' to this one
			TArray<INT> RelevantBones;
			// Add itself of course
			RelevantBones.AddItem(BoneIndex);
			// Now look through AddToParentPerPolyCollisionBone to find any bones whose best parent is this one
			for(INT j=0; j<AddToParentPerPolyCollisionBone.Num(); j++)
			{
				FName ChildName = AddToParentPerPolyCollisionBone(j);
				if(FindBestParent(ChildName, PerPolyCollisionBones, this) == BoneIndex)
				{
					INT ChildIndex = MatchRefBone(ChildName);
					if(ChildIndex != INDEX_NONE)
					{
						RelevantBones.AddItem(ChildIndex);
					}
				}
			}

			// Get the transform from mesh space to bone space
			const FBoneAtom& MeshToBone = RefBasesInvMatrix(BoneIndex);

			// Verts to use for collision for this bone
			TArray<INT> CollisionToGraphicsVertMap;
			// Current position in AllVerts
			INT VertIndex = 0;
			for(INT ChunkIndex = 0;ChunkIndex < LODModel->Chunks.Num();ChunkIndex++)
			{
				FSkelMeshChunk& Chunk = LODModel->Chunks(ChunkIndex);

				// Only consider rigidly weighted verts for this kind of collision
				for(INT i=0; i<Chunk.RigidVertices.Num(); i++)
				{
					FRigidSkinVertex& RV = Chunk.RigidVertices(i);
					if( RelevantBones.ContainsItem( Chunk.BoneMap(RV.Bone) ) )
					{
						CollisionToGraphicsVertMap.AddItem(VertIndex);
					}

					VertIndex++;
				}

				// If desired, consider soft verts with any weighting to specified bone
				if(bPerPolyUseSoftWeighting)
				{
					for(INT i=0; i<Chunk.SoftVertices.Num(); i++)
					{
						FSoftSkinVertex& SV = Chunk.SoftVertices(i);
						if( (SV.InfluenceWeights[0] > 0 && RelevantBones.ContainsItem(Chunk.BoneMap(SV.InfluenceBones[0]) ))||
							(SV.InfluenceWeights[1] > 0 && RelevantBones.ContainsItem(Chunk.BoneMap(SV.InfluenceBones[1]) ))||
							(SV.InfluenceWeights[2] > 0 && RelevantBones.ContainsItem(Chunk.BoneMap(SV.InfluenceBones[2]) ))||
							(SV.InfluenceWeights[3] > 0 && RelevantBones.ContainsItem(Chunk.BoneMap(SV.InfluenceBones[3]) )) )
						{
							CollisionToGraphicsVertMap.AddItem(VertIndex);
						}

						VertIndex++;
					}
				}
				else
				{
					// Wind on VertIndex over all the soft verts in this chunk.
					VertIndex += Chunk.SoftVertices.Num();
				}
			}

			// For all the verts that are rigidly weighted to this bone, transform them into 'bone space'
			Data.CollisionVerts.Add(CollisionToGraphicsVertMap.Num());
			for(INT VertIdx=0; VertIdx<CollisionToGraphicsVertMap.Num(); VertIdx++)
			{
				Data.CollisionVerts(VertIdx) = MeshToBone.TransformFVector( AllVerts(CollisionToGraphicsVertMap(VertIdx)) );
			}

			// Now make triangles for building kDOP
			// Find all triangles where all 3 verts are rigidly weighted to bone (ie all three verts are in CollisionToGraphicsVertMap)
			TArray<FkDOPBuildCollisionTriangle<WORD> > KDOPTriangles;
			for(INT j=0; j<LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Num(); j+=3)
			{
				WORD Index0 = (WORD)LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Get(j+0);
				WORD Index1 = (WORD)LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Get(j+1);
				WORD Index2 = (WORD)LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Get(j+2);

				INT CollisionIndex0 = CollisionToGraphicsVertMap.FindItemIndex(Index0);
				INT CollisionIndex1 = CollisionToGraphicsVertMap.FindItemIndex(Index1);
				INT CollisionIndex2 = CollisionToGraphicsVertMap.FindItemIndex(Index2);

				if(	CollisionIndex0 != INDEX_NONE && 
					CollisionIndex1 != INDEX_NONE && 
					CollisionIndex2 != INDEX_NONE )
				{
					// Build a new kDOP collision triangle
					new (KDOPTriangles) FkDOPBuildCollisionTriangle<WORD>(CollisionIndex0, CollisionIndex1, CollisionIndex2,
						0,
						Data.CollisionVerts(CollisionIndex0), Data.CollisionVerts(CollisionIndex1), Data.CollisionVerts(CollisionIndex2));
				}
			}

			Data.KDOPTree.Build(KDOPTriangles);
			debugf(TEXT("--- Bone: %s  Tris: %d"), *PerPolyCollisionBones(i).ToString(), Data.KDOPTree.Triangles.Num());
		}
	}
}

UBOOL USkeletalMeshComponent::GetBonesWithinRadius( const FVector& Origin, FLOAT Radius, DWORD TraceFlags, TArray<FName>& out_Bones )
{
	if( !SkeletalMesh )
		return FALSE;

	FLOAT RadiusSq = Radius*Radius;

	// Transform the Origin into mesh local space so we don't have to transform the (mesh local) bone locations
	FVector TestLocation = LocalToWorld.Inverse().TransformFVector(Origin);
	for( INT Idx = 0; Idx < SpaceBases.Num(); Idx++ )
	{
		FLOAT DistSquared = (TestLocation - SpaceBases(Idx).GetOrigin()).SizeSquared();
		if( DistSquared <= RadiusSq )
		{
			out_Bones.AddItem( SkeletalMesh->RefSkeleton(Idx).Name );
		}
	}

	return (out_Bones.Num()>0);
}

