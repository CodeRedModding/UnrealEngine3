/*=============================================================================
	DynamicRHI.h: Dynamically bound Render Hardware Interface definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DYNAMICRHI_H__
#define __DYNAMICRHI_H__ 1

// Forward declarations.
template<ERHIResourceTypes ResourceType>
class TDynamicRHIResourceReference;

#if USE_DYNAMIC_RHI

/** This type is just used to give a resource a distinct type when it's passed around outside of this reference counted wrapper. */
template<ERHIResourceTypes ResourceType>
class TDynamicRHIResource
{
};

//
// Statically bound RHI resource reference type definitions for the dynamically bound RHI.
//
#define DEFINE_DYNAMICRHI_REFERENCE_TYPE(Type,ParentType) \
	template<> class TDynamicRHIResource<RRT_##Type> : public TDynamicRHIResource<RRT_##ParentType> {}; \
	typedef TDynamicRHIResource<RRT_##Type>*			F##Type##RHIParamRef; \
	typedef TDynamicRHIResourceReference<RRT_##Type>	F##Type##RHIRef;

ENUM_RHI_RESOURCE_TYPES(DEFINE_DYNAMICRHI_REFERENCE_TYPE);

#undef DEFINE_DYNAMICRHI_REFERENCE_TYPE

/** The interface which is implemented by the dynamically bound RHI. */
class FDynamicRHI
{
public:

	/** Declare a virtual destructor, so the dynamic RHI can be deleted without knowing its type. */
	virtual ~FDynamicRHI() {}

	virtual void PushEvent(const TCHAR* Name) {}

	virtual void PopEvent() {}

	// The RHI methods are defined as virtual functions in URenderHardwareInterface.
	#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) virtual Type Name ParameterTypesAndNames = 0
	#include "RHIMethods.h"
	#undef DEFINE_RHIMETHOD

	// Reference counting API for the different resource types.
	#define DEFINE_DYNAMICRHI_REFCOUNTING_FORTYPE(Type,ParentType) \
		virtual void AddResourceRef(TDynamicRHIResource<RRT_##Type>* Reference) = 0; \
		virtual void RemoveResourceRef(TDynamicRHIResource<RRT_##Type>* Reference) = 0; \
		virtual DWORD GetRefCount(TDynamicRHIResource<RRT_##Type>* Reference) = 0;
	ENUM_RHI_RESOURCE_TYPES(DEFINE_DYNAMICRHI_REFCOUNTING_FORTYPE);
	#undef DEFINE_DYNAMICRHI_REFCOUNTING_FORTYPE
};

/** A global pointer to the dynamically bound RHI implementation. */
extern FDynamicRHI* GDynamicRHI;

/**
 * A reference to a dynamically bound RHI resource.
 * When using the dynamically bound RHI, the reference counting goes through a dynamically bound interface in GDynamicRHI.
 * @param ResourceType - The type of resource the reference may point to.
 */
template<ERHIResourceTypes ResourceType>
class TDynamicRHIResourceReference
{
public:

	typedef TDynamicRHIResource<ResourceType>* ReferenceType;

	/** Default constructor. */
	TDynamicRHIResourceReference():
		Reference(NULL)
	{}

	/** Initialization constructor. */
	TDynamicRHIResourceReference(ReferenceType InReference)
	{
		Reference = InReference;
		if(Reference)
		{
			GDynamicRHI->AddResourceRef(Reference);
		}
	}

	/** Copy constructor. */
	TDynamicRHIResourceReference(const TDynamicRHIResourceReference& Copy)
	{
		Reference = Copy.Reference;
		if(Reference)
		{
			GDynamicRHI->AddResourceRef(Reference);
		}
	}

	/** Destructor. */
	~TDynamicRHIResourceReference()
	{
		if(Reference)
		{
			GDynamicRHI->RemoveResourceRef(Reference);
		}
	}

	/** Assignment operator. */
	TDynamicRHIResourceReference& operator=(ReferenceType InReference)
	{
		ReferenceType OldReference = Reference;
		if(InReference)
		{
			GDynamicRHI->AddResourceRef(InReference);
		}
		Reference = InReference;
		if(OldReference)
		{
			GDynamicRHI->RemoveResourceRef(OldReference);
		}
		return *this;
	}

	/** Assignment operator. */
	TDynamicRHIResourceReference& operator=(const TDynamicRHIResourceReference& InPtr)
	{
		return *this = InPtr.Reference;
	}

	/** Equality operator. */
	UBOOL operator==(const TDynamicRHIResourceReference& Other) const
	{
		return Reference == Other.Reference;
	}

	/** Dereference operator. */
	void* operator->() const
	{
		return Reference;
	}

	/** Dereference operator. */
	operator ReferenceType() const
	{
		return Reference;
	}

	/** Type hashing. */
	friend DWORD GetTypeHash(ReferenceType Reference)
	{
		return PointerHash(Reference);
	}

	// RHI reference interface.
	template<ERHIResourceTypes T>
	friend UBOOL IsValidRef(const TDynamicRHIResourceReference<T>& Ref);

	void SafeRelease()
	{
		*this = NULL;
	}
	DWORD GetRefCount()
	{
		if(Reference)
		{
			return GDynamicRHI->GetRefCount(Reference);
		}
		else
		{
			return 0;
		}
	}

private:
	ReferenceType Reference;
};

// Global declaration for this friend function (for some versions of GCC)
template<ERHIResourceTypes T>
UBOOL IsValidRef(const TDynamicRHIResourceReference<T>& Ref)
{
	return Ref.Reference != NULL;
}
template<ERHIResourceTypes T>
FORCEINLINE UBOOL IsValidRef(const TDynamicRHIResource<T>* Ref)
{
	return Ref != NULL;
}

// Implement the statically bound RHI methods to simply call the dynamic RHI.
#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	FORCEINLINE Type RHI##Name ParameterTypesAndNames \
	{ \
		check(GDynamicRHI); \
		ReturnStatement GDynamicRHI->Name ParameterNames; \
	}
#include "RHIMethods.h"
#undef DEFINE_RHIMETHOD

#endif	//#if USE_DYNAMIC_RHI


#if !CONSOLE || USE_NULL_RHI

/**
 * Defragment the texture pool.
 */
void appDefragmentTexturePool();

/**
 * Checks if the texture data is allocated within the texture pool or not.
 */
UBOOL appIsPoolTexture( FTextureRHIParamRef TextureRHI );

/**
 * Log the current texture memory stats.
 *
 * @param Message	This text will be included in the log
 */
void appDumpTextureMemoryStats(const TCHAR* /*Message*/);

#endif	//#if !CONSOLE

#endif
