/*=============================================================================
	UnMath.inl: Unreal inlined math functions
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

	NOTE: This file should ONLY be included by UnMath.h!
=============================================================================*/


/**
 * FMatrix inline functions.
 */


// Constructors.

FORCEINLINE FMatrix::FMatrix()
{
}

FORCEINLINE FMatrix::FMatrix(const FPlane& InX,const FPlane& InY,const FPlane& InZ,const FPlane& InW)
{
	M[0][0] = InX.X; M[0][1] = InX.Y;  M[0][2] = InX.Z;  M[0][3] = InX.W;
	M[1][0] = InY.X; M[1][1] = InY.Y;  M[1][2] = InY.Z;  M[1][3] = InY.W;
	M[2][0] = InZ.X; M[2][1] = InZ.Y;  M[2][2] = InZ.Z;  M[2][3] = InZ.W;
	M[3][0] = InW.X; M[3][1] = InW.Y;  M[3][2] = InW.Z;  M[3][3] = InW.W;
}

FORCEINLINE FMatrix::FMatrix(const FVector& InX,const FVector& InY,const FVector& InZ,const FVector& InW)
{
	M[0][0] = InX.X; M[0][1] = InX.Y;  M[0][2] = InX.Z;  M[0][3] = 0.0f;
	M[1][0] = InY.X; M[1][1] = InY.Y;  M[1][2] = InY.Z;  M[1][3] = 0.0f;
	M[2][0] = InZ.X; M[2][1] = InZ.Y;  M[2][2] = InZ.Z;  M[2][3] = 0.0f;
	M[3][0] = InW.X; M[3][1] = InW.Y;  M[3][2] = InW.Z;  M[3][3] = 1.0f;
}


#if XBOX
	/**
	 * XMMATRIX to FMatrix conversion constructor
	 *
	 * @param InMatrix	XMMATRIX to convert to FMatrix
	 */
	FORCEINLINE FMatrix::FMatrix( const XMMATRIX& InMatrix )
	{
		*((XMMATRIX*)this) = InMatrix;
	}

	/**
	 * FMatrix to XMMATRIX conversion operator.
	 */
	FORCEINLINE FMatrix::operator XMMATRIX() const
	{
		return *((XMMATRIX*)this);
	}
#endif


inline void FMatrix::SetIdentity()
{
	M[0][0] = 1; M[0][1] = 0;  M[0][2] = 0;  M[0][3] = 0;
	M[1][0] = 0; M[1][1] = 1;  M[1][2] = 0;  M[1][3] = 0;
	M[2][0] = 0; M[2][1] = 0;  M[2][2] = 1;  M[2][3] = 0;
	M[3][0] = 0; M[3][1] = 0;  M[3][2] = 0;  M[3][3] = 1;
}


FORCEINLINE void FMatrix::operator*=(const FMatrix& Other)
{
	VectorMatrixMultiply( this, this, &Other );
}


FORCEINLINE FMatrix FMatrix::operator*(const FMatrix& Other) const
{
	FMatrix Result;
	VectorMatrixMultiply( &Result, this, &Other );
	return Result;
}


FORCEINLINE FMatrix	FMatrix::operator+(const FMatrix& Other) const
{
	FMatrix ResultMat;

	for(INT X = 0;X < 4;X++)
	{
		for(INT Y = 0;Y < 4;Y++)
		{
			ResultMat.M[X][Y] = M[X][Y]+Other.M[X][Y];
		}
	}

	return ResultMat;
}

FORCEINLINE void FMatrix::operator+=(const FMatrix& Other)
{
	*this = *this + Other;
}

FORCEINLINE FMatrix	FMatrix::operator*(FLOAT Other) const
{
	FMatrix ResultMat;

	for(INT X = 0;X < 4;X++)
	{
		for(INT Y = 0;Y < 4;Y++)
		{
			ResultMat.M[X][Y] = M[X][Y]*Other;
		}
	}

	return ResultMat;
}

FORCEINLINE void FMatrix::operator*=(FLOAT Other)
{
	*this = *this*Other;
}

// Comparison operators.

inline UBOOL FMatrix::operator==(const FMatrix& Other) const
{
	for(INT X = 0;X < 4;X++)
	{
		for(INT Y = 0;Y < 4;Y++)
		{
			if(M[X][Y] != Other.M[X][Y])
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

// Error-tolerant comparison.
inline UBOOL FMatrix::Equals(const FMatrix& Other, FLOAT Tolerance/*=KINDA_SMALL_NUMBER*/) const
{
	for(INT X = 0;X < 4;X++)
	{
		for(INT Y = 0;Y < 4;Y++)
		{
			if( Abs(M[X][Y] - Other.M[X][Y]) > Tolerance )
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

inline UBOOL FMatrix::operator!=(const FMatrix& Other) const
{
	return !(*this == Other);
}


// Homogeneous transform.

FORCEINLINE FVector4 FMatrix::TransformFVector4(const FVector4 &P) const
{
	FVector4 Result;

#if ASM_X86
	#ifdef _MSC_VER
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

		movss	xmm3,[eax]FVector4.W
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
		#error Please implement for your compiler.
	#endif

#else
	Result.X = P.X * M[0][0] + P.Y * M[1][0] + P.Z * M[2][0] + P.W * M[3][0];
	Result.Y = P.X * M[0][1] + P.Y * M[1][1] + P.Z * M[2][1] + P.W * M[3][1];
	Result.Z = P.X * M[0][2] + P.Y * M[1][2] + P.Z * M[2][2] + P.W * M[3][2];
	Result.W = P.X * M[0][3] + P.Y * M[1][3] + P.Z * M[2][3] + P.W * M[3][3];
#endif

	return Result;
}


// Regular transform.

/** Transform a location - will take into account translation part of the FMatrix. */
FORCEINLINE FVector4 FMatrix::TransformFVector(const FVector &V) const
{
	return TransformFVector4(FVector4(V.X,V.Y,V.Z,1.0f));
}

/** Inverts the matrix and then transforms V - correctly handles scaling in this matrix. */
FORCEINLINE FVector FMatrix::InverseTransformFVector(const FVector &V) const
{
	FMatrix InvSelf = this->Inverse();
	return InvSelf.TransformFVector(V);
}

/** Faster version of InverseTransformFVector that assumes no scaling. WARNING: Will NOT work correctly if there is scaling in the matrix. */
FORCEINLINE FVector FMatrix::InverseTransformFVectorNoScale(const FVector &V) const
{
	// Check no scaling in matrix
	checkSlow( Abs(1.f - Abs(RotDeterminant())) < 0.01f );

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

/** 
 *	Transform a direction vector - will not take into account translation part of the FMatrix. 
 *	If you want to transform a surface normal (or plane) and correctly account for non-uniform scaling you should use TransformByUsingAdjointT.
 */
FORCEINLINE FVector4 FMatrix::TransformNormal(const FVector& V) const
{
	return TransformFVector4(FVector4(V.X,V.Y,V.Z,0.0f));
}

/** Faster version of InverseTransformNormal that assumes no scaling. WARNING: Will NOT work correctly if there is scaling in the matrix. */
FORCEINLINE FVector FMatrix::InverseTransformNormal(const FVector &V) const
{
	FMatrix InvSelf = this->Inverse();
	return InvSelf.TransformNormal(V);
}

/** 
 *	Transform a direction vector by the inverse of this matrix - will not take into account translation part.
 *	If you want to transform a surface normal (or plane) and correctly account for non-uniform scaling you should use TransformByUsingAdjointT with adjoint of matrix inverse.
 */
FORCEINLINE FVector FMatrix::InverseTransformNormalNoScale(const FVector &V) const
{
	// Check no scaling in matrix
	checkSlow( Abs(1.f - Abs(RotDeterminant())) < 0.01f );

	return FVector( V.X * M[0][0] + V.Y * M[0][1] + V.Z * M[0][2],
					V.X * M[1][0] + V.Y * M[1][1] + V.Z * M[1][2],
					V.X * M[2][0] + V.Y * M[2][1] + V.Z * M[2][2] );
}


// Transpose.

FORCEINLINE FMatrix FMatrix::Transpose() const
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

inline FLOAT FMatrix::Determinant() const
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

/** Calculate determinant of rotation 3x3 matrix */
inline FLOAT FMatrix::RotDeterminant() const
{
	return	
		M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
		M[1][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1]) +
		M[2][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1]);
}

// Inverse.
/** Fast path, doesn't check for nil matrices in final release builds */
inline FMatrix FMatrix::Inverse() const
{
	// If we're in non final release, then make sure we're not creating NaNs
#if 0 && !FINAL_RELEASE
	// Check for zero scale matrix to invert
	if(	GetAxis(0).IsNearlyZero(SMALL_NUMBER) && 
		GetAxis(1).IsNearlyZero(SMALL_NUMBER) && 
		GetAxis(2).IsNearlyZero(SMALL_NUMBER) ) 
	{
		appErrorf(TEXT("FMatrix::Inverse(), trying to invert a NIL matrix, this results in NaNs! Use InverseSafe() instead."));
	}
#endif
	FMatrix Result;
	VectorMatrixInverse( &Result, this );
	return Result;
}

// Inverse.
inline FMatrix FMatrix::InverseSafe() const
{
	FMatrix Result;

	// Check for zero scale matrix to invert
	if(	GetAxis(0).IsNearlyZero(SMALL_NUMBER) && 
		GetAxis(1).IsNearlyZero(SMALL_NUMBER) && 
		GetAxis(2).IsNearlyZero(SMALL_NUMBER) ) 
	{
		// just set to zero - avoids unsafe inverse of zero and duplicates what QNANs were resulting in before (scaling away all children)
		Result = FMatrix(FVector(0.0f), FVector(0.0f), FVector(0.0f), FVector(0.0f));
	}
	else
	{
		VectorMatrixInverse( &Result, this );
	}
	return Result;
}

inline FMatrix FMatrix::InverseSlow() const
{
	FMatrix Result;
	const FLOAT	Det = Determinant();

	if(Det == 0.0f)
		return FMatrix::Identity;

	const FLOAT	RDet = 1.0f / Det;

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

inline FMatrix FMatrix::TransposeAdjoint() const
{
	FMatrix TA;

	TA.M[0][0] = this->M[1][1] * this->M[2][2] - this->M[1][2] * this->M[2][1];
	TA.M[0][1] = this->M[1][2] * this->M[2][0] - this->M[1][0] * this->M[2][2];
	TA.M[0][2] = this->M[1][0] * this->M[2][1] - this->M[1][1] * this->M[2][0];
	TA.M[0][3] = 0.f;

	TA.M[1][0] = this->M[2][1] * this->M[0][2] - this->M[2][2] * this->M[0][1];
	TA.M[1][1] = this->M[2][2] * this->M[0][0] - this->M[2][0] * this->M[0][2];
	TA.M[1][2] = this->M[2][0] * this->M[0][1] - this->M[2][1] * this->M[0][0];
	TA.M[1][3] = 0.f;

	TA.M[2][0] = this->M[0][1] * this->M[1][2] - this->M[0][2] * this->M[1][1];
	TA.M[2][1] = this->M[0][2] * this->M[1][0] - this->M[0][0] * this->M[1][2];
	TA.M[2][2] = this->M[0][0] * this->M[1][1] - this->M[0][1] * this->M[1][0];
	TA.M[2][3] = 0.f;

	TA.M[3][0] = 0.f;
	TA.M[3][1] = 0.f;
	TA.M[3][2] = 0.f;
	TA.M[3][3] = 1.f;

	return TA;
}

#if XBOX
// Remove any scaling from this matrix (ie magnitude of each row is 1)
FORCEINLINE void FMatrix::RemoveScaling(FLOAT Tolerance/*=SMALL_NUMBER*/)
#else 
// NOTE: There is some compiler optimization issues with WIN64 that cause FORCEINLINE to cause a crash
// Remove any scaling from this matrix (ie magnitude of each row is 1)
inline void FMatrix::RemoveScaling(FLOAT Tolerance/*=SMALL_NUMBER*/)
#endif
{
	// For each row, find magnitude, and if its non-zero re-scale so its unit length.
	const FLOAT SquareSum0 = (M[0][0] * M[0][0]) + (M[0][1] * M[0][1]) + (M[0][2] * M[0][2]);
	const FLOAT SquareSum1 = (M[1][0] * M[1][0]) + (M[1][1] * M[1][1]) + (M[1][2] * M[1][2]);
	const FLOAT SquareSum2 = (M[2][0] * M[2][0]) + (M[2][1] * M[2][1]) + (M[2][2] * M[2][2]);
	const FLOAT Scale0 = appFloatSelect( SquareSum0 - Tolerance, appInvSqrt(SquareSum0), 1.0f );
	const FLOAT Scale1 = appFloatSelect( SquareSum1 - Tolerance, appInvSqrt(SquareSum1), 1.0f );
	const FLOAT Scale2 = appFloatSelect( SquareSum2 - Tolerance, appInvSqrt(SquareSum2), 1.0f );
	M[0][0] *= Scale0; 
	M[0][1] *= Scale0; 
	M[0][2] *= Scale0; 
	M[1][0] *= Scale1; 
	M[1][1] *= Scale1; 
	M[1][2] *= Scale1; 
	M[2][0] *= Scale2; 
	M[2][1] *= Scale2; 
	M[2][2] *= Scale2;
}

// Returns matrix without scale information
inline FMatrix FMatrix::GetMatrixWithoutScale(FLOAT Tolerance/*=SMALL_NUMBER*/) const
{
	FMatrix Result = *this;
	Result.RemoveScaling(Tolerance);
	return Result;
}

/** Remove any scaling from this matrix (ie magnitude of each row is 1) and return the 3D scale vector that was initially present. */
inline FVector FMatrix::ExtractScaling(FLOAT Tolerance/*=SMALL_NUMBER*/)
{
	FVector Scale3D(0,0,0);

	// For each row, find magnitude, and if its non-zero re-scale so its unit length.
	const FLOAT SquareSum0 = (M[0][0] * M[0][0]) + (M[0][1] * M[0][1]) + (M[0][2] * M[0][2]);
	const FLOAT SquareSum1 = (M[1][0] * M[1][0]) + (M[1][1] * M[1][1]) + (M[1][2] * M[1][2]);
	const FLOAT SquareSum2 = (M[2][0] * M[2][0]) + (M[2][1] * M[2][1]) + (M[2][2] * M[2][2]);

	if( SquareSum0 > Tolerance )
	{
		FLOAT Scale0 = appSqrt(SquareSum0);
		Scale3D[0] =  Scale0;
		FLOAT InvScale0 = 1.f / Scale0;
		M[0][0] *= InvScale0; 
		M[0][1] *= InvScale0; 
		M[0][2] *= InvScale0; 
	}
	if( SquareSum1 > Tolerance )
	{
		FLOAT Scale1 = appSqrt(SquareSum1);
		Scale3D[1] =  Scale1;
		FLOAT InvScale1 = 1.f / Scale1;
		M[1][0] *= InvScale1; 
		M[1][1] *= InvScale1; 
		M[1][2] *= InvScale1; 
	}
	if( SquareSum2 > Tolerance )
	{
		FLOAT Scale2 = appSqrt(SquareSum2);
		Scale3D[2] =  Scale2;
		FLOAT InvScale2 = 1.f / Scale2;
		M[2][0] *= InvScale2; 
		M[2][1] *= InvScale2; 
		M[2][2] *= InvScale2; 
	}

	return Scale3D;
}

/** return a 3D scale vector calculated from this matrix (where each component is the magnitude of a row vector). */
inline FVector FMatrix::GetScaleVector(FLOAT Tolerance/*=SMALL_NUMBER*/) const
{
	FVector Scale3D(1,1,1);

	// For each row, find magnitude, and if its non-zero re-scale so its unit length.
	for(INT i=0; i<3; i++)
	{
		const FLOAT SquareSum = (M[i][0] * M[i][0]) + (M[i][1] * M[i][1]) + (M[i][2] * M[i][2]);
		if(SquareSum > Tolerance)
		{
			Scale3D[i] = appSqrt(SquareSum);
		}
	}

	return Scale3D;
}
// Remove any translation from this matrix
inline FMatrix FMatrix::RemoveTranslation() const
{
	FMatrix Result = *this;
	Result.M[3][0] = 0.0f;
	Result.M[3][1] = 0.0f;
	Result.M[3][2] = 0.0f;
	return Result;
}

FORCEINLINE FMatrix FMatrix::ConcatTranslation(const FVector& Translation) const
{
	FMatrix Result;

	FLOAT* RESTRICT Dest = &Result.M[0][0];
	const FLOAT* RESTRICT Src = &M[0][0];
	const FLOAT* RESTRICT Trans = &Translation.X;

	Dest[0] = Src[0];
	Dest[1] = Src[1];
	Dest[2] = Src[2];
	Dest[3] = Src[3];
	Dest[4] = Src[4];
	Dest[5] = Src[5];
	Dest[6] = Src[6];
	Dest[7] = Src[7];
	Dest[8] = Src[8];
	Dest[9] = Src[9];
	Dest[10] = Src[10];
	Dest[11] = Src[11];
	Dest[12] = Src[12] + Trans[0];
	Dest[13] = Src[13] + Trans[1];
	Dest[14] = Src[14] + Trans[2];
	Dest[15] = Src[15];

	return Result;
}

/** Returns TRUE if any element of this matrix is NaN */
inline UBOOL FMatrix::ContainsNaN() const
{
	for(INT i=0; i<4; i++)
	{
		for(INT j=0; j<4; j++)
		{
			if(appIsNaN(M[i][j]) || !appIsFinite(M[i][j]))
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/** @return the maximum magnitude of any row of the matrix. */
inline FLOAT FMatrix::GetMaximumAxisScale() const
{
	const FLOAT MaxRowScaleSquared = Max(
		GetAxis(0).SizeSquared(),
		Max(
			GetAxis(1).SizeSquared(),
			GetAxis(2).SizeSquared()
			)
		);
	return appSqrt(MaxRowScaleSquared);
}

inline void FMatrix::ScaleTranslation(const FVector& Scale3D)
{
	M[3][0] *= Scale3D.X;
	M[3][1] *= Scale3D.Y;
	M[3][2] *= Scale3D.Z;
}

// GetOrigin

inline FVector FMatrix::GetOrigin() const
{
	return FVector(M[3][0],M[3][1],M[3][2]);
}

inline FVector FMatrix::GetAxis(INT i) const
{
	checkSlow(i >= 0 && i <= 2);
	return FVector(M[i][0], M[i][1], M[i][2]);
}

inline void FMatrix::GetAxes(FVector &X, FVector &Y, FVector &Z) const
{
	X.X = M[0][0]; X.Y = M[0][1]; X.Z = M[0][2];
	Y.X = M[1][0]; Y.Y = M[1][1]; Y.Z = M[1][2];
	Z.X = M[2][0]; Z.Y = M[2][1]; Z.Z = M[2][2];
}

inline void FMatrix::SetAxis( INT i, const FVector& Axis )
{
	checkSlow(i >= 0 && i <= 2);
	M[i][0] = Axis.X;
	M[i][1] = Axis.Y;
	M[i][2] = Axis.Z;
}

inline void FMatrix::SetOrigin( const FVector& NewOrigin )
{
	M[3][0] = NewOrigin.X;
	M[3][1] = NewOrigin.Y;
	M[3][2] = NewOrigin.Z;
}

inline void FMatrix::SetAxes(FVector* Axis0 /*= NULL*/, FVector* Axis1 /*= NULL*/, FVector* Axis2 /*= NULL*/, FVector* Origin /*= NULL*/)
{
	if (Axis0 != NULL)
	{
		M[0][0] = Axis0->X;
		M[0][1] = Axis0->Y;
		M[0][2] = Axis0->Z;
	}
	if (Axis1 != NULL)
	{
		M[1][0] = Axis1->X;
		M[1][1] = Axis1->Y;
		M[1][2] = Axis1->Z;
	}
	if (Axis2 != NULL)
	{
		M[2][0] = Axis2->X;
		M[2][1] = Axis2->Y;
		M[2][2] = Axis2->Z;
	}
	if (Origin != NULL)
	{
		M[3][0] = Origin->X;
		M[3][1] = Origin->Y;
		M[3][2] = Origin->Z;
	}
}

inline FVector FMatrix::GetColumn(INT i) const
{
	checkSlow(i >= 0 && i <= 2);
	return FVector(M[0][i], M[1][i], M[2][i]);
}

// Frustum plane extraction.
FORCEINLINE UBOOL FMatrix::GetFrustumNearPlane(FPlane& OutPlane) const
{
	return MakeFrustumPlane(
		M[0][2],
		M[1][2],
		M[2][2],
		M[3][2],
		OutPlane
		);
}

FORCEINLINE UBOOL FMatrix::GetFrustumFarPlane(FPlane& OutPlane) const
{
	return MakeFrustumPlane(
		M[0][3] - M[0][2],
		M[1][3] - M[1][2],
		M[2][3] - M[2][2],
		M[3][3] - M[3][2],
		OutPlane
		);
}

FORCEINLINE UBOOL FMatrix::GetFrustumLeftPlane(FPlane& OutPlane) const
{
	return MakeFrustumPlane(
		M[0][3] + M[0][0],
		M[1][3] + M[1][0],
		M[2][3] + M[2][0],
		M[3][3] + M[3][0],
		OutPlane
		);
}

FORCEINLINE UBOOL FMatrix::GetFrustumRightPlane(FPlane& OutPlane) const
{
	return MakeFrustumPlane(
		M[0][3] - M[0][0],
		M[1][3] - M[1][0],
		M[2][3] - M[2][0],
		M[3][3] - M[3][0],
		OutPlane
		);
}

FORCEINLINE UBOOL FMatrix::GetFrustumTopPlane(FPlane& OutPlane) const
{
	return MakeFrustumPlane(
		M[0][3] - M[0][1],
		M[1][3] - M[1][1],
		M[2][3] - M[2][1],
		M[3][3] - M[3][1],
		OutPlane
		);
}

FORCEINLINE UBOOL FMatrix::GetFrustumBottomPlane(FPlane& OutPlane) const
{
	return MakeFrustumPlane(
		M[0][3] + M[0][1],
		M[1][3] + M[1][1],
		M[2][3] + M[2][1],
		M[3][3] + M[3][1],
		OutPlane
		);
}

/**
 * Utility for mirroring this transform across a certain plane,
 * and flipping one of the axis as well.
 */
inline void FMatrix::Mirror(BYTE MirrorAxis, BYTE FlipAxis)
{
	if(MirrorAxis == AXIS_X)
	{
		M[0][0] *= -1.f;
		M[1][0] *= -1.f;
		M[2][0] *= -1.f;

		M[3][0] *= -1.f;
	}
	else if(MirrorAxis == AXIS_Y)
	{
		M[0][1] *= -1.f;
		M[1][1] *= -1.f;
		M[2][1] *= -1.f;

		M[3][1] *= -1.f;
	}
	else if(MirrorAxis == AXIS_Z)
	{
		M[0][2] *= -1.f;
		M[1][2] *= -1.f;
		M[2][2] *= -1.f;

		M[3][2] *= -1.f;
	}

	if(FlipAxis == AXIS_X)
	{
		M[0][0] *= -1.f;
		M[0][1] *= -1.f;
		M[0][2] *= -1.f;
	}
	else if(FlipAxis == AXIS_Y)
	{
		M[1][0] *= -1.f;
		M[1][1] *= -1.f;
		M[1][2] *= -1.f;
	}
	else if(FlipAxis == AXIS_Z)
	{
		M[2][0] *= -1.f;
		M[2][1] *= -1.f;
		M[2][2] *= -1.f;
	}
}

// Serializer.
inline FArchive& operator<<(FArchive& Ar,FMatrix& M)
{
	Ar << M.M[0][0] << M.M[0][1] << M.M[0][2] << M.M[0][3];
	Ar << M.M[1][0] << M.M[1][1] << M.M[1][2] << M.M[1][3];
	Ar << M.M[2][0] << M.M[2][1] << M.M[2][2] << M.M[2][3];
	Ar << M.M[3][0] << M.M[3][1] << M.M[3][2] << M.M[3][3];
	return Ar;
}

