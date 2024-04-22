/*=============================================================================
	UnMathNeon.h: ARM-NEON-specific vector intrinsics

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNMATHNEON_H__
#define __UNMATHNEON_H__

// Include the intrinsic functions header
#include <arm_neon.h>

/*=============================================================================
 *	Helpers:
 *============================================================================*/

/** 16-byte vector register type */
typedef float32x4_t __attribute((aligned(16))) VectorRegister;
typedef uint32x4_t  __attribute((aligned(16))) IntVectorRegister;

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
	union { VectorRegister V; DWORD F[4]; } Tmp;
	Tmp.F[0] = X;
	Tmp.F[1] = Y;
	Tmp.F[2] = Z;
	Tmp.F[3] = W;
	return Tmp.V;
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
	union { VectorRegister V; FLOAT F[4]; } Tmp;
	Tmp.F[0] = X;
	Tmp.F[1] = Y;
	Tmp.F[2] = Z;
	Tmp.F[3] = W;
	return Tmp.V;
}

static const VectorRegister Vec1110 = MakeVectorRegister( 1.0f, 1.0f, 1.0f, 0.0f );
static const VectorRegister Vec0001 = MakeVectorRegister( 0.0f, 0.0f, 0.0f, 1.0f );
static const VectorRegister Vec255 = MakeVectorRegister( 255.f, 255.f, 255.f, 255.f );

/*
#define VectorPermute(Vec1, Vec2, Mask) my_perm(Vec1, Vec2, Mask)

/ ** Reads NumBytesMinusOne+1 bytes from the address pointed to by Ptr, always reading the aligned 16 bytes containing the start of Ptr, but only reading the next 16 bytes if the data straddles the boundary * /
FORCEINLINE VectorRegister VectorLoadNPlusOneUnalignedBytes(const void* Ptr, INT NumBytesMinusOne)
{
	return VectorPermute( my_ld (0, (float*)Ptr), my_ld(NumBytesMinusOne, (float*)Ptr), my_lvsl(0, (float*)Ptr) );
}
*/


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
FORCEINLINE VectorRegister VectorZero()
{	
	return vdupq_n_f32( 0.0f );
}

/**
 * Returns a vector with all ones.
 *
 * @return		VectorRegister(1.0f, 1.0f, 1.0f, 1.0f)
 */
FORCEINLINE VectorRegister VectorOne()	
{
	return vdupq_n_f32( 1.0f );
}

/**
 * Loads 4 FLOATs from unaligned memory.
 *
 * @param Ptr	Unaligned memory pointer to the 4 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], Ptr[3])
 */
FORCEINLINE VectorRegister VectorLoad( const void * Ptr )
{
	return vld1q_f32( (float32_t*)Ptr );
}

/**
 * Loads 3 FLOATs from unaligned memory and leaves W undefined.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], undefined)
 */
FORCEINLINE VectorRegister VectorLoadFloat3( const void * Ptr ) 
{
	return vld1q_f32( (float32_t*)Ptr );
}

/**
 * Loads 3 FLOATs from unaligned memory and sets W=0.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], 0.0f)
 */
FORCEINLINE VectorRegister VectorLoadFloat3_W0( const void * Ptr )
{
	return vmulq_f32( vld1q_f32( (float32_t*)Ptr ), Vec1110 );
}

/**
 * Loads 3 FLOATs from unaligned memory and sets W=1.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], 1.0f)
 */
FORCEINLINE VectorRegister VectorLoadFloat3_W1( const void * Ptr )
{
	return vmlaq_f32( Vec0001, vld1q_f32( (float32_t*)Ptr ), Vec1110 );
}

/**
 * Sets a single component of a vector. Must be a define since ElementIndex needs to be a constant integer
 */
#define VectorSetComponent( Vec, ElementIndex, Scalar ) vsetq_lane_f32( Scalar, Vec, ElementIndex )


/**
 * Loads 4 FLOATs from aligned memory.
 *
 * @param Ptr	Aligned memory pointer to the 4 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], Ptr[3])
 */
FORCEINLINE VectorRegister VectorLoadAligned( const void * Ptr )
{
	return vld1q_f32( (float32_t*)Ptr );
}

/**
 * Loads 1 FLOAT from unaligned memory and replicates it to all 4 elements.
 *
 * @param Ptr	Unaligned memory pointer to the FLOAT
 * @return		VectorRegister(Ptr[0], Ptr[0], Ptr[0], Ptr[0])
 */
FORCEINLINE VectorRegister VectorLoadFloat1( const void *Ptr )
{
	return vdupq_n_f32( ((float32_t *)Ptr)[0] );
}
/**
 * Creates a vector out of three FLOATs and leaves W undefined.
 *
 * @param X		1st FLOAT component
 * @param Y		2nd FLOAT component
 * @param Z		3rd FLOAT component
 * @return		VectorRegister(X, Y, Z, undefined)
 */
FORCEINLINE VectorRegister VectorSetFloat3( FLOAT X, FLOAT Y, FLOAT Z )
{
	union { VectorRegister V; float F[4]; } Tmp;
	Tmp.F[0] = X;
	Tmp.F[1] = Y;
	Tmp.F[2] = Z;
	return Tmp.V;
}

/**
 * Creates a vector out of four FLOATs.
 *
 * @param X		1st FLOAT component
 * @param Y		2nd FLOAT component
 * @param Z		3rd FLOAT component
 * @param W		4th FLOAT component
 * @return		VectorRegister(X, Y, Z, W)
 */
FORCEINLINE VectorRegister VectorSet( FLOAT X, FLOAT Y, FLOAT Z, FLOAT W ) 
{
	return MakeVectorRegister( X, Y, Z, W );
}

/**
 * Stores a vector to aligned memory.
 *
 * @param Vec	Vector to store
 * @param Ptr	Aligned memory pointer
 */
FORCEINLINE void VectorStoreAligned( VectorRegister Vec, void * Ptr )
{
	vst1q_f32( (float32_t *)Ptr, Vec );
}

/**
 * Stores a vector to memory (aligned or unaligned).
 *
 * @param Vec	Vector to store
 * @param Ptr	Memory pointer
 */
FORCEINLINE void VectorStore( VectorRegister Vec, void * Ptr )
{
	vst1q_f32( (float32_t *)Ptr, Vec );
}

/**
 * Stores the XYZ components of a vector to unaligned memory.
 *
 * @param Vec	Vector to store XYZ
 * @param Ptr	Unaligned memory pointer
 */
FORCEINLINE void VectorStoreFloat3( VectorRegister & Vec, void * Ptr )
{
	vst1q_lane_f32( ((float32_t *)Ptr) + 0, Vec, 0 );
	vst1q_lane_f32( ((float32_t *)Ptr) + 1, Vec, 1 );
	vst1q_lane_f32( ((float32_t *)Ptr) + 2, Vec, 2 );
}

/**
 * Stores the X component of a vector to unaligned memory.
 *
 * @param Vec	Vector to store X
 * @param Ptr	Unaligned memory pointer
 */
FORCEINLINE void VectorStoreFloat1( VectorRegister Vec, void * Ptr )
{
	vst1q_lane_f32( (float32_t *)Ptr, Vec, 0 );
}

/**
 * Replicates one element into all four elements and returns the new vector. Must be a #define for ELementIndex
 * to be a constant integer
 *
 * @param Vec			Source vector
 * @param ElementIndex	Index (0-3) of the element to replicate
 * @return				VectorRegister( Vec[ElementIndex], Vec[ElementIndex], Vec[ElementIndex], Vec[ElementIndex] )
 */
#define VectorReplicate( Vec, ElementIndex ) vdupq_n_f32(vgetq_lane_f32(Vec, ElementIndex))



/**
 * Returns the absolute value (component-wise).
 *
 * @param Vec			Source vector
 * @return				VectorRegister( abs(Vec.x), abs(Vec.y), abs(Vec.z), abs(Vec.w) )
 */
FORCEINLINE VectorRegister VectorAbs( VectorRegister Vec )	
{
	return vabsq_f32( Vec );
}

/**
 * Returns the negated value (component-wise).
 *
 * @param Vec			Source vector
 * @return				VectorRegister( -Vec.x, -Vec.y, -Vec.z, -Vec.w )
 */
FORCEINLINE VectorRegister VectorNegate( VectorRegister Vec )
{
	return vnegq_f32( Vec );
}

/**
 * Adds two vectors (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x+Vec2.x, Vec1.y+Vec2.y, Vec1.z+Vec2.z, Vec1.w+Vec2.w )
 */
FORCEINLINE VectorRegister VectorAdd( VectorRegister Vec1, VectorRegister Vec2 )
{
	return vaddq_f32( Vec1, Vec2 );
}

/**
 * Subtracts a vector from another (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x-Vec2.x, Vec1.y-Vec2.y, Vec1.z-Vec2.z, Vec1.w-Vec2.w )
 */
FORCEINLINE VectorRegister VectorSubtract( VectorRegister Vec1, VectorRegister Vec2 )
{
	return vsubq_f32( Vec1, Vec2 );
}

/**
 * Multiplies two vectors (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x*Vec2.x, Vec1.y*Vec2.y, Vec1.z*Vec2.z, Vec1.w*Vec2.w )
 */
FORCEINLINE VectorRegister VectorMultiply( VectorRegister Vec1, VectorRegister Vec2 ) 
{
	return vmulq_f32( Vec1, Vec2 );
}

/**
 * Multiplies two vectors (component-wise), adds in the third vector and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @param Vec3	3rd vector
 * @return		VectorRegister( Vec1.x*Vec2.x + Vec3.x, Vec1.y*Vec2.y + Vec3.y, Vec1.z*Vec2.z + Vec3.z, Vec1.w*Vec2.w + Vec3.w )
 */
FORCEINLINE VectorRegister VectorMultiplyAdd( VectorRegister Vec1, VectorRegister Vec2, VectorRegister Vec3 )
{
	return vmlaq_f32( Vec3, Vec1, Vec2 );
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
	VectorRegister Temp = VectorMultiply( Vec1, Vec2 );
	Temp = vsetq_lane_f32( 0.0f, Temp, 3 );
	float32x2_t sum = vpadd_f32( vget_low_f32( Temp ), vget_high_f32( Temp ) );
	sum = vpadd_f32( sum, sum );
	return vdupq_lane_f32( sum, 0 );
}

/**
 * Calculates the dot4 product of two vectors and returns a vector with the result in all 4 components.
 * Only really efficient on Xbox 360.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		d = dot4(Vec1.xyzw, Vec2.xyzw), VectorRegister( d, d, d, d )
 */
FORCEINLINE VectorRegister VectorDot4( VectorRegister Vec1, VectorRegister Vec2 )
{
	VectorRegister Temp = VectorMultiply( Vec1, Vec2 );
	float32x2_t sum = vpadd_f32( vget_low_f32( Temp ), vget_high_f32( Temp ) );
	sum = vpadd_f32( sum, sum );
	return vdupq_lane_f32( sum, 0 );
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
	return (VectorRegister)vceqq_f32( Vec1, Vec2 );
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
	return (VectorRegister)vmvnq_u32( vceqq_f32( Vec1, Vec2 ) );
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
	return (VectorRegister)vcgtq_f32( Vec1, Vec2 );
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
	return (VectorRegister)vcgeq_f32( Vec1, Vec2 );
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
	return vbslq_f32((IntVectorRegister)Mask, Vec1, Vec2);
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
	return (VectorRegister)vorrq_u32( (IntVectorRegister)Vec1, (IntVectorRegister)Vec2 );
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
	return (VectorRegister)vandq_u32( (IntVectorRegister)Vec1, (IntVectorRegister)Vec2 );
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
	return (VectorRegister)veorq_u32( (IntVectorRegister)Vec1, (IntVectorRegister)Vec2 );
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

#define VectorSwizzle( Vec, X, Y, Z, W ) __builtin_shufflevector(Vec, Vec, X, Y, Z, W)

/**
 * Calculates the cross product of two vectors (XYZ components). W is set to 0.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		cross(Vec1.xyz, Vec2.xyz). W is set to 0.
 */
FORCEINLINE VectorRegister VectorCross( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister C = VectorSubtract( VectorMultiply( VectorSwizzle(Vec1,1,2,0,1), VectorSwizzle(Vec2,2,0,1,3) ), VectorMultiply( VectorSwizzle(Vec1,2,0,1,3), VectorSwizzle(Vec2,1,2,0,1) ) );
	C = VectorSetComponent( C, 3, 0.0f );
	return C;
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
	//@TODO: Optimize this
	union { VectorRegister V; FLOAT F[4]; } B, E;
	B.V = Base;
	E.V = Exponent;
	return MakeVectorRegister( powf(B.F[0], E.F[0]), powf(B.F[1], E.F[1]), powf(B.F[2], E.F[2]), powf(B.F[3], E.F[3]) );
}

/**
* Returns an estimate of 1/sqrt(c) for each component of the vector
*
* @param Vector		Vector 
* @return			VectorRegister(1/sqrt(t), 1/sqrt(t), 1/sqrt(t), 1/sqrt(t))
*/
#define VectorReciprocalSqrt(Vec)					vrsqrteq_f32(Vec)

/**
 * Computes an estimate of the reciprocal of a vector (component-wise) and returns the result.
 *
 * @param Vec	1st vector
 * @return		VectorRegister( (Estimate) 1.0f / Vec.x, (Estimate) 1.0f / Vec.y, (Estimate) 1.0f / Vec.z, (Estimate) 1.0f / Vec.w )
 */
#define VectorReciprocal(Vec)						vrecpeq_f32(Vec)


/**
* Return Reciprocal Length of the vector
*
* @param Vector		Vector 
* @return			VectorRegister(rlen, rlen, rlen, rlen) when rlen = 1/sqrt(dot4(V))
*/
FORCEINLINE VectorRegister VectorReciprocalLen( const VectorRegister& Vector )
{
	VectorRegister LengthSquared = VectorDot4( Vector, Vector );
	return VectorReciprocalSqrt( LengthSquared );
}

/**
* Return the reciprocal of the square root of each component
*
* @param Vector		Vector 
* @return			VectorRegister(1/sqrt(Vec.X), 1/sqrt(Vec.Y), 1/sqrt(Vec.Z), 1/sqrt(Vec.W))
*/
FORCEINLINE VectorRegister VectorReciprocalSqrtAccurate(const VectorRegister& Vec)
{
	// Perform a single pass of Newton-Raphson iteration on the hardware estimate
	// This is a builtin instruction (VRSQRTS)

	// Initial estimate
	VectorRegister RecipSqrt = VectorReciprocalSqrt(Vec);

	// Two refinement
	RecipSqrt = vmulq_f32(vrsqrtsq_f32(Vec, vmulq_f32(RecipSqrt, RecipSqrt)), RecipSqrt);
	return vmulq_f32(vrsqrtsq_f32(Vec, vmulq_f32(RecipSqrt, RecipSqrt)), RecipSqrt);
}

/**
 * Computes the reciprocal of a vector (component-wise) and returns the result.
 *
 * @param Vec	1st vector
 * @return		VectorRegister( 1.0f / Vec.x, 1.0f / Vec.y, 1.0f / Vec.z, 1.0f / Vec.w )
 */
FORCEINLINE VectorRegister VectorReciprocalAccurate(const VectorRegister& Vec)
{
	// Perform two passes of Newton-Raphson iteration on the hardware estimate
	// This is a built-in instruction (VRECPS)

	// Initial estimate
	VectorRegister Reciprocal = VectorReciprocal(Vec);

	// 2 refinement iterations
	Reciprocal = vmulq_f32(vrecpsq_f32(Vec, Reciprocal), Reciprocal);
	return vmulq_f32(vrecpsq_f32(Vec, Reciprocal), Reciprocal);
}

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
#define VectorSet_W0( Vec )		VectorSetComponent( Vec, 3, 0.0f )

/**
* Loads XYZ and sets W=1.
*
* @param Vector	VectorRegister
* @return		VectorRegister(X, Y, Z, 1.0f)
*/
#define VectorSet_W1( Vec )		VectorSetComponent( Vec, 3, 1.0f )

/**
 * Multiplies two 4x4 matrices.
 *
 * @param Result	Pointer to where the result should be stored
 * @param Matrix1	Pointer to the first matrix
 * @param Matrix2	Pointer to the second matrix
 */
FORCEINLINE void VectorMatrixMultiply( void *Result, const void* Matrix1, const void* Matrix2 )
{
	const VectorRegister *A	= (const VectorRegister *) Matrix1;
	const VectorRegister *B	= (const VectorRegister *) Matrix2;
	VectorRegister *R		= (VectorRegister *) Result;
	VectorRegister Temp, R0, R1, R2, R3;

	// First row of result (Matrix1[0] * Matrix2).
	Temp    = vmulq_lane_f32(       B[0], vget_low_f32(  A[0] ), 0 );
	Temp    = vmlaq_lane_f32( Temp, B[1], vget_low_f32(  A[0] ), 1 );
	Temp    = vmlaq_lane_f32( Temp, B[2], vget_high_f32( A[0] ), 0 );
	R0      = vmlaq_lane_f32( Temp, B[3], vget_high_f32( A[0] ), 1 );

	// Second row of result (Matrix1[1] * Matrix2).
	Temp    = vmulq_lane_f32(       B[0], vget_low_f32(  A[1] ), 0 );
	Temp    = vmlaq_lane_f32( Temp, B[1], vget_low_f32(  A[1] ), 1 );
	Temp    = vmlaq_lane_f32( Temp, B[2], vget_high_f32( A[1] ), 0 );
	R1      = vmlaq_lane_f32( Temp, B[3], vget_high_f32( A[1] ), 1 );

	// Third row of result (Matrix1[2] * Matrix2).
	Temp    = vmulq_lane_f32(       B[0], vget_low_f32(  A[2] ), 0 );
	Temp    = vmlaq_lane_f32( Temp, B[1], vget_low_f32(  A[2] ), 1 );
	Temp    = vmlaq_lane_f32( Temp, B[2], vget_high_f32( A[2] ), 0 );
	R2      = vmlaq_lane_f32( Temp, B[3], vget_high_f32( A[2] ), 1 );

	// Fourth row of result (Matrix1[3] * Matrix2).
	Temp    = vmulq_lane_f32(       B[0], vget_low_f32(  A[3] ), 0 );
	Temp    = vmlaq_lane_f32( Temp, B[1], vget_low_f32(  A[3] ), 1 );
	Temp    = vmlaq_lane_f32( Temp, B[2], vget_high_f32( A[3] ), 0 );
	R3      = vmlaq_lane_f32( Temp, B[3], vget_high_f32( A[3] ), 1 );

	// Store result
	R[0] = R0;
	R[1] = R1;
	R[2] = R2;
	R[3] = R3;
}

/**
 * Calculate the inverse of an FMatrix.
 *
 * @param DstMatrix		FMatrix pointer to where the result should be stored
 * @param SrcMatrix		FMatrix pointer to the Matrix to be inversed
 */
// OPTIMIZE ME: stolen from UnMathFpu.h
FORCEINLINE void VectorMatrixInverse( void *DstMatrix, const void *SrcMatrix )
{
	typedef FLOAT Float4x4[4][4];
	const Float4x4& M = *((const Float4x4*) SrcMatrix);
	Float4x4 Result;
	FLOAT Det[4];
	Float4x4 Tmp;
	
	Tmp[0][0]       = M[2][2] * M[3][3] - M[2][3] * M[3][2];
	Tmp[0][1]       = M[1][2] * M[3][3] - M[1][3] * M[3][2];
	Tmp[0][2]       = M[1][2] * M[2][3] - M[1][3] * M[2][2];
	
	Tmp[1][0]       = M[2][2] * M[3][3] - M[2][3] * M[3][2];
	Tmp[1][1]       = M[0][2] * M[3][3] - M[0][3] * M[3][2];
	Tmp[1][2]       = M[0][2] * M[2][3] - M[0][3] * M[2][2];
	
	Tmp[2][0]       = M[1][2] * M[3][3] - M[1][3] * M[3][2];
	Tmp[2][1]       = M[0][2] * M[3][3] - M[0][3] * M[3][2];
	Tmp[2][2]       = M[0][2] * M[1][3] - M[0][3] * M[1][2];
	
	Tmp[3][0]       = M[1][2] * M[2][3] - M[1][3] * M[2][2];
	Tmp[3][1]       = M[0][2] * M[2][3] - M[0][3] * M[2][2];
	Tmp[3][2]       = M[0][2] * M[1][3] - M[0][3] * M[1][2];
	
	Det[0]          = M[1][1]*Tmp[0][0] - M[2][1]*Tmp[0][1] + M[3][1]*Tmp[0][2];
	Det[1]          = M[0][1]*Tmp[1][0] - M[2][1]*Tmp[1][1] + M[3][1]*Tmp[1][2];
	Det[2]          = M[0][1]*Tmp[2][0] - M[1][1]*Tmp[2][1] + M[3][1]*Tmp[2][2];
	Det[3]          = M[0][1]*Tmp[3][0] - M[1][1]*Tmp[3][1] + M[2][1]*Tmp[3][2];
	
	FLOAT Determinant = M[0][0]*Det[0] - M[1][0]*Det[1] + M[2][0]*Det[2] - M[3][0]*Det[3];
	const FLOAT     RDet = 1.0f / Determinant;
	
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
FORCEINLINE VectorRegister VectorMin( VectorRegister Vec1, VectorRegister Vec2 )
{
	return vminq_f32( Vec1, Vec2 );
}

/**
 * Returns the maximum values of two vectors (component-wise).
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( max(Vec1.x,Vec2.x), max(Vec1.y,Vec2.y), max(Vec1.z,Vec2.z), max(Vec1.w,Vec2.w) )
 */
FORCEINLINE VectorRegister VectorMax( VectorRegister Vec1, VectorRegister Vec2 )
{
	return vmaxq_f32( Vec1, Vec2 );
}

/**
 * Merges the XYZ components of one vector with the W component of another vector and returns the result.
 *
 * @param VecXYZ	Source vector for XYZ_
 * @param VecW		Source register for ___W (note: the fourth component is used, not the first)
 * @return			VectorRegister(VecXYZ.x, VecXYZ.y, VecXYZ.z, VecW.w)
 */
FORCEINLINE VectorRegister VectorMergeVecXYZ_VecW(const VectorRegister& VecXYZ, const VectorRegister& VecW)
{
	return vsetq_lane_f32(vgetq_lane_f32(VecW, 3), VecXYZ, 3);
}

/**
 * Loads 4 BYTEs from unaligned memory and converts them into 4 FLOATs.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Ptr			Unaligned memory pointer to the 4 BYTEs.
 * @return				VectorRegister( FLOAT(Ptr[0]), FLOAT(Ptr[1]), FLOAT(Ptr[2]), FLOAT(Ptr[3]) )
 */
FORCEINLINE VectorRegister VectorLoadByte4( const void * Ptr )
{
	// OPTIMIZE ME!
	const BYTE *P = (const BYTE *)Ptr;
	return MakeVectorRegister( (FLOAT)P[0], (FLOAT)P[1], (FLOAT)P[2], (FLOAT)P[3] );
}

/**
 * Loads 4 BYTEs from unaligned memory and converts them into 4 FLOATs in reversed order.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Ptr			Unaligned memory pointer to the 4 BYTEs.
 * @return				VectorRegister( FLOAT(Ptr[3]), FLOAT(Ptr[2]), FLOAT(Ptr[1]), FLOAT(Ptr[0]) )
 */
FORCEINLINE VectorRegister VectorLoadByte4Reverse( const void * Ptr )
{
	// OPTIMIZE ME!
	const BYTE *P = (const BYTE *)Ptr;
	return MakeVectorRegister( (FLOAT)P[3], (FLOAT)P[2], (FLOAT)P[1], (FLOAT)P[0] );
}

/**
 * Converts the 4 FLOATs in the vector to 4 BYTEs, clamped to [0,255], and stores to unaligned memory.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Vec			Vector containing 4 FLOATs
 * @param Ptr			Unaligned memory pointer to store the 4 BYTEs.
 */
FORCEINLINE void VectorStoreByte4( VectorRegister Vec, void * Ptr )
{
    uint16x8_t u16x8 = (uint16x8_t)vcvtq_u32_f32( VectorMin( Vec, Vec255 ) );
    uint8x8_t u8x8 = (uint8x8_t)vget_low_u16( vuzpq_u16( u16x8, u16x8 ).val[0] );
    u8x8 = vuzp_u8( u8x8, u8x8 ).val[0];
	uint32_t buf[2];
	vst1_u8( (uint8_t *)buf, u8x8 );
	*(uint32_t *)Ptr = buf[0]; 
}

/**
 * Returns non-zero if any element in Vec1 is greater than the corresponding element in Vec2, otherwise 0.
 *
 * @param Vec1			1st source vector
 * @param Vec2			2nd source vector
 * @return				Non-zero integer if (Vec1.x > Vec2.x) || (Vec1.y > Vec2.y) || (Vec1.z > Vec2.z) || (Vec1.w > Vec2.w)
 */
FORCEINLINE INT VectorAnyGreaterThan( VectorRegister Vec1, VectorRegister Vec2 )
{
    uint16x8_t u16x8 = (uint16x8_t)vcgtq_f32( Vec1, Vec2 );
    uint8x8_t u8x8 = (uint8x8_t)vget_low_u16( vuzpq_u16( u16x8, u16x8 ).val[0] );
    u8x8 = vuzp_u8( u8x8, u8x8 ).val[0];
	uint32_t buf[2];
	vst1_u8( (uint8_t *)buf, u8x8 );
	return (INT)buf[0]; // each byte of output corresponds to a component comparison
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

static const VectorRegister QMULTI_SIGN_MASK0 = MakeVectorRegister( 1.f, -1.f, 1.f, -1.f );
static const VectorRegister QMULTI_SIGN_MASK1 = MakeVectorRegister( 1.f, 1.f, -1.f, -1.f );
static const VectorRegister QMULTI_SIGN_MASK2 = MakeVectorRegister( -1.f, 1.f, 1.f, -1.f );

/**
* Multiplies two quaternions: The order matters.
*
* @param Result	Returns Quat1 * Quat2
* @param Quat1	First quaternion
* @param Quat2	Second quaternion
*/
FORCEINLINE VectorRegister VectorQuaternionMultiply2( const VectorRegister& Quat1, const VectorRegister& Quat2 )
{
	VectorRegister Result = VectorMultiply(VectorReplicate(Quat1, 3), Quat2);
	Result = VectorMultiplyAdd( VectorMultiply(VectorReplicate(Quat1, 0), VectorSwizzle(Quat2, 3,2,1,0)), QMULTI_SIGN_MASK0, Result);
	Result = VectorMultiplyAdd( VectorMultiply(VectorReplicate(Quat1, 1), VectorSwizzle(Quat2, 2,3,0,1)), QMULTI_SIGN_MASK1, Result);
	Result = VectorMultiplyAdd( VectorMultiply(VectorReplicate(Quat1, 2), VectorSwizzle(Quat2, 1,0,3,2)), QMULTI_SIGN_MASK2, Result);

	return Result;
}

/**
* Multiplies two quaternions: The order matters
*
* @param Result	Pointer to where the result should be stored
* @param Quat1	Pointer to the first quaternion (must not be the destination)
* @param Quat2	Pointer to the second quaternion (must not be the destination)
*/
FORCEINLINE void VectorQuaternionMultiply( void* RESTRICT Result, const void* RESTRICT Quat1, const void* RESTRICT Quat2)
{
	*((VectorRegister *)Result) = VectorQuaternionMultiply2(*((const VectorRegister *)Quat1), *((const VectorRegister *)Quat2));
}

// Returns true if the vector contains a component that is either NAN or +/-infinite.
inline UBOOL VectorContainsNaNOrInfinite(const VectorRegister& Vec)
{
	checkMsg(false, "Not implemented for NEON"); //@TODO: Implement this method for NEON
	return FALSE;
}

// To be continued...


#endif
