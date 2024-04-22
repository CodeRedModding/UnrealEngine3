/*=============================================================================
	UnMathFpu.h: Fallback FPU vector intrinsics

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef HEADER_UNMATHFPU
#define HEADER_UNMATHFPU


/*=============================================================================
 *	Helpers:
 *============================================================================*/

/**
 *	float4 vector register type, where the first float (X) is stored in the lowest 32 bits, and so on.
 */
struct VectorRegister
{
#if WIIU
	// tell the compiler that the FLOATSs may have DWORDs jammed into them, and treat them appreopriately
	union
	{
#endif
		FLOAT	V[4];
#if WIIU
		DWORD	D[4];
	};
#endif
};

/**
 * Returns a bitwise equivalent vector based on 4 DWORDs.
 *
 * @param X		1st DWORD component
 * @param Y		2nd DWORD component
 * @param Z		3rd DWORD component
 * @param W		4th DWORD component
 * @return		Bitwise equivalent vector with 4 floats
 */
FORCEINLINE VectorRegister MakeVectorRegister( DWORD X, DWORD Y, DWORD Z, DWORD W )
{
	VectorRegister Vec;
	((DWORD&)Vec.V[0]) = X;
	((DWORD&)Vec.V[1]) = Y;
	((DWORD&)Vec.V[2]) = Z;
	((DWORD&)Vec.V[3]) = W;
	return Vec;
}

/**
 * Returns a vector based on 4 FLOATs.
 *
 * @param X		1st FLOAT component
 * @param Y		2nd FLOAT component
 * @param Z		3rd FLOAT component
 * @param W		4th FLOAT component
 * @return		Vector of the 4 FLOATs
 */
FORCEINLINE VectorRegister MakeVectorRegister( FLOAT X, FLOAT Y, FLOAT Z, FLOAT W )
{
	VectorRegister Vec = { X, Y, Z, W };
	return Vec;
}

/** Vector that represents (0,0,0,0) */
static const VectorRegister VECTOR_ZERO = MakeVectorRegister( 0.0f, 0.0f, 0.0f, 0.0f );

/** Vector that represents (1,1,1,1) */
static const VectorRegister VECTOR_ONE  = MakeVectorRegister( 1.0f, 1.0f, 1.0f, 1.0f );


/*=============================================================================
 *	Constants:
 *============================================================================*/

#include "UnMathVectorConstants.h"


/*=============================================================================
 *	Intrinsics:
 *============================================================================*/

/**
 * Returns a vector with all zeros.
 *
 * @return		VectorRegister(0.0f, 0.0f, 0.0f, 0.0f)
 */
#define VectorZero()					VECTOR_ZERO

/**
 * Returns a vector with all ones.
 *
 * @return		VectorRegister(1.0f, 1.0f, 1.0f, 1.0f)
 */
#define VectorOne()						VECTOR_ONE

/**
 * Loads 4 FLOATs from unaligned memory.
 *
 * @param Ptr	Unaligned memory pointer to the 4 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], Ptr[3])
 */
#define VectorLoad( Ptr )				MakeVectorRegister( ((const FLOAT*)(Ptr))[0], ((const FLOAT*)(Ptr))[1], ((const FLOAT*)(Ptr))[2], ((const FLOAT*)(Ptr))[3] )

/**
 * Loads 3 FLOATs from unaligned memory and leaves W undefined.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], undefined)
 */
#define VectorLoadFloat3( Ptr )			MakeVectorRegister( ((const FLOAT*)(Ptr))[0], ((const FLOAT*)(Ptr))[1], ((const FLOAT*)(Ptr))[2], 0.0f )

/**
 * Loads 3 FLOATs from unaligned memory and sets W=0.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], 0.0f)
 */
#define VectorLoadFloat3_W0( Ptr )		MakeVectorRegister( ((const FLOAT*)(Ptr))[0], ((const FLOAT*)(Ptr))[1], ((const FLOAT*)(Ptr))[2], 0.0f )

/**
 * Loads 3 FLOATs from unaligned memory and sets W=1.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], 1.0f)
 */
#define VectorLoadFloat3_W1( Ptr )		MakeVectorRegister( ((const FLOAT*)(Ptr))[0], ((const FLOAT*)(Ptr))[1], ((const FLOAT*)(Ptr))[2], 1.0f )

/**
 * Loads 4 FLOATs from aligned memory.
 *
 * @param Ptr	Aligned memory pointer to the 4 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], Ptr[3])
 */
#define VectorLoadAligned( Ptr )		MakeVectorRegister( ((const FLOAT*)(Ptr))[0], ((const FLOAT*)(Ptr))[1], ((const FLOAT*)(Ptr))[2], ((const FLOAT*)(Ptr))[3] )

/**
 * Loads 1 FLOAT from unaligned memory and replicates it to all 4 elements.
 *
 * @param Ptr	Unaligned memory pointer to the FLOAT
 * @return		VectorRegister(Ptr[0], Ptr[0], Ptr[0], Ptr[0])
 */
#define VectorLoadFloat1( Ptr )			MakeVectorRegister( ((const FLOAT*)(Ptr))[0], ((const FLOAT*)(Ptr))[0], ((const FLOAT*)(Ptr))[0], ((const FLOAT*)(Ptr))[0] )

/**
 * Creates a vector out of three FLOATs and leaves W undefined.
 *
 * @param X		1st FLOAT component
 * @param Y		2nd FLOAT component
 * @param Z		3rd FLOAT component
 * @return		VectorRegister(X, Y, Z, undefined)
 */
#define VectorSetFloat3( X, Y, Z )		MakeVectorRegister( X, Y, Z, 0.0f )

/**
 * Creates a vector out of four FLOATs.
 *
 * @param X		1st FLOAT component
 * @param Y		2nd FLOAT component
 * @param Z		3rd FLOAT component
 * @param W		4th FLOAT component
 * @return		VectorRegister(X, Y, Z, W)
 */
#define VectorSet( X, Y, Z, W )			MakeVectorRegister( X, Y, Z, W )

/**
 * Stores a vector to aligned memory.
 *
 * @param Vec	Vector to store
 * @param Ptr	Aligned memory pointer
 */
#define VectorStoreAligned( Vec, Ptr )	appMemcpy( Ptr, &(Vec), 16 )

/**
 * Stores a vector to memory (aligned or unaligned).
 *
 * @param Vec	Vector to store
 * @param Ptr	Memory pointer
 */
#define VectorStore( Vec, Ptr )			appMemcpy( Ptr, &(Vec), 16 )

/**
 * Stores the XYZ components of a vector to unaligned memory.
 *
 * @param Vec	Vector to store XYZ
 * @param Ptr	Unaligned memory pointer
 */
#define VectorStoreFloat3( Vec, Ptr )	appMemcpy( Ptr, &(Vec), 12 )

/**
 * Stores the X component of a vector to unaligned memory.
 *
 * @param Vec	Vector to store X
 * @param Ptr	Unaligned memory pointer
 */
#define VectorStoreFloat1( Vec, Ptr )	appMemcpy( Ptr, &(Vec), 4 )

/**
 * Replicates one element into all four elements and returns the new vector.
 *
 * @param Vec			Source vector
 * @param ElementIndex	Index (0-3) of the element to replicate
 * @return				VectorRegister( Vec[ElementIndex], Vec[ElementIndex], Vec[ElementIndex], Vec[ElementIndex] )
 */
#define VectorReplicate( Vec, ElementIndex )	MakeVectorRegister( (Vec).V[ElementIndex], (Vec).V[ElementIndex], (Vec).V[ElementIndex], (Vec).V[ElementIndex] )

/**
 * Returns the absolute value (component-wise).
 *
 * @param Vec			Source vector
 * @return				VectorRegister( abs(Vec.x), abs(Vec.y), abs(Vec.z), abs(Vec.w) )
 */
FORCEINLINE VectorRegister VectorAbs( const VectorRegister &Vec )
{
	VectorRegister Vec2;
	Vec2.V[0] = Abs(Vec.V[0]);
	Vec2.V[1] = Abs(Vec.V[1]);
	Vec2.V[2] = Abs(Vec.V[2]);
	Vec2.V[3] = Abs(Vec.V[3]);
	return Vec2;
}

/**
 * Returns the negated value (component-wise).
 *
 * @param Vec			Source vector
 * @return				VectorRegister( -Vec.x, -Vec.y, -Vec.z, -Vec.w )
 */
#define VectorNegate( Vec )				MakeVectorRegister( -(Vec).V[0], -(Vec).V[1], -(Vec).V[2], -(Vec).V[3] )

/**
 * Adds two vectors (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x+Vec2.x, Vec1.y+Vec2.y, Vec1.z+Vec2.z, Vec1.w+Vec2.w )
 */
FORCEINLINE VectorRegister VectorAdd( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Vec;
	Vec.V[0] = Vec1.V[0] + Vec2.V[0];
	Vec.V[1] = Vec1.V[1] + Vec2.V[1];
	Vec.V[2] = Vec1.V[2] + Vec2.V[2];
	Vec.V[3] = Vec1.V[3] + Vec2.V[3];
	return Vec;
}

/**
 * Subtracts a vector from another (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x-Vec2.x, Vec1.y-Vec2.y, Vec1.z-Vec2.z, Vec1.w-Vec2.w )
 */
FORCEINLINE VectorRegister VectorSubtract( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Vec;
	Vec.V[0] = Vec1.V[0] - Vec2.V[0];
	Vec.V[1] = Vec1.V[1] - Vec2.V[1];
	Vec.V[2] = Vec1.V[2] - Vec2.V[2];
	Vec.V[3] = Vec1.V[3] - Vec2.V[3];
	return Vec;
}

/**
 * Multiplies two vectors (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x*Vec2.x, Vec1.y*Vec2.y, Vec1.z*Vec2.z, Vec1.w*Vec2.w )
 */
FORCEINLINE VectorRegister VectorMultiply( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Vec;
	Vec.V[0] = Vec1.V[0] * Vec2.V[0];
	Vec.V[1] = Vec1.V[1] * Vec2.V[1];
	Vec.V[2] = Vec1.V[2] * Vec2.V[2];
	Vec.V[3] = Vec1.V[3] * Vec2.V[3];
	return Vec;
}

/**
 * Multiplies two vectors (component-wise), adds in the third vector and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @param Vec3	3rd vector
 * @return		VectorRegister( Vec1.x*Vec2.x + Vec3.x, Vec1.y*Vec2.y + Vec3.y, Vec1.z*Vec2.z + Vec3.z, Vec1.w*Vec2.w + Vec3.w )
 */
FORCEINLINE VectorRegister VectorMultiplyAdd( const VectorRegister& Vec1, const VectorRegister& Vec2, const VectorRegister& Vec3 )
{
	VectorRegister Vec;
	Vec.V[0] = Vec1.V[0] * Vec2.V[0] + Vec3.V[0];
	Vec.V[1] = Vec1.V[1] * Vec2.V[1] + Vec3.V[1];
	Vec.V[2] = Vec1.V[2] * Vec2.V[2] + Vec3.V[2];
	Vec.V[3] = Vec1.V[3] * Vec2.V[3] + Vec3.V[3];
	return Vec;
}

/**
 * Calculates the dot3 product of two vectors and returns a vector with the result in all 4 components.
 * Only really efficient on Xbox 360.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		d = dot3(Vec1.xyz, Vec2.xyz), VectorRegister( d, d, d, d )
 */
FORCEINLINE VectorRegister VectorDot3( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	FLOAT D = Vec1.V[0] * Vec2.V[0] + Vec1.V[1] * Vec2.V[1] + Vec1.V[2] * Vec2.V[2];
	VectorRegister Vec = { D, D, D, D };
	return Vec;
}

/**
 * Calculates the dot4 product of two vectors and returns a vector with the result in all 4 components.
 * Only really efficient on Xbox 360.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		d = dot4(Vec1.xyzw, Vec2.xyzw), VectorRegister( d, d, d, d )
 */
FORCEINLINE VectorRegister VectorDot4( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	FLOAT D = Vec1.V[0] * Vec2.V[0] + Vec1.V[1] * Vec2.V[1] + Vec1.V[2] * Vec2.V[2] + Vec1.V[3] * Vec2.V[3];
	VectorRegister Vec = { D, D, D, D };
	return Vec;
}

/**
 * Creates a four-part mask based on component-wise == compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x == Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */

FORCEINLINE VectorRegister VectorCompareEQ( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return MakeVectorRegister(
		(DWORD)(Vec1.V[0] == Vec2.V[0] ? 0xFFFFFFFF : 0), 
		Vec1.V[1] == Vec2.V[1] ? 0xFFFFFFFF : 0, 
		Vec1.V[2] == Vec2.V[2] ? 0xFFFFFFFF : 0, 
		Vec1.V[3] == Vec2.V[3] ? 0xFFFFFFFF : 0);
}

/**
 * Creates a four-part mask based on component-wise != compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x != Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */

FORCEINLINE VectorRegister VectorCompareNE( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return MakeVectorRegister(
		(DWORD)(Vec1.V[0] != Vec2.V[0] ? 0xFFFFFFFF : 0),
		Vec1.V[1] != Vec2.V[1] ? 0xFFFFFFFF : 0, 
		Vec1.V[2] != Vec2.V[2] ? 0xFFFFFFFF : 0, 
		Vec1.V[3] != Vec2.V[3] ? 0xFFFFFFFF : 0);
}

/**
 * Creates a four-part mask based on component-wise > compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x > Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */

FORCEINLINE VectorRegister VectorCompareGT( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return MakeVectorRegister(
		(DWORD)(Vec1.V[0] > Vec2.V[0] ? 0xFFFFFFFF : 0),
		Vec1.V[1] > Vec2.V[1] ? 0xFFFFFFFF : 0, 
		Vec1.V[2] > Vec2.V[2] ? 0xFFFFFFFF : 0, 
		Vec1.V[3] > Vec2.V[3] ? 0xFFFFFFFF : 0);
}

/**
 * Creates a four-part mask based on component-wise >= compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x >= Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */

FORCEINLINE VectorRegister VectorCompareGE( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return MakeVectorRegister(
		(DWORD)(Vec1.V[0] >= Vec2.V[0] ? 0xFFFFFFFF : 0),
		Vec1.V[1] >= Vec2.V[1] ? 0xFFFFFFFF : 0,
		Vec1.V[2] >= Vec2.V[2] ? 0xFFFFFFFF : 0,
		Vec1.V[3] >= Vec2.V[3] ? 0xFFFFFFFF : 0);
}

/**
 * Does a bitwise vector selection based on a mask (e.g., created from VectorCompareXX)
 *
 * @param Mask  Mask (when 1: use the corresponding bit from Vec1 otherwise from Vec2)
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Mask[i] ? Vec1[i] : Vec2[i] )
 *
 */

FORCEINLINE VectorRegister VectorSelect(const VectorRegister& Mask, const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	DWORD* V1 = (DWORD*)(&(Vec1.V[0]));
	DWORD* V2 = (DWORD*)(&(Vec2.V[0]));
	DWORD* M = (DWORD*)(&(Mask.V[0]));

	return MakeVectorRegister(
		V2[0] ^ (M[0] & (V2[0] ^ V1[0])),
		V2[1] ^ (M[1] & (V2[1] ^ V1[1])),
		V2[2] ^ (M[2] & (V2[2] ^ V1[2])),
		V2[3] ^ (M[3] & (V2[3] ^ V1[3]))
	);
}

/**
 * Combines two vectors using bitwise OR (treating each vector as a 128 bit field)
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Vec1[i] | Vec2[i] )
 */
FORCEINLINE VectorRegister VectorBitwiseOr(const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return MakeVectorRegister(
		(DWORD)( ((DWORD*)(Vec1.V))[0] | ((DWORD*)(Vec2.V))[0] ),
		((DWORD*)(Vec1.V))[1] | ((DWORD*)(Vec2.V))[1],
		((DWORD*)(Vec1.V))[2] | ((DWORD*)(Vec2.V))[2],
		((DWORD*)(Vec1.V))[3] | ((DWORD*)(Vec2.V))[3]);
}

/**
 * Combines two vectors using bitwise AND (treating each vector as a 128 bit field)
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Vec1[i] & Vec2[i] )
 */
FORCEINLINE VectorRegister VectorBitwiseAnd(const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return MakeVectorRegister(
		(DWORD)( ((DWORD*)(Vec1.V))[0] & ((DWORD*)(Vec2.V))[0] ),
		((DWORD*)(Vec1.V))[1] & ((DWORD*)(Vec2.V))[1],
		((DWORD*)(Vec1.V))[2] & ((DWORD*)(Vec2.V))[2],
		((DWORD*)(Vec1.V))[3] & ((DWORD*)(Vec2.V))[3]);
}

/**
 * Combines two vectors using bitwise XOR (treating each vector as a 128 bit field)
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Vec1[i] ^ Vec2[i] )
 */
FORCEINLINE VectorRegister VectorBitwiseXor(const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return MakeVectorRegister(
		(DWORD)( ((DWORD*)(Vec1.V))[0] ^ ((DWORD*)(Vec2.V))[0] ),
		((DWORD*)(Vec1.V))[1] ^ ((DWORD*)(Vec2.V))[1],
		((DWORD*)(Vec1.V))[2] ^ ((DWORD*)(Vec2.V))[2],
		((DWORD*)(Vec1.V))[3] ^ ((DWORD*)(Vec2.V))[3]);
}


/**
 * Calculates the cross product of two vectors (XYZ components). W is set to 0.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		cross(Vec1.xyz, Vec2.xyz). W is set to 0.
 */
FORCEINLINE VectorRegister VectorCross( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Vec;
	Vec.V[0] = Vec1.V[1] * Vec2.V[2] - Vec1.V[2] * Vec2.V[1];
	Vec.V[1] = Vec1.V[2] * Vec2.V[0] - Vec1.V[0] * Vec2.V[2];
	Vec.V[2] = Vec1.V[0] * Vec2.V[1] - Vec1.V[1] * Vec2.V[0];
	Vec.V[3] = 0.0f;
	return Vec;
}

/**
 * Calculates x raised to the power of y (component-wise).
 *
 * @param Base		Base vector
 * @param Exponent	Exponent vector
 * @return			VectorRegister( Base.x^Exponent.x, Base.y^Exponent.y, Base.z^Exponent.z, Base.w^Exponent.w )
 */
FORCEINLINE VectorRegister VectorPow( const VectorRegister& Base, const VectorRegister& Exponent )
{
	VectorRegister Vec;
	Vec.V[0] = appPow(Base.V[0], Exponent.V[0]);
	Vec.V[1] = appPow(Base.V[1], Exponent.V[1]);
	Vec.V[2] = appPow(Base.V[2], Exponent.V[2]);
	Vec.V[3] = appPow(Base.V[3], Exponent.V[3]);
	return Vec;
}

/**
* Returns an estimate of 1/sqrt(c) for each component of the vector
*
* @param Vector		Vector 
* @return			VectorRegister(1/sqrt(t), 1/sqrt(t), 1/sqrt(t), 1/sqrt(t))
*/
FORCEINLINE VectorRegister VectorReciprocalSqrt(const VectorRegister& Vec)
{
	return MakeVectorRegister(1.0f / appSqrt(Vec.V[0]), 1.0f / appSqrt(Vec.V[1]), 1.0f / appSqrt(Vec.V[2]), 1.0f / appSqrt(Vec.V[3]));
}

/**
 * Computes an estimate of the reciprocal of a vector (component-wise) and returns the result.
 *
 * @param Vec	1st vector
 * @return		VectorRegister( (Estimate) 1.0f / Vec.x, (Estimate) 1.0f / Vec.y, (Estimate) 1.0f / Vec.z, (Estimate) 1.0f / Vec.w )
 */
FORCEINLINE VectorRegister VectorReciprocal(const VectorRegister& Vec)
{
	return MakeVectorRegister(1.0f / Vec.V[0], 1.0f / Vec.V[1], 1.0f / Vec.V[2], 1.0f / Vec.V[3]);
}

/**
* Return Reciprocal Length of the vector
*
* @param Vector		Vector 
* @return			VectorRegister(rlen, rlen, rlen, rlen) when rlen = 1/sqrt(dot4(V))
*/
FORCEINLINE VectorRegister VectorReciprocalLen( const VectorRegister& Vector )
{
	VectorRegister Len = VectorDot4( Vector, Vector );
	FLOAT rlen = 1.0f / appSqrt(Len.V[0]);
	
	VectorRegister Result;
	Result.V[0] = rlen;
	Result.V[1] = rlen;
	Result.V[2] = rlen;
	Result.V[3] = rlen;
	return Result;
}

/**
* Return the reciprocal of the square root of each component
*
* @param Vector		Vector 
* @return			VectorRegister(1/sqrt(Vec.X), 1/sqrt(Vec.Y), 1/sqrt(Vec.Z), 1/sqrt(Vec.W))
*/
#define VectorReciprocalSqrtAccurate(Vec)	VectorReciprocalSqrt(Vec)

/**
 * Computes the reciprocal of a vector (component-wise) and returns the result.
 *
 * @param Vec	1st vector
 * @return		VectorRegister( 1.0f / Vec.x, 1.0f / Vec.y, 1.0f / Vec.z, 1.0f / Vec.w )
 */
#define VectorReciprocalAccurate(Vec)	VectorReciprocal(Vec)

/**
* Normalize vector
*
* @param Vector		Vector to normalize
* @return			Normalized VectorRegister
*/
FORCEINLINE VectorRegister VectorNormalize( const VectorRegister& Vector )
{
	return VectorMultiply( Vector, VectorReciprocalLen( Vector ) );
}

/**
* Loads XYZ and sets W=0
*
* @param Vector	VectorRegister
* @return		VectorRegister(X, Y, Z, 0.0f)
*/
#define VectorSet_W0( Vec )		MakeVectorRegister( (Vec).V[0], (Vec).V[1], (Vec).V[2], 0.0f )

/**
* Loads XYZ and sets W=1
*
* @param Vector	VectorRegister
* @return		VectorRegister(X, Y, Z, 1.0f)
*/
#define VectorSet_W1( Vec )		MakeVectorRegister( (Vec).V[0], (Vec).V[1], (Vec).V[2], 1.0f )


// 40% faster version of the Quaternion multiplication.
#define USE_FAST_QUAT_MUL 1

/**
* Multiplies two quaternions: The order matters.
*
* @param Result	Pointer to where the result should be stored
* @param Quat1	Pointer to the first quaternion (must not be the destination)
* @param Quat2	Pointer to the second quaternion (must not be the destination)
*/
FORCEINLINE void VectorQuaternionMultiply( void *Result, const void* Quat1, const void* Quat2)
{
	typedef FLOAT Float4[4];
	const Float4& A = *((const Float4*) Quat1);
	const Float4& B = *((const Float4*) Quat2);
	Float4 & R = *((Float4*) Result);

#if USE_FAST_QUAT_MUL
	const FLOAT T0 = (A[2] - A[1]) * (B[1] - B[2]);
	const FLOAT T1 = (A[3] + A[0]) * (B[3] + B[0]);
	const FLOAT T2 = (A[3] - A[0]) * (B[1] + B[2]);
	const FLOAT T3 = (A[1] + A[2]) * (B[3] - B[0]);
	const FLOAT T4 = (A[2] - A[0]) * (B[0] - B[1]);
	const FLOAT T5 = (A[2] + A[0]) * (B[0] + B[1]);
	const FLOAT T6 = (A[3] + A[1]) * (B[3] - B[2]);
	const FLOAT T7 = (A[3] - A[1]) * (B[3] + B[2]);
	const FLOAT T8 = T5 + T6 + T7;
	const FLOAT T9 = 0.5f * (T4 + T8);

	R[0] = T1 + T9 - T8;
	R[1] = T2 + T9 - T7;
	R[2] = T3 + T9 - T6;
	R[3] = T0 + T9 - T5;
#else
	// store intermediate results in temporaries
	const FLOAT TX = A[3]*B[0] + A[0]*B[3] + A[1]*B[2] - A[2]*B[1];
	const FLOAT TY = A[3]*B[1] - A[0]*B[2] + A[1]*B[3] + A[2]*B[0];
	const FLOAT TZ = A[3]*B[2] + A[0]*B[1] - A[1]*B[0] + A[2]*B[3];
	const FLOAT TW = A[3]*B[3] - A[0]*B[0] - A[1]*B[1] - A[2]*B[2];

	// copy intermediate result to *this
	R[0] = TX;
	R[1] = TY;
	R[2] = TZ;
	R[3] = TW;
#endif
}

/**
* Multiplies two quaternions: The order matters.
*
* @param Result	Returns Quat1 * Quat2
* @param Quat1	Pointer to the first quaternion
* @param Quat2	Pointer to the second quaternion
*/
FORCEINLINE VectorRegister VectorQuaternionMultiply2( const VectorRegister& Quat1, const VectorRegister& Quat2 )
{
	VectorRegister Result;
	VectorQuaternionMultiply(&Result, &Quat1, &Quat2);
	return Result;
}

/**
 * Multiplies two 4x4 matrices.
 *
 * @param Result	Pointer to where the result should be stored
 * @param Matrix1	Pointer to the first matrix
 * @param Matrix2	Pointer to the second matrix
 */
FORCEINLINE void VectorMatrixMultiply( void* Result, const void* Matrix1, const void* Matrix2 )
{
	typedef FLOAT Float4x4[4][4];
	const Float4x4& A = *((const Float4x4*) Matrix1);
	const Float4x4& B = *((const Float4x4*) Matrix2);
	Float4x4 Temp;
	Temp[0][0] = A[0][0] * B[0][0] + A[0][1] * B[1][0] + A[0][2] * B[2][0] + A[0][3] * B[3][0];
	Temp[0][1] = A[0][0] * B[0][1] + A[0][1] * B[1][1] + A[0][2] * B[2][1] + A[0][3] * B[3][1];
	Temp[0][2] = A[0][0] * B[0][2] + A[0][1] * B[1][2] + A[0][2] * B[2][2] + A[0][3] * B[3][2];
	Temp[0][3] = A[0][0] * B[0][3] + A[0][1] * B[1][3] + A[0][2] * B[2][3] + A[0][3] * B[3][3];

	Temp[1][0] = A[1][0] * B[0][0] + A[1][1] * B[1][0] + A[1][2] * B[2][0] + A[1][3] * B[3][0];
	Temp[1][1] = A[1][0] * B[0][1] + A[1][1] * B[1][1] + A[1][2] * B[2][1] + A[1][3] * B[3][1];
	Temp[1][2] = A[1][0] * B[0][2] + A[1][1] * B[1][2] + A[1][2] * B[2][2] + A[1][3] * B[3][2];
	Temp[1][3] = A[1][0] * B[0][3] + A[1][1] * B[1][3] + A[1][2] * B[2][3] + A[1][3] * B[3][3];

	Temp[2][0] = A[2][0] * B[0][0] + A[2][1] * B[1][0] + A[2][2] * B[2][0] + A[2][3] * B[3][0];
	Temp[2][1] = A[2][0] * B[0][1] + A[2][1] * B[1][1] + A[2][2] * B[2][1] + A[2][3] * B[3][1];
	Temp[2][2] = A[2][0] * B[0][2] + A[2][1] * B[1][2] + A[2][2] * B[2][2] + A[2][3] * B[3][2];
	Temp[2][3] = A[2][0] * B[0][3] + A[2][1] * B[1][3] + A[2][2] * B[2][3] + A[2][3] * B[3][3];

	Temp[3][0] = A[3][0] * B[0][0] + A[3][1] * B[1][0] + A[3][2] * B[2][0] + A[3][3] * B[3][0];
	Temp[3][1] = A[3][0] * B[0][1] + A[3][1] * B[1][1] + A[3][2] * B[2][1] + A[3][3] * B[3][1];
	Temp[3][2] = A[3][0] * B[0][2] + A[3][1] * B[1][2] + A[3][2] * B[2][2] + A[3][3] * B[3][2];
	Temp[3][3] = A[3][0] * B[0][3] + A[3][1] * B[1][3] + A[3][2] * B[2][3] + A[3][3] * B[3][3];
	memcpy( Result, &Temp, 16*sizeof(FLOAT) );
}

/**
 * Calculate the inverse of an FMatrix.
 *
 * @param DstMatrix		FMatrix pointer to where the result should be stored
 * @param SrcMatrix		FMatrix pointer to the Matrix to be inversed
 */
FORCEINLINE void VectorMatrixInverse( void* DstMatrix, const void* SrcMatrix )
{
	typedef FLOAT Float4x4[4][4];
	const Float4x4& M = *((const Float4x4*) SrcMatrix);
	Float4x4 Result;
	FLOAT Det[4];
	Float4x4 Tmp;

	Tmp[0][0]	= M[2][2] * M[3][3] - M[2][3] * M[3][2];
	Tmp[0][1]	= M[1][2] * M[3][3] - M[1][3] * M[3][2];
	Tmp[0][2]	= M[1][2] * M[2][3] - M[1][3] * M[2][2];

	Tmp[1][0]	= M[2][2] * M[3][3] - M[2][3] * M[3][2];
	Tmp[1][1]	= M[0][2] * M[3][3] - M[0][3] * M[3][2];
	Tmp[1][2]	= M[0][2] * M[2][3] - M[0][3] * M[2][2];

	Tmp[2][0]	= M[1][2] * M[3][3] - M[1][3] * M[3][2];
	Tmp[2][1]	= M[0][2] * M[3][3] - M[0][3] * M[3][2];
	Tmp[2][2]	= M[0][2] * M[1][3] - M[0][3] * M[1][2];

	Tmp[3][0]	= M[1][2] * M[2][3] - M[1][3] * M[2][2];
	Tmp[3][1]	= M[0][2] * M[2][3] - M[0][3] * M[2][2];
	Tmp[3][2]	= M[0][2] * M[1][3] - M[0][3] * M[1][2];

	Det[0]		= M[1][1]*Tmp[0][0] - M[2][1]*Tmp[0][1] + M[3][1]*Tmp[0][2];
	Det[1]		= M[0][1]*Tmp[1][0] - M[2][1]*Tmp[1][1] + M[3][1]*Tmp[1][2];
	Det[2]		= M[0][1]*Tmp[2][0] - M[1][1]*Tmp[2][1] + M[3][1]*Tmp[2][2];
	Det[3]		= M[0][1]*Tmp[3][0] - M[1][1]*Tmp[3][1] + M[2][1]*Tmp[3][2];

	FLOAT Determinant = M[0][0]*Det[0] - M[1][0]*Det[1] + M[2][0]*Det[2] - M[3][0]*Det[3];
	const FLOAT	RDet = 1.0f / Determinant;

	Result[0][0] =  RDet * Det[0];
	Result[0][1] = -RDet * Det[1];
	Result[0][2] =  RDet * Det[2];
	Result[0][3] = -RDet * Det[3];
	Result[1][0] = -RDet * (M[1][0]*Tmp[0][0] - M[2][0]*Tmp[0][1] + M[3][0]*Tmp[0][2]);
	Result[1][1] =  RDet * (M[0][0]*Tmp[1][0] - M[2][0]*Tmp[1][1] + M[3][0]*Tmp[1][2]);
	Result[1][2] = -RDet * (M[0][0]*Tmp[2][0] - M[1][0]*Tmp[2][1] + M[3][0]*Tmp[2][2]);
	Result[1][3] =  RDet * (M[0][0]*Tmp[3][0] - M[1][0]*Tmp[3][1] + M[2][0]*Tmp[3][2]);
	Result[2][0] =  RDet * (
					M[1][0] * (M[2][1] * M[3][3] - M[2][3] * M[3][1]) -
					M[2][0] * (M[1][1] * M[3][3] - M[1][3] * M[3][1]) +
					M[3][0] * (M[1][1] * M[2][3] - M[1][3] * M[2][1])
				);
	Result[2][1] = -RDet * (
					M[0][0] * (M[2][1] * M[3][3] - M[2][3] * M[3][1]) -
					M[2][0] * (M[0][1] * M[3][3] - M[0][3] * M[3][1]) +
					M[3][0] * (M[0][1] * M[2][3] - M[0][3] * M[2][1])
				);
	Result[2][2] =  RDet * (
					M[0][0] * (M[1][1] * M[3][3] - M[1][3] * M[3][1]) -
					M[1][0] * (M[0][1] * M[3][3] - M[0][3] * M[3][1]) +
					M[3][0] * (M[0][1] * M[1][3] - M[0][3] * M[1][1])
				);
	Result[2][3] = -RDet * (
					M[0][0] * (M[1][1] * M[2][3] - M[1][3] * M[2][1]) -
					M[1][0] * (M[0][1] * M[2][3] - M[0][3] * M[2][1]) +
					M[2][0] * (M[0][1] * M[1][3] - M[0][3] * M[1][1])
				);
	Result[3][0] = -RDet * (
					M[1][0] * (M[2][1] * M[3][2] - M[2][2] * M[3][1]) -
					M[2][0] * (M[1][1] * M[3][2] - M[1][2] * M[3][1]) +
					M[3][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1])
				);
	Result[3][1] =  RDet * (
					M[0][0] * (M[2][1] * M[3][2] - M[2][2] * M[3][1]) -
					M[2][0] * (M[0][1] * M[3][2] - M[0][2] * M[3][1]) +
					M[3][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1])
				);
	Result[3][2] = -RDet * (
					M[0][0] * (M[1][1] * M[3][2] - M[1][2] * M[3][1]) -
					M[1][0] * (M[0][1] * M[3][2] - M[0][2] * M[3][1]) +
					M[3][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1])
				);
	Result[3][3] =  RDet * (
				M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
				M[1][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1]) +
				M[2][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1])
			);

	memcpy( DstMatrix, &Result, 16*sizeof(FLOAT) );
}

/**
 * Returns the minimum values of two vectors (component-wise).
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( min(Vec1.x,Vec2.x), min(Vec1.y,Vec2.y), min(Vec1.z,Vec2.z), min(Vec1.w,Vec2.w) )
 */
FORCEINLINE VectorRegister VectorMin( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Vec;
	Vec.V[0] = Min(Vec1.V[0], Vec2.V[0]);
	Vec.V[1] = Min(Vec1.V[1], Vec2.V[1]);
	Vec.V[2] = Min(Vec1.V[2], Vec2.V[2]);
	Vec.V[3] = Min(Vec1.V[3], Vec2.V[3]);
	return Vec;
}

/**
 * Returns the maximum values of two vectors (component-wise).
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( max(Vec1.x,Vec2.x), max(Vec1.y,Vec2.y), max(Vec1.z,Vec2.z), max(Vec1.w,Vec2.w) )
 */
FORCEINLINE VectorRegister VectorMax( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Vec;
	Vec.V[0] = Max(Vec1.V[0], Vec2.V[0]);
	Vec.V[1] = Max(Vec1.V[1], Vec2.V[1]);
	Vec.V[2] = Max(Vec1.V[2], Vec2.V[2]);
	Vec.V[3] = Max(Vec1.V[3], Vec2.V[3]);
	return Vec;
}

/**
 * Swizzles the 4 components of a vector and returns the result.
 *
 * @param Vec		Source vector
 * @param X			Index for which component to use for X (literal 0-3)
 * @param Y			Index for which component to use for Y (literal 0-3)
 * @param Z			Index for which component to use for Z (literal 0-3)
 * @param W			Index for which component to use for W (literal 0-3)
 * @return			The swizzled vector
 */
#define VectorSwizzle( Vec, X, Y, Z, W )	MakeVectorRegister( (Vec).V[X], (Vec).V[Y], (Vec).V[Z], (Vec).V[W] )

/**
 * Merges the XYZ components of one vector with the W component of another vector and returns the result.
 *
 * @param VecXYZ	Source vector for XYZ_
 * @param VecW		Source register for ___W (note: the fourth component is used, not the first)
 * @return			VectorRegister(VecXYZ.x, VecXYZ.y, VecXYZ.z, VecW.w)
 */
FORCEINLINE VectorRegister VectorMergeVecXYZ_VecW(const VectorRegister& VecXYZ, const VectorRegister& VecW)
{
	return MakeVectorRegister(VecXYZ.V[0], VecXYZ.V[1], VecXYZ.V[2], VecW.V[3]);
}

/**
 * Loads 4 BYTEs from unaligned memory and converts them into 4 FLOATs.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Ptr			Unaligned memory pointer to the 4 BYTEs.
 * @return				VectorRegister( FLOAT(Ptr[0]), FLOAT(Ptr[1]), FLOAT(Ptr[2]), FLOAT(Ptr[3]) )
 */
#define VectorLoadByte4( Ptr )			MakeVectorRegister( FLOAT(((const BYTE*)(Ptr))[0]), FLOAT(((const BYTE*)(Ptr))[1]), FLOAT(((const BYTE*)(Ptr))[2]), FLOAT(((const BYTE*)(Ptr))[3]) )

/**
 * Loads 4 BYTEs from unaligned memory and converts them into 4 FLOATs in reversed order.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Ptr			Unaligned memory pointer to the 4 BYTEs.
 * @return				VectorRegister( FLOAT(Ptr[3]), FLOAT(Ptr[2]), FLOAT(Ptr[1]), FLOAT(Ptr[0]) )
 */
#define VectorLoadByte4Reverse( Ptr )	MakeVectorRegister( FLOAT(((const BYTE*)(Ptr))[3]), FLOAT(((const BYTE*)(Ptr))[2]), FLOAT(((const BYTE*)(Ptr))[1]), FLOAT(((const BYTE*)(Ptr))[0]) )

/**
 * Converts the 4 FLOATs in the vector to 4 BYTEs, clamped to [0,255], and stores to unaligned memory.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Vec			Vector containing 4 FLOATs
 * @param Ptr			Unaligned memory pointer to store the 4 BYTEs.
 */
FORCEINLINE void VectorStoreByte4( const VectorRegister& Vec, void* Ptr )
{
	BYTE *BytePtr = (BYTE*) Ptr;
	BytePtr[0] = BYTE( Vec.V[0] );
	BytePtr[1] = BYTE( Vec.V[1] );
	BytePtr[2] = BYTE( Vec.V[2] );
	BytePtr[3] = BYTE( Vec.V[3] );
}

/**
 * Returns non-zero if any element in Vec1 is greater than the corresponding element in Vec2, otherwise 0.
 *
 * @param Vec1			1st source vector
 * @param Vec2			2nd source vector
 * @return				Non-zero integer if (Vec1.x > Vec2.x) || (Vec1.y > Vec2.y) || (Vec1.z > Vec2.z) || (Vec1.w > Vec2.w)
 */
FORCEINLINE DWORD VectorAnyGreaterThan(const VectorRegister& Vec1, const VectorRegister& Vec2)
{
	// Note: Bitwise OR:ing all results together to avoid branching.
	return (Vec1.V[0] > Vec2.V[0]) | (Vec1.V[1] > Vec2.V[1]) | (Vec1.V[2] > Vec2.V[2]) | (Vec1.V[3] > Vec2.V[3]);
}

/**
 * Resets the floating point registers so that they can be used again.
 * Some intrinsics use these for MMX purposes (e.g. VectorLoadByte4 and VectorStoreByte4).
 */
#define VectorResetFloatRegisters()

/**
 * Returns the control register.
 *
 * @return			The DWORD control register
 */
#define VectorGetControlRegister()		0

/**
 * Sets the control register.
 *
 * @param ControlStatus		The DWORD control status value to set
 */
#define	VectorSetControlRegister(ControlStatus)

/**
 * Control status bit to round all floating point math results towards zero.
 */
#define VECTOR_ROUND_TOWARD_ZERO		0

// Returns true if the vector contains a component that is either NAN or +/-infinite.
inline UBOOL VectorContainsNaNOrInfinite(const VectorRegister& Vec)
{
	return appIsNaN(Vec.V[0]) || appIsNaN(Vec.V[1]) || appIsNaN(Vec.V[2]) || appIsNaN(Vec.V[3]) ||
		!appIsFinite(Vec.V[0]) || !appIsFinite(Vec.V[1]) || !appIsFinite(Vec.V[2]) || !appIsFinite(Vec.V[3]);
}

// To be continued...


#endif
