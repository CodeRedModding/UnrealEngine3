/*=============================================================================
	StaticArray.h: Static array definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __STATICARRAY_H__
#define __STATICARRAY_H__

/** An array with a static number of elements. */
template<typename TElement,UINT NumElements>
class TStaticArray
{
public:

	/** Default constructor. */
	TStaticArray() {}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,TStaticArray<TElement,NumElements>& StaticArray)
	{
		for(UINT Index = 0;Index < NumElements;++Index)
		{
			Ar << StaticArray[Index];
		}
		return Ar;
	}
	
	// Accessors.
	TElement& operator[](UINT Index)
	{
		check(Index < NumElements);
		return Elements[Index];
	}
	const TElement& operator[](UINT Index) const
	{
		check(Index < NumElements);
		return Elements[Index];
	}

	// Comparisons.
	friend bool operator==(const TStaticArray<TElement,NumElements>& A,const TStaticArray<TElement,NumElements>& B)
	{
		for(UINT ElementIndex = 0;ElementIndex < NumElements;++ElementIndex)
		{
			if(!(A[ElementIndex] == B[ElementIndex]))
			{
				return false;
			}
		}
		return true;
	}
	friend bool operator!=(const TStaticArray<TElement,NumElements>& A,const TStaticArray<TElement,NumElements>& B)
	{
		for(UINT ElementIndex = 0;ElementIndex < NumElements;++ElementIndex)
		{
			if(!(A[ElementIndex] == B[ElementIndex]))
			{
				return true;
			}
		}
		return false;
	}
	
	/** Hash function. */
	friend UINT GetTypeHash(const TStaticArray<TElement,NumElements>& Array)
	{
		UINT Result = 0;
		for(UINT ElementIndex = 0;ElementIndex < NumElements;++ElementIndex)
		{
			Result ^= GetTypeHash(Array[ElementIndex]);
		}
		return Result;
	}

private:
	TElement Elements[NumElements];
};

/** A shortcut for initializing a TStaticArray with 2 elements. */
template<typename TElement>
class TStaticArray2 : public TStaticArray<TElement,2>
{
public:

	TStaticArray2(
		typename TCallTraits<TElement>::ParamType In0,
		typename TCallTraits<TElement>::ParamType In1
		)
	{
		(*this)[0] = In0;
		(*this)[1] = In1;
	}
};

/** A shortcut for initializing a TStaticArray with 3 elements. */
template<typename TElement>
class TStaticArray3 : public TStaticArray<TElement,3>
{
public:

	TStaticArray3(
		typename TCallTraits<TElement>::ParamType In0,
		typename TCallTraits<TElement>::ParamType In1,
		typename TCallTraits<TElement>::ParamType In2
		)
	{
		(*this)[0] = In0;
		(*this)[1] = In1;
		(*this)[2] = In2;
	}
};

/** A shortcut for initializing a TStaticArray with 4 elements. */
template<typename TElement>
class TStaticArray4 : public TStaticArray<TElement,4>
{
public:

	TStaticArray4(
		typename TCallTraits<TElement>::ParamType In0,
		typename TCallTraits<TElement>::ParamType In1,
		typename TCallTraits<TElement>::ParamType In2,
		typename TCallTraits<TElement>::ParamType In3
		)
	{
		(*this)[0] = In0;
		(*this)[1] = In1;
		(*this)[2] = In2;
		(*this)[3] = In3;
	}
};

/** Creates a static array filled with the specified value. */
template<typename TElement,UINT NumElements>
TStaticArray<TElement,NumElements> MakeUniformStaticArray(typename TCallTraits<TElement>::ParamType InValue)
{
	TStaticArray<TElement,NumElements> Result;
	for(UINT ElementIndex = 0;ElementIndex < NumElements;++ElementIndex)
	{
		Result[ElementIndex] = InValue;
	}
	return Result;
}

#endif
