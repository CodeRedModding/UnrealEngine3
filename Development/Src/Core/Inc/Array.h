/*=============================================================================
	Array.h: Dynamic array definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ARRAY_H__
#define __ARRAY_H__

/**
 * Generic non-const iterator which can operate on types that expose the following:
 * - A type called ElementType representing the contained type.
 * - A method IndexType Num() const that returns the number of items in the container.
 * - A method UBOOL IsValidIndex(IndexType index) which returns whether a given index is valid in the container.
 * - A method T& operator(IndexType index) which returns a reference to a contained object by index.
 */
template< typename ContainerType, typename IndexType = INT >
class TIndexedContainerIterator
{
public:
	typedef typename ContainerType::ElementType ElementType;

	TIndexedContainerIterator(ContainerType& InContainer)
		:	Container( InContainer )
		,	Index(0)
	{
	}

	TIndexedContainerIterator(const TIndexedContainerIterator& Other)
		:	Container( Other.Container )
		,	Index( Other.Index )
	{
	}

	/** Advances iterator to the next element in the container. */
	TIndexedContainerIterator& operator++()
	{
		++Index;
		return *this;
	}
	TIndexedContainerIterator operator++(int)
	{
		TIndexedContainerIterator Tmp(*this);
		++Index;
		return Tmp;
	}

	/** Moves iterator to the previous element in the container. */
	TIndexedContainerIterator& operator--()
	{
		--Index;
		return *this;
	}
	TIndexedContainerIterator operator--(int)
	{
		TIndexedContainerIterator Tmp(*this);
		--Index;
		return Tmp;
	}

	/** pointer arithmetic support */
	TIndexedContainerIterator& operator+=(INT Offset)
	{
		Index += Offset;
		return *this;
	}

	TIndexedContainerIterator operator+(INT Offset) const
	{
		TIndexedContainerIterator Tmp(*this);
		return Tmp += Offset;
	}

	TIndexedContainerIterator& operator-=(INT Offset)
	{
		return *this += -Offset;
	}

	TIndexedContainerIterator operator-(INT Offset) const
	{
		TIndexedContainerIterator Tmp(*this);
		return Tmp -= Offset;
	}

	TIndexedContainerIterator operator()(INT Offset) const
	{
		return *this + Offset;
	}

	/** @name Element access */
	//@{
	ElementType& operator* () const
	{
		return Container( Index );
	}

	ElementType* operator-> () const
	{
		return &Container( Index );
	}
	//@}

	/** conversion to "bool" returning TRUE if the iterator has not reached the last element. */
	typedef bool PrivateBooleanType;
	operator PrivateBooleanType() const
	{
		return Container.IsValidIndex(Index) ? &TIndexedContainerIterator::Index : NULL;
	}

	/** inverse of the "bool" operator */
	bool operator !() const
	{
		return !operator PrivateBooleanType();
	}

	/** Returns an index to the current element. */
	IndexType GetIndex() const
	{
		return Index;
	}

	/** Resets the iterator to the first element. */
	void Reset()
	{
		Index = 0;
	}
private:
	ContainerType&	Container;
	IndexType		Index;
};

/** operator + */
template< typename ContainerType, typename IndexType >
TIndexedContainerIterator<ContainerType, IndexType> operator+(
	INT Offset,
	TIndexedContainerIterator<ContainerType, IndexType> RHS)
{
	return RHS + Offset;
}



/**
* Generic const iterator which can operate on types that expose the following:
* - A type called ElementType representing the contained type.
* - A method IndexType Num() const that returns the number of items in the container.
* - A method UBOOL IsValidIndex(IndexType index) which returns whether a given index is valid in the container.
* - A method T& operator(IndexType index) const which returns a reference to a contained object by index.
*/
template< typename ContainerType, typename IndexType = INT >
class TIndexedContainerConstIterator
{
public:
	typedef typename ContainerType::ElementType ElementType;

	TIndexedContainerConstIterator(const ContainerType& InContainer)
		:	Container( InContainer )
		,	Index(0)
	{
	}

	TIndexedContainerConstIterator(const TIndexedContainerConstIterator& Other)
		:	Container( Other.Container )
		,	Index( Other.Index )
	{
	}

	/** Advances iterator to the next element in the container. */
	TIndexedContainerConstIterator& operator++()
	{
		++Index;
		return *this;
	}
	TIndexedContainerConstIterator operator++(int)
	{
		TIndexedContainerConstIterator Tmp(*this);
		++Index;
		return Tmp;
	}

	/** Moves iterator to the previous element in the container. */
	TIndexedContainerConstIterator& operator--()
	{
		--Index;
		return *this;
	}
	TIndexedContainerConstIterator operator--(int)
	{
		TIndexedContainerConstIterator Tmp(*this);
		--Index;
		return Tmp;
	}

	/** iterator arithmetic support */
	TIndexedContainerConstIterator& operator+=(INT Offset)
	{
		Index += Offset;
		return *this;
	}

	TIndexedContainerConstIterator operator+(INT Offset) const
	{
		TIndexedContainerConstIterator Tmp(*this);
		return Tmp += Offset;
	}

	TIndexedContainerConstIterator& operator-=(INT Offset)
	{
		return *this += -Offset;
	}

	TIndexedContainerConstIterator operator-(INT Offset) const
	{
		TIndexedContainerConstIterator Tmp(*this);
		return Tmp -= Offset;
	}

	TIndexedContainerConstIterator operator()(INT Offset) const
	{
		return *this + Offset;
	}

	/** @name Element access */
	//@{
	const ElementType& operator* () const
	{
		return Container( Index );
	}

	const ElementType* operator-> () const
	{
		return &Container( Index );
	}
	//@}

	/** conversion to "bool" returning TRUE if the iterator has not reached the last element. */
	typedef bool PrivateBooleanType;
	operator PrivateBooleanType() const
	{
		return Container.IsValidIndex(Index) ? &TIndexedContainerConstIterator::Index : NULL;
	}

	/** inverse of the "bool" operator */
	bool operator !() const
	{
		return !operator PrivateBooleanType();
	}

	/** Returns an index to the current element. */
	IndexType GetIndex() const
	{
		return Index;
	}

	/** Resets the iterator to the first element. */
	void Reset()
	{
		Index = 0;
	}
private:
	const ContainerType&	Container;
	IndexType				Index;
};

/** operator + */
template< typename ContainerType, typename IndexType >
TIndexedContainerConstIterator<ContainerType, IndexType> operator+(
	INT Offset,
	TIndexedContainerConstIterator<ContainerType, IndexType> RHS)
{
	return RHS + Offset;
}

/**
 * Base dynamic array.
 * An untyped data array; mirrors a TArray's members, but doesn't need an exact C++ type for its elements.
 **/
class FScriptArray : protected FHeapAllocator::ForAnyElementType
{
public:
	void* GetData()
	{
		return this->GetAllocation();
	}
	const void* GetData() const
	{
		return this->GetAllocation();
	}
	UBOOL IsValidIndex( INT i ) const
	{
		return i>=0 && i<ArrayNum;
	}
	FORCEINLINE INT Num() const
	{
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);
		return ArrayNum;
	}
	void InsertZeroed( INT Index, INT Count, INT NumBytesPerElement )
	{
		Insert( Index, Count, NumBytesPerElement );
		appMemzero( (BYTE*)this->GetAllocation()+Index*NumBytesPerElement, Count*NumBytesPerElement );
	}
	void Insert( INT Index, INT Count, INT NumBytesPerElement )
	{
		check(Count>=0);
		check(ArrayNum>=0);
		check(ArrayMax>=ArrayNum);
		check(Index>=0);
		check(Index<=ArrayNum);

		const INT OldNum = ArrayNum;
		if( (ArrayNum+=Count)>ArrayMax )
		{
			ArrayMax = this->CalculateSlack( ArrayNum, ArrayMax, NumBytesPerElement );
			this->ResizeAllocation(OldNum,ArrayMax,NumBytesPerElement);
		}
		appMemmove
		(
			(BYTE*)this->GetAllocation() + (Index+Count )*NumBytesPerElement,
			(BYTE*)this->GetAllocation() + (Index       )*NumBytesPerElement,
			                                               (OldNum-Index)*NumBytesPerElement
		);
	}
	INT Add( INT Count, INT NumBytesPerElement )
	{
		check(Count>=0);
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);

		const INT OldNum = ArrayNum;
		if( (ArrayNum+=Count)>ArrayMax )
		{
			ArrayMax = this->CalculateSlack( ArrayNum, ArrayMax, NumBytesPerElement );
			this->ResizeAllocation(OldNum,ArrayMax,NumBytesPerElement);
		}

		return OldNum;
	}
	INT AddZeroed( INT Count, INT NumBytesPerElement )
	{
		const INT Index = Add( Count, NumBytesPerElement );
		appMemzero( (BYTE*)this->GetAllocation()+Index*NumBytesPerElement, Count*NumBytesPerElement );
		return Index;
	}
	void Shrink( INT NumBytesPerElement )
	{
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);
		if( ArrayMax != ArrayNum )
		{
			ArrayMax = ArrayNum;
			this->ResizeAllocation(ArrayNum,ArrayMax,NumBytesPerElement);
		}
	}
	void Empty( INT Slack, INT NumBytesPerElement )
	{
		checkSlow(Slack>=0);
		ArrayNum = 0;
		// only reallocate if we need to, I don't trust realloc to the same size to work
		if (ArrayMax != Slack)
		{
			ArrayMax = Slack;
			this->ResizeAllocation(0,ArrayMax,NumBytesPerElement);
		}
	}
	void Swap(INT A, INT B, INT NumBytesPerElement )
	{
		appMemswap(
			(BYTE*)this->GetAllocation()+(NumBytesPerElement*A),
			(BYTE*)this->GetAllocation()+(NumBytesPerElement*B),
			NumBytesPerElement
			);
	}
	FScriptArray()
	:   ArrayNum( 0 )
	,	ArrayMax( 0 )

	{}
	FScriptArray( ENoInit )
	:	FHeapAllocator::ForAnyElementType( E_NoInit )
	{}
	~FScriptArray()
	{
		ArrayNum = ArrayMax = 0;
	}
	void CountBytes( FArchive& Ar, INT NumBytesPerElement  )
	{
		Ar.CountBytes( ArrayNum*NumBytesPerElement, ArrayMax*NumBytesPerElement );
	}
	/**
	 * Returns the amount of slack in this array in elements.
	 */
	INT GetSlack() const
	{
		return ArrayMax - ArrayNum;
	}
		
	void Remove( INT Index, INT Count, INT NumBytesPerElement  )
	{
		checkSlow(Count >= 0);
		checkSlow(Index >= 0); 
		checkSlow(Index <= ArrayNum);
		checkSlow(Index + Count <= ArrayNum);

		// Skip memmove in the common case that there is nothing to move.
		INT NumToMove = ArrayNum - Index - Count;
		if( NumToMove )
		{
			appMemmove
			(
				(BYTE*)this->GetAllocation() + (Index      ) * NumBytesPerElement,
				(BYTE*)this->GetAllocation() + (Index+Count) * NumBytesPerElement,
				NumToMove * NumBytesPerElement
			);
		}
		ArrayNum -= Count;
		
		const INT NewArrayMax = this->CalculateSlack(ArrayNum,ArrayMax,NumBytesPerElement);
		if(NewArrayMax != ArrayMax)
		{
			ArrayMax = NewArrayMax;
			this->ResizeAllocation(ArrayNum,ArrayMax,NumBytesPerElement);
		}
		checkSlow(ArrayNum >= 0);
		checkSlow(ArrayMax >= ArrayNum);
	}

protected:

	FScriptArray( INT InNum, INT NumBytesPerElement  )
	:   ArrayNum( InNum )
	,	ArrayMax( InNum )

	{
		this->ResizeAllocation(0,ArrayMax,NumBytesPerElement);
	}
	INT	  ArrayNum;
	INT	  ArrayMax;
};


/**
 * Templated dynamic array
 *
 * A dynamically sized array of typed elements.  Makes the assumption that your elements are relocate-able; 
 * i.e. that they can be transparently moved to new memory without a copy constructor.  The main implication 
 * is that pointers to elements in the TArray may be invalidated by adding or removing other elements to the array. 
 * Removal of elements is O(N) and invalidates the indices of subsequent elements.
 *
 * Caution: as noted below some methods are not safe for element types that require constructors.
 *
 **/
template<typename InElementType, typename Allocator>
class TArray
{
public:
	typedef InElementType ElementType;

	TArray()
	:   ArrayNum( 0 )
	,	ArrayMax( 0 )
	{}
	TArray( ENoInit )
	:	AllocatorInstance(E_NoInit)
	{}
	/** Caution, this will create elements without calling the constructor and this is not appropriate for element types that require a constructor to function properly. */
	explicit TArray( INT InNum )
	:   ArrayNum( InNum )
	,	ArrayMax( InNum )
	{
		AllocatorInstance.ResizeAllocation(0,ArrayMax,sizeof(ElementType));
	}

	/**
	 * Copy constructor. Use the common routine to perform the copy
	 *
	 * @param Other the source array to copy
	 */
	template<typename OtherAllocator>
	TArray(const TArray<ElementType,OtherAllocator>& Other)
	:   ArrayNum( 0 )
	,	ArrayMax( 0 )
	{
		Copy(Other);
	}

	TArray(const TArray<ElementType,Allocator>& Other)
	:   ArrayNum( 0 )
	,	ArrayMax( 0 )
	{
		Copy(Other);
	}

	~TArray()
	{
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);
		DestructItems(0,ArrayNum);
		ArrayNum = ArrayMax = 0;

		#if defined(_MSC_VER)
			// ensure that DebugGet gets instantiated.
			//@todo it would be nice if we had a cleaner solution for DebugGet
			volatile const ElementType* Dummy = &DebugGet(0);
		#endif
	}

	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @return pointer to first array entry or NULL if ArrayMax==0
	 */
	FORCEINLINE ElementType* GetTypedData()
	{
		return (ElementType*)AllocatorInstance.GetAllocation();
	}
	FORCEINLINE ElementType* GetData()
	{
		return (ElementType*)AllocatorInstance.GetAllocation();
	}
	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @return pointer to first array entry or NULL if ArrayMax==0
	 */
	FORCEINLINE const ElementType* GetTypedData() const
	{
		return (const ElementType*)AllocatorInstance.GetAllocation();
	}
	FORCEINLINE const ElementType* GetData() const
	{
		return (const ElementType*)AllocatorInstance.GetAllocation();
	}
	/** 
	 * Helper function returning the size of the inner type
	 *
	 * @return size in bytes of array type
	 */
	FORCEINLINE DWORD GetTypeSize() const
	{
		return sizeof(ElementType);
	}

	/** 
	 * Helper function to return the amount of memory allocated by this container 
	 *
	 * @return number of bytes allocated by this container
	 */
	FORCEINLINE DWORD GetAllocatedSize( void ) const
	{
		return AllocatorInstance.GetAllocatedSize(ArrayMax, sizeof(ElementType));
	}

	/**
	 * Returns the amount of slack in this array in elements.
	 */
	INT GetSlack() const
	{
		return ArrayMax - ArrayNum;
	}

	FORCEINLINE UBOOL IsValidIndex( INT i ) const
	{
		return i>=0 && i<ArrayNum;
	}
	FORCEINLINE INT Num() const
	{
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);
		return ArrayNum;
	}

	FORCEINLINE ElementType& operator()( INT i )
	{
		checkSlowish(i>=0 && (i<ArrayNum||(i==0 && ArrayNum==0)) ); // (i==0 && ArrayNum==0) is workaround for &MyArray(0) abuse
		checkSlow(ArrayMax>=ArrayNum);
		return GetTypedData()[i];
	}
	FORCEINLINE const ElementType& operator()( INT i ) const
	{
		checkSlowish(i>=0 && (i<ArrayNum||(i==0 && ArrayNum==0)) ); // (i==0 && ArrayNum==0) is workaround for &MyArray(0) abuse
		checkSlow(ArrayMax>=ArrayNum);
		return GetTypedData()[i];
	}
	ElementType Pop()
	{
		check(ArrayNum>0);
		checkSlow(ArrayMax>=ArrayNum);
		ElementType Result = GetTypedData()[ArrayNum-1];
		Remove( ArrayNum-1 );
		return Result;
	}
	void Push( const ElementType& Item )
	{
		AddItem(Item);
	}
	ElementType& Top()
	{
		return Last();
	}
	const ElementType& Top() const
	{
		return Last();
	}
	ElementType& Last( INT c=0 )
	{
		check(AllocatorInstance.GetAllocation());
		check(c<ArrayNum);
		checkSlow(ArrayMax>=ArrayNum);
		return GetTypedData()[ArrayNum-c-1];
	}
	const ElementType& Last( INT c=0 ) const
	{
		check(GetTypedData());
		checkSlow(c<ArrayNum);
		checkSlow(ArrayMax>=ArrayNum);
		return GetTypedData()[ArrayNum-c-1];
	}
	void Shrink()
	{
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);
		if( ArrayMax != ArrayNum )
		{
			ArrayMax = ArrayNum;
			AllocatorInstance.ResizeAllocation(ArrayNum,ArrayMax,sizeof(ElementType));
		}
	}
	UBOOL FindItem( const ElementType& Item, INT& Index ) const
	{
		const ElementType* const RESTRICT DataEnd = GetTypedData() + ArrayNum;
		for(const ElementType* RESTRICT Data = GetTypedData();
			Data < DataEnd;
			Data++
			)
		{
			if( *Data==Item )
			{
				Index = (INT)(Data - GetTypedData());
				return TRUE;
			}
		}
		return FALSE;
	}
	INT FindItemIndex( const ElementType& Item ) const
	{
		const ElementType* const RESTRICT DataEnd = GetTypedData() + ArrayNum;
		for(const ElementType* RESTRICT Data = GetTypedData();
			Data < DataEnd;
			Data++
			)
		{
			if( *Data==Item )
			{
				return (INT)(Data - GetTypedData());
			}
		}
		return INDEX_NONE;
	}
	/**
	 * Finds an item by key (assuming the ElementType overloads operator== for the comparison).
	 * @param Key	The key to search by
	 * @return		Index to the first matching element, or INDEX_NONE if none is found
	 */
	template <typename KeyType>
	INT FindItemIndexByKey( const KeyType& Key ) const
	{
		const ElementType* const RESTRICT DataEnd = GetTypedData() + ArrayNum;
		for(const ElementType* RESTRICT Data = GetTypedData();
			Data < DataEnd;
			Data++
			)
		{
			if( *Data==Key )
			{
				return (INT)(Data - GetTypedData());
			}
		}
		return INDEX_NONE;
	}
	/**
	 * Finds an item by key (assuming the ElementType overloads operator== for the comparison).
	 * @param Key	The key to search by
	 * @return		Pointer to the first matching element, or NULL if none is found
	 */
	template <typename KeyType>
	const ElementType* FindItemByKey( const KeyType& Key ) const
	{
		const ElementType* const RESTRICT DataEnd = GetTypedData() + ArrayNum;
		for(const ElementType* RESTRICT Data = GetTypedData();
			Data < DataEnd;
			Data++
			)
		{
			if( *Data==Key )
			{
				return Data;
			}
		}
		return NULL;
	}
	/**
	 * Finds an item by key (assuming the ElementType overloads operator== for the comparison).
	 * @param Key	The key to search by
	 * @return		Pointer to the first matching element, or NULL if none is found
	 */
	template <typename KeyType>
	ElementType* FindItemByKey( const KeyType& Key )
	{
		ElementType* const RESTRICT DataEnd = GetTypedData() + ArrayNum;
		for(ElementType* RESTRICT Data = GetTypedData();
			Data < DataEnd;
			Data++
			)
		{
			if( *Data==Key )
			{
				return Data;
			}
		}
		return NULL;
	}
	UBOOL ContainsItem( const ElementType& Item ) const
	{
		return ( FindItemIndex(Item) != INDEX_NONE );
	}
	UBOOL operator==(const TArray& OtherArray) const
	{
		if(Num() != OtherArray.Num())
			return FALSE;
		for(INT Index = 0;Index < Num();Index++)
		{
			if(!((*this)(Index) == OtherArray(Index)))
				return FALSE;
		}
		return TRUE;
	}
	UBOOL operator!=(const TArray& OtherArray) const
	{
		if(Num() != OtherArray.Num())
			return TRUE;
		for(INT Index = 0;Index < Num();Index++)
		{
			if(!((*this)(Index) == OtherArray(Index)))
				return TRUE;
		}
		return FALSE;
	}
	friend FArchive& operator<<( FArchive& Ar, TArray& A )
	{
		A.CountBytes( Ar );
		if( sizeof(ElementType)==1 )
		{
			// Serialize simple bytes which require no construction or destruction.
			Ar << A.ArrayNum;
			check( A.ArrayNum >= 0 );
			if( Ar.IsLoading() )
			{
				A.ArrayMax = A.ArrayNum;
				A.AllocatorInstance.ResizeAllocation(0,A.ArrayMax,sizeof(ElementType));
			}
			Ar.Serialize( A.GetData(), A.Num() );
		}
		else if( Ar.IsLoading() )
		{
			// Load array.
			INT NewNum;
			Ar << NewNum;
			A.Empty( NewNum );
			for( INT i=0; i<NewNum; i++ )
			{
				Ar << *::new(A)ElementType;
			}
		}
		else
		{
			// Save array.
			Ar << A.ArrayNum;
			for( INT i=0; i<A.ArrayNum; i++ )
			{
				Ar << A( i );
			}
		}
		return Ar;
	}
	/**
	 * Bulk serialize array as a single memory blob when loading. Uses regular serialization code for saving
	 * and doesn't serialize at all otherwise (e.g. transient, garbage collection, ...).
	 * 
	 * Requirements:
	 *   - T's << operator needs to serialize ALL member variables in the SAME order they are layed out in memory.
	 *   - T's << operator can NOT perform any fixup operations. This limitation can be lifted by manually copying
	 *     the code after the BulkSerialize call.
	 *   - T can NOT contain any member variables requiring constructor calls or pointers
	 *   - sizeof(ElementType) must be equal to the sum of sizes of it's member variables.
	 *        - e.g. use pragma pack (push,1)/ (pop) to ensure alignment
	 *        - match up BYTE/ WORDs so everything always end up being properly aligned
	 *   - Code can not rely on serialization of T if neither ArIsLoading nor ArIsSaving is TRUE.
	 *   - Can only be called platforms that either have the same endianness as the one the content was saved with
	 *     or had the endian conversion occur in a cooking process like e.g. for consoles.
	 *
	 * Notes:
	 *   - it is safe to call BulkSerialize on TTransArrays
	 *
	 * IMPORTANT:
	 *   - This is Overridden in XeD3dResourceArray.h  Please make certain changes are propogated accordingly
	 *
	 * @param Ar	FArchive to bulk serialize this TArray to/from
	 */
	void BulkSerialize(FArchive& Ar,INT ElementSize = sizeof(ElementType))
	{
		// Serialize element size to detect mismatch across platforms.
		INT SerializedElementSize = 0;
		SerializedElementSize = ElementSize;
		Ar << SerializedElementSize;
#if !__INTEL_BYTE_ORDER__ && !CONSOLE
		// We need to handle endian conversion of content as all non cooked content will always be in 
		// Intel endian format. Consoles have their data converted to the appropriate endianness on
		// cooking which is why we don't have to hit this codepath for the console case.
		Ar << *this;
#else
		if( Ar.IsSaving() || Ar.Ver() < GPackageFileVersion || Ar.LicenseeVer() < GPackageFileLicenseeVersion )
		{
			// Use regular endian clean saving to correctly handle content cooking.
			// @todo: in theory we only need to do this if ArForceByteSwapping though for now it also 
			// @todo: serves as a good sanity check to ensure that bulk serialization matches up with 
			// @todo: regular serialization.
			Ar << *this;
		}
		else 
		{
			CountBytes(Ar);
			if( Ar.IsLoading() )
			{
				// Basic sanity checking to ensure that sizes match.
				checkf(SerializedElementSize==0 || SerializedElementSize==ElementSize,TEXT("Expected %i, Got: %i"),ElementSize,SerializedElementSize);
				// Serialize the number of elements, block allocate the right amount of memory and deserialize
				// the data as a giant memory blob in a single call to Serialize. Please see the function header
				// for detailed documentation on limitations and implications.
				INT NewArrayNum;
				Ar << NewArrayNum;
				Empty( NewArrayNum );
				Add( NewArrayNum );
				Ar.Serialize( GetData(), NewArrayNum * SerializedElementSize );
			}
		}
#endif
	}
	void CountBytes( FArchive& Ar )
	{
		Ar.CountBytes( ArrayNum*sizeof(ElementType), ArrayMax*sizeof(ElementType) );
	}

	// Add, Insert, Remove, Empty interface.
	/** Caution, Add() will create elements without calling the constructor and this is not appropriate for element types that require a constructor to function properly. */
	INT Add( INT Count=1 )
	{
		check(Count>=0);
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);

		const INT OldNum = ArrayNum;
		if( (ArrayNum+=Count)>ArrayMax )
		{
			ArrayMax = AllocatorInstance.CalculateSlack( ArrayNum, ArrayMax, sizeof(ElementType) );
			AllocatorInstance.ResizeAllocation(OldNum,ArrayMax, sizeof(ElementType));
		}

		return OldNum;
	}
	/** Caution, Insert() will create elements without calling the constructor and this is not appropriate for element types that require a constructor to function properly. */
	void Insert( INT Index, INT Count=1 )
	{
		check(Count>=0);
		check(ArrayNum>=0);
		check(ArrayMax>=ArrayNum);
		check(Index>=0);
		check(Index<=ArrayNum);

		const INT OldNum = ArrayNum;
		if( (ArrayNum+=Count)>ArrayMax )
		{
			ArrayMax = AllocatorInstance.CalculateSlack( ArrayNum, ArrayMax, sizeof(ElementType) );
			AllocatorInstance.ResizeAllocation(OldNum,ArrayMax,sizeof(ElementType));
		}
		appMemmove
		(
			(BYTE*)AllocatorInstance.GetAllocation() + (Index+Count )*sizeof(ElementType),
			(BYTE*)AllocatorInstance.GetAllocation() + (Index       )*sizeof(ElementType),
			                                               (OldNum-Index)*sizeof(ElementType)
		);
	}
	/** Caution, InsertZeroed() will create elements without calling the constructor and this is not appropriate for element types that require a constructor to function properly. */
	void InsertZeroed( INT Index, INT Count=1 )
	{
		Insert( Index, Count );
		appMemzero( (BYTE*)AllocatorInstance.GetAllocation()+Index*sizeof(ElementType), Count*sizeof(ElementType) );
	}
	INT InsertItem( const ElementType& Item, INT Index )
	{
		// construct a copy in place at Index (this new operator will insert at 
		// Index, then construct that memory with Item)
		Insert(Index,1);
		new(GetTypedData() + Index) ElementType(Item);
		return Index;
	}
	void Remove( INT Index, INT Count=1 )
	{
		checkSlow(Count >= 0);
		check(Index>=0);
		check(Index<=ArrayNum);
		check(Index+Count<=ArrayNum);

		DestructItems(Index,Count);

		// Skip memmove in the common case that there is nothing to move.
		INT NumToMove = ArrayNum - Index - Count;
		if( NumToMove )
		{
			appMemmove
			(
				(BYTE*)AllocatorInstance.GetAllocation() + (Index      ) * sizeof(ElementType),
				(BYTE*)AllocatorInstance.GetAllocation() + (Index+Count) * sizeof(ElementType),
				NumToMove * sizeof(ElementType)
			);
		}
		ArrayNum -= Count;
		
		const INT NewArrayMax = AllocatorInstance.CalculateSlack(ArrayNum,ArrayMax,sizeof(ElementType));
		if(NewArrayMax != ArrayMax)
		{
			ArrayMax = NewArrayMax;
			AllocatorInstance.ResizeAllocation(ArrayNum,ArrayMax,sizeof(ElementType));
		}
		checkSlow(ArrayNum >= 0);
		checkSlow(ArrayMax >= ArrayNum);
	}
	// RemoveSwap, this version is much more efficient O(Count) instead of O(ArrayNum), but does not preserve the order
	void RemoveSwap( INT Index, INT Count=1 )
	{
		check(Index>=0);
		check(Index<=ArrayNum);
		check(Index+Count<=ArrayNum);

		DestructItems(Index,Count);
		
		checkSlow(Count >= 0);
		checkSlow(Index >= 0); 
		checkSlow(Index <= ArrayNum);
		checkSlow(Index + Count <= ArrayNum);

		// Replace the elements in the hole created by the removal with elements from the end of the array, so the range of indices used by the array is contiguous.
		const INT NumElementsInHole = Count;
		const INT NumElementsAfterHole = ArrayNum - (Index + Count);
		const INT NumElementsToMoveIntoHole = Min(NumElementsInHole,NumElementsAfterHole);
		if(NumElementsToMoveIntoHole)
		{
			appMemcpy(
				(BYTE*)AllocatorInstance.GetAllocation() + (Index                             ) * sizeof(ElementType),
				(BYTE*)AllocatorInstance.GetAllocation() + (ArrayNum-NumElementsToMoveIntoHole) * sizeof(ElementType),
				NumElementsToMoveIntoHole * sizeof(ElementType)
				);
		}
		ArrayNum -= Count;

		const INT NewArrayMax = AllocatorInstance.CalculateSlack(ArrayNum,ArrayMax,sizeof(ElementType));
		if(NewArrayMax != ArrayMax)
		{
			ArrayMax = NewArrayMax;
			AllocatorInstance.ResizeAllocation(ArrayNum,ArrayMax,sizeof(ElementType));
		}
		checkSlow(ArrayNum >= 0);
		checkSlow(ArrayMax >= ArrayNum);
	}
	void Empty( INT Slack=0 )
	{
		DestructItems(0,ArrayNum);

		checkSlow(Slack>=0);
		ArrayNum = 0;
		// only reallocate if we need to, I don't trust realloc to the same size to work
		if (ArrayMax != Slack)
		{
			ArrayMax = Slack;
			AllocatorInstance.ResizeAllocation(0,ArrayMax,sizeof(ElementType));
		}
	}
	void SetNum(INT NewNum)
	{
		if (NewNum > Num())
		{
			Add(NewNum-Num());
		}
		else if (NewNum < Num())
		{
			Remove(NewNum, Num() - NewNum);
		}
	}

	/**
	 * Appends the specified array to this array.
	 * Cannot append to self.
	 */
	FORCEINLINE void Append(const TArray& Source)
	{
		// Do nothing if the source and target match, or the source is empty.
		if ( this != &Source && Source.Num() > 0 )
		{
			// Allocate memory for the new elements.
			Reserve( ArrayNum + Source.ArrayNum );

			if ( TContainerTraits<ElementType>::NeedsConstructor )
			{
				// Construct each element.
				for ( INT Index = 0 ; Index < Source.ArrayNum ; ++Index )
				{
					new(GetTypedData() + ArrayNum + Index) ElementType(Source(Index));
				}
			}
			else
			{
				// Do a bulk copy.
				appMemcpy( (BYTE*)AllocatorInstance.GetAllocation() + ArrayNum * sizeof(ElementType), Source.AllocatorInstance.GetAllocation(), sizeof(ElementType) * Source.ArrayNum );
			}
			ArrayNum += Source.ArrayNum;
		}
	}

	/**
	 * Appends the specified array to this array.
	 * Cannot append to self.
	 */
	TArray& operator+=( const TArray& Other )
	{
		Append( Other );
		return *this;
	}

	/**
	 * Copies the source array into this one. Uses the common copy method
	 *
	 * @param Other the source array to copy
	 */
	template<typename OtherAllocator>
	TArray& operator=( const TArray<ElementType,OtherAllocator>& Other )
	{
		Copy(Other);
		return *this;
	}

	TArray& operator=( const TArray<ElementType,Allocator>& Other )
	{
		Copy(Other);
		return *this;
	}

	/**
	 * Adds a new item to the end of the array, possibly reallocating the whole array to fit.
	 *
	 * @param Item	The item to add
	 * @return		Index to the new item
	 */
	INT AddItem( const ElementType& Item )
	{
		const INT Index = Add(1);
		new(GetTypedData() + Index) ElementType(Item);
		return Index;
	}
	/** Caution, AddZeroed() will create elements without calling the constructor and this is not appropriate for element types that require a constructor to function properly. */
	INT AddZeroed( INT Count=1 )
	{
		const INT Index = Add( Count );
		appMemzero( (BYTE*)AllocatorInstance.GetAllocation()+Index*sizeof(ElementType), Count*sizeof(ElementType) );
		return Index;
	}
	INT AddUniqueItem( const ElementType& Item )
	{
		for( INT Index=0; Index<ArrayNum; Index++ )
			if( (*this)(Index)==Item )
				return Index;
		return AddItem( Item );
	}

	/**
	 * Reserves memory such that the array can contain at least Number elements.
	 */
	void Reserve(INT Number)
	{
		if (Number > ArrayMax)
		{
			ArrayMax = Number;
			AllocatorInstance.ResizeAllocation(ArrayNum,ArrayMax,sizeof(ElementType));
		}
	}
	
	/** Sets the size of the array. */
	void Init(INT Number)
	{
		Empty(Number);
		Add(Number);
	}

	/** Sets the size of the array, filling it with the given element. */
	void Init(const ElementType& Element,INT Number)
	{
		Empty(Number);
		for(INT Index = 0;Index < Number;++Index)
		{
			new(*this) ElementType(Element);
		}
	}

	/**
	 * Removes the first occurrence of the specified item in the array, maintaining order but not indices.
	 *
	 * @param	Item	The item to remove
	 *
	 * @return	The number of items removed.  For RemoveSingleItem, this is always either 0 or 1.
	 */
	INT RemoveSingleItem( const ElementType& Item )
	{
		// It isn't valid to specify an Item that is in the array, since removing that item will change Item's value.
		check( ((&Item) < GetTypedData()) || ((&Item) >= GetTypedData()+ArrayMax) );

		for( INT Index=0; Index<ArrayNum; Index++ )
		{
			if( GetTypedData()[Index] == Item )
			{
				// Destruct items that match the specified Item.
				DestructItems(Index,1);
				const INT NextIndex = Index + 1;
				if( NextIndex < ArrayNum )
				{
					const INT NumElementsToMove = ArrayNum - NextIndex;
					appMemmove(&GetTypedData()[Index],&GetTypedData()[NextIndex],sizeof(ElementType) * NumElementsToMove);
				}

				// Update the array count
				--ArrayNum;

				// Removed one item
				return 1;
			}
		}

		// Specified item was not found.  Removed zero items.
		return 0;
	}

	/** Removes as many instances of Item as there are in the array, maintaining order but not indices. */
	INT RemoveItem( const ElementType& Item )
	{
		// It isn't valid to specify an Item that is in the array, since removing that item will change Item's value.
		check( ((&Item) < GetTypedData()) || ((&Item) >= GetTypedData()+ArrayMax) );

		const INT OriginalNum = ArrayNum;
		if (!OriginalNum)
		{
			return 0; // nothing to do, loop assumes one item so need to deal with this edge case here
		}

		INT WriteIndex = 0;
		INT ReadIndex = 0;
		UBOOL NotMatch = !(GetTypedData()[ReadIndex] == Item); // use a ! to guarantee it can't be anything other than zero or one
		do
		{
			INT RunStartIndex = ReadIndex++;
			while (ReadIndex < OriginalNum && NotMatch == !(GetTypedData()[ReadIndex] == Item))
			{
				ReadIndex++;
			}
			INT RunLength = ReadIndex - RunStartIndex;
			checkSlow(RunLength > 0);
			if (NotMatch)
			{
				// this was a non-matching run, we need to move it
				if (WriteIndex != RunStartIndex)
				{
					appMemmove( &GetTypedData()[ WriteIndex ], &GetTypedData()[ RunStartIndex ], sizeof(ElementType) * RunLength );
				}
				WriteIndex += RunLength;
			}
			else
			{
				// this was a matching run, delete it
				DestructItems( RunStartIndex, RunLength );
			}
			NotMatch = !NotMatch;
		} while (ReadIndex < OriginalNum);

		ArrayNum = WriteIndex;
		return OriginalNum - ArrayNum;
	}


	/**
	 * Removes the first occurrence of the specified item in the array.  This version is much more efficient
	 * O(Count) instead of O(ArrayNum), but does not preserve the order
	 *
	 * @param	Item	The item to remove
	 *
	 * @return	The number of items removed.  For RemoveSingleItem, this is always either 0 or 1.
	 */
	INT RemoveSingleItemSwap( const ElementType& Item )
	{
		check( ((&Item) < (ElementType*)AllocatorInstance.GetAllocation()) || ((&Item) >= (ElementType*)AllocatorInstance.GetAllocation()+ArrayMax) );
		const INT OriginalNum=ArrayNum;
		for( INT Index=0; Index<ArrayNum; Index++ )
		{
			if( (*this)(Index)==Item )
			{
				RemoveSwap( Index-- );

				// Removed one item
				return 1;
			}
		}

		// Specified item was not found.  Removed zero items.
		return 0;
	}

	/** RemoveItemSwap, this version is much more efficient O(Count) instead of O(ArrayNum), but does not preserve the order */
	INT RemoveItemSwap( const ElementType& Item )
	{
		check( ((&Item) < (ElementType*)AllocatorInstance.GetAllocation()) || ((&Item) >= (ElementType*)AllocatorInstance.GetAllocation()+ArrayMax) );
		const INT OriginalNum=ArrayNum;
		for( INT Index=0; Index<ArrayNum; Index++ )
		{
			if( (*this)(Index)==Item )
			{
				RemoveSwap( Index-- );
			}
		}
		return OriginalNum - ArrayNum;
	}

	void Swap(INT A, INT B)
	{
		appMemswap(
			(BYTE*)AllocatorInstance.GetAllocation()+(sizeof(ElementType)*A),
			(BYTE*)AllocatorInstance.GetAllocation()+(sizeof(ElementType)*B),
			sizeof(ElementType)
			);
	}

	void SwapItems(INT A, INT B)
	{
		check((A >= 0) && (B >= 0));
		check((ArrayNum > A) && (ArrayNum > B));
		if (A != B)
		{
			Swap(A,B);
		}
	}

	/**
	 * Same as empty, but doesn't change memory allocations, unless the new size is larger than
	 * the current array. It calls the destructors on held items if needed and then zeros the ArrayNum.
	 *
	 * @param NewSize the expected usage size
	 */
	void Reset(INT NewSize = 0)
	{
		// If we have space to hold the excepted size, then don't reallocate
		if (NewSize <= ArrayMax)
		{
			DestructItems(0,ArrayNum);
			ArrayNum = 0;
		}
		else
		{
			Empty(NewSize);
		}
	}

	/**
	 * Searches for the first entry of the specified type, will only work
	 * with TArray<UObject*>.  Optionally return the item's index, and can
	 * specify the start index.
	 */
	template<typename SearchType> UBOOL FindItemByClass(SearchType **Item = NULL, INT *ItemIndex = NULL, INT StartIndex = 0)
	{
		UClass* SearchClass = SearchType::StaticClass();
		for (INT Idx = StartIndex; Idx < ArrayNum; Idx++)
		{
			if ((*this)(Idx) != NULL && (*this)(Idx)->IsA(SearchClass))
			{
				if (Item != NULL)
				{
					*Item = (SearchType*)((*this)(Idx));
				}
				if (ItemIndex != NULL)
				{
					*ItemIndex = Idx;
				}
				return TRUE;
			}
		}
		return FALSE;
	}

	// Iterators
	typedef TIndexedContainerIterator< TArray<ElementType,Allocator> >  TIterator;
	typedef TIndexedContainerConstIterator< TArray<ElementType,Allocator> >  TConstIterator;

#if defined(_MSC_VER)
private:
	/**
	* Helper function that can be used inside the debuggers watch window to debug TArrays. E.g. "*Class->Defaults.DebugGet(5)". 
	*
	* @param	i	Index
	* @return		pointer to type T at Index i
	*/
	FORCENOINLINE const ElementType& DebugGet( INT i ) const
	{
		return GetTypedData()[i];
	}
#endif

protected:

	/**
	 * Copies data from one array into this array. Uses the fast path if the
	 * data in question does not need a constructor.
	 *
	 * @param Source the source array to copy
	 */
	template<typename OtherAllocator>
	void Copy(const TArray<ElementType,OtherAllocator>& Source)
	{
		if ((void*)this != (void*)&Source)
		{
			// Just empty our array if there is nothing to copy
			if (Source.Num() > 0)
			{
				// Presize the array so there are no extra allocs/memcpys
				Empty(Source.Num());
				// Determine whether we need per element construction or bulk
				// copy is fine
				if (TContainerTraits<ElementType>::NeedsConstructor)
				{
					// Use the inplace new to copy the element to an array element
					for (INT Index = 0; Index < Source.Num(); Index++)
					{
						new(GetTypedData() + Index) ElementType(Source(Index));
					}
				}
				else
				{
					// Use the much faster path for types that allow it
					appMemcpy(AllocatorInstance.GetAllocation(),&Source(0),sizeof(ElementType) * Source.Num());
				}
				ArrayNum = Source.Num();
			}
			else
			{
				Empty(0);
			}
		}
	}
	
	/** Destructs a range of items in the array. */
	FORCEINLINE void DestructItems(INT Index,INT Count)
	{
		if( TContainerTraits<ElementType>::NeedsDestructor )
		{
			for( INT i=Index; i<Index+Count; i++ )
			{
				(&(*this)(i))->~ElementType();
			}
		}
	}

	enum { AllocatorNeedsElementType = Allocator::NeedsElementType };

	typedef typename TChooseClass<
		AllocatorNeedsElementType,
		typename Allocator::template ForElementType<ElementType>,
		typename Allocator::ForAnyElementType
		>::Result ElementAllocatorType;

	ElementAllocatorType AllocatorInstance;
	INT	  ArrayNum;
	INT	  ArrayMax;
};

// Can't override allocator in TArrayNoInit due to handling w/ FScriptArray
template<typename T>
class TArrayNoInit : public TArray<T>
{
public:
	TArrayNoInit()
	: TArray<T>(E_NoInit)
	{}
	explicit TArrayNoInit(EForceInit)
	{}
	
	TArrayNoInit& operator=( const TArrayNoInit& Other )
	{
		TArray<T>::operator=(Other);
		return *this;
	}
	TArrayNoInit& operator=( const TArray<T>& Other )
	{
		TArray<T>::operator=(Other);
		return *this;
	}
};

//
// Array operator news.
//
template <typename T,typename Allocator> void* operator new( size_t Size, TArray<T,Allocator>& Array )
{
	check(Size == sizeof(T));
	const INT Index = Array.Add(1);
	return &Array(Index);
}
template <typename T,typename Allocator> void* operator new( size_t Size, TArray<T,Allocator>& Array, INT Index )
{
	check(Size == sizeof(T));
	Array.Insert(Index,1);
	return &Array(Index);
}

/** A specialization of the exchange macro that avoids reallocating when exchanging two arrays. */
template <typename T> inline void Exchange( TArray<T>& A, TArray<T>& B )
{
	appMemswap( &A, &B, sizeof(TArray<T>) );
}

/** A specialization of the exchange macro that avoids reallocating when exchanging two arrays. */
template<typename ElementType,typename Allocator>
inline void Exchange( TArray<ElementType,Allocator>& A, TArray<ElementType,Allocator>& B )
{
	appMemswap( &A, &B, sizeof(TArray<ElementType,Allocator>) );
}

/*-----------------------------------------------------------------------------
	MRU array.
-----------------------------------------------------------------------------*/

/**
 * Same as TArray except:
 * - Has an upper limit of the number of items it will store.
 * - Any item that is added to the array is moved to the top.
 */
template<typename T,typename Allocator = FDefaultAllocator>
class TMRUArray : public TArray<T,Allocator>
{
public:
	typedef TArray<T,Allocator> Super;

	/** The maximum number of items we can store in this array. */
	INT MaxItems;

	TMRUArray()
	:	Super()
	{
		MaxItems = 0;
	}
	TMRUArray( INT InNum )
	:	Super( InNum )
	{
		MaxItems = 0;
	}
	TMRUArray( const TMRUArray& Other )
	:	Super( Other.ArrayNum )
	{
		MaxItems = 0;
	}
	TMRUArray( ENoInit )
	:	Super( E_NoInit )
	{
		MaxItems = 0;
	}
	~TMRUArray()
	{
		checkSlow(this->ArrayNum>=0);
		checkSlow(this->ArrayMax>=this->ArrayNum);
		this->Remove( 0, this->ArrayNum );
	}

	T& operator()( INT i )
	{
		checkSlowish(i>=0 && i<this->ArrayNum);
		checkSlow(this->ArrayMax>=this->ArrayNum);
		return this->GetTypedData()[i];
	}
	const T& operator()( INT i ) const
	{
		checkSlowish(i>=0 && i<this->ArrayNum);
		checkSlow(this->ArrayMax>=this->ArrayNum);
		return this->GetTypedData()[i];
	}

	void Empty( INT Slack=0 )
	{
		Super::Empty( Slack );
	}
	INT AddItem( const T& Item )
	{
		const INT idx = Super::AddItem( Item );
		this->SwapItems( idx, 0 );
		CullArray();
		return 0;
	}
	INT AddZeroed( INT Count=1 )
	{
		const INT idx = Super::AddZeroed( Count );
		this->SwapItems( idx, 0 );
		CullArray();
		return 0;
	}
	INT AddUniqueItem( const T& Item )
	{
		// Remove any existing copies of the item.
		this->RemoveItem(Item);

		this->Insert( 0 );
		(*this)(0) = Item;

		CullArray();

		return 0;
	}

	/**
	 * Makes sure that the array never gets beyond MaxItems in size.
	 */

	void CullArray()
	{
		// 0 = no limit
		if( !MaxItems )
		{
			return;
		}

		while( this->Num() > MaxItems )
		{
			this->Remove( this->Num()-1, 1 );
		}
	}
};
#if 0
/*-----------------------------------------------------------------------------
	Indirect array.
	Same as a TArray above, but stores pointers to the elements, to allow
	resizing the array index without relocating the actual elements.
-----------------------------------------------------------------------------*/

template<typename T,typename Allocator = FDefaultAllocator>
class TIndirectArray : public TArray<void*,Allocator>
{
public:
	typedef TArray<void*,Allocator> Super;

	typedef T ElementType;
	TIndirectArray()
	:	Super()
	{}
	TIndirectArray( INT InNum )
	:	Super( InNum, sizeof(void*) )
	{}
	TIndirectArray( const TIndirectArray& Other )
	:	Super( Other.ArrayNum, sizeof(void*) )
	{
		this->ArrayNum=0;
		for( INT i=0; i<Other.ArrayNum; i++ )
		{
			new(*this)T(Other(i));
		}
	}
	TIndirectArray( ENoInit )
	:	Super( E_NoInit )
	{}
	~TIndirectArray()
	{
		checkSlow(this->ArrayNum>=0);
		checkSlow(this->ArrayMax>=this->ArrayNum);
		Remove( 0, this->ArrayNum );
	}
	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @return pointer to first array entry or NULL if this->ArrayMax==0
	 */
	T** GetTypedData()
	{
		return (T**)AllocatorInstance.GetAllocation();
	}
	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @return pointer to first array entry or NULL if this->ArrayMax==0
	 */
	const T** GetTypedData() const
	{
		return (T**)AllocatorInstance.GetAllocation();
	}
	/** 
	 * Helper function returning the size of the inner type
	 *
	 * @return size in bytes of array type
	 */
	DWORD GetTypeSize() const
	{
		return sizeof(T*);
	}
	/**
	 * Copies the source array into this one.
	 *
	 * @param Other the source array to copy
	 */
	TIndirectArray& operator=( const TIndirectArray& Other )
	{
		Empty( Other.Num() );
		for( INT i=0; i<Other.Num(); i++ )
		{
			new(*this)T(Other(i));
		}	
		return *this;
	}
    T& operator()( INT i )
	{
		checkSlowish(i>=0 && i<this->ArrayNum );
		checkSlow(this->ArrayMax>=this->ArrayNum);
		return *((T**)AllocatorInstance.GetAllocation())[i];
	}
	const T& operator()( INT i ) const
	{
		checkSlowish(i>=0 && i<this->ArrayNum);
		checkSlow(this->ArrayMax>=this->ArrayNum);
		return *((T**)AllocatorInstance.GetAllocation())[i];
	}
	void Shrink()
	{
		Super::Shrink( sizeof(void*) );
	}
	UBOOL FindItem( const T*& Item, INT& Index ) const
	{
		for( Index=0; Index<this->ArrayNum; Index++ )
		{
			if( (*this)(Index)==*Item )
			{
				return TRUE;
			}
		}
		return FALSE;
	}
	INT FindItemIndex( const T*& Item ) const
	{
		for( INT Index=0; Index<this->ArrayNum; Index++ )
		{
			if( (*this)(Index)==*Item )
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
	/**
	 * Special serialize function passing the owning UObject along as required by FUnytpedBulkData
	 * serialization.
	 *
	 * @param	Ar		Archive to serialize with
	 * @param	Owner	UObject this structure is serialized within
	 */
	void Serialize( FArchive& Ar, UObject* Owner )
	{
		CountBytes( Ar );
		if( Ar.IsLoading() )
		{
			// Load array.
			INT NewNum;
			Ar << NewNum;
			Empty( NewNum );
			for( INT i=0; i<NewNum; i++ )
			{
				new(*this) T;
			}
			for( INT i=0; i<NewNum; i++ )
			{
				(*this)(i).Serialize( Ar, Owner, i );
			}
		}
		else
		{
			// Save array.
			Ar << this->ArrayNum;
			for( INT i=0; i<this->ArrayNum; i++ )
			{
				(*this)(i).Serialize( Ar, Owner, i );
			}
		}
	}
	friend FArchive& operator<<( FArchive& Ar, TIndirectArray& A )
	{
		A.CountBytes( Ar );
		if( Ar.IsLoading() )
		{
			// Load array.
			INT NewNum;
			Ar << NewNum;
			A.Empty( NewNum );
			for( INT i=0; i<NewNum; i++ )
			{
				Ar << *new(A)T;
			}
		}
		else
		{
			// Save array.
			Ar << A.ArrayNum;
			for( INT i=0; i<A.ArrayNum; i++ )
			{
				Ar << A( i );
			}
		}
		return Ar;
	}
	void CountBytes( FArchive& Ar )
	{
		Super::CountBytes( Ar, sizeof(void*) );
	}
	void Remove( INT Index, INT Count=1 )
	{
		check(Index>=0);
		check(Index<=this->ArrayNum);
		check(Index+Count<=this->ArrayNum);
		for( INT i=Index; i<Index+Count; i++ )
		{
			delete ((T**)AllocatorInstance.GetAllocation())[i];
		}
		Super::Remove( Index, Count );
	}
	void Empty( INT Slack=0 )
	{
		for( INT i=0; i<this->ArrayNum; i++ )
		{
			delete ((T**)AllocatorInstance.GetAllocation())[i];
		}
		Super::Empty( Slack );
	}
	INT AddRawItem(T* Item)
	{
		const INT	Index = Super::Add(1);
		((T**)AllocatorInstance.GetAllocation())[Index] = Item;
		return Index;
	}
	/** 
	* Helper function to return the amount of memory allocated by this container 
	*
	* @return number of bytes allocated by this container
	*/
	DWORD GetAllocatedSize( void ) const
	{
		return( this->ArrayMax * (sizeof(T) + sizeof(T*)) );
	}
};

#else

/*-----------------------------------------------------------------------------
	Indirect array.
	Same as a TArray above, but stores pointers to the elements, to allow
	resizing the array index without relocating the actual elements.
-----------------------------------------------------------------------------*/

template<typename T,typename Allocator = FDefaultAllocator>
class TIndirectArray : public TArray<void*,Allocator>
{
public:
	typedef TArray<void*,Allocator> Super;

	typedef T ElementType;
	TIndirectArray()
	:	Super()
	{}
	TIndirectArray( INT InNum )
	:	Super( InNum )
	{}
	TIndirectArray( const TIndirectArray& Other )
	:	Super( Other.ArrayNum )
	{
		this->ArrayNum=0;
		for( INT i=0; i<Other.ArrayNum; i++ )
		{
			new(*this)T(Other(i));
		}
	}
	TIndirectArray( ENoInit )
	:	Super( E_NoInit )
	{}
	~TIndirectArray()
	{
		checkSlow(this->ArrayNum>=0);
		checkSlow(this->ArrayMax>=this->ArrayNum);
		Remove( 0, this->ArrayNum );
	}
	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @return pointer to first array entry or NULL if this->ArrayMax==0
	 */
	T** GetTypedData()
	{
		return (T**)this->AllocatorInstance.GetAllocation();
	}
	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @return pointer to first array entry or NULL if this->ArrayMax==0
	 */
	const T** GetTypedData() const
	{
		return (const T**)this->AllocatorInstance.GetAllocation();
	}
	/** 
	 * Helper function returning the size of the inner type
	 *
	 * @return size in bytes of array type
	 */
	DWORD GetTypeSize() const
	{
		return sizeof(T*);
	}
	/**
	 * Copies the source array into this one.
	 *
	 * @param Other the source array to copy
	 */
	TIndirectArray& operator=( const TIndirectArray& Other )
	{
		Empty( Other.Num() );
		for( INT i=0; i<Other.Num(); i++ )
		{
			new(*this)T(Other(i));
		}	
		return *this;
	}
    T& operator()( INT i )
	{
		checkSlowish(i>=0 && i<this->ArrayNum);
		checkSlow(this->ArrayMax>=this->ArrayNum);
		return *((T**)this->AllocatorInstance.GetAllocation())[i];
	}
	const T& operator()( INT i ) const
	{
		checkSlowish(i>=0 && i<this->ArrayNum);
		checkSlow(this->ArrayMax>=this->ArrayNum);
		return *((T**)this->AllocatorInstance.GetAllocation())[i];
	}
	void Shrink()
	{
		Super::Shrink( );
	}
	UBOOL FindItem( const T* Item, INT& Index ) const
	{
		for( Index=0; Index<this->ArrayNum; Index++ )
		{
			if( (*this)(Index)==*Item )
			{
				return TRUE;
			}
		}
		return FALSE;
	}
	INT FindItemIndex( const T* Item ) const
	{
		for( INT Index=0; Index<this->ArrayNum; Index++ )
		{
			if( (*this)(Index)==*Item )
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
	/**
	 * Special serialize function passing the owning UObject along as required by FUnytpedBulkData
	 * serialization.
	 *
	 * @param	Ar		Archive to serialize with
	 * @param	Owner	UObject this structure is serialized within
	 */
	void Serialize( FArchive& Ar, UObject* Owner )
	{
		CountBytes( Ar );
		if( Ar.IsLoading() )
		{
			// Load array.
			INT NewNum;
			Ar << NewNum;
			Empty( NewNum );
			for( INT i=0; i<NewNum; i++ )
			{
				new(*this) T;
			}
			for( INT i=0; i<NewNum; i++ )
			{
				(*this)(i).Serialize( Ar, Owner, i );
			}
		}
		else
		{
			// Save array.
			Ar << this->ArrayNum;
			for( INT i=0; i<this->ArrayNum; i++ )
			{
				(*this)(i).Serialize( Ar, Owner, i );
			}
		}
	}
	friend FArchive& operator<<( FArchive& Ar, TIndirectArray& A )
	{
		A.CountBytes( Ar );
		if( Ar.IsLoading() )
		{
			// Load array.
			INT NewNum;
			Ar << NewNum;
			A.Empty( NewNum );
			for( INT i=0; i<NewNum; i++ )
			{
				Ar << *new(A)T;
			}
		}
		else
		{
			// Save array.
			Ar << A.ArrayNum;
			for( INT i=0; i<A.ArrayNum; i++ )
			{
				Ar << A( i );
			}
		}
		return Ar;
	}
	void CountBytes( FArchive& Ar )
	{
		Super::CountBytes( Ar );
	}
	void Remove( INT Index, INT Count=1 )
	{
		check(Index>=0);
		check(Index<=this->ArrayNum);
		check(Index+Count<=this->ArrayNum);
		for( INT i=Index; i<Index+Count; i++ )
		{
			delete ((T**)this->AllocatorInstance.GetAllocation())[i];
		}
		Super::Remove( Index, Count );
	}
	void Empty( INT Slack=0 )
	{
		for( INT i=0; i<this->ArrayNum; i++ )
		{
			delete ((T**)this->AllocatorInstance.GetAllocation())[i];
		}
		Super::Empty( Slack );
	}
	INT AddRawItem(T* Item)
	{
		const INT	Index = Super::Add(1);
		((T**)this->AllocatorInstance.GetAllocation())[Index] = Item;
		return Index;
	}
	/** 
	* Helper function to return the amount of memory allocated by this container 
	*
	* @return number of bytes allocated by this container
	*/
	DWORD GetAllocatedSize( void ) const
	{
		return( this->ArrayMax * (sizeof(T) + sizeof(T*)) );
	}
};

#endif

template<typename T> void* operator new( size_t Size, TIndirectArray<T>& Array )
{
	check(Size == sizeof(T));
	const INT Index = Array.AddRawItem((T*)appMalloc(Size));
	return &Array(Index);
}

/*-----------------------------------------------------------------------------
	Transactional array.
-----------------------------------------------------------------------------*/

// NOTE: Right now, you can't use a custom allocation policy with transactional arrays. If
// you need to do it, you will have to fix up FTransaction::FObjectRecord to use the correct TArray<Allocator>.
template< typename T >
class TTransArray : public TArray<T>
{
public:
	typedef TArray<T> Super;

	// Constructors.
	TTransArray( UObject* InOwner, INT InNum=0 )
	:	Super( InNum )
	,	Owner( InOwner )
	{
		checkSlow(Owner);
	}
	TTransArray( UObject* InOwner, const Super& Other )
	:	Super( Other )
	,	Owner( InOwner )
	{
		checkSlow(Owner);
	}
	TTransArray& operator=( const TTransArray& Other )
	{
		operator=( (const Super&)Other );
		return *this;
	}

	// Add, Insert, Remove, Empty interface.
	INT Add( INT Count=1 )
	{
		const INT Index = Super::Add( Count );
		if( GUndo )
		{
			GUndo->SaveArray( Owner, (FScriptArray*)this, Index, Count, 1, sizeof(T), SerializeItem, DestructItem );
		}
		return Index;
	}
	void Insert( INT Index, INT Count=1 )
	{
		Super::Insert( Index, Count );
		if( GUndo )
		{
			GUndo->SaveArray( Owner, (FScriptArray*)this, Index, Count, 1, sizeof(T), SerializeItem, DestructItem );
		}
	}
	void Remove( INT Index, INT Count=1 )
	{
		if( GUndo )
		{
			GUndo->SaveArray( Owner, (FScriptArray*)this, Index, Count, -1, sizeof(T), SerializeItem, DestructItem );
		}
		Super::Remove( Index, Count );
	}
	void Empty( INT Slack=0 )
	{
		if( GUndo )
		{
			GUndo->SaveArray( Owner, (FScriptArray*)this, 0, this->ArrayNum, -1, sizeof(T), SerializeItem, DestructItem );
		}
		Super::Empty( Slack );
	}

	// Functions dependent on Add, Remove.
	TTransArray& operator=( const Super& Other )
	{
		if( this != &Other )
		{
			Empty( Other.Num() );
			for( INT i=0; i<Other.Num(); i++ )
			{
				new( *this )T( Other(i) );
			}
		}
		return *this;
	}
	INT AddItem( const T& Item )
	{
		new(*this) T(Item);
		return this->Num() - 1;
	}
	INT AddZeroed( INT n=1 )
	{
		const INT Index = Add(n);
		appMemzero( &(*this)(Index), n*sizeof(T) );
		return Index;
	}
	INT AddUniqueItem( const T& Item )
	{
		for( INT Index=0; Index<this->ArrayNum; Index++ )
		{
			if( (*this)(Index)==Item )
			{
				return Index;
			}
		}
		return AddItem( Item );
	}
	INT RemoveItem( const T& Item )
	{
		check( ((&Item) < (T*)this->AllocatorInstance.GetAllocation()) || ((&Item) >= (T*)this->AllocatorInstance.GetAllocation()+this->ArrayMax) );
		const INT OriginalNum=this->ArrayNum;
		for( INT Index=0; Index<this->ArrayNum; Index++ )
		{
			if( (*this)(Index)==Item )
			{
				Remove( Index-- );
			}
		}
		return OriginalNum - this->ArrayNum;
	}

	// TTransArray interface.
	UObject* GetOwner()
	{
		return Owner;
	}
	void SetOwner( UObject* NewOwner )
	{
		Owner = NewOwner;
	}
	void ModifyItem( INT Index )
	{
		if( GUndo )
			GUndo->SaveArray( Owner, (FScriptArray*)this, Index, 1, 0, sizeof(T), SerializeItem, DestructItem );
	}
	void ModifyAllItems()
	{
		if( GUndo )
		{
			GUndo->SaveArray( Owner, (FScriptArray*)this, 0, this->Num(), 0, sizeof(T), SerializeItem, DestructItem );
		}
	}
	friend FArchive& operator<<( FArchive& Ar, TTransArray& A )
	{
		Ar << A.Owner;
		Ar << (Super&)A;
		return Ar;
	}
protected:
	static void SerializeItem( FArchive& Ar, void* TPtr )
	{
		Ar << *(T*)TPtr;
	}
	static void DestructItem( void* TPtr )
	{
		((T*)TPtr)->~T();
	}
	UObject* Owner;
private:

	// Disallow the copy constructor.
	TTransArray( const Super& Other )
	{}
};

//
// Transactional array operator news.
//
template <typename T> void* operator new( size_t Size, TTransArray<T>& Array )
{
	check(Size == sizeof(T));
	const INT Index = Array.Add();
	return &Array(Index);
}
template <typename T> void* operator new( size_t Size, TTransArray<T>& Array, INT Index )
{
	check(Size == sizeof(T));
	Array.Insert(Index);
	return &Array(Index);
}

/**
 * A statically allocated array with a dynamic number of elements.
 */
template<typename Type,UINT MaxElements>
class TPreallocatedArray
{
public:
	TPreallocatedArray(): NumElements(0) {}

	UINT AddUniqueItem(const Type& Item)
	{
		for(UINT Index = 0;Index < Num();Index++)
		{
			if(Elements[Index] == Item)
			{
				return Index;
			}
		}
		return AddItem(Item);
	}

	UINT AddItem(const Type& Item)
	{
		//checkSlow(NumElements < MaxElements);
		Elements[NumElements] = Item;
		return NumElements++;
	}

	void Remove(UINT BaseIndex,UINT Count = 1)
	{
		if (Count > 0)
		{
			checkSlow(BaseIndex <= NumElements);
			checkSlow(BaseIndex + Count <= NumElements);

			// Destruct the elements being removed.
			if(TContainerTraits<Type>::NeedsDestructor)
			{
				for(UINT Index = BaseIndex;Index < BaseIndex + Count;Index++)
				{
					(&Elements[Index])->~Type();
				}
			}

			// Move the elements which have a greater index than the removed elements into the gap.
			appMemmove
				(
				(BYTE*)Elements + (BaseIndex      ) * sizeof(Type),
				(BYTE*)Elements + (BaseIndex+Count) * sizeof(Type),
				(NumElements - BaseIndex - Count ) * sizeof(Type)
				);
			NumElements -= Count;

			checkSlow(MaxElements >= NumElements);
		}
	}

	void Empty()
	{
		NumElements = 0;
	}

	Type& operator()(UINT Index)
	{
		//checkSlow(Index < NumElements);
		return Elements[Index];
	}

	const Type& operator()(UINT Index) const
	{
		//checkSlow(Index < NumElements);
		return Elements[Index];
	}

	UINT Num() const
	{
		return NumElements;
	}
	/** 
	* Helper function to return the amount of memory allocated by this container 
	*
	* @return number of bytes allocated by this container
	*/
	DWORD GetAllocatedSize( void ) const
	{
		return( MaxElements * sizeof( Type ) );
	}
private:
	Type Elements[MaxElements];
	UINT NumElements;
};

#endif
