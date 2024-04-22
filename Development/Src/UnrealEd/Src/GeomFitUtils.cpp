/*=============================================================================
	GeomFitUtils.cpp: Utilities for fitting collision models to static meshes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EnginePhysicsClasses.h"
#include "BSPOps.h"

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
	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	URB_BodySetup* bs = StaticMesh->BodySetup;
	if(bs)
	{
		// If we already have some simplified collision for this mesh - check before we clobber it.
		UBOOL doReplace = appMsgf( AMT_YesNo, *LocalizeUnrealEd("Prompt_9") );

		if(doReplace)
		{
			bs->AggGeom.EmptyElements();
			bs->ClearShapeCache();
		}
		else
		{
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

	// Construct temporary UModel for kdop creation. We keep no refs to it, so it can be GC'd.
	UModel* TempModel = new UModel(NULL,1);

	// For each vertex, project along each kdop direction, to find the max in that direction.
	const FStaticMeshRenderData& RenderData = StaticMesh->LODModels(0);
	for(UINT i=0; i<RenderData.NumVertices; i++)
	{
		for(INT j=0; j<kCount; j++)
		{
			FLOAT dist = RenderData.PositionVertexBuffer.VertexPosition(i) | dirs(j);
			maxDist(j) = Max(dist, maxDist(j));
		}
	}

	// Now we have the planes of the kdop, we work out the face polygons.
	TArray<FPlane> planes;
	for(INT i=0; i<kCount; i++)
		planes.AddItem( FPlane(dirs(i), maxDist(i)) );

	for(INT i=0; i<planes.Num(); i++)
	{
		FPoly*	Polygon = new(TempModel->Polys->Element) FPoly();
		FVector Base, AxisX, AxisY;

		Polygon->Init();
		Polygon->Normal = planes(i);
		Polygon->Normal.FindBestAxisVectors(AxisX,AxisY);

		Base = planes(i) * planes(i).W;

		new(Polygon->Vertices) FVector(Base + AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX);
		new(Polygon->Vertices) FVector(Base + AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX);
		new(Polygon->Vertices) FVector(Base - AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX);
		new(Polygon->Vertices) FVector(Base - AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX);

		for(INT j=0; j<planes.Num(); j++)
		{
			if(i != j)
			{
				if(!Polygon->Split(-FVector(planes(j)), planes(j) * planes(j).W))
				{
					Polygon->Vertices.Empty();
					break;
				}
			}
		}

		if(Polygon->Vertices.Num() < 3)
		{
			// If poly resulted in no verts, remove from array
			TempModel->Polys->Element.Remove(TempModel->Polys->Element.Num()-1);
		}
		else
		{
			// Other stuff...
			Polygon->iLink = i;
			Polygon->CalcNormal(1);
		}
	}

	if(TempModel->Polys->Element.Num() < 4)
	{
		TempModel = NULL;
		return;
	}

	// Build bounding box.
	TempModel->BuildBound();

	// Build BSP for the brush.
	FBSPOps::bspBuild(TempModel,FBSPOps::BSP_Good,15,70,1,0);
	FBSPOps::bspRefresh(TempModel,1);
	FBSPOps::bspBuildBounds(TempModel);

	KModelToHulls(&bs->AggGeom, TempModel);
	
	// Mark staticmesh as dirty, to help make sure it gets saved.
	StaticMesh->MarkPackageDirty();
	GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI, StaticMesh ) );
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
static void CalcBoundingSphere(const FStaticMeshRenderData& RenderData, FSphere& sphere, FVector& LimitVec)
{
	FBox Box;

	if(RenderData.NumVertices == 0)
		return;

	INT minIx[3], maxIx[3]; // Extreme points.

	// First, find AABB, remembering furthest points in each dir.
	Box.Min = RenderData.PositionVertexBuffer.VertexPosition(0) * LimitVec;
	Box.Max = Box.Min;

	minIx[0] = minIx[1] = minIx[2] = 0;
	maxIx[0] = maxIx[1] = maxIx[2] = 0;

	for(UINT i=1; i<RenderData.NumVertices; i++) 
	{
		FVector p = RenderData.PositionVertexBuffer.VertexPosition(i) * LimitVec;

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
		FVector diff = (RenderData.PositionVertexBuffer.VertexPosition(maxIx[i]) - RenderData.PositionVertexBuffer.VertexPosition(minIx[i])) * LimitVec;
		FLOAT tmpd2 = diff.SizeSquared();

		if(tmpd2 > d2)
		{
			d2 = tmpd2;
			FVector centre = RenderData.PositionVertexBuffer.VertexPosition(minIx[i]) + (0.5f * diff);
			centre *= LimitVec;
			sphere.Center.X = centre.X;
			sphere.Center.Y = centre.Y;
			sphere.Center.Z = centre.Z;
			sphere.W = 0.f;
		}
	}

	// radius and radius squared
	FLOAT r = 0.5f * appSqrt(d2);
	FLOAT r2 = r * r;

	// Now check each point lies within this sphere. If not - expand it a bit.
	for(UINT i=0; i<RenderData.NumVertices; i++) 
	{
		FVector cToP = (RenderData.PositionVertexBuffer.VertexPosition(i) * LimitVec) - sphere.Center;
		FLOAT pr2 = cToP.SizeSquared();

		// If this point is outside our current bounding sphere..
		if(pr2 > r2)
		{
			// ..expand sphere just enough to include this point.
			FLOAT pr = appSqrt(pr2);
			r = 0.5f * (r + pr);
			r2 = r * r;

			sphere.Center += ((pr-r)/pr * cToP);
		}
	}

	sphere.W = r;
}

// This is the one thats already used by unreal.
// Seems to do better with more symmetric input...
static void CalcBoundingSphere2(const FStaticMeshRenderData& RenderData, FSphere& sphere, FVector& LimitVec)
{
	FBox Box(0);
	
	for(UINT i=0; i<RenderData.NumVertices; i++)
	{
		Box += RenderData.PositionVertexBuffer.VertexPosition(i) * LimitVec;
	}

	FVector centre, extent;
	Box.GetCenterAndExtents(centre, extent);

	sphere.Center.X = centre.X;
	sphere.Center.Y = centre.Y;
	sphere.Center.Z = centre.Z;
	sphere.W = 0;

	for( UINT i=0; i<RenderData.NumVertices; i++ )
	{
		FLOAT Dist = FDistSquared(RenderData.PositionVertexBuffer.VertexPosition(i) * LimitVec, sphere.Center);
		if( Dist > sphere.W )
			sphere.W = Dist;
	}
	sphere.W = appSqrt(sphere.W);
}

// // //

void GenerateSphereAsKarmaCollision(UStaticMesh* StaticMesh)
{
	URB_BodySetup* bs = StaticMesh->BodySetup;
	if(bs)
	{
		// If we already have some simplified collision for this mesh, check user want to replace it with sphere.
		int totalGeoms = 1 + bs->AggGeom.GetElementCount();
		if(totalGeoms > 0)
		{
			UBOOL doReplace = appMsgf( AMT_YesNo, *LocalizeUnrealEd("Prompt_9") );

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
	const FStaticMeshRenderData& RenderData = StaticMesh->LODModels(0);

	FSphere bSphere, bSphere2, bestSphere;
	FVector unitVec = FVector(1,1,1);
	CalcBoundingSphere(RenderData, bSphere, unitVec);
	CalcBoundingSphere2(RenderData, bSphere2, unitVec);

	if(bSphere.W < bSphere2.W)
		bestSphere = bSphere;
	else
		bestSphere = bSphere2;

	// Dont use if radius is zero.
	if(bestSphere.W <= 0.f)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("Prompt_10"));
		return;
	}

	int ex = bs->AggGeom.SphereElems.AddZeroed();
	FKSphereElem* s = &bs->AggGeom.SphereElems(ex);
	s->TM = FMatrix::Identity;
	s->TM.M[3][0] = bestSphere.Center.X;
	s->TM.M[3][1] = bestSphere.Center.Y;
	s->TM.M[3][2] = bestSphere.Center.Z;
	s->Radius = bestSphere.W;


	// Mark staticmesh as dirty, to help make sure it gets saved.
	StaticMesh->MarkPackageDirty();
	GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI, StaticMesh ) );
}



/* ******************************** KARMA CYLINDER ******************************** */

 // X = 0, Y = 1, Z = 2
// THIS FUNCTION REPLACES EXISTING KARMA WITH CYLINDER, BUT DOES NOT CHANGE COLLISION MODEL

void GenerateCylinderAsKarmaCollision(UStaticMesh* StaticMesh,INT dir)
{
}
