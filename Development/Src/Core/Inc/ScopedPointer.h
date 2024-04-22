/*=============================================================================
	ScopedPointer.h: Scoped pointer type definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SCOPED_POINTER_H__
#define __SCOPED_POINTER_H__

/**
 * Wrapper around a raw pointer that destroys it automatically.
 * Calls operator delete on the object, so it must have been allocated with
 * operator new. Modeled after boost::scoped_ptr.
 * 
 * If a custom deallocator is needed, this class will have to
 * expanded with a deletion policy.
 */
template<typename ReferencedType> 
class TScopedPointer
{
private:

	ReferencedType* Reference;
	typedef TScopedPointer<ReferencedType> SelfType;

public:

	/** Initialization constructor. */
	TScopedPointer(ReferencedType* InReference = NULL)
	:	Reference(InReference)
	{}

	/** Copy constructor. */
	TScopedPointer(const TScopedPointer& InCopy)
	{
		Reference = InCopy.Reference ?
			new ReferencedType(*InCopy.Reference) :
			NULL;
	}

	/** Destructor. */
	~TScopedPointer()
	{
		delete Reference;
		Reference = NULL;
	}

	/** Assignment operator. */
	TScopedPointer& operator=(const TScopedPointer& InCopy)
	{
		if(&InCopy != this)
		{
			delete Reference;
			Reference = InCopy.Reference ?
				new ReferencedType(*InCopy.Reference) :
				NULL;
		}
		return *this;
	}

	// Dereferencing operators.
	ReferencedType& operator*() const
	{
		check(Reference != 0);
		return *Reference;
	}

	ReferencedType* operator->() const
	{
		check(Reference != 0);
		return Reference;
	}

	ReferencedType* GetOwnedPointer() const
	{
		return Reference;
	}

	/** Returns true if the pointer is valid */
	UBOOL IsValid() const
	{
		return ( Reference != 0 );
	}

	// implicit conversion to the reference type.
	operator ReferencedType*() const
	{
		return Reference;
	}

	void Swap(SelfType& b)
	{
		ReferencedType* Tmp = b.Reference;
		b.Reference = Reference;
		Reference = Tmp;
	}

	/** Deletes the current pointer and sets it to a new value. */
	void Reset(ReferencedType* NewReference = NULL)
	{
		check(!Reference || Reference != NewReference);
		delete Reference;
		Reference = NewReference;
	}

	/** Releases the owned pointer and returns it so it doesn't get deleted. */
	ReferencedType* Release()
	{
		ReferencedType* Result = GetOwnedPointer();
		Reference = NULL;
		return Result;
	}

	// Serializer.
	friend FArchive& operator<<(FArchive& Ar,SelfType& P)
	{
		if(Ar.IsLoading())
		{
			// When loading, allocate a new value.
			ReferencedType* OldReference = P.Reference;
			P.Reference = new ReferencedType;

			// Delete the old value.
			delete OldReference;
		}

		// Serialize the value.  The caller of this serializer is responsible to only serialize for saving non-NULL pointers. */
		check(P.Reference);
		Ar << *P.Reference;

		return Ar;
	}
};

/** specialize container traits */
template<typename ReferencedType>
struct TContainerTraits<TScopedPointer<ReferencedType> > : public TContainerTraitsBase<TScopedPointer<ReferencedType> >
{
	typedef ReferencedType* ConstInitType;
	typedef ReferencedType* ConstPointerType;
};

/** Implement movement of a scoped pointer to avoid copying the referenced value. */
template<typename ReferencedType> void Move(TScopedPointer<ReferencedType>& A,ReferencedType* B)
{
	A.Reset(B);
}

#endif
