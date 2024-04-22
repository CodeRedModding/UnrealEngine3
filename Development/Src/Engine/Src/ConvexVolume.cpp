/*=============================================================================
	ConvexVolume.cpp: Convex volume implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

/**
 * Builds the permuted planes for SIMD fast clipping
 */
void FConvexVolume::Init(void)
{
	INT NumToAdd = Planes.Num() / 4;
	INT NumRemaining = Planes.Num() % 4;
	// Presize the array
	PermutedPlanes.Empty(NumToAdd * 4 + (NumRemaining ? 4 : 0));
	// For each set of four planes
	for (INT Count = 0, Offset = 0; Count < NumToAdd; Count++, Offset += 4)
	{
		// Add them in SSE ready form
		new(PermutedPlanes)FPlane(Planes(Offset + 0).X,Planes(Offset + 1).X,Planes(Offset + 2).X,Planes(Offset + 3).X);
		new(PermutedPlanes)FPlane(Planes(Offset + 0).Y,Planes(Offset + 1).Y,Planes(Offset + 2).Y,Planes(Offset + 3).Y);
		new(PermutedPlanes)FPlane(Planes(Offset + 0).Z,Planes(Offset + 1).Z,Planes(Offset + 2).Z,Planes(Offset + 3).Z);
		new(PermutedPlanes)FPlane(Planes(Offset + 0).W,Planes(Offset + 1).W,Planes(Offset + 2).W,Planes(Offset + 3).W);
	}
	// Pad the last set so we have an even 4 planes of vert data
	if (NumRemaining)
	{
		FPlane Last1, Last2, Last3, Last4;
		// Read the last set of verts
		switch (NumRemaining)
		{
			case 3:
			{
				Last1 = Planes(NumToAdd * 4 + 0);
				Last2 = Planes(NumToAdd * 4 + 1);
				Last3 = Planes(NumToAdd * 4 + 2);
				Last4 = Last1;
				break;
			}
			case 2:
			{
				Last1 = Planes(NumToAdd * 4 + 0);
				Last2 = Planes(NumToAdd * 4 + 1);
				Last3 = Last4 = Last1;
				break;
			}
			case 1:
			{
				Last1 = Planes(NumToAdd * 4 + 0);
				Last2 = Last3 = Last4 = Last1;
				break;
			}
			default:
			{
				Last1 = FPlane(0, 0, 0, 0);
				Last2 = Last3 = Last4 = Last1;
				break;
			}
		}
		// Add them in SIMD ready form
		new(PermutedPlanes)FPlane(Last1.X,Last2.X,Last3.X,Last4.X);
		new(PermutedPlanes)FPlane(Last1.Y,Last2.Y,Last3.Y,Last4.Y);
		new(PermutedPlanes)FPlane(Last1.Z,Last2.Z,Last3.Z,Last4.Z);
		new(PermutedPlanes)FPlane(Last1.W,Last2.W,Last3.W,Last4.W);
	}
}

//
//	FConvexVolume::ClipPolygon
//

UBOOL FConvexVolume::ClipPolygon(FPoly& Polygon) const
{
	for(INT PlaneIndex = 0;PlaneIndex < Planes.Num();PlaneIndex++)
	{
		const FPlane&	Plane = Planes(PlaneIndex);
		if(!Polygon.Split(-FVector(Plane),Plane * Plane.W))
			return 0;
	}
	return 1;
}


//
//	FConvexVolume::GetBoxIntersectionOutcode
//

FOutcode FConvexVolume::GetBoxIntersectionOutcode(const FVector& Origin,const FVector& Extent) const
{
	FOutcode Result(1,0);

	checkSlow(PermutedPlanes.Num() % 4 == 0);

	// Load the origin & extent
	VectorRegister Orig = VectorLoadFloat3(&Origin);
	VectorRegister Ext = VectorLoadFloat3(&Extent);
	// Splat origin into 3 vectors
	VectorRegister OrigX = VectorReplicate(Orig, 0);
	VectorRegister OrigY = VectorReplicate(Orig, 1);
	VectorRegister OrigZ = VectorReplicate(Orig, 2);
	// Splat extent into 3 vectors
	VectorRegister ExtentX = VectorReplicate(Ext, 0);
	VectorRegister ExtentY = VectorReplicate(Ext, 1);
	VectorRegister ExtentZ = VectorReplicate(Ext, 2);
	// Splat the abs for the pushout calculation
	VectorRegister AbsExt = VectorAbs(Ext);
	VectorRegister AbsExtentX = VectorReplicate(AbsExt, 0);
	VectorRegister AbsExtentY = VectorReplicate(AbsExt, 1);
	VectorRegister AbsExtentZ = VectorReplicate(AbsExt, 2);
	// Since we are moving straight through get a pointer to the data
	const FPlane* RESTRICT PermutedPlanePtr = (FPlane*)PermutedPlanes.GetData();
	// Process four planes at a time until we have < 4 left
	for (INT Count = 0; Count < PermutedPlanes.Num(); Count += 4)
	{
		// Load 4 planes that are already all Xs, Ys, ...
		VectorRegister PlanesX = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesY = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesZ = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesW = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		// Calculate the distance (x * x) + (y * y) + (z * z) - w
		VectorRegister DistX = VectorMultiply(OrigX,PlanesX);
		VectorRegister DistY = VectorMultiplyAdd(OrigY,PlanesY,DistX);
		VectorRegister DistZ = VectorMultiplyAdd(OrigZ,PlanesZ,DistY);
		VectorRegister Distance = VectorSubtract(DistZ,PlanesW);
		// Now do the push out Abs(x * x) + Abs(y * y) + Abs(z * z)
		VectorRegister PushX = VectorMultiply(AbsExtentX,VectorAbs(PlanesX));
		VectorRegister PushY = VectorMultiplyAdd(AbsExtentY,VectorAbs(PlanesY),PushX);
		VectorRegister PushOut = VectorMultiplyAdd(AbsExtentZ,VectorAbs(PlanesZ),PushY);

		// Check for completely outside
		if (VectorAnyGreaterThan(Distance,PushOut))
		{
			Result.SetInside(0);
			Result.SetOutside(1);
			break;
		}

		// See if any part is outside
		if (VectorAnyGreaterThan(Distance,VectorNegate(PushOut)))
		{
			Result.SetOutside(1);
		}
	}

	return Result;
}

//
//	FConvexVolume::IntersectBox
//

static FORCEINLINE UBOOL IntersectBoxWithPermutedPlanes(
	const FConvexVolume::FPermutedPlaneArray& PermutedPlanes,
	const VectorRegister BoxOrigin,
	const VectorRegister BoxExtent )
{
	UBOOL Result = TRUE;

	checkSlow(PermutedPlanes.Num() % 4 == 0);

	// Splat origin into 3 vectors
	VectorRegister OrigX = VectorReplicate(BoxOrigin, 0);
	VectorRegister OrigY = VectorReplicate(BoxOrigin, 1);
	VectorRegister OrigZ = VectorReplicate(BoxOrigin, 2);
	// Splat extent into 3 vectors
	VectorRegister ExtentX = VectorReplicate(BoxExtent, 0);
	VectorRegister ExtentY = VectorReplicate(BoxExtent, 1);
	VectorRegister ExtentZ = VectorReplicate(BoxExtent, 2);
	// Splat the abs for the pushout calculation
	VectorRegister AbsExt = VectorAbs(BoxExtent);
	VectorRegister AbsExtentX = VectorReplicate(AbsExt, 0);
	VectorRegister AbsExtentY = VectorReplicate(AbsExt, 1);
	VectorRegister AbsExtentZ = VectorReplicate(AbsExt, 2);
	// Since we are moving straight through get a pointer to the data
	const FPlane* RESTRICT PermutedPlanePtr = (FPlane*)PermutedPlanes.GetData();
	// Process four planes at a time until we have < 4 left
	for ( INT Count = 0; Count < PermutedPlanes.Num(); Count += 4 )
	{
		// Load 4 planes that are already all Xs, Ys, ...
		VectorRegister PlanesX = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesY = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesZ = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesW = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		// Calculate the distance (x * x) + (y * y) + (z * z) - w
		VectorRegister DistX = VectorMultiply(OrigX,PlanesX);
		VectorRegister DistY = VectorMultiplyAdd(OrigY,PlanesY,DistX);
		VectorRegister DistZ = VectorMultiplyAdd(OrigZ,PlanesZ,DistY);
		VectorRegister Distance = VectorSubtract(DistZ,PlanesW);
		// Now do the push out Abs(x * x) + Abs(y * y) + Abs(z * z)
		VectorRegister PushX = VectorMultiply(AbsExtentX,VectorAbs(PlanesX));
		VectorRegister PushY = VectorMultiplyAdd(AbsExtentY,VectorAbs(PlanesY),PushX);
		VectorRegister PushOut = VectorMultiplyAdd(AbsExtentZ,VectorAbs(PlanesZ),PushY);

		// Check for completely outside
		if ( VectorAnyGreaterThan(Distance,PushOut) )
		{
			Result = FALSE;
			break;
		}
	}
	return Result;
}

UBOOL FConvexVolume::IntersectBox(const FVector& Origin,const FVector& Extent) const
{
	// Load the origin & extent
	const VectorRegister Orig = VectorLoadFloat3( &Origin );
	const VectorRegister Ext = VectorLoadFloat3( &Extent );
	return IntersectBoxWithPermutedPlanes( PermutedPlanes, Orig, Ext );
}

/**
 * Intersection test with a translated axis-aligned box.
 * @param Origin - Origin of the box.
 * @param Translation - Translation to apply to the box.
 * @param Extent - Extent of the box along each axis.
 * @returns TRUE if this convex volume intersects the given translated box.
 */
UBOOL FConvexVolume::IntersectBox( const FVector& Origin,const FVector& Translation,const FVector& Extent ) const
{
	const VectorRegister Orig = VectorLoadFloat3( &Origin );
	const VectorRegister Trans = VectorLoadFloat3( &Translation );	
	const VectorRegister BoxExtent = VectorLoadFloat3( &Extent );
	const VectorRegister BoxOrigin = VectorAdd( Orig, Trans );
	return IntersectBoxWithPermutedPlanes( PermutedPlanes, BoxOrigin, BoxExtent );
}


UBOOL FConvexVolume::IntersectBox(const FVector& Origin,const FVector& Extent, UBOOL& bOutFullyContained) const
{
	UBOOL Result = TRUE;

	// Assume fully contained
	bOutFullyContained = TRUE;

	checkSlow(PermutedPlanes.Num() % 4 == 0);

	// Load the origin & extent
	VectorRegister Orig = VectorLoadFloat3(&Origin);
	VectorRegister Ext = VectorLoadFloat3(&Extent);
	// Splat origin into 3 vectors
	VectorRegister OrigX = VectorReplicate(Orig, 0);
	VectorRegister OrigY = VectorReplicate(Orig, 1);
	VectorRegister OrigZ = VectorReplicate(Orig, 2);
	// Splat extent into 3 vectors
	VectorRegister ExtentX = VectorReplicate(Ext, 0);
	VectorRegister ExtentY = VectorReplicate(Ext, 1);
	VectorRegister ExtentZ = VectorReplicate(Ext, 2);
	// Splat the abs for the pushout calculation
	VectorRegister AbsExt = VectorAbs(Ext);
	VectorRegister AbsExtentX = VectorReplicate(AbsExt, 0);
	VectorRegister AbsExtentY = VectorReplicate(AbsExt, 1);
	VectorRegister AbsExtentZ = VectorReplicate(AbsExt, 2);
	// Since we are moving straight through get a pointer to the data
	const FPlane* RESTRICT PermutedPlanePtr = (FPlane*)PermutedPlanes.GetData();
	// Process four planes at a time until we have < 4 left
	for (INT Count = 0; Count < PermutedPlanes.Num(); Count += 4)
	{
		// Load 4 planes that are already all Xs, Ys, ...
		VectorRegister PlanesX = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesY = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesZ = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesW = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		// Calculate the distance (x * x) + (y * y) + (z * z) - w
		VectorRegister DistX = VectorMultiply(OrigX,PlanesX);
		VectorRegister DistY = VectorMultiplyAdd(OrigY,PlanesY,DistX);
		VectorRegister DistZ = VectorMultiplyAdd(OrigZ,PlanesZ,DistY);
		VectorRegister Distance = VectorSubtract(DistZ,PlanesW);
		// Now do the push out Abs(x * x) + Abs(y * y) + Abs(z * z)
		VectorRegister PushX = VectorMultiply(AbsExtentX,VectorAbs(PlanesX));
		VectorRegister PushY = VectorMultiplyAdd(AbsExtentY,VectorAbs(PlanesY),PushX);
		VectorRegister PushOut = VectorMultiplyAdd(AbsExtentZ,VectorAbs(PlanesZ),PushY);
		VectorRegister PushOutNegative = VectorNegate(PushOut);

		// Check for completely outside
		if (VectorAnyGreaterThan(Distance,PushOut))
		{
			Result = FALSE;
			bOutFullyContained = FALSE;
			break;
		}

		// Definitely inside frustums, but check to see if it's fully contained
		if (VectorAnyGreaterThan(Distance,PushOutNegative))
		{
			bOutFullyContained = FALSE;
		}
	}
	return Result;
}

//
//	FConvexVolume::IntersectSphere
//

UBOOL FConvexVolume::IntersectSphere(const FVector& Origin,const FLOAT& Radius) const
{
	UBOOL Result = TRUE;

	checkSlow(PermutedPlanes.Num() % 4 == 0);

	// Load the origin & radius
	VectorRegister Orig = VectorLoadFloat3(&Origin);
	VectorRegister VRadius = VectorLoadFloat1(&Radius);
	// Splat origin into 3 vectors
	VectorRegister OrigX = VectorReplicate(Orig, 0);
	VectorRegister OrigY = VectorReplicate(Orig, 1);
	VectorRegister OrigZ = VectorReplicate(Orig, 2);
	// Since we are moving straight through get a pointer to the data
	const FPlane* RESTRICT PermutedPlanePtr = (FPlane*)PermutedPlanes.GetData();
	// Process four planes at a time until we have < 4 left
	for (INT Count = 0; Count < PermutedPlanes.Num(); Count += 4)
	{
		// Load 4 planes that are already all Xs, Ys, ...
		VectorRegister PlanesX = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesY = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesZ = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesW = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		// Calculate the distance (x * x) + (y * y) + (z * z) - w
		VectorRegister DistX = VectorMultiply(OrigX,PlanesX);
		VectorRegister DistY = VectorMultiplyAdd(OrigY,PlanesY,DistX);
		VectorRegister DistZ = VectorMultiplyAdd(OrigZ,PlanesZ,DistY);
		VectorRegister Distance = VectorSubtract(DistZ,PlanesW);

		// Check for completely outside
		if (VectorAnyGreaterThan(Distance,VRadius))
		{
			Result = FALSE;
			break;
		}
	}
	return Result;
}


//
//	FConvexVolume::IntersectSphere with the addition check of if the sphere is COMPLETELY contained or only partially contained
//

UBOOL FConvexVolume::IntersectSphere(const FVector& Origin,const FLOAT& Radius, UBOOL& bOutFullyContained) const
{
	UBOOL Result = TRUE;

	//Assume fully contained
	bOutFullyContained = TRUE;

	checkSlow(PermutedPlanes.Num() % 4 == 0);

	// Load the origin & radius
	VectorRegister Orig = VectorLoadFloat3(&Origin);
	VectorRegister VRadius = VectorLoadFloat1(&Radius);
	VectorRegister NegativeVRadius = VectorNegate(VRadius);
	// Splat origin into 3 vectors
	VectorRegister OrigX = VectorReplicate(Orig, 0);
	VectorRegister OrigY = VectorReplicate(Orig, 1);
	VectorRegister OrigZ = VectorReplicate(Orig, 2);
	// Since we are moving straight through get a pointer to the data
	const FPlane* RESTRICT PermutedPlanePtr = (FPlane*)PermutedPlanes.GetData();
	// Process four planes at a time until we have < 4 left
	for (INT Count = 0; Count < PermutedPlanes.Num(); Count += 4)
	{
		// Load 4 planes that are already all Xs, Ys, ...
		VectorRegister PlanesX = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesY = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesZ = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesW = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		// Calculate the distance (x * x) + (y * y) + (z * z) - w
		VectorRegister DistX = VectorMultiply(OrigX,PlanesX);
		VectorRegister DistY = VectorMultiplyAdd(OrigY,PlanesY,DistX);
		VectorRegister DistZ = VectorMultiplyAdd(OrigZ,PlanesZ,DistY);
		VectorRegister Distance = VectorSubtract(DistZ,PlanesW);

		// Check for completely outside
		INT Mask = VectorAnyGreaterThan(Distance,VRadius);
		if (Mask)
		{
			Result = FALSE;
			bOutFullyContained = FALSE;
			break;
		}

		//the sphere is definitely inside the frustums, but let's check if it's FULLY contained by checking the NEGATIVE radius (on the inside of each frustum plane)
		Mask = VectorAnyGreaterThan(Distance,NegativeVRadius);
		if (Mask)
		{
			bOutFullyContained = FALSE;
		}
	}
	return Result;
}

void GetViewFrustumBounds(FConvexVolume& OutResult, const FMatrix& ViewProjectionMatrix,UBOOL UseNearPlane)
{
	OutResult.Planes.Empty( 6 );
	FPlane	Temp;

	// Near clipping plane.
	if(UseNearPlane && ViewProjectionMatrix.GetFrustumNearPlane(Temp))
	{
		OutResult.Planes.AddItem(Temp);
	}

	// Left clipping plane.
	if(ViewProjectionMatrix.GetFrustumLeftPlane(Temp))
	{
		OutResult.Planes.AddItem(Temp);
	}

	// Right clipping plane.
	if(ViewProjectionMatrix.GetFrustumRightPlane(Temp))
	{
		OutResult.Planes.AddItem(Temp);
	}

	// Top clipping plane.
	if(ViewProjectionMatrix.GetFrustumTopPlane(Temp))
	{
		OutResult.Planes.AddItem(Temp);
	}

	// Bottom clipping plane.
	if(ViewProjectionMatrix.GetFrustumBottomPlane(Temp))
	{
		OutResult.Planes.AddItem(Temp);
	}

	// Far clipping plane.
	if(ViewProjectionMatrix.GetFrustumFarPlane(Temp))
	{
		OutResult.Planes.AddItem(Temp);
	}

	OutResult.Init();
}

/**
 * Serializor
 *
 * @param	Ar				Archive to serialize data to
 * @param	ConvexVolume	Convex volumes to serialize to archive
 *
 * @return passed in archive
 */
FArchive& operator<<(FArchive& Ar,FConvexVolume& ConvexVolume)
{
	Ar << ConvexVolume.Planes;
	Ar << ConvexVolume.PermutedPlanes;
	return Ar;
}

void DrawFrustumWireframe(
	FPrimitiveDrawInterface* PDI,
	const FMatrix& FrustumToWorld,
	FColor Color,
	BYTE DepthPriority
	)
{
	FVector Vertices[2][2][2];
	for(UINT Z = 0;Z < 2;Z++)
	{
		for(UINT Y = 0;Y < 2;Y++)
		{
			for(UINT X = 0;X < 2;X++)
			{
				FVector4 UnprojectedVertex = FrustumToWorld.TransformFVector4(
					FVector4(
						(X ? -1.0f : 1.0f),
						(Y ? -1.0f : 1.0f),
						(Z ?  0.0f : 1.0f),
						1.0f
						)
					);
				Vertices[X][Y][Z] = FVector(UnprojectedVertex) / UnprojectedVertex.W;
			}
		}
	}

	PDI->DrawLine(Vertices[0][0][0],Vertices[0][0][1],Color,DepthPriority);
	PDI->DrawLine(Vertices[1][0][0],Vertices[1][0][1],Color,DepthPriority);
	PDI->DrawLine(Vertices[0][1][0],Vertices[0][1][1],Color,DepthPriority);
	PDI->DrawLine(Vertices[1][1][0],Vertices[1][1][1],Color,DepthPriority);

	PDI->DrawLine(Vertices[0][0][0],Vertices[0][1][0],Color,DepthPriority);
	PDI->DrawLine(Vertices[1][0][0],Vertices[1][1][0],Color,DepthPriority);
	PDI->DrawLine(Vertices[0][0][1],Vertices[0][1][1],Color,DepthPriority);
	PDI->DrawLine(Vertices[1][0][1],Vertices[1][1][1],Color,DepthPriority);

	PDI->DrawLine(Vertices[0][0][0],Vertices[1][0][0],Color,DepthPriority);
	PDI->DrawLine(Vertices[0][1][0],Vertices[1][1][0],Color,DepthPriority);
	PDI->DrawLine(Vertices[0][0][1],Vertices[1][0][1],Color,DepthPriority);
	PDI->DrawLine(Vertices[0][1][1],Vertices[1][1][1],Color,DepthPriority);
}
