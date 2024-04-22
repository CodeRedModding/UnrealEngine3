/*=============================================================================
	StaticLightingAggregateMesh.cpp: Static lighting aggregate mesh implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "StaticLightingPrivate.h"

#if !CONSOLE

// Definitions.
#define TRIANGLE_AREA_THRESHOLD				DELTA
#define MAX_TRIANGLES_PER_AGGREGATED_MESH	200

void FStaticLightingAggregateMesh::AddMesh(const FStaticLightingMesh* Mesh)
{
	// Only use shadow casting meshes.
	if(Mesh->bCastShadow)
	{
		// Update the aggregate scene bounding box
		SceneBounds = SceneBounds + Mesh->BoundingBox;

		// Add meshes with many triangles to the octree, add the triangles of the lower polygon meshes to the world-space kd-tree.
		// Also add meshes which don't uniformly cast shadows, as the kd-tree only supports uniform shadow casting.
		if ( (Mesh->NumTriangles > MAX_TRIANGLES_PER_AGGREGATED_MESH || !Mesh->IsUniformShadowCaster())
			&& !Mesh->IsControllingShadowPerElement() )
		{
			MeshOctree.AddElement(Mesh);
		}
		else
		{
			const INT BaseVertexIndex = Vertices.Num();
			Vertices.Add(Mesh->NumVertices);

			for(INT TriangleIndex = 0;TriangleIndex < Mesh->NumTriangles;TriangleIndex++)
			{
				// Read the triangle from the mesh.
				FStaticLightingVertex V0;
				FStaticLightingVertex V1;
				FStaticLightingVertex V2;
				Mesh->GetTriangle(TriangleIndex,V0,V1,V2);

				INT I0 = 0;
				INT I1 = 0;
				INT I2 = 0;
				Mesh->GetTriangleIndices(TriangleIndex,I0,I1,I2);

				Vertices(BaseVertexIndex + I0) = V0.WorldPosition;
				Vertices(BaseVertexIndex + I1) = V1.WorldPosition;
				Vertices(BaseVertexIndex + I2) = V2.WorldPosition;

				// Compute the triangle's normal.
				const FVector TriangleNormal = (V2.WorldPosition - V0.WorldPosition) ^ (V1.WorldPosition - V0.WorldPosition);

				// Compute the triangle area.
				const FLOAT TriangleArea = TriangleNormal.Size() * 0.5f;

				// Ignore zero area triangles.
				if( TriangleArea > TRIANGLE_AREA_THRESHOLD && Mesh->IsTriangleCastingShadow( TriangleIndex ) )
				{
					new(kDOPTriangles) FkDOPBuildCollisionTriangle<DWORD>(
						BaseVertexIndex + I0,BaseVertexIndex + I1,BaseVertexIndex + I2,
						kDopMeshes.AddItem(Mesh), // Use the triangle's material index as an index into kDOPMeshes.
						V0.WorldPosition,V1.WorldPosition,V2.WorldPosition
						);
				}
			}
		}
	}
}

FBox FStaticLightingAggregateMesh::GetBounds() const
{
	return SceneBounds;
}

class FStaticLightingAggregateMeshDataProvider
{
public:

	/** Initialization constructor. */
	FStaticLightingAggregateMeshDataProvider(const FStaticLightingAggregateMesh* InMesh,const FLightRay& InLightRay):
		Mesh(InMesh),
		LightRay(InLightRay)
	{}

	// kDOP data provider interface.

	FORCEINLINE const FVector& GetVertex(DWORD Index) const
	{
		return Mesh->Vertices(Index);
	}

	FORCEINLINE UMaterialInterface* GetMaterial(DWORD MaterialIndex) const
	{
		return NULL;
	}

	FORCEINLINE INT GetItemIndex(WORD MaterialIndex) const
	{
		return 0;
	}

	FORCEINLINE UBOOL ShouldCheckMaterial(INT MaterialIndex) const
	{
		return TRUE;
	}

	FORCEINLINE const TkDOPTree<const FStaticLightingAggregateMeshDataProvider,DWORD>& GetkDOPTree(void) const
	{
		return Mesh->kDopTree;
	}

	FORCEINLINE const FMatrix& GetLocalToWorld(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE const FMatrix& GetWorldToLocal(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE FMatrix GetLocalToWorldTransposeAdjoint(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE FLOAT GetDeterminant(void) const
	{
		return 1.0f;
	}

private:

	const FStaticLightingAggregateMesh* Mesh;
	const FLightRay& LightRay;
};

FLightRayIntersection FStaticLightingAggregateMesh::IntersectLightRay(const FLightRay& LightRay,UBOOL bFindClosestIntersection,FCoherentRayCache& CoherentRayCache) const
{
	FLightRay ClippedLightRay = LightRay;
	FLightRayIntersection ClosestIntersection = FLightRayIntersection::None();

	// Check the mesh which was last hit by a ray in this thread.
	if(!bFindClosestIntersection)
	{
		ClosestIntersection = CoherentRayCache.LastHitMesh.IntersectLightRay(ClippedLightRay,bFindClosestIntersection);
		if(ClosestIntersection.bIntersects)
		{
			return ClosestIntersection;
		}
	}

	// Reset the last hit mesh.
	CoherentRayCache.LastHitMesh = NULL;

	// Check the kd-tree containing low polygon meshes first.
	FCheckResult Result(1.0f);
	FStaticLightingAggregateMeshDataProvider kDOPDataProvider(this,ClippedLightRay);
	TkDOPLineCollisionCheck<const FStaticLightingAggregateMeshDataProvider,DWORD> kDOPCheck(LightRay.Start,LightRay.Start + LightRay.Direction * LightRay.Length,0,kDOPDataProvider,&Result);
	if(kDopTree.LineCheck(kDOPCheck))
	{
		// Setup a vertex to represent the intersection.
		ClosestIntersection.bIntersects = TRUE;
		ClosestIntersection.IntersectionVertex.WorldPosition = ClippedLightRay.Start + ClippedLightRay.Direction * ClippedLightRay.Length * Result.Time;
		ClosestIntersection.IntersectionVertex.WorldTangentZ = kDOPCheck.GetHitNormal();

		if(bFindClosestIntersection)
		{
			ClippedLightRay.ClipAgainstIntersection(ClosestIntersection.IntersectionVertex.WorldPosition);
		}
		else
		{
			return ClosestIntersection;
		}
	}

	// Check against the mesh octree.
	for(FStaticLightingMeshOctree::TConstIterator<> OctreeIt(MeshOctree);OctreeIt.HasPendingNodes();OctreeIt.Advance())
	{
		const FStaticLightingMeshOctree::FNode& CurrentNode = OctreeIt.GetCurrentNode();
		const FOctreeNodeContext& CurrentContext = OctreeIt.GetCurrentContext();

		// If this node has children that intersect the ray, enqueue them for future iteration.
		if(!CurrentNode.IsLeaf())
		{
			FOREACH_OCTREE_CHILD_NODE(ChildRef)
			{
				if(CurrentNode.HasChild(ChildRef))
				{
					FOctreeNodeContext ChildContext = CurrentContext.GetChildContext(ChildRef);
					if(FLineBoxIntersection(
						ChildContext.Bounds.GetBox(),
						ClippedLightRay.Start,
						ClippedLightRay.End,
						ClippedLightRay.Direction,
						ClippedLightRay.OneOverDirection
						))
					{
						OctreeIt.PushChild(ChildRef);
					}
				}
			}
		}

		// Check the line segment for intersection with the meshes in this node.
		for(FStaticLightingMeshOctree::ElementConstIt MeshIt(CurrentNode.GetElementIt());MeshIt;++MeshIt)
		{
			const FMeshAndBounds& MeshAndBounds = *MeshIt;
			FLightRayIntersection MeshIntersection = MeshAndBounds.IntersectLightRay(ClippedLightRay,bFindClosestIntersection);
			if(MeshIntersection.bIntersects)
			{
				ClosestIntersection = MeshIntersection;
				if(bFindClosestIntersection)
				{
					ClippedLightRay.ClipAgainstIntersection(ClosestIntersection.IntersectionVertex.WorldPosition);
				}
				else
				{
					// Update the thread's last hit mesh and return a hit.
					CoherentRayCache.LastHitMesh = MeshAndBounds.Mesh;
					return ClosestIntersection;
				}
			}
		}
	}

	return ClosestIntersection;
}

FStaticLightingAggregateMesh::FStaticLightingAggregateMesh():
	MeshOctree(FVector(0,0,0),HALF_WORLD_MAX),
	SceneBounds(0)
{}

#endif
