/*=============================================================================
	UnPhysCollision.cpp: Skeletal mesh collision code
	Copyright 2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"

#ifdef WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

///////////////////////////////////////
///////////// UKMeshProps /////////////
///////////////////////////////////////


void UKMeshProps::CopyMeshPropsFrom(UKMeshProps* fromProps)
{
	COMNudge = fromProps->COMNudge;
	AggGeom = fromProps->AggGeom;
}

void UKMeshProps::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
}

///////////////////////////////////////
/////////// FKAggregateGeom ///////////
///////////////////////////////////////

FBox FKAggregateGeom::CalcAABB(const FMatrix& BoneTM, FLOAT Scale)
{
	FBox Box(0);

	for(INT i=0; i<SphereElems.Num(); i++)
		Box += SphereElems(i).CalcAABB(BoneTM, Scale);

	for(INT i=0; i<BoxElems.Num(); i++)
		Box += BoxElems(i).CalcAABB(BoneTM, Scale);

	for(INT i=0; i<SphylElems.Num(); i++)
		Box += SphylElems(i).CalcAABB(BoneTM, Scale);

	for(INT i=0; i<ConvexElems.Num(); i++)
		Box += ConvexElems(i).CalcAABB(BoneTM, Scale);

	return Box;

}

// These line-checks do the normal weird Unreal thing of returning 0 if we hit or 1 otherwise.
UBOOL FKAggregateGeom::LineCheck(FCheckResult& Result, const FMatrix& Matrix, const FVector& Scale3D,  const FVector& End, const FVector& Start, const FVector& Extent)
{
	// JTODO: Do some kind of early rejection here! This is a bit crap...

	Result.Time = 1.0f;

	FCheckResult TempResult;

	if( Scale3D.IsUniform() )
	{
		for(INT i=0; i<SphereElems.Num(); i++)
		{
			TempResult.Time = 1.0f;

			FMatrix ElemTM = SphereElems(i).TM;
			ElemTM.ScaleTranslation(Scale3D);
			ElemTM *= Matrix;

			SphereElems(i).LineCheck(TempResult, ElemTM, Scale3D.X, End, Start, Extent);

			if(TempResult.Time < Result.Time)
				Result = TempResult;
		}

		for(INT i=0; i<BoxElems.Num(); i++)
		{
			TempResult.Time = 1.0f;

			FMatrix ElemTM = BoxElems(i).TM;
			ElemTM.ScaleTranslation(Scale3D);
			ElemTM *= Matrix;

			BoxElems(i).LineCheck(TempResult, ElemTM, Scale3D.X, End, Start, Extent);

			if(TempResult.Time < Result.Time)
				Result = TempResult;
		}

		for(INT i=0; i<SphylElems.Num(); i++)
		{
			TempResult.Time = 1.0f;

			FMatrix ElemTM = SphylElems(i).TM;
			ElemTM.ScaleTranslation(Scale3D);
			ElemTM *= Matrix;

			SphylElems(i).LineCheck(TempResult, ElemTM, Scale3D.X, End, Start, Extent);

			if(TempResult.Time < Result.Time)
				Result = TempResult;
		}
	}

	for(INT i=0; i<ConvexElems.Num(); i++)
	{
		TempResult.Time = 1.0f;

		ConvexElems(i).LineCheck(TempResult, Matrix, Scale3D, End, Start, Extent);

		if(TempResult.Time < Result.Time)
			Result = TempResult;
	}

	if(Result.Time < 1.0f)
		return 0;
	else
		return 1;

}

#ifdef WITH_NOVODEX

static void ScaleNovodexTMPosition(NxMat34& TM, const FVector& Scale3D)
{
	TM.t.x *= Scale3D.X;
	TM.t.y *= Scale3D.Y;
	TM.t.z *= Scale3D.Z;
}

NxActorDesc* FKAggregateGeom::InstanceNovodexGeom(const FVector& uScale3D, const TCHAR* debugName)
{
	// Convert scale to physical units.
	FVector pScale3D = uScale3D * U2PScale;

	UINT NumElems;
	if (pScale3D.IsUniform())
	{
		NumElems = GetElementCount();
	}
	else
	{
		NumElems = ConvexElems.Num();
	}

	if (NumElems == 0)
	{
		if (!pScale3D.IsUniform() && GetElementCount() > 0)
		{
			debugf(TEXT("FKAggregateGeom::InstanceNovodexGeom: (%s) Cannot 3D-Scale rigid-body primitives (sphere, box, sphyl)."), debugName);
		}
		else
		{
			debugf(TEXT("FKAggregateGeom::InstanceNovodexGeom: (%s) No geometries in FKAggregateGeom."), debugName);
		}
	}

	NxActorDesc* ActorDesc = new NxActorDesc;

	// Include spheres, boxes and sphyls only when the scale is uniform.
	if (pScale3D.IsUniform())
	{
		// Sphere primitives
		for (int i = 0; i < SphereElems.Num(); i++)
		{
			FKSphereElem* SphereElem = &SphereElems(i);
			
			NxMat34 RelativeTM = U2NMatrixCopy(SphereElem->TM);
			ScaleNovodexTMPosition(RelativeTM, pScale3D);

			NxSphereShapeDesc* SphereDesc = new NxSphereShapeDesc;
			SphereDesc->radius = SphereElem->Radius * pScale3D.X;
			SphereDesc->localPose = RelativeTM;

			ActorDesc->shapes.pushBack(SphereDesc);
		}

		// Box primitives
		for (int i = 0; i < BoxElems.Num(); i++)
		{
			FKBoxElem* BoxElem = &BoxElems(i);

			NxMat34 RelativeTM = U2NMatrixCopy(BoxElem->TM);
			ScaleNovodexTMPosition(RelativeTM, pScale3D);

			NxBoxShapeDesc* BoxDesc = new NxBoxShapeDesc;
			BoxDesc->dimensions = 0.5f * NxVec3(BoxElem->X * pScale3D.X, BoxElem->Y * pScale3D.X, BoxElem->Z * pScale3D.X);
			BoxDesc->localPose = RelativeTM;

			ActorDesc->shapes.pushBack(BoxDesc);
		}

		// Sphyl (aka Capsule) primitives
		for (int i =0; i < SphylElems.Num(); i++)
		{
			FKSphylElem* SphylElem = &SphylElems(i);

			// The stored sphyl transform assumes the sphyl axis is down Z. In Novodex, it points down Y, so we twiddle the matrix a bit here (swap Y and Z and negate X).
			FMatrix SphylRelTM = FMatrix::Identity;
			SphylRelTM.SetAxis( 0, -1.f * SphylElem->TM.GetAxis(0) );
			SphylRelTM.SetAxis( 1, SphylElem->TM.GetAxis(2) );
			SphylRelTM.SetAxis( 2, SphylElem->TM.GetAxis(1) );
			SphylRelTM.SetOrigin( SphylElem->TM.GetOrigin() );

			NxMat34 RelativeTM = U2NMatrixCopy(SphylRelTM);
			ScaleNovodexTMPosition(RelativeTM, pScale3D);

			NxCapsuleShapeDesc* CapsuleDesc = new NxCapsuleShapeDesc;
			CapsuleDesc->radius = SphylElem->Radius * pScale3D.X;
			CapsuleDesc->height = SphylElem->Length * pScale3D.X;
			CapsuleDesc->localPose = RelativeTM;

			ActorDesc->shapes.pushBack(CapsuleDesc);
		}
	}

	// Convex mesh primitives
	for (int i = 0; i < ConvexElems.Num(); i++)
	{
		FKConvexElem* ConvexElem = &ConvexElems(i);

#if 1
		// First scale verts stored into physics scsale - including any non-uniform scaling for this mesh.
		TArray<FVector> PhysScaleConvexVerts;
		PhysScaleConvexVerts.Add( ConvexElem->VertexData.Num() );

		for (int j = 0; j < ConvexElem->VertexData.Num(); j++)
		{
			PhysScaleConvexVerts(j) = ConvexElem->VertexData(j) * pScale3D;
		}

		// Create mesh. Will compute convex hull of supplied verts.
		NxTriangleMeshDesc ConvexMeshDesc;
		ConvexMeshDesc.numVertices = PhysScaleConvexVerts.Num();
		ConvexMeshDesc.pointStrideBytes = sizeof(FVector);
		ConvexMeshDesc.points = PhysScaleConvexVerts.GetData();
		ConvexMeshDesc.flags = NX_MF_CONVEX|NX_MF_COMPUTE_CONVEX;

		NxTriangleMesh* ConvexMesh = GNovodexSDK->createTriangleMesh(ConvexMeshDesc);

		// Convex mesh creation may fail - in which case NULL will be returned and we ignore this part.
		if(ConvexMesh)
		{
			NxTriangleMeshShapeDesc* ConvexShapeDesc = new NxTriangleMeshShapeDesc;
			ConvexShapeDesc->meshData = ConvexMesh;
			ConvexShapeDesc->meshFlags = 0;

			ActorDesc->shapes.pushBack(ConvexShapeDesc);
		}
		else
		{
			debugf( TEXT("FKAggregateGeom::InstanceNovodexGeom: (%s) Problem instancing ConvexElems(%d)."), debugName, i );
		}
#else
		FBox Box(0);
		for (int j = 0; j < ConvexElem->VertexData.Num(); j++)
			Box += ConvexElem->VertexData(j) * pScale3D;

		// Construct the translation that places the origin of the coordinate system at the center of the bounding box.
		NxVec3 Translation;
		Translation.x = 0.5f * (Box.Min.X + Box.Max.X);
		Translation.y = 0.5f * (Box.Min.Y + Box.Max.Y);
		Translation.z = 0.5f * (Box.Min.Z + Box.Max.Z);
		
		NxMat34 RelativeTM;
		RelativeTM.t = Translation;

		NxBoxShapeDesc* BoxDesc = new NxBoxShapeDesc;
		BoxDesc->dimensions = 0.5f * NxVec3(Box.Max.X - Box.Min.X, Box.Max.Y - Box.Min.Y, Box.Max.Z - Box.Min.Z);
		BoxDesc->localPose = RelativeTM;

		ActorDesc->shapes.pushBack(BoxDesc);
#endif
	}

	return ActorDesc;
}
#endif // WITH_NOVODEX

///////////////////////////////////////
///////////// FKSphereElem ////////////
///////////////////////////////////////

FBox FKSphereElem::CalcAABB(const FMatrix& BoneTM, FLOAT Scale)
{
	FMatrix ElemTM = TM;
	ElemTM.ScaleTranslation( FVector(Scale) );
	ElemTM *= BoneTM;

	FVector BoxCenter = ElemTM.GetOrigin();
	FVector BoxExtents(Radius * Scale);

	return FBox(BoxCenter - BoxExtents, BoxCenter + BoxExtents);

}

static UBOOL SphereLineIntersect(FCheckResult& Result, const FVector& Origin, FLOAT Radius, const FVector& Start, const FVector& Dir, FLOAT Length)
{
	FVector StartToOrigin = Origin - Start;

	FLOAT L2 = StartToOrigin.SizeSquared(); // Distance of line start from sphere centre (squared).
	FLOAT R2 = Radius*Radius;

	// If we are starting inside sphere, return a hit.
	if ( L2 < R2 )
	{
		Result.Time = 0.0f;
		Result.Location = Start;
		Result.Normal = -StartToOrigin.SafeNormal();

		return 0;
	}

	if(Length < KINDA_SMALL_NUMBER)
		return 1; // Zero length and not starting inside - doesn't hit.

	FLOAT D = StartToOrigin | Dir; // distance of sphere centre in direction of query.
	if ( D < 0.0f )
		return 1; // sphere is behind us, but we are not inside it.

	FLOAT M2 = L2 - (D*D); // pythag - triangle involving StartToCenter, (Dir * D) and vec between SphereCenter & (Dir * D)
	if (M2 > R2) 
		return 1; // ray misses sphere

	// this does pythag again. Q2 = R2 (radius squared) - M2
	FLOAT t = D - appSqrt(R2 - M2);
	if ( t > Length ) 
		return 1; // Ray doesn't reach sphere, reject here.

	Result.Location = Start + Dir * t;
	Result.Normal = (1.0f/Radius) * (Result.Location - Origin);
	Result.Time = t * (1.0f/Length);

	return 0;

}

UBOOL FKSphereElem::LineCheck(FCheckResult& Result, const FMatrix& Matrix, FLOAT Scale, const FVector& End, const FVector& Start, const FVector& Extent)
{
	if( !Extent.IsZero() )
		return 1;

	FVector SphereCenter = Matrix.GetOrigin();
	FVector Dir = End - Start;
	FLOAT Length = Dir.Size();
	if(Length > KINDA_SMALL_NUMBER)
		Dir *= (1.0f/Length);

	return SphereLineIntersect(Result, SphereCenter, Radius*Scale, Start, Dir, Length);

}

///////////////////////////////////////
////////////// FKBoxElem //////////////
///////////////////////////////////////

FBox FKBoxElem::CalcAABB(const FMatrix& BoneTM, FLOAT Scale)
{
	FMatrix ElemTM = TM;
	ElemTM.ScaleTranslation( FVector(Scale) );
	ElemTM *= BoneTM;

	FVector Extent(0.5f * Scale * X, 0.5f * Scale * Y, 0.5f * Scale * Z);
	FBox LocalBox(-Extent, Extent);

	return LocalBox.TransformBy(ElemTM);

}

UBOOL FKBoxElem::LineCheck(FCheckResult& Result, const FMatrix& Matrix, FLOAT Scale,  const FVector& End, const FVector& Start, const FVector& Extent)
{
	if( !Extent.IsZero() )
		return 1;

	// Transform start and end points into box space.
	FVector LocalStart = Matrix.InverseTransformFVector(Start);
	FVector LocalEnd = Matrix.InverseTransformFVector(End);

	FVector Radii = Scale * 0.5f * FVector(X, Y, Z);
	FBox LocalBox( -Radii, Radii);

	FVector LocalHitLocation, LocalHitNormal;
	FLOAT HitTime;

	UBOOL bHit = FLineExtentBoxIntersection(LocalBox, LocalStart, LocalEnd, FVector(0,0,0), LocalHitLocation, LocalHitNormal, HitTime);

	if(bHit)
	{
		Result.Location = Matrix.TransformFVector(LocalHitLocation);
		Result.Normal = Matrix.TransformNormal(LocalHitNormal);
		Result.Time = HitTime;

		return 0;
	}
	else
		return 1;


}

///////////////////////////////////////
////////////// FKSphylElem ////////////
///////////////////////////////////////

FBox FKSphylElem::CalcAABB(const FMatrix& BoneTM, FLOAT Scale)
{
	FMatrix ElemTM = TM;
	ElemTM.ScaleTranslation( FVector(Scale) );
	ElemTM *= BoneTM;

	FVector SphylCenter = ElemTM.GetOrigin();

	FVector StartPos = SphylCenter + Scale*0.5f*Length*ElemTM.GetAxis(2);
	FVector EndPos = SphylCenter - Scale*0.5f*Length*ElemTM.GetAxis(2);
	FVector Extent(Scale*Radius);

	FBox StartBox(StartPos-Extent, StartPos+Extent);
	FBox EndBox(EndPos-Extent, EndPos+Extent);

	return StartBox + EndBox;

}

UBOOL FKSphylElem::LineCheck(FCheckResult& Result, const FMatrix& Matrix, FLOAT Scale,  const FVector& End, const FVector& Start, const FVector& Extent)
{
	FVector LocalStart = Matrix.InverseTransformFVector(Start);
	FVector LocalEnd = Matrix.InverseTransformFVector(End);
	FLOAT HalfHeight = Scale*0.5f*Length;

	// bools indicate if line enters different areas of sphyl
	UBOOL doTest[3] = {0, 0, 0};
	if(LocalStart.Z >= HalfHeight) // start in top
	{
		doTest[0] = 1;

		if(LocalEnd.Z < HalfHeight)
		{
			doTest[1] = 1;

			if(LocalEnd.Z < -HalfHeight)
				doTest[2] = 1;
		}
	}
	else if(LocalStart.Z >= -HalfHeight) // start in middle
	{
		doTest[1] = 1;

		if(LocalEnd.Z >= HalfHeight)
			doTest[0] = 1;

		if(LocalEnd.Z < -HalfHeight)
			doTest[2] = 1;
	}
	else // start at bottom
	{
		doTest[2] = 1;

		if(LocalEnd.Z >= -HalfHeight)
		{
			doTest[1] = 1;

			if(LocalEnd.Z >= HalfHeight)
				doTest[0] = 1;
		}
	}


	// find line in terms of point and unit direction vector
	FVector RayDir = LocalEnd - LocalStart;
	FLOAT RayLength = RayDir.Size();
	FLOAT RecipRayLength = (1.0f/RayLength);
	if(RayLength > KINDA_SMALL_NUMBER)
		RayDir *= RecipRayLength;

	FVector SphereCenter(0);
	FLOAT R = Radius*Scale;
	FLOAT R2 = R*R;

	// Check line against each sphere, then the cylinder Because shape
	// is convex, once we hit something, we dont have to check any more.
	UBOOL bNoHit = 1;
	FCheckResult LocalResult;

	if(doTest[0])
	{
		SphereCenter.Z = HalfHeight;
		bNoHit = SphereLineIntersect(LocalResult, SphereCenter, R, LocalStart, RayDir, RayLength);

		// Discard hit if in cylindrical region.
		if(!bNoHit && LocalResult.Location.Z < HalfHeight)
			bNoHit = 1;
	}

	if(doTest[2] && bNoHit)
	{
		SphereCenter.Z = -HalfHeight;
		bNoHit = SphereLineIntersect(LocalResult, SphereCenter, R, LocalStart, RayDir, RayLength);

		if(!bNoHit && LocalResult.Location.Z > -HalfHeight)
			bNoHit = 1;
	}

	if(doTest[1] && bNoHit)
	{
		// First, check if start is inside cylinder. If so - just return it.
		if( LocalStart.SizeSquared2D() <= R2 && LocalStart.Z <= HalfHeight && LocalStart.Z >= -HalfHeight )
		{
			Result.Location = Start;
			Result.Normal = -RayDir;
			Result.Time = 0.0f;

			return 0;
		}
		else // if not.. solve quadratic
		{
			FLOAT A = RayDir.SizeSquared2D();
			FLOAT B = 2*(LocalStart.X*RayDir.X + LocalStart.Y*RayDir.Y);
			FLOAT C = LocalStart.SizeSquared2D() - R2;
			FLOAT disc = B*B - 4*A*C;

			if (disc >= 0 && Abs(A) > KINDA_SMALL_NUMBER*KINDA_SMALL_NUMBER) 
			{
				FLOAT root = appSqrt(disc);
				FLOAT s = (-B-root)/(2*A);
				FLOAT z1 = LocalStart.Z + s*RayDir.Z;

				// if its not before the start of the line, or past its end, or beyond the end of the cylinder, we have a hit!
				if(s > 0 && s < RayLength && z1 <= HalfHeight && z1 >= -HalfHeight)
				{
					LocalResult.Time = s * RecipRayLength;

					LocalResult.Location.X = LocalStart.X + s*RayDir.X;
					LocalResult.Location.Y = LocalStart.Y + s*RayDir.Y;
					LocalResult.Location.Z = z1;

					LocalResult.Normal.X = LocalResult.Location.X;
					LocalResult.Normal.Y = LocalResult.Location.Y;
					LocalResult.Normal.Z = 0;
					LocalResult.Normal.Normalize();

					bNoHit = 0;
				}
			}
		}
	}

	// If we didn't hit anything - return
	if(bNoHit)
		return 1;

	Result.Location = Matrix.TransformFVector(LocalResult.Location);
	Result.Normal = Matrix.TransformNormal(LocalResult.Normal);
	Result.Time = LocalResult.Time;

	return 0;

}

///////////////////////////////////////
///////////// FKConvexElem ////////////
///////////////////////////////////////

FBox FKConvexElem::CalcAABB(const FMatrix& BoneTM, FLOAT Scale)
{
	FVector BoxCenter = BoneTM.GetOrigin();

	return FBox(BoxCenter, BoxCenter);

}

UBOOL FKConvexElem::LineCheck(FCheckResult& Result, const FMatrix& Matrix, const FVector& Scale3D,  const FVector& End, const FVector& Start, const FVector& Extent)
{
	return 1;

}