/*=============================================================================
	UnMath.cpp: Unreal math routines, implementation of FGlobalMath class
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	Globals
-----------------------------------------------------------------------------*/

FVector FVector::ZeroVector(0.0f, 0.0f, 0.0f);
FRotator FRotator::ZeroRotator(0,0,0);

const VectorRegister VECTOR_INV_255 = { 1.f/255.f, 1.f/255.f, 1.f/255.f, 1.f/255.f };

/** 32 bit values where GBitFlag[x] == (1<<x) */
DWORD GBitFlag[32] =
{
	(1U << 0),	(1U << 1),	(1U << 2),	(1U << 3),
	(1U << 4),	(1U << 5),	(1U << 6),	(1U << 7),
	(1U << 8),	(1U << 9),	(1U << 10),	(1U << 11),
	(1U << 12),	(1U << 13),	(1U << 14),	(1U << 15),
	(1U << 16),	(1U << 17),	(1U << 18),	(1U << 19),
	(1U << 20),	(1U << 21),	(1U << 22),	(1U << 23),
	(1U << 24),	(1U << 25),	(1U << 26),	(1U << 27),
	(1U << 28),	(1U << 29),	(1U << 30),	(1U << 31),
};

/*-----------------------------------------------------------------------------
	FGlobalMath constructor.
-----------------------------------------------------------------------------*/

// Constructor.
FGlobalMath::FGlobalMath()
{
	// Init base angle table.
	{for( INT i=0; i<NUM_ANGLES; i++ )
		TrigFLOAT[i] = appSin((FLOAT)i * 2.f * PI / (FLOAT)NUM_ANGLES);}
}

/*-----------------------------------------------------------------------------
	Compressed serialization.
-----------------------------------------------------------------------------*/

/**
 * Serializes the vector compressed for e.g. network transmission.
 * 
 * @param	Ar	Archive to serialize to/ from
 */
void FVector::SerializeCompressed( FArchive& Ar )
{
	INT IntX	= appRound(X);
	INT IntY	= appRound(Y);
	INT IntZ	= appRound(Z);
			
	DWORD Bits	= Clamp<DWORD>( appCeilLogTwo( 1 + Max3( Abs(IntX), Abs(IntY), Abs(IntZ) ) ), 1, 20 ) - 1;
	
	Ar.SerializeInt( Bits, 20 );

	INT   Bias	= 1<<(Bits+1);
	DWORD Max	= 1<<(Bits+2);
	DWORD DX	= IntX + Bias;
	DWORD DY	= IntY + Bias;
	DWORD DZ	= IntZ + Bias;
	
	Ar.SerializeInt( DX, Max );
	Ar.SerializeInt( DY, Max );
	Ar.SerializeInt( DZ, Max );
	
	if( Ar.IsLoading() )
	{
		X = (INT)DX-Bias;
		Y = (INT)DY-Bias;
		Z = (INT)DZ-Bias;
	}
}

/**
 * Serializes the rotator compressed for e.g. network transmission.
 * 
 * @param	Ar	Archive to serialize to/ from
 */
void FRotator::SerializeCompressed( FArchive& Ar )
{
	BYTE BytePitch	= Pitch>>8;
	BYTE ByteYaw	= Yaw>>8;
	BYTE ByteRoll	= Roll>>8;

	BYTE B = (BytePitch!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
	{
		Ar << BytePitch;
	}
	else
	{
		BytePitch = 0;
	}
	
	B = (ByteYaw!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
	{
		Ar << ByteYaw;
	}
	else
	{
		ByteYaw = 0;
	}
	
	B = (ByteRoll!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
	{
		Ar << ByteRoll;
	}
	else
	{
		ByteRoll = 0;
	}
	
	if( Ar.IsLoading() )
	{
		Pitch	= BytePitch << 8;
		Yaw		= ByteYaw << 8;
		Roll	= ByteRoll << 8;
	}
}


/*-----------------------------------------------------------------------------
	Conversion functions.
-----------------------------------------------------------------------------*/

// Return the FRotator corresponding to the direction that the vector
// is pointing in.  Sets Yaw and Pitch to the proper numbers, and sets
// roll to zero because the roll can't be determined from a vector.
FRotator FVector::Rotation() const
{
	FRotator R;

	// Find yaw.
	R.Yaw = appRound(appAtan2(Y,X) * (FLOAT)MAXWORD / (2.f*PI));

	// Find pitch.
	R.Pitch = appRound(appAtan2(Z,appSqrt(X*X+Y*Y)) * (FLOAT)MAXWORD / (2.f*PI));

	// Find roll.
	R.Roll = 0;

	return R;
}

// Return the FRotator corresponding to the direction that the vector
// is pointing in.  Sets Yaw and Pitch to the proper numbers, and sets
// roll to zero because the roll can't be determined from a vector.
FRotator FVector4::Rotation() const
{
	FRotator R;

	// Find yaw.
	R.Yaw = appRound(appAtan2(Y,X) * (FLOAT)MAXWORD / (2.f*PI));

	// Find pitch.
	R.Pitch = appRound(appAtan2(Z,appSqrt(X*X+Y*Y)) * (FLOAT)MAXWORD / (2.f*PI));

	// Find roll.
	R.Roll = 0;

	return R;
}

/**
 * Find good arbitrary axis vectors to represent U and V axes of a plane,
 * given just the normal.
 */
void FVector::FindBestAxisVectors( FVector& Axis1, FVector& Axis2 ) const
{
	const FLOAT NX = Abs(X);
	const FLOAT NY = Abs(Y);
	const FLOAT NZ = Abs(Z);

	// Find best basis vectors.
	if( NZ>NX && NZ>NY )	Axis1 = FVector(1,0,0);
	else					Axis1 = FVector(0,0,1);

	Axis1 = (Axis1 - *this * (Axis1 | *this)).SafeNormal();
	Axis2 = Axis1 ^ *this;
}

/** Clamps a vector to not be longer than MaxLength. */
FVector ClampLength( const FVector& V, FLOAT MaxLength)
{
	FVector ResultV = V;
	const FLOAT Length = V.Size();
	if(Length > MaxLength)
	{
		ResultV *= (MaxLength/Length);
	}
	return ResultV;
}

/** Find the point on line segment from LineStart to LineEnd which is closest to Point */
FVector ClosestPointOnLine(const FVector& LineStart, const FVector& LineEnd, const FVector& Point)
{
	// Solve to find alpha along line that is closest point
	// Weisstein, Eric W. "Point-Line Distance--3-Dimensional." From MathWorld--A Wolfram Web Resource. http://mathworld.wolfram.com/Point-LineDistance3-Dimensional.html 
	const FLOAT A = (LineStart-Point) | (LineEnd - LineStart);
	const FLOAT B = (LineEnd - LineStart).SizeSquared();
	const FLOAT T = ::Clamp(-A/B, 0.f, 1.f);

	// Generate closest point
	FVector ClosestPoint = LineStart + (T * (LineEnd - LineStart));

	return ClosestPoint;
}

void CreateOrthonormalBasis(FVector& XAxis,FVector& YAxis,FVector& ZAxis)
{
	// Project the X and Y axes onto the plane perpendicular to the Z axis.
	XAxis -= (XAxis | ZAxis) / (ZAxis | ZAxis) * ZAxis;
	YAxis -= (YAxis | ZAxis) / (ZAxis | ZAxis) * ZAxis;

	// If the X axis was parallel to the Z axis, choose a vector which is orthogonal to the Y and Z axes.
	if(XAxis.SizeSquared() < DELTA*DELTA)
	{
		XAxis = YAxis ^ ZAxis;
	}

	// If the Y axis was parallel to the Z axis, choose a vector which is orthogonal to the X and Z axes.
	if(YAxis.SizeSquared() < DELTA*DELTA)
	{
		YAxis = XAxis ^ ZAxis;
	}

	// Normalize the basis vectors.
	XAxis.Normalize();
	YAxis.Normalize();
	ZAxis.Normalize();
}

/** Utility to ensure angle is between +/- 180 degrees by unwinding. */
static void UnwindDegreeComponent(FLOAT& A)
{
	while(A > 180.f)
	{
		A -= 360.f;
	}

	while(A < -180.f)
	{
		A += 360.f;
	}
}

/** When this vector contains Euler angles (degrees), ensure that angles are between +/-180 */
void FVector::UnwindEuler()
{
	UnwindDegreeComponent(X);
	UnwindDegreeComponent(Y);
	UnwindDegreeComponent(Z);
}

/*-----------------------------------------------------------------------------
	FBoxSphereBounds implementation.
-----------------------------------------------------------------------------*/

FBoxSphereBounds FBoxSphereBounds::TransformBy(const FMatrix& M) const
{
	FBoxSphereBounds	Result;

	Result.Origin = M.TransformFVector(Origin);
	Result.BoxExtent = FVector(0,0,0);
	FLOAT	Signs[2] = { -1.0f, 1.0f };
	for(INT X = 0;X < 2;X++)
	{
		for(INT Y = 0;Y < 2;Y++)
		{
			for(INT Z = 0;Z < 2;Z++)
			{
				FVector	Corner = M.TransformNormal(FVector(Signs[X] * BoxExtent.X,Signs[Y] * BoxExtent.Y,Signs[Z] * BoxExtent.Z));
				Result.BoxExtent.X = Max(Corner.X,Result.BoxExtent.X);
				Result.BoxExtent.Y = Max(Corner.Y,Result.BoxExtent.Y);
				Result.BoxExtent.Z = Max(Corner.Z,Result.BoxExtent.Z);
			}
		}
	}

	const FVector XAxis(M.M[0][0],M.M[0][1],M.M[0][2]);
	const FVector YAxis(M.M[1][0],M.M[1][1],M.M[1][2]);
	const FVector ZAxis(M.M[2][0],M.M[2][1],M.M[2][2]);

	Result.SphereRadius = appSqrt(Max(XAxis|XAxis,Max(YAxis|YAxis,ZAxis|ZAxis))) * SphereRadius;

	return Result;
}

/*-----------------------------------------------------------------------------
	FSphere implementation.
-----------------------------------------------------------------------------*/

//
// Compute a bounding sphere from an array of points.
//
FSphere::FSphere( const FVector* Pts, INT Count )
: Center(0,0,0), W(0)
{
	if( Count )
	{
		const FBox Box( Pts, Count );
		*this = FSphere( (Box.Min+Box.Max)/2, 0 );
		for( INT i=0; i<Count; i++ )
		{
			const FLOAT Dist = FDistSquared(Pts[i],Center);
			if( Dist > W )
			{
				W = Dist;
			}
		}
		W = appSqrt(W) * 1.001f;
	}
}

/*-----------------------------------------------------------------------------
	FBox implementation.
-----------------------------------------------------------------------------*/

FBox::FBox( const FVector* Points, INT Count )
: Min(0,0,0), Max(0,0,0), IsValid(0)
{
	for( INT i=0; i<Count; i++ )
	{
		*this += Points[i];
	}
}


FBox::FBox( const TArray<FVector>& Points )
: Min(0,0,0), Max(0,0,0), IsValid(0)
{
	for( INT i=0; i < Points.Num(); ++i )
	{
		*this += Points(i);
	}
}


//
//	FBox::TransformBy
//
FBox FBox::TransformBy(const FMatrix& M) const
{
	// If we are not valid, return another invalid box.
	if(!IsValid)
	{
		return FBox(0);
	}

	VectorRegister Vertices[8];
	VectorRegister m0 = VectorLoadAligned( M.M[0] );
	VectorRegister m1 = VectorLoadAligned( M.M[1] );
	VectorRegister m2 = VectorLoadAligned( M.M[2] );
	VectorRegister m3 = VectorLoadAligned( M.M[3] );
	Vertices[0]   = VectorLoadFloat3( &Min );
	Vertices[1]   = VectorSetFloat3( Min.X, Min.Y, Max.Z );
	Vertices[2]   = VectorSetFloat3( Min.X, Max.Y, Min.Z );
	Vertices[3]   = VectorSetFloat3( Max.X, Min.Y, Min.Z );
	Vertices[4]   = VectorSetFloat3( Max.X, Max.Y, Min.Z );
	Vertices[5]   = VectorSetFloat3( Max.X, Min.Y, Max.Z );
	Vertices[6]   = VectorSetFloat3( Min.X, Max.Y, Max.Z );
	Vertices[7]   = VectorLoadFloat3( &Max );
	VectorRegister r0 = VectorMultiply( VectorReplicate(Vertices[0],0), m0 );
	VectorRegister r1 = VectorMultiply( VectorReplicate(Vertices[1],0), m0 );
	VectorRegister r2 = VectorMultiply( VectorReplicate(Vertices[2],0), m0 );
	VectorRegister r3 = VectorMultiply( VectorReplicate(Vertices[3],0), m0 );
	VectorRegister r4 = VectorMultiply( VectorReplicate(Vertices[4],0), m0 );
	VectorRegister r5 = VectorMultiply( VectorReplicate(Vertices[5],0), m0 );
	VectorRegister r6 = VectorMultiply( VectorReplicate(Vertices[6],0), m0 );
	VectorRegister r7 = VectorMultiply( VectorReplicate(Vertices[7],0), m0 );

	r0 = VectorMultiplyAdd( VectorReplicate(Vertices[0],1), m1, r0 );
	r1 = VectorMultiplyAdd( VectorReplicate(Vertices[1],1), m1, r1 );
	r2 = VectorMultiplyAdd( VectorReplicate(Vertices[2],1), m1, r2 );
	r3 = VectorMultiplyAdd( VectorReplicate(Vertices[3],1), m1, r3 );
	r4 = VectorMultiplyAdd( VectorReplicate(Vertices[4],1), m1, r4 );
	r5 = VectorMultiplyAdd( VectorReplicate(Vertices[5],1), m1, r5 );
	r6 = VectorMultiplyAdd( VectorReplicate(Vertices[6],1), m1, r6 );
	r7 = VectorMultiplyAdd( VectorReplicate(Vertices[7],1), m1, r7 );

	r0 = VectorMultiplyAdd( VectorReplicate(Vertices[0],2), m2, r0 );
	r1 = VectorMultiplyAdd( VectorReplicate(Vertices[1],2), m2, r1 );
	r2 = VectorMultiplyAdd( VectorReplicate(Vertices[2],2), m2, r2 );
	r3 = VectorMultiplyAdd( VectorReplicate(Vertices[3],2), m2, r3 );
	r4 = VectorMultiplyAdd( VectorReplicate(Vertices[4],2), m2, r4 );
	r5 = VectorMultiplyAdd( VectorReplicate(Vertices[5],2), m2, r5 );
	r6 = VectorMultiplyAdd( VectorReplicate(Vertices[6],2), m2, r6 );
	r7 = VectorMultiplyAdd( VectorReplicate(Vertices[7],2), m2, r7 );

	r0 = VectorAdd( r0, m3 );
	r1 = VectorAdd( r1, m3 );
	r2 = VectorAdd( r2, m3 );
	r3 = VectorAdd( r3, m3 );
	r4 = VectorAdd( r4, m3 );
	r5 = VectorAdd( r5, m3 );
	r6 = VectorAdd( r6, m3 );
	r7 = VectorAdd( r7, m3 );

	FBox NewBox;
	VectorRegister min0 = VectorMin( r0, r1 );
	VectorRegister min1 = VectorMin( r2, r3 );
	VectorRegister min2 = VectorMin( r4, r5 );
	VectorRegister min3 = VectorMin( r6, r7 );
	VectorRegister max0 = VectorMax( r0, r1 );
	VectorRegister max1 = VectorMax( r2, r3 );
	VectorRegister max2 = VectorMax( r4, r5 );
	VectorRegister max3 = VectorMax( r6, r7 );
	min0 = VectorMin( min0, min1 );
	min1 = VectorMin( min2, min3 );
	max0 = VectorMax( max0, max1 );
	max1 = VectorMax( max2, max3 );
	min0 = VectorMin( min0, min1 );
	max0 = VectorMax( max0, max1 );
	VectorStoreFloat3( min0, &NewBox.Min );
	VectorStoreFloat3( max0, &NewBox.Max );
	NewBox.IsValid = 1;

	return NewBox;
}

FBox FBox::TransformBy( const FBoneAtom & M ) const
{
	FBox NewBox;
	NewBox.Min = M.TransformFVector(Min);
	NewBox.Max = M.TransformFVector(Max);
	NewBox.IsValid = 1;
	return NewBox;
}

/** 
* Transforms and projects a world bounding box to screen space
*
* @param	ProjM - projection matrix
* @return	transformed box
*/
FBox FBox::TransformProjectBy( const FMatrix& ProjM ) const
{
	FVector Vertices[8] = 
	{
		FVector(Min),
		FVector(Min.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Min.Z),
		FVector(Max.X, Min.Y, Min.Z),
		FVector(Max.X, Max.Y, Min.Z),
		FVector(Max.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Max.Z),
		FVector(Max)
	};

	FBox NewBox(0);
	for(INT VertexIndex = 0;VertexIndex < ARRAY_COUNT(Vertices);VertexIndex++)
	{
		FVector4 ProjectedVertex = ProjM.TransformFVector(Vertices[VertexIndex]);
		NewBox += ((FVector)ProjectedVertex) / ProjectedVertex.W;
	}

	return NewBox;
}

/*-----------------------------------------------------------------------------
	FRotator functions.
-----------------------------------------------------------------------------*/

FRotator::FRotator(const FQuat& Quat)
{
	*this = FQuatRotationTranslationMatrix( Quat,  FVector(0.f) ).Rotator();
}

//
// Convert a rotation into a vector facing in its direction.
//
FVector FRotator::Vector() const
{
	return FRotationMatrix( *this ).GetAxis(0);
}

//
// Convert a rotation into a quaternion.
//

FQuat FRotator::Quaternion() const
{
	return FQuat( FRotationMatrix( *this ) );
}

/** Convert a Rotator into floating-point Euler angles (in degrees). */
FVector FRotator::Euler() const
{
	return FVector( Roll * (180.f / 32768.f), Pitch * (180.f / 32768.f), Yaw * (180.f / 32768.f) );
}

/** Convert a vector of floating-point Euler angles (in degrees) into a Rotator. */
FRotator FRotator::MakeFromEuler(const FVector& Euler)
{
	return FRotator( appTrunc(Euler.Y * (32768.f / 180.f)), appTrunc(Euler.Z * (32768.f / 180.f)), appTrunc(Euler.X * (32768.f / 180.f)) );
}



/** 
 *	Decompose this Rotator into a Winding part (multiples of 65536) and a Remainder part. 
 *	Remainder will always be in [-32768, 32767] range.
 */
void FRotator::GetWindingAndRemainder(FRotator& Winding, FRotator& Remainder) const
{
	//// YAW
	Remainder.Yaw = Yaw & 65535;
	if(Remainder.Yaw >= 32768)
	{
		Remainder.Yaw -= 65536;
	}

	Winding.Yaw = Yaw - Remainder.Yaw;

	//// PITCH
	Remainder.Pitch = Pitch & 65535;
	if(Remainder.Pitch >= 32768)
	{
		Remainder.Pitch -= 65536;
	}

	Winding.Pitch = Pitch - Remainder.Pitch;

	//// ROLL
	Remainder.Roll = Roll & 65535;
	if(Remainder.Roll >= 32768)
	{
		Remainder.Roll -= 65536;
	}

	Winding.Roll = Roll - Remainder.Roll;
}

/** 
 *	Alter this Rotator to form the 'shortest' rotation that will have the effect. 
 *	First clips the rotation between [0,65535], then checks direction for each component.
 *  Returns values [-32768, 32767]
 */
void FRotator::MakeShortestRoute()
{
	//// YAW

	// Clip rotation to [0,65535] range
	Yaw = Yaw & 65535;

	// Then ensure it takes the 'shortest' route.
	if(Yaw >= 32768)
		Yaw -= 65536;

	//// PITCH
	Pitch = Pitch & 65535;

	if(Pitch >= 32768)
		Pitch -= 65536;

	//// ROLL
	Roll = Roll & 65535;

	if(Roll >= 32768)
		Roll -= 65536;
}

/*-----------------------------------------------------------------------------
	FQuaternion and FMatrix support functions
-----------------------------------------------------------------------------*/

FRotator FMatrix::Rotator() const
{
	const FVector		XAxis	= GetAxis( 0 );
	const FVector		YAxis	= GetAxis( 1 );
	const FVector		ZAxis	= GetAxis( 2 );

	FRotator	Rotator	= FRotator( 
									appRound(appAtan2( XAxis.Z, appSqrt(Square(XAxis.X)+Square(XAxis.Y)) ) * 32768.f / PI), 
									appRound(appAtan2( XAxis.Y, XAxis.X ) * 32768.f / PI), 
									0 
								);
	
	const FVector		SYAxis	= FRotationMatrix( Rotator ).GetAxis(1);
	Rotator.Roll		= appRound(appAtan2( ZAxis | SYAxis, YAxis | SYAxis ) * 32768.f / PI);
	return Rotator;
}

/** 
 * Transform a rotation matrix into a quaternion.
 * Warning, rotation part will need to be unit length for this to be right!
 */
FQuat FMatrix::ToQuat() const
{
	FQuat Result(*this);
	return Result;
}

const FMatrix FMatrix::Identity(FPlane(1,0,0,0),FPlane(0,1,0,0),FPlane(0,0,1,0),FPlane(0,0,0,1));

const FQuat FQuat::Identity(0,0,0,1);

FString FMatrix::ToString() const
{
	FString Output;

	Output += FString::Printf(TEXT("[%f %f %f %f] "), M[0][0], M[0][1], M[0][2], M[0][3]);
	Output += FString::Printf(TEXT("[%f %f %f %f] "), M[1][0], M[1][1], M[1][2], M[1][3]);
	Output += FString::Printf(TEXT("[%f %f %f %f] "), M[2][0], M[2][1], M[2][2], M[2][3]);
	Output += FString::Printf(TEXT("[%f %f %f %f] "), M[3][0], M[3][1], M[3][2], M[3][3]);

	return Output;
}

void FMatrix::DebugPrint() const
{
	debugf(TEXT("%s"), *ToString());
}

/** Convert a vector of floating-point Euler angles (in degrees) into a Quaternion. */
FQuat FQuat::MakeFromEuler(const FVector& Euler)
{
	return FQuat( FRotationTranslationMatrix( FRotator::MakeFromEuler(Euler), FVector(0.f) ) );
}

/** Convert a Quaternion into floating-point Euler angles (in degrees). */
FVector FQuat::Euler() const
{
	return FQuatRotationTranslationMatrix( *this, FVector(0.f) ).Rotator().Euler();
}

FQuat FQuatFindBetween(const FVector& vec1, const FVector& vec2)
{
	const FVector cross = vec1 ^ vec2;
	const FLOAT crossMag = cross.Size();

	// See if vectors are parallel or anti-parallel
	if(crossMag < KINDA_SMALL_NUMBER)
	{
		// If these vectors are parallel - just return identity quaternion (ie no rotation).
		const FLOAT Dot = vec1 | vec2;
		if(Dot > -KINDA_SMALL_NUMBER)
		{
			return FQuat::Identity; // no rotation
		}
		// Exactly opposite..
		else
		{
			// ..rotation by 180 degrees around a vector orthogonal to vec1 & vec2
			FVector Vec = vec1.SizeSquared() > vec2.SizeSquared() ? vec1 : vec2;
			Vec.Normalize();

			FVector AxisA, AxisB;
			Vec.FindBestAxisVectors(AxisA, AxisB);

			return FQuat(AxisA.X, AxisA.Y, AxisA.Z, 0.f); // (axis*sin(pi/2), cos(pi/2)) = (axis, 0)
		}
	}

	// Not parallel, so use normal code
	FLOAT angle = appAsin(crossMag);

	const FLOAT dot = vec1 | vec2;
	if(dot < 0.0f)
	{
		angle = PI - angle;
	}

	const FLOAT sinHalfAng = appSin(0.5f * angle);
	const FLOAT cosHalfAng = appCos(0.5f * angle);
	const FVector axis = cross / crossMag;

	return FQuat(
		sinHalfAng * axis.X,
		sinHalfAng * axis.Y,
		sinHalfAng * axis.Z,
		cosHalfAng );
}

// Returns quaternion with W=0 and V=theta*v.

FQuat FQuat::Log() const
{
	FQuat Result;
	Result.W = 0.f;

	if ( Abs(W) < 1.f )
	{
		const FLOAT Angle = appAcos(W);
		const FLOAT SinAngle = appSin(Angle);

		if ( Abs(SinAngle) >= SMALL_NUMBER )
		{
			const FLOAT Scale = Angle/SinAngle;
			Result.X = Scale*X;
			Result.Y = Scale*Y;
			Result.Z = Scale*Z;

			return Result;
		}
	}

	Result.X = X;
	Result.Y = Y;
	Result.Z = Z;

	return Result;
}

// Assumes a quaternion with W=0 and V=theta*v (where |v| = 1).
// Exp(q) = (sin(theta)*v, cos(theta))

FQuat FQuat::Exp() const
{
	const FLOAT Angle = appSqrt(X*X + Y*Y + Z*Z);
	const FLOAT SinAngle = appSin(Angle);

	FQuat Result;
	Result.W = appCos(Angle);

	if ( Abs(SinAngle) >= SMALL_NUMBER )
	{
		const FLOAT Scale = SinAngle/Angle;
		Result.X = Scale*X;
		Result.Y = Scale*Y;
		Result.Z = Scale*Z;
	}
	else
	{
		Result.X = X;
		Result.Y = Y;
		Result.Z = Z;
	}

	return Result;
}

/*-----------------------------------------------------------------------------
	Swept-Box vs Box test.
-----------------------------------------------------------------------------*/

/* Line-extent/Box Test Util */
UBOOL FLineExtentBoxIntersection(const FBox& inBox, 
								 const FVector& Start, 
								 const FVector& End,
								 const FVector& Extent,
								 FVector& HitLocation,
								 FVector& HitNormal,
								 FLOAT& HitTime)
{
	FBox box = inBox;
	box.Max.X += Extent.X;
	box.Max.Y += Extent.Y;
	box.Max.Z += Extent.Z;
	
	box.Min.X -= Extent.X;
	box.Min.Y -= Extent.Y;
	box.Min.Z -= Extent.Z;

	const FVector Dir = (End - Start);
	
	FVector	Time;
	UBOOL	Inside = 1;
	FLOAT   faceDir[3] = {1, 1, 1};
	
	/////////////// X
	if(Start.X < box.Min.X)
	{
		if(Dir.X <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			faceDir[0] = -1;
			Time.X = (box.Min.X - Start.X) / Dir.X;
		}
	}
	else if(Start.X > box.Max.X)
	{
		if(Dir.X >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.X = (box.Max.X - Start.X) / Dir.X;
		}
	}
	else
		Time.X = 0.0f;
	
	/////////////// Y
	if(Start.Y < box.Min.Y)
	{
		if(Dir.Y <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			faceDir[1] = -1;
			Time.Y = (box.Min.Y - Start.Y) / Dir.Y;
		}
	}
	else if(Start.Y > box.Max.Y)
	{
		if(Dir.Y >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Y = (box.Max.Y - Start.Y) / Dir.Y;
		}
	}
	else
		Time.Y = 0.0f;
	
	/////////////// Z
	if(Start.Z < box.Min.Z)
	{
		if(Dir.Z <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			faceDir[2] = -1;
			Time.Z = (box.Min.Z - Start.Z) / Dir.Z;
		}
	}
	else if(Start.Z > box.Max.Z)
	{
		if(Dir.Z >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Z = (box.Max.Z - Start.Z) / Dir.Z;
		}
	}
	else
		Time.Z = 0.0f;
	
	// If the line started inside the box (ie. player started in contact with the fluid)
	if(Inside)
	{
		HitLocation = Start;
		HitNormal = FVector(0, 0, 1);
		HitTime = 0;
		return 1;
	}
	// Otherwise, calculate when hit occured
	else
	{	
		if(Time.Y > Time.Z)
		{
			HitTime = Time.Y;
			HitNormal = FVector(0, faceDir[1], 0);
		}
		else
		{
			HitTime = Time.Z;
			HitNormal = FVector(0, 0, faceDir[2]);
		}
		
		if(Time.X > HitTime)
		{
			HitTime = Time.X;
			HitNormal = FVector(faceDir[0], 0, 0);
		}
		
		if(HitTime >= 0.0f && HitTime <= 1.0f)
		{
			HitLocation = Start + Dir * HitTime;
			const FLOAT BOX_SIDE_THRESHOLD = 0.1f;
			if(	HitLocation.X > box.Min.X - BOX_SIDE_THRESHOLD && HitLocation.X < box.Max.X + BOX_SIDE_THRESHOLD &&
				HitLocation.Y > box.Min.Y - BOX_SIDE_THRESHOLD && HitLocation.Y < box.Max.Y + BOX_SIDE_THRESHOLD &&
				HitLocation.Z > box.Min.Z - BOX_SIDE_THRESHOLD && HitLocation.Z < box.Max.Z + BOX_SIDE_THRESHOLD)
			{				
				return 1;
			}
		}
		
		return 0;
	}
}

/*-----------------------------------------------------------------------------
	Bezier curves
-----------------------------------------------------------------------------*/

/**
 * Generates a list of sample points on a Bezier curve defined by 2 points.
 *
 * @param	ControlPoints	Array of 4 FVectors (vert1, controlpoint1, controlpoint2, vert2).
 * @param	NumPoints		Number of samples.
 * @param	OutPoints		Receives the output samples.
 * @return					Path length.
 */
FLOAT EvaluateBezier(const FVector* ControlPoints, INT NumPoints, TArray<FVector>& OutPoints)
{
	check( ControlPoints );
	check( NumPoints >= 2 );

	// var q is the change in t between successive evaluations.
	const FLOAT q = 1.f/(NumPoints-1); // q is dependent on the number of GAPS = POINTS-1

	// recreate the names used in the derivation
	const FVector& P0 = ControlPoints[0];
	const FVector& P1 = ControlPoints[1];
	const FVector& P2 = ControlPoints[2];
	const FVector& P3 = ControlPoints[3];

	// coefficients of the cubic polynomial that we're FDing -
	const FVector a = P0;
	const FVector b = 3*(P1-P0);
	const FVector c = 3*(P2-2*P1+P0);
	const FVector d = P3-3*P2+3*P1-P0;

	// initial values of the poly and the 3 diffs -
	FVector S  = a;						// the poly value
	FVector U  = b*q + c*q*q + d*q*q*q;	// 1st order diff (quadratic)
	FVector V  = 2*c*q*q + 6*d*q*q*q;	// 2nd order diff (linear)
	FVector W  = 6*d*q*q*q;				// 3rd order diff (constant)

	// Path length.
	FLOAT Length = 0.f;

	FVector OldPos = P0;
	OutPoints.AddItem( P0 );	// first point on the curve is always P0.

	for( INT i = 1 ; i < NumPoints ; ++i )
	{
		// calculate the next value and update the deltas
		S += U;			// update poly value
		U += V;			// update 1st order diff value
		V += W;			// update 2st order diff value
		// 3rd order diff is constant => no update needed.

		// Update Length.
		Length += FDist( S, OldPos );
		OldPos  = S;

		OutPoints.AddItem( S );
	}

	// Return path length as experienced in sequence (linear interpolation between points).
	return Length;
}

FLOAT EvaluateBezier(const FLinearColor* ControlPoints, INT NumPoints, TArray<FLinearColor>& OutPoints)
{
	check( ControlPoints );
	check( NumPoints >= 2 );

	// var q is the change in t between successive evaluations.
	const FLOAT q = 1.f/(NumPoints-1); // q is dependent on the number of GAPS = POINTS-1

	// recreate the names used in the derivation
	const FLinearColor& P0 = ControlPoints[0];
	const FLinearColor& P1 = ControlPoints[1];
	const FLinearColor& P2 = ControlPoints[2];
	const FLinearColor& P3 = ControlPoints[3];

	// coefficients of the cubic polynomial that we're FDing -
	const FLinearColor a = P0;
	const FLinearColor b = 3*(P1-P0);
	const FLinearColor c = 3*(P2-2*P1+P0);
	const FLinearColor d = P3-3*P2+3*P1-P0;

	// initial values of the poly and the 3 diffs -
	FLinearColor S  = a;						// the poly value
	FLinearColor U  = b*q + c*q*q + d*q*q*q;	// 1st order diff (quadratic)
	FLinearColor V  = 2*c*q*q + 6*d*q*q*q;	// 2nd order diff (linear)
	FLinearColor W  = 6*d*q*q*q;				// 3rd order diff (constant)

	// Path length.
	FLOAT Length = 0.f;

	FLinearColor OldPos = P0;
	OutPoints.AddItem( P0 );	// first point on the curve is always P0.

	for( INT i = 1 ; i < NumPoints ; ++i )
	{
		// calculate the next value and update the deltas
		S += U;			// update poly value
		U += V;			// update 1st order diff value
		V += W;			// update 2st order diff value
		// 3rd order diff is constant => no update needed.

		// Update Length.
		Length += FDist( S, OldPos );
		OldPos  = S;

		OutPoints.AddItem( S );
	}

	// Return path length as experienced in sequence (linear interpolation between points).
	return Length;
}


/*-----------------------------------------------------------------------------
	Cubic Quaternion interpolation support functions
-----------------------------------------------------------------------------*/

/**
 * Spherical interpolation. Will correct alignment. Output is not normalized.
 */
FQuat SlerpQuat(const FQuat& Quat1,const FQuat& Quat2, float Slerp)
{
	// Get cosine of angle between quats.
	const FLOAT RawCosom = 
		    Quat1.X * Quat2.X +
			Quat1.Y * Quat2.Y +
			Quat1.Z * Quat2.Z +
			Quat1.W * Quat2.W;
	// Unaligned quats - compensate, results in taking shorter route.
	const FLOAT Cosom = appFloatSelect( RawCosom, RawCosom, -RawCosom );
	
	FLOAT Scale0, Scale1;

	if( Cosom < 0.9999f )
	{	
		const FLOAT Omega = appAcos(Cosom);
		const FLOAT InvSin = 1.f/appSin(Omega);
		Scale0 = appSin( (1.f - Slerp) * Omega ) * InvSin;
		Scale1 = appSin( Slerp * Omega ) * InvSin;
	}
	else
	{
		// Use linear interpolation.
		Scale0 = 1.0f - Slerp;
		Scale1 = Slerp;	
	}

	// In keeping with our flipped Cosom:
	Scale1 = appFloatSelect( RawCosom, Scale1, -Scale1 );

	FQuat Result;
		
	Result.X = Scale0 * Quat1.X + Scale1 * Quat2.X;
	Result.Y = Scale0 * Quat1.Y + Scale1 * Quat2.Y;
	Result.Z = Scale0 * Quat1.Z + Scale1 * Quat2.Z;
	Result.W = Scale0 * Quat1.W + Scale1 * Quat2.W;

	return Result;
}

// Simpler Slerp that doesn't do any checks for 'shortest distance' etc.
// We need this for the cubic interpolation stuff so that the multiple Slerps dont go in different directions.
FQuat SlerpQuatFullPath(const FQuat &quat1, const FQuat &quat2, FLOAT Alpha )
{
	const FLOAT CosAngle = Clamp(quat1 | quat2, -1.f, 1.f);
	const FLOAT Angle = appAcos(CosAngle);

	//debugf( TEXT("CosAngle: %f Angle: %f"), CosAngle, Angle );

	if ( Abs(Angle) < KINDA_SMALL_NUMBER )
	{
		return quat1;
	}

	const FLOAT SinAngle = appSin(Angle);
	const FLOAT InvSinAngle = 1.f/SinAngle;

	const FLOAT Scale0 = appSin((1.0f-Alpha)*Angle)*InvSinAngle;
	const FLOAT Scale1 = appSin(Alpha*Angle)*InvSinAngle;

	return quat1*Scale0 + quat2*Scale1;
}

// Given start and end quaternions of quat1 and quat2, and tangents at those points tang1 and tang2, calculate the point at Alpha (between 0 and 1) between them.
FQuat SquadQuat(const FQuat& quat1, const FQuat& tang1, const FQuat& quat2, const FQuat& tang2, FLOAT Alpha)
{
	const FQuat Q1 = SlerpQuatFullPath(quat1, quat2, Alpha);
	//debugf(TEXT("Q1: %f %f %f %f"), Q1.X, Q1.Y, Q1.Z, Q1.W);

	const FQuat Q2 = SlerpQuatFullPath(tang1, tang2, Alpha);
	//debugf(TEXT("Q2: %f %f %f %f"), Q2.X, Q2.Y, Q2.Z, Q2.W);

	const FQuat Result = SlerpQuatFullPath(Q1, Q2, 2.f * Alpha * (1.f - Alpha));
	//FQuat Result = SlerpQuat(Q1, Q2, 2.f * Alpha * (1.f - Alpha));
	//debugf(TEXT("Result: %f %f %f %f"), Result.X, Result.Y, Result.Z, Result.W);

	return Result;
}


void LegacyCalcQuatTangents( const FQuat& PrevP, const FQuat& P, const FQuat& NextP, FLOAT Tension, FQuat& OutTan )
{
	const FQuat InvP = -P;
	const FQuat Part1 = (InvP * PrevP).Log();
	const FQuat Part2 = (InvP * NextP).Log();

	const FQuat PreExp = (Part1 + Part2) * -0.25f;

	OutTan = P * PreExp.Exp();
}


void CalcQuatTangents( const FQuat& PrevP, const FQuat& P, const FQuat& NextP, FLOAT Tension, FQuat& OutTan )
{
	const FQuat InvP = -P;
	const FQuat Part1 = (InvP * PrevP).Log();
	const FQuat Part2 = (InvP * NextP).Log();

	const FQuat PreExp = (Part1 + Part2) * -0.5f;

	OutTan = P * PreExp.Exp();
}

static void FindBounds( FLOAT& OutMin, FLOAT& OutMax,  FLOAT Start, FLOAT StartLeaveTan, FLOAT StartT, FLOAT End, FLOAT EndArriveTan, FLOAT EndT, UBOOL bCurve )
{
	OutMin = Min( Start, End );
	OutMax = Max( Start, End );

	// Do we need to consider extermeties of a curve?
	if(bCurve)
	{
		// Scale tangents based on time interval, so this code matches the behaviour in FInterpCurve::Eval
		FLOAT Diff = EndT - StartT;
		StartLeaveTan *= Diff;
		EndArriveTan *= Diff;

		const FLOAT a = 6.f*Start + 3.f*StartLeaveTan + 3.f*EndArriveTan - 6.f*End;
		const FLOAT b = -6.f*Start - 4.f*StartLeaveTan - 2.f*EndArriveTan + 6.f*End;
		const FLOAT c = StartLeaveTan;

		const FLOAT Discriminant = (b*b) - (4.f*a*c);
		if(Discriminant > 0.f)
		{
			const FLOAT SqrtDisc = appSqrt( Discriminant );

			const FLOAT x0 = (-b + SqrtDisc)/(2.f*a); // x0 is the 'Alpha' ie between 0 and 1
			const FLOAT t0 = StartT + x0*(EndT - StartT); // Then t0 is the actual 'time' on the curve
			if(t0 > StartT && t0 < EndT)
			{
				const FLOAT Val = CubicInterp( Start, StartLeaveTan, End, EndArriveTan, x0 );

				OutMin = ::Min( OutMin, Val );
				OutMax = ::Max( OutMax, Val );
			}

			const FLOAT x1 = (-b - SqrtDisc)/(2.f*a);
			const FLOAT t1 = StartT + x1*(EndT - StartT);
			if(t1 > StartT && t1 < EndT)
			{
				const FLOAT Val = CubicInterp( Start, StartLeaveTan, End, EndArriveTan, x1 );

				OutMin = ::Min( OutMin, Val );
				OutMax = ::Max( OutMax, Val );
			}
		}
	}
}

void CurveFloatFindIntervalBounds( const FInterpCurvePoint<FLOAT>& Start, const FInterpCurvePoint<FLOAT>& End, FLOAT& CurrentMin, FLOAT& CurrentMax )
{
	const UBOOL bIsCurve = Start.IsCurveKey();

	FLOAT OutMin, OutMax;

	FindBounds(OutMin, OutMax, Start.OutVal, Start.LeaveTangent, Start.InVal, End.OutVal, End.ArriveTangent, End.InVal, bIsCurve);

	CurrentMin = ::Min( CurrentMin, OutMin );
	CurrentMax = ::Max( CurrentMax, OutMax );
}

void CurveVector2DFindIntervalBounds( const FInterpCurvePoint<FVector2D>& Start, const FInterpCurvePoint<FVector2D>& End, FVector2D& CurrentMin, FVector2D& CurrentMax )
{
	const UBOOL bIsCurve = Start.IsCurveKey();

	FLOAT OutMin, OutMax;

	FindBounds(OutMin, OutMax, Start.OutVal.X, Start.LeaveTangent.X, Start.InVal, End.OutVal.X, End.ArriveTangent.X, End.InVal, bIsCurve);
	CurrentMin.X = ::Min( CurrentMin.X, OutMin );
	CurrentMax.X = ::Max( CurrentMax.X, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.Y, Start.LeaveTangent.Y, Start.InVal, End.OutVal.Y, End.ArriveTangent.Y, End.InVal, bIsCurve);
	CurrentMin.Y = ::Min( CurrentMin.Y, OutMin );
	CurrentMax.Y = ::Max( CurrentMax.Y, OutMax );
}

void CurveVectorFindIntervalBounds( const FInterpCurvePoint<FVector>& Start, const FInterpCurvePoint<FVector>& End, FVector& CurrentMin, FVector& CurrentMax )
{
	const UBOOL bIsCurve = Start.IsCurveKey();

	FLOAT OutMin, OutMax;

	FindBounds(OutMin, OutMax, Start.OutVal.X, Start.LeaveTangent.X, Start.InVal, End.OutVal.X, End.ArriveTangent.X, End.InVal, bIsCurve);
	CurrentMin.X = ::Min( CurrentMin.X, OutMin );
	CurrentMax.X = ::Max( CurrentMax.X, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.Y, Start.LeaveTangent.Y, Start.InVal, End.OutVal.Y, End.ArriveTangent.Y, End.InVal, bIsCurve);
	CurrentMin.Y = ::Min( CurrentMin.Y, OutMin );
	CurrentMax.Y = ::Max( CurrentMax.Y, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.Z, Start.LeaveTangent.Z, Start.InVal, End.OutVal.Z, End.ArriveTangent.Z, End.InVal, bIsCurve);
	CurrentMin.Z = ::Min( CurrentMin.Z, OutMin );
	CurrentMax.Z = ::Max( CurrentMax.Z, OutMax );
}

void CurveTwoVectorsFindIntervalBounds(const FInterpCurvePoint<FTwoVectors>& Start, const FInterpCurvePoint<FTwoVectors>& End, FTwoVectors& CurrentMin, FTwoVectors& CurrentMax)
{
	const UBOOL bIsCurve = Start.IsCurveKey();

	FLOAT OutMin;
	FLOAT OutMax;

	// Do the first curve
	FindBounds(OutMin, OutMax, Start.OutVal.v1.X, Start.LeaveTangent.v1.X, Start.InVal, End.OutVal.v1.X, End.ArriveTangent.v1.X, End.InVal, bIsCurve);
	CurrentMin.v1.X = ::Min( CurrentMin.v1.X, OutMin );
	CurrentMax.v1.X = ::Max( CurrentMax.v1.X, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.v1.Y, Start.LeaveTangent.v1.Y, Start.InVal, End.OutVal.v1.Y, End.ArriveTangent.v1.Y, End.InVal, bIsCurve);
	CurrentMin.v1.Y = ::Min( CurrentMin.v1.Y, OutMin );
	CurrentMax.v1.Y = ::Max( CurrentMax.v1.Y, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.v1.Z, Start.LeaveTangent.v1.Z, Start.InVal, End.OutVal.v1.Z, End.ArriveTangent.v1.Z, End.InVal, bIsCurve);
	CurrentMin.v1.Z = ::Min( CurrentMin.v1.Z, OutMin );
	CurrentMax.v1.Z = ::Max( CurrentMax.v1.Z, OutMax );

	// Do the second curve
	FindBounds(OutMin, OutMax, Start.OutVal.v2.X, Start.LeaveTangent.v2.X, Start.InVal, End.OutVal.v2.X, End.ArriveTangent.v2.X, End.InVal, bIsCurve);
	CurrentMin.v2.X = ::Min( CurrentMin.v2.X, OutMin );
	CurrentMax.v2.X = ::Max( CurrentMax.v2.X, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.v2.Y, Start.LeaveTangent.v2.Y, Start.InVal, End.OutVal.v2.Y, End.ArriveTangent.v2.Y, End.InVal, bIsCurve);
	CurrentMin.v2.Y = ::Min( CurrentMin.v2.Y, OutMin );
	CurrentMax.v2.Y = ::Max( CurrentMax.v2.Y, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.v2.Z, Start.LeaveTangent.v2.Z, Start.InVal, End.OutVal.v2.Z, End.ArriveTangent.v2.Z, End.InVal, bIsCurve);
	CurrentMin.v2.Z = ::Min( CurrentMin.v2.Z, OutMin );
	CurrentMax.v2.Z = ::Max( CurrentMax.v2.Z, OutMax );
}


void CurveLinearColorFindIntervalBounds( const FInterpCurvePoint<FLinearColor>& Start, const FInterpCurvePoint<FLinearColor>& End, FLinearColor& CurrentMin, FLinearColor& CurrentMax )
{
	const UBOOL bIsCurve = Start.IsCurveKey();

	FLOAT OutMin, OutMax;

	FindBounds(OutMin, OutMax, Start.OutVal.R, Start.LeaveTangent.R, Start.InVal, End.OutVal.R, End.ArriveTangent.R, End.InVal, bIsCurve);
	CurrentMin.R = ::Min( CurrentMin.R, OutMin );
	CurrentMax.R = ::Max( CurrentMax.R, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.G, Start.LeaveTangent.G, Start.InVal, End.OutVal.G, End.ArriveTangent.G, End.InVal, bIsCurve);
	CurrentMin.G = ::Min( CurrentMin.G, OutMin );
	CurrentMax.G = ::Max( CurrentMax.G, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.B, Start.LeaveTangent.B, Start.InVal, End.OutVal.B, End.ArriveTangent.B, End.InVal, bIsCurve);
	CurrentMin.B = ::Min( CurrentMin.B, OutMin );
	CurrentMax.B = ::Max( CurrentMax.B, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.A, Start.LeaveTangent.A, Start.InVal, End.OutVal.A, End.ArriveTangent.A, End.InVal, bIsCurve);
	CurrentMin.A = ::Min( CurrentMin.A, OutMin );
	CurrentMax.A = ::Max( CurrentMax.A, OutMax );
}




/**
 * Calculates the distance of a given Point in world space to a given line,
 * defined by the vector couple (Origin, Direction).
 *
 * @param	Point				point to check distance to Axis
 * @param	Direction			unit vector indicating the direction to check against
 * @param	Origin				point of reference used to calculate distance
 * @param	out_ClosestPoint	optional point that represents the closest point projected onto Axis
 *
 * @return	distance of Point from line defined by (Origin, Direction)
 */
FLOAT PointDistToLine(const FVector &Point, const FVector &Direction, const FVector &Origin, FVector &OutClosestPoint)
{
	const FVector SafeDir = Direction.SafeNormal();
	OutClosestPoint = Origin + (SafeDir * ((Point-Origin) | SafeDir));
	return (OutClosestPoint-Point).Size();
}

FLOAT PointDistToLine(const FVector &Point, const FVector &Direction, const FVector &Origin)
{
	const FVector SafeDir = Direction.SafeNormal();
	const FVector OutClosestPoint = Origin + (SafeDir * ((Point-Origin) | SafeDir));
	return (OutClosestPoint-Point).Size();
}


/**
 * Returns closest distance from a point to a segment.
 * The idea is to project point on line formed by segment.
 * Then we see if the closest point on the line is outside of segment or inside.
 *
 * @param	Point			point to check distance for
 * @param	StartPoint		StartPoint of segment
 * @param	EndPoint		EndPoint of segment
 * @param	OutClosestPoint	Closest point on segment.
 *
 * @return	closest distance from Point to segment defined by (StartPoint, EndPoint).
 */
FLOAT PointDistToSegment(const FVector &Point, const FVector &StartPoint, const FVector &EndPoint, FVector &OutClosestPoint) 
{
    const FVector Segment = EndPoint - StartPoint;
    const FVector VectToPoint = Point - StartPoint;

	// See if closest point is before StartPoint
	const FLOAT Dot1 = VectToPoint | Segment;
    if( Dot1 <= 0 )
	{
		OutClosestPoint = StartPoint;
        return VectToPoint.Size();
	}

	// See if closest point is beyond EndPoint
    const FLOAT Dot2 = Segment | Segment;
    if( Dot2 <= Dot1 )
	{
		OutClosestPoint = EndPoint;
        return (Point - EndPoint).Size();
	}

	// Closest Point is within segment
	OutClosestPoint = StartPoint + Segment * (Dot1 / Dot2);
    return (Point - OutClosestPoint).Size();
}

/** 
* Find closest points between 2 segments.
* @param	(A1, B1)	defines the first segment.
* @param	(A2, B2)	defines the second segment.
* @param	OutP1		Closest point on segment 1 to segment 2.
* @param	OutP2		Closest point on segment 2 to segment 1.
*/
void SegmentDistToSegmentSafe(FVector A1, FVector B1, FVector A2, FVector B2, FVector& OutP1, FVector& OutP2)
{
	// Segments
	const	FVector	S1		= B1 - A1;
	const	FVector	S2		= B2 - A2;
	const	FVector	S3		= A1 - A2;


	const	FVector	S1_norm		= S1.SafeNormal();
	const	FVector	S2_norm		= S2.SafeNormal();

	const	FLOAT	Dot11	= S1 | S1;	// always >= 0
	const	FLOAT	Dot22	= S2 | S2;	// always >= 0
	const	FLOAT	Dot12	= S1 | S2;
	const	FLOAT	Dot13	= S1 | S3;
	const	FLOAT	Dot23	= S2 | S3;

	const	FLOAT	Dot11_norm	= S1_norm | S1_norm;	// always >= 0
	const	FLOAT	Dot22_norm	= S2_norm | S2_norm;	// always >= 0
	const	FLOAT	Dot12_norm	= S1_norm | S2_norm;

	// Numerator
	FLOAT	N1, N2;

	// Denominator
	const	FLOAT	D		= Dot11*Dot22 - Dot12*Dot12;	// always >= 0
	const	FLOAT	D_norm	= Dot11_norm*Dot22_norm - Dot12_norm*Dot12_norm;	// always >= 0

	FLOAT	D1		= D;		// T1 = N1 / D1, default D1 = D >= 0
	FLOAT	D2		= D;		// T2 = N2 / D2, default D2 = D >= 0

	// compute the line parameters of the two closest points
	if( D < KINDA_SMALL_NUMBER || D_norm < KINDA_SMALL_NUMBER ) 
	{ 
		// the lines are almost parallel
		N1 = 0.f;	// force using point A on segment S1
		D1 = 1.f;	// to prevent possible division by 0 later
		N2 = Dot23;
		D2 = Dot22;
	}
	else 
	{                
		// get the closest points on the infinite lines
		N1 = (Dot12*Dot23 - Dot22*Dot13);
		N2 = (Dot11*Dot23 - Dot12*Dot13);

		if( N1 < 0.f ) 
		{
			// t1 < 0.f => the s==0 edge is visible
			N1 = 0.f;
			N2 = Dot23;
			D2 = Dot22;
		}
		else if( N1 > D1 ) 
		{
			// t1 > 1 => the t1==1 edge is visible
			N1 = D1;
			N2 = Dot23 + Dot12;
			D2 = Dot22;
		}
	}

	if( N2 < 0.f ) 
	{           
		// t2 < 0 => the t2==0 edge is visible
		N2 = 0.f;

		// recompute t1 for this edge
		if( -Dot13 < 0.f )
		{
			N1 = 0.f;
		}
		else if( -Dot13 > Dot11 )
		{
			N1 = D1;
		}
		else 
		{
			N1 = -Dot13;
			D1 = Dot11;
		}
	}
	else if( N2 > D2 ) 
	{      
		// t2 > 1 => the t2=1 edge is visible
		N2 = D2;

		// recompute t1 for this edge
		if( (-Dot13 + Dot12) < 0.f )
		{
			N1 = 0.f;
		}
		else if( (-Dot13 + Dot12) > Dot11 )
		{
			N1 = D1;
		}
		else 
		{
			N1 = (-Dot13 + Dot12);
			D1 = Dot11;
		}
	}

	// finally do the division to get the points' location
	const FLOAT T1 = (Abs(N1) < KINDA_SMALL_NUMBER ? 0.f : N1 / D1);
	const FLOAT T2 = (Abs(N2) < KINDA_SMALL_NUMBER ? 0.f : N2 / D2);

	// return the closest points
	OutP1 = A1 + T1 * S1;
	OutP2 = A2 + T2 * S2;
}

/** 
 * Find closest points between 2 segments.
 * @param	(A1, B1)	defines the first segment.
 * @param	(A2, B2)	defines the second segment.
 * @param	OutP1		Closest point on segment 1 to segment 2.
 * @param	OutP2		Closest point on segment 2 to segment 1.
 * @warning - not numerically stable when lines are colinear
 */
void SegmentDistToSegment(FVector A1, FVector B1, FVector A2, FVector B2, FVector& OutP1, FVector& OutP2)
{
	// Segments
	const	FVector	S1		= B1 - A1;
	const	FVector	S2		= B2 - A2;
	const	FVector	S3		= A1 - A2;

	const	FLOAT	Dot11	= S1 | S1;	// always >= 0
	const	FLOAT	Dot22	= S2 | S2;	// always >= 0
	const	FLOAT	Dot12	= S1 | S2;
	const	FLOAT	Dot13	= S1 | S3;
	const	FLOAT	Dot23	= S2 | S3;

	// Numerator
			FLOAT	N1, N2;

	// Denominator
	const	FLOAT	D		= Dot11*Dot22 - Dot12*Dot12;	// always >= 0
			FLOAT	D1		= D;		// T1 = N1 / D1, default D1 = D >= 0
			FLOAT	D2		= D;		// T2 = N2 / D2, default D2 = D >= 0

	// compute the line parameters of the two closest points
	if( D < KINDA_SMALL_NUMBER ) 
	{ 
		// the lines are almost parallel
		N1 = 0.f;	// force using point A on segment S1
		D1 = 1.f;	// to prevent possible division by 0 later
		N2 = Dot23;
		D2 = Dot22;
	}
	else 
	{                
		// get the closest points on the infinite lines
		N1 = (Dot12*Dot23 - Dot22*Dot13);
		N2 = (Dot11*Dot23 - Dot12*Dot13);

		if( N1 < 0.f ) 
		{
			// t1 < 0.f => the s==0 edge is visible
			N1 = 0.f;
			N2 = Dot23;
			D2 = Dot22;
		}
		else if( N1 > D1 ) 
		{
			// t1 > 1 => the t1==1 edge is visible
			N1 = D1;
			N2 = Dot23 + Dot12;
			D2 = Dot22;
		}
	}

	if( N2 < 0.f ) 
	{           
		// t2 < 0 => the t2==0 edge is visible
		N2 = 0.f;

		// recompute t1 for this edge
		if( -Dot13 < 0.f )
		{
			N1 = 0.f;
		}
		else if( -Dot13 > Dot11 )
		{
			N1 = D1;
		}
		else 
		{
			N1 = -Dot13;
			D1 = Dot11;
		}
	}
	else if( N2 > D2 ) 
	{      
		// t2 > 1 => the t2=1 edge is visible
		N2 = D2;

		// recompute t1 for this edge
		if( (-Dot13 + Dot12) < 0.f )
		{
			N1 = 0.f;
		}
		else if( (-Dot13 + Dot12) > Dot11 )
		{
			N1 = D1;
		}
		else 
		{
			N1 = (-Dot13 + Dot12);
			D1 = Dot11;
		}
	}

	// finally do the division to get the points' location
	const FLOAT T1 = (Abs(N1) < KINDA_SMALL_NUMBER ? 0.f : N1 / D1);
	const FLOAT T2 = (Abs(N2) < KINDA_SMALL_NUMBER ? 0.f : N2 / D2);

	// return the closest points
	OutP1 = A1 + T1 * S1;
	OutP2 = A2 + T2 * S2;
}

FLOAT GetTForSegmentPlaneIntersect(const FVector& StartPoint, const FVector& EndPoint, const FPlane& Plane)
{
	return ( Plane.W - (StartPoint|Plane) ) / ( (EndPoint - StartPoint)|Plane);	
}

UBOOL SegmentPlaneIntersection(const FVector& StartPoint, const FVector& EndPoint, const FPlane& Plane, FVector& out_IntersectPoint)
{
	FLOAT T = GetTForSegmentPlaneIntersect(StartPoint,EndPoint,Plane);
	// If the parameter value is not between 0 and 1, there is no intersection
	if( T > -KINDA_SMALL_NUMBER && T < 1.f+KINDA_SMALL_NUMBER )
	{
		out_IntersectPoint = StartPoint + T * (EndPoint - StartPoint);
		return TRUE;
	}
	return FALSE;
}

/**
* Determine if a plane and an AABB intersect
* @param P - the plane to test
* @param AABB - the axis aligned bounding box to test
* @return if collision occurs
*/
UBOOL FPlaneAABBIsect(const FPlane& P, const FBox& AABB)
{
	// find diagonal most closely aligned with normal of plane
	FVector Vmin, Vmax;

	// Bypass the slow FVector[] operator. Not RESTRICT because it won't update Vmin, Vmax
	FLOAT* VminPtr = (FLOAT*)&Vmin;
	FLOAT* VmaxPtr = (FLOAT*)&Vmax;

	// Use restrict to get better instruction scheduling and to bypass the slow FVector[] operator
	const FLOAT* RESTRICT AABBMinPtr = (const FLOAT*)&AABB.Min;
	const FLOAT* RESTRICT AABBMaxPtr = (const FLOAT*)&AABB.Max;
	const FLOAT* RESTRICT PlanePtr = (const FLOAT*)&P;

	for(INT Idx=0;Idx<3;++Idx)
	{
		if(PlanePtr[Idx] >= 0.f)
		{
			VminPtr[Idx] = AABBMinPtr[Idx];
			VmaxPtr[Idx] = AABBMaxPtr[Idx];
		}
		else
		{
			VminPtr[Idx] = AABBMaxPtr[Idx];
			VmaxPtr[Idx] = AABBMinPtr[Idx]; 
		}
	}

	// if either diagonal is right on the plane, or one is on either side we have an interesection
	FLOAT dMax = P.PlaneDot(Vmax);
	FLOAT dMin = P.PlaneDot(Vmin);

	// if Max is below plane, or Min is above we know there is no intersection.. otherwise there must be one
	return (dMax >= 0.f && dMin <= 0.f);
}

/**
* Returns closest point on a triangle to a point.
* The idea is to identify the halfplanes that the point is
* in relative to each triangle segment "plane"
*
* @param	Point			point to check distance for
* @param	A,B,C			counter clockwise ordering of points defining a triangle
*
* @return	Point on triangle ABC closest to given point or Point if failure
*/
FVector ClosestPointOnTriangleToPoint(const FVector& Point, const FVector& A, const FVector& B, const FVector& C)
{
	//Figure out what region the point is in and compare against that "point" or "edge"
	const FVector BA = A - B;
	const FVector AC = C - A;
	const FVector CB = B - C;
	const FVector TriNormal = BA ^ CB;

	// Get the planes that define this triangle
	// edges BA, AC, BC with normals perpendicular to the edges facing outward
	const FPlane Planes[3] = { FPlane(B, TriNormal ^ BA), FPlane(A, TriNormal ^ AC), FPlane(C, TriNormal ^ CB) };
	INT PlaneHalfspaceBitmask = 0;

	//Determine which side of each plane the test point exists
	for (INT i=0; i<3; i++)
	{
		if (Planes[i].PlaneDot(Point) > 0.0f)
		{
			PlaneHalfspaceBitmask |= (1 << i);
		}
	}

	FVector Result(Point.X, Point.Y, Point.Z);
	switch (PlaneHalfspaceBitmask)
	{
	case 0: //000 Inside
		return FPointPlaneProject(Point, A, B, C);
	case 1:	//001 Segment BA
		PointDistToSegment(Point, B, A, Result);
		break;
	case 2:	//010 Segment AC
		PointDistToSegment(Point, A, C, Result);
		break;
	case 3:	//011 point A
		return A;
	case 4: //100 Segment BC
		PointDistToSegment(Point, B, C, Result);
		break;
	case 5: //101 point B
		return B;
	case 6: //110 point C
		return C;
	default:
		debugf(TEXT("Impossible result in ClosestPointOnTriangleToPoint"));
		break;
	}

	return Result;
}

/*
 * Computes the barycentric coordinates for a given point in a triangle
 *
 * @param	Point			point to convert to barycentric coordinates (in plane of ABC)
 * @param	A,B,C			three non-colinear points defining a triangle in CCW
 * 
 * @return Vector containing the three weights a,b,c such that Point = a*A + b*B + c*C
 *							                                or Point = A + b*(B-A) + c*(C-A) = (1-b-c)*A + b*B + c*C
 */
FVector ComputeBaryCentric2D(const FVector& Point, const FVector& A, const FVector& B, const FVector& C)
{
	// Compute the normal of the triangle
	const FVector TriNorm = (B-A) ^ (C-A);

	//check collinearity of A,B,C
	check(TriNorm.SizeSquared() > SMALL_NUMBER && "Collinear points in ComputeBaryCentric2D()");

	const FVector N = TriNorm.SafeNormal();

	// Compute twice area of triangle ABC
	const FLOAT AreaABCInv = 1.0f / (N | TriNorm);

	// Compute a contribution
	const FLOAT AreaPBC = N | ((B-Point) ^ (C-Point));
	const FLOAT a = AreaPBC * AreaABCInv;

	// Compute b contribution
	const FLOAT AreaPCA = N | ((C-Point) ^ (A-Point));
	const FLOAT b = AreaPCA * AreaABCInv;

	// Compute c contribution
	return FVector(a, b, 1.0f - a - b);
}

/*
 * Computes the barycentric coordinates for a given point on a tetrahedron (3D)
 *
 * @param	Point			point to convert to barycentric coordinates
 * @param	A,B,C,D			four points defining a tetrahedron
 *
 * @return Vector containing the four weights a,b,c,d such that Point = a*A + b*B + c*C + d*D
 */
FVector4 ComputeBaryCentric3D(const FVector& Point, const FVector& A, const FVector& B, const FVector& C, const FVector& D)
{	
	//http://www.devmaster.net/wiki/Barycentric_coordinates
	//Pick A as our origin and
	//Setup three basis vectors AB, AC, AD
	const FVector B1 = (B-A);
	const FVector B2 = (C-A);
	const FVector B3 = (D-A);

	//check co-planarity of A,B,C,D
	check( fabsf(B1 | (B2 ^ B3)) > SMALL_NUMBER && "Coplanar points in ComputeBaryCentric3D()");

	//Transform Point into this new space
	const FVector V = (Point - A);

	//Create a matrix of linearly independent vectors
	const FMatrix SolvMat(B1, B2, B3, FVector(0,0,0));

	//The point V can be expressed as Ax=v where x is the vector containing the weights {w1...wn}
	//Solve for x by multiplying both sides by AInv   (AInv * A)x = AInv * v ==> x = AInv * v
	const FMatrix InvSolvMat = SolvMat.InverseSafe();
	const FPlane BaryCoords = InvSolvMat.TransformNormal(V);	 

	//Reorder the weights to be a, b, c, d
	return FVector4(1.0f - BaryCoords.X - BaryCoords.Y - BaryCoords.Z, BaryCoords.X, BaryCoords.Y, BaryCoords.Z);
}

/**
 * Returns closest point on a tetrahedron to a point.
 * The idea is to identify the halfplanes that the point is
 * in relative to each face of the tetrahedron
 *
 * @param	Point			point to check distance for
 * @param	A,B,C,D			four points defining a tetrahedron
 *
 * @return	Point on tetrahedron ABCD closest to given point or Point if inside/failure
 */
FVector ClosestPointOnTetrahedronToPoint(const FVector& Point, const FVector& A, const FVector& B, const FVector& C, const FVector& D)
{
	//Check for coplanarity of all four points
	check( fabsf((C-A) | ((B-A)^(D-C))) > 0.0001f && "Coplanar points in ComputeBaryCentric3D()");

	//http://osdir.com/ml/games.devel.algorithms/2003-02/msg00394.html
	//     D
	//    /|\		  C-----------B
	//   / | \		   \         /
	//  /  |  \	   or	\  \A/	/
    // C   |   B		 \	|  /
	//  \  |  /			  \	| /
    //   \ | /			   \|/
	//     A				D
	
	// Figure out the ordering (is D in the direction of the CCW triangle ABC)
	FVector Pt1(A),Pt2(B),Pt3(C),Pt4(D);
 	const FPlane ABC(A,B,C);
 	if (ABC.PlaneDot(D) < 0.0f)
 	{
 		//Swap two points to maintain CCW orders
 		Pt3 = D;
 		Pt4 = C;
 	}
		
	//Tetrahedron made up of 4 CCW faces - DCA, DBC, DAB, ACB
	const FPlane Planes[4] = {FPlane(Pt4,Pt3,Pt1), FPlane(Pt4,Pt2,Pt3), FPlane(Pt4,Pt1,Pt2), FPlane(Pt1,Pt3,Pt2)};

	//Determine which side of each plane the test point exists
	INT PlaneHalfspaceBitmask = 0;
	for (INT i=0; i<4; i++)
	{
		if (Planes[i].PlaneDot(Point) > 0.0f)
		{
			PlaneHalfspaceBitmask |= (1 << i);
		}
	}

	//Verts + Faces - Edges = 2	(Euler)
	FVector Result(Point.X, Point.Y, Point.Z);
	switch (PlaneHalfspaceBitmask)
	{
	case 0:	 //inside (0000)
		//@TODO - could project point onto any face
		break;
	case 1:	 //0001 Face	DCA
		return ClosestPointOnTriangleToPoint(Point, Pt4, Pt3, Pt1);
	case 2:	 //0010 Face	DBC
		return ClosestPointOnTriangleToPoint(Point, Pt4, Pt2, Pt3);
	case 3:  //0011	Edge	DC
		PointDistToSegment(Point, Pt4, Pt3, Result);
		break;
	case 4:	 //0100 Face	DAB
		return ClosestPointOnTriangleToPoint(Point, Pt4, Pt1, Pt2);
	case 5:  //0101	Edge	DA
		PointDistToSegment(Point, Pt4, Pt1, Result);
		break;
	case 6:  //0110	Edge	DB
		PointDistToSegment(Point, Pt4, Pt2, Result);
		break;
	case 7:	 //0111 Point	D
		return Pt4;
	case 8:	 //1000 Face	ACB
		return ClosestPointOnTriangleToPoint(Point, Pt1, Pt3, Pt2);
	case 9:  //1001	Edge	AC	
		PointDistToSegment(Point, Pt1, Pt3, Result);
		break;
	case 10: //1010	Edge	BC
		PointDistToSegment(Point, Pt2, Pt3, Result);
		break;
	case 11: //1011 Point	C
		return Pt3;
	case 12: //1100	Edge	BA
		PointDistToSegment(Point, Pt2, Pt1, Result);
		break;
	case 13: //1101 Point	A
		return Pt1;
	case 14: //1110 Point	B
		return Pt2;
	default: //impossible (1111)
		debugf(TEXT("ClosestPointOnTetrahedronToPoint() : impossible result"));
		break;
	}

	return Result;
}

/** 
 * Find closest point on a Sphere to a Line.
 * When line intersects	Sphere, then closest point to LineOrigin is returned.
 * @param SphereOrigin		Origin of Sphere
 * @param SphereRadius		Radius of Sphere
 * @param LineOrigin		Origin of line
 * @param LineDir			Direction of line.
 * @param OutClosestPoint	Closest point on sphere to given line.
 */
void SphereDistToLine(FVector SphereOrigin, FLOAT SphereRadius, FVector LineOrigin, FVector LineDir, FVector& OutClosestPoint)
{
	const FLOAT A	= LineDir | LineDir;
	const FLOAT B	= 2.f * (LineDir | (LineOrigin - SphereOrigin));
	const FLOAT C	= (SphereOrigin|SphereOrigin) + (LineOrigin|LineOrigin) - 2.f *(SphereOrigin|LineOrigin) - Square(SphereRadius);
	const FLOAT D	= Square(B) - 4.f * A * C;

	if( D <= KINDA_SMALL_NUMBER )
	{
		// line is not intersecting sphere (or is tangent at one point if D == 0 )
		const FVector PointOnLine = LineOrigin + ( -B / 2.f * A ) * LineDir;
		OutClosestPoint = SphereOrigin + (PointOnLine - SphereOrigin).SafeNormal() * SphereRadius;
	}
	else
	{
		// Line intersecting sphere in 2 points. Pick closest to line origin.
		const FLOAT	E	= appSqrt(D);
		const FLOAT T1	= (-B + E) / (2.f * A);
		const FLOAT T2	= (-B - E) / (2.f * A);
		const FLOAT T	= Abs(T1) < Abs(T2) ? T1 : T2;

		OutClosestPoint	= LineOrigin + T * LineDir;
	}
}

/**
 * Calculates whether a Point is within a cone segment, and also what percentage within the cone
 *
 * @param Point - The Point in question
 * @param ConeStartPoint - the beginning of the cone (with the smallest radius)
 * @param ConeLine - the line out from the start point that ends at the largest radius point of the cone
 * @param radiusAtStart - the radius at the ConeStartPoint (0 for a 'proper' cone)
 * @param radiusAtEnd - the largest radius of the cone
 * @param percentageOut - output variable the holds how much within the cone the point is (1 = on center line, 0 = on exact edge or outside cone).
 *
 * @return true if the point is within the cone, false otherwise.
 */
UBOOL GetDistanceWithinConeSegment(FVector Point, FVector ConeStartPoint, FVector ConeLine, FLOAT RadiusAtStart, FLOAT RadiusAtEnd, FLOAT &PercentageOut)
{
	check(RadiusAtStart >= 0.0f && RadiusAtEnd >= 0.0f && ConeLine.SizeSquared() > 0);
	// -- First we'll draw out a line from the ConeStartPoint down the ConeLine. We'll find the closest point on that line to Point.
	//    If we're outside the max distance, or behind the StartPoint, we bail out as that means we've no chance to be in the cone.

	FVector PointOnCone; // Stores the point on the cone's center line closest to our target point.

	const FLOAT Distance = PointDistToLine(Point, ConeLine, ConeStartPoint, PointOnCone); // distance is how far from the viewline we are

	PercentageOut = 0.0; // start assuming we're outside cone until proven otherwise.

	const FVector VectToStart = ConeStartPoint - PointOnCone;
	const FVector VectToEnd = (ConeStartPoint + ConeLine) - PointOnCone;
	
	const FLOAT ConeLengthSqr = ConeLine.SizeSquared();
	const FLOAT DistToStartSqr = VectToStart.SizeSquared();
	const FLOAT DistToEndSqr = VectToEnd.SizeSquared();

	if (DistToStartSqr > ConeLengthSqr || DistToEndSqr > ConeLengthSqr)
	{
		//Outside cone
		return FALSE;
	}

	const FLOAT PercentAlongCone = appSqrt(DistToStartSqr) / appSqrt(ConeLengthSqr); // don't have to catch outside 0->1 due to above code (saves 2 sqrts if outside)
	const FLOAT RadiusAtPoint = RadiusAtStart + ((RadiusAtEnd - RadiusAtStart) * PercentAlongCone);

	if(Distance > RadiusAtPoint) // target is farther from the line than the radius at that distance)
		return FALSE;

	PercentageOut = RadiusAtPoint > 0.0f ? (RadiusAtPoint - Distance) / RadiusAtPoint : 1.0f;

	return TRUE;
}

/**
 * Calculates the dotted distance of vector 'Direction' to coordinate system O(AxisX,AxisY,AxisZ).
 *
 * Orientation: (consider 'O' the first person view of the player, and 'Direction' a vector pointing to an enemy)
 * - positive azimuth means enemy is on the right of crosshair. (negative means left).
 * - positive elevation means enemy is on top of crosshair, negative means below.
 *
 * @Note: 'Azimuth' (.X) sign is changed to represent left/right and not front/behind. front/behind is the funtion's return value.
 *
 * @param	OutDotDist	.X = 'Direction' dot AxisX relative to plane (AxisX,AxisZ). (== Cos(Azimuth))
 *						.Y = 'Direction' dot AxisX relative to plane (AxisX,AxisY). (== Sin(Elevation))
 * @param	Direction	direction of target.
 * @param	AxisX		X component of reference system.
 * @param	AxisY		Y component of reference system.
 * @param	AxisZ		Z component of reference system.
 *
 * @return	true if 'Direction' is facing AxisX (Direction dot AxisX >= 0.f)
 */

UBOOL GetDotDistance
( 
	FVector2D	&OutDotDist, 
	FVector		&Direction, 
	FVector		&AxisX, 
	FVector		&AxisY, 
	FVector		&AxisZ 
)
{
	const FVector NormalDir = Direction.SafeNormal();

	// Find projected point (on AxisX and AxisY, remove AxisZ component)
	FVector NoZProjDir = ( NormalDir - (NormalDir | AxisZ) * AxisZ ).SafeNormal();
	
	// Figure out on which Azimuth dot is on right or left.
	const FLOAT AzimuthSign = ( (NoZProjDir | AxisY) < 0.f ) ? -1.f : 1.f;

	OutDotDist.Y	= NormalDir | AxisZ;
	const FLOAT DirDotX	= NoZProjDir | AxisX;
	OutDotDist.X	= AzimuthSign * Abs(DirDotX);

	return (DirDotX >= 0.f );
}


/**
 * Calculates the angular distance of vector 'Direction' to coordinate system O(AxisX,AxisY,AxisZ).
 *
 * Orientation: (consider 'O' the first person view of the player, and 'Direction' a vector pointing to an enemy)
 * - positive azimuth means enemy is on the right of crosshair. (negative means left).
 * - positive elevation means enemy is on top of crosshair, negative means below.
 *
 * @param	OutAngularDist	.X = Azimuth angle (in radians) of 'Direction' vector compared to plane (AxisX,AxisZ).
 *							.Y = Elevation angle (in radians) of 'Direction' vector compared to plane (AxisX,AxisY).
 * @param	Direction		Direction of target.
 * @param	AxisX			X component of reference system.
 * @param	AxisY			Y component of reference system.
 * @param	AxisZ			Z component of reference system.
 *
 * @return	true if 'Direction' is facing AxisX (Direction dot AxisX >= 0.f)
 */

UBOOL GetAngularDistance
(
	FVector2D	&OutAngularDist, 
	FVector		&Direction, 
	FVector		&AxisX, 
	FVector		&AxisY, 
	FVector		&AxisZ 	
)
{
	FVector2D	DotDist;

	// Get Dotted distance
	const UBOOL bIsInFront = GetDotDistance( DotDist, Direction, AxisX, AxisY, AxisZ );
	GetAngularFromDotDist( OutAngularDist, DotDist );

	return bIsInFront;
}

/**
* Converts Dot distance to angular distance.
* @see	GetAngularDistance() and GetDotDistance().
*
* @param	OutAngDist	Angular distance in radians.
* @param	DotDist		Dot distance.
*/

void GetAngularFromDotDist( FVector2D &OutAngDist, FVector2D &DotDist )
{
	const FLOAT AzimuthSign = (DotDist.X < 0.f) ? -1.f : 1.f;
	DotDist.X = Abs(DotDist.X);

	// Convert to angles in Radian.
	// Because of mathematical imprecision, make sure dot values are within the [-1.f,1.f] range.
	OutAngDist.X = appAcos(DotDist.X) * AzimuthSign;
	OutAngDist.Y = appAsin(DotDist.Y);
}

/** Interpolate a normal vector Current to Target, by interpolating the angle between those vectors with constant step. */
FVector VInterpNormalRotationTo(const FVector& Current, const FVector& Target, FLOAT DeltaTime, FLOAT RotationSpeedDegrees)
{
	// Find delta rotation between both normals.
	FQuat DeltaQuat = FQuatFindBetween(Current, Target);

	// Decompose into an axis and angle for rotation
	FVector DeltaAxis(0.f);
	FLOAT DeltaAngle = 0.f;
	DeltaQuat.ToAxisAndAngle(DeltaAxis, DeltaAngle);

	// Find rotation step for this frame
	const FLOAT RotationStepRadians = RotationSpeedDegrees * (PI / 180) * DeltaTime;

	if( Abs(DeltaAngle) > RotationStepRadians )
	{
		DeltaAngle = Clamp(DeltaAngle, -RotationStepRadians, RotationStepRadians);
		DeltaQuat = FQuat(DeltaAxis, DeltaAngle);
		return FQuatRotationTranslationMatrix( DeltaQuat, FVector(0.f) ).TransformNormal(Current);
	}
	return Target;
}

/** Interpolate a normal vector from Current to Target with constant step */
FVector VInterpNormalConstantTo(const FVector Current, const FVector& Target, FLOAT DeltaTime, FLOAT InterpSpeed)
{
	const FVector Delta = Target - Current;
	const FLOAT DeltaM = Delta.Size();
	const FLOAT MaxStep = InterpSpeed * DeltaTime;

	if( DeltaM > MaxStep )
	{
		if( MaxStep > 0.f )
		{
			const FVector DeltaN = Delta / DeltaM;
			return (Current + DeltaN * MaxStep).SafeNormal();
		}
		else
		{
			return Current;
		}
	}

	return Target;
}

/** Interpolate vector from Current to Target with constant step */
FVector VInterpConstantTo(const FVector Current, const FVector& Target, FLOAT DeltaTime, FLOAT InterpSpeed)
{
	const FVector Delta = Target - Current;
	const FLOAT DeltaM = Delta.Size();
	const FLOAT MaxStep = InterpSpeed * DeltaTime;

	if( DeltaM > MaxStep )
	{
		if( MaxStep > 0.f )
		{
			const FVector DeltaN = Delta / DeltaM;
			return Current + DeltaN * MaxStep;
		}
		else
		{
			return Current;
		}
	}

	return Target;
}

/** Clamp of Vector A From Min to Max of XYZ **/
FVector VClamp(FVector A, FVector Min, FVector Max)
{
	A.X = Clamp<FLOAT>(A.X,Min.X,Max.X);
	A.Y = Clamp<FLOAT>(A.Y,Min.Y,Max.Y);
	A.Z = Clamp<FLOAT>(A.Z,Min.Z,Max.Z);
	return A;
}

/** Interpolate vector from Current to Target. Scaled by distance to Target, so it has a strong start speed and ease out. */
FVector VInterpTo( const FVector& Current, const FVector& Target, FLOAT& DeltaTime, FLOAT InterpSpeed )
{
	// If no interp speed, jump to target value
	if( InterpSpeed <= 0.f )
	{
		return Target;
	}

	// Distance to reach
	const FVector Dist = Target - Current;

	// If distance is too small, just set the desired location
	if( Dist.SizeSquared() < KINDA_SMALL_NUMBER )
	{
		return Target;
	}

	// Delta Move, Clamp so we do not over shoot.
	const FVector	DeltaMove = Dist * Clamp<FLOAT>(DeltaTime * InterpSpeed, 0.f, 1.f);

	return Current + DeltaMove;
}

FRotator RInterpTo( const FRotator& Current, const FRotator& Target, FLOAT& DeltaTime, FLOAT InterpSpeed, UBOOL bConstantInterpSpeed )
{
	// if DeltaTime is 0, do not perform any interpolation (Location was already calculated for that frame)
	if( DeltaTime == 0.f || Current == Target )
	{
		return Current;
	}

	// If no interp speed, jump to target value
	if( InterpSpeed <= 0.f )
	{
		return Target;
	}

	const FLOAT DeltaInterpSpeed = InterpSpeed * DeltaTime;
	if (!bConstantInterpSpeed)
	{
		// Delta Move, Clamp so we do not over shoot.
		const FRotator DeltaMove = (Target - Current).GetNormalized() * Clamp<FLOAT>(DeltaInterpSpeed, 0.f, 1.f);

		// If steps are too small, just return Target and consider we've reached our destination.
		if( DeltaMove.IsZero() )
		{
			return Target;
		}

		return (Current + DeltaMove).GetNormalized();
	}
	else
	{
		const FRotator DeltaMove = (Target - Current).GetNormalized();
		FRotator Result = Current;
		INT DeltaInterpSpeedInt = appTrunc(DeltaInterpSpeed);
		Result.Pitch += Clamp(DeltaMove.Pitch,-DeltaInterpSpeedInt,DeltaInterpSpeedInt);
		Result.Yaw += Clamp(DeltaMove.Yaw,-DeltaInterpSpeedInt,DeltaInterpSpeedInt);
		Result.Roll += Clamp(DeltaMove.Roll,-DeltaInterpSpeedInt,DeltaInterpSpeedInt);
		return Result.GetNormalized();
	}
}


FLOAT FInterpTo( FLOAT Current, FLOAT Target, FLOAT DeltaTime, FLOAT InterpSpeed )
{
	// If no interp speed, jump to target value
	if( InterpSpeed == 0.f )
	{
		return Target;
	}

	// Distance to reach
	const FLOAT Dist = Target - Current;

	// If distance is too small, just set the desired location
	if( Square(Dist) < SMALL_NUMBER )
	{
		return Target;
	}

	// Delta Move, Clamp so we do not over shoot.
	const FLOAT DeltaMove = Dist * Clamp<FLOAT>(DeltaTime * InterpSpeed, 0.f, 1.f);

	return Current + DeltaMove;
}

FLOAT FInterpConstantTo( FLOAT Current, FLOAT Target, FLOAT DeltaTime, FLOAT InterpSpeed )
{
	const FLOAT Dist = Target - Current;

	// If distance is too small, just set the desired location
	if( Square(Dist) < SMALL_NUMBER )
	{
		return Target;
	}

	const FLOAT Step = InterpSpeed * DeltaTime;
	return Current + Clamp<FLOAT>(Dist, -Step, Step);
}


/** Interpolate between A and B, applying an ease in/out function.  Exp controls the degree of the curve. */
FLOAT FInterpEaseInOut( FLOAT A, FLOAT B, FLOAT Alpha, FLOAT Exp )
{
	FLOAT ModifiedAlpha;

	if( Alpha < 0.5f )
	{
		ModifiedAlpha = 0.5f * appPow(2.f * Alpha, Exp);
	}
	else
	{
		ModifiedAlpha = 1.f - 0.5f * appPow(2.f * (1.f - Alpha), Exp);
	}

	return Lerp<FLOAT>(A, B, ModifiedAlpha);
}


/**
 * Clamps a tangent formed by the specified control point values
 */
FLOAT FClampFloatTangent( FLOAT PrevPointVal, FLOAT PrevTime, FLOAT CurPointVal, FLOAT CurTime, FLOAT NextPointVal, FLOAT NextTime )
{
	const FLOAT PrevToNextTimeDiff = Max< DOUBLE >( KINDA_SMALL_NUMBER, NextTime - PrevTime );
	const FLOAT PrevToCurTimeDiff = Max< DOUBLE >( KINDA_SMALL_NUMBER, CurTime - PrevTime );
	const FLOAT CurToNextTimeDiff = Max< DOUBLE >( KINDA_SMALL_NUMBER, NextTime - CurTime );

	FLOAT OutTangentVal = 0.0f;

	const FLOAT PrevToNextHeightDiff = NextPointVal - PrevPointVal;
	const FLOAT PrevToCurHeightDiff = CurPointVal - PrevPointVal;
	const FLOAT CurToNextHeightDiff = NextPointVal - CurPointVal;

	// Check to see if the current point is crest
	if( ( PrevToCurHeightDiff >= 0.0f && CurToNextHeightDiff <= 0.0f ) ||
		( PrevToCurHeightDiff <= 0.0f && CurToNextHeightDiff >= 0.0f ) )
	{
		// Neighbor points are both both on the same side, so zero out the tangent
		OutTangentVal = 0.0f;
	}
	else
	{
		// The three points form a slope

		// Constants
		const FLOAT ClampThreshold = 0.333f;

		// Compute height deltas
		const FLOAT CurToNextTangent = CurToNextHeightDiff / CurToNextTimeDiff;
		const FLOAT PrevToCurTangent = PrevToCurHeightDiff / PrevToCurTimeDiff;
		const FLOAT PrevToNextTangent = PrevToNextHeightDiff / PrevToNextTimeDiff;

		// Default to not clamping
		const FLOAT UnclampedTangent = PrevToNextTangent;
		FLOAT ClampedTangent = UnclampedTangent;

		const FLOAT LowerClampThreshold = ClampThreshold;
		const FLOAT UpperClampThreshold = 1.0f - ClampThreshold;

		// @todo: Would we get better results using percentange of TIME instead of HEIGHT?
		const FLOAT CurHeightAlpha = PrevToCurHeightDiff / PrevToNextHeightDiff;

		if( PrevToNextHeightDiff > 0.0f )
		{
			if( CurHeightAlpha < LowerClampThreshold )
			{
				// 1.0 = maximum clamping (flat), 0.0 = minimal clamping (don't touch)
				const FLOAT ClampAlpha = 1.0f - CurHeightAlpha / ClampThreshold;
				const FLOAT LowerClamp = Lerp( PrevToNextTangent, PrevToCurTangent, ClampAlpha );
				ClampedTangent = Min( ClampedTangent, LowerClamp );
			}

			if( CurHeightAlpha > UpperClampThreshold )
			{
				// 1.0 = maximum clamping (flat), 0.0 = minimal clamping (don't touch)
				const FLOAT ClampAlpha = ( CurHeightAlpha - UpperClampThreshold ) / ClampThreshold;
				const FLOAT UpperClamp = Lerp( PrevToNextTangent, CurToNextTangent, ClampAlpha );
				ClampedTangent = Min( ClampedTangent, UpperClamp );
			}
		}
		else
		{

			if( CurHeightAlpha < LowerClampThreshold )
			{
				// 1.0 = maximum clamping (flat), 0.0 = minimal clamping (don't touch)
				const FLOAT ClampAlpha = 1.0f - CurHeightAlpha / ClampThreshold;
				const FLOAT LowerClamp = Lerp( PrevToNextTangent, PrevToCurTangent, ClampAlpha );
				ClampedTangent = Max( ClampedTangent, LowerClamp );
			}

			if( CurHeightAlpha > UpperClampThreshold )
			{
				// 1.0 = maximum clamping (flat), 0.0 = minimal clamping (don't touch)
				const FLOAT ClampAlpha = ( CurHeightAlpha - UpperClampThreshold ) / ClampThreshold;
				const FLOAT UpperClamp = Lerp( PrevToNextTangent, CurToNextTangent, ClampAlpha );
				ClampedTangent = Max( ClampedTangent, UpperClamp );
			}
		}

		OutTangentVal = ClampedTangent;
	}

	return OutTangentVal;
}

/** ConeHalfAngleRad is the half-angle of cone, in radians.  Returns a normalized vector. */
FVector VRandCone(FVector const& Dir, FLOAT ConeHalfAngleRad)
{
	if (ConeHalfAngleRad > 0.f)
	{
		FLOAT const RandU = appFrand();
		FLOAT const RandV = appFrand();

		// Get spherical coords that have an even distribution over the unit sphere
		// Method described at http://mathworld.wolfram.com/SpherePointPicking.html	
		FLOAT Theta = 2.f * PI * RandU;
		FLOAT Phi = appAcos((2.f * RandV) - 1.f);

		// restrict phi to [0, ConeHalfAngleRad]
		// this gives an even distribution of points on the surface of the cone
		// centered at the origin, pointing upward (z), with the desired angle
		Phi = appFmod(Phi, ConeHalfAngleRad);

		// get axes we need to rotate around
		FMatrix const DirMat = FRotationMatrix(Dir.Rotation());
		// note the axis translation, since we want the variation to be around X
		FVector const DirZ = DirMat.GetAxis(0);		
		FVector const DirY = DirMat.GetAxis(1);

		// convert to unreal rot units, to satisfy RotateAngleAxis
		FLOAT const Rad2Unr = 65536.f/(2.f*PI);
		FVector Result = Dir.RotateAngleAxis(appTrunc(Rad2Unr*Phi), DirY);
		Result = Result.RotateAngleAxis(appTrunc(Rad2Unr*Theta), DirZ);

		// ensure it's a unit vector (might not have been passed in that way)
		Result = Result.SafeNormal();
		
		return Result;
	}
	else
	{
		return Dir.SafeNormal();
	}
}

/** 
 * This is a version of VRandCone that handles "squished" cones, i.e. with different angle limits in the Y and Z axes.
 * Assumes world Y and Z, although this could be extended to handle arbitrary rotations.
 */
FVector VRandCone(FVector const& Dir, FLOAT HorizontalConeHalfAngleRad, FLOAT VerticalConeHalfAngleRad)
{
	if ( (VerticalConeHalfAngleRad > 0.f) && (HorizontalConeHalfAngleRad > 0.f) )
	{
		FLOAT const RandU = appFrand();
		FLOAT const RandV = appFrand();

		// Get spherical coords that have an even distribution over the unit sphere
		// Method described at http://mathworld.wolfram.com/SpherePointPicking.html	
		FLOAT Theta = 2.f * PI * RandU;
		FLOAT Phi = appAcos((2.f * RandV) - 1.f);

		// restrict phi to [0, ConeHalfAngleRad]
		// where ConeHalfAngleRad is now a function of Theta
		// (specifically, radius of an ellipse as a function of angle)
		// function is ellipse function (x/a)^2 + (y/b)^2 = 1, converted to polar coords
		FLOAT ConeHalfAngleRad = Square(appCos(Theta) / HorizontalConeHalfAngleRad) + Square(appSin(Theta) / VerticalConeHalfAngleRad);
		ConeHalfAngleRad = appSqrt(1.f / ConeHalfAngleRad);

		// clamp to make a cone instead of a sphere
		Phi = appFmod(Phi, ConeHalfAngleRad);

		// get axes we need to rotate around
		FMatrix const DirMat = FRotationMatrix(Dir.Rotation());
		// note the axis translation, since we want the variation to be around X
		FVector const DirZ = DirMat.GetAxis(0);		
		FVector const DirY = DirMat.GetAxis(1);

		// convert to unreal rot units, to satisfy RotateAngleAxis
		FLOAT const Rad2Unr = 65536.f/(2.f*PI);
		FVector Result = Dir.RotateAngleAxis(appTrunc(Rad2Unr*Phi), DirY);
		Result = Result.RotateAngleAxis(appTrunc(Rad2Unr*Theta), DirZ);

		// ensure it's a unit vector (might not have been passed in that way)
		Result = Result.SafeNormal();

		return Result;
	}
	else
	{
		return Dir.SafeNormal();
	}
}

struct FClusterMovedHereToMakeCompile
{
	FVector ClusterPosAccum;
	INT ClusterSize;
};


/**
 * Given a current set of cluster centers, a set of points, iterate N times to move clusters to be central. 
 *
 * @param NumConnectionsToBeValid  Someties you will have long strings that come off the mass of points which happen to have been chosen as Cluster starting points.  You want to be able to disregard those.
 **/
void GenerateClusterCenters(TArray<FVector>& Clusters, const TArray<FVector>& Points, INT NumIterations, INT NumConnectionsToBeValid)
{
	// Check we have >0 points and clusters
	if(Points.Num() == 0 || Clusters.Num() == 0)
	{
		return;
	}

	// Temp storage for each cluster that mirrors the order of the passed in Clusters array
	TArray<FClusterMovedHereToMakeCompile> ClusterData;
	ClusterData.AddZeroed( Clusters.Num() );

	// Then iterate
	for(INT ItCount=0; ItCount<NumIterations; ItCount++)
	{
		// Classify each point - find closest cluster center
		for(INT i=0; i<Points.Num(); i++)
		{
			const FVector& Pos = Points(i);

			// Iterate over all clusters to find closes one
			INT NearestClusterIndex = INDEX_NONE;
			FLOAT NearestClusterDist = BIG_NUMBER;
			for(INT j=0; j<Clusters.Num() ; j++)
			{
				const FLOAT Dist = (Pos - Clusters(j)).Size();
				if(Dist < NearestClusterDist)
				{
					NearestClusterDist = Dist;
					NearestClusterIndex = j;
				}
			}
			// Update its info with this point
			if( NearestClusterIndex != INDEX_NONE )
			{
				ClusterData(NearestClusterIndex).ClusterPosAccum += Pos;
				ClusterData(NearestClusterIndex).ClusterSize++;
			}
		}

		// All points classified - update cluster center as average of membership
		for(INT i=0; i<Clusters.Num(); i++)
		{
			if(ClusterData(i).ClusterSize > 0)
			{
				Clusters(i) = ClusterData(i).ClusterPosAccum / (FLOAT)ClusterData(i).ClusterSize;
			}
		}
	}

	// so now after we have possible cluster centers we want to remove the ones that are outliers and not part of the main cluster
	for(INT i=0; i<ClusterData.Num(); i++)
	{
		if(ClusterData(i).ClusterSize < NumConnectionsToBeValid)
		{
			Clusters.Remove(i);
		}
	}
}

/**
 * Tests a memory region to see that it's working properly.
 * Results are reported with appOutputDebugString().
 *
 * @param BaseAddress	Starting address
 * @param NumBytes		Number of bytes to test (will be rounded down to a multiple of 4)
 * @param MemoryName	Descriptive name of the memory region
 * @return				TRUE if the memory region passed the test
 */
UBOOL appMemoryTest( void* BaseAddress, DWORD NumBytes, const TCHAR* MemoryName )
{
	volatile DWORD* Ptr;
	DWORD NumDwords = NumBytes / 4;
	DWORD TestWords[2] = { 0xdeadbeef, 0x1337c0de };
	UBOOL bSucceeded = TRUE;

	appOutputDebugStringf(TEXT("Running memory test on %s memory...\n"), MemoryName);

	for ( INT TestIndex=0; TestIndex < 2; ++TestIndex )
	{
		// Fill the memory with a pattern.
		Ptr = (DWORD*) BaseAddress;
		for ( DWORD Index=0; Index < NumDwords; ++Index )
		{
			*Ptr = TestWords[TestIndex];
			Ptr++;
		}

		// Check that each DWORD is still ok and overwrite it with the complement.
		Ptr = (DWORD*) BaseAddress;
		for ( DWORD Index=0; Index < NumDwords; ++Index )
		{
			if ( *Ptr != TestWords[TestIndex] )
			{
				appOutputDebugStringf(TEXT("Failed memory test at 0x%08x, wrote: 0x%08x, read: 0x%08x\n"), Ptr, TestWords[TestIndex], *Ptr );
				bSucceeded = FALSE;
			}
			*Ptr = ~TestWords[TestIndex];
			Ptr++;
		}

		// Check again, now going backwards in memory.
		Ptr = ((DWORD*) BaseAddress) + NumDwords;
		for ( DWORD Index=0; Index < NumDwords; ++Index )
		{
			Ptr--;
			if ( *Ptr != ~TestWords[TestIndex] )
			{
				appOutputDebugStringf(TEXT("Failed memory test at 0x%08x, wrote: 0x%08x, read: 0x%08x\n"), Ptr, ~TestWords[TestIndex], *Ptr );
				bSucceeded = FALSE;
			}
			*Ptr = TestWords[TestIndex];
		}
	}

	if ( bSucceeded )
	{
		appOutputDebugString(TEXT("Memory test passed.\n\n"));
	}
	else
	{
		appOutputDebugString(TEXT("Memory test failed!\n\n"));
	}

	return bSucceeded;
}

/** Vector permute masks used by FBoneAtomVectorized on Xbox and PS3. */
#if ENABLE_VECTORIZED_FBONEATOM && ( XBOX || PS3 )

#define VectorPermuteMask(x,y,z,w) \
		MakeVectorRegister( \
			(DWORD)(0x00010203 | ((x) << 2) | ((x) << 10) | ((x) << 18) | ((x) << 26)), \
			(DWORD)(0x00010203 | ((y) << 2) | ((y) << 10) | ((y) << 18) | ((y) << 26)), \
			(DWORD)(0x00010203 | ((z) << 2) | ((z) << 10) | ((z) << 18) | ((z) << 26)), \
			(DWORD)(0x00010203 | ((w) << 2) | ((w) << 10) | ((w) << 18) | ((w) << 26)))

const VectorRegister FBoneAtom::Ax_Ay_Bx_Bz = VectorPermuteMask(0, 1, 4, 6);
const VectorRegister FBoneAtom::Az_By = VectorPermuteMask(2, 5, 0, 0);
const VectorRegister FBoneAtom::Ax_Bx_Bw_Aw = VectorPermuteMask(0, 4, 7, 3);
const VectorRegister FBoneAtom::Bz_Ay_By_Aw = VectorPermuteMask(6, 1, 5, 3);
const VectorRegister FBoneAtom::Bx_By_Az_Aw = VectorPermuteMask(4, 5, 2, 3);
const VectorRegister FBoneAtom::Ax_Ay_Bx_By = VectorPermuteMask(0, 1, 4, 5);
const VectorRegister FBoneAtom::Az_Az_Bz_Bz = VectorPermuteMask(2, 2, 6, 6);
const VectorRegister FBoneAtom::Ax_Az_Bx_By = VectorPermuteMask(0, 2, 4, 5);
const VectorRegister FBoneAtom::Ay_Ay_Bz_Bz = VectorPermuteMask(1, 1, 6, 6);
const VectorRegister FBoneAtom::Ax_Bz_By_Az = VectorPermuteMask(0, 6, 5, 2);
const VectorRegister FBoneAtom::Bx_Ay_Bw_Aw = VectorPermuteMask(4, 1, 7, 3);
const VectorRegister FBoneAtom::Bz_Bx_Ax_Az = VectorPermuteMask(6, 4, 0, 2);

#undef VectorPermuteMask

#endif // #if ENABLE_VECTORIZED_FBONEATOM && ( XBOX || PS3 )

///////////////////////////////////////////////////////
