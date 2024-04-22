/*=============================================================================
	DynamicRHIResourceArray.h: Resource array definitions for dynamically bound RHIs.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/** alignment for supported resource types */
enum EResourceAlignment
{
	VERTEXBUFFER_ALIGNMENT	=DEFAULT_ALIGNMENT,
	INDEXBUFFER_ALIGNMENT	=DEFAULT_ALIGNMENT
};

/**
 * A array which allocates memory which can be used for UMA rendering resources.
 * In the dynamically bound RHI, it isn't any different from the default array type,
 * since none of the dynamically bound RHI implementations have UMA.
 *
 * @param Alignment - memory alignment to use for the allocation
 */
template< typename ElementType, DWORD Alignment = DEFAULT_ALIGNMENT >
class TResourceArray
	:	public FResourceArrayInterface
	,	public TArray<ElementType, TAlignedHeapAllocator<Alignment> >
{
public:
	typedef TArray<ElementType, TAlignedHeapAllocator<Alignment> > Super;

	/** 
	* Constructor 
	*/
	TResourceArray(UBOOL InNeedsCPUAccess=FALSE)
		:	Super()
		,	bNeedsCPUAccess(InNeedsCPUAccess)
	{
	}

	// FResourceArrayInterface

	/**
	* @return A pointer to the resource data.
	*/
	virtual const void* GetResourceData() const 
	{ 
		return &(*this)(0); 
	}

	/**
	* @return size of resource data allocation
	*/
	virtual DWORD GetResourceDataSize() const
	{
		return this->Num() * sizeof(ElementType);
	}

	/**
	* Called on non-UMA systems after the RHI has copied the resource data, and no longer needs the CPU's copy.
	* Only discard the resource memory on clients, and if the CPU doesn't need access to it.
	* Non-clients can't discard the data because they may need to serialize it.
	*/
	virtual void Discard()
	{		
		if(!bNeedsCPUAccess && !GIsEditor && !GIsUCC)
		{
			this->Empty();
		}
	}

	/**
	* @return TRUE if the resource array is static and shouldn't be modified
	*/
	virtual UBOOL IsStatic() const
	{
		return FALSE;
	}

	/**
	* @return TRUE if the resource keeps a copy of its resource data after the RHI resource has been created
	*/
	virtual UBOOL GetAllowCPUAccess() const
	{
		return bNeedsCPUAccess;
	}

	/** 
	* Sets whether the resource array will be accessed by CPU. 
	*/
	virtual void SetAllowCPUAccess(UBOOL bInNeedsCPUAccess)
	{
		bNeedsCPUAccess = bInNeedsCPUAccess;
	}

	// Assignment operators.
	TResourceArray& operator=(const TResourceArray& Other)
	{
		bNeedsCPUAccess = Other.bNeedsCPUAccess;
		Super::operator=(Other);
		return *this;
	}
	TResourceArray& operator=(const Super& Other)
	{
		Super::operator=(Other);
		return *this;
	}

	/**
	* Serialize data as a single block. See TArray::BulkSerialize for more info.
	*
	* IMPORTANT:
	*   - This is Overridden from UnTemplate.h TArray::BulkSerialize  Please make certain changes are propogated accordingly
	*
	* @param Ar	FArchive to bulk serialize this TArray to/from
	*/
	void BulkSerialize(FArchive& Ar)
	{
		Super::BulkSerialize(Ar);
	}

	/**
	* Serializer for this class
	* @param Ar - archive to serialize to
	* @param ResourceArray - resource array data to serialize
	*/
	friend FArchive& operator<<(FArchive& Ar,TResourceArray<ElementType,Alignment>& ResourceArray)
	{
		return Ar << *(Super*)&ResourceArray;
	}	

private:
	/** 
	* TRUE if this array needs to be accessed by the CPU.  
	* If no CPU access is needed then the resource is freed once its RHI resource has been created
	*/
	UBOOL bNeedsCPUAccess;
};
