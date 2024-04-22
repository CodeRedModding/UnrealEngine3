/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

//	Constants.

#define MAX_SH_ORDER	3
#define MAX_SH_BASIS	(MAX_SH_ORDER*MAX_SH_ORDER)

/** A vector of spherical harmonic coefficients. */
MS_ALIGN(16) class FSHVector
{
public:

	enum { NumComponentsPerSIMDVector = 4 };
	enum { NumSIMDVectors = (MAX_SH_BASIS + NumComponentsPerSIMDVector - 1) / NumComponentsPerSIMDVector };

	FLOAT V[NumSIMDVectors * NumComponentsPerSIMDVector];

	/** The integral of the constant SH basis. */
	static const FLOAT ConstantBasisIntegral;

	/** Default constructor. */
	FSHVector()
	{
		appMemzero(V,sizeof(V));
	}

	explicit FSHVector(const class FQuantizedSHVector& Quantized);

	/** Initialization constructor. */
	FSHVector(
		FLOAT InV0,
		FLOAT InV1,
		FLOAT InV2,
		FLOAT InV3,
		FLOAT InV4,
		FLOAT InV5,
		FLOAT InV6,
		FLOAT InV7,
		FLOAT InV8
		)
	{
		appMemzero(V,sizeof(V));
		V[0] = InV0;
		V[1] = InV1;
		V[2] = InV2;
		V[3] = InV3;
		V[4] = InV4;
		V[5] = InV5;
		V[6] = InV6;
		V[7] = InV7;
		V[8] = InV8;
	}

	/** Scalar multiplication operator. */
	/** Changed to FLOAT& from FLOAT to avoid LHS **/
	friend FORCEINLINE FSHVector operator*(const FSHVector& A,const FLOAT& B)
	{
		const VectorRegister ReplicatedScalar = VectorLoadFloat1(&B);

		FSHVector Result;
		for(INT BasisIndex = 0;BasisIndex < NumSIMDVectors;BasisIndex++)
		{
			VectorRegister MulResult = VectorMultiply(
				VectorLoadAligned(&A.V[BasisIndex * NumComponentsPerSIMDVector]),
				ReplicatedScalar
				);
			VectorStoreAligned(MulResult, &Result.V[BasisIndex * NumComponentsPerSIMDVector]);
		}
		return Result;
	}

	/** Addition operator. */
	friend FORCEINLINE FSHVector operator+(const FSHVector& A,const FSHVector& B)
	{
		FSHVector Result;
		for(INT BasisIndex = 0;BasisIndex < NumSIMDVectors;BasisIndex++)
		{
			VectorRegister AddResult = VectorAdd(
				VectorLoadAligned(&A.V[BasisIndex * NumComponentsPerSIMDVector]),
				VectorLoadAligned(&B.V[BasisIndex * NumComponentsPerSIMDVector])
				);

			VectorStoreAligned(AddResult, &Result.V[BasisIndex * NumComponentsPerSIMDVector]);
		}
		return Result;
	}
	
	/** Subtraction operator. */
	friend FORCEINLINE FSHVector operator-(const FSHVector& A,const FSHVector& B)
	{
		FSHVector Result;
		for(INT BasisIndex = 0;BasisIndex < NumSIMDVectors;BasisIndex++)
		{
			VectorRegister SubResult = VectorSubtract(
				VectorLoadAligned(&A.V[BasisIndex * NumComponentsPerSIMDVector]),
				VectorLoadAligned(&B.V[BasisIndex * NumComponentsPerSIMDVector])
				);

			VectorStoreAligned(SubResult, &Result.V[BasisIndex * NumComponentsPerSIMDVector]);
		}
		return Result;
	}

	/** Dot product operator. */
	friend FORCEINLINE FLOAT Dot(const FSHVector& A,const FSHVector& B)
	{
		VectorRegister ReplicatedResult = VectorZero();
		for(INT BasisIndex = 0;BasisIndex < NumSIMDVectors;BasisIndex++)
		{
			ReplicatedResult = VectorAdd(
				ReplicatedResult,
				VectorDot4(
					VectorLoadAligned(&A.V[BasisIndex * NumComponentsPerSIMDVector]),
					VectorLoadAligned(&B.V[BasisIndex * NumComponentsPerSIMDVector])
					)
				);
		}
		FLOAT Result;
		VectorStoreFloat1(ReplicatedResult,&Result);
		return Result;
	}

	/** In-place addition operator. */
	/** Changed from (*this = *this + B;} to calculate here to avoid LHS **/
	/** Now this avoids FSHVector + operator thus LHS on *this as well as Result and more **/
	FORCEINLINE FSHVector& operator+=(const FSHVector& B)
	{
		for(INT BasisIndex = 0;BasisIndex < NumSIMDVectors;BasisIndex++)
		{
			VectorRegister AddResult = VectorAdd(
				VectorLoadAligned(&V[BasisIndex * NumComponentsPerSIMDVector]),
				VectorLoadAligned(&B.V[BasisIndex * NumComponentsPerSIMDVector])
				);

			VectorStoreAligned(AddResult, &V[BasisIndex * NumComponentsPerSIMDVector]);
		}
		return *this;
	}
	
	/** In-place subtraction operator. */
	/** Changed from (*this = *this - B;} to calculate here to avoid LHS **/
	/** Now this avoids FSHVector - operator thus LHS on *this as well as Result and **/
	FORCEINLINE FSHVector& operator-=(const FSHVector& B)
	{
		for(INT BasisIndex = 0;BasisIndex < NumSIMDVectors;BasisIndex++)
		{
			VectorRegister SubResult = VectorSubtract(
				VectorLoadAligned(&V[BasisIndex * NumComponentsPerSIMDVector]),
				VectorLoadAligned(&B.V[BasisIndex * NumComponentsPerSIMDVector])
				);

			VectorStoreAligned(SubResult, &V[BasisIndex * NumComponentsPerSIMDVector]);
		}
		return *this;
	}

	/** In-place scalar division operator. */
	/** Changed to FLOAT& from FLOAT to avoid LHS **/
	/** Changed from (*this = *this * (1.0f/B);) to calculate here to avoid LHS **/
	/** Now this avoids FSHVector * operator thus LHS on *this as well as Result and LHS **/
	FORCEINLINE FSHVector& operator/=(const FLOAT& Scalar)
	{
		// Talk to Smedis - so make this to / function 
		const FLOAT B = (1.0f/Scalar);
		const VectorRegister ReplicatedScalar = VectorLoadFloat1(&B);

		for(INT BasisIndex = 0;BasisIndex < NumSIMDVectors;BasisIndex++)
		{
			VectorRegister MulResult = VectorMultiply(
				VectorLoadAligned(&V[BasisIndex * NumComponentsPerSIMDVector]),
				ReplicatedScalar
				);
			VectorStoreAligned(MulResult, &V[BasisIndex * NumComponentsPerSIMDVector]);
		}
		return *this;
	}

	/** In-place scalar multiplication operator. */
	/** Changed to FLOAT& from FLOAT to avoid LHS **/
	/** Changed from (*this = *this * B;) to calculate here to avoid LHS **/
	/** Now this avoids FSHVector * operator thus LHS on *this as well as Result and LHS **/
	FORCEINLINE FSHVector& operator*=(const FLOAT& B)
	{
		const VectorRegister ReplicatedScalar = VectorLoadFloat1(&B);

		for(INT BasisIndex = 0;BasisIndex < NumSIMDVectors;BasisIndex++)
		{
			VectorRegister MulResult = VectorMultiply(
				VectorLoadAligned(&V[BasisIndex * NumComponentsPerSIMDVector]),
				ReplicatedScalar
				);
			VectorStoreAligned(MulResult, &V[BasisIndex * NumComponentsPerSIMDVector]);
		}
		return *this;
	}

	/** Calculates the integral of the function over the surface of the sphere. */
	FLOAT CalcIntegral() const
	{
		return V[0] * ConstantBasisIntegral;
	}

	/** Scales the function uniformly so its integral equals one. */
	void Normalize()
	{
		const FLOAT Integral = CalcIntegral();
		if(Integral > DELTA)
		{
			*this /= Integral;
		}
	}

	/** The upper hemisphere of the sky light inceident lighting function. */
	static FSHVector UpperSkyFunction();

	/** The lower hemisphere of the sky light incident lighting function. */
	static FSHVector LowerSkyFunction();

	/** The ambient incident lighting function. */
	static FSHVector AmbientFunction();
} GCC_ALIGN(16);

/** A vector of colored spherical harmonic coefficients. */
class FSHVectorRGB
{
public:

	FSHVector R;
	FSHVector G;
	FSHVector B;

	FSHVectorRGB() {}
	explicit FSHVectorRGB(const class FQuantizedSHVectorRGB& Quantized);

	/** Calculates greyscale spherical harmonic coefficients. */
	FSHVector GetLuminance() const
	{
		return R * 0.3f + G * 0.59f + B * 0.11f;
	}

	/** Calculates the integral of the function over the surface of the sphere. */
	FLinearColor CalcIntegral() const
	{
		FLinearColor Result;
		Result.R = R.CalcIntegral();
		Result.G = G.CalcIntegral();
		Result.B = B.CalcIntegral();
		Result.A = 1.0f;
		return Result;
	}

	/** Scalar multiplication operator. */
	/** Changed to FLOAT& from FLOAT to avoid LHS **/
	friend FORCEINLINE FSHVectorRGB operator*(const FSHVectorRGB& A, const FLOAT& Scalar)
	{
		FSHVectorRGB Result;
		Result.R = A.R * Scalar;
		Result.G = A.G * Scalar;
		Result.B = A.B * Scalar;
		return Result;
	}

	/** Scalar multiplication operator. */
	/** Changed to FLOAT& from FLOAT to avoid LHS **/
	friend FORCEINLINE FSHVectorRGB operator*(const FLOAT& Scalar,const FSHVectorRGB& A)
	{
		FSHVectorRGB Result;
		Result.R = A.R * Scalar;
		Result.G = A.G * Scalar;
		Result.B = A.B * Scalar;
		return Result;
	}

	/** Color multiplication operator. */
	friend FORCEINLINE FSHVectorRGB operator*(const FSHVectorRGB& A,const FLinearColor& Color)
	{
		FSHVectorRGB Result;
		Result.R = A.R * Color.R;
		Result.G = A.G * Color.G;
		Result.B = A.B * Color.B;
		return Result;
	}

	/** Color multiplication operator. */
	friend FORCEINLINE FSHVectorRGB operator*(const FLinearColor& Color,const FSHVectorRGB& A)
	{
		FSHVectorRGB Result;
		Result.R = A.R * Color.R;
		Result.G = A.G * Color.G;
		Result.B = A.B * Color.B;
		return Result;
	}

	/** Addition operator. */
	friend FORCEINLINE FSHVectorRGB operator+(const FSHVectorRGB& A,const FSHVectorRGB& B)
	{
		FSHVectorRGB Result;
		Result.R = A.R + B.R;
		Result.G = A.G + B.G;
		Result.B = A.B + B.B;
		return Result;
	}
	
	/** Subtraction operator. */
	friend FORCEINLINE FSHVectorRGB operator-(const FSHVectorRGB& A,const FSHVectorRGB& B)
	{
		FSHVectorRGB Result;
		Result.R = A.R - B.R;
		Result.G = A.G - B.G;
		Result.B = A.B - B.B;
		return Result;
	}

	/** Dot product operator. */
	friend FORCEINLINE FLinearColor Dot(const FSHVectorRGB& A,const FSHVector& B)
	{
		FLinearColor Result;
		Result.R = Dot(A.R,B);
		Result.G = Dot(A.G,B);
		Result.B = Dot(A.B,B);
		Result.A = 1.0f;
		return Result;
	}

	/** In-place addition operator. */
	/** Changed from (*this = *this + InB;) to separate all calc to avoid LHS **/

	/** Now it calls directly += operator in FSHVector (avoid FSHVectorRGB + operator) **/
	FORCEINLINE FSHVectorRGB& operator+=(const FSHVectorRGB& InB)
	{
		R += InB.R;
		G += InB.G;
		B += InB.B;

		return *this;
	}
	
	/** In-place subtraction operator. */
	/** Changed from (*this = *this - InB;) to separate all calc to avoid LHS **/
	/** Now it calls directly -= operator in FSHVector (avoid FSHVectorRGB - operator) **/
	FORCEINLINE FSHVectorRGB& operator-=(const FSHVectorRGB& InB)
	{
		R -= InB.R;
		G -= InB.G;
		B -= InB.B;

		return *this;
	}

	/** In-place scalar multiplication operator. */
	/** Changed from (*this = *this * InB;) to separate all calc to avoid LHS **/
	/** Now it calls directly *= operator in FSHVector (avoid FSHVectorRGB * operator) **/
	FORCEINLINE FSHVectorRGB& operator*=(const FLOAT& Scalar)
	{
		R *= Scalar;
		G *= Scalar;
		B *= Scalar;

		return *this;
	}

	/** Adds an impulse to the SH environment. */
	inline void AddIncomingRadiance(const FLinearColor& IncomingRadiance, FLOAT Weight, const FVector4& WorldSpaceDirection);

	/** Adds ambient lighting. */
	inline void AddAmbient(const FLinearColor& Intensity);
};

/** Color multiplication operator. */
FORCEINLINE FSHVectorRGB operator*(const FSHVector& A,const FLinearColor& B)
{
	FSHVectorRGB Result;
	Result.R = A * B.R;
	Result.G = A * B.G;
	Result.B = A * B.B;

	return Result;
}

/** Returns the value of the SH basis L,M at the point on the sphere defined by the unit vector Vector. */
extern FSHVector SHBasisFunction(const FVector& Vector);

inline void FSHVectorRGB::AddIncomingRadiance(const FLinearColor& IncomingRadiance, FLOAT Weight, const FVector4& WorldSpaceDirection)
{
	*this += SHBasisFunction(WorldSpaceDirection) * (IncomingRadiance * Weight);
}

inline void FSHVectorRGB::AddAmbient(const FLinearColor& Intensity)
{
	*this += FSHVector::AmbientFunction() * Intensity;
}

/** A vector of quantized spherical harmonic coefficients. */
class FQuantizedSHVector
{
public:
	FFloat16 MinCoefficient;
	FFloat16 MaxCoefficient;
	BYTE V[MAX_SH_BASIS];

	friend FArchive& operator<<(FArchive& Ar, FQuantizedSHVector& Vector)
	{
		Ar << Vector.MinCoefficient;
		Ar << Vector.MaxCoefficient;
		Ar.Serialize(Vector.V, sizeof(Vector.V));
		return Ar;
	}
};

/** A vector of colored, quantized spherical harmonic coefficients. */
class FQuantizedSHVectorRGB
{
public:

	FQuantizedSHVector R;
	FQuantizedSHVector G;
	FQuantizedSHVector B;

	friend FArchive& operator<<(FArchive& Ar, FQuantizedSHVectorRGB& Vector)
	{
		Ar << Vector.R;
		Ar << Vector.G;
		Ar << Vector.B;
		return Ar;
	}
};

/** Returns the basis index of the SH basis L,M. */
FORCEINLINE INT SHGetBasisIndex(INT L,INT M)
{
	return L * (L + 1) + M;
}
