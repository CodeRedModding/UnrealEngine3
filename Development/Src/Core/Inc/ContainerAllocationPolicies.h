/*=============================================================================
	ContainerAllocationPolicies.h: Defines allocation policies for containers.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __CONTAINERALLOCATIONPOLICIES_H__
#define __CONTAINERALLOCATIONPOLICIES_H__

#include "MemoryBase.h"

/** The default slack calculation heuristic. */
extern INT DefaultCalculateSlack(INT NumElements,INT NumAllocatedElements,UINT BytesPerElement);

/** Used to determine the alignment of an element type. */
template<typename ElementType>
class TElementAlignmentCalculator
{
private:

	// We disable this warning as we're intentionally expecting packing to affect the alignment
#pragma warning(push)
#pragma warning(disable : 4121) // alignment of a member was sensitive to packing

	/**
	 * This is a dummy type that's used to calculate the padding added between the byte and the element
	 * to fulfill the type's required alignment.
	 */
	struct FAlignedElements
	{
		BYTE MisalignmentPadding;
		ElementType Element;

		/** FAlignedElement's default constructor is declared but never implemented to avoid the need for a ElementType default constructor. */
		FAlignedElements();
	};

#pragma warning(pop)

public:

	enum { Alignment = sizeof(FAlignedElements) - sizeof(ElementType) };
};

/** A type which is used to represent a script type that is unknown at compile time. */
struct FScriptContainerElement
{
};

/**
 * Used to declare an untyped array of data with compile-time alignment.
 * It needs to use template specialization as the MS_ALIGN and GCC_ALIGN macros require literal parameters.
 */
template<INT Size,DWORD Alignment>
class TAlignedBytes
{
	BYTE Data[-Size]; // this intentionally won't compile, we don't support the requested alignment
};

/** Unaligned storage. */
template<INT Size>
struct TAlignedBytes<Size,1>
{
	BYTE Pad[Size];
};


// C++/CLI doesn't support alignment of native types in managed code, so we enforce that the element
// size is a multiple of the desired alignment
#ifdef __cplusplus_cli
	#define IMPLEMENT_ALIGNED_STORAGE(Align) \
		template<INT Size>        \
		struct TAlignedBytes<Size,Align> \
		{ \
			BYTE Pad[Size]; \
			checkAtCompileTime( Size % Align == 0, CLRInteropTypesMustNotBeAligned ); \
		};
#else
/** A macro that implements TAlignedBytes for a specific alignment. */
#define IMPLEMENT_ALIGNED_STORAGE(Align) \
	template<INT Size>        \
	struct TAlignedBytes<Size,Align> \
	{ \
		struct MS_ALIGN(Align) TPadding \
		{ \
			BYTE Pad[Size]; \
		} GCC_ALIGN(Align); \
		TPadding Padding; \
	};
#endif

// Implement TAlignedBytes for these alignments.
IMPLEMENT_ALIGNED_STORAGE(16);
IMPLEMENT_ALIGNED_STORAGE(8);
IMPLEMENT_ALIGNED_STORAGE(4);
IMPLEMENT_ALIGNED_STORAGE(2);

#undef IMPLEMENT_ALIGNED_STORAGE

/** An untyped array of data with compile-time alignment and size derived from another type. */
template<typename ElementType>
class TTypeCompatibleBytes :
	public TAlignedBytes<
		sizeof(ElementType),
		TElementAlignmentCalculator<ElementType>::Alignment
		>
{};

/** This is the allocation policy interface; it exists purely to document the policy's interface, and should not be used. */
class FContainerAllocatorInterface
{
public:

	/** Determines whether the user of the allocator may use the ForAnyElementType inner class. */
	enum { NeedsElementType = TRUE };

	/**
	 * A class that receives both the explicit allocation policy template parameters specified by the user of the container,
	 * but also the implicit ElementType template parameter from the container type.
	 */
	template<typename ElementType>
	class ForElementType
	{
		/** Accesses the container's current data. */
		ElementType* GetAllocation() const;

		/**
		 * Resizes the container's allocation.
		 * @param PreviousNumElements - The number of elements that were stored in the previous allocation.
		 * @param NumElements - The number of elements to allocate space for.
		 * @param NumBytesPerElement - The number of bytes/element.
		 */
		void ResizeAllocation(
			INT PreviousNumElements,
			INT NumElements,
			INT NumBytesPerElement
			);

		/**
		 * Calculates the amount of slack to allocate for an array that has just grown to a given number of elements.
		 * @param NumElements - The number of elements to allocate space for.
		 * @param CurrentNumSlackElements - The current number of slack elements allocated.
		 * @param NumBytesPerElement - The number of bytes/element.
		 */
		INT CalculateSlack(
			INT NumElements,
			INT CurrentNumSlackElements,
			INT NumBytesPerElement
			) const;

		INT GetAllocatedSize(INT NumAllocatedElements, INT NumBytesPerElement) const;
	};

	/** A class that may be used when NeedsElementType=TRUE is specified. */
	typedef ForElementType<FScriptContainerElement> ForAnyElementType;
};

/** The indirect allocation policy always allocates the elements indirectly. */
template<DWORD Alignment = DEFAULT_ALIGNMENT>
class TAlignedHeapAllocator
{
public:

	enum { NeedsElementType = FALSE };

	class ForAnyElementType
	{
	public:

		/** Default constructor. */
		ForAnyElementType()
			: Data(NULL)
		{}

		/** ENoInit constructor. */
		ForAnyElementType(ENoInit)
		{}

		/** Destructor. */
		~ForAnyElementType()
		{
			if(Data)
			{
				appFree(Data);
				Data = NULL;
			}
		}

		// FContainerAllocatorInterface
		FORCEINLINE FScriptContainerElement* GetAllocation() const
		{
			return Data;
		}
		void ResizeAllocation(
			INT PreviousNumElements,
			INT NumElements,
			INT NumBytesPerElement
			)
		{
			// Avoid calling appRealloc( NULL, 0 ) as ANSI C mandates returning a valid pointer which is not what we want.
			if( Data || NumElements )
			{
				//checkSlow(((QWORD)NumElements*(QWORD)ElementTypeInfo.GetSize() < (QWORD)INT_MAX));
				Data = (FScriptContainerElement*)appRealloc( Data, NumElements*NumBytesPerElement, Alignment );
			}
		}
		INT CalculateSlack(
			INT NumElements,
			INT NumAllocatedElements,
			INT NumBytesPerElement
			) const
		{
			return DefaultCalculateSlack(NumElements,NumAllocatedElements,NumBytesPerElement);
		}

		INT GetAllocatedSize(INT NumAllocatedElements, INT NumBytesPerElement) const
		{
			return NumAllocatedElements * NumBytesPerElement;
		}

	private:

		/** A pointer to the container's elements. */
		FScriptContainerElement* Data;
	};

	template<typename ElementType>
	class ForElementType : public ForAnyElementType
	{
	public:

		/** Default constructor. */
		ForElementType()
		{}

		/** ENoInit constructor. */
		ForElementType(ENoInit)
		:	ForAnyElementType(E_NoInit)
		{}

		FORCEINLINE ElementType* GetAllocation() const
		{
			return (ElementType*)ForAnyElementType::GetAllocation();
		}
	};
};

/** The indirect allocation policy always allocates the elements indirectly. */
class FHeapAllocator
{
public:

	enum { NeedsElementType = FALSE };

	class ForAnyElementType
	{
	public:

		/** Default constructor. */
		ForAnyElementType()
			: Data(NULL)
		{}

		/** ENoInit constructor. */
		ForAnyElementType(ENoInit)
		{}

		/** Destructor. */
		~ForAnyElementType()
		{
			if(Data)
			{
				appFree(Data);
				Data = NULL;
			}
		}

		// FContainerAllocatorInterface
		FORCEINLINE FScriptContainerElement* GetAllocation() const
		{
			return Data;
		}
		void ResizeAllocation(INT PreviousNumElements,INT NumElements,INT NumBytesPerElement)
		{
			// Avoid calling appRealloc( NULL, 0 ) as ANSI C mandates returning a valid pointer which is not what we want.
			if( Data || NumElements )
			{
				//checkSlow(((QWORD)NumElements*(QWORD)ElementTypeInfo.GetSize() < (QWORD)INT_MAX));
				Data = (FScriptContainerElement*)appRealloc( Data, NumElements*NumBytesPerElement, DEFAULT_ALIGNMENT );
			}
		}
		INT CalculateSlack(INT NumElements,INT NumAllocatedElements,INT NumBytesPerElement) const
		{
			return DefaultCalculateSlack(NumElements,NumAllocatedElements,NumBytesPerElement);
		}

		INT GetAllocatedSize(INT NumAllocatedElements, INT NumBytesPerElement) const
		{
			return NumAllocatedElements * NumBytesPerElement;
		}

	private:

		/** A pointer to the container's elements. */
		FScriptContainerElement* Data;
	};
	
	template<typename ElementType>
	class ForElementType : public ForAnyElementType
	{
	public:

		/** Default constructor. */
		ForElementType()
		{}

		/** ENoInit constructor. */
		ForElementType(ENoInit)
		:	ForAnyElementType(E_NoInit)
		{}

		FORCEINLINE ElementType* GetAllocation() const
		{
			return (ElementType*)ForAnyElementType::GetAllocation();
		}
	};
};

/** The indirect allocation policy with DEFAULT_ALIGNMENT is used by default. */
class FDefaultAllocator : public FHeapAllocator
{
};

/**
 * The inline allocation policy allocates up to a specified number of bytes in the same allocation as the container.
 * Any allocation needed beyond that causes all data to be moved into an indirect allocation.
 * It always uses DEFAULT_ALIGNMENT.
 */
template<UINT NumInlineElements,typename SecondaryAllocator = FDefaultAllocator >
class TInlineAllocator
{
public:

	enum { NeedsElementType = TRUE };

	template<typename ElementType>
	class ForElementType
	{
	public:

		/** Default constructor. */
		ForElementType()
		{
		}

		/** ENoInit constructor. */
		ForElementType(ENoInit)
		{}

		/** Destructor. */
		~ForElementType()
		{
		}

		// FContainerAllocatorInterface
		FORCEINLINE ElementType* GetAllocation() const
		{
			return IfAThenAElseB<ElementType>(SecondaryData.GetAllocation(),GetInlineElements());
		}
		void ResizeAllocation(INT PreviousNumElements,INT NumElements,INT NumBytesPerElement)
		{
			const INT PreviousNumBytes = PreviousNumElements * NumBytesPerElement;

			// Check if the new allocation will fit in the inline data area.
			if(NumElements <= NumInlineElements)
			{
				// If the old allocation wasn't in the inline data area, move it into the inline data area.
				if(SecondaryData.GetAllocation())
				{
					appMemcpy(GetInlineElements(),SecondaryData.GetAllocation(),PreviousNumBytes);

					// Free the old indirect allocation.
					SecondaryData.ResizeAllocation(0,0,NumBytesPerElement);
				}
			}
			else
			{
				if(!SecondaryData.GetAllocation())
				{
					// Allocate new indirect memory for the data.
					SecondaryData.ResizeAllocation(0,NumElements,NumBytesPerElement);

					// Move the data out of the inline data area into the new allocation.
					appMemcpy(SecondaryData.GetAllocation(),GetInlineElements(),PreviousNumBytes);
				}
				else
				{
					// Reallocate the indirect data for the new size.
					SecondaryData.ResizeAllocation(PreviousNumElements,NumElements,NumBytesPerElement);
				}
			}
		}
		INT CalculateSlack(INT NumElements,INT NumAllocatedElements,INT NumBytesPerElement) const
		{
			// If the elements use less space than the inline allocation, only use the inline allocation as slack.
			return NumElements <= NumInlineElements ?
				NumInlineElements :
				SecondaryData.CalculateSlack(NumElements,NumAllocatedElements,NumBytesPerElement);
		}

		INT GetAllocatedSize(INT NumAllocatedElements, INT NumBytesPerElement) const
		{
			return SecondaryData.GetAllocatedSize(NumAllocatedElements, NumBytesPerElement);
		}

	private:

		/** The data is stored in this array if less than NumInlineBytes is needed. */
		TTypeCompatibleBytes<ElementType> InlineData[NumInlineElements];

		/** The data is allocated through the indirect allocation policy if more than NumInlineBytes is needed. */
		typename SecondaryAllocator::template ForElementType<ElementType> SecondaryData;

		/** @return the base of the aligned inline element data */
		ElementType* GetInlineElements() const
		{
			return (ElementType*)InlineData;
		}
	};

	typedef ForElementType<FScriptContainerElement> ForAnyElementType;
};

/** Bit arrays use a 4 DWORD inline allocation policy by default. */
class FDefaultBitArrayAllocator : public TInlineAllocator<4>
{
};

enum { NumBitsPerDWORD = 32 };
enum { NumBitsPerDWORDLogTwo = 5 };

//
// Sparse array allocation definitions
//

/** Encapsulates the allocators used by a sparse array in a single type. */
template<typename InElementAllocator = FDefaultAllocator,typename InBitArrayAllocator = FDefaultBitArrayAllocator>
class TSparseArrayAllocator
{
public:

	typedef InElementAllocator ElementAllocator;
	typedef InBitArrayAllocator BitArrayAllocator;
};

/** An inline sparse array allocator that allows sizing of the inline allocations for a set number of elements. */
template<
	UINT NumInlineElements,
	typename SecondaryAllocator = TSparseArrayAllocator<FDefaultAllocator,FDefaultAllocator>
	>
class TInlineSparseArrayAllocator
{
private:

	/** The size to allocate inline for the bit array. */
	enum { InlineBitArrayDWORDs = (NumInlineElements + NumBitsPerDWORD - 1) / NumBitsPerDWORD};

public:

	typedef TInlineAllocator<NumInlineElements,typename SecondaryAllocator::ElementAllocator>		ElementAllocator;
	typedef TInlineAllocator<InlineBitArrayDWORDs,typename SecondaryAllocator::BitArrayAllocator>	BitArrayAllocator;
};

class FDefaultSparseArrayAllocator : public TSparseArrayAllocator<>
{
};

//
// Set allocation definitions.
//

#define DEFAULT_NUMBER_OF_ELEMENTS_PER_HASH_BUCKET	2
#define DEFAULT_BASE_NUMBER_OF_HASH_BUCKETS			8
#define DEFAULT_MIN_NUMBER_OF_HASHED_ELEMENTS		4

/** Encapsulates the allocators used by a set in a single type. */
template<
	typename InSparseArrayAllocator = TSparseArrayAllocator<>,
	typename InHashAllocator = TInlineAllocator<1,FDefaultAllocator>,
	UINT AverageNumberOfElementsPerHashBucket = DEFAULT_NUMBER_OF_ELEMENTS_PER_HASH_BUCKET,
	UINT BaseNumberOfHashBuckets = DEFAULT_BASE_NUMBER_OF_HASH_BUCKETS,
	UINT MinNumberOfHashedElements = DEFAULT_MIN_NUMBER_OF_HASHED_ELEMENTS
	>
class TSetAllocator
{
public:

	/** Computes the number of hash buckets to use for a given number of elements. */
	static FORCEINLINE UINT GetNumberOfHashBuckets(UINT NumHashedElements)
	{
		if(NumHashedElements >= MinNumberOfHashedElements)
		{
			return appRoundUpToPowerOfTwo(NumHashedElements / AverageNumberOfElementsPerHashBucket + BaseNumberOfHashBuckets);
		}
		else
		{
			return 1;
		}
	}

	typedef InSparseArrayAllocator SparseArrayAllocator;
	typedef InHashAllocator HashAllocator;
};

/** An inline set allocator that allows sizing of the inline allocations for a set number of elements. */
template<
	UINT NumInlineElements,
	typename SecondaryAllocator = TSetAllocator<TSparseArrayAllocator<FDefaultAllocator,FDefaultAllocator>,FDefaultAllocator>,
	UINT AverageNumberOfElementsPerHashBucket = DEFAULT_NUMBER_OF_ELEMENTS_PER_HASH_BUCKET,
	UINT MinNumberOfHashedElements = DEFAULT_MIN_NUMBER_OF_HASHED_ELEMENTS
	>
class TInlineSetAllocator
{
private:

	enum { NumInlineHashBuckets = (NumInlineElements + AverageNumberOfElementsPerHashBucket - 1) / AverageNumberOfElementsPerHashBucket };

public:

	/** Computes the number of hash buckets to use for a given number of elements. */
	static FORCEINLINE UINT GetNumberOfHashBuckets(UINT NumHashedElements)
	{
		const UINT NumDesiredHashBuckets = appRoundUpToPowerOfTwo(NumHashedElements / AverageNumberOfElementsPerHashBucket);
		if(NumDesiredHashBuckets < NumInlineHashBuckets)
        {
	        return NumInlineHashBuckets;
		}
		else if(NumHashedElements < MinNumberOfHashedElements)
		{
			return NumInlineHashBuckets;
		}
		else
		{
			return NumDesiredHashBuckets;
		}
	}

	typedef TInlineSparseArrayAllocator<NumInlineElements,typename SecondaryAllocator::SparseArrayAllocator> SparseArrayAllocator;
	typedef TInlineAllocator<NumInlineHashBuckets,typename SecondaryAllocator::HashAllocator> HashAllocator;
};

class FDefaultSetAllocator : public TSetAllocator<>
{
};

#endif
