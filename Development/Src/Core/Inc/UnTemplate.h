/*=============================================================================
	UnTemplate.h: Unreal common template definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNTEMPLATE_H__
#define __UNTEMPLATE_H__

//
// Hash functions for common types.
//

inline DWORD GetTypeHash( const BYTE A )
{
	return A;
}
inline DWORD GetTypeHash( const SBYTE A )
{
	return A;
}
inline DWORD GetTypeHash( const WORD A )
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
#if !PLATFORM_MACOSX
inline DWORD GetTypeHash( const UINT A )
{
	return A;
}
#endif
// @todo flash: should probably just mimic MACOS above
#if FLASH
inline DWORD GetTypeHash( const long int A )
#else
inline DWORD GetTypeHash( const DWORD A )
#endif
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

inline DWORD GetTypeHash( const void* A )
{
	return (DWORD)(PTRINT)A;
}

inline DWORD GetTypeHash( void* A )
{
	return (DWORD)(PTRINT)A;
}

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
/**
 * Aligns a value to the nearest higher multiple of 'Alignment', which must be a power of two.
 *
 * @param Ptr			Value to align
 * @param Alignment		Alignment, must be a power of two
 * @return				Aligned value
 */
template< class T > inline T Align( const T Ptr, INT Alignment )
{
	return (T)(((PTRINT)Ptr + Alignment - 1) & ~(Alignment-1));
}
/**
 * Aligns a value to the nearest higher multiple of 'Alignment'.
 *
 * @param Ptr			Value to align
 * @param Alignment		Alignment, can be any arbitrary DWORD
 * @return				Aligned value
 */
template< class T > inline T AlignArbitrary( const T Ptr, DWORD Alignment )
{
	return (T) ( ( ((UPTRINT)Ptr + Alignment - 1) / Alignment ) * Alignment );
}
template< class T > inline void Swap( T& A, T& B )
{
	const T Temp = A;
	A = B;
	B = Temp;
}
template< class T > inline void Exchange( T& A, T& B )
{
	Swap(A, B);
}

/**
 * Chooses between the two parameters based on whether the first is NULL or not.
 * @return If the first parameter provided is non-NULL, it is returned; otherwise the second parameter is returned.
 */
template<typename ReferencedType>
FORCEINLINE ReferencedType* IfAThenAElseB(ReferencedType* A,ReferencedType* B)
{
	const PTRINT IntA = reinterpret_cast<PTRINT>(A);
	const PTRINT IntB = reinterpret_cast<PTRINT>(B);

	// Compute a mask which has all bits set if IntA is zero, and no bits set if it's non-zero.
	const PTRINT MaskB = -(!IntA);

	return reinterpret_cast<ReferencedType*>(IntA | (MaskB & IntB));
}

/** branchless pointer selection based on predicate
* return PTRINT(Predicate) ? A : B;
**/
template<typename PredicateType,typename ReferencedType>
FORCEINLINE ReferencedType* IfPThenAElseB(PredicateType Predicate,ReferencedType* A,ReferencedType* B)
{
	const PTRINT IntA = reinterpret_cast<PTRINT>(A);
	const PTRINT IntB = reinterpret_cast<PTRINT>(B);

	// Compute a mask which has all bits set if Predicate is zero, and no bits set if it's non-zero.
	const PTRINT MaskB = -(!PTRINT(Predicate));

	return reinterpret_cast<ReferencedType*>((IntA & ~MaskB) | (IntB & MaskB));
}


/** This is used to provide type specific behavior for a move which may change the value of B. */
template<typename T> void Move(T& A,typename TContainerTraits<T>::ConstInitType B)
{
	// Destruct the previous value of A.
	A.~T();

	// Use placement new and a copy constructor so types with const members will work.
	new(&A) T(B);
}

template< class T, class U > FORCEINLINE_DEBUGGABLE T Lerp( const T& A, const T& B, const U& Alpha )
{
	return (T)(A + Alpha * (B-A));
}

template< class T, class U > FORCEINLINE_DEBUGGABLE T BiLerp(const T& P00,const T& P10,const T& P01,const T& P11, const U& FracX, const U& FracY)
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
template< class T, class U > FORCEINLINE_DEBUGGABLE T CubicInterp( const T& P0, const T& T0, const T& P1, const T& T1, const U& A )
{
	const FLOAT A2 = A  * A;
	const FLOAT A3 = A2 * A;

	return (T)(((2*A3)-(3*A2)+1) * P0) + ((A3-(2*A2)+A) * T0) + ((A3-A2) * T1) + (((-2*A3)+(3*A2)) * P1);
}

template< class T, class U > FORCEINLINE_DEBUGGABLE T CubicInterpDerivative( const T& P0, const T& T0, const T& P1, const T& T1, const U& A )
{
	T a = 6.f*P0 + 3.f*T0 + 3.f*T1 - 6.f*P1;
	T b = -6.f*P0 - 4.f*T0 - 2.f*T1 + 6.f*P1;
	T c = T0;

	const FLOAT A2 = A  * A;

	return (a * A2) + (b * A) + c;
}

template< class T, class U > FORCEINLINE_DEBUGGABLE T CubicInterpSecondDerivative( const T& P0, const T& T0, const T& P1, const T& T1, const U& A )
{
	T a = 12.f*P0 + 6.f*T0 + 6.f*T1 - 12.f*P1;
	T b = -6.f*P0 - 4.f*T0 - 2.f*T1 + 6.f*P1;

	return (a * A) + b;
}

/** Interpolate between A and B, applying an ease in/out function.  Exp controls the degree of the curve. */
template< class T > FORCEINLINE_DEBUGGABLE T InterpEaseInOut( const T& A, const T& B, FLOAT Alpha, FLOAT Exp )
{
	FLOAT const ModifiedAlpha = ( Alpha < 0.5f ) ?
		0.5f * appPow(2.f * Alpha, Exp) :
		1.f - 0.5f * appPow(2.f * (1.f - Alpha), Exp);
	
	return Lerp<T>(A, B, ModifiedAlpha);
}

/*----------------------------------------------------------------------------
	FLOAT specialization of templates.
----------------------------------------------------------------------------*/

#if PS3
template<> FORCEINLINE FLOAT Abs( const FLOAT A )
{
	return __fabsf( A );
}
#else
template<> FORCEINLINE FLOAT Abs( const FLOAT A )
{
	return fabsf( A );
}
#endif

#if XBOX
template<> FORCEINLINE FLOAT Max( const FLOAT A, const FLOAT B )
{
	return __fsel( A - B, A, B );
}
template<> FORCEINLINE FLOAT Min( const FLOAT A, const FLOAT B )
{
	return __fsel( A - B, B, A );
}
//@todo optimization: the below causes crashes in release mode when compiled with VS.NET 2003
#elif __HAS_SSE__ && 0
template<> FORCEINLINE FLOAT Max( const FLOAT A, const FLOAT B )
{
	return _mm_max_ss( _mm_set_ss(A), _mm_set_ss(B) ).m128_f32[0];
}
template<> FORCEINLINE FLOAT Min( const FLOAT A, const FLOAT B )
{
	return _mm_max_ss( _mm_set_ss(A), _mm_set_ss(B) ).m128_f32[0];
}
#elif PS3
template<> FORCEINLINE FLOAT Max( const FLOAT A, const FLOAT B )
{
	return __fsels( A - B, A, B );
}
template<> FORCEINLINE FLOAT Min( const FLOAT A, const FLOAT B )
{
	return __fsels( A - B, B, A );
}
#endif

/*----------------------------------------------------------------------------
	DWORD specialization of templates.
----------------------------------------------------------------------------*/

// this specialization interacts poorly with PS3 and Mac-specific compiler optimization flag
#if !PS3 && !PLATFORM_MACOSX
/** Returns the smaller of the two values */
template<> FORCEINLINE DWORD Min(const DWORD A, const DWORD B)
{
    // Negative if B is less than A (i.e. the high bit will be set)
	DWORD Delta  = B - A;
	// Relies on sign bit rotating in
    DWORD Mask   = static_cast<INT>(Delta) >> 31;
    DWORD Result = A + (Delta & Mask);

    return Result;
}
#endif

/*----------------------------------------------------------------------------
	Standard macros.
----------------------------------------------------------------------------*/

// Number of elements in an array.
#define ARRAY_COUNT( array ) \
	( sizeof(array) / sizeof((array)[0]) )

// Offset of a struct member.

#ifdef __GNUC__
/**
 * gcc3 thinks &((myclass*)NULL)->member is an invalid use of the offsetof
 * macro. This is a broken heuristic in the compiler and the workaround is
 * to use a non-zero offset.
 */
#define STRUCT_OFFSET( struc, member )	( ( (PTRINT)&((struc*)0x1)->member ) - 0x1 )
#else
#define STRUCT_OFFSET( struc, member )	( (PTRINT)&((struc*)NULL)->member )
#endif

/*-----------------------------------------------------------------------------
	Allocators.
-----------------------------------------------------------------------------*/

template <class T> class TAllocator
{};

/**
 * works just like std::min_element.
 */
template<class ForwardIt> inline
ForwardIt MinElement(ForwardIt First, ForwardIt Last)
{
	ForwardIt Result = First;
	for (; ++First != Last; )
	{
		if (*First < *Result) 
		{
			Result = First;
		}
	}
	return Result;
}

/**
 * works just like std::min_element.
 */
template<class ForwardIt, class PredicateType> inline
ForwardIt MinElement(ForwardIt First, ForwardIt Last, PredicateType Predicate)
{
	ForwardIt Result = First;
	for (; ++First != Last; )
	{
		if (Predicate(*First,*Result))
		{
			Result = First;
		}
	}
	return Result;
}

/**
* works just like std::max_element.
*/
template<class ForwardIt> inline
ForwardIt MaxElement(ForwardIt First, ForwardIt Last)
{
	ForwardIt Result = First;
	for (; ++First != Last; )
	{
		if (*Result < *First) 
		{
			Result = First;
		}
	}
	return Result;
}

/**
* works just like std::max_element.
*/
template<class ForwardIt, class PredicateType> inline
ForwardIt MaxElement(ForwardIt First, ForwardIt Last, PredicateType Predicate)
{
	ForwardIt Result = First;
	for (; ++First != Last; )
	{
		if (Predicate(*Result,*First))
		{
			Result = First;
		}
	}
	return Result;
}

/**
 * utility template for a class that should not be copyable.
 * Derive from this class to make your class non-copyable
 */
class FNoncopyable
{
protected:
	// ensure the class cannot be constructed directly
	FNoncopyable() {}
	// the class should not be used polymorphically
	~FNoncopyable() {}
private:
	FNoncopyable(const FNoncopyable&);
	FNoncopyable& operator=(const FNoncopyable&);
};


/** 
 * exception-safe guard around saving/restoring a value.
 * Commonly used to make sure a value is restored 
 * even if the code early outs in the future.
 * Usage:
 *  	TGuardValue<UBOOL> GuardSomeBool(bSomeBool, FALSE); // Sets bSomeBool to FALSE, and restores it in dtor.
 */
template <typename Type>
struct TGuardValue : private FNoncopyable
{
	TGuardValue(Type& ReferenceValue, const Type& NewValue)
	: RefValue(ReferenceValue), OldValue(ReferenceValue)
	{
		RefValue = NewValue;
	}
	~TGuardValue()
	{
		RefValue = OldValue;
	}

	/**
	 * Overloaded dereference operator.
	 * Provides read-only access to the original value of the data being tracked by this struct
	 *
	 * @return	a const reference to the original data value
	 */
	FORCEINLINE const Type& operator*() const
	{
		return OldValue;
	}

private:
	Type& RefValue;
	Type OldValue;
};

/** Chooses between two different classes based on a boolean. */
template<bool Predicate,typename TrueClass,typename FalseClass>
class TChooseClass {};

template<typename TrueClass,typename FalseClass>
class TChooseClass<true,TrueClass,FalseClass>
{
public:
	typedef TrueClass Result;
};

template<typename TrueClass,typename FalseClass>
class TChooseClass<false,TrueClass,FalseClass>
{
public:
	typedef FalseClass Result;
};

/**
 * Helper class to make it easy to use key/value pairs with a container.
 */
template <typename KeyType, typename ValueType>
struct TKeyValuePair
{
	TKeyValuePair( const KeyType& InKey, ValueType& InValue )
	:	Key(InKey), Value(InValue)
	{
	}
	TKeyValuePair( const KeyType& InKey )
	:	Key(InKey)
	{
	}
	TKeyValuePair()
	{
	}
	UBOOL operator==( const TKeyValuePair& Other ) const
	{
		return Key == Other.Key;
	}
	UBOOL operator!=( const TKeyValuePair& Other ) const
	{
		return Key != Other.Key;
	}
	static INT Compare( const TKeyValuePair& A, const TKeyValuePair& B )
	{				
		if ( B.Key > A.Key )
		{
			return 1;
		}
		else if ( B.Key < A.Key )
		{
			return -1;
		}
		return 0;
	}
	KeyType		Key;
	ValueType	Value;
};

#endif
