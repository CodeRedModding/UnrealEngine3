/*=============================================================================
	StaticBitArray.h: Static bit array definition
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __STATICBITARRAY_H__
#define __STATICBITARRAY_H__

/** Used to read/write a bit in the static array as a UBOOL. */
template<typename T>
class TStaticBitReference
{
public:

	TStaticBitReference(T& InData,T InMask)
	:	Data(InData)
	,	Mask(InMask)
	{}

	FORCEINLINE operator UBOOL() const
	{
		 return (Data & Mask) != 0;
	}
	FORCEINLINE void operator=(const UBOOL NewValue)
	{
		if(NewValue)
		{
			Data |= Mask;
		}
		else
		{
			Data &= ~Mask;
		}
	}

private:
	T& Data;
	T Mask;
};

/** Used to read a bit in the static array as a UBOOL. */
template<typename T>
class TConstStaticBitReference
{
public:

	TConstStaticBitReference(const T& InData,T InMask)
	:	Data(InData)
	,	Mask(InMask)
	{}

	FORCEINLINE operator UBOOL() const
	{
		 return (Data & Mask) != 0;
	}

private:
	const T& Data;
	T Mask;
};


/**
 * A statically sized bit array 
 */
template<UINT NumBits>
class TStaticBitArray
{
	typedef QWORD WordType;

	struct FBoolType;
	typedef INT* FBoolType::* UnspecifiedBoolType;
	typedef FLOAT* FBoolType::* UnspecifiedZeroType;
public:

	/**
	 * Minimal initialization constructor
	 */
	FORCEINLINE TStaticBitArray()
	{
		Clear_();
	}

	/**
	 * Constructor that allows initializing by assignment from 0
	 */
	FORCEINLINE TStaticBitArray(UnspecifiedZeroType)
	{
		Clear_();
	}

	/**
	 * Non-initializing constructor
	 */
	FORCEINLINE explicit TStaticBitArray(ENoInit)
	{
	}

	/**
	 * Constructor to initialize to a single bit
	 */
	FORCEINLINE explicit TStaticBitArray(EForceInit Dummy, UINT InBitIndex)
	{
		/***********************************************************************

		 If this line fails to compile you are attempting to construct a bit
		 array with an out-of bounds bit index. Follow the compiler errors to
		 the initialization point

		***********************************************************************/
//		checkAtCompileTime( InBitIndex >= 0 && InBitIndex < NumBits, invalidBitValue );

		checkSlowish((NumBits > 0) ? (InBitIndex<NumBits):1);

		UINT DestWordIndex = InBitIndex / NumBitsPerWord;
		WordType Word = (WordType)1 << (InBitIndex & (NumBitsPerWord - 1));

		for(INT WordIndex = 0; WordIndex < NumWords; ++WordIndex)
		{
			Words[WordIndex] = WordIndex == DestWordIndex ? Word : (WordType)0;
		}
	}

	/**
	 * Constructor to initialize from string
	 */
	explicit TStaticBitArray(const FString& Str)
	{
		INT Length = Str.Len();

		// Trim count to length of bit array
		if(NumBits < Length)
		{
			Length = NumBits;
		}
		Clear_();

		INT Pos = Length;
		for(INT Index = 0; Index < Length; ++Index)
		{
			const TCHAR ch = Str[--Pos];
			if(ch == TEXT('1'))
			{
				operator()(Index) = TRUE;
			}
			else if(ch != TEXT('0'))
			{
				ErrorInvalid_();
			}
		}
	}

	// Conversion to bool
	FORCEINLINE operator UnspecifiedBoolType() const
	{
		WordType And = 0;
		for(INT Index = 0; Index < NumWords; ++Index)
		{
			And |= Words[Index];
		}

		return And ? &FBoolType::Valid : NULL;
	}

	// Accessors.
	FORCEINLINE static INT Num() { return NumBits; }
	FORCEINLINE TStaticBitReference<WordType> operator()(INT Index)
	{
		checkSlowish(Index>=0 && Index<NumBits);
		return TStaticBitReference<WordType>(
			Words[Index / NumBitsPerWord],
			(WordType)1 << (Index & (NumBitsPerWord - 1))
			);
	}
	FORCEINLINE const TConstStaticBitReference<WordType> operator()(INT Index) const
	{
		checkSlowish(Index>=0 && Index<NumBits);
		return TConstStaticBitReference<WordType>(
			Words[Index / NumBitsPerWord],
			(WordType)1 << (Index & (NumBitsPerWord - 1))
			);
	}

	// Modifiers.
	FORCEINLINE TStaticBitArray& operator|=(const TStaticBitArray& Other)
	{
		for(INT Index = 0; Index < NumWords; ++Index)
		{
			Words[Index] |= Other.Words[Index];
		}
		return *this;
	}
	FORCEINLINE TStaticBitArray& operator&=(const TStaticBitArray& Other)
	{
		for(INT Index = 0; Index < NumWords; ++Index)
		{
			Words[Index] &= Other.Words[Index];
		}
		return *this;
	}
	FORCEINLINE TStaticBitArray& operator^=(const TStaticBitArray& Other )
	{
		for(INT Index = 0; Index < NumWords; ++Index)
		{
			Words[Index] ^= Other.Words[Index];
		}
		return *this;
	}
	friend FORCEINLINE TStaticBitArray<NumBits> operator~(const TStaticBitArray<NumBits>& A)
	{
		TStaticBitArray Result(E_NoInit);
		for(INT Index = 0; Index < NumWords; ++Index)
		{
			Result.Words[Index] = ~A.Words[Index];
		}
		Result.Trim_();
		return Result;
	}
	friend FORCEINLINE TStaticBitArray<NumBits> operator|(const TStaticBitArray<NumBits>& A, const TStaticBitArray<NumBits>& B)
	{
		// is not calling |= because doing it in here has less LoadHitStores and is therefore faster.
		TStaticBitArray Results(E_NoInit);

		const WordType* RESTRICT APtr = (const WordType* RESTRICT)A.Words;
		const WordType* RESTRICT BPtr = (const WordType* RESTRICT)B.Words;
		WordType* RESTRICT ResultsPtr = (WordType* RESTRICT)Results.Words;
		for(INT Index = 0; Index < NumWords; ++Index)
		{
			ResultsPtr[Index] = APtr[Index] | BPtr[Index];
		}

		return Results;
	}
	friend FORCEINLINE TStaticBitArray<NumBits> operator&(const TStaticBitArray<NumBits>& A, const TStaticBitArray<NumBits>& B)
	{
		// is not calling &= because doing it in here has less LoadHitStores and is therefore faster.
		TStaticBitArray Results(E_NoInit);

		const WordType* RESTRICT APtr = (const WordType* RESTRICT)A.Words;
		const WordType* RESTRICT BPtr = (const WordType* RESTRICT)B.Words;
		WordType* RESTRICT ResultsPtr = (WordType* RESTRICT)Results.Words;
		for(INT Index = 0; Index < NumWords; ++Index)
		{
			ResultsPtr[Index] = APtr[Index] & BPtr[Index];
		}

		return Results;
	}
	friend FORCEINLINE TStaticBitArray<NumBits> operator^(const TStaticBitArray<NumBits>& A, const TStaticBitArray<NumBits>& B)
	{
		TStaticBitArray Results(A);
		Results ^= B;
		return Results;
	}
	friend FORCEINLINE bool operator==(const TStaticBitArray<NumBits>& A, const TStaticBitArray<NumBits>& B)
	{
		for(INT Index = 0; Index < A.NumWords; ++Index)
		{
			if(A.Words[Index] != B.Words[Index])
			{
				return FALSE;
			}
		}
		return TRUE;
	}
	/** This operator only exists to disambiguate == in statements of the form (flags == 0) */
	friend FORCEINLINE bool operator==(const TStaticBitArray<NumBits>& A, UnspecifiedBoolType Value)
	{
		return (UnspecifiedBoolType)A == Value;
	}
	/** != simple maps to == */
	friend FORCEINLINE bool operator!=(const TStaticBitArray<NumBits>& A, const TStaticBitArray<NumBits>& B)
	{
		return !(A == B);
	}
	/** != simple maps to == */
	friend FORCEINLINE bool operator!=(const TStaticBitArray<NumBits>& A, UnspecifiedBoolType Value)
	{
		return !(A == Value);
	}

	/**
	 * Serializer.
	 */
	friend FArchive& operator<<(FArchive& Ar, TStaticBitArray& BitArray)
	{
		UINT ArchivedNumWords = BitArray.NumWords;
		Ar << ArchivedNumWords;

		if(Ar.IsLoading())
		{
			appMemset(BitArray.Words, 0, sizeof(BitArray.Words));
			ArchivedNumWords = Min(BitArray.NumWords, ArchivedNumWords);
		}

		Ar.Serialize(BitArray.Words, ArchivedNumWords * sizeof(BitArray.Words[0]));
		return Ar;
	}

	/**
	 * Converts the bitarray to a string representing the binary representation of the array
	 */
	FString ToString() const
	{
		FString Str;
		Str.Empty(NumBits);

		for(INT Index = NumBits - 1; Index >= 0; --Index)
		{
			Str += operator()(Index) ? TEXT('1') : TEXT('0');
		}

		return Str;
	}

	static const UINT NumOfBits = NumBits;
private:
//	checkAtCompileTime( NumBits > 0, mustHaveAtLeast1Bit );
	static const UINT NumBitsPerWord = sizeof(WordType) * 8;
	static const UINT NumWords = ((NumBits + NumBitsPerWord - 1) & ~(NumBitsPerWord - 1)) / NumBitsPerWord;
	WordType Words[NumWords];

	// Helper class for bool conversion
	struct FBoolType
	{
		INT* Valid;
	};

	/**
	 * Resets the bit array to a 0 value
	 */
	FORCEINLINE void Clear_()
	{
		for(INT Index = 0; Index < NumWords; ++Index)
		{
			Words[Index] = 0;
		}
	}

	/**
	 * Clears any trailing bits in the last word
	 */
	void Trim_()
	{
		if(NumBits % NumBitsPerWord != 0)
		{
			Words[NumWords-1] &= (WordType(1) << (NumBits % NumBitsPerWord)) - 1;
		}
	}

	/**
	 * Reports an invalid string element in the bitset conversion
	 */
	void ErrorInvalid_() const
	{
		appErrorf(TEXT("invalid TStaticBitArray<NumBits> character"));
	}
};

#endif
