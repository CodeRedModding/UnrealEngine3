/*=============================================================================
	UnStaticMesh.cpp: Static mesh creation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Factories.h"
#include "UnTextureLayout.h"
#include "UnFracturedStaticMesh.h"
#include "EnginePhysicsClasses.h"
#include "BSPOps.h"

UBOOL GBuildStaticMeshCollision = 1;

/** Compare function used to sort non-indexed triangles by texture/polyflags. */
IMPLEMENT_COMPARE_CONSTREF( FStaticMeshTriangle, UnStaticMeshEdit, { return (A.MaterialIndex - B.MaterialIndex); } )

static FLOAT MeshToPrimTolerance = 0.001f;

/** Floating point comparitor */
static FORCEINLINE UBOOL AreEqual(FLOAT a, FLOAT b)
{
	return Abs(a - b) < MeshToPrimTolerance;
}

/** Returns 1 if vectors are parallel OR anti-parallel */
static FORCEINLINE UBOOL AreParallel(const FVector& a, const FVector& b)
{
	FLOAT Dot = a | b;

	if( AreEqual(Abs(Dot), 1.f) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

/** Utility struct used in AddBoxGeomFromTris. */
struct FPlaneInfo
{
	FVector Normal;
	INT DistCount;
	FLOAT PlaneDist[2];

	FPlaneInfo()
	{
		Normal = FVector(0.f);
		DistCount = 0;
		PlaneDist[0] = 0.f;
		PlaneDist[1] = 0.f;
	}
};

/** 
 *	Function for adding a box collision primitive to the supplied collision geometry based on the mesh of the box.
 * 
 *	We keep a list of triangle normals found so far. For each normal direction,
 *	we should have 2 distances from the origin (2 parallel box faces). If the 
 *	mesh is a box, we should have 3 distinct normal directions, and 2 distances
 *	found for each. The difference between these distances should be the box
 *	dimensions. The 3 directions give us the key axes, and therefore the
 *	box transformation matrix. This shouldn't rely on any vertex-ordering on 
 *	the triangles (normals are compared +ve & -ve). It also shouldn't matter 
 *	about how many triangles make up each side (but it will take longer). 
 *	We get the centre of the box from the centre of its AABB.
 */
void AddBoxGeomFromTris( const TArray<FPoly>& Tris, FKAggregateGeom* AggGeom, const TCHAR* ObjName )
{
	TArray<FPlaneInfo> Planes;

	for(INT i=0; i<Tris.Num(); i++)
	{
		UBOOL bFoundPlane = false;
		for(INT j=0; j<Planes.Num() && !bFoundPlane; j++)
		{
			// if this triangle plane is already known...
			if( AreParallel( Tris(i).Normal, Planes(j).Normal ) )
			{
				// Always use the same normal when comparing distances, to ensure consistent sign.
				FLOAT Dist = Tris(i).Vertices(0) | Planes(j).Normal;

				// we only have one distance, and its not that one, add it.
				if( Planes(j).DistCount == 1 && !AreEqual(Dist, Planes(j).PlaneDist[0]) )
				{
					Planes(j).PlaneDist[1] = Dist;
					Planes(j).DistCount = 2;
				}
				// if we have a second distance, and its not that either, something is wrong.
				else if( Planes(j).DistCount == 2 && !AreEqual(Dist, Planes(j).PlaneDist[1]) )
				{
					debugf(TEXT("AddBoxGeomFromTris (%s): Found more than 2 planes with different distances."), ObjName);
					return;
				}

				bFoundPlane = true;
			}
		}

		// If this triangle does not match an existing plane, add to list.
		if(!bFoundPlane)
		{
			check( Planes.Num() < Tris.Num() );

			FPlaneInfo NewPlane;
			NewPlane.Normal = Tris(i).Normal;
			NewPlane.DistCount = 1;
			NewPlane.PlaneDist[0] = Tris(i).Vertices(0) | NewPlane.Normal;
			
			Planes.AddItem(NewPlane);
		}
	}

	// Now we have our candidate planes, see if there are any problems

	// Wrong number of planes.
	if(Planes.Num() != 3)
	{
		debugf(TEXT("AddBoxGeomFromTris (%s): Not very box-like (need 3 sets of planes)."), ObjName);
		return;
	}

	// If we don't have 3 pairs, we can't carry on.
	if((Planes(0).DistCount != 2) || (Planes(1).DistCount != 2) || (Planes(2).DistCount != 2))
	{
		debugf(TEXT("AddBoxGeomFromTris (%s): Incomplete set of planes (need 2 per axis)."), ObjName);
		return;
	}

	FMatrix BoxTM = FMatrix::Identity;

	BoxTM.SetAxis(0, Planes(0).Normal);
	BoxTM.SetAxis(1, Planes(1).Normal);

	// ensure valid TM by cross-product
	FVector ZAxis = Planes(0).Normal ^ Planes(1).Normal;

	if( !AreParallel(ZAxis, Planes(2).Normal) )
	{
		debugf(TEXT("AddBoxGeomFromTris (%s): Box axes are not perpendicular."), ObjName);
		return;
	}

	BoxTM.SetAxis(2, ZAxis);

	// OBB centre == AABB centre.
	FBox Box(0);
	for(INT i=0; i<Tris.Num(); i++)
	{
		Box += Tris(i).Vertices(0);
		Box += Tris(i).Vertices(1);
		Box += Tris(i).Vertices(2);
	}

	BoxTM.SetOrigin( Box.GetCenter() );

	// Allocate box in array
	INT NewIndex = AggGeom->BoxElems.AddZeroed();
	FKBoxElem* BoxElem = &AggGeom->BoxElems(NewIndex);

	BoxElem->TM = BoxTM;
	
	// distance between parallel planes is box edge lengths.
	BoxElem->X = Abs(Planes(0).PlaneDist[0] - Planes(0).PlaneDist[1]);
	BoxElem->Y = Abs(Planes(1).PlaneDist[0] - Planes(1).PlaneDist[1]);
	BoxElem->Z = Abs(Planes(2).PlaneDist[0] - Planes(2).PlaneDist[1]);
}

/**
 *	Function for adding a sphere collision primitive to the supplied collision geometry based on a set of Verts.
 *	
 *	Simply put an AABB around mesh and use that to generate centre and radius.
 *	It checks that the AABB is square, and that all vertices are either at the
 *	centre, or within 5% of the radius distance away.
 */
void AddSphereGeomFromVerts( const TArray<FVector>& Verts, FKAggregateGeom* AggGeom, const TCHAR* ObjName )
{
	if(Verts.Num() == 0)
	{
		return;
	}

	FBox Box(0);

	for(INT i=0; i<Verts.Num(); i++)
	{
		Box += Verts(i);
	}

	FVector Center, Extents;
	Box.GetCenterAndExtents(Center, Extents);
	FLOAT Longest = 2.f * Extents.GetMax();
	FLOAT Shortest = 2.f * Extents.GetMin();

	// check that the AABB is roughly a square (5% tolerance)
	if((Longest - Shortest)/Longest > 0.05f)
	{
		debugf(TEXT("AddSphereGeomFromVerts (%s): Sphere bounding box not square."), ObjName);
		return;
	}

	FMatrix SphereTM = FMatrix::Identity;
	SphereTM.SetOrigin(Center);

	FLOAT Radius = 0.5f * Longest;

	// Test that all vertices are a similar radius (5%) from the sphere centre.
	FLOAT MaxR = 0;
	FLOAT MinR = BIG_NUMBER;
	for(INT i=0; i<Verts.Num(); i++)
	{
		FVector CToV = Verts(i) - Center;
		FLOAT RSqr = CToV.SizeSquared();

		MaxR = ::Max(RSqr, MaxR);

		// Sometimes vertex at centre, so reject it.
		if(RSqr > KINDA_SMALL_NUMBER)
		{
			MinR = ::Min(RSqr, MinR);
		}
	}

	MaxR = appSqrt(MaxR);
	MinR = appSqrt(MinR);

	if((MaxR-MinR)/Radius > 0.05f)
	{
		debugf(TEXT("AddSphereGeomFromVerts (%s): Vertices not at constant radius."), ObjName );
		return;
	}

	// Allocate sphere in array
	INT NewIndex = AggGeom->SphereElems.AddZeroed();
	FKSphereElem* SphereElem = &AggGeom->SphereElems(NewIndex);

	SphereElem->TM = SphereTM;

	SphereElem->Radius = Radius;
}

/** Utility for adding one convex hull from the given verts */
void AddConvexGeomFromVertices( const TArray<FVector>& Verts, FKAggregateGeom* AggGeom, const TCHAR* ObjName )
{
	if(Verts.Num() == 0)
	{
		return;
	}

	FKConvexElem* ConvexElem = new(AggGeom->ConvexElems) FKConvexElem();
	appMemzero(ConvexElem, sizeof(FKConvexElem));
	ConvexElem->VertexData = Verts;
	ConvexElem->GenerateHullData();
}

/**
 * Creates a static mesh object from raw triangle data.
 */
UStaticMesh* CreateStaticMesh(TArray<FStaticMeshTriangle>& Triangles,TArray<FStaticMeshElement>& Materials,UObject* InOuter,FName InName)
{
	const UPackage* Package = InOuter->GetOutermost();
	if ( Package->PackageFlags & PKG_Cooked )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
		return NULL;
	}

	// Create the UStaticMesh object.
	FStaticMeshComponentReattachContext	ComponentReattachContext(FindObject<UStaticMesh>(InOuter,*InName.ToString()));
	UStaticMesh*						StaticMesh = new(InOuter,InName,RF_Public|RF_Standalone) UStaticMesh;

	// Add one LOD for the base mesh
	new(StaticMesh->LODModels) FStaticMeshRenderData();
	StaticMesh->LODInfo.AddItem(FStaticMeshLODInfo());

	Sort<USE_COMPARE_CONSTREF(FStaticMeshTriangle,UnStaticMeshEdit)>(&Triangles(0),Triangles.Num());

	StaticMesh->LODModels(0).RawTriangles.RemoveBulkData();
	StaticMesh->LODModels(0).RawTriangles.Lock(LOCK_READ_WRITE);
	void* RawTriangleData = StaticMesh->LODModels(0).RawTriangles.Realloc(Triangles.Num());
	check( StaticMesh->LODModels(0).RawTriangles.GetBulkDataSize() == Triangles.Num() * Triangles.GetTypeSize() );
	appMemcpy( RawTriangleData, Triangles.GetData(), StaticMesh->LODModels(0).RawTriangles.GetBulkDataSize() );
	StaticMesh->LODModels(0).RawTriangles.Unlock();

	for(UINT MaterialIndex = 0;MaterialIndex < (UINT)Materials.Num();MaterialIndex++)
	{
		new(StaticMesh->LODModels(0).Elements) FStaticMeshElement(Materials(MaterialIndex));
	}

	StaticMesh->Build();
	StaticMesh->MarkPackageDirty();
	return StaticMesh;

}


/**
 *Constructor, setting all values to usable defaults 
 */
FMergeStaticMeshParams::FMergeStaticMeshParams()
	: Offset(0.0f, 0.0f, 0.0f)
	, Rotation(0, 0, 0)
	, ScaleFactor(1.0f)
	, ScaleFactor3D(1.0f, 1.0f, 1.0f)
	, bDeferBuild(FALSE)
	, OverrideElement(INDEX_NONE)
	, bUseUVChannelRemapping(FALSE)
	, bUseUVScaleBias(FALSE)
{
	// initialize some UV channel arrays
	for (INT Channel = 0; Channel < ARRAY_COUNT(UVChannelRemap); Channel++)
	{
		// we can't just map channel to channel by default, because we need to know when a UV channel is
		// actually being redirected in to, so that we can update Triangle.NumUVs
		UVChannelRemap[Channel] = INDEX_NONE;

		// default to a noop scale/bias
		UVScaleBias[Channel] = FVector4(1.0f, 1.0f, 0.0f, 0.0f);
	}
}

/**
 * Merges SourceMesh into DestMesh, applying transforms along the way
 *
 * @param DestMesh The static mesh that will have SourceMesh merged into
 * @param SourceMesh The static mesh to merge in to DestMesh
 * @param Params Settings for the merge
 */
void MergeStaticMesh(UStaticMesh* DestMesh, UStaticMesh* SourceMesh, const FMergeStaticMeshParams& Params)
{
	check(DestMesh && SourceMesh);
	check(DestMesh != SourceMesh);
	check(SourceMesh->LODModels.Num() > 0);
	check(SourceMesh->LODModels.Num() == SourceMesh->LODInfo.Num());
	check(!SourceMesh->IsA(UFracturedStaticMesh::StaticClass()));
	check(DestMesh->LODModels.Num() > 0);
	check(DestMesh->LODModels.Num() == DestMesh->LODInfo.Num());

	//if the destination mesh is a fractured mesh it needs to be handled appropriately
	UFracturedStaticMesh* DestFracturedMesh = Cast<UFracturedStaticMesh>(DestMesh);

	//components of DestMesh need to be reattached
	FStaticMeshComponentReattachContext	ComponentReattachContext(DestMesh);

	//handle every LOD in the source mesh
	for (INT LODIndex = 0; LODIndex < SourceMesh->LODModels.Num(); LODIndex++)
	{
		FStaticMeshRenderData& SourceRenderData = SourceMesh->LODModels(LODIndex);

		//add a new LOD to the destination mesh if needed
		if (LODIndex >= DestMesh->LODModels.Num())
		{
			DestMesh->LODModels.AddRawItem(new FStaticMeshRenderData);
			DestMesh->LODInfo.AddZeroed();
		}
		FStaticMeshRenderData& DestRenderData = DestMesh->LODModels(LODIndex);

		const UINT NumOriginalTriangles = DestRenderData.RawTriangles.GetElementCount();
		const UINT NumSourceTriangles = SourceRenderData.RawTriangles.GetElementCount();
		const UINT NumMergedTriangles = NumOriginalTriangles + NumSourceTriangles;
		const UINT NumOriginalElements = DestRenderData.Elements.Num();

		//copy raw triangle data from the source mesh
		check(SourceRenderData.RawTriangles.GetElementSize() == sizeof(FStaticMeshTriangle));
		FStaticMeshTriangle* SourceRawTriangleData = (FStaticMeshTriangle*)SourceRenderData.RawTriangles.Lock(LOCK_READ_ONLY);
		TArray<FStaticMeshTriangle> TempSourceRawTriangles;
		TempSourceRawTriangles.Add(NumSourceTriangles);
		appMemcpy(TempSourceRawTriangles.GetData(), SourceRawTriangleData, NumSourceTriangles * sizeof(FStaticMeshTriangle));
		SourceRenderData.RawTriangles.Unlock();

		// transform verts and update the source mesh triangles to have their own MaterialIndex range
		const FMatrix MeshTM = FScaleRotationTranslationMatrix( Params.ScaleFactor * Params.ScaleFactor3D, Params.Rotation, Params.Offset );
		const FMatrix MeshTA = MeshTM.TransposeAdjoint();

		// calculate the final 3d scale factor
		FVector FinalScale3D = Params.ScaleFactor * Params.ScaleFactor3D;
		// we need to flip winding if there's a negative scale overall
		UBOOL bNeedsWindingFlip = FinalScale3D.X * FinalScale3D.Y * FinalScale3D.Z < 0;

		for(INT TriangleIndex = 0;TriangleIndex < TempSourceRawTriangles.Num();TriangleIndex++)
		{
			FStaticMeshTriangle& Triangle = TempSourceRawTriangles(TriangleIndex);

			if (Params.OverrideElement != INDEX_NONE)
			{
				Triangle.MaterialIndex = Params.OverrideElement;
			}
			else
			{
				Triangle.MaterialIndex += NumOriginalElements;
			}

			// flip winding if needed
			if (bNeedsWindingFlip)
			{
				Swap(Triangle.Vertices[1], Triangle.Vertices[2]);
				Swap(Triangle.Colors[1], Triangle.Colors[2]);
				Swap(Triangle.TangentX[1], Triangle.TangentX[2]);
				Swap(Triangle.TangentY[1], Triangle.TangentY[2]);
				Swap(Triangle.TangentZ[1], Triangle.TangentZ[2]);

				// array of UVs needs special attn
				FVector2D TempUVs[8];
				appMemcpy(TempUVs, Triangle.UVs[1], sizeof(TempUVs));
				appMemcpy(Triangle.UVs[1], Triangle.UVs[2], sizeof(TempUVs));
				appMemcpy(Triangle.UVs[2], TempUVs, sizeof(TempUVs));
			}

			for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			{
				Triangle.Vertices[VertexIndex] = MeshTM.TransformFVector(Triangle.Vertices[VertexIndex]);

				Triangle.TangentX[VertexIndex] = MeshTA.TransformNormal(Triangle.TangentX[VertexIndex]);
				Triangle.TangentY[VertexIndex] = MeshTA.TransformNormal(Triangle.TangentY[VertexIndex]);
				Triangle.TangentZ[VertexIndex] = MeshTA.TransformNormal(Triangle.TangentZ[VertexIndex]);

				// remap UVs if specified
				if (Params.bUseUVChannelRemapping)
				{
					FVector2D NewUVs[ARRAY_COUNT(Params.UVChannelRemap)];
					// remap old channels into new channels
					for (INT Channel = 0; Channel < ARRAY_COUNT(Params.UVChannelRemap); Channel++)
					{
						INT SourceChannel = Params.UVChannelRemap[Channel];
						// apply the remap if it's specified
						if (SourceChannel == INDEX_NONE)
						{
							NewUVs[Channel] = Triangle.UVs[VertexIndex][Channel];
						}
						else
						{
							NewUVs[Channel] = Triangle.UVs[VertexIndex][SourceChannel];
							
							// if we have a remapping into a channel, we assume it's used, so update the
							// number of UVs valid on this triangle (otherwise it will never be used in the
							// shader)
							Triangle.NumUVs = Max(Triangle.NumUVs, Channel + 1);
						}
					}

					// copy back over the source channels
					appMemcpy(Triangle.UVs[VertexIndex], NewUVs, sizeof(NewUVs));
				}

				// modify UV if specified
				if (Params.bUseUVScaleBias)
				{
					for (INT Channel = 0; Channel < ARRAY_COUNT(Params.UVScaleBias); Channel++)
					{
						Triangle.UVs[VertexIndex][Channel] = Triangle.UVs[VertexIndex][Channel] * 
							FVector2D(Params.UVScaleBias[Channel].X, Params.UVScaleBias[Channel].Y) + 
							FVector2D(Params.UVScaleBias[Channel].Z, Params.UVScaleBias[Channel].W);
					}
				}
			}
		}

		//place the new triangles in a unique fragment
		if (DestFracturedMesh)
		{
			const UINT NumOriginalFragments = DestFracturedMesh->GetNumFragments();
			for(INT TriangleIndex = 0;TriangleIndex < TempSourceRawTriangles.Num();TriangleIndex++)
			{
				TempSourceRawTriangles(TriangleIndex).FragmentIndex = NumOriginalFragments;
			}
		}

		//resize destination raw triangles and copy the new triangles in
		DestRenderData.RawTriangles.Lock(LOCK_READ_WRITE);
		FStaticMeshTriangle* MergedRawTriangleData = (FStaticMeshTriangle*)DestRenderData.RawTriangles.Realloc(NumMergedTriangles);
		appMemcpy(MergedRawTriangleData + NumOriginalTriangles, TempSourceRawTriangles.GetData(), TempSourceRawTriangles.Num() * TempSourceRawTriangles.GetTypeSize());
		DestRenderData.RawTriangles.Unlock();

		if (Params.OverrideElement == INDEX_NONE)
		{
			//add the source mesh's elements to the destination mesh
			for(INT MaterialIndex = 0;MaterialIndex < SourceRenderData.Elements.Num();MaterialIndex++)
			{
				DestRenderData.Elements.AddItem(FStaticMeshElement(SourceRenderData.Elements(MaterialIndex).Material, NumOriginalElements + MaterialIndex));
				//setup the number of fragments in the built mesh
				if (DestFracturedMesh)
				{
					const UINT NumOriginalFragments = DestFracturedMesh->GetNumFragments();
					DestRenderData.Elements.Last().Fragments.Empty(NumOriginalFragments + 1);
					DestRenderData.Elements.Last().Fragments.AddZeroed(NumOriginalFragments + 1);
				}
			}

			//update the destination mesh's LODInfos
			for(INT InfoMaterialIndex = 0;InfoMaterialIndex < SourceMesh->LODInfo(LODIndex).Elements.Num();InfoMaterialIndex++)
			{
				DestMesh->LODInfo(LODIndex).Elements.AddItem(SourceMesh->LODInfo(LODIndex).Elements(InfoMaterialIndex));
			}
		}
	}

	if (DestFracturedMesh)
	{
		TArray<FFragmentInfo> TempFragments = DestFracturedMesh->GetFragments();
		//set the fragment index of the core
		DestFracturedMesh->CoreFragmentIndex = TempFragments.Num();
		//@todo: actually generate the needed information here
		FKConvexElem NewConvexElem;
		appMemzero(&NewConvexElem, sizeof(FKConvexElem));
		TArray<BYTE> TempNeighbours;
		TArray<FLOAT> TempNeighbourDims;
		TempFragments.AddItem(FFragmentInfo(SourceMesh->Bounds.Origin, NewConvexElem, TempNeighbours, TempNeighbourDims, FALSE, FALSE, FALSE, FVector(0,0,0)));
		DestFracturedMesh->Fragments = TempFragments;

		// Save pointer to core mesh
		DestFracturedMesh->SourceCoreMesh = SourceMesh;
		DestFracturedMesh->CoreMeshScale = Params.ScaleFactor;
		DestFracturedMesh->CoreMeshScale3D = Params.ScaleFactor3D;
		DestFracturedMesh->CoreMeshOffset = Params.Offset;
		DestFracturedMesh->CoreMeshRotation = Params.Rotation;
	}

	//build the resulting merged mesh
	if(!Params.bDeferBuild)
	{
		DestMesh->Build();	
	}
	
	DestMesh->MarkPackageDirty();
}

//
//	FVerticesEqual
//

inline UBOOL FVerticesEqual(FVector& V1,FVector& V2)
{
	if(Abs(V1.X - V2.X) > THRESH_POINTS_ARE_SAME * 4.0f)
	{
		return 0;
	}

	if(Abs(V1.Y - V2.Y) > THRESH_POINTS_ARE_SAME * 4.0f)
	{
		return 0;
	}

	if(Abs(V1.Z - V2.Z) > THRESH_POINTS_ARE_SAME * 4.0f)
	{
		return 0;
	}

	return 1;
}

INT FindMaterialIndex(TArray<FStaticMeshElement>& Materials,UMaterialInterface* Material)
{
	for(INT MaterialIndex = 0;MaterialIndex < Materials.Num();MaterialIndex++)
	{
		if(Materials(MaterialIndex).Material == Material)
		{
			return MaterialIndex;
		}
	}

	const INT NewMaterialIndex = Materials.Num();
	new(Materials) FStaticMeshElement(Material,NewMaterialIndex);

	return NewMaterialIndex;
}

void GetBrushTriangles(TArray<FStaticMeshTriangle>& Triangles,TArray<FStaticMeshElement>& Materials,AActor* Brush,UModel* Model )
{
	// Calculate the local to world transform for the source brush.

	FMatrix	LocalToWorld = Brush ? Brush->LocalToWorld() : FMatrix::Identity;
	UBOOL	ReverseVertices = 0;
	FVector4	PostSub = Brush ? Brush->Location : FVector4(0,0,0,0);


	INT NumPolys = Model->Polys->Element.Num();

	// For each polygon in the model...
	for( INT PolygonIndex = 0; PolygonIndex < NumPolys; ++PolygonIndex )
	{
		FPoly& Polygon = Model->Polys->Element(PolygonIndex);

		UMaterialInterface*	Material = Polygon.Material;

		// Find a material index for this polygon.

		INT	MaterialIndex = FindMaterialIndex(Materials,Material);

		// Cache the texture coordinate system for this polygon.

		FVector	TextureBase = Polygon.Base - (Brush ? Brush->PrePivot : FVector(0,0,0)),
				TextureX = Polygon.TextureU / 128.0f,
				TextureY = Polygon.TextureV / 128.0f;

		// For each vertex after the first two vertices...

		for(INT VertexIndex = 2;VertexIndex < Polygon.Vertices.Num();VertexIndex++)
		{
			// Create a triangle for the vertex.

			FStaticMeshTriangle*	Triangle = new(Triangles) FStaticMeshTriangle;

			Triangle->MaterialIndex = MaterialIndex;
			Triangle->FragmentIndex = 0;

			// Generate different smoothing mask for each poly to give the mesh hard edges.  Note: only 32 smoothing masks supported.
			Triangle->SmoothingMask = 1<<(PolygonIndex%32);
			Triangle->NumUVs = 1;

			Triangle->Vertices[ReverseVertices ? 0 : 2] = LocalToWorld.TransformFVector(Polygon.Vertices(0)) - PostSub;
			Triangle->UVs[ReverseVertices ? 0 : 2][0].X = (Triangle->Vertices[ReverseVertices ? 0 : 2] - TextureBase) | TextureX;
			Triangle->UVs[ReverseVertices ? 0 : 2][0].Y = (Triangle->Vertices[ReverseVertices ? 0 : 2] - TextureBase) | TextureY;
			Triangle->Colors[0] = FColor(255,255,255,255);

			Triangle->Vertices[1] = LocalToWorld.TransformFVector(Polygon.Vertices(VertexIndex - 1)) - PostSub;
			Triangle->UVs[1][0].X = (Triangle->Vertices[1] - TextureBase) | TextureX;
			Triangle->UVs[1][0].Y = (Triangle->Vertices[1] - TextureBase) | TextureY;
			Triangle->Colors[1] = FColor(255,255,255,255);

			Triangle->Vertices[ReverseVertices ? 2 : 0] = LocalToWorld.TransformFVector(Polygon.Vertices(VertexIndex)) - PostSub;
			Triangle->UVs[ReverseVertices ? 2 : 0][0].X = (Triangle->Vertices[ReverseVertices ? 2 : 0] - TextureBase) | TextureX;
			Triangle->UVs[ReverseVertices ? 2 : 0][0].Y = (Triangle->Vertices[ReverseVertices ? 2 : 0] - TextureBase) | TextureY;
			Triangle->Colors[2] = FColor(255,255,255,255);
		
			Triangle->bOverrideTangentBasis = FALSE;
			Triangle->bExplicitNormals = FALSE;
			for( INT NormalIndex = 0; NormalIndex < 3; ++NormalIndex )
			{
				Triangle->TangentX[ NormalIndex ] = FVector( 0.0f, 0.0f, 0.0f );
				Triangle->TangentY[ NormalIndex ] = FVector( 0.0f, 0.0f, 0.0f );
				Triangle->TangentZ[ NormalIndex ] = FVector( 0.0f, 0.0f, 0.0f );
			}
		}
	}

	// Merge vertices within a certain distance of each other.

	for(INT TriangleIndex = 0;TriangleIndex < Triangles.Num();TriangleIndex++)
	{
		FStaticMeshTriangle&	Triangle = Triangles(TriangleIndex);
		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
		{
			FVector&	Vertex = Triangle.Vertices[VertexIndex];

			for(INT OtherTriangleIndex = 0;OtherTriangleIndex < Triangles.Num();OtherTriangleIndex++)
			{
				FStaticMeshTriangle&	OtherTriangle = Triangles(OtherTriangleIndex);

				for(INT OtherVertexIndex = 0;OtherVertexIndex < 3;OtherVertexIndex++)
				{
					if(FVerticesEqual(Vertex,OtherTriangle.Vertices[OtherVertexIndex]))
						Vertex = OtherTriangle.Vertices[OtherVertexIndex];
				}
			}
		}
	}

}

/**
 * CreateStaticMeshFromBrush - Creates a static mesh from the triangles in a model.
 * 
 * @param Outer				- Outer for the new Static Mesh.
 * @param Name				- Name for the new Static Mesh.
 * @param Brush				- Used to orientate the new static mesh (NULL will result in the origin being assumed to be the pivot).
 * @param Model				- Model the new static mesh is build from.
 * @param MoveToLocalSpace	- If true and no brush is provided the function will assume that the current selections pivot is the new meshes pivot.
 *
 * @return Created static mesh.
 */
UStaticMesh* CreateStaticMeshFromBrush(UObject* Outer,FName Name,ABrush* Brush,UModel* Model, UBOOL MoveToLocalSpace)
{
	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("CreatingStaticMeshE")), TRUE );

	TArray<FStaticMeshTriangle>	Triangles;
	TArray<FStaticMeshElement>	Materials;
	GetBrushTriangles(Triangles,Materials,Brush,Model);
	
	// GetBrushTriangles doesn't move the verts into locals space unless its given a brush. 
	// If thats the case we have made an assumption that The current selections pivot point
	// is the brushes location. 
	if (MoveToLocalSpace && !Brush)
	{
		const FVector PivotLocation(GEditorModeTools().PivotLocation);
		for( INT x = 0 ; x < Triangles.Num() ; ++x )
		{
			FStaticMeshTriangle* Tri = &Triangles(x);

			Tri->Vertices[0] -= PivotLocation;
			Tri->Vertices[1] -= PivotLocation;
			Tri->Vertices[2] -= PivotLocation;
		}
	}

	UStaticMesh*	StaticMesh = CreateStaticMesh(Triangles,Materials,Outer,Name);

	GWarn->EndSlowTask();

	return StaticMesh;
}

/**
 * Creates a model from the triangles in a static mesh.
 */
void CreateModelFromStaticMesh(UModel* Model,AStaticMeshActor* StaticMeshActor)
{
	UStaticMesh*	StaticMesh = StaticMeshActor->StaticMeshComponent->StaticMesh;
	FMatrix			LocalToWorld = StaticMeshActor->LocalToWorld();

	Model->Polys->Element.Empty();

	const FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) StaticMesh->LODModels(0).RawTriangles.Lock(LOCK_READ_ONLY);
	if(StaticMesh->LODModels(0).RawTriangles.GetElementCount())
	{
		for(INT TriangleIndex = 0;TriangleIndex < StaticMesh->LODModels(0).RawTriangles.GetElementCount();TriangleIndex++)
		{
			const FStaticMeshTriangle&	Triangle	= RawTriangleData[TriangleIndex];
			FPoly*						Polygon		= new(Model->Polys->Element) FPoly;

			Polygon->Init();
			Polygon->iLink = Polygon - &Model->Polys->Element(0);
			Polygon->Material = StaticMesh->LODModels(0).Elements(Triangle.MaterialIndex).Material;
			Polygon->PolyFlags = PF_DefaultFlags;
			Polygon->SmoothingMask = Triangle.SmoothingMask;

			new(Polygon->Vertices) FVector(LocalToWorld.TransformFVector(Triangle.Vertices[2]));
			new(Polygon->Vertices) FVector(LocalToWorld.TransformFVector(Triangle.Vertices[1]));
			new(Polygon->Vertices) FVector(LocalToWorld.TransformFVector(Triangle.Vertices[0]));

			Polygon->CalcNormal(1);
			Polygon->Finalize(NULL,0);
			FTexCoordsToVectors(Polygon->Vertices(2),FVector(Triangle.UVs[0][0].X * 128.0f,Triangle.UVs[0][0].Y * 128.0f,1),
								Polygon->Vertices(1),FVector(Triangle.UVs[1][0].X * 128.0f,Triangle.UVs[1][0].Y * 128.0f,1),
								Polygon->Vertices(0),FVector(Triangle.UVs[2][0].X * 128.0f,Triangle.UVs[2][0].Y * 128.0f,1),
								&Polygon->Base,&Polygon->TextureU,&Polygon->TextureV);
		}
	}
	StaticMesh->LODModels(0).RawTriangles.Unlock();

	Model->Linked = 1;
	FBSPOps::bspValidateBrush(Model,0,1);
	Model->BuildBound();
}

void UStaticMeshFactory::StaticConstructor()
{
	new(GetClass(),TEXT("Pitch"),RF_Public) UIntProperty(CPP_PROPERTY(Pitch),TEXT("Import"),0);
	new(GetClass(),TEXT("Roll"),RF_Public) UIntProperty(CPP_PROPERTY(Roll),TEXT("Import"),0);
	new(GetClass(),TEXT("Yaw"),RF_Public) UIntProperty(CPP_PROPERTY(Yaw),TEXT("Import"),0);
	new(GetClass(),TEXT("bOneConvexPerUCXObject"),RF_Public) UBoolProperty(CPP_PROPERTY(bOneConvexPerUCXObject),TEXT(""),CPF_Edit);
	new(GetClass(),TEXT("bSingleSmoothGroupSingleTangent"),RF_Public) UBoolProperty(CPP_PROPERTY(bSingleSmoothGroupSingleTangent),TEXT(""),CPF_Edit);
	new(GetClass(),TEXT("bReplaceExistingVertexColors"), RF_Public) UBoolProperty(CPP_PROPERTY(bReplaceVertexColors),TEXT(""),CPF_Edit);

	// Hide the object category
	new(GetClass()->HideCategories) FName(NAME_Object);
}

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UStaticMeshFactory::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UStaticMesh::StaticClass();
	new(Formats)FString(TEXT("t3d;Static Mesh"));
#if WITH_ACTORX
	new(Formats)FString(TEXT("ase;Static Mesh"));
#endif
	bCreateNew = 0;
	bText = 1;
}
UStaticMeshFactory::UStaticMeshFactory()
{
	bEditorImport = 1;
}

static void TransformPolys(UPolys* Polys,const FMatrix& Matrix)
{
	for(INT PolygonIndex = 0;PolygonIndex < Polys->Element.Num();PolygonIndex++)
	{
		FPoly&	Polygon = Polys->Element(PolygonIndex);

		for(INT VertexIndex = 0;VertexIndex < Polygon.Vertices.Num();VertexIndex++)
			Polygon.Vertices(VertexIndex) = Matrix.TransformFVector( Polygon.Vertices(VertexIndex) );

		Polygon.Base		= Matrix.TransformFVector( Polygon.Base		);
		Polygon.TextureU	= Matrix.TransformFVector( Polygon.TextureU );
		Polygon.TextureV	= Matrix.TransformFVector( Polygon.TextureV	);
	}

}

struct FASEMaterialInfo
{
	enum { MAX_NAME_LEN=128 };

	FASEMaterialInfo()
	{
		Width = Height = 256;
		UTiling = VTiling = 1;
		Material = NULL;
	}
	FASEMaterialInfo( TCHAR* InName, INT InWidth, INT InHeight, FLOAT InUTiling, FLOAT InVTiling, UMaterialInterface* InMaterial )
	{
		appStrncpy( Name, InName, MAX_NAME_LEN );
		Width = InWidth;
		Height = InHeight;
		UTiling = InUTiling;
		VTiling = InVTiling;
		Material = InMaterial;
	}

	TCHAR Name[MAX_NAME_LEN];
	INT Width, Height;
	FLOAT UTiling, VTiling;
	UMaterialInterface* Material;
};

struct FASEMaterialHeaderInfo
{
	TArray<FASEMaterialInfo> Materials;
};

struct FASEMappingChannel
{
	TArray<FVector2D>	TexCoord;
	TArray<INT>			FaceTexCoordIdx;
};

struct FASEConnectivityVertex
{
	FVector				Position;
	TArray<INT>			Triangles;

	/** Constructor */
	FASEConnectivityVertex( const FVector &v )
		: Position( v )
	{
	}

	/** Check if this vertex is in the same place as given point */
	FORCEINLINE UBOOL IsSame( const FVector &v )
	{
		const FLOAT eps = 0.01f;
		return Abs( v.X - Position.X ) < eps && 
		       Abs( v.Y - Position.Y ) < eps &&
		       Abs( v.Z - Position.Z ) < eps;
	}
	
	/** Add link to triangle */
	FORCEINLINE VOID AddTriangleLink( INT Triangle )
	{
		Triangles.AddItem( Triangle );
	}
};

struct FASEConnectivityTriangle
{
	INT					Vertices[3];
	INT					Group;

	/** Constructor */
	FASEConnectivityTriangle( INT a, INT b, INT c )
		: Group( INDEX_NONE )
	{
		Vertices[0] = a;
		Vertices[1] = b;
		Vertices[2] = c;
	}
};

struct FASEConnectivityGroup
{
	TArray<INT>		Triangles;
};

class FASEConnectivityBuilder
{
public:
	TArray<FASEConnectivityVertex>		Vertices;
	TArray<FASEConnectivityTriangle>	Triangles;
	TArray<FASEConnectivityGroup>		Groups;

public:
	/** Add vertex to connectivity information */
	INT AddVertex( const FVector &v )
	{
		// Try to find existing vertex
		// TODO: should use hash map
		for ( INT i=0; i<Vertices.Num(); ++i )
		{
			if ( Vertices(i).IsSame( v ) )
			{
				return i;
			}
		}

		// Add new vertex
		new ( Vertices ) FASEConnectivityVertex( v );
		return Vertices.Num() - 1;
	}

	/** Add triangle to connectivity information */
	INT AddTriangle( const FVector &a, const FVector &b, const FVector &c )
	{
		// Map vertices
		INT VertexA = AddVertex( a );
		INT VertexB = AddVertex( b );
		INT VertexC = AddVertex( c );

		// Make sure triangle is not degenerated
		if ( VertexA!=VertexB && VertexB!=VertexC && VertexC!=VertexA )
		{
			// Setup connectivity info
			INT TriangleIndex = Triangles.Num();
			Vertices( VertexA ).AddTriangleLink( TriangleIndex );
			Vertices( VertexB ).AddTriangleLink( TriangleIndex );
			Vertices( VertexC ).AddTriangleLink( TriangleIndex );

			// Create triangle
			new ( Triangles ) FASEConnectivityTriangle( VertexA, VertexB, VertexC );
			return TriangleIndex;
		}
		else
		{
			// Degenerated triangle
			return INDEX_NONE;
		}
	}

	/** Create connectivity groups */
	VOID CreateConnectivityGroups()
	{
		// Delete group list
		Groups.Empty();

		// Reset group assignments
		for ( INT i=0; i<Triangles.Num(); i++ )
		{
			Triangles(i).Group = INDEX_NONE;
		}

		// Flood fill using connectivity info
		for ( ;; )
		{
			// Find first triangle without group assignment
			INT InitialTriangle = INDEX_NONE;
			for ( INT i=0; i<Triangles.Num(); i++ )
			{
				if ( Triangles(i).Group == INDEX_NONE )
				{
					InitialTriangle = i;
					break;
				}
			}

			// No more unassigned triangles, flood fill is done
			if ( InitialTriangle == INDEX_NONE )
			{
				break;
			}

			// Create group
			INT GroupIndex = Groups.AddZeroed( 1 );

			// Start flood fill using connectivity information
			FloodFillTriangleGroups( InitialTriangle, GroupIndex );
		}
	}

private:
	/** FloodFill core */
	VOID FloodFillTriangleGroups( INT InitialTriangleIndex, INT GroupIndex )
	{
		TArray<INT> TriangleStack;

		// Start with given triangle
		TriangleStack.AddItem( InitialTriangleIndex );

		// Set the group for our first triangle
		Triangles( InitialTriangleIndex ).Group = GroupIndex;

		// Process until we have triangles in stack
		while ( TriangleStack.Num() )
		{
			// Pop triangle index from stack
			INT TriangleIndex = TriangleStack.Pop();

			FASEConnectivityTriangle &Triangle = Triangles( TriangleIndex );

			// All triangles should already have a group before we start processing neighbors
			checkSlow( Triangle.Group == GroupIndex );

			// Add to list of triangles in group
			Groups( GroupIndex ).Triangles.AddItem( TriangleIndex );

			// Recurse to all other triangles connected with this one
			for ( INT i=0; i<3; i++ )
			{
				INT VertexIndex = Triangle.Vertices[ i ];
				const FASEConnectivityVertex &Vertex = Vertices( VertexIndex );

				for ( INT j=0; j<Vertex.Triangles.Num(); j++ )
				{
					INT OtherTriangleIndex = Vertex.Triangles( j );
					FASEConnectivityTriangle &OtherTriangle = Triangles( OtherTriangleIndex );

					// Only recurse if triangle was not already assigned to a group
					if ( OtherTriangle.Group == INDEX_NONE )
					{
						// OK, the other triangle now belongs to our group!
						OtherTriangle.Group = GroupIndex;

						// Add the other triangle to the stack to be processed
						TriangleStack.AddItem( OtherTriangleIndex );
					}
				}
			}
		}
	}
};

enum EGeomObjType
{
	GOT_Graphics,
	GOT_Convex,
	GOT_Box,
	GOT_Sphere
};

struct ExistingStaticMeshData
{
	TArray<UMaterialInterface*>	ExistingElementMaterial;
	TArray<UBOOL>				ExistingElementCollision;
	TArray<UBOOL>				ExistingElementShadowCasting;

	UBOOL						ExistingUseSimpleLineCollision;
	UBOOL						ExistingUseSimpleBoxCollision;
	UBOOL						ExistingUseSimpleRigidBodyCollision;
	UBOOL						ExistingUseFullPrecisionUVs;
	UBOOL						ExistingUsedForInstancing;
	UBOOL						ExistingUseMaximumStreamingTexelRatio;
	UBOOL						ExistingPartitionForEdgeGeometry;
	UBOOL						ExistingCanBecomeDynamic;
	UBOOL						ExistingStripkDOPForConsole;

	INT							ExistingLightMapResolution;
	INT							ExistingLightMapCoordinateIndex;
	INT							ExistingVertexPositionVersionNumber;

	FRotator					ExistingThumbnailAngle;
	FLOAT						ExistingThumbnailDistance;

	UModel*						ExistingCollisionModel;
	URB_BodySetup*				ExistingBodySetup;

	FLOAT						ExistingStreamingDistanceMultiplier;
	FLOAT						ExistingLODMaxRange;
	FLOAT						ExistingLODDistanceRatio;

	// A mapping of vertex positions to their color in the existing static mesh
	TMap<FVector, FColor>		ExistingVertexColorData;
};

void DecomposeUCXMesh( const TArray<FVector>& CollisionVertices, const TArray<INT>& CollisionFaceIdx, FKAggregateGeom& AggGeom )
{
	// We keep no ref to this Model, so it will be GC'd at some point after the import.
	UModel* TempModel = new UModel(NULL,1);

	FASEConnectivityBuilder ConnectivityBuilder;

	// Send triangles to connectivity builder
	for(INT x = 0;x < CollisionFaceIdx.Num();x += 3)
	{
		const FVector &VertexA = CollisionVertices( CollisionFaceIdx(x + 2) );
		const FVector &VertexB = CollisionVertices( CollisionFaceIdx(x + 1) );
		const FVector &VertexC = CollisionVertices( CollisionFaceIdx(x + 0) );
		ConnectivityBuilder.AddTriangle( VertexA, VertexB, VertexC );
	}

	ConnectivityBuilder.CreateConnectivityGroups();

	// For each valid group build BSP and extract convex hulls
	for ( INT i=0; i<ConnectivityBuilder.Groups.Num(); i++ )
	{
		const FASEConnectivityGroup &Group = ConnectivityBuilder.Groups( i );

		// TODO: add some BSP friendly checks here
		// e.g. if group triangles form a closed mesh

		// Generate polygons from group triangles
		TempModel->Polys->Element.Empty();

		for ( INT j=0; j<Group.Triangles.Num(); j++ )
		{
			const FASEConnectivityTriangle &Triangle = ConnectivityBuilder.Triangles( Group.Triangles(j) );

			FPoly*	Poly = new( TempModel->Polys->Element ) FPoly();
			Poly->Init();
			Poly->iLink = j / 3;

			// Add vertices
			new( Poly->Vertices ) FVector( ConnectivityBuilder.Vertices( Triangle.Vertices[0] ).Position );
			new( Poly->Vertices ) FVector( ConnectivityBuilder.Vertices( Triangle.Vertices[1] ).Position );
			new( Poly->Vertices ) FVector( ConnectivityBuilder.Vertices( Triangle.Vertices[2] ).Position );

			// Update polygon normal
			Poly->CalcNormal(1);
		}

		// Build bounding box.
		TempModel->BuildBound();

		// Build BSP for the brush.
		FBSPOps::bspBuild( TempModel,FBSPOps::BSP_Good,15,70,1,0 );
		FBSPOps::bspRefresh( TempModel, 1 );
		FBSPOps::bspBuildBounds( TempModel );

	
		// Convert collision model into a collection of convex hulls.
		// Generated convex hulls will be added to existing ones
		const UBOOL bSuccess = KModelToHulls( &AggGeom, TempModel, FALSE );
		if ( !bSuccess )
		{
			warnf( NAME_Error, *LocalizeUnrealEd("Error_CollisionModelImportFailed") );
		}
	}
}

	

ExistingStaticMeshData* SaveExistingStaticMeshData(UStaticMesh* ExistingMesh)
{
	struct ExistingStaticMeshData* ExistingMeshDataPtr = NULL;

	if (ExistingMesh)
	{
		ExistingMeshDataPtr = new ExistingStaticMeshData();

		ExistingMeshDataPtr->ExistingElementMaterial.Add(ExistingMesh->LODModels(0).Elements.Num());
		ExistingMeshDataPtr->ExistingElementCollision.Add(ExistingMesh->LODModels(0).Elements.Num());
		ExistingMeshDataPtr->ExistingElementShadowCasting.Add(ExistingMesh->LODModels(0).Elements.Num());

		for(INT i=0; i<ExistingMesh->LODModels(0).Elements.Num(); i++)
		{
			ExistingMeshDataPtr->ExistingElementMaterial(i) = ExistingMesh->LODModels(0).Elements(i).Material;
			ExistingMeshDataPtr->ExistingElementCollision(i) = ExistingMesh->LODModels(0).Elements(i).EnableCollision;
			ExistingMeshDataPtr->ExistingElementShadowCasting(i) = ExistingMesh->LODModels(0).Elements(i).bEnableShadowCasting;
		}

		ExistingMeshDataPtr->ExistingUseSimpleLineCollision = ExistingMesh->UseSimpleLineCollision;
		ExistingMeshDataPtr->ExistingUseSimpleBoxCollision = ExistingMesh->UseSimpleBoxCollision;
		ExistingMeshDataPtr->ExistingUseSimpleRigidBodyCollision = ExistingMesh->UseSimpleRigidBodyCollision;
		ExistingMeshDataPtr->ExistingUseFullPrecisionUVs = ExistingMesh->UseFullPrecisionUVs;
		ExistingMeshDataPtr->ExistingUsedForInstancing = ExistingMesh->bUsedForInstancing;
		ExistingMeshDataPtr->ExistingUseMaximumStreamingTexelRatio = ExistingMesh->bUseMaximumStreamingTexelRatio;
		ExistingMeshDataPtr->ExistingPartitionForEdgeGeometry = ExistingMesh->bPartitionForEdgeGeometry;
		ExistingMeshDataPtr->ExistingCanBecomeDynamic = ExistingMesh->bCanBecomeDynamic;
		ExistingMeshDataPtr->ExistingStripkDOPForConsole = ExistingMesh->bStripkDOPForConsole;

		ExistingMeshDataPtr->ExistingLightMapResolution = ExistingMesh->LightMapResolution;
		ExistingMeshDataPtr->ExistingLightMapCoordinateIndex = ExistingMesh->LightMapCoordinateIndex;
		ExistingMeshDataPtr->ExistingVertexPositionVersionNumber = ExistingMesh->VertexPositionVersionNumber;

		ExistingMeshDataPtr->ExistingThumbnailAngle = ExistingMesh->ThumbnailAngle;
		ExistingMeshDataPtr->ExistingThumbnailDistance = ExistingMesh->ThumbnailDistance;

		ExistingMeshDataPtr->ExistingBodySetup = ExistingMesh->BodySetup;

		ExistingMeshDataPtr->ExistingStreamingDistanceMultiplier = ExistingMesh->StreamingDistanceMultiplier;
		ExistingMeshDataPtr->ExistingLODMaxRange = ExistingMesh->LODMaxRange;
		ExistingMeshDataPtr->ExistingLODDistanceRatio = ExistingMesh->LODDistanceRatio;

	}

	return ExistingMeshDataPtr;
}

void RestoreExistingMeshData(struct ExistingStaticMeshData* ExistingMeshDataPtr, UStaticMesh* NewMesh)
{
	if ( ExistingMeshDataPtr && NewMesh )
	{

		// Restore Section settings (material and collision) for overlap between old and new Sections array.
		NewMesh->LODInfo(0).Elements.Empty();
		NewMesh->LODInfo(0).Elements.AddZeroed( NewMesh->LODModels(0).Elements.Num() );

		INT NumElements = ::Min( ExistingMeshDataPtr->ExistingElementMaterial.Num(), NewMesh->LODModels(0).Elements.Num() );
		for(INT i=0; i<NumElements; i++)
		{
			NewMesh->LODModels(0).Elements(i).Material = ExistingMeshDataPtr->ExistingElementMaterial(i);
			NewMesh->LODModels(0).Elements(i).EnableCollision = ExistingMeshDataPtr->ExistingElementCollision(i);
			NewMesh->LODModels(0).Elements(i).bEnableShadowCasting = ExistingMeshDataPtr->ExistingElementShadowCasting(i);

			// Assign values for editor
			NewMesh->LODInfo(0).Elements(i).Material = ExistingMeshDataPtr->ExistingElementMaterial(i);
			NewMesh->LODInfo(0).Elements(i).bEnableCollision = ExistingMeshDataPtr->ExistingElementCollision(i);
			NewMesh->LODInfo(0).Elements(i).bEnableShadowCasting = ExistingMeshDataPtr->ExistingElementShadowCasting(i);
		}

		NewMesh->UseSimpleLineCollision = ExistingMeshDataPtr->ExistingUseSimpleLineCollision;
		NewMesh->UseSimpleBoxCollision = ExistingMeshDataPtr->ExistingUseSimpleBoxCollision;
		NewMesh->UseSimpleRigidBodyCollision = ExistingMeshDataPtr->ExistingUseSimpleRigidBodyCollision;
		NewMesh->UseFullPrecisionUVs = ExistingMeshDataPtr->ExistingUseFullPrecisionUVs;
		NewMesh->bUsedForInstancing = ExistingMeshDataPtr->ExistingUsedForInstancing;
		NewMesh->bUseMaximumStreamingTexelRatio = ExistingMeshDataPtr->ExistingUseMaximumStreamingTexelRatio;
		NewMesh->bPartitionForEdgeGeometry = ExistingMeshDataPtr->ExistingPartitionForEdgeGeometry;
		NewMesh->bCanBecomeDynamic = ExistingMeshDataPtr->ExistingCanBecomeDynamic;
		NewMesh->bStripkDOPForConsole = ExistingMeshDataPtr->ExistingStripkDOPForConsole;

		NewMesh->LightMapResolution = ExistingMeshDataPtr->ExistingLightMapResolution;
		NewMesh->LightMapCoordinateIndex = ExistingMeshDataPtr->ExistingLightMapCoordinateIndex;
		NewMesh->VertexPositionVersionNumber = ExistingMeshDataPtr->ExistingVertexPositionVersionNumber;

		NewMesh->ThumbnailAngle = ExistingMeshDataPtr->ExistingThumbnailAngle;
		NewMesh->ThumbnailDistance = ExistingMeshDataPtr->ExistingThumbnailDistance;

		NewMesh->StreamingDistanceMultiplier = ExistingMeshDataPtr->ExistingStreamingDistanceMultiplier;

		NewMesh->LODMaxRange = ExistingMeshDataPtr->ExistingLODMaxRange;
		NewMesh->LODDistanceRatio = ExistingMeshDataPtr->ExistingLODDistanceRatio;

		// If we already had some collision info...
		if(ExistingMeshDataPtr->ExistingBodySetup)
		{
			// If we didn't import anything, always keep collision.
			UBOOL bKeepCollision;
			if(!NewMesh->BodySetup)
			{
				bKeepCollision = TRUE;
			}
			// If we imported something- ask if we want to keep the current stuff.
			else
			{
				const UBOOL bOverwriteData = appMsgf( AMT_YesNo, *LocalizeUnrealEd("OverwriteCollisionData") );

				if( bOverwriteData )
				{
					bKeepCollision = FALSE;
				}
				else
				{
					bKeepCollision = TRUE;
				}
			}

			if(bKeepCollision)
			{
				NewMesh->BodySetup = ExistingMeshDataPtr->ExistingBodySetup;
			}
			else
			{
				// New collision geometry, but we still want the original settings
				NewMesh->BodySetup->bAlwaysFullAnimWeight = ExistingMeshDataPtr->ExistingBodySetup->bAlwaysFullAnimWeight;
				NewMesh->BodySetup->bBlockNonZeroExtent = ExistingMeshDataPtr->ExistingBodySetup->bBlockNonZeroExtent;
				NewMesh->BodySetup->bEnableContinuousCollisionDetection = ExistingMeshDataPtr->ExistingBodySetup->bEnableContinuousCollisionDetection;
				NewMesh->BodySetup->bFixed = ExistingMeshDataPtr->ExistingBodySetup->bFixed;
				NewMesh->BodySetup->bNoCollision = ExistingMeshDataPtr->ExistingBodySetup->bNoCollision;
				NewMesh->BodySetup->BoneName = ExistingMeshDataPtr->ExistingBodySetup->BoneName;
				NewMesh->BodySetup->COMNudge = ExistingMeshDataPtr->ExistingBodySetup->COMNudge;
				NewMesh->BodySetup->MassScale = ExistingMeshDataPtr->ExistingBodySetup->MassScale;
				NewMesh->BodySetup->PhysMaterial = ExistingMeshDataPtr->ExistingBodySetup->PhysMaterial;

				NewMesh->BodySetup->PreCachedPhysScale.Empty( ExistingMeshDataPtr->ExistingBodySetup->PreCachedPhysScale.Num() );
				for( INT i = 0; i < ExistingMeshDataPtr->ExistingBodySetup->PreCachedPhysScale.Num(); i++ )
				{
					NewMesh->BodySetup->PreCachedPhysScale( i ) = ExistingMeshDataPtr->ExistingBodySetup->PreCachedPhysScale( i );
				}
			}
		}
	}
}

UObject* UStaticMeshFactory::FactoryCreateText
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	const TCHAR*	BufferTemp	= Buffer;
	FMatrix			Transform	= FRotationMatrix( FRotator(Pitch,Yaw,Roll) );

	FStaticMeshComponentReattachContext	ComponentReattachContext(FindObject<UStaticMesh>(InParent,*Name.ToString()));

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	if(appStricmp(Type,TEXT("ASE")) == 0 )
	{
#if WITH_ACTORX
		FLOAT ImportVertexCountWarningPercentage = 25.0f;
		GConfig->GetFloat(TEXT("ASEImporter"), TEXT("ImportVertexCountWarningPercentage"), ImportVertexCountWarningPercentage, GEditorIni);
		ImportVertexCountWarningPercentage = ImportVertexCountWarningPercentage / 100.0f;

		INT ImportedVertexCount = 0;
		UBOOL bSingleSmoothingGroup = FALSE;

		// Before importing the new static mesh - see if it already exists, and if so copy any data out we don't want to lose.
		UStaticMesh* ExistingMesh = FindObject<UStaticMesh>( InParent, *Name.ToString() );

		UBOOL	bRestoreExistingData = FALSE;
		struct ExistingStaticMeshData* ExistingMeshDataPtr = NULL;

		// A mapping of vertex positions to their color in the existing static mesh
		TMap<FVector, FColor>		ExistingVertexColorData;

		// If we should get vertex colors. Defaults to the checkbox value available to the user in the import property window
		UBOOL bGetVertexColors = bReplaceVertexColors;

		//@todo: Figure out why ASE never exports explicit normals
		UBOOL bExplicitNormals = FALSE;

		if(ExistingMesh)
		{
			ExistingMesh->GetVertexColorData(ExistingVertexColorData);

			if (0 == ExistingVertexColorData.Num())
			{
				// If there were no vertex colors, automatically take vertex colors from the file.
				bGetVertexColors = TRUE;
			}

			// Free any RHI resources for existing mesh before we re-create in place.
			ExistingMesh->PreEditChange(NULL);
			
			ExistingMeshDataPtr = SaveExistingStaticMeshData(ExistingMesh);
			bRestoreExistingData = TRUE;
		}
		else
		{
			// Vertex colors should be copied always if there is no existing static mesh.
			bGetVertexColors = TRUE;
		}

		UStaticMesh*	StaticMesh = new(InParent,Name,Flags|RF_Public) UStaticMesh;
		StaticMesh->SourceFilePath = GFileManager->ConvertToRelativePath(*CurrentFilename);

		FFileManager::FTimeStamp Timestamp;
		if (GFileManager->GetTimestamp( *CurrentFilename, Timestamp ))
		{
			FFileManager::FTimeStamp::TimestampToFString(Timestamp, /*out*/ StaticMesh->SourceFileTimestamp);
		}

		if(!StaticMesh->LODModels.Num())
		{
			// Add one LOD for the base mesh
			new(StaticMesh->LODModels) FStaticMeshRenderData();
			StaticMesh->LODInfo.AddItem(FStaticMeshLODInfo());
		}

		// make sure it has a new lighting guid
		StaticMesh->LightingGuid = appCreateGuid();

		// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoordindex exists for all LODs, etc).
		StaticMesh->LightMapResolution = 32;
		StaticMesh->LightMapCoordinateIndex = 1;

		FString StrLine, ExtraLine;
		while( ParseLine( &Buffer, StrLine ) )
		{
			const TCHAR* Str = *StrLine;

			if( appStrstr(Str,TEXT("*3DSMAX_ASCIIEXPORT")) )
			{
				debugf( NAME_Log, TEXT("Reading 3D Studio ASE file") );

				TArray<FVector> Vertex;							// 1 FVector per entry
				TArray<FColor>	Colors;							// 1 FColor per entry
				TArray<FVector> Normals;						// 1 Explicit normal per entry
				TArray<INT> FaceIdx;							// 3 INT's for vertex indices per entry
				TArray<INT>	FaceColorIdx;						// 3 INT's for color indices per entry
				TArray<INT> FaceMaterialsIdx;					// 1 INT for material ID per face
				TArray<FASEMappingChannel> MappingChannels;		// 1 per mapping channel
				TArray<FASEMaterialHeaderInfo> ASEMaterialHeaders;	// 1 per material (multiple sub-materials inside each one)
				TArray<DWORD>	SmoothingGroups;				// 1 DWORD per face.
				TArray<FVector>	CollisionVertices;
				TArray<INT>		CollisionFaceIdx;
				FString			NodeName;						// Store current object name in case we want to print its name for debugging
				
				INT		ASEMaterialRef = -1,
						CurrentMappingChannel = MappingChannels.AddZeroed();

				EGeomObjType GeomObjType = GOT_Graphics;

				UMaterialInterface* CurrentMaterial = NULL;
				FString CurrentMaterialName, wk;
				INT UTiling = 1, VTiling = 1;
				TArray<FString> Split;

				while( ParseLine( &Buffer, StrLine ) )
				{
					StrLine.Trim();
					Str = *StrLine;

					// ==
					// Every time we see a MATERIAL_NAME line, set that material as the current one.

					if( StrLine.StartsWith( TEXT("*MATERIAL_NAME") ) )
					{
						if( StrLine.ParseIntoArrayWS( &Split ) == 2 )
						{
							CurrentMaterialName = Split(1).TrimQuotes();

							// Find the material

							CurrentMaterial = NULL;
							for( TObjectIterator<UMaterialInterface> It ; It ; ++It )
							{
								if( It->GetName() == CurrentMaterialName )
								{
									CurrentMaterial = *It;
									break;
								}
							}
						}
						else
						{
							// The material name was formatted wrong and couldn't be parsed (i.e. it had a space in it)

							CurrentMaterial = NULL;
						}
					}

					// ==
					// Indicates the start of a material definition.  Create a header entry to hold them.

					if( StrLine.StartsWith( TEXT("*MATERIAL ") ) )
					{
						new( ASEMaterialHeaders )FASEMaterialHeaderInfo();
					}

					// ==
					// Parse the contents of the MAP_DIFFUSE block in the current material

					if( StrLine.StartsWith( TEXT("*MAP_DIFFUSE") ) )
					{
						while( ParseLine( &Buffer, StrLine ) )
						{
							StrLine.Trim();
							Str = *StrLine;

							if( StrLine == TEXT("}") )
							{
								// Add the material to the material list before leaving

								check(ASEMaterialHeaders.Num());
								new( ASEMaterialHeaders(ASEMaterialHeaders.Num()-1).Materials )FASEMaterialInfo( (TCHAR*)*CurrentMaterialName, CurrentMaterial->GetWidth(), CurrentMaterial->GetHeight(), UTiling, VTiling, CurrentMaterial );

								break;
							}
							
							if( StrLine.StartsWith( TEXT("*UVW_U_TILING") ) )
							{
								if( StrLine.ParseIntoArray( &Split, TEXT(" "), 1 ) == 2)
								{
									UTiling = appAtoi( *Split(1) );
								}
							}
							else if( StrLine.StartsWith( TEXT("*UVW_V_TILING") ) )
							{
								if( StrLine.ParseIntoArray( &Split, TEXT(" "), 1 ) == 2)
								{
									VTiling = appAtoi( *Split(1) );
								}
							}
							else if( StrLine.StartsWith( TEXT("*BITMAP ") ) )
							{
								// If no material could be established earlier, try the bitmap name as a last resort.

								if( CurrentMaterial == NULL )
								{
									FString Name = StrLine;
									Name = Name.Right( Name.Len() - Name.InStr(TEXT("\\"), -1 ) - 1 );	// Strip off path info
									Name = Name.Left( Name.Len() - 5 );									// Strip off the extension

									// Find the material

									for( TObjectIterator<UMaterialInterface> It; It ; ++It )
									{
										if( It->GetName() == Name )
										{
											CurrentMaterial = *It;
											break;
										}
									}
								}
							}
						}
					}

					// ==
					// Each GEOMOBJECT block contains the complete definition for a piece of geometry.

					if( StrLine.StartsWith( TEXT("*GEOMOBJECT") ) )
					{
						Vertex.Empty();
						Colors.Empty();
						Normals.Empty();
						FaceIdx.Empty();
						FaceColorIdx.Empty();
						FaceMaterialsIdx.Empty();
						MappingChannels.Empty();
						SmoothingGroups.Empty();
						CollisionVertices.Empty();
						CollisionFaceIdx.Empty();

						GeomObjType = GOT_Graphics;
						CurrentMappingChannel = MappingChannels.AddZeroed();
						NodeName = FString(TEXT(""));

						int SectionRefCount = 1;

						while( ParseLine( &Buffer, StrLine ) )
						{
							StrLine.Trim();
							Str = *StrLine;

							if( StrLine == TEXT("}") )
							{
								SectionRefCount--;
								if( !SectionRefCount )
								{
									// The GEOM section is over.  Continue reading the rest of the GEOMOBJECT section to get the MATERIAL_REF.

									while( ParseLine( &Buffer, StrLine ) )
									{
										StrLine.Trim();
										Str = *StrLine;

										if( StrLine == TEXT("}") )
										{
											break;
										}

										if( StrLine.StartsWith(TEXT("*MATERIAL_REF ") ) )
										{
											if( StrLine.ParseIntoArrayWS( &Split ) == 2 )
											{
												ASEMaterialRef = appAtoi( *Split(1) );
											}
										}
									}

									break;
								}
							}

							if( StrLine.StartsWith( TEXT("*NODE_TM") ) )
							{
								// Don't care about these sections, so just chew them up.

								while( ParseLine( &Buffer, StrLine ) )
								{
									StrLine.Trim();
									Str = *StrLine;

									if( StrLine == TEXT("}") )
									{
										break;
									}
								}
							}
							else if( StrLine.StartsWith( TEXT("*MESH_CVERTLIST") ) )
							{
								FVector color;

								while( ParseLine( &Buffer, StrLine ) )
								{
									StrLine.Trim();
									Str = *StrLine;

									if( StrLine == TEXT("}") )
									{
										break;
									}

									if( StrLine.ParseIntoArrayWS( &Split ) == 5 )
									{
										color.X = appAtof( *Split(2) );
										color.Y = appAtof( *Split(3) );
										color.Z = appAtof( *Split(4) );

										if( GeomObjType == GOT_Graphics )
										{
											new(Colors)FColor(
												Clamp(appRound(color.X*255),0,255),
												Clamp(appRound(color.Y*255),0,255),
												Clamp(appRound(color.Z*255),0,255),
												Clamp(appRound(color.X*255),0,255) );
										}
									}
								}
							}
							else if( StrLine.StartsWith( TEXT("*MESH_CFACELIST") ) )
							{
								int	idx1, idx2, idx3;

								while( ParseLine( &Buffer, StrLine ) )
								{
									StrLine.Trim();
									Str = *StrLine;

									if( StrLine == TEXT("}") )
									{
										break;
									}

									if( StrLine.ParseIntoArrayWS( &Split ) == 5 )
									{
										idx1 = appAtoi( *Split(2) );
										idx2 = appAtoi( *Split(3) );
										idx3 = appAtoi( *Split(4) );

										if( GeomObjType == GOT_Graphics )
										{
											new(FaceColorIdx) INT(idx1);
											new(FaceColorIdx) INT(idx2);
											new(FaceColorIdx) INT(idx3);
										}
									}
								}
							}
							else if ( StrLine.StartsWith( TEXT("*MESH_NORMALS") ) )
							{	
								FVector Normal;
								while( ParseLine( &Buffer, StrLine ) )
								{
									StrLine.Trim();
									if( StrLine.StartsWith( TEXT("*MESH_VERTEXNORMAL") ) )
									{
										if( StrLine.ParseIntoArrayWS( &Split ) == 5 )
										{
											if( GeomObjType == GOT_Graphics )
											{
												Normal.X = appAtof( *Split(2) );
												Normal.Y = -appAtof( *Split(3) );
												Normal.Z = appAtof( *Split(4) );
												new(Normals) FVector(Normal);
											}
										}
									}
									else if( StrLine == TEXT("}") )
									{
										break;
									}
								}
							}
							else if( StrLine.StartsWith( TEXT("*MESH_MAPPINGCHANNEL") ) )
							{
								INT	MappingChannel = -1;

								if( StrLine.ParseIntoArrayWS( &Split ) == 3 )
								{
									MappingChannel = appAtoi( *Split(1) );

									if( GeomObjType == GOT_Graphics )
									{
										CurrentMappingChannel = MappingChannels.AddZeroed();
									}

									SectionRefCount++;
								}

							}
							else if( StrLine.InStr(TEXT("*NODE_NAME")) != -1 )
							{
								if( StrLine.ParseIntoArrayWS( &Split ) == 2 )
								{
									NodeName = Split(1).TrimQuotes();

									// Check MCDCX for backwards compatibility..
									if( NodeName.InStr( TEXT("UCX") ) != INDEX_NONE || NodeName.InStr( TEXT("MCDCX") ) != INDEX_NONE )
									{	
										GeomObjType = GOT_Convex;
									}
									else if( NodeName.InStr( TEXT("UBX") ) != INDEX_NONE )
									{
										GeomObjType = GOT_Box;
									}
									else if( NodeName.InStr( TEXT("USP") ) != INDEX_NONE )
									{
										GeomObjType = GOT_Sphere;
									}
									// Anything other than those prefixes and we treat it as a graphics mesh.
									else
									{
										GeomObjType = GOT_Graphics;
									}
								}
							}
							else if( StrLine.StartsWith( TEXT("*MESH_VERTEX_LIST") ) )
							{
								while( ParseLine( &Buffer, StrLine ) )
								{
									StrLine.Trim();
									Str = *StrLine;

									if( StrLine == TEXT("}") )
									{
										break;
									}
									
									if( StrLine.ParseIntoArrayWS( &Split ) == 5 )
									{
										FVector vtx;

										// Invert vertex Y coordinate on import
										vtx.X = appAtof( *Split(2) );
										vtx.Y = -appAtof( *Split(3) );
										vtx.Z = appAtof( *Split(4) );

										if( GeomObjType == GOT_Graphics )
										{
											new(Vertex)FVector(vtx);
											// Collision vertices should not count towards the imported vertex count...
											ImportedVertexCount++;
										}
										else
										{
											new(CollisionVertices)FVector(vtx);
										}
									}
								}
							}
							else if( StrLine.StartsWith( TEXT("*MESH_FACE_LIST") ) )
							{
								while( ParseLine( &Buffer, StrLine ) )
								{
									StrLine.Trim();
									Str = *StrLine;

									if( StrLine == TEXT("}") )
									{
										break;
									}
									
									INT idx1, idx2, idx3;
									if( StrLine.ParseIntoArrayWS( &Split ) > 7 )
									{
										idx1 = appAtoi( *Split(3) );
										idx2 = appAtoi( *Split(5) );
										idx3 = appAtoi( *Split(7) );

										if( GeomObjType != GOT_Graphics )
										{
											new(CollisionFaceIdx)INT(idx1);
											new(CollisionFaceIdx)INT(idx2);
											new(CollisionFaceIdx)INT(idx3);
										}
										else
										{
											new(FaceIdx)INT(idx1);
											new(FaceIdx)INT(idx2);
											new(FaceIdx)INT(idx3);

											// SmoothING group

											if( StrLine.InStr( TEXT("*MESH_SMOOTHING") ) != INDEX_NONE )
											{
												// Find the location of the smooth info within the line (it moves depending on the exporting app)

												INT idx = INDEX_NONE;
												for( INT x = 0 ; x < Split.Num()-1 ; ++x )
												{
													if( Split(x) == TEXT("*MESH_SMOOTHING") )
													{
														idx = x + 1;
														break;
													}
												}

												if( idx > INDEX_NONE )
												{
													FString	SmoothingString = Split(idx);
													DWORD	SmoothingMask = 0;

													while(SmoothingString.Len())
													{
														INT	Length = SmoothingString.InStr(TEXT(",")), SmoothingGroup = (Length != -1) ? appAtoi(*SmoothingString.Left(Length)) : appAtoi(*SmoothingString);

														if(SmoothingGroup <= 32)
														{
															SmoothingMask |= (1 << (SmoothingGroup - 1));
														}

														SmoothingString = (Length != -1) ? SmoothingString.Right(SmoothingString.Len() - Length - 1) : TEXT("");
													}

												
													SmoothingGroups.AddItem(SmoothingMask);
												}
											}
											else
											{
												SmoothingGroups.AddItem(0);
											}

											// Material ID (always the last item in the line )

											INT MaterialID = appAtoi( *Split(Split.Num()-1) );

											new(FaceMaterialsIdx)INT(MaterialID);
										}
									}
								}
							}
							else if( StrLine.StartsWith( TEXT("*MESH_TVERTLIST") ) )
							{
								while( ParseLine( &Buffer, StrLine ) )
								{
									StrLine.Trim();
									Str = *StrLine;

									if( StrLine == TEXT("}") )
									{
										break;
									}
									
									if( StrLine.ParseIntoArrayWS( &Split ) == 5 )
									{
										FVector2D vtx;

										vtx.X = appAtof( *Split(2) );
										vtx.Y = appAtof( *Split(3) );

										if( GeomObjType == GOT_Graphics )
										{
											new( MappingChannels(CurrentMappingChannel).TexCoord )FVector2D(vtx);
										}
									}
								}
							}
							else if( StrLine.StartsWith( TEXT("*MESH_TFACELIST") ) )
							{
								while( ParseLine( &Buffer, StrLine ) )
								{
									StrLine.Trim();
									Str = *StrLine;

									if( StrLine == TEXT("}") )
									{
										break;
									}
									
									if( StrLine.ParseIntoArrayWS( &Split ) == 5 )
									{
										int idx1, idx2, idx3;

										idx1 = appAtoi( *Split(2) );
										idx2 = appAtoi( *Split(3) );
										idx3 = appAtoi( *Split(4) );

										if( GeomObjType == GOT_Graphics )
										{
											new( MappingChannels(CurrentMappingChannel).FaceTexCoordIdx )INT(idx1);
											new( MappingChannels(CurrentMappingChannel).FaceTexCoordIdx )INT(idx2);
											new( MappingChannels(CurrentMappingChannel).FaceTexCoordIdx )INT(idx3);
										}
									}
								}
							}
						}

						// Create the polys from the gathered info.

						for( INT ChannelIndex = 0 ; ChannelIndex < MappingChannels.Num() ; ChannelIndex++ )
						{
							if( FaceIdx.Num() != MappingChannels(ChannelIndex).FaceTexCoordIdx.Num() )
							{
								appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_ASEImporterError"),
									FaceIdx.Num(), ChannelIndex, MappingChannels(ChannelIndex).FaceTexCoordIdx.Num()));
								continue;
							}
						}

						// Materials

						if( ASEMaterialRef == -1 )
						{
							// No material.  Not a big deal really as this is a common case.  Log it anyway.
							warnf( *LocalizeUnrealEd("Error_ObjectHasNoMaterialRef") );

							const INT NewMaterialIndex = StaticMesh->LODModels(0).Elements.Num();
							new(StaticMesh->LODModels(0).Elements)FStaticMeshElement(NULL,NewMaterialIndex);
						}
						else
						{
							for( INT MaterialIndex = 0 ; MaterialIndex < ASEMaterialHeaders(ASEMaterialRef).Materials.Num() ; MaterialIndex++ )
							{
								const INT NewMaterialIndex = StaticMesh->LODModels(0).Elements.Num();
								FStaticMeshElement* Element = new(StaticMesh->LODModels(0).Elements)FStaticMeshElement(ASEMaterialHeaders(ASEMaterialRef).Materials(MaterialIndex).Material,NewMaterialIndex);
								Element->Name = ASEMaterialHeaders(ASEMaterialRef).Materials(MaterialIndex).Name;
							}
						}

						if( !GBuildStaticMeshCollision )
						{
							for( INT MaterialIndex = 0 ; MaterialIndex < StaticMesh->LODModels(0).Elements.Num() ; MaterialIndex++ )
							{
								StaticMesh->LODModels(0).Elements(MaterialIndex).EnableCollision = 0;
							}
						}

						if(!StaticMesh->LODModels(0).Elements.Num())
						{
							const INT NewMaterialIndex = StaticMesh->LODModels(0).Elements.Num();
							new(StaticMesh->LODModels(0).Elements) FStaticMeshElement(NULL,NewMaterialIndex);
						}

						// Create a BSP tree from the imported collision geometry.

						if( CollisionFaceIdx.Num() )
						{
							// If we dont already have physics props, construct them here.
							if( !StaticMesh->BodySetup )
							{
								StaticMesh->BodySetup = ConstructObject<URB_BodySetup>(URB_BodySetup::StaticClass(), StaticMesh);
							}

							if ( GeomObjType == GOT_Convex && !bOneConvexPerUCXObject )
							{
								DecomposeUCXMesh( CollisionVertices, CollisionFaceIdx, StaticMesh->BodySetup->AggGeom );
							}
							else
							{
								TArray<FPoly> CollisionTriangles;

								// Make triangles
								for(INT x = 0;x < CollisionFaceIdx.Num();x += 3)
								{
									FPoly*	Poly = new( CollisionTriangles ) FPoly();

									Poly->Init();

									new(Poly->Vertices) FVector( CollisionVertices(CollisionFaceIdx(x + 2)) );
									new(Poly->Vertices) FVector( CollisionVertices(CollisionFaceIdx(x + 1)) );
									new(Poly->Vertices) FVector( CollisionVertices(CollisionFaceIdx(x + 0)) );
									Poly->iLink = x / 3;

									Poly->CalcNormal(1);
								}

								// Construct geometry object
								if( GeomObjType == GOT_Convex )
								{
									AddConvexGeomFromVertices( CollisionVertices, &StaticMesh->BodySetup->AggGeom, *NodeName );
								}
								else if ( GeomObjType == GOT_Box )
								{
									AddBoxGeomFromTris( CollisionTriangles, &StaticMesh->BodySetup->AggGeom, *NodeName );
								}
								else if ( GeomObjType == GOT_Sphere )
								{
									AddSphereGeomFromVerts( CollisionVertices, &StaticMesh->BodySetup->AggGeom, *NodeName );
								}
							}

							// Clear any cached rigid-body collision shapes for this body setup.
							StaticMesh->BodySetup->ClearShapeCache();
						}

						// Faces

						FASEMaterialInfo	ASEMaterial;

						INT ExistingTris = StaticMesh->LODModels(0).RawTriangles.GetElementCount();
						StaticMesh->LODModels(0).RawTriangles.Lock(LOCK_READ_WRITE);
						FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) StaticMesh->LODModels(0).RawTriangles.Realloc( FaceIdx.Num()/3 + ExistingTris );
						RawTriangleData += ExistingTris;
						
						for( int x = 0 ; x < FaceIdx.Num() ; x += 3 )
						{
							FStaticMeshTriangle*	Triangle = (RawTriangleData++);
							
							Triangle->Vertices[0] = Vertex( FaceIdx(x) );
							Triangle->Vertices[1] = Vertex( FaceIdx(x+1) );
							Triangle->Vertices[2] = Vertex( FaceIdx(x+2) );

							check(SmoothingGroups.IsValidIndex(x / 3));
							Triangle->SmoothingMask = SmoothingGroups(x / 3);

							if(ASEMaterialRef != INDEX_NONE && FaceMaterialsIdx(x / 3) < ASEMaterialHeaders(ASEMaterialRef).Materials.Num())
							{
								Triangle->MaterialIndex = FaceMaterialsIdx(x / 3);
							}
							else
							{
								Triangle->MaterialIndex = 0;
							}

							// @todo: Support fragments
							Triangle->FragmentIndex = 0;

							Triangle->bOverrideTangentBasis = FALSE;
							Triangle->bExplicitNormals = FALSE;
							for( INT NormalIndex = 0; NormalIndex < 3; ++NormalIndex )
							{
								Triangle->TangentX[ NormalIndex ] = FVector( 0.0f, 0.0f, 0.0f );
								Triangle->TangentY[ NormalIndex ] = FVector( 0.0f, 0.0f, 0.0f );
								Triangle->TangentZ[ NormalIndex ] = bExplicitNormals ? Normals( x+NormalIndex ) : FVector( 0.0f, 0.0f, 0.0f );
							}

							if( ASEMaterialRef != -1 )
							{
								if( ASEMaterialHeaders(ASEMaterialRef).Materials.Num() )
								{
									if( ASEMaterialHeaders(ASEMaterialRef).Materials.Num() == 1 )
									{
										ASEMaterial = ASEMaterialHeaders(ASEMaterialRef).Materials(0);
									}
									else
									{
										// Sometimes invalid material references appear in the ASE file.  We can't do anything about
										// it, so when that happens just use the first material.

										if( FaceMaterialsIdx(x/3) >= ASEMaterialHeaders(ASEMaterialRef).Materials.Num() )
										{
											ASEMaterial = ASEMaterialHeaders(ASEMaterialRef).Materials(0);
										}
										else
										{
											ASEMaterial = ASEMaterialHeaders(ASEMaterialRef).Materials( FaceMaterialsIdx(x/3) );
										}
									}
								}
							}


							for( INT ChannelIndex = 0 ; ChannelIndex < MappingChannels.Num() && ChannelIndex < 8 ; ChannelIndex++ )
							{
								if( MappingChannels(ChannelIndex).FaceTexCoordIdx.Num() == FaceIdx.Num() )
								{
									FVector2D	ST1 = MappingChannels(ChannelIndex).TexCoord( MappingChannels(ChannelIndex).FaceTexCoordIdx(x + 0) ),
												ST2 = MappingChannels(ChannelIndex).TexCoord( MappingChannels(ChannelIndex).FaceTexCoordIdx(x + 1) ),
												ST3 = MappingChannels(ChannelIndex).TexCoord( MappingChannels(ChannelIndex).FaceTexCoordIdx(x + 2) );

									Triangle->UVs[0][ChannelIndex].X = ST1.X * ASEMaterial.UTiling;
									Triangle->UVs[0][ChannelIndex].Y = (1.0f - ST1.Y) * ASEMaterial.VTiling;

									Triangle->UVs[1][ChannelIndex].X = ST2.X * ASEMaterial.UTiling;
									Triangle->UVs[1][ChannelIndex].Y = (1.0f - ST2.Y) * ASEMaterial.VTiling;

									Triangle->UVs[2][ChannelIndex].X = ST3.X * ASEMaterial.UTiling;
									Triangle->UVs[2][ChannelIndex].Y = (1.0f - ST3.Y) * ASEMaterial.VTiling;
								}
							}

							Triangle->NumUVs = Min(MappingChannels.Num(),8);

							if( FaceColorIdx.Num() == FaceIdx.Num() )
							{
								// Set up the triangles with the color data that was imported
								for( INT Index = 0; Index < 3; ++Index )
								{	
									if( bGetVertexColors )
									{
										// If we are replacing vertex colors, look up the correct color found during import
										Triangle->Colors[Index] = Colors(FaceColorIdx(x + Index));

									}
									else
									{
										// try to match this triangles current vertex with one that existed in the previous mesh.
										// This is a find in a tmap which uses a fast hash table lookup.
										FColor* PaintedColor = ExistingVertexColorData.Find( Triangle->Vertices[Index] );
										if( PaintedColor )
										{
											// A matching color for this vertex was found
											Triangle->Colors[Index] = *PaintedColor;
										}
										else
										{
											// A matching color for this vertex could not be found. Make this vertex white
											Triangle->Colors[Index] = FColor( 255,255,255,255 ); 
										}
									}
								}
							}
							else
							{
								// No color data was found, still try to match existing colors
								for( INT Index = 0; Index < 3; ++Index )
								{	
									// try to match this triangles current vertex with one that existed in the previous mesh.
									// This is a find in a tmap which uses a fast hash table lookup.
									FColor* PaintedColor = ExistingVertexColorData.Find( Triangle->Vertices[Index] );
									if( PaintedColor )
									{
										// A matching color for this vertex was found
										Triangle->Colors[Index] = *PaintedColor;
									}
									else
									{
										// A matching color for this vertex could not be found. Make this vertex white
										Triangle->Colors[Index] = FColor(255,255,255,255);
									}
								}
							}

						}
						StaticMesh->LODModels(0).RawTriangles.Unlock();
					}
				}

				bSingleSmoothingGroup = (MappingChannels.Num() == 1) ? TRUE : FALSE;

				// Compress the materials array by removing any duplicates.
				for(int k=0;k<StaticMesh->LODModels.Num();k++)
				{
					TArray<FStaticMeshElement> SaveElements = StaticMesh->LODModels(k).Elements;
					StaticMesh->LODModels(k).Elements.Empty();

					for( int x = 0 ; x < SaveElements.Num() ; ++x )
					{
						// See if this material is already in the list.  If so, loop through all the raw triangles
						// and change the material index to point to the one already in the list.

						INT newidx = INDEX_NONE;
						for( INT y = 0 ; y < StaticMesh->LODModels(k).Elements.Num() ; ++y )
						{
							if( StaticMesh->LODModels(k).Elements(y).Name == SaveElements(x).Name )
							{
								newidx = y;
								break;
							}
						}

						if( newidx == INDEX_NONE )
						{
							StaticMesh->LODModels(k).Elements.AddItem( SaveElements(x) );
							newidx = StaticMesh->LODModels(k).Elements.Num()-1;
						}

						FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) StaticMesh->LODModels(k).RawTriangles.Lock(LOCK_READ_WRITE);
						for( INT t = 0 ; t < StaticMesh->LODModels(k).RawTriangles.GetElementCount() ; ++t )
						{
							if( RawTriangleData[t].MaterialIndex == x )
							{
								RawTriangleData[t].MaterialIndex = newidx;
							}
						}
						StaticMesh->LODModels(k).RawTriangles.Unlock();
					}
				}
			}
		}

		// Restore existing information if desired.
		if(bRestoreExistingData)
		{
			// Restore Section settings (material and collision) for overlap between old and new Sections array.
			RestoreExistingMeshData(ExistingMeshDataPtr, StaticMesh);
		}

		UBOOL bSingleTangentSet = FALSE;
		if (bSingleSmoothingGroup && bSingleSmoothGroupSingleTangent)
		{
			debugf(TEXT("Single tangent set per vertex will be enforced."));
			bSingleTangentSet = TRUE;
		}
		// Now that we've loaded all of the data (and potentially merged over settings from the original
		// mesh), go ahead and rebuild everything
		StaticMesh->Build(bSingleTangentSet);

		INT GeneratedVertexCount = StaticMesh->LODModels(0).NumVertices;
		INT Diff = Clamp<INT>(GeneratedVertexCount - ImportedVertexCount, 0, GeneratedVertexCount);
		FLOAT PercentageIncrease = (FLOAT)Diff / (FLOAT)ImportedVertexCount;
		FString PercentageIncreaseStr = FString::Printf(TEXT("%4.2f %%"), PercentageIncrease * 100.0f);
		FString AllowedPercentageStr = FString::Printf(TEXT("%4.2f percent vs %4.2f percent"), PercentageIncrease * 100.0f, ImportVertexCountWarningPercentage * 100.0f);
		debugf(LocalizeSecure(LocalizeUnrealEd("Message_ASEImportVertexPercentageResults"), GeneratedVertexCount, ImportedVertexCount, *PercentageIncreaseStr));
		if (bSingleSmoothingGroup && (PercentageIncrease >= ImportVertexCountWarningPercentage))
		{
			appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_ASEImportVertexPercentageWarning"), GeneratedVertexCount, ImportedVertexCount, *AllowedPercentageStr));
		}


		// Warn about bad light map UVs if we have any
		{
			TArray< FString > MissingUVSets;
			TArray< FString > BadUVSets;
			TArray< FString > ValidUVSets;
			UStaticMesh::CheckLightMapUVs( StaticMesh, MissingUVSets, BadUVSets, ValidUVSets );

			// NOTE: We don't care about missing UV sets here, just bad ones!

// 			if( BadUVSets.Num() > 0 )
// 			{
// 				appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd( "Error_NewStaticMeshHasBadLightMapUVSet_F" ), *GetName() ) );
// 			}
		}

 		// Warn about any bad mesh elements
		if (UStaticMesh::RemoveZeroTriangleElements(StaticMesh, FALSE) == TRUE)
		{
			// Build it again...
			StaticMesh->Build(bSingleTangentSet, TRUE);
		}
		return StaticMesh;
#else
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_ActorXDeprecated") );
		return NULL;
#endif // WITH_ACTORX
	}
	else if(appStricmp(Type,TEXT("T3D")) == 0)
	{
		if(GetBEGIN(&BufferTemp,TEXT("PolyList")))
		{
			UModelFactory*	ModelFactory = new UModelFactory;
			UModel*			Model = (UModel*) ModelFactory->FactoryCreateText(UModel::StaticClass(),InParent,NAME_None,0,NULL,TEXT("T3D"),Buffer,BufferEnd,Warn);

			TransformPolys(Model->Polys,Transform);

			if(Model)
			{
				UStaticMesh *StaticMesh =  CreateStaticMeshFromBrush(InParent,Name,NULL,Model);

				if(StaticMesh)
				{
					StaticMesh->SourceFilePath = GFileManager->ConvertToRelativePath(*CurrentFilename);

					FFileManager::FTimeStamp Timestamp;
					if (GFileManager->GetTimestamp( *CurrentFilename, Timestamp ))
					{
						FFileManager::FTimeStamp::TimestampToFString(Timestamp, /*out*/ StaticMesh->SourceFileTimestamp);
					}
				}

				return StaticMesh;
			}
			else
			{
				return NULL;
			}
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		return NULL;
	}
}

/**
 *	Initializes the given staticmesh from the SMData text block supplied.
 *	The SMData text block is assumed to have been generated by the UStaticMeshExporterT3D.
 *
 */
UBOOL UStaticMeshFactory::InitializeFromT3DSMDataText(UStaticMesh* InStaticMesh, const FString& Text, FFeedbackContext* Warn )
{
	const TCHAR* Buffer = *Text;

	FString StrLine;
	while( ParseLine(&Buffer,StrLine) )
	{
		const TCHAR* Str = *StrLine;

		FString ParsedText;
		
		if (Parse(Str, TEXT("VERSION="), ParsedText))
		{
			///** Versioning system... */
			//Ar.Logf(TEXT("%sVersion=%d.%d") LINE_TERMINATOR, appSpc(TextIndent), VersionMax, VersionMin);
		}
		else
		if (Parse(Str, TEXT("LODDISTANCERATIO="), ParsedText))
		{
			///** LOD distance ratio for this mesh */
			//ExportFloatProperty(TEXT("LODDistanceRatio"), StaticMeshObj->LODDistanceRatio, Ar, PortFlags);
			InStaticMesh->LODDistanceRatio = appAtof(*ParsedText);
		}
		else
		if (Parse(Str, TEXT("LODMAXRANGE="), ParsedText))
		{
			///** Range at which only the lowest detail LOD can be displayed */
			//ExportFloatProperty(TEXT("LODMaxRange"), StaticMeshObj->LODMaxRange, Ar, PortFlags);
			InStaticMesh->LODMaxRange = appAtof(*ParsedText);
		}
		else
		if (Parse(Str, TEXT("THUMBNAILANGLE="), ParsedText))
		{
			INT Index = StrLine.InStr(TEXT("="));
			Str += Index + 1;
// 			/** Thumbnail */
// 			ExportObjectStructProperty(TEXT("ThumbnailAngle"), TEXT("Rotator"), (BYTE*)&(StaticMeshObj->ThumbnailAngle), StaticMeshObj, Ar, PortFlags);
			UScriptStruct* TheStruct = FindField<UScriptStruct>(UObject::StaticClass(), TEXT("Rotator"));
			check(TheStruct);
			FString DataString;

			UStructProperty_ImportText(TheStruct, Str, (BYTE*)&(InStaticMesh->ThumbnailAngle), 0, InStaticMesh, NULL);
		}
		else
		if (Parse(Str, TEXT("THUMBNAILDISTANCE="), ParsedText))
		{
			//ExportFloatProperty(TEXT("ThumbnailDistance"), StaticMeshObj->ThumbnailDistance, Ar, PortFlags);
			InStaticMesh->ThumbnailDistance = appAtof(*ParsedText);
		}
		else
		if (Parse(Str, TEXT("LIGHTMAPRESOLUTION="), ParsedText))
		{
			//ExportIntProperty(TEXT("LightMapResolution"), StaticMeshObj->LightMapResolution, Ar, PortFlags);
			InStaticMesh->LightMapResolution = appAtoi(*ParsedText);
		}
		else
		if (Parse(Str, TEXT("LIGHTMAPCOORDINATEINDEX="), ParsedText))
		{
			//ExportIntProperty(TEXT("LightMapCoordinateIndex"), StaticMeshObj->LightMapCoordinateIndex, Ar, PortFlags);
			InStaticMesh->LightMapCoordinateIndex = appAtoi(*ParsedText);
		}
		else
		if (Parse(Str, TEXT("BOUNDS="), ParsedText))
		{
			//ExportObjectStructProperty(TEXT("Bounds"), TEXT("BoxSphereBounds"), (BYTE*)&(StaticMeshObj->Bounds), StaticMeshObj, Ar, PortFlags);
			INT Index = StrLine.InStr(TEXT("="));
			Str += Index + 1;
			UScriptStruct* TheStruct = FindField<UScriptStruct>(UObject::StaticClass(), TEXT("BoxSphereBounds"));
			check(TheStruct);
			FString DataString;

			UStructProperty_ImportText(TheStruct, Str, (BYTE*)&(InStaticMesh->Bounds), 0, InStaticMesh, NULL);
		}
// 			//
		else
		if (Parse(Str, TEXT("USESIMPLELINECOLLISION="), ParsedText))
		{
			//ExportBooleanProperty(TEXT("UseSimpleLineCollision"), StaticMeshObj->UseSimpleLineCollision, Ar, PortFlags);
			InStaticMesh->UseSimpleLineCollision = BOOL_STRING_IS_TRUE(ParsedText);
		}
		else
		if (Parse(Str, TEXT("USESIMPLEBOXCOLLISION="), ParsedText))
		{
			//ExportBooleanProperty(TEXT("UseSimpleBoxCollision"), StaticMeshObj->UseSimpleBoxCollision, Ar, PortFlags);
			InStaticMesh->UseSimpleBoxCollision = BOOL_STRING_IS_TRUE(ParsedText);
		}
		else
		if (Parse(Str, TEXT("USESIMPLERIGIDBODYCOLLISION="), ParsedText))
		{
			//ExportBooleanProperty(TEXT("UseSimpleRigidBodyCollision"), StaticMeshObj->UseSimpleRigidBodyCollision, Ar, PortFlags);
			InStaticMesh->UseSimpleRigidBodyCollision = BOOL_STRING_IS_TRUE(ParsedText);
		}
		else
		if (Parse(Str, TEXT("USEFULLPRECISIONUVS="), ParsedText))
		{
			//ExportBooleanProperty(TEXT("UseFullPrecisionUVs"), StaticMeshObj->UseFullPrecisionUVs, Ar, PortFlags);
			InStaticMesh->UseFullPrecisionUVs = BOOL_STRING_IS_TRUE(ParsedText);
		}
		else
		if (Parse(Str, TEXT("BUSEDFORINSTANCING="), ParsedText))
		{
			//ExportBooleanProperty(TEXT("bUsedForInstancing"), StaticMeshObj->bUsedForInstancing, Ar, PortFlags);
			InStaticMesh->bUsedForInstancing = BOOL_STRING_IS_TRUE(ParsedText);
		}
		else
		if (Parse(Str, TEXT("BUSEMAXIMUMSTREAMINGTEXELRATIO="), ParsedText))
		{
			//ExportBooleanProperty(TEXT("bUseMaximumStreamingTexelRatio"), StaticMeshObj->bUseMaximumStreamingTexelRatio, Ar, PortFlags);
			InStaticMesh->bUseMaximumStreamingTexelRatio = BOOL_STRING_IS_TRUE(ParsedText);
		}
		else
		if (Parse(Str, TEXT("BPARTITIONFOREDGEGEOMETRY="), ParsedText))
		{
			//ExportBooleanProperty(TEXT("bPartitionForEdgeGeometry"), StaticMeshObj->bPartitionForEdgeGeometry, Ar, PortFlags);
			InStaticMesh->bPartitionForEdgeGeometry = BOOL_STRING_IS_TRUE(ParsedText);
		}
		else
		if (Parse(Str, TEXT("BCANBECOMEDYNAMIC="), ParsedText))
		{
			//ExportBooleanProperty(TEXT("bCanBecomeDynamic"), StaticMeshObj->bCanBecomeDynamic, Ar, PortFlags);
			InStaticMesh->bCanBecomeDynamic = BOOL_STRING_IS_TRUE(ParsedText);
		}
		else
		if (Parse(Str, TEXT("BSTRIPKDOPFORCONSOLE="), ParsedText))
		{
			//ExportBooleanProperty(TEXT("bStripkDOPForConsole"), StaticMeshObj->bStripkDOPForConsole, Ar, PortFlags);
			InStaticMesh->bStripkDOPForConsole = BOOL_STRING_IS_TRUE(ParsedText);
		}
		else
		if (Parse(Str, TEXT("INTERNALVERSION="), ParsedText))
		{
			//ExportIntProperty(TEXT("InternalVersion"), StaticMeshObj->InternalVersion, Ar, PortFlags);
			InStaticMesh->InternalVersion = appAtoi(*ParsedText);
		}
		else
		if (Parse(Str, TEXT("HIGHRESSOURCEMESHNAME="), ParsedText))
		{
			//ExportStringProperty(TEXT("HighResSourceMeshName"), StaticMeshObj->HighResSourceMeshName, Ar, PortFlags);
			InStaticMesh->HighResSourceMeshName = ParsedText;
		}
		else
		if (Parse(Str, TEXT("HIGHRESSOURCEMESHCRC="), ParsedText))
		{
			//ExportIntProperty(TEXT("HighResSourceMeshCRC"), StaticMeshObj->HighResSourceMeshCRC, Ar, PortFlags);
			InStaticMesh->HighResSourceMeshCRC = appAtoi(*ParsedText);
		}
		else
		if (Parse(Str, TEXT("STREAMINGDISTANCEMULTIPLIER="), ParsedText))
		{
			//ExportFloatProperty(TEXT("StreamingDistanceMultiplier"), StaticMeshObj->StreamingDistanceMultiplier, Ar, PortFlags);
			InStaticMesh->StreamingDistanceMultiplier = appAtof(*ParsedText);
		}
		else
		if (GetBEGIN(&Str, TEXT("SMRENDERDATA")))
		{
			///** Array of LODs, holding their associated rendering and collision data */
			////TIndirectArray<FStaticMeshRenderData>	LODModels;
			//for (INT ModelIndex = 0; ModelIndex < StaticMeshObj->LODModels.Num(); ModelIndex++)
			//{
			//	FStaticMeshRenderData& Model = StaticMeshObj->LODModels(ModelIndex);
			//	ExportRenderData(Model, Ar, Warn, PortFlags);
			//}

			FStaticMeshRenderData* NewRenderData = new(InStaticMesh->LODModels) FStaticMeshRenderData();
			INT ElementCount = 0;
			INT ElementSize = 0;

			while(ParseLine(&Buffer,StrLine))
			{
				Str = *StrLine;
				if (Parse(Str, TEXT("NUMVERTICES="), ParsedText))
				{
					//ExportIntProperty(TEXT("NumVertices"), Model.NumVertices, Ar, PortFlags);
					NewRenderData->NumVertices = appAtoi(*ParsedText);
				}
				else
				if (GetBEGIN(&Str, TEXT("ELEMENTS")))
				{
					INT ExportedCount = 0;
					while(ParseLine(&Buffer,StrLine))
					{
						Str = *StrLine;

						if (GetEND(&Str, TEXT("ELEMENTS")))
						{
							break;
						}
						else
						if (Parse(Str, TEXT("COUNT="), ParsedText))
						{
							ExportedCount = appAtoi(*ParsedText);
						}

						if (GetBEGIN(&Str, TEXT("STATICMESHELEMENT")))
						{
							FStaticMeshElement* NewElement = new(NewRenderData->Elements)FStaticMeshElement;
							check(NewElement);

							while(ParseLine(&Buffer,StrLine))
							{
								Str = *StrLine;

								if (GetEND(&Str, TEXT("STATICMESHELEMENT")))
								{
									break;
								}
								else
								if (Parse(Str, TEXT("MATERIAL="), ParsedText))
								{
									 //Material=ExportTest.Materials.MAT_Testing
									if (ParsedText.Len() > 0)
									{
										NewElement->Material = FindObject<UMaterialInterface>(ANY_PACKAGE, *ParsedText);
										if (NewElement->Material == NULL)
										{
											INT PeriodIndex = ParsedText.InStr(TEXT("."));
											if (PeriodIndex != -1)
											{
												FString PackageString = ParsedText.Left(PeriodIndex);
												if (InStaticMesh->GetPathName().InStr(PackageString) != -1)
												{
													FString PartialName = ParsedText.Right(ParsedText.Len() - PeriodIndex - 1);
													// Try again...
													NewElement->Material = FindObject<UMaterialInterface>(ANY_PACKAGE, *PartialName);
												}
											}
										}
										if (NewElement->Material == NULL)
										{
											debugf(TEXT("Materials:"));
											for (TObjectIterator<UMaterialInterface> MatIt; MatIt; ++MatIt)
											{
												UMaterialInterface* MatIF = *MatIt;
												debugf(TEXT("\t%s"), *(MatIF->GetPathName()));
											}
											warnf(TEXT("Failed to find material %s for mesh %s"), *ParsedText, *(InStaticMesh->GetPathName()));
										}
									}
								}
								else
								if (Parse(Str, TEXT("ENABLECOLLISION="), ParsedText))
								{
									NewElement->EnableCollision = BOOL_STRING_IS_TRUE(ParsedText);
								}
								else
								if (Parse(Str, TEXT("OLDENABLECOLLISION="), ParsedText))
								{
									NewElement->OldEnableCollision = BOOL_STRING_IS_TRUE(ParsedText);
								}
								else
								if (Parse(Str, TEXT("BENABLESHADOWCASTING="), ParsedText))
								{
									NewElement->bEnableShadowCasting = BOOL_STRING_IS_TRUE(ParsedText);
								}
								else
								if (Parse(Str, TEXT("FIRSTINDEX="), ParsedText))
								{
									NewElement->FirstIndex = appAtoi(*ParsedText);
								}
								else
								if (Parse(Str, TEXT("NUMTRIANGLES="), ParsedText))
								{
									NewElement->NumTriangles = appAtoi(*ParsedText);
								}
								else
								if (Parse(Str, TEXT("MINVERTEXINDEX="), ParsedText))
								{
									NewElement->MinVertexIndex = appAtoi(*ParsedText);
								}
								else
								if (Parse(Str, TEXT("MAXVERTEXINDEX="), ParsedText))
								{
									NewElement->MaxVertexIndex = appAtoi(*ParsedText);
								}
								else
								if (Parse(Str, TEXT("MATERIALINDEX="), ParsedText))
								{
									NewElement->MaterialIndex = appAtoi(*ParsedText);
								}
								else
								if (GetBEGIN(&Str, TEXT("FRAGMENTS")))
								{
									//Fragments=((0,12))
									while(ParseLine(&Buffer,StrLine))
									{
										Str = *StrLine;

										if (GetEND(&Str, TEXT("FRAGMENTS")))
										{
											break;
										}

										//Assume it is a line of BaseIndex,NumPrimitives...
										FFragmentRange* NewFrag = new(NewElement->Fragments)FFragmentRange;
										check(NewFrag);

										if (appStrstr(Str, TEXT(",")) != NULL)
										{
											appSSCANF(Str, TEXT("%d,%d"), &(NewFrag->BaseIndex), &(NewFrag->NumPrimitives));
										}
									}
								}
							}
						}
					}

					check(ExportedCount == NewRenderData->Elements.Num());
				}

				if (GetBEGIN(&Str, TEXT("STATICMESHTRIANGLEBULKDATA")))
				{
					//@todo. Move this to some common function
					while(ParseLine(&Buffer,StrLine))
					{
						Str = *StrLine;
						if (GetBEGIN(&Str, TEXT("UNTYPEDBULKDATA")))
						{
							ImportUntypedBulkDataFromText(Buffer, NewRenderData->RawTriangles);
						}
						else
						if (GetEND(&Str, TEXT("STATICMESHTRIANGLEBULKDATA")))
						{
							break;
						}
					}
				}
				else
				if (GetEND(&Str, TEXT("SMRENDERDATA")))
				{
					break;
				}
			}
		}
	}

	// Update LOD info with the materials
	InStaticMesh->LODInfo.Empty();
	InStaticMesh->LODInfo.AddZeroed(InStaticMesh->LODModels.Num());
	for (INT LODIndex = 0; LODIndex < InStaticMesh->LODModels.Num(); LODIndex++)
	{
		FStaticMeshRenderData& LODData = InStaticMesh->LODModels(LODIndex);
		FStaticMeshLODInfo& LODInfo = InStaticMesh->LODInfo(LODIndex);

		LODInfo.Elements.Empty();
		LODInfo.Elements.AddZeroed( LODData.Elements.Num() );
		for (INT MatIndex = 0; MatIndex < LODData.Elements.Num(); MatIndex++)
		{
			LODInfo.Elements(MatIndex).Material = LODData.Elements(MatIndex).Material;
			LODInfo.Elements(MatIndex).bEnableShadowCasting = LODData.Elements(MatIndex).bEnableShadowCasting;
			LODInfo.Elements(MatIndex).bEnableCollision = LODData.Elements(MatIndex).EnableCollision;
		}
	}

	InStaticMesh->Build();

	return TRUE;
}

IMPLEMENT_CLASS(UStaticMeshFactory);
