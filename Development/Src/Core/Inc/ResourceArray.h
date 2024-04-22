/*=============================================================================
	ResourceArray.h: Resource array definitions and platform includes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * An element type independent interface to the resource array.
 */
class FResourceArrayInterface
{
public:

	/**
	* @return A pointer to the resource data.
	*/
	virtual const void* GetResourceData() const = 0;

	/**
	* @return size of resource data allocation
	*/
	virtual DWORD GetResourceDataSize() const = 0;

	/**
	* Called on non-UMA systems after the RHI has copied the resource data, and no longer needs the CPU's copy.
	*/
	virtual void Discard() = 0;

	/**
	* @return TRUE if the resource array is static and shouldn't be modified
	*/
	virtual UBOOL IsStatic() const = 0;

	/**
	* @return TRUE if the resource keeps a copy of its resource data after the RHI resource has been created
	*/
	virtual UBOOL GetAllowCPUAccess() const = 0;

	/** 
	* Sets whether the resource array will be accessed by CPU. 
	*/
	virtual void SetAllowCPUAccess( UBOOL bInNeedsCPUAccess ) = 0;
};

/**
* allows for direct GPU mem allocation for bulk resource types
*/
class FResourceBulkDataInterface
{
public:
	/** 
	* @return ptr to the resource memory which has been preallocated
	*/
	virtual void* GetResourceBulkData() const = 0;
	/** 
	* @return size of resource memory
	*/
	virtual DWORD GetResourceBulkDataSize() const = 0;
	/**
	* Free memory after it has been used to initialize RHI resource 
	*/
	virtual void Discard() = 0;
};

/**
* allows for direct GPU mem allocation for texture resource
*/
class FTexture2DResourceMem : public FResourceBulkDataInterface
{
public:
	/**
	* @param MipIdx index for mip to retrieve
	* @return ptr to the offset in bulk memory for the given mip
	*/
	virtual void* GetMipData(INT MipIdx) = 0;
	/**
	* @return total number of mips stored in this resource
	*/
	virtual INT	GetNumMips() = 0;
	/** 
	* @return width of texture stored in this resource
	*/
	virtual INT GetSizeX() = 0;
	/** 
	* @return height of texture stored in this resource
	*/
	virtual INT GetSizeY() = 0;
	/**
	 * @return Whether the resource memory is properly allocated or not.
	 **/
	virtual UBOOL IsValid() = 0;

	/**
	 * @return Whether the resource memory has an async allocation request and it's been completed.
	 */
	virtual UBOOL HasAsyncAllocationCompleted() const = 0;

	/**
	 * Blocks the calling thread until the allocation has been completed.
	 */
	virtual void FinishAsyncAllocation() = 0;

	/**
	 * Cancels any async allocation.
	 */
	virtual void CancelAsyncAllocation() = 0;

	virtual ~FTexture2DResourceMem() {}
};

#if XBOX
// Xenon D3D resource array
#include "XeD3DResourceArray.h"
#elif PS3
// PS3 resource array
#include "PS3ResourceArray.h"
#elif NGP
// NGP resource array
#include "NGPResourceArray.h"
#else
// Default to the dynamically bound RHI.
#include "DynamicRHIResourceArray.h"
#endif
