/**************************************************************************

Filename    :   RHI_ConsoleMeshCache.cpp
Content     :   RHI Mesh Cache implementation
Created     :
Authors     :

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#include "GFxUI.h"

#if WITH_GFx

#if RHI_UNIFIED_MEMORY

#include "Render/RHI_ConsoleMeshCache.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "Kernel/SF_HeapNew.h"
#include "Kernel/SF_Debug.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif


namespace Scaleform
{
namespace Render
{
namespace RHI
{


MeshBuffer::MeshBuffer ( UPInt size, AllocType type, unsigned arena )
	: SimpleMeshBuffer ( size, type, arena )
{
	// Buffers cannot be placed in local memory on ps3, and don't allocate a double buffer
	VertexBuffer = RHICreateVertexBuffer ( size, NULL, 0 );
	IndexBuffer = RHICreateAliasedIndexBuffer ( VertexBuffer, 2 );
	pData = RHILockVertexBuffer ( VertexBuffer, 0, 0, FALSE );
}


// Static temporary buffer used during some vertex operations
static UByte TempScratchBuffer[1024 * 64];


// ***** MeshCache

MeshCache::MeshCache ( MemoryHeap* pheap, const MeshCacheParams& params )
	: SimpleMeshCache ( pheap, params, &RSync )
{
	adjustMeshCacheParams(&Params);
}

MeshCache::~MeshCache()
{
	Reset();
}

// Initializes MeshCache for operation, including allocation of the reserve
// buffer. Typically called from SetVideoMode.
bool    MeshCache::Initialize()
{
	if ( !StagingBuffer.Initialize ( pHeap, Params.StagingBufferSize ) )
	{
		return false;
	}

	if ( !allocateReserve() )
	{
		return false;
	}

	if ( !createStaticVertexBuffers() )
	{
		return false;
	}

	return true;
}

void    MeshCache::Reset()
{
	MaskEraseBatchVertexBuffer.SafeRelease();
	releaseAllBuffers();
}

bool MeshCache::SetParams ( const MeshCacheParams& argParams )
{
	MeshCacheParams oldParams ( Params );
	CacheList.EvictAll();
	Params = argParams;
	adjustMeshCacheParams ( &Params );

	if ( StagingBuffer.GetBuffer() )
	{
		if ( Params.StagingBufferSize != oldParams.StagingBufferSize )
		{
			if ( !StagingBuffer.Initialize ( pHeap, Params.StagingBufferSize ) )
			{
				if ( !StagingBuffer.Initialize ( pHeap, Params.StagingBufferSize ) )
				{
					SF_DEBUG_ERROR ( 1, "MeshCache::SetParams - couldn't restore StagingBuffer after fail" );
				}
				return false;
			}
		}

		if ( ( Params.MemReserve != oldParams.MemReserve ) ||
		        ( Params.MemGranularity != oldParams.MemGranularity ) )
		{
			releaseAllBuffers();

			// Allocate new reserve. If not possible, restore previous one and fail.
			if ( Params.MemReserve && !allocateReserve() )
			{
				SF_DEBUG_ERROR ( 1, "MeshCache::SetParams - couldn't restore Reserve after fail" );
			}
		}
	}
	return true;
}

bool MeshCache::createStaticVertexBuffers()
{
	return createInstancingVertexBuffer() &&
	       createMaskEraseBatchVertexBuffer();
}

bool MeshCache::createInstancingVertexBuffer()
{
	return true;
}

bool MeshCache::createMaskEraseBatchVertexBuffer()
{
	MaskEraseBatchVertexBuffer = RHICreateVertexBuffer ( sizeof ( VertexXY16iInstance ) * 6 * MaxEraseBatchCount, NULL, RUF_Static );
	VertexXY16iInstance* pbuffer = ( VertexXY16iInstance* )
	                               RHILockVertexBuffer ( MaskEraseBatchVertexBuffer, 0, sizeof ( VertexXY16iInstance ) * 6 * MaxEraseBatchCount, FALSE );

	// For now we create a buffer with a list of const values so that it can be
	// used to carry matrix index. TBD: Perhaps there is a shader value we can use instead?
	for ( unsigned i = 0; i < MaxEraseBatchCount; i++ )
	{
		pbuffer[i * 6 + 0].x  = 0;
		pbuffer[i * 6 + 0].y  = 1;
		pbuffer[i * 6 + 0].Instance[0] = ( UByte ) i;
		pbuffer[i * 6 + 1].x  = 0;
		pbuffer[i * 6 + 1].y  = 0;
		pbuffer[i * 6 + 1].Instance[0] = ( UByte ) i;
		pbuffer[i * 6 + 2].x  = 1;
		pbuffer[i * 6 + 2].y  = 0;
		pbuffer[i * 6 + 2].Instance[0] = ( UByte ) i;

		pbuffer[i * 6 + 3].x  = 0;
		pbuffer[i * 6 + 3].y  = 1;
		pbuffer[i * 6 + 3].Instance[0] = ( UByte ) i;
		pbuffer[i * 6 + 4].x  = 1;
		pbuffer[i * 6 + 4].y  = 0;
		pbuffer[i * 6 + 4].Instance[0] = ( UByte ) i;
		pbuffer[i * 6 + 5].x  = 1;
		pbuffer[i * 6 + 5].y  = 1;
		pbuffer[i * 6 + 5].Instance[0] = ( UByte ) i;
	}
	RHIUnlockVertexBuffer ( MaskEraseBatchVertexBuffer );
	return true;
}

void MeshCache::adjustMeshCacheParams ( MeshCacheParams* p )
{
	// TBD: Detect/record HW instancing capability.

	if ( p->MaxBatchInstances > SF_RENDER_RHI_INSTANCE_MATRICES )
	{
		p->MaxBatchInstances = SF_RENDER_RHI_INSTANCE_MATRICES;
	}
	//p->InstancingThreshold = 1 << 30;
}

SimpleMeshBuffer*  MeshCache::createHWBuffer ( UPInt size, AllocType atype, unsigned arena )
{
	MeshBuffer* pbuffer = SF_HEAP_NEW ( pHeap ) MeshBuffer ( size, atype, arena );
	return pbuffer;
}

void        MeshCache::destroyHWBuffer ( SimpleMeshBuffer* pbuffer )
{
	delete pbuffer;
}

MeshCache::AllocResult  MeshCache::AllocCacheItem ( Render::MeshCacheItem** pdata,
        UByte** pvertexDataStart, IndexType** pindexDataStart,
        MeshCacheItem::MeshType meshType,
        MeshCacheItem::MeshBaseContent &mc,
        UPInt vertexBufferSize,
        unsigned vertexCount, unsigned indexCount,
        bool waitForCache, const VertexFormat* )
{
	UPInt   allocAddress;
#if XBOX
	UPInt   allocSize      = ( ( vertexBufferSize + 3 ) & ~3 ) + ( ( indexCount * sizeof ( IndexType ) + 3 ) & ~3 );
#elif WIIU
	UPInt   vertexAlignedSize = (vertexBufferSize + 0x1f) & ~0x1f;
	UPInt   allocSize      = (vertexAlignedSize + indexCount * sizeof(IndexType) + 0x3f) & ~0x3f;
#else
	UPInt   allocSize      = ( ( vertexBufferSize + 15 ) & ~15 ) + ( indexCount * sizeof ( IndexType ) );
#endif

	if ( !allocBuffer ( &allocAddress, allocSize, waitForCache ) )
	{
		return Alloc_Fail;
	}

	MeshBuffer* pbuffer  = ( MeshBuffer* ) findBuffer ( allocAddress );
	UPInt       vbOffset = allocAddress - ( UPInt ) pbuffer->pData;
	UPInt       ibOffset = vbOffset + vertexBufferSize;

	// Create new MeshCacheItem; add it to hash.
	*pdata = SimpleMeshCacheItem::Create ( meshType, &CacheList, sizeof ( SimpleMeshCacheItem ),
	                                       mc, pbuffer, allocAddress, allocSize, vbOffset, vertexCount, ibOffset, indexCount, 0 );

	*pvertexDataStart = ( UByte* ) allocAddress;
	*pindexDataStart  = ( IndexType* ) ( allocAddress + vertexBufferSize );
	return Alloc_Success;
}



// Generates meshes and uploads them to buffers.
// Returns 'false' if there is not enough space in Cache, so Unlock and
// flush needs to take place.
//   - Pass 'false' for firstCall is PreparePrimitive was already called once
//     with this data and failed.
bool    MeshCache::PreparePrimitive ( PrimitiveBatch* pbatch,
                                      MeshCacheItem::MeshContent &mc, bool waitForCache )
{
	Primitive* prim = pbatch->GetPrimitive();

	if ( mc.IsLargeMesh() )
	{
		check ( mc.GetMeshCount() == 1 );
		MeshResult mr = GenerateMesh ( mc[0], prim->GetVertexFormat(),
		                               pbatch->pFormat, 0, waitForCache );

		if ( mr.Succeded() )
		{
			pbatch->SetCacheItem ( mc[0]->CacheItems[0] );
		}
		// Return 'false' if we just need more cache, to flush and retry.
		if ( mr == MeshResult::Fail_LargeMesh_NeedCache )
		{
			return false;
		}
		return true;
	}

	// NOTE: We always know that meshes in one batch fit into Mesh Staging Cache.
	unsigned    totalVertexCount, totalIndexCount;
	pbatch->CalcMeshSizes ( &totalVertexCount, &totalIndexCount );

	// First, try to allocate the buffer space while swapping out any stale data.
	// Note that AllocBuffer takes place before mesh-gen to ensure that packing code
	// does not rely on MeshCacheItem items that may get evicted during allocation.
	// For vertex alignment purpose, we round all allocations to 16 bytes.
	UPInt   allocAddress;
	UPInt   vertexByteSize = totalVertexCount * pbatch->pFormat->Size;
#if WIIU
	UPInt   vertexAlignedSize = (vertexByteSize + 0x1f) & ~0x1f;
	UPInt   allocSize      = (vertexAlignedSize + totalIndexCount * sizeof(IndexType) + 0x3f) & ~0x3f;
#else
	UPInt   vertexAlignedSize = ( ( vertexByteSize + 15 ) & ~15 );
	UPInt   allocSize      = vertexAlignedSize + totalIndexCount * sizeof ( IndexType );
#endif
#if XBOX
	allocSize = ( allocSize + 3 ) & ~3;
#endif

	if ( !allocBuffer ( &allocAddress, allocSize, waitForCache ) )
	{
		return false;
	}

	check ( pbatch->pFormat );

	Render::MeshCacheItem* batchData = 0;

	MeshBuffer* pbuffer  = ( MeshBuffer* ) findBuffer ( allocAddress );
	UPInt       vbOffset = allocAddress - ( UPInt ) pbuffer->pData;
	UPInt       ibOffset = vbOffset + vertexAlignedSize;

	batchData = SimpleMeshCacheItem::Create ( MeshCacheItem::Mesh_Regular, &CacheList, sizeof ( SimpleMeshCacheItem ),
	            mc, pbuffer, allocAddress, allocSize, vbOffset, totalVertexCount, ibOffset, totalIndexCount, pbatch->pFormat );

	if ( !batchData )
	{
		return false;
	}

	pbatch->SetCacheItem ( batchData );

	// Prepare and Pin mesh data with the StagingBuffer.
	StagingBufferPrep   meshPrep ( this, mc, pbatch->GetPrimitive()->GetVertexFormat(), true, batchData );

	//GDebug::Message(GDebug::Message_Note, "Building BatchMesh %p with %d meshes >>>",
	//                pbatch->GetBatchMesh(), arrayMeshCount);


	// Copy meshes into the Vertex/Index buffers.
	// All the meshes have been pinned, so we can
	// go through them and copy them into buffers.
	UByte*      pvertexDataStart = ( UByte* ) allocAddress;
	IndexType*  pindexDataStart  = ( IndexType* ) ( allocAddress + vertexAlignedSize );
	UByte*      pstagingBuffer   = StagingBuffer.GetBuffer();
	const VertexFormat* pvf      = pbatch->GetPrimitive()->GetVertexFormat();

	unsigned        i;
	unsigned        indexStart = 0;

	for ( i = 0; i < mc.GetMeshCount(); i++ )
	{
		Mesh* pmesh = mc[i];
		//check(pmesh->Data.GetVertexFormat() == pvf);

		// Convert vertices and initialize them to the running index
		// within this primitive.
		UByte   instance = ( UByte ) i;
		void*   convertArgArray[1] = { &instance };

		// GDebug::Message(GDebug::Message_Note, "[i = %d] Mesh %p starts at Vertex %d", i, pmesh, indexStart);

		// If mesh is in staging buffer, initialize it from there.
		if ( pmesh->StagingBufferSize != 0 )
		{
			if ( sizeof ( TempScratchBuffer ) >= pmesh->VertexCount * pbatch->pFormat->Size )
			{
				ConvertVertices ( *pvf, pstagingBuffer + pmesh->StagingBufferOffset,
				                  *pbatch->pFormat, TempScratchBuffer,
				                  pmesh->VertexCount, &convertArgArray[0] );
				memcpy ( pvertexDataStart, TempScratchBuffer, pmesh->VertexCount * pbatch->pFormat->Size );
			}
			else
			{
				ConvertVertices ( *pvf, pstagingBuffer + pmesh->StagingBufferOffset,
				                  *pbatch->pFormat, pvertexDataStart,
				                  pmesh->VertexCount, &convertArgArray[0] );
			}

			// Copy and assign indices.
			IndexType* pindexSource = ( IndexType* ) ( pstagingBuffer + pmesh->StagingBufferIndexOffset );
			for ( unsigned j = 0; j < pmesh->IndexCount; j++ )
			{
				* ( pindexDataStart++ ) = pindexSource[j] + ( IndexType ) indexStart;
			}
		}
		else
		{
			// If mesh is not in a staging buffer, find the batch that has our mesh and location in it,
			// so vertex and index data can be copied.
			unsigned            prevVertexCount, prevIndexCount;
			SimpleMeshCacheItem*  psourceBatchMesh = ( SimpleMeshCacheItem* )
			        MeshCacheItem::FindMeshSourceBatch ( pmesh, &prevVertexCount, &prevIndexCount,
			                pbatch->GetCacheItem() );

			//     GDebug::Message(GDebug::Message_Note, "[i = %d of %d] Reusing mesh %p in BatchMesh %p from BatchMesh %p index %d",
			//                     i, meshContent.MeshCount, pmesh, pbatch->GetBatchMesh(), psourceBatchMesh, imesh);


			// Now, copy the old mesh data into the new location.
			const VertexFormat * psrcFmt = psourceBatchMesh->GetVertexFormat();
			UByte* psourceBufferStart = ( UByte* ) ( psourceBatchMesh->GetBuffer() )->pData;
			UByte* pvertexSource      = psourceBufferStart + psourceBatchMesh->GetVertexOffset() +
			                            prevVertexCount * psrcFmt->Size;

			// If the meshes have the same format, then we can copy the mesh data directly, and just update
			// the instancing indices for the new location. If they do not have the same data, we will need to
			// convert it.
			if ( psrcFmt && pbatch->pFormat == psrcFmt )
			{
				memcpy ( pvertexDataStart, pvertexSource, pmesh->VertexCount * pbatch->pFormat->Size );
				// Assign proper instance index.
				InitializeVertices ( *pbatch->pFormat, pvertexDataStart, pmesh->VertexCount, &convertArgArray[0] );
			}
			else
			{
				ConvertVertices ( *psrcFmt, pvertexSource,
				                  *pbatch->pFormat, pvertexDataStart,
				                  pmesh->VertexCount, &convertArgArray[0] );
			}

			// Convert and assign indices (always the same format).
			IndexType* pindexSource = ( ( IndexType* ) ( psourceBufferStart + psourceBatchMesh->GetIndexOffset() ) ) +
			                          prevIndexCount;
			for ( unsigned j = 0; j < pmesh->IndexCount; j++ )
			{
				* ( pindexDataStart++ ) = ( pindexSource[j] - ( IndexType ) ( prevVertexCount ) ) +
				                          ( IndexType ) indexStart;
			}
		}

		pvertexDataStart += pmesh->VertexCount * pbatch->pFormat->Size;
		indexStart       += pmesh->VertexCount;
	}

	// We invalidate GPU cache after we've written data to it, so that stale content
	// does not accidentally get used when rendering.
	PostUpdateMesh ( batchData );

	// ~StagingBufferPrep will Unpin meshes in the staging buffer.
	return true;
}

void MeshCache::PostUpdateMesh ( Render::MeshCacheItem * pcacheItem )
{
	SF_UNUSED ( pcacheItem );

	// We invalidate GPU cache after we've written data to it, so that stale content
	// does not accidentally get used when rendering.
#if PS3 && !USE_NULL_RHI
	cellGcmSetInvalidateVertexCache();
#endif
#if WIIU && !USE_NULL_RHI
	MeshCacheItem* pmesh = (MeshCacheItem*)pcacheItem;
	GX2Invalidate(GX2_INVALIDATE_CPU_ATTRIB_BUFFER, (void*)pmesh->AllocAddress, pmesh->AllocSize);
#endif
}


}
}
}; // namespace Scaleform::Render::RHI

#endif // RHI_UNIFIED_MEMORY

#endif//WITH_GFx
