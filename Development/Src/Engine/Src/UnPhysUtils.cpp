/*============================================================================
	Karma Integration Support
    
    - MeMemory/MeMessage glue
    - Debug line drawing
============================================================================*/

#include "EnginePrivate.h"

/* *********************************************************************** */
/* *********************************************************************** */
/* *********************** MODELTOHULLS  ********************************* */
/* *********************************************************************** */
/* *********************************************************************** */

#define LOCAL_EPS (0.01f)

static void AddVertexIfNotPresent(TArray<FVector> &vertices, const FVector& newVertex)
{
	UBOOL isPresent = 0;

	for(INT i=0; i<vertices.Num() && !isPresent; i++)
	{
		FLOAT diffSqr = (newVertex - vertices(i)).SizeSquared();
		if(diffSqr < LOCAL_EPS * LOCAL_EPS)
			isPresent = 1;
	}

	if(!isPresent)
		vertices.AddItem(newVertex);

}

static void AddConvexPrim(FKAggregateGeom* outGeom, TArray<FPlane> &planes, UModel* inModel, const FVector& prePivot)
{
	// Add Hull.
	int ex = outGeom->ConvexElems.AddZeroed();
	FKConvexElem* c = &outGeom->ConvexElems(ex);

	c->PlaneData = planes;

	FLOAT TotalPolyArea = 0;

	for(INT i=0; i<planes.Num(); i++)
	{
		FPoly	Polygon;
		FVector Base, AxisX, AxisY;

		Polygon.Normal = planes(i);
		Polygon.NumVertices = 4;
		Polygon.Normal.FindBestAxisVectors(AxisX,AxisY);

		Base = planes(i) * planes(i).W;

		Polygon.Vertex[0] = Base + AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX;
		Polygon.Vertex[1] = Base - AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX;
		Polygon.Vertex[2] = Base - AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX;
		Polygon.Vertex[3] = Base + AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX;

		for(INT j=0; j<planes.Num(); j++)
		{
			if(i != j)
			{
				if(!Polygon.Split(-FVector(planes(j)), planes(j) * planes(j).W))
				{
					Polygon.NumVertices = 0;
					break;
				}
			}
		}

		TotalPolyArea += Polygon.Area();

		// Add vertices of polygon to convex primitive.
		for(INT j=0; j<Polygon.NumVertices; j++)
		{
			// Because of errors with the polygon-clipping, we dont use the vertices we just generated,
			// but the ones stored in the model. We find the closest.
			INT nearestVert = INDEX_NONE;
			FLOAT nearestDistSqr = BIG_NUMBER;

			for(INT k=0; k<inModel->Verts.Num(); k++)
			{
				// Find vertex vector. Bit of  hack - sometimes FVerts are uninitialised.
				INT pointIx = inModel->Verts(k).pVertex;
				if(pointIx < 0 || pointIx >= inModel->Points.Num())
					continue;

				FLOAT distSquared = (Polygon.Vertex[j] - inModel->Points(pointIx)).SizeSquared();

				if( distSquared < nearestDistSqr )
				{
					nearestVert = k;
					nearestDistSqr = distSquared;
				}
			}

			// If we have found a suitably close vertex, use that
			if( nearestVert != INDEX_NONE && nearestDistSqr < LOCAL_EPS )
			{
				FVector localVert = ((inModel->Points(inModel->Verts(nearestVert).pVertex)) - prePivot);
				AddVertexIfNotPresent(c->VertexData, localVert);
			}
			else
			{
				FVector localVert = (Polygon.Vertex[j] - prePivot);
				AddVertexIfNotPresent(c->VertexData, localVert);
			}
		}
	}

#if 1
	if(TotalPolyArea < 0.001f || TotalPolyArea > 1000000000.0f)
	{
		debugf( TEXT("Total Polygon Area invalid: %f") );
		return;
	}

	// We need at least 4 vertices to make a convex hull with non-zero volume.
	// We shouldn't have the same vertex multiple times (using AddVertexIfNotPresent above)
	if(c->VertexData.Num() < 4)
	{
		outGeom->ConvexElems.Remove(ex);
		return;
	}

	// Check that not all vertices lie on a line (ie. find plane)
	// Again, this should be a non-zero vector because we shouldn't have duplicate verts.
	UBOOL found;
	FVector dir2, dir1;
	
	dir1 = c->VertexData(1) - c->VertexData(0);
	dir1.Normalize();

	found = 0;
	for(INT i=2; i<c->VertexData.Num() && !found; i++)
	{
		dir2 = c->VertexData(i) - c->VertexData(0);
		dir2.Normalize();

		// If line are non-parallel, this vertex forms our plane
		if((dir1 | dir2) < (1 - LOCAL_EPS))
			found = 1;
	}

	if(!found)
	{
		outGeom->ConvexElems.Remove(ex);
		return;
	}

	// Now we check that not all vertices lie on a plane, by checking at least one lies of the plane we have formed.
	FVector normal = dir1 ^ dir2;
	normal.Normalize();

	FPlane plane(c->VertexData(0), normal);

	found = 0;
	for(INT i=2; i<c->VertexData.Num() && !found; i++)
	{
		if(plane.PlaneDot(c->VertexData(i)) > LOCAL_EPS)
			found = 1;
	}
	
	if(!found)
	{
		outGeom->ConvexElems.Remove(ex);
		return;
	}
#endif

}

// Worker function for traversing collision mode/blocking volumes BSP.
// At each node, we record, the plane at this node, and carry on traversing.
// We are interested in 'inside' ie solid leafs.
static void ModelToHullsWorker(FKAggregateGeom* outGeom,
							   UModel* inModel, 
							   INT nodeIx, 
							   UBOOL bOutside, 
							   TArray<FPlane> &planes, 
							   const FVector& prePivot)
{
	FBspNode* node = &inModel->Nodes(nodeIx);

	// FRONT
	if(node->iChild[0] != INDEX_NONE) // If there is a child, recurse into it.
	{
		planes.AddItem(node->Plane);
		ModelToHullsWorker(outGeom, inModel, node->iChild[0], node->ChildOutside(0, bOutside), planes, prePivot);
		planes.Remove(planes.Num()-1);
	}
	else if(!node->ChildOutside(0, bOutside)) // If its a leaf, and solid (inside)
	{
		planes.AddItem(node->Plane);
		AddConvexPrim(outGeom, planes, inModel, prePivot);
		planes.Remove(planes.Num()-1);
	}

	// BACK
	if(node->iChild[1] != INDEX_NONE)
	{
		planes.AddItem(node->Plane.Flip());
		ModelToHullsWorker(outGeom, inModel, node->iChild[1], node->ChildOutside(1, bOutside), planes, prePivot);
		planes.Remove(planes.Num()-1);
	}
	else if(!node->ChildOutside(1, bOutside))
	{
		planes.AddItem(node->Plane.Flip());
		AddConvexPrim(outGeom, planes, inModel, prePivot);
		planes.Remove(planes.Num()-1);
	}

}

// Function to create a set of convex geometries from a UModel.
// Replaces any convex elements already in the FKAggregateGeom.
// Create it around the model origin, and applies the UNreal->Karma scaling.
void KModelToHulls(FKAggregateGeom* outGeom, UModel* inModel, const FVector& prePivot)
{
	outGeom->ConvexElems.Empty();
	
	if(!inModel)
		return;

	TArray<FPlane>	planes;
	ModelToHullsWorker(outGeom, inModel, 0, inModel->RootOutside, planes, prePivot);

}

// Util for creating has keys from pairs of actors

QWORD RBActorsToKey(AActor* a1, AActor* a2)
{
	check(sizeof(QWORD) == 2 * sizeof(AActor*));

	// Make sure m1 is the 'lower' pointer.
	if(a2 > a1)
	{
		AActor* tmp = a2;
		a2 = a1;
		a1 = tmp;
	}

	QWORD Key;
	AActor** actors = (AActor**)&Key;
	actors[0] = a1;
	actors[1] = a2;

	return Key;

}

QWORD RigidBodyIndicesToKey(INT Body1Index, INT Body2Index)
{
	check(sizeof(QWORD) == 2 * sizeof(INT));

	if(Body1Index > Body2Index)
	{
		INT tmp = Body2Index;
		Body2Index = Body1Index;
		Body1Index = tmp;
	}

	QWORD Key;
	INT* instances = (INT*)&Key;
	instances[0] = Body1Index;
	instances[1] = Body2Index;

	return Key;

}