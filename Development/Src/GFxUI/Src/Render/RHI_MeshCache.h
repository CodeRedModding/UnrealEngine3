/**************************************************************************

Filename    :   RHI_MeshCache.h
Content     :   RHI Mesh Cache implementation
Created     :   May 2009
Authors     :   Michael Antonov

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#ifndef INC_SF_Render_RHI_MeshCache_H
#define INC_SF_Render_RHI_MeshCache_H

#if WITH_GFx

#include "Engine.h"

#if RHI_UNIFIED_MEMORY
#include "Render/RHI_ConsoleMeshCache.h"
#else

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "Render/Render_MeshCache.h"
#include "Kernel/SF_Array.h"
#include "Kernel/SF_HeapNew.h"
#include "Kernel/SF_Debug.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

// If SF_RENDER_RHI_INSTANCE_MATRICES is decreased, make sure to update MeshCacheConfig defaults.
#if WITH_MOBILE_RHI
static const int SF_RENDER_RHI_INSTANCE_MATRICES = 12;
#else
static const int SF_RENDER_RHI_INSTANCE_MATRICES = 30;
#endif

namespace Scaleform
{
namespace Render
{
namespace RHI
{

class MeshBuffer;
class MeshBufferSet;
class VertexBuffer;
class IndexBuffer;
class MeshCache;
class HAL;


// RHI version of MeshCacheItem. In RHI index and vertex buffers
// are allocated separately.

class MeshCacheItem : public Render::MeshCacheItem
{
		friend class MeshCache;
		friend class MeshBuffer;
		friend class HAL;

		VertexBuffer* pVertexBuffer;
		IndexBuffer* pIndexBuffer;
		// Ranges of allocation in vertex and index buffers.
		UPInt        VertexOffset, VBAllocSize;
		UPInt        IndexOffset, IBAllocSize;

	public:

		static MeshCacheItem* Create ( MeshType type,
		                               MeshCacheListSet* pcacheList, MeshBaseContent& mc,
		                               VertexBuffer* pvb, IndexBuffer* pib,
		                               UPInt vertexOffset, UPInt vertexAllocSize, unsigned vertexCount,
		                               UPInt indexOffset, UPInt indexAllocSize, unsigned indexCount )
		{
			MeshCacheItem* p = ( RHI::MeshCacheItem* )
			                   Render::MeshCacheItem::Create ( type, pcacheList, sizeof ( MeshCacheItem ), mc,
			                           vertexAllocSize + indexAllocSize, vertexCount, indexCount );
			if ( p )
			{
				p->pVertexBuffer = pvb;
				p->pIndexBuffer  = pib;
				p->VertexOffset = vertexOffset;
				p->VBAllocSize   = vertexAllocSize;
				p->IndexOffset = indexOffset;
				p->IBAllocSize   = indexAllocSize;
			}
			return p;
		}

		inline FVertexBufferRHIParamRef GetVertexBuffer();
		inline FIndexBufferRHIParamRef  GetIndexBuffer();
};


enum
{
	MeshCache_MaxBufferCount      = 256,
	MeshCache_AddressToIndexShift = 24,
	// A multi-byte allocation unit is used in MeshBufferSet::Allocator.
	MeshCache_AllocatorUnitShift  = 4,
	MeshCache_AllocatorUnit       = 1 << MeshCache_AllocatorUnitShift
};


class MeshBuffer : public Render::MeshBuffer
{
	protected:
		UPInt       Index; // Index in MeshBufferSet::Buffers array.
		MeshBuffer* pNextLock;

	public:

		enum BufferType
		{
			Buffer_Vertex,
			Buffer_Index,
		};

		MeshBuffer ( UPInt size, AllocType type, unsigned arena )
			: Render::MeshBuffer ( size, type, arena )
		{ }
		virtual ~MeshBuffer() { }

		inline  UPInt   GetIndex() const
		{
			return Index;
		}

		virtual bool    allocBuffer() = 0;
		virtual bool    DoLock() = 0;
		virtual void    Unlock() = 0;
		virtual BufferType GetBufferType() const = 0;

		// Simple LockList class is used to track all MeshBuffers that were locked.
		struct LockList
		{
			MeshBuffer *pFirst;
			LockList() : pFirst ( 0 ) { }

			void Add ( MeshBuffer *pbuffer )
			{
				pbuffer->pNextLock = pFirst;
				pFirst = pbuffer;
			}
			void UnlockAll()
			{
				MeshBuffer* p = pFirst;
				while ( p )
				{
					p->Unlock();
					p = p->pNextLock;
				}
				pFirst = 0;
			}
		};

		inline  UByte*  Lock ( LockList& lockedBuffers )
		{
			if ( !pData )
			{
				if ( !DoLock() )
				{
					SF_DEBUG_WARNING ( 1, "Render::MeshCache - Vertex/IndexBuffer lock failed" );
					return 0;
				}
				lockedBuffers.Add ( this );
			}
			return ( UByte* ) pData;
		}
};


class MeshBufferSet
{
	public:
		typedef MeshBuffer::AllocType AllocType;

		// Buffers are addressable; index is stored in the upper bits of allocation address,
		// as tracked by the allocator. Buffers array may contain null pointers for freed buffers.
		ArrayLH<MeshBuffer*>  Buffers;
		AllocAddr             Allocator;
		UPInt                 Granularity;
		UPInt                 TotalSize;

		MeshBufferSet ( MemoryHeap* pheap, UPInt granularity )
			: Allocator ( pheap ), Granularity ( granularity ),  TotalSize ( 0 )
		{ }

		virtual ~MeshBufferSet()
		{ }

		inline void         SetGranularity ( UPInt granularity )
		{
			Granularity = granularity;
		}

		inline AllocAddr&   GetAllocator()
		{
			return Allocator;
		}
		inline UPInt        GetGranularity() const
		{
			return Granularity;
		}
		inline UPInt        GetTotalSize() const
		{
			return TotalSize;
		}

		virtual MeshBuffer* CreateBuffer ( UPInt size, AllocType type, unsigned arena,
		                                   MemoryHeap* pheap ) = 0;

		// Destroys all buffers of specified type; all types are destroyed if AT_None is passed.
		void        DestroyBuffers ( AllocType type = MeshBuffer::AT_None )
		{
			for ( UPInt i = 0; i < Buffers.GetSize(); i++ )
			{
				if ( Buffers[i] && ( ( type == MeshBuffer::AT_None ) || ( Buffers[i]->GetType() == type ) ) )
				{
					DestroyBuffer ( Buffers[i] );
				}
			}
		}

		static inline UPInt SizeToAllocatorUnit ( UPInt size )
		{
			return ( size + MeshCache_AllocatorUnit - 1 ) >> MeshCache_AllocatorUnitShift;
		}

		inline bool         CheckAllocationSize ( UPInt size, const char* bufferType )
		{
			SF_UNUSED ( bufferType );
			UPInt largestSize = 0;
			for ( UPInt i = 0; i < Buffers.GetSize(); i++ )
			{
				if ( Buffers[i]->Size > size )
				{
					return true;
				}
				if ( Buffers[i]->Size > largestSize )
				{
					largestSize = Buffers[i]->Size;
				}
			}
			SF_DEBUG_WARNING3 ( 1, "Render %s buffer allocation failed. Size = %d, MaxBuffer = %d",
			                    bufferType, size, largestSize );
			return false;
		}


		void        DestroyBuffer ( MeshBuffer* pbuffer )
		{
			Allocator.RemoveSegment ( pbuffer->GetIndex() << MeshCache_AddressToIndexShift,
			                          SizeToAllocatorUnit ( pbuffer->GetSize() ) );
			TotalSize -= pbuffer->GetSize();
			Buffers[pbuffer->GetIndex()] = 0;
			delete pbuffer;
		}


		// Alloc
		inline bool    Alloc ( UPInt size, MeshBuffer** pbuffer, UPInt* poffset )
		{
			UPInt offset = Allocator.Alloc ( SizeToAllocatorUnit ( size ) );
			if ( offset == ~UPInt ( 0 ) )
			{
				return false;
			}
			*pbuffer = Buffers[offset >> MeshCache_AddressToIndexShift];
			*poffset = ( offset & ( ( 1 << MeshCache_AddressToIndexShift ) - 1 ) ) << MeshCache_AllocatorUnitShift;
			return true;
		}

		inline UPInt    Free ( UPInt size, MeshBuffer* pbuffer, UPInt offset )
		{
			return Allocator.Free (
			           ( pbuffer->GetIndex() << MeshCache_AddressToIndexShift ) | ( offset >> MeshCache_AllocatorUnitShift ),
			           SizeToAllocatorUnit ( size ) ) << MeshCache_AllocatorUnitShift;
		}
};


template<class Buffer>
class MeshBufferSetImpl : public MeshBufferSet
{
	public:
		MeshBufferSetImpl ( MemoryHeap* pheap, UPInt granularity )
			: MeshBufferSet ( pheap, granularity )
		{ }
		virtual MeshBuffer* CreateBuffer ( UPInt size, AllocType type, unsigned arena, MemoryHeap* pheap )
		{
			// Single buffer can't be bigger then (1<<MeshCache_AddressToIndexShift)
			// size due to the way addresses are masked.
			SF_ASSERT ( size <= ( 1 << ( MeshCache_AddressToIndexShift + MeshCache_AllocatorUnitShift ) ) );
			return  Buffer::Create ( size, type, arena, pheap, *this );
		}
};


template<class BType, class Derived>
class MeshBufferImpl : public MeshBuffer
{
	protected:
		typedef MeshBufferImpl  Base;
		BType                   Buffer;
	public:

		MeshBufferImpl ( UPInt size, AllocType type, unsigned arena )
			: MeshBuffer ( size, type, arena )
		{ }

		inline BType GetHWBuffer() const
		{
			return Buffer;
		}

		static Derived* Create ( UPInt size, AllocType type, unsigned arena,
		                         MemoryHeap* pheap,
		                         MeshBufferSet& mbs )
		{
			// Determine which address index we'll use in the allocator.
			UPInt index = 0;
			while ( ( index < mbs.Buffers.GetSize() ) && mbs.Buffers[index] )
			{
				index++;
			}
			if ( index == MeshCache_MaxBufferCount )
			{
				return 0;
			}

			// Round-up to Allocator unit.
			size = ( ( size + MeshCache_AllocatorUnit - 1 ) >> MeshCache_AllocatorUnitShift ) << MeshCache_AllocatorUnitShift;

			Derived* p = SF_HEAP_NEW ( pheap ) Derived ( size, type, arena );
			if ( !p )
			{
				return 0;
			}

			if ( !p->allocBuffer() )
			{
				delete p;
				return 0;
			}
			p->Index = index;

			mbs.Allocator.AddSegment ( index << MeshCache_AddressToIndexShift, size >> MeshCache_AllocatorUnitShift );
			mbs.TotalSize += size;

			if ( index == mbs.Buffers.GetSize() )
				mbs.Buffers.PushBack ( p );
			else
			{
				mbs.Buffers[index] = p;
			}

			return p;
		}
};

class VertexBuffer : public MeshBufferImpl<FVertexBufferRHIRef, VertexBuffer>
{
	public:
		VertexBuffer ( UPInt size, AllocType atype, unsigned arena )
			: Base ( size, atype, arena )
		{ }

		virtual bool allocBuffer()
		{
			// Cannot use RUF_Dynamic because it discards existing data
			Buffer = RHICreateVertexBuffer ( Size, NULL, RUF_SmallUpdate );
			return Buffer != NULL;
		}

		virtual bool   DoLock()
		{
			SF_ASSERT ( !pData );
			pData = RHILockVertexBuffer ( Buffer, 0, Size, 0 );
			return pData != NULL;
		}
		virtual void    Unlock()
		{
			if ( pData )
			{
				RHIUnlockVertexBuffer ( Buffer );
				pData = 0;
			}
		}

		virtual BufferType GetBufferType() const
		{
			return Buffer_Vertex;
		}
};

class IndexBuffer : public MeshBufferImpl<FIndexBufferRHIRef, IndexBuffer>
{
	public:
		IndexBuffer ( UPInt size, AllocType atype, unsigned arena )
			: Base ( size, atype, arena )
		{ }

		virtual bool allocBuffer()
		{
			// Cannot use RUF_Dynamic because it discards existing data
			Buffer = RHICreateIndexBuffer ( 2, Size, NULL, RUF_SmallUpdate );
			return Buffer != NULL;
		}
		virtual bool   DoLock()
		{
			SF_ASSERT ( !pData );
			pData = RHILockIndexBuffer ( Buffer, 0, Size );
			return pData != NULL;
		}
		virtual void    Unlock()
		{
			if ( pData )
			{
				RHIUnlockIndexBuffer ( Buffer );
				pData = 0;
			}
		}

		virtual BufferType GetBufferType() const
		{
			return Buffer_Index;
		}
};

typedef MeshBufferSetImpl<VertexBuffer> VertexBufferSet;
typedef MeshBufferSetImpl<IndexBuffer>  IndexBufferSet;


inline FVertexBufferRHIParamRef MeshCacheItem::GetVertexBuffer()
{
	return pVertexBuffer->GetHWBuffer();
}

inline FIndexBufferRHIParamRef  MeshCacheItem::GetIndexBuffer()
{
	return pIndexBuffer->GetHWBuffer();
}

// RHI mesh cache implementation with following characteristics:
//  - Multiple cache lists and allocators; one per Buffer.
//  - Relies on 'Extended Locks'.

class MeshCache : public Render::MeshCache
{
		friend class HAL;

		enum
		{
			MinSupportedGranularity = 16 * 1024,
			MaxEraseBatchCount = ( 10 > SF_RENDER_RHI_INSTANCE_MATRICES ) ?
			10 : SF_RENDER_RHI_INSTANCE_MATRICES
		};

		MeshCacheListSet            CacheList;

		// Allocators managing the buffers.
		VertexBufferSet             VertexBuffers;
		IndexBufferSet              IndexBuffers;

		// Locked buffer into.
		bool                        Locked;
		UPInt                       VBSizeEvictedInLock;
		MeshBuffer::LockList        LockedBuffers;
		// A list of all buffers in the order they were allocated. Used to allow
		// freeing them in the opposite order (to support proper IB/VB balancing).
		// NOTE: Must use base MeshBuffer type for List<> to work with internal casts.
		List<Render::MeshBuffer>    ChunkBuffers;

		// Vertex buffer used for instancing. Right now it is simply
		// allocated to have constant values.
		FVertexBufferRHIRef         MaskEraseBatchVertexBuffer;

		inline MeshCache* getThis()
		{
			return this;
		}

		inline UPInt    getTotalSize() const
		{
			return VertexBuffers.GetTotalSize() + IndexBuffers.GetTotalSize();
		}


		//bool            createInstancingVertexBuffer();
        template<class MaskEraseVertexType>
		bool            createMaskEraseBatchVertexBuffer();

		// Allocates Vertex/Index buffer of specified size and adds it to free list.
		bool            allocCacheBuffers ( UPInt size, MeshBuffer::AllocType type, unsigned arena = 0 );

		void            evictMeshesInBuffer ( MeshCacheListSet::ListSlot* plist, UPInt count,
		                                      MeshBuffer* pbuffer );

		// Allocates a number of bytes in the specified buffer, while evicting LRU data.
		// Buffer can contain either vertex and/or index data.
		bool            allocBuffer ( UPInt* poffset, MeshBuffer** pbuffer,
		                              MeshBufferSet& mbs, UPInt size, bool waitForCache );

		void            destroyBuffers ( MeshBuffer::AllocType at = MeshBuffer::AT_None );

		// adjustMeshCacheParams can be called with either valid or null device.
		// Valid device is specified once MeshCache is initialized.
		void            adjustMeshCacheParams ( MeshCacheParams* p );

	public:

		MeshCache ( MemoryHeap* pheap,
		            const MeshCacheParams& params );
		~MeshCache();

		// Initializes MeshCache for operation, including allocation of the reserve
		// buffer. Typically called from SetVideoMode.
		bool            Initialize();
		// Resets MeshCache, releasing all buffers.
		void            Reset();

		virtual QueueMode GetQueueMode() const
		{
			return QM_ExtendLocks;
		}

		void AssertInFlightEmpty()
		{
			SF_ASSERT ( CacheList.GetSlot ( MCL_InFlight ).IsEmpty() );
		}


		// *** MeshCacheConfig Interface
		virtual void    ClearCache();
		virtual bool    SetParams ( const MeshCacheParams& params );


		virtual void    EndFrame();
		// Adds a fixed-size buffer to cache reserve; expected to be released at Release.
		//virtual bool    AddReserveBuffer(unsigned size, unsigned arena = 0);
		//virtual bool    ReleaseReserveBuffer(unsigned size, unsigned arena = 0);

		virtual bool    LockBuffers();
		virtual void    UnlockBuffers();
		virtual bool    AreBuffersLocked() const
		{
			return Locked;
		}

		virtual UPInt   Evict ( Render::MeshCacheItem* p, AllocAddr* pallocator = 0,
		                        MeshBase* pmesh = 0 );
		virtual bool    PreparePrimitive ( PrimitiveBatch* pbatch,
		                                   MeshCacheItem::MeshContent &mc, bool waitForCache );

		virtual AllocResult AllocCacheItem ( Render::MeshCacheItem** pdata,
		                                     UByte** pvertexDataStart, IndexType** pindexDataStart,
		                                     MeshCacheItem::MeshType meshType,
		                                     MeshCacheItem::MeshBaseContent &mc,
		                                     UPInt vertexBufferSize,
		                                     unsigned vertexCount, unsigned indexCount,
		                                     bool waitForCache, const VertexFormat* pDestFormat );
};



// Vertex structures used by both MeshCache and HAL.

class Render_InstanceData
{
	public:
		// Color format for instance, Alpha expected to have index.
		UInt32 Instance;
};

struct VertexXY32fInstance
{
	float   x,y;
	UInt8   Alpha[4];
};

}
}
};  // namespace Scaleform::Render::RHI

#endif // !RHI_UNIFIED_MEMORY

#endif//WITH_GFx

#endif
