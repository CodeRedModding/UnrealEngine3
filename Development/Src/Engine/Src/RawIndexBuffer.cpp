/*=============================================================================
	RawIndexBuffer.cpp: Raw index buffer implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#if !CONSOLE && !PLATFORM_UNIX && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
#if _WIN64
#pragma pack (push,16)
#endif
#include "../../../External/nvTriStrip/Inc/NvTriStrip.h"
#if _WIN64
#pragma pack (pop)
#endif

/**
 * Converts 16 bit indices to 32 bit prior to passing them into the real GenerateStrips util method
 */
void GenerateStrips(const BYTE* Indices, UBOOL Is32Bit, const UINT NumIndices, PrimitiveGroup** PrimGroups, UINT* NumGroups)
{
	if (Is32Bit)
	{
		GenerateStrips((UINT*)Indices, NumIndices, PrimGroups, NumGroups);
	}
	else
	{
		// convert to 32 bit
		UINT Idx;
		UINT* NewIndices = new UINT[NumIndices];
		for (Idx = 0; Idx < NumIndices; ++Idx)
		{
			NewIndices[Idx] = ((WORD*)Indices)[Idx];
		}
		
		GenerateStrips(NewIndices, NumIndices, PrimGroups, NumGroups);
	}

}

/**
* Converts a triangle list into a triangle strip.
*/
template<typename IndexDataType, typename Allocator>
INT StripifyIndexBuffer(TArray<IndexDataType,Allocator>& Indices)
{
	PrimitiveGroup*	PrimitiveGroups = NULL;
	UINT			NumPrimitiveGroups = 0;
	UBOOL Is32Bit = sizeof(IndexDataType) == 4;

	SetListsOnly(false);

	GenerateStrips((BYTE*)&Indices(0),Is32Bit,Indices.Num(),&PrimitiveGroups,&NumPrimitiveGroups);
	
	Indices.Empty();
	Indices.Add(PrimitiveGroups->numIndices);
	appMemcpy(&Indices(0),PrimitiveGroups->indices,Indices.Num() * sizeof(IndexDataType));

	delete [] PrimitiveGroups;

	return Indices.Num() - 2;
}

/**
* Orders a triangle list for better vertex cache coherency.
*/
template<typename IndexDataType, typename Allocator>
static void CacheOptimizeIndexBuffer(TArray<IndexDataType,Allocator>& Indices)
{
	PrimitiveGroup*	PrimitiveGroups = NULL;
	UINT			NumPrimitiveGroups = 0;
	UBOOL Is32Bit = sizeof(IndexDataType) == 4;

	SetListsOnly(true);

	GenerateStrips((BYTE*)&Indices(0),Is32Bit,Indices.Num(),&PrimitiveGroups,&NumPrimitiveGroups);

	Indices.Empty();
	Indices.Add(PrimitiveGroups->numIndices);
	
	if( Is32Bit )
	{
		appMemcpy(&Indices(0),PrimitiveGroups->indices,Indices.Num() * sizeof(IndexDataType));
	}
	else
	{
		for( UINT I = 0; I < PrimitiveGroups->numIndices; ++I )
		{
			Indices(I) = (WORD)PrimitiveGroups->indices[I];
		}
	}

	delete [] PrimitiveGroups;
}
#endif //!CONSOLE && !PLATFORM_UNIX && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER

/*-----------------------------------------------------------------------------
FRawIndexBuffer
-----------------------------------------------------------------------------*/

/**
* Converts a triangle list into a triangle strip.
*/
INT FRawIndexBuffer::Stripify()
{
#if !CONSOLE && !PLATFORM_UNIX && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	return StripifyIndexBuffer(Indices);
#else
	return 0;
#endif
}

/**
* Orders a triangle list for better vertex cache coherency.
*/
void FRawIndexBuffer::CacheOptimize()
{
#if !CONSOLE && !PLATFORM_UNIX && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	CacheOptimizeIndexBuffer(Indices);
#endif
}

void FRawIndexBuffer::InitRHI()
{
	DWORD Size = Indices.Num() * sizeof(WORD);
	if( Size > 0 )
	{
		// Create the index buffer.
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(WORD),Size,NULL,RUF_Static);

		// Initialize the buffer.
		void* Buffer = RHILockIndexBuffer(IndexBufferRHI,0,Size);
		appMemcpy(Buffer,&Indices(0),Size);
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
}

FArchive& operator<<(FArchive& Ar,FRawIndexBuffer& I)
{
	I.Indices.BulkSerialize( Ar );
	return Ar;
}

/*-----------------------------------------------------------------------------
FRawIndexBuffer16or32
-----------------------------------------------------------------------------*/

// on platforms that only support 16-bit indices, the FRawIndexBuffer16or32 class is just typedef'd to the 16-bit version
#if !DISALLOW_32BIT_INDICES

/**
* Converts a triangle list into a triangle strip.
*/
INT FRawIndexBuffer16or32::Stripify()
{
#if !CONSOLE && !PLATFORM_UNIX && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	return StripifyIndexBuffer(Indices);
#else
	return 0;
#endif
}

/**
* Orders a triangle list for better vertex cache coherency.
*/
void FRawIndexBuffer16or32::CacheOptimize()
{
#if !CONSOLE && !PLATFORM_UNIX && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	CacheOptimizeIndexBuffer(Indices);
#endif
}

void FRawIndexBuffer16or32::InitRHI()
{
#if WITH_MOBILE_RHI
	if( GUsingMobileRHI && !GUseSeekFreeLoading )
	{
		// When loading index buffers for ES2 on platforms where 32-bit indices are still allowed, we'll
		// need to perform a load-time convert as ES2 cannot render 32-bit indices

		// make an array for 16-bit indices
		TArray<WORD> NewIndices;
		NewIndices.Add(Indices.Num());
		
		// convert each one, checking to make sure it's in range
		for (INT Index = 0; Index < NewIndices.Num(); Index++)
		{
			DWORD OldIndex = Indices(Index);
			if (OldIndex > 0xFFFF)
			{
				appErrorf(TEXT("Failed to convert an index buffer to 16-bit for Mobile, the it's too big (run through the debugger to see what is being serialized)"));
			}
			NewIndices(Index) = (WORD)(OldIndex & 0xFFFF);
		}

		DWORD Size = Indices.Num() * sizeof(WORD);
		if( Size > 0 )
		{
			// Create the index buffer.
			IndexBufferRHI = RHICreateIndexBuffer(sizeof(WORD),Size,NULL,RUF_Static);

			// Initialize the buffer.
			void* Buffer = RHILockIndexBuffer(IndexBufferRHI,0,Size);
			appMemcpy(Buffer,&NewIndices(0),Size);
			RHIUnlockIndexBuffer(IndexBufferRHI);
		}
	}
	else
#endif
	{
		DWORD Size = Indices.Num() * sizeof(DWORD);
		if( Size > 0 )
		{
			// Create the index buffer.
			IndexBufferRHI = RHICreateIndexBuffer(sizeof(DWORD),Size,NULL,RUF_Static);

			// Initialize the buffer.
			void* Buffer = RHILockIndexBuffer(IndexBufferRHI,0,Size);
			appMemcpy(Buffer,&Indices(0),Size);
			RHIUnlockIndexBuffer(IndexBufferRHI);
		}
	}

	// Undo/redo can destroy and recreate the render resources for UModels without rebuilding the
	// buffers, so the indices need to be saved when in the editor.
	if (!GIsEditor)
	{
		Indices.Empty();
	}
}

FArchive& operator<<(FArchive& Ar,FRawIndexBuffer16or32& I)
{
	// need to convert to 16-bit indices for mobile devices
	if (Ar.IsSaving() && (GCookingTarget & UE3::PLATFORM_Mobile))
	{
		// make an array for 16-bit indices
		TArray<WORD> NewIndices;
		NewIndices.Add(I.Indices.Num());
		
		// convert each one, checking to make sure it's in range
		for (INT Index = 0; Index < NewIndices.Num(); Index++)
		{
			DWORD OldIndex = I.Indices(Index);
			if (OldIndex > 0xFFFF)
			{
				appErrorf(TEXT("Failed to convert an index buffer to 16-bit for Mobile, the it's too big (run through the debugger to see what is being serialized)"));
			}
			NewIndices(Index) = (WORD)(OldIndex & 0xFFFF);
		}

		// now write out the 16-bit indices
		NewIndices.BulkSerialize(Ar);
	}
	else
	{
		I.Indices.BulkSerialize( Ar );
	}
	return Ar;
}

#endif

/*-----------------------------------------------------------------------------
FRawStaticIndexBuffer
-----------------------------------------------------------------------------*/

/**
* Create the index buffer RHI resource and initialize its data
*/
void FRawStaticIndexBuffer::InitRHI()
{
	DWORD Size = Indices.Num() * sizeof(WORD);
	if(Indices.Num())
	{
		if (bSetupForInstancing)
		{
			// Create an instanced index buffer.
			check(NumVertsPerInstance > 0);
			// Create the index buffer.
			UINT NumInstances = 0;
			// Clamp the number of preallocated instances to avoid overflowing 16-bit vertex indices when offsetting the duplicate instances to the index buffer
			UINT ClampedPreallocateInstanceCount = Min<UINT>(PreallocateInstanceCount, 65535 / NumVertsPerInstance);
			IndexBufferRHI = RHICreateInstancedIndexBuffer(sizeof(WORD),Size,RUF_Static,ClampedPreallocateInstanceCount,NumInstances);
			check(NumInstances);
			// Initialize the buffer.
			WORD* Buffer = (WORD *)RHILockIndexBuffer(IndexBufferRHI,0,Size * NumInstances);
			WORD Offset = 0;
			check(NumInstances * NumVertsPerInstance < 65536);
			for (UINT Instance = 0; Instance < NumInstances; Instance++)
			{
				for (INT Index = 0; Index < Indices.Num(); Index++)
				{
					*Buffer++ = Indices(Index) + Offset;
				}
				Offset += (WORD)NumVertsPerInstance;
			}
			RHIUnlockIndexBuffer(IndexBufferRHI);
		}
		else
		{
			// Create the index buffer.
			IndexBufferRHI = RHICreateIndexBuffer(sizeof(WORD),Size,&Indices,RUF_Static);
		}
	}    
}

/**
* Serializer for this class
*
* @param	Ar				Archive to serialize with
* @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
*/
void FRawStaticIndexBuffer::Serialize( FArchive& Ar, UBOOL bNeedsCPUAccess )
{
	Indices.SetAllowCPUAccess( bNeedsCPUAccess );
	Indices.BulkSerialize( Ar );
	if (Ar.IsLoading())
	{
		// Make sure these are set to no-instancing values
		NumVertsPerInstance = 0;
		bSetupForInstancing = FALSE;
	}
}

/**
* Converts a triangle list into a triangle strip.
*/
INT FRawStaticIndexBuffer::Stripify()
{
#if !CONSOLE && !PLATFORM_UNIX && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	return StripifyIndexBuffer(Indices);
#else
	return 0;
#endif
}

/**
* Orders a triangle list for better vertex cache coherency.
*/
void FRawStaticIndexBuffer::CacheOptimize()
{
#if !CONSOLE && !PLATFORM_UNIX && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	CacheOptimizeIndexBuffer(Indices);
#endif
}


/*-----------------------------------------------------------------------------
FRawStaticIndexBuffer16or32
-----------------------------------------------------------------------------*/


/**
* Converts a triangle list into a triangle strip.
*/
template <typename INDEX_TYPE>
INT FRawStaticIndexBuffer16or32<INDEX_TYPE>::Stripify()
{
#if !CONSOLE && !PLATFORM_UNIX && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	return StripifyIndexBuffer(Indices);
#else
	return 0;
#endif
}

/**
* Orders a triangle list for better vertex cache coherency.
*/
template <typename INDEX_TYPE>
void FRawStaticIndexBuffer16or32<INDEX_TYPE>::CacheOptimize()
{
#if !CONSOLE && !PLATFORM_UNIX && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	CacheOptimizeIndexBuffer(Indices);
#endif
}


/*-----------------------------------------------------------------------------
	FRawGPUIndexBuffer
-----------------------------------------------------------------------------*/

/**
 *	Default constructor
 */
FRawGPUIndexBuffer::FRawGPUIndexBuffer()
:	NumIndices(0)
,	Stride(sizeof(WORD))
,	bIsDynamic(FALSE)
,	bIsEmpty(TRUE)
{
}

/**
 *	Setup constructor
 *	@param InNumIndices		- Number of indices to allocate space for
 *	@param InIsDynamic		- TRUE if the index buffer should be dynamic
 *	@param InStride			- Number of bytes per index
 */
FRawGPUIndexBuffer::FRawGPUIndexBuffer(UINT InNumIndices, UBOOL InIsDynamic/*=FALSE*/, UINT InStride/*=sizeof(WORD)*/)
:	NumIndices(InNumIndices)
,	Stride(InStride)
,	bIsDynamic(InIsDynamic)
,	bIsEmpty(TRUE)
{
}

/**
 *	Sets up the index buffer, if the default constructor was used.
 *	@param InNumIndices		- Number of indices to allocate space for
 *	@param InIsDynamic		- TRUE if the index buffer should be dynamic
 *	@param InStride			- Number of bytes per index
 */
void FRawGPUIndexBuffer::Setup(UINT InNumIndices, UBOOL InIsDynamic/*=FALSE*/, UINT InStride/*=sizeof(WORD)*/)
{
	check( bIsEmpty );
	NumIndices	= InNumIndices;
	Stride		= InStride;
	bIsDynamic	= InIsDynamic;
	bIsEmpty	= TRUE;
}

/**
 *	Renderthread API.
 *	Create the index buffer RHI resource and initialize its data.
 */
void FRawGPUIndexBuffer::InitRHI()
{
	if ( !bIsDynamic )
	{
		IndexBufferRHI = RHICreateIndexBuffer(Stride, NumIndices * Stride, NULL, bIsDynamic ? RUF_Dynamic : RUF_Static );
		bIsEmpty = TRUE;
	}
}

/**
 *	Renderthread API.
 *	Releases a dynamic index buffer RHI resource.
 *	Called when the resource is released, or when reseting all RHI resources.
 */
void FRawGPUIndexBuffer::ReleaseRHI()
{
	if ( !bIsDynamic )
	{
		IndexBufferRHI.SafeRelease();
		bIsEmpty = TRUE;
	}
}

/**
 *	Renderthread API.
 *	Create an empty dynamic index buffer RHI resource.
 */
void FRawGPUIndexBuffer::InitDynamicRHI()
{
	if ( bIsDynamic )
	{
		IndexBufferRHI = RHICreateIndexBuffer(Stride, NumIndices * Stride, NULL, bIsDynamic ? RUF_Dynamic : RUF_Static);
		bIsEmpty = TRUE;
	}
}

/**
 *	Renderthread API.
 *	Releases a dynamic index buffer RHI resource.
 *	Called when the resource is released, or when reseting all RHI resources.
 */
void FRawGPUIndexBuffer::ReleaseDynamicRHI()
{
	if ( bIsDynamic )
	{
		IndexBufferRHI.SafeRelease();
		bIsEmpty = TRUE;
	}
}

/**
 *	Renderthread API.
 *	Locks the index buffer and returns a pointer to the first index in the locked region.
 *
 *	@param FirstIndex		- First index in the locked region. Defaults to the first index in the buffer.
 *	@param NumIndices		- Number of indices to lock. Defaults to the remainder of the buffer.
 */
void* FRawGPUIndexBuffer::Lock( UINT FirstIndex, UINT InNumIndices )
{
	if ( InNumIndices == 0 )
	{
		InNumIndices = NumIndices - FirstIndex;
	}
	return RHILockIndexBuffer( IndexBufferRHI, FirstIndex * Stride, InNumIndices * Stride );
}

/**
 *	Renderthread API.
 *	Unlocks the index buffer.
 */
void FRawGPUIndexBuffer::Unlock( )
{
	RHIUnlockIndexBuffer( IndexBufferRHI );
	bIsEmpty = FALSE;
}
