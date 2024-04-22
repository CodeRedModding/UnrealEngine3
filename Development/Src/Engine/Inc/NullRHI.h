/*=============================================================================	
	NullRHI.h: Null RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __NULLRHI_H__
#define __NULLRHI_H__

/** The type of null RHI resources. */
template<ERHIResourceTypes ResourceType>
class TNullRHIResource : public TDynamicRHIResource<ResourceType>, public FRefCountedObject
{
};

/** Null viewport resource, used to track when to initialize and release global resources. */
class FNullViewportRHI : public TNullRHIResource<RRT_Viewport>
{
public:
	FNullViewportRHI();
	virtual ~FNullViewportRHI();
};

/** A null implementation of the dynamically bound RHI. */
class FNullDynamicRHI : public FDynamicRHI
{
public:

	// Implement the dynamic RHI interface using the null implementations defined in RHIMethods.h
	#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
		virtual Type Name ParameterTypesAndNames { NullImplementation; }
	#include "RHIMethods.h"
	#undef DEFINE_RHIMETHOD

	// Reference counting API for the different resource types.
	#define DEFINE_NULLRHI_REFCOUNTING_FORTYPE(Type,ParentType) \
		virtual void AddResourceRef(TDynamicRHIResource<RRT_##Type>* Reference) \
		{ \
			TNullRHIResource<RRT_##Type>* NullResource = (TNullRHIResource<RRT_##Type>*)Reference; \
			NullResource->AddRef(); \
		} \
		virtual void RemoveResourceRef(TDynamicRHIResource<RRT_##Type>* Reference) \
		{ \
			TNullRHIResource<RRT_##Type>* NullResource = (TNullRHIResource<RRT_##Type>*)Reference; \
			NullResource->Release(); \
		} \
		virtual DWORD GetRefCount(TDynamicRHIResource<RRT_##Type>* Reference) \
		{ \
			TNullRHIResource<RRT_##Type>* NullResource = (TNullRHIResource<RRT_##Type>*)Reference; \
			NullResource->AddRef(); \
			return NullResource->Release(); \
		}

	ENUM_RHI_RESOURCE_TYPES(DEFINE_NULLRHI_REFCOUNTING_FORTYPE);
	#undef DEFINE_NULLRHI_REFCOUNTING_FORTYPE

private:

	/** Allocates a static buffer for RHI functions to return as a write destination. */
	static void* GetStaticBuffer();
};

#endif
