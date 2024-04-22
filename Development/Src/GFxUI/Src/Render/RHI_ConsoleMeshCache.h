/**************************************************************************

Filename    :   RHI_ConsoleMeshCache.h
Content     :   RHI Mesh Cache header
Created     :
Authors     :

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#ifndef INC_SF_Render_RHI_ConsoleMeshCache_H
#define INC_SF_Render_RHI_ConsoleMeshCache_H

#if WITH_GFx

#if RHI_UNIFIED_MEMORY

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "Render/Render_SimpleMeshCache.h"
#include "Render/Render_MemoryManager.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

#define SF_RENDER_RHI_INSTANCE_MATRICES 15

namespace Scaleform
{
namespace Render
{
namespace RHI
{

class MeshCache;
class HAL;

class MeshBuffer : public SimpleMeshBuffer
{
	public:
		// These point to the same memory
		FVertexBufferRHIRef VertexBuffer;
		FIndexBufferRHIRef  IndexBuffer;

		MeshBuffer ( UPInt size, AllocType type, unsigned arena );
};

// RHI version of MeshCacheItem.
// We define this class primarily to allow HAL member access through friendship.

class MeshCacheItem : public SimpleMeshCacheItem
{
		friend class MeshCache;
		friend class HAL;

	public:
		static MeshCacheItem*  Create ( MeshType type, MeshCacheListSet* pcacheList, MeshContent& mc,
		                                MeshBuffer* pbuffer,
		                                UPInt allocAddress, UPInt allocSize,
		                                UPInt vertexOffset, unsigned vertexCount,
		                                UPInt indexOffset, unsigned indexCount, const VertexFormat * pfmt )
		{
			return ( MeshCacheItem* )
			       SimpleMeshCacheItem::Create ( type, pcacheList, sizeof ( MeshCacheItem ),
			                                     mc, pbuffer, allocAddress, allocSize,
			                                     vertexOffset, vertexCount,
			                                     indexOffset, indexCount, pfmt );
		}

		inline FVertexBufferRHIParamRef GetVertexBuffer()
		{
			return ( ( MeshBuffer* ) pBuffer )->VertexBuffer;
		}

		inline FIndexBufferRHIParamRef  GetIndexBuffer()
		{
			return ( ( MeshBuffer* ) pBuffer )->IndexBuffer;
		}
};


class RenderSync : public Render::RenderSync
{
	public:
		RenderSync() { }

		// RenderSync implementation
		virtual void     KickOffFences(FenceType)
		{
			RHIKickCommandBuffer();
		}

protected:

		virtual UInt64   SetFence()
		{
			return ( RHISetFence() );
		}
		virtual bool     IsPending ( FenceType, UInt64 InFence, const FenceFrame& )
		{
			return RHIIsFencePending ( InFence ) != 0;
		}
		virtual void     WaitFence ( FenceType, UInt64 InFence, const FenceFrame& )
		{
			RHIWaitFence ( InFence );
		}
};


// RHI MeshCache implementation is simple, relying on SimpleMeshCache to do
// most of the work.

class MeshCache : public SimpleMeshCache
{
		friend class HAL;

		enum
		{
		    MaxEraseBatchCount = 10
		};

		RenderSync                  RSync;

		FVertexBufferRHIRef         MaskEraseBatchVertexBuffer;

		// SimpleMeshCache implementation
		virtual SimpleMeshBuffer* createHWBuffer ( UPInt size, AllocType atype, unsigned arena );
		virtual void              destroyHWBuffer ( SimpleMeshBuffer* pbuffer );

		bool            createStaticVertexBuffers();
		bool            createInstancingVertexBuffer();
		bool            createMaskEraseBatchVertexBuffer();

		void            adjustMeshCacheParams ( MeshCacheParams* p );

	public:
		MeshCache ( MemoryHeap* pheap, const MeshCacheParams& params );
		~MeshCache();

		// Initializes MeshCache for operation, including allocation of the reserve
		// buffer. Typically called from SetVideoMode.
		bool            Initialize();
		// Resets MeshCache, releasing all buffers.
		void            Reset();

		RenderSync*     GetRenderSync()
		{
			return &RSync;
		}

		virtual bool    SetParams ( const MeshCacheParams& params );

		virtual AllocResult AllocCacheItem ( Render::MeshCacheItem** pdata,
		                                     UByte** pvertexDataStart, IndexType** pindexDataStart,
		                                     MeshCacheItem::MeshType meshType,
		                                     MeshCacheItem::MeshBaseContent &mc,
		                                     UPInt vertexBufferSize,
		                                     unsigned vertexCount, unsigned indexCount,
		                                     bool waitForCache, const VertexFormat* pDestFormat );

		virtual bool    PreparePrimitive ( PrimitiveBatch* pbatch, MeshCacheItem::MeshContent &mc, bool waitForCache );

		virtual void    PostUpdateMesh ( Render::MeshCacheItem * pcacheItem );
};

}
}
};  // namespace Scaleform::Render::RHI

#endif // RHI_UNIFIED_MEMORY

#endif//WITH_GFx

#endif

