/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

FORCEINLINE UBOOL VectorIsAligned(const void *Ptr)
{
#if IPHONE
	// @todo iphone: AFAICT, everything runs fine with the vectors unaligned, and they aren't unaligned
	// every time - however, I couldn't find what was making them get unaligned ever
	return TRUE;
#else
	return !(PTRINT(Ptr) & (SIMD_ALIGNMENT - 1));
#endif
}

// Returns a normalized 4 vector = Vector / |Vector|.
// There is no handling of zero length vectors, use VectorNormalizeSafe if this is a possible input.
FORCEINLINE VectorRegister VectorNormalizeAccurate( const VectorRegister& Vector )
{
	const VectorRegister SquareSum = VectorDot4(Vector, Vector);
	const VectorRegister InvLength = VectorReciprocalSqrtAccurate(SquareSum);
	const VectorRegister NormalizedVector = VectorMultiply(InvLength, Vector);
	return NormalizedVector;
}

// Returns ((Vector dot Vector) >= 1e-8) ? (Vector / |Vector|) : DefaultValue
// Uses accurate 1/sqrt, not the estimate
FORCEINLINE VectorRegister VectorNormalizeSafe( const VectorRegister& Vector, const VectorRegister& DefaultValue )
{
	const VectorRegister SquareSum = VectorDot4(Vector, Vector);
	const VectorRegister NonZeroMask = VectorCompareGE(SquareSum, GlobalVectorConstants::SmallLengthThreshold);
	const VectorRegister InvLength = VectorReciprocalSqrtAccurate(SquareSum);
	const VectorRegister NormalizedVector = VectorMultiply(InvLength, Vector);
	return VectorSelect(NonZeroMask, NormalizedVector, DefaultValue);
}

/**
 * Returns non-zero if any element in Vec1 is lesser than the corresponding element in Vec2, otherwise 0.
 *
 * @param Vec1			1st source vector
 * @param Vec2			2nd source vector
 * @return				Non-zero integer if (Vec1.x < Vec2.x) || (Vec1.y < Vec2.y) || (Vec1.z < Vec2.z) || (Vec1.w < Vec2.w)
 */
FORCEINLINE DWORD VectorAnyLesserThan( VectorRegister Vec1, VectorRegister Vec2 )
{
	return VectorAnyGreaterThan( Vec2, Vec1 );
}

/**
 * Returns non-zero if all elements in Vec1 are greater than the corresponding elements in Vec2, otherwise 0.
 *
 * @param Vec1			1st source vector
 * @param Vec2			2nd source vector
 * @return				Non-zero integer if (Vec1.x > Vec2.x) && (Vec1.y > Vec2.y) && (Vec1.z > Vec2.z) && (Vec1.w > Vec2.w)
 */
FORCEINLINE DWORD VectorAllGreaterThan( VectorRegister Vec1, VectorRegister Vec2 )
{
	return !VectorAnyGreaterThan( Vec2, Vec1 );
}

/**
 * Returns non-zero if all elements in Vec1 are lesser than the corresponding elements in Vec2, otherwise 0.
 *
 * @param Vec1			1st source vector
 * @param Vec2			2nd source vector
 * @return				Non-zero integer if (Vec1.x < Vec2.x) && (Vec1.y < Vec2.y) && (Vec1.z < Vec2.z) && (Vec1.w < Vec2.w)
 */
FORCEINLINE DWORD VectorAllLesserThan( VectorRegister Vec1, VectorRegister Vec2 )
{
	return !VectorAnyGreaterThan( Vec1, Vec2 );
}

/*----------------------------------------------------------------------------
	VectorRegister specialization of templates.
----------------------------------------------------------------------------*/

/** Returns the smaller of the two values (operates on each component individually) */
template<> FORCEINLINE VectorRegister Min(const VectorRegister A, const VectorRegister B)
{
	return VectorMin(A, B);
}

/** Returns the larger of the two values (operates on each component individually) */
template<> FORCEINLINE VectorRegister Max(const VectorRegister A, const VectorRegister B)
{
	return VectorMax(A, B);
}

// Specialization of Lerp template that works with vector registers
template<> FORCEINLINE VectorRegister Lerp(const VectorRegister& A, const VectorRegister& B, const VectorRegister& Alpha)
{
	const VectorRegister Delta = VectorSubtract(B, A);
	return VectorMultiplyAdd(Alpha, Delta, A);
}

// A and B are quaternions.  The result is A + (|A.B| >= 0 ? 1 : -1) * B
FORCEINLINE VectorRegister VectorAccumulateQuaternionShortestPath(const VectorRegister& A, const VectorRegister& B)
{
	// Blend rotation
	//     To ensure the 'shortest route', we make sure the dot product between the both rotations is positive.
	//     const FLOAT Bias = (|A.B| >= 0 ? 1 : -1)
	//     return A + B * Bias;
	const VectorRegister Zero = VectorZero();
	const VectorRegister RotationDot = VectorDot4(A, B);
	const VectorRegister QuatRotationDirMask = VectorCompareGE(RotationDot, Zero);
	const VectorRegister NegativeB = VectorSubtract(Zero, B);
	const VectorRegister BiasTimesB = VectorSelect(QuatRotationDirMask, B, NegativeB);
	return VectorAdd(A, BiasTimesB);
}

// Normalize quaternion ( result = (Q.Q >= 1e-8) ? (Q / |Q|) : (0,0,0,1) )
FORCEINLINE VectorRegister VectorNormalizeQuaternion(const VectorRegister& UnnormalizedQuat)
{
	return VectorNormalizeSafe(UnnormalizedQuat, GlobalVectorConstants::Float0001);
}

/** 
 * Fast Linear Quaternion Interpolation for quaternions stored in VectorRegisters.
 * Result is NOT normalized.
 */
FORCEINLINE VectorRegister VectorLerpQuat(const VectorRegister& A, const VectorRegister& B, const VectorRegister& Alpha)
{
	// Blend rotation
	//     To ensure the 'shortest route', we make sure the dot product between the both rotations is positive.
	//     const FLOAT Bias = (|A.B| >= 0 ? 1 : -1)
	//     Rotation = (B * Alpha) + (A * (Bias * (1.f - Alpha)));
	const VectorRegister Zero = VectorZero();

	const VectorRegister OneMinusAlpha = VectorSubtract(VectorOne(), Alpha);

	const VectorRegister RotationDot = VectorDot4(A, B);
	const VectorRegister QuatRotationDirMask = VectorCompareGE(RotationDot, Zero);
	const VectorRegister NegativeA = VectorSubtract(Zero, A);
	const VectorRegister BiasTimesA = VectorSelect(QuatRotationDirMask, A, NegativeA);
	const VectorRegister BTimesWeight = VectorMultiply(B, Alpha);
	const VectorRegister UnnormalizedResult = VectorMultiplyAdd(BiasTimesA, OneMinusAlpha, BTimesWeight);

	return UnnormalizedResult;
}

/** 
 * Bi-Linear Quaternion interpolation for quaternions stored in VectorRegisters.
 * Result is NOT normalized.
 */
FORCEINLINE VectorRegister VectorBiLerpQuat(const VectorRegister& P00, const VectorRegister& P10, const VectorRegister& P01, const VectorRegister& P11, const VectorRegister& FracX, const VectorRegister& FracY)
{
	return VectorLerpQuat(
		VectorLerpQuat(P00, P10, FracX),
		VectorLerpQuat(P01, P11, FracX),
		FracY);
}
