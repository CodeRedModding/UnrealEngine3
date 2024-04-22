/*=============================================================================
	SparseArray.h: Sparse array definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SPARSEARRAY_H__
#define __SPARSEARRAY_H__

#include "Array.h"
#include "BitArray.h"

// Forward declarations.
template<typename ElementType,typename Allocator = FDefaultSparseArrayAllocator >
class TSparseArray;

/**
 * The result of a sparse array allocation.
 */
struct FSparseArrayAllocationInfo
{
	INT Index;
	void* Pointer;
};

/** Allocated elements are overlapped with free element info in the element list. */
template<typename ElementType>
union TSparseArrayElementOrFreeListLink
{
	/** If the element is allocated, its value is stored here. */
	ElementType ElementData;

	/** If the element isn't allocated, this is a link in the free element list. */
	INT NextFreeIndex;
};

/**
 * A dynamically sized array where element indices aren't necessarily contiguous.  Memory is allocated for all 
 * elements in the array's index range, so it doesn't save memory; but it does allow O(1) element removal that 
 * doesn't invalidate the indices of subsequent elements.  It uses TArray to store the elements, and a TBitArray
 * to store whether each element index is allocated (for fast iteration over allocated elements).
 *
 **/
template<typename ElementType,typename Allocator /*= FDefaultSparseArrayAllocator */>
class TSparseArray
{
public:

	typedef TBitArray<typename Allocator::BitArrayAllocator> AllocationBitArrayType;

	/** Destructor. */
	~TSparseArray()
	{
		// Destruct the elements in the array.
		Empty();
	}

	/**
	 * Allocates space for an element in the array.  The element is not initialized, and you must use the corresponding placement new operator
	 * to construct the element in the allocated memory.
	 */
	FSparseArrayAllocationInfo Add()
	{
		FSparseArrayAllocationInfo Result;

		if(NumFreeIndices > 0)
		{
			// Remove and use the first index from the list of free elements.
			Result.Index = FirstFreeIndex;
			FirstFreeIndex = GetData(FirstFreeIndex).NextFreeIndex;
			--NumFreeIndices;
		}
		else
		{
			// Add a new element.
			Result.Index = Data.Add(1);
			AllocationFlags.AddItem(TRUE);
		}

		// Compute the pointer to the new element's data.
		Result.Pointer = &GetData(Result.Index).ElementData;

		// Flag the element as allocated.
		AllocationFlags(Result.Index) = TRUE;

		return Result;
	}

	/**
	 * Adds an element to the array.
	 */
	INT AddItem(typename TContainerTraits<ElementType>::ConstInitType Element)
	{
		FSparseArrayAllocationInfo Allocation = Add();
		new(Allocation) ElementType(Element);
		return Allocation.Index;
	}

	/**
	 * Removes an element from the array.
	 */
	void Remove(INT BaseIndex,INT Count = 1)
	{
		for(INT Index = 0;Index < Count;Index++)
		{
			check(AllocationFlags(BaseIndex + Index));

			// Destruct the element being removed.
			if(TContainerTraits<ElementType>::NeedsDestructor)
			{
				(*this)(BaseIndex + Index).~ElementType();
			}

			// Mark the element as free and add it to the free element list.
			GetData(BaseIndex + Index).NextFreeIndex = NumFreeIndices > 0 ? FirstFreeIndex : INDEX_NONE;
			FirstFreeIndex = BaseIndex + Index;
			++NumFreeIndices;
			AllocationFlags(BaseIndex + Index) = FALSE;
		}
	}

	/**
	 * Removes all elements from the array, potentially leaving space allocated for an expected number of elements about to be added.
	 * @param ExpectedNumElements - The expected number of elements about to be added.
	 */
	void Empty(INT ExpectedNumElements = 0)
	{
		// Destruct the allocated elements.
		if( TContainerTraits<ElementType>::NeedsDestructor )
		{
			for(TIterator It(*this);It;++It)
			{
				ElementType& Element = *It;
				Element.~ElementType();
			}
		}

		// Free the allocated elements.
		Data.Empty(ExpectedNumElements);
		FirstFreeIndex = 0;
		NumFreeIndices = 0;
		AllocationFlags.Empty(ExpectedNumElements);
	}

	/**
	 * Preallocates enough memory to contain the specified number of elements.
	 *
	 * @param	ExpectedNumElements		the total number of elements that the array will have
	 */
	void Reserve(INT ExpectedNumElements)
	{
		if ( ExpectedNumElements > Data.Num() )
		{
			const INT ElementsToAdd = ExpectedNumElements - Data.Num();
			if ( ElementsToAdd > 0 )
			{
				// allocate memory in the array itself
				INT ElementIndex = Data.Add(ElementsToAdd);

				// now mark the new elements as free
				const INT NewElementCount = Data.Num();
				for ( INT FreeIndex = ElementIndex; FreeIndex < NewElementCount; FreeIndex++ )
				{
					GetData(FreeIndex).NextFreeIndex = NumFreeIndices > 0 ? FirstFreeIndex : INDEX_NONE;
					FirstFreeIndex = FreeIndex;
					++NumFreeIndices;
				}
				//@fixme - this will have to do until TBitArray has a Reserve method....
				for ( INT i = 0; i < ElementsToAdd; i++ )
				{
					AllocationFlags.AddItem(FALSE);
				}
			}
		}
	}

	/** Shrinks the array's storage to avoid slack. */
	void Shrink()
	{
		// Determine the highest allocated index in the data array.
		INT MaxAllocatedIndex = INDEX_NONE;
		for(TConstSetBitIterator<typename Allocator::BitArrayAllocator> AllocatedIndexIt(AllocationFlags);AllocatedIndexIt;++AllocatedIndexIt)
		{
			MaxAllocatedIndex = Max(MaxAllocatedIndex,AllocatedIndexIt.GetIndex());
		}

		const INT FirstIndexToRemove = MaxAllocatedIndex + 1;
		if(FirstIndexToRemove < Data.Num())
		{
			if(NumFreeIndices > 0)
			{
				// Look for elements in the free list that are in the memory to be freed.
				INT* PreviousNextFreeIndex = &FirstFreeIndex;
				for(INT FreeIndex = FirstFreeIndex;
					FreeIndex != INDEX_NONE;
					FreeIndex = *PreviousNextFreeIndex)
				{
					if(FreeIndex >= FirstIndexToRemove)
					{
						*PreviousNextFreeIndex = GetData(FreeIndex).NextFreeIndex;
						--NumFreeIndices;
					}
					else
					{
						PreviousNextFreeIndex = &GetData(FreeIndex).NextFreeIndex;
					}
				}
			}

			// Truncate unallocated elements at the end of the data array.
			Data.Remove(FirstIndexToRemove,Data.Num() - FirstIndexToRemove);
			AllocationFlags.Remove(FirstIndexToRemove,AllocationFlags.Num() - FirstIndexToRemove);
		}

		// Shrink the data array.
		Data.Shrink();
	}

	/** Compacts the allocated elements into a contiguous index range. */
	void Compact()
	{
		// Copy the existing elements to a new array.
		TSparseArray<ElementType,Allocator> CompactedArray;
		CompactedArray.Empty(Num());
		for(TConstIterator It(*this);It;++It)
		{
			new(CompactedArray.Add()) ElementType(*It);
		}

		// Replace this array with the compacted array.
		Exchange(*this,CompactedArray);
	}

	/** Sorts the elements using the provided comparison class. */
	template<typename CompareClass>
	void Sort()
	{
		if(Num() > 0)
		{
			// Compact the elements array so all the elements are contiguous.
			Compact();

			// Sort the elements according to the provided comparison class.
			::Sort<FElementOrFreeListLink,ElementCompareClass<CompareClass> >(&GetData(0),Num());
		}
	}

	/** 
	 * Helper function to return the amount of memory allocated by this container 
	 * @return number of bytes allocated by this container
	 */
	DWORD GetAllocatedSize( void ) const
	{
		return	(Data.Num() + Data.GetSlack()) * sizeof(FElementOrFreeListLink) +
				AllocationFlags.GetAllocatedSize();
	}

	/** Tracks the container's memory use through an archive. */
	void CountBytes(FArchive& Ar)
	{
		Data.CountBytes(Ar);
		AllocationFlags.CountBytes(Ar);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,TSparseArray& Array)
	{
		Array.CountBytes(Ar);
		if( Ar.IsLoading() )
		{
			// Load array.
			INT NewNumElements = 0;
			Ar << NewNumElements;
			Array.Empty( NewNumElements );
			for(INT ElementIndex = 0;ElementIndex < NewNumElements;ElementIndex++)
			{
				Ar << *::new(Array.Add())ElementType;
			}
		}
		else
		{
			// Save array.
			INT NewNumElements = Array.Num();
			Ar << NewNumElements;
			for(TIterator It(Array);It;++It)
			{
				Ar << *It;
			}
		}
		return Ar;
	}

	/**
	 * Equality comparison operator.
	 * Checks that both arrays have the same elements and element indices; that means that unallocated elements are signifigant!
	 */
	friend UBOOL operator==(const TSparseArray& A,const TSparseArray& B)
	{
		if(A.GetMaxIndex() != B.GetMaxIndex())
		{
			return FALSE;
		}

		for(INT ElementIndex = 0;ElementIndex < A.GetMaxIndex();ElementIndex++)
		{
			const UBOOL bIsAllocatedA = A.IsAllocated(ElementIndex);
			const UBOOL bIsAllocatedB = B.IsAllocated(ElementIndex);
			if(bIsAllocatedA != bIsAllocatedB)
			{
				return FALSE;
			}
			else if(bIsAllocatedA)
			{
				if(A(ElementIndex) != B(ElementIndex))
				{
					return FALSE;
				}
			}
		}

		return TRUE;
	}

	/**
	 * Inequality comparison operator.
	 * Checks that both arrays have the same elements and element indices; that means that unallocated elements are signifigant!
	 */
	friend UBOOL operator!=(const TSparseArray& A,const TSparseArray& B)
	{
		return !(A == B);
	}

	/** Default constructor. */
	TSparseArray()
	:	FirstFreeIndex(0)
	,	NumFreeIndices(0)
	{}

	/** Copy constructor. */
	TSparseArray(const TSparseArray& InCopy)
	:	FirstFreeIndex(0)
	,	NumFreeIndices(0)
	{
		*this = InCopy;
	}

	/** Assignment operator. */
	TSparseArray& operator=(const TSparseArray& InCopy)
	{
		if(this != &InCopy)
		{
			// Reallocate the array.
			Empty(InCopy.GetMaxIndex());
			Data.Add(InCopy.GetMaxIndex());

			// Copy the other array's element allocation state.
			FirstFreeIndex = InCopy.FirstFreeIndex;
			NumFreeIndices = InCopy.NumFreeIndices;
			AllocationFlags = InCopy.AllocationFlags;

			// Determine whether we need per element construction or bulk copy is fine
			if (TContainerTraits<ElementType>::NeedsConstructor)
			{
				// Use the inplace new to copy the element to an array element
				for(INT Index = 0;Index < InCopy.GetMaxIndex();Index++)
				{
					const FElementOrFreeListLink& SourceElement = InCopy.GetData(Index);
					FElementOrFreeListLink& DestElement = GetData(Index);
					if(InCopy.IsAllocated(Index))
					{
						::new((BYTE*)&DestElement.ElementData) ElementType(*(ElementType*)&SourceElement.ElementData);
					}
					else
					{
						DestElement.NextFreeIndex = SourceElement.NextFreeIndex;
					}
				}
			}
			else
			{
				// Use the much faster path for types that allow it
				appMemcpy(Data.GetData(),InCopy.Data.GetData(),sizeof(FElementOrFreeListLink) * InCopy.GetMaxIndex());
			}
		}
		return *this;
	}

	// Accessors.
	ElementType& operator()(INT Index)
	{
		checkSlow(Index >= 0);
		checkSlow(Index < Data.Num());
		checkSlow(Index < AllocationFlags.Num());
		checkSlow(AllocationFlags(Index));
		return *(ElementType*)&GetData(Index).ElementData;
	}
	const ElementType& operator()(INT Index) const
	{
		checkSlow(Index >= 0);
		checkSlow(Index < Data.Num());
		checkSlow(AllocationFlags(Index));
		return *(ElementType*)&GetData(Index).ElementData;
	}
	UBOOL IsAllocated(INT Index) const { return AllocationFlags(Index); }
	INT GetMaxIndex() const { return Data.Num(); }
	INT Num() const { return Data.Num() - NumFreeIndices; }

private:

	/** The base class of sparse array iterators. */
	template<bool bConst>
	class TBaseIterator
	{
	private:

		typedef typename TChooseClass<bConst,const TSparseArray,TSparseArray>::Result ArrayType;
		typedef typename TChooseClass<bConst,const ElementType,ElementType>::Result ItElementType;

		// private class for safe bool conversion
		struct PrivateBooleanHelper { INT Value; };

	public:
		TBaseIterator(ArrayType& InArray,INT StartIndex = 0):
			Array(InArray),
			BitArrayIt(InArray.AllocationFlags,StartIndex)
		{}
		FORCEINLINE TBaseIterator& operator++()
		{
			// Iterate to the next set allocation flag.
			++BitArrayIt;
			return *this;
		}
		FORCEINLINE INT GetIndex() const { return BitArrayIt.GetIndex(); }

		/** conversion to "bool" returning TRUE if the iterator is valid. */
		typedef bool PrivateBooleanType;
		FORCEINLINE operator PrivateBooleanType() const { return BitArrayIt ? &PrivateBooleanHelper::Value : NULL; }
		FORCEINLINE bool operator !() const { return !operator PrivateBooleanType(); }

		FORCEINLINE ItElementType& operator*() const { return Array(GetIndex()); }
		FORCEINLINE ItElementType* operator->() const { return &Array(GetIndex()); }
		FORCEINLINE const FRelativeBitReference& GetRelativeBitReference() const { return BitArrayIt; }

		/** Safely removes the current element from the array. */
		void RemoveCurrent()
		{
			Array.Remove(GetIndex());
		}
	private:
		ArrayType& Array;
		TConstSetBitIterator<typename Allocator::BitArrayAllocator> BitArrayIt;
	};

public:

	/** Iterates over all allocated elements in a sparse array. */
	class TIterator : public TBaseIterator<false>
	{
	public:
		TIterator(TSparseArray& InArray,INT StartIndex = 0):
			TBaseIterator<false>(InArray,StartIndex)
		{}
	};

	/** Iterates over all allocated elements in a const sparse array. */
	class TConstIterator : public TBaseIterator<true>
	{
	public:
		TConstIterator(const TSparseArray& InArray,INT StartIndex = 0):
			TBaseIterator<true>(InArray,StartIndex)
		{}
	};

	/** An iterator which only iterates over the elements of the array which correspond to set bits in a separate bit array. */
	template<typename SubsetAllocator = FDefaultBitArrayAllocator>
	class TConstSubsetIterator
	{
	private:
		// private class for safe bool conversion
		struct PrivateBooleanHelper { INT Value; };

	public:
		TConstSubsetIterator( const TSparseArray& InArray, const TBitArray<SubsetAllocator>& InBitArray ):
			Array(InArray),
			BitArrayIt(InArray.AllocationFlags,InBitArray)
		{}
		FORCEINLINE TConstSubsetIterator& operator++()
		{
			// Iterate to the next element which is both allocated and has its bit set in the other bit array.
			++BitArrayIt;
			return *this;
		}
		FORCEINLINE INT GetIndex() const { return BitArrayIt.GetIndex(); }
		
		/** conversion to "bool" returning TRUE if the iterator is valid. */
		typedef bool PrivateBooleanType;
		FORCEINLINE operator PrivateBooleanType() const { return BitArrayIt ? &PrivateBooleanHelper::Value : NULL; }
		FORCEINLINE bool operator !() const { return !operator PrivateBooleanType(); }

		FORCEINLINE const ElementType& operator*() const { return Array(GetIndex()); }
		FORCEINLINE const ElementType* operator->() const { return &Array(GetIndex()); }
		FORCEINLINE const FRelativeBitReference& GetRelativeBitReference() const { return BitArrayIt; }
	private:
		const TSparseArray& Array;
		TConstDualSetBitIterator<typename Allocator::BitArrayAllocator,SubsetAllocator> BitArrayIt;
	};

	/** Concatenation operators */
	TSparseArray& operator+=( const TSparseArray& OtherArray )
	{
		this->Reserve(this->Num() + OtherArray.Num());
		for ( typename TSparseArray::TConstIterator It(OtherArray); It; ++It )
		{
			this->AddItem(*It);
		}
		return *this;
	}
	TSparseArray& operator+=( const TArray<ElementType>& OtherArray )
	{
		this->Reserve(this->Num() + OtherArray.Num());
		for ( INT Idx = 0; Idx < OtherArray.Num(); Idx++ )
		{
			this->AddItem(OtherArray(Idx));
		}
		return *this;
	}

private:

	/**
	 * The element type stored is only indirectly related to the element type requested, to avoid instantiating TArray redundantly for
	 * compatible types.
	 */
	typedef TSparseArrayElementOrFreeListLink<
		TAlignedBytes<sizeof(ElementType),TElementAlignmentCalculator<ElementType>::Alignment>
		> FElementOrFreeListLink;

	/** Extracts the element value from the array's element structure and passes it to the user provided comparison class. */
	template<typename CompareClass>
	class ElementCompareClass
	{
	public:
		static INT Compare(const FElementOrFreeListLink& A,const FElementOrFreeListLink& B)
		{
			return CompareClass::Compare(*(ElementType*)&A.ElementData,*(ElementType*)&B.ElementData);
		}
	};

	TArray<FElementOrFreeListLink,typename Allocator::ElementAllocator> Data;
	AllocationBitArrayType AllocationFlags;

	/** The index of an unallocated element in the array that currently contains the head of the linked list of free elements. */
	INT FirstFreeIndex;

	/** The number of elements in the free list. */
	INT NumFreeIndices;

	/** Accessor for the element or free list data. */
	FElementOrFreeListLink& GetData(INT Index)
	{
		return ((FElementOrFreeListLink*)Data.GetData())[Index];
	}

	/** Accessor for the element or free list data. */
	const FElementOrFreeListLink& GetData(INT Index) const
	{
		return ((FElementOrFreeListLink*)Data.GetData())[Index];
	}
};

/**
 * A placement new operator which constructs an element in a sparse array allocation.
 */
inline void* operator new(size_t Size,const FSparseArrayAllocationInfo& Allocation)
{
	return Allocation.Pointer;
}

/** A specialization of the exchange macro that avoids reallocating when exchanging two arrays. */
template <typename ElementType,typename Allocator>
inline void Exchange(
	TSparseArray<ElementType,Allocator>& A,
	TSparseArray<ElementType,Allocator>& B
	)
{
	appMemswap( &A, &B, sizeof(A) );
}

#endif
