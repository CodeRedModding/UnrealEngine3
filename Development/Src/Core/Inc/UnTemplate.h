/*=============================================================================
	UnTemplate.h: Unreal templates.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*-----------------------------------------------------------------------------
	Type information.
-----------------------------------------------------------------------------*/

#include <string.h>

//
// Type information for initialization.
//
template <class T> struct TTypeInfoBase
{
public:
	typedef const T& ConstInitType;
	static UBOOL NeedsDestructor() {return 1;}
	static UBOOL DefinitelyNeedsDestructor() {return 0;}
	static const T& ToInit( const T& In ) {return In;}
};
template <class T> struct TTypeInfo : public TTypeInfoBase<T>
{
};

template <> struct TTypeInfo<BYTE> : public TTypeInfoBase<BYTE>
{
public:
	static UBOOL NeedsDestructor() {return 0;}
};
template <> struct TTypeInfo<SBYTE> : public TTypeInfoBase<SBYTE>
{
public:
	static UBOOL NeedsDestructor() {return 0;}
};
template <> struct TTypeInfo<ANSICHAR> : public TTypeInfoBase<ANSICHAR>
{
public:
	static UBOOL NeedsDestructor() {return 0;}
};
template <> struct TTypeInfo<INT> : public TTypeInfoBase<INT>
{
public:
	static UBOOL NeedsDestructor() {return 0;}
};
template <> struct TTypeInfo<DWORD> : public TTypeInfoBase<DWORD>
{
public:
	static UBOOL NeedsDestructor() {return 0;}
};
template <> struct TTypeInfo<_WORD> : public TTypeInfoBase<_WORD>
{
public:
	static UBOOL NeedsDestructor() {return 0;}
};
template <> struct TTypeInfo<SWORD> : public TTypeInfoBase<SWORD>
{
public:
	static UBOOL NeedsDestructor() {return 0;}
};
template <> struct TTypeInfo<QWORD> : public TTypeInfoBase<QWORD>
{
public:
	static UBOOL NeedsDestructor() {return 0;}
};
template <> struct TTypeInfo<SQWORD> : public TTypeInfoBase<SQWORD>
{
public:
	static UBOOL NeedsDestructor() {return 0;}
};
template <> struct TTypeInfo<FName> : public TTypeInfoBase<FName>
{
public:
	static UBOOL NeedsDestructor() {return 0;}
};
template <> struct TTypeInfo<UObject*> : public TTypeInfoBase<UObject*>
{
public:
	static UBOOL NeedsDestructor() {return 0;}
};

/*-----------------------------------------------------------------------------
	Standard templates.
-----------------------------------------------------------------------------*/

template< class T > inline T Abs( const T A )
{
	return (A>=(T)0) ? A : -A;
}
template< class T > inline T Sgn( const T A )
{
	return (A>0) ? 1 : ((A<0) ? -1 : 0);
}
template< class T > inline T Max( const T A, const T B )
{
	return (A>=B) ? A : B;
}
template< class T > inline T Min( const T A, const T B )
{
	return (A<=B) ? A : B;
}
template< class T > inline T Max3( const T A, const T B, const T C )
{
	return Max ( Max( A, B ), C );
}
template< class T > inline T Min3( const T A, const T B, const T C )
{
	return Min ( Min( A, B ), C );
}
template< class T > inline T Square( const T A )
{
	return A*A;
}
template< class T > inline T Clamp( const T X, const T Min, const T Max )
{
	return X<Min ? Min : X<Max ? X : Max;
}
template< class T > inline T Align( const T Ptr, INT Alignment )
{
	return (T)(((DWORD)Ptr + Alignment - 1) & ~(Alignment-1));
}
template< class T > inline void Exchange( T& A, T& B )
{
	const T Temp = A;
	A = B;
	B = Temp;
}

template< class T, class U > T Lerp( const T& A, const T& B, const U& Alpha )
{
	return (T)(A + Alpha * (B-A));
}

template<class T> T BiLerp(const T& P00,const T& P10,const T& P01,const T& P11,FLOAT FracX,FLOAT FracY)
{
	return Lerp(
			Lerp(P00,P10,FracX),
			Lerp(P01,P11,FracX),
			FracY
			);
}

// P - end points
// T - tangent directions at end points
// Alpha - distance along spline
template< class T, class U > T CubicInterp( const T& P0, const T& T0, const T& P1, const T& T1, const U& A )
{
	FLOAT A3 = appPow(A,3);
	FLOAT A2 = appPow(A,2);  

	return (T)(((2*A3)-(3*A2)+1) * P0) + ((A3-(2*A2)+A) * T0) + ((A3-A2) * T1) + (((-2*A3)+(3*A2)) * P1);
}

inline DWORD GetTypeHash( const BYTE A )
{
	return A;
}
inline DWORD GetTypeHash( const SBYTE A )
{
	return A;
}
inline DWORD GetTypeHash( const _WORD A )
{
	return A;
}
inline DWORD GetTypeHash( const SWORD A )
{
	return A;
}
inline DWORD GetTypeHash( const INT A )
{
	return A;
}
inline DWORD GetTypeHash( const DWORD A )
{
	return A;
}
inline DWORD GetTypeHash( const QWORD A )
{
	return (DWORD)A+((DWORD)(A>>32) * 23);
}
inline DWORD GetTypeHash( const SQWORD A )
{
	return (DWORD)A+((DWORD)(A>>32) * 23);
}
inline DWORD GetTypeHash( const TCHAR* S )
{
	return appStrihash(S);
}
#define ExchangeB(A,B) {UBOOL T=A; A=B; B=T;}

/*----------------------------------------------------------------------------
	Standard macros.
----------------------------------------------------------------------------*/

// Number of elements in an array.
#define ARRAY_COUNT( array ) \
	( sizeof(array) / sizeof((array)[0]) )

// Offset of a struct member.
#define STRUCT_OFFSET( struc, member ) \
	( (INT)&((struc*)NULL)->member )

/*-----------------------------------------------------------------------------
	Allocators.
-----------------------------------------------------------------------------*/

template <class T> class TAllocator
{};

/*-----------------------------------------------------------------------------
	Dynamic array template.
-----------------------------------------------------------------------------*/

//
// Base dynamic array.
//
class FArray
{
public:
	void* GetData()
	{
		return Data;
	}
	const void* GetData() const
	{
		return Data;
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
	void InsertZeroed( INT Index, INT Count, INT ElementSize )
	{
		Insert( Index, Count, ElementSize );
		appMemzero( (BYTE*)Data+Index*ElementSize, Count*ElementSize );
	}
	void Insert( INT Index, INT Count, INT ElementSize )
	{
		checkSlow(Count>=0);
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);
		checkSlow(Index>=0);
		checkSlow(Index<=ArrayNum);

		INT OldNum = ArrayNum;
		if( (ArrayNum+=Count)>ArrayMax )
		{
			ArrayMax = ArrayNum + 3*ArrayNum/8 + 32;
			Realloc( ElementSize );
		}
		appMemmove
		(
			(BYTE*)Data + (Index+Count )*ElementSize,
			(BYTE*)Data + (Index       )*ElementSize,
			              (OldNum-Index)*ElementSize
		);
	}
	INT Add( INT Count, INT ElementSize )
	{
		checkSlow(Count>=0);
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);

		INT Index = ArrayNum;
		if( (ArrayNum+=Count)>ArrayMax )
		{
			ArrayMax = ArrayNum + 3*ArrayNum/8 + 32;
			Realloc( ElementSize );
		}

		return Index;
	}
	INT AddZeroed( INT ElementSize, INT n=1 )
	{
		INT Index = Add( n, ElementSize );
		appMemzero( (BYTE*)Data+Index*ElementSize, n*ElementSize );
		return Index;
	}
	void Shrink( INT ElementSize )
	{
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);
		if( ArrayMax != ArrayNum )
		{
			ArrayMax = ArrayNum;
			Realloc( ElementSize );
		}
	}
	void Empty( INT ElementSize, INT Slack=0 )
	{
		ArrayNum = 0;
		ArrayMax = Slack;
		Realloc( ElementSize );
	}
	void Swap(INT A, INT B, INT ElementSize)
	{
		appMemswap((BYTE*)Data+(ElementSize*A),(BYTE*)Data+(ElementSize*B),ElementSize);
	}
	FArray()
	:	ArrayNum( 0 )
	,	ArrayMax( 0 )
	,	Data	( NULL )
	{}
	FArray( ENoInit )
	{}
	~FArray()
	{
		if( Data )
			appFree( Data );
		Data = NULL;
		ArrayNum = ArrayMax = 0;
	}
	void CountBytes( FArchive& Ar, INT ElementSize )
	{
		Ar.CountBytes( ArrayNum*ElementSize, ArrayMax*ElementSize );
	}
	void Remove( INT Index, INT Count, INT ElementSize );
protected:
	void Realloc( INT ElementSize );
	FArray( INT InNum, INT ElementSize )
	:	ArrayNum( InNum )
	,	ArrayMax( InNum )
	,	Data    ( NULL  )
	{
		Realloc( ElementSize );
	}
	void* Data;
	INT	  ArrayNum;
	INT	  ArrayMax;
};

//
// Templated dynamic array.
//
template< class T > class TArray : public FArray
{
public:
	typedef T ElementType;
	TArray()
	:	FArray()
	{}
	TArray( INT InNum )
	:	FArray( InNum, sizeof(T) )
	{}
	TArray( const TArray& Other )
	:	FArray( Other.ArrayNum, sizeof(T) )
	{
		if( TTypeInfo<T>::NeedsDestructor() )
		{
			ArrayNum=0;
			for( INT i=0; i<Other.ArrayNum; i++ )
				new(*this)T(Other(i));
		}
		else if( sizeof(T)!=1 )
		{
			for( INT i=0; i<ArrayNum; i++ )
				(*this)(i) = Other(i);
		}
		else
		{
			appMemcpy( &(*this)(0), &Other(0), ArrayNum * sizeof(T) );
		}
	}
	TArray( ENoInit )
	: FArray( E_NoInit )
	{}
	~TArray()
	{
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);
		Remove( 0, ArrayNum );
#if (defined _MSC_VER) && (defined _DEBUG)
		//@todo it were nice if we had a cleaner solution for DebugGet
		volatile const T* Dummy = DebugGet(-1);
	}
	/**
	 * Helper function that can be used inside the debuggers watch window to debug TArrays. E.g. "*Class->Defaults.DebugGet(5)". 
	 *
	 * @param	i	Index
	 * @return		pointer to type T at Index i
	 */
	const T* DebugGet( INT i ) const
	{
		if( i >= 0 )
			return &((T*)Data)[i];
		else
			return NULL;
	}
#else
	}
#endif
	T& operator()( INT i )
	{
		checkSlow(i>=0);
		//checkSlow(i<=ArrayNum);
		checkSlow(ArrayMax>=ArrayNum);
		return ((T*)Data)[i];
	}
	const T& operator()( INT i ) const
	{
		checkSlow(i>=0);
		checkSlow(i<=ArrayNum);
		checkSlow(ArrayMax>=ArrayNum);
		return ((T*)Data)[i];
	}
	T Pop()
	{
		check(ArrayNum>0);
		checkSlow(ArrayMax>=ArrayNum);
		T Result = ((T*)Data)[ArrayNum-1];
		Remove( ArrayNum-1 );
		return Result;
	}
	T& Last( INT c=0 )
	{
		check(c<ArrayNum);
		checkSlow(ArrayMax>=ArrayNum);
		return ((T*)Data)[ArrayNum-c-1];
	}
	const T& Last( INT c=0 ) const
	{
		checkSlow(c<ArrayNum);
		checkSlow(ArrayMax>=ArrayNum);
		return ((T*)Data)[ArrayNum-c-1];
	}
	void Shrink()
	{
		FArray::Shrink( sizeof(T) );
	}
	UBOOL FindItem( const T& Item, INT& Index ) const
	{
		for( Index=0; Index<ArrayNum; Index++ )
			if( (*this)(Index)==Item )
				return 1;
		return 0;
	}
	INT FindItemIndex( const T& Item ) const
	{
		for( INT Index=0; Index<ArrayNum; Index++ )
			if( (*this)(Index)==Item )
				return Index;
		return INDEX_NONE;
	}
	UBOOL ContainsItem( const T& Item ) const
	{
		return ( FindItemIndex(Item) != INDEX_NONE );
	}
	friend FArchive& operator<<( FArchive& Ar, TArray& A )
	{
		A.CountBytes( Ar );
		if( sizeof(T)==1 )
		{
			// Serialize simple bytes which require no construction or destruction.
			Ar << A.ArrayNum;
			if( Ar.IsLoading() )
			{
				A.ArrayMax = A.ArrayNum;
				A.Realloc( sizeof(T) );
			}
			Ar.Serialize( &A(0), A.Num() );
		}
		else if( Ar.IsLoading() )
		{
			// Load array.
			INT NewNum;
			Ar << NewNum;
			A.Empty( NewNum );
			for( INT i=0; i<NewNum; i++ )
				Ar << *new(A)T;
		}
		else
		{
			// Save array.
			Ar << A.ArrayNum;
			for( INT i=0; i<A.ArrayNum; i++ )
				Ar << A( i );
		}
		return Ar;
	}
	void CountBytes( FArchive& Ar )
	{
		FArray::CountBytes( Ar, sizeof(T) );
	}

	// Add, Insert, Remove, Empty interface.
	INT Add( INT n=1 )
	{
		checkSlow(!TTypeInfo<T>::DefinitelyNeedsDestructor());
		return FArray::Add( n, sizeof(T) );
	}
	void Insert( INT Index, INT Count=1 )
	{
		checkSlow(!TTypeInfo<T>::DefinitelyNeedsDestructor());
		FArray::Insert( Index, Count, sizeof(T) );
	}
	void InsertZeroed( INT Index, INT Count=1 )
	{
		checkSlow(!TTypeInfo<T>::DefinitelyNeedsDestructor());
		FArray::InsertZeroed( Index, Count, sizeof(T) );
	}
	void Remove( INT Index, INT Count=1 )
	{
		check(Index>=0);
		check(Index<=ArrayNum);
		check(Index+Count<=ArrayNum);
		if( TTypeInfo<T>::NeedsDestructor() )
			for( INT i=Index; i<Index+Count; i++ )
				(&(*this)(i))->~T();
		FArray::Remove( Index, Count, sizeof(T) );
	}
	void Empty( INT Slack=0 )
	{
		if( TTypeInfo<T>::NeedsDestructor() )
			for( INT i=0; i<ArrayNum; i++ )
				(&(*this)(i))->~T();
		FArray::Empty( sizeof(T), Slack );
	}

	// Functions dependent on Add, Remove.
	TArray& operator+( const TArray& Other )
	{
		if( this != &Other )
		{
			for( INT i=0; i<Other.ArrayNum; i++ )
				new( *this )T( Other(i) );
		}
		return *this;
	}
	TArray& operator+=( const TArray& Other )
	{
		if( this != &Other )
		{
			*this = *this + Other;
		}
		return *this;
	}
	TArray& operator=( const TArray& Other )
	{
		if( this != &Other )
		{
			Empty( Other.ArrayNum );
			for( INT i=0; i<Other.ArrayNum; i++ )
				new( *this )T( Other(i) );
		}
		return *this;
	}
	INT AddItem( const T& Item )
	{
		new(*this) T(Item);
		return Num() - 1;
	}
	INT AddZeroed( INT n=1 )
	{
		return FArray::AddZeroed( sizeof(T), n );
	}
	INT AddUniqueItem( const T& Item )
	{
		for( INT Index=0; Index<ArrayNum; Index++ )
			if( (*this)(Index)==Item )
				return Index;
		return AddItem( Item );
	}
	INT RemoveItem( const T& Item )
	{
		INT OriginalNum=ArrayNum;
		for( INT Index=0; Index<ArrayNum; Index++ )
			if( (*this)(Index)==Item )
				Remove( Index-- );
		return OriginalNum - ArrayNum;
	}
	void SwapItems(INT A, INT B)
	{
		FArray::Swap(A,B,sizeof(T));
	}

	// Iterator.
	class TIterator
	{
	public:
		TIterator( TArray<T>& InArray ) : Array(InArray), Index(-1) { ++*this;      }
		void operator++()      { ++Index;                                           }
		void RemoveCurrent()   { Array.Remove(Index--); }
		INT GetIndex()   const { return Index;                                      }
		operator UBOOL() const { return Index < Array.Num();                        }
		T& operator*()   const { return Array(Index);                               }
		T* operator->()  const { return &Array(Index);                              }
		T& GetCurrent()  const { return Array( Index );                             }
		T& GetPrev()     const { return Array( Index ? Index-1 : Array.Num()-1 );   }
		T& GetNext()     const { return Array( Index<Array.Num()-1 ? Index+1 : 0 ); }
	private:
		TArray<T>& Array;
		INT Index;
	};
};

template<class T> class TArrayNoInit : public TArray<T>
{
public:
	TArrayNoInit()
	: TArray<T>(E_NoInit)
	{}
	TArrayNoInit& operator=( const TArrayNoInit& Other )
	{
		TArray<T>::operator=(Other);
		return *this;
	}
};

//
// Array operator news.
//
template <class T> void* operator new( size_t Size, TArray<T>& Array )
{
	INT Index = Array.FArray::Add(1,sizeof(T));
	return &Array(Index);
}
template <class T> void* operator new( size_t Size, TArray<T>& Array, INT Index )
{
	Array.FArray::Insert(Index,1,sizeof(T));
	return &Array(Index);
}

//
// Array exchanger.
//
template <class T> inline void ExchangeArray( TArray<T>& A, TArray<T>& B )
{
	appMemswap( &A, &B, sizeof(FArray) );
}

/*-----------------------------------------------------------------------------
	MRU array.

	Almost the same as TArray except:

	- has an upper limit of the number of items it will store
	- Any item that is added to the array is moved to the top
-----------------------------------------------------------------------------*/

template<class T> class TMRUArray : public TArray<T>
{
public:

	/** The maximum number of items we can store in this array. */
	INT MaxItems;

	TMRUArray<T>()
		:	TArray<T>()
	{
		MaxItems = 0;
	}
	TMRUArray<T>( INT InNum )
		:	TArray<T>( InNum, sizeof(T*) )
	{
		MaxItems = 0;
	}
	TMRUArray<T>( const TMRUArray& Other )
		:	TArray<T>( Other.ArrayNum, sizeof(T*) )
	{
		MaxItems = 0;
	}
	TMRUArray<T>( ENoInit )
		: TArray<T>( E_NoInit )
	{
		MaxItems = 0;
	}
	~TMRUArray()
	{
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);
		Remove( 0, ArrayNum );
	}

	T& operator()( INT i )
	{
		checkSlow(i>=0);
		//checkSlow(i<=ArrayNum);
		checkSlow(ArrayMax>=ArrayNum);
		return ((T*)Data)[i];
	}
	const T& operator()( INT i ) const
	{
		checkSlow(i>=0);
		checkSlow(i<=ArrayNum);
		checkSlow(ArrayMax>=ArrayNum);
		return ((T*)Data)[i];
	}

	void Empty( INT Slack=0 )
	{
		TArray<T>::Empty( Slack );
	}
	INT AddItem( const T& Item )
	{
		INT idx = TArray<T>::AddItem( Item );
		SwapItems( idx, 0 );
		CullArray();
		return 0;
	}
	INT AddZeroed( INT n=1 )
	{
		int idx = TArray<T>::AddZeroed( sizeof(T), n );
		SwapItems( idx, 0 );
		CullArray();
		return 0;
	}
	INT AddUniqueItem( const T& Item )
	{
		for( INT Index=0; Index<ArrayNum; Index++ )
		{
			if( (*this)(Index)==Item )
			{
				SwapItems( Index, 0 );
				return 0;
			}
		}

		Insert( 0 );
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

		while( Num() > MaxItems )
		{
			Remove( Num()-1, 1 );
		}
	}
};

/*-----------------------------------------------------------------------------
	Indirect array.
	Same as a TArray above, but stores pointers to the elements, to allow
	resizing the array index without relocating the actual elements.
-----------------------------------------------------------------------------*/

template<class T> class TIndirectArray : public FArray
{
public:
	typedef T ElementType;
	TIndirectArray()
	:	FArray()
	{}
	TIndirectArray( INT InNum )
	:	FArray( InNum, sizeof(T*) )
	{}
	TIndirectArray( const TIndirectArray& Other )
	:	FArray( Other.ArrayNum, sizeof(T*) )
	{
		ArrayNum=0;
		for( INT i=0; i<Other.ArrayNum; i++ )
			new(*this)T(Other(i));
	}
	TIndirectArray( ENoInit )
	: FArray( E_NoInit )
	{}
	~TIndirectArray()
	{
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);
		Remove( 0, ArrayNum );
	}
    T& operator()( INT i )
	{
		checkSlow(i>=0);
		checkSlow(i<ArrayNum);
		checkSlow(ArrayMax>=ArrayNum);
		return *((T**)Data)[i];
	}
	const T& operator()( INT i ) const
	{
		checkSlow(i>=0);
		checkSlow(i<ArrayNum);
		checkSlow(ArrayMax>=ArrayNum);
		return *((T**)Data)[i];
	}
	void Shrink()
	{
		FArray::Shrink( sizeof(T*) );
	}
	UBOOL FindItem( const T*& Item, INT& Index ) const
	{
		for( Index=0; Index<ArrayNum; Index++ )
			if( *(*this)(Index)==*Item )
				return 1;
		return 0;
	}
	INT FindItemIndex( const T*& Item ) const
	{
		for( INT Index=0; Index<ArrayNum; Index++ )
			if( *(*this)(Index)==*Item )
				return Index;
		return INDEX_NONE;
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
				Ar << *new(A)T;
		}
		else
		{
			// Save array.
			Ar << A.ArrayNum;
			for( INT i=0; i<A.ArrayNum; i++ )
				Ar << A( i );
		}
		return Ar;
	}
	void CountBytes( FArchive& Ar )
	{
		FArray::CountBytes( Ar, sizeof(T*) );
	}
	void Remove( INT Index, INT Count=1 )
	{
		check(Index>=0);
		check(Index<=ArrayNum);
		check(Index+Count<=ArrayNum);
		for( INT i=Index; i<Index+Count; i++ )
			delete ((T**)Data)[i];
		FArray::Remove( Index, Count, sizeof(T*) );
	}
	void Empty( INT Slack=0 )
	{
		for( INT i=0; i<ArrayNum; i++ )
			delete ((T**)Data)[i];
		FArray::Empty( sizeof(T), Slack );
	}
	INT AddRawItem(T* Item)
	{
		INT	Index = FArray::Add(1,sizeof(T*));
		((T**)Data)[Index] = Item;
		return Index;
	}
};

template<class T> void* operator new( size_t Size, TIndirectArray<T>& Array )
{
	INT Index = Array.AddRawItem((T*)appMalloc(Size));
	return &Array(Index);
}

/*-----------------------------------------------------------------------------
	Transactional array.
-----------------------------------------------------------------------------*/

template< class T > class TTransArray : public TArray<T>
{
public:
	// Constructors.
	TTransArray( UObject* InOwner, INT InNum=0 )
	:	TArray<T>( InNum )
	,	Owner( InOwner )
	{
		checkSlow(Owner);
	}
	TTransArray( UObject* InOwner, const TArray<T>& Other )
	:	TArray<T>( Other )
	,	Owner( InOwner )
	{
		checkSlow(Owner);
	}
	TTransArray& operator=( const TTransArray& Other )
	{
		operator=( (const TArray<T>&)Other );
		return *this;
	}

	// Add, Insert, Remove, Empty interface.
	INT Add( INT Count=1 )
	{
		INT Index = TArray<T>::Add( Count );
		if( GUndo )
			GUndo->SaveArray( Owner, this, Index, Count, 1, sizeof(T), SerializeItem, DestructItem );
		return Index;
	}
	void Insert( INT Index, INT Count=1 )
	{
		FArray::Insert( Index, Count, sizeof(T) );
		if( GUndo )
			GUndo->SaveArray( Owner, this, Index, Count, 1, sizeof(T), SerializeItem, DestructItem );
	}
	void Remove( INT Index, INT Count=1 )
	{
		if( GUndo )
			GUndo->SaveArray( Owner, this, Index, Count, -1, sizeof(T), SerializeItem, DestructItem );
		TArray<T>::Remove( Index, Count );
	}
	void Empty( INT Slack=0 )
	{
		if( GUndo )
			GUndo->SaveArray( Owner, this, 0, ArrayNum, -1, sizeof(T), SerializeItem, DestructItem );
		TArray<T>::Empty( Slack );
	}

	// Functions dependent on Add, Remove.
	TTransArray& operator=( const TArray<T>& Other )
	{
		if( this != &Other )
		{
			Empty( Other.Num() );
			for( INT i=0; i<Other.Num(); i++ )
				new( *this )T( Other(i) );
		}
		return *this;
	}
	INT AddItem( const T& Item )
	{
		new(*this) T(Item);
		return Num() - 1;
	}
	INT AddZeroed( INT n=1 )
	{
		INT Index = Add(n);
		appMemzero( &(*this)(Index), n*sizeof(T) );
		return Index;
	}
	INT AddUniqueItem( const T& Item )
	{
		for( INT Index=0; Index<ArrayNum; Index++ )
			if( (*this)(Index)==Item )
				return Index;
		return AddItem( Item );
	}
	INT RemoveItem( const T& Item )
	{
		INT OriginalNum=ArrayNum;
		for( INT Index=0; Index<ArrayNum; Index++ )
			if( (*this)(Index)==Item )
				Remove( Index-- );
		return OriginalNum - ArrayNum;
	}

	// TTransArray interface.
	UObject* GetOwner()
	{
		return Owner;
	}
	void ModifyItem( INT Index )
	{
		if( GUndo )
			GUndo->SaveArray( Owner, this, Index, 1, 0, sizeof(T), SerializeItem, DestructItem );
	}
	void ModifyAllItems()
	{
		if( GUndo )
			GUndo->SaveArray( Owner, this, 0, Num(), 0, sizeof(T), SerializeItem, DestructItem );
	}
	friend FArchive& operator<<( FArchive& Ar, TTransArray& A )
	{
		if( !Ar.IsTrans() )
			Ar << (TArray<T>&)A;
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
	TTransArray( const TArray<T>& Other )
	{}
};

//
// Transactional array operator news.
//
template <class T> void* operator new( size_t Size, TTransArray<T>& Array )
{
	INT Index = Array.Add();
	return &Array(Index);
}
template <class T> void* operator new( size_t Size, TTransArray<T>& Array, INT Index )
{
	Array.Insert(Index);
	return &Array(Index);
}

/*-----------------------------------------------------------------------------
	Lazy loading.
-----------------------------------------------------------------------------*/

//
// Lazy loader base class.
//
class FLazyLoader
{
	friend class ULinkerLoad;
protected:
	FArchive*	 SavedAr;
	INT          SavedPos;
public:
	FLazyLoader()
	: SavedAr( NULL )
	, SavedPos( 0 )
	{}
	virtual void Load()=0;
	virtual void Unload()=0;
	/**
	 * Returns the byte offset to the payload.
	 *
	 * @return offset in bytes from beginning of file to beginning of data
	 */
	virtual DWORD GetOffset() 
	{ 
		return Abs(SavedPos); 
	}
};

//
// Lazy-loadable dynamic array.
//
template <class T> class TLazyArray : public TArray<T>, public FLazyLoader
{
public:
	TLazyArray( INT InNum=0 )
	: TArray<T>( InNum )
	, FLazyLoader()
	{}
	~TLazyArray()
	{
		if( SavedAr )
			SavedAr->DetachLazyLoader( this );
	}
	/**
	 * Returns the byte offset to the payload, skipping the array size serialized
	 * by TArray's serializer.
	 *
	 * @return offset in bytes from beginning of file to beginning of data
	 */
	virtual DWORD GetOffset() 
	{ 
		// Skips array size being serialized.
		return Abs(SavedPos) + sizeof(INT); 
	}
	void Load()
	{
		// Make sure this array is loaded.
		if( SavedPos>0 )
		{
			// Lazy load it now.
			INT PushedPos = SavedAr->Tell();
			SavedAr->Seek( SavedPos );
			*SavedAr << (TArray<T>&)*this;
			SavedPos *= -1;
			SavedAr->Seek( PushedPos );
		}
	}
	void Unload()
	{
		// Make sure this array is unloaded.
		if( SavedPos<0 )
		{
			// Unload it now.
			Empty();
			SavedPos *= -1;
		}
	}
	void Detach()
	{
		if( SavedAr )
			SavedAr->DetachLazyLoader( this );
	}

	friend FArchive& operator<<( FArchive& Ar, TLazyArray& This )
	{
		if( Ar.IsLoading() )
		{
			INT SeekPos=0;
			Ar << SeekPos;
			if( GUglyHackFlags & 8 )
			{
				Ar << (TArray<T>&)This;
			}
			else
			{
				Ar.AttachLazyLoader( &This );
				if( !GLazyLoad )
					This.Load();
			}
			Ar.Seek( SeekPos );
		}
		else if( Ar.IsSaving() )
		{
			// If there's unloaded array data, load it.
			if(This.SavedPos > 0)
				This.Load();

			// Save out count for skipping over this data.
			INT CountPos = Ar.Tell();
			Ar << CountPos << (TArray<T>&)This;
			INT EndPos = Ar.Tell();
			Ar.Seek( CountPos );
			Ar << EndPos;
			Ar.Seek( EndPos );
		}
		else 
		{
			Ar << (TArray<T>&)This;
		}
		return Ar;
	}
};

/*-----------------------------------------------------------------------------
	Dynamic strings.
-----------------------------------------------------------------------------*/

//
// A dynamically sizeable string.
//
class FString : protected TArray<TCHAR>
{
public:
	FString()
	: TArray<TCHAR>()
	{}
	FString( const FString& Other )
	: TArray<TCHAR>( Other.ArrayNum )
	{
		if( ArrayNum )
			appMemcpy( &(*this)(0), &Other(0), ArrayNum*sizeof(TCHAR) );
	}
	FString( const TCHAR* In )
	: TArray<TCHAR>( *In ? (appStrlen(In)+1) : 0 )
	{
		if( ArrayNum )
			appMemcpy( &(*this)(0), In, ArrayNum*sizeof(TCHAR) );
	}
#ifdef UNICODE // separate this out if ANSICHAR != UNICHAR
	FString( const ANSICHAR* In )
	: TArray<TCHAR>( *In ? (strlen(In)+1) : 0 )
    {
		if( ArrayNum )
			appMemcpy(&(*this)(0), ANSI_TO_TCHAR(In), ArrayNum*sizeof(TCHAR));
    }
#endif
	FString( ENoInit )
	: TArray<TCHAR>( E_NoInit )
	{}
	FString& operator=( const TCHAR* Other )
	{
		if( &(*this)(0)!=Other )
		{
			ArrayNum = ArrayMax = *Other ? appStrlen(Other)+1 : 0;
			Realloc( sizeof(TCHAR) );
			if( ArrayNum )
				appMemcpy( &(*this)(0), Other, ArrayNum*sizeof(TCHAR) );
		}
		return *this;
	}
	FString& operator=( const FString& Other )
	{
		if( this != &Other )
		{
			ArrayNum = ArrayMax = Other.Num();
			Realloc( sizeof(TCHAR) );
			if( ArrayNum )
				appMemcpy( &(*this)(0), *Other, ArrayNum*sizeof(TCHAR) );
		}
		return *this;
	}
    TCHAR& operator[]( INT i )
	{
		checkSlow(i>=0);
		checkSlow(i<=ArrayNum);
		checkSlow(ArrayMax>=ArrayNum);
		return ((TCHAR*)Data)[i];
	}
	const TCHAR& operator[]( INT i ) const
	{
		checkSlow(i>=0);
		checkSlow(i<=ArrayNum);
		checkSlow(ArrayMax>=ArrayNum);
		return ((TCHAR*)Data)[i];
	}

	~FString()
	{
		TArray<TCHAR>::Empty();		
	}
	void Empty()
	{
		TArray<TCHAR>::Empty();
	}
	void Shrink()
	{
		TArray<TCHAR>::Shrink();
	}
	const TCHAR* operator*() const
	{
		return Num() ? &(*this)(0) : TEXT("");
	}
	operator UBOOL() const
	{
		return Num()!=0;
	}
	TArray<TCHAR>& GetCharArray()
	{
		//warning: Operations on the TArray<CHAR> can be unsafe, such as adding
		// non-terminating 0's or removing the terminating zero.
		return (TArray<TCHAR>&)*this;
	}
	FString& operator+=( const TCHAR* Str )
	{
		if( *Str != '\0' )
		{
		    if( ArrayNum )
		    {
			    INT Index = ArrayNum-1;
			    Add( appStrlen(Str) );
			    appStrcpy( &(*this)(Index), Str );
		    }
		    else if( *Str )
		    {
			    Add( appStrlen(Str)+1 );
			    appStrcpy( &(*this)(0), Str );
		    }
		}
		return *this;
	}
	FString& operator+=(const TCHAR inChar)
	{
		if (inChar != '\0')
		{
			Add(inChar);
		}
		return *this;
	}
	FString& operator+=( const FString& Str )
	{
		return operator+=( *Str );
	}
	FString operator+( const TCHAR* Str )
	{
		return FString( *this ) += Str;
	}
	FString operator+( const FString& Str )
	{
		return operator+( *Str );
	}
	FString& operator*=( const TCHAR* Str )
	{
		if( ArrayNum>1 && (*this)(ArrayNum-2)!=PATH_SEPARATOR[0] )
			*this += PATH_SEPARATOR;
		return *this += Str;
	}
	FString& operator*=( const FString& Str )
	{
		return operator*=( *Str );
	}
	FString operator*( const TCHAR* Str ) const
	{
		return FString( *this ) *= Str;
	}
	FString operator*( const FString& Str ) const
	{
		return operator*( *Str );
	}
	UBOOL operator<=( const TCHAR* Other ) const
	{
		return !(appStricmp( **this, Other ) > 0);
	}
	UBOOL operator<( const TCHAR* Other ) const
	{
		return appStricmp( **this, Other ) < 0;
	}
	UBOOL operator>=( const TCHAR* Other ) const
	{
		return !(appStricmp( **this, Other ) < 0);
	}
	UBOOL operator>( const TCHAR* Other ) const
	{
		return appStricmp( **this, Other ) > 0;
	}
	UBOOL operator==( const TCHAR* Other ) const
	{
		return appStricmp( **this, Other )==0;
	}
	UBOOL operator==( const FString& Other ) const
	{
		return appStricmp( **this, *Other )==0;
	}
	UBOOL operator!=( const TCHAR* Other ) const
	{
		return appStricmp( **this, Other )!=0;
	}
	UBOOL operator!=( const FString& Other ) const
	{
		return appStricmp( **this, *Other )!=0;
	}
	INT Len() const
	{
		return Num() ? Num()-1 : 0;
	}
	FString Left( INT Count ) const
	{
		return FString( Clamp(Count,0,Len()), **this );
	}
	FString LeftChop( INT Count ) const
	{
		return FString( Clamp(Len()-Count,0,Len()), **this );
	}
	FString Right( INT Count ) const
	{
		return FString( **this + Len()-Clamp(Count,0,Len()) );
	}
	FString Mid( INT Start, INT Count=MAXINT ) const
	{
		DWORD End = Start+Count;
		Start    = Clamp( (DWORD)Start, (DWORD)0,     (DWORD)Len() );
		End      = Clamp( (DWORD)End,   (DWORD)Start, (DWORD)Len() );
		return FString( End-Start, **this + Start );
	}
	INT InStr( const TCHAR* SubStr, UBOOL Right=0 ) const
	{
		if( !Right )
		{
			TCHAR* Tmp = appStrstr(**this,SubStr);
			return Tmp ? (Tmp-**this) : -1;
		}
		else
		{
			for( INT i=Len()-1; i>=0; i-- )
			{
				INT j;
				for( j=0; SubStr[j]; j++ )
					if( (*this)(i+j)!=SubStr[j] )
						break;
				if( !SubStr[j] )
					return i;
			}
			return -1;
		}
	}
	INT InStr( const FString& SubStr, UBOOL Right=0 ) const
	{
		return InStr( *SubStr, Right );
	}
	UBOOL Split( const FString& InS, FString* LeftS, FString* RightS, UBOOL Right=0 ) const
	{
		INT InPos = InStr(InS,Right);
		if( InPos<0 )
			return 0;
		if( LeftS )
			*LeftS = Left(InPos);
		if( RightS )
			*RightS = Mid(InPos+InS.Len());
		return 1;
	}
	FString Caps() const
	{
		FString New( **this );
		for( INT i=0; i<ArrayNum; i++ )
			New(i) = appToUpper(New(i));
		return New;
	}
	FString Locs() const
	{
		FString New( **this );
		for( INT i=0; i<ArrayNum; i++ )
			New(i) = appToLower(New(i));
		return New;
	}
	FString LeftPad( INT ChCount );
	FString RightPad( INT ChCount );
	
	UBOOL IsNumeric();
	
	VARARG_DECL( static FString, static FString, return, Printf, VARARG_NONE, const TCHAR*, VARARG_NONE, VARARG_NONE );

	static FString Chr( TCHAR Ch );
	friend FArchive& operator<<( FArchive& Ar, FString& S );
	friend struct FStringNoInit;
	// Breaks up this delimited string into elements of a string array.
	INT ParseIntoArray( const TCHAR* pchDelim, TArray<FString>* InArray)
	{
		check(InArray);

		FString S = *this;

		for( INT i = S.InStr( pchDelim ) ; i > 0 ; )
		{
			new(*InArray)FString( S.Left(i) );
			S = S.Mid( i + 1, S.Len() );
			i = S.InStr( pchDelim );
		}

		new(*InArray)FString( S );
		return InArray->Num();
	}
	// Reverses the string
	FString Reverse()
	{
		FString New;
		for( int x = Len()-1 ; x > -1 ; x-- )
			New += Mid(x,1);
		*this = New;
		return New;
	}
	// Replace all occurences of a substring.  !! Bad things happen if To contains From.
	FString Replace( const TCHAR* From, const TCHAR* To )
	{
		FString Result = *this;
		INT FromLen = appStrlen(From);
		INT i = Result.InStr(From);
		while( i != -1 )
		{
			Result = Result.Left(i) + To + Result.Mid(i+FromLen);
			i = Result.InStr(From);
		}
		return Result;
	}
	// Takes the number passed in and formats the string in comma format ( 12345 becomes "12,345")
	static FString FormatAsNumber( INT InNumber )
	{
		FString Number = appItoa( InNumber ), Result;

		int dec = 0;
		for( int x = Number.Len()-1 ; x > -1 ; --x )
		{
			Result += Number.Mid(x,1);

			dec++;
			if( dec == 3 && x > 0 )
			{
				Result += TEXT(",");
				dec = 0;
			}
		}

		return Result.Reverse();
	}
private:
	FString( INT InCount, const TCHAR* InSrc )
	:	TArray<TCHAR>( InCount ? InCount+1 : 0 )
	{
		if( ArrayNum )
			appStrncpy( &(*this)(0), InSrc, InCount+1 );
	}
};
struct FStringNoInit : public FString
{
	FStringNoInit()
	: FString( E_NoInit )
	{}
	FStringNoInit& operator=( const TCHAR* Other )
	{
		if( &(*this)(0)!=Other )
		{
			ArrayNum = ArrayMax = *Other ? appStrlen(Other)+1 : 0;
			Realloc( sizeof(TCHAR) );
			if( ArrayNum )
				appMemcpy( &(*this)(0), Other, ArrayNum*sizeof(TCHAR) );
		}
		return *this;
	}
	FStringNoInit& operator=( const FString& Other )
	{
		if( this != &Other )
		{
			ArrayNum = ArrayMax = Other.Num();
			Realloc( sizeof(TCHAR) );
			if( ArrayNum )
				appMemcpy( &(*this)(0), *Other, ArrayNum*sizeof(TCHAR) );
		}
		return *this;
	}
};
inline DWORD GetTypeHash( const FString& S )
{
	return appStrihash(*S);
}
template <> struct TTypeInfo<FString> : public TTypeInfoBase<FString>
{
	typedef const TCHAR* ConstInitType;
	static const TCHAR* ToInit( const FString& In ) {return *In;}
	static UBOOL DefinitelyNeedsDestructor() {return 0;}
};

/*-----------------------------------------------------------------------------
	FFilename.

	Utility class for quick inquiries against filenames.
-----------------------------------------------------------------------------*/

struct FFilename : public FString
{
public:
	FFilename()
		: FString()
	{}
	FFilename( const FString& Other )
		: FString( Other )
	{}
	FFilename( const TCHAR* In )
		: FString( In )
	{}
#ifdef UNICODE // separate this out if ANSICHAR != UNICHAR
	FFilename( const ANSICHAR* In )
		: FString( In )
    {}
#endif
	FFilename( ENoInit )
		: FString( E_NoInit )
	{}

	// Returns the text following the last period.

	FString GetExtension() const
	{
		INT Pos = InStr( TEXT("."), 1 );
		if( Pos != -1 )
			return Mid( Pos+1, Len()-Pos-1 );

		return TEXT("");
	}

	// Returns the base filename, minus any path information.

	FString GetCleanFilename() const
	{
		INT Pos = InStr( TEXT("\\"), 1 );
		if( Pos != -1 )
			return Mid( Pos+1, Len()-Pos-1 );

		return *this;
	}

	// Returns the same thing as GetCleanFilename, but without the extension

	FString GetBaseFilename() const
	{
		FString Wk = GetCleanFilename();

		INT Pos = Wk.InStr( TEXT("."), 1 );
		if( Pos != -1 )
			return Wk.Left( Pos );

		return Wk;
	}

	// Returns the path in front of the filename

	FString GetPath() const
	{
		INT Pos = InStr( TEXT("\\"), 1 );
		if( Pos != -1 )
			return Left( Pos );

		return *this;
	}
};

//
// String exchanger.
//
inline void ExchangeString( FString& A, FString& B )
{
	appMemswap( &A, &B, sizeof(FString) );
}

/*----------------------------------------------------------------------------
	Special archivers.
----------------------------------------------------------------------------*/

//
// String output device.
//
class FStringOutputDevice : public FString, public FOutputDevice
{
public:
	FStringOutputDevice( const TCHAR* InStr=TEXT("") )
	: FString( InStr )
	{}
	void Serialize( const TCHAR* Data, EName Event )
	{
		*this += (TCHAR*)Data;
	}
};

//
// Buffer writer.
//
class FBufferWriter : public FArchive
{
public:
	FBufferWriter( TArray<BYTE>& InBytes )
	: Bytes( InBytes )
	, Pos( 0 )
	{
		ArIsSaving = 1;
	}
	void Serialize( void* InData, INT Length )
	{
		if( Pos+Length>Bytes.Num() )
			Bytes.Add( Pos+Length-Bytes.Num() );
		appMemcpy( &Bytes(Pos), InData, Length );
		Pos += Length;
	}
	INT Tell()
	{
		return Pos;
	}
	void Seek( INT InPos )
	{
		Pos = InPos;
	}
	INT TotalSize()
	{
		return Bytes.Num();
	}
private:
	TArray<BYTE>& Bytes;
	INT Pos;
};

//
// Buffer archiver.
//
class FBufferArchive : public FBufferWriter, public TArray<BYTE>
{
public:
	FBufferArchive()
	: FBufferWriter( (TArray<BYTE>&)*this )
	{}
};

//
// Buffer reader.
//
class FBufferReader : public FArchive
{
public:
	FBufferReader( const TArray<BYTE>& InBytes )
	:	Bytes	( InBytes )
	,	Pos 	( 0 )
	{
		ArIsLoading = ArIsTrans = 1;
	}
	void Serialize( void* Data, INT Num )
	{
		check(Pos>=0);
		check(Pos+Num<=Bytes.Num());
		appMemcpy( Data, &Bytes(Pos), Num );
		Pos += Num;
	}
	INT Tell()
	{
		return Pos;
	}
	INT TotalSize()
	{
		return Bytes.Num();
	}
	void Seek( INT InPos )
	{
		check(InPos>=0);
		check(InPos<=Bytes.Num());
		Pos = InPos;
	}
	UBOOL AtEnd()
	{
		return Pos>=Bytes.Num();
	}
private:
	const TArray<BYTE>& Bytes;
	INT Pos;
};

/*----------------------------------------------------------------------------
	TMap.
----------------------------------------------------------------------------*/

//
// Maps unique keys to values.
//
template< class TK, class TI > class TMapBase
{
protected:
	class TPair
	{
	public:
		INT HashNext;
		TK Key;
		TI Value;
		TPair( typename TTypeInfo<TK>::ConstInitType InKey, typename TTypeInfo<TI>::ConstInitType InValue )
		: Key( InKey ), Value( InValue )
		{}
		TPair()
		{}
		friend FArchive& operator<<( FArchive& Ar, TPair& F )
		{
			return Ar << F.Key << F.Value;
		}
	};
	void Rehash()
	{
		checkSlow(!(HashCount&(HashCount-1)));
		INT* NewHash = new INT[HashCount];
		{for( INT i=0; i<HashCount; i++ )
		{
			NewHash[i] = INDEX_NONE;
		}}
		{for( INT i=0; i<Pairs.Num(); i++ )
		{
			TPair& Pair    = Pairs(i);
			INT    iHash   = (GetTypeHash(Pair.Key) & (HashCount-1));
			Pair.HashNext  = NewHash[iHash];
			NewHash[iHash] = i;
		}}
		if( Hash )
			delete[] Hash;
		Hash = NewHash;
	}
	FORCEINLINE void AllocHash()
	{
		if(!Hash)
			Rehash();
	}
	void Relax()
	{
		while( HashCount>Pairs.Num()*2+8 )
			HashCount /= 2;
		Rehash();
	}
	TI& Add( typename TTypeInfo<TK>::ConstInitType InKey, typename TTypeInfo<TI>::ConstInitType InValue )
	{
		TPair& Pair   = *new(Pairs)TPair( InKey, InValue );
		INT    iHash  = (GetTypeHash(Pair.Key) & (HashCount-1));
		Pair.HashNext = Hash[iHash];
		Hash[iHash]   = Pairs.Num()-1;
		if( HashCount*2 + 8 < Pairs.Num() )
		{
			HashCount *= 2;
			Rehash();
		}
		return Pair.Value;
	}
	TArray<TPair> Pairs;
	INT* Hash;
	INT HashCount;
public:
	TMapBase()
	:	Hash( NULL )
	,	HashCount( 8 )
	{}
	TMapBase( const TMapBase& Other )
	:	Pairs( Other.Pairs )
	,	HashCount( Other.HashCount )
	,	Hash( NULL )
	{
		Rehash();
	}
	~TMapBase()
	{
		if( Hash )
			delete Hash;
		Hash = NULL;
		HashCount = 0;
	}
	TMapBase& operator=( const TMapBase& Other )
	{
		Pairs     = Other.Pairs;
		HashCount = Other.HashCount;
		Rehash();
		return *this;
	}
	void Empty()
	{
		checkSlow(!(HashCount&(HashCount-1)));
		Pairs.Empty();
		HashCount = 8;
		Rehash();
	}

	TI& Set( typename TTypeInfo<TK>::ConstInitType InKey, typename TTypeInfo<TI>::ConstInitType InValue )
	{
		AllocHash();
		for( INT i=Hash[(GetTypeHash(InKey) & (HashCount-1))]; i!=INDEX_NONE; i=Pairs(i).HashNext )
			if( Pairs(i).Key==InKey )
				{Pairs(i).Value=InValue; return Pairs(i).Value;}
		return Add( InKey, InValue );
	}
	INT Remove( typename TTypeInfo<TK>::ConstInitType InKey )
	{
		INT Count=0;
		for( INT i=Pairs.Num()-1; i>=0; i-- )
			if( Pairs(i).Key==InKey )
				{Pairs.Remove(i); Count++;}
		if( Count )
			Relax();
		return Count;
	}
	// This is slow, but it's good for specific situations
	TK* FindKey( const TI& Value )
	{
		for( INT i = 0 ; i < Pairs.Num() ; ++i )
			if( Pairs(i).Value == Value )
				return &Pairs(i).Key;
		return NULL;
	}
	TI* Find( const TK& Key )
	{
		AllocHash();
		for( INT i=Hash[(GetTypeHash(Key) & (HashCount-1))]; i!=INDEX_NONE; i=Pairs(i).HashNext )
			if( Pairs(i).Key==Key )
				return &Pairs(i).Value;
		return NULL;
	}
	TI FindRef( const TK& Key ) const
	{
		if(Hash)
		{
			for( INT i=Hash[(GetTypeHash(Key) & (HashCount-1))]; i!=INDEX_NONE; i=Pairs(i).HashNext )
				if( Pairs(i).Key==Key )
					return Pairs(i).Value;
		}
		return (TI) NULL;
	}
	const TI* Find( const TK& Key ) const
	{
		if(Hash)
		{
			for( INT i=Hash[(GetTypeHash(Key) & (HashCount-1))]; i!=INDEX_NONE; i=Pairs(i).HashNext )
				if( Pairs(i).Key==Key )
					return &Pairs(i).Value;
		}
		return NULL;
	}
	friend FArchive& operator<<( FArchive& Ar, TMapBase& M )
	{
		Ar << M.Pairs;
		if( Ar.IsLoading() )
			M.Rehash();
		return Ar;
	}
	void Dump( FOutputDevice& Ar )
	{
		AllocHash();
		Ar.Logf( TEXT("TMapBase: %i items, %i hash slots"), Pairs.Num(), HashCount );
		for( INT i=0; i<HashCount; i++ )
		{
			INT c=0;
			for( INT j=Hash[i]; j!=INDEX_NONE; j=Pairs(j).HashNext )
				c++;
			Ar.Logf( TEXT("   Hash[%i] = %i"), i, c );
		}
	}
	class TIterator
	{
	public:
		TIterator( TMapBase& InMap ) : Map( InMap ), Pairs( InMap.Pairs ), Index( 0 ), Removed(0) {}
		~TIterator()               { if( Removed ) Map.Relax(); }
		void operator++()          { ++Index; }
		void RemoveCurrent()       { Pairs.Remove(Index--); Removed++; }
		operator UBOOL() const     { return Index<Pairs.Num(); }
		TK& Key() const            { return Pairs(Index).Key; }
		TI& Value() const          { return Pairs(Index).Value; }
	private:
		TMapBase& Map;
		TArray<TPair>& Pairs;
		INT Index;
		INT Removed;
	};
	class TConstIterator
	{
	public:
		TConstIterator( const TMapBase& InMap ) : Map(InMap), Pairs( InMap.Pairs ), Index( 0 ) {}
		~TConstIterator() {}
		void operator++()          { ++Index; }
		operator UBOOL() const     { return Index < Pairs.Num(); }
		const TK& Key() const      { return Pairs(Index).Key; }
		const TI& Value() const    { return Pairs(Index).Value; }
	private:
		const TMapBase& Map;
		const TArray<TPair>& Pairs;
		INT Index;
	};
	friend class TIterator;
	friend class TConstIterator;
};

template< class TK, class TI > class TMap : public TMapBase<TK,TI>
{
public:
	TMap& operator=( const TMap& Other )
	{
		TMapBase<TK,TI>::operator=( Other );
		return *this;
	}

	int Num()
	{
		return Pairs.Num();
	}
};
template< class TK, class TI > class TMultiMap : public TMapBase<TK,TI>
{
public:
	TMultiMap& operator=( const TMultiMap& Other )
	{
		TMapBase<TK,TI>::operator=( Other );
		return *this;
	}
	void MultiFind( const TK& Key, TArray<TI>& Values ) const
	{
		if(Hash)
		{
			for( INT i=Hash[(GetTypeHash(Key) & (HashCount-1))]; i!=INDEX_NONE; i=Pairs(i).HashNext )
				if( Pairs(i).Key==Key )
					new(Values)TI(Pairs(i).Value);
		}
	}
	TI& Add( typename TTypeInfo<TK>::ConstInitType InKey, typename TTypeInfo<TI>::ConstInitType InValue )
	{
		AllocHash();
		return TMapBase<TK,TI>::Add( InKey, InValue );
	}
	TI& AddUnique( typename TTypeInfo<TK>::ConstInitType InKey, typename TTypeInfo<TI>::ConstInitType InValue )
	{
		AllocHash();
		for( INT i=Hash[(GetTypeHash(InKey) & (HashCount-1))]; i!=INDEX_NONE; i=Pairs(i).HashNext )
			if( Pairs(i).Key==InKey && Pairs(i).Value==InValue )
				return Pairs(i).Value;
		return Add( InKey, InValue );
	}
	INT RemovePair( typename TTypeInfo<TK>::ConstInitType InKey, typename TTypeInfo<TI>::ConstInitType InValue )
	{
		INT Count=0;
		for( INT i=Pairs.Num()-1; i>=0; i-- )
			if( Pairs(i).Key==InKey && Pairs(i).Value==InValue )
				{Pairs.Remove(i); Count++;}
		if( Count )
			Relax();
		return Count;
	}
	TI* FindPair( const TK& Key, const TK& Value ) const
	{
		if(Hash)
		{
			for( INT i=Hash[(GetTypeHash(Key) & (HashCount-1))]; i!=INDEX_NONE; i=Pairs(i).HashNext )
				if( Pairs(i).Key==Key && Pairs(i).Value==Value )
					return &Pairs(i).Value;
		}
		return NULL;
	}
};

/**
 * A mapping from keys to values.  Like TMap, but optimized for dynamically
 * adding and removing values at the cost of a little additional memory
 * overhead.
 */
template<class KeyType,class ValueType> class TDynamicMap
{
protected:
	struct FPair
	{
		KeyType		Key;
		ValueType	Value;

		FPair*		HashNext;
		UBOOL		Free;

		FPair(typename TTypeInfo<KeyType>::ConstInitType InKey,typename TTypeInfo<ValueType>::ConstInitType InValue):
			Key(InKey),
			Value(InValue),
			HashNext(NULL),
			Free(0)
		{}
		FPair():
			HashNext(NULL)
		{}
		friend FArchive& operator<<(FArchive& Ar,FPair& Pair)
		{
			return Ar << Pair.Key << Pair.Value;
		}
	};
	FPair**					Hash;
	INT						HashSize;
	TIndirectArray<FPair>	Pairs;
	FPair*					FreePairs;
	UBOOL AllocHash()
	{
		INT	NewHashSize = 1 << appCeilLogTwo(Pairs.Num() / 8 + 8);
		if(NewHashSize != HashSize || !Hash)
		{
			FPair**	NewHash = new FPair*[NewHashSize];
			appMemzero(NewHash,sizeof(FPair*) * NewHashSize);

			for(INT PairIndex = 0;PairIndex < Pairs.Num();PairIndex++)
			{
				FPair&	Pair = Pairs(PairIndex);
				if(!Pair.Free)
				{
					INT	HashIndex = (GetTypeHash(Pair.Key) & (NewHashSize - 1));
					Pair.HashNext = NewHash[HashIndex];
					NewHash[HashIndex] = &Pair;
				}
			}

			delete [] Hash;
			Hash = NewHash;
			HashSize = NewHashSize;
			return 1;
		}
		else
			return 0;
	}
	ValueType& Add(typename TTypeInfo<KeyType>::ConstInitType InKey,typename TTypeInfo<ValueType>::ConstInitType InValue)
	{
		FPair*	Pair;
		if(FreePairs)
		{
			Pair = FreePairs;
			FreePairs = FreePairs->HashNext;
			Pair->Free = 0;
			Pair->Key = InKey;
			Pair->Value = InValue;
		}
		else
			Pair = new(Pairs)FPair(InKey,InValue);

		if(!AllocHash())
		{
			INT	HashIndex = GetTypeHash(Pair->Key) & (HashSize - 1);
			Pair->HashNext = Hash[HashIndex];
			Hash[HashIndex] = Pair;
		}

		return Pair->Value;
	}
	void RemoveIndex(INT PairIndex)
	{
		if(Hash)
		{
			FPair**	PrevHashNext = &Hash[GetTypeHash(Pairs(PairIndex).Key) & (HashSize - 1)];
			for(FPair* HashPair = *PrevHashNext;HashPair;PrevHashNext = &HashPair->HashNext,HashPair = HashPair->HashNext)
			{
				if(HashPair->Key == Pairs(PairIndex).Key)
				{
					*PrevHashNext = HashPair->HashNext;
					HashPair->HashNext = FreePairs;
					FreePairs = HashPair;
					HashPair->Free = 1;
					break;
				}
			}
		}
	}
public:
	TDynamicMap():
		Hash(NULL),
		HashSize(0),
		FreePairs(NULL)
	{}
	void Shrink()
	{
		FreePairs = NULL;
		for(INT PairIndex = Pairs.Num() - 1;PairIndex >= 0;PairIndex--)
			if(Pairs(PairIndex).Free)
				Pairs.Remove(PairIndex);
	}
	void Empty()
	{
		Pairs.Empty();
		FreePairs = NULL;
		HashSize = 0;
		AllocHash();
	}
	ValueType& Set(typename TTypeInfo<KeyType>::ConstInitType InKey,typename TTypeInfo<ValueType>::ConstInitType InValue)
	{
		if(Hash)
		{
			for(FPair* HashPair = Hash[GetTypeHash(InKey) & (HashSize - 1)];HashPair;HashPair = HashPair->HashNext)
			{
				if(HashPair->Key == InKey)
				{
					HashPair->Value = InValue;
					return HashPair->Value;
				}
			}
		}
		return Add(InKey,InValue);
	}
	void Remove(typename TTypeInfo<KeyType>::ConstInitType InKey)
	{
		if(Hash)
		{
			FPair**	PrevHashNext = &Hash[GetTypeHash(InKey) & (HashSize - 1)];
			for(FPair* HashPair = *PrevHashNext;HashPair;PrevHashNext = &HashPair->HashNext,HashPair = HashPair->HashNext)
			{
				if(HashPair->Key == InKey)
				{
					*PrevHashNext = HashPair->HashNext;
					HashPair->HashNext = FreePairs;
					FreePairs = HashPair;
					HashPair->Free = 1;
					break;
				}
			}
		}
	}
	const KeyType* FindKey(const ValueType& Value) const
	{
		for(INT PairIndex = 0;PairIndex < Pairs.Num();PairIndex++)
			if(Pairs(PairIndex).Value == Value)
				return &Pairs(PairIndex).Key;
		return NULL;
	}
	ValueType FindRef(const KeyType& InKey) const
	{
		if(Hash)
		{
			for(FPair* HashPair = Hash[GetTypeHash(InKey) & (HashSize - 1)];HashPair;HashPair = HashPair->HashNext)
			{
				if(HashPair->Key == InKey)
				{
					return HashPair->Value;
				}
			}
		}
		return (ValueType) NULL;
	}
	ValueType* Find(const KeyType& InKey)
	{
		if(Hash)
		{
			for(FPair* HashPair = Hash[GetTypeHash(InKey) & (HashSize - 1)];HashPair;HashPair = HashPair->HashNext)
			{
				if(HashPair->Key == InKey)
				{
					return &HashPair->Value;
				}
			}
		}
		return NULL;
	}
	const ValueType* Find(const KeyType& InKey) const
	{
		if(Hash)
		{
			for(const FPair* HashPair = Hash[GetTypeHash(InKey) & (HashSize - 1)];HashPair;HashPair = HashPair->HashNext)
			{
				if(HashPair->Key == InKey)
				{
					return &HashPair->Value;
				}
			}
		}
		return NULL;
	}
	friend FArchive& operator<<(FArchive& Ar,TDynamicMap& M)
	{
		if(Ar.IsSaving())
			M.Shrink();
		Ar << M.Pairs;
		if( Ar.IsLoading() )
		{
			delete M.Hash;
			M.AllocHash();
		}
		return Ar;
	}
	void Dump( FOutputDevice& Ar ) const
	{
		Ar.Logf(TEXT("TDynamicMap: %i items, %i hash slots"),Pairs.Num(),HashSize);
		for(INT HashIndex = 0;HashIndex < HashSize;HashIndex++)
		{
			INT	Depth = 0;
			for(const FPair* HashPair = Hash[HashIndex];HashPair;HashPair = HashPair->HashNext)
				Depth++;
			Ar.Logf(TEXT("\tHash[%i] = %i"),HashIndex,Depth);
		}
	}
	class TIterator
	{
	public:
		TIterator(TDynamicMap& InMap):
			Map(InMap),
			Pairs(InMap.Pairs),
			Index(-1)
		{
			++(*this);
		}
		void operator++()
		{
			do Index++; while(Index < Pairs.Num() && Pairs(Index).Free);
		}
		void RemoveCurrent()
		{
			Map.RemoveIndex(Index--);
		}
		operator UBOOL() const     { return Index<Pairs.Num(); }
		KeyType& Key() const       { return Pairs(Index).Key; }
		ValueType& Value() const   { return Pairs(Index).Value; }
	private:
		TDynamicMap& Map;
		TIndirectArray<FPair>& Pairs;
		INT Index;
		INT Removed;
	};
	class TConstIterator
	{
	public:
		TConstIterator(const TDynamicMap& InMap):
			Map(InMap),
			Pairs(InMap.Pairs),
			Index(-1)
		{
			++(*this);
		}
		void operator++()
		{
			do Index++; while(Index < Pairs.Num() && Pairs(Index).Free);
		}
		operator UBOOL() const     { return Index < Pairs.Num(); }
		KeyType& Key() const       { return Pairs(Index).Key; }
		ValueType& Value() const   { return Pairs(Index).Value; }
	private:
		const TDynamicMap& Map;
		const TIndirectArray<FPair>& Pairs;
		INT Index;
	};
	friend class TIterator;
	friend class TConstIterator;
};

/*----------------------------------------------------------------------------
	Sorting template.
----------------------------------------------------------------------------*/

#define IMPLEMENT_COMPARE_POINTER( Type, Filename, FunctionBody )		\
	class Compare##Filename##Type										\
	{																	\
	public:																\
		static inline INT Compare( Type* A, Type* B	)					\
			FunctionBody												\
	};

#define IMPLEMENT_COMPARE_CONSTREF( Type, Filename, FunctionBody )		\
	class Compare##Filename##Type										\
	{																	\
	public:																\
		static inline INT Compare( const Type& A, const Type& B	)		\
			FunctionBody												\
	};

#define USE_COMPARE_POINTER( Type, Filename )	Type*,Compare##Filename##Type
#define USE_COMPARE_CONSTREF( Type, Filename )	Type,Compare##Filename##Type


//
// Sort elements. The sort is unstable, meaning that the ordering of equal 
// items is not necessarily preserved.
//
template<class T> struct TStack
{
	T* Min;
	T* Max;
};
template<class T, class CompareClass> void Sort( T* First, INT Num )
{
	if( Num<2 )
		return;
	TStack<T> RecursionStack[32]={{First,First+Num-1}}, Current, Inner;
	for( TStack<T>* StackTop=RecursionStack; StackTop>=RecursionStack; --StackTop )
	{
		Current = *StackTop;
	Loop:
		INT Count = Current.Max - Current.Min + 1;
		if( Count <= 8 )
		{
			// Use simple bubble-sort.
			while( Current.Max > Current.Min )
			{
				T *Max, *Item;
				for( Max=Current.Min, Item=Current.Min+1; Item<=Current.Max; Item++ )
					if( CompareClass::Compare(*Item, *Max) > 0 )
						Max = Item;
				Exchange( *Max, *Current.Max-- );
			}
		}
		else
		{
			// Grab middle element so sort doesn't exhibit worst-cast behaviour with presorted lists.
			Exchange( Current.Min[Count/2], Current.Min[0] );

			// Divide list into two halves, one with items <=Current.Min, the other with items >Current.Max.
			Inner.Min = Current.Min;
			Inner.Max = Current.Max+1;
			for( ; ; )
			{
				while( ++Inner.Min<=Current.Max && CompareClass::Compare(*Inner.Min, *Current.Min) <= 0 );
				while( --Inner.Max> Current.Min && CompareClass::Compare(*Inner.Max, *Current.Min) >= 0 );
				if( Inner.Min>Inner.Max )
					break;
				Exchange( *Inner.Min, *Inner.Max );
			}
			Exchange( *Current.Min, *Inner.Max );

			// Save big half and recurse with small half.
			if( Inner.Max-1-Current.Min >= Current.Max-Inner.Min )
			{
				if( Current.Min+1 < Inner.Max )
				{
					StackTop->Min = Current.Min;
					StackTop->Max = Inner.Max - 1;
					StackTop++;
				}
				if( Current.Max>Inner.Min )
				{
					Current.Min = Inner.Min;
					goto Loop;
				}
			}
			else
			{
				if( Current.Max>Inner.Min )
				{
					StackTop->Min = Inner  .Min;
					StackTop->Max = Current.Max;
					StackTop++;
				}
				if( Current.Min+1<Inner.Max )
				{
					Current.Max = Inner.Max - 1;
					goto Loop;
				}
			}
		}
	}
}

/**
 * Encapsulates a link in a doubly linked list.
 */
template<class ElementType> class TDoubleLinkedList
{
public:

	/**
	 * Used to iterate over the elements of a linked list.
	 */
	class TIterator
	{
	public:

		TIterator(TDoubleLinkedList* FirstLink):
			CurrentLink(FirstLink)
		{}

		/**
		 * Advances the iteration to the next element.
		 */
		void Next()
		{
			checkSlow(CurrentLink);
			CurrentLink = CurrentLink->Next;
		}

		/**
		 * Checks for the end of the list.
		 */
		operator UBOOL() const { return CurrentLink != NULL; }

		// Accessors.
		ElementType& operator->() const
		{
			checkSlow(CurrentLink);
			return CurrentLink->Element;
		}
		ElementType& operator*() const
		{
			checkSlow(CurrentLink);
			return CurrentLink->Element;
		}

	private:

		TDoubleLinkedList* CurrentLink;
	};

	// Constructors.
	TDoubleLinkedList():
		Next(NULL),
		PrevLink(NULL)
	{}
	TDoubleLinkedList(const ElementType& InElement):
		Element(InElement),
		Next(NULL),
		PrevLink(NULL)
	{}

	/**
	 * Removes this element from the list in constant time.
	 */
	void Unlink()
	{
		if( Next )
			Next->PrevLink = PrevLink;
		*PrevLink = Next;
	}

	/**
	 * Adds this element to a list, before the given element.
	 *
	 * @param Before	The link to insert this element before.
	 */
	void Link(TDoubleLinkedList*& Before)
	{
		if(Before)
			Before->PrevLink = &Next;
		Next = Before;
		PrevLink = &Before;
		Before = this;
	}

	// Accessors.
	ElementType& operator->()
	{
		return Element;
	}
	const ElementType& operator->() const
	{
		return Element;
	}
	ElementType& operator*()
	{
		return Element;
	}
	const ElementType& operator*() const
	{
		return Element;
	}

private:
	ElementType Element;
	TDoubleLinkedList* Next;
	TDoubleLinkedList** PrevLink;
};

/*----------------------------------------------------------------------------
	TList.
----------------------------------------------------------------------------*/

//
// Simple single-linked list template.
//
template <class ElementType> class TList
{
public:

	ElementType			Element;
	TList<ElementType>*	Next;

	// Constructor.

	TList(ElementType InElement,TList<ElementType>* InNext = NULL)
	{
		Element = InElement;
		Next = InNext;
	}
};

/*----------------------------------------------------------------------------
	FRainbowPtr.
----------------------------------------------------------------------------*/

//
// A union of pointers of all base types.
//
union FRainbowPtr
{
	// All pointers.
	void*  PtrVOID;
	BYTE*  PtrBYTE;
	_WORD* PtrWORD;
	DWORD* PtrDWORD;
	QWORD* PtrQWORD;
	FLOAT* PtrFLOAT;

	// Conversion constructors.
	FRainbowPtr() {}
	FRainbowPtr( void* Ptr ) : PtrVOID(Ptr) {};
};

//
//	TStaticArray
//

template<class Type,UINT Max> struct TStaticArray
{
	Type	Elements[Max];
	UINT	Num;

	// Constructor.

	TStaticArray(): Num(0) {}

	// AddItem

	UINT AddItem(const Type& Item)
	{
		checkSlow(Num < Max);
		Elements[Num] = Item;
		return Num++;
	}

	// operator()

	Type& operator()(UINT Index)
	{
		checkSlow(Index < Num);
		return Elements[Index];
	}
};



/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
