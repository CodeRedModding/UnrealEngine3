/*=============================================================================
	UnMath.h: Unreal math routines
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*-----------------------------------------------------------------------------
	Defintions.
-----------------------------------------------------------------------------*/

// Forward declarations.
class  FVector;
class  FPlane;
class  FRotator;
class  FScale;
class  FGlobalMath;
class  FMatrix;
class  FQuat;

// Constants.
#undef  PI
#define PI 					(3.1415926535897932)
#define SMALL_NUMBER		(1.e-8)
#define KINDA_SMALL_NUMBER	(1.e-4)
#define BIG_NUMBER			(3.4e+38f)

// Aux constants.
#define INV_PI			(0.31830988618)
#define HALF_PI			(1.57079632679)

// Magic numbers for numerical precision.
#define DELTA			(0.00001f)
#define SLERP_DELTA		(0.0001f)

/*-----------------------------------------------------------------------------
	Global functions.
-----------------------------------------------------------------------------*/

//
// Snap a value to the nearest grid multiple.
//
inline FLOAT FSnap( FLOAT Location, FLOAT Grid )
{
	if( Grid==0.f )	return Location;
	else			return appFloor((Location + 0.5*Grid)/Grid)*Grid;
}

//
// Internal sheer adjusting function so it snaps nicely at 0 and 45 degrees.
//
inline FLOAT FSheerSnap (FLOAT Sheer)
{
	if		(Sheer < -0.65f) return Sheer + 0.15f;
	else if (Sheer > +0.65f) return Sheer - 0.15f;
	else if (Sheer < -0.55f) return -0.50f;
	else if (Sheer > +0.55f) return 0.50f;
	else if (Sheer < -0.05f) return Sheer + 0.05f;
	else if (Sheer > +0.05f) return Sheer - 0.05f;
	else					 return 0.f;
}

//
// Find the closest power of 2 that is >= N.
//
inline DWORD FNextPowerOfTwo( DWORD N )
{
	if (N<=0L		) return 0L;
	if (N<=1L		) return 1L;
	if (N<=2L		) return 2L;
	if (N<=4L		) return 4L;
	if (N<=8L		) return 8L;
	if (N<=16L	    ) return 16L;
	if (N<=32L	    ) return 32L;
	if (N<=64L 	    ) return 64L;
	if (N<=128L     ) return 128L;
	if (N<=256L     ) return 256L;
	if (N<=512L     ) return 512L;
	if (N<=1024L    ) return 1024L;
	if (N<=2048L    ) return 2048L;
	if (N<=4096L    ) return 4096L;
	if (N<=8192L    ) return 8192L;
	if (N<=16384L   ) return 16384L;
	if (N<=32768L   ) return 32768L;
	if (N<=65536L   ) return 65536L;
	else			  return 0;
}

inline UBOOL FIsPowerOfTwo( DWORD N )
{
	return !(N & (N - 1));
}

//
// Add to a word angle, constraining it within a min (not to cross)
// and a max (not to cross).  Accounts for funkyness of word angles.
// Assumes that angle is initially in the desired range.
//
inline _WORD FAddAngleConfined( INT Angle, INT Delta, INT MinThresh, INT MaxThresh )
{
	if( Delta < 0 )
	{
		if ( Delta<=-0x10000L || Delta<=-(INT)((_WORD)(Angle-MinThresh)))
			return MinThresh;
	}
	else if( Delta > 0 )
	{
		if( Delta>=0x10000L || Delta>=(INT)((_WORD)(MaxThresh-Angle)))
			return MaxThresh;
	}
	return (_WORD)(Angle+Delta);
}

//
// Eliminate all fractional precision from an angle.
//
INT ReduceAngle( INT Angle );

//
// Fast 32-bit float evaluations. 
// Warning: likely not portable, and useful on Pentium class processors only.
//
inline UBOOL IsNegativeFloat(const float& F1)
{
	return ( (*(DWORD*)&F1) >= (DWORD)0x80000000 ); // Detects sign bit.
}

// Clamp any float F0 between zero and positive float Range
#define ClipFloatFromZero(F0,Range)\
{\
	if ( (*(DWORD*)&F0) >= (DWORD)0x80000000) F0 = 0.f;\
	else if	( (*(DWORD*)&F0) > (*(DWORD*)&Range)) F0 = Range;\
}

/*-----------------------------------------------------------------------------
FIntPoint.
-----------------------------------------------------------------------------*/

struct FIntPoint
{
	INT X, Y;
	FIntPoint()
	{}
	FIntPoint( INT InX, INT InY )
		:	X( InX )
		,	Y( InY )
	{}
	static FIntPoint ZeroValue()
	{
		return FIntPoint(0,0);
	}
	static FIntPoint NoneValue()
	{
		return FIntPoint(INDEX_NONE,INDEX_NONE);
	}
	const INT& operator()( INT i ) const
	{
		return (&X)[i];
	}
	INT& operator()( INT i )
	{
		return (&X)[i];
	}
	static INT Num()
	{
		return 2;
	}
	UBOOL operator==( const FIntPoint& Other ) const
	{
		return X==Other.X && Y==Other.Y;
	}
	UBOOL operator!=( const FIntPoint& Other ) const
	{
		return X!=Other.X || Y!=Other.Y;
	}
	FIntPoint& operator+=( const FIntPoint& Other )
	{
		X += Other.X;
		Y += Other.Y;
		return *this;
	}
	FIntPoint& operator-=( const FIntPoint& Other )
	{
		X -= Other.X;
		Y -= Other.Y;
		return *this;
	}
	FIntPoint operator+( const FIntPoint& Other ) const
	{
		return FIntPoint(*this) += Other;
	}
	FIntPoint operator-( const FIntPoint& Other ) const
	{
		return FIntPoint(*this) -= Other;
	}
	INT Size() const
	{
		return INT( appSqrt( FLOAT(X*X + Y*Y) ) );
	}
};

/*-----------------------------------------------------------------------------
FIntRect.
-----------------------------------------------------------------------------*/

struct FIntRect
{
	FIntPoint Min, Max;
	FIntRect()
	{}
	FIntRect( INT X0, INT Y0, INT X1, INT Y1 )
		:	Min( X0, Y0 )
		,	Max( X1, Y1 )
	{}
	FIntRect( FIntPoint InMin, FIntPoint InMax )
		:	Min( InMin )
		,	Max( InMax )
	{}
	const FIntPoint& operator()( INT i ) const
	{
		return (&Min)[i];
	}
	FIntPoint& operator()( INT i )
	{
		return (&Min)[i];
	}
	static INT Num()
	{
		return 2;
	}
	UBOOL operator==( const FIntRect& Other ) const
	{
		return Min==Other.Min && Max==Other.Max;
	}
	UBOOL operator!=( const FIntRect& Other ) const
	{
		return Min!=Other.Min || Max!=Other.Max;
	}
	FIntRect Right( INT Width )
	{
		return FIntRect( ::Max(Min.X,Max.X-Width), Min.Y, Max.X, Max.Y );
	}
	FIntRect Bottom( INT Height )
	{
		return FIntRect( Min.X, ::Max(Min.Y,Max.Y-Height), Max.X, Max.Y );
	}
	FIntPoint Size()
	{
		return FIntPoint( Max.X-Min.X, Max.Y-Min.Y );
	}
	INT Width()
	{
		return Max.X-Min.X;
	}
	INT Height()
	{
		return Max.Y-Min.Y;
	}
	FIntRect& operator+=( const FIntPoint& P )
	{
		Min += P;
		Max += P;
		return *this;
	}
	FIntRect& operator-=( const FIntPoint& P )
	{
		Min -= P;
		Max -= P;
		return *this;
	}
	FIntRect operator+( const FIntPoint& P ) const
	{
		return FIntRect( Min+P, Max+P );
	}
	FIntRect operator-( const FIntPoint& P ) const
	{
		return FIntRect( Min-P, Max-P );
	}
	FIntRect operator+( const FIntRect& R ) const
	{
		return FIntRect( Min+R.Min, Max+R.Max );
	}
	FIntRect operator-( const FIntRect& R ) const
	{
		return FIntRect( Min-R.Min, Max-R.Max );
	}
	FIntRect Inner( FIntPoint P ) const
	{
		return FIntRect( Min+P, Max-P );
	}
	UBOOL Contains( FIntPoint P ) const
	{
		return P.X>=Min.X && P.X<Max.X && P.Y>=Min.Y && P.Y<Max.Y;
	}
	INT Area() const
	{
		return (Max.X - Min.X) * (Max.Y - Min.Y);
	}
	void GetCenterAndExtents(FIntPoint& Center, FIntPoint& Extent) const
	{
		Extent.X = 0.5f*(Max.X - Min.X);
		Extent.Y = 0.5f*(Max.Y - Min.Y);

		Center.X = Min.X + Extent.X;
		Center.Y = Min.Y + Extent.Y;
	}
};

//
//	FVector2D
//
struct FVector2D 
{
	FLOAT	X,
			Y;

	// Constructors.
	FORCEINLINE FVector2D()
	{}
	FORCEINLINE FVector2D(FLOAT InX,FLOAT InY)
	:	X(InX), Y(InY)
	{}
	FORCEINLINE FVector2D(FIntPoint InPos)
	{
		X = (FLOAT)InPos.X;
		Y = (FLOAT)InPos.Y;
	}

	// Binary math operators.
	friend FVector2D operator*( FLOAT Scale, const FVector2D& V )
	{
		return FVector2D( V.X * Scale, V.Y * Scale );
	}
	FORCEINLINE FVector2D operator+( const FVector2D& V ) const
	{
		return FVector2D( X + V.X, Y + V.Y );
	}
	FORCEINLINE FVector2D operator-( const FVector2D& V ) const
	{
		return FVector2D( X - V.X, Y - V.Y );
	}
	FORCEINLINE FVector2D operator*( FLOAT Scale ) const
	{
		return FVector2D( X * Scale, Y * Scale );
	}
	FVector2D operator/( FLOAT Scale ) const
	{
		FLOAT RScale = 1.f/Scale;
		return FVector2D( X * RScale, Y * RScale );
	}
	FORCEINLINE FVector2D operator*( const FVector2D& V ) const
	{
		return FVector2D( X * V.X, Y * V.Y );
	}

	// Binary comparison operators.
	UBOOL operator==( const FVector2D& V ) const
	{
		return X==V.X && Y==V.Y;
	}
	UBOOL operator!=( const FVector2D& V ) const
	{
		return X!=V.X || Y!=V.Y;
	}

	// Unary operators.
	FORCEINLINE FVector2D operator-() const
	{
		return FVector2D( -X, -Y );
	}

	// Assignment operators.
	FORCEINLINE FVector2D operator+=( const FVector2D& V )
	{
		X += V.X; Y += V.Y;
		return *this;
	}
	FORCEINLINE FVector2D operator-=( const FVector2D& V )
	{
		X -= V.X; Y -= V.Y;
		return *this;
	}
	FORCEINLINE FVector2D operator*=( FLOAT Scale )
	{
		X *= Scale; Y *= Scale;
		return *this;
	}
	FVector2D operator/=( FLOAT V )
	{
		FLOAT RV = 1.f/V;
		X *= RV; Y *= RV;
		return *this;
	}
	FVector2D operator*=( const FVector2D& V )
	{
		X *= V.X; Y *= V.Y;
		return *this;
	}
	FVector2D operator/=( const FVector2D& V )
	{
		X /= V.X; Y /= V.Y;
		return *this;
	}
    FLOAT& operator[]( INT i )
	{
		check(i>-1);
		check(i<2);
		if( i == 0 )	return X;
		else			return Y;
	}

	// Simple functions.
	FLOAT GetMax() const
	{
		return Max(X,Y);
	}
	FLOAT GetAbsMax() const
	{
		return Max(Abs(X),Abs(Y));
	}
	FLOAT GetMin() const
	{
		return Min(X,Y);
	}
	FLOAT Size() const
	{
		return appSqrt( X*X + Y*Y );
	}
	FLOAT SizeSquared() const
	{
		return X*X + Y*Y;
	}
	FVector2D Normalize() const
	{	
		FLOAT Mag = Size();
		if(Mag < SMALL_NUMBER)
		{
			return FVector2D(0.f,0.f);
		}
		else
		{
			FLOAT RecipMag = 1.f/Mag;
			return FVector2D(X*RecipMag, Y*RecipMag);
		}
	}
	int IsNearlyZero() const
	{
		return	Abs(X)<KINDA_SMALL_NUMBER
			&&	Abs(Y)<KINDA_SMALL_NUMBER;
	}
	UBOOL IsZero() const
	{
		return X==0.f && Y==0.f;
	}
	FLOAT& Component( INT Index )
	{
		return (&X)[Index];
	}

	FIntPoint IntPoint() const
	{
		return FIntPoint( appRound(X), appRound(Y) );
	}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FVector2D& V )
	{
		return Ar << V.X << V.Y;
	}
};

/*-----------------------------------------------------------------------------
	FVector.
-----------------------------------------------------------------------------*/

// Information associated with a floating point vector, describing its
// status as a point in a rendering context.
enum EVectorFlags
{
	FVF_OutXMin		= 0x04,	// Outcode rejection, off left hand side of screen.
	FVF_OutXMax		= 0x08,	// Outcode rejection, off right hand side of screen.
	FVF_OutYMin		= 0x10,	// Outcode rejection, off top of screen.
	FVF_OutYMax		= 0x20,	// Outcode rejection, off bottom of screen.
	FVF_OutNear     = 0x40, // Near clipping plane.
	FVF_OutFar      = 0x80, // Far clipping plane.
	FVF_OutReject   = (FVF_OutXMin | FVF_OutXMax | FVF_OutYMin | FVF_OutYMax), // Outcode rejectable.
	FVF_OutSkip		= (FVF_OutXMin | FVF_OutXMax | FVF_OutYMin | FVF_OutYMax), // Outcode clippable.
};

//
// Floating point vector.
//

class FVector 
{
public:
	// Variables.
	FLOAT X,Y,Z;

	// Constructors.
	FORCEINLINE FVector()
	{}
	FORCEINLINE FVector(FLOAT InF)
	: X(InF), Y(InF), Z(InF)
	{}
	FORCEINLINE FVector( FLOAT InX, FLOAT InY, FLOAT InZ )
	:	X(InX), Y(InY), Z(InZ)
	{}
	FVector( FIntPoint A )
	{
		X = A.X - 0.5f;
		Y = A.Y - 0.5f;
		Z = 0.0f;
	}

	// Binary math operators.
	FORCEINLINE FVector operator^( const FVector& V ) const
	{
		return FVector
		(
			Y * V.Z - Z * V.Y,
			Z * V.X - X * V.Z,
			X * V.Y - Y * V.X
		);
	}
	FORCEINLINE FLOAT operator|( const FVector& V ) const
	{
		return X*V.X + Y*V.Y + Z*V.Z;
	}
	friend FVector operator*( FLOAT Scale, const FVector& V )
	{
		return FVector( V.X * Scale, V.Y * Scale, V.Z * Scale );
	}
	FORCEINLINE FVector operator+( const FVector& V ) const
	{
		return FVector( X + V.X, Y + V.Y, Z + V.Z );
	}
	FORCEINLINE FVector operator-( const FVector& V ) const
	{
		return FVector( X - V.X, Y - V.Y, Z - V.Z );
	}
	FORCEINLINE FVector operator*( FLOAT Scale ) const
	{
		return FVector( X * Scale, Y * Scale, Z * Scale );
	}
	FVector operator/( FLOAT Scale ) const
	{
		FLOAT RScale = 1.f/Scale;
		return FVector( X * RScale, Y * RScale, Z * RScale );
	}
	FORCEINLINE FVector operator*( const FVector& V ) const
	{
		return FVector( X * V.X, Y * V.Y, Z * V.Z );
	}

	// Binary comparison operators.
	UBOOL operator==( const FVector& V ) const
	{
		return X==V.X && Y==V.Y && Z==V.Z;
	}
	UBOOL operator!=( const FVector& V ) const
	{
		return X!=V.X || Y!=V.Y || Z!=V.Z;
	}

	// Unary operators.
	FORCEINLINE FVector operator-() const
	{
		return FVector( -X, -Y, -Z );
	}

	// Assignment operators.
	FORCEINLINE FVector operator+=( const FVector& V )
	{
		X += V.X; Y += V.Y; Z += V.Z;
		return *this;
	}
	FORCEINLINE FVector operator-=( const FVector& V )
	{
		X -= V.X; Y -= V.Y; Z -= V.Z;
		return *this;
	}
	FORCEINLINE FVector operator*=( FLOAT Scale )
	{
		X *= Scale; Y *= Scale; Z *= Scale;
		return *this;
	}
	FVector operator/=( FLOAT V )
	{
		FLOAT RV = 1.f/V;
		X *= RV; Y *= RV; Z *= RV;
		return *this;
	}
	FVector operator*=( const FVector& V )
	{
		X *= V.X; Y *= V.Y; Z *= V.Z;
		return *this;
	}
	FVector operator/=( const FVector& V )
	{
		X /= V.X; Y /= V.Y; Z /= V.Z;
		return *this;
	}
    FLOAT& operator[]( INT i )
	{
		check(i>-1);
		check(i<3);
		if( i == 0 )		return X;
		else if( i == 1)	return Y;
		else				return Z;
	}

	// Simple functions.
	FLOAT GetMax() const
	{
		return Max(Max(X,Y),Z);
	}
	FLOAT GetAbsMax() const
	{
		return Max(Max(Abs(X),Abs(Y)),Abs(Z));
	}
	FLOAT GetMin() const
	{
		return Min(Min(X,Y),Z);
	}
	FLOAT Size() const
	{
		return appSqrt( X*X + Y*Y + Z*Z );
	}
	FLOAT SizeSquared() const
	{
		return X*X + Y*Y + Z*Z;
	}
	FLOAT Size2D() const 
	{
		return appSqrt( X*X + Y*Y );
	}
	FLOAT SizeSquared2D() const 
	{
		return X*X + Y*Y;
	}
	int IsNearlyZero() const
	{
		return
				Abs(X)<KINDA_SMALL_NUMBER
			&&	Abs(Y)<KINDA_SMALL_NUMBER
			&&	Abs(Z)<KINDA_SMALL_NUMBER;
	}
	UBOOL IsZero() const
	{
		return X==0.f && Y==0.f && Z==0.f;
	}
	UBOOL Normalize()
	{
		FLOAT SquareSum = X*X+Y*Y+Z*Z;
		if( SquareSum >= SMALL_NUMBER )
		{
			FLOAT Scale = appInvSqrt(SquareSum);
			X *= Scale; Y *= Scale; Z *= Scale;
			return 1;
		}
		else return 0;
	}
	FVector Projection() const
	{
		FLOAT RZ = 1.f/Z;
		return FVector( X*RZ, Y*RZ, 1 );
	}
	FORCEINLINE FVector UnsafeNormal() const
	{
		FLOAT Scale = appInvSqrt(X*X+Y*Y+Z*Z);
		return FVector( X*Scale, Y*Scale, Z*Scale );
	}
	FVector GridSnap( const FLOAT& GridSz )
	{
		return FVector( FSnap(X, GridSz),FSnap(Y, GridSz),FSnap(Z, GridSz) );
	}
	FVector BoundToCube( FLOAT Radius )
	{
		return FVector
		(
			Clamp(X,-Radius,Radius),
			Clamp(Y,-Radius,Radius),
			Clamp(Z,-Radius,Radius)
		);
	}
	void AddBounded( const FVector& V, FLOAT Radius=MAXSWORD )
	{
		*this = (*this + V).BoundToCube(Radius);
	}
	FLOAT& Component( INT Index )
	{
		return (&X)[Index];
	}

	// Return a boolean that is based on the vector's direction.
	// When      V==(0,0,0) Booleanize(0)=1.
	// Otherwise Booleanize(V) <-> !Booleanize(!B).
	UBOOL Booleanize()
	{
		return
			X >  0.f ? 1 :
			X <  0.f ? 0 :
			Y >  0.f ? 1 :
			Y <  0.f ? 0 :
			Z >= 0.f ? 1 : 0;
	}

	// See if X == Y == Z (within fairly small tolerance)
	UBOOL IsUniform() const
	{
		return (Abs(X-Y) < KINDA_SMALL_NUMBER) && (Abs(Y-Z) < KINDA_SMALL_NUMBER);
	}

	// Mirror a vector about a normal vector.
	FVector MirrorByVector( const FVector& MirrorNormal ) const
	{
		return *this - MirrorNormal * (2.f * (*this | MirrorNormal));
	}

	// Mirror a vector about a plane
	FVector MirrorByPlane( const FPlane& Plane ) const;

	// Rotate around Axis (assumes Axis.Size() == 1)
	FVector RotateAngleAxis( const INT Angle, const FVector& Axis ) const;

	FORCEINLINE FVector SafeNormal() const
	{
		FLOAT SquareSum = X*X + Y*Y + Z*Z;
		if( SquareSum < SMALL_NUMBER )
			return FVector( 0.f, 0.f, 0.f );

		FLOAT Scale = appInvSqrt(SquareSum);
		return FVector( X*Scale, Y*Scale, Z*Scale );
	}

	FORCEINLINE FVector ProjectOnTo( const FVector& A ) const 
	{ 
		return (((*this | A) / (A | A)) * A); 
	}

	// Complicated functions.
	FRotator Rotation();
	void FindBestAxisVectors( FVector& Axis1, FVector& Axis2 ) const;

	// Friends.
	friend FLOAT FDist( const FVector& V1, const FVector& V2 );
	friend FLOAT FDistSquared( const FVector& V1, const FVector& V2 );
	friend UBOOL FPointsAreSame( const FVector& P, const FVector& Q );
	friend UBOOL FPointsAreNear( const FVector& Point1, const FVector& Point2, FLOAT Dist);
	friend FLOAT FPointPlaneDist( const FVector& Point, const FVector& PlaneBase, const FVector& PlaneNormal );
	friend FVector FLinePlaneIntersection( const FVector& Point1, const FVector& Point2, const FVector& PlaneOrigin, const FVector& PlaneNormal );
	friend FVector FLinePlaneIntersection( const FVector& Point1, const FVector& Point2, const FPlane& Plane );
	friend UBOOL FParallel( const FVector& Normal1, const FVector& Normal2 );
	friend UBOOL FCoplanar( const FVector& Base1, const FVector& Normal1, const FVector& Base2, const FVector& Normal2 );

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FVector& V )
	{
		return Ar << V.X << V.Y << V.Z;
	}
};


//
// Compressed vector.
//

#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,1)
#endif
class CVector
{
public:
	SWORD X GCC_PACK(1);
	SWORD Y GCC_PACK(1);
	SWORD Z GCC_PACK(1);

	FORCEINLINE void Compress( FVector& Src, FVector& InvScale )
	{
		X = (SWORD) appRound((Src.X * InvScale.X));
		Y = (SWORD) appRound((Src.Y * InvScale.Y));
		Z = (SWORD) appRound((Src.Z * InvScale.Z));
	}

	FORCEINLINE void Expand( FVector& Dst, FVector& Scale )
	{
		Dst.X = X * Scale.X;
		Dst.Y = Y * Scale.Y;
		Dst.Z = Z * Scale.Z;
	}

	friend FArchive& operator<<(FArchive& Ar, CVector& T)
	{
		return Ar << T.X << T.Y << T.Z;
	}
};
#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

/*-----------------------------------------------------------------------------
	FEdge.
-----------------------------------------------------------------------------*/

class FEdge
{
public:
	// Constructors.
	FEdge()
	{}
	FEdge( FVector v1, FVector v2)
	{
		Vertex[0] = v1;
		Vertex[1] = v2;
	}

	FVector Vertex[2];

	UBOOL operator==( const FEdge& E ) const
	{
		return ( (E.Vertex[0] == Vertex[0] && E.Vertex[1] == Vertex[1]) 
			|| (E.Vertex[0] == Vertex[1] && E.Vertex[1] == Vertex[0]) );
	}
};

/*-----------------------------------------------------------------------------
	FPlane.
-----------------------------------------------------------------------------*/

class FPlane : public FVector
{
public:
	// Variables.
	FLOAT W;

	// Constructors.
	FORCEINLINE FPlane()
	{}
	FORCEINLINE FPlane( const FPlane& P )
	:	FVector(P)
	,	W(P.W)
	{}
	FORCEINLINE FPlane( const FVector& V )
	:	FVector(V)
	,	W(0)
	{}
	FORCEINLINE FPlane( FLOAT InX, FLOAT InY, FLOAT InZ, FLOAT InW )
	:	FVector(InX,InY,InZ)
	,	W(InW)
	{}
	FORCEINLINE FPlane( FVector InNormal, FLOAT InW )
	:	FVector(InNormal), W(InW)
	{}
	FORCEINLINE FPlane( FVector InBase, const FVector &InNormal )
	:	FVector(InNormal)
	,	W(InBase | InNormal)
	{}
	FPlane( FVector A, FVector B, FVector C )
	:	FVector( ((B-A)^(C-A)).SafeNormal() )
	,	W( A | ((B-A)^(C-A)).SafeNormal() )
	{}

	// Functions.
	FORCEINLINE FLOAT PlaneDot( const FVector &P ) const
	{
		return X*P.X + Y*P.Y + Z*P.Z - W;
	}
	FPlane Flip() const
	{
		return FPlane(-X,-Y,-Z,-W);
	}
	FPlane TransformPlaneByOrtho( const FMatrix& M ) const;
	FPlane TransformBy( const FMatrix& M ) const;
	FPlane TransformByUsingAdjointT( const FMatrix& M, FLOAT DetM, const FMatrix& TA ) const;
	UBOOL operator==( const FPlane& V ) const
	{
		return X==V.X && Y==V.Y && Z==V.Z && W==V.W;
	}
	UBOOL operator!=( const FPlane& V ) const
	{
		return X!=V.X || Y!=V.Y || Z!=V.Z || W!=V.W;
	}
	FORCEINLINE FLOAT operator|( const FPlane& V ) const
	{
		return X*V.X + Y*V.Y + Z*V.Z + W*V.W;
	}
	FPlane operator+( const FPlane& V ) const
	{
		return FPlane( X + V.X, Y + V.Y, Z + V.Z, W + V.W );
	}
	FPlane operator-( const FPlane& V ) const
	{
		return FPlane( X - V.X, Y - V.Y, Z - V.Z, W - V.W );
	}
	FPlane operator/( FLOAT Scale ) const
	{
		FLOAT RScale = 1.f/Scale;
		return FPlane( X * RScale, Y * RScale, Z * RScale, W * RScale );
	}
	FPlane operator*( FLOAT Scale ) const
	{
		return FPlane( X * Scale, Y * Scale, Z * Scale, W * Scale );
	}
	FPlane operator*( const FPlane& V )
	{
		return FPlane ( X*V.X,Y*V.Y,Z*V.Z,W*V.W );
	}
	FPlane operator+=( const FPlane& V )
	{
		X += V.X; Y += V.Y; Z += V.Z; W += V.W;
		return *this;
	}
	FPlane operator-=( const FPlane& V )
	{
		X -= V.X; Y -= V.Y; Z -= V.Z; W -= V.W;
		return *this;
	}
	FPlane operator*=( FLOAT Scale )
	{
		X *= Scale; Y *= Scale; Z *= Scale; W *= Scale;
		return *this;
	}
	FPlane operator*=( const FPlane& V )
	{
		X *= V.X; Y *= V.Y; Z *= V.Z; W *= V.W;
		return *this;
	}
	FPlane operator/=( FLOAT V )
	{
		FLOAT RV = 1.f/V;
		X *= RV; Y *= RV; Z *= RV; W *= RV;
		return *this;
	}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FPlane &P )
	{
		return Ar << (FVector&)P << P.W;
	}
};


/*-----------------------------------------------------------------------------
	FSphere.
-----------------------------------------------------------------------------*/

class FSphere : public FPlane
{
public:
	// Constructors.
	FSphere()
	{}
	FSphere( INT )
	:	FPlane(0,0,0,0)
	{}
	FSphere( FVector V, FLOAT W )
	:	FPlane( V, W )
	{}
	FSphere( const FVector* Pts, INT Count );

	FSphere TransformBy(const FMatrix& M) const;

	friend FArchive& operator<<( FArchive& Ar, FSphere& S )
	{
		Ar << (FPlane&)S;
		return Ar;
	}
};

/*-----------------------------------------------------------------------------
	FScale.
-----------------------------------------------------------------------------*/

// An axis along which sheering is performed.
enum ESheerAxis
{
	SHEER_None = 0,
	SHEER_XY   = 1,
	SHEER_XZ   = 2,
	SHEER_YX   = 3,
	SHEER_YZ   = 4,
	SHEER_ZX   = 5,
	SHEER_ZY   = 6,
};

//
// Scaling and sheering info associated with a brush.  This is 
// easily-manipulated information which is built into a transformation
// matrix later.
//
class FScale 
{
public:
	// Variables.
	FVector		Scale;
	FLOAT		SheerRate;
	BYTE		SheerAxis; // From ESheerAxis

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FScale &S )
	{
		return Ar << S.Scale << S.SheerRate << S.SheerAxis;
	}

	// Constructors.
	FScale() {}
	FScale( const FVector &InScale, FLOAT InSheerRate, ESheerAxis InSheerAxis )
	:	Scale(InScale), SheerRate(InSheerRate), SheerAxis(InSheerAxis) {}

	// Operators.
	UBOOL operator==( const FScale &S ) const
	{
		return Scale==S.Scale && SheerRate==S.SheerRate && SheerAxis==S.SheerAxis;
	}

	// Functions.
	FLOAT  Orientation()
	{
		return Sgn(Scale.X * Scale.Y * Scale.Z);
	}
};

/*-----------------------------------------------------------------------------
	FRotator.
-----------------------------------------------------------------------------*/

//
// Rotation.
//
class FRotator
{
public:
	// Variables.
	INT Pitch; // Looking up and down (0=Straight Ahead, +Up, -Down).
	INT Yaw;   // Rotating around (running in circles), 0=East, +North, -South.
	INT Roll;  // Rotation about axis of screen, 0=Straight, +Clockwise, -CCW.

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FRotator& R )
	{
		return Ar << R.Pitch << R.Yaw << R.Roll;
	}

	// Constructors.
	FRotator() {}
	FRotator( INT InPitch, INT InYaw, INT InRoll )
	:	Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}

	FRotator( const FQuat& Quat );

	// Binary arithmetic operators.
	FRotator operator+( const FRotator &R ) const
	{
		return FRotator( Pitch+R.Pitch, Yaw+R.Yaw, Roll+R.Roll );
	}
	FRotator operator-( const FRotator &R ) const
	{
		return FRotator( Pitch-R.Pitch, Yaw-R.Yaw, Roll-R.Roll );
	}
	FRotator operator*( FLOAT Scale ) const
	{
		return FRotator( (INT)(Pitch*Scale), (INT)(Yaw*Scale), (INT)(Roll*Scale) );
	}
	friend FRotator operator*( FLOAT Scale, const FRotator &R )
	{
		return FRotator( (INT)(R.Pitch*Scale), (INT)(R.Yaw*Scale), (INT)(R.Roll*Scale) );
	}
	FRotator operator*= (FLOAT Scale)
	{
		Pitch = (INT)(Pitch*Scale); Yaw = (INT)(Yaw*Scale); Roll = (INT)(Roll*Scale);
		return *this;
	}
	// Binary comparison operators.
	UBOOL operator==( const FRotator &R ) const
	{
		return Pitch==R.Pitch && Yaw==R.Yaw && Roll==R.Roll;
	}
	UBOOL operator!=( const FRotator &V ) const
	{
		return Pitch!=V.Pitch || Yaw!=V.Yaw || Roll!=V.Roll;
	}
	// Assignment operators.
	FRotator operator+=( const FRotator &R )
	{
		Pitch += R.Pitch; Yaw += R.Yaw; Roll += R.Roll;
		return *this;
	}
	FRotator operator-=( const FRotator &R )
	{
		Pitch -= R.Pitch; Yaw -= R.Yaw; Roll -= R.Roll;
		return *this;
	}
	// Functions.
	FRotator Reduce() const
	{
		return FRotator( ReduceAngle(Pitch), ReduceAngle(Yaw), ReduceAngle(Roll) );
	}
	int IsZero() const
	{
		return ((Pitch&65535)==0) && ((Yaw&65535)==0) && ((Roll&65535)==0);
	}
	FRotator Add( INT DeltaPitch, INT DeltaYaw, INT DeltaRoll )
	{
		Yaw   += DeltaYaw;
		Pitch += DeltaPitch;
		Roll  += DeltaRoll;
		return *this;
	}
	FRotator AddBounded( INT DeltaPitch, INT DeltaYaw, INT DeltaRoll )
	{
		Yaw  += DeltaYaw;
		Pitch = FAddAngleConfined(Pitch,DeltaPitch,192*0x100,64*0x100);
		Roll  = FAddAngleConfined(Roll, DeltaRoll, 192*0x100,64*0x100);
		return *this;
	}
	FRotator GridSnap( const FRotator &RotGrid )
	{
		return FRotator
		(
			(INT)(FSnap(Pitch,RotGrid.Pitch)),
			(INT)(FSnap(Yaw,  RotGrid.Yaw)),
			(INT)(FSnap(Roll, RotGrid.Roll))
		);
	}
	FVector Vector();
	FQuat Quaternion();
	FVector Euler() const;

	static FRotator MakeFromEuler(const FVector& Euler);

	// Resets the rotation values so they fall within the range -65535,65535
	FRotator Clamp()
	{
		return FRotator( Pitch%65535, Yaw%65535, Roll%65535 );
	}
	FRotator ClampPos()
	{
		return FRotator( Abs(Pitch)%65535, Abs(Yaw)%65535, Abs(Roll)%65535 );
	}

	/** Normalize stuff from execNormalize so we can use it in C++ */
	FRotator Normalize()
	{
		FRotator Rot = *this;
		Rot.Pitch = Rot.Pitch & 0xFFFF; if( Rot.Pitch > 32767 ) Rot.Pitch -= 0x10000;
		Rot.Roll  = Rot.Roll  & 0xFFFF; if( Rot.Roll  > 32767 )	Rot.Roll  -= 0x10000;
		Rot.Yaw   = Rot.Yaw   & 0xFFFF; if( Rot.Yaw   > 32767 )	Rot.Yaw   -= 0x10000;
		return Rot;
	}

	FRotator Denormalize()
	{
		FRotator Rot = *this;
		Rot.Pitch	= Rot.Pitch & 0xFFFF;
		Rot.Yaw		= Rot.Yaw & 0xFFFF;
		Rot.Roll	= Rot.Roll & 0xFFFF;
		return Rot;
	}

	void MakeShortestRoute();
	void GetWindingAndRemainder(FRotator& Winding, FRotator& Remainder) const;
};

/*-----------------------------------------------------------------------------
	Bounds.
-----------------------------------------------------------------------------*/

//
// A rectangular minimum bounding volume.
//
class FBox
{
public:
	// Variables.
	FVector Min;
	FVector Max;
	BYTE IsValid;

	// Constructors.
	FBox() {}
	FBox(INT) { Init(); }
	FBox( const FVector& InMin, const FVector& InMax ) : Min(InMin), Max(InMax), IsValid(1) {}
	FBox( const FVector* Points, INT Count );

	// Accessors.
	FVector& GetExtrema( int i )
	{
		return (&Min)[i];
	}
	const FVector& GetExtrema( int i ) const
	{
		return (&Min)[i];
	}

	// Functions.
	void Init()
	{
		Min = Max = FVector(0,0,0);
		IsValid = 0;
	}
	FORCEINLINE FBox& operator+=( const FVector &Other )
	{
		if( IsValid )
		{
#if ASM
			__asm
			{
				mov		eax,[Other]
				mov		ecx,[this]
				
				movss	xmm3,[eax]FVector.X
				movss	xmm4,[eax]FVector.Y
				movss	xmm5,[eax]FVector.Z

				movss	xmm0,[ecx]FBox.Min.X
				movss	xmm1,[ecx]FBox.Min.Y
				movss	xmm2,[ecx]FBox.Min.Z
				minss	xmm0,xmm3
				minss	xmm1,xmm4
				minss	xmm2,xmm5
				movss	[ecx]FBox.Min.X,xmm0
				movss	[ecx]FBox.Min.Y,xmm1
				movss	[ecx]FBox.Min.Z,xmm2

				movss	xmm0,[ecx]FBox.Max.X
				movss	xmm1,[ecx]FBox.Max.Y
				movss	xmm2,[ecx]FBox.Max.Z
				maxss	xmm0,xmm3
				maxss	xmm1,xmm4
				maxss	xmm2,xmm5
				movss	[ecx]FBox.Max.X,xmm0
				movss	[ecx]FBox.Max.Y,xmm1
				movss	[ecx]FBox.Max.Z,xmm2
			}
#else
			Min.X = ::Min( Min.X, Other.X );
			Min.Y = ::Min( Min.Y, Other.Y );
			Min.Z = ::Min( Min.Z, Other.Z );

			Max.X = ::Max( Max.X, Other.X );
			Max.Y = ::Max( Max.Y, Other.Y );
			Max.Z = ::Max( Max.Z, Other.Z );
#endif
		}
		else
		{
			Min = Max = Other;
			IsValid = 1;
		}
		return *this;
	}
	FBox operator+( const FVector& Other ) const
	{
		return FBox(*this) += Other;
	}
	FBox& operator+=( const FBox& Other )
	{
		if( IsValid && Other.IsValid )
		{
#if ASM
			__asm
			{
				mov		eax,[Other]
				mov		ecx,[this]

				movss	xmm0,[ecx]FBox.Min.X
				movss	xmm1,[ecx]FBox.Min.Y
				movss	xmm2,[ecx]FBox.Min.Z
				minss	xmm0,[eax]FBox.Min.X
				minss	xmm1,[eax]FBox.Min.Y
				minss	xmm2,[eax]FBox.Min.Z
				movss	[ecx]FBox.Min.X,xmm0
				movss	[ecx]FBox.Min.Y,xmm1
				movss	[ecx]FBox.Min.Z,xmm2

				movss	xmm0,[ecx]FBox.Max.X
				movss	xmm1,[ecx]FBox.Max.Y
				movss	xmm2,[ecx]FBox.Max.Z
				maxss	xmm0,[eax]FBox.Max.X
				maxss	xmm1,[eax]FBox.Max.Y
				maxss	xmm2,[eax]FBox.Max.Z
				movss	[ecx]FBox.Max.X,xmm0
				movss	[ecx]FBox.Max.Y,xmm1
				movss	[ecx]FBox.Max.Z,xmm2
			}
#else
			Min.X = ::Min( Min.X, Other.Min.X );
			Min.Y = ::Min( Min.Y, Other.Min.Y );
			Min.Z = ::Min( Min.Z, Other.Min.Z );

			Max.X = ::Max( Max.X, Other.Max.X );
			Max.Y = ::Max( Max.Y, Other.Max.Y );
			Max.Z = ::Max( Max.Z, Other.Max.Z );
#endif
		}
		else *this = Other;
		return *this;
	}
	FBox operator+( const FBox& Other ) const
	{
		return FBox(*this) += Other;
	}
    FVector& operator[]( INT i )
	{
		check(i>-1);
		check(i<2);
		if( i == 0 )		return Min;
		else				return Max;
	}
	FBox TransformBy( const FMatrix& M ) const;
	FBox ExpandBy( FLOAT W ) const
	{
		return FBox( Min - FVector(W,W,W), Max + FVector(W,W,W) );
	}
	// Returns the midpoint between the min and max points.
	FVector GetCenter() const
	{
		return FVector( ( Min + Max ) * 0.5f );
	}
	// Returns the extent around the center
	FVector GetExtent() const
	{
		return 0.5f*(Max - Min);
	}

	void GetCenterAndExtents( FVector & center, FVector & Extents ) const
	{
		Extents = GetExtent();
		center = Min + Extents;
	}

	bool Intersect( const FBox & other ) const
	{
		if( Min.X > other.Max.X || other.Min.X > Max.X )
			return false;
		if( Min.Y > other.Max.Y || other.Min.Y > Max.Y )
			return false;
		if( Min.Z > other.Max.Z || other.Min.Z > Max.Z )
			return false;
		return true;
	}
	// Checks to see if the location is inside this box
	UBOOL IsInside( FVector& In )
	{
		return ( In.X > Min.X && In.X < Max.X
				&& In.Y > Min.Y && In.Y < Max.Y 
				&& In.Z > Min.Z && In.Z < Max.Z );
	}


	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FBox& Bound )
	{
		return Ar << Bound.Min << Bound.Max << Bound.IsValid;
	}
};

#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,1)
#endif
class CBox
{
public:
	CVector Min GCC_PACK(1);
	CVector Max GCC_PACK(1);

	FORCEINLINE void Compress( FBox& Src, FVector& InvScale )
	{
		Min.Compress( Src.Min, InvScale );
		Max.Compress( Src.Max, InvScale );
	}

	FORCEINLINE void Expand( FBox& Dst, FVector& Scale )
	{
		Min.Expand( Dst.Min, Scale );
		Max.Expand( Dst.Max, Scale );
		Dst.IsValid = 1;
	}

	friend FArchive& operator<<(FArchive& Ar, CBox& T)
	{
		return Ar << T.Min << T.Max;
	}
};
#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

//
//	FBoxSphereBounds - An axis aligned bounding box and bounding sphere with the same origin. (28 bytes)
//

struct FBoxSphereBounds
{
	FVector	Origin,
			BoxExtent;
	FLOAT	SphereRadius;

	// Constructor.

	FBoxSphereBounds() {}
	FBoxSphereBounds(const FVector& InOrigin,const FVector& InBoxExtent,FLOAT InSphereRadius):
		Origin(InOrigin),
		BoxExtent(InBoxExtent),
		SphereRadius(InSphereRadius)
	{}
	FBoxSphereBounds(const FBox& Box,const FSphere& Sphere)
	{
		Box.GetCenterAndExtents(Origin,BoxExtent);
		SphereRadius = Min(BoxExtent.Size(),((FVector)Sphere - Origin).Size() + Sphere.W);
	}
	FBoxSphereBounds(const FBox& Box)
	{
		Box.GetCenterAndExtents(Origin,BoxExtent);
		SphereRadius = BoxExtent.Size();
	}
	FBoxSphereBounds(const FVector* Points,UINT NumPoints)
	{
		// Find an axis aligned bounding box for the points.

		FBox	BoundingBox(0);
		for(UINT PointIndex = 0;PointIndex < NumPoints;PointIndex++)
			BoundingBox += Points[PointIndex];
		BoundingBox.GetCenterAndExtents(Origin,BoxExtent);

		// Using the center of the bounding box as the origin of the sphere, find the radius of the bounding sphere.
		SphereRadius = 0.0f;
		for(UINT PointIndex = 0;PointIndex < NumPoints;PointIndex++)
			SphereRadius = Max(SphereRadius,(Points[PointIndex] - Origin).Size());
	}

	// GetBoxExtrema

	FVector GetBoxExtrema(UINT Extrema) const
	{
		if(Extrema)
			return Origin + BoxExtent;
		else
			return Origin - BoxExtent;
	}

	// GetBox

	FBox GetBox() const
	{
		return FBox(Origin - BoxExtent,Origin + BoxExtent);
	}

	// GetSphere

	FSphere GetSphere() const
	{
		return FSphere(Origin,SphereRadius);
	}

	// TransformBy

	FBoxSphereBounds TransformBy(const FMatrix& M) const;

	// operator+

	friend FBoxSphereBounds operator+(const FBoxSphereBounds& A,const FBoxSphereBounds& B)
	{
		// Construct a bounding box containing both A and B.

		FBox	BoundingBox(0);
		BoundingBox += (A.Origin - A.BoxExtent);
		BoundingBox += (A.Origin + A.BoxExtent);
		BoundingBox += (B.Origin - B.BoxExtent);
		BoundingBox += (B.Origin + B.BoxExtent);

		// Build a bounding sphere from the bounding box's origin and the radii of A and B.

		FBoxSphereBounds	Result(BoundingBox);
		Result.SphereRadius = Min(Result.SphereRadius,Max((A.Origin - Result.Origin).Size() + A.SphereRadius,(B.Origin - Result.Origin).Size()));

		return Result;
	}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FBoxSphereBounds& Bounds)
	{
		return Ar << Bounds.Origin << Bounds.BoxExtent << Bounds.SphereRadius;
	}
};

//
//	FLinearColor - A linear, 32-bit/component floating point RGBA color.
//

struct FLinearColor
{
	FLOAT	R,
			G,
			B,
			A;

	FLinearColor() {}
	FLinearColor(FLOAT InR,FLOAT InG,FLOAT InB,FLOAT InA = 1.0f): R(InR), G(InG), B(InB), A(InA) {}
	FLinearColor(const FColor& C);

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FLinearColor& Color)
	{
		return Ar << Color.R << Color.G << Color.B << Color.A;
	}

	// Conversions.
	FColor ToRGBE() const;

	// Operators.

	friend FLinearColor operator+(const FLinearColor& ColorA,const FLinearColor& ColorB)
	{
		return FLinearColor(
			ColorA.R + ColorB.R,
			ColorA.G + ColorB.G,
			ColorA.B + ColorB.B,
			ColorA.A + ColorB.A
			);
	}
	FLinearColor& operator+=(const FLinearColor& ColorB)
	{
		R += ColorB.R;
		G += ColorB.G;
		B += ColorB.B;
		A += ColorB.A;
		return *this;
	}

	friend FLinearColor operator-(const FLinearColor& ColorA,const FLinearColor& ColorB)
	{
		return FLinearColor(
			ColorA.R - ColorB.R,
			ColorA.G - ColorB.G,
			ColorA.B - ColorB.B,
			ColorA.A - ColorB.A
			);
	}
	FLinearColor& operator-=(const FLinearColor& ColorB)
	{
		R -= ColorB.R;
		G -= ColorB.G;
		B -= ColorB.B;
		A -= ColorB.A;
		return *this;
	}

	friend FLinearColor operator*(const FLinearColor& ColorA,const FLinearColor& ColorB)
	{
		return FLinearColor(
			ColorA.R * ColorB.R,
			ColorA.G * ColorB.G,
			ColorA.B * ColorB.B,
			ColorA.A * ColorB.A
			);
	}
	FLinearColor& operator*=(const FLinearColor& ColorB)
	{
		R *= ColorB.R;
		G *= ColorB.G;
		B *= ColorB.B;
		A *= ColorB.A;
		return *this;
	}

	friend FLinearColor operator*(const FLinearColor& Color,FLOAT Scalar)
	{
		return FLinearColor(
			Color.R * Scalar,
			Color.G * Scalar,
			Color.B * Scalar,
			Color.A * Scalar
			);
	}
	friend FLinearColor operator*(FLOAT Scalar,const FLinearColor& Color)
	{
		return FLinearColor(
			Color.R * Scalar,
			Color.G * Scalar,
			Color.B * Scalar,
			Color.A * Scalar
			);
	}
	FLinearColor& operator*=(FLOAT Scalar)
	{
		R *= Scalar;
		G *= Scalar;
		B *= Scalar;
		A *= Scalar;
		return *this;
	}

	friend FLinearColor operator/(const FLinearColor& Color,FLOAT Scalar)
	{
		FLOAT	InvScalar = 1.0f / Scalar;
		return FLinearColor(
			Color.R * InvScalar,
			Color.G * InvScalar,
			Color.B * InvScalar,
			Color.A * InvScalar
			);
	}
	FLinearColor& operator/=(FLOAT Scalar)
	{
		FLOAT	InvScalar = 1.0f / Scalar;
		R *= InvScalar;
		G *= InvScalar;
		B *= InvScalar;
		A *= InvScalar;
		return *this;
	}

	friend UBOOL operator==(const FLinearColor& ColorA,const FLinearColor& ColorB)
	{
		return ColorA.R == ColorB.R && ColorA.G == ColorB.G && ColorA.B == ColorB.B && ColorA.A == ColorB.A;
	}

	// Common colors.	
	static const FLinearColor White;
	static const FLinearColor Black;
};

//
//	FColor
//

class FColor
{
public:
	// Variables.
#if __INTEL_BYTE_ORDER__
#ifdef __PSX2_EE__
	BYTE R GCC_ALIGN(4);
	BYTE G,B,A;
#else
#if _MSC_VER
	union { struct{ BYTE B,G,R,A; }; DWORD AlignmentDummy; };
#else
	BYTE B GCC_ALIGN(4);
	BYTE G,R,A;
#endif
#endif
#else
#ifdef XBOX
	union { struct{ BYTE A,R,G,B; }; DWORD AlignmentDummy; };
#else
	#error TODO: handle packing
	BYTE A,R,G,B;
#endif
#endif

	DWORD& DWColor(void) {return *((DWORD*)this);}
	const DWORD& DWColor(void) const {return *((DWORD*)this);}

	// Constructors.
	FColor() {}
	FColor( BYTE InR, BYTE InG, BYTE InB, BYTE InA = 255 )
		:	R(InR), G(InG), B(InB), A(InA) {}
	FColor(const FLinearColor& C);
	FColor( const FPlane& P )
			:	R(Clamp(appFloor(P.X*255),0,255))
			,	G(Clamp(appFloor(P.Y*255),0,255))
			,	B(Clamp(appFloor(P.Z*255),0,255))
			,	A(Clamp(appFloor(P.W*255),0,255))
		{}
	FColor( DWORD InColor )
	{ DWColor() = InColor; }

	// Serializer.
	friend FArchive& operator<< (FArchive &Ar, FColor &Color )
	{
		return Ar << Color.R << Color.G << Color.B << Color.A;
	}

	// Operators.
	UBOOL operator==( const FColor &C ) const
	{
		return DWColor() == C.DWColor();
	}
	UBOOL operator!=( const FColor& C ) const
	{
		return DWColor() != C.DWColor();
	}
	void operator+=(FColor C)
	{
#if ASM
		_asm
		{
			mov edi, DWORD PTR [this]
			mov ebx, DWORD PTR [C]
			mov eax, DWORD PTR [edi]

			xor ecx, ecx
			add al, bl
			adc ecx, 0xffffffff
			not ecx
			or al, cl
			mov BYTE PTR [edi], al
			inc edi
			shr eax, 8
			shr ebx, 8

			xor ecx, ecx
			add al, bl
			adc ecx, 0xffffffff
			not ecx
			or al, cl
			mov BYTE PTR [edi], al
			inc edi
			shr eax, 8
			shr ebx, 8

			xor ecx, ecx
			add al, bl
			adc ecx, 0xffffffff
			not ecx
			or al, cl
			mov BYTE PTR [edi], al
			inc edi
			shr eax, 8
			shr ebx, 8

			xor ecx, ecx
			add al, bl
			adc ecx, 0xffffffff
			not ecx
			or al, cl
			mov BYTE PTR [edi], al
		}
#else
		R = (BYTE) Min((INT) R + (INT) C.R,255);
		G = (BYTE) Min((INT) G + (INT) C.G,255);
		B = (BYTE) Min((INT) B + (INT) C.B,255);
		A = (BYTE) Min((INT) A + (INT) C.A,255);
#endif
	}
	INT Brightness() const
	{
		return (2*(INT)R + 3*(INT)G + 1*(INT)B)>>3;
	}
	FLOAT FBrightness() const
	{
		return (2.f*R + 3.f*G + 1.f*B)/(6.f*256.f);
	}
	FPlane Plane() const
	{
		return FPlane(R/255.f,G/255.f,B/255.f,A/255.f);
	}
	FColor Brighten( INT Amount )
	{
		return FColor( Plane() * (1.f - Amount/24.f) );
	}
	FColor RenderColor()
	{
		return *this;
	}
	FLinearColor FromRGBE() const;
	operator FPlane() const
	{
		return FPlane(R/255.f,G/255.f,B/255.f,A/255.f);
	}
	operator FVector() const
	{
		return FVector(R/255.f, G/255.f, B/255.f);
	}
	operator DWORD() const 
	{ 
		return DWColor(); 
	}
};

FLinearColor FGetHSV(BYTE H,BYTE S,BYTE V);
FColor MakeRandomColor();

/*-----------------------------------------------------------------------------
	FGlobalMath.
-----------------------------------------------------------------------------*/

//
// Global mathematics info.
//
class FGlobalMath
{
public:
	// Constants.
	enum {ANGLE_SHIFT 	= 2};		// Bits to right-shift to get lookup value.
	enum {ANGLE_BITS	= 14};		// Number of valid bits in angles.
	enum {NUM_ANGLES 	= 16384}; 	// Number of angles that are in lookup table.
	enum {ANGLE_MASK    =  (((1<<ANGLE_BITS)-1)<<(16-ANGLE_BITS))};

	// Basic math functions.
	FORCEINLINE FLOAT SinTab( int i )
	{
		return TrigFLOAT[((i>>ANGLE_SHIFT)&(NUM_ANGLES-1))];
	}
	FORCEINLINE FLOAT CosTab( int i )
	{
		return TrigFLOAT[(((i+16384)>>ANGLE_SHIFT)&(NUM_ANGLES-1))];
	}
	FLOAT SinFloat( FLOAT F )
	{
		return SinTab((INT)((F*65536.f)/(2.f*PI)));
	}
	FLOAT CosFloat( FLOAT F )
	{
		return CosTab((INT)((F*65536.f)/(2.f*PI)));
	}

	// Constructor.
	FGlobalMath();

private:
	// Tables.
	FLOAT  TrigFLOAT		[NUM_ANGLES];
};

inline INT ReduceAngle( INT Angle )
{
	return Angle & FGlobalMath::ANGLE_MASK;
};

/*-----------------------------------------------------------------------------
	Floating point constants.
-----------------------------------------------------------------------------*/

//
// Lengths of normalized vectors (These are half their maximum values
// to assure that dot products with normalized vectors don't overflow).
//
#define FLOAT_NORMAL_THRESH				(0.0001f)

//
// Magic numbers for numerical precision.
//
#define THRESH_POINT_ON_PLANE			(0.10f)		/* Thickness of plane for front/back/inside test */
#define THRESH_POINT_ON_SIDE			(0.20f)		/* Thickness of polygon side's side-plane for point-inside/outside/on side test */
#define THRESH_POINTS_ARE_SAME			(0.002f)	/* Two points are same if within this distance */
#define THRESH_POINTS_ARE_NEAR			(0.015f)	/* Two points are near if within this distance and can be combined if imprecise math is ok */
#define THRESH_NORMALS_ARE_SAME			(0.00002f)	/* Two normal points are same if within this distance */
													/* Making this too large results in incorrect CSG classification and disaster */
#define THRESH_VECTORS_ARE_NEAR			(0.0004f)	/* Two vectors are near if within this distance and can be combined if imprecise math is ok */
													/* Making this too large results in lighting problems due to inaccurate texture coordinates */
#define THRESH_SPLIT_POLY_WITH_PLANE	(0.25f)		/* A plane splits a polygon in half */
#define THRESH_SPLIT_POLY_PRECISELY		(0.01f)		/* A plane exactly splits a polygon */
#define THRESH_ZERO_NORM_SQUARED		(0.0001f)	/* Size of a unit normal that is considered "zero", squared */
#define THRESH_VECTORS_ARE_PARALLEL		(0.02f)		/* Vectors are parallel if dot product varies less than this */


/*-----------------------------------------------------------------------------
	FVector transformation.
-----------------------------------------------------------------------------*/

// Mirror a vector about a plane
inline FVector FVector::MirrorByPlane( const FPlane& Plane ) const
{
	return *this - Plane * (2.f * Plane.PlaneDot(*this) );
}

// Rotate around Axis (assumes Axis.Size() == 1)
inline FVector FVector::RotateAngleAxis( const INT Angle, const FVector& Axis ) const
{
	FLOAT S		= GMath.SinTab(Angle);
	FLOAT C		= GMath.CosTab(Angle);

	FLOAT XX	= Axis.X * Axis.X;
	FLOAT YY	= Axis.Y * Axis.Y;
	FLOAT ZZ	= Axis.Z * Axis.Z;

	FLOAT XY	= Axis.X * Axis.Y;
	FLOAT YZ	= Axis.Y * Axis.Z;
	FLOAT ZX	= Axis.Z * Axis.X;

	FLOAT XS	= Axis.X * S;
	FLOAT YS	= Axis.Y * S;
	FLOAT ZS	= Axis.Z * S;

	FLOAT OMC	= 1.f - C;

	return FVector(
		(OMC * XX + C ) * X + (OMC * XY - ZS) * Y + (OMC * ZX + YS) * Z,
		(OMC * XY + ZS) * X + (OMC * YY + C ) * Y + (OMC * YZ - XS) * Z,
		(OMC * ZX - YS) * X + (OMC * YZ + XS) * Y + (OMC * ZZ + C ) * Z
		);
}


/*-----------------------------------------------------------------------------
	FVector friends.
-----------------------------------------------------------------------------*/

//
// Compare two points and see if they're the same, using a threshold.
// Returns 1=yes, 0=no.  Uses fast distance approximation.
//
inline int FPointsAreSame( const FVector &P, const FVector &Q )
{
	FLOAT Temp;
	Temp=P.X-Q.X;
	if( (Temp > -THRESH_POINTS_ARE_SAME) && (Temp < THRESH_POINTS_ARE_SAME) )
	{
		Temp=P.Y-Q.Y;
		if( (Temp > -THRESH_POINTS_ARE_SAME) && (Temp < THRESH_POINTS_ARE_SAME) )
		{
			Temp=P.Z-Q.Z;
			if( (Temp > -THRESH_POINTS_ARE_SAME) && (Temp < THRESH_POINTS_ARE_SAME) )
			{
				return 1;
			}
		}
	}
	return 0;
}

//
// Compare two points and see if they're the same, using a threshold.
// Returns 1=yes, 0=no.  Uses fast distance approximation.
//
inline int FPointsAreNear( const FVector &Point1, const FVector &Point2, FLOAT Dist )
{
	FLOAT Temp;
	Temp=(Point1.X - Point2.X); if (Abs(Temp)>=Dist) return 0;
	Temp=(Point1.Y - Point2.Y); if (Abs(Temp)>=Dist) return 0;
	Temp=(Point1.Z - Point2.Z); if (Abs(Temp)>=Dist) return 0;
	return 1;
}

//
// Calculate the signed distance (in the direction of the normal) between
// a point and a plane.
//
inline FLOAT FPointPlaneDist
(
	const FVector &Point,
	const FVector &PlaneBase,
	const FVector &PlaneNormal
)
{
	return (Point - PlaneBase) | PlaneNormal;
}

//
// Euclidean distance between two points.
//
inline FLOAT FDist( const FVector &V1, const FVector &V2 )
{
	return appSqrt( Square(V2.X-V1.X) + Square(V2.Y-V1.Y) + Square(V2.Z-V1.Z) );
}

//
// Squared distance between two points.
//
inline FLOAT FDistSquared( const FVector &V1, const FVector &V2 )
{
	return Square(V2.X-V1.X) + Square(V2.Y-V1.Y) + Square(V2.Z-V1.Z);
}

//
// See if two normal vectors (or plane normals) are nearly parallel.
//
inline int FParallel( const FVector &Normal1, const FVector &Normal2 )
{
	FLOAT NormalDot = Normal1 | Normal2;
	return (Abs (NormalDot - 1.f) <= THRESH_VECTORS_ARE_PARALLEL);
}

//
// See if two planes are coplanar.
//
inline int FCoplanar( const FVector &Base1, const FVector &Normal1, const FVector &Base2, const FVector &Normal2 )
{
	if      (!FParallel(Normal1,Normal2)) return 0;
	else if (FPointPlaneDist (Base2,Base1,Normal1) > THRESH_POINT_ON_PLANE) return 0;
	else    return 1;
}

//
// Triple product of three vectors.
//
inline FLOAT FTriple( const FVector& X, const FVector& Y, const FVector& Z )
{
	return
	(	(X.X * (Y.Y * Z.Z - Y.Z * Z.Y))
	+	(X.Y * (Y.Z * Z.X - Y.X * Z.Z))
	+	(X.Z * (Y.X * Z.Y - Y.Y * Z.X)) );
}

//
// Compute pushout of a box from a plane.
//
inline FLOAT FBoxPushOut( const FVector & Normal, const FVector & Size )
{
    return fabs(Normal.X*Size.X) + fabs(Normal.Y*Size.Y) + fabs(Normal.Z*Size.Z);
}

/*-----------------------------------------------------------------------------
	Random numbers.
-----------------------------------------------------------------------------*/

//
// Return a uniformly distributed random unit vector.
//
inline FVector VRand()
{
	FVector Result;
	do
	{
		// Check random vectors in the unit sphere so result is statistically uniform.
		Result.X = appFrand()*2 - 1;
		Result.Y = appFrand()*2 - 1;
		Result.Z = appFrand()*2 - 1;
	} while( Result.SizeSquared() > 1.f );
	return Result.UnsafeNormal();
}

/*-----------------------------------------------------------------------------
	Advanced geometry.
-----------------------------------------------------------------------------*/

//
// Find the intersection of an infinite line (defined by two points) and
// a plane.  Assumes that the line and plane do indeed intersect; you must
// make sure they're not parallel before calling.
//
inline FVector FLinePlaneIntersection
(
	const FVector &Point1,
	const FVector &Point2,
	const FVector &PlaneOrigin,
	const FVector &PlaneNormal
)
{
	return
		Point1
	+	(Point2-Point1)
	*	(((PlaneOrigin - Point1)|PlaneNormal) / ((Point2 - Point1)|PlaneNormal));
}
inline FVector FLinePlaneIntersection
(
	const FVector &Point1,
	const FVector &Point2,
	const FPlane  &Plane
)
{
	return
		Point1
	+	(Point2-Point1)
	*	((Plane.W - (Point1|Plane))/((Point2 - Point1)|Plane));
}

//
// Determines whether a point is inside a box.
//

inline UBOOL FPointBoxIntersection
(
	const FVector&	Point,
	const FBox&		Box
)
{
	if(Point.X >= Box.Min.X && Point.X <= Box.Max.X &&
	   Point.Y >= Box.Min.Y && Point.Y <= Box.Max.Y &&
	   Point.Z >= Box.Min.Z && Point.Z <= Box.Max.Z)
		return 1;
	else
		return 0;
}

//
// Determines whether a line intersects a box.
//

#define BOX_SIDE_THRESHOLD	0.1f

inline UBOOL FLineBoxIntersection
(
	const FBox&		Box,
	const FVector&	Start,
	const FVector&	End,
	const FVector&	Direction,
	const FVector&	OneOverDirection
)
{
	FVector	Time;
	UBOOL	Inside = 1;

	if(Start.X < Box.Min.X)
	{
		if(Direction.X <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.X = (Box.Min.X - Start.X) * OneOverDirection.X;
		}
	}
	else if(Start.X > Box.Max.X)
	{
		if(Direction.X >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.X = (Box.Max.X - Start.X) * OneOverDirection.X;
		}
	}
	else
		Time.X = 0.0f;

	if(Start.Y < Box.Min.Y)
	{
		if(Direction.Y <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Y = (Box.Min.Y - Start.Y) * OneOverDirection.Y;
		}
	}
	else if(Start.Y > Box.Max.Y)
	{
		if(Direction.Y >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Y = (Box.Max.Y - Start.Y) * OneOverDirection.Y;
		}
	}
	else
		Time.Y = 0.0f;

	if(Start.Z < Box.Min.Z)
	{
		if(Direction.Z <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Z = (Box.Min.Z - Start.Z) * OneOverDirection.Z;
		}
	}
	else if(Start.Z > Box.Max.Z)
	{
		if(Direction.Z >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Z = (Box.Max.Z - Start.Z) * OneOverDirection.Z;
		}
	}
	else
		Time.Z = 0.0f;

	if(Inside)
		return 1;
	else
	{
		FLOAT	MaxTime = Max(Time.X,Max(Time.Y,Time.Z));

		if(MaxTime >= 0.0f && MaxTime <= 1.0f)
		{
			FVector	Hit = Start + Direction * MaxTime;

			if(	Hit.X > Box.Min.X - BOX_SIDE_THRESHOLD && Hit.X < Box.Max.X + BOX_SIDE_THRESHOLD &&
				Hit.Y > Box.Min.Y - BOX_SIDE_THRESHOLD && Hit.Y < Box.Max.Y + BOX_SIDE_THRESHOLD &&
				Hit.Z > Box.Min.Z - BOX_SIDE_THRESHOLD && Hit.Z < Box.Max.Z + BOX_SIDE_THRESHOLD)
				return 1;
		}

		return 0;
	}
}

UBOOL FLineExtentBoxIntersection(const FBox& inBox, 
								 const FVector& Start, 
								 const FVector& End,
								 const FVector& Extent,
								 FVector& HitLocation,
								 FVector& HitNormal,
								 FLOAT& HitTime);

//
// Determines whether a line intersects a sphere.
//

inline UBOOL FLineSphereIntersection(FVector Start,FVector Dir,FLOAT Length,FVector Origin,FLOAT Radius)
{
	FVector	EO = Start - Origin;
	FLOAT	v = (Dir | (Origin - Start)),
			disc = Radius * Radius - ((EO | EO) - v * v);

	if(disc >= 0.0f)
	{
		FLOAT	Time = (v - appSqrt(disc)) / Length;

		if(Time >= 0.0f && Time <= 1.0f)
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

/*-----------------------------------------------------------------------------
	FPlane functions.
-----------------------------------------------------------------------------*/

//
// Compute intersection point of three planes.
// Return 1 if valid, 0 if infinite.
//
inline UBOOL FIntersectPlanes3( FVector& I, const FPlane& P1, const FPlane& P2, const FPlane& P3 )
{
	// Compute determinant, the triple product P1|(P2^P3)==(P1^P2)|P3.
	FLOAT Det = (P1 ^ P2) | P3;
	if( Square(Det) < Square(0.001f) )
	{
		// Degenerate.
		I = FVector(0,0,0);
		return 0;
	}
	else
	{
		// Compute the intersection point, guaranteed valid if determinant is nonzero.
		I = (P1.W*(P2^P3) + P2.W*(P3^P1) + P3.W*(P1^P2)) / Det;
	}
	return 1;
}

//
// Compute intersection point and direction of line joining two planes.
// Return 1 if valid, 0 if infinite.
//
inline UBOOL FIntersectPlanes2( FVector& I, FVector& D, const FPlane& P1, const FPlane& P2 )
{
	// Compute line direction, perpendicular to both plane normals.
	D = P1 ^ P2;
	FLOAT DD = D.SizeSquared();
	if( DD < Square(0.001f) )
	{
		// Parallel or nearly parallel planes.
		D = I = FVector(0,0,0);
		return 0;
	}
	else
	{
		// Compute intersection.
		I = (P1.W*(P2^D) + P2.W*(D^P1)) / DD;
		D.Normalize();
		return 1;
	}
}

/*-----------------------------------------------------------------------------
	FQuat.          
-----------------------------------------------------------------------------*/

// floating point quaternion.
class FQuat 
{
public:

	static const FQuat Identity;

	// Variables.
	FLOAT X,Y,Z,W;
	// X,Y,Z, W also doubles as the Axis/Angle format.

	// Constructors.
	FQuat()
	{}

	FQuat( FLOAT InX, FLOAT InY, FLOAT InZ, FLOAT InA )
	:	X(InX), Y(InY), Z(InZ), W(InA)
	{}

	FQuat( const FMatrix& M );	

	// Assumes Axis is normalised.
	FQuat( FVector Axis, FLOAT Angle )
	{
		FLOAT half_a = 0.5f * Angle;
		FLOAT s = appSin(half_a);
		FLOAT c = appCos(half_a);

		X = s * Axis.X;
		Y = s * Axis.Y;
		Z = s * Axis.Z;
		W = c;
	}

	static FQuat MakeFromEuler(const FVector& Euler);

	FVector Euler() const;

	// Binary operators.
	FQuat operator+( const FQuat& Q ) const
	{		
		return FQuat( X + Q.X, Y + Q.Y, Z + Q.Z, W + Q.W );
	}

	FQuat operator-( const FQuat& Q ) const
	{
		return FQuat( X - Q.X, Y - Q.Y, Z - Q.Z, W - Q.W );
	}

	FQuat operator*( const FQuat& Q ) const
	{
		return FQuat( 
			W*Q.X + X*Q.W + Y*Q.Z - Z*Q.Y,
			W*Q.Y - X*Q.Z + Y*Q.W + Z*Q.X,
			W*Q.Z + X*Q.Y - Y*Q.X + Z*Q.W,
			W*Q.W - X*Q.X - Y*Q.Y - Z*Q.Z
			);
	}

	FQuat operator*( const FLOAT& Scale ) const
	{
		return FQuat( Scale*X, Scale*Y, Scale*Z, Scale*W);			
	}
	
	// Unary operators.
	FQuat operator-() const
	{
		//return FQuat( X, Y, Z, -W );
		return FQuat( -X, -Y, -Z, W );
	}

    // Misc operatorsddddd
	UBOOL operator!=( const FQuat& Q ) const
	{
		return X!=Q.X || Y!=Q.Y || Z!=Q.Z || W!=Q.W;
	}
	
	FLOAT operator|( const FQuat& Q ) const
	{
		return X*Q.X + Y*Q.Y + Z*Q.Z + W*Q.W;
	}

	UBOOL Normalize()
	{
		// 
		FLOAT SquareSum = (FLOAT)(X*X+Y*Y+Z*Z+W*W);
		if( SquareSum >= DELTA )
		{
			FLOAT Scale = appInvSqrt(SquareSum);
			X *= Scale; 
			Y *= Scale; 
			Z *= Scale;
			W *= Scale;
			return true;
		}
		else 
		{	
			X = 0.0f;
			Y = 0.0f;
			Z = 0.0f;
			W = 1.0f;
			return false;
		}
	}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FQuat& F )
	{
		return Ar << F.X << F.Y << F.Z << F.W;
	}

	// Warning : assumes normalized quaternions.
	FQuat FQuatToAngAxis()
	{
		FLOAT scale = (FLOAT)appSin(W);
		FQuat A;

		if (scale >= DELTA)
		{
			A.X = Z / scale;
			A.Y = Y / scale;
			A.Z = Z / scale;
			A.W = (2.0f * appAcos (W)); 
			// Degrees: A.W = ((FLOAT)appAcos(W) * 360.0f) * INV_PI;  
		}
		else 
		{
			A.X = 0.0f;
			A.Y = 0.0f;
			A.Z = 1.0f;
			A.W = 0.0f; 
		}

		return A;
	};

	//
	// Angle-Axis to Quaternion. No normalized axis assumed.
	//
	FQuat AngAxisToFQuat()
	{
		FLOAT scale = X*X + Y*Y + Z*Z;
		FQuat Q;

		if (scale >= DELTA)
		{
			FLOAT invscale = 1.0f /(FLOAT)appSqrt(scale);
			Q.X = X * invscale;
			Q.Y = Y * invscale;
			Q.Z = Z * invscale;
			Q.W = appCos( W * 0.5f); //Radians assumed.
		}
		else
		{
			Q.X = 0.0f;
			Q.Y = 0.0f;
			Q.Z = 1.0f;
			Q.W = 0.0f; 
		}
		return Q;
	}	

	FVector RotateVector(FVector v)
	{	
		// (q.W*q.W-qv.qv)v + 2(qv.v)qv + 2 q.W (qv x v)

		FVector qv(X, Y, Z);
		FVector vOut = 2.f * W * (qv ^ v);
		vOut += ((W * W) - (qv | qv)) * v;
		vOut += (2.f * (qv | v)) * qv;

		return vOut;
	}

	// Exp should really only be used after Log.
	FQuat Log();
	FQuat Exp();
};

// Generate the 'smallest' (geodesic) rotation between these two vectors.
FQuat FQuatFindBetween(const FVector& vec1, const FVector& vec2);

// Dot product of axes to get cos of angle  #Warning some people use .W component here too !
inline FLOAT FQuatDot(const FQuat& Q1,const FQuat& Q2)
{
	return( Q1.X*Q2.X + Q1.Y*Q2.Y + Q1.Z*Q2.Z );
};

// Error measure (angle) between two quaternions, ranged [0..1]
inline FLOAT FQuatError(FQuat& Q1,FQuat& Q2)
{
	// Returns the hypersphere-angle between two quaternions; alignment shouldn't matter, though 
	// normalized input is expected.
	FLOAT cosom = Q1.X*Q2.X + Q1.Y*Q2.Y + Q1.Z*Q2.Z + Q1.W*Q2.W;
	return (Abs(cosom) < 0.9999999f) ? appAcos(cosom)*(1.f/PI) : 0.0f;
}

// Ensure quat1 points to same side of the hypersphere as quat2
inline void AlignFQuatWith(FQuat &quat1, const FQuat &quat2)
{
	FLOAT Minus  = Square(quat1.X-quat2.X) + Square(quat1.Y-quat2.Y) + Square(quat1.Z-quat2.Z) + Square(quat1.W-quat2.W);
	FLOAT Plus   = Square(quat1.X+quat2.X) + Square(quat1.Y+quat2.Y) + Square(quat1.Z+quat2.Z) + Square(quat1.W+quat2.W);

	if (Minus > Plus)
	{
		quat1.X = - quat1.X;
		quat1.Y = - quat1.Y;
		quat1.Z = - quat1.Z;
		quat1.W = - quat1.W;
	}
}

// Spherical interpolation. Will correct alignment. Output is not normalized.
inline FQuat SlerpQuat(const FQuat &quat1,const FQuat &quat2, float slerp)
{
	FLOAT Cosom,RawCosom,scale0,scale1;

	// Get cosine of angle between quats.
	Cosom = RawCosom = 
		       quat1.X * quat2.X +
			quat1.Y * quat2.Y +
			quat1.Z * quat2.Z +
			quat1.W * quat2.W;

	// Unaligned quats - compensate, results in taking shorter route.
	if( RawCosom < 0.0f )  
	{	
		Cosom = -RawCosom;
	}
	
	if( Cosom < 0.9999f )
	{	
		FLOAT omega = appAcos(Cosom);
		FLOAT sininv = 1.f/appSin(omega);
		scale0 = appSin((1.f - slerp) * omega) * sininv;
		scale1 = appSin(slerp * omega) * sininv;
	}
	else
	{
		// Use linear interpolation.
		scale0 = 1.0f - slerp;
		scale1 = slerp;	
	}
	// In keeping with our flipped Cosom:
	if( RawCosom < 0.0f)
	{		
		scale1= -scale1;
	}

	FQuat result;
		
		result.X = scale0 * quat1.X + scale1 * quat2.X;
		result.Y = scale0 * quat1.Y + scale1 * quat2.Y;
		result.Z = scale0 * quat1.Z + scale1 * quat2.Z;
		result.W = scale0 * quat1.W + scale1 * quat2.W;

	return result;
}

FQuat SlerpQuatFullPath(const FQuat &quat1, const FQuat &quat2, float alpha);
FQuat SquadQuat(const FQuat& quat1, const FQuat& tang1, const FQuat& quat2, const FQuat& tang2, FLOAT Alpha);


// Literally corner-cutting spherical interpolation. Does complete alignment and renormalization.
inline void FastSlerpNormQuat(const FQuat *quat1,const FQuat *quat2, float slerp, FQuat *result)
{	
	FLOAT Cosom,RawCosom,scale0,scale1;

	// Get cosine of angle between quats.
	Cosom = RawCosom = 
		quat1->X * quat2->X +
		quat1->Y * quat2->Y +
		quat1->Z * quat2->Z +
		quat1->W * quat2->W;

	// Unaligned quats - compensate, results in taking shorter route.
	if( RawCosom < 0.0f )  
	{
		Cosom = -RawCosom;
	}

	// Linearly interpolatable ?
	if( Cosom < 0.55f ) // Arbitrary cutoff for linear interpolation. // .55 seems useful.
	{	
		FLOAT omega = appAcos(Cosom);
		FLOAT sininv = 1.f/appSin(omega);
		scale0 = appSin( ( 1.f - slerp ) * omega ) * sininv;
		scale1 = appSin( slerp * omega ) * sininv;
	}
	else
	{
		// Use linear interpolation.
		scale0 = 1.0f - slerp;
		scale1 = slerp;	
	}

	// In keeping with our flipped Cosom:
	if( RawCosom < 0.0f)
	{		
		scale1= -scale1;
	}

	result->X = scale0 * quat1->X + scale1 * quat2->X;
	result->Y = scale0 * quat1->Y + scale1 * quat2->Y;
	result->Z = scale0 * quat1->Z + scale1 * quat2->Z;
	result->W = scale0 * quat1->W + scale1 * quat2->W;	

	// Normalize:
	FLOAT SquareSum = result->X*result->X + 
		result->Y*result->Y + 
		result->Z*result->Z + 
		result->W*result->W; 

	if( SquareSum >= 0.00001f )
	{
		FLOAT Scale = 1.0f/(FLOAT)appSqrt(SquareSum);
		result->X *= Scale; 
		result->Y *= Scale; 
		result->Z *= Scale;
		result->W *= Scale;
	}	
	else  // Avoid divide by (nearly) zero.
	{	
		result->X = 0.0f;
		result->Y = 0.0f;
		result->Z = 0.0f;
		result->W = 1.0f;
	}	

	return;
}



/*-----------------------------------------------------------------------------
	FMatrix classes.
-----------------------------------------------------------------------------*/

class FMatrix
{
public:

	// Variables.
#if __GNUG__
	FLOAT M[4][4] GCC_ALIGN(16);
	static const FMatrix Identity;
#elif _MSC_VER
	__declspec(align(16)) FLOAT M[4][4];
	static const __declspec(align(16)) FMatrix Identity;
#else
#error FMatrix needs to be properly aligned
#endif 

	// Constructors.

	FORCEINLINE FMatrix()
	{
	}

	FORCEINLINE FMatrix(FPlane InX,FPlane InY,FPlane InZ,FPlane InW)
	{
		M[0][0] = InX.X; M[0][1] = InX.Y;  M[0][2] = InX.Z;  M[0][3] = InX.W;
		M[1][0] = InY.X; M[1][1] = InY.Y;  M[1][2] = InY.Z;  M[1][3] = InY.W;
		M[2][0] = InZ.X; M[2][1] = InZ.Y;  M[2][2] = InZ.Z;  M[2][3] = InZ.W;
		M[3][0] = InW.X; M[3][1] = InW.Y;  M[3][2] = InW.Z;  M[3][3] = InW.W;
	}

	FORCEINLINE FMatrix(FVector InX,FVector InY,FVector InZ,FVector InW)
	{
		M[0][0] = InX.X; M[0][1] = InX.Y;  M[0][2] = InX.Z;  M[0][3] = 0.0f;
		M[1][0] = InY.X; M[1][1] = InY.Y;  M[1][2] = InY.Z;  M[1][3] = 0.0f;
		M[2][0] = InZ.X; M[2][1] = InZ.Y;  M[2][2] = InZ.Z;  M[2][3] = 0.0f;
		M[3][0] = InW.X; M[3][1] = InW.Y;  M[3][2] = InW.Z;  M[3][3] = 1.0f;
	}

	FMatrix(const FVector& Origin, const FQuat& Q)
	{
		FLOAT wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;

		x2 = Q.X + Q.X;  y2 = Q.Y + Q.Y;  z2 = Q.Z + Q.Z;
		xx = Q.X * x2;   xy = Q.X * y2;   xz = Q.X * z2;
		yy = Q.Y * y2;   yz = Q.Y * z2;   zz = Q.Z * z2;
		wx = Q.W * x2;   wy = Q.W * y2;   wz = Q.W * z2;

		M[0][0] = 1.0f - (yy + zz);	M[1][0] = xy - wz;				M[2][0] = xz + wy;			M[3][0] = Origin.X;
		M[0][1] = xy + wz;			M[1][1] = 1.0f - (xx + zz);		M[2][1] = yz - wx;			M[3][1] = Origin.Y;
		M[0][2] = xz - wy;			M[1][2] = yz + wx;				M[2][2] = 1.0f - (xx + yy);	M[3][2] = Origin.Z;
		M[0][3] = 0.0f;				M[1][3] = 0.0f;					M[2][3] = 0.0f;				M[3][3] = 1.0f;
	}

	FMatrix(const FVector& Origin, const FRotator& Rotation);

	// Destructor.

	~FMatrix()
	{
	}

	void SetIdentity()
	{
		M[0][0] = 1; M[0][1] = 0;  M[0][2] = 0;  M[0][3] = 0;
		M[1][0] = 0; M[1][1] = 1;  M[1][2] = 0;  M[1][3] = 0;
		M[2][0] = 0; M[2][1] = 0;  M[2][2] = 1;  M[2][3] = 0;
		M[3][0] = 0; M[3][1] = 0;  M[3][2] = 0;  M[3][3] = 1;
	}

	// Concatenation operator.

	FORCEINLINE FMatrix operator*(const FMatrix& Other) const
	{
		FMatrix	Result;

#if ASM
		__asm
		{
		mov		eax,[Other]
		mov		ecx,[this]
	
		movups	xmm4,[eax]				// Other.M[0][0-3]
		movups	xmm5,[eax+16]			// Other.M[1][0-3]
		movups	xmm6,[eax+32]			// Other.M[2][0-3]
		movups	xmm7,[eax+48]			// Other.M[3][0-3]

		lea		eax,[Result]

		// Begin first row of result.
		movss	xmm0,[ecx]				// M[0][0] 
		shufps	xmm0,xmm0,0
		mulps	xmm0,xmm4

		movss	xmm1,[ecx+4]			// M[0][1]
		shufps	xmm1,xmm1,0
		mulps	xmm1,xmm5

		movss	xmm2,[ecx+8]			// M[0][2]
		shufps	xmm2,xmm2,0
		mulps	xmm2,xmm6

		addps	xmm1,xmm0				// First row done with xmm0

		movss	xmm3,[ecx+12]			// M[0][3]
		shufps	xmm3,xmm3,0
		mulps	xmm3,xmm7
		
		// Begin second row of result.
		movss	xmm0,[ecx+16]			// M[1][0] 
		shufps	xmm0,xmm0,0
		mulps	xmm0,xmm4
	
		addps	xmm3,xmm2				// First row done with xmm2
		
		movss	xmm2,[ecx+20]			// M[1][1]
		shufps	xmm2,xmm2,0
		mulps	xmm2,xmm5

		addps	xmm3,xmm1				// First row done with xmm1
	
		movss	xmm1,[ecx+24]			// M[1][2]
		shufps	xmm1,xmm1,0
		mulps	xmm1,xmm6

		movups	[eax],xmm3				// Store Result.M[0][0-3]
		// Done computing first row.
		
		addps	xmm2,xmm0				// Second row done with xmm0

		movss	xmm3,[ecx+28]			// M[1][3]
		shufps	xmm3,xmm3,0
		mulps	xmm3,xmm7
		
		// Begin third row of result.
		movss	xmm0,[ecx+32]			// M[2][0] 
		shufps	xmm0,xmm0,0
		mulps	xmm0,xmm4
	
		addps	xmm3,xmm1				// Second row done with xmm1
		
		movss	xmm1,[ecx+36]			// M[2][1]
		shufps	xmm1,xmm1,0
		mulps	xmm1,xmm5

		addps	xmm3,xmm2				// Second row done with xmm2

		movss	xmm2,[ecx+40]			// M[2][2]
		shufps	xmm2,xmm2,0
		mulps	xmm2,xmm6

		movups	[eax+16],xmm3			// Store Result.M[1][0-3]
		// Done computing second row.

		addps	xmm1,xmm0				// Third row done with xmm0

		movss	xmm3,[ecx+44]			// M[2][3]
		shufps	xmm3,xmm3,0
		mulps	xmm3,xmm7
		
		// Begin fourth row of result.
		movss	xmm0,[ecx+48]			// M[3][0]
		shufps	xmm0,xmm0,0
		mulps	xmm0,xmm4
	
		addps	xmm3,xmm2				// Third row done with xmm2
		
		movss	xmm2,[ecx+52]			// M[3][1]
		shufps	xmm2,xmm2,0
		mulps	xmm2,xmm5

		addps	xmm3,xmm1				// Third row done with xmm1
		
		movss	xmm1,[ecx+56]			// M[3][2]
		shufps	xmm1,xmm1,0
		mulps	xmm1,xmm6

		movups	[eax+32],xmm3			// Store Result.M[2][0-3]
		// Done computing third row.

		addps	xmm2,xmm0

		movss	xmm3,[ecx+60]			// M[3][3]
		shufps	xmm3,xmm3,0
		mulps	xmm3,xmm7
		
		// stall
	
		addps	xmm3,xmm1
		
		// stall

		addps	xmm3,xmm2
	
		movups	[eax+48],xmm3			// Store Result.M[3][0-3]
		// Done computing fourth row.
		}
#else

		Result.M[0][0] = M[0][0] * Other.M[0][0] + M[0][1] * Other.M[1][0] + M[0][2] * Other.M[2][0] + M[0][3] * Other.M[3][0];
		Result.M[0][1] = M[0][0] * Other.M[0][1] + M[0][1] * Other.M[1][1] + M[0][2] * Other.M[2][1] + M[0][3] * Other.M[3][1];
		Result.M[0][2] = M[0][0] * Other.M[0][2] + M[0][1] * Other.M[1][2] + M[0][2] * Other.M[2][2] + M[0][3] * Other.M[3][2];
		Result.M[0][3] = M[0][0] * Other.M[0][3] + M[0][1] * Other.M[1][3] + M[0][2] * Other.M[2][3] + M[0][3] * Other.M[3][3];

		Result.M[1][0] = M[1][0] * Other.M[0][0] + M[1][1] * Other.M[1][0] + M[1][2] * Other.M[2][0] + M[1][3] * Other.M[3][0];
		Result.M[1][1] = M[1][0] * Other.M[0][1] + M[1][1] * Other.M[1][1] + M[1][2] * Other.M[2][1] + M[1][3] * Other.M[3][1];
		Result.M[1][2] = M[1][0] * Other.M[0][2] + M[1][1] * Other.M[1][2] + M[1][2] * Other.M[2][2] + M[1][3] * Other.M[3][2];
		Result.M[1][3] = M[1][0] * Other.M[0][3] + M[1][1] * Other.M[1][3] + M[1][2] * Other.M[2][3] + M[1][3] * Other.M[3][3];

		Result.M[2][0] = M[2][0] * Other.M[0][0] + M[2][1] * Other.M[1][0] + M[2][2] * Other.M[2][0] + M[2][3] * Other.M[3][0];
		Result.M[2][1] = M[2][0] * Other.M[0][1] + M[2][1] * Other.M[1][1] + M[2][2] * Other.M[2][1] + M[2][3] * Other.M[3][1];
		Result.M[2][2] = M[2][0] * Other.M[0][2] + M[2][1] * Other.M[1][2] + M[2][2] * Other.M[2][2] + M[2][3] * Other.M[3][2];
		Result.M[2][3] = M[2][0] * Other.M[0][3] + M[2][1] * Other.M[1][3] + M[2][2] * Other.M[2][3] + M[2][3] * Other.M[3][3];

		Result.M[3][0] = M[3][0] * Other.M[0][0] + M[3][1] * Other.M[1][0] + M[3][2] * Other.M[2][0] + M[3][3] * Other.M[3][0];
		Result.M[3][1] = M[3][0] * Other.M[0][1] + M[3][1] * Other.M[1][1] + M[3][2] * Other.M[2][1] + M[3][3] * Other.M[3][1];
		Result.M[3][2] = M[3][0] * Other.M[0][2] + M[3][1] * Other.M[1][2] + M[3][2] * Other.M[2][2] + M[3][3] * Other.M[3][2];
		Result.M[3][3] = M[3][0] * Other.M[0][3] + M[3][1] * Other.M[1][3] + M[3][2] * Other.M[2][3] + M[3][3] * Other.M[3][3];

#endif

		return Result;
	}

	FORCEINLINE void operator*=(const FMatrix& Other)
	{
#if ASM
		__asm
		{
		mov		eax,[Other]
		mov		ecx,[this]
	
		movups	xmm4,[eax]				// Other.M[0][0-3]
		movups	xmm5,[eax+16]			// Other.M[1][0-3]
		movups	xmm6,[eax+32]			// Other.M[2][0-3]
		movups	xmm7,[eax+48]			// Other.M[3][0-3]

		// Begin first row of result.
		movss	xmm0,[ecx]				// M[0][0] 
		shufps	xmm0,xmm0,0
		mulps	xmm0,xmm4

		movss	xmm1,[ecx+4]			// M[0][1]
		shufps	xmm1,xmm1,0
		mulps	xmm1,xmm5

		movss	xmm2,[ecx+8]			// M[0][2]
		shufps	xmm2,xmm2,0
		mulps	xmm2,xmm6

		addps	xmm1,xmm0				// First row done with xmm0

		movss	xmm3,[ecx+12]			// M[0][3]
		shufps	xmm3,xmm3,0
		mulps	xmm3,xmm7
		
		// Begin second row of result.
		movss	xmm0,[ecx+16]			// M[1][0] 
		shufps	xmm0,xmm0,0
		mulps	xmm0,xmm4
	
		addps	xmm3,xmm2				// First row done with xmm2
		
		movss	xmm2,[ecx+20]			// M[1][1]
		shufps	xmm2,xmm2,0
		mulps	xmm2,xmm5

		addps	xmm3,xmm1				// First row done with xmm1
	
		movss	xmm1,[ecx+24]			// M[1][2]
		shufps	xmm1,xmm1,0
		mulps	xmm1,xmm6

		movups	[ecx],xmm3				// Store M[0][0-3]
		// Done computing first row.
		
		addps	xmm2,xmm0				// Second row done with xmm0

		movss	xmm3,[ecx+28]			// M[1][3]
		shufps	xmm3,xmm3,0
		mulps	xmm3,xmm7
		
		// Begin third row of result.
		movss	xmm0,[ecx+32]			// M[2][0] 
		shufps	xmm0,xmm0,0
		mulps	xmm0,xmm4
	
		addps	xmm3,xmm1				// Second row done with xmm1
		
		movss	xmm1,[ecx+36]			// M[2][1]
		shufps	xmm1,xmm1,0
		mulps	xmm1,xmm5

		addps	xmm3,xmm2				// Second row done with xmm2

		movss	xmm2,[ecx+40]			// M[2][2]
		shufps	xmm2,xmm2,0
		mulps	xmm2,xmm6

		movups	[ecx+16],xmm3			// Store M[1][0-3]
		// Done computing second row.

		addps	xmm1,xmm0				// Third row done with xmm0

		movss	xmm3,[ecx+44]			// M[2][3]
		shufps	xmm3,xmm3,0
		mulps	xmm3,xmm7
		
		// Begin fourth row of result.
		movss	xmm0,[ecx+48]			// M[3][0]
		shufps	xmm0,xmm0,0
		mulps	xmm0,xmm4
	
		addps	xmm3,xmm2				// Third row done with xmm2
		
		movss	xmm2,[ecx+52]			// M[3][1]
		shufps	xmm2,xmm2,0
		mulps	xmm2,xmm5

		addps	xmm3,xmm1				// Third row done with xmm1
		
		movss	xmm1,[ecx+56]			// M[3][2]
		shufps	xmm1,xmm1,0
		mulps	xmm1,xmm6

		movups	[ecx+32],xmm3			// Store M[2][0-3]
		// Done computing third row.

		addps	xmm2,xmm0

		movss	xmm3,[ecx+60]			// M[3][3]
		shufps	xmm3,xmm3,0
		mulps	xmm3,xmm7
		
		// stall
	
		addps	xmm3,xmm1
		
		// stall

		addps	xmm3,xmm2
	
		movups	[ecx+48],xmm3			// Store M[3][0-3]
		// Done computing fourth row.
		}
#else
		FMatrix Result;
		Result.M[0][0] = M[0][0] * Other.M[0][0] + M[0][1] * Other.M[1][0] + M[0][2] * Other.M[2][0] + M[0][3] * Other.M[3][0];
		Result.M[0][1] = M[0][0] * Other.M[0][1] + M[0][1] * Other.M[1][1] + M[0][2] * Other.M[2][1] + M[0][3] * Other.M[3][1];
		Result.M[0][2] = M[0][0] * Other.M[0][2] + M[0][1] * Other.M[1][2] + M[0][2] * Other.M[2][2] + M[0][3] * Other.M[3][2];
		Result.M[0][3] = M[0][0] * Other.M[0][3] + M[0][1] * Other.M[1][3] + M[0][2] * Other.M[2][3] + M[0][3] * Other.M[3][3];

		Result.M[1][0] = M[1][0] * Other.M[0][0] + M[1][1] * Other.M[1][0] + M[1][2] * Other.M[2][0] + M[1][3] * Other.M[3][0];
		Result.M[1][1] = M[1][0] * Other.M[0][1] + M[1][1] * Other.M[1][1] + M[1][2] * Other.M[2][1] + M[1][3] * Other.M[3][1];
		Result.M[1][2] = M[1][0] * Other.M[0][2] + M[1][1] * Other.M[1][2] + M[1][2] * Other.M[2][2] + M[1][3] * Other.M[3][2];
		Result.M[1][3] = M[1][0] * Other.M[0][3] + M[1][1] * Other.M[1][3] + M[1][2] * Other.M[2][3] + M[1][3] * Other.M[3][3];

		Result.M[2][0] = M[2][0] * Other.M[0][0] + M[2][1] * Other.M[1][0] + M[2][2] * Other.M[2][0] + M[2][3] * Other.M[3][0];
		Result.M[2][1] = M[2][0] * Other.M[0][1] + M[2][1] * Other.M[1][1] + M[2][2] * Other.M[2][1] + M[2][3] * Other.M[3][1];
		Result.M[2][2] = M[2][0] * Other.M[0][2] + M[2][1] * Other.M[1][2] + M[2][2] * Other.M[2][2] + M[2][3] * Other.M[3][2];
		Result.M[2][3] = M[2][0] * Other.M[0][3] + M[2][1] * Other.M[1][3] + M[2][2] * Other.M[2][3] + M[2][3] * Other.M[3][3];

		Result.M[3][0] = M[3][0] * Other.M[0][0] + M[3][1] * Other.M[1][0] + M[3][2] * Other.M[2][0] + M[3][3] * Other.M[3][0];
		Result.M[3][1] = M[3][0] * Other.M[0][1] + M[3][1] * Other.M[1][1] + M[3][2] * Other.M[2][1] + M[3][3] * Other.M[3][1];
		Result.M[3][2] = M[3][0] * Other.M[0][2] + M[3][1] * Other.M[1][2] + M[3][2] * Other.M[2][2] + M[3][3] * Other.M[3][2];
		Result.M[3][3] = M[3][0] * Other.M[0][3] + M[3][1] * Other.M[1][3] + M[3][2] * Other.M[2][3] + M[3][3] * Other.M[3][3];
		*this = Result;

#endif
	}

	// Comparison operators.

	inline UBOOL operator==(const FMatrix& Other) const
	{
		for(INT X = 0;X < 4;X++)
			for(INT Y = 0;Y < 4;Y++)
				if(M[X][Y] != Other.M[X][Y])
					return 0;

		return 1;
	}

	inline UBOOL operator!=(const FMatrix& Other) const
	{
		return !(*this == Other);
	}

	// Homogeneous transform.

	FORCEINLINE FPlane TransformFPlane(const FPlane &P) const
	{
		FPlane Result;

#if ASM
		__asm
		{
			mov		eax,[P]
			mov		ecx,[this]
			
			movups	xmm4,[ecx]			// M[0][0]
			movups	xmm5,[ecx+16]		// M[1][0]
			movups	xmm6,[ecx+32]		// M[2][0]
			movups	xmm7,[ecx+48]		// M[3][0]

			movss	xmm0,[eax]FVector.X
			shufps	xmm0,xmm0,0
			mulps	xmm0,xmm4

			movss	xmm1,[eax]FVector.Y
			shufps	xmm1,xmm1,0
			mulps	xmm1,xmm5

			movss	xmm2,[eax]FVector.Z
			shufps	xmm2,xmm2,0
			mulps	xmm2,xmm6

			addps	xmm0,xmm1

			movss	xmm3,[eax]FPlane.W
			shufps	xmm3,xmm3,0
			mulps	xmm3,xmm7
			
			// stall
			lea		eax,[Result]

			addps	xmm2,xmm3
			
			// stall

			addps	xmm0,xmm2
		
			movups	[eax],xmm0
		}
#else
		Result.X = P.X * M[0][0] + P.Y * M[1][0] + P.Z * M[2][0] + P.W * M[3][0];
		Result.Y = P.X * M[0][1] + P.Y * M[1][1] + P.Z * M[2][1] + P.W * M[3][1];
		Result.Z = P.X * M[0][2] + P.Y * M[1][2] + P.Z * M[2][2] + P.W * M[3][2];
		Result.W = P.X * M[0][3] + P.Y * M[1][3] + P.Z * M[2][3] + P.W * M[3][3];
#endif

		return Result;
	}

	// Regular transform.

	FORCEINLINE FVector TransformFVector(const FVector &V) const
	{
		return TransformFPlane(FPlane(V.X,V.Y,V.Z,1.0f));
	}

	FORCEINLINE FVector InverseTransformFVector(const FVector &V) const
	{
		FVector t, Result;

		t.X = V.X - M[3][0];
		t.Y = V.Y - M[3][1];
		t.Z = V.Z - M[3][2];

		Result.X = t.X * M[0][0] + t.Y * M[0][1] + t.Z * M[0][2];
		Result.Y = t.X * M[1][0] + t.Y * M[1][1] + t.Z * M[1][2];
		Result.Z = t.X * M[2][0] + t.Y * M[2][1] + t.Z * M[2][2];

		return Result;
	}

	// Normal transform.

	FORCEINLINE FPlane TransformNormal(const FVector& V) const
	{
		return TransformFPlane(FPlane(V.X,V.Y,V.Z,0.0f));
	}

	FORCEINLINE FVector InverseTransformNormal(const FVector &V) const
	{
		FVector Result;

		Result.X = V.X * M[0][0] + V.Y * M[0][1] + V.Z * M[0][2];
		Result.Y = V.X * M[1][0] + V.Y * M[1][1] + V.Z * M[1][2];
		Result.Z = V.X * M[2][0] + V.Y * M[2][1] + V.Z * M[2][2];

		return Result;
	}

	// Transpose.

	FORCEINLINE FMatrix Transpose()
	{
		FMatrix	Result;

		Result.M[0][0] = M[0][0];
		Result.M[0][1] = M[1][0];
		Result.M[0][2] = M[2][0];
		Result.M[0][3] = M[3][0];

		Result.M[1][0] = M[0][1];
		Result.M[1][1] = M[1][1];
		Result.M[1][2] = M[2][1];
		Result.M[1][3] = M[3][1];

		Result.M[2][0] = M[0][2];
		Result.M[2][1] = M[1][2];
		Result.M[2][2] = M[2][2];
		Result.M[2][3] = M[3][2];

		Result.M[3][0] = M[0][3];
		Result.M[3][1] = M[1][3];
		Result.M[3][2] = M[2][3];
		Result.M[3][3] = M[3][3];

		return Result;
	}

	// Determinant.

	inline FLOAT Determinant() const
	{
		return	M[0][0] * (
					M[1][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
					M[2][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) +
					M[3][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2])
					) -
				M[1][0] * (
					M[0][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
					M[2][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
					M[3][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2])
					) +
				M[2][0] * (
					M[0][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) -
					M[1][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
					M[3][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
					) -
				M[3][0] * (
					M[0][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2]) -
					M[1][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2]) +
					M[2][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
					);
	}

	// Inverse.

	FMatrix Inverse() const
	{
		FMatrix Result;
		FLOAT	Det = Determinant();

		if(Det == 0.0f)
			return FMatrix::Identity;

		FLOAT	RDet = 1.0f / Det;

		Result.M[0][0] = RDet * (
				M[1][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
				M[2][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) +
				M[3][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2])
				);
		Result.M[0][1] = -RDet * (
				M[0][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
				M[2][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
				M[3][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2])
				);
		Result.M[0][2] = RDet * (
				M[0][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) -
				M[1][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
				M[3][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
				);
		Result.M[0][3] = -RDet * (
				M[0][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2]) -
				M[1][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2]) +
				M[2][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
				);

		Result.M[1][0] = -RDet * (
				M[1][0] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
				M[2][0] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) +
				M[3][0] * (M[1][2] * M[2][3] - M[1][3] * M[2][2])
				);
		Result.M[1][1] = RDet * (
				M[0][0] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
				M[2][0] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
				M[3][0] * (M[0][2] * M[2][3] - M[0][3] * M[2][2])
				);
		Result.M[1][2] = -RDet * (
				M[0][0] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) -
				M[1][0] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
				M[3][0] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
				);
		Result.M[1][3] = RDet * (
				M[0][0] * (M[1][2] * M[2][3] - M[1][3] * M[2][2]) -
				M[1][0] * (M[0][2] * M[2][3] - M[0][3] * M[2][2]) +
				M[2][0] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
				);

		Result.M[2][0] = RDet * (
				M[1][0] * (M[2][1] * M[3][3] - M[2][3] * M[3][1]) -
				M[2][0] * (M[1][1] * M[3][3] - M[1][3] * M[3][1]) +
				M[3][0] * (M[1][1] * M[2][3] - M[1][3] * M[2][1])
				);
		Result.M[2][1] = -RDet * (
				M[0][0] * (M[2][1] * M[3][3] - M[2][3] * M[3][1]) -
				M[2][0] * (M[0][1] * M[3][3] - M[0][3] * M[3][1]) +
				M[3][0] * (M[0][1] * M[2][3] - M[0][3] * M[2][1])
				);
		Result.M[2][2] = RDet * (
				M[0][0] * (M[1][1] * M[3][3] - M[1][3] * M[3][1]) -
				M[1][0] * (M[0][1] * M[3][3] - M[0][3] * M[3][1]) +
				M[3][0] * (M[0][1] * M[1][3] - M[0][3] * M[1][1])
				);
		Result.M[2][3] = -RDet * (
				M[0][0] * (M[1][1] * M[2][3] - M[1][3] * M[2][1]) -
				M[1][0] * (M[0][1] * M[2][3] - M[0][3] * M[2][1]) +
				M[2][0] * (M[0][1] * M[1][3] - M[0][3] * M[1][1])
				);

		Result.M[3][0] = -RDet * (
				M[1][0] * (M[2][1] * M[3][2] - M[2][2] * M[3][1]) -
				M[2][0] * (M[1][1] * M[3][2] - M[1][2] * M[3][1]) +
				M[3][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1])
				);
		Result.M[3][1] = RDet * (
				M[0][0] * (M[2][1] * M[3][2] - M[2][2] * M[3][1]) -
				M[2][0] * (M[0][1] * M[3][2] - M[0][2] * M[3][1]) +
				M[3][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1])
				);
		Result.M[3][2] = -RDet * (
				M[0][0] * (M[1][1] * M[3][2] - M[1][2] * M[3][1]) -
				M[1][0] * (M[0][1] * M[3][2] - M[0][2] * M[3][1]) +
				M[3][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1])
				);
		Result.M[3][3] = RDet * (
				M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
				M[1][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1]) +
				M[2][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1])
				);

		return Result;
	}

	FMatrix TransposeAdjoint() const
	{
		FMatrix ta;

		ta.M[0][0] = this->M[1][1] * this->M[2][2] - this->M[1][2] * this->M[2][1];
		ta.M[0][1] = this->M[1][2] * this->M[2][0] - this->M[1][0] * this->M[2][2];
		ta.M[0][2] = this->M[1][0] * this->M[2][1] - this->M[1][1] * this->M[2][0];
		ta.M[0][3] = 0.f;

		ta.M[1][0] = this->M[2][1] * this->M[0][2] - this->M[2][2] * this->M[0][1];
		ta.M[1][1] = this->M[2][2] * this->M[0][0] - this->M[2][0] * this->M[0][2];
		ta.M[1][2] = this->M[2][0] * this->M[0][1] - this->M[2][1] * this->M[0][0];
		ta.M[1][3] = 0.f;

		ta.M[2][0] = this->M[0][1] * this->M[1][2] - this->M[0][2] * this->M[1][1];
		ta.M[2][1] = this->M[0][2] * this->M[1][0] - this->M[0][0] * this->M[1][2];
		ta.M[2][2] = this->M[0][0] * this->M[1][1] - this->M[0][1] * this->M[1][0];
		ta.M[2][3] = 0.f;

		ta.M[3][0] = 0.f;
		ta.M[3][1] = 0.f;
		ta.M[3][2] = 0.f;
		ta.M[3][3] = 1.f;

		return ta;
	}

	// Remove any scaling from this matrix (ie magnitude of each row is 1)
	void RemoveScaling()
	{
		FLOAT SquareSum, Scale;

		// For each row, find magnitude, and if its non-zero re-scale so its unit length.
		for(INT i=0; i<3; i++)
		{
			SquareSum = (M[i][0] * M[i][0]) + (M[i][1] * M[i][1]) + (M[i][2] * M[i][2]);
			if(SquareSum > SMALL_NUMBER)
			{
				Scale = 1.f/appSqrt(SquareSum);
				M[i][0] *= Scale; 
				M[i][1] *= Scale; 
				M[i][2] *= Scale; 
			}
		}
	}


	void ScaleTranslation(const FVector& Scale3D)
	{
		M[3][0] *= Scale3D.X;
		M[3][1] *= Scale3D.Y;
		M[3][2] *= Scale3D.Z;
	}

	// GetOrigin

	FVector GetOrigin() const
	{
		return FVector(M[3][0],M[3][1],M[3][2]);
	}

	FVector GetAxis(INT i) const
	{
		checkSlow(i >= 0 && i <= 2);
		return FVector(M[i][0], M[i][1], M[i][2]);

	}

	void SetAxis( INT i, const FVector& Axis )
	{
		checkSlow(i >= 0 && i <= 2);
		M[i][0] = Axis.X;
		M[i][1] = Axis.Y;
		M[i][2] = Axis.Z;
	}

	void SetOrigin( const FVector& NewOrigin )
	{
		M[3][0] = NewOrigin.X;
		M[3][1] = NewOrigin.Y;
		M[3][2] = NewOrigin.Z;
	}

	FRotator Rotator() const;

	// Frustum plane extraction.

	static UBOOL MakeFrustumPlane(FLOAT A,FLOAT B,FLOAT C,FLOAT D,FPlane& OutPlane)
	{
		FLOAT	LengthSquared = A * A + B * B + C * C;
		if(LengthSquared > DELTA*DELTA)
		{
			FLOAT	InvLength = 1.0f / appSqrt(LengthSquared);
			OutPlane = FPlane(-A * InvLength,-B * InvLength,-C * InvLength,D * InvLength);
			return 1;
		}
		else
			return 0;
	}

	UBOOL GetFrustumNearPlane(FPlane& OutPlane) const
	{
		return MakeFrustumPlane(
			M[0][2],
			M[1][2],
			M[2][2],
			M[3][2],
			OutPlane
			);
	}

	UBOOL GetFrustumFarPlane(FPlane& OutPlane) const
	{
		return MakeFrustumPlane(
			M[0][3] - M[0][2],
			M[1][3] - M[1][2],
			M[2][3] - M[2][2],
			M[3][3] - M[3][2],
			OutPlane
			);
	}

	UBOOL GetFrustumLeftPlane(FPlane& OutPlane) const
	{
		return MakeFrustumPlane(
			M[0][3] + M[0][0],
			M[1][3] + M[1][0],
			M[2][3] + M[2][0],
			M[3][3] + M[3][0],
			OutPlane
			);
	}

	UBOOL GetFrustumRightPlane(FPlane& OutPlane) const
	{
		return MakeFrustumPlane(
			M[0][3] - M[0][0],
			M[1][3] - M[1][0],
			M[2][3] - M[2][0],
			M[3][3] - M[3][0],
			OutPlane
			);
	}

	UBOOL GetFrustumTopPlane(FPlane& OutPlane) const
	{
		return MakeFrustumPlane(
			M[0][3] - M[0][1],
			M[1][3] - M[1][1],
			M[2][3] - M[2][1],
			M[3][3] - M[3][1],
			OutPlane
			);
	}

	UBOOL GetFrustumBottomPlane(FPlane& OutPlane) const
	{
		return MakeFrustumPlane(
			M[0][3] + M[0][1],
			M[1][3] + M[1][1],
			M[2][3] + M[2][1],
			M[3][3] + M[3][1],
			OutPlane
			);
	}

	// Serializer.
	friend FArchive& operator<<(FArchive& Ar,FMatrix& M)
	{
		return Ar << 
			M.M[0][0] << M.M[0][1] << M.M[0][2] << M.M[0][3] << 
			M.M[1][0] << M.M[1][1] << M.M[1][2] << M.M[1][3] << 
			M.M[2][0] << M.M[2][1] << M.M[2][2] << M.M[2][3] << 
			M.M[3][0] << M.M[3][1] << M.M[3][2] << M.M[3][3];
	}
};

// Matrix operations.

class FPerspectiveMatrix : public FMatrix
{
public:

	FPerspectiveMatrix(float FOVX, float FOVY, float MultFOVX, float MultFOVY, float MinZ, float MaxZ) :
	  FMatrix(
			FPlane(MultFOVX / appTan(FOVX),		0.0f,							0.0f,							0.0f),
			FPlane(0.0f,						MultFOVY / appTan(FOVY),		0.0f,							0.0f),
			FPlane(0.0f,						0.0f,							MaxZ / (MaxZ - MinZ),			1.0f),
			FPlane(0.0f,						0.0f,							-MinZ * (MaxZ / (MaxZ - MinZ)),	0.0f))
	{
	}

	FPerspectiveMatrix(float FOV, float Width, float Height, float MinZ, float MaxZ) :
		FMatrix(
			FPlane(1.0f / appTan(FOV),	0.0f,							0.0f,							0.0f),
			FPlane(0.0f,				Width / appTan(FOV) / Height,	0.0f,							0.0f),
			FPlane(0.0f,				0.0f,							MaxZ / (MaxZ - MinZ),			1.0f),
			FPlane(0.0f,				0.0f,							-MinZ * (MaxZ / (MaxZ - MinZ)),	0.0f))
	{
	}

#define Z_PRECISION	0.001f

	FPerspectiveMatrix(float FOV, float Width, float Height, float MinZ) :
		FMatrix(
			FPlane(1.0f / appTan(FOV),	0.0f,							0.0f,							0.0f),
			FPlane(0.0f,				Width / appTan(FOV) / Height,	0.0f,							0.0f),
			FPlane(0.0f,				0.0f,							(1.0f - Z_PRECISION),			1.0f),
			FPlane(0.0f,				0.0f,							-MinZ * (1.0f - Z_PRECISION),	0.0f))
	{
	}
};

class FOrthoMatrix : public FMatrix
{
public:

	FOrthoMatrix(float Width,float Height,float ZScale,float ZOffset) :
		FMatrix(
			FPlane(1.0f / Width,	0.0f,			0.0f,				0.0f),
			FPlane(0.0f,			1.0f / Height,	0.0f,				0.0f),
			FPlane(0.0f,			0.0f,			ZScale,				0.0f),
			FPlane(0.0f,			0.0f,			ZOffset * ZScale,	1.0f))
	{
	}
};

class FTranslationMatrix : public FMatrix
{
public:

	FTranslationMatrix(FVector Delta) :
		FMatrix(
			FPlane(1.0f,	0.0f,	0.0f,	0.0f),
			FPlane(0.0f,	1.0f,	0.0f,	0.0f),
			FPlane(0.0f,	0.0f,	1.0f,	0.0f),
			FPlane(Delta.X,	Delta.Y,Delta.Z,1.0f))
	{
	}
};

class FRotationMatrix : public FMatrix
{
public:

#if 0
	FRotationMatrix(FRotator Rot) :
		FMatrix(
			Identity *
			FMatrix(	// Roll
				FPlane(1.0f,					0.0f,					0.0f,						0.0f),
				FPlane(0.0f,					+GMath.CosTab(Rot.Roll),-GMath.SinTab(Rot.Roll),	0.0f),
				FPlane(0.0f,					+GMath.SinTab(Rot.Roll),+GMath.CosTab(Rot.Roll),	0.0f),
				FPlane(0.0f,					0.0f,					0.0f,						1.0f)) *
			FMatrix(	// Pitch
				FPlane(+GMath.CosTab(Rot.Pitch),0.0f,					+GMath.SinTab(Rot.Pitch),	0.0f),
				FPlane(0.0f,					1.0f,					0.0f,						0.0f),
				FPlane(-GMath.SinTab(Rot.Pitch),0.0f,					+GMath.CosTab(Rot.Pitch),	0.0f),
				FPlane(0.0f,					0.0f,					0.0f,						1.0f)) *
			FMatrix(	// Yaw
				FPlane(+GMath.CosTab(Rot.Yaw),	+GMath.SinTab(Rot.Yaw), 0.0f,	0.0f),
				FPlane(-GMath.SinTab(Rot.Yaw),	+GMath.CosTab(Rot.Yaw), 0.0f,	0.0f),
				FPlane(0.0f,					0.0f,					1.0f,	0.0f),
				FPlane(0.0f,					0.0f,					0.0f,	1.0f))
			)
	{
	}
#else
	FRotationMatrix(FRotator Rot)
	{
		FLOAT	SR	= GMath.SinTab(Rot.Roll),
				SP	= GMath.SinTab(Rot.Pitch),
				SY	= GMath.SinTab(Rot.Yaw),
				CR	= GMath.CosTab(Rot.Roll),
				CP	= GMath.CosTab(Rot.Pitch),
				CY	= GMath.CosTab(Rot.Yaw);

		M[0][0]	= CP * CY;
		M[0][1]	= CP * SY;
		M[0][2]	= SP;
		M[0][3]	= 0.f;

		M[1][0]	= SR * SP * CY - CR * SY;
		M[1][1]	= SR * SP * SY + CR * CY;
		M[1][2]	= - SR * CP;
		M[1][3]	= 0.f;

		M[2][0]	= -( CR * SP * CY + SR * SY );
		M[2][1]	= CY * SR - CR * SP * SY;
		M[2][2]	= CR * CP;
		M[2][3]	= 0.f;

		M[3][0]	= 0.f;
		M[3][1]	= 0.f;
		M[3][2]	= 0.f;
		M[3][3]	= 1.f;
	}
#endif
};

class FInverseRotationMatrix : public FMatrix
{
public:

	FInverseRotationMatrix(FRotator Rot) :
		FMatrix(
			FMatrix(	// Yaw
				FPlane(+GMath.CosTab(-Rot.Yaw),	+GMath.SinTab(-Rot.Yaw), 0.0f,	0.0f),
				FPlane(-GMath.SinTab(-Rot.Yaw),	+GMath.CosTab(-Rot.Yaw), 0.0f,	0.0f),
				FPlane(0.0f,					0.0f,					1.0f,	0.0f),
				FPlane(0.0f,					0.0f,					0.0f,	1.0f)) *
			FMatrix(	// Pitch
				FPlane(+GMath.CosTab(-Rot.Pitch),0.0f,					+GMath.SinTab(-Rot.Pitch),	0.0f),
				FPlane(0.0f,					1.0f,					0.0f,						0.0f),
				FPlane(-GMath.SinTab(-Rot.Pitch),0.0f,					+GMath.CosTab(-Rot.Pitch),	0.0f),
				FPlane(0.0f,					0.0f,					0.0f,						1.0f)) *
			FMatrix(	// Roll
				FPlane(1.0f,					0.0f,					0.0f,						0.0f),
				FPlane(0.0f,					+GMath.CosTab(-Rot.Roll),-GMath.SinTab(-Rot.Roll),	0.0f),
				FPlane(0.0f,					+GMath.SinTab(-Rot.Roll),+GMath.CosTab(-Rot.Roll),	0.0f),
				FPlane(0.0f,					0.0f,					0.0f,						1.0f))
			)
	{
	}
};

class FScaleMatrix : public FMatrix
{
public:

	FScaleMatrix(FVector Scale) :
		FMatrix(
			FPlane(Scale.X,	0.0f,		0.0f,		0.0f),
			FPlane(0.0f,	Scale.Y,	0.0f,		0.0f),
			FPlane(0.0f,	0.0f,		Scale.Z,	0.0f),
			FPlane(0.0f,	0.0f,		0.0f,		1.0f))
	{
	}
};

//
//	FRangeMapMatrix - Maps points from a bounded range to another bounded range.
//

struct FRangeMapMatrix: FMatrix
{
	FRangeMapMatrix(const FVector& MinA,const FVector& MaxA,const FVector& MinB,const FVector& MaxB)
	{
		for(UINT RowIndex = 0;RowIndex < 4;RowIndex++)
			for(UINT ColumnIndex = 0;ColumnIndex < 4;ColumnIndex++)
				M[RowIndex][ColumnIndex] = 0.0f;
		M[3][3] = 1.0f;

		for(UINT ComponentIndex = 0;ComponentIndex < 3;ComponentIndex++)
		{
			FLOAT	RangeA = (&MaxA.X)[ComponentIndex] - (&MinA.X)[ComponentIndex],
				RangeB = (&MaxB.X)[ComponentIndex] - (&MinB.X)[ComponentIndex];

			M[ComponentIndex][ComponentIndex] = RangeB / RangeA;
			M[3][ComponentIndex] = ((&MinB.X)[ComponentIndex] - (&MinA.X)[ComponentIndex]) * M[ComponentIndex][ComponentIndex];
		}
	}
};

//
//	FBasisVectorMatrix
//

struct FBasisVectorMatrix: FMatrix
{
	FBasisVectorMatrix(const FVector& XAxis,const FVector& YAxis,const FVector& ZAxis,const FVector& Origin)
	{
		for(UINT RowIndex = 0;RowIndex < 3;RowIndex++)
		{
			M[RowIndex][0] = (&XAxis.X)[RowIndex];
			M[RowIndex][1] = (&YAxis.X)[RowIndex];
			M[RowIndex][2] = (&ZAxis.X)[RowIndex];
			M[RowIndex][3] = 0.0f;
		}
		M[3][0] = Origin | XAxis;
		M[3][1] = Origin | YAxis;
		M[3][2] = Origin | ZAxis;
		M[3][3] = 1.0f;
	}
};

//
//	FBox::TransformBy
//

inline FBox FBox::TransformBy(const FMatrix& M) const
{
	FBox	NewBox(0);

	for(INT X = 0;X < 2;X++)
		for(INT Y = 0;Y < 2;Y++)
			for(INT Z = 0;Z < 2;Z++)
				NewBox += M.TransformFVector(FVector(GetExtrema(X).X,GetExtrema(Y).Y,GetExtrema(Z).Z));

	return NewBox;
}


/*-----------------------------------------------------------------------------
	FPlane implementation.
-----------------------------------------------------------------------------*/

//
// Transform a point by a coordinate system, moving
// it by the coordinate system's origin if nonzero.
//
inline FPlane FPlane::TransformPlaneByOrtho( const FMatrix& M ) const
{
	FVector Normal = M.TransformFPlane(FPlane(X,Y,Z,0));
	return FPlane( Normal, W - (M.TransformFVector(FVector(0,0,0)) | Normal) );
}

inline FPlane FPlane::TransformBy( const FMatrix& M ) const
{
	FMatrix tmpTA = M.TransposeAdjoint();
	float DetM = M.Determinant();
	return this->TransformByUsingAdjointT(M, DetM, tmpTA);
}

// You can optionally pass in the matrices transpose-adjoint, which save it recalculating it.
// MSM: If we are going to save the transpose-adjoint we should also save the more expensive
// determinant.
inline FPlane FPlane::TransformByUsingAdjointT( const FMatrix& M, float DetM, const FMatrix& TA ) const
{
	FVector newNorm = TA.TransformNormal(*this).SafeNormal();

	if(DetM < 0.f)
		newNorm *= -1.0f;

	return FPlane(M.TransformFVector(*this * W), newNorm);
}

inline FSphere FSphere::TransformBy(const FMatrix& M) const
{
	FSphere	Result;

	(FVector&)Result = M.TransformFVector(*this);

	FVector	XAxis(M.M[0][0],M.M[0][1],M.M[0][2]),
			YAxis(M.M[1][0],M.M[1][1],M.M[1][2]),
			ZAxis(M.M[2][0],M.M[2][1],M.M[2][2]);

	Result.W = appSqrt(Max(XAxis|XAxis,Max(YAxis|YAxis,ZAxis|ZAxis))) * W;

	return Result;
}

inline FBoxSphereBounds FBoxSphereBounds::TransformBy(const FMatrix& M) const
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

	FVector	XAxis(M.M[0][0],M.M[0][1],M.M[0][2]),
			YAxis(M.M[1][0],M.M[1][1],M.M[1][2]),
			ZAxis(M.M[2][0],M.M[2][1],M.M[2][2]);

	Result.SphereRadius = appSqrt(Max(XAxis|XAxis,Max(YAxis|YAxis,ZAxis|ZAxis))) * SphereRadius;

	return Result;
}

struct FCompressedPosition
{
	FVector Location;
	FRotator Rotation;
	FVector Velocity;
};

/*-----------------------------------------------------------------------------
	FComplex implementation.
-----------------------------------------------------------------------------*/

// Complex number
struct FComplex
{
	FLOAT Real, Imag;
	
	FComplex(): Real(0.0), Imag(0.0) { }
	FComplex(const FComplex& InComplex): Real(InComplex.Real), Imag(InComplex.Imag) { }
	FComplex(FLOAT InReal, FLOAT InImag = 0.0): Real(InReal), Imag(InImag) { }

	// Unit imaginary
	static FComplex I() { return FComplex(0.0, 1.0); }

	// Magnitude
	FLOAT Mag() const
	{
		return appSqrt(Real * Real + Imag * Imag);
	}

	// Squared magnitude
	FLOAT SqrMag() const
	{
		return (Real * Real + Imag * Imag);
	}

	// Argument
	FLOAT Arg() const
	{
		return appAtan2(Imag, Real);
	}

	// Conjugate
	FComplex Conj() const
	{
		FComplex Result(*this);
		Result.Imag = -Result.Imag;
		return Result;
	}

	FComplex& operator*=(FLOAT Scalar)
	{
		Real *= Scalar;
		Imag *= Scalar;
		return *this;
	}

	FComplex operator*(FLOAT Scalar) const
	{
		FComplex Result(*this);
		Result *= Scalar;
		return Result;
	}

	FComplex& operator/=(FLOAT Scalar)
	{
		Real /= Scalar;
		Imag /= Scalar;
		return *this;
	}

	FComplex operator/(FLOAT Scalar) const
	{
		FComplex Result(*this);
		Result /= Scalar;
		return Result;
	}

	FComplex& operator+=(const FComplex& Term)
	{
		Real += Term.Real;
		Imag += Term.Imag;
		return *this;
	}

	FComplex operator+(const FComplex& Term) const
	{
		FComplex Result(*this);
		Result += Term;
		return Result;
	}

	FComplex& operator-=(const FComplex& Term)
	{
		Real -= Term.Real;
		Imag -= Term.Imag;
		return *this;
	}

	FComplex operator-(const FComplex& Term) const
	{
		FComplex Result(*this);
		Result -= Term;
		return Result;
	}

	FComplex& operator*=(const FComplex& Term)
	{
		FLOAT OldReal = Real;
		Real = Real * Term.Real - Imag * Term.Imag;
		Imag = OldReal * Term.Imag + Imag * Term.Real;
		return *this;
	}

	FComplex operator*(const FComplex& Term) const
	{
		FComplex Result(*this);
		Result *= Term;
		return Result;
	}

	FComplex& operator/=(const FComplex& Term)
	{
		*this *= Term.Conj();
		*this /= Term.Mag();
		return *this;
	}

	FComplex operator/(const FComplex& Term) const
	{
		FComplex Result(*this);
		Result /= Term;
		return Result;
	}
};

FComplex Exp(const FComplex& Exponent);

/*-----------------------------------------------------------------------------
	Fourier transform
-----------------------------------------------------------------------------*/

void FourierTransform(const TArray<FComplex>& Input, TArray<FComplex>& Output, UINT LowerIndex, UINT Length);
void InverseFourierTransform(const TArray<FComplex>& Input, TArray<FComplex>& Output, UINT LowerIndex, UINT Length);

void FourierTransform2D(const TArray<FComplex>& Input, TArray<FComplex>& Output, UINT NumRows, UINT NumColumns);
void InverseFourierTransform2D(const TArray<FComplex>& Input, TArray<FComplex>& Output, UINT NumRows, UINT NumColumns);

/*-----------------------------------------------------------------------------
	Probability distributions
-----------------------------------------------------------------------------*/

FLOAT GaussianRandom(FLOAT Mean, FLOAT StandardDeviation);


/*-----------------------------------------------------------------------------
FInterpCurve.
-----------------------------------------------------------------------------*/

template< class T, class U > void AutoCalcTangent( const T& PrevP, const T& P, const T& NextP, const U& Tension, T& OutTan )
{
	OutTan = 0.5f * (1.f - Tension) * ( (P - PrevP) + (NextP - P) );
}

//////////////////////////////////////////////////////////////////////////
// Support for InterpCurves of Quaternions

template< class U > FQuat Lerp( const FQuat& A, const FQuat& B, const U& Alpha)
{
	return SlerpQuat(A, B, Alpha);
}

// In the case of quaternions, we use a bezier like approach.
// T - Actual 'control' orientations.
template< class U > FQuat CubicInterp( const FQuat& P0, const FQuat& T0, const FQuat& P1, const FQuat& T1, const U& A)
{
	return SquadQuat(P0, T0, P1, T1, A);
}

void CalcQuatTangents( const FQuat& PrevP, const FQuat& P, const FQuat& NextP, FLOAT Tension, FQuat& OutTan );


// This actual returns the control point not a tangent. This is expected by the CubicInterp function for Quaternions above.
template< class U > void AutoCalcTangent( const FQuat& PrevP, const FQuat& P, const FQuat& NextP, const U& Tension, FQuat& OutTan  )
{
	CalcQuatTangents(PrevP, P, NextP, Tension, OutTan);
}
//////////////////////////////////////////////////////////////////////////


enum EInterpCurveMode
{
	/** A straight line between two keypoint values. */
	CIM_Linear,

	/** A cubic-hermite curve between two keypoints, using Arrive/Leave tangents. These tangents will be automatically updated when points are moved etc. */
	CIM_CurveAuto,

	/** The out value is held constant until the next key, then will jump to that value. */
	CIM_Constant,

	/** A smooth curve just like CIM_Curve, but tangents are not automatically updated so you can have manual control over them (eg. in Curve Editor). */
	CIM_CurveUser,

	/** A curve like CIM_Curve, but the arrive and leave tangents are not forced to be the same, so you can create a 'corner' at this key. */
	CIM_CurveBreak,

	/** Invalid or unknown curve type. */
	CIM_Unknown
};

template< class T > class FInterpCurvePoint
{
public:
	/** Float input value that corresponds to this key (eg. time). */
	FLOAT		InVal;

	/** Output value of templated type when input is equal to InVal. */
	T			OutVal;

	/** Tangent of curve arrive this point. */
	T			ArriveTangent; 

	/** Tangent of curve leaving this point. */
	T			LeaveTangent; 

	/** Interpolation mode between this point and the next one. @see EInterpCurveMode */
	BYTE		InterpMode; 

	FInterpCurvePoint() {}

	FInterpCurvePoint(const FLOAT In, const T Out) : 
	InVal(In), 
		OutVal(Out)
	{
		appMemset( &ArriveTangent, 0, sizeof(T) );	
		appMemset( &LeaveTangent, 0, sizeof(T) );

		InterpMode = CIM_Linear;
	}

	FInterpCurvePoint(const FLOAT In, const T Out, const T InArriveTangent, const T InLeaveTangent, const EInterpCurveMode InInterpMode) : 
	InVal(In), 
		OutVal(Out), 
		ArriveTangent(InArriveTangent),
		LeaveTangent(InLeaveTangent),
		InterpMode(InInterpMode)
	{
	}

	FORCEINLINE UBOOL IsCurveKey() const
	{
		return ((InterpMode == CIM_CurveAuto) || (InterpMode == CIM_CurveUser) || (InterpMode == CIM_CurveBreak));
	}


	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FInterpCurvePoint& Point )
	{
		Ar << Point.InVal << Point.OutVal;
		Ar << Point.ArriveTangent << Point.LeaveTangent;
		Ar << InterpMode;

		return Ar;
	}
};


void CurveFloatFindIntervalBounds( const FInterpCurvePoint<FLOAT>& Start, const FInterpCurvePoint<FLOAT>& End, FLOAT& CurrentMin, FLOAT& CurrentMax );
void CurveVectorFindIntervalBounds( const FInterpCurvePoint<FVector>& Start, const FInterpCurvePoint<FVector>& End, FVector& CurrentMin, FVector& CurrentMax );

template< class T, class U > void CurveFindIntervalBounds( const FInterpCurvePoint<T>& Start, const FInterpCurvePoint<T>& End, T& CurrentMin, T& CurrentMax, const U& Dummy )
{ }

template< class U > void CurveFindIntervalBounds( const FInterpCurvePoint<FLOAT>& Start, const FInterpCurvePoint<FLOAT>& End, FLOAT& CurrentMin, FLOAT& CurrentMax, const U& Dummy )
{
	CurveFloatFindIntervalBounds(Start, End, CurrentMin, CurrentMax);
}

template< class U > void CurveFindIntervalBounds( const FInterpCurvePoint<FVector>& Start, const FInterpCurvePoint<FVector>& End, FVector& CurrentMin, FVector& CurrentMax, const U& Dummy )
{
	CurveVectorFindIntervalBounds(Start, End, CurrentMin, CurrentMax);
}


template< class T > class FInterpCurve
{
public:
	TArrayNoInit< FInterpCurvePoint<T> >	Points;

	/** Add a new keypoint to the InterpCurve with the supplied In and Out value. Returns the index of the new key.*/
	INT AddPoint( const FLOAT InVal, const T OutVal )
	{
		INT i=0; for( i=0; i<Points.Num() && Points(i).InVal < InVal; i++);
		Points.Insert(i);
		Points(i) = FInterpCurvePoint< T >(InVal, OutVal);
		return i;
	}

	/** Move a keypoint to a new In value. This may change the index of the keypoint, so the new key index is returned. */
	INT MovePoint( INT PointIndex, FLOAT NewInVal )
	{
		if( PointIndex < 0 || PointIndex >= Points.Num() )
			return PointIndex;

		T OutVal = Points(PointIndex).OutVal;
		BYTE Mode = Points(PointIndex).InterpMode;
		T ArriveTan = Points(PointIndex).ArriveTangent;
		T LeaveTan = Points(PointIndex).LeaveTangent;

		Points.Remove(PointIndex);

		INT NewPointIndex = AddPoint( NewInVal, OutVal );
		Points(NewPointIndex).InterpMode = Mode;
		Points(NewPointIndex).ArriveTangent = ArriveTan;
		Points(NewPointIndex).LeaveTangent = LeaveTan;

		return NewPointIndex;
	}

	/** Clear all keypoints from InterpCurve. */
	void Reset()
	{
		Points.Empty();
	}

	/** 
	 *	Evaluate the output for an arbitary input value. 
	 *	For inputs outside the range of the keys, the first/last key value is assumed.
	 */
	T Eval( const FLOAT InVal, const T& Default ) const
	{
		INT NumPoints = Points.Num();

		// If no point in curve, return the Default value we passed in.
		if( NumPoints == 0 )
		{
			return Default;
		}

		// If only one point, or before the first point in the curve, return the first points value.
		if( NumPoints < 2 || (InVal <= Points(0).InVal) )
		{
			return Points(0).OutVal;
		}

		// If beyond the last point in the curve, return its value.
		if( InVal >= Points(NumPoints-1).InVal )
		{
			return Points(NumPoints-1).OutVal;
		}

		// Somewhere with curve range - linear search to find value.
		for( INT i=1; i<NumPoints; i++ )
		{	
			if( InVal < Points(i).InVal )
			{
				FLOAT Diff = Points(i).InVal - Points(i-1).InVal;

				if( Diff > 0.f && Points(i-1).InterpMode != CIM_Constant )
				{
					FLOAT Alpha = (InVal - Points(i-1).InVal) / Diff;

					if( Points(i-1).InterpMode == CIM_Linear )
					{
						return Lerp( Points(i-1).OutVal, Points(i).OutVal, Alpha );
					}
					else
					{
						return CubicInterp( Points(i-1).OutVal, Points(i-1).LeaveTangent, Points(i).OutVal, Points(i).ArriveTangent, Alpha );
					}
				}
				else
				{
					return Points(i-1).OutVal;
				}
			}
		}

		// Shouldn't really reach here.
		return Points(NumPoints-1).OutVal;
	}

	void AutoSetTangents(FLOAT Tension = 0.f)
	{
		// Iterate over all points in this InterpCurve
		for(INT PointIndex=0; PointIndex<Points.Num(); PointIndex++)
		{
			T ArriveTangent = Points(PointIndex).ArriveTangent;
			T LeaveTangent = Points(PointIndex).LeaveTangent;

			if(PointIndex == 0)
			{
				if(PointIndex < Points.Num()-1) // Start point
				{
					// If first section is not a curve, or is a curve and first point has manual tangent setting.
					if( Points(PointIndex).InterpMode == CIM_CurveAuto )
					{
						appMemset( &LeaveTangent, 0, sizeof(T) );
					}
				}
				else // Only point
				{
					appMemset( &LeaveTangent, 0, sizeof(T) );
				}
			}
			else
			{
				if(PointIndex < Points.Num()-1) // Inner point
				{
					if( Points(PointIndex-1).IsCurveKey() && Points(PointIndex).IsCurveKey() && Points(PointIndex).InterpMode == CIM_CurveAuto )
					{
						AutoCalcTangent( Points(PointIndex-1).OutVal, Points(PointIndex).OutVal, Points(PointIndex+1).OutVal, Tension, ArriveTangent );
						LeaveTangent = ArriveTangent;
					}
				}
				else // End point
				{
					// If last section is not a curve, or is a curve and final point has manual tangent setting.
					if( Points(PointIndex).InterpMode == CIM_CurveAuto )
					{
						appMemset( &ArriveTangent, 0, sizeof(T) );
					}
				}
			}

			Points(PointIndex).ArriveTangent = ArriveTangent;
			Points(PointIndex).LeaveTangent = LeaveTangent;
		}
	}

	/** Calculate the min/max out value that can be returned by this InterpCurve. */
	void CalcBounds(T& OutMin, T& OutMax, const T& Default) const
	{
		if(Points.Num() == 0)
		{
			OutMin = Default;
			OutMax = Default;
		}
		else if(Points.Num() == 1)
		{
			OutMin = Points(0).OutVal;
			OutMax = Points(0).OutVal;
		}
		else
		{
			OutMin = Points(0).OutVal;
			OutMax = Points(0).OutVal;

			for(INT i=1; i<Points.Num(); i++)
			{
				CurveFindIntervalBounds( Points(i-1), Points(i), OutMin, OutMax, 0.f );
			}
		}
	}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FInterpCurve& Curve )
	{
		return Ar << Curve.Points;
	}

	// Assignment (copy)
	void operator=(const FInterpCurve &Other)
	{
		Points = Other.Points;
	}
};

template< class T > class FInterpCurveInit : public FInterpCurve< T >
{
public:
	FInterpCurveInit()
	{
		appMemzero( &Points, sizeof(Points) );
	}
};

typedef FInterpCurve<FLOAT>				FInterpCurveFloat;
typedef FInterpCurve<FVector>			FInterpCurveVector;
typedef FInterpCurve<FQuat>				FInterpCurveQuat;

typedef FInterpCurveInit<FLOAT>			FInterpCurveInitFloat;
typedef FInterpCurveInit<FVector>		FInterpCurveInitVector;
typedef FInterpCurveInit<FQuat>			FInterpCurveInitQuat;

/*-----------------------------------------------------------------------------
	FCurveEdInterface
-----------------------------------------------------------------------------*/


/** Interface that allows the CurveEditor to edit this type of object. */
class FCurveEdInterface
{
public:
	/** Get number of keyframes in curve. */
	virtual INT		GetNumKeys() { return 0; }

	/** Get number of 'sub curves' in this Curve. For example, a vector curve will have 3 sub-curves, for X, Y and Z. */
	virtual INT		GetNumSubCurves() { return 0; }

	/** Get the input value for the Key with the specified index. KeyIndex must be within range ie >=0 and < NumKeys. */
	virtual FLOAT	GetKeyIn(INT KeyIndex) { return 0.f; }

	/** 
	 *	Get the output value for the key with the specified index on the specified sub-curve. 
	 *	SubIndex must be within range ie >=0 and < NumSubCurves.
	 *	KeyIndex must be within range ie >=0 and < NumKeys.
	 */
	virtual FLOAT	GetKeyOut(INT SubIndex, INT KeyIndex) { return 0.f; }

	/** Allows the Curve to specify a color for each keypoint. */
	virtual FColor	GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor) { return CurveColor; }

	/** Evaluate a subcurve at an arbitary point. Outside the keyframe range, curves are assumed to continue their end values. */
	virtual FLOAT	EvalSub(INT SubIndex, FLOAT InVal) { return 0.f; }

	/** 
	 *	Get the interpolation mode of the specified keyframe. This can be CIM_Constant, CIM_Linear or CIM_Curve. 
	 *	KeyIndex must be within range ie >=0 and < NumKeys.
	 */
	virtual BYTE	GetKeyInterpMode(INT KeyIndex) { return CIM_Linear; }

	/** 
	 *	Get the incoming and outgoing tangent for the given subcurve and key.
	 *	SubIndex must be within range ie >=0 and < NumSubCurves.
	 *	KeyIndex must be within range ie >=0 and < NumKeys.
	 */
	virtual void	GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent) { ArriveTangent=0.f; LeaveTangent=0.f; }

	/** Get input range of keys. Outside this region curve continues constantly the start/end values. */
	virtual void	GetInRange(FLOAT& MinIn, FLOAT& MaxIn) { MinIn=0.f; MaxIn=0.f; }

	/** Get overall range of output values. */
	virtual void	GetOutRange(FLOAT& MinOut, FLOAT& MaxOut) { MinOut=0.f; MaxOut=0.f; }

	/** 
	 *	Add a new key to the curve with the specified input. Its initial value is set using EvalSub at that location. 
	 *	Returns the index of the new key.
	 */
	virtual INT		CreateNewKey(FLOAT KeyIn) { return INDEX_NONE; }

	/** 
	 *	Remove the specified key from the curve.
	 *	KeyIndex must be within range ie >=0 and < NumKeys.
	 */
	virtual void	DeleteKey(INT KeyIndex) {}

	/** 
	 *	Set the input value of the specified Key. This may change the index of the key, so the new index of the key is retured. 
	 *	KeyIndex must be within range ie >=0 and < NumKeys.
	 */
	virtual INT		SetKeyIn(INT KeyIndex, FLOAT NewInVal) { return KeyIndex; }

	/** 
	 *	Set the output values of the specified key.
	 *	SubIndex must be within range ie >=0 and < NumSubCurves.
	 *	KeyIndex must be within range ie >=0 and < NumKeys.
	 */
	virtual void	SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal) {}

	/** 
	 *	Set the method to use for interpolating between the give keyframe and the next one.
	 *	KeyIndex must be within range ie >=0 and < NumKeys.
	 */
	virtual void	SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode) {}


	/** 
	 *	Set the incoming and outgoing tangent for the given subcurve and key.
	 *	SubIndex must be within range ie >=0 and < NumSubCurves.
	 *	KeyIndex must be within range ie >=0 and < NumKeys.
	 */
	virtual void	SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent) {}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

