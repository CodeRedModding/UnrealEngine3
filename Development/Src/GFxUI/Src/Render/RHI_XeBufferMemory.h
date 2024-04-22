/**********************************************************************

PublicHeader:   Render
Filename    :   RHI_XeBufferMemory.h
Content     :   RenderBufferManager implementation, which allocates all
                rendering surfaces from user provided memory block(s).
Created     :   Sept 2011
Authors     :   Bart Muzzin

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

***********************************************************************/

#ifndef INC_SF_RHI_XeBufferMemory_H
#define INC_SF_RHI_XeBufferMemory_H

#if WITH_GFx

#if XBOX

#if !USE_NULL_RHI
	#define SF_RENDERBUFFERMEMORY_ENABLE 1
#else
	#define SF_RENDERBUFFERMEMORY_ENABLE 0
#endif

#include "Render/Render_Buffer.h"
#include "Render/RHI_Texture.h"
#include "Kernel/SF_Array.h"
#include "Kernel/SF_AllocAddr.h"

namespace Scaleform
{
namespace Render
{
namespace RHI
{
namespace RBMemoryImpl
{

class RenderBufferManager;

//------------------------------------------------------------------------
class RenderTarget;
class DepthStencilBuffer;

static const int RHI_XeTEXTURE_MEMORY_ALIGN = 4096;
static const int RHI_XeTEXTURE_MEMORY_SHIFT = 12;

class RenderBufferManager : public Render::RenderBufferManager
{
		friend class RenderTarget;
		friend class DepthStencilBuffer;

	public:

		RenderBufferManager ( MemoryHeap* pheap = Memory::GetGlobalHeap() );
		virtual ~RenderBufferManager();

		virtual bool Initialize ( Render::TextureManager* manager, ImageFormat format, const ImageSize& screenSize = ImageSize ( 0 ) );

		// Adds a block of video memory from a texture to be used for render buffer allocation.
		void AddResolveMemory ( const FTexture2DRHIRef &texture );
		void AddSurfaceMemory ( const FSurfaceRHIRef& surface );

		// Adds all blocks of surface/resolve memory to their allocators.
		void AcquireVideoMemory( );

		// Releases use of all claimed resolve/surface memory from their allocators.
		void ReleaseVideoMemory( );

		virtual void Destroy();
		virtual void EndFrame();
		virtual void Reset();

		virtual Render::RenderTarget* CreateRenderTarget ( const ImageSize& size, RenderBufferType type, ImageFormat format, Render::Texture* texture = 0 );
		virtual Render::RenderTarget* CreateTempRenderTarget ( const ImageSize& size );
		virtual Render::DepthStencilBuffer* CreateDepthStencilBuffer ( const ImageSize& size );

	protected:

		FSurfaceRHIRef   allocateSurface ( const ImageSize& size, EPixelFormat format, SInt32& edramLocation, unsigned& edramSize );
		FTexture2DRHIRef allocateRenderTargetResolve ( const ImageSize &size );

		void clearRenderTargetSurface ( RenderTarget& rt );
		void clearRenderTargetResolve ( RenderTarget& rt, bool bIsUnused = false );
		void clearDepthStencilSurface ( DepthStencilBuffer& dsb );

		RHI::TextureManager*                pTextureManager;     // Texture manager
		Ptr<Render::TextureManagerLocks>    pManagerLocks;       // Hiding access to the locking mechanisms within TextureManager.
		MemoryHeap*                         pMainHeap;           // Heap from which main memory allocations are made.
		AllocAddr                           ResolveAlloc;        // Allocator managing video memory blocks.
		AllocAddr                           SurfaceAlloc;        // Allocator for managing EDRAM.
		UPInt                               AvailableResolve;    // Total amount of available resolve memory.
		UPInt                               AvailableSurface;    // Total amount of available surface memory (EDRAM pages).
		List<RBMemoryImpl::RenderTarget>    RenderTargets;       // List of currently allocated render targets.

		struct AllocRange
		{
			UPInt Location;
			UPInt Size;
			bool Overlaps ( const AllocRange& ar ) const
			{
				return ( ar.End() >= Location && ar.Location <= End() ) ||
				       ( End() >= ar.Location && Location <= ar.End() );
			}
			bool Contains ( const AllocRange &ar ) const
			{
				return ( Location <= ar.Location && End() >= ar.End() );
			}
			UPInt End() const
			{
				return Location + Size;
			}
		};
		ArrayPOD<AllocRange>                ResolveSegments;    // Tracks resolve segments, so overlapping segments are not provided to the allocator.
		ArrayPOD<AllocRange>                SurfaceSegments;    // Tracks surface segments, so overlapping segments are not provided to the allocator.

		void addSegment ( ArrayPOD<AllocRange>& segmentList, const AllocRange& range );
		void allocateSegments ( ArrayPOD<AllocRange>& segmentList, AllocAddr& allocator, UPInt& availableMemory, UPInt shift );
};

//------------------------------------------------------------------------
class RenderTarget : public Render::RenderTarget, public ListNode<RenderTarget>
{
		friend class RenderBufferManager;
	public:
		RenderTarget ( RenderBufferManager* manager, RenderBufferType type,
		               const ImageSize& bufferSize )
			: Render::RenderTarget ( manager, type, bufferSize ),
			  ListNode<RenderTarget>(),
			  EDRAMTile ( -1 ),
			  EDRAMSize ( 0 ),
			  pTexture ( 0 ),
			  RTStatus ( RTS_Lost )
		{ }

		virtual ~RenderTarget()
		{
			Render::RenderTarget::SetInUse ( false );
		}

		virtual void      AddRef()
		{
			RefCount++;
		}
		virtual void      Release()
		{
			RefCount--;
			if ( RefCount == 0 )
			{
				delete this;
			}
		}

		virtual Texture*    GetTexture() const
		{
			return pTexture;
		}

		virtual RenderTargetStatus GetStatus() const
		{
			return RTStatus;
		}

		virtual void        SetInUse ( RenderTargetUse inUse );

	protected:
		// Return derived-class version of manager.
		RenderBufferManager* getManager() const
		{
			return ( RenderBufferManager* ) pManager;
		}

		void initSurface ( SInt32 edramTile, unsigned edramSize )
		{
			EDRAMTile = edramTile;
			EDRAMSize = edramSize;
		}

		void initTexture ( RHI::Texture* texture )
		{
			pTexture = texture;
			RTStatus = RTS_InUse;
		}

		void initViewRect ( const Rect<int>& viewRect )
		{
			ViewRect = viewRect;
		}

		SInt32              EDRAMTile;
		unsigned            EDRAMSize;

		Ptr<RHI::Texture>  pTexture;
		RenderTargetStatus  RTStatus;
};


// DepthStencilBuffer implementation with swapping support through CacheData;
// holds DepthStencilSurface.

class DepthStencilBuffer : public Render::DepthStencilBuffer, public ListNode<DepthStencilBuffer>
{
		friend class RenderBufferManager;
	public:
		DepthStencilBuffer ( RenderBufferManager* manager, const ImageSize& bufferSize )
			: Render::DepthStencilBuffer ( manager, bufferSize ), ListNode<DepthStencilBuffer>(),
			  EDRAMTile ( -1 ), EDRAMSize ( 0 ), pSurface ( 0 )
		{ }

		virtual void      AddRef()
		{
			RefCount++;
		}
		virtual void      Release()
		{
			RefCount--;
			if ( RefCount == 0 )
			{
				delete this;
			}
		}

		virtual ~DepthStencilBuffer()
		{
			if ( getManager() )
			{
				getManager()->clearDepthStencilSurface ( *this );
			}
		}

		virtual DepthStencilSurface* GetSurface() const
		{
			return pSurface;
		}

	protected:
		void initSurface ( DepthStencilSurface* surface, SInt32 edramTile, unsigned edramSize )
		{
			pSurface = surface;
			EDRAMTile = edramTile;
			EDRAMSize = edramSize;
		}

		RenderBufferManager* getManager() const
		{
			return ( RenderBufferManager* ) pManager;
		}

		SInt32                      EDRAMTile;
		unsigned                    EDRAMSize;
		Ptr<DepthStencilSurface>    pSurface;
};

} // ::RBMemoryImpl

typedef RBMemoryImpl::RenderBufferManager RenderBufferManagerMemory;

}
}
} // Scaleform::Render::RHI

#endif // XBOX

#endif//WITH_GFx

#endif // INC_SF_RHI_XeBufferMemory_H
