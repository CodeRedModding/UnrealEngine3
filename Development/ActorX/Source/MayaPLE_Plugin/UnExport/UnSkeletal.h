/*=============================================================================

	UnSkeletal.h: General ActorXporter support functions, misc math classes ripped-out from Unreal.

    Copyright (c) 1999,2000 Epic MegaGames, Inc. All Rights Reserved.

=============================================================================*/

// Dirty subset of Unreal's skeletal math routines and type defs.

#ifndef UNSKELMATH_H
#define UNSKELMATH_H

#include <math.h>
//#include <stdio.h>


// Compile-time defines for debugging popups and logfiles
#define DEBUGMSGS (0)  // set to 0 to avoid debug popups
#define DEBUGFILE (0)  // set to 0 to suppress misc. log files being written from SceneIFC.cpp 
                       // -> can cause crashes #debug
#define DOLOGFILE (0)  // Log extra animation/skin info while writing a .PSA or .PSK file


#define  AXNode void
//#debug TODO - eliminate CStr dependecy from Max-specific code ?
#define  TSTR CStr

/*-----------------------------------------------------------------------------
	Types, 	pragmas, etc snipped from the Unreal Tournament codebase.
-----------------------------------------------------------------------------*/

// Unwanted VC++ level 4 warnings to disable. 
#pragma warning(disable : 4244) /* conversion to float, possible loss of data							*/
#pragma warning(disable : 4699) /* creating precompiled header											*/
#pragma warning(disable : 4200) /* Zero-length array item at end of structure, a VC-specific extension	*/
#pragma warning(disable : 4100) /* unreferenced formal parameter										*/
#pragma warning(disable : 4514) /* unreferenced inline function has been removed						*/
#pragma warning(disable : 4201) /* nonstandard extension used : nameless struct/union					*/
#pragma warning(disable : 4710) /* inline function not expanded											*/
#pragma warning(disable : 4702) /* unreachable code in inline expanded function							*/
#pragma warning(disable : 4711) /* function selected for autmatic inlining								*/
#pragma warning(disable : 4725) /* Pentium fdiv bug														*/
#pragma warning(disable : 4127) /* Conditional expression is constant									*/
#pragma warning(disable : 4512) /* assignment operator could not be generated                           */
#pragma warning(disable : 4530) /* C++ exception handler used, but unwind semantics are not enabled     */
#pragma warning(disable : 4245) /* conversion from 'enum ' to 'unsigned long', signed/unsigned mismatch */
#pragma warning(disable : 4305) /* truncation from 'const double' to 'float'                            */
#pragma warning(disable : 4238) /* nonstandard extension used : class rvalue used as lvalue             */
#pragma warning(disable : 4251) /* needs to have dll-interface to be used by clients of class 'ULinker' */
#pragma warning(disable : 4275) /* non dll-interface class used as base for dll-interface class         */
#pragma warning(disable : 4511) /* copy constructor could not be generated                              */
#pragma warning(disable : 4284) /* return type is not a UDT or reference to a UDT                       */
#pragma warning(disable : 4355) /* this used in base initializer list                                   */
#pragma warning(disable : 4097) /* typedef-name '' used as synonym for class-name ''                    */
#pragma warning(disable : 4291) /* typedef-name '' used as synonym for class-name ''                    */


// Unsigned base types.
typedef unsigned char		BYTE;		// 8-bit  unsigned.
typedef unsigned short		_WORD;		// 16-bit unsigned.
typedef unsigned long		DWORD;		// 32-bit unsigned.
typedef unsigned __int64	QWORD;		// 64-bit unsigned.

// Signed base types.
typedef	signed char			SBYTE;		// 8-bit  signed.
typedef signed short		SWORD;		// 16-bit signed.
typedef signed int  		INT;		// 32-bit signed.
typedef signed __int64		SQWORD;		// 64-bit signed.

// Character types.
typedef char			    ANSICHAR;	// An ANSI character.
typedef unsigned short      UNICHAR;	// A unicode character.

// Other base types.
typedef signed int			UBOOL;		// Boolean 0 (false) or 1 (true).
typedef float				FLOAT;		// 32-bit IEEE floating point.
typedef double				DOUBLE;		// 64-bit IEEE double.
typedef unsigned long       SIZE_T;     // Corresponds to C SIZE_T.


// Math constants.
#define INV_PI			(0.31830988618)
#define HALF_PI			(1.57079632679)

// Magic numbers for numerical precision.
#define DELTA			(0.00001)
#define SLERP_DELTA		(0.0001)   // Todo: test if it can be bigger (=faster code).

// Unreal's Constants.
// #define PI 					(3.1415926535897932)
#define SMALL_NUMBER		(1.e-8)
#define KINDA_SMALL_NUMBER	(1.e-4)



#define MAXPATHCHARS (255)
#define MAXINPUTCHARS (600)

// String support.

UBOOL FindSubString(const char* CheckString, const char* Fragment, INT& Pos);
UBOOL CheckSubString(const char* CheckString, const char* Fragment);
INT FindValueTag(const char* CheckString, const char* Fragment, INT DigitNum);
char* CleanString(char* name);
UBOOL RemoveExtString(const char* CheckString, const char* Fragment, char* OutString);
int ResizeString(char* Src, int Size);


// Forward declarations.
class  FVector;
class  FPlane;
class  FCoords;
class  FMatrix;
class  FQuat;
class  FAngAxis;  

// Fixed point quaternion.
//
// Floating point 4-element vector, for use with KNI 4x[4x3] matrix
// Warning: not fully or properly implemented all the FVector equivalent methods yet.
// 

// Floating point vector.
//
class FVector 
{
public:
	// Variables.
	union
	{
		struct {FLOAT X,Y,Z;};
		struct {FLOAT R,G,B;};
	};

	// Constructors.
	FVector()
	{}
	FVector( FLOAT InX, FLOAT InY, FLOAT InZ )
	:	X(InX), Y(InY), Z(InZ)
	{}

	// Binary math operators.
	FVector operator^( const FVector& V ) const
	{
		return FVector
		(
			Y * V.Z - Z * V.Y,
			Z * V.X - X * V.Z,
			X * V.Y - Y * V.X
		);
	}
	FLOAT operator|( const FVector& V ) const
	{
		return X*V.X + Y*V.Y + Z*V.Z;
	}
	friend FVector operator*( FLOAT Scale, const FVector& V )
	{
		return FVector( V.X * Scale, V.Y * Scale, V.Z * Scale );
	}
	FVector operator+( const FVector& V ) const
	{
		return FVector( X + V.X, Y + V.Y, Z + V.Z );
	}
	FVector operator-( const FVector& V ) const
	{
		return FVector( X - V.X, Y - V.Y, Z - V.Z );
	}
	FVector operator*( FLOAT Scale ) const
	{
		return FVector( X * Scale, Y * Scale, Z * Scale );
	}
	FVector operator/( FLOAT Scale ) const
	{
		FLOAT RScale = 1.0f/Scale;
		return FVector( X * RScale, Y * RScale, Z * RScale );
	}
	FVector operator*( const FVector& V ) const
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
	FVector operator-() const
	{
		return FVector( -X, -Y, -Z );
	}

	// Assignment operators.
	FVector operator+=( const FVector& V )
	{
		X += V.X; Y += V.Y; Z += V.Z;
		return *this;
	}
	FVector operator-=( const FVector& V )
	{
		X -= V.X; Y -= V.Y; Z -= V.Z;
		return *this;
	}
	FVector operator*=( FLOAT Scale )
	{
		X *= Scale; Y *= Scale; Z *= Scale;
		return *this;
	}
	FVector operator/=( FLOAT V )
	{
		FLOAT RV = 1.0f/V;
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

	// Simple functions.
	FLOAT Size() const
	{
		return (FLOAT) sqrt( X*X + Y*Y + Z*Z );
	}
	FLOAT SizeSquared() const
	{
		return X*X + Y*Y + Z*Z;
	}
	FLOAT Size2D() const 
	{
		return (FLOAT) sqrt( X*X + Y*Y );
	}
	FLOAT SizeSquared2D() const 
	{
		return X*X + Y*Y;
	}
	UBOOL IsZero() const
	{
		return X==0.0 && Y==0.0 && Z==0.0;
	}
	UBOOL Normalize()
	{
		FLOAT SquareSum = X*X+Y*Y+Z*Z;
		if( SquareSum >= SMALL_NUMBER )
		{
			FLOAT Scale = 1.0f/(FLOAT)sqrt(SquareSum);
			X *= Scale; Y *= Scale; Z *= Scale;
			return 1;
		}
		else return 0;
	}
	FVector Projection() const
	{
		FLOAT RZ = 1.0f/Z;
		return FVector( X*RZ, Y*RZ, 1 );
	}
	FVector UnsafeNormal() const
	{
		FLOAT Scale = 1.0f/(FLOAT)sqrt(X*X+Y*Y+Z*Z);
		return FVector( X*Scale, Y*Scale, Z*Scale );
	}
	FLOAT& Component( INT Index )
	{
		return (&X)[Index];
	}

	FVector TransformVectorBy( const FCoords& Coords ) const;

	// Return a boolean that is based on the vector's direction.
	// When      V==(0.0.0) Booleanize(0)=1.
	// Otherwise Booleanize(V) <-> !Booleanize(!B).
	UBOOL Booleanize()
	{
		return
			X >  0.0 ? 1 :
			X <  0.0 ? 0 :
			Y >  0.0 ? 1 :
			Y <  0.0 ? 0 :
			Z >= 0.0 ? 1 : 0;
	}

	FVector SafeNormal() const; //warning: Not inline because of compiler bug.
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
	FPlane()
	{}
	FPlane( const FPlane& P )
	:	FVector(P)
	,	W(P.W)
	{}
	FPlane( const FVector& V )
	:	FVector(V)
	,	W(0)
	{}
	FPlane( FLOAT InX, FLOAT InY, FLOAT InZ, FLOAT InW )
	:	FVector(InX,InY,InZ)
	,	W(InW)
	{}
	FPlane( FVector InNormal, FLOAT InW )
	:	FVector(InNormal), W(InW)
	{}
	FPlane( FVector InBase, const FVector &InNormal )
	:	FVector(InNormal)
	,	W(InBase | InNormal)
	{}

	// Functions.
	FLOAT PlaneDot( const FVector &P ) const
	{
		return X*P.X + Y*P.Y + Z*P.Z - W;
	}
	FPlane Flip() const
	{
		return FPlane(-X,-Y,-Z,-W);
	}
	UBOOL operator==( const FPlane& V ) const
	{
		return X==V.X && Y==V.Y && Z==V.Z && W==V.W;
	}
	UBOOL operator!=( const FPlane& V ) const
	{
		return X!=V.X || Y!=V.Y || Z!=V.Z || W!=V.W;
	}

};

class FCoords
{
public:
	FVector	Origin;
	FVector	XAxis;
	FVector YAxis;
	FVector ZAxis;

	// Constructors.
	FCoords() {}
	FCoords( const FVector &InOrigin )
	:	Origin(InOrigin), XAxis(1,0,0), YAxis(0,1,0), ZAxis(0,0,1) {}
	FCoords( const FVector &InOrigin, const FVector &InX, const FVector &InY, const FVector &InZ )
	:	Origin(InOrigin), XAxis(InX), YAxis(InY), ZAxis(InZ) {}

	FCoords FCoords::ApplyPivot(const FCoords& CoordsB) const;

	FCoords Inverse() const;

	/*
	// Functions.
	FCoords MirrorByVector( const FVector& MirrorNormal ) const;
	FCoords MirrorByPlane( const FPlane& MirrorPlane ) const;
	FCoords	Transpose() const;
	FCoords Inverse() const;
	*/
};





/*-----------------------------------------------------------------------------
	FMatrix.          
-----------------------------------------------------------------------------*/
//
// Floating point 4 x 4  (4 x 3)  KNI-friendly matrix
//
class FMatrix
{
public:

	// Variables.
	union
	{
		FLOAT M[4][4]; 
		struct
		{
			FPlane XPlane; // each plane [x,y,z,w] is a *column* in the matrix.
			FPlane YPlane;
			FPlane ZPlane;
			FPlane WPlane;
		};
	};

	// Constructors.
	FMatrix()
	{}
	FMatrix( FPlane InX, FPlane InY, FPlane InZ )
	:	XPlane(InX), YPlane(InY), ZPlane(InZ), WPlane(0,0,0,0)
	{}
	FMatrix( FPlane InX, FPlane InY, FPlane InZ, FPlane InW )
	:	XPlane(InX), YPlane(InY), ZPlane(InZ), WPlane(InW)
	{}


	// Regular transform
	FVector TransformFVector(const FVector &V) const
	{
		FVector FV;

		FV.X = V.X * M[0][0] + V.Y * M[0][1] + V.Z * M[0][2] + M[0][3];
		FV.Y = V.X * M[1][0] + V.Y * M[1][1] + V.Z * M[1][2] + M[1][3];
		FV.Z = V.X * M[2][0] + V.Y * M[2][1] + V.Z * M[2][2] + M[2][3];

		return FV;
	}

	// Homogeneous transform
	FPlane TransformFPlane(const FPlane &P) const
	{
		FPlane FP;

		FP.X = P.X * M[0][0] + P.Y * M[0][1] + P.Z * M[0][2] + M[0][3];
		FP.Y = P.X * M[1][0] + P.Y * M[1][1] + P.Z * M[1][2] + M[1][3];
		FP.Z = P.X * M[2][0] + P.Y * M[2][1] + P.Z * M[2][2] + M[2][3];
		FP.W = P.X * M[3][0] + P.Y * M[3][1] + P.Z * M[3][2] + M[3][3];

		return FP;
	}

	// Combine transforms binary operation MxN
	friend FMatrix CombineTransforms(const FMatrix& M, const FMatrix& N);
	friend FMatrix FMatrixFromFCoords(const FCoords& FC);
	friend FCoords FCoordsFromFMatrix(const FMatrix& FM);

};



// Conversions for Unreal1 coordinate system class.

inline FMatrix FMatrixFromFCoords(const FCoords& FC) 
{
	FMatrix M;
	M.XPlane = FPlane( FC.XAxis.X, FC.XAxis.Y, FC.XAxis.Z, FC.Origin.X );
	M.YPlane = FPlane( FC.YAxis.X, FC.YAxis.Y, FC.YAxis.Z, FC.Origin.Y );
	M.ZPlane = FPlane( FC.ZAxis.X, FC.ZAxis.Y, FC.ZAxis.Z, FC.Origin.Z );
	M.WPlane = FPlane( 0.0f,     0.0f,    0.0f,    1.0f    );
	return M;
}

inline FCoords FCoordsFromFMatrix(const FMatrix& FM)
{
	FCoords C;
	C.Origin = FVector( FM.XPlane.W, FM.YPlane.W, FM.ZPlane.W );
	C.XAxis  = FVector( FM.XPlane.X, FM.XPlane.Y, FM.XPlane.Z );
	C.YAxis  = FVector( FM.YPlane.X, FM.YPlane.Y, FM.YPlane.Z );
	C.ZAxis  = FVector( FM.ZPlane.X, FM.ZPlane.Y, FM.ZPlane.Z );
	return C;
}


/*-----------------------------------------------------------------------------
	FQuat.          
-----------------------------------------------------------------------------*/

//
// floating point quaternion.
//
class FQuat 
{
	public:
	// Variables.
	struct {FLOAT X,Y,Z,W;};

	// Constructors.
	FQuat()
	{}

	FQuat( FLOAT InX, FLOAT InY, FLOAT InZ, FLOAT InA )
	:	X(InX), Y(InY), Z(InZ), W(InA)
	{}


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
			X*Q.X - Y*Q.Y - Z*Q.Z - W*Q.W, 
			X*Q.Y + Y*Q.X + Z*Q.W - W*Q.Z, 
			X*Q.Z - Y*Q.W + Z*Q.X + W*Q.Y, 
			X*Q.W + Y*Q.Z - Z*Q.Y + W*Q.X
			);
	}

	FQuat operator*( const FLOAT& Scale ) const
	{
		return FQuat( Scale*X, Scale*Y, Scale*Z, Scale*W);			
	}
	
	// Unary operators.
	FQuat operator-() const
	{
		return FQuat( X, Y, Z, -W );
	}

    // Misc operators
	UBOOL operator!=( const FQuat& Q ) const
	{
		return X!=Q.X || Y!=Q.Y || Z!=Q.Z || W!=Q.W;
	}
	
	UBOOL Normalize()
	{
		// 
		FLOAT SquareSum = (FLOAT)(X*X+Y*Y+Z*Z+W*W);
		if( SquareSum >= DELTA )
		{
			FLOAT Scale = 1.0f/(FLOAT)sqrt(SquareSum);
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
			Z = 0.1f;
			W = 0.0f;
			return false;
		}
	}

	/*
    // Angle scaling.
	FQuat operator*( FLOAT Scale, const FQuat& Q )
	{
		return FQuat( Q.X, Q.Y, Q.Z, Q.W * Scale );
	}
	*/

	// friends
	// friend FAngAxis	FQuatToFAngAxis(const FQuat& Q);
	// friend void SlerpQuat(const FQuat& Q1, const FQuat& Q2, FLOAT slerp, FQuat& Result)		
	// friend FQuat	BlendQuatWith(const FQuat& Q1, FQuat& Q2, FLOAT Blend)
	// friend  FLOAT FQuatDot(const FQuat&1, FQuat& Q2);
};

//
// Misc conversions 
//

FQuat FMatrixToFQuat(const FMatrix& M);
FMatrix FQuatToFMatrix(const FQuat& Q);


//
// Dot product of axes to get cos of angle  #Warning some people use .W component here too !
//
inline FLOAT FQuatDot(const FQuat& Q1,const FQuat& Q2)
{
	return( Q1.X*Q2.X + Q1.Y*Q2.Y + Q1.Z*Q2.Z );
};

//
// Floating point Angle-axis rotation representation.
//
class FAngAxis
{
public:
	// Variables.
	struct {FLOAT X,Y,Z,A;};
};


// Inlines 

inline FLOAT Square(const FLOAT F1)
{
	return F1*F1;
}

// Error measure (angle) between two quaternions, ranged [0..1]
inline FLOAT FQuatError(FQuat& Q1,FQuat& Q2)
{
	// Returns the hypersphere-angle between two quaternions; alignment shouldn't matter, though 
	// normalized input is expected.
	FLOAT cosom = Q1.X*Q2.X + Q1.Y*Q2.Y + Q1.Z*Q2.Z + Q1.W*Q2.W;
	return ( abs(cosom) < 0.9999999f) ? acos(cosom)*(1.f/3.1415926535f) : 0.0f;
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

// No-frills spherical interpolation. Assumes aligned quaternions, and the output is not normalized.
inline FQuat SlerpQuat(const FQuat &quat1,const FQuat &quat2, float slerp)
{
	FQuat result;
	float omega,cosom,sininv,scale0,scale1;

	// Get cosine of angle betweel quats.
	cosom = quat1.X * quat2.X +
			quat1.Y * quat2.Y +
			quat1.Z * quat2.Z +
			quat1.W * quat2.W;

	if( cosom < 0.99999999f )
	{	
		omega = acos(cosom);
		sininv = 1.f/sin(omega);
		scale0 = sin((1.f - slerp) * omega) * sininv;
		scale1 = sin(slerp * omega) * sininv;
		
		result.X = scale0 * quat1.X + scale1 * quat2.X;
		result.Y = scale0 * quat1.Y + scale1 * quat2.Y;
		result.Z = scale0 * quat1.Z + scale1 * quat2.Z;
		result.W = scale0 * quat1.W + scale1 * quat2.W;
		return result;
	}
	else
	{
		return quat1;
	}
	
}

inline FVector FVector::TransformVectorBy( const FCoords &Coords ) const
{
	return FVector(	*this | Coords.XAxis, *this | Coords.YAxis, *this | Coords.ZAxis );
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

/*-----------------------------------------------------------------------------
	Texturing.
-----------------------------------------------------------------------------*/

// Accepts a triangle (XYZ and ST values for each point) and returns a poly base and UV vectors
inline void FTexCoordsToVectors( FVector V0, FVector ST0, FVector V1, FVector ST1, FVector V2, FVector ST2, FVector* InBaseResult, FVector* InUResult, FVector* InVResult )
{
	// Create polygon normal.
	FVector PN = FVector((V0-V1) ^ (V2-V0));
	PN = PN.SafeNormal();

	// Fudge UV's to make sure no infinities creep into UV vector math, whenever we detect identical U or V's.
	if( ( ST0.X == ST1.X ) || ( ST2.X == ST1.X ) || ( ST2.X == ST0.X ) ||
		( ST0.Y == ST1.Y ) || ( ST2.Y == ST1.Y ) || ( ST2.Y == ST0.Y ) )
	{
		ST1 += FVector(0.004173f,0.004123f,0.0f);
		ST2 += FVector(0.003173f,0.003123f,0.0f);
	}

	//
	// Solve the equations to find our texture U/V vectors 'TU' and 'TV' by stacking them 
	// into a 3x3 matrix , one for  u(t) = TU dot (x(t)-x(o) + u(o) and one for v(t)=  TV dot (.... , 
	// then the third assumes we're perpendicular to the normal. 
	//
	FCoords TexEqu; 
	TexEqu.XAxis = FVector(	V1.X - V0.X, V1.Y - V0.Y, V1.Z - V0.Z );
	TexEqu.YAxis = FVector( V2.X - V0.X, V2.Y - V0.Y, V2.Z - V0.Z );
	TexEqu.ZAxis = FVector( PN.X,        PN.Y,        PN.Z        );
	TexEqu.Origin =FVector( 0.0f, 0.0f, 0.0f );
	TexEqu = TexEqu.Inverse();

	FVector UResult( ST1.X-ST0.X, ST2.X-ST0.X, 0.0f );
	FVector TUResult = UResult.TransformVectorBy( TexEqu );

	FVector VResult( ST1.Y-ST0.Y, ST2.Y-ST0.Y, 0.0f );
	FVector TVResult = VResult.TransformVectorBy( TexEqu );

	//
	// Adjust the BASE to account for U0 and V0 automatically, and force it into the same plane.
	//				
	FCoords BaseEqu;
	BaseEqu.XAxis = TUResult;
	BaseEqu.YAxis = TVResult; 
	BaseEqu.ZAxis = FVector( PN.X, PN.Y, PN.Z );
	BaseEqu.Origin = FVector( 0.0f, 0.0f, 0.0f );

	FVector BResult = FVector( ST0.X - ( TUResult|V0 ), ST0.Y - ( TVResult|V0 ),  0.0f );

	*InBaseResult = - 1.0f *  BResult.TransformVectorBy( BaseEqu.Inverse() );
	*InUResult = TUResult;
	*InVResult = TVResult;

}



/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

#endif // UNSKELMATH_H