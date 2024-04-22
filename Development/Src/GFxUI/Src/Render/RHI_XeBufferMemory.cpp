/**********************************************************************

PublicHeader:   Render
Filename    :   Render_BufferMemory.cpp
Content     :
Created     :   Sep 2011
Authors     :   Bart Muzzin

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

***********************************************************************/

#include "GFxUI.h"

#if WITH_GFx

#include "Render/RHI_XeBufferMemory.h"
#include "Render/RHI_HAL.h"
#include "Kernel/SF_Debug.h"

#if XBOX

namespace Scaleform
{
namespace Render
{
namespace RHI
{
namespace RBMemoryImpl
{

RenderBufferManager::RenderBufferManager ( MemoryHeap* pheap ) :
	Render::RenderBufferManager(),
	pTextureManager ( 0 ),
	pManagerLocks ( 0 ),
	pMainHeap ( pheap ),
	ResolveAlloc ( pMainHeap ),
	SurfaceAlloc ( pMainHeap ),
	AvailableResolve ( 0 ),
	AvailableSurface ( 0 )
{
}

RenderBufferManager::~RenderBufferManager()
{
	Destroy();
}


bool RenderBufferManager::Initialize ( Render::TextureManager* manager, ImageFormat format, const ImageSize& screenSize )
{
	pTextureManager = ( RHI::TextureManager* ) manager;
	pManagerLocks = *SF_HEAP_NEW ( pMainHeap ) TextureManagerLocks ( ( RHI::TextureManager* ) manager );
	SF_UNUSED2 ( format, screenSize );
	return true;
}

Render::RenderTarget* RenderBufferManager::CreateTempRenderTarget ( const ImageSize& size )
{
	SInt32 edramTile;
	unsigned edramSize;
	FSurfaceRHIRef surface = allocateSurface ( size, PF_A8R8G8B8, edramTile, edramSize );
	if ( !IsValidRef ( surface ) )
	{
		return 0;
	}
	FTexture2DRHIRef texture = allocateRenderTargetResolve ( size );
	if ( !IsValidRef ( texture ) )
	{
		return 0;
	}

	// Allocate texture/render target. NOTE: this is outside the texture manager's control.
	Ptr<RHI::Texture> pt = *SF_HEAP_NEW ( pMainHeap ) RHI::Texture ( pManagerLocks, 0, size, 0 );
	RenderTarget* target = SF_HEAP_NEW ( pMainHeap ) RenderTarget ( this, RBuffer_Temporary, size );

	pt->pTextures[0].Size               = size;
	pt->pTextures[0].Tex                = new TextureResource ( pt );
	pt->pTextures[0].Tex->TextureRHI    = texture;
	pt->pTextures[0].Tex->Texture2DRHI  = texture;
	pt->pTextures[0].Tex->SurfaceRHI    = surface;
	pt->pTextures[0].UpdateResource();

	target->initSurface ( edramTile, edramSize );
	target->initTexture ( pt );
	target->initViewRect ( Rect<int> ( size.Width, size.Height ) );
	RenderTargets.PushBack ( target );
	target->AddRef();

	return target;
}

void RenderBufferManager::AddResolveMemory ( const FTexture2DRHIRef& texture )
{
#if SF_RENDERBUFFERMEMORY_ENABLE
	if ( !IsValidRef ( texture ) || !texture->GetSharedTexture() )
	{
		return;
	}

	FSharedMemoryResourceRHIRef sharedMemory = texture->GetSharedTexture()->GetSharedMemory();
	if ( IsValidRef ( sharedMemory ) )
	{
		UPInt baseData = ( UPInt ) sharedMemory->BaseAddress;
		UPInt baseSize = ( UPInt ) sharedMemory->AllocatedSize;

		// Must pass a valid address and a minimum of 4k.
		if ( baseData == 0 || baseSize < RHI_XeTEXTURE_MEMORY_ALIGN )
		{
			return;
		}

		AllocRange newSegment = {baseData, baseSize};
		addSegment ( ResolveSegments, newSegment );
	}
#endif
}

void RenderBufferManager::AddSurfaceMemory ( const FSurfaceRHIRef& surface )
{
#if SF_RENDERBUFFERMEMORY_ENABLE
	if ( !IsValidRef ( surface ) )
	{
		return;
	}

	UPInt baseData = ( UPInt ) surface.XeSurfaceInfo.GetOffset();
	UPInt baseSize = ( UPInt ) surface.XeSurfaceInfo.GetSize();
	AllocRange newSegment = {baseData, baseSize};
	addSegment ( SurfaceSegments, newSegment );
#endif
}

void RenderBufferManager::AcquireVideoMemory( )
{
#if SF_RENDERBUFFERMEMORY_ENABLE
	allocateSegments ( ResolveSegments, ResolveAlloc, AvailableResolve, RHI_XeTEXTURE_MEMORY_SHIFT );

	// Hardcoded EDRAM values.
	AllocRange depthRange = { 0, 720 };
	AllocRange freeRange =  { 1440, 608 };
	addSegment ( SurfaceSegments, depthRange );
	addSegment ( SurfaceSegments, freeRange );
	allocateSegments ( SurfaceSegments, SurfaceAlloc, AvailableSurface, 0 );
#endif
}

void RenderBufferManager::ReleaseVideoMemory()
{
	Reset();

	// Make sure everything in the allocators is freed.
	ResolveAlloc.~AllocAddr();
	SurfaceAlloc.~AllocAddr();
}

void RenderBufferManager::Destroy()
{
	Reset();

	AvailableResolve = 0;
	pManagerLocks = 0;
}

void RenderBufferManager::clearRenderTargetSurface ( RenderTarget& rt )
{
	RHI::Texture* pt = rt.GetTexture();
	if ( !pt  || !pt->pTextures[0].Tex )
	{
		return;
	}

	// If the texture has no remaining texture pointer, it is lost.
	if ( !pt->pTextures[0].Tex->SurfaceRHI )
	{
		rt.RTStatus = RTS_Lost;
	}

	//printf("Free (surface): 0x%x\n", pt->pTextures[0].Tex->SurfaceRHI.XeSurfaceInfo.GetOffset() );
	if ( rt.EDRAMSize )
	{
		SurfaceAlloc.Free ( rt.EDRAMTile, rt.EDRAMSize );
		rt.EDRAMSize = 0;
	}

	// Clear the render surface from the RenderTargetData, and the texture.
	RenderTargetData* phd = ( RenderTargetData* ) rt.GetHALData();
	pt->pTextures[0].Tex->SurfaceRHI = 0;
	phd->Resource.ColorBuffer = 0;
	AvailableSurface += rt.EDRAMSize;
}

void RenderBufferManager::clearRenderTargetResolve ( RenderTarget& rt, bool bIsUnused )
{
	RHI::Texture* pt = rt.GetTexture();
	if ( !pt || !pt->pTextures[0].Tex )
	{
		return;
	}

	SF_ASSERT ( pt->pTextures[0].Tex );
	//printf("Free (texture): 0x%x\n", pt->pTextures[0].Tex->Texture2DRHI->GetBaseAddress() );

	// If the texture still have a surface, it could be resolved again.
	if ( pt->pTextures[0].Tex->SurfaceRHI && !bIsUnused )
	{
		rt.RTStatus = RTS_Unresolved;
	}
	else
	{
		rt.RTStatus = RTS_Lost;
	}

	UPInt baseSize = ( UPInt ) RHIGetTextureSize ( pt->pTextures[0].Tex->Texture2DRHI );
	UPInt baseData = ( UPInt ) RHIGetTextureBase ( pt->pTextures[0].Tex->Texture2DRHI );
	ResolveAlloc.Free ( baseData >> RHI_XeTEXTURE_MEMORY_SHIFT, baseSize >> RHI_XeTEXTURE_MEMORY_SHIFT );
	pt->pTextures[0].Tex->Texture2DRHI = 0;
	pt->pTextures[0].Tex->TextureRHI = 0;

	// Clear the render target from the RenderTargetData
	RenderTargetData* phd = ( RenderTargetData* ) rt.GetHALData();
	phd->Resource.TextureRHI = 0;

	AvailableResolve += baseSize;
}

void RenderBufferManager::clearDepthStencilSurface ( DepthStencilBuffer& ds )
{
	if ( !ds.GetSurface() || !ds.GetSurface()->Resource.IsInitialized() )
	{
		return;
	}

	DepthStencilSurface* pdss = ds.GetSurface();
	//printf("Free (surface): 0x%x\n", pdss->Resource.DepthBuffer.XeSurfaceInfo.GetOffset() );

	// Make sure that the surface isn't set on the device before destroying it.
	RHISetRenderTarget ( 0, 0 );

	SurfaceAlloc.Free ( ds.EDRAMTile, ds.EDRAMSize );
	pdss->Resource.DepthBuffer = 0;
}

Render::DepthStencilBuffer* RenderBufferManager::CreateDepthStencilBuffer ( const ImageSize& InSize )
{
	// For extremely large surfaces (full screen), make them smaller, because we probably won't have enough room.
	ImageSize size = InSize;
	if ( size.Width >= 1280 || size.Height >= 720 )
	{
		size.Width /= 2;
		size.Height /= 2;
	}

	DepthStencilBuffer* pdsb = SF_HEAP_NEW ( pMainHeap ) DepthStencilBuffer ( this, size );
	DepthStencilSurface* pdss = SF_HEAP_NEW ( pMainHeap ) DepthStencilSurface ( 0, size );
	pdsb->pSurface = pdss;

	FSurfaceRHIRef surface = allocateSurface ( size, PF_DepthStencil, pdsb->EDRAMTile, pdsb->EDRAMSize );
	if ( !surface )
	{
		// pdss is deleted automatically, smart pointer within pdsb.
		delete pdsb;
		return 0;
	}

	pdss->Resource.DepthBuffer = surface;
	return pdsb;
}

FSurfaceRHIRef RenderBufferManager::allocateSurface ( const ImageSize& InSize, EPixelFormat format, SInt32& edramLocation, unsigned& edramSize )
{
	// For extremely large surfaces (full screen), make them smaller, because we probably won't have enough room.
	ImageSize size = InSize;
	if ( size.Width >= 1280 || size.Height >= 720 )
	{
		size.Width /= 2;
		size.Height /= 2;
	}
	edramSize = RHICalcTargetableSurfaceSize ( size.Width, size.Height, format );
	edramLocation = SurfaceAlloc.Alloc ( edramSize );
	if ( edramLocation == ~UPInt ( 0 ) )
	{
        UPInt largest = SurfaceAlloc.GetLargestAvailable();
        UPInt total = SurfaceAlloc.GetFreeSize();
		RenderTarget* prt = RenderTargets.GetFirst();
		RenderTarget* prtNext;

		// Remove all other render target surfaces in EDRAM by resolving them.
		while ( !RenderTargets.IsNull ( prt ) )
		{
			prtNext = prt->pNext;
			if ( prt->GetStatus() == RTS_Unresolved ||
					prt->GetStatus() == RTS_Available )
			{
				RenderTargetData* phd = ( RenderTargetData* ) prt->GetHALData();
				if ( prt->GetStatus() == RTS_Unresolved && phd->Resource.ColorBuffer )
				{
					RHICopyToResolveTarget ( phd->Resource.ColorBuffer, FALSE, FResolveParams() );
				}
				clearRenderTargetSurface ( *prt );
				if ( prt->GetStatus() == RTS_Lost )
				{
					prt->RemoveNode();
					prt->Release();
				}
			}
			prt = prtNext;
		}

		// Try to allocate again.
		edramLocation = SurfaceAlloc.Alloc ( edramSize );
		if ( edramLocation == ~UPInt ( 0 ) )
		{
			return 0;
		}
	}

	// Allocate CPU-side strutures.
	FSurfaceRHIRef surface = RHICreateTargetableSurfaceExplicit ( size.Width, size.Height, format, NULL, 0, edramLocation );
	if ( !IsValidRef ( surface ) )
	{
		SurfaceAlloc.Free ( edramLocation, edramSize );
		return 0;
	}

    AvailableSurface -= edramSize;

	//printf("Allocate (surface): 0x%x\n", surface.XeSurfaceInfo.GetOffset() );
	return surface;

}

FTexture2DRHIRef RenderBufferManager::allocateRenderTargetResolve ( const ImageSize &InSize )
{
	// For extremely large surfaces (full screen), make them smaller, because we probably won't have enough room.
	ImageSize size = InSize;
	if ( size.Width >= 1280 || size.Height >= 720 )
	{
		size.Width /= 2;
		size.Height /= 2;
	}

	const unsigned shift = RHI_XeTEXTURE_MEMORY_SHIFT;
	UINT baseSize  = RHIGetTextureSize ( size.Width, size.Height, PF_A8R8G8B8, 1, 0 );
	UINT baseAllocSize = ( baseSize + ( 1 << ( shift - 1 ) ) ) >> shift;

	UPInt textureMemory = ResolveAlloc.Alloc ( baseAllocSize );
	if ( textureMemory != ~UPInt ( 0 ) )
	{
		//printf("Allocate (resolve): 0x%x size = 0x%x\n", textureMemory << shift, baseAllocSize << shift );
		FTexture2DRHIRef texture = RHICreateTexture2DExplicit ( size.Width, size.Height, PF_A8R8G8B8, 0, textureMemory << shift );
		if ( !IsValidRef ( texture ) )
		{
			ResolveAlloc.Free ( textureMemory, baseAllocSize );
			return 0;
		}
		AvailableResolve -= baseSize;
		return texture;
	}

	// Try to evict any available render targets, and allocate again.
	RenderTarget* prt = RenderTargets.GetFirst();
	while ( !RenderTargets.IsNull ( prt ) )
	{
		RenderTarget* prtNext = prt->pNext;
		if ( prt->GetStatus() == RTS_Available )
		{
			clearRenderTargetResolve ( *prt );
			if ( prt->GetStatus() == RTS_Lost )
			{
				prt->RemoveNode();
				prt->Release();
			}
		}
		prt = prtNext;
	}

	// Try to allocate again (pass or fail).
	textureMemory = ResolveAlloc.Alloc ( baseAllocSize );
	if ( textureMemory != ~UPInt ( 0 ) )
	{
		//printf("Allocate (resolve): 0x%x size = 0x%x\n", textureMemory << shift, baseAllocSize << shift );

		FTexture2DRHIRef texture = RHICreateTexture2DExplicit ( size.Width, size.Height, PF_A8R8G8B8, 0, textureMemory << shift );
		if ( !IsValidRef ( texture ) )
		{
			ResolveAlloc.Free ( textureMemory, baseAllocSize );
			return 0;
		}
		AvailableResolve -= baseSize;
		return texture;
	}
	return 0;
}

void RenderBufferManager::EndFrame()
{
#ifdef SF_BUILD_DEBUG
	// Make sure no render targets are in use (debug only)
	RenderTarget* prt = RenderTargets.GetFirst();
	while ( !RenderTargets.IsNull ( prt ) )
	{
		SF_ASSERT ( prt->RTStatus != RTS_InUse );
		prt = prt->pNext;
	}
#endif
}

void RenderBufferManager::Reset()
{
	// Evict all render targets.
	RenderTarget* prt = RenderTargets.GetFirst();
	while ( !RenderTargets.IsNull(prt) )
	{
		clearRenderTargetSurface(*prt);
		clearRenderTargetResolve(*prt);
		RenderTarget* prtNext = RenderTargets.GetNext(prt);
		SF_ASSERT(prt->GetStatus() == RTS_Lost);
		prt->RemoveNode();
		prt->Release();
		prt = prtNext;
	}
	SF_ASSERT(RenderTargets.IsEmpty());
}

Render::RenderTarget* RenderBufferManager::CreateRenderTarget ( const ImageSize& size, RenderBufferType type, ImageFormat format, Render::Texture* texture )
{
	SF_ASSERT ( type != RBuffer_DepthStencil );
	RenderTarget* target = SF_HEAP_AUTO_NEW ( this ) RenderTarget ( this, type, size );
	if ( target )
	{
		// Apply a texture if relevant.
		target->initTexture ( ( RHI::Texture* ) texture );
		target->initViewRect ( Rect<int> ( size.Width, size.Height ) );
	}

	// By default, render targets don't get added to the list,
	// as it is up to end user to free them.
	return target;
}

void RenderBufferManager::addSegment ( ArrayPOD<AllocRange>& segmentList, const AllocRange& range )
{
	// Find the location within the list (sorted by start location) and insert it.
	for ( UPInt location = 0; location < segmentList.GetSize(); location++ )
	{
		if ( range.Location <= segmentList[location].Location )
		{
			segmentList.InsertAt ( location, range );
			return;
		}
	}
	segmentList.PushBack ( range );
}

void RenderBufferManager::allocateSegments ( ArrayPOD<AllocRange>& segmentList, AllocAddr& allocator, UPInt& availableMemory, UPInt shift )
{
	// Coalesce blocks and pass them into add segment.
	AllocRange nextRange = { 0, 0 };
	for ( UPInt location = 0; location < segmentList.GetSize(); ++location )
	{
		// New range.
		if ( nextRange.Size == 0 )
		{
			nextRange = segmentList[location];
			continue;
		}

		// Old range completely contains old range.
		if ( nextRange.Contains ( segmentList[location] ) )
		{
			continue;
		}

		// Old range and new range overlap.
		if ( nextRange.Overlaps ( segmentList[location] ) )
		{
			nextRange.Size += segmentList[location].End() - nextRange.End();
			continue;
		}

		// Old range doesn't overlap new range at all, allocate the old one.
		allocator.AddSegment ( nextRange.Location >> shift, nextRange.Size >> shift );
		//printf("Add Segment 0x%x, Size = 0x%x\n", nextRange.Location, nextRange.Size);
		availableMemory += nextRange.Size;
		nextRange = segmentList[location];
	}

	// Allocate the final block.
	if ( nextRange.Size > 0 )
	{
		allocator.AddSegment ( nextRange.Location >> shift, nextRange.Size >> shift );
		//printf("Add Segment 0x%x, Size = 0x%x\n", nextRange.Location, nextRange.Size);
		availableMemory += nextRange.Size;
	}
	segmentList.Clear();
}

void RenderTarget::SetInUse ( RenderTargetUse inUse )
{
	// If we are not already lost, and unused, kill the render target immediately.
	bool release = false;
	if ( RTStatus != RTS_Lost && inUse == RTUse_Unused )
	{
		if ( GetType() == RBuffer_Temporary )
		{
			RenderBufferManager* mgr = getManager();
			mgr->clearRenderTargetResolve ( *this, true );
			mgr->clearRenderTargetSurface ( *this );
			RemoveNode();
			release = true;
		}
		pTexture = 0;
	}

	// If we are cacheable, set it to be available, which may mean it could be evicted.
	if ( inUse == RTUse_Unused_Cacheable )
	{
		RTStatus = RTS_Available;
	}

	// Must release last, just in case we're going to be deleted.
	if ( release )
	{
		Release();
	}
}

}
}
}
}; // Scaleform::Render::RHI::RBMemoryImpl

#endif // XBOX

#endif//WITH_GFx