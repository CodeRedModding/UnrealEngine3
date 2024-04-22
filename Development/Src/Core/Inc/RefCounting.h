/*=============================================================================
	RefCounting.h: Reference counting definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * The base class of reference counted objects.
 */
class FRefCountedObject
{
public:
	FRefCountedObject(): NumRefs(0) {}
	virtual ~FRefCountedObject() { check(!NumRefs); }
	DWORD AddRef() const
	{
		return DWORD(++NumRefs);
	}
	DWORD Release() const
	{
		DWORD Refs = DWORD(--NumRefs);
		if(Refs == 0)
		{
			delete this;
		}
		return Refs;
	}
	DWORD GetRefCount() const
	{
		return DWORD(NumRefs);
	}
private:
	mutable INT NumRefs;
};

/**
 * A smart pointer to an object which implements AddRef/Release.
 */
template<typename ReferencedType>
class TRefCountPtr
{
	typedef ReferencedType* ReferenceType;
public:

	TRefCountPtr():
		Reference(NULL)
	{}

	TRefCountPtr(ReferencedType* InReference,UBOOL bAddRef = TRUE)
	{
		Reference = InReference;
		if(Reference && bAddRef)
		{
			Reference->AddRef();
		}
	}

	TRefCountPtr(const TRefCountPtr& Copy)
	{
		Reference = Copy.Reference;
		if(Reference)
		{
			Reference->AddRef();
		}
	}

	~TRefCountPtr()
	{
		if(Reference)
		{
			Reference->Release();
		}
	}

	TRefCountPtr& operator=(ReferencedType* InReference)
	{
		// Call AddRef before Release, in case the new reference is the same as the old reference.
		ReferencedType* OldReference = Reference;
		Reference = InReference;
		if(Reference)
		{
			Reference->AddRef();
		}
		if(OldReference)
		{
			OldReference->Release();
		}
		return *this;
	}
	
	TRefCountPtr& operator=(const TRefCountPtr& InPtr)
	{
		return *this = InPtr.Reference;
	}

	UBOOL operator==(const TRefCountPtr& Other) const
	{
		return Reference == Other.Reference;
	}

	ReferencedType* operator->() const
	{
		return Reference;
	}

	operator ReferenceType() const
	{
		return Reference;
	}

	ReferencedType** GetInitReference()
	{
		*this = NULL;
		return &Reference;
	}

	ReferencedType* GetReference() const
	{
		return Reference;
	}

	friend UBOOL IsValidRef(const TRefCountPtr& Reference)
	{
		return Reference.Reference != NULL;
	}

	void SafeRelease()
	{
		*this = NULL;
	}

	DWORD GetRefCount()
	{
		if(Reference)
		{
			Reference->AddRef();
			return Reference->Release();
		}
		else
		{
			return 0;
		}
	}

	friend FArchive& operator<<(FArchive& Ar,TRefCountPtr& Ptr)
	{
		ReferenceType Reference = Ptr.Reference;
		Ar << Reference;
		if(Ar.IsLoading())
		{
			Ptr = Reference;
		}
		return Ar;
	}

private:
	ReferencedType* Reference;
};
