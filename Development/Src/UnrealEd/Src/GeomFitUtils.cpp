/*=============================================================================
	GeomFitUtils.cpp: Utilities for fitting collision models to static meshes.
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

    Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"


#define LOCAL_EPS (0.01f)
static void AddVertexIfNotPresent(TArray<FVector> &vertices, FVector &newVertex)
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

/* ******************************** KDOP ******************************** */

// This function takes the current collision model, and fits a k-DOP around it.
// It uses the array of k unit-length direction vectors to define the k bounding planes.

// THIS FUNCTION REPLACES EXISTING KARMA AND COLLISION MODEL WITH KDOP
#define MY_FLTMAX (3.402823466e+38F)

void GenerateKDopAsCollisionModel(UStaticMesh* StaticMesh,TArray<FVector> &dirs)
{
	// If we already have a collision model for this staticmesh, ask if we want to replace it.
	if(StaticMesh->CollisionModel)
	{
		UBOOL doReplace = appMsgf(1, TEXT("Static Mesh already has a collision model. \nDo you want to replace it?"));
		if(doReplace)
			StaticMesh->CollisionModel = NULL;
		else
			return;
	}

	URB_BodySetup* bs = StaticMesh->BodySetup;
	if(bs)
	{
		// If we already have some karma collision for this mesh, check user want to replace it with sphere.
		int totalGeoms = 1 + bs->AggGeom.GetElementCount();
		if(totalGeoms > 0)
		{
			UBOOL doReplace = appMsgf(1, TEXT("Static Mesh already has physics collision geoemtry. \n")
				TEXT("Are you sure you want replace it with a K-DOP?"));

			if(doReplace)
				bs->AggGeom.EmptyElements();
			else
				return;
		}
	}
	else
	{
		// Otherwise, create one here.
		StaticMesh->BodySetup = ConstructObject<URB_BodySetup>(URB_BodySetup::StaticClass(), StaticMesh);
		bs = StaticMesh->BodySetup;
	}

	// Do k- specific stuff.
	INT kCount = dirs.Num();
	TArray<FLOAT> maxDist;
	for(INT i=0; i<kCount; i++)
		maxDist.AddItem(-MY_FLTMAX);

	StaticMesh->CollisionModel = new(StaticMesh->GetOuter()) UModel(NULL,1);

	// For each vertex, project along each kdop direction, to find the max in that direction.
	TArray<FStaticMeshVertex>* verts = &StaticMesh->Vertices;
	for(INT i=0; i<verts->Num(); i++)
	{
		for(INT j=0; j<kCount; j++)
		{
			FLOAT dist = (*verts)(i).Position | dirs(j);
			maxDist(j) = Max(dist, maxDist(j));
		}
	}

	// Now we have the planes of the kdop, we work out the face polygons.
	TArray<FPlane> planes;
	for(INT i=0; i<kCount; i++)
		planes.AddItem( FPlane(dirs(i), maxDist(i)) );

	for(INT i=0; i<planes.Num(); i++)
	{
		FPoly*	Polygon = new(StaticMesh->CollisionModel->Polys->Element) FPoly();
		FVector Base, AxisX, AxisY;

		Polygon->Init();
		Polygon->Normal = planes(i);
		Polygon->NumVertices = 4;
		Polygon->Normal.FindBestAxisVectors(AxisX,AxisY);

		Base = planes(i) * planes(i).W;

		Polygon->Vertex[0] = Base + AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX;
		Polygon->Vertex[1] = Base + AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX;
		Polygon->Vertex[2] = Base - AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX;
		Polygon->Vertex[3] = Base - AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX;

		for(INT j=0; j<planes.Num(); j++)
		{
			if(i != j)
			{
				if(!Polygon->Split(-FVector(planes(j)), planes(j) * planes(j).W))
				{
					Polygon->NumVertices = 0;
					break;
				}
			}
		}

		if(Polygon->NumVertices < 3)
		{
			// If poly resulted in no verts, remove from array
			StaticMesh->CollisionModel->Polys->Element.Remove(StaticMesh->CollisionModel->Polys->Element.Num()-1);
		}
		else
		{
			// Other stuff...
			Polygon->iLink = i;
			Polygon->CalcNormal(1);
		}
	}

	if(StaticMesh->CollisionModel->Polys->Element.Num() < 4)
	{
		StaticMesh->CollisionModel = NULL;
		return;
	}

	// Build bounding box.
	StaticMesh->CollisionModel->BuildBound();

	// Build BSP for the brush.
	GEditor->bspBuild(StaticMesh->CollisionModel,BSP_Good,15,70,1,0);
	GEditor->bspRefresh(StaticMesh->CollisionModel,1);
	GEditor->bspBuildBounds(StaticMesh->CollisionModel);

	KModelToHulls(&bs->AggGeom, StaticMesh->CollisionModel, FVector(0, 0, 0));
	
	// Mark staticmesh as dirty, to help make sure it gets saved.
	StaticMesh->MarkPackageDirty();
}

/* ******************************** OBB ******************************** */

// Automatically calculate the principal axis for fitting a Oriented Bounding Box to a static mesh.
// Then use k-DOP above to calculate it.
void GenerateOBBAsCollisionModel(UStaticMesh* StaticMesh)
{
}

/* ******************************** KARMA SPHERE ******************************** */

// Can do bounding circles as well... Set elements of limitVect to 1.f for directions to consider, and 0.f to not consider.
// Have 2 algorithms, seem better in different cirumstances

// This algorithm taken from Ritter, 1990
// This one seems to do well with asymmetric input.
static void CalcBoundingSphere(TArray<FStaticMeshVertex>& verts, FSphere& sphere, FVector& LimitVec)
{
	FBox Box;

	if(verts.Num() == 0)
		return;

	INT minIx[3], maxIx[3]; // Extreme points.

	// First, find AABB, remembering furthest points in each dir.
	Box.Min = verts(0).Position * LimitVec;
	Box.Max = Box.Min;

	minIx[0] = minIx[1] = minIx[2] = 0;
	maxIx[0] = maxIx[1] = maxIx[2] = 0;

	for(INT i=1; i<verts.Num(); i++) 
	{
		FVector p = verts(i).Position * LimitVec;

		// X //
		if(p.X < Box.Min.X)
		{
			Box.Min.X = p.X;
			minIx[0] = i;
		}
		else if(p.X > Box.Max.X)
		{
			Box.Max.X = p.X;
			maxIx[0] = i;
		}

		// Y //
		if(p.Y < Box.Min.Y)
		{
			Box.Min.Y = p.Y;
			minIx[1] = i;
		}
		else if(p.Y > Box.Max.Y)
		{
			Box.Max.Y = p.Y;
			maxIx[1] = i;
		}

		// Z //
		if(p.Z < Box.Min.Z)
		{
			Box.Min.Z = p.Z;
			minIx[2] = i;
		}
		else if(p.Z > Box.Max.Z)
		{
			Box.Max.Z = p.Z;
			maxIx[2] = i;
		}
	}

	//  Now find extreme points furthest apart, and initial centre and radius of sphere.
	FLOAT d2 = 0.f;
	for(INT i=0; i<3; i++)
	{
		FVector diff = (verts(maxIx[i]).Position - verts(minIx[i]).Position) * LimitVec;
		FLOAT tmpd2 = diff.SizeSquared();

		if(tmpd2 > d2)
		{
			d2 = tmpd2;
			FVector centre = verts(minIx[i]).Position + (0.5f * diff);
			centre *= LimitVec;
			sphere.X = centre.X;
			sphere.Y = centre.Y;
			sphere.Z = centre.Z;
			sphere.W = 0.f;
		}
	}

	// radius and radius squared
	FLOAT r = 0.5f * appSqrt(d2);
	FLOAT r2 = r * r;

	// Now check each point lies within this sphere. If not - expand it a bit.
	for(INT i=0; i<verts.Num(); i++) 
	{
		FVector cToP = (verts(i).Position * LimitVec) - sphere;
		FLOAT pr2 = cToP.SizeSquared();

		// If this point is outside our current bounding sphere..
		if(pr2 > r2)
		{
			// ..expand sphere just enough to include this point.
			FLOAT pr = appSqrt(pr2);
			r = 0.5f * (r + pr);
			r2 = r * r;

			sphere += (pr-r)/pr * cToP;
		}
	}

	sphere.W = r;
}

// This is the one thats already used by unreal.
// Seems to do better with more symmetric input...
static void CalcBoundingSphere2(TArray<FStaticMeshVertex>& verts, FSphere& sphere, FVector& LimitVec)
{
	FBox Box(0);
	
	for(INT i=0; i<verts.Num(); i++)
	{
		Box += verts(i).Position * LimitVec;
	}

	FVector centre, extent;
	Box.GetCenterAndExtents(centre, extent);

	sphere.X = centre.X;
	sphere.Y = centre.Y;
	sphere.Z = centre.Z;
	sphere.W = 0;

	for( INT i=0; i<verts.Num(); i++ )
	{
		FLOAT Dist = FDistSquared(verts(i).Position * LimitVec, sphere);
		if( Dist > sphere.W )
			sphere.W = Dist;
	}
	sphere.W = appSqrt(sphere.W);
}

// // //

void GenerateSphereAsKarmaCollision(UStaticMesh* StaticMesh)
{
	// If we already have a collision model for this staticmesh, ask if we want to replace it.
	if(StaticMesh->CollisionModel)
	{
		UBOOL doReplace = appMsgf(1, TEXT("Static Mesh already has a collision model. \nDo you want to replace it?"));
		if(doReplace)
			StaticMesh->CollisionModel = NULL;
		else
			return;
	}

	URB_BodySetup* bs = StaticMesh->BodySetup;
	if(bs)
	{
		// If we already have some karma collision for this mesh, check user want to replace it with sphere.
		int totalGeoms = 1 + bs->AggGeom.GetElementCount();
		if(totalGeoms > 0)
		{
			UBOOL doReplace = appMsgf(1, TEXT("Static Mesh already has physics collision geoemtry. \n")
				TEXT("Are you sure you want replace it with a sphere?"));

			if(doReplace)
				bs->AggGeom.EmptyElements();
			else
				return;
		}
	}
	else
	{
		// Otherwise, create one here.
		StaticMesh->BodySetup = ConstructObject<URB_BodySetup>(URB_BodySetup::StaticClass(), StaticMesh);
		bs = StaticMesh->BodySetup;
	}

	// Calculate bounding sphere.
	TArray<FStaticMeshVertex>* verts = &StaticMesh->Vertices;

	FSphere bSphere, bSphere2, bestSphere;
	FVector unitVec = FVector(1,1,1);
	CalcBoundingSphere(*verts, bSphere, unitVec);
	CalcBoundingSphere2(*verts, bSphere2, unitVec);

	if(bSphere.W < bSphere2.W)
		bestSphere = bSphere;
	else
		bestSphere = bSphere2;

	// Dont use if radius is zero.
	if(bestSphere.W <= 0.f)
	{
		appMsgf(0, TEXT("Could not create geometry."));
		return;
	}

	int ex = bs->AggGeom.SphereElems.AddZeroed();
	FKSphereElem* s = &bs->AggGeom.SphereElems(ex);
	s->TM = FMatrix::Identity;
	s->TM.M[3][0] = bestSphere.X;
	s->TM.M[3][1] = bestSphere.Y;
	s->TM.M[3][2] = bestSphere.Z;
	s->Radius = bestSphere.W;


	// Mark staticmesh as dirty, to help make sure it gets saved.
	StaticMesh->MarkPackageDirty();
}



/* ******************************** KARMA CYLINDER ******************************** */

 // X = 0, Y = 1, Z = 2
// THIS FUNCTION REPLACES EXISTING KARMA WITH CYLINDER, BUT DOES NOT CHANGE COLLISION MODEL

void GenerateCylinderAsKarmaCollision(UStaticMesh* StaticMesh,INT dir)
{
}