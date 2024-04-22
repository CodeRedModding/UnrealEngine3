/*=============================================================================
	BitArray.h: Bit array definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __BITARRAY_H__
#define __BITARRAY_H__

// Forward declaration.
template<typename Allocator = FDefaultBitArrayAllocator>
class TBitArray;

template<typename Allocator = FDefaultBitArrayAllocator>
class TConstSetBitIterator;

template<typename Allocator = FDefaultBitArrayAllocator,typename OtherAllocator = FDefaultBitArrayAllocator>
class TConstDualSetBitIterator;

/**
 * Serializer (predefined for no friend injection in gcc 411)
 */
template<typename Allocator>
FArchive& operator<<(FArchive& Ar, TBitArray<Allocator>& BitArray);

/** Used to read/write a bit in the array as a UBOOL. */
class FBitReference
{
public:

	FBitReference(DWORD& InData,DWORD InMask)
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
	DWORD& Data;
	DWORD Mask;
};

/** Used to read a bit in the array as a UBOOL. */
class FConstBitReference
{
public:

	FConstBitReference(const DWORD& InData,DWORD InMask)
	:	Data(InData)
	,	Mask(InMask)
	{}

	FORCEINLINE operator UBOOL() const
	{
		 return (Data & Mask) != 0;
	}

private:
	const DWORD& Data;
	DWORD Mask;
};

/** Used to reference a bit in an unspecified bit array. */
class FRelativeBitReference
{
	template<typename>
	friend class TBitArray;
	template<typename>
	friend class TConstSetBitIterator;
	template<typename,typename>
	friend class TConstDualSetBitIterator;
public:

	FRelativeBitReference(INT BitIndex)
	:	DWORDIndex(BitIndex >> NumBitsPerDWORDLogTwo)
	,	Mask(1 << (BitIndex & (NumBitsPerDWORD - 1)))
	{}

protected:
	INT DWORDIndex;
	DWORD Mask;
};

/**
 * A dynamically sized bit array.
 * An array of Booleans.  They stored in one bit/Boolean.  There are iterators that efficiently iterate over only set bits.
 */
template<typename Allocator /*= FDefaultBitArrayAllocator*/>
class TBitArray : protected Allocator::template ForElementType<DWORD>
{
public:

	template<typename>
	friend class TConstSetBitIterator;

	template<typename,typename>
	friend class TConstDualSetBitIterator;

	/**
	 * Minimal initialization constructor.
	 * @param Value - The value to initial the bits to.
	 * @param InNumBits - The initial number of bits in the array.
	 */
	explicit TBitArray( const UBOOL Value = FALSE, const INT InNumBits = 0 )
	:	NumBits(0)
	,	MaxBits(0)
	{
		Init(Value,InNumBits);
	}

	/**
	 * Copy constructor.
	 */
	TBitArray(const TBitArray& Copy)
	:	NumBits(0)
	,	MaxBits(0)
	{
		*this = Copy;
	}

	/**
	 * Assignment operator.
	 */
	TBitArray& operator=(const TBitArray& Copy)
	{
		// check for self assignment since we don't use swamp() mechanic
		if( this == &Copy )
		{
			return *this;
		}

		Empty(Copy.Num());
		NumBits = MaxBits = Copy.NumBits;
		if(NumBits)
		{
			const INT NumDWORDs = (MaxBits + NumBitsPerDWORD - 1) / NumBitsPerDWORD;
			Realloc(0);
			appMemcpy(GetData(),Copy.GetData(),NumDWORDs * sizeof(DWORD));
		}
		return *this;
	}

	/**
	 * Serializer
	 */
	friend FArchive& operator<<(FArchive& Ar, TBitArray& BitArray)
	{
		// serialize number of bits
		Ar << BitArray.NumBits;

		if (Ar.IsLoading())
		{
			// no need for slop when reading
			BitArray.MaxBits = BitArray.NumBits;

			// allocate room for new bits
			BitArray.Realloc(0);
		}

		// calc the number of dwords for all the bits
		const INT NumDWORDs = (BitArray.NumBits + NumBitsPerDWORD - 1) / NumBitsPerDWORD; 

		// serialize the data as one big chunk
		Ar.Serialize(BitArray.GetData(), NumDWORDs * sizeof(DWORD));

		return Ar;
	}

	/**
	 * Adds a bit to the array with the given value.
	 * @return The index of the added bit.
	 */
	INT AddItem(const UBOOL Value)
	{
		const INT Index = NumBits;
		const UBOOL bReallocate = (NumBits + 1) > MaxBits;

		NumBits++;

		if(bReallocate)
		{
			// Allocate memory for the new bits.
			const UINT MaxDWORDs = this->CalculateSlack(
				(NumBits + NumBitsPerDWORD - 1) / NumBitsPerDWORD,
				(MaxBits + NumBitsPerDWORD - 1) / NumBitsPerDWORD,
				sizeof(DWORD)
				);
			MaxBits = MaxDWORDs * NumBitsPerDWORD;
			Realloc(NumBits - 1);
		}

		(*this)(Index) = Value;

		return Index;
	}

	/**
	 * Removes all bits from the array, potentially leaving space allocated for an expected number of bits about to be added.
	 * @param ExpectedNumBits - The expected number of bits about to be added.
	 */
	void Empty(INT ExpectedNumBits = 0)
	{
		NumBits = 0;

		// If the expected number of bits doesn't match the allocated number of bits, reallocate.
		if(MaxBits != ExpectedNumBits)
		{
			MaxBits = ExpectedNumBits;
			Realloc(0);
		}
	}

	/**
	 * Resets the array's contents.
	 * @param Value - The value to initial the bits to.
	 * @param NumBits - The number of bits in the array.
	 */
	void Init(UBOOL Value,INT InNumBits)
	{
		Empty(InNumBits);
		if(InNumBits)
		{
			NumBits = InNumBits;
			appMemset(GetData(),Value ? 0xff : 0,(NumBits + NumBitsPerDWORD - 1) / NumBitsPerDWORD * sizeof(DWORD));
		}
	}

	/**
	 * Removes bits from the array.
	 * @param BaseIndex - The index of the first bit to remove.
	 * @param NumBitsToRemove - The number of consecutive bits to remove.
	 */
	void Remove(INT BaseIndex,INT NumBitsToRemove = 1)
	{
		check(BaseIndex >= 0 && BaseIndex + NumBitsToRemove <= NumBits);

		// Until otherwise necessary, this is an obviously correct implementation rather than an efficient implementation.
		FIterator WriteIt(*this);
		for(FConstIterator ReadIt(*this);ReadIt;++ReadIt)
		{
			// If this bit isn't being removed, write it back to the array at its potentially new index.
			if(ReadIt.GetIndex() < BaseIndex || ReadIt.GetIndex() >= BaseIndex + NumBitsToRemove)
			{
				if(WriteIt.GetIndex() != ReadIt.GetIndex())
				{
					WriteIt.GetValue() = (UBOOL)ReadIt.GetValue();
				}
				++WriteIt;
			}
		}
		NumBits -= NumBitsToRemove;
	}

	/* Removes bits from the array by swapping them with bits at the end of the array.
	 * This is mainly implemented so that other code using TArray::RemoveSwap will have
	 * matching indices.
 	 * @param BaseIndex - The index of the first bit to remove.
	 * @param NumBitsToRemove - The number of consecutive bits to remove.
	 */
	void RemoveSwap( INT BaseIndex, INT NumBitsToRemove=1 )
	{
		check(BaseIndex >= 0 && BaseIndex + NumBitsToRemove <= NumBits);
		if( BaseIndex < NumBits - NumBitsToRemove )
		{
			// Copy bits from the end to the region we are removing
			for( INT Index=0;Index<NumBitsToRemove;Index++ )
			{
				(*this)(BaseIndex + Index) = (*this)(NumBits - NumBitsToRemove + Index);
			}
		}
		// Remove the bits from the end of the array.
		Remove(NumBits - NumBitsToRemove, NumBitsToRemove);
	}
	

	/** 
	 * Helper function to return the amount of memory allocated by this container 
	 * @return number of bytes allocated by this container
	 */
	DWORD GetAllocatedSize( void ) const
	{
		return (MaxBits / NumBitsPerDWORD) * sizeof(DWORD);
	}

	/** Tracks the container's memory use through an archive. */
	void CountBytes(FArchive& Ar)
	{
		Ar.CountBytes(
			(NumBits / NumBitsPerDWORD) * sizeof(DWORD),
			(MaxBits / NumBitsPerDWORD) * sizeof(DWORD)
			);
	}

	// Accessors.
	FORCEINLINE INT Num() const { return NumBits; }
	FORCEINLINE FBitReference operator()(INT Index)
	{
		checkSlowish(Index>=0 && Index<NumBits);
		return FBitReference(
			GetData()[Index / NumBitsPerDWORD],
			1 << (Index & (NumBitsPerDWORD - 1))
			);
	}
	FORCEINLINE const FConstBitReference operator()(INT Index) const
	{
		checkSlowish(Index>=0 && Index<NumBits);
		return FConstBitReference(
			GetData()[Index / NumBitsPerDWORD],
			1 << (Index & (NumBitsPerDWORD - 1))
			);
	}
	FORCEINLINE FBitReference AccessCorrespondingBit(const FRelativeBitReference& RelativeReference)
	{
		checkSlow(RelativeReference.Mask);
		checkSlow(RelativeReference.DWORDIndex >= 0);
		checkSlow(((UINT)RelativeReference.DWORDIndex + 1) * NumBitsPerDWORD - 1 - appCountLeadingZeros(RelativeReference.Mask) < (UINT)NumBits);
		return FBitReference(
			GetData()[RelativeReference.DWORDIndex],
			RelativeReference.Mask
			);
	}
	FORCEINLINE const FConstBitReference AccessCorrespondingBit(const FRelativeBitReference& RelativeReference) const
	{
		checkSlow(RelativeReference.Mask);
		checkSlow(RelativeReference.DWORDIndex >= 0);
		checkSlow(((UINT)RelativeReference.DWORDIndex + 1) * NumBitsPerDWORD - 1 - appCountLeadingZeros(RelativeReference.Mask) < (UINT)NumBits);
		return FConstBitReference(
			GetData()[RelativeReference.DWORDIndex],
			RelativeReference.Mask
			);
	}

	/** BitArray iterator. */
	class FIterator : public FRelativeBitReference
	{
	public:
		FIterator(TBitArray<Allocator>& InArray,INT StartIndex = 0)
		:	FRelativeBitReference(StartIndex)
		,	Array(InArray)
		,	Index(StartIndex)
		{
		}
		FORCEINLINE FIterator& operator++()
		{
			++Index;
			this->Mask <<= 1;
			if(!this->Mask)
			{
				// Advance to the next DWORD.
				this->Mask = 1;
				++this->DWORDIndex;
			}
			return *this;
		}
		/** conversion to "bool" returning TRUE if the iterator is valid. */
		typedef bool PrivateBooleanType;
		FORCEINLINE operator PrivateBooleanType() const { return Index < Array.Num() ? &FIterator::Index : NULL; }
		FORCEINLINE bool operator !() const { return !operator PrivateBooleanType(); }

		FORCEINLINE FBitReference GetValue() const { return FBitReference(Array.GetData()[this->DWORDIndex],this->Mask); }
		FORCEINLINE INT GetIndex() const { return Index; }
	private:
		TBitArray<Allocator>& Array;
		INT Index;
	};

	/** Const BitArray iterator. */
	class FConstIterator : public FRelativeBitReference
	{
	public:
		FConstIterator(const TBitArray<Allocator>& InArray,INT StartIndex = 0)
		:	FRelativeBitReference(StartIndex)
		,	Array(InArray)
		,	Index(StartIndex)
		{
		}
		FORCEINLINE FConstIterator& operator++()
		{
			++Index;
			this->Mask <<= 1;
			if(!this->Mask)
			{
				// Advance to the next DWORD.
				this->Mask = 1;
				++this->DWORDIndex;
			}
			return *this;
		}
		/** conversion to "bool" returning TRUE if the iterator is valid. */
		typedef bool PrivateBooleanType;
		FORCEINLINE operator PrivateBooleanType() const { return Index < Array.Num() ? &FConstIterator::Index : NULL; }
		FORCEINLINE bool operator !() const { return !operator PrivateBooleanType(); }

		FORCEINLINE FConstBitReference GetValue() const { return FConstBitReference(Array.GetData()[this->DWORDIndex],this->Mask); }
		FORCEINLINE INT GetIndex() const { return Index; }
	private:
		const TBitArray<Allocator>& Array;
		INT Index;
	};

	FORCEINLINE const DWORD* GetData() const
	{
		return (DWORD*)this->GetAllocation();
	}

	FORCEINLINE DWORD* GetData()
	{
		return (DWORD*)this->GetAllocation();
	}

private:
	INT NumBits;
	INT MaxBits;

	void Realloc(INT PreviousNumBits)
	{
		const INT PreviousNumDWORDs = (PreviousNumBits + NumBitsPerDWORD - 1) / NumBitsPerDWORD;
		const INT MaxDWORDs = (MaxBits + NumBitsPerDWORD - 1) / NumBitsPerDWORD;

		this->ResizeAllocation(PreviousNumDWORDs,MaxDWORDs,sizeof(DWORD));

		if(MaxDWORDs)
		{
			// Reset the newly allocated slack DWORDs.
			appMemzero((DWORD*)this->GetAllocation() + PreviousNumDWORDs,(MaxDWORDs - PreviousNumDWORDs) * sizeof(DWORD));
		}
	}
};

/** An iterator which only iterates over set bits. */
template<typename Allocator>
class TConstSetBitIterator : public FRelativeBitReference
{
public:

	/** Constructor. */
	TConstSetBitIterator(const TBitArray<Allocator>& InArray,INT StartIndex = 0)
	:	FRelativeBitReference(StartIndex)
	,	Array(InArray)
	,	UnvisitedBitMask((~0) << (StartIndex & (NumBitsPerDWORD - 1)))
	,	CurrentBitIndex(StartIndex)
	,	BaseBitIndex(StartIndex & ~(NumBitsPerDWORD - 1))
	{
		FindFirstSetBit();
	}

	/** Advancement operator. */
	FORCEINLINE TConstSetBitIterator& operator++()
	{
		// Mark the current bit as visited.
		UnvisitedBitMask &= ~this->Mask;

		// Find the first set bit that hasn't been visited yet.
		FindFirstSetBit();

		return *this;
	}

	/** conversion to "bool" returning TRUE if the iterator is valid. */
	typedef bool PrivateBooleanType;
	FORCEINLINE operator PrivateBooleanType() const { return CurrentBitIndex < Array.Num() ? &TConstSetBitIterator::CurrentBitIndex : NULL; }
	FORCEINLINE bool operator !() const { return !operator PrivateBooleanType(); }

	/** Index accessor. */
	FORCEINLINE INT GetIndex() const
	{
		return CurrentBitIndex;
	}

private:

	const TBitArray<Allocator>& Array;

	DWORD UnvisitedBitMask;
	INT CurrentBitIndex;
	INT BaseBitIndex;

	/** Find the first set bit starting with the current bit, inclusive. */
	FORCEINLINE void FindFirstSetBit()
	{
		const DWORD EmptyArrayData = 0;
		const DWORD* ArrayData = IfAThenAElseB(Array.GetData(),&EmptyArrayData);

		// Advance to the next non-zero DWORD.
		DWORD RemainingBitMask = ArrayData[this->DWORDIndex] & UnvisitedBitMask;
		while(!RemainingBitMask)
		{
			this->DWORDIndex++;
			BaseBitIndex += NumBitsPerDWORD;
			const INT LastDWORDIndex = (Array.Num() - 1) / NumBitsPerDWORD;
			if(this->DWORDIndex <= LastDWORDIndex)
			{
				RemainingBitMask = ArrayData[this->DWORDIndex];
				UnvisitedBitMask = ~0;
			}
			else
			{
				// We've advanced past the end of the array.
				CurrentBitIndex = Array.Num();
				return;
			}
		};

		// We can assume that RemainingBitMask!=0 here.
		checkSlow(RemainingBitMask);

		// This operation has the effect of unsetting the lowest set bit of BitMask
		const DWORD NewRemainingBitMask = RemainingBitMask & (RemainingBitMask - 1);

		// This operation XORs the above mask with the original mask, which has the effect
		// of returning only the bits which differ; specifically, the lowest bit
		this->Mask = NewRemainingBitMask ^ RemainingBitMask;

		// If the Nth bit was the lowest set bit of BitMask, then this gives us N
		CurrentBitIndex = BaseBitIndex + NumBitsPerDWORD - 1 - appCountLeadingZeros(this->Mask);
	}
};

/** An iterator which only iterates over the bits which are set in both of two bit-arrays. */
template<typename Allocator,typename OtherAllocator>
class TConstDualSetBitIterator : public FRelativeBitReference
{
public:

	/** Constructor. */
	TConstDualSetBitIterator(
		const TBitArray<Allocator>& InArrayA,
		const TBitArray<OtherAllocator>& InArrayB,
		INT StartIndex = 0
		)
	:	FRelativeBitReference(StartIndex)
	,	ArrayA(InArrayA)
	,	ArrayB(InArrayB)
	,	UnvisitedBitMask((~0) << (StartIndex & (NumBitsPerDWORD - 1)))
	,	CurrentBitIndex(StartIndex)
	,	BaseBitIndex(StartIndex & ~(NumBitsPerDWORD - 1))
	{
		check(ArrayA.Num() == ArrayB.Num());

		FindFirstSetBit();
	}

	/** Advancement operator. */
	FORCEINLINE TConstDualSetBitIterator& operator++()
	{
		checkSlow(ArrayA.Num() == ArrayB.Num());

		// Mark the current bit as visited.
		UnvisitedBitMask &= ~this->Mask;

		// Find the first set bit that hasn't been visited yet.
		FindFirstSetBit();

		return *this;

	}

	/** conversion to "bool" returning TRUE if the iterator is valid. */
	typedef bool PrivateBooleanType;
	FORCEINLINE operator PrivateBooleanType() const { return CurrentBitIndex < ArrayA.Num() ? &TConstDualSetBitIterator::CurrentBitIndex : NULL; }
	FORCEINLINE bool operator !() const { return !operator PrivateBooleanType(); }

	/** Index accessor. */
	FORCEINLINE INT GetIndex() const
	{
		return CurrentBitIndex;
	}

private:

	const TBitArray<Allocator>& ArrayA;
	const TBitArray<OtherAllocator>& ArrayB;

	DWORD UnvisitedBitMask;
	INT CurrentBitIndex;
	INT BaseBitIndex;

	/** Find the first bit that is set in both arrays, starting with the current bit, inclusive. */
	void FindFirstSetBit()
	{
		static const DWORD EmptyArrayData = 0;
		const DWORD* ArrayDataA = IfAThenAElseB(ArrayA.GetData(),&EmptyArrayData);
		const DWORD* ArrayDataB = IfAThenAElseB(ArrayB.GetData(),&EmptyArrayData);

		// Advance to the next non-zero DWORD.
		DWORD RemainingBitMask = ArrayDataA[this->DWORDIndex] & ArrayDataB[this->DWORDIndex] & UnvisitedBitMask;
		while(!RemainingBitMask)
		{
			this->DWORDIndex++;
			BaseBitIndex += NumBitsPerDWORD;
			const INT LastDWORDIndex = (ArrayA.Num() - 1) / NumBitsPerDWORD;
			if(this->DWORDIndex <= LastDWORDIndex)
			{
				RemainingBitMask = ArrayDataA[this->DWORDIndex] & ArrayDataB[this->DWORDIndex];
				UnvisitedBitMask = ~0;
			}
			else
			{
				// We've advanced past the end of the array.
				CurrentBitIndex = ArrayA.Num();
				return;
			}
		};

		// We can assume that RemainingBitMask!=0 here.
		checkSlow(RemainingBitMask);

		// This operation has the effect of unsetting the lowest set bit of BitMask
		const DWORD NewRemainingBitMask = RemainingBitMask & (RemainingBitMask - 1);

		// This operation XORs the above mask with the original mask, which has the effect
		// of returning only the bits which differ; specifically, the lowest bit
		this->Mask = NewRemainingBitMask ^ RemainingBitMask;

		// If the Nth bit was the lowest set bit of BitMask, then this gives us N
		CurrentBitIndex = BaseBitIndex + NumBitsPerDWORD - 1 - appCountLeadingZeros(this->Mask);
	}
};

/** A specialization of the exchange macro that avoids reallocating when exchanging two bit arrays. */
template<typename Allocator>
inline void Exchange(TBitArray<Allocator>& A,TBitArray<Allocator>& B)
{
	appMemswap(&A,&B,sizeof(TBitArray<Allocator>));
}

#endif
