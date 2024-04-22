//! @file std_wrappers.h
//! @brief Substance Air Framework stl compatible containers
//! @date 20120111
//! @copyright Allegorithmic. All rights reserved.
//!

#ifndef _SUBSTANCE_AIR_STD_WRAPPERS_H
#define _SUBSTANCE_AIR_STD_WRAPPERS_H

namespace SubstanceAir
{

template<class T>
class List
{
	TArray<T> mList;

public:
	typedef TIndexedContainerIterator< TArray<T> > TIterator;
	typedef TIndexedContainerConstIterator< TArray<T> > TConstIterator;

	List() {}

	const TArray<T>& getArray() const
	{
		return mList;
	}

	TArray<T>& getArray()
	{
		return mList;
	}

	inline TIterator itfront(){return TIterator(mList);}

	inline TConstIterator itfrontconst() const{return TConstIterator(mList);}

	inline T & operator[](UINT idx)
	{
		return mList.GetTypedData()[idx];
	}

	inline const T & operator[](UINT idx) const
	{
		return mList.GetTypedData()[idx];
	}

	inline const T & operator()(UINT idx) const
	{
		return mList.GetTypedData()[idx];
	}

	inline T & operator()(UINT idx)
	{
		return mList.GetTypedData()[idx];
	}

	inline void push(const T & elem)
	{
		mList.Push(elem);
	}

	INT AddZeroed(INT count)
	{
		return mList.AddZeroed(count);
	}

	INT AddUniqueItem(const T& elem)
	{
		return mList.AddUniqueItem(elem);
	}

	UBOOL FindItem(const T& elem, INT& idx ) const
	{
		return mList.FindItem(elem, idx);
	}

	INT FindItemIndex(const T& elem ) const
	{
		return mList.FindItemIndex(elem);
	}

	INT RemoveItem(const T& elem)
	{
		return mList.RemoveItem(elem);
	}

	void Remove(INT idx)
	{
		return mList.Remove(idx);
	}

	void Reserve(INT Size)
	{
		mList.Reserve(Size);
	}

	void BulkSerialize(FArchive& ar)
	{
		mList.BulkSerialize(ar);
	}

	List<T>& operator+=( const List<T>& Other )
	{
		mList.Append( Other.mList );
		return *this;
	}

	inline T front()
	{
		return mList.GetTypedData()[0];
	}

	inline T& Last()
	{
		return mList.Last();
	}

	inline T pop()
	{
		return mList.Pop();
	}

	inline void erase(const T & elem)
	{
		mList.RemoveItem(elem);
	}

	inline UINT size() const
	{
		return mList.Num();
	}

	inline INT Num() const
	{
		return mList.Num();
	}

	inline void Empty()
	{
		mList.Empty();
	}
};


template<class T>
class Vector
{
	TArray<T> mVector;

public:

	typedef TIndexedContainerIterator< TArray<T> > TIterator;
	typedef TIndexedContainerConstIterator< TArray<T> > TConstIterator;

	inline TIterator itfront(){return TIterator(mVector);}

	inline TConstIterator itfrontconst() const{return TConstIterator(mVector);}

	Vector() {}

	Vector(const Vector & other) : mVector(other.mVector)
	{
	}

	Vector & operator=(const Vector & other) 
	{ 
		mVector = other.mVector;
		return *this; 
	}

	inline void push_back(const T & elem)
	{
		mVector.Push(elem);
	}

	inline T & operator[](UINT idx)
	{
		return mVector.GetTypedData()[idx];
	}

	inline const T & operator[](UINT idx) const
	{
		return mVector.GetTypedData()[idx];
	}

	inline UINT size() const
	{
		return (UINT)mVector.Num();
	}

	inline void clear()
	{
		mVector.Reset();
	}

	inline bool operator==(const Vector & other) const
	{
		return (mVector == other.mVector != 0 ? true : false);
	}

	inline void reserve(UINT elemCount)
	{
		mVector.Reserve(elemCount);
	}

	inline void resize(UINT elemCount)
	{
		mVector.Reserve(elemCount);
		while (elemCount != size())
		{
			mVector.Push(T());
		}
	}

	inline INT removeItem(const T& elem)
	{
		return mVector.RemoveItem(elem);
	}

	void pop_front()
	{
		mVector.Remove(0);
	}
};

}

#endif // _SUBSTANCE_AIR_STD_WRAPPERS_H
