/*=============================================================================
	Color.h: Unreal color definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * A linear, 32-bit/component floating point RGBA color.
 */
struct FLinearColor
{
	FLOAT	R,
			G,
			B,
			A;

	/** Static lookup table used for FColor -> FLinearColor conversion. */
	static FLOAT PowOneOver255Table[256];

	FLinearColor() {}
	FLinearColor(FLOAT InR,FLOAT InG,FLOAT InB,FLOAT InA = 1.0f): R(InR), G(InG), B(InB), A(InA) {}
	FLinearColor(const FColor& C);
	FLinearColor(const class FVector& Vector);
	explicit FLinearColor(const class FFloat16Color& C);

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FLinearColor& Color)
	{
		return Ar << Color.R << Color.G << Color.B << Color.A;
	}

	// Conversions.
	FColor ToRGBE() const;

	// Operators.

	FLOAT& Component(INT Index)
	{
		return (&R)[Index];
	}

	const FLOAT& Component(INT Index) const
	{
		return (&R)[Index];
	}

	FLinearColor operator+(const FLinearColor& ColorB) const
	{
		return FLinearColor(
			this->R + ColorB.R,
			this->G + ColorB.G,
			this->B + ColorB.B,
			this->A + ColorB.A
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

	FLinearColor operator-(const FLinearColor& ColorB) const
	{
		return FLinearColor(
			this->R - ColorB.R,
			this->G - ColorB.G,
			this->B - ColorB.B,
			this->A - ColorB.A
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

	FLinearColor operator*(const FLinearColor& ColorB) const
	{
		return FLinearColor(
			this->R * ColorB.R,
			this->G * ColorB.G,
			this->B * ColorB.B,
			this->A * ColorB.A
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

	FLinearColor operator*(FLOAT Scalar) const
	{
		return FLinearColor(
			this->R * Scalar,
			this->G * Scalar,
			this->B * Scalar,
			this->A * Scalar
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

	FLinearColor operator/(const FLinearColor& ColorB) const
	{
		return FLinearColor(
			this->R / ColorB.R,
			this->G / ColorB.G,
			this->B / ColorB.B,
			this->A / ColorB.A
			);
	}
	FLinearColor& operator/=(const FLinearColor& ColorB)
	{
		R /= ColorB.R;
		G /= ColorB.G;
		B /= ColorB.B;
		A /= ColorB.A;
		return *this;
	}

	FLinearColor operator/(FLOAT Scalar) const
	{
		const FLOAT	InvScalar = 1.0f / Scalar;
		return FLinearColor(
			this->R * InvScalar,
			this->G * InvScalar,
			this->B * InvScalar,
			this->A * InvScalar
			);
	}
	FLinearColor& operator/=(FLOAT Scalar)
	{
		const FLOAT	InvScalar = 1.0f / Scalar;
		R *= InvScalar;
		G *= InvScalar;
		B *= InvScalar;
		A *= InvScalar;
		return *this;
	}

	/** Comparison operators */
	UBOOL operator==(const FLinearColor& ColorB) const
	{
		return this->R == ColorB.R && this->G == ColorB.G && this->B == ColorB.B && this->A == ColorB.A;
	}
	UBOOL operator!=(const FLinearColor& Other) const
	{
		return this->R != Other.R || this->G != Other.G || this->B != Other.B || this->A != Other.A;
	}

	// Error-tolerant comparison.
	UBOOL Equals(const FLinearColor& ColorB, FLOAT Tolerance=KINDA_SMALL_NUMBER) const
	{
		return Abs(this->R - ColorB.R) < Tolerance && Abs(this->G - ColorB.G) < Tolerance && Abs(this->B - ColorB.B) < Tolerance && Abs(this->A - ColorB.A) < Tolerance;
	}

	/**
	 * Converts byte hue-saturation-brightness to floating point red-green-blue.
	 */
	static FLinearColor FGetHSV(BYTE H,BYTE S,BYTE V);

	/** Converts a linear space RGB color to an HSV color */
	FLinearColor LinearRGBToHSV() const;

	/** Converts an HSV color to a linear space RGB color */
	FLinearColor HSVToLinearRGB() const;

	/** Quantizes the linear color and returns the result as a FColor.  This bypasses the SRGB conversion. */
	FColor Quantize() const;

	/** Quantizes the linear color and returns the result as a FColor with optional sRGB conversion and quality as goal. */
	FColor ToFColor(const UBOOL bSRGB) const;

	/**
	 * Returns a desaturated color, with 0 meaning no desaturation and 1 == full desaturation
	 *
	 * @param	Desaturation	Desaturation factor in range [0..1]
	 * @return	Desaturated color
	 */
	FLinearColor Desaturate( FLOAT Desaturation ) const;

	/** Computes the perceptually weighted luminance value of a color. */
	FLOAT ComputeLuminance() const;

	/**
	 * Returns the maximum value in this color structure
	 *
	 * @return	The maximum color channel value
	 */
	FLOAT GetMax() const
	{
		return Max( Max( Max( R, G ), B ), A );
	}


	/**
	 * Returns the minimum value in this color structure
	 *
	 * @return	The minimum color channel value
	 */
	FLOAT GetMin() const
	{
		return Min( Min( Min( R, G ), B ), A );
	}

	FLOAT GetLuminance() const 
	{ 
		return R * 0.3f + G * 0.59f + B * 0.11f; 
	}

	// Common colors.	
	static const FLinearColor White;
	static const FLinearColor Gray;
	static const FLinearColor Black;
};

inline FLinearColor operator*(FLOAT Scalar,const FLinearColor& Color)
{
	return Color.operator*( Scalar );
}

//
//	FColor
//

class FColor
{
public:
	// Variables.
#if __INTEL_BYTE_ORDER__
    #if _MSC_VER
		// Win32 x86
	    union { struct{ BYTE B,G,R,A; }; DWORD AlignmentDummy; };
    #else
		// Linux x86, etc
	    BYTE B GCC_ALIGN(4);
	    BYTE G,R,A;
    #endif
#else // __INTEL_BYTE_ORDER__
	union { struct{ BYTE A,R,G,B; }; DWORD AlignmentDummy; };
#endif

	DWORD& DWColor(void) {return *((DWORD*)this);}
	const DWORD& DWColor(void) const {return *((DWORD*)this);}

	// Constructors.
	FColor() {}
	FColor( BYTE InR, BYTE InG, BYTE InB, BYTE InA = 255 )
	{
		// put these into the body for proper ordering with INTEL vs non-INTEL_BYTE_ORDER
		R = InR;
		G = InG;
		B = InB;
		A = InA;

	}
	
	// fast, for more accuracy use FLinearColor::ToFColor()
	// TODO: doesn't handle negative colors well, implicit constructor can cause
	// accidental conversion (better use .ToFColor(TRUE))
	FColor(const FLinearColor& C)
		// put these into the body for proper ordering with INTEL vs non-INTEL_BYTE_ORDER
	{
		R = Clamp(appTrunc(appPow(C.R,1.0f / 2.2f) * 255.0f),0,255);
		G = Clamp(appTrunc(appPow(C.G,1.0f / 2.2f) * 255.0f),0,255);
		B = Clamp(appTrunc(appPow(C.B,1.0f / 2.2f) * 255.0f),0,255);
		A = Clamp(appTrunc(       C.A              * 255.0f),0,255);
	}

	explicit FColor( DWORD InColor )
	{ DWColor() = InColor; }

	// Serializer.
	friend FArchive& operator<< (FArchive &Ar, FColor &Color )
	{
		return Ar << Color.DWColor();
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
	void operator+=(const FColor& C)
	{
		R = (BYTE) Min((INT) R + (INT) C.R,255);
		G = (BYTE) Min((INT) G + (INT) C.G,255);
		B = (BYTE) Min((INT) B + (INT) C.B,255);
		A = (BYTE) Min((INT) A + (INT) C.A,255);
	}
	FLinearColor FromRGBE() const;

	/**
	 * Makes a random but quite nice color.
	 */
	static FColor MakeRandomColor();

	/**
	 * Makes a color red->green with the passed in scalar (e.g. 0 is red, 1 is green)
	 */
	static FColor MakeRedToGreenColorFromScalar(FLOAT Scalar);

	/** Reinterprets the color as a linear color. */
	FLinearColor ReinterpretAsLinear() const
	{
		return FLinearColor(R/255.f,G/255.f,B/255.f,A/255.f);
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("(R=%i,G=%i,B=%i,A=%i)"),R,G,B,A);
	}

	/**
	 * Initialize this Color based on an FString. The String is expected to contain R=, G=, B=, A=.
	 * The FColor will be bogus when InitFromString returns FALSE.
	 *
	 * @param	InSourceString	FString containing the color values.
	 *
	 * @return TRUE if the R,G,B values were read successfully; FALSE otherwise.
	 */
	UBOOL InitFromString( const FString & InSourceString )
	{
		R = G = B = 0;
		A = 255;

		// The initialization is only successful if the R, G, and B values can all be parsed from the string
		const UBOOL bSuccessful = Parse( *InSourceString, TEXT("R=") , R ) && Parse( *InSourceString, TEXT("G="), G ) && Parse( *InSourceString, TEXT("B="), B );
		
		// Alpha is optional, so don't factor in its presence (or lack thereof) in determining initialization success
		Parse( *InSourceString, TEXT("A="), A );
		
		return bSuccessful;
	}
};

inline DWORD GetTypeHash( const FColor& Color )
{
	return Color.DWColor();
}

/** Computes a brightness and a fixed point color from a floating point color. */
extern void ComputeAndFixedColorAndIntensity(const FLinearColor& InLinearColor,FColor& OutColor,FLOAT& OutIntensity);

// These act like a POD
template <> struct TIsPODType<FColor> { enum { Value = true }; };
template <> struct TIsPODType<FLinearColor> { enum { Value = true }; };


/**
 * Helper struct for a 16 bit 565 color of a DXT1/3/5 block.
 */
struct FDXTColor565
{
	/** Blue component, 5 bit. */
	WORD B:5;
	/** Green component, 6 bit. */
	WORD G:6;
	/** Red component, 5 bit */
	WORD R:5;
};

/**
 * Helper struct for a 16 bit 565 color of a DXT1/3/5 block.
 */
struct FDXTColor16
{
	union 
	{
		/** 565 Color */
		FDXTColor565 Color565;
		/** 16 bit entity representation for easy access. */
		WORD Value;
	};
};

/**
 * Structure encompassing single DXT1 block.
 */
struct FDXT1
{
	/** Color 0/1 */
	union
	{
		FDXTColor16 Color[2];
		DWORD Colors;
	};
	/** Indices controlling how to blend colors. */
	DWORD Indices;
};

/**
 * Structure encompassing single DXT5 block
 */
struct FDXT5
{
	/** Alpha component of DXT5 */
	BYTE	Alpha[8];
	/** DXT1 color component. */
	FDXT1	DXT1;
};

// Make DXT helpers act like PODs
template <> struct TIsPODType<FDXT1> { enum { Value = true }; };
template <> struct TIsPODType<FDXT5> { enum { Value = true }; };
template <> struct TIsPODType<FDXTColor16> { enum { Value = true }; };
template <> struct TIsPODType<FDXTColor565> { enum { Value = true }; };

